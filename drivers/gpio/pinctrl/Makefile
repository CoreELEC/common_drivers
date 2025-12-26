# SPDX-License-Identifier: GPL-2.0-only

ccflags-y += -I$(srctree)/drivers

$(MODULE_NAME)-$(CONFIG_AMLOGIC_PINCTRL_MESON) += pinctrl/pinctrl-meson.o
$(MODULE_NAME)-$(CONFIG_AMLOGIC_PINCTRL_MESON) += pinctrl/pinctrl-meson-axg-pmx.o

ccflags-y += -I$(srctree)/drivers/pinctrl/meson

MODULE_NAME_A4 = amlogic-pinctrl-a4

obj-$(CONFIG_AMLOGIC_PINCTRL_A4) += $(MODULE_NAME_A4).o
$(MODULE_NAME_A4)-y += pinctrl/pinctrl-amlogic-a4.o
PR_FMT_DEFINE_NA4="-Dpr_fmt(fmt)= \"[$(MODULE_NAME_A4)]: \" fmt"
CFLAGS_pinctrl-amlogic-a4.o += $(PR_FMT_DEFINE_NA4)

S4_MODULE_NAME = amlogic-pinctrl-soc-s4
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_S4) += $(S4_MODULE_NAME).o
$(S4_MODULE_NAME)-y += pinctrl/pinctrl-meson-s4.o
PR_FMT_S4 = $(subst amlogic-,,$(S4_MODULE_NAME))
PR_FMT_DEFINE_S4="-Dpr_fmt(fmt)= \"[$(PR_FMT_S4)]: \" fmt"
CFLAGS_pinctrl-meson-s4.o += $(PR_FMT_DEFINE_S4)

C2_MODULE_NAME = amlogic-pinctrl-soc-c2
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_C2) += $(C2_MODULE_NAME).o
$(C2_MODULE_NAME)-y += pinctrl/pinctrl-meson-c2.o
PR_FMT_C2 = $(subst amlogic-,,$(C2_MODULE_NAME))
PR_FMT_DEFINE_C2="-Dpr_fmt(fmt)= \"[$(PR_FMT_C2)]: \" fmt"
CFLAGS_pinctrl-meson-c2.o += $(PR_FMT_DEFINE_C2)

C3_MODULE_NAME = amlogic-pinctrl-soc-c3
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_C3) += $(C3_MODULE_NAME).o
$(C3_MODULE_NAME)-y += pinctrl/pinctrl-meson-c3.o
PR_FMT_C3 = $(subst amlogic-,,$(C3_MODULE_NAME))
PR_FMT_DEFINE_C3="-Dpr_fmt(fmt)= \"[$(PR_FMT_C3)]: \" fmt"
CFLAGS_pinctrl-meson-c3.o += $(PR_FMT_DEFINE_C3)

A1_MODULE_NAME = amlogic-pinctrl-soc-a1
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_A1) += $(A1_MODULE_NAME).o
$(A1_MODULE_NAME)-y += pinctrl/pinctrl-meson-a1.o
PR_FMT_A1 = $(subst amlogic-,,$(A1_MODULE_NAME))
PR_FMT_DEFINE_A1="-Dpr_fmt(fmt)= \"[$(PR_FMT_A1)]: \" fmt"
CFLAGS_pinctrl-meson-a1.o += $(PR_FMT_DEFINE_A1)

SC2_MODULE_NAME = amlogic-pinctrl-soc-sc2
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_SC2) += $(SC2_MODULE_NAME).o
$(SC2_MODULE_NAME)-y += pinctrl/pinctrl-meson-sc2.o
PR_FMT_SC2 = $(subst amlogic-,,$(SC2_MODULE_NAME))
PR_FMT_DEFINE_SC2="-Dpr_fmt(fmt)= \"[$(PR_FMT_SC2)]: \" fmt"
CFLAGS_pinctrl-meson-sc2.o += $(PR_FMT_DEFINE_SC2)

T3_MODULE_NAME = amlogic-pinctrl-soc-t3
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T3) += $(T3_MODULE_NAME).o
$(T3_MODULE_NAME)-y += pinctrl/pinctrl-meson-t3.o
PR_FMT_T3 = $(subst amlogic-,,$(T3_MODULE_NAME))
PR_FMT_DEFINE_T3="-Dpr_fmt(fmt)= \"[$(PR_FMT_T3)]: \" fmt"
CFLAGS_pinctrl-meson-t3.o += $(PR_FMT_DEFINE_T3)

