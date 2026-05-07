// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/hw_random.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/crypto.h>
#include <crypto/scatterwalk.h>
#include <crypto/algapi.h>
#include <crypto/hash.h>
#include <crypto/sha3.h>
#include <crypto/internal/hash.h>
#include "aml-crypto-dma.h"
#include "aml-sha3-dma.h"

/* SHA flags */
#define SHA_FLAGS_BUSY			BIT(0)
#define SHA_FLAGS_DMA_ACTIVE    BIT(1)
#define SHA_FLAGS_OUTPUT_READY  BIT(2)
#define SHA_FLAGS_INIT          BIT(3)

#define SHA_FLAGS_FINAL         BIT(16)
#define SHA_FLAGS_SHA3_224      BIT(17)
#define SHA_FLAGS_SHA3_256      BIT(18)
#define SHA_FLAGS_SHA3_384      BIT(19)
#define SHA_FLAGS_SHA3_512      BIT(20)
#define SHA_FLAGS_SHAKE_128     BIT(21)
#define SHA_FLAGS_SHAKE_256     BIT(22)
#define SHA_FLAGS_ERROR         BIT(23)

#define SHA_OP_UPDATE	1
#define SHA_OP_FINAL	2

#define SHA3_HW_CONTEXT_SIZE (201)
#define SHA3_CONTEXT_LEN ALIGN(SHA3_HW_CONTEXT_SIZE, 64)

#define AML_SHA3_DIGEST_BUFSZ (SHA3_512_DIGEST_SIZE)

#define DEFAULT_AUTOSUSPEND_DELAY	1000

#define ENABLE_SHAKE128 (0)

struct aml_sha3_dev;

struct aml_sha3_reqctx {
	struct aml_sha3_dev *dd;
	unsigned long flags;
	unsigned long op;

	struct scatterlist *sg;
	u32 total;
	u64 digcnt[2];

	dma_addr_t dma_addr;
	dma_addr_t hash_ctx_addr;

	size_t block_size;
	u32 fast_nents;

	u8  *state;
	u8  state_v[SHA3_CONTEXT_LEN];
};

struct aml_sha3_ctx {
	struct aml_sha3_dev	*dd;
};

#define AML_SHA_QUEUE_LENGTH	50

struct aml_sha3_dev {
	struct list_head	list;
	struct device		*dev;
	struct device		*parent;

	struct aml_dma_dev      *dma;
	u32 thread;
	u32 status;
	int			err;
	struct tasklet_struct	done_task;

	unsigned long		flags;
	struct crypto_queue	queue;
	struct ahash_request	*req;
	void	*descriptor;
	dma_addr_t	dma_descript_tab;
};

struct aml_sha3_drv {
	struct list_head	dev_list;
	spinlock_t		lock; /* spinlock for device list */
};

static struct aml_sha3_drv aml_sha3 = {
	.dev_list = LIST_HEAD_INIT(aml_sha3.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(aml_sha3.lock),
};

static int aml_sha3_update_dma_stop(struct aml_sha3_dev *dd);
static int aml_shake_finish(struct ahash_request *req);

int aml_sha3_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct aml_sha3_ctx *tctx = crypto_ahash_ctx(tfm);
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha3_dev *dd = NULL;
	struct aml_sha3_dev *tmp;

	spin_lock_bh(&aml_sha3.lock);
	if (!tctx->dd) {
		list_for_each_entry(tmp, &aml_sha3.dev_list, list) {
			dd = tmp;
			break;
		}
		tctx->dd = dd;
	} else {
		dd = tctx->dd;
	}

	spin_unlock_bh(&aml_sha3.lock);

	ctx->dd = dd;

	ctx->flags = 0;
	ctx->state = NULL;
	ctx->hash_ctx_addr = 0;

	dbgp(1, "sha3 init: tfm: %px, ctx: %px, digest size: %d\n",
	     tfm, ctx, crypto_ahash_digestsize(tfm));

	switch (crypto_ahash_digestsize(tfm)) {
	case SHA3_224_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHA3_224;
		ctx->block_size = SHA3_224_BLOCK_SIZE;
		break;
	case SHA3_256_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHA3_256;
		ctx->block_size = SHA3_256_BLOCK_SIZE;
		break;
	case SHA3_384_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHA3_384;
		ctx->block_size = SHA3_384_BLOCK_SIZE;
		break;
	case SHA3_512_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHA3_512;
		ctx->block_size = SHA3_512_BLOCK_SIZE;
		break;
	default:
		return -EINVAL;
	}

	ctx->state = dma_alloc_coherent(dd->dev, SHA3_CONTEXT_LEN,
					&ctx->hash_ctx_addr, GFP_ATOMIC | GFP_DMA);
	if (!ctx->state)
		return -ENOMEM;

	ctx->digcnt[0] = 0;
	ctx->digcnt[1] = 0;
	ctx->fast_nents = 0;

