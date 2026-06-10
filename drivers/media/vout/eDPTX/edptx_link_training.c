// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/DPCD_REG.h>
#include "edptx_common.h"

#define DP_FULL_LINK_TRAINING_ATTEMPTS 3
#define DP_TRAINING_LINKRATE_ATTEMPTS  3
#define DP_CLOCK_REC_TIMEOUT    150
#define DP_TRAINING_CR_ATTEMPTS 5
#define DP_TRAINING_EQ_ATTEMPTS 5

enum DP_training_status_e {
	DP_TRAINING_CLOCK_REC,
	DP_TRAINING_CHANNEL_EQ,
	DP_TRAINING_SUCCESS,
	DP_TRAINING_FAILED,
	DP_TRAINING_ADJ_SPD_CR_FAIL,
	DP_TRAINING_ADJ_SPD_CR_FAIL_IN_EQ,
	DP_TRAINING_ADJ_SPD_EQ_FAIL_OVERTIME,
};

struct DPTX_test_pat_s DP_test_pat[11] = {
	{"TPS_DISABLE",        0x00, 0, 1}, //TPS_DISABLE
	{"TPS1",               0x01, 1, 1}, //TPS1
	{"TPS2",               0x02, 1, 1}, //TPS2
	{"TPS3",               0x03, 1, 1}, //TPS3
	{"TPS4",               0x07, 1, 1}, //TPS4
	{"QUAL_PAT_DISABLE",   0x00, 0, 1}, //QUAL_PAT_DISABLE
	{"D10.2(unscrambled)", 0x01, 1, 1}, //D10.2
	{"SYMBOL_ERROR_MSR",   0x02, 0, 1}, //SYMBOL_ERROR_MSR
	{"PRBS7",              0x03, 1, 0}, //PRBS7
	{"80BIT_CUSTOM",       0x04, 1, 0}, //80BIT_CUSTOM
	{"HBR2_EYE",           0x05, 0, 1}, //HBR2_EYE
};

static void dptx_training_status_print(struct edptx_drv_s *edp)
{
	unsigned char aux_data[3] = {0, 0, 0}, port;
	int ret;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		ret = edptx_if_aux_read(edp, port, DPCD_LANE0_1_STATUS, 3, aux_data);
		if (ret == 0) {
			eDP_P_DBG(edp, port, "%s: aux_data [0]=0x%x, [1]=0x%x, [2]=0x%x",
				__func__, aux_data[0], aux_data[1], aux_data[2]);
		}
	}
}

/* @return:
 * 0: link rate reduced
 * 1: link rate already lowest RBR (1.62G)
 * 2: link rate not cover resolution (eDP)
 */
static int dptx_link_rate_config_reduce(struct edptx_drv_s *edp)
{
	u8 link_rate, lane_cnt, port;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;

		if (edp->sink.link[port].link_rate_update != 2) {
			edp->sink.link[port].link_rate_update = 0;
			continue;
		}
		link_rate = edp->sink.link[port].link_rate;
		lane_cnt = edp->sink.link[port].lane_count;

		if (link_rate > eDP_LINK_RATE_HBR2) {
			link_rate = eDP_LINK_RATE_HBR2;
		} else if (link_rate > eDP_LINK_RATE_HBR) {
			link_rate = eDP_LINK_RATE_HBR;
		} else if (link_rate > eDP_LINK_RATE_RBR) {
			link_rate = eDP_LINK_RATE_RBR;
		} else {
			eDP_ERR(edp, "%s: already lowest link rate", __func__);
			return 1;
		}
		eDP_P_PR(edp, port, "%s: link_rate: 0x%x, lane: %d",
			__func__, link_rate, lane_cnt);

		edp->sink.link[port].link_rate = link_rate;
		edp->sink.link[port].link_rate_update = 1;
	}

	return 0;
}

/* @brief:
 * 1. read DPCD_ADJUST_REQUEST_LANE0_1 2_3
 * 2. store to edp_cfg: [adj_req_preem, adj_req_vswing]
 * 3. compare between [curr*, adj_reg*], set phy_update tag
 */
