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

static unsigned int pll_ss_reg_table[][2] = {
	/* dep_sel,  str_m  */
	{ 0,          0}, /* 0: disable */
	{ 4,          1}, /* 1: +/-0.1% */
	{ 4,          2}, /* 2: +/-0.2% */
	{ 4,          3}, /* 3: +/-0.3% */
	{ 4,          4}, /* 4: +/-0.4% */
	{ 4,          5}, /* 5: +/-0.5% */
	{ 4,          6}, /* 6: +/-0.6% */
	{ 4,          7}, /* 7: +/-0.7% */
	{ 4,          8}, /* 8: +/-0.8% */
	{ 4,          9}, /* 9: +/-0.9% */
	{ 4,         10}, /* 10: +/-1.0% */
	{ 11,         4}, /* 11: +/-1.1% */
	{ 12,         4}, /* 12: +/-1.2% */
	{ 10,         5}, /* 13: +/-1.25% */
	{ 8,          7}, /* 14: +/-1.4% */
	{ 6,         10}, /* 15: +/-1.5% */
	{ 8,          8}, /* 16: +/-1.6% */
	{ 11,         6}, /* 17: +/-1.65% */
	{ 8,          9}, /* 18: +/-1.8% */
	{ 11,         7}, /* 19: +/-1.925% */
	{ 10,         8}, /* 20: +/-2.0% */
	{ 12,         7}, /* 21: +/-2.1% */
	{ 11,         8}, /* 22: +/-2.2% */
	{ 9,         10}, /* 23: +/-2.25% */
	{ 12,         8}, /* 24: +/-2.4% */
	{ 10,        10}, /* 25: +/-2.5% */
	{ 10,        10}, /* 26: +/-2.5% */
	{ 12,         9}, /* 27: +/-2.7% */
	{ 11,        10}, /* 28: +/-2.75% */
	{ 11,        10}, /* 29: +/-2.75% */
	{ 12,        10}, /* 30: +/-3.0% */
};

static void lcd_pll_ss_init(struct lcd_clk_config_s *cconf)
{
	if (!cconf)
		return;

	if (cconf->ss_level > 0) {
		cconf->ss_en = 1;
		cconf->ss_dep_sel = pll_ss_reg_table[cconf->ss_level][0];
		cconf->ss_str_m = pll_ss_reg_table[cconf->ss_level][1];
		cconf->ss_ppm = cconf->ss_dep_sel * cconf->ss_str_m * cconf->data->ss_dep_base;
	} else {
		cconf->ss_en = 0;
	}
}

static void lcd_pll_ss_enable(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl2, flag;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl2 = lcd_ana_read(HHI_TCON_PLL_CNTL2);
	pll_ctrl2 &= ~((1 << 15) | (0xf << 16) | (0xf << 28));

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
		pll_ctrl2 |= ((1 << 15) | (cconf->ss_dep_sel << 28) | (cconf->ss_str_m << 16));
		LCDPR("pll ss enable: level %d, %dppm\n", cconf->ss_level, cconf->ss_ppm);
	} else {
		cconf->ss_en = 0;
		LCDPR("pll ss disable\n");
	}
	lcd_ana_write(HHI_TCON_PLL_CNTL2, pll_ctrl2);
}

