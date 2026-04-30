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
#include "c5.h"
#include <dt-bindings/clock/amlogic,c5-clkc.h>

static struct clk_regmap ao_rough_32k_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_ROUGH_32K_CTRL,
		.shift = 0,
		.width = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rough_32k_div",
		.ops = &clk_regmap_divider_ops,
		.parent_data = &(const struct clk_parent_data) {
			.fw_name = "osc_32m"
		},
		.num_parents = 1,
	},
};

static struct clk_regmap ao_rough_32k = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_ROUGH_32K_CTRL,
		.bit_idx = 20,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_rough_32k",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_rough_32k_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ao_oscin32k_clk_parent_data[] = {
	{ .hw = &ao_rough_32k.hw },
	{ .fw_name = "xtal_32k" }
};

static struct clk_regmap ao_oscin32k_clk = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_OSCIN_CTRL,
		.mask = 0x1,
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

static const struct clk_parent_data ao_psram_parent_data[] = {
	{ .hw = &ao_oscin_clk.hw },
	{ .fw_name = "fclk_div5" },
	{ .fw_name = "fclk_div4" },
	{ .fw_name = "fclk_div3" }
};

static struct clk_regmap ao_psram_mux = {
	.data = &(struct clk_regmap_mux_data) {
		.offset = AO_CLKCTRL_PSRAM_CLK_CTRL,
		.mask = 0x3,
		.shift = 9,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_psram_mux",
		.ops = &clk_regmap_mux_ops,
		.parent_data = ao_psram_parent_data,
		.num_parents = ARRAY_SIZE(ao_psram_parent_data),
	},
};

static struct clk_regmap ao_psram_div = {
	.data = &(struct clk_regmap_div_data) {
		.offset = AO_CLKCTRL_PSRAM_CLK_CTRL,
		.shift = 0,
		.width = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_psram_div",
		.ops = &clk_regmap_divider_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_psram_mux.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static struct clk_regmap ao_psram = {
	.data = &(struct clk_regmap_gate_data) {
		.offset = AO_CLKCTRL_PSRAM_CLK_CTRL,
		.bit_idx = 8,
	},
	.hw.init = &(struct clk_init_data) {
		.name = "ao_psram",
		.ops = &clk_regmap_gate_ops,
		.parent_hws = (const struct clk_hw *[]) {
			&ao_psram_div.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
	},
};

static const struct clk_parent_data ao_pwm_parent_data[] = {
	{ .hw = &ao_oscin_clk.hw },
	{ .hw = &ao_sys_clk.hw },
	{ .hw = &ao_oscin32k_clk.hw },
	{ .fw_name = "fclk_div5" }
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

MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_reg, AO_CLKCTRL_RTC_SYS_CLK0, 1);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_clktree, AO_CLKCTRL_RTC_SYS_CLK0, 2);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rst_ctrl, AO_CLKCTRL_RTC_SYS_CLK0, 3);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_clk_pad, AO_CLKCTRL_RTC_SYS_CLK0, 4);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_dig, AO_CLKCTRL_RTC_SYS_CLK0, 5);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_irq, AO_CLKCTRL_RTC_SYS_CLK0, 6);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwrctrl, AO_CLKCTRL_RTC_SYS_CLK0, 7);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwm_a, AO_CLKCTRL_RTC_SYS_CLK0, 8);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_pwm_b, AO_CLKCTRL_RTC_SYS_CLK0, 9);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_i3c, AO_CLKCTRL_RTC_SYS_CLK0, 10);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_spisg0, AO_CLKCTRL_RTC_SYS_CLK0, 11);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_ir_ctrl, AO_CLKCTRL_RTC_SYS_CLK0, 12);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_i2c_m_a, AO_CLKCTRL_RTC_SYS_CLK0, 15);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_i2c_m_b, AO_CLKCTRL_RTC_SYS_CLK0, 16);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_sed, AO_CLKCTRL_RTC_SYS_CLK0, 19);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_uart_a, AO_CLKCTRL_RTC_SYS_CLK0, 20);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_uart_b, AO_CLKCTRL_RTC_SYS_CLK0, 21);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_ao_spi, AO_CLKCTRL_RTC_SYS_CLK0, 25);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_rtc_secure, AO_CLKCTRL_RTC_SYS_CLK0, 26);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_ao_isp, AO_CLKCTRL_RTC_SYS_CLK0, 27);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_aocpu, AO_CLKCTRL_RTC_SYS_CLK0, 28);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_sram, AO_CLKCTRL_RTC_SYS_CLK0, 29);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_psram_apb, AO_CLKCTRL_RTC_SYS_CLK0, 30);
MESON_CLK_GATE_AO_SYS_CLK(ao_sys_psram_axi, AO_CLKCTRL_RTC_SYS_CLK0, 31);

