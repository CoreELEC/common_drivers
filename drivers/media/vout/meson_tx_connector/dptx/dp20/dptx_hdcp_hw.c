// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/amlogic/media/vout/meson_tx_connector/dptx_common/dptx_hw_common.h>
#include "dptx_log.h"
#include "dptx_aux_helper.h"
#include "dptx_hw.h"
#include "dptx_hdcp_hw.h"
#include "../../hdcp/hdmitx_hdcp.h"

bool dptx_get_hdcptx_result(struct hdcptx_common *hdcptx_comm, int hdcp_type)
{
	struct arm_smccc_res res;

	if (!hdcptx_comm)
		return 0;
	if (hdcp_type == HDCP_VER_HDCP2X && hdcptx_comm->chip_type != MESON_CPU_ID_S1A) {
		arm_smccc_smc(HDCPTX_IOOPR, HDCP22_RESULT, 0, 0, 0, 0, 0, 0, &res);
		return res.a0 == 1;
	}

	DPTX_INFO("only dptx hdcp22 use the api get result\n");
	return 0;
}

int dptx_hdcptx_get_hdcprx_ver(struct hdcptx_common *hdcptx_comm)
{
	int ret = 0;
	struct dptx_hw_common *hw_comm = hdcptx_comm->bind_hw_comm;

	if (!hdcptx_comm || !hw_comm) {
		DPTX_ERROR("hdcptx_comm is NULL\n");
		return -EINVAL;
	}
	/*
	 * it easily read fails under FRL mode as FRL ddc bus stall operation,
	 * so use hdmitx_ddcm_read() method instead
	 */
	/* hdmitx_ddc_read_1byte(DDC_HDCP_DEVICE_ADDR, REG_DDC_HDCP_VERSION, &cap_val); */
	//if (hdcptx_comm->ddcm_read)
	//	ret = hdcptx_comm->ddcm_read(0, DDC_HDCP_DEVICE_ADDR,
	//		REG_DDC_HDCP_VERSION, &cap_val, 1, TPI_DDC_CMD_SEQUENTIAL_READ);
	//if (ret)
	//	HDMITX_ERROR("hdmitx: ddc read hdcp version failed\n");

	//todo here maybe no need aux read, maybe use the sw value
	ret = dptx_hdcp2_msg_read_rxcaps(hdcptx_comm->bind_hw_comm);
	if (ret < 0) {
		//todo aux read fail
		DPTX_ERROR("get_hdcprx_ver rxcap read ret:%d\n", ret);
	}
	if ((((hw_comm->dp_hdcp->rxcap >> 16) & 0xff) & 0x2) != 0x2)
		return HDCP_VER_HDCP2X;
	else
		return HDCP_VER_HDCP1X;
}

bool dptx_hdcptx_ddc_check_busy(void)
{
//	enum ddc_err_t error = DDC_ERR_NONE;
//	u8 time_out_ms = 7;
//
//	usleep_range(2000, 3000);
//	do {
//		if (!hdmitx_ddc_bus_wait_free()) {
//			error = DDC_ERR_BUSY;
//			time_out_ms--;
//			if (!time_out_ms)
//				return false;
//			usleep_range(2000, 3000);
//		} else {
//			error = DDC_ERR_NONE;
//		}
//	} while (error != DDC_ERR_NONE);
	return true;
}

void dptx_hdcptx_en_aes_dual_pipe(struct dptx_hw_common *hw_comm, bool en)
{
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_CP2TX_AESCTL_IVCTX, en ? 3 : 0,
						BIT_CP2TX_AESCTL_RI_AES_DUALPIPE_OVR, 2);
}

static void dptx_hdcptx2_smng_auto(struct dptx_hw_common *hw_comm, bool en)
{
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_CP2TX_CTRL_0_IVCTX, en,
						BIT_CP2TX_CTRL_0_RI_SMNG_AUTO, 1);
}

