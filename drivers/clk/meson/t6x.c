// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/init.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/clkdev.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include "clk-pll.h"
#include "clk-regmap.h"
#include "clk-dualdiv.h"
#include <dt-bindings/clock/t6x-clkc.h>

/* basic clk: 0xfe000000 */
#define CLKCTRL_OSCIN_CTRL			(0x01 << 2)
#define CLKCTRL_RTC_BY_OSCIN_CTRL0		(0x02 << 2)
#define CLKCTRL_RTC_BY_OSCIN_CTRL1		(0x03 << 2)
#define CLKCTRL_RTC_CTRL			(0x04 << 2)
#define CLKCTRL_SYS_CLK_CTRL0			(0x10 << 2)
#define CLKCTRL_SYS_CLK_EN0_REG0		(0x11 << 2)
#define CLKCTRL_SYS_CLK_EN0_REG1		(0x12 << 2)
#define CLKCTRL_SYS_CLK_VPU_EN0			(0x19 << 2)
#define CLKCTRL_SYS_CLK_VPU_EN1			(0x1a << 2)
#define CLKCTRL_AXI_CLK_CTRL0			(0x1b << 2)
#define CLKCTRL_TST_CTRL0			(0x20 << 2)
#define CLKCTRL_TST_CTRL1			(0x21 << 2)
#define CLKCTRL_CECB_CTRL0			(0x24 << 2)
#define CLKCTRL_CECB_CTRL1			(0x25 << 2)
#define CLKCTRL_SC_CLK_CTRL			(0x26 << 2)
#define CLKCTRL_CLK12_24_CTRL			(0x2a << 2)
#define CLKCTRL_VID_CLK0_CTRL			(0x30 << 2)
#define CLKCTRL_VID_CLK0_CTRL2			(0x31 << 2)
#define CLKCTRL_VID_CLK0_DIV			(0x32 << 2)
#define CLKCTRL_VIID_CLK0_DIV			(0x33 << 2)
#define CLKCTRL_VIID_CLK0_CTRL			(0x34 << 2)
#define CLKCTRL_ENC0_HDMI_CLK_CTRL		(0x35 << 2)
#define CLKCTRL_HDMI_CLK_CTRL			(0x38 << 2)
#define CLKCTRL_VID_PLL_CLK0_DIV		(0x39 << 2)
#define CLKCTRL_VPU_CLK_CTRL			(0x3a << 2)
#define CLKCTRL_VID_LOCK_CLK_CTRL		(0x3d << 2)
#define CLKCTRL_VDIN_MEAS_CLK_CTRL		(0x3e << 2)
#define CLKCTRL_VAPBCLK_CTRL			(0x3f << 2)
#define CLKCTRL_AMFC_CLK_CTRL			(0x40 << 2)
#define CLKCTRL_CDAC_CLK_CTRL			(0x42 << 2)
#define CLKCTRL_DSC_CLK_CTRL			(0x43 << 2)
#define CLKCTRL_HTX_CLK_CTRL0			(0x47 << 2)
#define CLKCTRL_HTX_CLK_CTRL1			(0x48 << 2)
#define CLKCTRL_HRX_CLK_CTRL0			(0x4a << 2)
#define CLKCTRL_HRX_CLK_CTRL1			(0x4b << 2)
#define CLKCTRL_HRX_CLK_CTRL2			(0x4c << 2)
#define CLKCTRL_HRX_CLK_CTRL3			(0x4d << 2)
#define CLKCTRL_VDEC2_CLK_CTRL			(0x51 << 2)
#define CLKCTRL_VDEC4_CLK_CTRL			(0x53 << 2)
#define CLKCTRL_TS_CLK_CTRL			(0x56 << 2)
#define CLKCTRL_MALI_CLK_CTRL			(0x57 << 2)
#define CLKCTRL_ETH_CLK_CTRL			(0x59 << 2)
#define CLKCTRL_NAND_CLK_CTRL			(0x5a << 2)
#define CLKCTRL_SPISG_CLK_CTRL			(0x5b << 2)
#define CLKCTRL_SPISG_CLK_CTRL1			(0x5c << 2)
#define CLKCTRL_SPISG_CLK_CTRL2			(0x5d << 2)
#define CLKCTRL_GEN_CLK_CTRL			(0x5e << 2)
#define CLKCTRL_SAR_CLK_CTRL0			(0x5f << 2)
#define CLKCTRL_PWM_CLK_AB_CTRL			(0x60 << 2)
#define CLKCTRL_PWM_CLK_CD_CTRL			(0x61 << 2)
#define CLKCTRL_PWM_CLK_EF_CTRL			(0x62 << 2)
#define CLKCTRL_PWM_CLK_GH_CTRL			(0x63 << 2)
#define CLKCTRL_PWM_CLK_IJ_CTRL			(0x64 << 2)
#define CLKCTRL_CMPR_CLK_CTRL			(0x70 << 2)
#define CLKCTRL_MDCSYS_CLK_CNTL			(0x71 << 2)
#define CLKCTRL_VID_PLL_CLK1_DIV		(0x7d << 2)
#define CLKCTRL_VID_PLL_CLK2_DIV		(0x7e << 2)
#define CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL		(0x80 << 2)
#define CLKCTRL_HDMI_VID_PLL_CLK_DIV		(0x81 << 2)
#define CLKCTRL_DEMOD_CLK_CNTL			(0x82 << 2)
#define CLKCTRL_DEMOD_CLK_CNTL1			(0x83 << 2)
#define CLKCTRL_DEMOD_32K_CNTL0			(0x84 << 2)
#define CLKCTRL_DEMOD_32K_CNTL1			(0x85 << 2)
#define CLKCTRL_ATV_DMD_SYS_CLK_CNTL		(0x86 << 2)
#define CLKCTRL_TCON_CLK_CNTL			(0x87 << 2)
#define CLKCTRL_BCON_CLK_CNTL			(0x88 << 2)
#define CLKCTRL_DAE_CLK_CNTL			(0x89 << 2)
#define CLKCTRL_DPE_CLK_CNTL			(0x8a << 2)
#define CLKCTRL_USB_CLK_CNTL			(0x8b << 2)
#define CLKCTRL_TSIN_CLK_CNTL			(0x8c << 2)
#define CLKCTRL_CLK81_GATE_CTRL_SYSWRAPPER	(0x8d << 2)
#define CLKCTRL_CLK81_GATE_CTRL_PERIPH		(0x8e << 2)
#define CLKCTRL_USB_CLK_CTRL0			(0x8f << 2)
#define CLKCTRL_USB_CLK_CTRL1			(0x90 << 2)
/* ANA_CTRL - Registers
 * REG_BASE:  REGISTER_BASE_ADDR = 0xfe008000
 */
#define ANACTRL_FIXPLL_CTRL0			(0x10 << 2)
#define ANACTRL_FIXPLL_CTRL1			(0x11 << 2)
#define ANACTRL_GP0PLL_CTRL0			(0x20 << 2)
#define ANACTRL_GP0PLL_CTRL1			(0x21 << 2)
#define ANACTRL_GP0PLL_CTRL2			(0x22 << 2)
#define ANACTRL_GP0PLL_CTRL3			(0x23 << 2)
#define ANACTRL_GP0PLL_STS			(0x27 << 2)
#define ANACTRL_GP1PLL_CTRL0			(0x30 << 2)
#define ANACTRL_GP1PLL_CTRL1			(0x31 << 2)
#define ANACTRL_GP1PLL_CTRL2			(0x32 << 2)
#define ANACTRL_GP1PLL_CTRL3			(0x33 << 2)
#define ANACTRL_GP1PLL_STS			(0x37 << 2)
#define ANACTRL_GP2PLL_CTRL0			(0x40 << 2)
#define ANACTRL_GP2PLL_CTRL1			(0x41 << 2)
#define ANACTRL_GP2PLL_CTRL2			(0x42 << 2)
#define ANACTRL_GP2PLL_CTRL3			(0x43 << 2)
#define ANACTRL_HIFIPLL_CTRL0			(0x50 << 2)
#define ANACTRL_HIFIPLL_CTRL1			(0x51 << 2)
#define ANACTRL_HIFIPLL_CTRL2			(0x52 << 2)
#define ANACTRL_HIFIPLL_CTRL3			(0x53 << 2)
#define ANACTRL_HIFIPLL_STS			(0x57 << 2)
#define ANACTRL_HIFI1PLL_CTRL0			(0x60 << 2)
#define ANACTRL_HIFI1PLL_CTRL1			(0x61 << 2)
#define ANACTRL_HIFI1PLL_CTRL2			(0x62 << 2)
#define ANACTRL_HIFI1PLL_CTRL3			(0x63 << 2)
#define ANACTRL_HIFI1PLL_STS			(0x67 << 2)

static const struct pll_params_table t6x_fix_pll_params_table[] = {
	PLL_PARAMS_OD(250, 3, 0), /*DCO=2000M OD=0M*/
	{ /* sentinel */ }
};

static struct clk_regmap t6x_fixed_pll = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.od = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
		.rst = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.table = t6x_fix_pll_params_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fixed_pll",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_fclk_div2_div = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_fclk_div2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div2_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_fclk_div3_div = {
	.mult = 1,
	.div = 3,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_fclk_div3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 26,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div3",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div3_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_fclk_div4_div = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_fclk_div4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 27,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div4",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div4_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_fclk_div5_div = {
	.mult = 1,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_fclk_div5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 28,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div5_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_fclk_div7_div = {
	.mult = 1,
	.div = 7,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_fixed_pll.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_fclk_div7 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div7",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div7_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_fclk_div2p5_div = {
	.mult = 2,
	.div = 5,
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_fclk_div2p5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL0,
		.bit_idx = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "fclk_div2p5",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div2p5_div.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_mpll_50m_div = {
	.mult = 1,
	.div = 40,
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fixed_pll.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_mpll_50m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = ANACTRL_FIXPLL_CTRL1,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mpll_50m",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_mpll_50m_div.hw
		},
		.num_parents = 1,
	},
};

static const struct pll_params_table t6x_gp0_pll_table[] = {
	PLL_PARAMS_OD(192, 1, 1), /* DCO = 2304M OD = 1 PLL = 1152M */
	{ /* sentinel */  }
};

static const struct pll_params_table t6x_gp1_pll_table[] = {
	PLL_PARAMS_OD(70, 1, 0), /* DCO = 1680M OD = 0 PLL = 1680M */
	PLL_PARAMS_OD(74, 1, 0), /* DCO = 1776M OD = 0 PLL = 1776M */
	{ /* sentinel */  }
};

static const struct pll_params_table t6x_gp2_pll_table[] = {
	PLL_PARAMS_OD(134, 1, 1), /* DCO = 1608M OD = 1 PLL = 804M */
	PLL_PARAMS_OD(136, 1, 1), /* DCO = 1632M OD = 1 PLL = 816M */
	PLL_PARAMS_OD(140, 1, 1), /* DCO = 1680M OD = 1 PLL = 840M */
	{ /* sentinel */  }
};

static const struct reg_sequence t6x_gp0_init_regs[] = {
	{ .reg = ANACTRL_GP0PLL_CTRL1,	.def = 0x03a00000 }, /* frac = 0 as default */
	{ .reg = ANACTRL_GP0PLL_CTRL2,	.def = 0x00040000 },
	{ .reg = ANACTRL_GP0PLL_CTRL3,	.def = 0x0f0da000 },
};

static const struct reg_sequence t6x_gp1_init_regs[] = {
	{ .reg = ANACTRL_GP1PLL_CTRL1,	.def = 0x3420500f },
	{ .reg = ANACTRL_GP1PLL_CTRL2,	.def = 0x00023041 },
	{ .reg = ANACTRL_GP1PLL_CTRL3,	.def = 0x00000000 },
};

static const struct reg_sequence t6x_gp2_init_regs[] = {
	{ .reg = ANACTRL_GP2PLL_CTRL1,	.def = 0x03a00000 },/* frac = 0 as default */
	{ .reg = ANACTRL_GP2PLL_CTRL2,	.def = 0x00040000 },
	{ .reg = ANACTRL_GP2PLL_CTRL3,	.def = 0x0f0da000 },
};

static struct clk_regmap t6x_gp0_pll = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
		/* analog support, and emmc not need it */
		//.frac = {
		//	.reg_off = ANACTRL_GP0PLL_CTRL2,
		//	.shift   = 0,
		//	.width   = 19,
		//},
		.l = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_GP0PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_GP0PLL_CTRL3,
			.shift   = 9,
			.width   = 1,
		},
		.table = t6x_gp0_pll_table,
		.init_regs = t6x_gp0_init_regs,
		.init_count = ARRAY_SIZE(t6x_gp0_init_regs),
		.flags = CLK_MESON_PLL_FIXED_EN0P5 | CLK_MESON_PLL_NOINIT_ENABLED
			| CLK_MESON_PLL_L_RSTN,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp0_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_gp1_pll = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.od = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift	 = 12,
			.width	 = 3,
		},
		.l = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_GP1PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_GP1PLL_CTRL2,
			.shift   = 6,
			.width   = 1,
		},
		.table = t6x_gp1_pll_table,
		.init_regs = t6x_gp1_init_regs,
		.init_count = ARRAY_SIZE(t6x_gp1_init_regs),
		.flags = CLK_MESON_PLL_NOINIT_ENABLED,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp1_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_gp2_pll = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_GP2PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_GP2PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.n = {
			.reg_off = ANACTRL_GP2PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_GP2PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.od = {
			.reg_off = ANACTRL_GP2PLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
		.l = {
			.reg_off = ANACTRL_GP2PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_GP2PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_GP2PLL_CTRL3,
			.shift   = 9,
			.width   = 1,
		},
		.table = t6x_gp2_pll_table,
		.init_regs = t6x_gp2_init_regs,
		.init_count = ARRAY_SIZE(t6x_gp2_init_regs),
		.flags = CLK_MESON_PLL_FIXED_EN0P5 | CLK_MESON_PLL_L_RSTN |
			CLK_MESON_PLL_NOINIT_ENABLED,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gp2_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		/*
		 * The TCON driver writes the GP2 register directly.
		 * remove CLK_IGNORE_UNUSED if the tcon driver use clock API to
		 * control gp2 pll.
		 */
		.flags = CLK_IGNORE_UNUSED,
	},
};

