// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/of_pci.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/reset.h>
#include <linux/of_gpio.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/ktime.h>

#include "amlogic-pcie-v3.h"

static u8 __aml_pcie_find_next_cap(struct amlogic_pcie *pci, u8 cap_ptr,
				  u8 cap)
{
	u8 cap_id, next_cap_ptr;
	u16 reg;

	if (!cap_ptr)
		return 0;

	reg = amlogic_pcieinter_readw(pci, cap_ptr);
	cap_id = (reg & 0x00ff);

	if (cap_id > PCI_CAP_ID_MAX)
		return 0;

	if (cap_id == cap)
		return cap_ptr;

	next_cap_ptr = (reg & 0xff00) >> 8;
	return __aml_pcie_find_next_cap(pci, next_cap_ptr, cap);
}

u8 aml_pcie_find_capability(struct amlogic_pcie *pci, u8 cap)
{
	u8 next_cap_ptr;
	u16 reg;

	reg = amlogic_pcieinter_readw(pci, PCI_CAPABILITY_LIST);
	next_cap_ptr = (reg & 0x00ff);

	return __aml_pcie_find_next_cap(pci, next_cap_ptr, cap);
}
EXPORT_SYMBOL_GPL(aml_pcie_find_capability);

static int amlogic_pcie_get_reset_for_m31_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *node = dev->of_node;
	struct resource *rst_regs;
	int ret = 0;

	/*m31phy_pcie_wrapper_rst*/
	amlogic->m31phy_rst = devm_reset_control_get_exclusive(dev,
							       "m31phy_rst");
	if (IS_ERR(amlogic->m31phy_rst)) {
		if (PTR_ERR(amlogic->m31phy_rst) != -EPROBE_DEFER)
			goto get_rst_reg;
	}

	/*pcie_gen3_l0_rst*/
	amlogic->gen3_l0_rst = devm_reset_control_get_exclusive(dev,
								"gen3_l0_rst");
	if (IS_ERR(amlogic->gen3_l0_rst)) {
		if (PTR_ERR(amlogic->gen3_l0_rst) != -EPROBE_DEFER)
			dev_err(dev, "gen3_l0_rst reset property in node\n");
		return PTR_ERR(amlogic->gen3_l0_rst);
	}

	amlogic->pcie_apb_rst = devm_reset_control_get_exclusive(dev,
								 "apb_rst");
	if (IS_ERR(amlogic->pcie_apb_rst)) {
		if (PTR_ERR(amlogic->pcie_apb_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_apb_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_apb_rst);
	}

	amlogic->pcie_phy_rst = devm_reset_control_get_exclusive(dev,
								 "phy_rst");
	if (IS_ERR(amlogic->pcie_phy_rst)) {
		if (PTR_ERR(amlogic->pcie_phy_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_phy_rst property in node\n");
		return PTR_ERR(amlogic->pcie_phy_rst);
	}

	amlogic->pcie_a_rst = devm_reset_control_get_exclusive(dev,
							       "pcie_a_rst");
	if (IS_ERR(amlogic->pcie_a_rst)) {
		if (PTR_ERR(amlogic->pcie_a_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_a_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_a_rst);
	}

	amlogic->pcie_rst0 = devm_reset_control_get_exclusive(dev, "pcie_rst0");
	if (IS_ERR(amlogic->pcie_rst0)) {
		if (PTR_ERR(amlogic->pcie_rst0) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst0 property in node\n");
		return PTR_ERR(amlogic->pcie_rst0);
	}

	amlogic->pcie_rst1 = devm_reset_control_get_exclusive(dev, "pcie_rst1");
	if (IS_ERR(amlogic->pcie_rst1)) {
		if (PTR_ERR(amlogic->pcie_rst1) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst1 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst1);
	}

	amlogic->pcie_rst2 = devm_reset_control_get_exclusive(dev, "pcie_rst2");
	if (IS_ERR(amlogic->pcie_rst2)) {
		if (PTR_ERR(amlogic->pcie_rst2) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst2 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst2);
	}

	amlogic->pcie_rst3 = devm_reset_control_get_exclusive(dev, "pcie_rst3");
	if (IS_ERR(amlogic->pcie_rst3)) {
		if (PTR_ERR(amlogic->pcie_rst3) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst3 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst3);
	}

	amlogic->pcie_rst4 = devm_reset_control_get_exclusive(dev, "pcie_rst4");
	if (IS_ERR(amlogic->pcie_rst4)) {
		if (PTR_ERR(amlogic->pcie_rst4) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst4 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst4);
	}

	amlogic->pcie_rst5 = devm_reset_control_get_exclusive(dev, "pcie_rst5");
	if (IS_ERR(amlogic->pcie_rst5)) {
		if (PTR_ERR(amlogic->pcie_rst5) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst5 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst5);
	}

	amlogic->pcie_rst6 = devm_reset_control_get_exclusive(dev, "pcie_rst6");
	if (IS_ERR(amlogic->pcie_rst6)) {
		if (PTR_ERR(amlogic->pcie_rst6) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst6 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst6);
	}

	amlogic->pcie_rst7 = devm_reset_control_get_exclusive(dev, "pcie_rst7");
	if (IS_ERR(amlogic->pcie_rst7)) {
		if (PTR_ERR(amlogic->pcie_rst7) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst7 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst7);
	}

	return 0;

get_rst_reg:
	rst_regs = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"reset-base");
	amlogic->rst_base = devm_ioremap(dev, rst_regs->start,
					 resource_size(rst_regs));
	if (IS_ERR(amlogic->rst_base)) {
		dev_err(dev, "failed to request rst_base\n");
		return PTR_ERR(amlogic->rst_base);
	}

	ret = of_property_read_u32(node, "pcie-m31phy-rst-bit",
				   &amlogic->m31phy_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request m31phy_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-gen3-l0-rst-bit",
				   &amlogic->gen3_l0_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request gen3_l0_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-apb-rst-bit",
				   &amlogic->apb_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request apb_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-phy-rst-bit",
				   &amlogic->phy_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request phy_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-a-rst-bit",
				   &amlogic->pcie_a_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_a_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-bit",
				   &amlogic->pcie_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-mask",
				   &amlogic->pcie_rst_mask);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_size\n");
		return ret;
	}

	return 0;
}

static int amlogic_pcie_get_reset_for_m31_comphy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *node = dev->of_node;
	struct resource *rst_regs;
	int ret = 0;

	amlogic->gen2_l0_rst = devm_reset_control_get_exclusive(dev,
								"gen2_l0_rst");
	if (IS_ERR(amlogic->gen2_l0_rst)) {
		if (PTR_ERR(amlogic->gen2_l0_rst) != -EPROBE_DEFER)
			goto get_rst_reg;
	}

	amlogic->pcie_apb_rst = devm_reset_control_get_exclusive(dev,
								 "apb_rst");
	if (IS_ERR(amlogic->pcie_apb_rst)) {
		if (PTR_ERR(amlogic->pcie_apb_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_apb_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_apb_rst);
	}

	amlogic->pcie_phy_rst = devm_reset_control_get_exclusive(dev,
								 "phy_rst");
	if (IS_ERR(amlogic->pcie_phy_rst)) {
		if (PTR_ERR(amlogic->pcie_phy_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_phy_rst property in node\n");
		return PTR_ERR(amlogic->pcie_phy_rst);
	}

	amlogic->pcie_a_rst = devm_reset_control_get_exclusive(dev,
							       "pcie_a_rst");
	if (IS_ERR(amlogic->pcie_a_rst)) {
		if (PTR_ERR(amlogic->pcie_a_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_a_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_a_rst);
	}

	amlogic->pcie_rst0 = devm_reset_control_get_exclusive(dev, "pcie_rst0");
	if (IS_ERR(amlogic->pcie_rst0)) {
		if (PTR_ERR(amlogic->pcie_rst0) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst0 property in node\n");
		return PTR_ERR(amlogic->pcie_rst0);
	}

	amlogic->pcie_rst1 = devm_reset_control_get_exclusive(dev, "pcie_rst1");
	if (IS_ERR(amlogic->pcie_rst1)) {
		if (PTR_ERR(amlogic->pcie_rst1) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst1 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst1);
	}

	amlogic->pcie_rst2 = devm_reset_control_get_exclusive(dev, "pcie_rst2");
	if (IS_ERR(amlogic->pcie_rst2)) {
		if (PTR_ERR(amlogic->pcie_rst2) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst2 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst2);
	}

	amlogic->pcie_rst3 = devm_reset_control_get_exclusive(dev, "pcie_rst3");
	if (IS_ERR(amlogic->pcie_rst3)) {
		if (PTR_ERR(amlogic->pcie_rst3) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst3 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst3);
	}

	amlogic->pcie_rst4 = devm_reset_control_get_exclusive(dev, "pcie_rst4");
	if (IS_ERR(amlogic->pcie_rst4)) {
		if (PTR_ERR(amlogic->pcie_rst4) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst4 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst4);
	}

	amlogic->pcie_rst5 = devm_reset_control_get_exclusive(dev, "pcie_rst5");
	if (IS_ERR(amlogic->pcie_rst5)) {
		if (PTR_ERR(amlogic->pcie_rst5) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst5 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst5);
	}

	amlogic->pcie_rst6 = devm_reset_control_get_exclusive(dev, "pcie_rst6");
	if (IS_ERR(amlogic->pcie_rst6)) {
		if (PTR_ERR(amlogic->pcie_rst6) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst6 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst6);
	}

	amlogic->pcie_rst7 = devm_reset_control_get_exclusive(dev, "pcie_rst7");
	if (IS_ERR(amlogic->pcie_rst7)) {
		if (PTR_ERR(amlogic->pcie_rst7) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst7 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst7);
	}

	return 0;

get_rst_reg:
	rst_regs = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"reset-base");
	amlogic->rst_base = devm_ioremap(dev, rst_regs->start,
					 resource_size(rst_regs));
	if (IS_ERR(amlogic->rst_base)) {
		dev_err(dev, "failed to request rst_base\n");
		return PTR_ERR(amlogic->rst_base);
	}

	ret = of_property_read_u32(node, "pcie-gen2-l0-rst-bit",
				   &amlogic->gen2_l0_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request gen2_l0_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-apb-rst-bit",
				   &amlogic->apb_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request apb_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-phy-rst-bit",
				   &amlogic->phy_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request phy_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-a-rst-bit",
				   &amlogic->pcie_a_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_a_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-bit",
				   &amlogic->pcie_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-mask",
				   &amlogic->pcie_rst_mask);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_size\n");
		return ret;
	}

	return 0;
}

static int amlogic_pcie_get_reset_for_aml_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *node = dev->of_node;
	struct resource *rst_regs;
	int ret = 0;

	amlogic->amlphy_rst = devm_reset_control_get_exclusive(dev,
							       "amlphy_rst");
	if (IS_ERR(amlogic->amlphy_rst)) {
		if (PTR_ERR(amlogic->amlphy_rst) != -EPROBE_DEFER)
			goto get_rst_reg;
	}

	amlogic->pcie_apb_rst = devm_reset_control_get_exclusive(dev,
								 "apb_rst");
	if (IS_ERR(amlogic->pcie_apb_rst)) {
		if (PTR_ERR(amlogic->pcie_apb_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_apb_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_apb_rst);
	}

	amlogic->u3p2_phy_apb_rst = devm_reset_control_get_exclusive(dev,
								     "u3p2_phy_apb_rst");
	if (IS_ERR(amlogic->u3p2_phy_apb_rst)) {
		if (PTR_ERR(amlogic->u3p2_phy_apb_rst) != -EPROBE_DEFER)
			dev_err(dev, "u3p2_phy_apb_rst reset property in node\n");
		return PTR_ERR(amlogic->u3p2_phy_apb_rst);
	}

	amlogic->pcie_pipe_rst = devm_reset_control_get_exclusive(dev,
								  "pcie_pipe_rst");
	if (IS_ERR(amlogic->pcie_pipe_rst)) {
		if (PTR_ERR(amlogic->pcie_pipe_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_pipe_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_pipe_rst);
	}

	amlogic->pcie_a_rst = devm_reset_control_get_exclusive(dev,
							       "pcie_a_rst");
	if (IS_ERR(amlogic->pcie_a_rst)) {
		if (PTR_ERR(amlogic->pcie_a_rst) != -EPROBE_DEFER)
			dev_err(dev, "pcie_a_rst reset property in node\n");
		return PTR_ERR(amlogic->pcie_a_rst);
	}

	amlogic->pcie_rst0 = devm_reset_control_get_exclusive(dev, "pcie_rst0");
	if (IS_ERR(amlogic->pcie_rst0)) {
		if (PTR_ERR(amlogic->pcie_rst0) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst0 property in node\n");
		return PTR_ERR(amlogic->pcie_rst0);
	}

	amlogic->pcie_rst1 = devm_reset_control_get_exclusive(dev, "pcie_rst1");
	if (IS_ERR(amlogic->pcie_rst1)) {
		if (PTR_ERR(amlogic->pcie_rst1) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst1 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst1);
	}

	amlogic->pcie_rst2 = devm_reset_control_get_exclusive(dev, "pcie_rst2");
	if (IS_ERR(amlogic->pcie_rst2)) {
		if (PTR_ERR(amlogic->pcie_rst2) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst2 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst2);
	}

	amlogic->pcie_rst3 = devm_reset_control_get_exclusive(dev, "pcie_rst3");
	if (IS_ERR(amlogic->pcie_rst3)) {
		if (PTR_ERR(amlogic->pcie_rst3) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst3 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst3);
	}

	amlogic->pcie_rst4 = devm_reset_control_get_exclusive(dev, "pcie_rst4");
	if (IS_ERR(amlogic->pcie_rst4)) {
		if (PTR_ERR(amlogic->pcie_rst4) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst4 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst4);
	}

	amlogic->pcie_rst5 = devm_reset_control_get_exclusive(dev, "pcie_rst5");
	if (IS_ERR(amlogic->pcie_rst5)) {
		if (PTR_ERR(amlogic->pcie_rst5) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst5 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst5);
	}

	amlogic->pcie_rst6 = devm_reset_control_get_exclusive(dev, "pcie_rst6");
	if (IS_ERR(amlogic->pcie_rst6)) {
		if (PTR_ERR(amlogic->pcie_rst6) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst6 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst6);
	}

	amlogic->pcie_rst7 = devm_reset_control_get_exclusive(dev, "pcie_rst7");
	if (IS_ERR(amlogic->pcie_rst7)) {
		if (PTR_ERR(amlogic->pcie_rst7) != -EPROBE_DEFER)
			dev_err(dev, "pcie_rst7 reset property in node\n");
		return PTR_ERR(amlogic->pcie_rst7);
	}

	return 0;

get_rst_reg:
	rst_regs = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"reset-base");
	amlogic->rst_base = devm_ioremap(dev, rst_regs->start,
					 resource_size(rst_regs));
	if (IS_ERR(amlogic->rst_base)) {
		dev_err(dev, "failed to request rst_base\n");
		return PTR_ERR(amlogic->rst_base);
	}

	ret = of_property_read_u32(node, "pcie-amlphy-rst-bit",
				   &amlogic->amlphy_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request amlphy_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-apb-rst-bit",
				   &amlogic->apb_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request apb_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-pipe-rst-bit",
				   &amlogic->pcie_pipe_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_pipe_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "u3p2-phy-apb",
				   &amlogic->u3p2_phy_apb_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request u3p2_phy_apb_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-a-rst-bit",
				   &amlogic->pcie_a_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_a_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-bit",
				   &amlogic->pcie_rst_bit);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_bit\n");
		return ret;
	}

	ret = of_property_read_u32(node, "pcie-rst-mask",
				   &amlogic->pcie_rst_mask);
	if (ret) {
		dev_err(dev, "failed to request pcie_rst_size\n");
		return ret;
	}

	return 0;
}

int amlogic_pcie_get_resets(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int ret = 0;

	switch (amlogic->phy_type) {
	case AMLOGIC_PHY:
		dev_dbg(dev, " pcie use AMLOGIC_PHY\n");
		ret = amlogic_pcie_get_reset_for_aml_phy(amlogic);
		break;
	case M31_COMBPHY:
		dev_dbg(dev, " pcie use M31_COMBPHY\n");
		ret = amlogic_pcie_get_reset_for_m31_comphy(amlogic);
		break;
	case M31_PHY:
	default:
		dev_dbg(dev, " pcie use M31_PHY\n");
		ret = amlogic_pcie_get_reset_for_m31_phy(amlogic);
		break;
	}

	return ret;
}

static int amlogic_pcie_set_reset_for_m31_phy(struct amlogic_pcie *amlogic, bool set)
{
	struct device *dev = amlogic->dev;
	int err = 0, val = 0;
	int regs = 0;

	if (amlogic->rst_base)
		goto set_rst_reg;

	err = reset_control_deassert(amlogic->m31phy_rst);
	if (err < 0) {
		dev_err(dev, "deassert m31phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->m31phy_rst);
	if (err < 0) {
		dev_err(dev, "assert m31phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->gen3_l0_rst);
	if (err < 0) {
		dev_err(dev, "deassert gen3_l0_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->gen3_l0_rst);
	if (err < 0) {
		dev_err(dev, "assert gen3_l0_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "deassert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_phy_rst);
	if (err) {
		dev_err(dev, "deassert pcie_phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_phy_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_a_rst);
	if (err) {
		dev_err(dev, "deassert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_a_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst0);
	if (err) {
		dev_err(dev, "deassert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst0);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst1);
	if (err) {
		dev_err(dev, "deassert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst1);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst2);
	if (err) {
		dev_err(dev, "deassert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst2);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst3);
	if (err) {
		dev_err(dev, "deassert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst3);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst4);
	if (err) {
		dev_err(dev, "deassert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst4);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst5);
	if (err) {
		dev_err(dev, "deassert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst5);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst6);
	if (err) {
		dev_err(dev, "deassert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst6);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst7);
	if (err) {
		dev_err(dev, "deassert pcie_rst7 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst7);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst7 err %d\n", err);
		return err;
	}

	return 0;

set_rst_reg:
	if (!set) {
		val = readl(amlogic->rst_base + RESETCTRL3_OFFSET);
		val &= ~(amlogic->pcie_rst_mask << amlogic->pcie_rst_bit);
		writel(val, amlogic->rst_base + RESETCTRL3_OFFSET);

		val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
		val &= ~((1 << amlogic->pcie_a_rst_bit) |
			 (1 << amlogic->phy_rst_bit) |
			 (1 << amlogic->apb_rst_bit));
		writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);
	} else {
		val = readl(amlogic->rst_base + RESETCTRL3_OFFSET);
		val |= (amlogic->pcie_rst_mask << amlogic->pcie_rst_bit);
		writel(val, amlogic->rst_base + RESETCTRL3_OFFSET);

		/*PHY_Register_XCFGA_COM value from vendor recommend*/
		regs = readl(amlogic->phy_base + 0x828);
		regs &= ~GENMASK(31, 28);
		regs |= (2 << 28);
		writel(regs, amlogic->phy_base + 0x828);

		/*PHY_Register_XCFGD value from vendor recommend*/
		regs = readl(amlogic->phy_base + 0x460);
		regs &= ~GENMASK(23, 20);
		regs |= (4 << 20);
		writel(regs, amlogic->phy_base + 0x460);

		val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
		val |= ((1 << amlogic->pcie_a_rst_bit) |
			 (1 << amlogic->phy_rst_bit) |
			 (1 << amlogic->apb_rst_bit));
		writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);
	}

	return 0;
}

static int amlogic_pcie_set_reset_for_m31_combphy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err = 0, val = 0;

	if (amlogic->rst_base)
		goto set_rst_reg;

	err = reset_control_deassert(amlogic->gen2_l0_rst);
	if (err < 0) {
		dev_err(dev, "deassert gen2_l0_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->gen2_l0_rst);
	if (err < 0) {
		dev_err(dev, "assert gen2_l0_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "deassert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_phy_rst);
	if (err) {
		dev_err(dev, "deassert pcie_phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_phy_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_phy_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_a_rst);
	if (err) {
		dev_err(dev, "deassert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_a_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst0);
	if (err) {
		dev_err(dev, "deassert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst0);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst1);
	if (err) {
		dev_err(dev, "deassert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst1);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst2);
	if (err) {
		dev_err(dev, "deassert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst2);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst3);
	if (err) {
		dev_err(dev, "deassert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst3);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst4);
	if (err) {
		dev_err(dev, "deassert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst4);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst5);
	if (err) {
		dev_err(dev, "deassert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst5);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst6);
	if (err) {
		dev_err(dev, "deassert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst6);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst7);
	if (err) {
		dev_err(dev, "deassert pcie_rst7 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst7);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst7 err %d\n", err);
		return err;
	}

	return 0;

set_rst_reg:
	val = readl(amlogic->rst_base + RESETCTRL0_OFFSET);
	val &= ~(1 << amlogic->phy_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL0_OFFSET);

	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val &= ~((1 << amlogic->pcie_a_rst_bit) |
		 (1 << amlogic->gen2_l0_rst_bit));
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

	val = readl(amlogic->rst_base + RESETCTRL3_OFFSET);
	val &= ~(amlogic->pcie_rst_mask << amlogic->pcie_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL3_OFFSET);

	usleep_range(10, 20);

	val = readl(amlogic->rst_base + RESETCTRL0_OFFSET);
	val |= 1 << amlogic->phy_rst_bit;
	writel(val, amlogic->rst_base + RESETCTRL0_OFFSET);

	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val |= ((1 << amlogic->pcie_a_rst_bit) |
		(1 << amlogic->gen2_l0_rst_bit));
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

	val = readl(amlogic->rst_base + RESETCTRL3_OFFSET);
	val |= (amlogic->pcie_rst_mask << amlogic->pcie_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL3_OFFSET);

	usleep_range(10, 30);

	return 0;
}

int amlogic_pcie_set_reset_for_aml_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err = 0, val = 0;
	static int flag;

	if (amlogic->rst_base)
		goto set_rst_reg;

	err = reset_control_deassert(amlogic->amlphy_rst);
	if (err < 0) {
		dev_err(dev, "deassert amlphy_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->amlphy_rst);
	if (err < 0) {
		dev_err(dev, "assert amlphy_rst err %d\n", err);
		return err;
	}
	err = reset_control_deassert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "deassert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_apb_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_apb_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_a_rst);
	if (err) {
		dev_err(dev, "deassert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_a_rst);
	if (err < 0) {
		dev_err(dev, "assert pcie_a_rst err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst0);
	if (err) {
		dev_err(dev, "deassert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst0);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst0 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst1);
	if (err) {
		dev_err(dev, "deassert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst1);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst1 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst2);
	if (err) {
		dev_err(dev, "deassert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst2);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst2 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst3);
	if (err) {
		dev_err(dev, "deassert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst3);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst3 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst4);
	if (err) {
		dev_err(dev, "deassert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst4);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst4 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst5);
	if (err) {
		dev_err(dev, "deassert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst5);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst5 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst6);
	if (err) {
		dev_err(dev, "deassert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst6);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst6 err %d\n", err);
		return err;
	}

	err = reset_control_deassert(amlogic->pcie_rst7);
	if (err) {
		dev_err(dev, "deassert pcie_rst7 err %d\n", err);
		return err;
	}

	err = reset_control_assert(amlogic->pcie_rst7);
	if (err < 0) {
		dev_err(dev, "assert pcie_rst7 err %d\n", err);
		return err;
	}

	return 0;

set_rst_reg:
	if (!flag) {
		val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
		val &= ~(BIT(amlogic->amlphy_rst_bit) |
			 BIT(amlogic->pcie_a_rst_bit) |
			 BIT(amlogic->apb_rst_bit) |
			 BIT(amlogic->u3p2_phy_apb_rst_bit) |
			 BIT(amlogic->pcie_pipe_bit) |
			 (amlogic->pcie_rst_mask << amlogic->pcie_rst_bit));
		writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

		usleep_range(20, 30);

		val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
		val |= (BIT(amlogic->amlphy_rst_bit) |
			 BIT(amlogic->pcie_a_rst_bit) |
			 BIT(amlogic->apb_rst_bit) |
			 BIT(amlogic->u3p2_phy_apb_rst_bit) |
			 BIT(amlogic->pcie_pipe_bit) |
			 (amlogic->pcie_rst_mask << amlogic->pcie_rst_bit));
		writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

		usleep_range(10, 20);
		flag++;
	} else {
		val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
		val &= ~BIT(amlogic->pcie_pipe_bit);
		writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

		usleep_range(10, 20);

		val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
		val |= BIT(amlogic->pcie_pipe_bit);
		writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

		usleep_range(10, 20);
		flag = 0;
	}

	return 0;
}

static int amlogic_pcie_get_reset_gpio(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct device_node *node = dev->of_node;
	int ret;

	ret = of_property_read_u32(node, "gpio-type",
				   &amlogic->gpio_type);
	if (ret) {
		dev_err(dev, "failed to request gpio-type\n");
		return ret;
	}

	amlogic->reset_gpio = of_get_named_gpio(node, "reset-gpio", 0);
	if (gpio_is_valid(amlogic->reset_gpio)) {
		if (!amlogic->is_rc)
			return 0;

		ret = devm_gpio_request_one(dev, amlogic->reset_gpio,
					    GPIOF_OUT_INIT_LOW,
					    "pcie_perst");
		if (ret) {
			dev_err(dev, "unable to get reset gpio\n");
			return ret;
		}
	} else {
		dev_err(dev, "failed to get reset-gpio\n");
		return -ENODEV;
	}
	return 0;
}

static int amlogic_pcie_set_reset_gpio(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int ret = 0;

	if (!amlogic->do_reset_gpio) {
		dev_info(dev, "GPIO no need reset for W2 when resume\n");
		usleep_range(9000, 10000);
		return 0;
	}

	/*reset-gpio-type 0:Shared pad(no reset)1:OD pad2:Normal pad*/
	if (amlogic->gpio_type == 0) {
		dev_info(dev, "gpio multiplex, don't reset!\n");
	} else if (amlogic->gpio_type == 1) {
		dev_info(dev, "pad gpio\n");
		if (gpio_is_valid(amlogic->reset_gpio)) {
			dev_info(dev, "GPIO pad: assert reset\n");
			ret = gpio_direction_output(amlogic->reset_gpio, 0);
			if (ret)
				return ret;
			usleep_range(90000, 110000);
			ret = gpio_direction_input(amlogic->reset_gpio);
			if (ret)
				return ret;
		}
	} else {
		dev_info(dev, "normal gpio\n");
		if (gpio_is_valid(amlogic->reset_gpio)) {
			dev_info(dev, "GPIO normal: assert reset\n");
			gpio_set_value_cansleep(amlogic->reset_gpio, 0);
			usleep_range(90000, 110000);
			gpio_set_value_cansleep(amlogic->reset_gpio, 1);
		}
	}

	return 0;
}

static int amlogic_pcie_get_clks_for_m31_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;

	amlogic->pcie_400m_clk = devm_clk_get(dev, "pcie_400m_clk");
	if (IS_ERR(amlogic->pcie_400m_clk)) {
		dev_err(dev, "pcie_400m_clk not found\n");
		return PTR_ERR(amlogic->pcie_400m_clk);
	}

	amlogic->pcie_tl_clk = devm_clk_get(dev, "pcie_tl_clk");
	if (IS_ERR(amlogic->pcie_tl_clk)) {
		dev_err(dev, "pcie_tl_clk not found\n");
		return PTR_ERR(amlogic->pcie_tl_clk);
	}

	amlogic->cts_pcie_clk = devm_clk_get(dev, "cts_pcie_clk");
	if (IS_ERR(amlogic->cts_pcie_clk)) {
		dev_err(dev, "cts_pcie_clk not found\n");
		return PTR_ERR(amlogic->cts_pcie_clk);
	}

	amlogic->pcie_clk = devm_clk_get(dev, "pcie");
	if (IS_ERR(amlogic->pcie_clk)) {
		dev_err(dev, "pcie_clk not found\n");
		return PTR_ERR(amlogic->pcie_clk);
	}

	amlogic->phy_clk = devm_clk_get(dev, "pcie_phy");
	if (IS_ERR(amlogic->phy_clk)) {
		dev_err(dev, "phy_clk not found\n");
		return PTR_ERR(amlogic->phy_clk);
	}

	amlogic->refpll_clk = devm_clk_get(dev, "pcie_refpll");
	if (IS_ERR(amlogic->refpll_clk)) {
		dev_err(dev, "refpll_clk not found\n");
		return PTR_ERR(amlogic->refpll_clk);
	}

	amlogic->dev_clk = devm_clk_get(dev, "pcie_hcsl");
	if (IS_ERR(amlogic->dev_clk)) {
		dev_err(dev, "dev_clk not found\n");
		return PTR_ERR(amlogic->dev_clk);
	}

	return 0;
}

static int amlogic_pcie_get_clks_for_aml_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;

	amlogic->pcie_tl_clk = devm_clk_get(dev, "pcie_tl_clk");
	if (IS_ERR(amlogic->pcie_tl_clk)) {
		dev_err(dev, "pcie_tl_clk not found\n");
		return PTR_ERR(amlogic->pcie_tl_clk);
	}

	amlogic->pcie_clk = devm_clk_get(dev, "pcie");
	if (IS_ERR(amlogic->pcie_clk)) {
		dev_err(dev, "pcie_clk not found\n");
		return PTR_ERR(amlogic->pcie_clk);
	}

	amlogic->phy_clk = devm_clk_get(dev, "pcie_phy");
	if (IS_ERR(amlogic->phy_clk)) {
		dev_err(dev, "phy_clk not found\n");
		return PTR_ERR(amlogic->phy_clk);
	}

	return 0;
}

static int amlogic_pcie_get_clks(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int ret = 0;

	switch (amlogic->phy_type) {
	case AMLOGIC_PHY:
		dev_dbg(dev, " pcie use AMLOGIC_PHY get clks\n");
		ret = amlogic_pcie_get_clks_for_aml_phy(amlogic);
		break;
	case M31_COMBPHY:
		dev_dbg(dev, " pcie use M31_COMBPHY get clks\n");
		ret = amlogic_pcie_get_clks_for_m31_phy(amlogic);
		break;
	case M31_PHY:
	default:
		dev_dbg(dev, " pcie use M31_PHY get clks\n");
		ret = amlogic_pcie_get_clks_for_m31_phy(amlogic);
		break;
	}

	return ret;
}

int amlogic_pcie_parse_dt(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct device_node *node = dev->of_node;
	struct resource *res;
	int err;

	err = of_property_read_u32(node, "phy-type",
				   &amlogic->phy_type);
	if (err)
		amlogic->phy_type = M31_PHY;
	dev_dbg(amlogic->dev, "PCIE phy type is %d\n", amlogic->phy_type);

	err = amlogic_pcie_get_reset_gpio(amlogic);
	if (err)
		return err;

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "apb-base");
	amlogic->apb_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(amlogic->apb_base)) {
		dev_err(dev, "failed to request apb_base\n");
		return PTR_ERR(amlogic->apb_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "pcictrl-base");
	amlogic->pcictrl_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(amlogic->pcictrl_base)) {
		dev_err(dev, "failed to request pcictrl_base\n");
		return PTR_ERR(amlogic->pcictrl_base);
	}

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "ecam-base");
	if (res) {
		amlogic->ecam_size = resource_size(res);
		amlogic->ecam_bus_base  = res->start;
	} else {
		dev_err(dev, "Missing *ecam-base* reg space\n");
		return -ENODEV;
	}
	amlogic->ecam_base = devm_pci_remap_cfg_resource(dev, res);
	if (IS_ERR(amlogic->ecam_base)) {
		dev_err(dev, "failed to request ecam_base\n");
		return PTR_ERR(amlogic->ecam_base);
	}

	err = amlogic_pcie_get_phys(amlogic);
	if (err)
		return err;

	amlogic->lanes = 1;
	err = of_property_read_u32(node, "num-lanes", &amlogic->lanes);
	if (!err && amlogic->lanes == 0) {
		dev_err(dev, "invalid num-lanes, default to use one lane\n");
		amlogic->lanes = 1;
	}

	err = of_property_read_u32(node, "port-num", &amlogic->port_num);
	if (err)
		amlogic->port_num = 0;
	dev_dbg(dev, "pcie port_num = %d\n", amlogic->port_num);

	err = amlogic_pcie_get_resets(amlogic);
	if (err)
		return err;

	err = amlogic_pcie_get_clks(amlogic);
	if (err)
		return err;

	return 0;
}
EXPORT_SYMBOL_GPL(amlogic_pcie_parse_dt);

bool amlogic_pcie_link_up(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	bool ltssm_linkup = false;
	u32 neg_link_speed = 0;
	int cnt = 0;
	u32 val = 0;
	ktime_t start, end;

	start = ktime_get();

	do {
		val = amlogic_pciectrl_read(amlogic, PCIE_A_CTRL5);
		ltssm_linkup = PCIE_LINK_STATE_CHECK(val, LTSSM_L0);

		if (unlikely(cnt >= WAIT_LINKUP_TIMEOUT)) {
			dev_err(dev, "Error: Wait linkup timeout. Pls check pcie device\n");
			return false;
		} else if (unlikely(cnt >= amlogic->link_times)) {
			dev_info(dev, "%s:%d, ltssm_state=0x%x\n", __func__, __LINE__,
				 ((val >> 18) & 0x1f));
		}

		if (PCIE_LINK_STATE_CHECK(val, LTSSM_L1_IDLE)) {
			dev_info(dev, "ltssm state into L1_IDLE\n");
			break;
		}

		if (PCIE_LINK_STATE_CHECK(val, LTSSM_L2_IDLE)) {
			dev_info(dev, "ltssm state into L2_IDLE\n");
			break;
		}

		val = amlogic_pcieinter_read(amlogic, PCIE_BASIC_STATUS);
		neg_link_speed = (val >> 8) & 0xf;
		if (!neg_link_speed)
			dev_dbg(dev, "speed no okay\n");

		cnt++;
		udelay(1);
	} while (!ltssm_linkup);

	end = ktime_get();
	dev_info_once(dev, "LTSSM consumes time:%llu us\n", ktime_to_us(ktime_sub(end, start)));

	return true;
}

void amlogic_set_max_rd_req_size(struct amlogic_pcie *amlogic, int size)
{
	int max_rd_req_size = 1;
	u32 val = 0;

	switch (size) {
	case 128:
		max_rd_req_size = 0;
		break;
	case 256:
		max_rd_req_size = 1;
		break;
	case 512:
		max_rd_req_size = 2;
		break;
	case 1024:
		max_rd_req_size = 3;
		break;
	case 2048:
		max_rd_req_size = 4;
		break;
	case 4096:
		max_rd_req_size = 5;
		break;
	default:
		max_rd_req_size = 1;
		break;
	}

	val = amlogic_pcieinter_read(amlogic,
				     PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
	val &= (~PCI_EXP_DEVCTL_READRQ);
	val |= (max_rd_req_size << 12);
	amlogic_pcieinter_write(amlogic, val,
				PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
}

void amlogic_set_max_payload(struct amlogic_pcie *amlogic, int size)
{
	int max_payload_size = 1;
	u32 val = 0;

	switch (size) {
	case 128:
		max_payload_size = 0;
		break;
	case 256:
		max_payload_size = 1;
		break;
	case 512:
		max_payload_size = 2;
		break;
	case 1024:
		max_payload_size = 3;
		break;
	case 2048:
		max_payload_size = 4;
		break;
	case 4096:
		max_payload_size = 5;
		break;
	default:
		max_payload_size = 1;
		break;
	}

	val = amlogic_pcieinter_read(amlogic,
				     PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
	val &= (~PCI_EXP_DEVCTL_PAYLOAD);
	val |= (max_payload_size << 5);
	amlogic_pcieinter_write(amlogic, val,
				PCIE_CAP_OFFSET + PCI_EXP_DEVCTL);
}

static int amlogic_pcie_init_port_for_m31_phy(struct amlogic_pcie *amlogic)
{
	int err;
	u32 regs;
	u32 val;

	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val &= ~(1 << amlogic->m31phy_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);
	val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
	val |= (1 << amlogic->m31phy_rst_bit);
	writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

	err = amlogic_pcie_set_reset_for_m31_phy(amlogic, false);
	if (err)
		return err;

	/* XCFGI[706] value from vendor recommend for temperature compensation */
	val = readl(amlogic->phy_base + 0x858);
	val |= BIT(2);
	writel(val, amlogic->phy_base + 0x858);

	/* XCFGI[322:320] value from vendor recommend for temperature compensation */
	val = readl(amlogic->phy_base + 0x828);
	val |= 0x6;
	writel(val, amlogic->phy_base + 0x828);

	/*PHY_Register_XCFGD value from vendor recommend*/
	regs = readl(amlogic->phy_base + 0x470);
	regs |= (1 << 6);
	writel(regs, amlogic->phy_base + 0x470);

	/*set phy for gen3 device*/
	regs = readl(amlogic->phy_base);
	regs |= BIT(19);
	writel(regs, amlogic->phy_base);
	usleep_range(20, 30);

	err = amlogic_pcie_set_reset_for_m31_phy(amlogic, true);
	if (err)
		return err;

	return 0;
}

static int amlogic_pcie_init_port_for_m31_combphy(struct amlogic_pcie *amlogic)
{
	int ret = 0;
	u32 val;

	/* phy init */
	val = readl(amlogic->phy_base);
	val &= ~(BIT(0) | BIT(10) | BIT(22));
	val |= BIT(2) | BIT(12) | BIT(17) | BIT(23) | BIT(25);
	writel(val, amlogic->phy_base);
	val = readl(amlogic->phy_base + 0x4);

	val |= BIT(11);
	writel(val, amlogic->phy_base + 0x4);

	/* XCFGI[706] value from vendor recommend for temperature compensation */
	val = readl(amlogic->phy_base + 0x858);
	val |= BIT(2);
	writel(val, amlogic->phy_base + 0x858);

	/* XCFGI[322:320] value from vendor recommend for temperature compensation */
	val = readl(amlogic->phy_base + 0x828);
	val |= 0x6;
	writel(val, amlogic->phy_base + 0x828);

	ret = amlogic_pcie_set_reset_for_m31_combphy(amlogic);
	if (ret)
		return ret;

	/*
	 * workaround pcie1 ctrl just for S5, S5 pcie0 no need
	 * value recommend by VLSI
	 */
	if (amlogic->port_num == 1 && is_meson_s5_cpu()) {
		val = 0x73f << 2;
		amlogic_pciectrl_write(amlogic, val, 0xb4);
		val = amlogic_pciectrl_read(amlogic, 0xb8);
		val = 0x390073;
		amlogic_pciectrl_write(amlogic, val, 0xb8);

		val = amlogic_pciectrl_read(amlogic, 0xbc);
		val = 0xe0000;
		amlogic_pciectrl_write(amlogic, val, 0xbc);
	}

	return 0;
}

static int amlogic_pcie_do_pll_v1_set(struct amlogic_pcie *amlogic, int cnt)
{
	struct device *dev = amlogic->dev;
	u32 reg[2];
	int ret;

	ret = of_property_read_u32_array(dev_of_node(dev),
					 kasprintf(GFP_KERNEL, "phy_pll_setting_%d", cnt),
					 reg, ARRAY_SIZE(reg));
	if (ret) {
		dev_err(dev, "amlogic phy pll V1 setting read failed\n");
		return ret;
	}

	if (cnt != amlogic->pll_setting_number) {
		if (is_meson_s6_cpu() && is_meson_rev_b() && reg[0] == 0x10)
			reg[1] |= BIT(5);

		writel(reg[1], amlogic->phy_base + reg[0]);
		usleep_range(100, 200);
	} else {
		if (!(readl(amlogic->phy_base + reg[0]) & reg[1]))
			dev_err(dev, "aml pcie phy pll lock failed\n");
		else
			dev_dbg(dev, "aml pcie phy pll locked\n");
	}
	dev_dbg(dev, "aml pcie phy pll cnt=%d, reg[0]=0x%x, reg[1]=0x%x\n", cnt, reg[0], reg[1]);

	return ret;
}

int amlogic_pcie_set_phy_pll(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct device_node *node = dev->of_node;
	u32 aml_phy_pll_version;
	int ret, i;

	ret = of_property_read_u32(node, "aml_phy_pll_version", &aml_phy_pll_version);
	if (ret)
		return ret;

	switch (aml_phy_pll_version) {
	case AMLOGIC_PHY_PLL_V1:
	default:
		dev_dbg(dev, "amlogic pcie phy pll V1\n");
		ret = of_property_read_u32(node, "pll_setting_number",
					   &amlogic->pll_setting_number);
		if (ret)
			return ret;

		for (i = 1; i <= 6; i++)
			ret = amlogic_pcie_do_pll_v1_set(amlogic, i);

		for (i = 8; i <= amlogic->pll_setting_number; i++)
			ret = amlogic_pcie_do_pll_v1_set(amlogic, i);

		ret = amlogic_pcie_set_reset_for_aml_phy(amlogic);
		if (ret)
			return ret;
		break;
	case AMLOGIC_PHY_PLL_V2:
		dev_dbg(dev, "amlogic pcie phy pll V2\n");
		ret = of_property_read_u32(node, "pll_setting_number",
					   &amlogic->pll_setting_number);
		if (ret)
			return ret;
		break;
	}

	return ret;
}

static int amlogic_pcie_init_port_for_aml_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *efuse_res;
	void __iomem *efuse_base = NULL;
	u32 val, tmp;
	int ret = 0, i;

	ret = amlogic_pcie_set_reset_for_aml_phy(amlogic);
	if (ret)
		return ret;

	/*
	 * bit13 : always use pll1 ref clk for debug
	 * writel(readl(amlogic->phy_base + UPCRX_DR_REG1) | BIT(13),
	 *       amlogic->phy_base + UPCRX_DR_REG1);
	 */

	/* fom bypass */
	writel(0x42389800, amlogic->phy_base + EQVAL_CTRL_REG);

	/* CTLE reset */
	writel(0x2c032100, amlogic->phy_base + SW_CTRL_PMA_REG0);

	/* LEQ timer */
	writel(0x222e0, amlogic->phy_base + DELAY_TIME24);

	/* tx rtem odt */
	writel(0x3a98, amlogic->phy_base + SAMPLE_3RD_TIME);

	/* tx detect RX decrease delay to 8us */
	writel(0x1900190, amlogic->phy_base + SAMPLE_1ST_2ND_TIME);

	/* Improved TX signal quality */
	for (i = 0; i < 3; i++)
		writel(0x54da54da, amlogic->phy_base + UPCTX_CONT1_REG2 + i * 4);

	/* modify CDR PH_DIV and FR_DIV */
	for (i = 0; i < 3; i++) {
		val = readl(amlogic->phy_base + REG_DCHD_CDR_GEN1_REG + i * 4);
		val &= ~GENMASK(7, 0);
		val |= 0xa4;
		writel(val, amlogic->phy_base + REG_DCHD_CDR_GEN1_REG + i * 4);
	}

	/* RX and TX trim */
	efuse_res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "pci-efuse");
	efuse_base = devm_ioremap(dev, efuse_res->start, resource_size(efuse_res));
	if (IS_ERR(efuse_base)) {
		dev_err(dev, "failed to request efuse_base\n");
		return PTR_ERR(efuse_base);
	}

	if (efuse_base) {
		tmp = readl(efuse_base);
		dev_dbg(dev, "tx trim = 0x%x\n", (unsigned int)(tmp & GENMASK(5, 0)));
		dev_dbg(dev, "rx trim = 0x%x\n", (unsigned int)((tmp & GENMASK(10, 6)) >> 6));
		/* TX trim */
		if (tmp & BIT(5)) {
			val = readl(amlogic->phy_base + UPCTX_CTRL_REG);
			val &= ~GENMASK(7, 0);
			val |= (tmp & GENMASK(4, 0));
			writel(val, amlogic->phy_base + UPCTX_CTRL_REG);
		} else {
			val = readl(amlogic->phy_base + UPCTX_CTRL_REG);
			val &= ~GENMASK(7, 0);
			val |= 0x10;
			writel(val, amlogic->phy_base + UPCTX_CTRL_REG);
		}

		/* RX trim */
		if (tmp & BIT(10)) {
			for (i = 0; i < 5; i++) {
				val = readl(amlogic->phy_base + REG_DCHA_AFE_LPBK_REG + i * 4);
				val &= ~GENMASK(7, 4);
				val |= (((tmp & GENMASK(9, 6)) >> 6) << 4);
				writel(val, amlogic->phy_base + REG_DCHA_AFE_LPBK_REG + i * 4);
			}
		} else {
			for (i = 0; i < 5; i++) {
				val = readl(amlogic->phy_base + REG_DCHA_AFE_LPBK_REG + i * 4);
				val &= ~GENMASK(7, 4);
				val |= (0x7 << 4);
				writel(val, amlogic->phy_base + REG_DCHA_AFE_LPBK_REG + i * 4);
			}
		}
	}

	/* GEN3 EQ */
	for (i = 0; i < 4; i++) {
		val = readl(amlogic->phy_base + REG_DCHD_EQ_LPBK_REG + i * 4);
		val &= ~(2 << 6);
		val |= (1 << 6);
		writel(val, amlogic->phy_base + REG_DCHD_EQ_LPBK_REG + i * 4);
	}

	/* close RC EQ */
	amlogic_pcieinter_write(amlogic, 0, PHYMAC_CFG);

	/* set phy pll */
	if (of_property_read_bool(dev_of_node(dev), "phy_pll_setting_1")) {
		ret = amlogic_pcie_set_phy_pll(amlogic);
		if (ret)
			return ret;
	}

	return 0;
}

int amlogic_pcie_init_port(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int ret = 0;
	u32 reg;
	u32 val;

	switch (amlogic->phy_type) {
	case AMLOGIC_PHY:
		dev_dbg(dev, " pcie init port and AMLOGIC_PHY\n");
		ret = amlogic_pcie_init_port_for_aml_phy(amlogic);
		break;
	case M31_COMBPHY:
		dev_dbg(dev, " pcie init port and M31_COMBPHY\n");
		ret = amlogic_pcie_init_port_for_m31_combphy(amlogic);
		break;
	case M31_PHY:
	default:
		dev_dbg(dev, " pcie init port and M31_PHY\n");
		ret = amlogic_pcie_init_port_for_m31_phy(amlogic);
		break;
	}

	val = amlogic_pciectrl_read(amlogic, PCIE_A_CTRL0);
	if (amlogic->is_rc)
		val |= PORT_TYPE;
	else
		val &= ~PORT_TYPE;
	amlogic_pciectrl_write(amlogic, val, PCIE_A_CTRL0);

	if (amlogic->link_gen == 1) {
		u32 exp_cap_off = aml_pcie_find_capability(amlogic, PCI_CAP_ID_EXP);

		reg = amlogic_pcieinter_read(amlogic,
					     PCI_CFG_SPACE + exp_cap_off + PCI_EXP_LNKCAP);
		if ((reg & PCI_EXP_LNKCAP_SLS) != PCI_EXP_LNKCAP_SLS_2_5GB) {
			reg &= ~((u32)PCI_EXP_LNKCAP_SLS);
			reg |= PCI_EXP_LNKCAP_SLS_2_5GB;
			amlogic_pcieinter_write(amlogic, reg,
						PCI_CFG_SPACE + exp_cap_off + PCI_EXP_LNKCAP);
		}

		reg = amlogic_pcieinter_readw(amlogic, exp_cap_off + PCI_EXP_LNKCTL2);
		if ((reg & PCI_EXP_LNKCAP_SLS) != PCI_EXP_LNKCAP_SLS_2_5GB) {
			reg &= ~((u32)PCI_EXP_LNKCAP_SLS);
			reg |= PCI_EXP_LNKCAP_SLS_2_5GB;
			amlogic_pcieinter_writew(amlogic, reg, exp_cap_off + PCI_EXP_LNKCTL2);
		}
	} else if (amlogic->link_gen == 2) {
		u32 exp_cap_off = aml_pcie_find_capability(amlogic, PCI_CAP_ID_EXP);

		reg = amlogic_pcieinter_read(amlogic,
					     PCI_CFG_SPACE + exp_cap_off + PCI_EXP_LNKCAP);
		if ((reg & PCI_EXP_LNKCAP_SLS) != PCI_EXP_LNKCAP_SLS_5_0GB) {
			reg &= ~((u32)PCI_EXP_LNKCAP_SLS);
			reg |= PCI_EXP_LNKCAP_SLS_5_0GB;
			amlogic_pcieinter_write(amlogic, reg,
						PCI_CFG_SPACE + exp_cap_off + PCI_EXP_LNKCAP);
		}

		reg = amlogic_pcieinter_readw(amlogic, exp_cap_off + PCI_EXP_LNKCTL2);
		if ((reg & PCI_EXP_LNKCAP_SLS) != PCI_EXP_LNKCAP_SLS_5_0GB) {
			reg &= ~((u32)PCI_EXP_LNKCAP_SLS);
			reg |= PCI_EXP_LNKCAP_SLS_5_0GB;
			amlogic_pcieinter_writew(amlogic, reg, exp_cap_off + PCI_EXP_LNKCTL2);
		}
	} else {
		if (amlogic->link_gen > 3)
			dev_err(dev, "Set pcie parameter link_speed is ERR\n");
	}

	if (amlogic_pcie_set_reset_gpio(amlogic))
		dev_err(dev, "Set pcie reset gpio faile\n");

	if (!amlogic_pcie_link_up(amlogic))
		return -ETIMEDOUT;

	usleep_range(100, 200);
	reg = amlogic_pcieinter_read(amlogic, PCIE_BASIC_STATUS);

	dev_info(dev, "current link speed is GEN%d,link width is x%d\n",
		 ((reg >> 8) & 0x3f), (reg & 0xff));

	return ret;
}
EXPORT_SYMBOL_GPL(amlogic_pcie_init_port);

static int amlogic_pcie_get_m31_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;

	amlogic->pcie_phy = devm_of_phy_get(dev, dev->of_node, "pcie-phy");
	if (IS_ERR(amlogic->pcie_phy)) {
		if (PTR_ERR(amlogic->pcie_phy) != -EPROBE_DEFER)
			goto get_phy_reg;
	}

get_phy_reg:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "phy-base");
	amlogic->phy_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(amlogic->phy_base)) {
		dev_err(dev, "failed to request phy_base\n");
		return PTR_ERR(amlogic->phy_base);
	}
	return 0;
}

static int amlogic_pcie_get_aml_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	struct platform_device *pdev = to_platform_device(dev);
	struct resource *res;

	amlogic->pcie_phy = devm_of_phy_get(dev, dev->of_node, "pcie-phy");
	if (IS_ERR(amlogic->pcie_phy)) {
		if (PTR_ERR(amlogic->pcie_phy) != -EPROBE_DEFER)
			goto get_phy_reg;
	}

get_phy_reg:
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "aml-phy-base");
	amlogic->phy_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(amlogic->phy_base)) {
		dev_err(dev, "failed to request amlogic phy_base\n");
		return PTR_ERR(amlogic->phy_base);
	}

	return 0;
}

int amlogic_pcie_get_phys(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int ret = 0;

	switch (amlogic->phy_type) {
	case AMLOGIC_PHY:
		dev_dbg(dev, " pcie use AMLOGIC_PHY get phy\n");
		ret = amlogic_pcie_get_aml_phy(amlogic);
		break;
	case M31_COMBPHY:
		dev_dbg(dev, " pcie use M31_COMBPHY get phy\n");
		ret = amlogic_pcie_get_m31_phy(amlogic);
		break;
	case M31_PHY:
	default:
		dev_dbg(dev, " pcie use M31_PHY get phy\n");
		ret = amlogic_pcie_get_m31_phy(amlogic);
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(amlogic_pcie_get_phys);

void amlogic_pcie_deinit_phys(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	u32 val;

	switch (amlogic->phy_type) {
	case AMLOGIC_PHY:
		dev_dbg(dev, " pcie deinit AMLOGIC_PHY\n");
		val = readl(amlogic->rst_base + RESETCTRL1_OFFSET);
		val &= ~(BIT(amlogic->amlphy_rst_bit) |
			 BIT(amlogic->pcie_a_rst_bit) |
			 BIT(amlogic->apb_rst_bit) |
			 BIT(amlogic->pcie_pipe_bit) |
			 (amlogic->pcie_rst_mask << amlogic->pcie_rst_bit));
		writel(val, amlogic->rst_base + RESETCTRL1_OFFSET);

		writel(0x0, amlogic->phy_base + UPCTX_CONT0_REG2);
		writel(0x0, amlogic->phy_base + UPCTX_CONT1_REG2);
		writel(0x0, amlogic->phy_base + UPCTX_DR_REG0);
		writel(0x0, amlogic->phy_base + UPCRX_DR_REG0);
		writel(0x0, amlogic->phy_base + UPCRX_DR_REG1);
		writel(0x0, amlogic->phy_base + REG_MISC_GEN1_REG);
		writel(0x0, amlogic->phy_base + REG_DCHA_AFE_GEN1_REG);
		writel(0x0, amlogic->phy_base + REG_DCHA_DFE_GEN1_REG);
		writel(0x0, amlogic->phy_base + REG_DCHD_CDR_GEN1_REG);
		writel(0x0, amlogic->phy_base + REG_DCHD_EQ_GEN1_REG);
		break;
	case M31_COMBPHY:
		dev_dbg(dev, " pcie deinit M31_COMBPHY\n");
		val = readl(amlogic->rst_base + RESETCTRL0_OFFSET);
		val &= ~(1 << amlogic->phy_rst_bit);
		writel(val, amlogic->rst_base + RESETCTRL0_OFFSET);
		break;
	case M31_PHY:
	default:
		dev_dbg(dev, " pcie deinit M31_PHY\n");
		break;
	}
}
EXPORT_SYMBOL_GPL(amlogic_pcie_deinit_phys);

void amlogic_pcie_init_phys(struct amlogic_pcie *amlogic)
{
}
EXPORT_SYMBOL_GPL(amlogic_pcie_init_phys);

static int amlogic_pcie_enable_clks_for_m31_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err = 0, ret = 0;

	ret = clk_set_rate(amlogic->pcie_400m_clk, 400000000);
	err = clk_prepare_enable(amlogic->pcie_400m_clk);
	if (err || ret) {
		dev_err(dev, "unable to enable pcie_400m_clk clock\n");
		return err;
	}

	ret = clk_set_rate(amlogic->pcie_tl_clk, 125000000);
	err = clk_prepare_enable(amlogic->pcie_tl_clk);
	if (err || ret) {
		dev_err(dev, "unable to enable pcie_tl_clk clock\n");
		goto err_400m_clk;
	}

	ret = clk_set_rate(amlogic->cts_pcie_clk, 200000000);
	err = clk_prepare_enable(amlogic->cts_pcie_clk);
	if (err || ret) {
		dev_err(dev, "unable to enable cts_pcie_clk clock\n");
		goto err_tl_clk;
	}

	err = clk_prepare_enable(amlogic->dev_clk);
	if (err) {
		dev_err(dev, "unable to enable dev_clk clock\n");
		goto err_cts_pcie_clk;
	}

	err = clk_prepare_enable(amlogic->phy_clk);
	if (err) {
		dev_err(dev, "unable to enable phy_clk clock\n");
		goto err_dev_clk;
	}

	err = clk_prepare_enable(amlogic->refpll_clk);
	if (err) {
		dev_err(dev, "unable to enable refpll_clk clock\n");
		goto err_phy_clk;
	}

	err = clk_prepare_enable(amlogic->pcie_clk);
	if (err) {
		dev_err(dev, "unable to enable pcie_clk clock\n");
		goto err_refpll_clk;
	}

	return 0;

err_refpll_clk:
	clk_disable_unprepare(amlogic->refpll_clk);
err_phy_clk:
	clk_disable_unprepare(amlogic->phy_clk);
err_dev_clk:
	clk_disable_unprepare(amlogic->dev_clk);
err_cts_pcie_clk:
	clk_disable_unprepare(amlogic->cts_pcie_clk);
err_tl_clk:
	clk_disable_unprepare(amlogic->pcie_tl_clk);
err_400m_clk:
	clk_disable_unprepare(amlogic->pcie_400m_clk);
	return err;
}

static int amlogic_pcie_enable_clks_for_aml_phy(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int err = 0, ret = 0;

	ret = clk_set_rate(amlogic->pcie_tl_clk, 125000000);
	err = clk_prepare_enable(amlogic->pcie_tl_clk);
	if (err || ret) {
		dev_err(dev, "unable to enable pcie_tl_clk clock\n");
		return -ENODEV;
	}

	err = clk_prepare_enable(amlogic->phy_clk);
	if (err) {
		dev_err(dev, "unable to enable phy_clk clock\n");
		goto err_tl_clk;
	}

	err = clk_prepare_enable(amlogic->pcie_clk);
	if (err) {
		dev_err(dev, "unable to enable pcie_clk clock\n");
		goto err_phy_clk;
	}

	return 0;

err_phy_clk:
	clk_disable_unprepare(amlogic->phy_clk);
err_tl_clk:
	clk_disable_unprepare(amlogic->pcie_tl_clk);
	return err;
}

int amlogic_pcie_enable_clocks(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;
	int ret = 0;

	switch (amlogic->phy_type) {
	case AMLOGIC_PHY:
		dev_dbg(dev, " pcie use AMLOGIC_PHY enable clks\n");
		ret = amlogic_pcie_enable_clks_for_aml_phy(amlogic);
		break;
	case M31_COMBPHY:
		dev_dbg(dev, " pcie use M31_COMBPHY enable clks\n");
		ret = amlogic_pcie_enable_clks_for_m31_phy(amlogic);
		break;
	case M31_PHY:
	default:
		dev_dbg(dev, " pcie use M31_PHY enable clks\n");
		ret = amlogic_pcie_enable_clks_for_m31_phy(amlogic);
		break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(amlogic_pcie_enable_clocks);

static void amlogic_pcie_disable_clks_for_m31_phy(struct amlogic_pcie *amlogic)
{
	clk_disable_unprepare(amlogic->pcie_clk);
	clk_disable_unprepare(amlogic->refpll_clk);
	clk_disable_unprepare(amlogic->phy_clk);
	clk_disable_unprepare(amlogic->dev_clk);
	clk_disable_unprepare(amlogic->cts_pcie_clk);
	clk_disable_unprepare(amlogic->pcie_tl_clk);
	clk_disable_unprepare(amlogic->pcie_400m_clk);
}

static void amlogic_pcie_disable_clks_for_aml_phy(struct amlogic_pcie *amlogic)
{
	clk_disable_unprepare(amlogic->pcie_clk);
	clk_disable_unprepare(amlogic->phy_clk);
	clk_disable_unprepare(amlogic->pcie_tl_clk);
	clk_disable_unprepare(amlogic->pcie_400m_clk);
}

void amlogic_pcie_disable_clocks(struct amlogic_pcie *amlogic)
{
	struct device *dev = amlogic->dev;

	switch (amlogic->phy_type) {
	case AMLOGIC_PHY:
		dev_dbg(dev, " pcie use AMLOGIC_PHY disable clks\n");
		amlogic_pcie_disable_clks_for_aml_phy(amlogic);
		break;
	case M31_COMBPHY:
		dev_dbg(dev, " pcie use M31_COMBPHY disable clks\n");
		amlogic_pcie_disable_clks_for_m31_phy(amlogic);
		break;
	case M31_PHY:
	default:
		dev_dbg(dev, " pcie use M31_PHY disable clks\n");
		amlogic_pcie_disable_clks_for_m31_phy(amlogic);
		break;
	}
}
EXPORT_SYMBOL_GPL(amlogic_pcie_disable_clocks);

void amlogic_pcie_cfg_addr_map(struct amlogic_pcie *amlogic,
			       unsigned int atr_base,
			       u64 src_addr,
			       u64 trsl_addr,
			       int size,
			       int trsl_param)
{
	struct device *dev = amlogic->dev;
	u32 val;

	/* ATR_SRC_ADDR_LOW:
	 *  - bit 0: enable entry,
	 *  - bits 1-6: ATR window size: total size in bytes: 2^(ATR_WSIZE + 1)
	 *  - bits 7-11: reserved
	 *  - bits 12-31: start of source address
	 */
	val = (src_addr & 0xfffff000) | ((size & 0x3f) << 1) | (1 << 0);
	amlogic_pcieinter_write(amlogic, val, atr_base + ATR_SRC_ADDR_LOW);

	amlogic_pcieinter_write(amlogic,
				(src_addr >> 32),
				atr_base + ATR_SRC_ADDR_HIGH);
	amlogic_pcieinter_write(amlogic,
				(trsl_addr & 0xfffff000),
				atr_base + ATR_TRSL_ADDR_LOW);
	amlogic_pcieinter_write(amlogic,
				(trsl_addr >> 32),
				atr_base + ATR_TRSL_ADDR_HIGH);
	amlogic_pcieinter_write(amlogic, trsl_param,
				atr_base + ATR_TRSL_PARAM);

	dev_dbg(dev,
		"ATR Map:0x%010llx %s 0x%010llx [0x%010llx] (param: 0x%06x)\n",
		src_addr, (trsl_param & 0x400000) ? "<-" : "->", trsl_addr,
		((u64)1) << (size + 1), trsl_param);
}
EXPORT_SYMBOL_GPL(amlogic_pcie_cfg_addr_map);
