// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/amlogic/media/vout/hdmi_tx_repeater.h>
#include "dptx_log.h"
#include "dptx_hw.h"
#include "dptx_hdcp_hw.h"
#include "hdmi_rx_repeater.h"
#include "hdmitx_audio.h"
#include "../../hdcp/hdmitx_hdcp.h"
#include "../../meson_tx_task_mgr.h"

#define DEVICE_NAME_DP "dptx"

static unsigned int delay_ms = 10;
unsigned long dptx_hdcp_reauth_dbg = 1;
/* notify delay in ms, for debug */
static int notify_delay_ms = 1;

#define TEE_HDCP_IOC_START _IOW('P', 0, int)
#define TEE_HDCP_IOC_VALIDATE_KEY _IOWR('P', 0x1, int)

static void dptx_enable_hdcp(struct hdcptx21_core *p_hdcp);
static void dptx_hdcptx_reset(struct hdcptx21_core *p_hdcp);
static void dptx_hdcptx_reset_ksv_fifo(struct hdcptx21_core *p_hdcp);
static bool dptx_hdcptx_schedule_work(struct workqueue_struct *wq, struct hdcp_work *work,
	u32 delay_ms, u32 period_ms);
static void dptx_hdcptx_update_state(struct hdcptx21_core *p_hdcp, enum hdcp_stat_t state);
static u32 dptx_get_hdcptx_mode(void *hdcptx_instance);
static void dptx_hdcptx_update_csm(struct hdcptx21_core *p_hdcp);
static bool dptx_hdcptx_query_ds_repeater(struct hdcptx21_core *p_hdcp);
static void dptx_hdcptx_update_failures(struct hdcptx21_core *p_hdcp, enum hdcp_fail_types_t types);
static void dptx_hdcptx_encryption_update(struct hdcptx21_core *p_hdcp, bool en);

static bool dptx_hdcptx_need_ctrl_by_upstream(bool hdcp_rpt_en);
static void dptx_hdcptx_mode_set(struct hdcptx21_core *p_hdcp, unsigned int mode);

static const char * const dptx_fail_string[] = {
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

/* below code same as meson_tx_connector/hdcp/hdmitx_hdcp.c */
static int dptx_hdcp_communicate_open(struct inode *inode, struct file *file)
{
	struct hdcptx_common *hdcptx_comm = container_of(file->private_data,
		struct hdcptx_common, hdcp_misc_dev);

	file->private_data = hdcptx_comm;
	return 0;
}

static int dptx_hdcp_communicate_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

void dptx_rst_stream_type(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp)
		return;
	p_hdcp->csm_message.streamid_type = p_hdcp->def_stream_type;
}

static void dptx_hdcptx_send_csm_msg(struct hdcptx21_core *p_hdcp)
{
	if (p_hdcp->ds_repeater && p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		p_hdcp->csm_msg_sent = true;
		dptx_hdcptx2_csm_send(p_hdcp->bind_hdcptx_comm->bind_hw_comm, &p_hdcp->csm_message);
		p_hdcp->csm_message.seq_num_m++;
		if (p_hdcp->csm_message.seq_num_m > 0xFFFFFFUL)
			p_hdcp->csm_message.seq_num_m = 0;
		memcpy(&p_hdcp->prev_csm_message, &p_hdcp->csm_message, sizeof(struct hdcp_csm_t));
	}
	/* don't clear the stream type */
	/* hdmitx21_rst_stream_type(p_hdcp); */
}

static void dptx_hdcp2x_validate_csm_msg(struct hdcptx21_core *p_hdcp)
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
			dptx_hdcptx_send_csm_msg(p_hdcp);
	}
}

static void dptx_hdcptx_update_csm(struct hdcptx21_core *p_hdcp)
{
	dptx_hdcp2x_validate_csm_msg(p_hdcp);
}

static int dptx_validate_hdcp_key(struct hdcptx_common *hdcptx_comm, int hdcp_type)
{
	int ret = 0;
	int set_mode_ret = 0;
	u32 hdcp14_gate_status = 0;

	if (!hdcptx_comm) {
		DPTX_ERROR("NULL hdcp_comm instance\n");
		return 0;
	}

	if (hdcp_type == HDCP_VER_HDCP2X) {
		/* consider hdcp2.2 key always valid if key length match */
		if (dptx_hdcptx22_topo_ctrl(hdcptx_comm, GET_HDCP22_TOPO, 0))
			ret = 1;
	} else if (hdcp_type == HDCP_VER_HDCP1X) {
		if (dptx_get_hdcptx_lstore(hdcptx_comm, HDCP_VER_HDCP1X)) {
			/* enable tmds clk */
			if (hdcptx_comm->tmds_enable)
				set_mode_ret = hdcptx_comm->tmds_enable(hdcptx_comm->bind_tx_comm);
			if (set_mode_ret < 0)
				return -1;
			/* enable hdcp1.4 gate if gate is disabled */
			if (hdcptx_comm->chip_type >= MESON_CPU_ID_S7) {
				hdcp14_gate_status =
					dptx_get_gate_status(hdcptx_comm->bind_hw_comm) &
					BIT_HDMITX_TOP_CLK_GATE_HDCP1X;
				if (!hdcp14_gate_status)
					dptx_ctrl_hdcp_gate(hdcptx_comm, 1, true);
			}
			/* load key into HW for crc check */
			ret = dptx_hdcptx1_load_key();
			/* recover disable tmds clk */
			if (set_mode_ret == 0) {
				if (hdcptx_comm->tmds_disable)
					hdcptx_comm->tmds_disable(hdcptx_comm->bind_tx_comm);
			}
			/* recover hdcp1.4 gate status */
			//todo chip type
			if (hdcptx_comm->chip_type >= MESON_CPU_ID_S7) {
				if (!hdcp14_gate_status)
					dptx_ctrl_hdcp_gate(hdcptx_comm, 1, false);
			}
		}
	}
	return ret;
}

static long dptx_hdcp_communicate_ioctl(struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int ret;
	u8 hdcp_key_store = 0;
	int key_valid;
	int hdcp_key = 0;
	int hdcprx_ver = 0;
	struct hdcptx21_core *p_hdcp = file->private_data;
	void __user *argp = (void __user *)arg;
	void *cb_data;
	struct hdcptx_common *hdcp_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL p_hdcp instance\n");
		return -EINVAL;
	}

	cb_data = p_hdcp->bind_hdcptx_comm->tx_hdcp_cb.data;
	hdcp_comm = p_hdcp->bind_hdcptx_comm;

	switch (cmd) {
	case TEE_HDCP_IOC_START:
		/* notify by TEE, hdcp key ready */
		ret = 0;
		if (dptx_get_hdcptx_lstore(hdcp_comm, HDCP_VER_HDCP2X))
			hdcp_key_store |= BIT(1);
		if (dptx_get_hdcptx_lstore(hdcp_comm, HDCP_VER_HDCP1X))
			hdcp_key_store |= BIT(0);
		DPTX_HDCP_INFO("tee load hdcp key ready: 0x%x\n", hdcp_key_store);
		/* for linux platform, notify hdcp key load ready to drm side */
		if (hdcp_comm->hdcp_ctl_lvl > 0) {
			if (hdcp_comm->tx_hdcp_cb.hdcp_notify) {
				hdcp_comm->tx_hdcp_cb.hdcp_notify(cb_data,
					HDCP_KEY_UPDATE, HDCP_AUTH_UNKNOWN);
				DPTX_DEBUG("notify hdcp key load done to drm\n");
			}
			return ret;
		}
		if (hdcp_comm->hpd_state == 1 &&
			hdcp_comm->ready &&
			dptx_get_hdcptx_mode(p_hdcp) == 0) {
			DPTX_HDCP_INFO("hdmi ready but hdcp not enabled, enable now\n");
			if (dptx_hdcptx_need_ctrl_by_upstream(hdcp_comm->hdcp_rpt_en)) {
				DPTX_HDCP_INFO("currently hdcp should started by upstream\n");
			} else {
				if (hdcp_key_store & BIT(1)) {
					hdcprx_ver =
						dptx_hdcptx_get_hdcprx_ver(hdcp_comm);
					p_hdcp->bind_hdcptx_comm->dw_hdcp22_cap =
						(hdcprx_ver == 2) ? true : false;
				}
				dptx_enable_hdcp(p_hdcp);
			}
		}
		break;
	case TEE_HDCP_IOC_VALIDATE_KEY:
		if (copy_from_user(&hdcp_key, argp, _IOC_SIZE(cmd)))
			return -EINVAL;
		/* 14/1: hdcp1.4 key, 22/2: hdcp2.2 key */
		if (hdcp_key == 14 || hdcp_key == 1)
			key_valid = dptx_validate_hdcp_key(hdcp_comm, 0x1);
		else if (hdcp_key == 22 || hdcp_key == 2)
			key_valid = dptx_validate_hdcp_key(hdcp_comm, 0x2);
		else
			key_valid = 0;
		DPTX_HDCP_INFO("%ul key valid:%d\n", arg, key_valid);
		ret = copy_to_user((void __user *)arg, (void *)&key_valid, sizeof(key_valid));
		if (ret != 0)
			ret = -EFAULT;
		break;
	default:
		ret = -EPERM;
		break;
	}

	return ret;
}

static const struct file_operations dptx_hdcp_comm_file_operations = {
	.owner = THIS_MODULE,
	.open = dptx_hdcp_communicate_open,
	.release = dptx_hdcp_communicate_release,
	.unlocked_ioctl = dptx_hdcp_communicate_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = dptx_hdcp_communicate_ioctl,
#endif
};

static void dptx_init_hdcp_works(struct hdcp_work *work,
	void (*workfunc)(struct work_struct *), const char *name)
{
	INIT_DELAYED_WORK(&work->dwork, workfunc);
	work->name = name;
}

/* return true if work was pending, false otherwise */
static bool dptx_hdcptx_stop_work_sync(struct hdcp_work *work)
{
	bool ret = cancel_delayed_work_sync(&work->dwork);

	DPTX_DEBUG_HDCP("hdcptx: stop %s in sync\n", work->name);
	return ret;
}

