// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/kfifo.h>
#include <linux/platform_device.h>

#include "meson_tx_event_mgr.h"

#define TX_TRACE_SIZE (BIT(12)) /* 4k */

struct meson_tx_tracer {
	u32 event;
	int previous_event;
	int hdr10plus_event;
	struct kfifo log_fifo;
	struct meson_tx_event_mgr *event_mgr;
	/* to trigger userspace read fifo logs */
	struct work_struct uevent_work;
};

const char *meson_tx_event_to_str(enum tx_trace_log_bits event)
{
	switch (event) {
	case TX_HPD_PLUGOUT:
		return "tx_hpd_plugout\n";
	case TX_HPD_PLUGIN:
		return "tx_hpd_plugin\n";
	case TX_EDID_HDMI_DEVICE:
		return "tx_edid_hdmi_device\n";
	case TX_EDID_DVI_DEVICE:
		return "tx_edid_dvi_device\n";
	case TX_EDID_HDR_SUPPORT:
		return "tx_edid_hdr_device\n";
	case TX_EDID_DV_SUPPORT:
		return "tx_edid_dv_device\n";
	case TX_HDR_MODE_SDR:
		return "tx_hdr_mode_sdr\n";
	case TX_HDR_MODE_SMPTE2084:
		return "tx_hdr_mode_smpte2084\n";
	case TX_HDR_MODE_HLG:
		return "tx_hdr_mode_hlg\n";
	case TX_HDR_MODE_HDR10PLUS:
		return "tx_hdr_mode_hdr10plus\n";
	case TX_HDR_MODE_CUVA:
		return "tx_hdr_mode_cuva\n";
	case TX_HDR_MODE_DV_STD:
		return "tx_hdr_mode_dv_std\n";
	case TX_HDR_MODE_DV_LL:
		return "tx_hdr_mode_dv_ll\n";
	case TX_KMS_DISABLE_OUTPUT:
		return "output_disable\n";
	case TX_KMS_ENABLE_OUTPUT:
		return "output_enable\n";
	case TX_EDID_HEAD_ERROR:
		return "tx_edid_head_error\n";
	case TX_EDID_CHECKSUM_ERROR:
		return "tx_edid_checksum_error\n";
	case TX_EDID_I2C_ERROR:
		return "tx_edid_i2c_error\n";
	case TX_HDCP_AUTH_SUCCESS:
		return "tx_hdcp_auth_success\n";
	case TX_HDCP_AUTH_FAILURE:
		return "tx_hdcp_auth_failure\n";
	case TX_HDCP_HDCP_1_ENABLED:
		return "tx_hdcp_hdcp1_enabled\n";
	case TX_HDCP_HDCP_2_ENABLED:
		return "tx_hdcp_hdcp2_enabled\n";
	case TX_HDCP_NOT_ENABLED:
		return "tx_hdcp_not_enabled\n";
	case TX_HDCP_DEVICE_NOT_READY_ERROR:
		return "tx_hdcp_device_not_ready_error\n";
	case TX_HDCP_AUTH_NO_14_KEYS_ERROR:
		return "tx_hdcp_auth_no_14_keys_error\n";
	case TX_HDCP_AUTH_NO_22_KEYS_ERROR:
		return "tx_hdcp_auth_no_22_keys_error\n";
	case TX_HDCP_AUTH_READ_BKSV_ERROR:
		return "tx_hdcp_auth_read_bksv_error\n";
	case TX_HDCP_AUTH_VI_MISMATCH_ERROR:
		return "tx_hdcp_auth_vi_mismatch_error\n";
	case TX_HDCP_AUTH_TOPOLOGY_ERROR:
		return "tx_hdcp_auth_topology_error\n";
	case TX_HDCP_AUTH_R0_MISMATCH_ERROR:
		return "tx_hdcp_auth_r0_mismatch_error\n";
	case TX_HDCP_AUTH_REPEATER_DELAY_ERROR:
		return "tx_hdcp_auth_repeater_delay_error\n";
	case TX_HDCP_I2C_ERROR:
		return "tx_hdcp_i2c_error\n";
	case TX_KMS_ERROR:
		return "KMS_ERROR\n";
	case TX_KMS_SKIP:
		return "KMS_SKIP\n";
	case TX_AUDIO_MODE_SETTING:
		return "tx_audio_mode_setting\n";
	case TX_AUDIO_MUTE:
		return "tx_audio_mute\n";
	case TX_AUDIO_UNMUTE:
		return "tx_audio_unmute\n";
	default:
		return "Unknown event\n";
	}
}