T7_MODULE_NAME = amlogic-pinctrl-soc-t7
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T7) += $(T7_MODULE_NAME).o
$(T7_MODULE_NAME)-y += pinctrl/pinctrl-meson-t7.o
PR_FMT_T7 = $(subst amlogic-,,$(T7_MODULE_NAME))
PR_FMT_DEFINE_T7="-Dpr_fmt(fmt)= \"[$(PR_FMT_T7)]: \" fmt"
CFLAGS_pinctrl-meson-t7.o += $(PR_FMT_DEFINE_T7)

T5M_MODULE_NAME = amlogic-pinctrl-soc-t5m
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T5M) += $(T5M_MODULE_NAME).o
$(T5M_MODULE_NAME)-y += pinctrl/pinctrl-meson-t5m.o
PR_FMT_T5M = $(subst amlogic-,,$(T5M_MODULE_NAME))
PR_FMT_DEFINE_T5M="-Dpr_fmt(fmt)= \"[$(PR_FMT_T5M)]: \" fmt"
CFLAGS_pinctrl-meson-t5m.o += $(PR_FMT_DEFINE_T5M)

G12A_MODULE_NAME = amlogic-pinctrl-soc-g12a
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_G12A) += $(G12A_MODULE_NAME).o
$(G12A_MODULE_NAME)-y += pinctrl/pinctrl-meson-g12a.o
PR_FMT_G12A = $(subst amlogic-,,$(G12A_MODULE_NAME))
PR_FMT_DEFINE_G12A="-Dpr_fmt(fmt)= \"[$(PR_FMT_G12A)]: \" fmt"
CFLAGS_pinctrl-meson-g12a.o += $(PR_FMT_DEFINE_G12A)

S5_MODULE_NAME = amlogic-pinctrl-soc-s5
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_S5) += $(S5_MODULE_NAME).o
$(S5_MODULE_NAME)-y += pinctrl/pinctrl-meson-s5.o
PR_FMT_S5 = $(subst amlogic-,,$(S5_MODULE_NAME))
PR_FMT_DEFINE_S5="-Dpr_fmt(fmt)= \"[$(PR_FMT_S5)]: \" fmt"
CFLAGS_pinctrl-meson-s5.o += $(PR_FMT_DEFINE_S5)

T5W_MODULE_NAME = amlogic-pinctrl-soc-t5w
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T5W) += $(T5W_MODULE_NAME).o
$(T5W_MODULE_NAME)-y += pinctrl/pinctrl-meson-t5w.o
PR_FMT_T5W = $(subst amlogic-,,$(T5W_MODULE_NAME))
PR_FMT_DEFINE_T5W="-Dpr_fmt(fmt)= \"[$(PR_FMT_T5W)]: \" fmt"
CFLAGS_pinctrl-meson-t5w.o += $(PR_FMT_DEFINE_T5W)

T3X_MODULE_NAME = amlogic-pinctrl-soc-t3x
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T3X) += $(T3X_MODULE_NAME).o
$(T3X_MODULE_NAME)-y += pinctrl/pinctrl-meson-t3x.o
PR_FMT_T3X = $(subst amlogic-,,$(T3X_MODULE_NAME))
PR_FMT_DEFINE_T3X="-Dpr_fmt(fmt)= \"[$(PR_FMT_T3X)]: \" fmt"
CFLAGS_pinctrl-meson-t3x.o += $(PR_FMT_DEFINE_T3X)

TXHD2_MODULE_NAME = amlogic-pinctrl-soc-txhd2
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_TXHD2) += $(TXHD2_MODULE_NAME).o
$(TXHD2_MODULE_NAME)-y += pinctrl/pinctrl-meson-txhd2.o
PR_FMT_TXHD2 = $(subst amlogic-,,$(TXHD2_MODULE_NAME))
PR_FMT_DEFINE_TXHD2="-Dpr_fmt(fmt)= \"[$(PR_FMT_TXHD2)]: \" fmt"
CFLAGS_pinctrl-meson-txhd2.o += $(PR_FMT_DEFINE_TXHD2)

