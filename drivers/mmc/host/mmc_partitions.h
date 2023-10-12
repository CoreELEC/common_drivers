/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */

#ifndef _MMC_PARTITIONS_H_
#define _MMC_PARTITIONS_H_

#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>

#define     MAX_PART_NAME_LEN               16
#define     MAX_MMC_PART_NUM                32

/* MMC Partition Table */
#define     MMC_PARTITIONS_MAGIC            "MPT"

struct partitions {
	/* identifier string */
	char name[MAX_PART_NAME_LEN];
	/* partition size, byte unit */
	u64 size;
	/* offset within the master space, byte unit */
	u64 offset;
	/* master flags to mask out for this partition */
	unsigned int mask_flags;
};

struct mmc_partitions_fmt {
	char magic[4];
	unsigned char version[12];
	int part_num;
	int checksum;
	struct partitions partitions[MAX_MMC_PART_NUM];
};

int aml_emmc_partition_ops(struct mmc_card *card, struct gendisk *disk);

#endif
