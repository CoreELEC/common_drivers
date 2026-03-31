// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "page_info.h"
#include <linux/mtd/nand.h>
//#include <linux/amlogic/aml_spi_nand.h>

struct boot_info *page_info;
static int page_info_state;
static s32 page_info_adjust_bbtlen;

enum BOOT_LAYOUT_VERS {
	BOOT_DISCRETE_DEFAULT = 0,
	BOOT_DISCRETE_ALL,
	BOOT_DISCRETE_BL2,
	BOOT_DISCRETE_MAX,
};

unsigned char page_info_get_data_lanes_mode(void)
{
	return page_info->dev_cfg0.bus_width & 0x0f;
}
EXPORT_SYMBOL_GPL(page_info_get_data_lanes_mode);

unsigned char page_info_get_cmd_lanes_mode(void)
{
	return page_info->dev_cfg1.ca_lanes & 0x0f;
}
EXPORT_SYMBOL_GPL(page_info_get_cmd_lanes_mode);

unsigned char page_info_get_addr_lanes_mode(void)
{
	return (page_info->dev_cfg1.ca_lanes >> 4) & 0x0f;
}
EXPORT_SYMBOL_GPL(page_info_get_addr_lanes_mode);

unsigned char page_info_get_frequency_index(void)
{
	return page_info->host_cfg.frequency_index;
}
EXPORT_SYMBOL_GPL(page_info_get_frequency_index);

unsigned char page_info_get_adj_index(void)
{
	return page_info->host_cfg.mode_rx_adj & 0x3f;
}

unsigned char page_info_get_work_mode(void)
{
	return (page_info->host_cfg.mode_rx_adj >> 6) & 0x3;
}
EXPORT_SYMBOL_GPL(page_info_get_work_mode);

unsigned char page_info_get_line_delay1(void)
{
	return page_info->host_cfg.lines_delay[0];
}
EXPORT_SYMBOL_GPL(page_info_get_line_delay1);

unsigned char page_info_get_line_delay2(void)
{
	return page_info->host_cfg.lines_delay[1];
}
EXPORT_SYMBOL_GPL(page_info_get_line_delay2);

unsigned char page_info_get_core_div(void)
{
	return page_info->host_cfg.core_div;
}
EXPORT_SYMBOL_GPL(page_info_get_core_div);

unsigned char page_info_get_bus_cycle(void)
{
	return page_info->host_cfg.bus_cycle;
}
EXPORT_SYMBOL_GPL(page_info_get_bus_cycle);

unsigned char page_info_get_device_ecc_disable(void)
{
	return page_info->host_cfg.device_ecc_disable & 0x01;
}

unsigned int page_info_get_n2m_command(void)
{
	return page_info->host_cfg.n2m_cmd;
}
EXPORT_SYMBOL_GPL(page_info_get_n2m_command);

void page_info_set_n2m_command(unsigned int n2m_cmd)
{
	page_info->host_cfg.n2m_cmd = n2m_cmd;
}
EXPORT_SYMBOL_GPL(page_info_set_n2m_command);

unsigned int page_info_get_page_size(void)
{
	return page_info->dev_cfg0.page_size;
}
EXPORT_SYMBOL_GPL(page_info_get_page_size);

void page_info_set_page_size(unsigned int page_size)
{
	page_info->dev_cfg0.page_size = page_size;
}
EXPORT_SYMBOL_GPL(page_info_set_page_size);

unsigned char page_info_get_planes(void)
{
	return  page_info->dev_cfg0.planes_per_lun & 0x0f;
}

unsigned char page_info_get_plane_shift(void)
{
	return (page_info->dev_cfg0.planes_per_lun >> 4) & 0x0f;
}

unsigned char page_info_get_cache_plane_shift(void)
{
	return (page_info->dev_cfg0.bus_width >> 4) & 0x0f;
}

unsigned char page_info_get_cs_deselect_time(void)
{
	return page_info->dev_cfg1.cs_deselect_time;
}
EXPORT_SYMBOL_GPL(page_info_get_cs_deselect_time);

unsigned char page_info_get_dummy_cycles(void)
{
	return page_info->dev_cfg1.dummy_cycles;
}

unsigned int page_info_get_block_size(void)
{
	return page_info->dev_cfg1.block_size;
}
EXPORT_SYMBOL_GPL(page_info_get_block_size);

unsigned short *page_info_get_bbt(void)
{
	return &page_info->dev_cfg1.bbt[0];
}

unsigned char page_info_get_enable_bbt(void)
{
	return page_info->dev_cfg1.enable_bbt;
}

unsigned char page_info_get_high_speed_mode(void)
{
	return page_info->dev_cfg1.high_speed_mode;
}

unsigned char page_info_get_layout_method(void)
{
	return page_info->boot_layout.layout_method;
}