static void meson_tx_log_events_handler(struct work_struct *work)
{
	static u32 cnt;
	struct meson_tx_tracer *tracer =
		container_of(work, struct meson_tx_tracer, uevent_work);

	meson_tx_event_mgr_send_uevent(tracer->event_mgr,
		TX_CUR_ST_EVENT, ++cnt, false);

	meson_tx_event_mgr_send_uevent(tracer->event_mgr,
		TX_TRACER_EVENT, tracer->event, false);
}

int meson_tx_tracer_write_event(struct meson_tx_tracer *tracer,
	enum tx_trace_log_bits event)
{
	const char *log_str;
	int ret = 0;

	if (!tracer)
		return -EINVAL;

	/*
	 * Found, skip duplicate logging
	 * For example, UEvent spamming of HDCP support (b/220687552)
	 */
	if (event == tracer->previous_event)
		return 0;
	tracer->previous_event = event;

	/* Play HDR10+ videos, HDMITX_HDR_MODE_HDR10PLUS only writes once */
	if (tracer->hdr10plus_event == TX_HDR_MODE_HDR10PLUS)
		return 0;

	log_str = meson_tx_event_to_str(event);

	if (event & TX_ERROR_MASK)
		HDMITX_WARNING(log_str);

	ret = kfifo_in(&tracer->log_fifo, log_str, strlen(log_str));
	if (!ret)
		kfifo_reset(&tracer->log_fifo);
	else
		tracer->hdr10plus_event = tracer->previous_event;

	tracer->event = event;
	/* after write trace, trigger uevent to save trace log */
	schedule_work(&tracer->uevent_work);

	return ret;
}

/* When hdr10plus mode ends, clear hdr10plus_event flag */
int meson_tx_tracer_clean_hdr10plus_event(struct meson_tx_tracer *tracer,
	enum tx_trace_log_bits event)
{
	if (event == TX_HDR_MODE_HDR10PLUS)
		tracer->hdr10plus_event = -1;

	return 0;
}

int meson_tx_tracer_read_event(struct meson_tx_tracer *tracer,
	char *buf, int read_len)
{
	if (!tracer)
		return -EINVAL;

	return kfifo_out(&tracer->log_fifo, buf, read_len);
}

struct meson_tx_tracer *meson_tx_tracer_create(struct meson_tx_event_mgr *event_mgr)
{
	struct meson_tx_tracer *instance = vmalloc(sizeof(*instance));
	int ret = 0;

	if (!instance) {
		TX_ERROR(event_mgr->log, "%s FAIL\n", __func__);
	} else {
		instance->event_mgr = event_mgr;
		instance->previous_event = -1;
		instance->hdr10plus_event = -1;
		ret = kfifo_alloc(&instance->log_fifo, TX_TRACE_SIZE, GFP_KERNEL);
		if (ret)
			TX_ERROR(event_mgr->log, "alloc tx_log_kfifo fail [%d]\n", ret);
		INIT_WORK(&instance->uevent_work, meson_tx_log_events_handler);
	}

	return instance;
}

int meson_tx_tracer_destroy(struct meson_tx_tracer *tracer)
{
	if (tracer) {
		kfifo_free(&tracer->log_fifo);
		vfree(tracer);
	}

	return 0;
}

/* start of uevent and notifier */
static const unsigned int meson_tx_extcon_cable[] = {
	EXTCON_DISP_HDMI,
	EXTCON_DISP_DP,
	EXTCON_NONE,
};

struct meson_tx_uevent {
	const enum meson_tx_event type;
	const char *env;
	int state;
};

static struct meson_tx_uevent tx_events[] = {
	{
		.type = TX_HPD_EVENT,
		.env = "tx_hpd=",
	},
	{
		.type = TX_HDCP_EVENT,
		.env = "tx_hdcp=",
	},
	{
		.type = TX_CUR_ST_EVENT,
		.env = "tx_current_status=",
	},
	{
		.type = TX_AUDIO_EVENT,
		.env = "tx_audio=",
	},
	{
		.type = TX_HDCPPWR_EVENT,
		.env = "tx_hdcppwr=",
	},
	{
		.type = TX_HDR_EVENT,
		.env = "tx_hdr=",
	},
	{
		.type = TX_RXSENSE_EVENT,
		.env = "tx_rxsense=",
	},
	{
		.type = TX_CEDST_EVENT,
		.env = "tx_cedst=",
	},
	{
		.type = TX_SOUNDBAR_EVENT,
		.env = "tx_soundbar=",
	},
	{
		.type = TX_VMODE_AUDIO_EVENT,
		.env = "tx_vmode_audio=",
	},
	{
		.type = TX_TRACER_EVENT,
		.env = "tx_tracer=",
	},
	{
		.type = TX_LINK_UNSTABLE,
		.env = "tx_link_unstable=",
	},
	{
		.type = TX_INCOMPATIBLE_CABLE,
		.env = "tx_incompatible_cable=",
	},
	{ /* end of tx_events[] */
		.type = TX_NONE_EVENT,
	},
};

