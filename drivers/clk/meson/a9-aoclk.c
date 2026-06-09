// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/module.h>
#ifdef CONFIG_AMLOGIC_CLK_DEBUG
#include <linux/clkdev.h>
#endif
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "meson-clkc-utils.h"
#include "clk-dualdiv.h"
#include "clk-regmap.h"
#include "a9.h"
#include <dt-bindings/clock/amlogic,a9-clkc.h>

static const struct clk_parent_data ao_oscin_clk_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "xtal_32k" }
};

static struct clk_regmap ao_oscin_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_OSCIN_CTRL,
		.mask = 0x1,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_oscin_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_oscin_clk_parent_data,
		.num_parents = ARRAY_SIZE(ao_oscin_clk_parent_data),
	},
};

static const struct clk_parent_data ao_sys_clk_parent_data[] = {
	{ .hw = &ao_oscin_clk.hw },
	{ .fw_name = "sys_clk" }
};

static struct clk_regmap ao_sys_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_OSCIN_CTRL,
		.mask = 0x1,
		.shift = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_sys_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_sys_clk_parent_data,
		.num_parents = ARRAY_SIZE(ao_sys_clk_parent_data),
	},
};

static struct clk_regmap ao_rtc_dual_clkin = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rtc_dual_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param ao_rtc_dual_div_table[] = {
	{ 733, 732, 8, 11, 1 },
	{ }
};

