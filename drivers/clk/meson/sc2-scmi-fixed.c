// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 * Copyright (c) 2026 Team CoreELEC
 *
 * Read-only SC2 base clocks on the SCMI clock node, for a BL31 without the
 * SCMI clock protocol. sys_clk carries the name the audio clocks use as
 * parent name.
 *
 * Such a BL31 still serves the cpu clock registers through the legacy
 * CPUCLK_SECURE_RW call. The cpu clock then switches between the fclk
 * based dyn path and sys_pll, which keeps the rate of the bootloader.
 * The dsu clock is read only.
 */

#include <linux/arm-smccc.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/of_address.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include "clk-pll.h"
#include "clk-regmap.h"
#include <dt-bindings/clock/amlogic,sc2-scmi-clkc.h>

#define ANACTRL_FIXPLL_CTRL0		(0x0010 << 2)
#define ANACTRL_FIXPLL_CTRL1		(0x0011 << 2)
#define ANACTRL_FIXPLL_CTRL3		(0x0013 << 2)
#define CLKCTRL_SYS_CLK_CTRL0		(0x0010 << 2)
#define ANACTRL_SYSPLL_CTRL0		(0x0000 << 2)
#define ANACTRL_GP1PLL_CTRL0		(0x0030 << 2)
#define ANACTRL_GP1PLL_CTRL1		(0x0031 << 2)

#define CPUCLK_SECURE_RW		0x8200009B
#define CLK_CPU_REG_RW			0
#define CLK_DSU_REG_RW			1
#define CLK_CPU_DSU_REG_RW		6
#define SET_CPU0_MUX_PARENT		7
#define CPU_DIVIDER_SET_RATE		11

static struct clk_regmap sc2_fw_fixed_pll_dco = {
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
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_FIXPLL_CTRL1,
			.shift   = 0,
			.width   = 17,
		},
		.l = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_FIXPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.flags = CLK_MESON_PLL_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_fixed_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_fw_fixed_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_FIXPLL_CTRL0,
		.shift = 16,
		.width = 2,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_fixed_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_fixed_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

#define SC2_FW_FCLK(_name, _divider)					\
static struct clk_fixed_factor sc2_fw_##_name = {			\
	.mult = 1,							\
	.div = _divider,						\
	.hw.init = &(struct clk_init_data){				\
		.name = "sc2_fw_" #_name,				\
		.ops = &clk_fixed_factor_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&sc2_fw_fixed_pll.hw				\
		},							\
		.num_parents = 1,					\
	},								\
}

SC2_FW_FCLK(fclk_div2, 2);
SC2_FW_FCLK(fclk_div3, 3);
SC2_FW_FCLK(fclk_div4, 4);
SC2_FW_FCLK(fclk_div5, 5);
SC2_FW_FCLK(fclk_div7, 7);

static u32 mux_table_sys_ab_clk_sel[] = { 0, 1, 2, 3, 4, 6, 7 };
static const struct clk_parent_data sc2_fw_sys_ab_clk_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_fw_fclk_div2.hw },
	{ .hw = &sc2_fw_fclk_div3.hw },
	{ .hw = &sc2_fw_fclk_div4.hw },
	{ .hw = &sc2_fw_fclk_div5.hw },
	{ .fw_name = "axi_clk_frcpu", },
	{ .hw = &sc2_fw_fclk_div7.hw },
};

#define SC2_FW_SYSCLK(_ab, _sel_shift, _div_shift, _gate_bit)		\
static struct clk_regmap sc2_fw_sysclk_##_ab##_sel = {			\
	.data = &(struct clk_regmap_mux_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.mask = 0x7,						\
		.shift = _sel_shift,					\
		.table = mux_table_sys_ab_clk_sel,			\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "sc2_fw_sysclk_" #_ab "_sel",			\
		.ops = &clk_regmap_mux_ro_ops,				\
		.parent_data = sc2_fw_sys_ab_clk_sel,			\
		.num_parents = ARRAY_SIZE(sc2_fw_sys_ab_clk_sel),	\
	},								\
};									\
									\
