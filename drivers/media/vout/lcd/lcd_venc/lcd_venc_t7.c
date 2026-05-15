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

static void lcd_venc_wait_vsync(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt, line_cnt_previous;
	unsigned int reg, offset;
	int i = 0;

	if (!pdrv)
		return;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = VPU_VENCP_STAT + offset;

	line_cnt = 0x1fff;
	line_cnt_previous = lcd_vcbus_getb(reg, 16, 13);
	while (i++ < LCD_WAIT_VSYNC_TIMEOUT) {
		line_cnt = lcd_vcbus_getb(reg, 16, 13);
		if (line_cnt < line_cnt_previous)
			break;
		line_cnt_previous = line_cnt;
		udelay(2);
	}
	/*LCDPR("line_cnt=%d, line_cnt_previous=%d, i=%d\n",
	 *	line_cnt, line_cnt_previous, i);
	 */
}

static unsigned int lcd_venc_get_max_lint_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset, reg, line_cnt;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = ENCL_VIDEO_MAX_LNCNT + offset;

	line_cnt = lcd_vcbus_read(reg) + 1;
	/*LCDPR("[%d]: %s: line_cnt=%d", pdrv->index, __func__, line_cnt); */

	return line_cnt;
}

static void lcd_venc_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int reg, offset;

	offset = pdrv->data->offset_venc_data[pdrv->index];
	reg = LCD_GAMMA_CNTL_PORT0 + offset;

	if (flag) {
		if (lcd_vcbus_getb(reg, 0, 1) == 0) {
			lcd_vcbus_setb(reg, 1, 0, 1);
			LCD_PR(pdrv, "%s: %d", __func__, flag);
		}
	} else {
		if (lcd_vcbus_getb(reg, 0, 1)) {
			lcd_vcbus_setb(reg, 0, 0, 1);
			LCD_PR(pdrv, "%s: %d", __func__, flag);
		}
	}
}

struct lcd_enc_test_t {
	char *name;
	unsigned int mode;
	unsigned int y;
	unsigned int cb;
	unsigned int cr;
	unsigned int en;
	unsigned int vfifo_en;
	unsigned int rgb_in;
};

static struct lcd_enc_test_t lcd_enc_tst_comm[] = {
	{"0-None",         0,    0x200,   0x200, 0x200, 0, 1, 3},  /* 0 */
	{"1-Color Bar",    1,    0x200,   0x200, 0x200, 1, 0, 1},  /* 1 */
	{"2-Thin Line",    2,    0x200,   0x200, 0x200, 1, 0, 1},  /* 2 */
	{"3-Dot Grid",     3,    0x200,   0x200, 0x200, 1, 0, 1},  /* 3 */
	{"4-Gray",         0,    0x1ff,   0x1ff, 0x1ff, 1, 0, 3},  /* 4 */
	{"5-Red",          0,    0x3ff,     0x0,   0x0, 1, 0, 3},  /* 5 */
	{"6-Green",        0,      0x0,   0x3ff,   0x0, 1, 0, 3},  /* 6 */
	{"7-Blue",         0,      0x0,     0x0, 0x3ff, 1, 0, 3},  /* 7 */
	{"8-Black",        0,      0x0,     0x0,   0x0, 1, 0, 3},  /* 8 */
};

static struct lcd_enc_test_t lcd_enc_tst_t6d[] = {
	{"0-None",         0,    0x200,   0x200, 0x200, 0, 1, 3},  /* 0 */
	{"1-Color Bar",    1,    0x200,   0x200, 0x200, 1, 0, 1},  /* 1 */
	{"2-Thin Line",    2,    0x200,   0x200, 0x200, 1, 0, 1},  /* 2 */
	{"3-Dot Grid",     3,    0x200,   0x200, 0x200, 1, 0, 1},  /* 3 */
	{"4-Gray",         0,    0x1ff,   0x1ff, 0x1ff, 1, 0, 3},  /* 4 */
	{"5-Red",          0,    0x3ff,     0x0,   0x0, 1, 0, 3},  /* 5 */
	{"6-Green",        0,      0x0,   0x3ff,   0x0, 1, 0, 3},  /* 6 */
	{"7-Blue",         0,      0x0,     0x0, 0x3ff, 1, 0, 3},  /* 7 */
	{"8-Black",        0,      0x0,     0x0,   0x0, 1, 0, 3},  /* 8 */
	{"9-Not support",  0,    0x200,   0x200, 0x200, 0, 1, 3},  /* 9 */
	{"10-Gray Scale",   5, 0xffffffff,   0x7,   0x7, 1, 0, 3}, /* 10 */
	{"11-Red Scale",   5,      0x1,     0x0,   0x1, 1, 0, 3},  /* 11 */
	{"12-Green Scale", 5,      0x1,     0x0,   0x2, 1, 0, 3},  /* 12 */
	{"13-Blue Scale",  5,      0x1,     0x0,   0x4, 1, 0, 3},  /* 13 */
};

