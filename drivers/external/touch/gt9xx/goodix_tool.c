// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include "gt9xx.h"
#define DATA_LENGTH_UINT	512
#define CMD_HEAD_LENGTH		(sizeof(struct st_cmd_head) - sizeof(u8 *))

#pragma pack(1)
struct st_cmd_head {
	u8	wr;		/*write read flag 0:R 1:W 2:PID 3:*/
	u8	flag;		/*0:no need flag/int 1: need flag 2:need int*/
	u8	flag_addr[2];	/*flag address*/
	u8	flag_val;	/*flag val*/
	u8	flag_relation;	/*flag_val:flag 0:not equal 1:equal 2:> 3:<*/
	u16	circle;		/*polling cycle*/
	u8	times;		/*plling times*/
	u8	retry;		/*I2C retry times*/
	u16	delay;		/*delay before read or after write*/
	u16	data_len;	/*data length*/
	u8	addr_len;	/*address length*/
	u8	addr[2];	/*address*/
	u8	res[3];		/*reserved*/
	u8	*data; };	/*data pointer*/
#pragma pack()
struct st_cmd_head cmd_head;

static ssize_t goodix_tool_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t goodix_tool_write(struct file *, const char __user *, size_t, loff_t *);

static const struct proc_ops gtp_proc_ops = {
	.proc_read = goodix_tool_read,
	.proc_write = goodix_tool_write,
};

static s32 (*tool_i2c_read)(struct goodix_ts_data *ts, u8 *, u16);
static s32 (*tool_i2c_write)(struct goodix_ts_data *ts, u8 *, u16);

static s32 DATA_LENGTH = (s32)0;
static s8 IC_TYPE[16] = "GT9XX";

/*---------------------- I2C read/write implementations ---------------------*/
static s32 tool_i2c_read_no_extra(struct goodix_ts_data *ts, u8 *buf, u16 len)
{
	s32 ret = -1;
	int i;

	for (i = 0; i < cmd_head.retry; i++) {
		ret = gtp_i2c_read(ts->client, buf, len + GTP_ADDR_LENGTH);
		if (ret > 0)
			break;
	}
	return ret;
}

static s32 tool_i2c_write_no_extra(struct goodix_ts_data *ts, u8 *buf, u16 len)
{
	s32 ret = -1;
	int i;

	for (i = 0; i < cmd_head.retry; i++) {
		ret = gtp_i2c_write(ts->client, buf, len);
		if (ret > 0)
			break;
	}
	return ret;
}

static s32 tool_i2c_read_with_extra(struct goodix_ts_data *ts, u8 *buf, u16 len)
{
	u8 pre[2] = {0x0f, 0xff};
	u8 end[2] = {0x80, 0x00};

	tool_i2c_write_no_extra(ts, pre, 2);
	s32 ret = tool_i2c_read_no_extra(ts, buf, len);

	tool_i2c_write_no_extra(ts, end, 2);
	return ret;
}

static s32 tool_i2c_write_with_extra(struct goodix_ts_data *ts, u8 *buf, u16 len)
{
	u8 pre[2] = {0x0f, 0xff};
	u8 end[2] = {0x80, 0x00};

	tool_i2c_write_no_extra(ts, pre, 2);
	s32 ret = tool_i2c_write_no_extra(ts, buf, len);

	tool_i2c_write_no_extra(ts, end, 2);
	return ret;
}

static void register_i2c_func(struct i2c_client *client)
{
	if (strncmp(IC_TYPE, "GT8110", 6) &&
	    strncmp(IC_TYPE, "GT8105", 6) &&
	    strncmp(IC_TYPE, "GT801", 5) &&
	    strncmp(IC_TYPE, "GT800", 5) &&
	    strncmp(IC_TYPE, "GT801PLUS", 9) &&
	    strncmp(IC_TYPE, "GT811", 5) &&
	    strncmp(IC_TYPE, "GTxxx", 5) &&
	    strncmp(IC_TYPE, "GT9XX", 5)) {
		tool_i2c_read = tool_i2c_read_with_extra;
		tool_i2c_write = tool_i2c_write_with_extra;
		dev_dbg(&client->dev, "I2C function: with pre and end cmd!");
	} else {
		tool_i2c_read = tool_i2c_read_no_extra;
		tool_i2c_write = tool_i2c_write_no_extra;
		dev_info(&client->dev, "I2C function: without pre and end cmd!");
	}
}

