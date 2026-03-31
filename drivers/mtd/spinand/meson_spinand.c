// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/div64.h>
#include <linux/sizes.h>
#include <linux/of.h>
#include <linux/version.h>
#include <linux/spi/spi-mem.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/spinand.h>
#include <linux/pinctrl/consumer.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/aml_spi_nand.h>
#include <linux/amlogic/aml_storage.h>
#include <linux/amlogic/aml_spi_mem.h>
#include <linux/amlogic/nand_encryption.h>
#include <linux/amlogic/aml_pageinfo.h>

#define NAND_BLOCK_GOOD	0
#define NAND_BLOCK_BAD	1
#define NAND_FACTORY_BAD	2

//#define CONFIG_NOT_SKIP_BAD_BLOCK

struct meson_spinand {
	struct mtd_info *mtd;
	struct spinand_device *spinand;
	struct meson_rsv_handler_t *rsv;
	s8 *block_status;
	unsigned int erasesize_shift;
};

struct meson_spinand *meson_spinand_global;

int meson_spinand_rsv_erase(struct mtd_info *mtd, struct erase_info *einfo)
{
	int ret;

	ret = nanddev_mtd_erase(mtd, einfo);

	return ret;
}

int meson_spinand_rsv_write_oob(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops)
{
	return spinand_mtd_write_unlock(mtd, to, ops);
}

int meson_spinand_rsv_read_oob(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops)
{
	return spinand_mtd_read_unlock(mtd, from, ops);
}

int meson_spinand_rsv_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	int eraseblock;
	u8 status;

	eraseblock = (int)(offs >> (ffs(mtd->erasesize) - 1));
	status = meson_spinand->block_status[eraseblock];

	return status ? 1 : 0;
}

int meson_spinand_rsv_get_device(struct mtd_info *mtd)
{
	struct spinand_device *spinand = mtd_to_spinand(mtd);

	mutex_lock(&spinand->lock);

	return 0;
}

void meson_spinand_rsv_release_device(struct mtd_info *mtd)
{
	struct spinand_device *spinand = mtd_to_spinand(mtd);

	mutex_unlock(&spinand->lock);
}

void spinand_get_tpl_info(u32 *fip_size, u32 *fip_copies)
{
	*fip_size = meson_nand_get_fipsize();
	*fip_copies = meson_nand_get_fipcopies();
}
EXPORT_SYMBOL_GPL(spinand_get_tpl_info);

bool meson_spinand_isbad(struct nand_device *nand, const struct nand_pos *pos)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	u8 block_status;

	BUG_ON(!meson_spinand->block_status);

	block_status = meson_spinand->block_status[pos->eraseblock];
	if (block_status == NAND_BLOCK_BAD) {
		pr_err("NAND bbt detect Bad block at %llx\n",
				(u64)nanddev_pos_to_offs(nand, (const struct nand_pos *)pos));
		return true;
	}
	if (block_status == NAND_FACTORY_BAD) {
		pr_err("NAND bbt detect factory Bad block at %llx\n",
				(u64)nanddev_pos_to_offs(nand, (const struct nand_pos *)pos));
		return true;
	}

	return false;
}
EXPORT_SYMBOL_GPL(meson_spinand_isbad);

static int spinand_mtd_block_isbad(struct mtd_info *mtd, loff_t offs)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	struct spinand_device *spinand = meson_spinand->spinand;
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct nand_pos pos;
	int block_status;

	mutex_lock(&spinand->lock);
	nanddev_offs_to_pos(nand, offs, &pos);
	block_status = meson_spinand_isbad(nand, &pos);
	mutex_unlock(&spinand->lock);

	return block_status;
}

static int spinand_mtd_block_markbad(struct mtd_info *mtd, loff_t offs)
{
	struct meson_spinand *meson_spinand = meson_spinand_global;
	struct spinand_device *spinand = meson_spinand->spinand;
	struct nand_device *nand = mtd_to_nanddev(mtd);
	struct nand_pos pos;
	u8 bad_block;
	s8 *buf = NULL;

	nanddev_offs_to_pos(nand, offs, &pos);
	mutex_lock(&spinand->lock);
	if (meson_spinand->block_status) {
		/* TODO: Keep one plane */
		bad_block = meson_spinand->block_status[pos.eraseblock];
		if (bad_block != NAND_BLOCK_BAD &&
		    bad_block != NAND_FACTORY_BAD &&
		    bad_block != NAND_BLOCK_GOOD) {
			pr_err("bad block table is mixed\n");
			mutex_unlock(&spinand->lock);
			return -EINVAL;
		}
		if (bad_block == NAND_BLOCK_GOOD) {
			buf = meson_spinand->block_status;
			buf[pos.eraseblock] = NAND_BLOCK_BAD;
			meson_rsv_bbt_write((u_char *)buf, meson_rsv_get_bbt_size());
		}
		mutex_unlock(&spinand->lock);
		return 0;
	}
	pr_info("bbt table is not initial");
	mutex_unlock(&spinand->lock);
	return -EINVAL;
}