void dptx_hdcptx_init_reg(struct dptx_hw_common *hw_comm)
{
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP_CTRL_IVCTX, false, BIT(2), 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP_CTRL_IVCTX, true, BIT(2), 1);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TP1_IVCTX, 0x92);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_TPI_MISC_IVCTX, 0x1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_CP2TX_CTRL_2_IVCTX, false,
						BIT_CP2TX_CTRL_2_RI_SEQNUMM_AUTO, 1);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_RI_128_COMP_IVCTX, 0x0);
	dptx_hdcptx2_smng_auto(hw_comm, false);
}

u8 dptx_hdmi_ddc_status_check(void)
{
	bool check_failed = false;
	//u8 ddc_status;

	//ddc_status = hdmitx21_rd_reg(DDC_STATUS_IVCTX);
	//if (ddc_status & (BIT_DDC_STATUS_BUSLOW | BIT_DDC_STATUS_NACK)) {
	//	hdmitx21_wr_reg(DDC_STATUS_IVCTX, 0x00);
	//	check_failed = true;
	//}

	return check_failed;
}

u8 dptx_hdcptx2_rpt_dev_cnt_get(struct dptx_hw_common *hw_comm)
{
	return dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RPT_DEVCNT_IVCTX);
}

u8 dptx_hdcptx2_topology_get(struct dptx_hw_common *hw_comm)
{
	return dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RPT_DETAIL_IVCTX);
}

u8 dptx_hdcptx2_rpt_depth_get(struct dptx_hw_common *hw_comm)
{
	return dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RPT_DEPTH_IVCTX);
}

void dptx_hdcptx2_ds_rpt_rcvid_list_read(struct dptx_hw_common *hw_comm, u8 *p_rpt_rcv_id,
				u8 dev_count, u8 bytes_to_read)
{
	u8 i, j;

	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, true,
						BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD_START, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, false,
						BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD_START, 1);

	for (j = 0; j < dev_count; j++) {
		for (i = 0; i < bytes_to_read; i++) {
			*p_rpt_rcv_id++ = dptx20_reg_read(hw_comm, CORE_LEVEL,
							DP_CP2TX_RPT_RCVID_OUT_IVCTX);
			dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, false,
							BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD, 1);
			dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, true,
							BIT_CP2TX_TX_CTRL_0_RI_RPT_RCVID_RD, 1);
		}
	}
}

void dptx_hdcptx2_ds_rcv_id_read(struct dptx_hw_common *hw_comm, u8 *p_rcv_id)
{
	p_rcv_id[4] = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RX_ID_CORE_0_IVCTX);
	p_rcv_id[3] = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RX_ID_CORE_1_IVCTX);
	p_rcv_id[2] = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RX_ID_CORE_2_IVCTX);
	p_rcv_id[1] = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RX_ID_CORE_3_IVCTX);
	p_rcv_id[0] = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_RX_ID_CORE_4_IVCTX);
}

u8 dptx_hdcp2x_get_state_st(struct dptx_hw_common *hw_comm)
{
	return dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_STATE_IVCTX);
}

bool dptx_hdcptx2_auth_status(struct dptx_hw_common *hw_comm)
{
	u8 hdcp2_auth_status = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_AUTH_STAT_IVCTX);
	u8 hdcp2_auth_state_status = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_STATE_IVCTX);

	if (hdcp2_auth_status == 0x81 && hdcp2_auth_state_status == 0x2B)
		return true;
	else
		return false;
}

/* cmd: 1: get. 2: set */
bool dptx_hdcptx22_topo_ctrl(struct hdcptx_common *hdcptx_comm, int cmd, int topo_type)
{
	struct arm_smccc_res res;

	if (!hdcptx_comm || hdcptx_comm->chip_type == MESON_CPU_ID_S1A)
		return false;
	if (cmd == GET_HDCP22_TOPO) {
		arm_smccc_smc(HDCPTX_IOOPR, HDCP22_GET_TOPO, 0, 0, 0, 0, 0, 0, &res);
		return res.a0 == 1;
	} else if (cmd == SET_HDCP22_TOPO) {
		DPTX_INFO("hdcptx22_topo_ctrl: %d\n", topo_type);
		arm_smccc_smc(HDCPTX_IOOPR, HDCP22_SET_TOPO, topo_type, 0, 0, 0, 0, 0, &res);
	}
	return false;
}

