// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>
/* #include <mach/am_regs.h> */
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/amvecm/ve.h>
/* #include <linux/amlogic/aml_common.h> */
/* media module used media/registers/cpu_version.h since kernel 5.4 */
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include "arch/vpp_regs.h"
#include "arch/ve_regs.h"
#include "arch/vpp_s7d_sr_regs.h"
#include "amve.h"
#include <linux/io.h>
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include "arch/vpp_a4_regs.h"
#include "amve_gamma_table.h"
#include "dnlp_cal.h"
#include "local_contrast.h"
#include "amve_v2.h"
#endif
#include "amcm.h"
#include "reg_helper.h"
#include "amcsc.h"
#include "arch/vpp_hdr_regs.h"
#include "am_dma_ctrl.h"
#include "blue_stretch/blue_str.h"

#define pr_amve_dbg(fmt, args...)\
	do {\
		if (amve_debug & 0x1)\
			pr_info("AMVE: " fmt, ## args);\
	} while (0)\
/* #define pr_amve_error(fmt, args...) */
/* printk(KERN_##(KERN_INFO) "AMVECM: " fmt, ## args) */

#define pr_amve_bringup_dbg(fmt, args...)\
	do {\
		if (amve_bringup_debug & 0x1)\
			pr_info("amve_bringup: " fmt, ## args);\
	} while (0)

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#define GAMMA_RETRY        1000

unsigned int gamma_loadprotect_en;
unsigned int gamma_update_flag_r;
unsigned int gamma_update_flag_g;
unsigned int gamma_update_flag_b;
unsigned int gamma_disable_flag;
#endif

/* 0: Invalid */
/* 1: Valid */
/* 2: Updated in 2D mode */
/* 3: Updated in 3D mode */
unsigned long flags;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
/* #if (MESON_CPU_TYPE>=MESON_CPU_TYPE_MESONG9TV) */
#define NEW_DNLP_IN_SHARPNESS 2
#define NEW_DNLP_IN_VPP 1

unsigned int dnlp_sel = NEW_DNLP_IN_SHARPNESS;
/* #endif */
#endif

int flag_lc_evc;
int flag_lc_evc_src;
int cur_contrast_v;
int cur_contrast_u;
int cur_vpp_index;

int amve_debug;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
int amve_bringup_debug;
int fmeter_en = 1;
int fmeter_slt;
struct ve_hist_s video_ve_hist;
#endif

unsigned int vpp_log[128][10];

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct tcon_gamma_table_s video_gamma_table_r;
struct tcon_gamma_table_s video_gamma_table_g;
struct tcon_gamma_table_s video_gamma_table_b;
struct tcon_gamma_table_s video_gamma_table_r_sub;
struct tcon_gamma_table_s video_gamma_table_g_sub;
struct tcon_gamma_table_s video_gamma_table_b_sub;
struct tcon_gamma_table_s video_gamma_table_r_adj;
struct tcon_gamma_table_s video_gamma_table_g_adj;
struct tcon_gamma_table_s video_gamma_table_b_adj;
struct tcon_gamma_table_s video_gamma_table_ioctl_set;
struct gm_tbl_s gt;
unsigned int gamma_index;
unsigned int gm_par_idx;
unsigned int gamma_index_sub = 1;
#endif

struct tcon_rgb_ogo_s video_rgb_ogo = {
	0, /* wb enable */
	0, /* -1024~1023, r_pre_offset */
	0, /* -1024~1023, g_pre_offset */
	0, /* -1024~1023, b_pre_offset */
	1024, /* 0~2047, r_gain */
	1024, /* 0~2047, g_gain */
	1024, /* 0~2047, b_gain */
	0, /* -1024~1023, r_post_offset */
	0, /* -1024~1023, g_post_offset */
	0  /* -1024~1023, b_post_offset */
};

struct tcon_rgb_ogo_s video_rgb_ogo_sub = {
	0, /* wb enable */
	0, /* -1024~1023, r_pre_offset */
	0, /* -1024~1023, g_pre_offset */
	0, /* -1024~1023, b_pre_offset */
	1024, /* 0~2047, r_gain */
	1024, /* 0~2047, g_gain */
	1024, /* 0~2047, b_gain */
	0, /* -1024~1023, r_post_offset */
	0, /* -1024~1023, g_post_offset */
	0  /* -1024~1023, b_post_offset */
};


#define FLAG_LVDS_FREQ_SW1       BIT(6)

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
int dnlp_en;/* 0:disable;1:enable */
module_param(dnlp_en, int, 0664);
MODULE_PARM_DESC(dnlp_en, "\n enable or disable dnlp\n");

static int dnlp_status = 1;/* 0:done;1:todo */
int dnlp_en_2_pre = 1;
int dnlp_en_2 = 1;/* 0:disable;1:enable */
int dnlp_en_dsw = 1;/* 0:disable;1:enable */
int lut3d_en;/* lut3d_en enable/disable */
int lut3d_order;/* 0 RGB 1 GBR 2 BGR*/
int lut3d_debug;
#endif

/*static int frame_lock_freq;*/
int video_rgb_ogo_mode_sw;
int video_rgb_ogo_xvy_mtx;

int lut3d_dma_case = 1;

int video_rgb_ogo_xvy_mtx_latch;

int lc_evc[LC_EVC_SIZE] = {
	0x08000000, 0x08000000, 0x00000800, 0x1f001f00, 0x00001f00
};

static unsigned int assist_cnt;/* ASSIST_SPARE8_REG1; */

/*0: recovery mode
 *1: certification mode
 *2: bypass mode, vadj1 follow config for ui setting
 */
int dv_pq_bypass;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
/* 3d sync parts begin */
unsigned int sync_3d_h_start;
unsigned int sync_3d_h_end;
unsigned int sync_3d_v_start = 10;
unsigned int sync_3d_v_end = 20;
unsigned int sync_3d_polarity;
unsigned int sync_3d_out_inv;
unsigned int sync_3d_black_color = 0x008080;/* yuv black */
/* 3d sync to v by one enable/disable */
unsigned int sync_3d_sync_to_vbo;
/* 3d sync parts end */
#endif

unsigned int contrast_adj_sel;/*0:vdj1, 1:vd1 mtx rgb contrast*/

/*gxlx adaptive sr level*/
static unsigned int sr_adapt_level;

/*sharpness gain for ai pq*/
int sr_gain[2];

#define RDMA_TABLE_NUM_MAX 4

static struct rdma_partition_ins_s pq_rdma_part_ins[RDMA_TABLE_NUM_MAX];

bool pq_rdma_init;
static bool use_rdma_part_tables = true;

static int ep_ipt_flag;
int input_444_mode;

/* *********************************************************************** */
/* *** VPP_FIQ-oriented functions **************************************** */
/* *********************************************************************** */
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void ve_hist_gamma_tgt(struct vframe_s *vf, struct vpp_hist_param_s *vp)
{
	int ave_luma;

	video_ve_hist.sum    = vp->vpp_luma_sum;
	video_ve_hist.width  = vp->vpp_width;
	video_ve_hist.height = vp->vpp_height;

	video_ve_hist.ave =
		video_ve_hist.sum / (video_ve_hist.height *
				video_ve_hist.width);
	if ((vf->source_type == VFRAME_SOURCE_TYPE_OTHERS &&
	     is_meson_gxtvbb_cpu()) ||
	     cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
		ave_luma = video_ve_hist.ave;
		ave_luma = (ave_luma - 16) < 0 ? 0 : (ave_luma - 16);
		video_ve_hist.ave = ave_luma * 255 / (235 - 16);
		if (video_ve_hist.ave > 255)
			video_ve_hist.ave = 255;
	}
}

void ve_hist_gamma_reset(void)
{
	if (video_ve_hist.height != 0 ||
	    video_ve_hist.width != 0)
		memset(&video_ve_hist, 0, sizeof(struct ve_hist_s));
}

void ve_dnlp_load_reg(void)
{
	int i;
	int dnlp_reg = 0;

	if (chip_type_id != chip_t3x) {
		if (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
			if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
				for (i = 0; i < 16; i++)
					WRITE_VPP_REG(SRSHARP1_DNLP_00 + i,
						ve_dnlp_reg[i]);
			} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
				if ((!vinfo_lcd_support() && chip_type_id != chip_s5) ||
					is_meson_t7_cpu() ||
					chip_type_id == chip_txhd2)
					dnlp_reg = SRSHARP0_DNLP2_00;
				else
					dnlp_reg = SRSHARP1_DNLP2_00;

				if (chip_type_id == chip_s7d ||
					chip_type_id == chip_s6 ||
					chip_type_id == chip_t6d ||
					chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x ||
					chip_type_id == chip_a9)
					dnlp_reg = VPP_DNLP_YGRID_0;

				for (i = 0; i < 32; i++)
					WRITE_VPP_REG(dnlp_reg + i,
						ve_dnlp_reg_v2[i]);
			} else {
				for (i = 0; i < 16; i++)
					WRITE_VPP_REG(SRSHARP0_DNLP_00 + i,
						ve_dnlp_reg[i]);
			}
		} else {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(VPP_DNLP_CTRL_00 + i,
					ve_dnlp_reg[i]);
		}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	} else {
		ve_dnlp_set(ve_dnlp_reg_v2);
#endif
	}
}

static void ve_dnlp_load_def_reg(void)
{
	int i;
	int dnlp_reg = 0;

	if  (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
		if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP1_DNLP_00 + i,
					ve_dnlp_reg[i]);
		} else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1)) {
			if (!vinfo_lcd_support() ||
				is_meson_t7_cpu() ||
				chip_type_id == chip_txhd2)
				dnlp_reg = SRSHARP0_DNLP2_00;
			else
				dnlp_reg = SRSHARP1_DNLP2_00;

			if (chip_type_id == chip_s7d || chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d || chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x || chip_type_id == chip_a9)
				dnlp_reg = VPP_DNLP_YGRID_0;

			for (i = 0; i < 32; i++)
				WRITE_VPP_REG(dnlp_reg + i,
					ve_dnlp_reg_v2[i]);
		} else {
			for (i = 0; i < 16; i++)
				WRITE_VPP_REG(SRSHARP0_DNLP_00 + i,
					ve_dnlp_reg[i]);
		}
	} else {
		for (i = 0; i < 16; i++)
			WRITE_VPP_REG(VPP_DNLP_CTRL_00 + i,
				ve_dnlp_reg_def[i]);
	}
}

void ve_on_vs(struct vframe_s *vf, int vpp_index, struct vpp_hist_param_s *vp)
{
	if (dnlp_en_dsw) {
		/* calculate dnlp target data */
		if (ve_dnlp_calculate_tgtx(vf, vpp_index, vp)) {
			/* calculate dnlp low-pass-filter data */
			ve_dnlp_calculate_lpf();
			/* calculate dnlp reg data */
			ve_dnlp_calculate_reg();
			/* load dnlp reg data */
			ve_dnlp_load_reg();
		}
	}

	/* sharpness process */
}

void dnlp_en_update(int vpp_index)
{
	if (dnlp_en_2_pre != dnlp_en_2) {
		dnlp_en_2_pre = dnlp_en_2;

		if (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
			if (is_meson_gxlx_cpu() || is_meson_txlx_cpu())
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_DNLP_EN,
					dnlp_en_2, 0, 1, vpp_index);
			else if (chip_type_id == chip_s7d || chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d || chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x || chip_type_id == chip_a9)
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_DNLP_EN_MODE,
						dnlp_en_2, 4, 1, vpp_index);
			else if (cpu_after_eq(MESON_CPU_MAJOR_ID_TL1))
				if (!vinfo_lcd_support() || is_meson_t7_cpu())
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_DNLP_EN,
						dnlp_en_2, 0, 1, vpp_index);
				else
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_DNLP_EN,
						dnlp_en_2, 0, 1, vpp_index);
			else
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_DNLP_EN,
					dnlp_en_2, 0, 1, vpp_index);
		} else {
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				dnlp_en_2, DNLP_EN_BIT, DNLP_EN_WID);
		}
	}
}
/* *********************************************************************** */
/* *** IOCTL-oriented functions ****************************************** */
/* *********************************************************************** */
int vpp_get_encl_viu_mux(void)
{
	unsigned int temp;

	temp = READ_VPP_REG(VPU_VIU_VENC_MUX_CTRL);
	if ((temp & 0x3) == 0)
		return 1;
	if (((temp >> 2) & 0x3) == 0)
		return 2;
	return 0;
}

int vpp_get_vout_viu_mux(void)
{
	unsigned int temp = 0;
	struct vinfo_s *vinfo1, *vinfo2, *vinfo3;

	vinfo1 = NULL;
	vinfo2 = NULL;
	vinfo3 = NULL;
	#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vinfo1 = get_current_vinfo();
	if (vinfo1 && vinfo1->mode == 2) {
		temp = (vinfo1->viu_mux >> 4) & 0xf;
		if (temp == 0)
			temp = 1;
		else if (temp == 1)
			temp = 2;
		else if (temp > 1)
			temp = 3;
		return temp;
	}
	#endif

	#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vinfo2 = get_current_vinfo2();
	if (vinfo2 && vinfo2->mode == 2) {
		temp = (vinfo2->viu_mux >> 4) & 0xf;
		if (temp == 0)
			temp = 1;
		else if (temp == 1)
			temp = 2;
		else if (temp > 1)
			temp = 3;
		return temp;
	}
	#endif

	#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vinfo3 = get_current_vinfo3();
	if (vinfo3 && vinfo3->mode == 2) {
		temp = (vinfo3->viu_mux >> 4) & 0xf;
		if (temp == 0)
			temp = 1;
		else if (temp == 1)
			temp = 2;
		else if (temp > 1)
			temp = 3;
		return temp;
	}
	#endif

	return temp;
}

void vpp_enable_lcd_gamma_table(int viu_sel, int rdma_write, int vpp_index)
{
	unsigned int offset = 0x0;
	unsigned int reg_ctrl = L_GAMMA_CNTL_PORT;
	unsigned int data = 0x0;

	if (viu_sel == 0) /*venc0*/
		offset = 0;
	else if (viu_sel == 1) /*venc1*/
		offset = 0x100;
	else if (viu_sel == 2) /*venc2*/
		offset = 0x200;

	if (chip_type_id == chip_t6x) {
		if (viu_sel == 0) /*venc0*/
			offset = 0x5e35;
	}

	if (cpu_after_eq_t7()) {
		if (chip_type_id == chip_a4 ||
			chip_type_id == chip_c5) {
			reg_ctrl = LCD_GAMMA_CNTL_PORT0_A4;
		} else if (chip_type_id == chip_t3x) {
			reg_ctrl = 0x14e9;
		} else if (chip_type_id == chip_txhd2) {
			reg_ctrl = L_GAMMA_CNTL_PORT;
			rdma_write = 1;
		} else {
			reg_ctrl = LCD_GAMMA_CNTL_PORT0;
		}
	}

	if (cpu_after_eq_t7()) {
		pr_amve_bringup_dbg("%s: reg_ctrl = %d, rdma_write/offset = %d/%d\n",
			__func__, reg_ctrl, rdma_write, offset);

		data = READ_VPP_REG(reg_ctrl + offset);
		pr_amve_dbg("\n[%s] enable_lcd_gamma_table data = 0x%x\n",
			__func__, data);

		if (data & 0x1)
			return;

		if (rdma_write)
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctrl + offset,
				(data | 0x1), vpp_index);
		else
			WRITE_VPP_REG_BITS(reg_ctrl + offset,
				1, L_GAMMA_EN, 1);
	} else {
		if (rdma_write == 1) /*viu1 vsync rdma*/
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg_ctrl,
				1, GAMMA_EN, 1, vpp_index);
		else
			WRITE_VPP_REG_BITS(reg_ctrl,
				1, GAMMA_EN, 1);
	}

	pr_amve_dbg("\n[%s] set enable_lcd_gamma_table.\n", __func__);
}

void vpp_disable_lcd_gamma_table(int viu_sel, int rdma_write, int vpp_index)
{
	unsigned int offset = 0x0;
	unsigned int reg_ctrl = L_GAMMA_CNTL_PORT;
	unsigned int data = 0x0;

	if (viu_sel == 0) /*venc0*/
		offset = 0;
	else if (viu_sel == 1) /*venc1*/
		offset = 0x100;
	else if (viu_sel == 2) /*venc2*/
		offset = 0x200;

	if (chip_type_id == chip_t6x) {
		if (viu_sel == 0) /*venc0*/
			offset = 0x5e35;
	}

	if (cpu_after_eq_t7()) {
		if (chip_type_id == chip_a4 ||
			chip_type_id == chip_c5) {
			reg_ctrl = LCD_GAMMA_CNTL_PORT0_A4;
		} else if (chip_type_id == chip_t3x) {
			reg_ctrl = 0x14e9;
		} else if (chip_type_id == chip_txhd2) {
			reg_ctrl = L_GAMMA_CNTL_PORT;
			rdma_write = 1;
		} else {
			reg_ctrl = LCD_GAMMA_CNTL_PORT0;
		}
	}

	if (cpu_after_eq_t7()) {
		pr_amve_bringup_dbg("%s: reg_ctrl = %d, rdma_write/offset = %d/%d\n",
			__func__, reg_ctrl, rdma_write, offset);

		data = READ_VPP_REG(reg_ctrl + offset);
		pr_amve_dbg("\n[%s] disable_lcd_gamma_table data = 0x%x\n", __func__, data);

		if (!(data & 0x1))
			return;

		if (rdma_write)
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_ctrl + offset,
				(data & 0xfffffffe), vpp_index);
		else
			WRITE_VPP_REG_BITS(reg_ctrl + offset,
				0, L_GAMMA_EN, 1);
	} else {
		if (rdma_write == 1) /*viu1 vsync rdma*/
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg_ctrl,
				0, GAMMA_EN, 1, vpp_index);
		else
			WRITE_VPP_REG_BITS(reg_ctrl,
				0, GAMMA_EN, 1);
	}

	pr_amve_dbg("\n[%s] set disable_lcd_gamma_table.\n", __func__);
}

/*new gamma interface, start from T7*/
/*data[3][256] is r/g/b 256 data
 *wr_mod:  reg write mode: rdma or vcbus
 *rw_mod:  read gamma or write gamma
 */
void lcd_gamma_api(unsigned int index,
	u16 *r_data, u16 *g_data, u16 *b_data,
	enum wr_md_e wr_mod, enum rw_md_e rw_mod, int vpp_index)
{
	int i;
	unsigned int val;
	unsigned int offset = 0;
	int auto_inc = 0;
	int max_idx = 0;
	struct gamma_data_s *p_gm;
	unsigned int reg_addr = LCD_GAMMA_ADDR_PORT0;
	unsigned int reg_data = LCD_GAMMA_DATA_PORT0;

	/*force init gamma*/
	/*if (!gamma_en)*/
	/*	return;*/

	if (index == 0)
		offset = 0;
	else if (index == 1)
		offset = 0x100;
	else if (index == 2)
		offset = 0x200;

	if (chip_type_id == chip_t6x) {
		if (index == 0) /*venc0*/
			offset = 0x5e35;
	}

	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		auto_inc = 0x1 << L_H_AUTO_INC_2;
		max_idx = 257;
	} else if (chip_type_id == chip_t5m ||
		chip_type_id == chip_t3x ||
		chip_type_id == chip_txhd2 ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		p_gm = get_gm_data();
		auto_inc = p_gm->auto_inc;
		max_idx = p_gm->max_idx;
	} else {
		auto_inc = 0x1 << L_H_AUTO_INC;
		max_idx = 256;
	}

	if (chip_type_id == chip_a4 ||
		chip_type_id == chip_c5) {
		reg_addr = LCD_GAMMA_ADDR_PORT0_A4;
		reg_data = LCD_GAMMA_DATA_PORT0_A4;
	} else if (chip_type_id == chip_t3x) {
		reg_addr = 0x14eb;
		reg_data = 0x14ea;
	} else if (chip_type_id == chip_txhd2) {
		reg_addr = L_GAMMA_ADDR_PORT;
		reg_data = L_GAMMA_DATA_PORT;
	}

	pr_amve_bringup_dbg("%s: addr/data = %d/%d, auto_inc/offset = %d/%d\n",
		__func__, reg_addr, reg_data, auto_inc, offset);

	if (rw_mod == RD_MOD) {
		WRITE_VPP_REG(reg_addr + offset,
			auto_inc);
		for (i = 0; i < max_idx; i++) {
			val = READ_VPP_REG(reg_data + offset);
			r_data[i] = (val >> L_GAMMA_R) & 0x3ff;
			g_data[i] = (val >> L_GAMMA_G) & 0x3ff;
			b_data[i] = (val >> L_GAMMA_B) & 0x3ff;
		}
		return;
	}

	switch (wr_mod) {
	case WR_VCB:
		WRITE_VPP_REG(reg_addr + offset,
			auto_inc);
		for (i = 0; i < max_idx; i++)
			WRITE_VPP_REG(reg_data + offset,
			(r_data[i] << L_GAMMA_R) |
			(g_data[i] << L_GAMMA_G) |
			(b_data[i] << L_GAMMA_B));
		break;
	case WR_DMA:
		VSYNC_WRITE_VPP_REG_VPP_SEL(reg_addr + offset,
			auto_inc, vpp_index);
		for (i = 0; i < max_idx; i++)
			VSYNC_WRITE_VPP_REG_VPP_SEL(reg_data + offset,
			(r_data[i] << L_GAMMA_R) |
			(g_data[i] << L_GAMMA_G) |
			(b_data[i] << L_GAMMA_B), vpp_index);
		break;
	default:
		break;
	}
}

void vpp_set_lcd_gamma_table(u16 *data, u32 rgb_mask, int viu_sel)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}

	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

u16 gamma_data_r[257] = {0};
u16 gamma_data_g[257] = {0};
u16 gamma_data_b[257] = {0};
void vpp_get_lcd_gamma_table(u32 rgb_mask)
{
	int i;
	int cnt = 0;
	struct gamma_data_s *p_gm;

	if (cpu_after_eq_t7()) {
		if (chip_type_id == chip_t5m ||
			chip_type_id == chip_t3x ||
			chip_type_id == chip_txhd2 ||
			chip_type_id == chip_a4 ||
			chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x ||
			chip_type_id == chip_c5 ||
			chip_type_id == chip_a9) {
			p_gm = get_gm_data();
			lcd_gamma_api(gamma_index,
				p_gm->dbg_gm_tbl.gamma_r,
				p_gm->dbg_gm_tbl.gamma_g,
				p_gm->dbg_gm_tbl.gamma_b,
				WR_VCB, RD_MOD, 0);
		} else {
			lcd_gamma_api(gamma_index,
				gamma_data_r,
				gamma_data_g,
				gamma_data_b,
				WR_VCB, RD_MOD, 0);
		}
		return;
	}

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;

	pr_info("read gamma begin\n");
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}

	cnt = 0;
	for (i = 0; i < 256; i++) {
		cnt = 0;
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY) {
				pr_info("%s ADR_RDY timeout\n", __func__);
				break;
			}
		}
		WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_RD) |
						(0x0 << H_AUTO_INC) |
						(0x1 << rgb_mask)	|
						(i << HADR));

		cnt = 0;
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << RD_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY) {
				pr_info("%s ADR_RDY timeout\n", __func__);
				break;
			}
		}
		if (rgb_mask == H_SEL_R)
			gamma_data_r[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
		else if (rgb_mask == H_SEL_G)
			gamma_data_g[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
		else if (rgb_mask == H_SEL_B)
			gamma_data_b[i] = READ_VPP_REG(L_GAMMA_DATA_PORT);
	}

	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));
	pr_info("read gamma over\n");
}

void vpp_get_lcd_gamma_table_sub(void)
{
	if (is_meson_t7_cpu() ||
		chip_type_id == chip_a9)
		lcd_gamma_api(gamma_index_sub,
			gamma_data_r,
			gamma_data_g,
			gamma_data_b,
			WR_VCB, RD_MOD, 0);
}

void amve_write_gamma_table(u16 *data, u32 rgb_mask)
{
	int i;
	int cnt = 0;
	unsigned long flags = 0;
	struct gamma_data_s *p_gm;
	int max_idx;

	if (cpu_after_eq_t7()) {
		if (chip_type_id == chip_t5m ||
			chip_type_id == chip_t3x ||
			chip_type_id == chip_txhd2 ||
			chip_type_id == chip_a4 ||
			chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x ||
			chip_type_id == chip_c5 ||
			chip_type_id == chip_a9) {
			p_gm = get_gm_data();
			max_idx = p_gm->max_idx;
			lcd_gamma_api(gamma_index, p_gm->dbg_gm_tbl.gamma_r,
				p_gm->dbg_gm_tbl.gamma_g, p_gm->dbg_gm_tbl.gamma_b,
				WR_VCB, RD_MOD, 0);
			if (rgb_mask == H_SEL_R)
				memcpy(p_gm->dbg_gm_tbl.gamma_r, data, sizeof(u16) * max_idx);
			else if (rgb_mask == H_SEL_G)
				memcpy(p_gm->dbg_gm_tbl.gamma_g, data, sizeof(u16) * max_idx);
			else if (rgb_mask == H_SEL_B)
				memcpy(p_gm->dbg_gm_tbl.gamma_b, data, sizeof(u16) * max_idx);
			lcd_gamma_api(gamma_index, p_gm->dbg_gm_tbl.gamma_r,
				p_gm->dbg_gm_tbl.gamma_g, p_gm->dbg_gm_tbl.gamma_b,
				WR_VCB, WR_MOD, 0);
		} else {
			lcd_gamma_api(gamma_index, gamma_data_r,
				gamma_data_g, gamma_data_b, WR_VCB, RD_MOD, 0);
			if (rgb_mask == H_SEL_R)
				memcpy(gamma_data_r, data, sizeof(u16) * 256);
			else if (rgb_mask == H_SEL_G)
				memcpy(gamma_data_g, data, sizeof(u16) * 256);
			else if (rgb_mask == H_SEL_B)
				memcpy(gamma_data_b, data, sizeof(u16) * 256);
			lcd_gamma_api(gamma_index, gamma_data_r,
				gamma_data_g, gamma_data_b, WR_VCB, WR_MOD, 0);
		}
		return;
	}

	if (!(READ_VPP_REG(ENCL_VIDEO_EN) & 0x1))
		return;

	spin_lock_irqsave(&vpp_lcd_gamma_lock, flags);

	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	cnt = 0;
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x0 << HADR));
	for (i = 0; i < 256; i++) {
		while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << WR_RDY))) {
			udelay(10);
			if (cnt++ > GAMMA_RETRY)
				break;
		}
		cnt = 0;
		WRITE_VPP_REG(L_GAMMA_DATA_PORT, data[i]);
	}
	while (!(READ_VPP_REG(L_GAMMA_CNTL_PORT) & (0x1 << ADR_RDY))) {
		udelay(10);
		if (cnt++ > GAMMA_RETRY)
			break;
	}
	WRITE_VPP_REG(L_GAMMA_ADDR_PORT, (0x1 << H_AUTO_INC) |
				    (0x1 << rgb_mask)   |
				    (0x23 << HADR));

	spin_unlock_irqrestore(&vpp_lcd_gamma_lock, flags);
}

void amve_write_gamma_table_sub(u16 *data, u32 rgb_mask)
{
	if (is_meson_t7_cpu() ||
		chip_type_id == chip_a9) {
		lcd_gamma_api(gamma_index_sub,
			gamma_data_r, gamma_data_g, gamma_data_b, WR_VCB, RD_MOD, 0);

		if (rgb_mask == H_SEL_R)
			memcpy(gamma_data_r, data, sizeof(u16) * 256);
		else if (rgb_mask == H_SEL_G)
			memcpy(gamma_data_g, data, sizeof(u16) * 256);
		else if (rgb_mask == H_SEL_B)
			memcpy(gamma_data_b, data, sizeof(u16) * 256);

		lcd_gamma_api(gamma_index_sub,
			gamma_data_r, gamma_data_g, gamma_data_b, WR_VCB, WR_MOD, 0);
	}
}

#endif

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

