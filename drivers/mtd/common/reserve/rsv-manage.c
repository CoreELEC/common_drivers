// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kmod.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/partitions.h>
//#include <linux/mtd/nand_ecc.h>
#include <linux/amlogic/aml_rsv.h>
#include <linux/amlogic/aml_mtd_nand.h>
#include <linux/amlogic/aml_pageinfo.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

char *aml_nand_get_rsv_cmdline(void);
static u32 rsv_block_cnt = DEFAULT_NAND_RSV_BLOCK_NUM;

struct meson_rsv_handler_t *rsv_handler;

/* protect flag inside */
static int rsv_protect = 1;
static struct meson_rsv_part_t rsv_parts[] = {
	{"ngap", DEFAULT_NAND_GAP_BLOCK_NUM},
	{"nbbt", DEFAULT_NAND_BBT_BLOCK_NUM},
	{"nenv", DEFAULT_NAND_ENV_BLOCK_NUM},
	{"nkey", DEFAULT_NAND_KEY_BLOCK_NUM},
	{"ndtb", DEFAULT_NAND_DTB_BLOCK_NUM},
	{"nddr", DEFAULT_NAND_DDR_BLOCK_NUM},
};

u32 meson_rsv_get_block_cnt(void)
{
	return rsv_block_cnt;
}
EXPORT_SYMBOL(meson_rsv_get_block_cnt);

int meson_rsv_name2index(const char *name)
{
	struct meson_rsv_info_t *rsv_info = rsv_handler->rsv_info;
	int i;

	for (i = 0; i < rsv_handler->entries; i++)
		if (!strcmp(name, rsv_info[i].name))
			return i;
	return -1;
}
EXPORT_SYMBOL(meson_rsv_name2index);

static inline void _aml_rsv_disprotect(void)
{
	rsv_protect = 0;
}

static inline void _aml_rsv_protect(void)
{
	rsv_protect = 1;
}

static inline int _aml_rsv_isprotect(void)
{
	return rsv_protect;
}

static struct free_node_t *get_free_node(struct meson_rsv_info_t *rsv_info)
{
	struct meson_rsv_handler_t *handler = rsv_info->handler;
	unsigned long long freenodebitmask = handler->freenodebitmask;
	static unsigned int rsv_block_num;
	unsigned long index;

	rsv_block_num = meson_rsv_get_block_cnt();
	index = find_first_zero_bit((void *)&handler->freenodebitmask, rsv_block_num);
	if (index >= rsv_block_num) {
		pr_err("%s (%s) error: index %lu is greater than max %u\n",
			__func__, rsv_info->name, index, rsv_block_num);
		return NULL;
	}
	WARN_ON(test_and_set_bit(index, (void *)&handler->freenodebitmask));

	pr_debug("%s (%s): pre bitmap:%llx, now bitmap:%llx\n", __func__,
		rsv_info->name, freenodebitmask, handler->freenodebitmask);

	return &handler->free_node[index];
}

static void release_free_node(struct meson_rsv_info_t *rsv_info,
			      struct free_node_t *free_node)
{
	struct meson_rsv_handler_t *handler = rsv_info->handler;
	unsigned int index_save = free_node->index;
	unsigned long long freenodebitmask = handler->freenodebitmask;
	static unsigned int rsv_block_num;

	rsv_block_num = meson_rsv_get_block_cnt();
	if (index_save > rsv_block_num) {
		pr_info("%s (%s) error: index %u is greater than max %u! error",
			__func__, rsv_info->name, index_save, rsv_block_num);
		return;
	}

	WARN_ON(!test_and_clear_bit(index_save,
				    (void *)&handler->freenodebitmask));

	/*memset zero to protect from dead-loop*/
	memset(free_node, 0, sizeof(struct free_node_t));
	free_node->index = index_save;
	pr_debug("%s (%s): pre bitmap:%llx, now bitmap:%llx\n", __func__,
		rsv_info->name, freenodebitmask, handler->freenodebitmask);
}

static void add_free_node(struct meson_rsv_info_t *rsv_info,
	struct free_node_t *free_node)
{
	struct free_node_t *temp_node;

	if (!rsv_info->free_node) {
		rsv_info->free_node = free_node;
	} else {
		temp_node = rsv_info->free_node;
		while (temp_node->next)
			temp_node = temp_node->next;

		temp_node->next = free_node;
	}
}

