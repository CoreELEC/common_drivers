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

static inline
unsigned int __cal_h_timing_to_reg(unsigned int origin, unsigned int ppc)
{
	unsigned int val = 0;

	val = origin / ppc;
	return  (val > 0 ? val - 1 : 0);
}

static inline
unsigned int __lcd_round_inc(unsigned int base, unsigned int val, unsigned int round_const)
{
	unsigned int res;

	res = base + val;
	return res < round_const ? res : res % round_const;
}

static void lcd_venc_wait_vsync(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt, line_cnt_previous;
	unsigned int reg, offset;
	int i = 0;

	if (!pdrv)
		return;

	offset = pdrv->index == 1 ? 0x600 : 0x00;
	reg = VPU_VENCP_STAT + (offset << 2);

	line_cnt = 0x1fff;
	line_cnt_previous = lcd_vcbus_getb(reg, 0, 13);
	while (i++ < LCD_WAIT_VSYNC_TIMEOUT) {
		line_cnt = lcd_vcbus_getb(reg, 0, 13);
		if (line_cnt < line_cnt_previous)
			break;
		line_cnt_previous = line_cnt;
		udelay(2);
	}
}

static unsigned int lcd_venc_get_max_lint_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt;

	line_cnt = lcd_vcbus_getb(ENCL_VIDEO_MAX_CNT_T6X, 0, 16) + 1;
	/*LCDPR("[%d]: %s: line_cnt=%d", pdrv->index, __func__, line_cnt); */

	return line_cnt;
}

static void lcd_venc_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int reg;

	reg = LCD_GAMMA_CNTL_PORT0_T6X;

	if (flag) {
		if (lcd_vcbus_getb(reg, 0, 1) == 0) {
			lcd_vcbus_setb(reg, 1, 0, 1);
			LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, flag);
		}
	} else {
		if (lcd_vcbus_getb(reg, 0, 1)) {
			lcd_vcbus_setb(reg, 0, 0, 1);
			LCDPR("[%d]: %s: %d\n", pdrv->index, __func__, flag);
		}
	}
}

#define LCD_ENC_TST_NUM_MAX    10
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

static struct lcd_enc_test_t lcd_enc_tst[] = {
	/*          tst_mode,   Y/R,  Cb/G,   Cr/B,  tst_en,  vfifo_en  rgbin*/
	{"0-none",         0, 0x200,      0x200,  0x200,   0,      1,        1},
	{"1-Color Bar",    1, 0x200,      0x200,  0x200,   1,      0,        0},  /* 1 */
	{"2-Thin Line",    2, 0x200,      0x200,  0x200,   1,      0,        0},  /* 2 */
	{"3-Dot Grid",     3, 0x200,      0x200,  0x200,   1,      0,        0},  /* 3 */
	{"4-Gray",         0, 0x1ff,      0x1ff,  0x1ff,   1,      0,        1},  /* 4 */
	{"5-Red",          0, 0x3ff,      0x000,  0x000,   1,      0,        1},  /* 5 */
	{"6-Green",        0, 0x000,      0x3ff,  0x000,   1,      0,        1},  /* 6 */
	{"7-Blue",         0, 0x000,      0x000,  0x3ff,   1,      0,        1},  /* 7 */
	{"8-Black",        0, 0x000,      0x000,  0x000,   1,      0,        1},  /* 8 */
	{"9-X icon",       4, 0x200,      0x200,  0x200,   1,      0,        0},  /* 9 */

	/*                 bit[9:0]=step_dat   bit[2:0]=init_sel bit[2:0]=step_sel
	 *                                      bit[9:3]=rsvd     bit[9:3]=step_wid
	 */
	{"10-Gray Scale",  5,   0x1,            0x0,              0x7,           1, 0, 1}, /* 10 */
	{"11-Red Scale",   5,   0x1,            0x0,              0x1,           1, 0, 1}, /* 11 */
	{"12-Green Scale", 5,   0x1,            0x0,              0x2,           1, 0, 1}, /* 12 */
	{"13-Blue Scale",  5,   0x1,            0x0,              0x4,           1, 0, 1}, /* 13 */

};