unsigned int page_info_get_boot_size(void)
{
	return page_info->boot_layout.boot_size;
}

unsigned int page_info_get_pages_in_block(void)
{
	unsigned int block_size, page_size;
	static unsigned int pages_in_block;

	if (pages_in_block)
		return pages_in_block;

	block_size = page_info_get_block_size();
	page_size = page_info_get_page_size();
	pages_in_block = block_size / page_size;

	return pages_in_block;
}

unsigned int page_info_get_pages_in_boot(void)
{
	unsigned int page_size, boot_size;

	page_size = page_info_get_page_size();
	boot_size = page_info_get_boot_size();

	return boot_size / page_size;
}

void page_info_initialize(unsigned int default_n2m,
			  unsigned char bus_width, unsigned char ca)
{
	page_info->dev_cfg0.page_size = sizeof(struct boot_info);
	page_info->dev_cfg0.planes_per_lun = 0;
	page_info->dev_cfg0.bus_width = bus_width;
	page_info->host_cfg.frequency_index = 0xFF;
	page_info->host_cfg.n2m_cmd = default_n2m;
	page_info->dev_cfg1.ca_lanes = ca;
	page_info->dev_cfg1.cs_deselect_time = 0xFF;
	page_info->dev_cfg1.dummy_cycles = 0xFF;
}
EXPORT_SYMBOL_GPL(page_info_initialize);

int get_page_info_version(void)
{
	return page_info->version & 0x0F;
}
EXPORT_SYMBOL_GPL(get_page_info_version);

int get_page_info_size(void)
{
	return sizeof(struct boot_info);
}
EXPORT_SYMBOL_GPL(get_page_info_size);

static void page_info_set_boot_layout(u8 boot_layout)
{
	page_info->version |= ((boot_layout & 0x0F) << 4);
}

static u32 get_boot_layout_info(struct mtd_info *mtd)
{
	static bool read;
	static u32 boot_layout;

	if (read)
		return boot_layout;

	if (of_property_read_u32(dev_of_node(mtd->dev.parent),
					"boot_layout", &boot_layout))
		pr_info("%s: not found boot_layout in dts\n", __func__);
	else
		pr_info("%s: found boot_layout(0x%x) in dts\n", __func__, boot_layout);

	read = true;

	return boot_layout;
}

int get_boot_layout_type(struct mtd_info *mtd)
{
	u32 boot_layout = get_boot_layout_info(mtd);

	return (boot_layout & 0x0F);
}
EXPORT_SYMBOL_GPL(get_boot_layout_type);

int get_bl2_copy_number(struct mtd_info *mtd)
{
	u32 boot_layout = get_boot_layout_info(mtd);
	int bl2_copy_number = ((boot_layout >> 8) & 0x0F);

	/* default 8 copies if bl2_copy_number not in boot_layout (specified via
	 * cmdline in U-boot configure, cmdline is configured it in v2019
	 * and beyond)
	 */
	if (!bl2_copy_number)
		bl2_copy_number = 8;

	return bl2_copy_number;
}
EXPORT_SYMBOL_GPL(get_bl2_copy_number);

int get_bl2_pages_per_copy(struct mtd_info *mtd)
{
	u32 boot_layout = get_boot_layout_info(mtd);
	int bl2_pages_per_copy = ((boot_layout >> 12) & 0xFFFF);

	/* if bl2_pages_per_copy not in boot_layout (specified via
	 * cmdline in U-boot configure, cmdline is configured it in v2019
	 * and beyond), get bl2_pages_per_copy by DEFAULT_ECC_MODE
	 */
	if (!bl2_pages_per_copy)
		bl2_pages_per_copy = get_bl2_total_pages(mtd) / get_bl2_copy_number(mtd);

	return bl2_pages_per_copy;
}
EXPORT_SYMBOL_GPL(get_bl2_pages_per_copy);

unsigned int get_bl2_total_pages(struct mtd_info *mtd)
{
	static u32 bl2_total_pages;
	u32 bl2_copy_number = get_bl2_copy_number(mtd);

	if (bl2_total_pages)
		return bl2_total_pages;

	if (bl2_copy_number)
		bl2_total_pages = bl2_copy_number * get_bl2_pages_per_copy(mtd);
	else
		bl2_total_pages = NAND_BOOT_MAX_PAGES;

	return bl2_total_pages;
}
EXPORT_SYMBOL_GPL(get_bl2_total_pages);

static unsigned int do_checksum(unsigned char *buf, int len)
{
	unsigned int i, checksum = 0;

	for (i = 0; i < len; i++)
		checksum += buf[i];

	return checksum;
}