static int meson_free_rsv_info(struct meson_rsv_info_t *rsv_info)
{
	struct mtd_info *mtd;
	struct erase_info erase_info;
	struct free_node_t *tmp_node, *next_node = NULL;
	struct meson_rsv_ops *rsv_ops = &rsv_handler->rsv_ops;
	int error = 0;
	loff_t addr = 0;

	mtd = rsv_info->mtd;
	pr_info("free %s:\n", rsv_info->name);

	if (rsv_info->valid) {
		addr = rsv_info->valid_node->phy_blk_addr;
		addr *= mtd->erasesize;
		memset(&erase_info,
		       0, sizeof(struct erase_info));
		erase_info.addr = addr;
		erase_info.len = mtd->erasesize;
		_aml_rsv_disprotect();
		error = rsv_ops->_erase(mtd, &erase_info); //nand_erase
		_aml_rsv_protect();
		pr_info("erasing valid info block: %llx\n", addr);
		rsv_info->valid_node->phy_blk_addr = -1;
		rsv_info->valid_node->ec = -1;
		rsv_info->valid_node->phy_page_addr = 0;
		rsv_info->valid_node->timestamp = 0;
		rsv_info->valid_node->status = 0;
		rsv_info->valid = 0;
	}
	tmp_node = rsv_info->free_node;
	while (tmp_node) {
		next_node = tmp_node->next;
		release_free_node(rsv_info, tmp_node);
		tmp_node = next_node;
	}
	rsv_info->free_node = NULL;

	return error;
}

static int meson_rsv_write(struct meson_rsv_info_t *rsv_info, u_char *buf)
{
	struct mtd_info *mtd;
	struct oobinfo_t oobinfo;
	struct mtd_oob_ops oob_ops;
	struct meson_rsv_ops *rsv_ops = &rsv_handler->rsv_ops;
	size_t length = 0;
	loff_t offset;
	int ret = 0;

	mtd = rsv_info->mtd;
	offset = rsv_info->valid_node->phy_blk_addr;
	offset *= mtd->erasesize;
	offset += rsv_info->valid_node->phy_page_addr * (loff_t)mtd->writesize;
	pr_info("%s:%d,save %s info to %llx\n", __func__, __LINE__,
		rsv_info->name, offset);

	memcpy(oobinfo.name, rsv_info->name, 4);
	oobinfo.ec = rsv_info->valid_node->ec;
	oobinfo.timestamp = rsv_info->valid_node->timestamp;
	while (length < rsv_info->size) {
		oob_ops.mode = MTD_OPS_AUTO_OOB;
		oob_ops.len = min_t(u32, mtd->writesize,
				    (rsv_info->size - length));
		oob_ops.ooblen = sizeof(struct oobinfo_t);
		oob_ops.ooboffs = 0;
		oob_ops.datbuf = buf + length;
		oob_ops.oobbuf = (u8 *)&oobinfo;

		ret = rsv_ops->_write_oob(mtd, offset, &oob_ops); //nand_write_oob
		if (ret) {
			pr_info("blk check good but write failed: %llx, %d\n",
				(u64)offset, ret);
			return 1;
		}
		offset += mtd->writesize;
		length += oob_ops.len;
	}
	return ret;
}