static struct clk_regmap ao_rtc_dual_div = {
	.data = &(struct meson_clk_dualdiv_data) {
		.n1 = {
			.reg_off = AO_CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = AO_CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = AO_CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = AO_CLKCTRL_RTC_BY_OSCIN_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = AO_CLKCTRL_RTC_BY_OSCIN_CTRL0,
			.shift   = 28,
			.width   = 2,
		},
		.table = ao_rtc_dual_div_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rtc_dual_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_rtc_dual_clkin.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data ao_rtc_dual_parent_data[] = {
	{ .hw = &ao_rtc_dual_div.hw },
	{ .hw = &ao_rtc_dual_clkin.hw }
};

static struct clk_regmap ao_rtc_dual_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rtc_dual_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_rtc_dual_parent_data,
		.num_parents = ARRAY_SIZE(ao_rtc_dual_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_rtc_dual = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_RTC_BY_OSCIN_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rtc_dual",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_rtc_dual_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ao_rtc_clk_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &ao_rtc_dual.hw }
};

static struct clk_regmap ao_rtc_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_RTC_BY_OSCIN_CTRL1,
		.mask = 0x3,
		.shift = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rtc_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_rtc_clk_parent_data,
		.num_parents = ARRAY_SIZE(ao_rtc_clk_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_cecb_dual_clkin = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_CECB_CTRL0,
		.bit_idx = 31,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_cecb_dual_clkin",
		.ops = &clk_regmap_gate_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static const struct meson_clk_dualdiv_param ao_cecb_dual_div_table[] = {
	{ 733, 732, 8, 11, 1 },
	{ }
};

static struct clk_regmap ao_cecb_dual_div = {
	.data = &(struct meson_clk_dualdiv_data) {
		.n1 = {
			.reg_off = AO_CLKCTRL_CECB_CTRL0,
			.shift   = 0,
			.width   = 12,
		},
		.n2 = {
			.reg_off = AO_CLKCTRL_CECB_CTRL0,
			.shift   = 12,
			.width   = 12,
		},
		.m1 = {
			.reg_off = AO_CLKCTRL_CECB_CTRL1,
			.shift   = 0,
			.width   = 12,
		},
		.m2 = {
			.reg_off = AO_CLKCTRL_CECB_CTRL1,
			.shift   = 12,
			.width   = 12,
		},
		.dual = {
			.reg_off = AO_CLKCTRL_CECB_CTRL0,
			.shift   = 28,
			.width   = 2,
		},
		.table = ao_cecb_dual_div_table,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_cecb_dual_div",
		.ops = &meson_clk_dualdiv_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_cecb_dual_clkin.hw,
		},
		.num_parents = 1,
	},
};

static const struct clk_parent_data ao_cecb_dual_parent_data[] = {
	{ .hw = &ao_cecb_dual_div.hw },
	{ .hw = &ao_cecb_dual_clkin.hw }
};

static struct clk_regmap ao_cecb_dual_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_CECB_CTRL1,
		.mask = 0x1,
		.shift = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_cecb_dual_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_cecb_dual_parent_data,
		.num_parents = ARRAY_SIZE(ao_cecb_dual_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_cecb_dual = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_CECB_CTRL0,
		.bit_idx = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_cecb_dual",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_cecb_dual_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ao_cecb_clk_parent_data[] = {
	{ .hw = &ao_cecb_dual.hw },
	{ .fw_name = "rtc_clk" }
};

static struct clk_regmap ao_cecb_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_CECB_CTRL1,
		.mask = 0x3,
		.shift = 30,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_cecb_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_cecb_clk_parent_data,
		.num_parents = ARRAY_SIZE(ao_cecb_clk_parent_data),
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ao_pwm_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" }
};

static struct clk_regmap ao_pwm_a_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_A_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_a_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_a_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_A_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_a_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_a_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_a = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_A_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_a",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_a_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_b_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_B_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_b_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_b_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_B_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_b_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_b_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_b = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_B_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_b",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_b_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_c_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_C_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_c_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_c_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_C_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_c_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_c_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_c = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_C_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_c",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_c_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_d_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_D_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_d_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_d_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_D_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_d_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_d_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_d = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_D_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_d",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_d_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_e_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_E_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_e_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_e_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_E_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_e_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_e_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_e = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_E_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_e",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_e_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_f_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_F_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_f_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_f_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_F_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_f_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_f_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_f = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_F_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_f",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_f_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_g_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_G_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_g_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_g_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_G_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_g_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_g_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_g = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_G_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_g",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_g_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_CLK_GATE_AO_SYS_CLK(_name, _reg, _bit)                  \
static struct clk_regmap _name = {                                           \
	.data = &(struct clk_regmap_gate_data) {                      \
		.offset = (_reg),                                     \
		.bit_idx = (_bit),                                    \
	},                                                            \
	.hw.init = &(struct clk_init_data) {                          \
		.name = #_name,                                       \
		.ops = &clk_regmap_gate_ops,                          \
		.parent_hws = (const struct clk_hw *[]) {             \
			&ao_sys_clk.hw,                               \
		},                                                    \
		.num_parents = 1,                                     \
		.flags = CLK_IGNORE_UNUSED,                           \
	},                                                            \
}

MESON_CLK_GATE_AO_SYS_CLK(ao_sys_i3c, AO_CLKCTRL_RTC_SYS_CLK0, 0);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_reg, AO_CLKCTRL_RTC_SYS_CLK0, 1);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_clktree, AO_CLKCTRL_RTC_SYS_CLK0, 2);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rst_ctrl, AO_CLKCTRL_RTC_SYS_CLK0, 3);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pad, AO_CLKCTRL_RTC_SYS_CLK0, 4);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_dig, AO_CLKCTRL_RTC_SYS_CLK0, 5);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_irq, AO_CLKCTRL_RTC_SYS_CLK0, 6);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwrctrl, AO_CLKCTRL_RTC_SYS_CLK0, 7);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwm_a, AO_CLKCTRL_RTC_SYS_CLK0, 8);

#define MESON_CLK_GATE_AO_SYS_HW(_name, _reg, _bit, _parent_hw)       \
struct clk_regmap _name = {                                           \
	.data = &(struct clk_regmap_gate_data) {                      \
		.offset = (_reg),                                     \
		.bit_idx = (_bit),                                    \
	},                                                            \
	.hw.init = &(struct clk_init_data) {                          \
		.name = #_name,                                       \
		.ops = &clk_regmap_gate_ops,                          \
		.parent_hws = (const struct clk_hw *[]) {             \
			&(_parent_hw)                                 \
		},                                                    \
		.num_parents = 1,                                     \
		.flags = CLK_IGNORE_UNUSED,                           \
	},                                                            \
}

