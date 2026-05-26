// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reset.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_venc.h"
#include "lcd_c_series_vout_reg.h"

static unsigned int lcd_dth_lut[16] = {
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x82412814, 0x48122481, 0x18242184, 0x18242841,
	0x9653ca56, 0x6a3ca635, 0x6ca9c35a, 0xca935635,
	0x7dbedeb7, 0xde7dbed7, 0xeb7dbed7, 0xdbe77edb
};

static void lcd_venc_set(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int reoder, timgen_mode, serial_rate, field_mode, dith_mode;
	unsigned int bot_bgn_lne, top_bgn_lne;
	unsigned int vs_lne_bgn_o, vs_lne_end_o, vs_pix_bgn_o, vs_pix_end_o;
	unsigned int de_lne_bgn_o, de_lne_end_o;
	int i;

	reoder = 36;

	//timgen_mode:
	//1<<0:MIPI_TX, 1<<1:LCDs8,  1<<2:BT1120,  1<<3:BT656, 1<<4:lcds6,
	//1<<5:lcdp6, 1<<6:lcdp8, 1<<7:lcd565, 1<<8:lcds9(6+3,3+6), 1<<9:lcds8(5+3,3+5)

	//serial_rate:
	//0:pix/1:cycle    1:pix/2cycle  2:pix/3cycle

	//field_mode:
	//0:progress 1:interlace

	switch (pconf->basic.lcd_type) {
	case LCD_MIPI:
		timgen_mode = (1 << 0);
		serial_rate = 0;
		field_mode = 0;
		dith_mode = 0; //10bit to 8bit
		break;
	case LCD_BT656:
		timgen_mode = (1 << 3);
		serial_rate = 1;
		field_mode = 1;
		dith_mode = 0; //10bit to 8bit
		break;
	case LCD_BT1120:
		timgen_mode = (1 << 2);
		serial_rate = 0;
		field_mode = 1;
		dith_mode = 0; //10bit to 8bit
		break;
	default:
		timgen_mode = (1 << 5);
		serial_rate = 0;
		field_mode = 0;
		dith_mode = 1; /*10bit to 6bit*/
		break;
	}

	bot_bgn_lne  =    0;
	top_bgn_lne  =    0;
	vs_lne_bgn_o =    0;
	vs_lne_end_o =    0;
	vs_pix_bgn_o =    0;
	vs_pix_end_o =    0;
	de_lne_bgn_o =    0;
	de_lne_end_o =    0;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_C3:
		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL, 0, 0, 1); //disable venc_en
		lcd_vcbus_setb(VPU_VOUT_DETH_CTRL, dith_mode, 5, 1);
		lcd_vcbus_setb(VPU_VOUT_DETH_CTRL, pconf->timing.act_timing.h_active, 6, 13);
		lcd_vcbus_setb(VPU_VOUT_DETH_CTRL, pconf->timing.act_timing.v_active, 19, 13);
		lcd_vcbus_setb(VPU_VOUT_INT_CTRL, 1, 14, 1); //dth_en

		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL, reoder, 4, 6);
		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL, timgen_mode, 16, 10);
		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL, serial_rate, 2, 2);

		lcd_vcbus_write(VPU_VOUT_DTH_ADDR, 0);
		for (i = 0; i < 32; i++)
			lcd_vcbus_write(VPU_VOUT_DTH_DATA, lcd_dth_lut[i % 16]);

		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL,    field_mode,  1, 1);
		lcd_vcbus_setb(VPU_VOUT_MAX_SIZE,     pconf->timing.act_timing.h_period, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_MAX_SIZE,     pconf->timing.act_timing.v_period,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_FLD_BGN_LINE, bot_bgn_lne, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_FLD_BGN_LINE, top_bgn_lne,  0, 13);

		lcd_vcbus_setb(VPU_VOUT_HS_POS,      pconf->timing.hs_hs_addr, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_HS_POS,      pconf->timing.hs_he_addr,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_E_POS,  pconf->timing.vs_vs_addr, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_E_POS,  pconf->timing.vs_ve_addr,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_E_POS,  pconf->timing.vs_hs_addr, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_E_POS,  pconf->timing.vs_he_addr,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_O_POS,  vs_lne_bgn_o, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_O_POS,  vs_lne_end_o,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_O_POS,  vs_pix_bgn_o, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_O_POS,  vs_pix_end_o,  0, 13);

		lcd_vcbus_setb(VPU_VOUT_DELN_E_POS,  pconf->timing.vstart, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_DELN_E_POS,  pconf->timing.vend,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_DELN_O_POS,  de_lne_bgn_o, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_DELN_O_POS,  de_lne_end_o,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_DE_PX_EN,    pconf->timing.hstart, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_DE_PX_EN,    pconf->timing.hend,  0, 13);

		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL, 1, 0, 1); //venc_en
		break;
	case LCD_CHIP_C5:
		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL_C5, 0, 0, 1); //disable venc_en
		lcd_vcbus_setb(VPU_VOUT_DETH_CTRL_C5, dith_mode, 5, 1);
		lcd_vcbus_setb(VPU_VOUT_DETH_CTRL_C5, pconf->timing.act_timing.h_active, 6, 13);
		lcd_vcbus_setb(VPU_VOUT_DETH_CTRL_C5, pconf->timing.act_timing.v_active, 19, 13);
		lcd_vcbus_setb(VPU_VOUT_INT_CTRL_C5, 1, 14, 1); //dth_en

		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL_C5, reoder, 4, 6);
		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL_C5, timgen_mode, 16, 10);
		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL_C5, serial_rate, 2, 2);

		lcd_vcbus_write(VPU_VOUT_DTH_ADDR_C5, 0);
		for (i = 0; i < 32; i++)
			lcd_vcbus_write(VPU_VOUT_DTH_DATA_C5, lcd_dth_lut[i % 16]);

		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL_C5,    field_mode,  1, 1);
		lcd_vcbus_setb(VPU_VOUT_MAX_SIZE_C5,     pconf->timing.act_timing.h_period, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_MAX_SIZE_C5,     pconf->timing.act_timing.v_period,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_FLD_BGN_LINE_C5, bot_bgn_lne, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_FLD_BGN_LINE_C5, top_bgn_lne,  0, 13);

		lcd_vcbus_setb(VPU_VOUT_HS_POS_C5,      pconf->timing.hs_hs_addr, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_HS_POS_C5,      pconf->timing.hs_he_addr,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_E_POS_C5,  pconf->timing.vs_vs_addr, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_E_POS_C5,  pconf->timing.vs_ve_addr,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_E_POS_C5,  pconf->timing.vs_hs_addr, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_E_POS_C5,  pconf->timing.vs_he_addr,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_O_POS_C5,  vs_lne_bgn_o, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSLN_O_POS_C5,  vs_lne_end_o,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_O_POS_C5,  vs_pix_bgn_o, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_VSPX_O_POS_C5,  vs_pix_end_o,  0, 13);

		lcd_vcbus_setb(VPU_VOUT_DELN_E_POS_C5,  pconf->timing.vstart, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_DELN_E_POS_C5,  pconf->timing.vend,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_DELN_O_POS_C5,  de_lne_bgn_o, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_DELN_O_POS_C5,  de_lne_end_o,  0, 13);
		lcd_vcbus_setb(VPU_VOUT_DE_PX_EN_C5,    pconf->timing.hstart, 16, 13);
		lcd_vcbus_setb(VPU_VOUT_DE_PX_EN_C5,    pconf->timing.hend,  0, 13);

		lcd_vcbus_setb(VPU_VOUT_CORE_CTRL_C5, 1, 0, 1); //venc_en
		break;
	default:
		break;
	}
}

