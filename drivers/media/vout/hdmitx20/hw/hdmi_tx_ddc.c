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
#include <linux/rtc.h>
#include <linux/timekeeping.h>

#include "common.h"
#include "hdmi_tx_reg.h"
#include "reg_ops.h"

const char *slave_msg[] = {
	[EDID_SLAVE] = "edid",
	[HDCP_SLAVE] = "hdcp",
	[SCDC_SLAVE] = "scdc",
};

const char *scdc_msg[] = {
	[SINK_VER] = "sink version",
	[SOURCE_VER] = "source version",
	[UPDATE_0] = "update0",
	[UPDATE_1] = "update1",
	[TMDS_CFG] = "tmds_config",
	[SCRAMBLER_ST] = "scrambler_status",
	[CONFIG_0] = "config_0",
	[STATUS_FLAGS_0] = "status_flags_0",
	[STATUS_FLAGS_1] = "status_flags_1",
	[ERR_DET_0_L] = "error detect ch0 L",
	[ERR_DET_0_H] = "error detect ch0 H",
	[ERR_DET_1_L] = "error detect ch1 L",
	[ERR_DET_1_H] = "error detect ch1 H",
	[ERR_DET_2_L] = "error detect ch2 L",
	[ERR_DET_2_H] = "error detect ch2 H",
	[ERR_DET_CHKSUM] = "error detect checksum",
	[TEST_CONFIG_0] = "test_config_0",
	[MANUFACT_IEEE_OUI_2] = "manufacturer ieee oui 2",
	[MANUFACT_IEEE_OUI_1] = "manufacturer ieee oui 1",
	[MANUFACT_IEEE_OUI_0] = "manufacturer ieee oui 0",
	[DEVICE_ID] = "device id",
	[MANUFACT_SPECIFIC] = "manufacturer specific",
};

static void pr_scdc_err_info(const char *func_name, u8 slave, u8 offset_addr,
	bool wr_flag, u8 *data)
{
	/* if operation is writing, then print data */
	if (wr_flag) {
		if (slave == SCDC_SLAVE) {
			HDMITX_ERROR("E: %s %s %s 0x%02x\n",
				func_name, slave_msg[SCDC_SLAVE],
				scdc_msg[offset_addr] ? scdc_msg[offset_addr] : "rsvd", *data);
		} else {
			if (slave_msg[slave])
				HDMITX_ERROR("E: %s %s 0x%02x 0x%02x 0x%02x\n",
					func_name, slave_msg[slave], slave, offset_addr, *data);
			else
				HDMITX_ERROR("E: %s unknown slave: <0x%x> 0x%02x 0x%02x\n",
					func_name, slave, offset_addr, *data);
		}
	} else {
		if (slave == SCDC_SLAVE) {
			HDMITX_ERROR("E: %s %s %s\n",
				func_name, slave_msg[SCDC_SLAVE],
				scdc_msg[offset_addr] ? scdc_msg[offset_addr] : "rsvd");
		} else {
			if (slave_msg[slave])
				HDMITX_ERROR("E: %s %s 0x%02x 0x%02x\n",
					func_name, slave_msg[slave], slave, offset_addr);
			else
				HDMITX_ERROR("E: %s unknown slave: <0x%x> 0x%02x\n",
					func_name, slave, offset_addr);
		}
	}
}

static DEFINE_MUTEX(ddc_mutex);
static uint32_t ddc_write_1byte(u8 slave, u8 offset_addr, u8 data)
{
	u32 st = 0;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_DATAO, data);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 4);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_scdc_err_info(__func__, slave, offset_addr, 1, &data);
	} else {
		st = 1;
	}
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);

	mutex_unlock(&ddc_mutex);
	return st;
}

static uint32_t ddc_read_8byte(u8 slave, u8 offset_addr, u8 *data)
{
	u32 st = 0;
	s32 i;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 2);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_scdc_err_info(__func__, slave, offset_addr, 0, data);
	} else {
		st = 1;
	}
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
	for (i = 0; i < 8; i++)
		data[i] = hdmitx_rd_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i);

	mutex_unlock(&ddc_mutex);
	return st;
}

static uint32_t ddc_read_1byte(u8 slave, u8 offset_addr, u8 *data)
{
	u32 st = 0;

	mutex_lock(&ddc_mutex);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, slave);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS, offset_addr);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 0);
	mdelay(2);
	if (hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 0)) {
		st = 0;
		pr_scdc_err_info(__func__, slave, offset_addr, 0, data);
	} else {
		st = 1;
	}
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 0x7);
	*data = hdmitx_rd_reg(HDMITX_DWC_I2CM_DATAI);

	mutex_unlock(&ddc_mutex);
	return st;
}

