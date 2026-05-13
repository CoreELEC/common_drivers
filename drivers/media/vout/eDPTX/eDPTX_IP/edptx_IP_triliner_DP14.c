// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "./edptx_IP_REG_tr14.h"
#include "./edptx_IP_ctrls.h"
#include "../edptx_reg_op.h"
#include "../edptx_common.h"

static void edptx_aux_request(struct edptx_drv_s *edp, u8 port, struct dptx_aux_req_s *req)
{
	unsigned int state, timeout = 0;
	int i = 0;

	timeout = 0;
	while (timeout++ < DPTX_AUX_NO_REPLY_RETRY) {
		state = edp_reg_getb(edp, port, TR_DPTX_AUX_STATUS, 1, 1);
		if (state == 0)
			break;
		dptx_delay_ms(DPTX_AUX_NO_REPLY_TIMEOUT);
	};

	// 20-bit address for native AUX requests or 8-bit address for I2C over AUX requests.
	edp_reg_write(edp, port, TR_DPTX_AUX_ADDRESS, req->address & 0xfffff);
	/*submit data only for write commands*/
	if (req->cmd_code == eDPTX_AUX_CMD_I2C_WRITE ||
	    req->cmd_code == eDPTX_AUX_CMD_I2C_WRITE_MOT ||
	    req->cmd_code == eDPTX_AUX_CMD_I2C_WRITE_STATUS ||
	    req->cmd_code == eDPTX_AUX_CMD_WRITE) {
		for (i = 0; i < req->byte_cnt; i++)
			edp_reg_write(edp, port, TR_DPTX_AUX_WRITE_FIFO, req->data[i]);
	}
	/*submit the command and the data size*/
	edp_reg_write(edp, port, TR_DPTX_AUX_COMMAND,
		((0 << 12) | //ADDRESS_ONLY
		(req->cmd_code << 8) | // COMMAND
		((req->byte_cnt - 1) & 0xf))); //number of bytes
}

static u8 edptx_aux_submit_cmd(struct edptx_drv_s *edp, u8 port,
						struct dptx_aux_req_s *req)
{
	unsigned int status = 0, reply = 0, dc;
	unsigned int retry_cnt = 0, timeout = 0;//, irq_status;//, i2c_read_timeout_ignore = 0;
	char str[48];

	if (!edp)
		return 1;
	if (!edp_reg_read(edp, port, TR_DPTX_TRANSMITTER_ENABLE)) {
		eDP_P_ERR(edp, port, "eDPtx is not enabled");
		return 1;
	}

	switch (req->cmd_code) {
	case eDPTX_AUX_CMD_I2C_WRITE:
		sprintf(str, "Aux I2C Write: 0x%04x", req->address);
		break;
	case eDPTX_AUX_CMD_I2C_WRITE_MOT:
		sprintf(str, "Aux I2C Write MOT: 0x%04x", req->address);
		break;
	case eDPTX_AUX_CMD_I2C_WRITE_STATUS:
		sprintf(str, "Aux I2C Write Status: 0x%04x", req->address);
		break;
	case eDPTX_AUX_CMD_I2C_READ:
		sprintf(str, "Aux I2C Read: 0x%04x", req->address);
		// i2c_read_timeout_ignore = 1;
		break;
	case eDPTX_AUX_CMD_I2C_READ_MOT:
		sprintf(str, "Aux I2C Read MOT: 0x%04x", req->address);
		// i2c_read_timeout_ignore = 1;
		break;
	case eDPTX_AUX_CMD_READ:
		sprintf(str, "Aux Native Read: 0x%04x", req->address);
		break;
	case eDPTX_AUX_CMD_WRITE:
		sprintf(str, "Aux Native Write: 0x%04x", req->address);
		break;
	default:
		eDP_ERR(edp, "unknown Aux cmd");
		return 1;
	}

	// clean up reg
	edp_reg_read(edp, port, TR_DPTX_AUX_STATUS);
	edp_reg_read(edp, port, TR_DPTX_AUX_REPLY_CODE);

edptx_aux_submit_cmd_retry:
	edptx_aux_request(edp, port, req);

