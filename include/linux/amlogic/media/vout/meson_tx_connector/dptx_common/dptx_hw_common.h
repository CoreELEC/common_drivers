/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_HW_COMMON_H
#define __DPTX_HW_COMMON_H

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>

#define MAX_FIFO_SIZE      600
//todo size define
/* rxinfo: 2, seq_num_v: 3, v: 16, receiver id list: 5*device_count */
/* device_count max is 31 */
#define RCVID_LIST_SIZE    176
#define STREAM_READY_SIZE  42
#define AKESENDCERT_FIFO   533
#define H_PRIME_SIZE       32
#define EKHKM_SIZE         16
#define LPRIME_SIZE        32

// 0: HW 0   AUX 0   OK
// 1: HW 1   AUX 0
// 2: HW 0   AUX 1
// 3: HW 1   AUX 1   OK
#define TYPE_TEST_PATTERN 3

/* RXCAP */
#define RXCAP_REPEATER		BIT(0)

struct dptx_timer_manager {
	u32 timers_num;
	struct dptx_timer_handler *handlers;
};

struct dptx_aux;
struct dptx_aux_msg;

struct dptx_hdcp {
	/* dptx hdcp hw releate */
	/* 8051 send fifo */
	u8 snd_fifo[MAX_FIFO_SIZE];
	u32 snd_fifo_len;

	/* dptx hdcp spec releate */
	u8 rxstatus;
	/*
	 * bit[23:16] version
	 * bit[1]     hdcp_capable
	 * bit[0]     repeater
	 */
	u32 rxcap;
	/*
	 * bit[23:16] version, must set to 0x2
	 * bit[15:0]  reserved as 0
	 */
	u32 txcap;

	u8 receiverid_list[RCVID_LIST_SIZE];
	u8 stream_ready[STREAM_READY_SIZE];
	u8 akesendcert_fifo[AKESENDCERT_FIFO];
	u8 h_prime_fifo[H_PRIME_SIZE];
	u8 ekhkm_fifo[EKHKM_SIZE];
	u8 lprime_fifo[LPRIME_SIZE];

	/* for irq */
	struct completion sndfifo_completion;
	struct completion pairing_available_completion;
	struct completion havailable_completion;
	struct completion ready_completion;
};

struct dptx_hw_common {
	struct meson_tx_hw hw_base;
	/* for debug hdcp */
	struct work_struct work_dptxhdcp;

	/* irq id of dptx */
	u32 dptx_irq_id;
	u32 hdcp2tx_irq_id;
	/* wait timer before hpd plugin handle */
	u32 hpd_in_filter_ms;

	/* instance index */
	u32 dev_idx;

	/* core ID/revision */
	u32 core_info;
	bool mst_en;
	bool fec_en;
	u8 psr_en;
	bool vid_enable;
	u8 hdcp_sec_en;
	/* the spinlock for secondary data packet */
	spinlock_t sdp_lock;

	/* point to aux instance in dptx_common, as it's needed in
	 * mst config during HW mode set
	 */
	struct dptx_aux *tx_aux;
	/* transfers a message representing a single AUX transaction.
	 * Upon success, the implementation should return the number
	 * of payload bytes that were transferred, or a negative
	 * error-code on failure.
	 *
	 * The function must only modify the reply field of
	 * the dptx_aux_msg structure.
	 */
	ssize_t (*aux_transfer)(struct dptx_hw_common *tx_comm, struct dptx_aux_msg *msg);
	struct dptx_timer_manager *timer_manager;

	u32 is_edp;
	/* use external edptx driver to control HW core */
	bool ext_edptx_ctrl_en;
	/* for link training debug */
	u8 force_tps_pattern;
	/* DP1.4 chapter2.2.3 stream clk and link symbol clk share the same reference clk */
	bool vid_clk_sync_mode;

	/* dptx hdcp */
	struct dptx_hdcp *dp_hdcp;
};

#define to_dptx_hw_common(x)	container_of(x, struct dptx_hw_common, hw_base)

int dptx_hw_cntl(struct meson_tx_hw *tx_hw, u32 cmd,
	void *input_argv, void *output_struct);

/***** dptx20 hw api*****/
struct meson_tx_hw *dptx20_alloc_tx_hw(void);
void dptx20_free_tx_hw(struct meson_tx_hw *tx_hw);

#endif
