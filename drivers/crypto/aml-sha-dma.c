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
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/hash.h>
#include <crypto/internal/hash.h>
#include "aml-crypto-dma.h"
#include "aml-sha-dma.h"

/* SHA flags */
#define SHA_FLAGS_BUSY			BIT(0)
#define SHA_FLAGS_DMA_ACTIVE    BIT(1)
#define SHA_FLAGS_OUTPUT_READY  BIT(2)
#define SHA_FLAGS_INIT          BIT(3)
#define SHA_FLAGS_NEED_KEY      BIT(4)

#define SHA_FLAGS_FINAL         BIT(16)
#define SHA_FLAGS_SHA1          BIT(18)
#define SHA_FLAGS_SHA224        BIT(19)
#define SHA_FLAGS_SHA256        BIT(20)
#define SHA_FLAGS_ERROR         BIT(21)
#define SHA_FLAGS_FAST          BIT(22)

#define SHA_OP_UPDATE	1
#define SHA_OP_FINAL	2

#define SHA_BUFFER_LEN	\
	ALIGN_DOWN((512 - sizeof(struct aml_sha_reqctx)), 64)
#define DEFAULT_AUTOSUSPEND_DELAY	1000

struct aml_sha_dev;

struct aml_sha_reqctx {
	struct aml_sha_dev	*dd;
	unsigned long	flags;
	unsigned long	op;

	u64	digcnt[2];
	size_t	bufcnt;
	size_t	buflen;
	dma_addr_t	dma_addr;
	dma_addr_t	hash_addr;

	/* walk state */
	struct scatterlist	*sg;
	unsigned int	offset;	/* offset in current sg */
	unsigned int	total;	/* total request */

	size_t block_size;
	u32 fast_nents;

	u8  *hw_ctx;
	u8  buffer[] __aligned(sizeof(u32));
};

struct aml_sha_ctx {
	struct aml_sha_dev	*dd;
	u8	key[SHA512_BLOCK_SIZE];
	u32	keylen;
	/* 8 bytes bit length and 8 bytes garbage */
	u32 is_hmac;
	struct crypto_shash	*shash;
	s32 kte;
};

#define AML_SHA_QUEUE_LENGTH	50

struct aml_sha_dev {
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
	u32		hw_ctx_sz;
	u32		support_sha1_zero_len;
};

struct aml_sha_drv {
	struct list_head	dev_list;
	spinlock_t		lock; /* spinlock for device list */
};

static struct aml_sha_drv aml_sha = {
	.dev_list = LIST_HEAD_INIT(aml_sha.dev_list),
	.lock = __SPIN_LOCK_UNLOCKED(aml_sha.lock),
};

static int aml_sha_update_dma_stop(struct aml_sha_dev *dd);

static size_t aml_sha_append_sg(struct aml_sha_reqctx *ctx)
{
	size_t count = 0;

	if (!ctx->sg)
		return 0;

	while ((ctx->bufcnt < ctx->buflen) && ctx->total) {
		count = min(ctx->sg->length - ctx->offset, ctx->total);
		count = min(count, ctx->buflen - ctx->bufcnt);

		if (count <= 0)
			break;

		scatterwalk_map_and_copy(ctx->buffer + ctx->bufcnt, ctx->sg,
					 ctx->offset, count, 0);

		ctx->bufcnt += count;
		ctx->offset += count;
		ctx->total -= count;

		if (ctx->offset == ctx->sg->length) {
			ctx->sg = sg_next(ctx->sg);
			ctx->offset = 0;
		}
	}

	return 0;
}

int aml_sha_init(struct ahash_request *req)
{
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct aml_sha_ctx *tctx = crypto_ahash_ctx(tfm);
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha_dev *dd = NULL;
	struct aml_sha_dev *tmp;

	spin_lock_bh(&aml_sha.lock);
	if (!tctx->dd) {
		list_for_each_entry(tmp, &aml_sha.dev_list, list) {
			dd = tmp;
			break;
		}
		tctx->dd = dd;
	} else {
		dd = tctx->dd;
	}

	spin_unlock_bh(&aml_sha.lock);

	ctx->dd = dd;

	ctx->flags = 0;

	dbgp(1, "init: tfm: %p, ctx: %p, digest size: %d\n",
	     tfm, ctx, crypto_ahash_digestsize(tfm));

	switch (crypto_ahash_digestsize(tfm)) {
	case SHA1_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHA1;
		ctx->block_size = SHA1_BLOCK_SIZE;
		break;
	case SHA224_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHA224;
		ctx->block_size = SHA224_BLOCK_SIZE;
		break;
	case SHA256_DIGEST_SIZE:
		ctx->flags |= SHA_FLAGS_SHA256;
		ctx->block_size = SHA256_BLOCK_SIZE;
		break;
	default:
		return -EINVAL;
	}

	ctx->hw_ctx = kzalloc(dd->hw_ctx_sz, GFP_ATOMIC | GFP_DMA);
	if (!ctx->hw_ctx)
		return -ENOMEM;

	if (tctx->is_hmac)
		ctx->flags |= SHA_FLAGS_NEED_KEY;

	ctx->bufcnt = 0;
	ctx->digcnt[0] = 0;
	ctx->digcnt[1] = 0;
	ctx->fast_nents = 0;
	ctx->buflen = SHA_BUFFER_LEN;

	return 0;
}

