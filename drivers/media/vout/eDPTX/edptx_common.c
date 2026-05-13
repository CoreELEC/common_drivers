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
#include "edptx_common.h"

#define VOUT_CONNECTOR_MAX 3

void edptx_act_timing_to_venc_config(struct edptx_drv_s *edp)
{
	unsigned short h_period, v_period, h_active, v_active;
	unsigned short hsync_bp, hsync_width, vsync_bp, vsync_width;
	unsigned short de_hstart, de_vstart;
	unsigned short hstart, hend, vstart, vend;
	//unsigned short h_delay = 0;

	edp->venc_cfg.ppc = 1;
	edp->venc_cfg.enc_clk = edp->act_timing.pclk / edp->venc_cfg.ppc;
	if (edp->venc_cfg.ppc > 1) {
		eDP_PR(edp, "%s: ppc=%d, pixel_clk=%d, enc_clk=%d",
			__func__, edp->venc_cfg.ppc, edp->act_timing.pclk, edp->venc_cfg.enc_clk);
	}

	h_period    = edp->act_timing.h_period;
	v_period    = edp->act_timing.v_period;
	h_active    = edp->act_timing.h_act;
	v_active    = edp->act_timing.v_act;
	hsync_bp    = edp->act_timing.h_bp;
	hsync_width = edp->act_timing.h_pw;
	vsync_bp    = edp->act_timing.v_bp;
	vsync_width = edp->act_timing.v_pw;

	de_hstart = hsync_bp + hsync_width;
	de_vstart = vsync_bp + vsync_width;

	//edp->venc_cfg.hstart = de_hstart - h_delay;
	edp->venc_cfg.hstart = de_hstart;
	edp->venc_cfg.vstart = de_vstart;
	edp->venc_cfg.hend   = h_active + edp->venc_cfg.hstart - 1;
	edp->venc_cfg.vend   = v_active + edp->venc_cfg.vstart - 1;

	edp->venc_cfg.de_hs_addr = de_hstart;
	edp->venc_cfg.de_he_addr = de_hstart + h_active;
	edp->venc_cfg.de_vs_addr = de_vstart;
	edp->venc_cfg.de_ve_addr = de_vstart + v_active - 1;

	hstart = (de_hstart + h_period - hsync_bp - hsync_width) % h_period;
	hend   = (de_hstart + h_period - hsync_bp) % h_period;
	edp->venc_cfg.hs_hs_addr = hstart;
	edp->venc_cfg.hs_he_addr = hend;
	edp->venc_cfg.hs_vs_addr = 0;
	edp->venc_cfg.hs_ve_addr = v_period - 1;

	edp->venc_cfg.vs_hs_addr = (hstart + h_period) % h_period;
	edp->venc_cfg.vs_he_addr = edp->venc_cfg.vs_hs_addr;
	vstart = (de_vstart + v_period - vsync_bp - vsync_width) % v_period;
	vend   = (de_vstart + v_period - vsync_bp) % v_period;
	edp->venc_cfg.vs_vs_addr = vstart;
	edp->venc_cfg.vs_ve_addr = vend;

	eDP_DBG(edp,
		"hs_hs=%d hs_he=%d hs_vs=%d hs_ve=%d vs_hs=%d vs_he=%d vs_vs=%d vs_ve=%d",
		edp->venc_cfg.hs_hs_addr, edp->venc_cfg.hs_he_addr,
		edp->venc_cfg.hs_vs_addr, edp->venc_cfg.hs_ve_addr,
		edp->venc_cfg.vs_hs_addr, edp->venc_cfg.vs_he_addr,
		edp->venc_cfg.vs_vs_addr, edp->venc_cfg.vs_ve_addr);
}

void edptx_set_phy_config(struct edptx_drv_s *edp, u8 port, u8 use_preset)
{
	u8 i, data[4];

	for (i = 0; i < 4; i++)
		data[i] = use_preset ?
			edp->sink.link[port].preset_ds[i] : edp->sink.link[port].curr_ds[i];

	edptx_if_set_phy_cfg(edp, port, use_preset);

	edptx_phy_set_lane(edp, port, 0xf);

	//write the preset values to the sink device

	data[0] = ds_to_DPCD_LANESET(data[0]);
	data[1] = ds_to_DPCD_LANESET(data[1]);
	data[2] = ds_to_DPCD_LANESET(data[2]);
	data[3] = ds_to_DPCD_LANESET(data[3]);
	if (edptx_if_aux_write(edp, port, DPCD_TRAINING_LANE0_SET, 4, data))
		eDP_P_ERR(edp, port, "DP sink set phy failed");
}

void edptx_set_lane_config(struct edptx_drv_s *edp, u8 port)
{
	unsigned char auxdata[2];

	eDP_P_PR(edp, port, "%s: %d lane, %u.%u GHz, ss_en: %d, enhanced_frame: %d",
		__func__, edp->sink.link[port].lane_count,
		(edp->sink.link[port].link_rate * 27) / 100,
		(edp->sink.link[port].link_rate * 27) % 100,
		edp->sink.link[port].cap.link_cap & BIT(5) ? 1 : 0,
		edp->sink.link[port].enh_frame_en);

	edptx_if_set_lane_cfg(edp, port);

	//sink Link-rate and Lane_count
	auxdata[0] = edp->sink.link[port].link_rate;  //DPCD_LINK_BANDWIDTH_SET
	//DPCD_LANE_COUNT_SET
	auxdata[1] = edp->sink.link[port].lane_count |
		     (edp->sink.link[port].enh_frame_en ? BIT(7) : 0);

	if (edptx_if_aux_write(edp, port, DPCD_LINK_BW_SET, 2, auxdata))
		eDP_P_ERR(edp, port, "sink set lane rate & count failed");

	auxdata[0] = edp->sink.link[port].cap.link_cap & BIT(5) ? BIT(4) : 0; // down_ss
	if (edptx_if_aux_write(edp, port, DPCD_DOWNSPREAD_CONTROL, 1, auxdata))
		eDP_P_ERR(edp, port, "sink set down-spread failed.");
}