static uint32_t hdcp_rd_bksv(u8 *data)
{
	return ddc_read_8byte(HDCP_SLAVE, HDCP14_BKSV, data);
}

void scdc_rd_sink(u8 adr, u8 *val)
{
	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ddc_read_1byte(SCDC_SLAVE, adr, val);
}

void scdc_wr_sink(u8 adr, u8 val)
{
	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ddc_write_1byte(SCDC_SLAVE, adr, val);
}

uint32_t hdcp_rd_hdcp14_ver(void)
{
	int ret = 0;
	u8 bksv[8] = {0};

	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ret = hdcp_rd_bksv(&bksv[0]);
	if (ret)
		return 1;
	ret = hdcp_rd_bksv(&bksv[0]);
	if (ret)
		return 1;

	return 0;
}

uint32_t hdcp_rd_hdcp22_ver(void)
{
	u32 ret;
	u8 ver;

	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ret = ddc_read_1byte(HDCP_SLAVE, HDCP2_VERSION, &ver);
	if (ret)
		return ver == 0x04;
	ret = ddc_read_1byte(HDCP_SLAVE, HDCP2_VERSION, &ver);
	if (ret)
		return ver == 0x04;

	return 0;
}

/* only for I2C reactive using */
void edid_read_head_8bytes(void)
{
	u8 head[8] = {0};

	hdmitx_ddc_hw_op(DDC_MUX_DDC);
	ddc_read_8byte(EDID_SLAVE, 0x00, head);
}

/*
 * Note: read 8 Bytes of EDID data every time
 */
#define EDID_WAIT_TIMEOUT	10
void hdmitx_read_edid(unsigned char *rx_edid)
{
	unsigned int timeout = 0;
	unsigned int i;
	unsigned int byte_num = 0;
	unsigned char blk_no = 1;

	if (!rx_edid)
		return;
	mutex_lock(&ddc_mutex);
	/* Program SLAVE/ADDR */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SLAVE, 0x50);
	hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 1 << 1);
	/* Read complete EDID data sequentially */
	while (byte_num < 128 * blk_no) {
		hdmitx_wr_reg(HDMITX_DWC_I2CM_ADDRESS,  byte_num & 0xff);
		if (byte_num >= 256 && byte_num < 512 && blk_no > 2) {
			/* Program SEGMENT/SEGPTR */
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR, 0x30);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0x1);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 3);
		} else if ((byte_num >= 512) && (byte_num < 768) && (blk_no > 2)) {
			/* Program SEGMENT/SEGPTR */
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR, 0x30);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0x2);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 3);
		} else if ((byte_num >= 768) && (byte_num < 1024) && (blk_no > 2)) {
			/* Program SEGMENT/SEGPTR */
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR, 0x30);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0x3);
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 3);
		} else {
			hdmitx_wr_reg(HDMITX_DWC_I2CM_OPERATION, 1 << 2);
		}
		/* Wait until I2C done */
		timeout = 0;
		while (timeout < EDID_WAIT_TIMEOUT &&
		       !(hdmitx_rd_reg(HDMITX_DWC_IH_I2CM_STAT0) & (1 << 1))) {
			mdelay(2);
			timeout++;
		}
		if (timeout == EDID_WAIT_TIMEOUT) {
			HDMITX_INFO("ddc timeout\n");
			hdmitx_current_status(HDMITX_EDID_I2C_ERROR);
		}
		hdmitx_wr_reg(HDMITX_DWC_IH_I2CM_STAT0, 1 << 1);
		/* Read back 8 bytes */
		for (i = 0; i < 8; i++) {
			rx_edid[byte_num] =
				hdmitx_rd_reg(HDMITX_DWC_I2CM_READ_BUFF0 + i);
			if (byte_num == 126)
				blk_no = rx_edid[126] + 1;
			byte_num++;
		}
		if (byte_num == 136)
			if (rx_edid[128 + 4] == EXTENSION_EEODB_EXT_TAG &&
				rx_edid[128 + 5] == EXTENSION_EEODB_EXT_CODE)
				blk_no = rx_edid[128 + 6] + 1;
		if (blk_no > 8) {
			HDMITX_INFO("edid extension block number:");
			HDMITX_INFO(" %d, reset to MAX 7\n", blk_no - 1);
			blk_no = 8; /* Max extended block */
		}
	}
	/* Because DRM will use segment registers,
	 * so clear the registers to default
	 */
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGADDR, 0x0);
	hdmitx_wr_reg(HDMITX_DWC_I2CM_SEGPTR, 0x0);
	mutex_unlock(&ddc_mutex);
} /* hdmi20_tx_read_edid */
