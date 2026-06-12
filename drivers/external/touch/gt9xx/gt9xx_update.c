// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/kthread.h>
#include "gt9xx.h"
#include <linux/namei.h>
#include <linux/mount.h>
#include <linux/firmware.h>
#include <linux/ctype.h>

#define GUP_REG_HW_INFO		0x4220
#define GUP_REG_FW_MSG		0x41E4
#define GUP_REG_PID_VID		0x8140

#define FIRMWARE_NAME_LEN_MAX		256
#define GOODIX_FIRMWARE_FILE_NAME	"goodix_firmware.bin"
#define GOODIX_CONFIG_FILE_NAME		"goodix_config.cfg"

#define FW_HEAD_LENGTH			14
#define FW_SECTION_LENGTH		0x2000	/*  8K */
#define FW_DSP_ISP_LENGTH		0x1000	/*  4K */
#define FW_DSP_LENGTH			0x1000	/*  4K */
#define FW_BOOT_LENGTH			0x800	/*  2K */
#define FW_SS51_LENGTH		(4 * FW_SECTION_LENGTH)	/*  32K */
#define FW_BOOT_ISP_LENGTH		0x800	/*  2k */
#define FW_GLINK_LENGTH			0x3000	/*  12k */
#define FW_GWAKE_LENGTH		(4 * FW_SECTION_LENGTH) /*  32k */

#define PACK_SIZE			256
#define MAX_FRAME_CHECK_TIME		5

#define _bRW_MISCTL__SRAM_BANK		0x4048
#define _bRW_MISCTL__MEM_CD_EN		0x4049
#define _bRW_MISCTL__CACHE_EN		0x404B
#define _bRW_MISCTL__TMR0_EN		0x40B0
#define _rRW_MISCTL__SWRST_B0_		0x4180
#define _bWO_MISCTL__CPU_SWRST_PULSE	0x4184
#define _rRW_MISCTL__BOOTCTL_B0_	0x4190
#define _rRW_MISCTL__BOOT_OPT_B0_	0x4218
#define _rRW_MISCTL__BOOT_CTL_		0x5094

#pragma pack(1)
struct st_fw_head {
	u8	hw_info[4];			 /* hardware info */
	u8	pid[8];				 /* product id */
	u16 vid;				 /* version id */
};

#pragma pack()

struct st_update_msg {
	u8 fw_damaged;
	u8 fw_flag;
	const u8 *fw_data;
	struct file *cfg_file;
	struct st_fw_head ic_fw_msg;
	u32 fw_total_len;
	u32 fw_burned_len;
	const struct firmware *fw;
} update_msg;

u16 show_len;
u16 total_len;

static struct i2c_client *gup_client;
void *gup_dir;

static u8 gup_burn_fw_gwake_section(struct i2c_client *client,
				    u8 *fw_section, u16 start_addr,
				    u32 len, u8 bank_cmd);

static s32 gup_init_panel(struct goodix_ts_data *ts)
{
	s32 ret = 0;
	u8 opr_buf[16];
	u8 sensor_id = 0;
	u8 drv_cfg_version;
	u8 flash_cfg_version;
	struct goodix_config_data *cfg = &ts->pdata->config;

	if (cfg->length < GTP_CONFIG_MIN_LENGTH) {
		dev_err(&ts->client->dev,
			"No valid config with sensor_ID(%d) ",
			sensor_id);
		return -EPERM;
	}

	ret = gtp_i2c_read_dbl_check(ts->client, GTP_REG_CONFIG_DATA,
				     &opr_buf[0], 1);
	if (ret == SUCCESS) {
		dev_dbg(&ts->client->dev,
			"CFG_GROUP%d Config Version: %d, IC Config Version: %d",
			sensor_id, cfg->data[GTP_ADDR_LENGTH], opr_buf[0]);

		flash_cfg_version = opr_buf[0];
		drv_cfg_version = cfg->data[GTP_ADDR_LENGTH];

		if (flash_cfg_version < 90 &&
		    flash_cfg_version > drv_cfg_version)
			cfg->data[GTP_ADDR_LENGTH] = 0x00;
	} else {
		dev_err(&ts->client->dev,
			"Failed to get ic config version!No config sent!");
		return -EPERM;
	}

	ret = gtp_send_cfg(ts->client);
	if (ret < 0)
		dev_err(&ts->client->dev, "Send config error.");
	else
		usleep_range(10000, 11000);

	/* restore config version */
	cfg->data[GTP_ADDR_LENGTH] = drv_cfg_version;

	return 0;
}

