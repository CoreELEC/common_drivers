/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef AMVECM_H
#define AMVECM_H

//#define T7_BRINGUP_MULTI_VPP

#include "linux/amlogic/media/amvecm/ve.h"
#include "linux/amlogic/media/amvecm/cm.h"
#include "linux/amlogic/media/amvecm/hdr10_tmo_alg.h"
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/utils/amstream.h>
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/video_sink/vpp.h>
#include <drm/drmP.h>
#include <uapi/amlogic/amvecm_ext.h>

#ifndef MAX
#define MAX(a, b) ({ \
	typeof(a) _a = a; \
	typeof(b) _b = b; \
	_a > _b ? _a : _b; \
	})
#endif // MAX

#ifndef MIN
#define MIN(c, d) ({ \
	typeof(c) _c = c; \
	typeof(d) _d = d; \
	_c < _d ? _c : _d; \
	})
#endif // MIN

#ifndef FMETER_SCORE
#define FMETER_SCORE(x, y, z) ({ \
			typeof(x) _x = x; \
			typeof(y) _y = y; \
			typeof(z) _z = z; \
			MAX(MIN(100, (_x * 1000) / (_x + _y + _z)), \
			MIN(100, (_x + _y) * 1000 / \
			(_x + _y + _z) / 3)); \
			})
#endif // FMETER_SCORE

#define FMETER_VERSION "v2025.02.26.1(minimum ko version: v2025.02.26.1)"

/* struct ve_dnlp_s          video_ve_dnlp; */
#define FLAG_COLORPRI_LATCH     BIT(31)
#define FLAG_VADJ1_COLOR        BIT(30)
#define FLAG_VE_DNLP            BIT(29)
#define FLAG_VE_NEW_DNLP        BIT(28)
#define FLAG_VE_LC_CURV         BIT(27)
#define FLAG_HDR_OOTF_LATCH     BIT(26)
#define FLAG_3D_BLACK_DIS       BIT(25)
#define FLAG_3D_BLACK_EN        BIT(24)
#define FLAG_3D_SYNC_DIS        BIT(23)
#define FLAG_3D_SYNC_EN         BIT(22)
#define FLAG_VLOCK_DIS          BIT(21)
#define FLAG_VLOCK_EN          BIT(20)
#define FLAG_VE_DNLP_EN         BIT(19)
#define FLAG_VE_DNLP_DIS        BIT(18)
#define FLAG_VADJ1_CON			BIT(17)
#define FLAG_VADJ1_BRI			BIT(16)
#define FLAG_GAMMA_TABLE_EN     BIT(15)
#define FLAG_GAMMA_TABLE_DIS    BIT(14)
#define FLAG_GAMMA_TABLE_R      BIT(13)
#define FLAG_GAMMA_TABLE_G      BIT(12)
#define FLAG_GAMMA_TABLE_B      BIT(11)
#define FLAG_RGB_OGO            BIT(10)
#define FLAG_VADJ_EN            BIT(9)
#define FLAG_MATRIX_UPDATE      BIT(8)
#define FLAG_BRI_CON            BIT(7)
#define FLAG_LVDS_FREQ_SW       BIT(6)
#define FLAG_REG_MAP5           BIT(5)
#define FLAG_REG_MAP4           BIT(4)
#define FLAG_REG_MAP3           BIT(3)
#define FLAG_REG_MAP2           BIT(2)
#define FLAG_REG_MAP1           BIT(1)
#define FLAG_REG_MAP0           BIT(0)

/*
 *#define VPP_VADJ2_BLMINUS_EN        (1 << 3)
 *#define VPP_VADJ2_EN                (1 << 2)
 *#define VPP_VADJ1_BLMINUS_EN        (1 << 1)
 *#define VPP_VADJ1_EN                (1 << 0)
 */
#define LUT3D_BASE_UPDATE           BIT(31)

#define VPP_MARTIX_UPDATE_BRIGHT    BIT(30)
#define VPP_MARTIX_UPDATE_CONTRAST  BIT(29)
#define VPP_MARTIX_UPDATE_HUE_SAT   BIT(28)
#define VPP_MARTIX_UPDATE_EN        BIT(27)

