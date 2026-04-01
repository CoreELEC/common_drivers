// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/irq.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/spinlock_types.h>
#include <linux/extcon-provider.h>

#include "hdmi_tx_connector/hdcp/hdmitx_hdcp.h"
#include "hdmi_tx_connector/hdmitx/tx21/hdmitx_hw_platform.h"
#include "hdmi_tx_connector/hdmitx_audio.h"
#include "hdmi_rx_repeater.h"

#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>

#define TEE_HDCP_IOC_START _IOW('P', 0, int)
#define TEE_HDCP_IOC_VALIDATE_KEY _IOWR('P', 0x1, int)

/* notify delay in ms, for debug */
static int notify_delay_ms = 1;

static unsigned int delay_ms = 10;
unsigned long hdcp_reauth_dbg = 1;
unsigned long streamtype_dbg;
unsigned long en_fake_rcv_id;
/* static u8 fake_rcv_id[5] = {0x0}; */

/* avmute and wait a few ms for TV to take effect */
unsigned long avmute_ms = 30;

/*
 * for debug purpose, clear video mute and
 * wait a few ms before clear avmute
 */
unsigned long vid_mute_ms = 30;

static bool hdcptx_schedule_work(struct workqueue_struct *wq,
	struct hdcp_work *work, u32 delay_ms, u32 period_ms);
static bool hdcptx_stop_work(struct hdcp_work *work);
static bool hdcptx_stop_work_sync(struct hdcp_work *work);
static void hdcptx_update_failures(struct hdcptx21_core_priv *p_hdcp, enum hdcp_fail_types_t types);
static bool hdcp1x_ds_ksv_fifo_ready(u8 intr_reg);
static void hdcp2x_auth_stop(struct hdcptx21_core_priv *p_hdcp);
static void hdcptx_send_csm_msg(struct hdcptx21_core_priv *p_hdcp);
static void hdcptx_reset(struct hdcptx21_core_priv *p_hdcp);
static void hdcptx_get_ds_bksv_list(struct hdcptx21_core_priv *p_hdcp);
static void hdcptx_get_ds_rcv_id(struct hdcptx21_core_priv *p_hdcp);
static void hdcptx_update_csm(struct hdcptx21_core_priv *p_hdcp);

/*
 * for hdcp repeater, downstream auth should be controlled
 * (started) by upstream side, and should not started
 * until upstream request, otherwise will cause most
 * of the cts items fail. for example:
 * when start hdcp1.4 3B-01b, hdmitx detects hpd reset-->
 * hdmitx read EDID and notify hdmirx to hpd reset-->
 * hdmitx set mode and enable hdcp1.4 auth-->
 * hdmirx side detect hdcp1.4 auth from TE source-->
 * hdmirx notify hdmitx side to re-auth-->
 * the first hdcp1.4 auth break and then restart-->
 * TE doesn't respond to this re-auth, timeout and fail.
 */
bool hdcptx_need_ctrl_by_upstream(bool hdcp_rpt_en)
{
	if (!hdcp_rpt_en)
		return false;

	if (!get_rx_active_sts())
		return false;
	return true;
}

void hdcptx_mode_set(struct hdcptx21_core_priv *p_hdcp, unsigned int mode)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp private instance\n", __func__);
		return;
	}
	HDMITX_HDCP_INFO("%s[%d] %d\n", __func__, __LINE__, mode);

	if (mode == 0) {
		hdcptx_reset(p_hdcp);
		p_hdcp->hdcptx_enabled = 0;
		return;
	}

	if (mode != 1 && mode != 2)
		return;

	p_hdcp->hdcptx_enabled = 1;
	if (mode == 1)
		p_hdcp->req_hdcp_ver = HDCP_VER_HDCP1X;
	if (mode == 2)
		p_hdcp->req_hdcp_ver = HDCP_VER_HDCP2X;
	hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_start, 1, 0);
}

void hdmitx21_enable_hdcp(struct hdmitx_common *tx_comm)
{
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}

	if (hdmitx_hw_cntl(tx_comm->tx_hw, PLATFORM_RXSENSE, NULL, NULL) == 0)
		hdmitx_current_status(tx_comm, HDMITX_HDCP_DEVICE_NOT_READY_ERROR);
	p_hdcp = tx_comm->hdcptx_priv;
	/*
	 * lstore: 0 by default, 0x11/0x12 for debug usage
	 * 0: enable hdcp mode based on stored key and downstream hdcp cap
	 * priority: hdcp2.x > hdcp1.x
	 * 0x12: force hdcp2.x
	 * 0x11: force hdcp1.x
	 */
	mutex_lock(&p_hdcp->hdcptx_auth_mutex);
	if (tx_comm->hdcptx_comm.hdcp_mode != 0) {
		HDMITX_HDCP_INFO("[%s] hdcp %d already enabled, exit\n",
			__func__, tx_comm->hdcptx_comm.hdcp_mode);
		mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
		return;
	}

	if (tx_comm->hdcptx_comm.hdcp_lstore == 0) {
		if (hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_22_LSTORE, NULL, NULL) &&
			tx_comm->hdcptx_comm.dw_hdcp22_cap) {
			/* enable hdcp gate */
			hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 2, true);
			tx_comm->hdcptx_comm.hdcp_mode = 2;
			hdcptx_mode_set(p_hdcp, 2);
		} else {
			if (tx_comm->fmt_para.frl_rate > FRL_NONE &&
				tx_comm->fmt_para.frl_rate < FRL_RATE_MAX) {
				tx_comm->hdcptx_comm.hdcp_mode = 0;
				HDMITX_HDCP_INFO("[%s] should not enable hdcp1.4 under FRL mode\n",
					__func__);
			} else if (get_hdcp1_lstore(tx_comm)) {
				hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type,
					1, true);
				tx_comm->hdcptx_comm.hdcp_mode = 1;
				hdcptx_mode_set(p_hdcp, 1);
			} else {
				hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type,
					0, false);
				tx_comm->hdcptx_comm.hdcp_mode = 0;
			}
		}
	} else if (tx_comm->hdcptx_comm.hdcp_lstore & 0x2) {
		if (get_hdcp2_lstore(tx_comm) && tx_comm->hdcptx_comm.dw_hdcp22_cap) {
			hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type,
				2, true);
			tx_comm->hdcptx_comm.hdcp_mode = 2;
			hdcptx_mode_set(p_hdcp, 2);
		} else {
			hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type,
				0, false);
			tx_comm->hdcptx_comm.hdcp_mode = 0;
		}
	} else if (tx_comm->hdcptx_comm.hdcp_lstore & 0x1) {
		if (tx_comm->fmt_para.frl_rate > FRL_NONE &&
			tx_comm->fmt_para.frl_rate < FRL_RATE_MAX) {
			tx_comm->hdcptx_comm.hdcp_mode = 0;
			HDMITX_HDCP_INFO("[%s] should not enable hdcp1.4 under FRL mode\n",
				__func__);
		} else if (get_hdcp1_lstore(tx_comm)) {
			hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 1, true);
			tx_comm->hdcptx_comm.hdcp_mode = 1;
			hdcptx_mode_set(p_hdcp, 1);
		} else {
			hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 0, false);
			tx_comm->hdcptx_comm.hdcp_mode = 0;
		}
	} else {
		HDMITX_DEBUG_HDCP("debug: not enable hdcp as no key stored\n");
	}
	mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
}

/* use hdcptx_stop_work_sync outside of hdcp auth handlers */
static void hdcptx_cancel_works(struct hdcptx21_core_priv *p_hdcp)
{
	u8 stop_work_max = 3;
	bool hdcp_work_scheduling = false;

	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp private instance\n", __func__);
		return;
	}
	/*
	 * two note points:
	 * 1.wait hdcp work stop finish in case there's asynchronous problem:
	 * for example: hdcp work stop is called, and then hdcp HW is disabled.
	 * if the hdcp work is not canceled in sync mode, it may be queued again
	 * after hdcp HW is disabled, and HDCP HW may be enabled again and
	 * cause some abnormal DDC issue.
	 * 2.one hdcp work may queue another work, such as hdcp_auth_fail_retry
	 * work will queue hdcp_rcv_auth and ddc_check_nak again, and hdcp_rcv_auth
	 * or ddc_check_nak may queue hdcp_auth_fail_retry work again. so need
	 * to double confirm all hdcp related work are disabled
	 */
	do {
		hdcp_work_scheduling = false;
		/*
		 * if work is pending in the queue, it means that it can be canceled
		 * cleanly and won't schedule other works, otherwise there may be
		 * two cases: 1. work is now under scheduling, 2.work is not queued
		 * but there's no kernel API to get whether it's case 1 or 2
		 * for case 1, it may schedule other new hdcp works, and need to cancel
		 * new works again. As delay works need time to schedule, so we assume
		 * that the new queued works can be canceled by twice stop work action.
		 */
		if (!hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_start))
			hdcp_work_scheduling = true;
		if (!hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_rcv_auth))
			hdcp_work_scheduling = true;
		if (!hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_rpt_auth))
			hdcp_work_scheduling = true;
		if (!hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_auth_fail_retry))
			hdcp_work_scheduling = true;
		if (!hdcptx_stop_work_sync(&p_hdcp->timer_bksv_poll_done))
			hdcp_work_scheduling = true;
		if (!hdcptx_stop_work_sync(&p_hdcp->timer_ddc_check_nak))
			hdcp_work_scheduling = true;
		if (!hdcptx_stop_work_sync(&p_hdcp->timer_update_csm))
			hdcp_work_scheduling = true;
	} while (--stop_work_max > 0 && hdcp_work_scheduling);
}

/*
 * when disable signal, should also disable hdcp
 * there're below cases need to disable hdcp:
 * 1.suspend, 2.before switch mode, 3.plug out
 * note disable hdcp mode should mutex with EDID
 * operation(plugin bottom half), which already
 * mutexed with above 3 operations
 */
void hdmitx21_disable_hdcp(struct hdcptx21_core_priv *p_hdcp)
{
	struct hdmitx_common *tx_comm;

	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp private instance\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;

	mutex_lock(&p_hdcp->hdcptx_auth_mutex);
	cancel_delayed_work(&p_hdcp->work_up_hdcp_timeout);
	cancel_delayed_work(&p_hdcp->work_tx_start_hdcp);
	cancel_delayed_work_sync(&p_hdcp->work_drm_start_hdcp);
	if (tx_comm->hdcptx_comm.hdcp_mode != 0) {
		hdcptx_cancel_works(p_hdcp);
		hdcptx_mode_set(p_hdcp, 0);
		/* need to update hdcp auth result */
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_GET_AUTH_RESULT, NULL, NULL);
		tx_comm->hdcptx_comm.hdcp_mode = 0;
		mdelay(10);
		hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 0, false);
	}
	mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
}

u32 hdmitx21_get_hdcp_mode(struct hdmitx_common *tx_comm)
{
	u32 hdcp_mode = 0;
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("NULL tx_comm or NULL hdcp_private instance\n");
		return hdcp_mode;
	}
	p_hdcp = (struct hdcptx21_core_priv *)tx_comm->hdcptx_priv;

	mutex_lock(&p_hdcp->hdcptx_auth_mutex);
	hdcp_mode = tx_comm->hdcptx_comm.hdcp_mode;
	mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
	return hdcp_mode;
}

/* KSV FIFO first byte indicator, not use b_cap ready bit */
static bool hdcp1x_ds_ksv_fifo_ready(u8 intr_reg)
{
	if (intr_reg & 0x08)
		return true;
	else
		return false;
}

static void hdcptx_update_state(struct hdcptx21_core_priv *p_hdcp, enum hdcp_stat_t state)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	if (p_hdcp->hdcp_state != state || state == HDCP_STAT_NONE) {
		p_hdcp->hdcp_state = state;
		HDMITX_DEBUG_HDCP("%s[%d] %d\n", __func__, __LINE__, state);
	}
}

static void hdcp2x_reauth_start(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	if (!p_hdcp->reauth_ignored)
		hdcptx2_reauth_send();
}

