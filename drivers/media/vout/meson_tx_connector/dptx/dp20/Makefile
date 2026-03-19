# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/ \
		-I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/dptx/

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_DPTX) += \
	vout/meson_tx_connector/dptx/dp20/dptx_hw_aux.o \
	vout/meson_tx_connector/dptx/dp20/dptx_hw.o \
	vout/meson_tx_connector/dptx/dp20/dptx_pktmgmt.o \
	vout/meson_tx_connector/dptx/dp20/dptx_hw_timer.o \
	vout/meson_tx_connector/dptx/dp20/dptx_hw_gtc.o \
	vout/meson_tx_connector/dptx/dp20/dptx_reg.o \
	vout/meson_tx_connector/dptx/dp20/dptx_hdcp.o \
	vout/meson_tx_connector/dptx/dp20/dptx_hdcp_hw.o \
	vout/meson_tx_connector/dptx/dp20/dptx_hdcp22_interrupts.o \
	vout/meson_tx_connector/dptx/dp20/dptx_hdcp1x.o
