// SPDX-License-Identifier: (GPL-2.0+ OR MIT)

//#define DEBUG
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/slab.h>

#include <linux/scatterlist.h>
#include <linux/swap.h>		/* For nr_free_buffer_pages() */
#include <linux/list.h>

#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/seq_file.h>
#include <linux/module.h>

#include "core.h"
#include "card.h"
#include "host.h"
#include "bus.h"
#include "mmc_ops.h"
#include <linux/types.h>
#include <linux/stddef.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/scatterlist.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/aml_sd.h>
#include "mmc_common.h"
#include "mmc_partitions.h"
#include "../../../../block/blk.h"

static struct mmc_partitions_fmt *pt_fmt;

static void bdev_set_nr_sectors(struct block_device *bdev, sector_t sectors)
{
	spin_lock(&bdev->bd_size_lock);
	i_size_write(bdev->bd_inode, (loff_t)sectors << SECTOR_SHIFT);
	spin_unlock(&bdev->bd_size_lock);
}

static inline int card_proc_info(struct seq_file *m, char *dev_name, int i)
{
	struct partitions *this = &pt_fmt->partitions[i];

	if (i >= pt_fmt->part_num)
		return 0;

	seq_printf(m, "%s%02d: %9llx %9x \"%s\"\n", dev_name,
		   i + 1, (unsigned long long)this->size,
		   512 * 1024, this->name);
	return 0;
}

static int card_proc_show(struct seq_file *m, void *v)
{
	int i;

	seq_puts(m, "dev:	size   erasesize  name\n");
	for (i = 0; i < 16; i++)
		card_proc_info(m, "inand", i);

	return 0;
}

static int card_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, card_proc_show, inode->i_private);
}

