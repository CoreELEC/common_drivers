// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include "clk-regmap.h"
#include "c4.h"
#include <dt-bindings/clock/amlogic,c4-clkc.h>

static struct clk_regmap ao_rough_32k_clk_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_ROUGH_32K_CTRL,
		.shift = 0,
		.width = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rough_32k_clk_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "osc_32m"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap ao_rough_32k_clk = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_ROUGH_32K_CTRL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rough_32k_clk",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_rough_32k_clk_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ao_oscin32k_clk_parent_data[] = {
	{ .hw = &ao_rough_32k_clk.hw },
	{ .fw_name = "xtal_32k" }
};

static struct clk_regmap ao_oscin32k_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_OSCIN_CTRL,
		.mask = 0x3,
		.shift = 2,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_oscin32k_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_oscin32k_clk_parent_data,
		.num_parents = ARRAY_SIZE(ao_oscin32k_clk_parent_data),
	},
};

static const struct clk_parent_data ao_oscin_clk_parent_data[] = {
	{ .fw_name = "xtal" },
	{ .hw = &ao_oscin32k_clk.hw }
};

static struct clk_regmap ao_oscin_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_OSCIN_CTRL,
		.mask = 0x1,
		.shift = 1,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_oscin_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_oscin_clk_parent_data,
		.num_parents = ARRAY_SIZE(ao_oscin_clk_parent_data),
	},
};

static const struct clk_parent_data ao_sys_clk_parent_data[] = {
	{ .fw_name = "sys_clk" },
	{ .hw = &ao_oscin_clk.hw }
};

static struct clk_regmap ao_sys_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_OSCIN_CTRL,
		.mask = 0x1,
		.shift = 0,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_sys_clk",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_sys_clk_parent_data,
		.num_parents = ARRAY_SIZE(ao_sys_clk_parent_data),
	},
};

static const struct clk_parent_data ao_pwm_parent_data[] = {
	{ .hw = &ao_oscin_clk.hw },
	{ .hw = &ao_sys_clk.hw },
	{ .hw = &ao_oscin32k_clk.hw },
	{ .fw_name = "xtal" }
};

static struct clk_regmap ao_pwm_j_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_JK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_j_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_j_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_JK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_j_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_j_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_j = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_JK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_j",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_j_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_k_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PWM_CLK_JK_CTRL,
		.mask = 0x3,
		.shift = 25,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_k_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_pwm_parent_data,
		.num_parents = ARRAY_SIZE(ao_pwm_parent_data),
	},
};

static struct clk_regmap ao_pwm_k_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PWM_CLK_JK_CTRL,
		.shift = 16,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_k_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_k_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_pwm_k = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PWM_CLK_JK_CTRL,
		.bit_idx = 24,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_pwm_k",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_pwm_k_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

#define MESON_CLK_GATE_AO_SYS_CLK(_name, _reg, _bit)                  \
struct clk_regmap _name = {                                           \
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

MESON_CLK_GATE_AO_SYS_CLK(ao_sys_apb_clk, AO_CLKCTRL_RTC_SYS_CLK, 0);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_reg, AO_CLKCTRL_RTC_SYS_CLK, 1);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_clk, AO_CLKCTRL_RTC_SYS_CLK, 2);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_reset_ctrl, AO_CLKCTRL_RTC_SYS_CLK, 3);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pad_ctrl, AO_CLKCTRL_RTC_SYS_CLK, 4);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_dig, AO_CLKCTRL_RTC_SYS_CLK, 5);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_irq_ctrl, AO_CLKCTRL_RTC_SYS_CLK, 6);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwr_ctrl, AO_CLKCTRL_RTC_SYS_CLK, 7);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwm_j, AO_CLKCTRL_RTC_SYS_CLK, 8);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwm_k, AO_CLKCTRL_RTC_SYS_CLK, 9);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_sed, AO_CLKCTRL_RTC_SYS_CLK, 10);

