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

static void lcd_set_pll(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl0;
	int ret, cnt = 0;

	LCD_DBG_ADV2(pdrv, "%s", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl0 = (cconf->pll_config[0].pll_m << 0) | (cconf->pll_config[0].pll_od1_sel << 20);
set_pll_retry:
	lcd_ana_write(ANACTRL_DSIPLL_CTRL0, pll_ctrl0 | 0x00010000);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL0, pll_ctrl0 | 0x10010000);

	lcd_ana_write(ANACTRL_DSIPLL_CTRL1, 0x11480000 | cconf->pll_config[0].pll_frac);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL2, 0x121db010);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL3, 0x00008010);

	lcd_ana_write(ANACTRL_DSIPLL_CTRL1, 0x19480000 | cconf->pll_config[0].pll_frac);

	lcd_ana_write(ANACTRL_DSIPLL_CTRL0, pll_ctrl0 |  0x30010000);

	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL3, 0x20008010);
	lcd_delay_us(20);

	ret = lcd_pll_wait_lock(cconf->pll_config[0].pll_id, ANACTRL_DSIPLL_CTRL0, 31);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry;
		LCD_ERR(pdrv, "pll lock failed");
	}
}

static void lcd_clk_set_s6(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll(pdrv);
}

static void lcd_set_vid_pll_div_s6(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;

	LCD_DBG_ADV2(pdrv, "%s", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (cconf->data->pll_data[0]->div_sel_max == CLK_DIV_SEL_1 ||
	    cconf->pll_config->div_sel > cconf->data->pll_data[0]->div_sel_max ||
	    cconf->pll_config->div_sel >= ARRAY_SIZE(lcd_clk_div_table)) {
		LCD_ERR(pdrv, "invalid clk divider");
		return;
	}

	shift_val = lcd_clk_div_table[cconf->pll_config->div_sel].shift_val;
	shift_sel = lcd_clk_div_table[cconf->pll_config->div_sel].shift_sel;

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_S6, 1, 18, 1);
	} else {
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_S6, 0, 18, 1);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_S6, shift_sel, 16, 2);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_S6, 1, 15, 1);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_S6, shift_val, 0, 15);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_S6, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_S6, 1, 19, 1);
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_clk_setb(CLKCTRL_VIID_CLK_CTRL_S6, 0, VCLK2_EN, 1);
	udelay(2);

	lcd_set_vid_pll_div_s6(pdrv);

	/* setup the XD divider value */
	lcd_clk_setb(CLKCTRL_VIID_CLK_DIV_S6, (cconf->xd - 1), VCLK2_XD, 8);
	udelay(5);

	/* select dsi_pll_clk */
	lcd_clk_setb(CLKCTRL_VIID_CLK_CTRL_S6, cconf->data->vclk_sel, VCLK2_CLK_IN_SEL, 3);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	lcd_clk_setb(CLKCTRL_VIID_CLK_DIV_S6, 8, ENCL_CLK_SEL, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	lcd_clk_setb(CLKCTRL_VIID_CLK_DIV_S6, 1, VCLK2_XD_EN, 2);
	udelay(5);

	lcd_clk_setb(CLKCTRL_VIID_CLK_CTRL_S6, 1, VCLK2_DIV1_EN, 1);
	lcd_clk_setb(CLKCTRL_VIID_CLK_CTRL_S6, 1, VCLK2_SOFT_RST, 1);
	lcd_clk_setb(CLKCTRL_VIID_CLK_CTRL_S6, 0, VCLK2_SOFT_RST, 1);
	udelay(5);

	/* enable CTS_ENCL clk gate */
	lcd_clk_setb(CLKCTRL_VID_CLK_CTRL2_S6, 1, ENCL_GATE_VCLK, 1);

	lcd_clk_setb(CLKCTRL_VIID_CLK_CTRL_S6, 1, VCLK2_EN, 1);
	udelay(2);
}

static void lcd_clk_disable_s6(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VIID_CLK_DIV_S6, 0, VCLK2_EN, 1);
	lcd_clk_setb(CLKCTRL_VID_CLK_CTRL2_S6, 0, ENCL_GATE_VCLK, 1);

	lcd_ana_write(ANACTRL_DSIPLL_CTRL0, 0x0);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL1, 0x0);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL2, 0x0);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL3, 0x0);
}

// ANACTRL_DSIPLL_CTRL3
//  [8]    : dsi_pll_ssc_en
//  [13:10]: dsi_pll_ssc_str_m
//  [19:16]: dsi_pll_ssc_dep_sel
//  [23:22]: dsi_pll_ss_mode
static void lcd_pll_ss_enable(struct aml_lcd_drv_s *pdrv, int status)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl3;
	unsigned int flag = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl3 = lcd_ana_read(ANACTRL_DSIPLL_CTRL3);
	pll_ctrl3 &= ~((1 << 8) | (0xf << 10) | (0xf << 16) | (0x3 << 22));

	if (status && cconf->ss_level > 0)
		flag = 1;

	if (flag) {
		cconf->ss_en = 1;
		pll_ctrl3 |= ((1 << 8) | (cconf->ss_str_m << 10) |
			      (cconf->ss_dep_sel << 16) | (cconf->ss_mode < 22));
		LCD_PR(pdrv, "pll ss enable: level: %d, %dppm", cconf->ss_level, cconf->ss_ppm);
	} else {
		cconf->ss_en = 0;
		LCD_PR(pdrv, "pll ss disable");
	}
	lcd_ana_write(ANACTRL_DSIPLL_CTRL3, pll_ctrl3);
}

