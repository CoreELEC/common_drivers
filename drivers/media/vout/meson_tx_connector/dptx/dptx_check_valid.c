// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_log.h>
#include "dptx_log.h"
#include "meson_tx_internal.h"
#include "dptx_internal.h"

static inline u32 link_rate_khz(enum dp_link_rate_e rate)
{
	return rate * 27000 * 10;
};

/* check timing/refresh_rate and cs/cd support by SOC DPTX */
static bool dptx_check_tx_cap(struct tx_timing *timing, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd, struct dptx_cap *tx_cap)
{
	if (!timing || !tx_cap) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return false;
	}

	/* step1: check timing cap of DPTX connector:
	 * 1.check if there's any limitation of timing
	 * 2.check refresh rate is within cap of DPTX
	 */
	if (timing->v_sync > tx_cap->max_fresh_rate)
		return false;
	if (timing->h_active > tx_cap->max_h_active)
		return false;
	if (timing->v_active > tx_cap->max_v_active)
		return false;

	/* step2: check if cs/cd under specific timing is
	 * supported by SOC dptx.
	 * currently treat cs/cd is supported by default,
	 * add limitation check if necessary
	 */
	/* TODO: check if link rate/lane count supported by SOC */
	return true;
}

/* check timing/refresh_rate and cs/cd support by DP RX */
static bool dptx_check_rx_cap(struct tx_timing *timing, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd, struct rx_cap *rx_cap)
{
	if (!timing || !rx_cap) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return false;
	}
	/* step1: check timing cap of RX EDID */
	if (!meson_tx_edid_validate_timing(timing, rx_cap))
		return false;
	/* step2: check if cs/cd under specific timing is supported by RX. */
	if (!meson_tx_edid_validate_color(timing, cs, cd, rx_cap))
		return false;
	return true;
}

/* check bandwidth of DP TX and RX.
 * Note: Disp_ID_V1.3 Table 4-51 of chapter 4.12 and chapter 1.8
 * In the case there is discrepancy between information in this block
 * and information contained within the interface configuration data (e.g.
 * DPCD for DisplayPort), the source shall use interface configuration data
 */
static bool dptx_check_bandwidth_cap(enum number_of_links lane_cnt, enum dp_link_rate_e link_rate,
	struct rx_cap *rx_cap, struct dpcd_cap *dpcd_cap, struct dptx_cap *tx_cap)
{
	struct display_id_cap *disp_cap = NULL;

	if (!rx_cap || !tx_cap)
		return false;

	/* step1: check bandwidth cap of TX */
	if (lane_cnt > tx_cap->max_lane_count || link_rate_khz(link_rate) > tx_cap->max_link_rate)
		return false;

	/* step2: check bandwidth cap of RX */
	disp_cap = &rx_cap->disp_id_cap;
	/* when displayID cap and DPCD */
	if (lane_cnt > disp_cap->display_interface.link.links) {
		DPTX_DEBUG_EDID("[%s]: lane count(%d) exceed rx cap(%d)\n",
			__func__, lane_cnt, disp_cap->display_interface.link.links);
		/* return false */
	}
	/* check max lane count and link rate in DPCD */
	if (lane_cnt > dpcd_cap->max_lane_count ||
		link_rate > dpcd_cap->max_link_rate) {
		DPTX_DEBUG_EDID("lane count/link_rate(%d %d) exceed rx cap(%d %d)\n",
			lane_cnt, link_rate, dpcd_cap->max_lane_count, dpcd_cap->max_link_rate);
		return false;
	}
	return true;
}

/* validate mode of SW & HW format para, to see timing/cs+cd/bandwidth
 * supported by DP TX and RX
 */
static bool dptx_validate_fmt_para(struct meson_tx_dev *tx_base,
	struct meson_tx_format_para *sw_para, struct dptx_hw_fmt_para *hw_para)
{
	struct tx_timing *timing = NULL;

	if (!tx_base || !sw_para || !hw_para)
		return false;

	timing = &sw_para->timing;

	if (tx_base->pxp_mode) {
		DPTX_INFO("[%s]: treat mode valid in PXP\n", __func__);
		return true;
	}

	/* step1: validate timing + cs/cd in rx_cap */
	if (!dptx_check_rx_cap(timing, sw_para->cs, sw_para->cd, &tx_base->rxcap))
		return false;

	/* step2: validate timing + cs/cd in tx_cap. */
	if (!dptx_check_tx_cap(timing, sw_para->cs, sw_para->cd, tx_base->tx_cap))
		return false;

	/* step3: validate bandwidth in tx/rx cap */
	if (!dptx_check_bandwidth_cap((enum number_of_links)hw_para->lane_count,
		hw_para->link_rate, &tx_base->rxcap, &tx_base->dpcd_cap, tx_base->tx_cap))
		return false;

	return true;
}

/* currently drm pass format para within meson_tx_state
 * need to get hw fmt para and then do validate
 */
int dptx_validate_tx_state_fmt_para(struct meson_tx_dev *tx_base,
	struct meson_tx_state *base_state)
{
	bool is_valid;
	struct dptx_hw_fmt_para *hw_para;

	if (!tx_base || !base_state) {
		DPTX_ERROR("[%s]: invalid input param\n", __func__);
		return -EINVAL;
	}

	hw_para = &base_state->para.tx_hw_para.dptx_hw_para;
	/* build HW format param */
	if (dptx_calc_hw_fmt_para(tx_base, &base_state->para, hw_para))
		return -EINVAL;
	is_valid = dptx_validate_fmt_para(tx_base, &base_state->para, hw_para);
	if (is_valid)
		return 0;
	else
		return -EINVAL;
}

/* build format with current timing and basic cs/cd,
 * and check if it's supported by tx_cap/rx_cap.
 * Note it's similar as dptx_validate_fmt_para but
 * skip timing check in RX cap. it's only used for
 * get mode list api
 */
bool dptx_validate_timing_with_basic_cs(struct meson_tx_dev *tx_base, struct tx_timing *timing)
{
	struct meson_tx_format_para sw_para;
	struct dptx_hw_fmt_para hw_para;
	enum hdmi_color_depth cd = COLORDEPTH_24B;
	enum hdmi_colorspace cs = HDMI_COLORSPACE_RGB;
	const enum hdmi_quantization_range cr = HDMI_QUANTIZATION_RANGE_FULL;

	if (!tx_base || !timing)
		return false;

	/* step1: validate cs/cd in rx_cap,
	 * note skip timing check as timing is selected in rx cap
	 */
	if (!meson_tx_edid_validate_color(timing, cs, cd, &tx_base->rxcap))
		return false;

	/* step2: validate timing + cs/cd in tx_cap */
	if (!dptx_check_tx_cap(timing, cs, cd, tx_base->tx_cap))
		return false;

	/* build SW/HW format param */
	if (meson_tx_format_para_init(tx_base, timing, 0, cs, cd, cr, &sw_para))
		return false;
	if (dptx_calc_hw_fmt_para(tx_base, &sw_para, &hw_para))
		return false;

	/* step3: validate bandwidth in tx/rx cap */
	if (!dptx_check_bandwidth_cap((enum number_of_links)hw_para.lane_count,
		hw_para.link_rate, &tx_base->rxcap, &tx_base->dpcd_cap, tx_base->tx_cap))
		return false;

	return true;
}

