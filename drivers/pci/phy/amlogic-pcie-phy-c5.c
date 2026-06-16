// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/phy/phy.h>
#include <linux/reset.h>
#include <linux/platform_device.h>
#include <linux/bitfield.h>
#include <dt-bindings/phy/phy.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/bits.h>
#include "amlogic-pcie-phy.h"

static const struct pcie_reset_seq reset_info[] = {
	{ "mac_apb",	10 },
	{ "por",	10 },
	{ "rst0",	0 },
	{ "rst1",	0 },
	{ "rst2",	0 },
	{ "rst3",	0 },
	{ "rst4",	0 },
	{ "rst5",	0 },
	{ "rst6",	0 },
	{ "rst7",	10 },
	{ "phy",	10 },
	{ "pipe",	10 },
	{ "phy_apb",	10 },
};

static int amlogic_pcie_phy_c5_power_on(struct amlogic_pcie_phy *pcie_phy)
{
	/* int ret; */

	return 0;
}

static void amlogic_pcie_phy_do_reset(struct amlogic_pcie_phy *pcie_phy, int cnt, u32 flag)
{
	struct device *dev = pcie_phy->dev;
	u32 reg[2], val;
	int ret = 0;

	ret = of_property_read_u32_array(dev_of_node(dev),
					 kasprintf(GFP_KERNEL, "pcie_rst_%d", cnt),
					 reg, ARRAY_SIZE(reg));
	if (ret) {
		dev_err(dev, "pcie_rst setting read failed\n");
		return;
	}

	if (flag == 0) {
		val = readl(pcie_phy->reset_base + reg[0]);
		val &= ~(reg[1]);
		writel(val, pcie_phy->reset_base + reg[0]);
		dev_dbg(dev, "assert cnt=%d, reg[0]=0x%x, reg[1]=0x%x\n", cnt, reg[0], reg[1]);
	} else {
		val = readl(pcie_phy->reset_base + reg[0]);
		val |= (reg[1]);
		writel(val, pcie_phy->reset_base + reg[0]);
		dev_dbg(dev, "deassert cnt=%d, reg[0]=0x%x, reg[1]=0x%x\n", cnt, reg[0], reg[1]);
	}
}

static int amlogic_pcie_phy_assert_reset(struct amlogic_pcie_phy *pcie_phy)
{
	struct device *dev = pcie_phy->dev;
	int ret = 0, i;

	if (pcie_phy->reset_base)
		goto set_rst_reg;

	for (i = 0; i < pcie_phy->reset_number; i++) {
		ret = reset_control_assert(pcie_phy->resets[i].rstc);
		if (ret < 0) {
			dev_err(dev, "Failed to assert '%s': %d\n", pcie_phy->resets[i].id, ret);
			return ret;
		}
	}

	return 0;

set_rst_reg:
	for (i = 1; i <= pcie_phy->reset_number; i++)
		amlogic_pcie_phy_do_reset(pcie_phy, i, 0);

	return 0;
}

static int amlogic_pcie_phy_deassert_reset(struct amlogic_pcie_phy *pcie_phy)
{
	struct device *dev = pcie_phy->dev;
	int ret = 0, i;

	if (pcie_phy->reset_base)
		goto set_rst_reg;

	for (i = 0; i < pcie_phy->reset_number; i++) {
		ret = reset_control_deassert(pcie_phy->resets[i].rstc);
		if (ret < 0) {
			dev_err(dev, "Failed to deassert '%s': %d\n", pcie_phy->resets[i].id, ret);
			return ret;
		}

		if (reset_info[i].delay_us)
			usleep_range(reset_info[i].delay_us, reset_info[i].delay_us + 10);
	}

	return 0;

set_rst_reg:
	for (i = 1; i <= pcie_phy->reset_number; i++)
		amlogic_pcie_phy_do_reset(pcie_phy, i, 1);

	return 0;
}

static int amlogic_pcie_phy_do_pll_set(struct amlogic_pcie_phy *pcie_phy, int cnt)
{
	struct device *dev = pcie_phy->dev;
	u32 reg[2], val;
	int ret;

	ret = of_property_read_u32_array(dev_of_node(dev),
					 kasprintf(GFP_KERNEL, "phy_pll_setting_%d", cnt),
					 reg, ARRAY_SIZE(reg));
	if (ret) {
		dev_err(dev, "amlogic phy pll setting read failed\n");
		return ret;
	}

	if (reg[0] != 0x017c && reg[0] != 0x117c) {
		ret = regmap_write(pcie_phy->regmap, reg[0], reg[1]);
		if (ret)
			return ret;
		usleep_range(50, 100);
	} else {
		ret = regmap_read_poll_timeout(pcie_phy->regmap, reg[0], val,
					       (val & reg[1]), 1, 10000);
		if (ret)
			dev_err(dev, "aml pcie phy pll lock failed\n");
		else
			dev_dbg(dev, "aml pcie phy pll locked\n");
	}

	dev_dbg(dev, "aml pcie phy pll cnt=%d, reg[0]=0x%x, reg[1]=0x%x\n", cnt, reg[0], reg[1]);
	return ret;
}

