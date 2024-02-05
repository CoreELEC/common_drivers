/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_COMMON_H
#define __HDMITX_COMMON_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/hdmi.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/jiffies.h>

#include <drm/amlogic/meson_connector_dev.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_format_para.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_hw_common.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_edid.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_types.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_tracer.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_event_mgr.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx.h>
#include <linux/amlogic/media/vout/dsc.h>
#include <linux/amlogic/media/vrr/vrr.h>

#define HDMITX20 20
#define HDMITX21 21
#define DEVICE_NAME "amhdmitx"

#define HDMI_TX_POOL_NUM      6
#define HDMI_TX_RESOURCE_NUM  4
#define HDMI_TX_PWR_CTRL_NUM  6
/* this value must be >= 1 */
#define CSC_DELAY_FRAME       2

struct hdmitx_common_state {
	struct hdmi_format_para para;
	enum vmode_e mode;
	u32 hdr_priority;
	/* save sequence id for drm connecter state */
	u64 state_sequence_id;
};

typedef void (*audio_en_callback)(bool enable);
typedef int (*audio_st_callback)(void);

struct hdcptx_common {
	/* device which communicate with hdcp_tx22 daemon and tee_hdcp
	 * note: should not move its position in struct
	 */
	struct miscdevice hdcp_misc_dev;

	/* current hdcp mode, 2: hdcp2.2, 1: hdcp1.4, 8: hdcp off */
	u8 hdcp_mode;
	u8 test_hdcp_mode;
	/* hdcp tx key status */
	u32 hdcp_lstore;
	/* hdcp repeater enable, such as on T7 platform */
	bool hdcp_rpt_en;

	/* 1: downstream is repeater, 0: downstream is receiver */
	int hdcp_bcaps_repeater;
	/* downstream device hdcp2.2 capability */
	bool dw_hdcp22_cap;
	/* hdcp status check timer */
	struct task_struct *task_hdcp;

	/* policy related */
	/* HWC to enable hdcp flow 1: for SNPS chip, 0: for hdmitx21 chip */
	bool hdcp_user;
	/* hdcp control level, 0: android, 1:drm driver, 2: linux app */
	u32 hdcp_ctl_lvl;
	/* not send hdcp fail uevent during filter period when hdcp off.
	 * only for special hdmitx21 project. if need to enable this feature,
	 * need to set need_filter_hdcp_off = 1 and configure
	 * the filter period by set filter_hdcp_off_period(second)
	 */
	bool need_filter_hdcp_off;
	u32 filter_hdcp_off_period;

	/* linux/drm fallback */
	int hdcp_auth_result;
	int hdcp_fail_cnt;
	/* hdcp result callback for drm */
	struct connector_hdcp_cb tx_hdcp_cb;

	/* diff hw20/hw21, for sysfs operation */
	void (*set_hdcp_mode)(struct hdmitx_common *tx_comm, const char *buf);
	int (*get_hdcp_ver)(struct hdmitx_common *tx_comm, char *buf, int len);
	int (*get_tx_hdcp_cap)(struct hdmitx_common *tx_comm);

	/* diff hw20/hw21, for linux/drm control */
	void (*drm_hdcp_init)(struct hdmitx_common *tx_comm);
	void (*drm_hdcp_exit)(struct hdmitx_common *tx_comm);
	void (*drm_hdcp_enable)(struct hdmitx_common *tx_comm, int hdcp_type);
	void (*drm_hdcp_disable)(struct hdmitx_common *tx_comm);
	void (*drm_hdcp_disconnect)(struct hdmitx_common *tx_comm);
};

struct hdmitx_clk_tree_s {
	/* hdmitx clk tree */
	struct clk *hdmi_clk_vapb;
	struct clk *hdmi_clk_vpu;
	struct clk *hdcp22_tx_skp;
	struct clk *hdcp22_tx_esm;
	struct clk *cts_hdmi_axi_clk;
	struct clk *venci_top_gate;
	struct clk *venci_0_gate;
	struct clk *venci_1_gate;
};

struct st_debug_param {
	unsigned int avmute_frame;
	unsigned int disable_delay_ms;
	/* whether disable dmc request AXI bus for hdcp22/emp when suspend */
	bool dis_dmc_req;
};

