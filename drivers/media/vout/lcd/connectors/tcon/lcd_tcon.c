// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/major.h>
#include <linux/compat.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/aml_free_reserved.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#include <linux/amlogic/media/vout/panel_utils/aml_model.h>
#include <linux/amlogic/media/vout/panel_utils/lcd_resman.h>
#include <linux/amlogic/media/vout/backlight/backlight.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_tcon_fw.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_BACKLIGHT
#include <linux/amlogic/media/vout/backlight/aml_bl.h>
#endif
#include <linux/fs.h>
#include <linux/uaccess.h>
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
#include <linux/amlogic/tee.h>
#endif
#include "../../lcd_common.h"
#include "../../lcd_reg.h"
#include "lcd_tcon.h"
#include "lcd_tcon_init.h"
#include "lcd_tcon_pdf.h"
#include "lcd_tcon_swpdf.h"
#include "lcd_tcon_rdma.h"
#include "../lcd_connector.h"

enum {
	TCON_AXI_MEM_TYPE_OD = 0,
	TCON_AXI_MEM_TYPE_DEMURA,
};

static struct lcd_tcon_config_s *lcd_tcon_conf;
static struct tcon_rmem_s tcon_rmem = {
	.flag = 0,
	.rsv_mem_size = 0,
	.rsv_mem_paddr = 0,
};

static struct tcon_mem_map_table_s tcon_mm_table = {
	.version = 0xff,
	.block_cnt = 0,
	.data_complete = 0,
	.bin_path_valid = 0,
	.effect_id = 0xff,
	.effect_type = 0xff,

	.lut_valid_flag = 0,
};

static struct lcd_tcon_local_cfg_s tcon_local_cfg;

static void lcd_tcon_data_multi_current_set(struct tcon_mem_map_table_s *mm_table,
		unsigned short block_type, unsigned int index);
static int lcd_tcon_data_multi_remvoe(struct tcon_mem_map_table_s *mm_table);
static int lcd_tcon_data_multi_reset(struct tcon_mem_map_table_s *mm_table);

#ifdef TCON_DBG_TIME
static unsigned long long *dbg_vsync_time;
static unsigned int dbg_vsync_cnt, dbg_cnt_0;

static inline void lcd_tcon_dbg_vsync_time_save(unsigned long long n,
		int line_cnt, unsigned int flag)
{
	if (!dbg_vsync_time)
		return;

	if (dbg_vsync_cnt >= dbg_cnt_0)
		dbg_vsync_cnt = 0;
	if ((flag >> 12) & 0xf) {
		dbg_vsync_time[dbg_vsync_cnt++] = n;
		dbg_vsync_time[dbg_vsync_cnt++] = 0xffff000000000000 | (line_cnt << 16) | flag;
	} else {
		dbg_vsync_time[dbg_vsync_cnt++] = n;
		dbg_vsync_time[dbg_vsync_cnt++] = (line_cnt << 16) | flag;
	}
}

static void lcd_tcon_time_print(unsigned long long *table)
{
	unsigned int len, i;
	char *buf;

	buf = kcalloc(1024, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	len = 0;
	for (i = 0; i < 9; i++)
		len += sprintf(buf + len, " %llu,", table[i]);
	len += sprintf(buf + len, " %llu", table[9]);
	pr_err("%s\n", buf);

	kfree(buf);
}

void lcd_tcon_dbg_trace_clear(void)
{
	memset(tcon_mm_table.vsync_time, 0, sizeof(unsigned long long) * 10);
	if (dbg_vsync_time)
		memset(dbg_vsync_time, 0, sizeof(unsigned long long) * dbg_cnt_0);
	dbg_vsync_cnt = 0;
}

void lcd_tcon_dbg_trace_print(void)
{
	char *buf;
	unsigned long long data;
	unsigned int len, n, m;
	int i, j;

	len = 24 * 20;
	buf = kcalloc(len, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	pr_err("vsync_time:\n");
	lcd_tcon_time_print(tcon_mm_table.vsync_time);

	pr_info("\ndbg_vsync_time:\n");
	if (dbg_vsync_time) {
		for (i = 0; i < dbg_cnt_0; i += 8) {
			n = 0;
			m = 0;
			for (j = 0; j < 8; j++) {
				data = dbg_vsync_time[i + j];
				if (j % 2)
					n += sprintf(buf + n, " 0x%llx", data);
				else
					n += sprintf(buf + n, " %llu", data);
				if (data)
					m = 1;
			}
			pr_err("%s\n", buf);
			if (m == 0)
				break;
		}
	}
	kfree(buf);
}
#endif

/* **********************************
 * tcon common function
 * **********************************
 */
int lcd_tcon_valid_check(void)
{
	if (!lcd_tcon_conf) {
		LCD_ERR(NULL, "invalid tcon data\n");
		return -1;
	}
	if (lcd_tcon_conf->tcon_valid == 0) {
		LCD_ERR(NULL, "invalid tcon\n");
		return -1;
	}

	return 0;
}

struct lcd_tcon_config_s *get_lcd_tcon_config(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return lcd_tcon_conf;
}

struct tcon_rmem_s *get_lcd_tcon_rmem(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return &tcon_rmem;
}

struct tcon_mem_map_table_s *get_lcd_tcon_mm_table(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return &tcon_mm_table;
}

struct lcd_tcon_local_cfg_s *get_lcd_tcon_local_cfg(void)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return NULL;

	return &tcon_local_cfg;
}

unsigned int lcd_tcon_data_size_align(unsigned int size)
{
	unsigned int new_size;

	/* ready for burst 128bit */
	new_size = ((size + 15) / 16) * 16;

	return new_size;
}

unsigned char lcd_tcon_checksum(unsigned char *buf, unsigned int len)
{
	unsigned int temp = 0;
	unsigned int i;

	if (!buf)
		return 0;
	if (len == 0)
		return 0;
	for (i = 0; i < len; i++)
		temp += buf[i];

	return (unsigned char)(temp & 0xff);
}

unsigned char lcd_tcon_lrc(unsigned char *buf, unsigned int len)
{
	unsigned char temp = 0;
	unsigned int i;

	if (!buf)
		return 0xff;
	if (len == 0)
		return 0xff;
	temp = buf[0];
	for (i = 1; i < len; i++)
		temp = temp ^ buf[i];

	return temp;
}

void lcd_tcon_mem_sync(struct aml_lcd_drv_s *pdrv,
		unsigned long paddr, unsigned int mem_size)
{
	if (!pdrv || !paddr || !mem_size)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ISR)
		LCDPR("%s: paddr=0x%lx, mem_size=%#x\n", __func__, paddr, mem_size);

	dma_sync_single_for_device(pdrv->dev,
		paddr,
		PAGE_ALIGN(mem_size),
		DMA_TO_DEVICE);
}

unsigned char *lcd_tcon_paddrtovaddr(unsigned long paddr, unsigned int mem_size)
{
	unsigned int highmem_flag = 0;
	int max_mem_size = 0;
	void *vaddr = NULL;

	if (tcon_rmem.flag == 0) {
		LCDPR("%s: invalid paddr\n", __func__);
		return NULL;
	}

	if (tcon_rmem.flag == 1) {
		highmem_flag = PageHighMem(phys_to_page(paddr));
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
			LCDPR("%s: paddr 0x%lx highmem_flag:%d\n",
				__func__, paddr, highmem_flag);
		}
		if (highmem_flag) {
			max_mem_size = PAGE_ALIGN(mem_size);
			max_mem_size = roundup(max_mem_size, PAGE_SIZE);
			vaddr = lcd_vmap(paddr, max_mem_size);
			if (!vaddr) {
				LCDPR("tcon paddr mapping error: addr: 0x%lx\n",
					(unsigned long)paddr);
				return NULL;
			}
			/*LCDPR("tcon vaddr: 0x%p\n", vaddr);*/
		} else {
			vaddr = phys_to_virt(paddr);
			if (!vaddr) {
				LCD_ERR(NULL, "tcon vaddr mapping failed: 0x%lx\n",
					(unsigned long)paddr);
				return NULL;
			}
			/*LCDPR("tcon vaddr: 0x%p\n", vaddr);*/
		}
	} else if (tcon_rmem.flag == 2) {
		vaddr = ioremap_cache(paddr, mem_size);
		if (!vaddr) {
			LCD_ERR(NULL, "tcon vaddr mapping failed: 0x%lx, size: 0x%x\n",
				(unsigned long)paddr, mem_size);
			return NULL;
		}

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("tcon paddr:0x%lx, vaddr:0x%px, size:0x%x\n",
				(unsigned long)paddr, vaddr, mem_size);
		}
	}

	return (unsigned char *)vaddr;
}

/* **********************************
 * tcon function api
 * **********************************
 */
//ret:  bit[0]:tcon: fatal error, block driver
//      bit[1]:tcon: warning, only print warning message
int lcd_tcon_init_setting_check(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
		struct tcon_core_reg_info_s *core_reg_info)
{
	char *ferr_str, *warn_str;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	ferr_str = kzalloc(PR_BUF_MAX, GFP_KERNEL);
	if (!ferr_str) {
		LCD_ERR(NULL, "tcon: setting_check fail for NOMEM\n");
		return 0;
	}
	warn_str = kzalloc(PR_BUF_MAX, GFP_KERNEL);
	if (!warn_str) {
		LCD_ERR(NULL, "tcon: setting_check fail for NOMEM\n");
		kfree(ferr_str);
		return 0;
	}

	if (lcd_tcon_conf->tcon_check)
		ret = lcd_tcon_conf->tcon_check(pdrv, ptiming, core_reg_info, ferr_str, warn_str);
	else
		ret = 0;

	if (ret) {
		pr_err("**************** lcd tcon setting check ****************\n");
		if (ret & 0x1) {
			pr_err("lcd: tcon: FATAL ERROR:\n"
				"%s\n", ferr_str);
		}
		if (ret & 0x2) {
			pr_err("lcd: tcon: WARNING:\n"
				"%s\n", warn_str);
		}
		pr_err("************** lcd tcon setting check end ****************\n");
	}
	kfree(ferr_str);
	kfree(warn_str);

	return ret;
}

unsigned int lcd_tcon_reg_read(struct aml_lcd_drv_s *pdrv, unsigned int addr)
{
	unsigned int val;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	if (addr < lcd_tcon_conf->top_reg_base) {
		if (lcd_tcon_conf->core_reg_width == 8)
			val = lcd_tcon_read_byte(pdrv, addr);
		else
			val = lcd_tcon_read(pdrv, addr);
	} else {
		val = lcd_tcon_read(pdrv, addr);
	}

	return val;
}

void lcd_tcon_reg_write(struct aml_lcd_drv_s *pdrv,
			unsigned int addr, unsigned int val)
{
	unsigned char temp;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (addr < lcd_tcon_conf->top_reg_base) {
		if (lcd_tcon_conf->core_reg_width == 8) {
			temp = (unsigned char)val;
			lcd_tcon_write_byte(pdrv, addr, temp);
		} else {
			lcd_tcon_write(pdrv, addr, val);
		}
	} else {
		lcd_tcon_write(pdrv, addr, val);
	}
}

#define PR_LINE_BUF_MAX    200
void lcd_tcon_reg_table_print(struct tcon_core_reg_info_s *core_reg_info)
{
	struct lcd_tcon_reg_block_info_s *reg_blk = NULL;
	int i, j, n, cnt, left, blk, offset;
	int ret, byte_start = 0, byte_len = 0, data_byte = 1;
	char *buf;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (!(core_reg_info && core_reg_info->table)) {
		LCD_ERR(NULL, "%s: reg_table is null\n", __func__);
		return;
	}

	buf = kcalloc(PR_LINE_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	LCDPR("%s:\n", __func__);
	if (core_reg_info->header->ext_header_size &&
			core_reg_info->ext_header.reg_blk_num) {
		data_byte = core_reg_info->header->data_byte_width;
		offset = 0;
		for (blk = 0; blk < core_reg_info->ext_header.reg_blk_num; blk++) {
			reg_blk = &core_reg_info->ext_header.reg_blk_info[blk];
			byte_start = reg_blk->start * data_byte;
			byte_len = reg_blk->len * data_byte;
			pr_info("block[%d]: (start=%#x, len=%d)\n",
				blk, byte_start, byte_len);
			for (i = 0; i < byte_len; i += 16) {
				n = snprintf(buf, PR_LINE_BUF_MAX, "%04x: ", byte_start + i);
				for (j = 0; j < 16; j++) {
					if ((i + j) >= byte_len)
						break;
					left = PR_LINE_BUF_MAX - n - 1;
					n += snprintf(buf + n, left, " %02x",
						core_reg_info->table[offset++]);
				}
				buf[n] = '\0';
				pr_info("%s\n", buf);
			}
		}
	} else {
		cnt = core_reg_info->table_size;
		for (i = 0; i < cnt; i += 16) {
			n = snprintf(buf, PR_LINE_BUF_MAX, "%04x: ", i);
			for (j = 0; j < 16; j++) {
				if ((i + j) >= cnt)
					break;
				left = PR_LINE_BUF_MAX - n - 1;
				n += snprintf(buf + n, left, " %02x",
					      core_reg_info->table[i + j]);
			}
			buf[n] = '\0';
			pr_info("%s\n", buf);
		}
	}
	kfree(buf);
}

int lcd_tcon_get_core_size(struct lcd_tcon_config_s *tcon_conf,
	struct lcd_tcon_init_block_header_s *header,
	struct lcd_tcon_init_block_ext_header_s *ext_header)
{
	struct lcd_tcon_reg_block_info_s *reg_blk = NULL;
	int table_size = 0, i;

	if (!tcon_conf || !header)
		return -1;

	table_size = tcon_conf->reg_table_len;
	if (ext_header && ext_header->reg_blk_num) {
		table_size = 0;
		for (i = 0; i < ext_header->reg_blk_num; i++) {
			reg_blk = &ext_header->reg_blk_info[i];
			table_size += reg_blk->len * header->data_byte_width;
		}
	}

	return table_size;
}

void lcd_tcon_core_info_print(struct aml_lcd_drv_s *pdrv,
		struct tcon_core_reg_info_s *core_reg_info)
{
	struct lcd_tcon_init_block_header_s *header;
	struct lcd_tcon_init_block_ext_header_s *ext_header;
	struct lcd_tcon_reg_block_info_s *reg_blk_info;
	int i = 0;

	if (!pdrv || !core_reg_info) {
		LCD_ERR(NULL, "%s: invalid parameter\n", __func__);
		return;
	}

	if (!core_reg_info->header) {
		LCD_ERR(NULL, "%s: header is null\n", __func__);
		return;
	}

	header = core_reg_info->header;
	ext_header = &core_reg_info->ext_header;

	LCDPR("core info:\n");
	LCDPR("  table      = 0x%lx\n", (unsigned long)core_reg_info->table);
	LCDPR("  table_size = %d\n", core_reg_info->table_size);
	LCDPR("header:\n");
	LCDPR("  crc32      = 0x%08x\n", header->crc32);
	LCDPR("  chipid     = %d\n", header->chipid);
	LCDPR("  block_size = %d\n", header->block_size);
	LCDPR("  block_type = 0x%x\n", header->block_type);
	LCDPR("  block_ctrl = 0x%x\n", header->block_ctrl);
	LCDPR("  byte_width = 0x%x\n", header->data_byte_width);
	LCDPR("  resolution = %dx%d\n", header->h_active, header->v_active);
	LCDPR("  name       = %s\n", header->name);
	LCDPR("  header_size = %d\n", header->header_size);
	LCDPR("  ext_header_size = %d\n", header->ext_header_size);
	if (header->ext_header_size) {
		LCDPR("extern header:\n");
		LCDPR("  framerate_range   = %d~%d\n",
			ext_header->framerate_min, ext_header->framerate_max);
		LCDPR("  reg_block_num     = %d\n", ext_header->reg_blk_num);
		for (i = 0; i < ext_header->reg_blk_num; i++) {
			reg_blk_info = &ext_header->reg_blk_info[i];
			LCDPR("  reg_block[%d]: start=%#-6x, len=%-4d(%#-5x)  (%#x~%#x)\n",
				i, reg_blk_info->start, reg_blk_info->len,
				reg_blk_info->len, reg_blk_info->start,
				(reg_blk_info->start + reg_blk_info->len - 1));
		}
	}
	if (core_reg_info->user_info)
		LCDPR("user info:\n%s\n", core_reg_info->user_info);
}

static int tcon_reg_is_skip(unsigned int reg)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_tcon_reg_skip_s *reg_skip = NULL;
	int i, is_skip = 0;

	if (!tcon_conf)
		return 0;

	for (i = 0; i < tcon_conf->reg_skip_tbl_len; i++) {
		reg_skip = &tcon_conf->reg_skip_tbl[i];
		if (reg >= reg_skip->start_reg &&
				reg <= reg_skip->end_reg) {
			is_skip = 1;
			break;
		}
	}

	return is_skip;
}

