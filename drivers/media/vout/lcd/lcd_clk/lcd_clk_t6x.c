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

#define LCD_CLK_DIV_OUT_JUDGELINE 765000000

static const unsigned int gp2_od_table[6] = {1, 2, 4, 8};

static unsigned int tcon_div_t6x[][4] = {
	/* vx1pll_div214h, tcon_bypass_en, vx1pll_clk1x_selh */
	{0, 1, 1, 0x80},  /* div1 */
	{0, 1, 0, 0x80},  /* div2 */
	{1, 1, 0, 0x80},  /* div4 */
	{0, 0, 0, 0x0f},  /* div8 */
	{1, 0, 0, 0x0f},  /* div16 */
};

static unsigned char lcd_ss_freq_dep_opt_t6x[] = {
/*             freq, cnt, dep values       */
/* 0-29.5k */	0,    12,  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
/* 1-31.5k */	1,    6,   1, 3, 6, 8, 9, 11,
/* 2-50.0k */	2,    7,   1, 2, 5, 6, 7, 11, 12,
/* 3-75k */	3,    7,   1, 4, 5, 7, 8, 11, 12,
/* 4-100k  */	4,    12,  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
/* 5-150k  */   5,    12,  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
/* 6-200k  */	6,    12,  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
/* 7-0 end */   7,    0,
};

static void lcd_pll_frac_set_t6x(struct aml_lcd_drv_s *pdrv, unsigned int frac)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg, val;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	reg = ANACTRL_TCON_PLL0_CNTL4;
	val = lcd_vx1_lvds_ctrl_read(pdrv, reg);
	lcd_vx1_lvds_ctrl_setb(pdrv, reg, frac, 0, 17);
	LCD_DBG(pdrv, "tcon pll frac reg 0x%x: 0x%08x->0x%08x",
			reg, val, lcd_vx1_lvds_ctrl_read(pdrv, reg));

	reg = ANACTRL_TCON_PLL0_CNTL0;
	val = lcd_vx1_lvds_ctrl_read(pdrv, reg);
	if ((val & 0x1ff) != cconf->pll_config[0].pll_m) {
		lcd_vx1_lvds_ctrl_setb(pdrv, reg, cconf->pll_config[0].pll_m, 0, 9);
		LCD_DBG(pdrv, "tcon pll m reg 0x%x: 0x%08x->0x%08x",
				reg, val, lcd_vx1_lvds_ctrl_read(pdrv, reg));
	}
	lcd_delay_us(10);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK, 1, 4, 1);
	lcd_delay_us(10);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK, 0, 4, 1);

	if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL) {
		reg = ANACTRL_GP2PLL_CTRL1;
		val = lcd_ana_read(reg);
		lcd_ana_setb(reg, cconf->pll_config[1].pll_frac, 0, 19);
		LCD_DBG(pdrv, "gp2 pll frac reg 0x%x: 0x%08x->0x%08x",
				reg, val, lcd_ana_read(reg));

		reg = ANACTRL_GP2PLL_CTRL0;
		val = lcd_ana_read(reg);
		if ((val & 0x1ff) != cconf->pll_config[1].pll_m) {
			lcd_ana_setb(reg, cconf->pll_config[1].pll_m, 0, 9);
			LCD_DBG(pdrv, "gp2 pll m reg 0x%x: 0x%08x->0x%08x",
					reg, val, lcd_ana_read(reg));
		}
	}
}

static void lcd_set_pll_ss_t6x(struct aml_lcd_drv_s *pdrv,
			struct lcd_clk_config_s *cconf, unsigned int ss_flag)
{
	unsigned int pll_ctrl0, pll_ctrl2;
	char prt_str[64];
	int len = 0;

	if (lcd_pxp_mode)
		return;

