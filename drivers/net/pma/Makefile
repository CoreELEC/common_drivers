# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

MODULE_NAME = amlogic-hsg-phy
PR_FMT = $(subst amlogic-,,$(MODULE_NAME))
PR_FMT_DEFINE="-Dpr_fmt(fmt)= \"[$(PR_FMT)]: \" fmt"
CFLAGS_phy-aml-hsgmii-eth.o += $(PR_FMT_DEFINE)
obj-$(CONFIG_AMLOGIC_HSGMII_PHY) = $(MODULE_NAME).o
$(MODULE_NAME)-y += phy-aml-hsgmii-eth.o
