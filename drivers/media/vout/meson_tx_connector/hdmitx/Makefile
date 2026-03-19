# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_HDMITX_DRIVER_MODERN) += \
							vout/meson_tx_connector/hdmitx/hdmitx_boot_parameters.o \
							vout/meson_tx_connector/hdmitx/hdmitx_common.o \
							vout/meson_tx_connector/hdmitx/hdmitx_sysfs_common.o \
							vout/meson_tx_connector/hdmitx/hdmitx_hw_common.o \
							vout/meson_tx_connector/hdmitx/hdmitx_hdr.o \
							vout/meson_tx_connector/hdmitx/hdmitx_format_para.o \
							vout/meson_tx_connector/hdmitx/hdmitx_mode_set.o \
							vout/meson_tx_connector/hdmitx/hdmitx_vout.o \
							vout/meson_tx_connector/hdmitx/hdmitx_audio.o \
							vout/meson_tx_connector/hdmitx/hdmitx_log.o \
							vout/meson_tx_connector/hdmitx/hdmitx_compliance.o \
							vout/meson_tx_connector/hdmitx/hdmitx_check_valid.o \
							vout/meson_tx_connector/hdmitx/hdmitx_hdcp.o \
							vout/meson_tx_connector/hdmitx/hdmitx_infoframe.o \
							vout/meson_tx_connector/hdmitx/hdmitx_dump.o \
							vout/meson_tx_connector/hdmitx/hdmitx_ext.o \
							vout/meson_tx_connector/hdmitx/hdmitx_drv.o \

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/hdmitx/tx20/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/hdmitx/tx21/Makefile