void dptx_hdcptx2_csm_send(struct dptx_hw_common *hw_comm, struct hdcp_csm_t *csm_msg)
{
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_RPT_SMNG_K_IVCTX, 1);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_SEQ_NUM_M_0_IVCTX,
					(u8)(csm_msg->seq_num_m & 0xff));
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_SEQ_NUM_M_1_IVCTX,
					(u8)((csm_msg->seq_num_m >> 8) & 0xff));
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_SEQ_NUM_M_2_IVCTX,
					(u8)((csm_msg->seq_num_m >> 16) & 0xff));
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX,
					BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_WR_START);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0x00);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_RPT_SMNG_IN_IVCTX,
					(u8)(csm_msg->streamid_type >> 8));
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX,
					BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_WR);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_RPT_SMNG_IN_IVCTX,
					(u8)(csm_msg->streamid_type));
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX,
					BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_WR);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0);
}

bool dptx_hdcptx2_ds_rptr_capability(struct dptx_hw_common *hw_comm)
{
	if (dptx20_reg_read(hw_comm, CORE_LEVEL, DP_CP2TX_GEN_STATUS_IVCTX) &
		BIT_CP2TX_GEN_STATUS_RO_REPEATER)
		return true;
	else
		return false;
}

void dptx_hdcptx2_rpt_smng_xfer_start(struct dptx_hw_common *hw_comm)
{
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX,
					 BIT_CP2TX_TX_CTRL_0_RI_RPT_SMNG_XFER_START);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0);
}

void dptx_hdcptx2_encryption_update(struct dptx_hw_common *hw_comm, bool en)
{
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_0_IVCTX, en,
						BIT_HDCP2X_CTL_0_ENCRYPT_EN, 1);
}

//todo need confirm with vlsi
void dptx_hdcptx2_auth_stop(struct dptx_hw_common *hw_comm)
{
	u8 ddc_status;
	u8 count = 0;

	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_HDCP2X_POLL_CS_IVCTX, 0x71);

	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, false,
						BIT_HDCP2X_CTL_1_HPD_SW, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, true,
						BIT_HDCP2X_CTL_1_HPD_OVR, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, true,
						BIT_HDCP2X_CTL_1_REAUTH_SW, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, false,
						BIT_HDCP2X_CTL_1_REAUTH_SW, 1);

	//TODO
	do {
		ddc_status = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_HDCP2X_DDCM_STS_IVCTX);
		if ((ddc_status & BIT_REGTX0_HDCP2X_DDC_STS_CTL_CS_B3_B0) == false)
			break;
		count++;
		if ((ddc_status & BIT_REGTX0_HDCP2X_DDC_STS_CTL_CS_B3_B0) == 0x0E) {
			dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_0_IVCTX, false,
								BIT_HDCP2X_CTL_0_EN, 1);
			dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TPI_DDC_MASTER_EN_IVCTX, true,
								BIT_TPI_DDC_MASTER_EN_HW_EN, 1);
			dptx20_reg_write(hw_comm, CORE_LEVEL, DP_DDC_CMD_IVCTX,
							DDC_CMD_ABORT_TRANSACTION);
			usleep_range(2000, 3000);
			dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TPI_DDC_MASTER_EN_IVCTX, false,
								BIT_TPI_DDC_MASTER_EN_HW_EN, 1);
			dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_0_IVCTX, true,
								BIT_HDCP2X_CTL_0_EN, 1);
		}
		usleep_range(1000, 2000);
	} while (count < 5);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_0_IVCTX, false,
						BIT_HDCP2X_CTL_0_EN, 1);
}