static const struct reg_sequence t6x_hifi_init_regs[] = {
	{ .reg = ANACTRL_HIFIPLL_CTRL1,	.def = 0x03a04e20 },
	{ .reg = ANACTRL_HIFIPLL_CTRL2,	.def = 0x40000 },
	{ .reg = ANACTRL_HIFIPLL_CTRL3,	.def = 0x0f0da000 },
};

static const struct pll_params_table hifi_pll_table[] = {
	PLL_PARAMS_OD(150, 1, 0), /* DCO = 1800M OD = 0 PLL = 1800M */
	PLL_PARAMS_OD(150, 1, 2), /* DCO = 1800M OD = 4 PLL = 450M */
	PLL_PARAMS_OD(163, 1, 2), /* DCO = 1944M OD = 4 PLL = 486M */
	{ /* sentinel */  }
};

static struct clk_regmap t6x_hifi_pll = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.n = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.m = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.od = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
		.frac = {
			.reg_off = ANACTRL_HIFIPLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HIFIPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_HIFIPLL_CTRL3,
			.shift   = 9,
			.width   = 1,
		},
		.table = hifi_pll_table,
		.init_regs = t6x_hifi_init_regs,
		.init_count = ARRAY_SIZE(t6x_hifi_init_regs),
		.flags = CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			CLK_MESON_PLL_FIXED_EN0P5 | CLK_MESON_PLL_L_RSTN
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hifi_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct reg_sequence t6x_hifi1_init_regs[] = {
	{ .reg = ANACTRL_HIFI1PLL_CTRL1,	.def = 0x03a04e20 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL2,	.def = 0x40000 },
	{ .reg = ANACTRL_HIFI1PLL_CTRL3,	.def = 0x0f0da000 },
};

static struct clk_regmap t6x_hifi1_pll = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.n = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.m = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 0,
			.width   = 9,
		},
		.od = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift	 = 10,
			.width	 = 3,
		},
		.frac = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL1,
			.shift   = 0,
			.width   = 19,
		},
		.l = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.l_rst = {
			.reg_off = ANACTRL_HIFI1PLL_CTRL3,
			.shift   = 9,
			.width   = 1,
		},
		.table = hifi_pll_table,
		.init_regs = t6x_hifi1_init_regs,
		.init_count = ARRAY_SIZE(t6x_hifi1_init_regs),
		.flags = CLK_MESON_PLL_FIXED_FRAC_WEIGHT_PRECISION |
			CLK_MESON_PLL_FIXED_EN0P5 | CLK_MESON_PLL_L_RSTN
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hifi1_pll",
		.ops = &meson_clk_pll_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_rtc_32k_in = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_in",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param clk_32k_div_table[] = {
	{
		.n1	= 733, .m1	= 8,
		.n2	= 732, .m2	= 11,
		.dual	= 1,
	},
	{}
};

static struct clk_regmap t6x_rtc_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = clk_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_rtc_32k_in.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_rtc_32k_force_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_force_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_rtc_32k_div.hw,
			&t6x_rtc_32k_in.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_rtc_32k_out = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "rtc_32k_out",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_rtc_32k_force_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_rtc_32k_mux0_0 = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RTC_CTRL,
		.mask = 0x1,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_mux0_0",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &t6x_rtc_32k_out.hw },
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_rtc_32k_mux0_1 = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RTC_CTRL,
		.mask = 0x1,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc_32k_mux0_1",
		.ops = &clk_regmap_mux_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "pad", },
			{ .fw_name = "xtal", },
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_rtc_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_RTC_CTRL,
		.mask = 0x1,
		.shift = 1,
	},
	.hw.init = &(struct clk_init_data){
		.name = "rtc",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_rtc_32k_mux0_0.hw,
			&t6x_rtc_32k_mux0_1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_sys_clk_sel[] = { 0, 1, 2, 3, 4 };

static const struct clk_parent_data t6x_sys_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div2.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div5.hw }
};

static struct clk_regmap t6x_sys_clk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_sys_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk_0_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = t6x_sys_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_sys_clk_sel),
	},
};

static struct clk_regmap t6x_sys_clk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk_0_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_names = (const char *[]){ "sys_clk_0_sel" },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_sys_clk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sys_clk_0",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_names = (const char *[]){ "sys_clk_0_div" },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_sys_clk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_sys_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk_1_sel",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = t6x_sys_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_sys_clk_sel),
	},
};

static struct clk_regmap t6x_sys_clk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk_1_div",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_names = (const char *[]){ "sys_clk_1_sel" },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_sys_clk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sys_clk_1",
		.ops = &clk_regmap_gate_ro_ops,
		.parent_names = (const char *[]){ "sys_clk_1_div" },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_sys_clk_0.hw,
			&t6x_sys_clk_1.hw
		},
		.num_parents = 2,
	},
};

static u32 mux_table_axi_clk_sel[] = { 0, 1, 2, 3, 4, 6, 7 };

static const struct clk_parent_data t6x_axi_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div2.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div5.hw },
	{ .hw = &t6x_fclk_div2p5.hw },
	{ .hw = &t6x_rtc_clk.hw }
};

static struct clk_regmap t6x_axi_clk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_axi_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_axi_clk_sel),
	},
};

static struct clk_regmap t6x_axi_clk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_names = (const char *[]){ "axi_clk_0_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_axi_clk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "axi_clk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "axi_clk_0_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap t6x_axi_clk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_axi_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_axi_clk_sel),
	},
};

static struct clk_regmap t6x_axi_clk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_names = (const char *[]){ "axi_clk_1_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_axi_clk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "axi_clk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "axi_clk_1_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap t6x_axi_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AXI_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "axi_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_axi_clk_0.hw,
			&t6x_axi_clk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static struct clk_regmap t6x_mdcsys_clk_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MDCSYS_CLK_CNTL,
		.mask = 0x7,
		.shift = 10,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mdcsys_clk_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_axi_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_axi_clk_sel),
	},
};

static struct clk_regmap t6x_mdcsys_clk_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MDCSYS_CLK_CNTL,
		.shift = 0,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mdcsys_clk_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_names = (const char *[]){ "mdcsys_clk_0_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_mdcsys_clk_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MDCSYS_CLK_CNTL,
		.bit_idx = 13,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mdcsys_clk_0",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "mdcsys_clk_0_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap t6x_mdcsys_clk_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MDCSYS_CLK_CNTL,
		.mask = 0x7,
		.shift = 26,
		.table = mux_table_axi_clk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mdcsys_clk_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_axi_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_axi_clk_sel),
	},
};

static struct clk_regmap t6x_mdcsys_clk_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MDCSYS_CLK_CNTL,
		.shift = 16,
		.width = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mdcsys_clk_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_names = (const char *[]){ "mdcsys_clk_1_sel" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_mdcsys_clk_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MDCSYS_CLK_CNTL,
		.bit_idx = 29,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "mdcsys_clk_1",
		.ops = &clk_regmap_gate_ops,
		.parent_names = (const char *[]){ "mdcsys_clk_1_div" },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IS_CRITICAL,
	},
};

static struct clk_regmap t6x_mdcsys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MDCSYS_CLK_CNTL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mdcsys_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_mdcsys_clk_0.hw,
			&t6x_mdcsys_clk_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static u32 t6x_saradc_mux_table[] = { 0, 1, 2 };

static const struct clk_parent_data t6x_saradc_sel_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_sys_clk.hw },
	{ .hw = &t6x_fclk_div5.hw },
};

static struct clk_regmap t6x_saradc_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_saradc_mux_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_saradc_sel_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_saradc_sel_clk_sel),
	},
};

static struct clk_regmap t6x_saradc_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_saradc_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_saradc = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_SAR_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "saradc",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_saradc_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_cecb_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cecb_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_cecb_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_CECB_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_CECB_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = clk_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cecb_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_cecb_32k_sel_pre = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 24,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel_pre",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cecb_32k_div.hw,
			&t6x_cecb_32k_clkin.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_cecb_32k_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 31,
		.flags = CLK_MUX_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cecb_32k_sel_pre.hw,
			&t6x_rtc_clk.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_cecb_32k_clkout = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CECB_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cecb_32k_clkout",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cecb_32k_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_sc_mux_table[] = {0, 1, 2, 3, 4};

static const struct clk_parent_data t6x_sc_parent_data[] = {
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div5.hw },
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div2.hw },
};

static struct clk_regmap t6x_sc_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t6x_sc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_sc_parent_data,
		.num_parents = ARRAY_SIZE(t6x_sc_parent_data),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_sc_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sc_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_sc_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_sc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SC_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_sc_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static u32 mux_table_vclk_sel[] = {1, 2, 3, 4, 5, 6, 7};
static const struct clk_hw *t6x_vclk_parent_hws[] = {
	//&t6x_vid_pll.hw, //TODO: Need to confirm vid pll with vlsi
	&t6x_gp0_pll.hw,
	&t6x_hifi_pll.hw,
	&t6x_hifi1_pll.hw,
	&t6x_fclk_div4.hw,
	&t6x_fclk_div5.hw,
	&t6x_fclk_div7.hw
};

static struct clk_regmap t6x_vclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.mask = 0x7,
		.shift = 16,
		.table = mux_table_vclk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_vclk2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.mask = 0x7,
		.shift = 16,
		.table = mux_table_vclk_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_vclk_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_vclk_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_vclk_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk2_input = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.bit_idx = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_input",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk2_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_vclk2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk2_input.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_vclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 19,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk2_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk2_div1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk2_div2_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div2_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk2_div4_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div4_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk2_div6_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 3,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div6_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vclk2_div12_en = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VIID_CLK0_CTRL,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vclk2_div12_en",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vclk2.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_fixed_factor t6x_vclk_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_vclk_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_vclk_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_vclk_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk_div12_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_vclk2_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk2_div2_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_vclk2_div4 = {
	.mult = 1,
	.div = 4,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div4",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk2_div4_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_vclk2_div6 = {
	.mult = 1,
	.div = 6,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div6",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk2_div6_en.hw
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_vclk2_div12 = {
	.mult = 1,
	.div = 12,
	.hw.init = &(struct clk_init_data){
		.name = "vclk2_div12",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vclk2_div12_en.hw
		},
		.num_parents = 1,
	},
};

static u32 mux_table_cts_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *t6x_cts_parent_hws[] = {
	&t6x_vclk_div1.hw,
	&t6x_vclk_div2.hw,
	&t6x_vclk_div4.hw,
	&t6x_vclk_div6.hw,
	&t6x_vclk_div12.hw,
	&t6x_vclk2_div1.hw,
	&t6x_vclk2_div2.hw,
	&t6x_vclk2_div4.hw,
	&t6x_vclk2_div6.hw,
	&t6x_vclk2_div12.hw
};

static struct clk_regmap t6x_cts_enci_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_enci_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_cts_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_cts_vdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VIID_CLK0_DIV,
		.mask = 0xf,
		.shift = 28,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_vdac_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_cts_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_cts_encp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VID_CLK0_DIV,
		.mask = 0xf,
		.shift = 20,
		.table = mux_table_cts_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_encp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_cts_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_cts_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

/* TOFIX: add support for cts_tcon */
static u32 mux_table_hdmi_tx_sel[] = { 0, 1, 2, 3, 4, 8, 9, 10, 11, 12 };
static const struct clk_hw *t6x_cts_hdmi_tx_parent_hws[] = {
	&t6x_vclk_div1.hw,
	&t6x_vclk_div2.hw,
	&t6x_vclk_div4.hw,
	&t6x_vclk_div6.hw,
	&t6x_vclk_div12.hw,
	&t6x_vclk2_div1.hw,
	&t6x_vclk2_div2.hw,
	&t6x_vclk2_div4.hw,
	&t6x_vclk2_div6.hw,
	&t6x_vclk2_div12.hw
};

static struct clk_regmap t6x_hdmi_tx_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HDMI_CLK_CTRL,
		.mask = 0xf,
		.shift = 16,
		.table = mux_table_hdmi_tx_sel,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmi_tx_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_cts_hdmi_tx_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_cts_hdmi_tx_parent_hws),
		.flags = CLK_SET_RATE_NO_REPARENT | CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_cts_enci = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_enci",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_enci_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_cts_encp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_encp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_encp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_cts_vdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 4,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_vdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_vdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_hdmi_tx = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_CLK0_CTRL2,
		.bit_idx = 5,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmi_tx",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hdmi_tx_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

/*mali_clk*/
/*
 * The MALI IP is clocked by two identical clocks (mali_0 and mali_1)
 * muxed by a glitch-free switch on Meson8b and Meson8m2 and later.
 *
 * CLK_SET_RATE_PARENT is added for mali_0_sel clock
 * 1.gp0 pll only support the 846M, avoid other rate 500/400M from it
 * 2.hifi pll is used for other module, skip it, avoid some rate from it
 */
static u32 mux_table_mali[] = { 0, 3, 4, 5, 6};

static const struct clk_parent_data t6x_mali_0_1_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div2p5.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_gp1_pll.hw },
};

static struct clk_regmap t6x_mali_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(t6x_mali_0_1_parent_data),
	},
};

static struct clk_regmap t6x_mali_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_mali_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_mali_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_mali_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_mali_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = mux_table_mali,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_mali_0_1_parent_data,
		.num_parents = ARRAY_SIZE(t6x_mali_0_1_parent_data),
	},
};

static struct clk_regmap t6x_mali_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_mali_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_mali_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_mali_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *t6x_mali_parent_hws[] = {
	&t6x_mali_0.hw,
	&t6x_mali_1.hw
};