/* 0: VESA DSC 1.2a is not supported
 * 1: up to 1 slice and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 2: up to 2 slices and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 3: up to 4 slices and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 4: up to 8 slices and up to (340 MHz/K SliceAdjust) pixel clock per slice
 * 5: up to 8 slices and up to (400 MHz/K SliceAdjust) pixel clock per slice
 * 6: up to 12 slices and up to (400 MHz/K SliceAdjust) pixel clock per slice
 * 7: up to 16 slices and up to (400 MHz/K SliceAdjust) pixel clock per slice
 * 8-15: Reserved
 */

static const u8 dsc_max_slices_num[] = {
	0,
	1,
	2,
	4,
	8,
	8,
	12,
	16
};

struct ced_cnt {
	bool ch0_valid;
	u16 ch0_cnt:15;
	bool ch1_valid;
	u16 ch1_cnt:15;
	bool ch2_valid;
	u16 ch2_cnt:15;
	u8 chksum;
	bool ch3_valid;
	u16 ch3_cnt:15;
	bool rs_c_valid;
	u16 rs_c_cnt:15;
};

struct scdc_locked_st {
	u8 clock_detected:1;
	u8 ch0_locked:1;
	u8 ch1_locked:1;
	u8 ch2_locked:1;
	u8 ch3_locked:1;
};

typedef void (*pf_callback)(bool st);

struct vendor_info_data {
	u8 *vendor_name; /* Max Chars: 8 */
	/* vendor_id, 3 Bytes, Refer to
	 * http://standards.ieee.org/develop/regauth/oui/oui.txt
	 */
	u8 *product_desc; /* Max Chars: 16 */
	u8 *cec_osd_string; /* Max Chars: 14 */
	u32 cec_config; /* 4 bytes: use to control cec switch on/off */
	u32 vendor_id;
};

struct hdmi_config_platform_data {
	struct vendor_info_data *vend_data;
	/* additional config data */
};

struct hdmitx_common {
	/* 1. general platform device related */
	struct cdev cdev;
	dev_t hdmitx_id;
	struct device *hdtx_dev;
	/* Indicates current status, HDMITX20/HDMITX21 */
	int hdmi_init;
	/* for platform pinmux */
	struct device *pdev;
	struct notifier_block reboot_nb;

	/* 2. drm connector/vout related */
	struct meson_connector_dev base;
	int drm_hdmitx_id;
	struct drm_vrr_ctrl_ops *drm_vrr_ctrl_ops;
	struct connector_hpd_cb drm_hpd_cb;
	struct vinfo_s hdmitx_vinfo;
	struct vout_device_s *vdev;

	/* 3. hdmitx internal HW function pointer */
	struct hdmitx_hw_common *tx_hw;

	/* 4. HPD related */
	/* workqueue dedicated for hpd event */
	struct workqueue_struct *hdmi_hpd_wq;
	struct delayed_work work_hpd_plugin;
	struct delayed_work work_hpd_plugout;
	/* save the last plug out/in work done state */
	enum hdmi_event_t last_hpd_handle_done_stat;
	/* 1: connect; 0: disconnect */
	unsigned char hpd_state;
	/*
	 * if HDMI plugin even once time, then set 1
	 * if never hdmi plugin, then keep as 0
	 * only for android ott.
	 */
	u32 already_used;
	/* 0xff: always running, TO BE REMOVED */
	u8 hpd_event;

	/* 5. edid related */
	/* edid hdr/dv cap lock, hdr/dv handle in irq, need spinlock */
	spinlock_t edid_spinlock;
	/* for external loading EDID */
	u32 forced_edid;
	unsigned char edid_buf[EDID_MAX_BLOCK * 128];
	u32 edid_mask_qms:1;
	struct rx_cap rxcap;
	/*
	 * edid parse debug flag
	 * true: edid parse in hdmitx
	 * false(default): edid parse in drm
	 */
	bool edid_parse_dbg;