void dptx_hdcptx2_src_auth_start(struct dptx_hw_common *hw_comm, u8 content_type)
{
	u32 reset_val = 0;

	if (content_type != 0 && content_type != 1)
		content_type = 0;

	/* reset hdcp2x logic and HW state machine */
	/* mostly, ddc bus is already free after previous stop
	 * operation, now double check
	 */
	//todo
	//if (!hdmitx_ddc_bus_wait_free())
	reset_val = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_HDCP2X_TX_SRST_IVCTX);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_HDCP2X_TX_SRST_IVCTX, reset_val | 0x20);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_HDCP2X_TX_SRST_IVCTX, reset_val | (~0x20));

	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_AON_CYP_CTL_IVCTX, true, BIT(3), 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_AON_CYP_CTL_IVCTX, false, BIT(3), 1);

	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, true,
						BIT_HDCP2X_CTL_1_HPD_SW, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, true,
						BIT_HDCP2X_CTL_1_HPD_OVR, 1);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_GP_IN2_IVCTX,
				BIT_CP2TX_TIMEOUT_DISABLE_B2 | BIT_CP2TX_IGNORE_RXSTATUS_B0);
	//todo need confirm with vlsi, hdmi set bit7/0 same time
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_0_IVCTX, true,
						BIT_HDCP2X_CTL_0_EN, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_0_IVCTX, true,
						BIT_HDCP2X_CTL_0_ENCRYPT_EN, 1);
	//hdmitx21_set_bit(HDCP2X_CTL_0_IVCTX,
	//	BIT_HDCP2X_CTL_0_EN | BIT_HDCP2X_CTL_0_ENCRYPT_EN, true);
}

//todo need confirm with vlsi
void dptx_hdcptx2_reauth_send(struct dptx_hw_common *hw_comm)
{
	/* Enable Polling */
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_HDCP2X_POLL_CS_IVCTX, 0x70);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, true,
							BIT_HDCP2X_CTL_1_HPD_SW, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, true,
							BIT_HDCP2X_CTL_1_REAUTH_SW, 1);
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, false,
							BIT_HDCP2X_CTL_1_REAUTH_SW, 1);

	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_CTRL_1_IVCTX, 0x21);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DP_CP2TX_CTRL_1_IVCTX, 0x20);
}

static void dptx_hdcptx_ctrl_gate(struct dptx_hw_common *hw_comm, int hdcp_mode, bool en)
{
	if (hdcp_mode == 1) {
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TOP_CLK_GATE, en,
							BIT_HDMITX_TOP_CLK_GATE_HDCP1X, 1);
		DPTX_DEBUG("hdcp1x gate %d\n", en);
	} else if (hdcp_mode == 2) {
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TOP_CLK_GATE, en,
							BIT_HDMITX_TOP_CLK_GATE_HDCP2X, 1);
		DPTX_DEBUG("hdcp2x gate %d\n", en);
	} else if (hdcp_mode == 0) {
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TOP_CLK_GATE, en,
							BIT_HDMITX_TOP_CLK_GATE_HDCP1X, 1);
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TOP_CLK_GATE, en,
							BIT_HDMITX_TOP_CLK_GATE_HDCP2X, 1);
		DPTX_DEBUG("hdcp gate %d\n", en);
	}
}

void dptx_ctrl_hdcp_gate(struct hdcptx_common *hdcptx_comm, int hdcp_mode, bool en)
{
	//switch (hdcptx_comm->chip_type) {
	//case MESON_CPU_ID_S7:
	//case MESON_CPU_ID_S7D:
	//case MESON_CPU_ID_S6:
	dptx_hdcptx_ctrl_gate(hdcptx_comm->bind_hw_comm, hdcp_mode, en);
	//	break;
	//default:
	//	break;
	//}
}

