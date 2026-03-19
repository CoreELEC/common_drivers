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

#define HDCP1X_MAX_DEV_CNT 127
/* DP HDP1.1 Spec chapter2.2.1
 * r0' must be available for the HDCP Transmitter to read
 * within 100 milliseconds from the time that the HDCP
 * Transmitter finishes writing Aksv to the HDCP Receiver.
 * The HDCP Transmitter must not read the r0' value
 * sooner than 100ms after write AKSV
 */
#define DP1X_R0_TIMEOUT_MS 100
/* DP HDP1.1 Spec chapter2.2.2
 * The HDCP Transmitter, having determined that the REPEATER
 * bit read earlier in the protocol is set, sets a five-second
 * watchdog timer. It may either poll the HDCP Repeater's READY
 * status bit or alternatively check the READY bit when a CP_IRQ
 * interrupt is received.
 */
#define DP1X_RPT_RDY_MS 5000

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
static void hdcp1_tx_state_init(struct dptx_hw_common *hw_comm)
{
	if (!hw_comm)
		return;

	/* hardware init */
	/* set mode to HDCP 1.x */
	hdcp1_tx_hw_mode_set(hw_comm, true);
	/* enable hdcp */
	hdcp1_tx_hw_state_enable(hw_comm, true);
	/* bit2 lvp enc */
	dptx20_reg_write(hw_comm, CORE_LEVEL,
		DPTX20_HDCP_CIPHER_CONTROL, (0x1 << 2) | (0x1 << 0));
}

