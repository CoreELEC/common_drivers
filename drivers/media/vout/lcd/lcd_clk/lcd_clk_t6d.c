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

static unsigned int tcon_div_t6d[][3] = {
	/* vx1pll_div214h, vx1pll_phase_div_en, vx1pll_clk1x_selh */
	{0, 0, 1},  /* div1 */
	{0, 0, 0},  /* div2 */
	{0, 0, 0},  /* div4 */
	{0, 1, 0},  /* div8 */
	{1, 1, 0},  /* div16 */
};

static void lcd_pll_frac_set_t6d(struct aml_lcd_drv_s *pdrv, unsigned int frac)
{
	struct lcd_clk_config_s *cconf;
	unsigned int reg, val;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	reg = ANACTRL_TCON_PLL0_CNTL4;
	val = lcd_ana_read(reg);
	lcd_ana_setb(reg, frac, 0, 17);
	LCD_DBG(pdrv, "tcon pll frac reg 0x%x: 0x%08x->0x%08x",
			reg, val, lcd_ana_read(reg));
	reg = ANACTRL_TCON_PLL0_CNTL0;
	val = lcd_ana_read(reg);
	if ((val & 0x1ff) != cconf->pll_config[0].pll_m) {
		lcd_ana_setb(reg, cconf->pll_config[0].pll_m, 0, 9);
		LCD_DBG(pdrv, "tcon pll m reg 0x%x: 0x%08x->0x%08x", reg, val, lcd_ana_read(reg));
	}
	lcd_delay_us(10);
	lcd_ana_setb(ANACTRL_TCON_PLL_VLOCK, 1, 4, 1);
	lcd_delay_us(10);
	lcd_ana_setb(ANACTRL_TCON_PLL_VLOCK, 0, 4, 1);
}

static void lcd_set_pll_ss_t6d(struct aml_lcd_drv_s *pdrv, struct lcd_clk_config_s *cconf,
			       unsigned int ss_flag)
{
	unsigned int pll_ctrl0, pll_ctrl2;
	char prt_str[64];
	int len = 0, ret;

	pll_ctrl0 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL0);
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2);

	if (ss_flag & LCD_SSC_LEVEL) {
		pll_ctrl0 &= ~(1 << 11);
		pll_ctrl2 &= ~((0xf << 4) | (0xf << 16) | (1 << 27));

		if (cconf->ss_level > 0) {
			ret = lcd_pll_ss_level_generate(cconf);
			if (ret == 0) {
				cconf->ss_en = 1;
				pll_ctrl0 |= (1 << 11);
				pll_ctrl2 |= (1 << 27);//lpf_r3
				pll_ctrl2 |= ((cconf->ss_dep_sel << 16) | (cconf->ss_str_m << 4));
				len += sprintf(prt_str + len, "level: %d, %dppm",
					       cconf->ss_level, cconf->ss_ppm);
			}
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

	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, pll_ctrl2);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, pll_ctrl0);

	LCD_PR(pdrv, "pll set ssc: %s", len ? prt_str : "none");
}

static void lcd_set_pll_ss_tuning(struct aml_lcd_drv_s *pdrv, unsigned int ss_flag)
{
	struct lcd_clk_config_s *cconf;

	if (pdrv->lcd_pxp)
		return;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_set_pll_ss_t6d(pdrv, cconf, ss_flag);
}

static void lcd_pll_ss_enable_t6d(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0, pll_ctrl2;
	unsigned int flag;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl0 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL0);
	pll_ctrl2 = lcd_ana_read(ANACTRL_TCON_PLL0_CNTL2);

	pll_ctrl0 &= ~(1 << 11);
	pll_ctrl2 &= ~((0xf << 4) | (0xf << 16) | (1 << 27));

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
		pll_ctrl2 |= (1 << 27);//lpf_r3
		pll_ctrl2 |= ((cconf->ss_dep_sel << 16) | (cconf->ss_str_m << 4));
		LCD_PR(pdrv, "pll ss enable: level: %d, %dppm", cconf->ss_level, cconf->ss_ppm);
	} else {
		cconf->ss_en = 0;
		LCD_PR(pdrv, "pll ss disable");
	}
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, pll_ctrl2);
	lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, pll_ctrl0);
}

static void lcd_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, 29, 1);
	usleep_range(10, 20);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, 29, 1);
}

