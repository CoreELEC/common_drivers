#
# Makefile for gpio read
#

#obj-$(CONFIG_SEI_GPIO_CTRL) += gpio_ctrl.o

# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MODULE_NAME = sei_gpio_ctrl
obj-$(CONFIG_SEI_GPIO_CTRL) += $(MODULE_NAME).o
$(MODULE_NAME)-y += gpio_ctrl.o

# for [sei_gpio_ctrl] kernel log prefix
PR_FMT = $(subst sei-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

