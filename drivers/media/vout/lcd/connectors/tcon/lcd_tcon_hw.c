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
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/reset.h>
#include <linux/sched/clock.h>
#include <linux/amlogic/media/vout/lcd/lcd_model.h>
#include "../../lcd_common.h"
#include "../../lcd_reg.h"
#include "lcd_tcon.h"
#include "lcd_tcon_pdf.h"
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
#include <linux/amlogic/tee.h>
#endif

struct lcd_tcon_dma_info_s {
	phys_addr_t paddr;
	unsigned int size;
	struct list_head list;
};

int tcon_get_lock_status_t5m(struct aml_lcd_drv_s *pdrv)
{
	return lcd_periphs_getb(pdrv, PADCTRL_GPIOH_I, 0, 1);
}

int tcon_get_lock_status_t6w(struct aml_lcd_drv_s *pdrv)
{
	return lcd_tcon_getb(pdrv, 0x4c8, 3, 1);
}

int tcon_lut_dma_get_frame_cnt(struct aml_lcd_drv_s *pdrv)
{
	return lcd_vcbus_getb(VPU_DMA_RDMIF7_CTRL, 28, 2);
}

int tcon_lut_dma_get_frame_cnt_t6w(struct aml_lcd_drv_s *pdrv)
{
	lcd_wait_vsync(pdrv);
	return lcd_tcon_getb(pdrv, 0x5d3, 6, 1);
}

void tcon_lut_dma_clk_en(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_setb(pdrv, 0x207, 1, 31, 1); //enable dma clk
}

void tcon_lut_dma_start(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_setb(pdrv, 0x207, 1, 31, 1); //enable dma clk
	lcd_tcon_setb(pdrv, 0x367, 1, 0, 14); // trigger delay
	lcd_tcon_setb(pdrv, 0x367, 1, 16, 1); // enable tcon intr
}

void tcon_lut_dma_start_t6d(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_setb(pdrv, 0x36f, 1, 15, 1); // enable tcon interrupt
	lcd_tcon_setb(pdrv, 0x36f, 1, 0, 14); // trigger delay
	lcd_tcon_setb(pdrv, 0x222, 1, 31, 1); // enable dma clk
}

void tcon_lut_dma_stop(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_setb(pdrv, 0x367, 0, 16, 1); // disable tcon intr
}

void tcon_lut_dma_stop_t6d(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_setb(pdrv, 0x36f, 0, 15, 1); // disable tcon intr
}

void tcon_lut_dma_init_t5m(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops)
{
	ops->status = 0;
	ops->addr_list = NULL;
	lcd_vcbus_write(VPU_LUT_DMA_INTR_SEL, 0);//0:sel tcon 1:sel venc2
}

void tcon_lut_dma_init_t3x(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops)
{
	ops->status = 0;
	ops->addr_list = NULL;
	lcd_vcbus_write(VPU_TOP_MISC, 1);
}

void tcon_lut_dma_init_t6w(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops)
{
	ops->status = 0;
	ops->addr_list = NULL;
}

void tcon_lut_dma_deinit(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops)
{
	struct lcd_tcon_dma_info_s *info;
	struct list_head *list_temp;

	if (!pdrv || !ops)
		return;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T5M:
	case LCD_CHIP_T3X:
	case LCD_CHIP_T6W:
	case LCD_CHIP_T6X:
		lcd_tcon_setb(pdrv, 0x367, 0, 16, 1); // disable tcon intr
		lcd_tcon_setb(pdrv, 0x207, 0, 31, 1); // disable dma clk
		break;
	case LCD_CHIP_T6D:
		lcd_tcon_setb(pdrv, 0x36f, 0, 15, 1); // disable tcon intr
		lcd_tcon_setb(pdrv, 0x222, 0, 31, 1); // disable dma clk
		break;
	default:
		break;
	}

	while (ops->addr_list) {
		info = list_entry(ops->addr_list, struct lcd_tcon_dma_info_s, list);

		if (ops->addr_list->next != ops->addr_list) {
			list_temp = ops->addr_list->next;
			list_del(ops->addr_list);
			ops->addr_list = list_temp;
		} else {
			ops->addr_list = NULL;
		}
		kfree(info);
	}
	ops->status = 0;
}

/*
 * must check addr and size before called
 * paddr: 16bytes aligned
 * size: 16bytes aligned
 */
void tcon_lut_dma_mif_set(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size)
{
	unsigned int cmd_cnt = 0;

	cmd_cnt = size >> 4;
	lcd_vcbus_write(VPU_DMA_RDMIF7_BADR0, paddr >> 4);
	lcd_vcbus_write(VPU_DMA_RDMIF7_BADR1, paddr >> 4);
	lcd_vcbus_write(VPU_DMA_RDMIF7_BADR2, paddr >> 4);
	lcd_vcbus_write(VPU_DMA_RDMIF7_BADR3, paddr >> 4);
	//reset index to 0
	lcd_vcbus_write(VPU_DMA_RDMIF7_CTRL,
					1 << 30 | //clr_fcnt reset cnt to //reg_rd7_frm_ini
					0 << 27 | //lut_frm_cnt_ctl cnt range: 0-[0~3] 1-[0~1]
					0 << 26 | //frm_froce force to frm_ini
					0 << 24 | //frm_ini
					2 << 16 | //sel intr from 2=encl/tcon 1=viu1
					0 << 14 | //lut_reg_swap_64bit
					0 << 13 | //lut_reg_little_endian
					cmd_cnt);//lut_reg_stride
}

void tcon_lut_dma_mif_set_t6w(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size)
{
	unsigned int stride;

	if (paddr % 16 || size % 16) {
		LCDERR("%s: paddr or size should be 16 byte aligned\n", __func__);
		return;
	}
	stride = size / 16;
	lcd_tcon_write(pdrv, 0x5d0, 0x00000040);
	lcd_tcon_write(pdrv, 0x5d6, paddr);
	lcd_tcon_setb(pdrv, 0x5d9, stride, 0, 13);
	lcd_tcon_setb(pdrv, 0x5dc, 0, 0, 13);
	lcd_tcon_setb(pdrv, 0x5dc, (stride - 1), 16, 13);
}

void tcon_vrr_dma_mif_set(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size)
{
	unsigned int stride;

	if (paddr % 16 || size % 16) {
		LCDERR("%s: paddr or size should be 16 byte aligned\n", __func__);
		return;
	}
	stride = size >> 4;
	lcd_tcon_write(pdrv, 0x5d0, 0x00000040);// bit7: reg_rdmif_swap_64bit
	lcd_tcon_write(pdrv, 0x5d5, paddr);// reg_lut_dma_vrr_baddr
	lcd_tcon_setb(pdrv, 0x5d8, stride, 16, 13); //reg_lut_dma_vrr_stride
	lcd_tcon_setb(pdrv, 0x5db, 0, 0, 13); //reg_lut_dma_vrr_x_start fix 0
	lcd_tcon_setb(pdrv, 0x5db, (stride - 1), 16, 13); //reg_lut_dma_vrr_x_end
}

void tcon_sw_dma_mif_set(struct aml_lcd_drv_s *pdrv, phys_addr_t paddr, unsigned int size)
{
	unsigned int stride;

	if (paddr % 16 || size % 16) {
		LCDERR("%s: paddr or size should be 16 byte aligned\n", __func__);
		return;
	}
	stride = size >> 4;
	lcd_tcon_write(pdrv, 0x5d0, 0x00000040);// bit7: reg_rdmif_swap_64bit
	lcd_tcon_write(pdrv, 0x5d7, paddr);// reg_lut_dma_vrr_baddr
	lcd_tcon_setb(pdrv, 0x5d9, stride, 16, 13); //reg_lut_dma_vrr_stride
	lcd_tcon_setb(pdrv, 0x5dd, 0, 0, 13); //reg_lut_dma_vrr_x_start fix 0
	lcd_tcon_setb(pdrv, 0x5dd, (stride - 1), 16, 13); //reg_lut_dma_vrr_x_end
}