static struct clk_regmap t6x_mali = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_MALI_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "mali",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_mali_parent_hws,
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static u32 mux_table_hevcb[] = { 0, 1, 2, 5, 7};

static const struct clk_parent_data t6x_hevcb_parent_data[] = {
	{.hw = &t6x_fclk_div2p5.hw,},
	{.hw = &t6x_fclk_div3.hw,},
	{.hw = &t6x_fclk_div4.hw,},
	{.hw = &t6x_gp1_pll.hw,},
	{ .fw_name = "xtal", },
};

static struct clk_regmap t6x_hevcb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.table = mux_table_hevcb,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hevcb_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hevcb_parent_data),
	},
};

static struct clk_regmap t6x_hevcb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.shift = 16,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hevcb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hevcb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hevcb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hevcb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.table = mux_table_hevcb,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hevcb_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hevcb_parent_data),
	},
};

static struct clk_regmap t6x_hevcb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevc_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hevcb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hevcb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hevcb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hevcb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hevcb = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hevcb",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hevcb_0.hw,
			&t6x_hevcb_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static u32 mux_table_hcodec[] = { 0, 1, 2, 3, 4, 7};

static const struct clk_parent_data t6x_hcodec_parent_data[] = {
	{.hw = &t6x_fclk_div2p5.hw,},
	{.hw = &t6x_fclk_div3.hw,},
	{.hw = &t6x_fclk_div4.hw,},
	{.hw = &t6x_fclk_div4.hw,},
	{.hw = &t6x_fclk_div4.hw,},
	{ .fw_name = "xtal", },
};

static struct clk_regmap t6x_hcodec_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.flags = CLK_MUX_ROUND_CLOSEST,
		.table = mux_table_hcodec,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hcodec_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hcodec_parent_data),
	},
};

static struct clk_regmap t6x_hcodec_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.shift = 0,
		.width = 7,
		.flags = CLK_DIVIDER_ROUND_CLOSEST,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hcodec_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hcodec_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC2_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hcodec_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hcodec_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hcodec_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hcodec_parent_data),
	},
};

static struct clk_regmap t6x_hcodec_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hcodec_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hcodec_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hcodec_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hcodec_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_hcodec = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDEC4_CLK_CTRL,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hcodec",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_hcodec_0.hw,
			&t6x_hcodec_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static u32 t6x_vpu_mux_table[] = {0, 1, 2, 3};

static const struct clk_hw *t6x_vpu_parent_hws[] = {
	&t6x_fclk_div3.hw,
	&t6x_fclk_div4.hw,
	&t6x_gp1_pll.hw,
	&t6x_fclk_div2p5.hw,
};

static struct clk_regmap t6x_vpu_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t6x_vpu_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_vpu_parent_hws),
	},
};

static struct clk_regmap t6x_vpu_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vpu_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_vpu_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vpu_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vpu_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = t6x_vpu_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_vpu_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_vpu_parent_hws),
	},
};

static struct clk_regmap t6x_vpu_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vpu_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_vpu_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vpu_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vpu_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vpu = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VPU_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vpu",
		.ops = &clk_regmap_mux_ops,
		/*
		 * bit 31 selects from 2 possible parents:
		 * vpu_0 or vpu_1
		 */
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vpu_0.hw,
			&t6x_vpu_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static u32 t6x_vapb_table[] = { 0, 1, 2, 3, 7};
static const struct clk_hw *t6x_vapb_parent_hws[] = {
	&t6x_fclk_div4.hw,
	&t6x_fclk_div3.hw,
	&t6x_fclk_div5.hw,
	&t6x_fclk_div7.hw,
	&t6x_fclk_div2p5.hw
};

static struct clk_regmap t6x_vapb_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t6x_vapb_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_vapb_parent_hws),
	},
};

static struct clk_regmap t6x_vapb_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vapb_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_vapb_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vapb_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vapb_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = t6x_vapb_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_vapb_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_vapb_parent_hws),
	},
};

static struct clk_regmap t6x_vapb_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vapb_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_vapb_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vapb_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vapb_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_vapb = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vapb_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vapb_0.hw,
			&t6x_vapb_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static struct clk_regmap t6x_ge2d_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VAPBCLK_CTRL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ge2d_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vapb.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data t6x_hdmirx_sys_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div5.hw }
};

static struct clk_regmap t6x_hdmirx_5m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_5m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t6x_hdmirx_5m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_5m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_5m_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_5m  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_5m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_5m_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_2m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_2m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t6x_hdmirx_2m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_2m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_2m_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_2m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL0,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_2m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_2m_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_cfg_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t6x_hdmirx_cfg_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_cfg_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_cfg_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_cfg  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_cfg",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_cfg_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_hdcp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_hdcp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t6x_hdmirx_hdcp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_hdcp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_hdcp_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_hdcp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL1,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_hdcp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_hdcp_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_aud_pll_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_aud_pll_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t6x_hdmirx_aud_pll_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_aud_pll_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_aud_pll_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_aud_pll  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_aud_pll",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_aud_pll_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_acr_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t6x_hdmirx_acr_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_acr_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_acr_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_acr = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL2,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_acr",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_acr_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_meter_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_hdmirx_sys_parent_data,
		.num_parents = ARRAY_SIZE(t6x_hdmirx_sys_parent_data),
	},
};

static struct clk_regmap t6x_hdmirx_meter_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "hdmirx_meter_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_meter_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_hdmirx_meter  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_HRX_CLK_CTRL3,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "hdmirx_meter",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_hdmirx_meter_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static u32 t6x_pwm_clk_table[] = {0, 2, 3};

static const struct clk_parent_data t6x_pwm_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div3.hw }
};

static struct clk_regmap t6x_pwm_c_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_pwm_d_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_d_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_CD_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_d_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_pwm_e_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_e_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_e = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_e",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_e_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_pwm_f_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_f_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_f = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_EF_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_f",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_f_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_pwm_g_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_g_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_g = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_g",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_g_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_pwm_h_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_h_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_h_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_h = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_GH_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_h",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_h_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_pwm_i_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_i_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_i_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_i = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_i",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_i_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_pwm_j_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.mask = 0x3,
		.shift = 25,
		.table = t6x_pwm_clk_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_pwm_parent_data,
		.num_parents = ARRAY_SIZE(t6x_pwm_parent_data),
	},
};

static struct clk_regmap t6x_pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_j_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_pwm_j = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_PWM_CLK_IJ_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "pwm_j",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_pwm_j_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 t6x_spicc_mux_table[] = {0, 1, 2, 3, 4, 5, 6};

static const struct clk_parent_data t6x_spicc_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_sys_clk.hw },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div2.hw },
	{ .hw = &t6x_fclk_div5.hw },
	{ .hw = &t6x_fclk_div7.hw },
};

static struct clk_regmap t6x_spicc0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
		.table = t6x_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_spicc_parent_hws),
	},
};

static struct clk_regmap t6x_spicc0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.mask = 0x7,
		.shift = 23,
		.table = t6x_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_spicc_parent_hws),
	},
};

static struct clk_regmap t6x_spicc1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc2_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.mask = 0x7,
		.shift = 7,
		.table = t6x_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_spicc_parent_hws),
	},
};

static struct clk_regmap t6x_spicc2_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc2_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc2_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc2 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc2",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc2_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc3_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.mask = 0x7,
		.shift = 23,
		.table = t6x_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc3_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_spicc_parent_hws),
	},
};

static struct clk_regmap t6x_spicc3_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc3_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc3_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc3 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL1,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc3",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc3_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc4_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL2,
		.mask = 0x7,
		.shift = 7,
		.table = t6x_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc4_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_spicc_parent_hws),
	},
};

static struct clk_regmap t6x_spicc4_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL2,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc4_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc4_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc4 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL2,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc4",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc4_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc5_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL2,
		.mask = 0x7,
		.shift = 23,
		.table = t6x_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc5_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_spicc_parent_hws),
	},
};

static struct clk_regmap t6x_spicc5_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL2,
		.shift = 16,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "spicc5_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc5_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_spicc5 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_SPISG_CLK_CTRL2,
		.bit_idx = 22,
	},
	.hw.init = &(struct clk_init_data){
		.name = "spicc5",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_spicc5_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_bcon_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_BCON_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
		.table = t6x_spicc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "bcon_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_spicc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_spicc_parent_hws),
	},
};

static struct clk_regmap t6x_bcon_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_BCON_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "bcon_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_bcon_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_bcon = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_BCON_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "bcon",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_bcon_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_sd_emmc_mux_table[] = {0, 1, 2, 4, 7};

static const struct clk_parent_data t6x_sd_emmc_parent_data[]  = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div2.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div2p5.hw },
	{ .hw = &t6x_gp0_pll.hw }
};

static struct clk_regmap t6x_sd_emmc_c_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t6x_sd_emmc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_sd_emmc_parent_data,
		.num_parents = ARRAY_SIZE(t6x_sd_emmc_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t6x_sd_emmc_c_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "sd_emmc_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_sd_emmc_c_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE //| CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_sd_emmc_c = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_NAND_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sd_emmc_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_sd_emmc_c_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE //| CLK_SET_RATE_PARENT
	},
};

static const struct clk_parent_data t6x_vdin_parent_hws[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div5.hw },
};

static u32 t6x_vdin_meas_table[] = {0, 1, 2, 3};

static struct clk_regmap t6x_vdin_meas_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t6x_vdin_meas_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_vdin_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_vdin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_vdin_meas_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vdin_meas_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vdin_meas_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_vdin_meas = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VDIN_MEAS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vdin_meas",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vdin_meas_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_vid_lock_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vid_lock_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_vid_lock_clk  = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_VID_LOCK_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vid_lock_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_vid_lock_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static u32 t6x_eth_rmii_table[] = { 0 };

static struct clk_regmap t6x_eth_rmii_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_eth_rmii_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div2.hw,
		},
		.num_parents = 1
	},
};

static struct clk_regmap t6x_eth_rmii_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_eth_rmii_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_eth_rmii = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_rmii",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_eth_rmii_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_fixed_factor t6x_eth_div8 = {
	.mult = 1,
	.div = 8,
	.hw.init = &(struct clk_init_data){
		.name = "eth_div8",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_fclk_div2.hw },
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_eth_125m = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = CLKCTRL_ETH_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "eth_125m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_eth_div8.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_ts_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_ts_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TS_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "ts_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_ts_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static u32 t6x_vafe_mux_table[] = { 1 };

static struct clk_regmap t6x_vafe_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ATV_DMD_SYS_CLK_CNTL,
		.mask = 0x3,
		.shift = 24,
		.table = t6x_vafe_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vafe_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_vafe_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ATV_DMD_SYS_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "vafe_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vafe_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_vafe = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ATV_DMD_SYS_CLK_CNTL,
		.bit_idx = 23,
	},
	.hw.init = &(struct clk_init_data){
		.name = "vafe",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_vafe_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_tcon_pll_mux_table[] = { 0, 1, 2, 3 };

static const struct clk_parent_data t6x_cts_tcon_pll_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div5.hw },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div3.hw },
};

static struct clk_regmap t6x_cts_tcon_pll_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_TCON_CLK_CNTL,
		.mask = 0x7,
		.shift = 7,
		.table = t6x_tcon_pll_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_tcon_pll_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_cts_tcon_pll_clk_parent_data,
		.num_parents = ARRAY_SIZE(t6x_cts_tcon_pll_clk_parent_data),
	},
};

static struct clk_regmap t6x_cts_tcon_pll_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_TCON_CLK_CNTL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_tcon_pll_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_tcon_pll_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_cts_tcon_pll_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TCON_CLK_CNTL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_tcon_pll_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_tcon_pll_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_adc_extclk_mux_table[] = { 0, 1, 2, 3, 4 };

static const struct clk_parent_data t6x_adc_extclk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div5.hw },
	{ .hw = &t6x_fclk_div7.hw },
};

static struct clk_regmap t6x_adc_extclk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL,
		.mask = 0x7,
		.shift = 25,
		.table = t6x_adc_extclk_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_adc_extclk_parent_data,
		.num_parents = ARRAY_SIZE(t6x_adc_extclk_parent_data),
	},
};

static struct clk_regmap t6x_adc_extclk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "adc_extclk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_adc_extclk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_adc_extclk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "adc_extclk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_adc_extclk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_atv_dmd_mux_table[] = { 1 };

static const struct clk_hw *t6x_atv_dmd_parent_hws[] = {
	&t6x_adc_extclk.hw,
};

static struct clk_regmap t6x_atv_dmd_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_ATV_DMD_SYS_CLK_CNTL,
		.mask = 0x3,
		.shift = 8,
		.table = t6x_atv_dmd_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "atv_dmd_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_atv_dmd_parent_hws,
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_atv_dmd_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_ATV_DMD_SYS_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "atv_dmd_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_atv_dmd_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_atv_dmd = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_ATV_DMD_SYS_CLK_CNTL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "atv_dmd",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_atv_dmd_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_demod_32k_clkin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEMOD_32K_CNTL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "demod_32k_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param t6x_demod_32k_div_table[] = {
	{
		.dual	= 0,
		.n1	= 733,
	},
	{}
};

static struct clk_regmap t6x_demod_32k_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_DEMOD_32K_CNTL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_DEMOD_32K_CNTL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_DEMOD_32K_CNTL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_DEMOD_32K_CNTL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_DEMOD_32K_CNTL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = t6x_demod_32k_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_32k_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_demod_32k_clkin.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_demod_32k = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEMOD_32K_CNTL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data){
		.name = "demod_32k",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_demod_32k_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_demod_core_mux_table[] = { 0, 1, 2 };

static const struct clk_parent_data t6x_cts_demod_core_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div7.hw },
	{ .hw = &t6x_fclk_div4.hw },
};

