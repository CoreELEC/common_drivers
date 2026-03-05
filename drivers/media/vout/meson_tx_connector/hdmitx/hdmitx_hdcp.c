// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include<linux/kthread.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_common.h>
#include "hdmitx_log.h"
#include "meson_tx_event_mgr.h"
#include "../hdcp/hdmitx_hdcp.h"

static int hdmi_authenticated;
MODULE_PARM_DESC(hdmi_authenticated, "\n hdmi_authenticated\n");
module_param(hdmi_authenticated, int, 0444);

/* bit1: hdcp22 key, bit0: hdcp1.4 key */
unsigned int hdmitx_common_get_key_store(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!", __func__);
		return 0;
	}

	if (tx_comm->hdcptx_comm.hdcp_lstore < 0x10) {
		tx_comm->hdcptx_comm.hdcp_lstore = 0;
		if (hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_14_LSTORE, NULL, NULL))
			tx_comm->hdcptx_comm.hdcp_lstore |= HDCP_MODE14;
		if (hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_22_LSTORE, NULL, NULL))
			tx_comm->hdcptx_comm.hdcp_lstore |= HDCP_MODE22;
	}

	HDMITX_DEBUG("%s tx hdcp [%d]\n", __func__, tx_comm->hdcptx_comm.hdcp_lstore);
	return tx_comm->hdcptx_comm.hdcp_lstore & (HDCP_MODE14 | HDCP_MODE22);
}

unsigned int hdcptx_get_rx_version(struct hdmitx_common *tx_comm)
{
	unsigned int hdcprx_cap = 0;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!", __func__);
		return 0;
	}
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
	} else if (hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_22_LSTORE, NULL, NULL) &&
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP22_GET_RX_VER, NULL, NULL)) {
		tx_comm->hdcptx_comm.dw_hdcp22_cap = 1;
		hdcprx_cap = HDCP_MODE22 | HDCP_MODE14;
	} else {
		/* must assume RX support HDCP14, otherwise affect 1A-03 */
		tx_comm->hdcptx_comm.dw_hdcp22_cap = 0;
		hdcprx_cap = HDCP_MODE14;
	}
	return hdcprx_cap;
}

static void hdcptx_result_notify_drm(struct hdmitx_common *tx_comm, int auth)
{
	void *cb_data = NULL;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!", __func__);
		return;
	}

	HDMITX_DEBUG_HDCP("HDCP cb %d\n", auth);

	cb_data = tx_comm->hdcptx_comm.tx_hdcp_cb.data;

	if (tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify)
		tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify(cb_data,
			tx_comm->hdcptx_comm.hdcp_mode,
			auth);
}

/* 40 * 200ms = 8s */
#define HDCP_AUTH_TIMEOUT (40)

