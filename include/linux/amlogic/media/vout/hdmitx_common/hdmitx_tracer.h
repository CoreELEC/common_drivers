/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_TRACER_H
#define __HDMITX_TRACER_H

/*
 * For Debug:
 * write event to fifo, and userspace can read by sysfs.
 */
#define HDMITX_HDMI_ERROR_MASK BIT(16)
enum hdmitx_event_log_bits {
	HDMITX_HPD_PLUGOUT                      = 1,
	HDMITX_HPD_PLUGIN,
	/* EDID states */
	HDMITX_EDID_HDMI_DEVICE,
	HDMITX_EDID_DVI_DEVICE,
	HDMITX_EDID_HDR_SUPPORT,
	HDMITX_EDID_DV_SUPPORT,
	/* HDCP states */
	HDMITX_HDCP_AUTH_SUCCESS,
	HDMITX_HDCP_AUTH_FAILURE,
	HDMITX_HDCP_HDCP_1_ENABLED,
	HDMITX_HDCP_HDCP_2_ENABLED,
	HDMITX_HDCP_NOT_ENABLED,
	/* Output states */
	HDMITX_KMS_DISABLE_OUTPUT,
	HDMITX_KMS_ENABLE_OUTPUT,
	HDMITX_KMS_SKIP,
	/* AUDIO format setting */
	HDMITX_AUDIO_MODE_SETTING,
	HDMITX_AUDIO_MUTE,
	HDMITX_AUDIO_UNMUTE,
	/*HDR format setting*/
	HDMITX_HDR_MODE_SDR,
	HDMITX_HDR_MODE_SMPTE2084,
	HDMITX_HDR_MODE_HLG,
	HDMITX_HDR_MODE_HDR10PLUS,
	HDMITX_HDR_MODE_CUVA,
	HDMITX_HDR_MODE_DV_STD,
	HDMITX_HDR_MODE_DV_LL,
	/* HDCP */
	HDMITX_HDCP_AUTH_NO_14_KEYS_ERROR,
	HDMITX_HDCP_AUTH_NO_22_KEYS_ERROR,

	/* EDID errors */
	HDMITX_EDID_HEAD_ERROR                  = HDMITX_HDMI_ERROR_MASK | 1,
	HDMITX_EDID_CHECKSUM_ERROR              = HDMITX_HDMI_ERROR_MASK | 2,
	HDMITX_EDID_I2C_ERROR                   = HDMITX_HDMI_ERROR_MASK | 3,
	/* HDCP errors */
	HDMITX_HDCP_DEVICE_NOT_READY_ERROR      = HDMITX_HDMI_ERROR_MASK | 4,
	HDMITX_HDCP_AUTH_READ_BKSV_ERROR        = HDMITX_HDMI_ERROR_MASK | 5,
	HDMITX_HDCP_AUTH_VI_MISMATCH_ERROR      = HDMITX_HDMI_ERROR_MASK | 6,
	HDMITX_HDCP_AUTH_TOPOLOGY_ERROR         = HDMITX_HDMI_ERROR_MASK | 7,
	HDMITX_HDCP_AUTH_R0_MISMATCH_ERROR      = HDMITX_HDMI_ERROR_MASK | 8,
	HDMITX_HDCP_AUTH_REPEATER_DELAY_ERROR   = HDMITX_HDMI_ERROR_MASK | 9,
	HDMITX_HDCP_I2C_ERROR                   = HDMITX_HDMI_ERROR_MASK | 10,
	/* Mode setting erros */
	HDMITX_KMS_ERROR                        = HDMITX_HDMI_ERROR_MASK | 11,
};

struct hdmitx_tracer;

void hdmitx_tracer_destroy(struct hdmitx_tracer *tracer);

/* When hdr10plus mode ends, clear hdr10plus_event flag */
int hdmitx_tracer_clean_hdr10plus_event(struct hdmitx_tracer *tracer,
	enum hdmitx_event_log_bits event);
/*write event to fifo, return 0 if write success, else return errno.*/
void hdmitx_tracer_write_event(struct hdmitx_tracer *tracer,
	enum hdmitx_event_log_bits event);
/*read events log in fifo, return read buffer len.*/
int hdmitx_tracer_read_event(struct hdmitx_tracer *tracer,
	char *buf, int read_len);

#endif
