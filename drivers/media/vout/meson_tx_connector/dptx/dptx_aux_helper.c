// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/component.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include "dptx_aux_helper.h"
#include "dpcd_reg.h"
#include "dptx_log.h"
#ifdef __UBOOT__
#include <amlogic/media/vout/meson_tx-connector/meson_tx_edid.h>
#elif __KERNEL__
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_edid.h>
#endif

/* return accessed count if success, otherwise a negetive error code */
int dptx_dpcd_access(struct dptx_aux *aux, u8 request,
	unsigned int offset, void *buffer, size_t size)
{
	struct dptx_aux_msg msg;
	unsigned int retry, reply;
	int err = 0, ret = 0;

	memset(&msg, 0, sizeof(msg));
	msg.address = offset;
	msg.request = request;
	msg.buffer = buffer;
	msg.size = size;

	if (!aux) {
		DPTX_ERROR("%s invalid aux instance\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&aux->hw_mutex);

	for (retry = 0; retry < aux->aux_retry_times; retry++) {
		if (ret != 0 && ret != -ETIMEDOUT) {
			usleep_range(AUX_RETRY_INTERVAL,
				     AUX_RETRY_INTERVAL + 100);
		}

		ret = aux->hw_comm->aux_transfer(aux->hw_comm, &msg);
		if (ret >= 0) {
			reply = msg.reply & (DP_AUX_NATIVE_REPLY_MASK | DP_AUX_I2C_REPLY_MASK);
			/* DP_AUX_NATIVE_REPLY_ACK and DP_AUX_I2C_REPLY_ACK both equals 0 */
			if (reply == DP_AUX_NATIVE_REPLY_ACK) {
				if (ret == size)
					goto unlock;

				ret = -EPROTO;
			} else {
				ret = -EIO;
			}
		}
		DPTX_DEBUG("%s retry: %d, ret = %d\n", __func__, retry, ret);
		/*
		 * We want the error we return to be the error we received on
		 * the first transaction, since we may get a different error the
		 * next time we retry
		 */
		if (!err)
			err = ret;
	}

	ret = err;

unlock:
	mutex_unlock(&aux->hw_mutex);
	return ret;
}

/* return accessed count if success, otherwise a negetive error code */
int dptx_aux_read_dpcd(struct dptx_aux *aux, unsigned int offset,
			 void *buffer, size_t size)
{
	int ret;

	ret = dptx_dpcd_access(aux, DP_AUX_NATIVE_READ, offset,
				   buffer, size);

	return ret;
}

int dptx_aux_write_dpcd(struct dptx_aux *aux, unsigned int offset,
			  void *buffer, size_t size)
{
	int ret;

	ret = dptx_dpcd_access(aux, DP_AUX_NATIVE_WRITE, offset,
				   buffer, size);

	return ret;
}

/* return read count if normal, otherwise return error code */
static int dptx_read_extended_dpcd_cap(struct dptx_aux *aux,
					  u8 *dpcd, size_t size)
{
	u8 dpcd_ext[DPTX_RECEIVER_CAP_SIZE];
	int ret;

	/* The Extended Receiver Capability field is valid with DPCD r1.4 (and higher).
	 * 02200h ~ 0220Fh match 00000h ~ 0000Fh but except for the following 3 registers
	 * DPCD_REV, MAX_LINK_RATE, DOWN_STREAM_PORT_PRESENT.
	 * refer to DP1.4 Spec, a DPRX that supports Extended Receiver Capability filed
	 * shall set bit7 of 0x000E to 1.
	 */
	if (!(dpcd[DP_TRAINING_AUX_RD_INTERVAL] &
	      DP_EXTENDED_RECEIVER_CAP_FIELD_PRESENT))
		return 0;

	DPTX_INFO("%s replace dpcd by extended dpcd cap\n", __func__);

	ret = dptx_aux_read_dpcd(aux, DP_DP13_DPCD_REV, &dpcd_ext,
			       sizeof(dpcd_ext));
	if (ret < 0)
		return ret;
	if (ret != sizeof(dpcd_ext))
		return -EIO;

	/*
	 * refer to dp1.4 spec, chapter 2.9.3.1;
	 * The DPCD Revision number at DPCD Address 00000h shall
	 * not be higher than that at DPCD Address 02200h
	 */
	if (dpcd[DP_DPCD_REV] > dpcd_ext[DP_DPCD_REV])
		return ret;

	if (!memcmp(dpcd, dpcd_ext, sizeof(dpcd_ext)))
		return ret;

	/*
	 * refer to dp1.4 spec, chapter 2.9.3.1
	 * The Extended Receiver Capability registers at DPCD Addresses
	 * 02200h through 0220Fh shall contain the DPRX's true capability.
	 *
	 * replace Receiver Capability by Extended Receiver Capability filed
	 */
	memcpy(dpcd, dpcd_ext, sizeof(dpcd_ext));

	return ret;
}

int dptx_aux_read_dpcd_caps(struct dptx_aux *aux,
			  u8 *dpcd, size_t size)
{
	int ret;

	ret = dptx_aux_read_dpcd(aux, DP_DPCD_REV, dpcd, size);
	if (ret < 0)
		return ret;
	if (ret != size || dpcd[DP_DPCD_REV] == 0)
		return -EIO;

	ret = dptx_read_extended_dpcd_cap(aux, dpcd, DPTX_RECEIVER_CAP_SIZE);
	if (ret < 0)
		return ret;

	return ret;
}

static int aux_read_edid_single_block(struct dptx_aux *aux, u8 *edid)
{
	int i;
	int ret = 0;
	int edid_read_cnt_once = aux->edid_read_cnt_once;

	for (i = 0; i < EDID_LENGTH; i += edid_read_cnt_once) {
		ret = dptx_dpcd_access(aux, DP_AUX_I2C_MOT | DP_AUX_I2C_READ, DDC_ADDR,
					   edid + i, edid_read_cnt_once);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x 0x%x read mot err\n", DDC_ADDR, i);
			return ret;
		} else {
			/* ret should match edid_read_cnt_once */
			if (ret != edid_read_cnt_once)
				DPTX_ERROR("i2c 0x%x read reply count: %d\n", DDC_ADDR, ret);
		}
	}
	return ret;
}

int dptx_aux_read_edid_data(struct dptx_aux *aux,
			  u8 *_edid, int size)
{
	int ret = 0;
	u8 tmp = 0;
	u8 seg_ptr = 0;
	u8 i2c_sub_addr = 0;
	u8 *edid = _edid;
	u32 blk_idx = 0;
	u8 ext_block_num = 0;

	if (!aux || !_edid)
		return -EIO;
	if (size % 128)
		DPTX_INFO("edid buffer length %d should be multiple of 128\n", size);

	/* Read complete EDID data sequentially */
	while (blk_idx < (1 + ext_block_num)) {
		if ((size - (blk_idx + 1) * EDID_LENGTH) <= 0) {
			DPTX_ERROR("edid buff not enough\n");
			return ret;
		}
		/* write seg_pointer */
		if (blk_idx % 2 == 0) {
			ret = dptx_dpcd_access(aux, DP_AUX_I2C_MOT | DP_AUX_I2C_WRITE, DDC_SEG_PTR,
				&seg_ptr, 1);
			if (ret < 0) {
				DPTX_ERROR("i2c 0x%x write mot err\n", DDC_SEG_PTR);
				return ret;
			}
			seg_ptr++;
		}

		/* write offset */
		i2c_sub_addr = (blk_idx % 2 == 0) ? 0 : 0x80;
		ret = dptx_dpcd_access(aux, DP_AUX_I2C_MOT | DP_AUX_I2C_WRITE, DDC_ADDR,
			&i2c_sub_addr, 1);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x write mot err\n", DDC_ADDR);
			return ret;
		}

		/* read edid block */
		ret = aux_read_edid_single_block(aux, edid);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x read block%d err\n", DDC_ADDR, 0);
			return ret;
		}
		if (blk_idx == 0) {
			ext_block_num = edid[126];
		} else if (blk_idx == 1) {
			if (edid[4] == EXTENSION_EEODB_EXT_TAG &&
				edid[5] == EXTENSION_EEODB_EXT_CODE)
				ext_block_num = edid[6];
		}

		size -= EDID_LENGTH;
		edid += EDID_LENGTH;
		blk_idx++;

		/* i2c read w/o MOT, dummy read to close the transaction after each block */
		ret = dptx_dpcd_access(aux, DP_AUX_I2C_READ, DDC_ADDR, &tmp, 0);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x read err\n", DDC_ADDR);
			return ret;
		}
	}
	return 0;
}

