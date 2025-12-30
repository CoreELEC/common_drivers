// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "../../lcd_common.h"
#include "./dsi_common.h"
#include "./dsi_ctrl/dsi_ctrl.h"
#include "../lcd_connector.h"
#include <linux/amlogic/media/vout/lcd/lcd_model.h>

char *dsi_op_mode_table[] = {"Video", "Command"};

char *dsi_video_mode_type_table[] = {
	"Non-Burst (sync pulse)",
	"Non-Burst (sync event)",
	"Burst",
};

char *dsi_video_data_type_table[] = {
	"16b CFG-1",
	"16b CFG-2",
	"16b CFG-3",
	"18b CFG-1",
	"18b CFG-2",
	"24b",
	"20b YUV422 (loosely)",
	"24b YUV422",
	"16b YUV422",
	"30b",
	"36b",
	"12b YUV420",
	"un-support type",
};

void dsi_config_print_helper(struct lcd_config_s *pconf, u8 pr_flag)
{
	struct dsi_config_s *dconf = &pconf->control.mipi_cfg;
	u32 esc_clk;

	if (pr_flag & DSI_HOST_CFG_PR_BASIC) {
		pr_info("DSI Config:\n"
			"  %u-port, %u-lane\n"
			"  bit rate = %lluHz (limit < %uMHz)\n"
			"  operation mode:  init:%s(%u), display:%s(%u)\n"
			"  video mode type: %s(%d)\n"
			"  clk always hs:   %u\n"
			"  data format:     %s\n",
			dconf->multi_port_cfg ? 2 : 1, dconf->lane_num,
			pconf->timing.bit_rate, dconf->bit_rate_target,
			dsi_op_mode_table[dconf->operation_mode_init],
			dconf->operation_mode_init,
			dsi_op_mode_table[dconf->operation_mode_display],
			dconf->operation_mode_display,
			dsi_video_mode_type_table[dconf->video_mode_type],
			dconf->video_mode_type,
			dconf->clk_always_hs,
			dsi_video_data_type_table[dconf->dpi_data_format]);
	}

	if (pr_flag & DSI_HOST_CFG_PR_VID_TIMING) {
		pr_info("DSI video timing:\n"
			"  HLINE=%u HSA=%u HBP=%u\n"
			"  VSA=%u VBP=%u VFP=%u VACT=%u\n",
			dconf->hline, dconf->hsa, dconf->hbp,
			pconf->timing.act_timing.vsync_width,
			pconf->timing.act_timing.vsync_bp,
			pconf->timing.act_timing.vsync_fp,
			pconf->timing.act_timing.v_active);
	}

	if (pr_flag & DSI_HOST_CFG_PR_CLK) {
		pr_info("DSI clk:\n"
		      "  Pixel CLK   = %10uHz\n"
		      "  bit_rate    = %10lluHz\n"
		      "  LaneByteClk = %10uHz\n"
		      "  PCLK_period/lanebyteclk_period = (num=%u/den=%u)\n",
		      pconf->timing.act_timing.pixel_clk,
		      pconf->timing.bit_rate,
		      dconf->lane_byte_clk, dconf->factor_numerator, dconf->factor_denominator);
	}

	if (pr_flag & DSI_HOST_CFG_PR_DPHY_TIM) {
		esc_clk = div_around(1000000, dconf->dphy.lp_tesc[1]);

		pr_info("DSI DPHY setting:\n"
			"  t-UI       = %u.%02uns\n"
			"  LP-ESC clk = %d.%03dMHz\n"
			"  LP : tesc=0x%02x:%3uns lpx=0x%02x:%3uns ta_sure=0x%02x:%3uns ta_go=0x%02x:%3uns ta_get=0x%02x:%3uns\n"
			"  HS : exit=0x%02x:%3uns trail=0x%02x:%3uns zero=0x%02x:%3uns prepare=0x%02x:%3uns\n"
			"  CLK: pre =0x%02x:%3uns trail=0x%02x:%3uns zero=0x%02x:%3uns prepare=0x%02x:%3uns post=0x%02x:%3uns\n"
			"  init = 0x%02x wakeup = 0x%02x\n"
			"  phy: hs2lp=0x%02x:%5uns lp2hs=0x%02x:%5uns\n"
			"  max_rd_time = 0x%02x:%5uns\n",
			dconf->dphy.t_ui / 100,     dconf->dphy.t_ui % 100,
			(esc_clk / 1000),           (esc_clk % 1000),
			dconf->dphy.lp_tesc[0],     dconf->dphy.lp_tesc[1],
			dconf->dphy.lp_lpx[0],      dconf->dphy.lp_lpx[1],
			dconf->dphy.lp_ta_sure[0],  dconf->dphy.lp_ta_sure[1],
			dconf->dphy.lp_ta_go[0],    dconf->dphy.lp_ta_go[1],
			dconf->dphy.lp_ta_get[0],   dconf->dphy.lp_ta_get[1],
			dconf->dphy.hs_exit[0],     dconf->dphy.hs_exit[1],
			dconf->dphy.hs_trail[0],    dconf->dphy.hs_trail[1],
			dconf->dphy.hs_zero[0],     dconf->dphy.hs_zero[1],
			dconf->dphy.hs_prepare[0],  dconf->dphy.hs_prepare[1],
			dconf->dphy.clk_pre[0],     dconf->dphy.clk_pre[1],
			dconf->dphy.clk_trail[0],   dconf->dphy.clk_trail[1],
			dconf->dphy.clk_zero[0],    dconf->dphy.clk_zero[1],
			dconf->dphy.clk_prepare[0], dconf->dphy.clk_prepare[1],
			dconf->dphy.clk_post[0],    dconf->dphy.clk_post[1],
			dconf->dphy.init, dconf->dphy.wakeup,
			dconf->dphy.phy_hs2lp_time[0], dconf->dphy.phy_hs2lp_time[1],
			dconf->dphy.phy_lp2hs_time[0], dconf->dphy.phy_lp2hs_time[1],
			dconf->dphy.max_rd_time[0], dconf->dphy.max_rd_time[1]);
	}

	if (pr_flag & DSI_HOST_CFG_PR_N_BURST_ST) {
		if (dconf->video_mode_type != DSI_VID_BURST) {
			pr_info("DSI NON-BURST setting:\n"
				"  vid_num_chunks: %d\n"
				"  vid_pkt_size:   %d\n"
				"  vid_null_size:  %d\n\n",
				dconf->vid_num_chunks,
				dconf->vid_pkt_size,
				dconf->vid_null_size);
		}
	}
}