static int meson_rsv_save(struct meson_rsv_info_t *rsv_info, u_char *buf)
{
	struct mtd_info *mtd;
	struct free_node_t *free_node, *temp_node;
	struct erase_info erase_info;
	struct meson_rsv_ops *rsv_ops = &rsv_handler->rsv_ops;
	int ret = 0, i = 1, pages_per_blk;
	loff_t offset = 0;

	mtd = rsv_info->mtd;
	pages_per_blk = mtd->erasesize / mtd->writesize;
	/*solve power off and ecc error*/
	if (rsv_info->valid_node->status & POWER_ABNORMAL_FLAG ||
	    rsv_info->valid_node->status & ECC_ABNORMAL_FLAG)
		rsv_info->valid_node->phy_page_addr = pages_per_blk;

	if (mtd->writesize < rsv_info->size)
		i = (rsv_info->size + mtd->writesize - 1) / mtd->writesize;
	pr_info("%s %d: %s valid=%d, pages=%d\n", __func__, __LINE__,
		rsv_info->name, rsv_info->valid, i);
RE_SEARCH:
	if (rsv_info->valid) {
		rsv_info->valid_node->phy_page_addr += i;

		if ((rsv_info->valid_node->phy_page_addr + i) > pages_per_blk) {
			if ((rsv_info->valid_node->phy_page_addr - i) ==
				 pages_per_blk) {
				offset = rsv_info->valid_node->phy_blk_addr;
				offset *= mtd->erasesize;
				memset(&erase_info,
				       0, sizeof(struct erase_info));

				erase_info.addr = offset;
				erase_info.len = mtd->erasesize;
				_aml_rsv_disprotect();
				ret = rsv_ops->_erase(mtd, &erase_info);
				_aml_rsv_protect();
				if (ret) {
					pr_info("rsv free blk erase failed %d\n",
						ret);
					mtd->_block_markbad(mtd, offset);
				}
				rsv_info->valid_node->ec++;
				pr_info("---erase bad rsv block:%llx\n",
					offset);
			}
			/* free_node = kzalloc(sizeof(struct free_node_t), */
			/* GFP_KERNEL); */
			free_node = get_free_node(rsv_info);
			if (!free_node)
				return -ENOMEM;

			free_node->phy_blk_addr =
				rsv_info->valid_node->phy_blk_addr;
			free_node->ec = rsv_info->valid_node->ec;
			add_free_node(rsv_info, free_node);

			temp_node = rsv_info->free_node;
			rsv_info->valid_node->phy_blk_addr =
				temp_node->phy_blk_addr;
			rsv_info->valid_node->phy_page_addr = 0;
			rsv_info->valid_node->ec = temp_node->ec;
			rsv_info->valid_node->timestamp += 1;
			rsv_info->free_node = temp_node->next;
			release_free_node(rsv_info, temp_node);
		}
	} else {
		temp_node = rsv_info->free_node;
		rsv_info->valid_node->phy_blk_addr = temp_node->phy_blk_addr;
		rsv_info->valid_node->phy_page_addr = 0;
		rsv_info->valid_node->ec = temp_node->ec;
		rsv_info->valid_node->timestamp += 1;
		rsv_info->free_node = temp_node->next;
		release_free_node(rsv_info, temp_node);
	}

	offset = rsv_info->valid_node->phy_blk_addr;
	offset *= mtd->erasesize;
	offset += rsv_info->valid_node->phy_page_addr * (loff_t)mtd->writesize;

	if (rsv_info->valid_node->phy_page_addr == 0) {
		ret = rsv_ops->_block_isbad(mtd, offset);
		if (ret) {
			/*
			 *bad block here, need fix it
			 *because of info_blk list may be include bad block,
			 *so we need check it and done here. if don't,
			 *some bad blocks may be erase here
			 *and env will lost or too much ecc error
			 **/
			pr_info("have bad block in info_blk list!!!!\n");
			rsv_info->valid_node->phy_page_addr =
				pages_per_blk - i;
			goto RE_SEARCH;
		}

		memset(&erase_info, 0, sizeof(struct erase_info));
		erase_info.addr = offset;
		erase_info.len = mtd->erasesize;
		_aml_rsv_disprotect();
		ret = rsv_ops->_erase(mtd, &erase_info);
		_aml_rsv_protect();
		if (ret) {
			pr_info("env free blk erase failed %d\n", ret);
			mtd->_block_markbad(mtd, offset);
			return ret;
		}
		rsv_info->valid_node->ec++;
	}
	ret = meson_rsv_write(rsv_info, buf);
	if (ret) {
		pr_info("update nand rsv FAILED!\n");
		return 1;
	}
	rsv_info->valid = 1;
	/* clear status when write successfully*/
	rsv_info->valid_node->status = 0;
	return ret;
}

static inline u32 skip_bbt_blocks(struct meson_rsv_info_t *rsv_info, u32 start)
{
	if (memcmp(BBT_NAND_MAGIC, rsv_info->name, 4))
		if (start >= BBT_START_BLOCK &&
		    start < BBT_START_BLOCK + BBT_TOTAL_BLOCKS)
			return start + BBT_TOTAL_BLOCKS;
	return start;
}

static void meson_print_rsv_block_info(struct meson_rsv_info_t *rsv_info)
{
	char dbg_info_buf[256];
	int n = 0;
	struct free_node_t *temp_node;

	if (rsv_info->valid == 0)
		return;

	pr_info("%s valid block info: BN:%d EC:%d TS:%d\n",
			rsv_info->name,
			rsv_info->valid_node->phy_blk_addr,
			rsv_info->valid_node->ec,
			rsv_info->valid_node->timestamp);
	n += sprintf(dbg_info_buf + n, "%s free list: ", rsv_info->name);
	temp_node = rsv_info->free_node;
	while (temp_node) {
		if (unlikely(n > (sizeof(dbg_info_buf) - 48))) {
			dbg_info_buf[n - 1] = '\0';
			pr_info("%s ", dbg_info_buf);
			n = 0;
		}
		n += sprintf(dbg_info_buf + n, "BN:%d EC:%d DF:%d; ",
				temp_node->phy_blk_addr,
				temp_node->ec,
				temp_node->dirty_flag);
		temp_node = temp_node->next;
	}
	dbg_info_buf[n - 1] = '\0';
	pr_info("%s\n", dbg_info_buf);
}

