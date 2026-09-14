// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux headers */
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/of.h>
#include <linux/of_device.h>
/* #include <linux/amlogic/aml_common.h> */
#include <linux/ctype.h>/* for parse_para_pq */
#include <linux/vmalloc.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
#include <linux/amlogic/pixel_probe.h>
#endif
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/workqueue.h>
#include <linux/sched/clock.h>

#ifdef CONFIG_AMLOGIC_LCD
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include <linux/amlogic/media/amvecm/color_tune.h>
#include "arch/vpp_hdr_regs.h"
#include "arch/vpp_a4_regs.h"
#endif
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"
#include "arch/vpp_s7d_sr_regs.h"
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "../../vin/tvin/tvin_global.h"
#endif

#include "amve.h"
#include "amcm.h"
#include "amcsc.h"
#include "amcsc_pip.h"
#include "bitdepth.h"
#include "cm2_adj.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include "keystone_correction.h"
#include "pattern_detection.h"
#include "dnlp_cal.h"
#include "local_contrast.h"
#include "set_hdr2_v0.h"
#include "s5_set_hdr2_v0.h"
#include "ai_pq/ai_pq.h"
#include "reg_default_setting.h"
#include "util/enc_dec.h"
#include "cabc_aadc/cabc_aadc_fw.h"
#include "hdr/am_hdr10_plus.h"
#include "hdr/am_hdr10_tm.h"
#include "hdr/am_hdr10_tmo_fw.h"
#include "hdr/am_cuva_hdr_tm.h"
#include "hdr/gamut_convert.h"
#include "hdr/am_hdr10p_adaptive.h"
#include "frame_lock_policy.h"
#include "amve_v2.h"
#include "color/ai_color.h"
#include "am_lut3d.h"
#include "hdr/am_hdr_sbtm.h"
#endif
#include "vlock.h"
#include "reg_helper.h"
#include "../../video_sink/vpp_pq.h"
#include "am_dma_ctrl.h"

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_IOTRACE)
#include <linux/amlogic/aml_iotrace.h>
#endif

#define AMVECM_VERSION "amvecm module ver_20260105-0"

#define pr_amvecm_dbg(fmt, args...)\
	do {\
		if (debug_amvecm)\
			pr_info("AMVECM: " fmt, ## args);\
	} while (0)

#define pr_amvecm_error(fmt, args...)\
	pr_error("AMVECM: " fmt, ## args)

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#define pr_amvecm_bringup_dbg(fmt, args...)\
	do {\
		if (debug_amvecm_bringup)\
			pr_info("amvecm_bringup: " fmt, ## args);\
	} while (0)
#else
#define pr_amvecm_bringup_dbg(fmt, args...)
#endif

#define AMVECM_NAME               "amvecm"
#define AMVECM_DRIVER_NAME        "amvecm"
#define AMVECM_MODULE_NAME        "amvecm"
#define AMVECM_DEVICE_NAME        "amvecm"
#define AMVECM_CLASS_NAME         "amvecm"
#define AMVECM_VER				"Ref.2018/11/20"

struct amvecm_dev_s {
	dev_t                       devt;
	struct cdev                 cdev;
	dev_t                       devno;
	struct device               *dev;
	struct class                *clsp;
	wait_queue_head_t           hdr_queue;
	/*hdr*/
	struct hdr_data_t	hdr_d;
	struct gamma_data_s gm_data;
	/*vlock cts_vid_lock_clk*/
	struct clk *vlock_clk;
};

#ifdef CONFIG_AMLOGIC_LCD
struct work_struct aml_lcd_vlock_param_work;
#endif

static struct amvecm_dev_s amvecm_dev;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct resource *res_viu2_vsync_irq;
static struct resource *res_lc_curve_irq;
static struct workqueue_struct *aml_cabc_queue;
static struct work_struct cabc_proc_work;
static struct work_struct cabc_bypass_work;
#endif

enum chip_type chip_type_id;
enum chip_cls_e chip_cls_id;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct demo_data_s demo_data;

/*gamma loading protect*/
spinlock_t vpp_lcd_gamma_lock;
/*3dlut loading protect*/
struct mutex vpp_lut3d_lock;
#endif

signed int vd1_brightness = 0, vd1_contrast = 0;
signed int vd1_brightness2 = 0, vd1_contrast2 = 0;

static int hue_pre;  /*-25~25*/
static int saturation_pre;  /*-128~127*/
static int hue_post;  /*-25~25*/
static int saturation_post;  /*-128~127*/

static s16 saturation_ma;
static s16 saturation_mb;
static s16 saturation_ma_shift;
static s16 saturation_mb_shift;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
enum ecm_color_type cm_cur_work_color_md = cm_14_color;
int cm2_debug;

static int cm2_hue_array[cm_14_ecm2colormode_max][3];
static int cm2_luma_array[cm_14_ecm2colormode_max][3];
static int cm2_sat_array[cm_14_ecm2colormode_max][3];
static int cm2_hue_by_hs_array[cm_14_ecm2colormode_max][3];

unsigned int sr1_reg_val[101];
unsigned int sr1_ret_val[101];
struct vpp_hist_param_s vpp_hist_param;
static unsigned int pre_hist_height, pre_hist_width;
unsigned int pc_mode = 0xff;
static unsigned int pc_mode_last = 0xff;
static struct hdr_metadata_info_s vpp_hdr_metadata_s;
#endif
unsigned int atv_source_flg;

#define VDJ_FLAG_BRIGHTNESS		BIT(0)
#define VDJ_FLAG_BRIGHTNESS2		BIT(1)
#define VDJ_FLAG_SAT_HUE		BIT(2)
#define VDJ_FLAG_SAT_HUE_POST		BIT(3)
#define VDJ_FLAG_CONTRAST		BIT(4)
#define VDJ_FLAG_CONTRAST2		BIT(5)
#define VDJ_FLAG_VADJ_EN        BIT(6)

static int vdj_mode_flg;
struct am_vdj_mode_s vdj_mode_s;
struct am_osd_vdj_mode_s osd_vdj_mod_s;
struct pq_ctrl_s pq_cfg;
struct pq_ctrl_s dv_cfg_bypass;
struct pq_ctrl_s pq_cfg_cur;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#define RECOVERY_REG_CM_MAX 183
#define RECOVERY_REG_SR_MAX 234
#define RECOVERY_REG_LC_MAX 69
#define RECOVERY_REG_VE_MAX 12
#define RECOVERY_REG_MTX_MAX 14

bool resume_mtx_flag;
bool suspend_drv_flag;
struct am_regs_s amregs_store;
struct am_reg_s *reg_sr0_list;
struct am_reg_s *reg_sr1_list;
struct am_reg_s *reg_lc_list;
struct am_reg_s *reg_cm_list;
struct am_reg_s *reg_ve_list;
struct am_reg_s *reg_matrix_list;
struct am_reg_s *reg_vsr_list;

#endif
/*demo final gain */
uint demo_pk_sr_final_pgains = 0xa0;
uint demo_pk_sr_final_ngains = 0xa0;

uint reg_pk_dir_final_gain = 0xFF;
uint reg_pk_cir_final_gain = 0xFF;
uint reg_pk_final_pgain = 0xFF;
uint reg_pk_final_ngain = 0xFF;
uint reg_pk_nor_rsft_mode;
int hsize_in = 3840;
int vsize_in = 2160;
int pq_module_status[pq_module_module_max] = {1};

struct pq_ctrl_s pq_cfg_init[PQ_CFG_MAX] = {
	/*for tv enable pq module*/
	{
		.sharpness0_en = 1,
		.sharpness1_en = 1,
		.dnlp_en = 1,
		.cm_en = 1,
		.vadj1_en = 1,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 1,
		.gamma_en = 1,
		.lc_en = 1,
		.black_ext_en = 1,
		.chroma_cor_en = 0,
		.reserved = 0,
	},
	/*for box disable all pq module*/
	{
		.sharpness0_en = 0,
		.sharpness1_en = 0,
		.dnlp_en = 0,
		.cm_en = 0,
		.vadj1_en = 0,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 0,
		.gamma_en = 0,
		.lc_en = 0,
		.black_ext_en = 0,
		.chroma_cor_en = 0,
		.reserved = 0,
	},
	/*for tv dv bypass pq module*/
	{
		.sharpness0_en = 0,
		.sharpness1_en = 0,
		.dnlp_en = 0,
		.cm_en = 0,
		.vadj1_en = 0,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 1,
		.gamma_en = 1,
		.lc_en = 0,
		.black_ext_en = 0,
		.chroma_cor_en = 0,
		.reserved = 0,
	},
	/*for ott dv bypass pq module*/
	{
		.sharpness0_en = 0,
		.sharpness1_en = 0,
		.dnlp_en = 0,
		.cm_en = 0,
		.vadj1_en = 0,
		.vd1_ctrst_en = 0,
		.vadj2_en = 0,
		.post_ctrst_en = 0,
		.wb_en = 0,
		.gamma_en = 0,
		.lc_en = 0,
		.black_ext_en = 0,
		.chroma_cor_en = 0,
		.reserved = 0,
	},
	/*for init current cfg*/
	{
		.sharpness0_en = 0xf,
		.sharpness1_en = 0xf,
		.dnlp_en = 0xf,
		.cm_en = 0xf,
		.vadj1_en = 0xf,
		.vd1_ctrst_en = 0xf,
		.vadj2_en = 0xf,
		.post_ctrst_en = 0xf,
		.wb_en = 0xf,
		.gamma_en = 0xf,
		.lc_en = 0xf,
		.black_ext_en = 0xf,
		.chroma_cor_en = 0xf,
		.reserved = 0xf,
	}
};

/*void __iomem *amvecm_hiu_reg_base;*//* = *ioremap(0xc883c000, 0x2000); */

static int debug_amvecm;
unsigned int probe_ok;/* probe ok or not */
unsigned int pq_load_en = 1;  /*load pq table enable/disable*/
unsigned int vecm_latch_flag;
unsigned int vecm_latch_flag2;
unsigned int pq_user_latch_flag;
int wb_en;  /* wb_en enable/disable */
int tx_op_color_primary;  /*0: 709/601, 1: bt2020*/
unsigned int debug_game_mode_1;
int freerun_en = GAME_MODE;/* 0:game mode;1:freerun mode */
unsigned int hdr_output_mode;
unsigned int data_path;  /* 0:main;1:sub */
unsigned int pq_user_value;
unsigned int pq_bypass_debug_flag;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static int debug_amvecm_bringup;
int gamma_en;  /* wb_gamma_en enable/disable */
unsigned int lut3d_long_sec_en = 1;  /* lut3d_long_sec_en enable/disable */
unsigned int lut3d_compress = 1;  /* lut3d_compress enable/disable */
unsigned int lut3d_write_from_file = 1;  /* lut3d_write from a file */
unsigned int lut3d_data_source = 2;/* read fron bin */
static unsigned int sr1_index;/* for sr1 read */
static int mtx_sel_dbg;/* for mtx debug */
unsigned int fmeter_debug = 255;/* for fmeter debug */
unsigned int fmeter_count = 2;/* for fmeter count */
int bs_3dlut_en = 1;  /*blue stretch function with 3dlut*/
unsigned int ct_en;
unsigned int ai_color_enable;
unsigned int pre_aiclr_en = 1;
unsigned int eo_curve_type = 0xff;
enum hdr_type_e hdr_source_type = HDRTYPE_NONE;
unsigned int pd_detect_en;
unsigned int pd_det;
int pre_fmeter_level = 0, cur_fmeter_level = 0, fmeter_flag = 0;
int cur_sr_level = 5;
static int num_3dlut_tmp = LUT3D_TMP_BUF_LEN;
int g_lut3d_tmp_buf[LUT3D_TMP_BUF_LEN] = {0};
module_param_array(g_lut3d_tmp_buf, int, &num_3dlut_tmp, 0664);
MODULE_PARM_DESC(g_lut3d_tmp_buf, "\n num_3dlut_tmp\n");
#endif

int pd_weak_fix_lvl = PD_LOW_LVL;
int pd_fix_lvl = PD_HIG_LVL;

int hdr_tool_matrix_mode;

unsigned int gmv_weak_th = 4;
unsigned int gmv_th = 17;
unsigned int flag_cm_lc_dma_en = 3;

int sat_hue_offset_val;

/* bit0: SDR->HDR10 */
/* bit1: SDR->HLG */
/* bit2: HDR10->SDR */
/* bit3: HDR10->HLG */
/* bit4: HLG->SDR */
/* bit5: HLG->HDR10 */
/* bit6: HDR10_PLUS->HDR10 */
/* bit7: HDR10_PLUS->SDR */
/* bit8: HDR10_PLUS->HLG */
/* bit9: CUVA_HDR->SDR */
/* bit10: CUVA_HDR->HDR10 */
/* bit11: CUVA_HDR->HLG */
/* bit12: CUVA_HLG->SDR */
/* bit13: CUVA_HLG->HDR10 */
/* bit14: CUVA_HLG->HLG */
/* bit15: DOLBY_VISION->SDR */
/* bit16: SDR->DOLBY_VISION */
/* bit17: DOLBY_VISION->HDR10*/
/* bit18: HDR10->DOLBY_VISION */
/* bit19: HLG->DOLBY_VISION */
int hdr_cap;

static int wb_init_bypass_coef[24] = {
	0, 0, 0, /* pre offset */
	1024,	0,	0,
	0,	1024,	0,
	0,	0,	1024,
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 0, 0, /* offset */
	0, 0, 0  /* mode, right_shift, clip_en */
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static int sr0_gain_val[11] = {
	128, 128, 120, 112, 104, 96, 88, 80, 72, 64, 64
};

static int sr0_shoot_val[11] = {
	30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10
};

static int sr1_gain_val[11] = {
	128, 128, 120, 112, 104, 96, 88, 80, 72, 64, 64
};

static int sr1_shoot_val[11] = {
	40, 38, 36, 34, 32, 30, 28, 26, 24, 22, 20
};

static int sr0_gain_lmt[11] = {
	5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 60
};

static int sr1_gain_lmt[11] = {
	20, 25, 28, 30, 35, 38, 40, 45, 50, 55, 60
};

static int nn_coring[11] = {
	10, 8, 6, 4, 3, 2, 1, 1, 1, 1, 1
};

static int vsr_update_flag = 1;
int dump_flag;
#endif

#define AIPQ_SCENE_MAX 25
#define AIPQ_FUNC_MAX 10
#define AIPQ_SINGL_DATA_LEN 3

struct vpp_mtx_info_s mtx_info = {
	MTX_NULL,
	{
		{0, 0, 0},
		{
			{0x400, 0x0, 0x0},
			{0x0, 0x400, 0x0},
			{0x0, 0x0, 0x400},
		},
		{0, 0, 0},
		0,
		0,
	}
};

unsigned int watermark_support;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct vpp_hist_param_s *get_vpp_hist(void)
{
	return &vpp_hist_param;
}

static struct pre_gamma_table_s pre_gamma;
struct eye_protect_s eye_protect;
static int hist_chl;
unsigned int ll_hist_mode;

static unsigned int cm_slice_idx;

unsigned int osd_pic_en;
unsigned int slt_lut3d_en;

unsigned int slt_en;
static void amvecm_wb_init(bool en);
#define COEFF_NORM(a) ((int)((((a) * 2048.0) + 1) / 2))
#define MATRIX_5x3_COEF_SIZE 24

static int RGB709_to_YUV709l_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(0.181873),	COEFF_NORM(0.611831),	COEFF_NORM(0.061765),
	COEFF_NORM(-0.100251),	COEFF_NORM(-0.337249),	COEFF_NORM(0.437500),
	COEFF_NORM(0.437500),	COEFF_NORM(-0.397384),	COEFF_NORM(-0.040116),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	64, 512, 512, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

struct fmeter_data_s data_meter;
struct aml_fmeter_drv_param_s fmeter_drv_param;

bool is_hdr_stb_mode(void)
{
	if ((is_meson_txlx_cpu() && !vinfo_lcd_support()) ||
		(is_meson_tm2_cpu() && !vinfo_lcd_support()) ||
		(is_meson_t7_cpu() && !vinfo_lcd_support()) ||
		is_meson_gxm_cpu() || is_meson_g12a_cpu() ||
		is_meson_g12b_cpu() || is_meson_sm1_cpu() ||
		is_meson_sc2_cpu() || is_meson_s4d_cpu() ||
		is_meson_s5_cpu() || is_meson_s4_cpu() ||
		is_meson_s7_cpu() || is_meson_s7d_cpu() ||
		is_meson_s6_cpu() || chip_type_id == chip_a9)
		return true;
	else
		return false;
}

bool is_hdr_tv_mode(void)
{
	if ((is_meson_txlx_cpu() && vinfo_lcd_support()) ||
		(is_meson_tm2_cpu() && vinfo_lcd_support()) ||
		(is_meson_t7_cpu() && vinfo_lcd_support()) ||
		is_meson_t3_cpu() || is_meson_t5w_cpu() ||
		is_meson_t5m_cpu() || is_meson_t5d_cpu())
		return true;
	else
		return false;
}

void str_sapr_to_d(char *s, int *d, int n)
{
	int i, j, count;
	long value = 0;
	char des[9] = {0};

	count = (strlen(s) + n - 2) / (n - 1);
	for (i = 0; i < count; i++) {
		for (j = 0; j < n - 1; j++)
			des[j] = s[j + i * (n - 1)];

		des[n - 1] = '\0';
		if (kstrtol(des, 10, &value) < 0)
			return;
		d[i] = value;
	}
}

void d_convert_str(int num,
			  int num_num, char cur_s[],
			  int char_bit, int bit_chose)
{
	char buf[9] = {0};
	int i, count;

	if (bit_chose == 10)
		snprintf(buf, sizeof(buf), "%d", num);
	else if (bit_chose == 16)
		snprintf(buf, sizeof(buf), "%x", num);
	count = strlen(buf);
	if (count > 4)
		count = 4;
	for (i = 0; i < count; i++)
		buf[i + char_bit] = buf[i];
	for (i = 0; i < char_bit; i++)
		buf[i] = '0';
	count = strlen(buf);
	for (i = 0; i < char_bit; i++)
		buf[i] = buf[count - char_bit + i];
	if (num_num > 0) {
		for (i = 0; i < char_bit; i++)
			cur_s[i + num_num * char_bit] =
				buf[i];
	} else {
		for (i = 0; i < char_bit; i++)
			cur_s[i] = buf[i];
	}
}

void int_convert_str(int num,
	int num_num, char cur_s[],
	int char_bit, int bit_chose)
{
	char buf[9] = {0};
	int i, count;

	if (bit_chose == 10)
		snprintf(buf, sizeof(buf), "%d", num);
	else if (bit_chose == 16)
		snprintf(buf, sizeof(buf), "%08x", num);
	count = strlen(buf);
	if (char_bit > count)
		char_bit = count;
	for (i = 0; i < char_bit; i++)
		buf[i] = buf[count - char_bit + i];
	if (num_num > 0) {
		for (i = 0; i < char_bit; i++)
			cur_s[i + num_num * char_bit] =
				buf[i];
	} else {
		for (i = 0; i < char_bit; i++)
			cur_s[i] = buf[i];
	}
}
#endif

bool enable_hdr10plus;/* enable hdr10+ or not */
u32 hdr_cap_dbg;

/* vpp brightness/contrast/saturation/hue */
int __init amvecm_load_pq_val(char *str)
{
	int i = 0, err = 0;
	char *tk = NULL, *tmp[4];
	long val;

	if (!str) {
		pr_err("[amvecm] pq val error !!!\n");
		return 0;
	}

	for (tk = strsep(&str, ","); tk; tk = strsep(&str, ",")) {
		tmp[i] = tk;
		err = kstrtol(tmp[i], 10, &val);
		if (err) {
			pr_err("[amvecm] pq string error !!!\n");
			break;
		}
		/* pr_err("[amvecm] pq[%d]: %d\n", i, (int)val[i]); */

		/* only need to get sat/hue value,*/
		/*brightness/contrast can be got from registers */
		if (i == 2)
			saturation_post = (int)val;
		else if (i == 3)
			hue_post = (int)val;
		i++;
		if (i >= 4)
			return 0;
	}

	return 1;
}
__setup("pq=", amvecm_load_pq_val);

void amvecm_vadj_latch_process(int vpp_index)
{
	/*vadj switch control according to vadj1_en/vadj2_en*/
	unsigned int cur_vadj1_en, cur_vadj2_en;
	unsigned int vadj1_en = vdj_mode_s.vadj1_en;
	unsigned int vadj2_en = vdj_mode_s.vadj2_en;

	if (vecm_latch_flag & FLAG_VADJ_EN) {
		vecm_latch_flag &= ~FLAG_VADJ_EN;
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			cur_vadj1_en = READ_VPP_REG_BITS(VPP_VADJ1_MISC, 0, 1);
			cur_vadj2_en = READ_VPP_REG_BITS(VPP_VADJ2_MISC, 0, 1);
			if (cur_vadj1_en != vadj1_en) {
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_MISC,
							 vadj1_en, 0, 1, vpp_index);
				pr_amvecm_dbg("[amvecm.]vadj1 switch[%d->%d]success.\n",
					      cur_vadj1_en, vadj1_en);
			} else {
				pr_amvecm_dbg("[amvecm.] vadj1_en status unchanged.\n");
			}

			if (cur_vadj2_en != vadj2_en) {
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ2_MISC,
							 vadj2_en, 0, 1, vpp_index);
				pr_amvecm_dbg("[amvecm.] vadj2 switch [%d->%d] success.\n",
					      cur_vadj2_en, vadj2_en);
			} else {
				pr_amvecm_dbg("[amvecm.] vadj2_en status unchanged.\n");
			}
		} else {
			cur_vadj1_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 1);
			cur_vadj2_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 2, 1);

			if (cur_vadj1_en != vadj1_en) {
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL,
							 vadj1_en, 0, 1, vpp_index);
				pr_amvecm_dbg("[amvecm] vadj1 switch [%d->%d] success.\n",
					      cur_vadj1_en, vadj1_en);
			} else {
				pr_amvecm_dbg("[amvecm] vadj1_en status unchanged.\n");
			}

			if (cur_vadj2_en != vadj2_en) {
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL,
							 vadj2_en, 2, 1, vpp_index);
				pr_amvecm_dbg("[amvecm] vadj2 switch [%d->%d] success.\n",
					      cur_vadj2_en, vadj2_en);
			} else {
				pr_amvecm_dbg("[amvecm] vadj2_en status unchanged.\n");
			}
		}
	}
}

static int amvecm_set_contrast2(int val)
{
	val += 0x80;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		WRITE_VPP_REG_BITS(VOUT_VADJ_Y,
			val, 0, 8);
	} else if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_contrast_set(val, VE_VADJ2, WR_VCB, 0);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y_2,
				   val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 0, 1);

	} else {
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y,
				   val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	}
	return 0;
}

static int amvecm_set_brightness2(int val)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		WRITE_VPP_REG_BITS(VOUT_VADJ_Y,
			val, 8, 11);
		return 0;
	} else if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_brigtness_set(val, VE_VADJ2, WR_VCB, 0);
		return 0;
	}
#endif
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y,
				   val, 8, 9);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y_2,
				   val, 8, 11);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else
		WRITE_VPP_REG_BITS(VPP_VADJ2_Y,
				   val >> 1, 8, 10);
#endif

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 0, 1);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
#endif
	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void amvecm_size_patch(struct vframe_s *vf,
	unsigned int cm_in_w,
	unsigned int cm_in_h,
	int vpp_index)
{
	unsigned int hs, he, vs, ve;

	if (chip_type_id == chip_s7 ||
		chip_type_id == chip_c5)
		return;

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		cm2_frame_size_patch(vf, cm_in_w, cm_in_h, vpp_index);
		return;
	}

	if (!vf)
		return;

	if (cm_in_w == 0 && cm_in_h == 0) {
		hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 16, 13);
		he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END, 0, 13);

		vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 16, 13);
		ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END, 0, 13);
		cm2_frame_size_patch(vf, he - hs + 1, ve - vs + 1, vpp_index);
	} else {
		cm2_frame_size_patch(vf, cm_in_w, cm_in_h, vpp_index);
	}
}
#endif

/* video adj1 */
static ssize_t video_adj1_brightness_show(const struct class *cla,
					  const struct class_attribute *attr,
					  char *buf)
{
	s32 val = 0;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		val = ve_brightness_contrast_get(VE_VADJ1);
		val = (val >> 8) & 0x7ff;
		val = (val << 21) >> 21;
		return sprintf(buf, "%d\n", val);
	}
#endif
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
		val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		val = (READ_VPP_REG(VPP_VADJ1_Y_2) >> 8) & 0x7ff;
		val = (val << 21) >> 21;

		return sprintf(buf, "%d\n", val);
	}
	val = (READ_VPP_REG(VPP_VADJ1_Y) >> 8) & 0x3ff;
	val = (val << 22) >> 22;

	return sprintf(buf, "%d\n", val << 1);
}

static ssize_t video_adj1_brightness_store(const struct class *cla,
					   const struct class_attribute *attr,
					   const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1023)
		return -EINVAL;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		ve_brigtness_set(val, VE_VADJ1, WR_VCB, 0);
#endif
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB)
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 8, 9);
	else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y_2, val, 8, 11);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val >> 1, 8, 10);

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		return count;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	else
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);

	return count;
}

static ssize_t video_adj1_contrast_show(const struct class *cla,
					const struct class_attribute *attr,
					char *buf)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		return sprintf(buf, "%d\n",
			(int)(ve_brightness_contrast_get(VE_VADJ1) & 0xff) - 0x80);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ1_Y_2) & 0xff) - 0x80);
	else
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ1_Y) & 0xff) - 0x80);
}

static ssize_t video_adj1_contrast_store(const struct class *cla,
					 const struct class_attribute *attr,
					 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -127 || val > 127)
		return -EINVAL;

	val += 0x80;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_contrast_set(val, VE_VADJ1, WR_VCB, 0);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y_2, val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
	} else {
		WRITE_VPP_REG_BITS(VPP_VADJ1_Y, val, 0, 8);
		WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	}

	return count;
}

/* video adj2 */
static ssize_t video_adj2_brightness_show(const struct class *cla,
					  const struct class_attribute *attr,
					  char *buf)
{
	s32 val = 0;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		val = (READ_VPP_REG(VOUT_VADJ_Y) >> 8) & 0x7ff;
		val = (val << 21) >> 21;
		return sprintf(buf, "%d\n", val);
	} else if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		val = ve_brightness_contrast_get(VE_VADJ2);
		val = (val >> 8) & 0x7ff;
		val = (val << 21) >> 21;
		return sprintf(buf, "%d\n", val);
	}
#endif
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	else if (get_cpu_type() <= MESON_CPU_MAJOR_ID_GXTVBB) {
		val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x1ff;
		val = (val << 23) >> 23;

		return sprintf(buf, "%d\n", val);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		val = (READ_VPP_REG(VPP_VADJ2_Y_2) >> 8) & 0x7ff;
		val = (val << 21) >> 21;

		return sprintf(buf, "%d\n", val);
	}
	val = (READ_VPP_REG(VPP_VADJ2_Y) >> 8) & 0x3ff;
	val = (val << 22) >> 22;

	return sprintf(buf, "%d\n", val << 1);
}

static ssize_t video_adj2_brightness_store(const struct class *cla,
					   const struct class_attribute *attr,
			const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1023)
		return -EINVAL;
	amvecm_set_brightness2(val);
	return count;
}

static ssize_t video_adj2_contrast_show(const struct class *cla,
					const struct class_attribute *attr,
					char *buf)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VOUT_VADJ_Y) & 0xff) - 0x80);
	} else if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		return sprintf(buf, "%d\n",
			(int)(ve_brightness_contrast_get(VE_VADJ2) & 0xff) - 0x80);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ2_Y_2) & 0xff) - 0x80);
	else
		return sprintf(buf, "%d\n",
			(int)(READ_VPP_REG(VPP_VADJ2_Y) & 0xff) - 0x80);
}

static ssize_t video_adj2_contrast_store(const struct class *cla,
					 const struct class_attribute *attr,
					 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -127 || val > 127)
		return -EINVAL;
	amvecm_set_contrast2(val);
	return count;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static ssize_t amvecm_usage_show(const struct class *cla,
				 const struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info("brightness_val range:-255~255\n");
	pr_info("contrast_val range:-127~127\n");
	pr_info("saturation_val range:-128~128\n");
	pr_info("hue_val range:-25~25\n");
	pr_info("************video brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness1\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast1\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_pre\n");
	pr_info("************after video+osd blender, brightness & contrast & saturation_hue adj as flow*************\n");
	pr_info("echo brightness_val > /sys/class/amvecm/brightness2\n");
	pr_info("echo contrast_val > /sys/class/amvecm/contrast2\n");
	pr_info("echo saturation_val hue_val > /sys/class/amvecm/saturation_hue_post\n");
	return 0;
}
#endif

static void parse_param_amvecm(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void amvecm_3d_sync_status(void)
{
	unsigned int sync_h_start, sync_h_end, sync_v_start,
		sync_v_end, sync_polarity,
		sync_out_inv, sync_en;
	if (!is_meson_gxtvbb_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return;
	}
	sync_h_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 13);
	sync_h_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 16, 13);
	sync_v_start = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13);
	sync_v_end = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 16, 13);
	sync_polarity = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 29, 1);
	sync_out_inv = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 15, 1);
	sync_en = READ_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 31, 1);
	pr_info("\n current 3d sync state:\n");
	pr_info("sync_h_start:%d\n", sync_h_start);
	pr_info("sync_h_end:%d\n", sync_h_end);
	pr_info("sync_v_start:%d\n", sync_v_start);
	pr_info("sync_v_end:%d\n", sync_v_end);
	pr_info("sync_polarity:%d\n", sync_polarity);
	pr_info("sync_out_inv:%d\n", sync_out_inv);
	pr_info("sync_en:%d\n", sync_en);
	pr_info("sync_3d_black_color:%d\n", sync_3d_black_color);
	pr_info("sync_3d_sync_to_vbo:%d\n", sync_3d_sync_to_vbo);
}

static ssize_t amvecm_3d_sync_show(const struct class *cla,
				   const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		"echo hstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo hend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo vstart val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo vend val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo pola val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo inv val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo black_color val(Hex) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo sync_to_vx1 val(D) > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo enable > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo disable > /sys/class/amvecm/sync_3d\n");
	len += sprintf(buf + len,
		"echo status > /sys/class/amvecm/sync_3d\n");
	return len;
}

static ssize_t amvecm_3d_sync_store(const struct class *cla,
				    const struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val;

	if (!buf)
		return count;

	if (!is_meson_gxtvbb_cpu()) {
		pr_info("\n chip does not support 3D sync process!!!\n");
		return count;
	}

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "hstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_h_start = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
	} else if (!strncmp(parm[0], "hend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_h_end = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
	} else if (!strncmp(parm[0], "vstart", 6)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_v_start = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
	} else if (!strncmp(parm[0], "vend", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_v_end = val & 0x1fff;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
	} else if (!strncmp(parm[0], "pola", 4)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_polarity = val & 0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
	} else if (!strncmp(parm[0], "inv", 3)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_out_inv = val & 0x1;
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
	} else if (!strncmp(parm[0], "black_color", 11)) {
		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_black_color = val & 0xffffff;
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				   sync_3d_black_color, 0, 24);
	} else if (!strncmp(parm[0], "sync_to_vx1", 11)) {
		if (kstrtol(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		sync_3d_sync_to_vbo = val & 0x1;
	} else if (!strncmp(parm[0], "enable", 6)) {
		vecm_latch_flag |= FLAG_3D_SYNC_EN;
	} else if (!strncmp(parm[0], "disable", 7)) {
		vecm_latch_flag |= FLAG_3D_SYNC_DIS;
	} else if (!strncmp(parm[0], "status", 7)) {
		amvecm_3d_sync_status();
	}
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_vlock_show(const struct class *cla,
				 const struct class_attribute *attr, char *buf)
{
	return vlock_debug_show(cla, attr, buf);
}

static ssize_t amvecm_vlock_store(const struct class *cla,
				  const struct class_attribute *attr,
		const char *buf, size_t count)
{
	return vlock_debug_store(cla, attr, buf, count);
}

static ssize_t amvecm_frame_lock_show(const struct class *cla,
				 const struct class_attribute *attr, char *buf)
{
	return frame_lock_debug_show(cla, attr, buf);
}

static ssize_t amvecm_frame_lock_store(const struct class *cla,
				  const struct class_attribute *attr,
		const char *buf, size_t count)
{
	return frame_lock_debug_store(cla, attr, buf, count);
}

static ssize_t amvecm_slt_vl_lock_st_show(const struct class *cla,
				 const struct class_attribute *attr, char *buf)
{
	return vlock_slt_lock_st_show(cla, attr, buf);
}

static ssize_t amvecm_slt_vl_lock_st_store(const struct class *cla,
				  const struct class_attribute *attr,
		const char *buf, size_t count)
{
	return vlock_slt_lock_st_store(cla, attr, buf, count);
}

static ssize_t amvecm_enable_hdr10plus_show
	 (const struct class *cla,
	  const struct class_attribute *attr,
	  char *buf)
{
	return sprintf(buf, "%d\n", enable_hdr10plus);
}

static ssize_t amvecm_hdr_cap_dbg_show
	 (const struct class *cla,
	  const struct class_attribute *attr,
	  char *buf)
{
	ssize_t len = 0;
	int i;
	struct dv_info *dv_info = &hdr_cap_info.dv_info;

	len += sprintf(buf + len, "HDR RX support list:\n");
	len += sprintf(buf + len,
		"  HDR10Plus Supported: %d\n", (hdr_cap_info.hdr_support & HDRP_SUPPORT) ? 1 : 0);
	len += sprintf(buf + len,
		"  HDR10 Supported: %d\n", (hdr_cap_info.hdr_support & HDR_SUPPORT) ? 1 : 0);
	len += sprintf(buf + len,
		"  HLG Supported: %d\n", (hdr_cap_info.hdr_support & HLG_SUPPORT) ? 1 : 0);
	len += sprintf(buf + len,
		"  CUVA Supported: %d\n\n", (hdr_cap_info.hdr_support & CUVA_SUPPORT) ? 1 : 0);
	if (hdr_cap_info.dv_info.ieeeoui != DV_IEEE_OUI ||
		hdr_cap_info.dv_info.block_flag != CORRECT) {
		len += sprintf(buf + len,
			"The Rx don't support DolbyVision\n");
	} else {
		len += sprintf(buf + len,
			"DolbyVision RX support list:\n");
		len += sprintf(buf + len,
			"VSVDB Version: V%d\n", dv_info->ver);
		len += sprintf(buf + len,
			"2160p%shz: 1\n", dv_info->sup_2160p60hz ? "60" : "30");
		len += sprintf(buf + len,
			"Parity: %d\n", dv_info->parity);
		len += sprintf(buf + len,
			"Support mode:\n");
		if (dv_info->Interface != 0x00 && dv_info->Interface != 0x01) {
			len += sprintf(buf + len,
				"  DV_RGB_444_8BIT\n");
			if (dv_info->sup_yuv422_12bit)
				len += sprintf(buf + len,
					"  DV_YCbCr_422_12BIT\n");
		}
		len += sprintf(buf + len,
			"  LL_YCbCr_422_12BIT\n");
		if (dv_info->Interface == 0x01 || dv_info->Interface == 0x03) {
			if (dv_info->sup_10b_12b_444 == 0x1) {
				len += sprintf(buf + len,
					"  LL_RGB_444_10BIT\n");
			}
			if (dv_info->sup_10b_12b_444 == 0x2) {
				len += sprintf(buf + len,
					"  LL_RGB_444_12BIT\n");
			}
		}
		len += sprintf(buf + len,
			"IEEEOUI: 0x%06x\n", dv_info->ieeeoui);
		len += sprintf(buf + len,
			"EMP: %d\n", dv_info->dv_emp_cap);
		len += sprintf(buf + len, "VSVDB: ");
	}
	for (i = 0; i < (dv_info->length + 1); i++)
		len += sprintf(buf + len, "%02x", dv_info->rawdata[i]);
	len += sprintf(buf + len,
		"\n\noutput attr:\n");
	if (hdr_cap_info.cs == 0)
		len += sprintf(buf + len, "  rgb,");
	else if (hdr_cap_info.cs == 1)
		len += sprintf(buf + len, "  422,");
	else if (hdr_cap_info.cs == 2)
		len += sprintf(buf + len, "  444,");
	else if (hdr_cap_info.cs == 3)
		len += sprintf(buf + len, "  420,");
	if (hdr_cap_info.cd == 4)
		len += sprintf(buf + len, "8bit\n");
	else if (hdr_cap_info.cd == 5)
		len += sprintf(buf + len, "10bit\n");
	else if (hdr_cap_info.cd == 6)
		len += sprintf(buf + len, "12bit\n");
	else if (hdr_cap_info.cd == 7)
		len += sprintf(buf + len, "16bit\n");
	return len;
}

/* #endif */

unsigned int pr_hist;
#endif
/* return lum_ave:
 * -1: no vframe, error ave
 * 0~255: 8bit ave = lum_sum/pixel_sum
 */
static int lum_ave = -1;

int get_lum_ave(void)
{
	return lum_ave;
}
EXPORT_SYMBOL(get_lum_ave);

void set_lum_ave(int ave)
{
	lum_ave = ave;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT

static void vpp_dump_histgram(void)
{
	uint i;
	unsigned int tmp;

	pr_info("%s:\n", __func__);

	pr_info("\n\t dump_dnlp_hist begin\n");
	for (i = 0; i < 64; i++) {
		pr_info("[%d]0x%-8x\t", i, vpp_hist_param.vpp_gamma[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	pr_info("\t dump_dnlp_hist done\n");

	if (chip_type_id == chip_t6x) {
		pr_info("\n\t dump_minhist begin\n");
		for (i = 0; i < 6; i++) {
			pr_info("[%d]0x%-8x\t", i, vpp_hist_param.vpp_6binhist_min[i]);
			if ((i + 1) % 8 == 0)
				pr_info("\n");
		}
		pr_info("\t dump_min_hist done\n");

		pr_info("\n\t dump_max_hist begin\n");
		for (i = 0; i < 6; i++) {
			pr_info("[%d]0x%-8x\t", i, vpp_hist_param.vpp_6binhist_max[i]);
			if ((i + 1) % 8 == 0)
				pr_info("\n");
		}
		pr_info("\t dump_max_hist done\n");
	}

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t3x ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		pr_info("\n\t dump vpp dark hist\n");
		for (i = 0; i < 64; i++) {
			pr_info("[%d]0x%-8x\t", i,
				vpp_hist_param.vpp_dark_hist[i]);
			if ((i + 1) % 8 == 0)
				pr_info("\n");
		}
		pr_info("\n\t dump vpp dark hist done\n");
	}

	pr_info("\t dump_hdr_hist begin\n");
	for (i = 0; i < 128; i++) {
		if (chip_type_id == chip_t3x ||
			chip_type_id == chip_s5)
			tmp = s5_hdr_hist[NUM_HDR_HIST - 1][i];
		else
			tmp = hdr_hist[NUM_HDR_HIST - 1][i];
		pr_info("[%d]0x%-8x\t", i, tmp);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	pr_info("\t dump_hdr_hist done\n");

	/*
	 *if (chip_type_id == chip_t3x) {
	 *	pr_info("\t dump_hdr_hist_vd2 begin\n");
	 *	for (i = 0; i < 128; i++) {
	 *		pr_info("[%d]0x%-8x\t", i,
	 *			s5_hdr_hist_vd2[NUM_HDR_HIST - 1][i]);
	 *		if ((i + 1) % 8 == 0)
	 *			pr_info("\n");
	 *	}
	 *	pr_info("\t dump_hdr_hist_vd2 done\n");
	 *}
	 */
}

void vpp_get_hist_en(void)
{
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5)
		return;

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		vpp_luma_hist_init();
		return;
	}

	if (hist_chl)
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x2, 11, 3);
	else
		WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 11, 3);

	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 0x1, 0, 1);
	WRITE_VPP_REG(VI_HIST_GCLK_CTRL, 0xffffffff);
	WRITE_VPP_REG_BITS(VI_HIST_CTRL, 2, VI_HIST_POW_BIT, VI_HIST_POW_WID);
}

static unsigned int vpp_luma_max;

void get_dark_luma_hist(struct vpp_hist_param_s *vp, struct vframe_s *vf)
{
	int i;
	int hist_idx;
	int hist_val;
	int rs;
	unsigned int h_rate = 0;
	unsigned int w_rate = 0;
	unsigned int src_h = 0;
	unsigned int src_w = 0;
	unsigned int val = 2;
	unsigned int height;
	unsigned int width;

	if (!vf)
		return;

	w_rate = vf->dpss_pps_dsx;
	h_rate = vf->dpss_pps_dsy;

	height = (vf->type & VIDTYPE_COMPRESS) ? vf->compHeight : vf->height;
	width = (vf->type & VIDTYPE_COMPRESS) ? vf->compWidth : vf->width;

	if (h_rate)
		src_h = height / (h_rate + 1);
	else
		src_h = height;

	if (w_rate)
		src_w = width / (w_rate + 1);
	else
		src_w = width;

	if (src_w >= 3840 && src_h >= 2160)
		val = 4;
	else if ((src_w >= 3840 && src_h >= 1080) ||
		(src_w >= 2560 && src_h >= 1440))
		val = 3;
	else
		val = 2;

	rs = val + 3;
	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		hist_idx = 1 << 6;
		WRITE_VPP_REG(VI_RO_HIST_LOW_IDX, hist_idx);
		for (i = 0; i < 64; i++) {
			hist_val = READ_VPP_REG(VI_RO_HIST_LOW);
			if (ll_hist_mode)
				hist_val >>= rs;
			vp->vpp_dark_hist[i] = hist_val;
		}
	}
}

void get_cm_hist(struct vpp_hist_param_s *vp)
{
	int i;
	int addr_port;
	int data_port;

	/*s5 not support color hist*/
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_s7d ||
		suspend_drv_status_get())
		return;

	if (enable_pattern_detect == 1) {
		if (chip_type_id != chip_t3x) {
			if (chip_type_id == chip_t6x && (flag_cm_lc_dma_en & 0x1) &&
				!check_dma_status(EN_DMA_WR_ID_CM2_LC)) {
				am_dma_get_mif_data_cm2_hist_hue(0, vp->vpp_hue_gamma, 32);
				am_dma_get_mif_data_cm2_hist_sat(0, vp->vpp_sat_gamma, 32);
				return;
			}

			addr_port = VPP_CHROMA_ADDR_PORT;
			data_port = VPP_CHROMA_DATA_PORT;

			for (i = 0; i < 32; i++) {
				WRITE_VPP_REG(addr_port,
					RO_CM_HUE_HIST_BIN0 + i);
				vp->vpp_hue_gamma[i] =
					READ_VPP_REG(data_port);
			}

			for (i = 0; i < 32; i++) {
				WRITE_VPP_REG(addr_port,
					RO_CM_SAT_HIST_BIN0 + i);
				vp->vpp_sat_gamma[i] =
					READ_VPP_REG(data_port);
			}
		} else {
			cm_hist_get(vp, RO_CM_HUE_HIST_BIN0,
				RO_CM_SAT_HIST_BIN0);
		}
	}
}

void refresh_hist_info(struct vframe_s *vf,
	struct vpp_hist_param_s *vp, int vpp_index)
{
	if (chip_type_id == chip_t3x) {
		get_luma_hist(vf, vp, vpp_index);
		s5_get_hist(VD1_PATH, HIST_E_RGBMAX, vpp_index);
	}
}

struct luma_minmax_param_s {
	int max_thr_rate; /*0 - 63*/
	int min_thr_rate;
	int max_thr_cnt;
	int min_thr_cnt;
	int luma_max;
	int luma_min;
	int hist_sum_max;
	int hist_sum_min;
	int vpp_luma_max;
	int vpp_luma_min;
};

struct luma_minmax_param_s param;

void  get_luma_minmax_64bin_fw(void)
{
	int i;
	int hist_sum;

	param.min_thr_rate = 3;
	param.max_thr_rate = 3;

	int max_thr_rate = param.max_thr_rate; //3/64 = 0.0468
	int min_thr_rate = param.min_thr_rate;
	int vpp_hist_sum = 0;

	for (i = 0; i < DNLP_VPP_HIST_BIN_NUM; i++)
		vpp_hist_sum  += vpp_hist_param.vpp_gamma[i];

	param.max_thr_cnt = (vpp_hist_sum * max_thr_rate + 32) >> 6;
	param.min_thr_cnt = (vpp_hist_sum * min_thr_rate + 32) >> 6;

	hist_sum = 0;
	param.luma_max = 0;
	param.luma_min = 0;

	for (i = 63; i >=  0; i--) {
		hist_sum += vpp_hist_param.vpp_gamma[i];
		if (hist_sum > param.max_thr_cnt) {
			param.luma_max = (i << 2);
			param.hist_sum_max = hist_sum - vpp_hist_param.vpp_gamma[i];
			break;
		}
	}

	hist_sum = 0;
	for (i = 0; i < 64; i++) {
		hist_sum += vpp_hist_param.vpp_gamma[i];
		if (hist_sum > param.min_thr_cnt) {
			param.luma_min = (i << 2);
			param.hist_sum_min = hist_sum - vpp_hist_param.vpp_gamma[i];
			break;
		}
	}
//	pr_amvecm_dbg("luma_min = %d, luma_max = %d, hist_sum_min = %d , hist_sum_max = %d\n",
//		param.luma_min, param.luma_max, param.hist_sum_min, param.hist_sum_max);
}

void get_luma_minmax_4bin_fw(void)
{
	int i;
	int hist_sum;
	int max_thr_cnt = param.max_thr_cnt;
	int min_thr_cnt = param.min_thr_cnt;
	int luma_min_p = param.luma_min;
	int luma_max_p = param.luma_max;
	int hist_sum_min_low = param.hist_sum_min - vpp_hist_param.vpp_6binhist_min[0];
	int hist_sum_max_low = param.hist_sum_max - vpp_hist_param.vpp_6binhist_max[5];
	int hist_sum_min_high = hist_sum_min_low;
	int hist_sum_max_high = hist_sum_max_low;

	for (i = 0; i < 6; i++) {
		hist_sum_min_high += vpp_hist_param.vpp_6binhist_min[i];
		hist_sum_max_high += vpp_hist_param.vpp_6binhist_max[i];
	}

	if (max_thr_cnt > hist_sum_max_low && max_thr_cnt <= hist_sum_max_high) {
		hist_sum = hist_sum_max_low;
		for (i = 5; i >= 0; i--) {
			hist_sum += vpp_hist_param.vpp_6binhist_max[i];

			if (hist_sum > param.max_thr_cnt) {
				param.vpp_luma_max = luma_max_p + (i - 1);
				break;
			}
		}
	} else {
		param.vpp_luma_max = luma_max_p;
	}

	if (min_thr_cnt > hist_sum_min_low && min_thr_cnt <= hist_sum_min_high) {
		hist_sum = hist_sum_min_low;
		for (i = 0; i < 6; i++) {
			hist_sum += vpp_hist_param.vpp_6binhist_min[i];

			if (hist_sum > param.min_thr_cnt) {
				param.vpp_luma_min = luma_min_p + (i - 1);
				break;
			}
		}
	} else {
		param.vpp_luma_min = luma_min_p;
	}

	WRITE_VPP_REG_BITS(DNLP_HIST_MINMAX_8B, param.vpp_luma_max, 0, 8);
	WRITE_VPP_REG_BITS(DNLP_HIST_MINMAX_8B, param.vpp_luma_min, 8, 16);

	pr_amvecm_dbg("\n");
//	for (i = 0; i < 6; i++)
//		pr_amvecm_dbg("i = %d, mincnt = %d  maxcnt = %d\n",
//		i, vpp_hist_param.vpp_6binhist_min[i], vpp_hist_param.vpp_6binhist_max[i]);

//	pr_amvecm_dbg("min_thr_cnt = %d, max_thr_cnt = %d\n", param.min_thr_cnt,
//		param.max_thr_cnt);

//	pr_amvecm_dbg("vpp_luma_min = %d, vpp_luma_max = %d\n", param.vpp_luma_min,
//		param.vpp_luma_max);
}

void vpp_hist_shft_cfg(struct vframe_s *vf)
{
	unsigned int h_rate = 0;
	unsigned int w_rate = 0;
	unsigned int src_h = 0;
	unsigned int src_w = 0;
	unsigned int val = 2;
	unsigned int height;
	unsigned int width;
	static unsigned int pre_val;

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		vpp_hist_shft_cfg_v2(vf);
		return;
	}

	if (!vf) {
		if (pre_val != val) {
			VSYNC_WRITE_VPP_REG_BITS(VI_HIST_CTRL, val,
				VI_HIST_POW_BIT, VI_HIST_POW_WID);
			pre_val = val;
			pr_amvecm_dbg("no vf, pre_val=%d, val=%d\n", pre_val, val);
		}
		return;
	}

	w_rate = vf->dpss_pps_dsx;
	h_rate = vf->dpss_pps_dsy;

	height = (vf->type & VIDTYPE_COMPRESS) ? vf->compHeight : vf->height;
	width = (vf->type & VIDTYPE_COMPRESS) ? vf->compWidth : vf->width;

	if (h_rate)
		src_h = height / (h_rate + 1);
	else
		src_h = height;

	if (w_rate)
		src_w = width / (w_rate + 1);
	else
		src_w = width;

	if (src_w >= 3840 && src_h >= 2160)
		val = 4;
	else if ((src_w >= 3840 && src_h >= 1080) ||
		(src_w >= 2560 && src_h >= 1440))
		val = 3;
	else
		val = 2;

	if (pre_val != val) {
		VSYNC_WRITE_VPP_REG_BITS(VI_HIST_CTRL, val,
			VI_HIST_POW_BIT, VI_HIST_POW_WID);
		pre_val = val;
		pr_amvecm_dbg("src_w=%d, src_h=%d, pre_val=%d, val=%d\n",
			height, width, pre_val, val);
	}
}

void vpp_get_vframe_hist_info(struct vframe_s *vf,
	struct vpp_hist_param_s *vp, int vpp_index)
{
	unsigned int hist_height, hist_width;
	u64 divid;

	if (!vf)
		return;

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		get_luma_hist(vf, vp, vpp_index);
		get_cm_hist(vp);
		return;
	}

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		/*TL1 remove VPP_IN_H_V_SIZE register*/
		hist_width = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 0, 13);
		hist_height = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 16, 13);
	} else {
		hist_height = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 0, 13);
		hist_width = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 16, 13);
	}

	if (hist_height != pre_hist_height ||
	    hist_width != pre_hist_width) {
		pre_hist_height = hist_height;
		pre_hist_width = hist_width;
		WRITE_VPP_REG_BITS(VI_HIST_PIC_SIZE, hist_height, 16, 13);
		WRITE_VPP_REG_BITS(VI_HIST_PIC_SIZE, hist_width, 0, 13);
	}
	/* fetch hist info */
	/* vf->prop.hist.luma_sum   = READ_CBUS_REG_BITS(VDIN_HIST_SPL_VAL,*/
	/* HIST_LUMA_SUM_BIT,    HIST_LUMA_SUM_WID   ); */
	vf->prop.hist.hist_pow   =
	READ_VPP_REG_BITS(VI_HIST_CTRL,
			  VI_HIST_POW_BIT, VI_HIST_POW_WID);
	vp->vpp_luma_sum   =
	READ_VPP_REG(VI_HIST_SPL_VAL);
	/* vf->prop.hist.chroma_sum = READ_CBUS_REG_BITS(VDIN_HIST_CHROMA_SUM,*/
	/* HIST_CHROMA_SUM_BIT,  HIST_CHROMA_SUM_WID ); */
	vp->vpp_chroma_sum =
	READ_VPP_REG(VI_HIST_CHROMA_SUM);
	vp->vpp_pixel_sum  =
	READ_VPP_REG_BITS(VI_HIST_SPL_PIX_CNT,
			  VI_HIST_PIX_CNT_BIT, VI_HIST_PIX_CNT_WID);
	vp->vpp_height     =
	READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			  VI_HIST_PIC_HEIGHT_BIT, VI_HIST_PIC_HEIGHT_WID);
	vp->vpp_width      =
	READ_VPP_REG_BITS(VI_HIST_PIC_SIZE,
			  VI_HIST_PIC_WIDTH_BIT, VI_HIST_PIC_WIDTH_WID);
	vp->vpp_luma_max   =
	READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			  VI_HIST_MAX_BIT, VI_HIST_MAX_WID);
	vp->vpp_luma_min   =
	READ_VPP_REG_BITS(VI_HIST_MAX_MIN,
			  VI_HIST_MIN_BIT, VI_HIST_MIN_WID);
	vp->vpp_gamma[0]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST00,
			  VI_HIST_ON_BIN_00_BIT, VI_HIST_ON_BIN_00_WID);
	vp->vpp_gamma[1]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST00,
			  VI_HIST_ON_BIN_01_BIT, VI_HIST_ON_BIN_01_WID);
	vp->vpp_gamma[2]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST01,
			  VI_HIST_ON_BIN_02_BIT, VI_HIST_ON_BIN_02_WID);
	vp->vpp_gamma[3]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST01,
			  VI_HIST_ON_BIN_03_BIT, VI_HIST_ON_BIN_03_WID);
	vp->vpp_gamma[4]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST02,
			  VI_HIST_ON_BIN_04_BIT, VI_HIST_ON_BIN_04_WID);
	vp->vpp_gamma[5]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST02,
			  VI_HIST_ON_BIN_05_BIT, VI_HIST_ON_BIN_05_WID);
	vp->vpp_gamma[6]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST03,
			  VI_HIST_ON_BIN_06_BIT, VI_HIST_ON_BIN_06_WID);
	vp->vpp_gamma[7]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST03,
			  VI_HIST_ON_BIN_07_BIT, VI_HIST_ON_BIN_07_WID);
	vp->vpp_gamma[8]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST04,
			  VI_HIST_ON_BIN_08_BIT, VI_HIST_ON_BIN_08_WID);
	vp->vpp_gamma[9]   =
	READ_VPP_REG_BITS(VI_DNLP_HIST04,
			  VI_HIST_ON_BIN_09_BIT, VI_HIST_ON_BIN_09_WID);
	vp->vpp_gamma[10]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST05,
			  VI_HIST_ON_BIN_10_BIT, VI_HIST_ON_BIN_10_WID);
	vp->vpp_gamma[11]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST05,
			  VI_HIST_ON_BIN_11_BIT, VI_HIST_ON_BIN_11_WID);
	vp->vpp_gamma[12]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST06,
			  VI_HIST_ON_BIN_12_BIT, VI_HIST_ON_BIN_12_WID);
	vp->vpp_gamma[13]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST06,
			  VI_HIST_ON_BIN_13_BIT, VI_HIST_ON_BIN_13_WID);
	vp->vpp_gamma[14]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST07,
			  VI_HIST_ON_BIN_14_BIT, VI_HIST_ON_BIN_14_WID);
	vp->vpp_gamma[15]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST07,
			  VI_HIST_ON_BIN_15_BIT, VI_HIST_ON_BIN_15_WID);
	vp->vpp_gamma[16]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST08,
			  VI_HIST_ON_BIN_16_BIT, VI_HIST_ON_BIN_16_WID);
	vp->vpp_gamma[17]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST08,
			  VI_HIST_ON_BIN_17_BIT, VI_HIST_ON_BIN_17_WID);
	vp->vpp_gamma[18]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST09,
			  VI_HIST_ON_BIN_18_BIT, VI_HIST_ON_BIN_18_WID);
	vp->vpp_gamma[19]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST09,
			  VI_HIST_ON_BIN_19_BIT, VI_HIST_ON_BIN_19_WID);
	vp->vpp_gamma[20]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST10,
			  VI_HIST_ON_BIN_20_BIT, VI_HIST_ON_BIN_20_WID);
	vp->vpp_gamma[21]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST10,
			  VI_HIST_ON_BIN_21_BIT, VI_HIST_ON_BIN_21_WID);
	vp->vpp_gamma[22]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST11,
			  VI_HIST_ON_BIN_22_BIT, VI_HIST_ON_BIN_22_WID);
	vp->vpp_gamma[23]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST11,
			  VI_HIST_ON_BIN_23_BIT, VI_HIST_ON_BIN_23_WID);
	vp->vpp_gamma[24]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST12,
			  VI_HIST_ON_BIN_24_BIT, VI_HIST_ON_BIN_24_WID);
	vp->vpp_gamma[25]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST12,
			  VI_HIST_ON_BIN_25_BIT, VI_HIST_ON_BIN_25_WID);
	vp->vpp_gamma[26]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST13,
			  VI_HIST_ON_BIN_26_BIT, VI_HIST_ON_BIN_26_WID);
	vp->vpp_gamma[27]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST13,
			  VI_HIST_ON_BIN_27_BIT, VI_HIST_ON_BIN_27_WID);
	vp->vpp_gamma[28]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST14,
			  VI_HIST_ON_BIN_28_BIT, VI_HIST_ON_BIN_28_WID);
	vp->vpp_gamma[29]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST14,
			  VI_HIST_ON_BIN_29_BIT, VI_HIST_ON_BIN_29_WID);
	vp->vpp_gamma[30]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST15,
			  VI_HIST_ON_BIN_30_BIT, VI_HIST_ON_BIN_30_WID);
	vp->vpp_gamma[31]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST15,
			  VI_HIST_ON_BIN_31_BIT, VI_HIST_ON_BIN_31_WID);
	vp->vpp_gamma[32]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST16,
			  VI_HIST_ON_BIN_32_BIT, VI_HIST_ON_BIN_32_WID);
	vp->vpp_gamma[33]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST16,
			  VI_HIST_ON_BIN_33_BIT, VI_HIST_ON_BIN_33_WID);
	vp->vpp_gamma[34]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST17,
			  VI_HIST_ON_BIN_34_BIT, VI_HIST_ON_BIN_34_WID);
	vp->vpp_gamma[35]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST17,
			  VI_HIST_ON_BIN_35_BIT, VI_HIST_ON_BIN_35_WID);
	vp->vpp_gamma[36]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST18,
			  VI_HIST_ON_BIN_36_BIT, VI_HIST_ON_BIN_36_WID);
	vp->vpp_gamma[37]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST18,
			  VI_HIST_ON_BIN_37_BIT, VI_HIST_ON_BIN_37_WID);
	vp->vpp_gamma[38]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST19,
			  VI_HIST_ON_BIN_38_BIT, VI_HIST_ON_BIN_38_WID);
	vp->vpp_gamma[39]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST19,
			  VI_HIST_ON_BIN_39_BIT, VI_HIST_ON_BIN_39_WID);
	vp->vpp_gamma[40]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST20,
			  VI_HIST_ON_BIN_40_BIT, VI_HIST_ON_BIN_40_WID);
	vp->vpp_gamma[41]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST20,
			  VI_HIST_ON_BIN_41_BIT, VI_HIST_ON_BIN_41_WID);
	vp->vpp_gamma[42]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST21,
			  VI_HIST_ON_BIN_42_BIT, VI_HIST_ON_BIN_42_WID);
	vp->vpp_gamma[43]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST21,
			  VI_HIST_ON_BIN_43_BIT, VI_HIST_ON_BIN_43_WID);
	vp->vpp_gamma[44]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST22,
			  VI_HIST_ON_BIN_44_BIT, VI_HIST_ON_BIN_44_WID);
	vp->vpp_gamma[45]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST22,
			  VI_HIST_ON_BIN_45_BIT, VI_HIST_ON_BIN_45_WID);
	vp->vpp_gamma[46]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST23,
			  VI_HIST_ON_BIN_46_BIT, VI_HIST_ON_BIN_46_WID);
	vp->vpp_gamma[47]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST23,
			  VI_HIST_ON_BIN_47_BIT, VI_HIST_ON_BIN_47_WID);
	vp->vpp_gamma[48]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST24,
			  VI_HIST_ON_BIN_48_BIT, VI_HIST_ON_BIN_48_WID);
	vp->vpp_gamma[49]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST24,
			  VI_HIST_ON_BIN_49_BIT, VI_HIST_ON_BIN_49_WID);
	vp->vpp_gamma[50]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST25,
			  VI_HIST_ON_BIN_50_BIT, VI_HIST_ON_BIN_50_WID);
	vp->vpp_gamma[51]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST25,
			  VI_HIST_ON_BIN_51_BIT, VI_HIST_ON_BIN_51_WID);
	vp->vpp_gamma[52]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST26,
			  VI_HIST_ON_BIN_52_BIT, VI_HIST_ON_BIN_52_WID);
	vp->vpp_gamma[53]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST26,
			  VI_HIST_ON_BIN_53_BIT, VI_HIST_ON_BIN_53_WID);
	vp->vpp_gamma[54]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST27,
			  VI_HIST_ON_BIN_54_BIT, VI_HIST_ON_BIN_54_WID);
	vp->vpp_gamma[55]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST27,
			  VI_HIST_ON_BIN_55_BIT, VI_HIST_ON_BIN_55_WID);
	vp->vpp_gamma[56]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST28,
			  VI_HIST_ON_BIN_56_BIT, VI_HIST_ON_BIN_56_WID);
	vp->vpp_gamma[57]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST28,
			  VI_HIST_ON_BIN_57_BIT, VI_HIST_ON_BIN_57_WID);
	vp->vpp_gamma[58]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST29,
			  VI_HIST_ON_BIN_58_BIT, VI_HIST_ON_BIN_58_WID);
	vp->vpp_gamma[59]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST29,
			  VI_HIST_ON_BIN_59_BIT, VI_HIST_ON_BIN_59_WID);
	vp->vpp_gamma[60]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST30,
			  VI_HIST_ON_BIN_60_BIT, VI_HIST_ON_BIN_60_WID);
	vp->vpp_gamma[61]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST30,
			  VI_HIST_ON_BIN_61_BIT, VI_HIST_ON_BIN_61_WID);
	vp->vpp_gamma[62]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST31,
			  VI_HIST_ON_BIN_62_BIT, VI_HIST_ON_BIN_62_WID);
	vp->vpp_gamma[63]  =
	READ_VPP_REG_BITS(VI_DNLP_HIST31,
			  VI_HIST_ON_BIN_63_BIT, VI_HIST_ON_BIN_63_WID);

	if (chip_type_id == chip_t6x) {
		/*get dnlp hist 6bin min*/
		vp->vpp_6binhist_min[0]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MIN_0_1,
				  VI_HIST_ON_BIN_00_BIT, VI_HIST_ON_BIN_00_WID);
		vp->vpp_6binhist_min[1]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MIN_0_1,
				  VI_HIST_ON_BIN_01_BIT, VI_HIST_ON_BIN_01_WID);
		vp->vpp_6binhist_min[2]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MIN_2_3,
				  VI_HIST_ON_BIN_02_BIT, VI_HIST_ON_BIN_02_WID);
		vp->vpp_6binhist_min[3]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MIN_2_3,
				  VI_HIST_ON_BIN_03_BIT, VI_HIST_ON_BIN_03_WID);
		vp->vpp_6binhist_min[4]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MIN_4_5,
				  VI_HIST_ON_BIN_04_BIT, VI_HIST_ON_BIN_04_WID);
		vp->vpp_6binhist_min[5]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MIN_4_5,
				  VI_HIST_ON_BIN_05_BIT, VI_HIST_ON_BIN_05_WID);

		/*get dnlp hist 6bin max*/
		vp->vpp_6binhist_max[0]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MAX_0_1,
				  VI_HIST_ON_BIN_00_BIT, VI_HIST_ON_BIN_00_WID);
		vp->vpp_6binhist_max[1]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MAX_0_1,
				  VI_HIST_ON_BIN_01_BIT, VI_HIST_ON_BIN_01_WID);
		vp->vpp_6binhist_max[2]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MAX_2_3,
				  VI_HIST_ON_BIN_02_BIT, VI_HIST_ON_BIN_02_WID);
		vp->vpp_6binhist_max[3]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MAX_2_3,
				  VI_HIST_ON_BIN_03_BIT, VI_HIST_ON_BIN_03_WID);
		vp->vpp_6binhist_max[4]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MAX_4_5,
				  VI_HIST_ON_BIN_04_BIT, VI_HIST_ON_BIN_04_WID);
		vp->vpp_6binhist_max[5]   =
		READ_VPP_REG_BITS(DNLP_HIST_6BIN_MAX_4_5,
				  VI_HIST_ON_BIN_05_BIT, VI_HIST_ON_BIN_05_WID);
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
		chip_type_id == chip_t5m) {
		unsigned int h_rate = vf->dpss_pps_dsy;
		unsigned int w_rate = vf->dpss_pps_dsx;
		unsigned int src_h = (h_rate) ? (vf->height / (h_rate + 1)) : vf->height;
		unsigned int src_w = (w_rate) ? (vf->width / (w_rate + 1)) : vf->width;

		vf->fmeter0_hcnt[0] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE0);
		vf->fmeter0_hcnt[1] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE1);
		vf->fmeter0_hcnt[2] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE2);
		vf->fmeter0_hcnt[3] =
		READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE3);
		vf->fmeter1_hcnt[0] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE0);
		vf->fmeter1_hcnt[1] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE1);
		vf->fmeter1_hcnt[2] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE2);
		vf->fmeter1_hcnt[3] =
		READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE3);

		data_meter.fmeter0_hcnt[0] = READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE0);
		data_meter.fmeter0_hcnt[1] = READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE1);
		data_meter.fmeter0_hcnt[2] = READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE2);
		data_meter.fmeter0_hcnt[3] = READ_VPP_REG(SRSHARP0_RO_FMETER_HCNT_TYPE3);

		data_meter.fmeter0_vcnt[0] = READ_VPP_REG(SRSHARP0_RO_FMETER_VCNT_TYPE0);
		data_meter.fmeter0_vcnt[1] = READ_VPP_REG(SRSHARP0_RO_FMETER_VCNT_TYPE1);
		data_meter.fmeter0_vcnt[2] = READ_VPP_REG(SRSHARP0_RO_FMETER_VCNT_TYPE2);
		data_meter.fmeter0_vcnt[3] = READ_VPP_REG(SRSHARP0_RO_FMETER_VCNT_TYPE3);

		data_meter.fmeter0_pdcnt[0] = READ_VPP_REG(SRSHARP0_RO_FMETER_PDCNT_TYPE0);
		data_meter.fmeter0_pdcnt[1] = READ_VPP_REG(SRSHARP0_RO_FMETER_PDCNT_TYPE1);
		data_meter.fmeter0_pdcnt[2] = READ_VPP_REG(SRSHARP0_RO_FMETER_PDCNT_TYPE2);
		data_meter.fmeter0_pdcnt[3] = READ_VPP_REG(SRSHARP0_RO_FMETER_PDCNT_TYPE3);

		data_meter.fmeter0_ndcnt[0] = READ_VPP_REG(SRSHARP0_RO_FMETER_NDCNT_TYPE0);
		data_meter.fmeter0_ndcnt[1] = READ_VPP_REG(SRSHARP0_RO_FMETER_NDCNT_TYPE1);
		data_meter.fmeter0_ndcnt[2] = READ_VPP_REG(SRSHARP0_RO_FMETER_NDCNT_TYPE2);
		data_meter.fmeter0_ndcnt[3] = READ_VPP_REG(SRSHARP0_RO_FMETER_NDCNT_TYPE3);

		data_meter.fmeter1_hcnt[0] = READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE0);
		data_meter.fmeter1_hcnt[1] = READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE1);
		data_meter.fmeter1_hcnt[2] = READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE2);
		data_meter.fmeter1_hcnt[3] = READ_VPP_REG(SRSHARP1_RO_FMETER_HCNT_TYPE3);

		data_meter.fmeter1_vcnt[0] = READ_VPP_REG(SRSHARP1_RO_FMETER_VCNT_TYPE0);
		data_meter.fmeter1_vcnt[1] = READ_VPP_REG(SRSHARP1_RO_FMETER_VCNT_TYPE1);
		data_meter.fmeter1_vcnt[2] = READ_VPP_REG(SRSHARP1_RO_FMETER_VCNT_TYPE2);
		data_meter.fmeter1_vcnt[3] = READ_VPP_REG(SRSHARP1_RO_FMETER_VCNT_TYPE3);

		data_meter.fmeter1_pdcnt[0] = READ_VPP_REG(SRSHARP1_RO_FMETER_PDCNT_TYPE0);
		data_meter.fmeter1_pdcnt[1] = READ_VPP_REG(SRSHARP1_RO_FMETER_PDCNT_TYPE1);
		data_meter.fmeter1_pdcnt[2] = READ_VPP_REG(SRSHARP1_RO_FMETER_PDCNT_TYPE2);
		data_meter.fmeter1_pdcnt[3] = READ_VPP_REG(SRSHARP1_RO_FMETER_PDCNT_TYPE3);

		data_meter.fmeter1_ndcnt[0] = READ_VPP_REG(SRSHARP1_RO_FMETER_NDCNT_TYPE0);
		data_meter.fmeter1_ndcnt[1] = READ_VPP_REG(SRSHARP1_RO_FMETER_NDCNT_TYPE1);
		data_meter.fmeter1_ndcnt[2] = READ_VPP_REG(SRSHARP1_RO_FMETER_NDCNT_TYPE2);
		data_meter.fmeter1_ndcnt[3] = READ_VPP_REG(SRSHARP1_RO_FMETER_NDCNT_TYPE3);

		if (fmeter_drv_param.fmeter_cal_score) {
			fmeter_drv_param.fmeter_cal_score(src_w, src_h);
			vf->fmeter0_score = fmeter_drv_param.fmeter0_score;
			vf->fmeter1_score = fmeter_drv_param.fmeter1_score;
		}
	}

	get_cm_hist(vp);
	get_dark_luma_hist(vp, vf);

	if (debug_game_mode_1 &&
	    vpp_luma_max != vp->vpp_luma_max) {
		divid = sched_clock();
		vf->ready_clock_hist[1] = sched_clock();
		do_div(divid, 1000);
		pr_info("vpp output done %lld us. luma_max(0x%x-->0x%x)\n",
			divid, vpp_luma_max, vp->vpp_luma_max);
		vpp_luma_max = vp->vpp_luma_max;
	}
}

static void ioctrl_get_hdr_metadata(struct vframe_s *vf)
{
	if (((vf->signal_type >> 16) & 0xff) == 9) {
		if (vf->prop.master_display_colour.present_flag) {
			memcpy(vpp_hdr_metadata_s.primaries,
			       vf->prop.master_display_colour.primaries,
				sizeof(u32) * 6);
			memcpy(vpp_hdr_metadata_s.white_point,
			       vf->prop.master_display_colour.white_point,
				sizeof(u32) * 2);
			vpp_hdr_metadata_s.luminance[0] =
				vf->prop.master_display_colour.luminance[0];
			vpp_hdr_metadata_s.luminance[1] =
				vf->prop.master_display_colour.luminance[1];
		} else {
			memset(vpp_hdr_metadata_s.primaries, 0,
			       sizeof(vpp_hdr_metadata_s.primaries));
		}

		if (vf->prop.master_display_colour.content_light_level.present_flag) {
			vpp_hdr_metadata_s.content_light_level.present_flag = 1;
			vpp_hdr_metadata_s.content_light_level.max_content =
				vf->prop.master_display_colour.content_light_level.max_content;
			vpp_hdr_metadata_s.content_light_level.max_pic_average =
				vf->prop.master_display_colour.content_light_level.max_pic_average;
		} else {
			vpp_hdr_metadata_s.content_light_level.present_flag = 0;
			vpp_hdr_metadata_s.content_light_level.max_content = 0;
			vpp_hdr_metadata_s.content_light_level.max_pic_average = 0;
		}
	} else {
		memset(vpp_hdr_metadata_s.primaries, 0,
		       sizeof(vpp_hdr_metadata_s.primaries));
		vpp_hdr_metadata_s.content_light_level.present_flag = 0;
		vpp_hdr_metadata_s.content_light_level.max_content = 0;
		vpp_hdr_metadata_s.content_light_level.max_pic_average = 0;
	}
}

void vpp_demo_func(struct vframe_s *vf,
	struct vpq_size_s *pvpq_size, int vpp_index)
{
	struct demo_data_s *p = &demo_data;
	unsigned int val, val1;
	int cm_size_chg = 0;
	int sr_size_chg = 0;
	int sps_hsize;
	int sps_vsize;
	int cm_hsize;
	int cm_vsize;

	sps_hsize = pvpq_size->sr1_in_hsize;
	sps_vsize = pvpq_size->sr1_in_vsize;
	cm_hsize = pvpq_size->cm_hsize;
	cm_vsize = pvpq_size->cm_vsize;

	if (!vf) {
		if (p->en_st[E_DEMO_SR] &&
			!p->update_flag[E_DEMO_SR])
			p->update_flag[E_DEMO_SR] = 1;

		if (p->en_st[E_DEMO_CM] &&
			!p->update_flag[E_DEMO_CM])
			p->update_flag[E_DEMO_CM] = 1;

		if (p->en_st[E_DEMO_LC] &&
			!p->update_flag[E_DEMO_LC])
			p->update_flag[E_DEMO_LC] = 1;

		return;
	}

	/*because frame size delay one frame, add size change*/
	if (p->in_size.cm_hsize != cm_hsize ||
		p->in_size.cm_vsize != cm_vsize) {
		p->in_size.cm_hsize = cm_hsize;
		p->in_size.cm_vsize = cm_vsize;
		cm_size_chg = 1;
	}

	if (p->in_size.sr_hsize != sps_hsize ||
		p->in_size.sr_vsize != sps_vsize) {
		p->in_size.sr_hsize = sps_hsize;
		p->in_size.sr_vsize = sps_vsize;
		sr_size_chg = 1;
	}

	switch (chip_type_id) {
	case chip_s7:
	case chip_s7d:
	case chip_s6:
	case chip_t6d:
	case chip_t6w:
		if (p->update_flag[E_DEMO_SR] ||
			(cm_size_chg && p->en_st[E_DEMO_SR])) {
			if (p->en_st[E_DEMO_SR]) {
				val = 0;
				val1 = (cm_hsize >> 1) | (cm_vsize << 16);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DEBUG_DEMO_WND_COEF_0,
					val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DEBUG_DEMO_WND_COEF_1,
					val1, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DEBUG_DEMO_WND_EN,
					0x11, vpp_index);
			} else {
				val = 0;
				val1 = (cm_hsize >> 1) | (cm_vsize << 16);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DEBUG_DEMO_WND_COEF_0,
					val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DEBUG_DEMO_WND_COEF_1,
					val1, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DEBUG_DEMO_WND_EN,
					0x0, vpp_index);
			}
			p->update_flag[E_DEMO_SR] = 0;
			pr_amvecm_dbg("sr hsize: %d, vsize: %d, sr_demo = %d\n",
				cm_hsize, cm_vsize, p->en_st[E_DEMO_SR]);
		}

		if (p->update_flag[E_DEMO_CM] ||
			(cm_size_chg && p->en_st[E_DEMO_CM])) {
			if (p->en_st[E_DEMO_CM]) {
				val = (p->in_size.cm_hsize >> 1) << 16;
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x209, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, val, vpp_index);
				val = p->in_size.cm_vsize << 16;
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x20a, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x20f, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, 0x40, vpp_index);
			} else {
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x20f, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, 0x0, vpp_index);
			}
			p->update_flag[E_DEMO_CM] = 0;
			pr_amvecm_dbg("cm hsize: %d, vsize: %d, cm_demo = %d\n",
				cm_hsize, cm_vsize, p->en_st[E_DEMO_CM]);
		}
		/*dnlp/lc same demo reg*/
		if (p->update_flag[E_DEMO_LC] ||
			(cm_size_chg && p->en_st[E_DEMO_LC])) {
			if (p->en_st[E_DEMO_LC]) {
				val = 0;
				val1 = (cm_hsize >> 1) | (cm_vsize << 16);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CONTRAST_DEBUG_DEMO_WND_COEF_0,
					val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CONTRAST_DEBUG_DEMO_WND_COEF_1,
					val1, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CONTRAST_DEBUG_DEMO_WND_EN,
					0x11, vpp_index);
			} else {
				val = 0;
				val1 = (cm_hsize >> 1) | (cm_vsize << 16);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CONTRAST_DEBUG_DEMO_WND_COEF_0,
					val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CONTRAST_DEBUG_DEMO_WND_COEF_1,
					val1, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CONTRAST_DEBUG_DEMO_WND_EN,
					0x0, vpp_index);
			}
			p->update_flag[E_DEMO_LC] = 0;
			pr_amvecm_dbg("lc_demo = %d\n", lc_demo_mode);
		}

		break;
	default:
		if (p->update_flag[E_DEMO_SR] ||
			(sr_size_chg && p->en_st[E_DEMO_SR])) {
			if (p->en_st[E_DEMO_SR]) {
				val = (2 << 17) |
					(1 << 16) |
					(p->in_size.sr_hsize >> 1);
				VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_DEMO_CRTL, val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_DEMO_CRTL, val, vpp_index);
			} else {
				val = (2 << 17) |
					(0 << 16) |
					(p->in_size.sr_hsize >> 1);
				VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_DEMO_CRTL, val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_DEMO_CRTL, val, vpp_index);
			}
			p->update_flag[E_DEMO_SR] = 0;
			pr_amvecm_dbg("sr hsize: %d, vsize: %d, sr_demo = %d\n",
				sps_hsize, sps_vsize, p->en_st[E_DEMO_SR]);
		}

		if (p->update_flag[E_DEMO_CM] ||
			(cm_size_chg && p->en_st[E_DEMO_CM])) {
			if (p->en_st[E_DEMO_CM]) {
				val = (p->in_size.cm_hsize >> 1) << 16;
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x209, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, val, vpp_index);
				val = p->in_size.cm_vsize << 16;
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x20a, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, val, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x20f, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, 0x40, vpp_index);
			} else {
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_ADDR_PORT, 0x20f, vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_CHROMA_DATA_PORT, 0x0, vpp_index);
			}
			p->update_flag[E_DEMO_CM] = 0;
			pr_amvecm_dbg("cm hsize: %d, vsize: %d, cm_demo = %d\n",
				cm_hsize, cm_vsize, p->en_st[E_DEMO_CM]);
		}

		if (p->update_flag[E_DEMO_LC]) {
			if (p->en_st[E_DEMO_LC])
				lc_demo_mode = 1;
			else
				lc_demo_mode = 0;
			p->update_flag[E_DEMO_LC] = 0;
			pr_amvecm_dbg("lc_demo = %d\n", lc_demo_mode);
		}
		break;
	}
}

void amvecm_dejaggy_patch(struct vframe_s *vf)
{
	int gmv;
	unsigned int h_rate = 0;
	unsigned int w_rate = 0;
	unsigned int src_h = 0;
	unsigned int src_w = 0;

	if (!vf) {
		if (pd_detect_en)
			pd_detect_en = 0;
		return;
	}

	w_rate = vf->dpss_pps_dsx;
	h_rate = vf->dpss_pps_dsy;

	if (h_rate)
		src_h = vf->height / (h_rate + 1);
	else
		src_h = vf->height;

	if (w_rate)
		src_w = vf->width / (w_rate + 1);
	else
		src_w = vf->width;

	if (!pd_det)
		return;

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		return;

	gmv = vf->di_gmv / 10000;

	if (src_h == 1080 &&
	    src_w == 1920 &&
	    (vf->di_pulldown & (1 << 3)) &&
	    ((vf->di_pulldown & 0x7) || gmv >= gmv_th)) {
		if (pd_detect_en == 1)
			return;
		pd_detect_en = 1;
		pd_combing_fix_patch(pd_fix_lvl);
		pr_amvecm_dbg("pd_detect_en1 = %d; level = %d, vf->di_pulldown = 0x%x, gmv %d\n",
			      pd_detect_en, pd_fix_lvl, vf->di_pulldown, gmv);
	} else if ((src_h == 1080) &&
		 (src_w == 1920) &&
		 (vf->di_pulldown & (1 << 3)) &&
		 (gmv >= gmv_weak_th)) {
		if (pd_detect_en == 2)
			return;
		pd_detect_en = 2;

		pd_combing_fix_patch(pd_weak_fix_lvl);
		pr_amvecm_dbg("pd_detect_en2 = %d; level = %d, vf->di_pulldown = 0x%x, gmv %d\n",
			      pd_detect_en, pd_weak_fix_lvl, vf->di_pulldown, gmv);
	} else if (pd_detect_en) {
		pd_detect_en = 0;
		pd_combing_fix_patch(PD_DEF_LVL);
		pr_amvecm_dbg("pd_detect_en = %d; pd_fix_lvl = %d\n",
			      pd_detect_en, pd_fix_lvl);
	}
}

struct aml_fmeter_drv_param_s *fmeter_drv_param_get(void)
{
	return &fmeter_drv_param;
}
EXPORT_SYMBOL(fmeter_drv_param_get);

/*sr fmeter interface*/
void amvecm_fmeter_process(struct vframe_s *vf, int vpp_index)
{
	uint val;
	uint reg_val;

	if (!vf)
		return;

	if (fmeter_slt)
		return;

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	if (fmeter_debug == 1)
		frc_set_seg_display(1, fmeter_drv_param.fmeter_score_hundred,
			fmeter_drv_param.fmeter_score_ten,
			fmeter_drv_param.fmeter_score_unit);
	if (fmeter_debug == 0)
		frc_set_seg_display(0, 0, 0, 0);
#endif

	if (fmeter_drv_param.cur_fmeter_level == pre_fmeter_level)
		fmeter_flag++;
	else
		fmeter_flag = 0;

	if (fmeter_flag == fmeter_count && fmeter_en) {
		fmeter_flag = 0;
		if (cur_sr_level < fmeter_drv_param.cur_fmeter_level)
			cur_sr_level++;
		else if (cur_sr_level > fmeter_drv_param.cur_fmeter_level)
			cur_sr_level--;
		else
			return;

		val = sr1_gain_val[cur_sr_level] << 24 |
			sr1_gain_val[cur_sr_level] << 16 |
			sr1_gain_val[cur_sr_level] << 8 |
			sr1_gain_val[cur_sr_level];
		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_SR7_PKLONG_PF_GAIN, val, vpp_index);

		reg_val = READ_VPP_REG(SRSHARP1_PK_OS_STATIC);

		reg_val &= ~(0x3ff | (0x3ff << 12));
		val = reg_val |
			((sr1_shoot_val[cur_sr_level] & 0x3ff) << 12) |
			(sr1_shoot_val[cur_sr_level] & 0x3ff);

		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_PK_OS_STATIC, val, vpp_index);

		val = sr0_gain_val[cur_sr_level] << 24 |
			sr0_gain_val[cur_sr_level] << 16 |
			sr0_gain_val[cur_sr_level] << 8 |
			sr0_gain_val[cur_sr_level];
		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_SR7_PKLONG_PF_GAIN, val, vpp_index);

		reg_val = READ_VPP_REG(SRSHARP0_PK_OS_STATIC);
		reg_val &= ~(0x3ff | (0x3ff << 12));
		val = reg_val |
			((sr0_shoot_val[cur_sr_level] & 0x3ff) << 12) |
			(sr0_shoot_val[cur_sr_level] & 0x3ff);

		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_PK_OS_STATIC, val, vpp_index);

		reg_val = READ_VPP_REG(SRSHARP1_PK_CON_2CIRHPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr1_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_PK_CON_2CIRHPGAIN_LIMIT, val, vpp_index);


		reg_val = READ_VPP_REG(SRSHARP1_PK_CON_2CIRBPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr1_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_PK_CON_2CIRBPGAIN_LIMIT, val, vpp_index);

		reg_val = READ_VPP_REG(SRSHARP0_PK_CON_2CIRHPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr0_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_PK_CON_2CIRHPGAIN_LIMIT, val, vpp_index);

		reg_val = READ_VPP_REG(SRSHARP0_PK_CON_2CIRBPGAIN_LIMIT);
		reg_val &= ~(0xff << 24);
		val = reg_val |
			((sr0_gain_lmt[cur_sr_level] & 0xff) << 24);

		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_PK_CON_2CIRBPGAIN_LIMIT, val, vpp_index);

		if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3) {
			reg_val = READ_VPP_REG(NN_ADP_CORING);
			reg_val &= ~(0xff << 8);
			val = reg_val |
				((nn_coring[cur_sr_level] & 0xff) << 8);

			VSYNC_WRITE_VPP_REG_VPP_SEL(NN_ADP_CORING, val, vpp_index);
		}
	}
	pre_fmeter_level = fmeter_drv_param.cur_fmeter_level;
}

static int vpp_mtx_update(struct vpp_mtx_info_s *mtx_info, int vpp_index)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;

	enum vpp_matrix_e mtx_sel;
	unsigned int coef00_01 = 0;
	unsigned int coef02_10 = 0;
	unsigned int coef11_12 = 0;
	unsigned int coef20_21 = 0;
	unsigned int coef22 = 0;
	unsigned int clip = 0;
	unsigned int offset0_1 = 0;
	unsigned int offset2 = 0;
	unsigned int pre_offset0_1 = 0;
	unsigned int pre_offset2 = 0;
	unsigned int en = 0;

	mtx_sel = mtx_info->mtx_sel;

	if (flag_lc_evc && mtx_sel == VD1_MTX)
		return 0;

	switch (mtx_sel) {
	case VD1_MTX:
		matrix_coef00_01 = VPP_VD1_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_VD1_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_VD1_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_VD1_MATRIX_COEF20_21;
		matrix_coef22 = VPP_VD1_MATRIX_COEF22;
		matrix_clip = VPP_VD1_MATRIX_CLIP;
		matrix_offset0_1 = VPP_VD1_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_VD1_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_VD1_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_VD1_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_VD1_MATRIX_EN_CTRL;
		break;
	case POST2_MTX:
		matrix_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST2_MATRIX_COEF22;
		matrix_clip = VPP_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST2_MATRIX_EN_CTRL;
		break;
	case POST_MTX:
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST_MATRIX_COEF22;
		matrix_clip = VPP_POST_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;
		break;
	case VPP1_POST2_MTX:
		matrix_coef00_01 = VPP1_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP1_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP1_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP1_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP1_POST2_MATRIX_COEF22;
		matrix_clip = VPP1_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP1_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP1_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP1_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP1_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP1_POST2_MATRIX_EN_CTRL;
		break;
	case VPP2_POST2_MTX:
		matrix_coef00_01 = VPP2_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP2_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP2_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP2_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP2_POST2_MATRIX_COEF22;
		matrix_clip = VPP2_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP2_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP2_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP2_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP2_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP2_POST2_MATRIX_EN_CTRL;
		break;
	case OSD1_HDR_MTX:
		matrix_coef00_01 = OSD1_HDR2_MATRIXI_COEF00_01;
		matrix_coef02_10 = OSD1_HDR2_MATRIXI_COEF02_10;
		matrix_coef11_12 = OSD1_HDR2_MATRIXI_COEF11_12;
		matrix_coef20_21 = OSD1_HDR2_MATRIXI_COEF20_21;
		matrix_coef22 = OSD1_HDR2_MATRIXI_COEF22;
		matrix_clip = OSD1_HDR2_MATRIXI_CLIP;
		matrix_offset0_1 = OSD1_HDR2_MATRIXI_OFFSET0_1;
		matrix_offset2 = OSD1_HDR2_MATRIXI_OFFSET2;
		matrix_pre_offset0_1 = OSD1_HDR2_MATRIXI_PRE_OFFSET0_1;
		matrix_pre_offset2 = OSD1_HDR2_MATRIXI_PRE_OFFSET2;
		matrix_en_ctrl = OSD1_HDR2_MATRIXI_EN_CTRL;
		break;
	case OSD1_PQ_MTX:
		matrix_coef00_01 = OSD1_PQ_MATRIX_COEF00_01;
		matrix_coef02_10 = OSD1_PQ_MATRIX_COEF02_10;
		matrix_coef11_12 = OSD1_PQ_MATRIX_COEF11_12;
		matrix_coef20_21 = OSD1_PQ_MATRIX_COEF20_21;
		matrix_coef22 = OSD1_PQ_MATRIX_COEF22;
		matrix_clip = OSD1_PQ_MATRIX_CLIP;
		matrix_offset0_1 = OSD1_PQ_MATRIX_OFFSET0_1;
		matrix_offset2 = OSD1_PQ_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = OSD1_PQ_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = OSD1_PQ_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = OSD1_PQ_MATRIX_EN_CTRL;
		break;
	case MTX_NULL:
	default:
		break;
	}

	if (!mtx_sel) {
		if (vecm_latch_flag2 & VPP_MARTIX_UPDATE)
			vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE;
		else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_BRIGHT)
			vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_BRIGHT;
		else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_CONTRAST)
			vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_CONTRAST;
		else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_HUE_SAT)
			vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_HUE_SAT;
		else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_EN)
			vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_EN;
		return 0;
	}

	if (vecm_latch_flag2 & VPP_MARTIX_UPDATE) {
		pre_offset0_1 =
			(mtx_info->mtx_coef.pre_offset[0] << 16) |
			mtx_info->mtx_coef.pre_offset[1];
		pre_offset2 = mtx_info->mtx_coef.pre_offset[2];

		coef00_01 =
			(mtx_info->mtx_coef.matrix_coef[0][0] << 16) |
			mtx_info->mtx_coef.matrix_coef[0][1];
		coef02_10 =
			(mtx_info->mtx_coef.matrix_coef[0][2] << 16) |
			mtx_info->mtx_coef.matrix_coef[1][0];
		coef11_12 =
			(mtx_info->mtx_coef.matrix_coef[1][1] << 16) |
			mtx_info->mtx_coef.matrix_coef[1][2];
		coef20_21 =
			(mtx_info->mtx_coef.matrix_coef[2][0] << 16) |
			mtx_info->mtx_coef.matrix_coef[2][1];
		coef22 = mtx_info->mtx_coef.matrix_coef[2][2];

		offset0_1 =
			(mtx_info->mtx_coef.post_offset[0] << 16) |
			mtx_info->mtx_coef.post_offset[1];
		offset2 = mtx_info->mtx_coef.post_offset[2];

		en = mtx_info->mtx_coef.en;
		clip = mtx_info->mtx_coef.right_shift;

		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef00_01, coef00_01, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef02_10, coef02_10, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12, coef11_12, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef20_21, coef20_21, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22, coef22, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1, offset0_1, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset2, offset2, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1, pre_offset0_1, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset2, pre_offset2, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_en_ctrl, en, 0, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_clip, clip, 5, 3, vpp_index);
		vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE;
	} else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_BRIGHT) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_pre_offset0_1,
			mtx_info->mtx_coef.pre_offset[0], 16, 16, vpp_index);
		vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_BRIGHT;
	} else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_CONTRAST) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_coef00_01,
			mtx_info->mtx_coef.matrix_coef[0][0], 16, 16, vpp_index);
		vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_CONTRAST;
	} else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_HUE_SAT) {
		coef11_12 = (mtx_info->mtx_coef.matrix_coef[1][1] << 16) |
			mtx_info->mtx_coef.matrix_coef[1][2];
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12,
			coef11_12, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_coef20_21,
			mtx_info->mtx_coef.matrix_coef[2][1], 0, 16, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22,
			mtx_info->mtx_coef.matrix_coef[2][2], vpp_index);
		vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_HUE_SAT;
	} else if (vecm_latch_flag2 & VPP_MARTIX_UPDATE_EN) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(matrix_en_ctrl,
			mtx_info->mtx_coef.en, 0, 1, vpp_index);
		vecm_latch_flag2 &= ~VPP_MARTIX_UPDATE_EN;
	}

	return 0;
}

static int vpp_mtx_get(struct vpp_mtx_info_s *mtx_info)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;

	enum vpp_matrix_e mtx_sel;
	unsigned int coef00_01 = 0;
	unsigned int coef02_10 = 0;
	unsigned int coef11_12 = 0;
	unsigned int coef20_21 = 0;
	unsigned int coef22 = 0;
	unsigned int clip = 0;
	unsigned int offset0_1 = 0;
	unsigned int offset2 = 0;
	unsigned int pre_offset0_1 = 0;
	unsigned int pre_offset2 = 0;
	unsigned int en = 0;

	mtx_sel = mtx_info->mtx_sel;

	switch (mtx_sel) {
	case VD1_MTX:
		matrix_coef00_01 = VPP_VD1_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_VD1_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_VD1_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_VD1_MATRIX_COEF20_21;
		matrix_coef22 = VPP_VD1_MATRIX_COEF22;
		matrix_clip = VPP_VD1_MATRIX_CLIP;
		matrix_offset0_1 = VPP_VD1_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_VD1_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_VD1_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_VD1_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_VD1_MATRIX_EN_CTRL;
		break;
	case POST2_MTX:
		matrix_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST2_MATRIX_COEF22;
		matrix_clip = VPP_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST2_MATRIX_EN_CTRL;
		break;
	case POST_MTX:
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST_MATRIX_COEF22;
		matrix_clip = VPP_POST_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;
		break;
	case VPP1_POST2_MTX:
		matrix_coef00_01 = VPP1_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP1_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP1_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP1_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP1_POST2_MATRIX_COEF22;
		matrix_clip = VPP1_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP1_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP1_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP1_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP1_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP1_POST2_MATRIX_EN_CTRL;
		break;
	case VPP2_POST2_MTX:
		matrix_coef00_01 = VPP2_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP2_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP2_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP2_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP2_POST2_MATRIX_COEF22;
		matrix_clip = VPP2_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP2_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP2_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP2_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP2_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP2_POST2_MATRIX_EN_CTRL;
		break;
	case OSD1_HDR_MTX:
		matrix_coef00_01 = OSD1_HDR2_MATRIXI_COEF00_01;
		matrix_coef02_10 = OSD1_HDR2_MATRIXI_COEF02_10;
		matrix_coef11_12 = OSD1_HDR2_MATRIXI_COEF11_12;
		matrix_coef20_21 = OSD1_HDR2_MATRIXI_COEF20_21;
		matrix_coef22 = OSD1_HDR2_MATRIXI_COEF22;
		matrix_clip = OSD1_HDR2_MATRIXI_CLIP;
		matrix_offset0_1 = OSD1_HDR2_MATRIXI_OFFSET0_1;
		matrix_offset2 = OSD1_HDR2_MATRIXI_OFFSET2;
		matrix_pre_offset0_1 = OSD1_HDR2_MATRIXI_PRE_OFFSET0_1;
		matrix_pre_offset2 = OSD1_HDR2_MATRIXI_PRE_OFFSET2;
		matrix_en_ctrl = OSD1_HDR2_MATRIXI_EN_CTRL;
		break;
	case OSD1_PQ_MTX:
		matrix_coef00_01 = OSD1_PQ_MATRIX_COEF00_01;
		matrix_coef02_10 = OSD1_PQ_MATRIX_COEF02_10;
		matrix_coef11_12 = OSD1_PQ_MATRIX_COEF11_12;
		matrix_coef20_21 = OSD1_PQ_MATRIX_COEF20_21;
		matrix_coef22 = OSD1_PQ_MATRIX_COEF22;
		matrix_clip = OSD1_PQ_MATRIX_CLIP;
		matrix_offset0_1 = OSD1_PQ_MATRIX_OFFSET0_1;
		matrix_offset2 = OSD1_PQ_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = OSD1_PQ_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = OSD1_PQ_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = OSD1_PQ_MATRIX_EN_CTRL;
		break;
	case MTX_NULL:
	default:
		break;
	}

	if (!mtx_sel)
		return 0;

	coef00_01 = READ_VPP_REG(matrix_coef00_01);
	coef02_10 = READ_VPP_REG(matrix_coef02_10);
	coef11_12 = READ_VPP_REG(matrix_coef11_12);
	coef20_21 = READ_VPP_REG(matrix_coef20_21);
	coef22 = READ_VPP_REG(matrix_coef22);
	pre_offset0_1 = READ_VPP_REG(matrix_pre_offset0_1);
	pre_offset2 = READ_VPP_REG(matrix_pre_offset2);
	offset0_1 = READ_VPP_REG(matrix_offset0_1);
	offset2 = READ_VPP_REG(matrix_offset2);
	en = READ_VPP_REG_BITS(matrix_en_ctrl, 0, 1);
	clip = READ_VPP_REG_BITS(matrix_clip, 5, 3);

	mtx_info->mtx_coef.pre_offset[0] =
		(u16)((pre_offset0_1 >> 16) & 0xffff);
	mtx_info->mtx_coef.pre_offset[1] =
		(u16)(pre_offset0_1 & 0xffff);
	mtx_info->mtx_coef.pre_offset[2] =
		(u16)(pre_offset2 & 0xffff);
	mtx_info->mtx_coef.matrix_coef[0][0] =
		(u16)((coef00_01 >> 16) & 0xffff);
	mtx_info->mtx_coef.matrix_coef[0][1] =
		(u16)(coef00_01 & 0xffff);
	mtx_info->mtx_coef.matrix_coef[0][2] =
		(u16)((coef02_10 >> 16) & 0xffff);
	mtx_info->mtx_coef.matrix_coef[1][0] =
		(u16)(coef02_10 & 0xffff);
	mtx_info->mtx_coef.matrix_coef[1][1] =
		(u16)((coef11_12 >> 16) & 0xffff);
	mtx_info->mtx_coef.matrix_coef[1][2] =
		(u16)(coef11_12 & 0xffff);
	mtx_info->mtx_coef.matrix_coef[2][0] =
		(u16)((coef20_21 >> 16) & 0xffff);
	mtx_info->mtx_coef.matrix_coef[2][1] =
		(u16)(coef20_21 & 0xffff);
	mtx_info->mtx_coef.matrix_coef[2][2] =
		(u16)(coef22 & 0xffff);
	mtx_info->mtx_coef.post_offset[0] =
		(u16)((offset0_1 >> 16) & 0xffff);
	mtx_info->mtx_coef.post_offset[1] =
		(u16)(offset0_1 & 0xffff);
	mtx_info->mtx_coef.post_offset[2] =
		(u16)(offset2 & 0xffff);
	mtx_info->mtx_coef.en = en;
	mtx_info->mtx_coef.right_shift = clip;

	return 0;
}

void pre_gma_update(struct pre_gamma_table_s *pre_gma_lut, int vpp_index)
{
	if (vecm_latch_flag2 & VPP_PRE_GAMMA_UPDATE) {
		set_pre_gamma_reg(pre_gma_lut, vpp_index);
		vecm_latch_flag2 &= ~VPP_PRE_GAMMA_UPDATE;
	}
}

void eye_prot_update(struct eye_protect_s *eye_prot, int vpp_index)
{
	if (vecm_latch_flag2 & VPP_EYE_PROTECT_UPDATE) {
		if (chip_type_id != chip_s5)
			eye_proc(eye_prot->mtx_ep, eye_prot->en, vpp_index);
		else
			ve_eye_proc(eye_prot->mtx_ep, eye_prot->en, vpp_index);
		vecm_latch_flag2 &= ~VPP_EYE_PROTECT_UPDATE;
	}
}

static int amvecm_set_osd_mtx_init(int enable)
{
	int *m = NULL;
	unsigned int OSD_MATRIX_COEF00_01 = 0;
	unsigned int OSD_MATRIX_COEF02_10 = 0;
	unsigned int OSD_MATRIX_COEF11_12 = 0;
	unsigned int OSD_MATRIX_COEF20_21 = 0;
	unsigned int OSD_MATRIX_COEF22 = 0;
	unsigned int OSD_MATRIX_OFFSET0_1 = 0;
	unsigned int OSD_MATRIX_OFFSET2 = 0;
	unsigned int OSD_MATRIX_PRE_OFFSET0_1 = 0;
	unsigned int OSD_MATRIX_PRE_OFFSET2 = 0;
	unsigned int OSD_MATRIX_EN_CTRL = 0;
	unsigned int OSD_PQ_MATRIX_COEF00_01 = 0;
	unsigned int OSD_PQ_MATRIX_COEF02_10 = 0;
	unsigned int OSD_PQ_MATRIX_COEF11_12 = 0;
	unsigned int OSD_PQ_MATRIX_COEF20_21 = 0;
	unsigned int OSD_PQ_MATRIX_COEF22 = 0;
	unsigned int OSD_PQ_MATRIX_OFFSET0_1 = 0;
	unsigned int OSD_PQ_MATRIX_OFFSET2 = 0;
	unsigned int OSD_PQ_MATRIX_PRE_OFFSET0_1 = 0;
	unsigned int OSD_PQ_MATRIX_PRE_OFFSET2 = 0;
	unsigned int OSD_PQ_MATRIX_CLIP = 0;
	unsigned int OSD_PQ_MATRIX_EN_CTRL = 0;

	if (!enable)
		return 0;

	if (chip_type_id == chip_t5m) {
		OSD_MATRIX_COEF00_01 = VPP_WRAP_OSD1_MATRIX_COEF00_01;
		OSD_MATRIX_COEF02_10 = VPP_WRAP_OSD1_MATRIX_COEF02_10;
		OSD_MATRIX_COEF11_12 = VPP_WRAP_OSD1_MATRIX_COEF11_12;
		OSD_MATRIX_COEF20_21 = VPP_WRAP_OSD1_MATRIX_COEF20_21;
		OSD_MATRIX_COEF22 = VPP_WRAP_OSD1_MATRIX_COEF22;
		OSD_MATRIX_OFFSET0_1 = VPP_WRAP_OSD1_MATRIX_OFFSET0_1;
		OSD_MATRIX_OFFSET2 = VPP_WRAP_OSD1_MATRIX_OFFSET2;
		OSD_MATRIX_PRE_OFFSET0_1 = VPP_WRAP_OSD1_MATRIX_PRE_OFFSET0_1;
		OSD_MATRIX_PRE_OFFSET2 = VPP_WRAP_OSD1_MATRIX_PRE_OFFSET2;
		OSD_MATRIX_EN_CTRL = VPP_WRAP_OSD1_MATRIX_EN_CTRL;

		OSD_PQ_MATRIX_COEF00_01 = OSD1_HDR2_MATRIXI_COEF00_01;
		OSD_PQ_MATRIX_COEF02_10 = OSD1_HDR2_MATRIXI_COEF02_10;
		OSD_PQ_MATRIX_COEF11_12 = OSD1_HDR2_MATRIXI_COEF11_12;
		OSD_PQ_MATRIX_COEF20_21 = OSD1_HDR2_MATRIXI_COEF20_21;
		OSD_PQ_MATRIX_COEF22 = OSD1_HDR2_MATRIXI_COEF22;
		OSD_PQ_MATRIX_OFFSET0_1 = OSD1_HDR2_MATRIXI_OFFSET0_1;
		OSD_PQ_MATRIX_OFFSET2 = OSD1_HDR2_MATRIXI_OFFSET2;
		OSD_PQ_MATRIX_PRE_OFFSET0_1 = OSD1_HDR2_MATRIXI_PRE_OFFSET0_1;
		OSD_PQ_MATRIX_PRE_OFFSET2 = OSD1_HDR2_MATRIXI_PRE_OFFSET2;
		OSD_PQ_MATRIX_CLIP = OSD1_HDR2_MATRIXI_CLIP;
		OSD_PQ_MATRIX_EN_CTRL = OSD1_HDR2_MATRIXI_EN_CTRL;
	} else if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w  || chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
//		OSD_MATRIX_COEF00_01 = OSD1_HDR2_MATRIXI_COEF00_01;
//		OSD_MATRIX_COEF02_10 = OSD1_HDR2_MATRIXI_COEF02_10;
//		OSD_MATRIX_COEF11_12 = OSD1_HDR2_MATRIXI_COEF11_12;
//		OSD_MATRIX_COEF20_21 = OSD1_HDR2_MATRIXI_COEF20_21;
//		OSD_MATRIX_COEF22 = OSD1_HDR2_MATRIXI_COEF22;
//		OSD_MATRIX_OFFSET0_1 = OSD1_HDR2_MATRIXI_OFFSET0_1;
//		OSD_MATRIX_OFFSET2 = OSD1_HDR2_MATRIXI_OFFSET2;
//		OSD_MATRIX_PRE_OFFSET0_1 = OSD1_HDR2_MATRIXI_PRE_OFFSET0_1;
//		OSD_MATRIX_PRE_OFFSET2 = OSD1_HDR2_MATRIXI_PRE_OFFSET2;
//		OSD_MATRIX_EN_CTRL = OSD1_HDR2_MATRIXI_EN_CTRL;

		if (chip_type_id == chip_a9) {
			OSD_MATRIX_COEF00_01 = OSD1_HDR2_MATRIXI_COEF00_01;
			OSD_MATRIX_COEF02_10 = OSD1_HDR2_MATRIXI_COEF02_10;
			OSD_MATRIX_COEF11_12 = OSD1_HDR2_MATRIXI_COEF11_12;
			OSD_MATRIX_COEF20_21 = OSD1_HDR2_MATRIXI_COEF20_21;
			OSD_MATRIX_COEF22 = OSD1_HDR2_MATRIXI_COEF22;
			OSD_MATRIX_OFFSET0_1 = OSD1_HDR2_MATRIXI_OFFSET0_1;
			OSD_MATRIX_OFFSET2 = OSD1_HDR2_MATRIXI_OFFSET2;
			OSD_MATRIX_PRE_OFFSET0_1 = OSD1_HDR2_MATRIXI_PRE_OFFSET0_1;
			OSD_MATRIX_PRE_OFFSET2 = OSD1_HDR2_MATRIXI_PRE_OFFSET2;
			OSD_MATRIX_EN_CTRL = OSD1_HDR2_MATRIXI_EN_CTRL;
		}

		OSD_PQ_MATRIX_COEF00_01 = OSD1_PQ_MATRIX_COEF00_01;
		OSD_PQ_MATRIX_COEF02_10 = OSD1_PQ_MATRIX_COEF02_10;
		OSD_PQ_MATRIX_COEF11_12 = OSD1_PQ_MATRIX_COEF11_12;
		OSD_PQ_MATRIX_COEF20_21 = OSD1_PQ_MATRIX_COEF20_21;
		OSD_PQ_MATRIX_COEF22 = OSD1_PQ_MATRIX_COEF22;
		OSD_PQ_MATRIX_OFFSET0_1 = OSD1_PQ_MATRIX_OFFSET0_1;
		OSD_PQ_MATRIX_OFFSET2 = OSD1_PQ_MATRIX_OFFSET2;
		OSD_PQ_MATRIX_PRE_OFFSET0_1 = OSD1_PQ_MATRIX_PRE_OFFSET0_1;
		OSD_PQ_MATRIX_PRE_OFFSET2 = OSD1_PQ_MATRIX_PRE_OFFSET2;
		OSD_PQ_MATRIX_CLIP = OSD1_PQ_MATRIX_CLIP;
		OSD_PQ_MATRIX_EN_CTRL = OSD1_PQ_MATRIX_EN_CTRL;
	} else {
		return 0;
	}

	if (chip_type_id == chip_t5m) {
		/* RGB -> 709 limit */
		m = RGB709_to_YUV709l_coeff;
		WRITE_VPP_REG(OSD_MATRIX_PRE_OFFSET0_1,
			((m[0] & 0xfff) << 16) | (m[1] & 0xfff));
		WRITE_VPP_REG(OSD_MATRIX_PRE_OFFSET2,
			m[2] & 0xfff);
		WRITE_VPP_REG(OSD_MATRIX_COEF00_01,
			((m[3] & 0x1fff) << 16) | (m[4] & 0x1fff));
		WRITE_VPP_REG(OSD_MATRIX_COEF02_10,
			((m[5]  & 0x1fff) << 16) | (m[6] & 0x1fff));
		WRITE_VPP_REG(OSD_MATRIX_COEF11_12,
			((m[7] & 0x1fff) << 16) | (m[8] & 0x1fff));
		WRITE_VPP_REG(OSD_MATRIX_COEF20_21,
			((m[9] & 0x1fff) << 16) | (m[10] & 0x1fff));
		WRITE_VPP_REG(OSD_MATRIX_COEF22,
			m[11] & 0x1fff);
		WRITE_VPP_REG(OSD_MATRIX_OFFSET0_1,
			((m[18] & 0xfff) << 16) | (m[19] & 0xfff));
		WRITE_VPP_REG(OSD_MATRIX_OFFSET2,
			m[20] & 0xfff);
		WRITE_VPP_REG_BITS(OSD_MATRIX_EN_CTRL, 1, 0, 1);
	}

	/* osd pq setting matrix*/
	WRITE_VPP_REG(OSD_PQ_MATRIX_PRE_OFFSET0_1,
		(0x7c0 << 16) | 0x600);
	WRITE_VPP_REG(OSD_PQ_MATRIX_PRE_OFFSET2,
		0x600);
	WRITE_VPP_REG(OSD_PQ_MATRIX_COEF00_01, 0x400 << 16);
	WRITE_VPP_REG(OSD_PQ_MATRIX_COEF02_10, 0x0);
	WRITE_VPP_REG(OSD_PQ_MATRIX_COEF11_12, 0x400 << 16);
	WRITE_VPP_REG(OSD_PQ_MATRIX_COEF20_21, 0x0);
	WRITE_VPP_REG(OSD_PQ_MATRIX_COEF22, 0x400);
	WRITE_VPP_REG(OSD_PQ_MATRIX_OFFSET0_1,
		(0x0040 << 16) | 0x0200);
	WRITE_VPP_REG(OSD_PQ_MATRIX_OFFSET2,
		0x0200);
	WRITE_VPP_REG_BITS(OSD_PQ_MATRIX_EN_CTRL, 1, 0, 1);
	WRITE_VPP_REG_BITS(OSD_PQ_MATRIX_CLIP, 0, 5, 3);

	return 0;
}

static int amvecm_set_osd_mtx_enable(int enable)
{
	struct vpp_mtx_info_s *mtx_p = &mtx_info;

	if (!osd_pic_en)
		return 0;

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		if (chip_type_id == chip_t5m)
			mtx_p->mtx_sel = OSD1_HDR_MTX;
		else
			mtx_p->mtx_sel = OSD1_PQ_MTX;

		vpp_mtx_get(mtx_p);

		mtx_p->mtx_coef.en = enable;
		vecm_latch_flag2 |= VPP_MARTIX_UPDATE_EN;
	}

	return 0;
}

static int amvecm_set_osd_brightness(int val)
{
	struct vpp_mtx_info_s *mtx_p = &mtx_info;

	if (!osd_pic_en)
		return 0;

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		if (chip_type_id == chip_t5m)
			mtx_p->mtx_sel = OSD1_HDR_MTX;
		else
			mtx_p->mtx_sel = OSD1_PQ_MTX;

		if (val < -1024)
			val = -1024;
		else if (val > 1023)
			val = 1023;

		if (val < 0)
			val += 2048;

		mtx_p->mtx_coef.pre_offset[0] = val;
		vecm_latch_flag2 |= VPP_MARTIX_UPDATE_BRIGHT;
	}

	return 0;
}

static int amvecm_set_osd_contrast(int val)
{
	struct vpp_mtx_info_s *mtx_p = &mtx_info;

	if (!osd_pic_en)
		return 0;

	if (val < -1024)
		val = -1024;
	else if (val > 1023)
		val = 1023;

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		if (chip_type_id == chip_t5m)
			mtx_p->mtx_sel = OSD1_HDR_MTX;
		else
			mtx_p->mtx_sel = OSD1_PQ_MTX;

		mtx_p->mtx_coef.matrix_coef[0][0] = val + 1024;
		vecm_latch_flag2 |= VPP_MARTIX_UPDATE_CONTRAST;
	}

	return 0;
}

static int amvecm_set_osd_hue_sat(int hue_val, int sat_val)
{
	struct vpp_mtx_info_s *mtx_p = &mtx_info;
	int i, ma, mb, mc;

	int hue_cos[] = {
			/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245,
		/*13~25*/
		243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209
	};
	int hue_sin[] = {
		/*-25~-13*/
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		 -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,  -50,
		 -44,  -38,  -31,  -25, -19, -13,  -6,      /*-12~-1*/
		0,  /*0*/
		 /*1~12*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,  62,
		68,  74,   80,   86,   92,	98,  104,  109,  115,  121,
		126,  132, 137, 142, 147 /*13~25*/
	};

	if (!osd_pic_en)
		return 0;

	if (hue_val < -25)
		hue_val = -25;
	else if (hue_val > 25)
		hue_val = 25;

	if (sat_val < -128)
		sat_val = -128;
	else if (sat_val > 128)
		sat_val = 128;

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		if (chip_type_id == chip_t5m)
			mtx_p->mtx_sel = OSD1_HDR_MTX;
		else
			mtx_p->mtx_sel = OSD1_PQ_MTX;

		i = (hue_val > 0) ? hue_val : -hue_val;
		ma = (hue_cos[i] * (sat_val + 128)) >> 5;
		mb = (hue_sin[25 + hue_val] * (sat_val + 128)) >> 5;
		if (ma > 2047)
			ma = 2047;
		if (mb > 2047)
			mb = 2047;

		mtx_p->mtx_coef.matrix_coef[1][1] = ma;
		mtx_p->mtx_coef.matrix_coef[2][2] = ma;

		mc = -mb;
		if (mb < 0)
			mb += 8192;
		else if (mb > 0)
			mc += 8192;

		mtx_p->mtx_coef.matrix_coef[1][2] = mb;
		mtx_p->mtx_coef.matrix_coef[2][1] = mc;
		vecm_latch_flag2 |= VPP_MARTIX_UPDATE_HUE_SAT;
	}

	return 0;
}

static ssize_t osd_brightness_show(const struct class *cla,
					const struct class_attribute *attr,
					char *buf)
{
	struct vpp_mtx_info_s *mtx_p = &mtx_info;
	int brightness;

	if (!osd_pic_en)
		return -EINVAL;

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		if (chip_type_id == chip_t5m)
			mtx_p->mtx_sel = OSD1_HDR_MTX;
		else
			mtx_p->mtx_sel = OSD1_PQ_MTX;

		vpp_mtx_get(mtx_p);

		brightness = mtx_p->mtx_coef.pre_offset[0];
		if (brightness > 1023)
			brightness -= 2048;

		return sprintf(buf, "%d\n", brightness);
	} else {
		return -EINVAL;
	}
}

static ssize_t osd_brightness_store(const struct class *cla,
					 const struct class_attribute *attr,
					 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (val < -1024 || val > 1023) {
		pr_info("osd brightness range: -1024 ~ 1023\n");
		return -EINVAL;
	}
	amvecm_set_osd_brightness(val);
	return count;
}

static ssize_t osd_contrast_show(const struct class *cla,
					const struct class_attribute *attr,
					char *buf)
{
	struct vpp_mtx_info_s *mtx_p = &mtx_info;

	if (!osd_pic_en)
		return -EINVAL;

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		if (chip_type_id == chip_t5m)
			mtx_p->mtx_sel = OSD1_HDR_MTX;
		else
			mtx_p->mtx_sel = OSD1_PQ_MTX;

		vpp_mtx_get(mtx_p);

		return sprintf(buf, "%d\n",
			(int)(mtx_p->mtx_coef.matrix_coef[0][0] - 1024));
	} else {
		return -EINVAL;
	}
}

static ssize_t osd_contrast_store(const struct class *cla,
					 const struct class_attribute *attr,
					 const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (val < -1024 || val > 1023) {
		pr_info("osd brightness range: -1024 ~ 1023\n");
		return -EINVAL;
	}
	amvecm_set_osd_contrast(val);
	return count;
}

static ssize_t osd_hue_sat_show(const struct class *cla,
	const struct class_attribute *attr,
	char *buf)
{
	struct vpp_mtx_info_s *mtx_p = &mtx_info;

	if (!osd_pic_en)
		return -EINVAL;

	if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		if (chip_type_id == chip_t5m)
			mtx_p->mtx_sel = OSD1_HDR_MTX;
		else
			mtx_p->mtx_sel = OSD1_PQ_MTX;

		vpp_mtx_get(mtx_p);

		return sprintf(buf, "c11 = 0x%x, c12 = 0x%x, c21 = 0x%x\n",
			mtx_p->mtx_coef.matrix_coef[1][1],
			mtx_p->mtx_coef.matrix_coef[1][2],
			mtx_p->mtx_coef.matrix_coef[2][1]);
	} else {
		return -EINVAL;
	}
}

static ssize_t osd_hue_sat_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long hue_val, sat_val;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);

	if (kstrtol(parm[0], 10, &hue_val) < 0)
		goto free_buf;

	if (kstrtol(parm[1], 10, &sat_val) < 0)
		goto free_buf;

	pr_info("hue_val= %d, sat_val= %d\n", (int)hue_val, (int)sat_val);
	amvecm_set_osd_hue_sat((int)hue_val, (int)sat_val);
	kfree(buf_orig);
	return count;
free_buf:
	kfree(buf_orig);
	return -EINVAL;
}
#endif

void amvecm_saturation_hue_update(int offset_val)
{
	sat_hue_offset_val = offset_val;
	pq_user_latch_flag |= PQ_USER_CMS_SAT_HUE;
	if (debug_amvecm & 8)
		pr_info("[amvecm..] %s: saturation_hue:%d offset=%d\n",
		__func__,
		vdj_mode_s.saturation_hue,
		sat_hue_offset_val);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
unsigned int force_lut3d_bypass;

void bs_ct_update(int vpp_index)
{
	if (vecm_latch_flag2 & LUT3D_BASE_UPDATE) {
		vecm_latch_flag2 &= ~LUT3D_BASE_UPDATE;
		lut3d_update(0, vpp_index);
	}

	if (vecm_latch_flag2 & LUT3D_UPDATE) {
		vecm_latch_flag2 &= ~LUT3D_UPDATE;
		lut3d_set_api(vpp_index);
	}

	if (force_lut3d_bypass) {
		vpp_lut3d_table_init(-1, -1, -1);
		lut3d_update(0, vpp_index);
		force_lut3d_bypass = 0;
	}
}

void bs_ct_latch(void)
{
	if (ct_en || lut3d_en)
		vecm_latch_flag2 |= LUT3D_UPDATE;
}

void gamut_mapping_wrapper_process(int vpp_index)
{
	static unsigned int pre_gamut0_dma_state;
	static unsigned int pre_gamut1_dma_state;

	if (chip_type_id != chip_t6x)
		return;

	if (pre_gamut0_dma_state == DMA_STATE_ENABLE) {
		am_dma_set_dma_mif_rd_en(EN_DMA_RD_ID_GAMUT0, 0, vpp_index);
		pre_gamut0_dma_state = DMA_STATE_DISABLE;
		gamut_mapping0_param.flag_update_lut = DMA_STATE_DISABLE;
	}

	if (pre_gamut1_dma_state == DMA_STATE_ENABLE) {
		am_dma_set_dma_mif_rd_en(EN_DMA_RD_ID_GAMUT1, 0, vpp_index);
		pre_gamut1_dma_state = DMA_STATE_DISABLE;
		gamut_mapping1_param.flag_update_lut = DMA_STATE_DISABLE;
	}

	if (vecm_latch_flag2 & FLAG_GAMUT_MAPPING0_UPDATE) {
		vecm_latch_flag2 &= ~FLAG_GAMUT_MAPPING0_UPDATE;
		set_gamut_mapping_wrapper(0, vpp_index);
	}

	if (vecm_latch_flag2 & FLAG_GAMUT_MAPPING1_UPDATE) {
		vecm_latch_flag2 &= ~FLAG_GAMUT_MAPPING1_UPDATE;
		set_gamut_mapping_wrapper(1, vpp_index);
	}

	pre_gamut0_dma_state = gamut_mapping0_param.flag_update_lut;
	pre_gamut1_dma_state = gamut_mapping1_param.flag_update_lut;

}

void gamut_mapping_wrapper_crtl(int module, int en)
{
	if (module == 0 && gamut_mapping0_en != en) {
		gamut_mapping0_en = en;
		vecm_latch_flag2 |= FLAG_GAMUT_MAPPING0_UPDATE;
	} else if (module == 1 && gamut_mapping1_en != en) {
		gamut_mapping1_en = en;
		vecm_latch_flag2 |= FLAG_GAMUT_MAPPING1_UPDATE;
	}
}

#endif

void amvecm_video_latch(int vpp_index)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	unsigned int temp;

	resume_recovery_process(vpp_index);
	pc_mode_process(vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] pc_mode done.\n");
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	cm_latch_process(vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] cm_latch done.\n");
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/*amvecm_size_patch();*/
	ve_dnlp_latch_process();
	pr_amvecm_bringup_dbg("[on_vs] dnlp_latch done.\n");
	/*venc*/
	if (cpu_after_eq_t7())
		temp = vpp_get_vout_viu_mux();
	else
		temp = vpp_get_encl_viu_mux();
	pr_amvecm_bringup_dbg("[on_vs] viu_mux done.\n");

	if (temp)
		ve_lcd_gamma_process(vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] lcd_gamma done.\n");
#endif
	lvds_freq_process();

/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (0) {
		amvecm_3d_sync_process();
		amvecm_3d_black_process();
	}
#endif
/* #endif */
	pq_user_latch_process(vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] user_latch done.\n");

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		ve_lc_latch_process();
	pr_amvecm_bringup_dbg("[on_vs] lc_latch done.\n");
#endif

	/* ioc vadj1/2 switch */
	amvecm_vadj_latch_process(vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] vadj_latch done.\n");
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/*matrix wr & rd latch */
	vpp_mtx_update(&mtx_info, vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] mtx_update done.\n");
	pre_gma_update(&pre_gamma, vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] pre_gma done.\n");
	eye_prot_update(&eye_protect, vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] eye_prot done.\n");

	bs_ct_update(vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] bs_ct done.\n");
	dnlp_en_update(vpp_index);
	sharpness_gain_update(vpp_index);
	pre_saturation_gain_update();
	gamut_mapping_wrapper_process(vpp_index);
#endif
}

static void amvecm_overscan_process(struct vframe_s *vf,
				    struct vframe_s *toggle_vf,
				    int flags,
				    enum vd_path_e vd_path,
				    enum vpp_index_e vpp_index)
{
	if (vpp_index == VPP_TOP0 && vd_path != VD1_PATH)
		return;

	if (flags & CSC_FLAG_CHECK_OUTPUT) {
		if (toggle_vf || vf)
			amvecm_fresh_overscan(toggle_vf, vf);
		/*pr_amvecm_dbg("CSC_FLAG_CHECK_OUTPUT fresh_overscan.\n");*/
		return;
	}

	if (!toggle_vf && !vf) {
		amvecm_reset_overscan();
		/*pr_amvecm_dbg("no vframe reset_overscan.\n");*/
	}

	if (vf) {
		amvecm_fresh_overscan(0, vf);
		/*pr_amvecm_dbg("repeat vf fresh_overscan.\n");*/
	} else {
		amvecm_reset_overscan();
		/*pr_amvecm_dbg("no repeat vf reset_overscan.\n");*/
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static int cabc_add_hist_proc(struct vframe_s *vf, struct vpp_hist_param_s *vp)
{
	int *hist;
	int i;

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		return 0;

	hist = vf_hist_get();

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		chip_type_id != chip_txhd2) {
		vpp_pst_hist_sta_read(hist);
		return 1;
	}

	if (vf) {
		for (i = 0; i < 64; i++)
			hist[i] = vp->vpp_gamma[i];
	} else {
		/*default 1080p linear hist*/
		for (i = 0; i < 64; i++)
			hist[i] = 1012;
	}

	return 1;
}

static int cabc_aad_on_vs(int vf_state)
{
	int cabc_en;

	cabc_en = fw_en_get();

	if (!vf_state)
		return 0;

	if (cabc_en)
		queue_work(aml_cabc_queue, &cabc_proc_work);
	else
		queue_work(aml_cabc_queue, &cabc_bypass_work);

	return 0;
}

#endif

#ifdef T7_BRINGUP_MULTI_VPP
int min_vpp_process(int vpp_top_index, enum vpp_index_e vpp_index)
{
	int result = 0;
	//write csc and gain/offset for vpp1/2 here
	// update hdr matrix
	result = amvecm_matrix_process(toggle_vf, vf, flags, vd_path, vpp_index);
	// to do

	return result
}
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void resume_mtx_flag_set(bool flag)
{
	resume_mtx_flag = flag;
}

bool resume_mtx_flag_get(void)
{
	return resume_mtx_flag;
}
#endif

void amvecm_size_info_update(int vpp_index)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x)
		ve_size_info_update(vpp_index);
#endif
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct dump_output_cfg_s {
	unsigned int height; // 4k: 1280, 2k: 640
	unsigned int width;
	unsigned int row_num;
	unsigned int col_num;
	unsigned int first_row_num;
	unsigned int last_row_num;
	unsigned int point_count;
	unsigned int bit_depth;
	unsigned int delay_count;
	unsigned int probe_port;
	unsigned int v_start; //4k:380, 2k:190
};

struct dump_output_point_pos_s {
	unsigned int val_x;
	unsigned int val_y;
};

struct dump_output_cfg_s dump_output_cfg_data = {
	1280, 3840, 20, 40, 11, 9, 740, 10, 1, 1, 380
};

struct dump_output_point_pos_s dump_output_pos[750];
unsigned int dump_output_flag;
unsigned int dump_output_read_latch;

void amvecm_dump_output_pos_cal(void)
{
	int i, j, tmp, total_count = 0;
	int step_x = 0;
	int step_y = 0;

	step_x = dump_output_cfg_data.width / dump_output_cfg_data.col_num;
	step_y = dump_output_cfg_data.height / dump_output_cfg_data.row_num;

	/*calculate pos x and y*/
	for (i = 0; i < dump_output_cfg_data.row_num; i++) {
		if (i == 0)
			tmp = dump_output_cfg_data.first_row_num;
		else if (i == dump_output_cfg_data.row_num - 1)
			tmp = dump_output_cfg_data.last_row_num;
		else
			tmp = dump_output_cfg_data.col_num;

		for (j = 0; j < tmp; j++) {
			dump_output_pos[total_count].val_y = step_y / 2 + i * step_y +
				dump_output_cfg_data.v_start;
			dump_output_pos[total_count].val_x = step_x / 2 + j * step_x;
			total_count++;
		}
	}
}

void amvecm_dump_output_data(void)
{
	static int total_count;
	int val = 0;
	int reg_val = 0;
	int val_x = 0;
	int val_y = 0;
	unsigned int reg_pos = 0;
	unsigned int reg_probe_color = 0;
	unsigned int reg_probe_color1 = 0;

	if (!dump_output_flag)
		return;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		!is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
		chip_type_id != chip_txhd2 &&
		chip_type_id != chip_s1a &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d &&
		!is_meson_s6_cpu()) {
		reg_pos = VPP_PROBE_POS;
		reg_probe_color = VPP_PROBE_COLOR;
		reg_probe_color1 = VPP_PROBE_COLOR1;
	} else {
		reg_pos = VPP_MATRIX_PROBE_POS;
		reg_probe_color = VPP_MATRIX_PROBE_COLOR;
		reg_probe_color1 = VPP_MATRIX_PROBE_COLOR1;
	}

	if (total_count < dump_output_cfg_data.point_count) {
		val_x = dump_output_pos[total_count].val_x;
		val_y = dump_output_pos[total_count].val_y;

		if (!dump_output_read_latch) {
			reg_val = READ_VPP_REG(reg_pos);
			reg_val = reg_val & 0xe000e000;
			reg_val = reg_val | (val_x << 16) | val_y;
			WRITE_VPP_REG(reg_pos, reg_val);

			dump_output_read_latch =
				dump_output_cfg_data.delay_count;
		} else if (dump_output_read_latch == 1) {
			if (!total_count)
				pr_info("**********dump output data start*********\n");

			if (dump_output_cfg_data.bit_depth == 10) {
				val = READ_VPP_REG(reg_probe_color);
				pr_info("[idx:%d] pos:(%d,%d), val:(%d,%d,%d)\n",
					total_count, val_x, val_y,
					(val >> 20) & 0x3ff,
					(val >> 10) & 0x3ff,
					(val >> 0) & 0x3ff);
			} else {
				val = READ_VPP_REG(reg_probe_color);
				reg_val = READ_VPP_REG(reg_probe_color1);
				pr_info("[idx:%d] pos:(%d,%d), val:(%d,%d,%d) (%d,%d,%d)\n",
					total_count, val_x, val_y,
					((reg_val & 0xf) << 8) | ((val >> 24) & 0xff),
					(val >> 12) & 0xfff, val & 0xfff,
					(((reg_val & 0xf) << 8) | ((val >> 24) & 0xff)) >> 2,
					((val >> 12) & 0xfff) >> 2, (val & 0xfff) >> 2);
			}

			total_count++;

			dump_output_read_latch = 0;
		} else {
			dump_output_read_latch--;
		}
	} else {
		pr_info("**********dump output data end (%d)**********\n", total_count);
		dump_output_flag = 0;
		total_count = 0;
	}
}

static void hdr_test_set_regs(unsigned int test_case, int vpp_index)
{
	u32 L_HDR2_CTRL = 0;
	u32 L_EOTF_LUT_ADDR_PORT = 0;
	u32 L_EOTF_LUT_DATA_PORT = 0;
	u32 L_OETF_LUT_ADDR_PORT = 0;
	u32 L_OETF_LUT_DATA_PORT = 0;
	u32 L_OGAIN_LUT_ADDR_PORT = 0;
	u32 L_OGAIN_LUT_DATA_PORT = 0;
	u32 L_OGAIN_LUT1_ADDR_PORT = 0;
	u32 L_OGAIN_LUT1_DATA_PORT = 0;
	u32 L_CGAIN_LUT_ADDR_PORT = 0;
	u32 L_CGAIN_LUT_DATA_PORT = 0;
	u32 L_HDR2_CGAIN_OFFT = 0;
	u32 L_HDR2_CGAIN_COEF0 = 0;
	u32 L_HDR2_CGAIN_COEF1 = 0;
	u32 L_HDR2_ADPS_CTRL = 0;
	u32 L_HDR2_ADPS_ALPHA0 = 0;
	u32 L_HDR2_ADPS_ALPHA1 = 0;
	u32 L_HDR2_ADPS_BETA0 = 0;
	u32 L_HDR2_ADPS_BETA1 = 0;
	u32 L_HDR2_ADPS_BETA2 = 0;
	u32 L_HDR2_ADPS_COEF0 = 0;
	u32 L_HDR2_ADPS_COEF1 = 0;
	u32 L_HDR2_GMUT_CTRL  = 0;
	u32 L_HDR2_GMUT_COEF0 = 0;
	u32 L_HDR2_GMUT_COEF1 = 0;
	u32 L_HDR2_GMUT_COEF2 = 0;
	u32 L_HDR2_GMUT_COEF3 = 0;
	u32 L_HDR2_GMUT_COEF4 = 0;
	u32 L_HDR2_HIST_RD = 0;
	u32 L_HDR2_HIST_CTRL  = 0;
	u32 L_HDR2_GMUT_COMP0 = 0;
	u32 L_HDR2_GMUT_COMP1 = 0;
	u32 L_HDR2_GMUT_COMP2 = 0;
	u32 L_HDR2_GMUT_COMP3 = 0;
	u32 L_HDR2_GMUT_COMP4 = 0;
	u32 L_HDR2_GMUT_COMP5 = 0;
	u32 L_HDR2_GMUT_COMP6 = 0;
	u32 L_HDR2_GMUT_COMP7 = 0;
	u32 L_HDR2_GMUT_COMP8 = 0;
	u32 L_HDR2_CGAIN_VIVID = 0;
	u32 HDR2_10BIT = 0;
	u32 L_HDR2_MATRIXI_COEF00_01 = 0;
	u32 L_HDR2_MATRIXI_COEF02_10 = 0;
	u32 L_HDR2_MATRIXI_COEF11_12 = 0;
	u32 L_HDR2_MATRIXI_COEF20_21 = 0;
	u32 L_HDR2_MATRIXI_COEF22 = 0;
	u32 L_HDR2_MATRIXI_COEF30_31 = 0;
	u32 L_HDR2_MATRIXI_COEF32_40 = 0;
	u32 L_HDR2_MATRIXI_COEF41_42 = 0;
	u32 L_HDR2_MATRIXI_OFFSET0_1 = 0;
	u32 L_HDR2_MATRIXI_OFFSET2 = 0;
	u32 L_HDR2_MATRIXI_PRE_OFFSET0_1 = 0;
	u32 L_HDR2_MATRIXI_PRE_OFFSET2 = 0;
	u32 L_HDR2_MATRIXO_COEF00_01 = 0;
	u32 L_HDR2_MATRIXO_COEF02_10 = 0;
	u32 L_HDR2_MATRIXO_COEF11_12 = 0;
	u32 L_HDR2_MATRIXO_COEF20_21 = 0;
	u32 L_HDR2_MATRIXO_COEF22 = 0;
	u32 L_HDR2_MATRIXO_COEF30_31 = 0;
	u32 L_HDR2_MATRIXO_COEF32_40 = 0;
	u32 L_HDR2_MATRIXO_COEF41_42 = 0;
	u32 L_HDR2_MATRIXO_OFFSET0_1 = 0;
	u32 L_HDR2_MATRIXO_OFFSET2 = 0;
	u32 L_HDR2_MATRIXO_PRE_OFFSET0_1 = 0;
	u32 L_HDR2_MATRIXO_PRE_OFFSET2 = 0;
	u32 L_HDR2_MATRIXI_EN_CTRL = 0;
	u32 L_HDR2_MATRIXO_EN_CTRL = 0;
	int i = 0;
	int *p_eotf_lut;
	int *p_oetf_lut;
	int *p_ootf_lut0;
	int *p_ootf_lut1;
	int *p_cgain_lut;
	unsigned int reg_offset = 0;
	struct aml_vm_reg_s aml_vm_reg;

	pr_info("%s: test_case = %d\n", __func__, test_case);

	if (test_case == EN_HDR_TEST_CASE_VD2_SDR_HDR_SR ||
		test_case == EN_HDR_TEST_CASE_VD2_HDR_SDR_SR)
		reg_offset = 0x1700;

	L_HDR2_CTRL = DOLBY_HDR2_CTRL + reg_offset;
	L_EOTF_LUT_ADDR_PORT = DOLBY_HDR2_EOTF_LUT_ADDR_PORT + reg_offset;
	L_EOTF_LUT_DATA_PORT = DOLBY_HDR2_EOTF_LUT_DATA_PORT + reg_offset;
	L_OETF_LUT_ADDR_PORT = DOLBY_HDR2_OETF_LUT_ADDR_PORT + reg_offset;
	L_OETF_LUT_DATA_PORT = DOLBY_HDR2_OETF_LUT_DATA_PORT + reg_offset;
	L_OGAIN_LUT_ADDR_PORT = DOLBY_HDR2_OGAIN_LUT_ADDR_PORT + reg_offset;
	L_OGAIN_LUT_DATA_PORT = DOLBY_HDR2_OGAIN_LUT_DATA_PORT + reg_offset;
	L_OGAIN_LUT1_ADDR_PORT = DOLBY_HDR2_OGAIN_LUT1_ADDR_PORT + reg_offset;
	L_OGAIN_LUT1_DATA_PORT = DOLBY_HDR2_OGAIN_LUT1_DATA_PORT + reg_offset;
	L_CGAIN_LUT_ADDR_PORT = DOLBY_HDR2_CGAIN_LUT_ADDR_PORT + reg_offset;
	L_CGAIN_LUT_DATA_PORT = DOLBY_HDR2_CGAIN_LUT_DATA_PORT + reg_offset;
	L_HDR2_CGAIN_OFFT = DOLBY_HDR2_CGAIN_OFFT + reg_offset;
	L_HDR2_CGAIN_COEF0 = DOLBY_HDR2_CGAIN_COEF0 + reg_offset;
	L_HDR2_CGAIN_COEF1 = DOLBY_HDR2_CGAIN_COEF1 + reg_offset;
	L_HDR2_ADPS_CTRL = DOLBY_HDR2_ADPS_CTRL + reg_offset;
	L_HDR2_ADPS_ALPHA0 = DOLBY_HDR2_ADPS_ALPHA0 + reg_offset;
	L_HDR2_ADPS_ALPHA1 = DOLBY_HDR2_ADPS_ALPHA1 + reg_offset;
	L_HDR2_ADPS_BETA0 = DOLBY_HDR2_ADPS_BETA0 + reg_offset;
	L_HDR2_ADPS_BETA1 = DOLBY_HDR2_ADPS_BETA1 + reg_offset;
	L_HDR2_ADPS_BETA2 = DOLBY_HDR2_ADPS_BETA2 + reg_offset;
	L_HDR2_ADPS_COEF0 = DOLBY_HDR2_ADPS_COEF0 + reg_offset;
	L_HDR2_ADPS_COEF1 = DOLBY_HDR2_ADPS_COEF1 + reg_offset;
	L_HDR2_GMUT_CTRL = DOLBY_HDR2_GMUT_CTRL + reg_offset;
	L_HDR2_GMUT_COEF0 = DOLBY_HDR2_GMUT_COEF0 + reg_offset;
	L_HDR2_GMUT_COEF1 = DOLBY_HDR2_GMUT_COEF1 + reg_offset;
	L_HDR2_GMUT_COEF2 = DOLBY_HDR2_GMUT_COEF2 + reg_offset;
	L_HDR2_GMUT_COEF3 = DOLBY_HDR2_GMUT_COEF3 + reg_offset;
	L_HDR2_GMUT_COEF4 = DOLBY_HDR2_GMUT_COEF4 + reg_offset;
	L_HDR2_HIST_RD = DOLBY_HDR2_HIST_RD + reg_offset;
	L_HDR2_HIST_CTRL = DOLBY_HDR2_HIST_CTRL + reg_offset;
	L_HDR2_GMUT_COMP0 = DOLBY_HDR2_GMUT_COMP0 + reg_offset;
	L_HDR2_GMUT_COMP1 = DOLBY_HDR2_GMUT_COMP1 + reg_offset;
	L_HDR2_GMUT_COMP2 = DOLBY_HDR2_GMUT_COMP2 + reg_offset;
	L_HDR2_GMUT_COMP3 = DOLBY_HDR2_GMUT_COMP3 + reg_offset;
	L_HDR2_GMUT_COMP4 = DOLBY_HDR2_GMUT_COMP4 + reg_offset;
	L_HDR2_GMUT_COMP5 = DOLBY_HDR2_GMUT_COMP5 + reg_offset;
	L_HDR2_GMUT_COMP6 = DOLBY_HDR2_GMUT_COMP6 + reg_offset;
	L_HDR2_GMUT_COMP7 = DOLBY_HDR2_GMUT_COMP7 + reg_offset;
	L_HDR2_GMUT_COMP8 = DOLBY_HDR2_GMUT_COMP8 + reg_offset;
	L_HDR2_CGAIN_VIVID = DOLBY_HDR2_CGAIN_VIVID + reg_offset;
	HDR2_10BIT = 10;
	L_HDR2_MATRIXI_COEF00_01 = DOLBY_HDR2_MATRIXI_COEF00_01 + reg_offset;
	L_HDR2_MATRIXI_COEF02_10 = DOLBY_HDR2_MATRIXI_COEF02_10 + reg_offset;
	L_HDR2_MATRIXI_COEF11_12 = DOLBY_HDR2_MATRIXI_COEF11_12 + reg_offset;
	L_HDR2_MATRIXI_COEF20_21 = DOLBY_HDR2_MATRIXI_COEF20_21 + reg_offset;
	L_HDR2_MATRIXI_COEF22 = DOLBY_HDR2_MATRIXI_COEF22 + reg_offset;
	L_HDR2_MATRIXI_OFFSET0_1 = DOLBY_HDR2_MATRIXI_OFFSET0_1 + reg_offset;
	L_HDR2_MATRIXI_OFFSET2 = DOLBY_HDR2_MATRIXI_OFFSET2 + reg_offset;
	L_HDR2_MATRIXI_PRE_OFFSET0_1 = DOLBY_HDR2_MATRIXI_PRE_OFFSET0_1 +
		reg_offset;
	L_HDR2_MATRIXI_PRE_OFFSET2 = DOLBY_HDR2_MATRIXI_PRE_OFFSET2 + reg_offset;
	L_HDR2_MATRIXO_COEF00_01 = DOLBY_HDR2_MATRIXO_COEF00_01 + reg_offset;
	L_HDR2_MATRIXO_COEF02_10 = DOLBY_HDR2_MATRIXO_COEF02_10 + reg_offset;
	L_HDR2_MATRIXO_COEF11_12 = DOLBY_HDR2_MATRIXO_COEF11_12 + reg_offset;
	L_HDR2_MATRIXO_COEF20_21 = DOLBY_HDR2_MATRIXO_COEF20_21 + reg_offset;
	L_HDR2_MATRIXO_COEF22 = DOLBY_HDR2_MATRIXO_COEF22 + reg_offset;
	L_HDR2_MATRIXO_OFFSET0_1 = DOLBY_HDR2_MATRIXO_OFFSET0_1 + reg_offset;
	L_HDR2_MATRIXO_OFFSET2 = DOLBY_HDR2_MATRIXO_OFFSET2 + reg_offset;
	L_HDR2_MATRIXO_PRE_OFFSET0_1 = DOLBY_HDR2_MATRIXO_PRE_OFFSET0_1 +
		reg_offset;
	L_HDR2_MATRIXO_PRE_OFFSET2 = DOLBY_HDR2_MATRIXO_PRE_OFFSET2 + reg_offset;
	L_HDR2_CGAIN_OFFT = DOLBY_HDR2_CGAIN_OFFT + reg_offset;
	L_HDR2_MATRIXI_EN_CTRL = DOLBY_HDR2_MATRIXI_EN_CTRL + reg_offset;
	L_HDR2_MATRIXO_EN_CTRL = DOLBY_HDR2_MATRIXO_EN_CTRL + reg_offset;
	L_HDR2_CTRL = DOLBY_HDR2_CTRL + reg_offset;
	L_HDR2_MATRIXI_COEF30_31 = DOLBY_HDR2_MATRIXI_COEF30_31 + reg_offset;
	L_HDR2_MATRIXI_COEF32_40 = DOLBY_HDR2_MATRIXI_COEF32_40 + reg_offset;
	L_HDR2_MATRIXI_COEF41_42 = DOLBY_HDR2_MATRIXI_COEF41_42 + reg_offset;
	L_HDR2_MATRIXO_COEF30_31 = DOLBY_HDR2_MATRIXO_COEF30_31 + reg_offset;
	L_HDR2_MATRIXO_COEF32_40 = DOLBY_HDR2_MATRIXO_COEF32_40 + reg_offset;
	L_HDR2_MATRIXO_COEF41_42 = DOLBY_HDR2_MATRIXO_COEF41_42 + reg_offset;

	if (test_case ==  EN_HDR_TEST_CASE_SDR_HDR_SR ||
		test_case == EN_HDR_TEST_CASE_VD2_SDR_HDR_SR) {
		memcpy(&aml_vm_reg, &sdr2hdr_sr_mode, sizeof(aml_vm_reg));
		p_eotf_lut = my_s2h_eotf_lut;
		p_oetf_lut = my_s2h_oetf_lut;
		p_ootf_lut0 = my_s2h_ogain_lut0;
		p_ootf_lut1 = my_s2h_ogain_lut1;
		p_cgain_lut = my_s2h_cgain_lut;
	} else {
		memcpy(&aml_vm_reg, &hdr2sdr_sr_mode, sizeof(aml_vm_reg));
		p_eotf_lut = my_h2s_eotf_lut;
		p_oetf_lut = my_h2s_oetf_lut;
		p_ootf_lut0 = my_h2s_ogain_lut0;
		p_ootf_lut1 = my_h2s_ogain_lut1;
		p_cgain_lut = my_h2s_cgain_lut;
	}

	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_CTRL,
		aml_vm_reg.reg_ergb_sel_mode << 24 |
		aml_vm_reg.reg_din_swap << 18 |
		aml_vm_reg.reg_out_rgb << 17 |
		aml_vm_reg.reg_only_mat << 16 |
		aml_vm_reg.reg_mtrxo_en << 15 |
		aml_vm_reg.reg_mtrxi_en << 14 |
		aml_vm_reg.reg_hdr2_top_en << 13 |
		aml_vm_reg.reg_c_gain_mode << 12 |
		aml_vm_reg.reg_gmut_mode  << 6 |
		aml_vm_reg.reg_in_shift << 5 |
		aml_vm_reg.reg_in_fmt << 4 |
		aml_vm_reg.reg_eo_enable << 3 |
		aml_vm_reg.reg_oe_enable << 2 |
		aml_vm_reg.reg_ogain_enable << 1 |
		aml_vm_reg.reg_cgain_enable << 0, vpp_index);

	//mtxin
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_COEF00_01,
		(aml_vm_reg.reg_mtrxi_coef[0][0] << 16) |
		(aml_vm_reg.reg_mtrxi_coef[0][1] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_COEF02_10,
		(aml_vm_reg.reg_mtrxi_coef[0][2] << 16) |
		(aml_vm_reg.reg_mtrxi_coef[1][0] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_COEF11_12,
		(aml_vm_reg.reg_mtrxi_coef[1][1] << 16) |
		(aml_vm_reg.reg_mtrxi_coef[1][2] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_COEF20_21,
		(aml_vm_reg.reg_mtrxi_coef[2][0] << 16) |
		(aml_vm_reg.reg_mtrxi_coef[2][1] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_COEF22,
		aml_vm_reg.reg_mtrxi_coef[2][2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_OFFSET0_1,
		(aml_vm_reg.reg_mtrxi_offst_oup[0] << 16) |
		(aml_vm_reg.reg_mtrxi_offst_oup[1] & 0xFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_OFFSET2,
		aml_vm_reg.reg_mtrxi_offst_oup[2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_PRE_OFFSET0_1,
		(aml_vm_reg.reg_mtrxi_offst_inp[0] << 16) |
		(aml_vm_reg.reg_mtrxi_offst_inp[1] & 0xFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_PRE_OFFSET2,
		aml_vm_reg.reg_mtrxi_offst_inp[2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXI_EN_CTRL,
		aml_vm_reg.reg_matrixi_en_ctrl, vpp_index);

	// mtx out
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_COEF00_01,
		(aml_vm_reg.reg_mtrxo_coef[0][0] << 16) |
		(aml_vm_reg.reg_mtrxo_coef[0][1] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_COEF02_10,
		(aml_vm_reg.reg_mtrxo_coef[0][2] << 16) |
		(aml_vm_reg.reg_mtrxo_coef[1][0] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_COEF11_12,
		(aml_vm_reg.reg_mtrxo_coef[1][1] << 16) |
		(aml_vm_reg.reg_mtrxo_coef[1][2] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_COEF20_21,
		(aml_vm_reg.reg_mtrxo_coef[2][0] << 16) |
		(aml_vm_reg.reg_mtrxo_coef[2][1] & 0x1FFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_COEF22,
		aml_vm_reg.reg_mtrxo_coef[2][2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_OFFSET0_1,
		(aml_vm_reg.reg_mtrxo_offst_oup[0] << 16) |
		(aml_vm_reg.reg_mtrxo_offst_oup[1] & 0xFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_OFFSET2,
		aml_vm_reg.reg_mtrxo_offst_oup[2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_PRE_OFFSET0_1,
		(aml_vm_reg.reg_mtrxo_offst_inp[0] << 16) |
		(aml_vm_reg.reg_mtrxo_offst_inp[1] & 0xFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_PRE_OFFSET2,
		aml_vm_reg.reg_mtrxo_offst_inp[2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_MATRIXO_CLIP + reg_offset,
		aml_vm_reg.reg_mtrxo_rs << 5, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_CGAIN_OFFT,
		(aml_vm_reg.reg_cgain_oft[2] << 16) | aml_vm_reg.reg_cgain_oft[1],
		vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_MATRIXO_EN_CTRL,
		aml_vm_reg.reg_matrixo_en_ctrl, vpp_index);

	// config gmut coef
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_GMUT_CTRL,
		aml_vm_reg.reg_new_mode  << 4 | aml_vm_reg.reg_gmut_shift, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_GMUT_COEF0,
		aml_vm_reg.reg_gmut_coef[0][1] << 16 |
		(aml_vm_reg.reg_gmut_coef[0][0] & 0xFFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_GMUT_COEF1,
		aml_vm_reg.reg_gmut_coef[1][0] << 16 |
		(aml_vm_reg.reg_gmut_coef[0][2] & 0xFFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_GMUT_COEF2,
		aml_vm_reg.reg_gmut_coef[1][2] << 16 |
		(aml_vm_reg.reg_gmut_coef[1][1] & 0xFFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_GMUT_COEF3,
		aml_vm_reg.reg_gmut_coef[2][1] << 16 |
		(aml_vm_reg.reg_gmut_coef[2][0] & 0xFFFF), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_GMUT_COEF4,
		aml_vm_reg.reg_gmut_coef[2][2], vpp_index);

	// config c gain
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_CGAIN_COEF0,
		aml_vm_reg.reg_c_gain_lim_coef[1] << 16 |
		aml_vm_reg.reg_c_gain_lim_coef[0], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_CGAIN_COEF1,
		aml_vm_reg.reg_sel_opt << 31 | //reg_sel_opt
		aml_vm_reg.reg_c_gain_lim_maxrgb << 16 | //reg_max_rgb
		aml_vm_reg.reg_c_gain_lim_coef[2], vpp_index);

	// config adaptive scaler
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_CTRL,
		aml_vm_reg.reg_adpscl1_sft << 20 |
		aml_vm_reg.reg_ogain_blend_mode << 17 |
		aml_vm_reg.reg_adpscl_sel_opt << 16 |
		aml_vm_reg.reg_adpscl_max << 8 |
		aml_vm_reg.reg_adpscl_clip_en << 7 |
		aml_vm_reg.reg_adpscl_bypass[2] << 6 |
		aml_vm_reg.reg_adpscl_bypass[1] << 5 |
		aml_vm_reg.reg_adpscl_bypass[0] << 4 |
		aml_vm_reg.reg_adpscl1_mode << 2 |
		aml_vm_reg.reg_adpscl_mode, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_ALPHA0,
		aml_vm_reg.reg_adpscl_alpha[1] << 16 |
		aml_vm_reg.reg_adpscl_alpha[0], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_ALPHA1,
		aml_vm_reg.reg_adpscl_shift0 << 28 |
		aml_vm_reg.reg_adpscl_shift1 << 20 |
		aml_vm_reg.reg_adpscl_shift2[2] << 16 |
		aml_vm_reg.reg_adpscl_alpha[2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_BETA0,
		aml_vm_reg.reg_adpscl_beta_s[0] << 20 |
		aml_vm_reg.reg_adpscl_beta[0], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_BETA1,
		aml_vm_reg.reg_adpscl_beta_s[1] << 20 |
		aml_vm_reg.reg_adpscl_beta[1], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_BETA2,
		aml_vm_reg.reg_adpscl_beta_s[2] << 20 |
		aml_vm_reg.reg_adpscl_beta[2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_COEF0,
		aml_vm_reg.reg_adpscl_ys_coef[1] << 16 |
		aml_vm_reg.reg_adpscl_ys_coef[0], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_ADPS_COEF1,
		aml_vm_reg.reg_adpscl_ys_coef[2], vpp_index);

	//gamut_cmp
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP0 + reg_offset,
		aml_vm_reg.reg_hdr2_gm_comp_en << 31 |
		aml_vm_reg.reg_hdr_comp_ofst_r << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP1 + reg_offset,
		aml_vm_reg.reg_hdr_comp_ofst_g << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP2 + reg_offset,
		aml_vm_reg.reg_hdr_comp_ofst_b << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP3 + reg_offset,
		aml_vm_reg.reg_hdr_comp_min_r << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP4 + reg_offset,
		aml_vm_reg.reg_hdr_comp_min_g << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP5 + reg_offset,
		aml_vm_reg.reg_hdr_comp_min_b << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP6 + reg_offset,
		aml_vm_reg.reg_hdr_comp_rat_r << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP7 + reg_offset,
		aml_vm_reg.reg_hdr_comp_rat_g << 8, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_GMUT_COMP8 + reg_offset,
		aml_vm_reg.reg_hdr_comp_rat_b << 8, vpp_index);

	// ootf1
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_HDR2_CGAIN_VIVID,
		aml_vm_reg.reg_omax_sync_gain_sft << 26 |
		aml_vm_reg.reg_omax_sync_gain << 16 |
		aml_vm_reg.reg_cgain_pos << 2 |
		aml_vm_reg.reg_ogain_inser << 1, vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_ADPSCL1_COEF0 + reg_offset,
	aml_vm_reg.reg_adpscl_ys_coef1[1] << 16 | aml_vm_reg.reg_adpscl_ys_coef1[0],
	vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_ADPSCL1_COEF1 + reg_offset,
	aml_vm_reg.reg_bypass_ootf2_gain << 16 | aml_vm_reg.reg_adpscl_ys_coef1[2],
	vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_RGB_GM_CTRL + reg_offset,
		aml_vm_reg.reg_rgb_gm_mode << 1 |
		aml_vm_reg.reg_rgb_gm_en, vpp_index);

	// config eotf lut
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_EOTF_LUT_ADDR_PORT, 0x0, vpp_index);
	for (i = 0; i < 148; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(L_EOTF_LUT_DATA_PORT,
			p_eotf_lut[i], vpp_index);

	// config oetf lut
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_OETF_LUT_ADDR_PORT, 0x0, vpp_index);
	for (i = 0; i < 74; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(L_OETF_LUT_DATA_PORT,
		(p_oetf_lut[i * 2 + 1] << 16) + p_oetf_lut[i * 2], vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_OETF_LUT_DATA_PORT,
		p_oetf_lut[148], vpp_index);

	// config ogain lut
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_OGAIN_LUT_ADDR_PORT, 0x0, vpp_index);
	for (i = 0; i < 74; i++) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(L_OGAIN_LUT_DATA_PORT,
			(p_ootf_lut0[i * 2 + 1] << 16) + p_ootf_lut0[i * 2], vpp_index);
	}
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_OGAIN_LUT_DATA_PORT,
		p_ootf_lut0[148], vpp_index);

	//config ogain lut1
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_OGAIN_LUT1_ADDR_PORT, 0x0, vpp_index);
	for (i = 0; i < 149; i++) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(L_OGAIN_LUT1_DATA_PORT,
			p_ootf_lut1[i], vpp_index);
	}
	// config cgain lut
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_CGAIN_LUT_ADDR_PORT, 0x0, vpp_index);
	for (i = 0; i < 32; i++) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(L_CGAIN_LUT_DATA_PORT,
			(p_cgain_lut[i * 2 + 1] << 16) + p_cgain_lut[i * 2], vpp_index);
	}
	VSYNC_WRITE_VPP_REG_VPP_SEL(L_CGAIN_LUT_DATA_PORT,
		p_cgain_lut[64], vpp_index);
}

unsigned int hdr_test_case_set_flag;
struct am_regs_s *test_case_reg;
struct hdr_test_case_lut_s *test_case_lut;
int hdr_core_test_flag;
int hdr_core_test_sel;
int hdr_core_test_mode;

static void set_hdr_test_case(int vpp_index)
{
	unsigned int i = 0;
	unsigned int reg_eotf_addr;
	unsigned int reg_eotf_data;
	unsigned int reg_oetf_addr;
	unsigned int reg_oetf_data;
	unsigned int reg_cgain_addr;
	unsigned int reg_cgain_data;
	unsigned int reg_ogain_addr;
	unsigned int reg_ogain_data;
	unsigned int reg_ogain_ext_addr;
	unsigned int reg_ogain_ext_data;
	int offset_addr_vd1 = 0;
	int offset_addr_vd2 = 0x1700;
//	int offset_addr_vd3 = 0;

	if (!hdr_test_case_set_flag)
		return;

	pr_info("%s: hdr_test_case_set_flag = %d\n", __func__,
		hdr_test_case_set_flag);

	if (chip_type_id == chip_t6w ||
		chip_type_id == chip_a9) {
		offset_addr_vd1 = 0x1600;
		offset_addr_vd2 = 0x80;
	}

	reg_eotf_addr = 0x4c1e + offset_addr_vd1;
	reg_eotf_data = 0x4c1f + offset_addr_vd1;
	reg_oetf_addr = 0x4c20 + offset_addr_vd1;
	reg_oetf_data = 0x4c21 + offset_addr_vd1;
	reg_ogain_addr = 0x4c26 + offset_addr_vd1;
	reg_ogain_data = 0x4c27 + offset_addr_vd1;
	reg_ogain_ext_addr = 0x4c40 + offset_addr_vd1;
	reg_ogain_ext_data = 0x4c41 + offset_addr_vd1;
	reg_cgain_addr = 0x4c22 + offset_addr_vd1;
	reg_cgain_data = 0x4c23 + offset_addr_vd1;

	if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_CUVA_SDR_SC1 ||
		hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_SC1 ||
		hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_STATIC) {
		reg_eotf_addr += offset_addr_vd2;
		reg_eotf_data += offset_addr_vd2;
		reg_oetf_addr += offset_addr_vd2;
		reg_oetf_data += offset_addr_vd2;
		reg_ogain_addr += offset_addr_vd2;
		reg_ogain_data += offset_addr_vd2;
		reg_ogain_ext_addr += offset_addr_vd2;
		reg_ogain_ext_data += offset_addr_vd2;
		reg_cgain_addr += offset_addr_vd2;
		reg_cgain_data += offset_addr_vd2;
		for (i = 0; i < test_case_reg->length; i++)
			test_case_reg->am_reg[i].addr += offset_addr_vd2;
	} else if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD1_1_CUVA_SDR_SC1) {
		reg_eotf_addr += 0x80;
		reg_eotf_data += 0x80;
		reg_oetf_addr += 0x80;
		reg_oetf_data += 0x80;
		reg_ogain_addr += 0x80;
		reg_ogain_data += 0x80;
		reg_ogain_ext_addr += 0x80;
		reg_ogain_ext_data += 0x80;
		reg_cgain_addr += 0x80;
		reg_cgain_data += 0x80;
		for (i = 0; i < test_case_reg->length; i++)
			test_case_reg->am_reg[i].addr += 0x80;
	} else if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_1_CUVA_SDR_SC1) {
		reg_eotf_addr += 0x1780;
		reg_eotf_data += 0x1780;
		reg_oetf_addr += 0x1780;
		reg_oetf_data += 0x1780;
		reg_ogain_addr += 0x1780;
		reg_ogain_data += 0x1780;
		reg_ogain_ext_addr += 0x1780;
		reg_ogain_ext_data += 0x1780;
		reg_cgain_addr += 0x1780;
		reg_cgain_data += 0x1780;
		for (i = 0; i < test_case_reg->length; i++)
			test_case_reg->am_reg[i].addr += 0x1780;
	}

	if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_HDR_SDR_SR ||
		hdr_test_case_set_flag == EN_HDR_TEST_CASE_SDR_HDR_SR ||
		hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_HDR_SDR_SR ||
		hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_SDR_HDR_SR) {
		hdr_test_set_regs(hdr_test_case_set_flag, vpp_index);
		hdr_test_case_set_flag = 0;
		pr_info("%s: hdr_test_case_set_flag = %d", __func__,
				hdr_test_case_set_flag);
		return;
	}

	am_set_regmap(test_case_reg, 0);
	pr_info("%s: am_set_regmap\n  addr = %x", __func__,
		test_case_reg->am_reg[0].addr);

	VSYNC_WRITE_VPP_REG_VPP_SEL(reg_eotf_addr, 0x0, vpp_index);
	for (i = 0; i < 148; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_eotf_data,
			test_case_lut->lut_eotf[i], vpp_index);
	pr_info("%s: eotf set addr = %x\n", __func__, reg_eotf_addr);

	VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ogain_addr, 0x0, vpp_index);
	for (i = 0; i < 75; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ogain_data,
			test_case_lut->lut_ogain[i], vpp_index);

	if (chip_type_id != chip_a9) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_oetf_addr, 0x0, vpp_index);
		for (i = 0; i < 75; i++)
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_oetf_data,
				test_case_lut->lut_oetf[i], vpp_index);
	}

	VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ogain_ext_addr, 0x0, vpp_index);

	if (chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		for (i = 0; i < 74; i++) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ogain_ext_data,
				(test_case_lut->lut_ogain_ext[i] & 0xffff), vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ogain_ext_data,
				(test_case_lut->lut_ogain_ext[i] >> 16), vpp_index);
		}
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ogain_ext_data,
			(test_case_lut->lut_ogain_ext[74] & 0xffff), vpp_index);
	} else {
		for (i = 0; i < 75; i++) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ogain_ext_data,
				test_case_lut->lut_ogain_ext[i], vpp_index);
		}
	}

	pr_info("%s: oetf(%x)/ogain(%x)/ogain_ext(%x) set\n", __func__,
		reg_oetf_addr, reg_ogain_addr, reg_ogain_ext_addr);

	VSYNC_WRITE_VPP_REG_VPP_SEL(reg_cgain_addr, 0x0, vpp_index);
	for (i = 0; i < 33; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_cgain_data,
			test_case_lut->lut_cgain[i], vpp_index);
	pr_info("%s: cgain set addr = %x\n", __func__, reg_cgain_addr);

	if (chip_type_id == chip_t6x) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_ADPSCL1_COEF0,
			0x04000400, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_ADPSCL1_COEF1,
			0xc8000400, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(DOLBY_HDR2_RGB_GM_CTRL, 0x4, vpp_index);
	} else if (chip_type_id == chip_a9) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(VD1_HDR2_ADPSCL1_COEF0 + 0x2a00,
			0x04000400, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(VD1_HDR2_ADPSCL1_COEF1 + 0x2a00,
			0xc8000400, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(VD1_HDR2_RGB_GM_CTRL + 0x2a00,
			0x4, vpp_index);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VD1_HDR2_CGAIN_COEF1 + 0x2a00,
			0xfff, 16, 13, vpp_index);

		if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_CUVA_SDR_SC1 ||
			hdr_test_case_set_flag == EN_HDR_TEST_CASE_CUVAHLG_SDR_SC1 ||
			hdr_test_case_set_flag == EN_HDR_TEST_CASE_CUVAHLG_SDR_STATIC)
			set_oetf_lut_by_type(VD1_HDR, 0, 12, vpp_index);
	}

	//reset reg addr
	if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_CUVA_SDR_SC1 ||
		hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_SC1 ||
		hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_STATIC) {
		for (i = 0; i < test_case_reg->length; i++)
			test_case_reg->am_reg[i].addr -= 0x1700;
	} else if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD1_1_CUVA_SDR_SC1) {
		for (i = 0; i < test_case_reg->length; i++)
			test_case_reg->am_reg[i].addr -= 0x80;
	} else if (hdr_test_case_set_flag == EN_HDR_TEST_CASE_VD2_1_CUVA_SDR_SC1) {
		for (i = 0; i < test_case_reg->length; i++)
			test_case_reg->am_reg[i].addr -= 0x1780;
	}

	hdr_test_case_set_flag = 0;
}

void test_hdr_core(int path_idx, int proc_mode, enum vpp_index_e vpp_index)
{
	enum hdr_module_sel module_sel;
	enum hdr_process_sel hdr_process_select;

	if (!path_idx || !proc_mode)
		return;

	if (path_idx == 2)
		module_sel = VD2_HDR;
	else if (path_idx == 3)
		module_sel = VD3_HDR;
	else if (path_idx == 4)
		module_sel = OSD1_HDR;
	else if (path_idx == 5)
		module_sel = OSD2_HDR;
	else
		module_sel = VD1_HDR;

	if (proc_mode == 2)
		hdr_process_select = SDR_HDR;
	else if (proc_mode == 3)
		hdr_process_select = HDR_SDR;
	else if (proc_mode == 4)
		hdr_process_select = SDR_HLG;
	else if (proc_mode == 5)
		hdr_process_select = HLG_SDR;
	else
		hdr_process_select = HDR_BYPASS;

	hdr_func(module_sel, hdr_process_select,
		NULL, NULL, vpp_index);
}

unsigned int dpss_test_case_set_flag;
unsigned int dpss_test_case_index;

static void set_dpss_hdr_test_case(int vpp_index)
{
	enum vpp_muxio_sel_e sel = VPP_MUXIO_SEL_DE_LINK;

	if (!dpss_test_case_set_flag)
		return;

	switch (dpss_test_case_index) {
	case 0:
	default:
		sel = VPP_MUXIO_SEL_DE_LINK;
		break;
	case 1:
		sel = VPP_MUXIO_SEL_VD1_HDR;
		break;
	case 2:
		sel = VPP_MUXIO_SEL_DPSS_HDR_DPE;
		break;
	case 3:
		sel = VPP_MUXIO_SEL_DPSS_HDR;
		break;
	case 4:
		sel = VPP_MUXIO_SEL_DPSS_DCT_HDR;
		break;
	case 5:
		sel = VPP_MUXIO_SEL_LC_EVC_HDR;
		break;
	}

	muxio_config(sel, 1, vpp_index);
	dpss_test_case_set_flag = 0;
}

static int g_gamma_lut[65] = {
	0, 154, 211, 254, 290, 321, 348, 374, 397, 419, 439, 459, 477, 495,
	512, 529, 544, 559, 574, 589, 602, 616, 629, 642, 655, 667, 679,
	691, 702, 713, 724, 735, 746, 757, 767, 777, 787, 797, 807, 816,
	826, 835, 844, 853, 862, 871, 880, 889, 897, 906, 914, 922, 930,
	938, 946, 954, 962, 970, 978, 985, 993, 1000, 1008, 1015, 1023
};

int rgb2ycbcr_10b[9] = {
	230, 594, 52, -125, -323, 448, 448, -412, -36
};

int gamut_2020_709_8bit[9] = {
	425, -150, -18, -31, 290, -2, -4, -25, 286
};

void gamut_test_case(int test_case)
{
	int i = 0;
	gamut_mapping0_en = 1;
	gamut_mapping1_en = 1;
	gamut_mapping_wrapper_init();

	if (test_case == 0) {
		gamut_mapping0_param.mtx_en = 1;
		gamut_mapping0_param.eotf_en = 1;
		gamut_mapping0_param.oetf_en = 1;
		gamut_mapping0_param.ootf_en = 1;
		for (i = 0; i < 9; i++)
			gamut_mapping0_param.mtx[i / 3][i % 3] = gamut_2020_709_8bit[i];
		gamut_mapping0_param.flag_update_mtx = 1;
		WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL, 0x0, 0, 1);
		vpp_enable_lut3d(0);
		gamut_mapping1_en = 0;
	} else if (test_case == 1) {
		gamut_mapping0_param.mtx_en = 1;
		gamut_mapping0_param.eotf_en = 1;
		gamut_mapping0_param.oetf_en = 1;
		gamut_mapping0_param.ootf_en = 1;
		gamut_mapping0_param.gmt0_after_osd = 1;
		for (i = 0; i < 9; i++)
			gamut_mapping0_param.mtx[i / 3][i % 3] = gamut_2020_709_8bit[i];
		gamut_mapping0_param.flag_update_mtx = 1;
		WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL, 0x0, 0, 1);
		vpp_enable_lut3d(0);
		gamut_mapping1_en = 0;
	} else if (test_case == 2) {
		gamut_mapping1_param.mtx_en = 0;
		gamut_mapping1_param.eotf_en = 1;
		gamut_mapping1_param.oetf_en = 0;
		pre_gamma.en = 1;
		memcpy(pre_gamma.lut_r,
		       g_gamma_lut, 65 * sizeof(unsigned int));
		memcpy(pre_gamma.lut_g,
		       g_gamma_lut, 65 * sizeof(unsigned int));
		memcpy(pre_gamma.lut_b,
		       g_gamma_lut, 65 * sizeof(unsigned int));
		vecm_latch_flag2 |= VPP_PRE_GAMMA_UPDATE;

		lut3d_test(1, 1);
		gamut_mapping0_en = 0;
	} else if (test_case == 3) {
		gamut_mapping1_param.mtx_en = 1;
		gamut_mapping1_param.eotf_en = 1;
		gamut_mapping1_param.oetf_en = 1;
		gamut_mapping1_param.gmt1_3dlut_inside = 1;
		for (i = 0; i < 9; i++)
			gamut_mapping1_param.mtx[i / 3][i % 3] = rgb2ycbcr_10b[i];
		gamut_mapping1_param.flag_update_mtx = 1;
		gamut_mapping1_param.mtx_rs = 2;
		gamut_mapping1_param.mtx_pos_offset[1] = ((1 << 19) >> 5);
		gamut_mapping1_param.mtx_pos_offset[2] = ((1 << 19) >> 5);
		WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL, 0x0, 0, 1);
		lut3d_test(2, 1);
		gamut_mapping0_en = 0;
	} else if (test_case == 4) { //reset lut
		memset(&gamut_mapping0_param, 0, sizeof(gamut_mapping0_param));
		memset(&gamut_mapping1_param, 0, sizeof(gamut_mapping1_param));
		gamut_mapping0_param.flag_update_lut = 1;
		gamut_mapping1_param.flag_update_lut = 1;
		WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL, 0x0, 0, 1);
		vpp_enable_lut3d(0);
	} else {
		return;
	}

	vecm_latch_flag2 |= FLAG_GAMUT_MAPPING0_UPDATE;
	vecm_latch_flag2 |= FLAG_GAMUT_MAPPING1_UPDATE;
}
#endif

static int pre_mute_state;
void vpp_vadj1_align_vd1_mute(void)
{
	int vadj1_en = 0;
	int mute_state;

	if (probe_ok == 0)
		return;

	mute_state = get_video_mute();
	if (pre_mute_state != mute_state) {
		if (!get_video_mute()) {
			if (dv_pq_bypass == 1 || dv_pq_bypass == 2)
				vadj1_en = dv_cfg_bypass.vadj1_en;
			else
				vadj1_en = pq_cfg.vadj1_en;
		} else {
			vadj1_en = 0;
		}
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_MISC,
					vadj1_en, 0, 1, VPP_TOP0);
		pre_mute_state = mute_state;
		pr_amvecm_dbg("%s: mute_state:%d, vadj1_en:%d\n",
			__func__, mute_state, vadj1_en);
	}
}
EXPORT_SYMBOL(vpp_vadj1_align_vd1_mute);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
bool mosaic_mode;
static int amvecm_pre_matrix_process(struct vframe_s *toggle_vf,
			  struct vframe_s *vf, int flags,
			  enum vd_path_e vd_path, enum vpp_index_e vpp_index)
{
	int result = 0;
	unsigned int mosaic_idx[4] = {0, 2, 1, 3};
	enum vd_path_e mosaic_vd_path[4] = {VD1_PATH, VD1_1_PATH,
		VD2_PATH, VD2_1_PATH};
	unsigned int i = 0;
	struct vframe_s *toggle_vf_mosaic = NULL;
	struct vframe_s *vf_mosaic = NULL;

	if (vd_path == VD1_PATH) {
		if ((toggle_vf && toggle_vf->type_ext & VIDTYPE_EXT_MOSAIC_22) ||
			(vf && vf->type_ext & VIDTYPE_EXT_MOSAIC_22)) {
			mosaic_mode = 1;
			am_dma_updat_hdr2_hist(mosaic_mode);
			for (i = 0; i < 4; i++) {
				if (toggle_vf && toggle_vf->vc_private)
					toggle_vf_mosaic =
						toggle_vf->vc_private->mosaic_vf[mosaic_idx[i]];
				else
					toggle_vf_mosaic = NULL;

				if (vf && vf->vc_private)
					vf_mosaic = vf->vc_private->mosaic_vf[mosaic_idx[i]];
				else
					vf_mosaic = NULL;

				result |= amvecm_matrix_process(toggle_vf_mosaic,
					vf_mosaic, flags, mosaic_vd_path[i], vpp_index);
			}
		} else {
			if (mosaic_mode) {// reset 2*2 hdr status when mosaic_mode exit
				for (i = 0; i < 4; i++) {
					if (toggle_vf && toggle_vf->vc_private)
						toggle_vf_mosaic =
						toggle_vf->vc_private->mosaic_vf[mosaic_idx[i]];
					else
						toggle_vf_mosaic = NULL;

					if (vf && vf->vc_private)
						vf_mosaic =
						vf->vc_private->mosaic_vf[mosaic_idx[i]];
					else
						vf_mosaic = NULL;

					result |= amvecm_matrix_process(toggle_vf_mosaic,
						vf_mosaic, flags, mosaic_vd_path[i], vpp_index);
				}
				mosaic_mode = 0;
				am_dma_updat_hdr2_hist(mosaic_mode);
			} else {
				result = amvecm_matrix_process(toggle_vf, vf, flags, vd_path,
					vpp_index);
			}
		}
	} else {
		if (!mosaic_mode)// bypass vd2 hdr process when mosaic_mode on
			result = amvecm_matrix_process(toggle_vf, vf, flags, vd_path,
				vpp_index);
	}

	return result;
}
#endif

int amvecm_on_vs(struct vframe_s *vf,
		 struct vframe_s *toggle_vf,
		 int flags,
		 struct vpq_size_s vpq_size,
		 enum vd_path_e vd_path,
		 enum vpp_index_e vpp_index)
{
	int result = 0;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int vf_state = 0;
	int i = 0;
#endif
#ifdef T7_BRINGUP_MULTI_VPP
	// to do, t7 vecm bringup,
	int vpp_top_index = 0;
	// temp flag, should update it from video display module in future
	// assuming here that post process only be used for vd1 input of vpp top0
	// and there is no post process for vpp top 1/2
#endif

	if (probe_ok == 0)
		return 0;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	//if (vd_path == VD1_PATH)
	//	update_muxio_mode(toggle_vf ? toggle_vf : vf, vpp_index);
#endif

	if (vd_path == VD1_PATH)
		set_vpp_enh_clk(toggle_vf, vf, vpp_index);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if ((is_meson_s5_cpu() || is_meson_t3x_cpu()) &&
		(vd_path == VD1_PATH)) {
		if (!ai_color_enable) {
			if (pre_aiclr_en == 1) {
				disable_ai_color(vpp_index);
				pre_aiclr_en = 0;
			}
		} else {
			pre_aiclr_en = 1;
		}
	}
#endif

#ifdef T7_BRINGUP_MULTI_VPP
	// todo, will not support in pxp bringup stage
	if ((get_cpu_type() == MESON_CPU_MAJOR_ID_T7 ||
		chip_type_id == chip_a9) &&
		vpp_top_index != 0) {
		// for t7 case, for min vpp 1 & 2
		// keep the legacy driver for vd1 not changed for back compatible
		//and try to minimum the changes and add independent handler for min vpp newly added
		result = min_vpp_process(vpp_top_index, vpp_index);
		return result;
	}
#endif
	amvecm_overscan_process(vf, toggle_vf, flags, vd_path, vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] overscan_process done.\n");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (for_amdv_certification() && vd_path == VD1_PATH)
		return 0;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (!dnlp_insmod_ok && vd_path == VD1_PATH)
		dnlp_alg_param_init();

	pr_amvecm_bringup_dbg("[on_vs] dnlp_alg done %d, %d.\n",
		dnlp_insmod_ok, flags);

	if (flags & CSC_FLAG_CHECK_OUTPUT) {
		pr_amvecm_bringup_dbg("[on_vs] CSC_FLAG_CHECK_OUTPUT\n");
		/* to test if output will change */
		return amvecm_pre_matrix_process(toggle_vf, vf, flags, vd_path, vpp_index);

	} else if (vd_path == VD1_PATH) {
		send_hdr10_plus_pkt(vd_path, vpp_index);
		send_cuva_pkt(vd_path, vpp_index);
		pr_amvecm_bringup_dbg("[on_vs] VD1_PATH\n");
	}

	if ((toggle_vf) || (vf)) {
		/* matrix adjust */
		if (resume_mtx_flag_get()) {
			flags |= CSC_FLAG_FORCE_SIGNAL;
			resume_mtx_flag_set(false);
		}
		result = amvecm_pre_matrix_process(toggle_vf, vf, flags, vd_path,
			vpp_index);

		pr_amvecm_bringup_dbg("[on_vs] matrix_process done.\n");

		if (toggle_vf) {
			ioctrl_get_hdr_metadata(toggle_vf);
			vf_state = cabc_add_hist_proc(toggle_vf, &vpp_hist_param);
		}
		pr_amvecm_bringup_dbg("[on_vs] cabc_add done.\n");

		if (vd_path == VD1_PATH)
			amvecm_size_patch(toggle_vf ? toggle_vf : vf,
				vpq_size.cm_hsize, vpq_size.cm_vsize, vpp_index);
		pr_amvecm_bringup_dbg("[on_vs] size_patch done.\n");

		if (toggle_vf && vd_path == VD1_PATH) {
			if (chip_type_id != chip_t3x) {
				lc_process(toggle_vf, &vpq_size,
					vpp_index, &vpp_hist_param);
			} else {
				if ((toggle_vf->flag & VFRAME_FLAG_COMPOSER_DONE) &&
					toggle_vf->composer_info &&
					toggle_vf->composer_info->count > 1) {
					/*for multi-path to vd1*/
					lc_process(NULL, &vpq_size, vpp_index, &vpp_hist_param);
					pr_amvecm_bringup_dbg("[on_vs] lc multi-path to vd1.\n");
					if (!slt_en)
						dnlp_en_dsw = 0;
				} else {
					lc_process(toggle_vf, &vpq_size,
						vpp_index, &vpp_hist_param);
					if (!slt_en)
						dnlp_en_dsw = 1;
				}
			}
			pr_amvecm_bringup_dbg("[on_vs] lc_proc done.\n");
			/*1080i pulldown combing workaround*/
			amvecm_dejaggy_patch(toggle_vf);
			pr_amvecm_bringup_dbg("[on_vs] dejaggy_patch done.\n");
			if ((get_cpu_type() == MESON_CPU_MAJOR_ID_T3) ||
				(get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) ||
				chip_type_id == chip_t5m) {
				/*frequence meter size config*/
				amve_fmetersize_config(toggle_vf->width, toggle_vf->height,
					vpq_size.sr1_in_hsize, vpq_size.sr1_in_vsize, vpp_index);
				amvecm_fmeter_process(toggle_vf, vpp_index);
			}
			vpp_hist_shft_cfg(toggle_vf);
		}
		/*refresh vframe*/
		if (!toggle_vf && vf && vd_path == VD1_PATH) {
			if (chip_type_id != chip_t3x) {
				lc_process(vf, &vpq_size, vpp_index, &vpp_hist_param);
			} else {
				if ((vf->flag & VFRAME_FLAG_COMPOSER_DONE) &&
					vf->composer_info &&
					vf->composer_info->count > 1) {
					/*for multi-path to vd1*/
					lc_process(NULL, &vpq_size, vpp_index, &vpp_hist_param);
					pr_amvecm_bringup_dbg("[on_vs] lc multi-path to vd1.\n");
					if (!slt_en)
						dnlp_en_dsw = 0;
				} else {
					lc_process(vf, &vpq_size, vpp_index, &vpp_hist_param);
					if (!slt_en)
						dnlp_en_dsw = 1;
				}
			}

			vf_state = cabc_add_hist_proc(vf, &vpp_hist_param);
		}
		pr_amvecm_bringup_dbg("[on_vs] refresh vframe done.\n");

		vpp_demo_func(toggle_vf ? toggle_vf : vf,
			&vpq_size, vpp_index);
		pr_amvecm_bringup_dbg("[on_vs] demo_func done.\n");
	} else {
		result = amvecm_pre_matrix_process(NULL, NULL, flags, vd_path,
			vpp_index);
		pr_amvecm_bringup_dbg("[on_vs] matrix_process else done.\n");

		if (vd_path == VD1_PATH) {
			amvecm_size_patch(NULL, 0, 0, 0);
			lc_process(NULL, &vpq_size, vpp_index, &vpp_hist_param);
			pr_amvecm_bringup_dbg("[on_vs] lc_proc else done.\n");

			/*1080i pulldown combing workaround*/
			amvecm_dejaggy_patch(NULL);
			pr_amvecm_bringup_dbg("[on_vs] dejaggy_patch else done.\n");
			pr_amvecm_bringup_dbg("[on_vs] size_patch else done.\n");
			vpp_demo_func(NULL, &vpq_size, vpp_index);
			pr_amvecm_bringup_dbg("[on_vs] demo_func else done.\n");
			hdr_first_frame_flag_update();
			vpp_hist_shft_cfg(NULL);
		}
		vf_state = cabc_add_hist_proc(NULL, &vpp_hist_param);
		pr_amvecm_bringup_dbg("[on_vs] cabc_add done.\n");
	}

	if (chip_type_id == chip_t3x)
		amvecm_osd_matrix_process(vd_path, vpp_index);
#endif

	if (vd_path != VD1_PATH)
		return result;

	/* add some flag to trigger */
	if (vf) {
		/*gxlx sharpness adaptive setting*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (is_meson_gxlx_cpu())
			amve_sharpness_adaptive_setting(vf,
				vpq_size.sr1_hsc_en, vpq_size.sr1_vsc_en, vpp_index);
#endif
#endif
		amvecm_bricon_process(vd1_brightness,
			vd1_contrast + vd1_contrast_offset, vf, vpp_index);
		pr_amvecm_bringup_dbg("[on_vs] bc_proc done.\n");

		amvecm_color_process(saturation_pre + saturation_offset,
			hue_pre, vf, vpp_index);
		pr_amvecm_bringup_dbg("[on_vs] color_proc done.\n");
	}

	/* pq latch process */
	amvecm_video_latch(vpp_index);
	pr_amvecm_bringup_dbg("[on_vs] video_latch done.\n");

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/*wq for cacb and aad*/
	cabc_aad_on_vs(vf_state);
	pr_amvecm_bringup_dbg("[on_vs] cabc_aad done.\n");

	if (chip_type_id >= chip_s7d &&
		vsr_update_flag)
		amve_vsr_config_update(vf, vpp_index);

	if (chip_type_id >= chip_t6d)
		amve_444_config_update(toggle_vf ? toggle_vf : vf,
			vpp_index);

	set_hdr_test_case(vpp_index);
	amvecm_dump_output_data();

	set_dpss_hdr_test_case(vpp_index);

	if (chip_type_id == chip_a9 && dump_flag == 1) {
		WRITE_VPP_REG_BITS(VPP_PST_STA_CTRL, 0, 16, 16);
		for (i = 0; i < 64; i++)
			pr_info("[%d] 0x%08x\n",
				i, READ_VPP_REG(VPP_PST_STA_RO_HIST));
		dump_flag = 0;
	}

	if (hdr_core_test_flag) {
		test_hdr_core(hdr_core_test_sel, hdr_core_test_mode, vpp_index);
		hdr_core_test_flag = 0;
	}
#endif
	return result;
}
EXPORT_SYMBOL(amvecm_on_vs);

void refresh_on_vs(struct vframe_s *vf, struct vframe_s *rpt_vf, u32 vpp_index)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int ave = -1;

	if (probe_ok == 0)
		return;

#ifdef T7_BRINGUP_MULTI_VPP
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T7 &&
		vpp_top_index != 0) {
		return 0;
	}
#endif

	if (vf || rpt_vf) {
		vpp_get_vframe_hist_info(vf ? vf : rpt_vf,
			&vpp_hist_param, vpp_index);
		pr_amvecm_bringup_dbg("[on_vs] refresh get_vframe_hist_info done.\n");
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!for_amdv_certification())
#endif
			ve_on_vs(vf ? vf : rpt_vf, vpp_index, &vpp_hist_param);
		pr_amvecm_bringup_dbg("[on_vs] ve_on_vs done.\n");
		if (is_video_layer_on(VD1_PATH)) {
			ve_hist_gamma_tgt(vf ? vf : rpt_vf, &vpp_hist_param);
	//		vpp_backup_histgram(vf ? vf : rpt_vf);
			pr_amvecm_bringup_dbg("[on_vs] hist related done.\n");
		}
		pattern_detect(vf ? vf : rpt_vf);
		pr_amvecm_bringup_dbg("[on_vs] pattern_detect done.\n");
	} else {
		ve_hist_gamma_reset();
		set_lum_ave(ave);
		pr_amvecm_bringup_dbg("[on_vs] hist reset done.\n");
	}
#endif
}
EXPORT_SYMBOL(refresh_on_vs);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static irqreturn_t amvecm_viu2_vsync_isr(int irq, void *dev_id)
{
	if (vpp_get_encl_viu_mux() == 2)
		ve_lcd_gamma_process(0);
	return IRQ_HANDLED;
}

static irqreturn_t amvecm_lc_curve_isr(int irq, void *dev_id)
{
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_IOTRACE)
	iotrace_misc_record_write(RECORD_TYPE_AMVECM_IN, 0, 0, 0);
#endif

	if (use_lc_curve_isr) /* && chip_type_id != chip_t3x)*/
		lc_read_region(8, 12, 0);

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_IOTRACE)
	iotrace_misc_record_write(RECORD_TYPE_AMVECM_OUT, 0, 0, 0);
#endif

	return IRQ_HANDLED;
}
#endif

static int amvecm_open(struct inode *inode, struct file *file)
{
	struct amvecm_dev_s *devp;
	/* Get the per-device structure that contains this cdev */
	devp = container_of(inode->i_cdev, struct amvecm_dev_s, cdev);
	file->private_data = devp;
	/*init queue*/
	init_waitqueue_head(&devp->hdr_queue);
	return 0;
}

static int amvecm_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
static struct am_regs_s amregs_ext;
#endif
struct ve_pq_overscan_s overscan_table[TIMING_MAX];

static int parse_para_pq(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	if (!params)
		return -ENOMEM;
	params_base = params;
	token = params;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) ||
				 !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0)
			break;
		if (!token || kstrtoint(token, 0, &res) < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

int amvecm_set_saturation_hue(int mab, enum wr_md_e mode, int vpp_index)
{
	s16 mc = 0, md = 0;
	s16 ma, mb;

	if (mab & 0xfc00fc00)
		return -EINVAL;
	ma = (s16)((mab << 6) >> 22);
	mb = (s16)((mab << 22) >> 22);

	saturation_ma = ma - 0x100;
	saturation_mb = mb;

	ma += saturation_ma_shift;
	mb += saturation_mb_shift;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_color_mab_set(mab, VE_VADJ1, mode, vpp_index);
	} else {
#endif
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			if (mode == WR_VCB)
				WRITE_VPP_REG(VPP_VADJ1_MA_MB_2, mab);
			else
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MA_MB_2, mab, vpp_index);
		} else {
			if (mode == WR_VCB)
				WRITE_VPP_REG(VPP_VADJ1_MA_MB, mab);
			else
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MA_MB, mab, vpp_index);
		}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	}
#endif

	mc = (s16)((mab << 22) >> 22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc <  -512)
		mc = -512;
	md = (s16)((mab << 6) >> 22);  /* md =  ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_color_mcd_set(mab, VE_VADJ1, mode, vpp_index);
	} else {
#endif
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			if (mode == WR_VCB) {
				WRITE_VPP_REG(VPP_VADJ1_MC_MD_2, mab);
				/*WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);*/
			} else {
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MC_MD_2, mab, vpp_index);
				/*VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_MISC,*/
				/*	1, 0, 1, vpp_index);*/
			}
		} else {
			if (mode == WR_VCB) {
				WRITE_VPP_REG(VPP_VADJ1_MC_MD, mab);
				/*WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);*/
			} else {
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MC_MD, mab, vpp_index);
				/*VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL,*/
				/*	1, 0, 1, vpp_index);*/
			}
		}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	}
#endif

	pr_amvecm_dbg("%s set video_saturation_hue OK!!!\n", __func__);
	return 0;
}

static int amvecm_set_saturation_hue_post(int val1,
	int val2)
{
	int i, ma, mb, mab, mc, md;
	int hue_cos_len, hue_sin_len;
	int hue_cos[] = {
		/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250,
		248, 247, 245, 243, 241, 239, 237, 234, 231, 229,
		226, 223, 220, 216, 213, 209  /*13~25*/
	};
	int hue_sin[] = {
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		-98, -92, -86, -80, /*-25~-13*/-74,  -68,  -62,  -56,
		-50,  -44,  -38,  -31,  -25, -19, -13,  -6, /*-12~-1*/
		0, /*0*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,
		62,	68,  74,      /*1~12*/	80,   86,   92,	98,  104,
		109,  115,  121,  126,  132, 137, 142, 147 /*13~25*/
	};
	hue_cos_len = sizeof(hue_cos) / sizeof(int);
	hue_sin_len = sizeof(hue_sin) / sizeof(int);
	i = (val2 > 0) ? val2 : -val2;
	if (val1 < -128 || val1 > 128 ||
	    val2 < -25 || val2 > 25 ||
	    i >= hue_cos_len ||
	    (val2 >= (hue_sin_len - 25)))
		return -EINVAL;
	saturation_post = val1;
	hue_post = val2;
	ma = (hue_cos[i] * (saturation_post + 128)) >> 7;
	mb = (hue_sin[25 + hue_post] * (saturation_post + 128)) >> 7;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);
	pr_info("\n[amvideo..] saturation_post:%d hue_post:%d mab:%x\n",
		saturation_post, hue_post, mab);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5)
		WRITE_VPP_REG(VOUT_VADJ_MA_MB, mab);
	else if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		ve_color_mab_set(mab, VE_VADJ2, WR_VCB, 0);
	else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		WRITE_VPP_REG(VPP_VADJ2_MA_MB_2, mab);
	else
		WRITE_VPP_REG(VPP_VADJ2_MA_MB, mab);
	mc = (s16)((mab << 22) >> 22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab << 6) >> 22);  /* md =	ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		WRITE_VPP_REG(VOUT_VADJ_MC_MD, mab);
	} else if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_color_mcd_set(mab, VE_VADJ2, WR_VCB, 0);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		WRITE_VPP_REG(VPP_VADJ2_MC_MD_2, mab);
		/*WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 0, 1);*/
	} else {
		WRITE_VPP_REG(VPP_VADJ2_MC_MD, mab);
		/*WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);*/
	}
	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static int gamma_table_compare(struct tcon_gamma_table_s *table1,
			       struct tcon_gamma_table_s *table2)
{
	int i = 0, flag = 0;

	for (i = 0; i < 256; i++)
		if (table1->data[i] != table2->data[i]) {
			flag = 1;
			break;
		}

	return flag;
}
#endif

static void parse_overscan_table(unsigned int length,
	struct ve_pq_table_s *amvecm_pq_load_table)
{
	unsigned int i;
	unsigned int offset = TIMING_UHD + 1;

	pr_amvecm_dbg("%s pre load_flag[0]/[%d] = %d/%d\n",
		__func__, offset,
		overscan_table[0].load_flag, overscan_table[offset].load_flag);

	memset(overscan_table, 0, sizeof(overscan_table));
	for (i = 0; i < length; i++) {
		overscan_table[i].load_flag =
			(amvecm_pq_load_table[i].src_timing >> 31) & 0x1;
		overscan_table[i].afd_enable =
			(amvecm_pq_load_table[i].src_timing >> 30) & 0x1;
		overscan_table[i].screen_mode =
			(amvecm_pq_load_table[i].src_timing >> 24) & 0x3f;
		overscan_table[i].source =
			(amvecm_pq_load_table[i].src_timing >> 16) & 0xff;
		overscan_table[i].timing =
			amvecm_pq_load_table[i].src_timing & 0xffff;
		overscan_table[i].hs =
			amvecm_pq_load_table[i].value1 & 0xffff;
		overscan_table[i].he =
			(amvecm_pq_load_table[i].value1 >> 16) & 0xffff;
		overscan_table[i].vs =
			amvecm_pq_load_table[i].value2 & 0xffff;
		overscan_table[i].ve =
				(amvecm_pq_load_table[i].value2 >> 16) & 0xffff;
	}

	/* overscan reset for dtv auto afd set.
	 * if auto set load_flag = 0 by user, overscan set by dtv afd
	 */
	if (!overscan_table[0].load_flag &&
		!overscan_table[offset].load_flag &&
		(chip_type_id != chip_t3x &&
		chip_type_id != chip_t6d))
		pq_user_latch_flag |= PQ_USER_OVERSCAN_RESET;

	/*because SOURCE_TV is 0,so need to add a flg to check ATV*/
	if (overscan_table[offset].load_flag == 1 &&
		overscan_table[offset].source == SOURCE_TV)
		atv_source_flg = 1;
	else
		atv_source_flg = 0;

	pr_amvecm_dbg("%s load_flag[0]/[%d] = %d/%d\n",
		__func__, offset,
		overscan_table[0].load_flag, overscan_table[offset].load_flag);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void hdr_tone_mapping_get(enum lut_type_e lut_type,
	unsigned int length, unsigned int *hdr_tm)
{
	int i;

	if (hdr_tm) {
		if (lut_type & HLG_LUT) {
			for (i = 0; i < length; i++)
				oo_y_lut_hlg_sdr[i] = hdr_tm[i];

			cuva_static_hlg_en = 0;
			vecm_latch_flag |= FLAG_HDR_OOTF_LATCH;

			if (debug_amvecm & 4) {
				for (i = 0; i < length; i++) {
					pr_info("oo_y_lut_hlg_sdr[%d] = %d",
						i, oo_y_lut_hlg_sdr[i]);
					if (i % 8 == 0)
						pr_info("\n");
				}
			}
		} else if (lut_type & HDR_LUT) {
			for (i = 0; i < length; i++)
				oo_y_lut_hdr_sdr_def[i] = hdr_tm[i];

			vecm_latch_flag |= FLAG_HDR_OOTF_LATCH;

			if (debug_amvecm & 4) {
				for (i = 0; i < length; i++) {
					pr_info("oo_y_lut_hdr_sdr[%d] = %d",
						i, oo_y_lut_hdr_sdr_def[i]);
					if (i % 8 == 0)
						pr_info("\n");
				}
				pr_info("\n");
			}
		}
	}
}

static int parse_aipq_ofst_table(int *table_ptr,
				 unsigned int height, unsigned int width)
{
	unsigned int i;
	unsigned int size = 0;

	size = width * sizeof(int);
	for (i = 0; i < height; i++)
		memcpy(vpp_pq_data[i], table_ptr + (i * width), size);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	update_aipq_data();
#endif
	return 0;
}
#endif

static long amvecm_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	unsigned int mem_size;
	struct ve_pq_load_s vpp_pq_load;
	struct ve_pq_table_s *vpp_pq_load_table = NULL;
	int tmp = 0;
	struct vpp_pq_ctrl_s pq_ctrl;
	enum meson_cpu_ver_e cpu_ver;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	enum color_primary_e color_pri;
	struct hdr_tone_mapping_s hdr_tone_mapping;
	unsigned int *hdr_tm = NULL;
	struct aipq_load_s aipq_load_table;
	char *aipq_char_data_ptr = NULL;
	int *aipq_int_data_ptr = NULL;
	int size, size1;
	int aipq_data_length;
	struct cm_color_md cm_color_mode;
	struct table_3dlut_s *p3dlut;
	int lut_order, lut_index, sdr_hdr_ctrl;
	struct vpp_mtx_info_s *mtx_p = &mtx_info;
	struct pre_gamma_table_s *pre_gma_tb = NULL;
	struct eye_protect_s *eye_prot = NULL;
	struct hdr_tmo_sw *pre_tmo_reg = NULL;
	struct hdr_tmo_sw_ext *pre_tmo_reg_ext = NULL;
	struct db_cabc_param_s *db_cabc_param = NULL;
	struct db_aad_param_s *db_aad_param = NULL;
	struct primary_s *color_pr = NULL;
	struct ve_ble_whe_param_s *ble_whe = NULL;
	struct color_param_s *ct_parm1 = NULL;
	struct color_tune_parm_s *ct_param = NULL;
	struct db_aicolor_param_s *db_aicolor_param = NULL;
	int cm_color = 0;
	int lut3d_single_sz;
	struct video_color_matrix gamut_mtx;
	struct hdr_parameter_reg_s hdr_customer_reg_data;
	struct hdr_mtrx_data_s hdr_mtrx_data;
	struct hdr_gamut_data_s hdr_gamut_data;
	struct panel_primary_s *panel_pri = NULL;
	enum eotf_type_e eo_type;
#endif

	if (debug_amvecm & 2)
		pr_info("[amvecm..] %s: cmd_nr = 0x%x\n",
			__func__, _IOC_NR(cmd));

	if (probe_ok == 0)
		return ret;

	if (pq_load_en == 0) {
		if (debug_amvecm & 4)
			pr_amvecm_dbg("[amvecm..] pq ioctl function disabled !!\n");
		return ret;
	}

	switch (cmd) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	case AMVECM_IOC_LOAD_REG:
		if ((vecm_latch_flag & FLAG_REG_MAP0) &&
			(vecm_latch_flag & FLAG_REG_MAP1) &&
			(vecm_latch_flag & FLAG_REG_MAP2) &&
			(vecm_latch_flag & FLAG_REG_MAP3) &&
			(vecm_latch_flag & FLAG_REG_MAP4) &&
			(vecm_latch_flag & FLAG_REG_MAP5)) {
			ret = -EBUSY;
			pr_amvecm_dbg("load regs error: loading regs, please wait\n");
			break;
		}
		if (copy_from_user(&amregs_ext,
			(void __user *)arg,
			sizeof(struct am_regs_s))) {
			pr_amvecm_dbg("0x%lx load reg errors: can't get buffer length\n",
				FLAG_REG_MAP0);
			ret = -EFAULT;
		} else {
			ret = cm_load_reg(&amregs_ext);
		}
		break;
#endif
	case AMVECM_IOC_S_CM_CTRL:
		if (copy_from_user(&tmp,
			(void __user *)arg,
			sizeof(int)))
			ret = -EFAULT;
		else
			cm_en = tmp;
		pr_amvecm_dbg("AMVECM_IOC_S_CM_CTRL: %d\n", cm_en);
		break;
	case AMVECM_IOC_S_FORCE_OUT:
		if (copy_from_user(&tmp,
			(void __user *)arg,
			sizeof(int)))
			ret = -EFAULT;
		else
			force_output = tmp;
		pr_amvecm_dbg("AMVECM_IOC_S_FORCE_OUT: %d\n", force_output);
		break;
	case AMVECM_IOC_G_FORCE_OUT:
		tmp = force_output;
		argp = (void __user *)arg;
		if (copy_to_user(argp, &tmp, sizeof(int)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_HDR_POLICY:
		if (copy_from_user(&tmp,
			(void __user *)arg,
			sizeof(int)))
			ret = -EFAULT;
		else
			hdr_policy = tmp;
		pr_amvecm_dbg("AMVECM_IOC_S_HDR_POLICY: %d\n", hdr_policy);
		break;
	case AMVECM_IOC_S_RGB_OGO:
		pr_amvecm_dbg("AMVECM_IOC_S_RGB_OGO, wb_en=%d\n", wb_en);
		if (!wb_en)
			return -EINVAL;

		if (copy_from_user(&video_rgb_ogo,
				   (void __user *)arg,
				sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;
		else
			ve_ogo_param_update();
		break;
	case AMVECM_IOC_G_RGB_OGO:
		pr_amvecm_dbg("AMVECM_IOC_G_RGB_OGO, wb_en=%d\n", wb_en);
		if (!wb_en)
			return -EINVAL;

		if (copy_to_user((void __user *)arg,
				 &video_rgb_ogo, sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;

		break;
	case AMVECM_IOC_SET_OVERSCAN:
		if (copy_from_user(&vpp_pq_load,
				   (void __user *)arg,
				sizeof(struct ve_pq_load_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] pq ioctl copy fail!!\n");
			break;
		}
		if (!(vpp_pq_load.param_id & TABLE_NAME_OVERSCAN)) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] overscan ioctl param_id fail!!\n");
			break;
		}
		/*check pq_table length copy_from_user*/
		if (vpp_pq_load.length > TIMING_MAX ||
		    vpp_pq_load.length <= 0)  {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] pq ioctl length check fail!!\n");
			break;
		}

		mem_size = vpp_pq_load.length * sizeof(struct ve_pq_table_s);
		vpp_pq_load_table = kmalloc(mem_size, GFP_KERNEL);
		if (!vpp_pq_load_table) {
			pr_info("vpp_pq_load_table kmalloc fail!!!\n");
			return -EFAULT;
		}
		argp = (void __user *)vpp_pq_load.param_ptr;
		if (copy_from_user(vpp_pq_load_table, argp, mem_size)) {
			pr_amvecm_dbg("[amvecm..] overscan copy fail!!\n");
			break;
		}
		parse_overscan_table(vpp_pq_load.length, vpp_pq_load_table);
		break;
	case AMVECM_IOC_G_PIC_MODE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &vdj_mode_s, sizeof(struct am_vdj_mode_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_PIC_MODE:
		if (copy_from_user(&vdj_mode_s,
				   (void __user *)arg,
				   sizeof(struct am_vdj_mode_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] vdj_mode_s ioctl copy fail!!\n");
			break;
		}
		vdj_mode_flg = vdj_mode_s.flag;
		/*vadj switch control according to vadj1_en/vadj2_en*/
		if (vdj_mode_flg & VDJ_FLAG_VADJ_EN) {
			pr_amvecm_dbg("IOC--vadj1_en=%d,vadj2_en=%d.\n",
				      vdj_mode_s.vadj1_en, vdj_mode_s.vadj2_en);
			vecm_latch_flag |= FLAG_VADJ_EN;
		}

		if (vdj_mode_flg & VDJ_FLAG_BRIGHTNESS) { /*brightness*/
			vd1_brightness = vdj_mode_s.brightness;
			vecm_latch_flag |= FLAG_VADJ1_BRI;
			pr_amvecm_dbg("vdj_mode_s.brightness:%d\n",
				vdj_mode_s.brightness);
		}
		if (vdj_mode_flg & VDJ_FLAG_BRIGHTNESS2) { /*brightness2*/
			if (vdj_mode_s.brightness2 < -1024 ||
			    vdj_mode_s.brightness2 > 1023) {
				pr_amvecm_dbg("load brightness2 value invalid!!!\n");
				return -EINVAL;
			}
			vd1_brightness2 = vdj_mode_s.brightness2;
			ret = amvecm_set_brightness2(vdj_mode_s.brightness2);
			pr_amvecm_dbg("vdj_mode_s.brightness:%d2\n",
				vdj_mode_s.brightness2);
		}
		if (vdj_mode_flg & VDJ_FLAG_SAT_HUE) { /*saturation_hue*/
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			/*ai pq get saturation*/
			aipq_base_satur_param(vdj_mode_s.saturation_hue);
			//ret =
			//amvecm_set_saturation_hue(vdj_mode_s.saturation_hue, WR_VCB);
			pq_user_latch_flag |= PQ_USER_CMS_SAT_HUE;
			pr_amvecm_dbg("vdj_mode_s.saturation_hue:%d\n",
				vdj_mode_s.saturation_hue);
#else
		ret = amvecm_set_saturation_hue(vdj_mode_s.saturation_hue, WR_VCB, 0);
#endif
		}
		if (vdj_mode_flg & VDJ_FLAG_SAT_HUE_POST) {
			/*saturation_hue_post*/
			int sat_post, hue_post, sat_hue_post;

			sat_hue_post = vdj_mode_s.saturation_hue_post;
			sat_post = (((sat_hue_post >> 16) & 0xffff) / 2) - 128;
			hue_post = sat_hue_post & 0xffff;
			if (hue_post >= 0 && hue_post <= 150)
				hue_post = hue_post / 6;
			else
				hue_post = (hue_post - 1024) / 6;
			ret =
			amvecm_set_saturation_hue_post(sat_post, hue_post);
			pr_amvecm_dbg("sat_post:%d, hue_post:%d\n",
				sat_post, hue_post);
			if (ret < 0)
				break;
		}
		if (vdj_mode_flg & 0x10) { /*contrast*/
			if (vdj_mode_s.contrast < -1024 ||
			    vdj_mode_s.contrast > 1023) {
				ret = -EINVAL;
				pr_amvecm_dbg("[amvecm..] ioctrl contrast value invalid!!\n");
				break;
			}
			vd1_contrast = vdj_mode_s.contrast;
			vecm_latch_flag |= FLAG_VADJ1_CON;
			pr_amvecm_dbg("vdj_mode_s.contrast:%d\n",
				vdj_mode_s.contrast);
		}
		if (vdj_mode_flg & 0x20) { /*constract2*/
			if (vdj_mode_s.contrast2 < -127 ||
			    vdj_mode_s.contrast2 > 127) {
				ret = -EINVAL;
				pr_amvecm_dbg("[amvecm..] ioctrl contrast2 value invalid!!\n");
				break;
			}
			vd1_contrast2 = vdj_mode_s.contrast2;
			ret = amvecm_set_contrast2(vdj_mode_s.contrast2);
			pr_amvecm_dbg("vdj_mode_s.contrast2:%d\n",
				vdj_mode_s.contrast2);
		}
		break;
	case AMVECM_IOC_S_PQ_CTRL:
		if (copy_from_user(&pq_ctrl,
				   (void __user *)arg,
				   sizeof(struct vpp_pq_ctrl_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("pq control cp vpp_pq_ctrl_s fail\n");
		} else {
			argp = (void __user *)pq_ctrl.ptr;
			pr_amvecm_dbg("argp = %p\n", argp);
			mem_size = sizeof(struct pq_ctrl_s);
			if (pq_ctrl.length > mem_size) {
				pq_ctrl.length = mem_size;
				pr_amvecm_dbg("system control length > kernel length\n");
			}
			if (copy_from_user(&pq_cfg,
					   argp,
					   mem_size)) {
				ret = -EFAULT;
				pr_amvecm_dbg("pq control cp pq_ctrl_s fail\n");
			} else {
				pq_user_latch_flag |= PQ_USER_PQ_MODULE_CTL;
				pr_amvecm_dbg("pq control load success\n");
			}
		}
		break;
	case AMVECM_IOC_G_PQ_CTRL:
		if (copy_from_user(
			&pq_ctrl,
			(void __user *)arg,
			sizeof(struct vpp_pq_ctrl_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("pq control cp vpp_pq_ctrl_s fail\n");
		} else {
			argp = (void __user *)pq_ctrl.ptr;
			pr_amvecm_dbg("argp = %p\n", argp);
			mem_size = sizeof(struct pq_ctrl_s);
			if (pq_ctrl.length > mem_size) {
				pq_ctrl.length = mem_size;
				pr_amvecm_dbg("system control length > kernel length\n");
			}
			if (copy_to_user(
				argp,
				&pq_cfg,
				mem_size)) {
				ret = -EFAULT;
				pr_amvecm_dbg("pq control cp to user fail\n");
			} else
				pr_amvecm_dbg("pq control cp to user success\n");
		}
		break;
	case AMVECM_IOC_S_MESON_CPU_VER:
		if (!is_meson_tm2_cpu())
			break;
		if (copy_from_user(&cpu_ver,
				   (void __user *)arg,
				   sizeof(enum meson_cpu_ver_e))) {
			ret = -EFAULT;
		} else {
			if ((is_meson_rev_a() && cpu_ver == VER_A) ||
				(is_meson_rev_b() && cpu_ver == VER_B))
				break;
			ret = -EINVAL;
			pr_amvecm_dbg("cpu version doesn't match\n");
		}
		break;
	case AMVECM_IOC_S_FREERUN_TYPE:
		if (copy_from_user(&freerun_en,
			(void __user *)arg,
			sizeof(enum freerun_type_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_CHIP_TYPE:
		argp = (void __user *)arg;
		tmp = get_cpu_type();
		if (copy_to_user(argp, &tmp, sizeof(int))) {
			ret = -EFAULT;
		} else {
			pr_amvecm_dbg("AMVECM_IOC_G_CHIP_TYPE 0x%x\n",
				tmp);
		}
		break;
	case AMVECM_IOC_G_CHIP_ClASS:
		argp = (void __user *)arg;
		if (copy_to_user(argp, &chip_cls_id, sizeof(int)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_PQ_LOAD_EN:
		if (copy_from_user(&tmp,
			(void __user *)arg, sizeof(unsigned int))) {
			ret = -EFAULT;
		} else {
			pq_load_en = tmp;
			pr_amvecm_dbg("AMVECM_IOC_S_PQ_LOAD_EN = %d\n",
				tmp);
		}
		break;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case AMVECM_IOC_VE_NEW_DNLP:
		if (copy_from_user(&dnlp_curve_param_load,
			(void __user *)arg,
			sizeof(struct ve_dnlp_curve_param_s))) {
			pr_amvecm_dbg("dnlp load fail\n");
			ret = -EFAULT;
		} else {
			ve_new_dnlp_param_update();
			aipq_base_dnlp_param(dnlp_curve_param_load.param[ve_dnlp_final_gain]);
			pr_amvecm_dbg("dnlp load success\n");
		}
		break;
	case AMVECM_IOC_G_HIST_AVG:
		argp = (void __user *)arg;
		if (video_ve_hist.height == 0 || video_ve_hist.width == 0)
			ret = -EFAULT;
		else if (copy_to_user(argp,
				      &video_ve_hist,
				sizeof(struct ve_hist_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_HIST_BIN:
		argp = (void __user *)arg;
		if (vpp_hist_param.vpp_pixel_sum == 0)
			ret = -EFAULT;
		else if (copy_to_user(argp, &vpp_hist_param,
				      sizeof(struct vpp_hist_param_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_HDR_METADATA:
		argp = (void __user *)arg;
		if (copy_to_user(argp, &vpp_hdr_metadata_s,
				 sizeof(struct hdr_metadata_info_s)))
			ret = -EFAULT;
		break;
	/**********************************************************************/
	/*gamma ioctl*/
	/**********************************************************************/
	case AMVECM_IOC_GAMMA_TABLE_EN:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		break;
	case AMVECM_IOC_GAMMA_TABLE_DIS:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		break;
	case AMVECM_IOC_GAMMA_TABLE_R:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
				   (void __user *)arg,
				sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
					     &video_gamma_table_r)) {
			memcpy(&video_gamma_table_r,
			       &video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		} else {
			pr_amvecm_dbg("load same gamma_r table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_G:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
				   (void __user *)arg,
				sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
					     &video_gamma_table_g)) {
			memcpy(&video_gamma_table_g,
			       &video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		} else {
			pr_amvecm_dbg("load same gamma_g table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_B:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
				   (void __user *)arg,
				sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
					     &video_gamma_table_b)) {
			memcpy(&video_gamma_table_b,
			       &video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		} else {
			pr_amvecm_dbg("load same gamma_b table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_EN_SUB:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag2 |= FLAG_GAMMA_TABLE_EN_SUB;
		break;
	case AMVECM_IOC_GAMMA_TABLE_DIS_SUB:
		if (!gamma_en)
			return -EINVAL;

		vecm_latch_flag2 |= FLAG_GAMMA_TABLE_DIS_SUB;
		break;
	case AMVECM_IOC_GAMMA_TABLE_R_SUB:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
			(void __user *)arg,
			sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
				&video_gamma_table_r_sub)) {
			memcpy(&video_gamma_table_r_sub,
				&video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag2 |= FLAG_GAMMA_TABLE_R_SUB;
		} else {
			pr_amvecm_dbg("load same gamma_r_sub table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_G_SUB:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
			(void __user *)arg,
			sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
			&video_gamma_table_g_sub)) {
			memcpy(&video_gamma_table_g_sub,
				&video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag2 |= FLAG_GAMMA_TABLE_G_SUB;
		} else {
			pr_amvecm_dbg("load same gamma_g_sub table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_TABLE_B_SUB:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&video_gamma_table_ioctl_set,
			(void __user *)arg,
			sizeof(struct tcon_gamma_table_s))) {
			ret = -EFAULT;
		} else if (gamma_table_compare(&video_gamma_table_ioctl_set,
			&video_gamma_table_b_sub)) {
			memcpy(&video_gamma_table_b_sub,
				&video_gamma_table_ioctl_set,
				sizeof(struct tcon_gamma_table_s));
			vecm_latch_flag2 |= FLAG_GAMMA_TABLE_B_SUB;
		} else {
			pr_amvecm_dbg("load same gamma_b_sub table,no need to change\n");
		}
		break;
	case AMVECM_IOC_GAMMA_SET:
		if (!gamma_en)
			return -EINVAL;

		if (copy_from_user(&gt, (void __user *)arg,
			sizeof(struct gm_tbl_s))) {
			pr_amvecm_dbg("load gamma table fail\n");
			ret = -EFAULT;
		} else {
			pr_amvecm_dbg(" gm_par_idx = %d, load gm success\n", gm_par_idx);
		}
		break;
	case AMVECM_IOC_S_RGB_OGO_SUB:
		pr_amvecm_dbg("AMVECM_IOC_S_RGB_OGO_SUB, wb_en=%d\n", wb_en);
		if (!wb_en)
			return -EINVAL;

		if (copy_from_user(&video_rgb_ogo_sub,
			(void __user *)arg,
			sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;
		else
			ve_ogo_param_update_sub();

		break;
	case AMVECM_IOC_G_RGB_OGO_SUB:
		pr_amvecm_dbg("AMVECM_IOC_G_RGB_OGO_SUB, wb_en=%d\n", wb_en);
		if (!wb_en)
			return -EINVAL;

		if (copy_to_user((void __user *)arg,
			&video_rgb_ogo_sub, sizeof(struct tcon_rgb_ogo_s)))
			ret = -EFAULT;

		break;
	case AMVECM_IOC_SET_3D_LUT:
		if (chip_type_id == chip_t6d)
			lut3d_single_sz = LUT3D_SINGLE_SIZE9;
		else
			lut3d_single_sz = LUT3D_SINGLE_SIZE;

		p3dlut = vmalloc(LUT3D_SINGLE_SIZE * 3 * sizeof(unsigned int));
		if (!p3dlut)
			return -ENOMEM;

		memset(p3dlut, 0, LUT3D_SINGLE_SIZE * 3 * sizeof(unsigned int));

		if (copy_from_user(p3dlut,
			(void __user *)arg,
			lut3d_single_sz * 3 * sizeof(unsigned int))) {
			ret = -EFAULT;
		} else {
			/*vpp_lut3d_table_init(0, 0, 0);*/
			vpp_set_lut3d(0, 0, p3dlut->data, 0, 1);
			pr_amvecm_dbg("AMVECM_IOC_SET_3D_LUT update data.\n");
			/*vpp_lut3d_table_release();*/
			if (ct_en) {
				update_lut3d_base_data(p3dlut->data);
				pr_amvecm_dbg("AMVECM_IOC_SET_3D_LUT update base data.\n");
			}
		}

		vfree(p3dlut);
		break;
	case AMVECM_IOC_LOAD_3D_LUT:
		if (copy_from_user(&lut_index,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
			break;
		}

		vpp_lut3d_table_init(-1, -1, -1);
		vpp_set_lut3d(lut3d_data_source, lut_index, 0, 0, 0);
		/*vpp_lut3d_table_release();*/
		break;
	case AMVECM_IOC_SET_3D_LUT_ORDER:
		if (copy_from_user(&lut_order,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
		} else {
			lut3d_order = lut_order;
		}
		break;
	case AMVECM_IOC_3D_LUT_EN:
		if (copy_from_user(&tmp,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
		} else {
			lut3d_en = tmp;
			lut3d_en &= 0x1;
			vpp_enable_lut3d(lut3d_en);
		}
		break;
	case AMVECM_IOC_COLOR_PRI_EN:
		if (copy_from_user(&tmp,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
		} else {
			pr_amvecm_dbg("AMVECM_IOC_COLOR_PRI_EN val = %d\n", tmp);
			force_primary = tmp;
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_COLOR_PRIMARY:
		color_pr = kmalloc(sizeof(*color_pr), GFP_KERNEL);
		if (!color_pr) {
			ret = -EFAULT;
			pr_amvecm_dbg("color_pr kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(color_pr,
			(void __user *)arg,
			sizeof(struct primary_s))) {
			ret = -EFAULT;
		} else {
			memcpy(force_src_primary, color_pr->src,
				8 * sizeof(u32));
			memcpy(force_dst_primary, color_pr->dest,
				8 * sizeof(u32));
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	/*VLOCK*/
	case AMVECM_IOC_VLOCK_EN:
		vecm_latch_flag |= FLAG_VLOCK_EN;
		break;
	case AMVECM_IOC_VLOCK_DIS:
		vecm_latch_flag |= FLAG_VLOCK_DIS;
		break;
	/*3D-SYNC*/
	case AMVECM_IOC_3D_SYNC_EN:
		vecm_latch_flag |= FLAG_3D_SYNC_EN;
		break;
	case AMVECM_IOC_3D_SYNC_DIS:
		vecm_latch_flag |= FLAG_3D_SYNC_DIS;
		break;
	case AMVECM_IOC_S_HDR_TM:
		if (copy_from_user(&hdr_tone_mapping,
				   (void __user *)arg,
				   sizeof(struct hdr_tone_mapping_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("hdr ioc fail!!!\n");
			break;
		}

		if (hdr_tone_mapping.lutlength > HDR2_OOTF_LUT_SIZE) {
			pr_amvecm_dbg("hdr tm over size !!!\n");
			ret = -EFAULT;
			break;
		}
		mem_size = hdr_tone_mapping.lutlength * sizeof(unsigned int);
		hdr_tm = kmalloc(mem_size, GFP_KERNEL);
		argp = (void __user *)hdr_tone_mapping.tm_lut;
		if (!hdr_tm) {
			ret = -EFAULT;
			pr_amvecm_dbg("hdr tm kmalloc fail!!!\n");
			break;
		}
		if (copy_from_user(hdr_tm, argp, mem_size)) {
			pr_amvecm_dbg("[amvecm..] hdr_tm copy fail!!\n");
			ret = -EFAULT;
			break;
		}
		hdr_tone_mapping_get(hdr_tone_mapping.lut_type,
					 hdr_tone_mapping.lutlength,
					 hdr_tm);
		break;
	case AMVECM_IOC_G_DNLP_STATE:
		if (copy_to_user((void __user *)arg,
			&dnlp_en, sizeof(enum dnlp_state_e)))
			ret = -EFAULT;
		pr_amvecm_dbg("[amvecm..] AMVECM_IOC_G_DNLP_STATE, dnlp_en=%d\n",
			dnlp_en);
		break;
	case AMVECM_IOC_S_DNLP_STATE:
		if (copy_from_user(&dnlp_en,
				   (void __user *)arg,
				   sizeof(enum dnlp_state_e)))
			ret = -EFAULT;
		pr_amvecm_dbg("[amvecm..] AMVECM_IOC_S_DNLP_STATE, dnlp_en=%d\n",
			dnlp_en);
		break;
	case AMVECM_IOC_G_PQMODE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &pc_mode, sizeof(enum pc_mode_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_PQMODE:
		if (copy_from_user(&pc_mode,
				   (void __user *)arg,
				   sizeof(enum pc_mode_e)))
			ret = -EFAULT;
		else
			pc_mode_last = 0xff;
		break;
	case AMVECM_IOC_G_CSCTYPE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &cur_csc_type[VD1_PATH],
				sizeof(enum vpp_matrix_csc_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_COLOR_PRI:
		argp = (void __user *)arg;
		color_pri = get_color_primary();
		if (copy_to_user(argp,
				 &color_pri,
				 sizeof(enum color_primary_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_CSCTYPE:
		if (copy_from_user(&cur_csc_type[VD1_PATH],
				   (void __user *)arg,
				   sizeof(enum vpp_matrix_csc_e))) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] cur_csc_type ioctl copy fail!!\n");
		}
		break;
	case AMVECM_IOC_S_LC_CURVE:
		if (copy_from_user(&lc_curve_parm_load,
				   (void __user *)arg,
			sizeof(struct ve_lc_curve_parm_s))) {
			pr_amvecm_dbg("lc load curve parm fail\n");
			ret = -EFAULT;
		} else {
			ve_lc_curve_update();
			pr_amvecm_dbg("lc load curve parm success\n");
		}
		break;
	case AMVECM_IOC_G_HDR_TYPE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &hdr_source_type, sizeof(enum hdr_type_e)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_AIPQ_TABLE:
		if (copy_from_user(&aipq_load_table,
				   (void __user *)arg,
				   sizeof(struct aipq_load_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq load ioc fail\n");
			break;
		}
		if (aipq_load_table.height > AIPQ_SCENE_MAX)
			aipq_load_table.height = AIPQ_SCENE_MAX;
		if (aipq_load_table.width > AIPQ_FUNC_MAX)
			aipq_load_table.width = AIPQ_FUNC_MAX;
		aipq_data_length = AIPQ_SINGL_DATA_LEN *
						aipq_load_table.height *
						aipq_load_table.width;

		size = (aipq_data_length + 1) * sizeof(char);
		aipq_char_data_ptr = kmalloc(size, GFP_KERNEL);
		if (!aipq_char_data_ptr) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq_char_data_ptr kmalloc fail!!!\n");
			break;
		}

		size1 = aipq_load_table.height *
			aipq_load_table.width *
			sizeof(int);
		aipq_int_data_ptr = kmalloc(size1, GFP_KERNEL);
		if (!aipq_int_data_ptr) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq_int_data_ptr kmalloc fail!!!\n");
			break;
		}

		argp = (void __user *)aipq_load_table.table_ptr;
		if (copy_from_user(aipq_char_data_ptr, argp, size)) {
			ret = -EFAULT;
			pr_amvecm_dbg("aipq table copy from user fail\n");
			break;
		}

		aipq_char_data_ptr[aipq_data_length] = '\0';
		if (strlen(aipq_char_data_ptr) != aipq_data_length) {
			pr_amvecm_dbg("aipq data length not eq 3*height*width!!!\n");
			break;
		}

		str_sapr_to_d(aipq_char_data_ptr, aipq_int_data_ptr, 4);

		parse_aipq_ofst_table(aipq_int_data_ptr,
				      aipq_load_table.height,
				      aipq_load_table.width);
		break;
	case AMVECM_IOC_S_CMS_LUMA:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_luma_array[cm_color][0] = cm_color_mode.color_value;
				cm2_luma_array[cm_color][1] = 0;
				cm2_luma_array[cm_color][2] = 1;
				cm2_luma(cm_color_mode,
					cm2_luma_array[cm_color][0],
					cm2_luma_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_LUMA;
			}
		}
		break;
	case AMVECM_IOC_S_CMS_SAT:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_sat_array[cm_color][0] = cm_color_mode.color_value;
				cm2_sat_array[cm_color][1] = 0;
				cm2_sat_array[cm_color][2] = 1;
				cm2_sat(cm_color_mode,
					cm2_sat_array[cm_color][0],
					cm2_sat_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_SAT;
			}
		}
		break;
	case AMVECM_IOC_S_CMS_HUE:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_hue_array[cm_color][0] = cm_color_mode.color_value;
				cm2_hue_array[cm_color][1] = 0;
				cm2_hue_array[cm_color][2] = 1;
				cm2_hue(cm_color_mode,
					cm2_hue_array[cm_color][0],
					cm2_hue_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE;
			}
		}
		break;
	case AMVECM_IOC_S_CMS_HUE_HS:
		if (copy_from_user(&cm_color_mode, (void __user *)arg,
				   sizeof(struct cm_color_md))) {
			ret = -EFAULT;
		} else {
			if (cm_color_mode.color_type == cm_9_color)
				cm_color = cm_color_mode.cm_9_color_md;
			else
				cm_color = cm_color_mode.cm_14_color_md;

			cm_cur_work_color_md = cm_color_mode.color_type;
			if (cm_color >= 0 && cm_color <= (cm_14_ecm2colormode_max - 1)) {
				cm2_hue_by_hs_array[cm_color][0] =
				cm_color_mode.color_value;
				cm2_hue_by_hs_array[cm_color][1] = 0;
				cm2_hue_by_hs_array[cm_color][2] = 1;
				cm2_hue_by_hs(cm_color_mode,
				    cm2_hue_by_hs_array[cm_color][0],
				    cm2_hue_by_hs_array[cm_color][1]);
				pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE_HS;
			}
		}
		break;
	case AMVECM_IOC_S_MTX_COEF:
		if (copy_from_user(mtx_p, (void __user *)arg,
				   sizeof(struct vpp_mtx_info_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("mtx info cp from usr failed\n");
		} else {
			vecm_latch_flag2 |= VPP_MARTIX_UPDATE;
		}
		break;
	case AMVECM_IOC_G_MTX_COEF:
		argp = (void __user *)arg;
		vecm_latch_flag2 |= VPP_MARTIX_GET;
		vpp_mtx_update(mtx_p, 0);
		if (copy_to_user(argp, mtx_p,
				 sizeof(struct vpp_mtx_info_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("mtx coef copy to user fail\n");
		} else {
			pr_amvecm_dbg("mtx coef copy to user success\n");
		}
		break;
	case AMVECM_IOC_S_PRE_GAMMA:
		mem_size = sizeof(struct pre_gamma_table_s);
		pre_gma_tb = kmalloc(mem_size, GFP_KERNEL);
		if (!pre_gma_tb) {
			pr_amvecm_dbg("pre_gma_tb malloc fail\n");
			ret = -ENOMEM;
			break;
		}
		if (copy_from_user(pre_gma_tb, (void __user *)arg,
				   sizeof(struct pre_gamma_table_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre gam struct cp from usr failed\n");
		} else {
			pr_amvecm_dbg("pre gam struct cp from usr success\n");
			pre_gamma.en = pre_gma_tb->en;
			memcpy(pre_gamma.lut_r,
			       pre_gma_tb->lut_r, 65 * sizeof(unsigned int));
			memcpy(pre_gamma.lut_g,
			       pre_gma_tb->lut_g, 65 * sizeof(unsigned int));
			memcpy(pre_gamma.lut_b,
			       pre_gma_tb->lut_b, 65 * sizeof(unsigned int));
			vecm_latch_flag2 |= VPP_PRE_GAMMA_UPDATE;
		}
		break;
	case AMVECM_IOC_G_PRE_GAMMA:
		argp = (void __user *)arg;
		mem_size = sizeof(struct pre_gamma_table_s);
		pre_gma_tb = kmalloc(mem_size, GFP_KERNEL);
		if (!pre_gma_tb) {
			pr_amvecm_dbg("pre_gma_tb malloc fail\n");
			ret = -EFAULT;
			break;
		}
		pre_gma_tb->en = pre_gamma.en;
		memcpy(pre_gma_tb->lut_r,
		       pre_gamma.lut_r, 65 * sizeof(unsigned int));
		memcpy(pre_gma_tb->lut_g,
		       pre_gamma.lut_g, 65 * sizeof(unsigned int));
		memcpy(pre_gma_tb->lut_b,
		       pre_gamma.lut_b, 65 * sizeof(unsigned int));
		if (copy_to_user(argp, pre_gma_tb, mem_size)) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre gam struct cp to usr failed\n");
		} else {
			pr_amvecm_dbg("pre gam struct cp to usr success\n");
		}
		break;
	case AMVECM_IOC_S_HDR_TMO:
		pre_tmo_reg = kmalloc(sizeof(*pre_tmo_reg), GFP_KERNEL);
		if (!pre_tmo_reg) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre_tmo_reg kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(pre_tmo_reg,
			(void __user *)arg,
			sizeof(struct hdr_tmo_sw))) {
			ret = -EFAULT;
			pr_info("tmo_reg info cp from usr failed\n");
		} else {
			hdr10_tmo_reg_set(pre_tmo_reg);
			if (!dpss_mode)
				force_toggle();
		}
		break;
	case AMVECM_IOC_G_HDR_TMO:
		pre_tmo_reg = kmalloc(sizeof(*pre_tmo_reg), GFP_KERNEL);
		if (!pre_tmo_reg) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre_tmo_reg kmalloc fail!!!\n");
			break;
		}

		hdr10_tmo_reg_get(pre_tmo_reg);
		argp = (void __user *)arg;
		if (copy_to_user(argp, pre_tmo_reg,
			sizeof(struct hdr_tmo_sw))) {
			ret = -EFAULT;
			pr_info("tmo_reg copy to user fail\n");
		} else {
			pr_info("tmo_reg copy to user success\n");
		}
		break;
	case AMVECM_IOC_S_HDR_TMO_EXT:
		pre_tmo_reg_ext = kmalloc(sizeof(*pre_tmo_reg_ext), GFP_KERNEL);
		if (!pre_tmo_reg_ext) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre_tmo_reg_ext kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(pre_tmo_reg_ext,
			(void __user *)arg,
			sizeof(struct hdr_tmo_sw_ext))) {
			ret = -EFAULT;
			pr_info("tmo_reg ext info cp from usr failed\n");
		} else {
			hdr10_tmo_reg_ext_set(pre_tmo_reg_ext);
			pr_info("tmo_reg ext set success\n");
		}
		break;
	case AMVECM_IOC_G_HDR_TMO_EXT:
		pre_tmo_reg_ext = kmalloc(sizeof(*pre_tmo_reg_ext), GFP_KERNEL);
		if (!pre_tmo_reg_ext) {
			ret = -EFAULT;
			pr_amvecm_dbg("pre_tmo_reg_ext kmalloc fail!!!\n");
			break;
		}

		argp = (void __user *)arg;
		hdr10_tmo_reg_ext_get(pre_tmo_reg_ext);
		if (copy_to_user(argp, pre_tmo_reg_ext,
			sizeof(struct hdr_tmo_sw_ext))) {
			ret = -EFAULT;
			pr_info("tmo_reg ext copy to user fail\n");
		} else {
			pr_info("tmo_reg ext copy to user success\n");
		}
		break;
	case AMVECM_IOC_S_CABC_PARAM:
		db_cabc_param = kmalloc(sizeof(*db_cabc_param), GFP_KERNEL);
		if (!db_cabc_param) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_cabc_param kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(db_cabc_param,
			(void __user *)arg,
			sizeof(struct db_cabc_param_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_cabc_param copy from user fail\n");
		} else {
			/*
			 * the same structure code db_aad_param_set is normal
			 */
			/* coverity[tainted_data:SUPPRESS] */
			db_cabc_param_set(db_cabc_param);
			pr_amvecm_dbg("db_cabc_param set success\n");
		}
		break;
	case AMVECM_IOC_S_AAD_PARAM:
		db_aad_param = kmalloc(sizeof(*db_aad_param), GFP_KERNEL);
		if (!db_aad_param) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_aad_param kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(db_aad_param,
			(void __user *)arg,
			sizeof(struct db_aad_param_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_aad_param copy from user fail\n");
		} else {
			db_aad_param_set(db_aad_param);
			pr_amvecm_dbg("db_aad_param set success\n");
		}
		break;
	case AMVECM_IOC_S_COLOR_TUNE:
		ct_parm1 = kmalloc(sizeof(*ct_parm1), GFP_KERNEL);
		if (!ct_parm1) {
			ret = -EFAULT;
			pr_amvecm_dbg("ct_parm1 kmalloc fail!!!\n");
			break;
		}

		ct_param = kmalloc(sizeof(*ct_param), GFP_KERNEL);
		if (!ct_param) {
			ret = -EFAULT;
			pr_amvecm_dbg("ct_param kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(ct_param, (void __user *)arg,
				sizeof(struct color_tune_parm_s))) {
			pr_amvecm_dbg("AMVECM_IOC_S_COLOR_TUNE set color tune failed\n");
			ret = -EFAULT;
		} else {
			memcpy(ct_parm1, ct_param, sizeof(struct color_tune_parm_s));
			ct_parm_set(ct_parm1);
			bs_ct_latch();
			pr_amvecm_dbg("AMVECM_IOC_S_COLOR_TUNE set color tune success\n");
		}
		break;
	case AMVECM_IOC_S_EYE_PROT:
		mem_size = sizeof(struct eye_protect_s);
		eye_prot = kmalloc(mem_size, GFP_KERNEL);
		if (!eye_prot) {
			pr_amvecm_dbg("eye_protect malloc fail\n");
			ret = -ENOMEM;
			break;
		}
		if (copy_from_user(eye_prot, (void __user *)arg,
				   sizeof(struct eye_protect_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("eye_protect struct cp from usr failed\n");
		} else {
			pr_amvecm_dbg("eye_protect struct cp from usr success\n");
			eye_protect.en = eye_prot->en;
			memcpy(eye_protect.mtx_ep,
				eye_prot->mtx_ep, 16 * sizeof(int));
			vecm_latch_flag2 |= VPP_EYE_PROTECT_UPDATE;
		}
		break;
	case AMVECM_IOC_S_GAMUT_CONV_EN:
		if (copy_from_user(&gamut_conv_enable,
			(void __user *)arg,
			sizeof(enum gamut_conv_enable_e))) {
			ret = -EFAULT;
		} else {
			pr_amvecm_dbg("gamut conv enable cp from usr = %d\n",
				gamut_conv_enable);
			force_toggle();
		}
		break;
	case AMVECM_IOC_COLOR_MTX_EN:
		if (copy_from_user(&tmp,
			(void __user *)arg,
			sizeof(int))) {
			ret = -EFAULT;
			pr_amvecm_dbg("color matrix cp from usr failed\n");
		} else {
			pr_amvecm_dbg("color matrix cp from usr success\n");
			force_matrix = tmp;
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_COLOR_MATRIX_DATA:
		if (copy_from_user(&gamut_mtx,
			(void __user *)arg,
			sizeof(struct video_color_matrix))) {
			ret = -EFAULT;
			pr_amvecm_dbg("color matrix data cp from usr failed\n");
		} else {
			pr_amvecm_dbg("color matrix data cp from usr success\n");
			memcpy(force_matrix_primary, gamut_mtx.data,
				sizeof(force_matrix_primary));
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_G_COLOR_MATRIX_DATA:
		argp = (void __user *)arg;
		memcpy(gamut_mtx.data, force_matrix_primary,
			sizeof(force_matrix_primary));
		if (copy_to_user(argp, &gamut_mtx,
			sizeof(struct video_color_matrix))) {
			ret = -EFAULT;
			pr_amvecm_dbg("gamut matrix copy to user fail\n");
		} else {
			pr_amvecm_dbg("gamut matrix copy to user success\n");
		}
		break;
	case AMVECM_IOC_S_BLE_WHE:
		ble_whe = kmalloc(sizeof(*ble_whe), GFP_KERNEL);
		if (!ble_whe) {
			ret = -EFAULT;
			pr_amvecm_dbg("ble_whe kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(ble_whe,
			(void __user *)arg,
			sizeof(struct ve_ble_whe_param_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("ble whe copy from user fail\n");
		} else {
			memcpy(&ble_whe_param_load, ble_whe,
				sizeof(struct ve_ble_whe_param_s));
			vecm_latch_flag2 |= BLE_WHE_UPDATE;
			pr_amvecm_dbg("ble whe set success\n");
		}
		break;
	case AMVECM_IOC_AI_COLOR_EN:
		if (copy_from_user(&tmp,
				   (void __user *)arg,
				   sizeof(int))) {
			ret = -EFAULT;
			pr_amvecm_dbg("ai_color_enable copy from user fail\n");
		} else {
			ai_color_enable = tmp;
			pr_amvecm_dbg("ai_color_enable set success, ai_color_enable=%d\n",
				ai_color_enable);
		}
		break;
	case AMVECM_IOC_S_AI_COLOR_PARAM:
		db_aicolor_param = kmalloc(sizeof(*db_aicolor_param), GFP_KERNEL);
		if (!db_aicolor_param) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_aicolor_param kmalloc fail!!!\n");
			break;
		}

		if (copy_from_user(db_aicolor_param,
			(void __user *)arg,
			sizeof(struct db_aicolor_param_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("db_aicolor_param copy from user fail\n");
		} else {
			db_aicolor_param_set(db_aicolor_param);
			pr_amvecm_dbg("db_aicolor_param set success\n");
		}
		break;
	case AMVECM_IOC_S_SDR2HDR_CTRL:
		if (copy_from_user(&sdr_hdr_ctrl,
			(void __user *)arg, sizeof(int))) {
			ret = -EFAULT;
		} else {
			if (chip_cls_id == TV_CHIP) {
				if (sdr_hdr_ctrl == 1) {
					hdr_policy = 2;
					set_force_output(BT2020_PQ);
				} else {
					hdr_policy = 0;
					set_force_output(UNKNOWN_FMT);
				}

				set_sdr_ext_mode_for_dpss(sdr_hdr_ctrl);

				if (!dpss_mode)
					force_toggle();

				pr_amvecm_dbg("AMVECM_IOC_S_SDR2HDR_CTRL sdr_hdr_ctrl = %d\n",
					sdr_hdr_ctrl);
			}
		}
		break;
	case AMVECM_IOC_S_HDR_MODE:
		if (copy_from_user(&tmp, (void __user *)arg, sizeof(int)))
			ret = -EFAULT;
		else
			hdr_mode = tmp;
		pr_amvecm_dbg("AMVECM_IOC_S_HDR_MODE: %d\n", hdr_mode);
		break;
	case AMVECM_IOC_S_SDR_MODE:
		if (copy_from_user(&tmp, (void __user *)arg, sizeof(int)))
			ret = -EFAULT;
		else
			sdr_mode = tmp;
		pr_amvecm_dbg("AMVECM_IOC_S_SDR_MODE: %d\n", sdr_mode);
		break;
	case AMVECM_IOC_S_CUVA_SDR_MAX_LUM:
		if (copy_from_user(&tmp, (void __user *)arg, sizeof(int)))
			ret = -EFAULT;
		else
			set_max_output_lum(0, tmp);
		pr_amvecm_dbg("AMVECM_IOC_S_CUVA_SDR_MAX_LUM: %d\n", tmp);
		break;
	case AMVECM_IOC_S_CUVA_HDR_MAX_LUM:
		if (copy_from_user(&tmp, (void __user *)arg, sizeof(int)))
			ret = -EFAULT;
		else
			set_max_output_lum(1, tmp);
		pr_amvecm_dbg("AMVECM_IOC_S_CUVA_HDR_MAX_LUM: %d\n", tmp);
		break;
	case AMVECM_IOC_S_HDR_TOP_CTRL_MODE:
		if (copy_from_user(&tmp,
			(void __user *)arg, sizeof(int))) {
			ret = -EFAULT;
		} else {
			set_hdr_top_ctrl_mode(tmp);
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
			pr_amvecm_dbg("hdr_top_ctrl_mode cp from user = %d\n", tmp);
		}
		break;
	case AMVECM_IOC_S_HDR_PARAM_REG:
		if (copy_from_user(&hdr_customer_reg_data,
			(void __user *)arg,
			sizeof(struct hdr_parameter_reg_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("hdr_parameter_reg cp from usr failed\n");
		} else {
			set_hdr_parameter_reg(&hdr_customer_reg_data);
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_HDR_GAMUT_COEF:
		if (copy_from_user(&hdr_gamut_data,
			(void __user *)arg,
			sizeof(struct hdr_gamut_data_s))) {
			ret = -EFAULT;
		} else {
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			set_hdr_gamut_coef(&hdr_gamut_data);
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_HDR_MTRX_COEF:
		if (copy_from_user(&hdr_mtrx_data,
			(void __user *)arg,
			sizeof(struct hdr_mtrx_data_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("hdr_mtrx_data cp from usr failed\n");
		} else {
			set_hdr_mtrx_coef(&hdr_mtrx_data);
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_OSD_PIC_MODE:
		if (copy_from_user(&osd_vdj_mod_s,
				   (void __user *)arg,
				   sizeof(struct am_osd_vdj_mode_s))) {
			ret = -EFAULT;
			pr_amvecm_dbg("[amvecm..] AMVECM_IOC_S_OSD_PIC_MODE ioctl copy fail!!\n");
			break;
		}
		if (osd_vdj_mod_s.flag & VDJ_FLAG_VADJ_EN) {
			pr_amvecm_dbg("IOC--osd_pic_en=%d.\n", osd_vdj_mod_s.osd_pic_en);
			amvecm_set_osd_mtx_enable(osd_vdj_mod_s.osd_pic_en);
		}

		if (osd_vdj_mod_s.flag & VDJ_FLAG_BRIGHTNESS) {
			amvecm_set_osd_brightness(osd_vdj_mod_s.brightness);
			pr_amvecm_dbg("osd_vdj_mod_s.brightness:%d\n",
				osd_vdj_mod_s.brightness);
		}

		if (osd_vdj_mod_s.flag & VDJ_FLAG_CONTRAST) {
			amvecm_set_osd_contrast(osd_vdj_mod_s.contrast);
			pr_amvecm_dbg("osd_vdj_mod_s.contrast:%d\n",
				osd_vdj_mod_s.contrast);
		}

		if (osd_vdj_mod_s.flag & VDJ_FLAG_SAT_HUE) {
			amvecm_set_osd_hue_sat(osd_vdj_mod_s.hue, osd_vdj_mod_s.saturation);
			pr_amvecm_dbg("osd_vdj_mod_s.hue = %d, osd_vdj_mod_s.saturation:%d\n",
				osd_vdj_mod_s.hue, osd_vdj_mod_s.saturation);
		}
		break;
	case AMVECM_IOC_G_OSD_PIC_MODE:
		argp = (void __user *)arg;
		if (copy_to_user(argp,
				 &osd_vdj_mod_s, sizeof(struct am_osd_vdj_mode_s)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_G_SUPPORT_GMT_WRAPPER:
		argp = (void __user *)arg;
		if (copy_to_user(argp, &support_gmt_wrapper, sizeof(int)))
			ret = -EFAULT;
		break;
	case AMVECM_IOC_S_GMT_WRAPPER_EN:
		if (copy_from_user(&gamut_mapping1_en,
			(void __user *)arg,
			sizeof(enum gamut_conv_enable_e))) {
			ret = -EFAULT;
		} else {
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_GMT_WRAPPER_MODE:
		if (copy_from_user(&tmp,
			(void __user *)arg, sizeof(int))) {
			ret = -EFAULT;
		} else {
			gamut_mode = tmp;
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_GMT_WRAPPER_COEF:
		if (copy_from_user(&hdr_gamut_data,
			(void __user *)arg,
			sizeof(struct hdr_gamut_data_s))) {
			ret = -EFAULT;
		} else {
			memcpy(&force_gamut_mtx, &hdr_gamut_data,
				sizeof(struct hdr_gamut_data_s));
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_S_PANEL_PRIMARY:
		panel_pri = kmalloc(sizeof(*panel_pri), GFP_KERNEL);
		if (!panel_pri) {
			ret = -EFAULT;
			break;
		}

		if (copy_from_user(panel_pri,
			(void __user *)arg,
			sizeof(struct panel_primary_s))) {
			ret = -EFAULT;
		} else {
			memcpy(panel_primary, panel_pri->primary,
				8 * sizeof(u32));
		}
		break;
	case AMVECM_IOC_S_CM_MODE:
		if (copy_from_user(&tmp,
			(void __user *)arg, sizeof(int))) {
			ret = -EFAULT;
		} else {
			cm_mode = tmp;
			for (tmp = 0; tmp < cm_14_ecm2colormode_max; tmp++) {
				cm2_sat_array[tmp][2] = 1;
				cm2_luma_array[tmp][2] = 1;
				cm2_hue_by_hs_array[tmp][2] = 1;
				cm2_hue_array[tmp][2] = 1;
			}
			pq_user_latch_flag |= PQ_USER_CMS_CURVE_SAT;
			pq_user_latch_flag |= PQ_USER_CMS_CURVE_LUMA;
			pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE_HS;
			pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE;
		}
		break;
	case AMVECM_IOC_S_HDR_ON:
		if (copy_from_user(&tmp,
			(void __user *)arg,
			sizeof(int))) {
			ret = -EFAULT;
		} else {
			hdr_set_on = tmp;
			vecm_latch_flag2 |= FLAG_HDR_ON;
			if (!dpss_mode)
				force_toggle();
			pr_amvecm_dbg("AMVECM_IOC_S_HDR_ON: %d\n", hdr_set_on);
		}
		break;
	case AMVECM_IOC_G_HDR_ON:
		tmp = get_hdr_on();
		argp = (void __user *)arg;
		if (copy_to_user(argp, &tmp, sizeof(int)))
			ret = -EFAULT;
		else
			pr_amvecm_dbg("AMVECM_IOC_G_HDR_ON = %d\n", tmp);
		break;
	case AMVECM_IOC_S_HDR10P_ON:
		if (copy_from_user(&tmp,
			(void __user *)arg,
			sizeof(int))) {
			ret = -EFAULT;
		} else {
			hdr10p_set_on = tmp;
			vecm_latch_flag2 |= FLAG_HDR10P_ON;
			if (!dpss_mode)
				force_toggle();
			pr_amvecm_dbg("AMVECM_IOC_S_HDR10P_ON: %d\n", hdr10p_set_on);
		}
		break;
	case AMVECM_IOC_S_GMT_WRAPPER0_COEF:
		if (copy_from_user(&hdr_gamut_data,
			(void __user *)arg,
			sizeof(struct hdr_gamut_data_s))) {
			ret = -EFAULT;
		} else {
			memcpy(&force_gamut0_mtx, &hdr_gamut_data,
				sizeof(struct hdr_gamut_data_s));
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
		}
		break;
	case AMVECM_IOC_G_SIGNAL_TYPE:
		mem_size = get_cur_signal_type();
		argp = (void __user *)arg;
		if (copy_to_user(argp, &mem_size, sizeof(unsigned int))) {
			ret = -EFAULT;
			pr_amvecm_dbg("AMVECM_IOC_G_HDR_ON fail\n");
		}
		break;
	case AMVECM_IOC_G_SBTM_MODE:
		argp = (void __user *)arg;
		tmp = sbtm_mode_support_get();
		if (copy_to_user(argp, &tmp, sizeof(int)))
			ret = -EFAULT;
		pr_amvecm_dbg("AMVECM_IOC_G_SBTM_MODE %d\n", tmp);
		break;
	case AMVECM_IOC_S_SBTM_MODE:
		if (copy_from_user(&tmp, (void __user *)arg, sizeof(int))) {
			ret = -EFAULT;
		} else {
			sbtm_mode_set(tmp);
			pr_amvecm_dbg("AMVECM_IOC_S_SBTM_MODE %d\n", tmp);
		}
		break;
	case AMVECM_IOC_S_SBTM_EN:
			if (copy_from_user(&tmp, (void __user *)arg, sizeof(int))) {
				ret = -EFAULT;
			} else {
				user_sbtm_set(tmp);
				sbtm_mode_update();
				pr_amvecm_dbg("AMVECM_IOC_S_SBTM_EN %d\n", tmp);
			}
		break;
	case AMVECM_IOC_S_EOTF_TYPE:
		if (copy_from_user(&eo_type,
			(void __user *)arg,
			sizeof(enum eotf_type_e))) {
			ret = -EFAULT;
		} else {
			if (eo_curve_type != eo_type) {
				set_eotf_type(eo_type);
				vecm_latch_flag |= FLAG_HDR_OOTF_LATCH;
				eo_curve_type = eo_type;
			}
		}
		break;
#endif
	default:
		ret = -EINVAL;
		pr_amvecm_dbg("ioctl default case(0x%x).\n", cmd);
		break;
	}

	kfree(vpp_pq_load_table);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	kfree(hdr_tm);
	kfree(aipq_char_data_ptr);
	kfree(aipq_int_data_ptr);
	kfree(pre_gma_tb);
	kfree(eye_prot);
	kfree(pre_tmo_reg);
	kfree(pre_tmo_reg_ext);
	kfree(db_cabc_param);
	kfree(db_aad_param);
	kfree(ct_parm1);
	kfree(ct_param);
	kfree(color_pr);
	kfree(ble_whe);
	kfree(db_aicolor_param);
	kfree(panel_pri);
#endif
	return ret;
}

#ifdef CONFIG_COMPAT
static long amvecm_compat_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
	long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = amvecm_ioctl(file, cmd, arg);
	return ret;
}
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static ssize_t amvecm_dnlp_debug_show(const struct class *cla,
				      const struct class_attribute *attr, char *buf)
{
	if (dnlp_dbg_flag & DNLP_PARAM_RD_UPDATE) {
		dnlp_dbg_flag &= ~DNLP_PARAM_RD_UPDATE;
		return sprintf(buf, "%d\n", dnlp_rd_param);
	}

	if (dnlp_dbg_flag & DNLP_CV_RD_UPDATE) {
		dnlp_dbg_flag &= ~DNLP_CV_RD_UPDATE;
		return sprintf(buf, "%s\n", dnlp_rd_curve);
	}

	return 0;
}

static ssize_t amvecm_dnlp_debug_store(const struct class *cla,
				       const struct class_attribute *attr,
				       const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	int ret = 0;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		return -ENOMEM;
	}
	parse_param_amvecm(buf_orig, (char **)&parm);

	ret = dnlp_debug_store(parm);
	if (ret)
		pr_info("set parameters failed %d\n", ret);
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_cabc_aad_show(const struct class *cla,
				    const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len = cabc_aad_print(buf);
	return len;
}

static ssize_t amvecm_cabc_aad_store(const struct class *cla,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	ret = cabc_aad_debug(parm);

	if (ret < 0)
		pr_info("set parameters failed\n");

	kfree(buf_orig);
	return count;
}
#endif

static ssize_t amvecm_brightness_show(const struct class *cla,
				      const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_brightness);
}

static ssize_t amvecm_brightness_store(const struct class *cla,
				       const struct class_attribute *attr,
				       const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1024)
		return -EINVAL;

	vd1_brightness = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_BRI;
	return count;
}

static ssize_t amvecm_contrast_show(const struct class *cla,
				    const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", vd1_contrast);
}

static ssize_t amvecm_contrast_store(const struct class *cla,
				     const struct class_attribute *attr,
		const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val < -1024 || val > 1023)
		return -EINVAL;

	vd1_contrast = val;
	/*vecm_latch_flag |= FLAG_BRI_CON;*/
	vecm_latch_flag |= FLAG_VADJ1_CON;
	return count;
}

static ssize_t amvecm_saturation_hue_show(const struct class *cla,
					  const struct class_attribute *attr,
					  char *buf)
{
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		return sprintf(buf, "0x%x\n",
			READ_VPP_REG(VPP_VADJ1_MA_MB_2));
	else
		return sprintf(buf, "0x%x\n",
			READ_VPP_REG(VPP_VADJ1_MA_MB));
}

static ssize_t amvecm_saturation_hue_store(const struct class *cla,
					   const struct class_attribute *attr,
					   const char *buf, size_t count)
{
	size_t r;
	s32 mab = 0;

	r = sscanf(buf, "0x%x", &mab);
	if (r != 1 || (mab & 0xfc00fc00))
		return -EINVAL;
	amvecm_set_saturation_hue(mab, WR_VCB, 0);
	return count;
}

void vpp_vd_adj1_saturation_hue(signed int sat_val,
				signed int hue_val, struct vframe_s *vf, int vpp_index)
{
	int i, ma, mb, mab, mc, md;
	int hue_cos[] = {
			/*0~12*/
		256, 256, 256, 255, 255, 254, 253, 252, 251, 250, 248, 247, 245,
		/*13~25*/
		243, 241, 239, 237, 234, 231, 229, 226, 223, 220, 216, 213, 209
	};
	int hue_sin[] = {
		/*-25~-13*/
		-147, -142, -137, -132, -126, -121, -115, -109, -104,
		 -98,  -92,  -86,  -80,  -74,  -68,  -62,  -56,  -50,
		 -44,  -38,  -31,  -25, -19, -13,  -6,      /*-12~-1*/
		0,  /*0*/
		 /*1~12*/
		6,   13,   19,	25,   31,   38,   44,	50,   56,  62,
		68,  74,   80,   86,   92,	98,  104,  109,  115,  121,
		126,  132, 137, 142, 147 /*13~25*/
	};

	i = (hue_val > 0) ? hue_val : -hue_val;
	ma = (hue_cos[i] * (sat_val + 128)) >> 7;
	mb = (hue_sin[25 + hue_val] * (sat_val + 128)) >> 7;
	saturation_ma_shift = ma - 0x100;
	saturation_mb_shift = mb;

	ma += saturation_ma;
	mb += saturation_mb;
	if (ma > 511)
		ma = 511;
	if (ma < -512)
		ma = -512;
	if (mb > 511)
		mb = 511;
	if (mb < -512)
		mb = -512;
	mab =  ((ma & 0x3ff) << 16) | (mb & 0x3ff);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		ve_color_mab_set(mab, VE_VADJ1, WR_VCB, 0);
	else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MA_MB_2, mab, vpp_index);
	else
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MA_MB, mab, vpp_index);
	mc = (s16)((mab << 22) >> 22); /* mc = -mb */
	mc = 0 - mc;
	if (mc > 511)
		mc = 511;
	if (mc < -512)
		mc = -512;
	md = (s16)((mab << 6) >> 22);  /* md =	ma; */
	mab = ((mc & 0x3ff) << 16) | (md & 0x3ff);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_color_mcd_set(mab, VE_VADJ1, WR_VCB, 0);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MC_MD_2, mab, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_MISC,
			1, 0, 1, vpp_index);
	} else {
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VADJ1_MC_MD, mab, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL,
			1, 0, 1, vpp_index);
	}
};

static ssize_t amvecm_saturation_hue_pre_show(const struct class *cla,
					      const struct class_attribute *attr,
					      char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_pre, hue_pre);
}

static ssize_t amvecm_saturation_hue_pre_store(const struct class *cla,
					       const struct class_attribute *attr,
					       const char *buf, size_t count)
{
	int parsed[2];

	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;

	if (parsed[0] < -128 || parsed[0] > 128 ||
	    parsed[1] < -25 || parsed[1] > 25) {
		return -EINVAL;
	}
	saturation_pre = parsed[0];
	hue_pre = parsed[1];
	vecm_latch_flag |= FLAG_VADJ1_COLOR;

	return count;
}

static ssize_t amvecm_saturation_hue_post_show(const struct class *cla,
					       const struct class_attribute *attr,
					       char *buf)
{
	return snprintf(buf, 20, "%d %d\n", saturation_post, hue_post);
}

static ssize_t amvecm_saturation_hue_post_store(const struct class *cla,
						const struct class_attribute *attr,
						const char *buf, size_t count)
{
	int parsed[2], ret;

	if (likely(parse_para_pq(buf, 2, parsed) != 2))
		return -EINVAL;
	ret = amvecm_set_saturation_hue_post(parsed[0],
					     parsed[1]);
	if (ret < 0)
		return ret;
	return count;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
char cm_rd_curve[321];
int cm_rd_param;
unsigned int cm_dbg_flag;
#define CM_PARAM_RD_UPDATE 0x1
#define CM_CURVE_RD_UPDATE 0x2

static ssize_t amvecm_cm2_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	pr_info("Usage:");
	pr_info(" echo wm addr data0 data1 data2 data3 data4 ");
	pr_info("> /sys/class/amvecm/cm2\n");
	pr_info(" echo rm addr > /sys/class/amvecm/cm2\n");
	if (cm_dbg_flag & CM_PARAM_RD_UPDATE) {
		cm_dbg_flag &= ~CM_PARAM_RD_UPDATE;
		return sprintf(buf, "%d\n", cm_rd_param);

	} else if (cm_dbg_flag & CM_CURVE_RD_UPDATE) {
		cm_dbg_flag &= ~CM_CURVE_RD_UPDATE;
		return sprintf(buf, "%s\n", cm_rd_curve);
	}
	return 0;
}

static ssize_t amvecm_cm2_store(const struct class *cls,
				const struct class_attribute *attr,
				const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr;
	int data[5] = {0};
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	int i, j = 0;
	char *stemp = NULL;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct cm_port_s cm_port;

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		cm_port = get_cm_port();
		switch (cm_slice_idx) {
		case SLICE0:
			addr_port = cm_port.cm_addr_port[0];
			data_port = cm_port.cm_data_port[0];
			break;
		case SLICE1:
			addr_port = cm_port.cm_addr_port[1];
			data_port = cm_port.cm_data_port[1];
			break;
		case SLICE2:
			addr_port = cm_port.cm_addr_port[2];
			data_port = cm_port.cm_data_port[2];
			break;
		case SLICE3:
			addr_port = cm_port.cm_addr_port[3];
			data_port = cm_port.cm_data_port[3];
			break;
		default:
			break;
		}
	}
#endif

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (n == 0) {
		pr_info("strsep fail,parm[] uninitialized !\n");
		kfree(buf_orig);
		return -EINVAL;
	}
	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr % 8;
		if (kstrtol(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[0] = val;
		if (kstrtol(parm[3], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[1] = val;
		if (kstrtol(parm[4], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[2] = val;
		if (kstrtol(parm[5], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[3] = val;
		if (kstrtol(parm[6], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		data[4] = val;
		WRITE_VPP_REG(addr_port, addr);
		WRITE_VPP_REG(data_port, data[0]);
		WRITE_VPP_REG(addr_port, addr + 1);
		WRITE_VPP_REG(data_port, data[1]);
		WRITE_VPP_REG(addr_port, addr + 2);
		WRITE_VPP_REG(data_port, data[2]);
		WRITE_VPP_REG(addr_port, addr + 3);
		WRITE_VPP_REG(data_port, data[3]);
		WRITE_VPP_REG(addr_port, addr + 4);
		WRITE_VPP_REG(data_port, data[4]);
		pr_info("wm: [0x%x] <-- 0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			pr_info("read: invalid parameter\n");
			pr_info("please: cat /sys/class/amvecm/cm2\n");
			kfree(buf_orig);
			return count;
		}
		if (kstrtol(parm[1], 16, &val) < 0)
			return -EINVAL;
		addr = val;
		addr = addr - addr % 8;
		WRITE_VPP_REG(addr_port, addr);
		data[0] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 1);
		data[1] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 2);
		data[2] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 3);
		data[3] = READ_VPP_REG(data_port);
		WRITE_VPP_REG(addr_port, addr + 4);
		data[4] = READ_VPP_REG(data_port);
		pr_info("rm:[0x%x]-->[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
			addr, data[0], data[1],
				data[2], data[3], data[4]);
	} else if (!strncmp(parm[0], "sat_via_hs", 10)) {
		stemp = kzalloc(321, GFP_KERNEL);
		if (!stemp)
			goto free_buf;
		for (i = 0; i < 3; i++)
			for (j = 0; j < 32; j++)
				int_convert_str(def_sat_via_hs[i][j],
					i * 32 + j, stemp, 2, 16);
		stemp[i * 32 * 2] = '\0';
		memcpy(cm_rd_curve, stemp, i * 32 * 2 + 1);
		cm_dbg_flag |= CM_CURVE_RD_UPDATE;
	} else if (!strncmp(parm[0], "hue_via_s", 9)) {
		stemp = kzalloc(321, GFP_KERNEL);
		if (!stemp)
			goto free_buf;
		for (i = 0; i < 5; i++)
			for (j = 0; j < 32; j++)
				int_convert_str(def_hue_via_s[i][j],
					i * 32 + j, stemp, 2, 16);
		stemp[i * 32 * 2] = '\0';
		memcpy(cm_rd_curve, stemp, i * 32 * 2 + 1);
		cm_dbg_flag |= CM_CURVE_RD_UPDATE;
	} else if (!strncmp(parm[0], "hue_via_hue", 11)) {
		stemp = kzalloc(321, GFP_KERNEL);
		if (!stemp)
			goto free_buf;
		for (j = 0; j < 32; j++)
			int_convert_str(def_hue_via_hue[j],
				j, stemp, 2, 16);
		stemp[32 * 2] = '\0';
		memcpy(cm_rd_curve, stemp, 32 * 2 + 1);
		cm_dbg_flag |= CM_CURVE_RD_UPDATE;
	} else if (!strncmp(parm[0], "luma_via_hue", 12)) {
		stemp = kzalloc(321, GFP_KERNEL);
		if (!stemp)
			goto free_buf;
		for (j = 0; j < 32; j++)
			int_convert_str(def_luma_via_hue[j],
				j, stemp, 2, 16);
		stemp[32 * 2] = '\0';
		memcpy(cm_rd_curve, stemp, 32 * 2 + 1);
		cm_dbg_flag |= CM_CURVE_RD_UPDATE;
	} else if (!strncmp(parm[0], "cm_mode", 7)) {
		if (kstrtol(parm[1], 10, &val) < 0)
			goto free_buf;
		cm_mode = val;
		for (i = 0; i < cm_14_ecm2colormode_max; i++) {
			cm2_sat_array[i][2] = 1;
			cm2_luma_array[i][2] = 1;
			cm2_hue_by_hs_array[i][2] = 1;
			cm2_hue_array[i][2] = 1;
		}
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_SAT;
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_LUMA;
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE_HS;
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE;
	}  else if (!strncmp(parm[0], "get_cm_mode", 11)) {
		pr_info("cm_mode = %d\n", cm_mode);
		cm_rd_param = cm_mode;
		cm_dbg_flag |= CM_PARAM_RD_UPDATE;
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/bit");
	}
free_buf:
	kfree(buf_orig);
	kfree(stemp);
	return count;
}

/*cm2 v2 cmd used for s5 4slice*/
static ssize_t amvecm_cm2_idx_show(const struct class *cla,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", cm_slice_idx);
}

static ssize_t amvecm_cm2_idx_store(const struct class *cls,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%d\n", &val);
	if (r != 1 || val > 3)
		return -EINVAL;

	cm_slice_idx = val;
	return count;
}
#endif

static ssize_t amvecm_cm_reg_show(const struct class *cla,
				  const struct class_attribute *attr, char *buf)
{
	pr_info("Usage: echo addr value > /sys/class/amvecm/cm_reg");
	return 0;
}

static ssize_t amvecm_cm_reg_store(const struct class *cls,
				   const struct class_attribute *attr,
		 const char *buffer, size_t count)
{
	int data[5] = {0};
	unsigned int addr, value;
	long val = 0;
	int i, node, reg_node;
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */
	char *buf_orig, *parm[2] = {NULL};
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct cm_port_s cm_port;
#endif

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		cm_port = get_cm_port();
		switch (cm_slice_idx) {
		case SLICE0:
			addr_port = cm_port.cm_addr_port[0];
			data_port = cm_port.cm_data_port[0];
			break;
		case SLICE1:
			addr_port = cm_port.cm_addr_port[1];
			data_port = cm_port.cm_data_port[1];
			break;
		case SLICE2:
			addr_port = cm_port.cm_addr_port[2];
			data_port = cm_port.cm_data_port[2];
			break;
		case SLICE3:
			addr_port = cm_port.cm_addr_port[3];
			data_port = cm_port.cm_data_port[3];
			break;
		default:
			break;
		}
	}
#endif

	if (kstrtoul(parm[0], 16, &val) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}
	addr = val;
	if (kstrtoul(parm[1], 16, &val) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}
	value = val;

	node = (addr - 0x100) / 8;
	reg_node = (addr - 0x100) % 8;

	for (i = 0; i < 5; i++) {
		if (i == reg_node) {
			data[i] = value;
			continue;
		}
		addr = node * 8 + 0x100 + i;
		WRITE_VPP_REG(addr_port, addr);
		data[i] = READ_VPP_REG(data_port);
	}

	for (i = 0; i < 5; i++) {
		addr = node * 8 + 0x100 + i;
		WRITE_VPP_REG(addr_port, addr);
		WRITE_VPP_REG(data_port, data[i]);
	}

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_write_reg_show(const struct class *cla,
				     const struct class_attribute *attr, char *buf)
{
	pr_info("Usage: echo w addr value > /sys/class/amvecm/pq_reg_rw\n");
	pr_info("Usage: echo bw addr value start length > /...\n");
	pr_info("Usage: echo r addr > /sys/class/amvecm/pq_reg_rw\n");
	pr_info("addr and value must be hex\n");
	return 0;
}

static ssize_t amvecm_write_reg_store(const struct class *cls,
				      const struct class_attribute *attr,
		 const char *buffer, size_t count)
{
	unsigned int addr, value, bitstart, bitlength;
	long val = 0;
	char *buf_orig, *parm[5] = {NULL};

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "r", 1)) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		value = READ_VPP_REG(addr);
		pr_info("0x%x=0x%x\n", addr, value);
	} else if (!strncmp(parm[0], "w", 1)) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		value = val;
		WRITE_VPP_REG(addr, value);
	} else if (!strncmp(parm[0], "bw", 2)) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		value = val;
		if (kstrtoul(parm[3], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		bitstart = val;
		if (kstrtoul(parm[4], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		bitlength = val;
		WRITE_VPP_REG_BITS(addr, value, bitstart, bitlength);
	}

	kfree(buf_orig);
	return count;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static unsigned int cal_crc32(unsigned int crc, const unsigned char *buf, int buf_len)
{
	unsigned int crcu32 = crc;
	unsigned char b;
	unsigned int s_crc32[16] = {
		0, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
		0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
		0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
		0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c,
	};

	if (buf_len <= 0)
		return 0;
	if (!buf)
		return 0;

	crcu32 = ~crcu32;
	while (buf_len--) {
		b = *buf++;
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b & 0xf)];
		crcu32 = (crcu32 >> 4) ^ s_crc32[(crcu32 & 0xf) ^ (b >> 4)];
	}

	return ~crcu32;
}

static ssize_t amvecm_gamma_show(const struct class *cls,
				 const struct class_attribute *attr,
			char *buf)
{
	int i;
	int len = 0;

	if (vecm_latch_flag2 & GAMMA_READ_R) {
		for (i = 0; i < 256; i++)
			len += sprintf(buf + len, "%03x", gamma_data_r[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_R;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_G) {
		for (i = 0; i < 256; i++)
			len += sprintf(buf + len, "%03x", gamma_data_g[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_G;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_B) {
		for (i = 0; i < 256; i++)
			len += sprintf(buf + len, "%03x", gamma_data_b[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_B;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_PASS) {
		len += sprintf(buf + len, "gamma set crc pass\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_PASS;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_FAIL) {
		len += sprintf(buf + len, "gamma set crc fail\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_FAIL;
		return len;
	}

	pr_info("Usage:");
	pr_info("	echo sgr|sgg|sgb xxx...xx > /sys/class/amvecm/gamma\n");
	pr_info("	echo sgr_sub|sgg_sub|sgb_sub xxx...xx > /sys/class/amvecm/gamma\n");
	pr_info("Notes:\n");
	pr_info("	if the string xxx......xx is less than 256*3,");
	pr_info("	then the remaining will be set value 0\n");
	pr_info("	if the string xxx......xx is more than 256*3, ");
	pr_info("	then the remaining will be ignored\n");
	pr_info("Usage:");
	pr_info("	echo ggr|ggg|ggb xxx > /sys/class/amvecm/gamma\n");
	pr_info("	echo ggr_sub|ggg_sub|ggb_sub xxx > /sys/class/amvecm/gamma\n");
	pr_info("Notes:\n");
	pr_info("	read all as point......xxx is 'all'.\n");
	pr_info("	read all as strings......xxx is 'all_str'.\n");
	pr_info("	read one point......xxx is a value '0~255'.\n ");
	return 0;
}

static ssize_t amvecm_gamma_store(const struct class *cls,
				  const struct class_attribute *attr,
			const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4] = {NULL};
	unsigned short *gamma_r;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	char *stemp = NULL;
	unsigned int len;
	unsigned int crc_data;
	unsigned int rgb_mask;

	stemp = kmalloc(600, GFP_KERNEL);
	if (!stemp)
		return -ENOMEM;

	gamma_r = kmalloc(256 * sizeof(unsigned short), GFP_KERNEL);
	if (!gamma_r) {
		kfree(stemp);
		return -ENOMEM;
	}

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		goto free_buf;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (!gamma_r || !stemp || n == 0)
		goto free_buf;

	// parm[0] sgr/sgg/sgb/ggr/ggg/ggb
	if ((parm[0][0] == 's') && (parm[0][1] == 'g') &&
		((parm[0][2] == 'r') || (parm[0][2] == 'g') || (parm[0][2] == 'b'))) {
		// parm[1] gamma data (256 * 3 Bytes --- 10bit, need 3 ASCII Bytes)
		len = strlen(parm[1]);
		if (len != 768) {
			vecm_latch_flag2 |= GAMMA_CRC_FAIL;
			pr_info("data length is not 768 Bytes.\n");
			goto free_buf;
		}

		//gamma data should be hex character
		for (i = 0; i < len; i++) {
			if ((parm[1][i] - '0') < 10 || (parm[1][i] | 0x20) - 'a' < 6)
				continue;

			pr_info("error char\n");
			goto free_buf;
		}

		//parm[2] crc value
		if (parm[2]) {
			crc_data = cal_crc32(0, parm[1], len);
			if (kstrtoul(parm[2], 16, &val) < 0) {
				pr_info("cmd crc error\n");
				goto free_buf;
			}
			if (crc_data == val) {
				vecm_latch_flag2 |= GAMMA_CRC_PASS;
			} else {
				vecm_latch_flag2 |= GAMMA_CRC_FAIL;
				goto free_buf;
			}
		}
		memset(gamma_r, 0, 256 * sizeof(unsigned short));
		gamma_count = (strlen(parm[1]) + 2) / 3;
		if (gamma_count > 256)
			gamma_count = 256;

		for (i = 0; i < gamma_count; ++i) {
			gamma[0] = parm[1][3 * i + 0];
			gamma[1] = parm[1][3 * i + 1];
			gamma[2] = parm[1][3 * i + 2];
			gamma[3] = '\0';
			if (kstrtol(gamma, 16, &val) < 0)
				goto free_buf;
			gamma_r[i] = val;
		}

		switch (parm[0][2]) {
		case 'r':
			rgb_mask = H_SEL_R;
			break;

		case 'g':
			rgb_mask = H_SEL_G;
			break;

		case 'b':
			rgb_mask = H_SEL_B;
			break;
		default:
			break;
		}

		if (!data_path)
			amve_write_gamma_table(gamma_r, rgb_mask);
		else
			amve_write_gamma_table_sub(gamma_r, rgb_mask);

	} else if (!strcmp(parm[0], "ggr")) {
		if (!data_path)
			vpp_get_lcd_gamma_table(H_SEL_R);
		else
			vpp_get_lcd_gamma_table_sub();

		vpp_get_lcd_gamma_table(H_SEL_R);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < 256; i++)
					d_convert_str(gamma_data_r[i], i, stemp, 3, 16);
				pr_info("gamma_r str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_R;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
		}
	} else if (!strcmp(parm[0], "ggg")) {
		if (!data_path)
			vpp_get_lcd_gamma_table(H_SEL_G);
		else
			vpp_get_lcd_gamma_table_sub();

		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < 256; i++)
					d_convert_str(gamma_data_g[i], i, stemp, 3, 16);
				pr_info("gamma_g str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_G;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		}

	} else if (!strcmp(parm[0], "ggb")) {
		if (!data_path)
			vpp_get_lcd_gamma_table(H_SEL_B);
		else
			vpp_get_lcd_gamma_table_sub();

		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < 256; i++)
					d_convert_str(gamma_data_b[i], i, stemp, 3, 16);
				pr_info("gamma_b str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_B;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		}
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/gamma");
	}
	kfree(buf_orig);
	kfree(stemp);
	kfree(gamma_r);
	return count;
free_buf:
	kfree(buf_orig);
	kfree(stemp);
	kfree(gamma_r);
	return -EINVAL;
}

static ssize_t amvecm_gamma_v2_show(const struct class *cls,
				 const struct class_attribute *attr,
			char *buf)
{
	int i;
	int len = 0;
	struct gamma_data_s *p_gm;
	int max_idx;

	if (chip_type_id < chip_t5m)
		return 0;

	p_gm = get_gm_data();
	max_idx = p_gm->max_idx;

	if (vecm_latch_flag2 & GAMMA_READ_R) {
		for (i = 0; i < max_idx; i++)
			len += sprintf(buf + len, "%03x",
				p_gm->dbg_gm_tbl.gamma_r[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_R;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_G) {
		for (i = 0; i < max_idx; i++)
			len += sprintf(buf + len, "%03x",
				p_gm->dbg_gm_tbl.gamma_g[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_G;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_READ_B) {
		for (i = 0; i < max_idx; i++)
			len += sprintf(buf + len, "%03x",
				p_gm->dbg_gm_tbl.gamma_b[i]);
		len += sprintf(buf + len, "\n");
		vecm_latch_flag2 &= ~GAMMA_READ_B;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_PASS) {
		len += sprintf(buf + len, "gamma set crc pass\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_PASS;
		return len;
	}

	if (vecm_latch_flag2 & GAMMA_CRC_FAIL) {
		len += sprintf(buf + len, "gamma set crc fail\n");
		vecm_latch_flag2 &= ~GAMMA_CRC_FAIL;
		return len;
	}

	pr_info("Usage:");
	pr_info("	echo sgr|sgg|sgb xxx...xx > /sys/class/amvecm/gamma_v2\n");
	pr_info("Notes:\n");
	pr_info("	if the string xxx......xx is less than 257*3,");
	pr_info("	then the remaining will be set value 0\n");
	pr_info("	if the string xxx......xx is more than 257*3, ");
	pr_info("	then the remaining will be ignored\n");
	pr_info("Usage:");
	pr_info("	echo ggr|ggg|ggb xxx > /sys/class/amvecm/gamma_v2\n");
	pr_info("Notes:\n");
	pr_info("	read all as point......xxx is 'all'.\n");
	pr_info("	read all as strings......xxx is 'all_str'.\n");
	pr_info("	read one point......xxx is a value '0~256'.\n ");
	return 0;
}

static ssize_t amvecm_gamma_v2_store(const struct class *cls,
				  const struct class_attribute *attr,
			const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4] = {NULL};
	unsigned short *gm_cv;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	char *stemp = NULL;
	unsigned int len;
	unsigned int crc_data;
	struct gamma_data_s *p_gm;
	int max_idx;

	stemp = kmalloc(600, GFP_KERNEL);
	if (!stemp)
		return -ENOMEM;

	if (chip_type_id < chip_t5m) {
		kfree(stemp);
		return -EIO;
	}

	p_gm = get_gm_data();
	max_idx = p_gm->max_idx;

	gm_cv = kmalloc_array(max_idx, sizeof(unsigned short), GFP_KERNEL);
	if (!gm_cv) {
		kfree(stemp);
		return -ENOMEM;
	}

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		goto free_buf;
	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (!gm_cv || !stemp || n == 0)
		goto free_buf;

	// parm[0] sgr/sgg/sgb/ggr/ggg/ggb
	if ((parm[0][0] == 's') && (parm[0][1] == 'g') &&
		((parm[0][2] == 'r') || (parm[0][2] == 'g') || (parm[0][2] == 'b'))) {
		len = strlen(parm[1]);
		if (len != max_idx * 3) {
			vecm_latch_flag2 |= GAMMA_CRC_FAIL;
			pr_info("data length is not 771 Bytes.\n");
			goto free_buf;
		}

		//gamma data should be hex character
		for (i = 0; i < len; i++) {
			if ((parm[1][i] - '0') < 10 || (parm[1][i] | 0x20) - 'a' < 6)
				continue;
			pr_info("error char\n");
			goto free_buf;
		}

		//parm[2] crc value
		if (parm[2]) {
			crc_data = cal_crc32(0, parm[1], len);
			if (kstrtoul(parm[2], 16, &val) < 0) {
				pr_info("cmd crc error\n");
				goto free_buf;
			}
			if (crc_data == val) {
				vecm_latch_flag2 |= GAMMA_CRC_PASS;
			} else {
				vecm_latch_flag2 |= GAMMA_CRC_FAIL;
				goto free_buf;
			}
		}
		memset(gm_cv, 0, max_idx * sizeof(unsigned short));
		gamma_count = (strlen(parm[1]) + 2) / 3;
		if (gamma_count > max_idx)
			gamma_count = max_idx;

		for (i = 0; i < gamma_count; ++i) {
			gamma[0] = parm[1][3 * i + 0];
			gamma[1] = parm[1][3 * i + 1];
			gamma[2] = parm[1][3 * i + 2];
			gamma[3] = '\0';
			if (kstrtol(gamma, 16, &val) < 0)
				goto free_buf;
			gm_cv[i] = val;
		}

		switch (parm[0][2]) {
		case 'r':
			amve_write_gamma_table(gm_cv, H_SEL_R);
			break;

		case 'g':
			amve_write_gamma_table(gm_cv, H_SEL_G);
			break;

		case 'b':
			amve_write_gamma_table(gm_cv, H_SEL_B);
			break;
		default:
			break;
		}
	} else if (!strcmp(parm[0], "ggr")) {
		vpp_get_lcd_gamma_table(H_SEL_R);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < max_idx; i++)
				pr_info("gamma_r[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_r[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < max_idx; i++)
					d_convert_str(p_gm->dbg_gm_tbl.gamma_r[i], i, stemp, 3, 16);
				pr_info("gamma_r str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_R;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i < max_idx)
				pr_info("gamma_r[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_r[i]);
		}
	} else if (!strcmp(parm[0], "ggg")) {
		vpp_get_lcd_gamma_table(H_SEL_G);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < max_idx; i++)
				pr_info("gamma_g[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_g[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < max_idx; i++)
					d_convert_str(p_gm->dbg_gm_tbl.gamma_g[i], i, stemp, 3, 16);
				pr_info("gamma_g str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_G;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i < max_idx)
				pr_info("gamma_g[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_g[i]);
		}

	} else if (!strcmp(parm[0], "ggb")) {
		vpp_get_lcd_gamma_table(H_SEL_B);
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < max_idx; i++)
				pr_info("gamma_b[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_b[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			if (!parm[2]) {
				for (i = 0; i < max_idx; i++)
					d_convert_str(p_gm->dbg_gm_tbl.gamma_b[i], i, stemp, 3, 16);
				pr_info("gamma_b str: %s\n", stemp);
			} else if (!strcmp(parm[2], "adb")) {
				vecm_latch_flag2 |= GAMMA_READ_B;
			}
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i < max_idx)
				pr_info("gamma_b[%d] = %x\n",
					i, p_gm->dbg_gm_tbl.gamma_b[i]);
		}
	} else if (!strcmp(parm[0], "ggr_sub")) {
		vpp_get_lcd_gamma_table_sub();
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			for (i = 0; i < 256; i++)
				d_convert_str(gamma_data_r[i], i, stemp, 3, 16);
			pr_info("gamma_r str: %s\n", stemp);
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_r[%d] = %x\n",
					i, gamma_data_r[i]);
			}
	} else if (!strcmp(parm[0], "ggg_sub")) {
		vpp_get_lcd_gamma_table_sub();
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			for (i = 0; i < 256; i++)
				d_convert_str(gamma_data_g[i], i, stemp, 3, 16);
			pr_info("gamma_g str: %s\n", stemp);
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_g[%d] = %x\n",
					i, gamma_data_g[i]);
		}

	} else if (!strcmp(parm[0], "ggb_sub")) {
		vpp_get_lcd_gamma_table_sub();
		if (!strcmp(parm[1], "all")) {
			for (i = 0; i < 256; i++)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		} else if (!strcmp(parm[1], "all_str")) {
			for (i = 0; i < 256; i++)
				d_convert_str(gamma_data_b[i], i, stemp, 3, 16);
			pr_info("gamma_b str: %s\n", stemp);
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0) {
				pr_info("invalid command\n");
				goto free_buf;
			}
			i = val;
			if (i >= 0 && i <= 255)
				pr_info("gamma_b[%d] = %x\n",
					i, gamma_data_b[i]);
		}
	} else {
		pr_info("invalid command\n");
		pr_info("please: cat /sys/class/amvecm/gamma");
	}

	kfree(buf_orig);
	kfree(stemp);
	kfree(gm_cv);
	return count;
free_buf:
	kfree(buf_orig);
	kfree(stemp);
	kfree(gm_cv);
	return -EINVAL;
}

static ssize_t set_gamma_pattern_show(const struct class *cla,
				      const struct class_attribute *attr, char *buf)
{
	pr_info("8bit: echo r g b > /sys/class/amvecm/gamma_pattern\n");
	pr_info("10bit: echo r g b 0xa > /sys/class/amvecm/gamma_pattern\n");
	pr_info("	r g b should be hex\n");
	pr_info("disable gamma pattern:\n");
	pr_info("echo disable > /sys/class/amvecm/gamma_pattern\n");
	return 0;
}

static ssize_t set_gamma_pattern_store(const struct class *cls,
				       const struct class_attribute *attr,
			const char *buffer, size_t count)
{
	static unsigned short r_val[257], g_val[257], b_val[257];
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned int gamma[3];
	long val, i;
	char deliml[3] = " ";
	char delim2[2] = "\n";

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	ps = buf_orig;
	strcat(deliml, delim2);
	*(parm + 3) = NULL;
	while (1) {
		token = strsep(&ps, deliml);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	if (!strcmp(parm[0], "disable")) {
		if (!data_path) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		} else {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R_SUB;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G_SUB;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B_SUB;
		}

		kfree(buf_orig);
		return count;
	}

	if (!strcmp(parm[0], "disable_sub")) {
		vecm_latch_flag |= FLAG_GAMMA_TABLE_R_SUB;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_G_SUB;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_B_SUB;
		kfree(buf_orig);
		return count;
	}

	if (*(parm + 3) != NULL) {
		if (kstrtol(parm[3], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (val == 10) {
			if (kstrtol(parm[0], 16, &val) < 0) {
				kfree(buf_orig);
				return -EINVAL;
			}
			gamma[0] = val;

			if (kstrtol(parm[1], 16, &val) < 0) {
				kfree(buf_orig);
				return -EINVAL;
			}
			gamma[1] = val;

			if (kstrtol(parm[2], 16, &val) < 0) {
				kfree(buf_orig);
				return -EINVAL;
			}
			gamma[2] = val;
		} else {
			kfree(buf_orig);
			return count;
		}
	} else {
		if (kstrtol(parm[0], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		gamma[0] = val << 2;

		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		gamma[1] = val << 2;

		if (kstrtol(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		gamma[2] = val << 2;
	}

	for (i = 0; i < 257; i++) {
		r_val[i] = gamma[0];
		g_val[i] = gamma[1];
		b_val[i] = gamma[2];
	}

	if (cpu_after_eq_t7()) {
		if (!data_path)
			lcd_gamma_api(gamma_index, r_val, g_val, b_val, WR_VCB, WR_MOD, 0);
		else
			lcd_gamma_api(gamma_index_sub, r_val, g_val, b_val, WR_VCB, WR_MOD, 0);
	} else {
		amve_write_gamma_table(r_val, H_SEL_R);
		amve_write_gamma_table(g_val, H_SEL_G);
		amve_write_gamma_table(b_val, H_SEL_B);
	}

	kfree(buf_orig);
	return count;
}
#endif

void white_balance_adjust(int sel, int value)
{
	switch (sel) {
	/*0: en*/
	/*1: pre r   2: pre g   3: pre b*/
	/*4: gain r  5: gain g  6: gain b*/
	/*7: post r  8: post g  9: post b*/
	case 0:
		video_rgb_ogo.en = value;
		break;
	case 1:
		video_rgb_ogo.r_pre_offset = value;
		break;
	case 2:
		video_rgb_ogo.g_pre_offset = value;
		break;
	case 3:
		video_rgb_ogo.b_pre_offset = value;
		break;
	case 4:
		video_rgb_ogo.r_gain = value;
		break;
	case 5:
		video_rgb_ogo.g_gain = value;
		break;
	case 6:
		video_rgb_ogo.b_gain = value;
		break;
	case 7:
		video_rgb_ogo.r_post_offset = value;
		break;
	case 8:
		video_rgb_ogo.g_post_offset = value;
		break;
	case 9:
		video_rgb_ogo.b_post_offset = value;
		break;
	default:
		break;
	}
	ve_ogo_param_update();
}

void white_balance_adjust_sub(int sel, int value)
{
	switch (sel) {
	/*0: en*/
	/*1: pre r   2: pre g   3: pre b*/
	/*4: gain r  5: gain g  6: gain b*/
	/*7: post r  8: post g  9: post b*/
	case 0:
		video_rgb_ogo_sub.en = value;
		break;
	case 1:
		video_rgb_ogo_sub.r_pre_offset = value;
		break;
	case 2:
		video_rgb_ogo_sub.g_pre_offset = value;
		break;
	case 3:
		video_rgb_ogo_sub.b_pre_offset = value;
		break;
	case 4:
		video_rgb_ogo_sub.r_gain = value;
		break;
	case 5:
		video_rgb_ogo_sub.g_gain = value;
		break;
	case 6:
		video_rgb_ogo_sub.b_gain = value;
		break;
	case 7:
		video_rgb_ogo_sub.r_post_offset = value;
		break;
	case 8:
		video_rgb_ogo_sub.g_post_offset = value;
		break;
	case 9:
		video_rgb_ogo_sub.b_post_offset = value;
		break;
	default:
		break;
	}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	ve_ogo_param_update_sub();
#endif
}

static int wb_dbg_flag;
static int wb_rd_val;
static ssize_t amvecm_wb_show(const struct class *cla,
			      const struct class_attribute *attr, char *buf)
{
	if (wb_dbg_flag & WB_PARAM_RD_UPDATE) {
		wb_dbg_flag &= ~WB_PARAM_RD_UPDATE;
		return sprintf(buf, "%d\n", wb_rd_val);
	}

	pr_info("read:	echo r gain_r > /sys/class/amvecm/wb;");
	pr_info("cat /sys/class/amvecm/wb\n");
	pr_info("read:	echo r pre_r > /sys/class/amvecm/wb;");
	pr_info("cat /sys/class/amvecm/wb\n");
	pr_info("read:	echo r post_r > /sys/class/amvecm/wb;");
	pr_info("cat /sys/class/amvecm/wb\n");
	pr_info("write:	echo gain_r value > /sys/class/amvecm/wb\n");
	pr_info("write:	echo preofst_r value > /sys/class/amvecm/wb\n");
	pr_info("write:	echo postofst_r value > /sys/class/amvecm/wb\n");
	return 0;
}

static ssize_t amvecm_wb_store(const struct class *cls,
			       const struct class_attribute *attr,
			const char *buffer, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long value;
	struct tcon_rgb_ogo_s *ogo_data;

	if (!buffer)
		return count;
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "r", 1)) {
		if (!data_path)
			ogo_data = &video_rgb_ogo;
		else
			ogo_data = &video_rgb_ogo_sub;

		if (!strncmp(parm[1], "pre_r", 5)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->r_pre_offset;
		} else if (!strncmp(parm[1], "pre_g", 5)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->g_pre_offset;
		} else if (!strncmp(parm[1], "pre_b", 5)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->b_pre_offset;
		} else if (!strncmp(parm[1], "gain_r", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->r_gain;
		} else if (!strncmp(parm[1], "gain_g", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->g_gain;
		} else if (!strncmp(parm[1], "gain_b", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->b_gain;
		} else if (!strncmp(parm[1], "post_r", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->r_post_offset;
		} else if (!strncmp(parm[1], "post_g", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->g_post_offset;
		} else if (!strncmp(parm[1], "post_b", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->b_post_offset;
		} else if (!strncmp(parm[1], "en", 2)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = ogo_data->en;
		} else if (!strncmp(parm[1], "pre_r_sub", 9)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.r_pre_offset;
		} else if (!strncmp(parm[1], "pre_g_sub", 9)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.g_pre_offset;
		} else if (!strncmp(parm[1], "pre_b_sub", 9)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.b_pre_offset;
		} else if (!strncmp(parm[1], "gain_r_sub", 10)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.r_gain;
		} else if (!strncmp(parm[1], "gain_g_sub", 10)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.g_gain;
		} else if (!strncmp(parm[1], "gain_b_sub", 10)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.b_gain;
		} else if (!strncmp(parm[1], "post_r_sub", 10)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.r_post_offset;
		} else if (!strncmp(parm[1], "post_g_sub", 10)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.g_post_offset;
		} else if (!strncmp(parm[1], "post_b_sub", 10)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.b_post_offset;
		} else if (!strncmp(parm[1], "en_sub", 6)) {
			wb_dbg_flag |= WB_PARAM_RD_UPDATE;
			wb_rd_val = video_rgb_ogo_sub.en;
		}
	} else {
		if (kstrtol(parm[1], 10, &value) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		if (!strncmp(parm[0], "wb_en", 5)) {
			if (!data_path)
				white_balance_adjust(0, value);
			else
				white_balance_adjust_sub(0, value);
			pr_info("\t set wb en\n");
		} else if (!strncmp(parm[0], "preofst_r", 9)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t preofst r over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(1, value);
				else
					white_balance_adjust_sub(1, value);
				pr_info("\t set wb preofst r\n");
			}
		} else if (!strncmp(parm[0], "preofst_g", 9)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t preofst g over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(2, value);
				else
					white_balance_adjust_sub(2, value);
				pr_info("\t set wb preofst g\n");
			}
		} else if (!strncmp(parm[0], "preofst_b", 9)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t preofst b over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(3, value);
				else
					white_balance_adjust_sub(3, value);
				pr_info("\t set wb preofst b\n");
			}
		} else if (!strncmp(parm[0], "gain_r", 6)) {
			if (value > 2047 || value < 0) {
				pr_info("\t gain r over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(4, value);
				else
					white_balance_adjust_sub(4, value);
				pr_info("\t set wb gain r\n");
			}
		} else if (!strncmp(parm[0], "gain_g", 6)) {
			if (value > 2047 || value < 0) {
				pr_info("\t gain g over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(5, value);
				else
					white_balance_adjust_sub(5, value);
				pr_info("\t set wb gain g\n");
			}
		} else if (!strncmp(parm[0], "gain_b", 6)) {
			if (value > 2047 || value < 0) {
				pr_info("\t gain b over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(6, value);
				else
					white_balance_adjust_sub(6, value);
				pr_info("\t set wb gain b\n");
			}
		} else if (!strncmp(parm[0], "postofst_r", 10)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t postofst r over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(7, value);
				else
					white_balance_adjust_sub(7, value);
				pr_info("\t set wb postofst r\n");
			}
		} else if (!strncmp(parm[0], "postofst_g", 10)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t postofst g over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(8, value);
				else
					white_balance_adjust_sub(8, value);
				pr_info("\t set wb postofst g\n");
			}
		} else if (!strncmp(parm[0], "postofst_b", 10)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t postofst b over range\n");
			} else {
				if (!data_path)
					white_balance_adjust(9, value);
				else
					white_balance_adjust_sub(9, value);
				pr_info("\t set wb postofst b\n");
			}
		} else if (!strncmp(parm[0], "wb_en_sub", 9)) {
			white_balance_adjust_sub(0, value);
			pr_info("\t set sub wb en\n");
		} else if (!strncmp(parm[0], "preofst_r_sub", 13)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t sub preofst r over range\n");
			} else {
				white_balance_adjust_sub(1, value);
				pr_info("\t set sub wb preofst r\n");
			}
		} else if (!strncmp(parm[0], "preofst_g_sub", 13)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t sub preofst g over range\n");
			} else {
				white_balance_adjust_sub(2, value);
				pr_info("\t set sub wb preofst g\n");
			}
		} else if (!strncmp(parm[0], "preofst_b_sub", 13)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t sub preofst b over range\n");
			} else {
				white_balance_adjust_sub(3, value);
				pr_info("\t set sub wb preofst b\n");
			}
		} else if (!strncmp(parm[0], "gain_r_sub", 10)) {
			if (value > 2047 || value < 0) {
				pr_info("\t sub gain r over range\n");
			} else {
				white_balance_adjust_sub(4, value);
				pr_info("\t set sub wb gain r\n");
			}
		} else if (!strncmp(parm[0], "gain_g_sub", 10)) {
			if (value > 2047 || value < 0) {
				pr_info("\t sub gain g over range\n");
			} else {
				white_balance_adjust_sub(5, value);
				pr_info("\t set sub wb gain g\n");
			}
		} else if (!strncmp(parm[0], "gain_b_sub", 10)) {
			if (value > 2047 || value < 0) {
				pr_info("\t sub gain b over range\n");
			} else {
				white_balance_adjust_sub(6, value);
				pr_info("\t set sub wb gain b\n");
			}
		} else if (!strncmp(parm[0], "postofst_r_sub", 14)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t sub postofst r over range\n");
			} else {
				white_balance_adjust_sub(7, value);
				pr_info("\t set sub wb postofst r\n");
			}
		} else if (!strncmp(parm[0], "postofst_g_sub", 14)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t sub postofst g over range\n");
			} else {
				white_balance_adjust_sub(8, value);
				pr_info("\t set sub wb postofst g\n");
			}
		} else if (!strncmp(parm[0], "postofst_b_sub", 14)) {
			if (value > 1023 || value < -1024) {
				pr_info("\t sub postofst b over range\n");
			} else {
				white_balance_adjust_sub(9, value);
				pr_info("\t set sub wb postofst b\n");
			}
		}
	}

	kfree(buf_orig);
	return count;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static ssize_t set_hdr_289lut_show(const struct class *cla,
				   const struct class_attribute *attr, char *buf)
{
	int i;

	for (i = 0; i < 289; i++) {
		pr_info("0x%-8x\t", lut_289_mapping[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
	return 0;
}

static ssize_t set_hdr_289lut_store(const struct class *cls,
				    const struct class_attribute *attr,
				    const char *buffer, size_t count)
{
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[4];
	unsigned short *hdr289lut;
	unsigned int gamma_count;
	char gamma[4];
	int i = 0;
	long val;
	char deliml[3] = " ";
	char delim2[2] = "\n";

	hdr289lut = kmalloc(289 * sizeof(unsigned short), GFP_KERNEL);

	buf_orig = kstrdup(buffer, GFP_KERNEL);
	if (!buf_orig) {
		kfree(hdr289lut);
		return -ENOMEM;
	}
	ps = buf_orig;
	strcat(deliml, delim2);
	while (1) {
		token = strsep(&ps, deliml);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (n == 0 || !hdr289lut) {
		kfree(buf_orig);
		kfree(hdr289lut);
		return -EINVAL;
	}
	memset(hdr289lut, 0, 289 * sizeof(unsigned short));
	gamma_count = (strlen(parm[0]) + 2) / 3;
	if (gamma_count > 289)
		gamma_count = 289;

	for (i = 0; i < gamma_count; ++i) {
		gamma[0] = parm[0][3 * i + 0];
		gamma[1] = parm[0][3 * i + 1];
		gamma[2] = parm[0][3 * i + 2];
		gamma[3] = '\0';
		if (kstrtol(gamma, 16, &val) < 0) {
			kfree(buf_orig);
			kfree(hdr289lut);
			return -EINVAL;
		}
		hdr289lut[i] = val;
	}

	for (i = 0; i < gamma_count; i++)
		lut_289_mapping[i] = hdr289lut[i];

	kfree(buf_orig);
	kfree(hdr289lut);
	return count;
}

int cts_pos_x[19];
int amvecm_probe_output_cts(int width, int height, int offset)
{
	int i;
	int val_x, val_y, reg_val;
	int val1 = 0, val2 = 0;
	int cts_pos_y;
	int w, h;

	w = width;
	h = height;
	cts_pos_y = (h / 2 - offset);
	for (i = 0; i < 19; ++i)
		cts_pos_x[i] = (i * w / 19) + w / 38;

	for (i = 0; i < 19; i++) {
		/*set position (x,y)*/
		val_x = cts_pos_x[i] & 0x1fff;
		val_y = cts_pos_y & 0x1fff;

		reg_val = READ_VPP_REG(VPP_MATRIX_PROBE_POS);
		reg_val = reg_val & 0xe000e000;
		reg_val = reg_val | (val_x << 16) | val_y;

		WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, reg_val);
		msleep(50);
		/*get data*/
		val1 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR);
		val2 = READ_VPP_REG(VPP_MATRIX_PROBE_COLOR1);
		pr_info("  %d  %d  %d  %d  %d  %d  %d  %d  %d\n", i + 1, val_x, val_y,
		((val2 & 0xf) << 8) | ((val1 >> 24) & 0xff),
		(val1 >> 12) & 0xfff, val1 & 0xfff,
		(((val2 & 0xf) << 8) | ((val1 >> 24) & 0xff)) >> 2,
		((val1 >> 12) & 0xfff) >> 2,
		(val1 & 0xfff) >> 2);
	}
	return 1;
}

static ssize_t amvecm_dump_output_show(const struct class *cla,
	const struct class_attribute *attr,
	char *buf)
{
	pr_info("echo cfg xxx val > /sys/class/amvecm/amvecm_dump_output\n");
	pr_info("echo type point_count > /sys/class/amvecm/amvecm_dump_output\n");
	pr_info("0/1: cuvahlg/cuva\n");
	pr_info("current config:\n");
	pr_info("width: %d\n", dump_output_cfg_data.width);
	pr_info("height: %d\n", dump_output_cfg_data.height);
	pr_info("row_num: %d\n", dump_output_cfg_data.row_num);
	pr_info("col_num: %d\n", dump_output_cfg_data.col_num);
	pr_info("first_row_num: %d\n", dump_output_cfg_data.first_row_num);
	pr_info("last_row_num: %d\n", dump_output_cfg_data.last_row_num);
	pr_info("point_count: %d\n", dump_output_cfg_data.point_count);
	pr_info("bit_depth: %d\n", dump_output_cfg_data.bit_depth);
	pr_info("delay_count: %d\n", dump_output_cfg_data.delay_count);
	pr_info("probe_port: %d\n", dump_output_cfg_data.probe_port);
	pr_info("1/2/4/8/16: vadj1/vadj2/osd2/postblend/osd1 input\n");
	pr_info("33/34/36/40/48: vadj1/vadj2/osd2/postblend/osd1 output\n");

	return 0;
}

static ssize_t amvecm_dump_output_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	int val = 0;
	int reg_val = 0;
	char *buf_orig, *parm[4] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "cfg", 3)) {
		if (kstrtoint(parm[2], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}

		if (val < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}

		pr_info("%s: cfg val: %d\n", __func__, val);

		if (!strncmp(parm[1], "width", 5)) {
			dump_output_cfg_data.width = val;
		} else if (!strncmp(parm[1], "height", 5)) {
			dump_output_cfg_data.height = val;
		} else if (!strncmp(parm[1], "row_num", 7)) {
			dump_output_cfg_data.row_num = val;
		} else if (!strncmp(parm[1], "col_num", 7)) {
			dump_output_cfg_data.col_num = val;
		} else if (!strncmp(parm[1], "first_row_num", 13)) {
			dump_output_cfg_data.first_row_num = val;
		} else if (!strncmp(parm[1], "last_row_num", 12)) {
			dump_output_cfg_data.last_row_num = val;
		} else if (!strncmp(parm[1], "bit_depth", 9)) {
			dump_output_cfg_data.bit_depth = val;
		} else if (!strncmp(parm[1], "delay_count", 11)) {
			dump_output_cfg_data.delay_count = val;
		} else if (!strncmp(parm[1], "probe_port", 10)) {
			dump_output_cfg_data.probe_port = val;
		} else if (!strncmp(parm[1], "v_start", 7)) {
			dump_output_cfg_data.v_start = val;
		} else {
			kfree(buf_orig);
			return -EINVAL;
		}
	} else {
		if (kstrtoint(parm[0], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}

		pr_info("%s: type val: %d\n", __func__, val);

		if (val == 0) /*hlg*/
			dump_output_cfg_data.first_row_num = 11;
		else if (val == 1) /*pq*/
			dump_output_cfg_data.first_row_num = 12;

		if (kstrtoint(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}

		pr_info("%s: count val: %d\n", __func__, val);

		dump_output_cfg_data.point_count = dump_output_cfg_data.col_num *
			(dump_output_cfg_data.row_num - 2) +
			dump_output_cfg_data.last_row_num +
			dump_output_cfg_data.first_row_num;

		if (val < dump_output_cfg_data.point_count &&
			val > 0)
			dump_output_cfg_data.point_count = val;

		amvecm_dump_output_pos_cal();

		val = dump_output_cfg_data.probe_port;

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
			!is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
			chip_type_id != chip_txhd2 &&
			chip_type_id != chip_s1a &&
			chip_type_id != chip_s7 &&
			chip_type_id != chip_s7d &&
			!is_meson_s6_cpu()) {
			reg_val = READ_VPP_REG(VPP_PROBE_CTRL);
			reg_val = reg_val & 0xffffffc0;
			reg_val = reg_val | (val & 0x3f);
			if (val & (1 << 5))
				reg_val |= 1 << 15;
			else
				reg_val &= ~(1 << 15);

			WRITE_VPP_REG(VPP_PROBE_CTRL, reg_val);
			WRITE_VPP_REG(VPP_HI_COLOR, 0x80000000);

			pr_info("%s: VPP_PROBE_CTRL[0x%x]: 0x%x\n",
				__func__, VPP_PROBE_CTRL, reg_val);
		} else {
			reg_val = READ_VPP_REG(VPP_MATRIX_CTRL);
			reg_val = reg_val & 0xffff03ff;
			reg_val = reg_val | ((val & 0x3f) << 10);

			WRITE_VPP_REG(VPP_MATRIX_CTRL, reg_val);

			pr_info("%s: VPP_MATRIX_CTRL[0x%x]: 0x%x\n",
				__func__, VPP_MATRIX_CTRL, reg_val);
		}

		dump_output_flag = 1;
	}

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_set_post_matrix_show(const struct class *cla,
					   const struct class_attribute *attr,
					   char *buf)
{
	int val;

	pr_info("Usage:\n");
	pr_info("echo port > /sys/class/amvecm/matrix_set\n");
	pr_info("1 : vadj1 input\n");
	pr_info("2 : vadj2 input\n");
	pr_info("4 : osd2 input\n");
	pr_info("8 : postblend input\n");
	pr_info("16 : osd1 input\n");
	pr_info("33 : vadj1 output\n");
	pr_info("34 : vadj2 output\n");
	pr_info("36 : osd2 output\n");
	pr_info("40 : postblend output\n");
	pr_info("48: osd1 output\n");
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		!is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
		chip_type_id != chip_txhd2 && chip_type_id != chip_s1a &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d &&
		!is_meson_s6_cpu()) {
		val = READ_VPP_REG(VPP_PROBE_CTRL);
		pr_info("current setting: %d\n", val & 0x3f);
	} else {
		val = READ_VPP_REG(VPP_MATRIX_CTRL);
		pr_info("current setting: %d\n", (val >> 10) & 0x3f);
	}

	return 0;
}

static ssize_t amvecm_set_post_matrix_store(const struct class *cla,
					    const struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int val, reg_val;

	if (kstrtoint(buf, 10, &val) < 0)
		return -EINVAL;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		!is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
		chip_type_id != chip_txhd2 &&
		chip_type_id != chip_s1a &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d &&
		!is_meson_s6_cpu()) {
		reg_val = READ_VPP_REG(VPP_PROBE_CTRL);
		reg_val = reg_val & 0xffffffc0;
		/*reg_val |= 0x10000;*/
		/* enable probe hit */
		reg_val = reg_val | (val & 0x3f);
		if (val & (1 << 5))
			reg_val |= 1 << 15;
		else
			reg_val &= ~(1 << 15);

		WRITE_VPP_REG(VPP_PROBE_CTRL, reg_val);
		WRITE_VPP_REG(VPP_HI_COLOR, 0x80000000);

		pr_info("VPP_PROBE_CTRL is set\n");

	} else {
		reg_val = READ_VPP_REG(VPP_MATRIX_CTRL);
		reg_val = reg_val & 0xffff03ff;
		reg_val = reg_val | ((val & 0x3f) << 10);

		WRITE_VPP_REG(VPP_MATRIX_CTRL, reg_val);

		pr_info("VPP_MATRIX_CTRL is set\n");
	}
	return count;
}

static ssize_t amvecm_post_matrix_pos_show(const struct class *cla,
					   const struct class_attribute *attr,
					   char *buf)
{
	int val;

	pr_info("Usage:\n");
	pr_info("echo x y > /sys/class/amvecm/matrix_pos\n");

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
	    !is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
	    chip_type_id != chip_txhd2 &&
	    chip_type_id != chip_s1a &&
	    chip_type_id != chip_s7 &&
	    chip_type_id != chip_s7d &&
	    !is_meson_s6_cpu())
		val = READ_VPP_REG(VPP_PROBE_POS);
	else
		val = READ_VPP_REG(VPP_MATRIX_PROBE_POS);
	pr_info("current position: %d %d\n",
		(val >> 16) & 0x1fff,
			(val >> 0) & 0x1fff);
	return 0;
}

static ssize_t amvecm_post_matrix_pos_store(const struct class *cla,
					    const struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int val_x, val_y, reg_val;
	char *buf_orig, *parm[2] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (kstrtoint(parm[0], 10, &val_x) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}
	if (kstrtoint(parm[1], 10, &val_y) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}

	val_x = val_x & 0x1fff;
	val_y = val_y & 0x1fff;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		!is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
		chip_type_id != chip_txhd2 &&
		chip_type_id != chip_s1a &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d &&
		!is_meson_s6_cpu())
		reg_val = READ_VPP_REG(VPP_PROBE_POS);
	else
		reg_val = READ_VPP_REG(VPP_MATRIX_PROBE_POS);
	reg_val = reg_val & 0xe000e000;
	reg_val = reg_val | (val_x << 16) | val_y;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		!is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
		chip_type_id != chip_txhd2 &&
		chip_type_id != chip_s1a &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d &&
		!is_meson_s6_cpu())
		WRITE_VPP_REG(VPP_PROBE_POS, reg_val);
	else
		WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, reg_val);

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_post_matrix_data_show(const struct class *cla,
					    const struct class_attribute *attr,
					    char *buf)
{
	int len = 0, val1 = 0, val2 = 0;
	u8 bit_depth = 12;
	u32 probe_color, probe_color1;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
		!is_meson_s4d_cpu() && !is_meson_s4_cpu() &&
		chip_type_id != chip_txhd2 &&
		chip_type_id != chip_s1a &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d &&
		!is_meson_s6_cpu()) {
		probe_color = VPP_PROBE_COLOR;
		probe_color1 = VPP_PROBE_COLOR1;
	} else {
		probe_color = VPP_MATRIX_PROBE_COLOR;
		probe_color1 = VPP_MATRIX_PROBE_COLOR1;
	}

	if (is_meson_tl1_cpu() ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
		is_meson_s4_cpu() ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5M ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		chip_type_id == chip_txhd2 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		is_meson_s4d_cpu() ||
		chip_type_id == chip_s1a)
		bit_depth = 10;

	if (bit_depth == 10) {
		val1 = READ_VPP_REG(probe_color);
		len += sprintf(buf + len,
		"VPP_MATRIX_PROBE_COLOR %d, %d, %d\n",
		(val1 >> 20) & 0x3ff,
		(val1 >> 10) & 0x3ff,
		(val1 >> 0) & 0x3ff);
	} else {
		val1 = READ_VPP_REG(probe_color);
		val2 = READ_VPP_REG(probe_color1);
		len += sprintf(buf + len,
		"VPP_MATRIX_PROBE_COLOR %d, %d, %d\n",
		((val2 & 0xf) << 8) | ((val1 >> 24) & 0xff),
		(val1 >> 12) & 0xfff, val1 & 0xfff);
	}

	return len;
}

static ssize_t amvecm_post_matrix_data_store(const struct class *cla,
					     const struct class_attribute *attr,
					     const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_sr1_reg_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	unsigned int addr;

	addr = ((sr1_index + 0x3280) << 2) | 0xd0100000;
	return sprintf(buf, "0x%x = 0x%x\n",
			addr, sr1_ret_val[sr1_index]);
}

static ssize_t amvecm_sr1_reg_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	size_t r;
	unsigned int addr, off_addr = 0;

	r = sscanf(buf, "0x%x", &addr);
	addr = (addr & 0xffff) >> 2;
	if (r != 1 || addr > 0x32e4 || addr < 0x3280)
		return -EINVAL;
	off_addr = addr - 0x3280;
	sr1_index = off_addr;
	sr1_ret_val[off_addr] = sr1_reg_val[off_addr];

	return count;
}

static ssize_t amvecm_write_sr1_reg_val_show(const struct class *cla,
					     const struct class_attribute *attr,
					     char *buf)
{
	return 0;
}

static ssize_t amvecm_write_sr1_reg_val_store(const struct class *cla,
					      const struct class_attribute *attr,
					      const char *buf, size_t count)
{
	size_t r;
	unsigned int val;

	r = sscanf(buf, "0x%x", &val);
	if (r != 1)
		return -EINVAL;
	sr1_reg_val[sr1_index] = val;

	return count;
}
#endif

static ssize_t amvecm_dump_reg_show(const struct class *cla,
				    const struct class_attribute *attr,
				    char *buf)
{
	unsigned int addr;
	unsigned int value;
	unsigned int base_reg;

	base_reg = codecio_reg_start[CODECIO_VCBUS_BASE];

	pr_info("----dump sharpness0 reg----\n");
	for (addr = 0x3200;
		addr <= 0x3264; addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		for (addr = 0x3265;
			addr <= 0x3272; addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
	if (is_meson_txlx_cpu()) {
		for (addr = 0x3273;
			addr <= 0x327f; addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
#endif
	pr_info("----dump sharpness1 reg----\n");
	for (addr = (0x3200 + 0x80);
		addr <= (0x3264 + 0x80); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_txl_cpu() || is_meson_txlx_cpu()) {
		for (addr = (0x3265 + 0x80);
			addr <= (0x3272 + 0x80); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
	if (is_meson_txlx_cpu()) {
		for (addr = (0x3273 + 0x80);
			addr <= (0x327f + 0x80); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
#endif
	pr_info("----dump cm reg----\n");
	for (addr = 0x200; addr <= 0x21e; addr++) {
		WRITE_VPP_REG_EX(VPP_CHROMA_ADDR_PORT, addr, 0);
		value = READ_VPP_REG_EX(VPP_CHROMA_DATA_PORT, 0);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			addr, addr,
				value);
	}
	for (addr = 0x100; addr <= 0x1fc; addr++) {
		WRITE_VPP_REG_EX(VPP_CHROMA_ADDR_PORT, addr, 0);
		value = READ_VPP_REG_EX(VPP_CHROMA_DATA_PORT, 0);
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			addr, addr,
				value);
	}

	pr_info("----dump vd1 IF0 reg----\n");
	for (addr = (0x1a50);
		addr <= (0x1a69); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump vpp1 part1 reg----\n");
	for (addr = (0x1d00);
		addr <= (0x1d6e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));

	pr_info("----dump vpp1 part2 reg----\n");
	for (addr = (0x1d72);
		addr <= (0x1de4); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));

	if (chip_type_id != chip_t6w && chip_type_id != chip_t6x) {
		pr_info("----dump ndr reg----\n");
		for (addr = (0x2d00);
			addr <= (0x2d78); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
		pr_info("----dump nr3 reg----\n");
		for (addr = (0x2ff0);
			addr <= (0x2ff6); addr++)
			pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
				(base_reg + (addr << 2)), addr,
					READ_VPP_REG_EX(addr, 0));
	}
	pr_info("----dump vlock reg----\n");
	for (addr = (0x3000);
		addr <= (0x3020); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump super scaler0 reg----\n");
	for (addr = (0x3100);
		addr <= (0x3115); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump super scaler1 reg----\n");
	for (addr = (0x3118);
		addr <= (0x312e); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump xvycc reg----\n");
	for (addr = (0x3158);
		addr <= (0x3179); addr++)
		pr_info("[0x%x]vcbus[0x%04x]=0x%08x\n",
			(base_reg + (addr << 2)), addr,
				READ_VPP_REG_EX(addr, 0));
	pr_info("----dump reg done----\n");
	return 0;
}

static ssize_t amvecm_dump_reg_store(const struct class *cla,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static ssize_t amvecm_dump_vpp_hist_show(const struct class *cla,
					 const struct class_attribute *attr,
					 char *buf)
{
	vpp_dump_histgram();
	return 0;
}

static ssize_t amvecm_dump_vpp_hist_store(const struct class *cla,
					  const struct class_attribute *attr,
					   const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_dbg_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	int ret;

	ret = amvecm_hdr_dbg(0);

	return 0;
}

static ssize_t amvecm_hdr_dbg_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long val = 0;
	char *buf_orig, *parm[5] = {NULL};
	int i;
	int curve_val[65] = {0};
	char *stemp = NULL;
	int addr_ofst, sel_val, para_val, comp_val;
	unsigned int v_size, h_size = 0;

	if (!buf)
		return count;

	stemp = kzalloc(400, GFP_KERNEL);
	if (!stemp)
		return 0;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		kfree(stemp);
		return -ENOMEM;
	}

	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "hdr_dbg", 7)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		debug_hdr = val;
		pr_info("debug_hdr=0x%x\n", debug_hdr);
	} else if (!strncmp(parm[0], "hdr10_pr", 8)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_pr = val;
		pr_info("hdr10_pr=0x%x\n", hdr10_pr);
	} else if (!strncmp(parm[0], "clip_disable", 12)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_clip_disable = val;
		pr_info("hdr10_clip_disable=0x%x\n",
			hdr10_clip_disable);
	} else if (!strncmp(parm[0], "clip_luma", 9)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_clip_luma = val;
		pr_info("clip_luma=0x%x\n", hdr10_clip_luma);
	} else if (!strncmp(parm[0], "clip_margin", 11)) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		hdr10_clip_margin = val;
		pr_info("hdr10_clip_margin=0x%x\n", hdr10_clip_margin);
	} else if (!strncmp(parm[0], "hdr_sdr_ootf", 12)) {
		for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++) {
			pr_info("%d ", oo_y_lut_hdr_sdr_def[i]);
			if ((i + 1) % 10 == 0)
				pr_info("\n");
		}
		pr_info("\n");
	} else if (!strncmp(parm[0], "cgain_lut", 9)) {
		if (!strncmp(parm[1], "rv", 2)) {
			for (i = 0; i < 65; i++)
				d_convert_str(cgain_lut_bypass[i],
					      i, stemp, 4, 10);
			pr_info("%s\n", stemp);
		} else if (!strncmp(parm[1], "wv", 2)) {
			str_sapr_to_d(parm[2], curve_val, 5);
			for (i = 0; i < 65; i++)
				cgain_lut_bypass[i] = curve_val[i];
		}
	} else if (!strncmp(parm[0], "lut1_param", 10)) {
		if (!strncmp(parm[1], "blend", 5))
			sel_val = 0;
		else if (!strncmp(parm[1], "sft", 3))
			sel_val = 1;
		else if (!strncmp(parm[1], "idx", 3))
			sel_val = 2;
		else
			goto free_buf;
		pr_info("lut1_param sel_val = %d\n", sel_val);
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		para_val = val;
		pr_info("lut1_param para_val = %d\n", para_val);
		if (kstrtoul(parm[3], 16, &val) < 0)
			addr_ofst = 0x2a00;
		else
			addr_ofst = val;
		pr_info("lut1_param addr_ofst = 0x%x\n", addr_ofst);
		hdr_lut1_param_debug(addr_ofst, sel_val, para_val);
	} else if (!strncmp(parm[0], "gmt_comp_en", 11)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		comp_val = val;
		pr_info("gmt_comp_en comp_val = %d\n", comp_val);
		if (kstrtoul(parm[2], 16, &val) < 0)
			addr_ofst = 0x2a00;
		else
			addr_ofst = val;
		pr_info("gmt_comp_en addr_ofst = 0x%x\n", addr_ofst);
		sel_val = GMT_COMP_SEL_EN;
		hdr_gmut_comp_debug(addr_ofst, sel_val, comp_val);
	} else if (!strncmp(parm[0], "gmt_comp", 8)) {
		if (!strncmp(parm[1], "ofst_r", 6))
			sel_val = GMT_COMP_SEL_OFST_R;
		else if (!strncmp(parm[1], "ofst_g", 6))
			sel_val = GMT_COMP_SEL_OFST_G;
		else if (!strncmp(parm[1], "ofst_b", 6))
			sel_val = GMT_COMP_SEL_OFST_B;
		else if (!strncmp(parm[1], "min_r", 5))
			sel_val = GMT_COMP_SEL_MIN_R;
		else if (!strncmp(parm[1], "min_g", 5))
			sel_val = GMT_COMP_SEL_MIN_G;
		else if (!strncmp(parm[1], "min_b", 5))
			sel_val = GMT_COMP_SEL_MIN_B;
		else
			goto free_buf;
		pr_info("gmt_comp sel_val = %d\n", sel_val);
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		comp_val = val;
		pr_info("gmt_comp comp_val = %d\n", comp_val);
		if (kstrtoul(parm[3], 16, &val) < 0)
			addr_ofst = 0x2a00;
		else
			addr_ofst = val;
		pr_info("gmt_comp addr_ofst = 0x%x\n", addr_ofst);
		hdr_gmut_comp_debug(addr_ofst, sel_val, comp_val);
	} else if (!strcmp(parm[0], "cuva_dbg")) {
		cuva_hdr_dbg();
	} else if (!strcmp(parm[0], "reg_dump")) {
		if (!parm[1]) {
			hdr_reg_dump(0);
		} else {
			//if (kstrtoul(parm[1], 16, &val) < 0)
			if (kstrtol(parm[1], 10, &val) < 0)
				goto free_buf;
			hdr_reg_dump(val);
		}
	} else if (!strcmp(parm[0], "s5_reg_dump")) {
		if (!parm[1]) {
			s5_hdr_reg_dump(0);
		} else {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			s5_hdr_reg_dump(val);
		}
	} else if (!strcmp(parm[0], "sbtm")) {
		sbtm_tmo_hdr_hdr_param_dbg(parm);
		sbtm_sbtmdb_reg_dbg(parm);
		sbtm_param_dbg(parm);
	} else if (!strncmp(parm[0], "emds_pkt_idx", 12)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		emds_group_idx = val;
		pr_info("cuva emds pkt group idx is %d\n", emds_group_idx);
	} else if (!strncmp(parm[0], "tx_cuva_mode", 12)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		tx_cuva_mode = val;
		pr_info("tx_cuva_mode is %d\n", tx_cuva_mode);
	} else if (!strncmp(parm[0], "cuva_dbg_mode", 13)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		cuva_dbg_mode = val;
		pr_info("cuva_dbg_mode is %d\n", cuva_dbg_mode);
	} else if (!strncmp(parm[0], "get_cuva_lum", 12)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		sel_val = val;
		val = get_max_output_lum(sel_val);
		pr_info("cuva get type(%d) max_output_lum %ld\n", sel_val, val);
	} else if (!strncmp(parm[0], "set_cuva_lum", 12)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		sel_val = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		set_max_output_lum(sel_val, val);
		pr_info("cuva set type(%d) max_output_lum %ld\n", sel_val, val);
	} else if (!strcmp(parm[0], "hdr10p_adp")) {
		hdr10p_adp_dbg(parm);
	} else if (!strcmp(parm[0], "get_hist")) {
		if (chip_type_id != chip_t6x)
			goto free_buf;

		if (!parm[1])
			goto free_buf;
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		hdr_hist_test_module = val;

		if (parm[2]) {
			if (kstrtoul(parm[2], 10, &val) < 0)
				hdr_hist_test_cnt = 1;
			else
				hdr_hist_test_cnt = val;
		} else {
			hdr_hist_test_cnt = 1;
		}
	} else if (!strcmp(parm[0], "hist_size")) {
		unsigned int hdr_size = VPU_HDR2_SIZE_IN;
		unsigned int hdr_offset = 0;

		if (chip_type_id != chip_t6x)
			goto free_buf;

		if (!parm[1])
			goto free_buf;
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;

		if (val == 1) {
			hdr_size = VPU_HDR2_SIZE_IN;
		} else if (val == 2) {
			hdr_size = VD2_HDR_IN_SIZE;
			hdr_offset = 0x1700;
		} else if (val == 3) {
			hdr_size = VPU_HDR2_FRM2_SIZE;
			hdr_offset = 0x80;
		} else if (val == 4) {
			hdr_size = VD2_1_HDR_IN_SIZE;
			hdr_offset = 0x1780;
		}

		h_size = READ_VPP_REG_BITS(hdr_size, 0, 13);
		v_size = READ_VPP_REG_BITS(hdr_size, 16, 13);
		WRITE_VPP_REG(DOLBY_HDR2_HIST_H_START_END + hdr_offset, h_size - 1);
		WRITE_VPP_REG(DOLBY_HDR2_HIST_V_START_END + hdr_offset, v_size - 1);
		WRITE_VPP_REG(DOLBY_HDR2_HIST_CTRL + hdr_offset, 0x10010);
	} else if (!strcmp(parm[0], "hist_dma_case")) {
		if (!parm[1]) {
			pr_info("cur hdr_hist_dma_case = %d\n", hdr_hist_dma_case);
		} else {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
			hdr_hist_dma_case = val;
		}
	}

	hdr10_tmo_dbg(parm);

free_buf:
	kfree(stemp);
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_ai_color_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	int len = 0;

	ai_color_parm_show();
	len = aicolor_param_adb_show(buf);
	return len;
}

static ssize_t amvecm_ai_color_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig, *parm[5] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	parse_param_amvecm(buf_orig, (char **)&parm);

	ai_color_debug_store(parm);
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_hdr_reg_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	int ret;

	ret = amvecm_hdr_dbg(1);

	return 0;
}

static ssize_t amvecm_hdr_reg_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_tmo_show(const struct class *cla,
			const struct class_attribute *attr, char *buf)
{
	int len = 0;

	// hdr10_tmo_parm_show();
	len = hdr_tmo_adb_show(buf);
	return len;

}

static ssize_t amvecm_hdr_tmo_store(const struct class *cla,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_hdr_fw_dbg_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	// hdr10_tmo_parm_show();
	len = hdr_tmo_alg_dbg_show(buf);
	return len;
}

static ssize_t amvecm_hdr_fw_dbg_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[5] = {NULL};
	int ret;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);
	ret = hdr_tmo_alg_dbg(parm);
	kfree(buf_orig);

	if (ret < 0)
		pr_info("set parameters failed\n");

	return count;
}
static ssize_t amvecm_hdr_param_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t amvecm_hdr_param_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	char *buf_orig, *param[5] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&param);
	hdr10_param_dbg(param);
	force_toggle();

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_pc_mode_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	pr_info("pc:echo 0x0 > /sys/class/amvecm/pc_mode\n");
	pr_info("other:echo 0x1 > /sys/class/amvecm/pc_mode\n");
	pr_info("pc_mode:%d,pc_mode_last:%d\n", pc_mode, pc_mode_last);
	return 0;
}

static ssize_t amvecm_pc_mode_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1)
		return -EINVAL;

	if (!pq_load_en)
		return count;

	if (val == 1) {
		pc_mode = 1;
	} else if (val == 0) {
		pc_mode = 0;
		pc_mode_last = 0xff;
	}

	return count;
}

static ssize_t amvecm_color_tune_show(const struct class *cla,
				    const struct class_attribute *attr, char *buf)
{
	struct ct_func_s *ct_f = get_ct_func();

	return sprintf(buf, "en = %d\nrgain_r = %d\nrgain_g = %d\nrgain_b = %d\n"
		"ggain_r = %d\nggain_g = %d\nggain_b = %d\n"
		"bgain_r = %d\nbgain_g = %d\nbgain_b = %d\n"
		"cgain_r = %d\ncgain_g = %d\ncgain_b = %d\n"
		"mgain_r = %d\nmgain_g = %d\nmgain_b = %d\n"
		"ygain_r = %d\nygain_g = %d\nygain_b = %d\n",
		ct_f->cl_par->en,
		ct_f->cl_par->rgain_r, ct_f->cl_par->rgain_g, ct_f->cl_par->rgain_b,
		ct_f->cl_par->ggain_r, ct_f->cl_par->ggain_g, ct_f->cl_par->ggain_b,
		ct_f->cl_par->bgain_r, ct_f->cl_par->bgain_g, ct_f->cl_par->bgain_b,
		ct_f->cl_par->cgain_r, ct_f->cl_par->cgain_g, ct_f->cl_par->cgain_b,
		ct_f->cl_par->mgain_r, ct_f->cl_par->mgain_g, ct_f->cl_par->mgain_b,
		ct_f->cl_par->ygain_r, ct_f->cl_par->ygain_g, ct_f->cl_par->ygain_b);
}

static ssize_t amvecm_color_tune_store(const struct class *cla,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	int ret;
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);

	ret = ct_dbg(parm);

	if (ret < 0)
		pr_info("set parameters failed\n");

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_dma_buf_show(const struct class *cla,
				    const struct class_attribute *attr, char *buf)
{
	read_dma_buf();
	return 0;
}

static ssize_t amvecm_dma_buf_store(const struct class *cla,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long tbl_id;
	long value;
	long table_offset = 0;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);

	if (kstrtoul(parm[0], 10, &tbl_id) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}

	if (kstrtoul(parm[1], 16, &value) < 0) {
		kfree(buf_orig);
		return -EINVAL;
	}

	if (parm[2]) {
		if (kstrtoul(parm[2], 16, &table_offset) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
	}

	write_dma_buf((u32)table_offset, (u32)tbl_id, (u32)value);

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_ble_whe_dbg_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	if (dnlp_dbg_flag & BLK_ADJ_EN) {
		dnlp_dbg_flag &= ~BLK_ADJ_EN;
		len += sprintf(buf + len, "blk_adj_en = %d\n",
			ble_whe_param_load.blk_adj_en);
	}

	if (dnlp_dbg_flag & BLK_END) {
		dnlp_dbg_flag &= ~BLK_END;
		len += sprintf(buf + len, "blk_end = %d\n",
			ble_whe_param_load.blk_end);
	}

	if (dnlp_dbg_flag & BLK_SLP) {
		dnlp_dbg_flag &= ~BLK_SLP;
		len += sprintf(buf + len, "blk_slp = %d\n",
			ble_whe_param_load.blk_slp);
	}

	if (dnlp_dbg_flag & BRT_ADJ_EN) {
		dnlp_dbg_flag &= ~BRT_ADJ_EN;
		len += sprintf(buf + len, "brt_adj_en = %d\n",
			ble_whe_param_load.brt_adj_en);
	}

	if (dnlp_dbg_flag & BRT_START) {
		dnlp_dbg_flag &= ~BRT_START;
		len += sprintf(buf + len, "brt_start = %d\n",
			ble_whe_param_load.brt_start);
	}

	if (dnlp_dbg_flag & BRT_SLP) {
		dnlp_dbg_flag &= ~BRT_SLP;
		len += sprintf(buf + len, "brt_slp = %d\n",
			ble_whe_param_load.brt_slp);
	}

	return len;
}

static ssize_t amvecm_ble_whe_dbg_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	long val = 0;
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "blk_adj_en")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BLK_ADJ_EN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		ble_whe_param_load.blk_adj_en = (int)val;
		pr_info("blk_adj_en = %d\n", (int)val);
	} else if (!strcmp(parm[0], "blk_end")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BLK_END;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		ble_whe_param_load.blk_end = (int)val;
		pr_info("blk_end = %d\n", (int)val);
	} else if (!strcmp(parm[0], "blk_slp")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BLK_SLP;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		ble_whe_param_load.blk_slp = (int)val;
		pr_info("blk_slp = %d\n", (int)val);
	} else if (!strcmp(parm[0], "brt_adj_en")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BRT_ADJ_EN;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		ble_whe_param_load.brt_adj_en = (int)val;
		pr_info("brt_adj_en = %d\n", (int)val);
	} else if (!strcmp(parm[0], "brt_start")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BRT_START;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		ble_whe_param_load.brt_start = (int)val;
		pr_info("brt_start = %d\n", (int)val);
	} else if (!strcmp(parm[0], "brt_slp")) {
		if (!parm[1]) {
			dnlp_dbg_flag |= BRT_SLP;
			goto for_read;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		ble_whe_param_load.brt_slp = (int)val;
		pr_info("brt_slp = %d\n", (int)val);
	} else if (!strcmp(parm[0], "read_param")) {
		pr_info("blk_adj_en = %d\n", ble_whe_param_load.blk_adj_en);
		pr_info("blk_end = %d\n", ble_whe_param_load.blk_end);
		pr_info("blk_slp = %d\n", ble_whe_param_load.blk_slp);
		pr_info("brt_adj_en = %d\n", ble_whe_param_load.brt_adj_en);
		pr_info("brt_start = %d\n", ble_whe_param_load.brt_start);
		pr_info("brt_slp = %d\n", ble_whe_param_load.brt_slp);
	}

	vecm_latch_flag2 |= BLE_WHE_UPDATE;
	kfree(buf_orig);
	return count;

for_read:
	kfree(buf_orig);
	return count;

free_buf:
	kfree(buf_orig);
	return -EINVAL;
}

void pc_mode_process(int vpp_index)
{
	if (pc_mode == 1 &&
		pc_mode != pc_mode_last &&
		pc_mode_last != 0xff) {
		/* open dnlp clock gate */
		lc_en = pq_cfg.lc_en;
		dnlp_en = pq_cfg.dnlp_en;
		if (dnlp_en)
			ve_enable_dnlp();
		else
			ve_disable_dnlp();
		cm_en = pq_cfg.cm_en;

		if (chip_type_id >= chip_s7d)
			amve_sharpness_sub_vsync_ctrl(1, vpp_index);
		else
			amve_old_sharpness_sub_vsync_ctrl(1, vpp_index);

		pc_mode_last = pc_mode;
	} else if (pc_mode == 0 &&
		pc_mode != pc_mode_last) {
		dnlp_en = 0;
		lc_en = 0;
		ve_disable_dnlp();
		cm_en = 0;

		if (chip_type_id >= chip_s7d)
			amve_sharpness_sub_vsync_ctrl(0, vpp_index);
		else
			amve_old_sharpness_sub_vsync_ctrl(0, vpp_index);

		pc_mode_last = pc_mode;
	}
}

void amvecm_black_ext_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 3, 1);
	else
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 3, 1);
}

void amvecm_black_ext_start_adj(unsigned int value)
{
	if (value > 255)
		return;
	WRITE_VPP_REG_BITS(VPP_BLACKEXT_CTRL, value, 24, 8);
}

void amvecm_black_ext_slope_adj(unsigned int value)
{
	if (value > 255)
		return;
	WRITE_VPP_REG_BITS(VPP_BLACKEXT_CTRL, value, 16, 8);
}

void amvecm_sr0_pk_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   1, 1, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   0, 1, 1);
}

void amvecm_sr1_pk_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   1, 1, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   0, 1, 1);
}

void amvecm_sr0_dering_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   1, 28, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   0, 28, 3);
}

void amvecm_sr1_dering_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   1, 28, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   0, 28, 3);
}

void amvecm_sr0_dejaggy_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   1, 0, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   0, 0, 1);
}

void amvecm_sr1_dejaggy_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   1, 0, 1);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   0, 0, 1);
}

void amvecm_sr0_direction_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   7, 0, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   0, 0, 3);
}

void amvecm_sr1_direction_enable(unsigned int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   7, 0, 3);
	else
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   0, 0, 3);
}
#endif

void pq_user_latch_process(int vpp_index)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int i = 0;
	int input_clr_md = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;
	struct cm_color_md cm_clr_md;
#endif
	int sat_hue_val = 0;

	if (pq_user_latch_flag & PQ_USER_BLK_EN) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		pq_user_latch_flag &= ~PQ_USER_BLK_EN;
		amvecm_black_ext_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_BLK_DIS) {
		pq_user_latch_flag &= ~PQ_USER_BLK_DIS;
		amvecm_black_ext_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_BLK_START) {
		pq_user_latch_flag &= ~PQ_USER_BLK_START;
		amvecm_black_ext_start_adj(pq_user_value);
	} else if (pq_user_latch_flag & PQ_USER_BLK_SLOPE) {
		pq_user_latch_flag &= ~PQ_USER_BLK_SLOPE;
		amvecm_black_ext_slope_adj(pq_user_value);
	} else if (pq_user_latch_flag & PQ_USER_SR0_PK_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_PK_EN;
		amvecm_sr0_pk_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_PK_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_PK_DIS;
		amvecm_sr0_pk_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_PK_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_PK_EN;
		amvecm_sr1_pk_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_PK_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_PK_DIS;
		amvecm_sr1_pk_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DERING_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DERING_EN;
		amvecm_sr0_dering_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DERING_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DERING_DIS;
		amvecm_sr0_dering_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DERING_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DERING_EN;
		amvecm_sr1_dering_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DERING_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DERING_DIS;
		amvecm_sr1_dering_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DEJAGGY_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DEJAGGY_EN;
		amvecm_sr0_dejaggy_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DEJAGGY_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DEJAGGY_DIS;
		amvecm_sr0_dejaggy_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DEJAGGY_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DEJAGGY_EN;
		amvecm_sr1_dejaggy_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DEJAGGY_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DEJAGGY_DIS;
		amvecm_sr1_dejaggy_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DIRECTION_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DIRECTION_EN;
		amvecm_sr0_direction_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR0_DIRECTION_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR0_DIRECTION_DIS;
		amvecm_sr0_direction_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DIRECTION_EN) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DIRECTION_EN;
		amvecm_sr1_direction_enable(true);
	} else if (pq_user_latch_flag & PQ_USER_SR1_DIRECTION_DIS) {
		pq_user_latch_flag &= ~PQ_USER_SR1_DIRECTION_DIS;
		amvecm_sr1_direction_enable(false);
	} else if (pq_user_latch_flag & PQ_USER_CMS_CURVE_SAT ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_LUMA ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE_HS ||
		   pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE) {
		if (cm_cur_work_color_md == cm_9_color) {
			cm_color_md_max = ecm2colormode_max;
			cm_clr_md.color_type = cm_9_color;
			cm_clr_md.cm_9_color_md = ecm2colormode_purple;
			cm_clr_md.cm_14_color_md = cm_14_ecm2colormode_max;
			cm_clr_md.color_value = 0;
		} else {
			cm_color_md_max = cm_14_ecm2colormode_max;
			cm_clr_md.color_type = cm_14_color;
			cm_clr_md.cm_9_color_md = ecm2colormode_max;
			cm_clr_md.cm_14_color_md =
				cm_14_ecm2colormode_blue_purple;
			cm_clr_md.color_value = 0;
		}

		if (cm2_debug) {
			pr_info("cm_color_md_max:%d\n",
				cm_color_md_max);
			pr_info("color_type:%d\n",
				cm_clr_md.color_type);
			pr_info("cm_9_color_md:%d\n",
				cm_clr_md.cm_9_color_md);
			pr_info("cm_14_color_md:%d\n",
				cm_clr_md.cm_14_color_md);
			pr_info("color_value:%d\n",
				cm_clr_md.color_value);
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_SAT) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_SAT;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_sat_array[i][2] == 1) {
					cm2_curve_update_sat(cm_clr_md);
					cm2_sat_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_LUMA) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_LUMA;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_luma_array[i][2] == 1) {
					cm2_curve_update_luma(cm_clr_md);
					cm2_luma_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE_HS) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_HUE_HS;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_hue_by_hs_array[i][2] == 1) {
					cm2_curve_update_hue_by_hs(cm_clr_md);
					cm2_hue_by_hs_array[i][2] = 0;
				}
			}
		}
		if (pq_user_latch_flag & PQ_USER_CMS_CURVE_HUE) {
			pq_user_latch_flag &= ~PQ_USER_CMS_CURVE_HUE;
			input_clr_md = 0;
			for (i = 0; i < cm_color_md_max; i++) {
				if (cm_cur_work_color_md == cm_9_color)
					cm_clr_md.cm_9_color_md = input_clr_md;
				else
					cm_clr_md.cm_14_color_md = input_clr_md;

				input_clr_md++;
				if (cm2_hue_array[i][2] == 1) {
					cm2_curve_update_hue(cm_clr_md);
					cm2_hue_array[i][2] = 0;
				}
			}
		}
#endif
	} else if (pq_user_latch_flag & PQ_USER_CMS_SAT_HUE) {
		pq_user_latch_flag &= ~PQ_USER_CMS_SAT_HUE;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		sat_hue_val =
			aipq_saturation_hue_get_base_val() + sat_hue_offset_val;
		/* ai_pq switch on, */
		/* if saturation val(sat_hue_val  >> 16) > max val(511), */
		/*	saturation val can not add sat offset val. */
		if ((sat_hue_val  >> 16) > 511)
			sat_hue_val = aipq_saturation_hue_get_base_val();
#else
		sat_hue_val = sat_hue_offset_val;
#endif
		amvecm_set_saturation_hue(sat_hue_val, WR_DMA, vpp_index);
	}

	if (pq_user_latch_flag & PQ_USER_PQ_MODULE_CTL) {
		pq_user_latch_flag &= ~PQ_USER_PQ_MODULE_CTL;
		vpp_pq_ctrl_config(pq_cfg, WR_DMA, vpp_index);
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static const char *amvecm_pq_user_usage_str = {
	"Usage:\n"
	"echo blk_ext_en > /sys/class/amvecm/pq_user_set: blk ext en\n"
	"echo blk_ext_dis > /sys/class/amvecm/pq_user_set: blk ext dis\n"
	"echo blk_start val > /sys/class/amvecm/pq_user_set: start adj\n"
	"echo blk_slope val > /sys/class/amvecm/pq_user_set: slope adj\n"
	"echo sr0_pk_en > /sys/class/amvecm/pq_user_set: sr0 pk en\n"
	"echo sr0_pk_dis > /sys/class/amvecm/pq_user_set: sr0 pk dis\n"
	"echo sr1_pk_en > /sys/class/amvecm/pq_user_set: sr0 pk en\n"
	"echo sr1_pk_dis > /sys/class/amvecm/pq_user_set: sr0 pk dis\n"
	"echo sr0_dering_en > /sys/class/amvecm/pq_user_set: sr0 dr en\n"
	"echo sr0_dering_dis > /sys/class/amvecm/pq_user_set: sr0 dr dis\n"
	"echo sr1_dering_en > /sys/class/amvecm/pq_user_set: sr1 dr en\n"
	"echo sr1_dering_dis > /sys/class/amvecm/pq_user_set: sr1 dr dis\n"
	"echo sr0_dejaggy_en > /sys/class/amvecm/pq_user_set: sr0 dj en\n"
	"echo sr0_dejaggy_dis > /sys/class/amvecm/pq_user_set: sr0 dj dis\n"
	"echo sr1_dejaggy_en > /sys/class/amvecm/pq_user_set: sr1 dj en\n"
	"echo sr1_dejaggy_dis > /sys/class/amvecm/pq_user_set: sr1 dj dis\n"
	"echo sr0_direc_en > /sys/class/amvecm/pq_user_set: sr0 direc en\n"
	"echo sr0_direc_dis > /sys/class/amvecm/pq_user_set: sr0 direc dis\n"
	"echo sr1_direc_en > /sys/class/amvecm/pq_user_set: sr1 direc en\n"
	"echo sr1_direc_dis > /sys/class/amvecm/pq_user_set: sr1 direc dis\n"

};

static ssize_t amvecm_pq_user_show(const struct class *cla,
				   const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", amvecm_pq_user_usage_str);
}

static ssize_t amvecm_pq_user_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "blk_ext_en", 10)) {
		pq_user_latch_flag |= PQ_USER_BLK_EN;
	} else if (!strncmp(parm[0], "blk_ext_dis", 11)) {
		pq_user_latch_flag |= PQ_USER_BLK_DIS;
	} else if (!strncmp(parm[0], "blk_start", 9)) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		pq_user_value = val;
		pq_user_latch_flag |= PQ_USER_BLK_START;
	} else if (!strncmp(parm[0], "blk_slope", 9)) {
		if (kstrtoul(parm[1], 10, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		pq_user_value = val;
		pq_user_latch_flag |= PQ_USER_BLK_SLOPE;
	} else if (!strncmp(parm[0], "sr0_pk_en", 9)) {
		pq_user_latch_flag |= PQ_USER_SR0_PK_EN;
	} else if (!strncmp(parm[0], "sr0_pk_dis", 10)) {
		pq_user_latch_flag |= PQ_USER_SR0_PK_DIS;
	} else if (!strncmp(parm[0], "sr1_pk_en", 9)) {
		pq_user_latch_flag |= PQ_USER_SR1_PK_EN;
	} else if (!strncmp(parm[0], "sr1_pk_dis", 10)) {
		pq_user_latch_flag |= PQ_USER_SR1_PK_DIS;
	} else if (!strncmp(parm[0], "sr0_dering_en", 13)) {
		pq_user_latch_flag |= PQ_USER_SR0_DERING_EN;
	} else if (!strncmp(parm[0], "sr0_dering_dis", 14)) {
		pq_user_latch_flag |= PQ_USER_SR0_DERING_DIS;
	} else if (!strncmp(parm[0], "sr1_dering_en", 13)) {
		pq_user_latch_flag |= PQ_USER_SR1_DERING_EN;
	} else if (!strncmp(parm[0], "sr1_dering_dis", 14)) {
		pq_user_latch_flag |= PQ_USER_SR1_DERING_DIS;
	} else if (!strncmp(parm[0], "sr0_dejaggy_en", 14)) {
		pq_user_latch_flag |= PQ_USER_SR0_DEJAGGY_EN;
	} else if (!strncmp(parm[0], "sr0_dejaggy_dis", 15)) {
		pq_user_latch_flag |= PQ_USER_SR0_DEJAGGY_DIS;
	} else if (!strncmp(parm[0], "sr1_dejaggy_en", 14)) {
		pq_user_latch_flag |= PQ_USER_SR1_DEJAGGY_EN;
	} else if (!strncmp(parm[0], "sr1_dejaggy_dis", 15)) {
		pq_user_latch_flag |= PQ_USER_SR1_DEJAGGY_DIS;
	} else if (!strncmp(parm[0], "sr0_direc_en", 12)) {
		pq_user_latch_flag |= PQ_USER_SR0_DIRECTION_EN;
	} else if (!strncmp(parm[0], "sr0_direc_dis", 13)) {
		pq_user_latch_flag |= PQ_USER_SR0_DIRECTION_DIS;
	} else if (!strncmp(parm[0], "sr1_direc_en", 12)) {
		pq_user_latch_flag |= PQ_USER_SR1_DIRECTION_EN;
	} else if (!strncmp(parm[0], "sr1_direc_dis", 13)) {
		pq_user_latch_flag |= PQ_USER_SR1_DIRECTION_DIS;
	}

	kfree(buf_orig);
	return count;
}

static const char *dnlp_insmod_debug_usage_str = {
	"usage: echo 1 > /sys/class/amvecm/dnlp_insmod\n"
};

static ssize_t amvecm_dnlp_insmod_show(const struct class *cla,
				       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", dnlp_insmod_debug_usage_str);
}

static ssize_t amvecm_dnlp_insmod_store(const struct class *cla,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	size_t r;
	int val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1)
		return -EINVAL;

	if (val == 1)
		dnlp_alg_param_init();

	return count;
}

static ssize_t amvecm_vpp_demo_show(const struct class *cla,
				    const struct class_attribute *attr,
				    char *buf)
{
	pr_info("echo sr_demo val(0/1) > /sys/class/amvecm/vpp_demo\n");
	pr_info("echo cm_demo val(0/1) > /sys/class/amvecm/vpp_demo\n");
	pr_info("echo lc_demo val(0/1) > /sys/class/amvecm/vpp_demo\n");
	return 0;
}

static ssize_t amvecm_vpp_demo_store(const struct class *cla,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	struct demo_data_s *p = &demo_data;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "sr_demo", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;
		if (val)
			p->en_st[E_DEMO_SR] = E_ENABLE;
		else
			p->en_st[E_DEMO_SR] = E_DISABLE;
		p->update_flag[E_DEMO_SR] = 1;
		pr_amvecm_dbg("sr_demo = %d\n", p->en_st[E_DEMO_SR]);
	} else if (!strncmp(parm[0], "cm_demo", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;
		if (val)
			p->en_st[E_DEMO_CM] = E_ENABLE;
		else
			p->en_st[E_DEMO_CM] = E_DISABLE;
		p->update_flag[E_DEMO_CM] = 1;
		pr_amvecm_dbg("cm_demo = %d\n", p->en_st[E_DEMO_CM]);
	} else if (!strncmp(parm[0], "lc_demo", 7)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto kfree_buf;
		if (val)
			p->en_st[E_DEMO_LC] = E_ENABLE;
		else
			p->en_st[E_DEMO_LC] = E_DISABLE;
		p->update_flag[E_DEMO_LC] = 1;
		pr_amvecm_dbg("lc_demo = %d\n", p->en_st[E_DEMO_LC]);
	}

	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;

}
#endif

static void dump_vpp_size_info(void)
{
	unsigned int vpp_input_h, vpp_input_v,
		pps_input_length, pps_input_height,
		pps_output_hs, pps_output_he, pps_output_vs, pps_output_ve,
		vd1_preblend_hs, vd1_preblend_he,
		vd1_preblend_vs, vd1_preblend_ve,
		vd2_preblend_hs, vd2_preblend_he,
		vd2_preblend_vs, vd2_preblend_ve,
		prelend_input_hsize,
		vd1_postblend_hs, vd1_postblend_he,
		vd1_postblend_vs, vd1_postblend_ve,
		postblend_hsize,
		ve_hsize, ve_vsize, psr_hsize, psr_vsize,
		cm_hsize, cm_vsize;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct cm_port_s cm_port;
#endif

	vpp_input_h = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 16, 13);
	vpp_input_v = READ_VPP_REG_BITS(VPP_IN_H_V_SIZE, 0, 13);
	pps_input_length = READ_VPP_REG_BITS(VPP_LINE_IN_LENGTH, 0, 13);
	pps_input_height = READ_VPP_REG_BITS(VPP_PIC_IN_HEIGHT, 0, 13);
	pps_output_hs = READ_VPP_REG_BITS(VPP_HSC_REGION12_STARTP, 16, 13);
	pps_output_he = READ_VPP_REG_BITS(VPP_HSC_REGION4_ENDP, 0, 13);
	pps_output_vs = READ_VPP_REG_BITS(VPP_VSC_REGION12_STARTP, 16, 13);
	pps_output_ve = READ_VPP_REG_BITS(VPP_VSC_REGION4_ENDP, 0, 13);
	vd1_preblend_he = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_H_START_END,
					    0, 13);
	vd1_preblend_hs = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_H_START_END,
					    16, 13);
	vd1_preblend_ve = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_V_START_END,
					    0, 13);
	vd1_preblend_vs = READ_VPP_REG_BITS(VPP_PREBLEND_VD1_V_START_END,
					    16, 13);
	vd2_preblend_he = READ_VPP_REG_BITS(VPP_BLEND_VD2_H_START_END, 0, 13);
	vd2_preblend_hs = READ_VPP_REG_BITS(VPP_BLEND_VD2_H_START_END, 16, 13);
	vd2_preblend_ve = READ_VPP_REG_BITS(VPP_BLEND_VD2_V_START_END, 0, 13);
	vd2_preblend_vs = READ_VPP_REG_BITS(VPP_BLEND_VD2_V_START_END, 16, 13);
	prelend_input_hsize = READ_VPP_REG_BITS(VPP_PREBLEND_H_SIZE, 0, 13);
	vd1_postblend_he = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END,
					     0, 13);
	vd1_postblend_hs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_H_START_END,
					     16, 13);
	vd1_postblend_ve = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END,
					     0, 13);
	vd1_postblend_vs = READ_VPP_REG_BITS(VPP_POSTBLEND_VD1_V_START_END,
					     16, 13);
	postblend_hsize = READ_VPP_REG_BITS(VPP_POSTBLEND_H_SIZE, 0, 13);
	ve_hsize = READ_VPP_REG_BITS(VPP_VE_H_V_SIZE, 16, 13);
	ve_vsize = READ_VPP_REG_BITS(VPP_VE_H_V_SIZE, 0, 13);
	psr_hsize = READ_VPP_REG_BITS(VPP_PSR_H_V_SIZE, 16, 13);
	psr_vsize = READ_VPP_REG_BITS(VPP_PSR_H_V_SIZE, 0, 13);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		cm_port = get_cm_port();
		WRITE_VPP_REG(cm_port.cm_addr_port[0], 0x205);
		cm_hsize = READ_VPP_REG(cm_port.cm_data_port[0]);
	} else
#endif
	{
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x205);
		cm_hsize = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
	}

	cm_vsize = (cm_hsize >> 16) & 0xffff;
	cm_hsize = cm_hsize & 0xffff;

	pr_info("\n vpp size info:\n");
	pr_info("vpp_input_h:%d, vpp_input_v:%d\n"
		"pps_input_length:%d, pps_input_height:%d\n"
		"pps_output_hs:%d, pps_output_he:%d\n"
		"pps_output_vs:%d, pps_output_ve:%d\n"
		"vd1_preblend_hs:%d, vd1_preblend_he:%d\n"
		"vd1_preblend_vs:%d, vd1_preblend_ve:%d\n"
		"vd2_preblend_hs:%d, vd2_preblend_he:%d\n"
		"vd2_preblend_vs:%d, vd2_preblend_ve:%d\n"
		"prelend_input_hsize:%d\n"
		"vd1_postblend_hs:%d, vd1_postblend_he:%d\n"
		"vd1_postblend_vs:%d, vd1_postblend_ve:%d\n"
		"postblend_hsize:%d\n"
		"ve_hsize:%d, ve_vsize:%d\n"
		"psr_hsize:%d, psr_vsize:%d\n"
		"cm_hsize:%d, cm_vsize:%d\n",
		vpp_input_h, vpp_input_v,
		pps_input_length, pps_input_height,
		pps_output_hs, pps_output_he,
		pps_output_vs, pps_output_ve,
		vd1_preblend_hs, vd1_preblend_he,
		vd1_preblend_vs, vd1_preblend_ve,
		vd2_preblend_hs, vd2_preblend_he,
		vd2_preblend_vs, vd2_preblend_ve,
		prelend_input_hsize,
		vd1_postblend_hs, vd1_postblend_he,
		vd1_postblend_vs, vd1_postblend_ve,
		postblend_hsize,
		ve_hsize, ve_vsize,
		psr_hsize, psr_vsize,
		cm_hsize, cm_vsize);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void amvecm_sharpness_enable(int sel)
{
	if (chip_type_id == chip_t3x) {
		ve_sharpness_enable(sel);
		return;
	}

	/*0:peaking enable   1:peaking disable*/
	/*2:lti/cti enable   3:lti/cti disable*/
	switch (sel) {
	case 0:
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   1, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   1, 1, 1);
		break;
	case 1:
		WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
				   0, 1, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
				   0, 1, 1);
		break;
	case 2:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP,
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP,
				   1, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC,
				   1, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP,
				   1, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP,
				   1, 14, 1);
		break;
	case 3:
		WRITE_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_HLTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VLTI_FLT_CON_CLP,
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_VCTI_FLT_CON_CLP,
				   0, 14, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_HCTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_HLTI_FLT_CLP_DC,
				   0, 28, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VLTI_FLT_CON_CLP,
				   0, 14, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_VCTI_FLT_CON_CLP,
				   0, 14, 1);
		break;
	/*sr4 drtlpf theta en*/
	case 4:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   7, 4, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   7, 3, 3);
		break;
	case 5:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   0, 4, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   0, 3, 3);
		break;
	/*sr4 debanding en*/
	case 6:
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   1, 23, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   1, 23, 1);
		break;
	case 7:
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL,
				   0, 23, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 4, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 5, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 22, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DB_FLT_CTRL,
				   0, 23, 1);
		break;
	/*sr3 dejaggy en*/
	case 8:
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   1, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   1, 0, 1);
		break;
	case 9:
		WRITE_VPP_REG_BITS(SRSHARP0_DEJ_CTRL,
				   0, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_DEJ_CTRL,
				   0, 0, 1);
		break;
	/*sr3 dering en*/
	case 10:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   1, 28, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   1, 28, 3);
		break;
	case 11:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL,
				   0, 28, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DERING_CTRL,
				   0, 28, 3);
		break;
	/*sr3 direction lpf en*/
	case 12:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   7, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   7, 0, 3);
		break;
	case 13:
		WRITE_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN,
				   0, 0, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR3_DRTLPF_EN,
				   0, 0, 3);
		break;

	default:
		break;
	}
}
#endif

void amvecm_clip_range_limit(bool limit_en)
{
	/*fix mbox av out flicker black dot*/
	if (limit_en) {
		/*cvbs output 16-235 16-240 16-240*/
		WRITE_VPP_REG(VPP_CLIP_MISC0, 0x3acf03c0);
		WRITE_VPP_REG(VPP_CLIP_MISC1, 0x4010040);
	} else {
		/*retore for other mode*/
		WRITE_VPP_REG(VPP_CLIP_MISC0, 0x3fffffff);
		WRITE_VPP_REG(VPP_CLIP_MISC1, 0x0);
	}
}
EXPORT_SYMBOL(amvecm_clip_range_limit);

static void amvecm_pq_enable(int enable)
{
	if (enable) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		lc_en = 1;

		if (chip_type_id == chip_t3x) {
			ve_vadj_ctl(WR_VCB, VE_VADJ1, 1, 0);
			ve_ble_ctl(WR_VCB, 1, 0);
			ve_cc_ctl(WR_VCB, 1, 0);
		} else {
			/* black_ext_en */
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 3, 1);
			/* chroma_coring */
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 4, 1);

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
			else
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
		}
		ve_enable_dnlp();
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!is_amdv_enable())
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			amcm_enable(WR_VCB, 0);

		if (chip_type_id >= chip_s7d) {
			amve_sharpness_sub_ctrl(0, enable);
			amve_sharpness_sub_ctrl(1, enable);
			amve_sharpness_sub_ctrl(2, enable);
			amve_sharpness_sub_ctrl(3, enable);
			amve_sharpness_sub_ctrl(6, enable);
		} else {
			amvecm_sharpness_enable(0);
			amvecm_sharpness_enable(2);

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
				amvecm_sharpness_enable(8);
				amvecm_sharpness_enable(10);
				amvecm_sharpness_enable(12);
			}

			/*sr4 drtlpf theta/ debanding en*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_meson_txlx_cpu() || is_meson_txhd_cpu()) {
				amvecm_sharpness_enable(4);
				amvecm_sharpness_enable(6);
			}
#endif
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
						   0x3f, 0, 6);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
						   0x7, 8, 3);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKDRT_BLD_EN,
						   1, 0, 1);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
						   3, 2, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
						   3, 12, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_XTI_SDFDEN,
						   3, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_TI_BPF_EN,
						   0xf, 0, 4);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKLONG_PF_EN,
						   3, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_CC_PK_ADJ,
						   1, 24, 1);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_GRAPHIC_CTRL,
						   1, 10, 1);

				WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
						   0x3f, 0, 6);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
						   0x7, 8, 3);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKDRT_BLD_EN,
						   1, 0, 1);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
						   3, 2, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
						   3, 12, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_XTI_SDFDEN,
						   3, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_TI_BPF_EN,
						   0xf, 0, 4);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKLONG_PF_EN,
						   3, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_CC_PK_ADJ,
						   1, 24, 1);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_GRAPHIC_CTRL,
						   1, 10, 1);
			}
		}
		gamut_mapping_wrapper_crtl(0, 1);
		gamut_mapping_wrapper_crtl(1, 1);
#endif

		white_balance_adjust(0, 1);

		vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		pq_bypass_debug_flag = 0;
	} else {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		lc_en = 0;

		if (chip_type_id == chip_t3x) {
			ve_vadj_ctl(WR_VCB, VE_VADJ1, 0, 0);
			ve_ble_ctl(WR_VCB, 0, 0);
			ve_cc_ctl(WR_VCB, 0, 0);
		} else {
			/* black_ext_en */
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 3, 1);
			/* chroma_coring */
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 4, 1);

			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 0, 0, 1);
			else
				WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 0, 1);
		}

		ve_disable_dnlp();

		amcm_disable(WR_VCB, 0);

		if (chip_type_id >= chip_s7d) {
			amve_sharpness_sub_ctrl(0, enable);
			amve_sharpness_sub_ctrl(1, enable);
			amve_sharpness_sub_ctrl(2, enable);
			amve_sharpness_sub_ctrl(3, enable);
			amve_sharpness_sub_ctrl(6, enable);
		} else {
			amvecm_sharpness_enable(1);
			amvecm_sharpness_enable(3);

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
				amvecm_sharpness_enable(9);
				amvecm_sharpness_enable(11);
				amvecm_sharpness_enable(13);
			}
			/*sr4 drtlpf theta/ debanding en*/
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
			if (is_meson_txlx_cpu()) {
				amvecm_sharpness_enable(5);
				amvecm_sharpness_enable(7);
			}
#endif

			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
						   0, 0, 6);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN,
						   0, 8, 3);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKDRT_BLD_EN,
						   0, 0, 1);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
						   0, 2, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT,
						   0, 12, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_XTI_SDFDEN,
						   0, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_TI_BPF_EN,
						   0, 0, 4);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKLONG_PF_EN,
						   0, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_CC_PK_ADJ,
						   0, 24, 1);
				WRITE_VPP_REG_BITS(SRSHARP0_SR7_GRAPHIC_CTRL,
						   0, 10, 1);

				WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
						   0, 0, 6);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN,
						   0, 8, 3);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKDRT_BLD_EN,
						   0, 0, 1);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
						   0, 2, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT,
						   0, 12, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_XTI_SDFDEN,
						   0, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_TI_BPF_EN,
						   0, 0, 4);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKLONG_PF_EN,
						   0, 0, 2);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_CC_PK_ADJ,
						   0, 24, 1);
				WRITE_VPP_REG_BITS(SRSHARP1_SR7_GRAPHIC_CTRL,
						   0, 10, 1);
			}
		}
		gamut_mapping_wrapper_crtl(0, 0);
		gamut_mapping_wrapper_crtl(1, 0);
#endif

		white_balance_adjust(0, 0);

		vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
		pq_bypass_debug_flag = 1;
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void amvecm_dither_enable(int enable)
{
	switch (enable) {
		/*dither enable*/
	case 0:/*disable*/
		WRITE_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 0, 0, 1);
		break;
	case 1:/*enable*/
		WRITE_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 1, 0, 1);
		break;
		/*dither round enable*/
	case 2:/*disable*/
		WRITE_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 0, 1, 1);
		break;
	case 3:/*enable*/
		WRITE_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 1, 1, 1);
		break;
	default:
		break;
	}
}

static void amvecm_vpp_mtx_debug(int mtx_sel, int coef_sel)
{
	if (mtx_sel & (1 << VPP_MATRIX_1)) {
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 5, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 8, 3);
		mtx_sel_dbg &= ~(1 << VPP_MATRIX_1);
	} else if (mtx_sel & (1 << VPP_MATRIX_2)) {
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 0, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 8, 3);
		mtx_sel_dbg &= ~(1 << VPP_MATRIX_2);
	} else if (mtx_sel & (1 << VPP_MATRIX_3)) {
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 6, 1);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 3);
		mtx_sel_dbg &= ~(1 << VPP_MATRIX_3);
	}
	/*coef_sel 1: 10bit yuvl2rgb   2:rgb2yuvl*/
	/*coef_sel 3: 12bit yuvl2rgb   4:rgb2yuvl*/
	if (coef_sel == 1) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04A80000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x072C04A8);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1F261DDD);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x04A80876);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0xfc00e00);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0e00);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (coef_sel == 2) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x00bb0275);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x003f1f99);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1ea601c2);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x01c21e67);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x00001fd7);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x00400200);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x00000200);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (coef_sel == 3) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x04A80000);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x072C04A8);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1F261DDD);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x04A80876);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x8000800);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x800);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x7000000);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0000);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	} else if (coef_sel == 4) {
		WRITE_VPP_REG(VPP_MATRIX_COEF00_01, 0x00bb0275);
		WRITE_VPP_REG(VPP_MATRIX_COEF02_10, 0x003f1f99);
		WRITE_VPP_REG(VPP_MATRIX_COEF11_12, 0x1ea601c2);
		WRITE_VPP_REG(VPP_MATRIX_COEF20_21, 0x01c21e67);
		WRITE_VPP_REG(VPP_MATRIX_COEF22, 0x00001fd7);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, 0x01000000);
		WRITE_VPP_REG(VPP_MATRIX_OFFSET2, 0x00000000);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, 0x0);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, 0x0);
		WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP, 0, 5, 3);
	}
}

static void vpp_clip_config(unsigned int mode_sel, unsigned int color,
			    unsigned int color_mode)
{
	unsigned int addr_cliptop, addr_clipbot, value_cliptop, value_clipbot;

	if (mode_sel == 0) {/*vd1*/
		addr_cliptop = VPP_VD1_CLIP_MISC0;
		addr_clipbot = VPP_VD1_CLIP_MISC1;
	} else if (mode_sel == 1) {/*vd2*/
		addr_cliptop = VPP_VD2_CLIP_MISC0;
		addr_clipbot = VPP_VD2_CLIP_MISC1;
	} else if (mode_sel == 2) {/*xvycc*/
		addr_cliptop = VPP_XVYCC_MISC0;
		addr_clipbot = VPP_XVYCC_MISC1;
	} else if (mode_sel == 3) {/*final clip*/
		addr_cliptop = VPP_CLIP_MISC0;
		addr_clipbot = VPP_CLIP_MISC1;
	} else {
		addr_cliptop = mode_sel;
		addr_clipbot = mode_sel + 1;
	}
	if (color == 0) {/*default*/
		value_cliptop = 0x3fffffff;
		value_clipbot = 0x0;
	} else if (color == 1) {/*Blue*/
		if (color_mode == 0) {/*yuv*/
			value_cliptop = (0x29 << 22) | (0xf0 << 12) |
				(0x6e << 2);
			value_clipbot = (0x29 << 22) | (0xf0 << 12) |
				(0x6e << 2);
		} else {/*RGB*/
			value_cliptop = 0xFF << 2;
			value_clipbot = 0xFF << 2;
		}
	} else if (color == 2) {/*Black*/
		if (color_mode == 0) {/*yuv*/
			value_cliptop = (0x10 << 22) | (0x80 << 12) |
				(0x80 << 2);
			value_clipbot = (0x10 << 22) | (0x80 << 12) |
				(0x80 << 2);
		} else {
			value_cliptop = 0;
			value_clipbot = 0;
		}
	} else {
		value_cliptop = color;
		value_clipbot = color;
	}
	WRITE_VPP_REG(addr_cliptop, value_cliptop);
	WRITE_VPP_REG(addr_clipbot, value_clipbot);
}

#define MAX_CLIP_VAL ((1 << 30) - 1)
static ssize_t amvecm_clamp_color_top_show(const struct class *cla,
					   const struct class_attribute *attr,
					   char *buf)
{
	return sprintf(buf, "0x%08x\n", READ_VPP_REG(VPP_CLIP_MISC0));
}

static ssize_t amvecm_clamp_color_top_store(const struct class *cla,
					    const struct class_attribute *attr,
					    const char *buf, size_t count)
{
	size_t r;
	u32 val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1 || val > MAX_CLIP_VAL)
		return -EINVAL;

	WRITE_VPP_REG(VPP_CLIP_MISC0, val);
	return count;
}

static ssize_t amvecm_clamp_color_bottom_show(const struct class *cla,
					      const struct class_attribute *attr,
					      char *buf)
{
	return sprintf(buf, "0x%08x\n", READ_VPP_REG(VPP_CLIP_MISC1));
}

static ssize_t amvecm_clamp_color_bottom_store(const struct class *cla,
					       const struct class_attribute *attr,
					       const char *buf, size_t count)
{
	size_t r;
	u32 val;

	r = sscanf(buf, "%x\n", &val);
	if (r != 1 || val > MAX_CLIP_VAL)
		return -EINVAL;

	WRITE_VPP_REG(VPP_CLIP_MISC1, val);
	return count;
}
#endif

static ssize_t amvecm_cpu_ver_show(const struct class *cla,
				   const struct class_attribute *attr, char *buf)
{
	pr_info("echo r cpu_ver > /sys/class/amvecm/cpu_ver");
	return 0;
}

static ssize_t amvecm_cpu_ver_store(const struct class *cla,
				    const struct class_attribute *attr,
			const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strncmp(parm[0], "r", 1)) {
		if (!strncmp(parm[1], "cpu_ver", 7)) {
			if (!is_meson_tm2_cpu()) {
				pr_info("VER_NULL\n");
				kfree(buf_orig);
				return count;
			}
			if (is_meson_rev_a())
				pr_info("VER_A\n");
			else if (is_meson_rev_b())
				pr_info("VER_B\n");
			else
				pr_info("no ver\n");
		} else {
			pr_info("error cmd\n");
		}
	} else {
		pr_info("error cmd\n");
	}

	kfree(buf_orig);
	return count;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static ssize_t amvecm_cm2_hue_show(const struct class *cla,
				   const struct class_attribute *attr, char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_hue_array[i][0], cm2_hue_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_hue_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_hue", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;

			cm_cur_work_color_md = cm_color_md_dbg.color_type;
		}
		//parm[2]: color modes
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_array[color_mode][1] = val;
			cm2_hue_array[color_mode][2] = 1;
		}

		cm2_hue(cm_color_md_dbg, cm2_hue_array[color_mode][0],
			cm2_hue_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE;
		pr_info("cm2_hue ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t amvecm_cm2_luma_show(const struct class *cla,
				    const struct class_attribute *attr,
				    char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_luma_array[i][0], cm2_luma_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_luma_store(const struct class *cla,
				     const struct class_attribute *attr,
				     const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_luma", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;
			cm_cur_work_color_md = cm_color_md_dbg.color_type;
		}
		//parm[2]: color mode
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_luma_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_luma_array[color_mode][1] = val;
			cm2_luma_array[color_mode][2] = 1;
		}

		cm2_luma(cm_color_md_dbg, cm2_luma_array[color_mode][0],
			 cm2_luma_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_LUMA;
		pr_info("cm2_luma ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t amvecm_cm2_sat_show(const struct class *cla,
				   const struct class_attribute *attr,
				   char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_sat_array[i][0], cm2_sat_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_sat_store(const struct class *cla,
				    const struct class_attribute *attr,
				    const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_sat", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;
			cm_cur_work_color_md = cm_color_md_dbg.color_type;
		}
		//parm[2]: color mode
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_sat_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_sat_array[color_mode][1] = val;
			cm2_sat_array[color_mode][2] = 1;
		}

		cm2_sat(cm_color_md_dbg, cm2_sat_array[color_mode][0],
			cm2_sat_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_SAT;
		pr_info("cm2_sat ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static ssize_t amvecm_cm2_hue_by_hs_show(const struct class *cla,
					 const struct class_attribute *attr,
					 char *buf)
{
	int i;
	int pos = 0;
	int cm_color_md_max = cm_14_ecm2colormode_max;

	if (cm_cur_work_color_md == cm_9_color)
		cm_color_md_max = ecm2colormode_max;

	for (i = 0; i < cm_color_md_max; i++)
		pos += sprintf(buf + pos, "%d %d %d\n", i,
			cm2_hue_by_hs_array[i][0], cm2_hue_by_hs_array[i][1]);
	return pos;
}

static ssize_t amvecm_cm2_hue_by_hs_store(const struct class *cla,
					  const struct class_attribute *attr,
					  const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int color_mode;
	struct cm_color_md cm_color_md_dbg;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "cm2_hue_by_hs", 7)) {
		//parm[1]: 0: 9-color; 1: 14-color
		{
			if (!strncmp(parm[1], "0", 1))
				cm_color_md_dbg.color_type = cm_9_color;
			else if (!strncmp(parm[1], "1", 1))
				cm_color_md_dbg.color_type = cm_14_color;
			else
				goto kfree_buf;
			cm_cur_work_color_md = cm_color_md_dbg.color_type;
		}
		//parm[2]: color mode
		{
			if (kstrtoul(parm[2], 10, &val) < 0)
				goto kfree_buf;
			color_mode = val;
			if (cm_color_md_dbg.color_type == cm_9_color) {
				cm_color_md_dbg.cm_9_color_md = val;
				cm_color_md_dbg.cm_14_color_md =
					cm_14_ecm2colormode_max;
			} else {
				cm_color_md_dbg.cm_9_color_md = ecm2colormode_max;
				cm_color_md_dbg.cm_14_color_md = val;
			}
		}
		//parm[3]: color value
		{
			if (kstrtol(parm[3], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_by_hs_array[color_mode][0] = val;
			cm_color_md_dbg.color_value = val;
		}
		//parm[4]: lpf flag
		{
			if (kstrtoul(parm[4], 10, &val) < 0)
				goto kfree_buf;
			cm2_hue_by_hs_array[color_mode][1] = val;
			cm2_hue_by_hs_array[color_mode][2] = 1;
		}

		cm2_hue_by_hs(cm_color_md_dbg,
				cm2_hue_by_hs_array[color_mode][0],
			      cm2_hue_by_hs_array[color_mode][1]);
		pq_user_latch_flag |= PQ_USER_CMS_CURVE_HUE_HS;
		pr_info("cm2_hue_by_hs ok\n");
	}
	kfree(buf_orig);
	return count;

kfree_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static void cm_hist_config(unsigned int en, unsigned int mode,
	unsigned int wd0, unsigned int wd1,
	unsigned int wd2, unsigned int wd3)
{
	unsigned int value;
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;
	unsigned int data_port = VPP_CHROMA_DATA_PORT;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct cm_port_s port;
	int slice_max = 0;
	int i;

	if (chip_type_id == chip_t3x) {
		slice_max = get_slice_max();
		port = get_cm_port();
		for (i = SLICE0; i < slice_max; i++) {
			addr_port = port.cm_addr_port[i];
			data_port = port.cm_data_port[i];

			WRITE_VPP_REG(addr_port, STA_CFG_REG);
			value = READ_VPP_REG(data_port);
			value = (value & (~0xc0000000)) | (en << 30);
			WRITE_VPP_REG(addr_port, STA_CFG_REG);
			WRITE_VPP_REG(data_port, value);

			WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
			value = READ_VPP_REG(data_port);
			value = (value & (~(0x1fff0000))) | (mode << 16);
			WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
			WRITE_VPP_REG(data_port, value);

			WRITE_VPP_REG(addr_port, STA_WIN_XYXY0_REG);
			WRITE_VPP_REG(data_port, wd0 | (wd1 << 16));

			WRITE_VPP_REG(addr_port, STA_WIN_XYXY1_REG);
			WRITE_VPP_REG(data_port, wd2 | (wd3 << 16));
		}
	} else
#endif
	{
		WRITE_VPP_REG(addr_port, STA_CFG_REG);
		value = READ_VPP_REG(data_port);
		value = (value & (~0xc0000000)) | (en << 30);
		WRITE_VPP_REG(addr_port, STA_CFG_REG);
		WRITE_VPP_REG(data_port, value);

		WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
		value = READ_VPP_REG(data_port);
		value = (value & (~(0x1fff0000))) | (mode << 16);
		WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
		WRITE_VPP_REG(data_port, value);

		WRITE_VPP_REG(addr_port, STA_WIN_XYXY0_REG);
		WRITE_VPP_REG(data_port, wd0 | (wd1 << 16));

		WRITE_VPP_REG(addr_port, STA_WIN_XYXY1_REG);
		WRITE_VPP_REG(data_port, wd2 | (wd3 << 16));
	}
}

static void cm_sta_hist_range_thrd(int r, int ro_frame,
	int thrd0, int thrd1)
{
	unsigned int value0, value1;
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;
	unsigned int data_port = VPP_CHROMA_DATA_PORT;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct cm_port_s port;
	int slice_max = 0;
	int i;

	if (chip_type_id == chip_t3x) {
		slice_max = get_slice_max();
		port = get_cm_port();
		for (i = SLICE0; i < slice_max; i++) {
			addr_port = port.cm_addr_port[i];
			data_port = port.cm_data_port[i];

			if (r) {
				WRITE_VPP_REG(addr_port, STA_SAT_HIST0_REG);
				value0 = READ_VPP_REG(data_port);
				WRITE_VPP_REG(addr_port, STA_SAT_HIST1_REG);
				value1 = READ_VPP_REG(data_port);
				pr_info("HIST0_REG = 0x%x, HIST1_REG = 0x%x\n", value0, value1);
			} else {
				WRITE_VPP_REG(addr_port, STA_SAT_HIST0_REG);
				WRITE_VPP_REG(data_port, thrd0 | (ro_frame << 24));

				WRITE_VPP_REG(addr_port, STA_SAT_HIST1_REG);
				WRITE_VPP_REG(data_port, thrd1);
			}
		}
	} else
#endif
	{
		if (r) {
			WRITE_VPP_REG(addr_port, STA_SAT_HIST0_REG);
			value0 = READ_VPP_REG(data_port);
			WRITE_VPP_REG(addr_port, STA_SAT_HIST1_REG);
			value1 = READ_VPP_REG(data_port);
			pr_info("HIST0_REG = 0x%x, HIST1_REG = 0x%x\n", value0, value1);
		} else {
			WRITE_VPP_REG(addr_port, STA_SAT_HIST0_REG);
			WRITE_VPP_REG(data_port, thrd0 | (ro_frame << 24));

			WRITE_VPP_REG(addr_port, STA_SAT_HIST1_REG);
			WRITE_VPP_REG(data_port, thrd1);
		}
	}
}
#endif

static void cm_luma_bri_con(int r, int brightness, int contrast,
	int blk_lel)
{
	int value;
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;
	unsigned int data_port = VPP_CHROMA_DATA_PORT;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct cm_port_s port;
	int slice_max = 0;
	int i;

	if (chip_type_id == chip_t3x) {
		slice_max = get_slice_max();
		port = get_cm_port();
		for (i = SLICE0; i < slice_max; i++) {
			addr_port = port.cm_addr_port[i];
			data_port = port.cm_data_port[i];

			if (r) {
				WRITE_VPP_REG(addr_port, LUMA_ADJ0_REG);
				value = READ_VPP_REG(data_port);
				pr_info("contrast = 0x%x, blklel = 0x%x\n",
					value & 0xfff, (value >> 12) & 0x3ff);
				WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
				value = READ_VPP_REG(data_port);
				pr_info("bright = 0x%x, hist_mode = 0x%x\n",
					value & 0x1fff, (value >> 16) & 0x1fff);
			} else {
				WRITE_VPP_REG(addr_port, LUMA_ADJ0_REG);
				WRITE_VPP_REG(data_port,
					(blk_lel << 12) | contrast);

				WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
				value = READ_VPP_REG(data_port);
				WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
				WRITE_VPP_REG(data_port,
					(value & (~(0x1fff))) | brightness);
			}
		}
	} else
#endif
	{
		if (r) {
			WRITE_VPP_REG(addr_port, LUMA_ADJ0_REG);
			value = READ_VPP_REG(data_port);
			pr_info("contrast = 0x%x, blklel = 0x%x\n",
				value & 0xfff, (value >> 12) & 0x3ff);
			WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
			value = READ_VPP_REG(data_port);
			pr_info("bright = 0x%x, hist_mode = 0x%x\n",
				value & 0x1fff, (value >> 16) & 0x1fff);
		} else {
			WRITE_VPP_REG(addr_port, LUMA_ADJ0_REG);
			WRITE_VPP_REG(data_port,
				(blk_lel << 12) | contrast);

			WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
			value = READ_VPP_REG(data_port);
			WRITE_VPP_REG(addr_port, LUMA_ADJ1_REG);
			WRITE_VPP_REG(data_port,
				(value & (~(0x1fff))) | brightness);
		}
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void pr_cm_hist(enum cm_hist_e hist_sel)
{
	unsigned int *hist;
	int i;
	int addr_port;
	int data_port;

	if (chip_type_id == chip_s5) {
		pr_info("not support\n");
		return;
	}

	addr_port = VPP_CHROMA_ADDR_PORT;
	data_port = VPP_CHROMA_DATA_PORT;

	hist = kmalloc(32 * sizeof(unsigned int), GFP_KERNEL);
	if (!hist)
		return;
	memset(hist, 0, 32 * sizeof(unsigned int));

	switch (hist_sel) {
	case CM_HUE_HIST:
		if (chip_type_id != chip_t3x) {
			for (i = 0; i < 32; i++) {
				WRITE_VPP_REG(addr_port,
					RO_CM_HUE_HIST_BIN0 + i);
				hist[i] = READ_VPP_REG(data_port);
				pr_info("hue_hist[%d] = 0x%8x\n", i, hist[i]);
			}
			if (chip_type_id == chip_t6x && (flag_cm_lc_dma_en & 0x1)) {
				for (i = 0; i < 32; i++) {
					pr_info("dma hue_hist[%d] = 0x%8x\n", i,
						vpp_hist_param.vpp_hue_gamma[i]);
				}
			}

	#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		} else {
			cm_hist_by_type_get(hist_sel, hist, 32,
				RO_CM_HUE_HIST_BIN0);
			for (i = 0; i < 32; i++)
				pr_info("hue_hist[%d] = 0x%8x\n", i, hist[i]);
	#endif
		}
		break;
	case CM_SAT_HIST:
		if (chip_type_id != chip_t3x) {
			for (i = 0; i < 32; i++) {
				WRITE_VPP_REG(addr_port,
					RO_CM_SAT_HIST_BIN0 + i);
				hist[i] = READ_VPP_REG(data_port);
				pr_info("sat_hist[%d] = 0x%8x\n", i, hist[i]);
			}
			if (chip_type_id == chip_t6x && (flag_cm_lc_dma_en & 0x1)) {
				for (i = 0; i < 32; i++) {
					pr_info("dma sat_hist[%d] = 0x%8x\n", i,
						vpp_hist_param.vpp_sat_gamma[i]);
				}
			}
	#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		} else {
			cm_hist_by_type_get(hist_sel, hist, 32,
				RO_CM_SAT_HIST_BIN0);
			for (i = 0; i < 32; i++)
				pr_info("sat_hist[%d] = 0x%8x\n", i, hist[i]);
	#endif
		}
		break;
	default:
		break;
	}
	kfree(hist);
}

static void cm_init_config(int bitdepth)
{
	int i, j, reg;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int m;
	struct cm_port_s cm_port;
	unsigned int addr_port;
	unsigned int data_port;
	int slice_max;
#endif

	if (chip_type_id == chip_s7 || chip_type_id == chip_s7d)
		return;

	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
		am_set_regmap(&cm_default, 0);
	else
		am_set_regmap(&cm_default_legacy, 0);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		slice_max = get_slice_max();
		for (m = SLICE0; m < slice_max; m++) {
			cm_port = get_cm_port();
			addr_port = cm_port.cm_addr_port[m];
			data_port = cm_port.cm_data_port[m];

			if (bitdepth == 12) {
				WRITE_VPP_REG(addr_port, XVYCC_YSCP_REG);
				WRITE_VPP_REG(data_port, 0xfff0000);
				WRITE_VPP_REG(addr_port, XVYCC_USCP_REG);
				WRITE_VPP_REG(data_port, 0xfff0000);
				WRITE_VPP_REG(addr_port, XVYCC_VSCP_REG);
				WRITE_VPP_REG(data_port, 0xfff0000);
				WRITE_VPP_REG(addr_port, LUMA_ADJ0_REG);
				WRITE_VPP_REG(data_port, 0x100400);
			} else {
				WRITE_VPP_REG(addr_port, XVYCC_YSCP_REG);
				WRITE_VPP_REG(data_port, 0x3ff0000);
				WRITE_VPP_REG(addr_port, XVYCC_USCP_REG);
				WRITE_VPP_REG(data_port, 0x3ff0000);
				WRITE_VPP_REG(addr_port, XVYCC_VSCP_REG);
				WRITE_VPP_REG(data_port, 0x3ff0000);
				WRITE_VPP_REG(addr_port, LUMA_ADJ0_REG);
				WRITE_VPP_REG(data_port, 0x40400);
			}

			for (i = 0; i < 32; i++) {
				for (j = 0; j < 5; j++) {
					reg = CM2_ENH_COEF0_H00 + i * 8 + j;
					WRITE_VPP_REG(addr_port, reg);
					WRITE_VPP_REG(data_port, 0x0);
				}
			}
		}
	} else {
#endif
		if (bitdepth == 10) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x40400);
		} else if (bitdepth == 12) {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0xfff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x100400);
		} else {
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_YSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_USCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, XVYCC_VSCP_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x3ff0000);
			WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, LUMA_ADJ0_REG);
			WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x40400);
		}

		for (i = 0; i < 32; i++) {
			for (j = 0; j < 5; j++) {
				reg = CM2_ENH_COEF0_H00 + i * 8 + j;
				WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, reg);
				WRITE_VPP_REG(VPP_CHROMA_DATA_PORT, 0x0);
			}
		}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	}
#endif
}

static void dnlp_init_config(void)
{
	memcpy(&dnlp_curve_param_load, &dnlp_default,
		sizeof(struct ve_dnlp_curve_param_s));
	ve_new_dnlp_param_update();
}

static void sr_init_config(void)
{
	switch (chip_type_id) {
	case chip_txhd2:
		am_set_regmap(&sr0_default, 0);
	break;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case chip_t3x:
		am_set_regmap(&sr0_default, 0);
		am_set_regmap(&sr1_default, 0);
		am_set_regmap(&s1_sr0_default, 0);
		am_set_regmap(&s1_sr1_default, 0);
	break;
#endif
	case chip_s7d:
	case chip_s6:
	case chip_t6d:
	case chip_t6w:
	case chip_t6x:
	case chip_a9:
		s7d_sharpness_init();
	break;
	default:
		am_set_regmap(&sr0_default, 0);
		am_set_regmap(&sr1_default, 0);
	break;
	}
}

void vlock_clk_config(struct amvecm_dev_s *devp, struct device *dev)
{
	if (!vlock_en)
		return;

	if (chip_type_id == chip_s5 ||
		chip_cls_id == AD_CHIP ||
		chip_cls_id == STB_CHIP)
		return;

	vlock_reg_config(dev);
	/*need set clock tree */
	devp->vlock_clk = devm_clk_get(dev, "cts_vid_lock_clk");
	if (!IS_ERR(devp->vlock_clk)) {
		clk_set_rate(devp->vlock_clk, 24000000);
		if (clk_prepare_enable(devp->vlock_clk) < 0)
			pr_info("vlock clk enable fail\n");
		/*clk_frq = clk_get_rate(clk);*/
		/*pr_info("cts_vid_lock_clk:%d\n", clk_frq);*/
		hw_clk_ok = 1;
	} else {
		pr_err("vlock clk not cfg\n");
		hw_clk_ok = 0;
	}
}

void vlock_clk_suspend(void)
{
	clk_disable_unprepare(amvecm_dev.vlock_clk);
}

void vlock_clk_resume(void)
{
	clk_prepare_enable(amvecm_dev.vlock_clk);
}

void suspend_drv_status_set(bool status)
{
	suspend_drv_flag = status;
}

bool suspend_drv_status_get(void)
{
	return suspend_drv_flag;
}

void suspend_matrix(void)
{
	int i;
	unsigned int reg;
	unsigned int length;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12B)
		return;

	if (chip_type_id == chip_s7d ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		length = RECOVERY_REG_MTX_MAX * 4;
	else if (chip_type_id == chip_t7)
		length = RECOVERY_REG_MTX_MAX * 2 + 1;
	else if (chip_cls_id == TV_CHIP)
		length = RECOVERY_REG_MTX_MAX * 2;
	else
		length = RECOVERY_REG_MTX_MAX;

	reg_matrix_list = kcalloc(length, sizeof(struct am_reg_s), GFP_KERNEL);
	if (!reg_matrix_list)
		return;

	if (chip_type_id == chip_c5) {
		reg = VOUT_MATRIX_COEF00_01;
		for (i = 0; i < 14; i++)
			reg_matrix_list[i].addr = reg + i;
	} else {
		reg = OSD1_HDR2_MATRIXI_COEF00_01;
		for (i = 0; i < 12; i++)
			reg_matrix_list[i].addr = reg + i;

		reg = OSD1_HDR2_MATRIXI_CLIP;
		reg_matrix_list[12].addr = reg;

		reg = OSD1_HDR2_MATRIXI_EN_CTRL;
		reg_matrix_list[13].addr = reg;
	}

	if (chip_type_id == chip_s7d ||
		chip_type_id == chip_s6) {
		reg = VPP_WRAP_OSD1_MATRIX_COEF00_01;
		for (i = 14; i < 28; i++)
			reg_matrix_list[i].addr = reg + i - 14;

		reg = OSD2_HDR2_MATRIXI_COEF00_01 + 0x500;
		for (i = 28; i < 40; i++)
			reg_matrix_list[i].addr = reg + i - 28;

		reg = OSD2_HDR2_MATRIXI_CLIP + 0x500;
		reg_matrix_list[40].addr = reg;

		reg = OSD2_HDR2_MATRIXI_EN_CTRL + 0x500;
		reg_matrix_list[41].addr = reg;

		reg = VPP_OSD2_MATRIX_COEF00_01;
		for (i = 42; i < 56; i++)
			reg_matrix_list[i].addr = reg + i - 42;
	} else if (chip_type_id == chip_t7 ||
		(chip_cls_id == TV_CHIP &&
		chip_type_id != chip_t6d &&
		chip_type_id != chip_t6w &&
		chip_type_id != chip_t6x)) {
		reg = VPP_POST2_MATRIX_COEF00_01;
		for (i = 14; i < 28; i++)
			reg_matrix_list[i].addr = reg + i - 14;
	} else if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		reg = OSD1_PQ_MATRIX_COEF00_01;
		for (i = 14; i < 28; i++)
			reg_matrix_list[i].addr = reg + i - 14;

		reg = VPP_POST2_MATRIX_COEF00_01;
		for (i = 28; i < 42; i++)
			reg_matrix_list[i].addr = reg + i - 28;

		reg = VPP_POST_MATRIX_COEF00_01;
		for (i = 42; i < 56; i++)
			reg_matrix_list[i].addr = reg + i - 42;
	}

	if (chip_type_id == chip_t7) {
		reg = OSD1_HDR2_CTRL;
		reg_matrix_list[length - 1].addr = reg;
	}

	for (i = 0; i < length; i++) {
		reg_matrix_list[i].type = REG_TYPE_VCBUS;
		reg_matrix_list[i].mask = 0xffffffff;
		reg = reg_matrix_list[i].addr;
		reg_matrix_list[i].val = READ_VPP_REG(reg);
		pr_amvecm_dbg("amvecm: suspend matrix [0x%x] = 0x%x\n",
			reg, reg_matrix_list[i].val);
	}

	pr_amvecm_dbg("amvecm: suspend matrix\n");
}

void suspend_ve(void)
{
	int i;
	unsigned int reg;
	unsigned int length = RECOVERY_REG_VE_MAX;

	if (chip_type_id == chip_c5)
		return;

	if (chip_type_id == chip_t3x)
		length += 6;
	else if (chip_type_id == chip_s5)
		length += 1;

	reg_ve_list = kcalloc(length, sizeof(struct am_reg_s), GFP_KERNEL);
	if (!reg_ve_list)
		return;

	if (pq_cfg.black_ext_en || pq_cfg.chroma_cor_en) {
		reg = VPP_BLACKEXT_CTRL;
		reg_ve_list[0].addr = reg;

		reg = VPP_BLUE_STRETCH_1;
		for (i = 1; i < 4; i++)
			reg_ve_list[i].addr = reg + i - 1;

		reg = VPP_CCORING_CTRL;
		for (i = 4; i < 8 ; i++)
			reg_ve_list[i].addr = reg + i - 4;
	}

	if (pq_cfg.dnlp_en) {
		if (chip_type_id == chip_t3x) {
			reg_ve_list[8].addr = 0x5245;//VPP_SRSHARP1_DNLP_EN
			reg_ve_list[9].addr = 0x5245 + 0x2500; //slice1
		} else {
			if (dnlp_sel == 2) {
				if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
					reg_ve_list[8].addr = offset_addr(SRSHARP1_DNLP_EN);
				} else if (chip_type_id == chip_s7d ||
					chip_type_id == chip_s6 ||
					chip_type_id == chip_t6d ||
					chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x) {
					reg_ve_list[8].addr = offset_addr(VPP_DNLP_EN_MODE);
				} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
					if ((!vinfo_lcd_support() && chip_type_id != chip_s5) ||
						is_meson_t7_cpu() ||
						chip_type_id == chip_txhd2)
						reg_ve_list[8].addr = offset_addr(SRSHARP0_DNLP_EN);
					else
						reg_ve_list[8].addr = offset_addr(SRSHARP1_DNLP_EN);
				} else {
					reg_ve_list[8].addr = offset_addr(SRSHARP0_DNLP_EN);
				}
			} else {
				reg_ve_list[8].addr = VPP_VE_ENABLE_CTRL;
			}
		}
	}

	if (chip_type_id == chip_t3x) {
		reg_ve_list[10].addr = 0x1a40; //VI_HIST_CTRL
		reg_ve_list[11].addr = 0x1a41;//VI_HIST_H_START_END
		reg_ve_list[12].addr = 0x1a42; //VI_HIST_V_START_END
		reg_ve_list[13].addr = 0x1a68; //VI_HIST_PIC_SIZE
		/*slice1*/
		reg_ve_list[14].addr = 0x1a40 + 0x30;
		reg_ve_list[15].addr = 0x1a41 + 0x30;
		reg_ve_list[16].addr = 0x1a42 + 0x30;
		reg_ve_list[17].addr = 0x1a68 + 0x30;
	} else if (chip_type_id == chip_s5) {
		reg_ve_list[9].addr = 0x1a40; //VI_HIST_CTRL
		reg_ve_list[10].addr = 0x1a41;//VI_HIST_H_START_END
		reg_ve_list[11].addr = 0x1a42; //VI_HIST_V_START_END
		reg_ve_list[12].addr = 0x1a68; //VI_HIST_PIC_SIZE
	} else {
		reg_ve_list[9].addr = VI_HIST_CTRL;
		reg_ve_list[10].addr = VI_HIST_GCLK_CTRL;
		reg_ve_list[11].addr = VI_HIST_PIC_SIZE;
	}

	for (i = 0; i < length; i++) {
		reg_ve_list[i].type = REG_TYPE_VCBUS;
		reg_ve_list[i].mask = 0xffffffff;
		reg = reg_ve_list[i].addr;
		reg_ve_list[i].val = READ_VPP_REG(reg);
	}

	pr_amvecm_dbg("amvecm: suspend ve\n");
}

void suspend_cm(void)
{
	int i, j, k;
	unsigned int reg;
	unsigned int addr_port = VPP_CHROMA_ADDR_PORT;/* 0x1d70; */
	unsigned int data_port = VPP_CHROMA_DATA_PORT;/* 0x1d71; */
	struct cm_port_s cm_port;

//	if (!pq_cfg.cm_en)
//		return;

	if (chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_c5)
		return;

	if (chip_type_id == chip_s5) {
		cm_port = get_cm_port();
		switch (cm_slice_idx) {
		case SLICE0:
			addr_port = cm_port.cm_addr_port[0];
			data_port = cm_port.cm_data_port[0];
			break;
		case SLICE1:
			addr_port = cm_port.cm_addr_port[1];
			data_port = cm_port.cm_data_port[1];
			break;
		case SLICE2:
			addr_port = cm_port.cm_addr_port[2];
			data_port = cm_port.cm_data_port[2];
			break;
		case SLICE3:
			addr_port = cm_port.cm_addr_port[3];
			data_port = cm_port.cm_data_port[3];
			break;
		default:
			break;
		}
	}

	reg_cm_list =
		kcalloc(RECOVERY_REG_CM_MAX, sizeof(struct am_reg_s), GFP_KERNEL);
	if (!reg_cm_list)
		return;

	k = 0;
	for (i = 0; i < 32; i++) {
		for (j = 0; j < 5; j++) {
			reg_cm_list[k].addr = CM2_ENH_COEF0_H00 + i * 8 + j;
			k++;
		}
	}

	for (i = 0; i < 160; i++) {
		reg_cm_list[i].type = REG_TYPE_INDEX_VPP_COEF;
		reg_cm_list[i].mask = 0xffffffff;
		reg = reg_cm_list[i].addr;
		WRITE_VPP_REG(addr_port, reg);
		reg_cm_list[i].val = READ_VPP_REG(data_port);
	}

	for (i = 160; i < 171; i++)
		reg_cm_list[i].addr = 0x200 + i - 160;
	reg_cm_list[171].addr = 0x20f;
	for (i = 172; i < RECOVERY_REG_CM_MAX; i++)
		reg_cm_list[i].addr = 0x215 + i - 172;

	for (i = 160; i < RECOVERY_REG_CM_MAX; i++) {
		reg_cm_list[i].type = REG_TYPE_INDEX_VPPCHROMA;
		reg_cm_list[i].mask = 0xffffffff;
		reg = reg_cm_list[i].addr;
		WRITE_VPP_REG(addr_port, reg);
		reg_cm_list[i].val = READ_VPP_REG(data_port);
	}
	cm_size = 0;
	pr_amvecm_dbg("amvecm: suspend cm\n");
}

void suspend_sr(void)
{
	int i;
	unsigned int reg;
	unsigned int len = 0;

	if (chip_type_id == chip_s7d ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_c5)
		return;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_TL1)
		len = RECOVERY_REG_SR_MAX - 114;
	else
		len = RECOVERY_REG_SR_MAX + 1;

	if (pq_cfg.sharpness0_en) {
		reg_sr0_list =
			kcalloc(len, sizeof(struct am_reg_s), GFP_KERNEL);
		if (!reg_sr0_list)
			return;

		reg = offset_addr(SRSHARP0_SHARP_HVSIZE);
		reg_sr0_list[0].addr = reg;
		reg = offset_addr(SRSHARP0_SHARP_HVBLANK_NUM);
		reg_sr0_list[1].addr = reg;
		reg = offset_addr(SRSHARP0_NR_GAUSSIAN_MODE);
		reg_sr0_list[2].addr = reg;
		reg = offset_addr(SRSHARP0_PK_CON_2CIRHPGAIN_TH_RATE);
		for (i = 3; i < 68; i++)
			reg_sr0_list[i].addr = reg + i - 3;
		reg = offset_addr(SRSHARP0_DEMO_CRTL);
		for (i = 68; i < 100; i++)
			reg_sr0_list[i].addr = reg + i - 68;
		reg = offset_addr(SRSHARP0_DB_FLT_CTRL);
		for (i = 100; i < 118; i++)
			reg_sr0_list[i].addr = reg + i - 100;
		reg = SRSHARP0_SHARP_SYNC_CTRL + get_sr0_offset();
		reg_sr0_list[118].addr = reg;

		if (get_cpu_type() <= MESON_CPU_MAJOR_ID_TL1) {
			reg_sr0_list[len - 1].addr = VPP_SRSHARP0_CTRL;
		} else {
			reg = 0x50b2;
			for (i = 119; i < 128; i++)
				reg_sr0_list[i].addr = reg + i - 119;
			reg = SRSHARP0_FMETER_CTRL;
			for (i = 128; i < 135; i++)
				reg_sr0_list[i].addr = reg + i - 128;
			reg = SRSHARP0_SR7_DRTLPF_EN;
			for (i = 135; i < 199; i++)
				reg_sr0_list[i].addr = reg + i - 135;
			reg = SRSHARP0_SR7_CLR_PRT_PARAM;
			for (i = 199; i < len - 2; i++)
				reg_sr0_list[i].addr = reg + i - 199;
			reg_sr0_list[len - 2].addr = VPP_SRSHARP0_CTRL;
			reg_sr0_list[len - 1].addr = 0x5164;
		}

		for (i = 0; i < len; i++) {
			reg_sr0_list[i].type = REG_TYPE_VCBUS;
			reg_sr0_list[i].mask = 0xffffffff;
			reg = reg_sr0_list[i].addr;
			reg_sr0_list[i].val = READ_VPP_REG(reg);
		}
		pr_amvecm_dbg("amvecm: suspend sr0\n");
	}

	if (chip_type_id == chip_t7 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_txhd2)
		return;

	if (pq_cfg.sharpness1_en) {
		reg_sr1_list =
			kcalloc(len, sizeof(struct am_reg_s), GFP_KERNEL);
		if (!reg_sr1_list)
			return;

		reg = offset_addr(SRSHARP1_SHARP_HVSIZE);
		reg_sr1_list[0].addr = reg;
		reg = offset_addr(SRSHARP1_SHARP_HVBLANK_NUM);
		reg_sr1_list[1].addr = reg;
		reg = offset_addr(SRSHARP1_NR_GAUSSIAN_MODE);
		reg_sr1_list[2].addr = reg;
		reg = offset_addr(SRSHARP1_PK_CON_2CIRHPGAIN_TH_RATE);
		for (i = 3; i < 68; i++)
			reg_sr1_list[i].addr = reg + i - 3;
		reg = offset_addr(SRSHARP1_DEMO_CRTL);
		for (i = 68; i < 100; i++)
			reg_sr1_list[i].addr = reg + i - 68;
		reg = offset_addr(SRSHARP1_DB_FLT_CTRL);
		for (i = 100; i < 118; i++)
			reg_sr1_list[i].addr = reg + i - 100;
		reg = SRSHARP1_SHARP_SYNC_CTRL + get_sr0_offset();
		reg_sr1_list[118].addr = reg;

		if (get_cpu_type() <= MESON_CPU_MAJOR_ID_TL1) {
			reg_sr1_list[len - 1].addr = VPP_SRSHARP1_CTRL;
		} else {
			reg = 0x52b2;
			for (i = 119; i < 128; i++)
				reg_sr1_list[i].addr = reg + i - 119;
			reg = SRSHARP1_FMETER_CTRL;
			for (i = 128; i < 135; i++)
				reg_sr1_list[i].addr = reg + i - 128;
			reg = SRSHARP1_SR7_DRTLPF_EN;
			for (i = 135; i < 199; i++)
				reg_sr1_list[i].addr = reg + i - 135;
			reg = SRSHARP1_SR7_CLR_PRT_PARAM;
			for (i = 199; i < len - 2; i++)
				reg_sr1_list[i].addr = reg + i - 199;
			reg_sr1_list[len - 2].addr = VPP_SRSHARP1_CTRL;
			reg_sr1_list[len - 1].addr = 0x5364;
		}

		for (i = 0; i < len; i++) {
			reg_sr1_list[i].type = REG_TYPE_VCBUS;
			reg_sr1_list[i].mask = 0xffffffff;
			reg = reg_sr1_list[i].addr;
			reg_sr1_list[i].val = READ_VPP_REG(reg);
		}

		pr_amvecm_dbg("amvecm: suspend sr1\n");
	}
}

void suspend_lc(void)
{
	int i;
	unsigned int reg;
	unsigned int reg_addr0 = SRSHARP1_LC_INPUT_MUX;
	unsigned int reg_addr1 = SRSHARP1_LC_TOP_CTRL;
	unsigned int length = RECOVERY_REG_LC_MAX;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		reg_addr0 = VPP_LC_BLK_HV_NUM;
		reg_addr1 = VPP_LC_INPUT_SEL;
	}

	if (!pq_cfg.lc_en)
		return;

	reg_lc_list =
		kcalloc(length, sizeof(struct am_reg_s), GFP_KERNEL);
	if (!reg_lc_list)
		return;

	reg = offset_addr(reg_addr0);
	reg_lc_list[0].addr = reg;

	reg = offset_addr(reg_addr1);
	for (i = 1; i < 35; i++)
		reg_lc_list[i].addr = reg + i - 1;

	reg = LC_CURVE_CTRL;
	for (i = 35; i < 57; i++)
		reg_lc_list[i].addr = reg + i - 35;

	reg = LC_CURVE_YMINVAL_LMT_12_13;
	for (i = 57; i < 69; i++)
		reg_lc_list[i].addr = reg + i - 57;

	for (i = 0; i < length; i++) {
		reg_lc_list[i].type = REG_TYPE_VCBUS;
		reg_lc_list[i].mask = 0xffffffff;
		reg = reg_lc_list[i].addr;
		reg_lc_list[i].val = READ_VPP_REG(reg);
	}

	pr_amvecm_dbg("amvecm: suspend lc\n");
}

void suspend_vsr_sharpness(void)
{
	int i;
	unsigned int reg;
	unsigned int ext_num = 9;

	if (chip_type_id != chip_t6d &&
		chip_type_id != chip_s7d &&
		chip_type_id != chip_s6 &&
		chip_type_id != chip_t6w &&
		chip_type_id != chip_t6x)
		return;

	if (pq_cfg.sharpness0_en) {
		if (chip_type_id == chip_t6x) {
			ext_num = 107 + 6;
			reg_vsr_list = kcalloc(RECOVERY_REG_SR_MAX + ext_num,
				sizeof(struct am_reg_s), GFP_KERNEL);
			if (!reg_vsr_list)
				return;
			reg = offset_addr(VPP_SR_MISC);
			for (i = 0; i < 0x100; i++)
				reg_vsr_list[i].addr = reg + i;

			reg = 0x7a00;
			for (i = 0x100; i < 0x155; i++) //os,cc
				reg_vsr_list[i].addr = reg + i - 0x100;

			reg = offset_addr(VPP_DNLP_SATPRT_DIV_M);
			for (i = 0x155; i < RECOVERY_REG_SR_MAX + ext_num; i++)
				reg_vsr_list[i].addr = reg + i - 0x155;

			for (i = 0; i < RECOVERY_REG_SR_MAX + ext_num; i++) {
				reg_vsr_list[i].type = REG_TYPE_VCBUS;
				reg_vsr_list[i].mask = 0xffffffff;
				reg = reg_vsr_list[i].addr;
				reg_vsr_list[i].val = READ_VPP_REG_EX(reg, 0);
			}
		} else {
			ext_num = 9;
			reg_vsr_list = kcalloc(RECOVERY_REG_SR_MAX + ext_num,
				sizeof(struct am_reg_s), GFP_KERNEL);
			if (!reg_vsr_list)
				return;

			reg = offset_addr(VPP_SR_MISC);
			for (i = 0; i < 5; i++)
				reg_vsr_list[i].addr = reg + i;

			reg = offset_addr(VPP_SR_DIR_EN);
			for (i = 5; i < 237; i++)
				reg_vsr_list[i].addr = reg + i - 5;

			reg = offset_addr(VPP_DNLP_SATPRT_DIV_M);
			for (i = 237; i < RECOVERY_REG_SR_MAX + ext_num; i++)
				reg_vsr_list[i].addr = reg + i - 237;

			for (i = 0; i < RECOVERY_REG_SR_MAX + ext_num; i++) {
				reg_vsr_list[i].type = REG_TYPE_VCBUS;
				reg_vsr_list[i].mask = 0xffffffff;
				reg = reg_vsr_list[i].addr;
				reg_vsr_list[i].val = READ_VPP_REG(reg);
			}
		}

		pr_amvecm_dbg("amvecm: suspend vsr sharpness\n");
	}
}

void resume_mtx_t7(void)
{
	unsigned int start_idx = RECOVERY_REG_MTX_MAX;
	int i;
	unsigned int val = 0;
	unsigned int addr = 0;
	struct am_regs_s *p = &amregs_store;

	if (!reg_matrix_list)
		return;

	p->length = RECOVERY_REG_MTX_MAX;
	if (!(memcpy(p->am_reg, reg_matrix_list + start_idx,
		RECOVERY_REG_MTX_MAX * sizeof(struct am_reg_s))))
		return;

	for (i = 0; i < p->length; i++) {
		/*mask == 0xffffffff*/
		val = p->am_reg[i].val;
		addr = p->am_reg[i].addr;

		if (addr == 0) {
			pr_amvecm_dbg("\n[%s] i=%d, val=%d, p length=%d\n",
				__func__, i, val, p->length);
			continue;
		}
		WRITE_VPP_REG(addr, val);
	}

	pr_info("amvecm: resume post2 mtx\n");
}

void resume_matrix(int vpp_index)
{
	int i;
	unsigned int reg, tmp;
	unsigned int length;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12B)
		return;

	if (!reg_matrix_list)
		return;

	if (chip_type_id == chip_s7d ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		length = RECOVERY_REG_MTX_MAX * 4;
	else if (chip_type_id == chip_t7)
		length = RECOVERY_REG_MTX_MAX * 2 + 1;
	else if (chip_cls_id == TV_CHIP)
		length = RECOVERY_REG_MTX_MAX * 2;
	else
		length = RECOVERY_REG_MTX_MAX;

	amregs_store.length = length;
	if (!(memcpy(amregs_store.am_reg, reg_matrix_list,
		length * sizeof(struct am_reg_s))))
		return;

	for (i = 0; i < length; i++) {
		reg = reg_matrix_list[i].addr;
		tmp = READ_VPP_REG(reg);
		pr_amvecm_dbg("amvecm:resume matrix [0x%x] = 0x%x/0x%x\n",
			reg, reg_matrix_list[i].val, tmp);
	}

	am_set_regmap(&amregs_store, vpp_index);

	kfree(reg_matrix_list);
	reg_matrix_list = NULL;
	pr_amvecm_dbg("amvecm: resume matrix\n");
}

void resume_ve(int vpp_index)
{
	if (chip_type_id == chip_c5)
		return;

	unsigned int length = RECOVERY_REG_VE_MAX;

	if (chip_type_id == chip_t3x)
		length += 6;
	else if (chip_type_id == chip_s5)
		length += 1;

	if (!reg_ve_list)
		return;

	amregs_store.length = length;
	if (!(memcpy(amregs_store.am_reg, reg_ve_list,
		length * sizeof(struct am_reg_s))))
		return;

	am_set_regmap(&amregs_store, vpp_index);

	kfree(reg_ve_list);
	reg_ve_list = NULL;
	pr_amvecm_dbg("amvecm: resume ve\n");
}

void resume_cm(int vpp_index)
{
	if (chip_type_id != chip_s6)
		return;

	pr_amvecm_dbg("amvecm: resume cm0, pq_cfg.cm_en/cm_en=%d/%d\n",
		pq_cfg.cm_en, cm_en);

	if (!cm_en)
		amcm_disable(WR_VCB, vpp_index);

//	if (pq_cfg.cm_en && reg_cm_list) {
	if (reg_cm_list) {
		amregs_store.length = RECOVERY_REG_CM_MAX;
		if (!(memcpy(amregs_store.am_reg, reg_cm_list,
			RECOVERY_REG_CM_MAX * sizeof(struct am_reg_s))))
			return;

		am_set_regmap(&amregs_store, vpp_index);

		kfree(reg_cm_list);
		reg_cm_list = NULL;
		pr_amvecm_dbg("amvecm: resume cm\n");

		if (cm_en)
			amcm_enable(WR_VCB, vpp_index);
	}
}

void resume_sr(int vpp_index)
{
	unsigned int len = 0;

	if (chip_type_id == chip_s7d ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		return;

	if (get_cpu_type() <= MESON_CPU_MAJOR_ID_TL1)
		len = RECOVERY_REG_SR_MAX - 114;
	else
		len = RECOVERY_REG_SR_MAX + 1;

	if (pq_cfg.sharpness0_en && reg_sr0_list) {
		amregs_store.length = len;
		if (!(memcpy(amregs_store.am_reg, reg_sr0_list,
			len * sizeof(struct am_reg_s))))
			return;
		am_set_regmap(&amregs_store, vpp_index);

		kfree(reg_sr0_list);
		reg_sr0_list = NULL;
		pr_amvecm_dbg("amvecm: resume sr0\n");
	}

	if (chip_type_id == chip_t7 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_txhd2)
		return;

	if (pq_cfg.sharpness1_en && reg_sr1_list) {
		amregs_store.length = len;
		if (!(memcpy(amregs_store.am_reg, reg_sr1_list,
			len * sizeof(struct am_reg_s))))
			return;
		am_set_regmap(&amregs_store, vpp_index);

		kfree(reg_sr1_list);
		reg_sr1_list = NULL;
		pr_amvecm_dbg("amvecm: resume sr1\n");
	}
}

void resume_vsr_sharpness(int vpp_index)
{
	int len = 0;

	if (chip_type_id != chip_t6d &&
		chip_type_id != chip_s7d &&
		chip_type_id != chip_s6 &&
		chip_type_id != chip_t6w &&
		chip_type_id != chip_t6x)
		return;

	if (!reg_vsr_list)
		return;

	if (pq_cfg.sharpness0_en) {
		if (chip_type_id == chip_t6x)
			len = RECOVERY_REG_SR_MAX + 107 + 6;
		else
			len = RECOVERY_REG_SR_MAX + 3 + 6;

		amregs_store.length = len;
		if (!(memcpy(amregs_store.am_reg, reg_vsr_list,
			len * sizeof(struct am_reg_s))))
			return;

		am_set_regmap(&amregs_store, vpp_index);

		kfree(reg_vsr_list);
		reg_vsr_list = NULL;
		pr_amvecm_dbg("amvecm: resume vsr sharpness\n");
	}
}

void resume_lc(int vpp_index)
{
	if (!pq_cfg.lc_en || !reg_lc_list)
		return;

	amregs_store.length = RECOVERY_REG_LC_MAX;
	if (!(memcpy(amregs_store.am_reg, reg_lc_list,
		RECOVERY_REG_LC_MAX * sizeof(struct am_reg_s))))
		return;

	am_set_regmap(&amregs_store, vpp_index);

	kfree(reg_lc_list);
	reg_lc_list = NULL;
	pr_amvecm_dbg("amvecm: resume lc\n");
}

void resume_dnlp(void)
{
	if (chip_type_id == chip_c5)
		return;

	if (!pq_cfg.dnlp_en)
		return;

	ve_new_dnlp_param_update();
	pr_amvecm_dbg("amvecm: resume dnlp\n");
}

void resume_vadj1(int vpp_index)
{
	if (!pq_cfg.vadj1_en)
		return;

	if (chip_type_id == chip_c5)
		return;

	if (chip_type_id == chip_s5) {
		ve_vadj_ctl(WR_DMA, VE_VADJ1, pq_cfg.vadj1_en, vpp_index);
	} else {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ1_MISC,
				pq_cfg.vadj1_en, 0, 1);
		else
			VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
				pq_cfg.vadj1_en, 0, 1);
	}

	vecm_latch_flag |= FLAG_VADJ1_BRI;
	vecm_latch_flag |= FLAG_VADJ1_CON;
	pq_user_latch_flag |= PQ_USER_CMS_SAT_HUE;

	pr_amvecm_dbg("amvecm: resume vadj1\n");
}

void resume_vadj2(int vpp_index)
{
	if (!pq_cfg.vadj2_en)
		return;

	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		WRITE_VPP_REG_BITS(VOUT_VADJ_MISC,
				pq_cfg.vadj2_en, 0, 1);
	} else if (chip_type_id == chip_s5) {
		ve_vadj_ctl(WR_DMA, VE_VADJ2, pq_cfg.vadj2_en, vpp_index);
	} else {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ2_MISC,
				pq_cfg.vadj2_en, 0, 1);
		else
			VSYNC_WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
				pq_cfg.vadj2_en, 2, 1);
	}

	amvecm_set_brightness2(vd1_brightness2);
	amvecm_set_contrast2(vd1_contrast2);
	amvecm_set_saturation_hue_post(saturation_post, hue_post);
	pr_amvecm_dbg("amvecm: resume vadj2\n");
}

void resume_wb(int vpp_index)
{
	if (!pq_cfg.wb_en)
		return;

	if (chip_type_id == chip_s5)
		post_wb_ctl(WR_DMA, wb_en, vpp_index);
	else
		amvecm_wb_enable(wb_en);

	ve_ogo_param_update();
	pr_amvecm_dbg("amvecm: resume wb\n");
}

void resume_lut3d(int vpp_index)
{
	if (chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_c5)
		return;

	pr_amvecm_dbg("amvecm: resume lut3d lut3d_en=%d, ct_en = %d\n",
		lut3d_en, ct_en);
	if (lut3d_en || ct_en) {
		vpp_enable_lut3d(1);
		lut3d_update(0, vpp_index);
	} else {
		flag_lut3d_resume = 1;
	}
}

void resume_lcd_gamma(int vpp_index)
{
	if (!pq_cfg.gamma_en)
		return;

	if (gamma_en) {
		vpp_enable_lcd_gamma_table(0, 1, vpp_index);
		vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		pr_amvecm_dbg("amvecm: resume gamma\n");
	}
}

void resume_hdr_clk_gate(void)
{
	if (chip_type_id != chip_t5w)
		return;

	WRITE_VPP_REG(VD1_HDR2_CLK_GATE, 0xaaa);
	pr_amvecm_dbg("amvecm: resume vd1 hdr clk gate\n");
}

static void resume_dma(void)
{
	if (chip_type_id == chip_t6x) {
		am_dma_set_mif_wr_status(1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_CM2_LC, 0);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VD1_HDR_HIST, 1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VD2_HDR_HIST, 1);
		am_dma_set_mif_rd(EN_DMA_RD_ID_GAMUT0, 0);
		am_dma_set_mif_rd(EN_DMA_RD_ID_GAMUT1, 0);
		am_dma_set_mif_rd(EN_DMA_RD_ID_3DLUT, 1);
	}
}

static void resume_gamut_wrapper(void)
{
	if (chip_type_id == chip_t6x) {
		gamut_mapping0_param.flag_update_lut = 1;
		gamut_mapping0_param.flag_update_mtx = 1;
		gamut_mapping1_param.flag_update_lut = 1;
		gamut_mapping1_param.flag_update_mtx = 1;
		vecm_latch_flag2 |= FLAG_GAMUT_MAPPING0_UPDATE;
		vecm_latch_flag2 |= FLAG_GAMUT_MAPPING1_UPDATE;
	}
}

void resume_recovery_process(int vpp_index)
{
	struct vinfo_s *vinfo = get_current_vinfo();

	if (vecm_latch_flag2 & FLAG_RESUME_RECOVERY) {
		vecm_latch_flag2 &= ~FLAG_RESUME_RECOVERY;

		if (chip_type_id == chip_a4)
			return;

		resume_matrix(vpp_index);

		if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu() ||
			is_meson_txlx_cpu() || is_meson_txhd_cpu() ||
			is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T7 ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5M ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T6W ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T6X ||
			chip_type_id == chip_a9) {
			if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
				/*frequency meter init*/
				if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
					get_cpu_type() == MESON_CPU_MAJOR_ID_T5W)
					amve_fmeter_init(fmeter_en);

				if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7))
					vpp_pst_hist_sta_config(1, HIST_MAXRGB,
						AFTER_POST2_MTX, vinfo);

				if (chip_type_id == chip_t6d ||
					chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x ||
					chip_type_id == chip_a9) {
					s7d_sharpness_init();
					resume_vsr_sharpness(vpp_index);
					pr_amvecm_dbg("amvecm: resume sharpness\n");
				} else {
					resume_sr(vpp_index);
				}

				if (chip_cls_id == TV_CHIP)
					resume_lc(vpp_index);
			}
		} else if (is_meson_g12a_cpu() ||
			is_meson_g12b_cpu() ||
			is_meson_sm1_cpu() ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_SC2 ||
			is_meson_s4_cpu() ||
			is_meson_s4d_cpu() ||
			is_meson_s7_cpu() ||
			is_meson_s7d_cpu() ||
			is_meson_s6_cpu()) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6) {
				s7d_sharpness_init();
				resume_vsr_sharpness(vpp_index);
				pr_amvecm_dbg("amvecm: resume sharpness\n");
			} else {
				resume_sr(vpp_index);
			}
		} else if (chip_type_id == chip_s5) {
			cm_top_ctl(WR_DMA, 1, vpp_index);
		}

//		resume_cm(vpp_index);
		resume_ve(vpp_index);
		resume_dnlp();
		resume_vadj1(vpp_index);
		resume_wb(vpp_index);
		resume_vadj2(vpp_index);
		resume_lut3d(vpp_index);

		if ((get_cpu_type() == MESON_CPU_MAJOR_ID_T7 ||
			chip_type_id == chip_a9) ||
			chip_cls_id == TV_CHIP || get_cpu_type() == MESON_CPU_MAJOR_ID_G12B)
			resume_lcd_gamma(vpp_index);

		pc_mode_last = 0xff;
		suspend_drv_status_set(false);
		pr_info("amvecm: resume recovery vsync\n");
	}
}

void dump_pq_cfg(struct pq_ctrl_s *cfg_data)
{
	if (!cfg_data)
		return;

	pr_info("sharpness0_en: %d\n", cfg_data->sharpness0_en);
	pr_info("sharpness1_en: %d\n", cfg_data->sharpness1_en);
	pr_info("dnlp_en: %d\n", cfg_data->dnlp_en);
	pr_info("cm_en: %d\n", cfg_data->cm_en);
	pr_info("vadj1_en: %d\n", cfg_data->vadj1_en);
	pr_info("vd1_ctrst_en: %d\n", cfg_data->vd1_ctrst_en);
	pr_info("vadj2_en: %d\n", cfg_data->vadj2_en);
	pr_info("post_ctrst_en: %d\n", cfg_data->post_ctrst_en);
	pr_info("wb_en: %d\n", cfg_data->wb_en);
	pr_info("gamma_en: %d\n", cfg_data->gamma_en);
	pr_info("lc_en: %d\n", cfg_data->lc_en);
	pr_info("black_ext_en: %d\n", cfg_data->black_ext_en);
	pr_info("chroma_cor_en: %d\n", cfg_data->chroma_cor_en);
}
#endif

static const char *amvecm_debug_usage_str = {
	"Usage:\n"
	"echo vpp_size > /sys/class/amvecm/debug; get vpp size config\n"
	"echo wb enable > /sys/class/amvecm/debug\n"
	"echo wb disable > /sys/class/amvecm/debug\n"
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	"echo gamma enable > /sys/class/amvecm/debug\n"
	"echo gamma disable > /sys/class/amvecm/debug\n"
	"echo gamma load_protect_en > /sys/class/amvecm/debug\n"
	"echo gamma load_protect_dis > /sys/class/amvecm/debug\n"
	"echo sharpness enable > /sys/class/amvecm/debug\n"
	"echo sharpness disable > /sys/class/amvecm/debug\n"
	"echo sharpness peaking_en > /sys/class/amvecm/debug\n"
	"echo sharpness peaking_dis > /sys/class/amvecm/debug\n"
	"echo sharpness lcti_en > /sys/class/amvecm/debug\n"
	"echo sharpness lcti_dis > /sys/class/amvecm/debug\n"
	"echo sharpness dejaggy_en > /sys/class/amvecm/debug\n"
	"echo sharpness dejaggy_dis > /sys/class/amvecm/debug\n"
	"echo sharpness dering_en > /sys/class/amvecm/debug\n"
	"echo sharpness dering_dis > /sys/class/amvecm/debug\n"
	"echo sharpness drlpf_en > /sys/class/amvecm/debug\n"
	"echo sharpness drlpf_dis > /sys/class/amvecm/debug\n"
	"echo sharpness theta_en > /sys/class/amvecm/debug\n"
	"echo sharpness theta_dis > /sys/class/amvecm/debug\n"
	"echo sharpness deband_en > /sys/class/amvecm/debug\n"
	"echo sharpness deband_dis > /sys/class/amvecm/debug\n"
	"echo cm enable > /sys/class/amvecm/debug\n"
	"echo cm disable > /sys/class/amvecm/debug\n"
	"echo cm cm2_clr_dbg val > /sys/class/amvecm/debug\n"
	"echo cm cur_color_md val(0:9clr 1:14clr) > /sys/class/amvecm/debug\n"
	"echo dnlp enable > /sys/class/amvecm/debug\n"
	"echo dnlp disable > /sys/class/amvecm/debug\n"
#endif
	"echo vpp_pq enable > /sys/class/amvecm/debug\n"
	"echo vpp_pq disable > /sys/class/amvecm/debug\n"
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	"echo keystone_process > /sys/class/amvecm/debug; keystone init config\n"
	"echo keystone_status > /sys/class/amvecm/debug; keystone parameter status\n"
	"echo keystone_regs > /sys/class/amvecm/debug; keystone regs value\n"
	"echo keystone_config param1(D) param2(D) > /sys/class/amvecm/debug; keystone param config\n"
	"echo vpp_mtx xvycc_10 rgb2yuv > /sys/class/amvecm/debug; 10bit xvycc mtx\n"
	"echo vpp_mtx xvycc_10 yuv2rgb > /sys/class/amvecm/debug; 10bit xvycc mtx\n"
	"echo vpp_mtx post_10 rgb2yuv > /sys/class/amvecm/debug; 10bit post mtx\n"
	"echo vpp_mtx post_10 yuv2rgb > /sys/class/amvecm/debug; 10bit post mtx\n"
	"echo vpp_mtx vd1_10 rgb2yuv > /sys/class/amvecm/debug; 10bit vd1 mtx\n"
	"echo vpp_mtx vd1_10 yuv2rgb > /sys/class/amvecm/debug; 10bit vd1 mtx\n"
	"echo vpp_mtx xvycc_12 rgb2yuv > /sys/class/amvecm/debug; 12bit xvycc mtx\n"
	"echo vpp_mtx xvycc_12 yuv2rgb > /sys/class/amvecm/debug; 12bit xvycc mtx\n"
	"echo vpp_mtx post_12 rgb2yuv > /sys/class/amvecm/debug; 12bit post mtx\n"
	"echo vpp_mtx post_12 yuv2rgb > /sys/class/amvecm/debug; 12bit post mtx\n"
	"echo vpp_mtx vd1_12 rgb2yuv > /sys/class/amvecm/debug; 12bit vd1 mtx\n"
	"echo vpp_mtx vd1_12 yuv2rgb > /sys/class/amvecm/debug; 12bit vd1 mtx\n"
	"echo hdr_top_ctrl_mode val > /sys/class/amvecm/debug;\n"
	"-->val=0:drv ctrl, 1:parameter only, 2:gamut mtrx only\n"
	"-->3:in+out mtrx, 4:in+out+gamut mtrx, 5:parameter and mtrx\n"
	"echo gamut coef string > /sys/class/amvecm/debug; string=9*4 bytes coef hex\n"
	"echo gamut get_coef_str > /sys/class/amvecm/debug\n"
	"echo hdr_mtrx in coef string > /sys/class/amvecm/debug; string=15*4 bytes coef hex\n"
	"echo hdr_mtrx in get_coef_str > /sys/class/amvecm/debug\n"
	"echo hdr_mtrx out coef string > /sys/class/amvecm/debug; string=15*4 bytes coef hex\n"
	"echo hdr_mtrx out get_coef_str > /sys/class/amvecm/debug\n"
#endif
	"echo bitdepth 10/12/other-num > /sys/class/amvecm/debug; config data path\n"
	"echo datapath_config param1(D) param2(D) > /sys/class/amvecm/debug; config data path\n"
	"echo datapath_status > /sys/class/amvecm/debug; data path status\n"
	"echo clip_config 0/1/2/.. 0/1/... 0/1 > /sys/class/amvecm/debug; config clip\n"
	"echo vpp_mtrx_test sel csc on slice > /sys/class/amvecm/debug;\n"
};

static ssize_t amvecm_debug_show(const struct class *cla,
				 const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int i = 0;

	pr_info("cm2_debug:%d\n", cm2_debug);
	pr_info("cm_cur_work_color_md:%d\n", cm_cur_work_color_md);

	amvecm_update_module_status();

	for (i = 0; i < pq_module_module_max; i++)
		len += sprintf(buf + len,
			"pq_module_status_for_tool:%d=%d\n", i, pq_module_status[i]);
#endif

	len += sprintf(buf, "%s\n", amvecm_debug_usage_str);

	pr_info("[AMVECM_VERSION] %s\n", AMVECM_VERSION);

	return len;
}

static ssize_t amvecm_debug_store(const struct class *cla,
				  const struct class_attribute *attr,
				  const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	long val2 = 0;
	unsigned int i = 0;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	long tmp_val[4] = {0};
	enum vpp_matrix_e mtx_sel;
	int mtx_csc;
	int mtx_on;
	enum vpp_slice_e slice;
	unsigned int mode_sel, color, color_mode;
	struct vinfo_s *vinfo = get_current_vinfo();
	enum vd_path_e vd_path = VD1_PATH;
	enum lut_dma_wr_id_e dma_wr_id = EN_DMA_WR_ID_LC_STTS_0;
	int coef[15] = {0};
	int char_count = 0;
	char char_val[5] = {0};
	char *stemp = NULL;
	unsigned int string_len = 0;
	struct hdr_mtrx_data_s hdr_mtrx_data;
	struct hdr_gamut_data_s hdr_gamut_data;
	int temp = 0;
	long r_index = 0;
	char *hex_str = NULL;
	unsigned short r, g, b;
	unsigned int reg_gb;
#endif

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "vpp_size", 8)) {
		dump_vpp_size_info();
	} else if (!strncmp(parm[0], "vpp_state", 9)) {
		pr_info("amvecm driver version :  %s\n", AMVECM_VER);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (!strncmp(parm[0], "dump_pq_cfg_cur", 15)) {
		pr_info("---amvecm: pq_cfg_cur:\n");
		dump_pq_cfg(&pq_cfg_cur);
	} else if (!strncmp(parm[0], "dump_pq_cfg", 11)) {
		pr_info("---amvecm: pq_cfg:\n");
		dump_pq_cfg(&pq_cfg);
	} else if (!strncmp(parm[0], "dump_dd_cfg_bypass", 18)) {
		pr_info("---amvecm: dd_cfg_bypass:\n");
		dump_pq_cfg(&dv_cfg_bypass);
	} else if (!strncmp(parm[0], "hdr_fix_mode", 12)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		hdr_core_fix_mode = val;
		pr_info("hdr_core_fix_mode: %d\n", hdr_core_fix_mode);
	} else if (!strncmp(parm[0], "osd_gamut", 9)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		osd_gamut_conv_type = val;
		pr_info("osd_gamut_type(0:off,1:bt709,2:dci-p3,3:bt2020): %d\n",
			osd_gamut_conv_type);
	} else if (!strncmp(parm[0], "main_path", 9)) {
		data_path = 0;
		pr_info("main data_path: %d\n", data_path);
	} else if (!strncmp(parm[0], "sub_path", 8)) {
		data_path = 1;
		pr_info("sub data_path: %d\n", data_path);
	} else if (!strncmp(parm[0], "dma_ctrl", 8)) {
		if (chip_type_id == chip_t3x) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;

			switch (val) {
			case 0:
				dma_wr_id = EN_DMA_WR_ID_LC_STTS_0;
				break;
			case 1:
				dma_wr_id = EN_DMA_WR_ID_LC_STTS_1;
				break;
			case 2:
				dma_wr_id = EN_DMA_WR_ID_VI_HIST_SPL_0;
				break;
			case 3:
				dma_wr_id = EN_DMA_WR_ID_VI_HIST_SPL_1;
				break;
			case 4:
				dma_wr_id = EN_DMA_WR_ID_CM2_HIST_0;
				break;
			case 5:
				dma_wr_id = EN_DMA_WR_ID_CM2_HIST_1;
				break;
			case 6:
				dma_wr_id = EN_DMA_WR_ID_VD1_HDR_0;
				break;
			case 7:
				dma_wr_id = EN_DMA_WR_ID_VD1_HDR_1;
				break;
			case 8:
				dma_wr_id = EN_DMA_WR_ID_VD2_HDR;
				break;
			default:
				break;
			}

			if (!strncmp(parm[2], "enable", 6)) {
				am_dma_set_mif_wr(dma_wr_id, 1);
				pr_info("dma enable %d\n", dma_wr_id);
			} else if (!strncmp(parm[2], "disable", 7)) {
				am_dma_set_mif_wr(dma_wr_id, 0);
				pr_info("dma disable %d\n", dma_wr_id);
			}
		}
	} else if (!strncmp(parm[0], "update_vd2_hdr_hist", 20)) {
		/*s5_get_hist(VD2_PATH, HIST_E_RGBMAX);*/
		pr_info("amvecm update_vd2_hdr_hist path %d\n", VD2_PATH);
	} else if (!strncmp(parm[0], "refresh_hdr_hist", 16)) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		if (val > 0)
			vd_path = VD2_PATH;
		get_hist(vd_path, HIST_E_RGBMAX, VPP_VCBUS);
		pr_info("amvecm refresh_hdr_hist path %d\n", vd_path);
	} else if (!strncmp(parm[0], "checkpattern", 12)) {
		if (!strncmp(parm[1], "enable", 6)) {
			pattern_detect_debug = 1;
			enable_pattern_detect = 1;
			pr_info("enable pattern detection\n");
		} else if (!strncmp(parm[1], "debug", 5)) {
			pattern_detect_debug = 2;
			pr_info("enable pattern detection debug info\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			pattern_detect_debug = 0;
			enable_pattern_detect = 0;
			pr_info("disable pattern detection\n");
		} else if (!strncmp(parm[1], "setmask", 7)) {
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			pattern_mask = val;
			pr_info("pattern_mask is 0x%x\n", pattern_mask);
		} else if (!strncmp(parm[1], "getmask", 7)) {
			pr_info("pattern_mask is 0x%x\n", pattern_mask);
		}
	} else if (!strncmp(parm[0], "vadj1", 5)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_vadj_enable(VE_VADJ1, 1);
			pr_info("enable vadj1\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_vadj_enable(VE_VADJ1, 0);
			pr_info("disable vadj1\n");
		}
	} else if (!strncmp(parm[0], "vadj2", 5)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_vadj_enable(VE_VADJ2, 1);
			pr_info("enable vadj2\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_vadj_enable(VE_VADJ2, 0);
			pr_info("disable vadj2\n");
		}
	} else if (!strncmp(parm[0], "wb", 2)) {
		if (!strncmp(parm[1], "enable", 6)) {
			if (!data_path)
				amvecm_wb_enable(1);
			else
				amvecm_wb_enable_sub(1);
			pr_info("enable wb\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			if (!data_path)
				amvecm_wb_enable(0);
			else
				amvecm_wb_enable_sub(0);
			pr_info("disable wb\n");
		}
	} else if (!strncmp(parm[0], "pre_gamma", 2)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_pre_gamma_enable(1);
			pr_info("enable pre_gamma\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_pre_gamma_enable(0);
			pr_info("disable pre_gamma\n");
		}
	} else if (!strncmp(parm[0], "gamma", 5)) {
		if (!strncmp(parm[1], "enable", 6)) {
			if (!data_path)
				vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;	/* gamma off */
			else
				vecm_latch_flag |= FLAG_GAMMA_TABLE_EN_SUB;
			pr_info("enable gamma\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			if (!data_path)
				vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;/* gamma off */
			else
				vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS_SUB;
			pr_info("disable gamma\n");
		} else if (!strncmp(parm[1], "load_protect_en", 15)) {
			gamma_loadprotect_en = 1;
			pr_info("disable gamma before loading new gamma\n");
		} else if (!strncmp(parm[1], "load_protect_dis", 16)) {
			gamma_loadprotect_en = 0;
			pr_info("loading new gamma without protect");
		} else if (!strncmp(parm[1], "rdma_wr", 7)) {
			if (!parm[2])
				goto free_buf;

			if (!strncmp(parm[2], "r", 1)) {
				memset(&video_gamma_table_r, 0,
					sizeof(struct tcon_gamma_table_s));
				pr_info("r curve to 0\n");
			} else if (!strncmp(parm[2], "g", 1)) {
				memset(&video_gamma_table_g, 0,
					sizeof(struct tcon_gamma_table_s));
				pr_info("g curve to 0\n");
			} else if (!strncmp(parm[2], "b", 1)) {
				memset(&video_gamma_table_b, 0,
					sizeof(struct tcon_gamma_table_s));
				pr_info("b curve to 0\n");
			} else if (!strncmp(parm[2], "linear", 6)) {
				for (i = 0; i < 256; i++) {
					video_gamma_table_r.data[i] = i << 2;
					video_gamma_table_g.data[i] = i << 2;
					video_gamma_table_b.data[i] = i << 2;
				}
				pr_info("all curve to linear\n");
			}
			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		} else if (!strncmp(parm[1], "dump", 4)) {
			for (i = 0; i < 256; i++) {
				pr_info("[%d] (%d, %d, %d)\n", i,
					video_gamma_table_r.data[i],
					video_gamma_table_g.data[i],
					video_gamma_table_b.data[i]);
			}
		}
	} else if (!strncmp(parm[0], "gamma_sub", 9)) {
		if (!strncmp(parm[1], "enable", 6)) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_EN_SUB; /* gamma on */
			pr_info("enable gamma\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS_SUB; /* gamma off */
			pr_info("disable gamma\n");
		}
	} else if (!strncmp(parm[0], "sharpness", 9)) {
		if (!strncmp(parm[1], "peaking_en", 10)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(1, 1);
			else
				amvecm_sharpness_enable(0);
			pr_info("enable peaking\n");
		} else if (!strncmp(parm[1], "peaking_dis", 11)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(1, 0);
			else
				amvecm_sharpness_enable(1);
			pr_info("disable peaking\n");
		} else if (!strncmp(parm[1], "lcti_en", 7)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(2, 1);
			else
				amvecm_sharpness_enable(2);
			pr_info("enable lti cti\n");
		} else if (!strncmp(parm[1], "lcti_dis", 8)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(2, 0);
			else
				amvecm_sharpness_enable(3);
			pr_info("disable lti cti\n");
		} else if (!strncmp(parm[1], "theta_en", 8)) {
			amvecm_sharpness_enable(4);
			pr_info("SR4 enable drtlpf theta\n");
		} else if (!strncmp(parm[1], "theta_dis", 9)) {
			amvecm_sharpness_enable(5);
			pr_info("SR4 disable drtlpf theta\n");
		} else if (!strncmp(parm[1], "deband_en", 9)) {
			amvecm_sharpness_enable(6);
			pr_info("SR4 enable debanding\n");
		} else if (!strncmp(parm[1], "deband_dis", 10)) {
			amvecm_sharpness_enable(7);
			pr_info("SR4 disable debanding\n");
		} else if (!strncmp(parm[1], "dejaggy_en", 10)) {
			amvecm_sharpness_enable(8);
			pr_info("SR3 enable dejaggy\n");
		} else if (!strncmp(parm[1], "dejaggy_dis", 11)) {
			amvecm_sharpness_enable(9);
			pr_info("SR3 disable dejaggy\n");
		} else if (!strncmp(parm[1], "dering_en", 9)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(3, 1);
			else
				amvecm_sharpness_enable(10);
			pr_info("SR3 enable dering\n");
		} else if (!strncmp(parm[1], "dering_dis", 10)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(3, 0);
			else
				amvecm_sharpness_enable(11);
			pr_info("SR3 disable dering\n");
		} else if (!strncmp(parm[1], "drlpf_en", 8)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(6, 1);
			else
				amvecm_sharpness_enable(12);
			pr_info("SR3 enable drlpf\n");
		} else if (!strncmp(parm[1], "drlpf_dis", 9)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9)
				amve_sharpness_sub_ctrl(6, 0);
			else
				amvecm_sharpness_enable(13);
			pr_info("SR3 disable drlpf\n");
		} else if (!strncmp(parm[1], "enable", 6)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9) {
				amve_sharpness_sub_ctrl(0, 1);
			} else {
				amvecm_sharpness_enable(0);
				amvecm_sharpness_enable(2);
				amvecm_sharpness_enable(4);
				amvecm_sharpness_enable(6);
				amvecm_sharpness_enable(8);
				amvecm_sharpness_enable(10);
				amvecm_sharpness_enable(12);
			}
			pr_info("sharpness enable\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9) {
				amve_sharpness_sub_ctrl(0, 0);
			} else {
				amvecm_sharpness_enable(1);
				amvecm_sharpness_enable(3);
				amvecm_sharpness_enable(5);
				amvecm_sharpness_enable(7);
				amvecm_sharpness_enable(9);
				amvecm_sharpness_enable(11);
				amvecm_sharpness_enable(13);
			}
			pr_info("sharpness disable\n");
		}
	} else if (!strcmp(parm[0], "cm")) {
		if (!strncmp(parm[1], "enable", 6)) {
			cm_en = 1;
			pr_info("enable cm_en:%d\n", cm_en);
		} else if (!strncmp(parm[1], "disable", 7)) {
			cm_en = 0;
			pr_info("disable cm_en:%d\n", cm_en);
		} else if (!strcmp(parm[1], "cur_color_md")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				cm_cur_work_color_md = val;
			} else {
				pr_info("unsupport cm_cur_work_color_md cmd\n");
				goto free_buf;
			}
		} else if (!strcmp(parm[1], "cm2_clr_dbg")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				cm2_debug = val;
			} else {
				pr_info("unsupport cm2_clr_dbg cmd\n");
				goto free_buf;
			}
		} else if (!strcmp(parm[1], "cm_init")) {
			cm_init_config(12);
			pr_info("cm init\n");
		} else {
			pr_info("unsupport cm cmd\n");
			goto free_buf;
		}
	} else if (!strncmp(parm[0], "dnlp", 4)) {
		if (!strncmp(parm[1], "enable", 6)) {
			ve_enable_dnlp();
			pr_info("enable dnlp\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			ve_disable_dnlp();
			pr_info("disable dnlp\n");
		}
#endif
	} else if (!strncmp(parm[0], "vpp_pq", 6)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_pq_enable(1);
			pr_info("enable vpp_pq\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_pq_enable(0);
			pr_info("disable vpp_pq\n");
		}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (!strncmp(parm[0], "vpp_mtx", 7)) {
		if (!strncmp(parm[1], "vd1_10", 6)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_1;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);
				pr_info("10bit vd1 mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 2);
				pr_info("10bit vd1 mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "post_10", 7)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_2;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);
				pr_info("10bit post mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 2);
				pr_info("10bit post mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "xvycc_10", 8)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_3;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);
				pr_info("10bit xvycc mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 2);
				pr_info("10bit xvycc mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "vd1_12", 6)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_1;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 3);
				pr_info("1wbit vd1 mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 4);
				pr_info("1wbit vd1 mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "post_12", 7)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_2;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 3);
				pr_info("1wbit post mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 4);
				pr_info("1wbit post mtx rgb2yuv\n");
			}
		} else if (!strncmp(parm[1], "xvycc_12", 8)) {
			mtx_sel_dbg |= 1 << VPP_MATRIX_3;
			if (!strncmp(parm[2], "yuv2rgb", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 3);
				pr_info("1wbit xvycc mtx yuv2rgb\n");
			} else if (!strncmp(parm[2], "rgb2yuv", 7)) {
				amvecm_vpp_mtx_debug(mtx_sel_dbg, 4);
				pr_info("1wbit xvycc mtx rgb2yuv\n");
			}
		}
	} else if (!strncmp(parm[0], "ve_dith", 7)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amvecm_dither_enable(1);
			pr_info("enable ve dither\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amvecm_dither_enable(0);
			pr_info("disable ve dither\n");
		} else if (!strncmp(parm[1], "rd_en", 5)) {
			amvecm_dither_enable(3);
			pr_info("enable ve round dither\n");
		} else if (!strncmp(parm[1], "rd_dis", 6)) {
			amvecm_dither_enable(2);
			pr_info("disable ve round dither\n");
		}
	} else if (!strcmp(parm[0], "keystone_process")) {
		keystone_correction_process();
		pr_info("keystone_correction_process done!\n");
	} else if (!strcmp(parm[0], "keystone_status")) {
		keystone_correction_status();
	} else if (!strcmp(parm[0], "keystone_regs")) {
		keystone_correction_regs();
	} else if (!strcmp(parm[0], "keystone_config")) {
		enum vks_param_e vks_param;
		unsigned int vks_param_val;

		if (!parm[2]) {
			pr_info("misss param\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		vks_param = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		vks_param_val = val;
		keystone_correction_config(vks_param, vks_param_val);
#endif
	} else if (!strcmp(parm[0], "bitdepth")) {
		unsigned int bitdepth;

		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		bitdepth = val;
		vpp_bitdepth_config(bitdepth);
	} else if (!strcmp(parm[0], "datapath_config")) {
		unsigned int node, param1, param2;

		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		node = val;

		if (!parm[2]) {
			pr_info("misss param2\n");
			goto free_buf;
		}
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		param1 = val;

		if (!parm[3]) {
			pr_info("misss param3,default is 0\n");
			goto free_buf;
		}
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto free_buf;
		param2 = val;
		vpp_datapath_config(node, param1, param2);
	} else if (!strcmp(parm[0], "datapath_status")) {
		vpp_datapath_status();
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (!strcmp(parm[0], "clip_config")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			mode_sel = val;
		} else {
			mode_sel = 0;
		}
		if (parm[2]) {
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			color = val;
		} else {
			color = 0;
		}
		if (parm[3]) {
			if (kstrtoul(parm[3], 16, &val) < 0)
				goto free_buf;
			color_mode = val;
		} else {
			color_mode = 0;
		}
		vpp_clip_config(mode_sel, color, color_mode);
		pr_info("vpp_clip_config done!\n");
	} else if (!strcmp(parm[0], "3dlut_bypass")) {
		force_lut3d_bypass = 1;
		pr_info("force_lut3d_bypass = %d\n", force_lut3d_bypass);
	} else if (!strcmp(parm[0], "3dlut_testpattern")) {
		int r, g, b;

		if (parm[1]) {
			if (kstrtol(parm[1], 10, &val) < 0)
				goto free_buf;
			r = val;
		} else {
			r = 0;
		}
		if (parm[2]) {
			if (kstrtol(parm[2], 10, &val) < 0)
				goto free_buf;
			g = val;
		} else {
			g = 0;
		}
		if (parm[3]) {
			if (kstrtol(parm[3], 10, &val) < 0)
				goto free_buf;
			b = val;
		} else {
			b = 0;
		}
		vpp_lut3d_table_init(r, g, b);
		vpp_set_lut3d(0, 0, 0, 0, 0);
		/*vpp_lut3d_table_release();*/
	} else if (!strcmp(parm[0], "3dlut")) {
		if (!parm[1])
			goto free_buf;

		if (!strcmp(parm[1], "enable")) {
			lut3d_en = 1;
		} else if (!strcmp(parm[1], "disable")) {
			vpp_enable_lut3d(0);
			lut3d_en = 0;
		} else if (!strcmp(parm[1], "open")) {
			vpp_enable_lut3d(1);
		} else if (!strcmp(parm[1], "close")) {
			vpp_enable_lut3d(0);
		} else if (!strcmp(parm[1], "release")) {
			vpp_lut3d_table_release();
		} else if (!strcmp(parm[1], "debug")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_debug = val;
			} else {
				lut3d_debug = 0;
			}
		} else if (!strcmp(parm[1], "dma_case")) {
			if (!parm[2]) {
				pr_info("cur lut3d_dma_case = %d\n", lut3d_dma_case);
			} else {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_dma_case = val;
				WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, lut3d_dma_case, 1, 1);
			}
		} else if (!strcmp(parm[1], "long_section")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_long_sec_en = val;
			} else {
				lut3d_long_sec_en = 0;
			}
		} else if (!strcmp(parm[1], "compress")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_compress = val;
			} else {
				lut3d_compress = 0;
			}
		} else if (!strcmp(parm[1], "write_source")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_write_from_file = val;
			} else {
				lut3d_write_from_file = 0;
			}
		} else if (!strcmp(parm[1], "read_source")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_data_source = val;
			} else {
				lut3d_data_source = 0;
			}
		} else if (!strcmp(parm[1], "order")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				lut3d_order = val;
			} else {
				lut3d_order = 0;
			}
		} else if (!strcmp(parm[1], "load")) {
			unsigned int index;

			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
				index = val;
			} else {
				index = 0;
			}

			if (index > 2) {
				index = 2;
				pr_info("support up to 3 different luts\n");
			}

			vpp_lut3d_table_init(0, 0, 0);
			vpp_set_lut3d(lut3d_data_source, index, 0, 0, 0);
			/*vpp_lut3d_table_release();*/
		} else if (!strcmp(parm[1], "writesection")) {
			unsigned int section_len, start, paracount;
			unsigned int *section_in;
			int readcount = 0;
			unsigned int encode_table_size;
			char data[4];
			char *buffer = NULL;
			#ifdef CONFIG_SET_FS
			struct file *fp;
			mm_segment_t fs;
			loff_t pos;
			#endif
			unsigned int section_max;

			encode_table_size =
				257 * sizeof(unsigned long);
			if (chip_type_id == chip_t6d) {
				section_len = LUT3D_POINTS9;
				section_max = LUT3D_SINGLE_SIZE9;
			} else {
				section_len = LUT3D_POINTS;
				section_max = LUT3D_SINGLE_SIZE;
			}
			if (lut3d_long_sec_en) {
				if (chip_type_id == chip_t6d)
					section_len = LUT3D_SINGLE_SIZE9;
				else
					section_len = LUT3D_SINGLE_SIZE;
			}
			section_in = kmalloc(sizeof(unsigned int) * section_len * 3,
				GFP_KERNEL);
			if (!section_in)
				goto free_buf;

			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0) {
					kfree(section_in);
					goto free_buf;
				}
				start = val;
			} else {
				start = 0;
			}

			if (start > ((section_max / section_len) - 1)) {
				start = (section_max / section_len) - 1;
				pr_info("section index should be in 0 ~ %d-1\n",
					(section_max / section_len));
			}

			memset(section_in, 0,
			       section_len * 3 * sizeof(unsigned int));

			if (!parm[3]) {
				kfree(section_in);
				goto free_buf;
			}

			buffer = parm[3];
			readcount = strlen(buffer);
			if (lut3d_write_from_file) {
				buffer =
				kmalloc(section_len * 9 + encode_table_size,
					GFP_KERNEL);
				if (!buffer) {
					kfree(section_in);
					goto free_buf;
				}

				#ifdef CONFIG_SET_FS
				fp = filp_open(parm[3], O_RDONLY, 0);
				if (IS_ERR(fp)) {
					kfree(section_in);
					kfree(buffer);
					goto free_buf;
				}
				//fs = get_fs();
				//set_fs(KERNEL_DS);
				memset(buffer, 0,
				       section_len * 9 + encode_table_size);
				pos = 0;
				readcount =
				vfs_read(fp, buffer,
					 section_len * 9 + encode_table_size,
					 &pos);
				pr_info("read file lut data size %d\n",
					readcount);
				if (readcount <= 0) {
					kfree(section_in);
					kfree(buffer);
					goto free_buf;
				}
				filp_close(fp, NULL);
				//set_fs(fs);
				#endif
			}
			if (lut3d_compress) {
				huff64_decode(buffer, (unsigned int)readcount,
					      section_in, section_len * 3);
			} else {
				paracount = (strlen(buffer) + 2) / 3;
				if (paracount > (section_len * 3))
					paracount = section_len * 3;

				for (i = 0; i < paracount; ++i) {
					data[0] = buffer[3 * i + 0];
					data[1] = buffer[3 * i + 1];
					data[2] = buffer[3 * i + 2];
					data[3] = '\0';
					if (kstrtoul(data, 16, &val) < 0) {
						kfree(section_in);
						goto free_buf;
					}
					section_in[i] = val;
				}
			}

			vpp_write_lut3d_section(start,
						section_len,
						section_in);

			kfree(section_in);
			if (lut3d_write_from_file)
				kfree(buffer);
		} else if (!strcmp(parm[1], "readsection")) {
			unsigned int section_len, start, len;
			unsigned int *section_out;
			unsigned int encode_table_size;
			char *tmp, tmp1[10] = {0};
			#ifdef CONFIG_SET_FS
			struct file *fp;
			mm_segment_t fs;
			loff_t pos;
			#endif
			unsigned int section_max;

			encode_table_size =
				257 * sizeof(unsigned long);
			if (chip_type_id == chip_t6d) {
				section_len = LUT3D_POINTS9;
				section_max = LUT3D_SINGLE_SIZE9;
			} else {
				section_len = LUT3D_POINTS;
				section_max = LUT3D_SINGLE_SIZE;
			}
			if (lut3d_long_sec_en) {
				if (chip_type_id == chip_t6d)
					section_len = LUT3D_SINGLE_SIZE9;
				else
					section_len = LUT3D_SINGLE_SIZE;
			}
			section_out =
			kmalloc(sizeof(unsigned int) * section_len * 3,
				GFP_KERNEL);
			if (!section_out)
				goto free_buf;
			/* extra space is for encoding table */
			/* make sure there is enough buffer to use */
			tmp =
			kmalloc(section_len * 9 + encode_table_size,
				GFP_KERNEL);
			if (!tmp) {
				kfree(section_out);
				goto free_buf;
			}
			memset(tmp, 0,
			       section_len * 9 + encode_table_size);

			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0) {
					kfree(section_out);
					kfree(tmp);
					goto free_buf;
				}
				start = val;
			} else {
				start = 0;
			}

			if (start > ((section_max / section_len) - 1)) {
				start = (section_max / section_len) - 1;
				pr_info("section index should be in 0 ~ %d-1\n",
					(section_max / section_len));
			}

			vpp_read_lut3d_section(start,
					       section_len,
					       section_out);

			if (lut3d_compress) {
				len = huff64_encode(section_out,
						    section_len * 3,
						    tmp);
				tmp[len] = 0;
				pr_info("compressed len %d vs %d\n",
					len, section_len * 9);

			} else {
				for (i = 0; i < section_len; ++i) {
					sprintf(tmp1, "%03x%03x%03x",
						section_out[i * 3 + 0],
						section_out[i * 3 + 1],
						section_out[i * 3 + 2]);
					strcat(tmp, tmp1);
				}
				len = section_len * 9;
			}

			if (parm[3]) {
				#ifdef CONFIG_SET_FS
				fp = filp_open(parm[3],
					       O_RDWR | O_CREAT | O_APPEND,
					       0644);
				if (IS_ERR(fp)) {
					kfree(section_out);
					kfree(tmp);
					goto free_buf;
				}
				//fs = get_fs();
				//set_fs(KERNEL_DS);
				pos = fp->f_pos;
				vfs_write(fp, tmp,
					  len,
					  &pos);

				filp_close(fp, NULL);
				//set_fs(fs);
				#endif
			}

			kfree(section_out);
			kfree(tmp);
		} else {
			pr_info("unsupport cmd!\n");
		}
	} else if (!strcmp(parm[0], "3dlut_dump")) {
		if (!strcmp(parm[1], "init_tab"))
			dump_plut3d_table();
		else if (!strcmp(parm[1], "reg_tab"))
			dump_plut3d_reg_table();
		else
			pr_info("unsupport cmd!\n");
	} else if (!strcmp(parm[0], "cm_hist")) {
		if (!parm[1]) {
			pr_info("miss param1\n");
			goto free_buf;
		}
		if (!strcmp(parm[1], "hue"))
			pr_cm_hist(CM_HUE_HIST);
		else if (!strcmp(parm[1], "sat"))
			pr_cm_hist(CM_SAT_HIST);
		else
			pr_info("unsupport cmd\n");
	}  else if (!strcmp(parm[0], "cm_hist_config")) {
		unsigned int en, mode, wd0, wd1, wd2, wd3;

		if (!parm[6]) {
			pr_info("miss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		en = val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		mode = val;
		if (kstrtoul(parm[3], 10, &val) < 0)
			goto free_buf;
		wd0 = val;
		if (kstrtoul(parm[4], 10, &val) < 0)
			goto free_buf;
		wd1 = val;
		if (kstrtoul(parm[5], 10, &val) < 0)
			goto free_buf;
		wd2 = val;
		if (kstrtoul(parm[6], 10, &val) < 0)
			goto free_buf;
		wd3 = val;
		cm_hist_config(en, mode, wd0, wd1, wd2, wd3);
		pr_info("cm hist config success\n");
	} else if (!strcmp(parm[0], "cm_hist_thrd")) {
		int rd, ro_frame, thrd0, thrd1;

		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			rd = val;
		} else {
			pr_info("unsupport cmd\n");
			goto free_buf;
		}
		if (rd) {
			cm_sta_hist_range_thrd(rd, 0, 0, 0);
		} else {
			if (!parm[4]) {
				pr_info("miss param1\n");
				goto free_buf;
			}
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			ro_frame = val;
			if (kstrtoul(parm[3], 16, &val) < 0)
				goto free_buf;
			thrd0 = val;
			if (kstrtoul(parm[4], 16, &val) < 0)
				goto free_buf;
			thrd1 = val;
			cm_sta_hist_range_thrd(rd, ro_frame, thrd0, thrd1);
			pr_info("cm hist thrd set success\n");
		}
#endif
	} else if (!strcmp(parm[0], "cm_bri_con")) {
		int rd, bri, con, blk_lel;

		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			rd = val;
		} else {
			pr_info("unsupport cmd\n");
			goto free_buf;
		}

		if (rd) {
			cm_luma_bri_con(rd, 0, 0, 0);
		} else {
			if (!parm[3]) {
				pr_info("miss param1\n");
				goto free_buf;
			}
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			bri = val;
			if (kstrtoul(parm[2], 16, &val) < 0)
				goto free_buf;
			con = val;
			if (kstrtoul(parm[3], 16, &val) < 0)
				goto free_buf;
			blk_lel = val;
			cm_luma_bri_con(rd, bri, con, blk_lel);
			pr_info("cm hist bri_con set success\n");
		}
	} else if (!strcmp(parm[0], "dump_overscan_table")) {
		for (i = 0; i < TIMING_MAX; i++) {
			pr_info("*****dump overscan_tab[%d]*****\n", i);
			pr_info("hs:%d, he:%d, vs:%d, ve:%d, screen_mode:%d, afd:%d, flag:%d.\n",
				overscan_table[i].hs, overscan_table[i].he,
				overscan_table[i].vs, overscan_table[i].ve,
				overscan_table[i].screen_mode,
				overscan_table[i].afd_enable,
				overscan_table[i].load_flag);
		}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (!strcmp(parm[0], "set_gamma_lut_65")) {
		if (!strcmp(parm[1], "default"))
			set_gamma_regs(1, 0, 0);
		else if (!strcmp(parm[1], "straight"))
			set_gamma_regs(1, 1, 0);
		if (!strcmp(parm[1], "default1"))
			set_gamma_regs(1, 0, 1);
		else if (!strcmp(parm[1], "straight1"))
			set_gamma_regs(1, 1, 1);
		else
			pr_info("unsupport cmd\n");
	} else if (!strcmp(parm[0], "dump_post_sta")) {
		dump_flag = 1;
	} else if (!strcmp(parm[0], "force_toggle")) {
		force_toggle();
#endif
	} else if (!strcmp(parm[0], "pd_fix_lvl")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		if (val > PD_DEF_LVL)
			pd_fix_lvl = PD_DEF_LVL;
		else
			pd_fix_lvl = val;
	} else if (!strcmp(parm[0], "gmv_th")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		gmv_th = val;
	} else if (!strcmp(parm[0], "pd_weak_fix_lvl")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		if (val > PD_DEF_LVL)
			pd_weak_fix_lvl = PD_DEF_LVL;
		else
			pd_weak_fix_lvl = val;
	} else if (!strcmp(parm[0], "gmv_weak_th")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		if (val >= gmv_th)
			gmv_weak_th = gmv_th - 1;
		else
			gmv_weak_th = val;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (!strcmp(parm[0], "post2mtx")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 16, &val) < 0)
				goto free_buf;
			mtx_setting(POST2_MTX, val, 1);
		}
	} else if (!strcmp(parm[0], "mltcast_ratio1")) {
		pr_info("current value: %d\n", mltcast_ratio1);
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		mltcast_ratio1 = val;
		pr_info("setting value: %d\n", mltcast_ratio1);
	} else if (!strcmp(parm[0], "mltcast_ratio2")) {
		pr_info("current value: %d\n", mltcast_ratio2);
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		mltcast_ratio2 = val;
		pr_info("setting value: %d\n", mltcast_ratio2);
	} else if (!strcmp(parm[0], "mltcast_skip_en")) {
		pr_info("current value: %d\n", mltcast_skip_en);
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		mltcast_skip_en = val;
		pr_info("setting value: %d\n", mltcast_skip_en);
	} else if (!strcmp(parm[0], "pst_hist_sel")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
			if (val == 1) {
				vpp_pst_hist_sta_config(1, HIST_MAXRGB,
					AFTER_POST2_MTX, vinfo);
				pr_info("hist sel: max rgb hist\n");
			} else {
				vpp_pst_hist_sta_config(1, HIST_YR,
					BEFORE_POST2_MTX, vinfo);
				pr_info("hist sel: Y hist\n");
			}
		}
	} else if (!strcmp(parm[0], "pd_det")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		pd_det = (uint)val;
		pr_info("pd_det: %d\n", pd_det);
	} else if (!strcmp(parm[0], "dyn_gamma")) {
		u8 dynamic_gamma_num;

		if (!strcmp(parm[1], "gamma_tb")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
			}

			for (i = 0; i < 256; i++)
				pr_info("vd_gamma_r_data[%d] :%d vd_gamma_g_data[%d] :%d vd_gamma_b_data[%d] :%d\n",
				i, video_gamma_table_r.data[i],
				i, video_gamma_table_g.data[i],
				i, video_gamma_table_b.data[i]);
		} else if (!strcmp(parm[1], "gamma_gt_tb")) {
			if (parm[2]) {
				if (kstrtoul(parm[2], 10, &val) < 0)
					goto free_buf;
			}
			dynamic_gamma_num = (u8)val;

			if (dynamic_gamma_num >= FREESYNC_DYNAMIC_GAMMA_NUM)
				dynamic_gamma_num = FREESYNC_DYNAMIC_GAMMA_NUM - 1;

			for (i = 0; i < 256; i++)
				pr_info("gt_gamma_r_data[%d][%d] :%d gt_gamma_g_data[%d][%d] :%d gt_gamma_b_data[%d][%d] :%d\n",
				dynamic_gamma_num, i, gt.gm_tb[dynamic_gamma_num][0].data[i],
				dynamic_gamma_num, i, gt.gm_tb[dynamic_gamma_num][1].data[i],
				dynamic_gamma_num, i, gt.gm_tb[dynamic_gamma_num][2].data[i]);
		}
	} else if (!strcmp(parm[0], "pr_hist")) {
		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &val) < 0)
				goto free_buf;
		}
		pr_hist = (uint)val;
		pr_info("pr_hist = %d\n", pr_hist);
	} else if (!strcmp(parm[0], "vlk_clk")) {
		if (!strcmp(parm[1], "disable")) {
			clk_disable_unprepare(amvecm_dev.vlock_clk);
			pr_info("vid_lock_clk disable!!!");
		} else if (!strcmp(parm[1], "enable")) {
			clk_prepare_enable(amvecm_dev.vlock_clk);
			pr_info("vid_lock_clk enable!!!");
		} else {
			pr_info("unsupport cmd\n");
		}
	} else if (!strcmp(parm[0], "vsr_pq")) {
		/*s7d sharpness, safa debug*/
		if (!strcmp(parm[1], "480p")) {
			vsr_pq_config(RES_480P, WR_VCB, 0);
			pr_info("set vsr_pq 480p\n");
		} else if (!strcmp(parm[1], "720p")) {
			vsr_pq_config(RES_720P, WR_VCB, 0);
			pr_info("set vsr_pq 720p\n");
		} else if (!strcmp(parm[1], "1080p")) {
			vsr_pq_config(RES_1080P, WR_VCB, 0);
			pr_info("set vsr_pq 1080p\n");
		} else if (!strcmp(parm[1], "default")) {
			vsr_pq_config(DEFAULT_PARAM, WR_VCB, 0);
			pr_info("set vsr_pq default\n");
		}
	} else if (!strcmp(parm[0], "safa_pq")) {
		if (!strcmp(parm[1], "480p")) {
			safa_pq_config(RES_480P, WR_VCB, 0);
			pr_info("set safa_pq 480p\n");
		} else if (!strcmp(parm[1], "720p")) {
			safa_pq_config(RES_720P, WR_VCB, 0);
			pr_info("set safa_pq 720p\n");
		} else if (!strcmp(parm[1], "1080p")) {
			safa_pq_config(RES_1080P, WR_VCB, 0);
			pr_info("set safa_pq 1080p\n");
		} else if (!strcmp(parm[1], "default")) {
			safa_pq_config(DEFAULT_PARAM, WR_VCB, 0);
			pr_info("set safa_pq default\n");
		}
	} else if (!strcmp(parm[0], "pi_pq")) {
		if (!strcmp(parm[1], "480p")) {
			pi_pq_config(RES_480P, WR_VCB, 0);
			pr_info("set pi_pq 480p\n");
		} else if (!strcmp(parm[1], "720p")) {
			pi_pq_config(RES_720P, WR_VCB, 0);
			pr_info("set pi_pq 720p\n");
		} else if (!strcmp(parm[1], "1080p")) {
			pi_pq_config(RES_1080P, WR_VCB, 0);
			pr_info("set pi_pq 1080p\n");
		} else if (!strcmp(parm[1], "default")) {
			pi_pq_config(DEFAULT_PARAM, WR_VCB, 0);
			pr_info("set pi_pq default\n");
		}
	} else if (!strcmp(parm[0], "vpp_mtrx_test")) {
		if (chip_type_id != chip_t3x)
			goto free_buf;

		if (parm[1]) {
			if (kstrtoul(parm[1], 10, &tmp_val[0]) < 0)
				goto free_buf;
		}

		if (tmp_val[0] == 0)
			mtx_sel = VD1_MTX;
		else if (tmp_val[0] == 1)
			mtx_sel = POST2_MTX;
		else
			mtx_sel = POST_MTX;

		if (parm[2]) {
			if (kstrtoul(parm[2], 10, &tmp_val[1]) < 0)
				goto free_buf;
		}
		mtx_csc = (int)tmp_val[1];

		if (parm[3]) {
			if (kstrtoul(parm[3], 10, &tmp_val[2]) < 0)
				goto free_buf;
		}

		if (tmp_val[2] > 0)
			mtx_on = 1;
		else
			mtx_on = 0;

		if (parm[4]) {
			if (kstrtoul(parm[4], 10, &tmp_val[3]) < 0)
				goto free_buf;
		}

		if (tmp_val[3] > 0)
			slice = SLICE1;
		else
			slice = SLICE0;
		ve_mtrx_setting(mtx_sel, mtx_csc, mtx_on, slice);
		pr_info("ve_mtrx: %d %d %d %d\n",
			mtx_sel, mtx_csc, mtx_on, slice);
	} else if (!strcmp(parm[0], "s_vsr_update_flag")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		vsr_update_flag = val;
	} else if (!strcmp(parm[0], "g_vsr_update_flag")) {
		pr_info("vsr_update_flag: %d\n", vsr_update_flag);
	} else if (!strncmp(parm[0], "pi", 2)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amve_sharpness_sub_ctrl(4, 1);
			pr_info("enable pi\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amve_sharpness_sub_ctrl(4, 0);
			pr_info("disable pi\n");
		}
	} else if (!strncmp(parm[0], "safa", 4)) {
		if (!strncmp(parm[1], "enable", 6)) {
			amve_sharpness_sub_ctrl(5, 1);
			pr_info("enable safa\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			amve_sharpness_sub_ctrl(5, 0);
			pr_info("disable safa\n");
		}
	} else if (!strcmp(parm[0], "set_viu2_gamma_lut_65")) {
		if (!strcmp(parm[1], "default"))
			set_viu2_gamma_regs(1, 0);
		else if (!strcmp(parm[1], "straight"))
			set_viu2_gamma_regs(1, 1);
		else if (!strcmp(parm[1], "disable"))
			set_viu2_gamma_regs(0, 0);
		else
			pr_info("unsupport cmd\n");
#endif
	} else if (!strcmp(parm[0], "s_peak_final_ngain")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		demo_pk_sr_final_ngains = val;
	} else if (!strcmp(parm[0], "g_peak_final_ngain")) {
		pr_info("demo_pk_final_ngain: %d\n", demo_pk_sr_final_ngains);
	} else if (!strcmp(parm[0], "s_peak_final_pgain")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		demo_pk_sr_final_pgains = val;
	} else if (!strcmp(parm[0], "g_peak_final_pgain")) {
		pr_info("vsr_update_flag: %d\n", demo_pk_sr_final_pgains);
	} else if (!strncmp(parm[0], "osd_sharpness", 13)) {
		if (!strncmp(parm[1], "enable", 6)) {
			osd_sharpness_ctrl(0, 1);
			pr_info("enable osd sharpness\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			osd_sharpness_ctrl(0, 0);
			pr_info("disable osd sharpness\n");
		} else if (!strncmp(parm[1], "pk_en", 5)) {
			osd_sharpness_ctrl(1, 1);
			pr_info("enable osd sharpness peak\n");
		} else if (!strncmp(parm[1], "pk_dis", 6)) {
			osd_sharpness_ctrl(1, 0);
			pr_info("disable osd sharpness peak\n");
		} else if (!strncmp(parm[1], "os_en", 5)) {
			osd_sharpness_ctrl(2, 1);
			pr_info("enable osd sharpness os\n");
		} else if (!strncmp(parm[1], "os_dis", 6)) {
			osd_sharpness_ctrl(2, 0);
			pr_info("disable osd sharpness os\n");
		} else if (!strncmp(parm[1], "cc_en", 5)) {
			osd_sharpness_ctrl(3, 1);
			pr_info("enable osd sharpness cc\n");
		} else if (!strncmp(parm[1], "cc_dis", 6)) {
			osd_sharpness_ctrl(3, 0);
			pr_info("disable osd sharpness cc\n");
		}
	} else if (!strcmp(parm[0], "s_hsize_in")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		hsize_in = val;
	} else if (!strcmp(parm[0], "g_hsize_in")) {
		pr_info("hsize_in: %d\n", hsize_in);
	} else if (!strcmp(parm[0], "s_vsize_in")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		vsize_in = val;
	} else if (!strcmp(parm[0], "g_vsize_in")) {
		pr_info("vsize_in: %d\n", vsize_in);
	} else if (!strcmp(parm[0], "s_reg_pk_dir_final_gain")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		reg_pk_dir_final_gain = val;
	} else if (!strcmp(parm[0], "g_reg_pk_dir_final_gain")) {
		pr_info("reg_pk_dir_final_gain: %d\n", reg_pk_dir_final_gain);
	} else if (!strcmp(parm[0], "s_reg_pk_cir_final_gain")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		reg_pk_cir_final_gain = val;
	} else if (!strcmp(parm[0], "g_reg_pk_cir_final_gain")) {
		pr_info("reg_pk_cir_final_gain: %d\n", reg_pk_cir_final_gain);
	} else if (!strcmp(parm[0], "s_reg_pk_final_pgain")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		reg_pk_final_pgain = val;
	} else if (!strcmp(parm[0], "g_reg_pk_final_pgain")) {
		pr_info("reg_pk_final_pgain: %d\n", reg_pk_final_pgain);
	} else if (!strcmp(parm[0], "s_reg_pk_final_ngain")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		reg_pk_final_ngain = val;
	} else if (!strcmp(parm[0], "g_reg_pk_final_ngain")) {
		pr_info("reg_pk_final_ngain: %d\n", reg_pk_final_ngain);
	} else if (!strcmp(parm[0], "s_reg_pk_nor_rsft_mode")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		reg_pk_nor_rsft_mode = val;
	} else if (!strcmp(parm[0], "g_reg_pk_nor_rsft_mode")) {
		pr_info("reg_pk_nor_rsft_mode: %d\n", reg_pk_nor_rsft_mode);
	} else if (kstrtol(parm[0], 10, &val) >= 0) {
		if (kstrtol(parm[1], 10, &val2) < 0)
			goto free_buf;
		val2 = (val2 == 0) ? 0 : 1;
		pr_info("pq_module(%d), enable(%d)\n", (int)val, (int)val2);
		switch (val) {
		case pq_module_vpp_pq:
			amvecm_pq_enable(val2);
			pq_module_status[pq_module_vpp_pq] = val2;
			break;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		case pq_module_sharpness:
			if (val2) {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x) {
					amve_sharpness_sub_ctrl(0, 1);
				} else {
					amvecm_sharpness_enable(0);
					amvecm_sharpness_enable(2);
					amvecm_sharpness_enable(4);
					amvecm_sharpness_enable(6);
					amvecm_sharpness_enable(8);
					amvecm_sharpness_enable(10);
					amvecm_sharpness_enable(12);
				}
			} else {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x) {
					amve_sharpness_sub_ctrl(0, 0);
				} else {
					amvecm_sharpness_enable(1);
					amvecm_sharpness_enable(3);
					amvecm_sharpness_enable(5);
					amvecm_sharpness_enable(7);
					amvecm_sharpness_enable(9);
					amvecm_sharpness_enable(11);
					amvecm_sharpness_enable(13);
				}
			}
			break;
		case pq_module_sr_peaking:
			if (val2) {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(1, 1);
				else
					amvecm_sharpness_enable(0);
			} else {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(1, 1);
				else
					amvecm_sharpness_enable(1);
			}
			break;
		case pq_module_sr_lcti:
			if (val2) {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(2, 1);
				else
					amvecm_sharpness_enable(2);
			} else {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(2, 0);
				else
					amvecm_sharpness_enable(3);
			}
			break;
		case pq_module_sr_theta:
			if (val2)
				amvecm_sharpness_enable(4);
			else
				amvecm_sharpness_enable(5);
			break;
		case pq_module_sr_deband:
			if (val2)
				amvecm_sharpness_enable(6);
			else
				amvecm_sharpness_enable(7);
			break;
		case pq_module_sr_dejaggy:
			if (val2)
				amvecm_sharpness_enable(8);
			else
				amvecm_sharpness_enable(9);
			break;
		case pq_module_sr_dering:
			if (val2) {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(3, 1);
				else
					amvecm_sharpness_enable(10);
			} else {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(3, 0);
				else
					amvecm_sharpness_enable(11);
			}
			break;
		case pq_module_sr_drlpf:
			if (val2) {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(6, 1);
				else
					amvecm_sharpness_enable(12);
			} else {
				if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x)
					amve_sharpness_sub_ctrl(6, 0);
				else
					amvecm_sharpness_enable(13);
			}
			break;
		case pq_module_dnlp:
			if (val2)
				ve_enable_dnlp();
			else
				ve_disable_dnlp();
			break;
		case pq_module_cm:
			if (val2)
				cm_en = 1;
			else
				cm_en = 0;
			break;
		case pq_module_wb:
			if (!data_path)
				amvecm_wb_enable(val2);
			else
				amvecm_wb_enable_sub(val2);
			break;
		case pq_module_pre_gamma:
			amvecm_pre_gamma_enable(val2);
			break;
		case pq_module_gamma:
			if (val2) {
				if (!data_path)
					vecm_latch_flag |= FLAG_GAMMA_TABLE_EN; /* gamma off */
				else
					vecm_latch_flag |= FLAG_GAMMA_TABLE_EN_SUB;
			} else {
				if (!data_path)
					vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;/* gamma off */
				else
					vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS_SUB;
			}
			break;
		case pq_module_lc:
			if (val2)
				lc_en = 1;
			else
				lc_en = 0;
			break;
		case pq_module_black_ext:
			amvecm_black_ext_enable(val2);
			break;
		case pq_module_chroma_cor:
			vpp_datapath_config(CHROMA_CORING, val2, 0);
			break;
		case pq_module_dither:
			amvecm_dither_enable(val2);
			break;
		case pq_module_3dlut:
			lut3d_en = val2;
			vpp_enable_lut3d(val2);
			break;
		case pq_module_vadj1:
			amvecm_vadj_enable(VE_VADJ1, val2);
			break;
		case pq_module_vadj2:
			amvecm_vadj_enable(VE_VADJ2, val2);
			break;
#endif
		default:
			break;
		}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (!strcmp(parm[0], "hdr_top_ctrl_mode")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		set_hdr_top_ctrl_mode((int)val);
		vecm_latch_flag |= FLAG_COLORPRI_LATCH;
		force_toggle();
	} else if (!strcmp(parm[0], "gamut")) {
		if (!parm[1])
			goto free_buf;

		if (!strcmp(parm[1], "coef")) {
			/*parm[2] coef data (9 * 4 Bytes)*/
			string_len = strlen(parm[2]);
			if (string_len != 9 * 4) {
				pr_info("data length %d is not 36 Bytes.\n",
					string_len);
				goto free_buf;
			}

			/*data should be hex character*/
			for (i = 0; i < string_len; i++) {
				if ((parm[2][i] - '0') < 10 ||
					(parm[2][i] | 0x20) - 'a' < 6)
					continue;

				pr_info("error char, not hex.\n");
				goto free_buf;
			}

			char_count = (strlen(parm[2]) + 2) / 4;
			if (char_count < 9) {
				pr_info("coef_count = %d, not enough\n", char_count);
				goto free_buf;
			} else {
				char_count = 9;
			}

			memset(&hdr_gamut_data,
				0, sizeof(struct hdr_gamut_data_s));

			for (i = 0; i < char_count; i++) {
				char_val[0] = parm[2][4 * i + 0];
				char_val[1] = parm[2][4 * i + 1];
				char_val[2] = parm[2][4 * i + 2];
				char_val[3] = parm[2][4 * i + 3];
				char_val[4] = '\0';
				if (kstrtol(char_val, 16, &val) < 0) {
					pr_info("error parsing data.\n");
					goto free_buf;
				}
				temp = sizeof(long) * 8 - 16; /*convert to signed */
				hdr_gamut_data.coef[i] = (val << temp) >> temp;

			}

			set_hdr_gamut_coef(&hdr_gamut_data);
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
			pr_info("set hdr_gamut coef data success.\n");
		} else if (!strcmp(parm[1], "get_coef_str")) {
			stemp = kmalloc(20, GFP_KERNEL);
			if (!stemp)
				goto free_buf;

			memset(&hdr_gamut_data,
				0, sizeof(struct hdr_gamut_data_s));
			get_hdr_gamut_coef(&hdr_gamut_data);

			for (i = 0; i < 9; i++)
				int_convert_str(hdr_gamut_data.coef[i],
					i, stemp, 4, 16);

			for (i = 0; i < 3; i++)
				pr_info("%04x %04x %04x\n",
				       (hdr_gamut_data.coef[i * 3] & 0xffff),
				       (hdr_gamut_data.coef[i * 3 + 1] & 0xffff),
				       (hdr_gamut_data.coef[i * 3 + 2] & 0xffff));
			pr_info("hdr_gamut coef_str: %s\n", stemp);
			kfree(stemp);
		}
	} else if (!strcmp(parm[0], "hdr_mtrx")) {
		if (!parm[1])
			goto free_buf;

		memset(&hdr_mtrx_data, 0, sizeof(struct hdr_mtrx_data_s));

		if (!strcmp(parm[1], "in"))
			hdr_mtrx_data.mtrx_type = MATRIX_MODE_IN;
		else if (!strcmp(parm[1], "out"))
			hdr_mtrx_data.mtrx_type = MATRIX_MODE_OUT;
		else
			goto free_buf;

		if (!strcmp(parm[2], "coef")) {
			/*parm[3] coef data (15 * 4 Bytes)*/
			string_len = strlen(parm[3]);
			if (string_len != 15 * 4) {
				pr_info("data length %d is not 60 Bytes.\n",
					string_len);
				goto free_buf;
			}

				/*data should be hex character*/
			for (i = 0; i < string_len; i++) {
				if ((parm[3][i] - '0') < 10 ||
					(parm[3][i] | 0x20) - 'a' < 6)
					continue;

				pr_info("error char, not hex.\n");
				goto free_buf;
			}

			char_count = (strlen(parm[3]) + 2) / 4;
			if (char_count < 15) {
				pr_info("coef_count = %d, not enough\n", char_count);
				goto free_buf;
			} else {
				char_count = 15;
			}

			memset(coef, 0, sizeof(coef));

			for (i = 0; i < char_count; i++) {
				char_val[0] = parm[3][4 * i + 0];
				char_val[1] = parm[3][4 * i + 1];
				char_val[2] = parm[3][4 * i + 2];
				char_val[3] = parm[3][4 * i + 3];
				char_val[4] = '\0';
				if (kstrtol(char_val, 16, &val) < 0) {
					pr_info("error parsing data.\n");
					goto free_buf;
				}
				coef[i] = val;
			}

			for (i = 0; i < 15; i++) {
				if (i < 9)
					hdr_mtrx_data.coef[i] = coef[i];
				else if (i < 12)
					hdr_mtrx_data.pos_offset[i - 9] = coef[i];
				else
					hdr_mtrx_data.pre_offset[i - 12] = coef[i];
			}

			set_hdr_mtrx_coef(&hdr_mtrx_data);
			force_toggle();
			pr_info("set hdr_mtrx data success.\n");
		} else if (!strcmp(parm[2], "get_coef_str")) {
			stemp = kmalloc(20, GFP_KERNEL);
			if (!stemp)
				goto free_buf;

			get_hdr_mtrx_coef(&hdr_mtrx_data);

			for (i = 0; i < 15; i++) {
				if (i < 9)
					coef[i] = hdr_mtrx_data.coef[i];
				else if (i < 12)
					coef[i] = hdr_mtrx_data.pos_offset[i - 9];
				else
					coef[i] = hdr_mtrx_data.pre_offset[i - 12];
			}

			for (i = 0; i < 15; i++)
				d_convert_str(coef[i], i, stemp, 4, 16);

			pr_info("hdr_mtrx coef_str: %s\n", stemp);
			kfree(stemp);
		}
	} else if (!strcmp(parm[0], "eo_y_lut_hdr_sdr_tool_write")) {
		set_hdr_top_ctrl_mode(6);
		force_toggle();
	} else if (!strcmp(parm[0], "oo_y_lut_hdr_sdr_tool_write")) {
		set_hdr_top_ctrl_mode(7);
		force_toggle();
	} else if (!strcmp(parm[0], "oe_y_lut_hdr_sdr_tool_write")) {
		set_hdr_top_ctrl_mode(8);
		force_toggle();
	} else if (!strcmp(parm[0], "hdr_gamut_tool_write")) {
		set_hdr_top_ctrl_mode(2);
		hdr_tool_matrix_mode = 1;
		force_toggle();
	} else if (!strcmp(parm[0], "hdr_in_mtx_tool_write")) {
		set_hdr_top_ctrl_mode(3);
		hdr_tool_matrix_mode = 1;
		force_toggle();
	} else if (!strcmp(parm[0], "hdr_out_mtx_tool_write")) {
		set_hdr_top_ctrl_mode(4);
		hdr_tool_matrix_mode = 1;
		force_toggle();
	} else if (!strcmp(parm[0], "osd_mtx_en")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		amvecm_set_osd_mtx_enable(val);
	} else if (!strcmp(parm[0], "fmeter_version")) {
		pr_info("fmeter driver version : %s\n", FMETER_VERSION);
	} else if (!strcmp(parm[0], "read_pre_sat")) {
		pr_info("pre_sat enable = %d, gain = %d\n",
			pre_sat_data.enable, pre_sat_data.gain);
	} else if (!strcmp(parm[0], "write_pre_sat")) {
		if (!parm[1] || !parm[2])
			goto free_buf;

		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		if (kstrtoul(parm[2], 10, &val2) < 0)
			goto free_buf;
		pre_sat_data.enable = val;
		pre_sat_data.gain = val2;

		pr_info("write pre_sat enable = %d, gain = %d\n",
			pre_sat_data.enable, pre_sat_data.gain);
		vecm_latch_flag2 |= FLAG_PRE_SAT_UPDATE;
	} else if (!strcmp(parm[0], "minmax")) {
		get_luma_minmax_64bin_fw();
		get_luma_minmax_4bin_fw();
	} else if (!strcmp(parm[0], "g_hdr_set_on")) {
		pr_info("hdr_set_on: %d\n", hdr_set_on);
	} else if (!strcmp(parm[0], "s_hdr_set_on")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		hdr_set_on = val;
		vecm_latch_flag2 |= FLAG_HDR_ON;
		force_toggle();
	} else if (!strcmp(parm[0], "g_hdr_on")) {
		hdr_on = get_hdr_on();
		pr_info("hdr_on: %d\n", hdr_on);
	} else if (!strncmp(parm[0], "skin", 4)) {
		if (!strncmp(parm[1], "enable", 6)) {
			set_skin_api(1);
			pr_info("enable set skin\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			set_skin_api(0);
			pr_info("disable set skin\n");
		}
	} else if (!strcmp(parm[0], "get_skin")) {
		temp = get_skin_api();
		pr_info("get_skin = %d\n", temp);
	} else if (!strncmp(parm[0], "set_dummy", 9)) {
		if (!strncmp(parm[1], "enable", 6)) {
			set_dummy_flag = 1;
			force_toggle();
			pr_info("enable set_dummy\n");
		} else if (!strncmp(parm[1], "disable", 7)) {
			set_dummy_flag = 0;
			force_toggle();
			pr_info("disable set_dummy\n");
		}
	} else if (!strcmp(parm[0], "g_hdr10p_set_on")) {
		pr_info("hdr10p_set_on: %d\n", hdr10p_set_on);
	} else if (!strcmp(parm[0], "s_hdr10p_set_on")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		hdr10p_set_on = val;
		vecm_latch_flag2 |= FLAG_HDR10P_ON;
		force_toggle();
	} else if (!strcmp(parm[0], "min_mc")) {
		struct hdr_path_mux_sel_s *p = &h_p_s;

		if (!parm[1]) {
			pr_info("miss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		p->min_mc = val;
		pr_info("min_mc = %d\n", p->min_mc);
	} else if (!strcmp(parm[0], "cts_output")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		if (kstrtoul(parm[2], 10, &val2) < 0)
			goto free_buf;
		if (kstrtoint(parm[2], 10, &temp) < 0)
			goto free_buf;
		amvecm_probe_output_cts(val, val2, temp);
	} else if (!strcmp(parm[0], "g_hdr_set_on")) {
		pr_info("hdr_set_on: %d\n", hdr_set_on);
	} else if (!strcmp(parm[0], "s_hdr_set_on")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		hdr_set_on = val;
		vecm_latch_flag2 |= FLAG_HDR_ON;
		force_toggle();
	} else if (!strcmp(parm[0], "g_hdr_on")) {
		hdr_on = get_hdr_on();
		pr_info("hdr_on: %d\n", hdr_on);
	} else if (!strcmp(parm[0], "s_hdr_eotf_type")) {
		if (!parm[1]) {
			pr_info("misss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		force_eotf_type = val;
		force_oetf_type = val;
		vecm_latch_flag2 |= FLAG_HDR_OOTF_LATCH;
	} else if (!strcmp(parm[0], "hdr_core_test")) {
		if (!parm[1] ||
			kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		hdr_core_test_sel = val;
		if (!parm[2] ||
			kstrtoul(parm[2], 10, &val) < 0)
			goto free_buf;
		hdr_core_test_mode = val;
		if (!parm[3] ||
			kstrtoul(parm[3], 10, &val) < 0)
			goto free_buf;
		hdr_core_test_flag = val;
	} else if (!strcmp(parm[0], "ll_hist_mode")) {
		if (!parm[1]) {
			pr_info("miss param1\n");
			goto free_buf;
		}
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto free_buf;
		ll_hist_mode = val;
		pr_info("ll_hist_mode = %d\n", ll_hist_mode);
	} else if (!strcmp(parm[0], "3dlut_write")) {
		if (!parm[1] || !parm[2]) {
			pr_info("Usage: 3dlut_write r_idx hex_str\n");
			goto free_buf;
		}

		hex_str = parm[2];

		if (kstrtoul(parm[1], 10, &r_index) < 0)
			goto free_buf;

		if (r_index >= 17) {
			pr_info("r_index must 0~16\n");
			goto free_buf;
		}

		const int per_point_hex = 9;
		const int point_cnt = 17 * 17;
		const int need_hex_len = point_cnt * per_point_hex;
		int hex_len = strlen(hex_str);

		if (hex_len != need_hex_len) {
			pr_info("hex len err: expect %d, input %d\n",
				need_hex_len, hex_len);
			goto free_buf;
		}

		for (i = 0; i < point_cnt; i++) {
			char tmp[4];
			unsigned long val;
			int base_off = i * 9;

			/* parse R: base+0~2 */
			memcpy(tmp, hex_str + base_off + 0, 3);
			tmp[3] = '\0';
			if (kstrtoul(tmp, 16, &val) < 0) {
				pr_info("parse R fail at point %d\n", i);
				goto free_buf;
			}
			g_lut3d_tmp_buf[i * 3 + 0] = val & 0xFFF;

			/* parse G: base+3~5 */
			memcpy(tmp, hex_str + base_off + 3, 3);
			tmp[3] = '\0';
			if (kstrtoul(tmp, 16, &val) < 0) {
				pr_info("parse G fail at point %d\n", i);
				goto free_buf;
			}
			g_lut3d_tmp_buf[i * 3 + 1] = val & 0xFFF;

			/* parse B: base+6~8 */
			memcpy(tmp, hex_str + base_off + 6, 3);
			tmp[3] = '\0';
			if (kstrtoul(tmp, 16, &val) < 0) {
				pr_info("parse B fail at point %d\n", i);
				goto free_buf;
			}
			g_lut3d_tmp_buf[i * 3 + 2] = val & 0xFFF;
		}

		unsigned int ctltemp;

		WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, 0, 1, 1);
		ctltemp = READ_VPP_REG(VPP_LUT3D_CTRL);
		WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp & 0xFFFFFFFE);
		usleep_range(16000, 16001);

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);

		for (i = 0; i < 17 * 17; i++) {
			unsigned char g_idx = i / 17;
			unsigned char b_idx = i % 17;

			unsigned int ram_addr = (r_index << 16) | (g_idx << 8) | b_idx;

			WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, ram_addr);

			r = g_lut3d_tmp_buf[i * 3 + 0];
			g = g_lut3d_tmp_buf[i * 3 + 1];
			b = g_lut3d_tmp_buf[i * 3 + 2];

			WRITE_VPP_REG(VPP_LUT3D_RAM_DATA, ((g & 0xFFF) << 16) | (b & 0xFFF));
			WRITE_VPP_REG(VPP_LUT3D_RAM_DATA, (r & 0xFFF));
		}

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
		WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp);
	} else if (!strcmp(parm[0], "3dlut_read")) {
		if (!parm[1]) {
			pr_info("Usage: parm fail\n");
			goto free_buf;
		}

		if (kstrtoul(parm[1], 10, &r_index) < 0)
			goto free_buf;

		if (r_index >= 17) {
			pr_info("r_index must 0~16\n");
			goto free_buf;
		}

		unsigned int ctltemp;

		WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, 0, 1, 1);
		ctltemp = READ_VPP_REG(VPP_LUT3D_CTRL);
		WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp & 0xFFFFFFFE);
		usleep_range(16000, 16001);

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);

		for (i = 0; i < 17 * 17; i++) {
			unsigned char g_idx = i / 17;
			unsigned char b_idx = i % 17;
			unsigned int ram_addr = (1 << 31) | (r_index << 16) | (g_idx << 8) | b_idx;

			WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, ram_addr);

			reg_gb = READ_VPP_REG(VPP_LUT3D_RAM_DATA);

			g_lut3d_tmp_buf[i * 3 + 1] = (reg_gb >> 16) & 0xFFF; // G
			g_lut3d_tmp_buf[i * 3 + 2] = reg_gb & 0xFFF;	     // B
			unsigned int reg_r = READ_VPP_REG(VPP_LUT3D_RAM_DATA);

			g_lut3d_tmp_buf[i * 3 + 0] = reg_r & 0xFFF;// R
		}

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
		WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp);

		for (i = 0; i < 17 * 17; i++) {
			int pos = i * 3;

			pr_info("%03x%03x%03x\n",
				g_lut3d_tmp_buf[pos + 0],   // R
				g_lut3d_tmp_buf[pos + 1],   // G
				g_lut3d_tmp_buf[pos + 2]);  // B
		}
#endif
	} else {
		pr_info("unsupport cmd\n");
	}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	vpp_pst_hist_sta_config(1, HIST_MAXRGB,
		AFTER_POST2_MTX, vinfo);
#endif

	kfree(buf_orig);
	return count;

free_buf:
	kfree(buf_orig);
	return -EINVAL;
}

static const char *amvecm_reg_usage_str = {
	"Usage:\n"
	"echo rv addr(H) > /sys/class/amvecm/reg;\n"
	"echo rc addr(H) > /sys/class/amvecm/reg;\n"
	"echo rh addr(H) > /sys/class/amvecm/reg; read hiu reg\n"
	"echo wv addr(H) value(H) > /sys/class/amvecm/reg; write vpu reg\n"
	"echo wc addr(H) value(H) > /sys/class/amvecm/reg; write cbus reg\n"
	"echo wh addr(H) value(H) > /sys/class/amvecm/reg; write hiu reg\n"
	"echo dv|c|h addr(H) num > /sys/class/amvecm/reg; dump reg from addr\n"
};

static ssize_t amvecm_reg_show(const struct class *cla,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", amvecm_reg_usage_str);
}

static ssize_t amvecm_reg_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	long val = 0;
	unsigned int reg_addr, reg_val, i;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!parm[1]) {
		pr_info("unsupport cmd, parm[1] == NULL!\n");
		kfree(buf_orig);
		return -EINVAL;
	}
	if (!strcmp(parm[0], "rv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		reg_val = READ_VPP_REG_EX(reg_addr, 0);
		pr_info("VPU[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "rc")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		reg_val = aml_read_cbus(reg_addr);
		pr_info("CBUS[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "rh")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		amvecm_hiu_reg_read(reg_addr, &reg_val);
		pr_info("HIU[0x%04x]=0x%08x\n", reg_addr, reg_val);
	} else if (!strcmp(parm[0], "wv")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (!parm[2]) {
			pr_info("unsupport cmd,parm[2] == NULL!\n");
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_val = val;
		WRITE_VPP_REG_EX(reg_addr, reg_val, 0);
	} else if (!strcmp(parm[0], "wc")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (!parm[2]) {
			pr_info("unsupport cmd,parm[2] == NULL!\n");
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_val = val;
		aml_write_cbus(reg_addr, reg_val);
	} else if (!strcmp(parm[0], "wh")) {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (!parm[2]) {
			pr_info("unsupport cmd,parm[2] == NULL!\n");
			kfree(buf_orig);
			return -EINVAL;
		}
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_val = val;
		amvecm_hiu_reg_write(reg_addr, reg_val);
	} else if (parm[0][0] == 'd') {
		if (kstrtoul(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		reg_addr = val;
		if (kstrtoul(parm[2], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		for (i = 0; i < val; i++) {
			if (parm[0][1] == 'v') {
				reg_val = READ_VPP_REG_EX(reg_addr + i, 0);
			} else if (parm[0][1] == 'c') {
				reg_val = aml_read_cbus(reg_addr + i);
			} else if (parm[0][1] == 'h') {
				amvecm_hiu_reg_read((reg_addr + i),
						    &reg_val);
			} else {
				pr_info("unsupport cmd!\n");
				kfree(buf_orig);
				return -EINVAL;
			}
			pr_info("REG[0x%04x]=0x%08x\n",
				(reg_addr + i), reg_val);
		}
	} else {
		pr_info("unsupport cmd!\n");
	}
	kfree(buf_orig);
	return count;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void amvecm_slt_hdr_enable(void)
{
	set_hdr_policy(2);
	set_force_output(BT2020_PQ);
	force_toggle();
}

void amvecm_slt_hdr_disable(void)
{
	set_hdr_policy(0);
	set_force_output(0);
	force_toggle();
}

void amvecm_slt_cm_enable(void)
{
	if (chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d)
		return;

	cm_en = 1;
	cm_load_reg(&slt_cm_default);
	amcm_enable(0, 0);
}

void amvecm_slt_fmeter_enable(void)
{
	fmeter_slt = 1;
}

void amvecm_slt_fmeter_disable(void)
{
	fmeter_slt = 0;
}

void amvecm_slt_sr_enable(void)
{
	int sr_s1_oft = 0;
	int i = 0;

	if (chip_type_id == chip_t3x)
		sr_s1_oft = 0x2500;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		amve_safa_demo_ctrl(1);

		amve_sharpness_sub_ctrl(0, 1);
		amve_sharpness_sub_ctrl(1, 1);
		amve_sharpness_sub_ctrl(2, 1);
		amve_sharpness_sub_ctrl(3, 1);
		amve_sharpness_sub_ctrl(4, 1);
		amve_sharpness_sub_ctrl(5, 1);
		amve_sharpness_sub_ctrl(6, 1);
	} else {
		if (chip_type_id == chip_t3x) {
			cm_load_reg(&slt_sr0_default);
			cm_load_reg(&slt_sr1_default);

			memcpy(&amregs_store, &slt_sr0_default, sizeof(slt_sr0_default));
			for (i = 0; i < amregs_store.length; i++)
				amregs_store.am_reg[i].addr += sr_s1_oft;
			cm_load_reg(&amregs_store);

			memcpy(&amregs_store, &slt_sr1_default, sizeof(slt_sr1_default));
			for (i = 0; i < amregs_store.length; i++)
				amregs_store.am_reg[i].addr += sr_s1_oft;
			cm_load_reg(&amregs_store);
		} else {
			amvecm_slt_fmeter_enable();
			cm_load_reg(&slt_sr0_default);
			cm_load_reg(&slt_sr1_default);
		}

		if (chip_type_id == chip_t3x) {
			amvecm_sharpness_enable(0);
		} else {
			WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE, 1, 1, 1);
			if (chip_type_id != chip_txhd2)
				WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE, 1, 1, 1);
		}

		amvecm_sharpness_enable(2);
		amvecm_sharpness_enable(8);
		amvecm_sharpness_enable(10);
		amvecm_sharpness_enable(12);
		amvecm_sharpness_enable(4);
		amvecm_sharpness_enable(6);

		WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN, 0x3f, 0, 6);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN, 0x7, 8, 3);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKDRT_BLD_EN, 1, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT, 3, 2, 2);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT, 3, 12, 2);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_XTI_SDFDEN, 3, 0, 2);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_TI_BPF_EN, 0xf, 0, 4);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKLONG_PF_EN, 3, 0, 2);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_CC_PK_ADJ, 1, 24, 1);
		WRITE_VPP_REG_BITS(SRSHARP0_SR7_GRAPHIC_CTRL, 1, 10, 1);

		WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN, 0x3f, 0, 6);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN, 0x7, 8, 3);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKDRT_BLD_EN, 1, 0, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT, 3, 2, 2);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT, 3, 12, 2);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_XTI_SDFDEN, 3, 0, 2);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_TI_BPF_EN, 0xf, 0, 4);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKLONG_PF_EN, 3, 0, 2);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_CC_PK_ADJ, 1, 24, 1);
		WRITE_VPP_REG_BITS(SRSHARP1_SR7_GRAPHIC_CTRL, 1, 10, 1);

		if (chip_type_id == chip_t3x) {
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN + sr_s1_oft, 0x3f, 0, 6);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_DRTLPF_EN + sr_s1_oft, 0x7, 8, 3);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKDRT_BLD_EN + sr_s1_oft, 1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT + sr_s1_oft, 3, 2, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TIBLD_PRT + sr_s1_oft, 3, 12, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_XTI_SDFDEN + sr_s1_oft, 3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_TI_BPF_EN + sr_s1_oft, 0xf, 0, 4);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_PKLONG_PF_EN + sr_s1_oft, 3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_CC_PK_ADJ + sr_s1_oft, 1, 24, 1);
			WRITE_VPP_REG_BITS(SRSHARP0_SR7_GRAPHIC_CTRL + sr_s1_oft, 1, 10, 1);

			WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN + sr_s1_oft, 0x3f, 0, 6);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_DRTLPF_EN + sr_s1_oft, 0x7, 8, 3);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKDRT_BLD_EN + sr_s1_oft, 1, 0, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT + sr_s1_oft, 3, 2, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TIBLD_PRT + sr_s1_oft, 3, 12, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_XTI_SDFDEN + sr_s1_oft, 3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_TI_BPF_EN + sr_s1_oft, 0xf, 0, 4);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_PKLONG_PF_EN + sr_s1_oft, 3, 0, 2);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_CC_PK_ADJ + sr_s1_oft, 1, 24, 1);
			WRITE_VPP_REG_BITS(SRSHARP1_SR7_GRAPHIC_CTRL + sr_s1_oft, 1, 10, 1);
		}
	}
}

void amvecm_slt_dnlp_enable(void)
{
	if (chip_type_id == chip_s7)
		return;

	ve_enable_dnlp();
	dnlp_en_dsw = 0;
	ve_dnlp_tgt_init(1);
	ve_dnlp_calculate_reg();
	ve_dnlp_load_reg();
}

void amvecm_slt_dnlp_disable(void)
{
	if (chip_type_id == chip_s7)
		return;

	dnlp_en_dsw = 1;
}

void amvecm_slt_lc_enable(void)
{
	if (chip_type_id == chip_s6 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d)
		return;
	lc_slt_en(1);
}

void amvecm_slt_lc_disable(void)
{
	if (chip_type_id == chip_s6 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d)
		return;
	lc_slt_en(0);
}

void amvecm_slt_ble_enable(void)
{
	if (chip_type_id == chip_s5 || chip_type_id == chip_t3x) {
		ve_ble_ctl(0, 1, 0);
		WRITE_VPP_REG(0x2e64, 0x4621201c);
		WRITE_VPP_REG(0x2864, 0x4621201c);
	} else {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 3, 1);
		WRITE_VPP_REG(VPP_BLACKEXT_CTRL, 0x4621201c);
	}
}

void amvecm_slt_cc_enable(void)
{
	if (chip_type_id == chip_s5 || chip_type_id == chip_t3x) {
		ve_cc_ctl(0, 1, 0);
		WRITE_VPP_REG(0x2e62, 0x02000c0d);
		WRITE_VPP_REG(0x2862, 0x02000c0d);
	} else {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 4, 1);
		WRITE_VPP_REG(VPP_CCORING_CTRL, 0x02000c0d);
	}
}

void amvecm_slt_bs_enable(void)
{
	u32 temp;

	if (chip_type_id == chip_s5) {
		ve_bs_ctl(0, 1, 0);
		temp = READ_VPP_REG(VPP_BLUE_STRETCH_1);
		temp = (temp & 0x80000000) | 0x70400040;
		WRITE_VPP_REG(VPP_BLUE_STRETCH_1, temp);
		WRITE_VPP_REG(VPP_BLUE_STRETCH_2, 0x71007100);
		WRITE_VPP_REG(VPP_BLUE_STRETCH_3, 0x62006200);
	} else if (chip_type_id == chip_t3x) {
		WRITE_VPP_REG(0x2e78, 0xe0100860);
		WRITE_VPP_REG(0x2e79, 0x001f0020);
		WRITE_VPP_REG(0x2e7a, 0x001f0020);
		WRITE_VPP_REG(0x2e7b, 0x00100800);
		WRITE_VPP_REG(0x2e7c, 0x00100800);
		WRITE_VPP_REG(0x2878, 0xe0100860);
		WRITE_VPP_REG(0x2879, 0x001f0020);
		WRITE_VPP_REG(0x287a, 0x001f0020);
		WRITE_VPP_REG(0x287b, 0x00100800);
		WRITE_VPP_REG(0x287c, 0x00100800);
	} else {
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 1, 0, 1);
		WRITE_VPP_REG(VPP_BLUE_STRETCH_1, 0x70400040);
		WRITE_VPP_REG(VPP_BLUE_STRETCH_2, 0x71007100);
		WRITE_VPP_REG(VPP_BLUE_STRETCH_3, 0x62006200);
	}
}

void amvecm_slt_bs_disable(void)
{
	if (chip_type_id == chip_s5 || chip_type_id == chip_t3x)
		ve_bs_ctl(0, 0, 0);
	else
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 0, 1);
}

void amvecm_slt_vadj1_enable(void)
{
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_vadj_ctl(0, VE_VADJ1, 1, 0);
	} else {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
		else
			WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 0, 1);
	}
	vd1_brightness = 0;
	vd1_contrast = 0;
	hue_pre = 0;
	saturation_pre = 0;
	vecm_latch_flag |= FLAG_VADJ1_BRI;
	vecm_latch_flag |= FLAG_VADJ1_CON;
	pq_user_latch_flag |= PQ_USER_CMS_SAT_HUE;
}

void amvecm_slt_vadj2_enable(void)
{
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		WRITE_VPP_REG_BITS(VOUT_VADJ_MISC, 1, 0, 1);
	} else if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_vadj_ctl(0, VE_VADJ2, 1, 0);
	} else {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, 1, 0, 1);
		else
			WRITE_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 2, 1);
	}
	vd1_brightness2 = 0;
	vd1_contrast2 = 0;
	hue_post = 0;
	saturation_post = 0;
	amvecm_set_brightness2(vd1_brightness2);
	amvecm_set_contrast2(vd1_contrast2);
	amvecm_set_saturation_hue_post(saturation_post, hue_post);
}

void amvecm_slt_wb_enable(void)
{
	amvecm_wb_init(1);

	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		post_wb_ctl(0, 1, 0);
	else
		amvecm_wb_enable(1);
}

void amvecm_slt_gamma_enable(void)
{
	int i;
	unsigned short data[257];

	if (chip_cls_id == STB_CHIP)
		return;

	for (i = 0; i < 257; i++) {
		data[i] = i << 2;
		if (data[i] >= (1 << 10))
			data[i] = (1 << 10) - 1;
		video_gamma_table_r.data[i] = data[i];
		video_gamma_table_g.data[i] = data[i];
		video_gamma_table_b.data[i] = data[i];
	}

	vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
	vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
}

void amvecm_slt_pre_gamma_enable(void)
{
	int i;

	if (chip_type_id == chip_t3x) {
		set_gamma_regs(1, 0, 0);
		amvecm_pre_gamma_enable(1);
		return;
	}

	if (chip_type_id != chip_s6 &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d &&
		chip_type_id != chip_t6w &&
		chip_type_id != chip_t6x)
		return;

	pre_gamma.en = 1;

	for (i = 0; i < 64; i++) {
		pre_gamma.lut_r[i] = i << 4;
		pre_gamma.lut_g[i] = i << 4;
		pre_gamma.lut_b[i] = i << 4;
	}
	pre_gamma.lut_r[64] = 1023;
	pre_gamma.lut_g[64] = 1023;
	pre_gamma.lut_b[64] = 1023;

	vecm_latch_flag2 |= VPP_PRE_GAMMA_UPDATE;
}

void amvecm_slt_pre_gamma_disable(void)
{
	if (chip_type_id == chip_t3x) {
		amvecm_pre_gamma_enable(0);
		return;
	}

	if (chip_type_id != chip_s6 &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d)
		return;

	pre_gamma.en = 0;
	vecm_latch_flag2 |= VPP_PRE_GAMMA_UPDATE;
}

void amvecm_slt_viu2_pre_gamma_enable(void)
{
	if (chip_type_id != chip_s6)
		return;

	set_viu2_gamma_regs(1, 1);
}

void amvecm_slt_viu2_pre_gamma_disable(void)
{
	if (chip_type_id != chip_s6)
		return;

	set_viu2_gamma_regs(0, 1);
}

void amvecm_slt_lut3d_enable(void)
{
	if (chip_type_id == chip_s7 || chip_type_id == chip_s7d)
		return;

	slt_lut3d_en = lut3d_en;
	lut3d_en = 1;
	if (!slt_lut3d_en) {
		vpp_lut3d_table_init(-1, -1, -1);
		vpp_set_lut3d(0, 0, 0, 0, 0);
		vpp_lut3d_table_release();
	}
	vpp_enable_lut3d(1);
}

void amvecm_slt_lut3d_disable(void)
{
	if (chip_type_id == chip_s7 || chip_type_id == chip_s7d)
		return;

	lut3d_en = slt_lut3d_en;
	if (!lut3d_en)
		vpp_enable_lut3d(0);
}

void amvecm_slt_enable(void)
{
	slt_en = 1;
	pq_load_en = 0;
	vecm_latch_flag = 0;
	vecm_latch_flag2 = 0;
	pq_user_latch_flag = 0;
	amvecm_slt_dnlp_enable();
	amvecm_slt_hdr_enable();
	amvecm_slt_cm_enable();
	amvecm_slt_sr_enable();
	amvecm_slt_lc_enable();
	amvecm_slt_ble_enable();
	amvecm_slt_cc_enable();
	amvecm_slt_bs_enable();
	amvecm_slt_vadj1_enable();
	amvecm_slt_vadj2_enable();
	amvecm_slt_lut3d_enable();
	amvecm_slt_wb_enable();
	amvecm_slt_gamma_enable();
	amvecm_slt_pre_gamma_enable();
	amvecm_slt_viu2_pre_gamma_enable();
	pr_info("slt enable\n");
}

void amvecm_slt_disable(void)
{
	slt_en = 0;
	pq_load_en = 1;

	amvecm_slt_hdr_disable();
	amvecm_slt_fmeter_disable();
	amvecm_slt_dnlp_disable();
	amvecm_slt_lc_disable();
	amvecm_slt_bs_disable();
	amvecm_slt_lut3d_disable();
	amvecm_slt_viu2_pre_gamma_disable();

	pq_user_latch_flag |= PQ_USER_PQ_MODULE_CTL;

	pr_info("slt disable\n");
}

void amvecm_list_module_status(void)
{
	unsigned int sharpness0_en = 0, sharpness0_en_s1 = 0;
	unsigned int sharpness1_en = 0, sharpness1_en_s1 = 0;
	unsigned int dnlp_en = 0, dnlp_en_s1 = 0;
	unsigned int lc_en = 0, lc_en_s1 = 0;
	unsigned int cm_en = 0, cm_en_s1 = 0;
	unsigned int chroma_coring_en, chroma_coring_en_s1;
	unsigned int black_ext_en, black_ext_en_s1;
	unsigned int blue_stretch_en, blue_stretch_en_s1;
	unsigned int vadj1_en, vadj1_en_s1;
	unsigned int vadj2_en, vadj2_en_s1;
	unsigned int wb_en, wb_en_s1;
	unsigned int gamma_en = 0;
	unsigned int lut3d_en = 0, lut3d_en_s1 = 0;
	unsigned int vd1_hdr_en, vd1_hdr_en_s1;
	unsigned int vd2_hdr_en;
	unsigned int pre_gamma_en = 0, pre_gamma_en_s1 = 0;
	unsigned int viu2_pre_gamma_en = 0;
	int temp;
	int addr_port;
	int data_port;
	struct cm_port_s cm_port;
	unsigned int reg_ctrl;
	unsigned int addr_offset_vd1 = 0;
	unsigned int addr_offset_vd2 = 0;
	unsigned int reg_lc = SRSHARP1_LC_TOP_CTRL;
	unsigned int oft_s1 = 0;

	if (chip_type_id == chip_t6d) {
		addr_offset_vd1 = 0x2a00;
		addr_offset_vd2 = 0x2a30;
		reg_lc = VPP_LC_MODE;
	} else if (chip_type_id == chip_t6w) {
		addr_offset_vd1 = 0x1400;
		addr_offset_vd2 = 0x2a30;
		reg_lc = VPP_LC_MODE;
	} else if (chip_type_id == chip_t6x) {
		addr_offset_vd1 = 0x1400;
		addr_offset_vd2 = 0x2ab0;
		reg_lc = VPP_LC_MODE;
	}

	/*hdr*/
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		oft_s1 = 0x100;
		vd1_hdr_en = READ_VPP_REG_BITS(0x25a0, 13, 1);
		vd2_hdr_en = READ_VPP_REG_BITS(0x3800, 13, 1);
		vd1_hdr_en_s1 = READ_VPP_REG_BITS(0x25a0 + oft_s1, 13, 1);
	} else {
		vd1_hdr_en = READ_VPP_REG_BITS(VD1_HDR2_CTRL + addr_offset_vd1, 13, 1);
		vd2_hdr_en = READ_VPP_REG_BITS(VD2_HDR2_CTRL + addr_offset_vd2, 13, 1);
	}

	/*cm*/
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		cm_port = get_cm_port();

		addr_port = cm_port.cm_addr_port[0];
		data_port = cm_port.cm_data_port[0];
		WRITE_VPP_REG(addr_port, 0x208);
		temp = READ_VPP_REG(data_port);
		cm_en = temp & 0x01;

		addr_port = cm_port.cm_addr_port[1];
		data_port = cm_port.cm_data_port[1];
		WRITE_VPP_REG(addr_port, 0x208);
		temp = READ_VPP_REG(data_port);
		cm_en_s1 = temp & 0x01;
	} else if (chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d) {
		WRITE_VPP_REG(VPP_CHROMA_ADDR_PORT, 0x208);
		temp = READ_VPP_REG(VPP_CHROMA_DATA_PORT);
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			cm_en = temp & 0x01;
		else
			cm_en = temp & 0x02;
	}

	/*sr*/
	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		sharpness0_en = READ_VPP_REG_BITS(VPP_SR_EN, 0, 1);
	} else if (chip_type_id == chip_t3x) {
		oft_s1 = 0x2500;
		sharpness0_en = READ_VPP_REG_BITS(0x5027, 1, 1);
		sharpness1_en = READ_VPP_REG_BITS(0x5227, 1, 1);
		sharpness0_en_s1 = READ_VPP_REG_BITS(0x5027 + oft_s1, 1, 1);
		sharpness1_en_s1 = READ_VPP_REG_BITS(0x5227 + oft_s1, 1, 1);
	} else {
		sharpness0_en = READ_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE, 1, 1);
		if (chip_type_id != chip_txhd2)
			sharpness1_en = READ_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE, 1, 1);
	}

	/*lc*/
	if (chip_type_id == chip_t3x) {
		reg_lc = 0x52c0;
		oft_s1 = 0x2500;
		lc_en = READ_VPP_REG_BITS(reg_lc, 4, 1);
		lc_en_s1 = READ_VPP_REG_BITS(reg_lc + oft_s1, 4, 1);
	} else if (chip_type_id != chip_s6 &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d) {
		lc_en = READ_VPP_REG_BITS(reg_lc, 4, 1);
	}

	/*dnlp*/
	if (chip_type_id == chip_t3x) {
		temp = READ_VPP_REG_S5(0x5245);
		dnlp_en = temp & 0x01;
		oft_s1 = 0x2500;
		temp = READ_VPP_REG_S5(0x5245 + oft_s1);
		dnlp_en_s1 = temp & 0x01;
	} else if (dnlp_sel == 2) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
			reg_ctrl = SRSHARP1_DNLP_EN;
		} else if (chip_type_id == chip_t6d ||
			chip_type_id == chip_s7d ||
			chip_type_id == chip_s6 ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x) {
			reg_ctrl = VPP_DNLP_EN_MODE;
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if ((!vinfo_lcd_support() && chip_type_id != chip_s5) ||
				is_meson_t7_cpu() ||
				chip_type_id == chip_txhd2)
				reg_ctrl = SRSHARP0_DNLP_EN;
			else
				reg_ctrl = SRSHARP1_DNLP_EN;
		} else {
			reg_ctrl = SRSHARP0_DNLP_EN;
		}

		if (chip_type_id != chip_t6d &&
			chip_type_id != chip_s7d &&
			chip_type_id != chip_s6 &&
			chip_type_id != chip_t6w &&
			chip_type_id != chip_t6x)
			dnlp_en = READ_VPP_REG_BITS(reg_ctrl, 0, 1);
		else if (chip_type_id != chip_s7)
			dnlp_en = READ_VPP_REG_BITS(reg_ctrl, 4, 1);
	} else {
		dnlp_en = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, DNLP_EN_BIT, DNLP_EN_BIT);
	}

	/*ble black_ext*/
	if (chip_type_id == chip_t3x) {
		reg_ctrl = 0x2863 + 0x600;
		black_ext_en = READ_VPP_REG_BITS(reg_ctrl, 3, 1);
		reg_ctrl = 0x2863;
		black_ext_en_s1 = READ_VPP_REG_BITS(reg_ctrl, 3, 1);
	} else {
		black_ext_en = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 3, 1);
	}

	/*cc chroma_coring*/
	if (chip_type_id == chip_t3x) {
		reg_ctrl = 0x2863 + 0x600;
		chroma_coring_en = READ_VPP_REG_BITS(reg_ctrl, 1, 1);
		reg_ctrl = 0x2863;
		chroma_coring_en_s1 = READ_VPP_REG_BITS(reg_ctrl, 1, 1);
	} else {
		chroma_coring_en = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 4, 1);
	}

	/*bs blue stretch*/
	if (chip_type_id == chip_s5) {
		reg_ctrl = 0x1dc0;
		blue_stretch_en = READ_VPP_REG_BITS(reg_ctrl, 31, 1);
	} else if (chip_type_id == chip_t3x) {
		reg_ctrl = 0x2878 + 0x600;
		blue_stretch_en = READ_VPP_REG_BITS(reg_ctrl, 31, 1);
		reg_ctrl = 0x2878;
		blue_stretch_en_s1 = READ_VPP_REG_BITS(reg_ctrl, 31, 1);
	} else {
		blue_stretch_en = READ_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, 1);
	}

	/*vadj1*/
	if (chip_type_id == chip_t3x) {
		reg_ctrl = 0x2880 + 0x600;
		vadj1_en = READ_VPP_REG_BITS(reg_ctrl, 0, 1);
		reg_ctrl = 0x2880;
		vadj1_en_s1 = READ_VPP_REG_BITS(reg_ctrl, 0, 1);
	} else {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			vadj1_en = READ_VPP_REG_BITS(VPP_VADJ1_MISC, 0, 1);
		else
			vadj1_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 0, 1);
	}

	/*vadj2*/
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		vadj2_en = READ_VPP_REG_BITS(VOUT_VADJ_MISC, 0, 1);
	} else if (chip_type_id == chip_t3x) {
		reg_ctrl = 0x2570;
		vadj2_en = READ_VPP_REG_BITS(reg_ctrl, 0, 1);
		oft_s1 = 0x100;
		vadj2_en_s1 = READ_VPP_REG_BITS(reg_ctrl + oft_s1, 0, 1);
	} else {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
			vadj2_en = READ_VPP_REG_BITS(VPP_VADJ2_MISC, 0, 1);
		else
			vadj2_en = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 2, 1);
	}

	/*3dlut*/
	if (chip_type_id == chip_t3x) {
		lut3d_en = READ_VPP_REG_BITS(0x2540, 0, 1);
		oft_s1 = 0x100;
		lut3d_en_s1 = READ_VPP_REG_BITS(0x2540 + oft_s1, 0, 1);
	} else if (chip_type_id != chip_s7 && chip_type_id != chip_s7d) {
		lut3d_en = READ_VPP_REG_BITS(VPP_LUT3D_CTRL, 0, 1);
	}

	/*wb*/
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		wb_en = READ_VPP_REG_BITS(VOUT_GAINOFF_CTRL0, 31, 1);
	} else if (chip_type_id == chip_t3x) {
		wb_en = READ_VPP_REG_BITS(0x2550, 31, 1);
		oft_s1 = 0x100;
		wb_en_s1 = READ_VPP_REG_BITS(0x2550 + oft_s1, 31, 1);
	} else if (video_rgb_ogo_xvy_mtx) {
		wb_en = READ_VPP_REG_BITS(VPP_MATRIX_CTRL, 6, 1);
	} else {
		wb_en = READ_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 31, 1);
	}

	/*gamma*/
	/*pre_gamma*/
	if (chip_cls_id == STB_CHIP || chip_type_id == chip_t3x ||
		chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
		if (chip_type_id == chip_t3x) {
			pre_gamma_en = post_pre_gamma_get(SLICE0, 0, 1);
			pre_gamma_en_s1 = post_pre_gamma_get(SLICE1, 0, 1);
		} else if (chip_type_id == chip_s6) {
			pre_gamma_en = READ_VPP_REG_BITS(VPP_GAMMA_CTRL, 0, 1);
			viu2_pre_gamma_en = READ_VPP_REG_BITS(VPP2_GAMMA_CTRL, 0, 1);
		} else if (chip_type_id == chip_s7 || chip_type_id == chip_s7d ||
			chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
			pre_gamma_en = READ_VPP_REG_BITS(VPP_GAMMA_CTRL, 0, 1);
		}
	}
	/*tv lcd_gamma*/
	if (chip_cls_id == TV_CHIP) {
		if (cpu_after_eq_t7()) {
			if (chip_type_id == chip_a4 ||
				chip_type_id == chip_c5)
				reg_ctrl = LCD_GAMMA_CNTL_PORT0_A4;
			else if (chip_type_id == chip_t3x)
				reg_ctrl = 0x14e9;
			else if (chip_type_id == chip_txhd2)
				reg_ctrl = L_GAMMA_CNTL_PORT;
			else if (chip_type_id == chip_t6x)
				reg_ctrl = 0x72e9;
			else
				reg_ctrl = LCD_GAMMA_CNTL_PORT0;
		} else {
			reg_ctrl = L_GAMMA_CNTL_PORT;
		}
		gamma_en = READ_VPP_REG_BITS(reg_ctrl, 0, 1);
	}

	pr_info("amvecm module status(0:disable;1:enable)\n");
	pr_info("vd1 hdr : %d\n", vd1_hdr_en);
	pr_info("vd2 hdr : %d\n", vd2_hdr_en);
	if (chip_type_id != chip_s7 && chip_type_id != chip_s7d)
		pr_info("cm : %d\n", cm_en);
	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		pr_info("sr : %d\n", sharpness0_en);
	} else {
		pr_info("sharpness0 : %d\n", sharpness0_en);
		if (chip_type_id != chip_txhd2 && chip_type_id != chip_s7)
			pr_info("sharpness1 : %d\n", sharpness1_en);
	}

	if (chip_type_id != chip_s6 &&
		chip_type_id != chip_s7 &&
		chip_type_id != chip_s7d)
		pr_info("lc : %d\n", lc_en);
	if (chip_type_id != chip_s7)
		pr_info("dnlp : %d\n", dnlp_en);
	pr_info("black_ext : %d\n", black_ext_en);
	pr_info("chroma_cor : %d\n", chroma_coring_en);
	pr_info("bs : %d\n", blue_stretch_en);
	pr_info("vadj1 : %d\n", vadj1_en);
	pr_info("vadj2 : %d\n", vadj2_en);
	if (chip_type_id != chip_s7 && chip_type_id != chip_s7d)
		pr_info("3dlut : %d\n", lut3d_en);
	pr_info("wb : %d\n", wb_en);
	if (chip_cls_id == STB_CHIP || chip_type_id == chip_t3x ||
		chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
		if (chip_type_id == chip_t3x) {
			pr_info("pre_gamma_en : %d\n", pre_gamma_en);
		} else if (chip_type_id == chip_s6) {
			pr_info("pre_gamma_en : %d\n", pre_gamma_en);
			pr_info("viu2_gamma_en : %d\n", viu2_pre_gamma_en);
		} else if (chip_type_id == chip_s7 || chip_type_id == chip_s7d ||
			chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
			pr_info("pre_gamma_en : %d\n", pre_gamma_en);
		}
	}
	if (chip_cls_id == TV_CHIP)
		pr_info("gamma : %d\n", gamma_en);

	if (chip_type_id == chip_t3x) {
		pr_info("slice1 vd1 hdr : %d\n", vd1_hdr_en_s1);
		pr_info("slice1 cm : %d\n", cm_en_s1);
		pr_info("slice1 sharpness0 : %d\n", sharpness0_en_s1);
		pr_info("slice1 sharpness1 : %d\n", sharpness1_en_s1);
		pr_info("slice1 lc : %d\n", lc_en_s1);
		pr_info("slice1 dnlp : %d\n", dnlp_en_s1);
		pr_info("slice1 black_ext : %d\n", black_ext_en_s1);
		pr_info("slice1 chroma_cor : %d\n", chroma_coring_en_s1);
		pr_info("slice1 bs : %d\n", blue_stretch_en_s1);
		pr_info("slice1 vadj1 : %d\n", vadj1_en_s1);
		pr_info("slice1 vadj2 : %d\n", vadj2_en_s1);
		pr_info("slice1 3dlut : %d\n", lut3d_en_s1);
		pr_info("slice1 wb : %d\n", wb_en_s1);
		pr_info("slice1 pre_gamma_en : %d\n", pre_gamma_en_s1);
	}

	pq_module_status[pq_module_sharpness] = sharpness0_en;
	pq_module_status[pq_module_dnlp] = dnlp_en;
	pq_module_status[pq_module_cm] = cm_en;
	pq_module_status[pq_module_wb] = wb_en;
	pq_module_status[pq_module_gamma] = gamma_en;
	pq_module_status[pq_module_lc] = lc_en;
	pq_module_status[pq_module_black_ext] = black_ext_en;
	pq_module_status[pq_module_chroma_cor] = chroma_coring_en;
	pq_module_status[pq_module_3dlut] = lut3d_en;
	pq_module_status[pq_module_vadj1] = vadj1_en;
	pq_module_status[pq_module_vadj2] = vadj2_en;
}

void amvecm_update_module_status(void)
{
	amvecm_list_module_status();
	if (chip_type_id == chip_s7d ||
	chip_type_id == chip_s6 ||
	chip_type_id == chip_t6d ||
	chip_type_id == chip_t6w ||
	chip_type_id == chip_t6x) {
		pq_module_status[pq_module_sr_peaking] =
			READ_VPP_REG_BITS(VPP_PK_EN, 0, 1);
		pq_module_status[pq_module_sr_lcti] =
			READ_VPP_REG_BITS(VPP_HTI_EN_MODE, 12, 1);
		pq_module_status[pq_module_sr_dering] =
			READ_VPP_REG_BITS(VPP_DERING_EN, 0, 1);
		pq_module_status[pq_module_sr_drlpf] =
			READ_VPP_REG_BITS(VPP_NR_LPF_EN, 24, 1);
	} else {
		pq_module_status[pq_module_sr_peaking] =
			READ_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE, 1, 1);
		pq_module_status[pq_module_sr_lcti] =
			READ_VPP_REG_BITS(SRSHARP0_HCTI_FLT_CLP_DC, 28, 1);
		pq_module_status[pq_module_sr_dering] =
			READ_VPP_REG_BITS(SRSHARP0_SR3_DERING_CTRL, 28, 3);
		pq_module_status[pq_module_sr_drlpf] =
			(READ_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN, 0, 3) == 0) ? 0 : 1;
	}
	pq_module_status[pq_module_sr_theta] =
		(READ_VPP_REG_BITS(SRSHARP0_SR3_DRTLPF_EN, 4, 3) == 0) ? 0 : 1;
	pq_module_status[pq_module_sr_deband] =
		READ_VPP_REG_BITS(SRSHARP0_DB_FLT_CTRL, 4, 1);
	pq_module_status[pq_module_sr_dejaggy] =
		READ_VPP_REG_BITS(SRSHARP0_DEJ_CTRL, 0, 1);
	pq_module_status[pq_module_dither] =
			READ_VPP_REG_BITS(VPP_VE_DITHER_CTRL, 0, 1);
	if (chip_type_id == chip_t3x)
		pq_module_status[pq_module_pre_gamma] =
			post_pre_gamma_get(SLICE0, 0, 1);
	else
		pq_module_status[pq_module_pre_gamma] =
			READ_VPP_REG_BITS(VPP_GAMMA_CTRL, 0, 1);
}

static const char *amvecm_slt_usage_str = {
	"Usage:\n"
	"echo enable > /sys/class/amvecm/slt_dbg;\n"
	"echo disable > /sys/class/amvecm/slt_dbg;\n"
	"echo pq_status > /sys/class/amvecm/slt_dbg;\n"
};

static ssize_t amvecm_slt_debug_show(const struct class *cla,
			       const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", amvecm_slt_usage_str);
}

static ssize_t amvecm_slt_debug_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;
	parse_param_amvecm(buf_orig, (char **)&parm);
	if (!strncmp(parm[0], "enable", 6))
		amvecm_slt_enable();
	else if (!strncmp(parm[0], "disable", 7))
		amvecm_slt_disable();
	else if (!strncmp(parm[0], "hdr_1", 5))
		amvecm_slt_hdr_enable();
	else if (!strncmp(parm[0], "hdr_0", 5))
		amvecm_slt_hdr_disable();
	else if (!strncmp(parm[0], "cm_1", 4))
		amvecm_slt_cm_enable();
	else if (!strncmp(parm[0], "sr_1", 4))
		amvecm_slt_sr_enable();
	else if (!strncmp(parm[0], "sr_0", 4))
		amvecm_slt_fmeter_disable();
	else if (!strncmp(parm[0], "lc_1", 4))
		amvecm_slt_lc_enable();
	else if (!strncmp(parm[0], "lc_0", 4))
		amvecm_slt_lc_disable();
	else if (!strncmp(parm[0], "dnlp_1", 6))
		amvecm_slt_dnlp_enable();
	else if (!strncmp(parm[0], "dnlp_0", 6))
		amvecm_slt_dnlp_disable();
	else if (!strncmp(parm[0], "bs_1", 4))
		amvecm_slt_bs_enable();
	else if (!strncmp(parm[0], "bs_0", 4))
		amvecm_slt_bs_disable();
	else if (!strncmp(parm[0], "lut3d_1", 7))
		amvecm_slt_lut3d_enable();
	else if (!strncmp(parm[0], "lut3d_0", 7))
		amvecm_slt_lut3d_disable();
	else if (!strncmp(parm[0], "ble_1", 5))
		amvecm_slt_ble_enable();
	else if (!strncmp(parm[0], "cc_1", 4))
		amvecm_slt_cc_enable();
	else if (!strncmp(parm[0], "wb_1", 4))
		amvecm_slt_wb_enable();
	else if (!strncmp(parm[0], "gamma_1", 7))
		amvecm_slt_gamma_enable();
	else if (!strncmp(parm[0], "pre_gamma_1", 11))
		amvecm_slt_pre_gamma_enable();
	else if (!strncmp(parm[0], "pre_gamma_0", 11))
		amvecm_slt_pre_gamma_disable();
	else if (!strncmp(parm[0], "viu2_pre_gamma_1", 16))
		amvecm_slt_viu2_pre_gamma_enable();
	else if (!strncmp(parm[0], "viu2_pre_gamma_0", 16))
		amvecm_slt_viu2_pre_gamma_disable();
	else if (!strncmp(parm[0], "vadj1_1", 7))
		amvecm_slt_vadj1_enable();
	else if (!strncmp(parm[0], "vadj2_1", 7))
		amvecm_slt_vadj2_enable();
	else if (!strncmp(parm[0], "pq_status", 9))
		amvecm_list_module_status();
	else
		pr_info("unsupport cmd!\n");

	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_get_hdr_type_show(const struct class *cla,
					const struct class_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%x\n", hdr_source_type);
}

static ssize_t amvecm_get_hdr_type_store(const struct class *cls,
					 const struct class_attribute *attr,
					 const char *buffer, size_t count)
{
	return count;
}

static ssize_t amvecm_hdr_test_case_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	pr_info("Usage:\n");
	pr_info("echo case_index > /sys/class/amvecm/hdr_test_case\n");
	pr_info("0: EN_HDR_TEST_CASE_CUVA_SDR_SC1\n");
	pr_info("1: EN_HDR_TEST_CASE_CUVAHLG_SDR_SC1\n");
	pr_info("2: EN_HDR_TEST_CASE_CUVAHLG_SDR_STATIC\n");
	pr_info("3: EN_HDR_TEST_CASE_HDR_SDR_SR\n");
	pr_info("4: EN_HDR_TEST_CASE_SDR_HDR_SR\n");
	pr_info("5: EN_HDR_TEST_CASE_VD2_CUVA_SDR_SC1\n");
	pr_info("6: EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_SC1\n");
	pr_info("7: EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_STATIC\n");
	pr_info("8: EN_HDR_TEST_CASE_VD2_HDR_SDR_SR\n");
	pr_info("9: EN_HDR_TEST_CASE_VD2_SDR_HDR_SR\n");
	pr_info("10: EN_HDR_TEST_CASE_VD1_1_CUVA_SDR_SC1\n");
	pr_info("11: EN_HDR_TEST_CASE_VD2_1_CUVA_SDR_SC1\n");
	pr_info("\n");
	return 0;
}

static ssize_t amvecm_hdr_test_case_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[2] = {NULL};
	int val;

	if (!buf || (chip_type_id != chip_t6w &&
		chip_type_id != chip_t6x && chip_type_id != chip_a9))
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);
	if (kstrtoint(parm[0], 10, &val) < 0)
		goto free_buf;

	if (val < 0 || val >= EN_HDR_TEST_CASE_MAX)
		goto free_buf;

	pr_info("%s: hdr test case %d\n", __func__, val);

	switch (val) {
	case EN_HDR_TEST_CASE_CUVA_SDR_SC1:
	default:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		break;
	case EN_HDR_TEST_CASE_CUVAHLG_SDR_SC1:
		test_case_reg = &reg_cuvahlg_sdr_sc1;
		test_case_lut = &lut_cuvahlg_sdr_sc1;
		break;
	case EN_HDR_TEST_CASE_CUVAHLG_SDR_STATIC:
		test_case_reg = &reg_cuvahlg_sdr_static;
		test_case_lut = &lut_cuvahlg_sdr_static;
		break;
	case EN_HDR_TEST_CASE_HDR_SDR_SR:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_HDR_SDR_SR;
		break;
	case EN_HDR_TEST_CASE_SDR_HDR_SR:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_SDR_HDR_SR;
		break;
	case EN_HDR_TEST_CASE_VD2_CUVA_SDR_SC1:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_VD2_CUVA_SDR_SC1;
		break;
	case EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_SC1:
		test_case_reg = &reg_cuvahlg_sdr_sc1;
		test_case_lut = &lut_cuvahlg_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_SC1;
		break;
	case EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_STATIC:
		test_case_reg = &reg_cuvahlg_sdr_static;
		test_case_lut = &lut_cuvahlg_sdr_static;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_VD2_CUVAHLG_SDR_STATIC;
		break;
	case EN_HDR_TEST_CASE_VD2_HDR_SDR_SR:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_VD2_HDR_SDR_SR;
		break;
	case EN_HDR_TEST_CASE_VD2_SDR_HDR_SR:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_VD2_SDR_HDR_SR;
		break;
	case EN_HDR_TEST_CASE_VD1_1_CUVA_SDR_SC1:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_VD1_1_CUVA_SDR_SC1;
		break;
	case EN_HDR_TEST_CASE_VD2_1_CUVA_SDR_SC1:
		test_case_reg = &reg_cuva_sdr_sc1;
		test_case_lut = &lut_cuva_sdr_sc1;
		hdr_test_case_set_flag = EN_HDR_TEST_CASE_VD2_1_CUVA_SDR_SC1;
		break;
	}

	muxio_config(VPP_MUXIO_SEL_VD1_HDR, 0, 0);

	pr_info("%s: VPP_MUXIO_SEL_VD1_HDR\n", __func__);

	if (!hdr_test_case_set_flag)
		hdr_test_case_set_flag = 1;

free_buf:
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_dpss_hdr_test_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	pr_info("Usage:\n");
	pr_info("echo case_index > /sys/class/amvecm/dpss_hdr_test\n");
	pr_info("0~5: DE_LINK/VD1/DPSS_HDR_DPE/_HDR/_DCT_HDR/LC_EVC/\n");
	return 0;
}

static ssize_t amvecm_dpss_hdr_test_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[2] = {NULL};
	int val;

	if (!buf || (chip_type_id != chip_t6w && chip_type_id != chip_t6x))
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);
	if (kstrtoint(parm[0], 10, &val) < 0)
		goto free_buf;

	if (val < 0)
		goto free_buf;

	pr_info("%s: dpss_hdr_test case %d\n", __func__, val);

	if (!dpss_test_case_set_flag) {
		dpss_test_case_index = val;
		dpss_test_case_set_flag = 1;
	}

free_buf:
	kfree(buf_orig);
	return count;
}

static ssize_t amvecm_gamut_mapping_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	get_gamut_mapping_wrapper();
	return 0;
}

static ssize_t amvecm_gamut_mapping_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	char *buf_orig, *parm[2] = {NULL};
	int val;
	long val2;
	int i = 0;
	int char_count = 0;
	char char_val[5] = {0};
	char *stemp = NULL;
	unsigned int string_len = 0;

	if (!buf ||  chip_type_id != chip_t6x)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!parm[0])
		goto free_buf;

	if (!strcmp(parm[0], "case0")) {
		gamut_test_case(0);
	} else if (!strcmp(parm[0], "case1")) {
		gamut_test_case(1);
	} else if (!strcmp(parm[0], "case2")) {
		gamut_test_case(2);
	} else if (!strcmp(parm[0], "case3")) {
		gamut_test_case(3);
	} else if (!strcmp(parm[0], "reset")) {
		gamut_test_case(4);
	} else if (!strcmp(parm[0], "check")) {
		vpp_check_lut3d();
	} else if (!strcmp(parm[0], "gamut1_en")) {
		if (kstrtoint(parm[1], 10, &val) < 0)
			goto free_buf;
		gamut_mapping1_en = val;
		vecm_latch_flag |= FLAG_COLORPRI_LATCH;
		force_toggle();
		pr_info("gamut_mapping1_en = %d\n", gamut_mapping1_en);
	} else if (!strcmp(parm[0], "gamut_mode")) {
		if (kstrtoint(parm[1], 10, &val) < 0)
			goto free_buf;
		gamut_mode = val;
		vecm_latch_flag |= FLAG_COLORPRI_LATCH;
		force_toggle();
	} else if (!strcmp(parm[0], "coef")) {
		/*parm[2] coef data (9 * 4 Bytes)*/
		string_len = strlen(parm[1]);
		if (string_len != 9 * 4) {
			pr_info("data length %d is not 36 Bytes.\n",
				string_len);
			goto free_buf;
		}

		/*data should be hex character*/
		for (i = 0; i < string_len; i++) {
			if ((parm[1][i] - '0') < 10 ||
				(parm[1][i] | 0x20) - 'a' < 6)
				continue;

			pr_info("error char, not hex.\n");
			goto free_buf;
		}

		char_count = (strlen(parm[1]) + 2) / 4;
		if (char_count < 9) {
			pr_info("coef_count = %d, not enough\n", char_count);
			goto free_buf;
		} else {
			char_count = 9;
		}

		memset(&force_gamut_mtx,
			0, sizeof(struct hdr_gamut_data_s));

		for (i = 0; i < char_count; i++) {
			char_val[0] = parm[1][4 * i + 0];
			char_val[1] = parm[1][4 * i + 1];
			char_val[2] = parm[1][4 * i + 2];
			char_val[3] = parm[1][4 * i + 3];
			char_val[4] = '\0';
			if (kstrtol(char_val, 16, &val2) < 0) {
				pr_info("error parsing data.\n");
				goto free_buf;
			}
			force_gamut_mtx.coef[i] = val2;
		}
		force_toggle();
		vecm_latch_flag |= FLAG_COLORPRI_LATCH;
		pr_info("set gamut coef data success.\n");
	} else if (!strcmp(parm[0], "get_coef_str")) {
		stemp = kmalloc(20, GFP_KERNEL);
		if (!stemp)
			goto free_buf;

		for (i = 0; i < 9; i++)
			int_convert_str(force_gamut_mtx.coef[i],
				i, stemp, 4, 16);
		for (i = 0; i < 3; i++)
			pr_info("%04x %04x %04x\n",
			       (force_gamut_mtx.coef[i * 3] & 0xffff),
			       (force_gamut_mtx.coef[i * 3 + 1] & 0xffff),
			       (force_gamut_mtx.coef[i * 3 + 2] & 0xffff));

		pr_info("hdr_gamut coef_str: %s\n", stemp);
		kfree(stemp);
	} else if (!strcmp(parm[0], "vpp_color_pri_sel")) {
		if (parm[1]) {
			if (kstrtoint(parm[1], 10, &val) < 0)
				goto free_buf;
			vpp_color_pri_sel = val;
			vecm_latch_flag |= FLAG_COLORPRI_LATCH;
			force_toggle();
			pr_info("set vpp_color_pri_sel = %d\n", vpp_color_pri_sel);
		}
	} else if (!strcmp(parm[0], "dma_case")) {
		if (!parm[1]) {
			pr_info("cur gamut_dma_case = %d\n", gamut_dma_case);
		} else {
			if (kstrtoint(parm[1], 10, &val) < 0)
				goto free_buf;
			gamut_dma_case = val;
		}
	} else if (!strcmp(parm[0], "coef0")) {
		/*parm[2] coef data (9 * 4 Bytes)*/
		string_len = strlen(parm[1]);
		if (string_len != 9 * 4) {
			pr_info("data length %d is not 36 Bytes.\n",
				string_len);
			goto free_buf;
		}

		/*data should be hex character*/
		for (i = 0; i < string_len; i++) {
			if ((parm[1][i] - '0') < 10 ||
				(parm[1][i] | 0x20) - 'a' < 6)
				continue;

			pr_info("error char, not hex.\n");
			goto free_buf;
		}

		char_count = (strlen(parm[1]) + 2) / 4;
		if (char_count < 9) {
			pr_info("coef_count = %d, not enough\n", char_count);
			goto free_buf;
		} else {
			char_count = 9;
		}

		memset(&force_gamut0_mtx,
			0, sizeof(struct hdr_gamut_data_s));

		for (i = 0; i < char_count; i++) {
			char_val[0] = parm[1][4 * i + 0];
			char_val[1] = parm[1][4 * i + 1];
			char_val[2] = parm[1][4 * i + 2];
			char_val[3] = parm[1][4 * i + 3];
			char_val[4] = '\0';
			if (kstrtol(char_val, 16, &val2) < 0) {
				pr_info("error parsing data.\n");
				goto free_buf;
			}
			force_gamut0_mtx.coef[i] = val2;
		}
		force_toggle();
		vecm_latch_flag |= FLAG_COLORPRI_LATCH;
		pr_info("set gamut0 coef data success.\n");
	}
free_buf:
		kfree(buf_orig);
		return count;
}

static ssize_t amvecm_gamut_mtrx_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	int i = 0;
	char stemp[37];
	struct hdr_gamut_data_s hdr_gamut_data;

	memset(&hdr_gamut_data,
		0, sizeof(struct hdr_gamut_data_s));
	get_hdr_gamut_coef(&hdr_gamut_data);

	memset(stemp, 0, sizeof(stemp));
	for (i = 0; i < 9; i++)
		int_convert_str(hdr_gamut_data.coef[i],
			i, stemp, 4, 16);
	stemp[36] = '\0';
	sprintf(buf, "hdr_gamut coef_str: %s\n", stemp);

	if (chip_type_id == chip_t6x) {
		memcpy(&hdr_gamut_data, &force_gamut_mtx,
			sizeof(struct hdr_gamut_data_s));
		memset(stemp, 0, sizeof(stemp));
		for (i = 0; i < 9; i++)
			int_convert_str(hdr_gamut_data.coef[i],
				i, stemp, 4, 16);
		stemp[36] = '\0';
		sprintf(buf + strlen(buf), "gamut_coef_str: %s\n", stemp);

		memcpy(&hdr_gamut_data, &force_gamut0_mtx,
			sizeof(struct hdr_gamut_data_s));
		memset(stemp, 0, sizeof(stemp));
		for (i = 0; i < 9; i++)
			int_convert_str(hdr_gamut_data.coef[i],
				i, stemp, 4, 16);
		stemp[36] = '\0';
		sprintf(buf + strlen(buf), "gamut_coef0_str: %s\n", stemp);
	}

	return strlen(buf);
}

static ssize_t amvecm_gamut_mtrx_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	return 0;
}

static ssize_t amvecm_lc_show(const struct class *cla,
			      const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	char *temp_cur;

	if (lc_dbg_flag & LC_CUR_RD_UPDATE) {
		temp_cur = kmalloc(300, GFP_KERNEL);
		if (!temp_cur)
			return len;
		memset(temp_cur, 0, 300);

		switch (reg_sel) {
		case SATUR_LUT:
			lc_rd_reg(SATUR_LUT, 1, temp_cur);
			break;
		case YMINVAL_LMT:
			lc_rd_reg(YMINVAL_LMT, 1, temp_cur);
			break;
		case YPKBV_YMAXVAL_LMT:
			lc_rd_reg(YPKBV_YMAXVAL_LMT, 1, temp_cur);
			break;
		case YMAXVAL_LMT:
			lc_rd_reg(YMAXVAL_LMT, 1, temp_cur);
			break;
		case YPKBV_LMT:
			lc_rd_reg(YPKBV_LMT, 1, temp_cur);
			break;
		case YPKBV_RAT:
			lc_rd_reg(YPKBV_RAT, 1, temp_cur);
			break;
		default:
			pr_info("unsupport cmd!\n");
			break;
		}
		lc_dbg_flag &= ~LC_CUR_RD_UPDATE;
		reg_sel = MAX_REG_LUT;
		len = sprintf(buf, "%s\n", temp_cur);
		kfree(temp_cur);
		return len;
	}

	if (lc_dbg_flag & LC_PARAM_RD_UPDATE) {
		lc_dbg_flag &= ~LC_PARAM_RD_UPDATE;
		return sprintf(buf, "%d\n", lc_temp);
	}

	if (lc_dbg_flag & LC_CUR2_RD_UPDATE) {
		lc_dbg_flag &= ~LC_CUR2_RD_UPDATE;
		return sprintf(buf, "%s\n", lc_dbg_curve);
	}

	len += sprintf(buf + len,
		"echo lc enable > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc disable > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_dbg value > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_demo_mode enable/disable > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_dump_reg parm1 > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_rd_reg parm1 parm2 > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"parm1: 0x1,0x2,0x4,0x8,0x10,0x20...\n");
	len += sprintf(buf + len,
		"parm2: decimal strings, each data width is 4.\n");
	len += sprintf(buf + len,
		"echo dump_hist all > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo dump_hist chosen hs he vs ve cnt > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo dump_curve cnt > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_osd_setting show > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo lc_osd_setting set xxx ... xxx > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo osd_iir_en val > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo iir_refresh show > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo iir_refresh set xxx ... xxx > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo scene_change_th val > /sys/class/amvecm/lc\n");
	len += sprintf(buf + len,
		"echo irr_dbg_en val > /sys/class/amvecm/lc\n");
	return len;
}

static ssize_t amvecm_lc_store(const struct class *cls,
			       const struct class_attribute *attr,
		 const char *buf, size_t count)
{
	char *buf_orig, *parm[8] = {NULL};
	int ret;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		return -ENOMEM;
	}
	parse_param_amvecm(buf_orig, (char **)&parm);
	ret = lc_debug_store(parm);
	if (ret)
		pr_info("set parameters failed %d\n", ret);
	kfree(buf_orig);
	return count;
}

static void def_hdr_sdr_mode(void)
{
	sdr_mode = 0;
}

void hdr_hist_config_int(void)
{
	unsigned int addr_offset_vd1 = 0;
	unsigned int addr_offset_vd2 = 0;
	unsigned int default_val = 0x5510;

	if (chip_type_id == chip_t6d ||
		chip_type_id == chip_a9) {
		addr_offset_vd1 = 0x2a00;
		addr_offset_vd2 = 0x2a30;
	} else if (chip_type_id == chip_t6w) {
		addr_offset_vd1 = 0x1400;
		addr_offset_vd2 = 0x2a30;
		default_val = 0x10010;
	} else if (chip_type_id == chip_t6x) {
		addr_offset_vd1 = 0x1400;
		addr_offset_vd2 = 0x2ab0;
		default_val = 0x10010;
	}

	if (chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		WRITE_VPP_REG(DOLBY_HDR2_HIST_SLC_X_ST_ED_0, 0x20000eff);

	if (chip_type_id == chip_a9)
		WRITE_VPP_REG(VD1_HDR2_HIST_SLC_X_ST_ED_0 + addr_offset_vd1,
			0x20000eff);

	if (chip_type_id != chip_t3x) {
		WRITE_VPP_REG(VD1_HDR2_HIST_CTRL + addr_offset_vd1, default_val);
		WRITE_VPP_REG(VD1_HDR2_HIST_H_START_END + addr_offset_vd1, 0x10000);
		WRITE_VPP_REG(VD1_HDR2_HIST_V_START_END + addr_offset_vd1, 0x0);

		if (get_cpu_type() != MESON_CPU_MAJOR_ID_T5 &&
			get_cpu_type() != MESON_CPU_MAJOR_ID_T5D &&
			chip_type_id != chip_txhd2) {
			WRITE_VPP_REG(VD2_HDR2_HIST_CTRL + addr_offset_vd2, 0x5510);
			WRITE_VPP_REG(VD2_HDR2_HIST_H_START_END + addr_offset_vd2, 0x10000);
			WRITE_VPP_REG(VD2_HDR2_HIST_V_START_END + addr_offset_vd2, 0x0);

			WRITE_VPP_REG(OSD1_HDR2_HIST_CTRL, 0x5510);
			WRITE_VPP_REG(OSD1_HDR2_HIST_H_START_END, 0x10000);
			WRITE_VPP_REG(OSD1_HDR2_HIST_V_START_END, 0x0);
		}
	} else {
		s5_hdr_hist_config_int();
	}
}
#endif

struct param_parse_cmd_s {
	char *parse_string;
	int *value;
};

struct param_parse_cmd_s param_cmd[] = {
	{"probe_ok", &probe_ok},
	{"pq_load_en", &pq_load_en},
	{"debug_amvecm", &debug_amvecm},
	{"debug_amve", &amve_debug},
	{"debug_amcm", &debug_amcm},
	{"debug_regload", &debug_regload},
	{"debug_game_mode_1", &debug_game_mode_1},
	{"wb_en", &wb_en},
	{"cm_en", &cm_en},
	{"pq_reg_wr_rdma", &pq_reg_wr_rdma},
	{"video_rgb_ogo_mode_sw", &video_rgb_ogo_mode_sw},
	{"video_rgb_ogo_xvy_mtx", &video_rgb_ogo_xvy_mtx},
	{"overscan_timing", &overscan_timing},
	{"overscan_screen_mode", &overscan_screen_mode},
	{"overscan_disable", &overscan_disable},
	{"dv_pq_bypass", &dv_pq_bypass},
	{"contrast_adj_sel", &contrast_adj_sel},
	{"debug_csc", &debug_csc},
	{"hdr_policy", &hdr_policy},
	{"primary_policy", &primary_policy},
	{"force_output", &force_output},
	{"prime_mode", &prime_mode},
	{"end", NULL},
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct param_parse_cmd_s param_cut_cmd[] = {
	{"debug_amvecm_bringup", &debug_amvecm_bringup},
	{"gamma_en", &gamma_en},
	{"fmeter_debug", &fmeter_debug},
	{"fmeter_count", &fmeter_count},
	{"bs_3dlut_en", &bs_3dlut_en},
	{"ct_en", &ct_en},
	{"ai_color_enable", &ai_color_enable},
	{"osd_pic_en", &osd_pic_en},
	{"dnlp_hist_sel", &hist_sel},
	{"dnlp_dbg_print", &dnlp_dbg_print},
	{"dnlp_en_dsw", &dnlp_en_dsw},
	{"gamma_loadprotect_en", &gamma_loadprotect_en},
	{"lc_debug", &amlc_debug},
	{"lc_skip_iir", &lc_skip_iir},
	{"lc_demo_mode", &lc_demo_mode},
	{"lc_force_ctrl", &lc_force_ctrl},
	{"lc_alpha1", &alpha1},
	{"vev2_dbg", &vev2_dbg},
	{"hdr_disable_flush_flag", &disable_flush_flag},
	{"hdr_adp_scal_x_shift", &adp_scal_x_shift},
	{"hdr_adp_scal_y_shift", &adp_scal_y_shift},
	{"hdr_clip_func", &clip_func},
	{"fw_alg_ctl_en", &fw_alg_ctl_en},
	{"hdr_vd1_bp_force", &vd1_bp_force},
	{"hdr_mode", &hdr_mode},
	{"sdr_mode", &sdr_mode},
	{"cuva_static_hlg_en", &cuva_static_hlg_en},
	{"gamut_conv_enable", &gamut_conv_enable},
	{"print_lut_mtx", &print_lut_mtx},
	{"csc_en", &csc_en},
	{"force_csc_type", &force_csc_type},
	{"range_control", &range_control},
	{"rdma_flag", &rdma_flag},
	{"lut_289_en", &lut_289_en},
	{"knee_factor", &knee_factor},
	{"knee_interpolation_mode", &knee_interpolation_mode},
	{"force_customer_panel_lumin", &force_customer_panel_lumin},
	{"customer_panel_lumin", &customer_panel_lumin},
	{"customer_hdr_clipping", &customer_hdr_clipping},
	{"reload_lut", &reload_lut},
	{"video_lut_switch", &video_lut_switch},
	{"hdr10_tm_dbg", &hdr10_tm_dbg},
	{"hdr10_tm_sel", &hdr10_tm_sel},
	{"hdr10_tm_panell", &panell},
	{"hdr10p_printk", &hdr10_plus_printk},
	{"hdr10p_force_ref_peak", &force_ref_peak},
	{"pr_tmo_en", &pr_tmo_en},
	{"gmt_print", &gmt_print},
	{"cuva_sw_dbg", &cuva_sw_dbg},
	{"aipq_debug", &aipq_debug},
	{"aipq_smooth_dbg", &aipq_smooth_dbg},
	{"aipq_en", &aipq_en},
	{"aipq_bld_rs", &aipq_bld_rs},
	{"aipq_slower_coef", &slower_coef},
	{"ai_clr_dbg", &ai_clr_dbg},
	{"am_dma_ctrl_dbg", &am_dma_ctrl_dbg},
	{"pr_cabc_aad", &pr_cabc_aad},
	{"vks_theta_angle", &vks_theta_angle},
	{"vks_alph0_angle", &vks_alph0_angle},
	{"vks_alph1_angle", &vks_alph1_angle},
	{"lc_pattern_detect_log1", &lc_pattern_detect_log1},
	{"lc_read_curve_nodes_changed_en", &lc_read_curve_nodes_changed_en},
	{"lc_change_curve_nodes_en", &lc_change_curve_nodes_en},
	{"cuva_mode", &cuva_mode},
	{"hdr_hist_select", &hdr_hist_select},
	{"end", NULL},
};
#endif

static ssize_t amvecm_param_ctrl_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	int i = 0;

	pr_info("************param_ctrl command list************\n");

	while (strcmp(param_cmd[i].parse_string, "end")) {
		pr_info("echo w %s 0x%x > /sys/class/amvecm/param_ctrl\n",
			param_cmd[i].parse_string, *param_cmd[i].value);
		i++;
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	i = 0;

	while (strcmp(param_cut_cmd[i].parse_string, "end")) {
		pr_info("echo w %s 0x%x > /sys/class/amvecm/param_ctrl\n",
			param_cut_cmd[i].parse_string, *param_cut_cmd[i].value);
		i++;
	}
#endif

	pr_info("************param_ctrl command list end************\n");

	return 0;
}

static ssize_t amvecm_param_ctrl_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	int i;
	long val = 0;
	char *buf_orig, *parm[8] = {NULL};

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return -ENOMEM;

	parse_param_amvecm(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "w")) {
		for (i = 0; param_cmd[i].value; i++) {
			if (!strcmp(parm[1],
				param_cmd[i].parse_string)) {
				if (kstrtoul(parm[2], 16, &val) < 0)
					goto free_buf;

				*param_cmd[i].value = val;
				pr_amvecm_dbg("set %s: %d\n",
					param_cmd[i].parse_string,
					*param_cmd[i].value);
				break;
			}
		}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		for (i = 0; param_cut_cmd[i].value; i++) {
			if (!strcmp(parm[1],
				param_cut_cmd[i].parse_string)) {
				if (kstrtoul(parm[2], 16, &val) < 0)
					goto free_buf;

				*param_cut_cmd[i].value = val;
				pr_amvecm_dbg("set %s: %d\n",
					param_cut_cmd[i].parse_string,
					*param_cut_cmd[i].value);
				break;
			}
		}
#endif
	}

	kfree(buf_orig);
	return count;

free_buf:
	kfree(buf_orig);
	return -EINVAL;
}

#define PQ_TV 1
#define PQ_BOX 0
void init_pq_control(unsigned int enable)
{
	if (enable) {
		memcpy(&pq_cfg, &pq_cfg_init[TV_CFG_DEF],
			sizeof(struct pq_ctrl_s));
		memcpy(&dv_cfg_bypass, &pq_cfg_init[TV_DV_BYPASS],
			sizeof(struct pq_ctrl_s));
	} else {
		memcpy(&pq_cfg, &pq_cfg_init[OTT_CFG_DEF],
			sizeof(struct pq_ctrl_s));
		memcpy(&dv_cfg_bypass, &pq_cfg_init[OTT_DV_BYPASS],
			sizeof(struct pq_ctrl_s));
	}

	memcpy(&pq_cfg_cur, &pq_cfg_init[INIT_CUR_CFG],
		sizeof(struct pq_ctrl_s));
}

int vinfo_lcd_support(void)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	if (vinfo->mode == VMODE_LCD || vinfo->mode == VMODE_eDP ||
		vinfo->mode == VMODE_DUMMY_ENCP)
		return 1;
	else
		return 0;
}

/*for s5 dsc enable, vpp convert yuv2rgb when hdmi rgb out. ret: 0 yuv, 1 rgb*/

int vinfo_hdmi_out_fmt(void)
{
	struct vinfo_s *vinfo = get_current_vinfo();

	if (chip_type_id != chip_s5)
		return 0;

	if ((vinfo->mode & VMODE_MODE_BIT_MASK) == VMODE_HDMI &&
		vinfo->vpp_post_out_color_fmt)
		return 1;
	else
		return 0;
}

/* #if (MESON_CPU_TYPE == MESON_CPU_TYPE_MESONG9TV) */
void init_pq_setting(void)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct vinfo_s *vinfo = get_current_vinfo();
#endif

	int bitdepth;
	int lcd_support = vinfo_lcd_support();

//	pr_info("[debug] pq_setting_init start, lcd_support %d\n", lcd_support);

	if (lcd_support)
		init_pq_control(PQ_TV);
	else
		init_pq_control(PQ_BOX);

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_SC2)
		init_pq_control(PQ_BOX);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		c5_vadj_gamma_init();
		vpp_pq_ctrl_config(pq_cfg, WR_VCB, 0);
		pq_reg_wr_rdma = 1;
		return;
	}

	/*ai pq interface*/
	ai_detect_scene_init();
	adaptive_param_init();

	if (chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		set_hdr_tmo_reg();

	if (chip_cls_id == STB_CHIP)
		set_dummy_flag = 1;

#endif

	if (is_meson_gxtvbb_cpu() || is_meson_txl_cpu() ||
		is_meson_txlx_cpu() || is_meson_txhd_cpu() ||
		is_meson_tl1_cpu() || is_meson_tm2_cpu() ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T7 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_t3x ||
		chip_type_id == chip_txhd2 ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		(chip_type_id == chip_a9 && lcd_support))
		goto tvchip_pq_setting;
	else if (is_meson_g12a_cpu() || is_meson_g12b_cpu() ||
		is_meson_sm1_cpu() ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_SC2 ||
		is_meson_s4_cpu() ||
		is_meson_s4d_cpu() ||
		is_meson_s7_cpu() ||
		chip_type_id == chip_s7d ||
		is_meson_s6_cpu() ||
		(chip_type_id == chip_a9 && !lcd_support)) {
		if (is_meson_s4_cpu())
			bitdepth = 10;
		else
			bitdepth = 12;
		/*confirm with vlsi-Lunhai.Chen, for G12A/G12B,
		 *VPP_GCLK_CTRL1 must enable
		 */
		WRITE_VPP_REG_BITS(VPP_GCLK_CTRL1, 0xf, 0, 4);

		if (chip_type_id == chip_s6 ||
//			chip_type_id == chip_t6w ||
//			chip_type_id == chip_t6x ||
			chip_type_id == chip_a9)
			osd_sharpness_init();

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		sr_init_config();
		dnlp_init_config();
		cm_init_config(bitdepth);
//		pr_info("[debug] cm_init_config\n");

		if (chip_type_id < chip_s7d) {
			/*dnlp off*/
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0,
				DNLP_EN_BIT, DNLP_EN_WID);
			/*sr0 chroma filter bypass*/
			WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0,
				0x4000);
			WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0,
				0x4000);
		}

		/*kernel sdr2hdr match uboot setting*/
		if (chip_type_id != chip_a9)
			def_hdr_sdr_mode();
#endif
		vpp_pq_ctrl_config(pq_cfg, WR_VCB, 0);
//		pr_info("[debug] vpp_pq_ctrl_config\n");

		pq_reg_wr_rdma = 1;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else if (chip_type_id == chip_s5) {
		bitdepth = 12;
		cm_top_ctl(WR_VCB, 1, 0);
		cm_init_config(bitdepth);
		vpp_pq_ctrl_config(pq_cfg, WR_VCB, 0);

		pq_reg_wr_rdma = 1;
#endif
	}
	return;

tvchip_pq_setting:
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		if (is_meson_tl1_cpu())
			bitdepth = 10;
		else if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
			chip_type_id == chip_t5m ||
			chip_type_id == chip_t3x ||
			chip_type_id == chip_txhd2 ||
			chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x)
			bitdepth = 10;
		else if (is_meson_tm2_cpu())
			bitdepth = 12;
		else
			bitdepth = 12;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		sr_init_config();
		dnlp_init_config();

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_t3x)
			cm_top_ctl(WR_VCB, 1, 0);
#endif
		cm_init_config(bitdepth);
		/*lc init*/
		lc_init(bitdepth);

		/*frequence meter init*/
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
			get_cpu_type() == MESON_CPU_MAJOR_ID_T5W ||
			chip_type_id == chip_t5m)
			amve_fmeter_init(fmeter_en);
#endif

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2))
			hdr_hist_config_int();

		if (chip_type_id == chip_t6w || chip_type_id == chip_t6x)
			set_muxio_link_mode(0, NULL, VPP_VCBUS);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7) &&
			chip_type_id != chip_txhd2)
			vpp_pst_hist_sta_config(1, HIST_MAXRGB,
				AFTER_POST2_MTX, vinfo);
#endif
	}

	/* enable vadj1 by default */
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		if (chip_type_id != chip_t3x)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 0, 1);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		else
			ve_vadj_ctl(WR_VCB, VE_VADJ1, 1, 0);
#endif
	} else {
		WRITE_VPP_REG(VPP_VADJ_CTRL, 0xd);
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/*probe close sr0 peaking for switch on video*/
	WRITE_VPP_REG_BITS(VPP_SRSHARP0_CTRL, 1, 0, 1);
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
		WRITE_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 0, 0, 1);
		/*VPP_VADJ1_MISC bit1: minus black level enable for vadj1*/
		if (chip_type_id != chip_t3x)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 1, 1);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		else
			ve_vadj_misc_set(0, WR_VCB, VE_VADJ1, SLICE0, 1, 1, 0);
#endif
	} else {
		WRITE_VPP_REG_BITS(VPP_SRSHARP1_CTRL, 1, 0, 1);
	}
#else
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
		WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, 1, 1, 1);
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/*default dnlp off*/
	WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
		0, 1, 1);
	WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
		0, 1, 1);

	if (chip_type_id != chip_t3x)
		WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL, 0, DNLP_EN_BIT, DNLP_EN_WID);
	/*end*/

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		WRITE_VPP_REG_BITS(SRSHARP1_PK_FINALGAIN_HP_BP,
			2, 16, 2);
		/*sr0 sr1 chroma filter bypass*/
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0,
			0x4000);
		WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0,
			0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_HCOEF0,
			0x4000);
		WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_VCOEF0,
			0x4000);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_t3x) {
			WRITE_VPP_REG_BITS(SRSHARP1_PK_FINALGAIN_HP_BP + SLICE_OFFSET_1,
				2, 16, 2);
			WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_HCOEF0 + SLICE_OFFSET,
				0x4000);
			WRITE_VPP_REG(SRSHARP0_SHARP_SR2_CBIC_VCOEF0 + SLICE_OFFSET,
				0x4000);
			WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_HCOEF0 + SLICE_OFFSET_1,
				0x4000);
			WRITE_VPP_REG(SRSHARP1_SHARP_SR2_CBIC_VCOEF0 + SLICE_OFFSET_1,
				0x4000);
		}
#endif
	}
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_gxlx_cpu())
		amve_sharpness_init(0);
#endif

	/*dnlp alg parameters init*/
	dnlp_alg_param_init();
#endif

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5D)
		pq_cfg.gamma_en = 0;

	vpp_pq_ctrl_config(pq_cfg, WR_VCB, 0);

	/*am_dma_ctrl init*/
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		am_dma_set_mif_wr_status(1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VI_HIST_SPL_0, 1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VI_HIST_SPL_1, 1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VD1_HDR_0, 1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VD1_HDR_1, 1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_LC_STTS_0, 0);
		/*am_dma_set_mif_wr(EN_DMA_WR_ID_LC_STTS_1, 1);*/
		/*am_dma_set_mif_wr(EN_DMA_WR_ID_CM2_HIST_0, 1);*/
		/*am_dma_set_mif_wr(EN_DMA_WR_ID_CM2_HIST_1, 0);*/
		/*am_dma_set_mif_wr(EN_DMA_WR_ID_VD2_HDR, 1);*/
	}

	gamut_mapping_wrapper_init();
#endif

	pq_reg_wr_rdma = 1;
}

/* #endif*/

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct gamma_data_s *get_gm_data(void)
{
	return &amvecm_dev.gm_data;
}

void amvecm_gamma_init(bool en)
{
	unsigned int i, j, k;
	unsigned short data[257];
	unsigned short temp;
	struct gamma_data_s *p_gm;

	if (chip_cls_id == STB_CHIP)
		return;

	for (i = 0; i < 257; i++) {
		data[i] = i << 2;
		if (data[i] >= (1 << 10))
			data[i] = (1 << 10) - 1;
		video_gamma_table_r.data[i] = data[i];
		video_gamma_table_g.data[i] = data[i];
		video_gamma_table_b.data[i] = data[i];

		for (k = 0; k < FREESYNC_DYNAMIC_GAMMA_NUM; k++)
			for (j = 0; j < FREESYNC_DYNAMIC_GAMMA_CHANNEL; j++)
				gt.gm_tb[k][j].data[i] = data[i];
	}

	if (cpu_after_eq_t7()) {
		if (is_meson_t7_cpu() ||
			chip_type_id == chip_t3x ||
			chip_type_id == chip_a9) {
			if (chip_type_id == chip_t3x ||
				chip_type_id == chip_a9) {
				p_gm = get_gm_data();
				p_gm->max_idx = 257;
				p_gm->auto_inc = 1 << L_H_AUTO_INC_2;
				p_gm->addr_port = LCD_GAMMA_ADDR_PORT0;
				p_gm->data_port = LCD_GAMMA_DATA_PORT0;
			}

			vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
			vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
		} else if (chip_type_id == chip_t5m ||
			chip_type_id == chip_txhd2 ||
			chip_type_id == chip_a4 ||
			chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x ||
			chip_type_id == chip_a9 ||
			chip_type_id == chip_c5) {
			p_gm = get_gm_data();
			p_gm->max_idx = 257;
			p_gm->auto_inc = 1 << L_H_AUTO_INC_2;
			if (chip_type_id == chip_a4 ||
				chip_type_id == chip_c5) {
				p_gm->addr_port = LCD_GAMMA_ADDR_PORT0_A4;
				p_gm->data_port = LCD_GAMMA_DATA_PORT0_A4;
			} else {
				p_gm->addr_port = LCD_GAMMA_ADDR_PORT0;
				p_gm->data_port = LCD_GAMMA_DATA_PORT0;
			}
			for (i = 0; i < p_gm->max_idx; i++) {
				temp = i << 2;
				if (temp >= (1 << 10))
					temp = (1 << 10) - 1;
				p_gm->gm_tbl.gamma_r[i] = temp;
				p_gm->gm_tbl.gamma_g[i] = temp;
				p_gm->gm_tbl.gamma_b[i] = temp;
			}
			lcd_gamma_api(0, p_gm->gm_tbl.gamma_r,
					p_gm->gm_tbl.gamma_g,
					p_gm->gm_tbl.gamma_b,
					WR_VCB, WR_MOD, 0);
		} else {
			lcd_gamma_api(0, video_gamma_table_r.data,
				video_gamma_table_g.data,
				video_gamma_table_b.data,
				WR_VCB, WR_MOD, 0);
		}
	} else {
		vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
	}

	if (chip_type_id != chip_t3x) {
		if (en)
			vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
		else
			vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
	}
}
#endif

static void amvecm_wb_init(bool en)
{
	int *initcoef;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct tcon_rgb_ogo_s wb_data;
#endif

	initcoef = wb_init_bypass_coef;

	if (chip_type_id == chip_s5)
		return;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12B)
		return;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		WRITE_VPP_REG(VOUT_GAINOFF_CTRL0,
					(en << 31) | (1024 << 16) | 1024);
		WRITE_VPP_REG(VOUT_GAINOFF_CTRL1,
					(1 << 27) | (1024 << 16));
		return;
	}
#endif

	if (video_rgb_ogo_xvy_mtx) {
		if (chip_type_id != chip_t3x) {
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 3);

			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
				((initcoef[0] & 0xfff) << 16) |
				(initcoef[1] & 0xfff));
			WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
				initcoef[2] & 0xfff);
			WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
				((initcoef[3] & 0x1fff) << 16) |
				(initcoef[4] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
				((initcoef[5]  & 0x1fff) << 16) |
				(initcoef[6] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
				((initcoef[7] & 0x1fff) << 16) |
				(initcoef[8] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
				((initcoef[9] & 0x1fff) << 16) |
				(initcoef[10] & 0x1fff));
			WRITE_VPP_REG(VPP_MATRIX_COEF22,
				initcoef[11] & 0x1fff);
			if (initcoef[21]) {
				WRITE_VPP_REG(VPP_MATRIX_COEF13_14,
					((initcoef[12] & 0x1fff) << 16) |
					(initcoef[13] & 0x1fff));
				WRITE_VPP_REG(VPP_MATRIX_COEF15_25,
					((initcoef[14] & 0x1fff) << 16) |
					(initcoef[17] & 0x1fff));
				WRITE_VPP_REG(VPP_MATRIX_COEF23_24,
					((initcoef[15] & 0x1fff) << 16) |
					(initcoef[16] & 0x1fff));
			}
			WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
				((initcoef[18] & 0xfff) << 16) |
				(initcoef[19] & 0xfff));
			WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
				initcoef[20] & 0xfff);
			WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
				initcoef[21], 3, 2);
			WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
				initcoef[22], 5, 3);

			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, en, 6, 1);
		}
	} else {
		if (chip_type_id != chip_t3x) {
			WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
				(en << 31) | (1024 << 16) | 1024);
			WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
				(1024 << 16));
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		} else {
			memset(&wb_data, 0,
				sizeof(struct tcon_rgb_ogo_s));
			wb_data.en = en;
			wb_data.r_gain = 1024;
			wb_data.g_gain = 1024;
			wb_data.b_gain = 1024;
			post_gainoff_set(&wb_data, WR_VCB, 0);
#endif
		}
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void amvecm_3dlut_init(bool en)
{
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_t3x ||
		chip_type_id == chip_a4 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_c5)
		return;

	if (ct_en) {
		lut3d_en = 1;
		vpp_lut3d_table_init(-1, -1, -1);
		vpp_set_lut3d(0, 0, 0, 0, 0);
		color_lut_init(ct_en);
		vpp_enable_lut3d(ct_en);
		vpp_lut3d_base_table_init();
	} else {
		vpp_lut3d_table_init(-1, -1, -1);
		vpp_set_lut3d(0, 0, 0, 0, 0);
		/*vpp_lut3d_table_release();*/
		vpp_enable_lut3d(en);
	}
}
#endif

static struct class_attribute amvecm_class_attrs[] = {
	__ATTR(debug, 0644,
		amvecm_debug_show,
		amvecm_debug_store),
	__ATTR(param_ctrl, 0644,
		amvecm_param_ctrl_show,
		amvecm_param_ctrl_store),
	__ATTR(brightness, 0644,
		amvecm_brightness_show,
		amvecm_brightness_store),
	__ATTR(contrast, 0644,
		amvecm_contrast_show,
		amvecm_contrast_store),
	__ATTR(saturation_hue, 0644,
		amvecm_saturation_hue_show,
		amvecm_saturation_hue_store),
	__ATTR(saturation_hue_pre, 0644,
		amvecm_saturation_hue_pre_show,
		amvecm_saturation_hue_pre_store),
	__ATTR(saturation_hue_post, 0644,
		amvecm_saturation_hue_post_show,
		amvecm_saturation_hue_post_store),
	__ATTR(cm_reg, 0644,
		amvecm_cm_reg_show,
		amvecm_cm_reg_store),
	__ATTR(wb, 0644,
		amvecm_wb_show,
		amvecm_wb_store),
	__ATTR(brightness1, 0644,
		video_adj1_brightness_show,
		video_adj1_brightness_store),
	__ATTR(contrast1, 0644,
		video_adj1_contrast_show,
		video_adj1_contrast_store),
	__ATTR(brightness2, 0644,
		video_adj2_brightness_show,
		video_adj2_brightness_store),
	__ATTR(contrast2, 0644,
		video_adj2_contrast_show,
		video_adj2_contrast_store),
	__ATTR(dump_reg, 0644,
		amvecm_dump_reg_show,
		amvecm_dump_reg_store),
	__ATTR(reg, 0644,
		amvecm_reg_show,
		amvecm_reg_store),
	__ATTR(pq_reg_rw, 0644,
		amvecm_write_reg_show,
		amvecm_write_reg_store),
	__ATTR(cpu_ver, 0644,
		amvecm_cpu_ver_show,
		amvecm_cpu_ver_store),
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	__ATTR(dnlp_debug, 0644,
		amvecm_dnlp_debug_show,
		amvecm_dnlp_debug_store),
	__ATTR(cm2_hue, 0644,
		amvecm_cm2_hue_show,
		amvecm_cm2_hue_store),
	__ATTR(cm2_luma, 0644,
		amvecm_cm2_luma_show,
		amvecm_cm2_luma_store),
	__ATTR(cm2_sat, 0644,
		amvecm_cm2_sat_show,
		amvecm_cm2_sat_store),
	__ATTR(cm2_hue_by_hs, 0644,
		amvecm_cm2_hue_by_hs_show,
		amvecm_cm2_hue_by_hs_store),
	__ATTR(cm2, 0644,
		amvecm_cm2_show,
		amvecm_cm2_store),
	__ATTR(cm2_idx, 0644,
		amvecm_cm2_idx_show,
		amvecm_cm2_idx_store),
	__ATTR(gamma, 0644,
		amvecm_gamma_show,
		amvecm_gamma_store),
	__ATTR(gamma_v2, 0644,
		amvecm_gamma_v2_show,
		amvecm_gamma_v2_store),
	__ATTR(brightness_osd, 0644,
		osd_brightness_show,
		osd_brightness_store),
	__ATTR(contrast_osd, 0644,
		osd_contrast_show,
		osd_contrast_store),
	__ATTR(help, 0644,
		amvecm_usage_show, NULL),
	__ATTR(sync_3d, 0644,
		amvecm_3d_sync_show,
		amvecm_3d_sync_store),
	__ATTR(vlock, 0644,
		amvecm_vlock_show,
		amvecm_vlock_store),
	__ATTR(matrix_set, 0644,
		amvecm_set_post_matrix_show,
		amvecm_set_post_matrix_store),
	__ATTR(matrix_pos, 0644,
		amvecm_post_matrix_pos_show,
		amvecm_post_matrix_pos_store),
	__ATTR(matrix_data, 0644,
		amvecm_post_matrix_data_show,
		amvecm_post_matrix_data_store),
	__ATTR(sr1_reg, 0644,
		amvecm_sr1_reg_show,
		amvecm_sr1_reg_store),
	__ATTR(write_sr1_reg_val, 0644,
		amvecm_write_sr1_reg_val_show,
		amvecm_write_sr1_reg_val_store),
	__ATTR(dump_vpp_hist, 0644,
		amvecm_dump_vpp_hist_show,
		amvecm_dump_vpp_hist_store),
	__ATTR(hdr_dbg, 0644,
		amvecm_hdr_dbg_show,
		amvecm_hdr_dbg_store),
	__ATTR(ai_color, 0644,
		amvecm_ai_color_show,
		amvecm_ai_color_store),
	__ATTR(hdr_reg, 0644,
		amvecm_hdr_reg_show,
		amvecm_hdr_reg_store),
	__ATTR(hdr_tmo, 0644,
		amvecm_hdr_tmo_show,
		amvecm_hdr_tmo_store),
	__ATTR(hdr_fw_dbg, 0644,
		amvecm_hdr_fw_dbg_show,
		amvecm_hdr_fw_dbg_store),
	__ATTR(gamma_pattern, 0644,
		set_gamma_pattern_show,
		set_gamma_pattern_store),
	__ATTR(pc_mode, 0644,
		amvecm_pc_mode_show,
		amvecm_pc_mode_store),
	__ATTR(set_hdr_289lut, 0644,
		set_hdr_289lut_show,
		set_hdr_289lut_store),
	__ATTR(vpp_demo, 0644,
		amvecm_vpp_demo_show,
		amvecm_vpp_demo_store),
	__ATTR(pq_user_set, 0644,
		amvecm_pq_user_show,
		amvecm_pq_user_store),
	__ATTR(get_hdr_type, 0644,
		amvecm_get_hdr_type_show,
		amvecm_get_hdr_type_store),
	__ATTR(dnlp_insmod, 0644,
		amvecm_dnlp_insmod_show,
		amvecm_dnlp_insmod_store),
	__ATTR(lc, 0644,
		amvecm_lc_show,
		amvecm_lc_store),
	__ATTR(color_top, 0644,
		amvecm_clamp_color_top_show,
		amvecm_clamp_color_top_store),
	__ATTR(color_bottom, 0644,
		amvecm_clamp_color_bottom_show,
		amvecm_clamp_color_bottom_store),
	__ATTR(cabc_aad, 0644,
		amvecm_cabc_aad_show,
		amvecm_cabc_aad_store),
	__ATTR(frame_lock, 0644,
		amvecm_frame_lock_show,
		amvecm_frame_lock_store),
	__ATTR(color_tune, 0664,
		amvecm_color_tune_show,
		amvecm_color_tune_store),
	__ATTR(dma_buf, 0664,
		amvecm_dma_buf_show,
		amvecm_dma_buf_store),
	__ATTR(ble_whe_dbg, 0644,
		amvecm_ble_whe_dbg_show,
		amvecm_ble_whe_dbg_store),
	__ATTR(slt_dbg, 0644,
		amvecm_slt_debug_show,
		amvecm_slt_debug_store),
	__ATTR(vl_lock_st, 0644,
		amvecm_slt_vl_lock_st_show,
		amvecm_slt_vl_lock_st_store),
	__ATTR(enable_hdr10plus, 0644,
		amvecm_enable_hdr10plus_show, NULL),
	__ATTR(hdr_cap_dbg, 0644,
		amvecm_hdr_cap_dbg_show, NULL),
	__ATTR(hdr_param_dbg, 0644,
		amvecm_hdr_param_show,
		amvecm_hdr_param_store),
	__ATTR(hue_sat_osd, 0644,
	    osd_hue_sat_show, osd_hue_sat_store),
	__ATTR(hdr_test_case, 0644,
		amvecm_hdr_test_case_show,
		amvecm_hdr_test_case_store),
	__ATTR(dpss_hdr_test, 0644,
		amvecm_dpss_hdr_test_show,
		amvecm_dpss_hdr_test_store),
	__ATTR(gamut_mapping, 0644,
		amvecm_gamut_mapping_show,
		amvecm_gamut_mapping_store),
	__ATTR(amvecm_dump_output, 0644,
		amvecm_dump_output_show,
		amvecm_dump_output_store),
	__ATTR(gamut_mtrx, 0644,
		amvecm_gamut_mtrx_show,
		amvecm_gamut_mtrx_store),
#endif
	__ATTR_NULL
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void amvecm_wakeup_queue(void)
{
	/*struct amvecm_dev_s *devp = &amvecm_dev;*/

	/*wake_up(&devp->hdr_queue);*/
}
#endif

static unsigned int amvecm_poll(struct file *file, poll_table *wait)
{
	/*struct amvecm_dev_s *devp = file->private_data;*/
	unsigned int mask = 0;

	/*poll_wait(file, &devp->hdr_queue, wait);*/
	/*mask = (POLLIN | POLLRDNORM);*/

	return mask;
}

static const struct file_operations amvecm_fops = {
	.owner   = THIS_MODULE,
	.open    = amvecm_open,
	.release = amvecm_release,
	.unlocked_ioctl   = amvecm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = amvecm_compat_ioctl,
#endif
	.poll = amvecm_poll,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
static const struct vecm_match_data_s vecm_dt_xxx = {
	.chip_id = chip_other,
	.chip_cls = OTHER_CLS,
	.vlk_chip = vlock_chip_txlx,
	.vlk_support = true,
	.vlk_new_fsm = 0,
	.vlk_hwver = vlock_hw_org,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};
#endif

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static const struct vecm_match_data_s vecm_dt_tl1 = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_tl1,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static const struct vecm_match_data_s vecm_dt_sm1 = {
	.chip_id = chip_other,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_sm1,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_tm2 = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_tm2,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_ver2,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_tm2_verb = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_tm2,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t5 = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t5d = {
	.chip_id = chip_other,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,/*same as t5d*/
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t7 = {
	.chip_id = chip_t7,
	.chip_cls = SMT_CHIP,
	.vlk_chip = vlock_chip_t7,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_t3 = {
	.chip_id = chip_t3,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t3,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vlk_ctl_for_frc = 1,
	.vrr_support_flag = 1,
};

/*t5w vlock follow t5 */
static const struct vecm_match_data_s vecm_dt_t5w = {
	.chip_id = chip_t5w,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_t5m = {
	.chip_id = chip_t5m,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5m,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vlk_ctl_for_frc = 0,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_s5 = {
	.chip_id = chip_s5,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t3x = {
	.chip_id = chip_t3x,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t3x,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_txhd2 = {
	.chip_id = chip_txhd2,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vrr_support_flag = 0,
};

static const struct vecm_match_data_s vecm_dt_s7 = {
	.chip_id = chip_s7,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_null,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_a4 = {
	.chip_id = chip_a4,
	.chip_cls = AD_CHIP,
	.vlk_chip = vlock_chip_null,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

#endif

static const struct vecm_match_data_s vecm_dt_s1a = {
	.chip_id = chip_s1a,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static const struct vecm_match_data_s vecm_dt_sc2 = {
	.chip_id = chip_sc2,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_sm1,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};
#endif

static const struct vecm_match_data_s vecm_dt_s7d = {
	.chip_id = chip_s7d,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_s6 = {
	.chip_id = chip_s6,
	.chip_cls = STB_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};

static const struct vecm_match_data_s vecm_dt_t6d = {
	.chip_id = chip_t6d,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vlk_ctl_for_frc = 0,
	.vrr_support_flag = 0,
};

static const struct vecm_match_data_s vecm_dt_t6w = {
	.chip_id = chip_t6w,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t3x,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vlk_ctl_for_frc = 0,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_t6x = {
	.chip_id = chip_t6x,
	.chip_cls = TV_CHIP,
	.vlk_chip = vlock_chip_t5,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vlk_ctl_for_frc = 0,
	.vrr_support_flag = 1,
};

static const struct vecm_match_data_s vecm_dt_a9 = {
	.chip_id = chip_a9,
	.chip_cls = SMT_CHIP,
	.vlk_chip = vlock_chip_t7,
	.vlk_support = true,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = true,
	.vlk_pll_sel = vlock_pll_sel_tcon,
	.vrr_support_flag = 1,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static const struct vecm_match_data_s vecm_dt_c5 = {
	.chip_id = chip_c5,
	.chip_cls = AD_CHIP,
	.vlk_chip = vlock_chip_null,
	.vlk_support = false,
	.vlk_new_fsm = 1,
	.vlk_hwver = vlock_hw_tm2verb,
	.vlk_phlock_en = false,
	.vlk_pll_sel = vlock_pll_sel_tcon,
};
#endif

static const struct of_device_id aml_vecm_dt_match[] = {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	{
		.compatible = "amlogic, vecm",
		.data = &vecm_dt_xxx,
	},
#endif
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic, vecm-tl1",
		.data = &vecm_dt_tl1,
	},
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, vecm-sm1",
		.data = &vecm_dt_sm1,
	},
	{
		.compatible = "amlogic, vecm-tm2",
		.data = &vecm_dt_tm2,
	},
	{
		.compatible = "amlogic, vecm-tm2-verb",
		.data = &vecm_dt_tm2_verb,
	},
	{
		.compatible = "amlogic, vecm-t5",
		.data = &vecm_dt_t5,
	},
	{
		.compatible = "amlogic, vecm-t5d",
		.data = &vecm_dt_t5d,
	},
	{
		.compatible = "amlogic, vecm-t7",
		.data = &vecm_dt_t7,
	},
	{
		.compatible = "amlogic, vecm-t3",
		.data = &vecm_dt_t3,
	},
	{
		.compatible = "amlogic, vecm-t5w",
		.data = &vecm_dt_t5w,
	},
	{
		.compatible = "amlogic, vecm-t5m",
		.data = &vecm_dt_t5m,
	},
	{
		.compatible = "amlogic, vecm-s5",
		.data = &vecm_dt_s5,
	},
	{
		.compatible = "amlogic, vecm-t3x",
		.data = &vecm_dt_t3x,
	},
	{
		.compatible = "amlogic, vecm-txhd2",
		.data = &vecm_dt_txhd2,
	},
	{
		.compatible = "amlogic, vecm-s7",
		.data = &vecm_dt_s7,
	},
	{
		.compatible = "amlogic, vecm-a4",
		.data = &vecm_dt_a4,
	},
#endif
	{
		.compatible = "amlogic, vecm-s1a",
		.data = &vecm_dt_s1a,
	},
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, vecm-sc2",
		.data = &vecm_dt_sc2,
	},
#endif
	{
		.compatible = "amlogic, vecm-s7d",
		.data = &vecm_dt_s7d,
	},
	{
		.compatible = "amlogic, vecm-s6",
		.data = &vecm_dt_s6,
	},
	{
		.compatible = "amlogic, vecm-t6d",
		.data = &vecm_dt_t6d,
	},
	{
		.compatible = "amlogic, vecm-t6w",
		.data = &vecm_dt_t6w,
	},
	{
		.compatible = "amlogic, vecm-t6x",
		.data = &vecm_dt_t6x,
	},
	{
		.compatible = "amlogic, vecm-a9",
		.data = &vecm_dt_a9,
	},
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, vecm-c5",
		.data = &vecm_dt_c5,
	},
#endif
	{},
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
int pkt_adv_chip(void)
{
	int ret;

	if (chip_type_id == chip_t7 || chip_type_id == chip_a9 ||
		(chip_type_id >= chip_s5 && chip_cls_id == STB_CHIP))
		ret = 1;
	else
		ret = 0;

	return ret;
}
#endif

static void aml_vecm_match_init(struct vecm_match_data_s *pdata)
{
	chip_type_id = pdata->chip_id;
	chip_cls_id = pdata->chip_cls;
	pr_info("vecm chip id: %d, chip_cls : %d\n", chip_type_id, chip_cls_id);
}

static void aml_vecm_dt_parse(struct amvecm_dev_s *devp, struct platform_device *pdev)
{
	struct device_node *node;
	unsigned int val = 0;
	int ret;
	const struct of_device_id *of_id;
	struct vecm_match_data_s *matchdata;

	node = pdev->dev.of_node;
	/* get integer value */
	if (node) {
		ret = of_property_read_u32(node, "wb_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  wb_en.\n");
		else
			wb_en = val;

		ret = of_property_read_u32(node, "wb_sel", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  wb_sel.\n");
		else
			video_rgb_ogo_xvy_mtx = val;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		ret = of_property_read_u32(node, "gamma_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  gamma_en.\n");
		else
			gamma_en = val;

		ret = of_property_read_u32(node, "lut3d_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  lut3d_en.\n");
		else
			lut3d_en = val;

		ret = of_property_read_u32(node, "ct_3dlut", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  ct_3dlut.\n");
		else
			ct_en = val;

		ret = of_property_read_u32(node, "cm_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  cm_en.\n");
		else
			cm_en = val;

		ret = of_property_read_u32(node, "detect_colorbar", &val);
		if (ret) {
			pr_amvecm_dbg("Can't find  detect_colorbar.\n");
		} else {
			if (val == 0)
				pattern_mask =
				pattern_mask &
				(~PATTERN_MASK(PATTERN_75COLORBAR));
			else
				pattern_mask =
				pattern_mask |
				PATTERN_MASK(PATTERN_75COLORBAR);
		}

		ret = of_property_read_u32(node, "detect_face", &val);
		if (ret) {
			pr_amvecm_dbg("Can't find  detect_face.\n");
		} else {
			if (val == 0)
				pattern_mask =
				pattern_mask &
				(~PATTERN_MASK(PATTERN_SKIN_TONE_FACE));
			else
				pattern_mask =
				pattern_mask |
				PATTERN_MASK(PATTERN_SKIN_TONE_FACE);
		}

		ret = of_property_read_u32(node, "detect_corn", &val);
		if (ret) {
			pr_amvecm_dbg("Can't find  detect_corn.\n");
		} else {
			if (val == 0)
				pattern_mask =
				pattern_mask &
				(~PATTERN_MASK(PATTERN_GREEN_CORN));
			else
				pattern_mask =
				pattern_mask |
				PATTERN_MASK(PATTERN_GREEN_CORN);
		}

		ret = of_property_read_u32(node, "hist_sel", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  hist_sel.\n");
		else
			hist_chl = val;

		/*hdr:cfg:osd_100*/
		ret = of_property_read_u32(node, "cfg_en_osd_100", &val);
		if (ret) {
			hdr_set_cfg_osd_100(0);
			pr_amvecm_dbg("hdr:Can't find  cfg_en_osd_100.\n");

		} else {
			hdr_set_cfg_osd_100((int)val);
		}

		ret = of_property_read_u32(node, "tx_op_color_primary", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  tx_op_color_primary.\n");
		else
			tx_op_color_primary = val;

		ret = of_property_read_u32(node, "osd_pic_en", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  osd_pic_en.\n");
		else
			osd_pic_en = val;

		ret = of_property_read_u32(node, "enable_hdr10plus", &val);
		if (ret)
			pr_amvecm_dbg("Can't find enable_hdr10plus.\n");
		else
			enable_hdr10plus = val;
		pr_info("enable_hdr10plus =%d\n", enable_hdr10plus);

		ret = of_property_read_u32(node, "vpp_color_pri_sel", &val);
		if (ret)
			pr_amvecm_dbg("Can't find vpp_color_pri.\n");
		else
			vpp_color_pri_sel = val;
#endif

		/*get compatible matched device, to get chip related data*/
		of_id = of_match_device(aml_vecm_dt_match, &pdev->dev);
		if (of_id) {
			pr_amvecm_dbg("%s", of_id->compatible);
			matchdata = (struct vecm_match_data_s *)of_id->data;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
		} else {
			matchdata = (struct vecm_match_data_s *)&vecm_dt_xxx;
			pr_amvecm_dbg("unable to get matched device\n");
#endif
		}

		aml_vecm_match_init(matchdata);

		if (chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x)
			watermark_support = 1;

		ret = of_property_read_u32(node, "watermark_support", &val);
		if (ret)
			pr_amvecm_dbg("Can't find  watermark_support.\n");
		else
			watermark_support = val;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		vlock_dt_match_init(matchdata);

		frame_lock_set_vrr_support_flag(matchdata->vrr_support_flag);

		/*vlock param config*/
		vlock_param_config(node);
		vlock_clk_config(devp, &pdev->dev);

		vlock_status_init();

		/*vrr param config*/
		frame_lock_param_config(node);
#endif
	}

	/* init module status */
#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
	vpp_probe_enable();
#endif

//	pr_info("[debug] wb_en = %d\n", wb_en);
	if (chip_type_id != chip_t3x)
		amvecm_wb_init(wb_en);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t6x) {
		am_dma_init();
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_CM2_LC);
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_VD1_HDR_HIST);
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_VD2_HDR_HIST);
		am_dma_rd_buffer_malloc(pdev, EN_DMA_RD_ID_GAMUT0);
		am_dma_rd_buffer_malloc(pdev, EN_DMA_RD_ID_GAMUT1);
		am_dma_rd_buffer_malloc(pdev, EN_DMA_RD_ID_3DLUT);
		am_dma_set_mif_wr_status(1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_CM2_LC, 0);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VD1_HDR_HIST, 1);
		am_dma_set_mif_wr(EN_DMA_WR_ID_VD2_HDR_HIST, 1);
		am_dma_set_mif_rd(EN_DMA_RD_ID_GAMUT0, 0);
		am_dma_set_mif_rd(EN_DMA_RD_ID_GAMUT1, 0);
		am_dma_set_mif_rd(EN_DMA_RD_ID_3DLUT, 1);
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T5 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5D ||
		chip_type_id == chip_t6d)
		gamma_en = 0;

//	pr_info("[debug] gamma_en = %d\n", gamma_en);
	amvecm_gamma_init(gamma_en);
	amvecm_3dlut_init(lut3d_en);
	slt_lut3d_en = lut3d_en;
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (!is_amdv_enable())
		WRITE_VPP_REG_BITS(VPP_MISC, 1, 28, 1);
#endif
	if (chip_type_id != chip_s5 && chip_type_id != chip_t3x)
		WRITE_VPP_REG_BITS(VPP_MISC, 1, 28, 1);

	if (cm_en) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (!is_amdv_enable())
#endif
			amcm_enable(WR_VCB, 0);
	} else {
		amcm_disable(WR_VCB, 0);
	}
	/*pr_info("cm_init done.\n");*/

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	amvecm_set_osd_mtx_init(osd_pic_en);
//	pr_info("[debug] osd_pic_en = %d\n", osd_pic_en);

	res_viu2_vsync_irq =
		platform_get_resource_byname(pdev, IORESOURCE_IRQ, "vsync2");
	res_lc_curve_irq =
		platform_get_resource_byname(pdev, IORESOURCE_IRQ, "lc_curve");
#endif
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifdef CONFIG_AMLOGIC_LCD
static int aml_lcd_gamma_notifier(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	unsigned int *param;

	if ((event & LCD_EVENT_GAMMA_UPDATE) == 0)
		return NOTIFY_DONE;

	if (!data) {
		pr_amvecm_dbg("%s: %d\n", __func__, __LINE__);
		return NOTIFY_DONE;
	}

	param = (unsigned int *)data;
	/*gamma_index: select which vpp,  vpp0/vpp1/vpp2 gamma*/
	gamma_index = param[0];
	/*freesync 10 groups gamma table index, index:0~10
	 *0xff: default(not tcon) gamma
	 */
	gm_par_idx = param[1];

	if (gm_par_idx >= FREESYNC_DYNAMIC_GAMMA_NUM &&
		gm_par_idx != 0xff) {
		pr_amvecm_dbg("%s: %d\n", __func__, __LINE__);
		return NOTIFY_DONE;
	}

	if (gm_par_idx != 0xff) {
		if (!frame_lock_get_vrr_status())
			return NOTIFY_DONE;

		memcpy(&video_gamma_table_r,
			&gt.gm_tb[gm_par_idx][0],
			sizeof(struct tcon_gamma_table_s));
		memcpy(&video_gamma_table_g,
			&gt.gm_tb[gm_par_idx][1],
			sizeof(struct tcon_gamma_table_s));
		memcpy(&video_gamma_table_b,
			&gt.gm_tb[gm_par_idx][2],
			sizeof(struct tcon_gamma_table_s));
	}

	if (gamma_en) {
		vecm_latch_flag |= FLAG_GAMMA_TABLE_R;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_G;
		vecm_latch_flag |= FLAG_GAMMA_TABLE_B;
	}
	pr_amvecm_dbg("%s: gamma_index = %d, gm_par_idx = %d\n",
		__func__, gamma_index, gm_par_idx);
	return NOTIFY_OK;
}

static struct notifier_block aml_lcd_gamma_nb = {
	.notifier_call = aml_lcd_gamma_notifier,
};
#endif

static struct notifier_block vlock_notifier_nb = {
	.notifier_call	= vlock_notify_callback,
};

static struct notifier_block flock_vdin_vrr_en_notifier_nb = {
	.notifier_call	= flock_vrr_nfy_callback,
};

static int aml_vecm_viu2_vsync_irq_init(void)
{
	if (res_viu2_vsync_irq) {
		if (request_irq(res_viu2_vsync_irq->start,
				amvecm_viu2_vsync_isr, IRQF_SHARED,
				"amvecm_vsync2", (void *)"amvecm_vsync2")) {
			pr_err("can't request amvecm_vsync2_irq\n");
		} else {
			pr_info("request amvecm_vsync2_irq successful\n");
		}
	}

	return 0;
}

static int aml_vecm_lc_curve_irq_init(void)
{
	if (res_lc_curve_irq) {
		if (request_irq(res_lc_curve_irq->start,
				amvecm_lc_curve_isr, IRQF_SHARED,
				"lc_curve_isr", (void *)"lc_curve_isr")) {
			pr_err("can't request res_lc_curve_irq\n");
		} else {
			lc_curve_isr_defined = 1;
			pr_info("request res_lc_curve_irq successful\n");
		}
	} else {
		pr_info("no support res_lc_curve_irq\n");
	}

	return 0;
}
#endif

int get_hdr_conversion_cap(void)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (is_hdr_stb_mode()) {
		hdr_cap = 0x7fff;
		if (get_dv_support_info() == 7) {
			hdr_cap = 0xfffff;
		};
	} else if (is_hdr_tv_mode()) {
		hdr_cap = (1 << 3) | (1 << 5) | (1 << 8) | (1 << 10) | (1 << 13);
		if (get_dv_support_info() == 15) {
			hdr_cap |= 1 << 16;
		};
	}
#endif
	return hdr_cap;
}
EXPORT_SYMBOL(get_hdr_conversion_cap);

int get_hdr_cur_output(void)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int mode = 0;

	if (is_amdv_on() && is_amdv_enable()) {
		mode = get_amdv_mode();
		if (mode == AMDV_OUTPUT_MODE_IPT ||
			mode == AMDV_OUTPUT_MODE_IPT_TUNNEL)
			hdr_output_mode = HDR_OUTPUT_MODE_DOLBY_VISION;
		else if (mode == AMDV_OUTPUT_MODE_HDR10)
			hdr_output_mode = HDR_OUTPUT_MODE_HDR10;
		else if (mode == AMDV_OUTPUT_MODE_SDR8 ||
			mode == AMDV_OUTPUT_MODE_SDR10)
			hdr_output_mode = HDR_OUTPUT_MODE_SDR;
		else
			mode = output_format;
		if (mode == BT709 || mode == BT_BYPASS)
			hdr_output_mode = HDR_OUTPUT_MODE_SDR;
		else if (mode == BT2020_PQ)
			hdr_output_mode = HDR_OUTPUT_MODE_HDR10;
		else if (mode == BT2020_HLG)
			hdr_output_mode = HDR_OUTPUT_MODE_HLG;
		else if (mode == BT2020_PQ_DYNAMIC)
			hdr_output_mode = HDR_OUTPUT_MODE_HDR10PLUS;
		else if (mode == BT2020YUV_BT2020RGB_CUVA)
			hdr_output_mode = HDR_OUTPUT_MODE_CUVA_HDR;
	} else {
		mode = output_format;
		if (mode == BT709 || mode == BT_BYPASS)
			hdr_output_mode = HDR_OUTPUT_MODE_SDR;
		else if (mode == BT2020_PQ)
			hdr_output_mode = HDR_OUTPUT_MODE_HDR10;
		else if (mode == BT2020_HLG)
			hdr_output_mode = HDR_OUTPUT_MODE_HLG;
		else if (mode == BT2020_PQ_DYNAMIC)
			hdr_output_mode = HDR_OUTPUT_MODE_HDR10PLUS;
		else if (mode == BT2020YUV_BT2020RGB_CUVA)
			hdr_output_mode = HDR_OUTPUT_MODE_CUVA_HDR;
	}
#endif

	return hdr_output_mode;
}
EXPORT_SYMBOL(get_hdr_cur_output);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void set_hdr_output(int out)
{
	int mode = 0;

	if (is_amdv_on() && is_amdv_enable()) {
		if (out == HDR_OUTPUT_MODE_DOLBY_VISION)
			mode = AMDV_OUTPUT_MODE_IPT_TUNNEL;
		else if (out == HDR_OUTPUT_MODE_HDR10)
			mode = AMDV_OUTPUT_MODE_HDR10;
		else if (out == HDR_OUTPUT_MODE_SDR)
			mode = AMDV_OUTPUT_MODE_SDR8;
		else
			pr_info("not support amdv output mode %d\n", mode);
		set_amdv_mode(mode);
	} else {
		if (out == HDR_OUTPUT_MODE_HDR10)
			mode = BT2020_PQ;
		else if (out == HDR_OUTPUT_MODE_HLG)
			mode = BT2020_HLG;
		else if (out == HDR_OUTPUT_MODE_SDR)
			mode = BT709;
		else
			pr_info("not support hdr output mode %d\n", mode);
		set_force_output(mode);
	}
}
EXPORT_SYMBOL(set_hdr_output);

void amvecm_muxio_on_vs(struct vframe_s *vf,
	enum vpp_index_e vpp_index)
{
	update_muxio_mode(vf, vpp_index);
}
EXPORT_SYMBOL(amvecm_muxio_on_vs);

void amvecm_set_dpss_mode(unsigned int val)
{
	set_dpss_mode(val);
}
EXPORT_SYMBOL(amvecm_set_dpss_mode);

void amvecm_hdr_init_for_dpss(struct vframe_s *vf)
{
	hdr_init_for_dpss(vf);
}
EXPORT_SYMBOL(amvecm_hdr_init_for_dpss);

void amvecm_update_hdr_path_for_dpss(struct vframe_s *vf)
{
	update_hdr_path_for_dpss(vf);
}
EXPORT_SYMBOL(amvecm_update_hdr_path_for_dpss);

unsigned int amvecm_get_muxio_ready_for_dpss(void)
{
	unsigned int ret;

	ret = get_muxio_ready_for_dpss();
	return ret;
}
EXPORT_SYMBOL(amvecm_get_muxio_ready_for_dpss);

void amvecm_set_muxio_link_for_dpss(unsigned int link_flag,
	struct vframe_s *vf, enum vpp_index_e vpp_index)
{
	struct hdr_path_mux_sel_s *p = &h_p_s;

	if (p->pre_path_mux == PATH_VD1) {
		pr_log(0x400, "vd on vd1 path,dpss not cfg hdr path\n");
		return;
	}

	set_muxio_link_mode(link_flag, vf, vpp_index);
}
EXPORT_SYMBOL(amvecm_set_muxio_link_for_dpss);

int amvecm_signal_type_for_dpss(struct vframe_s *vf)
{
	int ret = 0;

	if (vf)
		ret = signal_type_detect_for_dpss(vf);

	return ret;
}
EXPORT_SYMBOL(amvecm_signal_type_for_dpss);

void amvecm_hdr_process_for_dpss(struct vframe_s *vf)
{
	if (vf)
		hdr_process_for_dpss(vf);
}
EXPORT_SYMBOL(amvecm_hdr_process_for_dpss);

void amvecm_hdr_calculate_for_dpss(struct vframe_s *vf)
{
	if (vf)
		calculate_dynamic_curve_for_dpss(vf);
}
EXPORT_SYMBOL(amvecm_hdr_calculate_for_dpss);

unsigned int amvecm_get_dpss_mode(void)
{
	return dpss_mode;
}
EXPORT_SYMBOL(amvecm_get_dpss_mode);

void hdr_path_switch_to_dpss(unsigned int val)
{
	struct hdr_path_mux_sel_s *p = &h_p_s;

	if (val > 3) {
		pr_csc(0x400, "sw dpss: set mode = %d, val error\n", val);
		return;
	}

	pr_csc(0x400, "sw dpss: pre_path_mux = %s, set mode = %d\n",
		pm_str[p->pre_path_mux], val);

	if (p->pre_path_mux != PATH_DPSS) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (is_amdv_enable()) {
			if (val == 1 || val == 3)
				update_dd_mode(DOLBY5_DPSS_MODE); //dv_dpss_mode
			else if (val == 2)
				update_dd_mode(DOLBY5_VD1_MODE);//dv_vpp_mode
			else
				update_dd_mode(DOLBY5_WRAP_BYPS);//dv off
		}
#endif
		if (val == 1 || val == 3)
			set_dpss_mode(1);
		else
			set_dpss_mode(0);

		if (val == 3)
			p->fst_frame = 1;
		else
			p->fst_frame = 0;
		p->path_mux = PATH_DPSS;
		if (p->pre_path_mux == PATH_DELINK)
			p->pre_path_mux = PATH_DPSS;
		pr_csc(0x400, "sw dpss: hdr path switch to %s\n", val ? "dpss" : "vd1");
	}
}
EXPORT_SYMBOL(hdr_path_switch_to_dpss);

int hdr_path_delink_status(void)
{
	int ret = 0;
	struct hdr_path_mux_sel_s *p = &h_p_s;

	if (p->delink_status == DELINK)
		ret = DELINK;
	else if (p->delink_status == LINK_DPSS &&
		p->pre_path_mux == PATH_DPSS)
		ret = LINK_DPSS;
	else
		ret = LINK_ON;

	return ret;
}
EXPORT_SYMBOL(hdr_path_delink_status);

void amvecm_vd1_dpss_switch_proc(struct vframe_s *vf,
	struct vframe_s *rpt_vf,
	enum vpp_index_e vpp_index)
{
	vd1_dpss_switch_proc(vf, rpt_vf, vpp_index);
}
EXPORT_SYMBOL(amvecm_vd1_dpss_switch_proc);

void amvecm_update_link_state(struct vframe_s *vf,
	struct vframe_s *rpt_vf,
	enum vpp_index_e vpp_index)
{
	update_link_state(vf, rpt_vf, vpp_index);
}
EXPORT_SYMBOL(amvecm_update_link_state);

void amvecm_set_ext_status_for_dpss(unsigned int val)
{
	set_dct_status_for_dpss(val);
}
EXPORT_SYMBOL(amvecm_set_ext_status_for_dpss);

void amvecm_set_lc_evc_ctrl_for_dpss(unsigned int enable,
	unsigned int lc_evc_src)
{
	set_lc_evc_ctrl_for_dpss(enable, lc_evc_src);
}
EXPORT_SYMBOL(amvecm_set_lc_evc_ctrl_for_dpss);
#endif

bool is_hdr10plus_enable(void)
{
	return enable_hdr10plus;
}
EXPORT_SYMBOL(is_hdr10plus_enable);

void set_hdr10plus_enable(bool enable)
{
	enable_hdr10plus = enable;
}
EXPORT_SYMBOL(set_hdr10plus_enable);

static int aml_vecm_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct amvecm_dev_s *devp = &amvecm_dev;

	memset(devp, 0, (sizeof(struct amvecm_dev_s)));

//	pr_info("\n VECM probe start\n");

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	hdr_lut_buffer_malloc(pdev);
#endif

	ret = alloc_chrdev_region(&devp->devno, 0, 1, AMVECM_NAME);
	if (ret < 0)
		goto fail_alloc_region;

	devp->clsp = class_create(AMVECM_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}

	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		if (class_create_file(devp->clsp, &amvecm_class_attrs[i]) < 0)
			goto fail_class_create_file;
	}

	cdev_init(&devp->cdev, &amvecm_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno,
		NULL, AMVECM_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	spin_lock_init(&vpp_lcd_gamma_lock);
	mutex_init(&vpp_lut3d_lock);

#ifdef CONFIG_AMLOGIC_LCD
	ret = aml_lcd_atomic_notifier_register(&aml_lcd_gamma_nb);
	if (ret)
		pr_info("register aml_lcd_gamma_notifier failed\n");

	INIT_WORK(&aml_lcd_vlock_param_work, vlock_lcd_param_work);
#endif

	/* register vout client */
	vout_register_client(&vlock_notifier_nb);
	/* register vdin vrr en client*/
	frame_lock_vrr_off_done_init();
	aml_vrr_atomic_notifier_register(&flock_vdin_vrr_en_notifier_nb);
	/* #endif */
#endif

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_txlx_cpu()) {
		vpp_set_12bit_datapath2();
		/*post matrix 12bit yuv2rgb*/
		/* mtx_sel_dbg |= 1 << VPP_MATRIX_2; */
		/* amvecm_vpp_mtx_debug(mtx_sel_dbg, 1);*/
		WRITE_VPP_REG(VPP_MATRIX_PROBE_POS, 0x1fff1fff);
	}
#endif

	if (is_meson_txhd_cpu()) {
		vpp_set_10bit_datapath1();
	} else if (is_meson_g12a_cpu() || is_meson_g12b_cpu()) {
		vpp_set_12bit_datapath_g12a();
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	memset(&vpp_hist_param.vpp_gamma[0],
		0, sizeof(unsigned short) * 64);

	hdr_flag = (1 << 0) | (1 << 1) | (0 << 2) | (0 << 3) | (1 << 4);

	/* box sdr_mode:auto, tv sdr_mode:off */
	/* disable contrast and saturation adjustment for HDR on TV */
	/* disable SDR to HDR convert on TV */
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_gxl_cpu() || is_meson_gxm_cpu()) {
		hdr_flag = (1 << 0) | (1 << 1) | (1 << 2) | (1 << 3);
	}
#endif

	hdr_init(&amvecm_dev.hdr_d);
#endif

	aml_vecm_dt_parse(devp, pdev);
	init_pq_rdma_part_ins();

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_LC_STTS_0);
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_LC_STTS_1);
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_VI_HIST_SPL_0);
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_VI_HIST_SPL_1);
		/*am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_CM2_HIST_0);*/
		/*am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_CM2_HIST_1);*/
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_VD1_HDR_0);
		am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_VD1_HDR_1);
		/*am_dma_buffer_malloc(pdev, EN_DMA_WR_ID_VD2_HDR);*/
		am_dma_init();
	}

	init_pattern_detect();
	vpp_get_hist_en();
#endif
	init_pq_setting();

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	aml_vecm_viu2_vsync_irq_init();
	lc_curve_isr_defined = 0;
	aml_vecm_lc_curve_irq_init();

	pkt_delay_flag_init();

	aml_cabc_queue = create_workqueue("cabc workqueue");
	if (!aml_cabc_queue) {
		pr_amvecm_dbg("cacb queue create failed");
		ret = -1;
		goto fail_create_wq;
	}
	INIT_WORK(&cabc_proc_work, aml_cabc_alg_process);
	INIT_WORK(&cabc_bypass_work, aml_cabc_alg_bypass);
#endif

	probe_ok = 1;
	pr_info("%s: ok\n", __func__);
	return 0;

fail_create_device:
	pr_info("[amvecm.] : amvecm device create error.\n");
	cdev_del(&devp->cdev);
	return ret;
fail_add_cdev:
	pr_info("[amvecm.] : amvecm add device error.\n");
	return ret;
fail_class_create_file:
	pr_info("[amvecm.] : amvecm class create file error.\n");
	for (i = 0; amvecm_class_attrs[i].attr.name; i++) {
		class_remove_file(devp->clsp,
			&amvecm_class_attrs[i]);
	}
	class_destroy(devp->clsp);
	return ret;
fail_create_class:
	pr_info("[amvecm.] : amvecm class create error.\n");
	unregister_chrdev_region(devp->devno, 1);
	return ret;
fail_alloc_region:
	pr_info("[amvecm.] : amvecm alloc error.\n");
	pr_info("[amvecm.] : amvecm_init.\n");
	return ret;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
fail_create_wq:
	pr_info("[amvecm.] : amvecm create wq error\n");
	return ret;
#endif
}

static __maybe_unused void aml_vecm_remove(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (res_viu2_vsync_irq) {
		free_irq(res_viu2_vsync_irq->start,
			 (void *)"amvecm_vsync2");
	}

	hdr_lut_buffer_free(pdev);

	if (chip_type_id == chip_t3x) {
		am_dma_set_mif_wr_status(0);
		am_dma_buffer_free(pdev, EN_DMA_WR_ID_LC_STTS_0);
		am_dma_buffer_free(pdev, EN_DMA_WR_ID_LC_STTS_1);
		am_dma_buffer_free(pdev, EN_DMA_WR_ID_VI_HIST_SPL_0);
		am_dma_buffer_free(pdev, EN_DMA_WR_ID_VI_HIST_SPL_1);
		/*am_dma_buffer_free(pdev, EN_DMA_WR_ID_CM2_HIST_0);*/
		/*am_dma_buffer_free(pdev, EN_DMA_WR_ID_CM2_HIST_1);*/
		am_dma_buffer_free(pdev, EN_DMA_WR_ID_VD1_HDR_0);
		am_dma_buffer_free(pdev, EN_DMA_WR_ID_VD1_HDR_1);
		/*am_dma_buffer_free(pdev, EN_DMA_WR_ID_VD2_HDR);*/
	}

	hdr_exit();
#endif
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifdef CONFIG_AMLOGIC_LCD
	aml_lcd_notifier_unregister(&aml_lcd_gamma_nb);
	cancel_work_sync(&aml_lcd_vlock_param_work);
#endif
	vout_unregister_client(&vlock_notifier_nb);
	aml_vrr_atomic_notifier_unregister(&flock_vdin_vrr_en_notifier_nb);
#endif
	probe_ok = 0;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	lc_free();
#endif
	pr_info("[amvecm.] : amvecm_exit.\n");
}

#ifdef CONFIG_PM
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static int amvecm_drv_suspend(struct device *dev)
{
	if (probe_ok == 1)
		probe_ok = 0;

	if (chip_type_id == chip_t5w ||
		chip_type_id == chip_t3 ||
		chip_type_id == chip_t7 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_c5 ||
		is_meson_g12b_cpu()) {
		if (!suspend_drv_status_get()) {
			suspend_drv_status_set(true);
			suspend_cm();
			suspend_sr();
			suspend_vsr_sharpness();
			if (chip_cls_id == TV_CHIP &&
				cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				suspend_lc();
			suspend_ve();
			suspend_matrix();
		}
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
		vlock_clk_suspend();

	if (hdr_policy == 0x2 &&
		chip_cls_id == TV_CHIP) {
		hdr_policy_bak = hdr_policy;
//		pr_info("amvecm: hdr_policy_bak=0x%x\n",
//			hdr_policy_bak);
	}
	pr_info("amvecm: suspend module\n");
	return 0;
}

static int amvecm_drv_resume(struct device *dev)
{
	if (probe_ok == 0)
		probe_ok = 1;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
		vlock_clk_resume();

	if (chip_type_id == chip_t5w)
		resume_hdr_clk_gate();

	if (chip_type_id == chip_t7)
		resume_mtx_t7();

	resume_dma();
	resume_gamut_wrapper();

	if (chip_type_id == chip_t5w ||
		chip_type_id == chip_t3 ||
		chip_type_id == chip_t7 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_c5 ||
		is_meson_g12b_cpu()) {
		if (suspend_drv_status_get()) {
			resume_cm(0);
			vecm_latch_flag2 |= FLAG_RESUME_RECOVERY;
			resume_mtx_flag_set(true);
		}
	}

	reset_hdr_path_cfg();
	pr_info("amvecm: resume module\n");
	return 0;
}
#endif
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static int amvecm_drv_freeze(struct device *dev)
{
	if (probe_ok == 1)
		probe_ok = 0;

	if (chip_type_id == chip_t5w ||
		chip_type_id == chip_t7 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t3 ||
		chip_type_id == chip_c5 ||
		chip_type_id == chip_t6x) {
		if (!suspend_drv_status_get()) {
			suspend_drv_status_set(true);
			suspend_cm();
			suspend_sr();
			suspend_vsr_sharpness();
			if (chip_cls_id == TV_CHIP &&
				cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				suspend_lc();
			suspend_ve();
			suspend_matrix();
		}
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
		vlock_clk_suspend();
	pr_info("amvecm: freeze module\n");
	return 0;
}

static int amvecm_drv_thaw(struct device *dev)
{
	if (probe_ok == 0)
		probe_ok = 1;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
		vlock_clk_resume();

	if (chip_type_id == chip_t5w)
		resume_hdr_clk_gate();

	resume_dma();
	resume_gamut_wrapper();

	if (chip_type_id == chip_t5w ||
		chip_type_id == chip_t7 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t3 ||
		chip_type_id == chip_c5 ||
		chip_type_id == chip_t6x) {
		if (suspend_drv_status_get()) {
			vecm_latch_flag2 |= FLAG_RESUME_RECOVERY;
			resume_mtx_flag_set(true);
		}
	}

	reset_hdr_path_cfg();
	pr_info("amvecm: thaw module\n");
	return 0;
}

static int amvecm_drv_restore(struct device *dev)
{
	if (probe_ok == 0)
		probe_ok = 1;

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T5D))
		vlock_clk_resume();

	if (chip_type_id == chip_t5w)
		resume_hdr_clk_gate();

	if (chip_type_id == chip_t7)
		resume_mtx_t7();

	resume_dma();
	resume_gamut_wrapper();

	if (chip_type_id == chip_t5w ||
		chip_type_id == chip_t7 ||
		chip_type_id == chip_s7 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_s6 ||
		chip_type_id == chip_t3 ||
		chip_type_id == chip_c5 ||
		chip_type_id == chip_t6x) {
		if (suspend_drv_status_get()) {
			vecm_latch_flag2 |= FLAG_RESUME_RECOVERY;
			resume_mtx_flag_set(true);
		}
	}

	reset_hdr_path_cfg();
	pr_info("amvecm: restore module\n");
	return 0;
}
#endif
static void amvecm_shutdown(struct platform_device *pdev)
{
	struct amvecm_dev_s *devp = &amvecm_dev;

	if (chip_type_id != chip_a4 &&
		chip_type_id != chip_c5) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		hdr_exit();
		ve_disable_dnlp();
		amcm_disable(WR_VCB, 0);
#endif
		WRITE_VPP_REG(VPP_VADJ_CTRL, 0x0);
		amvecm_wb_enable(0);
		/*dnlp cm vadj1 wb gate*/
		WRITE_VPP_REG(VPP_GCLK_CTRL0, 0x11000400);
		WRITE_VPP_REG(VPP_GCLK_CTRL1, 0x14);
	}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (res_lc_curve_irq && lc_curve_isr_defined) {
		free_irq(res_lc_curve_irq->start,
			 (void *)"lc_curve_isr");
	}
#endif
	pr_info("amvecm: shutdown module\n");

	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	vlock_clk_suspend();
#ifdef CONFIG_AML_LCD
	aml_lcd_notifier_unregister(&aml_lcd_gamma_nb);
#endif
	if (chip_type_id != chip_a4 &&
		chip_type_id != chip_c5) {
		lc_free();
		vpp_lut3d_table_release();
		lut_release();
	}
#endif
}

#ifdef CONFIG_PM
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static const struct dev_pm_ops amvecm_pm_ops = {
	.freeze = amvecm_drv_freeze,
	.thaw = amvecm_drv_thaw,
	.restore = amvecm_drv_restore,
	.suspend = amvecm_drv_suspend,
	.resume = amvecm_drv_resume,
};
#endif
#endif

static struct platform_driver aml_vecm_driver = {
	.driver = {
		.name = "aml_vecm",
		.owner = THIS_MODULE,
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		.pm = &amvecm_pm_ops,
#endif
		.of_match_table = aml_vecm_dt_match,
	},
	.probe = aml_vecm_probe,
	.shutdown = amvecm_shutdown,
	.remove = __exit_p(aml_vecm_remove),
};

int __init aml_vecm_init(void)
{
	/*unsigned int hiu_reg_base;*/

	pr_info("%s: %s\n", __func__, AMVECM_VERSION);

	if (platform_driver_register(&aml_vecm_driver)) {
		pr_err("failed to register bl driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit aml_vecm_exit(void)
{
	pr_info("%s:module exit\n", __func__);
	/*iounmap(amvecm_hiu_reg_base);*/
	platform_driver_unregister(&aml_vecm_driver);
}

//MODULE_VERSION(AMVECM_VER);
//MODULE_DESCRIPTION("AMLOGIC amvecm driver");
//MODULE_LICENSE("GPL");