static struct lcd_enc_test_t lcd_enc_tst_t6w[] = {
	{"0-None",         0,    0x200,   0x200, 0x200, 0, 1, 3},  /* 0 */
	{"1-Color Bar",    1,    0x200,   0x200, 0x200, 1, 0, 1},  /* 1 */
	{"2-Thin Line",    2,    0x200,   0x200, 0x200, 1, 0, 1},  /* 2 */
	{"3-Dot Grid",     3,    0x200,   0x200, 0x200, 1, 0, 1},  /* 3 */
	{"4-Gray",         0,    0x1ff,   0x1ff, 0x1ff, 1, 0, 3},  /* 4 */
	{"5-Red",          0,    0x3ff,     0x0,   0x0, 1, 0, 3},  /* 5 */
	{"6-Green",        0,      0x0,   0x3ff,   0x0, 1, 0, 3},  /* 6 */
	{"7-Blue",         0,      0x0,     0x0, 0x3ff, 1, 0, 3},  /* 7 */
	{"8-Black",        0,      0x0,     0x0,   0x0, 1, 0, 3},  /* 8 */
	{"9-X icon",       4,    0x3ff,    0x10,   0x9, 1, 0, 2},  /* 9 */
	{"10-Gray Scale",   5, 0xffffffff,   0x7,   0x7, 1, 0, 3}, /* 10 */
	{"11-Red Scale",   5,      0x1,     0x0,   0x1, 1, 0, 3},  /* 11 */
	{"12-Green Scale", 5,      0x1,     0x0,   0x2, 1, 0, 3},  /* 12 */
	{"13-Blue Scale",  5,      0x1,     0x0,   0x4, 1, 0, 3},  /* 13 */
};

static int lcd_venc_bist_set(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int h_active, video_on_pixel, offset;
	struct lcd_enc_test_t *pcur_test = NULL;
	unsigned int cur_test_num = 0;
	unsigned short width, height, gcd_num;

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T6D:
		pcur_test = lcd_enc_tst_t6d;
		cur_test_num = ARRAY_SIZE(lcd_enc_tst_t6d);
		break;
	case LCD_CHIP_T6W:
		pcur_test = lcd_enc_tst_t6w;
		cur_test_num = ARRAY_SIZE(lcd_enc_tst_t6w);
		break;
	default:
		pcur_test = lcd_enc_tst_comm;
		cur_test_num = ARRAY_SIZE(lcd_enc_tst_comm);
		break;
	}

	if (num >= cur_test_num || !pcur_test)
		return -1;

	offset = pdrv->data->offset_venc[pdrv->index];
	h_active = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
	video_on_pixel = pdrv->curr_dev->dev_cfg.timing.hstart;

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, pcur_test[num].rgb_in);
	lcd_vcbus_write(ENCL_TST_MDSEL + offset, pcur_test[num].mode);
	lcd_vcbus_write(ENCL_TST_Y + offset, pcur_test[num].y);
	if (num == 9) {
		width = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
		height = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
		gcd_num = gcd(width, height);
		lcd_vcbus_write(ENCL_TST_CB + offset, width / gcd_num);
		lcd_vcbus_write(ENCL_TST_CR + offset, height / gcd_num);
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, height);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, width);
	} else {
		lcd_vcbus_write(ENCL_TST_CB + offset, pcur_test[num].cb);
		lcd_vcbus_write(ENCL_TST_CR + offset, pcur_test[num].cr);
		lcd_vcbus_write(ENCL_TST_CLRBAR_STRT + offset, video_on_pixel - 2);
		lcd_vcbus_write(ENCL_TST_CLRBAR_WIDTH + offset, (h_active / 9));
	}
	lcd_vcbus_setb(ENCL_TST_EN + offset, pcur_test[num].en, 0, 1);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, pcur_test[num].vfifo_en, 3, 1);
	if (num > 0)
		LCD_PR(pdrv, "show test pattern: %s", pcur_test[num].name);

	return 0;
}

static void lcd_venc_bist_change(struct aml_lcd_drv_s *pdrv, unsigned int level_r,
				 unsigned int level_g, unsigned int level_b)
{
	struct lcd_enc_test_t *pcur_test = lcd_enc_tst_comm;
	unsigned int bist_num, r_data, g_data, b_data, offset;

	if (level_r > 0x3ff) {
		bist_num = 0;
		r_data = 0;
		g_data = 0;
		b_data = 0;
	} else {
		bist_num = 8;
		r_data = level_r;
		g_data = level_g;
		b_data = level_b;
	}

	offset = pdrv->data->offset_venc[pdrv->index];
	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, pcur_test[bist_num].rgb_in);
	lcd_vcbus_write(ENCL_TST_MDSEL + offset, pcur_test[bist_num].mode);
	lcd_vcbus_write(ENCL_TST_Y + offset, r_data);
	lcd_vcbus_write(ENCL_TST_CB + offset, g_data);
	lcd_vcbus_write(ENCL_TST_CR + offset, b_data);
	lcd_vcbus_setb(ENCL_TST_EN + offset, pcur_test[bist_num].en, 0, 1);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, pcur_test[bist_num].vfifo_en, 3, 1);
}