void lcd_mipi_dsi_init_table_load_dts(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
					struct device_node *child)
{
	int table_cnt;
	struct dsi_config_s *dconf = &dev_p->dev_cfg.control.mipi_cfg;
	u8 i;
	u16 j;
	unsigned int *_temp_table_addr;
	struct dsi_table_s {
		unsigned char **table_addr_addr; char *name; uint16_t size_limit;
	} dsi_table[4] = {
		{&dconf->dsi_init_on,  "dsi_init_on", DSI_INIT_ON_MAX},
		{&dconf->dsi_init_off, "dsi_init_off", DSI_INIT_OFF_MAX},
		{&dconf->dsi_suspend,  "dsi_suspend", DSI_SUSPEND_MAX},
		{&dconf->dsi_resume,   "dsi_resume", DSI_RESUME_MAX},
	};

	for (i = 0; i < 4; i++) {
		kfree(*dsi_table[i].table_addr_addr);
		*dsi_table[i].table_addr_addr = NULL;

		table_cnt = of_property_count_elems_of_size(child, dsi_table[i].name, sizeof(u32));

		// table_cnt = table_cnt / sizeof(unsigned int);
		if (table_cnt <= 0 || table_cnt >= dsi_table[i].size_limit) {
			LCD_DEV_PR(pdrv, dev_p->idx, "table %s: get [%d] (limit:%u) invalid",
				dsi_table[i].name, table_cnt, dsi_table[i].size_limit);
			continue;
		}

		_temp_table_addr = kcalloc(table_cnt + 2, sizeof(u32), GFP_KERNEL);
		*dsi_table[i].table_addr_addr = kcalloc(table_cnt + 2, sizeof(u8), GFP_KERNEL);
		if (!(*dsi_table[i].table_addr_addr) || !_temp_table_addr) {
			kfree(*dsi_table[i].table_addr_addr);
			kfree(_temp_table_addr);
			*dsi_table[i].table_addr_addr = NULL;
			_temp_table_addr = NULL;
			continue;
		}
		LCD_DEV_PR(pdrv, dev_p->idx, "table %s get [%d]", dsi_table[i].name, table_cnt);
		of_property_read_u32_array(child, dsi_table[i].name, _temp_table_addr, table_cnt);

		for (j = 0; j < table_cnt; j++)
			(*dsi_table[i].table_addr_addr)[j] = _temp_table_addr[j];

		(*dsi_table[i].table_addr_addr)[table_cnt]     = 0xff;
		(*dsi_table[i].table_addr_addr)[table_cnt + 1] = 0xff;
		kfree(_temp_table_addr);
	}

}