MESON_CLK_GATE_AO_SYS_HW(ao_sys_pwm_b, AO_CLKCTRL_RTC_SYS_CLK0, 9, ao_sys_pwm_a.hw);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_pwm_c, AO_CLKCTRL_RTC_SYS_CLK0, 10, ao_sys_pwm_a.hw);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_pwm_d, AO_CLKCTRL_RTC_SYS_CLK0, 11, ao_sys_pwm_a.hw);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_pwm_e, AO_CLKCTRL_RTC_SYS_CLK0, 12, ao_sys_pwm_a.hw);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_pwm_f, AO_CLKCTRL_RTC_SYS_CLK0, 13, ao_sys_pwm_a.hw);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_pwm_g, AO_CLKCTRL_RTC_SYS_CLK0, 14, ao_sys_pwm_a.hw);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_i2c_m_a, AO_CLKCTRL_RTC_SYS_CLK0, 15);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_i2c_m_b, AO_CLKCTRL_RTC_SYS_CLK0, 16, ao_sys_i2c_m_a.hw);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_i2c_m_c, AO_CLKCTRL_RTC_SYS_CLK0, 17, ao_sys_i2c_m_a.hw);
MESON_CLK_GATE_AO_SYS_HW(ao_sys_i2c_m_d, AO_CLKCTRL_RTC_SYS_CLK0, 18, ao_sys_i2c_m_a.hw);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_sed, AO_CLKCTRL_RTC_SYS_CLK0, 19);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_ir_ctrl, AO_CLKCTRL_RTC_SYS_CLK0, 20);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_uart_b, AO_CLKCTRL_RTC_SYS_CLK0, 21);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_uart_c, AO_CLKCTRL_RTC_SYS_CLK0, 22);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_uart_d, AO_CLKCTRL_RTC_SYS_CLK0, 23);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_uart_e, AO_CLKCTRL_RTC_SYS_CLK0, 24);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_spisg_0, AO_CLKCTRL_RTC_SYS_CLK0, 25);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_sec, AO_CLKCTRL_RTC_SYS_CLK0, 26);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_cec, AO_CLKCTRL_RTC_SYS_CLK0, 27);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_aocpu, AO_CLKCTRL_RTC_SYS_CLK0, 28);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_sram, AO_CLKCTRL_RTC_SYS_CLK0, 29);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_spisg_1, AO_CLKCTRL_RTC_SYS_CLK0, 30);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_spisg_2, AO_CLKCTRL_RTC_SYS_CLK0, 31);

static struct clk_regmap *const aoclk_regmaps[] = {
	&ao_oscin_clk,
	&ao_sys_clk,
	&ao_rtc_dual_clkin,
	&ao_rtc_dual_div,
	&ao_rtc_dual_mux,
	&ao_rtc_dual,
	&ao_rtc_clk,
	&ao_cecb_dual_clkin,
	&ao_cecb_dual_div,
	&ao_cecb_dual_mux,
	&ao_cecb_dual,
	&ao_cecb_clk,
	&ao_pwm_a_mux,
	&ao_pwm_a_div,
	&ao_pwm_a,
	&ao_pwm_b_mux,
	&ao_pwm_b_div,
	&ao_pwm_b,
	&ao_pwm_c_mux,
	&ao_pwm_c_div,
	&ao_pwm_c,
	&ao_pwm_d_mux,
	&ao_pwm_d_div,
	&ao_pwm_d,
	&ao_pwm_e_mux,
	&ao_pwm_e_div,
	&ao_pwm_e,
	&ao_pwm_f_mux,
	&ao_pwm_f_div,
	&ao_pwm_f,
	&ao_pwm_g_mux,
	&ao_pwm_g_div,
	&ao_pwm_g,
	&ao_sys_i3c,
	&ao_sys_rtc_reg,
	&ao_sys_clktree,
	&ao_sys_rst_ctrl,
	&ao_sys_pad,
	&ao_sys_rtc_dig,
	&ao_sys_irq,
	&ao_sys_pwrctrl,
	&ao_sys_pwm_a,
	&ao_sys_pwm_b,
	&ao_sys_pwm_c,
	&ao_sys_pwm_d,
	&ao_sys_pwm_e,
	&ao_sys_pwm_f,
	&ao_sys_pwm_g,
	&ao_sys_i2c_m_a,
	&ao_sys_i2c_m_b,
	&ao_sys_i2c_m_c,
	&ao_sys_i2c_m_d,
	&ao_sys_sed,
	&ao_sys_ir_ctrl,
	&ao_sys_uart_b,
	&ao_sys_uart_c,
	&ao_sys_uart_d,
	&ao_sys_uart_e,
	&ao_sys_spisg_0,
	&ao_sys_rtc_sec,
	&ao_sys_cec,
	&ao_sys_aocpu,
	&ao_sys_sram,
	&ao_sys_spisg_1,
	&ao_sys_spisg_2
};

