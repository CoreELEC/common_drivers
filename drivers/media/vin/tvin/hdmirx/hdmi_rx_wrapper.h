/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef HDMI_RX_WRAPPER_H_
#define HDMI_RX_WRAPPER_H_

/* stable_check_lvl */
#define HACTIVE_EN		0x01
#define VACTIVE_EN		0x02
#define HTOTAL_EN		0x04
#define VTOTAL_EN		0x08
#define COLSPACE_EN		0x10
#define REFRESH_RATE_EN	0x20
#define REPEAT_EN		0x40
#define DVI_EN			0x80
#define INTERLACED_EN		0x100
#define HDCP_ENC_EN		0x200
#define COLOR_DEP_EN		0x400
#define ERR_CNT_EN		0x800
#define TMDS_VALID_EN		0x1000
#define ECC_ERR_CNT_EN      0x2000
#define DSC_HTOTAL_CHECK_EN      0x4000
#define HDMIRX_PORT_MAX		4
#define HTOTAL_THRESHOLD    5
/* aud sample rate stable range */
/* #define AUD_SR_RANGE 2000 */
#define PHY_REQUEST_CLK_MIN	170000000
#define PHY_REQUEST_CLK_MAX	370000000
#define TIMER_STATE_CHECK	(1 * HZ / 100)

#define HPD_WAIT_NORMAL 74
#define HPD_WAIT_21 110
#define HPD_WAIT_512 110
#define FRL_UNREADY_MAX 200
/* 2024.07.02 optimize suspend flow */
/* 2024.07.19 optimize cts flow */
/* 2024.08.22 Fix the issue of flashing green screen at the end of SDR playback */
/* 2024.08.30 fix hpd time too short */
/* 2024.09.04 optimize drm pkt handle flow in irq */
/* 2024.09.26 fix rx can not unmute on t3x */
/* 2024.10.08 use irq to set fpll cfg */
/* 2024.10.10 fix fem exit when open irq */
/* 2024.10.11 fix dsc timing judge */
/* 2024.10.12 reduce fsm print */
/* 2024.11.06 Adjust the HPD pull down time based on EDID type */
/* 2024.11.27 fix fsm unnormal */
/* 2024.12.03 adjust audio fifo enable timing */
/* 2024.12.16 remove fsm restart on t3x */
/* 2024.12.19 add protection when not in hdmi source */
/* 2024.12.24 rx statemachine hold when resume on other src*/
/* 2025.01.08 resume flag can not effect switch port*/
/* 2025.1.15 fix frl timing no signal */
/* 2025.2.11 add avi fsm change */
/* 2025.5.14 No delay added after checking hdcp */
/* 2025.5.16 fmt detect add 420 judge */
/* 2025.05.22 do not override 8bit when data was not 422 8bit */
/* 2025.06.27 Fix YUV420 8k signal recognition error issue */
/* 2025.07.14 update 1440p 420 in freq_ref*/
/* 2025.08.19 Configure video and audio after the signal is ready */
/* 2025.08.26 add 1080i 420 fmt */
/* 2025.09.09 remove fsm cd judge */
/* 2025.10.11 reduce signal time when avi range change */
/* 2025.10.15 fix green screen issue for case78 */
/* 2025.11.4 add edid switch protect */
/* 2025.11.11 enable EMP until signal stable */
/* 2025.11.4 add edid switch protect*/
/* 2025.11.14 optimize t6x signal time */
/* 2025.11.19 add hpd protect */
/* 2025.11.20 fix issue that hdcp reauth too slow */
/* 2025.12.02 optimise log & fastswitch */
/* 2025.12.09 fix EDID update failure issue */
/* 2025.12.17 modify frl signal flow */
/* 2025.12.24 update phy 12G settings and init flow */
/* 2025.12.26 fix training err*/
/* 2025.12.30 fix 420 no signal*/
/* 2026.1.5 Add timeout protection measures for HDMI 2.1 port*/
/* 2026.1.6 HDMI cannot recognize 420 */
/* 2026.1.8 add ch lock protect for frl training */
/* 2026.1.9 reduce frl signal detect time */
/* 2026.1.16 fix issue interrupts not enabled when HDMI Ready */
/* 2026.1.21 add lock detect before do pcs reset */
/* 2026.1.23 return unsupport when vic is unknown */
/* 2026.1.29 fix frame rate error caused by unstable signal */
/* 2026.1.30 modify frl signal flow */
/* 2026.2.02 remove htotal detect */
/* 2026.3.02 do pcs reset when fdet abnormal */
/* 2026.03.09 add additional extension colorimetry parsing */
/* 2026.03.13 fix eacr no sound */
/* 2026.03.16 add dsc check for mac */
/* 2026.03.23 add scdc frl rate monitor */
/* 2026.04.07 fix pip port color unnormal */
#define RX_WRAPPER_VER "ver.2026/04/07"

struct freq_ref_s {
	bool interlace;
	u8 cd420;
	u8 type_3d;
	unsigned int hactive;
	unsigned int vactive;
	enum hdmi_vic_e vic;
};

enum fsm_states_e {
	FSM_5V_LOST,
	FSM_INIT,
	FSM_HPD_LOW,
	FSM_HPD_HIGH,
	FSM_FRL_FLT_READY,
	FSM_FRL_TRN,
	FLT_RX_LTS_3,
	FLT_RX_LTS_P,
	FSM_FRL_CH_LOCK,
	FSM_FRL_VALID_M,
	FSM_FRL_FPLL_CFG,
	FSM_WAIT_CLK_STABLE,
	FSM_EQ_START,
	FSM_WAIT_EQ_DONE,
	FSM_PCS_RESET,
	FSM_SIG_UNSTABLE,
	FSM_SIG_WAIT_STABLE,
	FSM_SIG_HOLD,
	FSM_SIG_STABLE,
	FSM_SIG_STABLE_TO_READY,
	FSM_SIG_FRL_CHECK,
	FSM_SIG_FRL_DOUBLE_CHECK,
	FSM_SIG_FDET_CHECK,
	FSM_SIG_READY,
	FSM_NULL,
};

