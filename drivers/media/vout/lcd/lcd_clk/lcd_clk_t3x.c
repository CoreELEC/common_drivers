// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */
#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_clk_config.h"
#include "lcd_clk_ctrl.h"
#include "lcd_clk_utils.h"

static struct lcd_clk_ctrl_s pll_ctrl_table_t3x[] = {
	/* flag             reg                      bit              len*/
	{LCD_CLK_CTRL_EN,   ANACTRL_TCON_PLL0_CNTL0, LCD_PLL_EN_TL1,   1},
	{LCD_CLK_CTRL_RST,  ANACTRL_TCON_PLL0_CNTL0, LCD_PLL_RST_TL1,  1},
	{LCD_CLK_CTRL_M,    ANACTRL_TCON_PLL0_CNTL0, LCD_PLL_M_TL1,    8},
	{LCD_CLK_CTRL_FRAC, ANACTRL_TCON_PLL0_CNTL1,               0, 17},
	{LCD_CLK_CTRL_END,  LCD_CLK_REG_END,                       0,  0},
};

static void lcd_set_pll_ss_level(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	unsigned int level, dep_sel, str_m;
	unsigned int data[2] = {0, 0};
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	level = cconf->ss_level;
	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	pll_ctrl2 &= ~((1 << 15) | (0xf << 16) | (0xf << 28));

	if (level > 0) {
		cconf->ss_en = 1;
		ret = lcd_pll_ss_level_generate(data, level, 500);
		if (ret == 0) {
			dep_sel = data[0];
			str_m = data[1];
			dep_sel = (dep_sel > 10) ? 10 : dep_sel;
			str_m = (str_m > 10) ? 10 : str_m;
			pll_ctrl2 |= ((1 << 15) | (dep_sel << 28) |
				     (str_m << 16));
		}
	} else {
		cconf->ss_en = 0;
	}

	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);

	if (level > 0) {
		LCDPR("[%d]: set pll spread spectrum: %dppm\n",
		      pdrv->index, (level * 1000));
	} else {
		LCDPR("[%d]: set pll spread spectrum: disable\n", pdrv->index);
	}
}

static void lcd_set_pll_ss_advance(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	unsigned int freq, mode;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	freq = cconf->ss_freq;
	mode = cconf->ss_mode;
	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	pll_ctrl2 &= ~(0x7 << 24); /* ss_freq */
	pll_ctrl2 |= (freq << 24);
	pll_ctrl2 &= ~(0x3 << 22); /* ss_mode */
	pll_ctrl2 |= (mode << 22);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);

	LCDPR("[%d]: set pll spread spectrum: freq=%d, mode=%d\n",
	      pdrv->index, freq, mode);
}

static void lcd_pll_ss_enable(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, offset;
	unsigned int level, flag;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	level = cconf->ss_level;
	offset = cconf->pll_offset;
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2 + offset);
	if (status) {
		if (level > 0)
			flag = 1;
		else
			flag = 0;
	} else {
		flag = 0;
	}
	if (flag) {
		cconf->ss_en = 1;
		pll_ctrl2 |= (1 << 15);
		LCDPR("[%d]: pll ss enable: %dppm\n", pdrv->index, (level * 1000));
	} else {
		cconf->ss_en = 0;
		pll_ctrl2 &= ~(1 << 15);
		LCDPR("[%d]: pll ss disable\n", pdrv->index);
	}
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2 + offset, pll_ctrl2);
}

