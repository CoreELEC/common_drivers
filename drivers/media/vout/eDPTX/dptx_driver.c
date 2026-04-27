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
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPTX_notify.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#include <drm/amlogic/meson_connector_dev.h>
#include "dptx_common.h"
#include <linux/sched/clock.h>

#define DPTX_HPD_TIMEOUT        200
#define DPTX_DELAY_AFTER_HPD    200

void dptx_act_timing_to_vinfo(struct dptx_drv_s *dptx)
{
	dptx->vinfo.name = dptx->act_timing.name;
	dptx->vinfo.frac = 0;
	dptx->vinfo.mode = VMODE_eDP;
	dptx->vinfo.width               = dptx->act_timing.h_act;
	dptx->vinfo.height              = dptx->act_timing.v_act;
	dptx->vinfo.field_height        = dptx->act_timing.v_act;
	dptx->vinfo.aspect_ratio_num    = dptx->act_timing.h_size;
	dptx->vinfo.aspect_ratio_den    = dptx->act_timing.v_size;
	dptx->vinfo.screen_real_width   = dptx->sink.edid.display_size[0];
	dptx->vinfo.screen_real_height  = dptx->sink.edid.display_size[1];
	dptx->vinfo.sync_duration_num   = dptx->act_timing.sync_duration_num;
	dptx->vinfo.sync_duration_den   = dptx->act_timing.sync_duration_den;
	//dptx->vinfo.brr_duration = dptx->act_timings.frame_rate;
	dptx->vinfo.brr_duration = 0;
	dptx->vinfo.std_duration = dptx->act_timing.fr1000 / 1000;
	dptx->vinfo.vfreq_max    = dptx->act_timing.frame_rate_range[1];
	dptx->vinfo.vfreq_min    = dptx->act_timing.frame_rate_range[0];
	dptx->vinfo.video_clk    = dptx->act_timing.pclk;

	dptx->vinfo.htotal = dptx->act_timing.h_period;
	dptx->vinfo.vtotal = dptx->act_timing.v_period;
	dptx->vinfo.hsw    = dptx->act_timing.h_pw;
	dptx->vinfo.hbp    = dptx->act_timing.h_bp;
	dptx->vinfo.hfp    = dptx->act_timing.h_fp;
	dptx->vinfo.vsw    = dptx->act_timing.v_pw;
	dptx->vinfo.vbp    = dptx->act_timing.v_bp;
	dptx->vinfo.vfp    = dptx->act_timing.v_fp;
	dptx->vinfo.cur_enc_ppc =  1;
	dptx->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;
	dptx->vinfo.viu_mux =  VIU_MUX_ENCL | (dptx->idx << 4);
	dptx->vinfo.viu_color_fmt = (dptx->act_timing.cfmt <= DPTX_CFMT_RGB_12bit) ?
					COLOR_FMT_RGB444 : COLOR_FMT_YUV444;

	dptx->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_EDP_A + dptx->idx;

	dptx->vinfo.vpp_post_out_color_fmt = (dptx->act_timing.cfmt <= DPTX_CFMT_RGB_12bit) ? 0 : 1;
}

void dptx_HPD_trigger_set(struct dptx_drv_s *dptx, u8 en)
{
	// TODO: hpd_mask
	if (dptx->status & DPTX_STA_DRV_READY) {
		dptx_if_set_hpd_interrupt_mask(dptx, 0,
			en ? DPTX_IRQ_HPD_EVENT_MASK | DPTX_IRQ_HPD_IRQ_EVENT : 0);

		if (en) {
			dptx->status |= DPTX_STA_HPD_TRI_EN;
		} else {
			dptx->status &= ~DPTX_STA_HPD_TRI_EN;
			dptx->status &= ~DPTX_STA_HPD_HIGH;
		}
	}
}

static void dptx_config_timing_probe(struct dptx_drv_s *dptx)
{
	DPTX_DBG(dptx, "%s ok", __func__);
}

int dptx_timing_config_restore(struct dptx_drv_s *dptx)
{
	u8 port;
	int ret = 0;
	struct dptx_vmode_s *vmd_p;

	if (dptx->status & DPTX_STA_HPD_HIGH) {
		for (port = 0; port < eDPTX_MAX_PORT; port++) {
			if (!(dptx->sink.port_mask & BIT(port)))
				continue;

			if (dptx_DPCD_capability_to_link_cfg(dptx, port))
				DPTX_P_ERR(dptx, port, "DPCD capability detect ERROR");
			// TODO restore more status
			// ! DO NOT overwrite link/lane
			// dptx_link_policy_maker(dptx, port);
		}
	}

	if (dptx->panel_data.timing_cnt) {
		dptx_config_timing_probe(dptx);
	} else {
		ret = edptx_EDID_probe(dptx, 0);
		if (ret)
			vmd_p = &DPTX_SafeMode_640x480_vmode;
	}

	dptx_vmode_manage(dptx);
	if (ret) {
		vmd_p = &DPTX_SafeMode_640x480_vmode;
	} else {
		dptx->vmode_mgr.vmode_sel_idx = dptx_uboot_configs[dptx->idx].vmode_sel_idx;
		vmd_p = &dptx->vmode_mgr.vmodes[dptx->vmode_mgr.vmode_sel_idx];
	}
	dptx_vmode_apply_to_act_timing(dptx, vmd_p);
	// todo: edid not same

	dptx_act_timing_to_vinfo(dptx);

	return 0;
}