static u8 gup_get_ic_msg(struct i2c_client *client, u16 addr, u8 *msg, s32 len)
{
	s32 i = 0;

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;

	for (i = 0; i < 5; i++) {
		if (gtp_i2c_read(client, msg, GTP_ADDR_LENGTH + len) > 0)
			break;
	}

	if (i >= 5) {
		dev_err(&client->dev,
				"Read data from 0x%02x%02x failed!",
				msg[0], msg[1]);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_set_ic_msg(struct i2c_client *client, u16 addr, u8 val)
{
	s32 i = 0;
	u8 msg[3];

	msg[0] = (addr >> 8) & 0xff;
	msg[1] = addr & 0xff;
	msg[2] = val;

	for (i = 0; i < 5; i++) {
		if (gtp_i2c_write(client, msg, GTP_ADDR_LENGTH + 1) > 0)
			break;
	}

	if (i >= 5) {
		dev_err(&client->dev,
			"Set data to 0x%02x%02x failed!", msg[0], msg[1]);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_get_ic_fw_msg(struct i2c_client *client)
{
	s32 ret = -1;
	u8	retry = 0;
	u8	buf[16];
	u8	i;

	/*  step1:get hardware info */
	ret = gtp_i2c_read_dbl_check(client, GUP_REG_HW_INFO,
				     &buf[GTP_ADDR_LENGTH], 4);
	if (ret == FAIL) {
		dev_err(&client->dev, "[get_ic_fw_msg]get hw_info failed,exit");
		return FAIL;
	}

	for (i = 0; i < 4; i++)
		update_msg.ic_fw_msg.hw_info[i] = buf[GTP_ADDR_LENGTH + 3 - i];
	dev_dbg(&client->dev,
		"IC Hardware info:%02x%02x%02x%02x",
		update_msg.ic_fw_msg.hw_info[0],
		update_msg.ic_fw_msg.hw_info[1],
		update_msg.ic_fw_msg.hw_info[2],
		update_msg.ic_fw_msg.hw_info[3]);

	/*  step2:get firmware message */
	for (retry = 0; retry < 2; retry++) {
		ret = gup_get_ic_msg(client, GUP_REG_FW_MSG, buf, 1);
		if (ret == FAIL) {
			dev_err(&client->dev, "Read firmware message fail.");
			return ret;
		}

		update_msg.fw_damaged = buf[GTP_ADDR_LENGTH];
		if (update_msg.fw_damaged != 0xBE && !retry) {
			dev_info(&client->dev, "The check sum in ic is error.");
			dev_info(&client->dev, "The IC will be updated by force.");
			continue;
		}
		break;
	}
	dev_dbg(&client->dev,
		"IC force update flag:0x%x", update_msg.fw_damaged);

	/*  step3:get pid & vid */
	ret = gtp_i2c_read_dbl_check(client, GUP_REG_PID_VID,
				     &buf[GTP_ADDR_LENGTH], 6);
	if (ret == FAIL) {
		dev_err(&client->dev, "[get_ic_fw_msg]get pid & vid failed,exit");
		return FAIL;
	}

	memset(update_msg.ic_fw_msg.pid, 0, sizeof(update_msg.ic_fw_msg.pid));
	memcpy(update_msg.ic_fw_msg.pid, &buf[GTP_ADDR_LENGTH], 4);
	dev_dbg(&client->dev, "IC Product id:%s", update_msg.ic_fw_msg.pid);

	/* GT9XX PID MAPPING */
	if (update_msg.ic_fw_msg.pid[0] != 0) {
		if (!memcmp(update_msg.ic_fw_msg.pid, "9111", 4)) {
			dev_dbg(&client->dev, "IC Mapping Product id:%s",
					update_msg.ic_fw_msg.pid);
			memcpy(update_msg.ic_fw_msg.pid, "9110P", 5);
		}
	}

	update_msg.ic_fw_msg.vid = buf[GTP_ADDR_LENGTH + 4] +
				   (buf[GTP_ADDR_LENGTH + 5] << 8);
	dev_dbg(&client->dev, "IC version id:%04x", update_msg.ic_fw_msg.vid);

	return SUCCESS;
}

s32 gup_enter_update_mode(struct i2c_client *client)
{
	s32 ret = -1;
	s32 retry = 0;
	u8 rd_buf[3];
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (!gpio_is_valid(ts->pdata->rst_gpio)) {
		dev_err(&ts->client->dev, "update failed, no rst pin\n");
		return FAIL;
	}

	gpio_direction_output(ts->pdata->irq_gpio, 0);
	gpio_direction_output(ts->pdata->rst_gpio, 0);
	msleep(50);

	gtp_int_output(ts, client->addr == 0x14);
	usleep_range(1000, 1100);

	gpio_direction_output(ts->pdata->rst_gpio, 1);
	usleep_range(5000, 5100);
	gpio_direction_output(ts->pdata->irq_gpio, 0);

	while (retry++ < 200) {
		ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
		if (ret <= 0) {
			dev_dbg(&client->dev,
				"Hold ss51 & dsp I2C error,retry:%d",
				retry);
			continue;
		}

		ret = gup_get_ic_msg(client, _rRW_MISCTL__SWRST_B0_, rd_buf, 1);
		if (ret <= 0) {
			dev_dbg(&client->dev,
				"Hold ss51 & dsp I2C error,retry:%d",
				retry);
			continue;
		}
		if (rd_buf[GTP_ADDR_LENGTH] == 0x0C) {
			dev_dbg(&client->dev, "Hold ss51 & dsp confirm SUCCESS");
			break;
		}
		dev_dbg(&client->dev,
			"Hold ss51 & dsp confirm 0x4180 failed,value:%d",
			rd_buf[GTP_ADDR_LENGTH]);
	}
	if (retry >= 200) {
		dev_err(&client->dev, "Enter update Hold ss51 failed.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, 0x4010, 0x00);
	return ret;
}

void gup_leave_update_mode(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->pdata->int_sync && ts->pinctrl.pinctrl)
		pinctrl_select_state(ts->pinctrl.pinctrl,
				     ts->pinctrl.int_input);
	else if (ts->pdata->int_sync && gpio_is_valid(ts->pdata->irq_gpio))
		gpio_direction_input(ts->pdata->irq_gpio);
	dev_dbg(&client->dev, "[leave_update_mode]reset chip.");
	gtp_reset_guitar(client);
}

static u8 gup_enter_update_judge(struct i2c_client *client,
				 struct st_fw_head *fw_head)
{
	u16 u16_tmp;
	s32 i = 0;
	u32 fw_len = 0;
	s32 pid_cmp_len = 0;

	u16_tmp = fw_head->vid;
	fw_head->vid = (u16)(u16_tmp >> 8) + (u16)(u16_tmp << 8);

	dev_info(&client->dev, "FILE HARDWARE INFO:%*ph\n", 4,
		 &fw_head->hw_info[0]);
	dev_info(&client->dev, "FILE PID:%s\n", fw_head->pid);
	dev_info(&client->dev, "FILE VID:%04x\n", fw_head->vid);

	dev_info(&client->dev, "IC HARDWARE INFO:%*ph\n", 4,
		 &update_msg.ic_fw_msg.hw_info[0]);
	dev_info(&client->dev, "IC PID:%s\n", update_msg.ic_fw_msg.pid);
	dev_info(&client->dev, "IC VID:%04x\n", update_msg.ic_fw_msg.vid);

	if (!memcmp(fw_head->pid, "9158", 4) &&
	    !memcmp(update_msg.ic_fw_msg.pid, "915S", 4)) {
		dev_info(&client->dev, "Update GT915S to GT9158 directly!");
		return SUCCESS;
	}

	if (!memcmp(fw_head->hw_info, update_msg.ic_fw_msg.hw_info,
	sizeof(update_msg.ic_fw_msg.hw_info))) {
		fw_len = 42 * 1024;
	} else {
		fw_len = fw_head->hw_info[3];
		fw_len += (((u32)fw_head->hw_info[2]) << 8);
		fw_len += (((u32)fw_head->hw_info[1]) << 16);
		fw_len += (((u32)fw_head->hw_info[0]) << 24);
	}
	if (update_msg.fw_total_len != fw_len) {
		dev_err(&client->dev,
			"Inconsistent firmware size, Update aborted!");
		dev_err(&client->dev,
			" Default size: %d(%dK), actual size: %d(%dK)",
			fw_len, fw_len / 1024, update_msg.fw_total_len,
			update_msg.fw_total_len / 1024);
		return FAIL;
	}
	dev_info(&client->dev, "Firmware length:%d(%dK)",
		 update_msg.fw_total_len,
		 update_msg.fw_total_len / 1024);

	if (update_msg.fw_damaged != 0xBE) {
		dev_info(&client->dev, "FW chksum error,need enter update.");
		return SUCCESS;
	}

	if (strlen(update_msg.ic_fw_msg.pid) < 3) {
		dev_info(&client->dev, "Illegal IC pid, need enter update");
		return SUCCESS;
	}

	for (i = 0; i < 3; i++) {
		if (!isdigit(update_msg.ic_fw_msg.pid[i])) {
			dev_info(&client->dev,
				 "Illegal IC pid, need enter update");
			return SUCCESS;
		}
	}

	pid_cmp_len = strlen(fw_head->pid);
	if (pid_cmp_len < strlen(update_msg.ic_fw_msg.pid))
		pid_cmp_len = strlen(update_msg.ic_fw_msg.pid);

	if ((!memcmp(fw_head->pid, update_msg.ic_fw_msg.pid, pid_cmp_len)) ||
			(!memcmp(update_msg.ic_fw_msg.pid, "91XX", 4)) ||
			(!memcmp(fw_head->pid, "91XX", 4))) {
		if (!memcmp(fw_head->pid, "91XX", 4))
			dev_dbg(&client->dev,
				"Force none same pid update mode.");
		else
			dev_dbg(&client->dev, "Get the same pid.");

		if (fw_head->vid != update_msg.ic_fw_msg.vid) {
			dev_info(&client->dev, "Need enter update.");
			return SUCCESS;
		}
		dev_err(&client->dev, "File VID == Ic VID, update aborted!");
	} else {
		dev_err(&client->dev, "File PID != Ic PID, update aborted!");
	}

	return FAIL;
}

static int gup_update_config(struct i2c_client *client)
{
	s32 ret = 0;
	s32 i = 0;
	s32 file_cfg_len = 0;
	u8 *file_config;
	const struct firmware *fw_cfg;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	ret = request_firmware(&fw_cfg, GOODIX_CONFIG_FILE_NAME,
			       &client->dev);
	if (ret < 0) {
		dev_err(&client->dev,
			"Cannot get config file - %s (%d)\n",
			GOODIX_CONFIG_FILE_NAME, ret);
		return -EFAULT;
	}
	if (!fw_cfg || !fw_cfg->data || fw_cfg->size > PAGE_SIZE) {
		dev_err(&client->dev, "config file illegal");
		ret = -EFAULT;
		goto cfg_fw_err;
	}

	dev_dbg(&client->dev, "config firmware file len:%zu", fw_cfg->size);

	file_config = kzalloc(GTP_CONFIG_MAX_LENGTH + GTP_ADDR_LENGTH,
			      GFP_KERNEL);
	if (!file_config) {
		ret = -ENOMEM;
		goto cfg_fw_err;
	}
	file_config[0] = GTP_REG_CONFIG_DATA >> 8;
	file_config[1] = GTP_REG_CONFIG_DATA & 0xff;
	file_cfg_len = gtp_ascii_to_array(fw_cfg->data, fw_cfg->size,
					  &file_config[GTP_ADDR_LENGTH]);
	if (file_cfg_len < 0) {
		dev_err(&client->dev, "failed covert ascii to hex");
		ret = -EFAULT;
		goto update_cfg_file_failed;
	}

	GTP_DEBUG_ARRAY(file_config + GTP_ADDR_LENGTH, file_cfg_len);

	i = 0;
	while (i++ < 5) {
		ret = gtp_i2c_write(client, file_config, file_cfg_len + 2);
		if (ret > 0) {
			dev_info(&client->dev, "Send config SUCCESS.");
			msleep(1000);
			break;
		}
		dev_err(&ts->client->dev, "Send config i2c error.");
	}

update_cfg_file_failed:
	kfree(file_config);
cfg_fw_err:
	release_firmware(fw_cfg);
	return ret;
}

static u8 gup_check_firmware_name(struct i2c_client *client,
		u8 **path_p)
{
	u8 len;
	u8 *fname;

	if (!(*path_p)) {
		*path_p = GOODIX_FIRMWARE_FILE_NAME;
		return 0;
	}

	len = strnlen(*path_p, FIRMWARE_NAME_LEN_MAX);
	if (len >= FIRMWARE_NAME_LEN_MAX) {
		dev_err(&client->dev, "firmware name too long!");
		return -EINVAL;
	}

	fname = strrchr(*path_p, '/');
	if (fname) {
		fname = fname + 1;
		*path_p = fname;
	}

	return 0;
}

static u8 gup_get_update_file(struct i2c_client *client,
				struct st_fw_head *fw_head, u8 *path)
{
	s32 ret = 0;
	s32 i = 0;
	s32 fw_checksum = 0;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);

	if (ts->pdata->auto_update_cfg) {
		ret = gup_update_config(client);
		if (ret <= 0)
			dev_err(&client->dev, "Update config failed.");
	}

	ret = gup_check_firmware_name(client, &path);
	if (ret < 0)
		return FAIL;

	ret = request_firmware(&update_msg.fw, path, &client->dev);
	if (ret < 0) {
		dev_err(&client->dev, "Failed get firmware:%d\n", ret);
		return FAIL;
	}

	dev_info(&client->dev, "FW File: %s size=%zu",
		 path, update_msg.fw->size);
	update_msg.fw_data = update_msg.fw->data;
	update_msg.fw_total_len = update_msg.fw->size;

	if (update_msg.fw_total_len <
	    FW_HEAD_LENGTH + FW_SECTION_LENGTH * 4 + FW_DSP_ISP_LENGTH +
	    FW_DSP_LENGTH + FW_BOOT_LENGTH) {
		dev_err(&client->dev,
			"INVALID bin file(size: %d), update aborted.",
			update_msg.fw_total_len);
		goto invalid_fw;
	}

	update_msg.fw_total_len -= FW_HEAD_LENGTH;

	dev_dbg(&client->dev, "Bin firmware actual size: %d(%dK)",
		update_msg.fw_total_len, update_msg.fw_total_len / 1024);

	memcpy(fw_head, update_msg.fw_data, FW_HEAD_LENGTH);

	fw_checksum = 0;
	for (i = 0; i < update_msg.fw_total_len; i += 2) {
		u16 temp;

		temp = (update_msg.fw_data[FW_HEAD_LENGTH + i] << 8) +
			update_msg.fw_data[FW_HEAD_LENGTH + i + 1];
		fw_checksum += temp;
	}

	dev_dbg(&client->dev, "firmware checksum:%x", fw_checksum & 0xFFFF);
	if (fw_checksum & 0xFFFF) {
		dev_err(&client->dev, "Illegal firmware file.");
		goto invalid_fw;
	}

	return SUCCESS;

invalid_fw:
	update_msg.fw_data = NULL;
	update_msg.fw_total_len = 0;
	release_firmware(update_msg.fw);
	return FAIL;
}

static u8 gup_burn_proc(struct i2c_client *client, u8 *burn_buf,
		u16 start_addr, u16 total_length)
{
	s32 ret = 0;
	u16 burn_addr = start_addr;
	u16 frame_length = 0;
	u16 burn_length = 0;
	u8 wr_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	u8 rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	u8 retry = 0;

	dev_dbg(&client->dev, "Begin burn %dk data to addr 0x%x",
		total_length / 1024, start_addr);
	while (burn_length < total_length) {
		dev_dbg(&client->dev,
			"B/T:%04d/%04d", burn_length, total_length);
		frame_length = ((total_length - burn_length)
		> PACK_SIZE) ? PACK_SIZE : (total_length - burn_length);
		wr_buf[0] = (u8)(burn_addr >> 8);
		rd_buf[0] = wr_buf[0];
		wr_buf[1] = (u8)burn_addr;
		rd_buf[1] = wr_buf[1];
		memcpy(&wr_buf[GTP_ADDR_LENGTH],
		&burn_buf[burn_length], frame_length);

		for (retry = 0; retry < MAX_FRAME_CHECK_TIME; retry++) {
			ret = gtp_i2c_write(client,
			wr_buf, GTP_ADDR_LENGTH + frame_length);
			if (ret <= 0) {
				dev_err(&client->dev,
					"Write frame data i2c error.");
				continue;
			}
			ret = gtp_i2c_read(client, rd_buf,
					   GTP_ADDR_LENGTH + frame_length);
			if (ret <= 0) {
				dev_err(&client->dev,
					"Read back frame data i2c error.");
				continue;
			}
			if (memcmp(&wr_buf[GTP_ADDR_LENGTH],
				&rd_buf[GTP_ADDR_LENGTH], frame_length)) {
				dev_err(&client->dev,
					"Check frame data fail,not equal.");
				dev_dbg(&client->dev, "write array:");
				GTP_DEBUG_ARRAY(&wr_buf[GTP_ADDR_LENGTH],
				frame_length);
				dev_dbg(&client->dev, "read array:");
				GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH],
				frame_length);
				continue;
			} else {
				break;
			}
		}
		if (retry >= MAX_FRAME_CHECK_TIME) {
			dev_err(&client->dev,
				"Burn frame data time out,exit.");
			return FAIL;
		}
		burn_length += frame_length;
		burn_addr += frame_length;
	}

	return SUCCESS;
}

static u8 gup_load_section_file(u8 *buf, u32 offset, u16 length, u8 set_or_end)
{
	if (!update_msg.fw_data ||
	    update_msg.fw_total_len < offset + length) {
		dev_err(&gup_client->dev,
			"cannot load section data. fw_len=%d read end=%d\n",
			update_msg.fw_total_len,
			FW_HEAD_LENGTH + offset + length);
		return FAIL;
	}

	if (set_or_end == SEEK_SET) {
		memcpy(buf, &update_msg.fw_data[FW_HEAD_LENGTH + offset],
		       length);
	} else {
		memcpy(buf, &update_msg.fw_data[update_msg.fw_total_len +
		       FW_HEAD_LENGTH - offset], length);
	}

	return SUCCESS;
}

static u8 gup_recall_check(struct i2c_client *client, u8 *chk_src,
			   u16 start_rd_addr, u16 chk_length)
{
	u8 rd_buf[PACK_SIZE + GTP_ADDR_LENGTH];
	s32 ret = 0;
	u16 recall_addr = start_rd_addr;
	u16 recall_length = 0;
	u16 frame_length = 0;

	while (recall_length < chk_length) {
		frame_length = ((chk_length - recall_length)
				> PACK_SIZE) ? PACK_SIZE :
				(chk_length - recall_length);
		ret = gup_get_ic_msg(client, recall_addr, rd_buf, frame_length);
		if (ret <= 0) {
			dev_err(&client->dev, "recall i2c error,exit");
			return FAIL;
		}

		if (memcmp(&rd_buf[GTP_ADDR_LENGTH],
		&chk_src[recall_length], frame_length)) {
			dev_err(&client->dev, "Recall frame data fail,not equal.");
			dev_dbg(&client->dev, "chk_src array:");
			GTP_DEBUG_ARRAY(&chk_src[recall_length], frame_length);
			dev_dbg(&client->dev, "recall array:");
			GTP_DEBUG_ARRAY(&rd_buf[GTP_ADDR_LENGTH], frame_length);
			return FAIL;
		}

		recall_length += frame_length;
		recall_addr += frame_length;
	}
	dev_dbg(&client->dev,
			"Recall check %dk firmware success.",
			(chk_length / 1024));

	return SUCCESS;
}

static u8 gup_burn_fw_section(struct i2c_client *client, u8 *fw_section,
			      u16 start_addr, u8 bank_cmd)
{
	s32 ret = 0;
	u8	rd_buf[5];

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_section]hold ss51 & dsp fail.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_section]set scramble fail.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK,
	(bank_cmd >> 4) & 0x0F);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_section]select bank %d fail.",
			(bank_cmd >> 4) & 0x0F);
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_section]enable accessing code fail.");
		return FAIL;
	}

	ret = gup_burn_proc(client, fw_section, start_addr, FW_SECTION_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_section]burn fw_section fail.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_section]hold ss51 & release dsp fail.");
		return FAIL;
	}
	usleep_range(1000, 2000);

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, bank_cmd & 0x0f);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_section]send burn cmd fail.");
		return FAIL;
	}
	dev_dbg(&client->dev,
		"[burn_fw_section]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			dev_err(&client->dev,
				"[burn_fw_section]Get burn state fail");
			return FAIL;
		}
		usleep_range(10000, 11000);
	} while (rd_buf[GTP_ADDR_LENGTH]);

	ret = gup_set_ic_msg(client,
			     _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4) & 0x0F);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_section]select bank %d fail.",
			(bank_cmd >> 4) & 0x0F);
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_section]enable accessing code fail.");
		return FAIL;
	}

	ret = gup_recall_check(client,
	fw_section, start_addr, FW_SECTION_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_section]recall check %dk firmware fail.",
			FW_SECTION_LENGTH / 1024);
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_section]disable accessing code fail.");
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_burn_dsp_isp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_dsp_isp = NULL;
	u8	retry = 0;

	dev_info(&client->dev, "[burn_dsp_isp]Begin burn dsp isp---->>");

	while (retry++ < 5) {
		fw_dsp_isp = kzalloc(FW_DSP_ISP_LENGTH, GFP_KERNEL);
		if (!fw_dsp_isp) {
			continue;
		} else {
			dev_info(&client->dev,
				"[burn_dsp_isp]Alloc %dk byte memory success.",
				FW_DSP_ISP_LENGTH / 1024);
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev, "[burn_dsp_isp]Alloc memory fail,exit.");
		return FAIL;
	}

	ret = gup_load_section_file(fw_dsp_isp, FW_DSP_ISP_LENGTH,
				    FW_DSP_ISP_LENGTH, SEEK_END);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_dsp_isp]load firmware dsp_isp fail.");
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__TMR0_EN, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_dsp_isp]disable wdt fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}
	ret = gup_set_ic_msg(client, _bRW_MISCTL__CACHE_EN, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_dsp_isp]clear cache enable fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_dsp_isp]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOTCTL_B0_, 0x02);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_dsp_isp]set boot from sram fail");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _bWO_MISCTL__CPU_SWRST_PULSE, 0x01);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_dsp_isp]software reboot fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x02);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_dsp_isp]select bank2 fail");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__MEM_CD_EN, 0x01);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_dsp_isp]enable accessing code fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}

	ret = gup_burn_proc(client, fw_dsp_isp, 0xC000, FW_DSP_ISP_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev, "[burn_dsp_isp]burn dsp_isp fail.");
		goto exit_burn_dsp_isp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_dsp_isp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_dsp_isp;
	}
	update_msg.fw_burned_len += FW_DSP_ISP_LENGTH;
	dev_dbg(&client->dev, "[burn_dsp_isp]Burned length:%d",
			update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_dsp_isp:
	kfree(fw_dsp_isp);
	return ret;
}

