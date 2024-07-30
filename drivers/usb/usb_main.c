// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
//#define DEBUG
#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/ctype.h>
#include <linux/kallsyms.h>
#include "usb_main.h"
#include <linux/amlogic/module_merge.h>

struct dentry *amlogic_usb_debugfs_root;

int get_otg_mode(void)
{
	return 0;  /* force_device_mode */
}
EXPORT_SYMBOL(get_otg_mode);

static inline void __init amlogic_usb_debugfs_create_root(void)
{
	amlogic_usb_debugfs_root = debugfs_create_dir("amlogic", usb_debug_root);
}

//void __exit amlogic_usb_debugfs_remove_root(void)
//{
//	debugfs_remove_recursive(amlogic_usb_debugfs_root);
//	amlogic_usb_debugfs_root = NULL;
//}

static int __init usb_main_init(void)
{
	pr_debug("### %s() start\n", __func__);
	amlogic_usb_debugfs_create_root();
	call_sub_init(amlogic_new_c2_usb2_v2_driver_init);//usbc2phy
	call_sub_init(amlogic_new_c2_usb3_v2_driver_init);//usbc2phy
	call_sub_init(amlogic_new_usb3_v3_driver_init); //usb3v3phy
	call_sub_init(amlogic_cc_driver_init);		//cc
	call_sub_init(amlogic_bc_driver_init);		//bc

	call_sub_init(amlogic_new_usb2_v2_driver_init); //usb2phy/amlogic_usb2_phy.ko
	call_sub_init(amlogic_new_usb3_v2_driver_init);	//usb3v2phy/amlogic_usb3_v2_phy.ko
	call_sub_init(amlogic_usb2_m31_drv_init);	//crgdrdphy/
	call_sub_init(amlogic_usb3_m31_drv_init);	//crgdrdphy/
	call_sub_init(aml_usb3_phy_drv_init);

	call_sub_init(aml_xhci_hcd_init);
	call_sub_init(aml_xhci_plat_init);
	call_sub_init(aml_dwc3_init);
	call_sub_init(amlogic_crg_init);		//crg/amlogic_usb_crg.ko
	call_sub_init(amlogic_new_otg_driver_init);	//usbotg/amlogic_usb_otg.ko
	//call_sub_init(dwc_otg_init);			//dwc_otg/310/dwc_otg.ko
	call_sub_init(amlogic_crg_drd_usb2_drv_init);	//crgdrdphy/amlogic_usb2_crg_drd_phy.ko
	call_sub_init(amlogic_crg_drd_usb3_drv_init);	//crgdrdphy/amlogic_usb3_crg_drd_phy.ko
	call_sub_init(amlogic_crg_host_driver_init);	//crg/amlogic_usb_crg_drd.ko
	call_sub_init(crg_otg_init);			//crg/amlogic_usb_crg
	call_sub_init(crg_otg_v2_init);			//crg/amlogic_usb_crg.ko

	pr_debug("### %s() end\n", __func__);
	return 0;
}

late_initcall(usb_main_init);
MODULE_LICENSE("GPL v2");