static void lcd_set_pll_t6d(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0, pll_ctrl2, pll_ctrl3, pll_ctrl4;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;

	LCD_DBG_ADV2(pdrv, "%s", __func__);

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl0 = (0x2 << 16) | (1 << 13) | (cconf->pll_config[0].pll_m << 0);
	pll_ctrl2 = 0x02000000 | (cconf->data->pll_data[0]->pll_od_fb << 24);
	pll_ctrl3 = (0x7 << 7) |
		(cconf->pll_config[0].pll_od1_sel << 10) |
		(cconf->pll_config[0].pll_od2_sel << 14) |
		(cconf->pll_config[0].pll_od3_sel << 12) |
		(tcon_div_sel << 24) |
		(0x1 << 31);
	if (pconf->basic.lcd_type != LCD_MLVDS) {
		pll_ctrl3 &= ~(1 << 27);
	} else {
		if (tcon_div_sel == 1 || tcon_div_sel == 2) {
			LCD_ERR(pdrv, "div4 and div8 with phase shift not support");
			return;
		}
		pll_ctrl3 |= (1 << 27) |
			(tcon_div_t6d[tcon_div_sel][0] << 28) |
			(tcon_div_t6d[tcon_div_sel][1] << 29) |
			(tcon_div_t6d[tcon_div_sel][2] << 30);
	}
	pll_ctrl4 = cconf->pll_config[0].pll_frac;

	do {
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL0, pll_ctrl0);
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL1, 0x12301635);
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL2, pll_ctrl2);
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL3, pll_ctrl3);
		lcd_ana_write(ANACTRL_TCON_PLL0_CNTL4, pll_ctrl4);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, 28, 1);
		usleep_range(10, 15);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, 18, 1);
		usleep_range(80, 85);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, 18, 1);
		lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 3, 29, 2);

		ret = lcd_pll_wait_lock(cconf->pll_config[0].pll_id, ANACTRL_TCON_PLL0_STS, 31);
	} while (ret && ++cnt < PLL_RETRY_MAX);

	if (ret)
		LCD_ERR(pdrv, "pll lock failed\n");

	if (cconf->ss_level > 0)
		lcd_set_pll_ss_t6d(pdrv, cconf, (LCD_SSC_LEVEL | LCD_SSC_FREQ | LCD_SSC_MODE));

	/* set load to 0 */
	lcd_ana_setb(ANACTRL_TCON_PLL_VLOCK, 0, 4, 1);
	/* select ANACTRL_TCON_PLL_VLOCK[4] as load */
	lcd_ana_setb(ANACTRL_TCON_PLL_VLOCK, 0, 3, 1);
	/* enable load en*/
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, 14, 1);
}

static void lcd_set_vid_pll_div_t6d(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);

	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, VCLK2_EN, 1);
	usleep_range(5, 10);

	/* Disable the div output clock */
	lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);

	if (cconf->data->pll_data[0]->div_sel_max == CLK_DIV_SEL_1 ||
	    cconf->pll_config->div_sel > cconf->data->pll_data[0]->div_sel_max ||
	    cconf->pll_config->div_sel >= ARRAY_SIZE(lcd_clk_div_table)) {
		LCD_ERR(pdrv, "invalid clk divider");
		return;
	}

	shift_val = lcd_clk_div_table[cconf->pll_config->div_sel].shift_val;
	shift_sel = lcd_clk_div_table[cconf->pll_config->div_sel].shift_sel;

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 1, 18, 1);
	} else {
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 18, 1);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 20, 1);/*div8_25*/
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 16, 2);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 0, 15);

		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 1, 15, 1);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, shift_val, 0, 15);
		lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_ana_setb(ANACTRL_VID_PLL_CLK_DIV, 1, 19, 1);
}

static void lcd_clk_set_t6d(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll_t6d(pdrv);
	lcd_set_vid_pll_div_t6d(pdrv);
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

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

	if (pdrv->lcd_pxp) {
		/* setup the XD divider value */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, cconf->xd, VCLK2_XD, 8);
		usleep_range(5, 10);
		/* select vid_pll_clk */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 7, VCLK2_CLK_IN_SEL, 3);
	} else {
		/* setup the XD divider value */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, (cconf->xd - 1), VCLK2_XD, 8);
		usleep_range(5, 10);
		/* select vid_pll_clk */
		lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
	}
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 1, VCLK2_EN, 1);
	usleep_range(2, 7);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, 8, ENCL_CLK_SEL, 4);
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

	/*reset fifo*/
	lcd_reset_setb(pdrv, RESETCTRL_RESET0_MASK, 0, 19, 1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET0_LEVEL, 0, 19, 1);
	udelay(1);
	lcd_reset_setb(pdrv, RESETCTRL_RESET0_LEVEL, 1, 19, 1);
	udelay(2);
}

static int lcd_set_mlvds_clk_phase_t6d(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int val, p0, pa, pb;

	val = pconf->phy_cfg.act_phy->clk_phase;
	p0 = val & 0xf;
	pa = (val >> 8) & 0xf;
	pb = (val >> 4) & 0xf;

	val = ((pa << 4) | (pb << 0)) & 0xff;
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL2, p0,  28, 4);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL3, val, 16, 8);

	//set phase load sequence
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL3, 0, 31, 1);
	usleep_range(5, 10);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL3, 1, 31, 1);

	return 0;
}

