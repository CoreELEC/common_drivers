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
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_dev.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_format_para.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_hw_common.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_edid.h>
#include <linux/amlogic/media/vout/meson_tx_connector/hdmitx_common/hdmitx_types.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx.h>
#include <linux/amlogic/media/vout/dsc.h>
#include <linux/amlogic/media/vrr/vrr.h>

#define HDMITX20 20
#define HDMITX21 21
#define DEVICE_NAME "amhdmitx"

#define HDMI_TX_POOL_NUM	6
#define HDMI_TX_RESOURCE_NUM 4
#define HDMI_TX_PWR_CTRL_NUM	6
/* this value must be >= 1 */
#define CSC_DELAY_FRAME		2

typedef void (*audio_en_callback)(bool enable);
typedef int (*audio_st_callback)(void);
struct hdcptx_common;
struct meson_tx_event_mgr;
enum tx_trace_log_bits;
enum meson_tx_event;

struct hdcptx_api {
	bool (*get_hdcptx_lstore)(struct hdcptx_common *hdcptx_comm, int hdcp_type);
	bool (*get_hdcptx_result)(struct hdcptx_common *hdcptx_comm, int hdcp_type);
	bool (*hdcptx22_topo_ctrl)(struct hdcptx_common *hdcptx_comm, int cmd, int topo_type);
	void (*hdcptx_reset_param)(void *hdcptx_instance);
	void (*hdcptx_enable)(void *hdcptx_instance, int hdcp_type, unsigned long delay);
	void (*hdcptx_disable)(void *hdcptx_instance);
	int (*get_hdcprx_ver)(struct hdcptx_common *hdcptx_comm);
	u32 (*get_hdcptx_mode)(void *hdcptx_instance);
	u8 (*hdcptx_reauth_req)(void *hdcptx_instance, u8 hdcp_version);
	void (*hdcptx_uninit)(void *hdcptx_instance);
	void (*hdcptx_debug)(void *hdcptx_instance, const char *buf);
	void (*hdcptx14_get_aksv)(u8 *p_aksv, u8 b);
	void (*hdcptx14_get_bksv)(u8 *p_bksv, u8 b);
	void (*hdcptx14_get_an)(u8 *p_an, u8 b);
	void (*hdcptx14_get_bstatus)(u8 *p_bstatus);
	u16 (*hdcptx14_get_ri)(void);
	int (*hdcptx_validate_key)(struct hdcptx_common *hdcptx_comm, int hdcp_type);
};

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

	/* indicate hdmitx output ready, sw/hw mode setting done */
	bool ready;
	/* if hdmitx is in suspend state. */
	bool suspend_flag;
	/* 1: connect; 0: disconnect */
	unsigned char hpd_state;

	bool efuse_dis_hdcp_tx22;	/* hdcptx22 */
	bool efuse_dis_hdcp_tx14;	/* s1a hdcptx14 */

	enum amhdmitx_chip_e chip_type;
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
	/*
	 * hdcp fail event method 2: don't stop-restart hdcp auth
	 * only for special project.
	 * when start play game movie, it will switch from
	 * DV STD to DV LL, hdcp will stop and restart after
	 * mode setting done.
	 * now provide an option: when DV STD <-> DV LL
	 * switch caused by game movie, systemcontrol write 1
	 * to not_restart_hdcp node before do mode switch, it
	 * means that this colorspace(mode) switch will only
	 * switch mode, but won't stop->restart hdcp action to
	 * prevent hdcp fail uevent sent to app.
	 * note:
	 * 1.not sure if it will always work on different TV.
	 * 2.after mode switch done, not_restart_hdcp will be self cleared
	 */
	bool not_restart_hdcp;
	/* linux/drm fallback */
	int hdcp_auth_result;
	int hdcp_fail_cnt;
	/* hdcp result callback for drm */
	struct connector_hdcp_cb tx_hdcp_cb;
	u32 hdcp_debug_delay;
	/* hdcp1.4 key only need to be loaded once when bootup/resume,
	 * set this flag true after hdcp1.4 key loaded
	 */
	bool hdcp14_key_loaded;

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

	void *bind_tx_comm;		/* point to struct hdmitx_common */
	void *bind_hw_comm;		/* point to struct hdmitx_hw_common */
	void *event_mgr;		/* point to struct hdmitx_event_mgr */
	void (*video_mute_op)(void *tx_hw, u32 flag, unsigned int path);
	void (*audio_mute_op)(void *tx_comm, u32 flag, unsigned int path);
	int (*avmute_op)(void *tx_comm, int mute_flag, int mute_path_hint);
	int (*send_event)(void *event_mgr, unsigned long state, void *arg);
	int (*validate_hdcp14_key)(void *tx_comm);
	int (*tmds_enable)(void *tx_comm);
	int (*tmds_disable)(void *tx_comm);
	bool (*ddcm_read)(u8 seg_index, u8 slave_addr,
		u8 reg_addr, u8 *p_buf, u16 len, u8 read_cmd);
	bool (*ddcm_write)(u8 seg_index, u8 slave_addr, u8 reg_addr, u8 data);
	/* provided to external api */
	struct hdcptx_api *hdcptx_api;
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

