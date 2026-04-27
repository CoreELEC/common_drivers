# SPDX-License-Identifier: GPL-2.0-only
# Amlogic pcie drivers

ccflags-y += -I $(srctree)/drivers/pci/controller/dwc -I $(srctree)/drivers/pci/

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V2_HOST) += controller/amlogic-pcie-v2-host.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V3_HOST) += controller/amlogic-pcie-v3-host.o \
       						controller/amlogic-pcie-v3-comm.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V3_EP) += controller/amlogic-pcie-v3-ep.o \
						controller/pci_endpoint_test.o \
						controller/amlogic-pcie-v3-comm.o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PCIE_V4_HOST) += controller/amlogic-pcie-v4-host.o
