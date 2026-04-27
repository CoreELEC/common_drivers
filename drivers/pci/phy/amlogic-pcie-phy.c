// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/property.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/string_helpers.h>
#include <linux/ctype.h>
#include <linux/gpio/consumer.h>
#include <linux/phy/phy.h>
#include <linux/reset.h>
#include <linux/kprobes.h>
#include <linux/highmem.h>
#include "amlogic-pcie-phy.h"

static struct regmap_config amlogic_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};

static struct regmap *amlogic_map_resource(struct amlogic_pcie_phy *pcie_phy,
					   struct device_node *node, char *name)
{
	struct resource res;
	void __iomem *base;
	int i;

	i = of_property_match_string(node, "reg-names", name);
	if (of_address_to_resource(node, i, &res))
		return NULL;

	base = devm_ioremap_resource(pcie_phy->dev, &res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	amlogic_regmap_config.max_register = resource_size(&res) - 4;
	amlogic_regmap_config.name = devm_kasprintf(pcie_phy->dev, GFP_KERNEL, "%pOFn-%s", node,
						    name);
	if (!amlogic_regmap_config.name)
		return ERR_PTR(-ENOMEM);

	return devm_regmap_init_mmio(pcie_phy->dev, base, &amlogic_regmap_config);
}

static int amlogic_pcie_phy_parse_dt(struct amlogic_pcie_phy *pcie_phy, struct device_node *node)
{
	//if (of_property_read_bool(node, "amlogic,ao-interrupt-controller")) {
	//	pcie_phy->of_irq = of_find_compatible_node(NULL,
	//					     NULL,
	//					     "amlogic,amlogic-gpio-ao-intc");
	//} else {
	//	pcie_phy->of_irq = of_find_compatible_node(NULL,
	//					     NULL,
	//					     "amlogic,amlogic-gpio-intc-ext");
	//}

	pcie_phy->regmap = amlogic_map_resource(pcie_phy, node, "phy");
	if (IS_ERR_OR_NULL(pcie_phy->regmap)) {
		dev_err(pcie_phy->dev, "failed map phy reg base\n");
		return pcie_phy->regmap ? PTR_ERR(pcie_phy->regmap) : -ENOENT;
	}

	if (pcie_phy->data->parse_dt)
		return pcie_phy->data->parse_dt(pcie_phy);

	return 0;
}

static int amlogic_pcie_phy_reset(struct phy *phy)
{
	struct amlogic_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int ret = 0;

	ret = reset_control_assert(pcie_phy->reset);
	if (ret != 0)
		goto out;
	usleep_range(20, 30);

	ret = reset_control_deassert(pcie_phy->reset);
	if (ret != 0)
		goto out;
	usleep_range(20, 30);

	//TODO
	if (pcie_phy->data->phy_reset)
		return pcie_phy->data->phy_reset(pcie_phy);

out:
	return ret;
}

static int amlogic_pcie_phy_exit(struct phy *phy)
{
	struct amlogic_pcie_phy *pcie_phy = phy_get_drvdata(phy);

	//TODO
	if (pcie_phy->data->phy_exit)
		return pcie_phy->data->phy_exit(pcie_phy);

	//regmap_write(pcie_phy->regmap, MESON_PCIE_REG0, MESON_PCIE_PHY_INIT);
	return reset_control_assert(pcie_phy->reset);
}

static int amlogic_pcie_phy_init(struct phy *phy)
{
	struct amlogic_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int ret = 0;

	//TODO
	if (pcie_phy->data->phy_init)
		return pcie_phy->data->phy_init(pcie_phy);

	//regmap_write(pcie_phy->regmap, MESON_PCIE_REG0, MESON_PCIE_PHY_INIT);
	return ret;
}

static int amlogic_pcie_phy_power_on(struct phy *phy)
{
	struct amlogic_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int ret = 0;

	//TODO
	if (pcie_phy->data->phy_power_on)
		return pcie_phy->data->phy_power_on(pcie_phy);

	//regmap_write(pcie_phy->regmap, MESON_PCIE_REG0, MESON_PCIE_PHY_INIT);
	//regmap_update_bits(pcie_phy->regmap, MESON_PCIE_REG0, MESON_PCIE_POWERDOWN, 0);
	return ret;
}

static int amlogic_pcie_phy_power_off(struct phy *phy)
{
	struct amlogic_pcie_phy *pcie_phy = phy_get_drvdata(phy);
	int ret = 0;

	//TODO
	if (pcie_phy->data->phy_power_off)
		return pcie_phy->data->phy_power_off(pcie_phy);

	//regmap_write(pcie_phy->regmap, MESON_PCIE_REG0, MESON_PCIE_PHY_INIT);
	//regmap_update_bits(pcie_phy->regmap, MESON_PCIE_REG0, MESON_PCIE_POWERDOWN, 0);
	return ret;
}

static const struct phy_ops pcie_phy_ops = {
	.init		= amlogic_pcie_phy_init,
	.exit		= amlogic_pcie_phy_exit,
	.power_on	= amlogic_pcie_phy_power_on,
	.power_off	= amlogic_pcie_phy_power_off,
	.reset		= amlogic_pcie_phy_reset,
	.owner		= THIS_MODULE,
};

int amlogic_pcie_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct amlogic_pcie_phy *pcie_phy;
	struct phy_provider *pphy;
	int ret;

	pcie_phy = devm_kzalloc(dev, sizeof(struct amlogic_pcie_phy), GFP_KERNEL);
	if (!pcie_phy)
		return -ENOMEM;

	pcie_phy->dev = dev;

	pcie_phy->data = (struct amlogic_pcie_phy_data *)of_device_get_match_data(dev);
	if (!pcie_phy->data)
		return -EINVAL;

	ret = amlogic_pcie_phy_parse_dt(pcie_phy, np);
	if (ret)
		return ret;

	pcie_phy->phy = devm_phy_create(dev, np, &pcie_phy_ops);
	if (IS_ERR(pcie_phy->phy)) {
		ret = PTR_ERR(pcie_phy->phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to create PCIE PHY\n");
		return ret;
	}

	phy_set_drvdata(pcie_phy->phy, pcie_phy);
	dev_set_drvdata(dev, pcie_phy);
	pphy = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(pphy);
}
EXPORT_SYMBOL(amlogic_pcie_phy_probe);