static int edptx_training_phy_adj_req_process(struct edptx_drv_s *edp, u8 port)
{
	struct edptx_link_cfg_s *link_s = &edp->sink.link[port];
	unsigned char adj_req[2] = {0, 0}, i;

	if (edptx_if_aux_read(edp, port, DPCD_ADJUST_REQUEST_LANE0_1, 2, adj_req)) {
		eDP_P_PR(edp, port, "%s: aux read DPCD_ADJUST_REQUEST failed", __func__);
		link_s->phy_update = 0;
		return 0;
	}
	link_s->adj_req_ds[0] = dptx_v_p_to_ds(((adj_req[0] >> 0) & 0x3), (adj_req[0] >> 2) & 0x3);
	link_s->adj_req_ds[1] = dptx_v_p_to_ds(((adj_req[0] >> 4) & 0x3), (adj_req[0] >> 6) & 0x3);
	link_s->adj_req_ds[2] = dptx_v_p_to_ds(((adj_req[1] >> 0) & 0x3), (adj_req[1] >> 2) & 0x3);
	link_s->adj_req_ds[3] = dptx_v_p_to_ds(((adj_req[1] >> 4) & 0x3), (adj_req[1] >> 6) & 0x3);

	//determine vswing & preem change on any lane
	link_s->phy_update = 0;
	for (i = 0; i < 4; i++)
		link_s->phy_update |= (link_s->adj_req_ds[i] != link_s->curr_ds[i]) << i;

	eDP_P_PR(edp, port, "%s: phy update: 0x%x", __func__, link_s->phy_update);

	if (!(edptx_print_level >= LOG_V && link_s->phy_update))
		return 0;
	pr_info(" ----- 0:Vsw/Pre - 1:Vsw/Pre - 2:Vsw/Pre - 3:Vsw/Pre --\n"
		" adj_req:  %d %d   |   %d %d   |   %d %d   |   %d %d\n"
		" current:  %d %d   |   %d %d   |   %d %d   |   %d %d\n",
		dptx_ds_to_vswing(link_s->adj_req_ds[0]), dptx_ds_to_preem(link_s->adj_req_ds[0]),
		dptx_ds_to_vswing(link_s->adj_req_ds[1]), dptx_ds_to_preem(link_s->adj_req_ds[1]),
		dptx_ds_to_vswing(link_s->adj_req_ds[2]), dptx_ds_to_preem(link_s->adj_req_ds[2]),
		dptx_ds_to_vswing(link_s->adj_req_ds[3]), dptx_ds_to_preem(link_s->adj_req_ds[3]),
		dptx_ds_to_vswing(link_s->curr_ds[0]),    dptx_ds_to_preem(link_s->curr_ds[0]),
		dptx_ds_to_vswing(link_s->curr_ds[1]),    dptx_ds_to_preem(link_s->curr_ds[1]),
		dptx_ds_to_vswing(link_s->curr_ds[2]),    dptx_ds_to_preem(link_s->curr_ds[2]),
		dptx_ds_to_vswing(link_s->curr_ds[3]),    dptx_ds_to_preem(link_s->curr_ds[3]));
	return 0;
}

static void dptx_training_phy_update_apply(struct edptx_drv_s *edp, u8 port)
{
	struct edptx_link_cfg_s *cfg = &edp->sink.link[port];
	u8 i;

	if (cfg->phy_update == 0)
		return;

	for (i = 0; i < 4; i++)
		cfg->curr_ds[i] = cfg->adj_req_ds[i];

	edptx_set_phy_config(edp, port, 0);

	cfg->phy_update = 0;
}