static int meson_rsv_scan(struct meson_rsv_info_t *rsv_info)
{
	struct mtd_info *mtd;
	struct mtd_oob_ops oob_ops;
	struct oobinfo_t oobinfo;
	struct free_node_t *free_node;
	struct meson_rsv_ops *rsv_ops = &rsv_handler->rsv_ops;
	u64 offset;
	u32 start, end;
	int ret = -1, error, rsv_status, i, k;

	u8 scan_status;
	u8 good_addr[256] = {0};
	u32 page_num, pages_per_blk;
	u32  remainder;

	mtd = rsv_info->mtd;
RE_RSV_INFO_EXT:
	start = rsv_info->start_block;
	end = rsv_info->end_block;
	pr_debug("%s: info size=0x%x max_scan_blk=%d, start_blk=%d\n",
		rsv_info->name, rsv_info->size,
		end, start);

	do {
		offset = skip_bbt_blocks(rsv_info, start);
		offset *= mtd->erasesize;
		scan_status = 0;
RE_RSV_INFO:
		oob_ops.mode = MTD_OPS_AUTO_OOB;
		oob_ops.len = 0;
		oob_ops.ooblen = sizeof(struct oobinfo_t);
		oob_ops.ooboffs = 0;
		oob_ops.datbuf = NULL;
		oob_ops.oobbuf = (u8 *)&oobinfo;

		memset((u8 *)&oobinfo, 0, sizeof(struct oobinfo_t));

		error = rsv_ops->_read_oob(mtd, offset, &oob_ops);
		if (error != 0 && error != -EUCLEAN) {
			pr_err("blk check good but read failed: %llx, %d\n",
					(u64)offset, error);
			offset += rsv_info->size;
			div_u64_rem(offset, mtd->erasesize, &remainder);
			if (scan_status++ > 6 || !remainder) {
				pr_err("ECC error, scan ONE block exit\n");
				scan_status = 0;
				continue;
			}
			goto RE_RSV_INFO;
		}

		rsv_info->init = 1;
		rsv_info->valid_node->status = 0;
		if (!memcmp(oobinfo.name, rsv_info->name, 4)) {
			rsv_info->valid = 1;
			if (rsv_info->valid_node->phy_blk_addr == -1) {
				rsv_info->valid_node->phy_blk_addr =
					skip_bbt_blocks(rsv_info, start);
				rsv_info->valid_node->phy_page_addr = 0;
				rsv_info->valid_node->ec = oobinfo.ec;
				rsv_info->valid_node->timestamp =
					oobinfo.timestamp;
				continue;
			}

			free_node = get_free_node(rsv_info);
			if (!free_node)
				return -ENOMEM;

			free_node->dirty_flag = 1;
			if (oobinfo.timestamp > rsv_info->valid_node->timestamp) {
				free_node->phy_blk_addr =
					rsv_info->valid_node->phy_blk_addr;
				free_node->ec =
					rsv_info->valid_node->ec;
				rsv_info->valid_node->phy_blk_addr =
					skip_bbt_blocks(rsv_info, start);
				rsv_info->valid_node->phy_page_addr = 0;
				rsv_info->valid_node->ec = oobinfo.ec;
				rsv_info->valid_node->timestamp =
					oobinfo.timestamp;
			} else {
				free_node->phy_blk_addr = skip_bbt_blocks(rsv_info, start);
				free_node->ec = oobinfo.ec;
			}
		} else {
			free_node = get_free_node(rsv_info);
			if (!free_node)
				return -ENOMEM;
			free_node->phy_blk_addr = skip_bbt_blocks(rsv_info, start);
			free_node->ec = oobinfo.ec;
		}

		add_free_node(rsv_info, free_node);

	} while ((++start) < end);

	meson_print_rsv_block_info(rsv_info);

	if (rsv_info->valid != 1)
		goto out;

	/*second stage*/
	pages_per_blk = 1 << (mtd->erasesize_shift - mtd->writesize_shift);
	page_num = rsv_info->size >> mtd->writesize_shift;
	if (page_num == 0)
		page_num++;

	pr_debug("%s %d: %s size need page_nums:%d\n",
			__func__, __LINE__, rsv_info->name, page_num);

	oob_ops.mode = MTD_OPS_AUTO_OOB;
	oob_ops.len = 0;
	oob_ops.ooblen = sizeof(struct oobinfo_t);
	oob_ops.ooboffs = 0;
	oob_ops.datbuf = NULL;
	oob_ops.oobbuf = (u8 *)&oobinfo;

	for (i = 0; i < pages_per_blk; i++) {
		memset((u8 *)&oobinfo, 0, oob_ops.ooblen);

		offset = rsv_info->valid_node->phy_blk_addr;
		offset *= mtd->erasesize;
		offset += i * (u64)mtd->writesize;
		error = rsv_ops->_read_oob(mtd, offset, &oob_ops);
		if (error != 0 && error != -EUCLEAN) {
			pr_err("blk good but read failed:%llx,%d\n",
					(u64)offset, error);
			rsv_info->valid_node->status |= ECC_ABNORMAL_FLAG;
			continue;
		}

		if (!memcmp(oobinfo.name, rsv_info->name, 4)) {
			good_addr[i] = 1;
			rsv_info->valid_node->phy_page_addr = i;
			ret = 0;
		} else {
			break;
		}
	}

	if (mtd->writesize < rsv_info->size) {
		i = rsv_info->valid_node->phy_page_addr;
		if (((i + 1) % page_num) != 0) {
			ret = -1;
			rsv_info->valid_node->status |= POWER_ABNORMAL_FLAG;
			pr_err("find %s incomplete\n", rsv_info->name);
		}

		if (ret == -1) {
			for (i = 0; i < (pages_per_blk / page_num); i++) {
				rsv_status = 0;
				for (k = 0; k < page_num; k++) {
					if (!good_addr[k + i * page_num]) {
						rsv_status = 1;
						break;
					}
				}
				if (!rsv_status) {
					pr_err("find %d page ok\n",
							i * page_num);
					rsv_info->valid_node->phy_page_addr =
						k + i * page_num - 1;
					ret = 0;
				}
			}
		}

		if (ret == -1) {
			rsv_info->valid_node->status = 0;
			meson_free_rsv_info(rsv_info);
			goto RE_RSV_INFO_EXT;
		}

		i = (rsv_info->size + mtd->writesize - 1) / mtd->writesize;
		rsv_info->valid_node->phy_page_addr -= (i - 1);
	}

	offset = rsv_info->valid_node->phy_blk_addr;
	offset *= mtd->erasesize;
	offset += rsv_info->valid_node->phy_page_addr * (u64)mtd->writesize;
	pr_err("%s valid addr: %llx\n", rsv_info->name, (u64)offset);
out:
	return ret;
}

