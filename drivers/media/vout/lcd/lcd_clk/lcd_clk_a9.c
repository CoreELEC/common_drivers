// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include "../connectors/lcd_connector.h"

static void lcd_pll_ss_init(struct lcd_clk_config_s *cconf)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_pll_ss_enable(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0;
	unsigned int flag;
	int offset;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	offset = cconf->data->pll_data[0]->pll_offset;
	pll_ctrl0 = lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9);
	pll_ctrl0 &= ~((1 << 19) | (1 << 19) | (0xff << 20));

	if (status) {
		if (cconf->ss_level > 0)
			flag = 1;
		else
			flag = 0;
	} else {
		flag = 0;
	}

	if (flag) {
		cconf->ss_en = 1;
		pll_ctrl0 |= ((1 << 19) | (cconf->ss_dep_sel << 24) |
					(cconf->ss_str_m << 20));
		LCDPR("[%d]: pll ss enable: level: %d, %dppm\n",
		      pdrv->index, cconf->ss_level, cconf->ss_ppm);
	} else {
		cconf->ss_en = 0;
		LCDPR("[%d]: pll ss disable\n", pdrv->index);
	}
	lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9, pll_ctrl0);
}

static void lcd_set_pll_ss(struct aml_lcd_drv_s *pdrv, unsigned int ss_flag)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0, pll_ctrl1;
	char prt_str[64] = {0};
	int len = 0;
	int offset;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	offset = cconf->data->pll_data[0]->pll_offset;
	pll_ctrl0 = lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9);
	pll_ctrl1 = lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_CNTL1_A9);

	if (ss_flag & LCD_SSC_LEVEL) {
		pll_ctrl0 &= ~((1 << 19) | (0xff << 20));
		if (cconf->ss_level > 0) {
			cconf->ss_en = 1;
			pll_ctrl0 |= ((1 << 19) | (cconf->ss_dep_sel << 24) |
					(cconf->ss_str_m << 20));
			len += sprintf(prt_str + len, "level: %d, %dppm",
					cconf->ss_level, cconf->ss_ppm);
		} else {
			cconf->ss_en = 0;
			len += sprintf(prt_str + len, "disable");
		}
	}

	if (ss_flag & LCD_SSC_FREQ) {
		pll_ctrl1 &= ~(0x7 << 15);
		pll_ctrl1 |= (cconf->ss_freq << 15);
		len += sprintf(prt_str + len, "%sfreq=%d", len ? ", " : "", cconf->ss_freq);
	}

	if (ss_flag & LCD_SSC_MODE) {
		pll_ctrl1 &= ~(0x3 << 23); /* ss_mode */
		pll_ctrl1 |= (cconf->ss_mode << 23);
		len += sprintf(prt_str + len, "%smode=%d", len ? ", " : "", cconf->ss_mode);
	}

	lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL1_A9, pll_ctrl1);
	lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9, pll_ctrl0);

	LCDPR("[%d]: set ssc: %s\n", pdrv->index, len ? prt_str : "none");
}

static void lcd_pll_frac_set(struct aml_lcd_drv_s *pdrv, unsigned int frac)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg, val;
	int offset;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	offset = cconf->data->pll_data[0]->pll_offset;
	reg = ANACTRL_TCON_PLL0_CNTL2_A9;
	val = lcd_vx1_lvds_ctrl_read(pdrv, reg);
	lcd_vx1_lvds_ctrl_setb(pdrv, reg, frac, 0, 17);
	lcd_delay_us(10);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK_A9, 1, 4, 1);
	lcd_delay_us(10);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK_A9, 0, 4, 1);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("[%d]: reg 0x%x: 0x%08x->0x%08x\n",
		      pdrv->index, reg, val, lcd_vx1_lvds_ctrl_read(pdrv, reg));
	}
	if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL) {
		offset = cconf->data->pll_data[1]->pll_offset;
		reg = ANACTRL_TCON_PLL2_CNTL1_A9;
		val = lcd_vx1_lvds_ctrl_read(pdrv, reg);
		lcd_vx1_lvds_ctrl_setb(pdrv, reg, cconf->pll_config[1].pll_frac, 0, 17);
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
			LCDPR("[%d]: reg 0x%x: 0x%08x->0x%08x\n",
				pdrv->index, reg, val, lcd_vx1_lvds_ctrl_read(pdrv, reg));
		}
	}
}