static struct clk_hw *a9_aoclk_hw_clks[] = {
	[CLKID_AO_OSCIN_CLK]          = &ao_oscin_clk.hw,
	[CLKID_AO_SYS_CLK]            = &ao_sys_clk.hw,
	[CLKID_AO_RTC_DUAL_CLKIN]     = &ao_rtc_dual_clkin.hw,
	[CLKID_AO_RTC_DUAL_DIV]       = &ao_rtc_dual_div.hw,
	[CLKID_AO_RTC_DUAL_MUX]       = &ao_rtc_dual_mux.hw,
	[CLKID_AO_RTC_DUAL]           = &ao_rtc_dual.hw,
	[CLKID_AO_RTC_CLK]            = &ao_rtc_clk.hw,
	[CLKID_AO_CECB_DUAL_CLKIN]    = &ao_cecb_dual_clkin.hw,
	[CLKID_AO_CECB_DUAL_DIV]      = &ao_cecb_dual_div.hw,
	[CLKID_AO_CECB_DUAL_MUX]      = &ao_cecb_dual_mux.hw,
	[CLKID_AO_CECB_DUAL]          = &ao_cecb_dual.hw,
	[CLKID_AO_CECB_CLK]           = &ao_cecb_clk.hw,
	[CLKID_AO_PWM_A_MUX]          = &ao_pwm_a_mux.hw,
	[CLKID_AO_PWM_A_DIV]          = &ao_pwm_a_div.hw,
	[CLKID_AO_PWM_A]              = &ao_pwm_a.hw,
	[CLKID_AO_PWM_B_MUX]          = &ao_pwm_b_mux.hw,
	[CLKID_AO_PWM_B_DIV]          = &ao_pwm_b_div.hw,
	[CLKID_AO_PWM_B]              = &ao_pwm_b.hw,
	[CLKID_AO_PWM_C_MUX]          = &ao_pwm_c_mux.hw,
	[CLKID_AO_PWM_C_DIV]          = &ao_pwm_c_div.hw,
	[CLKID_AO_PWM_C]              = &ao_pwm_c.hw,
	[CLKID_AO_PWM_D_MUX]          = &ao_pwm_d_mux.hw,
	[CLKID_AO_PWM_D_DIV]          = &ao_pwm_d_div.hw,
	[CLKID_AO_PWM_D]              = &ao_pwm_d.hw,
	[CLKID_AO_PWM_E_MUX]          = &ao_pwm_e_mux.hw,
	[CLKID_AO_PWM_E_DIV]          = &ao_pwm_e_div.hw,
	[CLKID_AO_PWM_E]              = &ao_pwm_e.hw,
	[CLKID_AO_PWM_F_MUX]          = &ao_pwm_f_mux.hw,
	[CLKID_AO_PWM_F_DIV]          = &ao_pwm_f_div.hw,
	[CLKID_AO_PWM_F]              = &ao_pwm_f.hw,
	[CLKID_AO_PWM_G_MUX]          = &ao_pwm_g_mux.hw,
	[CLKID_AO_PWM_G_DIV]          = &ao_pwm_g_div.hw,
	[CLKID_AO_PWM_G]              = &ao_pwm_g.hw,
	[CLKID_AO_SYS_I3C]            = &ao_sys_i3c.hw,
	[CLKID_AO_SYS_RTC_REG]        = &ao_sys_rtc_reg.hw,
	[CLKID_AO_SYS_CLKTREE]        = &ao_sys_clktree.hw,
	[CLKID_AO_SYS_RST_CTRL]       = &ao_sys_rst_ctrl.hw,
	[CLKID_AO_SYS_PAD]            = &ao_sys_pad.hw,
	[CLKID_AO_SYS_RTC_DIG]        = &ao_sys_rtc_dig.hw,
	[CLKID_AO_SYS_IRQ]            = &ao_sys_irq.hw,
	[CLKID_AO_SYS_PWRCTRL]        = &ao_sys_pwrctrl.hw,
	[CLKID_AO_SYS_PWM_A]          = &ao_sys_pwm_a.hw,
	[CLKID_AO_SYS_PWM_B]          = &ao_sys_pwm_b.hw,
	[CLKID_AO_SYS_PWM_C]          = &ao_sys_pwm_c.hw,
	[CLKID_AO_SYS_PWM_D]          = &ao_sys_pwm_d.hw,
	[CLKID_AO_SYS_PWM_E]          = &ao_sys_pwm_e.hw,
	[CLKID_AO_SYS_PWM_F]          = &ao_sys_pwm_f.hw,
	[CLKID_AO_SYS_PWM_G]          = &ao_sys_pwm_g.hw,
	[CLKID_AO_SYS_I2C_M_A]        = &ao_sys_i2c_m_a.hw,
	[CLKID_AO_SYS_I2C_M_B]        = &ao_sys_i2c_m_b.hw,
	[CLKID_AO_SYS_I2C_M_C]        = &ao_sys_i2c_m_c.hw,
	[CLKID_AO_SYS_I2C_M_D]        = &ao_sys_i2c_m_d.hw,
	[CLKID_AO_SYS_SED]            = &ao_sys_sed.hw,
	[CLKID_AO_SYS_IR_CTRL]        = &ao_sys_ir_ctrl.hw,
	[CLKID_AO_SYS_UART_B]         = &ao_sys_uart_b.hw,
	[CLKID_AO_SYS_UART_C]         = &ao_sys_uart_c.hw,
	[CLKID_AO_SYS_UART_D]         = &ao_sys_uart_d.hw,
	[CLKID_AO_SYS_UART_E]         = &ao_sys_uart_e.hw,
	[CLKID_AO_SYS_SPISG_0]        = &ao_sys_spisg_0.hw,
	[CLKID_AO_SYS_RTC_SEC]        = &ao_sys_rtc_sec.hw,
	[CLKID_AO_SYS_CEC]            = &ao_sys_cec.hw,
	[CLKID_AO_SYS_AOCPU]          = &ao_sys_aocpu.hw,
	[CLKID_AO_SYS_SRAM]           = &ao_sys_sram.hw,
	[CLKID_AO_SYS_SPISG_1]        = &ao_sys_spisg_1.hw,
	[CLKID_AO_SYS_SPISG_2]        = &ao_sys_spisg_2.hw
};