static int meson_rsv_read(struct meson_rsv_info_t *rsv_info, u_char *buf)
{
	struct mtd_info *mtd;
	struct oobinfo_t oobinfo;
	struct mtd_oob_ops oob_ops;
	struct meson_rsv_ops *rsv_ops = &rsv_handler->rsv_ops;
	size_t length = 0;
	loff_t offset;
	int ret = 0;

	mtd = rsv_info->mtd;
READ_RSV_AGAIN:
	offset = rsv_info->valid_node->phy_blk_addr;
	offset *= mtd->erasesize;
	offset += rsv_info->valid_node->phy_page_addr * (loff_t)mtd->writesize;
	pr_info("%s:%d,read info %s from %llx\n", __func__, __LINE__,
		 rsv_info->name, offset);

	while (length < rsv_info->size) {
		oob_ops.mode = MTD_OPS_AUTO_OOB;
		oob_ops.len = min_t(u32, mtd->writesize,
				    (rsv_info->size - length));
		oob_ops.ooblen = sizeof(struct oobinfo_t);
		oob_ops.ooboffs = 0;
		oob_ops.datbuf = buf + length;
		oob_ops.oobbuf = (u8 *)&oobinfo;

		memset((u8 *)&oobinfo, 0, oob_ops.ooblen);

		ret = rsv_ops->_read_oob(mtd, offset, &oob_ops);
		if (ret != 0 && ret != -EUCLEAN) {
			pr_info("blk good but read failed: %llx, %d\n",
				(u64)offset, ret);
			ret = meson_rsv_scan(rsv_info);
			if (ret == -1)
				return 1;
			goto READ_RSV_AGAIN;
		}

		if (memcmp(oobinfo.name, rsv_info->name, 4))
			pr_info("invalid nand info %s magic: %llx\n",
				rsv_info->name, (u64)offset);

		offset += mtd->writesize;
		length += oob_ops.len;
	}
	return ret;
}