static int aml_sha_xmit_dma(struct aml_sha_dev *dd,
			    struct ahash_request *req, void *descriptor,
			    u32 nents, int final)
{
	int i = 0;
	u32 mode;
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	size_t length = 0;
	u32 status = 0;
	int err = 0;
	struct dma_dsc_v2_64 dsc;

	dbgp(1, "xmit_dma:ctx:%p,digcnt:0x%llx 0x%llx,nents: %u,final:%d\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0], nents, final);

	mode = MODE_SHA1;

	if (ctx->flags & SHA_FLAGS_SHA224)
		mode = MODE_SHA224;
	else if (ctx->flags & SHA_FLAGS_SHA256)
		mode = MODE_SHA256;

	ctx->hash_addr = dma_map_single(dd->parent, ctx->hw_ctx,
					dd->hw_ctx_sz, DMA_FROM_DEVICE);
	if (dma_mapping_error(dd->parent, ctx->hash_addr)) {
		dev_err(dd->dev, "hash %d bytes error\n",
			dd->hw_ctx_sz);
		return -EINVAL;
	}
	for (i = 0; i < nents; i++) {
		/* some fields are already filled, read it back and update dsc */
		aml_dma_dsc_reader(descriptor, i, &dsc, 0, dd->dma);

		dsc.tgt_addr = (uintptr_t)ctx->hash_addr;
		dsc.dsc_cfg.b.enc_sha_only = 1;
		dsc.dsc_cfg.b.mode = mode;
		dsc.dsc_cfg.b.begin =
		   (i == 0 && !(ctx->digcnt[0] || ctx->digcnt[1]) &&
		   !(tctx->is_hmac));
		dsc.dsc_cfg.b.end = final;
		dsc.dsc_cfg.b.op_mode = OP_MODE_SHA;
		dsc.dsc_cfg.b.eoc = (i == (nents - 1));
		dsc.dsc_cfg.b.owner = 1;

		aml_dma_dsc_writer(descriptor, i, &dsc, 0, dd->dma);

		length = dsc.length;
		ctx->digcnt[0] += length;
		if (ctx->digcnt[0] < length)
			ctx->digcnt[1]++;
	}
	aml_dma_debug(descriptor, nents, __func__, dd->thread, dd->status, dd->dma);
	/* should be non-zero before next lines to disable clocks later */
	if (final)
		ctx->flags |= SHA_FLAGS_FINAL; /* catch last interrupt */

	dd->req = req;

	dbgp(1, "xmit before:ctx:%p,digcnt:0x%llx 0x%llx,length:%zd,final:%d\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0], length, final);

	/* Start DMA transfer */
	dd->flags |=  SHA_FLAGS_DMA_ACTIVE;
	status = aml_dma_do_hw_crypto(dd->dma, descriptor, nents, dd->dma_descript_tab,
				      USE_BUSY_POLLING, DMA_FLAG_SHA_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR)
		dd->flags |= SHA_FLAGS_ERROR;
	aml_dma_debug(descriptor, nents, "end sha", dd->thread, dd->status, dd->dma);

	dd->flags |= SHA_FLAGS_OUTPUT_READY;
	dd->flags &= ~SHA_FLAGS_DMA_ACTIVE;

	err = aml_sha_update_dma_stop(dd);
	if (!err) {
		err = dd->flags & SHA_FLAGS_ERROR;
		dd->flags = (dd->flags & SHA_FLAGS_ERROR);
	}

	return err;
}

static int aml_sha_xmit_start(struct aml_sha_dev *dd,
			      struct ahash_request *req, void *dsc,
			      u32 nents, int final)
{
	return aml_sha_xmit_dma(dd, req, dsc, nents, final);
}

static int aml_sha_xmit_dma_map(struct aml_sha_dev *dd,
				struct ahash_request *req,
				struct aml_sha_reqctx *ctx,
				size_t length, int final)
{
	struct dma_dsc_v2_64 dsc;

	ctx->dma_addr = dma_map_single(dd->parent, ctx->buffer,
				       ctx->buflen, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->parent, ctx->dma_addr)) {
		dev_err(dd->dev, "dma %zd bytes error\n", ctx->buflen);
		return -EINVAL;
	}

	dma_sync_single_for_device(dd->parent, ctx->dma_addr,
				   SHA_BUFFER_LEN, DMA_TO_DEVICE);

	dsc.src_addr = (uintptr_t)ctx->dma_addr;
	dsc.length = length;
	dsc.dsc_cfg.d32 = 0;
	aml_dma_dsc_writer(dd->descriptor, 0, &dsc, 0, dd->dma);

	/* next call does not fail... so no unmap in the case of error */
	return aml_sha_xmit_start(dd, req, dd->descriptor, 1, final);
}

static int aml_sha_update_dma_slow(struct aml_sha_dev *dd,
				   struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	unsigned int final;
	size_t count;

	ctx->flags &= ~SHA_FLAGS_FAST;
	aml_sha_append_sg(ctx);

	final = (ctx->flags & SHA_FLAGS_FINAL) && !ctx->total;

	dbgp(1, "slow:ctx:%p,bc:%zd,dc:0x%llx 0x%llx,final:%d,total:%u\n",
	     ctx, ctx->bufcnt, ctx->digcnt[1], ctx->digcnt[0], final,
	     ctx->total);

	if (IS_ALIGNED(ctx->bufcnt, ctx->block_size) || final) {
		count = ctx->bufcnt;
		ctx->bufcnt = 0;
		return aml_sha_xmit_dma_map(dd, req, ctx, count, final);
	}
	return 0;
}

/* This function will adjust position of ctx->sg
 * according to the fast_nents which has been issued
 * to dma hw
 */
static void aml_sha_adjust_sg_head(struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);

	if (!ctx->total) {
		ctx->fast_nents = 0;
		return;
	}

	if (ctx->fast_nents) {
		/* walk across (nents - 1) sg */
		/* because the last sg maybe only partially used */
		while (ctx->fast_nents > 1) {
			ctx->sg = sg_next(ctx->sg);
			ctx->fast_nents--;
		}
		if (ctx->offset == ctx->sg->length) {
			ctx->sg = sg_next(ctx->sg);
			if (ctx->sg)
				ctx->offset = 0;
			else
				return; /* ctx->total should be 0 */
		}
		ctx->fast_nents = 0;
	}
}