static int lcd_venc_bist_set(struct aml_lcd_drv_s *pdrv, unsigned int num)
{
	unsigned int hstart, vstart, width, height, ppc, step;
	unsigned int cur_test_num = 0;

	cur_test_num = ARRAY_SIZE(lcd_enc_tst);
	if (num >= cur_test_num)
		return -1;

	ppc = pdrv->curr_dev->dev_cfg.timing.ppc;

	hstart = pdrv->curr_dev->dev_cfg.timing.hstart / ppc - 1;
	vstart = pdrv->curr_dev->dev_cfg.timing.vstart;
	height = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;

	if (num == 9) {
		width = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / ppc;
		step = width * 256 / height;
		lcd_vcbus_setb(ENCL_TST_DATA_T6X, step, 10, 10);//px step
		lcd_vcbus_setb(ENCL_TST_DATA_T6X, 0, 0, 10);//X line width
		lcd_vcbus_setb(ENCL_TST_DATA_T6X, 0x200, 20, 10);//color Y/R
		lcd_vcbus_setb(ENCL_TST_EN_T6X, 1, 12, 2);//rectangle width
		hstart -= 1;
	} else {
		width = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active / ppc / 8 - 1;
		hstart -= 2;
		lcd_vcbus_setb(ENCL_TST_DATA_T6X, lcd_enc_tst[num].y, 20, 10);//color Y/R
		lcd_vcbus_setb(ENCL_TST_DATA_T6X, lcd_enc_tst[num].cb, 10, 10);//color cb/G
		lcd_vcbus_setb(ENCL_TST_DATA_T6X, lcd_enc_tst[num].cr, 0, 10);//color cr/B
		if (lcd_enc_tst[num].mode == 5)
			lcd_vcbus_setb(ENCL_TST_CLRBAR_STRT, hstart,  0, 10);//ofst_x
	}

	lcd_vcbus_setb(ENCL_TST_CLRBAR_CNTL_T6X, hstart, 0, 13);//color bar x start
	lcd_vcbus_setb(ENCL_TST_CLRBAR_CNTL_T6X, width, 16, 13);//color bar  width
	lcd_vcbus_setb(ENCL_TST_CRSBAR_CNTL_T6X, vstart, 0, 13);//color bar y start
	lcd_vcbus_setb(ENCL_TST_CRSBAR_CNTL_T6X, height, 16, 13);//color bar  height
	lcd_vcbus_setb(ENCL_TST_EN_T6X, lcd_enc_tst[num].mode, 0, 8);//encl_test_model_sel
	lcd_vcbus_setb(ENCL_TST_EN_T6X, lcd_enc_tst[num].en, 8, 1);//enable

	lcd_vcbus_setb(ENCL_VIDEO_VSRC_CTRL_T6X, lcd_enc_tst[num].vfifo_en, 4, 1);
	/* 1: rgb to rgb   0: yuv to rgb) */
	lcd_vcbus_setb(ENCL_VIDEO_MODE_T6X, lcd_enc_tst[num].rgb_in, 16,  1);

	if (num > 0)
		LCDPR("[%d]: show test pattern: %s\n", pdrv->index, lcd_enc_tst[num].name);

	return 0;
}

static void lcd_venc_bist_change(struct aml_lcd_drv_s *pdrv, unsigned int level_r,
				 unsigned int level_g, unsigned int level_b)
{
	struct lcd_enc_test_t *pcur_test = lcd_enc_tst;
	unsigned int bist_num, data;

	if (level_r > 0x3ff) {
		bist_num = 0;
		data = 0;
	} else {
		bist_num = 8;
		data = ((level_r & 0x3ff) << 20) | ((level_g & 0x3ff) << 10) |
			((level_b & 0x3ff) << 0);
	}

	lcd_vcbus_write(ENCL_TST_DATA_T6X, data);
	lcd_vcbus_setb(ENCL_TST_EN_T6X, pcur_test[bist_num].mode, 0, 8);
	lcd_vcbus_setb(ENCL_TST_EN_T6X, pcur_test[bist_num].en, 8, 1);
	lcd_vcbus_setb(ENCL_VIDEO_MODE_T6X, pcur_test[bist_num].rgb_in, 16, 1);
	lcd_vcbus_setb(ENCL_VIDEO_VSRC_CTRL_T6X, pcur_test[bist_num].vfifo_en, 4, 1);
}