void lcd_mipi_dsi_init_table_load_json(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
					struct json_parse_s *jsp, struct json_s *parent)
{
	u8 i;
	const char *str = NULL;
	int cnt = 0, cnt_max;
	unsigned int *nums = NULL, nums_size = 0;
	struct dsi_config_s *dconf = &dev_p->dev_cfg.control.mipi_cfg;

	struct dsi_table_s {
		unsigned char **table_addr_addr; char *name; uint16_t size_limit;
	} dsi_table[4] = {
		{&dconf->dsi_init_on,  "dsi_init_on", DSI_INIT_ON_MAX},
		{&dconf->dsi_init_off, "dsi_init_off", DSI_INIT_OFF_MAX},
		{&dconf->dsi_suspend,  "dsi_suspend", DSI_SUSPEND_MAX},
		{&dconf->dsi_resume,   "dsi_resume", DSI_RESUME_MAX},
	};

	for (i = 0; i < 4; i++) {
		kfree(*dsi_table[i].table_addr_addr);
		*dsi_table[i].table_addr_addr = NULL;

		str = json_get_obj_str(jsp, parent, dsi_table[i].name, NULL);
		if (!str) {
			LCD_DEV_ERR(pdrv, dev_p->idx, "not find mipi %s", dsi_table[i].name);
			return;
		}
		cnt_max = lcd_get_str_array_cnt(str);
		if (cnt_max <= 0) {
			LCD_DEV_ERR(pdrv, dev_p->idx, "mipi %s error", dsi_table[i].name);
			return;
		}
		nums_size = cnt_max * sizeof(unsigned int);
		nums = kzalloc(nums_size, GFP_KERNEL);
		if (!nums)
			return;

		cnt = lcd_trans_str_array(str, nums, cnt_max);
		if (cnt <= 0) {
			LCD_DEV_ERR(pdrv, dev_p->idx, "mipi %s error", dsi_table[i].name);
			kfree(nums);
			return;
		}
		*dsi_table[i].table_addr_addr = kcalloc(cnt, sizeof(unsigned char), GFP_KERNEL);
		if (!(*dsi_table[i].table_addr_addr)) {
			kfree(nums);
			return;
		}
		LCD_DEV_PR(pdrv, dev_p->idx, "table %s get [%d]", dsi_table[i].name, cnt);
		for (i = 0; i < cnt; i++)
			(*dsi_table[i].table_addr_addr)[i] = nums[i];

		(*dsi_table[i].table_addr_addr)[cnt]     = 0xff;
		(*dsi_table[i].table_addr_addr)[cnt + 1] = 0xff;

		kfree(nums);
		nums = NULL;
	}
}