static u8 gup_burn_fw_ss51(struct i2c_client *client)
{
	u8 *fw_ss51 = NULL;
	u8	retry = 0;
	s32 ret = 0;

	dev_info(&client->dev, "[burn_fw_ss51]Begin burn ss51 firmware---->>");

	while (retry++ < 5) {
		fw_ss51 = kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (!fw_ss51) {
			continue;
		} else {
			dev_dbg(&client->dev,
				"[burn_fw_ss51]Alloc %dk byte memory success.",
				(FW_SECTION_LENGTH / 1024));
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev, "[burn_fw_ss51]Alloc memory fail,exit.");
		return FAIL;
	}

	dev_info(&client->dev, "[burn_fw_ss51]Reset first 8K of ss51 to 0xFF.");
	memset(fw_ss51, 0xFF, FW_SECTION_LENGTH);

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_ss51]clear control flag fail.");
		ret = FAIL;
		goto exit_burn_fw_ss51;
	}

	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x01);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_ss51]burn ss51 firmware section 1 fail.");
		goto exit_burn_fw_ss51;
	}

	ret = gup_load_section_file(fw_ss51, FW_SECTION_LENGTH,
				    FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_ss51]load ss51 firmware section 2 fail.");
		goto exit_burn_fw_ss51;
	}

	ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x02);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_ss51]burn ss51 firmware section 2 fail.");
		goto exit_burn_fw_ss51;
	}

	ret = gup_load_section_file(fw_ss51, 2 * FW_SECTION_LENGTH,
				    FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_ss51]load ss51 firmware section 3 fail.");
		goto exit_burn_fw_ss51;
	}

	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x13);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_ss51]burn ss51 firmware section 3 fail.");
		goto exit_burn_fw_ss51;
	}

	ret = gup_load_section_file(fw_ss51, 3 * FW_SECTION_LENGTH,
				    FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_ss51]load ss51 firmware section 4 fail.");
		goto exit_burn_fw_ss51;
	}

	ret = gup_burn_fw_section(client, fw_ss51, 0xE000, 0x14);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_ss51]burn ss51 firmware section 4 fail.");
		goto exit_burn_fw_ss51;
	}

	update_msg.fw_burned_len += (FW_SECTION_LENGTH * 4);
	dev_dbg(&client->dev, "[burn_fw_ss51]Burned length:%d",
		update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_ss51:
	kfree(fw_ss51);
	return ret;
}

