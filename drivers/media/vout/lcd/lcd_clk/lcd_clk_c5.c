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
	unsigned int pll_ctrl0;
	int ret, cnt = 0;

	LCD_DBG_ADV2(pdrv, "%s", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	pll_ctrl0 = (cconf->pll_config[0].pll_m << 0) | (cconf->pll_config[0].pll_od1_sel << 20);
set_pll_retry:
	lcd_ana_write(ANACTRL_DSIPLL_CTRL0_C5, pll_ctrl0 | 0x01010000);
	// lcd_ana_write(ANACTRL_DSIPLL_CTRL0_C5, pll_ctrl0 | 0x10010000);

	lcd_ana_write(ANACTRL_DSIPLL_CTRL1_C5, 0x11480000 | cconf->pll_config[0].pll_frac);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL2_C5, 0x1219b010);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL3_C5, 0x00008010);

	lcd_ana_write(ANACTRL_DSIPLL_CTRL1_C5, 0x19480000 | cconf->pll_config[0].pll_frac);

	lcd_ana_write(ANACTRL_DSIPLL_CTRL0_C5, pll_ctrl0 |  0x31010000);

	lcd_delay_us(10);
	lcd_ana_write(ANACTRL_DSIPLL_CTRL3_C5, 0x20008010);
	lcd_delay_us(20);

	ret = lcd_pll_wait_lock(cconf->pll_config[0].pll_id, ANACTRL_DSIPLL_CTRL0_C5, 31);
	if (ret) {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry;
		LCD_ERR(pdrv, "pll lock failed");
	}
}

static void lcd_clk_set_c5(struct aml_lcd_drv_s *pdrv)
{
	/* gp0 used by emmc, no permission for gp1, used fix clk */
	lcd_set_pll(pdrv);
	// lcd_set_fclk_div(pdrv);
}

static void lcd_set_vid_pll_div_c5(struct aml_lcd_drv_s *pdrv)
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
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_C5, 1, 18, 1);
	} else {
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_C5, 0, 18, 1);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_C5, shift_sel, 16, 2);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_C5, 1, 15, 1);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_C5, shift_val, 0, 15);
		lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_C5, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_clk_setb(CLKCTRL_DSI_PLL_CLK_DIV_C5, 1, 19, 1);
}

static void lcd_set_vclk_crt_c5(struct aml_lcd_drv_s *pdrv)  /* from c3 */
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("lcd clk: set_vclk_crt_c3\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	// lcd_set_fclk_div(pdrv);
	lcd_set_vid_pll_div_c5(pdrv);

	// /* cts_vout_clk */
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_C5, (cconf->xd - 1), 16, 7);
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_C5, cconf->data->vclk_sel, 25, 3);
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_C5, 1, 24, 1);  /* cts_vout_mclk en */
}

static void lcd_clk_disable_c5(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_C5, 0, 24, 1);
}

static int lcd_clk_config_print_c5(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0, i;
	u8 clk_ids[7] = {43, 44, 45, 46, 47, 49, 50,};
	char *clk_name[7] = {
		"vout_venc", "venc_clk_ph", "dsi_phy", "dsi_meas", "vout", "dsi_pll_div", "dsi_pll"
	};

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"lcd clk config:\n"
		"vclk_sel      %d\n"
		"xd:           %d\n"
		"fout:         %dkHz\n\n",
		cconf->data->vclk_sel,
		cconf->xd, cconf->fout);

	for (i = 0; i < 7; i++) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"%s:%u ====      %d\n",
			clk_name[i],
			clk_ids[i],
			meson_clk_measure(clk_ids[i]));
	}
	return len;
}

static int lcd_clk_reg_print_c5(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf;
	int n, len = 0, i;
	unsigned int *table = NULL, size = 0;

	unsigned int pll_reg_table[] = {
		ANACTRL_DSIPLL_CTRL0_C5,
		ANACTRL_DSIPLL_CTRL1_C5,
		ANACTRL_DSIPLL_CTRL2_C5,
		ANACTRL_DSIPLL_CTRL3_C5,
		ANACTRL_DSIPLL_STS_C5,
	};
	unsigned int clk_reg_table[] = {
		CLKCTRL_VOUTENC_CLK_CTRL_C5,
		CLKCTRL_MIPIDSI_PHY_CLK_CTRL_C5,
		CLKCTRL_DSI_MEAS_CLK_CTRL_C5,
		CLKCTRL_DSI_PLL_CLK_DIV_C5,
	};


	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

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

static struct lcd_pll_data_s lcd_pll_data_c5 = {
	.pll_od_fb = 0,
	.pll_div_0p5_en = 1,
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

static struct lcd_clk_data_s lcd_clk_data_c5 = {
	.pll_data[0] = &lcd_pll_data_c5,
	// .pll_data[0] = &lcd_pll_data_s6,
	.pll_data[1] = NULL,
	.xd_out_fmax = 200000000,
	.phy_clk_location = 1,

	// .vclk_sel = 3, //dsi_pll_clk
	.vclk_sel = 0, //dsi_pll_clk
	.enc_clk_msr_id = 43,
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

	.clktree_set = NULL,
	.clktree_index = {0, 0, 0, 0, 0, 0},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = NULL,
	.set_ss = NULL,
	.clk_ss_enable = NULL,
	.clk_ss_init = NULL,
	.pll_frac_set = NULL,
	.pll_m_set = NULL,
	.pll_hz_get = NULL,
	.pll_reset = NULL,
	.clk_set = lcd_clk_set_c5,
	.vclk_crt_set = lcd_set_vclk_crt_c5,
	.clk_disable = lcd_clk_disable_c5,
	.mlvds_clk_phase_set = NULL,
	.clk_set_dummy = NULL,
	.clk_config_init_print = NULL,
	.clk_config_print = lcd_clk_config_print_c5,
	.clk_reg_print = lcd_clk_reg_print_c5,
	.prbs_test = NULL,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_c5(struct aml_lcd_drv_s *pdrv)
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

	cconf->pll_conf_num = 1;
	cconf->pll_config = kcalloc(cconf->pll_conf_num, sizeof(struct lcd_pll_config_s),
						GFP_KERNEL);

	if (!cconf->pll_config) {
		LCDERR("[%d]: %s: Not enough memory for pll config\n", pdrv->index, __func__);
		kfree(cconf);
		return NULL;
	}

	cconf->data = &lcd_clk_data_c5;
	cconf->pll_config[0].pll_id = 0;

	return cconf;
}