	timeout = 0;
	while (timeout++ < DPTX_AUX_REPLY_WAIT_TIMEOUT) {
		dptx_delay_us(DPTX_AUX_REPLY_WAIT_TIMER);

		// irq_status = edp_reg_read(edp->idx, EDP_TX_AUX_INTERRUPT_STATUS);
		//REPLY_RECEIVED: AUX ACK, may not finished
		// IP doc not asked to read
		status = edp_reg_read(edp, port, TR_DPTX_AUX_STATUS);
		reply = edp_reg_read(edp, port, TR_DPTX_AUX_REPLY_CODE);

		if (status & AUX_STATUS_REQUEST_IN_PROGRESS ||
		    status & AUX_STATUS_REPLY_IN_PROGRESS)
			continue;

		eDP_P_DBG(edp, port, "%s, status=0x%x, reply=0x%x", str, status, reply);

		if (status & AUX_STATUS_REPLY_ERROR || status & AUX_STATUS_REPLY_RECEIVED) {
			switch (req->cmd_code) {
			case eDPTX_AUX_CMD_I2C_WRITE:
			case eDPTX_AUX_CMD_I2C_WRITE_MOT:
			case eDPTX_AUX_CMD_I2C_WRITE_STATUS:
			case eDPTX_AUX_CMD_WRITE:
				if (status & AUX_STATUS_REPLY_RECEIVED)
					return 0;
				if (status & AUX_STATUS_REPLY_ERROR) {
					eDP_P_ERR(edp, port, "%s, status=0x%x, reply=0x%x",
						str, status, reply);
					return 0;
				}
				break;
			case eDPTX_AUX_CMD_READ:
				if (status & AUX_STATUS_REPLY_ERROR) {
					timeout = DPTX_AUX_REPLY_WAIT_TIMEOUT;
					break;
				}
				if (!(status & AUX_STATUS_REPLY_RECEIVED))
					break; // not in any state, loop to wait

				if (reply & AUX_REPLY_CODE_AUX_DEFER) {
					eDP_P_DBG(edp, port, "%s Defer", str);

					if (retry_cnt++ < DPTX_AUX_NO_REPLY_RETRY) {
						dptx_delay_ms(DPTX_AUX_NO_REPLY_TIMEOUT);
						goto edptx_aux_submit_cmd_retry;
					}
					break;
				}
				// DPCD addr not supported by DPRX, or invalid request
				if (reply == AUX_REPLY_CODE_AUX_NACK) {
					eDP_P_DBG(edp, port, "%s NACK", str);
					retry_cnt = DPTX_AUX_NO_REPLY_RETRY;
					timeout = DPTX_AUX_REPLY_WAIT_TIMEOUT;
					break;
				}
				return 0;
			case eDPTX_AUX_CMD_I2C_READ:
			case eDPTX_AUX_CMD_I2C_READ_MOT:
				if (status & AUX_STATUS_REPLY_ERROR) {
					timeout = DPTX_AUX_REPLY_WAIT_TIMEOUT;
					break;
				}
				if (!(status & AUX_STATUS_REPLY_RECEIVED))
					break; // not in any state, loop to wait

				if (reply &
					(AUX_REPLY_CODE_AUX_Defer | AUX_REPLY_CODE_I2C_Defer)) {
					dc = edp_reg_read(edp, port,
						TR_DPTX_AUX_REPLY_DATA_COUNT);
					eDP_P_DBG(edp, port, "%s %s Defer dc:%d", str,
						(reply & AUX_REPLY_CODE_I2C_Defer) ?
							"I2C" : "", dc);

					if (dc) // read with data error
						return 1;

					if (retry_cnt++ < DPTX_AUX_NO_REPLY_RETRY) {
						dptx_delay_ms(DPTX_AUX_NO_REPLY_TIMEOUT);
						goto edptx_aux_submit_cmd_retry;
					}
					break;
				}
				// DPCD / I2C addr not supported by DPRX, invalid request
				if (reply == AUX_REPLY_CODE_AUX_NACK ||
					reply == AUX_REPLY_CODE_I2C_NACK) {
					eDP_P_DBG(edp, port, "%s %s NACK", str,
						(reply & AUX_REPLY_CODE_I2C_NACK) ?
							"I2C" : "");
					retry_cnt = DPTX_AUX_NO_REPLY_RETRY;
					timeout = DPTX_AUX_REPLY_WAIT_TIMEOUT;
					break;
				}
				return 0;
			default:
				return 0;
			}
		}
	}

	if (retry_cnt++ < DPTX_AUX_NO_REPLY_RETRY) {
		dptx_delay_ms(DPTX_AUX_NO_REPLY_TIMEOUT);
		eDP_P_PR(edp, port, "%s timeout, retry %d", str, retry_cnt);
		goto edptx_aux_submit_cmd_retry;
	}