	pll_ctrl0 = lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_CNTL0);
	pll_ctrl2 = lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_CNTL2);

	if (ss_flag & LCD_SSC_LEVEL) {
		pll_ctrl0 &= ~(1 << 11);
		pll_ctrl2 &= ~((0xf << 4) | (0xf << 16));

		if (cconf->ss_level > 0) {
			cconf->ss_en = 1;
			pll_ctrl0 |= (1 << 11);
			pll_ctrl2 |= ((cconf->ss_dep_sel << 16) | (cconf->ss_str_m << 4));
			len += sprintf(prt_str + len, "level: %d, %dppm",
					cconf->ss_level, cconf->ss_ppm);
		} else {
			cconf->ss_en = 0;
			len += sprintf(prt_str + len, "disable");
		}
	}

	if (ss_flag & LCD_SSC_FREQ) {
		pll_ctrl2 &= ~(0x7 << 8); /* ss_freq */
		pll_ctrl2 |= (cconf->ss_freq << 8);
		len += sprintf(prt_str + len, "%sfreq=%d", len ? ", " : "", cconf->ss_freq);
	}

	if (ss_flag & LCD_SSC_MODE) {
		pll_ctrl2 &= ~(0x3 << 2); /* ss_mode */
		pll_ctrl2 |= (cconf->ss_mode << 2);
		len += sprintf(prt_str + len, "%smode=%d", len ? ", " : "", cconf->ss_mode);
	}

	lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL2, pll_ctrl2);
	lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL0, pll_ctrl0);
	LCDPR("[%d]: set ssc: %s\n", pdrv->index, prt_str);
}

static int lcd_pll_ss_check(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf)
{
	unsigned int div_0p5_en = 1;
	unsigned long long pll_fvco;
	int ret;

	if (cconf->ss_level > 0) {
		ret = lcd_pll_ss_level_generate(cconf);
		if (ret)
			return -1;
		if (cconf->ss_ppm >= 50000)
			div_0p5_en = 0;
		if (div_0p5_en != cconf->data->pll_data[0]->pll_div_0p5_en) {
			cconf->data->pll_data[0]->pll_div_0p5_en = div_0p5_en;
			pll_fvco = cconf->pll_config[0].pll_fvco;
			ret = check_vco(cconf, 0, pll_fvco);
			if (ret == 0) {
				LCDERR("[%d]: %s: check_vco %lld error\n",
					pdrv->index, __func__, pll_fvco);
				return -1;
			}
			return 1;
		}
	}

	return 0;
}

static void lcd_set_pll_ss_tuning(struct aml_lcd_drv_s *pdrv, unsigned int ss_flag)
{
	struct lcd_clk_config_s *cconf;
	int ret;

	if (lcd_pxp_mode)
		return;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	ret = lcd_pll_ss_check(pdrv, cconf);
	if (ret < 0)
		return;
	if (ret == 1) {
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0,
				cconf->data->pll_data[0]->pll_div_0p5_en, 12, 1);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0,
				cconf->pll_config[0].pll_m, 0, 9);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL4,
				cconf->pll_config[0].pll_frac, 0, 17);
		lcd_delay_us(10);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK, 1, 4, 1);
		lcd_delay_us(10);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK, 0, 4, 1);
	}
	lcd_set_pll_ss_t6x(pdrv, cconf, ss_flag);
}

static void lcd_pll_ss_enable_t6x(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0, pll_ctrl2;
	unsigned int flag;

	if (lcd_pxp_mode)
		return;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl0 = lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_CNTL0);
	pll_ctrl2 = lcd_vx1_lvds_ctrl_read(pdrv, ANACTRL_TCON_PLL0_CNTL2);

	pll_ctrl0 &= ~(1 << 11);
	pll_ctrl2 &= ~((0xf << 4) | (0xf << 16));

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
		pll_ctrl0 |= (1 << 11);
		pll_ctrl2 |= ((cconf->ss_dep_sel << 16) | (cconf->ss_str_m << 4));
		LCD_PR(pdrv, "[%d]: pll ss enable: level: %d, %dppm",
			cconf->ss_level, cconf->ss_ppm);
	} else {
		cconf->ss_en = 0;
		LCD_PR(pdrv, "[%d]: pll ss disable");
	}
	lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL2, pll_ctrl2);
	lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL0, pll_ctrl0);
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

static void lcd_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 29, 1);
	usleep_range(10, 20);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 1, 29, 1);

	if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL) {
		lcd_ana_setb(ANACTRL_GP2PLL_CTRL0, 1, 29, 1);
		usleep_range(10, 20);
		lcd_ana_setb(ANACTRL_GP2PLL_CTRL0, 0, 29, 1);
	}
}

