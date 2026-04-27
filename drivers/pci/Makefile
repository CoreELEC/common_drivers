# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MODULE_NAME = amlogic-pcie

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

obj-$(CONFIG_AMLOGIC_PCIE) += $(MODULE_NAME).o
$(MODULE_NAME)-y = main.o

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/pci/controller/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/pci/phy/Makefile