static int aml_sha_update_dma_start(struct aml_sha_dev *dd,
				    struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	unsigned int length = 0, final = 0, tail = 0;
	struct scatterlist *sg;
	struct dma_dsc_v2_64 dsc;
	struct device *dev = dd->dev;
	int err = 0;

	dbgp(1, "start: ctx: %p, total: %u, fast_nents: %u offset: %u,",
	     ctx, ctx->total, ctx->fast_nents, ctx->offset);
	dbgp(1, " block size: %zd, flag: %lx\n",
	     ctx->block_size, ctx->flags);

	/* Sanity check */
	if (!ctx->total || !ctx->sg) {
		ctx->fast_nents = 0;
		return 0;
	}

	/* walk across (nents - 1) sg */
	/* because the last sg maybe only partially used */
	while (ctx->fast_nents > 1) {
		ctx->sg = sg_next(ctx->sg);
		ctx->fast_nents--;
	}

	if (!ctx->sg) {
		pr_err("%s:%d: no more sg found\n",
		       __func__, __LINE__);
		return 0;
	}

	/* Run out of data in current sg.
	 * Move sg to next.
	 */
	if (ctx->offset == ctx->sg->length) {
		ctx->sg = sg_next(ctx->sg);
		if (ctx->sg)
			ctx->offset = 0;
		else
			return 0;
	}

	ctx->fast_nents = 0;

	if (ctx->bufcnt || ctx->offset ||
	    ctx->total < ctx->block_size ||
	    ctx->sg->length < ctx->block_size) {
		do {
			err = aml_sha_update_dma_slow(dd, req);
		} while (!err && ctx->total);
		return err;
	}

	sg = ctx->sg;

	while (ctx->total && ctx->fast_nents < MAX_NUM_TABLES) {
		dbgp(1, "fast:ctx:%p,dig:0x%llx 0x%llx,bc:%zd,tot:%u,sgl: %u\n",
		     ctx, ctx->digcnt[1], ctx->digcnt[0],
		ctx->bufcnt, ctx->total, ctx->sg->length);

		length = min(ctx->total, sg->length);

		if (!(ctx->flags & SHA_FLAGS_FINAL && sg_next(sg) == 0)) {
			/* not last sg must be ctx->block_size aligned */
			tail = length & (ctx->block_size - 1);
			length -= tail;
		}

		ctx->total -= length;
		ctx->offset = length; /* offset where to start slow */

		final = (ctx->flags & SHA_FLAGS_FINAL) && !ctx->total;

		err = dma_map_sg(dd->parent, sg, 1, DMA_TO_DEVICE);
		if (!err) {
			dev_err(dev, "dma_map_sg() error\n");
			return 0;
		}

		ctx->dma_addr = sg_dma_address(sg);

		dsc.src_addr = (uintptr_t)ctx->dma_addr;
		dsc.tgt_addr = 0;
		dsc.length = length;
		dsc.dsc_cfg.d32 = 0;
		aml_dma_dsc_writer(dd->descriptor, ctx->fast_nents,
				   &dsc, 0, dd->dma);

		sg = sg_next(sg);
		ctx->fast_nents++;

		dbgp(1, "fast: ctx: %p, total: %u, offset: %u, tail: %u\n",
		     ctx, ctx->total, ctx->offset, tail);

		if (tail)
			break;
	}
	if (ctx->fast_nents) {
		ctx->flags |= SHA_FLAGS_FAST;
		err = aml_sha_xmit_start(dd, req, dd->descriptor, ctx->fast_nents, final);
		if (err && err != -EINPROGRESS) {
			pr_err("%s:%d: aml_sha_xmit_start failed with err = %d\n"
					, __func__, __LINE__, err);
			return err;
		}
		if (!err && ctx->total) {
			ctx->flags &= ~SHA_FLAGS_FAST;
			// If there is unhandled data,
			// adjust position of sg head and issue slow dma
			aml_sha_adjust_sg_head(req);
			do {
				err = aml_sha_update_dma_slow(dd, req);
			} while (!err && ctx->total);
		}
		return err;
	}
	return 0;
}

static int aml_sha_update_dma_stop(struct aml_sha_dev *dd)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(dd->req);

	aml_dma_debug(dd->descriptor, ctx->fast_nents ?
		      ctx->fast_nents : 1, __func__, dd->thread, dd->status, dd->dma);

	if (ctx->flags & SHA_FLAGS_FAST)
		dma_unmap_sg(dd->parent, ctx->sg, ctx->fast_nents, DMA_TO_DEVICE);
	else
		dma_unmap_single(dd->parent, ctx->dma_addr,
				 ctx->buflen, DMA_TO_DEVICE);

	if (ctx->hash_addr)
		dma_unmap_single(dd->parent, ctx->hash_addr,
				 dd->hw_ctx_sz, DMA_FROM_DEVICE);
	return 0;
}

static int aml_sha_update_req(struct aml_sha_dev *dd, struct ahash_request *req)
{
	int err;
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);

	dbgp(1, "update_req: ctx: %p, total: %u, digcnt: 0x%llx 0x%llx\n",
	     ctx, ctx->total, ctx->digcnt[1], ctx->digcnt[0]);

	err = aml_sha_update_dma_start(dd, req);

	return err;
}

static int aml_sha_final_req(struct aml_sha_dev *dd, struct ahash_request *req)
{
	int err = 0;
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);

	err = aml_sha_update_dma_slow(dd, req);

	dbgp(1, "final_req: ctx: %p, err: %d\n", ctx, err);

	return err;
}

static void aml_sha_copy_ready_hash(struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);

	if (!req->result)
		return;

	if (ctx->flags & SHA_FLAGS_SHA1)
		memcpy(req->result, ctx->hw_ctx, SHA1_DIGEST_SIZE);
	else if (ctx->flags & SHA_FLAGS_SHA224)
		memcpy(req->result, ctx->hw_ctx, SHA224_DIGEST_SIZE);
	else if (ctx->flags & SHA_FLAGS_SHA256)
		memcpy(req->result, ctx->hw_ctx, SHA256_DIGEST_SIZE);
}

