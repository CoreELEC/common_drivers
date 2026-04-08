/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_UEVENT_MGR_H
#define __HDMITX_UEVENT_MGR_H

#include <linux/notifier.h>

enum hdmi_event_t {
	HDMI_TX_NONE = 0,
	HDMI_TX_HPD_PLUGIN = 1,
	HDMI_TX_HPD_PLUGOUT = 2,
	HDMI_TX_INTERNAL_INTR = 4,
};

struct hdmitx_event_mgr {
	/*for uevent*/
	struct kobject *kobj;
	/* can't send uevent after enter deep suspend */
	bool deep_suspend_flag;
	/*for extcon event*/
	struct extcon_dev *hdmitx_extcon_hdmi;
	struct device *attached_extcon_dev;
	/*notifier for driver*/
	struct blocking_notifier_head hdmitx_event_notify_list;
	/* if en, not send hpd extcon even to android*/
	bool soundbar_en_flag;
};

enum hdmitx_event {
	HDMITX_NONE_EVENT = 0,
	HDMITX_HPD_EVENT,
	HDMITX_HDCP_EVENT,
	HDMITX_CUR_ST_EVENT,
	HDMITX_AUDIO_EVENT,
	HDMITX_HDCPPWR_EVENT,
	HDMITX_HDR_EVENT,
	HDMITX_RXSENSE_EVENT,
	HDMITX_CEDST_EVENT,
	HDMITX_SOUNDBAR_EVENT,
	HDMITX_VMODE_AUDIO_EVENT,
	HDMITX_TRACER_EVENT,
	HDMITX_LINK_UNSTABLE,
	HDMITX_INCOMPATIBLE_CABLE,
};

/*HDMITX_HDCPPWR_EVENT event value*/
#define HDMI_SUSPEND    0
#define HDMI_WAKEUP     1

#define MAX_UEVENT_LEN 64

/* struct and create function depends on platform,
 * check in platform header: for example, hdmitx_platform_linux.h
 */
struct hdmitx_event_mgr;
struct hdmitx_notifier_client;

/* send uevent:
 * force = 0: only send uevent when val changed;
 * force = 1: force send uevent even val didnot change;
 */
int hdmitx_event_mgr_send_uevent(struct hdmitx_event_mgr *event_mgr,
	enum hdmitx_event type, int val, bool force);
int hdmitx_event_mgr_set_uevent_state(struct hdmitx_event_mgr *event_mgr,
	enum hdmitx_event type, int state);
void hdmitx_event_mgr_suspend(struct hdmitx_event_mgr *event_mgr, bool suspend_flag);

int hdmitx_event_mgr_notifier_register(struct hdmitx_event_mgr *event_mgr,
	struct hdmitx_notifier_client *nb);
int hdmitx_event_mgr_notifier_unregister(struct hdmitx_event_mgr *event_mgr,
	struct hdmitx_notifier_client *nb);
int hdmitx_event_mgr_notify(struct hdmitx_event_mgr *event_mgr,
	unsigned long state, void *arg);

int hdmitx_event_mgr_destroy(struct hdmitx_event_mgr *event_mgr);

#endif
