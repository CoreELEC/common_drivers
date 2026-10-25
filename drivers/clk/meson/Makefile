# SPDX-License-Identifier: GPL-2.0-only
# Amlogic clock drivers

MODULE_NAME = amlogic-clk
obj-$(CONFIG_AMLOGIC_COMMON_CLK) = $(MODULE_NAME).o

ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/clk/meson

$(MODULE_NAME)-y = clk-common-init.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_AO_CLKC) += meson-aoclk.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_CPU_DYNDIV) += clk-cpu-dyndiv.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_DUALDIV) += clk-dualdiv.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_EE_CLKC) += meson-eeclk.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_MPLL) += clk-mpll.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_PHASE) += clk-phase.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_PLL) += clk-pll.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_REGMAP) += clk-regmap.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_SCLK_DIV) += sclk-div.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_VID_PLL_DIV) += vid-pll-div.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_COMMON_CLK_MESON_CLKC_UTILS) += meson-clkc-utils.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_MESON_CLK_MEASURE) += meson-clk-measure.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_CLK_DEBUG) += meson-clk-debug.o
PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
CFLAGS_meson-aoclk.o += $(PR_FMT_DEFINE)
CFLAGS_clk-cpu-dyndiv.o += $(PR_FMT_DEFINE)
CFLAGS_clk-dualdiv.o += $(PR_FMT_DEFINE)
CFLAGS_meson-eeclk.o += $(PR_FMT_DEFINE)
CFLAGS_clk-mpll.o += $(PR_FMT_DEFINE)
CFLAGS_clk-phase.o += $(PR_FMT_DEFINE)
CFLAGS_clk-pll.o += $(PR_FMT_DEFINE)
CFLAGS_clk-regmap.o += $(PR_FMT_DEFINE)
CFLAGS_vid-pll-div.o += $(PR_FMT_DEFINE)
CFLAGS_meson-clk-measure.o += $(PR_FMT_DEFINE)
CFLAGS_meson-clk-debug.o += $(PR_FMT_DEFINE)

S4_MODULE_NAME = amlogic-clk-soc-s4
obj-$(CONFIG_AMLOGIC_COMMON_CLK_S4) += $(S4_MODULE_NAME).o
$(S4_MODULE_NAME)-y = s4.o
PR_FMT_S4 = $(subst amlogic-,,$(S4_MODULE_NAME))
PR_FMT_DEFINE_S4="-Dpr_fmt(fmt)= \"[$(PR_FMT_S4)]: \" fmt"
CFLAGS_s4.o += $(PR_FMT_DEFINE_S4)

SC2_MODULE_NAME = amlogic-clk-soc-sc2
obj-$(CONFIG_AMLOGIC_COMMON_CLK_SC2) += $(SC2_MODULE_NAME).o
$(SC2_MODULE_NAME)-y = sc2.o
PR_FMT_SC2 = $(subst amlogic-,,$(SC2_MODULE_NAME))
PR_FMT_DEFINE_SC2="-Dpr_fmt(fmt)= \"[$(PR_FMT_SC2)]: \" fmt"
CFLAGS_sc2.o += $(PR_FMT_DEFINE_SC2)
obj-$(subst m,,$(CONFIG_AMLOGIC_COMMON_CLK_SC2)) += sc2-scmi-fixed.o

C2_MODULE_NAME = amlogic-clk-soc-c2
obj-$(CONFIG_AMLOGIC_COMMON_CLK_C2) += $(C2_MODULE_NAME).o
$(C2_MODULE_NAME)-y += c2.o
PR_FMT_C2 = $(subst amlogic-,,$(C2_MODULE_NAME))
PR_FMT_DEFINE_C2="-Dpr_fmt(fmt)= \"[$(PR_FMT_C2)]: \" fmt"
CFLAGS_c2.o += $(PR_FMT_DEFINE_C2)