void tcon_sw_dma_trig(struct aml_lcd_drv_s *pdrv)
{
	//falling edge trig
	lcd_tcon_setb(pdrv, 0x5dd, 0, 31, 1); //reg_lut_dma_sw_start
	lcd_tcon_setb(pdrv, 0x5dd, 1, 31, 1); //reg_lut_dma_sw_start
	lcd_tcon_setb(pdrv, 0x5dd, 0, 31, 1); //reg_lut_dma_sw_start
}

int tcon_sw_dma_done(struct aml_lcd_drv_s *pdrv)
{
	return !!lcd_tcon_getb(pdrv, 0x5d3, 7, 1);
}

int wait_tcon_sw_dma_done(struct aml_lcd_drv_s *pdrv, unsigned int timeout_us)
{
	unsigned int to = timeout_us >> 1;

	while (to-- > 0) {
		udelay(2);
		if (tcon_sw_dma_done(pdrv))
			return 1;
	}

	return 0;
}

static int tcon_sw_dma_valid(struct lcd_tcon_dma_ops_s *dma_ops)
{
	return (dma_ops->sw_clr_done && dma_ops->sw_is_done && dma_ops->sw_mif_set &&
		dma_ops->sw_trigger && dma_ops->sw_wait_done);
}

int tcon_aptu_dma_done(struct aml_lcd_drv_s *pdrv)
{
	return !!lcd_tcon_getb(pdrv, 0x5d3, 6, 1);
}

int wait_tcon_aptu_dma_done(struct aml_lcd_drv_s *pdrv, unsigned int timeout_us)
{
	unsigned int to = timeout_us >> 1;

	while (to-- > 0) {
		udelay(2);
		if (tcon_aptu_dma_done(pdrv))
			return 1;
	}

	return 0;
}

void tcon_clr_dma_done(struct aml_lcd_drv_s *pdrv)
{
	lcd_tcon_setb(pdrv, 0x5d3, 1, 31, 1);
	lcd_tcon_setb(pdrv, 0x5d3, 0, 31, 1);
}

static void lcd_tcon_dma_addr_add(struct lcd_tcon_dma_ops_s *ops,
		phys_addr_t paddr, unsigned int size)
{
	struct lcd_tcon_dma_info_s *dma_info;

	dma_info = kzalloc(sizeof(*dma_info), GFP_ATOMIC);//maybe alloced in isr
	if (!dma_info)
		return;

	dma_info->paddr = paddr;
	dma_info->size = size;
	if (ops->addr_list) {
		list_add_tail(&dma_info->list, ops->addr_list);
	} else {
		dma_info->list.prev = &dma_info->list;
		dma_info->list.next = &dma_info->list;
		ops->addr_list = &dma_info->list;
	}
}

void lcd_tcon_lut_dma_update(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops)
{
	struct lcd_tcon_dma_info_s *dma_info;
	struct list_head *list_temp;

	if (!ops || !ops->mif_set || !ops->start || !ops->stop || !ops->get_frame_cnt)
		return;

	if (ops->status) {
		if (ops->get_frame_cnt(pdrv)) {
			ops->stop(pdrv);
			ops->status = 0;
		} else {
			return;//enabled but not trans finish
		}
	}

	if (!ops->addr_list)
		return;

	dma_info = list_entry(ops->addr_list, struct lcd_tcon_dma_info_s, list);
	ops->mif_set(pdrv, dma_info->paddr, dma_info->size);

	ops->start(pdrv);
	ops->status = 1;

	if (ops->addr_list->next != ops->addr_list) {
		list_temp = ops->addr_list->next;
		list_del(ops->addr_list);
		ops->addr_list = list_temp;
	} else {
		ops->addr_list = NULL;
	}
	kfree(dma_info);
}

void lcd_tcon_lut_dma_update_t6x(struct aml_lcd_drv_s *pdrv, struct lcd_tcon_dma_ops_s *ops)
{
	struct lcd_tcon_dma_info_s *dma_info;
	struct list_head *list_temp;

	if (!ops || !ops->mif_set || !ops->start || !ops->stop || !ops->get_frame_cnt)
		return;

	if (ops->status) {
		if (tcon_aptu_dma_done(pdrv)) {
			ops->stop(pdrv);
			tcon_clr_dma_done(pdrv);
			ops->status = 0;
		} else {
			return;//enabled but not trans finish
		}
	}

	if (!ops->addr_list)
		return;

	dma_info = list_entry(ops->addr_list, struct lcd_tcon_dma_info_s, list);
	ops->mif_set(pdrv, dma_info->paddr, dma_info->size);

	ops->start(pdrv);
	ops->status = 1;

	if (ops->addr_list->next != ops->addr_list) {
		list_temp = ops->addr_list->next;
		list_del(ops->addr_list);
		ops->addr_list = list_temp;
	} else {
		ops->addr_list = NULL;
	}
	kfree(dma_info);
}

void lcd_tcon_lut_post_proc(struct aml_lcd_drv_s *pdrv, unsigned int lut_type)
{
	//below lut enable sample for customer usage, no need on ref board.
/*
 *	switch (lut_type) {
 *	case LCD_TCON_DATA_BLOCK_TYPE_DEMURA_LUT:
 *		lcd_tcon_setb(pdrv, 0x1a3, 1, 31, 1);
 *		//lcd_tcon_setb(pdrv, 0x198, 1, 31, 1); //for fhd tcon
 *		lcd_tcon_setb(pdrv, 0x190, 0, 30, 1);
 *		break;
 *	case LCD_TCON_DATA_BLOCK_TYPE_ACC_LUT:
 *		lcd_tcon_setb(pdrv, 0x222, 0, 14, 1);
 *		break;
 *	case LCD_TCON_DATA_BLOCK_TYPE_DITHER_LUT:
 *		lcd_tcon_setb(pdrv, 0x222, 0, 15, 1);
 *		break;
 *	case LCD_TCON_DATA_BLOCK_TYPE_OD_LUT:
 *		lcd_tcon_setb(pdrv, 0x263, 1, 31, 1);
 *		lcd_tcon_setb(pdrv, 0x240, 0, 1, 1);
 *		break;
 *	case LCD_TCON_DATA_BLOCK_TYPE_LOD_LUT:
 *		lcd_tcon_setb(pdrv, 0x150, 0, 1, 1);
 *		break;
 *	default:
 *		break;
 *	}
 */
}

static void lcd_tcon_core_reg_block_set(struct aml_lcd_drv_s *pdrv,
		struct tcon_core_reg_info_s *core_reg_info)
{
	struct lcd_tcon_reg_block_info_s *reg_info = NULL;
	struct lcd_tcon_init_block_ext_header_s *ext_header;
	unsigned int *table32, offset = 0;
	unsigned char *table8;
	int i, j;

	if (!pdrv || !core_reg_info)
		return;

	ext_header = &core_reg_info->ext_header;
	if (core_reg_info->header->data_byte_width == 4) {
		table32 = (unsigned int *)core_reg_info->table;
		for (j = 0; j < ext_header->reg_blk_num; j++) {
			reg_info = &ext_header->reg_blk_info[j];
			for (i = 0; i < reg_info->len; i++) {
				lcd_tcon_write(pdrv,
					reg_info->start + i, table32[offset++]);
			}
		}
	} else {
		table8 = core_reg_info->table;
		for (j = 0; j < ext_header->reg_blk_num; j++) {
			reg_info = &ext_header->reg_blk_info[j];
			for (i = 0; i < reg_info->len; i++) {
				lcd_tcon_write_byte(pdrv,
					reg_info->start + i, table8[offset++]);
			}
		}
	}
}

static void lcd_tcon_core_reg_continue_set(struct aml_lcd_drv_s *pdrv,
		struct lcd_tcon_config_s *tcon_conf,
		struct tcon_core_reg_info_s *core_reg_info)
{
	struct lcd_tcon_reg_skip_s *skip_reg = NULL;
	unsigned char *table8;
	unsigned int *table32;
	unsigned int len, offset;
	int i, j;

	if (!pdrv || !tcon_conf || !core_reg_info)
		return;