static u8 gup_burn_fw_dsp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_dsp = NULL;
	u8 retry = 0;
	u8 rd_buf[5];

	dev_info(&client->dev, "[burn_fw_dsp]Begin burn dsp firmware---->>");
	while (retry++ < 5) {
		fw_dsp = kzalloc(FW_DSP_LENGTH, GFP_KERNEL);
		if (!fw_dsp) {
			continue;
		} else {
			dev_dbg(&client->dev,
				"[burn_fw_dsp]Alloc %dk byte memory success.",
				FW_SECTION_LENGTH / 1024);
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev, "[burn_fw_dsp]Alloc memory fail,exit.");
		return FAIL;
	}

	ret = gup_load_section_file(fw_dsp, 4 * FW_SECTION_LENGTH,
				    FW_DSP_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev, "[burn_fw_dsp]load firmware dsp fail.");
		goto exit_burn_fw_dsp;
	}

	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_dsp]select bank3 fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_dsp]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_dsp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_dsp]release ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_dsp;
	}
	usleep_range(1000, 1100);

	ret = gup_burn_proc(client, fw_dsp, 0x9000, FW_DSP_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev, "[burn_fw_dsp]burn fw_section fail.");
		goto exit_burn_fw_dsp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x05);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_dsp]send burn cmd fail.");
		goto exit_burn_fw_dsp;
	}
	dev_dbg(&client->dev, "[burn_fw_dsp]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			dev_err(&client->dev, "[burn_fw_dsp]Get burn state fail");
			goto exit_burn_fw_dsp;
		}
		usleep_range(10000, 11000);
	} while (rd_buf[GTP_ADDR_LENGTH]);

	ret = gup_recall_check(client, fw_dsp, 0x9000, FW_DSP_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_dsp]recall check 4k dsp firmware fail.");
		goto exit_burn_fw_dsp;
	}

	update_msg.fw_burned_len += FW_DSP_LENGTH;
	dev_dbg(&client->dev, "[burn_fw_dsp]Burned length:%d",
			update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_dsp:
	kfree(fw_dsp);
	return ret;
}