/* *************** full link training ****************** */
static int edptx_set_pattern(struct edptx_drv_s *edp, u8 port, unsigned char pattern,
				u32 cos_80b_0,  u32 cos_80b_1, u32 cos_80b_2)
{
	unsigned char _data;//, scrambling_en, encoding_en;
	int ret = 0;

	/* DPCD reg:
	 * TRAINING_PATTERN_SET
	 *   bit[1:0]:TRAINING_PATTERN_SELECT
	 *     00:off, 01:TPS1, 10:TPS2, 11:TPS3(DPCD1.2+)
	 *   bit[3:2]:LINK_QUAL_PATTERN_SET                            ! DPCD1.1
	 *     00:off, 01:D10.2, 10:Symbol-Error-Rate-msr, 11:PRBS7
	 *   bit[3:2]:ALWAYS 00  ! DPCD1.2 & 1.3 (per-lane control in LINK_QUAL_LANEn_SET)
	 *   bit[3:0]:0x0:off, 0x1:TPS1, 0x2:TPS2, 0x3:TPS3, 0x7:TPS4  ! DPCD1.4 (8b/10b)
	 *   bit[3:0]:0x0:off, 0x1:128b/132b_TPS1, 0x2:128b/132b_TPS2  ! DPCD1.4 (128b/132b)
	 *   bit[4]  :RECOVERED_CLOCK_OUT_EN (not available in 128b/132b)
	 *   bit[5]  :SCRAMBLING_DISABLE
	 *   bit[7:6]:SYMBOL ERROR COUNT SEL (not available in 128b/132b)
	 */

	switch (pattern) {
	case eDPTX_TPS_DISABLE:
	case eDPTX_TPS1:
	case eDPTX_TPS2:
	case eDPTX_TPS3:
	case eDPTX_TPS4:
		edptx_if_transmit_pattern(edp, port, pattern, 0xf, 0, 0, 0);
		_data = (DP_test_pat[pattern].data & 0x3) |
			(DP_test_pat[pattern].SR_disable << 5) |
			(0x3 << 6);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TRAINING_PATTERN_SET, _data);
		if (edptx_print_level >= LOG_V || ret)
			eDP_P_PR(edp, port, "%s: %s %s", __func__,
				DP_test_pat[pattern].name, ret ? "failed" : "");
		break;
	case eDPTX_QUAL_PAT_DISABLE:
	case eDPTX_D10P2:
	case eDPTX_SYMBOL_ERROR_MSR:
	case eDPTX_PRBS7:
	case eDPTX_HBR2_EYE:
		edptx_if_transmit_pattern(edp, port, pattern, 0xf, cos_80b_0, cos_80b_1, cos_80b_2);
		_data = (DP_test_pat[pattern].SR_disable << 5) | (0x3 << 6);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TRAINING_PATTERN_SET, _data);
		// ! DPCD1.0/1.1 should use DPCD_TRAINING_PATTERN_SET[2:3]
		// in case of year 2024, no device using DPCD1.0/1.1? ignore
		_data = (DP_test_pat[pattern].data & 0x7);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE0_SET, _data);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE1_SET, _data);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE2_SET, _data);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE3_SET, _data);
		if (edptx_print_level >= LOG_V || ret)
			eDP_P_PR(edp, port, "%s: %s %s", __func__,
				DP_test_pat[pattern].name, ret ? "failed" : "");
		break;
	case eDPTX_80BIT_CUSTOM:
		edptx_if_transmit_pattern(edp, port, pattern, 0xf, cos_80b_0, cos_80b_1, cos_80b_2);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_0,
			cos_80b_0 & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_1,
			(cos_80b_0 >> 8) & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_2,
			(cos_80b_0 >> 16) & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_3,
			(cos_80b_0 >> 24) & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_4,
			cos_80b_1 & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_5,
			(cos_80b_1 >> 8) & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_6,
			(cos_80b_1 >> 16) & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_7,
			(cos_80b_1 >> 24) & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_8,
			cos_80b_2 & 0xff);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TEST_80B_COS_9,
			(cos_80b_2 >> 8) & 0xff);

		_data = (DP_test_pat[pattern].SR_disable << 5) | (0x3 << 6);
		ret = edptx_if_aux_write_single(edp, port, DPCD_TRAINING_PATTERN_SET, _data);
		// ! DPCD1.0/1.1 should use DPCD_TRAINING_PATTERN_SET[2:3]
		// in case of year 2026, no device using DPCD1.0/1.1? ignore
		_data = (DP_test_pat[pattern].data & 0x7);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE0_SET, _data);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE1_SET, _data);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE2_SET, _data);
		ret = edptx_if_aux_write_single(edp, port, DPCD_LINK_QUAL_LANE3_SET, _data);
		break;
	default:
		eDP_P_ERR(edp, port, "%s: (%u) invalid", __func__, pattern);
		break;
	}

	return ret;
}

