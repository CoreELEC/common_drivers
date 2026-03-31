/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_PAGEINFO_H_
#define __AML_PAGEINFO_H_
#include <linux/mtd/mtd.h>

#define MAX_BYTES_IN_BOOTINFO	(512)

#ifdef __PXP_DEBUG__
#define NFC_Print(...)	pr_info(...)

#define DUMP_BUFFER(buffer, size, loop, actual)		\
{							\
	unsigned char *info;				\
	unsigned int _loop = loop;			\
	unsigned int i, j, each_size = (size) / _loop;	\
	for (i = 0; i < _loop; i++) {			\
		info = \
		(unsigned char *)((unsigned long)(buffer) + i * each_size);	\
		for (j = 0; j < (actual); j++, info++) {\
			pr_info(" ");			\
			pr_info("%02x", *info);		\
		}					\
		pr_info("\n");				\
	}						\
}
#else
#define NFC_Print(...)
#define DUMP_BUFFER(buffer, size, loop, actual)
#endif

/* *NOTE* Change the setting for different SOCs */
// 2 << 20 : code1code0(10b')
// 1 << 19 : rand enable
// 1 << 17 : read, not write
// 7 << 14 : ECC mode which is from 0 to 7; Maybe different setting here,
//	   : such as 2 for AXG serials, 7 for g12a.
//	   : 0->none
//	   : 1->BCH8/512
//	   : 2->BCH8/1024
//	   : 3->BCH24/1024
//	   : 4->BCH30/1024
//	   : 5->BCH40/1024
//	   : 6->BCH50/1024
//	   : 7->BCH60/1024
// 1 << 13 : short mode enable
// 48 << 6 : ECC page size
// 1 << 0  : ECC page number
#define DEFAULT_ECC_MODE		\
(					\
	(2 << 20) |			\
	(0 << 19) |			\
	(1 << 17) |			\
	(1 << 14) |			\
	(0 << 13) |			\
	(0 << 6) |			\
	(1 << 0)			\
)

#define MAX_CLK_PROVIDER	13

enum BL2_LAYOUT_VERS {
	LAYOUT_VER0,
	LAYOUT_VER1,
	LAYOUT_VER2,
	LAYOUT_VER3,
	LAYOUT_VER_MAX,
};

enum PAGE_INFO_V {
	PAGE_INFO_V1 = 1,
	PAGE_INFO_V2,
	PAGE_INFO_V3
};

enum PAGE_INFO_STATE {
	PAGE_INFO_UNDO = 0,
	PAGE_INFO_DOING,
	PAGE_INFO_DONE
};

int get_page_info_version(void);
int get_page_info_size(void);
unsigned int get_bl2_total_pages(struct mtd_info *mtd);
int get_boot_layout_type(struct mtd_info *mtd);
int get_bl2_copy_number(struct mtd_info *mtd);
int get_bl2_pages_per_copy(struct mtd_info *mtd);
unsigned char page_info_get_work_mode(void);
unsigned char page_info_get_addr_lanes_mode(void);
unsigned char page_info_get_cmd_lanes_mode(void);
unsigned char page_info_get_data_lanes_mode(void);
unsigned char page_info_get_line_delay1(void);
unsigned char page_info_get_line_delay2(void);
unsigned char page_info_get_core_div(void);
unsigned char page_info_get_bus_cycle(void);
unsigned char page_info_get_cs_deselect_time(void);
unsigned int page_info_get_block_size(void);
unsigned int page_info_get_page_size(void);
void page_info_set_page_size(unsigned int page_size);
unsigned int page_info_get_n2m_command(void);
void page_info_set_n2m_command(unsigned int n2m_cmd);
int page_info_get_state(void);
void page_info_set_state(enum PAGE_INFO_STATE state);
unsigned char page_info_get_frequency_index(void);
void page_info_initialize(unsigned int default_n2m,
		unsigned char bus_width, unsigned char ca);
int page_info_pre_init(u8 *boot_info, int version);
bool page_info_is_page(struct mtd_info *mtd, int page);
unsigned char *page_info_post_init(struct mtd_info *mtd,
		u8 cmd, u32 fip_size, u32 fip_copies);
void meson_nand_set_page_info_bbtlen(s32 adjust_len);
void page_info_fetch_content(void *dst, int len);
#endif