static int lcd_vx_pll_wait_lock(struct aml_lcd_drv_s *pdrv, int id,
				unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = PLL_WAIT_LOCK_CNT; /* 200 */
	int ret = 0;

	do {
		usleep_range(40, 60);
		pll_lock = lcd_vx1_lvds_ctrl_getb(pdrv, reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (pll_lock == 0)
		ret = -1;
	LCD_PR(pdrv, "pll[%d]: lock=%d, wait_loop=%d",
	      id, pll_lock, (PLL_WAIT_LOCK_CNT - wait_loop));

	return ret;
}

static unsigned int tcon_div_a9[][4] = {
	/* vx1pll_div214h, tcon_bypass_en, vx1pll_clk1x_selh */
	{0, 0, 1, 0x80},  /* div1 */
	{0, 0, 0, 0x80},  /* div2 */
	{1, 0, 0, 0x80},  /* div4 */
	{0, 0, 0, 0x0f},  /* div8 */
	{1, 0, 0, 0x0f},  /* div16 */
};

static void lcd_set_pll_a9(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0, pll_ctrl1, pll_ctrl2, pll_ctrl3;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;
	int offset;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl0 = 0x13e00 | (cconf->pll_config[0].pll_m << 0);
	pll_ctrl2 = cconf->pll_config[0].pll_frac |
		(1 << 20) | /*pll sdm_en*/
		(tcon_div_a9[tcon_div_sel][0] << 24) | /*div2_4_sel*/
		(tcon_div_a9[tcon_div_sel][2] << 25) | /*clk1x_sel*/
		(tcon_div_a9[tcon_div_sel][1] << 31);
	pll_ctrl3 =
		(cconf->pll_config[0].pll_od1_sel << 10) |
		(cconf->pll_config[0].pll_od2_sel << 12) |
		(cconf->pll_config[0].pll_od3_sel << 14) |
		(1 << 8) | /*tcon_od_en*/
		(tcon_div_a9[tcon_div_sel][3]); /*tcon_bypass_en*/
	do {
		offset = cconf->data->pll_data[0]->pll_offset;
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9 + offset, pll_ctrl0);
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL1_A9 + offset, 0x94401545);
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL2_A9 + offset, pll_ctrl2);
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL3_A9 + offset, pll_ctrl3);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9 + offset, 1, 28, 1);
		lcd_delay_us(10);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9 + offset, 1, 30, 1);
		lcd_delay_us(80);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9 + offset, 1, 29, 1);
		offset = pdrv->index;
		ret = lcd_vx_pll_wait_lock(pdrv, cconf->pll_config[0].pll_id,
					ANACTRL_TCON_PLL0_STS_A9 + offset, 31);
	} while (ret && ++cnt < PLL_RETRY_MAX);
	if (ret)
		LCDERR("[%d]: vx1 pll lock failed\n", pdrv->index);

	/* set load to 0 */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK_A9 + offset, 0, 4, 1);
	/* select ANACTRL_TCON_PLL_VLOCK[4] as load */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK_A9 + offset, 0, 3, 1);
	/* enable load en*/
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0_A9 + offset, 1, 14, 1);

	if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL) {
		cnt = 0;
		ret = 0;
		do {
			offset = cconf->data->pll_data[1]->pll_offset;
			pll_ctrl0 = 0xe00000 |
				((cconf->pll_config[1].pll_od1_sel & 0x3) << 19) |
				(cconf->pll_config[1].pll_m & 0x1ff) |
				((cconf->pll_config[1].pll_od2_sel & 0x3) << 22);
			pll_ctrl1 = 0x11480000 | (cconf->pll_config[1].pll_frac & 0x1ffff);
			lcd_vx1_lvds_ctrl_write(pdrv,
						ANACTRL_TCON_PLL2_CNTL0_A9 + offset,
						pll_ctrl0);
			lcd_vx1_lvds_ctrl_write(pdrv,
						ANACTRL_TCON_PLL2_CNTL1_A9 + offset,
						pll_ctrl1);
			lcd_vx1_lvds_ctrl_write(pdrv,
						ANACTRL_TCON_PLL2_CNTL2_A9 + offset,
						0x12001230);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL2_CNTL0_A9 + offset, 1, 28, 1);
			lcd_delay_us(20);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL2_CNTL0_A9 + offset, 1, 29, 1);
			lcd_delay_us(20);
			lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL2_CNTL0_A9 + offset, 0, 30, 1);
			offset = pdrv->index;
			ret = lcd_vx_pll_wait_lock(pdrv, cconf->pll_config[1].pll_id,
						ANACTRL_TCON_PLL2_STS_A9 + offset, 31);
		} while (ret && ++cnt < PLL_RETRY_MAX);
		if (ret)
			LCDERR("[%d]: gp2 pll lock failed\n", pdrv->index);
	}

	if (cconf->ss_level > 0)
		lcd_set_pll_ss(pdrv, (LCD_SSC_LEVEL | LCD_SSC_FREQ | LCD_SSC_MODE));
}

