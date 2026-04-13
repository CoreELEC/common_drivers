/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_HW_H__
#define __DPTX_HW_H__

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include <linux/completion.h>

enum hw_io_type {
	TOP_LEVEL,
	CORE_LEVEL,
};

struct tx_task_manager;

struct dptx20_hw {
	struct dptx_hw_common hw_comm;
	/* save un-handled interrupts, clear after interrupt handled */
	u32 intr_state;

	/* for dptx20 private task manager */
	struct tx_task_manager *task_mgr;

	/* true: polling aux intr status mode, false: use aux intr for aux status update */
	bool aux_block_mode;
	struct completion aux_completion;
	/* save aux_status, for aux non-block only */
	u32 aux_status;
};

#define to_dptx20_hw(x) container_of(x, struct dptx20_hw, hw_comm)

#define DP_AUX_XFER_TIMEOUT 50 /* unit: ms */

/* aux transmitter status */
enum dptx_aux_status_t {
	/* no status reported (this shouldn't happen) */
	DPTX_AUX_STATUS_NONE,
	/* status is good, operation success */
	DPTX_AUX_STATUS_OK,
	/* status is good, operation in progress */
	DPTX_AUX_STATUS_IN_PROCESS,
	/* general error condition */
	DPTX_AUX_STATUS_ERROR,
	/* reply error condition */
	DPTX_AUX_STATUS_REPLY_TIMEOUT,
	/* ACK reply status condition */
	DPTX_AUX_STATUS_ACK,
	/* NACK reply status condition */
	DPTX_AUX_STATUS_NACK,
	/* DEFER reply status condition */
	DPTX_AUX_STATUS_DEFER,
	/* I2C ACK reply status condition */
	DPTX_AUX_STATUS_ACK_I2C,
	/* I2C NACK reply status condition */
	DPTX_AUX_STATUS_NACK_I2C,
	/* I2C DEFER reply status condition */
	DPTX_AUX_STATUS_DEFER_I2C,
	/* UNKNOWN reply status condition */
	DPTX_AUX_STATUS_UNKNOWN,
	/* operation resulted in a timeout */
	DPTX_AUX_STATUS_TIMEOUT,
	/* never found an appropriate interrupt */
	DPTX_AUX_STATUS_TIMEOUT_INTERRUPT,
	/* hpd is not asserted */
	DPTX_AUX_STATUS_NOT_CONNECTED,
	/* state machine not idle */
	DPTX_AUX_STATUS_NOT_IDLE,
	/* abort transaction, back to idle */
	DPTX_AUX_STATUS_ABORT,
	/* reset state machine, back to idle */
	DPTX_AUX_STATUS_RESET,
	/* unsupported command */
	DPTX_AUX_STATUS_UNSUPPORTED_CMD,
	/* unknown command */
	DPTX_AUX_STATUS_RESERVED_CMD,
};

int dptx20_reg_write(struct dptx_hw_common *tx_comm, u32 io_type, u32 offset, u32 value);
u32 dptx20_reg_read(struct dptx_hw_common *tx_comm, u32 io_type, u32 offset);
void dptx20_set_reg_bits(struct dptx_hw_common *tx_comm, u32 io_type,
	u32 addr, u32 value, u32 offset, u32 len);
void dptx20_seq_rd_reg(struct dptx_hw_common *tx_comm, u32 io_type, u32 offset, u8 *buf, u16 cnt);

u32 dptx20_rd_plt_reg(struct dptx_hw_common *tx_comm, u32 vaddr);
void dptx20_wr_plt_reg(struct dptx_hw_common *tx_comm, u32 vaddr, u32 val);
void dptx20_set_plt_reg_bits(struct dptx_hw_common *tx_comm, u32 addr,
	u32 value, u32 offset, u32 len);

u32 dptx20_hw_calc_mst_reg(u8 vc_id, u32 reg);

ssize_t dptx_aux_xfer(struct dptx_hw_common *tx_comm, struct dptx_aux_msg *msg);
void dptx_isr_disable(struct dptx_hw_common *tx_comm, u32 flags);
void dptx_isr_enable(struct dptx_hw_common *tx_comm, u32 flags);

void dptx_hw_get_mnvid_clock(struct dptx_hw_common *hw_comm, u8 video_num,
	u32 *pixel_clock, u32 *link_clock);
void dptx_hw_infoframe_send(struct dptx_hw_common *hw_comm, u8 vc_id, u16 info_type, u8 *body);
int dptx_hw_infoframe_raw_get(struct dptx_hw_common *hw_comm, u8 vc_id, u16 info_type, u8 *body);
int dptx_avi_infoframe_config(struct dptx_hw_common *tx_comm, u32 avi_cmd, u8 val);

void dptx_dump_infoframe_packets(struct dptx_hw_common *hw_comm, u8 vc_id);
int dptx_vsc_pkt_for_pixel_enc(struct dptx_hw_common *hw_comm,
	struct meson_tx_format_para *para, u8 sdp_sel);

#endif