void lcd_tcon_reg_readback_print(struct aml_lcd_drv_s *pdrv)
{
	int i, j, n, cnt, offset, left;
	char *buf;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	buf = kcalloc(PR_LINE_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!buf)
		return;

	LCDPR("%s:\n", __func__);
	cnt = lcd_tcon_conf->reg_table_len;
	offset = lcd_tcon_conf->core_reg_start;
	if (lcd_tcon_conf->core_reg_width == 8) {
		for (i = offset; i < cnt; i += 16) {
			n = snprintf(buf, PR_LINE_BUF_MAX, "%04x: ", i);
			for (j = 0; j < 16; j++) {
				if ((i + j) >= cnt)
					break;
				left = PR_LINE_BUF_MAX - n - 1;
				n += snprintf(buf + n, left, " %02x",
					lcd_tcon_read_byte(pdrv, i + j));
			}
			buf[n] = '\0';
			pr_info("%s\n", buf);
		}
	} else {
		if (lcd_tcon_conf->reg_table_width == 32) {
			cnt /= 4;
			for (i = offset; i < cnt; i += 4) {
				n = snprintf(buf, PR_LINE_BUF_MAX, "%04x: ", i);
				for (j = 0; j < 4; j++) {
					if ((i + j) >= cnt)
						break;
					left = PR_LINE_BUF_MAX - n - 1;
					if (tcon_reg_is_skip(i + j)) {
						n += snprintf(buf + n, left, " xxxxxxxx");
					} else {
						n += snprintf(buf + n, left, " %08x",
							lcd_tcon_read(pdrv, i + j));
					}
				}
				buf[n] = '\0';
				pr_info("%s\n", buf);
			}
		} else {
			for (i = offset; i < cnt; i += 16) {
				n = snprintf(buf, PR_LINE_BUF_MAX, "%04x: ", i);
				for (j = 0; j < 16; j++) {
					if ((i + j) >= cnt)
						break;
					left = PR_LINE_BUF_MAX - n - 1;
					if (tcon_reg_is_skip(i + j)) {
						n += snprintf(buf + n, left, " xx");
					} else {
						n += snprintf(buf + n, left, " %02x",
							lcd_tcon_read(pdrv, i + j));
					}
				}
				buf[n] = '\0';
				pr_info("%s\n", buf);
			}
		}
	}

	kfree(buf);
}

unsigned int lcd_tcon_table_read(unsigned int addr)
{
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct tcon_core_reg_info_s *core_reg_info = NULL;
	unsigned char *table8;
	unsigned int *table32, size = 0, val = 0;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	core_reg_info = &local_cfg->cur_core_info;
	if (!core_reg_info || !core_reg_info->table) {
		LCD_ERR(NULL, "tcon cur_reg_table is null\n");
		return 0;
	}

	if (core_reg_info->header->ext_header_size &&
			core_reg_info->ext_header.reg_blk_num) {
		ret = lcd_tcon_get_table32_reg(core_reg_info, addr, &val);
		if (ret < 0) {
			LCD_ERR(NULL, "invalid tcon reg_table addr: 0x%04x\n", addr);
			return 0;
		}
	} else {
		if (lcd_tcon_conf->core_reg_width == 8)
			size = core_reg_info->table_size;
		else
			size = core_reg_info->table_size / 4;
		if (addr >= size) {
			LCD_ERR(NULL, "invalid tcon reg_table addr: 0x%04x\n", addr);
			return 0;
		}

		if (lcd_tcon_conf->core_reg_width == 8) {
			table8 = core_reg_info->table;
			val = table8[addr];
		} else {
			table32 = (unsigned int *)core_reg_info->table;
			val = table32[addr];
		}
	}

	return val;
}

unsigned int lcd_tcon_table_write(unsigned int addr, unsigned int val)
{
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct tcon_core_reg_info_s *core_reg_info = NULL;
	unsigned char *table8;
	unsigned int *table32, size = 0, read_val = 0;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return 0;

	core_reg_info = &local_cfg->cur_core_info;
	if (!core_reg_info || !core_reg_info->table) {
		LCD_ERR(NULL, "tcon cur_reg_table is null\n");
		return 0;
	}

	if (core_reg_info->header->ext_header_size &&
			core_reg_info->ext_header.reg_blk_num) {
		lcd_tcon_set_table32_reg(core_reg_info, addr, val);
	} else {
		if (lcd_tcon_conf->core_reg_width == 8)
			size = core_reg_info->table_size;
		else
			size = core_reg_info->table_size / 4;
		if (addr >= size) {
			LCD_ERR(NULL, "invalid tcon reg_table addr: 0x%04x\n", addr);
			return 0;
		}

		if (lcd_tcon_conf->core_reg_width == 8) {
			table8 = core_reg_info->table;
			table8[addr] = (unsigned char)(val & 0xff);
			read_val = table8[addr];
		} else {
			table32 = (unsigned int *)core_reg_info->table;
			table32[addr] = val;
			read_val = table32[addr];
		}
	}

	return read_val;
}

int lcd_tcon_core_reg_get(struct aml_lcd_drv_s *pdrv,
			  unsigned char *buf, unsigned int size)
{
	unsigned int reg_max = 0, offset = 0, val;
	int i, ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	if (size > lcd_tcon_conf->reg_table_len) {
		LCD_ERR(NULL, "%s: size 0x%x is not enough(0x%x)\n",
		       __func__, size, lcd_tcon_conf->reg_table_len);
		return -1;
	}

	offset = lcd_tcon_conf->core_reg_start;
	if (lcd_tcon_conf->core_reg_width == 8) {
		for (i = offset; i < size; i++)
			buf[i] = lcd_tcon_read_byte(pdrv, i);
	} else {
		reg_max = size / 4;
		for (i = offset; i < reg_max; i++) {
			val = lcd_tcon_read(pdrv, i);
			buf[i * 4] = val & 0xff;
			buf[i * 4 + 1] = (val >> 8) & 0xff;
			buf[i * 4 + 2] = (val >> 16) & 0xff;
			buf[i * 4 + 3] = (val >> 24) & 0xff;
		}
	}

	return 0;
}

void lcd_tcon_global_reset(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (lcd_tcon_conf->tcon_global_reset) {
		lcd_tcon_conf->tcon_global_reset(pdrv);
		LCDPR("reset tcon\n");
	}
}

int lcd_tcon_core_update(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	lcd_tcon_core_reg_set(pdrv, lcd_tcon_conf,
		&tcon_mm_table, &tcon_local_cfg.cur_core_info);

	return 0;
}

int lcd_tcon_reload_pre(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	unsigned long long local_time[2];
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	local_time[0] = sched_clock();
	tcon_fw->tcon_state &= ~TCON_FW_STATE_TCON_EN;
	pdrv->status &= ~LCD_STATUS_TCON_RDY;

	if (lcd_tcon_conf->tcon_disable)
		lcd_tcon_conf->tcon_disable(pdrv);
	if (pdrv->curr_dev->dev_cfg.timing.switch_type != LCD_VMODE_SWITCH_MIN_WO_TCON_RST) {
		if (lcd_tcon_conf->tcon_global_reset) {
			lcd_tcon_conf->tcon_global_reset(pdrv);
			LCDPR("reset tcon\n");
		}
	}

	local_time[1] = sched_clock();
	pdrv->proc_time.tcon_off_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return 0;
}

int lcd_tcon_reload(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	unsigned long long local_time[2];
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	local_time[0] = sched_clock();
	if (lcd_tcon_conf->tcon_top_init)
		lcd_tcon_conf->tcon_top_init(pdrv);
	if (lcd_tcon_conf->tcon_enable)
		lcd_tcon_conf->tcon_enable(pdrv);

	pdrv->status |= LCD_STATUS_TCON_RDY;
	tcon_fw->tcon_state |= TCON_FW_STATE_TCON_EN;

	local_time[1] = sched_clock();
	pdrv->proc_time.tcon_on_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return 0;
}

int lcd_tcon_enable(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	unsigned long long local_time[2];
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;

	local_time[0] = sched_clock();
	if (lcd_tcon_conf->tcon_enable)
		lcd_tcon_conf->tcon_enable(pdrv);

	pdrv->status |= LCD_STATUS_TCON_RDY;
	tcon_fw->tcon_state |= TCON_FW_STATE_TCON_EN;

	local_time[1] = sched_clock();
	pdrv->proc_time.tcon_on_time = lcd_do_div(local_time[1] - local_time[0], 1000);

	return 0;
}

void lcd_tcon_disable(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	unsigned long long local_time[2];
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	LCDPR("lcd_debug: %s\n", __func__);

	local_time[0] = sched_clock();

	tcon_fw->tcon_state &= ~TCON_FW_STATE_TCON_EN;
	pdrv->status &= ~LCD_STATUS_TCON_RDY;

	/* release data*/
	if (tcon_mm_table.version < 0xff)
		lcd_tcon_data_multi_reset(&tcon_mm_table);

	if (lcd_tcon_conf->tcon_disable)
		lcd_tcon_conf->tcon_disable(pdrv);
	if (lcd_tcon_conf->tcon_global_reset) {
		lcd_tcon_conf->tcon_global_reset(pdrv);
		LCDPR("reset tcon\n");
	}

	local_time[1] = sched_clock();
	pdrv->proc_time.tcon_off_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

int lcd_tcon_top_init(struct aml_lcd_drv_s *pdrv)
{
	int ret = lcd_tcon_valid_check();

	if (ret)
		return ret;

	ret = -1;
	if (lcd_tcon_conf->tcon_top_init)
		ret = lcd_tcon_conf->tcon_top_init(pdrv);

	return ret;
}

int lcd_tcon_lock_status_get(struct aml_lcd_drv_s *pdrv)
{
	int ret = lcd_tcon_valid_check();

	if (ret)
		return ret;

	ret = -1;
	if (lcd_tcon_conf->tcon_lock_get_status)
		ret = lcd_tcon_conf->tcon_lock_get_status(pdrv);

	return ret;
}

void lcd_tcon_dbg_check(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming)
{
	int ret;

	ret = lcd_tcon_init_setting_check(pdrv, ptiming, &tcon_local_cfg.cur_core_info);
	if (ret == 0)
		pr_info("lcd tcon setting check: PASS\n");
}

#ifdef TCON_DBG_TIME
static void lcd_tcon_time_sort_save(unsigned long long *table, unsigned long long data)
{
	int i, j;

	for (i = 9; i >= 0; i--) {
		if (data > table[i]) {
			for (j = 0; j < i; j++)
				table[j] = table[j + 1];
			table[i] = data;
			break;
		}
	}
}
#endif

/*vsync stage*/
static int lcd_tcon_data_multi_match_policy(struct aml_lcd_drv_s *pdrv, unsigned int frame_rate,
		struct tcon_data_multi_s *data_multi, struct tcon_data_list_s *data_list)
{
#ifdef CONFIG_AMLOGIC_BACKLIGHT
	struct aml_bl_drv_s *bldrv;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned int temp;
#endif

	switch (data_list->ctrl_method) {
	case LCD_TCON_DATA_CTRL_MULTI_VFREQ_DIRECT:
	case LCD_TCON_DATA_CTRL_MULTI_VFREQ_NOTIFY:
		if (frame_rate < data_list->multi.range.min ||
		    frame_rate > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_policy_exit;
		if (!data_multi)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
			snprintf(data_multi->dbg_str, 64, "vfreq %d-%d hit: %d",
				data_list->multi.range.min, data_list->multi.range.max, frame_rate);
		}
		break;
	case LCD_TCON_DATA_CTRL_MULTI_BL_LEVEL:
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bldrv = aml_bl_get_driver(pdrv->index);
		if (!bldrv)
			goto lcd_tcon_data_multi_match_policy_err_type;
		temp = bldrv->level;

		if (temp < data_list->multi.range.min || temp > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_policy_exit;
		if (!data_multi)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
			snprintf(data_multi->dbg_str, 64, "bl_level %d-%d hit: %d",
				data_list->multi.range.min, data_list->multi.range.max, temp);
		}
#endif
		break;
	case LCD_TCON_DATA_CTRL_MULTI_BL_PWM_DUTY:
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bldrv = aml_bl_get_driver(pdrv->index);
		if (!bldrv)
			goto lcd_tcon_data_multi_match_policy_err_type;

		switch (bldrv->bconf.method) {
		case BL_CTRL_PWM:
			bl_pwm = bldrv->bconf.bl_pwm;
			break;
		case BL_CTRL_PWM_COMBO:
			if (data_list->ctrl_data) {
				if (data_list->ctrl_data[0])
					bl_pwm = bldrv->bconf.bl_pwm_combo0;
				else
					bl_pwm = bldrv->bconf.bl_pwm_combo1;
			}
			break;
		case BL_CTRL_PWM_ARRAY:
			if (data_list->ctrl_data[0] < 4)
				bl_pwm = bldrv->bconf.bl_pwm_array[data_list->ctrl_data[0]];
			break;
		default:
			break;
		}
		if (!bl_pwm)
			goto lcd_tcon_data_multi_match_policy_err_type;

		temp = bl_pwm->pwm_duty;
		if (temp < data_list->multi.range.min || temp > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_policy_exit;
		if (!data_multi)
			break;
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
			snprintf(data_multi->dbg_str, 64, "bl_pwm[%d] duty %d-%d hit: %d",
				bl_pwm->index, data_list->multi.range.min,
				data_list->multi.range.max, temp);
		}
#endif
		break;
	case LCD_TCON_DATA_CTRL_DEFAULT:
		return 1;
	default:
		return -1;
	}

	return 0;

lcd_tcon_data_multi_match_policy_exit:
	return -1;

#ifdef CONFIG_AMLOGIC_BACKLIGHT
lcd_tcon_data_multi_match_policy_err_type:
	LCD_ERR(NULL, "%s: %s type invalid\n", __func__, data_list->block_name);
	return -1;
#endif
}

static int lcd_tcon_data_multi_match_init(struct aml_lcd_drv_s *pdrv,
		struct tcon_data_list_s *data_list,
		struct lcd_tcon_data_part_ctrl_s *ctrl_part, unsigned char *p)
{
	unsigned int data_byte, data_cnt, frame_rate;
	struct lcd_detail_timing_s *pt;
#ifdef CONFIG_AMLOGIC_BACKLIGHT
	struct aml_bl_drv_s *bldrv;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned int temp;
#endif
	unsigned int i, j, k = 0;

	if (!ctrl_part)
		return -1;
	if (!(ctrl_part->ctrl_data_flag & LCD_TCON_DATA_CTRL_FLAG_MULTI))
		return -1;

	data_byte = ctrl_part->data_byte_width;
	data_cnt = ctrl_part->data_cnt;
	data_list->ctrl_method = LCD_TCON_DATA_CTRL_MULTI_MAX;
	frame_rate = pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate;

	data_list->multi.range.min = 0;
	data_list->multi.range.max = 0;
	data_list->ctrl_data_cnt = 0;
	kfree(data_list->ctrl_data);
	data_list->ctrl_data = NULL;

	switch (ctrl_part->ctrl_method) {
	case LCD_TCON_DATA_CTRL_MULTI_VFREQ_DIRECT:
		if (data_cnt != 2)
			goto lcd_tcon_data_multi_match_init_err_data_cnt;

		data_list->ctrl_method = ctrl_part->ctrl_method;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.min |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.max |= (p[k + j] << (j * 8));

		if (frame_rate < data_list->multi.range.min ||
		    frame_rate > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_init_not_match;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: vfreq %d-%d hit: %d\n",
				__func__, data_list->multi.range.min,
				data_list->multi.range.max, frame_rate);
		}
		break;
	case LCD_TCON_DATA_CTRL_MULTI_VFREQ_NOTIFY:
		if (data_cnt <= 2)
			goto lcd_tcon_data_multi_match_init_err_data_cnt;

		data_list->ctrl_method = ctrl_part->ctrl_method;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.min |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.max |= (p[k + j] << (j * 8));

		data_list->ctrl_data_cnt = data_cnt - 2;
		if (data_list->ctrl_data_cnt == 0)
			break;
		data_list->ctrl_data =
			kcalloc(data_list->ctrl_data_cnt, sizeof(unsigned int), GFP_KERNEL);
		if (!data_list->ctrl_data)
			goto lcd_tcon_data_multi_match_init_err_malloc;
		for (i = 0; i < data_list->ctrl_data_cnt; i++) {
			k += data_byte;
			for (j = 0; j < data_byte; j++)
				data_list->ctrl_data[i] |= (p[k + j] << (j * 8));
		}

		if (frame_rate < data_list->multi.range.min ||
		    frame_rate > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_init_not_match;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: vfreq %d-%d hit: %d\n",
				__func__, data_list->multi.range.min,
				data_list->multi.range.max, frame_rate);
		}
		break;
	case LCD_TCON_DATA_CTRL_MULTI_RESOLUTION:
		// for more possibility like resolution && fr_range
		if (data_cnt < 2)
			goto lcd_tcon_data_multi_match_init_err_data_cnt;

		data_list->ctrl_method = ctrl_part->ctrl_method;
		for (j = 0; j < data_byte; j++)
			data_list->multi.resolution.hsize |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.resolution.vsize |= (p[k + j] << (j * 8));

		pt = pdrv->curr_dev->dev_cfg.timing.base_timing;
		if (pt->h_active != data_list->multi.resolution.hsize ||
		    pt->v_active != data_list->multi.resolution.vsize)
			goto lcd_tcon_data_multi_match_init_not_match;
		if (data_cnt == 2) {//matched over only resolution
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("%s: resolution %dx%d hit\n",
					__func__, pt->h_active, pt->v_active);
			break;
		}
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.min |= (p[k + j] << (j * 8));
		if (data_cnt == 3) { // display mode(base base_timing), eg:3840x2160p120hz
			data_list->multi.range.max = data_list->multi.range.min;
			if (pt->frame_rate != data_list->multi.range.max)
				goto lcd_tcon_data_multi_match_init_not_match;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("%s: disp_mode %dx%dp%dhz hit\n",
					__func__, pt->h_active, pt->v_active, pt->frame_rate);
			break;
		}
		// next regard as frame rate range, and carry other information
		if (data_cnt < 4)
			goto lcd_tcon_data_multi_match_init_not_match;

		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.max |= (p[k + j] << (j * 8));

		frame_rate = pt->frame_rate;
		if (frame_rate < data_list->multi.range.min ||
		    frame_rate > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_init_not_match;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: disp_mode %dx%dp[%d~%d]hz hit\n",
				__func__, pt->h_active, pt->v_active,
				data_list->multi.range.min, data_list->multi.range.max);

		break;
	case LCD_TCON_DATA_CTRL_MULTI_BL_LEVEL:
		if (data_cnt != 2)
			goto lcd_tcon_data_multi_match_init_err_data_cnt;

		data_list->ctrl_method = ctrl_part->ctrl_method;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.min |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.max |= (p[k + j] << (j * 8));

#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bldrv = aml_bl_get_driver(pdrv->index);
		if (!bldrv)
			goto lcd_tcon_data_multi_match_init_not_match;
		temp = bldrv->level;

		if (temp < data_list->multi.range.min || temp > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_init_not_match;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: bl_level %d-%d hit: %d\n",
				__func__, data_list->multi.range.min,
				data_list->multi.range.max, temp);
		}