/*
 *static void lcd_venc_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag)
 *{
 *	if (flag) {
 *		lcd_vcbus_write(ENCL_TST_DATA_T6X, 0);
 *		lcd_vcbus_setb(ENCL_TST_EN_T6X, 0, 0, 8);
 *		lcd_vcbus_setb(ENCL_TST_EN_T6X, 1, 8, 1);
 *		lcd_vcbus_setb(ENCL_VIDEO_MODE_T6X, 1, 16, 1);
 *		lcd_vcbus_setb(ENCL_VIDEO_VSRC_CTRL_T6X, 0, 4, 1);
 *	} else {
 *		lcd_vcbus_setb(ENCL_VIDEO_VSRC_CTRL_T6X, 1, 4, 1);
 *		lcd_vcbus_setb(ENCL_TST_EN_T6X, 0, 8, 1);
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

static void lcd_venc_set_htotal(struct aml_lcd_drv_s *pdrv, unsigned int h_period)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int ppc, slice, p2s_px_dly;
	unsigned int hde_px_bgn, hde_px_end;
	unsigned int vde_ln_bgn, vde_ln_end;
	unsigned int hso_px_bgn, hso_px_end;
	unsigned int vso_px_bgn, vso_px_end;
	unsigned int vso_ln_bgn, vso_ln_end;
	unsigned int hact, ht, vt;

	ppc = pconf->timing.ppc;//must check zero in init place

	hde_px_bgn = __cal_h_timing_to_reg(pconf->timing.hstart, ppc);
	hde_px_end = __cal_h_timing_to_reg(pconf->timing.hend + 1, ppc) - 1;

	ht = h_period / ppc;
	vt = pconf->timing.act_timing.v_period;
	hact = hde_px_end - hde_px_bgn + 1;

	lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X, ht - 1, 16, 16);

	if (pconf->basic.lcd_type == LCD_VBYONE) {
		slice = ppc;
		p2s_px_dly = hact  + ((slice == 2) ? hact / 2 : 0);

		vde_ln_bgn = pconf->timing.vstart;
		vde_ln_end = pconf->timing.vend;

		hso_px_bgn =  __cal_h_timing_to_reg(pconf->timing.hs_hs_addr, ppc);
		hso_px_end = __cal_h_timing_to_reg(pconf->timing.hs_he_addr, ppc);

		vso_px_bgn = __cal_h_timing_to_reg(pconf->timing.vs_hs_addr, ppc);
		vso_px_end = __cal_h_timing_to_reg(pconf->timing.vs_he_addr, ppc);

		vso_ln_bgn = pconf->timing.vs_vs_addr;
		vso_ln_end = pconf->timing.vs_ve_addr;

		vde_ln_bgn = __lcd_round_inc(vde_ln_bgn, (hde_px_bgn + p2s_px_dly) / ht, vt);
		vde_ln_end = __lcd_round_inc(vde_ln_end, (hde_px_end + p2s_px_dly) / ht, vt);
		hde_px_bgn = __lcd_round_inc(hde_px_bgn, p2s_px_dly, ht);
		hde_px_end = __lcd_round_inc(hde_px_end + 1, p2s_px_dly, ht);
		hso_px_bgn = __lcd_round_inc(hso_px_bgn, p2s_px_dly, ht);
		hso_px_end = __lcd_round_inc(hso_px_end, p2s_px_dly, ht);
		vso_ln_bgn = __lcd_round_inc(vso_ln_bgn, (vso_px_bgn + p2s_px_dly) / ht, vt);
		vso_ln_end = __lcd_round_inc(vso_ln_end, (vso_px_end + p2s_px_dly) / ht, vt);
		vso_px_bgn = __lcd_round_inc(vso_px_bgn, p2s_px_dly, ht);
		vso_px_end = __lcd_round_inc(vso_px_end, p2s_px_dly, ht);

		lcd_vcbus_setb(ENCL_VIDEO_H_PRE_DE_PX_RNG_T6X, hde_px_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_H_PRE_DE_PX_RNG_T6X, hde_px_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_V_PRE_DE_LN_RNG_T6X, vde_ln_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_V_PRE_DE_LN_RNG_T6X, vde_ln_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_HSO_PRE_PX_RNG_T6X, hso_px_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_HSO_PRE_PX_RNG_T6X, hso_px_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_PX_RNG_T6X, vso_px_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_PX_RNG_T6X, vso_px_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_LN_RNG_T6X, vso_ln_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_LN_RNG_T6X, vso_ln_end, 0, 16);
	}
}

static void lcd_venc_set_timing(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int hstart, hend, vstart, vend, h_period, v_period;
	unsigned int pre_hde, pre_vde, pre_de_vs, pre_de_ve, pre_de_hs, pre_de_he;
	unsigned int hs_hs_addr, hs_he_addr, vs_vs_addr, vs_ve_addr, vs_hs_addr, vs_he_addr;
	unsigned int ppc, slice, p2s_px_dly;
	unsigned int hde_px_bgn, hde_px_end;
	unsigned int vde_ln_bgn, vde_ln_end;
	unsigned int hso_px_bgn, hso_px_end;
	unsigned int vso_px_bgn, vso_px_end;
	unsigned int vso_ln_bgn, vso_ln_end;
	unsigned int hact, ht, vt;

	ppc = pconf->timing.ppc;//must check zero in init place

	hstart = __cal_h_timing_to_reg(pconf->timing.hstart, ppc);
	hend = __cal_h_timing_to_reg(pconf->timing.hend + 1, ppc) - 1;
	hde_px_bgn = hstart;
	hde_px_end = hend;

	vstart = pconf->timing.vstart;
	vend = pconf->timing.vend;
	vde_ln_bgn = vstart;
	vde_ln_end = vend;

	h_period = pconf->timing.act_timing.h_period;
	v_period = pconf->timing.act_timing.v_period;
	ht = h_period / ppc;
	vt = v_period;
	hact = hde_px_end - hde_px_bgn + 1;

	hs_hs_addr =  __cal_h_timing_to_reg(pconf->timing.hs_hs_addr, ppc);
	hs_he_addr = __cal_h_timing_to_reg(pconf->timing.hs_he_addr, ppc);
	hso_px_bgn = hs_hs_addr;
	hso_px_end = hs_he_addr;

	vs_hs_addr = __cal_h_timing_to_reg(pconf->timing.vs_hs_addr, ppc);
	vs_he_addr = __cal_h_timing_to_reg(pconf->timing.vs_he_addr, ppc);
	vso_px_bgn = vs_hs_addr;
	vso_px_end = vs_he_addr;

	vs_vs_addr = pconf->timing.vs_vs_addr;
	vs_ve_addr = pconf->timing.vs_ve_addr;
	vso_ln_bgn = vs_vs_addr;
	vso_ln_end = vs_ve_addr;

	lcd_vcbus_setb(ENCL_VIDEO_MODE_T6X, 0, 15, 1);//bit[15];shadow_disable

	lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X, h_period / ppc - 1, 16, 16);
	lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X, v_period - 1, 0, 16);

	lcd_vcbus_setb(ENCL_VIDEO_HAVON_PX_RNG_T6X, hstart, 16, 16);//HAVON_BEGIN
	lcd_vcbus_setb(ENCL_VIDEO_HAVON_PX_RNG_T6X, hend, 0, 16);//HAVON_END

	lcd_vcbus_setb(ENCL_VIDEO_VAVON_LN_RNG_T6X, vstart, 16, 16);//BLINE
	lcd_vcbus_setb(ENCL_VIDEO_VAVON_LN_RNG_T6X, vend, 0, 16);//ELINE

	lcd_vcbus_setb(ENCL_VIDEO_HSO_PX_RNG_T6X, hs_hs_addr, 16, 16);//HSO_BEGIN
	lcd_vcbus_setb(ENCL_VIDEO_HSO_PX_RNG_T6X, hs_he_addr, 0, 16);//HSO_END

	lcd_vcbus_setb(ENCL_VIDEO_VSO_PX_RNG_T6X, vs_hs_addr, 16, 16);//HSO_BEGIN
	lcd_vcbus_setb(ENCL_VIDEO_VSO_PX_RNG_T6X, vs_he_addr, 0, 16);//HSO_END

	lcd_vcbus_setb(ENCL_VIDEO_VSO_LN_RNG_T6X, vs_vs_addr, 16, 16);//VSO BLINE
	lcd_vcbus_setb(ENCL_VIDEO_VSO_LN_RNG_T6X, vs_ve_addr, 0, 16);//VSO ELINE

	if (pconf->basic.lcd_type == LCD_P2P ||
	    pconf->basic.lcd_type == LCD_MLVDS) {
		pre_vde = pconf->timing.pre_de_v ? pconf->timing.pre_de_v : 16;
		pre_hde = pconf->timing.pre_de_h ? pconf->timing.pre_de_h : 6;
		pre_de_vs = pconf->timing.vstart - pre_vde;
		pre_de_ve = pconf->timing.act_timing.v_active + pre_de_vs;
		pre_de_hs = pconf->timing.hstart + pre_hde;
		pre_de_he = pconf->timing.act_timing.h_active + pre_de_hs;
		lcd_vcbus_setb(ENCL_VIDEO_V_PRE_DE_LN_RNG_T6X, pre_de_vs, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_V_PRE_DE_LN_RNG_T6X, pre_de_ve, 0, 16);
		lcd_vcbus_setb(ENCL_VIDEO_H_PRE_DE_PX_RNG_T6X, pre_de_hs / ppc, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_H_PRE_DE_PX_RNG_T6X, (pre_de_he / ppc) - 1, 0, 16);
	} else if (pconf->basic.lcd_type == LCD_VBYONE) {
		slice = ppc;
		p2s_px_dly = hact  + ((slice == 2) ? hact / 2 : 0);

		vde_ln_bgn = __lcd_round_inc(vde_ln_bgn, (hde_px_bgn + p2s_px_dly) / ht, vt);
		vde_ln_end = __lcd_round_inc(vde_ln_end, (hde_px_end + p2s_px_dly) / ht, vt);
		hde_px_bgn = __lcd_round_inc(hde_px_bgn, p2s_px_dly, ht);
		hde_px_end = __lcd_round_inc(hde_px_end + 1, p2s_px_dly, ht);
		hso_px_bgn = __lcd_round_inc(hso_px_bgn, p2s_px_dly, ht);
		hso_px_end = __lcd_round_inc(hso_px_end, p2s_px_dly, ht);
		vso_ln_bgn = __lcd_round_inc(vso_ln_bgn, (vso_px_bgn + p2s_px_dly) / ht, vt);
		vso_ln_end = __lcd_round_inc(vso_ln_end, (vso_px_end + p2s_px_dly) / ht, vt);
		vso_px_bgn = __lcd_round_inc(vso_px_bgn, p2s_px_dly, ht);
		vso_px_end = __lcd_round_inc(vso_px_end, p2s_px_dly, ht);

		lcd_vcbus_setb(ENCL_VIDEO_H_PRE_DE_PX_RNG_T6X, hde_px_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_H_PRE_DE_PX_RNG_T6X, hde_px_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_V_PRE_DE_LN_RNG_T6X, vde_ln_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_V_PRE_DE_LN_RNG_T6X, vde_ln_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_HSO_PRE_PX_RNG_T6X, hso_px_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_HSO_PRE_PX_RNG_T6X, hso_px_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_PX_RNG_T6X, vso_px_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_PX_RNG_T6X, vso_px_end, 0, 16);

		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_LN_RNG_T6X, vso_ln_bgn, 16, 16);
		lcd_vcbus_setb(ENCL_VIDEO_VSO_PRE_LN_RNG_T6X, vso_ln_end, 0, 16);
	} else if (pconf->basic.lcd_type == LCD_LVDS) {
		lcd_vcbus_setb(LCD_LCD_IF_CTRL, 1, 28, 3);//rgb pol
		// refs to lcd_lvds.c@lcd_lvds_enable
		if (pconf->timing.act_timing.vsync_pol == pconf->timing.act_timing.hsync_pol)
			lcd_vcbus_setb(LCD_LCD_IF_CTRL, 1, 29, 1);
	}

	lcd_vcbus_write(ENCL_INBUF_CNTL0_T6X, 0x2008000 | (pconf->timing.act_timing.h_active - 1));

	lcd_vcbus_setb(ENCL_VIDEO_MODE_T6X, 1, 15, 1);//bit[15];shadow_en
}

static void lcd_venc_set_dummy(struct aml_lcd_drv_s *pdrv)
{
	lcd_vcbus_setb(ENCL_VIDEO_MODE_T6X, 0, 15, 1);//bit[15];shadow_disable

	lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X, 951, 16, 16);
	lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X, 524, 0, 16);

	lcd_vcbus_setb(ENCL_VIDEO_HAVON_PX_RNG_T6X, 80, 16, 16);//HAVON_BEGIN
	lcd_vcbus_setb(ENCL_VIDEO_HAVON_PX_RNG_T6X, 799, 0, 16);//HAVON_END

	lcd_vcbus_setb(ENCL_VIDEO_VAVON_LN_RNG_T6X, 22, 16, 16);//BLINE
	lcd_vcbus_setb(ENCL_VIDEO_VAVON_LN_RNG_T6X, 501, 0, 16);//ELINE

	lcd_vcbus_setb(ENCL_VIDEO_HSO_PX_RNG_T6X, 0, 16, 16);//HSO_BEGIN
	lcd_vcbus_setb(ENCL_VIDEO_HSO_PX_RNG_T6X, 20, 0, 16);//HSO_END

	lcd_vcbus_setb(ENCL_VIDEO_VSO_PX_RNG_T6X, 0, 16, 16);//HSO_BEGIN
	lcd_vcbus_setb(ENCL_VIDEO_VSO_PX_RNG_T6X, 0, 0, 16);//HSO_END

	lcd_vcbus_setb(ENCL_VIDEO_VSO_LN_RNG_T6X, 0, 16, 16);//VSO BLINE
	lcd_vcbus_setb(ENCL_VIDEO_VSO_LN_RNG_T6X, 5, 0, 16);//VSO ELINE

	lcd_vcbus_setb(ENCL_VIDEO_MODE_T6X, 1, 15, 1);//bit[15];shadow_en
}

static void lcd_venc_set(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_disp_viu_ctrl;
	unsigned int frm_hsize, reg_data;//, vpp0_ppc_num;
	unsigned int ppc = 1;

	if (!pdrv || !pdrv->data)
		return;

	ppc = pdrv->curr_dev->dev_cfg.timing.ppc;

	lcd_vcbus_write(ENCL_VIDEO_EN_T6X, 0);

	lcd_vcbus_write(ENCL_VIDEO_MODE_T6X, 0x18040);//bit[15];shadow_en
	lcd_vcbus_setb(ENCL_VIDEO_VSRC_CTRL_T6X, 1, 4, 1);//reg_vfifo_en
	/*vfifo_upmode, 0:1ppc,1:2ppc,2, 4ppc */
	lcd_vcbus_setb(ENCL_VIDEO_VSRC_CTRL_T6X, 0, 0, 3);

	if (pdrv->status & LCD_CTRL_DUMMY)
		lcd_venc_set_dummy(pdrv);
	else
		lcd_venc_set_timing(pdrv);

	//restore test pattern
	lcd_venc_bist_set(pdrv, pdrv->test_state);

	if (pdrv->index == 0) {
		reg_data  = lcd_vcbus_read(ENCL_VIDEO_HAVON_PX_RNG_T6X);
		frm_hsize = (reg_data & 0xFFFF) - ((reg_data >> 16) & 0xFFFF) + 1;
		//vpp0_ppc_num  = lcd_vcbus_read(VPP_4S4P_CTRL) & 3;	//2: 1ppc, 1: 2ppc, 0: 4ppc
		//s2p enable, only for venc0
		lcd_vcbus_setb(VPU_VENC_DITH, frm_hsize, 0,  13); //hsize
		lcd_vcbus_setb(VPU_INTF_CTRL_T6X, ppc == 1 ? 1 : 0, 30, 1);
	}

	lcd_vcbus_setb(VPU_VENC_CTRL, 2, 0, 2);// enable timming
	lcd_vcbus_setb(ENCL_VIDEO_EN_T6X, 1, 0, 1);//enable

	lcd_vcbus_setb(LCD_LCD_IF_CTRL_T6X, (0x400 << 12), 0, 23);
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_P2P &&
		pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_MLVDS) {
		switch (pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits) {
		case 18:
			lcd_vcbus_write(LCD_DITH_CTRL_T6X,   0x600);
			break;
		case 24:
			lcd_vcbus_write(LCD_DITH_CTRL_T6X,   0x400);
			break;
		case 30:
		default:
			lcd_vcbus_write(LCD_DITH_CTRL_T6X,   0x0);
			break;
		}

	} else {
		lcd_vcbus_write(LCD_DITH_CTRL_T6X,   0x0);
	}

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
	case LCD_EDP:
		lcd_vcbus_write(reg_disp_viu_ctrl, (0 << 31) |
						(0 << 30) |
						(0 << 29) |
						(1 << 28));
		break;
	default:
		break;
	}

	lcd_venc_gamma_init(pdrv);
}