	len = core_reg_info->table_size;
	offset = tcon_conf->core_reg_start;
	if (tcon_conf->core_reg_width == 8) {
		table8 = core_reg_info->table;
		for (i = offset; i < len; i++)
			lcd_tcon_write_byte(pdrv, i, table8[i]);
	} else {
		if (tcon_conf->reg_table_width == 32) {
			len /= 4;
			table32 = (unsigned int *)core_reg_info->table;
			for (j = 0; j < tcon_conf->reg_skip_tbl_len; j++) {  //have skip reg table
				skip_reg = &tcon_conf->reg_skip_tbl[j];
				for (i = offset; i < skip_reg->start_reg && i < len; i++)
					lcd_tcon_write(pdrv, i, table32[i]);
				offset = skip_reg->end_reg + 1;
			}
			for (i = offset; i < len; i++)
				lcd_tcon_write(pdrv, i, table32[i]);
		} else {
			table8 = core_reg_info->table;
			for (i = offset; i < len; i++)
				lcd_tcon_write(pdrv, i, table8[i]);
		}
	}
}

void lcd_tcon_core_reg_set(struct aml_lcd_drv_s *pdrv,
			   struct lcd_tcon_config_s *tcon_conf,
			   struct tcon_mem_map_table_s *mm_table,
			   struct tcon_core_reg_info_s *core_reg_info)
{
	unsigned int p2p_type, cspi_alpha;
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return;

	if (!tcon_conf || !mm_table || !core_reg_info) {
		LCDERR("%s: table is NULL\n", __func__);
		return;
	}

	if (pdrv->config_check_en == 0) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("config_check disabled\n");
	} else {
		ret = lcd_tcon_init_setting_check(pdrv, &pdrv->curr_dev->dev_cfg.timing.act_timing,
				core_reg_info);
		if (ret & 0x1) {
			LCDERR("tcon: %s: tcon setting check fatal error!\n", __func__);
			return;
		}
	}

	p2p_type = pdrv->curr_dev->dev_cfg.control.p2p_cfg.p2p_type & 0x1f;
	cspi_alpha = pdrv->curr_dev->dev_cfg.control.p2p_cfg.cspi_alpha;

	if (core_reg_info->header->ext_header_size &&
			core_reg_info->ext_header.reg_blk_num) {
		lcd_tcon_core_reg_block_set(pdrv, core_reg_info);
	} else {
		lcd_tcon_core_reg_continue_set(pdrv, tcon_conf,
			core_reg_info);
	}

	if (p2p_type == P2P_CSPI_NEW) {
		lcd_tcon_setb(pdrv, 0x496, cspi_alpha, 21, 8);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("%s: set cspi new coding array_len=%#x\n", __func__, cspi_alpha);
	}

	if (tcon_conf->tcon_init_table_post_proc)
		tcon_conf->tcon_init_table_post_proc(pdrv);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s finish\n", __func__);
}

static void lcd_tcon_data_init_set(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	struct lcd_tcon_init_block_header_s *init_header;
	struct lcd_tcon_init_block_ext_header_s init_ext_header;
	struct lcd_detail_timing_s *timing = NULL;
	unsigned char *core_reg_table;
	unsigned int len = 0;  //header_size + ext_header_size + core_reg_size
	unsigned int ext_core_tbl_len = 0;
	int temp = -1;

	if (!tcon_conf || !mm_table || !local_cfg || !data_buf)
		return;

	timing = pdrv->curr_dev->dev_cfg.timing.base_timing;
	if (!timing)
		return;

	init_header = (struct lcd_tcon_init_block_header_s *)data_buf;
	core_reg_table = data_buf + init_header->header_size + init_header->ext_header_size;
	ext_core_tbl_len = tcon_conf->reg_table_len;
	memset(&init_ext_header, 0, sizeof(init_ext_header));
	if (init_header->ext_header_size) {
		memcpy(&init_ext_header, data_buf + init_header->header_size,
			sizeof(init_ext_header));
		if (init_ext_header.reg_blk_num) {
			init_ext_header.reg_blk_info = (struct lcd_tcon_reg_block_info_s *)
				(data_buf + TCON_EXT_BLK_INFO_PRE_OFFSET);
		}
		lcd_tcon_ext_header_check(&init_ext_header);
		temp = lcd_tcon_get_core_size(tcon_conf,
			init_header, &init_ext_header);
		ext_core_tbl_len = temp < 0 ? ext_core_tbl_len : temp;
	}
	len = ext_core_tbl_len + init_header->header_size
		+ init_header->ext_header_size;
	switch (init_header->block_ctrl) {
	case LCD_TCON_DATA_CTRL_FLAG_UFR:
		if (timing->h_active == init_header->h_active &&
		    timing->v_active == init_header->v_active) {
			if (init_header->ext_header_size) {
				if (timing->frame_rate < init_ext_header.framerate_min ||
				    timing->frame_rate > init_ext_header.framerate_max)
					break;
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
					LCDPR("%s: ufr match framerate range %d~%d\n", __func__,
						init_ext_header.framerate_min,
						init_ext_header.framerate_max);
				}
			}
			lcd_tcon_init_data_version_update(&local_cfg->cur_core_info,
				init_header->version);
			local_cfg->cur_core_info.user_info = NULL;
			if (init_header->block_size > len)
				local_cfg->cur_core_info.user_info = (char *)(data_buf + len);
			local_cfg->cur_core_info.header = init_header;
			local_cfg->cur_core_info.ext_header = init_ext_header;
			local_cfg->cur_core_info.table = core_reg_table;
			local_cfg->cur_core_info.table_size = ext_core_tbl_len;
			LCDPR("%s: ufr %dx%d@%dhz init, bin_ver:%s\n",
				__func__, init_header->h_active,
				init_header->v_active, timing->frame_rate,
				local_cfg->cur_core_info.bin_ver);
			lcd_tcon_core_reg_set(pdrv, tcon_conf, mm_table, &local_cfg->cur_core_info);
		}
		break;
	default:
		break;
	}
}

void lcd_tcon_axi_rmem_lut_load(struct aml_lcd_drv_s *pdrv,
		unsigned int index, unsigned char *buf, unsigned int size)
{
	struct tcon_rmem_s *tcon_rmem = get_lcd_tcon_rmem();
	unsigned long paddr;
	unsigned char *vaddr = NULL;
	unsigned long mem_paddr_min = 0, mem_paddr_max = 0;
	unsigned long sec_paddr_min = 0, sec_paddr_max = 0;

	if (!tcon_rmem || !tcon_rmem->axi_rmem) {
		LCDERR("axi_rmem is NULL\n");
		return;
	}
	if (index >= tcon_rmem->axi_bank) {
		LCDERR("axi_rmem index %d invalid\n", index);
		return;
	}
	if (tcon_rmem->axi_rmem[index].mem_size < size) {
		LCDERR("axi_mem[%d] size 0x%x is not enough, need 0x%x\n",
		       index, tcon_rmem->axi_rmem[index].mem_size, size);
		return;
	}

	mem_paddr_min = tcon_rmem->axi_rmem[index].mem_paddr;
	mem_paddr_max = tcon_rmem->axi_rmem[index].mem_paddr +
		tcon_rmem->axi_rmem[index].mem_size - 1;
	sec_paddr_min = tcon_rmem->secure_axi_rmem.mem_paddr;
	sec_paddr_max = tcon_rmem->secure_axi_rmem.mem_paddr +
		tcon_rmem->secure_axi_rmem.mem_size - 1;
	if (tcon_rmem->secure_axi_rmem.sec_protect &&
		!(mem_paddr_max < sec_paddr_min || sec_paddr_max < mem_paddr_min)) {
		LCDERR("%s: axi_mem[%d] can't use secure memory\n", __func__, index);
		return;
	}

	paddr = tcon_rmem->axi_rmem[index].mem_paddr;
	if (!tcon_rmem->axi_rmem[index].mem_vaddr) {
		vaddr = lcd_tcon_paddrtovaddr(paddr, size);
		if (!vaddr) {
			LCDERR("tcon axi_rmem[%d] mapping failed: 0x%lx\n", index, paddr);
			return;
		}

		memcpy(vaddr, buf, size);

		if (tcon_rmem->flag == 1)
			lcd_unmap_phyaddr(vaddr);
		else if (tcon_rmem->flag == 2)
			iounmap(vaddr);

	} else {
		vaddr = tcon_rmem->axi_rmem[index].mem_vaddr;
		memcpy(vaddr, buf, size);
	}

	if (tcon_rmem->flag == 1)
		lcd_tcon_mem_sync(pdrv, paddr, size);
}

