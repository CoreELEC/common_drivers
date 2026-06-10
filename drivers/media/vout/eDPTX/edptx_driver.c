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
#include <linux/amlogic/media/vout/eDPTX/eDPTX_notify.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#ifdef CONFIG_AMLOGIC_BACKLIGHT
#include <linux/amlogic/media/vout/backlight/backlight.h>
#endif
#include <drm/amlogic/meson_connector_dev.h>
#include "edptx_common.h"
#include <linux/sched/clock.h>

#define eDPTX_HPD_TIMEOUT_MS        200
#define DPTX_DELAY_AFTER_HPD    200

void edptx_act_timing_to_vinfo(struct edptx_drv_s *edp)
{
	edp->vinfo.name = edp->act_timing.name;
	edp->vinfo.frac = 0;
	edp->vinfo.mode = VMODE_eDP;
	edp->vinfo.width               = edp->act_timing.h_act;
	edp->vinfo.height              = edp->act_timing.v_act;
	edp->vinfo.field_height        = edp->act_timing.v_act;
	edp->vinfo.aspect_ratio_num    = edp->act_timing.h_size;
	edp->vinfo.aspect_ratio_den    = edp->act_timing.v_size;
	edp->vinfo.screen_real_width   = edp->sink.edid.display_size[0];
	edp->vinfo.screen_real_height  = edp->sink.edid.display_size[1];
	edp->vinfo.sync_duration_num   = edp->act_timing.sync_duration_num;
	edp->vinfo.sync_duration_den   = edp->act_timing.sync_duration_den;
	//edp->vinfo.brr_duration = edp->act_timings.frame_rate;
	edp->vinfo.brr_duration = 0;
	edp->vinfo.std_duration = edp->act_timing.fr1000 / 1000;
	edp->vinfo.vfreq_max    = edp->act_timing.frame_rate_range[1];
	edp->vinfo.vfreq_min    = edp->act_timing.frame_rate_range[0];
	edp->vinfo.video_clk    = edp->act_timing.pclk;

	edp->vinfo.htotal = edp->act_timing.h_period;
	edp->vinfo.vtotal = edp->act_timing.v_period;
	edp->vinfo.hsw    = edp->act_timing.h_pw;
	edp->vinfo.hbp    = edp->act_timing.h_bp;
	edp->vinfo.hfp    = edp->act_timing.h_fp;
	edp->vinfo.vsw    = edp->act_timing.v_pw;
	edp->vinfo.vbp    = edp->act_timing.v_bp;
	edp->vinfo.vfp    = edp->act_timing.v_fp;
	edp->vinfo.cur_enc_ppc =  1;
	edp->vinfo.fr_adj_type = VOUT_FR_ADJ_NONE;
	edp->vinfo.viu_mux =  VIU_MUX_ENCL | (edp->idx << 4);
	edp->vinfo.viu_color_fmt = (edp->act_timing.cfmt <= DPTX_CFMT_RGB_12bit) ?
					COLOR_FMT_RGB444 : COLOR_FMT_YUV444;

	edp->vinfo.connector_type = DRM_MODE_CONNECTOR_MESON_EDP_A + edp->idx;

	edp->vinfo.vpp_post_out_color_fmt = (edp->act_timing.cfmt <= DPTX_CFMT_RGB_12bit) ? 0 : 1;
}

void edptx_HPD_trigger_set(struct edptx_drv_s *edp, u8 en)
{
	// TODO: hpd_mask
	if (edp->status & eDPTX_STA_DRV_READY) {
		edptx_if_set_hpd_interrupt_mask(edp, 0,
			en ? DPTX_IRQ_HPD_EVENT_MASK | DPTX_IRQ_HPD_IRQ_EVENT : 0);

		if (en) {
			edp->status |= eDPTX_STA_HPD_TRI_EN;
		} else {
			edp->status &= ~eDPTX_STA_HPD_TRI_EN;
			edp->status &= ~eDPTX_STA_HPD_HIGH;
		}
	}
}

static void edptx_config_timing_probe(struct edptx_drv_s *edp)
{
	eDP_DBG(edp, "%s ok", __func__);
}

