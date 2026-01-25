/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_COMMON_H
#define __DPTX_COMMON_H

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_dev.h>

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif

struct dptx_hw_common;

struct meson_dp_aux_msg {
	unsigned int address;
	u8 request;
	u8 reply;
	void *buffer;
	size_t size;
};

struct meson_dp_aux {
	/**
	 * @name: user-visible name of this AUX channel and the
	 * I2C-over-AUX adapter.
	 *
	 * It's also used to specify the name of the I2C adapter. If set
	 * to %NULL, dev_name() of @dev will be used.
	 */
	const char *name;

	/**
	 * @hw_mutex: internal mutex used for locking transfers.
	 *
	 * Note that if the underlying hardware is shared among multiple
	 * channels, the driver needs to do additional locking to
	 * prevent concurrent access.
	 */
	struct mutex hw_mutex;

	/**
	 * @i2c_nack_count: Counts I2C NACKs, used for DP validation.
	 */
	unsigned int i2c_nack_count;
	/**
	 * @i2c_defer_count: Counts I2C DEFERs, used for DP validation.
	 */
	unsigned int i2c_defer_count;
};

enum timer_wait_type {
	TIMER_WAIT,
	TIMER_ISR,
	TIMER_REPEAT_ISR,
};

struct timer_config {
	u32 timer_type;
	enum timer_wait_type wait_type;
	u32 us; // timeout count in microseconds
	s32 repeater_times; // -1 means infinite times
	void (*callback)(void *cb_data);
	void *cb_data;
};

struct dptx_timer_handler {
	/* each timer has its own mutex */
	struct mutex timer_mutex;
	struct timer_config cfg;
};

struct meson_tx_phy;

/* dptx fresh rate/timing limitation, lane/link_rate limitation */
struct dptx_cap {
	u16 max_fresh_rate;
	u16 max_h_active;
	u16 max_v_active;
	u16 max_lane_count;
	u32 max_link_rate; /* unit: kHz, 5400000 is 5.4Gbps */
};

struct dptx_common {
	struct meson_tx_dev base;
	int drm_dptx_id;

	struct dptx_hw_common *hw_comm;
	struct link_train_t *link_train;
	/* instance index */
	u32 dev_idx;
	u32 is_edp;
	/* use external edptx driver to control HW core */
	bool ext_edptx_drv_en;

	struct dptx_aux *tx_aux;
	struct dptx_hw_fmt_para hw_fmt_para;
	/* link/sink status get after irq_hpd or hotplug */
	unsigned char link_sink_status[6];
	/* enc index: dp/edp device */
	u32 enc_idx;
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	struct early_suspend es;
	bool es_registered;
#endif
};

#define to_dptx_common(x)	container_of(x, struct dptx_common, base)
#define conn_to_dptx_common(x) container_of(to_meson_tx_dev(x), struct dptx_common, base)

/* for drm */
int dptx_get_mode_list(struct dptx_common *tx_comm, struct tx_timing **timing_list);
void dptx_register_dev_to_txdev_xlate(struct meson_tx_dev *base,
		conn_dev_to_txdev convert);

#endif
