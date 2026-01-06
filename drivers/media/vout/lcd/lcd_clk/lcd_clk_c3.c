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

static void lcd_set_pll(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int pll_ctrl, pll_ctrl1;
	int ret, cnt = 0;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl = ((cconf->pll_config[0].pll_od1_sel << 16) |
		(cconf->pll_config[0].pll_n << 10) |
		(cconf->pll_config[0].pll_m << 0));
	pll_ctrl1 = cconf->pll_config[0].pll_frac;

set_pll_retry_c3:
	lcd_ana_write(ANACTRL_GP0PLL_CTRL0, pll_ctrl);
	lcd_ana_write(ANACTRL_GP0PLL_CTRL1, pll_ctrl1);
	lcd_ana_write(ANACTRL_GP0PLL_CTRL2, 0x00023041);
	lcd_ana_write(ANACTRL_GP0PLL_CTRL3, 0x48681c00);
	lcd_ana_write(ANACTRL_GP0PLL_CTRL4, 0x88770290);
	lcd_ana_write(ANACTRL_GP0PLL_CTRL5, 0x3927200a);
	lcd_ana_setb(ANACTRL_GP0PLL_CTRL0, 3, 28, 2);
	lcd_ana_setb(ANACTRL_GP0PLL_CTRL0, 0, 29, 1);
	lcd_delay_us(20);
	lcd_ana_write(ANACTRL_GP0PLL_CTRL2, 0x00023001);
	lcd_delay_us(20);

	ret = lcd_pll_wait_lock(cconf->pll_config[0].pll_id, ANACTRL_GP0PLL_STS, 31);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_c3;
		LCDERR("[%d]: pll lock failed\n", pdrv->index);
	}
}

static void lcd_set_vclk_crt(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("lcd clk: set_vclk_crt_c3\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	if (lcd_pxp_mode) { //143M
		lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, 1, 16, 7);
		lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, 7, 25, 3);
	} else {
		lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, (cconf->xd - 1), 16, 7);
		lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, cconf->data->vclk_sel, 25, 3);
	}
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, 1, 24, 1);
}

static void lcd_set_dsi_phy_clk(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, (cconf->phy_div - 1), 0, 7);
	lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 1, 12, 3);
	lcd_clk_setb(CLKCTRL_MIPIDSI_PHY_CLK_CTRL, 1, 8, 1);
}

static void lcd_clk_set(struct aml_lcd_drv_s *pdrv)
{
	lcd_set_pll(pdrv);
	if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MIPI)
		lcd_set_dsi_phy_clk(pdrv);
}

static void lcd_clk_disable(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL, 0, 24, 1);

	lcd_ana_setb(ANACTRL_GP0PLL_CTRL0, 0, 28, 1); //disable
	lcd_ana_setb(ANACTRL_GP0PLL_CTRL0, 1, 29, 1); //reset
}

