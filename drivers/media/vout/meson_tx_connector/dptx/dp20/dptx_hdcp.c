// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include "dptx20_reg.h"
#include "dptx_log.h"
#include "dptx_aux_helper.h"
#include "dptx_hw.h"
#include "dptx_hdcp_hw.h"
#include "dpcd_reg.h"
#include "dptx_hw_opcode.h"

static void dptx_hdcptx_result_notify_drm(struct dptx_common *tx_comm, int auth)
{
	void *cb_data = NULL;

	if (!tx_comm) {
		DPTX_ERROR("dptx notify drm hdcp NULL tx_comm instance!\n");
		return;
	}

	cb_data = tx_comm->hdcptx_comm.tx_hdcp_cb.data;
	if (tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify)
		tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify(cb_data,
			tx_comm->hdcptx_comm.hdcp_mode, auth);
}

int drm_dptx_common_register_hdcp_notify(struct dptx_common *tx_comm,
					   struct connector_hdcp_cb *cb)
{
	if (!tx_comm) {
		DPTX_ERROR("NULL tx_comm instance!\n");
		return -EINVAL;
	}

	if (!cb) {
		DPTX_ERROR("NULL callback!\n");
		return -EINVAL;
	}

	if (tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify)
		DPTX_ERROR("register hdcp notify again!?\n");

	tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify = cb->hdcp_notify;
	tx_comm->hdcptx_comm.tx_hdcp_cb.data = cb->data;

	return 0;
}
EXPORT_SYMBOL(drm_dptx_common_register_hdcp_notify);

static int dptx_authenticated;
MODULE_PARM_DESC(dptx_authenticated, "\n dptx_authenticated\n");
module_param(dptx_authenticated, int, 0444);

/* 40 * 200ms = 8s */
#define HDCP_AUTH_TIMEOUT (40)
/* hdcp monitor task, check hdcp auth status every 200ms */
int dptx_hdcp_stat_monitor_task(void *data)
{
	static int auth_stat;
	struct dptx_common *tx_comm = (struct dptx_common *)data;
	struct meson_tx_hw *hw_base = NULL;
	u8 en;

	if (!tx_comm) {
		DPTX_ERROR("dptx hdcp monitor NULL tx_comm instance\n");
		return -1;
	}
	hw_base = &tx_comm->hw_comm->hw_base;

	auth_stat = hw_base->hw_cntl(hw_base, DP_HDCP_GET_AUTH_RESULT, tx_comm, NULL);
	tx_comm->hdcptx_comm.hdcp_auth_result = auth_stat;
	dptx_hdcptx_result_notify_drm(tx_comm, auth_stat);

	//TODO: event and tracer
	while (!kthread_should_stop()) {
		dptx_authenticated = hw_base->hw_cntl(hw_base, DP_HDCP_GET_AUTH_RESULT,
					tx_comm, NULL);

		tx_comm->hdcptx_comm.hdcp_auth_result = auth_stat;
		dptx_hdcptx_result_notify_drm(tx_comm, auth_stat);
		if (auth_stat != dptx_authenticated) {
			auth_stat = dptx_authenticated;
			DPTX_HDCP_INFO("mode %d, auth: %d\n",
				tx_comm->hdcptx_comm.hdcp_mode, auth_stat);
			if (auth_stat == 1) {
				tx_comm->hdcptx_comm.hdcp_fail_cnt = 0;
				tx_comm->hdcptx_comm.hdcp_mode_fallback_state =
						HDCP_FALLBACK_LOCKED;
			}
		}
		if (dptx_authenticated == 0 && tx_comm->hdcptx_comm.hdcp_mode == 2 &&
			tx_comm->hdcptx_comm.hdcp_mode_fallback_state == HDCP_FALLBACK_NONE) {
			tx_comm->hdcptx_comm.hdcp_fail_cnt++;
			if (tx_comm->hdcptx_comm.hdcp_fail_cnt > HDCP_AUTH_TIMEOUT) {
				tx_comm->hdcptx_comm.hdcptx_start_type = HDCP_START_HDCP1X;
				en = 1;
				DPTX_HDCP_INFO("start hdcp14\n");
				hw_base->hw_cntl(hw_base, DP_HDCP1X_AUTH_EN, &en, NULL);
				tx_comm->hdcptx_comm.hdcp_fail_cnt = 0;
				tx_comm->hdcptx_comm.hdcp_mode_fallback_state =
					HDCP_FALLBACK_LOCKED;
			}
		}
		msleep_interruptible(200);
	}
	return 0;
}