static void lcd_venc_enable_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int vout_core_ctrl_reg;

	if (pdrv->data->chip_type == LCD_CHIP_C3)
		vout_core_ctrl_reg = VPU_VOUT_CORE_CTRL;
	else if (pdrv->data->chip_type == LCD_CHIP_C5)
		vout_core_ctrl_reg = VPU_VOUT_CORE_CTRL_C5;
	else
		return;

	lcd_vcbus_setb(vout_core_ctrl_reg, flag ? 1 : 0, 0, 1);
}

static int lcd_venc_get_state(struct aml_lcd_drv_s *pdrv)
{
	unsigned int init_state;
	unsigned int vout_core_ctrl_reg;

	if (pdrv->data->chip_type == LCD_CHIP_C3)
		vout_core_ctrl_reg = VPU_VOUT_CORE_CTRL;
	else if (pdrv->data->chip_type == LCD_CHIP_C5)
		vout_core_ctrl_reg = VPU_VOUT_CORE_CTRL_C5;
	else
		return 0;

	init_state = lcd_vcbus_getb(vout_core_ctrl_reg, 0, 1);
	return init_state;
}

static int lcd_venc_get_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int init_state;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_C3:
		pconf->timing.act_timing.h_active = lcd_vcbus_getb(VPU_VOUT_DE_PX_EN, 0, 13)
			- lcd_vcbus_getb(VPU_VOUT_DE_PX_EN, 16, 13) + 1;
		pconf->timing.act_timing.v_active = lcd_vcbus_getb(VPU_VOUT_DELN_E_POS, 0, 13)
			- lcd_vcbus_getb(VPU_VOUT_DELN_E_POS, 16, 13) + 1;
		pconf->timing.act_timing.h_period = lcd_vcbus_getb(VPU_VOUT_MAX_SIZE, 16, 13);
		pconf->timing.act_timing.v_period = lcd_vcbus_getb(VPU_VOUT_MAX_SIZE, 0, 13);
		init_state = lcd_vcbus_getb(VPU_VOUT_CORE_CTRL, 0, 1);
		break;
	case LCD_CHIP_C5:
		pconf->timing.act_timing.h_active = lcd_vcbus_getb(VPU_VOUT_DE_PX_EN_C5, 0, 13)
			- lcd_vcbus_getb(VPU_VOUT_DE_PX_EN_C5, 16, 13) + 1;
		pconf->timing.act_timing.v_active = lcd_vcbus_getb(VPU_VOUT_DELN_E_POS_C5, 0, 13)
			- lcd_vcbus_getb(VPU_VOUT_DELN_E_POS_C5, 16, 13) + 1;
		pconf->timing.act_timing.h_period = lcd_vcbus_getb(VPU_VOUT_MAX_SIZE_C5, 16, 13);
		pconf->timing.act_timing.v_period = lcd_vcbus_getb(VPU_VOUT_MAX_SIZE_C5, 0, 13);
		init_state = lcd_vcbus_getb(VPU_VOUT_CORE_CTRL_C5, 0, 1);
		break;
	default:
		return 0;
	}

	return init_state;
}