static void lcd_set_pll_ss(struct aml_lcd_drv_s *pdrv, unsigned int ss_flag)
{
	struct lcd_clk_config_s *cconf;
	unsigned int level, pll_ctrl2;
	char prt_str[64];
	int len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	level = cconf->ss_level;
	pll_ctrl2 = lcd_ana_read(HHI_TCON_PLL_CNTL2);

	if (ss_flag & LCD_SSC_LEVEL) {
		pll_ctrl2 &= ~((1 << 15) | (0xf << 16) | (0xf << 28));

		if (level > 0) {
			cconf->ss_en = 1;
			cconf->ss_dep_sel = pll_ss_reg_table[level][0];
			cconf->ss_str_m = pll_ss_reg_table[level][1];
			cconf->ss_ppm =
				cconf->ss_dep_sel * cconf->ss_str_m * cconf->data->ss_dep_base;
			pll_ctrl2 |=
				((1 << 15) | (cconf->ss_dep_sel << 28) | (cconf->ss_str_m << 16));
			len += sprintf(prt_str + len, "level %d, %dppm", level, cconf->ss_ppm);
		} else {
			cconf->ss_en = 0;
			len += sprintf(prt_str + len, "disable");
		}
	}

	if (ss_flag & LCD_SSC_FREQ) {
		pll_ctrl2 &= ~(0x7 << 24); /* ss_freq */
		pll_ctrl2 |= (cconf->ss_freq << 24);
		len += sprintf(prt_str + len, "%sfreq=%d", len ? ", " : "", cconf->ss_freq);
	}

	if (ss_flag & LCD_SSC_MODE) {
		pll_ctrl2 &= ~(0x3 << 22); /* ss_mode */
		pll_ctrl2 |= (cconf->ss_mode << 22);
		len += sprintf(prt_str + len, "%smode=%d", len ? ", " : "", cconf->ss_mode);
	}

	lcd_ana_write(HHI_TCON_PLL_CNTL2, pll_ctrl2);
	LCDPR("set ssc: %s\n", prt_str);
}

static void lcd_pll_frac_set(struct aml_lcd_drv_s *pdrv, unsigned int frac)
{
	unsigned int val;

	val = lcd_ana_read(HHI_TCON_PLL_CNTL1);
	lcd_ana_setb(HHI_TCON_PLL_CNTL1, frac, 0, 19);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: reg 0x%x: 0x%08x->0x%08x, pll_frac=0x%x\n",
			__func__, HHI_TCON_PLL_CNTL1,
			val, lcd_ana_read(HHI_TCON_PLL_CNTL1), frac);
	}
}

static void lcd_pll_m_set(struct aml_lcd_drv_s *pdrv, unsigned int m)
{
	unsigned int val;

	val = lcd_ana_read(HHI_TCON_PLL_CNTL0);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, m, 0, 8);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL) {
		LCDPR("%s: reg 0x%x: 0x%08x->0x%08x, pll_m=0x%x\n",
			__func__, HHI_TCON_PLL_CNTL0,
			val, lcd_ana_read(HHI_TCON_PLL_CNTL0), m);
	}
}

static void lcd_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, 29, 1);
	usleep_range(10, 11);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 0, 29, 1);
}

static void lcd_set_pll(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1;
	unsigned int tcon_div_sel;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("lcd clk: set_pll_tl1\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	tcon_div_sel = cconf->pll_tcon_div_sel;
	pll_ctrl = ((0x3 << 17) | /* gate ctrl */
		(tcon_div[tcon_div_sel][2] << 16) |
		(cconf->pll_config[0].pll_n << LCD_PLL_N_TL1) |
		(cconf->pll_config[0].pll_m << LCD_PLL_M_TL1) |
		(cconf->pll_config[0].pll_od3_sel << LCD_PLL_OD3_TL1) |
		(cconf->pll_config[0].pll_od2_sel << LCD_PLL_OD2_TL1) |
		(cconf->pll_config[0].pll_od1_sel << LCD_PLL_OD1_TL1));
	pll_ctrl1 = (1 << 28) |
		(tcon_div[tcon_div_sel][0] << 22) |
		(tcon_div[tcon_div_sel][1] << 21) |
		((1 << 20) | /* sdm_en */
		(cconf->pll_config[0].pll_frac << 0));

set_pll_retry_tl1:
	lcd_ana_write(HHI_TCON_PLL_CNTL0, pll_ctrl);
	usleep_range(10, 15);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 15);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, LCD_PLL_EN_TL1, 1);
	usleep_range(10, 15);
	lcd_ana_write(HHI_TCON_PLL_CNTL1, pll_ctrl1);
	usleep_range(10, 15);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x0000110c);
	usleep_range(10, 15);
	lcd_ana_write(HHI_TCON_PLL_CNTL3, 0x10051400);
	usleep_range(10, 15);
	lcd_ana_setb(HHI_TCON_PLL_CNTL4, 0x0100c0, 0, 24);
	usleep_range(10, 15);
	lcd_ana_setb(HHI_TCON_PLL_CNTL4, 0x8300c0, 0, 24);
	usleep_range(10, 15);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, 26, 1);
	usleep_range(10, 15);
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 0, LCD_PLL_RST_TL1, 1);
	usleep_range(10, 15);
	lcd_ana_write(HHI_TCON_PLL_CNTL2, 0x0000300c);

	ret = lcd_pll_wait_lock(cconf->pll_config[0].pll_id, HHI_TCON_PLL_CNTL0, LCD_PLL_LOCK_TL1);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_tl1;
		LCDERR("hpll lock failed\n");
	} else {
		usleep_range(100, 105);
		lcd_ana_setb(HHI_TCON_PLL_CNTL2, 1, 5, 1);
	}

	if (cconf->ss_level > 0)
		lcd_set_pll_ss(pdrv, (LCD_SSC_LEVEL | LCD_SSC_FREQ | LCD_SSC_MODE));
}