#define FLAG_HDR10P_ON              BIT(26)
#define FLAG_HDR_ON                 BIT(25)
#define FLAG_GAMUT_MAPPING1_UPDATE  BIT(24)
#define FLAG_GAMUT_MAPPING0_UPDATE  BIT(23)
#define FLAG_PRE_SAT_UPDATE         BIT(22)
#define FLAG_RESUME_RECOVERY        BIT(21)
#define SHARPNESS_GAIN_UPDATE       BIT(20)
#define FLAG_GAMMA_TABLE_EN_SUB     BIT(19)
#define FLAG_GAMMA_TABLE_DIS_SUB    BIT(18)
#define FLAG_GAMMA_TABLE_R_SUB      BIT(17)
#define FLAG_GAMMA_TABLE_G_SUB      BIT(16)
#define FLAG_GAMMA_TABLE_B_SUB      BIT(15)

#define LUT3D_UPDATE                BIT(14)
#define BLE_WHE_UPDATE              BIT(13)
#define GAMMA_CRC_FAIL              BIT(12)
#define GAMMA_CRC_PASS              BIT(11)
#define GAMMA_READ_B                BIT(10)
#define GAMMA_READ_G                BIT(9)
#define GAMMA_READ_R                BIT(8)
#define VPP_EYE_PROTECT_UPDATE      BIT(7)
#define VPP_PRE_GAMMA_UPDATE        BIT(6)
#define VPP_MARTIX_GET              BIT(5)
#define VPP_MARTIX_UPDATE           BIT(4)
#define VPP_DEMO_DNLP_DIS           BIT(3)
#define VPP_DEMO_DNLP_EN            BIT(2)
#define VPP_DEMO_CM_DIS             BIT(1)
#define VPP_DEMO_CM_EN              BIT(0)

/*PQ USER LATCH*/
#define PQ_USER_PQ_MODULE_CTL      BIT(26)
#define PQ_USER_OVERSCAN_RESET     BIT(25)
#define PQ_USER_CMS_SAT_HUE        BIT(24)
#define PQ_USER_CMS_CURVE_HUE_HS   BIT(23)
#define PQ_USER_CMS_CURVE_HUE      BIT(22)
#define PQ_USER_CMS_CURVE_LUMA     BIT(21)
#define PQ_USER_CMS_CURVE_SAT      BIT(20)
#define PQ_USER_SR1_DIRECTION_DIS  BIT(19)
#define PQ_USER_SR1_DIRECTION_EN   BIT(18)
#define PQ_USER_SR0_DIRECTION_DIS  BIT(17)
#define PQ_USER_SR0_DIRECTION_EN   BIT(16)
#define PQ_USER_SR1_DEJAGGY_DIS    BIT(15)
#define PQ_USER_SR1_DEJAGGY_EN     BIT(14)
#define PQ_USER_SR0_DEJAGGY_DIS    BIT(13)
#define PQ_USER_SR0_DEJAGGY_EN     BIT(12)
#define PQ_USER_SR1_DERING_DIS     BIT(11)
#define PQ_USER_SR1_DERING_EN      BIT(10)
#define PQ_USER_SR0_DERING_DIS     BIT(9)
#define PQ_USER_SR0_DERING_EN      BIT(8)
#define PQ_USER_SR1_PK_DIS         BIT(7)
#define PQ_USER_SR1_PK_EN          BIT(6)
#define PQ_USER_SR0_PK_DIS         BIT(5)
#define PQ_USER_SR0_PK_EN          BIT(4)
#define PQ_USER_BLK_SLOPE          BIT(3)
#define PQ_USER_BLK_START          BIT(2)
#define PQ_USER_BLK_DIS            BIT(1)
#define PQ_USER_BLK_EN             BIT(0)

/*white balance latch*/
#define MTX_BYPASS_RGB_OGO			BIT(0)
#define MTX_RGB2YUVL_RGB_OGO		BIT(1)

#define DNLP_PARAM_RD_UPDATE 0x1
#define DNLP_CV_RD_UPDATE 0x2
#define WB_PARAM_RD_UPDATE 0x4
#define LC_CUR_RD_UPDATE 0x8
#define LC_PARAM_RD_UPDATE 0x10
#define LC_CUR2_RD_UPDATE 0x20

#define BLK_ADJ_EN        0x40
#define BLK_END           0x80
#define BLK_SLP           0x100
#define BRT_ADJ_EN        0x200
#define BRT_START         0x400
#define BRT_SLP           0x800