/* like dptx_hdcptx_get_hdcprx_ver */
static unsigned int dptx_hdcptx_get_rx_version(struct dptx_common *tx_comm)
{
	unsigned int hdcprx_cap = 0;
	struct meson_tx_hw *hw_base = NULL;

	if (!tx_comm) {
		DPTX_ERROR("get_rx_version NULL tx_comm instance!");
		return 0;
	}
	hw_base = &tx_comm->hw_comm->hw_base;
	/*
	 * 1.if TX don't have HDCP22 key, skip RX hdcp22 ver.
	 * 2.note that during hdcp1.4 authentication, read hdcp version
	 * of connected TV set(capable of hdcp2.2) may cause TV
	 * switch its hdcp mode, and flash screen. should not
	 * read hdcp version of sink during hdcp1.4 authentication.
	 * if hdcp1.4 authentication currently, force return hdcp1.4
	 */
	if (tx_comm->hdcptx_comm.hdcp_mode == 0x1) {
		hdcprx_cap = HDCP_MODE14;
	} else if (dptx_hw_cntl(hw_base, DP_HDCP_22_LSTORE, tx_comm, NULL) &&
		dptx_hw_cntl(hw_base, DP_HDCP_22_GET_RX_VER, tx_comm, NULL)) {
		/* load hdcp22 key and rx support hdcp22 version */
		tx_comm->hdcptx_comm.dw_hdcp22_cap = 1;
		hdcprx_cap = HDCP_MODE22 | HDCP_MODE14;
	} else {
		/* must assume RX support HDCP14, otherwise affect 1A-03 */
		tx_comm->hdcptx_comm.dw_hdcp22_cap = 0;
		hdcprx_cap = HDCP_MODE14;
	}
	return hdcprx_cap;
}

/* bit1: hdcp22, bit0: hdcp1.4 */
unsigned int drm_dptx_common_get_tx_hdcp_cap(struct dptx_common *tx_comm)
{
	struct meson_tx_hw *hw_base = NULL;

	if (!tx_comm) {
		DPTX_ERROR("get_rx_version NULL tx_comm instance!");
		return 0;
	}
	hw_base = &tx_comm->hw_comm->hw_base;

	if (tx_comm->hdcptx_comm.hdcp_lstore < 0x10) {
		tx_comm->hdcptx_comm.hdcp_lstore = 0;
		if (dptx_hw_cntl(hw_base, DP_HDCP_14_LSTORE, tx_comm, NULL))
			tx_comm->hdcptx_comm.hdcp_lstore |= HDCP_MODE14;
		if (dptx_hw_cntl(hw_base, DP_HDCP_22_LSTORE, tx_comm, NULL))
			tx_comm->hdcptx_comm.hdcp_lstore |= HDCP_MODE22;
	}

	DPTX_DEBUG("dptx hdcp lstore: [%d]\n", tx_comm->hdcptx_comm.hdcp_lstore);
	return tx_comm->hdcptx_comm.hdcp_lstore & (HDCP_MODE14 | HDCP_MODE22);
}
EXPORT_SYMBOL(drm_dptx_common_get_tx_hdcp_cap);

/* bit1: hdcp22, bit0: hdcp1.4 */
unsigned int drm_dptx_common_get_rx_hdcp_cap(struct dptx_common *tx_comm)
{
	unsigned int hdcprx_cap = 0;

	if (!tx_comm) {
		DPTX_ERROR("drm get rx hdcp cap: NULL tx_comm instance!\n");
		return 0;
	}

	hdcprx_cap = dptx_hdcptx_get_rx_version(tx_comm);
	DPTX_DEBUG("dptx get rx hdcp [%d]\n", hdcprx_cap);

	return hdcprx_cap;
}
EXPORT_SYMBOL(drm_dptx_common_get_rx_hdcp_cap);

static void dptx_hdcp2msg_copy_to_fifo(struct dptx_hw_common *tx_comm, enum hdcp_msgid id)
{
	int i;
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	u8 tofifo[MAX_FIFO_SIZE] = {0};
	u32 len = 0;

	switch (id) {
	case AKE_SEND_CERT:
		len = AKESENDCERT_FIFO;
		for (i = 0; i < len; i++)
			tofifo[i] = tx_hdcp->akesendcert_fifo[i];
		break;
	case AKE_SEND_H_PRIME:
		len = H_PRIME_SIZE;
		for (i = 0; i < len; i++)
			tofifo[i] = tx_hdcp->h_prime_fifo[i];
		break;
	case AKE_SEND_PAIRING_INFO:
		len = EKHKM_SIZE;
		for (i = 0; i < len; i++)
			tofifo[i] = tx_hdcp->ekhkm_fifo[i];
		break;
	case REP_AUTH_SEND_RXID_LIST:
		len = RCVID_LIST_SIZE;
		for (i = 0; i < len; i++)
			tofifo[i] = tx_hdcp->receiverid_list[i];
		break;
	case REP_AUTH_STREAM_READY:
		len = STREAM_READY_SIZE;
		for (i = 0; i < len; i++)
			tofifo[i] = tx_hdcp->stream_ready[i];
		break;
	default:
		DPTX_INFO("wrong msgid copy to fifo\n");
		return;
	}

	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP2_RCV_FIFO, id);
	for (i = 0; i < len; i++)
		dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP2_RCV_FIFO, tofifo[i]);

	DPTX_INFO("HDCP2 write data to rcv fifo end\n");
}