#endif
		break;
	case LCD_TCON_DATA_CTRL_MULTI_BL_PWM_DUTY:
		if (data_cnt != 3)
			goto lcd_tcon_data_multi_match_init_err_data_cnt;

		data_list->ctrl_method = ctrl_part->ctrl_method;
		//pwm combo index
		data_list->ctrl_data_cnt = 1;
		data_list->ctrl_data = kzalloc(sizeof(unsigned int), GFP_KERNEL);
		if (!data_list->ctrl_data)
			goto lcd_tcon_data_multi_match_init_err_malloc;
		for (j = 0; j < data_byte; j++)
			data_list->ctrl_data[0] |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.min |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			data_list->multi.range.max |= (p[k + j] << (j * 8));

#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bldrv = aml_bl_get_driver(pdrv->index);
		if (!bldrv)
			goto lcd_tcon_data_multi_match_init_not_match;

		switch (bldrv->bconf.method) {
		case BL_CTRL_PWM:
			bl_pwm = bldrv->bconf.bl_pwm;
			break;
		case BL_CTRL_PWM_COMBO:
			if (data_list->ctrl_data[0])
				bl_pwm = bldrv->bconf.bl_pwm_combo0;
			else
				bl_pwm = bldrv->bconf.bl_pwm_combo1;
			break;
		case BL_CTRL_PWM_ARRAY:
			if (data_list->ctrl_data[0] < 4)
				bl_pwm = bldrv->bconf.bl_pwm_array[data_list->ctrl_data[0]];
			break;
		default:
			break;
		}
		if (!bl_pwm)
			goto lcd_tcon_data_multi_match_init_not_match;

		temp = bl_pwm->pwm_duty;
		if (temp < data_list->multi.range.min || temp > data_list->multi.range.max)
			goto lcd_tcon_data_multi_match_init_not_match;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: bl_pwm[%d] duty %d-%d hit: %d\n",
				__func__, bl_pwm->index, data_list->multi.range.min,
				data_list->multi.range.max, temp);
		}
#endif
		break;
	case LCD_TCON_DATA_CTRL_DEFAULT:
		return 1;
	default:
		return -1;
	}

	return 0;

lcd_tcon_data_multi_match_init_not_match:
	return -1;

lcd_tcon_data_multi_match_init_err_data_cnt:
	LCD_ERR(NULL, "%s: ctrl_part %s data_cnt %d error\n",
		__func__, ctrl_part->name, data_cnt);
	return -1;

lcd_tcon_data_multi_match_init_err_malloc:
	LCD_ERR(NULL, "%s: ctrl_part %s malloc error\n", __func__, ctrl_part->name);
	return -1;
}

/*tcon init stage*/
int lcd_tcon_data_multi_init_check(struct aml_lcd_drv_s *pdrv, unsigned short block_type,
		struct lcd_tcon_data_part_ctrl_s *ctrl_part, unsigned char *p,
		unsigned int data_index)
{
	unsigned int frame_rate;
#ifdef CONFIG_AMLOGIC_BACKLIGHT
	struct aml_bl_drv_s *bldrv;
	struct bl_pwm_config_s *bl_pwm = NULL;
	unsigned int data = 0, temp;
#endif
	unsigned int data_byte, data_cnt, min = 0, max = 0, hsize = 0, vsize = 0;
	unsigned int j, k = 0;
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();

	if (!ctrl_part)
		return -1;

	if (mm_table && mm_table->effect_id < mm_table->block_cnt) {
		lcd_tcon_data_multi_current_set(&tcon_mm_table, block_type, data_index);
		return 0;
	}
	data_byte = ctrl_part->data_byte_width;
	data_cnt = ctrl_part->data_cnt;
	frame_rate = pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate;

	switch (ctrl_part->ctrl_method) {
	case LCD_TCON_DATA_CTRL_MULTI_VFREQ_DIRECT:
		if (data_cnt != 2)
			goto lcd_tcon_data_multi_init_check_err_data_cnt;

		for (j = 0; j < data_byte; j++)
			min |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			max |= (p[k + j] << (j * 8));
		if (frame_rate < min || frame_rate > max)
			goto lcd_tcon_data_multi_init_check_exit;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: vfreq %d-%d hit: %d\n", __func__, min, max, frame_rate);
		break;
	case LCD_TCON_DATA_CTRL_MULTI_RESOLUTION:
		if (data_cnt < 2)
			goto lcd_tcon_data_multi_init_check_err_data_cnt;

		for (j = 0; j < data_byte; j++)
			hsize |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			vsize |= (p[k + j] << (j * 8));

		if (pdrv->curr_dev->dev_cfg.timing.base_timing->h_active != hsize ||
		    pdrv->curr_dev->dev_cfg.timing.base_timing->v_active != vsize)
			goto lcd_tcon_data_multi_init_check_exit;

		if (data_cnt == 2) {
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("%s: resolution %dx%d hit\n", __func__, hsize, vsize);
			break;
		}
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			min |= (p[k + j] << (j * 8));
		if (data_cnt == 3) { //display mode (base bas_timing) eg:3840x2160p120hz
			if (min != pdrv->curr_dev->dev_cfg.timing.base_timing->frame_rate)
				goto lcd_tcon_data_multi_init_check_exit;
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
				LCDPR("%s: disp_mode %dx%dp%dhz hit\n",
					__func__, hsize, vsize, min);
			break;
		}

		k += data_byte;
		for (j = 0; j < data_byte; j++)
			max |= (p[k + j] << (j * 8));

		frame_rate = pdrv->curr_dev->dev_cfg.timing.base_timing->frame_rate;
		if (frame_rate < min || frame_rate > max)
			goto lcd_tcon_data_multi_init_check_exit;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: disp_mode %dx%dp[%d~%d]hz hit\n",
					__func__, hsize, vsize, min, max);
		break;
	case LCD_TCON_DATA_CTRL_MULTI_BL_LEVEL:
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bldrv = aml_bl_get_driver(pdrv->index);
		if (!bldrv)
			goto lcd_tcon_data_multi_init_check_err_type;
		temp = bldrv->level;

		if (data_cnt != 2)
			goto lcd_tcon_data_multi_init_check_err_data_cnt;
		for (j = 0; j < data_byte; j++)
			min |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			max |= (p[k + j] << (j * 8));
		if (temp < min || temp > max)
			goto lcd_tcon_data_multi_init_check_exit;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: bl_level %d-%d hit: %d\n", __func__, min, max, temp);
#endif
		break;
	case LCD_TCON_DATA_CTRL_MULTI_BL_PWM_DUTY:
#ifdef CONFIG_AMLOGIC_BACKLIGHT
		bldrv = aml_bl_get_driver(pdrv->index);
		if (!bldrv)
			goto lcd_tcon_data_multi_init_check_err_type;

		if (data_cnt != 3)
			goto lcd_tcon_data_multi_init_check_err_data_cnt;
		for (j = 0; j < data_byte; j++)
			data |= (p[k + j] << (j * 8)); //pwm_index
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			min |= (p[k + j] << (j * 8));
		k += data_byte;
		for (j = 0; j < data_byte; j++)
			max |= (p[k + j] << (j * 8));

		switch (bldrv->bconf.method) {
		case BL_CTRL_PWM:
			bl_pwm = bldrv->bconf.bl_pwm;
			break;
		case BL_CTRL_PWM_COMBO:
			if (data == 0)
				bl_pwm = bldrv->bconf.bl_pwm_combo0;
			else
				bl_pwm = bldrv->bconf.bl_pwm_combo1;
			break;
		case BL_CTRL_PWM_ARRAY:
			if (data < 4)
				bl_pwm = bldrv->bconf.bl_pwm_array[data];
			break;
		default:
			break;
		}
		if (!bl_pwm)
			goto lcd_tcon_data_multi_init_check_err_type;

		temp = bl_pwm->pwm_duty;
		if (temp < min || temp > max)
			goto lcd_tcon_data_multi_init_check_exit;

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: bl_pwm[%d] duty %d-%d hit: %d\n",
				__func__, bl_pwm->index, min, max, temp);
		}
#endif
		break;
	case LCD_TCON_DATA_CTRL_DEFAULT:
		return 1;
	default:
		return -1;
	}

	lcd_tcon_data_multi_current_set(&tcon_mm_table, block_type, data_index);
	return 0;

lcd_tcon_data_multi_init_check_exit:
	return -1;

lcd_tcon_data_multi_init_check_err_data_cnt:
	LCD_ERR(NULL, "%s: ctrl_part %s data_cnt error\n", __func__, ctrl_part->name);
	return -1;

#ifdef CONFIG_AMLOGIC_BACKLIGHT
lcd_tcon_data_multi_init_check_err_type:
	LCD_ERR(NULL, "%s: ctrl_part %s type invalid\n", __func__, ctrl_part->name);
	return -1;
#endif
}

static int lcd_tcon_data_multi_list_init(struct aml_lcd_drv_s *pdrv,
		struct tcon_data_list_s *data_list)
{
	struct lcd_tcon_data_block_header_s *block_header;
	struct lcd_tcon_data_block_ext_header_s *ext_header;
	struct lcd_tcon_data_part_ctrl_s *ctrl_part;
	unsigned char *data_buf, *p, *part_pos, part_type, part_mapping_byte;
	unsigned int size = 0, data_offset, offset, i, j;
	unsigned short part_cnt;
	unsigned int part_offset, part_start_offset;
	int ret;

	data_buf = data_list->block_vaddr;
	block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
	p = data_buf + block_header->header_size;
	ext_header = (struct lcd_tcon_data_block_ext_header_s *)p;
	part_cnt = ext_header->part_cnt;
	part_mapping_byte = ext_header->part_mapping_byte;

	part_pos = p + LCD_TCON_DATA_BLOCK_EXT_HEADER_SIZE_PRE;
	part_start_offset = block_header->header_size + block_header->ext_header_size;
	for (i = 0; i < part_cnt; i++) {
		p = part_pos + i * part_mapping_byte;
		part_offset = 0;
		for (j = 0; j < part_mapping_byte; j++)
			part_offset |= (p[j] << j * 8);
		data_offset = part_offset + part_start_offset;
		p = data_buf + data_offset;
		part_type = p[LCD_TCON_DATA_PART_NAME_SIZE + 3];

		switch (part_type) {
		case LCD_TCON_DATA_PART_TYPE_CONTROL:
			offset = LCD_TCON_DATA_PART_CTRL_SIZE_PRE;
			ctrl_part = (struct lcd_tcon_data_part_ctrl_s *)p;
			size = offset + (ctrl_part->data_cnt * ctrl_part->data_byte_width);
			if (!(ctrl_part->ctrl_data_flag & LCD_TCON_DATA_CTRL_FLAG_MULTI))
				break;
			if ((size + data_offset) > block_header->block_size)
				return -1;
			ret = lcd_tcon_data_multi_match_init(pdrv, data_list,
					ctrl_part, (p + offset));
			return ret;
		default:
			break;
		}
	}

	return -1;
}

/* tcon init stage */
static void lcd_tcon_data_multi_current_set(struct tcon_mem_map_table_s *mm_table,
		unsigned short block_type, unsigned int index)
{
	struct tcon_data_multi_s *data_multi = NULL;
	struct tcon_data_list_s *temp_list;
	int i;

	if (!mm_table || !mm_table->data_multi)
		return;
	if (mm_table->data_multi_cnt == 0)
		return;

	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		data_multi = &mm_table->data_multi[i];
		if (data_multi->block_type != block_type)
			continue;
		temp_list = data_multi->list_header;
		while (temp_list) {
			if (temp_list->id == index) {
				data_multi->list_cur = temp_list;
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("%s: multi[%d]: type=0x%x, id=%d, %s\n",
						__func__, i,
						data_multi->block_type,
						data_multi->list_cur->id,
						data_multi->list_cur->block_name);
				}
				return;
			}
			temp_list = temp_list->next;
		}
	}
}

/* for tcon multi lut update bypass debug */
void lcd_tcon_data_multi_bypass_set(struct tcon_mem_map_table_s *mm_table,
				    unsigned int block_type, int flag)
{
	struct tcon_data_multi_s *data_multi = NULL;
	int i;

	if (!mm_table || !mm_table->data_multi)
		return;
	if (mm_table->data_multi_cnt == 0)
		return;

	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		data_multi = &mm_table->data_multi[i];
		if (data_multi->block_type == block_type) {
			data_multi->bypass_flag = flag;
			LCDPR("tcon multi[%d]: block_type=0x%x, bypass: %d\n",
			      i, block_type, flag);
			return;
		}
	}

	LCD_ERR(NULL, "tcon multi[%d]: block_type=0x%x invalid\n", i, block_type);
}

/*vsync stage*/
static int lcd_tcon_data_multi_update(struct aml_lcd_drv_s *pdrv,
				      struct tcon_mem_map_table_s *mm_table)
{
	struct tcon_data_multi_s *data_multi = NULL;
	struct tcon_data_list_s *temp_list, *match_list;
	unsigned int acc_data[2], frame_rate;
#ifdef TCON_DBG_TIME
	unsigned long long local_time[3];
	unsigned int temp, line_cnt;
#endif
	int i, lut_hit = 0, ret = 0;

	if (!mm_table || !mm_table->data_multi)
		return 0;
	if (mm_table->data_multi_cnt == 0)
		return 0;

#ifdef TCON_DBG_TIME
	if (lcd_debug_print_flag & LCD_DBG_PR_TEST) {
		local_time[0] = sched_clock();
		line_cnt = lcd_get_encl_line_cnt(pdrv);
		lcd_tcon_dbg_vsync_time_save(local_time[0], line_cnt, 0);
	}
#endif

	if (pdrv->sw_vrr.en)
		frame_rate = lcd_get_sw_vrr_target_fr(0) * 10;//1000 multi
	else
		frame_rate = vout_frame_rate_measure(1); //1000 multi

	if (frame_rate == 0) {
		frame_rate = pdrv->curr_dev->dev_cfg.timing.act_timing.frame_rate;
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR)
			LCD_ERR(NULL, "%s: vout fr_msr 0, use sw fr %d\n", __func__, frame_rate);
	} else {
		frame_rate /= 1000;
	}