int edptx_timing_config_restore(struct edptx_drv_s *edp)
{
	u8 port;
	int ret = 0;
	struct dptx_vmode_s *vmd_p;

	if (edp->status & eDPTX_STA_HPD_HIGH) {
		for (port = 0; port < eDPTX_MAX_PORT; port++) {
			if (!(edp->sink.port_mask & BIT(port)))
				continue;

			if (edptx_DPCD_capability_to_link_cfg(edp, port))
				eDP_P_ERR(edp, port, "DPCD capability detect ERROR");
			// TODO restore more status
			// ! DO NOT overwrite link/lane
			// dptx_link_policy_maker(edp, port);
		}
	}

	if (edp->panel_data.timing_cnt) {
		edptx_config_timing_probe(edp);
	} else {
		ret = edptx_EDID_probe(edp, 0);
		if (ret)
			vmd_p = &DPTX_SafeMode_640x480_vmode;
	}

	edptx_vmode_manage(edp);
	if (ret) {
		vmd_p = &DPTX_SafeMode_640x480_vmode;
	} else {
		edp->vmode_mgr.vmode_sel_idx = dptx_uboot_configs[edp->idx].vmode_sel_idx;
		vmd_p = &edp->vmode_mgr.vmodes[edp->vmode_mgr.vmode_sel_idx];
	}
	edptx_vmode_apply_to_act_timing(edp, vmd_p);
	// todo: edid not same

	edptx_act_timing_to_vinfo(edp);

	return 0;
}

void edptx_driver_ready(struct edptx_drv_s *edp)
{
	u8 port;

	if (!(edp->status & (eDPTX_STA_PROBE_DONE))) {
		eDP_ERR(edp, "sta=0x%x, not ready for %s", edp->status, __func__);
		return;
	}

	eDP_PR(edp, "driver init(ver %s)", DPTX_DRV_VERSION);
	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_phy_enable(edp, port);
		//dptx_power_init(edp, port, 1);
	}
	edptx_venc_enable(edp, 0);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_if_transmitter_init(edp, port);
	}

	edptx_pinmux_set(edp, 1);

	edp->status |= eDPTX_STA_DRV_READY;
}

void edptx_driver_panel_power_ctrl(struct edptx_drv_s *edp, u8 en)
{
	if (IS_ERR_OR_NULL(edp->board_data.edptx_vcc_gpio_desc))
		edp->board_data.edptx_vcc_gpio_desc = devm_gpiod_get(edp->dev, "edptx_vcc",
			en ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW);
	if (IS_ERR_OR_NULL(edp->board_data.edptx_vcc_gpio_desc)) {
		eDP_ERR(edp, "regist vcc gpio: %p, err: %d",
		       edp->board_data.edptx_vcc_gpio_desc,
		       IS_ERR(edp->board_data.edptx_vcc_gpio_desc));
		return;
	}
	gpiod_direction_output(edp->board_data.edptx_vcc_gpio_desc, en);
	eDP_DBG(edp, "panel vcc gpio: %p, %d", edp->board_data.edptx_vcc_gpio_desc, en);
}

void edptx_drv_check_HPD(struct edptx_drv_s *edp)
{
	u16 i = 0;
	u8 data;

	if (!(edp->status & eDPTX_STA_DRV_READY)) {
		eDP_ERR(edp, "[0x%x]: not ready for %s", edp->status, __func__);
		return;
	}

	if (!edp->viu_sel) {
		eDP_PR(edp, "sta=0x%x, viu_sel=%x", edp->status, edp->viu_sel);
		return;
	}

	if (edp->setting.user_hpd_ignore) {
		eDP_PR(edp, "ignore HPD[%ums]", edp->setting.user_hpd_ignore);
		edptx_delay_ms(edp->setting.user_hpd_ignore);
		edp->status |= eDPTX_STA_HPD_HIGH;
		return;
	}

	while (i < eDPTX_HPD_TIMEOUT_MS * 100) {
		data = edptx_if_get_hpd_level(edp, 0);
		if (data) {
			edp->status |= eDPTX_STA_HPD_HIGH;
			eDP_PR(edp, "HPD after %u.%02ums", i / 100, i % 100);
			return;
		}
		edptx_delay_us(10);
		i++;
	}

	eDP_PR(edp, "HPD is LOW");
	edp->status &= ~eDPTX_STA_HPD_HIGH;
}