static struct clk_regmap sc2_fw_sysclk_##_ab##_div = {		\
	.data = &(struct clk_regmap_div_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.shift = _div_shift,					\
		.width = 10,						\
		.flags = CLK_DIVIDER_READ_ONLY,				\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "sc2_fw_sysclk_" #_ab "_div",			\
		.ops = &clk_regmap_divider_ro_ops,			\
		.parent_hws = (const struct clk_hw *[]) {		\
			&sc2_fw_sysclk_##_ab##_sel.hw			\
		},							\
		.num_parents = 1,					\
	},								\
};									\
									\
static struct clk_regmap sc2_fw_sysclk_##_ab = {			\
	.data = &(struct clk_regmap_gate_data){				\
		.offset = CLKCTRL_SYS_CLK_CTRL0,			\
		.bit_idx = _gate_bit,					\
	},								\
	.hw.init = &(struct clk_init_data){				\
		.name = "sc2_fw_sysclk_" #_ab,				\
		.ops = &clk_regmap_gate_ro_ops,				\
		.parent_hws = (const struct clk_hw *[]) {		\
			&sc2_fw_sysclk_##_ab##_div.hw			\
		},							\
		.num_parents = 1,					\
	},								\
}

SC2_FW_SYSCLK(a, 10, 0, 13);
SC2_FW_SYSCLK(b, 26, 16, 29);

static struct clk_regmap sc2_fw_sys_clk = {
	.data = &(struct clk_regmap_mux_data){
		.offset = CLKCTRL_SYS_CLK_CTRL0,
		.mask = 0x1,
		.shift = 31,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sys_clk",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_sysclk_a.hw,
			&sc2_fw_sysclk_b.hw,
		},
		.num_parents = 2,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_fixed_factor sc2_fw_fclk_50m_div = {
	.mult = 1,
	.div = 80,
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_fclk_50m_div",
		.ops = &clk_fixed_factor_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_fixed_pll_dco.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fw_fclk_50m = {
	.data = &(struct clk_regmap_mux_data){
		.offset = ANACTRL_FIXPLL_CTRL3,
		.mask = 0x1,
		.shift = 5,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_fclk_50m",
		.ops = &clk_regmap_mux_ro_ops,
		.parent_data = (const struct clk_parent_data []) {
			{ .fw_name = "xtal", },
			{ .hw = &sc2_fw_fclk_50m_div.hw },
		},
		.num_parents = 2,
	},
};

static unsigned long sc2_fw_cpuclk_smc(unsigned long cmd, unsigned long a1,
				       unsigned long a2, unsigned long a3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(CPUCLK_SECURE_RW, cmd, a1, a2, a3, 0, 0, 0, &res);

	return res.a0;
}

static u8 sc2_fw_cpu_mux_get_parent(struct clk_hw *hw)
{
	struct clk_regmap_mux_data *mux = clk_get_regmap_mux_data(to_clk_regmap(hw));
	unsigned int val = sc2_fw_cpuclk_smc(CLK_CPU_REG_RW, 0, 0, 0);

	val = (val >> mux->shift) & mux->mask;
	return clk_mux_val_to_index(hw, mux->table, mux->flags, val);
}

static int sc2_fw_cpu_mux_set_parent(struct clk_hw *hw, u8 index)
{
	struct clk_regmap_mux_data *mux = clk_get_regmap_mux_data(to_clk_regmap(hw));
	unsigned int val = clk_mux_index_to_val(mux->table, mux->flags, index);

	sc2_fw_cpuclk_smc(SET_CPU0_MUX_PARENT, val, mux->mask, mux->shift);

	return 0;
}

static int sc2_fw_cpu_mux_determine_rate(struct clk_hw *hw,
					 struct clk_rate_request *req)
{
	struct clk_regmap_mux_data *mux = clk_get_regmap_mux_data(to_clk_regmap(hw));

	return clk_mux_determine_rate_flags(hw, req, mux->flags);
}

static const struct clk_ops sc2_fw_cpu_mux_ops = {
	.get_parent = sc2_fw_cpu_mux_get_parent,
	.set_parent = sc2_fw_cpu_mux_set_parent,
	.determine_rate = sc2_fw_cpu_mux_determine_rate,
};

static unsigned long sc2_fw_cpu_div_recalc_rate(struct clk_hw *hw,
						unsigned long prate)
{
	struct clk_regmap_div_data *div = clk_get_regmap_div_data(to_clk_regmap(hw));
	unsigned int val = sc2_fw_cpuclk_smc(CLK_CPU_REG_RW, 0, 0, 0);

	val = (val >> div->shift) & clk_div_mask(div->width);
	return divider_recalc_rate(hw, prate, val, NULL, div->flags, div->width);
}

static int sc2_fw_cpu_div_determine_rate(struct clk_hw *hw,
					 struct clk_rate_request *req)
{
	struct clk_regmap_div_data *div = clk_get_regmap_div_data(to_clk_regmap(hw));

	return divider_determine_rate(hw, req, NULL, div->width, div->flags);
}

static int sc2_fw_cpu_div_set_rate(struct clk_hw *hw, unsigned long rate,
				   unsigned long parent_rate)
{
	struct clk_regmap_div_data *div = clk_get_regmap_div_data(to_clk_regmap(hw));
	int val = divider_get_val(rate, parent_rate, NULL, div->width, div->flags);

	if (val < 0)
		return val;

	sc2_fw_cpuclk_smc(CPU_DIVIDER_SET_RATE, val, div->width, div->shift);

	return 0;
}

static const struct clk_ops sc2_fw_cpu_div_ops = {
	.recalc_rate = sc2_fw_cpu_div_recalc_rate,
	.determine_rate = sc2_fw_cpu_div_determine_rate,
	.set_rate = sc2_fw_cpu_div_set_rate,
};

static const struct clk_ops sc2_fw_cpu_div_ro_ops = {
	.recalc_rate = sc2_fw_cpu_div_recalc_rate,
};

static struct clk_regmap sc2_fw_sys_pll_dco = {
	.data = &(struct meson_clk_pll_data){
		.en = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 28,
			.width   = 1,
		},
		.m = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 0,
			.width   = 8,
		},
		.n = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 10,
			.width   = 5,
		},
		.l = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 31,
			.width   = 1,
		},
		.rst = {
			.reg_off = ANACTRL_SYSPLL_CTRL0,
			.shift   = 29,
			.width   = 1,
		},
		.flags = CLK_MESON_PLL_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_sys_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static struct clk_regmap sc2_fw_sys_pll = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_SYSPLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_sys_pll",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_sys_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