static void lcd_venc_change_framerate(struct aml_lcd_drv_s *pdrv)
{
	unsigned int htotal, vtotal, ppc;

	ppc = pdrv->curr_dev->dev_cfg.timing.ppc;

	htotal = (lcd_vcbus_getb(ENCL_VIDEO_MAX_CNT_T6X, 16, 16) + 1) * ppc;
	vtotal = lcd_vcbus_getb(ENCL_VIDEO_MAX_CNT_T6X, 0, 16) + 1;
	if (pdrv->curr_dev->dev_cfg.timing.act_timing.h_period != htotal) {
		lcd_venc_set_htotal(pdrv,
			pdrv->curr_dev->dev_cfg.timing.act_timing.h_period);
	}
	if (pdrv->curr_dev->dev_cfg.timing.act_timing.v_period != vtotal) {
		lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X,
			pdrv->curr_dev->dev_cfg.timing.act_timing.v_period - 1, 0, 16);
	}
}

static void lcd_venc_enable_ctrl(struct aml_lcd_drv_s *pdrv, int flag)
{
	if (flag)
		lcd_vcbus_write(ENCL_VIDEO_EN_T6X, 1);
	else
		lcd_vcbus_write(ENCL_VIDEO_EN_T6X, 0);
}

static int lcd_venc_get_state(struct aml_lcd_drv_s *pdrv)
{
	unsigned int init_state;

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN_T6X);
	return init_state;
}