static void dptx_hdcp2_hw_enable_write(struct dptx_hw_common *tx_comm, bool enable)
{
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_ENABLE, enable);
}

static void dptx_hdcp2_hw_mode_write(struct dptx_hw_common *tx_comm, u32  val)
{
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_MODE, val);
}

static void dptx_hdcp2_hw_repeater_write(struct dptx_hw_common *tx_comm, bool enable)
{
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_REPEATER, enable);
}

static void dptx_hdcp2_hw_stream_cipher_write(struct dptx_hw_common *tx_comm, bool enable)
{
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_STREAM_CIPHER_ENABLE, enable);
}

static void dptx_hdcp2_hw_aes_input_write(struct dptx_hw_common *tx_comm, u32 input_id)
{
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_AES_INPUT_SELECT, (input_id & 0x07));
}

static void dptx_hdcp2_hw_aes_ctr_reset(struct dptx_hw_common *tx_comm)
{
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_AES_COUNTER_RESET, 1);
}

static bool dptx_hdcp2_hw_lc128_write(struct dptx_hw_common *tx_comm, const u8 *lc128)
{
	u32 wr_val;

	//todo, if null return
	wr_val = lc128[3] << 24 | lc128[2] << 16 | lc128[1] << 8 | lc128[0];
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_LC128_31_0, wr_val);

	wr_val = lc128[7] << 24 | lc128[6] << 16 | lc128[5] << 8 | lc128[4];
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_LC128_63_32, wr_val);

	wr_val = lc128[11] << 24 | lc128[10] << 16 | lc128[9] << 8 | lc128[8];
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_LC128_95_64, wr_val);

	wr_val = lc128[15] << 24 | lc128[14] << 16 | lc128[13] << 8 | lc128[12];
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_LC128_127_96, wr_val);

	return true;
}

/* from spec: use fixed value */
static const u8 fixed_lc[128] = {0xB5, 0xD8, 0xE9, 0xAB, 0x5F, 0x8A, 0xFE, 0xCA,
				 0x38, 0x55, 0xB1, 0xA5, 0x1E, 0xC9, 0xBC, 0x0F};
static bool dptx_hdcp2_state_init(struct dptx_hw_common *tx_comm)
{
	/* hardware init */
	/* Disable HDCP to reset the internal state machines to the default state. */
	dptx_hdcp2_hw_enable_write(tx_comm, false);
	/* Select HDCP 2.x mode */
	dptx_hdcp2_hw_mode_write(tx_comm, 2);
	/* Enable (0x01) or disable (0x00) repeater mode as required */
	dptx_hdcp2_hw_repeater_write(tx_comm, false);
	/* Disable the stream cipher */
	dptx_hdcp2_hw_stream_cipher_write(tx_comm, false);
	/* AES input select set to cipher mode */
	dptx_hdcp2_hw_aes_input_write(tx_comm, true);
	/* Issue a reset to the AES counter */
	dptx_hdcp2_hw_aes_ctr_reset(tx_comm);
	/* Enable HDCP */
	dptx_hdcp2_hw_enable_write(tx_comm, true);
	/* Set the lc128 value */
	dptx_hdcp2_hw_lc128_write(tx_comm, fixed_lc);

	/* bit[2] lvp enc cfg;bit[1] use hw ks */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_CIPHER_CONTROL, (0x1 << 2) | (0x1 << 1));
	/* bit[7] ks_update_en;bit[1] snd_done_auto */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP2_CFG, 0x82);
	/* bit[0] hpd */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP2_HPD_CFG, 0x1);

	return true;
}