static struct clk_regmap *const aoclk_regmaps[] = {
	&ao_rough_32k_clk_div,
	&ao_rough_32k_clk,
	&ao_oscin32k_clk,
	&ao_oscin_clk,
	&ao_sys_clk,
	&ao_pwm_j_mux,
	&ao_pwm_j_div,
	&ao_pwm_j,
	&ao_pwm_k_mux,
	&ao_pwm_k_div,
	&ao_pwm_k,
	&ao_sys_apb_clk,
	&ao_sys_rtc_reg,
	&ao_sys_rtc_clk,
	&ao_sys_reset_ctrl,
	&ao_sys_pad_ctrl,
	&ao_sys_rtc_dig,
	&ao_sys_irq_ctrl,
	&ao_sys_pwr_ctrl,
	&ao_sys_pwm_j,
	&ao_sys_pwm_k,
	&ao_sys_sed
};

static struct clk_hw *c4_aoclk_hw_clks[] = {
	[CLKID_AO_ROUGH_32K_CLK_DIV]  = &ao_rough_32k_clk_div.hw,
	[CLKID_AO_ROUGH_32K_CLK]      = &ao_rough_32k_clk.hw,
	[CLKID_AO_OSCIN32K_CLK]       = &ao_oscin32k_clk.hw,
	[CLKID_AO_OSCIN_CLK]          = &ao_oscin_clk.hw,
	[CLKID_AO_SYS_CLK]            = &ao_sys_clk.hw,
	[CLKID_AO_PWM_J_MUX]          = &ao_pwm_j_mux.hw,
	[CLKID_AO_PWM_J_DIV]          = &ao_pwm_j_div.hw,
	[CLKID_AO_PWM_J]              = &ao_pwm_j.hw,
	[CLKID_AO_PWM_K_MUX]          = &ao_pwm_k_mux.hw,
	[CLKID_AO_PWM_K_DIV]          = &ao_pwm_k_div.hw,
	[CLKID_AO_PWM_K]              = &ao_pwm_k.hw,
	[CLKID_AO_SYS_APB_CLK]        = &ao_sys_apb_clk.hw,
	[CLKID_AO_SYS_RTC_REG]        = &ao_sys_rtc_reg.hw,
	[CLKID_AO_SYS_RTC_CLK]        = &ao_sys_rtc_clk.hw,
	[CLKID_AO_SYS_RESET_CTRL]     = &ao_sys_reset_ctrl.hw,
	[CLKID_AO_SYS_PAD_CTRL]       = &ao_sys_pad_ctrl.hw,
	[CLKID_AO_SYS_RTC_DIG]        = &ao_sys_rtc_dig.hw,
	[CLKID_AO_SYS_IRQ_CTRL]       = &ao_sys_irq_ctrl.hw,
	[CLKID_AO_SYS_PWR_CTRL]       = &ao_sys_pwr_ctrl.hw,
	[CLKID_AO_SYS_PWM_J]          = &ao_sys_pwm_j.hw,
	[CLKID_AO_SYS_PWM_K]          = &ao_sys_pwm_k.hw,
	[CLKID_AO_SYS_SED]            = &ao_sys_sed.hw
};

static const struct meson_clk_hw_data c4_aoclks = {
	.hws = c4_aoclk_hw_clks,
	.num = ARRAY_SIZE(c4_aoclk_hw_clks),
};

static int meson_c4_aoclk_probe(struct platform_device *pdev)
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
		.compatible = "amlogic,c4-aoclkc",
		.data = &c4_aoclks
	},
	{}
};

static struct platform_driver c4_aoclk_driver = {
	.probe			= meson_c4_aoclk_probe,
	.driver			= {
		.name		= "c4-aoclkc",
		.of_match_table	= clkc_match_table,
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
int __init c4_aoclk_init(void)
{
	return platform_driver_register(&c4_aoclk_driver);
}

void __exit c4_aoclk_exit(void)
{
	platform_driver_unregister(&c4_aoclk_driver);
}
#else
builtin_platform_driver(c4_aoclk_driver);
MODULE_LICENSE("GPL v2");
#endif