/* use hdcptx_stop_work_sync outside of hdcp auth handlers */
static void dptx_hdcptx_cancel_works(struct hdcptx21_core *p_hdcp)
{
	u8 stop_work_max = 3;
	bool hdcp_work_scheduling = false;

	if (!p_hdcp) {
		DPTX_ERROR("NULL hdcp private instance\n");
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
		if (!dptx_hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_start))
			hdcp_work_scheduling = true;
		if (!dptx_hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_rcv_auth))
			hdcp_work_scheduling = true;
		if (!dptx_hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_rpt_auth))
			hdcp_work_scheduling = true;
		if (!dptx_hdcptx_stop_work_sync(&p_hdcp->timer_hdcp_auth_fail_retry))
			hdcp_work_scheduling = true;
		if (!dptx_hdcptx_stop_work_sync(&p_hdcp->timer_bksv_poll_done))
			hdcp_work_scheduling = true;
		if (!dptx_hdcptx_stop_work_sync(&p_hdcp->timer_ddc_check_nak))
			hdcp_work_scheduling = true;
		if (!dptx_hdcptx_stop_work_sync(&p_hdcp->timer_update_csm))
			hdcp_work_scheduling = true;
	} while (--stop_work_max > 0 && hdcp_work_scheduling);
}

/*
 * return true if work was pending and canceled, false otherwise
 * can't use cancel_delayed_work_sync in hdcptx_reset, as it may
 * lead to dead wait, for example: hdcptx_check_update_whandler will
 * call hdcptx_reset(and then will call dptx_hdcptx_stop_work), as a result
 * hdcptx_check_update_whandler will never exit
 */
static bool dptx_hdcptx_stop_work(struct hdcp_work *work)
{
	bool ret = cancel_delayed_work(&work->dwork);

	DPTX_DEBUG_HDCP("hdcptx: stop %s\n", work->name);
	return ret;
}

static void dptx_hdcp2x_reauth_start(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	if (!p_hdcp->reauth_ignored)
		dptx_hdcptx2_reauth_send(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
}

static void dptx_hdcp2x_auth_stop(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL hdcp_private instance\n");
		return;
	}
	dptx_hdcptx_stop_work(&p_hdcp->timer_ddc_check_nak);
	/*
	 * should always stop auth, otherwise the hw DDC may be
	 * free currently but will transact later,
	 * if do AON_CYP_CTL bit3 reset(DDC Master) during
	 * DDC transaction may pull SCL low.
	 * ddc busy check will add delay for ddc free before
	 * ddc stop
	 */
	dptx_hdcptx_ddc_check_busy();
	dptx_hdcptx2_auth_stop(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
}

static void dptx_hdcp2x_auth_start(struct hdcptx21_core *p_hdcp)
{
	u8 content_type;

	p_hdcp->hdcp_type = HDCP_VER_HDCP2X;
	if (p_hdcp->content_type == HDCP_CONTENT_TYPE_0)
		content_type = 0;
	else if (p_hdcp->content_type == HDCP_CONTENT_TYPE_1)
		content_type = 1;
	else
		content_type = 0xFF;
	DPTX_DEBUG_HDCP("content_type %d %d\n", p_hdcp->content_type, content_type);

	dptx_hdcptx2_src_auth_start(p_hdcp->bind_hdcptx_comm->bind_hw_comm, content_type);

	dptx_hdcp2x_reauth_start(p_hdcp);
	dptx_hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_ddc_check_nak, 100, 200);
	dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_AUTH);
}

static void dptx_hdcptx_topology_update(struct hdcptx21_core *p_hdcp)
{
	struct hdcp_topo_t *topo = &p_hdcp->hdcp_topology;
	struct hdcptx_common *hdcp_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcp_comm or NULL private_hdcp struct\n");
		return;
	}
	hdcp_comm = p_hdcp->bind_hdcptx_comm;

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		if (p_hdcp->ds_repeater) {
			u8 topo_val;

			topo_val = dptx_hdcptx2_topology_get(hdcp_comm->bind_hw_comm);
			/*
			 * should only add depth/count for repeater itself
			 * when propagate topology information to upstream side.
			 */
			topo->rp_depth = dptx_hdcptx2_rpt_depth_get(hdcp_comm->bind_hw_comm);
			topo->dev_count = dptx_hdcptx2_rpt_dev_cnt_get(hdcp_comm->bind_hw_comm);
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
			u8 max_devices = hdcp_comm->hdcp_rpt_en ?
				HDCP1X_MAX_TX_DEV_RPT : HDCP1X_MAX_TX_DEV_SRC;

			dptx_hdcptx1_bstatus_get(p_hdcp->bind_hdcptx_comm->bind_hw_comm, bstatus);
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

static void dptx_hdcptx_get_ds_rcv_id(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	dptx_hdcptx2_ds_rcv_id_read(p_hdcp->bind_hdcptx_comm->bind_hw_comm, p_hdcp->p_ksv_next);
	p_hdcp->p_ksv_next += KSV_SIZE;
}

static void dptx_hdcptx_check_ds_csm_status(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	DPTX_DEBUG_HDCP("ds_repeater %d  hdcp_type %d  csm_valid %d  content_type %d\n",
		p_hdcp->ds_repeater, p_hdcp->hdcp_type,
		p_hdcp->csm_valid, p_hdcp->content_type);
	if (p_hdcp->ds_repeater) {
		if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
			if (p_hdcp->csm_valid)
				dptx_hdcptx_send_csm_msg(p_hdcp);
		} else {
			dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
		}
	} else {
		if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
			dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
		} else {
			if (p_hdcp->csm_valid) {
				if (p_hdcp->content_type == HDCP_CONTENT_TYPE_1)
					dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_CONTENT_TYPE);
				else if (p_hdcp->content_type == HDCP_CONTENT_TYPE_0)
					dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
				else
					dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_CONTENT_TYPE);
			} else {
				dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
			}
		}
	}
}

static void dptx_hdcptx_get_ds_bksv_list(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	dptx_hdcptx1_ds_bksv_read(p_hdcp->bind_hdcptx_comm, p_hdcp->p_ksv_next, KSV_SIZE);
	p_hdcp->p_ksv_next += KSV_SIZE;
}

static void dptx_hdcptx_authenticated_handle(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL private_hdcp struct\n");
		return;
	}

	DPTX_HDCP_INFO("part 1 done\n");
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
		p_hdcp->ds_auth = true;
		DPTX_HDCP_INFO("1x AuthDone\n");
		p_hdcp->fail_type = HDCP_FAIL_NONE;
		dptx_hdcptx_encryption_update(p_hdcp, true);
		dptx_hdcptx_topology_update(p_hdcp);
		dptx_hdcptx_get_ds_bksv_list(p_hdcp);
		dptx_hdcptx_check_ds_csm_status(p_hdcp);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
		p_hdcp->ds_auth = true;
		DPTX_HDCP_INFO("2x AuthDone\n");
		p_hdcp->fail_type = HDCP_FAIL_NONE;
		dptx_hdcptx_encryption_update(p_hdcp, true);
		p_hdcp->ds_repeater = false;
		dptx_hdcptx_topology_update(p_hdcp);
		dptx_hdcptx_get_ds_rcv_id(p_hdcp);
		/* ds is hdcp2.2 TV */
		dptx_hdcptx22_topo_ctrl(p_hdcp->bind_hdcptx_comm,
			SET_HDCP22_TOPO, 1);
		dptx_hdcptx_check_ds_csm_status(p_hdcp);
	}
}

static bool dptx_hdcptx_process_repeater_fail(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return false;
	}

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		u8 ds_bstatus[2];

		dptx_hdcptx_get_ds_bksv_list(p_hdcp);

		dptx_hdcptx1_bstatus_get(p_hdcp->bind_hdcptx_comm->bind_hw_comm, ds_bstatus);
		if ((ds_bstatus[0] & 0x80) || (ds_bstatus[1] & 0x08)) {
			dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return false;
		}
		dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_V);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		u8 hdcp_topology;

		hdcp_topology = dptx_hdcptx2_topology_get(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
		if ((hdcp_topology & 0x04) || (hdcp_topology & 0x08)) {
			dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return false;
		}
		dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_READY_TO);
	}

	return true;
}

static bool dptx_hdcptx_is_topology_correct(struct hdcptx21_core *p_hdcp)
{
	u8 max_depth = HDCP1X_MAX_DEPTH;
	u8 max_device_count;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL private_hdcp struct\n");
		return false;
	}
	max_device_count = p_hdcp->bind_hdcptx_comm->hdcp_rpt_en ?
		HDCP1X_MAX_TX_DEV_RPT : HDCP1X_MAX_TX_DEV_SRC;

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		max_device_count = HDCP2X_MAX_DEV;
		max_depth = HDCP2X_MAX_DEPTH;
	}
	if (p_hdcp->hdcp_topology.max_devs_exceed || p_hdcp->hdcp_topology.max_cas_exceed)
		return false;

	return true;
}

static void dptx_hdcptx_get_ds_rcvid_lists(struct hdcptx21_core *p_hdcp, u8 count)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	dptx_hdcptx2_ds_rpt_rcvid_list_read(p_hdcp->bind_hdcptx_comm->bind_hw_comm,
				p_hdcp->p_ksv_next, count, KSV_SIZE);
	p_hdcp->p_ksv_next += count * KSV_SIZE;
}

static void dptx_hdcptx_assemble_ds_ksv_lists(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
		u8 ds_bstatus[2];
		u8 ds_count;
		u8 ds_depth;

		dptx_hdcptx1_bstatus_get(p_hdcp->bind_hdcptx_comm->bind_hw_comm, ds_bstatus);
		ds_count = ds_bstatus[0] & 0x7F;
		ds_depth = ds_bstatus[1] & 0x07;
		dptx_hdcptx1_get_ds_ksvlists(p_hdcp->bind_hdcptx_comm->bind_hw_comm,
						&p_hdcp->p_ksv_next, ds_count);
		/*
		 * include the bksv of downstream repeater, add to the
		 * ksv list which will report to upstream
		 */
		dptx_hdcptx_get_ds_bksv_list(p_hdcp);
	} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		u8 dev_cnt = dptx_hdcptx2_rpt_dev_cnt_get(p_hdcp->bind_hdcptx_comm->bind_hw_comm);

		dptx_hdcptx_get_ds_rcvid_lists(p_hdcp, dev_cnt);
		dptx_hdcptx_get_ds_rcv_id(p_hdcp);
	}
}

