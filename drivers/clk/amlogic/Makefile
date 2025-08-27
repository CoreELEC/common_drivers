# SPDX-License-Identifier: GPL-2.0-only
# Amlogic clock drivers

MODULE_NAME = amlogic-clk-v2
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_V2) = clk-main.o
obj-$(CONFIG_AMLOGIC_COMMON_CLK_V2) = $(MODULE_NAME).o
$(MODULE_NAME)-y += of-clk-common.o clk-basic.o clk-dualdiv-mux.o clk-generals.o clk-noglitch.o clk-pll.o

ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/clk/amlogic

PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
CFLAGS_of-clk-common.o += $(PR_FMT_DEFINE)
CFLAGS_clk-basic.o += $(PR_FMT_DEFINE)
CFLAGS_clk-dualdiv-mux.o += $(PR_FMT_DEFINE)
CFLAGS_clk-generals.o += $(PR_FMT_DEFINE)
CFLAGS_clk-noglitch.o += $(PR_FMT_DEFINE)
CFLAGS_clk-pll.o += $(PR_FMT_DEFINE)
CFLAGS_clk-main.o += $(PR_FMT_DEFINE)