/* for compliance with p/q/r/s/t, need both extcon and hdmi_event
 * there are mixed android/kernel combination, P/Q
 * only listen to extcon; while R/S (framework) only listen to uevent
 * t need listen to extcon(only hdmi hpd) and uevent
 */
struct meson_tx_event_mgr *meson_tx_event_mgr_create(struct meson_tx_dev *tx_dev)
{
	struct meson_tx_event_mgr *instance = vzalloc(sizeof(*instance));
	struct blocking_notifier_head *notify_head = &instance->tx_event_notify_list;
	int ret = 0;

	/*extcon event*/
	instance->log = &tx_dev->tx_log;
	instance->tx_extcon = devm_extcon_dev_allocate(tx_dev->pdev,
						       meson_tx_extcon_cable);
	instance->attached_extcon_dev = tx_dev->pdev;
	instance->conn_type = tx_dev->conn_dev.connector_type;
	if (IS_ERR(instance->tx_extcon)) {
		TX_ERROR(instance->log, "%s[%d] tx_extcon allocated failed\n",
			__func__, __LINE__);
		instance->tx_extcon = NULL;
	} else {
		ret = devm_extcon_dev_register(tx_dev->pdev, instance->tx_extcon);
		if (ret < 0) {
			TX_ERROR(instance->log, "%s[%d] tx_extcon register failed\n",
				__func__, __LINE__);

			instance->tx_extcon = NULL;
		}
	}

	/*blocking notifier*/
	BLOCKING_INIT_NOTIFIER_HEAD(notify_head);

	return instance;
}

void meson_tx_event_mgr_set_uevent_dev(struct meson_tx_event_mgr *event_mgr,
					struct device *dev, const char *uevent_prefix)
{
	/*uevent*/
	event_mgr->kobj = &dev->kobj;
	event_mgr->dev_name = uevent_prefix;
}

void meson_tx_event_mgr_suspend(struct meson_tx_event_mgr *event_mgr, bool suspend_flag)
{
	if (event_mgr)
		event_mgr->deep_suspend_flag = suspend_flag;
}

int meson_tx_event_mgr_destroy(struct meson_tx_event_mgr *event_mgr)
{
	if (!event_mgr)
		return 0;

	if (event_mgr->tx_extcon && event_mgr->attached_extcon_dev) {
		//devm_extcon_dev_unregister(event_mgr->attached_extcon_dev,
		//					event_mgr->tx_extcon);
		event_mgr->tx_extcon = 0;
	}

	vfree(event_mgr);

	return 0;
}

int meson_tx_event_mgr_set_uevent_state(struct meson_tx_event_mgr *event_mgr,
	enum meson_tx_event type, int state)
{
	unsigned int extcon_id;
	struct meson_tx_uevent *event;
	bool extcon_event = false;

	if (!event_mgr)
		return 0;

	if (event_mgr->conn_type > DRM_MODE_MESON_CONNECTOR_HDMI_START &&
		event_mgr->conn_type < DRM_MODE_MESON_CONNECTOR_HDMI_END)
		extcon_id = EXTCON_DISP_HDMI;
	else if (event_mgr->conn_type > DRM_MODE_CONNECTOR_MESON_TX_DP_START &&
		event_mgr->conn_type < DRM_MODE_CONNECTOR_MESON_TX_DP_END)
		extcon_id = EXTCON_DISP_DP;
	else
		extcon_id = EXTCON_NONE;

	for (event = tx_events; event->type != TX_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}

	if (event->type == TX_NONE_EVENT)
		return -EINVAL;

	event->state = state;

	if ((type == TX_AUDIO_EVENT || type == TX_SOUNDBAR_EVENT) &&
			event_mgr->tx_extcon) {
		extcon_set_state(event_mgr->tx_extcon, extcon_id, state);
		extcon_event = true;
	}