/*
 *static void lcd_venc_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag)
 *{
 *	unsigned int offset;
 *
 *	offset = pdrv->data->offset_venc[pdrv->index];
 *
 *	if (flag) {
 *		lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);
 *		lcd_vcbus_write(ENCL_TST_MDSEL + offset, 0);
 *		lcd_vcbus_write(ENCL_TST_Y + offset, 0);
 *		lcd_vcbus_write(ENCL_TST_CB + offset, 0);
 *		lcd_vcbus_write(ENCL_TST_CR + offset, 0);
 *		lcd_vcbus_setb(ENCL_TST_EN + offset, 1, 0, 1);
 *		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, 0, 3, 1);
 *	} else {
 *		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, 1, 3, 1);
 *		lcd_vcbus_setb(ENCL_TST_EN + offset, 0, 0, 1);
 *	}
 *}
 */

static void lcd_venc_gamma_init(struct aml_lcd_drv_s *pdrv)
{
	unsigned int data[2];
	int index = pdrv->index;

	if (pdrv->lcd_pxp)
		return;

	data[0] = index;
	data[1] = 0xff; //default gamma lut
	aml_lcd_atomic_notifier_call_chain(LCD_EVENT_GAMMA_UPDATE, (void *)data);
}

static void lcd_venc_set_tcon(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int offset_if, offset_data;
	unsigned int reg_rgb_base, reg_rgb_coeff, reg_dith_ctrl, reg_pol_ctrl;
	unsigned int reg_de_hs, reg_de_he, reg_de_vs, reg_de_ve;
	unsigned int reg_hsync_hs, reg_hsync_he, reg_hsync_vs, reg_hsync_ve;
	unsigned int reg_vsync_hs, reg_vsync_he, reg_vsync_vs, reg_vsync_ve;

	offset_data = pdrv->data->offset_venc_data[pdrv->index];
	offset_if = pdrv->data->offset_venc_if[pdrv->index];
	reg_rgb_base = LCD_RGB_BASE_ADDR + offset_data;
	reg_rgb_coeff = LCD_RGB_COEFF_ADDR + offset_data;
	reg_dith_ctrl = LCD_DITH_CNTL_ADDR + offset_data;
	reg_pol_ctrl = LCD_POL_CNTL_ADDR + offset_data;
	reg_de_hs = DE_HS_ADDR + offset_if;
	reg_de_he = DE_HE_ADDR + offset_if;
	reg_de_vs = DE_VS_ADDR + offset_if;
	reg_de_ve = DE_VE_ADDR + offset_if;
	reg_hsync_hs = HSYNC_HS_ADDR + offset_if;
	reg_hsync_he = HSYNC_HE_ADDR + offset_if;
	reg_hsync_vs = HSYNC_VS_ADDR + offset_if;
	reg_hsync_ve = HSYNC_VE_ADDR + offset_if;
	reg_vsync_hs = VSYNC_HS_ADDR + offset_if;
	reg_vsync_he = VSYNC_HE_ADDR + offset_if;
	reg_vsync_vs = VSYNC_VS_ADDR + offset_if;
	reg_vsync_ve = VSYNC_VE_ADDR + offset_if;

	lcd_vcbus_write(reg_rgb_base, 0x0);
	lcd_vcbus_write(reg_rgb_coeff, 0x400);

	if (pconf->basic.lcd_type != LCD_P2P &&
		pconf->basic.lcd_type != LCD_MLVDS) {
		switch (pconf->timing.act_timing.lcd_bits) {
		case 18:
			lcd_vcbus_write(reg_dith_ctrl,  0x600);
			break;
		case 24:
			lcd_vcbus_write(reg_dith_ctrl,  0x400);
			break;
		case 30:
		default:
			lcd_vcbus_write(reg_dith_ctrl,  0x0);
			break;
		}
	} else {
		lcd_vcbus_write(reg_dith_ctrl,  0x0);
	}

	switch (pconf->basic.lcd_type) {
	case LCD_LVDS:
		lcd_vcbus_setb(reg_pol_ctrl, 1, 0, 3);
		// refs to lcd_lvds.c@lcd_lvds_enable
		if (pconf->timing.act_timing.vsync_pol == pconf->timing.act_timing.hsync_pol)
			lcd_vcbus_setb(reg_pol_ctrl, 1, 1, 1);
		break;
	case LCD_VBYONE:
		if (pconf->timing.act_timing.hsync_pol)
			lcd_vcbus_setb(reg_pol_ctrl, 1, 0, 1);
		if (pconf->timing.act_timing.vsync_pol)
			lcd_vcbus_setb(reg_pol_ctrl, 1, 1, 1);
		break;
	case LCD_MIPI:
		//lcd_vcbus_setb(reg_pol_ctrl, 0x3, 0, 2);
		/*lcd_vcbus_write(reg_pol_ctrl,
		 *	(lcd_vcbus_read(reg_pol_ctrl) |
		 *	 ((0 << 2) | (vs_pol_adj << 1) | (hs_pol_adj << 0))));
		 */
		/*lcd_vcbus_write(reg_pol_ctrl, (lcd_vcbus_read(reg_pol_ctrl) |
		 *	 ((1 << LCD_TCON_DE_SEL) | (1 << LCD_TCON_VS_SEL) |
		 *	  (1 << LCD_TCON_HS_SEL))));
		 */
		break;
	default:
		break;
	}

	/* DE signal */
	lcd_vcbus_write(reg_de_hs,    pconf->timing.de_hs_addr);
	lcd_vcbus_write(reg_de_he,    pconf->timing.de_he_addr);
	lcd_vcbus_write(reg_de_vs,    pconf->timing.de_vs_addr);
	lcd_vcbus_write(reg_de_ve,    pconf->timing.de_ve_addr);

	/* Hsync signal */
	lcd_vcbus_write(reg_hsync_hs, pconf->timing.hs_hs_addr);
	lcd_vcbus_write(reg_hsync_he, pconf->timing.hs_he_addr);
	lcd_vcbus_write(reg_hsync_vs, pconf->timing.hs_vs_addr);
	lcd_vcbus_write(reg_hsync_ve, pconf->timing.hs_ve_addr);

	/* Vsync signal */
	lcd_vcbus_write(reg_vsync_hs, pconf->timing.vs_hs_addr);
	lcd_vcbus_write(reg_vsync_he, pconf->timing.vs_he_addr);
	lcd_vcbus_write(reg_vsync_vs, pconf->timing.vs_vs_addr);
	lcd_vcbus_write(reg_vsync_ve, pconf->timing.vs_ve_addr);
}

