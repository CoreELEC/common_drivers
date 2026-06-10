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
	{1, 666667},
	{2, 500000},
	{3, 400000},
	{6, 800000},
	{7, 285714},
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

	LCD_DBG_ADV(pdrv, "%s", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	f_target = pdrv->curr_dev->dev_cfg.timing.lcd_clk / 1000;
	if (f_target >= cconf->data->xd_out_fmax) {
		LCD_ERR(pdrv, "%s: freq(%dKHz) out of limit(75MHz)", __func__, f_target);
		return;
	}

	while (fclk_div_table[i][0] != LCD_CLK_CTRL_END)	{
		for (div = 1; div <= max_div; div++) {
			error = lcd_abs((fclk_div_table[i][1]) / div, f_target);
			if (error < min_err) {
				min_err_sel_idx = i;
				min_err_div = div;
				min_err = error;
				LCD_DBG_ADV2(pdrv, "_sel:%d, _div:%d, err:%d",
					fclk_div_table[i][0], min_err_div, min_err);
			}
		}
		i++;
	}

	cconf->xd = min_err_div;
	cconf->data->vclk_sel = fclk_div_table[min_err_sel_idx][0];
	cconf->fout = fclk_div_table[min_err_sel_idx][1] / min_err_div;

	LCD_PR(pdrv, "f_tar:%d, f_out:%d, fclk:%dkHz, div:%d, error:%d",
		f_target, cconf->fout, fclk_div_table[min_err_sel_idx][1], min_err_div, min_err);
}

static void lcd_clk_set_a4(struct aml_lcd_drv_s *pdrv)
{
	/* gp0 used by emmc, no permission for gp1, used fix clk */
	lcd_set_fclk_div(pdrv);
}

static void lcd_set_vclk_crt_a4(struct aml_lcd_drv_s *pdrv)  /* from c3 */
{
	struct lcd_clk_config_s *cconf;
	unsigned int clk_phase;

	LCD_DBG_ADV2(pdrv, "%s ent", __func__);
	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	/* phase */
	clk_phase = pdrv->curr_dev->dev_cfg.control.rgb_cfg.clk_pol;
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_A4, clk_phase, 28, 2);

	/* cts_vout_clk */
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_A4, (cconf->xd - 1), 16, 7);
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_A4, cconf->data->vclk_sel, 25, 3);
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_A4, 1, 24, 1);  /* cts_vout_mclk en */
}

static void lcd_clk_disable_a4(struct aml_lcd_drv_s *pdrv)
{
	lcd_clk_setb(CLKCTRL_VOUTENC_CLK_CTRL_A4, 0, 24, 1);
}

static int lcd_clk_config_print_a4(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
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

static struct lcd_clk_data_s lcd_clk_data_a4 = {
	.pll_data[0] = NULL,
	.pll_data[1] = NULL,
	.xd_out_fmax = 75000000,
	.phy_clk_location = 0,

	.xd_max = 0,
	.phy_div_max = 0,

	.ss_support = 0,

	.vclk_sel = 0xff, //unassigned
	.enc_clk_msr_id = LCD_CLK_MSR_INVALID,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
	.tcon_clk_msr_id = LCD_CLK_MSR_INVALID,

	.clktree_set = NULL,
	.clktree_index = {0, 0, 0, 0, 0, 0},

	.clk_parameter_init = NULL,
	.clk_generate_parameter = NULL,
	.pll_frac_generate = NULL,
	.set_ss = NULL,
	.clk_ss_enable = NULL,
	.clk_ss_init = NULL,
	.pll_frac_set = NULL,
	.pll_m_set = NULL,
	.pll_hz_get = NULL,
	.pll_reset = NULL,
	.clk_set = lcd_clk_set_a4,
	.vclk_crt_set = lcd_set_vclk_crt_a4,
	.clk_set_dummy = NULL,
	.clk_disable = lcd_clk_disable_a4,
	.mlvds_clk_phase_set = NULL,
	.clk_config_init_print = NULL,
	.clk_config_print = lcd_clk_config_print_a4,
	.prbs_test = NULL,
};

struct lcd_clk_config_s *lcd_clk_config_chip_init_a4(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = NULL;

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
	cconf->data = &lcd_clk_data_a4;

	return cconf;
}