/* hdcp monitor task, check hdcp auth status every 200ms */
int hdmitx_hdcp_stat_monitor_task(void *data)
{
	static int auth_stat;
	struct hdmitx_common *tx_comm = (struct hdmitx_common *)data;

	if (!tx_comm) {
		HDMITX_ERROR("NULL tx_comm instance\n", __func__);
		return -1;
	}

	while (!kthread_should_stop() && tx_comm->hpd_event != 0xff) {
		hdmi_authenticated = hdmitx_hw_cntl(tx_comm->tx_hw,
			HDCP_GET_AUTH_RESULT, NULL, NULL);
		/* uevent report */
		hdmitx_set_uevent(tx_comm, TX_HDCP_EVENT, hdmi_authenticated);
		/* status update, only collect the metric when hdmi is plugged in. */
		if (tx_comm->base.hpd_state == 1)
			hdmitx_current_status(tx_comm, auth_stat ?
				TX_HDCP_AUTH_SUCCESS : TX_HDCP_AUTH_FAILURE);
		tx_comm->hdcptx_comm.hdcp_auth_result = auth_stat;
		hdcptx_result_notify_drm(tx_comm, auth_stat);
		if (auth_stat != hdmi_authenticated) {
			auth_stat = hdmi_authenticated;
			HDMITX_HDCP_INFO("mode %d, auth: %d\n",
				tx_comm->hdcptx_comm.hdcp_mode, auth_stat);
			if (auth_stat == 1) {
				tx_comm->hdcptx_comm.hdcp_fail_cnt = 0;
				tx_comm->hdcptx_comm.hdcp_mode_fallback_state =
				HDCP_FALLBACK_LOCKED;
				tx_comm->hdcptx_comm.hdcp_state_mode = HDCP_RESULT_SUCCESS;
			}
		}
		if (hdmi_authenticated == 0 && tx_comm->hdcptx_comm.hdcp_mode == 2 &&
			tx_comm->hdcptx_comm.hdcp_mode_fallback_state == HDCP_FALLBACK_NONE) {
			tx_comm->hdcptx_comm.hdcp_fail_cnt++;
			if (tx_comm->hdcptx_comm.hdcp_fail_cnt > HDCP_AUTH_TIMEOUT) {
				tx_comm->hdcptx_comm.hdcptx_start_type = HDCP_START_HDCP1X;
				hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_ENABLE, NULL, NULL);
				tx_comm->hdcptx_comm.hdcp_fail_cnt = 0;
				tx_comm->hdcptx_comm.hdcp_mode_fallback_state =
					HDCP_FALLBACK_LOCKED;
			}
		}
		msleep_interruptible(200);
	}
	return 0;
}

/* use drm */
int drm_hdmitx_common_hdcp_init(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!\n", __func__);
		return -EINVAL;
	}

	if (tx_comm->hdcptx_comm.drm_hdcp_init) {
		tx_comm->hdcptx_comm.drm_hdcp_init(tx_comm);
		return 0;
	}
	HDMITX_DEBUG_HDCP("%s\n", __func__);

	return -EINVAL;
}
EXPORT_SYMBOL(drm_hdmitx_common_hdcp_init);

int drm_hdmitx_common_hdcp_exit(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!\n", __func__);
		return -EINVAL;
	}

	if (tx_comm->hdcptx_comm.drm_hdcp_exit) {
		tx_comm->hdcptx_comm.drm_hdcp_exit(tx_comm);
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(drm_hdmitx_common_hdcp_exit);

int drm_hdmitx_common_hdcp_enable(struct hdmitx_common *tx_comm, int hdcp_type)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!\n", __func__);
		return -EINVAL;
	}

	if (tx_comm->hdcptx_comm.drm_hdcp_enable) {
		tx_comm->hdcptx_comm.drm_hdcp_enable(tx_comm, hdcp_type);
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(drm_hdmitx_common_hdcp_enable);

int drm_hdmitx_common_hdcp_disable(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!\n", __func__);
		return -EINVAL;
	}
	if (tx_comm->hdcptx_comm.hdcp_state_mode == HDCP_STATE_DISCONNECT ||
		tx_comm->hdcptx_comm.hdcp_state_mode == HDCP_STATE_STOP)
		return 0;
	tx_comm->hdcptx_comm.hdcp_state_mode = HDCP_STATE_STOP;

	if (tx_comm->hdcptx_comm.drm_hdcp_disable) {
		tx_comm->hdcptx_comm.drm_hdcp_disable(tx_comm);
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(drm_hdmitx_common_hdcp_disable);

int drm_hdmitx_common_hdcp_disconnect(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!\n", __func__);
		return -EINVAL;
	}

	if (tx_comm->hdcptx_comm.hdcp_state_mode == HDCP_STATE_DISCONNECT ||
		tx_comm->hdcptx_comm.hdcp_state_mode == HDCP_STATE_STOP)
		return 0;

	tx_comm->hdcptx_comm.hdcp_state_mode = HDCP_STATE_DISCONNECT;
	if (tx_comm->hdcptx_comm.drm_hdcp_disconnect) {
		tx_comm->hdcptx_comm.drm_hdcp_disconnect(tx_comm);
		return 0;
	}
	return -EINVAL;
}
EXPORT_SYMBOL(drm_hdmitx_common_hdcp_disconnect);

/* bit1: hdcp22, bit0: hdcp1.4 */
unsigned int drm_hdmitx_common_get_tx_hdcp_cap(struct hdmitx_common *tx_comm)
{
	if (tx_comm && tx_comm->hdcptx_comm.get_tx_hdcp_cap)
		return tx_comm->hdcptx_comm.get_tx_hdcp_cap(tx_comm);
	else
		return 0;
}
EXPORT_SYMBOL(drm_hdmitx_common_get_tx_hdcp_cap);

/* bit1: hdcp22, bit0: hdcp1.4 */
unsigned int drm_hdmitx_common_get_rx_hdcp_cap(struct hdmitx_common *tx_comm)
{
	unsigned int hdcprx_cap = 0;

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!\n", __func__);
		return 0;
	}

	hdcprx_cap = hdcptx_get_rx_version(tx_comm);
	HDMITX_DEBUG("%s rx hdcp [%d]\n", __func__, hdcprx_cap);

	return hdcprx_cap;
}
EXPORT_SYMBOL(drm_hdmitx_common_get_rx_hdcp_cap);