static int lcd_venc_get_init_config(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int init_state, size, ppc = 1;

	ppc = 1 << pdrv->boot_ctrl->ppc;

	size = lcd_vcbus_getb(ENCL_VIDEO_HAVON_PX_RNG_T6X, 0, 16)
		- lcd_vcbus_getb(ENCL_VIDEO_HAVON_PX_RNG_T6X, 16, 16) + 1;
	pconf->timing.act_timing.h_active = size * ppc;
	size = lcd_vcbus_getb(ENCL_VIDEO_VAVON_LN_RNG_T6X, 0, 16)
		- lcd_vcbus_getb(ENCL_VIDEO_VAVON_LN_RNG_T6X, 16, 16) + 1;
	pconf->timing.act_timing.v_active = size;
	size = lcd_vcbus_getb(ENCL_VIDEO_MAX_CNT_T6X, 16, 16) + 1;
	pconf->timing.act_timing.h_period = size * ppc;
	size = lcd_vcbus_getb(ENCL_VIDEO_MAX_CNT_T6X, 0, 16) + 1;
	pconf->timing.act_timing.v_period = size;

	init_state = lcd_vcbus_read(ENCL_VIDEO_EN_T6X);
	return init_state;
}

static void lcd_venc_set_vrr_recovery(struct aml_lcd_drv_s *pdrv)
{
	unsigned int vtotal;

	vtotal = pdrv->curr_dev->dev_cfg.timing.act_timing.v_period;

	lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X, (vtotal - 1), 0, 16);
}