	eDP_P_ERR(edp, port, "%s failed", str);
	return 1;
}

static u8 _aux_write(struct edptx_drv_s *edp, u8 port,
				u32 addr, int len, u8 *buf)
{
	struct dptx_aux_req_s aux_req;

	if (!buf)
		return 1;

	aux_req.cmd_code = eDPTX_AUX_CMD_WRITE;
	// aux_req.cmd_state = 0;
	aux_req.address = addr;
	aux_req.byte_cnt = len;
	aux_req.data = buf;

	return edptx_aux_submit_cmd(edp, port, &aux_req);
}

static u8 _aux_write_single(struct edptx_drv_s *edp, u8 port, u32 addr, u8 val)
{
	struct dptx_aux_req_s aux_req;
	u8 auxdata = val;
	u8 ret;

	aux_req.cmd_code = eDPTX_AUX_CMD_WRITE;
	// aux_req.cmd_state = 0;
	aux_req.address = addr;
	aux_req.byte_cnt = 1;
	aux_req.data = &auxdata;

	ret = edptx_aux_submit_cmd(edp, port, &aux_req);
	return ret;
}

static u8 _aux_read(struct edptx_drv_s *edp, u8 port, u32 addr, int len, u8 *buf)
{
	struct dptx_aux_req_s aux_req;
	int i;//, reply_count;

	if (!buf)
		return 1;

	aux_req.cmd_code = eDPTX_AUX_CMD_READ;
	// aux_req.cmd_state = 0;
	aux_req.address = addr;
	aux_req.byte_cnt = len;
	aux_req.data = buf;

	if (edptx_aux_submit_cmd(edp, port, &aux_req))
		return 1;

	// reply_count = edp_reg_read(edp->idx, TR_DPTX_AUX_REPLY_DATA_COUNT);

	for (i = 0; i < len; i++)
		buf[i] = (unsigned char)(edp_reg_read(edp, port, TR_DPTX_AUX_REPLY_DATA));

	return 0;
}

static u8 _aux_i2c_op(struct edptx_drv_s *edp, u8 port, u8 cmd_type, u32 dev_addr, u8 len, u8 *data)
{
	struct dptx_aux_req_s aux_req;
	// unsigned char aux_data[4];
	u8 n = 0, reply_count = 0;
	u8 i, ret = 0;

	aux_req.cmd_code = cmd_type;
	// aux_req.cmd_state = 0;
	aux_req.address = dev_addr;
	aux_req.byte_cnt = len;

	switch (cmd_type) {
	case eDPTX_AUX_CMD_I2C_WRITE:
	case eDPTX_AUX_CMD_I2C_WRITE_MOT:
	case eDPTX_AUX_CMD_I2C_WRITE_STATUS:
		aux_req.data = data;
		ret = edptx_aux_submit_cmd(edp, port, &aux_req);
		dptx_delay_us(100);
		break;
	case eDPTX_AUX_CMD_I2C_READ:
	case eDPTX_AUX_CMD_I2C_READ_MOT:
		ret = edptx_aux_submit_cmd(edp, port, &aux_req);
		dptx_delay_us(100);
		reply_count = edp_reg_read(edp, port, TR_DPTX_AUX_REPLY_DATA_COUNT);
		if (reply_count != len) {
			eDP_P_ERR(edp, port, "Aux I2C cmd reply %d", reply_count);
			return -1;
		}
		for (i = 0; i < reply_count; i++) {
			data[n] = edp_reg_read(edp, port, TR_DPTX_AUX_REPLY_DATA);
			n++;
		}
		break;
	case eDPTX_AUX_CMD_READ:
	case eDPTX_AUX_CMD_WRITE:
		eDP_P_ERR(edp, port, "not Aux I2C cmd");
		break;
	default:
		eDP_P_ERR(edp, port, "unknown Aux cmd");
		break;
	}
	return ret;
}

