# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_scdc.o
ifndef CONFIG_AMLOGIC_ZAPPER_CUT
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_vrr.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_frl.o
endif

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_pktmgmt.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_reg_ops.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_enc_cfg_hw.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_ddc.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_clk.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_dump.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_t7.o
ifndef CONFIG_AMLOGIC_ZAPPER_CUT
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_s5.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_s7.o
endif
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_s1a.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_s7.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_s7d.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_s6.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_a9.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_interrupts.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX21) += vout/hdmi_tx_connector/hdmitx/tx21/hdmitx_frl_training.o