static unsigned int lcd_venc_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int line_cnt;

	if (!pdrv)
		return 0;

	line_cnt = lcd_vcbus_getb(VPU_VENCP_STAT, 16, 13);
	return line_cnt;
}

static unsigned int lcd_venc_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv)
{
	unsigned int cnt;

	if (!pdrv)
		return 0;

	cnt = lcd_vcbus_getb(VPU_VENCP_STAT, 13, 3);
	return cnt;
}

static void lcd_venc_set_vtotal(struct aml_lcd_drv_s *pdrv, unsigned int vtotal)
{
	lcd_vcbus_setb(ENCL_VIDEO_MAX_CNT_T6X, (vtotal - 1), 0, 16);
}

static int lcd_venc_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i, n, len = 0;
	unsigned int size_encl = 0;

	struct reg_name_set_s encl_reg[] = {
		{VPU_INTF_CTRL_T6X,                 "VPU_INTF_CTRL           "},
		{VPU_VIU_VENC_MUX_CTRL,             "VPU_VIU_VENC_MUX_CTRL   "},
		{VPU_DISP_VIU0_CTRL,                "VPU_DISP_VIU0_CTRL      "},
		{VPU_VENC_CTRL,                     "VPU_VENC_CTRL           "},
		{VPU_VENC_DITH,                     "VPU_VENC_DITH           "},
		{LCD_GAMMA_CNTL_PORT0_T6X,          "LCD_GAMMA_CNTL_PORT0    "},
		{LCD_LCD_IF_CTRL_T6X,               "LCD_LCD_IF_CTRL         "},
		{LCD_DITH_CTRL_T6X,                 "LCD_DITH_CTRL           "},
		{ENCL_INBUF_CNTL0_T6X,              "ENCL_INBUF_CNTL0        "},
		{ENCL_TST_EN_T6X,                   "ENCL_TST_EN             "},
		{ENCL_TST_DATA_T6X,                 "ENCL_TST_DATA           "},
		{ENCL_TST_CLRBAR_CNTL_T6X,          "ENCL_TST_CLRBAR_CNTL    "},
		{ENCL_TST_CRSBAR_CNTL_T6X,          "ENCL_TST_CRSBAR_CNTL    "},
		{ENCL_DACSEL_0_T6X,                 "ENCL_DACSEL_0           "},
		{ENCL_INBUF_CNTL0_T6X,              "ENCL_INBUF_CNTL0        "},
		{ENCL_INBUF_CNT_T6X,                "ENCL_INBUF_CNT          "},
		{ENCL_INBUF_FIX_PIX_NUM_T6X,        "ENCL_INBUF_FIX_PIX_NUM  "},
		{ENCL_VIDEO_EN_T6X,                 "ENCL_VIDEO_EN           "},
		{ENCL_VIDEO_GAIN_RGB_CTRL_T6X,      "ENCL_VIDEO_GAIN_RGB_CTRL"},
		{ENCL_VIDEO_VSRC_CTRL_T6X,          "ENCL_VIDEO_VSRC_CTRL    "},
		{ENCL_VIDEO_MODE_T6X,               "ENCL_VIDEO_MODE         "},
		{ENCL_VIDEO_MAX_CNT_T6X,            "ENCL_VIDEO_MAX_CNT      "},
		{ENCL_VIDEO_HAVON_PX_RNG_T6X,       "ENCL_VIDEO_HAVON_PX_RNG "},
		{ENCL_VIDEO_VAVON_LN_RNG_T6X,       "ENCL_VIDEO_VAVON_LN_RNG "},
		{ENCL_VIDEO_HSO_PX_RNG_T6X,         "ENCL_VIDEO_HSO_PX_RNG   "},
		{ENCL_VIDEO_VSO_PX_RNG_T6X,         "ENCL_VIDEO_VSO_PX_RNG   "},
		{ENCL_VIDEO_VSO_LN_RNG_T6X,         "ENCL_VIDEO_VSO_LN_RNG   "},
		{ENCL_VIDEO_H_PRE_DE_PX_RNG_T6X,    "ENCL_VID_H_PRE_DE_PX_RNG"},
		{ENCL_VIDEO_V_PRE_DE_LN_RNG_T6X,    "ENCL_VID_V_PRE_DE_LN_RNG"},
		{ENCL_DBG_PX_LN_RST_T6X,            "ENCL_DBG_PX_LN_RST      "},
		{ENCL_DBG_PX_LN_INT_T6X,            "ENCL_DBG_PX_LN_INT      "}
	};

	size_encl = ARRAY_SIZE(encl_reg);
	for (i = 0; i < size_encl; i++) {
		n = lcd_debug_info_len(len);
		len += snprintf((buf + len), n, "%s [0x%04x] = 0x%08x\n",
			encl_reg[i].name,
			encl_reg[i].reg,
			lcd_vcbus_read(encl_reg[i].reg));
	}

	return len;
}

