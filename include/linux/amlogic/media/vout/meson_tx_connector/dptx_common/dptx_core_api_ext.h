/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_CORE_API_EXT_H
#define __DPTX_CORE_API_EXT_H

#define EDPTX_CORE_IDX	0
#define DPTX_CORE_IDX	1

/* per dp1.4a spec Table 2-94 & Table 2-96 */
struct msa_s {
	u32 m_vid;
	u32 n_vid;

	u16 h_total;
	u16 v_total;
	u8 hsync_pol:1;
	u8 vsync_pol:1;
	u16 hsw;
	u16 vsw;
	u16 h_active;
	u16 v_active;
	u16 hbp;
	u16 vbp;

	/* mics0/1 */
	u8 sync_clk_mode:1;
	/* Unit: 1000, used only under sync_clk mode */
	unsigned int pixel_freq;
	/* 0x6/0xa/0x14/0x1e, used only under sync_clk mode */
	u8 link_rate;
	/* for SRCX_USER_DATA_COUNT calc */
	u8 lane_count;

	u8 interlace_mode:1;
	/* for 3D */
	u8 stereo_video_attr:2;
	/* 0:rgb, 1:y422, 2:y444, 3:reserved */
	u8 color_format:2;
	/* 0: VESA range, 1: CEA range */
	u8 dynamic_range;
	/* 0: BT601, 1: BT709, others: reserved for future */
	u8 ycbcr_colorimetry;
	/* 6/8/10/12/16 bits per color component */
	u8 bit_depth;

	u32 tu_size;
	/* SRC0_TRANSFER_UNIT_CONFIG */
	/* u32 tu_config; */
	/* SRC0_USER_SYNC_POLARITY */
	/* u32 user_sync_polarity; */
	/* SRC0_USER _CONTROL */
	/* u32 user_control; */
	/* SRC0_USER_DATA_COUNT */
	/* u32 user_data_count; */
};

#define DPTX_SCRAMBLE_RESET_OFF              0
#define DPTX_SCRAMBLE_RESET_ON               1
#define DPTX_eDP_ALTERNATIVE_SCRAMBLE_RESET  2

/* for DP core IRQ mask, When the corresponding bit
 * in this register is set to 1, the interrupt is
 * masked and will not generate an interrupt for the event.
 */
#define INTERRUPT_MASK_SRC3_ERR_EVENT				BIT(14)
#define INTERRUPT_MASK_SRC3_OVF_EVENT				BIT(13)
#define INTERRUPT_MASK_SRC2_ERR_EVENT				BIT(12)
#define INTERRUPT_MASK_SRC2_OVF_EVENT				BIT(11)
#define INTERRUPT_MASK_SRC1_ERR_EVENT				BIT(10)
#define INTERRUPT_MASK_SRC1_OVF_EVENT				BIT(9)
#define INTERRUPT_MASK_SRC0_ERR_EVENT				BIT(8)
#define INTERRUPT_MASK_SRC0_OVF_EVENT				BIT(7)
#define INTERRUPT_MASK_MST_TIMER_IRQ_EVENT			BIT(6)
#define INTERRUPT_MASK_HDCP_TIMER_IRQ_EVENT			BIT(5)
#define INTERRUPT_MASK_GP_TIMER_IRQ_EVENT			BIT(4)
#define INTERRUPT_MASK_REPLY_TIMEOUT_EVENT			BIT(3)
#define INTERRUPT_MASK_REPLY_RECEIVED_EVENT			BIT(2)
#define INTERRUPT_MASK_HPD_IRQ_EVENT				BIT(1)
#define INTERRUPT_MASK_HPD_EVENT					BIT(0)

/* AUX CMD */
#define DPTX_AUX_CMD_WRITE            0x8
#define DPTX_AUX_CMD_READ             0x9
#define DPTX_AUX_CMD_I2C_WRITE        0x0
#define DPTX_AUX_CMD_I2C_WRITE_MOT    0x4
#define DPTX_AUX_CMD_I2C_READ         0x1
#define DPTX_AUX_CMD_I2C_READ_MOT     0x5
#define DPTX_AUX_CMD_I2C_WRITE_STATUS 0x2

/* test pattern index */
#define eDPTX_TPS_DISABLE      0
#define eDPTX_TPS1             1
#define eDPTX_TPS2             2
#define eDPTX_TPS3             3
#define eDPTX_TPS4             4
#define eDPTX_QUAL_PAT_DISABLE 5
#define eDPTX_D10P2            6
#define eDPTX_SYMBOL_ERROR_MSR 7
#define eDPTX_PRBS7            8
#define eDPTX_80BIT_CUSTOM     9
#define eDPTX_HBR2_EYE         10
#define eDPTX_CP2520_1         eDPTX_HBR2_EYE
#define eDPTX_CP2520_2         11
#define eDPTX_CP2520_3         12
#define eDPTX_TRP_CR           eDPTX_TPS1
#define eDPTX_TRP_EQ           0xf0

u8 edptx_get_hpd_level(u8 dp_core_idx);
u16 edptx_get_hpd_event(u8 dp_core_idx);
void edptx_set_interrupt_mask(u8 dp_core_idx, u8 mask);

void edptx_reg_store_data(u8 dp_core_idx, uint32_t d0, uint32_t d1);
void edptx_reg_get_data(u8 dp_core_idx, uint32_t *d0, uint32_t *d1);
void edptx_reg_get_link(u8 dp_core_idx, uint32_t *link, u8 *lane);

u8 edptx_aux_write(u8 dp_core_idx, u32 addr, int len, u8 *buf);
u8 edptx_aux_write_single(u8 dp_core_idx, u32 addr, u8 val);
u8 edptx_aux_read(u8 dp_core_idx, u32 addr, int len, u8 *buf);
u8 edptx_aux_i2c_op(u8 dp_core_idx, u8 cmd_type, u32 dev_addr, u8 len, u8 *data);
u8 edptx_aux_read_edid(u8 dp_core_idx, u8 *buf, u16 len);
void edptx_set_msa(u8 dp_core_idx, struct msa_s *msa);
void edptx_set_phy_config_to_core(u8 dp_core_idx, u8 use_preset);
void edptx_set_lane_config_to_core(u8 dp_core_idx, u8 link_rate, u8 lane_count, u8 enh_frame_en);
void edptx_psr1_ctrl_set(u8 dp_core_idx, u8 flag);
void edptx_psr2_ctrl_set(u8 dp_core_idx, u8 flag);
void edptx_set_scramble_reset(u8 dp_core_idx, u8 sr_type);
void edptx_transmitter_init(u8 dp_core_idx);
void edptx_transmitter_output_set(u8 dp_core_idx, u8 en);
void edptx_transmit_pattern(u8 dp_core_idx, u8 pattern,
	u8 lane_mask, u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2);

#endif
