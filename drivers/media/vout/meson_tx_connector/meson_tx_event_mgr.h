/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_EVENT_MGR_H
#define __MESON_TX_EVENT_MGR_H

#include <linux/device.h>
#include <linux/notifier.h>
#include <linux/extcon.h>
#include <linux/extcon-provider.h>

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_dev.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_log.h>

struct meson_tx_tracer;

#define MAX_UEVENT_LEN 64

#define TX_ERROR_MASK BIT(16)
enum tx_trace_log_bits {
	TX_HPD_PLUGOUT	= 1,
	TX_HPD_PLUGIN,
	/* EDID states */
	TX_EDID_HDMI_DEVICE,
	TX_EDID_DVI_DEVICE,
	TX_EDID_HDR_SUPPORT,
	TX_EDID_DV_SUPPORT,
	/* HDCP states */
	TX_HDCP_AUTH_SUCCESS,
	TX_HDCP_AUTH_FAILURE,
	TX_HDCP_HDCP_1_ENABLED,
	TX_HDCP_HDCP_2_ENABLED,
	TX_HDCP_NOT_ENABLED,
	/* Output states */
	TX_KMS_DISABLE_OUTPUT,
	TX_KMS_ENABLE_OUTPUT,
	/* AUDIO format setting */
	TX_AUDIO_MODE_SETTING,
	TX_AUDIO_MUTE,
	TX_AUDIO_UNMUTE,
	/*HDR format setting*/
	TX_HDR_MODE_SDR,
	TX_HDR_MODE_SMPTE2084,
	TX_HDR_MODE_HLG,
	TX_HDR_MODE_HDR10PLUS,
	TX_HDR_MODE_CUVA,
	TX_HDR_MODE_DV_STD,
	TX_HDR_MODE_DV_LL,
	/* HDCP */
	TX_HDCP_AUTH_NO_14_KEYS_ERROR,
	TX_HDCP_AUTH_NO_22_KEYS_ERROR,

	/* EDID errors */
	TX_EDID_HEAD_ERROR                  = TX_ERROR_MASK | 1,
	TX_EDID_CHECKSUM_ERROR              = TX_ERROR_MASK | 2,
	TX_EDID_I2C_ERROR                   = TX_ERROR_MASK | 3,
	/* HDCP errors */
	TX_HDCP_DEVICE_NOT_READY_ERROR      = TX_ERROR_MASK | 4,
	TX_HDCP_AUTH_READ_BKSV_ERROR        = TX_ERROR_MASK | 5,
	TX_HDCP_AUTH_VI_MISMATCH_ERROR      = TX_ERROR_MASK | 6,
	TX_HDCP_AUTH_TOPOLOGY_ERROR         = TX_ERROR_MASK | 7,
	TX_HDCP_AUTH_R0_MISMATCH_ERROR      = TX_ERROR_MASK | 8,
	TX_HDCP_AUTH_REPEATER_DELAY_ERROR   = TX_ERROR_MASK | 9,
	TX_HDCP_I2C_ERROR                   = TX_ERROR_MASK | 10,
	/* Mode setting errors */
	TX_KMS_ERROR                        = TX_ERROR_MASK | 11,
	TX_KMS_SKIP                         = TX_ERROR_MASK | 12,
};

enum meson_tx_event {
	TX_NONE_EVENT = 0,
	TX_HPD_EVENT,
	TX_HDCP_EVENT,
	TX_CUR_ST_EVENT,
	TX_AUDIO_EVENT,
	TX_HDCPPWR_EVENT,
	TX_HDR_EVENT,
	TX_RXSENSE_EVENT,
	TX_CEDST_EVENT,
	TX_SOUNDBAR_EVENT,
	TX_VMODE_AUDIO_EVENT,
	TX_TRACER_EVENT,
	TX_LINK_UNSTABLE,
	TX_INCOMPATIBLE_CABLE,
};

struct meson_tx_event_mgr {
	/*for uevent*/
	struct kobject *kobj;
	/* can't send uevent after enter deep suspend */
	bool deep_suspend_flag;
	/*for extcon event*/
	struct extcon_dev *tx_extcon;
	struct device *attached_extcon_dev;
	/*notifier for driver*/
	struct blocking_notifier_head tx_event_notify_list;
	/* if en, not send hpd extcon even to android*/
	bool soundbar_en_flag;
	struct meson_tx_log *log;
	int conn_type;
	/* for send uevent */
	const char *dev_name;
};

struct tx_notifier_client {
	struct notifier_block base;
};

void meson_tx_tracer_destroy(struct meson_tx_tracer *tracer);
struct meson_tx_tracer *meson_tx_tracer_create(struct meson_tx_event_mgr *event_mgr);
int meson_tx_tracer_read_event(struct meson_tx_tracer *tracer,
	char *buf, int read_len);
void meson_tx_tracer_write_event(struct meson_tx_tracer *tracer,
	enum tx_trace_log_bits event);
int meson_tx_tracer_clean_hdr10plus_event(struct meson_tx_tracer *tracer,
	enum tx_trace_log_bits event);
struct meson_tx_event_mgr *meson_tx_event_mgr_create(struct meson_tx_dev *tx_dev);
void meson_tx_event_mgr_suspend(struct meson_tx_event_mgr *event_mgr, bool suspend_flag);
int meson_tx_event_mgr_destroy(struct meson_tx_event_mgr *event_mgr);
int meson_tx_event_mgr_set_uevent_state(struct meson_tx_event_mgr *event_mgr,
	enum meson_tx_event type, int state);
int meson_tx_event_mgr_send_uevent(struct meson_tx_event_mgr *uevent_mgr,
	enum meson_tx_event type, int state, bool force);
void meson_tx_event_mgr_set_uevent_dev(struct meson_tx_event_mgr *event_mgr,
	struct device *dev, const char *uevent_prefix);

int meson_tx_event_mgr_notifier_register(struct meson_tx_event_mgr *event_mgr,
	struct tx_notifier_client *nb);
int meson_tx_event_mgr_notifier_unregister(struct meson_tx_event_mgr *event_mgr,
	struct tx_notifier_client *nb);
int meson_tx_event_mgr_notify(struct meson_tx_event_mgr *event_mgr,
	unsigned long state, void *arg);

#endif
