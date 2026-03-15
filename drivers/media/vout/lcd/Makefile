# SPDX-License-Identifier: (GPL-2.0+ OR MIT)

define config_status
$(if $(value $1),y,n)
endef

DPI_info      = Tcon[$(call config_status,CONFIG_AMLOGIC_LCD_DPI)]
LVDS_info      = Tcon[$(call config_status,CONFIG_AMLOGIC_LCD_LVDS)]
tcon_info      = Tcon[$(call config_status,CONFIG_AMLOGIC_LCD_TCON)]
vbyone_info    = VbyOne[$(call config_status,CONFIG_AMLOGIC_LCD_VBYONE)]
dsi_info       = MIPI-DSI[$(call config_status,CONFIG_AMLOGIC_LCD_MIPI_DSI)]
# eDP_info       = eDP[$(call config_status,CONFIG_AMLOGIC_LCD_TABLET)]
connector_info = connector($(DPI_info) $(LVDS_info) $(tcon_info) $(vbyone_info) $(dsi_info))

feature_lcd_extern = Extern[$(call config_status,CONFIG_AMLOGIC_LCD_EXTERN)]
feature_ambilight  = ambilight[$(call config_status,CONFIG_AMLOGIC_AMBILIGHT)]
feature_info       = feature($(feature_lcd_extern) $(feature_ambilight))

bl         = Backlight[$(call config_status,CONFIG_AMLOGIC_BACKLIGHT)]
bl_extern  = Extern[$(call config_status,CONFIG_AMLOGIC_BL_EXTERN)]
bl_ldim    = LDIM[$(call config_status,CONFIG_AMLOGIC_BL_LDIM)]
bl_mcu     = LDIM-BLMCU[$(call config_status,CONFIG_AMLOGIC_BL_LDIM_BLMCU)]
bl_info    = $(bl)($(bl_extern) $(bl_ldim) $(bl_mcu))

$(info * Compile Amlogic LCD Module With $(connector_info), $(feature_info), $(bl_info) *)

$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_vout.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_reg.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_common.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_pinctrl.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_config.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_cus_ctrl.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_notify.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_debug.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_drm.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_dphy_ctrl.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_math.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_frame_lock.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_mode.o
$(MEDIA_MODULE_NAME)-$(CONFIG_AMLOGIC_LCD) += vout/lcd/lcd_device_mgr.o

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/lcd_resman/Makefile

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/lcd_venc/Makefile

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/lcd_clk/Makefile

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/lcd_phy/Makefile

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/lcd_extern/Makefile

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/backlight/Makefile

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/ambilight/Makefile

include $(srctree)/$(COMMON_DRIVERS_DIR)/drivers/media/vout/lcd/connectors/Makefile