u32 dptx_get_gate_status(struct dptx_hw_common *hw_comm)
{
	return dptx20_reg_read(hw_comm, CORE_LEVEL, DP_TOP_CLK_GATE);
}

bool dptx_get_hdcptx_lstore(struct hdcptx_common *hdcptx_comm, int hdcp_type)
{
	struct arm_smccc_res res;

	if (!hdcptx_comm)
		return 0;

	if (hdcp_type == HDCP_VER_HDCP1X) {
		if (hdcptx_comm->efuse_dis_hdcp_tx14)
			return 0;
		arm_smccc_smc(HDCPTX_IOOPR, HDCP14_KEY_READY, 0, 0, 0, 0, 0, 0, &res);
		return res.a0 == 1;
	} else if (hdcp_type == HDCP_VER_HDCP2X) {
		if (hdcptx_comm->chip_type == MESON_CPU_ID_S1A || hdcptx_comm->efuse_dis_hdcp_tx22)
			return 0;
		arm_smccc_smc(HDCPTX_IOOPR, HDCP22_KEY_READY, 0, 0, 0, 0, 0, 0, &res);
		return res.a0 == 1;
	}
	return 0;
}

bool dptx_hdcptx1_load_key(void)
{
	struct arm_smccc_res res;

	// hdcptx14_load_key
	arm_smccc_smc(HDCPTX_IOOPR, HDCP14_LOADKEY, 0, 0, 0, 0, 0, 0, &res);

	return res.a0 == 1;
}

void dptx_hdcptx1_encryption_update(struct dptx_hw_common *hw_comm, bool en)
{
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TPI_COPP_DATA2_IVCTX,
						!en, BIT_TPI_COPP_DATA2_INTR_ENCRYPTION, 1);
}

//todo
void dptx_hdcptx1_auth_start(void)
{
//	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, true);
//	hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX, BIT_TPI_COPP_DATA2_CANCEL_PROT_EN, false);
//	hdmitx21_set_bit(TPI_COPP_DATA2_IVCTX,
//		(BIT_TPI_COPP_DATA2_TPI_HDCP_PREP_EN |
//		BIT_TPI_COPP_DATA2_DOUBLE_RI_CHECK |
//		BIT_TPI_COPP_DATA2_INTER_RI_CHECK_EN |
//		/* BIT_TPI_COPP_DATA2_KSV_FORWARD | */
//		BIT_TPI_COPP_DATA2_COPP_PROTLEVEL), true);
//	hdmitx21_set_bit(LM_DDC_IVCTX, BIT_LM_DDC_SWTPIEN_B7, false);
}

bool dptx_hdcptx1_ds_rptr_capability(struct dptx_hw_common *hw_comm)
{
	if (dptx20_reg_read(hw_comm, CORE_LEVEL, DP_TPI_COPP_DATA1_IVCTX) &
				BIT_TPI_COPP_DATA1_HDCP_REP)
		return true;
	else
		return false;
}

u8 dptx_hdcptx1_ds_cap_status_get(struct dptx_hw_common *hw_comm)
{
	return dptx20_reg_read(hw_comm, CORE_LEVEL, DP_TPI_COPP_DATA1_IVCTX);
}

void dptx_hdcptx1_bstatus_get(struct dptx_hw_common *hw_comm, u8 *p_ds_bstatus)
{
	dptx20_seq_rd_reg(hw_comm, CORE_LEVEL, DP_TPI_BSTATUS1_IVCTX, p_ds_bstatus, 2);
}