void dptx_driver_ready(struct dptx_drv_s *dptx)
{
	u8 port;

	if (!(dptx->status & (DPTX_STA_PROBE_DONE))) {
		DPTX_ERR(dptx, "sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	DPTX_PR(dptx, "driver init(ver %s)", DPTX_DRV_VERSION);
	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_phy_enable(dptx, port);
		//dptx_power_init(dptx, port, 1);
	}
	dptx_venc_enable(dptx, 0);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_transmitter_init(dptx, port);
	}

	dptx->board_data.pin_hpd_c = pinctrl_get_select(dptx->dev, "edptx_hpd");
	if (IS_ERR_OR_NULL(dptx->board_data.pin_hpd_c)) {
		DPTX_ERR(dptx, "pinctrl mux: %p, err: %d",
		       dptx->board_data.pin_hpd_c, IS_ERR(dptx->board_data.pin_hpd_c));
	}

	dptx->status |= DPTX_STA_DRV_READY;
}

void dptx_driver_panel_power_ctrl(struct dptx_drv_s *dptx, u8 en)
{
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_vcc_gpio_desc))
		dptx->board_data.edptx_vcc_gpio_desc = devm_gpiod_get(dptx->dev, "edptx_vcc",
			en ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_vcc_gpio_desc)) {
		DPTX_ERR(dptx, "regist vcc gpio: %p, err: %d",
		       dptx->board_data.edptx_vcc_gpio_desc,
		       IS_ERR(dptx->board_data.edptx_vcc_gpio_desc));
		return;
	}
	gpiod_direction_output(dptx->board_data.edptx_vcc_gpio_desc, en);
	DPTX_DBG(dptx, "panel vcc gpio: %p, %d", dptx->board_data.edptx_vcc_gpio_desc, en);
}

void edptx_drv_check_HPD(struct dptx_drv_s *dptx)
{
	u16 i = 0;
	u8 data;

	if (!(dptx->status & DPTX_STA_DRV_READY)) {
		DPTX_ERR(dptx, "sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	if (!dptx->viu_sel) {
		DPTX_DBG(dptx, "sta=0x%x, viu_sel=%x", dptx->status, dptx->viu_sel);
		return;
	}

	if (dptx->setting.user_hpd_ignore) {
		DPTX_PR(dptx, "ignore HPD");
		dptx_delay_ms(dptx->setting.user_hpd_ignore);
		dptx->status |= DPTX_STA_HPD_HIGH;
		return;
	}

	while (i < DPTX_HPD_TIMEOUT) {
		data = dptx_if_get_hpd_level(dptx, 0);
		if (data) {
			dptx->status |= DPTX_STA_HPD_HIGH;
			DPTX_PR(dptx, "HPD after %ums", i);
			return;
		}
		dptx_delay_ms(1);
		i++;
	}

	DPTX_PR(dptx, "eDP HPD is LOW");
	dptx->status &= ~DPTX_STA_HPD_HIGH;
}

void dptx_drv_start(struct dptx_drv_s *dptx)
{
	struct dptx_vmode_s *dp_vmode = NULL;
	u8 port;
	int ret;

	DPTX_PR(dptx, "driver start(ver %s)", DPTX_DRV_VERSION);

	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY | DPTX_STA_HPD_HIGH))) {
		DPTX_ERR(dptx, "sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	dptx_venc_enable(dptx, 0);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_transmitter_init(dptx, port);
		__dptx_set_phy_config(dptx, port, 1); // ?

		// Power up link
		if (dptx_if_aux_write_single(dptx, port, DPCD_SET_POWER, 0x1))
			DPTX_P_ERR(dptx, port, "DPCD SET POWER ERROR");
		dptx_delay_ms(20);

		// dptx_link_cfg_dft(dptx, port);
		// DP protocol: EDID -> DPCD, Intel: DPCD -> EDID
		if (dptx_DPCD_capability_to_link_cfg(dptx, port))
			DPTX_P_ERR(dptx, port, "DPCD capability detect ERROR");

		dptx_link_policy_maker(dptx, port);
	}

	// multi port config check;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		edptx_set_lane_config(dptx, port);
		__dptx_set_phy_config(dptx, port, 1); // ?
	}

	__dptx_link_training(dptx);

	if (dptx->panel_data.timing_cnt) {
		dptx_config_timing_probe(dptx);
	} else {
		ret = edptx_EDID_probe(dptx, 0);
		if (ret)
			dp_vmode = &DPTX_SafeMode_640x480_vmode;
	}

	dptx_vmode_manage(dptx);
	if (dptx->setting.user_vmode_sel)
		dp_vmode = dptx_get_vmode(dptx, dptx->setting.user_vmode_sel);
	if (!dp_vmode)
		dp_vmode = dptx_get_optimum_vmode(dptx);
	//if (dptx->drm_hpd_cb.callback)
	//	dptx->drm_hpd_cb.callback(dptx->drm_hpd_cb.data);
	if (!dp_vmode) {
		dp_vmode = &DPTX_SafeMode_640x480_vmode;
		DPTX_ERR(dptx, "no vmode to satisfy BW, to safemode 480P");
	}
	dptx_vmode_apply_to_act_timing(dptx, dp_vmode);
	dptx_act_timing_apply(dptx);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_set_MSA(dptx, port);
		dptx_set_content_protection(dptx, port);
	}

	dptx_venc_enable(dptx, 1);

	//dptx_if_path_reset(dptx, 0, DPTX_RESET_VENC);
	dptx_delay_ms(1);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_transmitter_output(dptx, port, 1);
	}

	dptx->status |= DPTX_STA_LINK_ON;
	DPTX_PR(dptx, "%s enable main stream[0x%x] video finished", __func__, dptx->sink.port_mask);
}

