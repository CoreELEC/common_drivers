// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "../lcd_common.h"
#include <linux/sched/clock.h>
#include "./lcd_connector.h"

static int lcd_type_supported(struct aml_lcd_drv_s *pdrv)
{
	int lcd_type = pdrv->curr_dev->dev_cfg.basic.lcd_type;
	int ret = -1;

	switch (lcd_type) {
	case LCD_LVDS:
	case LCD_RGB:
	case LCD_BT656:
	case LCD_BT1120:
		return 0;
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		return 0;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
	case LCD_P2P:
		return 0;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		return 0;
#endif
	default:
		LCD_ERR(pdrv, "invalid lcd type: %s(%d)",
		       lcd_type_type_to_str(lcd_type), lcd_type);
		break;
	}
	return ret;
}

void lcd_connector_driver_init_pre(struct aml_lcd_drv_s *pdrv)
{
	LCD_PR(pdrv, "connector driver init(ver %s): %s", LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->curr_dev->dev_cfg.basic.lcd_type));

	if (lcd_type_supported(pdrv))
		return;

	//if (pdrv->mode == LCD_MODE_TABLET)
	//	lcd_frame_rate_change(pdrv);

#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_on(pdrv->lcd_vpu_dev);
#endif
	lcd_clk_gate_switch(pdrv, 1);

	/* init driver */
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_interrupt_enable(pdrv, 0);
		break;
#endif
	default:
		break;
	}

	lcd_set_clk(pdrv);
	lcd_set_venc(pdrv);

	LCD_DBG(pdrv, "%s finished\n", __func__);
}

void lcd_connector_driver_disable_post(struct aml_lcd_drv_s *pdrv)
{
	lcd_venc_enable(pdrv, 0);

	lcd_disable_clk(pdrv);
	lcd_clk_gate_switch(pdrv, 0);
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(pdrv->lcd_vpu_dev);
	vpu_dev_clk_release(pdrv->lcd_vpu_dev);
#endif

	LCD_DBG(pdrv, "%s finished", __func__);
}

void lcd_connector_driver_change(struct aml_lcd_drv_s *pdrv)
{
	unsigned long long local_time[2];

	if (!pdrv->probe_done)
		LCD_PR(pdrv, "config not loaded, bypass %s", __func__);

	local_time[0] = sched_clock();

	LCD_PR(pdrv, "connector driver change(ver %s): %s", LCD_DRV_VERSION,
	      lcd_type_type_to_str(pdrv->curr_dev->dev_cfg.basic.lcd_type));

	if (lcd_type_supported(pdrv))
		return;

	if (pdrv->status & LCD_STATUS_IF_ON) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
		if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_VBYONE)
			lcd_vbyone_interrupt_enable(pdrv, 0);
#endif
	}

	if (!(pdrv->status & LCD_STATUS_ENCL_DUMMY)) {
		lcd_clk_change(pdrv);
		lcd_venc_change(pdrv);
	}

	if (pdrv->status & LCD_STATUS_IF_ON) {
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
		if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_VBYONE) {
			lcd_vbyone_wait_stable(pdrv);
			lcd_vbyone_interrupt_enable(pdrv, 1);
		}
#endif
	}

	LCD_DBG(pdrv, "%s finished", __func__);
	local_time[1] = sched_clock();
	pdrv->proc_time.driver_change_time = lcd_do_div(local_time[1] - local_time[0], 1000);
}

void lcd_connector_driver_init(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_type_supported(pdrv))
		return;

	// lcd_power_screen_black(pdrv);
	// vpp power off when suspend, need cpu write mute
	if (pdrv->viu_sel == 1 && pdrv->data->chip_type == LCD_CHIP_S6) {
		LCD_PR(pdrv, "%s black", __func__);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		// lcd_post2_matrix();
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
		// mute_output_vcbus();
#endif
	}

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_RGB:
		lcd_rgb_control_set(pdrv, 1);
		lcd_rgb_pinmux_set(pdrv, 1);
		break;
	case LCD_BT656:
	case LCD_BT1120:
		lcd_bt_control_set(pdrv, 1);
		lcd_bt_pinmux_set(pdrv, 1);
		break;
	case LCD_LVDS:
		lcd_lvds_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_UP);
		lcd_lvds_dphy_set(pdrv, 1);
		lcd_lvds_enable(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		break;
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_pinmux_set(pdrv, 1);
		lcd_vbyone_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_UP);
		lcd_vbyone_dphy_set(pdrv, 1);
		lcd_vbyone_enable(pdrv);
		lcd_vbyone_wait_hpd(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_vbyone_power_on_wait_stable(pdrv);
		lcd_vbyone_interrupt_enable(pdrv, 1);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
		lcd_tcon_top_init(pdrv);
		lcd_mlvds_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_UP);
		lcd_mlvds_dphy_set(pdrv, 1);
		lcd_tcon_enable(pdrv);
		lcd_mlvds_pinmux_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		break;
	case LCD_P2P:
		lcd_tcon_top_init(pdrv);
		lcd_p2p_pinmux_set(pdrv, 1);
		lcd_p2p_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_UP);
		lcd_p2p_dphy_set(pdrv, 1);
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_tcon_enable(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		lcd_phy_set(pdrv, LCD_PHY_ON);
		lcd_mipi_dphy_set(pdrv, 1);
		lcd_dsi_tx_ctrl(pdrv, 1);
		break;
#endif
	default:
		break;
	}

	LCD_DBG(pdrv, "%s finished", __func__);

	if (pdrv->fr_lock)
		pdrv->fr_lock->rst = 1;
}