static int _lcd_set_pll_by_cconf(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	unsigned int pll_ctrl, pll_ctrl1, pll_stts;
	unsigned int reg_ctrl0, reg_ctrl1, reg_ctrl2, reg_ctrl3, reg_ctrl4;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;
	int done = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return 0;

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl = ((0x3 << 17) | /* gate ctrl */
		(tcon_div[tcon_div_sel][2] << 16) |
		(cconf->pll_n << LCD_PLL_N_TL1) |
		(cconf->pll_m << LCD_PLL_M_TL1) |
		(cconf->pll_od3_sel << LCD_PLL_OD3_T7) |
		(cconf->pll_od2_sel << LCD_PLL_OD2_T7) |
		(cconf->pll_od1_sel << LCD_PLL_OD1_T7));
	pll_ctrl1 = (1 << 28) |
		(tcon_div[tcon_div_sel][0] << 22) |
		(tcon_div[tcon_div_sel][1] << 21) |
		((1 << 20) | /* sdm_en */
		(cconf->pll_frac << 0));

	switch (cconf->pll_id) {
	case 1:
		pll_stts = ANACTRL_TCON_PLL1_STS;
		reg_ctrl0 = ANACTRL_TCON_PLL1_CNTL0;
		reg_ctrl1 = ANACTRL_TCON_PLL1_CNTL1;
		reg_ctrl2 = ANACTRL_TCON_PLL1_CNTL2;
		reg_ctrl3 = ANACTRL_TCON_PLL1_CNTL3;
		reg_ctrl4 = ANACTRL_TCON_PLL1_CNTL4;
		break;
	case 2:
		pll_stts = ANACTRL_TCON_PLL2_STS;
		reg_ctrl0 = ANACTRL_TCON_PLL2_CNTL0;
		reg_ctrl1 = ANACTRL_TCON_PLL2_CNTL1;
		reg_ctrl2 = ANACTRL_TCON_PLL2_CNTL2;
		reg_ctrl3 = ANACTRL_TCON_PLL2_CNTL3;
		reg_ctrl4 = ANACTRL_TCON_PLL2_CNTL4;
		break;
	case 0:
	default:
		pll_stts = ANACTRL_TCON_PLL0_STS;
		reg_ctrl0 = ANACTRL_TCON_PLL0_CNTL0;
		reg_ctrl1 = ANACTRL_TCON_PLL0_CNTL1;
		reg_ctrl2 = ANACTRL_TCON_PLL0_CNTL2;
		reg_ctrl3 = ANACTRL_TCON_PLL0_CNTL3;
		reg_ctrl4 = ANACTRL_TCON_PLL0_CNTL4;
		break;
	}

set_pll_retry_t7:
	lcd_ana_write(reg_ctrl0, pll_ctrl);
	udelay(10);
	lcd_ana_setb(reg_ctrl0, 1, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_setb(reg_ctrl0, 1, LCD_PLL_EN_TL1, 1);
	udelay(10);
	lcd_ana_write(reg_ctrl1, pll_ctrl1);
	udelay(10);
	lcd_ana_write(reg_ctrl2, 0x0000110c);
	udelay(10);
	if (cconf->pll_fvco < 3800000)
		lcd_ana_write(reg_ctrl3, 0x10051100);
	else
		lcd_ana_write(reg_ctrl3, 0x10051400);
	udelay(10);
	lcd_ana_setb(reg_ctrl4, 0x0100c0, 0, 24);
	udelay(10);
	lcd_ana_setb(reg_ctrl4, 0x8300c0, 0, 24);
	udelay(10);
	lcd_ana_setb(reg_ctrl0, 1, 26, 1);
	udelay(10);
	lcd_ana_setb(reg_ctrl0, 0, LCD_PLL_RST_TL1, 1);
	udelay(10);
	lcd_ana_write(reg_ctrl2, 0x0000300c);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_t7;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	} else {
		udelay(100);
		lcd_ana_setb(reg_ctrl2, 1, 5, 1);
		done = 1;
	}

	return done;
}

static void lcd_set_pll_t3x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	_lcd_set_pll_by_cconf(pdrv, &cconf[0]);
	if (pdrv->config.timing.clk_mode == LCD_CLK_MODE_INDEPENDENCE)
		_lcd_set_pll_by_cconf(pdrv, &cconf[1]);

	if (cconf[0].ss_level > 0) {
		lcd_set_pll_ss_level(pdrv);
		lcd_set_pll_ss_advance(pdrv);
	}
}

static void _lcd_set_vid_pll_div_by_cconf(struct aml_lcd_drv_s *pdrv,
		struct lcd_clk_config_s *cconf)
{
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	unsigned int shift_val, shift_sel;
	unsigned int clk_mode;
	int i;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	clk_mode = pdrv->config.timing.clk_mode;
	switch (cconf->pll_id) {
	case 1:
		reg_vid_pll_div = COMBO_DPHY_VID_PLL1_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		if (clk_mode == LCD_CLK_MODE_INDEPENDENCE)
			reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		break;
	case 2:
		reg_vid_pll_div = COMBO_DPHY_VID_PLL2_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		break;
	case 0:
	default:
		reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		break;
	}

	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	udelay(5);