/* HDCP stack initialization function */
static void hdcp1_tx_stack_init(struct dptx_hw_common *hw_comm)
{
	/* Initialize HDCP 1.x transmitter (source) state machine */
	hdcp1_tx_state_init(hw_comm);
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
		DPTX_ERROR("dptx hdp1.x read bcaps ret:%d\n", ret);
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
static void hdcp1_tx_a1_wr_ainfo_enter(struct dptx_hw_common *hw_comm)
{
	/* set REAUTHENTICATION_ENABLE_IRQ_HPD */
	hw_comm->dptx_hdcp1x_inst->ainfo = 1;
	/* write ainfo DPCD register */
	hdcp1_tx_dpcd_wr_ainfo(hw_comm);
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

	DPTX_DEBUG_HDCP("DPTX HDCP14 RPTR V ready Now\n");
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
	int ret = 0;

	DPTX_DEBUG_HDCP("DPTX HDCP1.x read ksvlist after R0' Prime\n");
	ret = hdcp1_tx_dpcd_rd_binfo(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return ret;
	}
	ret = hdcp1_tx_dpcd_rd_ksv_fifo(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return ret;
	}
	hdcp1_tx_ksv_lists_process(hw_comm);
	ret = hdcp1_tx_dpcd_rd_v_prime(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return ret;
	}
	ret = hdcp1_tx_get_rptr_v(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return ret;
	}
	if (compare_v_and_v_prime(hw_comm)) {
		ret = 0;
		DPTX_DEBUG_HDCP("DPTX HDCP14 Check RPTR V Prime Pass\n");
	} else {
		ret = -1;
		DPTX_ERROR("DPTX HDCP14 Check RPTR V Prime FAIL\n");
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

void dptx_hdcp1x_proc(struct dptx_hw_common *hw_comm)
{
	int ret;
	u8 hdcp1_tx_repeater_auth_done = 0;
	unsigned long intenal_r0_timeout = 0;
	bool internal_r0_ready = false;
	unsigned long stat = 0;

	if (!hw_comm || !hw_comm->dptx_hdcp1x_inst)
		return;

	hdcp1_tx_stack_init(hw_comm);
	ret = hdcp1_tx_dpcd_rd_bcaps(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return;
	}
	hdcp1_tx_hw_gen_an(hw_comm);
	ret = hdcp1_tx_dpcd_rd_bksv(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return;
	}
	ret = hdcp1_tx_a1_bksv_process(hw_comm);
	if (ret > 0) {
		/* TODO: abnormal retry logic */
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
		/* TODO: abnormal retry logic */
		DPTX_ERROR("DPTX HDCP1x internal r0 not ready\n");
		return;
	}

	hdcp1_tx_key_get_aksv(hw_comm);
	hdcp1_tx_a1_wr_ainfo_enter(hw_comm);
	ret = hdcp1_tx_dpcd_wr_an(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return;
	}

	/* reinit r0 completion before start write AKSV */
	reinit_completion(&hw_comm->dptx_hdcp1x_inst->r0_completion);
	reinit_completion(&hw_comm->dptx_hdcp1x_inst->ksv_list_completion);
	ret = hdcp1_tx_dpcd_wr_aksv(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return;
	}

	/* wait CP_IRQ for R0 prime */
	stat = wait_for_completion_timeout(&hw_comm->dptx_hdcp1x_inst->r0_completion,
		msecs_to_jiffies(hw_comm->dptx_hdcp1x_inst->rx_r0_timeout_ms));
	if (stat == 0) {
		DPTX_ERROR("DPTX HDCP1x wait for r0 timeout\n");
		/* TODO: abnormal retry logic */
		return;
	}
	/* read R0' from DP RX */
	ret = hdcp1_tx_dpcd_rd_r0_prime(hw_comm);
	if (ret < 0) {
		/* TODO: abnormal retry logic */
		return;
	}
	if (hw_comm->dptx_hdcp1x_inst->r0 != hw_comm->dptx_hdcp1x_inst->r0_prime) {
		DPTX_ERROR("DPTX HDCP14 Check R0' Prime fail!\n");
		/* TODO: abnormal retry logic */
		return;
	}

	DPTX_DEBUG_HDCP("DPTX HDCP1x Check R0' Prime pass\n");
	if (hw_comm->dptx_hdcp1x_inst->bstatus & DP_BSTATUS_READY) {
		ret = dptx_hdcp1x_rpt_proc(hw_comm);
		if (ret == 0)
			hdcp1_tx_repeater_auth_done = 1;
		else
			hdcp1_tx_repeater_auth_done = 0;
	}

	/* if hdcp repeater & DPCD rev is greater than v1.2 */
	if ((hw_comm->dptx_hdcp1x_inst->bcaps & DP_BCAPS_REPEATER_PRESENT) &&
		hdcp1_tx_repeater_auth_done == 0) {
		DPTX_DEBUG_HDCP("DPTX HDCP14 WAIT CP_IRQ FOR RPTR KSVLIST ready...\n");

		/* wait CP_IRQ for READY bit for ksv_list/V' */
		stat = wait_for_completion_timeout(&hw_comm->dptx_hdcp1x_inst->ksv_list_completion,
			msecs_to_jiffies(hw_comm->dptx_hdcp1x_inst->rpt_ksv_list_rdy_timeout_ms));
		if (stat == 0) {
			DPTX_ERROR("wait for ksv list timeout\n");
			/* TODO: abnormal retry logic */
			return;
		}

		if (hw_comm->dptx_hdcp1x_inst->bstatus & DP_BSTATUS_READY) {
			ret = dptx_hdcp1x_rpt_proc(hw_comm);
			if (ret == 0)
				hdcp1_tx_repeater_auth_done = 1;
			else
				hdcp1_tx_repeater_auth_done = 0;
		}
	}

	/* enable encryption */
	hdcp1_tx_hw_encrypt_enable(hw_comm, true);
}

struct dptx_hdcp1x *dptx20_hdcp1x_init(void)
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
	init_completion(&dptx_hdcp1x_inst->r0_completion);
	dptx_hdcp1x_inst->rx_r0_timeout_ms = DP1X_R0_TIMEOUT_MS;
	init_completion(&dptx_hdcp1x_inst->ksv_list_completion);
	dptx_hdcp1x_inst->rpt_ksv_list_rdy_timeout_ms = DP1X_RPT_RDY_MS;

	dptx_hdcp1x_inst->internal_sha_timeout_ms = 100;
	return dptx_hdcp1x_inst;
}

void dptx_hdcp1x_uninit(struct dptx_hdcp1x *dptx_hdcp1x_inst)
{
	kfree(dptx_hdcp1x_inst->ksv_lists);
	kfree(dptx_hdcp1x_inst);
}