static void dptx_transmit_pattern(struct edptx_drv_s *edp, u8 port, u8 pattern,
		u8 lane_mask, u32 cos_80b_0, u32 cos_80b_1, u32 cos_80b_2)
{
	u8 dptx_ip_pat_sets[] = {
	//TRAINING_PATTERN_SET: off, TPS1, TPS2, TPS3, TPS4
		0x00, 0x01, 0x02, 0x03, 0x04,
	//LINK_QUAL_PATTERN_SET: off, D10.2, SERm, PRBS7, 80b_cos, CP2520-1, CP2520-2, CP2520-3
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
	unsigned int reg_val;

	switch (pattern) {
	case eDPTX_TPS_DISABLE:
	case eDPTX_TPS1:
	case eDPTX_TPS2:
	case eDPTX_TPS3:
	case eDPTX_TPS4:
		reg_val = dptx_ip_pat_sets[pattern];
		edp_reg_write(edp, port,
			TR_DPTX_DISABLE_SCRAMBLING, DP_test_pat[pattern].SR_disable);
		edp_reg_write(edp, port, TR_DPTX_LINK_QUAL_PATTERN_SET, 0);
		edp_reg_write(edp, port, TR_DPTX_TRAINING_PATTERN_SET, reg_val);
		break;
	case eDPTX_QUAL_PAT_DISABLE:
	case eDPTX_D10P2:
	case eDPTX_SYMBOL_ERROR_MSR:
	case eDPTX_PRBS7:
	case eDPTX_HBR2_EYE:
	case eDPTX_CP2520_2:
	case eDPTX_CP2520_3:
		reg_val = (lane_mask & BIT(0) ? dptx_ip_pat_sets[pattern] << 0  : 0) |
			  (lane_mask & BIT(1) ? dptx_ip_pat_sets[pattern] << 8  : 0) |
			  (lane_mask & BIT(2) ? dptx_ip_pat_sets[pattern] << 16 : 0) |
			  (lane_mask & BIT(3) ? dptx_ip_pat_sets[pattern] << 24 : 0);
		edp_reg_write(edp, port,
			TR_DPTX_DISABLE_SCRAMBLING, DP_test_pat[pattern].SR_disable);
		edp_reg_write(edp, port, TR_DPTX_TRAINING_PATTERN_SET, 0);
		edp_reg_write(edp, port, TR_DPTX_LINK_QUAL_PATTERN_SET, reg_val);
		break;
	case eDPTX_80BIT_CUSTOM:
		edp_reg_write(edp, port, TR_DPTX_CUSTOM_80BIT_PATTERN_31_0,  cos_80b_0);
		edp_reg_write(edp, port, TR_DPTX_CUSTOM_80BIT_PATTERN_63_32, cos_80b_1);
		edp_reg_write(edp, port, TR_DPTX_CUSTOM_80BIT_PATTERN_79_64, cos_80b_2);
		reg_val = (lane_mask & BIT(0) ? dptx_ip_pat_sets[pattern] << 0  : 0) |
			  (lane_mask & BIT(1) ? dptx_ip_pat_sets[pattern] << 8  : 0) |
			  (lane_mask & BIT(2) ? dptx_ip_pat_sets[pattern] << 16 : 0) |
			  (lane_mask & BIT(3) ? dptx_ip_pat_sets[pattern] << 24 : 0);
		edp_reg_write(edp, port,
			TR_DPTX_DISABLE_SCRAMBLING, DP_test_pat[pattern].SR_disable);
		edp_reg_write(edp, port, TR_DPTX_TRAINING_PATTERN_SET, 0);
		edp_reg_write(edp, port, TR_DPTX_LINK_QUAL_PATTERN_SET, reg_val);
		break;
	default:
		eDP_ERR(edp, "%d invalid", pattern);
		break;
	}
}

static void dptx_set_MSA(struct edptx_drv_s *edp, u8 port)
{
	unsigned int hactive, vactive, htotal, vtotal, hsw, hbp, vsw, vbp;
	unsigned int bpp, symbol_count, user_data_count,
		misc0_data, misc1_data, bit_depth, sync_mode;
		// data_per_lane, misc0_data, misc1_data, bit_depth, sync_mode;
	unsigned int m_vid; /*pclk/1000 */
	unsigned int n_vid; /*162000, 270000, 540000 */
	u8 ppc = 1; /* 1 pix per clock pix0 only */
	unsigned int cfmt, tu_cfg;
	u8 tu_size;
	u8 port_cnt = edp->sink.port_mask == 0x3 ? 2 : 1;
	// u8 port_cnt = 1;

	// printf("temp port 111111\n");

	// UNUSED_PARAM(data_per_lane);

	hactive = edp->act_timing.h_act    / port_cnt;
	htotal  = edp->act_timing.h_period / port_cnt;
	hsw     = edp->act_timing.h_pw     / port_cnt;
	hbp     = edp->act_timing.h_bp     / port_cnt;
	vactive = edp->act_timing.v_act;
	vtotal  = edp->act_timing.v_period;
	vsw     = edp->act_timing.h_pw;
	vbp     = edp->act_timing.v_bp;

	m_vid = dptx_div_around(edp->act_timing.pclk / port_cnt, 1000);
	// m_vid = (edp->vid_clk.fout / port_cnt) / 1000; //pxp debug
	if (edp->sink.link[port].link_rate == DP_LINK_RATE_HBR2)
		n_vid = 540000;
	else if (edp->sink.link[port].link_rate == DP_LINK_RATE_HBR)
		n_vid = 270000;
	else
		n_vid = 162000;

	switch (edp->act_timing.cfmt) {
	case DPTX_CFMT_RGB_8bit:
		bit_depth = 0x1; cfmt = 0; bpp = 24; break;
	case DPTX_CFMT_RGB_10bit:
		bit_depth = 0x2; cfmt = 0; bpp = 30; break;
	case DPTX_CFMT_RGB_12bit:
		bit_depth = 0x3; cfmt = 0; bpp = 36; break;
	case DPTX_CFMT_YCbCr422_8bit:
		bit_depth = 0x1; cfmt = 1; bpp = 16; break;
	case DPTX_CFMT_YCbCr422_10bit:
		bit_depth = 0x2; cfmt = 1; bpp = 20; break;
	case DPTX_CFMT_YCbCr422_12bit:
		bit_depth = 0x3; cfmt = 1; bpp = 24; break;
	case DPTX_CFMT_YCbCr444_8bit:
		bit_depth = 0x1; cfmt = 2; bpp = 24; break;
	case DPTX_CFMT_YCbCr444_10bit:
		bit_depth = 0x2; cfmt = 2; bpp = 30; break;
	case DPTX_CFMT_YCbCr444_12bit:
		bit_depth = 0x3; cfmt = 2; bpp = 36; break;
	case DPTX_CFMT_RGB_6bit:
	case DPTX_CFMT_invalid:
	default:
		bit_depth = 0x0; cfmt = 0; bpp = 18; break;
	}

	// sync_mode = edp->sink.link[port].cap.sync_clk_mode;
	sync_mode = 0;

	/*bit[0] sync mode (1=sync 0=async) */
	misc0_data = (bit_depth << 5) | (cfmt << 1) | (sync_mode << 0);
	misc1_data = 0 << 7; // TODO: 1 for Y-only and RAW

	// m_vid is ppp in kHz // by experience
	if (m_vid > 800000) // > 800M
		tu_size = 32;
	else if (m_vid > 200000) // > 200M
		tu_size = 48;
	else
		tu_size = 64;
	// tu_size = 64;

	tu_cfg     = ((0 & 0xf) << 24)   | //FRAC_SYMBOLS_PER_TU
		     ((64 & 0xff) << 16) | //SYMBOLS_PER_TU
		     ((tu_size & 0xff) << 0); //TRANSFER_UNIT_SIZE

	symbol_count = ((hactive * bpp) + 7) / 8;
	user_data_count = (symbol_count + edp->sink.link[port].lane_count - 1) /
				edp->sink.link[port].lane_count;

	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_HTOTAL, htotal);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_VTOTAL, vtotal);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_POLARITY, (0 << 1) | (0 << 0));
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_HSWIDTH, hsw);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_VSWIDTH, vsw);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_HRES, hactive);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_VRES, vactive);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_HSTART, (hsw + hbp));
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_VSTART, (vsw + vbp));
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_MISC0, misc0_data);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_MISC1, misc1_data);
	edp_reg_write(edp, port, TR_DPTX_SRCX_MVID, m_vid); /*unit: 1kHz */
	edp_reg_write(edp, port, TR_DPTX_SRCX_TU_CONFIG, tu_cfg); /*unit: 1kHz */
	edp_reg_write(edp, port, TR_DPTX_SRCX_NVID, n_vid); /*unit: 100kHz */
	edp_reg_write(edp, port, TR_DPTX_SRCX_USER_PIXEL_COUNT, ppc); /*need confirm */
	edp_reg_write(edp, port, TR_DPTX_SRCX_USER_DATA_COUNT, user_data_count); /*need confirm*/
	edp_reg_write(edp, port, TR_DPTX_SRCX_MAIN_STREAM_INTERLACED, 0);
	edp_reg_write(edp, port, TR_DPTX_SRCX_USER_SYNC_POLARITY, 0x7); /*need confirm */
	edp_reg_write(edp, port, TR_DPTX_SRCX_USER_CONTROL, 0x01); /*need confirm */

	// SRC0_USER_FIFO_STATUS
	// SRC0_USER_FRAMING_STATUS
}