static struct clk_regmap t6x_cts_demod_core_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_demod_core_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_cts_demod_core_parent_data,
		.num_parents = ARRAY_SIZE(t6x_cts_demod_core_parent_data),
	},
};

static struct clk_regmap t6x_cts_demod_core_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_demod_core_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_cts_demod_core = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_demod_core",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_demod_core_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *t6x_dae_parent_hws[] = {
	&t6x_fclk_div3.hw,
	&t6x_fclk_div4.hw,
	&t6x_fclk_div5.hw,
	&t6x_fclk_div2p5.hw,
};

static struct clk_regmap t6x_dae_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DAE_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dae_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_dae_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_dae_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_dae_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DAE_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dae_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dae_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dae_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DAE_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dae_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dae_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT
				 | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_dae_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DAE_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dae_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_dae_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_dae_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dae_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DAE_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dae_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dae_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dae_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DAE_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dae_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dae_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT
				 | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_dae = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DAE_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dae",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_dae_0.hw,
			&t6x_dae_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static struct clk_regmap t6x_dpe_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DPE_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dpe_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_dae_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_dae_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_dpe_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DPE_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dpe_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dpe_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dpe_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DPE_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dpe_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dpe_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT
				 | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_dpe_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DPE_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dpe_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_dae_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_dae_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dpe_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DPE_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dpe_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dpe_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dpe_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DPE_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dpe_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_dpe_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT
				 | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_dpe = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DPE_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dpe",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_dpe_0.hw,
			&t6x_dpe_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static u32 t6x_demod_core_t2_mux_table[] = { 0, 1, 2 };

static const struct clk_parent_data t6x_cts_demod_core_t2_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div5.hw },
	{ .hw = &t6x_fclk_div4.hw },
};

static struct clk_regmap t6x_cts_demod_core_t2_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL1,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_demod_core_t2_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_t2_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_cts_demod_core_t2_parent_data,
		.num_parents = ARRAY_SIZE(t6x_cts_demod_core_t2_parent_data),
	},
};

static struct clk_regmap t6x_cts_demod_core_t2_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL1,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cts_demod_core_t2_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_demod_core_t2_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_cts_demod_core_t2_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DEMOD_CLK_CNTL1,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cts_demod_core_t2_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cts_demod_core_t2_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_cdac_mux_table[] = {0, 1};

static const struct clk_parent_data t6x_cdac_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div5.hw },
};

static struct clk_regmap t6x_cdac_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.mask = 0x3,
		.shift = 16,
		.table = t6x_cdac_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_cdac_parent_data,
		.num_parents = ARRAY_SIZE(t6x_cdac_parent_data),
		.flags = CLK_GET_RATE_NOCACHE
	},
};

static struct clk_regmap t6x_cdac_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.shift = 0,
		.width = 16,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "cdac_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cdac_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_cdac = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CDAC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "cdac",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_cdac_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE |	CLK_SET_RATE_PARENT
	},
};

static u32 t6x_usb3_250m_mux_table[] = { 0, 1, 2, 3, 7 };

static const struct clk_hw *t6x_usb3_250m_parent_hws[] = {
	&t6x_fclk_div4.hw,
	&t6x_fclk_div3.hw,
	&t6x_fclk_div5.hw,
	&t6x_fclk_div2.hw,
	&t6x_fclk_div2p5.hw,
};

static struct clk_regmap t6x_usb3_250m_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_usb3_250m_mux_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "usb3_250m_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_usb3_250m_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_usb3_250m_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_usb3_250m_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "usb3_250m_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_usb3_250m_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_usb3_250m = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb3_250m",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_usb3_250m_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT
				 | CLK_IGNORE_UNUSED,
	},
};

static u32 t6x_tsin_mux_table[] = { 0, 2, 3, 4, 5, 6 };

static const struct clk_hw *t6x_tsin_parent_hws[] = {
	&t6x_fclk_div2.hw,
	&t6x_fclk_div4.hw,
	&t6x_fclk_div3.hw,
	&t6x_fclk_div2p5.hw,
	&t6x_fclk_div5.hw,
	&t6x_fclk_div7.hw,
};

static struct clk_regmap t6x_tsin_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.mask = 0x3,
		.shift = 9,
		.table = t6x_tsin_mux_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "tsin_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_tsin_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_tsin_parent_hws),
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap t6x_tsin_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tsin_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_tsin_0_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_tsin_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tsin_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_tsin_0_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT
				 | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_tsin_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.mask = 0x3,
		.shift = 25,
		.table = t6x_tsin_mux_table
	},
	.hw.init = &(struct clk_init_data){
		.name = "tsin_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_tsin_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_tsin_parent_hws),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_tsin_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "tsin_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_tsin_1_sel.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_tsin_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tsin_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) { &t6x_tsin_1_div.hw },
		.num_parents = 1,
		.flags = CLK_SET_RATE_GATE | CLK_SET_RATE_PARENT
				 | CLK_IGNORE_UNUSED,
	},
};

static struct clk_regmap t6x_tsin_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tsin_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_tsin_0.hw,
			&t6x_tsin_1.hw
		},
		.num_parents = 2,
		.flags = CLK_OPS_PARENT_ENABLE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_tsin = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_TSIN_CLK_CNTL,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "tsin",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_tsin_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT | CLK_IGNORE_UNUSED,
	},
};

static u32 t6x_gen_clk_mux_table[] = { 0, 1, 5, 7, 8, 19, 20, 21, 22, 23, 24 };

static const struct clk_parent_data t6x_gen_sel_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_rtc_clk.hw },
	{ .hw = &t6x_gp0_pll.hw },
	{ .hw = &t6x_hifi_pll.hw },
	{ .hw = &t6x_hifi1_pll.hw },
	{ .hw = &t6x_fclk_div2.hw },
	{ .hw = &t6x_fclk_div2p5.hw },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div4.hw },
	{ .hw = &t6x_fclk_div5.hw },
	{ .hw = &t6x_fclk_div7.hw },
};

static struct clk_regmap t6x_gen_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.mask = 0x1f,
		.shift = 12,
		.table = t6x_gen_clk_mux_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_gen_sel_clk_sel,
		.num_parents = ARRAY_SIZE(t6x_gen_sel_clk_sel),
	},
};

static struct clk_regmap t6x_gen_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.shift = 0,
		.width = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "gen_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_gen_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_gen = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_GEN_CLK_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "gen",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_gen_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_24m_clk_gate = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 11,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "24m_clk_gate",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_fixed_factor t6x_24m_div2 = {
	.mult = 1,
	.div = 2,
	.hw.init = &(struct clk_init_data){
		.name = "24m_div2",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_24m_clk_gate.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_12m_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 10,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "12m_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_24m_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_25m_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div2.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_25m_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_CLK12_24_CTRL,
		.bit_idx = 12,
	},
	.hw.init = &(struct clk_init_data){
		.name = "25m_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_25m_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static u32 t6x_amfc_mux_table[] = {0, 1, 2, 3, 7};

static const struct clk_hw *t6x_amfc_parent_hws[] = {
	&t6x_fclk_div4.hw,
	&t6x_fclk_div3.hw,
	&t6x_fclk_div5.hw,
	&t6x_fclk_div7.hw,
	&t6x_fclk_div2p5.hw,
};

static struct clk_regmap t6x_amfc_0_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.mask = 0x7,
		.shift = 9,
		.table = t6x_amfc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "amfc_0_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_amfc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_amfc_parent_hws),
	},
};

static struct clk_regmap t6x_amfc_0_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.shift = 0,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "amfc_0_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_amfc_0_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_amfc_0 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.bit_idx = 7,
	},
	.hw.init = &(struct clk_init_data){
		.name = "amfc_0",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_amfc_0_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_amfc_1_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.mask = 0x7,
		.shift = 25,
		.table = t6x_amfc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "amfc_1_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_amfc_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_amfc_parent_hws),
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_amfc_1_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.shift = 16,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "amfc_1_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_amfc_1_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_amfc_1 = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "amfc_1",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_amfc_1_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_amfc = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_AMFC_CLK_CTRL,
		.mask = 1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "amfc",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_amfc_0.hw,
			&t6x_amfc_1.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT | CLK_OPS_PARENT_ENABLE,
	},
};

static struct clk_regmap t6x_usb2_48m_pre_in = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb2_48m_pre_in",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_fclk_div2.hw
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param usb_48m_div_table[] = {
	{
		.n1		= 20, .m1		= 0,
		.n2		= 21, .m2		= 5,
		.dual		= 1,
	},
	{}
};

static struct clk_regmap t6x_usb2_48m_pre_div = {
	.data = &(struct meson_clk_dualdiv_data){
		.n1 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = CLKCTRL_USB_CLK_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = CLKCTRL_USB_CLK_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.table = usb_48m_div_table,
	},
	.hw.init = &(struct clk_init_data){
		.name = "usb2_48m_pre_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_usb2_48m_pre_in.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap t6x_usb2_48m_pre_force_sel = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data){
		.name = "usb2_48m_pre_force_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_usb2_48m_pre_div.hw,
			&t6x_usb2_48m_pre_in.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_usb2_48m_pre = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb2_48m_pre",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_usb2_48m_pre_force_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_hw *t6x_usb2_48m_clk_tmp_parent_hws[] = {
	&t6x_gp0_pll.hw,
	&t6x_gp1_pll.hw,
	&t6x_gp2_pll.hw,
};

static struct clk_regmap t6x_usb2_48m_clk_tmp_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL0,
		.mask = 0x3,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb2_48m_tmp_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = t6x_usb2_48m_clk_tmp_parent_hws,
		.num_parents = ARRAY_SIZE(t6x_usb2_48m_clk_tmp_parent_hws),
	},
};

static struct clk_regmap t6x_usb2_48m_clk_tmp_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_USB_CLK_CTRL1,
		.shift = 25,
		.width = 7,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb2_48m_tmp_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_usb2_48m_clk_tmp_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_usb2_48m_clk_tmp = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_USB_CLK_CTRL0,
		.bit_idx = 26,
	},
	.hw.init = &(struct clk_init_data){
		.name = "usb2_48m_tmp",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_usb2_48m_clk_tmp_div.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT
	},
};

static struct clk_regmap t6x_usb2_48m_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_USB_CLK_CTRL0,
		.mask = 0x1,
		.shift = 27,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "usb2_48m_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_usb2_48m_clk_tmp.hw,
			&t6x_usb2_48m_pre.hw
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT
	},
};

static u32 t6x_dsc_mux_table[] = { 0, 2, 4 };

static const struct clk_parent_data t6x_dsc_clk_parent_data[] = {
	{ .fw_name = "xtal", },
	{ .hw = &t6x_fclk_div3.hw },
	{ .hw = &t6x_fclk_div4.hw },
};

static struct clk_regmap t6x_dsc_clk_sel = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_DSC_CLK_CTRL,
		.mask = 0x7,
		.shift = 7,
		.table = t6x_dsc_mux_table
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsc_clk_sel",
		.ops = &clk_regmap_mux_ops,
		.parent_data = t6x_dsc_clk_parent_data,
		.num_parents = ARRAY_SIZE(t6x_dsc_clk_parent_data),
	},
};