void edptx_set_pattern_to_all_port(struct edptx_drv_s *edp, unsigned char pattern,
			u32 cos_80b_0,  u32 cos_80b_1, u32 cos_80b_2)
{
	u8 port, pat;

	switch (pattern) {
	case eDPTX_TPS_DISABLE:
	case eDPTX_TPS1: // DPTX_TRP_CR
	case eDPTX_TPS2:
	case eDPTX_TPS3:
	case eDPTX_QUAL_PAT_DISABLE:
	case eDPTX_D10P2:
	case eDPTX_SYMBOL_ERROR_MSR:
	case eDPTX_PRBS7:
	case eDPTX_HBR2_EYE:
	case eDPTX_CP2520_2:
	case eDPTX_CP2520_3:
	case eDPTX_TPS4:
	case eDPTX_80BIT_CUSTOM:
		pat = pattern;
		break;
	case eDPTX_TRP_EQ:
		if (edp->sink.link[0].cap.link_cap & BIT(1) && edp->data->TPS_support & BIT(1))
			pat = eDPTX_TPS4;
		else if (edp->sink.link[0].cap.link_cap & BIT(0) && edp->data->TPS_support & BIT(0))
			pat = eDPTX_TPS3;
		else
			pat = eDPTX_TPS2;
		break;
	default:
		eDP_ERR(edp, "%s: (%u) invalid", __func__, pattern);
		return;
	}

	eDP_PR(edp, "%s [0x%x 0x%x 0x%x]", DP_test_pat[pat].name,
		cos_80b_0, cos_80b_1, cos_80b_2);

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;

		edptx_set_pattern(edp, port, pat, cos_80b_0, cos_80b_1, cos_80b_2);
	}
}

//Lane status register constants
#define BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE  0
#define BIT_DPCD_LANE_02_STATUS_CHAN_EQ_DONE  1
#define BIT_DPCD_LANE_02_STATUS_SYM_LOCK_DONE 2
#define BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE  4
#define BIT_DPCD_LANE_13_STATUS_CHAN_EQ_DONE  5
#define BIT_DPCD_LANE_13_STATUS_SYM_LOCK_DONE 6
#define BIT_DPCD_LANE_ALIGNMENT_DONE          0

static int dptx_check_channel_equalization(struct edptx_drv_s *edp, u8 port)
{
	unsigned int cr_done, channel_eq_done, symbol_lock_done, lane_align_done;
	unsigned char clk_rec[4], chan_eq[4], sym_lock[4], aux_data[3] = {0, 0, 0};
	unsigned char lane_cnt;
	unsigned char ret;

	lane_cnt = edp->sink.link[port].lane_count;

	ret = edptx_if_aux_read(edp, port, DPCD_LANE0_1_STATUS, 3, aux_data);
	if (ret) {
		eDP_P_ERR(edp, port, "%s: aux read DPCD_LANE0_1_STATUS failed", __func__);
		return -1;
	}

	clk_rec[0]  = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE)  & 0x01);
	clk_rec[1]  = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE)  & 0x01);
	clk_rec[2]  = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE)  & 0x01);
	clk_rec[3]  = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE)  & 0x01);
	chan_eq[0]  = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_CHAN_EQ_DONE)  & 0x01);
	chan_eq[1]  = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE)  & 0x01);
	chan_eq[2]  = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_CHAN_EQ_DONE)  & 0x01);
	chan_eq[3]  = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE)  & 0x01);
	sym_lock[0] = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_SYM_LOCK_DONE) & 0x01);
	sym_lock[1] = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_SYM_LOCK_DONE) & 0x01);
	sym_lock[2] = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_SYM_LOCK_DONE) & 0x01);
	sym_lock[3] = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_SYM_LOCK_DONE) & 0x01);

	cr_done          =  clk_rec[0] +  clk_rec[1] +  clk_rec[2] +  clk_rec[3];
	channel_eq_done  =  chan_eq[0] +  chan_eq[1] +  chan_eq[2] +  chan_eq[3];
	symbol_lock_done = sym_lock[0] + sym_lock[1] + sym_lock[2] + sym_lock[3];

	lane_align_done = ((aux_data[2] >> BIT_DPCD_LANE_ALIGNMENT_DONE) & 0x01);

	eDP_P_DBG(edp, port,
		"%s: CR:[%d %d %d %d] EQ:[%d %d %d %d] SymLock:[%d %d %d %d] Align:%d",
		__func__, clk_rec[0],  clk_rec[1],  clk_rec[2],  clk_rec[3],
		chan_eq[0],  chan_eq[1],  chan_eq[2],  chan_eq[3],
		sym_lock[0], sym_lock[1], sym_lock[2], sym_lock[3], lane_align_done);

	if (cr_done != lane_cnt)
		return 1;
	if (channel_eq_done == lane_cnt && symbol_lock_done == lane_cnt && lane_align_done)
		return 0;

	return 2;
}

