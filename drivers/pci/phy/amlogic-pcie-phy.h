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
	struct fwnode_handle *fwnode;

	void __iomem *reset_base;
	void __iomem *efuse_base;

	struct reset_control *mac_apb_rst;
	struct reset_control *por_rst;
	struct reset_control *phy_rst;
	struct reset_control *pipe_rst;
	struct reset_control *phy_apb_rst;
	struct reset_control *pcie_rst0;
	struct reset_control *pcie_rst1;
	struct reset_control *pcie_rst2;
	struct reset_control *pcie_rst3;
	struct reset_control *pcie_rst4;
	struct reset_control *pcie_rst5;
	struct reset_control *pcie_rst6;
	struct reset_control *pcie_rst7;
	u32 reset_number;

	struct clk *pcie_phy_clk;
	struct clk *pcie_apb_bus_clk;
	u32 pll_setting_number;
	u32 num_lanes;
	u32 port_num;
};

/* Common probe function */
int amlogic_pcie_phy_probe(struct platform_device *pdev);

#endif /* _AMLOGIC_PCIE_PHY_H_ */