struct meson_tx_hdr {
	/* hdr info */
	enum hdmi_hdr_transfer hdr_transfer_feature;
	enum hdmi_hdr_color hdr_color_feature;
	unsigned int colormetry;
	/*
	 * for SONY-KD-55A8F TV, need to mute more frames(default 20 frames)
	 * when switch DV(LL)->HLG
	 */
	int hdr_mute_frame;
	/* hdr mute operation */
	unsigned char vid_mute_op;
	/* HDR format state */
	u32 hdmi_last_hdr_mode;
	u32 hdmi_current_hdr_mode;
	/* enable flag for allowing output 8bit hdr */
	u32 hdr_8bit_en;
	/* enable flag for color space conversion */
	bool config_csc_en;
	u32 output_color_format;
	bool csc_config_in_next_frame;
	u8 csc_delay_frame;

	/* hdr10plus flag */
	unsigned int hdr10plus_feature;
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

	/* cuva/hdr vivid reset flag */
	bool cuva_dhdr_reset;
	/* amdv info */
	enum eotf_type hdmi_current_eotf_type;
	enum mode_type hdmi_current_tunnel_mode;
	bool hdmi_current_signal_sdr;

	/*hdr pkt config from hdr/dv core*/
	struct master_display_info_s drm_config_data;
	struct hdr10plus_para hdr10p_config_data;
	struct vsif_debug_save vsif_debug_info;

	/* colorimetry&range dynamic setting from hdr core */
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

	struct hdmitx_common *bind_instance;
};

/* meson_hdmi.c will allocate the memory including hw_fmt_para */
struct hdmitx_state {
	struct meson_tx_state base_state;
	struct hdmitx_hw_fmt_para hw_fmt_para;
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
	struct meson_tx_dev base;
	int drm_hdmitx_id;
	struct drm_vrr_ctrl_ops *drm_vrr_ctrl_ops;
	struct connector_hpd_cb drm_hpd_cb;
	struct vinfo_s hdmitx_vinfo;
	struct vout_device_s *vdev;

	/* 3. hdmitx internal HW function pointer */
	struct hdmitx_hw_common *tx_hw;

	/* 4. HPD related */
	/* 0xff: always running, TO BE REMOVED */
	u8 hpd_event;

	/* 5. edid related */
	/* edid hdr/dv cap lock, hdr/dv handle in irq, need spinlock */
	spinlock_t edid_spinlock;
	/* for external loading EDID */
	u32 forced_edid;
	u32 edid_mask_qms:1;
	/*
	 * edid parse debug flag
	 * true: edid parse in hdmitx
	 * false(default): edid parse in drm
	 */
	bool edid_parse_dbg;

