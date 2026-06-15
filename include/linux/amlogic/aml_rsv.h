/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_RSV_H_
#define __MESON_RSV_H_

#define CONFIG_ENV_SIZE  (64 * 1024U)

#define DEFAULT_NAND_RSV_BLOCK_NUM 48
#define DEFAULT_NAND_GAP_BLOCK_NUM 4
#define DEFAULT_NAND_BBT_BLOCK_NUM 4
#define DEFAULT_NAND_ENV_BLOCK_NUM 8
#define DEFAULT_NAND_KEY_BLOCK_NUM 8
#define DEFAULT_NAND_DTB_BLOCK_NUM 4
#define DEFAULT_NAND_DDR_BLOCK_NUM 2

#define RSV_NAND_MAGIC  "nrsv"
#define GAP_NAND_MAGIC  "ngap"
#define BBT_NAND_MAGIC	"nbbt"
#define ENV_NAND_MAGIC	"nenv"
#define KEY_NAND_MAGIC	"nkey"
#define SEC_NAND_MAGIC	"nsec"
#define DTB_NAND_MAGIC	"ndtb"
#define DDR_NAND_MAGIC	"nddr"

/*define abnormal state for reserved area*/
#define POWER_ABNORMAL_FLAG	0x01
#define ECC_ABNORMAL_FLAG	0x02

#define BBT_START_BLOCK     20
#define BBT_TOTAL_BLOCKS    4

struct meson_rsv_part_t {
	char name[8];
	u32 block_cnt;
	u32 size;
	u32 block_start;
};

struct meson_rsv_info_t {
	struct mtd_info *mtd;
	struct valid_node_t *valid_node;
	struct free_node_t *free_node;
	unsigned int start_block;
	unsigned int end_block;
	unsigned int size;
	char name[8];
	u_char valid;
	u_char init;
	void *handler;
	int (*read)(struct meson_rsv_info_t *rsv_info, u_char *dest, size_t size);
	int (*write)(struct meson_rsv_info_t *rsv_info, u_char *src, size_t size);
};

struct valid_node_t {
	s16 ec;
	s16	phy_blk_addr;
	s16	phy_page_addr;
	int timestamp;
	s16 status;
};

struct free_node_t {
	unsigned int index;
	s16 ec;
	s16	phy_blk_addr;
	int dirty_flag;
	struct free_node_t *next;
};

struct oobinfo_t {
	char name[4];//4
	s16 ec;//2
	unsigned timestamp:15;
	unsigned status_page:1;
};

struct meson_rsv_ops {
	int (*_erase)(struct mtd_info *mtd, struct erase_info *einfo);
	int (*_write_oob)(struct mtd_info *mtd, loff_t to, struct mtd_oob_ops *ops);
	int (*_read_oob)(struct mtd_info *mtd, loff_t from, struct mtd_oob_ops *ops);
	int (*_block_markbad)(struct mtd_info *mtd, loff_t off);
	int (*_block_isbad)(struct mtd_info *mtd, loff_t off);
	int (*_get_device)(struct mtd_info *mtd);
	void (*_release_device)(struct mtd_info *mtd);
};

#define MAX_MESON_RSV_INFO_NUM 8

struct meson_rsv_handler_t {
	struct mtd_info *mtd;
	unsigned long long freenodebitmask;
	struct free_node_t *free_node;
	int entries;
	struct meson_rsv_info_t rsv_info[MAX_MESON_RSV_INFO_NUM];
	struct meson_rsv_ops rsv_ops;
	s8 *bbt_buf;
};

#include<linux/cdev.h>
#define DTB_CDEV_NAME "dtb"
#define ENV_CDEV_NAME "nand_env"

struct meson_rsv_user_t {
	struct meson_rsv_info_t *info;
	dev_t devt;
	struct cdev cdev;
	struct device *dev;
	struct class *cls;
	/* in case crash */
	struct mutex lock;
};

int meson_rsv_prase_parameter_from_dtb(struct mtd_info *mtd,
	struct device_node *part_np);
int meson_rsv_prase_parameter_from_cmdline(struct mtd_info *mtd);
int meson_rsv_register_cdev(struct meson_rsv_info_t *info, char *name);
int meson_rsv_register_unifykey(struct meson_rsv_info_t *key);
size_t meson_rsv_get_bbt_size(void);
int meson_rsv_bbt_write(u_char *source, size_t size);
int meson_rsv_bbt_read(u_char *source, size_t size);
int meson_rsv_init(struct mtd_info *mtd, struct meson_rsv_handler_t *handler);

u32 meson_rsv_get_block_cnt(void);
int meson_rsv_name2index(const char *name);
void meson_rsv_check_all_except_bbt(void);
int meson_rsv_check_bbt(void);
char *aml_nand_get_rsv_cmdline(void);

s32 amlnf_key_read(u8 *buf, u32 len, u32 *actual_length);
s32 amlnf_key_write(u8 *buf, u32 len, u32 *actual_length);
int meson_rsv_get_ddrp_page(void);
#endif/* __MESON_RSV_H_ */
