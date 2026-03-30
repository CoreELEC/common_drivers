/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_HW_OPCODE_H
#define __DPTX_HW_OPCODE_H

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>
#include "cea861.h"

/* special offset for DPTX */
#define CMD_DPTX_OFFSET				(0x1 << 20)

/* 0x10 DDC */
enum cmd_dptx_aux {
	/* dptx ddc start */
	DP_AUX_CMD = CMD_DDC_AUX_OFFSET + CMD_DPTX_OFFSET,
	DP_AUX_PIN_MUX_OP,
	DP_AUX_BLK_MODE_SET
};

/* 0x11 HDCP */
enum cmd_dptx_hdcp {
	/* dptx hdcp start */
	DP_HDCP_CMD = CMD_HDCP_OFFSET + CMD_DPTX_OFFSET,
	DP_HDCP_INIT,
	DP_HDCP_UNINIT,
	DP_HDCP1X_AUTH_EN,
	DP_HDCP1X_REAUTH,
	DP_HDCP1X_R0_PRIME_READY,
	DP_HDCP1X_KSV_LIST_READY
};

/* 0x12 packet */
enum cmd_dptx_pkt {
	/* dp aux pkt start */
	DP_AUX_PKT_CMD = CMD_AUX_PKT_OFFSET + CMD_DPTX_OFFSET,
};

/* 0x13 audio */
enum cmd_dptx_aud {
	/* dpts aud start */
	DP_AUDIO_CMD = CMD_AUX_AUD_OFFSET + CMD_DPTX_OFFSET,
};

/* 0x16 DSC */
enum cmd_dptx_dsc {
	/* dptx dsc start */
	DP_DSC_CMD = CMD_DSC_OFFSET + CMD_DPTX_OFFSET,
};

/* 0x17 VRR/adaptive_sync */
enum cmd_dptx_adaptive_sync {
	/* dptx dsc start */
	DP_ADAPTIVE_SYNC_CMD = CMD_VRR_OFFSET + CMD_DPTX_OFFSET,
	DP_ADAPTIVE_SYNC_EN,
	DP_ADAPTIVE_SYNC_DIS
};

/* 0x18 core misc */
enum cmd_dptx_core_misc {
	/* dptx start */
	DP_CORE_MISC_CMD = CMD_CORE_MISC_OFFSET + CMD_DPTX_OFFSET,
	DP_TIMER_INIT,
	DP_TIMER_UNINIT,
	DP_TIMER_GET,
	DP_TIMER_START,
	DP_TIMER_STOP,
	DP_HPD_OVER,
	DP_GET_HPD_STATE,
	DP_GET_HPD_EVENT_STATE,
	DP_SET_IRQ_MASK,
	EDP_REG_STORE_DATA,
	EDP_REG_GET_DATA,
	DP_GTC_INIT,
	DP_GTC_START,
	DP_GTC_STOP,
	EDP_SET_MSA,
	DP_SCRAMBLE_DISABLE,
	EDP_ALTERNATE_SCRAMBLE_RESET,
	EDP_TRANSMITTER_INIT,
	DP_TRANSMITTER_OUTPUT_EN
};

/* 0x19 CMD_PLATFORM */
enum cmd_dptx_platform {
	/* dptx start */
	DP_PLATFORM_CNTL_CMD = CMD_PLATFORM_CNTL_OFFSET + CMD_DPTX_OFFSET,
};

/* 0x1b mode flow misc */
enum cmd_dptx_mode_flow {
	DP_MODE_FLOW_CMD = CMD_MODE_FLOW_MISC_OFFSET + CMD_DPTX_OFFSET,
};

/* 0x1c dptx link training */
enum cmd_link_training {
	/* dptx training start */
	DP_TRAINING_CMD = CMD_LINK_TRAINING_OFFSET + CMD_DPTX_OFFSET,
	LINKCONF_INIT_TPS1,
	LINKCONF_ENABLE_LINK,
	LINKCONF_ENABLE_SRC_VID,
	LINKCONF_ENABLE_ENHANCED_FRAME,
	LINKCONF_SET_LINK_RATE,
	LINKCONF_SET_LANE_COUNT,
	LINKCONF_SET_DOWNSPREAD,
	LINKCONF_SET_8B10B_CODING,
	LINKCONF_SET_TRAIN_PATTERN,
	LINKCONF_SET_TRAIN_PATTERN_VALUE,
	LINKCONF_SET_VSWING_PREEMP,
	LINKCONF_SAVE_LINK_RATE,
	LINKCONF_SAVE_LANE_COUNT,
	LINKCONF_GET_LINK_RATE,
	LINKCONF_GET_LANE_COUNT,
	LINKCONF_SET_QUAL_PATTERN,
	LINKCONF_SET_80BIT_CUSTOM
};

struct mst_conf_vcps_param {
	u32 vc_id;
	u32 pixel_clock;
	enum colorimetry_format cs;
	color_depth_t cd;
};

enum cmd_dptx_mst_ctrl {
	/* MST control */
	DP_MST_CMD = CMD_MST_OFFSET + CMD_DPTX_OFFSET,
	DP_MST_INIT,
	DP_MST_ALLOC_PAYLOAD,
	DP_MST_UPDATE_TABLE,
	DP_MST_0_TO_1,
	DP_MST_1_TO_0,
	DP_MST_COMPARE_0_1,
};

enum map_addr_idx_e {
	DPTX_COR_REG_IDX,
	SYSCTRL_REG_IDX,
	ANACTRL_REG_IDX,
	PWRCTRL_REG_IDX,
	RESETCTRL_REG_IDX,
	CLKCTRL_REG_IDX,
	VPUCTRL_REG_IDX,
	PADCTRL_REG_IDX,
	REG_IDX_MAX,
};

struct reg_access {
	/* 0: read, 1: write, 2: read & dump */
	u8 rd_wr_type;
	enum map_addr_idx_e reg_type;
	/* start and end address for read & dump case */
	u32 addr;
	u32 val;
};

#endif