static void lcd_venc_set_timing(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int hstart, hend, vstart, vend;
	unsigned int offset;
	unsigned int pre_hde, pre_vde, pre_de_vs, pre_de_ve, pre_de_hs, pre_de_he;

	offset = pdrv->data->offset_venc[pdrv->index];
	hstart = pconf->timing.hstart;
	hend = pconf->timing.hend;
	vstart = pconf->timing.vstart;
	vend = pconf->timing.vend;

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 0, 15, 1); /* bit[15] shadown disable */

	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, pconf->timing.act_timing.h_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, pconf->timing.act_timing.v_period - 1);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, hstart);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END + offset,   hend);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, vstart);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset, vend);
	if (pconf->basic.lcd_type == LCD_P2P ||
	    pconf->basic.lcd_type == LCD_MLVDS) {
		pre_vde = pconf->timing.pre_de_v ? pconf->timing.pre_de_v : 8;
		pre_hde = pconf->timing.pre_de_h ? pconf->timing.pre_de_h : PRE_DE_DELAY;
		pre_de_vs = pconf->timing.vstart - pre_vde;
		pre_de_ve = pconf->timing.act_timing.v_active + pre_de_vs;
		pre_de_hs = pconf->timing.hstart + pre_hde;
		pre_de_he = pconf->timing.act_timing.h_active - 1 + pre_de_hs;
		lcd_vcbus_write(ENCL_VIDEO_V_PRE_DE_BLINE + offset, pre_de_vs);
		lcd_vcbus_write(ENCL_VIDEO_V_PRE_DE_ELINE + offset, pre_de_ve);
		lcd_vcbus_write(ENCL_VIDEO_H_PRE_DE_BEGIN + offset, pre_de_hs);
		lcd_vcbus_write(ENCL_VIDEO_H_PRE_DE_END + offset,   pre_de_he);
	}

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, pconf->timing.hs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END + offset,   pconf->timing.hs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, pconf->timing.vs_hs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END + offset,   pconf->timing.vs_he_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, pconf->timing.vs_vs_addr);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, pconf->timing.vs_ve_addr);

	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T5W:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T6D:
	case LCD_CHIP_T6W:
		/*[15:14]: 2'b10 or 2'b01*/
		lcd_vcbus_write(ENCL_INBUF_CNTL1 + offset,
				(2 << 14) | (pconf->timing.act_timing.h_active - 1));
		lcd_vcbus_write(ENCL_INBUF_CNTL0 + offset, 0x200);
		break;
	case LCD_CHIP_T7:
		lcd_vcbus_write(ENCL_INBUF_CNTL1 + offset,
				(5 << 13) | (pconf->timing.act_timing.h_active - 1));
		lcd_vcbus_write(ENCL_INBUF_CNTL0 + offset, 0x200);
		break;
	case LCD_CHIP_T3:
		lcd_vcbus_write(ENCL_INBUF_CNTL1 + offset,
				(4 << 13) | (pconf->timing.act_timing.h_active - 1));
		lcd_vcbus_write(ENCL_INBUF_CNTL0 + offset, 0x200);
		break;
	default:
		break;
	}

	lcd_venc_set_tcon(pdrv);

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 1, 15, 1); /* bit[15] shadown en */
}