void lcd_connector_driver_disable(struct aml_lcd_drv_s *pdrv)
{
	LCD_PR(pdrv, "disable driver");

	if (lcd_type_supported(pdrv))
		return;

	if (pdrv->fr_lock)
		pdrv->fr_lock->rst = 1;

	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
	case LCD_RGB:
		lcd_rgb_pinmux_set(pdrv, 0);
		break;
	case LCD_BT656:
	case LCD_BT1120:
		lcd_bt_pinmux_set(pdrv, 0);
		break;
	case LCD_LVDS:
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_lvds_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_DOWN);
		lcd_lvds_disable(pdrv);
		break;
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_interrupt_enable(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_vbyone_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_DOWN);
		lcd_vbyone_pinmux_set(pdrv, 0);
		lcd_vbyone_disable(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		lcd_dsi_tx_ctrl(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_mipi_dphy_set(pdrv, 0);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
		lcd_tcon_disable(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_mlvds_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_DOWN);
		lcd_mlvds_pinmux_set(pdrv, 0);
		break;
	case LCD_P2P:
		lcd_tcon_disable(pdrv);
		lcd_phy_set(pdrv, LCD_PHY_OFF);
		lcd_p2p_dphy_set(pdrv, 0);
		lcd_phy_set(pdrv, LCD_PHY_PWR_DOWN);
		lcd_p2p_pinmux_set(pdrv, 0);
		break;
#endif
	default:
		break;
	}

	LCD_DBG(pdrv, "%s finished", __func__);
}

/* sync_duration is real_value * 100 */
void lcd_connector_frame_rate_adjust(struct aml_lcd_drv_s *pdrv, int duration)
{
	struct lcd_config_s *pconf;

	if (!pdrv || !pdrv->curr_dev)
		return;
	pconf = &pdrv->curr_dev->dev_cfg;

	LCD_PR(pdrv, "%s: sync_duration=%d", __func__, duration);

	lcd_vout_notify_mode_change_pre(pdrv);

	/* update target frame rate */
	pconf->timing.frame_rate = duration / 100;
	pconf->timing.sync_duration_num = duration;
	pconf->timing.sync_duration_den = 100;
	pconf->timing.frac = lcd_fr_is_frac(pdrv, pconf->timing.act_timing.frame_rate);

	/* update interface timing */
	lcd_frame_rate_change(pdrv);
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	if (pconf->basic.lcd_type == LCD_VBYONE)
		lcd_vbyone_interrupt_enable(pdrv, 0);
#endif

	/* change clk parameter */
	lcd_clk_change(pdrv);
	lcd_venc_change(pdrv);

#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	if (pconf->basic.lcd_type == LCD_VBYONE) {
		lcd_vbyone_wait_stable(pdrv);
		lcd_vbyone_interrupt_enable(pdrv, 1);
	}
#endif

	lcd_vout_notify_mode_change(pdrv);
}

void lcd_connector_config_probe(struct aml_lcd_drv_s *pdrv)
{
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
	case LCD_P2P:
		lcd_tcon_probe(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		lcd_vbyone_probe(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		// although config should in lcd_config.c by device,
		// dsi init table is too large and keep symmetrical with connector_config_remove
		// it is acceptable to contain a few kB fot each dsi lcd-device;
		//lcd_mipi_dsi_init_table_detect(pdrv);
		break;
#endif
	default:
		break;
	}
}

void lcd_connector_config_remove(struct aml_lcd_drv_s *pdrv)
{
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
	case LCD_P2P:
		lcd_tcon_remove(pdrv);
		lcd_swpdf_deinit(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		lcd_mipi_dsi_init_table_free(pdrv, pdrv->curr_dev);
		break;
#endif
	default:
		break;
	}
}
