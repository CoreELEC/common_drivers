// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/component.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/phy/phy.h>

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/phy/meson_tx_phy_core.h>
#include <dptx_aux_helper.h>
#include <dpcd_reg.h>
#include <dptx_log.h>
#include "dptx_link.h"
#include "dptx_hw_opcode.h"

/*
 * before the link training, prepare the training parameters
 * such as link rate, lane count, ... etc
 *
 * Function: link_training_prepare
 * Reset DisplayPort transmitter configuration
 * Updates device database with information from
 * Receiver Capability DPCD block (0x00..0x10)
 */
static void link_training_prepare(struct link_train_t *lt)
{
	struct dptx_link_param_s *param;
	struct dptx_link_cfg *lc;
	enum dp_lane_count_e max_lane_count = DPTX_LANE_COUNT_UNKNOWN;
	u8 *dpcd;

	if (!lt)
		return;

	lc = lt->link_cfg;
	param = &lt->link_cfg->curr;
	param->vswing = DPTX_PHY_VSWING_400;
	param->preemp = DPTX_PHY_PREEMP_0DB;
	dpcd = lt->tx_comm->base.dpcd_buf;
	/* retrieve necessary / useful information from DPCD */
	lc->version = dpcd[DP_DPCD_REV];
	param->link_rate = min(param->link_rate, dpcd[DP_MAX_LINK_RATE]);
	switch (dpcd[DP_MAX_LANE_COUNT] & DP_MAX_LANE_COUNT_MASK) {
	case 0x1:
		max_lane_count = DPTX_LANE_COUNT_1;
		break;
	case 0x2:
		max_lane_count = DPTX_LANE_COUNT_2;
		break;
	case 0x4:
		max_lane_count = DPTX_LANE_COUNT_4;
		break;
	default:
		DPTX_ERROR("DPCD MAX_LANE_COUNT[0x%x] error\n", dpcd[DP_MAX_LANE_COUNT]);
		max_lane_count = DPTX_LANE_COUNT_4;
		break;
	}
	param->lane_count = min(param->lane_count, max_lane_count);
	lc->aux_rd_interval = dpcd[DP_TRAINING_AUX_RD_INTERVAL] & DP_TRAINING_AUX_RD_MASK;
	lc->clk_rec_tmr = 1; /* should be 100us, but must be set to 1ms */
	/* refer to Spec1.4 Table 2-158 */
	switch (lc->aux_rd_interval) {
	case 0:
	case 1:
		lc->chan_eq_tmr = 4;
		break;
	case 2:
		lc->chan_eq_tmr = 8;
		break;
	case 3:
		lc->chan_eq_tmr = 12;
		break;
	case 4:
	default:
		lc->chan_eq_tmr = 16;
		break;
	}
	lc->tps3_support = !!(dpcd[DP_MAX_LANE_COUNT] & DP_TPS3_SUPPORTED);
	lc->enhanced_frame_cap = !!(dpcd[DP_MAX_LANE_COUNT] & DP_ENHANCED_FRAME_CAP);
	if (lc->version >= DP_DPCD_REV_14)
		lc->tps4_support = !!(dpcd[DP_MAX_DOWNSPREAD] & DP_TPS4_SUPPORTED);
	lc->downspread = dpcd[DP_MAX_DOWNSPREAD] & DP_MAX_DOWNSPREAD_0_5;
	lc->coding_8b10b = dpcd[DP_MAIN_LINK_CHANNEL_CODING] & DP_CAP_ANSI_8B10B;
}

/* Start LANEx_CR_DONE
 * Write the following Link Configuration parameters:
 *   LINK_BW_SET register
 •   LANE_COUNT_SET field in the LANE_COUNT_SET register
 •   DOWNSPREAD_CTRL register
 •   MAIN_LINK_CHANNEL_CODING_SET register
 * by using the AUX write request transaction
 *
 * Function: dptx_training_config_port
 * This function sync's DPTX and DPCD space with a minimum of aux-channel
 * writes. This function configures the following:
 * - Set DPTX TR_DPTX_ENHANCED_FRAME_EN (this is most likely set).
 * - Set Link Rate (DPTX and DPCD)
 * - Set Lane Count (DPTX and DPCD). Will also write EFM to DPCD.
 * - Set down-spread control (DPTX and DPCD).
 * - Set main link coding to 1 (DPCD only).
 */
static void dptx_training_config_port(struct link_train_t *lt)
{
	struct dptx_common *tx_comm = lt->tx_comm;
	struct dptx_link_cfg *lc = lt->link_cfg;
	bool enable = true;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_ENABLE_LINK, &enable, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_ENABLE_SRC_VID, &enable, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_ENABLE_ENHANCED_FRAME,
		&lc->enhanced_frame_cap, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_LINK_RATE,
		&lc->curr.link_rate, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_LANE_COUNT,
		&lc->curr.lane_count, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_DOWNSPREAD,
		&lc->downspread, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_8B10B_CODING,
		&lc->coding_8b10b, NULL);
}

/*
 * initialise dptx phy driver
 *
 * Function: initialise_phy_drive
 * Initialize the voltage swing and preemphasis
 */