enum err_code_e {
	ERR_NONE,
	ERR_5V_LOST,
	ERR_CLK_UNSTABLE,
	ERR_PHY_UNLOCK,
	ERR_DE_UNSTABLE,
	ERR_NO_HDCP14_KEY,
	ERR_TIMECHANGE,
	ERR_UNKNOWN
};

enum irq_flag_e {
	IRQ_AUD_FLAG = 0x01,
	IRQ_PACKET_FLAG = 0x02,
	IRQ_PACKET_ERR = 0x04,
	IRQ_AVI_CHG_FLAG = 0x8,
};

enum hdcp22_auth_state_e {
	HDCP22_AUTH_STATE_NOT_CAPABLE,
	HDCP22_AUTH_STATE_CAPABLE,
	HDCP22_AUTH_STATE_LOST,
	HDCP22_AUTH_STATE_SUCCESS,
	HDCP22_AUTH_STATE_FAILED,
};

enum esm_recovery_mode_e {
	ESM_REC_MODE_RESET = 1,
	ESM_REC_MODE_TMDS,
};

enum err_recovery_mode_e {
	ERR_REC_EQ_RETRY,
	ERR_REC_HPD_RST,
	ERR_REC_END
};

enum aud_clk_err_e {
	E_AUDPLL_OK,
	E_REQUESTCLK_ERR,
	E_PLLRATE_CHG,
	E_AUDCLK_ERR,
};

enum dumpinfo_e {
	RX_DUMP_VIDEO = 0,
	RX_DUMP_ALL = 1,
	RX_DUMP_AUDIO = 0x02,
	RX_DUMP_HDCP = 0x04,
	RX_DUMP_PHY = 0x08,
	RX_DUMP_CLK = 0x10,
	RX_DUMP_VIDEO_SIMPLE = 0x14
};

enum fps_e {
	E_0HZ,
	E_24HZ,
	E_25HZ,
	E_30HZ,
	E_50HZ,
	E_60HZ,
	E_72HZ,
	E_75HZ,
	E_100HZ,
	E_120HZ
};

enum fmt_vic_status {
	FMT_VIC_OK = 0,
	FMT_VIC_FRL_CTS_DEVICE,
	FMT_VIC_FRL_FDET_ERROR,
	FMT_VIC_TMDS_FDET_ERROR,
	FMT_VIC_HW_SW_MISMATCH,
	FMT_VIC_VESA_REPEAT_ERR,
};

typedef void (*earc_hdmirx_callback)(int earc_port, int st);
/* signal */
extern u32 force_vic;
extern u32 vpp_mute_enable;
extern u32 dbg_cs;
extern int rx_emp_dbg_en;
extern int fsm_debug;
extern int rs_err_chk;
extern int err_cnt;
extern int edid_seg_flag[4];
extern int dump_aud_max;
extern int htotal_threshold;
extern bool vtem_interval_debug;
extern earc_hdmirx_callback earc_hdmirx_hpdst;
enum tvin_sig_fmt_e hdmirx_hw_get_fmt(u8 port);
void rx_mute_vpp(u8 port_type);
void hdmirx_mute_vpp(bool en, u8 port);
void hdmirx_mute_vpp_pkt(bool en, u8 port);
void rx_main_state_machine(void);
void dump_audio_status(u8 port);
void rx_nosig_monitor(u8 port);
bool rx_is_nosig(u8 port);
bool rx_is_sig_ready(u8 port);
void hdmirx_open_main_port_t3x(u8 port);
void hdmirx_open_main_port(u8 port);
void hdmirx_open_sub_port(u8 port);
void hdmirx_close_port_t3x(u8 port);
void hdmirx_close_port(u8 port);
bool is_clk_stable(u8 port);
unsigned int rx_get_pll_lock_sts(void);
unsigned int rx_get_scdc_clkrate_sts(u8 port);
void hdmirx_clr_scdc(bool en, u8 port);
void fsm_restart(u8 port);
void rx_5v_monitor(void);
void rx_audio_pll_sw_update(u8 port);
void rx_acr_info_sw_update(void);
void rx_sw_reset(int level, u8 port);
void rx_aud_pll_ctl(bool en, u8 port);
void hdmirx_timer_handler(struct timer_list *t);
void rx_tmds_resource_allocate(struct device *dev, u8 port);
void rx_emp_resource_allocate(struct device *dev);
void rx_emp_data_capture(u8 port);
void rx_tmds_data_capture(u8 port);
void dump_state(int enable, u8 port);
void hdmirx_init_params(u8 port);
void set_video_mute(u32 owner, bool on);
u8 get_frame_interval_cnt(u8 cnt, u8 port);
void rx_edid_update_handler(struct work_struct *dwork);
void frate_monitor(void);
void frate_monitor1(void);

unsigned int rx_switch_vpu_clk(int over_clock_flag);

void __weak set_video_mute(u32 owner, bool on)
{
}

bool get_video_mute_val(u32 owner);
bool __weak get_video_mute_val(u32 owner)
{
	return false;
}

void dump_phy_status(u8 port);
void rx_monitor_error_counter(u8 port);
void rx_fsm_print_handler(struct work_struct *work);
bool rx_is_need_recovery_after_cor_reset(u8 port);
int rx_get_cur_hpd_sts(u8 port);
void rx_set_ext_state(u8 port, unsigned int ext_state);
#endif