	TX_INFO(event_mgr->log, "[%s] event_type: %s%d, %d\n",
		__func__, event->env, state, extcon_event);

	return 0;
}

int meson_tx_event_mgr_send_uevent(struct meson_tx_event_mgr *uevent_mgr,
	enum meson_tx_event type, int state, bool force)
{
	unsigned int extcon_id;
	char env[MAX_UEVENT_LEN];
	struct meson_tx_uevent *event = tx_events;
	char *envp[2];
	int ret = 0;
	bool extcon_event = false;

	if (!uevent_mgr)
		return 0;

	if (uevent_mgr->conn_type > DRM_MODE_MESON_CONNECTOR_HDMI_START &&
		uevent_mgr->conn_type < DRM_MODE_MESON_CONNECTOR_HDMI_END)
		extcon_id = EXTCON_DISP_HDMI;
	else if (uevent_mgr->conn_type > DRM_MODE_CONNECTOR_MESON_TX_DP_START &&
		uevent_mgr->conn_type < DRM_MODE_CONNECTOR_MESON_TX_DP_END)
		extcon_id = EXTCON_DISP_DP;
	else
		extcon_id = EXTCON_NONE;

	for (event = tx_events; event->type != TX_NONE_EVENT; event++) {
		if (type == event->type)
			break;
	}

	if (event->type == TX_NONE_EVENT)
		return -EINVAL;

	if (event->state == state && !force)
		return 0;

	event->state = state;
	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "%s%s%d", uevent_mgr->dev_name, event->env, state);

	if (uevent_mgr->deep_suspend_flag) {
		if (type == TX_HPD_EVENT && uevent_mgr->tx_extcon) {
			extcon_set_state(uevent_mgr->tx_extcon, extcon_id, state);
			extcon_event = true;
		}
	} else {
		/*
		 * kobj is set by drm side when probe, it will crash
		 * when call kobject_uevent_env() if kobj is invalid;
		 * so if kobj is invalid, just update event state and
		 * not sent uevent (as system is not ready to listen
		 * to uevent at this moment, no need to send uevent)
		 */
		if (uevent_mgr->kobj)
			ret = kobject_uevent_env(uevent_mgr->kobj, KOBJ_CHANGE, envp);

		/*
		 * for AndroidU framework, audio need hdmi disconnect when suspend
		 * Android S system control will check kobject hdmitx_audio_event and then
		 * nofity framework, tx driver will send kobject hdmitx_audio_event 0 when suspend;
		 * now AndroidU framework just check hdmitx extcon even:
		 * cat /sys/class/extcon/extcon0/cable.0/state, so tx driver need send
		 * hdmitx_audio_event 0 by hdmitx extcon
		 */
		if (type == TX_AUDIO_EVENT && uevent_mgr->tx_extcon &&
			!uevent_mgr->soundbar_en_flag) {
			extcon_set_state_sync(uevent_mgr->tx_extcon,
				extcon_id, state);
			extcon_event = true;
		}
		/* add new event type to control send soundbar event.
		 * When the soundbar is turned on, it is not necessary to send an
		 * extcon event to Android.
		 */
		if (type == TX_SOUNDBAR_EVENT && uevent_mgr->tx_extcon) {
			extcon_set_state_sync(uevent_mgr->tx_extcon,
				extcon_id, state);
			extcon_event = true;
		}
	}

	TX_DEBUG(uevent_mgr->log, "%s %s %d %d\n", __func__, env, ret, extcon_event);
	return ret;
}

/* below interfaces are for hpd/phy_addr/edid notify to cec/hdmirx */
int meson_tx_event_mgr_notifier_register(struct meson_tx_event_mgr *event_mgr,
	struct tx_notifier_client *nb)
{
	if (!event_mgr || !nb)
		return -EINVAL;

	return blocking_notifier_chain_register(&event_mgr->tx_event_notify_list,
		(struct notifier_block *)nb);
}

int meson_tx_event_mgr_notifier_unregister(struct meson_tx_event_mgr *event_mgr,
	struct tx_notifier_client *nb)
{
	if (!event_mgr || !nb)
		return -EINVAL;

	return blocking_notifier_chain_unregister(&event_mgr->tx_event_notify_list,
		(struct notifier_block *)nb);
}

int meson_tx_event_mgr_notify(struct meson_tx_event_mgr *event_mgr,
	unsigned long state, void *arg)
{
	if (!event_mgr)
		return -EINVAL;

	return blocking_notifier_call_chain(&event_mgr->tx_event_notify_list, state, arg);
}