static void dptx_hdcptx_rpt_ready_process(struct hdcptx21_core *p_hdcp, bool ksv_read_status)
{
	struct hdcptx_common *hdcp_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcp_comm or NULL private_hdcp struct\n");
		return;
	}
	hdcp_comm = p_hdcp->bind_hdcptx_comm;
	bool hdcp_rpt_en = hdcp_comm->hdcp_rpt_en;

	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	if (!(p_hdcp->hdcp_state == HDCP_STAT_FAILED ||
	      p_hdcp->hdcp_state == HDCP_STAT_AUTH)) {
		if (!(p_hdcp->hdcp_state == HDCP_STAT_SUCCESS ||
		      p_hdcp->hdcp_state == HDCP_STAT_CSM)) {
			dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_UNKNOWN);
			return;
		}
	}
	if (!ksv_read_status) {
		dptx_hdcptx_topology_update(p_hdcp);
		dptx_hdcptx_reset_ksv_fifo(p_hdcp);
		if (!dptx_hdcptx_process_repeater_fail(p_hdcp))
			return;
	} else {
		dptx_hdcptx_topology_update(p_hdcp);
		if (dptx_hdcptx_is_topology_correct(p_hdcp)) {
			dptx_hdcptx_assemble_ds_ksv_lists(p_hdcp);
			if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
				/*
				 * if no hdcp1.x or hdcp2.0 legacy devices
				 * downstream, then topo 1
				 */
				mutex_lock(&p_hdcp->stream_type_mutex);
				if (p_hdcp->hdcp_topology.ds_hdcp1x_dev == 0 &&
					p_hdcp->hdcp_topology.ds_hdcp2x_dev == 0) {
					dptx_hdcptx22_topo_ctrl(p_hdcp->bind_hdcptx_comm,
						SET_HDCP22_TOPO, 1);
					/* not care if currently in hdmirx channel or not */
					/* if (!hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) { */
						p_hdcp->csm_message.streamid_type = 1;
						DPTX_HDCP_INFO("auth with streamid_type = 1\n");
					/* } */
				} else {
					dptx_hdcptx22_topo_ctrl(p_hdcp->bind_hdcptx_comm,
						SET_HDCP22_TOPO, 0);
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
				if (dptx_hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) {
					if (p_hdcp->saved_upstream_type & 0x10) {
						p_hdcp->csm_message.streamid_type =
							p_hdcp->saved_upstream_type & 0xf;
						p_hdcp->saved_upstream_type &= 0xf;
						DPTX_HDCP_INFO("force upstream type: %d\n",
							p_hdcp->csm_message.streamid_type);
						/* need set to false, so that to re-send */
						p_hdcp->csm_updated = false;
						p_hdcp->csm_msg_sent = false;
					}
				}
				if (p_hdcp->csm_msg_sent) {
					dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
				} else {
					dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_CSM);
					if (p_hdcp->csm_valid) {
						void *hw_comm = hdcp_comm->bind_hw_comm;

						p_hdcp->csm_updated = true;
						dptx_hdcptx_send_csm_msg(p_hdcp);
						dptx_hdcptx2_rpt_smng_xfer_start(hw_comm);
						DPTX_HDCP_INFO("hdcptx2: send stream type: %d\n",
							p_hdcp->csm_message.streamid_type);
					}
				}
				mutex_unlock(&p_hdcp->stream_type_mutex);
			}
		} else {
			dptx_hdcptx_assemble_ds_ksv_lists(p_hdcp);
			dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_TOPOLOGY);
			return;
		}
	}
	p_hdcp->rpt_ready = true;
	DPTX_HDCP_INFO("rpt_ready %d\n", p_hdcp->rpt_ready);
}

static void dptx_hdcptx_encryption_update(struct hdcptx21_core *p_hdcp, bool en)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	if (p_hdcp->ds_auth) {
		p_hdcp->encryption_enabled = en;

		if (p_hdcp->hdcp_type == HDCP_VER_HDCP1X) {
			dptx_hdcptx1_encryption_update(p_hdcp->bind_hdcptx_comm->bind_hw_comm, en);
		} else if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
			dptx_hdcptx1_encryption_update(p_hdcp->bind_hdcptx_comm->bind_hw_comm, en);
			dptx_hdcptx2_encryption_update(p_hdcp->bind_hdcptx_comm->bind_hw_comm, en);
		}
	}
}

const char *dptxmsg1_info[] = {
	"ro_rpt_rcvid_changed",
	"ro_rpt_smng_changed",
	"ro_ake_sent_rcvd",
	"ro_ske_sent_rcvd",
	"ro_rpt_rcvid_xfer_done",
	"ro_rpt_smng_xfer_done",
	"ro_cert_sent_rcvd",
	"ro_gp3",
};

const char *dptxmsg2_info[] = {
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
void dptx_hdcp2x_process_intr(void *hdcptx_instance, u8 int_reg[])
{
	struct hdcptx21_core *p_hdcp = (struct hdcptx21_core *)hdcptx_instance;
	struct hdcptx_common *hdcp_comm = p_hdcp->bind_hdcptx_comm;
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
	u8 cp2tx_state = dptx_hdcp2x_get_state_st(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
	static int smng_times;

	DPTX_DEBUG_HDCP("0x%x 0x%x 0x%x 0x%x 0x%x\n", int_reg[0], int_reg[1],
				int_reg[2], int_reg[3], cp2tx_state);

	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_AUTH_DONE) {
		/* intr0 bit7 and bit0 needs to be 1, then start smng xfer */
		/* otherwise the 1B-09 will fail */
		/* if csm already update by upstream, don't transfer once more */
		/* mutex_lock(&p_hdcp->stream_type_mutex); */
		if ((cp2tx_intr0_st & BIT_CP2TX_INTR0_POLL_INTERVAL) &&
			p_hdcp->ds_repeater &&
			!p_hdcp->csm_updated) {
			p_hdcp->csm_updated = true;
			dptx_hdcptx_update_csm(p_hdcp);
			dptx_hdcptx2_rpt_smng_xfer_start(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
		}
		/* mutex_unlock(&p_hdcp->stream_type_mutex); */
		if (dptx_hdcptx2_auth_status(p_hdcp->bind_hdcptx_comm->bind_hw_comm)) {
			if (!dptx_hdcptx_query_ds_repeater(p_hdcp)) {
				dptx_hdcptx_authenticated_handle(p_hdcp);
				/* ds is receiver case */
				schedule_delayed_work(&p_hdcp->ksv_notify_wk,
					msecs_to_jiffies(notify_delay_ms));
			} else {
				dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
				dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
				DPTX_HDCP_INFO("hdcptx2: auth done\n");
			}
		}
		/* clear fail reason after passed */
		p_hdcp->fail_reason = HDCP_FAIL_NONE;
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_AUTH_FAIL) {
		dptx_hdcp2x_auth_stop(p_hdcp);
		dptx_hdcptx_update_failures(p_hdcp, p_hdcp->ds_repeater);
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_RPT_READY_CHANGE) {
		p_hdcp->ds_repeater = true;
		DPTX_HDCP_INFO("hdcptx2: repeater ready\n");
	}
	if (cp2tx_intr0_st & BIT_CP2TX_INTR0_REAUTH_REQ) {
		DPTX_HDCP_INFO("hdcptx2: reauth req from ds device\n");
		/*
		 * need to re-init flag, so that to send stream type
		 * management message after re-auth done
		 */
		p_hdcp->csm_msg_sent = false;
		p_hdcp->csm_updated = false;
		p_hdcp->hdcp14_second_part_pass = false;
		/* reset ksv fifo pointer when re-auth */
		dptx_hdcptx_reset_ksv_fifo(p_hdcp);
		dptx_hdcp2x_reauth_start(p_hdcp);
		/* only needed on customer board for hdcp re-auth filter */
		/* hdcp_reauth_req = 1; */
	}

	for (i = 0; i < 8; i++) {
		if (cp2tx_intr1_st & (1 << i))
			DPTX_HDCP_INFO("%s", dptxmsg1_info[i]);
	}
	for (i = 0; i < 8; i++) {
		if (cp2tx_intr2_st & (1 << i))
			DPTX_HDCP_INFO("%s", dptxmsg2_info[i]);
	}

	if (cp2tx_intr1_st & BIT_CP2TX_INTR1_RPT_RCVID_CHANGED) {
		DPTX_HDCP_INFO("hdcptx2: rcv_id changed");
		dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
		dptx_hdcptx_rpt_ready_process(p_hdcp, true);
		/* ds is repeater case */
		schedule_delayed_work(&p_hdcp->ksv_notify_wk, msecs_to_jiffies(notify_delay_ms));
	}
	/* BIT__HDCP2X_INTR0__AKE_SEND */
	if (cp2tx_intr1_st & BIT(2))
		p_hdcp->reauth_ignored = true;
	/* BIT__HDCP2X_INTR0__RPTR_SMNG_TRANS_DONE */
	if (cp2tx_intr1_st & BIT(5))
		dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_SUCCESS);
	/* BIT__HDCP2X_INTR0__CERT_RECV */
	if (cp2tx_intr1_st & BIT(6))
		p_hdcp->reauth_ignored = false;

	/* BIT__HDCP2X_INTR0__SKE_SEND */
	if (cp2tx_intr1_st & BIT(3)) {
		p_hdcp->reauth_ignored = false;
		smng_times = 0;
		if (dptx_hdcptx_query_ds_repeater(p_hdcp)) {
			/* down stream is a hdcp2.2 repeater */
			p_hdcp->ds_repeater = true;
			p_hdcp->ds_auth = true;
			dptx_hdcptx_encryption_update(p_hdcp, true);
			dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
			dptx_hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_rpt_auth,
				HDCP_RCVIDLIST_CHECK_TIMER, 0);
			/* update csm later, after got repeater topo info */
			/* dptx_hdcptx_check_ds_csm_status(p_hdcp); */
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
				dptx_hdcptx_update_csm(p_hdcp);
				dptx_hdcptx2_rpt_smng_xfer_start(hdcp_comm->bind_hw_comm);
				DPTX_HDCP_INFO("send smng_times %d\n", smng_times);
			} else {
				smng_times = 0;
				DPTX_HDCP_INFO("hdcptx2: M' hash fail, restart auth\n");
				schedule_delayed_work(&p_hdcp->req_reauth_wk,
						msecs_to_jiffies(1000));
			}
		} else {
			schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
		}
	}
}

static void dptx_hdcp1x_auth_start(struct hdcptx21_core *p_hdcp)
{
	bool key_valid;

	DPTX_DEBUG_HDCP("hdcp14 auth start\n");
	p_hdcp->hdcp_type = HDCP_VER_HDCP1X;
	dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_AUTH);
	dptx_hdcptx1_encryption_update(p_hdcp->bind_hdcptx_comm->bind_hw_comm, false);

	if (!p_hdcp->bind_hdcptx_comm->hdcp14_key_loaded) {
		key_valid = dptx_hdcptx1_load_key();
		if (!key_valid)
			DPTX_ERROR("hdcp1.4 key load failed!\n");
		else
			p_hdcp->bind_hdcptx_comm->hdcp14_key_loaded = true;
	} else {
		DPTX_DEBUG("hdcp1.4 key already loaded\n");
	}

	dptx_hdcptx1_auth_start();
	dptx_hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_bksv_poll_done,
		HDCP_BSKV_CHECK_TIMER, 0);
}