static void initialise_phy_drive(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;
	struct meson_tx_hw *tx_hw;
	int i;

	lc->curr.vswing = DPTX_PHY_VSWING_400;
	lc->curr.preemp = DPTX_PHY_PREEMP_0DB;

	tx_hw = &lt->tx_comm->hw_comm->hw_base;
	for (i = 0; i < 4; i++) {
		tx_hw->tx_phy_conf.dp_ops.voltage[i] = DPTX_PHY_VSWING_400;
		tx_hw->tx_phy_conf.dp_ops.pre[i] = DPTX_PHY_PREEMP_0DB;
	}
	meson_tx_phy_configure(tx_hw->tx_phy, &tx_hw->tx_phy_conf);
}

/*
 * Transmit TPS1 over Main-Link(minimum voltage swing and no pre-emphasis unless optimal
 * setting is already known) and set the Training Pattern value to 1, using an AUX write
 * request transaction
 *
 * Function: dptx_training_clk_rec_init
 * Sets test pattern in hardware
 * Sets drive parameters in hardware
 * Sends test pattern and drive parameters to receiver
 */
static void dptx_training_clk_rec_init(struct link_train_t *lt)
{
	u8 dpcd_buff[5];
	u8 dptx_vs, dptx_pe;
	struct dptx_common *tx_comm = lt->tx_comm;
	struct dptx_link_cfg *lc = lt->link_cfg;

	memset(dpcd_buff, 0, sizeof(dpcd_buff));   /* clear the temporary dpcd buffer before use */
	lc->sec_test_count = 0ul;	    /* reset secondary test count */

	/* set dptx hardware as training pattern sequence 1 */
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_INIT_TPS1, NULL, NULL);
	initialise_phy_drive(lt);			      /* set drive into hardware */

	dptx_vs = (uint8_t)lc->curr.vswing;   /* get current voltage swing */
	dptx_pe = (uint8_t)lc->curr.preemp;   /* get current preemphasis */

	dpcd_buff[0] = DP_TRAINING_PATTERN_1 | DP_LINK_SCRAMBLING_DISABLE;
	dpcd_buff[1] = (dptx_pe << 3) | dptx_vs;   /* set values into DPCD buffer */
	dpcd_buff[2] = (dptx_pe << 3) | dptx_vs;
	dpcd_buff[3] = (dptx_pe << 3) | dptx_vs;
	dpcd_buff[4] = (dptx_pe << 3) | dptx_vs;

	dptx_aux_write_dpcd(lt->aux, DP_TRAINING_PATTERN_SET, dpcd_buff, 5);
}

/*
 * Read LANEx_CR_DONE, LANEx_CHANNEL_EQ_DONE, and LANEx_SYMBOL_LOCKED bits,
 * and ADJUST_REQUEST_LANEx_y registers
 *
 * Function: dptx_dpcd_rd_training_status
 * LPM DPCD read function: Reads receiver LPM training status.
 * Blocking status: non-blocking.
 * This function was written to make transition between blocking and
 * non-blocking easy. Currently, this function is blocking.
 * DPCD data:
 * Offset DPCD reg  Description
 * 0      0x202     Lane 0 & 1 Status
 * 1      0x203     Lane 2 & 3 Status
 * 2      0x204     Lane Alignment Status Updated
 * 3      0x205     Sink Status
 * 4      0x206     Adjust Request Lane 0 & 1
 * 5      0x207     Adjust Request Lane 2 & 3
 */
static bool dptx_dpcd_rd_training_status(struct link_train_t *lt)
{
	int ret;
	struct dptx_link_cfg *lc = lt->link_cfg;
	int i = 0;

	ret = dptx_aux_read_dpcd(lt->aux, DP_LANE0_1_STATUS, lc->dpcd,
				DP_LINK_STATUS_SIZE);
	if (ret < 0) {
		DPTX_ERROR("%s dpcd read failed: %d\n", __func__, ret);
	} else {
		for (i = 0; i < DP_LINK_STATUS_SIZE; i++)
			DPTX_DEBUG_PHY("DPCD[0x%x] = 0x%x", DP_LANE0_1_STATUS + i, lc->dpcd[i]);
	}
	return ret;
}

/*
 * determine clock recovery status
 *
 * Function: check_cr_lane
 * This function determines clock recovery status
 */
static bool check_cr_lane(u8 lane01, u8 lane23, u8 lane_ct)
{
	u8 stat_mask = 0;
	u8 lane_mask = (1 << lane_ct) - 1;

	if (lane_ct == 0)
		return false;

	stat_mask |= (lane01 & BIT(0)) ? BIT(0) : 0;
	stat_mask |= (lane01 & BIT(4)) ? BIT(1) : 0;
	stat_mask |= (lane23 & BIT(0)) ? BIT(2) : 0;
	stat_mask |= (lane23 & BIT(4)) ? BIT(3) : 0;
	stat_mask &= lane_mask;

	return (stat_mask == lane_mask) ? true : false;
}

static bool check_cr_any_lane_set(u8 lane01, u8 lane23, u8 lane_ct)
{
	u8 stat_mask = 0;
	u8 lane_mask = (1 << lane_ct) - 1;

	if (lane_ct == 0)
		return false;

	stat_mask |= (lane01 & BIT(0)) ? BIT(0) : 0;
	stat_mask |= (lane01 & BIT(4)) ? BIT(1) : 0;
	stat_mask |= (lane23 & BIT(0)) ? BIT(2) : 0;
	stat_mask |= (lane23 & BIT(4)) ? BIT(3) : 0;
	stat_mask &= lane_mask;

	return stat_mask ? true : false;
}

/*
 * determine channel equalization status
 *
 * Function: check_ce_lane
 * This function determines channel equalization status
 */
