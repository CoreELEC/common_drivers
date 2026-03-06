/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_TX_HDCP_H
#define __HDMI_TX_HDCP_H
#include <linux/miscdevice.h>

#include <drm/amlogic/meson_connector_dev.h>

/* linux/drm hdcp usage only start */

/* ioctl numbers */
enum {
	TEE_HDCP_START,
	/*
	 * TEE_HDCP_END not used, but should not change position
	 * as the later ioctl commands are used
	 */
	TEE_HDCP_END,
	HDCP_DAEMON_LOAD_END,
	HDCP_DAEMON_REPORT,
};

#define TEE_HDCP_IOC_START    _IOW('P', TEE_HDCP_START, int)
#define TEE_HDCP_IOC_END    _IOW('P', TEE_HDCP_END, int)
#define HDCP_DAEMON_IOC_LOAD_END    _IOW('P', HDCP_DAEMON_LOAD_END, int)
#define HDCP_DAEMON_IOC_REPORT    _IOR('P', HDCP_DAEMON_REPORT, int)

enum {
	HDCP_TX22_DISCONNECT = 0,
	HDCP_TX22_START,
	HDCP_TX22_STOP
};

enum {
	HDCP22_DAEMON_LOADING = 0,
	HDCP22_DAEMON_DONE,
	HDCP22_DAEMON_TIMEOUT
};

#define HDCP_AUTH_TIMEOUT (40) /*40*200ms = 8s*/
#define HDCP22_LOAD_TIMEOUT (160)
#define TIMER_CHECK	(1 * HZ / 2)
#define TIMER_CHK_CNT 60

/* only for linux/drm */
struct hdcptx20_core_priv {
	/* 0: null hdcp 1: hdcp14 2: hdcp22 */
	unsigned int hdcp_execute_type;
	bool hdcp_en;

	/* for hdcp_tx22 daemon load check */
	int hdcp22_daemon_state;
	unsigned int key_chk_cnt;
	struct timer_list daemon_load_timer;
	struct delayed_work notify_work;

	/* for poll status by hdcp_tx22 daemon */
	int hdcp_poll_report;
	int hdcp_report;
	wait_queue_head_t hdcp_comm_queue;

	struct hdmitx_common *bind_instance;
};

bool is_hdcp22_stop_state(struct hdmitx_common *tx_comm);
bool drm_hdcp_tx22_daemon_ready(struct hdmitx_common *tx_comm);
unsigned int hdcptx_get_key_store(struct hdmitx_common *tx_comm);
unsigned int hdcptx_get_rx_version(struct hdmitx_common *tx_comm);
void tee_comm_dev_reg(struct miscdevice *hdcp_misc_dev, const struct file_operations *fops);
void tee_comm_dev_unreg(struct miscdevice *hdcp_misc_dev);

/* linux/drm hdcp usage only end */

int hdcp_ksv_valid(unsigned char *dat);
int hdmitx20_hdcp_init(struct hdmitx20_dev *hdev);
void hdmitx20_hdcp_uninit(struct hdmitx20_dev *hdev);
int hdmitx_hdcp_stat_monitor_task(void *data);
int esm_init(void);
void esm_exit(void);
int hdmitx_hdcp_esm_kill(struct hdmitx_common *tx_comm);
#endif

