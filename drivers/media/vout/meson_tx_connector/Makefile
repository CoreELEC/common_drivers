# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vin/tvin \
				-I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vin/tvin/hdmirx \
				-I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/efuse_unifykey

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_TX_CONNECTOR_COMMON_MODERN) += \
							vout/meson_tx_connector/cea861.o \
							vout/meson_tx_connector/meson_tx_task_mgr_linux.o \
							vout/meson_tx_connector/meson_tx_dev.o \
							vout/meson_tx_connector/meson_tx_audio.o \
							vout/meson_tx_connector/meson_tx_hw.o \
							vout/meson_tx_connector/meson_tx_log.o \
							vout/meson_tx_connector/meson_tx_edid_parse.o \
							vout/meson_tx_connector/meson_tx_mode.o \
							vout/meson_tx_connector/meson_tx_vout.o \
							vout/meson_tx_connector/meson_tx_mode_set.o \
							vout/meson_tx_connector/meson_tx_event_mgr.o \
							vout/meson_tx_connector/phy/meson_tx_phy_core.o \
							vout/meson_tx_connector/phy/meson_tx_dp_phy_drv.o \
							vout/meson_tx_connector/phy/meson_tx_dummy_phy_drv.o \
							vout/meson_tx_connector/clk/meson_tx_clk.o \
							vout/meson_tx_connector/clk/tx_clk_a9.o

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/hdmitx/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/hdcp/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/dptx/Makefile