static void lcd_set_vid_pll_div_a9(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;
	int offset = pdrv->index;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)
		return;

	lcd_clk_setb(CLKCTRL_HDMI_CLK_CTRL_A9, 1, 15, 1);
	udelay(5);

	/* Disable the div output clock */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 0, 19, 1);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 0, 15, 1);

	if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL) {
		if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
			LCDPR("no need to set clk divider for gp2\n\n");
		return;
	}

	if (cconf->data->pll_data[0]->div_sel_max == CLK_DIV_SEL_1 ||
	    cconf->pll_config[0].div_sel > cconf->data->pll_data[0]->div_sel_max ||
	    cconf->pll_config[0].div_sel >= ARRAY_SIZE(lcd_clk_div_table)) {
		LCDERR("[%d]: invalid clk divider\n", pdrv->index);
		return;
	}

	shift_val = lcd_clk_div_table[cconf->pll_config[0].div_sel].shift_val;
	shift_sel = lcd_clk_div_table[cconf->pll_config[0].div_sel].shift_sel;

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 1, 18, 1);
	} else {
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 0, 15, 3);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 0, 0, 14);

		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset,
					shift_sel, 16, 2);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 1, 15, 1);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset,
					(shift_val & 0x7fff), 0, 15);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + offset, 1, 19, 1);
}