static void lcd_clk_set(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	lcd_set_pll(pdrv);
	lcd_set_vid_pll_div_dft(pdrv);
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("lcd clk: set_vclk_crt\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_pxp_mode) {
		/* setup the XD divider value */
		lcd_clk_setb(HHI_VIID_CLK_DIV, cconf->xd, VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(HHI_VIID_CLK_CNTL, 7, VCLK2_CLK_IN_SEL, 3);
	} else {
		/* setup the XD divider value */
		lcd_clk_setb(HHI_VIID_CLK_DIV, (cconf->xd - 1), VCLK2_XD, 8);
		udelay(5);

		/* select vid_pll_clk */
		lcd_clk_setb(HHI_VIID_CLK_CNTL, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);
	}
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_EN, 1);
	udelay(2);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(HHI_VIID_CLK_DIV, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(HHI_VIID_CLK_DIV, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 1, VCLK2_SOFT_RST, 1);
	usleep_range(10, 11);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(HHI_VID_CLK_CNTL2, 1, ENCL_GATE_VCLK, 1);
}

static void lcd_clk_disable(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(HHI_VID_CLK_CNTL2, 0, 3, 1);

	/* close vclk2_div gate: 0x104b[4:0] */
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, 0, 5);
	lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, 19, 1);

	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 0, 28, 1);  //disable
	lcd_ana_setb(HHI_TCON_PLL_CNTL0, 1, 29, 1);  //reset
}

static int lcd_set_mlvds_clk_phase(struct aml_lcd_drv_s *pdrv)
{
	unsigned int phase_value;

	phase_value = pdrv->curr_dev->dev_cfg.phy_cfg.act_phy->clk_phase;
	lcd_ana_setb(HHI_TCON_PLL_CNTL1, (phase_value & 0xf), 24, 4);
	lcd_ana_setb(HHI_TCON_PLL_CNTL4, ((phase_value >> 4) & 0xf), 28, 4);
	lcd_ana_setb(HHI_TCON_PLL_CNTL4, ((phase_value >> 8) & 0xf), 24, 4);
	return 0;
}

static void lcd_set_tcon_clk_t5(struct aml_lcd_drv_s *pdrv)
{
#ifdef CONFIG_AMLOGIC_LCD_TCON
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned int freq;

	if (pdrv->status & LCD_STATUS_IF_ON)
		return;
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_MLVDS &&
	    pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_P2P)
		return;
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCDPR("lcd clk: set_tcon_clk_t5\n");

	switch (pconf->basic.lcd_type) {
	case LCD_MLVDS:
		lcd_set_mlvds_clk_phase(pdrv);

		/* tcon_clk */
		if (pconf->timing.enc_clk >= 100000000) /* 25M */
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
#endif
}