int lcd_venc_op_init_t6x(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op)
{
	if (!venc_op)
		return -1;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("%s %d\n", __func__, __LINE__);

	venc_op->wait_vsync = lcd_venc_wait_vsync;
	venc_op->get_max_lcnt = lcd_venc_get_max_lint_cnt;
	venc_op->gamma_test_en = lcd_venc_gamma_debug_test_en;
	venc_op->venc_debug_test = lcd_venc_bist_set;
	venc_op->venc_set_timing = lcd_venc_set_timing;
	venc_op->venc_set = lcd_venc_set;
	venc_op->venc_fr_change = lcd_venc_change_framerate;
	venc_op->venc_set_dummy = lcd_venc_set_dummy;
	venc_op->venc_enable = lcd_venc_enable_ctrl;
	venc_op->get_venc_state = lcd_venc_get_state;
	venc_op->get_venc_init_config = lcd_venc_get_init_config;
	venc_op->venc_vrr_recovery = lcd_venc_set_vrr_recovery;
	venc_op->get_encl_line_cnt = lcd_venc_get_encl_line_cnt;
	venc_op->get_encl_frm_cnt = lcd_venc_get_encl_frm_cnt;
	venc_op->venc_set_htotal = lcd_venc_set_htotal;
	venc_op->venc_set_vtotal = lcd_venc_set_vtotal;
	venc_op->venc_reg_dump = lcd_venc_reg_dump;
	venc_op->venc_bist_change = lcd_venc_bist_change;
	//venc_op->mute_set = lcd_venc_mute_set;

	return 0;
};
