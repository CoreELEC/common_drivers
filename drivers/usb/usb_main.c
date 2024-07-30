// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

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

#define call_sub_exit(func) { \
	pr_debug("call %s()\n", #func); \
	func(); \
}

bool force_device_mode;
module_param_named(otg_device, force_device_mode,
		bool, 0644);
static char otg_mode_string[2] = "0";

static int force_otg_mode(char *s)
{
	if (s)
		snprintf(otg_mode_string, 2, "%s", s);
	if (strncmp(otg_mode_string, "0", 1) == 0)
		force_device_mode = 0;
	else
		force_device_mode = 1;
	return 1;
}
__setup("otg_device=", force_otg_mode);

int get_otg_mode(void)
{
	return force_device_mode;
}
EXPORT_SYMBOL(get_otg_mode);

bool amlogic_usb_debug;
module_param(amlogic_usb_debug, bool, 0644);
MODULE_PARM_DESC(amlogic_usb_debug, "0 or 1");

/* TODO: Remove all these phy init once the phy porting is done.
 * You can still use legacy phy driver by undefining this MARCO.
 */
#define AML_USBPHY_REMOVE_LEGACY

static int __init usb_main_init(void)
{
	pr_debug("### %s() start\n", __func__);

#ifndef AML_USBPHY_REMOVE_LEGACY
	call_sub_init(amlogic_new_c2_usb2_v2_driver_init);//usbc2phy
	call_sub_init(amlogic_new_c2_usb3_v2_driver_init);//usbc2phy
	call_sub_init(amlogic_new_usb3_v3_driver_init); //usb3v3phy
	call_sub_init(amlogic_cc_driver_init);		//cc
	call_sub_init(amlogic_bc_driver_init);		//bc
#endif

	call_sub_init(crg_drd_glue_init);
	call_sub_init(meson_uphy_drv_init);

#ifndef AML_USBPHY_REMOVE_LEGACY
	call_sub_init(amlogic_new_usb2_v2_driver_init); //usb2phy/amlogic_usb2_phy.ko
	call_sub_init(amlogic_new_usb3_v2_driver_init);	//usb3v2phy/amlogic_usb3_v2_phy.ko
	call_sub_init(amlogic_usb2_m31_drv_init);	//crgdrdphy/
	call_sub_init(amlogic_usb3_m31_drv_init);	//crgdrdphy/
#endif

	call_sub_init(aml_xhci_hcd_init);
	call_sub_init(aml_xhci_plat_init);
	call_sub_init(aml_dwc3_init);
	call_sub_init(meson_dwc3_otg_switch_init);

#ifndef AML_USBPHY_REMOVE_LEGACY
	call_sub_init(amlogic_crg_init);		//crg/amlogic_usb_crg.ko
	call_sub_init(amlogic_new_otg_driver_init);	//usbotg/amlogic_usb_otg.ko
	call_sub_init(amlogic_crg_drd_usb2_drv_init);	//crgdrdphy/amlogic_usb2_crg_drd_phy.ko
	call_sub_init(amlogic_crg_drd_usb3_drv_init);	//crgdrdphy/amlogic_usb3_crg_drd_phy.ko
#endif

	call_sub_init(amlogic_crg_host_driver_init);

#ifndef AML_USBPHY_REMOVE_LEGACY
	call_sub_init(crg_otg_init);	//crgdrdphy/
	call_sub_init(crg_otg_v2_init);	//crgdrdphy/
#endif

	call_sub_init(dwc_otg_init);

	pr_debug("### %s() end\n", __func__);
	return 0;
}

static void __exit usb_main_exit(void)
{
	pr_debug("### %s() start\n", __func__);
	call_sub_exit(dwc_otg_exit);
	call_sub_exit(amlogic_crg_host_driver_exit);
	call_sub_exit(meson_dwc3_otg_switch_exit);
	call_sub_exit(aml_dwc3_exit);
	call_sub_exit(aml_xhci_plat_exit);
	call_sub_exit(aml_xhci_hcd_fini);
	call_sub_exit(meson_uphy_drv_exit);
	pr_debug("### %s() end\n", __func__);
}

late_initcall(usb_main_init);
module_exit(usb_main_exit);
MODULE_LICENSE("GPL v2");