static void page_info_adjust_content(struct boot_info *booti)
{
	/* The MAX_F_BAD_BLOCK_NUM is defaulting to 64, but it should be 16 in a1.
	 * so here to adjust it
	 */
	u8 *p = (u8 *)booti;
	u8 adjust_buffer[MAX_BYTES_IN_BOOTINFO];
	size_t len;

	memset(adjust_buffer, 0, MAX_BYTES_IN_BOOTINFO);
	booti->checksum = 0;

	len = (u8 *)&booti->dev_cfg1.bbt[page_info_adjust_bbtlen] - (u8 *)booti;
	memcpy(adjust_buffer, p, len);

	memcpy(adjust_buffer + len, (u8 *)&booti->dev_cfg1.enable_bbt,
		   (sizeof(struct boot_info) -
		   ((u8 *)&booti->dev_cfg1.enable_bbt - (u8 *)booti)));

	memcpy(p, adjust_buffer, min_t(int, sizeof(*booti), MAX_BYTES_IN_BOOTINFO));

	if (get_page_info_version() != PAGE_INFO_V2)
		booti->checksum = do_checksum(p, sizeof(*booti));
	else
		booti->checksum = do_checksum(p, 20);

	pr_info("page info adjust, checksum : 0x%x\n", booti->checksum);
}

static void page_info_init_from_mtd(struct mtd_info *mtd, u8 cmd, u32 fip_size, u32 fip_copies)
{
	struct nand_device *dev = mtd_to_nanddev(mtd);
	unsigned char ecc_steps;
	unsigned int check_len = sizeof(struct boot_info), i;
	enum PAGE_INFO_V page_info_ver;

	page_info_set_boot_layout((u8)get_boot_layout_type(mtd));

	page_info_ver = get_page_info_version();
	memcpy(page_info->magic, BOOTINFO_MAGIC, strlen(BOOTINFO_MAGIC));
	page_info->dev_cfg0.page_size = mtd->writesize;
	page_info->dev_cfg0.planes_per_lun = dev->memorg.planes_per_lun;
	if (page_info->dev_cfg0.planes_per_lun > 1) {
		page_info->dev_cfg0.planes_per_lun |= 6 << 4;
		page_info->dev_cfg0.bus_width =
			(mtd->writesize_shift + 1) << 4;
	}

	page_info->dev_cfg0.bus_width &= ~0x03;
	if (cmd == 0x6b)
		page_info->dev_cfg0.bus_width |= 2;
	else if (cmd == 0x3b)
		page_info->dev_cfg0.bus_width |= 1;
	NFC_Print("bus_width : %u", page_info->dev_cfg0.bus_width);
	if (page_info_ver == PAGE_INFO_V1) {
		/* for compatible,  a1/c1/c2 ... need to know fip's start and size */
		page_info->reserved[0] = 1024 / 64 + 48;
		page_info->reserved[1] = fip_size / mtd->erasesize;
		page_info->reserved[2] = fip_copies;
		page_info->dev_cfg1.block_size = mtd->erasesize;
	} else if (page_info_ver == PAGE_INFO_V2) {
		/* for compatible,  C3 use this field  */
		i = mtd->erasesize_shift + mtd->writesize_shift;
		page_info->reserved[2] = ((mtd->size >> i) ? (mtd->size >> i) : 1) & 0x3;
		check_len = 20;
	}

	if (page_info_ver != PAGE_INFO_V3)
		goto _do_final;

	ecc_steps = mtd->writesize >> 9;
	/* slc nand */
	if (cmd == 0)
		page_info->host_cfg.n2m_cmd = (DEFAULT_ECC_MODE & (~0x3F)) | ecc_steps;
	page_info->host_cfg.frequency_index = 0xFF;
	page_info->dev_cfg1.ca_lanes = 0;
	page_info->dev_cfg1.cs_deselect_time = 0xFF;
	page_info->dev_cfg1.dummy_cycles = 0xFF;
	page_info->dev_cfg1.block_size = mtd->erasesize;
	page_info->dev_cfg1.is_gang_programmer = 0;
	page_info->dev_cfg1.xor_bbt_start_block |= (1 << 24);
	page_info->dev_cfg1.block_num_in_chip = mtd->size >> mtd->erasesize_shift;

_do_final:
	if (get_boot_layout_type(mtd) == BOOT_DISCRETE_BL2 ||
	    get_bl2_copy_number(mtd) > 2)
		page_info->dev_cfg1.enable_bbt = 1;

	page_info->checksum = 0;
	page_info->checksum = do_checksum((unsigned char *)page_info, check_len);
	pr_info("page info updated checksum : 0x%x\n", page_info->checksum);
}