static int bypass_coeff[MATRIX_5x3_COEF_SIZE] = {
	0, 0, 0, /* pre offset */
	COEFF_NORM(1.0),	COEFF_NORM(0.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(1.0),	COEFF_NORM(0.0),
	COEFF_NORM(0.0),	COEFF_NORM(0.0),	COEFF_NORM(1.0),
	0, 0, 0, /* 10'/11'/12' */
	0, 0, 0, /* 20'/21'/22' */
	0, 0, 0, /* offset */
	0, 0, 0 /* mode, right_shift, clip_en */
};

#define MTX_RS 0
#define MTX_ENABLE 1
#define MATRIX_3x3_COEF_SIZE 17
static int matrix_yuv_bypass_coef[MATRIX_3x3_COEF_SIZE] = {
	-64, -512, -512,
	COEFF_NORM(1.0), COEFF_NORM(0), COEFF_NORM(0),
	COEFF_NORM(0), COEFF_NORM(1.0), COEFF_NORM(0),
	COEFF_NORM(0), COEFF_NORM(0), COEFF_NORM(1.0),
	64, 512, 512,
	MTX_RS,
	MTX_ENABLE
};

void vpp_set_rgb_ogo(struct tcon_rgb_ogo_s *p, int vpp_index)
{
	int m[24];
	int i;
	unsigned int gainoff_ctl0;
	unsigned int gainoff_ctl1;
	unsigned int gainoff_ctl2;
	unsigned int gainoff_ctl3;
	unsigned int gainoff_ctl4;

	/* write to registers */
	if (video_rgb_ogo_xvy_mtx) {
		if (video_rgb_ogo_xvy_mtx_latch & MTX_BYPASS_RGB_OGO) {
			memcpy(m, bypass_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_BYPASS_RGB_OGO;
		} else if (video_rgb_ogo_xvy_mtx_latch & MTX_RGB2YUVL_RGB_OGO) {
			memcpy(m, RGB709_to_YUV709l_coeff, sizeof(int) * 24);
			video_rgb_ogo_xvy_mtx_latch &= ~MTX_RGB2YUVL_RGB_OGO;
		} else {
			memcpy(m, bypass_coeff, sizeof(int) * 24);
		}

		m[3] = p->r_gain * m[3] / COEFF_NORM(1.0);
		m[4] = p->g_gain * m[4] / COEFF_NORM(1.0);
		m[5] = p->b_gain * m[5] / COEFF_NORM(1.0);
		m[6] = p->r_gain * m[6] / COEFF_NORM(1.0);
		m[7] = p->g_gain * m[7] / COEFF_NORM(1.0);
		m[8] = p->b_gain * m[8] / COEFF_NORM(1.0);
		m[9] = p->r_gain * m[9] / COEFF_NORM(1.0);
		m[10] = p->g_gain * m[10] / COEFF_NORM(1.0);
		m[11] = p->b_gain * m[11] / COEFF_NORM(1.0);

		if (vinfo_lcd_support()) {
			m[18] = (p->r_pre_offset + m[18] + 1024)
				* p->r_gain / COEFF_NORM(1.0)
				- p->r_gain + p->r_post_offset;
			m[19] = (p->g_pre_offset + m[19] + 1024)
				* p->g_gain / COEFF_NORM(1.0)
				- p->g_gain + p->g_post_offset;
			m[20] = (p->b_pre_offset + m[20] + 1024)
				* p->b_gain / COEFF_NORM(1.0)
				- p->b_gain + p->b_post_offset;
		} else {
			m[0] = p->r_gain * p->r_pre_offset / COEFF_NORM(1.0) +
				p->r_post_offset;
			m[1] = p->g_gain * p->g_pre_offset / COEFF_NORM(1.0) +
				p->g_post_offset;
			m[2] = p->b_gain * p->b_pre_offset / COEFF_NORM(1.0) +
				p->b_post_offset;
		}

		for (i = 18; i < 21; i++) {
			if (m[i] > 1023)
				m[i] = 1023;
			if (m[i] < -1024)
				m[i] = -1024;
		}

		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			VSYNC_WRITE_VPP_REG_BITS(VPP_POST_MATRIX_EN_CTRL,
					   p->en, 0, 1);
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_PRE_OFFSET0_1,
				      ((m[0] & 0xfff) << 16)
				      | (m[1] & 0xfff));
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_PRE_OFFSET2,
				      m[2] & 0xfff);
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF00_01,
				      ((m[3] & 0x1fff) << 16)
				      | (m[4] & 0x1fff));
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF02_10,
				      ((m[5]	& 0x1fff) << 16)
				      | (m[6] & 0x1fff));
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF11_12,
				      ((m[7] & 0x1fff) << 16)
				      | (m[8] & 0x1fff));
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF20_21,
				      ((m[9] & 0x1fff) << 16)
				      | (m[10] & 0x1fff));
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF22,
				      m[11] & 0x1fff);
			if (m[21]) {
				VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF13_14,
					      ((m[12] & 0x1fff) << 16)
					      | (m[13] & 0x1fff));
				VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF15_25,
					      ((m[14] & 0x1fff) << 16)
					      | (m[17] & 0x1fff));
				VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_COEF23_24,
					      ((m[15] & 0x1fff) << 16)
					      | (m[16] & 0x1fff));
			}
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_OFFSET0_1,
				      ((m[18] & 0xfff) << 16)
				      | (m[19] & 0xfff));
			VSYNC_WRITE_VPP_REG(VPP_POST_MATRIX_OFFSET2,
				      m[20] & 0xfff);
			VSYNC_WRITE_VPP_REG_BITS(VPP_POST_MATRIX_CLIP,
					   m[21], 3, 2);
			VSYNC_WRITE_VPP_REG_BITS(VPP_POST_MATRIX_CLIP,
					   m[22], 5, 3);
			return;
		}

		VSYNC_WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, p->en, 6, 1);
		VSYNC_WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 3, 8, 2);

		VSYNC_WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1,
			      ((m[0] & 0xfff) << 16)
			      | (m[1] & 0xfff));
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2,
			      m[2] & 0xfff);
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF00_01,
			      ((m[3] & 0x1fff) << 16)
			      | (m[4] & 0x1fff));
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF02_10,
			      ((m[5]	& 0x1fff) << 16)
			      | (m[6] & 0x1fff));
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF11_12,
			      ((m[7] & 0x1fff) << 16)
			      | (m[8] & 0x1fff));
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF20_21,
			      ((m[9] & 0x1fff) << 16)
			      | (m[10] & 0x1fff));
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF22,
			      m[11] & 0x1fff);
		if (m[21]) {
			VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF13_14,
				      ((m[12] & 0x1fff) << 16)
				      | (m[13] & 0x1fff));
			VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF15_25,
				      ((m[14] & 0x1fff) << 16)
				      | (m[17] & 0x1fff));
			VSYNC_WRITE_VPP_REG(VPP_MATRIX_COEF23_24,
				      ((m[15] & 0x1fff) << 16)
				      | (m[16] & 0x1fff));
		}
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1,
			      ((m[18] & 0xfff) << 16)
			      | (m[19] & 0xfff));
		VSYNC_WRITE_VPP_REG(VPP_MATRIX_OFFSET2,
			      m[20] & 0xfff);
		VSYNC_WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
				   m[21], 3, 2);
		VSYNC_WRITE_VPP_REG_BITS(VPP_MATRIX_CLIP,
				   m[22], 5, 3);
	} else {
		/*for txlx and txhd, pre_offset and post_offset become 13 bit*/
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_s5 ||
			chip_type_id == chip_t3x) {
			post_gainoff_set(p, WR_DMA, vpp_index);
		} else if (is_meson_txlx_cpu() || is_meson_txhd_cpu() ||
		    is_meson_tm2_cpu() || is_meson_t7_cpu()) {
			VSYNC_WRITE_VPP_REG(VPP_GAINOFF_CTRL0,
				      ((p->en << 31) & 0x80000000) |
				      ((p->r_gain << 16) & 0x07ff0000) |
				      ((p->g_gain <<  0) & 0x000007ff));
			VSYNC_WRITE_VPP_REG(VPP_GAINOFF_CTRL1,
				      ((p->b_gain << 16) & 0x07ff0000) |
				      ((p->r_post_offset <<  0) & 0x00001fff));
			VSYNC_WRITE_VPP_REG(VPP_GAINOFF_CTRL2,
				      ((p->g_post_offset << 16) & 0x1fff0000) |
				      ((p->b_post_offset <<  0) & 0x00001fff));
			VSYNC_WRITE_VPP_REG(VPP_GAINOFF_CTRL3,
				      ((p->r_pre_offset  << 16) & 0x1fff0000) |
				      ((p->g_pre_offset  <<  0) & 0x00001fff));
			VSYNC_WRITE_VPP_REG(VPP_GAINOFF_CTRL4,
				      ((p->b_pre_offset  <<  0) & 0x00001fff));
		} else
#endif
		{
		/*txl and before txl, and tl1 10bit path offset is 11bit*/
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			if (chip_type_id == chip_a4 ||
				chip_type_id == chip_c5) {
				gainoff_ctl0 = VOUT_GAINOFF_CTRL0;
				gainoff_ctl1 = VOUT_GAINOFF_CTRL1;
				gainoff_ctl2 = VOUT_GAINOFF_CTRL2;
				gainoff_ctl3 = VOUT_GAINOFF_CTRL3;
				gainoff_ctl4 = VOUT_GAINOFF_CTRL4;
			} else {
#endif
				gainoff_ctl0 = VPP_GAINOFF_CTRL0;
				gainoff_ctl1 = VPP_GAINOFF_CTRL1;
				gainoff_ctl2 = VPP_GAINOFF_CTRL2;
				gainoff_ctl3 = VPP_GAINOFF_CTRL3;
				gainoff_ctl4 = VPP_GAINOFF_CTRL4;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			}
#endif

			if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12B)
				return;

			VSYNC_WRITE_VPP_REG(gainoff_ctl0,
				      ((p->en << 31) & 0x80000000) |
				      ((p->r_gain << 16) & 0x07ff0000) |
				      ((p->g_gain <<  0) & 0x000007ff));
			VSYNC_WRITE_VPP_REG(gainoff_ctl1,
				      ((p->b_gain << 16) & 0x07ff0000) |
				      ((p->r_post_offset <<  0) & 0x000007ff));
			VSYNC_WRITE_VPP_REG(gainoff_ctl2,
				      ((p->g_post_offset << 16) & 0x07ff0000) |
				      ((p->b_post_offset <<  0) & 0x000007ff));
			VSYNC_WRITE_VPP_REG(gainoff_ctl3,
				      ((p->r_pre_offset  << 16) & 0x07ff0000) |
				      ((p->g_pre_offset  <<  0) & 0x000007ff));
			VSYNC_WRITE_VPP_REG(gainoff_ctl4,
				      ((p->b_pre_offset  <<  0) & 0x000007ff));
		}
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void vpp_set_rgb_ogo_sub(struct tcon_rgb_ogo_s *p)
{
	/*for t7 vpp1 go*/
	if (is_meson_t7_cpu()) {
		VSYNC_WRITE_VPP_REG(VPP1_GAINOFF_CTRL0,
			((p->en << 31) & 0x80000000) |
			((p->r_gain << 16) & 0x07ff0000) |
			((p->g_gain << 0) & 0x000007ff));
		VSYNC_WRITE_VPP_REG(VPP1_GAINOFF_CTRL1,
			((p->b_gain << 16) & 0x07ff0000) |
			((p->r_post_offset << 0) & 0x00001fff));
		VSYNC_WRITE_VPP_REG(VPP1_GAINOFF_CTRL2,
			((p->g_post_offset << 16) & 0x1fff0000) |
			((p->b_post_offset << 0) & 0x00001fff));
		VSYNC_WRITE_VPP_REG(VPP1_GAINOFF_CTRL3,
			((p->r_pre_offset << 16) & 0x1fff0000) |
			((p->g_pre_offset << 0) & 0x00001fff));
		VSYNC_WRITE_VPP_REG(VPP1_GAINOFF_CTRL4,
			((p->b_pre_offset << 0) & 0x00001fff));
	}
}

void ve_enable_dnlp(void)
{
	unsigned int reg_ctrl = SRSHARP1_DNLP_EN;

	ve_en = 1;

	if (chip_type_id != chip_t3x) {
		if (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
			if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
				reg_ctrl = SRSHARP1_DNLP_EN;
			} else if (chip_type_id == chip_s7d ||
				chip_type_id == chip_s6 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9) {
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

			if (chip_type_id != chip_s7d &&
				chip_type_id != chip_s6 &&
				chip_type_id != chip_t6d &&
				chip_type_id != chip_t6w &&
				chip_type_id != chip_t6x &&
				chip_type_id != chip_a9)
				WRITE_VPP_REG_BITS(reg_ctrl, 1, 0, 1);
			else
				WRITE_VPP_REG_BITS(reg_ctrl, 1, 4, 1);
		} else {
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				1, DNLP_EN_BIT, DNLP_EN_WID);
		}
	} else {
		ve_dnlp_ctl(1);
	}

	pr_amve_dbg("\n[amve..] ve_enable_dnlp done.\n");
}

void ve_disable_dnlp(void)
{
	unsigned int reg_ctrl = SRSHARP1_DNLP_EN;

	ve_en = 0;

	if (chip_type_id != chip_t3x) {
		if (dnlp_sel == NEW_DNLP_IN_SHARPNESS) {
			if (is_meson_gxlx_cpu() || is_meson_txlx_cpu()) {
				reg_ctrl = SRSHARP1_DNLP_EN;
			} else if (chip_type_id == chip_s7d ||
			chip_type_id == chip_s6 ||
			chip_type_id == chip_t6d ||
			chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x ||
			chip_type_id == chip_a9) {
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

			if (chip_type_id != chip_s7d &&
				chip_type_id != chip_s6 &&
				chip_type_id != chip_t6d &&
				chip_type_id != chip_t6w &&
				chip_type_id != chip_t6x &&
				chip_type_id != chip_a9)
				WRITE_VPP_REG_BITS(reg_ctrl, 0, 0, 1);
			else
				WRITE_VPP_REG_BITS(reg_ctrl, 0, 4, 1);
		} else {
			WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
				0, DNLP_EN_BIT, DNLP_EN_WID);
		}
	} else {
		ve_dnlp_ctl(0);
	}

	pr_amve_dbg("\n[amve..] ve_disable_dnlp done.\n");
}

void ve_dnlp_ctrl_vsync(int enable)
{
	dnlp_en_2 = enable;
}

void ve_set_dnlp_2(void)
{
	/* clear historic luma sum */
	if (dnlp_insmod_ok == 0)
		return;
	ve_dnlp_tgt_init(0);
	/* calculate dnlp reg data */
	ve_dnlp_calculate_reg();
	/* load dnlp reg data */
	/*ve_dnlp_load_reg();*/
	ve_dnlp_load_def_reg();
}
#endif

unsigned int ve_get_vs_cnt(void)
{
	return READ_VPP_REG(VPP_VDO_MEAS_VS_COUNT_LO);
}

void vpp_phase_lock_on_vs(unsigned int cycle,
			  unsigned int stamp,
			  bool         lock50,
			  unsigned int range_fast,
			  unsigned int range_slow)
{
	unsigned int vtotal_ori = READ_VPP_REG(ENCL_VIDEO_MAX_LNCNT);
	unsigned int vtotal     = lock50 ? 1349 : 1124;
	unsigned int stamp_in   = READ_VPP_REG(VDIN_MEAS_VS_COUNT_LO);
	unsigned int stamp_out  = ve_get_vs_cnt();
	unsigned int phase      = 0;
	unsigned int cnt = assist_cnt;/* READ_VPP_REG(ASSIST_SPARE8_REG1); */
	int step = 0, i = 0;
	/* get phase */
	if (stamp_out < stamp)
		phase = 0xffffffff - stamp + stamp_out + 1;
	else
		phase = stamp_out - stamp;
	while (phase >= cycle)
		phase -= cycle;
	/* 225~315 degree => tune fast panel output */
	if ((phase > ((cycle * 5) >> 3)) && (phase < ((cycle * 7) >> 3))) {
		vtotal -= range_slow;
		step = 1;
	} else if ((phase > (cycle >> 3)) && (phase < ((cycle * 3) >> 3))) {
		/* 45~135 degree => tune slow panel output */
		vtotal += range_slow;
		step = -1;
	} else if (phase >= ((cycle * 7) >> 3)) {
		/* 315~360 degree => tune fast panel output */
		vtotal -= range_fast;
		step = + 2;
	} else if (phase <= (cycle >> 3)) {
		/* 0~45 degree => tune slow panel output */
		vtotal += range_fast;
		step = -2;
	} else {/* 135~225 degree => keep still */
		vtotal = vtotal_ori;
		step = 0;
	}
	if (vtotal != vtotal_ori)
		WRITE_VPP_REG(ENCL_VIDEO_MAX_LNCNT, vtotal);
	if (cnt) {
		cnt--;
		/* WRITE_VPP_REG(ASSIST_SPARE8_REG1, cnt); */
		assist_cnt = cnt;
		if (cnt) {
			vpp_log[cnt][0] = stamp;
			vpp_log[cnt][1] = stamp_in;
			vpp_log[cnt][2] = stamp_out;
			vpp_log[cnt][3] = cycle;
			vpp_log[cnt][4] = phase;
			vpp_log[cnt][5] = vtotal;
			vpp_log[cnt][6] = step;
		} else {
			for (i = 127; i > 0; i--) {
				pr_amve_dbg("Ti=%10u Tio=%10u To=%10u CY=%6u ",
					    vpp_log[i][0],
					    vpp_log[i][1],
					    vpp_log[i][2],
					    vpp_log[i][3]);
				pr_amve_dbg("PH =%10u Vt=%4u S=%2d\n",
					    vpp_log[i][4],
					    vpp_log[i][5],
					    vpp_log[i][6]);
			}
		}
	}
}

void ve_frame_size_patch(unsigned int width, unsigned int height)
{
	unsigned int vpp_size = height | (width << 16);

	if (READ_VPP_REG(VPP_VE_H_V_SIZE) != vpp_size)
		WRITE_VPP_REG(VPP_VE_H_V_SIZE, vpp_size);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void ve_dnlp_latch_process(void)
{
	if (vecm_latch_flag & FLAG_VE_NEW_DNLP) {
		vecm_latch_flag &= ~FLAG_VE_NEW_DNLP;
		ve_set_v3_dnlp(&dnlp_curve_param_load);
	}

	if (vecm_latch_flag2 & BLE_WHE_UPDATE) {
		vecm_latch_flag2 &= ~BLE_WHE_UPDATE;
		ble_whe_param_update(&ble_whe_param_load);
	}

	if (dnlp_en && dnlp_status) {
		dnlp_status = 0;
		if (chip_type_id != chip_t6d &&
			chip_type_id != chip_t6w &&
			chip_type_id != chip_t6x)
			ve_set_dnlp_2();
		ve_enable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_enable_dnlp OK!!!\n");
	} else if (dnlp_en == 0 && !dnlp_status) {
		dnlp_status = 1;
		ve_disable_dnlp();
		pr_amve_dbg("\n[amve..] set vpp_disable_dnlp OK!!!\n");
	}
}

void ve_lc_latch_process(void)
{
	if (vecm_latch_flag & FLAG_VE_LC_CURV) {
		vecm_latch_flag &= ~FLAG_VE_LC_CURV;
		lc_load_curve(&lc_curve_parm_load);
	}
}

void ve_lcd_gamma_process(int vpp_index)
{
	int viu_sel;
	struct tcon_gamma_table_s *ptable;
	struct gamma_data_s *p_gm;
	unsigned int tmp = 0;

	if (cpu_after_eq_t7()) {
		viu_sel = vpp_get_vout_viu_mux();
		viu_sel = viu_sel - 1;
		if (viu_sel < 0)
			viu_sel = 0;

		if (chip_type_id == chip_txhd2)
			viu_sel = 0;

		gamma_index = viu_sel;

		if (is_meson_t7_cpu()) {
			if (gamma_index == 0)
				gamma_index_sub = 1;
			else
				gamma_index_sub = 0;
		}
	} else {
		viu_sel = vpp_get_encl_viu_mux();

		if (gamma_loadprotect_en && gamma_disable_flag) {
			if (gamma_update_flag_r) {
				vpp_set_lcd_gamma_table(video_gamma_table_r.data,
					H_SEL_R, viu_sel);
				gamma_update_flag_r = 0;
				pr_amve_dbg("\n[amve..] update R_lcd_gamma_table OK!!!\n");
			}

			if (gamma_update_flag_g) {
				vpp_set_lcd_gamma_table(video_gamma_table_g.data,
					H_SEL_G, viu_sel);
				gamma_update_flag_g = 0;
				pr_amve_dbg("\n[amve..] update G_lcd_gamma_table OK!!!\n");
			}

			if (gamma_update_flag_b) {
				vpp_set_lcd_gamma_table(video_gamma_table_b.data,
					H_SEL_B, viu_sel);
				gamma_update_flag_b = 0;
				pr_amve_dbg("\n[amve..] update B_lcd_gamma_table OK!!!\n");
			}

			if (!gamma_update_flag_r &&
				!gamma_update_flag_g &&
				!gamma_update_flag_b) {
				tmp = READ_VPP_REG(L_GAMMA_CNTL_PORT);
				tmp = (tmp & 0xfffffffe) | gamma_en;

				pr_amve_dbg("[amve..] enable_val/gamma_en = %d/%d\n",
					tmp, gamma_en);

				if (viu_sel == 1) { /* viu1 vsync rdma */
					VSYNC_WRITE_VPP_REG_VPP_SEL(L_GAMMA_CNTL_PORT,
						tmp, vpp_index);
				} else { /* viu2 directly write, rdma todo */
					WRITE_VPP_REG(L_GAMMA_CNTL_PORT, tmp);
				}

				gamma_disable_flag = 0;
			}
		}
	}

	if (vecm_latch_flag & FLAG_GAMMA_TABLE_EN) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_EN;
		vpp_enable_lcd_gamma_table(viu_sel, 1, vpp_index);
		pr_amve_dbg("\n[amve..] set vpp_enable_lcd_gamma_table OK!!!\n");
	}

	if (vecm_latch_flag & FLAG_GAMMA_TABLE_DIS) {
		vecm_latch_flag &= ~FLAG_GAMMA_TABLE_DIS;
		vpp_disable_lcd_gamma_table(viu_sel, 1, vpp_index);
		pr_amve_dbg("\n[amve..] set vpp_disable_lcd_gamma_table OK!!!\n");
	}

	if (cpu_after_eq_t7()) {
		if ((vecm_latch_flag & FLAG_GAMMA_TABLE_R) ||
			(vecm_latch_flag & FLAG_GAMMA_TABLE_G) ||
			(vecm_latch_flag & FLAG_GAMMA_TABLE_B)) {
			if (vecm_latch_flag & FLAG_GAMMA_TABLE_R)
				vecm_latch_flag &= ~FLAG_GAMMA_TABLE_R;
			if (vecm_latch_flag & FLAG_GAMMA_TABLE_G)
				vecm_latch_flag &= ~FLAG_GAMMA_TABLE_G;
			if (vecm_latch_flag & FLAG_GAMMA_TABLE_B)
				vecm_latch_flag &= ~FLAG_GAMMA_TABLE_B;
			if (chip_type_id == chip_t5m ||
				chip_type_id == chip_t3x ||
				chip_type_id == chip_txhd2 ||
				chip_type_id == chip_a4 ||
				chip_type_id == chip_t6d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_c5 ||
				chip_type_id == chip_a9) {
				p_gm = get_gm_data();
				memcpy(p_gm->gm_tbl.gamma_r,
					video_gamma_table_r.data,
					sizeof(u16) * p_gm->max_idx);
				memcpy(p_gm->gm_tbl.gamma_g,
					video_gamma_table_g.data,
					sizeof(u16) * p_gm->max_idx);
				memcpy(p_gm->gm_tbl.gamma_b,
					video_gamma_table_b.data,
					sizeof(u16) * p_gm->max_idx);
				lcd_gamma_api(gamma_index, p_gm->gm_tbl.gamma_r,
						p_gm->gm_tbl.gamma_g,
						p_gm->gm_tbl.gamma_b,
						WR_DMA, WR_MOD, vpp_index);
			} else {
				lcd_gamma_api(gamma_index, video_gamma_table_r.data,
						video_gamma_table_g.data,
						video_gamma_table_b.data,
						WR_DMA, WR_MOD, vpp_index);
			}
			pr_amve_dbg("\n[amve..] set vpp_set_lcd_gamma_table OK!!!\n");
		}
	} else {
		if (vecm_latch_flag & FLAG_GAMMA_TABLE_R) {
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_R;
			if (!gamma_loadprotect_en) {
				vpp_set_lcd_gamma_table(video_gamma_table_r.data,
					H_SEL_R, viu_sel);
				pr_amve_dbg("\n[amve..] set R vpp_set_lcd_gamma_table OK!!!\n");
			} else {
				gamma_update_flag_r = 1;
				if (!gamma_disable_flag) {
					vpp_disable_lcd_gamma_table(viu_sel, 0, vpp_index);
					gamma_disable_flag = 1;
				}
			}
		}

		if (vecm_latch_flag & FLAG_GAMMA_TABLE_G) {
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_G;
			if (!gamma_loadprotect_en) {
				vpp_set_lcd_gamma_table(video_gamma_table_g.data,
					H_SEL_G, viu_sel);
				pr_amve_dbg("\n[amve..] set G vpp_set_lcd_gamma_table OK!!!\n");
			} else {
				gamma_update_flag_g = 1;
				if (!gamma_disable_flag) {
					vpp_disable_lcd_gamma_table(viu_sel, 0, vpp_index);
					gamma_disable_flag = 1;
				}
			}
		}

		if (vecm_latch_flag & FLAG_GAMMA_TABLE_B) {
			vecm_latch_flag &= ~FLAG_GAMMA_TABLE_B;
			if (!gamma_loadprotect_en) {
				vpp_set_lcd_gamma_table(video_gamma_table_b.data,
					H_SEL_B, viu_sel);
				pr_amve_dbg("\n[amve..] set B vpp_set_lcd_gamma_table OK!!!\n");
			} else {
				gamma_update_flag_b = 1;
				if (!gamma_disable_flag) {
					vpp_disable_lcd_gamma_table(viu_sel, 0, vpp_index);
					gamma_disable_flag = 1;
				}
			}
		}
	}

	if (vecm_latch_flag & FLAG_RGB_OGO) {
		vecm_latch_flag &= ~FLAG_RGB_OGO;
		if (video_rgb_ogo_mode_sw) {
			if (video_rgb_ogo.en) {
				ptable = &video_gamma_table_r_adj;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_R, viu_sel);
				ptable = &video_gamma_table_g_adj;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_G, viu_sel);
				ptable = &video_gamma_table_b_adj;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_B, viu_sel);
			} else {
				ptable = &video_gamma_table_r;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_R, viu_sel);
				ptable = &video_gamma_table_g;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_G, viu_sel);
				ptable = &video_gamma_table_b;
				vpp_set_lcd_gamma_table(ptable->data,
							H_SEL_B, viu_sel);
			}
			pr_amve_dbg("\n[amve] vpp_set_lcd_gamma_table OK\n");
		} else {
			vpp_set_rgb_ogo(&video_rgb_ogo, vpp_index);
			if (is_meson_t7_cpu()) {
				vpp_set_rgb_ogo_sub(&video_rgb_ogo_sub);
				pr_amve_dbg("\n[amve] set_rgb_ogo_sub OK!!!\n");
			}
			pr_amve_dbg("\n[amve] vpp_set_rgb_ogo OK!!!\n");
		}
	}

	/*for t7 vpp1 lcd gamma*/
	if (is_meson_t7_cpu() ||
		chip_type_id == chip_a9) {
		if (vecm_latch_flag2 & FLAG_GAMMA_TABLE_EN_SUB) {
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_EN_SUB;
			vpp_enable_lcd_gamma_table(gamma_index_sub, 1, vpp_index);
			pr_amve_dbg("\n[amve..] set enable_lcd_gamma_sub OK!!!\n");
		}

		if (vecm_latch_flag2 & FLAG_GAMMA_TABLE_DIS_SUB) {
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_DIS_SUB;
			vpp_disable_lcd_gamma_table(gamma_index_sub, 1, vpp_index);
			pr_amve_dbg("\n[amve..] set disable_lcd_gamma_sub OK!!!\n");
		}

		if ((vecm_latch_flag2 & FLAG_GAMMA_TABLE_R_SUB) &&
			(vecm_latch_flag2 & FLAG_GAMMA_TABLE_G_SUB) &&
			(vecm_latch_flag2 & FLAG_GAMMA_TABLE_B_SUB)) {
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_R_SUB;
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_G_SUB;
			vecm_latch_flag2 &= ~FLAG_GAMMA_TABLE_B_SUB;
			lcd_gamma_api(gamma_index_sub,
				video_gamma_table_r_sub.data,
				video_gamma_table_g_sub.data,
				video_gamma_table_b_sub.data,
				WR_DMA, WR_MOD, vpp_index);
			pr_amve_dbg("\n[amve] set_lcd_gamma_table_sub OK!!!\n");
		}
	}
}
#endif