	/* Disable the div output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 19, 1);
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);

	i = 0;
	while (lcd_clk_div_table[i][0] != CLK_DIV_SEL_MAX) {
		if (cconf->div_sel == lcd_clk_div_table[i][0])
			break;
		i++;
	}
	if (lcd_clk_div_table[i][0] == CLK_DIV_SEL_MAX)
		LCDERR("[%d]: invalid clk divider\n", pdrv->index);
	shift_val = lcd_clk_div_table[i][1];
	shift_sel = lcd_clk_div_table[i][2];

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 18, 1);
	} else {
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 18, 1);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 16, 2);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 0, 14);

		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, shift_sel, 16, 2);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 15, 1);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, shift_val, 0, 15);
		lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 0, 15, 1);
	}

	/* select vid_pll1_clk */
	if (clk_mode == LCD_CLK_MODE_INDEPENDENCE && cconf->pll_id == 1)
		lcd_clk_setb(reg_vid2_clk_ctrl, 4, 16, 3);

	/* Enable the final output clock */
	lcd_combo_dphy_setb(pdrv, reg_vid_pll_div, 1, 19, 1);
}

static void lcd_set_vid_pll_div_t3x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (pdrv->config.timing.clk_mode == LCD_CLK_MODE_INDEPENDENCE) {
		_lcd_set_vid_pll_div_by_cconf(pdrv, &cconf[0]);
		_lcd_set_vid_pll_div_by_cconf(pdrv, &cconf[1]);
	} else {
		_lcd_set_vid_pll_div_by_cconf(pdrv, cconf);
	}
}

