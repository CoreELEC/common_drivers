// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "of-clk-common.h"
#include "clk-generals.h"

#define CLK_NAME_MAX_STR_SIZE		64

#define CLK_GENERAL_MUX_SHIFT		9
#define CLK_GENERAL_MUX_MASK		0x7

#define CLK_GENERAL_DIV_SHIFT		0
#define CLK_GENERAL_DIV_WIDTH		8

#define CLK_GENERAL_GATE_SHIFT		8
#define CLK_GENERAL_GATE_MASK		0x1

static u8 aml_clk_general_get_parent(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;
	unsigned int val;
	int ret;

	ret = regmap_read(clk->map, general->reg_offset, &val);
	if (ret)
		return ret;

	val >>=  CLK_GENERAL_MUX_SHIFT;
	val &= CLK_GENERAL_MUX_MASK;

	return clk_mux_val_to_index(hw, general->mux_table, 0, val);
}

static int aml_clk_general_set_parent(struct clk_hw *hw, u8 index)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;
	unsigned int val = clk_mux_index_to_val(general->mux_table, 0, index);
	int mux_shift = general->bit_offset + CLK_GENERAL_MUX_SHIFT;

	return regmap_update_bits(clk->map, general->reg_offset,
				  CLK_GENERAL_MUX_MASK << mux_shift,
				  val << mux_shift);
}

static unsigned long aml_clk_general_recalc_rate(struct clk_hw *hw,
						 unsigned long parent_rate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;
	unsigned int val;
	int ret;

	ret = regmap_read(clk->map, general->reg_offset, &val);
	if (ret)
		/* Gives a hint that something is wrong */
		return 0;

	val >>= general->bit_offset + CLK_GENERAL_DIV_SHIFT;
	val &= clk_div_mask(general->div_width);

	return divider_recalc_rate(hw, parent_rate, val, NULL, 0, general->div_width);
}

static int aml_clk_general_determine_rate_for_parent(struct clk_hw *rate_hw,
						     struct clk_rate_request *req,
						     struct clk_hw *parent_hw)
{
	struct aml_clk *clk = to_aml_clk(rate_hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;

	req->best_parent_hw = parent_hw;
	req->best_parent_rate = clk_hw_get_rate(parent_hw);

	return divider_determine_rate(rate_hw, req, NULL, general->div_width, 0);
}

static int aml_clk_general_determine_rate(struct clk_hw *hw,
					  struct clk_rate_request *req)
{
	struct clk_hw *parent;
	unsigned long rate_diff;
	unsigned long best_rate_diff = ULONG_MAX;
	unsigned long best_rate = 0;
	int i, ret;

	req->best_parent_hw = NULL;

	if (clk_hw_get_flags(hw) & CLK_SET_RATE_NO_REPARENT) {
		struct clk_rate_request tmp_req;

		parent = clk_hw_get_parent(hw);

		clk_hw_forward_rate_request(hw, req, parent, &tmp_req, req->rate);
		ret = aml_clk_general_determine_rate_for_parent(hw, &tmp_req, parent);
		if (ret)
			return ret;

		req->rate = tmp_req.rate;
		req->best_parent_hw = tmp_req.best_parent_hw;
		req->best_parent_rate = tmp_req.best_parent_rate;

		return 0;
	}

	for (i = 0; i < clk_hw_get_num_parents(hw); i++) {
		struct clk_rate_request tmp_req;

		parent = clk_hw_get_parent_by_index(hw, i);
		if (!parent)
			continue;

		clk_hw_forward_rate_request(hw, req, parent, &tmp_req, req->rate);
		ret = aml_clk_general_determine_rate_for_parent(hw, &tmp_req, parent);
		if (ret)
			continue;

		if (req->rate >= tmp_req.rate)
			rate_diff = req->rate - tmp_req.rate;
		else
			rate_diff = tmp_req.rate - req->rate;

		if (!rate_diff || !req->best_parent_hw ||
		    best_rate_diff > rate_diff) {
			req->best_parent_hw = parent;
			req->best_parent_rate = tmp_req.best_parent_rate;
			best_rate_diff = rate_diff;
			best_rate = tmp_req.rate;
		}

		if (!rate_diff)
			return 0;
	}