#define CM_SAT_DEBUG_FLAG 0x1
#define CM_HUE_DEBUG_FLAG 0x2
#define CM_LUMA_DEBUG_FLAG 0x4
#define CM_HUE_BY_HIS_DEBUG_FLAG 0x8

#define CSC_FLAG_TOGGLE_FRAME	1
#define CSC_FLAG_CHECK_OUTPUT	2
#define CSC_FLAG_FORCE_SIGNAL	4
#define CSC_FLAG_FORCE_PACKET	8

/*hdr output mode*/
#define HDR_OUTPUT_MODE_DOLBY_VISION			0
#define HDR_OUTPUT_MODE_HDR10					1
#define HDR_OUTPUT_MODE_HLG						2
#define HDR_OUTPUT_MODE_HDR10PLUS				3
#define HDR_OUTPUT_MODE_CUVA_HDR				4
#define HDR_OUTPUT_MODE_CUVA_HLG				5
#define HDR_OUTPUT_MODE_SDR						6
#define HDR_OUTPUT_MODE_BYPASS					7
#define LC_EVC_SIZE  5

//48-56hz gm_tb[1][3]
//57-64hz gm_tb[2][3]
//65-72hz gm_tb[3][3]
//73-80hz gm_tb[4][3]
//81-88hz gm_tb[5][3]
//89-96hz gm_tb[6][3]
//97-104hz gm_tb[7][3]
//105-112hz gm_tb[8][3]
//112-120hz gm_tb[9][3]
//121-144hz gm_tb[10][3]

enum cm_hist_e {
	CM_HUE_HIST = 0,
	CM_SAT_HIST,
	CM_MAX_HIST
};

enum dv_pq_ctl_e {
	DV_PQ_TV_BYPASS = 0,
	DV_PQ_STB_BYPASS,
	DV_PQ_CERT,
	DV_PQ_REC,
	DV_PQ_EP_IPT,
	DV_PQ_EP_YUV,
};

enum wr_md_e {
	WR_VCB = 0,
	WR_DMA
};

enum rw_md_e {
	RD_MOD = 1,
	WR_MOD
};

enum ep_mode_e {
	YUV_MOD = 0,
	IPT_MOD
};

struct vpp_hist_param_s *get_vpp_hist(void);

struct ve_pq_table_s {
	unsigned int src_timing;
	unsigned int value1;
	unsigned int value2;
	unsigned int reserved1;
	unsigned int reserved2;
};

struct pre_sat_data_s {
	unsigned int enable;
	unsigned int gain;
};

/* CMS ioctl data structure */
struct cms_data_s {
	int color;
	int value;
};

enum pd_comb_fix_lvl_e {
	PD_LOW_LVL = 0,
	PD_MID_LVL,
	PD_HIG_LVL,
	PD_DEF_LVL
};

enum vpp_transfer_characteristic_e {
	VPP_ST_NULL = 0,
	VPP_ST709 = 0x1,
	VPP_ST2084 = 0x2,
	VPP_ST2094_40 = 0x4,
};

/*pq_timing:
 *SD/HD/FHD/UHD for DTV/MEPG,
 *NTST_M/NTST_443/PAL_I/PAL_M/PAL_60/PAL_CN/SECAM/NTST_50 for AV/ATV
 */
enum ve_pq_timing_e {
	TIMING_SD_480 = 0,
	TIMING_SD_576,
	TIMING_HD,
	TIMING_FHD,
	TIMING_UHD,
	TIMING_NTST_M,
	TIMING_NTST_443,
	TIMING_PAL_I,
	TIMING_PAL_M,
	TIMING_PAL_60,
	TIMING_PAL_CN,
	TIMING_SECAM,
	TIMING_NTSC_50,
	TIMING_MAX,
};

/*overscan:
 *length 0~31bit :number of crop;
 *src_timing: bit31: on: load/save all crop
			  bit31: off: load one according to timing*
			  bit30: AFD_enable: 1 -> on; 0 -> off*
			  screen mode: bit24~bit29*
			  source: bit16~bit23 -> source*
			  timing: bit0~bit15 -> sd/hd/fhd/uhd*
 *value1: 0~15bit hs   16~31bit he*
 *value2: 0~15bit vs   16~31bit ve*
 */