static void unregister_i2c_func(struct i2c_client *client)
{
	tool_i2c_read = NULL;
	tool_i2c_write = NULL;
	dev_info(&client->dev, "I2C function: unregister i2c transfer function!");
}

/*---------------------- relation & confirm ---------------------*/
static u8 relation(struct i2c_client *client, u8 src, u8 dst, u8 rlt)
{
	u8 ret = 0;

	switch (rlt) {
	case 0:
		ret = (src != dst) ? true : false;
		break;
	case 1:
		ret = (src == dst) ? true : false;
		dev_dbg(&client->dev,
			"equal:src:0x%02x  dst:0x%02x  ret:%d.",
			src, dst, (s32)ret);
		break;
	case 2:
		ret = (src > dst) ? true : false;
		break;
	case 3:
		ret = (src < dst) ? true : false;
		break;
	case 4:
		ret = (src & dst) ? true : false;
		break;
	case 5:
		ret = (!(src | dst)) ? true : false;
		break;
	default:
		ret = false;
		break;
	}
	return ret;
}

static u8 confirm(struct goodix_ts_data *ts)
{
	s32 i = 0;
	u8 buf[32];
	struct i2c_client *client = ts->client;

	memcpy(buf, cmd_head.flag_addr, cmd_head.addr_len);

	for (i = 0; i < cmd_head.times; i++) {
		if (tool_i2c_read(ts, buf, 1) <= 0) {
			dev_err(&client->dev, "Read flag data failed!");
			return FAIL;
		}
		if (relation(client, buf[GTP_ADDR_LENGTH],
		     cmd_head.flag_val, cmd_head.flag_relation)) {
			dev_dbg(&client->dev, "value at flag addr:0x%02x.",
				buf[GTP_ADDR_LENGTH]);
			dev_dbg(&client->dev, "flag value:0x%02x.",
				cmd_head.flag_val);
			break;
		}
		msleep(cmd_head.circle);
	}

	if (i >= cmd_head.times) {
		dev_err(&client->dev, "Can't get the continue flag!");
		return FAIL;
	}
	return SUCCESS;
}

/*---------------------- init / uninit ---------------------*/
s32 init_wr_node(struct i2c_client *client)
{
	s32 i;
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	char procname[32];

	memset(&cmd_head, 0, sizeof(cmd_head));
	cmd_head.data = NULL;

	i = 6;
	while ((!cmd_head.data) && i) {
		cmd_head.data = kzalloc(i * DATA_LENGTH_UINT, GFP_KERNEL);
		if (cmd_head.data)
			break;
		i--;
	}
	if (i) {
		DATA_LENGTH = i * DATA_LENGTH_UINT - GTP_ADDR_LENGTH;
		dev_info(&client->dev,
			 "Alloc memory size:%d.", DATA_LENGTH);
	} else {
		dev_err(&client->dev, "Apply for memory failed.");
		return FAIL;
	}

	cmd_head.addr_len = 2;
	cmd_head.retry = 5;

	register_i2c_func(client);

	snprintf(procname, sizeof(procname), "gmnode_%d", ts->pdata->display_id);
	ts->gmnode_proc_entry = proc_create_data(procname, 0644, NULL,
						 &gtp_proc_ops, ts);
	if (!ts->gmnode_proc_entry) {
		dev_err(&client->dev, "Couldn't create proc entry!");
		return FAIL;
	}

	dev_info(&client->dev, "Create proc entry success!");
	return SUCCESS;
}

void uninit_wr_node(struct i2c_client *client)
{
	struct goodix_ts_data *ts = i2c_get_clientdata(client);
	char procname[32];

	kfree(cmd_head.data);
	cmd_head.data = NULL;
	unregister_i2c_func(client);

	if (ts->gmnode_proc_entry) {
		snprintf(procname, sizeof(procname), "gmnode_%d",
			 ts->pdata->display_id);
		remove_proc_entry(procname, NULL);
		ts->gmnode_proc_entry = NULL;
	}
}