static void lcd_clk_set_t3x(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll_t3x(pdrv);
	lcd_set_vid_pll_div_t3x(pdrv);
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg_vid2_clk_div, reg_vid2_clk_ctrl, reg_vid_clk_ctrl2;
	unsigned int venc_clk_sel_bit = 0xff;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (cconf->pll_id) {
	case 1:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK1_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK1_CTRL2;
		if (pdrv->config.timing.clk_mode == LCD_CLK_MODE_INDEPENDENCE) {
			reg_vid2_clk_div = CLKCTRL_VIID_CLK0_DIV;
			reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
			reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;
		}
		break;
	case 2:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK2_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK2_CTRL2;
		venc_clk_sel_bit = 25;
		break;
	case 0:
	default:
		reg_vid2_clk_div = CLKCTRL_VIID_CLK0_DIV;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;
		venc_clk_sel_bit = 24;
		break;
	}

	lcd_clk_write(reg_vid_clk_ctrl2, 0);
	lcd_clk_write(reg_vid2_clk_ctrl, 0);
	lcd_clk_write(reg_vid2_clk_div, 0);
	udelay(5);

	if (pdrv->lcd_pxp) {
		/* setup the XD divider value */
		lcd_clk_setb(reg_vid2_clk_div, cconf->xd, VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(reg_vid2_clk_ctrl, 7, VCLK2_CLK_IN_SEL, 3);
	} else {
		if (venc_clk_sel_bit < 0xff)
			lcd_clk_setb(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 0, venc_clk_sel_bit, 1);

		/* setup the XD divider value */
		lcd_clk_setb(reg_vid2_clk_div, (cconf->xd - 1), VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(reg_vid2_clk_ctrl, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
	}
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(reg_vid2_clk_div, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(reg_vid2_clk_div, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	udelay(10);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(reg_vid_clk_ctrl2, 1, ENCL_GATE_VCLK, 1);
}

static void lcd_clk_disable(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_ctrl_s *table;
	unsigned int reg_vid_clk_ctrl2, reg_vid2_clk_ctrl, offset;
	int i = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (cconf->pll_id) {
	case 1:
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK1_CTRL2;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK1_CTRL;
		break;
	case 2:
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK2_CTRL2;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK2_CTRL;
		break;
	case 0:
	default:
		reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;
		reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
		break;
	}
	offset = cconf->pll_offset;

	lcd_clk_setb(reg_vid_clk_ctrl2, 0, ENCL_GATE_VCLK, 1);

	/* close vclk2_div gate: [4:0] */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, 0, 5);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);

	if (!cconf->data->pll_ctrl_table)
		return;
	table = cconf->data->pll_ctrl_table;
	while (i < LCD_CLK_CTRL_CNT_MAX) {
		if (table[i].flag == LCD_CLK_CTRL_END)
			break;
		if (table[i].flag == LCD_CLK_CTRL_EN)
			lcd_ana_setb(table[i].reg + offset, 0, table[i].bit, table[i].len);
		else if (table[i].flag == LCD_CLK_CTRL_RST)
			lcd_ana_setb(table[i].reg + offset, 1, table[i].bit, table[i].len);
		i++;
	}
}

static void lcd_clk_gate_switch_t3x(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (status) {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_MIPI:
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_host_gate))
				LCDERR("%s: dsi_host_gate\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.dsi_host_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_phy_gate))
				LCDERR("%s: dsi_phy_gate\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.dsi_phy_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_meas))
				LCDERR("%s: dsi_meas\n", __func__);
			else
				clk_prepare_enable(cconf->clktree.dsi_meas);
			break;
		default:
			break;
		}
	} else {
		switch (pdrv->config.basic.lcd_type) {
		case LCD_MIPI:
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_host_gate))
				LCDERR("%s: dsi_host_gate\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.dsi_host_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_phy_gate))
				LCDERR("%s: dsi_phy_gate\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.dsi_phy_gate);
			if (IS_ERR_OR_NULL(cconf->clktree.dsi_meas))
				LCDERR("%s: dsi_meas\n", __func__);
			else
				clk_disable_unprepare(cconf->clktree.dsi_meas);
			break;
		default:
			break;
		}
	}
}

static void lcd_set_tcon_clk_t3x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->config;
	unsigned int freq, val;

	if (pdrv->index > 0) /* tcon_clk only valid for lcd0 */
		return;

	if (pdrv->config.basic.lcd_type != LCD_MLVDS &&
	    pdrv->config.basic.lcd_type != LCD_P2P)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd clk: set_tcon_clk_t3\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (pconf->basic.lcd_type) {
	case LCD_MLVDS:
		val = pconf->control.mlvds_cfg.clk_phase & 0xfff;
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL1, (val & 0xf), 24, 4);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4, ((val >> 4) & 0xf), 28, 4);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL4, ((val >> 8) & 0xf), 24, 4);

		/* tcon_clk */
		if (pconf->timing.lcd_clk >= 100000000) /* 25M */
			freq = 25000000;
		else /* 12.5M */
			freq = 12500000;
		if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
			clk_set_rate(cconf->clktree.tcon_clk, freq);
			clk_prepare_enable(cconf->clktree.tcon_clk);
		}
		break;
	case LCD_P2P:
		if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
			clk_set_rate(cconf->clktree.tcon_clk, 50000000);
			clk_prepare_enable(cconf->clktree.tcon_clk);
		}
		break;
	default:
		break;
	}

	lcd_tcon_global_reset(pdrv);
}

static void lcd_clktree_probe_t3x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct clk *temp_clk;
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	if (pdrv->index > 0) /* tcon_clk invalid for lcd1 */
		return;

	cconf->clktree.clk_gate_state = 0;

	cconf->clktree.tcon_gate = devm_clk_get(pdrv->dev, "tcon_gate");
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
		LCDERR("%s: get tcon_gate error\n", __func__);

	temp_clk = devm_clk_get(pdrv->dev, "fclk_div5");
	if (IS_ERR_OR_NULL(temp_clk)) {
		LCDERR("%s: clk fclk_div5\n", __func__);
		return;
	}
	cconf->clktree.tcon_clk = devm_clk_get(pdrv->dev, "clk_tcon");
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
		LCDERR("%s: clk clk_tcon\n", __func__);
	} else {
		ret = clk_set_parent(cconf->clktree.tcon_clk, temp_clk);
		if (ret)
			LCDERR("%s: clk clk_tcon set_parent error\n", __func__);
	}

	LCDPR("lcd_clktree_probe\n");
}