	/* 6. video mode set related */
	/*
	 * Normally, after the HPD in or late resume, there will reading EDID, and
	 * notify application to select a hdmi mode output. But during the mode
	 * setting moment, there may be HPD out. It will clear the edid data, ..., etc.
	 * To avoid such case, here adds the hdmimode_mutex to let the HPD in, HPD out
	 * handler and mode setting sequentially.
	 */
	struct mutex hdmimode_mutex;
	/* mode valid check during mode set or sysfs test */
	struct mutex valid_mutex;
	/* indicate hdmitx output ready, sw/hw mode setting done */
	bool ready;
	/* if hdmitx is in early suspend state. */
	bool suspend_flag;
	/* set the flag 1, don't check suspend_flag when mode setting. */
	bool drm_modeset_flag;
	/*current format para.*/
	struct hdmi_format_para fmt_para;
	/* for debug, it will override the frac mode set by drm.
	 * bit1: 1: force enable, 0: force disable
	 * bit0: 1: force fractional mode, 0: force non-fractional mode;
	 */
	u8 force_frac_mode;
	/*
	 * color standard
	 * 0: 709
	 * 1: 601
	 * 2: 2020
	 * 3: 2020c
	 */
	u8 in_colorimetry;
	u8 out_colorimetry;
	/*
	 * vid/pc
	 * 0: limit; 1: full
	 */
	u8 in_color_range;
	u8 out_color_range;
	/*
	 * yc/rgb
	 * 0: yuv; 1: rgb
	 */
	u8 in_color_fmt;
	/* Indicates spread spectrum, 1/on 0/off */
	u32 sspll;
	/* check clk stauts after mode set */
	struct task_struct *task_clk_check;
#ifdef CONFIG_AMLOGIC_VPU
	struct vpu_dev_s *encp_vpu_dev;
	struct vpu_dev_s *enci_vpu_dev;
	struct vpu_dev_s *hdmi_vpu_dev;
	struct vpu_dev_s *hdmitx_vpu_clk_gate_dev;
#endif
	/* when the mode setting fails, it is necessary to ensure
	 * that vsync is enabled so that subsequent actions are not
	 * blocked. Phy cannot be enabled to avoid affecting TV
	 */
	bool skip_phy_setting;
	u8 scan_info;

	/* 7. audio mode related */
	struct aud_para cur_audio_param;
	/* save the last audio param */
	struct aud_para hdmiaud_config_data;
	struct notifier_block hdmitx_notifier_nb_a;
	/* hdmitx_audio_mute_op need mutex */
	struct mutex aud_mute_mutex;

	/* 8. earc related */
	pf_callback earc_hdmitx_hpdst;

	/* 9.hdcp realted */
	struct hdcptx_common hdcptx_comm;
	/* hw private hdcp struct */
	void *hdcptx_priv;

	/* 10. HDR related */
	u32 hdr_priority;
	u32 hdr_8bit_en;
	/*for color space conversion*/
	bool config_csc_en;
	u32 output_color_format;
	bool csc_config_in_next_frame;
	u8 csc_delay_frame;
	u32 hdmi_last_hdr_mode;
	u32 hdmi_current_hdr_mode;
	/*
	 * for SONY-KD-55A8F TV, need to mute more frames(default 20 frames)
	 * when switch DV(LL)->HLG
	 */
	int hdr_mute_frame;
	/* hdr mute operation */
	unsigned char vid_mute_op;
	/*
	 * When exiting hdr10plus, hdmitx_set_hdr10plus_pkt will be
	 * called first to send a hdr10plus vsif packet with all zeros.
	 * At this time, all_zero_hdr10plus_pkt needs to be set to true.
	 * Then, when hdmitx_set_drm_pkt is called to disable the drm
	 * infoframe, if all_zero_hdr10plus_pkt is true, it is necessary
	 * to disable the hdr10plus vsif packet with all zeros and set
	 * all_zero_hdr10plus_pkt to false
	 */
	bool all_zero_hdr10plus_pkt;
	/* save the last sent infoframe */
	struct vsif_debug_save vsif_debug_info;
	struct master_display_info_s drm_config_data;
	struct hdr10plus_para hdr10p_config_data;
	/* save the last sent hdr param */
	enum hdmi_hdr_transfer hdr_transfer_feature;
	enum hdmi_hdr_color hdr_color_feature;
	unsigned int colormetry;
	/* hdr work */
	struct work_struct work_hdr;
	struct work_struct work_hdr_unmute;
	/* hdr10plus flag */
	unsigned int hdr10plus_feature;
	/* cuva/hdr vivid reset flag */
	bool cuva_dhdr_reset;
	/* amdv info */
	enum eotf_type hdmi_current_eotf_type;
	enum mode_type hdmi_current_tunnel_mode;
	bool hdmi_current_signal_sdr;
	/* hdmitx infoframe */
	struct hdmitx_infoframe infoframe;