	return 0;
}

static int aml_sha3_xmit_dma(struct aml_sha3_dev *dd,
			    struct ahash_request *req, void *descriptor,
			    u32 nents, int final)
{
	int i = 0;
	struct dma_dsc_v2_64 dsc;
	u32 mode;
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	size_t length = 0;
	u32 status = 0;
	int err = 0;
	u8 enc_sha_only = 0;
	u8 op_mode = 0;

	dbgp(1, "xmit_dma:ctx:%px,digcnt:0x%llx 0x%llx,nents: %u,final:%d, state: %lx\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0], nents, final, (uintptr_t)ctx->state);

	mode = MODE_SHA3;

	if (!ctx->hash_ctx_addr)
		return -ENOMEM;

	if (ctx->flags & SHA_FLAGS_SHA3_224) {
		op_mode = OP_MODE_SHA3_224;
		enc_sha_only = ENC_SHA_ONLY_SHA3;
	} else if (ctx->flags & SHA_FLAGS_SHA3_256) {
		op_mode = OP_MODE_SHA3_256;
		enc_sha_only = ENC_SHA_ONLY_SHA3;
	} else if (ctx->flags & SHA_FLAGS_SHA3_384) {
		op_mode = OP_MODE_SHA3_384;
		enc_sha_only = ENC_SHA_ONLY_SHA3;
	} else if (ctx->flags & SHA_FLAGS_SHA3_512) {
		op_mode = OP_MODE_SHA3_512;
		enc_sha_only = ENC_SHA_ONLY_SHA3;
	} else if (ctx->flags & SHA_FLAGS_SHAKE_128) {
		op_mode = OP_MODE_SHAKE_128_ABSORB;
		enc_sha_only = ENC_SHA_ONLY_SHAKE;
	} else if (ctx->flags & SHA_FLAGS_SHAKE_256) {
		op_mode = OP_MODE_SHAKE_256_ABSORB;
		enc_sha_only = ENC_SHA_ONLY_SHAKE;
	} else {
		return -EINVAL;
	}

	if (!nents) {
		dsc.src_addr = 0;
		dsc.length = 0;
		dsc.dsc_cfg.d32 = 0;
		dsc.tgt_addr = (uintptr_t)ctx->hash_ctx_addr;
		dsc.dsc_cfg.b.enc_sha_only = enc_sha_only;
		dsc.dsc_cfg.b.mode = mode;
		dsc.dsc_cfg.b.begin =
			(i == 0 && !(ctx->digcnt[0] || ctx->digcnt[1]));
		dsc.dsc_cfg.b.end = final;
		dsc.dsc_cfg.b.op_mode = op_mode;
		dsc.dsc_cfg.b.eoc = 1;
		dsc.dsc_cfg.b.owner = 1;

		aml_dma_dsc_writer(descriptor, 0, &dsc, 0, dd->dma);

		nents = 1;
		aml_dma_debug(descriptor, nents, __func__, dd->thread, dd->status,
			      dd->dma);
	} else {
		for (i = 0; i < nents; i++) {
			/* some fields are already filled, read it back and update dsc */
			aml_dma_dsc_reader(descriptor, i, &dsc, 0, dd->dma);

			dsc.dsc_cfg.b.enc_sha_only = enc_sha_only;
			dsc.dsc_cfg.b.mode = mode;
			dsc.dsc_cfg.b.begin =
				(i == 0 && !(ctx->digcnt[0] || ctx->digcnt[1]));
			dsc.dsc_cfg.b.op_mode = op_mode;
			dsc.dsc_cfg.b.owner = 1;
			if (i == (nents - 1)) {
				dsc.dsc_cfg.b.end = final;
				dsc.dsc_cfg.b.eoc = 1;
				dsc.tgt_addr = (uintptr_t)ctx->hash_ctx_addr;
			} else {
				dsc.dsc_cfg.b.end = 0;
				dsc.dsc_cfg.b.eoc = 0;
			}
			length = dsc.length;
			ctx->digcnt[0] += length;
			if (ctx->digcnt[0] < length)
				ctx->digcnt[1]++;

			aml_dma_dsc_writer(descriptor, i, &dsc, 0, dd->dma);
		}
		aml_dma_debug(descriptor, nents, __func__, dd->thread, dd->status,
			      dd->dma);
	}

	if (final)
		ctx->flags |= SHA_FLAGS_FINAL; /* catch last interrupt */

	dd->req = req;

	dbgp(1, "xmit before:ctx:%px,digcnt:0x%llx 0x%llx,length:%zd,final:%d\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0], length, final);

	/* Start DMA transfer */
	dd->flags |=  SHA_FLAGS_DMA_ACTIVE;
	status = aml_dma_do_hw_crypto(dd->dma, descriptor, nents, dd->dma_descript_tab,
				      USE_BUSY_POLLING, DMA_FLAG_SHA_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR)
		dd->flags |= SHA_FLAGS_ERROR;
	aml_dma_debug(descriptor, nents, "end sha3", dd->thread, dd->status,
		      dd->dma);

	dd->flags |= SHA_FLAGS_OUTPUT_READY;
	dd->flags &= ~SHA_FLAGS_DMA_ACTIVE;

	err = aml_sha3_update_dma_stop(dd);
	if (!err) {
		err = dd->flags & SHA_FLAGS_ERROR;
		dd->flags = (dd->flags & SHA_FLAGS_ERROR);
	}

	return err;
}

static int aml_sha3_update_dma_start(struct aml_sha3_dev *dd,
				    struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	unsigned int length = 0, final = 0;
	struct scatterlist *sg;
	struct dma_dsc_v2_64 dsc;
	struct device *dev = dd->dev;
	int nents = 0;
	u32 nents_act = 0;
	u32 mapped = 0;
	u32 i = 0;
	int err = 0;

	dbgp(1, "start: ctx: %px, total: %u, block size: %zd, flag: %lx",
	     ctx, ctx->total, ctx->block_size, ctx->flags);

	do {
		nents_act = sg_nents(ctx->sg);
		nents = sg_nents_for_len(ctx->sg, ctx->total);
		nents = nents > MAX_NUM_TABLES ? MAX_NUM_TABLES : nents;
		dbgp(1, "ctx:%px, sg: %px, total: %d, total nents:%d, using nents: %d\n",
				ctx, ctx->sg, ctx->total, nents_act, nents);

		sg = ctx->sg;
		if (nents > 0) {
			mapped = dma_map_sg(dd->parent, sg, nents, DMA_TO_DEVICE);
			if (!mapped) {
				dev_err(dev, "dma_map_sg() error\n");
				return 0;
			}
		} else {
			mapped = 0;
		}
		dbgp(1, "ctx:%px, mapped: %d\n", ctx, mapped);
		for (i = 0; i < mapped; i++) {
			dbgp(1, "ctx:%px,dig:0x%llx 0x%llx,tot:%u,sgl: %u\n",
					ctx, ctx->digcnt[1], ctx->digcnt[0],
					ctx->total, sg->length);

			length = sg->length;
			length = ctx->total > length ? length : ctx->total;
			ctx->dma_addr = sg_dma_address(sg);

			dsc.src_addr = (uintptr_t)ctx->dma_addr;
			dsc.length = length;
			dsc.dsc_cfg.d32 = 0;
			aml_dma_dsc_writer(dd->descriptor, i, &dsc, 0, dd->dma);

			sg = sg_next(sg);
			ctx->total -= length;
		}
		final = (ctx->flags & SHA_FLAGS_FINAL) && !ctx->total;
		ctx->fast_nents = mapped;
		err = aml_sha3_xmit_dma(dd, req, dd->descriptor, mapped, final);
		if (err && err != -EINPROGRESS) {
			pr_err("%s:%d: aml_sha3_xmit_dma failed with err = %d\n"
					, __func__, __LINE__, err);
			goto out;
		}
	} while (ctx->total);

out:
	return err;
}

static int aml_sha3_update_dma_stop(struct aml_sha3_dev *dd)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(dd->req);
	u32 nents = 0;

	aml_dma_debug(dd->descriptor, ctx->fast_nents, __func__, dd->thread,
		      dd->status, dd->dma);

	if (ctx->fast_nents) {
		dma_unmap_sg(dd->parent, ctx->sg, ctx->fast_nents, DMA_TO_DEVICE);
		for (nents = 0; nents < ctx->fast_nents; nents++)
			ctx->sg = sg_next(ctx->sg);
	}

	memcpy(ctx->state_v, ctx->state, SHA3_CONTEXT_LEN);

	return 0;
}

static int aml_sha3_update_req(struct aml_sha3_dev *dd, struct ahash_request *req)
{
	int err;
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);

	dbgp(1, "update_req: ctx: %px, total: %u, digcnt: 0x%llx 0x%llx\n",
	     ctx, ctx->total, ctx->digcnt[1], ctx->digcnt[0]);

	err = aml_sha3_update_dma_start(dd, req);

	return err;
}