static void lcd_venc_set_dummy_t6d(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc[pdrv->index];

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 0, 15, 1); /* bit[15] shadown disable */

	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, 951);
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, 524);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_BEGIN + offset, 80);
	lcd_vcbus_write(ENCL_VIDEO_HAVON_END + offset, 799);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_BLINE + offset, 22);
	lcd_vcbus_write(ENCL_VIDEO_VAVON_ELINE + offset, 501);

	lcd_vcbus_write(ENCL_VIDEO_HSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_HSO_END + offset,   20);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BEGIN + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_END + offset,   0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_BLINE + offset, 0);
	lcd_vcbus_write(ENCL_VIDEO_VSO_ELINE + offset, 5);

	lcd_vcbus_setb(ENCL_VIDEO_MODE + offset, 1, 15, 1); /* bit[15] shadown en */
}

static void dual_set_t7(struct aml_lcd_drv_s *pdrv, u8 en, u8 to_port, u8 dual_mode)
{
	// uint16_t single_ha = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / 2;

	if (pdrv->index != 0) {
		LCD_PR(pdrv, "dual-port on VENC %u not supported", pdrv->index);
		return;
	}

	if (to_port != 1) {
		LCD_PR(pdrv, "dual-port to PORT-%c not supported", 'A' + to_port);
		return;
	}

	if (en == 0) {
		lcd_vcbus_write(VPU_VENC_RGN_CTRL, 0);
		return;
	}

	LCD_PR(pdrv, "set dual split %s mode (%c->[%c] | %c->[%c])",
		(dual_mode == LCD_DUAL_PORT_L_R || dual_mode == LCD_DUAL_PORT_R_L) ?
			"Left-Right" : "Odd-Even",
		dual_mode == LCD_DUAL_PORT_L_R ? 'L' :
			(dual_mode == LCD_DUAL_PORT_R_L ? 'R' :
				(dual_mode == LCD_DUAL_PORT_O_E ? 'O' : 'E')), 'A' + pdrv->index,
		dual_mode == LCD_DUAL_PORT_L_R ? 'R' :
			(dual_mode == LCD_DUAL_PORT_R_L ? 'L' :
				(dual_mode == LCD_DUAL_PORT_O_E ? 'E' : 'O')), 'A' + to_port);

	if (pdrv->curr_dev->dev_cfg.timing.act_timing.h_active % 2 ||
		pdrv->curr_dev->dev_cfg.timing.act_timing.hsync_width % 2 ||
		pdrv->curr_dev->dev_cfg.timing.act_timing.hsync_bp % 2 ||
		pdrv->curr_dev->dev_cfg.timing.act_timing.hsync_fp % 2) {
		LCD_PR(pdrv, "dual-port H-active/bp/fp/sync should be even value");
	}

	// lcd_clk_setb(CLKCTRL_VID_CLK0_CTRL2, 1, 16, 1);

	/* @reg: VPU_VENC_RGN_RSIZE 0x278a
	 * [23:12] reg_region0_size: output region0 size (hsize+1)/2
	 * [11:0]  reg_region1_size: output region0 size hsize - reg_region0_size
	 */

	lcd_vcbus_write(VPU_VENC_RGN_RSIZE,
		(((pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / 2) << 12) |
		  (pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / 2)));
	/* @reg: VPU_DISP_WRAP_CTRL 0x278b
	 * [7:5] reg_difx_link_prot: 0:disp0 sync venc0 1:disp1 sync venc0 2:disp2 sync venc0
	 * [4] reg_splt2_mode: enable 1ppc->2ppc
	 * [2:0] reg_venc0_difx_link[2:0]:
	 */
	lcd_vcbus_write(VPU_DISP_WRAP_CTRL, 0x13);

	/* @reg: VPU_VENC_RGN_CTRL 0x2789
	 * [11:10] reg_gclk_ctrl: ram clk
	 * [9:8] reg_gclk_ctrl: logic clk
	 * [7] reg_rgn_swap: 0:(0-left, 1right) 1:converse
	 * [6] sw_rst: rgn_buffer soft reset
	 * [5] reg_sync_ctrl: rgn buffer related en signal sync enable
	 * [4] reg_sync_ctrl: vsync polarity 0:up edge 1:down edge
	 * [3] reg_vsync_ctrl: vsync polarity 0:positive 1:negative
	 * [2] reg_hsync_ctrl: hsync polarity 0:positive 1:negative
	 * [1] oe_sp_en: odd even split enable
	 * [0] rgn_en: vbo rgn_buffer enable
	 */
	lcd_vcbus_write(VPU_VENC_RGN_CTRL,
		((dual_mode == LCD_DUAL_PORT_L_R || dual_mode == LCD_DUAL_PORT_R_L) << 0 |
		 (dual_mode == LCD_DUAL_PORT_R_L || dual_mode == LCD_DUAL_PORT_E_O) << 7));
}

