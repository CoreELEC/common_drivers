# SPDX-License-Identifier: GPL-2.0-only

MODULE_NAME = meson-remote

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

obj-$(CONFIG_AMLOGIC_INPUT) = $(MODULE_NAME).o
$(MODULE_NAME)-y = main.o

obj-$(CONFIG_AMLOGIC_GPIO_KEY) += keyboard/gpio_keypad.o

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/input/ir/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/input/keyboard/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/input/meson_pmic6b/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/input/watchkey/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/input/misc/Makefile