#ifdef TCON_DBG_TIME
	if (lcd_debug_print_flag & LCD_DBG_PR_TEST) {
		local_time[1] = sched_clock();
		line_cnt = lcd_get_encl_line_cnt(pdrv);
		lcd_tcon_dbg_vsync_time_save(local_time[1] - local_time[0], line_cnt, 1);
	}
#endif

	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		data_multi = &mm_table->data_multi[i];

		/* bypass LCD_TCON_DATA_BLOCK_TYPE_BASIC_INIT for multi lut switch */
		if (is_block_type_basic_init(data_multi->block_type))
			continue;
		/* bypass_flag for debug */
		if (data_multi->bypass_flag)
			continue;

		/* step1: check current list first, for threshold overlap*/
		temp_list = data_multi->list_cur;
		if (temp_list) {
			ret = lcd_tcon_data_multi_match_policy(pdrv, frame_rate,
					data_multi, temp_list);
			if (ret == 0) //current range, no need update
				continue;
		}

		/* step2: traversing list*/
		temp_list = data_multi->list_header;
		match_list = NULL;
		while (temp_list) {
			lut_hit++;
			ret = lcd_tcon_data_multi_match_policy(pdrv, frame_rate,
					data_multi, temp_list);
			if (ret == 0) {
				match_list = temp_list;
				break;
			}
			temp_list = temp_list->next;
		}

		if (!match_list) //no target list, no need update
			continue;
		if (data_multi->list_cur && data_multi->list_cur->id == match_list->id)
			continue; //same list, no need update

		if (data_multi->block_type == LCD_TCON_DATA_BLOCK_TYPE_ACC_LUT &&
		    match_list->ctrl_method == LCD_TCON_DATA_CTRL_MULTI_VFREQ_NOTIFY) {
			acc_data[0] = pdrv->index;
			if (match_list->ctrl_data)
				acc_data[1] = match_list->ctrl_data[0];
			else
				acc_data[1] = 0xff; //default gamma lut
			aml_lcd_atomic_notifier_call_chain(LCD_EVENT_GAMMA_UPDATE,
					(void *)acc_data);
			data_multi->list_cur = match_list;
		} else {
			ret = lcd_tcon_data_common_parse_set(pdrv, match_list->block_vaddr,
					match_list->block_paddr, 0, match_list->id);
			if (ret == 0)
				data_multi->list_cur = match_list;
		}
		if (lcd_debug_print_flag & LCD_DBG_PR_ISR) {
			LCDPR("%s: multi[%d]: type=0x%x, %s: id=%d, %s\n",
			      __func__, i,
			      data_multi->block_type,
			      data_multi->dbg_str,
			      match_list->id,
			      match_list->block_name);
		}
	}
#ifdef TCON_DBG_TIME
	if (lcd_debug_print_flag & LCD_DBG_PR_TEST) {
		local_time[2] = sched_clock();
		line_cnt = lcd_get_encl_line_cnt(pdrv);
		temp = (0xf << 12) | (lut_hit << 4) | 2;
		lcd_tcon_dbg_vsync_time_save(local_time[2] - local_time[0], line_cnt, temp);
		lcd_tcon_time_sort_save(mm_table->vsync_time, (local_time[2] - local_time[0]));
	}
#endif

	return ret;
}

void lcd_tcon_vsync_isr(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_fw_s *tcon_fw = aml_lcd_tcon_get_fw();
	struct tcon_pdf_s *tcon_pdf = lcd_tcon_get_pdf();
	unsigned long long local_time[2];
	unsigned long flags = 0, flags2 = 0;

	if (!lcd_tcon_conf || lcd_tcon_conf->tcon_valid == 0)
		return;
	if (((pdrv->status & LCD_STATUS_IF_ON) == 0) ||
	    ((pdrv->status & LCD_STATUS_TCON_RDY) == 0))
		return;
	if (pdrv->tcon_isr_bypass)
		return;

	local_time[0] = sched_clock();

	spin_lock_irqsave(&pdrv->sw_vrr.set_lock, flags2);

	if (tcon_local_cfg.ip27.valid)
		tcon_ip27_vsync_proc(pdrv, &tcon_local_cfg.ip27);

	if (tcon_mm_table.version < 0xff) {
		if (tcon_mm_table.multi_lut_update) {
			spin_lock_irqsave(&tcon_local_cfg.multi_list_lock, flags);
			lcd_tcon_data_multi_update(pdrv, &tcon_mm_table);
			spin_unlock_irqrestore(&tcon_local_cfg.multi_list_lock, flags);
		}
	}

	if (!pdrv->sw_vrr.en) {
		if (lcd_tcon_conf->lut_dma_ops && lcd_tcon_conf->lut_dma_ops->update)
			lcd_tcon_conf->lut_dma_ops->update(pdrv, lcd_tcon_conf->lut_dma_ops);
	} else {
		if (pdrv->sw_vrr.dma_dly > 0)
			pdrv->sw_vrr.dma_dly--;
		else if (lcd_tcon_conf->lut_dma_ops && lcd_tcon_conf->lut_dma_ops->update)
			lcd_tcon_conf->lut_dma_ops->update(pdrv, lcd_tcon_conf->lut_dma_ops);
	}
	spin_unlock_irqrestore(&pdrv->sw_vrr.set_lock, flags2);

	if (tcon_pdf->vs_handler)
		tcon_pdf->vs_handler(tcon_pdf);
	else if (pdrv->curr_dev->dev_cfg.customer_sw_pdf)
		lcd_swpdf_vs_handle();

	lcd_tcon_rdma_vs_handler(pdrv);

	if (tcon_fw->vsync_isr)
		tcon_fw->vsync_isr(tcon_fw);

	local_time[1] = sched_clock();
	pdrv->proc_time.tcon_vs_isr_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

/* **********************************
 * tcon config
 * **********************************
 */
static inline void lcd_tcon_data_list_add(struct tcon_data_multi_s *data_multi,
		struct tcon_data_list_s *data_list)
{
	struct tcon_data_list_s *temp_list;

	if (!data_multi || !data_list)
		return;

	//multi list add
	if (!data_multi->list_header) { /* new list */
		data_multi->list_header = data_list;
	} else {
		temp_list = data_multi->list_header;
		if (temp_list->id == data_list->id) {
			data_multi->list_header = data_list;
			data_multi->list_remove = temp_list;
			goto lcd_tcon_data_list_add_end;
		}
		while (temp_list->next) {
			if (temp_list->next->id == data_list->id) {
				temp_list->next = data_list;
				data_multi->list_remove = temp_list->next;
				goto lcd_tcon_data_list_add_end;
			}
			temp_list = temp_list->next;
		}
		temp_list->next = data_list;
	}
	data_multi->list_cnt++;

lcd_tcon_data_list_add_end:
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: block[%d]: %s, multi_id: %d\n",
		      __func__, data_list->id, data_list->block_name, data_multi->id);
	}
}

static inline void lcd_tcon_data_list_remove(struct tcon_data_multi_s *data_multi)
{
	struct tcon_data_list_s *cur_list, *next_list;

	if (!data_multi)
		return;

	/* add to exist list */
	cur_list = data_multi->list_header;
	while (cur_list) {
		next_list = cur_list->next;
		kfree(cur_list->ctrl_data);
		kfree(cur_list);
		cur_list = next_list;
	}

	data_multi->block_type = LCD_TCON_DATA_BLOCK_TYPE_MAX;
	data_multi->list_cnt = 0;
	data_multi->list_header = NULL;
	data_multi->list_cur = NULL;
}

static int lcd_tcon_data_multi_add(struct aml_lcd_drv_s *pdrv,
				   struct tcon_mem_map_table_s *mm_table,
				   struct lcd_tcon_data_block_header_s *block_header,
				   unsigned int index)
{
	struct tcon_data_multi_s *data_multi = NULL;
	struct tcon_data_list_s *data_list;
	unsigned long flags = 0;
	int i, list_match;

	if (!mm_table->data_multi) {
		LCD_ERR(NULL, "%s: data_multi is null\n", __func__);
		return -1;
	}
	if (mm_table->data_multi_cnt > 0) {
		if ((mm_table->data_multi_cnt + 1) >= mm_table->block_cnt) {
			LCD_ERR(NULL, "%s: multi block %s invalid\n",
			       __func__, block_header->name);
			return -1;
		}
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s: block[%d]: %s\n", __func__, index, block_header->name);

	/* create list */
	data_list = kzalloc(sizeof(*data_list), GFP_KERNEL);
	if (!data_list)
		return -1;
	data_list->block_vaddr = mm_table->data_mem_vaddr[index];
	data_list->block_paddr = mm_table->data_mem_paddr[index];
	data_list->next = NULL;
	data_list->id = index;
	data_list->block_name = block_header->name;
	list_match = lcd_tcon_data_multi_list_init(pdrv, data_list);

	for (i = 0; i < mm_table->data_multi_cnt; i++) {
		if (mm_table->data_multi[i].block_type == block_header->block_type) {
			data_multi = &mm_table->data_multi[i];
			break;
		}
	}

	spin_lock_irqsave(&tcon_local_cfg.multi_list_lock, flags);
	if (!data_multi) { /* create new list */
		data_multi = &mm_table->data_multi[mm_table->data_multi_cnt];
		data_multi->id = mm_table->data_multi_cnt;
		data_multi->block_type = block_header->block_type;
		data_multi->list_header = NULL;
		data_multi->list_cur = NULL;
		data_multi->list_remove = NULL;
		data_multi->list_cnt = 0;
		mm_table->data_multi_cnt++;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: new multi[%d]: type=0x%x, data_multi_cnt=%d\n",
			      __func__, data_multi->id, data_multi->block_type,
			      mm_table->data_multi_cnt);
		}
	}
	lcd_tcon_data_list_add(data_multi, data_list);

	if (list_match == 0) {
		data_multi->list_cur = data_list;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: multi[%d]: type=0x%x, %s: block[%d], %s\n",
				__func__, data_multi->id,
				data_multi->block_type,
				data_multi->dbg_str,
				data_multi->list_cur->id,
				data_multi->list_cur->block_name);
		}
	}

	spin_unlock_irqrestore(&tcon_local_cfg.multi_list_lock, flags);

	kfree(data_multi->list_remove);
	data_multi->list_remove = NULL;

	return 0;
}

static int lcd_tcon_data_multi_remvoe(struct tcon_mem_map_table_s *mm_table)
{
	unsigned long flags = 0;
	int i;

	if (!mm_table->data_multi)
		return 0;
	if (mm_table->data_multi_cnt == 0)
		return 0;

	spin_lock_irqsave(&tcon_local_cfg.multi_list_lock, flags);
	for (i = 0; i < mm_table->data_multi_cnt; i++)
		lcd_tcon_data_list_remove(&mm_table->data_multi[i]);
	spin_unlock_irqrestore(&tcon_local_cfg.multi_list_lock, flags);

	return 0;
}

static int lcd_tcon_data_multi_reset(struct tcon_mem_map_table_s *mm_table)
{
	unsigned long flags = 0;
	int i;

	if (!mm_table->data_multi)
		return 0;
	if (mm_table->data_multi_cnt == 0)
		return 0;

	spin_lock_irqsave(&tcon_local_cfg.multi_list_lock, flags);
	for (i = 0; i < mm_table->data_multi_cnt; i++)
		mm_table->data_multi[i].list_cur = NULL;
	spin_unlock_irqrestore(&tcon_local_cfg.multi_list_lock, flags);

	return 0;
}

static void lcd_tcon_init_table_pre_proc(struct tcon_core_reg_info_s *core_reg_info)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned int reg = 0, val, bit, len, paddr, i = 0;

	if (!core_reg_info || !tcon_rmem)
		return;

	if (tcon_conf && tcon_conf->init_pre_setting) {
		while (1) {
			reg = tcon_conf->init_pre_setting[i].reg;
			if (reg == REG_LCD_TCON_MAX)
				break;
			val = tcon_conf->init_pre_setting[i].val;
			bit = tcon_conf->init_pre_setting[i].bit;
			len = tcon_conf->init_pre_setting[i].len;
			lcd_tcon_setb_table32_reg(core_reg_info, reg, val, bit, len);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s: reg: 0x%08x[%d:%d] = 0x%x\n",
					__func__, reg, (bit + len - 1), bit, val);
			}
			i++;
		}
	}

	//update axi paddr
	if (tcon_rmem->flag == 0 || !tcon_rmem->axi_rmem || !tcon_rmem->axi_reg) {
		LCDPR("%s: invalid axi_rmem\n", __func__);
		return;
	}
	for (i = 0; i < tcon_rmem->axi_bank; i++) {
		reg = tcon_rmem->axi_reg[i];
		paddr = tcon_rmem->axi_rmem[i].mem_paddr;
		lcd_tcon_set_table32_reg(core_reg_info, reg, paddr);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: axi[%d] reg: 0x%08x, paddr: 0x%08x\n",
				__func__, i, reg, paddr);
		}
	}
}

static void lcd_tcon_init_table_post_proc(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	unsigned int reg = 0, val, bit, len, i = 0;

	if (tcon_conf && tcon_conf->init_post_setting) {
		while (1) {
			reg = tcon_conf->init_post_setting[i].reg;
			if (reg == REG_LCD_TCON_MAX)
				break;
			val = tcon_conf->init_post_setting[i].val;
			bit = tcon_conf->init_post_setting[i].bit;
			len = tcon_conf->init_post_setting[i].len;
			lcd_tcon_setb(pdrv, reg, val, bit, len);
			if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
				LCDPR("%s: reg: 0x%08x[%d:%d] = 0x%x\n",
					__func__, reg, (bit + len - 1), bit, val);
			}
			i++;
		}
	}
}

void lcd_tcon_data_block_regen_crc(unsigned char *data)
{
	unsigned int raw_crc32 = 0, new_crc32 = 0;
	struct lcd_tcon_data_block_header_s *header;

	if (!data)
		return;
	header = (struct lcd_tcon_data_block_header_s *)data;

	raw_crc32 = (data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24));
	new_crc32 = cal_CRC32(0, data + 4, header->block_size - 4);
	if (raw_crc32 != new_crc32) {
		data[0] = (unsigned char)(new_crc32 & 0xff);
		data[1] = (unsigned char)((new_crc32 >> 8) & 0xff);
		data[2] = (unsigned char)((new_crc32 >> 16) & 0xff);
		data[3] = (unsigned char)((new_crc32 >> 24) & 0xff);
	}
}

void lcd_tcon_pdf_clean_data(struct list_head *head)
{
	struct lcd_tcon_pdf_data_s *pdf_item = NULL;
	struct lcd_tcon_pdf_data_s *next = NULL;

	list_for_each_entry_safe(pdf_item, next, head, list) {
		list_del(&pdf_item->list);
		kfree(pdf_item);
	}
}

static void lcd_tcon_update_ufr_cur_info(struct aml_lcd_drv_s *pdrv,
		struct lcd_tcon_config_s *tcon_conf, unsigned char *data_buf)
{
	struct lcd_detail_timing_s *timing = NULL;
	struct lcd_tcon_init_block_header_s *init_header = NULL;
	struct lcd_tcon_init_block_ext_header_s ext_header;
	unsigned char *core_reg_table;
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	int is_match = 0, data_len = 0, core_tbl_byte_len = 0;

	if (!pdrv || !tcon_conf || !data_buf || !local_cfg)
		return;

	timing = pdrv->curr_dev->dev_cfg.timing.base_timing;
	if (!timing)
		return;

	memset(&ext_header, 0, sizeof(ext_header));
	init_header = (struct lcd_tcon_init_block_header_s *)data_buf;
	core_tbl_byte_len = tcon_conf->reg_table_len;
	if (init_header->ext_header_size) {
		memcpy(&ext_header, data_buf + init_header->header_size,
			sizeof(ext_header));
		if (ext_header.reg_blk_num) {
			ext_header.reg_blk_info = (struct lcd_tcon_reg_block_info_s *)
				(data_buf + TCON_EXT_BLK_INFO_PRE_OFFSET);
		}
		lcd_tcon_ext_header_check(&ext_header);
		core_tbl_byte_len = lcd_tcon_get_core_size(tcon_conf,
			init_header, &ext_header);
	}

	core_reg_table = data_buf + init_header->header_size + init_header->ext_header_size;
	data_len = core_tbl_byte_len +
		init_header->header_size + init_header->ext_header_size;

	if (timing->h_active == init_header->h_active &&
		timing->v_active == init_header->v_active) {
		is_match = 1;
		if (init_header->ext_header_size &&
			(timing->frame_rate < ext_header.framerate_min ||
			timing->frame_rate > ext_header.framerate_max)) {
			is_match = 0;
		}
	}

