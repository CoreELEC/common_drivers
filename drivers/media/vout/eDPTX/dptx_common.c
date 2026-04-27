// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <linux/cma.h>
#include <linux/dma-map-ops.h>
#include <linux/dma-mapping.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#include "dptx_common.h"

#define VOUT_CONNECTOR_MAX 3

void dptx_act_timing_to_venc_config(struct dptx_drv_s *dptx)
{
	unsigned short h_period, v_period, h_active, v_active;
	unsigned short hsync_bp, hsync_width, vsync_bp, vsync_width;
	unsigned short de_hstart, de_vstart;
	unsigned short hstart, hend, vstart, vend;
	//unsigned short h_delay = 0;

	dptx->venc_cfg.ppc = 1;
	dptx->venc_cfg.enc_clk = dptx->act_timing.pclk / dptx->venc_cfg.ppc;
	if (dptx->venc_cfg.ppc > 1) {
		DPTX_PR(dptx, "%s: ppc=%d, pixel_clk=%d, enc_clk=%d",
		      __func__, dptx->venc_cfg.ppc, dptx->act_timing.pclk, dptx->venc_cfg.enc_clk);
	}

	h_period    = dptx->act_timing.h_period;
	v_period    = dptx->act_timing.v_period;
	h_active    = dptx->act_timing.h_act;
	v_active    = dptx->act_timing.v_act;
	hsync_bp    = dptx->act_timing.h_bp;
	hsync_width = dptx->act_timing.h_pw;
	vsync_bp    = dptx->act_timing.v_bp;
	vsync_width = dptx->act_timing.v_pw;

	de_hstart = hsync_bp + hsync_width;
	de_vstart = vsync_bp + vsync_width;

	//dptx->venc_cfg.hstart = de_hstart - h_delay;
	dptx->venc_cfg.hstart = de_hstart;
	dptx->venc_cfg.vstart = de_vstart;
	dptx->venc_cfg.hend   = h_active + dptx->venc_cfg.hstart - 1;
	dptx->venc_cfg.vend   = v_active + dptx->venc_cfg.vstart - 1;

	dptx->venc_cfg.de_hs_addr = de_hstart;
	dptx->venc_cfg.de_he_addr = de_hstart + h_active;
	dptx->venc_cfg.de_vs_addr = de_vstart;
	dptx->venc_cfg.de_ve_addr = de_vstart + v_active - 1;

	hstart = (de_hstart + h_period - hsync_bp - hsync_width) % h_period;
	hend   = (de_hstart + h_period - hsync_bp) % h_period;
	dptx->venc_cfg.hs_hs_addr = hstart;
	dptx->venc_cfg.hs_he_addr = hend;
	dptx->venc_cfg.hs_vs_addr = 0;
	dptx->venc_cfg.hs_ve_addr = v_period - 1;

	dptx->venc_cfg.vs_hs_addr = (hstart + h_period) % h_period;
	dptx->venc_cfg.vs_he_addr = dptx->venc_cfg.vs_hs_addr;
	vstart = (de_vstart + v_period - vsync_bp - vsync_width) % v_period;
	vend   = (de_vstart + v_period - vsync_bp) % v_period;
	dptx->venc_cfg.vs_vs_addr = vstart;
	dptx->venc_cfg.vs_ve_addr = vend;

	DPTX_DBG(dptx,
		"hs_hs=%d hs_he=%d hs_vs=%d hs_ve=%d vs_hs=%d vs_he=%d vs_vs=%d vs_ve=%d",
		dptx->venc_cfg.hs_hs_addr, dptx->venc_cfg.hs_he_addr,
		dptx->venc_cfg.hs_vs_addr, dptx->venc_cfg.hs_ve_addr,
		dptx->venc_cfg.vs_hs_addr, dptx->venc_cfg.vs_he_addr,
		dptx->venc_cfg.vs_vs_addr, dptx->venc_cfg.vs_ve_addr);
}

void __dptx_set_phy_config(struct dptx_drv_s *dptx, u8 port, u8 use_preset)
{
	u8 i, data[4];

	for (i = 0; i < 4; i++)
		data[i] = use_preset ?
			dptx->sink.link[port].preset_ds[i] : dptx->sink.link[port].curr_ds[i];

	dptx_if_set_phy_cfg(dptx, port, use_preset);

	dptx_phy_set_lane(dptx, port, 0xf);

	//write the preset values to the sink device

	data[0] = ds_to_DPCD_LANESET(data[0]);
	data[1] = ds_to_DPCD_LANESET(data[1]);
	data[2] = ds_to_DPCD_LANESET(data[2]);
	data[3] = ds_to_DPCD_LANESET(data[3]);
	if (dptx_if_aux_write(dptx, port, DPCD_TRAINING_LANE0_SET, 4, data))
		DPTX_P_ERR(dptx, port, "DP sink set phy failed");
}

void edptx_set_lane_config(struct dptx_drv_s *dptx, u8 port)
{
	unsigned char auxdata[2];

	DPTX_P_PR(dptx, port, "%s: %d lane, %u.%u GHz, ss_en: %d, enhanced_frame: %d",
		__func__, dptx->sink.link[port].lane_count,
		(dptx->sink.link[port].link_rate * 27) / 100,
		(dptx->sink.link[port].link_rate * 27) % 100,
		dptx->sink.link[port].cap.link_cap & BIT(5) ? 1 : 0,
		dptx->sink.link[port].enh_frame_en);

	dptx_if_set_lane_cfg(dptx, port);

	//sink Link-rate and Lane_count
	auxdata[0] = dptx->sink.link[port].link_rate;  //DPCD_LINK_BANDWIDTH_SET
	//DPCD_LANE_COUNT_SET
	auxdata[1] = dptx->sink.link[port].lane_count |
		     (dptx->sink.link[port].enh_frame_en ? BIT(7) : 0);

	if (dptx_if_aux_write(dptx, port, DPCD_LINK_BW_SET, 2, auxdata))
		DPTX_P_ERR(dptx, port, "sink set lane rate & count failed");

	auxdata[0] = dptx->sink.link[port].cap.link_cap & BIT(5) ? BIT(4) : 0; // down_ss
	if (dptx_if_aux_write(dptx, port, DPCD_DOWNSPREAD_CONTROL, 1, auxdata))
		DPTX_P_ERR(dptx, port, "sink set down-spread failed.");
}