	/* 11. vrr related*/
	struct vrr_device_s hdmitx_vrr_dev;
	/* 1: GAME-VRR, 2: QMS-VRR,  0: default no-VRR */
	enum vrr_type vrr_mode;
	/*
	 * the qms_log_id is referred from hw_sequence_id
	 * if value is not changed, the skip massive qms log
	 */
	u64 qms_log_id;

	/* 12. allm related */
	/* allm_mode: 1/on 0/off */
	u32 allm_mode;
	/* contenttype:0/off 1/game, 2/graphics, 3/photo, 4/cinema */
	u32 ct_mode;
	bool it_content;
	/* below are only for special hdmitx21 project */
	/* ll setting: 0/AUTOMATIC, 1/Always OFF, 2/ALWAYS ON */
	enum hdmi_ll_mode ll_user_set_mode;
	/* ll_mode enabled in auto or not */
	bool ll_enabled_in_auto_mode;

	/* 13. configuration from dts or efuse ******/
	/* 0: TV product, 1: stb/soundbar product;
	 * used to check if need to notify edid to
	 * upstream hdmirx.
	 */
	u32 hdmi_repeater;
	/* enc index: for non-ott product*/
	u32 enc_idx;
	int clk_source_idx;
	/*soc limitation config*/
	u32 res_1080p;
	/* efuse ctrl state
	 * 1 disable the function
	 * 0 dont disable the function
	 */
	bool efuse_dis_hdmi_4k60;	/* 4k50,60hz */
	bool efuse_dis_output_4k;	/* all 4k resolution*/
	bool efuse_dis_hdcp_tx22;	/* hdcptx22 */
	bool efuse_dis_hdmi_tx3d;	/* 3d */
	bool efuse_dis_hdcp_tx14;	/* s1a hdcptx14 */
	u32 max_refresh_rate;
	u32 hdmi_rext; /* Rext resistor */
	struct hdmi_config_platform_data config_data;
	u32 pxp_mode:1;
	u32 arc_rx_en;
	struct hdmitx_clk_tree_s hdmitx_clk_tree;
	struct pinctrl_state *pinctrl_ddc;
	struct pinctrl_state *pinctrl_hpd;
	struct pinctrl_state *pinctrl_i2c;	/* general i2c func */
	/* kernel 6.12 or later, /amhdmitx/frac_enable should be set as default 1 */
	bool frac_enable;

	/* 14. ced/rxsense related */
	bool cedst_en;
	u32 cedst_policy;
	struct workqueue_struct *cedst_wq;
	struct delayed_work work_cedst;
	u32 rxsense_policy;
	struct workqueue_struct *rxsense_wq;
	struct delayed_work work_rxsense;
	struct ced_cnt ced_cnt;
	struct scdc_locked_st ch_locked_st;
	u16 ced_check_count;

	/* 15. debug & log */
	struct hdmitx_tracer *tx_tracer;
	struct hdmitx_event_mgr *event_mgr;
	struct st_debug_param debug_param;
	struct dentry *hdmitx_file_dbgfs;
	struct proc_dir_entry *hdmitx_proc_dbgfs;
	/* hdmitx bist */
	unsigned int bist_lock:1;

	/**********only used for hdmitx20**********/
	/* in board dts file, here can add
	 * &amhdmitx {
	 *     hdcp_type_policy = <1>;
	 * };
	 * 0 is default for NTS 0->1, 1 is fixed as 1, and 2 is fixed as 0
	 */
	/* -1, fixed 0; 0, NTS 0->1; 1, fixed 1 */
	int hdcp_type_policy;
	int hdcp_tst_sig;
	struct hdcprp_topo *topo_info;
	bool hdcp22_type;
	/* enable poll rx_status for workaround of special hdcp2.2 TV */
	bool en_poll_rx_status;
	/* poll rx_status workaround method:
	 * 0: continuously poll rx_status(2bytes) for 300ms, default method
	 * 1: read only 1 byte of hdcp msg
	 */
	u8 poll_rx_status_mtd;
	/**********only used for hdmitx20 end**********/

	/* 16. interrupt related */
	u32 irq_hpd;
	u32 irq_hdmitx_vsync;
	struct delayed_work work_internal_intr;
};

#define to_hdmitx_common(x)	container_of(x, struct hdmitx_common, base)