	if (is_match) {
		local_cfg->cur_core_info.header = init_header;
		local_cfg->cur_core_info.ext_header = ext_header;
		local_cfg->cur_core_info.table = core_reg_table;
		local_cfg->cur_core_info.table_size = core_tbl_byte_len;
		local_cfg->cur_core_info.user_info = NULL;
		lcd_tcon_init_data_version_update(&local_cfg->cur_core_info,
			init_header->version);
		if (init_header->block_size > data_len)
			local_cfg->cur_core_info.user_info = (char *)(data_buf + data_len);
	}
}

int lcd_tcon_data_load(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf, int index)
{
	struct lcd_tcon_data_block_header_s *block_header;
	struct lcd_tcon_init_block_header_s *init_header;
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_detail_timing_s match_timing;
	struct tcon_core_reg_info_s core_reg_info;
	struct lcd_tcon_init_block_ext_header_s ext_header;

	if (!tcon_mm_table.data_size) {
		LCD_ERR(NULL, "%s: data_size buf error\n", __func__);
		return -1;
	}

	memset(&core_reg_info, 0, sizeof(core_reg_info));
	memset(&ext_header, 0, sizeof(ext_header));

	block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
	if (block_header->block_size < sizeof(struct lcd_tcon_data_block_header_s)) {
		LCD_ERR(NULL, "%s: block[%d] size 0x%x error\n",
			__func__, index, block_header->block_size);
		return -1;
	}

	if (is_block_type_basic_init(block_header->block_type)) {
		if (!tcon_conf)
			return -1;
		init_header = (struct lcd_tcon_init_block_header_s *)data_buf;
		if (init_header->ext_header_size) {
			memcpy(&ext_header, data_buf + init_header->header_size,
				sizeof(ext_header));
			if (ext_header.reg_blk_num) {
				ext_header.reg_blk_info = (struct lcd_tcon_reg_block_info_s *)
					(data_buf + TCON_EXT_BLK_INFO_PRE_OFFSET);
			}
			lcd_tcon_ext_header_check(&ext_header);
		}
		core_reg_info.header = init_header;
		core_reg_info.ext_header = ext_header;
		core_reg_info.table = data_buf + block_header->header_size
				+ block_header->ext_header_size;

		if (tcon_conf->tcon_init_table_pre_proc)
			tcon_conf->tcon_init_table_pre_proc(&core_reg_info);
		lcd_tcon_data_block_regen_crc(data_buf);

		match_timing.h_active = init_header->h_active;
		match_timing.v_active = init_header->v_active;
		lcd_tcon_init_setting_check(pdrv, &match_timing, &core_reg_info);

		if (is_block_ctrl_ufr(init_header->block_ctrl))
			lcd_tcon_update_ufr_cur_info(pdrv, tcon_conf, data_buf);
	} else {
		tcon_mm_table.lut_valid_flag |= block_header->block_flag;
	}

	tcon_mm_table.data_size[index] = block_header->block_size;

	/* add data multi list */
	if (!is_block_type_basic_init(block_header->block_type) &&
		is_block_ctrl_multi(block_header->block_ctrl))
		lcd_tcon_data_multi_add(pdrv, &tcon_mm_table, block_header, index);

	if (block_header->block_type == LCD_TCON_DATA_BLOCK_TYPE_PDF) {
		if (!list_empty(&tcon_local_cfg.pdf_data_list)) {
			/* clean exist list */
			lcd_tcon_pdf_clean_data(&tcon_local_cfg.pdf_data_list);
			tcon_local_cfg.pdf_list_load_flag = 0;
			INIT_LIST_HEAD(&tcon_local_cfg.pdf_data_list);
		}
		if (!lcd_tcon_data_common_parse_set(pdrv, data_buf, (phys_addr_t)NULL, 1, index)) {
			if (!lcd_tcon_pdf_get_config(&tcon_local_cfg.pdf_data_list))
				tcon_local_cfg.pdf_list_load_flag = 1;
		}
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s block[%d]: size=0x%x, type=0x%02x, name=%s\n",
			__func__, index,
			block_header->block_size,
			block_header->block_type,
			block_header->name);
	}

	return 0;
}

void lcd_tcon_complete_data_load(struct aml_lcd_drv_s *pdrv)
{
	if (tcon_mm_table.data_complete)
		return;

	tcon_mm_table.data_complete = 1;
	lcd_tcon_fw_lut_update_status(pdrv);
	lrm_resource_device_finish("lcd_tcon_data");
	lcd_resource_ready(pdrv->index, LCD_RES_TCON, 0);
	LCDPR("%s: data_complete: %d\n", __func__, tcon_mm_table.data_complete);
}

static char *err_msg[] = {
	"resv mem",  /*0*/
	"data_size", /*1*/
	"block_cnt", /*2*/
	"not ready", /*3*/
	"data crc"   /*4*/
};

static int lcd_tcon_bin_path_update(unsigned int size)
{
	struct lcd_tcon_bin_path_header_s *header;
	unsigned char *mem_vaddr;
	unsigned int temp_crc32, err_num = 0;

	if (!tcon_rmem.bin_path_rmem.mem_vaddr) {
		err_num = 0;
		goto lcd_tcon_bin_path_update_err;
	}
	mem_vaddr = tcon_rmem.bin_path_rmem.mem_vaddr;
	header = (struct lcd_tcon_bin_path_header_s *)mem_vaddr;
	if (header->data_size < sizeof(*header)) {
		err_num = 1;
		goto lcd_tcon_bin_path_update_err;
	}
	if (header->block_cnt > TCON_DATA_CNT_MAX) {
		err_num = 2;
		goto lcd_tcon_bin_path_update_err;
	}
	if (!header->ready) {
		err_num = 3;
		goto lcd_tcon_bin_path_update_err;
	}
	temp_crc32 = cal_CRC32(0, &mem_vaddr[4], (header->data_size - 4));
	if (header->crc32 != temp_crc32) {
		err_num = 4;
		goto lcd_tcon_bin_path_update_err;
	}

	tcon_mm_table.version = header->version;
	tcon_mm_table.data_load_level = header->data_load_level;
	tcon_mm_table.block_cnt = header->block_cnt;
	//tcon_mm_table.init_load = header->init_load;
	//LCDPR("%s: init_load: %d\n", __func__, tcon_mm_table.init_load);

	tcon_mm_table.bin_path_valid = 1;

	return 0;

lcd_tcon_bin_path_update_err:
	LCD_ERR(NULL, "%s: %s error\n", __func__, err_msg[err_num]);
	return -1;
}

static int lcd_tcon_mm_table_config(void)
{
	unsigned char *mem_vaddr;
	unsigned int cnt, data_size, n, i;

	tcon_mm_table.effect_id = 0xff;
	tcon_mm_table.effect_type = 0xff;
	if (tcon_mm_table.block_cnt > TCON_DATA_CNT_MAX) {
		LCD_ERR(NULL, "%s: tcon data block_cnt %d invalid\n",
		       __func__, tcon_mm_table.block_cnt);
		return -1;
	}

	if (tcon_mm_table.data_mem_vaddr)
		return 0;
	if (tcon_mm_table.block_cnt == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: block_cnt is zero\n", __func__);
		return 0;
	}

	cnt = tcon_mm_table.block_cnt;
	tcon_mm_table.data_mem_vaddr = kcalloc(cnt, sizeof(unsigned char *), GFP_KERNEL);
	tcon_mm_table.data_mem_paddr = kcalloc(cnt, sizeof(dma_addr_t), GFP_KERNEL);
	if (!tcon_mm_table.data_mem_vaddr || !tcon_mm_table.data_mem_paddr)
		return -1;

	tcon_mm_table.data_size = kcalloc(cnt, sizeof(unsigned int), GFP_KERNEL);
	if (!tcon_mm_table.data_size)
		return -1;

	tcon_mm_table.data_multi = kcalloc(cnt, sizeof(struct tcon_data_multi_s), GFP_KERNEL);
	if (!tcon_mm_table.data_multi)
		return -1;

	mem_vaddr = tcon_rmem.bin_path_rmem.mem_vaddr;
	for (i = 0; i < tcon_mm_table.block_cnt; i++) {
		n = 32 + (i * 256);
		data_size = mem_vaddr[n] |
			(mem_vaddr[n + 1] << 8) |
			(mem_vaddr[n + 2] << 16) |
			(mem_vaddr[n + 3] << 24);
		if (data_size == 0) {
			LCD_ERR(NULL, "%s: block[%d] size is zero\n", __func__, i);
			continue;
		}
		tcon_mm_table.data_size[i] = data_size;
	}

	return 0;
}

static void lcd_tcon_axi_tbl_set_valid(unsigned int type, int valid)
{
	struct lcd_tcon_axi_mem_cfg_s *axi_cfg = NULL;
	int i = 0;

	if (lcd_tcon_conf->axi_tbl_len) {
		for (i = 0; i < lcd_tcon_conf->axi_tbl_len; i++) {
			axi_cfg = &lcd_tcon_conf->axi_mem_cfg_tbl[i];
			if (type == axi_cfg->mem_type) {
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("%s [%d] type=%d, valid=%d\n", __func__,
						i, type, axi_cfg->mem_valid);
				}
				axi_cfg->mem_valid = valid;
			}
		}
	}
}

static int lcd_tcon_axi_tbl_check_valid(unsigned int type)
{
	struct lcd_tcon_axi_mem_cfg_s *axi_cfg = NULL;
	int i = 0;

	if (lcd_tcon_conf->axi_tbl_len && lcd_tcon_conf->axi_mem_cfg_tbl) {
		for (i = 0; i < lcd_tcon_conf->axi_tbl_len; i++) {
			axi_cfg = &lcd_tcon_conf->axi_mem_cfg_tbl[i];
			if (type == axi_cfg->mem_type)
				return axi_cfg->mem_valid;
		}
	} else {
		if (type == TCON_AXI_MEM_TYPE_OD)
			return (tcon_rmem.axi_bank > 0);
		else if (type == TCON_AXI_MEM_TYPE_DEMURA)
			return (tcon_rmem.axi_bank > 1);
	}

	return 0;
}

unsigned char *lcd_tcon_demura_mem_get(struct aml_lcd_drv_s *pdrv,
		int *mem_len)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	struct lcd_tcon_axi_mem_cfg_s *axi_mem_cfg = NULL;
	unsigned char *mem = NULL;
	int i = 0, len = 0;

	if (!tcon_conf || !tcon_rmem || !tcon_rmem->axi_rmem)
		goto __demura_mem_get_exit;

	for (i = 0; i < tcon_conf->axi_tbl_len; i++) {
		axi_mem_cfg = &tcon_conf->axi_mem_cfg_tbl[i];
		if (!axi_mem_cfg->mem_valid)
			continue;
		if (axi_mem_cfg->mem_type == TCON_AXI_MEM_TYPE_DEMURA) {
			mem = tcon_rmem->axi_rmem[i].mem_vaddr;
			len = tcon_rmem->axi_rmem[i].mem_size;
			break;
		}
	}

	if (mem)
		*mem_len = len;

__demura_mem_get_exit:
	return mem;
}

int lcd_tcon_mem_od_is_valid(void)
{
	return lcd_tcon_axi_tbl_check_valid(TCON_AXI_MEM_TYPE_OD);
}

int lcd_tcon_mem_demura_is_valid(void)
{
	return lcd_tcon_axi_tbl_check_valid(TCON_AXI_MEM_TYPE_DEMURA);
}

/* default od secure */
static void lcd_tcon_axi_mem_secure_t3(void)
{
	unsigned int *data;

	if (!tcon_rmem.secure_cfg_rmem.mem_vaddr)
		return;

	/* only default protect od mem */
	data = (unsigned int *)tcon_rmem.secure_cfg_rmem.mem_vaddr;
	tcon_rmem.secure_axi_rmem.sec_protect = *data;
	tcon_rmem.secure_axi_rmem.sec_handle = *(data + 1);
	if (tcon_rmem.secure_axi_rmem.sec_protect == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: axi_rmem[0] is unprotect\n", __func__);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: axi_rmem[0] protect handle: %d\n",
			__func__, tcon_rmem.secure_axi_rmem.sec_handle);
	}
}

#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
void lcd_tcon_mem_tee_get_status(void)
{
	char *print_buf;

	print_buf = kcalloc(PR_BUF_MAX, sizeof(char), GFP_KERNEL);
	if (!print_buf) {
		LCD_ERR(NULL, "%s: buf malloc error\n", __func__);
		return;
	}

	lcd_tcon_axi_mem_print(&tcon_rmem, print_buf, 0);
	pr_info("%s\n", print_buf);
	kfree(print_buf);
}

int lcd_tcon_mem_tee_protect(int protect_en)
{
	int ret;

	if (tcon_rmem.flag == 0 || !tcon_rmem.axi_rmem) {
		LCD_ERR(NULL, "%s: no axi_rmem\n", __func__);
		return -1;
	}

	if (protect_en) {
		if (tcon_rmem.secure_axi_rmem.sec_protect) {
			LCDPR("%s: secure_rmem is already protected\n", __func__);
			return 0;
		}

		ret = tee_protect_mem_by_type(TEE_MEM_TYPE_TCON,
					      tcon_rmem.secure_axi_rmem.mem_paddr,
					      tcon_rmem.secure_axi_rmem.mem_size,
					      &tcon_rmem.secure_axi_rmem.sec_handle);

		if (ret) {
			LCD_ERR(NULL,
				"%s: protect failed! secure_rmem start:0x%08x, size:0x%x, ret:%d",
			       __func__, (unsigned int)tcon_rmem.secure_axi_rmem.mem_paddr,
			       tcon_rmem.secure_axi_rmem.mem_size, ret);
			return -1;
		}
		tcon_rmem.secure_axi_rmem.sec_protect = 1;
		LCDPR("%s: protect OK. secure_rmem start: 0x%08x, size: 0x%x\n",
			__func__,
			(unsigned int)tcon_rmem.secure_axi_rmem.mem_paddr,
			tcon_rmem.secure_axi_rmem.mem_size);
	} else {
		if (tcon_rmem.secure_axi_rmem.sec_protect == 0)
			return 0;

		tee_unprotect_mem(tcon_rmem.secure_axi_rmem.sec_handle);
		tcon_rmem.secure_axi_rmem.sec_protect = 0;
		tcon_rmem.secure_axi_rmem.sec_handle = 0;
		LCDPR("%s: unprotect OK. secure_rmem start: 0x%08x, size: 0x%x\n",
			__func__,
			(unsigned int)tcon_rmem.secure_axi_rmem.mem_paddr,
			tcon_rmem.secure_axi_rmem.mem_size);
	}

	return 0;
}
#endif

static void lcd_tcon_axi_mem_config(void)
{
	struct lcd_tcon_axi_mem_cfg_s *axi_mem_cfg = NULL;
	unsigned int temp_size = 0;
	unsigned int mem_paddr = 0, od_mem_size = 0;
	int i;

	if (!lcd_tcon_conf->axi_tbl_len || !lcd_tcon_conf->axi_mem_cfg_tbl)
		return;

	tcon_rmem.axi_rmem = kcalloc(tcon_rmem.axi_bank,
		sizeof(struct tcon_rmem_config_s), GFP_KERNEL);
	if (!tcon_rmem.axi_rmem)
		return;

	tcon_rmem.axi_reg = kcalloc(tcon_rmem.axi_bank, sizeof(unsigned int), GFP_KERNEL);
	if (!tcon_rmem.axi_reg) {
		kfree(tcon_rmem.axi_rmem);
		return;
	}

	for (i = 0; i < lcd_tcon_conf->axi_tbl_len; i++) {
		axi_mem_cfg = &lcd_tcon_conf->axi_mem_cfg_tbl[i];
		tcon_rmem.axi_reg[i] = axi_mem_cfg->axi_reg;
		if (!axi_mem_cfg->mem_valid)
			continue;
		tcon_rmem.axi_rmem[i].mem_size = axi_mem_cfg->mem_size;
		tcon_rmem.axi_rmem[i].mem_paddr = tcon_rmem.axi_mem_paddr + temp_size;
		// OD is secure, cpu do not access
		if (axi_mem_cfg->mem_type == TCON_AXI_MEM_TYPE_OD) {
			tcon_rmem.axi_rmem[i].mem_vaddr = NULL;
		} else {
			tcon_rmem.axi_rmem[i].mem_vaddr =
				lcd_tcon_paddrtovaddr(tcon_rmem.axi_rmem[i].mem_paddr,
							axi_mem_cfg->mem_size);
			if (!tcon_rmem.axi_rmem[i].mem_vaddr) {
				tcon_rmem.axi_rmem[i].mem_paddr = 0;
				tcon_rmem.axi_rmem[i].mem_size = 0;
				continue;
			}
		}
		temp_size += axi_mem_cfg->mem_size;
		if (axi_mem_cfg->mem_type == TCON_AXI_MEM_TYPE_OD) {
			if (!mem_paddr)
				mem_paddr = tcon_rmem.axi_rmem[i].mem_paddr;
			od_mem_size += tcon_rmem.axi_rmem[i].mem_size;
		}
	}
	tcon_rmem.secure_axi_rmem.mem_paddr = mem_paddr;
	tcon_rmem.secure_axi_rmem.mem_vaddr = NULL;// secure memory, cpu do not access
	tcon_rmem.secure_axi_rmem.mem_size = od_mem_size;
}