static bool check_ce_lane(u8 lane01, u8 lane23, u8 lane_ct)
{
	u8 stat_mask = 0;
	u8 lane_mask = (1 << lane_ct) - 1;

	if (lane_ct == 0)
		return false;

	stat_mask |= (lane01 & BIT(1)) ? BIT(0) : 0;
	stat_mask |= (lane01 & BIT(5)) ? BIT(1) : 0;
	stat_mask |= (lane23 & BIT(1)) ? BIT(2) : 0;
	stat_mask |= (lane23 & BIT(5)) ? BIT(3) : 0;
	stat_mask &= lane_mask;

	return (stat_mask == lane_mask) ? true : false;
}

/*
 * determine symbol lock status
 *
 * Function: check_sl_lane
 * This function calculates symbol lock status
 */
static bool check_sl_lane(u8 lane01, u8 lane23, u8 lane_ct)
{
	u8 stat_mask = 0;
	u8 lane_mask = (1 << lane_ct) - 1;

	if (lane_ct == 0)
		return false;

	stat_mask |= (lane01 & BIT(2)) ? BIT(0) : 0;
	stat_mask |= (lane01 & BIT(6)) ? BIT(1) : 0;
	stat_mask |= (lane23 & BIT(2)) ? BIT(2) : 0;
	stat_mask |= (lane23 & BIT(6)) ? BIT(3) : 0;
	stat_mask &= lane_mask;

	return (stat_mask == lane_mask) ? true : false;
}

/*
 * LANEx_CR_DONE bit(s) all 1s on enabled lanes
 *
 * Function: lt_check_cr_status
 * Test the results of the clock recovery training
 */
static enum link_status_e lt_check_cr_status(struct link_train_t *lt)
{
	int ret;
	struct dptx_link_cfg *lc = lt->link_cfg;
	u8 *dpcd = lc->dpcd;

	lc->lane01_status = dpcd[0];
	lc->lane23_status = dpcd[1];
	lc->adj_req_lane01 = dpcd[4];
	lc->adj_req_lane23 = dpcd[5];
	ret = check_cr_lane(lc->lane01_status, lc->lane23_status, lc->curr.lane_count);
	if (ret)
		return LINK_ST_CR_PASS;
	else
		return LINK_ST_CR_FAIL;
}

/* Check CR loop condition:
 *   #3 AUX_ACK without LANEx_CR_DONE 10 times
 */
static enum link_status_e lt_check_cr_count(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;

	if (lc->max_test_count == LINK_CR_MAX_CT - 1)
		return LINK_ST_CR_MAX_COUNT;

	lc->max_test_count++;
	return LINK_ST_OK;
}

/*
 * Reduce link rate
 *
 * Function: lt_cr_dshift_lr
 * Clock-recovery down-shift link-rate
 */
static void lt_cr_dshift_lr(struct link_train_t *lt)
{
	struct dptx_common *tx_comm = lt->tx_comm;
	enum dp_link_rate_e new_rate = DPTX_LINK_RATE_1P62GHZ;
	enum dp_link_rate_e cur_rate = lt->link_cfg->curr.link_rate;
	enum link_pattern_e link_pattern = LINK_PATTERN_OFF;

	switch (cur_rate) {
	case DPTX_LINK_RATE_8P10GHZ:
		new_rate = DPTX_LINK_RATE_5P40GHZ;
		break;
	case DPTX_LINK_RATE_5P40GHZ:
		new_rate = DPTX_LINK_RATE_2P70GHZ;
		break;
	case DPTX_LINK_RATE_2P70GHZ:
		new_rate = DPTX_LINK_RATE_1P62GHZ;
		break;
	case DPTX_LINK_RATE_1P62GHZ:
	default:
		new_rate = DPTX_LINK_RATE_1P62GHZ;
		break;
	}
	DPTX_DEBUG_PHY("%s downshift link rate to: 0x%x\n", __func__, new_rate);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN,
		&link_pattern, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_LINK_RATE,
		&new_rate, NULL);
	lt->link_cfg->curr.link_rate = new_rate;
	link_pattern = LINK_PATTERN_1;
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN,
		&link_pattern, NULL);
}

/*
 * Reduce lane count matching those lowest-numbered LANEx_CR_DONE lanes
 *
 * Function: lt_cr_dshift_lc
 * Clock-recovery down-shift lane-count
 */
static void lt_cr_dshift_lc(struct link_train_t *lt)
{
	struct dptx_common *tx_comm = lt->tx_comm;
	enum dp_lane_count_e new_lane = DPTX_LANE_COUNT_1;
	struct dptx_link_cfg *lc = lt->link_cfg;
	enum dp_lane_count_e cur_lane = lc->curr.lane_count;

	if (cur_lane == DPTX_LANE_COUNT_4)
		new_lane = DPTX_LANE_COUNT_2;
	else
		new_lane = DPTX_LANE_COUNT_1;
	DPTX_DEBUG_PHY("%s downshift lane count to: 0x%x\n", __func__, new_lane);
	lc->curr.lane_count = new_lane;
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_LANE_COUNT,
		(void *)&new_lane, NULL);
}

/*
 * Set Lane Count to that desired by DPTX (within maximum supported by DPRX)
 * sequence
 *
 * Function: lt_eq_reset_lc
 * Reset lane count to base value
 */
