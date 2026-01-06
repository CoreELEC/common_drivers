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

static unsigned int fclk_div_table[][2] = {
/*  sel,  divclk */
	// {0, 666667}, // dsi pll
	{1, 666667},
	{2, 500000},
	{3, 400000},
	{6, 800000},
	{LCD_CLK_CTRL_END, LCD_CLK_CTRL_END}
};

static inline unsigned long long lcd_abs(unsigned long long a, unsigned long long b)
{
	return (a >= b) ? (a - b) : (b - a);
}

static void lcd_set_fclk_div(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int f_target;
	unsigned int max_div = 256;
	unsigned int i = 0, div = 0, min_err_sel_idx = 0, min_err_div = 1;
	unsigned int min_err = 100000, error;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	f_target = pdrv->curr_dev->dev_cfg.timing.act_timing.pixel_clk / 1000;
	if (f_target >= cconf->data->xd_out_fmax) {
		LCDERR("%s: freq(%dKHz) out of limit(%dkHz)\n", __func__,
			f_target, 75000000);
		return;
	}

	while (fclk_div_table[i][0] != LCD_CLK_CTRL_END) {
		for (div = 1; div <= max_div; div++) {
			error = lcd_abs((fclk_div_table[i][1]) / div, f_target);
			if (error < min_err) {
				min_err_sel_idx = i;
				min_err_div = div;
				min_err = error;
				if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
					LCDPR("_sel:%d, _div:%d, err:%d\n",
						fclk_div_table[i][0], min_err_div, min_err);
			}
		}
		i++;
	}

	cconf->xd = min_err_div;
	cconf->data->vclk_sel = fclk_div_table[min_err_sel_idx][0];
	cconf->fout = fclk_div_table[min_err_sel_idx][1] / min_err_div;

	LCDPR("[%d]: f_tar:%d, f_out:%d, fclk:%dkHz, div:%d, error:%d\n", pdrv->index,
		f_target, cconf->fout, fclk_div_table[min_err_sel_idx][1], min_err_div, min_err);
}

static void lcd_clk_set_c5(struct aml_lcd_drv_s *pdrv)
{
	/* gp0 used by emmc, no permission for gp1, used fix clk */
	lcd_set_fclk_div(pdrv);
}

static void lcd_set_vclk_crt_c5(struct aml_lcd_drv_s *pdrv)  /* from c3 */
{
	struct lcd_clk_config_s *cconf;

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("lcd clk: set_vclk_crt_c3\n");
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	lcd_set_fclk_div(pdrv);

	/* cts_vout_clk */
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
	int n, len = 0;

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

	return len;
}

static int lcd_clk_reg_print_c5(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	//struct lcd_clk_config_s *cconf;
	//int n, len = 0;

	//cconf = get_lcd_clk_config(pdrv);
	//if (!cconf)
	//	return -1;

	//table = pll_reg_table;
	//size = ARRAY_SIZE(pll_reg_table);
	//for (i = 0; i < size; i++) {
	//	n = lcd_debug_info_len(len + offset);
	//	len += snprintf((buf + len), n, "pll [0x%02x] = 0x%08x\n",
	//		table[i], lcd_vx1_lvds_ctrl_read(pdrv, table[i]));
	//}

	//return len;
	return 0;
}

static struct lcd_pll_data_s lcd_pll_data_c5 = {
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

static struct lcd_clk_data_s lcd_clk_data_c5 = {
	.pll_data[0] = &lcd_pll_data_c5,
	// .pll_data[0] = &lcd_pll_data_s6,
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

	.clktree_set = NULL,
	.clktree_index = {0, 0, 0, 0, 0, 0},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = NULL,
	.clk_generate_parameter = NULL,
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