static int lcd_tcon_mem_config(void)
{
	unsigned int mem_size = 0, mem_od_size = 0, mem_dmr_size = 0;
	unsigned int axi_mem_size = 0;
	int ret = -1, i = 0;
	struct lcd_tcon_axi_mem_cfg_s *axi_cfg = NULL;

	if (tcon_rmem.flag == 0)
		return -1;

	tcon_rmem.axi_bank = lcd_tcon_conf->axi_bank;
	if (lcd_tcon_conf->axi_tbl_len && lcd_tcon_conf->axi_mem_cfg_tbl) {
		for (i = 0; i < lcd_tcon_conf->axi_tbl_len; i++) {
			axi_cfg = &lcd_tcon_conf->axi_mem_cfg_tbl[i];
			switch (axi_cfg->mem_type) {
			case TCON_AXI_MEM_TYPE_OD:
				mem_od_size += axi_cfg->mem_size;
				break;
			case TCON_AXI_MEM_TYPE_DEMURA:
				mem_dmr_size += axi_cfg->mem_size;
				break;
			default:
				LCD_ERR(NULL, "Unsupport mem type=%d\n", axi_cfg->mem_type);
				break;
			}
		}

		/* check mem */
		axi_mem_size = mem_od_size + mem_dmr_size;
		mem_size = axi_mem_size + lcd_tcon_conf->bin_path_size +
			lcd_tcon_conf->secure_cfg_size;
		if (tcon_rmem.rsv_mem_size < mem_size) {
			axi_mem_size = mem_od_size;
			mem_size = axi_mem_size + lcd_tcon_conf->bin_path_size +
				lcd_tcon_conf->secure_cfg_size;
			if (tcon_rmem.rsv_mem_size < mem_size) {
				LCD_ERR(NULL, "%s: tcon mem size 0x%x is not enough, need min 0x%x",
					__func__, tcon_rmem.rsv_mem_size, mem_size);
				return -1;
			}
			lcd_tcon_axi_tbl_set_valid(TCON_AXI_MEM_TYPE_OD, 1);
		} else {
			lcd_tcon_axi_tbl_set_valid(TCON_AXI_MEM_TYPE_OD, 1);
			lcd_tcon_axi_tbl_set_valid(TCON_AXI_MEM_TYPE_DEMURA, 1);
		}

		lcd_tcon_conf->axi_mem_size = axi_mem_size;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("axi[%d] mem type=%d, size=%#x, reg=%#x, valid=%d\n",
				i, axi_cfg->mem_type, axi_cfg->mem_size,
				axi_cfg->axi_reg, axi_cfg->mem_valid);
		}
	} else {
		mem_size = lcd_tcon_conf->axi_mem_size + lcd_tcon_conf->bin_path_size
			+ lcd_tcon_conf->secure_cfg_size;

		if (tcon_rmem.rsv_mem_size < mem_size) {
			LCD_ERR(NULL, "%s: tcon mem size 0x%x is not enough, need 0x%x\n",
			       __func__, tcon_rmem.rsv_mem_size, mem_size);
			return -1;
		}
	}

	tcon_rmem.axi_mem_size = lcd_tcon_conf->axi_mem_size;
	tcon_rmem.axi_mem_paddr = tcon_rmem.rsv_mem_paddr;
	tcon_rmem.sw_mem_paddr = tcon_rmem.axi_mem_paddr + tcon_rmem.axi_mem_size;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: axi_mem paddr: 0x%08x, size: 0x%x, sw_mem paddr: 0x%08x\n",
			  __func__, (unsigned int)tcon_rmem.axi_mem_paddr,
			  tcon_rmem.axi_mem_size,
			  (unsigned int)tcon_rmem.sw_mem_paddr);
	}

	if (lcd_tcon_conf->tcon_axi_mem_config)
		lcd_tcon_conf->tcon_axi_mem_config();
	else
		lcd_tcon_axi_mem_config();

	tcon_rmem.bin_path_rmem.mem_size = lcd_tcon_conf->bin_path_size;
	tcon_rmem.bin_path_rmem.mem_paddr = tcon_rmem.sw_mem_paddr;
	tcon_rmem.bin_path_rmem.mem_vaddr =
		lcd_tcon_paddrtovaddr(tcon_rmem.bin_path_rmem.mem_paddr,
				      tcon_rmem.bin_path_rmem.mem_size);
	if ((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) &&
	    tcon_rmem.bin_path_rmem.mem_size > 0) {
		LCDPR("tcon bin_path paddr: 0x%08x, vaddr: 0x%px, size: 0x%x\n",
		      (unsigned int)tcon_rmem.bin_path_rmem.mem_paddr,
		      tcon_rmem.bin_path_rmem.mem_vaddr,
		      tcon_rmem.bin_path_rmem.mem_size);
	}

	ret = lcd_tcon_bin_path_update(tcon_rmem.bin_path_rmem.mem_size);
	if (ret)
		return -1;

	tcon_rmem.secure_cfg_rmem.mem_size = lcd_tcon_conf->secure_cfg_size;
	tcon_rmem.secure_cfg_rmem.mem_paddr =
		tcon_rmem.bin_path_rmem.mem_paddr + tcon_rmem.bin_path_rmem.mem_size;
	if (tcon_rmem.secure_cfg_rmem.mem_size > 0) {
		tcon_rmem.secure_cfg_rmem.mem_vaddr =
			lcd_tcon_paddrtovaddr(tcon_rmem.secure_cfg_rmem.mem_paddr,
				      tcon_rmem.secure_cfg_rmem.mem_size);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("tcon secure_cfg paddr: 0x%08x, vaddr: 0x%px, size: 0x%x\n",
				(unsigned int)tcon_rmem.secure_cfg_rmem.mem_paddr,
				tcon_rmem.secure_cfg_rmem.mem_vaddr,
				tcon_rmem.secure_cfg_rmem.mem_size);
		}
	} else {
		tcon_rmem.secure_cfg_rmem.mem_vaddr = NULL;
	}

	if (lcd_tcon_conf->tcon_axi_mem_secure && tcon_rmem.secure_cfg_rmem.mem_vaddr)
		lcd_tcon_conf->tcon_axi_mem_secure();

	ret = lcd_tcon_mm_table_config();
	return ret;
}

static int lcd_tcon_reserved_memory_init(struct aml_lcd_drv_s *pdrv)
{
	struct device_node *mem_node;
	struct resource res;
	int ret;
	u64 paddr = 0;
	u32 size = 0;

	/*
	 * this is a temporary use, consistent with the original architecture
	 * because tcon will change too much if reconstruct
	 */
	tcon_rmem.use_lrm = 0;
	lrm_get_by_name("lcd_tcon_rsvd", &paddr, &size);
	if (paddr && size) {
		tcon_rmem.rsv_mem_paddr = paddr;
		tcon_rmem.rsv_mem_size = size;
		tcon_rmem.use_lrm = 1;
		if (lrm_no_map())
			tcon_rmem.flag = 2;
		else
			tcon_rmem.flag = 1;
		goto lcd_tcon_reserved_memory_init_exit;
	}

	mem_node = of_parse_phandle(pdrv->dev->of_node, "memory-region", 0);
	if (!mem_node || !of_device_is_available(mem_node)) {
		LCD_ERR(NULL, "tcon get memory-region fail\n");
		return -1;
	}

	ret = of_address_to_resource(mem_node, 0, &res);
	if (ret) {
		LCD_ERR(NULL, "tcon reserve memory source fail\n");
		return -1;
	}

	tcon_rmem.rsv_mem_paddr = res.start;
	tcon_rmem.rsv_mem_size = resource_size(&res);
	if (tcon_rmem.rsv_mem_paddr == 0) {
		LCD_ERR(NULL, "tcon resv_mem paddr 0 error\n");
		tcon_rmem.rsv_mem_size = 0;
		return -1;
	}
	if (tcon_rmem.rsv_mem_size == 0) {
		LCD_ERR(NULL, "tcon resv_mem size 0 error\n");
		tcon_rmem.rsv_mem_paddr = 0;
		return -1;
	}

	if (of_find_property(mem_node, "no-map", NULL))
		tcon_rmem.flag = 2;
	else
		tcon_rmem.flag = 1;

lcd_tcon_reserved_memory_init_exit:
	LCDPR("tcon resv_mem flag:%d, paddr:0x%lx, size:0x%x\n",
		tcon_rmem.flag,
		(unsigned long)tcon_rmem.rsv_mem_paddr,
		tcon_rmem.rsv_mem_size);

	return 0;
}

static void lcd_tcon_reserved_memory_release(struct aml_lcd_drv_s *pdrv)
{
	struct device_node *mem_node;
	struct resource res;
	unsigned long end;
	unsigned int highmem_flag = 0;
	int ret;

	if (tcon_rmem.use_lrm)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_debug: %s\n", __func__);

	mem_node = of_parse_phandle(pdrv->dev->of_node, "memory-region", 0);
	if (!mem_node || !of_device_is_available(mem_node)) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("no tcon memory-region\n");
		return;
	}

	ret = of_address_to_resource(mem_node, 0, &res);
	if (ret) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("no tcon reserve memory source\n");
		return;
	}

	if (of_find_property(mem_node, "no-map", NULL)) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("no-map memory source, skip release\n");
		return;
	}

	end = PAGE_ALIGN(res.end);

	highmem_flag = PageHighMem(phys_to_page(res.start));
	if (!highmem_flag) {
		aml_free_reserved_area(__va(res.start), __va(end),
			0, "lcd_tcon_reserved");
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		if (highmem_flag) {
			LCDPR("Release memory need to confirm linear address\n");
		} else {
			LCDPR("free reserved area: paddr:0x%lx, size:0x%lx\n",
				(long)res.start, (long)resource_size(&res));
		}
	}
}

static irqreturn_t lcd_tcon_isr(int irq, void *dev_id)
{
	struct aml_lcd_drv_s *pdrv = aml_lcd_get_driver(0);
	unsigned int temp;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return IRQ_HANDLED;

	temp = lcd_tcon_read(pdrv, TCON_INTR_RO);
	if (temp & 0x2) {
		LCDPR("%s: tcon sw_reset triggered\n", __func__);
		lcd_tcon_core_update(pdrv);
	}
	if (temp & 0x40)
		LCDPR("%s: tcon ddr interface error triggered\n", __func__);

	return IRQ_HANDLED;
}

static void lcd_tcon_intr_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int tcon_irq = 0;

	if (!pdrv->drv_res.res_tcon_irq) {
		LCD_ERR(NULL, "res_tcon_irq is null\n");
		return;
	}
	tcon_irq = pdrv->drv_res.res_tcon_irq;
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("tcon_irq: %d\n", tcon_irq);

	if (request_irq(tcon_irq, lcd_tcon_isr, IRQF_SHARED,
		"lcd_tcon", (void *)"lcd_tcon")) {
		LCD_ERR(NULL, "can't request lcd_tcon irq\n");
	}

	lcd_tcon_write(pdrv, TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);
}

void lcd_tcon_init_data_version_update(struct tcon_core_reg_info_s *core_info,
		char *data_buf)
{
	if (!core_info || !data_buf)
		return;

	memcpy(core_info->bin_ver, data_buf, LCD_TCON_INIT_BIN_VERSION_SIZE);
	core_info->bin_ver[TCON_BIN_VER_LEN - 1] = '\0';
}

int lcd_tcon_init_data_update(char *data_buf, struct lcd_detail_timing_s *ptiming)
{
	struct lcd_tcon_init_block_header_s *data_header = NULL;
	struct lcd_tcon_init_block_ext_header_s ext_header;
	struct tcon_core_reg_info_s *core_reg_info = NULL;
	unsigned char *p;
	int data_len;
	char str[64] = {0};

	if (!data_buf)
		return -1;

	memset(&ext_header, 0, sizeof(ext_header));
	core_reg_info = &tcon_local_cfg.cur_core_info;
	data_header = (struct lcd_tcon_init_block_header_s *)data_buf;
	data_len = core_reg_info->table_size
			+ data_header->header_size
			+ data_header->ext_header_size;
	if (data_header->block_size < data_len) {
		LCD_ERR(NULL, "%s: data block_size(%d) is not enough, need %d\n",
			__func__, data_header->block_size, data_len);
		return -1;
	}

	if (data_header->ext_header_size > 0) {
		p = data_buf + data_header->header_size;
		memcpy(&ext_header, p, sizeof(ext_header));
		if (ext_header.reg_blk_num) {
			ext_header.reg_blk_info = (struct lcd_tcon_reg_block_info_s *)
				(data_buf + TCON_EXT_BLK_INFO_PRE_OFFSET);
		}
		lcd_tcon_ext_header_check(&ext_header);
	}

	if (ptiming) { //timing match check
		if (ptiming->h_active != data_header->h_active ||
		    ptiming->v_active != data_header->v_active) {
			return -1;
		}
		if (data_header->ext_header_size) {
			if (ptiming->frame_rate < ext_header.framerate_min ||
			    ptiming->frame_rate > ext_header.framerate_max)
				return -1;
		}
	}

	p = data_buf + data_header->header_size + data_header->ext_header_size;
	core_reg_info->table = p;

	core_reg_info->header = data_header;
	core_reg_info->ext_header = ext_header;
	//user info
	if (data_header->block_size > data_len) {
		p = data_buf + data_len;
		core_reg_info->user_info = (char *)p;
	} else {
		core_reg_info->user_info = NULL;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		if (data_header->ext_header_size) {
			snprintf(str, 64, "fr_range: %d~%d, ",
				ext_header.framerate_min, ext_header.framerate_max);
		}
		LCDPR("%s: data_size:%d, chipid:0x%x, block_ctrl:0x%x, %sname:%s\n",
			__func__, data_header->block_size,
			data_header->chipid, data_header->block_ctrl,
			str, data_header->name);
		if (core_reg_info->user_info)
			LCDPR("user info:\n%s\n", core_reg_info->user_info);
		memset(str, 0, 64);
	}

	lcd_tcon_init_data_version_update(core_reg_info, data_buf);

	if (ptiming)
		snprintf(str, 64, "@%dhz", ptiming->frame_rate);
	LCDPR("tcon init update: %dx%d%s, bin_ver:%s\n",
		data_header->h_active, data_header->v_active,
		str, core_reg_info->bin_ver);

	return 0;
}

void lcd_tcon_ext_header_check(struct lcd_tcon_init_block_ext_header_s *ext_header)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_tcon_reg_block_info_s *reg_block = NULL;
	struct lcd_tcon_reg_skip_s *reg_skip_blk = NULL;
	int i, j, reg_start, reg_end, valid = 0;
	unsigned int pre_reg_end = 0, max_len = 0, tbl_width;

	if (!ext_header || !tcon_conf)
		return;

	if (!ext_header->reg_blk_info)
		goto __ext_header_chk_exit;

	tbl_width = lcd_do_div(tcon_conf->reg_table_width, 8);
	for (i = 0; i < ext_header->reg_blk_num; i++) {
		reg_block = &ext_header->reg_blk_info[i];
		reg_start = reg_block->start;
		reg_end = reg_block->start + reg_block->len - 1;
		max_len += reg_block->len * tbl_width;

		if (reg_block->len <= 0)
			goto __ext_header_chk_exit;

		//check current block reg > pre block reg
		if (pre_reg_end && (reg_start <= pre_reg_end || reg_end <= pre_reg_end))
			goto __ext_header_chk_exit;

		//check skip range
		if (tcon_conf->reg_skip_tbl_len) {
			for (j = 0; j < tcon_conf->reg_skip_tbl_len; j++) {
				reg_skip_blk = &tcon_conf->reg_skip_tbl[j];

				if (reg_start >= reg_skip_blk->start_reg &&
					reg_start <= reg_skip_blk->end_reg)
					goto __ext_header_chk_exit;

				if (reg_end >= reg_skip_blk->start_reg &&
					reg_end <= reg_skip_blk->end_reg)
					goto __ext_header_chk_exit;
			}
		}

		pre_reg_end = reg_end;
	}

	if (max_len > tcon_conf->reg_table_len)
		goto __ext_header_chk_exit;

	valid = 1;

__ext_header_chk_exit:
	if (!valid) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCD_ERR(NULL, "%s: invalid reg block, change blk_num %d-->0\n",
				__func__, ext_header->reg_blk_num);
		}
		ext_header->reg_blk_num = 0;  //restore blk num to 0
		//ext_header->reg_blk_info = NULL;
	}
}

