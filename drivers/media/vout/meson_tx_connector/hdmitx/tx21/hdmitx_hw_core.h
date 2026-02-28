/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_HW_CORE_H__
#define __HDMITX_HW_CORE_H__
#include <linux/hdmi.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include "hdmitx_packet.h"
#include "hdmitx_hw_platform.h"
#include "hdmitx_packet.h"

/* L_0 will always be printed, set log level to L_1/2/3 for detail */
#define L_0 0
#define L_1 1
#define L_2 2
#define L_3 3

#define HDCP_STAGE1_RETRY_TIMER 2000 /* unit: ms */
#define HDCP_BSKV_CHECK_TIMER 100
#define HDCP_FAILED_RETRY_TIMER 200
#define HDCP_DS_KSVLIST_RETRY_TIMER 5000//200
#define HDCP_RCVIDLIST_CHECK_TIMER 3000//200

#define DEFAULT_STREAM_TYPE 0
#define VIDEO_MUTE_PATH_1 0x8000000 //mute by vid_mute sysfs node
#define VIDEO_MUTE_PATH_2 0x4000000 //mute by stream type 1
#define VIDEO_MUTE_PATH_3 0x2000000 //mute by rx request auth

#define AVMUTE_PATH_1 0x80 //mute by avmute sysfs node
#define AVMUTE_PATH_2 0x40 //mute by upstream side request re-auth

#define HDCPTX_IOOPR		0x820000ab

/* hdmitx soft reset */
#define RESET_18OR10TO20		0x01
#define RESET_SOFTWARE_LOGIC	0x02
#define RESET_CIPHER_ENGINE		0x04
#define RESET_HW_TPI			0x08
#define RESET_HDCP2X_LOGIC		0x10
#define RESET_PFIFO				0x20

/* 1. interrupts struct */
struct intr_t;
typedef void(*hdmi_intr_cb)(struct intr_t *, void *);

struct intr_t {
	const u32 intr_mask_reg;
	const u32 intr_st_reg;
	const u32 intr_clr_reg;
	const unsigned int intr_top_bit;
	u32 mask_data;
	u32 st_data;
	hdmi_intr_cb callback;
};

/* intr_array will be used for ISR quickly save status
 * note: the priority is in descending order
 */
struct hdmitx_intr {
	struct intr_t top_intr;
#ifdef CONFIG_AMLOGIC_HDCP_TX_MODERN
	struct intr_t tpi_intr;
	struct intr_t cp2tx_intr0;
	struct intr_t cp2tx_intr1;
	struct intr_t cp2tx_intr2;
	struct intr_t cp2tx_intr3;
#endif
	struct intr_t scdc_intr;
	struct intr_t intr2;
	struct intr_t intr5;
};

/* 2. DDC bus error codes */
enum ddc_err_t {
	DDC_ERR_NONE = 0x00,
	DDC_ERR_TIMEOUT = 0x01,
	DDC_ERR_NACK = 0x02,
	DDC_ERR_BUSY = 0x03,
	DDC_ERR_HW = 0x04,
	DDC_ERR_LIM_EXCEED = 0x05,
};

/* 3. hdcp */
enum hdcptx_oprcmd {
	HDCP_DEFAULT,
	HDCP14_KEY_READY,
	HDCP14_LOADKEY,
	HDCP14_RESULT,
	HDCP22_KEY_READY,
	HDCP22_LOADKEY,
	HDCP22_RESULT,
	HDCP22_SET_TOPO,
	HDCP22_GET_TOPO,
	CONF_ENC_IDX, /* 0: get idx; 1: set idx */
	HDMITX_GET_RTERM, /* get the rterm value */
	CHECK_SPEC_EDID
};

/* 4. VRR struct */
struct mvrr_const_val {
	/* unit: 100  6000, 5994, 5000, 3000, 2997, 2500, 2400, 2397 */
	u16 duration;
	u16 vtotal_fixed; /* vtotal_fixed is mutex with bit_len */
	u8 bit_len; /* current max value is 16 * 8, 128 */
	u8 frac_array[16];
};

struct mvrr_const_st {
	enum hdmi_vic brr_vic; /* the vic of brr */
	const struct mvrr_const_val *val[];
};

/* VRR parameters configuration */
struct vrr_conf_para {
	enum vrr_type type;
	u8 vrr_enabled;
	u8 fva_factor; /* should be larger than 0 */
	u8 brr_vic;
	int duration;
	/* EDID capability, refer to 2.1A P380 */
	u8 fapa_end_extended:1;
	u8 qms_sup:1;
	u8 mdelta_bit:1;
	u8 neg_mvrr_bit:1;
	u8 fva_sup:1;
	u8 fapa_start_loc:1;
	u8 qms_tfrmin:1;
	u8 qms_tfrmax:1;
	u8 vrrmin;
	u16 vrrmax;
};