static struct clk_regmap t6x_dsc_clk_div = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSC_CLK_CTRL,
		.shift = 0,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsc_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_dsc_clk_sel.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dsc_clk = {
	.data = &(struct clk_regmap_gate_data){
		.offset = CLKCTRL_DSC_CLK_CTRL,
		.bit_idx = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "dsc_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_dsc_clk_div.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE | CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap t6x_dsc_post_sel = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_DSC_CLK_CTRL,
		.shift = 16,
		.width = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "dsc_post_sel",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&t6x_dsc_clk.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_T6W_SYS_GATE(_name, _reg, _bit)				\
struct clk_regmap _name = {						\
	.data = &(struct clk_regmap_gate_data) {			\
		.offset = (_reg),					\
		.bit_idx = (_bit),					\
	},								\
	.hw.init = &(struct clk_init_data) {				\
		.name = #_name,						\
		.ops = &clk_regmap_gate_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&t6x_sys_clk.hw					\
		},							\
		.num_parents = 1,					\
		.flags = CLK_IGNORE_UNUSED,				\
	},								\
}

static MESON_T6W_SYS_GATE(t6x_sys_dos,			CLKCTRL_SYS_CLK_EN0_REG0, 0);
static MESON_T6W_SYS_GATE(t6x_sys_demod,		CLKCTRL_SYS_CLK_EN0_REG0, 1);
static MESON_T6W_SYS_GATE(t6x_sys_amfc,			CLKCTRL_SYS_CLK_EN0_REG0, 2);
static MESON_T6W_SYS_GATE(t6x_sys_iotm,			CLKCTRL_SYS_CLK_EN0_REG0, 3);
static MESON_T6W_SYS_GATE(t6x_sys_ddrtest,		CLKCTRL_SYS_CLK_EN0_REG0, 4);
static MESON_T6W_SYS_GATE(t6x_sys_am2axi1,		CLKCTRL_SYS_CLK_EN0_REG0, 5);
static MESON_T6W_SYS_GATE(t6x_sys_msr_clk,		CLKCTRL_SYS_CLK_EN0_REG0, 6);
static MESON_T6W_SYS_GATE(t6x_sys_audio,		CLKCTRL_SYS_CLK_EN0_REG0, 7);
static MESON_T6W_SYS_GATE(t6x_sys_tvfe,			CLKCTRL_SYS_CLK_EN0_REG0, 8);
static MESON_T6W_SYS_GATE(t6x_sys_ethmac,		CLKCTRL_SYS_CLK_EN0_REG0, 9);
static MESON_T6W_SYS_GATE(t6x_sys_ethphy,		CLKCTRL_SYS_CLK_EN0_REG0, 10);
static MESON_T6W_SYS_GATE(t6x_sys_ethaxi,		CLKCTRL_SYS_CLK_EN0_REG0, 11);
static MESON_T6W_SYS_GATE(t6x_sys_ge2d,			CLKCTRL_SYS_CLK_EN0_REG0, 12);
static MESON_T6W_SYS_GATE(t6x_sys_i2c_monitor,		CLKCTRL_SYS_CLK_EN0_REG0, 13);
static MESON_T6W_SYS_GATE(t6x_sys_usb_u22h,		CLKCTRL_SYS_CLK_EN0_REG0, 14);
static MESON_T6W_SYS_GATE(t6x_sys_usb_u2drd,		CLKCTRL_SYS_CLK_EN0_REG0, 15);
static MESON_T6W_SYS_GATE(t6x_sys_usb_u2h,		CLKCTRL_SYS_CLK_EN0_REG0, 16);
static MESON_T6W_SYS_GATE(t6x_sys_hdmirx20_aes,		CLKCTRL_SYS_CLK_EN0_REG0, 17);
static MESON_T6W_SYS_GATE(t6x_sys_hdmirx,		CLKCTRL_SYS_CLK_EN0_REG0, 19);
static MESON_T6W_SYS_GATE(t6x_sys_mmc_wrap,		CLKCTRL_SYS_CLK_EN0_REG0, 20);
static MESON_T6W_SYS_GATE(t6x_sys_cpu_subsys,		CLKCTRL_SYS_CLK_EN0_REG0, 21);
static MESON_T6W_SYS_GATE(t6x_sys_vpu_intr,		CLKCTRL_SYS_CLK_EN0_REG0, 24);
static MESON_T6W_SYS_GATE(t6x_sys_tcon,			CLKCTRL_SYS_CLK_EN0_REG0, 25);
static MESON_T6W_SYS_GATE(t6x_sys_saradc,		CLKCTRL_CLK81_GATE_CTRL_SYSWRAPPER, 0);
static MESON_T6W_SYS_GATE(t6x_sys_aocpu,		CLKCTRL_CLK81_GATE_CTRL_SYSWRAPPER, 1);
static MESON_T6W_SYS_GATE(t6x_sys_gic,			CLKCTRL_CLK81_GATE_CTRL_SYSWRAPPER, 2);
static MESON_T6W_SYS_GATE(t6x_sys_spisg,		CLKCTRL_CLK81_GATE_CTRL_SYSWRAPPER, 4);
static MESON_T6W_SYS_GATE(t6x_sys_sd_emmc_c,		CLKCTRL_CLK81_GATE_CTRL_SYSWRAPPER, 5);
static MESON_T6W_SYS_GATE(t6x_sys_aucpu,		CLKCTRL_CLK81_GATE_CTRL_SYSWRAPPER, 6);
static MESON_T6W_SYS_GATE(t6x_sys_uart_wrapper,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 0);
static MESON_T6W_SYS_GATE(t6x_sys_pwm_wrapper,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 1);
static MESON_T6W_SYS_GATE(t6x_sys_i2c_m_wrapper,	CLKCTRL_CLK81_GATE_CTRL_PERIPH, 2);
static MESON_T6W_SYS_GATE(t6x_sys_mailbox,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 3);
static MESON_T6W_SYS_GATE(t6x_sys_ir_crtl,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 4);
static MESON_T6W_SYS_GATE(t6x_sys_smart_card,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 5);
static MESON_T6W_SYS_GATE(t6x_sys_ciplus,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 6);
static MESON_T6W_SYS_GATE(t6x_sys_cec,			CLKCTRL_CLK81_GATE_CTRL_PERIPH, 7);
static MESON_T6W_SYS_GATE(t6x_sys_ts_cpu,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 8);
static MESON_T6W_SYS_GATE(t6x_sys_ts_top,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 9);
static MESON_T6W_SYS_GATE(t6x_sys_led_ctrl,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 10);
static MESON_T6W_SYS_GATE(t6x_sys_i2c_tcon,		CLKCTRL_CLK81_GATE_CTRL_PERIPH, 11);

/* Array of all clocks provided by this provider */
static struct clk_hw_onecell_data t6x_hw_onecell_data = {
	.hws = {
		[CLKID_FIXED_PLL]			= &t6x_fixed_pll.hw,
		[CLKID_FCLK_DIV2_DIV]			= &t6x_fclk_div2_div.hw,
		[CLKID_FCLK_DIV2]			= &t6x_fclk_div2.hw,
		[CLKID_FCLK_DIV3_DIV]			= &t6x_fclk_div3_div.hw,
		[CLKID_FCLK_DIV3]			= &t6x_fclk_div3.hw,
		[CLKID_FCLK_DIV4_DIV]			= &t6x_fclk_div4_div.hw,
		[CLKID_FCLK_DIV4]			= &t6x_fclk_div4.hw,
		[CLKID_FCLK_DIV5_DIV]			= &t6x_fclk_div5_div.hw,
		[CLKID_FCLK_DIV5]			= &t6x_fclk_div5.hw,
		[CLKID_FCLK_DIV7_DIV]			= &t6x_fclk_div7_div.hw,
		[CLKID_FCLK_DIV7]			= &t6x_fclk_div7.hw,
		[CLKID_FCLK_DIV2P5_DIV]			= &t6x_fclk_div2p5_div.hw,
		[CLKID_FCLK_DIV2P5]			= &t6x_fclk_div2p5.hw,
		[CLKID_GP0_PLL]				= &t6x_gp0_pll.hw,
		[CLKID_GP1_PLL]				= &t6x_gp1_pll.hw,
		[CLKID_GP2_PLL]				= &t6x_gp2_pll.hw,
		[CLKID_HIFI_PLL]			= &t6x_hifi_pll.hw,
		[CLKID_HIFI1_PLL]			= &t6x_hifi1_pll.hw,
		[CLKID_MPLL_50M_DIV]			= &t6x_mpll_50m_div.hw,
		[CLKID_MPLL_50M]			= &t6x_mpll_50m.hw,
		[CLKID_RTC_32K_IN]			= &t6x_rtc_32k_in.hw,
		[CLKID_RTC_32K_DIV]			= &t6x_rtc_32k_div.hw,
		[CLKID_RTC_32K_FORCE_SEL]		= &t6x_rtc_32k_force_sel.hw,
		[CLKID_RTC_32K_OUT]			= &t6x_rtc_32k_out.hw,
		[CLKID_RTC_32K_MUX0_0]			= &t6x_rtc_32k_mux0_0.hw,
		[CLKID_RTC_32K_MUX0_1]			= &t6x_rtc_32k_mux0_1.hw,
		[CLKID_RTC]				= &t6x_rtc_clk.hw,
		[CLKID_SYS_CLK_0_SEL]			= &t6x_sys_clk_0_sel.hw,
		[CLKID_SYS_CLK_0_DIV]			= &t6x_sys_clk_0_div.hw,
		[CLKID_SYS_CLK_0]			= &t6x_sys_clk_0.hw,
		[CLKID_SYS_CLK_1_SEL]			= &t6x_sys_clk_1_sel.hw,
		[CLKID_SYS_CLK_1_DIV]			= &t6x_sys_clk_1_div.hw,
		[CLKID_SYS_CLK_1]			= &t6x_sys_clk_1.hw,
		[CLKID_SYS_CLK]				= &t6x_sys_clk.hw,
		[CLKID_AXI_CLK_0_SEL]			= &t6x_axi_clk_0_sel.hw,
		[CLKID_AXI_CLK_0_DIV]			= &t6x_axi_clk_0_div.hw,
		[CLKID_AXI_CLK_0]			= &t6x_axi_clk_0.hw,
		[CLKID_AXI_CLK_1_SEL]			= &t6x_axi_clk_1_sel.hw,
		[CLKID_AXI_CLK_1_DIV]			= &t6x_axi_clk_1_div.hw,
		[CLKID_AXI_CLK_1]			= &t6x_axi_clk_1.hw,
		[CLKID_AXI_CLK]				= &t6x_axi_clk.hw,
		[CLKID_MDCSYS_CLK_0_SEL]		= &t6x_mdcsys_clk_0_sel.hw,
		[CLKID_MDCSYS_CLK_0_DIV]		= &t6x_mdcsys_clk_0_div.hw,
		[CLKID_MDCSYS_CLK_0]			= &t6x_mdcsys_clk_0.hw,
		[CLKID_MDCSYS_CLK_1_SEL]		= &t6x_mdcsys_clk_1_sel.hw,
		[CLKID_MDCSYS_CLK_1_DIV]		= &t6x_mdcsys_clk_1_div.hw,
		[CLKID_MDCSYS_CLK_1]			= &t6x_mdcsys_clk_1.hw,
		[CLKID_MDCSYS_CLK]			= &t6x_mdcsys_clk.hw,
		[CLKID_SARADC_SEL]			= &t6x_saradc_sel.hw,
		[CLKID_SARADC_DIV]			= &t6x_saradc_div.hw,
		[CLKID_SARADC]				= &t6x_saradc.hw,
		[CLKID_CECB_32K_CLKIN]			= &t6x_cecb_32k_clkin.hw,
		[CLKID_CECB_32K_DIV]			= &t6x_cecb_32k_div.hw,
		[CLKID_CECB_32K_SEL_PRE]		= &t6x_cecb_32k_sel_pre.hw,
		[CLKID_CECB_32K_SEL]			= &t6x_cecb_32k_sel.hw,
		[CLKID_CECB_32K_CLKOUT]			= &t6x_cecb_32k_clkout.hw,
		[CLKID_SC_CLK_SEL]			= &t6x_sc_clk_sel.hw,
		[CLKID_SC_CLK_DIV]			= &t6x_sc_clk_div.hw,
		[CLKID_SC_CLK]				= &t6x_sc_clk.hw,
		[CLKID_24M_CLK_GATE]			= &t6x_24m_clk_gate.hw,
		[CLKID_24M_DIV2]			= &t6x_24m_div2.hw,
		[CLKID_12M_CLK]				= &t6x_12m_clk.hw,
		[CLKID_25M_CLK_DIV]			= &t6x_25m_clk_div.hw,
		[CLKID_25M_CLK]				= &t6x_25m_clk.hw,
		[CLKID_VCLK_SEL]			= &t6x_vclk_sel.hw,
		[CLKID_VCLK2_SEL]			= &t6x_vclk2_sel.hw,
		[CLKID_VCLK_INPUT]			= &t6x_vclk_input.hw,
		[CLKID_VCLK2_INPUT]			= &t6x_vclk2_input.hw,
		[CLKID_VCLK_DIV]			= &t6x_vclk_div.hw,
		[CLKID_VCLK2_DIV]			= &t6x_vclk2_div.hw,
		[CLKID_VCLK]				= &t6x_vclk.hw,
		[CLKID_VCLK2]				= &t6x_vclk2.hw,
		[CLKID_VCLK_DIV1]			= &t6x_vclk_div1.hw,
		[CLKID_VCLK_DIV2_EN]			= &t6x_vclk_div2_en.hw,
		[CLKID_VCLK_DIV4_EN]			= &t6x_vclk_div4_en.hw,
		[CLKID_VCLK_DIV6_EN]			= &t6x_vclk_div6_en.hw,
		[CLKID_VCLK_DIV12_EN]			= &t6x_vclk_div12_en.hw,
		[CLKID_VCLK2_DIV1]			= &t6x_vclk2_div1.hw,
		[CLKID_VCLK2_DIV2_EN]			= &t6x_vclk2_div2_en.hw,
		[CLKID_VCLK2_DIV4_EN]			= &t6x_vclk2_div4_en.hw,
		[CLKID_VCLK2_DIV6_EN]			= &t6x_vclk2_div6_en.hw,
		[CLKID_VCLK2_DIV12_EN]			= &t6x_vclk2_div12_en.hw,
		[CLKID_VCLK_DIV2]			= &t6x_vclk_div2.hw,
		[CLKID_VCLK_DIV4]			= &t6x_vclk_div4.hw,
		[CLKID_VCLK_DIV6]			= &t6x_vclk_div6.hw,
		[CLKID_VCLK_DIV12]			= &t6x_vclk_div12.hw,
		[CLKID_VCLK2_DIV2]			= &t6x_vclk2_div2.hw,
		[CLKID_VCLK2_DIV4]			= &t6x_vclk2_div4.hw,
		[CLKID_VCLK2_DIV6]			= &t6x_vclk2_div6.hw,
		[CLKID_VCLK2_DIV12]			= &t6x_vclk2_div12.hw,
		[CLKID_CTS_ENCI_SEL]			= &t6x_cts_enci_sel.hw,
		[CLKID_CTS_VDAC_SEL]			= &t6x_cts_vdac_sel.hw,
		[CLKID_HDMI_TX_SEL]			= &t6x_hdmi_tx_sel.hw,
		[CLKID_CTS_ENCI]			= &t6x_cts_enci.hw,
		[CLKID_CTS_ENCP]			= &t6x_cts_encp.hw,
		[CLKID_CTS_VDAC]			= &t6x_cts_vdac.hw,
		[CLKID_HDMI_TX]				= &t6x_hdmi_tx.hw,
		[CLKID_MALI_0_SEL]			= &t6x_mali_0_sel.hw,
		[CLKID_MALI_0_DIV]			= &t6x_mali_0_div.hw,
		[CLKID_MALI_0]				= &t6x_mali_0.hw,
		[CLKID_MALI_1_SEL]			= &t6x_mali_1_sel.hw,
		[CLKID_MALI_1_DIV]			= &t6x_mali_1_div.hw,
		[CLKID_MALI_1]				= &t6x_mali_1.hw,
		[CLKID_MALI]				= &t6x_mali.hw,
		[CLKID_HEVCB_0_SEL]			= &t6x_hevcb_0_sel.hw,
		[CLKID_HEVCB_0_DIV]			= &t6x_hevcb_0_div.hw,
		[CLKID_HEVCB_0]				= &t6x_hevcb_0.hw,
		[CLKID_HEVCB_1_SEL]			= &t6x_hevcb_1_sel.hw,
		[CLKID_HEVCB_1_DIV]			= &t6x_hevcb_1_div.hw,
		[CLKID_HEVCB_1]				= &t6x_hevcb_1.hw,
		[CLKID_HEVCB]				= &t6x_hevcb.hw,
		[CLKID_HCODEC_0_SEL]			= &t6x_hcodec_0_sel.hw,
		[CLKID_HCODEC_0_DIV]			= &t6x_hcodec_0_div.hw,
		[CLKID_HCODEC_0]			= &t6x_hcodec_0.hw,
		[CLKID_HCODEC_1_SEL]			= &t6x_hcodec_1_sel.hw,
		[CLKID_HCODEC_1_DIV]			= &t6x_hcodec_1_div.hw,
		[CLKID_HCODEC_1]			= &t6x_hcodec_1.hw,
		[CLKID_HCODEC]				= &t6x_hcodec.hw,
		[CLKID_VPU_0_SEL]			= &t6x_vpu_0_sel.hw,
		[CLKID_VPU_0_DIV]			= &t6x_vpu_0_div.hw,
		[CLKID_VPU_0]				= &t6x_vpu_0.hw,
		[CLKID_VPU_1_SEL]			= &t6x_vpu_1_sel.hw,
		[CLKID_VPU_1_DIV]			= &t6x_vpu_1_div.hw,
		[CLKID_VPU_1]				= &t6x_vpu_1.hw,
		[CLKID_VPU]				= &t6x_vpu.hw,
		[CLKID_VAPB_0_SEL]			= &t6x_vapb_0_sel.hw,
		[CLKID_VAPB_0_DIV]			= &t6x_vapb_0_div.hw,
		[CLKID_VAPB_0]				= &t6x_vapb_0.hw,
		[CLKID_VAPB_1_SEL]			= &t6x_vapb_1_sel.hw,
		[CLKID_VAPB_1_DIV]			= &t6x_vapb_1_div.hw,
		[CLKID_VAPB_1]				= &t6x_vapb_1.hw,
		[CLKID_VAPB]				= &t6x_vapb.hw,
		[CLKID_DAE_0_SEL]			= &t6x_dae_0_sel.hw,
		[CLKID_DAE_0_DIV]			= &t6x_dae_0_div.hw,
		[CLKID_DAE_0]				= &t6x_dae_0.hw,
		[CLKID_DAE_1_SEL]			= &t6x_dae_1_sel.hw,
		[CLKID_DAE_1_DIV]			= &t6x_dae_1_div.hw,
		[CLKID_DAE_1]				= &t6x_dae_1.hw,
		[CLKID_DAE]				= &t6x_dae.hw,
		[CLKID_DPE_0_SEL]			= &t6x_dpe_0_sel.hw,
		[CLKID_DPE_0_DIV]			= &t6x_dpe_0_div.hw,
		[CLKID_DPE_0]				= &t6x_dpe_0.hw,
		[CLKID_DPE_1_SEL]			= &t6x_dpe_1_sel.hw,
		[CLKID_DPE_1_DIV]			= &t6x_dpe_1_div.hw,
		[CLKID_DPE_1]				= &t6x_dpe_1.hw,
		[CLKID_DPE]				= &t6x_dpe.hw,
		[CLKID_GE2D_GATE]			= &t6x_ge2d_gate.hw,
		[CLKID_HDMIRX_5M_SEL]			= &t6x_hdmirx_5m_sel.hw,
		[CLKID_HDMIRX_5M_DIV]			= &t6x_hdmirx_5m_div.hw,
		[CLKID_HDMIRX_5M]			= &t6x_hdmirx_5m.hw,
		[CLKID_HDMIRX_2M_SEL]			= &t6x_hdmirx_2m_sel.hw,
		[CLKID_HDMIRX_2M_DIV]			= &t6x_hdmirx_2m_div.hw,
		[CLKID_HDMIRX_2M]			= &t6x_hdmirx_2m.hw,
		[CLKID_HDMIRX_CFG_SEL]			= &t6x_hdmirx_cfg_sel.hw,
		[CLKID_HDMIRX_CFG_DIV]			= &t6x_hdmirx_cfg_div.hw,
		[CLKID_HDMIRX_CFG]			= &t6x_hdmirx_cfg.hw,
		[CLKID_HDMIRX_HDCP_SEL]			= &t6x_hdmirx_hdcp_sel.hw,
		[CLKID_HDMIRX_HDCP_DIV]			= &t6x_hdmirx_hdcp_div.hw,
		[CLKID_HDMIRX_HDCP]			= &t6x_hdmirx_hdcp.hw,
		[CLKID_HDMIRX_AUD_PLL_SEL]		= &t6x_hdmirx_aud_pll_sel.hw,
		[CLKID_HDMIRX_AUD_PLL_DIV]		= &t6x_hdmirx_aud_pll_div.hw,
		[CLKID_HDMIRX_AUD_PLL]			= &t6x_hdmirx_aud_pll.hw,
		[CLKID_HDMIRX_ACR_SEL]			= &t6x_hdmirx_acr_sel.hw,
		[CLKID_HDMIRX_ACR_DIV]			= &t6x_hdmirx_acr_div.hw,
		[CLKID_HDMIRX_ACR]			= &t6x_hdmirx_acr.hw,
		[CLKID_HDMIRX_METER_SEL]		= &t6x_hdmirx_meter_sel.hw,
		[CLKID_HDMIRX_METER_DIV]		= &t6x_hdmirx_meter_div.hw,
		[CLKID_HDMIRX_METER]			= &t6x_hdmirx_meter.hw,
		[CLKID_PWM_C_SEL]			= &t6x_pwm_c_sel.hw,
		[CLKID_PWM_C_DIV]			= &t6x_pwm_c_div.hw,
		[CLKID_PWM_C]				= &t6x_pwm_c.hw,
		[CLKID_PWM_D_SEL]			= &t6x_pwm_d_sel.hw,
		[CLKID_PWM_D_DIV]			= &t6x_pwm_d_div.hw,
		[CLKID_PWM_D]				= &t6x_pwm_d.hw,
		[CLKID_PWM_E_SEL]			= &t6x_pwm_e_sel.hw,
		[CLKID_PWM_E_DIV]			= &t6x_pwm_e_div.hw,
		[CLKID_PWM_E]				= &t6x_pwm_e.hw,
		[CLKID_PWM_F_SEL]			= &t6x_pwm_f_sel.hw,
		[CLKID_PWM_F_DIV]			= &t6x_pwm_f_div.hw,
		[CLKID_PWM_F]				= &t6x_pwm_f.hw,
		[CLKID_PWM_G_SEL]			= &t6x_pwm_g_sel.hw,
		[CLKID_PWM_G_DIV]			= &t6x_pwm_g_div.hw,
		[CLKID_PWM_G]				= &t6x_pwm_g.hw,
		[CLKID_PWM_H_SEL]			= &t6x_pwm_h_sel.hw,
		[CLKID_PWM_H_DIV]			= &t6x_pwm_h_div.hw,
		[CLKID_PWM_H]				= &t6x_pwm_h.hw,
		[CLKID_PWM_I_SEL]			= &t6x_pwm_i_sel.hw,
		[CLKID_PWM_I_DIV]			= &t6x_pwm_i_div.hw,
		[CLKID_PWM_I]				= &t6x_pwm_i.hw,
		[CLKID_PWM_J_SEL]			= &t6x_pwm_j_sel.hw,
		[CLKID_PWM_J_DIV]			= &t6x_pwm_j_div.hw,
		[CLKID_PWM_J]				= &t6x_pwm_j.hw,
		[CLKID_SPICC0_SEL]			= &t6x_spicc0_sel.hw,
		[CLKID_SPICC0_DIV]			= &t6x_spicc0_div.hw,
		[CLKID_SPICC0]				= &t6x_spicc0.hw,
		[CLKID_SPICC1_SEL]			= &t6x_spicc1_sel.hw,
		[CLKID_SPICC1_DIV]			= &t6x_spicc1_div.hw,
		[CLKID_SPICC1]				= &t6x_spicc1.hw,
		[CLKID_SPICC2_SEL]			= &t6x_spicc2_sel.hw,
		[CLKID_SPICC2_DIV]			= &t6x_spicc2_div.hw,
		[CLKID_SPICC2]				= &t6x_spicc2.hw,
		[CLKID_SPICC3_SEL]			= &t6x_spicc3_sel.hw,
		[CLKID_SPICC3_DIV]			= &t6x_spicc3_div.hw,
		[CLKID_SPICC3]				= &t6x_spicc3.hw,
		[CLKID_SPICC4_SEL]			= &t6x_spicc4_sel.hw,
		[CLKID_SPICC4_DIV]			= &t6x_spicc4_div.hw,
		[CLKID_SPICC4]				= &t6x_spicc4.hw,
		[CLKID_SPICC5_SEL]			= &t6x_spicc5_sel.hw,
		[CLKID_SPICC5_DIV]			= &t6x_spicc5_div.hw,
		[CLKID_SPICC5]				= &t6x_spicc5.hw,
		[CLKID_BCON_SEL]			= &t6x_bcon_sel.hw,
		[CLKID_BCON_DIV]			= &t6x_bcon_div.hw,
		[CLKID_BCON]				= &t6x_bcon.hw,
		[CLKID_SD_EMMC_C_SEL]			= &t6x_sd_emmc_c_sel.hw,
		[CLKID_SD_EMMC_C_DIV]			= &t6x_sd_emmc_c_div.hw,
		[CLKID_SD_EMMC_C]			= &t6x_sd_emmc_c.hw,
		[CLKID_VDIN_MEAS_SEL]			= &t6x_vdin_meas_sel.hw,
		[CLKID_VDIN_MEAS_DIV]			= &t6x_vdin_meas_div.hw,
		[CLKID_VDIN_MEAS]			= &t6x_vdin_meas.hw,
		[CLKID_VID_LOCK_DIV]			= &t6x_vid_lock_div.hw,
		[CLKID_VID_LOCK]			= &t6x_vid_lock_clk.hw,
		[CLKID_ETH_RMII_SEL]			= &t6x_eth_rmii_sel.hw,
		[CLKID_ETH_RMII_DIV]			= &t6x_eth_rmii_div.hw,
		[CLKID_ETH_RMII]			= &t6x_eth_rmii.hw,
		[CLKID_ETH_DIV8]			= &t6x_eth_div8.hw,
		[CLKID_ETH_125M]			= &t6x_eth_125m.hw,
		[CLKID_TS_CLK_DIV]			= &t6x_ts_clk_div.hw,
		[CLKID_TS_CLK]				= &t6x_ts_clk.hw,
		[CLKID_CTS_TCON_PLL_CLK_SEL]		= &t6x_cts_tcon_pll_clk_sel.hw,
		[CLKID_CTS_TCON_PLL_CLK_DIV]		= &t6x_cts_tcon_pll_clk_div.hw,
		[CLKID_CTS_TCON_PLL_CLK]		= &t6x_cts_tcon_pll_clk.hw,
		[CLKID_ADC_EXTCLK_SEL]			= &t6x_adc_extclk_sel.hw,
		[CLKID_ADC_EXTCLK_DIV]			= &t6x_adc_extclk_div.hw,
		[CLKID_ADC_EXTCLK]			= &t6x_adc_extclk.hw,
		[CLKID_DEMOD_32K_CLKIN]			= &t6x_demod_32k_clkin.hw,
		[CLKID_DEMOD_32K_DIV]			= &t6x_demod_32k_div.hw,
		[CLKID_DEMOD_32K]			= &t6x_demod_32k.hw,
		[CLKID_CTS_DEMOD_CORE_SEL]		= &t6x_cts_demod_core_sel.hw,
		[CLKID_CTS_DEMOD_CORE_DIV]		= &t6x_cts_demod_core_div.hw,
		[CLKID_CTS_DEMOD_CORE]			= &t6x_cts_demod_core.hw,
		[CLKID_CTS_DEMOD_CORE_T2_SEL]		= &t6x_cts_demod_core_t2_clk_sel.hw,
		[CLKID_CTS_DEMOD_CORE_T2_DIV]		= &t6x_cts_demod_core_t2_clk_div.hw,
		[CLKID_CTS_DEMOD_CORE_T2]		= &t6x_cts_demod_core_t2_clk.hw,
		[CLKID_CDAC_CLK_SEL]			= &t6x_cdac_sel.hw,
		[CLKID_CDAC_CLK_DIV]			= &t6x_cdac_div.hw,
		[CLKID_CDAC_CLK]			= &t6x_cdac.hw,
		[CLKID_TSIN_0_SEL]			= &t6x_tsin_0_sel.hw,
		[CLKID_TSIN_0_DIV]			= &t6x_tsin_0_div.hw,
		[CLKID_TSIN_0]				= &t6x_tsin_0.hw,
		[CLKID_TSIN_1_SEL]			= &t6x_tsin_1_sel.hw,
		[CLKID_TSIN_1_DIV]			= &t6x_tsin_1_div.hw,
		[CLKID_TSIN_1]				= &t6x_tsin_1.hw,
		[CLKID_TSIN_SEL]			= &t6x_tsin_sel.hw,
		[CLKID_TSIN]				= &t6x_tsin.hw,
		[CLKID_VAFE_SEL]			= &t6x_vafe_sel.hw,
		[CLKID_VAFE_DIV]			= &t6x_vafe_div.hw,
		[CLKID_VAFE]				= &t6x_vafe.hw,
		[CLKID_ATV_DMD_SEL]			= &t6x_atv_dmd_sel.hw,
		[CLKID_ATV_DMD_DIV]			= &t6x_atv_dmd_div.hw,
		[CLKID_ATV_DMD]				= &t6x_atv_dmd.hw,
		[CLKID_AMFC_0_SEL]			= &t6x_amfc_0_sel.hw,
		[CLKID_AMFC_0_DIV]			= &t6x_amfc_0_div.hw,
		[CLKID_AMFC_0]				= &t6x_amfc_0.hw,
		[CLKID_AMFC_1_SEL]			= &t6x_amfc_1_sel.hw,
		[CLKID_AMFC_1_DIV]			= &t6x_amfc_1_div.hw,
		[CLKID_AMFC_1]				= &t6x_amfc_1.hw,
		[CLKID_AMFC]				= &t6x_amfc.hw,
		[CLKID_USB2_48M_PRE_IN]			= &t6x_usb2_48m_pre_in.hw,
		[CLKID_USB2_48M_PRE_DIV]		= &t6x_usb2_48m_pre_div.hw,
		[CLKID_USB2_48M_PRE_FORCE_SEL]		= &t6x_usb2_48m_pre_force_sel.hw,
		[CLKID_USB2_48M_PRE]			= &t6x_usb2_48m_pre.hw,
		[CLKID_USB2_48M_CLK_TMP_SEL]		= &t6x_usb2_48m_clk_tmp_sel.hw,
		[CLKID_USB2_48M_CLK_TMP_DIV]		= &t6x_usb2_48m_clk_tmp_div.hw,
		[CLKID_USB2_48M_CLK_TMP]		= &t6x_usb2_48m_clk_tmp.hw,
		[CLKID_USB2_48M_CLK]			= &t6x_usb2_48m_clk.hw,
		[CLKID_USB3_250M_SEL]			= &t6x_usb3_250m_sel.hw,
		[CLKID_USB3_250M_DIV]			= &t6x_usb3_250m_div.hw,
		[CLKID_USB3_250M]			= &t6x_usb3_250m.hw,
		[CLKID_GEN_SEL]				= &t6x_gen_sel.hw,
		[CLKID_GEN_DIV]				= &t6x_gen_div.hw,
		[CLKID_GEN]				= &t6x_gen.hw,
		[CLKID_DSC_CLK_SEL]			= &t6x_dsc_clk_sel.hw,
		[CLKID_DSC_CLK_DIV]			= &t6x_dsc_clk_div.hw,
		[CLKID_DSC_CLK]				= &t6x_dsc_clk.hw,
		[CLKID_DSC_POST_SEL]			= &t6x_dsc_post_sel.hw,
		[CLKID_SYS_DOS]				= &t6x_sys_dos.hw,
		[CLKID_SYS_DEMOD]			= &t6x_sys_demod.hw,
		[CLKID_SYS_AMFC]			= &t6x_sys_amfc.hw,
		[CLKID_SYS_IOTM]			= &t6x_sys_iotm.hw,
		[CLKID_SYS_DDRTEST]			= &t6x_sys_ddrtest.hw,
		[CLKID_SYS_AM2AXI]			= &t6x_sys_am2axi1.hw,
		[CLKID_SYS_MSR_CLK]			= &t6x_sys_msr_clk.hw,
		[CLKID_SYS_AUDIO]			= &t6x_sys_audio.hw,
		[CLKID_SYS_TVFE]			= &t6x_sys_tvfe.hw,
		[CLKID_SYS_ETHMAC]			= &t6x_sys_ethmac.hw,
		[CLKID_SYS_ETHPHY]			= &t6x_sys_ethphy.hw,
		[CLKID_SYS_ETHAXI]			= &t6x_sys_ethaxi.hw,
		[CLKID_SYS_GE2D]			= &t6x_sys_ge2d.hw,
		[CLKID_SYS_I2C_MONITOR]			= &t6x_sys_i2c_monitor.hw,
		[CLKID_SYS_USB_U22H]			= &t6x_sys_usb_u22h.hw,
		[CLKID_SYS_USB_U2DRD]			= &t6x_sys_usb_u2drd.hw,
		[CLKID_SYS_USB_U2H]			= &t6x_sys_usb_u2h.hw,
		[CLKID_SYS_HDMIRX20_AES]		= &t6x_sys_hdmirx20_aes.hw,
		[CLKID_SYS_HDMIRX]			= &t6x_sys_hdmirx.hw,
		[CLKID_SYS_MMC_WRAP]			= &t6x_sys_mmc_wrap.hw,
		[CLKID_SYS_CPU_SUBSYS]			= &t6x_sys_cpu_subsys.hw,
		[CLKID_SYS_VPU_INTR]			= &t6x_sys_vpu_intr.hw,
		[CLKID_SYS_TCON]			= &t6x_sys_tcon.hw,
		[CLKID_SYS_SARADC]			= &t6x_sys_saradc.hw,
		[CLKID_SYS_AOCPU]			= &t6x_sys_aocpu.hw,
		[CLKID_SYS_GIC]				= &t6x_sys_gic.hw,
		[CLKID_SYS_SPISG]			= &t6x_sys_spisg.hw,
		[CLKID_SYS_SD_EMMC_C]			= &t6x_sys_sd_emmc_c.hw,
		[CLKID_SYS_AUCPU]			= &t6x_sys_aucpu.hw,
		[CLKID_SYS_UART_WRAPPER]		= &t6x_sys_uart_wrapper.hw,
		[CLKID_SYS_PWM_WRAPPER]			= &t6x_sys_pwm_wrapper.hw,
		[CLKID_SYS_I2C_M_WRAPPER]		= &t6x_sys_i2c_m_wrapper.hw,
		[CLKID_SYS_MAILBOX]			= &t6x_sys_mailbox.hw,
		[CLKID_SYS_IR_CTRL]			= &t6x_sys_ir_crtl.hw,
		[CLKID_SYS_SMART_CARD]			= &t6x_sys_smart_card.hw,
		[CLKID_SYS_CIPLUS]			= &t6x_sys_ciplus.hw,
		[CLKID_SYS_CEC]				= &t6x_sys_cec.hw,
		[CLKID_SYS_TS_CPU]			= &t6x_sys_ts_cpu.hw,
		[CLKID_SYS_TS_TOP]			= &t6x_sys_ts_top.hw,
		[CLKID_SYS_LED_CTRL]			= &t6x_sys_led_ctrl.hw,
		[CLKID_SYS_I2C_TCON]			= &t6x_sys_i2c_tcon.hw,
		[NR_CLKS]				= NULL
	},
	.num = NR_CLKS,
};

/* Convenience table to populate regmap in .probe */
static struct clk_regmap *const t6x_clk_regmaps[] = {
	&t6x_rtc_32k_in,
	&t6x_rtc_32k_div,
	&t6x_rtc_32k_force_sel,
	&t6x_rtc_32k_out,
	&t6x_rtc_32k_mux0_0,
	&t6x_rtc_32k_mux0_1,
	&t6x_rtc_clk,
	&t6x_sys_clk_0_sel,
	&t6x_sys_clk_0_div,
	&t6x_sys_clk_0,
	&t6x_sys_clk_1_sel,
	&t6x_sys_clk_1_div,
	&t6x_sys_clk_1,
	&t6x_sys_clk,
	&t6x_axi_clk_0_sel,
	&t6x_axi_clk_0_div,
	&t6x_axi_clk_0,
	&t6x_axi_clk_1_sel,
	&t6x_axi_clk_1_div,
	&t6x_axi_clk_1,
	&t6x_axi_clk,
	&t6x_mdcsys_clk_0_sel,
	&t6x_mdcsys_clk_0_div,
	&t6x_mdcsys_clk_0,
	&t6x_mdcsys_clk_1_sel,
	&t6x_mdcsys_clk_1_div,
	&t6x_mdcsys_clk_1,
	&t6x_mdcsys_clk,
	&t6x_saradc_sel,
	&t6x_saradc_div,
	&t6x_saradc,
	&t6x_cecb_32k_clkin,
	&t6x_cecb_32k_div,
	&t6x_cecb_32k_sel_pre,
	&t6x_cecb_32k_sel,
	&t6x_cecb_32k_clkout,
	&t6x_sc_clk_sel,
	&t6x_sc_clk_div,
	&t6x_sc_clk,
	&t6x_vclk_sel,
	&t6x_vclk2_sel,
	&t6x_vclk_input,
	&t6x_vclk2_input,
	&t6x_vclk_div,
	&t6x_vclk2_div,
	&t6x_vclk,
	&t6x_vclk2,
	&t6x_vclk_div1,
	&t6x_vclk_div2_en,
	&t6x_vclk_div4_en,
	&t6x_vclk_div6_en,
	&t6x_vclk_div12_en,
	&t6x_vclk2_div1,
	&t6x_vclk2_div2_en,
	&t6x_vclk2_div4_en,
	&t6x_vclk2_div6_en,
	&t6x_vclk2_div12_en,
	&t6x_cts_enci_sel,
	&t6x_cts_vdac_sel,
	&t6x_hdmi_tx_sel,
	&t6x_cts_enci,
	&t6x_cts_encp,
	&t6x_cts_vdac,
	&t6x_hdmi_tx,
	&t6x_mali_0_sel,
	&t6x_mali_0_div,
	&t6x_mali_0,
	&t6x_mali_1_sel,
	&t6x_mali_1_div,
	&t6x_mali_1,
	&t6x_mali,
	&t6x_hevcb_0_sel,
	&t6x_hevcb_0_div,
	&t6x_hevcb_0,
	&t6x_hevcb_1_sel,
	&t6x_hevcb_1_div,
	&t6x_hevcb_1,
	&t6x_hevcb,
	&t6x_hcodec_0_sel,
	&t6x_hcodec_0_div,
	&t6x_hcodec_0,
	&t6x_hcodec_1_sel,
	&t6x_hcodec_1_div,
	&t6x_hcodec_1,
	&t6x_hcodec,
	&t6x_vpu_0_sel,
	&t6x_vpu_0_div,
	&t6x_vpu_0,
	&t6x_vpu_1_sel,
	&t6x_vpu_1_div,
	&t6x_vpu_1,
	&t6x_vpu,
	&t6x_vapb_0_sel,
	&t6x_vapb_0_div,
	&t6x_vapb_0,
	&t6x_vapb_1_sel,
	&t6x_vapb_1_div,
	&t6x_vapb_1,
	&t6x_vapb,
	&t6x_dae_0_sel,
	&t6x_dae_0_div,
	&t6x_dae_0,
	&t6x_dae_1_sel,
	&t6x_dae_1_div,
	&t6x_dae_1,
	&t6x_dae,
	&t6x_dpe_0_sel,
	&t6x_dpe_0_div,
	&t6x_dpe_0,
	&t6x_dpe_1_sel,
	&t6x_dpe_1_div,
	&t6x_dpe_1,
	&t6x_dpe,
	&t6x_ge2d_gate,
	&t6x_hdmirx_5m_sel,
	&t6x_hdmirx_5m_div,
	&t6x_hdmirx_5m,
	&t6x_hdmirx_2m_sel,
	&t6x_hdmirx_2m_div,
	&t6x_hdmirx_2m,
	&t6x_hdmirx_cfg_sel,
	&t6x_hdmirx_cfg_div,
	&t6x_hdmirx_cfg,
	&t6x_hdmirx_hdcp_sel,
	&t6x_hdmirx_hdcp_div,
	&t6x_hdmirx_hdcp,
	&t6x_hdmirx_aud_pll_sel,
	&t6x_hdmirx_aud_pll_div,
	&t6x_hdmirx_aud_pll,
	&t6x_hdmirx_acr_sel,
	&t6x_hdmirx_acr_div,
	&t6x_hdmirx_acr,
	&t6x_hdmirx_meter_sel,
	&t6x_hdmirx_meter_div,
	&t6x_hdmirx_meter,
	&t6x_pwm_c_sel,
	&t6x_pwm_c_div,
	&t6x_pwm_c,
	&t6x_pwm_d_sel,
	&t6x_pwm_d_div,
	&t6x_pwm_d,
	&t6x_pwm_e_sel,
	&t6x_pwm_e_div,
	&t6x_pwm_e,
	&t6x_pwm_f_sel,
	&t6x_pwm_f_div,
	&t6x_pwm_f,
	&t6x_pwm_g_sel,
	&t6x_pwm_g_div,
	&t6x_pwm_g,
	&t6x_pwm_h_sel,
	&t6x_pwm_h_div,
	&t6x_pwm_h,
	&t6x_pwm_i_sel,
	&t6x_pwm_i_div,
	&t6x_pwm_i,
	&t6x_pwm_j_sel,
	&t6x_pwm_j_div,
	&t6x_pwm_j,
	&t6x_spicc0_sel,
	&t6x_spicc0_div,
	&t6x_spicc0,
	&t6x_spicc1_sel,
	&t6x_spicc1_div,
	&t6x_spicc1,
	&t6x_spicc2_sel,
	&t6x_spicc2_div,
	&t6x_spicc2,
	&t6x_spicc3_sel,
	&t6x_spicc3_div,
	&t6x_spicc3,
	&t6x_spicc4_sel,
	&t6x_spicc4_div,
	&t6x_spicc4,
	&t6x_spicc5_sel,
	&t6x_spicc5_div,
	&t6x_spicc5,
	&t6x_bcon_sel,
	&t6x_bcon_div,
	&t6x_bcon,
	&t6x_sd_emmc_c_sel,
	&t6x_sd_emmc_c_div,
	&t6x_sd_emmc_c,
	&t6x_vdin_meas_sel,
	&t6x_vdin_meas_div,
	&t6x_vdin_meas,
	&t6x_vid_lock_div,
	&t6x_vid_lock_clk,
	&t6x_eth_rmii_sel,
	&t6x_eth_rmii_div,
	&t6x_eth_rmii,
	&t6x_eth_125m,
	&t6x_ts_clk_div,
	&t6x_ts_clk,
	&t6x_amfc_0_sel,
	&t6x_amfc_0_div,
	&t6x_amfc_0,
	&t6x_amfc_1_sel,
	&t6x_amfc_1_div,
	&t6x_amfc_1,
	&t6x_amfc,
	&t6x_usb2_48m_pre_in,
	&t6x_usb2_48m_pre_div,
	&t6x_usb2_48m_pre_force_sel,
	&t6x_usb2_48m_pre,
	&t6x_usb2_48m_clk_tmp_sel,
	&t6x_usb2_48m_clk_tmp_div,
	&t6x_usb2_48m_clk_tmp,
	&t6x_usb2_48m_clk,
	&t6x_usb3_250m_sel,
	&t6x_usb3_250m_div,
	&t6x_usb3_250m,
	&t6x_dsc_clk_sel,
	&t6x_dsc_clk_div,
	&t6x_dsc_clk,
	&t6x_dsc_post_sel,
	&t6x_cts_tcon_pll_clk_sel,
	&t6x_cts_tcon_pll_clk_div,
	&t6x_cts_tcon_pll_clk,
	&t6x_adc_extclk_sel,
	&t6x_adc_extclk_div,
	&t6x_adc_extclk,
	&t6x_demod_32k_clkin,
	&t6x_demod_32k_div,
	&t6x_demod_32k,
	&t6x_cts_demod_core_sel,
	&t6x_cts_demod_core_div,
	&t6x_cts_demod_core,
	&t6x_cts_demod_core_t2_clk_sel,
	&t6x_cts_demod_core_t2_clk_div,
	&t6x_cts_demod_core_t2_clk,
	&t6x_cdac_sel,
	&t6x_cdac_div,
	&t6x_cdac,
	&t6x_tsin_0_sel,
	&t6x_tsin_0_div,
	&t6x_tsin_0,
	&t6x_tsin_1_sel,
	&t6x_tsin_1_div,
	&t6x_tsin_1,
	&t6x_tsin_sel,
	&t6x_tsin,
	&t6x_vafe_sel,
	&t6x_vafe_div,
	&t6x_vafe,
	&t6x_atv_dmd_sel,
	&t6x_atv_dmd_div,
	&t6x_atv_dmd,
	&t6x_gen_sel,
	&t6x_gen_div,
	&t6x_gen,
	&t6x_24m_clk_gate,
	&t6x_12m_clk,
	&t6x_25m_clk_div,
	&t6x_25m_clk,
	&t6x_sys_dos,
	&t6x_sys_demod,
	&t6x_sys_amfc,
	&t6x_sys_iotm,
	&t6x_sys_ddrtest,
	&t6x_sys_am2axi1,
	&t6x_sys_msr_clk,
	&t6x_sys_audio,
	&t6x_sys_tvfe,
	&t6x_sys_ethmac,
	&t6x_sys_ethphy,
	&t6x_sys_ethaxi,
	&t6x_sys_ge2d,
	&t6x_sys_i2c_monitor,
	&t6x_sys_usb_u22h,
	&t6x_sys_usb_u2drd,
	&t6x_sys_usb_u2h,
	&t6x_sys_hdmirx20_aes,
	&t6x_sys_hdmirx,
	&t6x_sys_mmc_wrap,
	&t6x_sys_cpu_subsys,
	&t6x_sys_vpu_intr,
	&t6x_sys_tcon,
	&t6x_sys_saradc,
	&t6x_sys_aocpu,
	&t6x_sys_gic,
	&t6x_sys_spisg,
	&t6x_sys_sd_emmc_c,
	&t6x_sys_aucpu,
	&t6x_sys_uart_wrapper,
	&t6x_sys_pwm_wrapper,
	&t6x_sys_i2c_m_wrapper,
	&t6x_sys_mailbox,
	&t6x_sys_ir_crtl,
	&t6x_sys_smart_card,
	&t6x_sys_ciplus,
	&t6x_sys_cec,
	&t6x_sys_ts_cpu,
	&t6x_sys_ts_top,
	&t6x_sys_led_ctrl,
	&t6x_sys_i2c_tcon,
};

static struct clk_regmap *const t6x_pll_clk_regmaps[] = {
	&t6x_fixed_pll,
	&t6x_fclk_div2,
	&t6x_fclk_div3,
	&t6x_fclk_div4,
	&t6x_fclk_div5,
	&t6x_fclk_div7,
	&t6x_fclk_div2p5,
	&t6x_gp0_pll,
	&t6x_gp1_pll,
	&t6x_gp2_pll,
	&t6x_hifi_pll,
	&t6x_hifi1_pll,
	&t6x_mpll_50m,
};

struct t6x_vpu_nb_data {
	struct notifier_block nb;
	struct clk_hw *mali;
	struct clk_hw *hevcb;
};

static int t6x_vpu_notifier_cb(struct notifier_block *nb,
				unsigned long event, void *data)
{
	struct t6x_vpu_nb_data *nb_data = container_of(nb, struct t6x_vpu_nb_data, nb);
	struct clk_notifier_data *ndata = data;
	unsigned long fdiv2p5_rate = clk_hw_get_rate(&t6x_fclk_div2p5.hw);
	unsigned long mali_rate = clk_hw_get_rate(nb_data->mali);
	unsigned long hevcb_rate = clk_hw_get_rate(nb_data->hevcb);
	unsigned long nrate = ndata->new_rate;
	static unsigned long mali_rate_saved, hevcb_rate_saved;

	switch (event) {
	case PRE_RATE_CHANGE:
		/*
		 * set vpu 800Mhz --> set gp1 1776Mhz --> set vpu 888Mz
		 * If mali used gp1 1680Mhz, gp1 can not set to 1776Mhz since
		 * gp1 is protected.
		 * Migrate mali to fclk2p5 when setting vpu equals 800Mhz
		 */
		if (mali_rate > fdiv2p5_rate) {
			clk_set_rate(nb_data->mali->clk, fdiv2p5_rate);
			mali_rate_saved = mali_rate;
		}
		if (hevcb_rate > fdiv2p5_rate) {
			clk_set_rate(nb_data->hevcb->clk, fdiv2p5_rate);
			hevcb_rate_saved = hevcb_rate;
		}

		return NOTIFY_OK;
	case POST_RATE_CHANGE:
		if (nrate == 888000000) {
			/*
			 * mali and hevcb can not switch to gp1 pll now,
			 * their max rate is fdiv2p5
			 */
			clk_hw_set_rate_range(nb_data->mali, 0, fdiv2p5_rate);
			clk_hw_set_rate_range(nb_data->hevcb, 0, fdiv2p5_rate);
		}
		if (nrate == 840000000) {
			/* Restore mali and hevcb maximum value limit */
			clk_hw_set_rate_range(nb_data->mali, 0, nrate);
			clk_hw_set_rate_range(nb_data->hevcb, 0, nrate);
			/* update mali to max, mali may changed by scaling frequency  */
			if (mali_rate == fdiv2p5_rate && mali_rate_saved > 0)
				clk_set_rate(nb_data->mali->clk, mali_rate_saved);
			/* restore hevcb rate if the saved rate > 800Mhz */
			if (hevcb_rate_saved > fdiv2p5_rate)
				clk_set_rate(nb_data->hevcb->clk, hevcb_rate_saved);
		}

		return NOTIFY_OK;

	default:
		return NOTIFY_DONE;
	}
}

static struct t6x_vpu_nb_data t6x_vpu_nb_data = {
	.nb.notifier_call = t6x_vpu_notifier_cb,
	.mali = &t6x_mali.hw,
	.hevcb = &t6x_hevcb.hw,
};

static const char * const scmi_clk_name[] = {"cpu_clk", "dsu_clk", "aclkm",
					     "pwm_a", "pwm_b"};
static unsigned long scmi_clk_saved_rate[ARRAY_SIZE(scmi_clk_name)];
/* Distinguish between std and str, syscore_ops is no need called when str */
static int hib_enable;

static int meson_t6x_syscore_suspend(void)
{
	int ret = 0;

	/* Do nothing in str */
	if (!hib_enable)
		return 0;

	ret = clk_save_context();
	if (ret)
		return ret;

	return ret;
}

static void meson_t6x_syscore_resume(void)
{
	/* Do nothing in str */
	if (!hib_enable)
		return;

	clk_restore_context();
}

static struct syscore_ops meson_t6x_syscore_ops = {
	.suspend	= meson_t6x_syscore_suspend,
	.resume		= meson_t6x_syscore_resume,
};

static int meson_t6x_pm_notify(struct notifier_block *notifier,
			      unsigned long pm_event,
			      void *unused)
{
	switch (pm_event) {
	case PM_HIBERNATION_PREPARE:
		hib_enable = 1;
		break;
	case PM_POST_HIBERNATION:
		hib_enable = 0;
		break;
	default:
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block meson_t6x_pm_nb = {
	.notifier_call = meson_t6x_pm_notify,
};

static int meson_t6x_freeze(struct device *dev)
{
	struct clk *scmi_clk;
	int i;

	/*
	 * scmi clk does not provide save_context/restore_context ops,
	 * so saved scmi clk rate at freeze and set it back at restore.
	 * there is IRQ warning if store SCMI clock in syscore callback.
	 * So do it in std call back.
	 */
	for (i = 0; i < ARRAY_SIZE(scmi_clk_name); ++i) {
		scmi_clk = devm_clk_get(dev, scmi_clk_name[i]);
		if (IS_ERR(scmi_clk))
			return PTR_ERR(scmi_clk);
		scmi_clk_saved_rate[i] = clk_get_rate(scmi_clk);
		devm_clk_put(dev, scmi_clk);
	}

	return 0;
}

static int meson_t6x_restore(struct device *dev)
{
	struct clk *scmi_clk;
	int i;

	for (i = 0; i < ARRAY_SIZE(scmi_clk_name); ++i) {
		scmi_clk = devm_clk_get(dev, scmi_clk_name[i]);
		if (IS_ERR(scmi_clk))
			return PTR_ERR(scmi_clk);
		/*
		 * CLK_SET_RATE_NOCACHE flag does not take effect when clk_set_rate,
		 * setting rate will be compared with the cached rate,  actual rate cannot
		 * be setted. before setting rate, using clk_get_rate to recalculate
		 * the scmi clk rate and update the cache.
		 */
		clk_get_rate(scmi_clk);
		clk_set_rate(scmi_clk, scmi_clk_saved_rate[i]);
		devm_clk_put(dev, scmi_clk);
	}

	return 0;
}

/*
 * scmi clk rate needs to be saved using clk_get_rate(), but interrupts are
 * enabled when the scmi clk rate is fetched, and syscore_suspend() is called
 * when interrupts are turned off, so scmi clk needs to be placed in freeze_noirq
 * to save state.
 */
const struct dev_pm_ops meson_t6x_pm_ops = {
	.freeze_noirq	= meson_t6x_freeze,
	.restore_noirq	= meson_t6x_restore,
};

static int meson_t6x_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map, *pll_map;
	int ret = 0, i, ssc_size = 0;

	/* Get regmap for different clock area */
	basic_map = meson_clk_regmap_resource(pdev, dev, 0);
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	pll_map = meson_clk_regmap_resource(pdev, dev, 1);
	if (IS_ERR(pll_map)) {
		dev_err(dev, "pll clk registers not found\n");
		return PTR_ERR(pll_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(t6x_clk_regmaps); i++)
		t6x_clk_regmaps[i]->map = basic_map;

	for (i = 0; i < ARRAY_SIZE(t6x_pll_clk_regmaps); i++)
		t6x_pll_clk_regmaps[i]->map = pll_map;

	for (i = 0; i < t6x_hw_onecell_data.num; i++) {
		/* array might be sparse */
		if (!t6x_hw_onecell_data.hws[i])
			continue;

		pr_debug("registering %dth  %s\n", i,
			t6x_hw_onecell_data.hws[i]->init->name);

		ret = devm_clk_hw_register(dev, t6x_hw_onecell_data.hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}
#ifdef CONFIG_AMLOGIC_CLK_DEBUG
		ret = devm_clk_hw_register_clkdev(dev, t6x_hw_onecell_data.hws[i],
					  NULL,
					  clk_hw_get_name(t6x_hw_onecell_data.hws[i]));
		if (ret < 0) {
			dev_err(dev, "Failed to clkdev register: %d\n", ret);
			return ret;
		}
#endif
	}

	ret = clk_notifier_register(t6x_vpu.hw.clk, &t6x_vpu_nb_data.nb);
	if (ret) {
		dev_err(&pdev->dev, "failed to register vpu clk notifier\n");
		return ret;
	}

	/* set gp0 emmc spread spectrum, Bits [7:4] of ANACTRL_GP0_PLL_CNTL2 control the size. */
	ret = of_property_read_u32(dev->of_node, "gp0-spread-spectrum", &ssc_size);
	if (ret)
		ssc_size = 0;

	if (ssc_size > 0) {
		regmap_update_bits(pll_map, ANACTRL_GP0PLL_CTRL2, BIT(8), BIT(8));
		regmap_update_bits(pll_map, ANACTRL_GP0PLL_CTRL2, 0xf << 4, (ssc_size / 500) << 4);
	} else {
		regmap_update_bits(pll_map, ANACTRL_GP0PLL_CTRL2, BIT(8), 0);
		regmap_update_bits(pll_map, ANACTRL_GP0PLL_CTRL2, 0xf << 4, 0);
	}

	ret = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					   &t6x_hw_onecell_data);
	if (ret)
		return ret;

	/* register syscore ops to save clk status at std */
	register_syscore_ops(&meson_t6x_syscore_ops);
	/*
	 * register pm notify, distinguish between std and str, and ensure
	 * that syscore_ops is called only when std is used.
	 */
	ret = register_pm_notifier(&meson_t6x_pm_nb);
	if (ret)
		return ret;

	return ret;
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,t6x-clkc"
	},
	{}
};

static struct platform_driver t6x_driver = {
	.probe		= meson_t6x_probe,
	.driver		= {
		.name	= "t6x-clkc",
		.of_match_table = clkc_match_table,
		.pm	= &meson_t6x_pm_ops,
	},
};

builtin_platform_driver(t6x_driver);

MODULE_LICENSE("GPL v2");