static int lcd_tcon_wr_n_data_write(struct aml_lcd_drv_s *pdrv,
				    struct lcd_tcon_data_part_wr_n_s *wr_n,
				    unsigned char *p,
				    unsigned int n,
				    unsigned int reg,
				    unsigned int init_flag)
{
	unsigned int k, data, d;

	if (wr_n->reg_inc) {
		for (k = 0; k < wr_n->data_cnt; k++) {
			data = 0;
			for (d = 0; d < wr_n->reg_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (wr_n->reg_data_byte == 1)
				lcd_tcon_write_byte(pdrv, (reg + k), data);
			else
				lcd_tcon_write(pdrv, (reg + k), data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_TCON) && init_flag) {
				LCDPR("%s: write reg 0x%x=0x%x\n",
				      __func__, (reg + k), data);
			}
			n += wr_n->reg_data_byte;
		}
	} else {
		for (k = 0; k < wr_n->data_cnt; k++) {
			data = 0;
			for (d = 0; d < wr_n->reg_data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (wr_n->reg_data_byte == 1)
				lcd_tcon_write_byte(pdrv, reg, data);
			else
				lcd_tcon_write(pdrv, reg, data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_TCON) && init_flag) {
				LCDPR("%s: write reg 0x%x=0x%x\n",
				      __func__, reg, data);
			}
			n += wr_n->reg_data_byte;
		}
	}

	return 0;
}