static void lt_eq_reset_lc(struct link_train_t *lt)
{
	struct dptx_common *tx_comm = lt->tx_comm;
	struct dptx_link_cfg *lc = lt->link_cfg;

	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_LANE_COUNT,
		(void *)&lc->base.lane_count, NULL);
	lc->curr.lane_count = lc->base.lane_count;
	DPTX_DEBUG_PHY("%s reset lane count to primary count: 0x%x\n",
		__func__, lc->curr.lane_count);
}

/*
 * reduce Link Rate
 *
 * Function: lt_eq_dshift_lr
 * Down-shift link-rate
 */
static void lt_eq_dshift_lr(struct link_train_t *lt)
{
	struct dptx_common *tx_comm = lt->tx_comm;
	struct dptx_link_cfg *lc = lt->link_cfg;
	enum dp_link_rate_e new_rate = DPTX_LINK_RATE_1P62GHZ;
	enum dp_link_rate_e cur_rate = lt->link_cfg->curr.link_rate;
	enum link_pattern_e pattern = LINK_PATTERN_OFF;

	switch (cur_rate) {
	case DPTX_LINK_RATE_8P10GHZ:
		new_rate = DPTX_LINK_RATE_5P40GHZ;
		break;
	case DPTX_LINK_RATE_5P40GHZ:
		new_rate = DPTX_LINK_RATE_2P70GHZ;
		break;
	case DPTX_LINK_RATE_2P70GHZ:
		new_rate = DPTX_LINK_RATE_1P62GHZ;
		break;
	case DPTX_LINK_RATE_1P62GHZ:
	default:
		new_rate = DPTX_LINK_RATE_1P62GHZ;
		break;
	}
	DPTX_DEBUG_PHY("%s downshift lr to: 0x%x\n", __func__, new_rate);
	/* LINK_PATTERN_OFF */
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN,
		(void *)&pattern, NULL);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_LINK_RATE,
		(void *)&new_rate, NULL);
	lt->link_cfg->curr.link_rate = new_rate;
	switch (lc->curr.link_rate) {
	/* refer to DP1.4a chapter3.5.1.2, If the upstream device
	 * finds the bit set, the upstream device shall write 07h to
	 * the TRAINING_PATTERN_SET register (DPCD Address 00102h)
	 * and transmit TPS4 during the link training
	 * LANEx_CHANNEL_EQ_DONE sequence (regardless of the speed
	 * at which the link is being trained).
	 */
	case DPTX_LINK_RATE_8P10GHZ:
		if (lc->tps4_support)
			pattern = LINK_PATTERN_4;
		break;
	case DPTX_LINK_RATE_5P40GHZ:
		if (lc->tps3_support)
			pattern = LINK_PATTERN_3;
		break;
	default:
		pattern = LINK_PATTERN_2;
		break;
	}
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN,
		(void *)&pattern, NULL);
}

/*
 * reduce Lane count
 *
 * Function: lt_eq_dshift_lc
 * Down-shift lane count
 */
static void lt_eq_dshift_lc(struct link_train_t *lt)
{
	lt_cr_dshift_lc(lt);
}

/*
 * reduce lane count and link rate
 *
 * Function: lt_eq_dshift_lc_lr
 */
static enum link_status_e lt_eq_dshift_lc_lr(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;
	enum link_status_e st = LINK_ST_TRAINING_FAIL;

	switch (lc->curr.lane_count) {
	case DPTX_LANE_COUNT_1:
		/* Already RBR */
		if (lc->curr.link_rate > DPTX_LINK_RATE_1P62GHZ) {
			/* Set Lane Count to that desired by DPTX (within maximum supported
			 * by DPRX), reduce Link Rate, and then return to LANEx_CR_DONE
			 * sequence
			 */
			lt_eq_reset_lc(lt);
			lt_eq_dshift_lr(lt);
			st = LINK_ST_INIT;
		} else {
			/* Already RBR is Yes */
			st = LINK_ST_TRAINING_FAIL;
		}
		break;
	/* 4 lanes enabled, then set to 2 lanes and return to LANEx_CR_DONE sequence */
	case DPTX_LANE_COUNT_4:
	/* 2 lanes enabled, then set to 1 lanes and return to LANEx_CR_DONE sequence */
	case DPTX_LANE_COUNT_2:
		lt_eq_dshift_lc(lt);
		st = LINK_ST_INIT;
		break;
	default:
		st = LINK_ST_TRAINING_FAIL;
		break;
	}
	DPTX_DEBUG_PHY("%s updated lr(%d) lc(%d)\n",
		__func__, lc->curr.link_rate, lc->curr.lane_count);
	return st;
}

/* Check CR loop condition:
 *   #1 Maximum voltage swing reached
 *   #2 AUX_ACK without LANEx_CR_DONE with the same ADJ_REQ five times
 */
static enum link_status_e lt_check_cr_dshift(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;
	enum link_status_e ret = LINK_ST_CR_UPDATE;

	if (lc->curr.vswing == DPTX_PHY_VSWING_1200 ||
	    lc->sec_test_count == LINK_CR_SEC_CT - 1)
		ret = LINK_ST_CR_DOWNSHIFT;
	if (lc->test.vswing == lc->curr.vswing &&
	    lc->test.preemp == lc->curr.preemp)
		lc->sec_test_count++;
	else
		lc->sec_test_count = 1;
	lc->test = lc->curr;

	return ret;
}

/*
 * Increase link rate to the highest needed
 *
 * Function: lt_cr_reset_lr
 * Clock-recovery down-shift link-rate
 */