	/* 6. video mode set related */
	/* indicate hdmitx output ready, sw/hw mode setting done */
	bool ready;
	/* if hdmitx is in suspend state. */
	bool suspend_flag;
	/*current format para.*/
	/* TODO: remove, it's moved to struct meson_tx_dev */
	struct meson_tx_format_para fmt_para;

	struct hdmitx_hw_fmt_para hw_fmt_para;
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
	struct meson_tx_hdr *hdr_state;
	u32 hdr_priority;
	/* hdmitx infoframe */
	struct hdmitx_infoframe infoframe;

	/* 11. vrr related*/
	struct vrr_device_s hdmitx_vrr_dev;
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
	/*soc limitation config*/
	u32 res_1080p;
	/* efuse ctrl state
	 * 1 disable the function
	 * 0 don't disable the function
	 */
	bool efuse_dis_hdmi_4k60;	/* 4k50,60hz */
	bool efuse_dis_output_4k;	/* all 4k resolution*/
	bool efuse_dis_hdmi_tx3d;	/* 3d */
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
	u32 rxsense_policy;
	struct ced_cnt ced_cnt;
	struct scdc_locked_st ch_locked_st;
	u16 ced_check_count;

	/* 15. debug & log */
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
};

#define to_hdmitx_common(x)	container_of(x, struct hdmitx_common, base.conn_dev)
#define to_hdmitx_state(x)	container_of(x, struct hdmitx_state, base_state)
#define tx_dev_to_hdmitx_common(x)	container_of(x, struct hdmitx_common, base)

void hdmitx_get_init_state(struct hdmitx_common *tx_common,
			   struct meson_tx_state *state);

/*******************************hdmitx common api*******************************/
int hdmitx_common_init(struct hdmitx_common *tx_common, struct hdmitx_hw_common *hw_comm);
int hdmitx_common_destroy(struct hdmitx_common *tx_common);
/* modename policy: get vic from name and check if support by rx;
 * return the vic of mode, if failed return HDMI_0_UNKNOWN;
 */
int hdmitx_common_parse_vic_in_edid(struct hdmitx_common *tx_comm, const char *mode);

/* create hdmi_format_para from config and also calc setting from hw; */
int hdmitx_common_build_format_para(struct hdmitx_common *tx_comm,
		struct meson_tx_format_para *para, enum hdmi_vic vic, u32 frac_rate_policy,
		enum hdmi_colorspace cs, enum hdmi_color_depth cd, enum hdmi_quantization_range cr);

/* For bootup init: init hdmi_format_para from hw configs.*/
int hdmitx_common_init_bootup_format_para(struct hdmitx_common *tx_comm,
		struct meson_tx_format_para *para);
/*mode name api*/
bool hdmitx_is_mode_name_frac(const char *name);
void hdmitx_convert_name_frac2int(const char *name, char *conv_name);

/*edid valid api*/
bool hdmitx_edid_only_support_sd(struct rx_cap *prxcap);

/* for bootup case or debug purpose, need to parse whole edid in hdmi side
 * for other case(plugin/resume), parse edid in two steps:
 * step1: drm_parse_part = false, only parse phy_addr(for cec)/audio(for audio cap)
 * part on hdmi side
 * step2: drm_parse_part = true, parse other data blocks on drm side
 */
void hdmitx_edid_process(struct hdmitx_common *tx_comm, bool boot_flag, bool drm_parse_part);

int hdmitx_set_uevent(struct hdmitx_common *tx_comm, enum meson_tx_event type, int val);

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
				  struct meson_tx_state *new,
				  struct meson_tx_state *old);
int hdmitx_common_validate_mode_locked(struct hdmitx_common *tx_comm,
	struct meson_tx_state *new_state,
	char *mode,  enum hdmi_colorspace cs,
	enum hdmi_color_depth cd, bool brr_valid);
int hdmitx_validate_tx_state_fmt_para(struct meson_tx_dev *tx_base,
	struct meson_tx_state *base_state);