static int aml_sha3_final_req(struct aml_sha3_dev *dd, struct ahash_request *req)
{
	int err = 0;
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);

	dbgp(1, "%s %d", __func__, __LINE__);
	err = aml_sha3_update_dma_start(dd, req);

	dbgp(1, "final_req: ctx: %px, err: %d\n", ctx, err);

	return err;
}

static void aml_sha3_copy_ready_hash(struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);

	if (!req->result)
		return;

	if (ctx->flags & SHA_FLAGS_SHA3_224)
		memcpy(req->result, ctx->state, SHA3_224_DIGEST_SIZE);
	else if (ctx->flags & SHA_FLAGS_SHA3_256)
		memcpy(req->result, ctx->state, SHA3_256_DIGEST_SIZE);
	else if (ctx->flags & SHA_FLAGS_SHA3_384)
		memcpy(req->result, ctx->state, SHA3_384_DIGEST_SIZE);
	else if (ctx->flags & SHA_FLAGS_SHA3_512)
		memcpy(req->result, ctx->state, SHA3_512_DIGEST_SIZE);
	else if (ctx->flags & SHA_FLAGS_SHAKE_128)
		memcpy(req->result, ctx->state, SHAKE_128_DIGEST_SIZE);
	else if (ctx->flags & SHA_FLAGS_SHAKE_256)
		memcpy(req->result, ctx->state, SHAKE_256_DIGEST_SIZE);
}