static const struct proc_ops card_proc_fops = {
	.proc_open = card_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/* This function is copy and modified from kernel function add_partition() */
static struct block_device *add_emmc_each_part(struct gendisk *disk, int partno,
				sector_t start, sector_t len, int flags,
				struct partition_meta_info *info)
{
	dev_t devt = MKDEV(0, 0);
	struct device *ddev = disk_to_dev(disk);
	struct device *pdev;
	struct block_device *bdev;
	const char *dname;
	int err;

	lockdep_assert_held(&disk->open_mutex);

	if (partno >= disk_max_parts(disk))
		return ERR_PTR(-EINVAL);

	/*
	 * Partitions are not supported on zoned block devices that are used as
	 * such.
	 */
	switch (disk->queue->limits.zoned) {
	case BLK_ZONED_HM:
		pr_warn("%s: partitions not supported on host managed zoned block device\n",
			disk->disk_name);
		return ERR_PTR(-ENXIO);
	case BLK_ZONED_HA:
		pr_info("%s: disabling host aware zoned block device support due to partitions\n",
			disk->disk_name);
		blk_queue_set_zoned(disk, BLK_ZONED_NONE);
		break;
	case BLK_ZONED_NONE:
		break;
	}

	if (xa_load(&disk->part_tbl, partno))
		return ERR_PTR(-EBUSY);

	/* ensure we always have a reference to the whole disk */
	get_device(disk_to_dev(disk));

	err = -ENOMEM;
	bdev = bdev_alloc(disk, partno);
	if (!bdev)
		goto out_put_disk;

	bdev->bd_start_sect = start;
	bdev_set_nr_sectors(bdev, len);

	pdev = &bdev->bd_device;
	dname = dev_name(ddev);
	dev_set_name(pdev, "%s", info->volname);

	device_initialize(pdev);
	pdev->class = &block_class;
	pdev->type = &part_type;
	pdev->parent = ddev;

	/* in consecutive minor range? */
	if (bdev->bd_partno < disk->minors) {
		devt = MKDEV(disk->major, disk->first_minor + bdev->bd_partno);
	} else {
		err = blk_alloc_ext_minor();
		if (err < 0)
			goto out_put;
		devt = MKDEV(BLOCK_EXT_MAJOR, err);
	}
	pdev->devt = devt;

	if (info) {
		err = -ENOMEM;
		bdev->bd_meta_info = kmemdup(info, sizeof(*info), GFP_KERNEL);
		if (!bdev->bd_meta_info)
			goto out_put;
	}

	/* delay uevent until 'holders' subdir is created */
	dev_set_uevent_suppress(pdev, 1);
	err = device_add(pdev);
	if (err)
		goto out_put;

	err = -ENOMEM;
	bdev->bd_holder_dir = kobject_create_and_add("holders", &pdev->kobj);
	if (!bdev->bd_holder_dir)
		goto out_del;

	dev_set_uevent_suppress(pdev, 0);

	/* everything is up and running, commence */
	err = xa_insert(&disk->part_tbl, partno, bdev, GFP_KERNEL);
	if (err)
		goto out_del;
	bdev_add(bdev, devt);

	/* suppress uevent if the disk suppresses it */
	if (!dev_get_uevent_suppress(ddev))
		kobject_uevent(&pdev->kobj, KOBJ_ADD);
	return bdev;

out_del:
	kobject_put(bdev->bd_holder_dir);
	device_del(pdev);
out_put:
	put_device(pdev);
	return ERR_PTR(err);
out_put_disk:
	put_disk(disk);
	return ERR_PTR(err);
}

static int add_emmc_partition(struct gendisk *disk,
			      struct mmc_partitions_fmt *pt_fmt)
{
	unsigned int i;
	struct block_device *ret = NULL;
	u64 offset, size, cap;
	struct partitions *pp;
	struct proc_dir_entry *proc_card;
	struct partition_meta_info info = {0};

	pr_info("%s\n", __func__);

	cap = get_capacity(disk); /* unit:512 bytes */
	for (i = 0; i < pt_fmt->part_num; i++) {
		pp = &pt_fmt->partitions[i];
		offset = pp->offset >> 9; /* unit:512 bytes */
		size = pp->size >> 9; /* unit:512 bytes */
		if ((offset + size) <= cap) {
			snprintf(info.volname, sizeof(info.volname), "%s", pp->name);
			ret = add_emmc_each_part(disk, 1 + i, offset,
						 size, 0, &info);

			pr_info("[%sp%02d] %20s  offset 0x%012llx, size 0x%012llx %s\n",
				disk->disk_name, 1 + i,
				pp->name, offset << 9,
				size << 9, IS_ERR(ret) ? "add fail" : "");
		} else {
			pr_info("[%s] %s: partition exceeds device capacity:\n",
				__func__, disk->disk_name);

			pr_info("%20s	offset 0x%012llx, size 0x%012llx\n",
				pp->name, offset << 9, size << 9);

			break;
		}
	}

	/* create /proc/inand */
	proc_card = proc_create("inand", 0444, NULL, &card_proc_fops);
	if (!proc_card)
		pr_info("[%s] create /proc/inand fail.\n", __func__);

	/* create /proc/ntd */
	if (!proc_create("ntd", 0444, NULL, &card_proc_fops))
		pr_info("[%s] create /proc/ntd fail.\n", __func__);

	return 0;
}

static int is_card_emmc(struct mmc_card *card)
{
	struct mmc_host *mmc = card->host;
	struct meson_host *host = mmc_priv(mmc);

	/* emmc port, so it must be an eMMC or TSD */
	if (aml_card_type_mmc(host))
		return 0;
	else
		return 1;
}

static int mmc_partition_tbl_checksum_calc(struct partitions *part,
					   int part_num)
{
	int i, j;
	u32 checksum = 0, *p;

	for (i = 0; i < part_num; i++) {
		p = (u32 *)part;

		for (j = sizeof(struct partitions) / sizeof(checksum);
				j > 0; j--) {
			checksum += *p;
			p++;
		}
	}

	return checksum;
}

static int mmc_read_partition_tbl(struct mmc_card *card,
				  struct mmc_partitions_fmt *pt_fmt)
{
	int ret = 0, start_blk, size, blk_cnt;
	int bit = card->csd.read_blkbits;
	int blk_size = 1 << bit; /* size of a block */
	char *buf, *dst;

	buf = kmalloc(blk_size, GFP_KERNEL);
	if (!buf) {
		/*	pr_info("malloc failed for buffer!\n");*/
		ret = -ENOMEM;
		goto exit_err;
	}
	memset(pt_fmt, 0, sizeof(struct mmc_partitions_fmt));
	memset(buf, 0, blk_size);
	start_blk = get_reserve_partition_off_from_tbl();
	if (start_blk < 0) {
		ret = -EINVAL;
		goto exit_err;
	}
	start_blk >>= bit;
	size = sizeof(struct mmc_partitions_fmt);
	dst = (char *)pt_fmt;
	if (size >= blk_size) {
		blk_cnt = size >> bit;
		ret = mmc_read_internal(card, start_blk, blk_cnt, dst);
		if (ret) { /* error */
			goto exit_err;
		}
		start_blk += blk_cnt;
		dst += blk_cnt << bit;
		size -= blk_cnt << bit;
	}
	if (size > 0) { /* the last block */
		ret = mmc_read_internal(card, start_blk, 1, buf);
		if (ret)
			goto exit_err;
		memcpy(dst, buf, size);
	}

	if ((strncmp(pt_fmt->magic, MMC_PARTITIONS_MAGIC,
		     sizeof(pt_fmt->magic)) == 0) && pt_fmt->part_num > 0 &&
			pt_fmt->part_num <= MAX_MMC_PART_NUM &&
			pt_fmt->checksum == mmc_partition_tbl_checksum_calc
			(pt_fmt->partitions, pt_fmt->part_num)) {
		ret = 0; /* everything is OK now */

	} else {
		if (strncmp(pt_fmt->magic, MMC_PARTITIONS_MAGIC,
			    sizeof(pt_fmt->magic)) != 0) {
			pr_info("magic error: %s\n", pt_fmt->magic);
		} else if ((pt_fmt->part_num < 0) ||
			    (pt_fmt->part_num > MAX_MMC_PART_NUM)) {
			pr_info("partition number error: %d\n",
				pt_fmt->part_num);
		} else {
			pr_info("checksum error: pt_fmt->checksum=%d,calc_result=%d\n",
				pt_fmt->checksum,
				mmc_partition_tbl_checksum_calc
				(pt_fmt->partitions, pt_fmt->part_num));
		}

		pr_info("[%s]: partition verified error\n", __func__);
		ret = -1; /* the partition information is invalid */
	}

exit_err:
	kfree(buf);

	pr_info("[%s] mmc read partition %s!\n",
		__func__, (ret == 0) ? "OK" : "ERROR");

	return ret;
}

int aml_emmc_partition_ops(struct mmc_card *card, struct gendisk *disk)
{
	int ret = 0;

	pr_info("Enter %s\n", __func__);
	if (is_card_emmc(card)) /* not emmc, nothing to do */
		return 0;

	mmc_claim_host(card->host);
	aml_disable_mmc_cqe(card);

	pt_fmt = kmalloc(sizeof(*pt_fmt), GFP_KERNEL);
	if (!pt_fmt) {
		/*	pr_info(
		 *	"[%s] malloc failed for struct mmc_partitions_fmt!\n",
		 *	__func__);
		 */
		aml_enable_mmc_cqe(card);
		mmc_release_host(card->host);
		return -ENOMEM;
	}

	ret = mmc_read_partition_tbl(card, pt_fmt);
	if (ret == 0) { /* ok */
		ret = add_emmc_partition(disk, pt_fmt);
	}
	aml_enable_mmc_cqe(card);
	mmc_release_host(card->host);

	pr_info("Exit %s %s.\n", __func__, (ret == 0) ? "OK" : "ERROR");
	return ret;
}
