/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _USB_MAIN_H__
#define _USB_MAIN_H__
#include <linux/debugfs.h>
#include <linux/usb.h>

extern bool amlogic_usb_debug;

#define AM_USB_DEBUG(dev, fmt, args...)				\
do {									\
	if (amlogic_usb_debug)						\
		dev_info(dev, fmt, ## args);			\
} while (0)

#if IS_ENABLED(CONFIG_AMLOGIC_USBPHY)
int __init amlogic_new_usb3_v2_driver_init(void);
#else
static inline  int __init amlogic_new_usb3_v2_driver_init(void)
{
	return -1;
}

#endif

#if IS_ENABLED(CONFIG_AMLOGIC_USB2PHY)
int __init amlogic_new_usb2_v2_driver_init(void);
#else
static inline  int __init amlogic_new_usb2_v2_driver_init(void)
{
	return -1;
}

#endif

#if IS_ENABLED(CONFIG_AMLOGIC_USB3PHY)
int __init amlogic_new_otg_driver_init(void);
int __init amlogic_new_usb3_v3_driver_init(void);
#else
static inline  int __init __maybe_unused amlogic_new_otg_driver_init(void)
{
	return -1;
}

static inline  int __init __maybe_unused amlogic_new_usb3_v3_driver_init(void)
{
	return -1;
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_USBPHYC2)
int __init amlogic_new_c2_usb2_v2_driver_init(void);
int __init amlogic_new_c2_usb3_v2_driver_init(void);
#else
static inline  int __init __maybe_unused amlogic_new_c2_usb2_v2_driver_init(void)
{
	return -1;
}

static inline  int __init __maybe_unused amlogic_new_c2_usb3_v2_driver_init(void)
{
	return -1;
}

#endif

#if IS_ENABLED(CONFIG_AMLOGIC_CRG)
int __init amlogic_crg_drd_usb2_drv_init(void);
int __init amlogic_crg_init(void);
int __init amlogic_crg_drd_usb3_drv_init(void);
int __init amlogic_crg_host_driver_init(void);
void __exit amlogic_crg_host_driver_exit(void);
int __init crg_drd_glue_init(void);
int __init amlogic_usb3_m31_drv_init(void);
int __init amlogic_usb2_m31_drv_init(void);
int __init crg_otg_init(void);
int __init crg_otg_v2_init(void);
void crg_exit(void);
int crg_init(void);
void crg_gadget_exit(void);
int crg_gadget_init(void);
const char *crg_udc_get_UDC_name(void);
int crg_otg_write_UDC(const char *udc_name);
#else
static inline  int __init __maybe_unused amlogic_crg_drd_usb2_drv_init(void)
{
	return -1;
}

static inline  int __init __maybe_unused amlogic_crg_init(void)
{
	return -1;
}

static inline  int __init __maybe_unused amlogic_crg_drd_usb3_drv_init(void)
{
	return -1;
}

static inline  int __init __maybe_unused amlogic_crg_host_driver_init(void)
{
	return -1;
}

static inline void __exit __maybe_unused amlogic_crg_host_driver_exit(void)
{
}

static inline int __init crg_drd_glue_init(void)
{
	return -1;
}

static inline int __init __maybe_unused crg_otg_init(void)
{
	return -1;
}

static inline  int __init __maybe_unused crg_otg_v2_init(void)
{	return -1;
}

static int __init __maybe_unused amlogic_usb3_m31_drv_init(void)
{
	return -1;
}

static int __init __maybe_unused amlogic_usb2_m31_drv_init(void)
{
	return -1;
}

static inline void crg_exit(void)
{
}

static inline int crg_init(void)
{
	return -1;
}

static inline void crg_gadget_exit(void)
{
}

static inline int crg_gadget_init(void)
{
	return -1;
}

static inline const char *crg_udc_get_UDC_name(void)
{
	return ERR_PTR(-ENODEV);
}

static inline int crg_otg_write_UDC(const char *udc_name)
{
	return -1;
}
#endif

#if IS_ENABLED(CONFIG_MESON_USBPHY)
int __init meson_uphy_drv_init(void);
void __exit meson_uphy_drv_exit(void);
#else
static inline int __init meson_uphy_drv_init(void)
{
	return -1;
}

static inline void __exit meson_uphy_drv_exit(void);
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_CC)
int __init amlogic_cc_driver_init(void);
#else
static inline  int __init __maybe_unused amlogic_cc_driver_init(void)
{
	return -1;
}

#endif

#if IS_ENABLED(CONFIG_AMLOGIC_BC)
int __init amlogic_bc_driver_init(void);
#else
static inline  int __init __maybe_unused amlogic_bc_driver_init(void)
{
	return -1;
}
#endif

int get_otg_mode(void);
int __init aml_xhci_plat_init(void);
void __exit aml_xhci_plat_exit(void);
int __init aml_xhci_hcd_init(void);
void __exit aml_xhci_hcd_fini(void);
#if IS_ENABLED(CONFIG_AMLOGIC_USB_DWC_OTG_HCD)
int __init dwc_otg_init(void);
void __exit dwc_otg_exit(void);
#else
static inline int __init dwc_otg_init(void)
{
	return -1;
}

static inline void __exit dwc_otg_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_USB_DWC3)
int __init aml_dwc3_init(void);
void __exit aml_dwc3_exit(void);
#else
static inline int __init aml_dwc3_init(void)
{
	return -1;
}

static inline void __exit aml_dwc3_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_USB_DWC3_OTG)
int __init dwc3_otg_init(void);
void __exit dwc3_otg_exit(void);
#else
static inline int __init dwc3_otg_init(void)
{
	return -1;
}

static inline void __exit dwc3_otg_exit(void)
{
}
#endif
#endif