C3_MODULE_NAME = amlogic-clk-soc-c3
obj-$(CONFIG_AMLOGIC_COMMON_CLK_C3) += $(C3_MODULE_NAME).o
$(C3_MODULE_NAME)-y += c3.o
PR_FMT_C3 = $(subst amlogic-,,$(C3_MODULE_NAME))
PR_FMT_DEFINE_C3="-Dpr_fmt(fmt)= \"[$(PR_FMT_C3)]: \" fmt"
CFLAGS_c3.o += $(PR_FMT_DEFINE_C3)

C4_MODULE_NAME = amlogic-clk-soc-c4
obj-$(CONFIG_AMLOGIC_COMMON_CLK_C4) += $(C4_MODULE_NAME).o
$(C4_MODULE_NAME)-y += c4.o c4-aoclk.o c4-main.o
PR_FMT_C4 = $(subst amlogic-,,$(C4_MODULE_NAME))
PR_FMT_DEFINE_C4="-Dpr_fmt(fmt)= \"[$(PR_FMT_C4)]: \" fmt"
CFLAGS_c4.o += $(PR_FMT_DEFINE_C4)

A1_MODULE_NAME = amlogic-clk-soc-a1
obj-$(CONFIG_AMLOGIC_COMMON_CLK_A1) += $(A1_MODULE_NAME).o
$(A1_MODULE_NAME)-y += a1.o
PR_FMT_A1 = $(subst amlogic-,,$(A1_MODULE_NAME))
PR_FMT_DEFINE_A1="-Dpr_fmt(fmt)= \"[$(PR_FMT_A1)]: \" fmt"
CFLAGS_a1.o += $(PR_FMT_DEFINE_A1)

T3_MODULE_NAME = amlogic-clk-soc-t3
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T3) += $(T3_MODULE_NAME).o
$(T3_MODULE_NAME)-y += t3.o
PR_FMT_T3 = $(subst amlogic-,,$(T3_MODULE_NAME))
PR_FMT_DEFINE_T3="-Dpr_fmt(fmt)= \"[$(PR_FMT_T3)]: \" fmt"
CFLAGS_t3.o += $(PR_FMT_DEFINE_T3)

T7_MODULE_NAME = amlogic-clk-soc-t7
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T7) += $(T7_MODULE_NAME).o
$(T7_MODULE_NAME)-y += t7.o
PR_FMT_T7 = $(subst amlogic-,,$(T7_MODULE_NAME))
PR_FMT_DEFINE_T7="-Dpr_fmt(fmt)= \"[$(PR_FMT_T7)]: \" fmt"
CFLAGS_t7.o += $(PR_FMT_DEFINE_T7)
obj-$(subst m,,$(CONFIG_AMLOGIC_COMMON_CLK_T7)) += t7-scmi-fixed.o

T5M_MODULE_NAME = amlogic-clk-soc-t5m
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T5M) += $(T5M_MODULE_NAME).o
$(T5M_MODULE_NAME)-y += t5m.o
PR_FMT_T5M = $(subst amlogic-,,$(T5M_MODULE_NAME))
PR_FMT_DEFINE_T5M="-Dpr_fmt(fmt)= \"[$(PR_FMT_T5M)]: \" fmt"
CFLAGS_t5m.o += $(PR_FMT_DEFINE_T5M)

G12A_MODULE_NAME = amlogic-clk-soc-g12a
obj-$(CONFIG_AMLOGIC_COMMON_CLK_G12A) += $(G12A_MODULE_NAME).o
$(G12A_MODULE_NAME)-y += g12a.o g12a-aoclk.o g12a-main.o
PR_FMT_G12A = $(subst amlogic-,,$(G12A_MODULE_NAME))
PR_FMT_DEFINE_G12A="-Dpr_fmt(fmt)= \"[$(PR_FMT_G12A)]: \" fmt"
CFLAGS_g12a.o += $(PR_FMT_DEFINE_G12A)
CFLAGS_g12a_ao.o += $(PR_FMT_DEFINE_G12A)
CFLAGS_g12a_main.o += $(PR_FMT_DEFINE_G12A)