static const struct meson_clk_hw_data a9_aoclks = {
	.hws = a9_aoclk_hw_clks,
	.num = ARRAY_SIZE(a9_aoclk_hw_clks),
};

static int meson_a9_aoclk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct regmap *basic_map;
	struct meson_clk_hw_data *data;
	int ret, i;

	data = (struct meson_clk_hw_data *)of_device_get_match_data(dev);
	if (!data)
		return -ENODEV;

	basic_map = meson_clk_regmap_resource(pdev, dev, 0);
	if (IS_ERR(basic_map)) {
		dev_err(dev, "basic clk registers not found\n");
		return PTR_ERR(basic_map);
	}

	/* Populate regmap for the regmap backed clocks */
	for (i = 0; i < ARRAY_SIZE(aoclk_regmaps); i++)
		aoclk_regmaps[i]->map = basic_map;

	for (i = 0; i < data->num; i++) {
		/* array might be sparse */
		if (!data->hws[i])
			continue;

		ret = devm_clk_hw_register(dev, data->hws[i]);
		if (ret) {
			dev_err(dev, "Clock registration failed\n");
			return ret;
		}

#ifdef CONFIG_AMLOGIC_CLK_DEBUG
		ret = devm_clk_hw_register_clkdev(dev, data->hws[i],
						  NULL,
						  clk_hw_get_name(data->hws[i]));
		if (ret < 0) {
			dev_err(dev, "Failed to clkdev register: %d\n", ret);
			return ret;
		}
#endif
	}

	return devm_of_clk_add_hw_provider(dev, meson_clk_hw_get,
					   (void *)data);
}

static const struct of_device_id clkc_match_table[] = {
	{
		.compatible = "amlogic,a9-aoclkc",
		.data = &a9_aoclks
	},
	{}
};

static struct platform_driver a9_aoclk_driver = {
	.probe			= meson_a9_aoclk_probe,
	.driver			= {
		.name		= "a9-aoclkc",
		.of_match_table	= clkc_match_table,
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
int __init a9_aoclk_init(void)
{
	return platform_driver_register(&a9_aoclk_driver);
}

void __exit a9_aoclk_exit(void)
{
	platform_driver_unregister(&a9_aoclk_driver);
}
#else
builtin_platform_driver(a9_aoclk_driver);
MODULE_LICENSE("GPL v2");
#endif