static void dptx_hdcp2_pa_core_init(struct dptx_hw_common *tx_comm)
{
	/* bit[7] clock gate */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_HDCP2X_DEBUG_CTRL2_IVCTX, 0x80);
	/* 0x8be  force HDP */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_HDCP2X_CTL_1_IVCTX, 0x6);
	/* 0x826  general purpose input ctrl*/
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_GPCTL_IVCTX, 0xdb);
	/* 0x810  eclk divider selection */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP0_IVCTX, 0x1);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP1_IVCTX, 0x92);
	/* restart wait time (default 1=12ms) */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP2_IVCTX, 0x1);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP3_IVCTX, 0x32);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP4_IVCTX, 0x14);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP5_IVCTX, 0x32);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP6_IVCTX, 0x16);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP7_IVCTX, 0xa);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP8_IVCTX, 0x9);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP9_IVCTX, 0x14);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP10_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP11_IVCTX, 0xc8);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP12_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP13_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP14_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TP15_IVCTX, 0x0);

	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_GP_IN0_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_GP_IN1_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_GP_IN2_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_GP_IN3_IVCTX, 0x0);

	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_CTRL_0_IVCTX, 0x9e);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_HDCP2X_POLL_VAL0_IVCTX, 0xa);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_INTR0_MASK_IVCTX, 0x3);

	/* 0x8bd  [0] reg_hdcp2x_en */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_HDCP2X_CTL_0_IVCTX, 0x1);
	/* 0x8bd  mux */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_GP0_IVCTX, 0x1);

	/* softreset */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_HDCP2X_TX_SRST_IVCTX, 0xff);
	/* release */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_HDCP2X_TX_SRST_IVCTX, 0x00);

	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_CTRL_1_IVCTX, 0xe1);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_CTRL_1_IVCTX, 0xe0);
}

static void dptx_irq_nb_init(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;

	init_completion(&tx_hdcp->sndfifo_completion);
	init_completion(&tx_hdcp->pairing_available_completion);
	init_completion(&tx_hdcp->h_available_completion);
	init_completion(&tx_hdcp->ready_completion);
}

static void dptx_hdcp22_init(struct dptx_hw_common *tx_comm)
{
	tx_comm->dp_hdcp = kzalloc(sizeof(*tx_comm->dp_hdcp), GFP_KERNEL);

	if (!tx_comm->dp_hdcp)
		DPTX_INFO("struct dptx_hdcp is null!!!\n");

	/* txcap must use the fixed value */
	tx_comm->dp_hdcp->txcap = 0x20000;

	dptx_irq_nb_init(tx_comm);
	dptx_hdcp2_state_init(tx_comm);
	dptx_hdcp2_pa_core_init(tx_comm);
	DPTX_INFO("%s hdcp22 init end\n", __func__);
}

ssize_t dptx_hdcp2_msg_read_rxcaps(struct dptx_hw_common *tx_comm)
{
	u32 rxcap = 0;
	int ret = 0;

	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_RX_CAPS_OFFSET,
				&rxcap, sizeof(rxcap));
	if (ret < 0)
		DPTX_ERROR("%s rxcap read ret:%d\n", __func__, ret);

	tx_comm->dp_hdcp->rxcap = rxcap;

	return ret;
}

static ssize_t dptx_hdcp2_msg_ake_init(struct dptx_hw_common *tx_comm)
{
	int ret = 0;

	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;

	if (tx_hdcp->snd_fifo[0] != AKE_INIT) {
		DPTX_ERROR("%s hdcp msg isn't AKE_INIT\n", __func__);
		return -EINVAL;
	}

	/* fifo need send start at byte1, and len - 1 */
	ret = dptx_aux_write_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_RTX_OFFSET,
				  &tx_hdcp->snd_fifo[1],
				  tx_hdcp->snd_fifo_len - 1);
	if (ret < 0)
		DPTX_INFO("AKE_INIT send fail");
	//todo len judgement sendfifo len and write success len

	return ret;
}

/* config dp hdcp hw type as 1 */
static void dptx_set_hw_hdcp2_type(struct dptx_hw_common *tx_comm, int val)
{
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_HDCP_RTX_63_32, val ? (1 << 24) : 0);
}

/* aux sned dp hdcp type as 1 */
static ssize_t dptx_set_aux_hdcp2_type(struct dptx_hw_common *tx_comm, int val)
{
	int ret = 0;
	u8 data = !!val;

	ret = dptx_aux_write_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_STREAM_TYPE_OFFSET, &data, 1);

	return ret;
}

static void dptx_hdcp2_msg_write_type_value(struct dptx_hw_common *tx_comm)
{
	int test_type = TYPE_TEST_PATTERN;
	int ret = 0;

	//todo bit0,1
	dptx_set_hw_hdcp2_type(tx_comm, test_type & 1);
	ret = dptx_set_aux_hdcp2_type(tx_comm, test_type & 2);
	if (ret < 0) {
		//todo abnormal handle
		//todo aux write fail
	}
}

