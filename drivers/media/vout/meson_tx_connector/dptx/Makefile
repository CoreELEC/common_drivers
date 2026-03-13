# SPDX-License-Identifier: (GPL-2.0+ OR MIT)


ccflags-y += -I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/ \
		-I$(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/dptx/

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_DPTX) += \
						vout/meson_tx_connector/dptx/dptx_aux_helper.o \
						vout/meson_tx_connector/dptx/dptx_check_valid.o \
						vout/meson_tx_connector/dptx/dptx_common.o \
						vout/meson_tx_connector/dptx/dptx_drv.o \
						vout/meson_tx_connector/dptx/dptx_hw_common.o \
						vout/meson_tx_connector/dptx/dptx_infoframe.o \
						vout/meson_tx_connector/dptx/dptx_link.o \
						vout/meson_tx_connector/dptx/dptx_audio.o \
						vout/meson_tx_connector/dptx/dptx_timer.o \
						vout/meson_tx_connector/dptx/dptx_gtc.o \
						vout/meson_tx_connector/dptx/dptx_log.o \
						vout/meson_tx_connector/dptx/dptx_sysfs.o \
						vout/meson_tx_connector/dptx/dptx_vout.o


include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/meson_tx_connector/dptx/dp20/Makefile