int lcd_tcon_data_common_parse_set(struct aml_lcd_drv_s *pdrv, unsigned char *data_buf,
			phys_addr_t paddr, int init_flag, unsigned int data_index)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_tcon_local_cfg_s *tcon_local = get_lcd_tcon_local_cfg();
	struct lcd_tcon_data_block_header_s *block_header;
	struct lcd_tcon_data_block_ext_header_s *ext_header;
	unsigned char *p, *part_pos;
	unsigned short part_cnt;
	unsigned char part_type, part_mapping_byte;
	unsigned int size = 0, reg, data, mask, temp, reg_base = 0;
	union lcd_tcon_data_part_u data_part;
	unsigned int data_offset, offset, i, j, k, d, m, n, step = 0;
	struct lcd_tcon_pdf_data_s *pdf_data = NULL;
	unsigned int reg_cnt, reg_byte, data_cnt, data_byte;
	unsigned short block_ctrl_flag;
	unsigned char exe_in_isr = 0, exe_ignore = 0;
	unsigned int part_start_offset, part_offset, temp_size = 0;
	struct lcd_tcon_dma_ops_s *dma_ops = NULL;
	u64 temp_addr = 0;
	phys_addr_t vrr_paddr;
	int ret;
	int vrr_data_valid = 0;
	struct lcd_tcon_vrr_data_s *vrr_data = NULL;

	if (!tcon_local || !tcon_conf)
		return -1;

	reg_base = tcon_conf->core_reg_start;

	block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
	p = data_buf + block_header->header_size;
	ext_header = (struct lcd_tcon_data_block_ext_header_s *)p;
	part_cnt = ext_header->part_cnt;
	part_mapping_byte = ext_header->part_mapping_byte;
	part_pos = p + LCD_TCON_DATA_BLOCK_EXT_HEADER_SIZE_PRE;

	block_ctrl_flag = block_header->block_ctrl;
	part_start_offset = block_header->header_size + block_header->ext_header_size;
	for (i = 0; i < part_cnt; i++) {
		p = part_pos + i * part_mapping_byte;
		part_offset = 0;
		for (j = 0; j < part_mapping_byte; j++)
			part_offset |= (p[j] << j * 8);
		data_offset = part_offset + part_start_offset;
		p = data_buf + data_offset;
		part_type = p[LCD_TCON_DATA_PART_NAME_SIZE + 3];
		exe_in_isr = p[LCD_TCON_DATA_PART_NAME_SIZE + 2];
		exe_ignore = (exe_in_isr & LCD_TCON_DATA_PART_FLAG_CMD_IGNORE_ISR) &&
					(init_flag == 0);

		if (((lcd_debug_print_flag & LCD_DBG_PR_ADV) && init_flag == 1) ||
			((lcd_debug_print_flag & LCD_DBG_PR_ADV) &&
			(lcd_debug_print_flag & LCD_DBG_PR_ISR) && init_flag == 0)) {
			LCDPR("%s: part[%d] start: %s, pos=0x%x, type=0x%02x\n",
			      __func__, step, p, part_offset, part_type);
		}

		switch (part_type) {
		case LCD_TCON_DATA_PART_TYPE_CONTROL:
			block_ctrl_flag = 0;
			data_part.ctrl = (struct lcd_tcon_data_part_ctrl_s *)p;
			offset = LCD_TCON_DATA_PART_CTRL_SIZE_PRE;
			size = offset + (data_part.ctrl->data_cnt *
					 data_part.ctrl->data_byte_width);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (init_flag == 0)
				break;
			if (block_header->block_ctrl == 0)
				break;
			if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
				LCDPR("%s: block %s: ctrl data_flag=0x%x, ctrl_method=0x%x\n",
				      __func__, block_header->name,
				      data_part.ctrl->ctrl_data_flag,
				      data_part.ctrl->ctrl_method);
			}
			if (block_header->block_ctrl != data_part.ctrl->ctrl_data_flag) {
				LCDERR("%s: block %s: block_ctrl %x mismatch ctrl_data_flag %x\n",
					__func__, block_header->name, block_header->block_ctrl,
					data_part.ctrl->ctrl_data_flag);
				return -1;
			}
			if (data_part.ctrl->ctrl_data_flag & LCD_TCON_DATA_CTRL_FLAG_MULTI) {
				ret = lcd_tcon_data_multi_init_check(pdrv, block_header->block_type,
						data_part.ctrl, (p + offset), data_index);
				if (ret) //not match, exit
					return 1;
			}
			//only set once
			if (block_header->block_type == LCD_TCON_DATA_BLOCK_TYPE_VRR_LUT) {
				vrr_data = &tcon_local->vrr_data;
				lcd_tcon_data_parse_vrr(vrr_data, p + offset,
					data_part.ctrl->data_cnt, data_part.ctrl->data_byte_width);
				if (vrr_data->support)
					vrr_data_valid = 1;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_WR_N:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.wr_n = (struct lcd_tcon_data_part_wr_n_s *)p;
			offset = LCD_TCON_DATA_PART_WR_N_SIZE_PRE;
			size = offset + (data_part.wr_n->reg_cnt *
					 data_part.wr_n->reg_addr_byte) +
					(data_part.wr_n->data_cnt *
					 data_part.wr_n->reg_data_byte);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			reg_cnt = data_part.wr_n->reg_cnt;
			reg_byte = data_part.wr_n->reg_addr_byte;
			m = offset; /*for reg*/
			/*for data*/
			n = m + (reg_cnt * reg_byte);
			for (j = 0; j < reg_cnt; j++) {
				reg = 0;
				for (d = 0; d < reg_byte; d++)
					reg |= (p[m + d] << (d * 8));
				if (reg < reg_base)
					goto lcd_tcon_data_common_parse_set_err_reg;
				lcd_tcon_wr_n_data_write(pdrv, data_part.wr_n,
							 p, n, reg, init_flag);
				m += reg_byte;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_WR_DDR:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.wr_ddr = (struct lcd_tcon_data_part_wr_ddr_s *)p;
			offset = LCD_TCON_DATA_PART_WR_DDR_SIZE_PRE;
			m = data_part.wr_ddr->data_cnt *
				data_part.wr_ddr->data_byte;
			size = offset + m;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			n = data_part.wr_ddr->axi_buf_id;
			lcd_tcon_axi_rmem_lut_load(pdrv, n, &p[offset], m);
			break;
		case LCD_TCON_DATA_PART_TYPE_WR_MASK:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.wr_mask = (struct lcd_tcon_data_part_wr_mask_s *)p;
			offset = LCD_TCON_DATA_PART_WR_MASK_SIZE_PRE;
			size = offset + data_part.wr_mask->reg_addr_byte +
				(2 * data_part.wr_mask->reg_data_byte);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			reg_byte = data_part.wr_mask->reg_addr_byte;
			data_byte = data_part.wr_mask->reg_data_byte;
			m = offset; /* for reg */
			/* for data */
			n = m + reg_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_byte;
			data = 0;
			for (d = 0; d < data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				lcd_tcon_update_bits_byte(pdrv, reg, mask, data);
			else
				lcd_tcon_update_bits(pdrv, reg, mask, data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_TCON) && init_flag) {
				LCDPR("%s: write reg 0x%x, data=0x%x, mask=0x%x\n",
				      __func__, reg, mask, data);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_RD_MASK:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.rd_mask = (struct lcd_tcon_data_part_rd_mask_s *)p;
			offset = LCD_TCON_DATA_PART_RD_MASK_SIZE_PRE;
			size = offset + data_part.rd_mask->reg_addr_byte +
				data_part.rd_mask->reg_data_byte;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			reg_byte = data_part.rd_mask->reg_addr_byte;
			data_byte = data_part.rd_mask->reg_data_byte;
			m = offset; /* for reg */
			/* for data */
			n = m + reg_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			if (data_byte == 1) {
				data = lcd_tcon_read_byte(pdrv, reg) & mask;
				if ((lcd_debug_print_flag & LCD_DBG_PR_TCON) && init_flag)
					LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
					      __func__, reg, data, mask);
			} else {
				data = lcd_tcon_read(pdrv, reg) & mask;
				if ((lcd_debug_print_flag & LCD_DBG_PR_TCON) && init_flag)
					LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
					      __func__, reg, data, mask);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_CHK_WR_MASK:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.chk_wr_mask = (struct lcd_tcon_data_part_chk_wr_mask_s *)p;
			offset = LCD_TCON_DATA_PART_CHK_WR_MASK_SIZE_PRE;
			size = offset + data_part.chk_wr_mask->reg_chk_addr_byte +
				//include mask
				data_part.chk_wr_mask->reg_chk_data_byte *
				(data_part.chk_wr_mask->data_chk_cnt + 1) +
				data_part.chk_wr_mask->reg_wr_addr_byte +
				//include mask, default
				data_part.chk_wr_mask->reg_wr_data_byte *
				(data_part.chk_wr_mask->data_chk_cnt + 2);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			reg_byte = data_part.chk_wr_mask->reg_chk_addr_byte;
			data_cnt = data_part.chk_wr_mask->data_chk_cnt;
			data_byte = data_part.chk_wr_mask->reg_chk_data_byte;
			m = offset; /* for reg */
			n = m + reg_byte; /* for data */
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				temp = lcd_tcon_read_byte(pdrv, reg) & mask;
			else
				temp = lcd_tcon_read(pdrv, reg) & mask;
			if ((lcd_debug_print_flag & LCD_DBG_PR_TCON) && init_flag) {
				LCDPR("%s read reg 0x%04x = 0x%02x, mask = 0x%02x\n",
				      __func__, reg, temp, mask);
			}
			n += data_byte;
			for (j = 0; j < data_cnt; j++) {
				data = 0;
				for (d = 0; d < data_byte; d++)
					data |= (p[n + d] << (d * 8));
				if ((data & mask) == temp)
					break;
				n += data_byte;
			}
			k = j;

			/* for reg */
			m = offset + reg_byte + data_byte * (data_cnt + 1);
			/* for data */
			n = m + data_part.chk_wr_mask->reg_wr_addr_byte;
			reg_byte = data_part.chk_wr_mask->reg_wr_addr_byte;
			data_byte = data_part.chk_wr_mask->reg_wr_data_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_byte;
			n += data_byte * k;
			data = 0;
			for (d = 0; d < data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				lcd_tcon_update_bits_byte(pdrv, reg, mask, data);
			else
				lcd_tcon_update_bits(pdrv, reg, mask, data);
			if ((lcd_debug_print_flag & LCD_DBG_PR_TCON) && init_flag) {
				LCDPR("%s: write reg 0x%x, data=0x%x, mask=0x%x\n",
				      __func__, reg, mask, data);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_CHK_EXIT:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.chk_exit = (struct lcd_tcon_data_part_chk_exit_s *)p;
			offset = LCD_TCON_DATA_PART_CHK_EXIT_SIZE_PRE;
			size = offset + data_part.chk_exit->reg_addr_byte +
				(2 * data_part.chk_exit->reg_data_byte);
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			reg_byte = data_part.chk_exit->reg_addr_byte;
			data_byte = data_part.chk_exit->reg_data_byte;
			m = offset; /* for reg */
			/* for data */
			n = m + reg_byte;
			reg = 0;
			for (d = 0; d < reg_byte; d++)
				reg |= (p[m + d] << (d * 8));
			if (reg < reg_base)
				goto lcd_tcon_data_common_parse_set_err_reg;
			mask = 0;
			for (d = 0; d < data_byte; d++)
				mask |= (p[n + d] << (d * 8));
			n += data_byte;
			data = 0;
			for (d = 0; d < data_byte; d++)
				data |= (p[n + d] << (d * 8));
			if (data_byte == 1)
				ret = lcd_tcon_check_bits_byte(pdrv, reg, mask, data);
			else
				ret = lcd_tcon_check_bits(pdrv, reg, mask, data);
			if (ret) {
				LCDPR("%s: block %s data_part %d check exit\n",
				      __func__, block_header->name, i);
				return 0;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_DELAY:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.delay = (struct lcd_tcon_data_part_delay_s *)p;
			size = LCD_TCON_DATA_PART_DELAY_SIZE;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			if (init_flag == 0) /* calling by vsync isr */
				break;
			if (data_part.delay->delay_us > 20000) {
				m = data_part.delay->delay_us / 1000;
				n = data_part.delay->delay_us % 1000;
				lcd_delay_ms(m);
				if (n > 20)
					usleep_range(n, n + 1);
				else if (n > 0)
					udelay(n);
			} else {
				n = data_part.delay->delay_us;
				if (n > 20)
					usleep_range(n, n + 1);
				else if (n > 0)
					udelay(n);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_PARAM:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.param = (struct lcd_tcon_data_part_param_s *)p;
			offset = LCD_TCON_DATA_PART_PARAM_SIZE_PRE;
			size = offset + data_part.param->param_size;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_DMA:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			data_part.dma = (struct lcd_tcon_data_part_dma_s *)p;

			size = LCD_TCON_DATA_PART_DMA_SIZE_PRE + data_part.dma->dma_data_size;
			if ((size + data_offset) > block_header->block_size)
				goto lcd_tcon_data_common_parse_set_err_size;

			offset = p - data_buf + LCD_TCON_DATA_PART_DMA_SIZE_PRE;
			if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
				LCDPR("%s: vaddr:0x%px, paddr:0x%llx,\n"
					"offset:0x%x, size:0x%x\n",
					__func__, data_buf, (unsigned long long)paddr,
					offset, data_part.dma->dma_data_size);
			paddr += offset;
			if (!paddr || paddr & 0xf || data_part.dma->dma_data_size & 0xf)
				break;

			dma_ops = tcon_conf->lut_dma_ops;
			if (!dma_ops)
				break;
			if (vrr_data_valid) {
				vrr_data = &tcon_local->vrr_data;
				if (!vrr_data->support || !vrr_data->part)
					break;
				/* uboot alloced this memory, no need once lcd disable->enable */
				if (lrm_get_by_name("tcon_vrr_data", &temp_addr, &temp_size) == 0) {
					vrr_paddr = (phys_addr_t)temp_addr;
					lrm_free(NULL, vrr_paddr);
				}
				vrr_data->en = 1;
				vrr_data->paddr = paddr;
				vrr_data->size = data_part.dma->dma_data_size;
				vrr_data->part_size = vrr_data->size / vrr_data->part;
				tcon_fr_detect_config(pdrv, 0, vrr_data->fr_level,
							vrr_data->fr_count, 1);
				tcon_vrr_dma_mif_set(pdrv, vrr_data->paddr, vrr_data->part_size);
				/* fr_det will not work at power on, need use sw_dma trig manually*/
				ret = lcd_tcon_vrr_fr_sw_match(pdrv, vrr_data);
				tcon_lut_dma_clk_en(pdrv);
				if (tcon_sw_dma_valid(dma_ops)) {
					dma_ops->sw_mif_set(pdrv,
							vrr_data->paddr + ret * vrr_data->part_size,
							vrr_data->part_size);
					dma_ops->sw_clr_done(pdrv);
					dma_ops->sw_trigger(pdrv);
					ret = dma_ops->sw_wait_done(pdrv, 3000);
					dma_ops->sw_clr_done(pdrv);
				}
				tcon_fr_detect_enable(pdrv, 1);
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s: vrr_dma: paddr:0x%llx, size:0x%x %s\n",
						__func__, (unsigned long long)vrr_data->paddr,
						vrr_data->part_size, ret ? "ok" : "fail");
				break;
			}

			if (init_flag) { //power on stage trigger now
				if (tcon_sw_dma_valid(dma_ops)) {
					dma_ops->sw_mif_set(pdrv, paddr,
						data_part.dma->dma_data_size);
					tcon_lut_dma_clk_en(pdrv);
					dma_ops->sw_clr_done(pdrv);
					dma_ops->sw_trigger(pdrv);
					ret = dma_ops->sw_wait_done(pdrv, 17000);//60hz 1frame
					dma_ops->sw_clr_done(pdrv);
					if (!ret) {
						LCDPR("%s: sw_dma: paddr:0x%llx, size:0x%x %s\n",
							__func__, (unsigned long long)paddr,
							data_part.dma->dma_data_size,
							ret ? "ok" : "fail");
					}
					break;
				}
			}
			if (pdrv->sw_vrr.en)
				pdrv->sw_vrr.dma_dly = pdrv->sw_vrr.dma_dly_tg;

			lcd_tcon_dma_addr_add(dma_ops, paddr, data_part.dma->dma_data_size);
			if (exe_ignore) {
				if ((lcd_debug_print_flag & LCD_DBG_PR_ADV))
					LCDPR("%s part[%d] ignored\n", __func__, step);
				break;
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_PDF_ACTION:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			if (tcon_local->pdf_list_load_flag)
				break;
			pdf_data = kzalloc(sizeof(*pdf_data), GFP_KERNEL);
			if (pdf_data) {
				pdf_data->data.pdf_action =
					(struct lcd_tcon_data_part_pdf_action_s *)p;
				pdf_data->data_type = part_type;
				offset = LCD_TCON_DATA_PART_PDF_ACT_SIZE_PRE;
				size = offset + pdf_data->data.pdf_action->dst_cnt + 2;
				list_add_tail(&pdf_data->list, &tcon_local->pdf_data_list);
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV) {
					int dst_i = 0;
					struct lcd_tcon_data_part_pdf_action_s *action =
						pdf_data->data.pdf_action;

					LCDPR("Add ACTION src_id=%#x\n",
						pdf_data->data.pdf_action->src_id);
					LCDPR("Add ACTION dst_id=");
					for (dst_i = 0; dst_i < action->dst_cnt; dst_i++)
						pr_info("%d,", action->dst_array[dst_i]);
					pr_info("\n");
				}
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_PDF_ACTION_DST:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			if (tcon_local->pdf_list_load_flag)
				break;
			pdf_data = kzalloc(sizeof(*pdf_data), GFP_KERNEL);
			if (pdf_data) {
				pdf_data->data.pdf_dst = (struct lcd_tcon_data_part_pdf_dst_s *)p;
				pdf_data->data_type = part_type;
				offset = LCD_TCON_DATA_PART_PDF_DST_SIZE_PRE;
				size = offset + 10;
				list_add_tail(&pdf_data->list, &tcon_local->pdf_data_list);
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
					LCDPR("Add DST id=%#x,mode=%d,idx=%d,mask=%#x,val=%#x\n",
						pdf_data->data.pdf_dst->part_id,
						pdf_data->data.pdf_dst->mode,
						pdf_data->data.pdf_dst->index,
						pdf_data->data.pdf_dst->data_mask,
						pdf_data->data.pdf_dst->data_value);
			}
			break;
		case LCD_TCON_DATA_PART_TYPE_PDF_ACTION_SRC:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			if (tcon_local->pdf_list_load_flag)
				break;
			pdf_data = kzalloc(sizeof(*pdf_data), GFP_KERNEL);
			if (pdf_data) {
				pdf_data->data.pdf_src = (struct lcd_tcon_data_part_pdf_src_s *)p;
				pdf_data->data_type = part_type;
				offset = LCD_TCON_DATA_PART_PDF_SRC_SIZE_PRE;
				size = offset + 13;
				list_add_tail(&pdf_data->list, &tcon_local->pdf_data_list);
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
					LCDPR("Add SRC id=%#x,mode=%d,reg=%#x,mask=%#x,val=%#x\n",
						pdf_data->data.pdf_src->part_id,
						pdf_data->data.pdf_src->data_check_mode,
						pdf_data->data.pdf_src->reg_addr,
						pdf_data->data.pdf_src->data_mask,
						pdf_data->data.pdf_src->data_value);
			}
			break;
		default:
			if (block_ctrl_flag)
				goto lcd_tcon_data_common_parse_set_ctrl_err;
			LCDERR("%s: unsupport data part type 0x%02x\n",
			       __func__, part_type);
			break;
		}
		if (((lcd_debug_print_flag & LCD_DBG_PR_ADV) && init_flag == 1) ||
			((lcd_debug_print_flag & LCD_DBG_PR_ADV) &&
			(lcd_debug_print_flag & LCD_DBG_PR_ISR) && init_flag == 0)) {
			LCDPR("%s: part[%d] end: %s, type=0x%02x, size=%d\n",
			      __func__, step, p, part_type, size);
		}
		step++;
	}

	if (((lcd_debug_print_flag & LCD_DBG_PR_NORMAL) && init_flag) ||
	    (lcd_debug_print_flag & LCD_DBG_PR_ADV)) {
		LCDPR("%s: %s, part_cnt: %d done\n", __func__, block_header->name, part_cnt);
	}

	return 0;

lcd_tcon_data_common_parse_set_ctrl_err:
	LCDERR("%s: block %s: need control part\n", __func__, block_header->name);
	return -1;

lcd_tcon_data_common_parse_set_err_reg:
	LCDERR("%s: block %s part[%d]: reg 0x%04x error\n",
	       __func__, block_header->name, step, reg);
	return -1;

lcd_tcon_data_common_parse_set_err_size:
	LCDERR("%s: block %s part[%d]: size error\n",
	       __func__, block_header->name, step);
	return -1;
}

int lcd_tcon_data_set(struct aml_lcd_drv_s *pdrv, struct tcon_mem_map_table_s *mm_table)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_tcon_data_block_header_s *block_header;
	unsigned char *data_buf;
	unsigned int temp_crc32;
	unsigned int i, chk_size;
	phys_addr_t paddr = 0;
	int ret;

	if (!mm_table->data_mem_vaddr) {
		LCDERR("%s: data_mem error\n", __func__);
		return -1;
	}

	for (i = 0; i < mm_table->block_cnt; i++) {
		if (mm_table->effect_id < mm_table->block_cnt && mm_table->effect_id != i)
			continue;
		if (!mm_table->data_mem_vaddr[i]) {
			LCDERR("%s: data_mem_vaddr[%d] is null\n", __func__, i);
			continue;
		}
		data_buf = mm_table->data_mem_vaddr[i];
		paddr = mm_table->data_mem_paddr[i];
		block_header = (struct lcd_tcon_data_block_header_s *)data_buf;
		if (block_header->block_size < sizeof(struct lcd_tcon_data_block_header_s)) {
			LCDERR("%s: block[%d] size 0x%x is invalid\n",
			       __func__, i, block_header->block_size);
			continue;
		}
		chk_size = block_header->block_size - 4;
		temp_crc32 = cal_CRC32(0, &data_buf[4], chk_size);
		if (temp_crc32 != block_header->crc32) {
			LCDERR("%s: block[%d] %s: data crc error\n",
				__func__, i, block_header->name);
			continue;
		}

		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("%s: block[%d] %s: size=0x%x, type=0x%02x, ctrl=0x%x\n",
			      __func__, i,
			      block_header->name,
			      block_header->block_size,
			      block_header->block_type,
			      block_header->block_ctrl);
		}

		if (mm_table->effect_type != 0xff &&
		    mm_table->effect_type != block_header->block_type)
			continue;
		/* apply data */
		switch (block_header->block_type) {
		case LCD_TCON_DATA_BLOCK_TYPE_BASIC_INIT:
			lcd_tcon_data_init_set(pdrv, data_buf);
			continue;
		case LCD_TCON_DATA_BLOCK_TYPE_OD_LUT:
			// skip od stage when memory is not ready
			if (!lcd_tcon_mem_od_is_valid()) {
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
					LCDERR("%s: bypass block[%d]: type 0x%x\n",
						__func__, i, block_header->block_type);
				continue;
			}
			break;
		case LCD_TCON_DATA_BLOCK_TYPE_DEMURA_LUT:
		case LCD_TCON_DATA_BLOCK_TYPE_DEMURA_SET:
			// skip demura stage when memory is not ready
			if (!lcd_tcon_mem_demura_is_valid()) {
				if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
					LCDERR("%s: bypass block[%d]: type 0x%x\n",
						__func__, i, block_header->block_type);
				continue;
			}
			break;
		default:
			break;
		}

		ret = lcd_tcon_data_common_parse_set(pdrv, data_buf, paddr, 1, i);
		if (ret == 0) {
			if (tcon_conf && tcon_conf->tcon_lut_post_proc)
				tcon_conf->tcon_lut_post_proc(pdrv, block_header->block_type);
		}
	}

	LCDPR("%s finish\n", __func__);
	return 0;
}

int lcd_tcon_top_set_t5(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int p2p_type;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_debug: %s\n", __func__);

	lcd_tcon_write(pdrv, TCON_CLK_CTRL, 0x001f);
	if (pconf->basic.lcd_type == LCD_P2P) {
		p2p_type = pconf->control.p2p_cfg.p2p_type & 0x1f;
		switch (p2p_type) {
		case P2P_CHPI:
		case P2P_USIT:
			lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8399);
			break;
		default:
			lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8b99);
			break;
		}
	} else {
		lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0x8b99);
	}
	lcd_tcon_write(pdrv, TCON_PLLLOCK_CNTL, 0x0037);
	//lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x003f);
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x0000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL0, 0x33fff000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL1, 0x300300);

	return 0;
}