static void hdcptx_topology_update(struct hdcptx21_core_priv *p_hdcp)
{
	struct hdcp_topo_t *topo = &p_hdcp->hdcp_topology;
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_common or NULL private_hdcp struct\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		if (p_hdcp->ds_repeater) {
			u8 topo_val;

			topo_val = hdcptx2_topology_get();
			/*
			 * should only add depth/count for repeater itself
			 * when propagate topology information to upstream side.
			 */
			topo->rp_depth = hdcptx2_rpt_depth_get();
			topo->dev_count = hdcptx2_rpt_dev_cnt_get();
			if (topo_val & 0x08 || topo->dev_count > HDCP2X_MAX_DEV)
				topo->max_devs_exceed = true;
			else
				topo->max_devs_exceed = false;
			if (topo_val & 0x04 || topo->rp_depth > HDCP2X_MAX_DEPTH)
				topo->max_cas_exceed = true;
			else
				topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = (topo_val & 0x02) ? true : false;
			topo->ds_hdcp1x_dev = (topo_val & 0x01) ? true : false;

			if (topo->max_devs_exceed)
				topo->dev_count = HDCP2X_MAX_DEV;
			if (p_hdcp->hdcp_topology.max_cas_exceed)
				topo->rp_depth = HDCP2X_MAX_DEPTH;
		} else {
			topo->rp_depth = 1;
			topo->dev_count = 1;
			topo->max_devs_exceed = false;
			topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = false;
			topo->ds_hdcp1x_dev = false;
		}
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		if (p_hdcp->ds_repeater) {
			u8 bstatus[2];
			u8 max_devices = tx_comm->hdcptx_comm.hdcp_rpt_en ?
				HDCP1X_MAX_TX_DEV_RPT : HDCP1X_MAX_TX_DEV_SRC;

			hdcptx1_bstatus_get(bstatus);
			/*
			 * for 3C-II-06/07, the maximum cascade and device count actually
			 * not exceed the maximum, if we add depth/count for repeater itself
			 * here, it will exceed the maximum by wrong and will retry hdcp
			 * auth as topology error, hdcp1.4 repeater CTS 3C-II-06/07 will fail
			 * by this retry. should only add depth/count for repeater itself
			 * when propagate topology information to upstream side.
			 */
			topo->rp_depth = bstatus[1] & 0x07;
			topo->dev_count = bstatus[0] & 0x7F;
			if (bstatus[0] & 0x80 || topo->dev_count > max_devices)
				topo->max_devs_exceed = true;
			else
				topo->max_devs_exceed = false;
			if (bstatus[1] & 0x08 || topo->rp_depth > HDCP1X_MAX_DEPTH)
				topo->max_cas_exceed = true;
			else
				topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = false;
			topo->ds_hdcp1x_dev = true;

			if (topo->max_devs_exceed)
				topo->dev_count = max_devices;
			if (topo->max_cas_exceed)
				topo->rp_depth = HDCP1X_MAX_DEPTH;
		} else {
			/* include the depth of repeater itself */
			topo->rp_depth = 1;
			topo->dev_count = 1;
			topo->max_devs_exceed = false;
			topo->max_cas_exceed = false;
			topo->ds_hdcp2x_dev = false;
			topo->ds_hdcp1x_dev = true;
			/* hdcptx1_ds_bksv_read(p_hdcp->p_ksv_lists, KSV_SIZE); */
		}
	}
}

static void hdcptx_encryption_update(struct hdcptx21_core_priv *p_hdcp, bool en)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	if (p_hdcp->ds_auth) {
		p_hdcp->encryption_enabled = en;

		if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
			hdcptx1_encryption_update(en);
		} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
			hdcptx1_encryption_update(en);
			hdcptx2_encryption_update(en);
		}
	}
}

static void hdcptx_check_ds_csm_status(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	HDMITX_DEBUG_HDCP("%s[%d] ds_repeater %d  hdcp_type %d  csm_valid %d  content_type %d\n",
		__func__, __LINE__, p_hdcp->ds_repeater, p_hdcp->hdcp_type,
		p_hdcp->csm_valid, p_hdcp->content_type);
	if (p_hdcp->ds_repeater) {
		if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
			if (p_hdcp->csm_valid)
				hdcptx_send_csm_msg(p_hdcp);
		} else {
			hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
		}
	} else {
		if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
			hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
		} else {
			if (p_hdcp->csm_valid) {
				if (p_hdcp->content_type == HDCP_CONTENT_TYPE_1)
					hdcptx_update_failures(p_hdcp, HDCP_FAIL_CONTENT_TYPE);
				else if (p_hdcp->content_type == HDCP_CONTENT_TYPE_0)
					hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
				else
					hdcptx_update_failures(p_hdcp, HDCP_FAIL_CONTENT_TYPE);
			} else {
				hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
			}
		}
	}
}

static void hdcptx_authenticated_handle(struct hdcptx21_core_priv *p_hdcp)
{
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_common or NULL private_hdcp struct\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;

	HDMITX_HDCP_INFO("part 1 done\n");
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
		p_hdcp->ds_auth = true;
		HDMITX_HDCP_INFO("1x AuthDone\n");
		p_hdcp->fail_type = HDCP_FAIL_NONE;
		hdcptx_encryption_update(p_hdcp, true);
		hdcptx_topology_update(p_hdcp);
		hdcptx_get_ds_bksv_list(p_hdcp);
		hdcptx_check_ds_csm_status(p_hdcp);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
		p_hdcp->ds_auth = true;
		HDMITX_HDCP_INFO("2x AuthDone\n");
		p_hdcp->fail_type = HDCP_FAIL_NONE;
		hdcptx_encryption_update(p_hdcp, true);
		p_hdcp->ds_repeater = false;
		hdcptx_topology_update(p_hdcp);
		hdcptx_get_ds_rcv_id(p_hdcp);
		/* ds is hdcp2.2 TV */
		set_hdcp2_topo(tx_comm->tx_hw->chip_data->chip_type, 1);
		hdcptx_check_ds_csm_status(p_hdcp);
	}
}

static bool hdcptx_query_ds_repeater(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return false;
	}

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X)
		return hdcptx2_ds_rptr_capability();
	else
		return hdcptx1_ds_rptr_capability();
}

static bool hdcptx_is_bksv_valid(void)
{
	u8 bksv[KSV_SIZE];
	u8 i, count = 0;

	hdcptx1_ds_bksv_read(bksv, KSV_SIZE);

	for (i = 0; i < KSV_SIZE; i++) {
		while (bksv[i] != 0) {
			if (bksv[i] & 1)
				count++;
			bksv[i] >>= 1;
		}
	}
	return count == 20;
}

static void hdcp1x_check_bksv_done(struct hdcptx21_core_priv *p_hdcp)
{
	u8 copp_data1;

	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	copp_data1 = hdcptx1_ds_cap_status_get();
	HDMITX_DEBUG_HDCP("%s copp_data1 0x%02x\n", __func__, copp_data1);

	if (copp_data1 & 0x02) {
		hdcptx_stop_work(&p_hdcp->timer_bksv_poll_done);
		hdcptx1_protection_enable(true);
		hdcptx1_intermed_ri_check_enable(1);
	}
}

static void hdcptx_reset_ksv_fifo(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	p_hdcp->p_ksv_next = p_hdcp->p_ksv_lists;
}

static bool hdcptx_is_topology_correct(struct hdcptx21_core_priv *p_hdcp)
{
	u8 max_depth = HDCP1X_MAX_DEPTH;
	u8 max_device_count;
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_common or NULL private_hdcp struct\n", __func__);
		return false;
	}
	tx_comm = p_hdcp->bind_instance;
	max_device_count = tx_comm->hdcptx_comm.hdcp_rpt_en ?
		HDCP1X_MAX_TX_DEV_RPT : HDCP1X_MAX_TX_DEV_SRC;

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		max_device_count = HDCP2X_MAX_DEV;
		max_depth = HDCP2X_MAX_DEPTH;
	}
	if (p_hdcp->hdcp_topology.max_devs_exceed || p_hdcp->hdcp_topology.max_cas_exceed)
		return false;

	return true;
}

static void hdcptx_get_ds_rcvid_lists(struct hdcptx21_core_priv *p_hdcp, u8 count)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	hdcptx2_ds_rpt_rcvid_list_read(p_hdcp->p_ksv_next, count, KSV_SIZE);
	p_hdcp->p_ksv_next += count * KSV_SIZE;
}

static void hdcptx_get_ds_rcv_id(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	hdcptx2_ds_rcv_id_read(p_hdcp->p_ksv_next);
	p_hdcp->p_ksv_next += KSV_SIZE;
}

static void hdcptx_get_ds_bksv_list(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	hdcptx1_ds_bksv_read(p_hdcp->p_ksv_next, KSV_SIZE);
	p_hdcp->p_ksv_next += KSV_SIZE;
}

static void hdcptx_assemble_ds_ksv_lists(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return;
	}

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		u8 ds_bstatus[2];
		u8 ds_count;
		u8 ds_depth;

		hdcptx1_bstatus_get(ds_bstatus);
		ds_count = ds_bstatus[0] & 0x7F;
		ds_depth = ds_bstatus[1] & 0x07;
		hdcptx1_get_ds_ksvlists(&p_hdcp->p_ksv_next, ds_count);
		/*
		 * include the bksv of downstream repeater, add to the
		 * ksv list which will report to upstream
		 */
		hdcptx_get_ds_bksv_list(p_hdcp);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		u8 dev_cnt = hdcptx2_rpt_dev_cnt_get();

		hdcptx_get_ds_rcvid_lists(p_hdcp, dev_cnt);
		hdcptx_get_ds_rcv_id(p_hdcp);
	}
}

static bool hdcptx_process_repeater_fail(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL private_hdcp struct\n", __func__);
		return false;
	}

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		u8 ds_bstatus[2];

		hdcptx_get_ds_bksv_list(p_hdcp);

		hdcptx1_bstatus_get(ds_bstatus);
		if ((ds_bstatus[0] & 0x80) || (ds_bstatus[1] & 0x08)) {
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return false;
		}
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_V);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		u8 hdcp_topology;

		hdcp_topology = hdcptx2_topology_get();
		if ((hdcp_topology & 0x04) || (hdcp_topology & 0x08)) {
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return false;
		}
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_READY_TO);
	}
	return true;
}

static void hdcptx_rpt_ready_process(struct hdcptx21_core_priv *p_hdcp, bool ksv_read_status)
{
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_common or NULL private_hdcp struct\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;
	bool hdcp_rpt_en = tx_comm->hdcptx_comm.hdcp_rpt_en;

	hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	if (!(p_hdcp->hdcp_state == HDCP_STAT_FAILED ||
	      p_hdcp->hdcp_state == HDCP_STAT_AUTH)) {
		if (!(p_hdcp->hdcp_state == HDCP_STAT_SUCCESS ||
		      p_hdcp->hdcp_state == HDCP_STAT_CSM)) {
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_UNKNOWN);
			return;
		}
	}
	if (!ksv_read_status) {
		hdcptx_topology_update(p_hdcp);
		hdcptx_reset_ksv_fifo(p_hdcp);
		if (!hdcptx_process_repeater_fail(p_hdcp))
			return;
	} else {
		hdcptx_topology_update(p_hdcp);
		if (hdcptx_is_topology_correct(p_hdcp)) {
			hdcptx_assemble_ds_ksv_lists(p_hdcp);
			if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
				/*
				 * if no hdcp1.x or hdcp2.0 legacy devices
				 * downstream, then topo 1
				 */
				mutex_lock(&p_hdcp->stream_type_mutex);
				if (p_hdcp->hdcp_topology.ds_hdcp1x_dev == 0 &&
					p_hdcp->hdcp_topology.ds_hdcp2x_dev == 0) {
					set_hdcp2_topo(tx_comm->tx_hw->chip_data->chip_type, 1);
					/* not care if currently in hdmirx channel or not */
					/* if (!hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) { */
						p_hdcp->csm_message.streamid_type = 1;
						HDMITX_HDCP_INFO("auth with streamid_type = 1\n");
					/* } */
				} else {
					set_hdcp2_topo(tx_comm->tx_hw->chip_data->chip_type, 0);
					/*
					 * customer special process: if 4k output && downstream
					 * contain non-hdcp2.2 device, then set stream type 1.
					 * it will cause NTS fail.
					 */
					/* if (is_current_4k_format()) */
						/* p_hdcp->csm_message.streamid_type = 1; */
					/* else */
					p_hdcp->csm_message.streamid_type = 0;
				}
				/* need to propagate the stream type sent by upstream side */
				if (hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) {
					if (p_hdcp->saved_upstream_type & 0x10) {
						p_hdcp->csm_message.streamid_type =
							p_hdcp->saved_upstream_type & 0xf;
						p_hdcp->saved_upstream_type &= 0xf;
						HDMITX_HDCP_INFO("force upstream type: %d\n",
							p_hdcp->csm_message.streamid_type);
						/* need set to false, so that to re-send */
						p_hdcp->csm_updated = false;
						p_hdcp->csm_msg_sent = false;
					}
				}
				if (p_hdcp->csm_msg_sent) {
					hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
				} else {
					hdcptx_update_state(p_hdcp, HDCP_STAT_CSM);
					if (p_hdcp->csm_valid) {
						p_hdcp->csm_updated = true;
						hdcptx_send_csm_msg(p_hdcp);
						hdcptx2_rpt_smng_xfer_start();
						HDMITX_HDCP_INFO("hdcptx2: send stream type: %d\n",
							p_hdcp->csm_message.streamid_type);
					}
				}
				mutex_unlock(&p_hdcp->stream_type_mutex);
			}
		} else {
			hdcptx_assemble_ds_ksv_lists(p_hdcp);
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return;
		}
	}
	p_hdcp->rpt_ready = true;
	HDMITX_HDCP_INFO("%s[%d] rpt_ready %d\n", __func__, __LINE__,
		p_hdcp->rpt_ready);
}