static void lcd_clktree_remove_t3x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd_clktree_remove\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	if (pdrv->index > 0) /* tcon_clk invalid for lcd1 */
		return;

	if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk))
		devm_clk_put(pdrv->dev, cconf->clktree.tcon_clk);
	if (IS_ERR_OR_NULL(cconf->clktree.tcon_gate))
		devm_clk_put(pdrv->dev, cconf->clktree.tcon_gate);
}

static void lcd_prbs_set_pll_vx1_t3x(struct aml_lcd_drv_s *pdrv)
{
	unsigned int pll_stts;
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	int cnt = 0, ret;

	pll_stts = ANACTRL_TCON_PLL0_STS;
	reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
	reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;

lcd_prbs_retry_pll_vx1_t3:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x000f04f7);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, LCD_PLL_EN_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x10110000);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00001108);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, 26, 1);
	usleep_range(10, 12);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003008);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003028);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_vx1_t3;
		LCDERR("pll lock failed\n");
	}

	/* pll_div */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_ana_setb(reg_vid_pll_div, 0, 19, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	lcd_ana_setb(reg_vid_pll_div, 0, 18, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 0, 14);

	lcd_ana_setb(reg_vid_pll_div, 2, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 1, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0x739c, 0, 15);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	/* Enable the final output clock */
	lcd_ana_setb(reg_vid_pll_div, 1, 19, 1);
}

static void lcd_prbs_set_pll_lvds_t3x(struct aml_lcd_drv_s *pdrv)
{
	unsigned int pll_stts;
	unsigned int reg_vid_pll_div, reg_vid2_clk_ctrl;
	int cnt = 0, ret;

	pll_stts = ANACTRL_TCON_PLL0_STS;
	reg_vid_pll_div = COMBO_DPHY_VID_PLL0_DIV;
	reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;

lcd_prbs_retry_pll_lvds_t3:
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x008e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x208e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x3006049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x10000000);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00001102);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, 0x10051400);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x010100c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, 0x038300c0);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x348e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, 0x148e049f);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003002);
	usleep_range(10, 12);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, 0x00003022);
	usleep_range(10, 12);

	ret = lcd_pll_wait_lock(pll_stts, LCD_PLL_LOCK_T7);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto lcd_prbs_retry_pll_lvds_t3;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	}

	/* pll_div */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_ana_setb(reg_vid_pll_div, 0, 19, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	lcd_ana_setb(reg_vid_pll_div, 0, 18, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0, 0, 14);

	lcd_ana_setb(reg_vid_pll_div, 1, 16, 2);
	lcd_ana_setb(reg_vid_pll_div, 1, 15, 1);
	lcd_ana_setb(reg_vid_pll_div, 0x3c78, 0, 15);
	lcd_ana_setb(reg_vid_pll_div, 0, 15, 1);

	/* Enable the final output clock */
	lcd_ana_setb(reg_vid_pll_div, 1, 19, 1);
}