int lcd_tcon_top_set_t6d(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_debug: %s\n", __func__);

	lcd_tcon_write(pdrv, TCON_CLK_CTRL, 0x001f);
	lcd_tcon_write(pdrv, TCON_TOP_CTRL, 0xbb99);
	lcd_tcon_write(pdrv, TCON_PLLLOCK_CNTL, 0x0037);
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x0000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL0, 0x33fff000);
	lcd_tcon_write(pdrv, TCON_DDRIF_CTRL1, 0x300300);

	return 0;
}

void lcd_tcon_global_reset_t5(struct aml_lcd_drv_s *pdrv)
{
	/* global reset tcon */
	lcd_reset_setb(pdrv, RESET1_MASK_T5, 0, 4, 1);
	lcd_reset_setb(pdrv, RESET1_LEVEL_T5, 0, 4, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESET1_LEVEL_T5, 1, 4, 1);
	udelay(2);
}

void lcd_tcon_global_reset_t3(struct aml_lcd_drv_s *pdrv)
{
	/* global reset tcon */
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_MASK, 0, 5, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_LEVEL, 0, 5, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_LEVEL, 1, 5, 1);
	udelay(2);
}

void lcd_tcon_global_reset_t3x(struct aml_lcd_drv_s *pdrv)
{
	/* global reset tcon */
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_MASK, 0, 3, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_LEVEL, 0, 3, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET2_LEVEL, 1, 3, 1);
	udelay(2);
}