static void hdcptx_notify_rpt_info(struct work_struct *work)
{
	unsigned int ksv_bytes = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	struct hdcptx21_core_priv *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core_priv, ksv_notify_wk);
	struct hdcp_topo_t *topo = &p_hdcp->hdcp_topology;
	/* hdcp_topo info transferred to upstream, 1.4 & 2.2 */
	struct hdcp_topo_s hdcp_topo;
	struct hdmitx_common *tx_comm;
	bool hdcp_rpt_en = false;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_common or NULL private_hdcp struct\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;
	hdcp_rpt_en = tx_comm->hdcptx_comm.hdcp_rpt_en;
	/*
	 * note: need calculate the topo info for hdcp repeater
	 * here instead of in hdcptx_topology_update()
	 */
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		if (p_hdcp->ds_repeater) {
			/*
			 * include the count/depth of downstream repeater itself.
			 * the rcvid list of downstream repeater will be added to notify
			 * list in hdcptx_assemble_ds_ksv_lists()
			 */
			if (hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) {
				topo->rp_depth += 1;
				topo->dev_count += 1;
			}
			if (topo->dev_count > HDCP2X_MAX_DEV)
				topo->max_devs_exceed = true;
			if (topo->rp_depth > HDCP2X_MAX_DEPTH)
				topo->max_cas_exceed = true;

			if (topo->max_devs_exceed)
				topo->dev_count = HDCP2X_MAX_DEV;
			if (topo->max_cas_exceed)
				topo->rp_depth = HDCP2X_MAX_DEPTH;
		}
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		if (p_hdcp->ds_repeater) {
			u8 max_devices = hdcp_rpt_en ?
				HDCP1X_MAX_TX_DEV_RPT : HDCP1X_MAX_TX_DEV_SRC;
			/*
			 * include the count/depth of downstream repeater itself.
			 * the bksv of downstream repeater will be added to notify
			 * list in hdcptx_assemble_ds_ksv_lists()
			 */
			if (hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) {
				topo->rp_depth += 1;
				topo->dev_count += 1;
			}
			if (topo->dev_count > max_devices)
				topo->max_devs_exceed = true;
			if (topo->rp_depth > HDCP1X_MAX_DEPTH)
				topo->max_cas_exceed = true;

			if (topo->max_devs_exceed)
				topo->dev_count = max_devices;
			if (topo->max_cas_exceed)
				topo->rp_depth = HDCP1X_MAX_DEPTH;
		}
	}

	memset(&hdcp_topo, 0, sizeof(hdcp_topo));
	hdcp_topo.hdcp_ver = p_hdcp->hdcp_type;
	HDMITX_DEBUG_HDCP("hdcp_type: %d\n", p_hdcp->hdcp_type);
	hdcp_topo.depth = p_hdcp->hdcp_topology.rp_depth;
	hdcp_topo.dev_cnt = p_hdcp->hdcp_topology.dev_count;
	if (en_fake_rcv_id) {
		/* memcpy(p_hdcp->p_ksv_next, fake_rcv_id, ARRAY_SIZE(fake_rcv_id)); */
		/* p_hdcp->p_ksv_next += ARRAY_SIZE(fake_rcv_id); */
		/* hdcp_topo.dev_cnt += 1; */
	}
	hdcp_topo.max_cascade_exceeded = p_hdcp->hdcp_topology.max_cas_exceed;
	hdcp_topo.max_devs_exceeded = p_hdcp->hdcp_topology.max_devs_exceed;
	hdcp_topo.hdcp1_dev_ds = p_hdcp->hdcp_topology.ds_hdcp1x_dev;
	hdcp_topo.hdcp2_dev_ds = p_hdcp->hdcp_topology.ds_hdcp2x_dev;
	/* p_hdcp->p_ksv_next - p_hdcp->p_ksv_lists */
	ksv_bytes = hdcp_topo.dev_cnt * 5;
	ksv_bytes = ksv_bytes > ARRAY_SIZE(hdcp_topo.ksv_list) ?
		ARRAY_SIZE(hdcp_topo.ksv_list) : ksv_bytes;
	memcpy(hdcp_topo.ksv_list, p_hdcp->p_ksv_lists, ksv_bytes);
	HDMITX_DEBUG_HDCP("%s depth:[%d] cnt:[%d] ksv_bytes:[%d] max_cas: [%d], max_dev: [%d]\n",
		__func__, hdcp_topo.depth, hdcp_topo.dev_cnt, ksv_bytes,
		hdcp_topo.max_cascade_exceeded, hdcp_topo.max_devs_exceeded);
	for (i = 0; i < ksv_bytes / 5; i++) {
		HDMITX_DEBUG_HDCP("ksv list[%d]: ", i);
		for (j = 0; j < 5; j++)
			HDMITX_DEBUG_HDCP("%02x\n", hdcp_topo.ksv_list[5 * i + j]);
		HDMITX_DEBUG_HDCP("\n");
	}
	hdmitx_event_mgr_notify(tx_comm->event_mgr,
			HDMITX_KSVLIST, &hdcp_topo);
}