static int lcd_tcon_core_reg_check_load(struct aml_lcd_drv_s *pdrv, unsigned char *buf, int len)
{
	int data_len;
	unsigned char *p;
	struct lcd_tcon_init_block_header_s *data_header = NULL, *tmp_header;
	struct lcd_tcon_init_block_ext_header_s data_ext_header;
	struct tcon_core_reg_info_s *core_reg_info = NULL;
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();

	if (!pdrv || !buf || !tcon_conf)
		return -1;

	memset(&data_ext_header, 0, sizeof(data_ext_header));
	core_reg_info = &tcon_local_cfg.def_core_info;
	tmp_header = (struct lcd_tcon_init_block_header_s *)buf;
	data_header = kzalloc(tmp_header->header_size, GFP_KERNEL);
	if (!data_header)
		goto lcd_tcon_core_reg_check_load_err;
	memcpy(data_header, tmp_header, tmp_header->header_size);
	if (data_header->ext_header_size > 0) {
		memcpy(&data_ext_header, buf + data_header->header_size,
			sizeof(data_ext_header));
		data_ext_header.reg_blk_info = NULL;
		if (data_ext_header.reg_blk_num) {
			data_ext_header.reg_blk_info = (struct lcd_tcon_reg_block_info_s *)
				kcalloc(data_ext_header.reg_blk_num,
					sizeof(struct lcd_tcon_reg_block_info_s), GFP_KERNEL);
			if (!data_ext_header.reg_blk_info) {
				LCD_ERR(NULL, "%s: alloc reg block fail\n", __func__);
				goto lcd_tcon_core_reg_check_load_err;
			}
			memcpy(data_ext_header.reg_blk_info,
				buf + TCON_EXT_BLK_INFO_PRE_OFFSET,
				data_ext_header.reg_blk_num *
				sizeof(struct lcd_tcon_reg_block_info_s));
		}
		lcd_tcon_ext_header_check(&data_ext_header);
		data_len = lcd_tcon_get_core_size(tcon_conf,
			data_header, &data_ext_header);
		core_reg_info->table_size = data_len < 0 ?
			tcon_conf->reg_table_len : data_len;
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: update core reg table size=%d (%#x)\n", __func__,
				core_reg_info->table_size, core_reg_info->table_size);
		}
	}
	data_len = core_reg_info->table_size +
		data_header->header_size + data_header->ext_header_size;
	if (len < data_len || data_header->block_size < data_len) {
		LCD_ERR(NULL, "%s: key_len(%d) or data block_size(%d) are not enough, need %d\n",
			__func__, len, data_header->block_size, data_len);
		goto lcd_tcon_core_reg_check_load_err;
	}
	core_reg_info->user_info = NULL;
	if (data_header->block_size > data_len) {
		//user info
		core_reg_info->user_info = kcalloc(data_header->block_size - data_len + 1,
				sizeof(char), GFP_KERNEL);
		if (!core_reg_info->user_info)
			goto lcd_tcon_core_reg_check_load_err;
		memcpy(core_reg_info->user_info, buf + data_len,
			data_header->block_size - data_len);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("user info:\n%s\n", core_reg_info->user_info);
	}
	lcd_tcon_init_data_version_update(core_reg_info, data_header->version);

	data_len = core_reg_info->table_size;
	core_reg_info->table = kcalloc(data_len, sizeof(unsigned char), GFP_KERNEL);
	if (!core_reg_info->table)
		goto lcd_tcon_core_reg_check_load_err;
	p = buf + data_header->header_size + data_header->ext_header_size;
	memcpy(core_reg_info->table, p, data_len);
	core_reg_info->header = data_header;
	core_reg_info->ext_header = data_ext_header;

	if (tcon_conf && tcon_conf->tcon_init_table_pre_proc)
		tcon_conf->tcon_init_table_pre_proc(core_reg_info);
	memcpy(&tcon_local_cfg.cur_core_info,
		core_reg_info, sizeof(*core_reg_info));
	lcd_tcon_init_setting_check(pdrv,
		&pdrv->curr_dev->dev_cfg.timing.act_timing, core_reg_info);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		lcd_tcon_core_info_print(pdrv, core_reg_info);
	LCDPR("tcon: load init data len: %d, ver: %s\n",
	      data_len, tcon_local_cfg.cur_core_info.bin_ver);
	return 0;

lcd_tcon_core_reg_check_load_err:
	kfree(data_header);
	kfree(data_ext_header.reg_blk_info);
	kfree(core_reg_info->user_info);
	LCD_ERR(NULL, "%s: tcon unifykey load error!!!\n", __func__);
	return -1;
}

static int lcd_tcon_load_init_data(struct aml_lcd_drv_s *pdrv)
{
	unsigned char *buf;
	int buf_size;

	buf = panel_param_mem_get("tcon_core_reg", &buf_size);
	if (!buf) {
		LCD_ERR(NULL, "%s can't get tcon_core_reg bin\n", __func__);
		return -1;
	}

	return lcd_tcon_core_reg_check_load(pdrv, buf, buf_size);
}

static int lcd_tcon_dccd_flow_check(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_fw_s *fw = aml_lcd_tcon_get_fw();

	if (pdrv->boot_ctrl->dccd_flag && fw && fw->valid) {
		lcd_resource_add(pdrv, LCD_RES_TCON_DCCD, 0);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("Add tcon dccd resource, wait resource ready\n");
	}

	return 0;
}

static int lcd_panel_parse_tcon_from_json(struct aml_lcd_drv_s *pdrv)
{
	struct json_parse_s *jsp = get_panel_jsp(pdrv->index);
	struct json_s *parent, *child;
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct lcd_tcon_ip27_s *ip27;

	if (!json_parse_ok(jsp)) {
		LCD_ERR(NULL, "panel%d jsp not ok\n", pdrv->index);
		return -1;
	}

	parent = json_path_to_node(jsp, jsp->root, "/tcon");
	if (!parent) {
		LCD_ERR(NULL, "find /lcd_extern\n");
		return -1;
	}
	/*ip27*/
	child = json_get_object_child(jsp, parent, "csot_ip27");
	if (child) {
		ip27 = &local_cfg->ip27;
		ip27->cfg_valid = json_get_obj_u32(jsp, child, "valid", 0);
		ip27->trig_mode = json_get_obj_u32(jsp, child, "trig_mode", 0);
		ip27->trig_line = json_get_obj_u32(jsp, child, "trig_line", 0);
		ip27->i2c_addr  = json_get_obj_u32(jsp, child, "i2c_addr", 0x33);
		ip27->i2c_speed = json_get_obj_u32(jsp, child, "i2c_speed", 400000);//400k
		//ip27->i2c_bytes = json_get_obj_u32(jsp, child, "i2c_byte", 4);//3byte mode
		ip27->data_mode = json_get_obj_u32(jsp, child, "data_mode", 0);//
		ip27->debug     = json_get_obj_u32(jsp, child, "debug", 0);//
		ip27->vcom_order = json_get_obj_u32(jsp, child, "vcom_order", 0);//
		ip27->i2c_reg_offset = json_get_obj_u32(jsp, child, "i2c_reg_offset", 0x25);//
		ip27->trig_line_dbg = ip27->trig_line;
		ip27->i2c_bytes = ip27->data_mode == 0 ? 24 + 1 : 3 + 1;
	}

	return 0;
}

unsigned int lcd_tcon_get_vfp_tail(void)
{
	if (!lcd_tcon_conf)
		return 0;

	return lcd_tcon_conf->vfp_tail;
}

unsigned int lcd_tcon_get_default_prede_v(void)
{
	if (!lcd_tcon_conf)
		return 0;

	return lcd_tcon_conf->prede_v_dft;
}

unsigned int lcd_tcon_get_default_prede_h(void)
{
	if (!lcd_tcon_conf)
		return 0;

	return lcd_tcon_conf->prede_h_dft;
}

static int lcd_tcon_get_config(struct aml_lcd_drv_s *pdrv)
{
	tcon_mm_table.data_init = NULL;
	tcon_local_cfg.def_core_info.table_size = lcd_tcon_conf->reg_table_len;
	INIT_LIST_HEAD(&tcon_local_cfg.pdf_data_list);

	lcd_tcon_load_init_data(pdrv);
	if (pdrv->config_load == LCD_CONFIG_FILE) {
		if (get_lcd_panel_file_type(pdrv->index) == PANEL_FILE_JSON)
			lcd_panel_parse_tcon_from_json(pdrv);
	}

	lcd_tcon_pdf_init(pdrv);
	//lcd_swpdf_init(pdrv);

	lcd_tcon_rdma_init(pdrv);

	tcon_mm_table.multi_lut_update = 1;
	if (pdrv->status & LCD_STATUS_IF_ON)
		pdrv->status |= LCD_STATUS_TCON_RDY;

	lcd_tcon_intr_init(pdrv);

	lcd_tcon_fw_prepare(pdrv, lcd_tcon_conf);

	lcd_tcon_debug_file_add(pdrv, &tcon_local_cfg);

	lcd_tcon_dccd_flow_check(pdrv);

	if (lcd_tcon_conf->lut_dma_ops && lcd_tcon_conf->lut_dma_ops->init)
		lcd_tcon_conf->lut_dma_ops->init(pdrv, lcd_tcon_conf->lut_dma_ops);

	tcon_ip27_probe_init(pdrv, &tcon_local_cfg.ip27);
	lrm_resource_device_finish("lcd_tcon");

	return 0;
}

/* **********************************
 * tcon match data
 * **********************************
 */
static struct lcd_tcon_axi_mem_cfg_s axi_mem_cfg_tbl_t5[] = {
	{ TCON_AXI_MEM_TYPE_OD,     0x00800000, 0x261, 0 },  /* 8M */
	{ TCON_AXI_MEM_TYPE_DEMURA, 0x00100000, 0x1a9, 0 },  /* 1M */
};

static struct lcd_tcon_axi_mem_cfg_s axi_mem_cfg_tbl_t5d[] = {
	{ TCON_AXI_MEM_TYPE_OD,     0x00400000, 0x261, 0 },  /* 4M */
};

static struct lcd_tcon_axi_mem_cfg_s axi_mem_cfg_tbl_t3x[] = {
	{ TCON_AXI_MEM_TYPE_OD,     0x00400000, 0x261, 0 },  /* 4M */
	{ TCON_AXI_MEM_TYPE_OD,     0x00400000, 0x26d, 0 },  /* 4M */
	{ TCON_AXI_MEM_TYPE_DEMURA, 0x00100000, 0x1a9, 0 },  /* 1M */
};

static struct lcd_tcon_axi_mem_cfg_s axi_mem_cfg_tbl_txhd2[] = {
	{ TCON_AXI_MEM_TYPE_OD,     0x00400000, 0x261, 0 },  /* 4M */
	{ TCON_AXI_MEM_TYPE_DEMURA, 0x00100000, 0x19b, 0 },  /* 1M */
};

static struct lcd_tcon_axi_mem_cfg_s axi_mem_cfg_tbl_t6d[] = {
	{ TCON_AXI_MEM_TYPE_OD,     0x00200000, 0x261, 0 },  /* 2M */
	{ TCON_AXI_MEM_TYPE_DEMURA, 0x00100000, 0x19b, 0 },  /* 1M */
};

static struct lcd_tcon_reg_skip_s tcon_skip_reg_tbl_t6w[] = {
	{ 0x0040, 0x00ff },
	{ 0x1200, 0x12ff },
};

static struct lcd_tcon_reg_skip_s tcon_skip_reg_tbl_t6x[] = {
	{ 0x0040, 0x006f },
	{ 0x1200, 0x12ff },
};

static struct lcd_tcon_dma_ops_s lcd_tcon_dma_ops_t5m = {
	.status = 0,
	.addr_list = NULL,
	.get_frame_cnt = tcon_lut_dma_get_frame_cnt,
	.start = tcon_lut_dma_start,
	.stop = tcon_lut_dma_stop,
	.mif_set = tcon_lut_dma_mif_set,
	.init = tcon_lut_dma_init_t5m,
	.deinit = tcon_lut_dma_deinit,
	.update = lcd_tcon_lut_dma_update,
	.is_done = NULL,
	.clr_done = NULL,
	.wait_done = NULL,
	.sw_mif_set = NULL,
	.sw_trigger = NULL,
	.sw_is_done = NULL,
	.sw_wait_done = NULL,
	.sw_clr_done = NULL,
};

static struct lcd_tcon_dma_ops_s lcd_tcon_dma_ops_t3x = {
	.status = 0,
	.addr_list = NULL,
	.get_frame_cnt = tcon_lut_dma_get_frame_cnt,
	.start = tcon_lut_dma_start,
	.stop = tcon_lut_dma_stop,
	.mif_set = tcon_lut_dma_mif_set,
	.init = tcon_lut_dma_init_t3x,
	.deinit = tcon_lut_dma_deinit,
	.update = lcd_tcon_lut_dma_update,
	.is_done = NULL,
	.clr_done = NULL,
	.wait_done = NULL,
	.sw_mif_set = NULL,
	.sw_trigger = NULL,
	.sw_is_done = NULL,
	.sw_wait_done = NULL,
	.sw_clr_done = NULL,
};

static struct lcd_tcon_dma_ops_s lcd_tcon_dma_ops_t6d = {
	.status = 0,
	.addr_list = NULL,
	.get_frame_cnt = tcon_lut_dma_get_frame_cnt,
	.start = tcon_lut_dma_start_t6d,
	.stop = tcon_lut_dma_stop_t6d,
	.mif_set = tcon_lut_dma_mif_set,
	.init = tcon_lut_dma_init_t5m,
	.deinit = tcon_lut_dma_deinit,
	.update = lcd_tcon_lut_dma_update,
	.is_done = NULL,
	.clr_done = NULL,
	.wait_done = NULL,
	.sw_mif_set = NULL,
	.sw_trigger = NULL,
	.sw_is_done = NULL,
	.sw_wait_done = NULL,
	.sw_clr_done = NULL,
};

static struct lcd_tcon_dma_ops_s lcd_tcon_dma_ops_t6w = {
	.status = 0,
	.addr_list = NULL,
	.get_frame_cnt = tcon_lut_dma_get_frame_cnt_t6w,
	.start = tcon_lut_dma_start,
	.stop = tcon_lut_dma_stop,
	.mif_set = tcon_lut_dma_mif_set_t6w,
	.init = tcon_lut_dma_init_t6w,
	.deinit = tcon_lut_dma_deinit,
	.update = lcd_tcon_lut_dma_update,
	.is_done = NULL,
	.clr_done = NULL,
	.wait_done = NULL,
	.sw_mif_set = NULL,
	.sw_trigger = NULL,
	.sw_is_done = NULL,
	.sw_wait_done = NULL,
	.sw_clr_done = NULL,
};

static struct lcd_tcon_dma_ops_s lcd_tcon_dma_ops_t6x = {
	.status = 0,
	.addr_list = NULL,
	.get_frame_cnt = tcon_lut_dma_get_frame_cnt_t6w,
	.start = tcon_lut_dma_start,
	.stop = tcon_lut_dma_stop,
	.mif_set = tcon_lut_dma_mif_set_t6w,
	.init = tcon_lut_dma_init_t6w,
	.deinit = tcon_lut_dma_deinit,
	.update = lcd_tcon_lut_dma_update_t6x,
	.is_done = tcon_aptu_dma_done,
	.clr_done = tcon_clr_dma_done,
	.wait_done = wait_tcon_aptu_dma_done,

	.sw_mif_set = tcon_sw_dma_mif_set,
	.sw_trigger = tcon_sw_dma_trig,
	.sw_is_done = tcon_sw_dma_done,
	.sw_clr_done = tcon_clr_dma_done,
	.sw_wait_done = wait_tcon_sw_dma_done,
};

static struct lcd_tcon_config_s tcon_data_t5d = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T5D,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5D,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5D,
	.reg_table_len = LCD_TCON_TABLE_LEN_T5D,
	.core_reg_start = TCON_CORE_REG_START_T5D,
	.top_reg_base = TCON_TOP_BASE,

	.axi_bank = LCD_TCON_AXI_BANK_T5D,

	.rsv_mem_size    = 0x00402840, /* 4M more */
	.axi_mem_size    = 0x00400000, /* 4M */
	.bin_path_size   = 0x00002800, /* 10K */
	.secure_cfg_size = 0x00000040,

	.vfp_tail = 3,
	.prede_v_dft = 8,
	.prede_h_dft = 8,

	.init_pre_setting = lcd_tcon_init_pre_setting_t5d,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t5d),
	.axi_mem_cfg_tbl = axi_mem_cfg_tbl_t5d,
	.lut_dma_ops = NULL,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t5,
	.tcon_top_init = lcd_tcon_top_set_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
	.tcon_lock_get_status = NULL,
	.tcon_check = lcd_tcon_setting_check_t5d,
};