static int dptx_run_channel_equalization_loop(struct edptx_drv_s *edp)
{
	int i = 0, ret = -1;
	u8 port, res = 0;

	edptx_set_pattern_to_all_port(edp, eDPTX_TRP_EQ, 0, 0, 0);

	//channel equalization & symbol lock loop
	while (i++ < DP_TRAINING_EQ_ATTEMPTS) {
		res = 0;
		for (port = 0; port < eDPTX_MAX_PORT; port++) {
			if (!(edp->sink.port_mask & BIT(port)))
				continue;
			dptx_training_phy_update_apply(edp, port);

			edptx_delay_us(dptx_train_rd_intv[edp->sink.link[port].cap.train_rd_intv]);
			ret = dptx_check_channel_equalization(edp, port);
			if (ret < 0)
				eDP_P_ERR(edp, port, "%s: aux error at loop[%u]", __func__, i);
			res |= (ret ? 0 : 1) << port;
		}

		if (res == edp->sink.port_mask)
			return 0;

		for (port = 0; port < eDPTX_MAX_PORT; port++) {
			if (!(edp->sink.port_mask & BIT(port)))
				continue;
			//if ((edp->config.control.edp_cfg.curr_vswing[0] & 0x03) !=
			//		VAL_DP_std_LEVEL_3)
			//!!!!! judged by 4 lane !!!!!!!
			//five times?
			edptx_training_phy_adj_req_process(edp, port);
		}
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		if (res & (1 << port))
			edp->sink.link[port].link_rate_update = 2;
	}

	return res;
}