S5_MODULE_NAME = amlogic-clk-soc-s5
obj-$(CONFIG_AMLOGIC_COMMON_CLK_S5) += $(S5_MODULE_NAME).o
$(S5_MODULE_NAME)-y += s5.o
PR_FMT_S5 = $(subst amlogic-,,$(S5_MODULE_NAME))
PR_FMT_DEFINE_S5="-Dpr_fmt(fmt)= \"[$(PR_FMT_S5)]: \" fmt"
CFLAGS_s5.o += $(PR_FMT_DEFINE_S5)

T5W_MODULE_NAME = amlogic-clk-soc-t5w
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T5W) += $(T5W_MODULE_NAME).o
$(T5W_MODULE_NAME)-y += t5w.o t5w-aoclk.o t5w-main.o
PR_FMT_T5W = $(subst amlogic-,,$(T5W_MODULE_NAME))
PR_FMT_DEFINE_T5W="-Dpr_fmt(fmt)= \"[$(PR_FMT_T5W)]: \" fmt"
CFLAGS_t5w.o += $(PR_FMT_DEFINE_T5W)
CFLAGS_t5w-aoclk.o += $(PR_FMT_DEFINE_T5W)
CFLAGS_t5w-main.o += $(PR_FMT_DEFINE_T5W)

T3X_MODULE_NAME = amlogic-clk-soc-t3x
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T3X) += $(T3X_MODULE_NAME).o
$(T3X_MODULE_NAME)-y += t3x.o
PR_FMT_T3X = $(subst amlogic-,,$(T3X_MODULE_NAME))
PR_FMT_DEFINE_T3X="-Dpr_fmt(fmt)= \"[$(PR_FMT_T3X)]: \" fmt"
CFLAGS_t3x.o += $(PR_FMT_DEFINE_T3X)

TXHD2_MODULE_NAME = amlogic-clk-soc-txhd2
obj-$(CONFIG_AMLOGIC_COMMON_CLK_TXHD2) += $(TXHD2_MODULE_NAME).o
$(TXHD2_MODULE_NAME)-y += txhd2.o txhd2-aoclk.o txhd2-main.o
PR_FMT_TXHD2 = $(subst amlogic-,,$(TXHD2_MODULE_NAME))
PR_FMT_DEFINE_TXHD2="-Dpr_fmt(fmt)= \"[$(PR_FMT_TXHD2)]: \" fmt"
CFLAGS_txhd2.o += $(PR_FMT_DEFINE_TXHD2)
CFLAGS_txhd2-aoclk.o += $(PR_FMT_DEFINE_TXHD2)
CFLAGS_txhd2-main.o += $(PR_FMT_DEFINE_TXHD2)

C1_MODULE_NAME = amlogic-clk-soc-c1
obj-$(CONFIG_AMLOGIC_COMMON_CLK_C1) += $(C1_MODULE_NAME).o
$(C1_MODULE_NAME)-y += c1.o
PR_FMT_C1 = $(subst amlogic-,,$(C1_MODULE_NAME))
PR_FMT_DEFINE_C1="-Dpr_fmt(fmt)= \"[$(PR_FMT_C1)]: \" fmt"
CFLAGS_c1.o += $(PR_FMT_DEFINE_C1)

S1A_MODULE_NAME = amlogic-clk-soc-s1a
obj-$(CONFIG_AMLOGIC_COMMON_CLK_S1A) += $(S1A_MODULE_NAME).o
$(S1A_MODULE_NAME)-y += s1a.o
PR_FMT_S1A = $(subst amlogic-,,$(S1A_MODULE_NAME))
PR_FMT_DEFINE_S1A="-Dpr_fmt(fmt)= \"[$(PR_FMT_S1A)]: \" fmt"
CFLAGS_s1a.o += $(PR_FMT_DEFINE_S1A)