static void lcd_set_pll_t6x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0, pll_ctrl1, pll_ctrl2, pll_ctrl3, pll_ctrl4;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	if (lcd_pxp_mode)
		return;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	ret = lcd_pll_ss_check(pdrv, cconf);
	if (ret < 0) {
		LCDERR("[%d]: %s: eixt for ss_check error\n", pdrv->index, __func__);
		return;
	}

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl0 = 0x8022000 | (cconf->pll_config[0].pll_m << 0) |
			(cconf->data->pll_data[0]->pll_div_0p5_en << 12);
	pll_ctrl1 = 0x1030600c;
	pll_ctrl2 = 0x32000000 |
		(1 << 24) |
		(tcon_div_t6x[tcon_div_sel][0] << 30) | /*div2_4_sel*/
		(tcon_div_t6x[tcon_div_sel][2] << 31); /*clk1x_sel*/
	pll_ctrl3 = 0x180 |
		(cconf->pll_config[0].pll_od1_sel << 10) |
		(cconf->pll_config[0].pll_od2_sel << 14) |
		(cconf->pll_config[0].pll_od3_sel << 12) |
		(1 << 9) | /*tcon_od_en*/
		(tcon_div_t6x[tcon_div_sel][1] << 30); /*tcon_bypass_en*/
	pll_ctrl4 = cconf->pll_config[0].pll_frac | (tcon_div_t6x[tcon_div_sel][3] << 20);

	do {
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL0, pll_ctrl0);
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL1, pll_ctrl1);
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL2, pll_ctrl2);
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL3, pll_ctrl3);
		lcd_vx1_lvds_ctrl_write(pdrv, ANACTRL_TCON_PLL0_CNTL4, pll_ctrl4);
		usleep_range(10, 15);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 1, 28, 1);
		usleep_range(10, 15);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 18, 1);
		usleep_range(10, 15);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 1, 30, 1);
		usleep_range(80, 85);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 1, 29, 1);
		usleep_range(80, 85);
		ret = lcd_vx_pll_wait_lock(pdrv, cconf->pll_config[0].pll_id,
						ANACTRL_TCON_PLL0_STS, 31);
	} while (ret && ++cnt < PLL_RETRY_MAX);
	if (ret)
		LCDERR("[%d]: vx1 pll lock failed\n", pdrv->index);

	//set phase load sequence
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL3, 0, 31, 1);
	usleep_range(10, 15);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL3, 1, 31, 1);

	/* set load to 0 */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK, 0, 4, 1);
	/* select ANACTRL_TCON_PLL_VLOCK[4] as load */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL_VLOCK, 0, 3, 1);
	/* enable load en*/
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 1, 14, 1);

	if ((cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)) {
		cnt = 0;
		ret = 0;
		do {
			pll_ctrl0 = 0x20000000 |
				((cconf->pll_config[1].pll_od1_sel & 0x3) << 10) |
				(cconf->pll_config[1].pll_m & 0x1ff) |
				((cconf->pll_config[1].pll_n & 0x1f) << 16);
			pll_ctrl1 = 0x03a00000 |
				(cconf->pll_config[1].pll_frac & 0x7ffff);
			pll_ctrl3 = 0x0b0da000 | (cconf->data->pll_data[1]->pll_div_0p5_en << 26);
			lcd_ana_write(ANACTRL_GP2PLL_CTRL0, pll_ctrl0);
			lcd_ana_setb(ANACTRL_GP2PLL_CTRL0, 1, 28, 1);
			lcd_ana_write(ANACTRL_GP2PLL_CTRL1, pll_ctrl1);
			lcd_ana_write(ANACTRL_GP2PLL_CTRL2, 0x00040000);
			lcd_ana_write(ANACTRL_GP2PLL_CTRL3, pll_ctrl3);
			usleep_range(20, 30);
			lcd_ana_setb(ANACTRL_GP2PLL_CTRL0, 0, 29, 1);
			usleep_range(20, 30);
			lcd_ana_setb(ANACTRL_GP2PLL_CTRL3, 1, 9, 1);
			ret = lcd_pll_wait_lock(cconf->pll_config[1].pll_id,
						ANACTRL_GP2PLL_CTRL0, 31);
		} while (ret && ++cnt < PLL_RETRY_MAX);
	}
	if (ret)
		LCDERR("[%d]: vx1 pll lock failed\n", pdrv->index);

	if (cconf->ss_level > 0)
		lcd_set_pll_ss_t6x(pdrv, cconf, (LCD_SSC_LEVEL | LCD_SSC_FREQ | LCD_SSC_MODE));
}