static void lcd_set_vclk_crt_by_path(struct aml_lcd_drv_s *pdrv, u8 path)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int reg_vid_clk_div, reg_vid_clk_ctrl;
	unsigned int encl_clk_sel_bit, encl_clk_sel;
	unsigned int encl_gate_bit;

	if (!cconf)
		return;

	switch (path) {
	case 1:
		reg_vid_clk_div = CLKCTRL_VIID_CLK_DIV_A9;
		reg_vid_clk_ctrl = CLKCTRL_VIID_CLK_CTRL_A9;
		encl_clk_sel_bit = 8;
		encl_clk_sel = 8;
		encl_gate_bit = 11;
		break;
	case 0:
	default:
		reg_vid_clk_div = CLKCTRL_VID_CLK_DIV_A9;
		reg_vid_clk_ctrl = CLKCTRL_VID_CLK_CTRL_A9;
		encl_clk_sel_bit = 12;
		encl_clk_sel = 0;
		encl_gate_bit = 10;
		break;
	}

	lcd_clk_setb(reg_vid_clk_ctrl, 0, VCLK2_EN, 1);
	udelay(2);
	lcd_set_vid_pll_div_a9(pdrv);

	if (pdrv->lcd_pxp) {
		/* setup the XD divider value */
		lcd_clk_setb(reg_vid_clk_div, 0, VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(reg_vid_clk_ctrl, 6, VCLK2_CLK_IN_SEL, 3);
	} else {
		lcd_clk_setb(reg_vid_clk_div, (cconf->xd - 1), VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(reg_vid_clk_ctrl, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
	}
	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(CLKCTRL_VIID_CLK_DIV_A9, encl_clk_sel, encl_clk_sel_bit, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(reg_vid_clk_div, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(reg_vid_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(reg_vid_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	lcd_clk_setb(reg_vid_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(CLKCTRL_VID_CLK_CTRL2_A9, 1, encl_gate_bit, 1);

	lcd_clk_setb(reg_vid_clk_ctrl, 1, VCLK2_EN, 1);
	udelay(2);
}

static void lcd_clk_set_a9(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll_a9(pdrv);
	lcd_set_vid_pll_div_a9(pdrv);
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_set_vclk_crt_by_path(pdrv, cconf->pll_config[0].pll_id);

	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MIPI) {
		if (pdrv->curr_dev->dev_cfg.control.mipi_cfg.multi_port_cfg & BIT(0)) {
			lcd_clk_setb(CLKCTRL_VID_CLK_CTRL2_A9, 3, 16, 1);
			lcd_clk_setb(CLKCTRL_VIID_CLK_DIV_A9, 1, 8, 4); // seltect enc1 clk source
			lcd_clk_setb(CLKCTRL_VID_CLK_CTRL_A9, 1, 1, 1); // enable div2 gate
		}
	}
}

static void lcd_clk_disable(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg_vid_clk_ctrl, offset;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	switch (cconf->pll_config[0].pll_id) {
	case 1:
		reg_vid_clk_ctrl = CLKCTRL_VIID_CLK_CTRL_A9;
		break;
	case 0:
	default:
		reg_vid_clk_ctrl = CLKCTRL_VID_CLK_CTRL_A9;
		break;
	}
	offset = cconf->data->pll_data[0]->pll_offset;

	lcd_clk_setb(CLKCTRL_VID_CLK_CTRL2_A9, 0, 10 + cconf->pll_config[0].pll_id, 1);

	/* close vclk2_div gate: [4:0] */
	lcd_clk_setb(reg_vid_clk_ctrl, 0, 0, 5);
	lcd_clk_setb(reg_vid_clk_ctrl, 0, VCLK2_EN, 1);
}

static int lcd_clk_reg_dump_a9(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i = 0, n, len = 0;
	unsigned int *table = NULL, size = 0;
	struct lcd_clk_config_s *cconf;
	unsigned int offset_phy = 0, offset_pix = 0;
	unsigned int pll_reg_table[] = {
		ANACTRL_TCON_PLL0_CNTL0_A9,
		ANACTRL_TCON_PLL0_CNTL1_A9,
		ANACTRL_TCON_PLL0_CNTL2_A9,
		ANACTRL_TCON_PLL0_CNTL3_A9,
	};
	unsigned int pixel_pll_reg_table[] = {
		ANACTRL_GP2PLL_CTRL0,
		ANACTRL_GP2PLL_CTRL1,
		ANACTRL_GP2PLL_CTRL2,
	};
	unsigned int clk_reg_table[][2] = {
		{
			CLKCTRL_VID_CLK_CTRL_A9,
			CLKCTRL_VID_CLK_DIV_A9,
		},
		{
			CLKCTRL_VIID_CLK_CTRL_A9,
			CLKCTRL_VIID_CLK_DIV_A9,
		}
	};

	cconf = get_lcd_clk_config(pdrv);
	if (!pdrv || pdrv->index >= 2 || !cconf)
		return 0;
	if (pdrv->data && cconf->data->pll_data[0])
		offset_phy = cconf->data->pll_data[0]->pll_offset;
	table = pll_reg_table;
	size = ARRAY_SIZE(pll_reg_table);
	for (i = 0; i < size; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
			table[i] + offset_phy,
			lcd_vx1_lvds_ctrl_read(pdrv, table[i] + offset_phy));
	}

	len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
		ANACTRL_TCON_PLL0_STS_A9 + pdrv->index,
		lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_STS_A9 + pdrv->index));

	len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
		ANACTRL_VID_PLL0_CLK_DIV_A9 + pdrv->index,
		lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_VID_PLL0_CLK_DIV_A9 + pdrv->index));

	if (cconf && (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)) {
		if (pdrv->data && cconf->data->pll_data[1])
			offset_pix = cconf->data->pll_data[1]->pll_offset;
		table = pixel_pll_reg_table;
		size = ARRAY_SIZE(pixel_pll_reg_table);
		for (i = 0; i < size; i++) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
				table[i], lcd_vx1_lvds_ctrl_read(pdrv, table[i] + offset_pix));
		}
	}

	table = clk_reg_table[pdrv->index];
	size = ARRAY_SIZE(clk_reg_table[pdrv->index]);
	for (i = 0; i < size; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "clk [0x%02x] = 0x%08x\n",
			table[i], lcd_clk_read(table[i]));
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "clk [0x%02x] = 0x%08x\n",
			CLKCTRL_VID_CLK_CTRL2_A9, lcd_clk_read(CLKCTRL_VID_CLK_CTRL2_A9));

	return len;
}