static int aml_sha_finish_hmac(struct ahash_request *req)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha_dev *dd = ctx->dd;
	struct dma_dsc_v2_64 dsc;
	dma_addr_t hmac_hw_ctx = 0;
	u8 *hw_ctx = 0;
	u32 mode;
	u32 ds = 0;
	u8 *key;
	dma_addr_t dma_key = 0;
	u32 status = 0;
	int err = 0;

	hw_ctx = dmam_alloc_coherent(dd->parent, dd->hw_ctx_sz,
				     &hmac_hw_ctx, GFP_ATOMIC | GFP_DMA);
	if (!hw_ctx)
		return -ENOMEM;

	memcpy(hw_ctx, ctx->hw_ctx, dd->hw_ctx_sz);

	key = kzalloc(tctx->keylen, GFP_ATOMIC | GFP_DMA);
	if (!key)
		return -ENOMEM;

	memcpy(key, tctx->key, tctx->keylen);
	dma_key = dma_map_single(dd->parent, key,
				 tctx->keylen, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->parent, dma_key)) {
		dev_err(dd->dev, "mapping key addr failed: %d\n", tctx->keylen);
		dmam_free_coherent(dd->parent, dd->hw_ctx_sz,
				   hw_ctx, hmac_hw_ctx);
		return -EINVAL;
	}

	mode = MODE_SHA1;
	if (ctx->flags & SHA_FLAGS_SHA224)
		mode = MODE_SHA224;
	else if (ctx->flags & SHA_FLAGS_SHA256)
		mode = MODE_SHA256;

	if (ctx->flags & SHA_FLAGS_SHA1)
		ds = SHA1_DIGEST_SIZE;
	else if (ctx->flags & SHA_FLAGS_SHA224)
		ds = SHA224_DIGEST_SIZE;
	else if (ctx->flags & SHA_FLAGS_SHA256)
		ds = SHA256_DIGEST_SIZE;

	/* opad */
	dsc.src_addr = (tctx->kte >= 0) ?
		(u64)(0xffffffffffffff00 | tctx->kte) :
		(uintptr_t)dma_key;
	dsc.tgt_addr = 0;
	dsc.length = tctx->keylen;
	dsc.dsc_cfg.d32 = 0;
	dsc.dsc_cfg.b.mode = mode;
	dsc.dsc_cfg.b.enc_sha_only = 1;
	dsc.dsc_cfg.b.op_mode = OP_MODE_HMAC_O;
	dsc.dsc_cfg.b.begin = 1;
	dsc.dsc_cfg.b.end = 0;
	dsc.dsc_cfg.b.eoc = 0;
	dsc.dsc_cfg.b.owner = 1;
	aml_dma_dsc_writer(dd->descriptor, 0, &dsc, 0, dd->dma);

	/* 2nd stage hash */
	dsc.src_addr = (uintptr_t)hmac_hw_ctx;
	dsc.tgt_addr = (uintptr_t)hmac_hw_ctx;
	dsc.length = ds;
	dsc.dsc_cfg.d32 = 0;
	dsc.dsc_cfg.b.mode = mode;
	dsc.dsc_cfg.b.enc_sha_only = 1;
	dsc.dsc_cfg.b.begin = 0;
	dsc.dsc_cfg.b.end = 1;
	dsc.dsc_cfg.b.eoc = 1;
	dsc.dsc_cfg.b.owner = 1;
	aml_dma_dsc_writer(dd->descriptor, 1, &dsc, 0, dd->dma);

	aml_dma_debug(dd->descriptor, 2, __func__, dd->thread, dd->status, dd->dma);
	status = aml_dma_do_hw_crypto(dd->dma, dd->descriptor, 2, dd->dma_descript_tab,
				      USE_BUSY_POLLING, DMA_FLAG_SHA_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR) {
		dev_err(dd->dev, "hw crypto failed, status: %u\n", status);
		err = -EINVAL;
	}
	aml_dma_debug(dd->descriptor, 2, "end hmac", dd->thread, dd->status, dd->dma);
	dma_unmap_single(dd->parent, dma_key,
			 tctx->keylen, DMA_TO_DEVICE);
	memcpy(ctx->hw_ctx, hw_ctx, dd->hw_ctx_sz);
	dmam_free_coherent(dd->parent, dd->hw_ctx_sz, hw_ctx, hmac_hw_ctx);

	return err;
}

static void aml_sha_clean_key(struct ahash_request *req)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha_dev *dd = tctx->dd;
	void *descriptor = NULL;
	struct dma_dsc_v2_64 dsc;
	dma_addr_t dma_key = 0;
	dma_addr_t dma_descript_tab = 0;
	u8 *key = 0;
	size_t dsc_sz = 0;

	dsc_sz = aml_dma_get_dsc_sz(0, dd->dma);
	descriptor = dmam_alloc_coherent(dd->parent, dsc_sz,
					 &dma_descript_tab, GFP_ATOMIC | GFP_DMA);
	if (!descriptor)
		return;

	key = dmam_alloc_coherent(dd->parent, tctx->keylen,
				  &dma_key, GFP_ATOMIC | GFP_DMA);
	if (!key) {
		goto out;
	}

	memset(key, 0, tctx->keylen);

	dsc.src_addr = (u64)dma_key;
	dsc.tgt_addr = 0;
	dsc.length = tctx->keylen;
	dsc.dsc_cfg.d32 = 0;
	dsc.dsc_cfg.b.mode = MODE_KEY;
	dsc.dsc_cfg.b.eoc = 1;
	dsc.dsc_cfg.b.owner = 1;
	aml_dma_dsc_writer(descriptor, 0, &dsc, 0, dd->dma);

	aml_dma_debug(descriptor, 1, __func__, dd->thread,
			dd->status, dd->dma);

	aml_write_crypto_reg(dd->dma, dd->thread,
			     (uintptr_t)dma_descript_tab | 2);
	while (aml_read_crypto_reg(dd->status) == 0)
		;
	aml_write_crypto_reg(dd->dma, dd->status, 0xffff);
out:
	if (key)
		dmam_free_coherent(dd->parent, tctx->keylen, key, dma_key);
	if (descriptor)
		dmam_free_coherent(dd->parent, dsc_sz,
				   descriptor, dma_descript_tab);
}

static int aml_sha_finish(struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	int err = 0;

	if (tctx->is_hmac) {
		err = aml_sha_finish_hmac(req);
		aml_sha_clean_key(req);
	}
	aml_sha_copy_ready_hash(req);

	kfree(ctx->hw_ctx);
	dbgp(1, "finish digcnt: ctx: %p, 0x%llx 0x%llx, bufcnt: %zd, %x %x\n",
	     ctx, ctx->digcnt[1], ctx->digcnt[0], ctx->bufcnt,
	     req->result[0], req->result[1]);

	return err;
}

