// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "main.h"

static int __init pcie_main_init(void)
{
	pr_debug("### %s() start\n", __func__);
	call_sub_init(aml_pcie_phy_a9_init);
	call_sub_init(aml_pcie_rc_v2_init);
	call_sub_init(aml_pcie_rc_v3_init);
	call_sub_init(aml_pcie_rc_v4_init);
	call_sub_init(aml_pcie_ep_v3_init);
	call_sub_init(aml_pcie_ep_test_init);
	pr_debug("### %s() end\n", __func__);
	return 0;
}

static void __exit pcie_main_exit(void)
{
	aml_pcie_phy_a9_exit();
	aml_pcie_rc_v2_exit();
	aml_pcie_rc_v3_exit();
	aml_pcie_rc_v4_exit();
	aml_pcie_ep_v3_exit();
	aml_pcie_ep_test_exit();
}

module_init(pcie_main_init);
module_exit(pcie_main_exit);

MODULE_LICENSE("GPL v2");