static int lcd_clk_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i = 0, n, len = 0;
	unsigned int *table = NULL, size = 0;
	unsigned int pll_reg_table[] = {
		HHI_TCON_PLL_CNTL0,
		HHI_TCON_PLL_CNTL1,
		HHI_TCON_PLL_CNTL2,
		HHI_TCON_PLL_CNTL3,
		HHI_TCON_PLL_CNTL4,
		HHI_VID_PLL_CLK_DIV
	};
	unsigned int clk_reg_table[] = {
		HHI_VIID_CLK_DIV,
		HHI_VIID_CLK_CNTL,
		HHI_VID_CLK_CNTL2,
		HHI_TCON_CLK_CNTL
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

static void lcd_prbs_config_clk(struct aml_lcd_drv_s *pdrv, unsigned int lcd_prbs_mode,
		unsigned int *encl_clk, unsigned int *fifo_clk)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned long long bit_rate = 0;

	if (!cconf)
		return;
	LCD_DBG_ADV2(pdrv, "%s", __func__);

	if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
		bit_rate = 2970000000ULL;
	} else if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
		bit_rate = 550000000ULL;
	} else {
		LCDERR("%s: unsupport lcd_prbs_mode %d\n", __func__, lcd_prbs_mode);
		return;
	}

	*encl_clk = lcd_do_div(bit_rate, 5);
	*fifo_clk = lcd_do_div(bit_rate, 10);
	lcd_clk_generate_prbs_clk(pdrv, *encl_clk, bit_rate);
	if (cconf->pll_config[0].done == 0)
		return;

	lcd_clk_set(pdrv);
	lcd_set_vclk_crt(pdrv);

	LCD_DBG_ADV2(pdrv, "%s ok", __func__);
}