/* within 100ms need can be read after AKE_Init */
static ssize_t dptx_hdcp2_msg_ake_send_cert(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	memset(tx_hdcp->akesendcert_fifo, 0, AKESENDCERT_FIFO);
	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_CERT_RX_OFFSET,
				tx_hdcp->akesendcert_fifo, AKESENDCERT_FIFO);
	if (ret < 0)
		DPTX_ERROR("%s read dpcd AKE_SEND_CERT ret:%d\n", __func__, ret);
	//todo len judgement
	dptx_hdcp2msg_copy_to_fifo(tx_comm, AKE_SEND_CERT);

	return ret;
}

static ssize_t dptx_hdcp2_msg_handle_stored_km(struct dptx_hw_common *tx_comm, u8 *type)
{
	u32 addr;
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	if (tx_hdcp->snd_fifo[0] == AKE_NO_STORED_EKM) {
		addr = DP_HDCP_2_2_REG_EKPUB_KM_OFFSET;
		*type = AKE_NO_STORED_EKM;
	} else if (tx_hdcp->snd_fifo[0] == AKE_STORED_EKM) {
		addr = DP_HDCP_2_2_REG_EKH_KM_WR_OFFSET;
		*type = AKE_STORED_EKM;
	} else {
		DPTX_INFO("AKE_NO_STORED_EKM or AKE_STORED_EKM msg sned fail\n");
		return -EINVAL;
	}

	ret = dptx_aux_write_dpcd(tx_comm->tx_aux, addr, &tx_hdcp->snd_fifo[1],
				tx_hdcp->snd_fifo_len - 1);

	if (ret < 0)
		DPTX_ERROR("%s write ret:%d\n", __func__, ret);
	//todo len judgement sendfifo len and write success len

	return ret;
}

static ssize_t dptx_hdcp2_msg_ake_send_h_prime(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	memset(tx_hdcp->h_prime_fifo, 0, H_PRIME_SIZE);
	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_HPRIME_OFFSET,
				 tx_hdcp->h_prime_fifo, H_PRIME_SIZE);
	if (ret < 0)
		DPTX_ERROR("%s read dpcd h_prime ret:%d\n", __func__, ret);
	//todo len judgement

	dptx_hdcp2msg_copy_to_fifo(tx_comm, AKE_SEND_H_PRIME);

	return ret;
}

static ssize_t dptx_hdcp2_msg_ake_read_pairing_info(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	memset(tx_hdcp->ekhkm_fifo, 0, EKHKM_SIZE);
	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_EKH_KM_RD_OFFSET,
				 tx_hdcp->ekhkm_fifo, EKHKM_SIZE);
	if (ret < 0)
		DPTX_ERROR("%s read dpcd ekhm_fifo ret:%d\n", __func__, ret);
	//todo len judgement

	dptx_hdcp2msg_copy_to_fifo(tx_comm, AKE_SEND_PAIRING_INFO);

	return ret;
}

static ssize_t dptx_hdcp2_msg_lc_init(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	if (tx_hdcp->snd_fifo[0] != LC_INIT) {
		DPTX_ERROR("%s hdcp msg isn't LC_INIT\n", __func__);
		return -EINVAL;
	}

	ret = dptx_aux_write_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_RN_OFFSET,
				  &tx_hdcp->snd_fifo[1], tx_hdcp->snd_fifo_len - 1);
	if (ret < 0)
		DPTX_ERROR("%s write hdcp msg LC_INIT ret:%d\n", __func__, ret);
	//todo len judgement

	return ret;
}

//todo: timer
static void hdcp_timer_block_wait_ms(u32 ms)
{
	msleep_interruptible(ms);
}

static ssize_t dptx_hdcp2_msg_lc_read_l_prime(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	memset(tx_hdcp->lprime_fifo, 0, LPRIME_SIZE);
	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_LPRIME_OFFSET,
				 tx_hdcp->lprime_fifo, LPRIME_SIZE);
	if (ret < 0)
		DPTX_ERROR("%s read dpcd l_prime ret:%d\n", __func__, ret);
	//todo len judgement

	dptx_hdcp2msg_copy_to_fifo(tx_comm, LC_SEND_L_PRIME);

	return ret;
}