static void lcd_prbs_config_clk_t3x(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode)
{
	unsigned int reg_vid2_clk_div, reg_vid2_clk_ctrl, reg_vid_clk_ctrl2;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	reg_vid2_clk_div = CLKCTRL_VIID_CLK0_DIV;
	reg_vid2_clk_ctrl = CLKCTRL_VIID_CLK0_CTRL;
	reg_vid_clk_ctrl2 = CLKCTRL_VID_CLK0_CTRL2;

	if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
		lcd_prbs_set_pll_vx1_t3x(pdrv);
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		lcd_prbs_set_pll_lvds_t3x(pdrv);
	} else {
		LCDERR("[%d]: %s: unsupport lcd_prbs_mode %d\n",
		       pdrv->index, __func__, lcd_prbs_mode);
		return;
	}

	lcd_clk_setb(reg_vid2_clk_div, 0, VCLK2_XD, 8);
	usleep_range(5, 10);

	/* select vid_pll_clk */
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_CLK_IN_SEL, 3);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(reg_vid2_clk_div, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(reg_vid2_clk_div, 1, VCLK2_XD_EN, 2);
	usleep_range(5, 10);

	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(reg_vid2_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	usleep_range(10, 12);
	lcd_clk_setb(reg_vid2_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	usleep_range(5, 10);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(reg_vid_clk_ctrl2, 1, ENCL_GATE_VCLK, 1);

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("[%d]: %s ok\n", pdrv->index, __func__);
}

static void lcd_clk_prbs_test_t3x(struct aml_lcd_drv_s *pdrv,
				unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int reg_phy_tx_ctrl0, reg_phy_tx_ctrl1;
	int encl_msr_id, fifo_msr_id;
	unsigned int lcd_prbs_mode, lcd_prbs_cnt;
	unsigned int val1, val2, timeout;
	unsigned int clk_err_cnt = 0;
	int i, j, ret;

	if (!cconf)
		return;

	switch (pdrv->index) {
	case 0:
		reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
		reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
		break;
	case 1:
		reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
		reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL3;
		break;
	default:
		LCDERR("[%d]: %s: invalid drv_index\n", pdrv->index, __func__);
		return;
	}
	encl_msr_id = cconf->data->enc_clk_msr_id;
	fifo_msr_id = cconf->data->fifo_clk_msr_id;

	timeout = (ms > 1000) ? 1000 : ms;

	for (i = 0; i < LCD_PRBS_MODE_MAX; i++) {
		if ((mode_flag & (1 << i)) == 0)
			continue;

		lcd_ana_write(reg_phy_tx_ctrl0, 0);
		lcd_ana_write(reg_phy_tx_ctrl1, 0);

		lcd_prbs_cnt = 0;
		clk_err_cnt = 0;
		lcd_prbs_mode = (1 << i);
		LCDPR("[%d]: lcd_prbs_mode: 0x%x\n", pdrv->index, lcd_prbs_mode);
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_encl_clk_check_std = 136000000;
			lcd_fifo_clk_check_std = 48000000;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_encl_clk_check_std = 594000000;
			lcd_fifo_clk_check_std = 297000000;
		}

		lcd_prbs_config_clk_t3x(pdrv, lcd_prbs_mode);
		usleep_range(500, 510);

		/* set fifo_clk_sel: div 10 */
		lcd_ana_write(reg_phy_tx_ctrl0, (3 << 6));
		/* set cntl_ser_en:  12-channel */
		lcd_ana_setb(reg_phy_tx_ctrl0, 0xfff, 16, 12);
		lcd_ana_setb(reg_phy_tx_ctrl0, 1, 2, 1);
		/* decoupling fifo enable, gated clock enable */
		lcd_ana_write(reg_phy_tx_ctrl1, (1 << 30) | (1 << 24));
		/* decoupling fifo write enable after fifo enable */
		lcd_ana_setb(reg_phy_tx_ctrl1, 1, 31, 1);
		/* prbs_err en */
		lcd_ana_setb(reg_phy_tx_ctrl0, 1, 13, 1);
		lcd_ana_setb(reg_phy_tx_ctrl0, 1, 12, 1);

		while (lcd_prbs_flag) {
			if (lcd_prbs_cnt++ >= timeout)
				break;
			ret = 1;
			val1 = lcd_ana_getb(reg_phy_tx_ctrl1, 12, 12);
			usleep_range(1000, 1001);

			for (j = 0; j < 20; j++) {
				usleep_range(5, 10);
				val2 = lcd_ana_getb(reg_phy_tx_ctrl1, 12, 12);
				if (val2 != val1) {
					ret = 0;
					break;
				}
			}
			if (ret) {
				LCDERR("[%d]: prbs check error 1, val:0x%03x, cnt:%d\n",
				       pdrv->index, val2, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t3;
			}
			if (lcd_ana_getb(reg_phy_tx_ctrl1, 0, 12)) {
				LCDERR("[%d]: prbs check error 2, cnt:%d\n",
				       pdrv->index, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t3;
			}

			if (lcd_prbs_clk_check(lcd_encl_clk_check_std, encl_msr_id,
					       lcd_fifo_clk_check_std, fifo_msr_id,
					       lcd_prbs_cnt))
				clk_err_cnt++;
			else
				clk_err_cnt = 0;
			if (clk_err_cnt >= 10) {
				LCDERR("[%d]: prbs check error 3(clkmsr), cnt: %d\n",
				       pdrv->index, lcd_prbs_cnt);
				goto lcd_prbs_test_err_t3;
			}
		}

		lcd_ana_write(reg_phy_tx_ctrl0, 0);
		lcd_ana_write(reg_phy_tx_ctrl1, 0);

		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_LVDS);
			LCDPR("[%d]: lvds prbs check ok\n", pdrv->index);
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_VX1);
			LCDPR("[%d]: vx1 prbs check ok\n", pdrv->index);
		} else {
			LCDPR("[%d]: prbs check: unsupport mode\n", pdrv->index);
		}
		continue;

lcd_prbs_test_err_t3:
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err |= LCD_PRBS_MODE_LVDS;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err |= LCD_PRBS_MODE_VX1;
		}
	}

	lcd_prbs_flag = 0;
}