static void lcd_clk_prbs_test(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned int lcd_prbs_mode, lcd_prbs_cnt;
	unsigned int reg0, reg1;
	unsigned int val1, val2, timeout;
	unsigned int clk_err_cnt = 0;
	unsigned int lcd_encl_clk_check_std = 0, lcd_fifo_clk_check_std = 0;
	int i, j, ret;

	if (!cconf)
		return;

	reg0 = HHI_LVDS_TX_PHY_CNTL0;
	reg1 = HHI_LVDS_TX_PHY_CNTL1;

	ms = (ms > 180000) ? 180000 : ms;
	timeout = ms / 5;

	for (i = 0; i < LCD_PRBS_MODE_MAX; i++) {
		if ((mode_flag & (1 << i)) == 0)
			continue;

		lcd_ana_write(reg0, 0);
		lcd_ana_write(reg1, 0);

		lcd_prbs_cnt = 0;
		clk_err_cnt = 0;
		lcd_prbs_mode = (1 << i);
		lcd_prbs_config_clk(pdrv, lcd_prbs_mode, &lcd_encl_clk_check_std,
				&lcd_fifo_clk_check_std);
		lcd_delay_ms(20);

		lcd_ana_write(reg0, 0x000000c0);
		lcd_ana_setb(reg0, 0xfff, 16, 12);
		lcd_ana_setb(reg0, 1, 2, 1);
		lcd_ana_write(reg1, 0x41000000);
		lcd_ana_setb(reg1, 1, 31, 1);

		lcd_ana_write(reg0, 0xfff20c4);
		lcd_ana_setb(reg0, 1, 12, 1);
		val1 = lcd_ana_getb(reg1, 12, 12);

		while (lcd_prbs_flag) {
			if (ms > 1) { /* when s=1, means always run */
				if (lcd_prbs_cnt++ >= timeout)
					break;
			}
			usleep_range(5000, 5001);
			ret = 1;
			for (j = 0; j < 5; j++) {
				val2 = lcd_ana_getb(reg1, 12, 12);
				if (val2 != val1) {
					ret = 0;
					break;
				}
			}
			if (ret) {
				LCDERR("prbs check error 1, val:0x%03x, cnt:%d\n",
				       val2, lcd_prbs_cnt);
				goto lcd_prbs_test_err;
			}
			val1 = val2;
			if (lcd_ana_getb(reg1, 0, 12)) {
				LCDERR("prbs check error 2, cnt:%d\n",
				       lcd_prbs_cnt);
				goto lcd_prbs_test_err;
			}

			if (lcd_prbs_clk_check(lcd_encl_clk_check_std, 9,
					       lcd_fifo_clk_check_std, 129,
					       lcd_prbs_cnt))
				clk_err_cnt++;
			else
				clk_err_cnt = 0;
			if (clk_err_cnt >= 10) {
				LCDERR("prbs check error 3(clkmsr), cnt: %d\n",
				       lcd_prbs_cnt);
				goto lcd_prbs_test_err;
			}
		}

		lcd_ana_write(reg0, 0);
		lcd_ana_write(reg1, 0);

		if (lcd_prbs_mode == LCD_PRBS_MODE_LVDS) {
			lcd_prbs_performed |= LCD_PRBS_MODE_LVDS;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_LVDS);
			LCDPR("lvds prbs check ok\n");
		} else if (lcd_prbs_mode == LCD_PRBS_MODE_VX1) {
			lcd_prbs_performed |= LCD_PRBS_MODE_VX1;
			lcd_prbs_err &= ~(LCD_PRBS_MODE_VX1);
			LCDPR("vx1 prbs check ok\n");
		} else {
			LCDPR("prbs check: unsupport mode\n");
		}
		continue;

lcd_prbs_test_err:
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

static struct lcd_pll_data_s lcd_pll_data_t5d = {
	.pll_od_fb = 0,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_div_0p5_en = 0,
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

static struct lcd_clk_data_s lcd_clk_data_t5d = {
	.pll_data[0] = &lcd_pll_data_t5d,
	.pll_data[1] = NULL,
	.xd_out_fmax = 400000000,
	.phy_clk_location = 0,

	.xd_max = 256,
	.phy_div_max = 256,

	.ss_support = 1,
	.ss_level_max = 30,
	.ss_freq_max = 6,
	.ss_mode_max = 2,
	.ss_dep_base = 500, //ppm
	.ss_dep_sel_max = 12,
	.ss_str_m_max = 10,
	.ss_freq_dep_opt = NULL,

	.vclk_sel = 0,
	.enc_clk_msr_id = 9,
	.fifo_clk_msr_id = 129,
	.tcon_clk_msr_id = 128,

	.clktree_set = lcd_set_tcon_clk_t5,
	.clktree_index = {
		CLKTREE_ENCL_TOP_GATE,
		CLKTREE_ENCL_INT_GATE,
		CLKTREE_TCON_GATE,
		CLKTREE_TCON,
		0, 0},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss = lcd_set_pll_ss,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = lcd_pll_ss_init,
	.pll_frac_set = lcd_pll_frac_set,
	.pll_m_set = lcd_pll_m_set,
	.pll_hz_get = NULL,
	.pll_reset = lcd_pll_reset,
	.clk_set = lcd_clk_set,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_set_dummy = NULL,
	.clk_disable = lcd_clk_disable,
	.mlvds_clk_phase_set = lcd_set_mlvds_clk_phase,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_dft,
	.clk_reg_print = lcd_clk_reg_dump,
	.prbs_test = lcd_clk_prbs_test,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_t5d(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

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

	cconf->pll_conf_num = 1;
	cconf->pll_config = kcalloc(cconf->pll_conf_num, sizeof(struct lcd_pll_config_s),
					GFP_KERNEL);
	if (!cconf->pll_config) {
		LCDERR("[%d]: %s: Not enough memory for pll config\n", pdrv->index, __func__);
		kfree(cconf);
		return NULL;
	}

	cconf->data = &lcd_clk_data_t5d;
	cconf->pll_config[0].pll_id = 0;

	return cconf;
}