static int edptx_check_cr(struct edptx_drv_s *edp, u8 port)
{
	unsigned char cr_done[4], aux_data[2] = {0, 0};
	unsigned char lane_cnt, cr_all_done = 0;
	int ret;

	lane_cnt = edp->sink.link[port].lane_count;

	ret = edptx_if_aux_read(edp, port, DPCD_LANE0_1_STATUS, 2, aux_data);
	if (ret) { //clear cr_done flags on failure of AUX transaction
		eDP_P_ERR(edp, port, "%s: aux read failed", __func__);
		return -1;
	}

	cr_done[0] = ((aux_data[0] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE) & 0x01);
	cr_done[1] = ((aux_data[0] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);
	cr_done[2] = ((aux_data[1] >> BIT_DPCD_LANE_02_STATUS_CLK_REC_DONE) & 0x01);
	cr_done[3] = ((aux_data[1] >> BIT_DPCD_LANE_13_STATUS_CLK_REC_DONE) & 0x01);

	cr_all_done = cr_done[0] + cr_done[1] + cr_done[2] + cr_done[3];

	eDP_P_PR(edp, port, "%s: CR result: [%d, %d, %d, %d]", __func__,
		cr_done[0], cr_done[1], cr_done[2], cr_done[3]);

	if (cr_all_done == lane_cnt)
		return 0;

	return 1;
}

static int edptx_run_clk_recovery_loop(struct edptx_drv_s *edp)
{
	int ret;
	u8 i = 0, port, res;

	edptx_set_pattern_to_all_port(edp, eDPTX_TRP_CR, 0, 0, 0);

	while (i++ < DP_TRAINING_CR_ATTEMPTS) {
		res = 0;
		for (port = 0; port < eDPTX_MAX_PORT; port++) {
			if (!(edp->sink.port_mask & BIT(port)))
				continue;
			dptx_training_phy_update_apply(edp, port);

			edptx_delay_ms(DP_CLOCK_REC_TIMEOUT); //wait for clock recovery
			ret = edptx_check_cr(edp, port); //0=success
			if (ret < 0)
				eDP_P_ERR(edp, port, "%s: aux error at loop[%u]", __func__, i);
			res |= (ret ? 0 : 1) << port;
		}

		if (res == edp->sink.port_mask)
			return 0;

		for (port = 0; port < eDPTX_MAX_PORT; port++) {
			if (!(edp->sink.port_mask & BIT(port)))
				continue;
			// TODO: check for max vswing level
			//if ((edp->config.control.edp_cfg.preset_vswing_rx[0] & 0x07) != 0x07)
			//max Vod or same  5 times?
			edptx_training_phy_adj_req_process(edp, port);
		}
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		if (res & (1 << port))
			edp->sink.link[port].link_rate_update = 2;
	}

	return res;
}

static int edptx_link_config_update(struct edptx_drv_s *edp)
{
	u8 port;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;

		if (edp->sink.link[port].link_rate_update != 1) {
			edp->sink.link[port].link_rate_update = 0;
			continue;
		}

		edptx_clk_set_link_clk(edp, port, edp->sink.link[port].link_rate);

		edptx_set_lane_config(edp, port);
	}

	return 0;
}

static int edptx_run_training_loop(struct edptx_drv_s *edp, unsigned int retry_cnt)
{
	unsigned int bit_rate_adaptive = 1, link_rate_retry_cnt = 0;
	unsigned int state = DP_TRAINING_CLOCK_REC;
	int ret;
	u8 j, port;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		for (j = 0; j < 4; j++)	{
			edp->sink.link[port].curr_ds[j]    = 0;
			edp->sink.link[port].adj_req_ds[j] = 0;
		}
	}

	//enter training loop
	while (!(state == DP_TRAINING_SUCCESS || state == DP_TRAINING_FAILED)) {
		if (retry_cnt++ > 5) {
			state = DP_TRAINING_FAILED;
			break;
		}
		switch (state) {
		case DP_TRAINING_CLOCK_REC:
		/* *************************************
		 * the initial state performs clock recovery.
		 * if successful, the training sequence moves on to symbol lock.
		 * if clock recovery fails, the training loop exits.
		 */
			eDP_DBG(edp, "---- CLOCK_REC @ %d ----", retry_cnt);
			ret = edptx_run_clk_recovery_loop(edp);
			if (ret == 0) {
				state = DP_TRAINING_CHANNEL_EQ;
				break;
			}

			eDP_PR(edp, "%s: CR failed", __func__);
			if (bit_rate_adaptive && ret == 1)
				state = DP_TRAINING_ADJ_SPD_CR_FAIL;
			else
				state = DP_TRAINING_FAILED;
			break;
		case DP_TRAINING_CHANNEL_EQ:
		/* *************************************
		 * once clock recovery is complete, perform symbol lock and channel equalization.
		 * if this state fails, then we can adjust the link rate.
		 */
			eDP_DBG(edp, "---- CHANNEL_EQ @ %d ----", retry_cnt);
			ret = dptx_run_channel_equalization_loop(edp);
			if (ret == 0) {
				state = DP_TRAINING_SUCCESS;
				break;
			}
			eDP_PR(edp, "%s: channel EQ failed", __func__);
			if (bit_rate_adaptive && ret == 1)
				state = DP_TRAINING_ADJ_SPD_CR_FAIL_IN_EQ;
			else if (bit_rate_adaptive && ret == 2)
				state = DP_TRAINING_ADJ_SPD_EQ_FAIL_OVERTIME;
			else
				state = DP_TRAINING_FAILED;
			break;
		case DP_TRAINING_ADJ_SPD_CR_FAIL:
		case DP_TRAINING_ADJ_SPD_CR_FAIL_IN_EQ:
		case DP_TRAINING_ADJ_SPD_EQ_FAIL_OVERTIME:
		/* *************************************
		 * when allowed by the function parameter, adjust the link speed and
		 *   attempt to retrain the link starting with clock recovery.
		 * the state of the state variable should not be changed in this state
		 *   allowing a failure condition to report the proper state.
		 */
			eDP_DBG(edp, "---- ADJ_SPD @ %d ----", retry_cnt);
			ret = dptx_link_rate_config_reduce(edp);
			edptx_link_config_update(edp);
			if (ret)
				link_rate_retry_cnt++;
			//dptx_phy_config_reinit(edp);
			if (link_rate_retry_cnt == DP_TRAINING_LINKRATE_ATTEMPTS) {
				state = DP_TRAINING_FAILED;
				break;
			}
			state = DP_TRAINING_CLOCK_REC;
		}
	}

	//training pattern off
	edptx_set_pattern_to_all_port(edp, eDPTX_TPS_DISABLE, 0, 0, 0);

	if (edptx_print_level >= LOG_I)
		dptx_training_status_print(edp);
	if (state == DP_TRAINING_SUCCESS)
		return 0;

	return -1;
}