static struct lcd_clk_data_s lcd_clk_data_t3x = {
	.pll_od_fb = PLL_OD_FB_TL1,
	.pll_m_max = PLL_M_MAX,
	.pll_m_min = PLL_M_MIN,
	.pll_n_max = PLL_N_MAX,
	.pll_n_min = PLL_N_MIN,
	.pll_frac_range = PLL_FRAC_RANGE_TL1,
	.pll_frac_sign_bit = PLL_FRAC_SIGN_BIT_TL1,
	.pll_od_sel_max = PLL_OD_SEL_MAX_TL1,
	.pll_ref_fmax = PLL_FREF_MAX,
	.pll_ref_fmin = PLL_FREF_MIN,
	.pll_vco_fmax = PLL_VCO_MAX_TM2,
	.pll_vco_fmin = PLL_VCO_MIN_TM2,
	.pll_out_fmax = 3700 * 1000,
	.pll_out_fmin = PLL_VCO_MIN_TL1 / 16,
	.div_in_fmax = 3700 * 1000,
	.div_out_fmax = 720 * 1000,
	.xd_out_fmax = ENCL_CLK_IN_MAX_TL1,

	.vclk_sel = 0,
	.enc_clk_msr_id = 222,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = 119,
	.pll_ctrl_table = pll_ctrl_table_t3x,

	.ss_support = 2,

	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss_level = lcd_set_pll_ss_level,
	.set_ss_advance = lcd_set_pll_ss_advance,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_set = lcd_clk_set_t3x,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable,
	.clk_gate_switch = lcd_clk_gate_switch_t3x,
	.clk_gate_optional_switch = lcd_clk_gate_optional_switch_dft,
	.clktree_set = lcd_set_tcon_clk_t3x,
	.clktree_probe = lcd_clktree_probe_t3x,
	.clktree_remove = lcd_clktree_remove_t3x,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.prbs_test = lcd_clk_prbs_test_t3x,
};

void lcd_clk_config_chip_init_t3x(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	unsigned int i = 0;
	unsigned int clk_mode = pdrv->config.timing.clk_mode;
	unsigned int loop_num = 1;

	if (clk_mode == LCD_CLK_MODE_INDEPENDENCE)
		loop_num = 2;
	for (i = 0; i < loop_num; i++) {
		cconf[i].data = &lcd_clk_data_t3x;
		cconf[i].pll_od_fb = lcd_clk_data_t3x.pll_od_fb;
	}

	cconf->data = &lcd_clk_data_t3x;
	cconf->pll_od_fb = lcd_clk_data_t3x.pll_od_fb;
	cconf->clk_path_change = NULL;
	switch (pdrv->index) {
	case 1:
		cconf[0].data->enc_clk_msr_id = 60;
		cconf[0].pll_id = 1;
		cconf[0].pll_offset = (0x5 << 2);
		break;
	case 0:
	default:
		cconf[0].data->enc_clk_msr_id = 62;
		cconf[0].pll_id = 0;
		if (pdrv->config.basic.lcd_type == LCD_P2P &&
				clk_mode == LCD_CLK_MODE_INDEPENDENCE) {
			// pll0 as phy clock
			cconf[0].data->enc_clk_msr_id = 60;
			cconf[0].pll_id = 0;

			// pll1 as pixel clock
			cconf[1].data->enc_clk_msr_id = 62;
			cconf[1].pll_id = 1;
			cconf[1].pll_offset = (0x5 << 2);
			cconf[1].data->vclk_sel = 0x4;
		}
		break;
	}
	cconf[0].data->enc_clk_msr_id = -1;
	if (clk_mode == LCD_CLK_MODE_INDEPENDENCE)
		cconf[1].data->enc_clk_msr_id = -1;
}

