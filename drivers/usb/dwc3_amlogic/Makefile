# SPDX-License-Identifier: GPL-2.0
# define_trace.h needs to know how to find our header
CFLAGS_trace.o				:= -I$(src)

$(MODULE_NAME)-$(CONFIG_AMLOGIC_USB_DWC3)				+= dwc3_amlogic/core-meson.o

ifneq ($(CONFIG_TRACING),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/trace-meson.o
endif

ifneq ($(filter y,$(CONFIG_AMLOGIC_USB_DWC3_HOST) $(CONFIG_AMLOGIC_USB_DWC3_DUAL_ROLE)),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/host-meson.o
endif

ifneq ($(filter y,$(CONFIG_AMLOGIC_USB_DWC3_GADGET) $(CONFIG_AMLOGIC_USB_DWC3_DUAL_ROLE)),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/gadget-meson.o dwc3_amlogic/ep0-meson.o
endif

ifneq ($(CONFIG_AMLOGIC_USB_DWC3_DUAL_ROLE),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/drd-meson.o
endif

ifneq ($(CONFIG_AMLOGIC_USB_DWC3_ULPI),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/ulpi-meson.o
endif

ifneq ($(CONFIG_AMLOGIC_USB_DWC3_DEBUGFS),)
	$(MODULE_NAME)-y				+= dwc3_amlogic/debugfs-meson.o
endif

##
# Platform-specific glue layers go here
#
# NOTICE: Make sure your glue layer doesn't depend on anything
# which is arch-specific and that it compiles on all situations.
#
# We want to keep this requirement in order to be able to compile
# the entire driver (with all its glue layers) on several architectures
# and make sure it compiles fine. This will also help with allmodconfig
# and allyesconfig builds.
##
$(MODULE_NAME)-$(CONFIG_AMLOGIC_USB_DWC3_OTG)		+= dwc3_amlogic/dwc3-otg.o