static void lcd_disable_pll_t6x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (pdrv->lcd_pxp)
		return;
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 28, 1);  //disable
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 29, 1);  //lock resetn
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 30, 1);  //pll resetn

	if ((cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)) {
		lcd_ana_setb(ANACTRL_GP2PLL_CTRL0, 0, 28, 1);
		lcd_ana_setb(ANACTRL_GP2PLL_CTRL0, 1, 29, 1);
	}
}

static void lcd_set_vid_pll_div_t6x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;

	if (lcd_pxp_mode)
		return;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);

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
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 1, 18, 1);
	} else {
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 18, 1);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 20, 1);/*div8_25*/
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 16, 2);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 0, 15);

		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, (shift_sel >> 2), 20, 1);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 1, 15, 1);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, (shift_val & 0x7fff), 0, 15);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, (shift_val >> 15), 21, 11);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_VID_PLL_CLK_DIV, 1, 19, 1);
}

static void lcd_clk_set_t6x(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll_t6x(pdrv);
	lcd_set_vid_pll_div_t6x(pdrv);
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int xd = 1, clk_sel = 7;
	unsigned int encl_clk_sel = 8;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_clk_write(CLKCTRL_VID_CLK0_CTRL2, 0);
	lcd_clk_write(CLKCTRL_VIID_CLK0_CTRL, 0);
	lcd_clk_write(CLKCTRL_VIID_CLK0_DIV, 0);
	usleep_range(5, 10);

	lcd_clk_setb(CLKCTRL_HDMI_VID_PLL_CLK_DIV, 0, 24, 1);

	if (lcd_pxp_mode) {
		pxp_clk_div_gen(pdrv->curr_dev->dev_cfg.timing.enc_clk * 2,
			&xd, &clk_sel);// *2 for pxp div 2
		/* setup the XD divider value */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, xd - 1, VCLK2_XD, 8);
		usleep_range(5, 10);
		/* select vid_pll_clk */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 7, VCLK2_CLK_IN_SEL, 3);
		lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 1, VCLK2_DIV2_EN, 1); // t6x 2ppc require
		encl_clk_sel = 9;// t6x 2ppc require div2
	} else {
		/* setup the XD divider value */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, (cconf->xd - 1), VCLK2_XD, 8);
		usleep_range(5, 10);
		/* select vid_pll_clk */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
		encl_clk_sel = 8;
	}
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 1, VCLK2_EN, 1);
	usleep_range(2, 7);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, encl_clk_sel, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, 1, VCLK2_XD_EN, 2);
	usleep_range(5, 10);

	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 1, VCLK2_SOFT_RST, 1);
	usleep_range(10, 15);
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, VCLK2_SOFT_RST, 1);
	usleep_range(5, 10);
	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(CLKCTRL_VID_CLK0_CTRL2, 1, ENCL_GATE_VCLK, 1);
}

/* tcon run base clk, include register access */
static void lcd_set_tcon_clk_t6x(struct aml_lcd_drv_s *pdrv)
{
#ifdef CONFIG_AMLOGIC_LCD_TCON
	struct lcd_clk_config_s *cconf;

	if (!pdrv->curr_dev || (pdrv->status & LCD_STATUS_IF_ON))
		return;
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_MLVDS &&
	    pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_P2P)
		return;
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	LCD_DBG(pdrv, "%s", __func__);

	/* tcon_clk 50M */
	if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
		clk_set_rate(cconf->clktree.tcon_clk, 50000000);
		clk_prepare_enable(cconf->clktree.tcon_clk);
	}

	lcd_tcon_global_reset(pdrv);
#endif
}

static void lcd_clk_set_dummy_t6x(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 5, 16, 3);
	lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, 19, 0, 8);

	lcd_disable_pll_t6x(pdrv);
}

static void lcd_clk_disable_t6x(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VID_CLK0_CTRL2, 0, ENCL_GATE_VCLK, 1);

	/* close vclk2_div gate: [4:0] */
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, 0, 5);
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, VCLK2_EN, 1);

	lcd_disable_pll_t6x(pdrv);
}