void lvds_freq_process(void)
{
/* #if ((MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TV)|| */
/* (MESON_CPU_TYPE==MESON_CPU_TYPE_MESON6TVD)) */
/*  lvds freq 50Hz/60Hz */
/* if (frame_lock_freq == 1){//50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* } */
/* else if (frame_lock_freq == 2){//60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if(READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* else if (frame_lock_freq == 0){ */
/*  lvds freq 50Hz/60Hz */
/* if (vecm_latch_flag & FLAG_LVDS_FREQ_SW){  //50 hz */
/* // panel freq is 60Hz => change back to 50Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) < 1237) */
/* (1124 + 1349 +1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1349); */
/* }else{	 //60 hz */
/* // panel freq is 50Hz => change back to 60Hz */
/* if (READ_VPP_REG(ENCP_VIDEO_MAX_LNCNT) >= 1237) */
/* (1124 + 1349 + 1) / 2 */
/* WRITE_VPP_REG(ENCP_VIDEO_MAX_LNCNT, 1124); */
/* } */
/* } */
/* #endif */
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void ve_dnlp_param_update(void)
{
	vecm_latch_flag |= FLAG_VE_DNLP;
}

void ve_new_dnlp_param_update(void)
{
	vecm_latch_flag |= FLAG_VE_NEW_DNLP;
}

void ve_lc_curve_update(void)
{
	vecm_latch_flag |= FLAG_VE_LC_CURV;
}

static void video_data_limitation(int *val)
{
	if (*val > 1023)
		*val = 1023;
	if (*val < 0)
		*val = 0;
}

static void video_lookup(struct tcon_gamma_table_s *tbl, int *val)
{
	unsigned int idx = (*val) >> 2, mod = (*val) & 3;

	if (idx < 255)
		*val = tbl->data[idx] +
		(((tbl->data[idx + 1] - tbl->data[idx]) * mod + 2) >> 2);
	else
		*val = tbl->data[idx] +
		(((1023 - tbl->data[idx]) * mod + 2) >> 2);
}

static void video_set_rgb_ogo(void)
{
	int i = 0, r = 0, g = 0, b = 0;

	for (i = 0; i < 256; i++) {
		r = video_curve_2d2.data[i];
		g = video_curve_2d2.data[i];
		b = video_curve_2d2.data[i];
		/* Pre_offset */
		r += video_rgb_ogo.r_pre_offset;
		g += video_rgb_ogo.g_pre_offset;
		b += video_rgb_ogo.b_pre_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Gain */
		r  *= video_rgb_ogo.r_gain;
		r >>= 10;
		g  *= video_rgb_ogo.g_gain;
		g >>= 10;
		b  *= video_rgb_ogo.b_gain;
		b >>= 10;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		/* Post_offset */
		r += video_rgb_ogo.r_post_offset;
		g += video_rgb_ogo.g_post_offset;
		b += video_rgb_ogo.b_post_offset;
		video_data_limitation(&r);
		video_data_limitation(&g);
		video_data_limitation(&b);
		video_lookup(&video_curve_2d2_inv, &r);
		video_lookup(&video_curve_2d2_inv, &g);
		video_lookup(&video_curve_2d2_inv, &b);
		/* Get gamma_ogo = curve_2d2_inv_ogo * gamma */
		video_lookup(&video_gamma_table_r, &r);
		video_lookup(&video_gamma_table_g, &g);
		video_lookup(&video_gamma_table_b, &b);
		/* Save gamma_ogo */
		video_gamma_table_r_adj.data[i] = r;
		video_gamma_table_g_adj.data[i] = g;
		video_gamma_table_b_adj.data[i] = b;
	}
}


void ve_ogo_param_update_sub(void)
{
	if (video_rgb_ogo_sub.en > 1)
		video_rgb_ogo_sub.en = 1;
	if (video_rgb_ogo_sub.r_pre_offset > 1023)
		video_rgb_ogo_sub.r_pre_offset = 1023;
	if (video_rgb_ogo_sub.r_pre_offset < -1024)
		video_rgb_ogo_sub.r_pre_offset = -1024;
	if (video_rgb_ogo_sub.g_pre_offset > 1023)
		video_rgb_ogo_sub.g_pre_offset = 1023;
	if (video_rgb_ogo_sub.g_pre_offset < -1024)
		video_rgb_ogo_sub.g_pre_offset = -1024;
	if (video_rgb_ogo_sub.b_pre_offset > 1023)
		video_rgb_ogo_sub.b_pre_offset = 1023;
	if (video_rgb_ogo_sub.b_pre_offset < -1024)
		video_rgb_ogo_sub.b_pre_offset = -1024;
	if (video_rgb_ogo_sub.r_gain > 2047)
		video_rgb_ogo_sub.r_gain = 2047;
	if (video_rgb_ogo_sub.g_gain > 2047)
		video_rgb_ogo_sub.g_gain = 2047;
	if (video_rgb_ogo_sub.b_gain > 2047)
		video_rgb_ogo_sub.b_gain = 2047;
	if (video_rgb_ogo_sub.r_post_offset > 1023)
		video_rgb_ogo_sub.r_post_offset = 1023;
	if (video_rgb_ogo_sub.r_post_offset < -1024)
		video_rgb_ogo_sub.r_post_offset = -1024;
	if (video_rgb_ogo_sub.g_post_offset > 1023)
		video_rgb_ogo_sub.g_post_offset = 1023;
	if (video_rgb_ogo_sub.g_post_offset < -1024)
		video_rgb_ogo_sub.g_post_offset = -1024;
	if (video_rgb_ogo_sub.b_post_offset > 1023)
		video_rgb_ogo_sub.b_post_offset = 1023;
	if (video_rgb_ogo_sub.b_post_offset < -1024)
		video_rgb_ogo_sub.b_post_offset = -1024;

	vecm_latch_flag |= FLAG_RGB_OGO;
}
#endif

void ve_ogo_param_update(void)
{
	if (video_rgb_ogo.en > 1)
		video_rgb_ogo.en = 1;
	if (video_rgb_ogo.r_pre_offset > 1023)
		video_rgb_ogo.r_pre_offset = 1023;
	if (video_rgb_ogo.r_pre_offset < -1024)
		video_rgb_ogo.r_pre_offset = -1024;
	if (video_rgb_ogo.g_pre_offset > 1023)
		video_rgb_ogo.g_pre_offset = 1023;
	if (video_rgb_ogo.g_pre_offset < -1024)
		video_rgb_ogo.g_pre_offset = -1024;
	if (video_rgb_ogo.b_pre_offset > 1023)
		video_rgb_ogo.b_pre_offset = 1023;
	if (video_rgb_ogo.b_pre_offset < -1024)
		video_rgb_ogo.b_pre_offset = -1024;
	if (video_rgb_ogo.r_gain > 2047)
		video_rgb_ogo.r_gain = 2047;
	if (video_rgb_ogo.g_gain > 2047)
		video_rgb_ogo.g_gain = 2047;
	if (video_rgb_ogo.b_gain > 2047)
		video_rgb_ogo.b_gain = 2047;
	if (video_rgb_ogo.r_post_offset > 1023)
		video_rgb_ogo.r_post_offset = 1023;
	if (video_rgb_ogo.r_post_offset < -1024)
		video_rgb_ogo.r_post_offset = -1024;
	if (video_rgb_ogo.g_post_offset > 1023)
		video_rgb_ogo.g_post_offset = 1023;
	if (video_rgb_ogo.g_post_offset < -1024)
		video_rgb_ogo.g_post_offset = -1024;
	if (video_rgb_ogo.b_post_offset > 1023)
		video_rgb_ogo.b_post_offset = 1023;
	if (video_rgb_ogo.b_post_offset < -1024)
		video_rgb_ogo.b_post_offset = -1024;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (video_rgb_ogo_mode_sw)
		video_set_rgb_ogo();
#endif

	vecm_latch_flag |= FLAG_RGB_OGO;
}

/*for gxbbtv rgb contrast adj in vd1 matrix */
void vpp_vd1_mtx_rgb_contrast(signed int cont_val, struct vframe_s *vf, int vpp_index)
{
	unsigned int vd1_contrast;
	unsigned int con_minus_value, rgb_con_en;

	if (cont_val > 1023 || cont_val < -1024)
		return;
	cont_val = cont_val + 1024;
	/*close rgb contrast protect*/
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(XVYCC_VD1_RGB_CTRST, 0, 0, 1, vpp_index);
	/*VPP_VADJ_CTRL bit 1 on for rgb contrast adj*/
	rgb_con_en = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 1, 1);
	if (!rgb_con_en)
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(XVYCC_VD1_RGB_CTRST, 1, 1, 1, vpp_index);

	/*select full or limit range setting*/
	con_minus_value = READ_VPP_REG_BITS(XVYCC_VD1_RGB_CTRST, 4, 10);
	if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
		if (con_minus_value != 64)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(XVYCC_VD1_RGB_CTRST, 64, 4, 10, vpp_index);
	} else {
		if (con_minus_value != 0)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(XVYCC_VD1_RGB_CTRST, 0, 4, 10, vpp_index);
	}

	vd1_contrast = (READ_VPP_REG(XVYCC_VD1_RGB_CTRST) & 0xf000ffff) |
					(cont_val << 16);

	VSYNC_WRITE_VPP_REG_VPP_SEL(XVYCC_VD1_RGB_CTRST, vd1_contrast, vpp_index);
}

void vpp_contrast_adj_by_uv(int cont_u, int cont_v, int vpp_index)
{
	unsigned int coef00 = 0;
	unsigned int coef01 = 0;
	unsigned int coef02 = 0;
	unsigned int coef10 = 0;
	unsigned int coef11 = 0;
	unsigned int coef12 = 0;
	unsigned int coef20 = 0;
	unsigned int coef21 = 0;
	unsigned int coef22 = 0;
	unsigned int offst0 = 0;
	unsigned int offst1 = 0;
	unsigned int offst2 = 0;
	unsigned int pre_offst0 = 0;
	unsigned int pre_offst1 = 0;
	unsigned int pre_offst2 = 0;
	unsigned int rs = 0;
	unsigned int en = 0;

	coef00 = matrix_yuv_bypass_coef[3];
	coef01 = matrix_yuv_bypass_coef[4];
	coef02 = matrix_yuv_bypass_coef[5];
	coef10 = matrix_yuv_bypass_coef[6];
	coef11 = cont_u;
	coef12 = matrix_yuv_bypass_coef[8];
	coef20 = matrix_yuv_bypass_coef[9];
	coef21 = matrix_yuv_bypass_coef[10];
	coef22 = cont_v;
	pre_offst0 = matrix_yuv_bypass_coef[0];
	pre_offst1 = matrix_yuv_bypass_coef[1];
	pre_offst2 = matrix_yuv_bypass_coef[2];
	offst0 = matrix_yuv_bypass_coef[12];
	offst1 = matrix_yuv_bypass_coef[13];
	offst2 = matrix_yuv_bypass_coef[14];
	rs = matrix_yuv_bypass_coef[15];
	en = matrix_yuv_bypass_coef[16];

	if (flag_lc_evc &&
		chip_type_id != chip_t6w &&
		chip_type_id != chip_t6x) {
		coef00 = coef00 << 1;
		coef01 = coef01 << 1;
		coef02 = coef02 << 1;
		coef10 = coef10 << 1;
		coef11 = coef11 << 1;
		coef12 = coef12 << 1;
		coef20 = coef20 << 1;
		coef21 = coef21 << 1;
		coef22 = coef22 << 1;
		pre_offst0 = (pre_offst0 >> 1) - 256;
		pre_offst1 = (pre_offst1 >> 1) - 256;
		pre_offst2 = (pre_offst2 >> 1) - 256;
	}

	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_COEF00_01,
			    ((coef00 & 0x1fff) << 16) | (coef01 & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_COEF02_10,
			    ((coef02 & 0x1fff) << 16) | (coef10 & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_COEF11_12,
			    ((coef11 & 0x1fff) << 16) | (coef12 & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_COEF20_21,
			    ((coef20 & 0x1fff) << 16) | (coef21 & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_COEF22,
			    (coef22 & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_OFFSET0_1,
			    ((offst0 & 0xfff) << 16) | (offst1 & 0xfff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_OFFSET2,
			    (offst2 & 0xfff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_PRE_OFFSET0_1,
			    ((pre_offst0 & 0xfff) << 16) |
			    (pre_offst1 & 0xfff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VD1_MATRIX_PRE_OFFSET2,
			    (pre_offst2 & 0xfff), vpp_index);

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VD1_MATRIX_CLIP, rs, 5, 3, vpp_index);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VD1_MATRIX_EN_CTRL, en, 0, 1, vpp_index);
}

/*for gxbbtv contrast adj in vadj1*/
void vpp_vd_adj1_contrast(signed int cont_val, struct vframe_s *vf, int vpp_index)
{
	unsigned int vd1_contrast;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	unsigned int vdj1_ctl;
#endif
	int contrast_uv;
	int contrast_u;
	int contrast_v;

	if (cont_val > 1023 || cont_val < -1024)
		return;
	contrast_uv = cont_val + 1024;
	cont_val = ((cont_val + 1024) >> 3);

	if (contrast_uv < 1024) {
		contrast_u = contrast_uv;
		contrast_v = contrast_uv;
	} else {
		contrast_u = (0x600 - 0x400) * (contrast_uv - 0x400) / 0x400
			+ 0x400;
		contrast_v = contrast_uv;
	}

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_gxtvbb_cpu()) {
		/*VPP_VADJ_CTRL bit 1 off for contrast adj*/
		vdj1_ctl = READ_VPP_REG_BITS(VPP_VADJ_CTRL, 1, 1);
		if (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS) {
			if (!vdj1_ctl)
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL, 1, 1, 1, vpp_index);
		} else {
			if (vdj1_ctl)
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL, 0, 1, 1, vpp_index);
		}
	}
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_contrast_set(cont_val, VE_VADJ1, WR_DMA, vpp_index);
		return;
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y_2) & 0x7ff00) |
						(cont_val << 0);
		//VSYNC_WRITE_VPP_REG(VPP_VADJ1_Y_2, vd1_contrast);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_Y_2, cont_val, 0, 8, vpp_index);

		cur_contrast_u = contrast_u;
		cur_contrast_v = contrast_v;
		cur_vpp_index = vpp_index;
		vpp_contrast_adj_by_uv(contrast_u, contrast_v, vpp_index);
		return;
	} else if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x3ff00) |
						(cont_val << 0);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	} else {
		vd1_contrast = (READ_VPP_REG(VPP_VADJ1_Y) & 0x1ff00) |
						(cont_val << 0);
#endif
	}
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_Y, cont_val, 0, 8, vpp_index);
}

void vpp_vd_adj1_brightness(signed int bri_val, struct vframe_s *vf, int vpp_index)
{
	signed int vd1_brightness;
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	signed int ao0 =  -64;
	signed int ao1 = -512;
	signed int ao2 = -512;
	unsigned int a01 =    0, a_2 =    0;
	/* enable vd0_csc */

	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;
#endif

	if (bri_val > 1023 || bri_val < -1024)
		return;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x) {
		ve_brigtness_set(bri_val, VE_VADJ1, WR_DMA, vpp_index);
	} else
#endif
	if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
		vd1_brightness = (READ_VPP_REG(VPP_VADJ1_Y_2) & 0xff) |
			(bri_val << 8);

		//WRITE_VPP_REG(VPP_VADJ1_Y_2, vd1_brightness);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_Y_2, bri_val, 8, 11, vpp_index);
	} else if (get_cpu_type() > MESON_CPU_MAJOR_ID_GXTVBB) {
		bri_val = bri_val >> 1;
		vd1_brightness = (READ_VPP_REG(VPP_VADJ1_Y) & 0xff) |
			(bri_val << 8);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_Y, bri_val, 8, 10, vpp_index);
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	} else {
		if (vf->source_type == VFRAME_SOURCE_TYPE_TUNER ||
		    vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		    vf->source_type == VFRAME_SOURCE_TYPE_COMP)
			vd1_brightness = bri_val;
		else if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			if ((((vf->signal_type >> 29) & 0x1) == 1) &&
			    (((vf->signal_type >> 16) & 0xff) == 9)) {
				bri_val += ao0;
				if (bri_val < -1024)
					bri_val = -1024;
				vd1_brightness = bri_val;
			} else {
				vd1_brightness = bri_val;
			}
		} else {
			bri_val += ao0;
			if (bri_val < -1024)
				bri_val = -1024;
			vd1_brightness = bri_val;
		}

		a01 = ((vd1_brightness << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<	0) & 0x00000fff);
		/*p01 = ((po0 << 16) & 0x0fff0000) |*/
				/*((po1 <<  0) & 0x00000fff);*/
		/*p_2 = ((po2 <<	0) & 0x00000fff);*/

		WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
		WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
		WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
#endif
	}
}

/* brightness/contrast adjust process begin */
static void vd1_brightness_contrast(signed int brightness,
				    signed int contrast)
{
	signed int ao0 =  -64, g00 = 1024, g01 =    0, g02 =    0, po0 =  64;
	signed int ao1 = -512, g10 =    0, g11 = 1024, g12 =    0, po1 = 512;
	signed int ao2 = -512, g20 =    0, g21 =    0, g22 = 1024, po2 = 512;
	unsigned int gc0 =    0, gc1 =    0, gc2 =    0, gc3 =    0, gc4 = 0;
	unsigned int a01 =    0, a_2 =    0, p01 =    0, p_2 =    0;
	/* enable vd0_csc */
	unsigned int ori = READ_VPP_REG(VPP_MATRIX_CTRL) | 0x00000020;
	/* point to vd0_csc */
	unsigned int ctl = (ori & 0xfffffcff) | 0x00000100;

	po0 += brightness >> 1;
	if (po0 >  1023)
		po0 =  1023;
	if (po0 < -1024)
		po0 = -1024;
	g00  *= contrast + 2048;
	g00 >>= 11;
	if (g00 >  4095)
		g00 =  4095;
	if (g00 < -4096)
		g00 = -4096;
	if (contrast < 0) {
		g11  *= contrast   + 2048;
		g11 >>= 11;
	}
	if (brightness < 0) {
		g11  += brightness >> 1;
		if (g11 >  4095)
			g11 =  4095;
		if (g11 < -4096)
			g11 = -4096;
	}
	if (contrast < 0) {
		g22  *= contrast   + 2048;
		g22 >>= 11;
	}
	if (brightness < 0) {
		g22  += brightness >> 1;
		if (g22 >  4095)
			g22 =  4095;
		if (g22 < -4096)
			g22 = -4096;
	}
	gc0 = ((g00 << 16) & 0x1fff0000) | ((g01 <<  0) & 0x00001fff);
	gc1 = ((g02 << 16) & 0x1fff0000) | ((g10 <<  0) & 0x00001fff);
	gc2 = ((g11 << 16) & 0x1fff0000) | ((g12 <<  0) & 0x00001fff);
	gc3 = ((g20 << 16) & 0x1fff0000) | ((g21 <<  0) & 0x00001fff);
	gc4 = ((g22 <<  0) & 0x00001fff);
	/* #if (MESON_CPU_TYPE >= MESON_CPU_TYPE_MESONG9TV) */
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_gxtvbb_cpu()) {
		a01 = ((ao0 << 16) & 0x0fff0000) |
				((ao1 <<  0) & 0x00000fff);
		a_2 = ((ao2 <<  0) & 0x00000fff);
		p01 = ((po0 << 16) & 0x0fff0000) |
				((po1 <<  0) & 0x00000fff);
		p_2 = ((po2 <<  0) & 0x00000fff);
	} else {
#else
	{
#endif
		/* #else */
		a01 = ((ao0 << 16) & 0x07ff0000) |
				((ao1 <<  0) & 0x000007ff);
		a_2 = ((ao2 <<  0) & 0x000007ff);
		p01 = ((po0 << 16) & 0x07ff0000) |
				((po1 <<  0) & 0x000007ff);
		p_2 = ((po2 <<  0) & 0x000007ff);
	}
	/* #endif */
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ctl);
	WRITE_VPP_REG(VPP_MATRIX_COEF00_01, gc0);
	WRITE_VPP_REG(VPP_MATRIX_COEF02_10, gc1);
	WRITE_VPP_REG(VPP_MATRIX_COEF11_12, gc2);
	WRITE_VPP_REG(VPP_MATRIX_COEF20_21, gc3);
	WRITE_VPP_REG(VPP_MATRIX_COEF22, gc4);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET0_1, a01);
	WRITE_VPP_REG(VPP_MATRIX_PRE_OFFSET2, a_2);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET0_1, p01);
	WRITE_VPP_REG(VPP_MATRIX_OFFSET2, p_2);
	WRITE_VPP_REG(VPP_MATRIX_CTRL, ori);
}

void amvecm_bricon_process(signed int bri_val,
			   signed int cont_val, struct vframe_s *vf, int vpp_index)
{
	if (!vf)
		return;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HWC) {
		vpp_vd_adj1_brightness(VD1_PATH, vf, vpp_index);
		if (!(vecm_latch_flag & FLAG_VADJ1_BRI))
			vecm_latch_flag |= FLAG_VADJ1_BRI;
		pr_amve_dbg("\n[%s] HWC reset brightness, set BRI flag.\n",
			__func__);
	} else {
		if (vecm_latch_flag & FLAG_VADJ1_BRI) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			if (is_video_layer_on(VD1_PATH)) {
#endif
				vecm_latch_flag &= ~FLAG_VADJ1_BRI;
				vpp_vd_adj1_brightness(bri_val, vf, vpp_index);
				pr_amve_dbg("\n[%s] set OK, brightness:%d, type:%d\n",
					__func__, bri_val, vf->source_type);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			}
#endif
		}
	}

	if (vecm_latch_flag & FLAG_VADJ1_CON) {
		vecm_latch_flag &= ~FLAG_VADJ1_CON;
		if (contrast_adj_sel)
			vpp_vd1_mtx_rgb_contrast(cont_val, vf, vpp_index);
		else
			vpp_vd_adj1_contrast(cont_val, vf, vpp_index);
		pr_amve_dbg("\n[amve..] set vd1_contrast OK!!!\n");
		if (amve_debug & 0x100)
			pr_info("\n[amve..]%s :contrast:%d!!!\n",
				__func__, cont_val);
	}

	if (0) { /* vecm_latch_flag & FLAG_BRI_CON) { */
		vecm_latch_flag &= ~FLAG_BRI_CON;
		vd1_brightness_contrast(bri_val, cont_val);
		pr_amve_dbg("\n[amve..] set vd1_brightness_contrast OK!!!\n");
	}
}

/* brightness/contrast adjust process end */

void amvecm_color_process(signed int sat_val,
			  signed int hue_val, struct vframe_s *vf, int vpp_index)
{
	if (vecm_latch_flag & FLAG_VADJ1_COLOR) {
		vecm_latch_flag &= ~FLAG_VADJ1_COLOR;
		vpp_vd_adj1_saturation_hue(sat_val, hue_val, vf, vpp_index);
		if (amve_debug & 0x100)
			pr_info("\n[amve..]%s :saturation:%d,hue:%d!!!\n",
				__func__, sat_val, hue_val);
	}
}

/* saturation/hue adjust process end */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
/* 3d process begin */
void amvecm_3d_black_process(void)
{
	if (vecm_latch_flag & FLAG_3D_BLACK_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 0, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 0, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_BLACK_EN) {
		WRITE_VPP_REG_BITS(VIU_MISC_CTRL0, 1, 8, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL, 1, 26, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, 1, 31, 1);
		WRITE_VPP_REG_BITS(VPP_BLEND_ONECOLOR_CTRL,
				   sync_3d_black_color & 0xffffff, 0, 24);
		if (sync_3d_sync_to_vbo)
			WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 13, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_BLACK_EN;
	}
}

void amvecm_3d_sync_process(void)
{
	if (vecm_latch_flag & FLAG_3D_SYNC_DIS) {
		/* disable reg_3dsync_enable */
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 0, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_DIS;
	}
	if (vecm_latch_flag & FLAG_3D_SYNC_EN) {
		/*select vpu pwm source clock*/
		switch (READ_VPP_REG_BITS(VPU_VIU_VENC_MUX_CTRL, 0, 2)) {
		case 0:/* ENCL */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 0, 29, 2);
			break;
		case 1:/* ENCI */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 1, 29, 2);
			break;
		case 2:/* ENCP */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 2, 29, 2);
			break;
		case 3:/* ENCT */
			WRITE_VPP_REG_BITS(VPU_VPU_PWM_V0, 3, 29, 2);
			break;
		default:
			break;
		}
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC2, sync_3d_h_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_start, 0, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_v_end, 16, 13);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_polarity, 29, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, sync_3d_out_inv, 15, 1);
		WRITE_VPP_REG_BITS(VPU_VPU_3D_SYNC1, 1, 31, 1);
		vecm_latch_flag &= ~FLAG_3D_SYNC_EN;
	}
}

/* 3d process end */

/*gxlx sr adaptive param*/
#define SR_SD_SCALE_LEVEL 0x1
#define SR_HD_SCALE_LEVEL 0x2
#define SR_4k_LEVEL 0x4
#define SR_CVBS_LEVEL 0x8
#define SR_NOSCALE_LEVEL 0x10
static void amve_sr_reg_setting(unsigned int adaptive_level, int vpp_index)
{
	if (adaptive_level & SR_SD_SCALE_LEVEL) {
		am_set_regmap(&sr1reg_sd_scale, vpp_index);
	} else if (adaptive_level & SR_HD_SCALE_LEVEL) {
		am_set_regmap(&sr1reg_hd_scale, vpp_index);
	} else if (adaptive_level & SR_4k_LEVEL) {
		amvecm_sharpness_enable(1);/*peaking*/
		amvecm_sharpness_enable(3);/*lcti*/
		amvecm_sharpness_enable(5);/*drtlpf theta*/
		amvecm_sharpness_enable(7);/*debanding*/
		amvecm_sharpness_enable(9);/*dejaggy*/
		amvecm_sharpness_enable(11);/*dering*/
		amvecm_sharpness_enable(13);/*drlpf*/
	} else if (adaptive_level & SR_CVBS_LEVEL) {
		am_set_regmap(&sr1reg_cvbs, vpp_index);
	} else if (adaptive_level & SR_NOSCALE_LEVEL) {
		am_set_regmap(&sr1reg_hv_noscale, vpp_index);
	}
}

void amve_sharpness_adaptive_setting(struct vframe_s *vf,
				     unsigned int sps_h_en,
				     unsigned int sps_v_en, int vpp_index)
{
	static unsigned int adaptive_level = 1;
	unsigned int cur_level;
	unsigned int width, height;
	struct vinfo_s *vinfo = get_current_vinfo();
	unsigned int h_rate = 0;
	unsigned int w_rate = 0;
	unsigned int src_h = 0;
	unsigned int src_w = 0;
	unsigned int src_compwidth = 0;
	unsigned int src_compheight = 0;

	if (!vf)
		return;

	w_rate = vf->dpss_pps_dsx;
	h_rate = vf->dpss_pps_dsy;

	if (h_rate) {
		src_h = vf->height / (h_rate + 1);
		src_compheight = vf->compHeight / (h_rate + 1);
	} else {
		src_h = vf->height;
		src_compheight = vf->compHeight;
	}

	if (w_rate) {
		src_w = vf->width / (w_rate + 1);
		src_compwidth = vf->compWidth / (w_rate + 1);
	} else {
		src_w = vf->width;
		src_compwidth = vf->compWidth;
	}

	if (vinfo->mode == VMODE_CVBS) {
		cur_level = SR_CVBS_LEVEL;
	} else {
		width = (vf->type & VIDTYPE_COMPRESS) ?
			src_compwidth : src_w;
		height = (vf->type & VIDTYPE_COMPRESS) ?
			src_compheight : src_h;

		if (sps_h_en == 1 && sps_v_en == 1) {
			/*super scaler h and v scale up x2 */
			if (height >= 2160 && width >= 3840)
				cur_level = SR_4k_LEVEL;
			else if (height >= 720 && width >= 1280)
				cur_level = SR_HD_SCALE_LEVEL;
			else
				cur_level = SR_SD_SCALE_LEVEL;
		} else {
			/*1. super scaler no up scale */
			/*2. super scaler h up scale x2*/
			if (height >= 2160 && width >= 3840)
				cur_level = SR_4k_LEVEL;
			else
				cur_level = SR_NOSCALE_LEVEL;
		}
	}

	if (adaptive_level == cur_level)
		return;

	amve_sr_reg_setting(cur_level, vpp_index);
	adaptive_level = cur_level;
	sr_adapt_level = adaptive_level;
	pr_amve_dbg("\n[amcm..]sr_adapt_level = %d :1->sd;2->hd;4->4k\n",
		    sr_adapt_level);
}

/*gxlx sr init*/
void amve_sharpness_init(int vpp_index)
{
	am_set_regmap(&sr1reg_sd_scale, vpp_index);
}
#endif

int overscan_timing = TIMING_MAX;
int overscan_screen_mode = 0xff;
int overscan_disable;

