/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_LINK_H__
#define __DPTX_LINK_H__

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_format_para.h>
#include "dptx_aux_helper.h"

enum dptx_phy_vswing_e {
	DPTX_PHY_VSWING_UNKNOWN = -1,
	DPTX_PHY_VSWING_400 = 0,
	DPTX_PHY_VSWING_600 = 1,
	DPTX_PHY_VSWING_800 = 2,
	DPTX_PHY_VSWING_1200 = 3,
	DPTX_PHY_VSWING_MAX,
};

enum dptx_phy_preemphasis_e {
	DPTX_PHY_PREEMP_UNKNOWN = -1,
	DPTX_PHY_PREEMP_0DB = 0,
	DPTX_PHY_PREEMP_3DB = 1,
	DPTX_PHY_PREEMP_6DB = 2,
	DPTX_PHY_PREEMP_9DB = 3,
	DPTX_PHY_PREEMP_MAX,
};

enum dptx_phy {
	DPTX_PHY_DPRX,
	DPTX_PHY_LTTPR1,
	DPTX_PHY_LTTPR2,
	DPTX_PHY_LTTPR3,
	DPTX_PHY_LTTPR4,
	DPTX_PHY_LTTPR5,
	DPTX_PHY_LTTPR6,
	DPTX_PHY_LTTPR7,
	DPTX_PHY_LTTPR8,
	DPTX_MAX_LTTPR_COUNT = DPTX_PHY_LTTPR8,
};

/*
 * for LINKCONF_SET_VSWING_PREEMP
 * set the phy vswing, pre-emphasis
 */
struct phy_vswing_preemp {
	enum dptx_phy_vswing_e vswing;
	enum dptx_phy_preemphasis_e preemp;
};

struct dptx_link_param_s {
	enum dp_link_rate_e link_rate;
	enum dp_lane_count_e lane_count;
	enum dptx_phy_vswing_e vswing;
	enum dptx_phy_preemphasis_e preemp;
};

/* Test Pattern Symbol mode
 * the link training pattern according to the two-bit code
 * 000 = Training off
 * 001 = Training pattern 1, clock recovery
 * 010 = Training pattern 2, inter-lane alignment and symbol recovery
 * 011 = Training pattern 3, inter-lane alignment and symbol recovery
 * 100 = Training pattern 4, inter-lane alignment and symbol recovery
 */
enum tps_mode_e {
	TPS_TRAINING_OFF,
	TPS_MODE_1,
	TPS_MODE_2,
	TPS_MODE_3,
	TPS_MODE_4,
};

enum link_pattern_e {
	LINK_PATTERN_OFF     = 0x00ul,
	LINK_PATTERN_D10_2   = 0x24ul,
	LINK_PATTERN_SYM_ERR = 0x08ul,
	LINK_PATTERN_PRBS7   = 0x2cul,
	LINK_PATTERN_1       = 0x21ul,
	LINK_PATTERN_2       = 0x22ul,
	LINK_PATTERN_3       = 0x23ul,
	LINK_PATTERN_4       = 0x07ul,
};

#define LINK_TRAIN_MAX_TIME (2000) /* the maximum time/ms of link training */
#define LINK_CR_MAX_CT     10
#define LINK_CR_SEC_CT     5
#define LINK_EQ_MAX_CT     5

enum link_status_e {
	/* status OK */
	LINK_ST_OK,
	/* initial status */
	LINK_ST_INIT,
	/* training failed */
	LINK_ST_TRAINING_FAIL,
	/* clock recovery init */
	LINK_ST_CR_INIT,
	/* clock recovery re-run */
	LINK_ST_CR_RERUN,
	/* clock recovery enter */
	LINK_ST_CR_PROC,
	/* clock recovery update */
	LINK_ST_CR_UPDATE,
	/* clock recovery training downshift required */
	LINK_ST_CR_DOWNSHIFT,
	/* clock recovery ran max count times */
	LINK_ST_CR_MAX_COUNT,
	/* clock recovery training phase failed */
	LINK_ST_CR_FAIL,
	/* clock recovery training phase passed */
	LINK_ST_CR_PASS,
	/* channel equalization enter */
	LINK_ST_EQ_PROC,
	/* channel equalization re-run */
	LINK_ST_EQ_RERUN,
	/* channel equalization update */
	LINK_ST_EQ_UPDATE,
	/* channel equalization ran max count times */
	LINK_ST_EQ_MAX_COUNT,
	/* channel equalization training phase failed */
	LINK_ST_EQ_FAIL,
	/* channel equalization training phase passed */
	LINK_ST_EQ_PASS,
};

struct dptx_link_cfg {
	/* DPCD version */
	u8 version;
	/* current link parameters */
	struct dptx_link_param_s curr;
	/* base link parameters */
	struct dptx_link_param_s base;
	/* test link parameters */
	struct dptx_link_param_s test;
	/* Link Status/Adjust Request read interval */
	u8 aux_rd_interval;
	/* Lane 0, 1, 2, 3 Status */
	u8 lane01_status;
	u8 lane23_status;
	/* VSwing / EQ Adjust Request for Lanes 0, 1, 2, 3 */
	u8 adj_req_lane01;
	u8 adj_req_lane23;
	/* TPS mode */
	enum tps_mode_e tps_mode;
	/* selected link pattern */
	enum link_pattern_e link_pattern;
	/* complete link status */
	enum link_status_e link_status;
	/* clock recovery timer in ms */
	u32 clk_rec_tmr;
	/* clock recovery timer in ms */
	u32 chan_eq_tmr;
	/* training pattern 3 support */
	bool tps3_support;
	/* enhanced framing mode */
	bool enhanced_frame_cap;
	/* training pattern 4 support */
	bool tps4_support;
	/* down-spread */
	bool downspread;
	/* 8-bit/10-bit coding */
	bool coding_8b10b;
	/* enhanced framing mode */
	bool efm_state;
	/* attempted times of CR or EQ */
	u32 max_test_count;
	/* secondary test count */
	u32 sec_test_count;
	/* dpcd status value */
	u8 dpcd[16];
};

struct link_train_t {
	struct dptx_link_cfg *link_cfg;
	enum link_status_e link_st;
	bool train_running;
	/* link training timeout period, unit: ms */
	u32 timeout_ms;
	struct dptx_aux *aux;
	struct dptx_common *tx_comm;
	/* force link rate and lane count for debug */
	enum dp_link_rate_e force_lr;
	enum dp_lane_count_e force_lc;

	/* link training retry times if training fail */
	u8 lt_retry_cnt;
};

/*
 * Function: dptx_link_training
 *   the dptx training processing
 * Return:
 *   0: successful
 *   -EAGAIN: may try again
 */
int dptx_link_training(struct link_train_t *link_train);
struct link_train_t *dptx_link_train_init(struct dptx_common *tx_comm);
int dptx_link_train_uninit(struct link_train_t *link_train);
int dptx_update_link_fmt_para(struct dptx_common *tx_comm, struct meson_tx_format_para *para);

#endif