void lcd_mipi_dsi_init_table_free(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct dsi_config_s *dsi_cfg;

	if (!dev_p)
		return;

	dsi_cfg = &dev_p->dev_cfg.control.mipi_cfg;

	kfree(dsi_cfg->dsi_init_on);
	kfree(dsi_cfg->dsi_init_off);
	kfree(dsi_cfg->dsi_suspend);
	kfree(dsi_cfg->dsi_resume);
	dsi_cfg->dsi_init_on = NULL;
	dsi_cfg->dsi_init_off = NULL;
	dsi_cfg->dsi_suspend = NULL;
	dsi_cfg->dsi_resume = NULL;
}

static void dsi_req_print(int ret, struct dsi_cmd_req_s *req)
{
	char *string;
	u32 n = 0, k;

	string = kcalloc(1024, sizeof(char), GFP_KERNEL);
	if (!string)
		return;

	u8 is_DCS =
		req->data_type == DT_DCS_LONG_WR ||
		req->data_type == DT_DCS_SHORT_WR_0 ||
		req->data_type == DT_DCS_SHORT_WR_1 ||
		req->data_type == DT_DCS_RD_0;
	u8 is_read =
		req->data_type == DT_GEN_RD_0 ||
		req->data_type == DT_GEN_RD_1 ||
		req->data_type == DT_GEN_RD_2 ||
		req->data_type == DT_DCS_RD_0;
	u8 is_long =
		req->data_type == DT_DCS_LONG_WR ||
		req->data_type == DT_GEN_LONG_WR;
	//DCS_LONG_WR:0x29, GEN_LONG_WR:0x39
	u8 num = (req->data_type & 0xf) == 0x9 ? 3 : (req->data_type >> 4) & 0xf;

	n += snprintf((string + n), 255 - n, "DSI %s %s %s%u%s: ",
		is_DCS  ? "DCS"          : "generic",
		is_read ? "RD"           : "WR",
		is_long ? "long("        : "",
		is_long ? req->pld_count : num,
		is_long ? ")"            : "");

	for (k = 0; k < req->pld_count; k++)
		n += snprintf((string + n), 255 - n, "0x%02x ", req->payload[k]);

	if (ret) {
		snprintf((string + n), 255 - n, " failed");
	} else {
		if (req->rd_out_len)
			n += snprintf((string + n), 255 - n, ": ");

		for (k = 0; k < req->rd_out_len; k++)
			n += snprintf((string + n), 255 - n, "0x%02x ", req->rd_data[k]);
	}
	pr_info("%s\n", string);
	kfree(string);
}

/* helper func to check req->pld_count satisfied req->data_type and return req->payload[pld_idx] */
static u32 cmd_pld_cnt_check(struct aml_lcd_drv_s *pdrv, struct dsi_cmd_req_s *req)
{
	u8 pld_req = 0;

	switch (req->data_type) {
	case DT_GEN_SHORT_WR_2:
	case DT_GEN_RD_2:
	case DT_SET_MAX_RET_PKT_SIZE:
	case DT_DCS_SHORT_WR_1:
		pld_req = 2;
		break;
	case DT_GEN_SHORT_WR_1:
	case DT_GEN_RD_1:
	case DT_DCS_SHORT_WR_0:
	case DT_DCS_RD_0:
		pld_req = 1;
		break;
	case DT_GEN_RD_0:
	case DT_GEN_SHORT_WR_0:
		pld_req = 0;
		break;
	case DT_DCS_LONG_WR:
	case DT_GEN_LONG_WR:
	default:
		pld_req = req->pld_count;
		break;
	}

	if (req->pld_count < pld_req) {
		LCD_ERR(pdrv, "payload count %d insufficient for 0x%02x (req:%d)",
			req->pld_count, req->data_type, pld_req);
		return -1;
	}
	return 0;
}

/* Function: dsi_run_oneline_cmd
 * Supported Data Type:
 *	DT_GEN_SHORT_WR_0, DT_GEN_SHORT_WR_1, DT_GEN_SHORT_WR_2,
 *	DT_DCS_SHORT_WR_0, DT_DCS_SHORT_WR_1,
 *	DT_GEN_LONG_WR, DT_DCS_LONG_WR,
 *	DT_SET_MAX_RET_PKT_SIZE
 *	DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
 *	DT_DCS_RD_0
 * @payload: one line of dsi_init_on/off
 * @rd_back_len: max this rd_back can contain
 * Return: -1:error; 0: Wr done; 0/>0: number of rdback
 */