void edptx_drv_start(struct edptx_drv_s *edp)
{
	struct dptx_vmode_s *dp_vmode = NULL;
	u8 port;
	int ret;

	eDP_PR(edp, "driver start(ver %s)", DPTX_DRV_VERSION);

	if (!(edp->status & (eDPTX_STA_PROBE_DONE | eDPTX_STA_DRV_READY | eDPTX_STA_HPD_HIGH))) {
		eDP_ERR(edp, "sta=0x%x, not ready for %s", edp->status, __func__);
		return;
	}

	edptx_venc_enable(edp, 0);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_if_transmitter_init(edp, port);
		edptx_set_phy_config(edp, port, 1); // ?

		// Power up link
		if (edptx_if_aux_write_single(edp, port, DPCD_SET_POWER, 0x1))
			eDP_P_ERR(edp, port, "DPCD SET POWER ERROR");
		edptx_delay_ms(20);

		// dptx_link_cfg_dft(edp, port);
		// DP protocol: EDID -> DPCD, Intel: DPCD -> EDID
		if (edptx_DPCD_capability_to_link_cfg(edp, port))
			eDP_P_ERR(edp, port, "DPCD capability detect ERROR");

		dptx_link_policy_maker(edp, port);
	}

	// multi port config check;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_set_lane_config(edp, port);
		edptx_set_phy_config(edp, port, 1); // ?
	}

	edptx_link_training(edp);

	if (edp->panel_data.timing_cnt) {
		edptx_config_timing_probe(edp);
	} else {
		ret = edptx_EDID_probe(edp, 0);
		if (ret)
			dp_vmode = &DPTX_SafeMode_640x480_vmode;
	}

	edptx_vmode_manage(edp);
	if (edp->setting.user_vmode_sel)
		dp_vmode = dptx_get_vmode(edp, edp->setting.user_vmode_sel);
	if (!dp_vmode)
		dp_vmode = dptx_get_optimum_vmode(edp);
	//if (edp->drm_hpd_cb.callback)
	//	edp->drm_hpd_cb.callback(edp->drm_hpd_cb.data);
	if (!dp_vmode) {
		dp_vmode = &DPTX_SafeMode_640x480_vmode;
		eDP_ERR(edp, "no vmode to satisfy BW, to safemode 480P");
	}
	edptx_vmode_apply_to_act_timing(edp, dp_vmode);
	edptx_act_timing_apply(edp);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_if_set_MSA(edp, port);
		edptx_set_content_protection(edp, port);
	}

	edptx_venc_enable(edp, 1);

	//edptx_if_path_reset(edp, 0, DPTX_RESET_VENC);
	edptx_delay_ms(1);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_if_transmitter_output(edp, port, 1);
	}

	edp->status |= eDPTX_STA_LINK_ON;
	eDP_PR(edp, "%s enable main stream[0x%x] video finished", __func__, edp->sink.port_mask);
}

