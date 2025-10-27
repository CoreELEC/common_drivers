# SPDX-License-Identifier: GPL-2.0-only

MODULE_NAME = amlogic-global-timer

obj-$(CONFIG_AMLOGIC_GLOBAL_TIMER) += $(MODULE_NAME).o

$(MODULE_NAME)-$(CONFIG_AMLOGIC_GLOBAL_TIMER) += main.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_GLOBAL_TIMER_CORE) += global_timer_core.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_GLOBAL_TIMER_ISP) += global_timer_isp.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_GLOBAL_TIMER_OUTPUT) += global_timer_output.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_GLOBAL_TIMER_INPUT) += global_timer_input.o

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)