int drm_hdmitx_common_register_hdcp_notify(struct hdmitx_common *tx_comm,
					   struct connector_hdcp_cb *cb)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm instance!\n", __func__);
		return -EINVAL;
	}

	if (!cb) {
		HDMITX_ERROR("%s NULL callback!\n", __func__);
		return -EINVAL;
	}

	if (tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify)
		HDMITX_ERROR("register hdcp notify again!?\n");

	tx_comm->hdcptx_comm.tx_hdcp_cb.hdcp_notify = cb->hdcp_notify;
	tx_comm->hdcptx_comm.tx_hdcp_cb.data = cb->data;

	HDMITX_DEBUG("%s\n", __func__);
	return 0;
}
EXPORT_SYMBOL(drm_hdmitx_common_register_hdcp_notify);

/*
 * get downstream hdcp topo info:
 * return 1 if downstream devices are all capable of hdcp2.2/2.3
 * return 0 if downstream contains hdcp1.4 or hdcp2.0 legacy device
 */
int drm_hdmitx_common_get_dw_hdcp_topo_info(struct hdmitx_common *tx_comm)
{
	int hdcp22_topo = 0;

	hdcp22_topo = hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_GET_TOPO_INFO, NULL, NULL);
	HDMITX_DEBUG("%s hdcp22_topo: %d\n", __func__, hdcp22_topo);

	return hdcp22_topo;
}
EXPORT_SYMBOL(drm_hdmitx_common_get_dw_hdcp_topo_info);
/* use drm end */

void tee_comm_dev_reg(struct miscdevice *hdcp_misc_dev, const struct file_operations *fops)
{
	int ret;

	if (!hdcp_misc_dev || !fops) {
		HDMITX_ERROR("null hdcp communicate device or file ops\n");
		return;
	}
	hdcp_misc_dev->minor = MISC_DYNAMIC_MINOR;
	hdcp_misc_dev->name = "tee_comm_hdcp";
	hdcp_misc_dev->fops = fops;

	ret = misc_register(hdcp_misc_dev);
	if (ret < 0)
		HDMITX_ERROR("%s misc_register fail\n", __func__);
}

void tee_comm_dev_unreg(struct miscdevice *hdcp_misc_dev)
{
	misc_deregister(hdcp_misc_dev);
}

void hdmitx_set_hdcp_mode(struct hdmitx_common *tx_comm)
{
	if (tx_comm && tx_comm->hdcptx_comm.set_hdcp_mode)
		tx_comm->hdcptx_comm.set_hdcp_mode(tx_comm);
}

int hdmitx_get_hdcp_ver(struct hdmitx_common *tx_comm, char *buf, int len)
{
	if (tx_comm && tx_comm->hdcptx_comm.get_hdcp_ver)
		return tx_comm->hdcptx_comm.get_hdcp_ver(tx_comm, buf, len);
	else
		return -EINVAL;
}