static void dptx_hdcptx_auth_start(struct hdcptx21_core *p_hdcp)
{
	enum hdcp_ver_t hdcp_mode;
	struct hdcptx_common *hdcp_comm = p_hdcp->bind_hdcptx_comm;

	if (!p_hdcp) {
		DPTX_ERROR("NULL hdcp_private instance\n");
		return;
	}

	hdcp_mode = p_hdcp->req_hdcp_ver;
	if (hdcp_mode != HDCP_VER_NONE) {
		hdcp_mode = p_hdcp->req_hdcp_ver;
		if (p_hdcp->hdcptx_enabled) {
			dptx_hdcptx_reset_ksv_fifo(p_hdcp);
			//move to dptx_hw.c
			//dptx_hdcp_enable_intr(hdcp_comm, 1);
			dptx_hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_rcv_auth,
				HDCP_STAGE1_RETRY_TIMER, 0);
			if (hdcp_mode == HDCP_VER_HDCP1X) {
				dptx_hdcptx_en_aes_dual_pipe(hdcp_comm->bind_hw_comm,
								false);
				dptx_hdcp1x_auth_start(p_hdcp);
			} else if (hdcp_mode == HDCP_VER_HDCP2X) {
				if (p_hdcp->frl_rate == FRL_NONE)
					dptx_hdcptx_en_aes_dual_pipe(hdcp_comm->bind_hw_comm,
								false);
				else
					dptx_hdcptx_en_aes_dual_pipe(hdcp_comm->bind_hw_comm,
								true);
				dptx_hdcp2x_auth_start(p_hdcp);
			}
			dptx_hdcptx_schedule_work(p_hdcp->hdcp_wq,
				&p_hdcp->timer_ddc_check_nak, 100, 200);
		}
	}
}

static void dptx_hdcptx_update_state(struct hdcptx21_core *p_hdcp, enum hdcp_stat_t state)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	if (p_hdcp->hdcp_state != state || state == HDCP_STAT_NONE) {
		p_hdcp->hdcp_state = state;
		DPTX_DEBUG_HDCP("hdcptx update state: %d\n", state);
	}
}

static void dptx_hdcptx_reset_ksv_fifo(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return;
	}

	p_hdcp->p_ksv_next = p_hdcp->p_ksv_lists;
}

static bool dptx_hdcptx_schedule_work(struct workqueue_struct *wq, struct hdcp_work *work,
	u32 delay_ms, u32 period_ms)
{
	if (!wq || !work) {
		DPTX_ERROR("NULL workqueue or NULL work\n");
		return false;
	}

	DPTX_DEBUG_HDCP("hdcptx: schedule %s: delay %d ms  period %d ms\n",
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

static void dptx_hdcptx_update_failures(struct hdcptx21_core *p_hdcp, enum hdcp_fail_types_t types)
{
	if (types > HDCP_FAIL_UNKNOWN) {
		types = HDCP_FAIL_UNKNOWN;
		dump_stack();
	}
	if (p_hdcp->fail_reason != types) {
		/* if fail type is DDC_NAK, and then comes BKSV_RXID, don't print */
		if (p_hdcp->fail_reason == HDCP_FAIL_DDC_NACK ||
			p_hdcp->fail_reason == HDCP_FAIL_BKSV_RXID) {
			DPTX_DEBUG_HDCP("hdcptx reason types: %s\n", dptx_fail_string[types]);
		} else {
			DPTX_HDCP_INFO("hdcptx reason types: %s\n", dptx_fail_string[types]);
		}
		p_hdcp->fail_reason = types;
	} else {
		DPTX_DEBUG_HDCP("hdcptx reason types: %s\n", dptx_fail_string[types]);
	}
	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	if (p_hdcp->hdcptx_enabled) {
		if (types == HDCP_FAIL_CONTENT_TYPE) {
			p_hdcp->fail_type = types;
			dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_FAILED);
		} else {
			p_hdcp->reauth_ignored = false;
			p_hdcp->fail_type = types;
			dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_FAILED);
			dptx_hdcptx_reset_ksv_fifo(p_hdcp);
			dptx_hdcptx_schedule_work(p_hdcp->hdcp_wq,
				&p_hdcp->timer_hdcp_auth_fail_retry,
				HDCP_FAILED_RETRY_TIMER, 0);
		}
	}
}

/* hdcp receive/repeater share the same work handler */
static void dptx_hdcptx_check_ds_auth_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core *p_hdcp = NULL;

	if (!work) {
		DPTX_ERROR("null work\n");
		return;
	}

	DPTX_DEBUG_HDCP("check_ds_auth_whandler period %d ms\n", work->period_ms);

	if (strcmp(work->name, "hdcp_rcv_auth") == 0)
		p_hdcp = container_of(work, struct hdcptx21_core, timer_hdcp_rcv_auth);
	else if (strcmp(work->name, "hdcp_rpt_auth") == 0)
		p_hdcp = container_of(work, struct hdcptx21_core, timer_hdcp_rpt_auth);
	if (!p_hdcp) {
		DPTX_ERROR("null private_hdcp instance!\n");
		return;
	}

	dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_AUTH_TIME_OUT);
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static bool dptx_hdcptx_is_bksv_valid(struct hdcptx21_core *p_hdcp)
{
	u8 bksv[KSV_SIZE];
	u8 i, count = 0;

	dptx_hdcptx1_ds_bksv_read(p_hdcp->bind_hdcptx_comm, bksv, KSV_SIZE);

	for (i = 0; i < KSV_SIZE; i++) {
		while (bksv[i] != 0) {
			if (bksv[i] & 1)
				count++;
			bksv[i] >>= 1;
		}
	}
	return count == 20;
}

static void dptx_hdcptx_check_bksv_done_whandler(struct work_struct *w)
{
	u8 copp_data1;
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core *p_hdcp = container_of(work, struct hdcptx21_core,
		timer_bksv_poll_done);

	if (!p_hdcp) {
		DPTX_ERROR("null private_hdcp instance!\n");
		return;
	}

	DPTX_DEBUG_HDCP("check_bksv_done_whandler period %d ms\n", work->period_ms);
	copp_data1 = dptx_hdcptx1_ds_cap_status_get(p_hdcp->bind_hdcptx_comm->bind_hw_comm);

	if (copp_data1 & 0x02) {
		dptx_hdcptx_stop_work(work);
		if (dptx_hdcptx_is_bksv_valid(p_hdcp))
			dptx_hdcptx1_protection_enable(p_hdcp->bind_hdcptx_comm->bind_hw_comm,
						true);
		else
			dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_BKSV_RXID);
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void dptx_hdcp1x_auth_stop(struct hdcptx21_core *p_hdcp)
{
	//todo hdcp1x
	//hdcptx1_auth_stop();
	//hdcptx1_protection_enable(false);
	//hdcptx_ddc_toggle_sw_tpi();
}

static void dptx_hdcp_reset_hw(struct hdcptx21_core *p_hdcp)
{
	dptx_hdcptx_init_reg(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
	dptx_hdcp1x_auth_stop(p_hdcp);
	dptx_hdcp2x_auth_stop(p_hdcp);
}

static void dptx_hdcptx_reset(struct hdcptx21_core *p_hdcp)
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
	dptx_rst_stream_type(p_hdcp);

	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_start);
	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rcv_auth);
	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_rpt_auth);
	dptx_hdcptx_stop_work(&p_hdcp->timer_hdcp_auth_fail_retry);
	dptx_hdcptx_stop_work(&p_hdcp->timer_bksv_poll_done);
	dptx_hdcptx_stop_work(&p_hdcp->timer_ddc_check_nak);
	dptx_hdcptx_stop_work(&p_hdcp->timer_update_csm);

	dptx_hdcptx_reset_ksv_fifo(p_hdcp);
	p_hdcp->ds_auth = false;
	dptx_hdcp_reset_hw(p_hdcp);

	dptx_hdcptx2_encryption_update(p_hdcp->bind_hdcptx_comm->bind_hw_comm, false);
}

static enum hdcp_ver_t dptx_hdcp_check_ds_hdcp2ver(struct hdcptx21_core *p_hdcp)
{
	enum hdcp_ver_t hdcp_type = HDCP_VER_NONE;
	enum ddc_err_t ddc_err = DDC_ERR_NONE;
	u8 cap_val = 0;

	//todo aux read rxcap hdcpver
	//ddc_err = hdmitx_ddcm_read(0, DDC_HDCP_DEVICE_ADDR, REG_DDC_HDCP_VERSION, &cap_val,
	//			   1, TPI_DDC_CMD_SEQUENTIAL_READ);

	if (ddc_err == DDC_ERR_NONE) {
		if (cap_val == 0x04)
			hdcp_type = HDCP_VER_HDCP2X;
		else
			hdcp_type = HDCP_VER_HDCP1X;
	} else {
		dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_DDC_NACK);
	}
	return hdcp_type;
}