T5D_MODULE_NAME = amlogic-clk-soc-t5d
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T5D) += $(T5D_MODULE_NAME).o
$(T5D_MODULE_NAME)-y += t5d.o t5d-aoclk.o t5d-main.o
PR_FMT_T5D = $(subst amlogic-,,$(T5D_MODULE_NAME))
PR_FMT_DEFINE_T5D="-Dpr_fmt(fmt)= \"[$(PR_FMT_T5D)]: \" fmt"
CFLAGS_t5d.o += $(PR_FMT_DEFINE_T5D)
CFLAGS_t5d-aoclk.o += $(PR_FMT_DEFINE_T5D)
CFLAGS_t5d-main.o += $(PR_FMT_DEFINE_T5D)

T6D_MODULE_NAME = amlogic-clk-soc-t6d
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T6D) += $(T6D_MODULE_NAME).o
$(T6D_MODULE_NAME)-y += t6d.o
PR_FMT_T6D = $(subst amlogic-,,$(T6D_MODULE_NAME))
PR_FMT_DEFINE_T6D="-Dpr_fmt(fmt)= \"[$(PR_FMT_T6D)]: \" fmt"
CFLAGS_t6d.o += $(PR_FMT_DEFINE_T6D)

A5_MODULE_NAME = amlogic-clk-soc-a5
obj-$(CONFIG_AMLOGIC_COMMON_CLK_A5) += $(A5_MODULE_NAME).o
$(A5_MODULE_NAME)-y += a5.o
PR_FMT_A5 = $(subst amlogic-,,$(A5_MODULE_NAME))
PR_FMT_DEFINE_A5="-Dpr_fmt(fmt)= \"[$(PR_FMT_A5)]: \" fmt"
CFLAGS_A5.o += $(PR_FMT_DEFINE_A5)

A4_MODULE_NAME = amlogic-clk-soc-a4
obj-$(CONFIG_AMLOGIC_COMMON_CLK_A4) += $(A4_MODULE_NAME).o
$(A4_MODULE_NAME)-y += a4.o
PR_FMT_A4 = $(subst amlogic-,,$(A4_MODULE_NAME))
PR_FMT_DEFINE_A4="-Dpr_fmt(fmt)= \"[$(PR_FMT_A4)]: \" fmt"
CFLAGS_A4.o += $(PR_FMT_DEFINE_A4)
TM2_MODULE_NAME = amlogic-clk-soc-tm2
obj-$(CONFIG_AMLOGIC_COMMON_CLK_TM2) += $(TM2_MODULE_NAME).o
$(TM2_MODULE_NAME)-y += tm2.o tm2-aoclk.o tm2-main.o
PR_FMT_TM2 = $(subst amlogic-,,$(TM2_MODULE_NAME))
PR_FMT_DEFINE_TM2="-Dpr_fmt(fmt)= \"[$(PR_FMT_TM2)]: \" fmt"
CFLAGS_tm2.o += $(PR_FMT_DEFINE_TM2)
CFLAGS_tm2-aoclk.o += $(PR_FMT_DEFINE_TM2)
CFLAGS_tm2-main.o += $(PR_FMT_DEFINE_TM2)

S7_MODULE_NAME = amlogic-clk-soc-s7
obj-$(CONFIG_AMLOGIC_COMMON_CLK_S7) += $(S7_MODULE_NAME).o
$(S7_MODULE_NAME)-y = s7.o
PR_FMT_S7 = $(subst amlogic-,,$(S7_MODULE_NAME))
PR_FMT_DEFINE_S7="-Dpr_fmt(fmt)= \"[$(PR_FMT_S7)]: \" fmt"
CFLAGS_s7.o += $(PR_FMT_DEFINE_S7)

S7D_MODULE_NAME = amlogic-clk-soc-s7d
obj-$(CONFIG_AMLOGIC_COMMON_CLK_S7D) += $(S7D_MODULE_NAME).o
$(S7D_MODULE_NAME)-y += s7d.o
PR_FMT_S7D = $(subst amlogic-,,$(S7D_MODULE_NAME))
PR_FMT_DEFINE_S7D="-Dpr_fmt(fmt)= \"[$(PR_FMT_S7D)]: \" fmt"
CFLAGS_s7d.o += $(PR_FMT_DEFINE_S7D)