int lcd_tcon_enable_t5(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct tcon_mem_map_table_s *mm_table = get_lcd_tcon_mm_table();
	struct lcd_tcon_local_cfg_s *local_cfg = get_lcd_tcon_local_cfg();
	unsigned long long local_time[3];
	int ret;

	ret = lcd_tcon_valid_check();
	if (ret)
		return -1;
	if (!pdrv || !tcon_conf || !mm_table || !local_cfg)
		return -1;

	//don't disable encl for system continuous vsync,
	//  just disable tcon pre_proc_clk in tcon bin
	//lcd_venc_enable(pdrv, 0);

	/* step 1: tcon top */
	//lcd_tcon_top_set_t5(pdrv);
	/* tcon_top_output_set */
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL0, 0x76543210);
	lcd_tcon_write(pdrv, TCON_OUT_CH_SEL1, 0xba98);

	/* step 2: tcon_core_reg_update */
	local_time[0] = sched_clock();
	if (local_cfg->def_core_info.header) {
		if (local_cfg->def_core_info.header->block_ctrl == 0) {
			memcpy(&local_cfg->cur_core_info, &local_cfg->def_core_info,
				sizeof(struct tcon_core_reg_info_s));
			lcd_tcon_core_reg_set(pdrv, tcon_conf,
				mm_table, &local_cfg->cur_core_info);
		}
	}

	if (tcon_conf->lut_dma_ops && tcon_conf->lut_dma_ops->init)
		tcon_conf->lut_dma_ops->init(pdrv, tcon_conf->lut_dma_ops);

	/* step 3: tcon data set */
	local_time[1] = sched_clock();
	if (mm_table->version < 0xff)
		lcd_tcon_data_set(pdrv, mm_table);

	/* step 5: tcon_intr_mask */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, TCON_INTR_MASKN_VAL);

	//lcd_venc_enable(pdrv, 1);
	//lcd_tcon_setb(pdrv, 0x207, 1, 4, 1);//enable pre_proc_clk, move to init_post_proc

	tcon_ip27_setup(pdrv);
	local_time[2] = sched_clock();

	pdrv->proc_time.tcon_reg_time = lcd_do_div(local_time[1] - local_time[0], 1000);
	pdrv->proc_time.tcon_data_time = lcd_do_div(local_time[2] - local_time[1], 1000);

	return 0;
}

int lcd_tcon_disable_t5(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();
	struct lcd_tcon_local_cfg_s *tcon_local = get_lcd_tcon_local_cfg();

	if (!pdrv || !tcon_conf || !tcon_local)
		return 0;

	if (tcon_conf->lut_dma_ops && tcon_conf->lut_dma_ops->deinit)
		tcon_conf->lut_dma_ops->deinit(pdrv, tcon_conf->lut_dma_ops);

	memset(&tcon_local->vrr_data, 0, sizeof(tcon_local->vrr_data));
	tcon_ip27_release(pdrv);

	/* demo od */
	lcd_tcon_setb(pdrv, 0x240, 1, 1, 1);
	/* demo demura */
	if (pdrv->data->chip_type != LCD_CHIP_T5D)
		lcd_tcon_setb(pdrv, 0x190, 1, 30, 1);
	lcd_delay_ms(20);

	/* disable unit(reg_func_enable) timing signal */
	lcd_tcon_write(pdrv, 0x30e, 0);

	/* disable tcon intr */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, 0);

	/* disable od ddr_if */
	lcd_tcon_setb(pdrv, 0x263, 0, 31, 1);
	/* disable demura ddr_if */
	lcd_tcon_setb(pdrv, 0x1a3, 0, 31, 1);
	lcd_delay_ms(30);

	lcd_tcon_setb(pdrv, 0x207, 0, 4, 1);//disable pre_proc_clk

	/* top reset */
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x003f);

	//move to tcon_disable api for common flow
	//lcd_tcon_global_reset_t5(pdrv);

	return 0;
}