static void dptx_reset_t7(struct edptx_drv_s *edp, u8 port, u8 mask)
{
	// unsigned int bit;

	if (mask & DPTX_RESET_PHY) {
		// edp_reg_write(edp, port, EDP_TX_PHY_RESET, 0xf); //reset the PHY
		dptx_delay_ms(1);
	}

	if (mask & DPTX_RESET_COMBO_DPHY) {
		//bit = (edp->idx == 0 && port == 0) ? 19 : 20; //combo dphy
		//reset_reg_setb(RESETCTRL_RESET1_MASK, 0, bit, 1);
		//reset_reg_setb(RESETCTRL_RESET1_LEVEL, 0, bit, 1);
		//disp_dptx_delay_us(1);
		//reset_reg_setb(RESETCTRL_RESET1_LEVEL, 1, bit, 1);
		//disp_dptx_delay_us(5);
		//reset_reg_setb(RESETCTRL_RESET1_MASK, 1, bit, 1);
	}

	if (mask & DPTX_RESET_eDP_CTRL) {
		//bit = (edp->idx == 0 && port == 0) ? 17 : 18; //eDP ctrl
		//reset_reg_setb(RESETCTRL_RESET1_MASK, 0, bit, 1);
		//reset_reg_setb(RESETCTRL_RESET1_LEVEL, 0, bit, 1);
		//disp_dptx_delay_us(1);
		//reset_reg_setb(RESETCTRL_RESET1_LEVEL, 1, bit, 1);
		//disp_dptx_delay_us(5);
		//reset_reg_setb(RESETCTRL_RESET1_MASK, 1, bit, 1);
	}

	if (mask & (DPTX_RESET_eDP_CTRL | DPTX_RESET_eDP_PIPE | DPTX_RESET_COMBO_DPHY))
		dptx_delay_ms(1);

	if (mask & DPTX_RESET_AUX_CLK_DIVIDER) {
		//Set Aux channel clk-div: 24MHz
		// edp_reg_write(edp, port, TR_DPTX_AUX_CLOCK_DIVIDER, 50);
		edp_reg_write(edp, port, TR_DPTX_AUX_CLOCK_DIVIDER, 200);
		// edp_reg_write(edp, port, TR_DPTX_AUX_CLOCK_DIVIDER, 162);
	}

	if (mask & DPTX_RESET_PHY) {
		//Enable the transmitter
		//remove the reset on the PHY
		// edp_reg_write(edp, port, EDP_TX_PHY_RESET, 0);
	}

	if (mask & DPTX_RESET_VENC) {
		//bit = (edp->idx == 0) ? 24 : 27;
		//reset_reg_setb(RESETCTRL_RESET0_MASK, 0, bit, 1);
		//reset_reg_setb(RESETCTRL_RESET0_LEVEL, 0, bit, 1);
		//disp_dptx_delay_us(1);
		//reset_reg_setb(RESETCTRL_RESET0_LEVEL, 1, bit, 1);
		//disp_dptx_delay_us(5);
		//reset_reg_setb(RESETCTRL_RESET0_MASK, 1, bit, 1);
	}
}

