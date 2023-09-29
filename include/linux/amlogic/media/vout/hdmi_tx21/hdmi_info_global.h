/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_INFO_GLOBAL_H
#define _HDMI_INFO_GLOBAL_H

#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_types.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include "../hdmi_tx_ext.h"

/* -------------------HDMI AUDIO-------------------------------- */
#define TYPE_AUDIO_INFOFRAMES 0x84
#define AUDIO_INFOFRAMES_VERSION 0x01
#define AUDIO_INFOFRAMES_LENGTH 0x0A

#define HDMI_E_NONE 0x0
/* HPD Event & Status */
#define E_HPD_PULG_IN 0x1
#define E_HPD_PLUG_OUT 0x2
#define S_HPD_PLUG_IN 0x1
#define S_HPD_PLUG_OUT 0x0

#define E_HDCP_CHK_BKSV 0x1

/* -------------------HDMI AUDIO END---------------------- */

/* -------------------HDCP-------------------------------- */
/* HDCP keys from Efuse are encrypted by default, in this test HDCP keys
 * are written by CPU with encryption manually added
 */
#define ENCRYPT_KEY 0xbe

enum hdcp_authstate {
	HDCP_NO_AUTH = 0,
	HDCP_NO_DEVICE_WITH_SLAVE_ADDR,
	HDCP_BCAP_ERROR,
	HDCP_BKSV_ERROR,
	HDCP_R0S_ARE_MISMATCH,
	HDCP_RIS_ARE_MISMATCH,
	HDCP_REAUTHENTATION_REQ,
	HDCP_REQ_AUTHENTICATION,
	HDCP_NO_ACK_FROM_DEV,
	HDCP_NO_RSEN,
	HDCP_AUTHENTICATED,
	HDCP_REPEATER_AUTH_REQ,
	HDCP_REQ_SHA_CALC,
	HDCP_REQ_SHA_HW_CALC,
	HDCP_FAILED_VIERROR,
	HDCP_MAX
};

/* -----------------------HDCP END---------------------------------------- */

/* -----------------------HDMI TX---------------------------------- */
enum hdmitx_disptype {
	CABLE_UNPLUG = 0,
	CABLE_PLUGIN_CHECK_EDID_I2C_ERROR,
	CABLE_PLUGIN_CHECK_EDID_HEAD_ERROR,
	CABLE_PLUGIN_CHECK_EDID_CHECKSUM_ERROR,
	CABLE_PLUGIN_DVI_OUT,
	CABLE_PLUGIN_HDMI_OUT,
	CABLE_MAX
};

struct hdmitx_supstatus {
	u32 hpd_state:1;
	u32 support_480i:1;
	u32 support_576i:1;
	u32 support_480p:1;
	u32 support_576p:1;
	u32 support_720p_60hz:1;
	u32 support_720p_50hz:1;
	u32 support_1080i_60hz:1;
	u32 support_1080i_50hz:1;
	u32 support_1080p_60hz:1;
	u32 support_1080p_50hz:1;
	u32 support_1080p_24hz:1;
	u32 support_1080p_25hz:1;
	u32 support_1080p_30hz:1;
};

struct hdmitx_suplpcminfo {
	u32 support_flag:1;
	u32 max_channel_num:3;
	u32 _192k:1;
	u32 _176k:1;
	u32 _96k:1;
	u32 _88k:1;
	u32 _48k:1;
	u32 _44k:1;
	u32 _32k:1;
	u32 _24bit:1;
	u32 _20bit:1;
	u32 _16bit:1;
};

struct hdmitx_supcompressedinfo {
	u32 support_flag:1;
	u32 max_channel_num:3;
	u32 _192k:1;
	u32 _176k:1;
	u32 _96k:1;
	u32 _88k:1;
	u32 _48k:1;
	u32 _44k:1;
	u32 _32k:1;
	u32 _max_bit:10;
};

struct hdmitx_supspeakerformat {
	u32 rlc_rrc:1;
	u32 flc_frc:1;
	u32 rc:1;
	u32 rl_rr:1;
	u32 fc:1;
	u32 lfe:1;
	u32 fl_fr:1;
};