static void dptx_hdcptx_check_update_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core *p_hdcp = container_of(work, struct hdcptx21_core,
		timer_hdcp_start);

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}

	DPTX_DEBUG_HDCP("[%s] period %d ms\n",  work->name, work->period_ms);
	dptx_hdcptx_reset(p_hdcp);
	p_hdcp->fail_reason = HDCP_FAIL_NONE;
	if (dptx_hdcp_reauth_dbg == 2) {
		mdelay(delay_ms);
	} else if (dptx_hdcp_reauth_dbg == 3) {
		mdelay(delay_ms);
		dptx_hdcptx_reset(p_hdcp);
	}
	if (p_hdcp->hdcptx_enabled) {
		/* only read downstream hdcp version when hdcp2.2 is capable on source side */
		if (dptx_hdcptx22_topo_ctrl(p_hdcp->bind_hdcptx_comm, GET_HDCP22_TOPO, 0))
			p_hdcp->hdcp_cap_ds = dptx_hdcp_check_ds_hdcp2ver(p_hdcp);
		else
			p_hdcp->hdcp_cap_ds = HDCP_VER_HDCP1X;
		if (p_hdcp->hdcp_cap_ds != HDCP_VER_NONE) {
			dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_AUTH);
			dptx_hdcptx_auth_start(p_hdcp);
		}
	} else {
		dptx_hdcptx_update_state(p_hdcp, HDCP_STAT_NONE);
		p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
		p_hdcp->fail_type = HDCP_FAIL_NONE;
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void dptx_hdcp_ddc_check_nack_whandler(struct work_struct *w)
{
	static int cnt;
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core *p_hdcp = container_of(work, struct hdcptx21_core,
		timer_ddc_check_nak);

	if (!p_hdcp) {
		DPTX_ERROR("null private_hdcp instance!\n");
		return;
	}

	if (cnt % 128 == 0) {
		cnt++;
		DPTX_DEBUG_HDCP("ddc_check_nack_whandler period %d ms\n", work->period_ms);
	}
	if (dptx_hdmi_ddc_status_check()) {
		p_hdcp->hdcp_cap_ds = HDCP_VER_NONE;
		dptx_hdcp2x_auth_stop(p_hdcp);
		dptx_hdcptx_update_failures(p_hdcp, HDCP_FAIL_DDC_NACK);
	}
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void dptx_hdcp_check_auth_fail_retry_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core *p_hdcp = container_of(work, struct hdcptx21_core,
		timer_hdcp_auth_fail_retry);

	if (!p_hdcp) {
		DPTX_ERROR("null private_hdcp instance!\n");
		return;
	}

	DPTX_DEBUG_HDCP("check_auth_fail_retry_whandler period %d ms\n", work->period_ms);
	dptx_hdcptx_reset(p_hdcp);
	dptx_hdcptx_auth_start(p_hdcp);
	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void dptx_hdcptx_update_csm_whandler(struct work_struct *w)
{
	struct hdcp_work *work = container_of((struct delayed_work *)w,
		struct hdcp_work, dwork);
	struct hdcptx21_core *p_hdcp = container_of(work, struct hdcptx21_core,
		timer_update_csm);

	if (!p_hdcp) {
		DPTX_ERROR("null private_hdcp instance!\n");
		return;
	}

	DPTX_DEBUG_HDCP("update_csm_whandler period %d ms\n", work->period_ms);
	dptx_hdcptx_update_csm(p_hdcp);

	if (work->period_ms)
		queue_delayed_work(p_hdcp->hdcp_wq, &work->dwork, work->period_ms);
}

static void dptx_hdcptx_notify_rpt_info(struct work_struct *work)
{
	unsigned int ksv_bytes = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	struct hdcptx21_core *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core, ksv_notify_wk);
	struct hdcp_topo_t *topo = &p_hdcp->hdcp_topology;
	/* hdcp_topo info transferred to upstream, 1.4 & 2.2 */
	struct hdcp_topo_s hdcp_topo;
	struct hdcptx_common *hdcp_comm = NULL;
	bool hdcp_rpt_en = false;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL private_hdcp struct\n");
		return;
	}
	hdcp_comm = p_hdcp->bind_hdcptx_comm;
	hdcp_rpt_en = hdcp_comm->hdcp_rpt_en;
	/*
	 * note: need calculate the topo info for hdcp repeater
	 * here instead of in dptx_hdcptx_topology_update()
	 */
	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
		if (p_hdcp->ds_repeater) {
			/*
			 * include the count/depth of downstream repeater itself.
			 * the rcvid list of downstream repeater will be added to notify
			 * list in hdcptx_assemble_ds_ksv_lists()
			 */
			if (dptx_hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) {
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
			if (dptx_hdcptx_need_ctrl_by_upstream(hdcp_rpt_en)) {
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
	DPTX_DEBUG_HDCP("hdcp_type: %d\n", p_hdcp->hdcp_type);
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
	DPTX_DEBUG_HDCP("depth:[%d] cnt:[%d] ksv_bytes:[%d] max_cas: [%d], max_dev: [%d]\n",
		hdcp_topo.depth, hdcp_topo.dev_cnt, ksv_bytes,
		hdcp_topo.max_cascade_exceeded, hdcp_topo.max_devs_exceeded);
	for (i = 0; i < ksv_bytes / 5; i++) {
		DPTX_DEBUG_HDCP("ksv list[%d]: ", i);
		for (j = 0; j < 5; j++)
			DPTX_DEBUG_HDCP("%02x\n", hdcp_topo.ksv_list[5 * i + j]);
		DPTX_DEBUG_HDCP("\n");
	}
	if (p_hdcp->bind_hdcptx_comm->send_event)
		p_hdcp->bind_hdcptx_comm->send_event(p_hdcp->bind_hdcptx_comm->event_mgr,
			TX_KSVLIST, &hdcp_topo);
}

/*
 * when disable signal, should also disable hdcp
 * there are below cases need to disable hdcp:
 * 1.suspend, 2.before switch mode, 3.plug out
 * note disable hdcp mode should mutex with EDID
 * operation(plugin bottom half), which already
 * mutexed with above 3 operations
 */
static void dptx_disable_hdcp(void *hdcptx_instance)
{
	struct hdcptx21_core *p_hdcp = (struct hdcptx21_core *)hdcptx_instance;
	struct hdcptx_common *hdcp_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp private instance\n");
		return;
	}
	hdcp_comm = p_hdcp->bind_hdcptx_comm;
	mutex_lock(&p_hdcp->hdcptx_auth_mutex);
	cancel_delayed_work(&p_hdcp->work_up_hdcp_timeout);
	cancel_delayed_work(&p_hdcp->work_start_hdcp);
	if (hdcp_comm->hdcp_mode != 0) {
		dptx_hdcptx_cancel_works(p_hdcp);
		hdcp_comm->hdcp_mode = 0;
		dptx_hdcptx_mode_set(p_hdcp, 0);
		mdelay(10);
		dptx_ctrl_hdcp_gate(hdcp_comm, 0, false);
	}
	mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
}

static void dptx_hdcptx_req_reauth_whandler(struct work_struct *work)
{
	struct hdcptx21_core *p_hdcp = container_of((struct delayed_work *)work,
			struct hdcptx21_core, req_reauth_wk);
	struct hdcptx_common *hdcptx_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL private_hdcp struct\n");
		return;
	}
	hdcptx_comm = p_hdcp->bind_hdcptx_comm;
	/* once receive hdcp auth from upstream side, cancel timeout */
	cancel_delayed_work(&p_hdcp->work_up_hdcp_timeout);
	/* if signal not ready, means hdmitx is setting mode, delay 1S */
	/* note: for CTS, it should not delay */
	if (hdcptx_comm->suspend_flag) {
		DPTX_HDCP_INFO("suspend, no need re-auth\n");
		return;
	}
	if (!hdcptx_comm->hpd_state) {
		DPTX_HDCP_INFO("dptx hpd low, no need re-auth\n");
		return;
	}
	if (!hdcptx_comm->ready) {
		schedule_delayed_work(&p_hdcp->req_reauth_wk, msecs_to_jiffies(1000));
		DPTX_HDCP_INFO("dptx signal not done, delay hdcp re-auth\n");
		return;
	}
	/* firstly need to disable current hdcp auth */
	/*
	 * set avmute, wait for sink side action, and then
	 * stop hdcp, at last clear avmute. to prevent TV
	 * display abnormal when detect signal from encryption
	 * to un-encryption.
	 * dptx_hdcp_reauth_dbg == 4 is only for debug purpose
	 */
	if (dptx_hdcp_reauth_dbg == 1) {
		/* before re-auth, do both video mute + avmute */
		if (hdcptx_comm->video_mute_op)
			hdcptx_comm->video_mute_op(hdcptx_comm->bind_hw_comm,
				0, VIDEO_MUTE_PATH_3);
		if (hdcptx_comm->avmute_op)
			hdcptx_comm->avmute_op(hdcptx_comm->bind_tx_comm,
				SET_AVMUTE, AVMUTE_PATH_2);
		msleep_interruptible(avmute_ms);
		dptx_disable_hdcp(p_hdcp);
		if (hdcptx_comm->video_mute_op)
			hdcptx_comm->video_mute_op(hdcptx_comm->bind_hw_comm,
				1, VIDEO_MUTE_PATH_3);
		/* msleep_interruptible(vid_mute_ms); */
		if (hdcptx_comm->avmute_op)
			hdcptx_comm->avmute_op(hdcptx_comm->bind_tx_comm,
				CLR_AVMUTE, AVMUTE_PATH_2);
	} else if (dptx_hdcp_reauth_dbg == 4) {
		if (hdcptx_comm->hdcp_mode) {
			if (p_hdcp->req_reauth_ver == 0 ||
				p_hdcp->req_reauth_ver == hdcptx_comm->hdcp_mode) {
				DPTX_HDCP_INFO("topology not changed, no need hdcp re-auth\n");
				schedule_delayed_work(&p_hdcp->ksv_notify_wk, 0);
				return;
			}
		}
		if (hdcptx_comm->video_mute_op)
			hdcptx_comm->video_mute_op(hdcptx_comm->bind_hw_comm,
				0, VIDEO_MUTE_PATH_3);
		mdelay(20);

		if (hdcptx_comm->avmute_op)
			hdcptx_comm->avmute_op(hdcptx_comm->bind_tx_comm,
				SET_AVMUTE, AVMUTE_PATH_2);
		msleep_interruptible(avmute_ms);
		dptx_disable_hdcp(p_hdcp);
		if (hdcptx_comm->video_mute_op)
			hdcptx_comm->video_mute_op(hdcptx_comm->bind_hw_comm,
				1, VIDEO_MUTE_PATH_3);
		msleep_interruptible(vid_mute_ms);
		if (hdcptx_comm->avmute_op)
			hdcptx_comm->avmute_op(hdcptx_comm->bind_tx_comm,
				CLR_AVMUTE, AVMUTE_PATH_2);
	}
	if (p_hdcp->req_reauth_ver == 0) {
		/* auto mode, need to use ds hdcp version */
		dptx_enable_hdcp(p_hdcp);
	} else if (p_hdcp->req_reauth_ver == 1) {
		/* force hdcp1.x mode */
		mutex_lock(&p_hdcp->hdcptx_auth_mutex);
		if (dptx_get_hdcptx_lstore(hdcptx_comm, HDCP_VER_HDCP1X)) {
			hdcptx_comm->hdcp_mode = 1;
			dptx_hdcptx_mode_set(p_hdcp, 1);
		} else {
			hdcptx_comm->hdcp_mode = 0;
		}
		mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
	} else if (p_hdcp->req_reauth_ver == 2) {
		/* force hdcp2.x mode */
		mutex_lock(&p_hdcp->hdcptx_auth_mutex);
		if (dptx_hdcptx22_topo_ctrl(hdcptx_comm, GET_HDCP22_TOPO, 0) &&
			hdcptx_comm->dw_hdcp22_cap) {
			hdcptx_comm->hdcp_mode = 2;
			dptx_hdcptx_mode_set(p_hdcp, 2);
		} else {
			hdcptx_comm->hdcp_mode = 0;
		}
		mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
	}
}

static void dptx_hdcp_stream_mute_whandler(struct work_struct *work)
{
	struct hdcptx21_core *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core, stream_mute_wk);
	struct hdcptx_common *hdcptx_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}
	hdcptx_comm = p_hdcp->bind_hdcptx_comm;
	/* not care race condition, use the last mute status */
	if (hdcptx_comm->video_mute_op)
		hdcptx_comm->video_mute_op(hdcptx_comm->bind_hw_comm,
			!p_hdcp->stream_mute, VIDEO_MUTE_PATH_2);
	if (hdcptx_comm->audio_mute_op)
		hdcptx_comm->audio_mute_op(hdcptx_comm->bind_tx_comm,
			!p_hdcp->stream_mute, AUDIO_MUTE_PATH_3);
}

