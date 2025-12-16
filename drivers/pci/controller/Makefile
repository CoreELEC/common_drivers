# SPDX-License-Identifier: GPL-2.0-only
# Amlogic pcie drivers

MODULE_NAME = amlogic-pcie

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

obj-$(CONFIG_AMLOGIC_PCIE) += $(MODULE_NAME).o
$(MODULE_NAME)-y = main.o amlogic-pcie-v3-comm.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V2_HOST) += amlogic-pcie-v2-host.o
CFLAGS_amlogic-pcie-v2-host.o += $(PR_FMT_DEFINE) -I $(srctree)/drivers/pci/controller/dwc

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V3_HOST) += amlogic-pcie-v3-host.o
CFLAGS_amlogic-pcie-v3-host.o += $(PR_FMT_DEFINE) -I $(srctree)/drivers/pci/

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V3_EP) += amlogic-pcie-v3-ep.o pci_endpoint_test.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V4_HOST) += amlogic-pcie-v4-host.o
CFLAGS_amlogic-pcie-v4-host.o += $(PR_FMT_DEFINE) -I $(srctree)/drivers/pci/controller/dwc