static u8 gup_burn_fw_boot(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_boot = NULL;
	u8	retry = 0;
	u8	rd_buf[5];

	dev_info(&client->dev,
		 "[burn_fw_boot]Begin burn bootloader firmware---->>");

	while (retry++ < 5) {
		fw_boot = kzalloc(FW_BOOT_LENGTH, GFP_KERNEL);
		if (!fw_boot) {
			continue;
		} else {
			dev_dbg(&client->dev,
				"[burn_fw_boot]Alloc %dk byte memory success.",
				FW_BOOT_LENGTH / 1024);
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev, "[burn_fw_boot]Alloc memory fail,exit.");
		return FAIL;
	}

	ret = gup_load_section_file(fw_boot,
				    4 * FW_SECTION_LENGTH + FW_DSP_LENGTH,
				    FW_BOOT_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_boot]load firmware bootcode fail.");
		goto exit_burn_fw_boot;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot]hold ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot]release ss51 & dsp fail");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}
	usleep_range(1000, 1100);

	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot]select bank3 fail.");
		ret = FAIL;
		goto exit_burn_fw_boot;
	}

	ret = gup_burn_proc(client, fw_boot, 0x9000, FW_BOOT_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev, "[burn_fw_boot]burn fw_boot fail.");
		goto exit_burn_fw_boot;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x06);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot]send burn cmd fail.");
		goto exit_burn_fw_boot;
	}
	dev_dbg(&client->dev,
		"[burn_fw_boot]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			dev_err(&client->dev,
				"[burn_fw_boot]Get burn state fail");
			goto exit_burn_fw_boot;
		}
		usleep_range(10000, 11000);
	} while (rd_buf[GTP_ADDR_LENGTH]);

	ret = gup_recall_check(client, fw_boot, 0x9000, FW_BOOT_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_boot]recall check 2k bootcode firmware fail");
		goto exit_burn_fw_boot;
	}

	update_msg.fw_burned_len += FW_BOOT_LENGTH;
	dev_dbg(&client->dev, "[burn_fw_boot]Burned length:%d",
		update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_boot:
	kfree(fw_boot);
	return ret;
}