void amvecm_fresh_overscan(struct vframe_s *vf, struct vframe_s *vf_rpt)
{
	unsigned int height = 0;
	unsigned int cur_overscan_timing = 0;
	unsigned int h_rate = 0;
	unsigned int src_h = 0;
	unsigned int src_compheight = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	unsigned int cur_fmt;
	unsigned int offset = TIMING_UHD + 1;/*av&atv*/
#endif
	struct vframe_s *cur_vf;

	cur_vf = vf ? vf : vf_rpt;

	if (overscan_disable) {
		cur_vf->ratio_control &= ~DISP_RATIO_ADAPTED_PICMODE;
		return;
	}

	h_rate = cur_vf->dpss_pps_dsy;

	if (h_rate) {
		src_h = cur_vf->height / (h_rate + 1);
		src_compheight = cur_vf->compHeight / (h_rate + 1);
	} else {
		src_h = cur_vf->height;
		src_compheight = cur_vf->compHeight;
	}

	if (overscan_table[0].load_flag) {
		height = (cur_vf->type & VIDTYPE_COMPRESS) ?
			src_compheight : src_h;
		if (height <= 480)
			cur_overscan_timing = TIMING_SD_480;
		else if (height <= 576)
			cur_overscan_timing = TIMING_SD_576;
		else if (height <= 720)
			cur_overscan_timing = TIMING_HD;
		else if (height <= 1088)
			cur_overscan_timing = TIMING_FHD;
		else
			cur_overscan_timing = TIMING_UHD;

		overscan_timing = cur_overscan_timing;
		overscan_screen_mode =
			overscan_table[overscan_timing].screen_mode;

		cur_vf->pic_mode.AFD_enable =
			overscan_table[overscan_timing].afd_enable;
		/*local play screen mode set by decoder*/
		if (!(cur_vf->pic_mode.screen_mode == VIDEO_WIDEOPTION_CUSTOM &&
		      cur_vf->pic_mode.AFD_enable)) {
			if (overscan_table[0].source == SOURCE_MPEG)
				cur_vf->pic_mode.screen_mode = 0xff;
			else
				cur_vf->pic_mode.screen_mode = overscan_screen_mode;
		}

		if (cur_vf->pic_mode.provider == PIC_MODE_PROVIDER_WSS &&
		    cur_vf->pic_mode.AFD_enable) {
			if (vf_rpt) {
				cur_vf->pic_mode.hs = overscan_table[overscan_timing].hs;
				cur_vf->pic_mode.he = overscan_table[overscan_timing].he;
				cur_vf->pic_mode.vs = overscan_table[overscan_timing].vs;
				cur_vf->pic_mode.ve = overscan_table[overscan_timing].ve;
				pr_amve_dbg("\n[%s] vf_rpt.\n", __func__);
			} else {
				cur_vf->pic_mode.hs += overscan_table[overscan_timing].hs;
				cur_vf->pic_mode.he += overscan_table[overscan_timing].he;
				cur_vf->pic_mode.vs += overscan_table[overscan_timing].vs;
				cur_vf->pic_mode.ve += overscan_table[overscan_timing].ve;
				pr_amve_dbg("\n[%s] vf.\n", __func__);
			}
		} else {
			cur_vf->pic_mode.hs = overscan_table[overscan_timing].hs;
			cur_vf->pic_mode.he = overscan_table[overscan_timing].he;
			cur_vf->pic_mode.vs = overscan_table[overscan_timing].vs;
			cur_vf->pic_mode.ve = overscan_table[overscan_timing].ve;
		}
		cur_vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		pr_amve_dbg("\n[%s] DISP_RATIO provider[%d]/AFD[%d]/hs[%d]/he[%d]/vs[%d]/ve[%d].\n",
			__func__, cur_vf->pic_mode.provider, cur_vf->pic_mode.AFD_enable,
			cur_vf->pic_mode.hs, cur_vf->pic_mode.he, cur_vf->pic_mode.vs,
			cur_vf->pic_mode.ve);
	}
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
	if (overscan_table[offset].load_flag) {
		cur_fmt = cur_vf->sig_fmt;
		if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_M)
			cur_overscan_timing = TIMING_NTST_M;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_443)
			cur_overscan_timing = TIMING_NTST_443;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_I)
			cur_overscan_timing = TIMING_PAL_I;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_M)
			cur_overscan_timing = TIMING_PAL_M;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_60)
			cur_overscan_timing = TIMING_PAL_60;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_PAL_CN)
			cur_overscan_timing = TIMING_PAL_CN;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_SECAM)
			cur_overscan_timing = TIMING_SECAM;
		else if (cur_fmt == TVIN_SIG_FMT_CVBS_NTSC_50)
			cur_overscan_timing = TIMING_NTSC_50;
		else
			return;
		overscan_timing = cur_overscan_timing;
		overscan_screen_mode =
			overscan_table[overscan_timing].screen_mode;
		cur_vf->pic_mode.AFD_enable =
			overscan_table[overscan_timing].afd_enable;

		if (!(cur_vf->pic_mode.screen_mode == VIDEO_WIDEOPTION_CUSTOM &&
		      cur_vf->pic_mode.AFD_enable))
			cur_vf->pic_mode.screen_mode = overscan_screen_mode;

		if (cur_vf->pic_mode.provider == PIC_MODE_PROVIDER_WSS &&
		    cur_vf->pic_mode.AFD_enable) {
			if (vf_rpt) {
				cur_vf->pic_mode.hs = overscan_table[overscan_timing].hs;
				cur_vf->pic_mode.he = overscan_table[overscan_timing].he;
				cur_vf->pic_mode.vs = overscan_table[overscan_timing].vs;
				cur_vf->pic_mode.ve = overscan_table[overscan_timing].ve;
				pr_amve_dbg("\n[%s] vf_rpt.\n", __func__);
			} else {
				cur_vf->pic_mode.hs += overscan_table[overscan_timing].hs;
				cur_vf->pic_mode.he += overscan_table[overscan_timing].he;
				cur_vf->pic_mode.vs += overscan_table[overscan_timing].vs;
				cur_vf->pic_mode.ve += overscan_table[overscan_timing].ve;
				pr_amve_dbg("\n[%s] vf.\n", __func__);
			}
		} else {
			cur_vf->pic_mode.hs = overscan_table[overscan_timing].hs;
			cur_vf->pic_mode.he = overscan_table[overscan_timing].he;
			cur_vf->pic_mode.vs = overscan_table[overscan_timing].vs;
			cur_vf->pic_mode.ve = overscan_table[overscan_timing].ve;
		}
		cur_vf->ratio_control |= DISP_RATIO_ADAPTED_PICMODE;
		pr_amve_dbg("\n[%s] DISP_RATIO provider[%d]/AFD[%d]/hs[%d]/he[%d]/vs[%d]/ve[%d].\n",
			__func__, cur_vf->pic_mode.provider, cur_vf->pic_mode.AFD_enable,
			cur_vf->pic_mode.hs, cur_vf->pic_mode.he, cur_vf->pic_mode.vs,
			cur_vf->pic_mode.ve);
		/*av&atv no reset*/
		pq_user_latch_flag &= ~PQ_USER_OVERSCAN_RESET;
	}
#endif
	if (pq_user_latch_flag & PQ_USER_OVERSCAN_RESET) {
		pq_user_latch_flag &= ~PQ_USER_OVERSCAN_RESET;
		cur_vf->pic_mode.AFD_enable = 0;
		cur_vf->pic_mode.screen_mode = 0;
		cur_vf->pic_mode.hs = 0;
		cur_vf->pic_mode.he = 0;
		cur_vf->pic_mode.vs = 0;
		cur_vf->pic_mode.ve = 0;
		cur_vf->ratio_control &= ~DISP_RATIO_ADAPTED_PICMODE;
		pr_amve_dbg("\n[%s] PQ_USER_OVERSCAN_RESET.\n",
			__func__);
	}
}

void amvecm_reset_overscan(void)
{
	unsigned int offset = TIMING_UHD + 1;/*av&atv*/
	enum ve_source_input_e source0;

	source0 = overscan_table[0].source;
	if (overscan_disable)
		return;

	if (overscan_timing != TIMING_MAX) {
		overscan_timing = TIMING_MAX;
		if (source0 != SOURCE_DTV &&
			source0 != SOURCE_MPEG)
			overscan_table[0].load_flag = 0;
		else if (!atv_source_flg)
			overscan_table[offset].load_flag = 0;

		if (source0 != SOURCE_DTV &&
			source0 != SOURCE_MPEG &&
			!atv_source_flg)
			overscan_screen_mode = 0xff;
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
unsigned int *plut3d;
unsigned int *plut3d_base;
static int P3dlut_tab[289] = {0};
static int P3dlut_regtab[291] = {0};
static unsigned char *pkeylutall;
static unsigned int *pkeylut;
static unsigned int max_3dlut_count = 3;
#define LUT3DBIN_PATH "/vendor/etc/tvconfig/panel/3dlut.bin"
/*plut3d[]: 17*17*17*3*10bit for tl1*/
/*plut3d[]: 17*17*17*3*12bit for tm2*/
/*bfromkey == 0 data from input para */
/*bfromkey == 1 data from unifykey */
/*bfromkey == 2 data from bin file */

static int _update_lut3d_data(unsigned int p3dlut_in[][3])
{
	int d0, d1, d2;
	int index0, index1, idn;
	int differ = 0;
	int tmp0, tmp1, tmp2;
	int lut3d_points;

	if (chip_type_id == chip_t6d)
		lut3d_points = LUT3D_POINTS9;
	else
		lut3d_points = LUT3D_POINTS;

	for (d0 = 0; d0 < lut3d_points; d0++) {
		for (d1 = 0; d1 < lut3d_points; d1++) {
			for (d2 = 0; d2 < lut3d_points; d2++) {
				index0 =
					d0 * lut3d_points * lut3d_points +
					d1 * lut3d_points + d2;
				index1 =
					d2 * lut3d_points * lut3d_points +
					d1 * lut3d_points + d0;
				if (lut3d_order == 2) {
					tmp0 = p3dlut_in[index0][2] & 0xfff;
					tmp1 = p3dlut_in[index0][1] & 0xfff;
					tmp2 = p3dlut_in[index0][0] & 0xfff;
				} else {
					idn = lut3d_order ? index1 : index0;
					tmp0 = p3dlut_in[idn][0] & 0xfff;
					tmp1 = p3dlut_in[idn][1] & 0xfff;
					tmp2 = p3dlut_in[idn][2] & 0xfff;
				}

				if (!differ &&
					(plut3d[index0 * 3 + 0] != tmp0 ||
					plut3d[index0 * 3 + 1] != tmp1 ||
					plut3d[index0 * 3 + 2] != tmp2))
					differ = 1;
				plut3d[index0 * 3 + 0] = tmp0;
				plut3d[index0 * 3 + 1] = tmp1;
				plut3d[index0 * 3 + 2] = tmp2;
			}
		}
	}

	return differ;
}

void update_lut3d_base_data(unsigned int p3dlut_in[][3])
{
	int d0, d1, d2;
	int index0, index1, idn;
	int lut3d_points;

	if (chip_type_id == chip_t6d)
		lut3d_points = LUT3D_POINTS9;
	else
		lut3d_points = LUT3D_POINTS;

	if (!plut3d_base)
		return;

	for (d0 = 0; d0 < lut3d_points; d0++) {
		for (d1 = 0; d1 < lut3d_points; d1++) {
			for (d2 = 0; d2 < lut3d_points; d2++) {
				index0 =
					d0 * lut3d_points * lut3d_points +
					d1 * lut3d_points + d2;
				index1 =
					d2 * lut3d_points * lut3d_points +
					d1 * lut3d_points + d0;
				if (lut3d_order == 2) {
					plut3d_base[index0 * 3 + 0] =
					p3dlut_in[index0][2] & 0xfff;
					plut3d_base[index0 * 3 + 1] =
					p3dlut_in[index0][1] & 0xfff;
					plut3d_base[index0 * 3 + 2] =
					p3dlut_in[index0][0] & 0xfff;
				} else {
					idn =
					lut3d_order ? index1 : index0;
					plut3d_base[index0 * 3 + 0] =
					p3dlut_in[idn][0] & 0xfff;
					plut3d_base[index0 * 3 + 1] =
					p3dlut_in[idn][1] & 0xfff;
					plut3d_base[index0 * 3 + 2] =
					p3dlut_in[idn][2] & 0xfff;
				}
			}
		}
	}
}

uint flag_lut3d_resume;

int vpp_set_lut3d(int bfromkey,
	int keyindex,
	unsigned int p3dlut_in[][3],
	int blut3dcheck, int rdma_w)
{
#ifdef CONFIG_AMLOGIC_LCD
	int i, key_len, key_count, offset = 0;
#else
	int i, key_len, key_count, offset = 0;
#endif
	int d0, d1, d2, counter, size, index0, index1, idn;
	u32 dwtemp, ctltemp, wrgb[3];
	unsigned int combine3 = 0;
	#ifdef CONFIG_SET_FS
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	#endif
	int lut3d_single_sz;
	int lut3d_points;
	int differ_flag = 1;

	if (chip_type_id == chip_t6d) {
		lut3d_single_sz = LUT3D_SINGLE_SIZE9;
		lut3d_points = LUT3D_POINTS9;
	} else {
		lut3d_single_sz = LUT3D_SINGLE_SIZE;
		lut3d_points = LUT3D_POINTS;
	}
	pr_amve_dbg("lut3d_en=%d, ct_en= %d, rdma_w =%d\n",
		lut3d_en, ct_en, rdma_w);

	if (!lut3d_en)
		return 1;

	if (!plut3d)
		return 1;

	mutex_lock(&vpp_lut3d_lock);

	/* load 3d lut from unifykey store */
	if (bfromkey) {
		/* allocate a little bit more buffer than required for safety */
		key_len = ((lut3d_single_sz + 1) * 5 * max_3dlut_count);
		pkeylutall = kmalloc(key_len, GFP_KERNEL);
		if (!pkeylutall) {
			mutex_unlock(&vpp_lut3d_lock);
			return 1;
		}

		pkeylut = kmalloc((lut3d_single_sz + 1) * sizeof(int) * 3,
				  GFP_KERNEL);
		if (!pkeylut) {
			kfree(pkeylutall);
			mutex_unlock(&vpp_lut3d_lock);
			return 1;
		}

		if (bfromkey == 1) {
#ifdef CONFIG_AMLOGIC_LCD
			//ret = -1; //lcd_unifykey_get_size("lcd_3dlut", &key_len);
			//if (ret < 0) {
				kfree(pkeylutall);
				kfree(pkeylut);
				mutex_unlock(&vpp_lut3d_lock);
				return 1;
			//}
			//ret = -1;
			//lcd_unifykey_get_no_header("lcd_3dlut",
			//	(unsigned char *)pkeylutall,
			//	key_len);
			//if (ret < 0) {
			//	kfree(pkeylutall);
			//	kfree(pkeylut);
			//	mutex_unlock(&vpp_lut3d_lock);
			//	return 1;
			//}
#else
			kfree(pkeylutall);
			kfree(pkeylut);
			mutex_unlock(&vpp_lut3d_lock);
			return 1;
#endif
		} else if (bfromkey == 2) {
#ifdef CONFIG_SET_FS
			fp = filp_open(LUT3DBIN_PATH, O_RDONLY, 0);
			if (IS_ERR(fp)) {
				kfree(pkeylutall);
				kfree(pkeylut);
				mutex_unlock(&vpp_lut3d_lock);
				return 1;
			}
			//fs = get_fs();
			//set_fs(KERNEL_DS);
			pos = 0;
			key_len = vfs_read(fp, pkeylutall, key_len, &pos);

			if (key_len == 0) {
				kfree(pkeylutall);
				kfree(pkeylut);
				mutex_unlock(&vpp_lut3d_lock);
				return 1;
			}
			filp_close(fp, NULL);
			//set_fs(fs);
#else
			kfree(pkeylutall);
			kfree(pkeylut);
			mutex_unlock(&vpp_lut3d_lock);
			return 1;
#endif
		}

		size = ((lut3d_single_sz + 1) * 9) / 2;
		key_count = key_len / size;
		if (keyindex >= key_count || keyindex <= INT_MIN) {
			pr_info("warn: key index out of range");
			kfree(pkeylutall);
			kfree(pkeylut);
			mutex_unlock(&vpp_lut3d_lock);
			return 1;
		}

		offset = keyindex * size;
		counter = 0;
		combine3 = 0;
		for (i = 0; i < size; i += 3) {
			combine3 = pkeylutall[offset + i];
			combine3 |= (pkeylutall[offset + i + 1] << 8);
			combine3 |= (pkeylutall[offset + i + 2] << 16);
			pkeylut[counter] = combine3 & 0xfff;
			++counter;
			pkeylut[counter] = (combine3 >> 12) & 0xfff;
			++counter;
		}

		if (counter != (lut3d_single_sz + 1) * 3)
			pr_info("warn: key lut read size error");

		for (d0 = 0; d0 < lut3d_points; d0++) {
			for (d1 = 0; d1 < lut3d_points; d1++) {
				for (d2 = 0; d2 < lut3d_points; d2++) {
					index0 =
						d0 * lut3d_points * lut3d_points +
						d1 * lut3d_points + d2;
					index1 =
						d2 * lut3d_points * lut3d_points +
						d1 * lut3d_points + d0;
					idn =
					lut3d_order ? index1 : index0;
					plut3d[index0 * 3 + 0] =
					pkeylut[idn * 3 + 0] & 0xfff;
					plut3d[index0 * 3 + 1] =
					pkeylut[idn * 3 + 1] & 0xfff;
					plut3d[index0 * 3 + 2] =
					pkeylut[idn * 3 + 2] & 0xfff;
				}
			}
		}

		if (ct_en && plut3d_base)
			memcpy(plut3d_base, plut3d, lut3d_single_sz * 3 * sizeof(int));

		differ_flag = 1;
		kfree(pkeylutall);
		kfree(pkeylut);
	} else {
		if (p3dlut_in)
			differ_flag = _update_lut3d_data(p3dlut_in);
	}

	if (!differ_flag && !flag_lut3d_resume) {
		pr_amve_dbg("[%s]same curve data not write.\n", __func__);
		mutex_unlock(&vpp_lut3d_lock);
		return 0;
	}

	if (flag_lut3d_resume)
		flag_lut3d_resume = 0;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		post_lut3d_set(plut3d);
	} else
#endif
	{
		if (lut3d_dma_case && chip_type_id == chip_t6x) {
			if (!rdma_w) {
				am_dma_set_mif_data_3dlut(plut3d);
				if (lut3d_debug == 1) {
					for (i = 0; i < lut3d_points * lut3d_points; i++)
						pr_info("%d: %03x %03x %03x\n",
							i,
							plut3d[i * 3 + 0],
							plut3d[i * 3 + 1],
							plut3d[i * 3 + 2]);
				}
			} else {
				if (ct_en || lut3d_en)
					vecm_latch_flag2 |= LUT3D_BASE_UPDATE;
			}
		} else {
			WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, 0, 1, 1);
			ctltemp  = READ_VPP_REG(VPP_LUT3D_CTRL);
			WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp & 0xFFFFFFFE);
			usleep_range(16000, 16001);

			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
			WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0 | (0 << 31));
			for (i = 0; i < lut3d_single_sz; i++) {
				//{comp0, comp1, comp2}
				WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
					((plut3d[i * 3 + 1] & 0xfff) << 16) |
					(plut3d[i * 3 + 2] & 0xfff));
				WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
					(plut3d[i * 3 + 0] & 0xfff)); /*MSB*/
				if (lut3d_debug == 1 && (i < lut3d_points * lut3d_points))
					pr_info("%d: %03x %03x %03x\n",
						i,
						plut3d[i * 3 + 0],
						plut3d[i * 3 + 1],
						plut3d[i * 3 + 2]);
			}
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
			WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp);
		}

		if (blut3dcheck) {
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
			WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0 | (1 << 31));
			for (i = 0; i < lut3d_single_sz; i++) {
				dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
				wrgb[2] = dwtemp & 0xfff;
				wrgb[1] = (dwtemp >> 16) & 0xfff;
				dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
				wrgb[0] = dwtemp & 0xfff;
				if (i < 97) {
					P3dlut_regtab[i * 3 + 2] = wrgb[2];
					P3dlut_regtab[i * 3 + 1] = wrgb[1];
					P3dlut_regtab[i * 3 + 0] = wrgb[0];
				}
				if (wrgb[0] != plut3d[i * 3 + 0]) {
					pr_info("%s:Error: Lut3d check error at R[%d]\n",
						__func__, i);
					WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
					return 1;
				}
				if (wrgb[1] != plut3d[i * 3 + 1]) {
					pr_info("%s:Error: Lut3d check error at G[%d]\n",
						__func__, i);
					WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
					return 1;
				}
				if (wrgb[2] != plut3d[i * 3 + 2]) {
					pr_info("%s:Error: Lut3d check error at B[%d]\n",
						__func__, i);
					WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
					return 1;
				}
			}
			pr_info("%s: Lut3d check ok!!\n", __func__);
		}
	}

	mutex_unlock(&vpp_lut3d_lock);

	return 0;
}

int vpp_check_lut3d(void)
{
	int i;
	int lut3d_single_sz;
	int lut3d_points;
	u32 dwtemp, wrgb[3];

	if (chip_type_id == chip_t6d) {
		lut3d_single_sz = LUT3D_SINGLE_SIZE9;
		lut3d_points = LUT3D_POINTS9;
	} else {
		lut3d_single_sz = LUT3D_SINGLE_SIZE;
		lut3d_points = LUT3D_POINTS;
	}

	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
	WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0 | (1 << 31));

	for (i = 0; i < lut3d_points; i++) {
		dwtemp	= READ_VPP_REG(VPP_LUT3D_RAM_DATA);
		wrgb[2] = dwtemp & 0xfff;
		wrgb[1] = (dwtemp >> 16) & 0xfff;
		dwtemp	= READ_VPP_REG(VPP_LUT3D_RAM_DATA);
		wrgb[0] = dwtemp & 0xfff;
		pr_info("[%d,%d,%d],read[%d,%d,%d]\n",
			plut3d[i * 3 + 0], plut3d[i * 3 + 1], plut3d[i * 3 + 2],
			wrgb[0], wrgb[1], wrgb[2]);
	}

	WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR, 0 | (1 << 31));
	for (i = 0; i < lut3d_single_sz; i++) {
		dwtemp	= READ_VPP_REG(VPP_LUT3D_RAM_DATA);
		wrgb[2] = dwtemp & 0xfff;
		wrgb[1] = (dwtemp >> 16) & 0xfff;
		dwtemp	= READ_VPP_REG(VPP_LUT3D_RAM_DATA);
		wrgb[0] = dwtemp & 0xfff;
		if (wrgb[0] != plut3d[i * 3 + 0]) {
			pr_info("%s:Error: Lut3d check error at R[%d]\n",
				__func__, i);
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
			return 1;
		}
		if (wrgb[1] != plut3d[i * 3 + 1]) {
			pr_info("%s:Error: Lut3d check error at G[%d]\n",
				__func__, i);
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
			return 1;
		}
		if (wrgb[2] != plut3d[i * 3 + 2]) {
			pr_info("%s:Error: Lut3d check error at B[%d]\n",
				__func__, i);
			WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
			return 1;
		}
	}
	pr_info("%s: Lut3d check ok!!\n", __func__);
	WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
	return 0;
}

static int g_degamma_lut[65] = {
	0, 0, 0, 1, 2, 3, 5, 7, 10, 13, 17, 21, 25, 30, 36, 42, 48, 55, 62, 70, 79,
	88, 97, 107, 118, 129, 141, 153, 165, 179, 193, 207, 222, 238, 254, 271, 288,
	306, 324, 344, 363, 384, 404, 426, 448, 471, 494, 518, 543, 568, 594, 620, 647,
	675, 703, 732, 762, 792, 823, 855, 887, 920, 953, 988, 1023
};

int lut3d_degamma_map_cmp(int idata, int bitdepth)
{
	int lft, rgt, frac;
	int odata;

	lft   =  idata >> (bitdepth - 6);
	rgt   = (lft == 64) ? 64 : (lft + 1);
	frac  =  idata - (lft << (bitdepth - 6));
	odata = (g_degamma_lut[lft] * (64 - frac) +
			g_degamma_lut[rgt] * frac + (1 << 5)) >> 6;
	return odata;
}

int lut3d_test(int test_case, int enable)
{
	unsigned int i;
	int lut3d_size;
	int lut3d_points;
	unsigned int bitdepth = 12;

	if (chip_type_id == chip_t6d) {
		lut3d_size = LUT3D_SINGLE_SIZE9;
		lut3d_points = LUT3D_POINTS9;
	} else {
		lut3d_size = LUT3D_SINGLE_SIZE;
		lut3d_points = LUT3D_POINTS;
	}

	if (is_meson_tl1_cpu() ||
		is_meson_t5d_cpu() ||
		is_meson_t5_cpu() ||
		is_meson_t3_cpu() ||
		is_meson_t5w_cpu() ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_t3x ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		bitdepth = 10;
	lut3d_en = enable;
	vpp_lut3d_table_init(-1, -1, -1);

	if (test_case == 1) { // degamma
		for (i = 0; i < lut3d_size; i++) {
			plut3d[i * 3] = lut3d_degamma_map_cmp(plut3d[i * 3], bitdepth);
			plut3d[i * 3 + 1] = lut3d_degamma_map_cmp(plut3d[i * 3 + 1],
				bitdepth);
			plut3d[i * 3 + 2] = lut3d_degamma_map_cmp(plut3d[i * 3 + 2],
				bitdepth);
		}
	} else if (test_case == 2) { //y2r
		for (i = 0; i < lut3d_size; i++) {
			ycbcr2rgbpc_nb(&plut3d[i * 3], &plut3d[i * 3 + 1],
				&plut3d[i * 3 + 2],
				plut3d[i * 3], plut3d[i * 3 + 1], plut3d[i * 3 + 2], bitdepth);
		}
	}
	vpp_set_lut3d(0, 0, 0, 0, 0);
	vpp_enable_lut3d(enable);
	return 0;
}

void lut3d_update(unsigned int p3dlut_in[][3], int vpp_index)
{
	int d0, d1, d2, index0;
	int i;
	int offset = 0;
	int lut3d_single_sz;
	int lut3d_points;

	pr_amve_dbg("lut3d_en=%d, ct_en= %d\n", lut3d_en, ct_en);
	if (!ct_en && !lut3d_en)
		return;

	if (chip_type_id == chip_t6d) {
		lut3d_single_sz = LUT3D_SINGLE_SIZE9;
		lut3d_points = LUT3D_POINTS9;
	} else {
		lut3d_single_sz = LUT3D_SINGLE_SIZE;
		lut3d_points = LUT3D_POINTS;
	}

	if (p3dlut_in) {
		if (is_meson_t7_cpu())
			offset = 2;

		for (d0 = 0; d0 < lut3d_points; d0++) {
			for (d1 = 0; d1 < lut3d_points; d1++) {
				for (d2 = 0; d2 < lut3d_points; d2++) {
					index0 =
						d0 * lut3d_points * lut3d_points +
						d1 * lut3d_points + d2;
					plut3d[index0 * 3 + 0] =
					(p3dlut_in[index0][0] << offset) & 0xfff;
					plut3d[index0 * 3 + 1] =
					(p3dlut_in[index0][1] << offset) & 0xfff;
					plut3d[index0 * 3 + 2] =
					(p3dlut_in[index0][2] << offset) & 0xfff;
				}
			}
		}
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		post_lut3d_update(plut3d, vpp_index);
	} else
#endif
	{
		if (lut3d_dma_case && chip_type_id == chip_t6x) {
			am_dma_set_mif_data_3dlut(plut3d);
			if (lut3d_debug == 1) {
				for (i = 0; i < lut3d_points * lut3d_points; i++)
					pr_info("%d: %03x %03x %03x\n",
						i,
						plut3d[i * 3 + 0],
						plut3d[i * 3 + 1],
						plut3d[i * 3 + 2]);
			}
		} else {
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LUT3D_CBUS2RAM_CTRL, 1, vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LUT3D_RAM_ADDR, 0 | (0 << 31), vpp_index);

			for (i = 0; i < lut3d_single_sz; i++) {
				//{comp0, comp1, comp2}
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LUT3D_RAM_DATA,
					((plut3d[i * 3 + 1] & 0xfff) << 16) |
					(plut3d[i * 3 + 2] & 0xfff), vpp_index);
				VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LUT3D_RAM_DATA,
					(plut3d[i * 3 + 0] & 0xfff), vpp_index); /*MSB*/
				if (lut3d_debug == 1 && (i < lut3d_points * lut3d_points))
					pr_info("%d: %03x %03x %03x\n",
						i,
						plut3d[i * 3 + 0],
						plut3d[i * 3 + 1],
						plut3d[i * 3 + 2]);
			}

			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_LUT3D_CBUS2RAM_CTRL, 0, vpp_index);
		}
	}
}

int vpp_write_lut3d_section(int index, int section_len,
	unsigned int *p3dlut_section_in)
{
	int i;
	int r_offset, g_offset;
	u32 ctltemp;
	int lut3d_points;

	if (!lut3d_en)
		return 1;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		post_lut3d_section_write(index, section_len,
			p3dlut_section_in);
	} else
#endif
	{
		if (chip_type_id == chip_t6d)
			lut3d_points = LUT3D_POINTS9;
		else
			lut3d_points = LUT3D_POINTS;

		index = index * section_len / lut3d_points;

		g_offset = index % lut3d_points;
		r_offset = index / lut3d_points;

		ctltemp  = READ_VPP_REG(VPP_LUT3D_CTRL);
		WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp & 0xFFFFFFFE);
		usleep_range(16000, 16001);

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
		/* LUT3D_RAM_ADDR  bit20:16 R, bit12:8 G, bit4:0 B */
		WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR,
			(r_offset << 16) | (g_offset << 8) | (0 << 31));
		for (i = 0; i < section_len; i++) {
			//{comp0, comp1, comp2}
			WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
				((p3dlut_section_in[i * 3 + 1] & 0xfff) << 16) |
				(p3dlut_section_in[i * 3 + 2] & 0xfff));
			WRITE_VPP_REG(VPP_LUT3D_RAM_DATA,
				(p3dlut_section_in[i * 3 + 0] & 0xfff)); /*MSB*/
		}

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
		WRITE_VPP_REG(VPP_LUT3D_CTRL, ctltemp);
	}

	return 0;
}

int vpp_read_lut3d_section(int index, int section_len,
	unsigned int *p3dlut_section_out)
{
	int i;
	int r_offset, g_offset;
	u32 dwtemp;
	int lut3d_points;

	if (!lut3d_en)
		return 1;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		post_lut3d_section_read(index, section_len,
			p3dlut_section_out);
	} else
#endif
	{
		if (chip_type_id == chip_t6d)
			lut3d_points = LUT3D_POINTS9;
		else
			lut3d_points = LUT3D_POINTS;

		index = index * section_len / lut3d_points;

		g_offset = index % lut3d_points;
		r_offset = index / lut3d_points;

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 1);
		/* LUT3D_RAM_ADDR  bit20:16 R, bit12:8 G, bit4:0 B */
		WRITE_VPP_REG(VPP_LUT3D_RAM_ADDR,
			(r_offset << 16) | (g_offset << 8) | (1 << 31));
		for (i = 0; i < section_len; i++) {
			dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
			p3dlut_section_out[i * 3 + 2] = dwtemp & 0xfff;
			p3dlut_section_out[i * 3 + 1] = (dwtemp >> 16) & 0xfff;
			dwtemp  = READ_VPP_REG(VPP_LUT3D_RAM_DATA);
			p3dlut_section_out[i * 3 + 0] = dwtemp & 0xfff;
		}

		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
	}

	return 0;
}

int vpp_enable_lut3d(int enable)
{

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t3x) {
		post_lut3d_ctl(WR_VCB, enable, 0);
	} else
#endif
	{
		pr_amve_dbg("enable = %d\n", enable);
		WRITE_VPP_REG(VPP_LUT3D_CBUS2RAM_CTRL, 0);
		/*reg_lut3d_extend_en[6:4]*/
		/*reg_lut3d_en*/
		WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, enable, 0, 1);
		WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, 0x7, 4, 3);
		WRITE_VPP_REG_BITS(VPP_LUT3D_CTRL, lut3d_dma_case, 1, 1);
	}
	return 0;
}