	req->rate = best_rate;
	return 0;
}

static int aml_clk_general_set_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long parent_rate)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;
	unsigned int val;
	int ret;
	int div_shift = general->bit_offset + CLK_GENERAL_DIV_SHIFT;

	ret = divider_get_val(rate, parent_rate, NULL, general->div_width, 0);
	if (ret < 0)
		return ret;

	val = (unsigned int)ret << div_shift;
	return regmap_update_bits(clk->map, general->reg_offset,
				  clk_div_mask(general->div_width) << div_shift, val);
}

static int aml_clk_general_set_rate_and_parent(struct clk_hw *hw,
					       unsigned long rate,
					       unsigned long parent_rate,
					       u8 index)
{
	unsigned long temp_rate;

	temp_rate = aml_clk_general_recalc_rate(hw, parent_rate);
	if (temp_rate > rate) {
		aml_clk_general_set_rate(hw, rate, parent_rate);
		aml_clk_general_set_parent(hw, index);
	} else {
		aml_clk_general_set_parent(hw, index);
		aml_clk_general_set_rate(hw, rate, parent_rate);
	}

	return 0;
}

static int aml_clk_general_is_enabled(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;
	unsigned int val;

	regmap_read(clk->map, general->reg_offset, &val);
	val &= BIT(general->bit_offset + CLK_GENERAL_GATE_SHIFT);

	return val ? 1 : 0;
}

static int aml_clk_general_enable(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;
	u8 bit_idx = general->bit_offset + CLK_GENERAL_GATE_SHIFT;

	return regmap_update_bits(clk->map, general->reg_offset,
				  BIT(bit_idx), BIT(bit_idx));
}

static void aml_clk_general_disable(struct clk_hw *hw)
{
	struct aml_clk *clk = to_aml_clk(hw);
	struct aml_clk_general_data *general = (struct aml_clk_general_data *)clk->data;
	u8 bit_idx = general->bit_offset + CLK_GENERAL_GATE_SHIFT;

	regmap_update_bits(clk->map, general->reg_offset,
			   BIT(bit_idx), 0);
}

const struct clk_ops aml_clk_general_ops = {
	.get_parent = aml_clk_general_get_parent,
	.set_parent = aml_clk_general_set_parent,
	.determine_rate = aml_clk_general_determine_rate,
	.recalc_rate = aml_clk_general_recalc_rate,
	.set_rate = aml_clk_general_set_rate,
	.set_rate_and_parent = aml_clk_general_set_rate_and_parent,
	.enable = aml_clk_general_enable,
	.disable = aml_clk_general_disable,
	.is_enabled = aml_clk_general_is_enabled,
};
EXPORT_SYMBOL_GPL(aml_clk_general_ops);

#ifdef CONFIG_OF
static struct aml_clk_general_data *
of_get_aml_clk_general_data(struct device *dev, u32 index)
{
	struct device_node *dev_np = dev_of_node(dev);
	struct aml_clk_general_data *general;
	const __be32 *prop;
	unsigned int psize;
	int elems_size = 2 * sizeof(u32);
	u32 u32_index = 2 * index;
	int div_width;

	prop = of_get_property(dev_np, "clock-reg-offsets", &psize);
	if (!prop || psize < (index + 1) * elems_size)
		return ERR_PTR(-EINVAL);

	div_width = of_get_aml_clk_div_width_index(dev_np, index);
	if (div_width < 0)
		return ERR_PTR(-EINVAL);

	general = devm_kzalloc(dev, sizeof(*general), GFP_KERNEL);
	if (!general)
		return ERR_PTR(-ENOMEM);

	general->reg_offset = be32_to_cpu(prop[u32_index]);
	general->bit_offset = be32_to_cpu(prop[u32_index + 1]);
	general->div_width = div_width ? div_width : CLK_GENERAL_DIV_WIDTH;
	general->mux_table = of_get_aml_clk_parent_table(dev, index);

	return general;
}