static int aml_sha3_finish(struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha3_dev *dd = ctx->dd;
	int err = 0;

	aml_sha3_copy_ready_hash(req);

	dma_free_coherent(dd->dev, SHA3_CONTEXT_LEN,
			ctx->state, ctx->hash_ctx_addr);
	ctx->hash_ctx_addr = 0;
	ctx->state = 0;
	dbgp(1, "finish digcnt: ctx: %px, 0x%llx 0x%llx, %x %x\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0],
	     req->result[0], req->result[1]);

	return err;
}

void aml_sha3_finish_req(struct ahash_request *req, int err)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha3_dev *dd = ctx->dd;
	unsigned long flags;

	if (!err) {
		if (SHA_FLAGS_FINAL & ctx->flags) {
			if (ctx->flags & SHA_FLAGS_SHAKE_128 ||
			    ctx->flags & SHA_FLAGS_SHAKE_256) {
				err = aml_shake_finish(req);
			} else {
				err = aml_sha3_finish(req);
			}
		}
	} else {
		ctx->flags |= SHA_FLAGS_ERROR;
	}

	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
	dd->flags &= ~(SHA_FLAGS_BUSY | SHA_FLAGS_OUTPUT_READY);
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
}

static int aml_sha3_hw_init(struct aml_sha3_dev *dd)
{
	u32 dma_dsc_sz = aml_dma_get_dsc_sz(0, dd->dma);

	if (!(dd->flags & SHA_FLAGS_INIT)) {
		dd->descriptor =
			dmam_alloc_coherent(dd->parent,
					    MAX_NUM_TABLES * dma_dsc_sz,
					    &dd->dma_descript_tab,
					    GFP_KERNEL | GFP_DMA);
		if (!dd->descriptor) {
			dev_err(dd->dev, "dma descriptor allocate error\n");
			return -ENOMEM;
		}
		dd->flags |= SHA_FLAGS_INIT;
		dd->err = 0;
	}

	return 0;
}

static int aml_sha3_state_restore(struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha3_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha3_dev *dd = tctx->dd;
	struct dma_dsc_v2_64 dsc;
	s32 len = SHA3_HW_CONTEXT_SIZE;
	u32 status = 0;
	int err = 0;

	if (!ctx->digcnt[0] && !ctx->digcnt[1])
		return err;

	memcpy(ctx->state, ctx->state_v, SHA3_CONTEXT_LEN);
	dsc.src_addr = (uintptr_t)ctx->hash_ctx_addr;
	dsc.tgt_addr = 0;
	dsc.length = len;
	dsc.dsc_cfg.d32 = 0;
	dsc.dsc_cfg.b.mode = MODE_KEY;
	dsc.dsc_cfg.b.eoc = 1;
	dsc.dsc_cfg.b.owner = 1;
	aml_dma_dsc_writer(dd->descriptor, 0, &dsc, 0, dd->dma);
	status = aml_dma_do_hw_crypto(dd->dma, dd->descriptor, 1, dd->dma_descript_tab,
				      USE_BUSY_POLLING, DMA_FLAG_SHA_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR) {
		dev_err(dd->dev, "hw crypto failed, status: %u\n", status);
		err = -EINVAL;
	}
	aml_dma_debug(dd->descriptor, 1, "end restore", dd->thread, dd->status,
		      dd->dma);
	return err;
}

static int aml_sha3_handle_queue(struct aml_sha3_dev *dd,
				struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx;
	unsigned long flags;
	int err = 0, ret = 0;

	if (!req) {
		pm_runtime_mark_last_busy(dd->parent);
		pm_runtime_put_autosuspend(dd->parent);
		return ret;
	}

	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->flags |= SHA_FLAGS_BUSY;
	dd->dma->dma_busy |= DMA_FLAG_MAY_OCCUPY;
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
	ctx = ahash_request_ctx(req);

	dbgp(1, "handling new req, ctx: %px, op: %lu, nbytes: %d\n",
	     ctx, ctx->op, req->nbytes);
	err = aml_sha3_state_restore(req);
	if (err)
		goto err1;
	dbgp(1, "%s %d", __func__, __LINE__);

	if (ctx->op == SHA_OP_UPDATE) {
		err = aml_sha3_update_req(dd, req);
		if (!err) {
			spin_lock_irqsave(&dd->dma->dma_lock, flags);
			dd->flags &= ~SHA_FLAGS_BUSY;
			dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
			spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
		}
	} else if (ctx->op == SHA_OP_FINAL) {
		dbgp(1, "%s %d", __func__, __LINE__);
		err = aml_sha3_final_req(dd, req);
		if (!err) {
			spin_lock_irqsave(&dd->dma->dma_lock, flags);
			dd->flags &= ~SHA_FLAGS_BUSY;
			dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
			spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
		}
	}

err1:
	return err;
}