static void lt_cr_reset_lr(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;
	enum link_pattern_e pattern = LINK_PATTERN_OFF;

	dptx_hw_cntl(&lt->tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN,
		(void *)&pattern, NULL);
	dptx_hw_cntl(&lt->tx_comm->hw_comm->hw_base, LINKCONF_SET_LINK_RATE,
		(void *)&lc->base.link_rate, NULL);
	lc->curr.link_rate = lc->base.link_rate;
	pattern = LINK_PATTERN_1;
	dptx_hw_cntl(&lt->tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN,
		(void *)&pattern, NULL);
}

/*
 * Adjust the drive setting as requested by a Sink device
 *
 * Function: lt_cr_drive_adjust
 * Update Voltage Swing & Pre-emphasis values
 */
static void lt_cr_drive_adjust(struct link_train_t *lt)
{
	u8 i;
	u8 vs[4] = {0};
	u8 pe[4] = {0};
	u8 vs_max, pe_max;
	struct dptx_link_cfg *lc = lt->link_cfg;
	enum dp_lane_count_e lanes = lc->curr.lane_count;

	switch (lanes) {
	case DPTX_LANE_COUNT_1:
		lc->curr.vswing = lc->adj_req_lane01 & 0x3;
		lc->curr.preemp = (lc->adj_req_lane01 >> 2) & 0x3;
		return;
	case DPTX_LANE_COUNT_2:
		vs[0] = lc->adj_req_lane01 & 0x3;
		pe[0] = (lc->adj_req_lane01 >> 2) & 0x3;
		vs[1] = (lc->adj_req_lane01 >> 4) & 0x3;
		pe[1] = (lc->adj_req_lane01 >> 6) & 0x3;
		break;
	case DPTX_LANE_COUNT_4:
		vs[0] = lc->adj_req_lane01 & 0x3;
		pe[0] = (lc->adj_req_lane01 >> 2) & 0x3;
		vs[1] = (lc->adj_req_lane01 >> 4) & 0x3;
		pe[1] = (lc->adj_req_lane01 >> 6) & 0x3;
		vs[2] = lc->adj_req_lane23 & 0x3;
		pe[3] = (lc->adj_req_lane23 >> 2) & 0x3;
		vs[3] = (lc->adj_req_lane23 >> 4) & 0x3;
		pe[3] = (lc->adj_req_lane23 >> 6) & 0x3;
		break;
	default:
		break;
	}

	vs_max = 0;
	pe_max = 0;
	for (i = 0; i < lanes; i++) {
		if (vs[i] > vs_max)
			vs_max = vs[i];
		if (pe[i] > pe_max)
			pe_max = pe[i];
	}
	lc->curr.vswing = vs_max;
	lc->curr.preemp = pe_max;
	DPTX_DEBUG_PHY("%s adjust vswing = 0x%x, preemp = 0x%x\n",
		__func__, vs_max, pe_max);
}

/*
 * Write an updated value to the TRAINING_LANEx_SET registers
 *
 * Function: lt_cr_update
 * Writes current training values for voltage swing and pre-emphasis to
 * both the source (dptx) and sink (dpcd) devices
 */
static void lt_cr_update(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;
	struct phy_vswing_preemp phy = {0};

	phy.vswing = lc->curr.vswing;
	phy.preemp = lc->curr.preemp;
	DPTX_DEBUG_PHY("%s update vswing = 0x%x, preemp = 0x%x\n",
		__func__, phy.vswing, phy.preemp);
	dptx_hw_cntl(&lt->tx_comm->hw_comm->hw_base, LINKCONF_SET_VSWING_PREEMP,
		(void *)&phy, NULL);
}

/* Write an updated value to the TRAINING_LANEx_SET register(s), using AUX CH
 */
static void lt_eq_update(struct link_train_t *lt)
{
	lt_cr_update(lt);
}

/*
 * Starting EQ training
 * Transmit TPS2, 3, or 4 over the Main-Link and write the Training Pattern
 * value equal to the TPS number and the current drive setting to the
 * TRAINING_LANEx_SET register(s), using an AUX write request transaction
 *
 * Turn on the correct test pattern
 */
static void dptx_training_chan_eq_init(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;
	struct dptx_common *tx_comm = lt->tx_comm;
	enum link_pattern_e pattern = LINK_PATTERN_2;

	switch (lc->curr.link_rate) {
	case DPTX_LINK_RATE_8P10GHZ:
	/* refer to DP1.4a chapter3.5.1.2, If the upstream device
	 * finds the bit set, the upstream device shall write 07h to
	 * the TRAINING_PATTERN_SET register (DPCD Address 00102h)
	 * and transmit TPS4 during the link training
	 * LANEx_CHANNEL_EQ_DONE sequence (regardless of the speed
	 * at which the link is being trained).
	 */
		if (lc->tps4_support)
			pattern = LINK_PATTERN_4;
		break;
	case DPTX_LINK_RATE_5P40GHZ:
		if (lc->tps3_support)
			pattern = LINK_PATTERN_3;
		break;
	default:
		pattern = LINK_PATTERN_2;
		break;
	}
	DPTX_DEBUG_PHY("%s pattern = 0x%x\n", __func__, pattern);
	dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SET_TRAIN_PATTERN,
		(void *)&pattern, NULL);
}

/*
 * LANEx_CHANNEL_EQ_DONE, LANEx_SYMBOL_LOCKED, and INTERLANE_ALIGN_DONE
 *
 * Function: lt_check_eq_status
 * This function determines channel equalization status
 */