static void dptx_set_lane_config_to_IP(struct edptx_drv_s *edp, u8 port)
{
	// tx Link-rate and Lane_count
	edp_reg_write(edp, port, TR_DPTX_LINK_BW_SET, edp->sink.link[port].link_rate);
	edp_reg_write(edp, port, TR_DPTX_LANE_COUNT_SET, edp->sink.link[port].lane_count);
	edp_reg_write(edp, port, TR_DPTX_ENHANCED_FRAMING_ENABLE,
			edp->sink.link[port].enh_frame_en);
	// edp_reg_write(edp, port,
		// EDP_TX_PHY_POWER_DOWN, (0xf << edp->sink.link[port].lane_count) & 0xf);
	// edp_reg_write(edp, port,
		// EDP_TX_DOWNSPREAD_CTRL, edp->sink.link[port].link_cap & BIT(5) ? 1 : 0);
}

static void dptx_set_phy_config_to_IP(struct edptx_drv_s *edp, u8 port, u8 use_preset)
{
	// UNUSED_PARAM(edp);
	// UNUSED_PARAM(port);
	// UNUSED_PARAM(use_preset);
}

static int dptx_wait_phy_ready(struct edptx_drv_s *edp, u8 port)
{
	//UNUSED_PARAM(edp);
	//UNUSED_PARAM(port);
	//u32 data = 0x7f;
	//u8 done = 0;

	//// to delete after chip back, pxp no phy
	//do {
	//	data = edp_reg_read(edp, port, TR_DPTX_PHY_STATUS);
	//	if (done > 8)
	//		eDP_P_DBG(edp, port, "wait phy ready: val=0x%x, cnt=%u", data, done);
	//	done++;
	//	disp_dptx_delay_us(1);
	//} while (((data & 0x7f) != 0x7f) && (done < 10));

	//if ((data & 0x7f) == 0x7f)
	//	return 0;

	//eDP_P_ERR(edp, port, "phy init error!");
	//return -1;
	return 0;
}