static int aml_sha3_enqueue(struct ahash_request *req, unsigned int op)
{
	struct aml_sha3_reqctx *ctx = NULL;
	struct aml_sha3_ctx *tctx = NULL;
	struct aml_sha3_dev *dd = NULL;
	int err = 0;

	if (!req)
		return -EINVAL;

	ctx = ahash_request_ctx(req);
	tctx = crypto_tfm_ctx(req->base.tfm);
	dd = tctx->dd;

	ctx->op = op;

	if (pm_runtime_suspended(dd->parent)) {
		err = pm_runtime_get_sync(dd->parent);
		if (err < 0) {
			dev_err(dd->dev, "%s: pm_runtime_get_sync fails: %d\n",
				__func__, err);
			return err;
		}
	}

	return aml_dma_crypto_enqueue_req(dd->dma, &req->base);
}

int aml_sha3_process(struct ahash_request *req)
{
	struct aml_sha3_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha3_dev *dd = tctx->dd;

	return aml_sha3_handle_queue(dd, req);
}

static int _aml_sha3_update(struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);

	dbgp(1, "%s:%d:req->nbytes = %d\n",
	     __func__, __LINE__, req->nbytes);

	ctx->total = req->nbytes;
	ctx->sg = req->src;
	ctx->fast_nents = 0;

	if (!req->nbytes)
		return 0;

	return aml_sha3_enqueue(req, SHA_OP_UPDATE);
}

int aml_sha3_update(struct ahash_request *req)
{
	return _aml_sha3_update(req);
}

static int _aml_sha3_final(struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);

	ctx->flags |= SHA_FLAGS_FINAL;
	ctx->total = 0;
	ctx->sg = 0;

	if (ctx->flags & SHA_FLAGS_ERROR)
		return 0; /* uncompleted hash is not needed */

	return aml_sha3_enqueue(req, SHA_OP_FINAL);
}

int aml_sha3_final(struct ahash_request *req)
{
	int err = 0;

	dbgp(1, "%s:%d: req->nbytes = %d\n",
	     __func__, __LINE__, req->nbytes);
	err =  _aml_sha3_final(req);
	return err;
}

static int _aml_sha3_finup(struct ahash_request *req)
{
	int err;
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);

	ctx->flags |= SHA_FLAGS_FINAL;
	err = _aml_sha3_update(req);
	if (err == -EINPROGRESS || err == -EBUSY)
		return err;
	/*
	 * final() has to be always called to cleanup resources
	 * even if update() failed, except EINPROGRESS
	 */
	err = aml_sha3_final(req);

	return err;
}

int aml_sha3_finup(struct ahash_request *req)
{
	s32 err = 0;

	err = _aml_sha3_finup(req);
	return err;
}

int aml_sha3_digest(struct ahash_request *req)
{
	int err = 0;

	err = aml_sha3_init(req);
	if (err) {
		pr_err("%s:%d:aml_sha3_init fails\n", __func__, __LINE__);
		goto out;
	}

	err = aml_sha3_finup(req);
	if (err && (err != -EINPROGRESS && err != -EBUSY)) {
		pr_err("%s:%d:aml_sha3_finup failed\n", __func__, __LINE__);
		goto out;
	}
out:
	return err;
}

int aml_sha3_import(struct ahash_request *req, const void *in)
{
	struct aml_sha3_reqctx *rctx = ahash_request_ctx(req);

	memcpy(rctx, in, sizeof(*rctx));
	return 0;
}

int aml_sha3_export(struct ahash_request *req, void *out)
{
	struct aml_sha3_reqctx *rctx = ahash_request_ctx(req);

	memcpy(out, rctx, sizeof(*rctx));
	dbgp(1, "export size: %zd %d %lx\n",
	     sizeof(*rctx), SHA3_CONTEXT_LEN, (uintptr_t)out);
	return 0;
}

static int aml_sha3_cra_init_alg(struct crypto_tfm *tfm, const char *alg_base)
{
	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct aml_sha3_reqctx));

	return 0;
}

static int aml_sha3_cra_init(struct crypto_tfm *tfm)
{
	return aml_sha3_cra_init_alg(tfm, NULL);
}

static void aml_sha3_cra_exit(struct crypto_tfm *tfm)
{
}

