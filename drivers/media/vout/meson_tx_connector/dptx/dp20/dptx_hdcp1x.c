// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/amlogic/arm-smccc.h>
#include "dptx20_reg.h"
#include "dptx_log.h"
#include "dptx_aux_helper.h"
#include "dptx_hw.h"
#include "dptx_hdcp_hw.h"
#include "dpcd_reg.h"
#include "../../hdcp/hdmitx_hdcp.h"

#define DPTX_HDCP1X_WQ_NAME "dptx_hdcp1x_workqueue"
#define HDCP1X_MAX_DEV_CNT 127
#define BINFO_MAX_DEVS_EXCEEDED BIT(7)
#define BINFO_MAX_CASCADE_EXCEEDED BIT(11)

/* DP HDP1.1 Spec chapter2.2.1
 * r0' must be available for the HDCP Transmitter to read
 * within 100 milliseconds from the time that the HDCP
 * Transmitter finishes writing Aksv to the HDCP Receiver.
 * The HDCP Transmitter must not read the r0' value
 * sooner than 100ms after write AKSV;
 * add extra 10ms for error tolerance
 */
#define DP1X_R0_TIMEOUT_MS (100 + 10)
/* DP HDP1.1 Spec chapter2.2.2
 * The HDCP Transmitter, having determined that the REPEATER
 * bit read earlier in the protocol is set, sets a five-second
 * watchdog timer. It may either poll the HDCP Repeater's READY
 * status bit or alternatively check the READY bit when a CP_IRQ
 * interrupt is received. add extra 10ms for error tolerance
 */
#define DP1X_RPT_RDY_MS (5000 + 10)

bool dptx_hdcp1_get_result(struct hdcptx_common *hdcptx_comm, int hdcp_type)
{
	struct arm_smccc_res res;

	if (!hdcptx_comm)
		return 0;
	//TODO: use HDCP_VER_HDCP1X
	if (hdcp_type == 1) {
		arm_smccc_smc(HDCPTX_IOOPR, HDCP14_RESULT, 0, 0, 0, 0, 0, 0, &res);
		return res.a0 == 1;
	}

	DPTX_INFO("only dptx hdcp14 use the api get result\n");
	return 0;
}

/* Set specified HDCP mode. */
static void hdcp1_tx_hw_mode_set(struct dptx_hw_common *hw_comm, bool enable)
{
	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_MODE, enable ? 1 : 0);
}

/* Enable / disable HDCP. */
static void hdcp1_tx_hw_state_enable(struct dptx_hw_common *hw_comm, bool enable)
{
	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_ENABLE, enable ? 1 : 0);
}

/* Function: hdcp1_tx_state_init
 * Initialize HDCP transmit authentication state machine.
 * Returns:  None
 */
static void hdcp1_tx_state_init(struct dptx_hw_common *hw_comm, bool en)
{
	if (!hw_comm)
		return;

	/* hardware init */
	/* set mode to HDCP 1.x */
	hdcp1_tx_hw_mode_set(hw_comm, true);
	/* enable hdcp */
	hdcp1_tx_hw_state_enable(hw_comm, en);
	/* bit2 lvp enc */
	dptx20_reg_write(hw_comm, CORE_LEVEL,
		DPTX20_HDCP_CIPHER_CONTROL, (en << 2) | (en << 0));
}

/* HDCP stack initialization function */
static void hdcp1_tx_stack_init(struct dptx_hw_common *hw_comm, bool en)
{
	/* Initialize HDCP 1.x transmitter (source) state machine */
	hdcp1_tx_state_init(hw_comm, en);
}

/* This function reads the HDCP 1.x bcaps register.
 *
 *  Register            DPCD        size
 *  DP_AUX_HDCP_BCAPS   0x68028     1
 *
 *  bcaps has the following bits:
 *  - TR_DP_HDCP1_BCAPS_HDCP
 *  - TR_DP_DP_BCAPS_REPEATER_PRESENT
 */
static int hdcp1_tx_dpcd_rd_bcaps(struct dptx_hw_common *hw_comm)
{
	int ret = 0;
	size_t len = 1;

	ret = dptx_aux_read_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_BCAPS,
		&hw_comm->dptx_hdcp1x_inst->bcaps, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdcp1.x read bcaps ret:%d\n", ret);
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}

/* Function: hdcp1_hw_tx_gen_an
 * Generate a 64-bit value for An.
 * Returns: None
 */
static void hdcp1_tx_hw_gen_an(struct dptx_hw_common *hw_comm)
{
	int i = 0;
	u32 an_data1 = 0;
	u32 an_data2 = 0;

	/* load An registers */
	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_RNG_CIPHER_STORE_AN, 1);
	usleep_range(10, 20);

	an_data1 = dptx20_reg_read(hw_comm, CORE_LEVEL, DPTX20_HDCP_RNG_CIPHER_AN_31_0);
	for (i = 0; i < 4; i++)
		hw_comm->dptx_hdcp1x_inst->an[i] = (an_data1 >> (i * 8)) & 0xFF;
	an_data2 = dptx20_reg_read(hw_comm, CORE_LEVEL, DPTX20_HDCP_RNG_CIPHER_AN_63_32);
	for (i = 0; i < 4; i++)
		hw_comm->dptx_hdcp1x_inst->an[i + 4] = (an_data2 >> (i * 8)) & 0xFF;

	/* load An registers */
	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_AN_31_0, an_data1);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_AN_63_32, an_data2);
}