void aml_sha_finish_req(struct ahash_request *req, int err)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha_dev *dd = ctx->dd;
	unsigned long flags;

	if (!err) {
		if (SHA_FLAGS_FINAL & ctx->flags)
			err = aml_sha_finish(req);
	} else {
		ctx->flags |= SHA_FLAGS_ERROR;
	}

	spin_lock_irqsave(&dd->dma->dma_lock, flags);
	dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
	dd->flags &= ~(SHA_FLAGS_BUSY | SHA_FLAGS_OUTPUT_READY);
	spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
}

static int aml_sha_hw_init(struct aml_sha_dev *dd)
{
	if (!(dd->flags & SHA_FLAGS_INIT)) {
		size_t dsc_sz = aml_dma_get_dsc_sz(0, dd->dma);

		dd->descriptor =
			dmam_alloc_coherent(dd->parent,
					    MAX_NUM_TABLES * dsc_sz,
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

static int aml_sha_state_restore(struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha_dev *dd = tctx->dd;
	dma_addr_t dma_ctx;
	struct dma_dsc_v2_64 dsc;
	s32 len = dd->hw_ctx_sz;
	u32 status = 0;
	int err = 0;

	if (!ctx->digcnt[0] && !ctx->digcnt[1])
		return err;

	dma_ctx = dma_map_single(dd->parent, ctx->hw_ctx,
				 dd->hw_ctx_sz, DMA_TO_DEVICE);
	if (dma_mapping_error(dd->parent, dma_ctx)) {
		dev_err(dd->dev, "mapping dma_ctx failed\n");
		return -ENOMEM;
	}

	dsc.src_addr = (uintptr_t)dma_ctx;
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
	dma_unmap_single(dd->parent, dma_ctx,
			 dd->hw_ctx_sz, DMA_TO_DEVICE);
	return err;
}

static int aml_hmac_install_key(struct aml_sha_dev *dd,
				struct ahash_request *req)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);
	struct crypto_ahash *tfm = crypto_ahash_reqtfm(req);
	struct dma_dsc_v2_64 dsc;
	void *descriptor = 0;
	size_t dsc_sz = 0;
	size_t dsc_cnt = 1;
	u32 bs = 0;
	int err = 0;
	dma_addr_t dma_key = 0;
	dma_addr_t dma_descript_tab = 0;
	dma_addr_t partial = 0;
	u8 *key = 0;
	u8 *hw_ctx = 0;
	u32 i;
	u32 mode = MODE_SHA1;
	u32 status = 0;

	switch (crypto_ahash_digestsize(tfm)) {
	case SHA1_DIGEST_SIZE:
		bs = SHA1_BLOCK_SIZE;
		break;
	case SHA224_DIGEST_SIZE:
		bs = SHA224_BLOCK_SIZE;
		break;
	case SHA256_DIGEST_SIZE:
		bs = SHA256_BLOCK_SIZE;
		break;
	default:
		return -EINVAL;
	}

	mode = MODE_SHA1;
	if (ctx->flags & SHA_FLAGS_SHA224)
		mode = MODE_SHA224;
	else if (ctx->flags & SHA_FLAGS_SHA256)
		mode = MODE_SHA256;

	if (tctx->kte >= 0)
		dsc_cnt++;

	dsc_sz = aml_dma_get_dsc_sz(0, dd->dma);
	descriptor = dmam_alloc_coherent(dd->parent, dsc_sz,
					 &dma_descript_tab, GFP_ATOMIC | GFP_DMA);
	if (!descriptor) {
		dd->flags &= ~SHA_FLAGS_BUSY;
		return -ENOMEM;
	}
	key = dmam_alloc_coherent(dd->parent, bs,
				  &dma_key, GFP_ATOMIC | GFP_DMA);
	if (!key) {
		err = -ENOMEM;
		goto out;
	}
	memcpy(key, tctx->key, tctx->keylen);

	if (tctx->kte < 0) {
		/* If key is NOT from kt, do software ipad */
		memset(key + tctx->keylen, 0, bs - tctx->keylen);
		/* Do ipad */
		for (i = 0; i < bs; i++)
			*(u8 *)(key + i) ^= 0x36;
	}

	hw_ctx = dmam_alloc_coherent(dd->parent, dd->hw_ctx_sz,
				     &partial, GFP_ATOMIC | GFP_DMA);
	if (!hw_ctx) {
		err = -ENOMEM;
		goto out;
	}

	i = 0;
	if (tctx->kte >= 0) {
		dsc.src_addr = (u64)(0xffffffffffffff00 | tctx->kte);
		dsc.tgt_addr = 0;
		dsc.length = tctx->keylen;
		dsc.dsc_cfg.d32 = 0;
		dsc.dsc_cfg.b.mode = mode;
		dsc.dsc_cfg.b.enc_sha_only = 1;
		dsc.dsc_cfg.b.op_mode = OP_MODE_HMAC_I;
		dsc.dsc_cfg.b.begin = 1;
		dsc.dsc_cfg.b.end = 0;
		dsc.dsc_cfg.b.eoc = 0;
		dsc.dsc_cfg.b.owner = 1;
		aml_dma_dsc_writer(descriptor, i++, &dsc, 0, dd->dma);
	}
	dsc.src_addr = dma_key;
	dsc.tgt_addr = partial;
	dsc.length = (tctx->kte >= 0) ? 0 : bs;
	dsc.dsc_cfg.d32 = 0;
	dsc.dsc_cfg.b.mode = mode;
	dsc.dsc_cfg.b.enc_sha_only = 1;
	dsc.dsc_cfg.b.op_mode = OP_MODE_SHA;
	dsc.dsc_cfg.b.begin = i ? 0 : 1;
	dsc.dsc_cfg.b.end = 0;
	dsc.dsc_cfg.b.eoc = 1;
	dsc.dsc_cfg.b.owner = 1;
	aml_dma_dsc_writer(descriptor, i, &dsc, 0, dd->dma);

	aml_dma_debug(descriptor, dsc_cnt, __func__, dd->thread, dd->status,
		      dd->dma);
	status = aml_dma_do_hw_crypto(dd->dma, descriptor, dsc_cnt, dma_descript_tab,
				      USE_BUSY_POLLING, DMA_FLAG_SHA_IN_USE);
	if (status & DMA_STATUS_KEY_ERROR) {
		dev_err(dd->dev, "hw crypto failed, status: %u\n", status);
		err = -EINVAL;
	}
	aml_dma_debug(descriptor, dsc_cnt, "end hmac key", dd->thread, dd->status,
			dd->dma);
	/* Save hw state in ctx */
	memcpy(ctx->hw_ctx, hw_ctx, dd->hw_ctx_sz);
out:
	if (key)
		dmam_free_coherent(dd->parent, bs, key, dma_key);
	if (descriptor)
		dmam_free_coherent(dd->parent, dsc_sz,
				descriptor, dma_descript_tab);
	if (hw_ctx)
		dmam_free_coherent(dd->parent, dd->hw_ctx_sz, hw_ctx, partial);
	return err;
}

static int aml_sha_handle_queue(struct aml_sha_dev *dd,
				struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx;
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

	dbgp(1, "handling new req, ctx: %p, op: %lu, nbytes: %d\n",
	     ctx, ctx->op, req->nbytes);

	if (ctx->flags & SHA_FLAGS_NEED_KEY) {
		/* hmac */
		err = aml_hmac_install_key(dd, req);
		if (err)
			goto err1;
		ctx->flags &= ~SHA_FLAGS_NEED_KEY;
	}

	err = aml_sha_state_restore(req);
	if (err)
		goto err1;

	if (ctx->op == SHA_OP_UPDATE) {
		err = aml_sha_update_req(dd, req);
		if (!err) {
			spin_lock_irqsave(&dd->dma->dma_lock, flags);
			dd->flags &= ~SHA_FLAGS_BUSY;
			dd->dma->dma_busy &= ~DMA_FLAG_MAY_OCCUPY;
			spin_unlock_irqrestore(&dd->dma->dma_lock, flags);
		}
	} else if (ctx->op == SHA_OP_FINAL) {
		err = aml_sha_final_req(dd, req);
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

static int aml_sha_enqueue(struct ahash_request *req, unsigned int op)
{
	struct aml_sha_reqctx *ctx = NULL;
	struct aml_sha_ctx *tctx = NULL;
	struct aml_sha_dev *dd = NULL;
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

int aml_sha_process(struct ahash_request *req)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(req->base.tfm);
	struct aml_sha_dev *dd = tctx->dd;

	return aml_sha_handle_queue(dd, req);
}

static int _aml_sha_update(struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);

	dbgp(1, "%s:%d:req->nbytes = %d\n",
	     __func__, __LINE__, req->nbytes);

	ctx->total = req->nbytes;
	ctx->sg = req->src;
	ctx->offset = 0;
	ctx->fast_nents = 0;

	if (!req->nbytes)
		return 0;

	return aml_sha_enqueue(req, SHA_OP_UPDATE);
}

int aml_sha_update(struct ahash_request *req)
{
	return _aml_sha_update(req);
}

static int _aml_sha_final(struct ahash_request *req)
{
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);

	ctx->flags |= SHA_FLAGS_FINAL;
	ctx->total = 0;
	ctx->sg = 0;
	ctx->offset = 0;

	if (ctx->flags & SHA_FLAGS_ERROR)
		return 0; /* uncompleted hash is not needed */

	return aml_sha_enqueue(req, SHA_OP_FINAL);
}

int aml_sha_final(struct ahash_request *req)
{
	int err = 0;

	dbgp(1, "%s:%d: req->nbytes = %d\n",
	     __func__, __LINE__, req->nbytes);
	err =  _aml_sha_final(req);
	return err;
}

static int _aml_sha_finup(struct ahash_request *req)
{
	int err;
	struct aml_sha_reqctx *ctx = ahash_request_ctx(req);

	ctx->flags |= SHA_FLAGS_FINAL;
	err = _aml_sha_update(req);
	if (err == -EINPROGRESS || err == -EBUSY)
		return err;
	/*
	 * final() has to be always called to cleanup resources
	 * even if update() failed, except EINPROGRESS
	 */
	err = _aml_sha_final(req);

	return err;
}

int aml_sha_finup(struct ahash_request *req)
{
	s32 err = 0;

	err = _aml_sha_finup(req);
	return err;
}

int aml_sha_digest(struct ahash_request *req)
{
	int err = 0;

	err = aml_sha_init(req);
	if (err) {
		pr_err("%s:%d:aml_sha_init fails\n", __func__, __LINE__);
		goto out;
	}

	err = _aml_sha_finup(req);
	if (err && (err != -EINPROGRESS && err != -EBUSY)) {
		pr_err("%s:%d:_aml_sha_finup failed\n", __func__, __LINE__);
		goto out;
	}
out:
	return err;
}

int aml_sha_import(struct ahash_request *req, const void *in)
{
	struct aml_sha_reqctx *rctx = ahash_request_ctx(req);
	const struct aml_sha_reqctx *ctx_in = in;

	memcpy(rctx, in, sizeof(*rctx) + ctx_in->bufcnt);
	return 0;
}

int aml_sha_export(struct ahash_request *req, void *out)
{
	struct aml_sha_reqctx *rctx = ahash_request_ctx(req);

	memcpy(out, rctx, sizeof(*rctx) + rctx->bufcnt);
	return 0;
}

int aml_shash_digest(struct crypto_shash *tfm, u32 flags,
		     const u8 *data, unsigned int len, u8 *out)
{
	struct shash_desc *shash = NULL;
	int err;

	shash = kzalloc(sizeof(*shash) +
		crypto_shash_descsize(tfm), GFP_KERNEL | GFP_ATOMIC);
	if (!shash)
		return -ENOMEM;

	shash->tfm = tfm;
	//shash->flags = flags & ~CRYPTO_TFM_REQ_MAY_SLEEP;

	err = crypto_shash_digest(shash, data, len, out);

	kfree(shash);
	return err;
}

int aml_sha_setkey(struct crypto_ahash *tfm, const u8 *key,
		   unsigned int keylen)
{
	struct aml_sha_ctx *tctx = crypto_ahash_ctx(tfm);
	int bs = crypto_shash_blocksize(tctx->shash);
	int ds = crypto_shash_digestsize(tctx->shash);
	struct aml_sha_dev *dd = NULL, *tmp;
	int err = 0;

	spin_lock_bh(&aml_sha.lock);
	if (!tctx->dd) {
		list_for_each_entry(tmp, &aml_sha.dev_list, list) {
			dd = tmp;
			break;
		}
		tctx->dd = dd;
	} else {
		dd = tctx->dd;
	}
	spin_unlock_bh(&aml_sha.lock);

	tctx->kte = -1;
	if (keylen > bs) {
		err = aml_shash_digest(tctx->shash,
				       crypto_shash_get_flags(tctx->shash),
				       key, keylen, tctx->key);
		if (err)
			return err;
		tctx->keylen = ds;
	} else {
		tctx->keylen = keylen;
		memcpy(tctx->key, key, tctx->keylen);
	}

	return err;
}

int aml_sha_kl_setkey(struct crypto_ahash *tfm, const u8 *key,
		   unsigned int keylen)
{
	struct aml_sha_ctx *tctx = crypto_ahash_ctx(tfm);
	struct aml_sha_dev *dd = NULL, *tmp;
	int err = 0;

	spin_lock_bh(&aml_sha.lock);
	if (!tctx->dd) {
		list_for_each_entry(tmp, &aml_sha.dev_list, list) {
			dd = tmp;
			break;
		}
		tctx->dd = dd;
	} else {
		dd = tctx->dd;
	}
	spin_unlock_bh(&aml_sha.lock);

	/* key[0:3] = kte */
	tctx->kte = *(uint32_t *)&key[0];
	WARN_ON(!(keylen == 16 || keylen == 32));
	tctx->keylen = keylen;

	return err;
}

static int aml_sha_cra_init_alg(struct crypto_tfm *tfm, const char *alg_base)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(tfm);

	crypto_ahash_set_reqsize(__crypto_ahash_cast(tfm),
				 sizeof(struct aml_sha_reqctx) +
				 SHA_BUFFER_LEN);

	if (alg_base) {
		tctx->shash = crypto_alloc_shash(alg_base, 0,
						 CRYPTO_ALG_NEED_FALLBACK);
		if (IS_ERR(tctx->shash)) {
			pr_err("aml-sha: could not load base driver '%s' c ",
			       alg_base);
			return PTR_ERR(tctx->shash);
		}
	} else {
		tctx->shash = NULL;
	}

	return 0;
}