void dptx_hdcptx1_get_ds_ksvlists(struct dptx_hw_common *hw_comm, u8 **p_ksv, u8 count)
{
	u16 bytes_to_read = 0;
	u16 fifo_byte_counter = 0;
	u8 time_out = 100; /* timeout for reading ds ksv list */
	u8 fifo_status = 0;
	int temp = 0;
	int i = 0;
	u32 fifo_byte;

	/* Clear hash done interrupt */
	dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_INTR2_SW_TPI_IVCTX,
					true, BIT_INTR2_SW_HASH_DONE_B6, 1);

	fifo_byte_counter = count * KSV_SIZE;
	while ((fifo_byte_counter != 0) && time_out--) {
		if (bytes_to_read != 0) {
			/* get ds bksv list from fifo */
			/* hdmitx21_fifo_read(TPI_KSV_FIFO_FORW_IVCTX, *p_ksv, bytes_to_read); */
			for (i = 0; i < bytes_to_read; i++) {
				fifo_byte = dptx20_reg_read(hw_comm, CORE_LEVEL,
									DP_TPI_KSV_FIFO_FORW_IVCTX);
				(*p_ksv)[i] = fifo_byte & 0xff;
				dptx20_reg_write(hw_comm, CORE_LEVEL, DP_TPI_KSV_FIFO_FORW_IVCTX,
								 fifo_byte);
				/* printk("%02x\n", (*p_ksv)[i]); */
			}
			temp = temp + bytes_to_read;
			if ((temp % 64) == 0) {
				temp = 0;
				/* wait for hash done interrupt */
				while ((dptx20_reg_read(hw_comm, CORE_LEVEL,
						DP_INTR2_SW_TPI_IVCTX) & BIT(6)) == 0)
					;
				/* clear hash done interrupt */
				dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_INTR2_SW_TPI_IVCTX,
							true, BIT_INTR2_SW_HASH_DONE_B6, 1);
			}
			*p_ksv += bytes_to_read;
			fifo_byte_counter -= bytes_to_read;
		}
		fifo_status = dptx20_reg_read(hw_comm, CORE_LEVEL, DP_TPI_KSV_FIFO_STAT_IVCTX);
		bytes_to_read = fifo_status & BIT_TPI_KSV_FIFO_STAT_BYTES;
		/* printk("ksv_fifo_status: 0x%x\n", fifo_status); */
		mdelay(1);
	}
}

void dptx_hdcptx1_ds_bksv_read(struct hdcptx_common *hdcptx_comm, u8 *p_bksv, u8 b)
{
	dptx20_seq_rd_reg(hdcptx_comm->bind_hw_comm, CORE_LEVEL,
			DP_TPI_WR_BKSV_1_IVCTX, p_bksv, b);
}

void dptx_hdcptx1_protection_enable(struct dptx_hw_common *hw_comm, bool en)
{
	if (en) {
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TPI_COPP_DATA2_IVCTX,
				false, BIT_TPI_COPP_DATA2_CANCEL_PROT_EN, 1);
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TPI_COPP_DATA2_IVCTX, true,
			(BIT_TPI_COPP_DATA2_COPP_PROTLEVEL |
			BIT_TPI_COPP_DATA2_DOUBLE_RI_CHECK |
			BIT_TPI_COPP_DATA2_INTER_RI_CHECK_EN |
			BIT_TPI_COPP_DATA2_KSV_FORWARD), 1);
	} else {
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TPI_COPP_DATA2_IVCTX, false,
			(BIT_TPI_COPP_DATA2_COPP_PROTLEVEL |
			BIT_TPI_COPP_DATA2_DOUBLE_RI_CHECK |
			BIT_TPI_COPP_DATA2_INTER_RI_CHECK_EN |
			BIT_TPI_COPP_DATA2_KSV_FORWARD), 1);
		dptx20_set_reg_bits(hw_comm, CORE_LEVEL, DP_TPI_COPP_DATA2_IVCTX,
			true, BIT_TPI_COPP_DATA2_CANCEL_PROT_EN, 1);
	}
}

void _dptx_intr_enable(struct dptx_hw_common *hw_comm, struct dp_intr_t *pint, bool en)
{
	dptx20_reg_write(hw_comm, CORE_LEVEL, pint->intr_mask_reg,
				en ? pint->mask_data : 0);
}