int aml_shake_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct aml_sha3_ctx *tctx = crypto_ahash_ctx(tfm);
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha3_dev *dd = NULL;
	struct aml_sha3_dev *tmp;

	spin_lock_bh(&aml_sha3.lock);
	if (!tctx->dd) {
		list_for_each_entry(tmp, &aml_sha3.dev_list, list) {
			dd = tmp;
			break;
		}
		tctx->dd = dd;
	} else {
		dd = tctx->dd;
	}

	spin_unlock_bh(&aml_sha3.lock);

	ctx->dd = dd;

	ctx->flags = 0;
	ctx->state = NULL;
	ctx->hash_ctx_addr = 0;

	dbgp(1, "shake init: tfm: %px, ctx: %px, digest size: %d\n",
	     tfm, ctx, crypto_ahash_digestsize(tfm));

	switch (crypto_ahash_digestsize(tfm)) {
	case SHAKE_128_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHAKE_128;
		ctx->block_size = SHAKE_128_BLOCK_SIZE;
		break;
	case SHAKE_256_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHAKE_256;
		ctx->block_size = SHAKE_256_BLOCK_SIZE;
		break;
	default:
		return -EINVAL;
	}

	ctx->state = dma_alloc_coherent(dd->dev, SHA3_CONTEXT_LEN,
					&ctx->hash_ctx_addr, GFP_ATOMIC | GFP_DMA);
	if (!ctx->state)
		return -ENOMEM;

	ctx->digcnt[0] = 0;
	ctx->digcnt[1] = 0;

	return 0;
}

int aml_shake_update(struct ahash_request *req)
{
	return aml_sha3_update(req);
}