static void lcd_prbs_config_clk_t6x(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode,
		unsigned int *encl_clk, unsigned int *fifo_clk)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned long long bit_rate = 0;

	if (!cconf)
		return;

	if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
		bit_rate = 2970000000ULL;
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		bit_rate = 550000000ULL;
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_FREQ) {
		bit_rate = lcd_prbs_freq * 1000000ULL;
	} else {
		LCDERR("[%d]: %s: unsupport lcd_prbs_mode %d\n",
		       pdrv->index, __func__, lcd_prbs_mode);
		return;
	}

	*encl_clk = lcd_do_div(bit_rate, 5);
	*fifo_clk = lcd_do_div(bit_rate, 10);
	lcd_clk_generate_prbs_clk(pdrv, *encl_clk, bit_rate);
	if (cconf->pll_config[0].done == 0)
		return;
	cconf->data->vclk_sel = 0;
	lcd_clk_set_t6x(pdrv);
	lcd_set_vclk_crt(pdrv);

	LCD_DBG(pdrv, "%s ok", __func__);
}

static void lcd_clk_set_dummy_t6x(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 5, 16, 3);
	lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, 19, 0, 8);
	if (lcd_pxp_mode)
		return;
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 28, 1);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 29, 1);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_TCON_PLL0_CNTL0, 0, 30, 1);
}

static void lcd_clk_prbs_test_t6x(struct aml_lcd_drv_s *pdrv,
				 unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int reg_phy_tx_ctrl1;
	int encl_msr_id, fifo_msr_id;
	unsigned int lcd_prbs_mode, lcd_prbs_cnt;
	unsigned int val, timeout;
	unsigned int clk_err_cnt = 0;
	unsigned int lcd_encl_clk_check_std = 0, lcd_fifo_clk_check_std = 0;
	int i, j, bit;
	unsigned int chdig_reg[8] = {
		ANACTRL_DIF_PHY_CNTL8, ANACTRL_DIF_PHY_CNTL9,
		ANACTRL_DIF_PHY_CNTL10, ANACTRL_DIF_PHY_CNTL11,
		ANACTRL_DIF_PHY_CNTL12, ANACTRL_DIF_PHY_CNTL13,
		ANACTRL_DIF_PHY_CNTL18, ANACTRL_DIF_PHY_CNTL19,
	};

	if (lcd_pxp_mode)
		return;

	if (!cconf)
		return;

	switch (pdrv->index) {
	case 0:
		reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
		break;
	default:
		LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
		return;
	}

	encl_msr_id = cconf->data->enc_clk_msr_id;
	fifo_msr_id = cconf->data->fifo_clk_msr_id;
	timeout = (ms > 1000) ? 1000 : ms;
	LCD_PR(pdrv, "ms:%d, mode_flag:0x%x, timeout:%d", ms, mode_flag, timeout);

	for (i = 0; i < LCD_PRBS_MODE_MAX; i++) {
		if ((mode_flag & (1 << i)) == 0)
			continue;

		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL14, 0, 19, 1);
		lcd_vx1_lvds_ctrl_write(pdrv, reg_phy_tx_ctrl1, 0);

		lcd_prbs_cnt = 0;
		clk_err_cnt = 0;
		lcd_prbs_mode = (1 << i);
		LCD_PR(pdrv, "lcd_prbs_mode: 0x%x", lcd_prbs_mode);
		lcd_prbs_config_clk_t6x(pdrv, lcd_prbs_mode, &lcd_encl_clk_check_std,
				&lcd_fifo_clk_check_std);
		usleep_range(500, 510);

		for (j = 0; j < 16; j++) {
			bit = j & 0x1 ? 16 : 0;
			lcd_vx1_lvds_ctrl_setb(pdrv, chdig_reg[j >> 1], 3, bit + 6, 2);
			lcd_vx1_lvds_ctrl_setb(pdrv, chdig_reg[j >> 1], 2, bit + 14, 2);
		}
		lcd_vx1_lvds_ctrl_write(pdrv, reg_phy_tx_ctrl1, 0x43000000);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL14, 1, 19, 1);
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL14, 3, 16, 2);
		while (lcd_prbs_flag) {
			if (lcd_prbs_cnt++ >= timeout)
				break;
			usleep_range(1000, 1001);

			if (lcd_prbs_clk_check(lcd_encl_clk_check_std, encl_msr_id,
					       lcd_fifo_clk_check_std, fifo_msr_id,
					       lcd_prbs_cnt))
				clk_err_cnt++;
			else
				clk_err_cnt = 0;
			if (clk_err_cnt >= 10) {
				LCD_ERR(pdrv, "prbs check error 1(clkmsr), cnt: %d", lcd_prbs_cnt);
				goto lcd_prbs_test_err_t6x;
			}

			val = lcd_vx1_lvds_ctrl_getb(pdrv, reg_phy_tx_ctrl1, 0, 16);
			if (val != 0xffff) {
				LCD_ERR(pdrv, "prbs check error 2(prbs), val:%d", val);
				goto lcd_prbs_test_err_t6x;
			}
		}

		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_LVDS);
			LCD_PR(pdrv, "lvds prbs check ok");
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_VX1);
			LCD_PR(pdrv, "vx1 prbs check ok");
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_FREQ) {
			lcd_prbs_performed |= LCD_PRBS_MODE_FREQ;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_FREQ);
			LCD_PR(pdrv, "freq %dMHz prbs check ok", lcd_prbs_freq);
		} else {
			LCD_PR(pdrv, "prbs check: unsupport mode");
		}
		continue;

