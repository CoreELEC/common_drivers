# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MODULE_NAME = amlogic-fw-antirollback
obj-$(CONFIG_AMLOGIC_FW_ANTIROLLBACK) = $(MODULE_NAME).o
$(MODULE_NAME)-y += meson-fw-antirollback.o

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)