static int meson_rsv_check(struct meson_rsv_info_t *rsv_info)
{
	int ret = 0;

	ret = meson_rsv_scan(rsv_info);
	if (!rsv_info->valid) {
		pr_err("%s %d no %s info exist!", __func__, __LINE__,
		       rsv_info->name);
		ret = 1;
	}
	return ret;
}

static int meson_ext_rsv_info_read(struct meson_rsv_info_t *rsv_info,
			     u_char *dest, size_t size)
{
	u_char *temp;
	size_t len;
	int ret;

	if (unlikely(!rsv_info) || unlikely(!dest) || unlikely(!size)) {
		pr_err("%s read %s parameter error %p %zd\n",
		       __func__, rsv_info ? rsv_info->name : "", dest, size);
		return 1;
	}

	if (rsv_info->valid == 0) {
		pr_err("%s %s is invalid\n", __func__, rsv_info->name);
		return 1;
	}

	len = rsv_info->size;
	temp = vzalloc(len);
	ret = meson_rsv_read(rsv_info, temp);
	memcpy(dest, temp, len > size ? size : len);
	vfree(temp);
	pr_info("%s read 0x%zx bytes from %s, ret %d\n",
		    __func__, len > size ? size : len, rsv_info->name, ret);
	return ret;
}

static int meson_ext_rsv_info_write(struct meson_rsv_info_t *rsv_info,
			      u_char *source, size_t size)
{
	u_char *temp;
	size_t len;
	int ret;

	if (unlikely(!rsv_info) || unlikely(!source) || unlikely(!size)) {
		pr_err("%s write %s parameter error %p %zd\n",
			__func__, rsv_info ? rsv_info->name : "", source, size);
	}
	len = rsv_info->size;
	temp = vzalloc(len);
	memcpy(temp, source, len > size ? size : len);
	ret = meson_rsv_save(rsv_info, temp);
	vfree(temp);

	pr_info("%s write 0x%zx bytes to %s, ret %d\n",
		    __func__, len > size ? size : len, rsv_info->name, ret);
	return ret;
}

int meson_rsv_get_ddrp_page(void)
{
	struct meson_rsv_info_t *rsv_info;
	struct mtd_info *mtd;
	int ddrp_page, index;

	index = meson_rsv_name2index(DDR_NAND_MAGIC);
	if (index < 0)
		return -EINVAL;

	rsv_info = &rsv_handler->rsv_info[index];
	mtd = rsv_info->mtd;
	ddrp_page = rsv_info->valid_node->phy_blk_addr <<
		     (mtd->erasesize_shift - mtd->writesize_shift);
	ddrp_page += rsv_info->valid_node->phy_page_addr;

	return ddrp_page;
}
EXPORT_SYMBOL(meson_rsv_get_ddrp_page);

size_t meson_rsv_get_bbt_size(void)
{
	return rsv_handler->rsv_info[meson_rsv_name2index(BBT_NAND_MAGIC)].size;
}
EXPORT_SYMBOL(meson_rsv_get_bbt_size);

int meson_rsv_bbt_write(u_char *source, size_t size)
{
	int index = meson_rsv_name2index(BBT_NAND_MAGIC);

	return meson_ext_rsv_info_write(&rsv_handler->rsv_info[index],
					      source, size);
}
EXPORT_SYMBOL(meson_rsv_bbt_write);

static int meson_rsv_info_alloc_init(struct mtd_info *mtd,
				     struct meson_rsv_handler_t *handler,
				     struct meson_rsv_part_t *rsv_part)
{
	struct meson_rsv_info_t *rsv_info = &handler->rsv_info[handler->entries++];

	rsv_info->valid_node = kzalloc(sizeof(*rsv_info->valid_node), GFP_KERNEL);
	if (!rsv_info->valid_node) {
		handler->entries--;
		pr_err("%s no mem to alloc\n", __func__);
		return -ENOMEM;
	}

	rsv_info->mtd = mtd;
	rsv_info->handler = handler;
	rsv_info->read = meson_ext_rsv_info_read;
	rsv_info->write = meson_ext_rsv_info_write;
	strncpy(rsv_info->name, rsv_part->name, 4);
	rsv_info->start_block = rsv_part->block_start;
	rsv_info->end_block = rsv_part->block_start + rsv_part->block_cnt;
	rsv_info->size = rsv_part->size;
	rsv_info->valid_node->phy_blk_addr = -1;

	return 0;
}

static int meson_rsv_info_init_from_dtb(struct mtd_info *mtd,
					struct meson_rsv_handler_t *handler)
{
	struct device_node *child, *rsv_node;
	struct meson_rsv_part_t rsv_part;
	const char *name;