static u8 gup_burn_fw_boot_isp(struct i2c_client *client)
{
	s32 ret = 0;
	u8 *fw_boot_isp = NULL;
	u8	retry = 0;
	u8	rd_buf[5];

	if (update_msg.fw_burned_len >= update_msg.fw_total_len) {
		dev_dbg(&client->dev, "No need to upgrade the boot_isp code!");
		return SUCCESS;
	}
	dev_info(&client->dev,
		"[burn_fw_boot_isp]Begin burn boot_isp firmware---->>");

	while (retry++ < 5) {
		fw_boot_isp = kzalloc(FW_BOOT_ISP_LENGTH, GFP_KERNEL);
		if (!fw_boot_isp) {
			continue;
		} else {
			dev_dbg(&client->dev,
				"[burn_fw_boot_isp]Alloc %dk byte memory success.",
				(FW_BOOT_ISP_LENGTH / 1024));
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev,
			"[burn_fw_boot_isp]Alloc memory fail,exit.");
		return FAIL;
	}

	ret = gup_load_section_file(fw_boot_isp, (update_msg.fw_burned_len - FW_DSP_ISP_LENGTH),
				    FW_BOOT_ISP_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_boot_isp]load firmware boot_isp fail.");
		goto exit_burn_fw_boot_isp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot_isp]hold ss51 & dsp fail");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot_isp]set scramble fail.");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_boot_isp]release ss51 & dsp fail.");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}
	usleep_range(1000, 2000);

	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, 0x03);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot_isp]select bank3 fail.");
		ret = FAIL;
		goto exit_burn_fw_boot_isp;
	}

	ret = gup_burn_proc(client, fw_boot_isp, 0x9000, FW_BOOT_ISP_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_boot_isp]burn fw_section fail.");
		goto exit_burn_fw_boot_isp;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x07);
	if (ret <= 0) {
		dev_err(&client->dev, "[burn_fw_boot_isp]send burn cmd fail.");
		goto exit_burn_fw_boot_isp;
	}
	dev_dbg(&client->dev,
		"[burn_fw_boot_isp]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			dev_err(&client->dev,
				"[burn_fw_boot_isp]Get burn state fail");
			goto exit_burn_fw_boot_isp;
		}
		usleep_range(10000, 11000);
	} while (rd_buf[GTP_ADDR_LENGTH]);

	ret = gup_recall_check(client, fw_boot_isp, 0x9000, FW_BOOT_ISP_LENGTH);
	if (ret == FAIL) {
		dev_err(&client->dev,
		"[burn_fw_boot_isp]recall check 2k bootcode_isp firmware fail.");
		goto exit_burn_fw_boot_isp;
	}

	update_msg.fw_burned_len += FW_BOOT_ISP_LENGTH;
	dev_dbg(&client->dev,
		"[burn_fw_boot_isp]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_boot_isp:
	kfree(fw_boot_isp);
	return ret;
}

