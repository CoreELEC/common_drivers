# SPDX-License-Identifier: (GPL-2.0+ OR MIT)


MODULE_NAME = amlogic-usb
obj-$(CONFIG_AMLOGIC_USB_SUPPORT)      += $(MODULE_NAME).o
$(MODULE_NAME)-y = usb_main.o

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE) #-DDEBUG

obj-$(CONFIG_TYPEC) += typec/

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/phy_meson_usb/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/crgdrdphy/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/usb2phy/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/usb3v2phy/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/usbc2phy/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/usb3v3phy/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/usbotg/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/cc/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/bc/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/xhci_amlogic/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/dwc3_amlogic/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/crg/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/usb/dwc_otg/310/Makefile