/* Function: hdcp1_tx_dpcd_rd_bksv
 * This function reads the HDCP 1.x bksv register.
 *
 * Register            DPCD        size
 * DP_AUX_HDCP_BKSV    0x68000     5
 */
static int hdcp1_tx_dpcd_rd_bksv(struct dptx_hw_common *hw_comm)
{
	int ret = 0;
	size_t len = ARRAY_SIZE(hw_comm->dptx_hdcp1x_inst->bksv);

	ret = dptx_aux_read_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_BKSV,
		hw_comm->dptx_hdcp1x_inst->bksv, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdp1.x read bksv ret:%d\n", ret);
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}

/* Function: hdcp1_tx_key_test_ksv
 * Verify KSV.
 *
 * Parameters: ksv to test
 *
 * Returns: true if bit count is 20 / false if not
 */
static bool hdcp1_tx_key_test_ksv(u8 *ksv_data)
{
	int i, ones = 0;
	u8 bksv[5] = {0};

	if (!ksv_data)
		return false;

	memcpy(bksv, ksv_data, sizeof(bksv));

	for (i = 0; i < 5; i++) {
		while (bksv[i] != 0) {
			if (bksv[i] & 0x1)
				ones++;
			bksv[i] >>= 1;
		}
	}
	return ones == 20;
}

/* Function: hdcp1_hw_tx_write_bksv
 * Store Bksv in the HDCP transmitter core.
 * Hardware Km is generated when upper value is written.
 */
static void hdcp1_tx_hw_write_bksv(struct dptx_hw_common *hw_comm)
{
	u32 lower = hw_comm->dptx_hdcp1x_inst->bksv[0] |
		hw_comm->dptx_hdcp1x_inst->bksv[1] << 8 |
		hw_comm->dptx_hdcp1x_inst->bksv[2] << 16 |
		hw_comm->dptx_hdcp1x_inst->bksv[3] << 24;
	u32 upper = hw_comm->dptx_hdcp1x_inst->bksv[4];

	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_BKSV_31_0, lower);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_BKSV_63_32, upper);
}

/* Function: hdcp1_tx_a1_bksv_process
 * check bksv valid and write into HW
 *
 * return 0 if bksv valid, otherwise return -1
 */
static int hdcp1_tx_a1_bksv_process(struct dptx_hw_common *hw_comm)
{
	bool bksv_valid = false;
	int ret = 0;

	bksv_valid = hdcp1_tx_key_test_ksv(hw_comm->dptx_hdcp1x_inst->bksv);
	if (bksv_valid) {
		if (hw_comm->dptx_hdcp1x_inst->bcaps & DP_BCAPS_REPEATER_PRESENT)
			dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_REPEATER, 0x1);
		/* write bksv into the hardware */
		hdcp1_tx_hw_write_bksv(hw_comm);
		ret = 0;
	} else {
		DPTX_ERROR("dptx hdp1.x BKSV(0x%2x%2x%2x%2x%2x) error\n",
			hw_comm->dptx_hdcp1x_inst->bksv[0],
			hw_comm->dptx_hdcp1x_inst->bksv[1],
			hw_comm->dptx_hdcp1x_inst->bksv[2],
			hw_comm->dptx_hdcp1x_inst->bksv[3],
			hw_comm->dptx_hdcp1x_inst->bksv[4]);
		ret = -1;
	}

	return ret;
}

/* Function: hdcp1_hw_tx_read_r0
 * Read r0 from the transmitter hardware
 *
 * Returns: true on success, false on failure
 */
static bool hdcp1_tx_hw_read_r0(struct dptx_hw_common *hw_comm)
{
	bool status;
	u32 r0 = dptx20_reg_read(hw_comm, CORE_LEVEL, DPTX20_HDCP_R0_STATUS);

	if (r0 & 0x10000) {
		hw_comm->dptx_hdcp1x_inst->r0 = r0 & 0xffffU;
		status = true;
	} else {
		status = false;
	}

	return status;
}

/* Function: hdcp1_tx_key_get_aksv
 * Transmitter key selection vector read from
 * the attached hardware key memory when the
 * dedicated hardware Km calculation is enabled.
 */
static void hdcp1_tx_key_get_aksv(struct dptx_hw_common *hw_comm)
{
	u8 i = 0;
	u32 aksv_data1 = 0;
	u32 aksv_data2 = 0;

	aksv_data1 = dptx20_reg_read(hw_comm, CORE_LEVEL, DPTX20_HDCP_AKSV_31_0);
	aksv_data2 = dptx20_reg_read(hw_comm, CORE_LEVEL, DPTX20_HDCP_AKSV_63_32);
	DPTX_DEBUG_HDCP("dptx hdp1.x HW AKSV: 0x%x%x\n", aksv_data1, aksv_data2);

	for (i = 0; i < 4; i++)
		hw_comm->dptx_hdcp1x_inst->aksv[i] = aksv_data1 >> (i * 8);
	hw_comm->dptx_hdcp1x_inst->aksv[4] = aksv_data2 & 0xFF;
}