/*---------------------- proc read/write ---------------------*/
ssize_t goodix_tool_write(struct file *filp, const char __user *buff,
			  size_t len, loff_t *off)
{
	s32 ret = 0;
	struct goodix_ts_data *ts = pde_data(file_inode(filp));
	struct i2c_client *client = ts->client;

	ret = copy_from_user(&cmd_head, buff, CMD_HEAD_LENGTH);
	if (ret) {
		dev_err(&client->dev, "copy_from_user failed.");
		return -EPERM;
	}

	dev_dbg(&client->dev, "[Operation]wr: %02X", cmd_head.wr);
	dev_dbg(&client->dev,
		"[Flag]flag: %02X,addr: %02X%02X,value: %02X,relation: %02X",
		cmd_head.flag, cmd_head.flag_addr[0],
		cmd_head.flag_addr[1], cmd_head.flag_val,
		cmd_head.flag_relation);
	dev_dbg(&client->dev,
		"[Retry]circle: %d,times: %d,retry: %d, delay: %d",
		(s32)cmd_head.circle,
		(s32)cmd_head.times, (s32)cmd_head.retry,
		(s32)cmd_head.delay);

	if (cmd_head.wr == 1) {
		if (cmd_head.data_len > DATA_LENGTH) {
			dev_err(&client->dev,
				"Tool write failed data too long");
			return -EPERM;
		}
		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
				     &buff[CMD_HEAD_LENGTH],
				     cmd_head.data_len);
		if (ret) {
			dev_err(&client->dev, "copy_from_user failed.");
			return -EPERM;
		}
		memcpy(&cmd_head.data[GTP_ADDR_LENGTH - cmd_head.addr_len],
		       cmd_head.addr, cmd_head.addr_len);

		GTP_DEBUG_ARRAY(cmd_head.data, cmd_head.data_len +
				cmd_head.addr_len);

		if (cmd_head.flag == 1) {
			if (confirm(ts) == FAIL) {
				dev_err(&client->dev,
					"[WRITE]Confirm fail!");
				return -EPERM;
			}
		} else if (cmd_head.flag == 2) {
			/* Need interrupt! */
		}
		if (tool_i2c_write(ts,
				   &cmd_head.data[GTP_ADDR_LENGTH -
				   cmd_head.addr_len],
				   cmd_head.data_len + cmd_head.addr_len) <= 0) {
			dev_err(&client->dev, "[WRITE]Write data failed!");
			return -EPERM;
		}

		GTP_DEBUG_ARRAY(&cmd_head.data[GTP_ADDR_LENGTH -
				cmd_head.addr_len],
				cmd_head.data_len + cmd_head.addr_len);
		if (cmd_head.delay)
			msleep(cmd_head.delay);
	} else if (cmd_head.wr == 3) {
		if (cmd_head.data_len > DATA_LENGTH) {
			dev_err(&client->dev,
				"Tool write failed data too long");
			return -EPERM;
		}
		ret = copy_from_user(&cmd_head.data[0], &buff[CMD_HEAD_LENGTH],
				     cmd_head.data_len);
		if (ret) {
			dev_err(&client->dev, "copy_from_user failed.");
			return -EPERM;
		}
		memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len);
		register_i2c_func(client);
	} else if (cmd_head.wr == 5) {
		/* memcpy(IC_TYPE, cmd_head.data, cmd_head.data_len); */
	} else if (cmd_head.wr == 7) {
		gtp_work_control_enable(ts, false);
		if (ts->pdata->esd_protect)
			gtp_esd_off(ts);
	} else if (cmd_head.wr == 9) {
		gtp_work_control_enable(ts, true);
		if (ts->pdata->esd_protect)
			gtp_esd_on(ts);
	} else if (cmd_head.wr == 17) {
		if (cmd_head.data_len > DATA_LENGTH) {
			dev_err(&client->dev,
				"Tool write failed data too long");
			return -EPERM;
		}
		ret = copy_from_user(&cmd_head.data[GTP_ADDR_LENGTH],
				     &buff[CMD_HEAD_LENGTH],
				     cmd_head.data_len);
		if (ret) {
			dev_dbg(&client->dev, "copy_from_user failed.");
			return -EPERM;
		}
		if (cmd_head.data[GTP_ADDR_LENGTH]) {
			dev_info(&client->dev, "gtp enter rawdiff.");
			set_bit(RAW_DATA_MODE, &ts->flags);
		} else {
			clear_bit(RAW_DATA_MODE, &ts->flags);
			dev_info(&client->dev, "gtp leave rawdiff.");
		}
	} else if (cmd_head.wr == 19) {
		gtp_reset_guitar(client);
	} else if (cmd_head.wr == 11) {
		if (gup_enter_update_mode(client) == FAIL)
			return -EPERM;
	} else if (cmd_head.wr == 13) {
		gup_leave_update_mode(client);
	} else if (cmd_head.wr == 15) {
		show_len = 0;
		total_len = 0;
		if (cmd_head.data_len > DATA_LENGTH) {
			dev_err(&client->dev,
				"Tool write failed data too long");
			return -EPERM;
		}
		memset(cmd_head.data, 0, DATA_LENGTH);
		ret = copy_from_user(cmd_head.data,
				     &buff[CMD_HEAD_LENGTH],
				     cmd_head.data_len);
		if (ret) {
			dev_dbg(&client->dev, "copy_from_user failed.");
			return -EPERM;
		}
		if (gup_update_proc((void *)cmd_head.data) == FAIL)
			return -EPERM;
	}

	return len;
}