static void dptx_transmitter_init(struct edptx_drv_s *edp, u8 port)
{
	dptx_reset_t7(edp, port, DPTX_RESET_AUX_CLK_DIVIDER | DPTX_RESET_PHY);
	dptx_wait_phy_ready(edp, port);
	dptx_delay_ms(1);

	edp_reg_write(edp, port, TR_DPTX_TRANSMITTER_ENABLE, 0x1);
	edp_reg_write(edp, port, TR_DPTX_SOFT_RESET, 0x1f); //need confirm
	//edp_reg_write(edp->idx, EDP_TX_AUX_INTERRUPT_MASK, 0); //turn off interrupt

	edp_reg_write(edp, port, TR_DPTX_SRCX_VIDEO_STREAM_ENABLE, 0x0);
}

static void dptx_transmitter_output_set(struct edptx_drv_s *edp, u8 port, u8 en)
{
	//if (en)
	//	edp_reg_write(edp, port, TR_DPTX_TRANSMITTER_ENABLE, 1);
	//else
	//	edp_reg_write(edp, port, EDP_TX_FORCE_SCRAMBLER_RESET, 0x1);
	edp_reg_write(edp, port, TR_DPTX_SRCX_VIDEO_STREAM_ENABLE, en ? 0x1 : 0);
	edp_reg_write(edp, port, TR_DPTX_SOURCE_ENABLE, en ? 0x1 : 0);

	//if (!en)
	//	edp_reg_write(edp, port, TR_DPTX_TRANSMITTER_ENABLE, 0x0);
}

static u8 dptx_get_hpd_level(struct edptx_drv_s *edp, u8 port)
{
	return edp_reg_getb(edp, port, TR_DPTX_HPD_INPUT_STATE, 0, 1);
}

static u16 dptx_get_hpd_irq(struct edptx_drv_s *edp, u8 port)
{
	u32 irq_cause;
	//irq_cause = edp_reg_read(edp, port, TR_DPTX_INTERRUPT_CAUSE);

	//if (irq_cause & 0x3) // HPD_IRQ | HPD_EVENT
	//	irq_cause = edp_reg_read(edp, port, TR_DPTX_INTERRUPT_STATE);
	//else
	//	irq_cause = 0;
	//edp_reg_write(edp, port, TR_DPTX_INTERRUPT_STATE, 0xffffffff);
	//return irq_cause;

	irq_cause = edp_reg_read(edp, port, TR_DPTX_INTERRUPT_STATE);
	edp_reg_write(edp, port, TR_DPTX_INTERRUPT_STATE, 0xffffffff);
	return irq_cause;
}

static void dptx_interrupt_mask_set(struct edptx_drv_s *edp, u8 port, u8 mask)
{
/* TR_DPTX_INTERRUPT_MASK
 * 1 HPD_IRQ_MASK: Write a 0 to this bit to allow HPD_IRQ events to cause an interrupt
 * 0 HPD_EVENT_MASK: Write a 0 to this bit to allow HPD present events to cause an interrupt.
 */
	edp_reg_write(edp, port, TR_DPTX_INTERRUPT_MASK, 0xffffffff & ~(mask));

	//if (mask)
	edp_reg_write(edp, port, TR_DPTX_INTERRUPT_STATE, 0xffffffff);
}