int edptx_full_lt(struct edptx_drv_s *edp)
{
	int ret;
	u8 aux_data[4] = {0, 0, 0, 0}, port, training_done = 0, retry_cnt = 0;

	while (training_done == 0) {
		if (retry_cnt > DP_FULL_LINK_TRAINING_ATTEMPTS)
			break;
		ret = edptx_run_training_loop(edp, retry_cnt);
		if (ret == 0)
			training_done = 1;
		retry_cnt++;
	}

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		memset(aux_data, 0, 4 * sizeof(u8));
		ret = edptx_if_aux_read(edp, port, DPCD_TRAINING_SCORE_LANE0, 4, aux_data);
		eDP_P_PR(edp, port, "%s: result: [0x%02x, 0x%02x, 0x%02x, 0x%02x]", __func__,
			aux_data[0], aux_data[1], aux_data[2], aux_data[3]);
	}
	if (training_done) {
		eDP_PR(edp, "%s: ok", __func__);
		return 0;
	}

	eDP_ERR(edp, "%s: failed", __func__);
	return -1;
}

/* *************** fast link training ****************** */
int edptx_fast_lt(struct edptx_drv_s *edp)
{
	u8 ret = 0, port;

	//set training pattern for CR
	edptx_set_pattern_to_all_port(edp, eDPTX_TRP_CR, 0, 0, 0);
	edptx_delay_ms(1);

	//set training pattern for EQ
	edptx_set_pattern_to_all_port(edp, eDPTX_TRP_EQ, 0, 0, 0);
	edptx_delay_ms(1);

	// check result
	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		if (dptx_check_channel_equalization(edp, port)) // 0 on success
			ret++;
	}

	//disable the training pattern
	edptx_set_pattern_to_all_port(edp, eDPTX_TPS_DISABLE, 0, 0, 0);

	eDP_PR(edp, "%s: res=%u", __func__, ret);

	return ret;
}

/* ******************* link training ********************* */
// multi port will run training simultaneously
int edptx_link_training(struct edptx_drv_s *edp)
{
	int ret;
	u8 port;

	for (port = 0; port < eDPTX_MAX_PORT; port++) {
		if (!(edp->sink.port_mask & BIT(port)))
			continue;
		edptx_clk_set_link_clk(edp, port, edp->sink.link[port].link_rate);
	}

	switch (edp->panel_data.eDP.training_mode) {
	case DPTX_FAST_LINK_TRAINING:
		ret = edptx_fast_lt(edp);
		break;
	case DPTX_FULL_LINK_TRAINING:
		ret = edptx_full_lt(edp);
		break;
	case DPTX_LINK_TRAINING_AUTO:
	default:
		ret = edptx_fast_lt(edp);
		if (ret)
			ret = edptx_full_lt(edp);
		break;
	}

	return ret;
}