void hdmitx_get_init_state(struct hdmitx_common *tx_common,
			   struct hdmitx_common_state *state);

/*******************************hdmitx common api*******************************/
/* if 4k can support Y420, return 1.
 * when current cs == 420, and vic can support 420,
 * current output is 4k420 mode.
 */
bool is_hdmi4k_support_420(enum hdmi_vic vic);
int hdmitx_common_init(struct hdmitx_common *tx_common, struct hdmitx_hw_common *hw_comm);
int hdmitx_common_post_init(struct hdmitx_common *tx_common, struct hdmitx_hw_common *hw_comm);
int hdmitx_common_destroy(struct hdmitx_common *tx_common);
/* modename policy: get vic from name and check if support by rx;
 * return the vic of mode, if failed return HDMI_0_UNKNOWN;
 */
int hdmitx_common_parse_vic_in_edid(struct hdmitx_common *tx_comm, const char *mode);

/* create hdmi_format_para from config and also calc setting from hw; */
int hdmitx_common_build_format_para(struct hdmitx_common *tx_comm,
		struct hdmi_format_para *para, enum hdmi_vic vic, u32 frac_rate_policy,
		enum hdmi_colorspace cs, enum hdmi_color_depth cd, enum hdmi_quantization_range cr);

/* For bootup init: init hdmi_format_para from hw configs.*/
int hdmitx_common_init_bootup_format_para(struct hdmitx_common *tx_comm,
		struct hdmi_format_para *para);
/*mode name api*/
bool is_mode_name_frac(const char *name);
void convert_name_frac2int(const char *name, char *conv_name);

/*edid valid api*/
bool hdmitx_edid_only_support_sd(struct rx_cap *prxcap);

/* for bootup case or debug purpose, need to parse whole edid in hdmi side
 * for other case(plugin/resume), parse edid in two steps:
 * step1: drm_parse_part = false, only parse phy_addr(for cec)/audio(for audio cap)
 * part on hdmi side
 * step2: drm_parse_part = true, parse other data blocks on drm side
 */
void hdmitx_edid_process(struct hdmitx_common *tx_comm, bool boot_flag, bool drm_parse_part);

/* Attach platform related functions to hdmitx_common;
 * Currently hdmitx_tracer, hdmitx_uevent_mgr is platform related;
 */
enum HDMITX_PLATFORM_API_TYPE {
	HDMITX_PLATFORM_TRACER = 0,
	HDMITX_PLATFORM_UEVENT,
};

int hdmitx_common_attch_platform_data(struct hdmitx_common *tx_comm,
	enum HDMITX_PLATFORM_API_TYPE type, void *plt_data);

/*
 * Notify hpd event to all outer modules: vpp by vout, drm, userspace
 * bool force_uevent: force send uevent even the hpd state NOT change
 */
int hdmitx_common_notify_ced_status(struct hdmitx_common *tx_comm);
int hdmitx_common_notify_hpd_status(struct hdmitx_common *tx_comm, bool force_uevent);

int hdmitx_set_uevent(struct hdmitx_common *tx_comm, enum hdmitx_event type, int val);

u32 hdmitx_edid_get_hdmi14_4k_vic(u32 vic);
/* packet api */
/* mode = 0 , disable allm; mode 1: set allm; mode -1: */
int hdmitx_common_set_allm_mode(struct hdmitx_common *tx_comm, int mode);

/*
 * avmute function with lock:
 * do set mute when mute cmd from any path;
 * do clear when all path have cleared avmute;
 */
#define AVMUTE_PATH_1 0x80 //mute by avmute sysfs node
#define AVMUTE_PATH_2 0x40 //mute by upstream side request re-auth
#define AVMUTE_PATH_DRM 0x20 //called by drm;
#define AVMUTE_PATH_HDMITX 0x10 //internal use

int hdmitx_common_avmute_locked(struct hdmitx_common *tx_comm,
		int mute_flag, int mute_path_hint);

/*read edid raw data and parse edid to rxcap*/
int hdmitx_common_get_edid(struct hdmitx_common *tx_comm);

/*modesetting function*/
int hdmitx_common_do_mode_setting(struct hdmitx_common *tx_comm,
				  struct hdmitx_common_state *new,
				  struct hdmitx_common_state *old);