/*table: use for yuv->rgb*/
void vpp_lut3d_table_init(int r, int g, int b)
{
	int d0, d1, d2, step, max_val = 4095;
	unsigned int i, index;
	int lut3d_size;
	int lut3d_points;

	if (chip_type_id == chip_t6d) {
		lut3d_size = LUT3D_SIZE9;
		lut3d_points = LUT3D_POINTS9;
	} else {
		lut3d_size = LUT3D_SIZE;
		lut3d_points = LUT3D_POINTS;
	}

	if (!plut3d)
		plut3d = kzalloc(lut3d_size * sizeof(int), GFP_ATOMIC);

	if (!plut3d)
		return;

	if (is_meson_tl1_cpu() ||
		is_meson_t5d_cpu() ||
		is_meson_t5_cpu() ||
		is_meson_t3_cpu() ||
		is_meson_t5w_cpu() ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_t3x ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		max_val = 1023;

	step = (max_val + 1) / (lut3d_points - 1);

	/*initialize the lut3d ad same input and output;*/
	for (d0 = 0; d0 < lut3d_points; d0++) {
		for (d1 = 0; d1 < lut3d_points; d1++) {
			for (d2 = 0; d2 < lut3d_points; d2++) {
				index =
				d0 * lut3d_points * lut3d_points * 3 +
				d1 * lut3d_points * 3 + d2 * 3;
				if (r < 0 || g < 0 || b < 0) {
					/* bypass data */
					plut3d[index + 0] =
						min(max_val, d0 * step);
					plut3d[index + 1] =
						min(max_val, d1 * step);
					plut3d[index + 2] =
						min(max_val, d2 * step);
				} else {
					plut3d[index + 0] =
						min(max_val, r);
					plut3d[index + 1] =
						min(max_val, g);
					plut3d[index + 2] =
						min(max_val, b);
				}
			}
		}
	}
	for (i = 0; i < lut3d_points * lut3d_points; i++)
		P3dlut_tab[i] = plut3d[i];

	am_dma_3dlut_buffer_init();
}

void vpp_lut3d_base_table_init(void)
{
	int d0, d1, d2, step, max_val = 4095;
	unsigned int index;
	int lut3d_size;
	int lut3d_points;

	if (chip_type_id == chip_t6d) {
		lut3d_size = LUT3D_SIZE9;
		lut3d_points = LUT3D_POINTS9;
	} else {
		lut3d_size = LUT3D_SIZE;
		lut3d_points = LUT3D_POINTS;
	}
	if (!plut3d_base)
		plut3d_base = vmalloc(lut3d_size * sizeof(int));

	if (!plut3d_base)
		return;

	if (is_meson_tl1_cpu() ||
		is_meson_t5d_cpu() ||
		is_meson_t5_cpu() ||
		is_meson_t3_cpu() ||
		is_meson_t5w_cpu() ||
		chip_type_id == chip_t5m ||
		chip_type_id == chip_t3x ||
		chip_type_id == chip_t6d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x)
		max_val = 1023;

	step = (max_val + 1) / (lut3d_points - 1);

	/*initialize the lut3d ad same input and output;*/
	for (d0 = 0; d0 < lut3d_points; d0++) {
		for (d1 = 0; d1 < lut3d_points; d1++) {
			for (d2 = 0; d2 < lut3d_points; d2++) {
				index =
				d0 * lut3d_points * lut3d_points * 3 +
				d1 * lut3d_points * 3 + d2 * 3;
				/* bypass data */
				plut3d_base[index + 0] =
					min(max_val, d0 * step);
				plut3d_base[index + 1] =
					min(max_val, d1 * step);
				plut3d_base[index + 2] =
					min(max_val, d2 * step);
			}
		}
	}
}

void vpp_lut3d_table_release(void)
{
	kfree(plut3d);
	plut3d = NULL;

	vfree(plut3d_base);
	plut3d_base = NULL;

	am_dma_3dlut_buffer_release();
}

void dump_plut3d_table(void)
{
	unsigned int i, j;
	int lut3d_points;

	if (chip_type_id == chip_t6d)
		lut3d_points = LUT3D_POINTS9;
	else
		lut3d_points = LUT3D_POINTS;

	pr_info("*****dump lut3d table:plut3d[0]~plut3d[%d]*****\n",
		lut3d_points * lut3d_points - 1);
	for (i = 0; i < lut3d_points; i++) {
		pr_info("*****dump plut3d:%d * %d*****\n", lut3d_points, i);
		for (j = 0; j < lut3d_points; j++) {
			if ((j % (lut3d_points - 1)) == 0)
				pr_info("\n");
			pr_info("%d\t", P3dlut_tab[i * lut3d_points + j]);
		}
	}
}

void dump_plut3d_reg_table(void)
{
	unsigned int i, j;
	int lut3d_points;

	if (chip_type_id == chip_t6d)
		lut3d_points = LUT3D_POINTS9;
	else
		lut3d_points = LUT3D_POINTS;

	pr_info("*****dump lut3d reg table:[0]~[%d]*****\n",
		lut3d_points * lut3d_points - 1);
	for (i = 0; i < lut3d_points; i++) {
		pr_info("*****dump plut3d regtab:%d * %d*****\n", lut3d_points, i);
		for (j = 0; j < lut3d_points; j++) {
			if ((j % (lut3d_points - 1)) == 0)
				pr_info("\n");
			pr_info("%d\t", P3dlut_regtab[i * lut3d_points + j]);
		}
	}
}

void set_gamma_regs(int en, int sel, int index)
{
	int i;
	int *gamma_lut = NULL;
	int offset = 0;

	static int gamma_lut_default[66] =  {
	0, 0, 0, 1, 2, 4, 6, 8, 11, 14, 17, 21, 26, 31,
	36, 42, 49, 55, 63, 71, 79, 88, 98, 108, 118,
	129, 141, 153, 166, 179, 193, 208, 223, 238,
	255, 271, 289, 307, 325, 344, 364, 384, 405,
	427, 449, 472, 495, 519, 544, 569, 595, 621,
	649, 676, 705, 734, 763, 794, 825, 856, 888,
	921, 955, 989, 1023, 0};

	static int gamma_lut_straight[66] =  {
	 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160,
	 176, 192, 208, 224, 240, 256, 272, 288, 304, 320,
	 336, 352, 368, 384, 400, 416, 432, 448, 464, 480,
	 496, 512, 528, 544, 560, 576, 592, 608, 624, 640,
	 656, 672, 688, 704, 720, 736, 752, 768, 784, 800,
	 816, 832, 848, 864, 880, 896, 912, 928, 944, 960,
	 976, 992, 1008, 1023, 0};

	if (!sel)
		gamma_lut = gamma_lut_default;
	else
		gamma_lut = gamma_lut_straight;

	if (index && chip_type_id == chip_a9)
		offset = 0x2ae1;

	if (en) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_s5 ||
			chip_type_id == chip_t3x) {
			post_pre_gamma_set(gamma_lut);
		} else
#endif
		{
			WRITE_VPP_REG(VPP_GAMMA_BIN_ADDR + offset, 0);
			for (i = 0; i < 33; i = i + 1)
				WRITE_VPP_REG(VPP_GAMMA_BIN_DATA + offset,
					      (((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
					      ((gamma_lut[i * 2] << 2) & 0xffff)));
			for (i = 0; i < 33; i = i + 1)
				WRITE_VPP_REG(VPP_GAMMA_BIN_DATA + offset,
					      (((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
					      ((gamma_lut[i * 2] << 2) & 0xffff)));
			for (i = 0; i < 33; i = i + 1)
				WRITE_VPP_REG(VPP_GAMMA_BIN_DATA + offset,
					      (((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
					      ((gamma_lut[i * 2] << 2) & 0xffff)));
			WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL + offset, 0x3, 0, 2);
		}
	}
}

void set_viu2_gamma_regs(int en, int sel)
{
	int i;
	int *gamma_lut = NULL;

	static int gamma_lut_default[66] =  {
	0, 0, 0, 1, 2, 4, 6, 8, 11, 14, 17, 21, 26, 31,
	36, 42, 49, 55, 63, 71, 79, 88, 98, 108, 118,
	129, 141, 153, 166, 179, 193, 208, 223, 238,
	255, 271, 289, 307, 325, 344, 364, 384, 405,
	427, 449, 472, 495, 519, 544, 569, 595, 621,
	649, 676, 705, 734, 763, 794, 825, 856, 888,
	921, 955, 989, 1023, 0};

	static int gamma_lut_straight[66] =  {
	 0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160,
	 176, 192, 208, 224, 240, 256, 272, 288, 304, 320,
	 336, 352, 368, 384, 400, 416, 432, 448, 464, 480,
	 496, 512, 528, 544, 560, 576, 592, 608, 624, 640,
	 656, 672, 688, 704, 720, 736, 752, 768, 784, 800,
	 816, 832, 848, 864, 880, 896, 912, 928, 944, 960,
	 976, 992, 1008, 1023, 0};

	if (!sel)
		gamma_lut = gamma_lut_default;
	else
		gamma_lut = gamma_lut_straight;

	if (en) {
		WRITE_VPP_REG_BITS(VPP2_MISC, 1, 17, 1);
		WRITE_VPP_REG_BITS(VPP2_GAMMA_CTRL, 1, 0, 1);
		WRITE_VPP_REG(VPP2_GAMMA_BIN_ADDR, 0);

		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(VPP2_GAMMA_BIN_DATA,
				(((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				((gamma_lut[i * 2] << 2) & 0xffff)));
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(VPP2_GAMMA_BIN_DATA,
				(((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				((gamma_lut[i * 2] << 2) & 0xffff)));
		for (i = 0; i < 33; i = i + 1)
			WRITE_VPP_REG(VPP2_GAMMA_BIN_DATA,
				(((gamma_lut[i * 2 + 1] << 2) & 0xffff) << 16 |
				((gamma_lut[i * 2] << 2) & 0xffff)));
	} else {
		WRITE_VPP_REG_BITS(VPP2_GAMMA_CTRL, 0, 0, 1);
	}
}

void amvecm_pre_gamma_enable(int enable)
{
	if (chip_type_id != chip_t3x)
		WRITE_VPP_REG_BITS(VPP_GAMMA_CTRL, enable, 0, 1);
	else
		post_pre_gamma_ctl(WR_VCB, enable, 0);
}

void set_pre_gamma_reg(struct pre_gamma_table_s *pre_gma_tb, int vpp_index)
{
	int i;
	int en;

	if (!pre_gma_tb)
		return;

	en = ((pre_gma_tb->en & 0x1) << 1) | (pre_gma_tb->en & 0x1);

	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_GAMMA_BIN_ADDR, 0, vpp_index);
	for (i = 0; i < 32; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_GAMMA_BIN_DATA,
				    (((pre_gma_tb->lut_r[i * 2 + 1] << 2) & 0xffff) << 16 |
				    ((pre_gma_tb->lut_r[i * 2] << 2) & 0xffff)), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_GAMMA_BIN_DATA,
		(pre_gma_tb->lut_r[64] << 2) & 0xffff, vpp_index);
	for (i = 0; i < 32; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_GAMMA_BIN_DATA,
				    (((pre_gma_tb->lut_g[i * 2 + 1] << 2) & 0xffff) << 16 |
				    ((pre_gma_tb->lut_g[i * 2] << 2) & 0xffff)),
				    vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_GAMMA_BIN_DATA,
		(pre_gma_tb->lut_g[64] << 2) & 0xffff, vpp_index);
	for (i = 0; i < 32; i++)
		VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_GAMMA_BIN_DATA,
				    (((pre_gma_tb->lut_b[i * 2 + 1] << 2) & 0xffff) << 16 |
				    ((pre_gma_tb->lut_b[i * 2] << 2) & 0xffff)),
				    vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_GAMMA_BIN_DATA,
		(pre_gma_tb->lut_b[64] << 2) & 0xffff, vpp_index);

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_GAMMA_CTRL, en, 0, 2, vpp_index);
}

void vpp_pst_hist_sta_config(int en,
	enum pst_hist_mod mod,
	enum pst_hist_pos pos,
	struct vinfo_s *vinfo)
{
	int cfg = 0;
	unsigned int height = 1079;
	unsigned int width = 1919;

	/*config*/
	cfg = (0 << 16) | /*rd index*/
		(0 << 8) | /*00: clk_en, 01: close clk, 1x: original clk*/
		(1 << 4) | /*pst_hist_window_en: 1: win_en*/
		(mod << 2) | /*pst hist model sel  2: y/r 1:U/G 0:V/B 3: max(y/r 1:U/G 0:V/B )*/
		(pos << 1) | /*pst hist after csc 1: rgb 0, yuv*/
		(en << 0);/*pst hist sta en  1: en*/
	WRITE_VPP_REG(VPP_PST_STA_CTRL, cfg);
	if (vinfo) {
		height = vinfo->height - 1;
		width = vinfo->width - 1;
	}
	WRITE_VPP_REG(VPP_PST_STA_WIN_X, width << 16);
	WRITE_VPP_REG(VPP_PST_STA_WIN_Y, height << 16);
}

void vpp_pst_hist_sta_read(unsigned int *hist)
{
	int i;

	WRITE_VPP_REG_BITS(VPP_PST_STA_CTRL, 0, 16, 16);
	for (i = 0; i < 64; i++)
		hist[i] = READ_VPP_REG(VPP_PST_STA_RO_HIST);
}

void set_sharpness_gain(int sr0_gain, int sr1_gain)
{
	sr_gain[0] = sr0_gain;
	sr_gain[1] = sr1_gain;

	vecm_latch_flag2 |= SHARPNESS_GAIN_UPDATE;
}

void sharpness_gain_update(int vpp_index)
{
	if (vecm_latch_flag2 & SHARPNESS_GAIN_UPDATE) {
		if (chip_type_id != chip_t3x) {
			if (chip_type_id == chip_s6 ||
				chip_type_id == chip_s7d ||
				chip_type_id == chip_t6w ||
				chip_type_id == chip_t6x ||
				chip_type_id == chip_a9) {
				VSYNC_WRITE_VPP_REG_BITS(VPP_PK_FINAL_GAIN,
					sr_gain[0], 0, 32);
			} else {
				VSYNC_WRITE_VPP_REG_BITS(SRSHARP0_PK_FINALGAIN_HP_BP,
					sr_gain[0], 0, 16);
				VSYNC_WRITE_VPP_REG_BITS(SRSHARP1_PK_FINALGAIN_HP_BP,
					sr_gain[0], 0, 16);
			}
		} else {
			ve_sharpness_gain_set(sr_gain[0], sr_gain[1],
				WR_DMA, vpp_index);
		}

		vecm_latch_flag2 &= ~SHARPNESS_GAIN_UPDATE;
	}
}

void amvecm_vadj_enable(enum vadj_index_e vadj_idx, int enable)
{
	if (chip_type_id == chip_t3x) {
		ve_vadj_ctl(WR_VCB, vadj_idx, enable, 0);
	} else {
		if (vadj_idx == VE_VADJ1)
			WRITE_VPP_REG_BITS(VPP_VADJ1_MISC, enable, 0, 1);
		else
			WRITE_VPP_REG_BITS(VPP_VADJ2_MISC, enable, 0, 1);
	}
}

struct pre_sat_data_s pre_sat_data;
void pre_saturation_gain_update(void)
{
	if (chip_type_id != chip_t6x)
		return;
	if (vecm_latch_flag2 & FLAG_PRE_SAT_UPDATE) {
		vecm_latch_flag2 &= ~FLAG_PRE_SAT_UPDATE;
		VSYNC_WRITE_VPP_REG_BITS(VPP_MISC2, pre_sat_data.enable, 28, 1);
		VSYNC_WRITE_VPP_REG_BITS(VPP_MISC2, pre_sat_data.gain, 16, 12);
	}
}

#endif

void amvecm_wb_enable(int enable)
{
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_G12B)
		return;

	if (enable) {
		wb_en = 1;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_a4 ||
			chip_type_id == chip_c5) {
			WRITE_VPP_REG_BITS(VOUT_GAINOFF_CTRL0, 1, 31, 1);
			return;
		}
#endif
		if (video_rgb_ogo_xvy_mtx) {
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 1, 6, 1);
		} else {
			if (chip_type_id != chip_t3x)
				WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 1, 31, 1);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			else
				post_wb_ctl(WR_VCB, 1, 0);
#endif
		}
	} else {
		wb_en = 0;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_a4 ||
			chip_type_id == chip_c5) {
			WRITE_VPP_REG_BITS(VOUT_GAINOFF_CTRL0, 0, 31, 1);
			return;
		}
#endif
		if (video_rgb_ogo_xvy_mtx) {
			WRITE_VPP_REG_BITS(VPP_MATRIX_CTRL, 0, 6, 1);
		} else {
			if (chip_type_id != chip_t3x)
				WRITE_VPP_REG_BITS(VPP_GAINOFF_CTRL0, 0, 31, 1);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			else
				post_wb_ctl(WR_VCB, 0, 0);
#endif
		}
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void amvecm_wb_enable_sub(int enable)
{
	if (enable)
		WRITE_VPP_REG_BITS(0x59a1, 1, 31, 1);
	else
		WRITE_VPP_REG_BITS(0x59a1, 0, 31, 1);
}

/*frequence meter init*/
void amve_fmeter_init(int enable)
{
	if (!enable)
		return;
	/*sr0 fmeter*/
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_CTRL, enable, 0, 1);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_CTRL, 0xe, 4, 4);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_CTRL, 0xa, 8, 4);
	WRITE_VPP_REG(SRSHARP0_FMETER_CORING, 0x04040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_RATIO_H, 0x00040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_RATIO_V, 0x00040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_RATIO_D, 0x00040404);
	WRITE_VPP_REG(SRSHARP0_FMETER_H_FILTER0_9TAP_0, 0xe11ddc24);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_H_FILTER0_9TAP_1, 0x14, 0, 10);
	WRITE_VPP_REG(SRSHARP0_FMETER_H_FILTER1_9TAP_0, 0x20f0ed26);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_H_FILTER1_9TAP_1, 0xf0, 0, 10);
	WRITE_VPP_REG(SRSHARP0_FMETER_H_FILTER2_9TAP_0, 0xe2e81932);
	WRITE_VPP_REG_BITS(SRSHARP0_FMETER_H_FILTER2_9TAP_1, 0x04, 0, 10);

	/*sr1 fmeter*/
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_CTRL, enable, 0, 1);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_CTRL, 0xe, 4, 4);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_CTRL, 0xa, 8, 4);
	WRITE_VPP_REG(SRSHARP1_FMETER_CORING, 0x04040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_RATIO_H, 0x00040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_RATIO_V, 0x00040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_RATIO_D, 0x00040404);
	WRITE_VPP_REG(SRSHARP1_FMETER_H_FILTER0_9TAP_0, 0xe11ddc24);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_H_FILTER0_9TAP_1, 0x14, 0, 10);
	WRITE_VPP_REG(SRSHARP1_FMETER_H_FILTER1_9TAP_0, 0x20f0ed26);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_H_FILTER1_9TAP_1, 0xf0, 0, 10);
	WRITE_VPP_REG(SRSHARP1_FMETER_H_FILTER2_9TAP_0, 0xe2e81932);
	WRITE_VPP_REG_BITS(SRSHARP1_FMETER_H_FILTER2_9TAP_1, 0x04, 0, 10);

	/*sr default value*/
	WRITE_VPP_REG(SRSHARP1_SR7_PKLONG_PF_GAIN, 0x40404040);
	WRITE_VPP_REG_BITS(SRSHARP1_PK_OS_STATIC, 0x14, 0, 10);
	WRITE_VPP_REG_BITS(SRSHARP1_PK_OS_STATIC, 0x14, 12, 10);

	fmeter_drv_param.fmeter_debug = &fmeter_debug;
	fmeter_drv_param.data_meter = &data_meter;
}

/*frequence meter size config*/
void amve_fmetersize_config(u32 sr0_w, u32 sr0_h, u32 sr1_w, u32 sr1_h, int vpp_index)
{
	static u32 pre_fm0_x_st, pre_fm0_y_end, pre_fm1_x_end, pre_fm1_y_end;
	u32 fm0_x_st, fm0_x_end, fm0_y_st, fm0_y_end;
	u32 fm1_x_st, fm1_x_end, fm1_y_st, fm1_y_end;

	fm0_x_st = 0;
	fm0_x_end = sr0_w & 0x1fff;
	fm0_y_st = 0;
	fm0_y_end = sr0_h & 0x1fff;

	fm1_x_st = 0;
	fm1_x_end = sr1_w & 0x1fff;
	fm1_y_st = 0;
	fm1_y_end = sr1_h & 0x1fff;

	if (fm0_x_end == pre_fm0_x_st &&
		fm0_y_end == pre_fm0_y_end &&
		fm1_x_end == pre_fm1_x_end &&
		fm1_y_end == pre_fm1_y_end)
		return;

	if (fmeter_en) {
		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_FMETER_WIN_HOR,
			fm0_x_st | fm0_x_end << 16, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP0_FMETER_WIN_VER,
			fm0_y_st | fm0_y_end << 16, vpp_index);

		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_FMETER_WIN_HOR,
			fm1_x_st | fm1_x_end << 16, vpp_index);
		VSYNC_WRITE_VPP_REG_VPP_SEL(SRSHARP1_FMETER_WIN_VER,
			fm1_y_st | fm1_y_end << 16, vpp_index);

		pre_fm0_x_st = fm0_x_end;
		pre_fm0_y_end = fm0_y_end;
		pre_fm1_x_end = fm1_x_end;
		pre_fm1_y_end = fm1_y_end;
	}
}
#endif

int vpp_pq_ctrl_config(struct pq_ctrl_s pq_cfg, enum wr_md_e md, int vpp_index)
{
	switch (md) {
	case WR_VCB:
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_a4 ||
			chip_type_id == chip_c5) {
			if (pq_cfg_cur.vadj2_en != pq_cfg.vadj2_en) {
				pq_cfg_cur.vadj2_en = pq_cfg.vadj2_en;
				WRITE_VPP_REG_BITS(VOUT_VADJ_MISC,
					pq_cfg.vadj2_en, 0, 1);
			}
			if (pq_cfg_cur.wb_en != pq_cfg.wb_en) {
				pq_cfg_cur.wb_en = pq_cfg.wb_en;
				amvecm_wb_enable(pq_cfg.wb_en);
			}
			if (pq_cfg_cur.gamma_en != pq_cfg.gamma_en) {
				pq_cfg_cur.gamma_en = pq_cfg.gamma_en;
				gamma_en = pq_cfg.gamma_en;
				if (gamma_en)
					vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
				else
					vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
			}
			return 0;
		}

		if (pq_cfg_cur.dnlp_en != pq_cfg.dnlp_en) {
			pq_cfg_cur.dnlp_en = pq_cfg.dnlp_en;
			if (pq_cfg.dnlp_en) {
				ve_enable_dnlp();
				dnlp_en = 1;
			} else {
				ve_disable_dnlp();
				dnlp_en = 0;
			}
		}
		if (pq_cfg_cur.cm_en != pq_cfg.cm_en) {
			pq_cfg_cur.cm_en = pq_cfg.cm_en;
			if (pq_cfg.cm_en) {
				if (!cm_en) {
					amcm_enable(WR_VCB, 0);
					cm_en = 1;
				}
			} else {
				amcm_disable(WR_VCB, 0);
				cm_en = 0;
			}
		}

		if (chip_type_id == chip_s5 ||
			chip_type_id == chip_t3x) {
			if (pq_cfg_cur.vadj1_en != pq_cfg.vadj1_en) {
				pq_cfg_cur.vadj1_en = pq_cfg.vadj1_en;
				ve_vadj_ctl(md, VE_VADJ1, pq_cfg.vadj1_en, vpp_index);
			}
			if (pq_cfg_cur.vadj2_en != pq_cfg.vadj2_en) {
				pq_cfg_cur.vadj2_en = pq_cfg.vadj2_en;
				ve_vadj_ctl(md, VE_VADJ2, pq_cfg.vadj2_en, vpp_index);
			}
			ve_bs_ctl(md, 0, vpp_index);
			if (pq_cfg_cur.black_ext_en != pq_cfg.black_ext_en) {
				pq_cfg_cur.black_ext_en = pq_cfg.black_ext_en;
				ve_ble_ctl(md, pq_cfg.black_ext_en, vpp_index);
			}
			if (pq_cfg_cur.chroma_cor_en != pq_cfg.chroma_cor_en) {
				pq_cfg_cur.chroma_cor_en = pq_cfg.chroma_cor_en;
				ve_cc_ctl(md, pq_cfg.chroma_cor_en, vpp_index);
			}
			if (pq_cfg_cur.wb_en != pq_cfg.wb_en &&
				chip_type_id != chip_t3x) {
				pq_cfg_cur.wb_en = pq_cfg.wb_en;
				post_wb_ctl(md, pq_cfg.wb_en, vpp_index);
			}

			if (chip_type_id == chip_t3x) {
				wb_en = pq_cfg.wb_en;
				pr_amve_dbg("\n[%s] WR_VCB gamma_en/pq_cfg.gamma_en = %d/%d\n",
					__func__, gamma_en, pq_cfg.gamma_en);
//				if (pq_cfg_cur.gamma_en != pq_cfg.gamma_en) {
//					pq_cfg_cur.gamma_en = pq_cfg.gamma_en;
//					gamma_en = pq_cfg.gamma_en;
//					if (gamma_en)
//						vpp_enable_lcd_gamma_table(0, 0, vpp_index);
//					else
//						vpp_disable_lcd_gamma_table(0, 0, vpp_index);
//				}
				if (pq_cfg_cur.lc_en != pq_cfg.lc_en) {
					pq_cfg_cur.lc_en = pq_cfg.lc_en;
					if (pq_cfg.lc_en)
						lc_en = 1;
					else
						lc_en = 0;
				}
				if (pq_cfg_cur.sharpness0_en != pq_cfg.sharpness0_en ||
					pq_cfg_cur.sharpness1_en != pq_cfg.sharpness1_en) {
					pq_cfg_cur.sharpness0_en = pq_cfg.sharpness0_en;
					pq_cfg_cur.sharpness1_en = pq_cfg.sharpness1_en;
					ve_sharpness_ctl(md, pq_cfg.sharpness0_en,
						pq_cfg.sharpness1_en, vpp_index);
				}
			} else {
				if (pq_cfg_cur.sharpness0_en != pq_cfg.sharpness0_en) {
					pq_cfg_cur.sharpness0_en = pq_cfg.sharpness0_en;
					WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
						pq_cfg.sharpness0_en, 1, 1);
				}
				if (pq_cfg_cur.sharpness1_en != pq_cfg.sharpness1_en) {
					pq_cfg_cur.sharpness1_en = pq_cfg.sharpness1_en;
					WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
						pq_cfg.sharpness1_en, 1, 1);
				}
			}
		} else {
			if (pq_cfg_cur.sharpness0_en != pq_cfg.sharpness0_en) {
				pq_cfg_cur.sharpness0_en = pq_cfg.sharpness0_en;
				if (chip_type_id == chip_t6d || chip_type_id == chip_s6 ||
					chip_type_id == chip_s7d || chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x || chip_type_id == chip_a9)
					WRITE_VPP_REG_BITS(VPP_SR_EN,
						pq_cfg.sharpness0_en, 0, 1);
				else
					WRITE_VPP_REG_BITS(SRSHARP0_PK_NR_ENABLE,
						pq_cfg.sharpness0_en, 1, 1);
			}
			if (pq_cfg_cur.sharpness1_en != pq_cfg.sharpness1_en) {
				pq_cfg_cur.sharpness1_en = pq_cfg.sharpness1_en;
				if (chip_type_id != chip_txhd2)
					WRITE_VPP_REG_BITS(SRSHARP1_PK_NR_ENABLE,
						pq_cfg.sharpness1_en, 1, 1);
			}
#endif
			if (pq_cfg_cur.vadj1_en != pq_cfg.vadj1_en) {
				pq_cfg_cur.vadj1_en = pq_cfg.vadj1_en;
				if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
					WRITE_VPP_REG_BITS(VPP_VADJ1_MISC,
						pq_cfg.vadj1_en, 0, 1);
				else
					WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
						pq_cfg.vadj1_en, 0, 1);
			}

			if (pq_cfg_cur.vd1_ctrst_en != pq_cfg.vd1_ctrst_en) {
				pq_cfg_cur.vd1_ctrst_en = pq_cfg.vd1_ctrst_en;
				WRITE_VPP_REG_BITS(VPP_VD1_RGB_CTRST,
					pq_cfg.vd1_ctrst_en, 1, 1);
			}

			if (pq_cfg_cur.vadj2_en != pq_cfg.vadj2_en) {
				pq_cfg_cur.vadj2_en = pq_cfg.vadj2_en;
				if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
					WRITE_VPP_REG_BITS(VPP_VADJ2_MISC,
						pq_cfg.vadj2_en, 0, 1);
				else
					WRITE_VPP_REG_BITS(VPP_VADJ_CTRL,
						pq_cfg.vadj2_en, 2, 1);
			}
			if (pq_cfg_cur.post_ctrst_en != pq_cfg.post_ctrst_en) {
				pq_cfg_cur.post_ctrst_en = pq_cfg.post_ctrst_en;
				WRITE_VPP_REG_BITS(VPP_POST_RGB_CTRST,
					pq_cfg.post_ctrst_en, 1, 1);
			}
			if (pq_cfg_cur.wb_en != pq_cfg.wb_en) {
				pq_cfg_cur.wb_en = pq_cfg.wb_en;
				amvecm_wb_enable(pq_cfg.wb_en);
			}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			if (pq_cfg_cur.gamma_en != pq_cfg.gamma_en) {
				pq_cfg_cur.gamma_en = pq_cfg.gamma_en;
				gamma_en = pq_cfg.gamma_en;
				if (gamma_en)
					vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
				else
					vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
			}
			if (pq_cfg_cur.lc_en != pq_cfg.lc_en) {
				pq_cfg_cur.lc_en = pq_cfg.lc_en;
				if (pq_cfg.lc_en) {
					lc_en = 1;
				} else {
					lc_en = 0;
					if (is_meson_tl1_cpu() ||
						is_meson_tm2_cpu())
						lc_disable(0, vpp_index);
				}
			}
			if (!slt_en) {
				if (pq_cfg_cur.black_ext_en != pq_cfg.black_ext_en) {
					pq_cfg_cur.black_ext_en = pq_cfg.black_ext_en;
					WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
						pq_cfg.black_ext_en, 3, 1);
				}
				if (pq_cfg_cur.chroma_cor_en != pq_cfg.chroma_cor_en) {
					pq_cfg_cur.chroma_cor_en = pq_cfg.chroma_cor_en;
					WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
						pq_cfg.chroma_cor_en, 4, 1);
				}
			}
			/*blue stretch*/
			if (chip_type_id == chip_txhd2)
				WRITE_VPP_REG_BITS(VPP_VE_ENABLE_CTRL,
					0, 0, 1);
		}