static enum link_status_e lt_check_eq_status(struct link_train_t *lt)
{
	int ret[3] = {0};
	struct dptx_link_cfg *lc = lt->link_cfg;
	u8 *dpcd = lc->dpcd;

	lc->lane01_status = dpcd[0];
	lc->lane23_status = dpcd[1];
	lc->adj_req_lane01 = dpcd[4];
	lc->adj_req_lane23 = dpcd[5];
	ret[0] = check_ce_lane(lc->lane01_status, lc->lane23_status, lc->curr.lane_count);
	ret[1] = check_sl_lane(lc->lane01_status, lc->lane23_status, lc->curr.lane_count);
	ret[2] = dpcd[2] & DP_INTERLANE_ALIGN_DONE;
	if (ret[0] && ret[1] && ret[2])
		return LINK_ST_EQ_PASS;
	else
		return LINK_ST_EQ_FAIL;
}

/*
 * Increment Loop Count by 1 on AUX_ACK without LANEx_CHANNEL_EQ_DONE, LANEx_SYMBOL_LOCKED,
 * and INTERLANE_ALIGN_DONE
 *
 * Function: lt_check_eq_count
 * Test for max channel equalization count
 */
static enum link_status_e lt_check_eq_count(struct link_train_t *lt)
{
	struct dptx_link_cfg *lc = lt->link_cfg;

	if (lc->max_test_count == LINK_EQ_MAX_CT - 1)
		return LINK_ST_EQ_MAX_COUNT;

	lc->max_test_count++;
	return LINK_ST_OK;
}

static enum link_status_e dptx_train_fsm(struct link_train_t *lt)
{
	int st = 0;
	enum link_status_e ret = 0;
	struct dptx_common *tx_comm = lt->tx_comm;
	struct dptx_link_cfg *lc = lt->link_cfg;
	enum link_pattern_e pattern = LINK_PATTERN_OFF;

