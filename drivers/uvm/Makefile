# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
#
# Makefile for amlogic uvm
#

MESON_UVM_MODULE_NAME = aml_uvm
obj-$(CONFIG_AMLOGIC_UVM) = $(MESON_UVM_MODULE_NAME).o

ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media \
             -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/common/ion_dev

KBUILD_EXTRA_SYMBOLS += $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/dhp/Module.symvers

$(MESON_UVM_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_ALLOCATOR) = meson_uvm_allocator.o
$(MESON_UVM_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE) += meson_uvm_core.o
$(MESON_UVM_MODULE_NAME)-$(CONFIG_AMLOGIC_UVM_CORE) += meson_uvm_interface.o