C1_MODULE_NAME = amlogic-pinctrl-soc-c1
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_C1) += $(C1_MODULE_NAME).o
$(C1_MODULE_NAME)-y += pinctrl/pinctrl-meson-c1.o
PR_FMT_C1 = $(subst amlogic-,,$(C1_MODULE_NAME))
PR_FMT_DEFINE_C1="-Dpr_fmt(fmt)= \"[$(PR_FMT_C1)]: \" fmt"
CFLAGS_pinctrl-meson-c1.o += $(PR_FMT_DEFINE_C1)

S1A_MODULE_NAME = amlogic-pinctrl-soc-s1a
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_S1A) += $(S1A_MODULE_NAME).o
$(S1A_MODULE_NAME)-y += pinctrl/pinctrl-meson-s1a.o
PR_FMT_S1A = $(subst amlogic-,,$(S1A_MODULE_NAME))
PR_FMT_DEFINE_S1A="-Dpr_fmt(fmt)= \"[$(PR_FMT_S1A)]: \" fmt"
CFLAGS_pinctrl-meson-s1a.o += $(PR_FMT_DEFINE_S1A)

T5D_MODULE_NAME = amlogic-pinctrl-soc-t5d
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T5D) += $(T5D_MODULE_NAME).o
$(T5D_MODULE_NAME)-y += pinctrl/pinctrl-meson-t5d.o
PR_FMT_T5D = $(subst amlogic-,,$(T5D_MODULE_NAME))
PR_FMT_DEFINE_T5D="-Dpr_fmt(fmt)= \"[$(PR_FMT_T5D)]: \" fmt"
CFLAGS_pinctrl-meson-t5d.o += $(PR_FMT_DEFINE_T5D)

A4_MODULE_NAME = amlogic-pinctrl-soc-a4
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_A4) += $(A4_MODULE_NAME).o
$(A4_MODULE_NAME)-y += pinctrl/pinctrl-meson-a4.o
PR_FMT_A4 = $(subst amlogic-,,$(A4_MODULE_NAME))
PR_FMT_DEFINE_A4="-Dpr_fmt(fmt)= \"[$(PR_FMT_A4)]: \" fmt"
CFLAGS_pinctrl-meson-a4.o += $(PR_FMT_DEFINE_A4)

A5_MODULE_NAME = amlogic-pinctrl-soc-a5
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_A5) += $(A5_MODULE_NAME).o
$(A5_MODULE_NAME)-y += pinctrl/pinctrl-meson-a5.o
PR_FMT_A5 = $(subst amlogic-,,$(A5_MODULE_NAME))
PR_FMT_DEFINE_A5="-Dpr_fmt(fmt)= \"[$(PR_FMT_A5)]: \" fmt"
CFLAGS_pinctrl-meson-a5.o += $(PR_FMT_DEFINE_A5)

TM2_MODULE_NAME = amlogic-pinctrl-soc-tm2
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_TM2) += $(TM2_MODULE_NAME).o
$(TM2_MODULE_NAME)-y += pinctrl/pinctrl-meson-tm2.o
PR_FMT_TM2 = $(subst amlogic-,,$(TM2_MODULE_NAME))
PR_FMT_DEFINE_TM2="-Dpr_fmt(fmt)= \"[$(PR_FMT_TM2)]: \" fmt"
CFLAGS_pinctrl-meson-tm2.o += $(PR_FMT_DEFINE_TM2)

S7_MODULE_NAME = amlogic-pinctrl-soc-s7
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_S7) += $(S7_MODULE_NAME).o
$(S7_MODULE_NAME)-y += pinctrl/pinctrl-meson-s7.o
PR_FMT_S7 = $(subst amlogic-,,$(S7_MODULE_NAME))
PR_FMT_DEFINE_S7="-Dpr_fmt(fmt)= \"[$(PR_FMT_S7)]: \" fmt"
CFLAGS_pinctrl-meson-s7.o += $(PR_FMT_DEFINE_S7)

S7D_MODULE_NAME = amlogic-pinctrl-soc-s7d
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_S7D) += $(S7D_MODULE_NAME).o
$(S7D_MODULE_NAME)-y += pinctrl/pinctrl-meson-s7d.o
PR_FMT_S7D = $(subst amlogic-,,$(S7D_MODULE_NAME))
PR_FMT_DEFINE_S7D="-Dpr_fmt(fmt)= \"[$(PR_FMT_S7D)]: \" fmt"
CFLAGS_pinctrl-meson-s7d.o += $(PR_FMT_DEFINE_S7D)