struct ve_pq_overscan_s {
	unsigned int load_flag;
	unsigned int afd_enable;
	unsigned int screen_mode;
	enum ve_source_input_e source;
	enum ve_pq_timing_e timing;
	unsigned int hs;
	unsigned int he;
	unsigned int vs;
	unsigned int ve;
};

extern struct ve_pq_overscan_s overscan_table[TIMING_MAX];

/*3D LUT IOCTL command list*/
struct table_3dlut_s {
	unsigned int data[17 * 17 * 17][3];
} /*table_3dlut_s */;

enum vlk_chiptype {
	vlock_chip_null,
	vlock_chip_txl,
	vlock_chip_txlx,
	vlock_chip_txhd,
	vlock_chip_tl1,
	vlock_chip_tm2,
	vlock_chip_sm1,
	vlock_chip_t5,/*same as t5d/t5w*/
	vlock_chip_t7,
	vlock_chip_t3,
	vlock_chip_t5m,
	vlock_chip_t3x,
};

enum chip_type {
	chip_other = 0,
	chip_t7,
	chip_t3,
	chip_t5w,
	chip_t5m,
	chip_s5,
	chip_t3x,
	chip_txhd2,
	chip_s1a,
	chip_s7,
	chip_a4,
	chip_sc2,
	chip_s7d,
	chip_s6,
	chip_t6d,
	chip_t6w,
	chip_t6x,
	chip_a9,
	chip_c5
};

enum chip_cls_e {
	OTHER_CLS = 0,
	TV_CHIP,
	STB_CHIP,
	SMT_CHIP,
	AD_CHIP
};

enum vlock_hw_ver_e {
	/*gxtvbb*/
	vlock_hw_org = 0,
	/*
	 *txl
	 *txlx
	 */
	vlock_hw_ver1,
	/* tl1 later
	 * fix bug:i problem
	 * fix bug:affect ss function
	 * add: phase lock
	 * tm2: have separate pll:tcon pll and hdmitx pll
	 */
	vlock_hw_ver2,
	/* tm2 verion B
	 * fix some bug
	 */
	vlock_hw_tm2verb,
};

struct vecm_match_data_s {
	enum chip_type chip_id;
	enum chip_cls_e chip_cls;
	enum vlk_chiptype vlk_chip;
	u32 vlk_support;
	u32 vlk_new_fsm;
	enum vlock_hw_ver_e vlk_hwver;
	u32 vlk_phlock_en;
	u32 vlk_pll_sel;/*independent panel pll and hdmitx pll*/
	u32 reg_addr_vlock;
	u32 reg_addr_hiu;
	u32 reg_addr_anactr;
	u32 vlk_ctl_for_frc;/*control frc flash patch*/
	u32 vrr_support_flag;/*frame lock control type 1:support 0:unsupport*/
};

enum vd_path_e {
	VD1_PATH = 0,
	VD2_PATH = 1,
	VD3_PATH = 2,
	VD1_1_PATH = 3,
	VD2_1_PATH = 4,
	VD_PATH_MAX = 5
};

enum vpp_index_e {
	VPP_TOP0 = 0,
	VPP_TOP1 = 1,
	VPP_TOP2 = 2,
	VPP_PRE_VS = 3,
	VPP_DPSS = 4,
	VPP_TOP_MAX_S = 5,
	VPP_VCBUS = 0xff,
};

enum vpp_slice_e {
	SLICE0 = 0,
	SLICE1,
	SLICE2,
	SLICE3,
	SLICE_MAX
};

enum vadj_index_e {
	VE_VADJ1 = 0,
	VE_VADJ2
};

enum pq_module_e {
	pq_module_vpp_pq = 0,
	pq_module_dnlp,
	pq_module_cm,
	pq_module_wb,
	pq_module_pre_gamma,
	pq_module_gamma,/*5*/
	pq_module_lc,
	pq_module_black_ext,
	pq_module_chroma_cor,
	pq_module_dither,
	pq_module_3dlut,/*10*/
	pq_module_vadj1,
	pq_module_vadj2,
	pq_module_sharpness,
	pq_module_sr_peaking,
	pq_module_sr_lcti,/*15*/
	pq_module_sr_theta,
	pq_module_sr_deband,
	pq_module_sr_dejaggy,
	pq_module_sr_dering,
	pq_module_sr_drlpf,/*20*/
	pq_module_module_max,
};

