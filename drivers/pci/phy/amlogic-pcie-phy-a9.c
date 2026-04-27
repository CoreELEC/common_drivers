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
#include "amlogic-pcie-phy.h"

static int amlogic_pcie_phy_a9_power_on(struct amlogic_pcie_phy *pcie_phy)
{
	/* int ret; */

	/* regmap_update_bits(priv->regmap, MESON_PCIE_REG0, MESON_PCIE_POWERDOWN, 0); */
	return 0;
}

static int amlogic_pcie_phy_a9_power_off(struct amlogic_pcie_phy *pcie_phy)
{
	/* int ret; */

	return 0;
}

static int amlogic_pcie_phy_a9_init(struct amlogic_pcie_phy *pcie_phy)
{
	/* int ret; */

	/* regmap_write(priv->regmap, MESON_PCIE_REG0, MESON_PCIE_PHY_INIT); */
	return reset_control_reset(pcie_phy->reset);
}

static int amlogic_pcie_phy_a9_exit(struct amlogic_pcie_phy *pcie_phy)
{
	/* int ret; */

	return reset_control_reset(pcie_phy->reset);
}

static int amlogic_pcie_phy_a9_reset(struct amlogic_pcie_phy *pcie_phy)
{
	int ret = 0;

	ret = reset_control_assert(pcie_phy->reset);
	if (ret != 0)
		goto out;

	ret = reset_control_deassert(pcie_phy->reset);
	if (ret != 0)
		goto out;

out:
	return ret;
}

static const struct amlogic_pcie_phy_data pcie_phy_a9_data = {
	.phy_init	= amlogic_pcie_phy_a9_init,
	.phy_exit	= amlogic_pcie_phy_a9_exit,
	.phy_power_on	= amlogic_pcie_phy_a9_power_on,
	.phy_power_off	= amlogic_pcie_phy_a9_power_off,
	.phy_reset	= amlogic_pcie_phy_a9_reset,
	.parse_dt	= amlogic_pcie_phy_a9_reset,
};

static const struct of_device_id pcie_phy_a9_of_match[] = {
	{
		.compatible = "amlogic,a9-pcie-phy",
		.data = &pcie_phy_a9_data,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, pcie_phy_a9_of_match);

static struct platform_driver pcie_phy_a9_driver = {
	.probe = amlogic_pcie_phy_probe,
	.driver = {
		.name = "pcie-phy-a9",
		.of_match_table = pcie_phy_a9_of_match,
	},
};

int __init aml_pcie_phy_a9_init(void)
{
	return platform_driver_register(&pcie_phy_a9_driver);
}

void __exit aml_pcie_phy_a9_exit(void)
{
	platform_driver_unregister(&pcie_phy_a9_driver);
}

/* module_platform_driver(pcie_phy_a9_driver); */

MODULE_AUTHOR("Amlogic Inc");
MODULE_DESCRIPTION("Amlogic PCIE PHY driver");
MODULE_LICENSE("GPL v2");
