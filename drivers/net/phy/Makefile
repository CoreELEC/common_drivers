# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MODULE_NAME = amlogic-inphy
PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
CFLAGS_meson-gxl.o += $(PR_FMT_DEFINE)
obj-$(CONFIG_AMLOGIC_INPHY) = $(MODULE_NAME).o
$(MODULE_NAME)-y += meson-gxl.o

RTL_MODULE_NAME = amlogic-realtek
PR_FMT_RTL = $(subst amlogic-,,$(RTL_MODULE_NAME))
PR_FMT_DEFINE_RTL = "-Dpr_fmt(fmt)= \"[$(PR_FMT_RTL)]: \" fmt"
CFLAGS_realtek.o += $(PR_FMT_DEFINE_RTL)
obj-$(CONFIG_AMLOGIC_REALTEK) += $(RTL_MODULE_NAME).o
$(RTL_MODULE_NAME)-y += realtek.o

MAE_MODULE_NAME = amlogic-maxio
PR_FMT_MAE = $(subst amlogic-,,$(MAE_MODULE_NAME))
PR_FMT_DEFINE_MAE = "-Dpr_fmt(fmt)= \"[$(PR_FMT_MAE)]: \" fmt"
CFLAGS_maxio.o += $(PR_FMT_DEFINE_MAE)
obj-$(CONFIG_AMLOGIC_MAXIO) += $(MAE_MODULE_NAME).o
$(MAE_MODULE_NAME)-y += maxio.o

JLSEMI_MODULE_NAME = amlogic-jlsemi
PR_FMT_MAXIO = $(subst amlogic-,,$(JLSEMI_MODULE_NAME))
PR_FMT_DEFINE_JLSEMI = "-Dpr_fmt(fmt)= \"[$(PR_FMT_JLSEMI)]: \" fmt"
CFLAGS_jlsemi.o += $(PR_FMT_DEFINE_JLSEMI)
obj-$(CONFIG_AMLOGIC_JLSEMI) += $(JLSEMI_MODULE_NAME).o
$(JLSEMI_MODULE_NAME)-y += jlsemi.o jlsemi-core.o