static u8 gup_burn_fw_link(struct i2c_client *client)
{
	u8 *fw_link = NULL;
	u8	retry = 0;
	s32 ret = 0;
	u32 offset;

	if (update_msg.fw_burned_len >= update_msg.fw_total_len) {
		dev_dbg(&client->dev, "No need to upgrade the link code!");
		return SUCCESS;
	}
	dev_info(&client->dev, "[burn_fw_link]Begin burn link firmware---->>");

	while (retry++ < 5) {
		fw_link = kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (!fw_link) {
			continue;
		} else {
			dev_dbg(&client->dev,
				"[burn_fw_link]Alloc %dk byte memory success.",
				(FW_SECTION_LENGTH / 1024));
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev, "[burn_fw_link]Alloc memory fail,exit.");
		return FAIL;
	}

	offset = update_msg.fw_burned_len - FW_DSP_ISP_LENGTH;
	ret = gup_load_section_file(fw_link, offset, FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_link]load firmware link section 1 fail.");
		goto exit_burn_fw_link;
	}

	ret = gup_burn_fw_gwake_section(client, fw_link, 0x9000, FW_SECTION_LENGTH, 0x38);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_link]burn link firmware section 1 fail.");
		goto exit_burn_fw_link;
	}

	offset += FW_SECTION_LENGTH;
	ret = gup_load_section_file(fw_link, offset,
				    FW_GLINK_LENGTH - FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_link]load link firmware section 2 fail.");
		goto exit_burn_fw_link;
	}

	ret = gup_burn_fw_gwake_section(client, fw_link, 0x9000,
					FW_GLINK_LENGTH - FW_SECTION_LENGTH, 0x39);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_link]burn link firmware section 2 fail.");
		goto exit_burn_fw_link;
	}

	update_msg.fw_burned_len += FW_GLINK_LENGTH;
	dev_dbg(&client->dev,
		"[burn_fw_link]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_link:
	kfree(fw_link);
	return ret;
}

static u8 gup_burn_fw_gwake_section(struct i2c_client *client,
		u8 *fw_section, u16 start_addr, u32 len, u8 bank_cmd)
{
	s32 ret = 0;
	u8	rd_buf[5];

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x0C);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_app_section]hold ss51 & dsp fail.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_OPT_B0_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_app_section]set scramble fail.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x04);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_app_section]hold ss51 & release dsp fail.");
		return FAIL;
	}
	usleep_range(1000, 2000);

	ret = gup_set_ic_msg(client, _bRW_MISCTL__SRAM_BANK, (bank_cmd >> 4) & 0x0F);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_section]select bank %d fail.",
			(bank_cmd >> 4) & 0x0F);
		return FAIL;
	}

	ret = gup_burn_proc(client, fw_section, start_addr, len);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_app_section]burn fw_section fail.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, bank_cmd & 0x0F);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_app_section]send burn cmd fail.");
		return FAIL;
	}
	dev_dbg(&client->dev,
		"[burn_fw_section]Wait for the burn is complete......");
	do {
		ret = gup_get_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, rd_buf, 1);
		if (ret <= 0) {
			dev_err(&client->dev,
				"[burn_fw_app_section]Get burn state fail");
			return FAIL;
		}
		usleep_range(10000, 11000);
	} while (rd_buf[GTP_ADDR_LENGTH]);

	ret = gup_recall_check(client, fw_section, start_addr, len);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_app_section]recall check %dk firmware fail.",
			len / 1024);
		return FAIL;
	}

	return SUCCESS;
}

static u8 gup_burn_fw_gwake(struct i2c_client *client)
{
	u8 *fw_gwake = NULL;
	u8	retry = 0;
	s32 ret = 0;
	u16 start_index = 4 * FW_SECTION_LENGTH +
	FW_DSP_LENGTH + FW_BOOT_LENGTH +
	FW_BOOT_ISP_LENGTH + FW_GLINK_LENGTH;

	if (start_index >= update_msg.fw_total_len) {
		dev_dbg(&client->dev, "No need to upgrade the gwake code!");
		return SUCCESS;
	}
	dev_info(&client->dev,
		 "[burn_fw_gwake]Begin burn gwake firmware---->>");

	while (retry++ < 5) {
		fw_gwake = kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (!fw_gwake) {
			continue;
		} else {
			dev_dbg(&client->dev,
				"[burn_fw_gwake]Alloc %dk byte memory success.",
				(FW_SECTION_LENGTH / 1024));
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev, "[burn_fw_gwake]Alloc memory fail,exit.");
		return FAIL;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_finish]clear control flag fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_load_section_file(fw_gwake, start_index,
				    FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]load app_code firmware section 1 fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000,
					FW_SECTION_LENGTH, 0x3A);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]burn app_code firmware section 1 fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_load_section_file(fw_gwake, start_index + FW_SECTION_LENGTH,
				    FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]load app_code firmware section 2 fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000,
					FW_SECTION_LENGTH, 0x3B);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]burn app_code firmware section 2 fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_load_section_file(fw_gwake, start_index + 2 * FW_SECTION_LENGTH,
				    FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]load app_code firmware section 3 fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000,
					FW_SECTION_LENGTH, 0x3C);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]burn app_code firmware section 3 fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_load_section_file(fw_gwake, start_index + 3 * FW_SECTION_LENGTH,
				    FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]load app_code firmware section 4 fail.");
		goto exit_burn_fw_gwake;
	}

	ret = gup_burn_fw_gwake_section(client, fw_gwake, 0x9000,
					FW_SECTION_LENGTH, 0x3D);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_gwake]burn app_code firmware section 4 fail.");
		goto exit_burn_fw_gwake;
	}

	dev_dbg(&client->dev,
		"[burn_fw_gwake]Burned length:%d", update_msg.fw_burned_len);
	ret = SUCCESS;

exit_burn_fw_gwake:
	kfree(fw_gwake);
	return ret;
}