static struct clk_regmap *const aoclk_regmaps[] = {
	&ao_rough_32k_div,
	&ao_rough_32k,
	&ao_oscin32k_clk,
	&ao_oscin_clk,
	&ao_sys_clk,
	&ao_psram_mux,
	&ao_psram_div,
	&ao_psram,
	&ao_pwm_a_mux,
	&ao_pwm_a_div,
	&ao_pwm_a,
	&ao_pwm_b_mux,
	&ao_pwm_b_div,
	&ao_pwm_b,
	&ao_sys_rtc_reg,
	&ao_sys_clktree,
	&ao_sys_rst_ctrl,
	&ao_sys_clk_pad,
	&ao_sys_rtc_dig,
	&ao_sys_irq,
	&ao_sys_pwrctrl,
	&ao_sys_pwm_a,
	&ao_sys_pwm_b,
	&ao_sys_i3c,
	&ao_sys_spisg0,
	&ao_sys_ir_ctrl,
	&ao_sys_i2c_m_a,
	&ao_sys_i2c_m_b,
	&ao_sys_sed,
	&ao_sys_uart_a,
	&ao_sys_uart_b,
	&ao_sys_ao_spi,
	&ao_sys_rtc_secure,
	&ao_sys_ao_isp,
	&ao_sys_aocpu,
	&ao_sys_sram,
	&ao_sys_psram_apb,
	&ao_sys_psram_axi
};

static struct clk_hw *c5_aoclk_hw_clks[] = {
	[CLKID_AO_ROUGH_32K_DIV]      = &ao_rough_32k_div.hw,
	[CLKID_AO_ROUGH_32K]          = &ao_rough_32k.hw,
	[CLKID_AO_OSCIN32K_CLK]       = &ao_oscin32k_clk.hw,
	[CLKID_AO_OSCIN_CLK]          = &ao_oscin_clk.hw,
	[CLKID_AO_SYS_CLK]            = &ao_sys_clk.hw,
	[CLKID_AO_PSRAM_MUX]          = &ao_psram_mux.hw,
	[CLKID_AO_PSRAM_DIV]          = &ao_psram_div.hw,
	[CLKID_AO_PSRAM]              = &ao_psram.hw,
	[CLKID_AO_PWM_A_MUX]          = &ao_pwm_a_mux.hw,
	[CLKID_AO_PWM_A_DIV]          = &ao_pwm_a_div.hw,
	[CLKID_AO_PWM_A]              = &ao_pwm_a.hw,
	[CLKID_AO_PWM_B_MUX]          = &ao_pwm_b_mux.hw,
	[CLKID_AO_PWM_B_DIV]          = &ao_pwm_b_div.hw,
	[CLKID_AO_PWM_B]              = &ao_pwm_b.hw,
	[CLKID_AO_SYS_RTC_REG]        = &ao_sys_rtc_reg.hw,
	[CLKID_AO_SYS_CLKTREE]        = &ao_sys_clktree.hw,
	[CLKID_AO_SYS_RST_CTRL]       = &ao_sys_rst_ctrl.hw,
	[CLKID_AO_SYS_CLK_PAD]        = &ao_sys_clk_pad.hw,
	[CLKID_AO_SYS_RTC_DIG]        = &ao_sys_rtc_dig.hw,
	[CLKID_AO_SYS_IRQ]            = &ao_sys_irq.hw,
	[CLKID_AO_SYS_PWRCTRL]        = &ao_sys_pwrctrl.hw,
	[CLKID_AO_SYS_PWM_A]          = &ao_sys_pwm_a.hw,
	[CLKID_AO_SYS_PWM_B]          = &ao_sys_pwm_b.hw,
	[CLKID_AO_SYS_I3C]            = &ao_sys_i3c.hw,
	[CLKID_AO_SYS_SPISG0]         = &ao_sys_spisg0.hw,
	[CLKID_AO_SYS_IR_CTRL]        = &ao_sys_ir_ctrl.hw,
	[CLKID_AO_SYS_I2C_M_A]        = &ao_sys_i2c_m_a.hw,
	[CLKID_AO_SYS_I2C_M_B]        = &ao_sys_i2c_m_b.hw,
	[CLKID_AO_SYS_SED]            = &ao_sys_sed.hw,
	[CLKID_AO_SYS_UART_A]         = &ao_sys_uart_a.hw,
	[CLKID_AO_SYS_UART_B]         = &ao_sys_uart_b.hw,
	[CLKID_AO_SYS_AO_SPI]         = &ao_sys_ao_spi.hw,
	[CLKID_AO_SYS_RTC_SECURE]     = &ao_sys_rtc_secure.hw,
	[CLKID_AO_SYS_AO_ISP]         = &ao_sys_ao_isp.hw,
	[CLKID_AO_SYS_AOCPU]          = &ao_sys_aocpu.hw,
	[CLKID_AO_SYS_SRAM]           = &ao_sys_sram.hw,
	[CLKID_AO_SYS_PSRAM_APB]      = &ao_sys_psram_apb.hw,
	[CLKID_AO_SYS_PSRAM_AXI]      = &ao_sys_psram_axi.hw
};

static const struct meson_clk_hw_data c5_aoclks = {
	.hws = c5_aoclk_hw_clks,
	.num = ARRAY_SIZE(c5_aoclk_hw_clks),
};

static int meson_c5_aoclk_probe(struct platform_device *pdev)
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
		.compatible = "amlogic,c5-aoclkc",
		.data = &c5_aoclks
	},
	{}
};

static struct platform_driver c5_aoclk_driver = {
	.probe			= meson_c5_aoclk_probe,
	.driver			= {
		.name		= "c5-aoclkc",
		.of_match_table	= clkc_match_table,
	},
};

#ifdef CONFIG_AMLOGIC_MODIFY
int __init c5_aoclk_init(void)
{
	return platform_driver_register(&c5_aoclk_driver);
}

void __exit c5_aoclk_exit(void)
{
	platform_driver_unregister(&c5_aoclk_driver);
}
#else
builtin_platform_driver(c5_aoclk_driver);
MODULE_LICENSE("GPL v2");
#endif
