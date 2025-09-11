/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_OF_CLK_COMMON_H
#define __AML_OF_CLK_COMMON_H

#include <linux/clk-provider.h>
#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#endif
#include <linux/platform_device.h>
#include <linux/regmap.h>

enum aml_clk_type {
	AML_CLK_MUX = 1,
	AML_CLK_DIV = 2,
	AML_CLK_GATE = 3,
	AML_CLK_DUALDIV = 4,
	AML_CLK_GENERAL = 5,
	AML_CLK_NOGLITCH = 6,
	AML_CLK_PLL = 7
};

struct aml_clk {
	struct clk_hw	hw;
	enum aml_clk_type type;
	struct regmap	*map;
	u64		min_rate;
	u64		max_rate;
	void		*data;
};

static inline struct aml_clk *to_aml_clk(struct clk_hw *hw)
{
	return container_of(hw, struct aml_clk, hw);
}

struct regmap *aml_clk_regmap_init(struct platform_device *pdev);

#ifdef CONFIG_DEBUG_FS
extern const struct file_operations aml_clk_type_fops;
#endif

#ifdef CONFIG_OF
u32 of_aml_clk_get_count(struct device_node *np);
int of_aml_clk_get_init_reg_count(struct device_node *np);
int of_aml_clk_get_init_reg(struct device_node *np, int reg_count,
			    struct reg_sequence *init_regs);
int of_aml_clk_get_clkid_index(struct device_node *np, u32 index);
const char *of_aml_clk_get_name_index(struct device_node *np, u32 index);
int of_aml_clk_get_flag_index(struct device_node *np, u32 index, u32 *d_flag,
			      u32 *hw_flag);
unsigned int of_aml_clk_get_parent_count(struct device *dev, int index);
struct clk_parent_data *of_aml_clk_get_parent_data(struct device *dev,
						   struct clk_hw **dev_hws,
						   int index);
void of_aml_clk_parent_data_release(struct device *dev,
				    struct clk_parent_data *data);
u32 *of_aml_clk_get_parent_table(struct device *dev, int index);
int of_aml_clk_get_div_width_index(struct device_node *np, u32 index);
int of_aml_clk_get_rate_range(struct device_node *np, u32 index, u64 *min, u64 *max);
#endif  /* CONFIG_OF */

#endif  /* __AML_OF_CLK_COMMON_H */
