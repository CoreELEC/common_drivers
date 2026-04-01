// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>

#include "hdmitx_hw_platform.h"
#include "hdmitx_hw_core.h"
#include "hdmi_tx_connector/hdcp/hdmitx_hdcp.h"
#include "hdmi_tx_connector/hdmitx_compliance.h"

static DEFINE_MUTEX(ddc_mutex);

static void ddc_print(int slave_addr, int reg_addr, int ddc_ret)
{
	if (ddc_ret)
		HDMITX_INFO("DDC ret = %d, slave_addr = 0x%x, reg_addr = 0x%x\n",
			ddc_ret, slave_addr, reg_addr);
}

void scdc21_rd_sink(u8 adr, u8 *val)
{
	hdmitx_ddcm_read(0, DDC_SCDC_ADDR, adr, val, 1, TPI_DDC_CMD_SEQUENTIAL_READ);
}

void scdc21_sequential_rd_sink(u8 adr, u8 *val, u8 len)
{
	hdmitx_ddcm_read(0, DDC_SCDC_ADDR, adr, val, len, TPI_DDC_CMD_SEQUENTIAL_READ);
}

void scdc21_wr_sink(u8 adr, u8 val)
{
	hdmitx_ddcm_write(0, DDC_SCDC_ADDR, adr, val);
}

void hdmitx21_read_edid(struct hdmitx_common *tx_comm)
{
	u32 blk_idx = 0;
	u8 ext_block_num = 0;
	u8 *rx_edid;
	bool ret = 0;

	/* skip edid reading in pxp */
	if (tx_comm->pxp_mode)
		return;

	rx_edid = tx_comm->edid_buf;

	if (0) {
		/* DDC_DELAY_CNT_IVCTX, config scl fre 100k */
		hdmitx21_wr_reg(DDC_DELAY_CNT_IVCTX, 0x3e);
		/* DDC_ST_STOP_DELAY_0, for stop to start timing debug config, 4.7us */
		hdmitx21_wr_reg(DDC_ST_STOP_DELAY_0, 0x69);
		/* DDC_SCL_DUTY_MODE_ADDR, bit1 open st_stop counter */
		hdmitx21_wr_reg(DDC_SCL_DUTY_MODE_ADDR,
			hdmitx21_rd_reg(DDC_SCL_DUTY_MODE_ADDR) | 0x2);
	}
	/* Read complete EDID data sequentially */
	while (blk_idx < (1 + ext_block_num)) {
		ret = hdmitx_ddcm_read(blk_idx >> 1, DDC_EDID_ADDR,
			(blk_idx * EDID_BLK_SIZE) & 0xff, &rx_edid[blk_idx * EDID_BLK_SIZE],
			EDID_BLK_SIZE, TPI_DDC_CMD_ENHANCED_DDC_READ);
		if (ret)
			hdmitx_current_status(tx_comm, HDMITX_EDID_I2C_ERROR);
		if (blk_idx == 0)
			ext_block_num = rx_edid[126];
		if (blk_idx == 1)
			if (rx_edid[128 + 4] == EXTENSION_EEODB_EXT_TAG &&
				rx_edid[128 + 5] == EXTENSION_EEODB_EXT_CODE)
				ext_block_num = rx_edid[128 + 6];
		if (ext_block_num > 7) {
			HDMITX_INFO("edid extension block number:");
			HDMITX_INFO(" %d, reset to MAX 7\n", ext_block_num);
			/* Max extended block */
			ext_block_num = 7;
		}
		blk_idx++;
	}
}

void hdmi_ddc_error_reset(void)
{
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, true);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_ABORT_TRANSACTION);
	usleep_range(1, 2);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_CLK_RESET);
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, false);
}

u8 hdmi_ddc_busy_check(void)
{
	return hdmitx21_rd_reg(DDC_STATUS_IVCTX) & BIT_DDC_STATUS_INPROG;
}

u8 hdmi_ddc_status_check(void)
{
	bool check_failed = false;
	u8 ddc_status;

	ddc_status = hdmitx21_rd_reg(DDC_STATUS_IVCTX);
	if (ddc_status & (BIT_DDC_STATUS_BUSLOW | BIT_DDC_STATUS_NACK)) {
		hdmitx21_wr_reg(DDC_STATUS_IVCTX, 0x00);
		check_failed = true;
	}

	return check_failed;
}

static u8 ddc_tx_hdcp2x_check(void)
{
	u8 val;

	val = hdmitx21_rd_reg(HDCP2X_CTL_0_IVCTX) & BIT_HDCP2X_CTL_0_EN;
	if (val)
		hdmitx21_set_bit(SCDC_CTL_IVCTX, BIT_SCDC_CTL_REG_DDC_STALL_REQ, true);
	return val;
}