/* Function: hdcp1_tx_dpcd_wr_ainfo
 * This function writes the HDCP 1.x ainfo register.
 *
 * Register            DPCD        size
 * DP_AUX_HDCP_AINFO   0x6803B     1
 */
static int hdcp1_tx_dpcd_wr_ainfo(struct dptx_hw_common *hw_comm)
{
	int ret = 0;
	int len = 1;

	ret = dptx_aux_write_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_AINFO,
		&hw_comm->dptx_hdcp1x_inst->ainfo, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdp1.x write ainfo ret:%d\n", ret);
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}

/* Function: hdcp1_tx_a1_wr_ainfo_enter
 * In this state, Ainfo is sent to the receiver
 * if the DPCD version is 1.2 or greater and the
 * Bcaps.REPEATER bit is set
 */
static int hdcp1_tx_a1_wr_ainfo_enter(struct dptx_hw_common *hw_comm)
{
	int ret = 0;

	/* set REAUTHENTICATION_ENABLE_IRQ_HPD */
	hw_comm->dptx_hdcp1x_inst->ainfo = 1;
	/* write ainfo DPCD register */
	ret = hdcp1_tx_dpcd_wr_ainfo(hw_comm);

	return ret;
}

/* Function: hdcp1_tx_dpcd_auth_init
 *
 * This function writes the HDCP 1.x An
 *
 * Register            DPCD        size
 * DPCD_HDCP1_AKSV     0x68007     5
 * DPCD_HDCP1_AN       0x6800C     8
 */
static int hdcp1_tx_dpcd_wr_an(struct dptx_hw_common *hw_comm)
{
	int ret = 0;
	int len = ARRAY_SIZE(hw_comm->dptx_hdcp1x_inst->an);

	ret = dptx_aux_write_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_AN,
		hw_comm->dptx_hdcp1x_inst->an, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdp1.x write an ret:%d\n", ret);
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}

static int hdcp1_tx_dpcd_wr_aksv(struct dptx_hw_common *hw_comm)
{
	int ret = 0;
	int len = ARRAY_SIZE(hw_comm->dptx_hdcp1x_inst->aksv);

	ret = dptx_aux_write_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_AKSV,
		hw_comm->dptx_hdcp1x_inst->aksv, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdp1.x write aksv ret:%d\n", ret);
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}

/* Function: hdcp1_tx_dpcd_rd_r0_prime
 *
 * This function reads the HDCP 1.x Read r0 prime register.
 *
 * Register            DPCD        size
 * DPCD_HDCP1_ROPRIME  0x68005     2
 *
 * Returns:
 * 0 - aux-channel transaction was started successfully
 * -1 - aux-channel transaction did not successfully start
 */
static int hdcp1_tx_dpcd_rd_r0_prime(struct dptx_hw_common *hw_comm)
{
	u8 r0_prime[2] = {0};
	int ret = 0;
	size_t len = ARRAY_SIZE(r0_prime);

	ret = dptx_aux_read_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_RI_PRIME,
		r0_prime, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdp1.x read r0' prime ret:%d\n", ret);
		ret = -1;
		hw_comm->dptx_hdcp1x_inst->r0_prime = 0;
	} else {
		ret = 0;
		hw_comm->dptx_hdcp1x_inst->r0_prime = r0_prime[1] << 8 | r0_prime[0];
	}
	return ret;
}

/* Function: hdcp1_tx_dpcd_rd_binfo
 *
 * This function reads the HDCP 1.x binfo register.
 *
 * Register            DPCD        size
 * DPCD_HDCP1_BINFO    0x6802A     2
 *
 * binfo has the following bits:
 * - TR_DP_HDCP_BINFO_DEVICE_COUNT
 * - TR_DP_HDCP_BINFO_MAX_DEVS_EXCEEDED
 * - TR_DP_HDCP_BINFO_DEPTH
 * - TR_DP_HDCP_BINFO_MAX_CASCADE_EXCEEDED
 * Returns:
 * true - aux-channel transaction was started successfully
 * false - aux-channel transaction did not successfully
 */
static int hdcp1_tx_dpcd_rd_binfo(struct dptx_hw_common *hw_comm)
{
	u8 binfo[2] = {0};
	int ret = 0;
	size_t len = ARRAY_SIZE(binfo);

	ret = dptx_aux_read_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_BINFO,
		binfo, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdp1.x read binfo ret:%d\n", ret);
		ret = -1;
		hw_comm->dptx_hdcp1x_inst->binfo = 0;
	} else {
		ret = 0;
		hw_comm->dptx_hdcp1x_inst->binfo = binfo[1] << 8 | binfo[0];
	}
	return ret;
}

/* Function: hdcp1_hw_tx_encrypt_enable
 * Enable or disable the stream cipher. DPTx core will control
 * encryption process automatically, based on this setting.
 *
 * Parameters:
 * enable - true=enable / false=disable
 *
 * Returns: None
 */
static void hdcp1_tx_hw_encrypt_enable(struct dptx_hw_common *hw_comm, bool enable)
{
	if (enable) {
		dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_STREAM_CIPHER_ENABLE, 1);
		dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_FORCE_SCRAMBLER_RESET, 1);
	} else {
		dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_STREAM_CIPHER_ENABLE, 0);
	}
}