static int aml_sha_cra_init(struct crypto_tfm *tfm)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(tfm);

	tctx->is_hmac = 0;
	return aml_sha_cra_init_alg(tfm, NULL);
}

static void aml_sha_cra_exit(struct crypto_tfm *tfm)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(tfm);

	if (tctx->is_hmac && tctx->shash)
		crypto_free_shash(tctx->shash);
}

static int aml_hmac_sha1_cra_init(struct crypto_tfm *tfm)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(tfm);

	tctx->is_hmac = 1;
	return aml_sha_cra_init_alg(tfm, "sha1");
}

static int aml_hmac_sha224_cra_init(struct crypto_tfm *tfm)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(tfm);

	tctx->is_hmac = 1;
	return aml_sha_cra_init_alg(tfm, "sha224");
}

static int aml_hmac_sha256_cra_init(struct crypto_tfm *tfm)
{
	struct aml_sha_ctx *tctx = crypto_tfm_ctx(tfm);

	tctx->is_hmac = 1;
	return aml_sha_cra_init_alg(tfm, "sha256");
}

static void aml_hmac_cra_exit(struct crypto_tfm *tfm)
{
}

static struct ahash_alg sha_algs[] = {
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.halg = {
			.digestsize	= SHA1_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "sha1-aml",
				.cra_driver_name  = "aml-sha1",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHA1_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_sha_cra_init,
				.cra_exit	  = aml_sha_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.halg = {
			.digestsize	= SHA256_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "sha256-aml",
				.cra_driver_name  = "aml-sha256",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHA256_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_sha_cra_init,
				.cra_exit	  = aml_sha_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.halg = {
			.digestsize	= SHA224_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "sha224-aml",
				.cra_driver_name  = "aml-sha224",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC,
				.cra_blocksize	  = SHA224_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_sha_cra_init,
				.cra_exit	  = aml_sha_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.setkey         = aml_sha_setkey,
		.halg = {
			.digestsize	= SHA1_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "hmac(sha1-aml)",
				.cra_driver_name  = "aml-hmac-sha1",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_NEED_FALLBACK,
				.cra_blocksize	  = SHA1_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_hmac_sha1_cra_init,
				.cra_exit	  = aml_hmac_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.setkey         = aml_sha_setkey,
		.halg = {
			.digestsize	= SHA224_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "hmac(sha224-aml)",
				.cra_driver_name  = "aml-hmac-sha224",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_NEED_FALLBACK,
				.cra_blocksize	  = SHA224_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_hmac_sha224_cra_init,
				.cra_exit	  = aml_hmac_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.setkey         = aml_sha_setkey,
		.halg = {
			.digestsize	= SHA256_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "hmac(sha256-aml)",
				.cra_driver_name  = "aml-hmac-sha256",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_NEED_FALLBACK,
				.cra_blocksize	  = SHA256_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_hmac_sha256_cra_init,
				.cra_exit	  = aml_hmac_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.setkey         = aml_sha_kl_setkey,
		.halg = {
			.digestsize	= SHA1_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "hmac(sha1-kl)",
				.cra_driver_name  = "aml-hmac-sha1-kl",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_NEED_FALLBACK,
				.cra_blocksize	  = SHA1_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_hmac_sha1_cra_init,
				.cra_exit	  = aml_hmac_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.setkey         = aml_sha_kl_setkey,
		.halg = {
			.digestsize	= SHA224_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "hmac(sha224-kl)",
				.cra_driver_name  = "aml-hmac-sha224-kl",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_NEED_FALLBACK,
				.cra_blocksize	  = SHA224_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_hmac_sha224_cra_init,
				.cra_exit	  = aml_hmac_cra_exit,
			}
		}
	},
	{
		.init		= aml_sha_init,
		.update		= aml_sha_update,
		.final		= aml_sha_final,
		.finup		= aml_sha_finup,
		.digest		= aml_sha_digest,
		.export     = aml_sha_export,
		.import     = aml_sha_import,
		.setkey         = aml_sha_kl_setkey,
		.halg = {
			.digestsize	= SHA256_DIGEST_SIZE,
			.statesize =
				sizeof(struct aml_sha_reqctx) + SHA_BUFFER_LEN,
			.base	= {
				.cra_name	  = "hmac(sha256-kl)",
				.cra_driver_name  = "aml-hmac-sha256-kl",
				.cra_priority	  = 100,
				.cra_flags	  = CRYPTO_ALG_ASYNC |
					CRYPTO_ALG_NEED_FALLBACK,
				.cra_blocksize	  = SHA256_BLOCK_SIZE,
				.cra_ctxsize	  = sizeof(struct aml_sha_ctx),
				.cra_alignmask	  = 0,
				.cra_module	  = THIS_MODULE,
				.cra_init	  = aml_hmac_sha256_cra_init,
				.cra_exit	  = aml_hmac_cra_exit,
			}
		}
	}
};

static void aml_sha_unregister_algs(struct aml_sha_dev *dd)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sha_algs); i++) {
		if (!dd->support_sha1_zero_len &&
		    !strcmp(sha_algs[i].halg.base.cra_name, "hmac(sha1-kl)"))
			continue;
		crypto_unregister_ahash(&sha_algs[i]);
	}
}

static int aml_sha_register_algs(struct aml_sha_dev *dd)
{
	int err, i, j;

	for (i = 0; i < ARRAY_SIZE(sha_algs); i++) {
		if (!dd->support_sha1_zero_len &&
		    !strcmp(sha_algs[i].halg.base.cra_name, "hmac(sha1-kl)"))
			continue;
		err = crypto_register_ahash(&sha_algs[i]);
		if (err)
			goto err_sha_algs;
	}

	return 0;

err_sha_algs:
	for (j = 0; j < i; j++)
		crypto_unregister_ahash(&sha_algs[j]);
	return err;
}

static int aml_sha_probe(struct platform_device *pdev)
{
	struct aml_sha_dev *sha_dd;
	struct device *dev = &pdev->dev;
	int err = -EPERM;
	/* default is 48 for most chips.
	 * For S7D, it is 40 and specified in dts
	 */
	u32 hw_ctx_sz = SHA256_DIGEST_SIZE + 16;

	/* Support sha1 with 0 length for most chips.
	 * For those without this support, it should be specified in dts
	 */
	u32 support_sha1_zero_len = 1;

	sha_dd = devm_kzalloc(dev, sizeof(struct aml_sha_dev), GFP_KERNEL);
	if (!sha_dd) {
		err = -ENOMEM;
		goto sha_dd_err;
	}

	if (of_find_property(pdev->dev.of_node,
		"not_support_sha1_zero_len", NULL))
		support_sha1_zero_len = 0;

	of_property_read_u32(pdev->dev.of_node, "hw_ctx_sz", &hw_ctx_sz);

	sha_dd->dev = dev;
	sha_dd->parent = dev->parent;
	sha_dd->dma = dev_get_drvdata(dev->parent);
	sha_dd->thread = sha_dd->dma->thread;
	sha_dd->status = sha_dd->dma->status;
	sha_dd->hw_ctx_sz = hw_ctx_sz;
	sha_dd->support_sha1_zero_len = support_sha1_zero_len;
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

	aml_sha_hw_init(sha_dd);

	spin_lock(&aml_sha.lock);
	list_add_tail(&sha_dd->list, &aml_sha.dev_list);
	spin_unlock(&aml_sha.lock);

	err = aml_sha_register_algs(sha_dd);
	if (err)
		goto err_algs;

	dev_dbg(dev, "Aml SHA1/SHA224/SHA256 dma\n");

	pm_runtime_put_sync_autosuspend(dev);
	return 0;

err_algs:
	spin_lock(&aml_sha.lock);
	list_del(&sha_dd->list);
	spin_unlock(&aml_sha.lock);
err_pm:
	pm_runtime_disable(dev);
sha_dd_err:
	dev_err(dev, "initialization failed.\n");

	return err;
}

static void aml_sha_remove(struct platform_device *pdev)
{
	static struct aml_sha_dev *sha_dd;
	size_t dsc_sz = 0;

	sha_dd = platform_get_drvdata(pdev);
	if (!sha_dd)
		return;

	spin_lock(&aml_sha.lock);
	list_del(&sha_dd->list);
	spin_unlock(&aml_sha.lock);

	dsc_sz = aml_dma_get_dsc_sz(0, sha_dd->dma);
	dmam_free_coherent(sha_dd->parent, MAX_NUM_TABLES * dsc_sz,
			   sha_dd->descriptor, sha_dd->dma_descript_tab);

	aml_sha_unregister_algs(sha_dd);
	pm_runtime_disable(sha_dd->parent);
}

#ifdef CONFIG_OF
static const struct of_device_id aml_sha_dt_match[] = {
	{	.compatible = "amlogic,sha_dma",
	},
	{},
};
#else
#define aml_sha_dt_match NULL
#endif
static struct platform_driver aml_sha_driver = {
	.probe		= aml_sha_probe,
	.remove		= aml_sha_remove,
	.driver		= {
		.name	= "aml_sha_dma",
		.owner	= THIS_MODULE,
		.of_match_table	= aml_sha_dt_match,
	},
};

int __init aml_sha_driver_init(void)
{
	return platform_driver_register(&aml_sha_driver);
}

void aml_sha_driver_exit(void)
{
	platform_driver_unregister(&aml_sha_driver);
}