#endif
		break;
	case WR_DMA:
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (chip_type_id == chip_a4 ||
			chip_type_id == chip_c5) {
			if (pq_cfg_cur.vadj2_en != pq_cfg.vadj2_en) {
				pq_cfg_cur.vadj2_en = pq_cfg.vadj2_en;
				VSYNC_WRITE_VPP_REG_BITS(VOUT_VADJ_MISC,
					pq_cfg.vadj2_en, 0, 1);
			}
			if (pq_cfg_cur.wb_en != pq_cfg.wb_en) {
				pq_cfg_cur.wb_en = pq_cfg.wb_en;
				amvecm_wb_enable(pq_cfg.wb_en);
			}
			if (pq_cfg_cur.gamma_en != pq_cfg.gamma_en) {
				pq_cfg_cur.gamma_en = pq_cfg.gamma_en;
				gamma_en = pq_cfg.gamma_en;
				if (gamma_en)
					vpp_enable_lcd_gamma_table(0, 1, vpp_index);
				else
					vpp_disable_lcd_gamma_table(0, 1, vpp_index);
			}
			return 0;
		}

		if (pq_cfg_cur.dnlp_en != pq_cfg.dnlp_en) {
			pq_cfg_cur.dnlp_en = pq_cfg.dnlp_en;
			if (pq_cfg.dnlp_en) {
				ve_enable_dnlp();
				dnlp_en = 1;
			} else {
				ve_disable_dnlp();
				dnlp_en = 0;
			}
		}

		if (pq_cfg_cur.cm_en != pq_cfg.cm_en) {
			pq_cfg_cur.cm_en = pq_cfg.cm_en;
			if (pq_cfg.cm_en) {
				amcm_enable(WR_DMA, vpp_index);
				cm_en = 1;
			} else {
				amcm_disable(WR_DMA, vpp_index);
				cm_en = 0;
			}
		}

		if (chip_type_id == chip_s5 ||
			chip_type_id == chip_t3x) {
			if (pq_cfg_cur.vadj1_en != pq_cfg.vadj1_en) {
				pq_cfg_cur.vadj1_en = pq_cfg.vadj1_en;
				ve_vadj_ctl(md, VE_VADJ1, pq_cfg.vadj1_en, vpp_index);
			}
			if (pq_cfg_cur.vadj2_en != pq_cfg.vadj2_en) {
				pq_cfg_cur.vadj2_en = pq_cfg.vadj2_en;
				ve_vadj_ctl(md, VE_VADJ2, pq_cfg.vadj2_en, vpp_index);
			}
			ve_bs_ctl(md, 0, vpp_index);
			if (pq_cfg_cur.black_ext_en != pq_cfg.black_ext_en) {
				pq_cfg_cur.black_ext_en = pq_cfg.black_ext_en;
				ve_ble_ctl(md, pq_cfg.black_ext_en, vpp_index);
			}
			if (pq_cfg_cur.chroma_cor_en != pq_cfg.chroma_cor_en) {
				pq_cfg_cur.chroma_cor_en = pq_cfg.chroma_cor_en;
				ve_cc_ctl(md, pq_cfg.chroma_cor_en, vpp_index);
			}
			if (pq_cfg_cur.wb_en != pq_cfg.wb_en) {
				pq_cfg_cur.wb_en = pq_cfg.wb_en;
				post_wb_ctl(md, pq_cfg.wb_en, vpp_index);
			}

			if (chip_type_id == chip_t3x) {
				wb_en = pq_cfg.wb_en;
				if (pq_cfg_cur.gamma_en != pq_cfg.gamma_en) {
					pq_cfg_cur.gamma_en = pq_cfg.gamma_en;
					gamma_en = pq_cfg.gamma_en;
					if (gamma_en)
						vpp_enable_lcd_gamma_table(0, 1, vpp_index);
					else
						vpp_disable_lcd_gamma_table(0, 1, vpp_index);
				}
				if (pq_cfg_cur.lc_en != pq_cfg.lc_en) {
					pq_cfg_cur.lc_en = pq_cfg.lc_en;
					if (pq_cfg.lc_en)
						lc_en = 1;
					else
						lc_en = 0;
				}
				if (pq_cfg_cur.sharpness0_en != pq_cfg.sharpness0_en ||
					pq_cfg_cur.sharpness1_en != pq_cfg.sharpness1_en) {
					pq_cfg_cur.sharpness0_en = pq_cfg.sharpness0_en;
					pq_cfg_cur.sharpness1_en = pq_cfg.sharpness1_en;
					ve_sharpness_ctl(md, pq_cfg.sharpness0_en,
						pq_cfg.sharpness1_en, vpp_index);
				}
			} else {
				if (pq_cfg_cur.sharpness0_en != pq_cfg.sharpness0_en) {
					pq_cfg_cur.sharpness0_en = pq_cfg.sharpness0_en;
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_PK_NR_ENABLE,
						pq_cfg.sharpness0_en, 1, 1, vpp_index);
				}
				if (pq_cfg_cur.sharpness1_en != pq_cfg.sharpness1_en) {
					pq_cfg_cur.sharpness1_en = pq_cfg.sharpness1_en;
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_PK_NR_ENABLE,
						pq_cfg.sharpness1_en, 1, 1, vpp_index);
				}
			}
		} else {
			if (pq_cfg_cur.sharpness0_en != pq_cfg.sharpness0_en) {
				pq_cfg_cur.sharpness0_en = pq_cfg.sharpness0_en;
				if (chip_type_id == chip_t6d || chip_type_id == chip_s6 ||
					chip_type_id == chip_s7d || chip_type_id == chip_t6w ||
					chip_type_id == chip_t6x || chip_type_id == chip_a9)
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SR_EN,
						pq_cfg.sharpness0_en, 0, 1, vpp_index);
				else
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_PK_NR_ENABLE,
						pq_cfg.sharpness0_en, 1, 1, vpp_index);
			}
			if (pq_cfg_cur.sharpness1_en != pq_cfg.sharpness1_en) {
				pq_cfg_cur.sharpness1_en = pq_cfg.sharpness1_en;
				if (chip_type_id != chip_txhd2)
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_PK_NR_ENABLE,
						pq_cfg.sharpness1_en, 1, 1, vpp_index);
			}
#endif
			if (pq_cfg_cur.vadj1_en != pq_cfg.vadj1_en) {
				pq_cfg_cur.vadj1_en = pq_cfg.vadj1_en;
				if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ1_MISC,
						pq_cfg.vadj1_en, 0, 1, vpp_index);
				else
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL,
						pq_cfg.vadj1_en, 0, 1, vpp_index);
			}
			if (pq_cfg_cur.vd1_ctrst_en != pq_cfg.vd1_ctrst_en) {
				pq_cfg_cur.vd1_ctrst_en = pq_cfg.vd1_ctrst_en;
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VD1_RGB_CTRST,
					pq_cfg.vd1_ctrst_en, 1, 1, vpp_index);
			}
			if (pq_cfg_cur.vadj2_en != pq_cfg.vadj2_en) {
				pq_cfg_cur.vadj2_en = pq_cfg.vadj2_en;
				if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ2_MISC,
						pq_cfg.vadj2_en, 0, 1, vpp_index);
				else
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VADJ_CTRL,
						pq_cfg.vadj2_en, 2, 1, vpp_index);
			}
			if (pq_cfg_cur.post_ctrst_en != pq_cfg.post_ctrst_en) {
				pq_cfg_cur.post_ctrst_en = pq_cfg.post_ctrst_en;
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_POST_RGB_CTRST,
					pq_cfg.post_ctrst_en, 1, 1, vpp_index);
			}
			if (pq_cfg_cur.wb_en != pq_cfg.wb_en) {
				pq_cfg_cur.wb_en = pq_cfg.wb_en;
				amvecm_wb_enable(pq_cfg.wb_en);
			}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			if (pq_cfg_cur.gamma_en != pq_cfg.gamma_en) {
				pq_cfg_cur.gamma_en = pq_cfg.gamma_en;
				gamma_en = pq_cfg.gamma_en;
				if (gamma_en) {
					if (is_meson_t7_cpu())
						vecm_latch_flag |= FLAG_GAMMA_TABLE_EN;
					else
						vpp_enable_lcd_gamma_table(0, 1, vpp_index);
				} else {
					if (is_meson_t7_cpu())
						vecm_latch_flag |= FLAG_GAMMA_TABLE_DIS;
					else
						vpp_disable_lcd_gamma_table(0, 1, vpp_index);
				}
			}
			if (pq_cfg_cur.lc_en != pq_cfg.lc_en) {
				pq_cfg_cur.lc_en = pq_cfg.lc_en;
				if (pq_cfg.lc_en) {
					lc_en = 1;
				} else {
					lc_en = 0;
					if (is_meson_tl1_cpu() ||
						is_meson_tm2_cpu())
						lc_disable(0, vpp_index);
				}
			}
			if (!slt_en) {
				if (pq_cfg_cur.black_ext_en != pq_cfg.black_ext_en) {
					pq_cfg_cur.black_ext_en = pq_cfg.black_ext_en;
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VE_ENABLE_CTRL,
						pq_cfg.black_ext_en, 3, 1, vpp_index);
				}
				if (pq_cfg_cur.chroma_cor_en != pq_cfg.chroma_cor_en) {
					pq_cfg_cur.chroma_cor_en = pq_cfg.chroma_cor_en;
					VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VE_ENABLE_CTRL,
						pq_cfg.chroma_cor_en, 4, 1, vpp_index);
				}
			}
			/*blue stretch*/
			if (chip_type_id == chip_txhd2)
				VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VE_ENABLE_CTRL,
					0, 0, 1, vpp_index);
		}
#endif
		break;
	default:
		break;
	}

	return 0;
}

unsigned int skip_pq_ctrl_load(struct am_reg_s *p)
{
	unsigned int ret = 0;
	struct pq_ctrl_s cfg;

	if (dv_pq_bypass == 3) {
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.vadj1_en = pq_cfg.vadj1_en;
	} else if (dv_pq_bypass == 2) {
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.sharpness0_en = pq_cfg.sharpness0_en;
		cfg.sharpness1_en = pq_cfg.sharpness1_en;
	} else if (dv_pq_bypass == 1) {
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
	} else {
		memcpy(&cfg, &pq_cfg, sizeof(struct pq_ctrl_s));
	}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (!cfg.sharpness0_en) {
		if (p->addr == (SRSHARP0_PK_NR_ENABLE)) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (!cfg.sharpness1_en) {
		if (p->addr == (SRSHARP1_PK_NR_ENABLE)) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (!cfg.cm_en) {
		if (p->addr == 0x208) {
			if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A)
				ret |= 1 << 0;
			else
				ret |= 1 << 1;
			return ret;
		}
	}
#endif

	if (!cfg.vadj1_en) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			if (p->addr == VPP_VADJ1_MISC) {
				ret |= 1 << 0;
				return ret;
			}
		} else {
			if (p->addr == VPP_VADJ_CTRL) {
				ret |= 1 << 0;
				return ret;
			}
		}
	}

	if (!cfg.vd1_ctrst_en) {
		if (p->addr == VPP_VD1_RGB_CTRST) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (!cfg.vadj2_en) {
		if (get_cpu_type() >= MESON_CPU_MAJOR_ID_G12A) {
			if (p->addr == VPP_VADJ2_MISC) {
				ret |= 1 << 0;
				return ret;
			}
		} else {
			if (p->addr == VPP_VADJ2_Y) {
				ret |= 1 << 0;
				return ret;
			}
		}
	}

	if (!cfg.post_ctrst_en) {
		if (p->addr == VPP_POST_RGB_CTRST) {
			ret |= 1 << 1;
			return ret;
		}
	}

	if (p->addr == VPP_VE_ENABLE_CTRL) {
		if (!cfg.black_ext_en)
			ret |= 1 << 3;
		if (!cfg.chroma_cor_en)
			ret |= 1 << 4;
		return ret;
	}

	return ret;
}

int dv_pq_ctl(enum dv_pq_ctl_e ctl)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	struct pq_ctrl_s cfg;
	int vpp_index = 0;

	if (chip_type_id == chip_t3x || pq_bypass_debug_flag)
		return 0;

	switch (ctl) {
	case DV_PQ_TV_BYPASS:
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.vadj1_en = pq_cfg.vadj1_en;
		cfg.cm_en = pq_cfg.cm_en;
		vpp_pq_ctrl_config(cfg, WR_DMA, vpp_index);
		dv_pq_bypass = 3;
		pr_amve_dbg("dv enable, for TV pq disable, dv_pq_bypass = %d\n",
			    dv_pq_bypass);
		break;
	case DV_PQ_STB_BYPASS:
		cfg.sharpness0_en = pq_cfg.sharpness0_en;
		cfg.sharpness1_en = pq_cfg.sharpness1_en;
		cfg.cm_en = pq_cfg.cm_en;
		cfg.dnlp_en = dv_cfg_bypass.dnlp_en;
		cfg.cm_en = dv_cfg_bypass.cm_en;
		cfg.vadj1_en = pq_cfg.vadj1_en;
		cfg.vd1_ctrst_en = dv_cfg_bypass.vd1_ctrst_en;
		cfg.vadj2_en = pq_cfg.vadj2_en;
		cfg.post_ctrst_en = dv_cfg_bypass.post_ctrst_en;
		cfg.wb_en = dv_cfg_bypass.wb_en;
		cfg.gamma_en = dv_cfg_bypass.gamma_en;
		cfg.lc_en = dv_cfg_bypass.lc_en;
		cfg.black_ext_en = dv_cfg_bypass.black_ext_en;
		cfg.chroma_cor_en = dv_cfg_bypass.chroma_cor_en;
		memcpy(&pq_cfg_cur, &pq_cfg_init[INIT_CUR_CFG],
			sizeof(struct pq_ctrl_s));
		vpp_pq_ctrl_config(cfg, WR_DMA, vpp_index);
		dv_pq_bypass = 2;
		pr_amve_dbg("dv enable, for STB pq disable, dv_pq_bypass = %d\n",
			dv_pq_bypass);
		break;
	case DV_PQ_CERT:
		memcpy(&cfg, &dv_cfg_bypass, sizeof(struct pq_ctrl_s));
		cfg.cm_en = pq_cfg.cm_en;
		vpp_pq_ctrl_config(cfg, WR_DMA, vpp_index);
		dv_pq_bypass = 1;
		pr_amve_dbg("dv certification mode, pq disable, dv_pq_bypass = %d\n",
			    dv_pq_bypass);
		break;
	case DV_PQ_REC:
		vpp_pq_ctrl_config(pq_cfg, WR_DMA, vpp_index);
		//vecm_latch_flag2 |= VPP_EYE_PROTECT_UPDATE;
		dv_pq_bypass = 0;
		pr_amve_dbg("dv disable, pq recovery, dv_pq_bypass = %d\n",
			    dv_pq_bypass);
		break;
	case DV_PQ_EP_IPT:
		if (!ep_ipt_flag) {
			vecm_latch_flag2 |= VPP_EYE_PROTECT_UPDATE;
			pr_amve_dbg("dv enable, set ipt out eye protect for stb\n");
		}
		ep_ipt_flag = 1;
		break;
	case DV_PQ_EP_YUV:
		if (ep_ipt_flag) {
			vecm_latch_flag2 |= VPP_EYE_PROTECT_UPDATE;
			pr_amve_dbg("dv enable, set yuv out eye protect for stb\n");
		}
		ep_ipt_flag = 0;
		break;

	default:
		break;
	}
#endif
	return 0;
}
EXPORT_SYMBOL(dv_pq_ctl);

int get_ep_ipt_flag(void)
{
	return ep_ipt_flag;
}

int mtx_mul_mtx(int (*mtx_a)[3], int (*mtx_b)[3], int (*mtx_out)[3])
{
	int i, j, k;

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++) {
			mtx_out[i][j] = 0;
			for (k = 0; k < 3; k++)
				mtx_out[i][j] += mtx_a[i][k] * mtx_b[k][j];
		}
	}

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			mtx_out[i][j] = (mtx_out[i][j] + (1 << 9)) >> 10;
	}

	return 0;
}

static int mtx_rgbto709l[3][3] = {
	{187, 629, 63},
	{-103, -346, 450},
	{450, -409, -41},
};

static int mtx_709ltorgb[3][3] = {
	{1192, 0, 1836},
	{1192, -218, -547},
	{1192, 2166, 0},
};

static int mtx_rgbto2020l[3][3] = {
	{230, 593, 51},
	{-125, -322, 448},
	{448, -411, -36},
};

static int mtx_2020ltorgb[3][3] = {
	{1197, 0, 1725},
	{1197, -192, -668},
	{1197, 2063, 0},
};

int mtx_multi(int mtx_ep[][4], int (*mtx_out)[3], enum ep_mode_e mode)
{
	int i, j;
	int mtx_rgb[3][3] = {0};
	int mtx_in[3][3] = {0};
	int mtx_torgb[3][3];
	int mtx_rgbto[3][3];

	if (mode == IPT_MOD) {
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				mtx_torgb[i][j] = mtx_2020ltorgb[i][j];
				mtx_rgbto[i][j] = mtx_rgbto2020l[i][j];
			}
		}
	} else {
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 3; j++) {
				mtx_torgb[i][j] = mtx_709ltorgb[i][j];
				mtx_rgbto[i][j] = mtx_rgbto709l[i][j];
			}
		}
	}

	for (i = 0; i < 4; i++) {
		for (j = 0; j < 4; j++)
			pr_amve_dbg("mtx_in[%d][%d] = %d\n",
			i, j, mtx_ep[i][j]);
	}

	/* because use only one matrix, so Tr/Tg/Tb can not be used,
	 * if add Tr/Tg/Tb in RGB out, need two matrix to workaround
	 */
	for (i = 0; i < 3; i++)
		for (j = 0; j < 3; j++)
			mtx_in[i][j] = mtx_ep[j][i];

	mtx_mul_mtx(mtx_in, mtx_torgb, mtx_rgb);
	mtx_mul_mtx(mtx_rgbto, mtx_rgb, mtx_out);

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 3; j++)
			pr_amve_dbg("mtx_out[%d][%d] = 0x%x\n",
			i, j, mtx_out[i][j]);
	}

	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void eye_proc(int mtx_ep[][4], int mtx_on, int vpp_index)
{
	unsigned int matrix_coef00_01 = 0;
	unsigned int matrix_coef02_10 = 0;
	unsigned int matrix_coef11_12 = 0;
	unsigned int matrix_coef20_21 = 0;
	unsigned int matrix_coef22 = 0;
	unsigned int matrix_coef13_14 = 0;
	unsigned int matrix_coef23_24 = 0;
	unsigned int matrix_coef15_25 = 0;
	unsigned int matrix_clip = 0;
	unsigned int matrix_offset0_1 = 0;
	unsigned int matrix_offset2 = 0;
	unsigned int matrix_pre_offset0_1 = 0;
	unsigned int matrix_pre_offset2 = 0;
	unsigned int matrix_en_ctrl = 0;
	unsigned int matrix2_en_ctrl = 0;
	int mtx_out[3][3] = {0};
	int pre_offset[3] = {
		-64, -512, -512
	};
	int offset[3] = {
		64, 512, 512
	};
	int i, j = 0;

	if (chip_type_id != chip_t7 && vinfo_lcd_support())
		return;

	pr_amve_dbg("ep_ipt_flag = %d, mtx_on= %d\n",
		ep_ipt_flag, mtx_on);

	if (ep_ipt_flag) {
		matrix_coef00_01 = VPP_POST2_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST2_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST2_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST2_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST2_MATRIX_COEF22;
		matrix_coef13_14 = VPP_POST2_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_POST2_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_POST2_MATRIX_COEF15_25;
		matrix_clip = VPP_POST2_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST2_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST2_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST2_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST2_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;
		matrix2_en_ctrl = VPP_POST2_MATRIX_EN_CTRL;
	}  else {
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST_MATRIX_COEF22;
		matrix_coef13_14 = VPP_POST_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_POST_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_POST_MATRIX_COEF15_25;
		matrix_clip = VPP_POST_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;
		matrix2_en_ctrl = VPP_POST2_MATRIX_EN_CTRL;
	}

	if (chip_type_id == chip_t7) {
		matrix_coef00_01 = VPP_POST_MATRIX_COEF00_01;
		matrix_coef02_10 = VPP_POST_MATRIX_COEF02_10;
		matrix_coef11_12 = VPP_POST_MATRIX_COEF11_12;
		matrix_coef20_21 = VPP_POST_MATRIX_COEF20_21;
		matrix_coef22 = VPP_POST_MATRIX_COEF22;
		matrix_coef13_14 = VPP_POST_MATRIX_COEF13_14;
		matrix_coef23_24 = VPP_POST_MATRIX_COEF23_24;
		matrix_coef15_25 = VPP_POST_MATRIX_COEF15_25;
		matrix_clip = VPP_POST_MATRIX_CLIP;
		matrix_offset0_1 = VPP_POST_MATRIX_OFFSET0_1;
		matrix_offset2 = VPP_POST_MATRIX_OFFSET2;
		matrix_pre_offset0_1 = VPP_POST_MATRIX_PRE_OFFSET0_1;
		matrix_pre_offset2 = VPP_POST_MATRIX_PRE_OFFSET2;
		matrix_en_ctrl = VPP_POST_MATRIX_EN_CTRL;

		for (i = 0; i < 3; i++) {
			pre_offset[i] = 0;
			offset[i] = 0;
			for (j = 0; j < 3; j++)
				mtx_out[i][j] = mtx_ep[i][j];
		}
		VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_en_ctrl, mtx_on & 0x1, vpp_index);
	} else {
		if (ep_ipt_flag) {
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix2_en_ctrl, mtx_on & 0x1, vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_en_ctrl, 0, vpp_index);
		} else {
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix2_en_ctrl, 0, vpp_index);
			VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_en_ctrl, mtx_on & 0x1, vpp_index);
		}

		if (!mtx_on)
			return;

		if (ep_ipt_flag)
			mtx_multi(mtx_ep, mtx_out, IPT_MOD);
		else
			mtx_multi(mtx_ep, mtx_out, YUV_MOD);
	}

	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef00_01,
		((mtx_out[0][0] & 0x1fff) << 16) | (mtx_out[0][1] & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef02_10,
		((mtx_out[0][2] & 0x1fff) << 16) | (mtx_out[1][0] & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef11_12,
		((mtx_out[1][1] & 0x1fff) << 16) | (mtx_out[1][2] & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef20_21,
		((mtx_out[2][0] & 0x1fff) << 16) | (mtx_out[2][1] & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_coef22, (mtx_out[2][2] & 0x1fff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset0_1,
		((offset[0] & 0x7ff) << 16) | (offset[1] & 0x7ff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_offset2, (offset[2] & 0x7ff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset0_1,
		((pre_offset[0] & 0x7ff) << 16) | (pre_offset[1] & 0x7ff), vpp_index);
	VSYNC_WRITE_VPP_REG_VPP_SEL(matrix_pre_offset2, (pre_offset[2] & 0x7ff), vpp_index);
}
#endif

/*t3 for power consumption, disable clock
 *modules only before post blend can be disable
 *because after postblend it used for color temperature
 *or color correction
 */
static void vpp_enhence_clk_ctl(int en, int vpp_index)
{
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_GCLK_CTRL0, en, 6, 2, vpp_index); /*blue stretch*/
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_GCLK_CTRL1,
		(en | (en << 2)), 0, 4, vpp_index); /*cm*/
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VE_ENABLE_CTRL,
		(en | (en << 2)), 24, 4, vpp_index); /*ble, chroma coring*/
}

void set_vpp_enh_clk(struct vframe_s *vf, struct vframe_s *rpt_vf, int vpp_index)
{
	/*cm / blue stretch/ black extension/ chroma coring*/
	/*other modules(sr0/sr1/dnlp/lc disable in amvideo)*/
	/*en: 1 disable clock,  0: enable clock*/
	static enum pw_state_e pre_state = PW_MAX;
	enum pw_state_e pw_state = PW_MAX;

	if (get_cpu_type() < MESON_CPU_MAJOR_ID_T3)
		return;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_s5 ||
		chip_type_id == chip_t3x)
		return;
#endif

	if (vf || rpt_vf)
		pw_state = PW_ON;
	else
		pw_state = PW_OFF;

	switch (pw_state) {
	case PW_ON:
		if (pre_state != PW_ON) {
			vpp_enhence_clk_ctl(PW_ON, vpp_index);
			pre_state = pw_state;
			pr_amve_dbg("PW_ON: pre_state: %d\n", pre_state);
		}
		break;
	case PW_OFF:
		if (pre_state != PW_OFF) {
			vpp_enhence_clk_ctl(PW_ON, vpp_index);
			pre_state = pw_state;
			pr_amve_dbg("PW_OFF: pre_state: %d\n", pre_state);
		}
		break;
	//case PW_MAX:
	/*
	 * missing the default will caused build error
	 */
	/* coverity[dead_error_begin:SUPPRESS] */
	default:
		pre_state = PW_MAX;
		break;
	}
}

void s7d_sharpness_init(void)
{
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_0_0, 0x30100800);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_0_1, 0x3c3c3838);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_1_0, 0x30100000);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_1_1, 0x3c3c3c38);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_2_0, 0x10000000);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_2_1, 0x38383830);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_3_0, 0x08000000);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_3_1, 0x38383018);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_4_0, 0x00000000);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_4_1, 0x30302010);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_5_0, 0x00000000);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_5_1, 0x20100400);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_6_0, 0x00000000);
	WRITE_VPP_REG(VPP_SR_DIR_MIMAXERR2_LUT_6_1, 0x10080000);
	WRITE_VPP_REG(VPP_HCTI_BP_3TAP_COEF, 0x0000c07f);
	WRITE_VPP_REG(VPP_SR_EN, 0x1);

//	WRITE_VPP_REG(VPP_PK_OS_ADP_LUT_0, 0x0a080604);/*52d5-52d7 */
//	WRITE_VPP_REG(VPP_PK_OS_ADP_LUT_1, 0x06080a0c);
//	WRITE_VPP_REG(VPP_PK_OS_ADP_LUT_F, 0x00000004);
	WRITE_VPP_REG(VPP_PI_LUMA_GAIN_1, 0x0810183f);/*5080 */

	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_0_0, 0x30303038);/*5061-5069*/
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_0_0, 0x301a0c04);
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_0_0, 0x00000000);
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_1_0, 0x30303038);
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_1_0, 0x301c1008);
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_1_0, 0x00000000);
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_0_2_0, 0x38303038);
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_1_2_0, 0x3024180c);
	WRITE_VPP_REG(VPP_PI_MAXSAD_GAMMA_LUT2D_2_2_0, 0x00000004);

	WRITE_VPP_REG(VPP_PI_GLB_GAIN, 0x3f);/*5088*/
	WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 2, 0, 2); /* reg_dir_dif_win_y=2 bit 0-1*/
	/*reg_edge_str_std_gain = 4;bit 0-3 */
//	WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN, 4, 0, 4);

	/*5287-5298 */
	WRITE_VPP_REG(VPP_PK_DIR0_HP_COEF, 0x00f8e840);
	WRITE_VPP_REG(VPP_PK_DIR1_HP_COEF, 0x0000e040);
	WRITE_VPP_REG(VPP_PK_DIR2_HP_COEF, 0x00f8e840);
	WRITE_VPP_REG(VPP_PK_DIR3_HP_COEF, 0x00f8e840);
	WRITE_VPP_REG(VPP_PK_DIR4_HP_COEF, 0xfffefbf6);

	WRITE_VPP_REG(VPP_PK_DIR4_HP_COEF_F, 0x0000f240);
	WRITE_VPP_REG(VPP_PK_DIR5_HP_COEF, 0x00f8e840);
	WRITE_VPP_REG(VPP_PK_DIR6_HP_COEF, 0x00f8e840);
	WRITE_VPP_REG(VPP_PK_DIR7_HP_COEF, 0x0000e040);
	WRITE_VPP_REG(VPP_PK_DIR0_BP_COEF, 0x00e00536);

	WRITE_VPP_REG(VPP_PK_DIR1_BP_COEF, 0x0000e040);
	WRITE_VPP_REG(VPP_PK_DIR2_BP_COEF, 0x00e00536);
	WRITE_VPP_REG(VPP_PK_DIR3_BP_COEF, 0x00e00536);
	WRITE_VPP_REG(VPP_PK_DIR4_BP_COEF, 0xfff701ea);
	WRITE_VPP_REG(VPP_PK_DIR4_BP_COEF_F, 0x00000338);
	WRITE_VPP_REG(VPP_PK_DIR5_BP_COEF, 0x00e00536);
	WRITE_VPP_REG(VPP_PK_DIR6_BP_COEF, 0x00e00536);
	WRITE_VPP_REG(VPP_PK_DIR7_BP_COEF, 0x0000e040);

	if (chip_type_id == chip_s6)
		WRITE_VPP_REG(SAFA_PPS_DEJAGGY_CTRL, 0x80004444);

}