static void lcd_venc_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_disp_viu_ctrl, offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE_ADV + offset, 0x0418); /* Sampling rate: 1 */
	lcd_vcbus_write(ENCL_VIDEO_FILT_CTRL + offset, 0x1000); /* bypass filter */
	if (pdrv->data->chip_type >= LCD_CHIP_T6D) {
		lcd_vcbus_setb(ENCL_TST_EN + offset, 3, 1, 2); /* mode and en vsync latch */
		lcd_vcbus_setb(ENCL_VIDEO_MODE_ADV + offset, 1, 8, 1); /* vfifo_en vsync latch */
	}
	if (pdrv->status & LCD_CTRL_DUMMY)
		lcd_venc_set_dummy_t6d(pdrv);
	else
		lcd_venc_set_timing(pdrv);

	lcd_vcbus_write(ENCL_VIDEO_RGBIN_CTRL + offset, 3);
	//restore test pattern
	lcd_venc_bist_set(pdrv, pdrv->test_state);

	lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);

	switch (pdrv->index) {
	case 0:
		reg_disp_viu_ctrl = VPU_DISP_VIU0_CTRL;
		break;
	case 1:
		reg_disp_viu_ctrl = VPU_DISP_VIU1_CTRL;
		break;
	case 2:
		reg_disp_viu_ctrl = VPU_DISP_VIU2_CTRL;
		break;
	default:
		LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
		return;
	}

	/*
	 * bit31: lvds enable
	 * bit30: vx1 enable
	 * bit29: hdmitx enable
	 * bit28: dsi_edp enable
	 */
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
		lcd_vcbus_write(reg_disp_viu_ctrl, (1 << 31) |
						(0 << 30) |
						(0 << 29) |
						(0 << 28));
		break;
	case LCD_VBYONE:
		lcd_vcbus_write(reg_disp_viu_ctrl, (0 << 31) |
						(1 << 30) |
						(0 << 29) |
						(0 << 28));
		break;
	case LCD_MIPI:
		lcd_vcbus_write(reg_disp_viu_ctrl, (0 << 31) |
						(0 << 30) |
						(0 << 29) |
						(1 << 28));
		break;
	default:
		break;
	}
	lcd_vcbus_setb(VPU_VENC_CTRL + offset, 2, 0, 2);

	lcd_venc_gamma_init(pdrv);

	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MIPI &&
	    pdrv->curr_dev->dev_cfg.control.mipi_cfg.multi_port_cfg & BIT(0)) {
		dual_set_t7(pdrv, 1, 1,
			(pdrv->curr_dev->dev_cfg.control.mipi_cfg.multi_port_cfg >> 4) & 0Xf);
	}
}

static void lcd_venc_change_framerate(struct aml_lcd_drv_s *pdrv)
{
	unsigned int htotal, vtotal, offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	htotal = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT + offset) + 1;
	vtotal = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT + offset) + 1;

	if (pdrv->curr_dev->dev_cfg.timing.act_timing.h_period != htotal) {
		lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset,
				pdrv->curr_dev->dev_cfg.timing.act_timing.h_period - 1);
	}
	if (pdrv->curr_dev->dev_cfg.timing.act_timing.v_period != vtotal) {
		lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset,
				pdrv->curr_dev->dev_cfg.timing.act_timing.v_period - 1);
	}
}

static void lcd_venc_enable_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	if (flag)
		lcd_vcbus_write(ENCL_VIDEO_EN + offset, 1);
	else
		lcd_vcbus_write(ENCL_VIDEO_EN + offset, 0);
}

static int lcd_venc_get_state(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset, init_state;

	offset = pdrv->data->offset_venc[pdrv->index];

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN + offset);
	return init_state;
}

static int lcd_venc_get_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int offset, init_state;
	unsigned int val = 0;
	struct lcd_boot_ctrl_s *boot_ctrl = pdrv->boot_ctrl;
	unsigned char valid;

	offset = pdrv->data->offset_venc[pdrv->index];

	pconf->timing.act_timing.h_active = lcd_vcbus_read(ENCL_VIDEO_HAVON_END + offset)
		- lcd_vcbus_read(ENCL_VIDEO_HAVON_BEGIN + offset) + 1;
	pconf->timing.act_timing.v_active = lcd_vcbus_read(ENCL_VIDEO_VAVON_ELINE + offset)
		- lcd_vcbus_read(ENCL_VIDEO_VAVON_BLINE + offset) + 1;
	pconf->timing.act_timing.h_period = lcd_vcbus_read(ENCL_VIDEO_MAX_PXCNT + offset) + 1;
	pconf->timing.act_timing.v_period = lcd_vcbus_read(ENCL_VIDEO_MAX_LNCNT + offset) + 1;

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN + offset);

	val = lcd_vcbus_read(L_STH1_HS_ADDR_T7 + offset);
	valid = (val >> 9) & 0xf;
	if (boot_ctrl && valid == 0xa) {
		boot_ctrl->init_level = val & 0xf;
		boot_ctrl->if_state = (val >> 4) & 0x1;
		boot_ctrl->dccd_flag = (val >> 5) & 0x1;
		boot_ctrl->mute_flag = (val >> 6) & 0x1;

		val = lcd_vcbus_read(L_STH1_HE_ADDR_T7 + offset);
		boot_ctrl->frame_rate = val & 0x1fff;

		val = lcd_vcbus_read(L_STH1_VS_ADDR_T7 + offset);
		boot_ctrl->lcd_type = val & 0xf;
		boot_ctrl->clk_mode = (val >> 4) & 0xf;
		boot_ctrl->ppc = (val >> 8) & 0x3;
		boot_ctrl->bl_state = (val >> 11) & 0x1;

		val = lcd_vcbus_read(L_STH1_VE_ADDR_T7 + offset);
		boot_ctrl->advanced_flag = val & 0xff;

		if (boot_ctrl->mute_flag) {
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
			set_output_mute(true);
			pdrv->mute_flag = 1;
#endif
		}

		init_state |= 0x2;
	}

	return init_state;
}