	rsv_node = of_get_child_by_name(dev_of_node(mtd->dev.parent),
					 "rsv_partition");
	if (!rsv_node)
		return -ENXIO;

	for_each_child_of_node(rsv_node, child) {
		if (of_property_read_string(child, "label", &name))
			return -EINVAL;
		strncpy(rsv_part.name, name, min_t(int, strlen(name), 4));
		if (of_property_read_u32(child, "block_start", &rsv_part.block_start))
			return -EINVAL;
		if (of_property_read_u32(child, "block_cnt", &rsv_part.block_cnt))
			return -EINVAL;
		if (of_property_read_u32(child, "size", &rsv_part.size))
			return -EINVAL;

		if (!strncmp(rsv_part.name, "nrsv", 4)) {
			rsv_block_cnt = rsv_part.block_cnt;
			continue;
		}

		if (rsv_part.block_cnt == 0)
			continue;
		if (meson_rsv_info_alloc_init(mtd, handler, &rsv_part))
			return -ENOMEM;
	}

	return 0;
}

static int meson_get_cmdline_part_info(char **str,
				 struct meson_rsv_part_t *rsv_part)
{
	/* The rsv partition format for cmdline is as follows:
	 * mtdrsvparts=<partdef>[,<partdef>]
	 * <partdef> = <size>@<block_cnt>@<block_start>(name)
	 */
	rsv_part->size = memparse(*str, str);
	if (**str != '@')
		return -EINVAL;
	(*str)++;

	rsv_part->block_cnt = memparse(*str, str);
	if (**str != '@')
		return -EINVAL;
	(*str)++;

	rsv_part->block_start = memparse(*str, str);
	if (**str != '(')
		return -EINVAL;
	(*str)++;

	if (sscanf(*str, "%31[^)]", rsv_part->name) != 1)
		return -EINVAL;
	return 0;
}

static int meson_rsv_info_init_from_cmdline(struct mtd_info *mtd,
					 struct meson_rsv_handler_t *handler)
{
	struct meson_rsv_part_t rsv_part;
	char *s, *token;

	s = kstrdup(aml_nand_get_rsv_cmdline(), GFP_KERNEL);
	if (!s)
		return -ENXIO;

	while ((token = strsep(&s, ","))) {
		if (meson_get_cmdline_part_info(&token, &rsv_part) < 0)
			return -EINVAL;

		rsv_part.block_start >>= mtd->erasesize_shift;
		rsv_part.block_cnt >>= mtd->erasesize_shift;
		if (!strncmp(rsv_part.name, "nrsv", 4))
			rsv_block_cnt = rsv_part.block_cnt;

		if (rsv_part.block_cnt == 0)
			continue;
		if (meson_rsv_info_alloc_init(mtd, handler, &rsv_part))
			return -ENOMEM;
	}

	return 0;
}

static int meson_rsv_info_init_from_default(struct mtd_info *mtd,
					 struct meson_rsv_handler_t *handler)
{
	struct meson_rsv_part_t rsv_part;
	u32 start = BOOT_TOTAL_PAGES >> (mtd->erasesize_shift - mtd->writesize_shift);
	int i;

	for (i = 0; i < ARRAY_SIZE(rsv_parts) && rsv_parts[i].block_cnt; i++) {
		strncpy(rsv_part.name, rsv_parts[i].name, 4);
		rsv_part.block_start = start;
		rsv_part.block_cnt = rsv_parts[i].block_cnt;
		start += rsv_parts[i].block_cnt;
		rsv_part.size = rsv_parts[i].size;

		if (!strncmp(rsv_part.name, BBT_NAND_MAGIC, 4)) {
			rsv_part.size = mtd->size >> mtd->erasesize_shift;
		} else if (!strncmp(rsv_part.name, ENV_NAND_MAGIC, 4)) {
			rsv_part.size = CONFIG_ENV_SIZE;
		} else if (!strncmp(rsv_part.name, KEY_NAND_MAGIC, 4)) {
			if (mtd->erasesize < 0x40000)
				rsv_part.size = mtd->erasesize >> 2;
			else
				rsv_part.size = 0x40000;
		} else if (!strncmp(rsv_part.name, DTB_NAND_MAGIC, 4)) {
			if (mtd->erasesize < 0x40000)
				rsv_part.size = mtd->erasesize >> 1;
			else
				rsv_part.size = 0x40000;
		} else if (!strncmp(rsv_part.name, DDR_NAND_MAGIC, 4)) {
			rsv_part.size = 0x1000;
		}

		if (meson_rsv_info_alloc_init(mtd, handler, &rsv_part))
			return -ENOMEM;
	}