static ssize_t dptx_hdcp2_msg_ske_send_eks(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	if (tx_hdcp->snd_fifo[0] != SKE_SEND_EKS) {
		DPTX_ERROR("%s hdcp msg isn't SKE_SEND_EKS\n", __func__);
		return -EINVAL;
	}

	ret = dptx_aux_write_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_EDKEY_KS_OFFSET,
					&tx_hdcp->snd_fifo[1],
					tx_hdcp->snd_fifo_len - 1);
	if (ret < 0)
		DPTX_ERROR("%s write dpcd SKE_SEND_EKS ret:%d\n", __func__, ret);
	//todo len judgement

	return ret;
}

//todo flow need double confirm
void dptx_hdcp2_stream_manage_process(struct dptx_hw_common *tx_comm)
{
	//todo mst_en, now only have sst mode
	DPTX_INFO("HDCP22 Repeater : SST enable,write 0x0 and Type to StreamID\n");
	/* k = 1 */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_RPT_SMNG_K_IVCTX, 0x01);
	/* VC PayloadID0 =0x00 */
	/* [4] Write start to clear rd_ptr */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0x10);
	/* Smng VC PayloadID */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_RPT_SMNG_IN_IVCTX, 0x00);
	/* [3] Write smng to buffer */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0x8);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0x0);
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0x0);
	/* Smng VC PayloadID */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_RPT_SMNG_IN_IVCTX, 0x00);
	/* [3] Write smng to buffer */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0x8);
	/* [3] Write smng to buffer */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DP_CP2TX_TX_CTRL_0_IVCTX, 0x0);
}

static ssize_t dptx_hdcp2_msg_repeaterauth_send_receiverid_list(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	memset(tx_hdcp->receiverid_list, 0, RCVID_LIST_SIZE);
	/* rxinfo, seq_num_v, v, receiver id list */
	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_RXINFO_OFFSET,
				 tx_hdcp->receiverid_list, RCVID_LIST_SIZE);
	if (ret < 0)
		DPTX_ERROR("%s read REP_AUTH_SEND_RXID_LIST fail\n", __func__);

	dptx_hdcp2msg_copy_to_fifo(tx_comm, REP_AUTH_SEND_RXID_LIST);
	//todo len judgement

	return ret;
}

static ssize_t dptx_hdcp2_msg_repeaterauth_send_ack(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	if (tx_hdcp->snd_fifo[0] != REP_AUTH_SEND_ACK) {
		DPTX_ERROR("%s hdcp msg isn't REP_AUTH_SEND_ACK\n", __func__);
		return -EINVAL;
	}

	ret = dptx_aux_write_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_V_OFFSET,
					&tx_hdcp->snd_fifo[1],
					tx_hdcp->snd_fifo_len - 1);
	if (ret < 0)
		DPTX_ERROR("%s write dpcd REP_AUTH_SEND_ACK ret:%d\n", __func__, ret);
	//todo len judgement

	return ret;
}

static ssize_t dptx_hdcp2_msg_repeaterauth_stream_manage(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	if (tx_hdcp->snd_fifo[0] != REP_AUTH_STREAM_MANAGE) {
		DPTX_ERROR("%s hdcp msg isn't REP_AUTH_STREAM_MANAGE\n", __func__);
		return -EINVAL;
	}

	ret = dptx_aux_write_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_SEQ_NUM_M_OFFSET,
					&tx_hdcp->snd_fifo[1],
					tx_hdcp->snd_fifo_len - 1);
	if (ret < 0)
		DPTX_ERROR("%s write dpcd REP_AUTH_STREAM_MANAGE ret:%d\n", __func__, ret);
	//todo len judgement

	return ret;
}

static ssize_t dptx_hdcp2_msg_repeaterauth_stream_ready(struct dptx_hw_common *tx_comm)
{
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	int ret = 0;

	memset(tx_hdcp->stream_ready, 0, STREAM_READY_SIZE);
	ret = dptx_aux_read_dpcd(tx_comm->tx_aux, DP_HDCP_2_2_REG_M_OFFSET,
				 tx_hdcp->stream_ready, STREAM_READY_SIZE);
	if (ret < 0)
		DPTX_ERROR("%s read REP_AUTH_STREAM_READY fail\n", __func__);
	//todo len judgement

	dptx_hdcp2msg_copy_to_fifo(tx_comm, REP_AUTH_STREAM_READY);

	return ret;
}

static void hdcp2_cipher_enable(struct dptx_hw_common *tx_comm)
{
	/* enable HDCP transmit */
	dptx_hdcp2_hw_enable_write(tx_comm, true);
	/* enable stream cipher */
	dptx_hdcp2_hw_stream_cipher_write(tx_comm, true);
	/* reset scrambler */
	dptx20_reg_write(tx_comm, CORE_LEVEL, DPTX20_FORCE_SCRAMBLER_RESET, 0x1);
}