static int meson_spinand_parse_dt(struct mtd_info *mtd)
{
	u32 bl_mode = NAND_FIPMODE_ADVANCE, fip_copies = 0, fip_size = 0;
	struct device_node *np = mtd_get_of_node(mtd);
	s32 page_info_adjust_bbtlen = -1;

	if (!np) {
		pr_err("%s %d: can't get np\n", __func__, __LINE__);
		return -ENODEV;
	}

	of_property_read_u32(np, "bl_mode", &bl_mode);
	of_property_read_u32(np, "fip_copies", &fip_copies);

	if (bl_mode == NAND_FIPMODE_DISCRETE) {
		if (of_property_read_u32(np, "fip_size", &fip_size)) {
			pr_err("%s %d: no fip size in dts\n", __func__, __LINE__);
			return -EINVAL;
		}
	}

	if (of_property_read_s32(np, "page_info_adjust_bbtlen",
	    &page_info_adjust_bbtlen) == 0)
		meson_nand_set_page_info_bbtlen(page_info_adjust_bbtlen);

	meson_nand_set_bootloader_mode(bl_mode);
	meson_nand_set_fipsize(fip_size);
	meson_nand_set_fipcopies(fip_copies);
#ifndef CONFIG_NOT_SKIP_BAD_BLOCK
	meson_nand_set_skip_bad_block(1);
#else
	meson_nand_set_skip_bad_block(0);
#endif

	return 0;
}

int meson_spinand_init(struct spinand_device *spinand, struct mtd_info *mtd)
{
	struct meson_spinand *meson_spinand = NULL;
	int err = 0;

	err = meson_spinand_parse_dt(mtd);
	if (err)
		return err;

	meson_spinand = kzalloc(sizeof(*meson_spinand), GFP_KERNEL);
	if (!meson_spinand)
		return -ENOMEM;

	spi_mem_set_mtd(mtd);
	meson_spinand_global = meson_spinand;
	meson_spinand->erasesize_shift = ffs(mtd->erasesize) - 1;
	meson_spinand->block_status =
		kzalloc((mtd->size >> meson_spinand->erasesize_shift),
			GFP_KERNEL);
	if (!meson_spinand->block_status) {
		err = -ENOMEM;
		goto exit_error2;
	}

	meson_spinand->mtd = mtd;
	meson_spinand->spinand = spinand;

	meson_spinand->rsv = kzalloc(sizeof(*meson_spinand->rsv),
				     GFP_KERNEL);
	if (!meson_spinand->rsv) {
		err = -ENOMEM;
		goto exit_error1;
	}

	mtd->name = AML_MTD_NAME;
	mtd->_block_isbad = spinand_mtd_block_isbad;
	mtd->_block_markbad = spinand_mtd_block_markbad;
	mtd->_block_isreserved = NULL;
	mtd->erasesize_shift = meson_spinand->erasesize_shift;
	mtd->writesize_shift = ffs(mtd->writesize) - 1;

	meson_spinand->rsv->mtd = mtd;
	meson_spinand->rsv->bbt_buf = meson_spinand->block_status;
	meson_spinand->rsv->rsv_ops._erase = meson_spinand_rsv_erase;
	meson_spinand->rsv->rsv_ops._write_oob = meson_spinand_rsv_write_oob;
	meson_spinand->rsv->rsv_ops._read_oob = meson_spinand_rsv_read_oob;
	meson_spinand->rsv->rsv_ops._block_markbad = NULL;
	meson_spinand->rsv->rsv_ops._block_isbad = meson_spinand_rsv_isbad;
	meson_spinand->rsv->rsv_ops._get_device = meson_spinand_rsv_get_device;
	meson_spinand->rsv->rsv_ops._release_device = meson_spinand_rsv_release_device;
	err = meson_rsv_init(mtd, meson_spinand->rsv);
	if (err) {
		pr_err("meson_rsv_init failed\n");
		goto exit_error;
	}

	return 0;
exit_error:
	kfree(meson_spinand->rsv);
exit_error1:
	kfree(meson_spinand->block_status);
exit_error2:
	kfree(meson_spinand);
	return err;
}
EXPORT_SYMBOL_GPL(meson_spinand_init);

MODULE_DESCRIPTION("MESON SPI NAND INTERFACE");
MODULE_AUTHOR("sunny luo<sunny.luo@amlogic.com>");
MODULE_LICENSE("GPL v2");