static u8 ddc_tx_busy_check(void)
{
	return hdmitx21_rd_reg(DDC_STATUS_IVCTX) & BIT_DDC_STATUS_INPROG;
}

static bool ddc_wait_free(void)
{
	u8 val;
	/* unit: ms */
	u8 tmo1 = 5;
	u8 tmo2 = 2;

	while (tmo2--) {
		tmo1 = 5;
		while (tmo1--) {
			val = ddc_tx_busy_check();
			if (!val)
				return true;
			usleep_range(2000, 2500);
		}
		HDMITX_INFO("ddc bus busy\n");
		hdmi_ddc_error_reset();
		usleep_range(2000, 2500);
	}
	return false;
}

static void ddc_tx_en(u8 seg_index, u8 slave_addr, u8 reg_addr)
{
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, true);
	hdmitx21_wr_reg(DDC_ADDR_IVCTX, slave_addr & BIT_DDC_ADDR_REG);
	hdmitx21_wr_reg(DDC_SEGM_IVCTX, seg_index);
	hdmitx21_wr_reg(DDC_OFFSET_IVCTX, reg_addr);
}

/* After fifo clear, need some delay */
static void ddc_tx_read(u8 seg_index, u16 length, u8 read_cmd)
{
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_CLR_FIFO);
	hdmitx21_wr_reg(DDC_DIN_CNT2_IVCTX, (u8)(length >> 8));
	hdmitx21_wr_reg(DDC_DIN_CNT1_IVCTX, (u8)length);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, read_cmd);
}

/* After fifo clear, need some delay */
static void ddc_tx_write(u8 data)
{
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_CLR_FIFO | 0x30);
	hdmitx21_wr_reg(DDC_DIN_CNT2_IVCTX, 0);
	hdmitx21_wr_reg(DDC_DIN_CNT1_IVCTX, 1);
	hdmitx21_wr_reg(DDC_DATA_AON_IVCTX, data);
	hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_SEQ_RW_IGNORE_ACK | 0x30);
}

static u8 ddc_tx_fifo_size_read(void)
{
	return hdmitx21_rd_reg(DDC_DOUT_CNT_IVCTX) & BIT_DDC_DOUT_CNT_DATA_OUT_CNT;
}

static bool ddc_tx_err_check(void)
{
	bool check_failed = false;
	u8 ddc_status;

	ddc_status = hdmitx21_rd_reg(DDC_STATUS_IVCTX);
	if (ddc_status & (BIT_DDC_STATUS_BUSLOW | BIT_DDC_STATUS_NACK)) {
		hdmitx21_wr_reg(DDC_STATUS_IVCTX, 0x00);
		check_failed = true;
	}

	return check_failed;
}

static void ddc_tx_scdc_clr(u8 val)
{
	if (val)
		hdmitx21_set_bit(SCDC_CTL_IVCTX, BIT_SCDC_CTL_REG_DDC_STALL_REQ, false);
}

static void ddc_tx_error_check(enum ddc_err_t ds_ddc_error)
{
	if (ds_ddc_error) {
		hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_ABORT_TRANSACTION);
		usleep_range(1, 2);
		hdmitx21_wr_reg(DDC_CMD_IVCTX, DDC_CMD_CLK_RESET);
	}
}

static void ddc_tx_fifo_read(u8 *p_buf, u16 fifo_size)
{
	hdmitx21_fifo_read(DDC_DATA_AON_IVCTX, p_buf, fifo_size);
}

static void ddc_tx_disable(void)
{
	hdmitx21_set_bit(TPI_DDC_MASTER_EN_IVCTX, BIT_TPI_DDC_MASTER_EN_HW_EN, false);
}