enum pq_ctl_cfg_e {
	TV_CFG_DEF = 0,
	OTT_CFG_DEF,
	TV_DV_BYPASS,
	OTT_DV_BYPASS,
	INIT_CUR_CFG,
	PQ_CFG_MAX
};

/*flag:
 *bit 0: brigtness
 *bit 1: contrast
 *bit 2: saturation
 *bit 3: hue
 */
struct vdj_parm_s {
	int flag;
	int brightness;
	int contrast;
	int sat_hue;
};

extern signed int vd1_brightness, vd1_contrast;
extern int gamma_en;
extern unsigned int atv_source_flg;
extern enum hdr_type_e hdr_source_type;
extern unsigned int pd_detect_en;
extern int wb_en;
extern struct pq_ctrl_s pq_cfg_cur;
extern struct pq_ctrl_s pq_cfg_init[PQ_CFG_MAX];

extern struct pq_ctrl_s pq_cfg;
extern struct pq_ctrl_s dv_cfg_bypass;
extern unsigned int lc_offset;
extern unsigned int pq_user_latch_flag;
extern struct eye_protect_s eye_protect;
extern unsigned int vecm_latch_flag2;

extern enum ecm_color_type cm_cur_work_color_md;
extern int cm2_debug;
extern int bs_3dlut_en;
extern unsigned int vecm_latch_flag2;

extern unsigned int ct_en;
void bs_ct_latch(void);

extern enum chip_type chip_type_id;
extern enum chip_cls_e chip_cls_id;

extern enum output_format_e output_format;

extern unsigned int osd_pic_en;
extern unsigned int slt_en;
extern bool pq_rdma_init;
extern int hdr_tool_matrix_mode;

struct vpq_size_s {
	unsigned int sr1_in_hsize;
	unsigned int sr1_in_vsize;
	unsigned int sr1_hsc_en;
	unsigned int sr1_vsc_en;
	unsigned int cm_hsize;//scaler out hsize
	unsigned int cm_vsize;//scaler out vsize
};

void amvecm_size_info_update(int vpp_index);
extern int flag_cuva_hdr;
extern int sel_val;

int amvecm_on_vs(struct vframe_s *display_vf,
		 struct vframe_s *toggle_vf,
		 int flags,
		 struct vpq_size_s vpq_size,
		 enum vd_path_e vd_path,
		 enum vpp_index_e vpp_index);
void refresh_on_vs(struct vframe_s *vf, struct vframe_s *rpt_vf, u32 vpp_index);
void pc_mode_process(int vpp_index);
void pq_user_latch_process(int vpp_index);
void vlock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts);
void frame_lock_process(struct vframe_s *vf,
		   struct vpp_frame_par_s *cur_video_sts, u16 line);
int frc_input_handle(struct vframe_s *vf, struct vpp_frame_par_s *cur_video_sts);
void get_hdr_process_name(int id, char *name, char *output_fmt);

void vpp_vd_adj1_saturation_hue(signed int sat_val,
				signed int hue_val, struct vframe_s *vf, int vpp_index);
void amvecm_sharpness_enable(int sel);
int metadata_read_u32(uint32_t *value);
int metadata_wait(struct vframe_s *vf);
int metadata_sync(u32 frame_id, uint64_t pts);
void amvecm_wakeup_queue(void);

int amvecm_drm_get_gamma_size(u32 index);
void amvecm_drm_init(u32 index);
int amvecm_drm_gamma_set(u32 index,
			 struct drm_color_lut *lut, int lut_size);
int amvecm_drm_gamma_get(u32 index, u16 *red, u16 *green, u16 *blue);
int amvecm_drm_gamma_enable(u32 index);
int amvecm_drm_gamma_disable(u32 index);
int am_meson_ctm_set(u32 index, struct drm_color_ctm *ctm);
int am_meson_ctm_disable(void);

int get_hdr_cur_output(void);
int get_hdr_conversion_cap(void);
void set_hdr_output(int out);

void enable_osd1_mtx(unsigned int en);
void set_cur_hdr_policy(uint policy);
void set_vout2_change(unsigned int flag);
bool di_api_mov_sel(unsigned int mode,
		    unsigned int *pdate);