static void lcd_venc_set_vrr_recovery(struct aml_lcd_drv_s *pdrv)
{
	unsigned int vtotal, offset;

	offset = pdrv->data->offset_venc[pdrv->index];

	vtotal = pdrv->curr_dev->dev_cfg.timing.act_timing.v_period;

	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, vtotal - 1);
}

static unsigned int lcd_venc_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg, offset, cnt;

	if (!pdrv)
		return 0;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = VPU_VENCP_STAT + offset;

	cnt = lcd_vcbus_getb(reg, 16, 13);
	return cnt;
}

static unsigned int lcd_venc_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg, offset, cnt;

	if (!pdrv)
		return 0;

	offset = pdrv->data->offset_venc[pdrv->index];
	reg = VPU_VENCP_STAT + offset;

	cnt = lcd_vcbus_getb(reg, 13, 3);
	return cnt;
}

static void lcd_venc_set_htotal(struct aml_lcd_drv_s *pdrv, unsigned int htotal)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];
	lcd_vcbus_write(ENCL_VIDEO_MAX_PXCNT + offset, htotal - 1);
}

static void lcd_venc_set_vtotal(struct aml_lcd_drv_s *pdrv, unsigned int vtotal)
{
	unsigned int offset;

	offset = pdrv->data->offset_venc[pdrv->index];
	lcd_vcbus_write(ENCL_VIDEO_MAX_LNCNT + offset, vtotal - 1);
}