void edptx_drv_disp_on(struct edptx_drv_s *edp)
{
	if (!(edp->status &
	      (eDPTX_STA_PROBE_DONE | eDPTX_STA_DRV_READY |
		eDPTX_STA_HPD_HIGH | eDPTX_STA_LINK_ON))) {
		eDP_ERR(edp, "sta=0x%x, not ready for %s", edp->status, __func__);
		return;
	}

	/*
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_en_gpio_desc))
	 *	edp->board_data.edptx_bl_en_gpio_desc = devm_gpiod_get(edp->dev,
	 *		"edptx_bl_en", GPIOD_OUT_HIGH);
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_en_gpio_desc)) {
	 *	eDP_ERR(edp, "regist bl en: %p, err: %d", edp->board_data.edptx_bl_en_gpio_desc,
	 *	       IS_ERR(edp->board_data.edptx_bl_en_gpio_desc));
	 *	return;
	 *}
	 *gpiod_direction_output(edp->board_data.edptx_bl_en_gpio_desc, 1);
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_pwm_gpio_desc))
	 *	edp->board_data.edptx_bl_pwm_gpio_desc = devm_gpiod_get(edp->dev,
	 *		"edptx_bl_pwm", GPIOD_OUT_LOW);
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_pwm_gpio_desc)) {
	 *	eDP_ERR(edp, "regist bl pwm: %p, err: %d",
	 *	       edp->board_data.edptx_bl_pwm_gpio_desc,
	 *	       IS_ERR(edp->board_data.edptx_bl_pwm_gpio_desc));
	 *	return;
	 *}
	 *gpiod_direction_output(edp->board_data.edptx_bl_pwm_gpio_desc, 1);
	 *eDP_DBG(edp, "panel bl pwm: %p:%d, %p:%d", edp->board_data.edptx_bl_en_gpio_desc, 1,
	 *					     edp->board_data.edptx_bl_pwm_gpio_desc, 1);
	 */

	edptx_mute_set(edp, 0);

#ifdef CONFIG_AMLOGIC_BACKLIGHT
	aml_bl_on_ctrl(edp->idx);
#endif

	edp->status |= eDPTX_STA_DISP_ON;
}

void edptx_drv_disp_off(struct edptx_drv_s *edp)
{
	if (!(edp->status & (eDPTX_STA_PROBE_DONE | eDPTX_STA_DRV_READY))) {
		eDP_ERR(edp, "sta=0x%x, not ready for %s", edp->status, __func__);
		return;
	}

	/*
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_en_gpio_desc))
	 *	edp->board_data.edptx_bl_en_gpio_desc = devm_gpiod_get(edp->dev,
	 *		"edptx_bl_en", GPIOD_OUT_LOW);
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_en_gpio_desc)) {
	 *	eDP_ERR(edp, "regist bl en: %p, err: %d", edp->board_data.edptx_bl_en_gpio_desc,
	 *	       IS_ERR(edp->board_data.edptx_bl_en_gpio_desc));
	 *	return;
	 *}
	 *gpiod_direction_output(edp->board_data.edptx_bl_en_gpio_desc, 0);
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_pwm_gpio_desc))
	 *	edp->board_data.edptx_bl_pwm_gpio_desc = devm_gpiod_get(edp->dev,
	 *		"edptx_bl_pwm", GPIOD_OUT_HIGH);
	 *if (IS_ERR_OR_NULL(edp->board_data.edptx_bl_pwm_gpio_desc)) {
	 *	eDP_ERR(edp, "regist bl pwm: %p, err: %d",
	 *	       edp->board_data.edptx_bl_pwm_gpio_desc,
	 *	       IS_ERR(edp->board_data.edptx_bl_pwm_gpio_desc));
	 *	return;
	 *}
	 *gpiod_direction_output(edp->board_data.edptx_bl_pwm_gpio_desc, 0);
	 *eDP_DBG(edp, "panel bl pwm: %p:%d, %p:%d", edp->board_data.edptx_bl_en_gpio_desc, 0,
	 *					     edp->board_data.edptx_bl_pwm_gpio_desc, 0);
	 */
#ifdef CONFIG_AMLOGIC_BACKLIGHT
	aml_bl_off_ctrl(edp->idx);
#endif
	edptx_mute_set(edp, 1);

	edp->status &= ~eDPTX_STA_DISP_ON;
}