/* tcon run base clk, include register access */
static void lcd_set_tcon_clk_t6d(struct aml_lcd_drv_s *pdrv)
{
#ifdef CONFIG_AMLOGIC_LCD_TCON
	struct lcd_clk_config_s *cconf;
	unsigned int freq;

	if (!pdrv->curr_dev || (pdrv->status & LCD_STATUS_IF_ON))
		return;
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_MLVDS)
		return;
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	LCD_DBG(pdrv, "%s", __func__);

	lcd_set_mlvds_clk_phase_t6d(pdrv);

	/* tcon_clk */
	if (pdrv->curr_dev->dev_cfg.timing.enc_clk >= 100000000) /* 25M */
		freq = 25000000;
	else /* 12.5M */
		freq = 12500000;
	if (!IS_ERR_OR_NULL(cconf->clktree.tcon_clk)) {
		clk_set_rate(cconf->clktree.tcon_clk, freq);
		clk_prepare_enable(cconf->clktree.tcon_clk);
	}

	lcd_tcon_global_reset(pdrv);
#endif
}

static void lcd_prbs_config_clk_t6d(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode,
		unsigned int *encl_clk, unsigned int *fifo_clk)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned long long bit_rate = 0;

	if (!cconf)
		return;

	if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		bit_rate = 550000000ULL;
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_FREQ) {
		bit_rate = lcd_prbs_freq * 1000000ULL;
	} else {
		LCD_ERR(pdrv, "%s: unsupport lcd_prbs_mode %d", __func__, lcd_prbs_mode);
		return;
	}

	*encl_clk = lcd_do_div(bit_rate, 5);
	*fifo_clk = lcd_do_div(bit_rate, 7);
	lcd_clk_generate_prbs_clk(pdrv, *encl_clk, bit_rate);
	if (cconf->pll_config[0].done == 0)
		return;

	lcd_clk_set_t6d(pdrv);
	lcd_set_vclk_crt(pdrv);

	LCD_DBG(pdrv, "%s ok", __func__);
}

static void lcd_clk_set_dummy_t6d(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 5, 16, 3);
	lcd_clk_setb(CLKCTRL_VIID_CLK0_DIV, 19, 0, 8);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, 28, 1);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, 29, 1);
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, 30, 1);
}

static void lcd_clk_prbs_test_t6d(struct aml_lcd_drv_s *pdrv,
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
	unsigned int chdig_reg[5] = {
		ANACTRL_DIF_PHY_CNTL8, ANACTRL_DIF_PHY_CNTL9,
		ANACTRL_DIF_PHY_CNTL10, ANACTRL_DIF_PHY_CNTL11,
		ANACTRL_DIF_PHY_CNTL12,
	};
	unsigned char is_mlvds = pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MLVDS;

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

		lcd_ana_setb(ANACTRL_DIF_PHY_CNTL14, 0, 19, 1);
		lcd_ana_write(reg_phy_tx_ctrl1, 0);

		lcd_prbs_cnt = 0;
		clk_err_cnt = 0;
		lcd_prbs_mode = (1 << i);
		LCD_PR(pdrv, "lcd_prbs_mode: 0x%x", lcd_prbs_mode);
		lcd_prbs_config_clk_t6d(pdrv, lcd_prbs_mode, &lcd_encl_clk_check_std,
				&lcd_fifo_clk_check_std);
		usleep_range(500, 510);

		for (j = 0; j < 10; j++) {
			bit = j & 0x1 ? 16 : 0;
			lcd_ana_setb(chdig_reg[j >> 1], 0xc4c0, bit, 16);
		}
		lcd_ana_write(reg_phy_tx_ctrl1, 0xc3000000);
		lcd_ana_setb(ANACTRL_DIF_PHY_CNTL14, 1, 19, 1);
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
				goto lcd_prbs_test_err_t3;
			}

			val = lcd_ana_getb(reg_phy_tx_ctrl1, 0, 12);
			if (val != 0x3ff) {
				LCD_ERR(pdrv, "prbs check error 2(prbs), val:%d", val);
				goto lcd_prbs_test_err_t3;
			}
		}

		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_LVDS);
			LCD_PR(pdrv, "lvds prbs check ok");
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_FREQ) {
			lcd_prbs_performed |= LCD_PRBS_MODE_FREQ;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_FREQ);
			LCD_PR(pdrv, "freq %dMHz prbs check ok", lcd_prbs_freq);
		} else {
			LCD_PR(pdrv, "prbs check: unsupport mode");
		}
		continue;