static int lcd_venc_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i, n, len = 0;
	unsigned int reg_table_c3[] = {
		VPU_VOUT_CORE_CTRL,
		VPU_VOUT_MAX_SIZE,
		VPU_VOUT_FLD_BGN_LINE,
		VPU_VOUT_HS_POS,
		VPU_VOUT_VSLN_E_POS,
		VPU_VOUT_VSPX_E_POS,
		VPU_VOUT_VSLN_O_POS,
		VPU_VOUT_VSPX_O_POS,
		VPU_VOUT_DE_PX_EN,
		VPU_VOUT_DELN_E_POS,
		VPU_VOUT_DELN_O_POS,
		VPU_VOUT_INT_CTRL,
		VPU_VOUT_DETH_CTRL
	};
	unsigned int reg_table_c5[] = {
		VPU_VOUT_CORE_CTRL_C5,
		VPU_VOUT_MAX_SIZE_C5,
		VPU_VOUT_FLD_BGN_LINE_C5,
		VPU_VOUT_HS_POS_C5,
		VPU_VOUT_VSLN_E_POS_C5,
		VPU_VOUT_VSPX_E_POS_C5,
		VPU_VOUT_VSLN_O_POS_C5,
		VPU_VOUT_VSPX_O_POS_C5,
		VPU_VOUT_DE_PX_EN_C5,
		VPU_VOUT_DELN_E_POS_C5,
		VPU_VOUT_DELN_O_POS_C5,
		VPU_VOUT_INT_CTRL_C5,
		VPU_VOUT_DETH_CTRL_C5,
	};

	if (pdrv->data->chip_type == LCD_CHIP_C3) {
		for (i = 0; i < ARRAY_SIZE(reg_table_c3); i++) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "vcbus [0x%04x] = 0x%08x\n",
				reg_table_c3[i], lcd_vcbus_read(reg_table_c3[i]));
		}
	} else if (pdrv->data->chip_type == LCD_CHIP_C5) {
		for (i = 0; i < ARRAY_SIZE(reg_table_c5); i++) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "vcbus [0x%04x] = 0x%08x\n",
				reg_table_c5[i], lcd_vcbus_read(reg_table_c5[i]));
		}
	}

	return len;
}

static char *lcd_enc_tst_str[11] = {
	"0-None",        /* 0 */
	"1-Color Bar",   /* 1 */
	"2-Thin Line",   /* 2 */
	"3-Dot Grid",    /* 3 */
	"4-Gray",        /* 4 */
	"5-Red",         /* 5 */
	"6-Green",       /* 6 */
	"7-Blue",        /* 7 */
	"8-Black",       /* 8 */
	"9-White",       /* 9 */
	"10-X Patten",   /* 10 */
};

static unsigned int lcd_enc_tst[11][4] = {
/*   Y,     Cb,    Cr,    mode_sel */
	{0x200, 0x200, 0x200, 0},  /* 0 */
	{0x0,     0x0,   0x0, 1},  /* 1 */
	{0x0,     0x0,   0x0, 2},  /* 2 */
	{0x0,     0x0,   0x0, 3},  /* 3 */
	{0x1ff, 0x1ff, 0x1ff, 0},  /* 4 */
	{0x3ff,   0x0,   0x0, 0},  /* 5 */
	{  0x0, 0x3ff,   0x0, 0},  /* 6 */
	{  0x0,   0x0, 0x3ff, 0},  /* 7 */
	{  0x0,   0x0,   0x0, 0},  /* 8 */
	{0x3ff, 0x3ff, 0x3ff, 0},  /* 9 */
	{0x3ff,   0x0,   0x0, 4},  /* 10 */
};