static int aml_shake_squeeze_dma(struct ahash_request *req)
{
	u32 mode;
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	u32 digest_size = crypto_ahash_digestsize(tfm);
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha3_ctx *tctx = crypto_ahash_ctx(tfm);
	struct aml_sha3_dev *dd = tctx->dd;
	struct dma_dsc_v2_64 dsc;
	int err = 0;
	u32 status = 0;
	u8 enc_sha_only = 0;
	u8 op_mode = 0;

	dbgp(1, "squeeze_dma:ctx:%px,digcnt:0x%llx 0x%llx,\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0]);

	mode = MODE_SHA3;

	if (ctx->flags & SHA_FLAGS_SHAKE_128) {
		op_mode = OP_MODE_SHAKE_128_SQUEEZE;
		enc_sha_only = ENC_SHA_ONLY_SHAKE;
	} else if (ctx->flags & SHA_FLAGS_SHAKE_256) {
		op_mode = OP_MODE_SHAKE_256_SQUEEZE;
		enc_sha_only = ENC_SHA_ONLY_SHAKE;
	} else {
		return -EINVAL;
	}

	dsc.src_addr = 0;
	dsc.tgt_addr = (uintptr_t)ctx->hash_ctx_addr;
	dsc.length = digest_size;
	dsc.dsc_cfg.d32 = 0;
	dsc.dsc_cfg.b.enc_sha_only = enc_sha_only;
	dsc.dsc_cfg.b.mode = mode;
	dsc.dsc_cfg.b.begin = 0;
	dsc.dsc_cfg.b.end = 1;
	dsc.dsc_cfg.b.op_mode = op_mode;
	dsc.dsc_cfg.b.eoc = 1;
	dsc.dsc_cfg.b.owner = 1;
	aml_dma_dsc_writer(dd->descriptor, 0, &dsc, 0, dd->dma);

	aml_dma_debug(dd->descriptor, 1, __func__, dd->thread, dd->status,
		      dd->dma);

	dd->req = req;

	/* Start DMA transfer */
	status = aml_dma_do_hw_crypto(dd->dma, dd->descriptor, 1, dd->dma_descript_tab,
				      USE_BUSY_POLLING, DMA_FLAG_SHA_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR) {
		dev_err(dd->dev, "hw crypto failed, status: %u\n", status);
		err = -EINVAL;
	}
	aml_dma_debug(dd->descriptor, 1, "end squeeze", dd->thread, dd->status,
		      dd->dma);
	return err;
}

static int aml_shake_finish(struct ahash_request *req)
{
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha3_dev *dd = ctx->dd;
	int err = 0;

	err = aml_shake_squeeze_dma(req);
	if (err) {
		dbgp(2, "%s:%d: squeeze err = %d\n",
				__func__, __LINE__, err);
		return err;
	}

	aml_sha3_copy_ready_hash(req);

	dma_free_coherent(dd->dev, SHA3_CONTEXT_LEN,
			ctx->state, ctx->hash_ctx_addr);
	ctx->hash_ctx_addr = 0;
	ctx->state = 0;
	dbgp(1, "finish digcnt: ctx: %px, 0x%llx 0x%llx, %x %x\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0],
	     req->result[0], req->result[1]);

	return err;
}

static int aml_shake_final(struct ahash_request *req)
{
	return aml_sha3_final(req);
}

static int _aml_shake_finup(struct ahash_request *req)
{
	int err;
	struct aml_sha3_reqctx *ctx = ahash_request_ctx(req);

	ctx->flags |= SHA_FLAGS_FINAL;
	err = _aml_sha3_update(req);
	if (err == -EINPROGRESS || err == -EBUSY)
		return err;
	/*
	 * final() has to be always called to cleanup resources
	 * even if update() failed, except EINPROGRESS
	 */
	err = aml_shake_final(req);

	return err;
}

int aml_shake_finup(struct ahash_request *req)
{
	s32 err = 0;

	err = _aml_shake_finup(req);
	return err;
}

int aml_shake_digest(struct ahash_request *req)
{
	int err = 0;

	err = aml_shake_init(req);
	if (err) {
		pr_err("%s:%d:aml_sha3_init fails\n", __func__, __LINE__);
		goto out;
	}

	err = aml_shake_finup(req);
	if (err && (err != -EINPROGRESS && err != -EBUSY)) {
		pr_err("%s:%d:_aml_shake_finup failed\n", __func__, __LINE__);
		goto out;
	}
out:
	return err;
}

int aml_shake_import(struct ahash_request *req, const void *in)
{
	return aml_sha3_import(req, in);
}

int aml_shake_export(struct ahash_request *req, void *out)
{
	return aml_sha3_export(req, out);
}

static int aml_shake_cra_init(struct crypto_tfm *tfm)
{
	return aml_sha3_cra_init(tfm);
}

static void aml_shake_cra_exit(struct crypto_tfm *tfm)
{
	return aml_sha3_cra_exit(tfm);
}

static struct ahash_alg sha3_algs[] = {
	{
		.init		= aml_sha3_init,
		.update		= aml_sha3_update,
		.final		= aml_sha3_final,
		.finup		= aml_sha3_finup,
		.digest		= aml_sha3_digest,
		.export     = aml_sha3_export,
		.import     = aml_sha3_import,
		.halg = {
			.digestsize	= SHA3_224_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha3_reqctx),
			.base	= {
				.cra_name	  = "sha3-224-aml",
				.cra_driver_name  = "aml-sha3-224",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHA3_224_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha3_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_sha3_cra_init,
				.cra_exit	  = aml_sha3_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha3_init,
		.update		= aml_sha3_update,
		.final		= aml_sha3_final,
		.finup		= aml_sha3_finup,
		.digest		= aml_sha3_digest,
		.export     = aml_sha3_export,
		.import     = aml_sha3_import,
		.halg = {
			.digestsize	= SHA3_256_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha3_reqctx),
			.base	= {
				.cra_name	  = "sha3-256-aml",
				.cra_driver_name  = "aml-sha3-256",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHA3_256_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha3_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_sha3_cra_init,
				.cra_exit	  = aml_sha3_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha3_init,
		.update		= aml_sha3_update,
		.final		= aml_sha3_final,
		.finup		= aml_sha3_finup,
		.digest		= aml_sha3_digest,
		.export     = aml_sha3_export,
		.import     = aml_sha3_import,
		.halg = {
			.digestsize	= SHA3_384_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha3_reqctx),
			.base	= {
				.cra_name	  = "sha3-384-aml",
				.cra_driver_name  = "aml-sha3-384",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHA3_384_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha3_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_sha3_cra_init,
				.cra_exit	  = aml_sha3_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha3_init,
		.update		= aml_sha3_update,
		.final		= aml_sha3_final,
		.finup		= aml_sha3_finup,
		.digest		= aml_sha3_digest,
		.export     = aml_sha3_export,
		.import     = aml_sha3_import,
		.halg = {
			.digestsize	= SHA3_512_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha3_reqctx),
			.base	= {
				.cra_name	  = "sha3-512-aml",
				.cra_driver_name  = "aml-sha3-512",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHA3_512_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha3_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_sha3_cra_init,
				.cra_exit	  = aml_sha3_cra_exit,
			}
		}
	},
#if ENABLE_SHAKE128
	/* SHAKE-128 is not able to support due to blocksize over MAX_ALGAPI_BLOCKSIZE */
	{
		.init		= aml_shake_init,
		.update		= aml_shake_update,
		.final		= aml_shake_final,
		.finup		= aml_shake_finup,
		.digest		= aml_shake_digest,
		.export     = aml_shake_export,
		.import     = aml_shake_import,
		.halg = {
			.digestsize	= SHAKE_128_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha3_reqctx),
			.base	= {
				.cra_name	  = "shake-128-aml",
				.cra_driver_name  = "aml-shake-128",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHAKE_128_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha3_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_shake_cra_init,
				.cra_exit	  = aml_shake_cra_exit,
			}
		}
	},
#endif
	{
		.init		= aml_shake_init,
		.update		= aml_shake_update,
		.final		= aml_shake_final,
		.finup		= aml_shake_finup,
		.digest		= aml_shake_digest,
		.export     = aml_shake_export,
		.import     = aml_shake_import,
		.halg = {
			.digestsize	= SHAKE_256_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha3_reqctx),
			.base	= {
				.cra_name	  = "shake-256-aml",
				.cra_driver_name  = "aml-shake-256",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHAKE_256_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha3_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_shake_cra_init,
				.cra_exit	  = aml_shake_cra_exit,
			}
		}
	}
};

static void aml_sha3_unregister_algs(struct aml_sha3_dev *dd)
{
	int i = 0, count = ARRAY_SIZE(sha3_algs);

	for (i = count - 1; i >= 0; --i)
		crypto_unregister_ahash(&sha3_algs[i]);
}

static int aml_sha3_register_algs(struct aml_sha3_dev *dd)
{
	int err, i, j;

	for (i = 0; i < ARRAY_SIZE(sha3_algs); i++) {
		err = crypto_register_ahash(&sha3_algs[i]);
		if (err)
			goto err_sha_algs;
	}

	return 0;

err_sha_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_ahash(&sha3_algs[j]);
	return err;
}

static int aml_sha3_probe(struct platform_device *pdev)
{
	struct aml_sha3_dev *sha_dd;
	struct device *dev = &pdev->dev;
	int err = -EPERM;

	sha_dd = devm_kzalloc(dev, sizeof(struct aml_sha3_dev), GFP_KERNEL);
	if (!sha_dd) {
		err = -ENOMEM;
		goto sha_dd_err;
	}

	sha_dd->dev = dev;
	sha_dd->parent = dev->parent;
	sha_dd->dma = dev_get_drvdata(dev->parent);
	sha_dd->thread = sha_dd->dma->thread;
	sha_dd->status = sha_dd->dma->status;

	platform_set_drvdata(pdev, sha_dd);

	INIT_LIST_HEAD(&sha_dd->list);

	pm_runtime_use_autosuspend(dev);
	pm_runtime_set_autosuspend_delay(dev, DEFAULT_AUTOSUSPEND_DELAY);
	pm_runtime_irq_safe(dev);
	pm_runtime_enable(dev);

	err = pm_runtime_get_sync(dev);
	if (err < 0) {
		dev_err(dev, "%s: pm_runtime_get_sync fails: %d\n",
			__func__, err);
		goto err_pm;
	}

	aml_sha3_hw_init(sha_dd);

	spin_lock(&aml_sha3.lock);
	list_add_tail(&sha_dd->list, &aml_sha3.dev_list);
	spin_unlock(&aml_sha3.lock);

	err = aml_sha3_register_algs(sha_dd);
	if (err) {
		dev_err(dev, "%s %d, err = %d\n", __func__, __LINE__, err);
		goto err_algs;
	}

	dev_dbg(dev, "Aml SHA3-224/256/384/512 SHAKE-128/256 dma\n");

	pm_runtime_put_sync_autosuspend(dev);
	return 0;

err_algs:
	spin_lock(&aml_sha3.lock);
	list_del(&sha_dd->list);
	spin_unlock(&aml_sha3.lock);
err_pm:
	pm_runtime_disable(dev);
sha_dd_err:
	dev_err(dev, "initialization failed :<.\n");

	return err;
}

static void aml_sha3_remove(struct platform_device *pdev)
{
	static struct aml_sha3_dev *sha_dd;
	u32 dma_dsc_sz = aml_dma_get_dsc_sz(0, sha_dd->dma);

	sha_dd = platform_get_drvdata(pdev);
	if (!sha_dd)
		return;
	spin_lock(&aml_sha3.lock);
	list_del(&sha_dd->list);
	spin_unlock(&aml_sha3.lock);

	dmam_free_coherent(sha_dd->parent, MAX_NUM_TABLES * dma_dsc_sz,
			   sha_dd->descriptor, sha_dd->dma_descript_tab);

	aml_sha3_unregister_algs(sha_dd);
	pm_runtime_disable(sha_dd->parent);
}

#ifdef CONFIG_OF
static const struct of_device_id aml_sha3_dt_match[] = {
	{	.compatible = "amlogic,sha3_dma",
	},
	{},
};
#else
#define aml_sha3_dt_match NULL
#endif
static struct platform_driver aml_sha3_driver = {
	.probe		= aml_sha3_probe,
	.remove		= aml_sha3_remove,
	.driver		= {
		.name	= "aml_sha3_dma",
		.owner	= THIS_MODULE,
		.of_match_table	= aml_sha3_dt_match,
	},
};

int __init aml_sha3_driver_init(void)
{
	return platform_driver_register(&aml_sha3_driver);
}

void aml_sha3_driver_exit(void)
{
	platform_driver_unregister(&aml_sha3_driver);
}
