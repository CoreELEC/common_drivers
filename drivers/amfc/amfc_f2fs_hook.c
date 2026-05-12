// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include <linux/zstd.h>
#include <linux/kprobes.h>
#include <linux/f2fs_fs.h>
#include <linux/amlogic/amfc.h>
#include "f2fs.h"

#ifdef CONFIG_ARM64
#define REG_0 regs->regs[0]
#define REG_LR regs->regs[30]
#endif

#ifdef CONFIG_ARM
#define REG_0 regs->ARM_r0
#define REG_LR regs->ARM_lr
#endif

#define F2FS_BOUNCE_SIZE		(16 * 1024)

static unsigned char *bounce_buffer[NR_CPUS] = {};

static int __nocfi __kprobes
zstd_init_compress_ctx_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct compress_ctx *cc = (struct compress_ctx *)REG_0;

	cc->clen = cc->rlen - PAGE_SIZE - COMPRESS_HEADER_SIZE;

	REG_0 = 0;
	instruction_pointer_set(regs, REG_LR);
	return 1;
}

static struct kprobe kp_zstd_init_compress_ctx = {
	.symbol_name = "zstd_init_compress_ctx",
	.pre_handler = zstd_init_compress_ctx_pre_handler,
};

static int __nocfi __kprobes
zstd_destroy_compress_ctx_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	instruction_pointer_set(regs, REG_LR);
	return 1;
}

static struct kprobe kp_zstd_destroy_compress_ctx = {
	.symbol_name = "zstd_destroy_compress_ctx",
	.pre_handler = zstd_destroy_compress_ctx_pre_handler,
};

static int __nocfi __kprobes
zstd_compress_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct compress_ctx *cc = (struct compress_ctx *)REG_0;
	int src_size = cc->rlen;
	int dst_size = src_size - PAGE_SIZE - COMPRESS_HEADER_SIZE;
	int ret;
	int ret1 = 0, i = 0, off = 0, cpu = smp_processor_id(), remain = src_size;
	unsigned long flags;
	int err;
	struct amfc *amfc = get_amfc_instance();

	/* compress 4K by 4K for S7D */
	local_irq_save(flags);
	while (src_size > 0) {
		ret = amfc_compress(cc->rbuf + i * PAGE_SIZE,
				    bounce_buffer[cpu] + off, PAGE_SIZE,
				    remain - off);
		if (ret < 0) {
			ret1 = ret;
			break;
		}
		off += ret;
		src_size -= PAGE_SIZE;
		i++;
		pr_debug("%s, src_size:%d, ret:%d, off:%d, remain:%d\n",
			__func__, src_size, ret, off, remain - off);
		if ((src_size == PAGE_SIZE) && (cc->rlen - off < PAGE_SIZE)) {
			ret1 = -AMFC_ENC_DST_SIZE_OVF;
			break;
		}
	}
	pr_debug("%s, src_size:%d, ret:%d %d, off:%d, remain:%d\n",
		__func__, src_size, ret, ret1, off, remain - off);
	if (ret1 < 0) {
		local_irq_restore(flags);
		if (ret1 == -AMFC_ENC_DST_SIZE_OVF) {
			err = -EAGAIN;
			goto error;
		}
		printk_ratelimited("%sF2FS-fs (%s): %s ZSTD_compressStream failed, ret: %d\n",
				KERN_ERR, F2FS_I_SB(cc->inode)->sb->s_id,
				__func__, ret);

		err = ret1;
		goto error;
	}
	if (off > dst_size) {
		local_irq_restore(flags);
		err = -EAGAIN;
		goto error;
	}
	pr_debug("%s, src_size:%d, ret:%d %d, off:%d, remain:%d\n",
		__func__, src_size, ret, ret1, off, remain - off);
	memcpy(cc->cbuf->cdata, bounce_buffer[cpu], off);
	local_irq_restore(flags);
	/* replace ZSTD tag to compressed size for store */
	*((unsigned int *)cc->cbuf) = off;
	cc->clen = off;
	amfc->f2fs_enc_in  += cc->rlen;
	amfc->f2fs_enc_out += PAGE_ALIGN(cc->clen);

	REG_0 = 0;
	instruction_pointer_set(regs, REG_LR);
	return 1;

error:
	amfc->f2fs_enc_fail++;
	REG_0 = (unsigned long)err;
	instruction_pointer_set(regs, REG_LR);
	return 1;
}