static int lcd_venc_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int h_start, v_start, clb_width, ha, va;
	unsigned int edge_rect_line_width, center_X_line_width, K_frac, K_int;

	ha = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
	va = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
	h_start = pdrv->curr_dev->dev_cfg.timing.hstart;
	v_start = pdrv->curr_dev->dev_cfg.timing.vstart;

	if (num >= 11) {
		LCDERR("Test %d invalid\n", num);
		return 0;
	}

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_C5:
		lcd_vcbus_setb(VPU_VOUT_BIST_SEL_C5, lcd_enc_tst[num][3], 0, 8);
		switch (lcd_enc_tst[num][3]) {
		case 1:  /* Color Bar */
			clb_width = ha / 8;
			lcd_vcbus_setb(VPU_VOUT_CLRBAR_CFG_C5, h_start, 16, 13);
			lcd_vcbus_setb(VPU_VOUT_CLRBAR_CFG_C5, clb_width - 1, 0, 13);
			break;
		case 4: /* X patten */
			edge_rect_line_width = 6;
			center_X_line_width = 4;
				/* clrbar_width: total width */
			lcd_vcbus_setb(VPU_VOUT_CLRBAR_CFG_C5, ha, 0, 13);
				/* clrbar_high: total high  */
			lcd_vcbus_setb(VPU_VOUT_BIST_HIGH_C5, va, 0, 13);
				/* Cb: K - 1 */
			K_int = ha / va;
			K_frac = ((ha - va) * (1 << 8)) / va;
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, K_frac, 10, 8);
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, K_int, 18, 2);
				/* Cr: X-line h width */
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, center_X_line_width, 0, 5); //L
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, center_X_line_width, 5, 5); //R
				/* vdcnt_strtset: rect_line_w + 1 */
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, edge_rect_line_width, 30, 2);
				/* clrbar_strt: start H pos (de-1) */
			lcd_vcbus_setb(VPU_VOUT_CLRBAR_CFG_C5, h_start, 16, 13);
				/* clrbar_vstrt: start V pos */
			lcd_vcbus_setb(VPU_VOUT_BIST_HIGH_C5, v_start, 16, 13);
			break;
		default:
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, lcd_enc_tst[num][0], 20, 10);
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, lcd_enc_tst[num][1], 10, 10);
			lcd_vcbus_setb(VPU_VOUT_BIST_CFG_YUV_C5, lcd_enc_tst[num][2], 0, 10);
			break;
		}
		lcd_vcbus_setb(VPU_VOUT_INT_CTRL_C5, num && 1, 15, 1);   /* bist_en */
		break;
	case LCD_CHIP_C3:
		if (num == 0 || (num >= 4 && num <= 9)) {
			lcd_vcbus_write(VPU_VOUT_BLEND_CTRL, num ? 0xc : 0x0);
			lcd_vcbus_setb(VPU_VOUT_BLEND_DUMDATA, lcd_enc_tst[num][0], 20, 10);
			lcd_vcbus_setb(VPU_VOUT_BLEND_DUMDATA, lcd_enc_tst[num][1], 10, 10);
			lcd_vcbus_setb(VPU_VOUT_BLEND_DUMDATA, lcd_enc_tst[num][2], 0, 10);
		} else {
			LCDERR("Test %d not supported by chip\n", num);
			return 0;
		}
		break;
	default:
		return 0;
	}
	LCDPR("[%d]: show test pattern: %s\n", pdrv->index, lcd_enc_tst_str[num]);
	return 0;
}

int lcd_venc_op_init_vout_model(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op)
{
	if (!venc_op)
		return -1;

	venc_op->wait_vsync = NULL;
	venc_op->get_max_lcnt = NULL;
	venc_op->gamma_test_en = NULL;
	venc_op->venc_debug_test = lcd_venc_debug_test;
	venc_op->venc_set_timing = NULL;
	venc_op->venc_set = lcd_venc_set;
	venc_op->venc_fr_change = NULL;
	venc_op->venc_enable = lcd_venc_enable_ctrl;
	venc_op->get_venc_state = lcd_venc_get_state;
	venc_op->get_venc_init_config = lcd_venc_get_init_config;
	venc_op->venc_vrr_recovery = NULL;
	venc_op->get_encl_line_cnt = NULL;
	venc_op->get_encl_frm_cnt = NULL;
	venc_op->venc_reg_dump = lcd_venc_reg_dump;

	return 0;
};
