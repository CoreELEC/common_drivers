# SPDX-License-Identifier: GPL-2.0-only
#obj-$(CONFIG_MESON_I3C_MASTER)		+= amlogic-i3c-master.o
# SPDX-License-Identifier: (GPL-2.0+ OR MIT)
MODULE_NAME = amlogic-i3c

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
ccflags-y += $(PR_FMT_DEFINE)

obj-$(CONFIG_AMLOGIC_I3C) = $(MODULE_NAME).o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_I3C_MASTER) += amlogic-i3c-master.o