void edptx_drv_disp_on(struct dptx_drv_s *dptx)
{
	if (!(dptx->status &
	      (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY | DPTX_STA_HPD_HIGH | DPTX_STA_LINK_ON))) {
		DPTX_ERR(dptx, "sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_en_gpio_desc))
		dptx->board_data.edptx_bl_en_gpio_desc = devm_gpiod_get(dptx->dev,
			"edptx_bl_en", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_en_gpio_desc)) {
		DPTX_ERR(dptx, "regist bl en: %p, err: %d", dptx->board_data.edptx_bl_en_gpio_desc,
		       IS_ERR(dptx->board_data.edptx_bl_en_gpio_desc));
		return;
	}
	gpiod_direction_output(dptx->board_data.edptx_bl_en_gpio_desc, 1);
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_pwm_gpio_desc))
		dptx->board_data.edptx_bl_pwm_gpio_desc = devm_gpiod_get(dptx->dev,
			"edptx_bl_pwm", GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_pwm_gpio_desc)) {
		DPTX_ERR(dptx, "regist bl pwm: %p, err: %d",
		       dptx->board_data.edptx_bl_pwm_gpio_desc,
		       IS_ERR(dptx->board_data.edptx_bl_pwm_gpio_desc));
		return;
	}
	gpiod_direction_output(dptx->board_data.edptx_bl_pwm_gpio_desc, 1);
	DPTX_DBG(dptx, "panel bl pwm: %p:%d, %p:%d", dptx->board_data.edptx_bl_en_gpio_desc, 1,
						     dptx->board_data.edptx_bl_pwm_gpio_desc, 1);

	dptx_mute_set(dptx, 0);

	dptx->status |= DPTX_STA_DISP_ON;
}

void edptx_drv_disp_off(struct dptx_drv_s *dptx)
{
	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY))) {
		DPTX_ERR(dptx, "sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_en_gpio_desc))
		dptx->board_data.edptx_bl_en_gpio_desc = devm_gpiod_get(dptx->dev,
			"edptx_bl_en", GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_en_gpio_desc)) {
		DPTX_ERR(dptx, "regist bl en: %p, err: %d", dptx->board_data.edptx_bl_en_gpio_desc,
		       IS_ERR(dptx->board_data.edptx_bl_en_gpio_desc));
		return;
	}
	gpiod_direction_output(dptx->board_data.edptx_bl_en_gpio_desc, 0);
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_pwm_gpio_desc))
		dptx->board_data.edptx_bl_pwm_gpio_desc = devm_gpiod_get(dptx->dev,
			"edptx_bl_pwm", GPIOD_OUT_HIGH);
	if (IS_ERR_OR_NULL(dptx->board_data.edptx_bl_pwm_gpio_desc)) {
		DPTX_ERR(dptx, "regist bl pwm: %p, err: %d",
		       dptx->board_data.edptx_bl_pwm_gpio_desc,
		       IS_ERR(dptx->board_data.edptx_bl_pwm_gpio_desc));
		return;
	}
	gpiod_direction_output(dptx->board_data.edptx_bl_pwm_gpio_desc, 0);
	DPTX_DBG(dptx, "panel bl pwm: %p:%d, %p:%d", dptx->board_data.edptx_bl_en_gpio_desc, 0,
						     dptx->board_data.edptx_bl_pwm_gpio_desc, 0);


	dptx_mute_set(dptx, 1);

	dptx->status &= ~DPTX_STA_DISP_ON;
}