int hdmitx_build_hw_format_para(struct meson_tx_dev *tx_base,
	struct meson_tx_format_para *para);
int hdmitx_common_check_valid_para_of_vic(struct hdmitx_common *tx_comm,
					  enum hdmi_vic vic);

int hdmitx_common_disable_mode(struct hdmitx_common *tx_comm,
			       struct meson_tx_state *new_state);
int set_disp_mode_debug(struct hdmitx_common *tx_comm, const char *mode, char *fmt_attr);

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

int hdmitx_audio_register_ctrl_callback(struct meson_tx_tracer *tracer,
						audio_en_callback cb1, audio_st_callback cb2);

int hdmitx_get_hpd_state(struct hdmitx_common *tx_comm);
u64 hdmitx_get_hpd_hw_sequence_id(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_ready_state(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_edid_valid_state(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_hdcp_user_state(struct hdmitx_common *tx_comm);

int hdmitx_get_attr(struct hdmitx_common *tx_comm, int *cs, int *cd);

int hdmitx_get_hdrinfo(struct hdmitx_common *tx_comm, struct hdr_info *hdrinfo);
int hdmitx_get_hdrinfo_rx(struct hdmitx_common *tx_comm, struct hdr_info *hdrinfo);

int hdmitx_set_hdr_priority(struct hdmitx_common *tx_comm, u32 hdr_priority,
		struct hdr_info *hdr_info, struct dv_info *dv_info);
int hdmitx_get_hdr_priority(struct hdmitx_common *tx_comm, u32 *hdr_priority);

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
void hdmitx_vout_uninit(struct hdmitx_common *tx_comm);
void hdmitx_build_fmt_attr_str(struct hdmitx_common *tx_comm);
void hdmitx_current_status(struct hdmitx_common *tx_comm, enum tx_trace_log_bits event);
ssize_t hdcp_lstore_show(struct device *dev, struct device_attribute *attr,
				char *buf);
ssize_t hdcp_mode_show(struct device *dev, struct device_attribute *attr,
			      char *buf);
ssize_t hdcp_ver_show(struct device *dev, struct device_attribute *attr,
			     char *buf);

void hdmitx_bootup_post_process(struct hdmitx_common *tx_comm);
/*  for plugin/plugout irq handler */
int hdmitx_hpd_plugin_handler(struct meson_tx_dev *tx_dev);
int hdmitx_hpd_plugout_handler(struct meson_tx_dev *tx_dev);

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
int hdmitx_common_get_mode_list(struct hdmitx_common *tx_comm, struct tx_timing **timing_list);
int hdmitx_common_get_vrr_mode_list(struct hdmitx_common *tx_comm,
	struct tx_timing *timing_list, int count, struct tx_timing **vrr_timing_list);
void get_hdmi_efuse(struct hdmitx_common *tx_comm);
enum hdmi_vic hdmitx_get_prefer_vic(struct hdmitx_common *tx_comm, enum hdmi_vic vic);
enum frl_rate_enum get_dsc_frl_rate(enum dsc_encode_mode dsc_mode);
enum hdmi_hdr_status hdmitx_common_get_hdr_status(struct hdmitx_common *tx_comm);
void hdmitx_get_qms_init_state(struct hdmitx_common *tx_common, u32 *brr, u32 *qms_en);
u32 hdmitx_common_get_vrr_cap(struct hdmitx_common *tx_comm);
bool hdmitx_common_get_sink_device_type(struct hdmitx_common *tx_comm);

int hdmitx_common_get_vrr_mode_group(struct hdmitx_common *tx_comm,
				     struct hdmitx_vrr_mode_group *group,
				     int max_group);
int hdmitx_common_set_vframe_rate_hint(struct hdmitx_common *tx_comm,
		struct meson_tx_state *tx_state, int rate, void *data);
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
unsigned int hdmitx_common_get_key_store(struct hdmitx_common *tx_comm);
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