int dsi_run_oneline_cmd(struct aml_lcd_drv_s *pdrv, u8 port,
			u8 *payload, u8 *rd_back, u32 rd_back_len)
{
	struct dsi_cmd_req_s dsi_cmd_req;
	u8 is_rd = 0;
	u16 dsi_rd_len = 0;
	int ret = 0;

	/* @payload:
	 * [0]: data_type
	 * [1]: pld_count
	 * [2++]: pld context
	 */
	dsi_cmd_req.data_type = payload[0];
	dsi_cmd_req.pld_count = payload[1];
	dsi_cmd_req.payload = &payload[2];
	dsi_cmd_req.vc_id = MIPI_DSI_VIRTUAL_CHAN_ID;
	dsi_cmd_req.rd_out_len = 0;

	ret = cmd_pld_cnt_check(pdrv, &dsi_cmd_req);
	if (ret)
		return -1;

	switch (dsi_cmd_req.data_type) {
	case DT_GEN_SHORT_WR_0:
	case DT_GEN_SHORT_WR_1:
	case DT_GEN_SHORT_WR_2:
		dsi_cmd_req.req_ack = ACK_CTRL_GEN_SHORT_WR;
		ret = dsi_DT_generic_short_write(pdrv, port, &dsi_cmd_req);
		break;
	case DT_DCS_SHORT_WR_0:
	case DT_DCS_SHORT_WR_1:
		dsi_cmd_req.req_ack = ACK_CTRL_DCS_SHORT_WR;
		ret = dsi_DT_DCS_short_write(pdrv, port, &dsi_cmd_req);
		break;
	case DT_DCS_LONG_WR:
		dsi_cmd_req.req_ack = ACK_CTRL_GEN_LONG_WR;
		ret = dsi_DT_DCS_long_write(pdrv, port, &dsi_cmd_req);
		break;
	case DT_GEN_LONG_WR:
		dsi_cmd_req.req_ack = ACK_CTRL_DCS_LONG_WR;
		ret = dsi_DT_generic_long_write(pdrv, port, &dsi_cmd_req);
		break;
	case DT_TURN_ON:
		dsi_DT_sink_turn_on(pdrv, port);
		break;
	case DT_SHUT_DOWN:
		dsi_DT_sink_shut_down(pdrv, port);
		break;
	case DT_SET_MAX_RET_PKT_SIZE:
		dsi_cmd_req.req_ack = ACK_CTRL_SET_MAX_RET_PKT_SIZE;
		dsi_rd_len  = dsi_cmd_req.payload[0];
		dsi_rd_len |= (u16)dsi_cmd_req.payload[1] << 8;
		pdrv->curr_dev->dev_cfg.control.mipi_cfg.dsi_rd_n = dsi_rd_len;
		// DSI_RD_MAX default size is 4, if any read will return size over 4,
		// enlarge DSI_RD_MAX, each req will kcalloc such mem.
		if (dsi_rd_len > DSI_RD_MAX)
			LCD_PR(pdrv, "set max rx %d bytes, need to enlarge DSI_RD_MAX (%d)",
				dsi_rd_len, DSI_RD_MAX);
		ret = dsi_DT_set_max_return_pkt_size(pdrv, port, &dsi_cmd_req);
		return ret;
	case DT_GEN_RD_0:
	case DT_GEN_RD_1:
	case DT_GEN_RD_2:
		is_rd = 1;
		dsi_cmd_req.req_ack = ACK_CTRL_GEN_RD; //need BTA ack
		ret = dsi_DT_generic_read(pdrv, port, &dsi_cmd_req);
		break;
	case DT_DCS_RD_0:
		is_rd = 1;
		dsi_cmd_req.req_ack = ACK_CTRL_DCS_RD; //need BTA ack
		ret = dsi_DT_DCS_read(pdrv, port, &dsi_cmd_req);
		break;
	default:
		LCD_ERR(pdrv, "unsupport DSI cmd: 0x%02x", dsi_cmd_req.data_type);
		return -1;
	}

	if (ret || is_rd || lcd_debug_print_flag & LCD_DBG_PR_ADV)
		dsi_req_print(ret, &dsi_cmd_req);

	if (!ret && is_rd && rd_back_len && rd_back) {
		if (dsi_cmd_req.rd_out_len > rd_back_len) {
			LCD_ERR(pdrv, "dsi cmd: rd out %d, back limit %d",
				dsi_cmd_req.rd_out_len, rd_back_len);
			return 0;
		}
		memcpy(rd_back, dsi_cmd_req.rd_data, sizeof(char) * dsi_cmd_req.rd_out_len);
		return dsi_cmd_req.rd_out_len;
	}
	return ret;
}