enum hdr_type_e get_cur_source_type(enum vd_path_e vd_path,
	enum vpp_index_e vpp_index, struct vframe_s *vf);

int amvecm_set_saturation_hue(int mab, enum wr_md_e mode, int vpp_index);
void amvecm_saturation_hue_update(int offset_val);
void amvecm_update_module_status(void);
void vpp_vadj1_align_vd1_mute(void);

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
int frc_set_seg_display(u8 enable, u8 seg1, u8 seg2, u8 seg3);
#endif

/*ai detected scenes*/
enum detect_scene_e {
	BLUE_SCENE = 0,
	GREEN_SCENE,
	SKIN_TONE_SCENE,
	PEAKING_SCENE,
	SATURATION_SCENE,
	DYNAMIC_CONTRAST_SCENE,
	NOISE_SCENE,
	SCENE_MAX
};

/*detected single scene process*/
struct single_scene_s {
	int enable;
	int (*func)(int offset, int enable);
};
extern struct single_scene_s detected_scenes[SCENE_MAX];
extern int freerun_en;
u32 hdr_set(u32 module_sel, u32 hdr_process_select, enum vpp_index_e vpp_index);
int vinfo_lcd_support(void);
int vinfo_hdmi_out_fmt(void);
int dv_pq_ctl(enum dv_pq_ctl_e ctl);
int cm_force_update_flag(void);
int get_lum_ave(void);
extern int flag_lc_evc;
extern int flag_lc_evc_src;
int get_ep_ipt_flag(void);

enum demo_module_e {
	E_DEMO_SR = 0,/*SHARPNESS/DEJAGGY/DNLP/LC*/
	E_DEMO_CM,
	E_DEMO_LC,
	E_DEMO_MAX
};

struct demo_size_s {
	int sr_hsize;
	int sr_vsize;
	int cm_hsize;
	int cm_vsize;
};

enum enable_state_e {
	E_DISABLE = 0,
	E_ENABLE
};

struct demo_data_s {
	int update_flag[E_DEMO_MAX];
	enum demo_module_e mod[E_DEMO_MAX];
	struct demo_size_s in_size;
	enum enable_state_e en_st[E_DEMO_MAX];
};

struct venc_gamma_table_s {
	u16 gamma_r[257];
	u16 gamma_g[257];
	u16 gamma_b[257];
};

struct gamma_data_s {
	int max_idx;
	unsigned int auto_inc;
	int addr_port;
	int data_port;
	struct venc_gamma_table_s gm_tbl;
	struct venc_gamma_table_s dbg_gm_tbl;
};

struct fmeter_data_s {
	u32 fmeter0_hcnt[4];
	u32 fmeter0_vcnt[4];
	u32 fmeter0_pdcnt[4];
	u32 fmeter0_ndcnt[4];
	u32 fmeter1_hcnt[4];
	u32 fmeter1_vcnt[4];
	u32 fmeter1_pdcnt[4];
	u32 fmeter1_ndcnt[4];
};

extern struct fmeter_data_s data_meter;

struct gamma_data_s *get_gm_data(void);
void bs_ct_latch(void);
int pkt_adv_chip(void);
extern unsigned int ai_color_enable;
void amve_lc_evc_ctrl(unsigned int enable, unsigned int lc_evc_src);
void lcd_post2_matrix(void);

int register_osd_status_cb(int (*get_osd_enable_status)(u32 index));
void resume_recovery_process(int vpp_index);
extern uint demo_pk_sr_final_pgains;
extern uint demo_pk_sr_final_ngains;
extern uint reg_pk_dir_final_gain;
extern uint reg_pk_cir_final_gain;
extern uint reg_pk_final_pgain;
extern uint reg_pk_final_ngain;
extern uint reg_pk_nor_rsft_mode;
extern int hsize_in;
extern int vsize_in;

void amve_safa_demo_ctrl(unsigned int enable);
void osd_sharpness_size_ctrl(void);
void osd_sharpness_demo_ctrl(void);
bool is_hdr10plus_enable(void);

bool is_hdr10plus_enable(void);
void d_convert_str(int num,
			  int num_num, char cur_s[],
			  int char_bit, int bit_chose);