/* below will be used in the vrr sync handler */
struct tx_vrr_params {
	/* the member conf_params is critical and may change at anytime */
	spinlock_t lock;
	struct vrr_conf_para conf_params;
	/* vrr_para_tmp is used for saving conf_params if conf_params is updated */
	struct vrr_conf_para vrr_para_tmp;
	const struct mvrr_const_val *mconst_val; /* for qms */
	struct mvrr_const_val game_val; /* for game */
	struct emp_packet_st emp_vrr_pkt;
	u32 frame_cnt; /* set to 0 when vrr_params changed */
	u32 mdelta_limit; /* for mdelta = 1 case */
	u8 fapa_early_cnt;
};

/* 5. FRL struct */
enum flt_tx_states {
	FLT_TX_LTS_L,	/* legacy mode */
	FLT_TX_LTS_1,	/* read edid */
	FLT_TX_LTS_2,	/* prepare for frl */
	FLT_TX_LTS_3,	/* training in progress */
	FLT_TX_LTS_4,	/* update frl_rate */
	/* frl training passed, start frl w/o video */
	FLT_TX_LTS_P1,
	/* start frl w/ video */
	FLT_TX_LTS_P2,
	/* maintain frl video */
	FLT_TX_LTS_P3,
};

enum ffe_levels {
	FFE_LEVEL0 = 0,
	FFE_LEVEL1 = 1,
	FFE_LEVEL2 = 2,
	FFE_LEVEL3 = 3,
};

enum ltp_patterns {
	/* No Link Training Pattern requested */
	LTP_PATTERN_NO_LTP = 0,
	/* All 1's pattern */
	LTP_PATTERN_LTP1 = 1,
	/* All 0's pattern */
	LTP_PATTERN_LTP2 = 2,
	/* Nyquist clock pattern */
	LTP_PATTERN_LTP3 = 3,
	/* Source TxFFE Compliance Test Pattern */
	LTP_PATTERN_LTP4 = 4,
	/* LFSR 0 */
	LTP_PATTERN_LTP5 = 5,
	/* LFSR 1 */
	LTP_PATTERN_LTP6 = 6,
	/* LFSR 2 */
	LTP_PATTERN_LTP7 = 7,
	/* LFSR 3 */
	LTP_PATTERN_LTP8 = 8,
	/* reserved 9 ~ 0xd */
	LTP_PATTERN_RSVD = 9,
	/* Sink requests Source to update FFE */
	LTP_PATTERN_UPDATE_FFE = 0xE,
	/* Sink requests Source to change link mode(rate) to a lower value */
	LTP_PATTERN_CHG_LINK_MODE = 0x0F,
};

struct frl_work {
	u32 delay_ms;
	u32 period_ms;
	const char *name;
	struct delayed_work dwork;
};

struct frl_train_t {
	struct workqueue_struct *frl_wq;
	struct frl_work timer_frl_flt;
	u8 src_test_cfg;
	u8 lane_count;
	u8 update_flags;
	enum flt_tx_states flt_tx_state;
	enum flt_tx_states last_state;
	enum frl_rate_enum max_frl_rate;
	enum frl_rate_enum max_edid_frl_rate;
	enum frl_rate_enum user_max_frl_rate;
	enum frl_rate_enum min_frl_rate;
	enum frl_rate_enum frl_rate;
	enum ffe_levels max_ffe_level;
	enum ffe_levels ffe_level[4];
	bool ds_frl_support;
	bool req_legacy_mode;
	bool frl_rate_no_change;
	bool req_frl_mode;
	bool txffe_pre_shoot_only;
	bool txffe_de_emphasis_only;
	bool txffe_no_ffe;
	bool flt_no_timeout;
	bool flt_timeout;
	bool auto_ffe_update;
	bool auto_pattern_update;
	bool flt_running; /* if flt_running is false, return */
};

/* 1. interrupts */
void hdmitx_core_intr_process(struct hdmitx_common *tx_comm);
int hdmitx_setup_irqs(struct hdmitx_hw_common *tx_hw);
void intr_status_init_clear(void);
void hdmitx_fifo_flow_enable_intr(bool en);

/* 2. DDC/SCDC related */
u8 hdmi_ddc_status_check(void);
u8 hdmi_ddc_busy_check(void);
void hdmi_ddc_error_reset(void);
bool ddc_bus_wait_free(void);
bool hdmitx_ddcm_read(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 *p_buf, u16 len, u8 read_cmd);
bool hdmitx_ddcm_write(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 data);

#define LT_TX_CMD_TXFFE_UPDATE 0x02
#define LT_TX_CMD_LTP_UPDATE   0x03
void scdc_bus_stall_set(bool en);
u16 scdc_tx_ltp0123_get(void);
bool scdc_tx_frl_cfg1_set(u8 cfg1);
u8 scdc_tx_update_flags_get(void);
bool scdc_tx_update_flags_set(u8 update_flags);
u8 scdc_tx_flt_ready_status_get(void);
u8 scdc_tx_sink_version_get(void);
void scdc_tx_source_version_set(u8 src_ver);
u8 scdc_tx_source_test_cfg_get(void);
void scdc_tx_frl_get_rx_rate(u8 *data);

