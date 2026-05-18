# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

ccflags-y += -I$(srctree)/drivers/thermal

MODULE_NAME = amlogic-thermal

obj-$(CONFIG_AMLOGIC_AMLOGIC_THERMAL) = $(MODULE_NAME).o
$(MODULE_NAME)-y += thermal_init.o
$(MODULE_NAME)-y += meson_tsensor.o amlogic_thermal.o vir_tsens.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COOLDEV) += meson_cooldev.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_GPU_THERMAL) += gpu_cooling.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_DDR_THERMAL) += ddr_params.o ddr_control.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_CPUCORE_THERMAL) += cpucore_cooling.o

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)