/* ************************************************** *
 * vout server api
 * **************************************************
 */
void dptx_list_support_vmode(struct dptx_drv_s *dptx)
{
	dptx_print_vmode(dptx, NULL, 0xff);
}

void dptx_user_set_vmode(struct dptx_drv_s *dptx, u8 vmd_idx)
{
	struct dptx_vmode_s *dp_vmode;
	u8 port;

	dp_vmode = dptx_get_vmode(dptx, vmd_idx);
	if (!dp_vmode) {
		DPTX_ERR(dptx, "vmode[%u] not available", vmd_idx);
		return;
	}
	dptx_venc_enable(dptx, 0);

	dptx_vmode_apply_to_act_timing(dptx, dp_vmode);
	dptx_act_timing_apply(dptx);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_set_MSA(dptx, port);
		dptx_set_content_protection(dptx, port);

		// dptx_if_path_reset(dptx, port, DPTX_RESET_eDP_PIPE);
	}

	dptx_venc_enable(dptx, 1);

	dptx_delay_ms(10);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(dptx->sink.port_mask & BIT(port)))
			continue;
		dptx_if_transmitter_output(dptx, port, 1);
	}
	////????????????????????????uboooooooot
	// dptx_venc_enable(dptx, 1);

	// dptx_delay_ms(10);
}

void dptx_eDP_PSR1(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	u8 auxdata[4] = {0, 0, 0, 0};

	if (!(dptx->sink.port_mask & BIT(port)))
		return;

	if (dptx_if_aux_write_single(dptx, port,
				DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(1) | BIT(3)))
		DPTX_P_ERR(dptx, port, "DPCD SET PSR1 EN ERROR");

	dptx_if_PSR1_ctrl(dptx, port, 1);

	// // error, should check vbp time
	dptx_delay_us(dptx_PSR_setup_time[dptx->sink.link[port].cap.eDP.PSR_setup_time] + 20);
	dptx_if_aux_write_single(dptx, port, DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(3));

	dptx_if_aux_read(dptx, port, DPCD_PSR_ERROR_STATUS, 4, auxdata);

	DPTX_PR(dptx, "%s finished 0x%x 0x%x 0x%x", __func__, auxdata[0], auxdata[1], auxdata[2]);
}

void dptx_eDP_PSR2(struct dptx_drv_s *dptx, u8 port, u8 flag)
{
	u8 auxdata[4] = {0, 0, 0, 0};

	if (!(dptx->sink.port_mask & BIT(port)))
		return;

	if (dptx_if_aux_write_single(dptx, port,
					DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(1) | BIT(3)))
		DPTX_P_ERR(dptx, port, "DPCD SET PSR2 EN ERROR");

	dptx_if_PSR1_ctrl(dptx, port, 1);

	// // error, should check vbp time
	dptx_delay_us(dptx_PSR_setup_time[dptx->sink.link[port].cap.eDP.PSR_setup_time] + 20);
	dptx_delay_ms(100);
	//dptx_if_aux_write_single(dptx, port, DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(3));

	dptx_if_aux_read(dptx, port, DPCD_PSR_ERROR_STATUS, 4, auxdata);

	DPTX_PR(dptx, "%s finished 0x%x 0x%x 0x%x\n", __func__, auxdata[0], auxdata[1], auxdata[2]);
}

u8 dptx_vmode_str_check(struct dptx_drv_s *dptx, char *vmd_str)
{
	u8 idx;
	u32 fr100;
	struct dptx_vmode_s *vmode_p;
	char v_name_t[32];

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		memset(v_name_t, 0, 32);
		vmode_p = &dptx->vmode_mgr.vmodes[idx];
		if (!(vmode_p->flag & VMODE_FLAG_VALID))
			continue;

		if (vmode_p->fr_adv == 1) { //5994 case
			fr100 = vmode_p->fr100_int * 1000;
			fr100 = fr100 / 1001;
		} else {
			fr100 = vmode_p->fr100_int;
		}
		__str_add_vmode(dptx, v_name_t, vmode_p, fr100 / 100);

		if (!strcmp(v_name_t, vmd_str)) {
			dptx->next_vmd_idx = idx;
			return 0;
		}
		if (!strcmp("640x480p60hz", vmd_str)) {
			dptx->next_vmd_idx = 0xff;
			return 0;
		}
	}

	dptx->next_vmd_idx = 0xff;
	return 1;
}

void dptx_vout_notify_mode_change_pre(struct dptx_drv_s *dptx)
{
	if (dptx->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &dptx->vinfo.mode);
#endif
	} else if (dptx->viu_sel == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &dptx->vinfo.mode);
#endif
	} else if (dptx->viu_sel == 3) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &dptx->vinfo.mode);
#endif
	}
}

void dptx_vout_notify_mode_change(struct dptx_drv_s *dptx)
{
	if (dptx->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &dptx->vinfo.mode);
#endif
	} else if (dptx->viu_sel == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &dptx->vinfo.mode);
#endif
	} else if (dptx->viu_sel == 3) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &dptx->vinfo.mode);
#endif
	}
}