/* 3. hdcp related */
/* hdcp platform related */
int hdmitx_hdcp_stat_monitor_task(void *data);
void tee_comm_dev_reg(struct miscdevice *hdcp_misc_dev, const struct file_operations *fops);
void tee_comm_dev_unreg(struct miscdevice *hdcp_misc_dev);
void hdcp_enable_intr(bool en);

/* hdcp mute related */
extern unsigned long avmute_ms;
extern unsigned long vid_mute_ms;
void hdmitx21_av_mute_op(u32 flag, unsigned int path);
void hdmitx21_video_mute_op(struct hdmitx_hw_common *hw_comm, u32 flag, unsigned int path);

/* hdcp debug */
extern unsigned long hdcp_reauth_dbg;
extern unsigned long streamtype_dbg;
extern unsigned long en_fake_rcv_id;

/* 4. VRR parts */
irqreturn_t hdmitx_vrr_vsync_handler(struct hdmitx21_dev *hdev);
void tx_vrr_params_init(void);
void hdmitx_set_vrr_para(const struct vrr_conf_para *para);
void hdmitx_vrr_set_maxlncnt(u32 max_lcnt);
u32 hdmitx_vrr_get_maxlncnt(void);
int hdmitx_set_vrr_rate(struct hdmitx_hw_common *tx_hw, int duration, void *data);
void hdmitx_unregister_vrr(u32 enc_idx);
void hdmitx_register_vrr(struct hdmitx21_dev *hdev);
int hdmitx_dump_vrr_status(struct seq_file *s, void *p);
void hdmitx_vrr_enable(void);
void hdmitx_vrr_disable(void);
void hdmitx_register_drm_vrr_api(struct hdmitx21_dev *hdev);
void hdmitx21_vrr_init(struct hdmitx21_dev *hdev);

/* 5. frl related */
bool flt_tx_cmd_execute(u8 lt_cmd);
void flt_tx_ltp_req_write(u8 ltp01, u8 ltp23);
void flt_tx_update_set(void);
void frl_tx_start_mod(bool start);
bool flt_tx_update_cleared_wait(void);
bool frl_tx_rate_written(void);
u8 flt_tx_cfg1_get(void);
u8 frl_tx_tx_get_rate(void);
void frl_tx_tx_enable(bool enable);
void frl_tx_av_enable(bool enable);
void frl_tx_sb_enable(bool enable, enum frl_rate_enum frl_rate);
bool frl_tx_pattern_init(u16 patterns);
void frl_tx_pattern_stop(void);
void frl_tx_pin_swap_set(bool en);
bool frl_tx_pattern_set(enum ltp_patterns frl_pat, u8 lane);
bool frl_tx_ffe_set(enum ffe_levels ffe_level, u8 lane);
bool frl_tx_tx_phy_init(bool disable_ffe);
void frl_tx_tx_init(void);
void frl_tx_tx_phy_set(void);
void tmds_tx_phy_set(void);
void frl_tx_lts_1_hdmi21_config(void);
void frl_tx_training_handler(struct hdmitx21_dev *hdev);
void frl_tx_stop(void);
bool frl_check_full_bw(enum hdmi_colorspace cs, enum hdmi_color_depth cd, u32 pixel_clock,
	u32 h_active, enum frl_rate_enum frl_rate, u32 *tri_bytes);
/* data flow metering config */
void hdmitx_dfm_cfg(u8 bw_type, u16 h_active);
void hdmitx_set_frl_rate_none(struct hdmitx_common *tx_comm);

/* 6. dsc related */
#ifdef CONFIG_AMLOGIC_DSC
irqreturn_t hdmitx_emp_vsync_handler(struct hdmitx21_dev *hdev);
void hdmitx_dsc_cvtem_pkt_send(struct dsc_pps_data_s *pps,
	struct tx_timing *timing);
void hdmitx_dsc_cvtem_pkt_disable(void);
#endif

/* 7. packets related */
unsigned int hdmitx21_get_vendor_infoframe_ieee(void);
void hdmitx_dump_infoframe_packets(struct seq_file *s);
void hdmi_gcppkt_manual_set(bool en);

/* 8. drm hdcp/vrr */
unsigned int drm_hdmitx_get_rx_hdcp_cap(void);

/* 9. others */
void hdmitx_soft_reset(u32 bits);
void hdmitx21_set_audioclk(bool en);
void hdmitx21_pbist_config(struct hdmitx21_dev *hdev, enum hdmi_vic vic, int pbist_en);

#endif /* __HDMITX_HW_CORE_H__ */