int dsi_exec_init_table(struct aml_lcd_drv_s *pdrv,
		u8 *payload, u32 pld_limit, u8 *rd_back, u32 rd_back_max)
{
	u16 i = 0, j = 0, step = 0;
	u8 cmd_size, k;
	u32 delay_ms;
	int rd_cnt = 0, ret, curr_port = 0x01;

	/* mipi command(payload)
	 * format:  data_type, cmd_size, data....
	 *	data_type=0xff,
	 *		cmd_size<0xff means delay ms,
	 *		cmd_size=0xff or 0 means ending.
	 *	data_type=0xf0, for gpio control
	 *		data0=gpio_index, data1=gpio_value.
	 *		data0=gpio_index, data1=gpio_value, data2=delay.
	 *	data_type=0xfd, for delay ms
	 *		data0=delay, data_1=delay, ..., data_n=delay.
	 */
	while ((i + 1) < pld_limit) {
		cmd_size = payload[i + 1];
		if (payload[i] == LCD_EXT_CMD_TYPE_END) {
			if (cmd_size == 0xff || cmd_size == 0)
				return rd_cnt;
			cmd_size = 0;
			mdelay(payload[i + 1]);
		}

		if (cmd_size == 0) {
			i += (cmd_size + 2);
			continue;
		}
		if (i + 2 + cmd_size > pld_limit) {
			LCD_ERR(pdrv, "step %d: cmd_size out of support", step);
			break;
		}

		switch (payload[i]) {
		case LCD_EXT_CMD_TYPE_DELAY:
			delay_ms = 0;
			for (j = 0; j < cmd_size; j++)
				delay_ms += payload[i + 2 + j];
			mdelay(delay_ms);
			break;
		case LCD_EXT_CMD_TYPE_GPIO:
			if (cmd_size < 2) {
				LCD_ERR(pdrv, "step %d: invalid size %d for gpio", step, cmd_size);
				break;
			}
			lcd_cpu_gpio_set(pdrv, payload[i + 2], payload[i + 3]);
			if (cmd_size > 2)
				mdelay(payload[i + 4]);
			break;
		case LCD_EXT_CMD_TYPE_CHECK:
			if (cmd_size < 3) {
				LCD_ERR(pdrv, "step %d: invalid size %d for check", step, cmd_size);
				break;
			}
			mipi_dsi_check_state(pdrv, payload[i + 2], payload[i + 3]);
			break;
		case LCD_EXT_CMD_TYPE_SWITCH_PORT:
			if (cmd_size != 1) {
				LCD_ERR(pdrv, "step %d: invalid size %d for switch port",
					step, cmd_size);
				break;
			}
			LCD_DBG(pdrv, "step %d: following cmd will be on port-%c%c", step,
				payload[i + 2] & 0x1 ? 'A' : ' ',
				payload[i + 2] & 0x2 ? 'B' : ' ');
			curr_port = payload[i + 2];
			break;
		case DT_GEN_SHORT_WR_0:
		case DT_GEN_SHORT_WR_1:
		case DT_GEN_SHORT_WR_2:
		case DT_DCS_SHORT_WR_0:
		case DT_DCS_SHORT_WR_1:
		case DT_DCS_LONG_WR:
		case DT_GEN_LONG_WR:
		case DT_TURN_ON:
		case DT_SHUT_DOWN:
		case DT_SET_MAX_RET_PKT_SIZE:
			for (k = 0; k < 4; k++) {
				if (curr_port & (1 << k))
					dsi_run_oneline_cmd(pdrv, k, &payload[i], NULL, 0);
			}
			break;
		case DT_GEN_RD_0:
		case DT_GEN_RD_1:
		case DT_GEN_RD_2:
		case DT_DCS_RD_0:
			// ! read support on port A
			if (!rd_back) {
				dsi_run_oneline_cmd(pdrv, 0, &payload[i], NULL, 0);
			} else {
				ret = dsi_run_oneline_cmd(pdrv, 0, &payload[i],
					&rd_back[rd_cnt], rd_back_max - rd_cnt);
				if (ret > 0) //bypass wr or error
					rd_cnt += ret;
			}
			break;
		default:
			break;
		}
		i += (cmd_size + 2);
		step++;
	}
	return rd_cnt;
}