/* Function: hdcp1_tx_dpcd_rd_ksv_fifo
 *
 * This function reads the HDCP 1.x KSV FIFO. The entire FIFO is read.
 *
 * Register                DPCD        size
 * DPCD_HDCP1_KSV_FIFO     0x6802C     15
 */
static int hdcp1_tx_dpcd_rd_ksv_fifo(struct dptx_hw_common *hw_comm)
{
	int ret = 0;
	size_t len = 15;
	u8 dev_count = hw_comm->dptx_hdcp1x_inst->binfo & 0x7F;
	u32 total_ksv_len = dev_count * 5;
	u32 read_ksv_len = 0;

	/* per chapter2.6.1 of hdcp1.1 on display port, the KSV_FIFO will
	 * auto increment after 15 bytes read successfully
	 */
	while (read_ksv_len < total_ksv_len) {
		ret = dptx_aux_read_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_KSV_FIFO,
			hw_comm->dptx_hdcp1x_inst->ksv_lists, len);
		if (ret < 0) {
			DPTX_ERROR("dptx hdp1.x read ksv fifo ret:%d\n", ret);
			ret = -1;
			break;
		}
		read_ksv_len += ret;
		ret = 0;
	}
	if (ret == 0) {
		if (read_ksv_len != total_ksv_len)
			DPTX_ERROR("dptx hdp1.x total_ksv_len(%d) and read_ksv_len(%d) not match\n",
				total_ksv_len, read_ksv_len);
	}

	return ret;
}

static void hdcp1_tx_ksv_lists_process(struct dptx_hw_common *hw_comm)
{
	int idx = 0;

	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_BINFO, hw_comm->dptx_hdcp1x_inst->binfo);
	dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_SHA_START, 0x1);

	for (idx = 0; idx < 5 * (hw_comm->dptx_hdcp1x_inst->binfo & 0x7f); idx++) {
		dptx20_reg_write(hw_comm, CORE_LEVEL, DPTX20_HDCP_KSV_FIFO,
			hw_comm->dptx_hdcp1x_inst->ksv_lists[idx]);
		usleep_range(10, 20);
	}
}

/* Function: hdcp1_tx_dpcd_rd_v_prime
 *
 * This function reads the HDCP 1.x v-prime value.
 * Register                DPCD        size
 * DPCD_HDCP1_VPRIME_H0    0x68014     20
 *
 * Returns:
 * 0 - aux-channel transaction was started successfully
 * -1 - aux-channel transaction did not successfully
 */
