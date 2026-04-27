/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_PCIE_PHY_H_
#define _AMLOGIC_PCIE_PHY_H_
struct amlogic_pcie_phy;

struct amlogic_pcie_phy_data {
	int (*parse_dt)(struct amlogic_pcie_phy *pcie_phy);
	int (*phy_reset)(struct amlogic_pcie_phy *pcie_phy);
	int (*phy_exit)(struct amlogic_pcie_phy *pcie_phy);
	int (*phy_init)(struct amlogic_pcie_phy *pcie_phy);
	int (*phy_power_on)(struct amlogic_pcie_phy *pcie_phy);
	int (*phy_power_off)(struct amlogic_pcie_phy *pcie_phy);
};

struct amlogic_pcie_phy {
	struct device *dev;
	struct phy *phy;
	struct amlogic_pcie_phy_data *data;
	struct regmap *regmap;
	struct reset_control *reset;
	struct fwnode_handle *fwnode;
};

/* Common probe function */
int amlogic_pcie_phy_probe(struct platform_device *pdev);

#endif /* _AMLOGIC_PCIE_PHY_H_ */