void dptx_driver_close(struct dptx_drv_s *dptx)
{
	unsigned char auxdata;
	int ret;
	struct dptx_vmode_s *dp_vmode = NULL;
	u8 port;

	if (!(dptx->status & (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY))) {
		DPTX_ERR(dptx, "sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}
	//dptx_clear_timing(dptx);
	//Power down link

	if (!(dptx->status & DPTX_STA_LINK_ON)) {
		DPTX_PR(dptx, "%s finished", __func__);
		return;
	}

	auxdata = 0x5;
	// 010b = Set all link to D3 (power-down mode).
	// 101b = Set Main-Link D3, keep AUX block fully powered
	for (port = 3; port > 0; port--) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		ret = dptx_if_aux_write(dptx, port, DPCD_SET_POWER, 1, &auxdata);
		if (ret)
			DPTX_P_DBG(dptx, port, "sink power down link failed");
		//all ch may close when first POWER_DOWN, so, no warn
	}

	DPTX_PR(dptx, "disable main stream video");

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_transmitter_output(dptx, port, 0);
		dptx_if_path_reset(dptx, port, DPTX_RESET_ALL);
	}

	if (dptx->viu_sel) {
		dp_vmode = &DPTX_SafeMode_640x480_vmode;
		dptx_vmode_apply_to_act_timing(dptx, dp_vmode);
		dptx_act_timing_apply(dptx);
	}

	// memset(&dptx->link_cfg, 0, sizeof(struct dptx_link_cfg_s));
	dptx->status &= ~DPTX_STA_LINK_ON;
	DPTX_PR(dptx, "%s finished", __func__);
}

void dptx_drv_eDP_PSR1_en(struct dptx_drv_s *dptx, u8 port_mask, u8 en)
{
	u8 port;

	if (!(dptx->status &
	      (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY | DPTX_STA_HPD_HIGH | DPTX_STA_LINK_ON))) {
		DPTX_ERR(dptx, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_eDP_PSR1(dptx, port, en);
	}
}

void dptx_drv_eDP_PSR2_en(struct dptx_drv_s *dptx, u8 port_mask, u8 en)
{
	u8 port;

	if (!(dptx->status &
	      (DPTX_STA_PROBE_DONE | DPTX_STA_DRV_READY | DPTX_STA_HPD_HIGH | DPTX_STA_LINK_ON))) {
		DPTX_ERR(dptx, "DPTX sta=0x%x, not ready for %s", dptx->status, __func__);
		return;
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_eDP_PSR2(dptx, port, en);
	}
}

void dptx_notify_check_HPD(struct dptx_drv_s *dptx)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;

	edptx_drv_check_HPD(dptx);
}

void dptx_notify_set_backlight(struct dptx_drv_s *dptx, u8 en)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;
}

void dptx_notify_set_screen_mute(struct dptx_drv_s *dptx, u8 en)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;

	if (en)
		edptx_drv_disp_off(dptx);
	else
		edptx_drv_disp_on(dptx);
}

void dptx_notify_set_HPD_trigger(struct dptx_drv_s *dptx, u8 en)
{
	dptx_HPD_trigger_set(dptx, en);
}

void dptx_notify_set_link(struct dptx_drv_s *dptx, u8 en)
{
	if (!(dptx->status & DPTX_STA_DRV_READY))
		return;

	if (!en) {
		dptx_driver_close(dptx);
		return;
	}

	if (!(dptx->status & DPTX_STA_HPD_HIGH))
		edptx_drv_check_HPD(dptx);

	if (dptx->status & DPTX_STA_HPD_HIGH)
		dptx_drv_start(dptx);
}

void dptx_notify_set_driver_ready(struct dptx_drv_s *dptx, u8 en)
{
	if (en) {
		dptx_driver_panel_power_ctrl(dptx, 1);
		dptx_driver_ready(dptx);
	} else {
		dptx_driver_close(dptx);
		dptx_driver_panel_power_ctrl(dptx, 0);
		dptx->status &= ~DPTX_STA_DRV_READY;
	}
}

void dptx_notify_set_venc(struct dptx_drv_s *dptx, u8 en)
{
}