static int amlogic_pcie_phy_c5_init(struct amlogic_pcie_phy *pcie_phy)
{
	struct device *dev = pcie_phy->dev;
	struct device_node *node = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *efuse_res;
	void __iomem *efuse_base = NULL;
	void __iomem *comb_phy_base = NULL;
	u32 val;
	int ret = 0, i;

	ret = clk_prepare_enable(pcie_phy->pcie_phy_clk);
	if (ret) {
		dev_err(dev, "unable to enable pcie_phy clock\n");
		return ret;
	}

	ret = amlogic_pcie_phy_assert_reset(pcie_phy);
	if (ret)
		return ret;

	ret = amlogic_pcie_phy_deassert_reset(pcie_phy);
	if (ret)
		return ret;

	/* phy init */
	/* close fast_sim */
	ret = regmap_update_bits(pcie_phy->regmap, 0x1b8, BIT(21), 0);
	if (ret)
		return ret;

	/* set eq */
	ret = regmap_update_bits(pcie_phy->regmap, 0x50, GENMASK(9, 5), 0);
	if (ret)
		return ret;

	/* select PCIe [0]=pcie2 [1]=usb3 default usb3 */
	comb_phy_base = ioremap(0xfe0086e0, 0x4);
	val = readl(comb_phy_base);
	val &= ~BIT(0);
	writel(val, comb_phy_base);
	dev_dbg(dev, "PCIe comb phy sel =0x%x\n", readl(comb_phy_base));

	/* close gen1 dfe */
	ret = regmap_write(pcie_phy->regmap, 0x90, 0x54154268);
	if (ret)
		return ret;

	/* close gen2 dfe */
	ret = regmap_write(pcie_phy->regmap, 0x94, 0x54144268);
	if (ret)
		return ret;

	efuse_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "efuse");
	if (efuse_res) {
		efuse_base = devm_ioremap(dev, efuse_res->start, resource_size(efuse_res));
		if (IS_ERR(efuse_base)) {
			dev_err(dev, "failed to request efuse_base\n");
			return PTR_ERR(efuse_base);
		}
	} else {
		dev_err(dev, "failed to get reset_base resource\n");
		return -ENODEV;
	}

	if (efuse_base) {
		val = readl(efuse_base);
		dev_dbg(dev, " efuse val = 0x%x\n", val);
		dev_dbg(dev, "boost = 0x%x\n", (u32)((val & GENMASK(11, 7)) >> 7));
		dev_dbg(dev, "pdio = 0x%x\n", (u32)(val & GENMASK(2, 0)));
		dev_dbg(dev, "puio = 0x%x\n", (u32)((val & GENMASK(5, 3)) >> 3));
		dev_dbg(dev, "dcha = 0x%x\n", (u32)((val & GENMASK(16, 13)) >> 13));

		/* boost */
		if (val & BIT(12)) {
			ret = regmap_update_bits(pcie_phy->regmap,
						 0x3c, GENMASK(18, 16),
						 ((val & GENMASK(9, 7)) >> 7));
			if (ret)
				return ret;
		}
	}

	/* set sqh */
	ret = regmap_write(pcie_phy->regmap, 0x64, 0x7200);
	if (ret)
		return ret;

	ret = regmap_update_bits(pcie_phy->regmap, 0x5c, BIT(20), 1);
	if (ret)
		return ret;

	ret = regmap_update_bits(pcie_phy->regmap, 0x54, BIT(5), 1);
	if (ret)
		return ret;

	ret = regmap_update_bits(pcie_phy->regmap, 0x5c, BIT(20), 0);
	if (ret)
		return ret;

	ret = of_property_read_u32(node, "pll_setting_number",
				   &pcie_phy->pll_setting_number);
	if (ret)
		return ret;

	for (i = 1; i <= pcie_phy->pll_setting_number; i++)
		ret = amlogic_pcie_phy_do_pll_set(pcie_phy, i);

	return ret;
}

static int amlogic_pcie_phy_c5_power_off(struct amlogic_pcie_phy *pcie_phy)
{
	int ret;

	ret = amlogic_pcie_phy_assert_reset(pcie_phy);
	if (ret)
		return ret;

	if (__clk_is_enabled(pcie_phy->pcie_phy_clk))
		clk_disable_unprepare(pcie_phy->pcie_phy_clk);
	return 0;
}