lcd_prbs_test_err_t6x:
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err |= LCD_PRBS_MODE_LVDS;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err |= LCD_PRBS_MODE_LVDS;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_FREQ) {
			lcd_prbs_performed |= LCD_PRBS_MODE_FREQ;
			lcd_prbs_err |= LCD_PRBS_MODE_FREQ;
		}
	}
	for (j = 0; j < 16; j++) {
		bit = j & 0x1 ? 16 : 0;
		lcd_vx1_lvds_ctrl_setb(pdrv, chdig_reg[j >> 1], 0x8030, bit, 16);
	}

	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL14, 0, 19, 1);
	lcd_vx1_lvds_ctrl_write(pdrv, reg_phy_tx_ctrl1, 0);
	lcd_clk_generate_parameter(pdrv);
	lcd_clk_set_t6x(pdrv);
	lcd_set_vclk_crt(pdrv);
	lcd_vx1_lvds_ctrl_write(pdrv, reg_phy_tx_ctrl1, 0x3000000);
	lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_DIF_PHY_CNTL14, 1, 19, 1);

	lcd_prbs_flag = 0;
}

static int lcd_clk_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i = 0, n, len = 0;
	unsigned int *table = NULL, size = 0;
	struct lcd_clk_config_s *cconf;
	unsigned int pll_reg_table[] = {
		ANACTRL_TCON_PLL0_CNTL0,
		ANACTRL_TCON_PLL0_CNTL1,
		ANACTRL_TCON_PLL0_CNTL2,
		ANACTRL_TCON_PLL0_CNTL3,
		ANACTRL_TCON_PLL0_CNTL4,
		ANACTRL_TCON_PLL0_STS,
		ANACTRL_VID_PLL_CLK_DIV,
	};
	unsigned int gp2_pll_reg_table[] = {
		ANACTRL_GP2PLL_CTRL0,
		ANACTRL_GP2PLL_CTRL1,
		ANACTRL_GP2PLL_CTRL2,
		ANACTRL_GP2PLL_CTRL3,
	};
	unsigned int clk_reg_table[] = {
		CLKCTRL_VIID_CLK0_DIV,
		CLKCTRL_VIID_CLK0_CTRL,
		CLKCTRL_VID_CLK0_CTRL2,
		CLKCTRL_HDMI_VID_PLL_CLK_DIV,
		CLKCTRL_TCON_CLK_CNTL,
	};

	if (!pdrv)
		return 0;

	if (!lcd_pxp_mode) {
		table = pll_reg_table;
		size = ARRAY_SIZE(pll_reg_table);
		for (i = 0; i < size; i++) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
				table[i], lcd_vx1_lvds_ctrl_read(pdrv, table[i]));
		}
	}

	cconf = get_lcd_clk_config(pdrv);
	if (cconf && (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)) {
		table = gp2_pll_reg_table;
		size = ARRAY_SIZE(gp2_pll_reg_table);
		for (i = 0; i < size; i++) {
			n = lcd_debug_info_len(len + offset);
			len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
				table[i], lcd_ana_read(table[i]));
		}
	}

	table = clk_reg_table;
	size = ARRAY_SIZE(clk_reg_table);
	for (i = 0; i < size; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "clk [0x%02x] = 0x%08x\n",
			table[i], lcd_clk_read(table[i]));
	}

	return len;
}

