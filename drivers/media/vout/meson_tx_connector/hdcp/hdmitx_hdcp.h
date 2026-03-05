/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMI_HDCP_H__
#define __HDMI_HDCP_H__
#include "hdmitx/tx21/hdmitx_hw_core.h"

#define DDC_HDCP_DEVICE_ADDR 0x74
#define REG_DDC_HDCP_VERSION 0x50

enum hdcp_ver_t {
	HDCP_VER_NONE,
	HDCP_VER_HDCP1X,
	HDCP_VER_HDCP2X,
};

#define HDCP1X_MAX_DEPTH 7
/* for repeater enable case, as maximum ksv fifo 16*5 bytes */
#define HDCP1X_MAX_TX_DEV_RPT 16 //127
/* for source case(hdcp repeater disabled) */
#define HDCP1X_MAX_TX_DEV_SRC 127
#define HDCP1X_MAX_RPTR_DEV 32

#define HDCP2X_MAX_DEV 31
#define HDCP2X_MAX_DEPTH 4

#define KSV_SIZE 5

struct hdcp_ksv_t {
	u8 b[KSV_SIZE]; /* 40-bit ksv */
};

struct hdcp_topo_t {
	/* hdcp1x compliant device in the topology if true, for hdcp2x */
	bool ds_hdcp1x_dev;
	/* hdcp 2.0 compliant repeater in the topology if true, for hdcp2x */
	bool ds_hdcp2x_dev;
	/* more than 7 level for hdcp1x or 4 level for hdcp2x cascaded together if true */
	bool max_cas_exceed;
	/* more than 127 devices(for hdcp1x) or 31 devices(for hdcp2x) as attached if true */
	bool max_devs_exceed;
	/* total number of attached downstream devices */
	u8 dev_count;
	/* repeater cascade depth */
	u8 rp_depth;
	/* seq_num_v value, for hdcp2x */
	u32 seq_num_v;
};

enum hdcp_content_type_t {
	HDCP_CONTENT_TYPE_0,
	HDCP_CONTENT_TYPE_1,
	HDCP_CONTENT_TYPE_NONE = 0xff,
};

struct hdcp_csm_t {
	u32 seq_num_m; /* seq_num_m value, hdcp2x use only */
	u16 streamid_type; /* streamid_type */
};

enum hdcp_fail_types_t {
	HDCP_FAIL_NONE,
	HDCP_FAIL_DDC_NACK,
	HDCP_FAIL_BKSV_RXID,
	HDCP_FAIL_AUTH_FAIL,
	HDCP_FAIL_READY_TO,
	HDCP_FAIL_V,
	HDCP_FAIL_TOPOLOGY,
	HDCP_FAIL_RI,
	HDCP_FAIL_REAUTH_REQ,
	HDCP_FAIL_CONTENT_TYPE,
	HDCP_FAIL_AUTH_TIME_OUT,
	HDCP_FAIL_HASH,
	HDCP_FAIL_UNKNOWN,
};

enum hdcp_stat_t {
	HDCP_STAT_NONE,
	HDCP_STAT_AUTH,
	HDCP_STAT_CSM,
	HDCP_STAT_SUCCESS,
	HDCP_STAT_FAILED,
};

enum hdcp22_topo_cmd_t {
	NONE_HDCP22_TOPO,
	GET_HDCP22_TOPO,
	SET_HDCP22_TOPO,
};

/* 3. HDCP struct */
struct hdcp_work {
	u32 delay_ms;
	u32 period_ms;
	const char *name;
	struct delayed_work dwork;
};

/* hw21 core private hdcp struct */
struct hdcptx21_core {
	/* device which communicate with hdcp_tx22 daemon and tee_hdcp
	 * note: should not move its position in struct
	 */
	struct miscdevice hdcp_misc_dev;

