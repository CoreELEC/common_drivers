/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/amvecm/amvecm.h>

#ifndef AM_CSC_H
#define AM_CSC_H

extern uint debug_csc;
#define pr_csc(lvl, fmt, args...)\
	do {\
		if (debug_csc & (lvl))\
			pr_info("[%s](%d):" fmt, __func__, __LINE__, ## args);\
	} while (0)

#define ENABLE_PRINT 1
#if ENABLE_PRINT
#define pr_log(lvl, fmt, args...)\
	do {\
		if (debug_csc & (lvl))\
			pr_info("[%s](%d):" fmt, __func__, __LINE__, ## args);\
	} while (0)
#else
#define pr_log(lvl, fmt, args...)\
	do {} while (0)
#endif

/* white balance value */
void ve_ogo_param_update(void);
extern struct tcon_rgb_ogo_s video_rgb_ogo;

enum vpp_matrix_sel_e {
	VPP_MATRIX_0 = 0,	/* OSD convert matrix - new from GXL */
	VPP_MATRIX_1,		/* vd1 matrix before post-blend */
	VPP_MATRIX_2,		/* post matrix */
	VPP_MATRIX_3,		/* xvycc matrix */
	VPP_MATRIX_4,		/* in video eotf - new from GXL */
	VPP_MATRIX_5,		/* in osd eotf - new from GXL */
	VPP_MATRIX_6		/* vd2 matrix before pre-blend */
};

#define NUM_MATRIX 6

/* matrix names */
#define VPP_MATRIX_OSD		VPP_MATRIX_0
#define VPP_MATRIX_VD1		VPP_MATRIX_1
#define VPP_MATRIX_POST		VPP_MATRIX_2
#define VPP_MATRIX_XVYCC	VPP_MATRIX_3
#define VPP_MATRIX_EOTF		VPP_MATRIX_4
#define VPP_MATRIX_OSD_EOTF	VPP_MATRIX_5
#define VPP_MATRIX_VD2		VPP_MATRIX_6

/*	osd->eotf->matrix5->oetf->matrix0-+->post blend*/
/*		->vadj2->matrix2->eotf->matrix4->oetf->matrix3*/
/*	video1->cm->lut->vadj1->matrix1-^*/
/*			  video2->matrix6-^*/

#define CSC_ON              1
#define CSC_OFF             0

enum vpp_lut_sel_e {
	VPP_LUT_OSD_EOTF = 0,
	VPP_LUT_OSD_OETF,
	VPP_LUT_EOTF,
	VPP_LUT_OETF,
	VPP_LUT_INV_EOTF
};

#define NUM_LUT 5

/* matrix registers */
struct matrix_s {
	u16 pre_offset[3];
	u16 matrix[3][3];
	u16 offset[3];
	u16 right_shift;
};

enum mtx_en_e {
	POST_MTX_EN = 0,
	VD2_MTX_EN = 4,
	VD1_MTX_EN,
	XVY_MTX_EN,
	OSD1_MTX_EN
};

enum output_format_e {
	UNKNOWN_FMT = 0,
	BT709,
	BT709_HDR,
	BT2020,
	BT2020_PQ,
	BT2020_PQ_DYNAMIC,
	BT2020_HLG,
	BT2100_IPT,
	BT2020YUV_BT2020RGB_CUVA,
	BT2020_HLG_DYNAMIC = 10,
	/* force bypass all process */
	BT_BYPASS
};

struct hdr_cap_info_s {
	struct dv_info dv_info;
	u32 hdr_support;
	enum hdmi_color_depth cd;
	enum hdmi_colorspace cs;
};

enum cuva_mode_type_e {
	CUVA_SPECIAL_NO = 0,
	CUVA_MONITOR,
	CUVA_RECEIVER,
	CUVA_AUTO
};

#define POST_MTX_EN_MASK BIT(POST_MTX_EN)
#define VD2_MTX_EN_MASK  BIT(VD2_MTX_EN)
#define VD1_MTX_EN_MASK  BIT(VD1_MTX_EN)
#define XVY_MTX_EN_MASK  BIT(XVY_MTX_EN)
#define OSD1_MTX_EN_MASK BIT(OSD1_MTX_EN)

#define SDR_SUPPORT		(BIT(1))
#define HDR_SUPPORT		(BIT(2))
#define HLG_SUPPORT		(BIT(3))
#define HDRP_SUPPORT		(BIT(4))
#define BT2020_SUPPORT		(BIT(5))
#define DV_SUPPORT_SHF		(6)
#define DV_SUPPORT		(3 << DV_SUPPORT_SHF)
#define CUVA_SUPPORT		(BIT(8))
#define HLGP_SUPPORT		(BIT(9))

#define CUVA_IEEEOUI 0x047503

bool is_vinfo_available(const struct vinfo_s *vinfo);
int is_sink_cap_changed(const struct vinfo_s *vinfo,
			int *p_current_hdr_cap,
			int *p_current_sink_available,
			enum vpp_index_e vpp_index);
int is_video_turn_on(bool *vd_on, enum vd_path_e vd_path);

#define SIG_CS_CHG	0x01
#define SIG_SRC_CHG	0x02
#define SIG_PRI_INFO	0x04
#define SIG_KNEE_FACTOR	0x08
#define SIG_HDR_MODE	0x10
#define SIG_HDR_SUPPORT	0x20
#define SIG_WB_CHG	0x40
#define SIG_HLG_MODE	0x80
#define SIG_HLG_SUPPORT	0x100
#define SIG_OP_CHG	0x200
#define SIG_SRC_OUTPUT_CHG	0x400/*for box*/
#define SIG_HDR10_PLUS_MODE	0x800
#define SIG_COLORIMETRY_SUPPORT 0x1000
#define SIG_OUTPUT_MODE_CHG	0x2000
#define SIG_HDR_OOTF_CHG 0x4000
#define SIG_FORCE_CHG 0x8000
#define SIG_CUVA_HDR_MODE	0x40000
#define SIG_CUVA_HLG_MODE	0x80000

#define LUT_289_SIZE	289
extern unsigned int lut_289_mapping[LUT_289_SIZE];
extern int dnlp_en;
/*extern int cm_en;*/

extern unsigned int vecm_latch_flag;
extern signed int vd1_contrast_offset;
extern signed int saturation_offset;
extern uint sdr_mode;
extern uint hdr_mode;
extern uint hdr_flag;
extern int video_rgb_ogo_xvy_mtx_latch;
extern int video_rgb_ogo_xvy_mtx;
extern int tx_op_color_primary;
extern uint cur_csc_type[VD_PATH_MAX];
extern struct hdr_cap_info_s hdr_cap_info;

/* 0: source: use src meta */
/* 1: Auto: 601/709=709 P3/2020=P3 */
/* 2: Native: 601/709=off P3/2020=2020 */
#define PRIMARIES_SOURCE	0
#define PRIMARIES_AUTO		1
#define PRIMARIES_NATIVE	2

int get_hdr_policy(void);
int get_primary_policy(void);
void set_hdr_policy(int policy);
void set_cur_hdr_policy(uint policy);
enum output_format_e get_force_output(void);
void set_force_output(enum output_format_e output);
void set_vout2_change(unsigned int flag);
unsigned int get_vout2_change(void);

enum cuva_mode_type_e get_cuva_mode(void);
enum cuva_mode_type_e get_tx_cuva_mode(struct vinfo_s *vinfo);

/* 0: hdr->hdr, 1:hdr->sdr, 2:hdr->hlg */
extern uint hdr_process_mode[VD_PATH_MAX];
extern uint cur_hdr_process_mode[VD_PATH_MAX];

/* 0: bypass, 1:hdr10p->hdr, 2:hdr10p->sdr, 3:hdr10p->hlg */
extern uint hdr10_plus_process_mode[VD_PATH_MAX];
extern uint cur_hdr10_plus_process_mode[VD_PATH_MAX];

/* 0: bypass, 1:cuva_hdr->hdr, 2:cuva_hdr->sdr, 3:cuva_hdr->hlg */
extern uint cuva_hdr_process_mode[VD_PATH_MAX];
extern uint cur_cuva_hdr_process_mode[VD_PATH_MAX];

/* 0: bypass, 1:cuva_hdr->hdr, 2:cuva_hdr->sdr, 3:cuva_hdr->hlg */
extern uint cuva_hlg_process_mode[VD_PATH_MAX];
extern uint cur_cuva_hlg_process_mode[VD_PATH_MAX];

/* 0: hlg->hlg, 1:hlg->sdr 2:hlg->hdr*/
extern uint hlg_process_mode[VD_PATH_MAX];
extern uint cur_hlg_process_mode[VD_PATH_MAX];

/* 0: bypass, 1:hlgp->hlg, 2:hlgp->sdr, 3:hlgp->hdr */
extern uint hlg_plus_process_mode[VD_PATH_MAX];
extern uint cur_hlg_plus_process_mode[VD_PATH_MAX];

/* 0: sdr->sdr, 1:sdr->hdr, 2:sdr->hlg */
extern uint sdr_process_mode[VD_PATH_MAX];
extern uint cur_sdr_process_mode[VD_PATH_MAX];

/* 0: tx don't support hdr10+, 1: tx support hdr10+*/
extern uint tx_hdr10_plus_support;

extern struct master_display_info_s dbg_hdr_send;

/* 0: not cuva spicial mode, 1:monitor, 2:receiver, 3:auto */
extern unsigned int tx_cuva_mode;
extern unsigned int cuva_dbg_mode; /* only for debug tx_cuva_mode*/

extern unsigned int fake_frame_flag[VD_PATH_MAX];

int amvecm_matrix_process(struct vframe_s *vf, struct vframe_s *vf_rpt, int flags,
			  enum vd_path_e vd_path, enum vpp_index_e vpp_index);
int amvecm_hdr_dbg(u32 sel);
u32 get_video_enabled(u8 layer_id);
void set_video_mute(u32 owner, bool on);
int get_video_mute(void);

void get_cur_vd_signal_type(enum vd_path_e vd_path);
enum color_primary_e get_color_primary(void);

unsigned int get_cur_vd_ext_signal_type(enum vd_path_e vd_path);
enum vpp_matrix_csc_e get_csc_type(void);
enum vpp_matrix_ext_csc_e get_ext_csc_type(void);
int is_hdmi_source(enum vd_path_e vd_path);
int get_vf_source_type(enum vd_path_e vd_path);

/*hdr*/
/*#define DBG_BUF_SIZE (1024)*/

struct hdr_cfg_t {
	unsigned int en_osd_lut_100;
};

struct hdr_data_t {
	struct hdr_cfg_t hdr_cfg;

	/*debug_fs*/
	struct dentry *dbg_root;
	/*char dbg_buf[DBG_BUF_SIZE];*/

};

void hdr_init(struct hdr_data_t *phdr_data);
void hdr_exit(void);
void hdr_set_cfg_osd_100(int val);
void hdr_osd_off(enum vpp_index_e vpp_index);
void hdr_vd1_off(enum vpp_index_e vpp_index);
void hdr_vd2_off(enum vpp_index_e vpp_index);
void hdr_vd1_iptmap(enum vpp_index_e vpp_index);
bool is_video_layer_on(enum vd_path_e vd_path);

#define HDR_MODULE_OFF		0
#define HDR_MODULE_ON		1
#define HDR_MODULE_BYPASS	2
void set_hdr_module_status(enum vd_path_e vd_path, int status);
int get_hdr_module_status(enum vd_path_e vd_path, enum vpp_index_e vpp_index);
int get_primaries_type(struct vframe_master_display_colour_s *p_mdc);

#define PROC_BYPASS			0
/* to backward compatible */
#define PROC_MATCH			9
#define PROC_OFF			8
/* sdr */
#define PROC_SDR_TO_HDR		1
#define PROC_SDR_TO_HLG		2
#define PROC_SDR_TO_CUVA	3
#define PROC_SDR_TO_TRG		4
/*AI Color enhance*/
#define PROC_SDR_AC_SDR		5

/* hdr */
#define PROC_HDR_TO_SDR		1
#define PROC_HDR_TO_HLG		2
#define PROC_HDR_TO_CUVA	3

/* hlg */
#define PROC_HLG_TO_SDR		1
#define PROC_HLG_TO_HDR		2
#define PROC_HLG_TO_CUVA	3

/* hdr+ */
#define PROC_HDRP_TO_HDR	1
#define PROC_HDRP_TO_SDR	2
#define PROC_HDRP_TO_HLG	3
#define PROC_HDRP_TO_CUVA	4

/* hlg+ */
#define PROC_HLGP_TO_HLG	1
#define PROC_HLGP_TO_SDR	2
#define PROC_HLGP_TO_HDR	3
#define PROC_HLGP_TO_CUVA	4

/* cuva_hdr */
#define PROC_CUVA_TO_SDR	1
#define PROC_CUVA_TO_HDR	2
#define PROC_CUVA_TO_HLG	3
#define PROC_CUVA_TO_HDR10P	4

/* cuva_hlg */
#define PROC_CUVAHLG_TO_HDR	1
#define PROC_CUVAHLG_TO_HLG	2
#define PROC_CUVAHLG_TO_SDR	3
#define PROC_CUVAHLG_TO_CUVA	4

uint get_hdr10_plus_pkt_delay(void);
void update_hdr10_plus_pkt(bool enable,
			   void *hdr10plus_params,
			   void *send_info,
			   enum vpp_index_e vpp_index);
void send_hdr10_plus_pkt(enum vd_path_e vd_path, enum vpp_index_e vpp_index);
void send_cuva_pkt(enum vd_path_e vd_path, enum vpp_index_e vpp_index);

#define HDRPLUS_PKT_UPDATE	2
#define HDRPLUS_PKT_REPEAT	1
#define HDRPLUS_PKT_IDLE	0

uint get_cuva_pkt_delay(void);
void update_cuva_pkt(bool enable,
	void *cuva_params,
	void *edms_params,
	void *send_info,
	enum vpp_index_e vpp_index);

#define CUVA_PKT_UPDATE	2
#define CUVA_PKT_REPEAT	1
#define CUVA_PKT_IDLE	0
void hdr10_plus_process_update(int force_source_lumin, enum vd_path_e vd_path,
		enum vpp_index_e vpp_index);
void hlg_plus_process_update(int force_source_lumin, enum vd_path_e vd_path,
		enum vpp_index_e vpp_index);
extern int customer_hdr_clipping;

/* api to get sink capability */
uint32_t sink_dv_support(const struct vinfo_s *vinfo);
uint32_t sink_hdr_support(const struct vinfo_s *vinfo);
uint32_t sink_hdr_support_ori_cap(const struct vinfo_s *vinfo);

extern uint hdr_policy;
extern uint primary_policy;
extern uint force_output;
extern unsigned int hdr_policy_bak;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
extern uint hdr_hist_select;
extern uint osd_gamut_conv_type;
extern uint gamut_conv_enable;
extern uint force_csc_type;
extern uint range_control;
extern uint rdma_flag;
extern uint csc_en;
extern int print_lut_mtx;
extern int lut_289_en;
extern int knee_factor;
extern int knee_interpolation_mode;
extern int force_customer_panel_lumin;
extern int customer_panel_lumin;
extern int customer_hdr_clipping;
extern int video_lut_switch;
extern unsigned int reload_lut;
extern uint cuva_mode;
#endif

void force_toggle(void);
int get_s5_slice_mode(void);

#define VD1_1SLICE							1
#define VD1_2SLICE							2
#define VD1_4SLICE							4

void pkt_delay_flag_init(void);

void get_source_csc_info(int vpp_index, int *source_type, int *csc_type);
void amvecm_osd_matrix_process(enum vd_path_e vd_path,
	enum vpp_index_e vpp_index);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
extern unsigned int pre_dpss_mode;

int signal_type_detect_for_dpss(struct vframe_s *vf);
void hdr_process_for_dpss(struct vframe_s *vf);
void calculate_dynamic_curve_for_dpss(struct vframe_s *vf);
void update_hdr_path_for_dpss(struct vframe_s *vf);
void hdr_init_for_dpss(struct vframe_s *vf);
void set_dpss_mode(unsigned int val);
void set_sdr_ext_mode_for_dpss(unsigned int val);
void set_muxio_link_mode(unsigned int link_flag,
	struct vframe_s *vf, enum vpp_index_e vpp_index);
void update_muxio_mode(struct vframe_s *vf,
	enum vpp_index_e vpp_index);
unsigned int get_muxio_ready_for_dpss(void);
void set_dct_status_for_dpss(unsigned int val);
void set_lc_evc_ctrl_for_dpss(unsigned int enable, unsigned int lc_evc_src);

struct gamut_mapping_s {
	unsigned int eotf_en;
	unsigned int oetf_en;
	unsigned int ootf_en;
	unsigned int mtx_en;
	unsigned int gmt0_after_osd; // 0: after VADJ1, 1: after OSD
	unsigned int gmt1_3dlut_inside ; // 1: lut3d inside gmt
	unsigned int flag_update_lut;
	int eotf_lut[143];
	int oetf_lut[144];
	int ootf_lut[144];
	unsigned int flag_update_mtx;
	int mtx[3][3];
	int mtx_pre_offset[3];
	int mtx_pos_offset[3];
	int mtx_rs;
};

extern struct gamut_mapping_s gamut_mapping0_param;
extern struct gamut_mapping_s gamut_mapping1_param;
extern uint gamut_mapping0_en;
extern uint gamut_mapping1_en;
extern int support_gmt_wrapper;

void gamut_mapping_wrapper_init(void);
void set_gamut_mapping_wrapper(int module, int vpp_index);
void get_gamut_mapping_wrapper(void);

void set_muxio_link_mode(unsigned int link_flag,
	struct vframe_s *vf, enum vpp_index_e vpp_index);
void update_muxio_mode(struct vframe_s *vf,
	enum vpp_index_e vpp_index);
unsigned int get_muxio_ready_for_dpss(void);
void update_hdr_settings_dpss(struct vframe_s *vf,
	enum vd_path_e vd_path, enum vpp_index_e vpp_index);
enum hdr_module_sel get_hdr_module(enum vd_path_e vd_path);
void video_post_process_for_dpss(struct vframe_s *vf,
				enum vd_path_e vd_path, enum vpp_index_e vpp_index);

enum path_mux_e {
	PATH_DELINK = 0,
	PATH_VD1 = 1,
	PATH_DPSS,
};

enum frm_src_e {
	NULL_FRM = 0,
	VD1_FRM,
	DPSS_FRM
};

enum dh_proc_e {
	NO_PROC = 0,
	D_DD,
	D_HDR
};

enum dl_st_e {
	LINK_ON = 0,
	DELINK,
	LINK_DPSS
};

enum frm_src_type_e {
	SRC_NULL = 0,
	SRC_HDR,
	SRC_AMDV
};

struct hdr_path_mux_sel_s {
	enum path_mux_e path_mux;
	enum path_mux_e pre_path_mux;
	unsigned int delink_status;
	unsigned int mute_cnt;
	unsigned int min_mc;

	enum frm_src_e frm_src;
	enum dh_proc_e dh_p;

	unsigned int fst_frame;
	enum frm_src_type_e pre_frm_type;
};

extern const char *pm_str[3];
extern struct hdr_path_mux_sel_s h_p_s;
void vd1_dpss_switch_proc(struct vframe_s *vf,
	struct vframe_s *rpt_vf, enum vpp_index_e vpp_index);
void update_link_state(struct vframe_s *vf,
	struct vframe_s *rpt_vf,
	enum vpp_index_e vpp_index);
u32 get_cur_signal_type_limited(void);
void reset_hdr_path_cfg(void);
void fake_frame_set_vd1_delink_flag(unsigned int en);
extern int gamut_dma_case;
extern int hdr_hist_test_module;
extern int hdr_hist_test_cnt;
void hdr_hist_test(enum vd_path_e vd_path);
unsigned int get_cur_signal_type(void);

enum src_primary_type_e {
	SRC_PRIMARY_BT709 = 0,
	SRC_PRIMARY_BT2020,
	SRC_PRIMARY_P3,
	SRC_PRIMARY_ADOBE_RGB,
};

enum src_primary_type_e get_src_primary_type(void);
#endif
#endif /* AM_CSC_H */