static void lcd_clk_generate_t6x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_P2P &&
	    pdrv->curr_dev->dev_cfg.timing.act_timing.clk_mode == LCD_BIT_RATE_ADAPT) {
		LCD_PR(pdrv, "%s: dual pll config", __func__);
		if (cconf->pll_conf_num < 2) {
			LCD_ERR(pdrv, "%s: clk_conf_num %d error", __func__, cconf->pll_conf_num);
			return;
		}
		cconf->pll_mode |= LCD_PLL_MODE_DUAL_PLL;
		cconf->data->vclk_sel = 3;
	} else {
		cconf->pll_mode &= ~LCD_PLL_MODE_DUAL_PLL;
		cconf->data->vclk_sel = 0;
	}

	lcd_clk_generate_dft(pdrv);
}

static struct lcd_pll_data_s lcd_pll_data_tcon_t6x = {
	.pll_od_fb = 1,
	.pll_div_0p5_en = 1,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_od_sel_max = 3,
	.pll_ref_fmax = 25000000,
	.pll_ref_fmin = 5000000,
	.pll_vco_fmax = 6000000000ULL,
	.pll_vco_fmin = 3000000000ULL,
	.pll_out_fmax = 4250000000ULL,
	.pll_out_fmin = 5859375,
	.od_cnt = 3,
	.have_tcon_div = 1,
	.div_in_fmax = 4250000000ULL,
	.div_out_fmax = 840000000,
	.div_sel_max = CLK_DIV_SEL_2p25,
};

static struct lcd_pll_data_s lcd_pll_data_gp2_t6x = {
	.pll_od_fb = 0,
	.pll_div_0p5_en = 1,
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

static struct lcd_clk_data_s lcd_clk_data_t6x = {
	.pll_data[0] = &lcd_pll_data_tcon_t6x,
	.pll_data[1] = &lcd_pll_data_gp2_t6x,
	.xd_out_fmax = 840000000,
	.phy_clk_location = 0,

	.xd_max = 256,
	.phy_div_max = 256,

	.ss_support = 1,
	.ss_level_max = 60,
	.ss_freq_max = 6,
	.ss_mode_max = 2,
	.ss_dep_base = 500, //ppm
	.ss_dep_sel_max = 12,
	.ss_str_m_max = 10,
	.ss_freq_dep_opt = lcd_ss_freq_dep_opt_t6x,

	.vclk_sel = 0,
	.enc_clk_msr_id = 222,
	.fifo_clk_msr_id = 86,
	.tcon_clk_msr_id = 119,

	.clktree_set = lcd_set_tcon_clk_t6x,
	.clktree_index = {
		CLKTREE_TCON_GATE,
		CLKTREE_TCON,
		0, 0
	},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_t6x,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss = lcd_set_pll_ss_tuning,
	.clk_ss_enable = lcd_pll_ss_enable_t6x,
	.clk_ss_init = lcd_pll_ss_init_dft,
	.pll_frac_set = lcd_pll_frac_set_t6x,
	.pll_m_set = NULL,
	.pll_hz_get = NULL,
	.pll_reset = lcd_pll_reset,
	.clk_set = lcd_clk_set_t6x,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_set_dummy = lcd_clk_set_dummy_t6x,
	.clk_disable = lcd_clk_disable_t6x,
	.mlvds_clk_phase_set = NULL,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.clk_reg_print = lcd_clk_reg_dump,
	.prbs_test = lcd_clk_prbs_test_t6x,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_t6x(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf;

	if (!pdrv)
		return NULL;
	pconf = &pdrv->curr_dev->dev_cfg;
	if (!pdrv->clk_conf) {
		cconf = kcalloc(1, sizeof(struct lcd_clk_config_s), GFP_KERNEL);
		if (!cconf) {
			LCD_ERR(pdrv, "%s: Not enough memory", __func__);
			return NULL;
		}
	} else {
		cconf = (struct lcd_clk_config_s *)pdrv->clk_conf;
		memset(cconf, 0, sizeof(struct lcd_clk_config_s));
	}
	if (pconf->basic.lcd_type == LCD_P2P)
		cconf->pll_conf_num = 2;
	else
		cconf->pll_conf_num = 1;
	cconf->pll_config = kcalloc(cconf->pll_conf_num, sizeof(struct lcd_pll_config_s),
					GFP_KERNEL);
	if (!cconf->pll_config) {
		LCD_ERR(pdrv, "%s: Not enough memory for pll config", __func__);
		kfree(cconf);
		return NULL;
	}

	cconf->data = &lcd_clk_data_t6x;
	cconf->pll_config[0].pll_id = 0;
	if (cconf->pll_conf_num > 1)
		cconf->pll_config[1].pll_id = 1;
	return cconf;
}