	bool hdcptx_enabled;
	bool ds_auth;
	bool encryption_enabled;
	bool ds_repeater;
	bool rpt_ready;
	bool update_topology;
	bool update_topo_state;
	bool reauth_ignored;
	bool csm_msg_sent;
	bool csm_valid;
	enum hdcp_fail_types_t fail_type;
	/* only used for limit print when failure */
	enum hdcp_fail_types_t fail_reason;
	enum hdcp_ver_t req_hdcp_ver;
	enum hdcp_content_type_t content_type;
	enum hdcp_stat_t hdcp_state;
	enum hdcp_ver_t hdcp_type;
	enum hdcp_ver_t hdcp_cap_ds;
	enum hdcptx_start_type_t hdcptx_start_type;
	u8 *p_ksv_lists;
	u8 *p_ksv_next;
	struct hdcp_topo_t hdcp_topology;
	struct hdcp_csm_t csm_message;
	struct hdcp_csm_t prev_csm_message;
	struct workqueue_struct *hdcp_wq;
	struct hdcp_work timer_hdcp_rcv_auth;
	struct hdcp_work timer_hdcp_rpt_auth;
	struct hdcp_work timer_hdcp_auth_fail_retry;
	struct hdcp_work timer_bksv_poll_done;
	struct hdcp_work timer_hdcp_start;
	struct hdcp_work timer_ddc_check_nak;
	struct hdcp_work timer_update_csm;

	/* for ksv/receiver_id list forward */
	struct delayed_work ksv_notify_wk;
	/* for hdcp reauth from upstream */
	struct delayed_work req_reauth_wk;
	/* for mute operation under stream type1 */
	struct delayed_work stream_mute_wk;
	/* for stream type propagate */
	struct delayed_work stream_type_wk;
	/* work for hdcp start */
	struct delayed_work work_start_hdcp;
	/* for upstream side request auth timeout */
	unsigned long up_hdcp_timeout_sec;
	struct delayed_work work_up_hdcp_timeout;

	/* hdcp stop & start should be mutexed */
	struct mutex hdcptx_auth_mutex;
	/*
	 * case1: upstream side update stream type
	 * case2: hdmitx itself update stream type
	 * when hdmirx-hdmitx passthrough enabled,
	 * the sequence of two cases is not certain,
	 * need to use the type of case1 at last.
	 * need to mutex csm_message.streamid_type
	 * and also make stream type management
	 * message sent only once
	 * it may involve hdcptx_auth_mutex inside!
	 */
	struct mutex stream_type_mutex;

	/* audio/video mute if upstream type = 1 */
	bool stream_mute;
	/* hdmirx side request flag, see hdmi_rx_repeater.h
	 * STREAMTYPE_UPDATE 0x10
	 * UPSTREAM_INACTIVE 0x20
	 * UPSTREAM_ACTIVE 0x40
	 * 0: notify reauth
	 */
	u8 rx_update_flag;
	/* the stream type notified by upstream side
	 * bit4: if 1, upstream may send stream type before
	 * hdmitx start hdcp(passthrough enabled), need
	 * to save it, and cover the stream type with bit3:0
	 * when hdmitx hdcp propagate stream type. else
	 * hdmitx is the active source and should decide
	 * the stream type itself
	 */
	u8 saved_upstream_type;
	/* 0: auto hdcp version, 1: hdcp1.4, 2: hdcp2.3 */
	u8 req_reauth_ver;
	u8 cont_smng_method;
	/* flag: csm already updated by single csm message */
	bool csm_updated;
	bool hdcp14_second_part_pass;
	struct hdcptx_common *bind_hdcptx_comm;

	u8 def_stream_type;

	/* switch mode by passthrough hdmirx input format to downstream hdmitx output
	 * if yes, system_control will write this node to 1.
	 * only for special hdmitx21 project.
	 */
	u32 is_passthrough_switch;
	int frl_rate;
};

u8 hdmitx_reauth_request(u8 hdcp_version);
bool hdcptx_need_ctrl_by_upstream(bool hdcp_rpt_en);
void hdcp2x_process_intr(void *hdcptx_instance, u8 int_reg[]);
void hdcp1x_process_intr(void *hdcptx_instance, u8 intr_reg);
void *hdmitx21_hdcp_init(void *hdcptx_comm);

#endif /* __HDMI_HDCP_H__ */
