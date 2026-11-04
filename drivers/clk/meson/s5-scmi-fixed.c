// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 * Copyright (c) 2026 Team CoreELEC
 *
 * Read-only S5 base clocks on the SCMI clock node, for a BL31 without the
 * SCMI clock protocol. sys_clk carries the name the audio clocks use as
 * parent name, the clock controller builds the fclk gates on top of
 * fixed_pll itself.
 */

#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "clk-pll.h"
#include "clk-regmap.h"
#include <dt-bindings/clock/amlogic,s5-scmi-clkc.h>

#define CLKCTRL_SYS_CLK_CTRL0		(0x0010 << 2)
#define CLKCTRL_FIXPLL_CTRL0		(0x0220 << 2)
#define CLKCTRL_FIXPLL_STS		(0x0227 << 2)

static struct clk_regmap s5_fw_fixed_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 16,
			.width   = 5,
		},
		.l = {
			.reg_off = CLKCTRL_FIXPLL_STS,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = CLKCTRL_FIXPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.flags = CLK_MESON_PLL_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "s5_fw_fixed_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
	},
};

static struct clk_regmap s5_fw_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = CLKCTRL_FIXPLL_CTRL0,
		.shift = 12,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "s5_fw_fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fw_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

#define S5_FW_FCLK(_name, _divider)					\
static struct clk_fixed_factor s5_fw_##_name = {			\
	.mult = 1,							\
	.div = _divider,						\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_" #_name,				\
		.ops = &clk_fixed_factor_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_fixed_pll.hw				\
		},							\
		.num_parents = 1,					\
	},								\
}

S5_FW_FCLK(fclk_div2, 2);
S5_FW_FCLK(fclk_div3, 3);
S5_FW_FCLK(fclk_div4, 4);
S5_FW_FCLK(fclk_div5, 5);

static u32 mux_table_sys_clk_sel[] = { 0, 1, 2, 3, 4 };
static const struct clk_parent_data s5_fw_sys_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &s5_fw_fclk_div2.hw },
	{ .hw = &s5_fw_fclk_div3.hw },
	{ .hw = &s5_fw_fclk_div4.hw },
	{ .hw = &s5_fw_fclk_div5.hw },
};

#define S5_FW_SYSCLK(_n, _sel_shift, _div_shift, _gate_bit)		\
static struct clk_regmap s5_fw_sysclk_##_n##_sel = {			\
	.data = &(struct clk_regmap_mux_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.mask = 0x7,						\
		.shift = _sel_shift,					\
		.table = mux_table_sys_clk_sel,				\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_sysclk_" #_n "_sel",			\
		.ops = &clk_regmap_mux_ro_ops,				\
		.parent_data = s5_fw_sys_clk_sel,			\
		.num_parents = ARRAY_SIZE(s5_fw_sys_clk_sel),		\
	},								\
};									\
									\
static struct clk_regmap s5_fw_sysclk_##_n##_div = {			\
	.data = &(struct clk_regmap_div_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.shift = _div_shift,					\
		.width = 10,						\
		.flags = CLK_DIVIDER_READ_ONLY,				\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_sysclk_" #_n "_div",			\
		.ops = &clk_regmap_divider_ro_ops,			\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_sysclk_##_n##_sel.hw			\
		},							\
		.num_parents = 1,					\
	},								\
};									\
									\
static struct clk_regmap s5_fw_sysclk_##_n = {				\
	.data = &(struct clk_regmap_gate_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.bit_idx = _gate_bit,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "s5_fw_sysclk_" #_n,				\
		.ops = &clk_regmap_gate_ro_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&s5_fw_sysclk_##_n##_div.hw			\
		},							\
		.num_parents = 1,					\
	},								\
}

S5_FW_SYSCLK(0, 10, 0, 13);
S5_FW_SYSCLK(1, 26, 16, 29);

static struct clk_regmap s5_fw_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 15,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&s5_fw_sysclk_0.hw,
			&s5_fw_sysclk_1.hw,
		},
		.num_parents = 2,
	},
};

static struct clk_regmap *const s5_fw_regmaps[] = {
	&s5_fw_fixed_pll_dco,
	&s5_fw_fixed_pll,
	&s5_fw_sysclk_0_sel,
	&s5_fw_sysclk_0_div,
	&s5_fw_sysclk_0,
	&s5_fw_sysclk_1_sel,
	&s5_fw_sysclk_1_div,
	&s5_fw_sysclk_1,
	&s5_fw_sys_clk,
};

/* registration order keeps every parent ahead of its children */
static struct clk_hw *const s5_fw_order[] = {
	&s5_fw_fixed_pll_dco.hw,
	&s5_fw_fixed_pll.hw,
	&s5_fw_fclk_div2.hw,
	&s5_fw_fclk_div3.hw,
	&s5_fw_fclk_div4.hw,
	&s5_fw_fclk_div5.hw,
	&s5_fw_sysclk_0_sel.hw,
	&s5_fw_sysclk_0_div.hw,
	&s5_fw_sysclk_0.hw,
	&s5_fw_sysclk_1_sel.hw,
	&s5_fw_sysclk_1_div.hw,
	&s5_fw_sysclk_1.hw,
	&s5_fw_sys_clk.hw,
};

static struct clk_hw *s5_fw_hws[] = {
	[CLKID_SYS_CLK]			= &s5_fw_sys_clk.hw,
	[CLKID_FIXED_PLL]		= &s5_fw_fixed_pll.hw,
};

/*
 * IDs not served here stay with the SCMI provider: -EPROBE_DEFER lets the
 * lookup move on to it, or wait for it like without this provider
 */
static struct clk_hw *s5_fw_clk_get(struct of_phandle_args *clkspec,
				    void *data)
{
	unsigned int idx = clkspec->args[0];

	if (idx >= ARRAY_SIZE(s5_fw_hws) || !s5_fw_hws[idx])
		return ERR_PTR(-EPROBE_DEFER);

	return s5_fw_hws[idx];
}

static const struct regmap_config s5_fw_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static void __init s5_fw_clk_init(struct device_node *np)
{
	struct device_node *clkc;
	void __iomem *base;
	struct regmap *map;
	unsigned int i;
	int ret;

	clkc = of_find_compatible_node(NULL, NULL, "amlogic,s5-clkc");
	if (!clkc)
		return;

	base = of_iomap(clkc, 0);
	of_node_put(clkc);
	if (!base) {
		pr_err("%pOF: cannot map clock registers\n", np);
		return;
	}

	map = regmap_init_mmio(NULL, base, &s5_fw_regmap_config);
	if (IS_ERR(map)) {
		pr_err("%pOF: cannot map clock registers\n", np);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(s5_fw_regmaps); i++)
		s5_fw_regmaps[i]->map = map;

	for (i = 0; i < ARRAY_SIZE(s5_fw_order); i++) {
		ret = of_clk_hw_register(np, s5_fw_order[i]);
		if (ret)
			goto err;
	}

	ret = of_clk_add_hw_provider(np, s5_fw_clk_get, NULL);
	if (!ret)
		return;
err:
	pr_err("%pOF: failed to register clocks: %d\n", np, ret);
}

CLK_OF_DECLARE(s5_scmi_fixed, "amlogic,s5-scmi-fixed-clkc", s5_fw_clk_init);