static struct kprobe kp_zstd_compress_pages = {
	.symbol_name = "zstd_compress_pages",
	.pre_handler = zstd_compress_pages_pre_handler,
};

static int __nocfi __kprobes
zstd_init_decompress_ctx_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	REG_0 = 0;
	instruction_pointer_set(regs, REG_LR);
	return 1;
}

static struct kprobe kp_zstd_init_decompress_ctx = {
	.symbol_name = "zstd_init_decompress_ctx",
	.pre_handler = zstd_init_decompress_ctx_pre_handler,
};

static int __nocfi __kprobes
zstd_destroy_decompress_ctx_pre_handler(struct kprobe *p, struct pt_regs *regs)
{

	instruction_pointer_set(regs, REG_LR);
	return 1;
}

static struct kprobe kp_zstd_destroy_decompress_ctx = {
	.symbol_name = "zstd_destroy_decompress_ctx",
	.pre_handler = zstd_destroy_decompress_ctx_pre_handler,
};

static int __nocfi __kprobes
zstd_decompress_pages_pre_handler(struct kprobe *p, struct pt_regs *regs)
{
	struct decompress_io_ctx *dic = (struct decompress_io_ctx *)REG_0;
	int ret;
	struct compress_data c;
	struct amfc *amfc = get_amfc_instance();

	/* back up */
	memcpy(&c, dic->cbuf, sizeof(c));
	memset(dic->cbuf, 0, sizeof(c));
	ret = amfc_decompress(dic->cbuf, dic->rbuf,
			      dic->clen + sizeof(c), dic->rlen, 0);
	pr_debug("%s, src:%px, dst:%px, ssize:%zu, dsize:%zu, ret:%d\n",
		__func__, dic->cbuf, dic->rbuf, dic->clen, dic->rlen, ret);
	// restore
	memcpy(dic->cbuf, &c, sizeof(c));
	if (ret != dic->rlen || ret < 0) {
		printk_ratelimited("%s ZSTD invalid rlen:%zu, expected:%lu, ret:%d",
				__func__, dic->rlen,
				PAGE_SIZE << dic->log_cluster_size, ret);

		REG_0 = (unsigned long)(-EIO);
		amfc->f2fs_dec_fail++;
		instruction_pointer_set(regs, REG_LR);

		return 1;
	}
	amfc->f2fs_dec_in  += PAGE_ALIGN(dic->clen);
	amfc->f2fs_dec_out += dic->rlen;
	REG_0 = (unsigned long)0;
	instruction_pointer_set(regs, REG_LR);

	return 1;
}

static struct kprobe kp_zstd_decompress_pages = {
	.symbol_name = "zstd_decompress_pages",
	.pre_handler = zstd_decompress_pages_pre_handler,
};

int hook_f2fs(void)
{
	int fail = 0, ret = 0;
	int cpu;
	int size;

	/* padding 64 bytes to avoid hardware overwrite */
	size = num_possible_cpus() * (F2FS_BOUNCE_SIZE + 64);
	if (!bounce_buffer[0]) {
		void *p = NULL;

		p = kmalloc(size, GFP_KERNEL);
		if (!p)
			return -ENOMEM;
		for (cpu = 0; cpu < num_possible_cpus(); cpu++) {
			bounce_buffer[cpu] = p;
			p += (F2FS_BOUNCE_SIZE + 64);
			pr_debug("%s, alloc %px for cpu:%d\n",
				__func__, bounce_buffer[cpu], cpu);
		}
	}

	ret = register_kprobe(&kp_zstd_init_compress_ctx);
	if (ret)
		fail |= (1 << 0);
	ret = register_kprobe(&kp_zstd_destroy_compress_ctx);
	if (ret)
		fail |= (1 << 1);
	ret = register_kprobe(&kp_zstd_compress_pages);
	if (ret)
		fail |= (1 << 2);
	ret = register_kprobe(&kp_zstd_init_decompress_ctx);
	if (ret)
		fail |= (1 << 3);
	ret = register_kprobe(&kp_zstd_destroy_decompress_ctx);
	if (ret)
		fail |= (1 << 4);
	ret = register_kprobe(&kp_zstd_decompress_pages);
	if (ret)
		fail |= (1 << 5);

	if (fail) {
		pr_err("%s, hook function failed:%x\n", __func__, fail);
		return -ENODEV;
	}
	return ret;
}