static void dptx_set_scramble_reset(struct edptx_drv_s *edp, u8 port, u8 sr_type)
{
	edp_reg_write(edp, port, TR_DPTX_DISABLE_SCRAMBLING,
		sr_type == DPTX_SCRAMBLE_RESET_OFF ? 0x01 : 0x00);
/* TR_DPTX_EDP_CAPABILITY_CONFIG
 *    1 ENABLE_REDUCED_AUX_SYNC
 *        For eDP applications only, enabling this control bit reduces the number of sync
 *        pulses sent during AUX channel transactions from 16 to 8.
 *    0 ALTERNATE_SCRAMBLER_RESET
 *        Set this bit to a 1 to force the transmitter core to use the alternate scrambler
 *        reset value.
 */
	edp_reg_write(edp, port, TR_DPTX_EDP_CAPABILITY_CONFIG,
		sr_type == DPTX_eDP_ALTERNATIVE_SCRAMBLE_RESET ? 0x01 : 0x00);

	//edp_reg_write(edp, port, EDP_TX_FORCE_SCRAMBLER_RESET, 0x1);
}

static void dptx_PSR1_ctrl_set(struct edptx_drv_s *edp, u8 port, u8 flag)
{
/*
 * 1 – Set to a 1 to enable 3D information to be transmitted in the VSC packet.
 * 0 – Set to a 1 to enable the transmission of panel self-refresh information in the VSC packet
 *  information fields.
 */
	edp_reg_write(edp, port, TR_DPTX_SECX_PSR_3D_ENABLE, flag ? 0x01 : 0x00);
/*
 *    3 ENABLE_Y_COORD: Set this configuration bit to a 1 to enable the transmission of the Y
 *      coordinate with PSR2 VSC packets. This bit should only be enabled when the connected
 *      sink device supports eDP 1.4a. (DPCD 0x00070 is 3 or greater).
 *    2 INACTIVE_NO_EXIT: When set to a 1, the internal PSR controller will transition to the
 *      inactive state at the next vertical sync without waiting for any additional capture frames.
 *    1 PSR_FRAMES_TO_ACTIVE: Determines the number of input frames to wait between the transition
 *      from the inactive to the active state. Set this bit to a 1 to wait for 2 input frames or a
 *      0 to wait for 1 input frame.
 *    0 PSR_DISABLE_CAPTURE: When set to a 1, this control bit allows the core to automatically
 *      disable the capture ports when the PSR state machine is in the ACTIVE state. Set to a 0 to
 *      control the enable and disable of the capture ports during PSR manually.
 */
	edp_reg_write(edp, port, TR_DPTX_SECX_PSR_CONFIG, flag ? 0x03 : 0x00);
}

static void dptx_PSR2_ctrl_set(struct edptx_drv_s *edp, u8 port, u8 flag)
{
	//edp_reg_write(edp, port, TR_DPTX_SECX_PSR_3D_ENABLE, flag ? 0x01 : 0x00);
	//TODO
}

static void edptx_reg_get_link(struct edptx_drv_s *edp, u8 port, u32 *link, u8 *lane)
{
	if (link) {
		*link = 0;
		*link |= edp_reg_read(edp, port, TR_DPTX_LINK_BW_SET);
	}
	if (lane) {
		*lane = 0;
		*lane |= edp_reg_read(edp, port, TR_DPTX_LANE_COUNT_SET);
	}
}

static const struct edptx_if_ctrl_s edptx_if_tr14 = {
	.aux_write = _aux_write,
	.aux_write_single = _aux_write_single,
	.aux_read = _aux_read,
	.aux_i2c_op = _aux_i2c_op,

	.transmit_pattern = dptx_transmit_pattern,
	.set_MSA = dptx_set_MSA,

	.path_reset = dptx_reset_t7,

	.lane_cfg_to_IP = dptx_set_lane_config_to_IP,
	.phy_cfg_to_IP = dptx_set_phy_config_to_IP,

	.transmitter_init = dptx_transmitter_init,
	.transmitter_output = dptx_transmitter_output_set,

	.get_hpd_level = dptx_get_hpd_level,
	.get_hpd_irq = dptx_get_hpd_irq,
	.set_hpd_interrupt_mask = dptx_interrupt_mask_set,

	.scramble_reset_set = dptx_set_scramble_reset,

	.PSR1_SDP_ctrl = dptx_PSR1_ctrl_set,
	.PSR2_SDP_ctrl = dptx_PSR2_ctrl_set,

	.reg_link_get = edptx_reg_get_link,
};

struct edptx_if_ctrl_s *edptx_if_bind_tr14(struct edptx_drv_s *edp)
{
	// UNUSED_PARAM(edp);
	return (struct edptx_if_ctrl_s *)&edptx_if_tr14;
}