static enum ddc_err_t _hdmitx_ddcm_read_(u8 seg_index,
	u8 slave_addr, u8 reg_addr, u8 *p_buf, u16 length, u8 read_cmd)
{
	enum ddc_err_t ds_ddc_error = DDC_ERR_NONE;
	u16 fifo_size;
	u16 timeout_ms;
	u8 val = ddc_tx_hdcp2x_check();

	mutex_lock(&ddc_mutex);
	do {
		if (length == 0 || !p_buf)
			break;

		if (!ddc_wait_free()) {
			/*
			 * need to clr DDC_STALL_REQ, otherwise
			 * DDC will always be occupied by SCDC
			 */
			ddc_tx_scdc_clr(val);
			mutex_unlock(&ddc_mutex);
			return DDC_ERR_BUSY;
		}

		ddc_tx_en(seg_index, slave_addr, reg_addr);
		ddc_tx_read(seg_index, length, read_cmd);

		timeout_ms = (u16)(length * 12 / 10);
		usleep_range(2000, 3000);
		do {
			fifo_size = ddc_tx_fifo_size_read();

			if (fifo_size) {
				if (fifo_size > length) {
					ds_ddc_error = DDC_ERR_HW;
					break;
				} else if (fifo_size > LEN_TPI_DDC_FIFO_SIZE) {
					ds_ddc_error = DDC_ERR_LIM_EXCEED;
					break;
				}
				/* read fifo_size bytes */
				ddc_tx_fifo_read(p_buf, fifo_size);

				length -= fifo_size;
				p_buf += fifo_size;
			} else {
				usleep_range(1000, 1500);
				timeout_ms--;
				if (ddc_tx_err_check()) {
					ds_ddc_error = DDC_ERR_NACK;
					break;
				}
			}
		} while (length && timeout_ms);

		if (ds_ddc_error)
			break;
	} while (false);

	ddc_tx_scdc_clr(val);
	ddc_tx_error_check(ds_ddc_error);

	/* disable the DDC master */
	ddc_tx_disable();
	mutex_unlock(&ddc_mutex);
	ddc_print(slave_addr, reg_addr, ds_ddc_error);
	return ds_ddc_error;
}

static enum ddc_err_t _hdmitx_ddcm_write_(u8 seg_index,
	u8 slave_addr, u8 reg_addr, u8 data)
{
	enum ddc_err_t ds_ddc_error = DDC_ERR_NONE;
	u8 val = ddc_tx_hdcp2x_check();

	mutex_lock(&ddc_mutex);
	if (!ddc_wait_free()) {
		ddc_tx_scdc_clr(val);
		mutex_unlock(&ddc_mutex);
		ddc_print(slave_addr, reg_addr, ds_ddc_error);
		return DDC_ERR_BUSY;
	}

	ddc_tx_en(seg_index, slave_addr, reg_addr);
	ddc_tx_write(data);

	usleep_range(2000, 3000);
	if (ddc_tx_err_check())
		ds_ddc_error = DDC_ERR_NACK;

	ddc_tx_scdc_clr(val);
	ddc_tx_error_check(ds_ddc_error);

	/* disable the DDC master */
	ddc_tx_disable();
	mutex_unlock(&ddc_mutex);
	ddc_print(slave_addr, reg_addr, ds_ddc_error);
	return ds_ddc_error;
}

void ddc_toggle_sw_tpi(void)
{
	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, true);
	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, false);
}

bool hdmitx_ddcm_read(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 *p_buf, u16 len, u8 read_cmd)
{
	enum ddc_err_t ddc_err;

	ddc_err = _hdmitx_ddcm_read_(seg_index, slave_addr, reg_addr, p_buf, len, read_cmd);
	return (ddc_err == DDC_ERR_NONE) ? false : true;
}

bool hdmitx_ddcm_write(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 data)
{
	enum ddc_err_t ddc_err;

	ddc_err = _hdmitx_ddcm_write_(seg_index, slave_addr, reg_addr, data);
	return (ddc_err == DDC_ERR_NONE) ? false : true;
}

bool is_rx_hdcp2ver(struct hdmitx_common *tx_comm)
{
	u8 cap_val = 0;
	bool ret = false;
	int i = 0;
	/* read hdcp version maximum times */
	int hdcp_ver_cnt = 1;

	if (hdmitx_find_vendor_hdcp_ver_retry(tx_comm->edid_buf))
		hdcp_ver_cnt = 2;
	/*
	 * it easily read fails under FRL mode as FRL ddc bus stall operation,
	 * so use hdmitx_ddcm_read() method instead
	 */
	/* hdmitx_ddc_read_1byte(DDC_HDCP_DEVICE_ADDR, REG_DDC_HDCP_VERSION, &cap_val); */
	for (i = 0; i < hdcp_ver_cnt && cap_val != 0x04; i++) {
		ret = hdmitx_ddcm_read(0, DDC_HDCP_DEVICE_ADDR, REG_DDC_HDCP_VERSION,
			&cap_val, 1, TPI_DDC_CMD_SEQUENTIAL_READ);
		if (ret)
			HDMITX_WARNING("hdmitx: ddc read hdcp version failed\n");
	}

	if (i > 1)
		HDMITX_WARNING("read hdcp version retry %d time(s)\n", i - 1);

	return cap_val == 0x04;
}