int hdmitx_common_validate_mode_locked(struct hdmitx_common *tx_comm,
				       struct hdmitx_common_state *new_state,
				       char *mode,  enum hdmi_colorspace cs,
				       enum hdmi_color_depth cd, bool brr_valid);
int hdmitx_common_check_valid_para_of_vic(struct hdmitx_common *tx_comm,
					  enum hdmi_vic vic);
int hdmitx_common_disable_mode(struct hdmitx_common *tx_comm,
			       struct hdmitx_common_state *new_state);
int set_disp_mode_debug(struct hdmitx_common *tx_comm, const char *mode, char *fmt_attr);

unsigned int hdmitx_get_frame_duration(struct hdmitx_common *tx_comm);
int hdmitx_set_display(struct hdmitx_common *tx_comm, enum hdmi_vic video_code);

/* hdcp api*/
void hdmitx_set_hdcp_mode(struct hdmitx_common *tx_comm, const char *buf);
int hdmitx_get_hdcp_ver(struct hdmitx_common *tx_comm, char *buf, int len);

/* drm hdcp api */
int drm_hdmitx_common_hdcp_init(struct hdmitx_common *tx_comm);
int drm_hdmitx_common_hdcp_exit(struct hdmitx_common *tx_comm);
int drm_hdmitx_common_hdcp_enable(struct hdmitx_common *tx_comm, int hdcp_type);
int drm_hdmitx_common_hdcp_disable(struct hdmitx_common *tx_comm);
int drm_hdmitx_common_hdcp_disconnect(struct hdmitx_common *tx_comm);
unsigned int drm_hdmitx_common_get_tx_hdcp_cap(struct hdmitx_common *tx_comm);
unsigned int drm_hdmitx_common_get_rx_hdcp_cap(struct hdmitx_common *tx_comm);
int drm_hdmitx_common_register_hdcp_notify(struct hdmitx_common *tx_comm,
					   struct connector_hdcp_cb *cb);
int drm_hdmitx_common_get_dw_hdcp_topo_info(struct hdmitx_common *tx_comm);

void hdmitx_common_sw_debug_func(struct hdmitx_common *tx_comm, const char *cmd_str);
/*******************************hdmitx common api end*******************************/

int hdmitx_register_hpd_cb(struct hdmitx_common *tx_comm, struct connector_hpd_cb *hpd_cb);
int hdmitx_fire_drm_hpd_cb_unlocked(struct hdmitx_common *tx_comm);
int hdmitx_audio_register_ctrl_callback(struct hdmitx_tracer *tracer,
						audio_en_callback cb1, audio_st_callback cb2);

