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

static void lcd_pll_ss_init(struct lcd_clk_config_s *cconf)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_pll_ss_enable(struct aml_lcd_drv_s *pdrv, int status)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_set_pll_ss(struct aml_lcd_drv_s *pdrv, unsigned int ss_flag)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_pll_frac_set(struct aml_lcd_drv_s *pdrv, unsigned int frac)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_pll_m_set(struct aml_lcd_drv_s *pdrv, unsigned int m)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_pll_reset(struct aml_lcd_drv_s *pdrv)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_set_pll_a9(struct aml_lcd_drv_s *pdrv)
{
	LCDPR("%s: TODO\n", __func__);
}

static void lcd_set_vid_pll_div_a9(struct aml_lcd_drv_s *pdrv)
{
	LCDPR("%s: TODO\n", __func__);
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
		encl_clk_sel_bit = 12;
		encl_clk_sel = 8;
		encl_gate_bit = 11;
		break;
	case 0:
	default:
		reg_vid_clk_div = CLKCTRL_VID_CLK_DIV_A9;
		reg_vid_clk_ctrl = CLKCTRL_VID_CLK_CTRL_A9;
		encl_clk_sel_bit = 8;
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

	if (lcd_debug_print_flag & LCD_DBG_PR_ADV2)
		LCDPR("[%d]: %s\n", pdrv->index, __func__);
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
	offset = cconf->pll_config[0].pll_offset;

	lcd_clk_setb(CLKCTRL_VID_CLK_CTRL2_A9, 0, 10 + cconf->pll_config[0].pll_id, 1);

	/* close vclk2_div gate: [4:0] */
	lcd_clk_setb(reg_vid_clk_ctrl, 0, 0, 5);
	lcd_clk_setb(reg_vid_clk_ctrl, 0, VCLK2_EN, 1);
}

static int lcd_clk_reg_dump_a9(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int i = 0, n, len = 0;
	unsigned int *table = NULL, size = 0;
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

	if (!pdrv || pdrv->index >= 2)
		return 0;

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

static void lcd_clk_parameter_init_a9(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);

	if (!cconf)
		return;

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_LVDS:
	case LCD_VBYONE:
		cconf->data->phy_clk_location = 0;
		break;
	case LCD_MIPI:
		cconf->data->phy_clk_location = 2;
		break;
	default:
		break;
	}
}

static struct lcd_pll_data_s lcd_pll_data_a9_0 = {
	.pll_od_fb = 0,
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
	.div_out_fmax = 1500000000U,
	.div_sel_max = CLK_DIV_SEL_2p33,
};

static struct lcd_clk_data_s lcd_clk_data_a9_0 = {
	.pll_data[0] = &lcd_pll_data_a9_0,
	.pll_data[1] = NULL,
	.xd_out_fmax = 750000000,
	.phy_clk_location = 0,

	.vclk_sel = 0,
	.enc_clk_msr_id = 53,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
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
	.clk_parameter_init = lcd_clk_parameter_init_a9,
	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss = lcd_set_pll_ss,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = lcd_pll_ss_init,
	.pll_frac_set = lcd_pll_frac_set,
	.pll_m_set = lcd_pll_m_set,
	.pll_reset = lcd_pll_reset,
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

static struct lcd_pll_data_s lcd_pll_data_a9_1 = {
	.pll_od_fb = 0,
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
	.div_out_fmax = 1500000000ULL,
	.div_sel_max = CLK_DIV_SEL_2p33,
};

static struct lcd_clk_data_s lcd_clk_data_a9_1 = {
	.pll_data[0] = &lcd_pll_data_a9_1,
	.pll_data[1] = NULL,
	.xd_out_fmax = 750000000,
	.phy_clk_location = 0,

	.vclk_sel = 0,
	.enc_clk_msr_id = 52,
	.fifo_clk_msr_id = LCD_CLK_MSR_INVALID,
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
	.clk_parameter_init = lcd_clk_parameter_init_a9,
	.clk_generate_parameter = lcd_clk_generate_dft,
	.pll_frac_generate = lcd_pll_frac_generate_dft,
	.set_ss = lcd_set_pll_ss,
	.clk_ss_enable = lcd_pll_ss_enable,
	.clk_ss_init = lcd_pll_ss_init,
	.pll_frac_set = lcd_pll_frac_set,
	.pll_m_set = lcd_pll_m_set,
	.pll_reset = lcd_pll_reset,
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

	cconf->pll_conf_num = 1;
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
		cconf->pll_config[0].pll_offset = 0;
	} else {
		cconf->data = &lcd_clk_data_a9_1;
		cconf->pll_config[0].pll_id = 1;
		cconf->pll_config[0].pll_offset = 0x5;
	}
	return cconf;
}