void edptx_driver_close(struct edptx_drv_s *edp)
{
	unsigned char auxdata;
	int ret;
	struct dptx_vmode_s *dp_vmode = NULL;
	u8 port;

	if (!(edp->status & (eDPTX_STA_PROBE_DONE | eDPTX_STA_DRV_READY))) {
		eDP_ERR(edp, "sta=0x%x, not ready for %s", edp->status, __func__);
		return;
	}
	//dptx_clear_timing(edp);
	//Power down link

	if (!(edp->status & eDPTX_STA_LINK_ON)) {
		eDP_PR(edp, "%s finished", __func__);
		return;
	}

	auxdata = 0x5;
	// 010b = Set all link to D3 (power-down mode).
	// 101b = Set Main-Link D3, keep AUX block fully powered
	for (port = 3; port > 0; port--) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		ret = edptx_if_aux_write(edp, port, DPCD_SET_POWER, 1, &auxdata);
		if (ret)
			eDP_P_DBG(edp, port, "sink power down link failed");
		//all ch may close when first POWER_DOWN, so, no warn
	}

	eDP_PR(edp, "disable main stream video");

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_if_transmitter_output(edp, port, 0);
		edptx_if_path_reset(edp, port, DPTX_RESET_ALL);
	}

	if (edp->viu_sel) {
		dp_vmode = &DPTX_SafeMode_640x480_vmode;
		edptx_vmode_apply_to_act_timing(edp, dp_vmode);
		edptx_act_timing_apply(edp);
	}

	// memset(&edp->link_cfg, 0, sizeof(struct edptx_link_cfg_s));
	edp->status &= ~eDPTX_STA_LINK_ON;
	eDP_PR(edp, "%s finished", __func__);
}

void edptx_drv_PSR1_en(struct edptx_drv_s *edp, u8 port_mask, u8 en)
{
	u8 port;

	if (!(edp->status &
	      (eDPTX_STA_PROBE_DONE | eDPTX_STA_DRV_READY |
		eDPTX_STA_HPD_HIGH | eDPTX_STA_LINK_ON))) {
		eDP_ERR(edp, "DPTX sta=0x%x, not ready for %s", edp->status, __func__);
		return;
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_setup_PSR1(edp, port, en);
	}
}

void edptx_drv_PSR2_en(struct edptx_drv_s *edp, u8 port_mask, u8 en)
{
	u8 port;

	if (!(edp->status &
	      (eDPTX_STA_PROBE_DONE | eDPTX_STA_DRV_READY |
		eDPTX_STA_HPD_HIGH | eDPTX_STA_LINK_ON))) {
		eDP_ERR(edp, "DPTX sta=0x%x, not ready for %s", edp->status, __func__);
		return;
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_setup_PSR2(edp, port, en);
	}
}

void edptx_notify_check_HPD(struct edptx_drv_s *edp)
{
	if (!(edp->status & eDPTX_STA_DRV_READY))
		return;

	edptx_drv_check_HPD(edp);
}

void edptx_notify_set_backlight(struct edptx_drv_s *edp, u8 en)
{
	if (!(edp->status & eDPTX_STA_DRV_READY))
		return;
}

void edptx_notify_set_screen_mute(struct edptx_drv_s *edp, u8 en)
{
	if (!(edp->status & eDPTX_STA_DRV_READY))
		return;

	if (en)
		edptx_drv_disp_off(edp);
	else
		edptx_drv_disp_on(edp);
}

void edptx_notify_set_HPD_trigger(struct edptx_drv_s *edp, u8 en)
{
	edptx_HPD_trigger_set(edp, en);
}

void edptx_notify_set_link(struct edptx_drv_s *edp, u8 en)
{
	if (!(edp->status & eDPTX_STA_DRV_READY))
		return;

	if (!en) {
		edptx_driver_close(edp);
		return;
	}

	if (!(edp->status & eDPTX_STA_HPD_HIGH))
		edptx_drv_check_HPD(edp);

	if (edp->status & eDPTX_STA_HPD_HIGH)
		edptx_drv_start(edp);
}

void edptx_notify_set_driver_ready(struct edptx_drv_s *edp, u8 en)
{
	if (en) {
		edptx_driver_panel_power_ctrl(edp, 1);
		edptx_driver_ready(edp);
	} else {
		edptx_driver_close(edp);
		edptx_driver_panel_power_ctrl(edp, 0);
		edp->status &= ~eDPTX_STA_DRV_READY;
	}
}

void edptx_notify_set_venc(struct edptx_drv_s *edp, u8 en)
{
}