static void lcd_clk_prbs_test_a9(struct aml_lcd_drv_s *pdrv,
				 unsigned int ms, unsigned int mode_flag)
{
	LCDPR("TODO\n");
}

static void lcd_clk_generate_a9(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MIPI) {
		LCDPR("[%d]: %s: dual pll config\n", pdrv->index, __func__);
		if (cconf->pll_conf_num < 2) {
			LCDERR("[%d]: %s: clk_conf_num %d error\n",
				pdrv->index, __func__, cconf->pll_conf_num);
			return;
		}
		cconf->pll_mode |= LCD_PLL_MODE_DUAL_PLL;
		cconf->data->vclk_sel = pdrv->index ? 3 : 1;
	} else {
		cconf->pll_mode &= ~LCD_PLL_MODE_DUAL_PLL;
		cconf->data->vclk_sel = pdrv->index ? 7 : 2;
	}

	lcd_clk_generate_od_div_5_7(pdrv);
}

static struct lcd_pll_data_s lcd_tcon_pll_data = {
	.pll_od_fb = 0,
	.pll_div_0p5_en = 1,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_od_sel_max = 4,
	.pll_ref_fmax = 25000000,
	.pll_ref_fmin = 5000000,
	.pll_vco_fmax = 6000000000ULL,
	.pll_vco_fmin = 3000000000ULL,
	.pll_out_fmax = 4250000000ULL,
	.pll_out_fmin = 5859375,
	.od_cnt = 3,
	.have_tcon_div = 1,
	.pll_od_div_5_7 = 1,
	.div_in_fmax = 4250000000ULL,
	.div_out_fmax = 840000000,
	.div_sel_max = CLK_DIV_SEL_15,
};

static struct lcd_pll_data_s lcd_pix_pll_data = {
	.pll_od_fb = 0,
	.pll_div_0p5_en = 0,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_od_sel_max = 3,
	.pll_ref_fmax = 25000000,
	.pll_ref_fmin = 5000000,
	.pll_vco_fmax = 3200000000ULL,
	.pll_vco_fmin = 1600000000ULL,
	.pll_out_fmax = 3200000000ULL,
	.pll_out_fmin = 200000000,
	.od_cnt = 1,
	.have_tcon_div = 0,
	.div_in_fmax = 3200000000ULL,
	.div_out_fmax = 840000000,
	.div_sel_max = CLK_DIV_SEL_1,
};

static struct lcd_clk_data_s lcd_clk_data_a9_0 = {
	.pll_data[0] = &lcd_tcon_pll_data,
	.pll_data[1] = &lcd_pix_pll_data,
	.xd_out_fmax = 750000000,
	.phy_clk_location = 0,

	.vclk_sel = 2,
	.enc_clk_msr_id = 53,
	.fifo_clk_msr_id = 86,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,

	.xd_max = 128,
	.phy_div_max = 128,