static enum vsr_pq_cfg_e pre_vsr_cfg;

void vsr_pq_config(enum vsr_pq_cfg_e vsr_cfg,
	enum wr_md_e mode, int vpp_index)
{
	unsigned int data32 = 0;
	if (chip_type_id >= chip_t6d)
		return;

	switch (vsr_cfg) {
	case RES_480P:
	case RES_480I:
		/*peaking*/
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_DERING_EDGE_CONF_GAIN, 8, 4, 4);
			WRITE_VPP_REG_BITS(VPP_HTI_EN_MODE, 1, 20, 1);
			WRITE_VPP_REG_BITS(VPP_HTI_OS_UP_DN_GAIN, 8, 8, 4);

			WRITE_VPP_REG_BITS(VPP_VTI_OS_UP_DN_GAIN, 8, 8, 4);

			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 16, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 24, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 32, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 56, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 80, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 80, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_F, 80, 0, 8);

			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 16, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 24, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 80, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 96, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_F, 16, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 12, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 16, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 28, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 32, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 40, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 56, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_F, 64, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 16, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 24, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 80, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 96, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_F, 16, 0, 8);

			WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE, 32, 16, 6);

			WRITE_VPP_REG_BITS(VPP_NR_DIR_LPF_GAIN, 10, 0, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_DERING_EDGE_CONF_GAIN);
			data32 = (data32 & 0xfffffff0) | (0x8 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_DERING_EDGE_CONF_GAIN,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_EN_MODE);
			data32 = (data32 & 0xffefffff) | (0x1 << 20);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0x8 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_OS_UP_DN_GAIN,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_VTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0x8 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VTI_OS_UP_DN_GAIN,
				data32, vpp_index);

			data32 = (0x38 << 24) | (0x20 << 16) | (0x18 << 8) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x50 << 24) | (0x50 << 16) | (0x50 << 8) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x50;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x40 << 24) | (0x18 << 16) | (0x10 << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x50 << 16) | (0x60 << 8) | 0x50;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = (0x1c << 24) | (0x10 << 16) | (0xc << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x38 << 16) | (0x28 << 8) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x40 << 24) | (0x18 << 16) | (0x10 << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x50 << 16) | (0x60 << 8) | 0x50;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_SR_DIR_PK_LA_ERR_DIS_RATE);
			data32 = (data32 & 0xffc0ffff) | (0x20 << 16);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_NR_DIR_LPF_GAIN);
			data32 = (data32 & 0xffffffc0) | 0xa;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_NR_DIR_LPF_GAIN,
				data32, vpp_index);
		}
		break;
	case RES_720P:
	case RES_720I:
		/*peaking*/
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_DERING_EDGE_CONF_GAIN, 3, 4, 4);
			WRITE_VPP_REG_BITS(VPP_HTI_EN_MODE, 0, 20, 1);
			WRITE_VPP_REG_BITS(VPP_HTI_OS_UP_DN_GAIN, 15, 8, 4);
			WRITE_VPP_REG_BITS(VPP_VTI_OS_UP_DN_GAIN, 15, 8, 4);

			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 16, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 24, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 32, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 56, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 48, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_F, 32, 0, 8);

			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 16, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 24, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 48, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 64, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 96, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_F, 32, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 12, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 16, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 28, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 32, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 40, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 56, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_F, 64, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 16, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 24, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 80, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 96, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_F, 16, 0, 8);

			WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE, 28, 16, 6);
			WRITE_VPP_REG_BITS(VPP_NR_DIR_LPF_GAIN, 10, 0, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_DERING_EDGE_CONF_GAIN);
			data32 = (data32 & 0xfffffff0) | (0x3 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_DERING_EDGE_CONF_GAIN,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_EN_MODE);
			data32 = (data32 & 0xffefffff) | (0x0 << 20);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0xf << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_OS_UP_DN_GAIN,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_VTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0xf << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VTI_OS_UP_DN_GAIN,
				data32, vpp_index);

			data32 = (0x38 << 24) | (0x20 << 16) | (0x18 << 8) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x38 << 24) | (0x40 << 16) | (0x40 << 8) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x30 << 24) | (0x18 << 16) | (0x10 << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x50 << 16) | (0x60 << 8) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = (0x1c << 24) | (0x10 << 16) | (0xc << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x38 << 16) | (0x28 << 8) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x40 << 24) | (0x18 << 16) | (0x10 << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x50 << 16) | (0x60 << 8) | 0x50;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_SR_DIR_PK_LA_ERR_DIS_RATE);
			data32 = (data32 & 0xffc0ffff) | (0x1c << 16);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_NR_DIR_LPF_GAIN);
			data32 = (data32 & 0xffffffc0) | 0xa;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_NR_DIR_LPF_GAIN,
				data32, vpp_index);
		}
		break;
	case RES_1080P:
	case RES_1080I:
		/*peaking*/
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_DERING_EDGE_CONF_GAIN, 3, 4, 4);
			WRITE_VPP_REG_BITS(VPP_HTI_EN_MODE, 1, 20, 1);
			WRITE_VPP_REG_BITS(VPP_HTI_OS_UP_DN_GAIN, 12, 8, 4);
			WRITE_VPP_REG_BITS(VPP_VTI_OS_UP_DN_GAIN, 15, 8, 4);

			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 16, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 24, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 32, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 56, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 80, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 46, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_F, 46, 0, 8);

			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 4, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 10, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 38, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 62, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 108, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 124, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 108, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 92, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_F, 92, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 12, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 16, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 28, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 32, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 40, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 32, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 16, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_F, 2, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 24, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 32, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 46, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 96, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 118, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_F, 16, 0, 8);

			WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE, 20, 16, 6);
			WRITE_VPP_REG_BITS(VPP_NR_DIR_LPF_GAIN, 16, 0, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_DERING_EDGE_CONF_GAIN);
			data32 = (data32 & 0xfffffff0) | (0x3 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_DERING_EDGE_CONF_GAIN,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_EN_MODE);
			data32 = (data32 & 0xffefffff) | (0x1 << 20);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0xc << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_OS_UP_DN_GAIN,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0xf << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VTI_OS_UP_DN_GAIN,
				data32, vpp_index);

			data32 = (0x38 << 24) | (0x20 << 16) | (0x18 << 8) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x2e << 24) | (0x40 << 16) | (0x50 << 8) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x2e;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x3e << 24) | (0x26 << 16) | (0xa << 8) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x5c << 24) | (0x6c << 16) | (0x7c << 8) | 0x6c;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x5c;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = (0x1c << 24) | (0x10 << 16) | (0xc << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x10 << 24) | (0x20 << 16) | (0x28 << 8) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x2;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x40 << 24) | (0x2e << 16) | (0x20 << 8) | 0x18;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x50 << 16) | (0x76 << 8) | 0x60;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_SR_DIR_PK_LA_ERR_DIS_RATE);
			data32 = (data32 & 0xffc0ffff) | (0x14 << 16);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_NR_DIR_LPF_GAIN);
			data32 = (data32 & 0xffffffc0) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_NR_DIR_LPF_GAIN,
				data32, vpp_index);
		}
		break;
	case DEFAULT_PARAM:
	default:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_DERING_EDGE_CONF_GAIN, 4, 4, 4);
			WRITE_VPP_REG_BITS(VPP_HTI_EN_MODE, 1, 20, 1);
			WRITE_VPP_REG_BITS(VPP_HTI_OS_UP_DN_GAIN, 8, 8, 4);
			WRITE_VPP_REG_BITS(VPP_VTI_OS_UP_DN_GAIN, 8, 8, 4);

			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 16, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 24, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 32, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_0, 56, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 80, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 64, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_1, 32, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_BST_GAIN_LUT_F, 4, 0, 8);

			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 16, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 24, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_0, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 80, 0, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 96, 8, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_HLTI_OS_ADP_LUT_F, 16, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 12, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 16, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_0, 28, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 32, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 40, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 32, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_1, 16, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_BST_GAIN_LUT_F, 2, 0, 8);

			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 8, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 16, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 24, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_0, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 80, 0, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 96, 8, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 80, 16, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_1, 64, 24, 8);
			WRITE_VPP_REG_BITS(VPP_VLTI_OS_ADP_LUT_F, 16, 0, 8);

			WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE, 32, 16, 6);
			WRITE_VPP_REG_BITS(VPP_NR_DIR_LPF_GAIN, 16, 0, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_DERING_EDGE_CONF_GAIN);
			data32 = (data32 & 0xfffffff0) | (0x4 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_DERING_EDGE_CONF_GAIN,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_EN_MODE);
			data32 = (data32 & 0xffefffff) | (0x1 << 20);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0x8 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HTI_OS_UP_DN_GAIN,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_VTI_OS_UP_DN_GAIN);
			data32 = (data32 & 0xfffff0ff) | (0x8 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VTI_OS_UP_DN_GAIN,
				data32, vpp_index);

			data32 = (0x38 << 24) | (0x20 << 16) | (0x18 << 8) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x20 << 24) | (0x40 << 16) | (0x50 << 8) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x40 << 24) | (0x18 << 16) | (0x10 << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x50 << 16) | (0x60 << 8) | 0x50;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_HLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_HLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = (0x1c << 24) | (0x10 << 16) | (0xc << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_0,
				data32, vpp_index);
			data32 = (0x10 << 24) | (0x20 << 16) | (0x28 << 8) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_BST_GAIN_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x2;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_BST_GAIN_LUT_F,
				data32, vpp_index);

			data32 = (0x40 << 24) | (0x18 << 16) | (0x10 << 8) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_0,
				data32, vpp_index);
			data32 = (0x40 << 24) | (0x50 << 16) | (0x60 << 8) | 0x50;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_1,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_VLTI_OS_ADP_LUT_F);
			data32 = (data32 & 0xffffff00) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_VLTI_OS_ADP_LUT_F,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_SR_DIR_PK_LA_ERR_DIS_RATE);
			data32 = (data32 & 0xffc0ffff) | (0x20 << 16);
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
				data32, vpp_index);

			data32 = READ_VPP_REG(VPP_NR_DIR_LPF_GAIN);
			data32 = (data32 & 0xffffffc0) | 0x10;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_NR_DIR_LPF_GAIN,
				data32, vpp_index);
		}
		break;
	}
}

void safa_pq_config(enum vsr_pq_cfg_e vsr_cfg,
	enum wr_md_e mode, int vpp_index)
{
	unsigned int data32 = 0;
	unsigned int addr_offset = 0;

	if (chip_type_id == chip_t6d)
		addr_offset = 0x7;
	else if (chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9)
		addr_offset = 0x8;

	switch (vsr_cfg) {
	case RES_480P:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN, 1, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 20, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 1, 8, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 0, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN + addr_offset, 2, 4, 2);
/*			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 1, 0, 2);*/
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 3, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 4, 0, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 20, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 15, 16, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 28, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 10, 16, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 0, 4, 2);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 8, 0, 4);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_XERR_GAIN + addr_offset, 8, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_RATE + addr_offset, 24, 8, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(SAFA_PPS_SR_422_EN);
			data32 = (data32 & 0xffffffef) | (0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_SR_422_EN,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_EN_MODE + addr_offset);
			data32 = (data32 & 0xffeffeff) | (0x0 << 20) | (0x1 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_EN_MODE + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_HIST_WIN + addr_offset);
			data32 = (data32 & 0xffffffee) |
				(0x0 << 4) | 0x0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_HIST_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_DIF_WIN + addr_offset);
			data32 = (data32 & 0xffffffcf) |
				(0x2 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_DIF_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_H + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xf << 16) | (0x14 << 8) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_H + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_V + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0x1c << 8) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_V + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) |
				(0x0 << 4) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_XERR_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_XERR_GAIN + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_RATE + addr_offset);
			data32 = (data32 & 0xffffc0ff) | (0x18 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_RATE + addr_offset,
				data32, vpp_index);
		}
		break;
	case RES_480I:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN, 1, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 20, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 1, 8, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 0, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 4, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN + addr_offset, 2, 4, 2);
/*			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 1, 0, 2);*/
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 3, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 4, 0, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 20, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 15, 16, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 28, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 10, 16, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 0, 4, 2);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 8, 0, 4);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_XERR_GAIN + addr_offset, 48, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_RATE + addr_offset, 36, 8, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(SAFA_PPS_SR_422_EN);
			data32 = (data32 & 0xffffffef) | (0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_SR_422_EN,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_EN_MODE + addr_offset);
			data32 = (data32 & 0xffeffeff) | (0x0 << 20) | (0x1 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_EN_MODE + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_HIST_WIN + addr_offset);
			data32 = (data32 & 0xffffffee) |
				(0x0 << 4) | 0x0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_HIST_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_DIF_WIN + addr_offset);
			data32 = (data32 & 0xffffffcf) |
				(0x2 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_DIF_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_H + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xf << 16) | (0x14 << 8) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_H + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_V + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0x1c << 8) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_V + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) |
				(0x0 << 4) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_XERR_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) | 0x30;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_XERR_GAIN + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_RATE + addr_offset);
			data32 = (data32 & 0xffffc0ff) | (0x24 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_RATE + addr_offset,
				data32, vpp_index);
		}
		break;
	case RES_720P:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN, 1, 4, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 20, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 1, 8, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 1, 0, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN + addr_offset, 1, 4, 2);
/*			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 1, 0, 2);*/
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 3, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 4, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 10, 16, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 10, 16, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 0, 4, 2);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 4, 0, 4);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_XERR_GAIN + addr_offset, 8, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_RATE + addr_offset, 24, 8, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(SAFA_PPS_SR_422_EN);
			data32 = (data32 & 0xffffffef) | (0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_SR_422_EN,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_EN_MODE + addr_offset);
			data32 = (data32 & 0xffeffeff) | (0x0 << 20) | (0x1 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_EN_MODE + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_HIST_WIN + addr_offset);
			data32 = (data32 & 0xffffffee) |
				(0x0 << 4) | 0x1;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_HIST_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_DIF_WIN + addr_offset);
			data32 = (data32 & 0xffffffcf) |
				(0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_DIF_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_H + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_H + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_V + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_V + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) |
				(0x0 << 4) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_XERR_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_XERR_GAIN + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_RATE + addr_offset);
			data32 = (data32 & 0xffffc0ff) | (0x18 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_RATE + addr_offset,
				data32, vpp_index);
		}
		break;
	case RES_720I:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN, 1, 4, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 20, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 1, 8, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 1, 0, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN + addr_offset, 1, 4, 2);
/*			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 1, 0, 2);*/
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 3, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 4, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 10, 16, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 10, 16, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 0, 4, 2);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 4, 0, 4);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_XERR_GAIN + addr_offset, 8, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_RATE + addr_offset, 24, 8, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(SAFA_PPS_SR_422_EN);
			data32 = (data32 & 0xffffffef) | (0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_SR_422_EN,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_EN_MODE + addr_offset);
			data32 = (data32 & 0xfeeffeff) | (0x0 << 20) | (0x1 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_EN_MODE + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_HIST_WIN + addr_offset);
			data32 = (data32 & 0xffffffee) |
				(0x0 << 4) | 0x1;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_HIST_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_DIF_WIN + addr_offset);
			data32 = (data32 & 0xffffffcf) |
				(0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_DIF_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_H + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_H + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_V + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_V + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) |
				(0x0 << 4) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_XERR_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_XERR_GAIN + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_RATE + addr_offset);
			data32 = (data32 & 0xffffc0ff) | (0x18 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_RATE + addr_offset,
				data32, vpp_index);
		}
		break;
	case RES_1080P:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN, 1, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 20, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 8, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 1, 0, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN + addr_offset, 1, 4, 2);
/*			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 2, 0, 2);*/
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 3, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 4, 0, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 10, 16, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 10, 16, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 1, 4, 2);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 4, 0, 4);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_XERR_GAIN + addr_offset, 8, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_RATE + addr_offset, 24, 8, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(SAFA_PPS_SR_422_EN);
			data32 = (data32 & 0xffffffef) | (0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_SR_422_EN,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_EN_MODE + addr_offset);
			data32 = (data32 & 0xffeffeff) | (0x0 << 20) | (0x0 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_EN_MODE + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_HIST_WIN + addr_offset);
			data32 = (data32 & 0xffffffee) |
				(0x0 << 4) | 0x1;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_HIST_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_DIF_WIN + addr_offset);
			data32 = (data32 & 0xffffffcf) |
				(0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_DIF_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_H + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_H + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_V + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_V + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) |
				(0x1 << 4) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_XERR_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_XERR_GAIN + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_RATE  + addr_offset);
			data32 = (data32 & 0xffffc0ff) | (0x18 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_RATE + addr_offset,
				data32, vpp_index);
		}
		break;
	case RES_1080I:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN, 1, 4, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 20, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 8, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 1, 0, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN + addr_offset, 1, 4, 2);
/*			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 2, 0, 2);*/
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 3, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 4, 0, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 10, 16, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 10, 16, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 1, 4, 2);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 4, 0, 4);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_XERR_GAIN + addr_offset, 8, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_RATE + addr_offset, 24, 8, 6);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(SAFA_PPS_SR_422_EN);
			data32 = (data32 & 0xffffffef) | (0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_SR_422_EN,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_EN_MODE + addr_offset);
			data32 = (data32 & 0xffeffeff) | (0x0 << 20) | (0x0 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_EN_MODE + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_HIST_WIN + addr_offset);
			data32 = (data32 & 0xffffffee) |
				(0x0 << 4) | 0x1;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_HIST_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_DIF_WIN + addr_offset);
			data32 = (data32 & 0xffffffcf) |
				(0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_DIF_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_H + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_H + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_V + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_V + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) |
				(0x1 << 4) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_XERR_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) | 0x8;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_XERR_GAIN + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_RATE + addr_offset);
			data32 = (data32 & 0xffffc0ff) | (0x18 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_RATE + addr_offset,
				data32, vpp_index);
		}
		break;
	case DEFAULT_PARAM:
	default:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN, 0, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 20, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_EN_MODE + addr_offset, 0, 8, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 0, 0, 1);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_HIST_WIN + addr_offset, 1, 4, 1);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN + addr_offset, 1, 4, 2);
/*			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_DIF_WIN, 2, 0, 2);*/

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 17, 0, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 17, 0, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_H + addr_offset, 10, 16, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 15, 8, 6);
			WRITE_VPP_REG_BITS(SAFA_PPS_DIR_SWAP_THD_V + addr_offset, 10, 16, 6);

			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 1, 4, 2);
			WRITE_VPP_REG_BITS(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset, 4, 0, 4);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(SAFA_PPS_SR_422_EN);
			data32 = (data32 & 0xffffffef) | (0x0 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_SR_422_EN,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_EN_MODE + addr_offset);
			data32 = (data32 & 0xffeffeff) | (0x0 << 20) | (0x0 << 8);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_EN_MODE + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_HIST_WIN + addr_offset);
			data32 = (data32 & 0xffffffee) |
				(0x1 << 4) | 0x0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_HIST_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_DIF_WIN + addr_offset);
			data32 = (data32 & 0xffffffcf) |
				(0x1 << 4);
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_DIF_WIN + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_H + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x11;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_H + addr_offset,
				data32, vpp_index);
			data32 = READ_VPP_REG(SAFA_PPS_DIR_SWAP_THD_V + addr_offset);
			data32 = (data32 & 0xffc0c0c0) |
				(0xa << 16) | (0xf << 8) | 0x11;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_DIR_SWAP_THD_V + addr_offset,
				data32, vpp_index);

			data32 = READ_VPP_REG(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset);
			data32 = (data32 & 0xffffffc0) |
				(0x1 << 4) | 0x4;
			VSYNC_WRITE_VPP_REG_VPP_SEL(SAFA_PPS_EDGE_STR_MODE_GAIN + addr_offset,
				data32, vpp_index);
		}
		break;
	}
}

void pi_pq_config(enum vsr_pq_cfg_e vsr_cfg,
	enum wr_md_e mode, int vpp_index)
{
	unsigned int data32 = 0;

	if (chip_type_id == chip_t6d)
		return;/*not support*/

	switch (vsr_cfg) {
	case RES_480P:
	case RES_480I:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_PI_DICT_NUM, 64, 0, 7);
			WRITE_VPP_REG_BITS(VPP_PI_EN_MODE, 2, 0, 2);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 8, 8, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 2, 4, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 3, 0, 4);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF_F, 255, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1e0, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1e0, 12, 9);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_PI_DICT_NUM);
			data32 = (data32 & 0xffffff00) | 0x40;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_DICT_NUM,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_EN_MODE);
			data32 = (data32 & 0xfffffffc) | 0x2;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_WIN_OFST);
			data32 = (data32 & 0xfffff000) |
				(0x8 << 8) | (0x2 << 4) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_WIN_OFST,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF_F);
			data32 = (data32 & 0xfffffe00) | 0xff;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF_F,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF);
			data32 = (data32 & 0xffe00e00) |
				(0x1e0 << 12) | 0x1e0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF,
				data32, vpp_index);
		}
		break;
	case RES_720P:
	case RES_720I:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_PI_DICT_NUM, 48, 0, 7);
			WRITE_VPP_REG_BITS(VPP_PI_EN_MODE, 1, 0, 2);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 6, 8, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 2, 4, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 3, 0, 4);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF_F, 204, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1e0, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1ed, 12, 9);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_PI_DICT_NUM);
			data32 = (data32 & 0xffffff00) | 0x30;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_DICT_NUM,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_EN_MODE);
			data32 = (data32 & 0xfffffffc) | 0x1;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_WIN_OFST);
			data32 = (data32 & 0xfffff000) |
				(0x6 << 8) | (0x2 << 4) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_WIN_OFST,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF_F);
			data32 = (data32 & 0xfffffe00) | 0xcc;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF_F,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF);
			data32 = (data32 & 0xffe00e00) |
				(0x1ed << 12) | 0x1e0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF,
				data32, vpp_index);
		}
		break;
	case RES_1080P:
	case RES_1080I:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_PI_DICT_NUM, 32, 0, 7);
			WRITE_VPP_REG_BITS(VPP_PI_EN_MODE, 0, 0, 2);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 4, 8, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 1, 4, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 3, 0, 4);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF_F, 96, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1eb, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1fd, 12, 9);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_PI_DICT_NUM);
			data32 = (data32 & 0xffffff00) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_DICT_NUM,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_EN_MODE);
			data32 = (data32 & 0xfffffffc) | 0x0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_WIN_OFST);
			data32 = (data32 & 0xfffff000) |
				(0x4 << 8) | (0x1 << 4) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_WIN_OFST,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF_F);
			data32 = (data32 & 0xfffffe00) | 0x60;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF_F,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF);
			data32 = (data32 & 0xffe00e00) |
				(0x1fd << 12) | 0x1eb;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF,
				data32, vpp_index);
		}
		break;
	case DEFAULT_PARAM:
	default:
		if (mode == WR_VCB) {
			WRITE_VPP_REG_BITS(VPP_PI_DICT_NUM, 32, 0, 7);
			WRITE_VPP_REG_BITS(VPP_PI_EN_MODE, 0, 0, 2);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 4, 8, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 1, 4, 4);
			WRITE_VPP_REG_BITS(VPP_PI_WIN_OFST, 3, 0, 4);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF_F, 96, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1eb, 0, 9);
			WRITE_VPP_REG_BITS(VPP_PI_HF_COEF, 0x1fd, 12, 9);
		} else if (mode == WR_DMA) {
			data32 = READ_VPP_REG(VPP_PI_DICT_NUM);
			data32 = (data32 & 0xffffff00) | 0x20;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_DICT_NUM,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_EN_MODE);
			data32 = (data32 & 0xfffffffc) | 0x0;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_EN_MODE,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_WIN_OFST);
			data32 = (data32 & 0xfffff000) |
				(0x4 << 8) | (0x1 << 4) | 0x3;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_WIN_OFST,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF_F);
			data32 = (data32 & 0xfffffe00) | 0x60;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF_F,
				data32, vpp_index);
			data32 = READ_VPP_REG(VPP_PI_HF_COEF);
			data32 = (data32 & 0xffe00e00) |
				(0x1fd << 12) | 0x1eb;
			VSYNC_WRITE_VPP_REG_VPP_SEL(VPP_PI_HF_COEF,
				data32, vpp_index);
		}
		break;
	}
}

unsigned char bak_en_422to444;
unsigned char bak_en_444to422;
unsigned char bak_en_pi;
unsigned char bak_dir_lpf_c_en;
unsigned char bak_nr_gau_c_en;
unsigned char bak_hcti_en;
unsigned char bak_vcti_en;
unsigned char bak_os2_var_v_win_mode;
unsigned char bak_os2_alp_info_sel;
unsigned char bak_dir_lpf_alp_sel;
unsigned char bak_pk_dir_alp_sel;
unsigned char bak_ti_dir_alp_sel;
unsigned char bak_dnlp_input_422;
unsigned char bak_lc_sr_in;
unsigned char bak_safa_input_422;

void backup_422_config(void)
{
	if (chip_type_id < chip_t6d)
		return;/*not support*/

	bak_en_422to444 = READ_VPP_REG_BITS(VPP_VSR_TOP_C42C44_MODE,
		6, 1);
	bak_en_444to422 = READ_VPP_REG_BITS(VPP_VSR_TOP_C42C44_MODE,
		2, 1);
	if (chip_type_id != chip_t6d)
		bak_en_pi = READ_VPP_REG_BITS(VPP_PI_EN_MODE,
			4, 1);
	bak_dir_lpf_c_en = READ_VPP_REG_BITS(VPP_NR_LPF_EN,
		16, 1);
	bak_nr_gau_c_en = READ_VPP_REG_BITS(VPP_NR_LPF_EN,
		4, 1);
	bak_hcti_en = READ_VPP_REG_BITS(VPP_HTI_EN_MODE,
		4, 1);
	bak_vcti_en = READ_VPP_REG_BITS(VPP_VTI_EN,
		16, 1);
	bak_os2_alp_info_sel = READ_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
		20, 3);
	bak_dir_lpf_alp_sel = READ_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
		16, 3);
	bak_pk_dir_alp_sel = READ_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
		12, 3);
	bak_ti_dir_alp_sel = READ_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
		8, 3);
	bak_dnlp_input_422 = READ_VPP_REG_BITS(VPP_DNLP_EN_MODE,
		12, 1);
	bak_lc_sr_in = READ_VPP_REG_BITS(VPP_LC_MODE,
		8, 1);
	bak_safa_input_422 = READ_VPP_REG_BITS(SAFA_PPS_SR_422_EN,
		0, 1);
	if (chip_type_id == chip_t6x)
		bak_os2_var_v_win_mode = READ_VPP_REG_BITS(VPP_OS2_APL_INFO,
			24, 1);
}

void update_444_config(int input_444_mode,
	enum wr_md_e mode, int vpp_index)
{
	unsigned int en_422to444;
	unsigned int en_444to422;
	unsigned int en_pi;
	unsigned int dir_lpf_c_en;
	unsigned int nr_gau_c_en;
	unsigned int hcti_en;
	unsigned int vcti_en;
	unsigned int os2_var_v_win_mode;
	unsigned int os2_alp_info_sel;
	unsigned int dir_lpf_alp_sel;
	unsigned int pk_dir_alp_sel;
	unsigned int ti_dir_alp_sel;
	unsigned int dnlp_input_422;
	unsigned int lc_sr_in;
	unsigned int safa_input_422;

	if (chip_type_id < chip_t6d)
		return;/*not support*/

	if (input_444_mode) {
		en_422to444 = 0;
		en_444to422 = 0;
		en_pi = 0;
		dir_lpf_c_en = 0;
		nr_gau_c_en = 0;
		hcti_en = 0;
		vcti_en = 0;
		os2_var_v_win_mode = 0;
		os2_alp_info_sel = 1;
		dir_lpf_alp_sel = 1;
		pk_dir_alp_sel = 1;
		ti_dir_alp_sel = 1;
		dnlp_input_422 = 0;
		lc_sr_in = 1;
		safa_input_422 = 0;
	} else {
		en_422to444 = bak_en_422to444;
		en_444to422 = bak_en_444to422;
		en_pi = bak_en_pi;
		dir_lpf_c_en = bak_dir_lpf_c_en;
		nr_gau_c_en = bak_nr_gau_c_en;
		hcti_en = bak_hcti_en;
		vcti_en = bak_vcti_en;
		os2_var_v_win_mode = bak_os2_var_v_win_mode;
		os2_alp_info_sel = bak_os2_alp_info_sel;
		dir_lpf_alp_sel = bak_dir_lpf_alp_sel;
		pk_dir_alp_sel = bak_pk_dir_alp_sel;
		ti_dir_alp_sel = bak_ti_dir_alp_sel;
		dnlp_input_422 = bak_dnlp_input_422;
		lc_sr_in = bak_lc_sr_in;
		safa_input_422 = bak_safa_input_422;
	}