static void lcd_pll_frac_generate_c3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned long long pll_fout, pll_fvco;
	unsigned int enc_clk, od;
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	enc_clk = pconf->timing.enc_clk;
	od = od_table[cconf->pll_config[0].pll_od1_sel];
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2) {
		LCDPR("m=%d, n=%d, od=%d, xd=%d\n",
			cconf->pll_config[0].pll_m, cconf->pll_config[0].pll_n,
			cconf->pll_config[0].pll_od1_sel, cconf->xd);
	}
	if (enc_clk > cconf->data->xd_out_fmax) {
		LCDERR("%s: wrong lcd_clk value %dHz\n", __func__, enc_clk);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pclk=%d\n", __func__, enc_clk);

	pll_fout = enc_clk;
	pll_fout *= cconf->xd;
	if (pll_fout > cconf->data->pll_data[0]->pll_out_fmax ||
	    pll_fout < cconf->data->pll_data[0]->pll_out_fmin) {
		LCDERR("%s: wrong pll_fout value %lldHz\n", __func__, pll_fout);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pll_fout=%lld\n", __func__, pll_fout);

	pll_fvco = pll_fout * od;
	if (pll_fvco < cconf->data->pll_data[0]->pll_vco_fmin ||
	    pll_fvco > cconf->data->pll_data[0]->pll_vco_fmax) {
		LCDERR("%s: wrong pll_fvco value %lldHz\n", __func__, pll_fvco);
		return;
	}
	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("%s pll_fvco=%lld\n", __func__, pll_fvco);

	ret = lcd_pll_get_frac(cconf, 0, pll_fvco);
	if (ret == 0) {
		cconf->fout = enc_clk;
		cconf->pll_config[0].pll_fout = pll_fout;
		cconf->pll_config[0].pll_fvco = pll_fvco;
		pconf->timing.clk_ctrl &= ~(0x1ffffff);
		pconf->timing.clk_ctrl |=
			(cconf->pll_config[0].pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_config[0].pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
	}
}

static int lcd_clk_config_print_c3(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lcd clk config:\n"
		"vclk_sel      %d\n"
		"pll_m:        %d\n"
		"pll_n:        %d\n"
		"pll_frac:     0x%x\n"
		"pll_fvco:     %lldHz\n"
		"pll_od:       %d\n"
		"pll_out:      %lldHz\n"
		"xd:           %d\n"
		"phy_div:      %d\n"
		"fout:         %dHz\n\n",
		cconf->data->vclk_sel,
		cconf->pll_config[0].pll_m, cconf->pll_config[0].pll_n,
		cconf->pll_config[0].pll_frac, cconf->pll_config[0].pll_fvco,
		cconf->pll_config[0].pll_od1_sel, cconf->pll_config[0].pll_fout,
		cconf->xd, cconf->phy_div, cconf->fout);

	return len;
}

static int lcd_clk_reg_dump(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i = 0, n, len = 0;
	unsigned int *table = NULL, size = 0;
	unsigned int pll_reg_table[] = {
		ANACTRL_GP0PLL_CTRL0,
		ANACTRL_GP0PLL_CTRL1,
		ANACTRL_GP0PLL_CTRL2,
		ANACTRL_GP0PLL_CTRL3,
		ANACTRL_GP0PLL_CTRL4,
		ANACTRL_GP0PLL_CTRL5,
		ANACTRL_GP0PLL_CTRL6,
		ANACTRL_GP0PLL_STS
	};
	unsigned int clk_reg_table[] = {
		CLKCTRL_VOUTENC_CLK_CTRL,
		CLKCTRL_MIPIDSI_PHY_CLK_CTRL
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

static struct lcd_pll_data_s lcd_pll_data_c3 = {
	.pll_od_fb = 0,
	.pll_div_0p5_en = 0,
	.pll_m_max = 511,
	.pll_m_min = 2,
	.pll_n_max = 1,
	.pll_n_min = 1,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_od_sel_max = 5,
	.pll_ref_fmax = 25000000,
	.pll_ref_fmin = 5000000,
	.pll_vco_fmax = 6000000000ULL,
	.pll_vco_fmin = 3000000000ULL,
	.pll_out_fmax = 6000000000ULL,
	.pll_out_fmin = 375000000,
	.od_cnt = 1,
	.have_tcon_div = 0,
	.div_in_fmax = 1600000000,
	.div_out_fmax = 1600000000,
	.div_sel_max = CLK_DIV_SEL_1,
};

static struct lcd_clk_data_s lcd_clk_data_c3 = {
	.pll_data[0] = &lcd_pll_data_c3,
	.pll_data[1] = NULL,
	.xd_out_fmax = 200000000,
	.phy_clk_location = 1,

	.xd_max = 128,
	.phy_div_max = 128,

	.ss_support = 0,

	.vclk_sel = 4, //gp0_pll
	.enc_clk_msr_id = LCD_CLK_MSR_INVALID,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,

	.clktree_set = NULL,
	.clktree_index = {0, 0, 0, 0, 0, 0},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_c3,
	.set_ss = NULL,
	.clk_ss_enable = NULL,
	.clk_ss_init = NULL,
	.pll_frac_set = NULL,
	.pll_m_set = NULL,
	.pll_hz_get = NULL,
	.pll_reset = NULL,
	.clk_set = lcd_clk_set,
	.vclk_crt_set = lcd_set_vclk_crt,
	.clk_set_dummy = NULL,
	.clk_disable = lcd_clk_disable,
	.mlvds_clk_phase_set = NULL,
	.clk_config_init_print = lcd_clk_config_init_print_dft,
	.clk_config_print = lcd_clk_config_print_c3,
	.clk_reg_print = lcd_clk_reg_dump,
	.prbs_test = NULL,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_c3(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = NULL;

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
	cconf->pll_conf_num = 1;
	cconf->pll_config = kcalloc(cconf->pll_conf_num, sizeof(struct lcd_pll_config_s),
					GFP_KERNEL);
	if (!cconf->pll_config) {
		LCDERR("[%d]: %s: Not enough memory for pll config\n", pdrv->index, __func__);
		kfree(cconf);
		return NULL;
	}
	cconf->data = &lcd_clk_data_c3;
	cconf->pll_config[0].pll_id = 0;

	return cconf;
}