static u8 gup_burn_fw_finish(struct i2c_client *client)
{
	u8 *fw_ss51 = NULL;
	u8	retry = 0;
	s32 ret = 0;

	dev_info(&client->dev,
		 "[burn_fw_finish]burn first 8K of ss51 and finish update.");
	while (retry++ < 5) {
		fw_ss51 = kzalloc(FW_SECTION_LENGTH, GFP_KERNEL);
		if (!fw_ss51) {
			continue;
		} else {
			dev_dbg(&client->dev,
				"[burn_fw_finish]Alloc %dk byte memory success.",
				(FW_SECTION_LENGTH / 1024));
			break;
		}
	}
	if (retry >= 5) {
		dev_err(&client->dev,
			"[burn_fw_finish]Alloc memory fail,exit.");
		return FAIL;
	}

	ret = gup_load_section_file(fw_ss51, 0, FW_SECTION_LENGTH, SEEK_SET);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_finish]load ss51 firmware section 1 fail.");
		goto exit_burn_fw_finish;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x00);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_finish]clear control flag fail.");
		goto exit_burn_fw_finish;
	}

	ret = gup_burn_fw_section(client, fw_ss51, 0xC000, 0x01);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[burn_fw_finish]burn ss51 firmware section 1 fail.");
		goto exit_burn_fw_finish;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__BOOT_CTL_, 0x99);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_finish]enable download DSP code fail.");
		goto exit_burn_fw_finish;
	}

	ret = gup_set_ic_msg(client, _rRW_MISCTL__SWRST_B0_, 0x08);
	if (ret <= 0) {
		dev_err(&client->dev,
			"[burn_fw_finish]release ss51 & hold dsp fail.");
		goto exit_burn_fw_finish;
	}

	kfree(fw_ss51);
	return SUCCESS;

exit_burn_fw_finish:
	kfree(fw_ss51);
	return FAIL;
}

static int gup_update_condition_check(struct goodix_ts_data *ts)
{
	if (test_bit(SLEEP_MODE, &ts->flags)) {
		dev_info(&ts->client->dev, "Update abort, tp in sleep mode\n");
		return -EINVAL;
	}
	return 0;
}

s32 gup_update_proc(struct goodix_ts_data *ts)
{
	s32 ret = 0;
	s32 update_ret = FAIL;
	u8	retry = 0;
	struct st_fw_head fw_head;
	struct i2c_client *client = ts->client;

	dev_dbg(&client->dev, "[update_proc]Begin update ......\n");

	show_len = 1;
	total_len = 100;

	ret = gup_update_condition_check(ts);
	if (ret) {
		dev_warn(&client->dev, "Update start failed\n");
		return FAIL;
	}

	if (test_and_set_bit(FW_UPDATE_RUNNING, &ts->flags)) {
		dev_warn(&client->dev, "FW update may already running\n");
		return FAIL;
	}

	gup_client = client;

	ret = gup_get_update_file(client, &fw_head, (u8 *)gup_dir);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"Failed get valied firmware data\n");
		clear_bit(FW_UPDATE_RUNNING, &ts->flags);
		gup_client = NULL;
		return FAIL;
	}

	gtp_work_control_enable(ts, false);
	gtp_esd_off(ts);

	ret = gup_get_ic_fw_msg(client);
	if (ret == FAIL) {
		dev_err(&client->dev, "[update_proc]get ic message fail.");
		goto file_fail;
	}

	if (ts->force_update || gup_dir) {
		dev_dbg(&client->dev, "Enter force update.");
	} else {
		ret = gup_enter_update_judge(client, &fw_head);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]Doesn't meet update condition\n");
			goto file_fail;
		}
	}

	ret = gup_enter_update_mode(client);
	if (ret == FAIL) {
		dev_err(&client->dev,
			"[update_proc]enter update mode fail.");
		goto update_fail;
	}

	while (retry++ < 5) {
		show_len = 10;
		total_len = 100;
		update_msg.fw_burned_len = 0;
		ret = gup_burn_dsp_isp(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn dsp isp fail.");
			continue;
		}

		show_len = 20;
		ret = gup_burn_fw_gwake(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn app_code firmware fail.");
			continue;
		}

		show_len = 30;
		ret = gup_burn_fw_ss51(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn ss51 firmware fail.");
			continue;
		}

		show_len = 40;
		ret = gup_burn_fw_dsp(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn dsp firmware fail.");
			continue;
		}

		show_len = 50;
		ret = gup_burn_fw_boot(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn bootloader firmware fail.");
			continue;
		}
		show_len = 60;

		ret = gup_burn_fw_boot_isp(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn boot_isp firmware fail.");
			continue;
		}

		show_len = 70;
		ret = gup_burn_fw_link(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn link firmware fail.");
			continue;
		}

		show_len = 80;
		ret = gup_burn_fw_finish(client);
		if (ret == FAIL) {
			dev_err(&client->dev,
				"[update_proc]burn finish fail.");
			continue;
		}
		show_len = 90;
		dev_info(&client->dev, "[update_proc]UPDATE SUCCESS.");
		retry = 0;
		break;
	}

	if (retry >= 5) {
		dev_err(&client->dev,
			"[update_proc]retry timeout,UPDATE FAIL.");
		update_ret = FAIL;
	} else {
		update_ret = SUCCESS;
	}

update_fail:
	dev_dbg(&client->dev, "[update_proc]leave update mode.");
	gup_leave_update_mode(client);

	msleep(GTP_100_DLY_MS);

	if (update_ret == SUCCESS) {
		dev_info(&client->dev,
			"firmware error auto update, resent config!\n");
		gup_init_panel(ts);
	}
	gtp_get_fw_info(client, &ts->fw_info);

file_fail:
	update_msg.fw_data = NULL;
	update_msg.fw_total_len = 0;
	release_firmware(update_msg.fw);

	clear_bit(FW_UPDATE_RUNNING, &ts->flags);
	gtp_work_control_enable(ts, true);
	gtp_esd_on(ts);
	total_len = 100;
	ts->force_update = false;
	gup_client = NULL;
	if (update_ret == SUCCESS) {
		show_len = 100;
		clear_bit(FW_ERROR, &ts->flags);
		return SUCCESS;
	}
	show_len = 200;
	return FAIL;
}

static int gup_update_thread(void *data)
{
	struct goodix_ts_data *ts = (struct goodix_ts_data *)data;

	gup_dir = NULL;
	gup_update_proc(ts);
	return 0;
}

u8 gup_init_update_proc(struct goodix_ts_data *ts)
{
	struct task_struct *thread = NULL;

	dev_info(&ts->client->dev, "Ready to run update thread.");

	thread = kthread_run(gup_update_thread, (void *)ts, "guitar_update");
	if (IS_ERR(thread)) {
		dev_err(&ts->client->dev,
				"Failed to create update thread.\n");
		return -EPERM;
	}

	return 0;
}

MODULE_AUTHOR("Amlogic, Inc");
MODULE_DESCRIPTION("GT9 serials touch controller Driver");
MODULE_LICENSE("GPL v2");