int dptx_aux_read_edid_dbg(struct dptx_aux *aux, u8 *_edid, u32 type)
{
	int ret = 0;
	u8 tmp = 0;
	u8 seg_ptr = 0;
	u8 i2c_sub_addr = 0;
	u8 *edid = _edid;

	if (!aux || !_edid)
		return -EIO;

	if (type == 0) {
		/* write seg_pointer */
		ret = dptx_dpcd_access(aux, DP_AUX_I2C_MOT | DP_AUX_I2C_WRITE, DDC_SEG_PTR,
			&seg_ptr, 1);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x write mot err\n", DDC_SEG_PTR);
			return ret;
		}
	} else if (type == 1) {
		/* write offset */
		ret = dptx_dpcd_access(aux, DP_AUX_I2C_MOT | DP_AUX_I2C_WRITE, DDC_ADDR,
			&i2c_sub_addr, 1);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x write mot err\n", DDC_ADDR);
			return ret;
		}
	} else if (type == 2) {
		/* read edid block */
		ret = aux_read_edid_single_block(aux, edid);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x read block%d err\n", DDC_ADDR, 0);
			return ret;
		}
	} else if (type == 3) {
		/* i2c read w/o MOT, dummy read to close the transaction after each block */
		ret = dptx_dpcd_access(aux, DP_AUX_I2C_READ, DDC_ADDR, &tmp, 0);
		if (ret < 0) {
			DPTX_ERROR("i2c 0x%x read err\n", DDC_ADDR);
			return ret;
		}
	} else {
		ret = dptx_aux_read_edid_data(aux, _edid, 128 * 8);
		if (ret < 0)
			DPTX_INFO("read edid failed\n");
	}

	return ret;
}

struct dptx_aux *dptx_aux_init(struct dptx_hw_common *hw_comm)
{
	struct dptx_aux *aux;

	aux = kzalloc(sizeof(*aux), GFP_KERNEL);
	if (!aux) {
		DPTX_ERROR("%s alloc fail\n", __func__);
		return NULL;
	}

	aux->hw_comm = hw_comm;
	mutex_init(&aux->hw_mutex);
	aux->aux_timeout_ms = 1;
	aux->aux_retry_times = DPCD_ACCESS_MAX_RETRY;
	aux->edid_read_cnt_once = 2;
	return aux;
}