static int amlogic_pcie_phy_c5_exit(struct amlogic_pcie_phy *pcie_phy)
{
	int ret = 0;

	ret = amlogic_pcie_phy_assert_reset(pcie_phy);
	if (ret)
		return ret;

	if (__clk_is_enabled(pcie_phy->pcie_phy_clk))
		clk_disable_unprepare(pcie_phy->pcie_phy_clk);
	return ret;
}

static int amlogic_pcie_phy_c5_reset(struct amlogic_pcie_phy *pcie_phy)
{
	return 0;
}

static int amlogic_pcie_phy_get_reset(struct amlogic_pcie_phy *pcie_phy)
{
	struct device *dev = pcie_phy->dev;
	struct device_node *node = dev->of_node;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;
	int ret = 0, i;

	pcie_phy->reset_number = ARRAY_SIZE(reset_info);
	pcie_phy->resets = devm_kcalloc(dev, pcie_phy->reset_number, sizeof(*pcie_phy->resets),
				       GFP_KERNEL);
	if (IS_ERR_OR_NULL(pcie_phy->resets))
		return -ENOMEM;

	for (i = 0; i < pcie_phy->reset_number; i++)
		pcie_phy->resets[i].id = reset_info[i].name;

	ret = devm_reset_control_bulk_get_exclusive(dev, pcie_phy->reset_number, pcie_phy->resets);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			goto get_rst_reg;
	}

	return ret;

get_rst_reg:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "reset");
	if (res) {
		pcie_phy->reset_base = devm_ioremap(dev, res->start, resource_size(res));
		if (IS_ERR(pcie_phy->reset_base)) {
			dev_err(dev, "failed to request reset_base\n");
			return PTR_ERR(pcie_phy->reset_base);
		}
	} else {
		dev_err(dev, "failed to get reset_base resource\n");
		return -ENODEV;
	}

	ret = of_property_read_u32(node, "pcie_rst_number",
				   &pcie_phy->reset_number);
	if (ret) {
		dev_err(dev, "failed to request pcie_reset_number\n");
		return ret;
	}

	return 0;
}

static int amlogic_pcie_phy_c5_parse_dt(struct amlogic_pcie_phy *pcie_phy)
{
	struct device *dev = pcie_phy->dev;
	struct device_node *node = dev->of_node;
	int ret = 0;

	pcie_phy->pcie_phy_clk = devm_clk_get(dev, "pcie_phy");
	if (IS_ERR(pcie_phy->pcie_phy_clk))
		return dev_err_probe(dev, PTR_ERR(pcie_phy->pcie_phy_clk),
				     "pcie_phy_clk clock source missing or invalid\n");

	ret = amlogic_pcie_phy_get_reset(pcie_phy);
	if (ret)
		return ret;

	if (of_property_read_u32(node, "num-lanes", &pcie_phy->num_lanes)) {
		dev_err(dev, "failed to get num-lanes\n");
		return ret;
	}

	ret = of_property_read_u32(node, "port-num", &pcie_phy->port_num);
	if (ret) {
		dev_err(dev, "failed to get port-num\n");
		return ret;
	}

	return ret;
}

static const struct amlogic_pcie_phy_data pcie_phy_c5_data = {
	.phy_init	= amlogic_pcie_phy_c5_init,
	.phy_exit	= amlogic_pcie_phy_c5_exit,
	.phy_power_on	= amlogic_pcie_phy_c5_power_on,
	.phy_power_off	= amlogic_pcie_phy_c5_power_off,
	.phy_reset	= amlogic_pcie_phy_c5_reset,
	.parse_dt	= amlogic_pcie_phy_c5_parse_dt,
};

static const struct of_device_id pcie_phy_c5_of_match[] = {
	{
		.compatible = "amlogic,c5-pcie-phy",
		.data = &pcie_phy_c5_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, pcie_phy_c5_of_match);

static struct platform_driver pcie_phy_c5_driver = {
	.probe = amlogic_pcie_phy_probe,
	.driver = {
		.name = "pcie-phy-c5",
		.of_match_table = pcie_phy_c5_of_match,
	},
};

int __init aml_pcie_phy_c5_init(void)
{
	return platform_driver_register(&pcie_phy_c5_driver);
}

void __exit aml_pcie_phy_c5_exit(void)
{
	platform_driver_unregister(&pcie_phy_c5_driver);
}

/* module_platform_driver(pcie_phy_c5_driver); */

MODULE_AUTHOR("Amlogic Inc");
MODULE_DESCRIPTION("Amlogic PCIE PHY driver");
MODULE_LICENSE("GPL v2");