	DPTX_INFO("train state: %d", lt->link_st);
	switch (lt->link_st) {
	case LINK_ST_INIT:
		link_training_prepare(lt);
		lt->link_st = LINK_ST_CR_PROC;
		break;
	case LINK_ST_CR_PROC:
		lc->max_test_count = 0;
		dptx_training_config_port(lt);
		lt->link_st = LINK_ST_CR_INIT;
		break;
	case LINK_ST_CR_INIT:
		dptx_training_clk_rec_init(lt);
		/* Wait for delay specified in TRAINING_AUX_RD_INTERVAL register */
		mdelay(lc->clk_rec_tmr);
		lt->link_st = LINK_ST_CR_RERUN;
		break;
	case LINK_ST_CR_RERUN:
		if (dptx_dpcd_rd_training_status(lt) == false) {
			lt->link_st = LINK_ST_TRAINING_FAIL;
			break;
		}
		/* Clock-recovery drive settings loop */
		ret = lt_check_cr_status(lt);
		if (ret == LINK_ST_CR_PASS) {
			/* End CR sequence, proceed to CH_EQ */
			lt->link_st = LINK_ST_EQ_PROC;
			DPTX_INFO("clock recovery pass\n");
			break;
		}
		st = lt_check_cr_count(lt);
		if (st == LINK_ST_CR_MAX_COUNT) {
			int curr_rate = lc->curr.link_rate;
			int curr_count = lc->curr.lane_count;

			/* Already RBR */
			if (curr_rate > DPTX_LINK_RATE_1P62GHZ) {
				/* Reduce link rate */
				lc->max_test_count = 0;
				lt_cr_dshift_lr(lt);
				DPTX_INFO("%s CR > 10times, downshift lr\n", __func__);
				lt->link_st = LINK_ST_CR_INIT;
				break;
			}

			/* Only lowest-numbered LANEx_CR_DONE lanes that
			 * have the LANEx_CR_DONE bit(s) set to 1?
			 */
			if ((curr_count == DPTX_LANE_COUNT_4 &&
				(lc->lane01_status & 0x11) == 0x11 &&
				(lc->lane23_status & 0x11) == 0x0) ||
				(curr_count == DPTX_LANE_COUNT_2 &&
				(lc->lane01_status & 0x11) == 0x1 &&
				(lc->lane23_status & 0x11) == 0x0)) {
				lt_cr_dshift_lc(lt);
				lt_cr_reset_lr(lt);
				lc->max_test_count = 0;
				lt->link_st = LINK_ST_CR_INIT;
				DPTX_INFO("%s CR > 10times, downshift lc\n", __func__);
			} else {
				/* End training */
				DPTX_INFO("%s CR > 10times, no LANEx_CR_DONE bits set\n", __func__);
				lt->link_st = LINK_ST_TRAINING_FAIL;
			}
			break;
		}

		st = lt_check_cr_dshift(lt);
		if (st == LINK_ST_CR_DOWNSHIFT) {
			int curr_rate = lc->curr.link_rate;
			int curr_count = lc->curr.lane_count;

			/* Already RBR */
			if (curr_rate > DPTX_LINK_RATE_1P62GHZ) {
				lc->max_test_count = 0;
				lt_cr_dshift_lr(lt);
				lt->link_st = LINK_ST_CR_INIT;
				DPTX_INFO("%s max swing reached, downshift lr\n", __func__);
				break;
			}

			/* Only lowest-numbered LANEx_CR_DONE lanes that
			 * have the LANEx_CR_DONE bit(s) set to 1?
			 */
			if ((curr_count == DPTX_LANE_COUNT_4 &&
				(lc->lane01_status & 0x11) == 0x11 &&
				(lc->lane23_status & 0x11) == 0x0) ||
				(curr_count == DPTX_LANE_COUNT_2 &&
				(lc->lane01_status & 0x11) == 0x1 &&
				(lc->lane23_status & 0x11) == 0x0)) {
				lt_cr_dshift_lc(lt);
				lt_cr_reset_lr(lt);
				lc->max_test_count = 0;
				lt->link_st = LINK_ST_CR_INIT;
				DPTX_INFO("%s max swing reached, downshift lc\n", __func__);
			} else {
				DPTX_INFO("max swing reached, no LANEx_CR_DONE bits set\n");
				lt->link_st = LINK_ST_TRAINING_FAIL;
			}
			break;
		}
		lt->link_st = LINK_ST_CR_UPDATE;
		break;
	case LINK_ST_CR_UPDATE:
		lt_cr_drive_adjust(lt);
		lt_cr_update(lt);
		mdelay(lc->clk_rec_tmr);
		lt->link_st = LINK_ST_CR_RERUN;
		break;
	case LINK_ST_CR_PASS:
		lt->link_st = LINK_ST_EQ_PROC;
		break;
	case LINK_ST_EQ_PROC:
		dptx_training_chan_eq_init(lt);
		/* Reset Loop Count */
		lc->max_test_count = 0;
		/* Wait for delay specified in TRAINING_AUX_RD_INTERVAL register */
		mdelay(lc->chan_eq_tmr);
		lt->link_st = LINK_ST_EQ_RERUN;
		break;
	case LINK_ST_EQ_RERUN:
		if (dptx_dpcd_rd_training_status(lt) == false) {
			lt->link_st = LINK_ST_TRAINING_FAIL;
			break;
		}

		/* LANEx_CR_DONE bits remain all 1s */
		ret = lt_check_cr_status(lt);
		if (ret == LINK_ST_CR_PASS) {
			ret = lt_check_eq_status(lt);
			/* End EQ sequence, proceed to video mode setting */
			if (ret == LINK_ST_EQ_PASS) {
				DPTX_INFO("%s EQ PASS\n", __func__);
				lt->link_st = LINK_ST_EQ_PASS;
				/* Clear TRAINING_PATTERN_SET byte with AUX wrtie transaction */
				pattern = LINK_PATTERN_OFF;
				dptx_hw_cntl(&tx_comm->hw_comm->hw_base,
					LINKCONF_SET_TRAIN_PATTERN,
					(void *)&pattern, NULL);
				break;
			}
			st = lt_check_eq_count(lt);
			if (st == LINK_ST_EQ_MAX_COUNT) {
				/* Loop Count is Yes */
				st = lt_eq_dshift_lc_lr(lt);
				lt->link_st = st;
				DPTX_INFO("%s EQ > 5times, downshift lc->lr\n", __func__);
				break;
			}
			/* Loop Count is No */
			lt->link_st = LINK_ST_EQ_UPDATE;
			break;
		}
		/* CR done bits not all set, check at least one
		 * LANEx_CR_DONE bit remains set to 1 for Target Lane Count
		 */
		if (check_cr_any_lane_set(lc->lane01_status, lc->lane23_status,
			lc->curr.lane_count)) {
			DPTX_INFO("%s not all CR_done bits set, downshift lc/lr\n", __func__);
			st = lt_eq_dshift_lc_lr(lt);
			lt->link_st = st;
			break;
		} else if (lc->curr.link_rate != DPTX_LINK_RATE_1P62GHZ) {
			DPTX_INFO("%s no LANEx_CR_DONE bit set, downshift lr\n", __func__);
			lt_eq_reset_lc(lt);
			lt_eq_dshift_lr(lt);
			lt->link_st = LINK_ST_INIT;
			break;
		}
		lt->link_st = LINK_ST_TRAINING_FAIL;
		break;
	case LINK_ST_EQ_UPDATE:
		lt_cr_drive_adjust(lt);
		lt_eq_update(lt);
		DPTX_INFO("%s EQ adjust vswing/preemp\n", __func__);
		/* Wait for delay specified in TRAINING_AUX_RD_INTERVAL register */
		mdelay(lc->chan_eq_tmr);
		lt->link_st = LINK_ST_EQ_RERUN;
		break;
	case LINK_ST_EQ_PASS:
		lt->link_st = LINK_ST_EQ_PASS;
		break;
	default:
		lt->link_st = LINK_ST_TRAINING_FAIL;
		break;
	}
	return lt->link_st;
}

