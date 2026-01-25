/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DPTX_AUX_HELPER_H_
#define _DPTX_AUX_HELPER_H_

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>

#define AUX_RETRY_INTERVAL 500 /* us */

#define EDID_READ_MAX_RETRY 5
#define DPCD_ACCESS_MAX_RETRY 32

struct dptx_hw_common;

struct dptx_aux_msg {
	unsigned int address;
	u8 request;
	u8 reply;
	void *buffer;
	size_t size;
};

struct dptx_aux {
	/* point to aux bound dptx_hw instance */
	struct dptx_hw_common *hw_comm;
	const char *name;
	/* mutex for concurrent AUX CH access */
	struct mutex hw_mutex;
	/* the count of i2c nacks */
	unsigned int i2c_nack_count;
	/* the count of i2c defer */
	unsigned int i2c_defer_count;

	/* aux reply timeout period */
	u32 aux_timeout_ms;
	/* retry times if aux fail  */
	u8 aux_retry_times;
	/* read EDID bytes every time */
	u8 edid_read_cnt_once;
};

struct dptx_aux *dptx_aux_init(struct dptx_hw_common *tx_comm);

int dptx_aux_read_dpcd(struct dptx_aux *aux, unsigned int offset,
			 void *buffer, size_t size);
int dptx_aux_write_dpcd(struct dptx_aux *aux, unsigned int offset,
			  void *buffer, size_t size);
int dptx_dpcd_access(struct dptx_aux *aux, u8 request,
	unsigned int offset, void *buffer, size_t size);
static inline ssize_t dptx_aux_readb_dpcd(struct dptx_aux *aux,
					unsigned int offset, u8 *valuep)
{
	return dptx_aux_read_dpcd(aux, offset, valuep, 1);
}

static inline ssize_t dptx_aux_writeb_dpcd(struct dptx_aux *aux,
					 unsigned int offset, u8 value)
{
	return dptx_aux_write_dpcd(aux, offset, &value, 1);
}

int dptx_aux_read_dpcd_caps(struct dptx_aux *aux, u8 *dpcd, size_t size);

int dptx_aux_read_edid_data(struct dptx_aux *aux, u8 *edid, int size);
int dptx_aux_read_edid_dbg(struct dptx_aux *aux, u8 *_edid, u32 type);

#endif /* _DPTX_AUX_HELPER_H_ */