static void page_info_dump_info(void)
{
	unsigned char planes_per_lun, plane_shift, bus_width, cache_plane_shift;
	unsigned char high_speed_mode, cmd_lanes, addr_lanes;
	unsigned char enable_bbt;
	unsigned int block_size, page_size;
	unsigned char frequency_index, mode, rx_adj;
	unsigned char device_ecc_disable = 0;
	unsigned int n2m_cmd;

	planes_per_lun = page_info_get_planes();
	plane_shift = page_info_get_plane_shift();
	cache_plane_shift = page_info_get_cache_plane_shift();
	high_speed_mode = page_info_get_high_speed_mode();
	page_size = page_info_get_page_size();
	block_size = page_info_get_block_size();
	enable_bbt = page_info_get_enable_bbt();
	bus_width = page_info_get_data_lanes_mode();
	cmd_lanes = page_info_get_cmd_lanes_mode();
	addr_lanes = page_info_get_addr_lanes_mode();

	frequency_index = page_info_get_frequency_index();
	mode = page_info_get_work_mode();
	rx_adj = page_info_get_adj_index();
	device_ecc_disable = page_info_get_device_ecc_disable();
	n2m_cmd = page_info_get_n2m_command();

	pr_info("bus_width: 0x%x\n", bus_width);
	pr_info("cmd_lanes: 0x%x\n", cmd_lanes);
	pr_info("addr_lanes: 0x%x\n", addr_lanes);
	pr_info("page_size: 0x%x\n", page_size);
	pr_info("planes_per_lun: 0x%x\n", planes_per_lun);
	pr_info("plane_shift: 0x%x\n", plane_shift);
	pr_info("cache_plane_shift: 0x%x\n", cache_plane_shift);
	pr_info("block_size: 0x%x\n", block_size);
	pr_info("high_speed_mode: 0x%x\n", high_speed_mode);
	pr_info("enable_bbt: 0x%x\n", enable_bbt);

	pr_info("frequency_index: 0x%x\n", frequency_index);
	pr_info("mode: 0x%x\n", mode);
	pr_info("rx_adj: 0x%x\n", rx_adj);
	pr_info("device_ecc_disable: 0x%x\n", device_ecc_disable);
	pr_info("n2m_cmd: 0x%x\n", n2m_cmd);
}

void page_info_set_state(enum PAGE_INFO_STATE state)
{
	page_info_state = state;
}
EXPORT_SYMBOL_GPL(page_info_set_state);

int page_info_get_state(void)
{
	return page_info_state;
}
EXPORT_SYMBOL_GPL(page_info_get_state);

unsigned char *page_info_post_init(struct mtd_info *mtd, u8 cmd, u32 fip_size, u32 fip_copies)
{
	page_info_init_from_mtd(mtd, cmd, fip_size, fip_copies);
	page_info_dump_info();
	return (unsigned char *)page_info;
}
EXPORT_SYMBOL_GPL(page_info_post_init);

void page_info_fetch_content(void *dst, int len)
{
	memset(dst, 0, len);
	memcpy(dst, (const void *)page_info, min_t(int, MAX_BYTES_IN_BOOTINFO, len));
	if (page_info_adjust_bbtlen > 0)
		page_info_adjust_content((struct boot_info *)dst);
}
EXPORT_SYMBOL_GPL(page_info_fetch_content);

int page_info_pre_init(u8 *boot_info, int version)
{
	page_info = (struct boot_info *)boot_info;
	page_info->version |= (version & 0x0F);
	return 0;
}
EXPORT_SYMBOL_GPL(page_info_pre_init);

bool page_info_is_page(struct mtd_info *mtd, int page)
{
	enum PAGE_INFO_V page_info_ver;
	bool is_info_page = 0;
	int pages_per_copy = get_bl2_pages_per_copy(mtd);
	int bl2_copy_number = get_bl2_copy_number(mtd);

	if (!pages_per_copy) {
		page_info_ver = get_page_info_version();
		if (page_info_ver == PAGE_INFO_V1)
			is_info_page =
			unlikely(((page % 128) == 31) && (page < NAND_BOOT_MAX_PAGES));
		else if (page_info_ver == PAGE_INFO_V2 || page_info_ver == PAGE_INFO_V3)
			is_info_page = unlikely(!(page % 128) && (page < NAND_BOOT_MAX_PAGES));
	} else {
		if (page < pages_per_copy * bl2_copy_number)
			is_info_page = unlikely(!(page % pages_per_copy));
	}

	return is_info_page;
}
EXPORT_SYMBOL_GPL(page_info_is_page);

void meson_nand_set_page_info_bbtlen(s32 adjust_len)
{
	if (adjust_len >= MAX_F_BAD_BLOCK_NUM) {
		pr_info("adjust page info bbt len (%d) invalid\n", adjust_len);
		return;
	}
	page_info_adjust_bbtlen = adjust_len;
	pr_info("adjust page info bbt len to %d\n", page_info_adjust_bbtlen);
}
EXPORT_SYMBOL_GPL(meson_nand_set_page_info_bbtlen);