int hdmitx_get_hpd_state(struct hdmitx_common *tx_comm);
u64 hdmitx_get_hpd_hw_sequence_id(struct hdmitx_common *tx_comm);
unsigned char *hdmitx_get_raw_edid(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_ready_state(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_edid_valid_state(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_hdcp_user_state(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_hdmi_used_state(struct hdmitx_common *tx_comm);

int hdmitx_get_attr(struct hdmitx_common *tx_comm, int *cs, int *cd);

int hdmitx_get_hdrinfo(struct hdmitx_common *tx_comm, struct hdr_info *hdrinfo);
int hdmitx_get_hdrinfo_rx(struct hdmitx_common *tx_comm, struct hdr_info *hdrinfo);

int hdmitx_set_hdr_priority(struct hdmitx_common *tx_comm, u32 hdr_priority,
		struct hdr_info *hdr_info, struct dv_info *dv_info);
int hdmitx_get_hdr_priority(struct hdmitx_common *tx_comm, u32 *hdr_priority);
void hdmitx_hdr_state_init(struct hdmitx_common *tx_comm);
bool hdmitx_hdr_en(struct hdmitx_hw_common *tx_hw);
bool hdmitx_dv_en(struct hdmitx_hw_common *tx_hw);
bool hdmitx_hdr10p_en(struct hdmitx_hw_common *tx_hw);

enum hdmi_scan_mode hdmitx_common_get_scan_info(struct hdmitx_common *tx_comm);
int hdmitx_common_set_scan_info(struct hdmitx_common *tx_comm, enum hdmi_scan_mode val);
enum hdmi_scan_mode hdmitx_check_scan_info(struct rx_cap *prxcap,
		enum hdmi_scan_mode val, enum hdmi_vic vic);

u32 hdmitx_get_frl_bandwidth(const enum frl_rate_enum rate);
u32 hdmitx_calc_frl_bandwidth(u32 pixel_freq, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd);
u32 hdmitx_calc_tmds_clk(u32 pixel_freq,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd);
enum frl_rate_enum hdmitx_select_frl_rate(u8 *dsc_en, u8 dsc_policy, enum hdmi_vic vic,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd);

/*edid related function.*/
bool is_tv_changed(char *cur_edid_chksum, char *boot_param_edid_chksum);

void hdmitx_vout_init(struct hdmitx_common *tx_comm, struct hdmitx_hw_common *tx_hw);
void hdmitx_vout_uninit(void);
struct vinfo_s *hdmitx_get_current_vinfo(void *tx_comm);
void hdmitx_build_fmt_attr_str(struct hdmitx_common *tx_comm);
void hdmitx_current_status(struct hdmitx_common *tx_comm, enum hdmitx_event_log_bits event);
ssize_t hdcp_lstore_show(struct device *dev, struct device_attribute *attr,
				char *buf);
ssize_t hdcp_mode_show(struct device *dev, struct device_attribute *attr,
			      char *buf);
ssize_t hdcp_ver_show(struct device *dev, struct device_attribute *attr,
			     char *buf);

/* common work for plugin/resume, which is done in lock */
void hdmitx_process_plugin(struct hdmitx_common *tx_comm, bool boot_flag, bool set_audio);
/* common work for plugout, which is done in lock */
void hdmitx_process_plugout(struct hdmitx_common *tx_comm);
void hdmitx_bootup_post_process(struct hdmitx_common *tx_comm);
/* delaywork for plugin/plugout */
void hdmitx_hpd_plugin_irq_handler(struct work_struct *work);
void hdmitx_hpd_plugout_irq_handler(struct work_struct *work);

/* common edid clear, which is done in lock */
void hdmitx_common_edid_clear(struct hdmitx_common *tx_comm);
/* common work for late resume, which is done in lock */
void hdmitx_common_late_resume(struct hdmitx_common *tx_comm);
/* common disable hdmitx output api */
void hdmitx_common_output_disable(struct hdmitx_common *tx_comm,
	bool phy_dis, bool hdcp_reset, bool pkt_clear, bool edid_clear);

/*******************************drm hdmitx api*******************************/

unsigned int hdmitx_common_get_content_types(struct hdmitx_common *tx_comm);
int hdmitx_common_set_contenttype(struct hdmitx_common *tx_comm, int content_type);
const struct dv_info *hdmitx_common_get_dv_info(struct hdmitx_common *tx_comm);
const struct dv_info *hdmitx_common_get_dv_info_rx(struct hdmitx_common *tx_comm);
const struct hdr_info *hdmitx_common_get_hdr_info(struct hdmitx_common *tx_comm);
const struct hdr_info *hdmitx_common_get_hdr_info_rx(struct hdmitx_common *tx_comm);
int hdmitx_common_get_vic_list(struct hdmitx_common *tx_comm, int **vics);
void get_hdmi_efuse(struct hdmitx_common *tx_comm);
enum hdmi_vic hdmitx_get_prefer_vic(struct hdmitx_common *tx_comm, enum hdmi_vic vic);
enum frl_rate_enum get_dsc_frl_rate(enum dsc_encode_mode dsc_mode);
void hdmitx_get_qms_init_state(struct hdmitx_common *tx_common, u32 *brr, u32 *qms_en);
enum hdmi_hdr_status hdmitx_common_get_hdr_status(struct hdmitx_common *tx_comm);
u32 hdmitx_common_get_vrr_cap(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_sink_device_type(struct hdmitx_common *tx_comm);

int hdmitx_common_get_vrr_mode_group(struct hdmitx_common *tx_comm,
				     struct hdmitx_vrr_mode_group *group,
				     int max_group);
int hdmitx_common_set_vframe_rate_hint(struct hdmitx_common *tx_comm,
		struct hdmitx_common_state *new_state, int rate, void *data);
/* hdmitx diagnostic system api */
unsigned char *hdmitx_common_get_resolution(struct hdmitx_common *tx_comm);
int hdmitx_common_get_rxsense(struct hdmitx_common *tx_comm);
enum hdmi_colorspace hdmitx_common_get_colorspace(struct hdmitx_common *tx_comm);
int hdmitx_common_get_colordepth(struct hdmitx_common *tx_comm);
enum hdmi_colorimetry hdmitx_common_get_colorimetry(struct hdmitx_common *tx_comm);
enum hdmi_extended_colorimetry hdmitx_common_get_extended_colorimetry(struct hdmitx_common
			*tx_comm);
int hdmitx_common_get_vic(struct hdmitx_common *tx_comm);
enum hdmi_picture_aspect hdmitx_common_get_picture_aspect(struct hdmitx_common *tx_comm);
enum hdmi_active_aspect hdmitx_common_get_active_aspect(struct hdmitx_common *tx_comm);
enum hdmi_quantization_range
	hdmitx_common_get_quantization_range(struct hdmitx_common *tx_comm);
enum hdmi_nups hdmitx_common_get_nups(struct hdmitx_common *tx_comm);
enum hdmi_ycc_quantization_range
	hdmitx_common_get_ycc_quantization_range(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_itc(struct hdmitx_common *tx_comm);
enum hdmi_content_type hdmitx_common_get_content_type(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_pixel_repeat(struct hdmitx_common *tx_comm);
unsigned short hdmitx_common_get_top_bar(struct hdmitx_common *tx_comm);
unsigned short hdmitx_common_get_bottom_bar(struct hdmitx_common *tx_comm);
unsigned short hdmitx_common_get_left_bar(struct hdmitx_common *tx_comm);
unsigned short hdmitx_common_get_right_bar(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_drm_eotf(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_audio_channel(struct hdmitx_common *tx_comm);
enum hdmi_audio_coding_type hdmitx_common_get_audio_coding_type(struct hdmitx_common *tx_comm);
enum hdmi_audio_sample_size hdmitx_common_get_audio_sample_size(struct hdmitx_common *tx_comm);
enum hdmi_audio_sample_frequency
	hdmitx_common_get_audio_sample_frequency(struct hdmitx_common *tx_comm);
enum hdmi_audio_coding_type_ext
	hdmitx_common_get_audio_coding_type_ext(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_audio_channel_allocation(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_audio_level_shift_value(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_audio_downmix_inhibit(struct hdmitx_common *tx_comm);
enum hdmi_color_depth hdmitx_common_get_color_depth(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_avmute_state(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_m_const(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_next_tfr(struct hdmitx_common *tx_comm);
unsigned char hdmitx_common_get_base_refresh_rate(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_audio_n(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_audio_cts(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_audio_layout(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_tmds_clk(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_pixel_clk(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_hdcp_auth_state(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_hdcp_an(struct hdmitx_common *tx_comm, u8 *an);
unsigned int hdmitx_common_get_hdcp_aksv(struct hdmitx_common *tx_comm, u8 *aksv);
unsigned int hdmitx_common_get_hdcp_bksv(struct hdmitx_common *tx_comm, u8 *bksv);
unsigned int hdmitx_common_get_hdcp_bstatus(struct hdmitx_common *tx_comm, u8 *bstatus);
unsigned int hdmitx_common_get_hdcp_bcaps(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_hdcp_ri(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_vsif_ieeeoui(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_vsif_vic(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_vsif_allm(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_vsif_amdv_signal(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_vsif_amdv_low_latency(struct hdmitx_common *tx_comm);
enum hdmi_spd_sdi hdmitx_common_get_spd_sdi(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_scdc_sts_flag0(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_scdc_ln0_ln1_ltp(struct hdmitx_common *tx_comm);
unsigned int hdmitx_common_get_scdc_ln2_ln3_ltp(struct hdmitx_common *tx_comm);

/**********only used for hdmitx20**********/
#define HDCP_SLAVE	0x3a
#define HDCP2_RD_MSG	0x80
unsigned int get_hdcp22_base(void);
bool is_hdcp22_stop_state(struct hdmitx_common *tx_comm);
void hdmitx_reset_tv_hdcp(void);
u32 ddc_read_1byte(u8 slave, uint8_t offset_addr, uint8_t *data);
void hdmitx_hdcp_do_work(struct hdmitx_common *tx_comm);
/**********only used for hdmitx20 end**********/

/**********only used for hdmitx21**********/
ssize_t _vrr_cap_show(struct device *dev, struct device_attribute *attr,
	char *buf);
/**********only used for hdmitx21 end**********/

#endif