static int hdcp1_tx_dpcd_rd_v_prime(struct dptx_hw_common *hw_comm)
{
	int ret = 0;
	size_t len = ARRAY_SIZE(hw_comm->dptx_hdcp1x_inst->blk_v_prime);

	ret = dptx_aux_read_dpcd(hw_comm->tx_aux, DP_AUX_HDCP_V_PRIME(0),
		hw_comm->dptx_hdcp1x_inst->blk_v_prime, len);
	if (ret != len) {
		DPTX_ERROR("dptx hdp1.x read V prime ret:%d\n", ret);
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}

/* get V from HW */
static int hdcp1_tx_get_rptr_v(struct dptx_hw_common *hw_comm)
{
	u32 blk_v;
	u32 v_ready;
	u8 idx;
	u8 vdx;
	unsigned long internal_sha_timeout = 0;

	internal_sha_timeout = jiffies +
		msecs_to_jiffies(hw_comm->dptx_hdcp1x_inst->internal_sha_timeout_ms);
	while (time_before(jiffies, internal_sha_timeout)) {
		v_ready = dptx20_reg_read(hw_comm, CORE_LEVEL, DPTX20_HDCP_SHA_STATUS) & 0x1;
		if (v_ready)
			break;
	}
	if (v_ready == 0)
		return -1;

	DPTX_DEBUG_HDCP("DPTX HDCP1X RPTR V ready Now\n");
	for (idx = 0; idx < 5; idx++) {
		blk_v = dptx20_reg_read(hw_comm, CORE_LEVEL, DPTX20_HDCP_RPT_VH0 + idx * 4);
		for (vdx = 0; vdx < 4; vdx++)
			hw_comm->dptx_hdcp1x_inst->blk_v[idx * 4 + vdx] =
				(blk_v >> (vdx * 8)) & 0xFF;
	}
	return 0;
}

static bool compare_v_and_v_prime(struct dptx_hw_common *hw_comm)
{
	u8 idx;
	bool ret = true;

	for (idx = 0; idx < ARRAY_SIZE(hw_comm->dptx_hdcp1x_inst->blk_v); idx++) {
		if (hw_comm->dptx_hdcp1x_inst->blk_v[idx] !=
			hw_comm->dptx_hdcp1x_inst->blk_v_prime[idx]) {
			ret = false;
			break;
		}
	}
	return ret;
}

/* process for hdcp1x repeater */
static int dptx_hdcp1x_rpt_proc(struct dptx_hw_common *hw_comm)
{
	int ret = -1;
	u8 i = 0;
	/* DP HDCP1.1 spec chapter 2.2.2:
	 * If V is not equal to V', the HDCP Transmitter must re-read
	 * the KSV list, Binfo and V two additional times (for a total
	 * of three consecutive V' checks) to account for the possibility
	 * of link errors. The authentication protocol is aborted on
	 * three consecutive mismatches between V and V' and
	 * authentication can be reattempted with the transmission of
	 * new An and the Aksv.
	 */
	u8 v_retry_cnt = 3;
	bool v_match = false;

	if (!hw_comm || !hw_comm->dptx_hdcp1x_inst) {
		DPTX_ERROR("null hw_comm or dptx_hdcp1x_inst\n");
		return -1;
	}

	DPTX_DEBUG_HDCP("DPTX HDCP1.x read ksvlist after R0' Prime\n");

	for (i = 0; i < v_retry_cnt; i++) {
		ret = hdcp1_tx_dpcd_rd_binfo(hw_comm);
		if (ret < 0)
			continue;

		ret = hdcp1_tx_dpcd_rd_ksv_fifo(hw_comm);
		if (ret < 0)
			continue;

		hdcp1_tx_ksv_lists_process(hw_comm);
		ret = hdcp1_tx_dpcd_rd_v_prime(hw_comm);
		if (ret < 0)
			continue;

		ret = hdcp1_tx_get_rptr_v(hw_comm);
		if (ret < 0)
			continue;

		if (compare_v_and_v_prime(hw_comm)) {
			v_match = true;
			DPTX_DEBUG_HDCP("DPTX HDCP1X Check RPTR V Prime Pass\n");
			break;
		}
		v_match = false;
		DPTX_ERROR("DPTX HDCP1X Check RPTR V Prime FAIL\n");
		continue;
	}

	if (!v_match) {
		hdcp1_tx_hw_encrypt_enable(hw_comm, false);
		queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		ret = -1;
	} else if (hw_comm->dptx_hdcp1x_inst->binfo & BINFO_MAX_DEVS_EXCEEDED ||
		hw_comm->dptx_hdcp1x_inst->binfo & BINFO_MAX_CASCADE_EXCEEDED) {
		/* DP hdcpv1.0 1B-06/1B-07:
		 * auth fail, disable hdcp encryption and abort auth
		 */
		hdcp1_tx_hw_encrypt_enable(hw_comm, false);
		ret = -1;
	} else {
		ret = 0;
	}
	return ret;
}

static void dptx_hdcp1x_proc(struct dptx_hw_common *hw_comm)
{
	int ret;
	unsigned long intenal_r0_timeout = 0;
	bool internal_r0_ready = false;
	/* TODO: assume downstream hdcp support dpcd revision 1.2 or highr */
	bool dpcd_rev_12 = true;

	if (!hw_comm || !hw_comm->dptx_hdcp1x_inst)
		return;

	if (!hw_comm->dptx_hdcp1x_inst->hdcp1x_en) {
		DPTX_ERROR("DPTX HDCP1x not enabled!\n");
		return;
	}

	ret = hdcp1_tx_dpcd_rd_bcaps(hw_comm);
	if (ret < 0) {
		queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		return;
	} else if ((hw_comm->dptx_hdcp1x_inst->bcaps & DP_BCAPS_HDCP_CAPABLE) !=
		DP_BCAPS_HDCP_CAPABLE) {
		DPTX_ERROR("DPRX HDCP1x not capable!\n");
		return;
	}
	hdcp1_tx_stack_init(hw_comm, true);
	hdcp1_tx_hw_gen_an(hw_comm);
	ret = hdcp1_tx_dpcd_rd_bksv(hw_comm);
	if (ret < 0) {
		queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		return;
	}
	ret = hdcp1_tx_a1_bksv_process(hw_comm);
	if (ret < 0) {
		queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		return;
	}

	intenal_r0_timeout = jiffies +
		msecs_to_jiffies(hw_comm->dptx_hdcp1x_inst->intenal_r0_timeout_ms);
	while (time_before(jiffies, intenal_r0_timeout)) {
		internal_r0_ready = hdcp1_tx_hw_read_r0(hw_comm);
		if (internal_r0_ready)
			break;
	}
	if (!internal_r0_ready) {
		DPTX_ERROR("DPTX HDCP1x internal r0 not ready\n");
		queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		return;
	}

	hdcp1_tx_key_get_aksv(hw_comm);
	/* DP HDCP1.1 spec chapter 2.2.1:
	 * After the HDCP Transmitter has written An and Aksv and read
	 * the receiver's Bksv and the REPEATER bit, it sets the
	 * REAUTHENTICATION_ENABLE_IRQ_HPD bit in the Ainfo register
	 * if the REPEATER bit is set and the transmitter determines
	 * that the downstream HDCP Repeater supports DPCD Revision
	 * 1.2 or higher.
	 */
	if ((hw_comm->dptx_hdcp1x_inst->bcaps & DP_BCAPS_REPEATER_PRESENT) &&
		dpcd_rev_12) {
		ret = hdcp1_tx_a1_wr_ainfo_enter(hw_comm);
		if (ret < 0) {
			queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
			return;
		}
	}
	ret = hdcp1_tx_dpcd_wr_an(hw_comm);
	if (ret < 0) {
		queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		return;
	}

	ret = hdcp1_tx_dpcd_wr_aksv(hw_comm);
	if (ret < 0) {
		queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
			&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		return;
	}

	/* wait for R0' ready */
	queue_delayed_work(hw_comm->dptx_hdcp1x_inst->hdcp_wq,
		&hw_comm->dptx_hdcp1x_inst->hdcp1x_r0_timeout_work,
		msecs_to_jiffies(hw_comm->dptx_hdcp1x_inst->rx_r0_timeout_ms));
}

static void dptx20_hdcp1x_reauth_whandler(struct work_struct *w)
{
	struct dptx_hdcp1x *dptx_hdcp1x_inst = container_of((struct delayed_work *)w,
		struct dptx_hdcp1x, hdcp1x_reauth_work);

	if (!dptx_hdcp1x_inst) {
		DPTX_ERROR("null dptx_hdcp1x_inst\n");
		return;
	}
	dptx_hdcp1x_inst->auth_result = false;
	if (!dptx_hdcp1x_inst->hdcp1x_en)
		return;
	/* disable encryption */
	hdcp1_tx_hw_encrypt_enable(dptx_hdcp1x_inst->bind_hw_comm, false);
	/* restart hdcp1x auth */
	dptx_hdcp1x_proc(dptx_hdcp1x_inst->bind_hw_comm);
}

static void dptx20_hdcp1x_r0_whandler(struct work_struct *w)
{
	int ret = 0;
	bool hdcp1_tx_repeater_auth_done = 0;
	/* DP HDCP1.1 spec chapter 2.2.1:
	 * If there's a mismatch between R0 and R0', the HDCP transmitter
	 * must re-read R0' for comparison against R0 two additional times(
	 * for a total of three consecutive comparisons) in order to account
	 * for the possibility of link errors. The authentication protocol is
	 * deemed to have failed on three consecutive mismatches between R0 and
	 * R0'. Authentication can be reattempted with the transmission of new
	 * An and Aksv on failure of the first part of authentication.
	 */
	u8 r0_retry_cnt = 3;
	u8 i = 0;
	bool r0_match = false;
	bool work_canceled = false;
	struct dptx_hdcp1x *dptx_hdcp1x_inst = container_of((struct delayed_work *)w,
		struct dptx_hdcp1x, hdcp1x_r0_work);

	if (!dptx_hdcp1x_inst) {
		DPTX_ERROR("null dptx_hdcp1x_inst\n");
		return;
	}
	if (!dptx_hdcp1x_inst->hdcp1x_en)
		return;
	work_canceled = cancel_delayed_work(&dptx_hdcp1x_inst->hdcp1x_r0_timeout_work);
	if (!work_canceled) {
		/* r0 handler and r0_timeout handler come together,
		 * should not go into this case(as we add error
		 * tolerance in timeout period). However if this
		 * abnormal case happens, exit r0 handler and
		 * continue timeout retry
		 */
		DPTX_INFO("warning: r0_timeout work in progress, continue timeout work\n");
		return;
	}
	/* read R0' from DP RX and check if match */
	for (i = 0; i < r0_retry_cnt; i++) {
		ret = hdcp1_tx_dpcd_rd_r0_prime(dptx_hdcp1x_inst->bind_hw_comm);
		if (ret < 0)
			continue;

		if (dptx_hdcp1x_inst->r0 != dptx_hdcp1x_inst->r0_prime) {
			r0_match = false;
			continue;
		} else {
			r0_match = true;
			break;
		}
	}
	if (!r0_match) {
		DPTX_ERROR("DPTX HDCP1X Check R0' Prime fail!\n");
		queue_delayed_work(dptx_hdcp1x_inst->hdcp_wq,
			&dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
		return;
	}

	DPTX_DEBUG_HDCP("DPTX HDCP1x Check R0' Prime pass\n");
	/* enable encryption
	 * DP HDCP1.1 spec chapter 2.2.1:
	 * The hdcp transmitter enables HDCP Encryption when the first
	 * part of the authentication protocol successfully completes.
	 */
	hdcp1_tx_hw_encrypt_enable(dptx_hdcp1x_inst->bind_hw_comm, true);

	/* for non-repater case, just return */
	if ((dptx_hdcp1x_inst->bcaps & DP_BCAPS_REPEATER_PRESENT) !=
		DP_BCAPS_REPEATER_PRESENT) {
		dptx_hdcp1x_inst->auth_result = true;
		return;
	}

	/* below for hdcp1.x repeater case */
	if (dptx_hdcp1x_inst->bstatus & DP_BSTATUS_READY) {
		ret = dptx_hdcp1x_rpt_proc(dptx_hdcp1x_inst->bind_hw_comm);
		if (ret == 0)
			hdcp1_tx_repeater_auth_done = true;
		else
			hdcp1_tx_repeater_auth_done = false;
		DPTX_DEBUG_HDCP("DPTX HDCP1x rpt auth %s\n",
			hdcp1_tx_repeater_auth_done ? "pass" : "fail");
		dptx_hdcp1x_inst->auth_result = hdcp1_tx_repeater_auth_done;
	}

	/* if hdcp repeater & DPCD rev is greater than v1.2 */
	if (!hdcp1_tx_repeater_auth_done) {
		DPTX_DEBUG_HDCP("DPTX HDCP1X wait CP_IRQ for RPTR KSVLIST ready...\n");

		/* wait CP_IRQ for READY bit for ksv_list/V' */
		queue_delayed_work(dptx_hdcp1x_inst->hdcp_wq,
			&dptx_hdcp1x_inst->hdcp1x_ksv_list_work,
			msecs_to_jiffies(dptx_hdcp1x_inst->rpt_ksv_list_rdy_timeout_ms));
	}
}

static void dptx20_hdcp1x_r0_timeout_whandler(struct work_struct *w)
{
	struct dptx_hdcp1x *dptx_hdcp1x_inst = container_of((struct delayed_work *)w,
		struct dptx_hdcp1x, hdcp1x_r0_timeout_work);

	if (!dptx_hdcp1x_inst) {
		DPTX_ERROR("null dptx_hdcp1x_inst\n");
		return;
	}
	if (!dptx_hdcp1x_inst->hdcp1x_en)
		return;
	DPTX_ERROR("DPTX HDCP1X wait r0' ready timeout!\n");
		queue_delayed_work(dptx_hdcp1x_inst->hdcp_wq,
			&dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
}

static void dptx20_hdcp1x_ksv_list_whandler(struct work_struct *w)
{
	int ret = 0;
	bool work_canceled = false;
	struct dptx_hdcp1x *dptx_hdcp1x_inst = container_of((struct delayed_work *)w,
		struct dptx_hdcp1x, hdcp1x_ksv_list_work);

	if (!dptx_hdcp1x_inst) {
		DPTX_ERROR("null dptx_hdcp1x_inst\n");
		return;
	}
	if (!dptx_hdcp1x_inst->hdcp1x_en)
		return;
	work_canceled = cancel_delayed_work(&dptx_hdcp1x_inst->hdcp1x_ksv_list_timeout_work);
	if (!work_canceled) {
		/* ksv_list handler and r0_ksv_list handler come together,
		 * should not go into this case(as we add error tolerance
		 * in timeout period). However if this abnormal case happens,
		 * exit ksv_list handle and continue timeout retry
		 */
		DPTX_INFO("warning: ksv_list_timeout work in progress, continue timeout work\n");
		return;
	}
	if (dptx_hdcp1x_inst->bstatus & DP_BSTATUS_READY) {
		ret = dptx_hdcp1x_rpt_proc(dptx_hdcp1x_inst->bind_hw_comm);
		if (ret == 0)
			dptx_hdcp1x_inst->auth_result = true;
		else
			dptx_hdcp1x_inst->auth_result = false;
	}
	DPTX_DEBUG_HDCP("DPTX HDCP1x rpt auth %s\n",
		dptx_hdcp1x_inst->auth_result ? "pass" : "fail");
}

static void dptx20_hdcp1x_ksv_list_timeout_whandler(struct work_struct *w)
{
	struct dptx_hdcp1x *dptx_hdcp1x_inst = container_of((struct delayed_work *)w,
		struct dptx_hdcp1x, hdcp1x_ksv_list_timeout_work);

	if (!dptx_hdcp1x_inst) {
		DPTX_ERROR("null dptx_hdcp1x_inst\n");
		return;
	}
	if (!dptx_hdcp1x_inst->hdcp1x_en)
		return;
	DPTX_ERROR("DPTX HDCP1X KSV list timeout!\n");
		queue_delayed_work(dptx_hdcp1x_inst->hdcp_wq,
			&dptx_hdcp1x_inst->hdcp1x_reauth_work,
			msecs_to_jiffies(5));
}

void dptx_hdcp1x_auth_enable(struct dptx_hw_common *hw_comm)
{
	if (!hw_comm || !hw_comm->dptx_hdcp1x_inst)
		return;

	mutex_lock(&hw_comm->dptx_hdcp1x_inst->hdcp1x_lock);
	hw_comm->dptx_hdcp1x_inst->hdcp1x_en = true;
	dptx_hdcp1x_proc(hw_comm);
	mutex_unlock(&hw_comm->dptx_hdcp1x_inst->hdcp1x_lock);
}

void dptx_hdcp1x_auth_disable(struct dptx_hw_common *hw_comm)
{
	u8 max_loops = 3;

	if (!hw_comm || !hw_comm->dptx_hdcp1x_inst)
		return;

	mutex_lock(&hw_comm->dptx_hdcp1x_inst->hdcp1x_lock);
	hw_comm->dptx_hdcp1x_inst->hdcp1x_en = false;
	/* double confirm all work canceled */
	do {
		cancel_delayed_work_sync(&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work);
		cancel_delayed_work_sync(&hw_comm->dptx_hdcp1x_inst->hdcp1x_r0_timeout_work);
		cancel_delayed_work_sync(&hw_comm->dptx_hdcp1x_inst->hdcp1x_ksv_list_timeout_work);
		cancel_delayed_work_sync(&hw_comm->dptx_hdcp1x_inst->hdcp1x_r0_work);
		cancel_delayed_work_sync(&hw_comm->dptx_hdcp1x_inst->hdcp1x_ksv_list_work);

		max_loops--;
	} while (max_loops > 0 &&
		(delayed_work_pending(&hw_comm->dptx_hdcp1x_inst->hdcp1x_reauth_work) ||
		delayed_work_pending(&hw_comm->dptx_hdcp1x_inst->hdcp1x_r0_timeout_work) ||
		delayed_work_pending(&hw_comm->dptx_hdcp1x_inst->hdcp1x_ksv_list_timeout_work) ||
		delayed_work_pending(&hw_comm->dptx_hdcp1x_inst->hdcp1x_r0_work) ||
		delayed_work_pending(&hw_comm->dptx_hdcp1x_inst->hdcp1x_ksv_list_work)));

	flush_workqueue(hw_comm->dptx_hdcp1x_inst->hdcp_wq);

	/* disable encryption */
	hdcp1_tx_hw_encrypt_enable(hw_comm, false);
	/* disable HDCP1.X HW */
	hdcp1_tx_stack_init(hw_comm, false);
	hw_comm->dptx_hdcp1x_inst->auth_result = false;
	mutex_unlock(&hw_comm->dptx_hdcp1x_inst->hdcp1x_lock);
}

struct dptx_hdcp1x *dptx20_hdcp1x_init(struct dptx_hw_common *hw_comm)
{
	struct dptx_hdcp1x *dptx_hdcp1x_inst = NULL;

	dptx_hdcp1x_inst = kzalloc(sizeof(*dptx_hdcp1x_inst), GFP_KERNEL);
	if (!dptx_hdcp1x_inst)
		return NULL;
	dptx_hdcp1x_inst->ksv_lists = kmalloc((HDCP1X_MAX_DEV_CNT + 1) * 5, GFP_KERNEL);
	if (!dptx_hdcp1x_inst->ksv_lists) {
		kfree(dptx_hdcp1x_inst);
		return NULL;
	}
	dptx_hdcp1x_inst->hdcp_wq = alloc_workqueue(DPTX_HDCP1X_WQ_NAME,
		WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	if (!dptx_hdcp1x_inst->hdcp_wq) {
		kfree(dptx_hdcp1x_inst->ksv_lists);
		kfree(dptx_hdcp1x_inst);
		return NULL;
	}
	dptx_hdcp1x_inst->bind_hw_comm = hw_comm;
	dptx_hdcp1x_inst->auth_result = false;

	dptx_hdcp1x_inst->rx_r0_timeout_ms = DP1X_R0_TIMEOUT_MS;
	dptx_hdcp1x_inst->rpt_ksv_list_rdy_timeout_ms = DP1X_RPT_RDY_MS;

	/* assume internal r0 calculate done in 20ms */
	dptx_hdcp1x_inst->intenal_r0_timeout_ms = 20;
	/* assume internal SHA calculate done in 20ms */
	dptx_hdcp1x_inst->internal_sha_timeout_ms = 20;

	mutex_init(&dptx_hdcp1x_inst->hdcp1x_lock);

	/* hdcp related work */
	INIT_DELAYED_WORK(&dptx_hdcp1x_inst->hdcp1x_reauth_work, dptx20_hdcp1x_reauth_whandler);

	INIT_DELAYED_WORK(&dptx_hdcp1x_inst->hdcp1x_r0_work,
		dptx20_hdcp1x_r0_whandler);
	INIT_DELAYED_WORK(&dptx_hdcp1x_inst->hdcp1x_r0_timeout_work,
		dptx20_hdcp1x_r0_timeout_whandler);
	INIT_DELAYED_WORK(&dptx_hdcp1x_inst->hdcp1x_ksv_list_work,
		dptx20_hdcp1x_ksv_list_whandler);
	INIT_DELAYED_WORK(&dptx_hdcp1x_inst->hdcp1x_ksv_list_timeout_work,
		dptx20_hdcp1x_ksv_list_timeout_whandler);

	return dptx_hdcp1x_inst;
}

void dptx_hdcp1x_uninit(struct dptx_hdcp1x *dptx_hdcp1x_inst)
{
	u8 max_loops = 3;

	/* double confirm all work canceled */
	do {
		cancel_delayed_work_sync(&dptx_hdcp1x_inst->hdcp1x_reauth_work);
		cancel_delayed_work_sync(&dptx_hdcp1x_inst->hdcp1x_r0_timeout_work);
		cancel_delayed_work_sync(&dptx_hdcp1x_inst->hdcp1x_ksv_list_timeout_work);
		cancel_delayed_work_sync(&dptx_hdcp1x_inst->hdcp1x_r0_work);
		cancel_delayed_work_sync(&dptx_hdcp1x_inst->hdcp1x_ksv_list_work);

		max_loops--;
	} while (max_loops > 0 &&
		(delayed_work_pending(&dptx_hdcp1x_inst->hdcp1x_reauth_work) ||
		delayed_work_pending(&dptx_hdcp1x_inst->hdcp1x_r0_timeout_work) ||
		delayed_work_pending(&dptx_hdcp1x_inst->hdcp1x_ksv_list_timeout_work) ||
		delayed_work_pending(&dptx_hdcp1x_inst->hdcp1x_r0_work) ||
		delayed_work_pending(&dptx_hdcp1x_inst->hdcp1x_ksv_list_work)));

	flush_workqueue(dptx_hdcp1x_inst->hdcp_wq);

	destroy_workqueue(dptx_hdcp1x_inst->hdcp_wq);
	kfree(dptx_hdcp1x_inst->ksv_lists);
	kfree(dptx_hdcp1x_inst);
}