void dptx_hdcp2_sndfifo_intr_handler(struct dptx_hw_common *tx_comm)
{
	int i;
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;

	memset(tx_hdcp->snd_fifo, 0, MAX_FIFO_SIZE);
	//todo define 16/0x3ff
	tx_hdcp->snd_fifo_len =
		(dptx20_reg_read(tx_comm, CORE_LEVEL, DPTX20_HDCP2_FIFO_STATUS) >> 16) & 0x3ff;
	for (i = 0; (i < tx_hdcp->snd_fifo_len) && (i < MAX_FIFO_SIZE); i++)
		tx_hdcp->snd_fifo[i] = dptx20_reg_read(tx_comm, CORE_LEVEL, DPTX20_HDCP2_SND_FIFO);

	complete(&tx_hdcp->sndfifo_completion);
}

static void dptx_hdcp2_process(struct dptx_hw_common *tx_comm)
{
	int ret = 0;
	struct dptx_hdcp *tx_hdcp = tx_comm->dp_hdcp;
	u8 store_type = 0;
	int stat = 0;

	ret = dptx_hdcp2_msg_read_rxcaps(tx_comm);
	if (ret < 0) {
		//todo aux read fail
	} else {
		DPTX_INFO("RxCaps[0] = 0x%x\n", tx_hdcp->rxcap & 0xff);
		DPTX_INFO("RxCaps[1] = 0x%x\n", (tx_hdcp->rxcap >> 8) & 0xff);
		DPTX_INFO("RxCaps[2] = 0x%x\n", (tx_hdcp->rxcap >> 16) & 0xff);
	}

	if ((((tx_hdcp->rxcap >> 16) & 0xff) & 0x2) != 0x2) {
		DPTX_ERROR("HDCP_CAPABLE = 0, RX not support hdcp22!!!\n");
		return;
	}

	stat = wait_for_completion_timeout(&tx_hdcp->sndfifo_completion,
		msecs_to_jiffies(200));
	if (stat == 0) {
		//todo abnormal handle
		DPTX_INFO("wait hdcp2 AKE_INIT msg time out\n");
	} else {
		ret = dptx_hdcp2_msg_ake_init(tx_comm);
		if (ret < 0) {
			//todo abnormal handle
			//todo aux write fail
		}
	}

	/* hw config */
	dptx_hdcp2_msg_write_type_value(tx_comm);

	msleep_interruptible(100);
	/* for AKE_NO_STORED_EKM and AKE_STORED_EKM */
	reinit_completion(&tx_hdcp->sndfifo_completion);
	/* read AKE_SEND_CERT */
	ret = dptx_hdcp2_msg_ake_send_cert(tx_comm);
	if (ret < 0) {
		//todo abnormal handle
		//todo aux read fail
	}

	stat = wait_for_completion_timeout(&tx_hdcp->sndfifo_completion,
			msecs_to_jiffies(200));
	/* for H_AVAILABLE */
	reinit_completion(&tx_hdcp->h_available_completion);
	if (stat == 0) {
		//todo abnormal handle
		DPTX_INFO("wait hdcp2 AKE_NO_STORED_EKM or AKE_STORED_EKM msg time out\n");
	} else {
		ret = dptx_hdcp2_msg_handle_stored_km(tx_comm, &store_type);
		if (ret < 0) {
			//todo abnormal handle
			//todo aux write fail
		}
	}

	/* wait irq 1s for store_km; 200ms for no_store_km */
	DPTX_INFO("Wait CP_IRQ to read H prime\n");
	if (store_type == AKE_NO_STORED_EKM) {
		stat = wait_for_completion_timeout(&tx_hdcp->h_available_completion,
			msecs_to_jiffies(200));
		/* for AKE_SEND_PAIRING_INFO */
		reinit_completion(&tx_hdcp->pairing_available_completion);
	} else if (store_type == AKE_STORED_EKM) {
		stat = wait_for_completion_timeout(&tx_hdcp->h_available_completion,
			msecs_to_jiffies(1000));
	} else {
		DPTX_INFO("wrong store_type for AKE_EKM\n");
	}
	/* for LC_INIT */
	reinit_completion(&tx_hdcp->sndfifo_completion);
	if (stat == 0) {
		//todo abnormal handle
		DPTX_INFO("wait rxstatus H_AVAILABLE time out\n");
	} else {
		ret = dptx_hdcp2_msg_ake_send_h_prime(tx_comm);
		if (ret < 0) {
			//todo abnormal handle
			//todo aux read fail
		}
	}

	if (store_type == AKE_NO_STORED_EKM) {
		stat = wait_for_completion_timeout(&tx_hdcp->pairing_available_completion,
			msecs_to_jiffies(200));
		if (stat == 0) {
			//todo abnormal handle
			DPTX_INFO("wait rxstatus PAIRING_AVAILABLE time out\n");
		} else {
			ret = dptx_hdcp2_msg_ake_read_pairing_info(tx_comm);
			if (ret < 0) {
				//todo abnormal handle
				//todo aux read fail
			}
		}
	}

	stat = wait_for_completion_timeout(&tx_hdcp->sndfifo_completion,
				msecs_to_jiffies(200));
	if (stat == 0) {
		//todo abnormal handle
		DPTX_INFO("wait hdcp2 LC_INIT msg time out\n");
	} else {
		ret = dptx_hdcp2_msg_lc_init(tx_comm);
		if (ret < 0) {
			//todo abnormal handle
			//todo aux write fail
		}
	}

	//todo: timer delay
	hdcp_timer_block_wait_ms(16);
	/* for SKE_SEND_EKS */
	reinit_completion(&tx_hdcp->sndfifo_completion);
	/* tx receiver LC_SEND_L_PRIME within 16ms from tx LC_INIT */
	ret = dptx_hdcp2_msg_lc_read_l_prime(tx_comm);
	if (ret < 0) {
		//todo abnormal handle
		//todo aux read fail
	}

	stat = wait_for_completion_timeout(&tx_hdcp->sndfifo_completion,
				msecs_to_jiffies(200));
	if (stat == 0) {
		//todo abnormal handle
		DPTX_INFO("wait hdcp2 SKE_SEND_EKS time out\n");
	} else {
		/* for REP_AUTH_SEND_RXID_LIST */
		reinit_completion(&tx_hdcp->ready_completion);
		ret = dptx_hdcp2_msg_ske_send_eks(tx_comm);
		if (ret < 0) {
			//todo abnormal handle
			//todo aux write fail
		}
	}

	if (tx_hdcp->rxcap & RXCAP_REPEATER) {
		dptx_hdcp2_stream_manage_process(tx_comm);

		stat = wait_for_completion_timeout(&tx_hdcp->ready_completion,
			msecs_to_jiffies(3000));
		/* for REP_AUTH_SEND_ACK */
		reinit_completion(&tx_hdcp->sndfifo_completion);
		if (stat == 0) {
			//todo abnormal handle
			DPTX_INFO("repeater REP_AUTH_SEND_RXID_LIST rxstatus ready timeout\n");
		} else {
			ret = dptx_hdcp2_msg_repeaterauth_send_receiverid_list(tx_comm);
			if (ret < 0) {
				//todo abnormal handle
				//todo aux read fail
			}
		}

		stat = wait_for_completion_timeout(&tx_hdcp->sndfifo_completion,
			msecs_to_jiffies(200));
		/* for REP_AUTH_STREAM_MANAGE */
		reinit_completion(&tx_hdcp->sndfifo_completion);
		if (stat == 0) {
			//todo abnormal handle
			DPTX_INFO("wait hdcp2 REP_AUTH_SEND_ACK msg time out\n");
		} else {
			ret = dptx_hdcp2_msg_repeaterauth_send_ack(tx_comm);
			if (ret < 0) {
				//todo abnormal handle
				//todo aux write fail
			}
		}

		stat = wait_for_completion_timeout(&tx_hdcp->sndfifo_completion,
			msecs_to_jiffies(200));
		if (stat == 0) {
			//todo abnormal handle
			DPTX_INFO("wait hdcp2 REP_AUTH_STREAM_MANAGE msg timeout\n");
		} else {
			ret = dptx_hdcp2_msg_repeaterauth_stream_manage(tx_comm);
			if (ret < 0) {
				//todo abnormal handle
				//todo aux write fail
			}
		}

		//todo: timer delay
		hdcp_timer_block_wait_ms(100);
		ret = dptx_hdcp2_msg_repeaterauth_stream_ready(tx_comm);
		if (ret < 0) {
			//todo abnormal handle
			//todo aux read fail
		}
	}

	hdcp_timer_block_wait_ms(200);
	hdcp2_cipher_enable(tx_comm);
}

void dptx_hdcp22_proc(struct dptx_hw_common *tx_comm)
{
	DPTX_INFO("%s start hdcp auth\n", __func__);
	dptx_hdcp22_init(tx_comm);
	dptx_hdcp2_process(tx_comm);
	DPTX_INFO("%s end hdcp auth\n", __func__);
}