	if (mode == WR_VCB) {
		WRITE_VPP_REG_BITS(VPP_VSR_TOP_C42C44_MODE,
			en_422to444, 6, 1);
		WRITE_VPP_REG_BITS(VPP_VSR_TOP_C42C44_MODE,
			en_444to422, 2, 1);
		if (chip_type_id != chip_t6d)
			WRITE_VPP_REG_BITS(VPP_PI_EN_MODE,
				en_pi, 4, 1);
		WRITE_VPP_REG_BITS(VPP_NR_LPF_EN,
			dir_lpf_c_en, 16, 1);
		WRITE_VPP_REG_BITS(VPP_NR_LPF_EN,
			nr_gau_c_en, 4, 1);
		WRITE_VPP_REG_BITS(VPP_HTI_EN_MODE,
			hcti_en, 4, 1);
		WRITE_VPP_REG_BITS(VPP_VTI_EN,
			vcti_en, 16, 1);
		WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			os2_alp_info_sel, 20, 3);
		WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			dir_lpf_alp_sel, 16, 3);
		WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			pk_dir_alp_sel, 12, 3);
		WRITE_VPP_REG_BITS(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			ti_dir_alp_sel, 8, 3);
		WRITE_VPP_REG_BITS(VPP_DNLP_EN_MODE,
			dnlp_input_422, 12, 1);
		WRITE_VPP_REG_BITS(VPP_LC_MODE,
			lc_sr_in, 8, 1);
		WRITE_VPP_REG_BITS(SAFA_PPS_SR_422_EN,
			safa_input_422, 0, 1);
		if (chip_type_id == chip_t6x)
			WRITE_VPP_REG_BITS(VPP_OS2_APL_INFO,
				os2_var_v_win_mode, 24, 1);
	} else if (mode == WR_DMA) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VSR_TOP_C42C44_MODE,
			en_422to444, 6, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VSR_TOP_C42C44_MODE,
			en_444to422, 2, 1, vpp_index);
		if (chip_type_id != chip_t6d)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_PI_EN_MODE,
				en_pi, 4, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_NR_LPF_EN,
			dir_lpf_c_en, 16, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_NR_LPF_EN,
			nr_gau_c_en, 4, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_HTI_EN_MODE,
			hcti_en, 4, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VTI_EN,
			vcti_en, 16, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			os2_alp_info_sel, 20, 3, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			dir_lpf_alp_sel, 16, 3, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			pk_dir_alp_sel, 12, 3, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SR_DIR_PK_LA_ERR_DIS_RATE,
			ti_dir_alp_sel, 8, 3, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_DNLP_EN_MODE,
			dnlp_input_422, 12, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_LC_MODE,
			lc_sr_in, 8, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SAFA_PPS_SR_422_EN,
			safa_input_422, 0, 1, vpp_index);
		if (chip_type_id == chip_t6x)
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_OS2_APL_INFO,
				os2_var_v_win_mode, 24, 1, vpp_index);
	}
}

unsigned int skip_444_settings(struct am_reg_s *p)
{
	unsigned int ret = 0;
	unsigned int addr_offset[2] = {0};

	if (chip_type_id == chip_t6x) {
		addr_offset[0] = 0x1b;
		addr_offset[1] = 0x21;

		if (p->addr == VPP_OS2_APL_INFO)
			ret = 0x1 << 24;
	}

	if (p->addr == VPP_VSR_TOP_C42C44_MODE)
		ret = (0x1 << 6) | (0x1 << 2);
	else if (p->addr == VPP_PI_EN_MODE)
		ret = 0x1 << 4;
	else if (p->addr == VPP_NR_LPF_EN + addr_offset[0])
		ret = (0x1 << 16) | (0x1 << 4);
	else if (p->addr == VPP_HTI_EN_MODE + addr_offset[0])
		ret = 0x1 << 4;
	else if (p->addr == VPP_VTI_EN + addr_offset[1])
		ret = 0x1 << 16;
	else if (p->addr == VPP_SR_DIR_PK_LA_ERR_DIS_RATE + addr_offset[0])
		ret = (0x7 << 20) | (0x7 << 16) | (0x7 << 12) | (0x7 << 8);
	else if (p->addr == VPP_DNLP_EN_MODE)
		ret = 0x1 << 12;
	else if (p->addr == VPP_LC_INPUT_SEL)
		ret = (0x7 << 4) | 0x7;
	else if (p->addr == VPP_LC_MODE)
		ret = 0x1 << 8;
	else if (p->addr == SAFA_PPS_SR_422_EN)
		ret = 0x1;

	return ret;
}

void amve_444_config_update(struct vframe_s *vf, int vpp_index)
{
	int cur_444_mode = 0;

	if (!vf)
		return;

	if (chip_type_id >= chip_t6d) {
		if ((vf->type & VIDTYPE_VIU_444) &&
			((vf->flag & VFRAME_FLAG_PC_MODE) || (vf->type & VIDTYPE_PIC)))
			cur_444_mode = 1;
		else
			cur_444_mode = 0;

		if (input_444_mode != cur_444_mode) {
			input_444_mode = cur_444_mode;

			if (cur_444_mode)
				backup_422_config();

			update_444_config(cur_444_mode, WR_DMA, vpp_index);
		}
	}
}

void amve_vsr_config_update(struct vframe_s *vf, int vpp_index)
{
	enum vsr_pq_cfg_e cur_vsr_cfg = DEFAULT_PARAM;
	unsigned int height_out = 2160;
	unsigned int width_out = 3840;
	unsigned int width_in = 1920;
	struct vinfo_s *vinfo = get_current_vinfo();
	unsigned int w_rate = 0;
	unsigned int src_w = 0;
	unsigned int src_compwidth = 0;

	if (!vf)
		return;

	if (chip_type_id == chip_c5)
		return;

	if (vinfo) {
		height_out = vinfo->height;
		width_out = vinfo->width;
	}

	w_rate = vf->dpss_pps_dsx;

	if (w_rate) {
		src_w = vf->width / (w_rate + 1);
		src_compwidth = vf->compWidth / (w_rate + 1);
	} else {
		src_w = vf->width;
		src_compwidth = vf->compWidth;
	}

	width_in = (vf->type & VIDTYPE_COMPRESS) ?
		src_compwidth : src_w;

	if (chip_type_id >= chip_s7d) {
		if (width_out > 1920 &&
			width_out <= 3840) {
		/* 4k out*/
			if (width_in <= 1024) {
				if ((vf->type_original & 0xf) != VIDTYPE_PROGRESSIVE)
					cur_vsr_cfg = RES_480I;
				else
					cur_vsr_cfg = RES_480P;
			} else if (width_in < 1920) {
				if ((vf->type_original & 0xf) != VIDTYPE_PROGRESSIVE)
					cur_vsr_cfg = RES_720I;
				else
					cur_vsr_cfg = RES_720P;
			} else {
				if ((vf->type_original & 0xf) != VIDTYPE_PROGRESSIVE)
					cur_vsr_cfg = RES_1080I;
				else
					cur_vsr_cfg = RES_1080P;
			}
		} else if (width_out > 1280 &&
			width_out <= 1920) {
		/* 2k out*/
			if (width_in <= 1024) {
				if ((vf->type_original & 0xf) != VIDTYPE_PROGRESSIVE)
					cur_vsr_cfg = RES_480I;
				else
					cur_vsr_cfg = RES_480P;
			} else if (width_in < 1920) {
				if ((vf->type_original & 0xf) != VIDTYPE_PROGRESSIVE)
					cur_vsr_cfg = RES_720I;
				else
					cur_vsr_cfg = RES_720P;
			}
		} else {
		/*sd out*/
			if (width_in <= 1024) {
				if ((vf->type_original & 0xf) != VIDTYPE_PROGRESSIVE)
					cur_vsr_cfg = RES_480I;
				else
					cur_vsr_cfg = RES_480P;
			}
		}

		if (cur_vsr_cfg != pre_vsr_cfg) {
			pr_amve_dbg("\n[%s] vf->type_original = %x,cur_vsr_cfg=%d\n",
				__func__, vf->type_original, cur_vsr_cfg);
			pr_amve_dbg("\n[%s] set OK %d-->%d, w_in=%d\n",
				__func__, pre_vsr_cfg, cur_vsr_cfg, width_in);
			vsr_pq_config(cur_vsr_cfg, WR_DMA, vpp_index);
			safa_pq_config(cur_vsr_cfg, WR_DMA, vpp_index);
			pi_pq_config(cur_vsr_cfg, WR_DMA, vpp_index);
			pre_vsr_cfg = cur_vsr_cfg;
		}
	}
}

void amve_sharpness_sub_ctrl(unsigned int sel, unsigned int enable)
{
	unsigned int addr_offset = 0;

	if (enable)
		enable = 1;

	if (chip_type_id == chip_a9)
		addr_offset = 0x8;

	/*0:sr all; 1:peaking; 2:lti/cti; 3:dering;*/
	/*4: pi; 5: safa; 6: drlpf;*/
	switch (sel) {
	case 0:
		WRITE_VPP_REG_BITS(VPP_SR_EN,
			enable, 0, 1);
		break;
	case 1:
		WRITE_VPP_REG_BITS(VPP_PK_EN,
			enable, 0, 1);
		break;
	case 2:
		WRITE_VPP_REG_BITS(VPP_HTI_EN_MODE,
			enable, 12, 1);
		WRITE_VPP_REG_BITS(VPP_HTI_EN_MODE,
			enable, 4, 1);
		if (chip_type_id == chip_t6x) {
			WRITE_VPP_REG_BITS(VPP_VTI_EN,
				enable, 20, 1);
			WRITE_VPP_REG_BITS(VPP_VTI_EN,
				enable, 16, 1);
		} else {
			WRITE_VPP_REG_BITS(VPP_VTI_EN,
				enable, 8, 1);
			WRITE_VPP_REG_BITS(VPP_VTI_EN,
				enable, 4, 1);
		}
		break;
	case 3:
		WRITE_VPP_REG_BITS(VPP_DERING_EN,
			enable, 0, 1);
		break;
	case 4:
		WRITE_VPP_REG_BITS(VPP_PI_EN_MODE,
			enable, 4, 1);
		break;
	case 5:
		WRITE_VPP_REG_BITS(SAFA_PPS_INTERP_EN_MODE + addr_offset,
			enable, 25, 1);
		break;
	case 6:
		WRITE_VPP_REG_BITS(VPP_NR_LPF_EN,
			enable, 24, 1);
		WRITE_VPP_REG_BITS(VPP_NR_LPF_EN,
			enable, 16, 1);
		WRITE_VPP_REG_BITS(VPP_NR_LPF_EN,
			enable, 12, 1);
		break;
	default:
		break;
	}
}

void amve_safa_demo_ctrl(unsigned int enable)
{
	if (enable) {
		WRITE_VPP_REG_BITS(VPP_PK_FINAL_GAIN, demo_pk_sr_final_pgains, 8, 8);
		WRITE_VPP_REG_BITS(VPP_PK_FINAL_GAIN, demo_pk_sr_final_ngains, 0, 8);

		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_0, 8, 0, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_0, 12, 8, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_0, 16, 16, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_0, 24, 24, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_1, 24, 0, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_1, 20, 8, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_1, 16, 16, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_1, 12, 24, 6);
		WRITE_VPP_REG_BITS(VPP_PK_OS_ADP_LUT_F, 8, 0, 6);
	} else {
		WRITE_VPP_REG_BITS(VPP_PK_FINAL_GAIN, 64, 8, 8);
		WRITE_VPP_REG_BITS(VPP_PK_FINAL_GAIN, 64, 0, 8);

		WRITE_VPP_REG(VPP_PK_OS_ADP_LUT_0, 0x0a080604);/*52d5-52d7 */
		WRITE_VPP_REG(VPP_PK_OS_ADP_LUT_1, 0x06080a0c);
		WRITE_VPP_REG(VPP_PK_OS_ADP_LUT_F, 0x00000004);
	}
}

void osd_sharpness_init(void)
{
	unsigned int addr_offset = 0;

	if (chip_type_id == chip_t6w || chip_type_id == chip_t6x)
		addr_offset = 0x2900;
	else if (chip_type_id == chip_a9)
		addr_offset = 0x1a00;

	WRITE_VPP_REG_BITS(OSD_SR_EN - addr_offset, 0, 0, 1);
	WRITE_VPP_REG_BITS(OSD_PK_EN - addr_offset, 0, 0, 1);
	WRITE_VPP_REG_BITS(OSD_PK_OS_EN_MODE - addr_offset, 0, 31, 1);
//	WRITE_VPP_REG_BITS(OSD_PK_OS_EN_MODE, 1, 5, 1);
//	WRITE_VPP_REG_BITS(OSD_PK_OS_EN_MODE, 1, 0, 1);
	WRITE_VPP_REG_BITS(OSD_SR_CC_EN - addr_offset, 0, 0, 1);

	/*size config*/
	WRITE_VPP_REG_BITS(OSD_SR_SIZE - addr_offset, 3840, 0, 16);//hsize
	WRITE_VPP_REG_BITS(OSD_SR_SIZE - addr_offset, 2160, 16, 16);//vsize

	/*mic config*/
	WRITE_VPP_REG_BITS(OSD_SR_MISC - addr_offset, 0x0, 16, 8);
	WRITE_VPP_REG_BITS(OSD_SR_MISC - addr_offset, 0x4, 8, 8);
	WRITE_VPP_REG_BITS(OSD_SR_MISC - addr_offset, 0x1, 1, 1);
	WRITE_VPP_REG_BITS(OSD_SR_MISC - addr_offset, 0x1, 0, 1);

	/*gclk*/
	WRITE_VPP_REG_BITS(OSD_SR_GCLK_CTRL - addr_offset, 0x0, 0, 32);
}

void osd_sharpness_ctrl(unsigned int sel, unsigned int enable)
{
	unsigned int addr_offset = 0;

	if (chip_type_id == chip_t6w || chip_type_id == chip_t6x)
		addr_offset = 0x2900;
	else if (chip_type_id == chip_a9)
		addr_offset = 0x1a00;

	if (enable)
		enable = 1;

	/* 0: sr; 1: pk; 2: os; 3: cc*/
	switch (sel) {
	case 0:
		WRITE_VPP_REG_BITS(OSD_SR_EN - addr_offset,
			enable, 0, 1);
		break;
	case 1:
		WRITE_VPP_REG_BITS(OSD_PK_EN - addr_offset,
			enable, 0, 1);
		break;
	case 2:
		WRITE_VPP_REG_BITS(OSD_PK_OS_EN_MODE - addr_offset,
			enable, 31, 1);
		break;
	case 3:
		WRITE_VPP_REG_BITS(OSD_SR_CC_EN - addr_offset,
			enable, 0, 1);
		break;
	default:
		break;
	}
}

void osd_sharpness_size_ctrl(void)
{
	unsigned int addr_offset = 0;

	if (chip_type_id == chip_t6w || chip_type_id == chip_t6x)
		addr_offset = 0x2900;
	else if (chip_type_id == chip_a9)
		addr_offset = 0x1a00;

	WRITE_VPP_REG_BITS(OSD_SR_SIZE - addr_offset, hsize_in, 0, 16);//hsize
	WRITE_VPP_REG_BITS(OSD_SR_SIZE - addr_offset, vsize_in, 16, 16);//vsize
}

void osd_sharpness_demo_ctrl(void)
{
	unsigned int addr_offset = 0;

	if (chip_type_id == chip_t6w || chip_type_id == chip_t6x)
		addr_offset = 0x2900;
	else if (chip_type_id == chip_a9)
		addr_offset = 0x1a00;

	WRITE_VPP_REG_BITS(OSD_PK_MODE - addr_offset, reg_pk_nor_rsft_mode, 20, 2);
	WRITE_VPP_REG_BITS(OSD_PK_FINAL_GAIN - addr_offset, reg_pk_dir_final_gain, 24, 8);
	WRITE_VPP_REG_BITS(OSD_PK_FINAL_GAIN - addr_offset, reg_pk_cir_final_gain, 16, 8);
	WRITE_VPP_REG_BITS(OSD_PK_FINAL_GAIN - addr_offset, reg_pk_final_pgain, 8, 8);
	WRITE_VPP_REG_BITS(OSD_PK_FINAL_GAIN - addr_offset, reg_pk_final_ngain, 0, 8);
}

static unsigned int pre_vsr_pk_final_gain;

void lc_evc_pq_settings(unsigned int lc_evc_src, int vpp_index)
{
	unsigned int reg_addr = VPP_PK_FINAL_GAIN;
	unsigned int val = 0;

	if (lc_evc_src) {
		pre_vsr_pk_final_gain = READ_VPP_REG(reg_addr) & 0xffff;
		val |= (pre_vsr_pk_final_gain & 0xff) >> 1;
		val |= ((pre_vsr_pk_final_gain & 0xff00) >> 9) << 8;
	} else {
		val = pre_vsr_pk_final_gain;
	}

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(reg_addr, val, 0, 16, vpp_index);
}

void amve_lc_evc_ctrl(unsigned int enable, unsigned int lc_evc_src)
{

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	enum vpp_muxio_sel_e sel = VPP_MUXIO_SEL_VD1_HDR;
	unsigned int val = 0;
#endif

	if (chip_type_id == chip_a9)
		return;

	if (flag_lc_evc == enable &&
		flag_lc_evc_src == lc_evc_src)
		return;

	pr_amve_dbg("[lc_evc_ctrl] set flag_lc_evc %d-->%d, flag_lc_evc_src %d-->%d\n",
		flag_lc_evc, enable, flag_lc_evc_src, lc_evc_src);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (chip_type_id == chip_t6w || chip_type_id == chip_t6x) {
		if (dpss_mode)
			return;

		if (lc_evc_src) {
			val = 1;
			sel = VPP_MUXIO_SEL_LC_EVC_HDR;
		} else {
			val = 0;
			sel = VPP_MUXIO_SEL_VD1_HDR;
		}

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(0x1a73, val, 25, 1, 0);
		muxio_config(sel, 1, 0);

		flag_lc_evc = enable;
		flag_lc_evc_src = lc_evc_src;

		lc_evc_pq_settings(lc_evc_src, VPP_TOP0);

		return;
	}
#endif

	if (enable) {
		flag_lc_evc = 1;
		vpp_contrast_adj_by_uv(cur_contrast_u, cur_contrast_v, cur_vpp_index);
	} else {
		flag_lc_evc = 0;
		vpp_contrast_adj_by_uv(cur_contrast_u, cur_contrast_v, cur_vpp_index);
	}

	flag_lc_evc_src = lc_evc_src;
}

void init_pq_rdma_part_ins(void)
{
	int i;
	int lut3d_reg_size = 0;

	if (pq_rdma_init)
		return;

	for (i = 0; i < RDMA_TABLE_NUM_MAX; i++)
		pq_rdma_part_ins[0].table_index = -1;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (ct_en || lut3d_en) {
		if (chip_type_id == chip_t6d)
			lut3d_reg_size = LUT3D_SINGLE_SIZE9 * 2;
		else
			lut3d_reg_size = LUT3D_SINGLE_SIZE * 2;
	} else {
		lut3d_reg_size = 0;
	}
#endif

	/*hdr table0*/
	pq_rdma_part_ins[0].vpp_index = RDMA_VPP0;
	pq_rdma_part_ins[0].table_index = AMVECM_PARTITION_TABLE_0;
	pq_rdma_part_ins[0].flag = use_rdma_part_tables;
	pq_rdma_part_ins[0].max_reg_cnt = 3180;
	pq_rdma_part_ins[0].reg_range_check = false;
	pq_rdma_part_ins[0].check_start_addr = 0x3800;
	pq_rdma_part_ins[0].check_end_addr = 0x5b8f;
	rdma_part_table_register(&pq_rdma_part_ins[0]);

	/*sr safa pi table1*/
	pq_rdma_part_ins[1].vpp_index = RDMA_VPP0;
	pq_rdma_part_ins[1].table_index = AMVECM_PARTITION_TABLE_1;
	pq_rdma_part_ins[1].flag = use_rdma_part_tables;
	pq_rdma_part_ins[1].max_reg_cnt = 2048;
	pq_rdma_part_ins[1].reg_range_check = false;
	pq_rdma_part_ins[1].check_start_addr = 0x3800;
	pq_rdma_part_ins[1].check_end_addr = 0x5b8f;
	rdma_part_table_register(&pq_rdma_part_ins[1]);

	/*vadj2 gainoff...*/
	pq_rdma_part_ins[2].vpp_index = RDMA_VPP0;
	pq_rdma_part_ins[2].table_index = AMVECM_PARTITION_TABLE_2;
	pq_rdma_part_ins[2].flag = use_rdma_part_tables;
	pq_rdma_part_ins[2].max_reg_cnt = 2048 + lut3d_reg_size;
	pq_rdma_part_ins[2].reg_range_check = false;
	pq_rdma_part_ins[2].check_start_addr = 0x3800;
	pq_rdma_part_ins[2].check_end_addr = 0x5b8f;
	rdma_part_table_register(&pq_rdma_part_ins[2]);

	/*lc dnlp*/
	pq_rdma_part_ins[3].vpp_index = RDMA_VPP0;
	pq_rdma_part_ins[3].table_index = AMVECM_PARTITION_TABLE_3;
	pq_rdma_part_ins[3].flag = use_rdma_part_tables;
	pq_rdma_part_ins[3].max_reg_cnt = 2500;
	pq_rdma_part_ins[3].reg_range_check = false;
	pq_rdma_part_ins[3].check_start_addr = 0x3800;
	pq_rdma_part_ins[3].check_end_addr = 0x5b8f;
	rdma_part_table_register(&pq_rdma_part_ins[3]);

	pq_rdma_init = true;
}

void lcd_post2_matrix(void)
{
	WRITE_VPP_REG(VPP_POST2_MATRIX_COEF00_01, 0x04ac0000);
	WRITE_VPP_REG(VPP_POST2_MATRIX_COEF02_10, 0x073104ac);
	WRITE_VPP_REG(VPP_POST2_MATRIX_COEF11_12, 0x1f251ddd);
	WRITE_VPP_REG(VPP_POST2_MATRIX_COEF20_21, 0x04ac0879);
	WRITE_VPP_REG(VPP_POST2_MATRIX_COEF22, 0x0);
	WRITE_VPP_REG(VPP_POST2_MATRIX_OFFSET0_1, 0x0);
	WRITE_VPP_REG(VPP_POST2_MATRIX_OFFSET2, 0x0);
	WRITE_VPP_REG(VPP_POST2_MATRIX_PRE_OFFSET0_1, 0x7c00600);
	WRITE_VPP_REG(VPP_POST2_MATRIX_PRE_OFFSET2, 0x00000600);
	WRITE_VPP_REG(VPP_POST2_MATRIX_EN_CTRL, 0x1);
}

void amve_sharpness_sub_vsync_ctrl(unsigned int enable, int vpp_index)
{
	pr_amve_dbg("\n[%s] enable=%d\n",
		__func__, enable);

	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_SR_EN,
		enable, 0, 1, vpp_index);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_PK_EN,
		enable, 0, 1, vpp_index);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_HTI_EN_MODE,
		enable, 12, 1, vpp_index);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_HTI_EN_MODE,
		enable, 4, 1, vpp_index);
	if (chip_type_id == chip_t6x) {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VTI_EN,
			enable, 20, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VTI_EN,
			enable, 16, 1, vpp_index);
	} else {
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VTI_EN,
			enable, 8, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_VTI_EN,
			enable, 4, 1, vpp_index);
	}
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_DERING_EN,
		enable, 0, 1, vpp_index);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_NR_LPF_EN,
		enable, 24, 1, vpp_index);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_NR_LPF_EN,
		enable, 16, 1, vpp_index);
	VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(VPP_NR_LPF_EN,
		enable, 12, 1, vpp_index);
}

void amve_old_sharpness_sub_vsync_ctrl(unsigned int enable, int vpp_index)
{
	unsigned int drtlpf_config;
	unsigned int s1_sr0_addr = 0x7527;
	unsigned int s1_sr1_addr = 0x7727;

	if (enable) {
		/* sharpness on */
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_PK_NR_ENABLE,
				pq_cfg.sharpness0_en, 1, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_PK_NR_ENABLE,
				pq_cfg.sharpness1_en, 1, 1, vpp_index);

		if (chip_type_id == chip_t3x) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(s1_sr0_addr,
				pq_cfg.sharpness0_en, 1, 1, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(s1_sr1_addr,
				pq_cfg.sharpness1_en, 1, 1, vpp_index);
		}

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_HCTI_FLT_CLP_DC,
				pq_cfg.sharpness0_en, 28, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_HCTI_FLT_CLP_DC,
				pq_cfg.sharpness1_en, 28, 1, vpp_index);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_HLTI_FLT_CLP_DC,
				pq_cfg.sharpness0_en, 28, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_HLTI_FLT_CLP_DC,
				pq_cfg.sharpness1_en, 28, 1, vpp_index);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_VLTI_FLT_CON_CLP,
				pq_cfg.sharpness0_en, 14, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_VLTI_FLT_CON_CLP,
				pq_cfg.sharpness1_en, 14, 1, vpp_index);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_VCTI_FLT_CON_CLP,
				pq_cfg.sharpness0_en, 14, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_VCTI_FLT_CON_CLP,
				pq_cfg.sharpness1_en, 14, 1, vpp_index);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_DEJ_CTRL,
					pq_cfg.sharpness0_en, 0, 1, vpp_index);

			drtlpf_config = pq_cfg.sharpness0_en ? 0x7 : 0x0;
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_SR3_DRTLPF_EN,
					drtlpf_config, 0, 3, vpp_index);

			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_SR3_DERING_CTRL,
					pq_cfg.sharpness0_en, 28, 3, vpp_index);

			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_DEJ_CTRL,
					pq_cfg.sharpness1_en, 0, 1, vpp_index);

			drtlpf_config = pq_cfg.sharpness1_en ? 0x7 : 0x0;
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_SR3_DRTLPF_EN,
					drtlpf_config, 0, 3, vpp_index);

			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_SR3_DERING_CTRL,
					pq_cfg.sharpness1_en, 28, 3, vpp_index);
		}
	} else {
		/* sharpness on */
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_PK_NR_ENABLE,
				0, 1, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_PK_NR_ENABLE,
				0, 1, 1, vpp_index);

		if (chip_type_id == chip_t3x) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(s1_sr0_addr,
				0, 1, 1, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(s1_sr1_addr,
				0, 1, 1, vpp_index);
		}

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_HCTI_FLT_CLP_DC,
				0, 28, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_HCTI_FLT_CLP_DC,
				0, 28, 1, vpp_index);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_HLTI_FLT_CLP_DC,
				0, 28, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_HLTI_FLT_CLP_DC,
				0, 28, 1, vpp_index);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_VLTI_FLT_CON_CLP,
				0, 14, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_VLTI_FLT_CON_CLP,
				0, 14, 1, vpp_index);

		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_VCTI_FLT_CON_CLP,
				0, 14, 1, vpp_index);
		VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_VCTI_FLT_CON_CLP,
				0, 14, 1, vpp_index);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TXL)) {
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_DEJ_CTRL,
					0, 0, 1, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_SR3_DRTLPF_EN,
					0, 0, 3, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP0_SR3_DERING_CTRL,
					0, 28, 3, vpp_index);

			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_DEJ_CTRL,
					0, 0, 1, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_SR3_DRTLPF_EN,
					0, 0, 3, vpp_index);
			VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(SRSHARP1_SR3_DERING_CTRL,
					0, 28, 3, vpp_index);
		}
	}
}

int get_skin_api(void)
{
	unsigned int value = 0;
	unsigned int value1 = 0;
	unsigned int value2 = 0;
	int temp = 0;

	if (chip_type_id == chip_t6d) {
		value = READ_VPP_REG_BITS(VPP_LC_MODE, 4, 1);
		value1 = READ_VPP_REG_BITS(VPP_DNLP_EN_MODE, 4, 1);
		value2 = READ_VPP_REG_BITS(VPP_PK_EN, 0, 1);

		if (value || value1 || value2)
			temp = 1;
		else
			temp = 0;
	}

	return temp;
}

void set_skin_api(int en)
{
	if (en) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		lc_en = 1;
#endif
		WRITE_VPP_REG_BITS(VPP_DNLP_EN_MODE, 1, 4, 1);
		WRITE_VPP_REG_BITS(VPP_PK_EN, 1, 0, 1);
		WRITE_VPP_REG_BITS(VPP_SR_EN, 1, 0, 1);
	} else {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		lc_en = 0;
#endif
		WRITE_VPP_REG_BITS(VPP_DNLP_EN_MODE, 0, 4, 1);
		WRITE_VPP_REG_BITS(VPP_SR_EN, 0, 0, 1);
		WRITE_VPP_REG_BITS(VPP_PK_EN, 0, 0, 1);
	}
}

void c5_vadj_gamma_init(void)
{
	if (chip_type_id != chip_c5)
		return;

	/*vadj curve init*/
	WRITE_VPP_REG(VOUT_VADJ_CURV_0, 0x80040000);
	WRITE_VPP_REG(VOUT_VADJ_CURV_1, 0x5);
	WRITE_VPP_REG(VOUT_VADJ_CURV_2, 0x891E0240);
	WRITE_VPP_REG(VOUT_VADJ_CURV_3, 0x2);

	/*en latch*/
	WRITE_VPP_REG_BITS(VOUT_VADJ_MISC, 1, 1, 1);
	WRITE_VPP_REG_BITS(VOUT_VADJ_MISC, 1, 2, 1);
	WRITE_VPP_REG_BITS(VOUT_VADJ_MISC, 1, 7, 1);

	WRITE_VPP_REG_BITS(VOUT_MATRIX_EN_CTRL, 1, 6, 1);
	WRITE_VPP_REG_BITS(VOUT_MATRIX_RGB_CTRST, 1, 13, 1);

	/*gamma*/
	WRITE_VPP_REG_BITS(LCD_GAMMA_CNTL_PORT0_A4, 1, 1, 1);
	WRITE_VPP_REG_BITS(LCD_GAMMA_CNTL_PORT0_A4, 1, 6, 1);
}