ssize_t goodix_tool_read(struct file *file, char __user *page,
			 size_t size, loff_t *ppos)
{
	struct goodix_ts_data *ts = pde_data(file_inode(file));
	struct i2c_client *client = ts->client;

	if (cmd_head.wr % 2) {
		return -EPERM;
	} else if (!cmd_head.wr) {
		u16 len, data_len, loc, addr;

		if (cmd_head.flag == 1) {
			if (confirm(ts) == FAIL) {
				dev_err(&client->dev, "[READ]Confirm fail!");
				return -EPERM;
			}
		} else if (cmd_head.flag == 2) {
			/* Need interrupt! */
		}

		if (cmd_head.delay)
			msleep(cmd_head.delay);

		data_len = cmd_head.data_len;
		addr = (cmd_head.addr[0] << 8) + cmd_head.addr[1];
		loc = 0;

		dev_info(&client->dev, "Read data[0x%04x] length:%d.",
			 addr, data_len);

		while (data_len > 0) {
			len = data_len > DATA_LENGTH ? DATA_LENGTH : data_len;
			cmd_head.data[0] = (addr >> 8) & 0xFF;
			cmd_head.data[1] = (addr & 0xFF);
			if (tool_i2c_read(ts, cmd_head.data, len) <= 0) {
				dev_err(&client->dev, "[READ]Read data failed!");
				return -EPERM;
			}
			if (copy_to_user(&page[loc],
					 &cmd_head.data[GTP_ADDR_LENGTH], len)) {
				dev_err(&client->dev, "copy_to_user failed.");
				return -EFAULT;
			}
			loc += len;
			addr += len;
			data_len -= len;
		}
		return cmd_head.data_len;
	} else if (cmd_head.wr == 2) {
		if (copy_to_user(page, IC_TYPE, sizeof(IC_TYPE))) {
			dev_err(&client->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		return sizeof(IC_TYPE);
	} else if (cmd_head.wr == 4) {
		u8 progress_buf[4];

		progress_buf[0] = show_len >> 8;
		progress_buf[1] = show_len & 0xff;
		progress_buf[2] = total_len >> 8;
		progress_buf[3] = total_len & 0xff;

		if (copy_to_user(page, progress_buf, 4)) {
			dev_err(&client->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		return 4;
	} else if (cmd_head.wr == 6) {
		/* Read error code */
	} else if (cmd_head.wr == 8) {
		if (copy_to_user(page, GTP_DRIVER_VERSION,
				 strlen(GTP_DRIVER_VERSION))) {
			dev_err(&client->dev, "copy_to_user failed.");
			return -EFAULT;
		}
		return strlen(GTP_DRIVER_VERSION);
	}

	return -EPERM;
}

MODULE_AUTHOR("Amlogic, Inc");
MODULE_DESCRIPTION("GT9 serials touch controller Driver");
MODULE_LICENSE("GPL v2");