static int aml_clk_general_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dev_np = dev_of_node(dev);
	struct regmap *regmap;
	int clkid, ret, i;
	int clk_num;
	const char *clk_name;
	struct clk_hw_onecell_data *clk_data;
	struct aml_clk *clk;
	struct aml_clk_general_data *general;
	struct clk_parent_data *parent_data;
	struct clk_init_data init = {
		.ops = &aml_clk_general_ops,
	};

	clk_num = of_get_aml_clk_count(dev_np);
	if (clk_num == 0)
		return -EINVAL;

	clk_data = devm_kzalloc(dev, struct_size(clk_data, hws, clk_num),
				GFP_KERNEL);
	if (!clk_data)
		return -ENOMEM;

	clk_data->num = clk_num;
	clk = devm_kcalloc(dev, clk_num, sizeof(*clk), GFP_KERNEL);
	if (!clk)
		return -ENOMEM;

	for (i = 0; i < clk_num; i++)
		clk_data->hws[i] = &clk[i].hw;

	regmap = aml_clk_regmap_init(pdev);
	if (IS_ERR_OR_NULL(regmap))
		return -EIO;

	for (i = 0, clkid = 0; i < clk_num; i++) {
		clkid = of_get_aml_clk_clkid_index(dev_np, i);
		if (clkid < 0 && clk_num > 1)
			return -EINVAL;

		clk_name = of_get_aml_clk_name_index(dev_np, i);
		if (!clk_name)
			return -EINVAL;

		init.name = clk_name;
		init.num_parents = of_get_aml_clk_parent_count(dev, i);
		parent_data = of_get_aml_clk_parent_data(dev, clk_data->hws, i);
		if (IS_ERR(parent_data)) {
			if (PTR_ERR(parent_data) != -EPROBE_DEFER)
				dev_err(dev, "Get parent_data failed!\n");

			return PTR_ERR(parent_data);
		}
		init.parent_data = parent_data;

		clk[clkid].hw.init = &init;
		clk[clkid].map = regmap;
		general = of_get_aml_clk_general_data(dev, i);
		if (IS_ERR(general))
			return PTR_ERR(general);

		clk[clkid].data = general;

		ret = devm_clk_hw_register(dev, &clk[clkid].hw);
		if (ret)
			return ret;

		of_aml_clk_parent_data_release(dev, parent_data);
	}

	if (clk_num == 1)
		return devm_of_clk_add_hw_provider(dev, of_clk_hw_simple_get,
						   &clk->hw);
	else
		return devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
						   clk_data);
}

static const struct of_device_id aml_clk_general_match_table[] = {
	{
		.compatible = "amlogic,general-clk",
	},
	{}
};

static struct platform_driver aml_clk_general_driver = {
	.probe		= aml_clk_general_probe,
	.driver		= {
		.name	= "general-clk",
		.of_match_table = aml_clk_general_match_table,
	},
};

#endif  /* CONFIG_OF */

#ifdef CONFIG_AMLOGIC_MODIFY
int __init clk_general_init(void)
{
	return platform_driver_register(&aml_clk_general_driver);
}

void __exit clk_general_exit(void)
{
}
#else
MODULE_DEVICE_TABLE(of, aml_clk_general_match_table);
module_platform_driver(aml_clk_general_driver);

MODULE_DESCRIPTION("Amlogic general clock driver");
MODULE_AUTHOR("Chuan Liu <chuan.liu@amlogic.com>");
MODULE_IMPORT_NS("CLK_AMLOGIC");
MODULE_LICENSE("GPL v2");
#endif  /* CONFIG_AMLOGIC_MODIFY */