static int lcd_venc_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i, n, len = 0;
	unsigned int *reg_table = NULL, size_encl = 0;
	unsigned int encl_0_reg[] = {
		VPU_VIU_VENC_MUX_CTRL,
		ENCL_VIDEO_EN,
		ENCL_VIDEO_MODE,
		ENCL_VIDEO_MODE_ADV,
		ENCL_VIDEO_MAX_PXCNT,
		ENCL_VIDEO_MAX_LNCNT,
		ENCL_VIDEO_HAVON_BEGIN,
		ENCL_VIDEO_HAVON_END,
		ENCL_VIDEO_VAVON_BLINE,
		ENCL_VIDEO_VAVON_ELINE,
		ENCL_VIDEO_HSO_BEGIN,
		ENCL_VIDEO_HSO_END,
		ENCL_VIDEO_VSO_BEGIN,
		ENCL_VIDEO_VSO_END,
		ENCL_VIDEO_VSO_BLINE,
		ENCL_VIDEO_VSO_ELINE,
		ENCL_VIDEO_H_PRE_DE_BEGIN,
		ENCL_VIDEO_H_PRE_DE_END,
		ENCL_VIDEO_V_PRE_DE_BLINE,
		ENCL_VIDEO_V_PRE_DE_ELINE,
		ENCL_VIDEO_RGBIN_CTRL,
		ENCL_INBUF_CNTL0,
		ENCL_INBUF_CNTL1,
		VPU_DISP_VIU0_CTRL,
		VPU_VENC_CTRL,
		LCD_GAMMA_CNTL_PORT0,
		LCD_RGB_BASE_ADDR,
		LCD_RGB_COEFF_ADDR,
		LCD_POL_CNTL_ADDR,
		LCD_DITH_CNTL_ADDR
	};
	unsigned int encl_1_reg[] = {
		VPU_VIU_VENC_MUX_CTRL,
		ENCL_VIDEO_EN + 0x600,
		ENCL_VIDEO_MODE + 0x600,
		ENCL_VIDEO_MODE_ADV + 0x600,
		ENCL_VIDEO_MAX_PXCNT + 0x600,
		ENCL_VIDEO_MAX_LNCNT + 0x600,
		ENCL_VIDEO_HAVON_BEGIN + 0x600,
		ENCL_VIDEO_HAVON_END + 0x600,
		ENCL_VIDEO_VAVON_BLINE + 0x600,
		ENCL_VIDEO_VAVON_ELINE + 0x600,
		ENCL_VIDEO_HSO_BEGIN + 0x600,
		ENCL_VIDEO_HSO_END + 0x600,
		ENCL_VIDEO_VSO_BEGIN + 0x600,
		ENCL_VIDEO_VSO_END + 0x600,
		ENCL_VIDEO_VSO_BLINE + 0x600,
		ENCL_VIDEO_VSO_ELINE + 0x600,
		ENCL_VIDEO_RGBIN_CTRL + 0x600,
		ENCL_INBUF_CNTL0 + 0x600,
		ENCL_INBUF_CNTL1 + 0x600,
		VPU_DISP_VIU1_CTRL,
		VPU_VENC_CTRL + 0x600,
		LCD_GAMMA_CNTL_PORT0 + 0x100,
		LCD_RGB_BASE_ADDR + 0x100,
		LCD_RGB_COEFF_ADDR + 0x100,
		LCD_POL_CNTL_ADDR + 0x100,
		LCD_DITH_CNTL_ADDR + 0x100
	};
	unsigned int encl_2_reg[] = {
		VPU_VIU_VENC_MUX_CTRL,
		ENCL_VIDEO_EN + 0x800,
		ENCL_VIDEO_MODE + 0x800,
		ENCL_VIDEO_MODE_ADV + 0x800,
		ENCL_VIDEO_MAX_PXCNT + 0x800,
		ENCL_VIDEO_MAX_LNCNT + 0x800,
		ENCL_VIDEO_HAVON_BEGIN + 0x800,
		ENCL_VIDEO_HAVON_END + 0x800,
		ENCL_VIDEO_VAVON_BLINE + 0x800,
		ENCL_VIDEO_VAVON_ELINE + 0x800,
		ENCL_VIDEO_HSO_BEGIN + 0x800,
		ENCL_VIDEO_HSO_END + 0x800,
		ENCL_VIDEO_VSO_BEGIN + 0x800,
		ENCL_VIDEO_VSO_END + 0x800,
		ENCL_VIDEO_VSO_BLINE + 0x800,
		ENCL_VIDEO_VSO_ELINE + 0x800,
		ENCL_VIDEO_RGBIN_CTRL + 0x800,
		ENCL_INBUF_CNTL0 + 0x800,
		ENCL_INBUF_CNTL1 + 0x800,
		VPU_DISP_VIU2_CTRL,
		VPU_VENC_CTRL + 0x800,
		LCD_GAMMA_CNTL_PORT0 + 0x200,
		LCD_RGB_BASE_ADDR + 0x200,
		LCD_RGB_COEFF_ADDR + 0x200,
		LCD_POL_CNTL_ADDR + 0x200,
		LCD_DITH_CNTL_ADDR + 0x200
	};

	if (!pdrv)
		return 0;

	if (pdrv->index == 2) {
		reg_table = encl_2_reg;
		size_encl = ARRAY_SIZE(encl_2_reg);
	} else if (pdrv->index == 1) {
		reg_table = encl_1_reg;
		size_encl = ARRAY_SIZE(encl_1_reg);
	} else {
		reg_table = encl_0_reg;
		size_encl = ARRAY_SIZE(encl_0_reg);
	}
	for (i = 0; i < size_encl; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "vcbus [0x%04x] = 0x%08x\n",
			reg_table[i], lcd_vcbus_read(reg_table[i]));
	}

	return len;
}

int lcd_venc_op_init_t7(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op)
{
	if (!venc_op)
		return -1;

	venc_op->wait_vsync = lcd_venc_wait_vsync;
	venc_op->get_max_lcnt = lcd_venc_get_max_lint_cnt;
	venc_op->gamma_test_en = lcd_venc_gamma_debug_test_en;
	venc_op->venc_debug_test = lcd_venc_bist_set;
	venc_op->venc_set_timing = lcd_venc_set_timing;
	venc_op->venc_set = lcd_venc_set;
	venc_op->venc_fr_change = lcd_venc_change_framerate;
	venc_op->venc_enable = lcd_venc_enable_ctrl;
	venc_op->get_venc_state = lcd_venc_get_state;
	venc_op->get_venc_init_config = lcd_venc_get_init_config;
	venc_op->venc_vrr_recovery = lcd_venc_set_vrr_recovery;
	venc_op->get_encl_line_cnt = lcd_venc_get_encl_line_cnt;
	venc_op->get_encl_frm_cnt = lcd_venc_get_encl_frm_cnt;
	venc_op->venc_set_htotal = lcd_venc_set_htotal;
	venc_op->venc_set_vtotal = lcd_venc_set_vtotal;
	venc_op->venc_reg_dump = lcd_venc_reg_dump;
	if (pdata->chip_type == LCD_CHIP_T6D || pdata->chip_type == LCD_CHIP_T6W) {
		venc_op->venc_set_dummy = lcd_venc_set_dummy_t6d;
		venc_op->venc_bist_change = lcd_venc_bist_change;
		//venc_op->mute_set = lcd_venc_mute_set;
	}

	return 0;
};