int lcd_tcon_disable_txhd2(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_tcon_config_s *tcon_conf = get_lcd_tcon_config();

	if (!tcon_conf)
		return 0;

	if (tcon_conf->lut_dma_ops && tcon_conf->lut_dma_ops->deinit)
		tcon_conf->lut_dma_ops->deinit(pdrv, tcon_conf->lut_dma_ops);

	/* demo od */
	lcd_tcon_setb(pdrv, 0x240, 1, 1, 1);
	/* demo demura */
	lcd_tcon_setb(pdrv, 0x190, 1, 30, 1);
	lcd_delay_ms(20);

	/* disable unit(reg_func_enable) timing signal */
	lcd_tcon_write(pdrv, 0x30e, 0);

	/* disable tcon intr */
	lcd_tcon_write(pdrv, TCON_INTR_MASKN, 0);

	/* disable od ddr_if */
	lcd_tcon_setb(pdrv, 0x263, 0, 31, 1);
	/* disable demura ddr_if */
	lcd_tcon_setb(pdrv, 0x198, 0, 31, 1);
	lcd_delay_ms(30);

	lcd_tcon_setb(pdrv, 0x207, 0, 4, 1);//disable pre_proc_clk

	/* top reset */
	lcd_tcon_write(pdrv, TCON_RST_CTRL, 0x003f);

	//move to tcon_disable api for common flow
	//lcd_tcon_global_reset_t5(pdrv);

	return 0;
}

/*
 * get real offset of tcon table in any case (continuous or discontinuous reg)
 */
static int lcd_tcon_get_table_reg_offset(struct tcon_core_reg_info_s *core_reg_info,
			unsigned int reg)
{
	struct lcd_tcon_reg_block_info_s *reg_blk;
	unsigned int i = 0, offset = 0, *table32 = NULL;
	int find = 0;

	if (!core_reg_info)
		return -1;

	table32 = (unsigned int *)core_reg_info->table;
	if (core_reg_info->header->ext_header_size &&
			core_reg_info->ext_header.reg_blk_num) {
		for (i = 0; i < core_reg_info->ext_header.reg_blk_num; i++) {
			reg_blk = &core_reg_info->ext_header.reg_blk_info[i];
			if (reg >= reg_blk->start &&
					reg < (reg_blk->start + reg_blk->len)) {
				find = 1;
				offset += (reg - reg_blk->start);
				break;
			}
			offset += reg_blk->len;
		}
	} else {
		find = 1;
		offset = reg;
	}

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("%s: reg=%#x, find=%d, offset=%#x, val=%#x\n",
			__func__, reg, find, offset, table32[offset]);
	}

	return find ? offset : -1;
}

int lcd_tcon_get_table32_reg(struct tcon_core_reg_info_s *core_reg_info,
		unsigned int reg, unsigned int *val)
{
	int offset = -1;
	unsigned int *table32 = NULL;

	offset = lcd_tcon_get_table_reg_offset(core_reg_info, reg);
	if (offset < 0)
		return -1;

	table32 = (unsigned int *)core_reg_info->table;
	*val = table32[offset];
	return 0;
}

int lcd_tcon_set_table32_reg(struct tcon_core_reg_info_s *core_reg_info,
		unsigned int reg, unsigned int val)
{
	int offset = -1;
	unsigned int *table32 = NULL;

	offset = lcd_tcon_get_table_reg_offset(core_reg_info, reg);
	if (offset < 0)
		return -1;

	table32 = (unsigned int *)core_reg_info->table;
	table32[offset] = val;
	return 0;
}

int lcd_tcon_setb_table32_reg(struct tcon_core_reg_info_s *core_reg_info,
		unsigned int reg, unsigned int val, unsigned int start, unsigned int len)
{
	int offset = -1;
	unsigned int *table32 = NULL;

	offset = lcd_tcon_get_table_reg_offset(core_reg_info, reg);
	if (offset < 0)
		return -1;

	table32 = (unsigned int *)core_reg_info->table;
	table32[offset] = ((table32[offset] &
			~(((1L << len) - 1) << start)) |
			((val & ((1L << len) - 1)) << start));
	return 0;
}

//ret: bit[0]: fatal error, block driver
//     bit[1]: warning, only print warning message
int lcd_tcon_setting_check_t5(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
		struct tcon_core_reg_info_s *core_reg_info, char *ferr_str, char *warn_str)
{	unsigned int val, tri_gate;
	int ferr_len = 0, warn_len = 0, ferr_left, warn_left, ret = 0;

	if (!ferr_str || !warn_str)
		return 0;
	if (!core_reg_info)
		return 0;

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x26e, &val))
		val = (val >> 21) & 0x7;
	if (ptiming->h_active == 1366) {
		if (val != 3) {
			ferr_left = lcd_debug_info_len(ferr_len);
			ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
				"  cmpr_lbuf_tail: %d, req: 3!!!\n", val);
			ret |= (1 << 0);
		}
	} else {
		if (val) {
			ferr_left = lcd_debug_info_len(ferr_len);
			ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
				"  cmpr_lbuf_tail: %d, req: 0!!!\n", val);
			ret |= (1 << 0);
		}
	}

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x240, &val))
		val = (val >> 2) & 0x1;
	if (val) {
		ferr_left = lcd_debug_info_len(ferr_len);
		ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
			"  od_cur_ref_sel_chk: %d, req: 0!!!\n", val);
		ret |= (1 << 0);
	}

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x45a, &val))
		val = (val >> 28) & 0x1;
	if (val) {
		ferr_left = lcd_debug_info_len(ferr_len);
		ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
			"  predmy_dt_en: %d, req: 0!!!\n", val);
		ret |= (1 << 0);
	}

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x45a, &val))
		val = (val >> 5) & 0x1f;
	if (val) {
		ferr_left = lcd_debug_info_len(ferr_len);
		ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
			"  predmy_num: %d, req: 0!!!\n", val);
		ret |= (1 << 0);
	}

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x30d, &val))
		val = (val >> 13) & 0x1;
	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x118, &tri_gate))
		tri_gate = (tri_gate >> 29) & 0x1;
	if (tri_gate == 0 && val) {
		warn_left = lcd_debug_info_len(warn_len);
		warn_len += snprintf(warn_str + warn_len, warn_left,
			"  reg_rgd_en: %d, only for tri-gate, please confirm!\n", val);
		ret |= (1 << 1);
	}

	return ret;
}

int lcd_tcon_setting_check_t5d(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming,
		struct tcon_core_reg_info_s *core_reg_info, char *ferr_str, char *warn_str)
{
	unsigned int val, tri_gate;
	int ferr_len = 0, warn_len = 0, ferr_left, warn_left, ret = 0;

	if (!ferr_str || !warn_str)
		return 0;
	if (!core_reg_info)
		return 0;

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x26e, &val))
		val = (val >> 21) & 0x7;
	if (ptiming->h_active == 1366) {
		if (val != 3) {
			ferr_left = lcd_debug_info_len(ferr_len);
			ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
				"  cmpr_lbuf_tail: %d, req: 3!!!\n", val);
			ret |= (1 << 0);
		}
	} else {
		if (val) {
			ferr_left = lcd_debug_info_len(ferr_len);
			ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
				"  cmpr_lbuf_tail: %d, req: 0!!!\n", val);
			ret |= (1 << 0);
		}
	}

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x240, &val))
		val = (val >> 2) & 0x1;
	if (val) {
		ferr_left = lcd_debug_info_len(ferr_len);
		ferr_len += snprintf(ferr_str + ferr_len, ferr_left,
			"  od_cur_ref_sel_chk: %d, req: 0!!!\n", val);
		ret |= (1 << 0);
	}

	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x30d, &val))
		val = (val >> 13) & 0x1;
	if (!lcd_tcon_get_table32_reg(core_reg_info, 0x118, &tri_gate))
		tri_gate = (tri_gate >> 29) & 0x1;
	if (tri_gate == 0 && val) {
		warn_left = lcd_debug_info_len(warn_len);
		warn_len += snprintf(warn_str + warn_len, warn_left,
			"  reg_rgd_en: %d, only for tri-gate, please confirm!\n", val);
		ret |= (1 << 1);
	}

	return ret;
}
