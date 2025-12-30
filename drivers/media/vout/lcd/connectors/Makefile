# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD_LVDS)   += vout/lcd/connectors/lvds.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD_DPI)    += vout/lcd/connectors/DPI.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD_VBYONE) += vout/lcd/connectors/vbyone.o

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD)        += vout/lcd/connectors/lcd_connector_common.o

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/connectors/MIPI_DSI/Makefile
include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/connectors/tcon/Makefile