static int lcd_clk_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i = 0, n, len = 0;
	unsigned int *table = NULL, size = 0;
	unsigned int pll_reg_table[] = {
		ANACTRL_DSIPLL_CTRL0,
		ANACTRL_DSIPLL_CTRL1,
		ANACTRL_DSIPLL_CTRL2,
		ANACTRL_DSIPLL_CTRL3,
		ANACTRL_DSIPLL_STS
	};
	unsigned int clk_reg_table[] = {
		CLKCTRL_DSI_PLL_CLK_DIV_S6,
		CLKCTRL_VID_CLK_CTRL2_S6,
		CLKCTRL_VIID_CLK_DIV_S6,
		CLKCTRL_VIID_CLK_CTRL_S6
	};

	static const char * const key_clk_msr_name[] = {
		"dsi_pll_div_clk_out", "dsi_pll_clk", "cts_encl", "cts_dsi_meas", "cts_dsi_phy"};
	unsigned int key_clk_msr_id[] = {29, 30, 53, 70, 71};

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

	table = key_clk_msr_id;
	size = ARRAY_SIZE(key_clk_msr_id);
	for (i = 0; i < size; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%-*s = %u MHz\n", 20,
			key_clk_msr_name[i], meson_clk_measure(key_clk_msr_id[i]));
	}
	return len;
}

static void lcd_prbs_test_s6(struct aml_lcd_drv_s *pdrv, unsigned int ms,
			    unsigned int mode_flag)
{
	struct lcd_clk_config_s *cconf;
	unsigned int val, timeout;
	unsigned int cnt = 0;
	unsigned int clk_err_cnt = 0;
	unsigned long lcd_encl_clk_check_std, lcd_byte_clk_check_std;
	int j, ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->pll_config[0].pll_m = 0x68; //2496MHz
	cconf->pll_config[0].pll_od1_sel = 0; //analog div 0
	cconf->pll_config[0].pll_frac = 0;
	cconf->pll_config->div_sel = CLK_DIV_SEL_3;
	cconf->xd = 2;
	lcd_encl_clk_check_std = 416000000;
	lcd_byte_clk_check_std = 312000000;

	cconf->data->vclk_crt_set(pdrv);
	cconf->data->clk_set(pdrv);

	lcd_ana_write(ANACTRL_MIPIDSI_CTRL0_S6, 0x393b2c55);
	lcd_ana_write(ANACTRL_MIPIDSI_CTRL1_S6, 0xc134061f);
	lcd_delay_us(20);
	lcd_ana_write(ANACTRL_MIPIDSI_CTRL1_S6, 0xe134061f);
	lcd_delay_us(20);
	lcd_ana_write(ANACTRL_MIPIDSI_CTRL0_S6, 0x393bcc55);

	timeout = (ms > 1000) ? 1000 : ms;

	lcd_delay_us(1000);

	while (cnt++ < timeout) {
		lcd_delay_us(10);
		ret = 1;
		for (j = 0; j < 20; j++) {
			lcd_delay_us(5);
			val = dsi_phy_getb(pdrv, 0, MIPI_DSI_TEST_CTRL0, 28, 4);
			if (val == 0x0f) {
				ret = 0;
				break;
			}
		}
		if (ret) {
			LCD_ERR(pdrv, "prbs check error, val:0x%x, cnt:%d", val, cnt);
			break;
		}

		if (lcd_prbs_clk_check(lcd_encl_clk_check_std, cconf->data->enc_clk_msr_id,
				       lcd_byte_clk_check_std, cconf->data->fifo_clk_msr_id, cnt))
			clk_err_cnt++;
		else
			clk_err_cnt = 0;
		if (clk_err_cnt >= 10) {
			LCD_ERR(pdrv, "prbs check error (clkmsr), cnt:%d", cnt);
			ret = 1;
			break;
		}
	}

	LCD_PR(pdrv, "[lcd prbs result]:");
	LCD_PR(pdrv, "MIPI-DSI prbs performed: 1, error: %d", ret);
}

static struct lcd_pll_data_s lcd_pll_data_s6 = {
	.pll_od_fb = 0,
	.pll_0_5_div_en = 0,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_od_sel_max = 5,
	.pll_ref_fmax = 24000000,
	.pll_ref_fmin = 24000000,
	.pll_vco_fmax = 2800000000ULL,
	.pll_vco_fmin = 1400000000ULL,
	.pll_out_fmax = 2800000000ULL,
	.pll_out_fmin = 1400000000ULL,
	.od_cnt = 0,
	.have_tcon_div = 0,
	.div_in_fmax = 2800000000ULL,
	.div_out_fmax = 2800000000U,
	.div_sel_max = CLK_DIV_SEL_15,
};

static struct lcd_clk_data_s lcd_clk_data_s6 = {
	.pll_data[0] = &lcd_pll_data_s6,
	.pll_data[1] = NULL,
	.xd_out_fmax = 666000000,
	.phy_clk_location = 1,

	.vclk_sel = 3, //dsi_pll_clk
	.enc_clk_msr_id = 53,
	.fifo_clk_msr_id = 71,
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
	.ss_freq_dep_opt = NULL,

	.clktree_set = NULL,
	.clktree_index = {0, 0, 0, 0, 0, 0},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = NULL,
	.set_ss = NULL,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = NULL,
	.pll_frac_set = NULL,
	.pll_m_set = NULL,
	.pll_hz_get = NULL,
	.pll_reset = NULL,
	.clk_set = lcd_clk_set_s6,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_set_dummy = NULL,
	.clk_disable = lcd_clk_disable_s6,
	.mlvds_clk_phase_set = NULL,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = NULL,
	.clk_reg_print = lcd_clk_reg_dump,
	.prbs_test = lcd_prbs_test_s6,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_s6(struct aml_lcd_drv_s *pdrv)
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

	cconf->data = &lcd_clk_data_s6;
	cconf->pll_config[0].pll_id = 0;

	return cconf;
}