	.ss_support = 1,
	.ss_level_max = 60,
	.ss_freq_max = 6,
	.ss_mode_max = 2,
	.ss_dep_base = 500, //ppm
	.ss_dep_sel_max = 12,
	.ss_str_m_max = 10,

	.clktree_set = NULL,
	.clktree_index = {CLKTREE_DSI_A_HOST_GATE, CLKTREE_DSI_A_PHY_GATE, CLKTREE_DSI_A_MEAS,
			  0, 0, 0},
	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_a9,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss = lcd_set_pll_ss,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = lcd_pll_ss_init,
	.pll_frac_set = lcd_pll_frac_set,
	.pll_m_set = NULL,
	.pll_reset = NULL,
	.clk_set = lcd_clk_set_a9,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable,
	.mlvds_clk_phase_set = NULL,
	.clk_set_dummy = NULL,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.clk_reg_print = lcd_clk_reg_dump_a9,
	.prbs_test = lcd_clk_prbs_test_a9,
};

static struct lcd_clk_data_s lcd_clk_data_a9_1 = {
	.pll_data[0] = &lcd_tcon_pll_data,
	.pll_data[1] = &lcd_pix_pll_data,
	.xd_out_fmax = 750000000,
	.phy_clk_location = 0,

	.vclk_sel = 0,
	.enc_clk_msr_id = 52,
	.fifo_clk_msr_id = 87,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,

	.xd_max = 128,
	.phy_div_max = 128,

	.ss_support = 1,
	.ss_level_max = 60,
	.ss_freq_max = 6,
	.ss_mode_max = 2,
	.ss_dep_base = 500, //ppm
	.ss_dep_sel_max = 12,
	.ss_str_m_max = 10,

	.clktree_set = NULL,
	.clktree_index = {CLKTREE_DSI_A_HOST_GATE, CLKTREE_DSI_A_PHY_GATE, CLKTREE_DSI_A_MEAS,
			  0, 0, 0},
	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_a9,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss = lcd_set_pll_ss,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = lcd_pll_ss_init,
	.pll_frac_set = lcd_pll_frac_set,
	.pll_m_set = NULL,
	.pll_reset = NULL,
	.clk_set = lcd_clk_set_a9,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_disable = lcd_clk_disable,
	.mlvds_clk_phase_set = NULL,
	.clk_set_dummy = NULL,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.clk_reg_print = lcd_clk_reg_dump_a9,
	.prbs_test = lcd_clk_prbs_test_a9,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_a9(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv)
		return NULL;

	if (!pdrv->clk_conf) {
		cconf = kcalloc(1, sizeof(struct lcd_clk_config_s), GFP_KERNEL);
		if (!cconf) {
			LCDERR("[%d]: %s: Not enough memory\n", pdrv->index, __func__);
			return NULL;
		}
	} else {
		cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
		memset(cconf, 0, sizeof(struct lcd_clk_config_s));
	}

	cconf->pll_conf_num = 2;
	cconf->pll_config = kcalloc(cconf->pll_conf_num, sizeof(struct lcd_pll_config_s),
					GFP_KERNEL);
	if (!cconf->pll_config) {
		LCDERR("[%d]: %s: Not enough memory for pll config\n", pdrv->index, __func__);
		kfree(cconf);
		return NULL;
	}

	if (pdrv->index == 0) {
		cconf->data = &lcd_clk_data_a9_0;
		cconf->pll_config[0].pll_id = 0;
		cconf->data->pll_data[0]->pll_offset = 0;
		cconf->data->pll_data[1]->pll_offset = 0;
	} else {
		cconf->data = &lcd_clk_data_a9_1;
		cconf->pll_config[0].pll_id = 1;
		cconf->data->pll_data[0]->pll_offset =
			ANACTRL_TCON_PLL1_CNTL0_A9 - ANACTRL_TCON_PLL0_CNTL0_A9;
		cconf->data->pll_data[1]->pll_offset =
			ANACTRL_TCON_PLL3_CNTL0_A9 - ANACTRL_TCON_PLL2_CNTL0_A9;
	}
	return cconf;
}