/* Function: dsi_read
 * payload struct: data_type, data_cnt, parameters...
 * Supported Data Type:
 * DT_DCS_RD_0, DT_GEN_RD_0, DT_GEN_RD_1, DT_GEN_RD_2,
 * Return: data count, -1 for error
 */
int dsi_read(struct aml_lcd_drv_s *pdrv, u8 *payload, u8 *rd_data, u8 len)
{
	int ret = -1;
	u8 rd_out;
	struct dsi_cmd_req_s dsi_cmd_req;

	dsi_cmd_req.data_type = payload[0];
	dsi_cmd_req.pld_count = payload[1];
	dsi_cmd_req.payload = &payload[2];
	dsi_cmd_req.rd_out_len = 0;
	dsi_cmd_req.vc_id = MIPI_DSI_VIRTUAL_CHAN_ID;

	switch (dsi_cmd_req.data_type) {
	case DT_GEN_RD_0:
	case DT_GEN_RD_1:
	case DT_GEN_RD_2:
		dsi_cmd_req.req_ack = ACK_CTRL_GEN_RD; /* need BTA ack */
		ret = dsi_DT_generic_read(pdrv, 0, &dsi_cmd_req);
		break;
	case DT_DCS_RD_0:
		dsi_cmd_req.req_ack = ACK_CTRL_DCS_RD; /* need BTA ack */
		ret = dsi_DT_DCS_read(pdrv, 0, &dsi_cmd_req);
		break;
	default:
		LCD_ERR(pdrv, "unsupported data_type: 0x%02x", dsi_cmd_req.data_type);
		break;
	}

	if (ret < 0)
		return -1;
	rd_out = dsi_cmd_req.rd_out_len > len ? len : dsi_cmd_req.rd_out_len;
	memcpy(rd_data, dsi_cmd_req.rd_data, rd_out);
	return rd_out;
}

static void dsi_panel_init(struct aml_lcd_drv_s *pdrv)
{
	struct dsi_config_s *dconf = &pdrv->curr_dev->dev_cfg.control.mipi_cfg;

	if (dconf->dsi_init_on) {
		dsi_exec_init_table(pdrv, dconf->dsi_init_on, DSI_INIT_ON_MAX, NULL, 0);
		LCD_PR(pdrv, "dsi init table");
	}
}

static void dsi_panel_deinit(struct aml_lcd_drv_s *pdrv)
{
	struct dsi_config_s *dconf = &pdrv->curr_dev->dev_cfg.control.mipi_cfg;

	if (dconf->dsi_init_off) {
		dsi_exec_init_table(pdrv, dconf->dsi_init_off, DSI_INIT_OFF_MAX, NULL, 0);
		LCD_PR(pdrv, "dsi deinit table");
	}
}

void lcd_dsi_tx_ctrl(struct aml_lcd_drv_s *pdrv, u8 en)
{
	if (en) {
		lcd_dsi_if_bind(pdrv);
		dsi_tx_ready(pdrv);
		dsi_panel_init(pdrv);
		dsi_disp_on(pdrv);
	} else {
		dsi_disp_off(pdrv);
		dsi_panel_deinit(pdrv);
		dsi_tx_close(pdrv);
	}
}

