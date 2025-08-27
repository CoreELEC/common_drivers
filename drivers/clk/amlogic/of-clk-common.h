/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2025 Amlogic, inc.
 * Author: Chuan Liu <chuan.liu@amlogic.com>
 */

#ifndef __AML_OF_CLK_COMMON_H
#define __AML_OF_CLK_COMMON_H

#include <linux/clk-provider.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

struct aml_clk {
	struct clk_hw	hw;
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

#ifdef CONFIG_OF
u32 of_get_aml_clk_count(struct device_node *np);
int of_get_aml_clk_clkid_index(struct device_node *np, u32 index);
const char *of_get_aml_clk_name_index(struct device_node *np, u32 index);
int of_get_aml_clk_flag_index(struct device_node *np, u32 index, u32 *d_flag,
			      u32 *hw_flag);
unsigned int of_get_aml_clk_parent_count(struct device *dev, int index);
struct clk_parent_data *of_get_aml_clk_parent_data(struct device *dev,
						   struct clk_hw **dev_hws,
						   int index);
void of_aml_clk_parent_data_release(struct device *dev,
				    struct clk_parent_data *data);
u32 *of_get_aml_clk_parent_table(struct device *dev, int index);
int of_get_aml_clk_div_width_index(struct device_node *np, u32 index);
#endif  /* CONFIG_OF */

#endif  /* __AML_OF_CLK_COMMON_H */