lcd_prbs_test_err_t3:
		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err |= LCD_PRBS_MODE_LVDS;
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_FREQ) {
			lcd_prbs_performed |= LCD_PRBS_MODE_FREQ;
			lcd_prbs_err |= LCD_PRBS_MODE_FREQ;
		}
	}
	for (j = 0; j < 10; j++) {
		bit = j & 0x1 ? 16 : 0;
		lcd_ana_setb(chdig_reg[j >> 1],
				0x8400 | ((is_mlvds ? 0xff : 0) << 2),
				bit, 16);
	}

	lcd_ana_setb(ANACTRL_DIF_PHY_CNTL14, 0, 19, 1);
	lcd_ana_write(reg_phy_tx_ctrl1, 0);
	lcd_clk_generate_parameter(pdrv);
	lcd_clk_set_t6d(pdrv);
	lcd_set_vclk_crt(pdrv);
	lcd_ana_write(reg_phy_tx_ctrl1, 0xc3000000);
	lcd_ana_setb(ANACTRL_DIF_PHY_CNTL14, 1, 19, 1);

	lcd_prbs_flag = 0;
}

static int lcd_clk_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i = 0, n, len = 0;
	unsigned int *table = NULL, size = 0;
	unsigned int pll_reg_table[] = {
		ANACTRL_TCON_PLL0_CNTL0,
		ANACTRL_TCON_PLL0_CNTL1,
		ANACTRL_TCON_PLL0_CNTL2,
		ANACTRL_TCON_PLL0_CNTL3,
		ANACTRL_TCON_PLL0_CNTL4,
		ANACTRL_TCON_PLL0_STS,
		ANACTRL_VID_PLL_CLK_DIV,
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

	table = pll_reg_table;
	size = ARRAY_SIZE(pll_reg_table);
	for (i = 0; i < size; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
			table[i], lcd_ana_read(table[i]));
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

static void lcd_clk_disable_t6d(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_clk_setb(CLKCTRL_VID_CLK0_CTRL2, 0, ENCL_GATE_VCLK, 1);

	/* close vclk2_div gate: [4:0] */
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, 0, 5);
	lcd_clk_setb(CLKCTRL_VIID_CLK0_CTRL, 0, VCLK2_EN, 1);

	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 0, 28, 1);  //disable
	lcd_ana_setb(ANACTRL_TCON_PLL0_CNTL0, 1, 30, 1);  //resetn
}

static struct lcd_pll_data_s lcd_pll_data_t6d = {
	.pll_od_fb = 1,
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
	.pll_vco_fmax = 6000000000ULL,
	.pll_vco_fmin = 3000000000ULL,
	.pll_out_fmax = 3100000000ULL,
	.pll_out_fmin = 187500000,
	.od_cnt = 3,
	.have_tcon_div = 1,
	.div_in_fmax = 3100000000ULL,
	.div_out_fmax = 750000000,
	.div_sel_max = CLK_DIV_SEL_2p33,
};

static struct lcd_clk_data_s lcd_clk_data_t6d = {
	.pll_data[0] = &lcd_pll_data_t6d,
	.pll_data[1] = NULL,
	.xd_out_fmax = 200000000,
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
	.ss_freq_dep_opt = NULL,

	.vclk_sel = 0,
	.enc_clk_msr_id = 222,
	.fifo_clk_msr_id = 86,
	.tcon_clk_msr_id = 119,

	.clktree_set = lcd_set_tcon_clk_t6d,
	.clktree_index = {
		CLKTREE_TCON_GATE,
		CLKTREE_TCON,
		0, 0
	},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss = lcd_set_pll_ss_tuning,
	.clk_ss_enable = lcd_pll_ss_enable_t6d,
	.clk_ss_init = lcd_pll_ss_init_dft,
	.pll_frac_set = lcd_pll_frac_set_t6d,
	.pll_m_set = NULL,
	.pll_hz_get = NULL,
	.pll_reset = lcd_pll_reset,
	.clk_set = lcd_clk_set_t6d,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_set_dummy = lcd_clk_set_dummy_t6d,
	.clk_disable = lcd_clk_disable_t6d,
	.mlvds_clk_phase_set = lcd_set_mlvds_clk_phase_t6d,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.clk_reg_print = lcd_clk_reg_dump,
	.prbs_test = lcd_clk_prbs_test_t6d,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_t6d(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (!pdrv)
		return NULL;

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

	cconf->pll_conf_num = 1;
	cconf->pll_config = kcalloc(cconf->pll_conf_num, sizeof(struct lcd_pll_config_s),
					GFP_KERNEL);
	if (!cconf->pll_config) {
		LCD_ERR(pdrv, "%s: Not enough memory for pll config", __func__);
		kfree(cconf);
		return NULL;
	}

	cconf->data = &lcd_clk_data_t6d;
	cconf->pll_config[0].pll_id = 0;
	return cconf;
}