S6_MODULE_NAME = amlogic-pinctrl-soc-s6
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_S6) += $(S6_MODULE_NAME).o
$(S6_MODULE_NAME)-y += pinctrl/pinctrl-meson-s6.o
PR_FMT_S6 = $(subst amlogic-,,$(S6_MODULE_NAME))
PR_FMT_DEFINE_S6="-Dpr_fmt(fmt)= \"[$(PR_FMT_S6)]: \" fmt"
CFLAGS_pinctrl-meson-s6.o += $(PR_FMT_DEFINE_S6)

T6D_MODULE_NAME = amlogic-pinctrl-soc-t6d
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T6D) += $(T6D_MODULE_NAME).o
$(T6D_MODULE_NAME)-y += pinctrl/pinctrl-meson-t6d.o
PR_FMT_T6D = $(subst amlogic-,,$(T6D_MODULE_NAME))
PR_FMT_DEFINE_T6D="-Dpr_fmt(fmt)= \"[$(PR_FMT_T6D)]: \" fmt"
CFLAGS_pinctrl-meson-t6d.o += $(PR_FMT_DEFINE_T6D)

T6W_MODULE_NAME = amlogic-pinctrl-soc-t6w
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T6W) += $(T6W_MODULE_NAME).o
$(T6W_MODULE_NAME)-y += pinctrl/pinctrl-meson-t6w.o
PR_FMT_T6W = $(subst amlogic-,,$(T6W_MODULE_NAME))
PR_FMT_DEFINE_T6W="-Dpr_fmt(fmt)= \"[$(PR_FMT_T6W)]: \" fmt"
CFLAGS_pinctrl-meson-t6w.o += $(PR_FMT_DEFINE_T6W)

T6X_MODULE_NAME = amlogic-pinctrl-soc-t6x
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_T6X) += $(T6X_MODULE_NAME).o
$(T6X_MODULE_NAME)-y += pinctrl/pinctrl-meson-t6x.o
PR_FMT_T6X = $(subst amlogic-,,$(T6X_MODULE_NAME))
PR_FMT_DEFINE_T6X="-Dpr_fmt(fmt)= \"[$(PR_FMT_T6X)]: \" fmt"
CFLAGS_pinctrl-meson-t6x.o += $(PR_FMT_DEFINE_T6X)

C4_MODULE_NAME = amlogic-pinctrl-soc-c4
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_C4) += $(C4_MODULE_NAME).o
$(C4_MODULE_NAME)-y += pinctrl/pinctrl-meson-c4.o
PR_FMT_C4 = $(subst amlogic-,,$(C4_MODULE_NAME))
PR_FMT_DEFINE_C4="-Dpr_fmt(fmt)= \"[$(PR_FMT_C4)]: \" fmt"
CFLAGS_pinctrl-meson-c4.o += $(PR_FMT_DEFINE_C4)

A9_MODULE_NAME = amlogic-pinctrl-soc-a9
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_A9) += $(A9_MODULE_NAME).o
$(A9_MODULE_NAME)-y += pinctrl/pinctrl-meson-a9.o
PR_FMT_A9 = $(subst amlogic-,,$(A9_MODULE_NAME))
PR_FMT_DEFINE_A9="-Dpr_fmt(fmt)= \"[$(PR_FMT_A9)]: \" fmt"
CFLAGS_pinctrl-meson-a9.o += $(PR_FMT_DEFINE_A9)

C5_MODULE_NAME = amlogic-pinctrl-soc-c5
obj-$(CONFIG_AMLOGIC_PINCTRL_MESON_C5) += $(C5_MODULE_NAME).o
$(C5_MODULE_NAME)-y += pinctrl/pinctrl-meson-c5.o
PR_FMT_C5 = $(subst amlogic-,,$(C5_MODULE_NAME))
PR_FMT_DEFINE_C5="-Dpr_fmt(fmt)= \"[$(PR_FMT_C5)]: \" fmt"
CFLAGS_pinctrl-meson-c5.o += $(PR_FMT_DEFINE_C5)