static const struct clk_parent_data sc2_fw_cpu_dyn_sel[] = {
	{ .fw_name = "xtal", },
	{ .hw = &sc2_fw_fclk_div2.hw },
	{ .hw = &sc2_fw_fclk_div3.hw },
};

static struct clk_regmap sc2_fw_cpu_clk_premux0 = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x3,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk_dyn0_sel",
		.ops = &sc2_fw_cpu_mux_ops,
		.parent_data = sc2_fw_cpu_dyn_sel,
		.num_parents = ARRAY_SIZE(sc2_fw_cpu_dyn_sel),
		.flags = CLK_SET_RATE_PARENT,
	},
};

/* parking path while the dyn0 path changes */
static struct clk_regmap sc2_fw_cpu_clk_premux1 = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x3,
		.shift = 16,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk_dyn1_sel",
		.ops = &sc2_fw_cpu_mux_ops,
		.parent_data = sc2_fw_cpu_dyn_sel,
		.num_parents = ARRAY_SIZE(sc2_fw_cpu_dyn_sel),
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap sc2_fw_cpu_clk_mux0_div = {
	.data = &(struct clk_regmap_div_data){
		.shift = 4,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk_dyn0_div",
		.ops = &sc2_fw_cpu_div_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_cpu_clk_premux0.hw
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_fw_cpu_clk_postmux0 = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk_dyn0",
		.ops = &sc2_fw_cpu_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_cpu_clk_premux0.hw,
			&sc2_fw_cpu_clk_mux0_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap sc2_fw_cpu_clk_mux1_div = {
	.data = &(struct clk_regmap_div_data){
		.shift = 20,
		.width = 6,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk_dyn1_div",
		.ops = &sc2_fw_cpu_div_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_cpu_clk_premux1.hw
		},
		.num_parents = 1,
	},
};

static struct clk_regmap sc2_fw_cpu_clk_postmux1 = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 18,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk_dyn1",
		.ops = &sc2_fw_cpu_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_cpu_clk_premux1.hw,
			&sc2_fw_cpu_clk_mux1_div.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_NO_REPARENT,
	},
};