static void hdcptx_req_reauth_whandler(struct work_struct *work)
{
	struct hdcptx21_core_priv *p_hdcp = container_of((struct delayed_work *)work,
			struct hdcptx21_core_priv, req_reauth_wk);
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_common or NULL private_hdcp struct\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;

	/* once receive hdcp auth from upstream side, cancel timeout */
	cancel_delayed_work(&p_hdcp->work_up_hdcp_timeout);
	/* if signal not ready, means hdmitx is setting mode, delay 1S */
	/* note: for CTS, it should not delay */
	mutex_lock(&tx_comm->hdmimode_mutex);
	if (tx_comm->suspend_flag) {
		HDMITX_HDCP_INFO("suspend, no need re-auth\n");
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	if (!tx_comm->hpd_state) {
		HDMITX_HDCP_INFO("hdmitx hpd low, no need re-auth\n");
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	if (!tx_comm->ready) {
		schedule_delayed_work(&p_hdcp->req_reauth_wk, msecs_to_jiffies(1000));
		HDMITX_HDCP_INFO("hdmitx signal not done, delay hdcp re-auth\n");
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	/* firstly need to disable current hdcp auth */
	/*
	 * set avmute, wait for sink side action, and then
	 * stop hdcp, at last clear avmute. to prevent TV
	 * display abnormal when detect signal from encryption
	 * to un-encryption.
	 * hdcp_reauth_dbg == 4 is only for debug purpose
	 */
	if (hdcp_reauth_dbg == 1) {
		/* before re-auth, do both video mute + avmute */
		hdmitx21_video_mute_op(tx_comm->tx_hw, 0, VIDEO_MUTE_PATH_3);
		hdmitx_common_avmute_locked(tx_comm, SET_AVMUTE, AVMUTE_PATH_2);
		msleep_interruptible(avmute_ms);
		hdmitx21_disable_hdcp(p_hdcp);
		hdmitx21_video_mute_op(tx_comm->tx_hw, 1, VIDEO_MUTE_PATH_3);
		/* msleep_interruptible(vid_mute_ms); */
		hdmitx_common_avmute_locked(tx_comm, CLR_AVMUTE, AVMUTE_PATH_2);
	} else if (hdcp_reauth_dbg == 4) {
		if (tx_comm->hdcptx_comm.hdcp_mode) {
			if (p_hdcp->req_reauth_ver == 0 ||
				p_hdcp->req_reauth_ver == tx_comm->hdcptx_comm.hdcp_mode) {
				HDMITX_HDCP_INFO("topology not changed, no need hdcp re-auth\n");
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
				mutex_unlock(&tx_comm->hdmimode_mutex);
				return;
			}
		}
		hdmitx21_video_mute_op(tx_comm->tx_hw, 0, VIDEO_MUTE_PATH_3);
		mdelay(20);

		hdmitx_common_avmute_locked(tx_comm, SET_AVMUTE, AVMUTE_PATH_2);
		msleep_interruptible(avmute_ms);
		hdmitx21_disable_hdcp(p_hdcp);
		hdmitx21_video_mute_op(tx_comm->tx_hw, 1, VIDEO_MUTE_PATH_3);
		msleep_interruptible(vid_mute_ms);
		hdmitx_common_avmute_locked(tx_comm, CLR_AVMUTE, AVMUTE_PATH_2);
	}
	if (p_hdcp->req_reauth_ver == 0) {
		/* auto mode, need to use ds hdcp version */
		hdmitx21_enable_hdcp(tx_comm);
	} else if (p_hdcp->req_reauth_ver == 1) {
		/* force hdcp1.x mode */
		mutex_lock(&p_hdcp->hdcptx_auth_mutex);
		if (get_hdcp1_lstore(tx_comm)) {
			tx_comm->hdcptx_comm.hdcp_mode = 1;
			hdcptx_mode_set(p_hdcp, 1);
		} else {
			tx_comm->hdcptx_comm.hdcp_mode = 0;
		}
		mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
	} else if (p_hdcp->req_reauth_ver == 2) {
		/* force hdcp2.x mode */
		mutex_lock(&p_hdcp->hdcptx_auth_mutex);
		if (get_hdcp2_lstore(tx_comm) && tx_comm->hdcptx_comm.dw_hdcp22_cap) {
			tx_comm->hdcptx_comm.hdcp_mode = 2;
			hdcptx_mode_set(p_hdcp, 2);
		} else {
			tx_comm->hdcptx_comm.hdcp_mode = 0;
		}
		mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
	}
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

void hdmitx21_rst_stream_type(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp)
		return;
	p_hdcp->csm_message.streamid_type = p_hdcp->def_stream_type;
}

static void hdcp_stream_mute_whandler(struct work_struct *work)
{
	struct hdcptx21_core_priv *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core_priv, stream_mute_wk);
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;
	/* not care race condition, use the last mute status */
	hdmitx21_video_mute_op(tx_comm->tx_hw, !p_hdcp->stream_mute, VIDEO_MUTE_PATH_2);
	hdmitx_audio_mute_op(tx_comm, !p_hdcp->stream_mute, AUDIO_MUTE_PATH_3);
}

/*
 * note that if hdcp is not enabled currently,
 * (as hdcp disabled somehow, e.g. hdmirx request
 * re-auth, but hdmitx suddenly disconnected),
 * should exit the propagate this time, just save
 * the stream type.
 */
static void hdmitx_propagate_stream_type(struct work_struct *work)
{
	struct hdcptx21_core_priv *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core_priv, stream_type_wk);
	u8 update_scene = 0;
	u8 upstream_type = 0;
	u8 cur_content_type = 0;
	bool ds_repeater = false;
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;
	update_scene = p_hdcp->rx_update_flag & 0xf0;
	upstream_type = p_hdcp->rx_update_flag & 0xf;

	/* if signal not ready, should not propagate stream type */
	mutex_lock(&tx_comm->hdmimode_mutex);
	mutex_lock(&p_hdcp->stream_type_mutex);
	if (update_scene == STREAMTYPE_UPDATE) {
		/* update upstream type firstly */
		p_hdcp->saved_upstream_type = 0x10 | upstream_type;
		if (tx_comm->suspend_flag) {
			HDMITX_HDCP_INFO("%s, suspend, no need propagate stream type\n", __func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		if (!tx_comm->hpd_state) {
			HDMITX_HDCP_INFO("%s, hdmitx hpd low, no need propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		if (!tx_comm->ready) {
			HDMITX_HDCP_INFO("%s, hdmitx signal not done, not propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		/* make sure hdcp is enabled */
		if (hdmitx21_get_hdcp_mode(tx_comm) == 0) {
			HDMITX_HDCP_INFO("%s, hdcp disabled, no need propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		ds_repeater = hdcptx_query_ds_repeater(p_hdcp);
		if (!ds_repeater || p_hdcp->hdcp_type != HDCP_VER_HDCP2X) {
			HDMITX_HDCP_INFO("[%d] not propagate stream type, ds_rpt:%d, ds_hdcp:%d\n",
				update_scene, ds_repeater, p_hdcp->hdcp_type);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		/* if downstream connect repeater, then propagate stream id to downstream */
		cur_content_type = p_hdcp->csm_message.streamid_type & 0x00FF;
		/*
		 * if current stream type not consistent with upstream,
		 * re-auth with new stream type notified by upstream,
		 * hdcp re-auth version is req_reauth_ver
		 */
		if (upstream_type != cur_content_type) {
			p_hdcp->csm_message.streamid_type =
				(p_hdcp->csm_message.streamid_type & 0xFF00) |
				upstream_type;
			HDMITX_HDCP_INFO("case[%d] stream type change from %d to %d\n",
				update_scene, cur_content_type, upstream_type);
			if (p_hdcp->cont_smng_method == 0) {
				p_hdcp->csm_updated = true;
				hdcptx_update_csm(p_hdcp);
				hdcptx2_rpt_smng_xfer_start();
			} else if (p_hdcp->cont_smng_method == 1) {
				schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
			}
		}
	} else if (update_scene == UPSTREAM_INACTIVE) {
		/* update upstream type firstly */
		p_hdcp->saved_upstream_type = 0;
		if (tx_comm->suspend_flag) {
			HDMITX_HDCP_INFO("%s, exit hdmirx, suspend, not propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		if (!tx_comm->hpd_state) {
			HDMITX_HDCP_INFO("%s, exit hdmirx, hpd low, not propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		if (!tx_comm->ready) {
			HDMITX_HDCP_INFO("%s, exit hdmirx, tx unready, not propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}

		/*
		 * when switch from hdmirx channel to hdmitx home
		 * need to start hdcp auth if it's not enabled
		 */
		if (hdmitx21_get_hdcp_mode(tx_comm) == 0) {
			HDMITX_HDCP_INFO("%s, exit hdmirx, need to enable hdcp\n", __func__);
			hdmitx21_enable_hdcp(tx_comm);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		ds_repeater = hdcptx_query_ds_repeater(p_hdcp);
		if (!ds_repeater || p_hdcp->hdcp_type != HDCP_VER_HDCP2X) {
			HDMITX_HDCP_INFO("[%d] not propagate stream type, ds_rpt:%d, ds_hdcp:%d\n",
				update_scene, ds_repeater, p_hdcp->hdcp_type);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		/*
		 * decide stream type to be sent according to
		 * downstream topology & current hdmitx mode
		 */
		if (p_hdcp->hdcp_topology.ds_hdcp1x_dev == 0 &&
			p_hdcp->hdcp_topology.ds_hdcp2x_dev == 0) {
			cur_content_type = 1;
		} else {
			/*
			 * customer special process: if 4k output && downstream
			 * contain non-hdcp2.2 device, then set stream type 1.
			 * it will cause NTS fail.
			 */
			/* if (is_current_4k_format()) */
				/* cur_content_type = 1; */
			/* else */
			cur_content_type = 0;
		}

		/*
		 * if current stream type not consistent with
		 * that to be sent, then send new stream management
		 */
		if ((p_hdcp->csm_message.streamid_type & 0xFF) != cur_content_type) {
			p_hdcp->csm_message.streamid_type =
				(p_hdcp->csm_message.streamid_type & 0xFF00) |
				cur_content_type;
			HDMITX_HDCP_INFO("case[%d] stream type change from %d to %d\n",
				update_scene, upstream_type, cur_content_type);
			if (p_hdcp->cont_smng_method == 0) {
				p_hdcp->csm_updated = true;
				hdcptx_update_csm(p_hdcp);
				hdcptx2_rpt_smng_xfer_start();
			} else if (p_hdcp->cont_smng_method == 1) {
				schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
			}
		}
	} else if (update_scene == UPSTREAM_ACTIVE) {
		/* update upstream type firstly */
		p_hdcp->saved_upstream_type = 0x10 | upstream_type;
		if (tx_comm->suspend_flag) {
			HDMITX_HDCP_INFO("%s, enter hdmirx, suspend, not propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		if (!tx_comm->hpd_state) {
			HDMITX_HDCP_INFO("%s, enter hdmirx, hpd low, not propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		if (!tx_comm->ready) {
			HDMITX_HDCP_INFO("%s, enter rx, tx unready, not propagate stream type\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}

		/* make sure hdcp is enabled */
		if (hdmitx21_get_hdcp_mode(tx_comm) == 0) {
			HDMITX_HDCP_INFO("%s, hdcp disabled, no need propagate stream type2\n",
				__func__);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		ds_repeater = hdcptx_query_ds_repeater(p_hdcp);
		if (!ds_repeater || p_hdcp->hdcp_type != HDCP_VER_HDCP2X) {
			HDMITX_HDCP_INFO("[%d] not propagate stream type, ds_rpt:%d, ds_hdcp:%d\n",
				update_scene, ds_repeater, p_hdcp->hdcp_type);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		cur_content_type = p_hdcp->csm_message.streamid_type & 0x00FF;
		/*
		 * if current stream type not consistent with upstream,
		 * re-auth with new stream type notified by upstream,
		 */
		if (upstream_type != cur_content_type) {
			p_hdcp->csm_message.streamid_type =
				(p_hdcp->csm_message.streamid_type & 0xFF00) |
				upstream_type;
			HDMITX_HDCP_INFO("case[%d] stream type change from %d to %d\n",
				update_scene, cur_content_type, upstream_type);
			if (p_hdcp->cont_smng_method == 0) {
				p_hdcp->csm_updated = true;
				hdcptx_update_csm(p_hdcp);
				hdcptx2_rpt_smng_xfer_start();
			} else {
				schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
			}
		}
	}
	mutex_unlock(&p_hdcp->stream_type_mutex);
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

/* note: it maybe used in timer */
u8 hdcptx_reauth_request(void *hdcptx_priv, u8 hdcp_version)
{
	u8 upstream_type = 0;
	u8 update_scene = 0;
	struct hdcptx21_core_priv *p_hdcp;

	if (!hdcptx_priv) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return 0;
	}
	p_hdcp = hdcptx_priv;

	HDMITX_HDCP_INFO("%s[%d] hdcp_ver 0x%x\n", __func__, __LINE__, hdcp_version);

	/* bit7~4: smng type notify, bit3~0: hdcp ver, 0:auto, 1:hdcp14, 2:hdcp22 */
	p_hdcp->rx_update_flag = hdcp_version;
	update_scene = hdcp_version & 0xf0;
	if (update_scene == 0x0) {
		/* first re-auth */
		hdcp_version &= 0xF;
		if (hdcp_version != 0 &&
			hdcp_version != 1 &&
			hdcp_version != 2)
			return 0;

		/*
		 * unmute for stream type:
		 * there're some special source devices, firstly do
		 * hdcp2.2 auth with hdmirx side and stream type 1,
		 * if hdmitx side connect hdcp1.4 TV, will do
		 * video/audio mute to block content stream.
		 * but then the source device switch to hdcp1.4 auth,
		 * hdmitx side should clear the video/audio mute
		 * for this case.
		 */
		if (p_hdcp->stream_mute) {
			p_hdcp->stream_mute = false;
			schedule_delayed_work(&p_hdcp->stream_mute_wk, 0);
		}
		p_hdcp->req_reauth_ver = hdcp_version;
		/* recovery default streamid_type, by default stream type = 0 */
		/* hdmitx21_rst_stream_type(p_hdcp); */
		schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
	} else if (update_scene == STREAMTYPE_UPDATE) {
		/* step1: mute if necessary */
		upstream_type = hdcp_version & 0xF;
		if (upstream_type == 1) {
			if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X ||
				p_hdcp->hdcp_topology.ds_hdcp1x_dev ||
				p_hdcp->hdcp_topology.ds_hdcp2x_dev) {
				/* this mute operation should have the highest priority */
				p_hdcp->stream_mute = true;
				schedule_delayed_work(&p_hdcp->stream_mute_wk, 0);
				HDMITX_HDCP_INFO("block stream to ds as upstream type = 1\n");
			}
		} else {
			/* this unmute operation should have the lowest priority */
			p_hdcp->stream_mute = false;
			schedule_delayed_work(&p_hdcp->stream_mute_wk, 0);
			HDMITX_DEBUG_HDCP("non-block stream to ds as upstream type = 0\n");
		}
		/*
		 * streamtype_dbg:
		 * 0: if ds is hdcp22 repeater, then propagate stream type
		 * 1: force to not re-auth, for debug
		 */
		if (streamtype_dbg == 1)
			return 0;

		/* step2: propagate upstream type to downstream */
		schedule_delayed_work(&p_hdcp->stream_type_wk, 0);
	} else if (update_scene == UPSTREAM_INACTIVE) {
		/* step1: unmute */
		p_hdcp->stream_mute = false;
		schedule_delayed_work(&p_hdcp->stream_mute_wk, 0);
		HDMITX_DEBUG_HDCP("upstream source inactive now, unmute\n");
		/* step2: set stream type as NTS requirement */
		schedule_delayed_work(&p_hdcp->stream_type_wk, 0);
	} else if (update_scene == UPSTREAM_ACTIVE) {
		/* almost the same as that of STREAMTYPE_UPDATE */
		upstream_type = hdcp_version & 0xF;
		/*
		 * case1: hdmirx channel->home->back to hdmirx channel:
		 * hdmirx will notify without request reauth
		 * if previous stream type is 1 and downstream contains
		 * non-hdcp2.2 compliance devices, then mute again
		 * case2: hdmirx input source hotplug/switch input, hdmirx
		 * port will close->open: hdmirx will request reauth
		 */
		/* step1: mute if needed */
		if (upstream_type == 1) {
			if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X ||
				p_hdcp->hdcp_topology.ds_hdcp1x_dev ||
				p_hdcp->hdcp_topology.ds_hdcp2x_dev) {
				/* this mute operation should have the highest priority */
				p_hdcp->stream_mute = true;
				schedule_delayed_work(&p_hdcp->stream_mute_wk, 0);
				HDMITX_DEBUG_HDCP("[2]block stream to ds as upstream type = 1\n");
			}
		} else {
			/*
			 * no need, UPSTREAM_INACTIVE/UPSTREAM_ACTIVE should be in pair
			 * except enter hdmirx input the first time(upstream_type = 0).
			 * and it's already unmute when notify UPSTREAM_INACTIVE
			 */
			/* this unmute operation should have the lowest priority */
			/* p_hdcp->stream_mute = false; */
			/* schedule_delayed_work(&p_hdcp->stream_mute_wk, 0); */
			/* pr_hdcp_info(L_2, "[2]non-block stream as upstream type = 0\n"); */
		}
		/* step2: propagate upstream type to downstream */
		schedule_delayed_work(&p_hdcp->stream_type_wk, 0);
	}
	return 1;
}

static void hdcp1x_process_intr(struct hdcptx21_core_priv *p_hdcp, u8 intr_reg)
{
	u8 hdcp1xcoppst, hdcp1xauthintst;
	u16 prime_ri;
	u8 bcaps_status;

	/* 0x63d */
	hdcp1xauthintst = intr_reg;
	/* 0x629 */
	hdcp1xcoppst = hdcptx1_copp_status_get();
	/* 0x222 0x223 */
	prime_ri = hdcptx1_get_prime_ri();
	HDMITX_DEBUG_HDCP("%s[%d] hdcp1xauthintst 0x%x hdcp1xcoppst 0x%x Ri 0x%x\n",
			__func__, __LINE__, hdcp1xauthintst, hdcp1xcoppst, prime_ri);
	if ((hdcp1xauthintst & BIT_TPI_INTR_ST0_BKSV_ERR) ||
		(hdcp1xauthintst & BIT_TPI_INTR_ST0_BKSV_BCAPS_ERR))
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_BKSV_RXID);
	if (hdcp1xauthintst & BIT_TPI_INTR_ST0_BKSV_BCAPS_DONE) {
		p_hdcp->update_topology = false;
		p_hdcp->update_topo_state = false;
		hdcp1x_check_bksv_done(p_hdcp);
	}
	if (hdcp1xauthintst & BIT_TPI_INTR_ST0_REAUTH_RI_MISMATCH) {
		p_hdcp->ds_repeater = false;
		switch (hdcp1xcoppst & (0x80 | 0x40)) {
		case 0x00:
			break;
		case 0x40:
			/* first part done */
			if (!hdcptx_query_ds_repeater(p_hdcp)) {
				p_hdcp->ds_repeater = false;
				/* R0 == R0' */
				hdcptx_authenticated_handle(p_hdcp);
				/* ds is receiver case */
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
			} else {
				p_hdcp->ds_repeater = true;
				p_hdcp->ds_auth = true;
				p_hdcp->update_topology = true;
				hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
				hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_rpt_auth,
					HDCP_DS_KSVLIST_RETRY_TIMER, 0);
			}
			/* clear fail reason after passed */
			p_hdcp->fail_reason = HDCP_FAIL_NONE;
			break;
		case(0x80 | 0x40):
			/* second part done */
			hdcptx_encryption_update(p_hdcp, true);
			/* g_prot secure -- downstream is repeater */
			p_hdcp->ds_repeater = true;
			p_hdcp->hdcp14_second_part_pass = true;
			HDMITX_HDCP_INFO("%s[%d] ds_repeater %d  rpt_ready %d\n",
				__func__, __LINE__,
				p_hdcp->ds_repeater, p_hdcp->rpt_ready);
			if (p_hdcp->rpt_ready) {
				p_hdcp->update_topo_state = false;
				hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
				/*
				 * ds is repeater case
				 * if set upstream READY for ksv list even V' invalid,
				 * it will fail hdcp1.4 repeater CTS 3C-II-05.
				 * only can notify upstream to set READY after
				 * downstream side auth pass with downstream repeater
				 */
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
			} else {
				p_hdcp->update_topo_state = true;
			}
			hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
			break;
		default:
			break;
		}
		switch (hdcptx1_ksv_v_get() & 0xc0) {
		case 0xc0:
			HDMITX_HDCP_INFO("hdcptx1: rx auth is pass and rx repeater auth begin\n");
			break;
		case 0x80:
			HDMITX_HDCP_INFO("hdcptx1: rx all auth is pass\n");
			break;
		case 0x40:
			HDMITX_HDCP_INFO("hdcptx1: rx auth is error\n");
			break;
		case 0x00:
		default:
			HDMITX_HDCP_INFO("hdcptx1: rx auth start again or ddc_gpu_tpi_granted\n");
			break;
		}
	}
	if (hdcp1xauthintst & BIT_TPI_INTR_ST0_REESTABLISH) {
		switch (hdcp1xcoppst & 0x30) {
		case 0x00:
		case 0x30:
			HDMITX_HDCP_INFO("hdcptx1: link normal or rsvd\n");
			break;
		case 0x10:
		case 0x20:
			HDMITX_HDCP_INFO("hdcptx1: link lost or reneg\n");
			hdcptx_update_failures(p_hdcp, (hdcp1xcoppst & 0x08));
			break;
		default:
			break;
		}
	}

	hdcptx1_bcaps_get(&bcaps_status);
	if (bcaps_status & 0x20) {
		hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
		hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
		hdcptx_topology_update(p_hdcp);
		if (!hdcptx_is_topology_correct(p_hdcp)) {
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
			return;
		}
	}

	if (hdcp1x_ds_ksv_fifo_ready(hdcp1xauthintst) || p_hdcp->hdcp14_second_part_pass) {
		if (p_hdcp->update_topology) {
			hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
			hdcptx_rpt_ready_process(p_hdcp, true);
			p_hdcp->update_topology = false;
			p_hdcp->update_topo_state = true;
			HDMITX_DEBUG_HDCP("%s[%d] update_topo_state %d\n", __func__,
				__LINE__, p_hdcp->update_topo_state);
			if (p_hdcp->update_topo_state) {
				p_hdcp->update_topo_state = false;
				hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
			}
			/*
			 * ds is repeater case, 1.only can notify upstream to set READY
			 * after second part succeed, otherwise 3C-II-05 will fail;
			 * 2.but for 1.4 repeater 3B-01b:Regular procedure With Repeater
			 * - DEVICE_COUNT=0, second part comes first before ksv fifo
			 * ready, here need to notify upstream side to update topo info
			 * 3.hdcp1.4 repeat 3C-II-06~09, still need to notify topo
			 * info to upstream side even if topo info exceed the maximum
			 */
			if (p_hdcp->hdcp14_second_part_pass ||
				p_hdcp->hdcp_topology.max_cas_exceed ||
				p_hdcp->hdcp_topology.max_devs_exceed)
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
		}
	}
}

void hdcp1x_intr_handler(struct intr_t *intr, void *intr_para)
{
	u8 hdcp14_intr_reg;
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)intr_para;
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	p_hdcp = tx_comm->hdcptx_priv;

	HDMITX_DEBUG_HDCP("%s[%d]\n", __func__, __LINE__);
	hdcp14_intr_reg = intr->st_data;
	/* clear intr state asap instead of clear after process intr */
	intr->st_data = 0;
	hdcp1x_process_intr(p_hdcp, hdcp14_intr_reg);
}

const char *msg1_info[] = {
	"ro_rpt_rcvid_changed",
	"ro_rpt_smng_changed",
	"ro_ake_sent_rcvd",
	"ro_ske_sent_rcvd",
	"ro_rpt_rcvid_xfer_done",
	"ro_rpt_smng_xfer_done",
	"ro_cert_sent_rcvd",
	"ro_gp3",
};

const char *msg2_info[] = {
	"ro_km_sent_rcvd",
	"ro_ekhkm_sent_rcvd",
	"ro_h_sent_rcvd",
	"ro_pair_sent_rcvd",
	"ro_lc_sent_rcvd",
	"ro_l_sent_rcvd",
	"ro_vack_sent_rcvd",
	"ro_mack_sent_rcvd",
};

#define MAX_SMNG_TIMES 15
static void hdcp2x_process_intr(struct hdcptx21_core_priv *p_hdcp, u8 int_reg[])
{
	int i;
	/* 0x803 auth status */
	u8 cp2tx_intr0_st = int_reg[0];
	/* 0x804 msg status */
	u8 cp2tx_intr1_st = int_reg[1];
	/* 0x805 */
	u8 cp2tx_intr2_st = int_reg[2];
	/* 0x806 */
	/* u8 cp2tx_intr3_st = int_reg[3]; */
	/* 0x80d */
	u8 cp2tx_state = hdcp2x_get_state_st();
	static int smng_times;

	HDMITX_DEBUG_HDCP("%s[%d] 0x%x 0x%x 0x%x 0x%x 0x%x\n", __func__, __LINE__,
		int_reg[0], int_reg[1], int_reg[2], int_reg[3], cp2tx_state);

	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_AUTH_DONE) {
		/* intr0 bit7 and bit0 needs to be 1, then start smng xfer */
		/* otherwise the 1B-09 will fail */
		/* if csm already update by upstream, don't transfer once more */
		/* mutex_lock(&p_hdcp->stream_type_mutex); */
		if ((cp2tx_intr0_st & BIT_CP2TX_INTR0_POLL_INTERVAL) &&
			p_hdcp->ds_repeater &&
			!p_hdcp->csm_updated) {
			p_hdcp->csm_updated = true;
			hdcptx_update_csm(p_hdcp);
			hdcptx2_rpt_smng_xfer_start();
		}
		/* mutex_unlock(&p_hdcp->stream_type_mutex); */
		if (hdcptx2_auth_status()) {
			if (!hdcptx_query_ds_repeater(p_hdcp)) {
				hdcptx_authenticated_handle(p_hdcp);
				/* ds is receiver case */
				schedule_delayed_work(&p_hdcp->ksv_notify_wk,
					msecs_to_jiffies(notify_delay_ms));
			} else {
				hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
				hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
				HDMITX_HDCP_INFO("hdcptx2: auth done\n");
			}
		}
		/* clear fail reason after passed */
		p_hdcp->fail_reason = HDCP_FAIL_NONE;
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_AUTH_FAIL) {
		hdcp2x_auth_stop(p_hdcp);
		hdcptx_update_failures(p_hdcp, p_hdcp->ds_repeater);
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_RPT_READY_CHANGE) {
		p_hdcp->ds_repeater = true;
		HDMITX_HDCP_INFO("hdcptx2: repeater ready\n");
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_REAUTH_REQ) {
		HDMITX_HDCP_INFO("hdcptx2: reauth req from ds device\n");
		/*
		 * need to re-init flag, so that to send stream type
		 * management message after re-auth done
		 */
		p_hdcp->csm_msg_sent = false;
		p_hdcp->csm_updated = false;
		p_hdcp->hdcp14_second_part_pass = false;
		/* reset ksv fifo pointer when re-auth */
		hdcptx_reset_ksv_fifo(p_hdcp);
		hdcp2x_reauth_start(p_hdcp);
		/* only needed on customer board for hdcp re-auth filter */
		/* hdcp_reauth_req = 1; */
	}

	for (i = 0; i < 8; i++) {
		if (cp2tx_intr1_st & (1 << i))
			HDMITX_HDCP_INFO("%s", msg1_info[i]);
	}
	for (i = 0; i < 8; i++) {
		if (cp2tx_intr2_st & (1 << i))
			HDMITX_HDCP_INFO("%s", msg2_info[i]);
	}

	if (cp2tx_intr1_st & BIT_CP2TX_INTR1_RPT_RCVID_CHANGED) {
		HDMITX_HDCP_INFO("hdcptx2: rcv_id changed");
		hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
		hdcptx_rpt_ready_process(p_hdcp, true);
		/* ds is repeater case */
		schedule_delayed_work(&p_hdcp->ksv_notify_wk, msecs_to_jiffies(notify_delay_ms));
	}
	/* BIT__HDCP2X_INTR0__AKE_SEND */
	if (cp2tx_intr1_st & BIT(2))
		p_hdcp->reauth_ignored = true;
	/* BIT__HDCP2X_INTR0__RPTR_SMNG_TRANS_DONE */
	if (cp2tx_intr1_st & BIT(5))
		hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
	/* BIT__HDCP2X_INTR0__CERT_RECV */
	if (cp2tx_intr1_st & BIT(6))
		p_hdcp->reauth_ignored = false;

	/* BIT__HDCP2X_INTR0__SKE_SEND */
	if (cp2tx_intr1_st & BIT(3)) {
		p_hdcp->reauth_ignored = false;
		smng_times = 0;
		if (hdcptx_query_ds_repeater(p_hdcp)) {
			/* down stream is a hdcp2.2 repeater */
			p_hdcp->ds_repeater = true;
			p_hdcp->ds_auth = true;
			hdcptx_encryption_update(p_hdcp, true);
			hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
			hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_rpt_auth,
				HDCP_RCVIDLIST_CHECK_TIMER, 0);
			/* update csm later, after got repeater topo info */
			/* hdcptx_check_ds_csm_status(p_hdcp); */
		} else {
			p_hdcp->ds_repeater = false;
		}
	}
	/* if (cp2tx_intr3_st & BIT(4))
	 *	;
	 */
	if (cp2tx_intr0_st & BIT(3)) {
		if (p_hdcp->cont_smng_method == 0) {
			/*
			 * Samsung Frame TV can't handle single RepeaterAuth_Stream_Manage message
			 * ( with content stream type different with previous stream type), and will
			 * cause M' hash fail.
			 * CTS will fail when retry directly, so send the smng times, the max times
			 * is 15 for safe, about 10 times will pass
			 */
			if (smng_times < MAX_SMNG_TIMES) {
				smng_times++;
				p_hdcp->csm_updated = true;
				hdcptx_update_csm(p_hdcp);
				hdcptx2_rpt_smng_xfer_start();
				HDMITX_HDCP_INFO("send smng_times %d\n", smng_times);
			} else {
				smng_times = 0;
				HDMITX_HDCP_INFO("hdcptx2: M' hash fail, restart auth\n");
				schedule_delayed_work(&p_hdcp->req_reauth_wk,
						msecs_to_jiffies(1000));
			}
		} else {
			schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
		}
	}
}

void hdcp2x_intr_handler(struct intr_t *intr, void *intr_para)
{
	u8 hdcp2_intr_reg[4];
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)intr_para;
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	p_hdcp = tx_comm->hdcptx_priv;

	intr_status_save_clr_cp2txs(hdcp2_intr_reg);
	hdcp2x_process_intr(p_hdcp, hdcp2_intr_reg);
}

static void hdcp1x_auth_start(struct hdcptx21_core_priv *p_hdcp)
{
	bool key_valid;

	HDMITX_DEBUG_HDCP("%s[%d]\n", __func__, __LINE__);
	p_hdcp->hdcp_type = HDCP_VER_HDCP1X;
	hdcptx_update_state(p_hdcp, HDCP_STAT_AUTH);
	hdcptx1_encryption_update(false);

	if (!p_hdcp->hdcp14_key_loaded) {
		key_valid = hdcptx1_load_key();
		if (!key_valid)
			HDMITX_ERROR("%s: hdcp1.4 key load failed!\n", __func__);
		else
			p_hdcp->hdcp14_key_loaded = true;
	} else {
		HDMITX_DEBUG("%s: hdcp1.4 key already loaded\n", __func__);
	}

	hdcptx1_auth_start();
	hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_bksv_poll_done,
		HDCP_BSKV_CHECK_TIMER, 0);
}

static void hdcp2x_auth_start(struct hdcptx21_core_priv *p_hdcp)
{
	u8 content_type;

	p_hdcp->hdcp_type = HDCP_VER_HDCP2X;
	if (p_hdcp->content_type == HDCP_CONTENT_TYPE_0)
		content_type = 0;
	else if (p_hdcp->content_type == HDCP_CONTENT_TYPE_1)
		content_type = 1;
	else
		content_type = 0xFF;
	HDMITX_DEBUG_HDCP("%s[%d] content_type %d %d\n", __func__, __LINE__,
		p_hdcp->content_type, content_type);

	hdcptx2_src_auth_start(content_type);

	hdcp2x_reauth_start(p_hdcp);
	hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_ddc_check_nak, 100, 200);
	hdcptx_update_state(p_hdcp, HDCP_STAT_AUTH);
}

static enum hdcp_ver_t hdcp_check_ds_hdcp2ver(struct hdcptx21_core_priv *p_hdcp)
{
	enum hdcp_ver_t hdcp_type = HDCP_VER_NONE;
	enum ddc_err_t ddc_err = DDC_ERR_NONE;
	u8 cap_val = 0;
	struct hdmitx_common *tx_comm = NULL;

	tx_comm = p_hdcp->bind_instance;

	ddc_err = hdmitx_ddcm_read(0, DDC_HDCP_DEVICE_ADDR, REG_DDC_HDCP_VERSION, &cap_val,
				   1, TPI_DDC_CMD_SEQUENTIAL_READ);

	if (ddc_err == DDC_ERR_NONE) {
		if (cap_val == 0x04)
			hdcp_type = HDCP_VER_HDCP2X;
		else
			hdcp_type = HDCP_VER_HDCP1X;
	} else {
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_DDC_NACK);
		hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_HDCP_I2C_ERROR);
	}
	return hdcp_type;
}

static void hdcp1x_auth_stop(struct hdcptx21_core_priv *p_hdcp)
{
	hdcptx1_auth_stop();
	hdcptx1_protection_enable(false);
	ddc_toggle_sw_tpi();
}

static void hdcp2x_auth_stop(struct hdcptx21_core_priv *p_hdcp)
{
	if (!p_hdcp) {
		HDMITX_ERROR("%s NULL hdcp_private instance\n", __func__);
		return;
	}
	hdcptx_stop_work(&p_hdcp->timer_ddc_check_nak);
	/*
	 * should always stop auth, otherwise the hw DDC may be
	 * free currently but will transact later,
	 * if do AON_CYP_CTL bit3 reset(DDC Master) during
	 * DDC transaction may pull SCL low.
	 * ddc busy check will add delay for ddc free before
	 * ddc stop
	 */
	hdcptx_ddc_check_busy();
	hdcptx2_auth_stop();
}

static void hdcp_reset_hw(struct hdcptx21_core_priv *p_hdcp)
{
	hdcptx_init_reg();
	hdcp1x_auth_stop(p_hdcp);
	hdcp2x_auth_stop(p_hdcp);
}

static void hdcptx_send_csm_msg(struct hdcptx21_core_priv *p_hdcp)
{
	if (p_hdcp->ds_repeater && p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		p_hdcp->csm_msg_sent = true;
		hdcptx2_csm_send(&p_hdcp->csm_message);
		p_hdcp->csm_message.seq_num_m++;
		if (p_hdcp->csm_message.seq_num_m > 0xFFFFFFUL)
			p_hdcp->csm_message.seq_num_m = 0;
		memcpy(&p_hdcp->prev_csm_message, &p_hdcp->csm_message, sizeof(struct hdcp_csm_t));
	}
	/* don't clear the stream type */
	/* hdmitx21_rst_stream_type(p_hdcp); */
}

static void hdcp2x_validate_csm_msg(struct hdcptx21_core_priv *p_hdcp)
{
	u8 content_type = (u8)(p_hdcp->csm_message.streamid_type & 0x00FF);

	if (content_type == 0x00)
		p_hdcp->content_type = HDCP_CONTENT_TYPE_0;
	else if (content_type == 0x01)
		p_hdcp->content_type = HDCP_CONTENT_TYPE_1;
	else
		p_hdcp->content_type = HDCP_CONTENT_TYPE_NONE;

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		if (!p_hdcp->rpt_ready)
			return;
		if (p_hdcp->ds_repeater)
			hdcptx_send_csm_msg(p_hdcp);
	}
}

static void hdcptx_update_csm(struct hdcptx21_core_priv *p_hdcp)
{
	hdcp2x_validate_csm_msg(p_hdcp);
}

static void hdcptx_reset(struct hdcptx21_core_priv *p_hdcp)
{
	/*!< attached is a repeater or not */
	p_hdcp->ds_repeater = false;
	p_hdcp->update_topology = false;
	p_hdcp->update_topo_state = false;
	p_hdcp->rpt_ready = false;
	p_hdcp->reauth_ignored = false;
	p_hdcp->csm_message.seq_num_m = 0;
	p_hdcp->hdcp_type = HDCP_VER_NONE;
	p_hdcp->hdcp_state = HDCP_STAT_NONE;
	p_hdcp->csm_valid = true;
	p_hdcp->csm_msg_sent = false;
	p_hdcp->csm_updated = false;
	p_hdcp->hdcp14_second_part_pass = false;
	/* recovery default streamid_type, by default stream type = 0 */
	hdmitx21_rst_stream_type(p_hdcp);

	hdcptx_stop_work(&p_hdcp->timer_hdcp_start);
	hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	hdcptx_stop_work(&p_hdcp->timer_hdcp_auth_fail_retry);
	hdcptx_stop_work(&p_hdcp->timer_bksv_poll_done);
	hdcptx_stop_work(&p_hdcp->timer_ddc_check_nak);
	hdcptx_stop_work(&p_hdcp->timer_update_csm);

	hdcptx_reset_ksv_fifo(p_hdcp);
	p_hdcp->ds_auth = false;
	hdcp_reset_hw(p_hdcp);

	hdcptx2_encryption_update(false);
}

static bool hdcptx_schedule_work(struct workqueue_struct *wq, struct hdcp_work *work,
	u32 delay_ms, u32 period_ms)
{
	if (!wq || !work) {
		HDMITX_ERROR("%s NULL workqueue or NULL work\n", __func__);
		return false;
	}

	HDMITX_DEBUG_HDCP("hdcptx: schedule %s: delay %d ms  period %d ms\n",
		work->name, delay_ms, period_ms);

	work->delay_ms = 0;
	work->period_ms = period_ms;

	if (delay_ms == 0 && period_ms == 0) {
		cancel_delayed_work(&work->dwork);
		return 1;
	}

	if (delay_ms)
		return queue_delayed_work(wq, &work->dwork,
				msecs_to_jiffies(delay_ms));
	else
		return queue_delayed_work(wq, &work->dwork,
				msecs_to_jiffies(period_ms));
}

/*
 * return true if work was pending and canceled, false otherwise
 * can't use cancel_delayed_work_sync in hdcptx_reset, as it may
 * lead to dead wait, for example: hdcptx_check_update_whandler will
 * call hdcptx_reset(and then will call hdcptx_stop_work), as a result
 * hdcptx_check_update_whandler will never exit
 */
static bool hdcptx_stop_work(struct hdcp_work *work)
{
	bool ret = cancel_delayed_work(&work->dwork);

	HDMITX_DEBUG_HDCP("hdcptx: stop %s\n", work->name);
	return ret;
}

/* return true if work was pending, false otherwise */
static bool hdcptx_stop_work_sync(struct hdcp_work *work)
{
	bool ret = cancel_delayed_work_sync(&work->dwork);

	HDMITX_DEBUG_HDCP("hdcptx: stop %s in sync\n", work->name);
	return ret;
}

static void hdcptx_auth_start(struct hdcptx21_core_priv *p_hdcp)
{
	enum hdcp_ver_t hdcp_mode;
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;

	hdcp_mode = p_hdcp->req_hdcp_ver;
	if (hdcp_mode != HDCP_VER_NONE) {
		hdcp_mode = p_hdcp->req_hdcp_ver;
		if (p_hdcp->hdcptx_enabled) {
			hdcptx_reset_ksv_fifo(p_hdcp);
			hdcp_enable_intr(1);
			hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_rcv_auth,
				HDCP_STAGE1_RETRY_TIMER, 0);
			if (hdcp_mode == HDCP_VER_HDCP1X) {
				hdcptx_en_aes_dual_pipe(false);
				hdcp1x_auth_start(p_hdcp);
			} else if (hdcp_mode == HDCP_VER_HDCP2X) {
				if (tx_comm->fmt_para.frl_rate == FRL_NONE)
					hdcptx_en_aes_dual_pipe(false);
				else
					hdcptx_en_aes_dual_pipe(true);
				hdcp2x_auth_start(p_hdcp);
			}
			hdcptx_schedule_work(p_hdcp->hdcp_wq,
				&p_hdcp->timer_ddc_check_nak, 100, 200);
		}
	}
}

static const char * const fail_string[] = {
	[HDCP_FAIL_NONE] = "none",
	[HDCP_FAIL_DDC_NACK] = "ddc_nack",
	[HDCP_FAIL_BKSV_RXID] = "bksv_rxid",
	[HDCP_FAIL_AUTH_FAIL] = "auth_fail",
	[HDCP_FAIL_READY_TO] = "ready_to",
	[HDCP_FAIL_V] = "v",
	[HDCP_FAIL_TOPOLOGY] = "topology",
	[HDCP_FAIL_RI] = "ri",
	[HDCP_FAIL_REAUTH_REQ] = "reauth_req",
	[HDCP_FAIL_CONTENT_TYPE] = "content_type",
	[HDCP_FAIL_AUTH_TIME_OUT] = "auth_time_out",
	[HDCP_FAIL_HASH] = "hash",
	[HDCP_FAIL_UNKNOWN] = "unknown",
};

static void hdcptx_update_failures(struct hdcptx21_core_priv *p_hdcp, enum hdcp_fail_types_t types)
{
	if (types > HDCP_FAIL_UNKNOWN) {
		types = HDCP_FAIL_UNKNOWN;
		dump_stack();
	}
	if (p_hdcp->fail_reason != types) {
		/* if fail type is DDC_NAK, and then comes BKSV_RXID, don't print */
		if (p_hdcp->fail_reason == HDCP_FAIL_DDC_NACK ||
			p_hdcp->fail_reason == HDCP_FAIL_BKSV_RXID) {
			HDMITX_DEBUG_HDCP("%s[%d] types: %s\n", __func__, __LINE__,
				fail_string[types]);
		} else {
			HDMITX_HDCP_INFO("%s[%d] types: %s\n", __func__, __LINE__,
				fail_string[types]);
		}
		p_hdcp->fail_reason = types;
	} else {
		HDMITX_DEBUG_HDCP("%s[%d] types: %s\n", __func__, __LINE__,
			fail_string[types]);
	}
	hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	if (p_hdcp->hdcptx_enabled) {
		if (types == HDCP_FAIL_CONTENT_TYPE) {
			p_hdcp->fail_type = types;
			hdcptx_update_state(p_hdcp, HDCP_STAT_FAILED);
		} else {
			p_hdcp->reauth_ignored = false;
			p_hdcp->fail_type = types;
			hdcptx_update_state(p_hdcp, HDCP_STAT_FAILED);
			hdcptx_reset_ksv_fifo(p_hdcp);
			hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_auth_fail_retry,
				HDCP_FAILED_RETRY_TIMER, 0);
		}
	}
}

/* hdcp receive/repeater share the same work handler */
static void hdcptx_check_ds_auth_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core_priv *p_hdcp = NULL;

	if (!work) {
		HDMITX_ERROR("%s null work\n", __func__);
		return;
	}

	HDMITX_DEBUG_HDCP("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);

	if (strcmp(work->name, "hdcp_rcv_auth") == 0)
		p_hdcp = container_of(work, struct hdcptx21_core_priv, timer_hdcp_rcv_auth);
	else if (strcmp(work->name, "hdcp_rpt_auth") == 0)
		p_hdcp = container_of(work, struct hdcptx21_core_priv, timer_hdcp_rpt_auth);
	if (!p_hdcp) {
		HDMITX_ERROR("%s null private_hdcp instance!\n", __func__);
		return;
	}

	hdcptx_update_failures(p_hdcp, HDCP_FAIL_AUTH_TIME_OUT);
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcptx_check_bksv_done_whandler(struct work_struct *w)
{
	u8 copp_data1;
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core_priv *p_hdcp = container_of(work, struct hdcptx21_core_priv,
		timer_bksv_poll_done);

	if (!p_hdcp) {
		HDMITX_ERROR("%s null private_hdcp instance!\n", __func__);
		return;
	}

	HDMITX_DEBUG_HDCP("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	copp_data1 = hdcptx1_ds_cap_status_get();

	if (copp_data1 & 0x02) {
		hdcptx_stop_work(work);
		if (hdcptx_is_bksv_valid())
			hdcptx1_protection_enable(true);
		else
			hdcptx_update_failures(p_hdcp, HDCP_FAIL_BKSV_RXID);
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcptx_check_update_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core_priv *p_hdcp = container_of(work, struct hdcptx21_core_priv,
		timer_hdcp_start);
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	tx_comm = p_hdcp->bind_instance;

	HDMITX_DEBUG_HDCP("%s [%s] period %d ms\n", __func__, work->name,
		work->period_ms);
	hdcptx_reset(p_hdcp);
	p_hdcp->fail_reason = HDCP_FAIL_NONE;
	if (hdcp_reauth_dbg == 2) {
		mdelay(delay_ms);
	} else if (hdcp_reauth_dbg == 3) {
		mdelay(delay_ms);
		hdcptx_reset(p_hdcp);
	}
	if (p_hdcp->hdcptx_enabled) {
		/* only read downstream hdcp version when hdcp2.2 is capable on source side */
		if (get_hdcp2_lstore(tx_comm))
			p_hdcp->hdcp_cap_ds = hdcp_check_ds_hdcp2ver(p_hdcp);
		else
			p_hdcp->hdcp_cap_ds = HDCP_VER_HDCP1X;
		if (p_hdcp->hdcp_cap_ds != HDCP_VER_NONE) {
			hdcptx_update_state(p_hdcp, HDCP_STAT_AUTH);
			hdcptx_auth_start(p_hdcp);
		}
	} else {
		hdcptx_update_state(p_hdcp, HDCP_STAT_NONE);
		p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
		p_hdcp->fail_type = HDCP_FAIL_NONE;
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcp_ddc_check_nack_whandler(struct work_struct *w)
{
	static int cnt;
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core_priv *p_hdcp = container_of(work, struct hdcptx21_core_priv,
		timer_ddc_check_nak);

	if (!p_hdcp) {
		HDMITX_ERROR("%s null private_hdcp instance!\n", __func__);
		return;
	}

	if (cnt % 128 == 0) {
		cnt++;
		HDMITX_DEBUG_HDCP("%s[%d] period %d ms\n", __func__, __LINE__,
			work->period_ms);
	}
	if (hdmi_ddc_status_check()) {
		p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
		hdcp2x_auth_stop(p_hdcp);
		hdcptx_update_failures(p_hdcp, HDCP_FAIL_DDC_NACK);
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcp_check_auth_fail_retry_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core_priv *p_hdcp = container_of(work, struct hdcptx21_core_priv,
		timer_hdcp_auth_fail_retry);

	if (!p_hdcp) {
		HDMITX_ERROR("%s null private_hdcp instance!\n", __func__);
		return;
	}

	HDMITX_DEBUG_HDCP("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	hdcptx_reset(p_hdcp);
	hdcptx_auth_start(p_hdcp);
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void hdcptx_update_csm_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core_priv *p_hdcp = container_of(work, struct hdcptx21_core_priv,
		timer_update_csm);

	if (!p_hdcp) {
		HDMITX_ERROR("%s null private_hdcp instance!\n", __func__);
		return;
	}

	HDMITX_DEBUG_HDCP("%s[%d] period %d ms\n", __func__, __LINE__,
		work->period_ms);
	hdcptx_update_csm(p_hdcp);

	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void init_hdcp_works(struct hdcp_work *work,
	void (*workfunc)(struct work_struct *), const char *name)
{
	INIT_DELAYED_WORK(&work->dwork, workfunc);
	work->name = name;
}

/* TODO, no mutex */
static void hdmitx21_set_hdcp_mode(struct hdmitx_common *tx_comm, const char *buf)
{
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}
	p_hdcp = (struct hdcptx21_core_priv *)tx_comm->hdcptx_priv;

	if (strncmp(buf, "f1", 2) == 0) {
		if (tx_comm->efuse_dis_hdcp_tx14) {
			HDMITX_ERROR("warning, efuse disable hdcptx14\n");
			return;
		}
		hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 1, true);
		tx_comm->hdcptx_comm.hdcp_mode = 0x1;
		hdcptx_mode_set(p_hdcp, 1);
	}
	if (strncmp(buf, "f2", 2) == 0) {
		if (tx_comm->efuse_dis_hdcp_tx22) {
			HDMITX_ERROR("warning, efuse disable hdcptx22\n");
			return;
		}
		hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 2, true);
		tx_comm->hdcptx_comm.hdcp_mode = 0x2;
		hdcptx_mode_set(p_hdcp, 2);
	}
	if (buf[0] == '0') {
		tx_comm->hdcptx_comm.hdcp_mode = 0x0;
		hdcptx_mode_set(p_hdcp, 0);
		hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 0, false);
	}
}

static int hdmitx21_get_hdcp_ver(struct hdmitx_common *tx_comm, char *buf, int len)
{
	int pos = 0;

	if (!tx_comm)
		return pos;

	if (!tx_comm->hpd_state) {
		HDMITX_HDCP_INFO("%s: hpd low, just return 14\n", __func__);
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "14\n\r");
		return pos;
	}
	if (tx_comm->hdcptx_comm.dw_hdcp22_cap) {
		pos += snprintf(buf + pos, PAGE_SIZE - pos, "22\n\r");
	} else {
		/*
		 * on hotplug case
		 * note that, when do hdcp repeater1.4 CTS,
		 * hdcp port access will affect item 3a-02 Irregular
		 * procedure: (First part of authentication) HDCP port
		 * access. Refer to hdcp1.4 cts spec: "If DUT does
		 * not read an HDCP register past 4 seconds after
		 * the previous attempt, then FAIL". after read hdcp
		 * version soon after plugin(access failed as TE
		 * not ack), our hdmitx side should keep retrying
		 * in 4S. but source TE start hdcp auth with
		 * hdmirx side too late(more than 4S), as hdcp auth
		 * of hdmitx side is started by hdmirx side, it will
		 * time out the access of hdcp port of 4 second.
		 * so for repeater CTS, should not read hdcp version
		 * whenever you want. Here add protect to only read
		 * hdcp version when currently not in hdmirx channel.
		 * special customer want to read downstream hdcp version
		 * after hotplug, and only send 4K output when EDID
		 * support 4K && support hdcp2.2. so add is_4k_sink()
		 * decision, it won't affect hdcp repeater CTS.
		 */
		if (hdcptx_need_ctrl_by_upstream(tx_comm->hdcptx_comm.hdcp_rpt_en)) {
			HDMITX_HDCP_INFO("%s: currently should not read hdcp version\n", __func__);
		} else if (hdmitx21_get_hdcp_mode(tx_comm) == 0) {
			if (get_hdcp2_lstore(tx_comm) && is_rx_hdcp2ver(tx_comm)) {
				pos += snprintf(buf + pos, PAGE_SIZE - pos, "22\n\r");
				tx_comm->hdcptx_comm.dw_hdcp22_cap = 1;
			}
			HDMITX_HDCP_INFO("%s: hdcp_mode: 0, rx_hdcp22_cap = %d\n",
				__func__, tx_comm->hdcptx_comm.dw_hdcp22_cap);
		}
	}
	/* Here, must assume RX support HDCP14, otherwise affect 1A-03 */
	pos += snprintf(buf + pos, PAGE_SIZE - pos, "14\n\r");

	return pos;
}

static int hdmitx21_get_tx_hdcp_cap(struct hdmitx_common *tx_comm)
{
	unsigned int hdcptx_cap = 0;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return 0;
	}

	/* check hdcp key load status */
	hdcptx_cap = hdcptx_get_key_store(tx_comm);
	return hdcptx_cap;
}

/*************DRM connector API**************/
void drm_hdmitx_start_hdcp_handler(struct work_struct *work)
{
	struct hdcptx21_core_priv *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core_priv, work_drm_start_hdcp);
	struct hdmitx_common *tx_comm;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL private hdcp struct\n", __func__);
		return;
	}

	tx_comm = p_hdcp->bind_instance;

	mutex_lock(&tx_comm->hdmimode_mutex);
	HDMITX_HDCP_INFO("%s: %d\n", __func__, tx_comm->hdcptx_comm.hdcp_mode);
	if (!tx_comm->ready || !tx_comm->hpd_state) {
		HDMITX_ERROR("%s hdmitx ready:%d, hpd:%d, skip hdcp auth\n",
			__func__, tx_comm->ready, tx_comm->hpd_state);
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	if (tx_comm->fmt_para.frl_rate) {
		HDMITX_HDCP_INFO("%s hdcp en for frl is on hdmitx side, skip\n", __func__);
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	switch (tx_comm->hdcptx_comm.hdcp_mode) {
	case HDCP_NULL:
		HDMITX_ERROR("%s enabled HDCP_NULL\n", __func__);
		break;
	case HDCP_MODE14:
	case HDCP_MODE22:
		hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type,
			tx_comm->hdcptx_comm.hdcp_mode, true);
		hdcptx_mode_set(p_hdcp, p_hdcp->bind_instance->hdcptx_comm.hdcp_mode);
		break;
	default:
		HDMITX_ERROR("%s unknown hdcp %d\n", __func__, tx_comm->hdcptx_comm.hdcp_mode);
		break;
	}
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

/* hdcp functions for linux/drm start */
/* should sync with hdmitx21_enable_hdcp() and hdmitx_start_hdcp_handler()
 * hdmi mode setting/hdcp enable or disable should be mutexed.
 * time delay may be needed after hdmi mode setting and before hdcp enable
 * so that hdcp auth is conducted under TV signal detection stable.
 */
static void drm_hdmitx_hdcp_enable(struct hdmitx_common *tx_comm, int hdcp_type)
{
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("NULL tx_comm or NULL hdcptx_comm instance\n");
		return;
	}
	p_hdcp = (struct hdcptx21_core_priv *)tx_comm->hdcptx_priv;

	mutex_lock(&tx_comm->hdmimode_mutex);
	HDMITX_HDCP_INFO("%s: %d\n", __func__, hdcp_type);
	if (!tx_comm->ready || !tx_comm->hpd_state) {
		HDMITX_ERROR("%s hdmitx ready:%d. hpd: %d, skip hdcp auth\n",
			__func__, tx_comm->ready, tx_comm->hpd_state);
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	if (tx_comm->fmt_para.frl_rate) {
		HDMITX_HDCP_INFO("%s hdcp en for frl is on hdmitx side, skip\n", __func__);
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	switch (hdcp_type) {
	case HDCP_NULL:
		HDMITX_ERROR("%s enabled HDCP_NULL\n", __func__);
		break;
	case HDCP_MODE14:
		/* hdcp1.4 auth may pass->fail->pass as hdmi signal detection
		 * need some time on TV side, so need postpone auth time to
		 * wait TV side signal detection stable
		 */
		msleep(100);
		hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 1, true);
		tx_comm->hdcptx_comm.hdcp_mode = 0x1;
		hdcptx_mode_set(p_hdcp, 1);
		break;
	case HDCP_MODE22:
		hdmitx21_ctrl_hdcp_gate(tx_comm->tx_hw->chip_data->chip_type, 2, true);
		tx_comm->hdcptx_comm.hdcp_mode = 0x2;
		hdcptx_mode_set(p_hdcp, 2);
		break;
	default:
		HDMITX_ERROR("%s unknown hdcp %d\n", __func__, hdcp_type);
		break;
	}
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

static void drm_hdmitx_hdcp_disable(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm;
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL private_hdcp instance\n", __func__);
		return;
	}
	hdcptx_comm = &tx_comm->hdcptx_comm;
	p_hdcp = tx_comm->hdcptx_priv;

	mutex_lock(&tx_comm->hdmimode_mutex);
	hdmitx21_disable_hdcp(p_hdcp);
	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	HDMITX_HDCP_INFO("%s\n", __func__);
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

static void drm_hdmitx_hdcp_disconnect(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	mutex_lock(&tx_comm->hdmimode_mutex);
	/* hdcp is disabled when hdmitx driver plugout, no need disable again */
	tx_comm->hdcptx_comm.hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	tx_comm->hdcptx_comm.hdcp_fail_cnt = 0;
	HDMITX_HDCP_INFO("%s\n", __func__);
	mutex_unlock(&tx_comm->hdmimode_mutex);
}

static void drm_hdmitx_hdcp_init(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;

	if (!tx_comm || !hdcptx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	hdcptx_comm->tx_hdcp_cb.data = NULL;
	hdcptx_comm->tx_hdcp_cb.hdcp_notify = NULL;

	HDMITX_DEBUG_HDCP("%s\n", __func__);
}

static void drm_hdmitx_hdcp_exit(struct hdmitx_common *tx_comm)
{
	struct hdcptx_common *hdcptx_comm = &tx_comm->hdcptx_comm;

	if (!tx_comm || !hdcptx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return;
	}

	hdcptx_comm->hdcp_auth_result = HDCP_AUTH_UNKNOWN;
	hdcptx_comm->hdcp_fail_cnt = 0;
	hdcptx_comm->tx_hdcp_cb.data = NULL;
	hdcptx_comm->tx_hdcp_cb.hdcp_notify = NULL;
	HDMITX_DEBUG_HDCP("%s\n", __func__);
}

/* hdcp functions for linux/drm end */

static int hdmitx_validate_hdcp_key(struct hdmitx_common *tx_comm, int hdcp_mode)
{
	int ret = 0;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance\n", __func__);
		return ret;
	}

	if (hdcp_mode == 2) {
		/* consider hdcp2.2 key always valid if key length match */
		if (get_hdcp2_lstore(tx_comm))
			ret = 1;
	} else if (hdcp_mode == 1) {
		if (get_hdcp1_lstore(tx_comm))
			ret = hdmitx_hw_cntl(tx_comm->tx_hw, HDCP14_KEY_VALIDATE, NULL, NULL);
	}

	return ret;
}

static void hdcptx_21_ctrl_ops_init(struct hdcptx_common *hdcptx_comm)
{
	if (!hdcptx_comm) {
		HDMITX_ERROR("NULL hdcptx_comm instance\n");
		return;
	}

	hdcptx_comm->drm_hdcp_init = drm_hdmitx_hdcp_init;
	hdcptx_comm->drm_hdcp_exit = drm_hdmitx_hdcp_exit;
	hdcptx_comm->drm_hdcp_enable = drm_hdmitx_hdcp_enable;
	hdcptx_comm->drm_hdcp_disable = drm_hdmitx_hdcp_disable;
	hdcptx_comm->drm_hdcp_disconnect = drm_hdmitx_hdcp_disconnect;

	hdcptx_comm->set_hdcp_mode = hdmitx21_set_hdcp_mode;
	hdcptx_comm->get_hdcp_ver = hdmitx21_get_hdcp_ver;
	hdcptx_comm->get_tx_hdcp_cap = hdmitx21_get_tx_hdcp_cap;
}

static int hdcp_communicate_open(struct inode *inode, struct file *file)
{
	struct hdcptx_common *hdcptx_comm = container_of(file->private_data,
		struct hdcptx_common, hdcp_misc_dev);

	file->private_data = hdcptx_comm;
	return 0;
}

static int hdcp_communicate_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long hdcp_communicate_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int ret;
	u8 hdcp_key_store = 0;
	int key_valid;
	int hdcp_key = 0;
	struct hdcptx_common *hdcptx_comm = file->private_data;
	struct hdmitx_common *tx_comm;
	struct hdcptx21_core_priv *p_hdcp;
	void __user *argp = (void __user *)arg;
	void *cb_data;

	if (!hdcptx_comm) {
		HDMITX_ERROR("%s NULL hdcptx_comm instance\n", __func__);
		return -EINVAL;
	}
	tx_comm = container_of(hdcptx_comm, struct hdmitx_common, hdcptx_comm);
	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return -EINVAL;
	}
	p_hdcp = (struct hdcptx21_core_priv *)tx_comm->hdcptx_priv;
	cb_data = tx_comm->hdcptx_comm.tx_hdcp_cb.data;

	switch (cmd) {
	case TEE_HDCP_IOC_START:
		/* notify by TEE, hdcp key ready */
		ret = 0;
		if (get_hdcp2_lstore(tx_comm))
			hdcp_key_store |= BIT(1);
		if (get_hdcp1_lstore(tx_comm))
			hdcp_key_store |= BIT(0);
		HDMITX_HDCP_INFO("tee load hdcp key ready: 0x%x\n", hdcp_key_store);
		/* for linux platform, notify hdcp key load ready to drm side */
		if (tx_comm->hdcptx_comm.hdcp_ctl_lvl > 0) {
			if (tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify) {
				tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify(cb_data,
					HDCP_KEY_UPDATE, HDCP_AUTH_UNKNOWN);
				HDMITX_DEBUG("notify hdcp key load done to drm\n");
			}
			return ret;
		}
		mutex_lock(&tx_comm->hdmimode_mutex);
		if (tx_comm->hpd_state == 1 &&
			tx_comm->ready &&
			hdmitx21_get_hdcp_mode(tx_comm) == 0) {
			HDMITX_HDCP_INFO("hdmi ready but hdcp not enabled, enable now\n");
			if (hdcptx_need_ctrl_by_upstream(tx_comm->hdcptx_comm.hdcp_rpt_en)) {
				HDMITX_HDCP_INFO("currently hdcp should started by upstream\n");
			} else {
				if (hdcp_key_store & BIT(1))
					tx_comm->hdcptx_comm.dw_hdcp22_cap =
						is_rx_hdcp2ver(tx_comm);
				hdmitx21_enable_hdcp(tx_comm);
			}
		}
		mutex_unlock(&tx_comm->hdmimode_mutex);
		break;
	case TEE_HDCP_IOC_VALIDATE_KEY:
		mutex_lock(&tx_comm->hdmimode_mutex);
		if (copy_from_user(&hdcp_key, argp, _IOC_SIZE(cmd))) {
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return -EINVAL;
		}
		/* 14/1: hdcp1.4 key, 22/2: hdcp2.2 key */
		if (hdcp_key == 14 || hdcp_key == 1)
			key_valid = hdmitx_validate_hdcp_key(tx_comm, 0x1);
		else if (hdcp_key == 22 || hdcp_key == 2)
			key_valid = hdmitx_validate_hdcp_key(tx_comm, 0x2);
		else
			key_valid = 0;
		HDMITX_HDCP_INFO("%ul key valid:%d\n", arg, key_valid);
		ret = copy_to_user((void __user *)arg, (void *)&key_valid, sizeof(key_valid));
		if (ret != 0)
			ret = -EFAULT;
		mutex_unlock(&tx_comm->hdmimode_mutex);
		break;
	default:
		ret = -EPERM;
		break;
	}
	return ret;
}

static const struct file_operations hdcp_comm_file_operations = {
	.owner = THIS_MODULE,
	.open = hdcp_communicate_open,
	.release = hdcp_communicate_release,
	.unlocked_ioctl = hdcp_communicate_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = hdcp_communicate_ioctl,
#endif
};

int hdmitx21_hdcp_init(struct hdmitx_common *tx_comm)
{
	struct hdcptx21_core_priv *p_hdcp = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("NULL tx_comm instance\n");
		return 1;
	}
	p_hdcp = kzalloc(sizeof(*p_hdcp), GFP_KERNEL);
	if (!p_hdcp) {
		HDMITX_ERROR("%s no enough mem for hdcp private struct\n", __func__);
		return 1;
	}

	HDMITX_DEBUG_HDCP("%s[%d]\n", __func__, __LINE__);
	/* hdcp private struct init */
	p_hdcp->bind_instance = tx_comm;
	p_hdcp->hdcp_state = HDCP_STAT_NONE;
	p_hdcp->hdcp_type = HDCP_VER_NONE;
	p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
	p_hdcp->ds_auth = false;
	p_hdcp->ds_repeater = false;
	p_hdcp->fail_type = HDCP_FAIL_NONE;
	p_hdcp->fail_reason = HDCP_FAIL_NONE;
	p_hdcp->req_hdcp_ver = HDCP_VER_NONE;
	p_hdcp->reauth_ignored = false;
	p_hdcp->encryption_enabled = false;
	p_hdcp->content_type = HDCP_CONTENT_TYPE_0;
	p_hdcp->cont_smng_method = 0;
	hdmitx21_rst_stream_type(p_hdcp);

	p_hdcp->p_ksv_lists =
		kmalloc((HDCP1X_MAX_TX_DEV_SRC + 1) * sizeof(struct hdcp_ksv_t), GFP_KERNEL);

	/* hdcp related work, specific for auth flow in HW core */
	p_hdcp->hdcp_wq = alloc_workqueue(DEVICE_NAME, WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	init_hdcp_works(&p_hdcp->timer_hdcp_rcv_auth,
		hdcptx_check_ds_auth_whandler, "hdcp_rcv_auth");
	init_hdcp_works(&p_hdcp->timer_hdcp_rpt_auth,
		hdcptx_check_ds_auth_whandler, "hdcp_rpt_auth");
	init_hdcp_works(&p_hdcp->timer_bksv_poll_done,
		hdcptx_check_bksv_done_whandler, "bksv_poll_done");
	init_hdcp_works(&p_hdcp->timer_hdcp_start,
		hdcptx_check_update_whandler, "hdcp_start");
	init_hdcp_works(&p_hdcp->timer_ddc_check_nak,
		hdcp_ddc_check_nack_whandler, "ddc_check_nak");
	init_hdcp_works(&p_hdcp->timer_hdcp_auth_fail_retry,
		hdcp_check_auth_fail_retry_whandler, "hdcp_auth_fail_retry");
	init_hdcp_works(&p_hdcp->timer_update_csm, hdcptx_update_csm_whandler, "update_csm");
	INIT_DELAYED_WORK(&p_hdcp->ksv_notify_wk, hdcptx_notify_rpt_info);
	INIT_DELAYED_WORK(&p_hdcp->req_reauth_wk, hdcptx_req_reauth_whandler);
	INIT_DELAYED_WORK(&p_hdcp->stream_mute_wk, hdcp_stream_mute_whandler);
	INIT_DELAYED_WORK(&p_hdcp->stream_type_wk, hdmitx_propagate_stream_type);

	/* hdcp related work for start hdcp auth, scheduled in system workqueue */
	mutex_init(&p_hdcp->hdcptx_auth_mutex);
	INIT_DELAYED_WORK(&p_hdcp->work_tx_start_hdcp, hdmitx_start_hdcp_handler);
	INIT_DELAYED_WORK(&p_hdcp->work_up_hdcp_timeout, hdmitx_up_hdcp_timeout_handler);
	INIT_DELAYED_WORK(&p_hdcp->work_drm_start_hdcp, drm_hdmitx_start_hdcp_handler);
	mutex_init(&p_hdcp->stream_type_mutex);

	/* hdcp common struct init */
	hdcptx_21_ctrl_ops_init(&tx_comm->hdcptx_comm);
	tx_comm->hdcptx_priv = (void *)p_hdcp;
	tee_comm_dev_reg(&tx_comm->hdcptx_comm.hdcp_misc_dev, &hdcp_comm_file_operations);
	tx_comm->hdcptx_comm.task_hdcp = kthread_run(hdmitx_hdcp_stat_monitor_task,
		(void *)tx_comm, "kthread_hdcp");

	return 0;
}

void hdmitx21_hdcp_uninit(struct hdmitx_common *tx_comm)
{
	struct hdcptx21_core_priv *p_hdcp;

	if (!tx_comm || !tx_comm->hdcptx_priv) {
		HDMITX_ERROR("NULL tx_comm or NULL hdcptx_comm instance\n");
		return;
	}

	p_hdcp = tx_comm->hdcptx_priv;
	tee_comm_dev_unreg(&tx_comm->hdcptx_comm.hdcp_misc_dev);
	hdmitx21_disable_hdcp(p_hdcp);
	kthread_stop(tx_comm->hdcptx_comm.task_hdcp);
	destroy_workqueue(p_hdcp->hdcp_wq);
	tx_comm->hdcptx_priv = NULL;
	kfree(p_hdcp->p_ksv_lists);
	kfree(p_hdcp);
}

void hdmitx_start_hdcp_handler(struct work_struct *work)
{
	struct hdcptx21_core_priv *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core_priv, work_tx_start_hdcp);
	struct hdmitx_common *tx_comm;
	unsigned long timeout_sec;
	u32 arg = 0;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}

	tx_comm = p_hdcp->bind_instance;

	if (hdcptx_need_ctrl_by_upstream(tx_comm->hdcptx_comm.hdcp_rpt_en)) {
		/* move is_passthrough_switch to p_hdcp */
		if (p_hdcp->is_passthrough_switch) {
			HDMITX_INFO("enable hdcp by passthrough switch mode\n");
			/* note: hdcp should only be started when hdmi signal ready */
			mutex_lock(&tx_comm->hdmimode_mutex);
			if (!tx_comm->ready || !tx_comm->hpd_state) {
				HDMITX_INFO("signal ready: %d, hpd_state: %d, eixt hdcp\n",
					tx_comm->ready, tx_comm->hpd_state);
				p_hdcp->is_passthrough_switch = 0;
				mutex_unlock(&tx_comm->hdmimode_mutex);
				return;
			}
			arg = CLR_AVMUTE;
			hdmitx_hw_cntl(tx_comm->tx_hw, AUX_PKT_CONFIG_AVMUTE, (void *)&arg, NULL);
			hdmitx21_enable_hdcp(tx_comm);
			mutex_unlock(&tx_comm->hdmimode_mutex);
		} else {
			/* for source->hdmirx->hdmitx->tv, plug on tv side */
			/*
			 * 1.for repeater CTS, only start hdcp by upstream side
			 * 2.however if upstream source side no signal output
			 * or never start hdcp auth with hdmirx(such as PC),
			 * then we add 5S timeout period, after 5S timeout,
			 * it means that no input source start hdcp auth with
			 * hdmirx, or "no signal" on hdmirx side, then hdmitx
			 * side will start hdcp auth itself.
			 * thus both 1/2 will be satisfied.
			 */
			HDMITX_INFO("hdcp should started by upstream, wait...\n");
			/* timeout period: hdcp1.4: 5S, hdcp2.2: 2S */
			if (tx_comm->hdcptx_comm.dw_hdcp22_cap)
				timeout_sec = 2;
			else
				timeout_sec = p_hdcp->up_hdcp_timeout_sec;
			schedule_delayed_work(&p_hdcp->work_up_hdcp_timeout,
					msecs_to_jiffies(timeout_sec * 1000));
		}
	} else {
		mutex_lock(&tx_comm->hdmimode_mutex);
		if (!tx_comm->ready || !tx_comm->hpd_state) {
			HDMITX_INFO("signal ready: %d, hpd_state: %d, eixt hdcp2\n",
				tx_comm->ready, tx_comm->hpd_state);
			p_hdcp->is_passthrough_switch = 0;
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		arg = CLR_AVMUTE;
		hdmitx_hw_cntl(tx_comm->tx_hw, AUX_PKT_CONFIG_AVMUTE, (void *)&arg, NULL);
		hdmitx21_enable_hdcp(tx_comm);
		mutex_unlock(&tx_comm->hdmimode_mutex);
	}
	/* clear after start hdcp */
	p_hdcp->is_passthrough_switch = 0;
}

void hdmitx_up_hdcp_timeout_handler(struct work_struct *work)
{
	struct hdcptx21_core_priv *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core_priv, work_up_hdcp_timeout);
	struct hdmitx_common *tx_comm;
	u32 arg = 0;

	if (!p_hdcp || !p_hdcp->bind_instance) {
		HDMITX_ERROR("%s NULL tx_comm or NULL hdcp_private instance\n", __func__);
		return;
	}

	tx_comm = p_hdcp->bind_instance;

	if (hdcptx_need_ctrl_by_upstream(tx_comm->hdcptx_comm.hdcp_rpt_en)) {
		HDMITX_INFO("enable hdcp as wait upstream hdcp timeout\n");
		/* note: hdcp should only be started when hdmi signal ready */
		mutex_lock(&tx_comm->hdmimode_mutex);
		if (!tx_comm->ready || !tx_comm->hpd_state) {
			HDMITX_INFO("signal ready: %d, hpd_state: %d, exit hdcp\n",
				tx_comm->ready, tx_comm->hpd_state);
			mutex_unlock(&tx_comm->hdmimode_mutex);
			return;
		}
		arg = CLR_AVMUTE;
		hdmitx_hw_cntl(tx_comm->tx_hw, AUX_PKT_CONFIG_AVMUTE, (void *)&arg, NULL);
		hdmitx21_enable_hdcp(tx_comm);
		mutex_unlock(&tx_comm->hdmimode_mutex);
	} else {
		HDMITX_INFO("wait upstream hdcp timeout, but now not in hdmirx channel\n");
	}
}