S7_MODULE_NAME = amlogic-clk-soc-s7
obj-$(CONFIG_AMLOGIC_COMMON_CLK_S7) += $(S7_MODULE_NAME).o
$(S7_MODULE_NAME)-y += s7.o
PR_FMT_S7 = $(subst amlogic-,,$(S7_MODULE_NAME))
PR_FMT_DEFINE_S7="-Dpr_fmt(fmt)= \"[$(PR_FMT_S7)]: \" fmt"
CFLAGS_s7.o += $(PR_FMT_DEFINE_S7)

S6_MODULE_NAME = amlogic-clk-soc-s6
obj-$(CONFIG_AMLOGIC_COMMON_CLK_S6) += $(S6_MODULE_NAME).o
$(S6_MODULE_NAME)-y += s6.o
PR_FMT_S6 = $(subst amlogic-,,$(S6_MODULE_NAME))
PR_FMT_DEFINE_S6="-Dpr_fmt(fmt)= \"[$(PR_FMT_S6)]: \" fmt"
CFLAGS_s6.o += $(PR_FMT_DEFINE_S6)

T6W_MODULE_NAME = amlogic-clk-soc-t6w
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T6W) += $(T6W_MODULE_NAME).o
$(T6W_MODULE_NAME)-y += t6w.o
PR_FMT_T6W = $(subst amlogic-,,$(T6W_MODULE_NAME))
PR_FMT_DEFINE_T6W="-Dpr_fmt(fmt)= \"[$(PR_FMT_T6W)]: \" fmt"
CFLAGS_t6w.o += $(PR_FMT_DEFINE_T6W)

T6X_MODULE_NAME = amlogic-clk-soc-t6x
obj-$(CONFIG_AMLOGIC_COMMON_CLK_T6X) += $(T6X_MODULE_NAME).o
$(T6X_MODULE_NAME)-y += t6x.o
PR_FMT_T6X = $(subst amlogic-,,$(T6X_MODULE_NAME))
PR_FMT_DEFINE_T6X="-Dpr_fmt(fmt)= \"[$(PR_FMT_T6X)]: \" fmt"
CFLAGS_t6x.o += $(PR_FMT_DEFINE_T6X)

C5_MODULE_NAME = amlogic-clk-soc-c5
obj-$(CONFIG_AMLOGIC_COMMON_CLK_C5) += $(C5_MODULE_NAME).o
$(C5_MODULE_NAME)-y += c5.o c5-aoclk.o c5-main.o
PR_FMT_C5 = $(subst amlogic-,,$(C5_MODULE_NAME))
PR_FMT_DEFINE_C5="-Dpr_fmt(fmt)= \"[$(PR_FMT_C5)]: \" fmt"
CFLAGS_c5.o += $(PR_FMT_DEFINE_C5)
CFLAGS_c5-aoclk.o += $(PR_FMT_DEFINE_C5)
CFLAGS_c5-main.o += $(PR_FMT_DEFINE_C5)

A9_MODULE_NAME = amlogic-clk-soc-a9
obj-$(CONFIG_AMLOGIC_COMMON_CLK_A9) += $(A9_MODULE_NAME).o
$(A9_MODULE_NAME)-y += a9.o a9-aoclk.o a9-main.o
PR_FMT_A9 = $(subst amlogic-,,$(A9_MODULE_NAME))
PR_FMT_DEFINE_A9="-Dpr_fmt(fmt)= \"[$(PR_FMT_A9)]: \" fmt"
CFLAGS_a9.o += $(PR_FMT_DEFINE_A9)
CFLAGS_a9-aoclk.o += $(PR_FMT_DEFINE_A9)
CFLAGS_a9-main.o += $(PR_FMT_DEFINE_A9)