static struct lcd_tcon_config_s tcon_data_t3 = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T3,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T5,
	.core_reg_start = TCON_CORE_REG_START_T5,
	.top_reg_base = TCON_TOP_BASE,

	.axi_bank = LCD_TCON_AXI_BANK_T5,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K) secure_cfg(64byte)
	 *             |----------------|-------------|-------------|
	 */
	.rsv_mem_size    = 0x00a02840,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.secure_cfg_size = 0x00000040,

	.vfp_tail = 5,
	.prede_v_dft = 8,
	.prede_h_dft = 8,

	.init_pre_setting = lcd_tcon_init_pre_setting_uhd,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t5),
	.axi_mem_cfg_tbl = axi_mem_cfg_tbl_t5,
	.lut_dma_ops = NULL,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t3,
	.tcon_top_init = lcd_tcon_top_set_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
	.tcon_lock_get_status = NULL,
	.tcon_check = lcd_tcon_setting_check_t5,
};

static struct lcd_tcon_config_s tcon_data_t5w = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T5W,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T5,
	.core_reg_start = TCON_CORE_REG_START_T5,
	.top_reg_base = TCON_TOP_BASE,

	.axi_bank = LCD_TCON_AXI_BANK_T5,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K)
	 *             |----------------|-------------|
	 */
	.rsv_mem_size    = 0x00a02840,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.secure_cfg_size = 0x00000040,

	.vfp_tail = 5,
	.prede_v_dft = 8,
	.prede_h_dft = 8,

	.init_pre_setting = lcd_tcon_init_pre_setting_uhd,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t5),
	.axi_mem_cfg_tbl = axi_mem_cfg_tbl_t5,
	.lut_dma_ops = NULL,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t5,
	.tcon_top_init = lcd_tcon_top_set_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
	.tcon_lock_get_status = NULL,
	.tcon_check = lcd_tcon_setting_check_t5,
};

static struct lcd_tcon_config_s tcon_data_t5m = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T5M,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T5,
	.core_reg_start = TCON_CORE_REG_START_T5,
	.top_reg_base = TCON_TOP_BASE,

	.axi_bank = LCD_TCON_AXI_BANK_T5,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K) secure_cfg(64byte)
	 *             |----------------|-------------|-------------|
	 */
	.rsv_mem_size    = 0x00a02840,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.secure_cfg_size = 0x00000040,

	.vfp_tail = 5,
	.prede_v_dft = 8,
	.prede_h_dft = 8,

	.init_pre_setting = lcd_tcon_init_pre_setting_uhd,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t5),
	.axi_mem_cfg_tbl = axi_mem_cfg_tbl_t5,
	.lut_dma_ops = &lcd_tcon_dma_ops_t5m,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t3,
	.tcon_top_init = lcd_tcon_top_set_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
	.tcon_lock_get_status = tcon_get_lock_status_t5m,
	.tcon_check = lcd_tcon_setting_check_t5,
};

static struct lcd_tcon_config_s tcon_data_t3x = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T3X,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T3X,
	.core_reg_start = TCON_CORE_REG_START_T5,
	.top_reg_base = TCON_TOP_BASE,

	.axi_bank = LCD_TCON_AXI_BANK_T3X,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K) secure_cfg(64byte)
	 *             |----------------|-------------|-------------|
	 */
	.rsv_mem_size    = 0x00a02840,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.secure_cfg_size = 0x00000040,

	.vfp_tail = 5,
	.prede_v_dft = 16,
	.prede_h_dft = 6,

	.init_pre_setting = lcd_tcon_init_pre_setting_uhd,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t3x),
	.axi_mem_cfg_tbl = (axi_mem_cfg_tbl_t3x),
	.lut_dma_ops = &lcd_tcon_dma_ops_t3x,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t3x,
	.tcon_top_init = lcd_tcon_top_set_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
	.tcon_lock_get_status = tcon_get_lock_status_t5m,
	.tcon_check = lcd_tcon_setting_check_t5,
};

static struct lcd_tcon_config_s tcon_data_txhd2 = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_TXHD2,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5D,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5D,
	.reg_table_len = LCD_TCON_TABLE_LEN_TXHD2,
	.core_reg_start = TCON_CORE_REG_START_T5D,

	.axi_bank = LCD_TCON_AXI_BANK_TXHD2,

	.rsv_mem_size    = 0x00502840,
	.axi_mem_size    = 0x00500000, /* 5M*/
	.bin_path_size   = 0x00002800, /* 10K */
	.secure_cfg_size = 0x00000040, /* 64byte */

	.vfp_tail = 3,
	.prede_v_dft = 8,
	.prede_h_dft = 8,

	.init_pre_setting = lcd_tcon_init_pre_setting_fhd,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_txhd2),
	.axi_mem_cfg_tbl = axi_mem_cfg_tbl_txhd2,
	.lut_dma_ops = NULL,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t5,
	.tcon_top_init = lcd_tcon_top_set_t5,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_txhd2,
	.tcon_lock_get_status = NULL,
	.tcon_check = lcd_tcon_setting_check_t5d,
};

static struct lcd_tcon_config_s tcon_data_t6d = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T6D,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5D,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5D,
	.reg_table_len = LCD_TCON_TABLE_LEN_T6D,
	.core_reg_start = TCON_CORE_REG_START_T5D,

	.axi_bank = LCD_TCON_AXI_BANK_T6D,

	.rsv_mem_size    = 0x00302840,
	.axi_mem_size    = 0x00300000, /* 3M*/
	.bin_path_size   = 0x00002800, /* 10K */
	.secure_cfg_size = 0x00000040, /* 64byte */

	.vfp_tail = 3,
	.prede_v_dft = 8,
	.prede_h_dft = 8,

	.init_pre_setting = lcd_tcon_init_pre_setting_fhd,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t6d),
	.axi_mem_cfg_tbl = axi_mem_cfg_tbl_t6d,
	.lut_dma_ops = &lcd_tcon_dma_ops_t6d,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t3,
	.tcon_top_init = lcd_tcon_top_set_t6d,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_txhd2,
	.tcon_lock_get_status = NULL,
	.tcon_check = lcd_tcon_setting_check_t5d,
};

static struct lcd_tcon_config_s tcon_data_t6w = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T6W,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T6W,
	.core_reg_start = TCON_CORE_REG_START_T6W,
	.top_reg_base = TCON_TOP_BASE,
	.reg_skip_tbl = tcon_skip_reg_tbl_t6w,
	.reg_skip_tbl_len = ARRAY_SIZE(tcon_skip_reg_tbl_t6w),

	.axi_bank = LCD_TCON_AXI_BANK_T6W,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K) secure_cfg(64byte)
	 *             |----------------|-------------|-------------|
	 */
	.rsv_mem_size    = 0x00a02840,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.secure_cfg_size = 0x00000040,

	.vfp_tail = 5,
	.prede_v_dft = 8,
	.prede_h_dft = 8,

	.init_pre_setting = lcd_tcon_init_pre_setting_uhd,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t5),
	.axi_mem_cfg_tbl = axi_mem_cfg_tbl_t5,
	.lut_dma_ops = &lcd_tcon_dma_ops_t6w,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t3,
	.tcon_top_init = lcd_tcon_top_set_t6d,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
	.tcon_lock_get_status = tcon_get_lock_status_t6w,
	.tcon_check = lcd_tcon_setting_check_t5,
};

static struct lcd_tcon_config_s tcon_data_t6x = {
	.tcon_valid = 0,
	.chip_type = TCON_CHIP_T6X,

	.core_reg_ver = 1, /* new version with header */
	.core_reg_width = LCD_TCON_CORE_REG_WIDTH_T5,
	.reg_table_width = LCD_TCON_TABLE_WIDTH_T5,
	.reg_table_len = LCD_TCON_TABLE_LEN_T6W,
	.core_reg_start = TCON_CORE_REG_START_T6W,
	.top_reg_base = TCON_TOP_BASE,
	.reg_skip_tbl = tcon_skip_reg_tbl_t6x,
	.reg_skip_tbl_len = ARRAY_SIZE(tcon_skip_reg_tbl_t6x),

	.axi_bank = LCD_TCON_AXI_BANK_T3X,

	/*rsv_mem(12M)    axi_mem(10M)   bin_path(10K) secure_cfg(64byte)
	 *             |----------------|-------------|-------------|
	 */
	.rsv_mem_size    = 0x00a02840,
	.axi_mem_size    = 0x00a00000,
	.bin_path_size   = 0x00002800,
	.secure_cfg_size = 0x00000040,

	.vfp_tail = 5,
	.prede_v_dft = 16,
	.prede_h_dft = 6,

	.init_pre_setting = lcd_tcon_init_pre_setting_t6x,
	.init_post_setting = lcd_tcon_init_post_setting,
	.axi_tbl_len = ARRAY_SIZE(axi_mem_cfg_tbl_t3x),
	.axi_mem_cfg_tbl = (axi_mem_cfg_tbl_t3x),
	.lut_dma_ops = &lcd_tcon_dma_ops_t6x,

	.tcon_axi_mem_secure = lcd_tcon_axi_mem_secure_t3,
	.tcon_init_table_pre_proc = lcd_tcon_init_table_pre_proc,
	.tcon_init_table_post_proc = lcd_tcon_init_table_post_proc,
	.tcon_lut_post_proc = lcd_tcon_lut_post_proc,
	.tcon_global_reset = lcd_tcon_global_reset_t3,
	.tcon_top_init = lcd_tcon_top_set_t6d,
	.tcon_enable = lcd_tcon_enable_t5,
	.tcon_disable = lcd_tcon_disable_t5,
	.tcon_lock_get_status = tcon_get_lock_status_t6w,
	.tcon_check = lcd_tcon_setting_check_t5,
};

void lcd_tcon_chip_init(struct lcd_data_s *pdata)
{
	if (!pdata)
		return;

	switch (pdata->chip_type) {
	case LCD_CHIP_T5D:
		lcd_tcon_conf = &tcon_data_t5d;
		break;
	case LCD_CHIP_T3:
		lcd_tcon_conf = &tcon_data_t3;
		break;
	case LCD_CHIP_T5M:
		lcd_tcon_conf = &tcon_data_t5m;
		break;
	case LCD_CHIP_T5W:
		lcd_tcon_conf = &tcon_data_t5w;
		break;
	case LCD_CHIP_T3X:
		lcd_tcon_conf = &tcon_data_t3x;
		break;
	case LCD_CHIP_TXHD2:
		lcd_tcon_conf = &tcon_data_txhd2;
		break;
	case LCD_CHIP_T6D:
		lcd_tcon_conf = &tcon_data_t6d;
		break;
	case LCD_CHIP_T6W:
		lcd_tcon_conf = &tcon_data_t6w;
		break;
	case LCD_CHIP_T6X:
		lcd_tcon_conf = &tcon_data_t6x;
		break;
	default:
		lcd_tcon_conf = NULL;
		break;
	}
}

int lcd_tcon_probe(struct aml_lcd_drv_s *pdrv)
{
	// struct cma *cma;
	// unsigned int mem_size;
	int ret = 0;

	if (!lcd_tcon_conf)
		return 0;

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_MLVDS:
		lcd_tcon_conf->tcon_valid = 1;
		break;
	case LCD_P2P:
		if (pdrv->data->chip_type == LCD_CHIP_T5D ||
		    pdrv->data->chip_type == LCD_CHIP_TXHD2 ||
		    pdrv->data->chip_type == LCD_CHIP_T6D)
			lcd_tcon_conf->tcon_valid = 0;
		else
			lcd_tcon_conf->tcon_valid = 1;
		break;
	default:
		break;
	}
	if (lcd_tcon_conf->tcon_valid == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: invalid tcon for current lcd_type\n", __func__);
		lcd_tcon_reserved_memory_release(pdrv);
		return 0;
	}

	pdrv->sw_vrr.dma_dly_tg = 1;
	mutex_init(&lcd_tcon_dbg_mutex);
	lcd_resource_add(pdrv, LCD_RES_TCON, 0);
	lrm_resource_device_prepare("lcd_tcon");
	lrm_resource_device_prepare("lcd_tcon_data");
	lcd_tcon_reserved_memory_init(pdrv);
	lcd_tcon_mem_config();

#ifdef TCON_DBG_TIME
	dbg_cnt_0 = 120 * 60 * 7;
	dbg_vsync_time = kcalloc(dbg_cnt_0, sizeof(unsigned long long), GFP_KERNEL);
	if (!dbg_vsync_time)
		LCD_ERR(NULL, "%s: dbg_vsync_time error\n", __func__);
#endif
	spin_lock_init(&tcon_local_cfg.multi_list_lock);

	ret = lcd_tcon_get_config(pdrv);

	return ret;
}

int lcd_tcon_remove(struct aml_lcd_drv_s *pdrv)
{
	int i;
	struct lcd_tcon_data_block_header_s *block_header;
	unsigned long flags = 0;

	spin_lock_irqsave(&pdrv->isr_lock, flags);
	if (lcd_tcon_conf) {
		/* lcd_tcon_conf == NULL; */
		lcd_tcon_conf->tcon_valid = 0;
	}
	spin_unlock_irqrestore(&pdrv->isr_lock, flags);

	lcd_tcon_debug_file_remove(&tcon_local_cfg);
	lcd_tcon_fw_finish(pdrv);
	lcd_tcon_rdma_remove(pdrv);
	lcd_tcon_pdf_remove(pdrv);

	kfree(tcon_local_cfg.def_core_info.table);
	kfree(tcon_local_cfg.def_core_info.user_info);
	if (tcon_local_cfg.def_core_info.header->ext_header_size)
		kfree(tcon_local_cfg.def_core_info.ext_header.reg_blk_info);
	kfree(tcon_local_cfg.def_core_info.header);
	memset(&tcon_local_cfg.def_core_info, 0,
		sizeof(tcon_local_cfg.def_core_info));
	memset(&tcon_local_cfg.cur_core_info, 0,
		sizeof(tcon_local_cfg.cur_core_info));

	if (tcon_local_cfg.pdf_data_list.next && !list_empty(&tcon_local_cfg.pdf_data_list))
		lcd_tcon_pdf_clean_data(&tcon_local_cfg.pdf_data_list);

	if (tcon_mm_table.version < 0xff) {
		tcon_mm_table.data_init = NULL;
		tcon_mm_table.lut_valid_flag = 0;
		tcon_mm_table.data_complete = 0;
		lcd_tcon_data_multi_remvoe(&tcon_mm_table);
		if (tcon_mm_table.data_mem_vaddr) {
			for (i = 0; i < tcon_mm_table.block_cnt; i++) {
				if (!tcon_mm_table.data_mem_vaddr[i])
					continue;

				block_header = (struct lcd_tcon_data_block_header_s *)
							tcon_mm_table.data_mem_vaddr[i];
				if (!is_block_type_basic_init(block_header->block_type) &&
					is_block_ctrl_dma(block_header->block_ctrl)) {
					if (lrm_exist())
						lrm_free(tcon_mm_table.data_mem_vaddr[i],
							tcon_mm_table.data_mem_paddr[i]);
				} else {
					kfree(tcon_mm_table.data_mem_vaddr[i]);
				}
				tcon_mm_table.data_mem_vaddr[i] = NULL;
				tcon_mm_table.data_mem_paddr[i] = 0;
			}
			kfree(tcon_mm_table.data_multi);
			tcon_mm_table.data_multi = NULL;

			kfree(tcon_mm_table.data_size);
			tcon_mm_table.data_size = NULL;

			kfree(tcon_mm_table.data_mem_vaddr);
			tcon_mm_table.data_mem_vaddr = NULL;

			kfree(tcon_mm_table.data_mem_paddr);
			tcon_mm_table.data_mem_paddr = NULL;
		}
	}

	if (tcon_rmem.flag == 1) {
		lcd_unmap_phyaddr(tcon_rmem.bin_path_rmem.mem_vaddr);
	} else {
		LCDPR("tcon free memory: base:0x%lx, size:0x%x\n",
			(unsigned long)tcon_rmem.rsv_mem_paddr,
			tcon_rmem.rsv_mem_size);
		memunmap(tcon_rmem.rsv_mem_vaddr);
		tcon_rmem.rsv_mem_vaddr = NULL;
		tcon_rmem.rsv_mem_paddr = 0;
	}

#ifdef TCON_DBG_TIME
	kfree(dbg_vsync_time);
#endif
	for (i = 0; i < tcon_rmem.axi_bank; i++)
		if (tcon_rmem.axi_rmem[i].mem_vaddr)
			lcd_unmap_phyaddr(tcon_rmem.axi_rmem[i].mem_vaddr);

	kfree(tcon_rmem.axi_reg);
	tcon_rmem.axi_reg = NULL;

	kfree(tcon_rmem.axi_rmem);
	tcon_rmem.axi_rmem = NULL;

	return 0;
}