u64 lcd_dsi_get_min_bitrate(struct aml_lcd_drv_s *pdrv)
{
	struct dsi_config_s *dconf = &pdrv->curr_dev->dev_cfg.control.mipi_cfg;
	u64 bit_rate_min, band_width;
	u8 port_cnt = dconf->multi_port_cfg & BIT(0) ? 2 : 1;

	/* unit in kHz for calculation */
	band_width = pdrv->curr_dev->dev_cfg.timing.act_timing.pixel_clk;
	if (dconf->operation_mode_display == OPERATION_VIDEO_MODE) {
		if (dconf->video_mode_type != DSI_VID_BURST) {
			//band_width = band_width * 4 * dconf->data_bits;
			band_width =
				band_width * pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits;
		} else {
			band_width =
				band_width * pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits;
		}
	} else {
		LCD_DBG(pdrv, "dsi get bitrate: Only VIDEO mode");
		return 0;
	}

	bit_rate_min = lcd_do_div(band_width, port_cnt * dconf->lane_num);
	return bit_rate_min;
}

static irqreturn_t lcd_dsi_interrupt_handler(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct dsi_intr_meas_sta_s sta_data;
	//u32 intr_sta;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return IRQ_HANDLED;

	dsi_top_read_intr_sta(pdrv, 0, &sta_data);

	//intr_sta = sta_data.dsi_top_intr_sta;
	//intr_sta = (intr_sta >> 16) & intr_sta;
	//if (sta_data.dsi_top_intr_sta & 0x1) // dwc_edpite interrupt
	//if (sta_data.dsi_top_intr_sta & 0x6) // vs_fall/vs_rise interrupt
	//if (sta_data.dsi_top_intr_sta & 0x18) // de_fall/de_rise interrupt
	//if (sta_data.dsi_top_intr_sta & 0x20) // EOF interrupt
	//if (sta_data.dsi_top_intr_sta & 0x40) // deskewcalhsdone_pulse_rt/rt  interrupt
	//if (sta_data.dsi_top_intr_sta & 0x180) // deskewcalhs_pulse_ft/rt  interrupt

	LCD_DBG_ISR(pdrv, "dsi irq: irq=0x%x edptile[%d] at [%d.%d]",
		sta_data.dsi_top_intr_sta, sta_data.edpihalt_status,
		sta_data.edpite_line, sta_data.edpite_pix);

	return IRQ_HANDLED;
}

int lcd_dsi_interrupt_ctrl(struct aml_lcd_drv_s *pdrv, u8 en)
{
	unsigned int dsi_intr_irq = 0;

	if (!(pdrv->status & LCD_STATUS_IF_ON))
		return -1;

	if (!pdrv->drv_res.res_dsi_intr_irq) {
		LCD_ERR(pdrv, "res_dsi_intr_irq is null");
		return -1;
	}
	dsi_intr_irq = pdrv->drv_res.res_dsi_intr_irq;
	LCD_DBG(pdrv, "MIPI-DSI TOP irq: %d", dsi_intr_irq);

	snprintf(pdrv->drv_res.dsi_intr_isr_name, 10, "dsi%d_intr", pdrv->index);
	if (!en) {
		free_irq(dsi_intr_irq, (void *)pdrv);
		LCD_PR(pdrv, "free %s", pdrv->drv_res.dsi_intr_isr_name);
		return 0;
	}

	if (request_irq(dsi_intr_irq, lcd_dsi_interrupt_handler, 0,
			pdrv->drv_res.dsi_intr_isr_name, (void *)pdrv)) {
		LCD_ERR(pdrv, "can't request %s", pdrv->drv_res.dsi_intr_isr_name);
	} else {
		LCD_DBG(pdrv, "request %s successful", pdrv->drv_res.dsi_intr_isr_name);
	}

	dsi_top_set_irq_meas(pdrv, 0, 1, 1, 0x01);
	// [    1] vs_rise interrupt
	// [    0] dwc_edpite interrupt

	return 0;
}