int dptx_link_training(struct link_train_t *link_train)
{
	ulong train_timeout;
	enum link_status_e st = LINK_ST_TRAINING_FAIL;
	int timeout_flag = 1;
	struct dptx_common *tx_comm = NULL;

	if (!link_train)
		return -EINVAL;
	tx_comm = link_train->tx_comm;
	memset(link_train->link_cfg, 0, sizeof(*link_train->link_cfg));
	link_train->link_st = LINK_ST_INIT;

	/* use force link rate and lane count with higher priority */
	if (link_train->force_lc > 0)
		link_train->link_cfg->curr.lane_count = link_train->force_lc;
	else
		link_train->link_cfg->curr.lane_count =
			tx_comm->base.sw_fmt_para.tx_hw_para.dptx_hw_para.lane_count;
	if (link_train->force_lr > 0)
		link_train->link_cfg->curr.link_rate = link_train->force_lr;
	else
		link_train->link_cfg->curr.link_rate =
			tx_comm->base.sw_fmt_para.tx_hw_para.dptx_hw_para.link_rate;
	/* save base link configuration for later fallback usage */
	memcpy(&link_train->link_cfg->base, &link_train->link_cfg->curr,
		sizeof(link_train->link_cfg->curr));

	DPTX_INFO("training at link_rate(%d) and lane_count(%d)\n",
		link_train->link_cfg->curr.link_rate, link_train->link_cfg->curr.lane_count);

	train_timeout = jiffies + msecs_to_jiffies(link_train->timeout_ms);
	while (time_before(jiffies, train_timeout)) {
		st = dptx_train_fsm(link_train);
		if (st == LINK_ST_EQ_PASS || st == LINK_ST_TRAINING_FAIL) {
			timeout_flag = 0;
			break;
		}
	}
	if (timeout_flag == 1)
		DPTX_INFO("training time out\n");
	else
		DPTX_INFO("training %s\n", st == LINK_ST_EQ_PASS ? "pass" : "fail");

	/*
	 * save link rate and lane count if force mode enabled,
	 * no matter link training fail(special for PXP) or pass
	 */
	if (link_train->force_lr > 0)
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SAVE_LINK_RATE,
			&link_train->force_lr, NULL);
	if (link_train->force_lc > 0)
		dptx_hw_cntl(&tx_comm->hw_comm->hw_base, LINKCONF_SAVE_LANE_COUNT,
			&link_train->force_lc, NULL);

	if (st == LINK_ST_EQ_PASS)
		return 0;
	else
		return -EAGAIN;
}

int dptx_update_link_fmt_para(struct dptx_common *tx_comm, struct meson_tx_format_para *para)
{
	struct meson_tx_clk *tx_clk = NULL;

	if (!tx_comm || !para)
		return -EINVAL;

	tx_clk = tx_comm->base.tx_hw_base->tx_clk;
	/*
	 * update link rate and lane count if force mode is enabled,
	 * otherwise update with the link rate and lane count which passed link training
	 */
	if (tx_comm->link_train->force_lr >= DPTX_LINK_RATE_1P62GHZ &&
		tx_comm->link_train->force_lr < DPTX_LINK_RATE_MAX)
		para->tx_hw_para.dptx_hw_para.link_rate = tx_comm->link_train->force_lr;
	else
		para->tx_hw_para.dptx_hw_para.link_rate =
			tx_comm->link_train->link_cfg->curr.link_rate;

	if (tx_comm->link_train->force_lc >= DPTX_LANE_COUNT_1 &&
		tx_comm->link_train->force_lc < DPTX_LANE_COUNT_MAX)
		para->tx_hw_para.dptx_hw_para.lane_count = tx_comm->link_train->force_lc;
	else
		para->tx_hw_para.dptx_hw_para.lane_count =
			tx_comm->link_train->link_cfg->curr.lane_count;

	/* update fromat param of dptx instance */
	tx_comm->base.sw_fmt_para.tx_hw_para.dptx_hw_para.link_rate =
		para->tx_hw_para.dptx_hw_para.link_rate;
	tx_comm->base.sw_fmt_para.tx_hw_para.dptx_hw_para.lane_count =
		para->tx_hw_para.dptx_hw_para.lane_count;

	switch (para->tx_hw_para.dptx_hw_para.link_rate) {
	case DPTX_LINK_RATE_1P62GHZ:
		tx_clk->tx_clk_cfg.phy_clk = 1620000;
		break;
	case DPTX_LINK_RATE_2P70GHZ:
		tx_clk->tx_clk_cfg.phy_clk = 2700000;
		break;
	case DPTX_LINK_RATE_5P40GHZ:
		tx_clk->tx_clk_cfg.phy_clk = 5400;
		break;
	case DPTX_LINK_RATE_8P10GHZ:
		tx_clk->tx_clk_cfg.phy_clk = 8100;
		break;
	default:
		DPTX_ERROR("%s invalid link rate, force 1.62Ghz\n", __func__);
		tx_clk->tx_clk_cfg.phy_clk = 1620000;
		break;
	}
	return 0;
}

struct link_train_t *dptx_link_train_init(struct dptx_common *tx_comm)
{
	struct link_train_t *link_train = NULL;

	if (!tx_comm)
		return NULL;

	link_train = kzalloc(sizeof(*tx_comm->link_train), GFP_KERNEL);
	if (!link_train)
		return NULL;
	link_train->link_cfg = kzalloc(sizeof(*tx_comm->link_train->link_cfg), GFP_KERNEL);
	if (!link_train->link_cfg) {
		kfree(link_train);
		return NULL;
	}
	link_train->tx_comm = tx_comm;
	link_train->aux = tx_comm->tx_aux;
	link_train->link_st = LINK_ST_INIT;
	link_train->timeout_ms = LINK_TRAIN_MAX_TIME;
	link_train->force_lr = DPTX_LINK_RATE_UNKNOWN;
	link_train->force_lc = DPTX_LANE_COUNT_UNKNOWN;

	return link_train;
}

int dptx_link_train_uninit(struct link_train_t *link_train)
{
	if (!link_train)
		return -EINVAL;

	kfree(link_train->link_cfg);
	link_train->link_cfg = NULL;
	kfree(link_train);
	link_train = NULL;

	return 0;
}