struct hdmitx_audpara {
	enum hdmi_audio_type type;
	enum hdmi_audio_chnnum channel_num;
	enum hdmi_audio_fs sample_rate;
	enum hdmi_audio_sampsize sample_size;
	enum hdmi_audio_source_if aud_src_if;
	unsigned char status[24]; /* AES/IEC958 channel status bits */
};

struct hdmitx_supaudinfo {
	struct hdmitx_suplpcminfo	_60958_PCM;
	struct hdmitx_supcompressedinfo	_AC3;
	struct hdmitx_supcompressedinfo	_MPEG1;
	struct hdmitx_supcompressedinfo	_MP3;
	struct hdmitx_supcompressedinfo	_MPEG2;
	struct hdmitx_supcompressedinfo	_AAC;
	struct hdmitx_supcompressedinfo	_DTS;
	struct hdmitx_supcompressedinfo	_ATRAC;
	struct hdmitx_supcompressedinfo	_One_Bit_Audio;
	struct hdmitx_supcompressedinfo	_Dolby;
	struct hdmitx_supcompressedinfo	_DTS_HD;
	struct hdmitx_supcompressedinfo	_MAT;
	struct hdmitx_supcompressedinfo	_DST;
	struct hdmitx_supcompressedinfo	_WMA;
	struct hdmitx_supspeakerformat		speaker_allocation;
};

struct hdmitx_audinfo {
	/* !< Signal decoding type -- TvAudioType */
	enum hdmi_audio_type type;
	enum hdmi_audio_format format;
	/* !< active audio channels bit mask. */
	enum hdmi_audio_chnnum channels;
	enum hdmi_audio_fs fs; /* !< Signal sample rate in Hz */
	enum hdmi_audio_sampsize ss;
};

#define Y420CMDB_MAX	32
struct hdmitx_info {
	struct hdmi_rx_audioinfo audio_info;
	struct hdmitx_supaudinfo tv_audio_info;
	/* Hdmi_tx_video_info_t            video_info; */
	enum hdcp_authstate auth_state;
	enum hdmitx_disptype output_state;
	/* -----------------Source Physical Address--------------- */
	struct vsdb_phyaddr vsdb_phy_addr;
	/* ------------------------------------------------------- */
	unsigned video_out_changing_flag:1;
	unsigned support_underscan_flag:1;
	unsigned support_ycbcr444_flag:1;
	unsigned support_ycbcr422_flag:1;
	unsigned tx_video_input_stable_flag:1;
	unsigned need_sup_cec:1;

	/* ------------------------------------------------------- */
	unsigned audio_out_changing_flag:1;
	unsigned audio_flag:1;
	unsigned support_basic_audio_flag:1;
	unsigned audio_fifo_overflow:1;
	unsigned audio_fifo_underflow:1;
	unsigned audio_cts_status_err_flag:1;
	unsigned support_ai_flag:1;
	unsigned hdmi_sup_480i:1;

	/* ------------------------------------------------------- */
	unsigned hdmi_sup_576i:1;
	unsigned hdmi_sup_480p:1;
	unsigned hdmi_sup_576p:1;
	unsigned hdmi_sup_720p_60hz:1;
	unsigned hdmi_sup_720p_50hz:1;
	unsigned hdmi_sup_1080i_60hz:1;
	unsigned hdmi_sup_1080i_50hz:1;
	unsigned hdmi_sup_1080p_60hz:1;

	/* ------------------------------------------------------- */
	unsigned hdmi_sup_1080p_50hz:1;
	unsigned hdmi_sup_1080p_24hz:1;
	unsigned hdmi_sup_1080p_25hz:1;
	unsigned hdmi_sup_1080p_30hz:1;

	/* ------------------------------------------------------- */
	/* for total = 32*8 = 256 VICs */
	/* for Y420CMDB bitmap */
	u8 bitmap_valid;
	u8 bitmap_length;
	u8 y420_all_vic;
	u8 y420cmdb_bitmap[Y420CMDB_MAX];
	/* ------------------------------------------------------- */
};

#endif  /* _HDMI_RX_GLOBAL_H */