void str_sapr_to_d(char *s, int *d, int n);
extern unsigned int dpss_mode;
extern unsigned int watermark_support;
extern int gamut_mode;

extern unsigned int muxio_ready_flag;
void amvecm_muxio_on_vs(struct vframe_s *vf,
	enum vpp_index_e vpp_index);
void amvecm_set_dpss_mode(unsigned int val);
void amvecm_hdr_init_for_dpss(struct vframe_s *vf);
void amvecm_update_hdr_path_for_dpss(struct vframe_s *vf);
unsigned int amvecm_get_muxio_ready_for_dpss(void);
void amvecm_set_muxio_link_for_dpss(unsigned int link_flag,
	struct vframe_s *vf, enum vpp_index_e vpp_index);
int amvecm_signal_type_for_dpss(struct vframe_s *vf);
void amvecm_hdr_process_for_dpss(struct vframe_s *vf);
void amvecm_hdr_calculate_for_dpss(struct vframe_s *vf);
unsigned int amvecm_get_dpss_mode(void);
void hdr_path_switch_to_dpss(unsigned int val);
int hdr_path_delink_status(void);
void amvecm_vd1_dpss_switch_proc(struct vframe_s *vf,
	struct vframe_s *rpt_vf,
	enum vpp_index_e vpp_index);
void amvecm_update_link_state(struct vframe_s *vf,
	struct vframe_s *rpt_vf,
	enum vpp_index_e vpp_index);
void amvecm_set_ext_status_for_dpss(unsigned int val);
void amvecm_set_lc_evc_ctrl_for_dpss(unsigned int enable, unsigned int lc_evc_src);

enum vpp_matrix_ext_csc_e {
	VPP_MATRIX_EXT_NULL = 0,
	VPP_MATRIX_SMPTE_ST_170,
	VPP_MATRIX_BT_709,
	VPP_MATRIX_XVYCC_601,
	VPP_MATRIX_XVYCC_709,
	VPP_MATRIX_SYCC_601,
	VPP_MATRIX_OPYCC_601,
	VPP_MATRIX_OP_RGB,
	VPP_MATRIX_BT_2020_YCC,
	VPP_MATRIX_BT_2020_RGBORYCC,
	VPP_MATRIX_SMPTE_ST_2113_P3D65RGB,
	VPP_MATRIX_SMPTE_ST_2113_P3DCIRGB,
	VPP_MATRIX_BT_2100,
};

struct sharpness_param_reg {
	unsigned int reg_vpp_sr_en;
	unsigned int reg_vpp_pk_en;
	unsigned int reg_vpp_hti_en;
	unsigned int reg_vpp_vti_en;
	unsigned int reg_vpp_dering_en;
	unsigned int reg_vpp_nr_lpf_en;
};

struct sharpness_param {
	unsigned int sr0_pk;
	unsigned int sr1_pk;
	unsigned int sr0_hcti;
	unsigned int sr1_hcti;
	unsigned int sr0_hlti;
	unsigned int sr1_hlti;
	unsigned int sr0_vcti;
	unsigned int sr1_vcti;
	unsigned int sr0_vlti;
	unsigned int sr1_vlti;
	unsigned int sr0_dej;
	unsigned int sr1_dej;
	unsigned int sr0_drt;
	unsigned int sr1_drt;
	unsigned int sr0_der;
	unsigned int sr1_der;
	unsigned int s1_sr0_pk;
	unsigned int s1_sr1_pk;
};

extern struct vpp_mtx_info_s mtx_info;
extern unsigned int pq_bypass_debug_flag;
extern int cur_contrast_v;
extern int cur_contrast_u;
extern int cur_vpp_index;
extern int hdr_on;
extern int hdr_set_on;
extern unsigned int flag_cm_lc_dma_en;
extern int last_out_range;
extern unsigned int hdr_core_fix_mode;
extern int set_dummy_flag;
extern bool mosaic_mode;

extern unsigned int flag_cm_lc_dma_en;
extern unsigned int pc_mode;
extern int hdr10p_set_on;
extern bool suspend_drv_flag;
extern int force_eotf_type;
extern int force_oetf_type;

void gamut_mapping_wrapper_crtl(int module, int en);
bool suspend_drv_status_get(void);

#endif /* AMVECM_H */