static struct clk_regmap sc2_fw_cpu_clk_dyn = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 10,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk_dyn",
		.ops = &sc2_fw_cpu_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_cpu_clk_postmux0.hw,
			&sc2_fw_cpu_clk_postmux1.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

/*
 * no CLK_MUX_ROUND_CLOSEST: the real rate never exceeds the requested OPP,
 * so the OPP voltage always covers it
 */
static struct clk_regmap sc2_fw_cpu_clk = {
	.data = &(struct clk_regmap_mux_data){
		.mask = 0x1,
		.shift = 11,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_cpu_clk",
		.ops = &sc2_fw_cpu_mux_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_cpu_clk_dyn.hw,
			&sc2_fw_sys_pll.hw,
		},
		.num_parents = 2,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static int sc2_fw_cpu_clk_postmux0_cb(struct notifier_block *nb,
				      unsigned long event, void *data)
{
	switch (event) {
	case PRE_RATE_CHANGE:
		/* park the cpu on fclk_div2 through the dyn1 path */
		clk_hw_set_parent(&sc2_fw_cpu_clk_premux1.hw, &sc2_fw_fclk_div2.hw);
		clk_hw_set_parent(&sc2_fw_cpu_clk_postmux1.hw,
				  &sc2_fw_cpu_clk_premux1.hw);
		clk_hw_set_parent(&sc2_fw_cpu_clk_dyn.hw, &sc2_fw_cpu_clk_postmux1.hw);
		return NOTIFY_OK;
	case POST_RATE_CHANGE:
		clk_hw_set_parent(&sc2_fw_cpu_clk_dyn.hw, &sc2_fw_cpu_clk_postmux0.hw);
		return NOTIFY_OK;
	default:
		return NOTIFY_DONE;
	}
}

static struct notifier_block sc2_fw_cpu_clk_postmux0_nb = {
	.notifier_call = sc2_fw_cpu_clk_postmux0_cb,
};

static struct clk_regmap sc2_fw_gp1_pll_dco = {
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
			.shift   = 10,
			.width   = 5,
		},
		.frac = {
			.reg_off = ANACTRL_GP1PLL_CTRL1,
			.shift   = 0,
			.width   = 17,
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
		.flags = CLK_MESON_PLL_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_gp1_pll_dco",
		.ops = &meson_clk_pll_ro_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "xtal",
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/* the bootloader runs the dsu from gp1_pll, cpufreq only reads it */
static struct clk_regmap sc2_fw_dsu_clk = {
	.data = &(struct clk_regmap_div_data){
		.offset = ANACTRL_GP1PLL_CTRL0,
		.shift = 16,
		.width = 3,
		.flags = CLK_DIVIDER_POWER_OF_TWO | CLK_DIVIDER_READ_ONLY,
	},
	.hw.init = &(struct clk_init_data){
		.name = "sc2_fw_dsu_clk",
		.ops = &clk_regmap_divider_ro_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&sc2_fw_gp1_pll_dco.hw
		},
		.num_parents = 1,
		.flags = CLK_GET_RATE_NOCACHE,
	},
};

/* registration order keeps every parent ahead of its children */
static struct clk_hw *const sc2_fw_cpu_order[] = {
	&sc2_fw_sys_pll_dco.hw,
	&sc2_fw_sys_pll.hw,
	&sc2_fw_cpu_clk_premux0.hw,
	&sc2_fw_cpu_clk_premux1.hw,
	&sc2_fw_cpu_clk_mux0_div.hw,
	&sc2_fw_cpu_clk_postmux0.hw,
	&sc2_fw_cpu_clk_mux1_div.hw,
	&sc2_fw_cpu_clk_postmux1.hw,
	&sc2_fw_cpu_clk_dyn.hw,
	&sc2_fw_cpu_clk.hw,
	&sc2_fw_gp1_pll_dco.hw,
	&sc2_fw_dsu_clk.hw,
};

/*
 * The cpu tree is only set up when the BL31 answers the legacy call and
 * the bootloader runs the dsu straight from gp1_pll, apart from the cpu
 * clock: CTRL6 bit 27 selects dsu_final while cpu1-3 follow cpu_clk, and
 * dsu_final, dsu_dyn and dyn0 of CTRL5 pass premux0 = gp1_pll through.
 * The dsu must not exceed 1.2 GHz, the rate the OPP tables allow for the
 * voltage of their lowest entries.
 */
#define SC2_FW_DSU_MAX_RATE	1200000000UL

static bool __init sc2_fw_cpu_clk_usable(struct regmap *pll_map)
{
	unsigned int ctrl0;
	unsigned long gp1;

	unsigned long ctrl = sc2_fw_cpuclk_smc(CLK_CPU_REG_RW, 0, 0, 0);
	unsigned long dsu = sc2_fw_cpuclk_smc(CLK_DSU_REG_RW, 0, 0, 0);
	unsigned long sync = sc2_fw_cpuclk_smc(CLK_CPU_DSU_REG_RW, 0, 0, 0);

	if ((long)ctrl == SMCCC_RET_NOT_SUPPORTED ||
	    (long)dsu == SMCCC_RET_NOT_SUPPORTED ||
	    (long)sync == SMCCC_RET_NOT_SUPPORTED)
		return false;

	if ((sync & GENMASK(27, 24)) != BIT(27) || (dsu & 0xc07) != 0x3)
		return false;

	/* gp1_pll = 24 MHz * m / n >> od */
	regmap_read(pll_map, ANACTRL_GP1PLL_CTRL0, &ctrl0);
	if (!((ctrl0 >> 10) & 0x1f))
		return false;
	gp1 = 24000000UL * (ctrl0 & 0xff) / ((ctrl0 >> 10) & 0x1f);
	gp1 >>= (ctrl0 >> 16) & 0x7;

	return gp1 <= SC2_FW_DSU_MAX_RATE;
}

static struct clk_regmap *const sc2_fw_pll_regmaps[] = {
	&sc2_fw_fixed_pll_dco,
	&sc2_fw_fixed_pll,
	&sc2_fw_fclk_50m,
	&sc2_fw_sys_pll_dco,
	&sc2_fw_sys_pll,
	&sc2_fw_gp1_pll_dco,
	&sc2_fw_dsu_clk,
};

static struct clk_regmap *const sc2_fw_sys_regmaps[] = {
	&sc2_fw_sysclk_a_sel,
	&sc2_fw_sysclk_a_div,
	&sc2_fw_sysclk_a,
	&sc2_fw_sysclk_b_sel,
	&sc2_fw_sysclk_b_div,
	&sc2_fw_sysclk_b,
	&sc2_fw_sys_clk,
};

/* registration order keeps every parent ahead of its children */
static struct clk_hw *const sc2_fw_order[] = {
	&sc2_fw_fixed_pll_dco.hw,
	&sc2_fw_fixed_pll.hw,
	&sc2_fw_fclk_div2.hw,
	&sc2_fw_fclk_div3.hw,
	&sc2_fw_fclk_div4.hw,
	&sc2_fw_fclk_div5.hw,
	&sc2_fw_fclk_div7.hw,
	&sc2_fw_sysclk_a_sel.hw,
	&sc2_fw_sysclk_a_div.hw,
	&sc2_fw_sysclk_a.hw,
	&sc2_fw_sysclk_b_sel.hw,
	&sc2_fw_sysclk_b_div.hw,
	&sc2_fw_sysclk_b.hw,
	&sc2_fw_sys_clk.hw,
	&sc2_fw_fclk_50m_div.hw,
	&sc2_fw_fclk_50m.hw,
};

static struct clk_hw *sc2_fw_hws[] = {
	[CLKID_SYS_CLK]			= &sc2_fw_sys_clk.hw,
	[CLKID_FIXED_PLL_DCO]		= &sc2_fw_fixed_pll_dco.hw,
	[CLKID_FIXED_PLL]		= &sc2_fw_fixed_pll.hw,
	[CLKID_FCLK_50M_DIV]		= &sc2_fw_fclk_50m_div.hw,
	[CLKID_FCLK_50M]		= &sc2_fw_fclk_50m.hw,
};

/*
 * IDs not served here stay with the SCMI provider: -EPROBE_DEFER lets the
 * lookup move on to it, or wait for it like without this provider
 */
static struct clk_hw *sc2_fw_clk_get(struct of_phandle_args *clkspec,
				     void *data)
{
	unsigned int idx = clkspec->args[0];

	if (idx >= ARRAY_SIZE(sc2_fw_hws) || !sc2_fw_hws[idx])
		return ERR_PTR(-EPROBE_DEFER);

	return sc2_fw_hws[idx];
}

static const struct regmap_config sc2_fw_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static struct regmap *__init sc2_fw_regmap(struct device_node *clkc,
					   int index)
{
	void __iomem *base = of_iomap(clkc, index);

	if (!base)
		return ERR_PTR(-ENOMEM);

	return regmap_init_mmio(NULL, base, &sc2_fw_regmap_config);
}

static void __init sc2_fw_clk_init(struct device_node *np)
{
	struct device_node *clkc;
	struct regmap *pll_map, *sys_map;
	unsigned int i;
	int ret;

	clkc = of_find_compatible_node(NULL, NULL, "amlogic,sc2-clkc");
	if (!clkc)
		return;

	sys_map = sc2_fw_regmap(clkc, 0);
	pll_map = sc2_fw_regmap(clkc, 1);
	of_node_put(clkc);
	if (IS_ERR(sys_map) || IS_ERR(pll_map)) {
		pr_err("%pOF: cannot map clock registers\n", np);
		return;
	}

	for (i = 0; i < ARRAY_SIZE(sc2_fw_pll_regmaps); i++)
		sc2_fw_pll_regmaps[i]->map = pll_map;
	for (i = 0; i < ARRAY_SIZE(sc2_fw_sys_regmaps); i++)
		sc2_fw_sys_regmaps[i]->map = sys_map;

	for (i = 0; i < ARRAY_SIZE(sc2_fw_order); i++) {
		ret = of_clk_hw_register(np, sc2_fw_order[i]);
		if (ret)
			goto err;
	}

	if (sc2_fw_cpu_clk_usable(pll_map)) {
		for (i = 0; i < ARRAY_SIZE(sc2_fw_cpu_order); i++) {
			ret = of_clk_hw_register(np, sc2_fw_cpu_order[i]);
			if (ret)
				break;
		}

		if (!ret)
			ret = clk_notifier_register(sc2_fw_cpu_clk_postmux0.hw.clk,
						    &sc2_fw_cpu_clk_postmux0_nb);
		if (!ret) {
			sc2_fw_hws[CLKID_CPU_CLK] = &sc2_fw_cpu_clk.hw;
			sc2_fw_hws[CLKID_DSU_CLK] = &sc2_fw_dsu_clk.hw;
		} else {
			pr_err("%pOF: failed to register cpu clocks: %d\n", np, ret);
		}
	}

	ret = of_clk_add_hw_provider(np, sc2_fw_clk_get, NULL);
	if (!ret)
		return;
err:
	pr_err("%pOF: failed to register clocks: %d\n", np, ret);
}

CLK_OF_DECLARE(sc2_scmi_fixed, "amlogic,sc2-scmi-fixed-clkc", sc2_fw_clk_init);