static u32 dptx_get_hdcptx_mode(void *hdcptx_instance)
{
	u32 hdcp_mode = 0;
	struct hdcptx21_core *p_hdcp = (struct hdcptx21_core *)hdcptx_instance;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL p_hdcp or NULL hdcp_comm instance\n");
		return hdcp_mode;
	}

	mutex_lock(&p_hdcp->hdcptx_auth_mutex);
	hdcp_mode = p_hdcp->bind_hdcptx_comm->hdcp_mode;
	mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
	return hdcp_mode;
}

static bool dptx_hdcptx_query_ds_repeater(struct hdcptx21_core *p_hdcp)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL private_hdcp struct\n");
		return false;
	}

	if (p_hdcp->hdcp_type == HDCP_VER_HDCP2X)
		return dptx_hdcptx2_ds_rptr_capability(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
	else
		return dptx_hdcptx1_ds_rptr_capability(p_hdcp->bind_hdcptx_comm->bind_hw_comm);
}

/*
 * note that if hdcp is not enabled currently,
 * (as hdcp disabled somehow, e.g. hdmirx request
 * re-auth, but hdmitx suddenly disconnected),
 * should exit the propagate this time, just save
 * the stream type.
 */
static void dptx_hdcptx_propagate_stream_type(struct work_struct *work)
{
	struct hdcptx21_core *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core, stream_type_wk);
	u8 update_scene = 0;
	u8 upstream_type = 0;
	u8 cur_content_type = 0;
	bool ds_repeater = false;
	struct hdcptx_common *hdcp_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}
	hdcp_comm = p_hdcp->bind_hdcptx_comm;
	update_scene = p_hdcp->rx_update_flag & 0xf0;
	upstream_type = p_hdcp->rx_update_flag & 0xf;

	/* if signal not ready, should not propagate stream type */
	mutex_lock(&p_hdcp->stream_type_mutex);
	if (update_scene == STREAMTYPE_UPDATE) {
		/* update upstream type firstly */
		p_hdcp->saved_upstream_type = 0x10 | upstream_type;
		if (hdcp_comm->suspend_flag) {
			DPTX_HDCP_INFO("suspend, no need propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		if (!hdcp_comm->hpd_state) {
			DPTX_HDCP_INFO("dptx hpd low, no need propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		if (!hdcp_comm->ready) {
			DPTX_HDCP_INFO("dptx signal not done, not propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		/* make sure hdcp is enabled */
		if (dptx_get_hdcptx_mode(p_hdcp) == 0) {
			DPTX_HDCP_INFO("hdcp disabled, no need propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		ds_repeater = dptx_hdcptx_query_ds_repeater(p_hdcp);
		if (!ds_repeater || p_hdcp->hdcp_type != HDCP_VER_HDCP2X) {
			DPTX_HDCP_INFO("[%d] not propagate stream type, ds_rpt:%d, ds_hdcp:%d\n",
				update_scene, ds_repeater, p_hdcp->hdcp_type);
			mutex_unlock(&p_hdcp->stream_type_mutex);
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
			DPTX_HDCP_INFO("case[%d] stream type change from %d to %d\n",
				update_scene, cur_content_type, upstream_type);
			if (p_hdcp->cont_smng_method == 0) {
				p_hdcp->csm_updated = true;
				dptx_hdcptx_update_csm(p_hdcp);
				dptx_hdcptx2_rpt_smng_xfer_start(hdcp_comm->bind_hw_comm);
			} else if (p_hdcp->cont_smng_method == 1) {
				schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
			}
		}
	} else if (update_scene == UPSTREAM_INACTIVE) {
		/* update upstream type firstly */
		p_hdcp->saved_upstream_type = 0;
		if (hdcp_comm->suspend_flag) {
			DPTX_HDCP_INFO("exit hdmirx, suspend, not propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		if (!hdcp_comm->hpd_state) {
			DPTX_HDCP_INFO("exit hdmirx, hpd low, not propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		if (!hdcp_comm->ready) {
			DPTX_HDCP_INFO("exit hdmirx, tx unready, not propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}

		/*
		 * when switch from hdmirx channel to hdmitx home
		 * need to start hdcp auth if it's not enabled
		 */
		if (dptx_get_hdcptx_mode(p_hdcp) == 0) {
			DPTX_HDCP_INFO("exit hdmirx, need to enable hdcp\n");
			dptx_enable_hdcp(p_hdcp);
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		ds_repeater = dptx_hdcptx_query_ds_repeater(p_hdcp);
		if (!ds_repeater || p_hdcp->hdcp_type != HDCP_VER_HDCP2X) {
			DPTX_HDCP_INFO("[%d] not propagate stream type, ds_rpt:%d, ds_hdcp:%d\n",
				update_scene, ds_repeater, p_hdcp->hdcp_type);
			mutex_unlock(&p_hdcp->stream_type_mutex);
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
			DPTX_HDCP_INFO("case[%d] stream type change from %d to %d\n",
				update_scene, upstream_type, cur_content_type);
			if (p_hdcp->cont_smng_method == 0) {
				p_hdcp->csm_updated = true;
				dptx_hdcptx_update_csm(p_hdcp);
				dptx_hdcptx2_rpt_smng_xfer_start(hdcp_comm->bind_hw_comm);
			} else if (p_hdcp->cont_smng_method == 1) {
				schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
			}
		}
	} else if (update_scene == UPSTREAM_ACTIVE) {
		/* update upstream type firstly */
		p_hdcp->saved_upstream_type = 0x10 | upstream_type;
		if (hdcp_comm->suspend_flag) {
			DPTX_HDCP_INFO("enter hdmirx, suspend, not propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		if (!hdcp_comm->hpd_state) {
			DPTX_HDCP_INFO("enter hdmirx, hpd low, not propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		if (!hdcp_comm->ready) {
			DPTX_HDCP_INFO("enter rx, tx unready, not propagate stream type\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}

		/* make sure hdcp is enabled */
		if (dptx_get_hdcptx_mode(p_hdcp) == 0) {
			DPTX_HDCP_INFO("hdcp disabled, no need propagate stream type2\n");
			mutex_unlock(&p_hdcp->stream_type_mutex);
			return;
		}
		ds_repeater = dptx_hdcptx_query_ds_repeater(p_hdcp);
		if (!ds_repeater || p_hdcp->hdcp_type != HDCP_VER_HDCP2X) {
			DPTX_HDCP_INFO("[%d] not propagate stream type, ds_rpt:%d, ds_hdcp:%d\n",
				update_scene, ds_repeater, p_hdcp->hdcp_type);
			mutex_unlock(&p_hdcp->stream_type_mutex);
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
			DPTX_HDCP_INFO("case[%d] stream type change from %d to %d\n",
				update_scene, cur_content_type, upstream_type);
			if (p_hdcp->cont_smng_method == 0) {
				p_hdcp->csm_updated = true;
				dptx_hdcptx_update_csm(p_hdcp);
				dptx_hdcptx2_rpt_smng_xfer_start(hdcp_comm->bind_hw_comm);
			} else {
				schedule_delayed_work(&p_hdcp->req_reauth_wk, 0);
			}
		}
	}
	mutex_unlock(&p_hdcp->stream_type_mutex);
}

static void dptx_hdcptx_mode_set(struct hdcptx21_core *p_hdcp, unsigned int mode)
{
	if (!p_hdcp) {
		DPTX_ERROR("NULL hdcp private instance\n");
		return;
	}
	DPTX_HDCP_INFO("hdcptx_mode_set: %d\n", mode);

	if (mode == 0) {
		dptx_hdcptx_reset(p_hdcp);
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
	dptx_hdcptx_schedule_work(p_hdcp->hdcp_wq, &p_hdcp->timer_hdcp_start, 1, 0);
}

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
static bool dptx_hdcptx_need_ctrl_by_upstream(bool hdcp_rpt_en)
{
	if (!hdcp_rpt_en)
		return false;

	if (!get_rx_active_sts())
		return false;
	return true;
}

static void dptx_hdcptx_enable_mode(struct hdcptx21_core *p_hdcp, int hdcp_type)
{
	struct hdcptx_common *hdcptx_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp private instance\n");
		return;
	}
	hdcptx_comm = p_hdcp->bind_hdcptx_comm;
	if (hdcp_type == HDCP_START_NONE) {
		hdcptx_comm->hdcp_mode = 0x0;
		dptx_hdcptx_mode_set(p_hdcp, hdcp_type);
		dptx_ctrl_hdcp_gate(hdcptx_comm, 0, false);
	} else if (hdcp_type == HDCP_START_HDCP1X) {
		if (hdcptx_comm->efuse_dis_hdcp_tx14) {
			DPTX_ERROR("warning, efuse disable hdcptx14\n");
			return;
		}
		hdcptx_comm->hdcp_mode = hdcp_type;
		dptx_hdcptx_mode_set(p_hdcp, hdcp_type);
		dptx_ctrl_hdcp_gate(hdcptx_comm, hdcp_type, true);
	} else if (hdcp_type == HDCP_START_HDCP2X) {
		if (hdcptx_comm->efuse_dis_hdcp_tx22) {
			DPTX_ERROR("warning, efuse disable hdcptx22\n");
			return;
		}
		dptx_ctrl_hdcp_gate(hdcptx_comm, hdcp_type, true);
		hdcptx_comm->hdcp_mode = hdcp_type;
		dptx_hdcptx_mode_set(p_hdcp, hdcp_type);
	}
}

static void dptx_enable_hdcp(struct hdcptx21_core *p_hdcp)
{
	struct hdcptx_common *hdcp_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}
	hdcp_comm = p_hdcp->bind_hdcptx_comm;
	/*
	 * lstore: 0 by default, 0x11/0x12 for debug usage
	 * 0: enable hdcp mode based on stored key and downstream hdcp cap
	 * priority: hdcp2.x > hdcp1.x
	 * 0x12: force hdcp2.x
	 * 0x11: force hdcp1.x
	 */
	mutex_lock(&p_hdcp->hdcptx_auth_mutex);
	if (p_hdcp->hdcptx_start_type == HDCP_START_AUTO) {
		if (hdcp_comm->hdcp_mode != 0) {
			DPTX_HDCP_INFO("hdcp %d already enabled, exit\n",
						hdcp_comm->hdcp_mode);
			mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
			return;
		}

		if (hdcp_comm->hdcp_lstore == 0) {
			if (dptx_get_hdcptx_lstore(p_hdcp->bind_hdcptx_comm, HDCP_VER_HDCP2X) &&
				hdcp_comm->dw_hdcp22_cap) {
				/* enable hdcp gate */
				dptx_ctrl_hdcp_gate(hdcp_comm, 2, true);
				hdcp_comm->hdcp_mode = 2;
				dptx_hdcptx_mode_set(p_hdcp, 2);
			} else {
				if (p_hdcp->frl_rate > FRL_NONE &&
					p_hdcp->frl_rate < FRL_RATE_MAX) {
					hdcp_comm->hdcp_mode = 0;
					DPTX_HDCP_INFO("not enable hdcp1x under FRL\n");
				} else if (dptx_get_hdcptx_lstore(hdcp_comm, HDCP_VER_HDCP1X)) {
					dptx_ctrl_hdcp_gate(hdcp_comm, 1, true);
					hdcp_comm->hdcp_mode = 1;
					dptx_hdcptx_mode_set(p_hdcp, 1);
				} else {
					dptx_ctrl_hdcp_gate(hdcp_comm, 0, false);
					hdcp_comm->hdcp_mode = 0;
				}
			}
		} else if (hdcp_comm->hdcp_lstore & 0x2) {
			if (dptx_get_hdcptx_lstore(p_hdcp->bind_hdcptx_comm, HDCP_VER_HDCP2X) &&
				hdcp_comm->dw_hdcp22_cap) {
				dptx_ctrl_hdcp_gate(hdcp_comm, 2, true);
				hdcp_comm->hdcp_mode = 2;
				dptx_hdcptx_mode_set(p_hdcp, 2);
			} else {
				dptx_ctrl_hdcp_gate(hdcp_comm, 0, false);
				hdcp_comm->hdcp_mode = 0;
			}
		} else if (hdcp_comm->hdcp_lstore & 0x1) {
			if (p_hdcp->frl_rate > FRL_NONE &&
				p_hdcp->frl_rate < FRL_RATE_MAX) {
				hdcp_comm->hdcp_mode = 0;
				DPTX_HDCP_INFO("should not enable hdcp1.4 under FRL mode\n");
			} else if (dptx_get_hdcptx_lstore(hdcp_comm, HDCP_VER_HDCP1X)) {
				dptx_ctrl_hdcp_gate(hdcp_comm, 1, true);
				hdcp_comm->hdcp_mode = 1;
				dptx_hdcptx_mode_set(p_hdcp, 1);
			} else {
				dptx_ctrl_hdcp_gate(hdcp_comm, 0, false);
				hdcp_comm->hdcp_mode = 0;
			}
		} else {
			DPTX_DEBUG_HDCP("debug: not enable hdcp as no key stored\n");
		}
	} else {
		/* for drm start hdcp or debug node */
		dptx_hdcptx_enable_mode(p_hdcp, p_hdcp->hdcptx_start_type);
	}
	mutex_unlock(&p_hdcp->hdcptx_auth_mutex);
}

static void dptx_hdcptx_start_hdcp_handler(struct work_struct *work)
{
	struct hdcptx21_core *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core, work_start_hdcp);
	struct hdcptx_common *hdcptx_comm = NULL;
	unsigned long timeout_sec;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}
	hdcptx_comm = p_hdcp->bind_hdcptx_comm;

	if (dptx_hdcptx_need_ctrl_by_upstream(hdcptx_comm->hdcp_rpt_en)) {
		/* move is_passthrough_switch to p_hdcp */
		if (p_hdcp->is_passthrough_switch) {
			DPTX_INFO("enable hdcp by passthrough switch mode\n");
			/* note: hdcp should only be started when hdmi signal ready */
			if (!hdcptx_comm->ready || !hdcptx_comm->hpd_state) {
				DPTX_INFO("signal ready: %d, hpd_state: %d, eixt hdcp\n",
					hdcptx_comm->ready, hdcptx_comm->hpd_state);
				p_hdcp->is_passthrough_switch = 0;
				return;
			}
			if (hdcptx_comm->avmute_op)
				//todo avmute?
				hdcptx_comm->avmute_op(hdcptx_comm->bind_tx_comm,
					CLR_AVMUTE, 0);
			dptx_enable_hdcp(p_hdcp);
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
			DPTX_INFO("hdcp should started by upstream, wait...\n");
			/* timeout period: hdcp1.4: 5S, hdcp2.2: 2S */
			if (hdcptx_comm->dw_hdcp22_cap)
				timeout_sec = 2;
			else
				timeout_sec = p_hdcp->up_hdcp_timeout_sec;
			schedule_delayed_work(&p_hdcp->work_up_hdcp_timeout,
					msecs_to_jiffies(timeout_sec * 1000));
		}
	} else {
		if (!hdcptx_comm->ready || !hdcptx_comm->hpd_state) {
			DPTX_INFO("signal ready: %d, hpd_state: %d, eixt hdcp2\n",
				hdcptx_comm->ready, hdcptx_comm->hpd_state);
			p_hdcp->is_passthrough_switch = 0;
			return;
		}
		if (hdcptx_comm->avmute_op)
			hdcptx_comm->avmute_op(hdcptx_comm->bind_tx_comm,
				CLR_AVMUTE, 0);
		dptx_enable_hdcp(p_hdcp);
	}
	/* clear after start hdcp */
	p_hdcp->is_passthrough_switch = 0;
}

static void dptx_hdcptx_up_hdcp_timeout_handler(struct work_struct *work)
{
	struct hdcptx21_core *p_hdcp = container_of((struct delayed_work *)work,
		struct hdcptx21_core, work_up_hdcp_timeout);
	struct hdcptx_common *hdcptx_comm = NULL;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}

	hdcptx_comm = p_hdcp->bind_hdcptx_comm;

	if (dptx_hdcptx_need_ctrl_by_upstream(hdcptx_comm->hdcp_rpt_en)) {
		DPTX_INFO("enable hdcp as wait upstream hdcp timeout\n");
		/* note: hdcp should only be started when hdmi signal ready */
		if (!hdcptx_comm->ready || !hdcptx_comm->hpd_state) {
			DPTX_INFO("signal ready: %d, hpd_state: %d, exit hdcp\n",
				hdcptx_comm->ready, hdcptx_comm->hpd_state);
			return;
		}
		if (hdcptx_comm->avmute_op)
			hdcptx_comm->avmute_op(hdcptx_comm->bind_tx_comm,
				CLR_AVMUTE, 0);
		dptx_enable_hdcp(p_hdcp);
	} else {
		DPTX_INFO("wait upstream hdcp timeout, but now not in hdmirx channel\n");
	}
}

void dptx_tee_comm_dev_reg(struct miscdevice *hdcp_misc_dev, const struct file_operations *fops)
{
	int ret;

	if (!hdcp_misc_dev || !fops) {
		DPTX_ERROR("null hdcp communicate device or file ops\n");
		return;
	}
	hdcp_misc_dev->minor = MISC_DYNAMIC_MINOR;
	hdcp_misc_dev->name = "tee_comm_hdcp";
	hdcp_misc_dev->fops = fops;

	ret = misc_register(hdcp_misc_dev);
	if (ret < 0)
		DPTX_ERROR("misc_register fail\n");
}

static void dptx_hdcptx_debug_func(void *hdcptx_instance, const char *buf)
{
	struct hdcptx21_core *p_hdcp = (struct hdcptx21_core *)hdcptx_instance;
	struct hdcptx_common *hdcptx_comm = NULL;
	int ret;
	unsigned long value;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}

	hdcptx_comm = p_hdcp->bind_hdcptx_comm;
	if (strncmp(buf, "reauth_dbg", 10) == 0) {
		ret = kstrtoul(buf + 10, 10, &value);
		hdcp_reauth_dbg = value;
		DPTX_INFO("set hdcp_reauth_dbg :%lu\n", hdcp_reauth_dbg);
	} else if (strncmp(buf, "streamtype_dbg", 14) == 0) {
		ret = kstrtoul(buf + 14, 10, &value);
		streamtype_dbg = value;
		DPTX_INFO("set streamtype_dbg :%lu\n", streamtype_dbg);
	} else if (strncmp(buf, "en_fake_rcv_id", 14) == 0) {
		ret = kstrtoul(buf + 14, 10, &value);
		en_fake_rcv_id = value;
		DPTX_INFO("set en_fake_rcv_id :%lu\n", en_fake_rcv_id);
	} else if (strncmp(buf, "hdcp_timeout", 12) == 0) {
		ret = kstrtoul(buf + 12, 10, &value);
		if (p_hdcp)
			p_hdcp->up_hdcp_timeout_sec = value;
	} else if (strncmp(buf, "avmute_ms", 9) == 0) {
		ret = kstrtoul(buf + 9, 10, &value);
		avmute_ms = value;
		DPTX_INFO("set avmute_ms :%lu\n", avmute_ms);
	} else if (strncmp(buf, "vid_mute_ms", 10) == 0) {
		ret = kstrtoul(buf + 10, 10, &value);
		vid_mute_ms = value;
		DPTX_INFO("set vid_mute_ms :%lu\n", vid_mute_ms);
	} else if (strncmp(buf, "hdcp_result", 11) == 0) {
		DPTX_INFO("hdcp filtered result: hdcp22: %d topo: %d, hdcp14: %d\n",
		dptx_get_hdcptx_result(hdcptx_comm, 2),
		dptx_hdcptx22_topo_ctrl(hdcptx_comm, GET_HDCP22_TOPO, 0),
		dptx_get_hdcptx_result(hdcptx_comm, 1));
	} else if (strncmp(buf, "propagate_stream_type", 21) == 0) {
		if (p_hdcp->ds_repeater && p_hdcp->hdcp_type == HDCP_VER_HDCP2X) {
			DPTX_INFO("%d\n", p_hdcp->csm_message.streamid_type & 0xFF);
		} else {
			DPTX_INFO("no stream type, as ds_repeater: %d, hdcp_type: %d\n",
				p_hdcp->ds_repeater, p_hdcp->hdcp_type);
		}
	} else if (strncmp(buf, "cont_smng_method", 16) == 0) {
		/*
		 * content stream management update method:
		 * when upstream side received new content stream
		 * management message, there're two method to
		 * update content stream type that propagated
		 * to downstream hdcp2.3 repeater:
		 * 0(default): only send content stream management
		 * message with new stream type to downstream
		 * 1: init new re-auth with downstream
		 */
		if (isdigit(buf[16])) {
			value = buf[16] - '0';
			DPTX_INFO("set cont_smng_method as %d\n", value);
			p_hdcp->cont_smng_method = value;
		}
	}  else if (strncmp(buf, "def_stream_type", 15) == 0) {
		if (isdigit(buf[15])) {
			value = buf[15] - '0';
			DPTX_INFO("set def_stream_type as %d\n", value);
			p_hdcp->def_stream_type = !!value;
		}
	} else if (strncmp(buf, "is_passthrough_switch", 21) == 0) {
		if (isdigit(buf[21])) {
			value = buf[21] - '0';
			DPTX_INFO("set is_passthrough_switch as %d\n", value);
			p_hdcp->is_passthrough_switch = !!value;
		}
	} else if (strncmp(buf, "not_restart_hdcp", 16) == 0) {
		if (!p_hdcp)
			return;
		if (isdigit(buf[16])) {
			value = buf[16] - '0';
			DPTX_INFO("set not_restart_hdcp as %d\n", value);
			hdcptx_comm->not_restart_hdcp = !!value;
		}
	} else if (strncmp(buf, "hdcp_delay", 10) == 0) {
		if (!p_hdcp)
			return;
		ret = kstrtoul(buf + 10, 10, &value);
		DPTX_INFO("hdcp_delay :%d\n", value);
		hdcptx_comm->hdcp_debug_delay = value;
	} else if (strncmp(buf, "load_14key", 10) == 0) {
		DPTX_INFO("hdcp1.4 key load result: %d\n",
			dptx_validate_hdcp_key(hdcptx_comm, 0x1));
	}
}

/* action in suspend/plugout handler, should not be done when disable_module */
static void dptx_hdcptx_reset_param(void *hdcptx_instance)
{
	struct hdcptx21_core *p_hdcp = (struct hdcptx21_core *)hdcptx_instance;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm or NULL hdcp_private instance\n");
		return;
	}

	dptx_rst_stream_type(p_hdcp);
	p_hdcp->saved_upstream_type = 0;
	p_hdcp->rx_update_flag = 0;
	p_hdcp->is_passthrough_switch = 0;
	p_hdcp->bind_hdcptx_comm->dw_hdcp22_cap = false;
	/* clear audio/video mute flag of stream type */
	if (p_hdcp->bind_hdcptx_comm->video_mute_op)
		p_hdcp->bind_hdcptx_comm->video_mute_op(p_hdcp->bind_hdcptx_comm->bind_hw_comm,
			1, VIDEO_MUTE_PATH_2);
	if (p_hdcp->bind_hdcptx_comm->audio_mute_op)
		p_hdcp->bind_hdcptx_comm->audio_mute_op(p_hdcp->bind_hdcptx_comm->bind_tx_comm,
			1, AUDIO_MUTE_PATH_3);
}

void dptx_tee_comm_dev_unreg(struct miscdevice *hdcp_misc_dev)
{
	misc_deregister(hdcp_misc_dev);
}

static void dptx_hdcptx_enable(void *hdcptx_instance, int hdcp_type, unsigned long delay)
{
	struct hdcptx21_core *p_hdcp = (struct hdcptx21_core *)hdcptx_instance;

	if (!p_hdcp) {
		DPTX_ERROR("NULL hdcp instance\n");
		return;
	}
	p_hdcp->hdcptx_start_type = hdcp_type;
	schedule_delayed_work(&p_hdcp->work_start_hdcp, msecs_to_jiffies(delay));
}

/* note: it maybe used in timer */
static u8 dptx_hdcptx_reauth_request(void *hdcptx_priv, u8 hdcp_version)
{
	u8 upstream_type = 0;
	u8 update_scene = 0;
	struct hdcptx21_core *p_hdcp;

	if (!hdcptx_priv) {
		DPTX_ERROR("NULL hdcp_private instance\n");
		return 0;
	}
	p_hdcp = hdcptx_priv;

	DPTX_HDCP_INFO("hdcptx_reauth_request hdcp_ver 0x%x\n", hdcp_version);

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
		 * there are some special source devices, firstly do
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
				DPTX_HDCP_INFO("block stream to ds as upstream type = 1\n");
			}
		} else {
			/* this unmute operation should have the lowest priority */
			p_hdcp->stream_mute = false;
			schedule_delayed_work(&p_hdcp->stream_mute_wk, 0);
			DPTX_DEBUG_HDCP("non-block stream to ds as upstream type = 0\n");
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
		DPTX_DEBUG_HDCP("upstream source inactive now, unmute\n");
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
				DPTX_DEBUG_HDCP("[2]block stream to ds as upstream type = 1\n");
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

static void dptx_hdcp22_uninit(void *hdcptx_instance)
{
	struct hdcptx21_core *p_hdcp = (struct hdcptx21_core *)hdcptx_instance;

	if (!p_hdcp || !p_hdcp->bind_hdcptx_comm) {
		DPTX_ERROR("NULL p_hdcp or NULL hdcptx_comm instance\n");
		return;
	}
	dptx_tee_comm_dev_unreg(&p_hdcp->hdcp_misc_dev);
	dptx_disable_hdcp(p_hdcp);
	destroy_workqueue(p_hdcp->hdcp_wq);
	kfree(p_hdcp->p_ksv_lists);
	kfree(p_hdcp);
}

static struct hdcptx_api dptx_hdcptx_api_init = {
	.get_hdcptx_lstore = dptx_get_hdcptx_lstore,
	.get_hdcptx_result = dptx_get_hdcptx_result,
	.hdcptx22_topo_ctrl = dptx_hdcptx22_topo_ctrl,
	.hdcptx_reset_param = dptx_hdcptx_reset_param,
	.hdcptx_enable = dptx_hdcptx_enable,
	.hdcptx_disable = dptx_disable_hdcp,
	.get_hdcprx_ver = dptx_hdcptx_get_hdcprx_ver,
	.get_hdcptx_mode = dptx_get_hdcptx_mode,
	.hdcptx_reauth_req = dptx_hdcptx_reauth_request,
	.hdcptx_uninit = dptx_hdcp22_uninit,
	.hdcptx_debug = dptx_hdcptx_debug_func,
	.hdcptx14_get_bksv = dptx_hdcptx1_ds_bksv_read,
	.hdcptx_validate_key = dptx_validate_hdcp_key,
};

void *dptx20_hdcp2x_init(void *hdcp_data)
{
	struct hdcptx21_core *p_hdcp = NULL;
	struct hdcptx_common *hdcptx_comm = (struct hdcptx_common *)hdcp_data;

	if (!hdcptx_comm) {
		DPTX_ERROR("NULL hdcptx_comm instance\n");
		return NULL;
	}
	p_hdcp = kzalloc(sizeof(*p_hdcp), GFP_KERNEL);
	if (!p_hdcp) {
		DPTX_ERROR("no enough mem for hdcp private struct\n");
		return NULL;
	}

	DPTX_INFO("hdcp init\n");
	/* hdcp private struct init */
	p_hdcp->bind_hdcptx_comm = hdcptx_comm;
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
	/* for hdcp repeater */
	p_hdcp->def_stream_type = DEFAULT_STREAM_TYPE;
	p_hdcp->bind_hdcptx_comm->not_restart_hdcp = false;
	/* wait for upstream start hdcp auth 5S */
	p_hdcp->up_hdcp_timeout_sec = 5;
	p_hdcp->bind_hdcptx_comm->hdcptx_api = &dptx_hdcptx_api_init;

	dptx_rst_stream_type(p_hdcp);

	p_hdcp->p_ksv_lists =
		kmalloc((HDCP1X_MAX_TX_DEV_SRC + 1) * sizeof(struct hdcp_ksv_t), GFP_KERNEL);

	/* hdcp related work, specific for auth flow in HW core */
	p_hdcp->hdcp_wq = alloc_workqueue(DEVICE_NAME_DP, WQ_HIGHPRI | WQ_CPU_INTENSIVE, 0);
	dptx_init_hdcp_works(&p_hdcp->timer_hdcp_rcv_auth,
		dptx_hdcptx_check_ds_auth_whandler, "hdcp_rcv_auth");
	dptx_init_hdcp_works(&p_hdcp->timer_hdcp_rpt_auth,
		dptx_hdcptx_check_ds_auth_whandler, "hdcp_rpt_auth");
	dptx_init_hdcp_works(&p_hdcp->timer_bksv_poll_done,
		dptx_hdcptx_check_bksv_done_whandler, "bksv_poll_done");
	dptx_init_hdcp_works(&p_hdcp->timer_hdcp_start,
		dptx_hdcptx_check_update_whandler, "hdcp_start");
	dptx_init_hdcp_works(&p_hdcp->timer_ddc_check_nak,
		dptx_hdcp_ddc_check_nack_whandler, "ddc_check_nak");
	dptx_init_hdcp_works(&p_hdcp->timer_hdcp_auth_fail_retry,
		dptx_hdcp_check_auth_fail_retry_whandler, "hdcp_auth_fail_retry");
	dptx_init_hdcp_works(&p_hdcp->timer_update_csm, dptx_hdcptx_update_csm_whandler,
					"update_csm");

	INIT_DELAYED_WORK(&p_hdcp->ksv_notify_wk, dptx_hdcptx_notify_rpt_info);
	INIT_DELAYED_WORK(&p_hdcp->req_reauth_wk, dptx_hdcptx_req_reauth_whandler);
	INIT_DELAYED_WORK(&p_hdcp->stream_mute_wk, dptx_hdcp_stream_mute_whandler);
	INIT_DELAYED_WORK(&p_hdcp->stream_type_wk, dptx_hdcptx_propagate_stream_type);

	/* hdcp related work for start hdcp auth, scheduled in system workqueue */
	mutex_init(&p_hdcp->hdcptx_auth_mutex);
	INIT_DELAYED_WORK(&p_hdcp->work_start_hdcp, dptx_hdcptx_start_hdcp_handler);
	INIT_DELAYED_WORK(&p_hdcp->work_up_hdcp_timeout, dptx_hdcptx_up_hdcp_timeout_handler);
	mutex_init(&p_hdcp->stream_type_mutex);

	dptx_tee_comm_dev_reg(&p_hdcp->hdcp_misc_dev, &dptx_hdcp_comm_file_operations);

	return (void *)p_hdcp;
}