/* ************************************************** *
 * vout server api
 * **************************************************
 */
void edptx_list_support_vmode(struct edptx_drv_s *edp)
{
	edptx_print_vmode(edp, NULL, 0xff);
}

void edptx_user_set_vmode(struct edptx_drv_s *edp, u8 vmd_idx)
{
	struct dptx_vmode_s *dp_vmode;
	u8 port;

	dp_vmode = dptx_get_vmode(edp, vmd_idx);
	if (!dp_vmode) {
		eDP_ERR(edp, "vmode[%u] not available", vmd_idx);
		return;
	}
	dptx_venc_enable(edp, 0);

	dptx_vmode_apply_to_act_timing(edp, dp_vmode);
	dptx_act_timing_apply(edp);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_if_set_MSA(edp, port);
		edptx_set_content_protection(edp, port);

		// edptx_if_path_reset(edp, port, DPTX_RESET_eDP_PIPE);
	}

	dptx_venc_enable(edp, 1);

	dptx_delay_ms(10);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_if_transmitter_output(edp, port, 1);
	}
	////????????????????????????uboooooooot
	// dptx_venc_enable(edp, 1);

	// dptx_delay_ms(10);
}

void edptx_setup_PSR1(struct edptx_drv_s *edp, u8 port, u8 flag)
{
	u8 auxdata[4] = {0, 0, 0, 0};

	if (!(edp->sink.port_mask & BIT(port)))
		return;

	if (edptx_if_aux_write_single(edp, port,
				DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(1) | BIT(3)))
		eDP_P_ERR(edp, port, "DPCD SET PSR1 EN ERROR");

	edptx_if_PSR1_ctrl(edp, port, 1);

	// // error, should check vbp time
	dptx_delay_us(dptx_PSR_setup_time[edp->sink.link[port].cap.eDP.PSR_setup_time] + 20);
	edptx_if_aux_write_single(edp, port, DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(3));

	edptx_if_aux_read(edp, port, DPCD_PSR_ERROR_STATUS, 4, auxdata);

	eDP_PR(edp, "%s finished 0x%x 0x%x 0x%x", __func__, auxdata[0], auxdata[1], auxdata[2]);
}

void edptx_setup_PSR2(struct edptx_drv_s *edp, u8 port, u8 flag)
{
	u8 auxdata[4] = {0, 0, 0, 0};

	if (!(edp->sink.port_mask & BIT(port)))
		return;

	if (edptx_if_aux_write_single(edp, port,
					DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(1) | BIT(3)))
		eDP_P_ERR(edp, port, "DPCD SET PSR2 EN ERROR");

	edptx_if_PSR1_ctrl(edp, port, 1);

	// // error, should check vbp time
	dptx_delay_us(dptx_PSR_setup_time[edp->sink.link[port].cap.eDP.PSR_setup_time] + 20);
	dptx_delay_ms(100);
	//edptx_if_aux_write_single(edp, port, DPCD_eDP_PSR_ENABLE_AND_CONF, BIT(0) | BIT(3));

	edptx_if_aux_read(edp, port, DPCD_PSR_ERROR_STATUS, 4, auxdata);

	eDP_PR(edp, "%s finished 0x%x 0x%x 0x%x\n", __func__, auxdata[0], auxdata[1], auxdata[2]);
}

u8 edptx_vmode_str_check(struct edptx_drv_s *edp, char *vmd_str)
{
	u8 idx;
	u32 fr100;
	struct dptx_vmode_s *vmode_p;
	char v_name_t[32];

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		memset(v_name_t, 0, 32);
		vmode_p = &edp->vmode_mgr.vmodes[idx];
		if (!(vmode_p->flag & VMODE_FLAG_VALID))
			continue;

		if (vmode_p->fr_adv == 1) { //5994 case
			fr100 = vmode_p->fr100_int * 1000;
			fr100 = fr100 / 1001;
		} else {
			fr100 = vmode_p->fr100_int;
		}
		__str_add_vmode(edp, v_name_t, vmode_p, fr100 / 100);

		if (!strcmp(v_name_t, vmd_str)) {
			edp->next_vmd_idx = idx;
			return 0;
		}
		if (!strcmp("640x480p60hz", vmd_str)) {
			edp->next_vmd_idx = 0xff;
			return 0;
		}
	}

	edp->next_vmd_idx = 0xff;
	return 1;
}

void edptx_vout_notify_mode_change_pre(struct edptx_drv_s *edp)
{
	if (edp->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &edp->vinfo.mode);
#endif
	} else if (edp->viu_sel == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &edp->vinfo.mode);
#endif
	} else if (edp->viu_sel == 3) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &edp->vinfo.mode);
#endif
	}
}

void edptx_vout_notify_mode_change(struct edptx_drv_s *edp)
{
	if (edp->viu_sel == 1) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &edp->vinfo.mode);
#endif
	} else if (edp->viu_sel == 2) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		vout2_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &edp->vinfo.mode);
#endif
	} else if (edp->viu_sel == 3) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		vout3_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &edp->vinfo.mode);
#endif
	}
}