	return 0;
}

static int meson_rsv_info_init(struct mtd_info *mtd,
			       struct meson_rsv_handler_t *handler)
{
	char dbg_rsv_parts_buf[512] = {};
	int ret, i, n = 0;

	ret = meson_rsv_info_init_from_dtb(mtd, handler);
	if (ret == 0) {
		n = sprintf(dbg_rsv_parts_buf, "%s", "rsvparts=dtb:");
		goto out;
	} else if (ret != -ENXIO) {
		pr_info("fail to parse reserved information from dtb\n");
		return ret;
	}

	ret = meson_rsv_info_init_from_cmdline(mtd, handler);
	if (ret == 0) {
		n = sprintf(dbg_rsv_parts_buf, "%s", "rsvparts=cmdline:");
		goto out;
	} else if (ret != -ENXIO) {
		pr_info("fail to parse reserved information from cmdline\n");
		return ret;
	}

	ret = meson_rsv_info_init_from_default(mtd, handler);
	if (ret < 0)
		return ret;
	n = sprintf(dbg_rsv_parts_buf, "%s", "rsvparts=default:");

out:
	for (i = 0; i < handler->entries; i++)
		n += sprintf(dbg_rsv_parts_buf + n, "%u@%u@%u(%s),",
			     handler->rsv_info[i].size,
			     handler->rsv_info[i].start_block,
			     handler->rsv_info[i].end_block,
			     handler->rsv_info[i].name);
	dbg_rsv_parts_buf[n - 1] = '\0';
	pr_info("%s\n", dbg_rsv_parts_buf);

	return 0;
}

static void meson_rsv_info_alloc_destroy(void)
{
	for (; rsv_handler->entries > 0; --rsv_handler->entries)
		kfree(rsv_handler->rsv_info[rsv_handler->entries - 1].valid_node);
}

int meson_rsv_init(struct mtd_info *mtd,
		   struct meson_rsv_handler_t *handler)
{
	int i, bbt_index, ret = 0;
	struct meson_rsv_info_t *info;
	s8 *block_status = handler->bbt_buf;
	int pages_per_blk = 1 << (mtd->erasesize_shift - mtd->writesize_shift);

	ret = meson_rsv_info_init(mtd, handler);
	if (ret)
		return ret;

	handler->free_node = kcalloc(rsv_block_cnt,
				     sizeof(struct free_node_t), GFP_KERNEL);
	if (!handler->free_node) {
		ret = -ENOMEM;
		goto error_malloc;
	}

	handler->freenodebitmask = 0;
	for (i = 0; i < rsv_block_cnt; i++)
		handler->free_node[i].index = i;

	rsv_handler = handler;
	bbt_index = meson_rsv_name2index(BBT_NAND_MAGIC);

	if (bbt_index >= 0)
		meson_rsv_check(&rsv_handler->rsv_info[bbt_index]);
	if (bbt_index < 0 || rsv_handler->rsv_info[bbt_index].valid == 0 ||
	    !block_status)
		goto error_rsv_bbt;
	meson_rsv_read(&rsv_handler->rsv_info[bbt_index], block_status);
	/* bl2 does not handle bad blocks, treats them as good blocks. */
	memset(block_status, 0, get_bl2_total_pages(mtd) / pages_per_blk);

	for (i = 0 ; i < handler->entries; i++) {
		info = &handler->rsv_info[i];

		if (!strncmp(info->name, BBT_NAND_MAGIC, 4))
			continue;
		meson_rsv_check(info);
		if (!strncmp(info->name, ENV_NAND_MAGIC, 4))
			meson_rsv_register_cdev(info, ENV_CDEV_NAME);
		else if (!strncmp(info->name, KEY_NAND_MAGIC, 4))
			meson_rsv_register_unifykey(info);
		else if (!strncmp(info->name, DTB_NAND_MAGIC, 4))
			meson_rsv_register_cdev(info, DTB_CDEV_NAME);
		else if (strncmp(info->name, BBT_NAND_MAGIC, 4))
			meson_rsv_register_cdev(info, info->name);
	}

	return ret;
error_rsv_bbt:
	pr_err("%s: meson rsv bbt failed\n", __func__);
	meson_rsv_info_alloc_destroy();

error_malloc:
	pr_err("%s: malloc free node failed\n", __func__);
	kfree(handler->free_node);

	rsv_handler = NULL;
	return ret;
}
EXPORT_SYMBOL(meson_rsv_init);
