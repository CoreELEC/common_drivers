// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/amlogic/major.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/utils/amstream.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/debugfs.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/sched.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"
#include "video_hw_s5.h"
#include "video_reg_s5.h"
#include "vpp_post_s5.h"
#include "video_common.h"
#include "video_priv.h"

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/utils/vdec_reg.h>

#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "videolog.h"

#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include "../common/vfm/vfm.h"
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include "video_receiver.h"
#ifdef CONFIG_AMLOGIC_MEDIA_LUT_DMA
#include <linux/amlogic/media/lut_dma/lut_dma.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif
#include <linux/amlogic/media/video_sink/video_signal_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
#include <linux/amlogic/media/frc/frc_common.h>
#endif

unsigned int debug_common_flag;
unsigned int aisr_size_threshold = 50;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct vd_proc_info_t *get_vd_proc_amdv_info(void)
{
	return &vd_proc_amdv;
}

struct vd_proc_amvecm_info_t *get_vd_proc_amvecm_info(void)
{
	return &vd_proc_amvecm;
}
#endif

u32 is_crop_left_odd(struct vpp_frame_par_s *frame_par)
{
	int crop_left_odd;

	/* odd, not even aligned*/
	if (frame_par->VPP_hd_start_lines_ & 0x01)
		crop_left_odd = 1;
	else
		crop_left_odd = 0;
	return crop_left_odd;
}

void get_pre_hscaler_para(u8 layer_id, int *ds_ratio, int *flt_num)
{
	switch (pre_scaler[layer_id].pre_hscaler_ntap) {
	case PRE_HSCALER_2TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 2;
		break;
	case PRE_HSCALER_4TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 4;
		break;
	case PRE_HSCALER_6TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 6;
		break;
	case PRE_HSCALER_8TAP:
		*ds_ratio = pre_scaler[layer_id].pre_hscaler_rate;
		*flt_num = 8;
		break;
	}
}

void get_pre_vscaler_para(u8 layer_id, int *ds_ratio, int *flt_num)
{
	switch (pre_scaler[layer_id].pre_vscaler_ntap) {
	case PRE_VSCALER_2TAP:
		*ds_ratio = pre_scaler[layer_id].pre_vscaler_rate;
		*flt_num = 2;
		break;
	case PRE_VSCALER_4TAP:
		*ds_ratio = pre_scaler[layer_id].pre_vscaler_rate;
		*flt_num = 4;
		break;
	}
}

void get_pre_hscaler_coef(u8 layer_id, int *pre_hscaler_table)
{
	if (pre_scaler[layer_id].pre_hscaler_coef_set) {
		pre_hscaler_table[0] = pre_scaler[layer_id].pre_hscaler_coef[0];
		pre_hscaler_table[1] = pre_scaler[layer_id].pre_hscaler_coef[1];
		pre_hscaler_table[2] = pre_scaler[layer_id].pre_hscaler_coef[2];
		pre_hscaler_table[3] = pre_scaler[layer_id].pre_hscaler_coef[3];
	} else {
		switch (pre_scaler[layer_id].pre_hscaler_ntap) {
		case PRE_HSCALER_2TAP:
			pre_hscaler_table[0] = 0x100;
			pre_hscaler_table[1] = 0x0;
			pre_hscaler_table[2] = 0x0;
			pre_hscaler_table[3] = 0x0;
			break;
		case PRE_HSCALER_4TAP:
			pre_hscaler_table[0] = 0xc0;
			pre_hscaler_table[1] = 0x40;
			pre_hscaler_table[2] = 0x0;
			pre_hscaler_table[3] = 0x0;
			break;
		case PRE_HSCALER_6TAP:
			pre_hscaler_table[0] = 0x9c;
			pre_hscaler_table[1] = 0x44;
			pre_hscaler_table[2] = 0x20;
			pre_hscaler_table[3] = 0x0;
			break;
		case PRE_HSCALER_8TAP:
			pre_hscaler_table[0] = 0x90;
			pre_hscaler_table[1] = 0x40;
			pre_hscaler_table[2] = 0x20;
			pre_hscaler_table[3] = 0x10;
			break;
		}
	}
}

void get_pre_vscaler_coef(u8 layer_id, int *pre_hscaler_table)
{
	if (pre_scaler[layer_id].pre_vscaler_coef_set) {
		pre_hscaler_table[0] = pre_scaler[layer_id].pre_vscaler_coef[0];
		pre_hscaler_table[1] = pre_scaler[layer_id].pre_vscaler_coef[1];
	} else {
		switch (pre_scaler[layer_id].pre_vscaler_ntap) {
		case PRE_VSCALER_2TAP:
			if (has_pre_hscaler_8tap(0)) {
				pre_hscaler_table[0] = 0x100;
				pre_hscaler_table[1] = 0x0;
			} else {
				pre_hscaler_table[0] = 0x40;
				pre_hscaler_table[1] = 0x0;
			}
			break;
		case PRE_VSCALER_4TAP:
			if (has_pre_hscaler_8tap(0)) {
				pre_hscaler_table[0] = 0xc0;
				pre_hscaler_table[1] = 0x40;
			} else {
				pre_hscaler_table[0] = 0xf8;
				pre_hscaler_table[1] = 0x48;
			}
			break;
		}
	}
}

u32 viu_line_stride(u32 buffr_width)
{
	u32 line_stride;

	/* input: buffer width not hsize */
	/* 1 stride = 16 byte */
	line_stride = (buffr_width + 15) / 16;
	return line_stride;
}

u32 viu_line_stride_ex(u32 buffr_width, u8 plane_bits)
{
	u32 line_stride;

	/* input: buffer width not hsize */
	/* 1 stride = 16 byte */
	line_stride = (((buffr_width * plane_bits + 127) / 128 + 3) >> 2) << 2;
	return line_stride;
}

void init_layer_canvas(struct video_layer_s *layer,
			      u32 start_canvas)
{
	u32 i, j;

	if (!layer)
		return;

	for (i = 0; i < CANVAS_TABLE_CNT; i++) {
		for (j = 0; j < 6; j++)
			layer->canvas_tbl[i][j] =
				start_canvas++;
		layer->disp_canvas[i][0] =
			(layer->canvas_tbl[i][2] << 16) |
			(layer->canvas_tbl[i][1] << 8) |
			layer->canvas_tbl[i][0];
		layer->disp_canvas[i][1] =
			(layer->canvas_tbl[i][5] << 16) |
			(layer->canvas_tbl[i][4] << 8) |
			layer->canvas_tbl[i][3];
	}
}

void vframe_canvas_set(struct canvas_config_s *config,
	u32 planes,
	u32 *index)
{
	int i;
	u32 *canvas_index = index;

	struct canvas_config_s *cfg = config;

	for (i = 0; i < planes; i++, canvas_index++, cfg++)
		canvas_config_config(*canvas_index, cfg);
}

bool is_layer_aisr_supported(struct video_layer_s *layer)
{
	/* only vd1 has aisr for t3 */
	if (!cur_dev->aisr_support ||
		!layer || layer->layer_id != 0)
		return false;
	else
		return true;
}

#define MIF_REG_CNT 23
#define MIF_LINEAR_REG_CNT 10
static void dump_mif_reg(void)
{
	int i, j = 0;
	u32 reg_addr, reg_val = 0;
	bool skip = false;
	u32 mif_reg[MAX_VD_LAYER][MIF_REG_CNT];
	u32 mif_linear_reg[MAX_VD_LAYER][MIF_LINEAR_REG_CNT];

	if (cur_dev->display_module == T7_DISPLAY_MODULE ||
		cur_dev->display_module == C3_DISPLAY_MODULE) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (!video_is_meson_t6d_cpu())
#endif
			skip = true;
	}
	if (!skip) {
		reg_addr = VD1_AFBCD0_MISC_CTRL;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = VD2_AFBCD1_MISC_CTRL;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = VIU_MISC_CTRL0;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_gen_reg;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_canvas0;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_canvas1;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma_x0;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma_y0;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_chroma_x0;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_chroma_y0;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma_x1;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma_y1;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_chroma_x1;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_chroma_y1;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_rpt_loop;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma0_rpt_pat;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_chroma0_rpt_pat;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma1_rpt_pat;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_chroma1_rpt_pat;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma_psel;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_chroma_psel;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_luma_fifo_size;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_gen_reg2;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.vd_if0_gen_reg3;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.viu_vd_fmt_ctrl;
		mif_reg[i][j++] = vd_layer[i].vd_mif_reg.viu_vd_fmt_w;
		j = 0;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_baddr_y;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_baddr_cb;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_baddr_cr;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_stride_0;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_stride_1;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_baddr_y_f1;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_baddr_cb_f1;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_baddr_cr_f1;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_stride_0_f1;
		mif_linear_reg[i][j++] = vd_layer[i].vd_mif_linear_reg.vd_if0_stride_1_f1;
		j = 0;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		pr_info("vd%d mif regs:\n", i);
		for (j = 0; j < MIF_REG_CNT; j++) {
			reg_addr = mif_reg[i][j];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
		if (cur_dev->mif_linear) {
			pr_info("vd%d mif linear regs:\n", i);
			for (j = 0; j < MIF_LINEAR_REG_CNT; j++) {
				reg_addr = mif_linear_reg[i][j];
				if (!reg_addr)
					continue;
				reg_val = READ_VCBUS_REG(reg_addr);
				pr_info("[0x%x] = 0x%X\n",
					   reg_addr, reg_val);
			}
		}
	}
}

#define AFBC_REG_CNT 18

static void dump_afbc_reg(void)
{
	int i, j = 0;
	u32 reg_addr, reg_val = 0;
	u32 afbc_reg[MAX_VD_LAYER][AFBC_REG_CNT];

	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		if (!glayer_info[i].afbc_support)
			continue;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_enable;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_mode;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_size_in;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_dec_def_color;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_conv_ctrl;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_lbuf_depth;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_head_baddr;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_body_baddr;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_size_out;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_out_yscope;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_vd_cfmt_ctrl;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_vd_cfmt_w;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_mif_hor_scope;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_mif_ver_scope;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_pixel_hor_scope;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_pixel_ver_scope;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_vd_cfmt_h;
		afbc_reg[i][j++] = vd_layer[i].vd_afbc_reg.afbc_top_ctrl;
		j = 0;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		if (!glayer_info[i].afbc_support)
			continue;
		pr_info("vd%d afbc regs:\n", i);
		for (j = 0; j < AFBC_REG_CNT; j++) {
			/*coverity[UNINIT]*/
			reg_addr = afbc_reg[i][j];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
	}
}

#define PPS_REG_CNT 21

static void dump_pps_reg(bool mosaic_mode)
{
	u32 i, j = 0;
	u32 pps_reg[MAX_VD_LAYER][PPS_REG_CNT];
	u32 mosaic_pps_reg[SLICE_NUM][PPS_REG_CNT];
	u32 reg_addr, reg_val = 0;
#if DUMP_REG_NAME
	static const char * const reg_name[] = {
		"vd_vsc_region12_startp",
		"vd_vsc_region34_startp",
		"vd_vsc_region4_endp",
		"vd_vsc_start_phase_step",
		"vd_vsc_region1_phase_slope",
		"vd_vsc_region3_phase_slope",
		"vd_vsc_phase_ctrl",
		"vd_vsc_init_phase",
		"vd_hsc_region12_startp",
		"vd_hsc_region34_startp",
		"vd_hsc_region4_endp",
		"vd_hsc_start_phase_step",
		"vd_hsc_region1_phase_slope",
		"vd_hsc_region3_phase_slope",
		"vd_hsc_phase_ctrl",
		"vd_sc_misc",
		"vd_hsc_phase_ctrl1",
		"vd_prehsc_coef",
		"vd_pre_scale_ctrl",
		"vd_prevsc_coef",
		"vd_prehsc_coef1",
	};
#endif
	if (mosaic_mode) {
		for (i = 0; i < SLICE_NUM; i++) {
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_region12_startp;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_region34_startp;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_region4_endp;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_start_phase_step;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_region1_phase_slope;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_region3_phase_slope;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_phase_ctrl;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_vsc_init_phase;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_region12_startp;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_region34_startp;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_region4_endp;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_start_phase_step;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_region1_phase_slope;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_region3_phase_slope;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_phase_ctrl;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_sc_misc;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_hsc_phase_ctrl1;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_prehsc_coef;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_pre_scale_ctrl;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_prevsc_coef;
			mosaic_pps_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.pps_reg.vd_prehsc_coef1;
			j = 0;
		}
		for (i = 0; i < SLICE_NUM; i++) {
			pr_info("mosaic slice%d pps regs:\n", i);
			for (j = 0; j < PPS_REG_CNT; j++) {
				reg_addr = mosaic_pps_reg[i][j];
				if (!reg_addr)
					continue;
				reg_val  = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
				pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, reg_name[j]);
#else
				pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
			}
		}
		return;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_region12_startp;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_region34_startp;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_region4_endp;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_start_phase_step;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_region1_phase_slope;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_region3_phase_slope;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_phase_ctrl;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_vsc_init_phase;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_region12_startp;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_region34_startp;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_region4_endp;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_start_phase_step;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_region1_phase_slope;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_region3_phase_slope;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_phase_ctrl;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_sc_misc;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_hsc_phase_ctrl1;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_prehsc_coef;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_pre_scale_ctrl;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_prevsc_coef;
		pps_reg[i][j++] = vd_layer[i].pps_reg.vd_prehsc_coef1;
		j = 0;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (cur_dev->vd1_vsr_safa_support && i == 0) {
			dump_vd_vsr_safa_reg();
			dump_vd_vsr_safa_nonlinear_reg();
			continue;
		}
#endif
		pr_info("vd%d pps regs:\n", i);
		for (j = 0; j < PPS_REG_CNT; j++) {
			reg_addr = pps_reg[i][j];
			if (!reg_addr)
				continue;
			reg_val  = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
			   reg_addr, reg_val, reg_name[j]);
#else
			pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
#endif
		}
	}
}

#define VPP_BLEND_REG_CNT 5

static void dump_vpp_blend_reg(void)
{
	int i, j = 0;
	u32 reg_addr, reg_val = 0;
	u32 vpp_blend_reg[MAX_VD_LAYERS][VPP_BLEND_REG_CNT];
#if DUMP_REG_NAME
	static const char * const reg_name[] = {
		"preblend_h_start_end",
		"preblend_v_start_end",	"preblend_h_size",
		"postblend_h_start_end", "postblend_v_start_end"
	};
#endif

	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		vpp_blend_reg[i][0] = vd_layer[i].vpp_blend_reg.preblend_h_start_end;
		vpp_blend_reg[i][1] = vd_layer[i].vpp_blend_reg.preblend_v_start_end;
		vpp_blend_reg[i][2] = vd_layer[i].vpp_blend_reg.preblend_h_size;
		vpp_blend_reg[i][3] = vd_layer[i].vpp_blend_reg.postblend_h_start_end;
		vpp_blend_reg[i][4] = vd_layer[i].vpp_blend_reg.postblend_v_start_end;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		pr_info("vd%d pps blend regs:\n", i);
		for (j = 0; j < VPP_BLEND_REG_CNT; j++) {
			reg_addr = vpp_blend_reg[i][j];
			reg_val  = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
			   reg_addr, reg_val, reg_name[j]);
#else
			pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
#endif
		}
	}
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#define T6W_MIF_REG_CNT 50

static void dump_mif_reg_t6w(bool mosaic_mode)
{
	int i, j = 0, k = 0;
	u32 **t6w_mif_reg = NULL;
	u32 **mosaic_mif_reg = NULL;
	u32 reg_addr, reg_val = 0;
#if DUMP_REG_NAME
	static const char * const reg_name[] = {
		"vfcd_top_ctrl0",
		"vfcd_top_ctrl2",
		"rmif_top_ctrl",
		"mif0_rmif_ctrl1",
		"mif0_rmif_ctrl2",
		"mif0_rmif_ctrl3",
		"mif0_rmif_ctrl4",
		"mif0_rmif_ctrl5",
		"mif1_rmif_ctrl1",
		"mif1_rmif_ctrl2",
		"mif1_rmif_ctrl3",
		"mif1_rmif_ctrl4",
		"mif1_rmif_ctrl5",
		"mif2_rmif_ctrl",
		"mif2_rmif_baddr",
		"mif2_rmif_stride",
		"luma_rmif_scope_x",
		"luma_rmif_scope_y",
		"chrm_rmif_scope_x",
		"chrm_rmif_scope_y",
		"rmif_dummy_pixel",
		"rmif_rpt_loop",
		"rmif_f0_luma_rpt_pat",
		"rmif_f0_chrm_rpt_pat",
		"rmif_f1_luma_rpt_pat",
		"rmif_f1_chrm_rpt_pat",
		"rmif_f1_luma_ysize",
		"rmif_f1_chrm_ysize",
		"rmif_f1_mif0_baddr",
		"rmif_f1_mif1_baddr",
		"rmif_f1_mif2_baddr",
		"rmif_f1_stride0",
		"rmif_f1_stride1",
		"rmif_fifo_size",
		"rmif_chrm_psel",
		"vfcd_afbc_conv_ctrl",
		"vfcd_afbc_vd_cfmt_ctrl",
		"vfcd_afbc_vd_cfmt_w",
		"vfcd_afbc_vd_cfmt_h",
		"vfcd_post_luma_size",
		"vfcd_conv_buf_lens",
		"vfcd_afbc_lbuf_depth",
		"vfcd_luma_pic_xpos",
		"vfcd_luma_pic_ypos",
		"vfcd_chrm_pic_xpos",
		"vfcd_chrm_pic_ypos",
		"vfcd_mmu_ctrl",
		"vfcd_mmu_baddr0",
		"vfcd_mmu_baddr1",
		"vfcd_mmu_baddr2",
	};
#endif

	if (mosaic_mode) {
		mosaic_mif_reg = kmalloc_array(SLICE_NUM, sizeof(u32 *), GFP_KERNEL);
		if (!mosaic_mif_reg)
			return;
		for (i = 0; i < SLICE_NUM; i++) {
			mosaic_mif_reg[i] = kmalloc_array(T6W_MIF_REG_CNT, sizeof(u32), GFP_KERNEL);
			if (!mosaic_mif_reg[i]) {
				for (k = 0; k < i; k++)
					kfree(mosaic_mif_reg[k]);
				kfree(mosaic_mif_reg);
				return;
			}
		}
		for (i = 0; i < SLICE_NUM; i++) {
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_top_ctrl0;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_top_ctrl2;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_top_ctrl;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif0_rmif_ctrl1;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif0_rmif_ctrl2;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif0_rmif_ctrl3;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif0_rmif_ctrl4;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif0_rmif_ctrl5;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif1_rmif_ctrl1;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif1_rmif_ctrl2;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif1_rmif_ctrl3;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif1_rmif_ctrl4;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif1_rmif_ctrl5;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif2_rmif_ctrl;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif2_rmif_baddr;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.mif2_rmif_stride;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.luma_rmif_scope_x;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.luma_rmif_scope_y;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.chrm_rmif_scope_x;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.chrm_rmif_scope_y;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_dummy_pixel;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_rpt_loop;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f0_luma_rpt_pat;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f0_chrm_rpt_pat;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_luma_rpt_pat;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_chrm_rpt_pat;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_luma_ysize;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_chrm_ysize;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_mif0_baddr;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_mif1_baddr;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_mif2_baddr;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_stride0;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_f1_stride1;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_fifo_size;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.rmif_chrm_psel;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_afbc_conv_ctrl;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_afbc_vd_cfmt_ctrl;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_afbc_vd_cfmt_w;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_afbc_vd_cfmt_h;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_post_luma_size;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_conv_buf_lens;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_afbc_lbuf_depth;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_luma_pic_xpos;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_luma_pic_ypos;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_chrm_pic_xpos;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_chrm_pic_ypos;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_mmu_ctrl;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_mmu_baddr0;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_mmu_baddr1;
			mosaic_mif_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_mif_reg.vfcd_mmu_baddr2;
			j = 0;
		}
		for (i = 0; i < SLICE_NUM; i++) {
			pr_info("mosaic slice%d mif regs:\n", i);
			for (j = 0; j < T6W_MIF_REG_CNT; j++) {
				reg_addr = mosaic_mif_reg[i][j];
				if (!reg_addr)
					continue;
				reg_val  = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
				pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, reg_name[j]);
#else
				pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
			}
		}
		for (i = 0; i < SLICE_NUM; i++)
			kfree(mosaic_mif_reg[i]);
		kfree(mosaic_mif_reg);
		return;
	}
	reg_addr = VPU_AXIRD_PATH_CTRL;
	reg_val = READ_VCBUS_REG(VPU_AXIRD_PATH_CTRL);
	pr_info("[0x%x] = 0x%X [VPU_AXIRD_PATH_CTRL]\n",
		   reg_addr, reg_val);
	reg_addr = VPU_AXIRD_TUNNEL_CTRL1;
	reg_val = READ_VCBUS_REG(VPU_AXIRD_TUNNEL_CTRL1);
	pr_info("[0x%x] = 0x%X [VPU_AXIRD_TUNNEL_CTRL1]\n",
		   reg_addr, reg_val);
	t6w_mif_reg = kmalloc_array(cur_dev->max_vd_layers, sizeof(u32 *), GFP_KERNEL);
	if (!t6w_mif_reg)
		return;
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		t6w_mif_reg[i] = kmalloc_array(T6W_MIF_REG_CNT, sizeof(u32), GFP_KERNEL);
		if (!t6w_mif_reg[i]) {
			for (k = 0; k < i; k++)
				kfree(t6w_mif_reg[k]);
			kfree(t6w_mif_reg);
			return;
		}
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_top_ctrl0;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_top_ctrl2;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_top_ctrl;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif0_rmif_ctrl1;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif0_rmif_ctrl2;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif0_rmif_ctrl3;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif0_rmif_ctrl4;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif0_rmif_ctrl5;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif1_rmif_ctrl1;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif1_rmif_ctrl2;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif1_rmif_ctrl3;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif1_rmif_ctrl4;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif1_rmif_ctrl5;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif2_rmif_ctrl;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif2_rmif_baddr;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.mif2_rmif_stride;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.luma_rmif_scope_x;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.luma_rmif_scope_y;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.chrm_rmif_scope_x;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.chrm_rmif_scope_y;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_dummy_pixel;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_rpt_loop;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f0_luma_rpt_pat;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f0_chrm_rpt_pat;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_luma_rpt_pat;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_chrm_rpt_pat;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_luma_ysize;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_chrm_ysize;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_mif0_baddr;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_mif1_baddr;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_mif2_baddr;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_stride0;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_f1_stride1;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_fifo_size;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.rmif_chrm_psel;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_afbc_conv_ctrl;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_afbc_vd_cfmt_ctrl;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_afbc_vd_cfmt_w;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_afbc_vd_cfmt_h;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_post_luma_size;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_conv_buf_lens;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_afbc_lbuf_depth;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_luma_pic_xpos;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_luma_pic_ypos;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_chrm_pic_xpos;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_chrm_pic_ypos;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_mmu_ctrl;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_mmu_baddr0;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_mmu_baddr1;
		t6w_mif_reg[i][j++] = vd_layer[i].vd_hw_mif_reg.vfcd_mmu_baddr2;
		j = 0;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		pr_info("vd%d mif regs:\n", i);
		for (j = 0; j < T6W_MIF_REG_CNT; j++) {
			reg_addr = t6w_mif_reg[i][j];
			if (!reg_addr)
				continue;
			reg_val  = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
			   reg_addr, reg_val, reg_name[j]);
#else
			pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
#endif
		}
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++)
		kfree(t6w_mif_reg[i]);
	kfree(t6w_mif_reg);
}

#define T6W_VFCD_REG_CNT 48

static void dump_vfcd_reg_t6w(bool mosaic_mode)
{
	int i, j = 0, k = 0;
	u32 **t6w_vfcd_reg = NULL;
	u32 **mosaic_vfcd_reg = NULL;
	u32 reg_addr, reg_val = 0;
#if DUMP_REG_NAME
	static const char * const reg_name[] = {
		"vfcd_top_ctrl0",
		"vfcd_top_ctrl2",
		"vfcd_mmu_ctrl",
		"vfcd_afrc1_param",
		"vfcd_afbc_mode",
		"vfcd_afbc_lbuf_depth",
		"vfcd_afbc_conv_ctrl",
		"vfcd_frm_pic_size",
		"vfcd_luma_pic_xpos",
		"vfcd_luma_pic_ypos",
		"vfcd_afbc_vd_cfmt_ctrl",
		"vfcd_afbc_vd_cfmt_w",
		"vfcd_afbc_vd_cfmt_h",
		"vfcd_afbc_iquant_enable",
		"vfcd_afbc_burst_ctrl",
		"vfcd_afbc_dec_def_color",
		"vfcd_afbc_head_baddr",
		"vfcd_afbc_body_baddr",
		"vfcd_afbc_enable",
		"vfcd_chrm_pic_xpos",
		"vfcd_chrm_pic_ypos",
		"vfcd_slc1_luma_pic_x pos",
		"vfcd_slc1_chrm_pic_xpos",
		"vfcd_slc2_luma_pic_xpos",
		"vfcd_slc2_chrm_pic_xpos",
		"vfcd_slc3_luma_pic_xpos",
		"vfcd_slc3_chrm_pic_xpos",
		"vfcd_post_luma_size",
		"vfcd_post_luma_win_h",
		"vfcd_post_chrm_win_h",
		"vfcd_post_luma_win_v",
		"vfcd_post_chrm_win_v",
		"vfcd_luma_pad_size",
		"vfcd_luma_pad_ofst",
		"vfcd_chrm_pad_size",
		"vfcd_chrm_pad_ofst",
		"vfcd_pad_dumy_data",
		"vfcd_mif1_urgent",
		"vfcd_mmu_baddr0",
		"vfcd_mmu_baddr1",
		"vfcd_afrc_ctrl0",
		"vfcd_afrc0_ctrl",
		"vfcd_afrc_ctrl1",
		"vfcd_afrc1_ctrl",
		"vfcd_afbc_head_baddr",
		"vfcd_afbc_body_baddr",
		"vfcd_afrc_chrm_head_baddr",
		"vfcd_afrc_chrm_body_baddr",
	};
#endif

	if (mosaic_mode) {
		mosaic_vfcd_reg = kmalloc_array(SLICE_NUM, sizeof(u32 *), GFP_KERNEL);
		if (!mosaic_vfcd_reg)
			return;
		for (i = 0; i < SLICE_NUM; i++) {
			mosaic_vfcd_reg[i] =
				kmalloc_array(T6W_VFCD_REG_CNT, sizeof(u32), GFP_KERNEL);
			if (!mosaic_vfcd_reg[i]) {
				for (k = 0; k < i; k++)
					kfree(mosaic_vfcd_reg[k]);
				kfree(mosaic_vfcd_reg);
				return;
			}
		}
		for (i = 0; i < SLICE_NUM; i++) {
			if (!g_mosaic_frame[i].virtual_layer_info.afbc_support)
				continue;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_top_ctrl0;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_top_ctrl2;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_mmu_ctrl;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afrc1_param;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_mode;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_lbuf_depth;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_conv_ctrl;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_frm_pic_size;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_luma_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_luma_pic_ypos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_vd_cfmt_ctrl;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_vd_cfmt_w;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_vd_cfmt_h;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_iquant_enable;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_burst_ctrl;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_dec_def_color;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_head_baddr;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_body_baddr;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_enable;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_chrm_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_chrm_pic_ypos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_slc1_luma_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_slc1_chrm_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_slc2_luma_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_slc2_chrm_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_slc3_luma_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_slc3_chrm_pic_xpos;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_post_luma_size;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_post_luma_win_h;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_post_chrm_win_h;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_post_luma_win_v;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_post_chrm_win_v;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_luma_pad_size;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_luma_pad_ofst;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_chrm_pad_size;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_chrm_pad_ofst;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_pad_dumy_data;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_mif1_urgent;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_mmu_baddr0;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_mmu_baddr1;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afrc_ctrl0;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afrc0_ctrl;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afrc_ctrl1;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afrc1_ctrl;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_head_baddr;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afbc_body_baddr;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afrc_chrm_head_baddr;
			mosaic_vfcd_reg[i][j++] =
			g_mosaic_frame[i].virtual_layer.vd_hw_vfcd_reg.vfcd_afrc_chrm_body_baddr;
			j = 0;
		}
		for (i = 0; i < SLICE_NUM; i++) {
			pr_info("mosaic slice%d vfcd regs:\n", i);
			for (j = 0; j < T6W_VFCD_REG_CNT; j++) {
				reg_addr = mosaic_vfcd_reg[i][j];
				if (!reg_addr)
					continue;
				reg_val  = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
				pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, reg_name[j]);
#else
				pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
			}
		}
		for (i = 0; i < SLICE_NUM; i++)
			kfree(mosaic_vfcd_reg[i]);
		kfree(mosaic_vfcd_reg);
		return;
	}
	t6w_vfcd_reg = kmalloc_array(cur_dev->max_vd_layers, sizeof(u32 *), GFP_KERNEL);
	if (!t6w_vfcd_reg)
		return;
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		t6w_vfcd_reg[i] = kmalloc_array(T6W_VFCD_REG_CNT, sizeof(u32), GFP_KERNEL);
		if (!t6w_vfcd_reg[i]) {
			for (k = 0; k < i; k++)
				kfree(t6w_vfcd_reg[k]);
			kfree(t6w_vfcd_reg);
			return;
		}
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_top_ctrl0;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_top_ctrl2;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_mmu_ctrl;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afrc1_param;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_mode;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_lbuf_depth;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_conv_ctrl;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_frm_pic_size;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_luma_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_luma_pic_ypos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_vd_cfmt_ctrl;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_vd_cfmt_w;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_vd_cfmt_h;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_iquant_enable;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_burst_ctrl;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_dec_def_color;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_head_baddr;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_body_baddr;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_enable;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_chrm_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_chrm_pic_ypos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_slc1_luma_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_slc1_chrm_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_slc2_luma_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_slc2_chrm_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_slc3_luma_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_slc3_chrm_pic_xpos;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_post_luma_size;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_post_luma_win_h;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_post_chrm_win_h;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_post_luma_win_v;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_post_chrm_win_v;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_luma_pad_size;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_luma_pad_ofst;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_chrm_pad_size;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_chrm_pad_ofst;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_pad_dumy_data;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_mif1_urgent;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_mmu_baddr0;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_mmu_baddr1;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afrc_ctrl0;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afrc0_ctrl;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afrc_ctrl1;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afrc1_ctrl;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_head_baddr;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afbc_body_baddr;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afrc_chrm_head_baddr;
		t6w_vfcd_reg[i][j++] = vd_layer[i].vd_hw_vfcd_reg.vfcd_afrc_chrm_body_baddr;
		j = 0;
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		pr_info("vd%d vfcd regs:\n", i);
		for (j = 0; j < T6W_VFCD_REG_CNT; j++) {
			reg_addr = t6w_vfcd_reg[i][j];
			if (!reg_addr)
				continue;
			reg_val  = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
			   reg_addr, reg_val, reg_name[j]);
#else
			pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
#endif
		}
	}
	for (i = 0; i < cur_dev->max_vd_layers; i++)
		kfree(t6w_vfcd_reg[i]);
	kfree(t6w_vfcd_reg);
}

#define VOUT_BLEND_REG_CNT 13

static void dump_vout_blend_reg(void)
{
	u32 i = 0;
	u32 reg_addr, reg_val = 0;
	u32 vout_blend_reg[VOUT_BLEND_REG_CNT];
#if DUMP_REG_NAME
	static const char * const reg_name[] = {
		"VPU_VOUT_BLEND_CTRL",
		"VPU_VOUT_BLEND_DUMDATA",
		"VPU_VOUT_BLEND_SIZE",
		"VPU_VOUT_BLD_SRC0_HPOS",
		"VPU_VOUT_BLD_SRC0_VPOS",
		"VPU_VOUT_BLD_SRC1_HPOS",
		"VPU_VOUT_BLD_SRC1_VPOS",
		"VPU_VOUT_TOP_CTRL",
		"VPU_VOUT_SECURE_BIT_NOR",
		"VPU_VOUT_SECURE_DATA",
		"VPU_VOUT_FRM_CTRL",
		"VPU_VOUT_IRQ_CTRL",
		"VPU_VOUT_VLK_CTRL",
	};
#endif

	if (cur_dev->display_module != C3_DISPLAY_MODULE)
		return;
	vout_blend_reg[i++] = cur_dev->vout_blend_reg.vpu_vout_blend_ctrl;
	vout_blend_reg[i++] = cur_dev->vout_blend_reg.vpu_vout_blend_dumdata;
	vout_blend_reg[i++] = cur_dev->vout_blend_reg.vpu_vout_blend_size;
	vout_blend_reg[i++] = cur_dev->vout_blend_reg.vpu_vout_bld_src0_hpos;
	vout_blend_reg[i++] = cur_dev->vout_blend_reg.vpu_vout_bld_src0_vpos;
	vout_blend_reg[i++] = cur_dev->vout_blend_reg.vpu_vout_bld_src1_hpos;
	vout_blend_reg[i++] = cur_dev->vout_blend_reg.vpu_vout_bld_src1_vpos;
	vout_blend_reg[i++] = VPU_VOUT_TOP_CTRL;
	vout_blend_reg[i++] = VPU_VOUT_SECURE_BIT_NOR;
	vout_blend_reg[i++] = VPU_VOUT_SECURE_DATA;
	vout_blend_reg[i++] = VPU_VOUT_FRM_CTRL;
	vout_blend_reg[i++] = VPU_VOUT_IRQ_CTRL;
	vout_blend_reg[i++] = VPU_VOUT_VLK_CTRL;
	pr_info("vout blend reg:\n");

	for (i = 0; i < VOUT_BLEND_REG_CNT; i++) {
		reg_addr = vout_blend_reg[i];
		reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
		pr_info("[0x%x] = 0x%X [%s]\n",
		  reg_addr, reg_val, reg_name[i]);
#else
		pr_info("[0x%x] = 0x%X\n",
		  reg_addr, reg_val);
#endif
	}
}

static void dump_fgrain_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;

	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		if (glayer_info[i].fgrain_support) {
			pr_info("vd%d fgrain regs:\n", i);
			reg_addr = vd_layer[i].fg_reg.fgrain_ctrl;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
			reg_addr = vd_layer[i].fg_reg.fgrain_win_h;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
			reg_addr = vd_layer[i].fg_reg.fgrain_win_v;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
	}
}

#define AISR_REG_CNT 44

static void dump_aisr_reg(void)
{
	u32 i = 0;
	u32 reg_addr, reg_val = 0;
	u32 aisr_reg[AISR_REG_CNT];

	if (cur_dev->vd1_vsr_safa_support)
		return;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_ctrl0;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_ctrl1;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_scope_x;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_scope_y;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr00;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr01;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr02;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr03;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr10;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr11;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr12;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr13;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr20;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr21;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr22;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr23;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr30;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr31;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr32;
	aisr_reg[i++] = aisr_reshape_reg.aisr_reshape_baddr33;
	aisr_reg[i++] = aisr_reshape_reg.aisr_post_ctrl;
	aisr_reg[i++] = aisr_reshape_reg.aisr_post_size;
	aisr_reg[i++] = aisr_reshape_reg.aisr_sr1_nn_post_top;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_region12_startp;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_region34_startp;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_region4_endp;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_start_phase_step;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_region1_phase_slope;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_region3_phase_slope;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_phase_ctrl;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_vsc_init_phase;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_region12_startp;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_region34_startp;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_region4_endp;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_start_phase_step;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_region1_phase_slope;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_region3_phase_slope;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_phase_ctrl;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_sc_misc;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_hsc_phase_ctrl1;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_prehsc_coef;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_pre_scale_ctrl;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_prevsc_coef;
	aisr_reg[i++] = cur_dev->aisr_pps_reg.vd_prehsc_coef1;
	pr_info("aisr reshape regs:\n");
	for (i = 0; i < AISR_REG_CNT; i++) {
		reg_addr = aisr_reg[i];
		if (!reg_addr)
			continue;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

#define VD1_VPP_PATH_SIZE_REG_CNT 25
#define VD2_VPP_PATH_SIZE_REG_CNT 7
#define VD3_VPP_PATH_SIZE_REG_CNT 7

static void dump_vpp_path_size_reg(void)
{
	u32 i = 0;
	u32 reg_addr, reg_val = 0;
	u32 vd1_vpp_path_reg[VD1_VPP_PATH_SIZE_REG_CNT];
	u32 vd2_vpp_path_reg[VD2_VPP_PATH_SIZE_REG_CNT];
	u32 vd3_vpp_path_reg[VD3_VPP_PATH_SIZE_REG_CNT];
	struct hw_vpp_path_size_s *p_vpp_path_size_reg = NULL;
#if DUMP_REG_NAME
	static const char * const vd1_reg_name[] = {
		"vd1_hdr_in_size",
		"vpp_line_in_length",
		"vpp_pic_in_height",
		"vd1_sc_h_startp",
		"vd1_sc_h_endp",
		"vd1_sc_v_startp",
		"vd1_sc_v_endp",
		"vpp_line_in_length",
		"vpp_pic_in_height",
		"vd1_sc_h_startp",
		"vd1_sc_h_endp",
		"vd1_sc_v_startp",
		"vd1_sc_v_endp",
		"schn_sc_h_startp",
		"schn_sc_h_endp",
		"schn_sc_v_startp",
		"schn_sc_v_endp",
		"vpp_preblend_h_size",
		"preblend_vd1_h_start_end",
		"preblend_vd1_v_start_end",
		"vpp_ve_h_v_size",
		"vpp_postblend_h_size",
		"vpp_out_h_v_size",
		"postblend_vd1_h_start_end",
		"postlend_vd1_v_start_end",
	};
	static const char * const vd2_reg_name[] = {
		"vd2_hdr_in_size",
		"vd2_sc_h_startp",
		"vd2_sc_h_endp",
		"vd2_sc_v_startp",
		"vd2_sc_v_endp",
		"blend_vd2_h_start_end",
		"blend_vd2_v_start_end",
	};
	static const char * const vd3_reg_name[] = {
		"vd3_hdr_in_size",
		"vd3_sc_h_startp",
		"vd3_sc_h_endp",
		"vd3_sc_v_startp",
		"vd3_sc_v_endp",
		"blend_vd3_h_start_end",
		"blend_vd3_v_start_end",
	};
#endif

	if (legacy_vpp)
		return;
	if (cur_dev->display_module == T6W_DISPLAY_MODULE)
		p_vpp_path_size_reg = &vpp_path_size_reg_t6w;
	else
		p_vpp_path_size_reg = &vpp_path_size_reg;
	pr_info("vpp path size reg:\n");
	if (glayer_info[0].layer_support) {
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_hdr_in_size;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_line_in_length;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_pic_in_height;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_h_startp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_h_endp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_v_startp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_v_endp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_line_in_length;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_pic_in_height;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_h_startp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_h_endp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_v_startp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vd1_sc_v_endp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->schn_sc_h_startp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->schn_sc_h_endp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->schn_sc_v_startp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->schn_sc_v_endp;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_preblend_h_size;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->preblend_vd1_h_start_end;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->preblend_vd1_v_start_end;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_ve_h_v_size;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_postblend_h_size;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->vpp_out_h_v_size;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->postblend_vd1_h_start_end;
		vd1_vpp_path_reg[i++] = p_vpp_path_size_reg->postlend_vd1_v_start_end;
		for (i = 0; i < VD1_VPP_PATH_SIZE_REG_CNT; i++) {
			reg_addr = vd1_vpp_path_reg[i];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, vd1_reg_name[i]);
#else
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
		}
		if (cur_dev->display_module == T6W_DISPLAY_MODULE &&
			video_is_after_meson_t6x_cpu()) {
			reg_addr = p_vpp_path_size_reg->vd1_sco_fifo_ctrl;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("vd1_sco_fifo_ctrl[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
	} else if (glayer_info[1].layer_support) {
		i = 0;
		vd2_vpp_path_reg[i++] = p_vpp_path_size_reg->vd2_hdr_in_size;
		vd2_vpp_path_reg[i++] = p_vpp_path_size_reg->vd2_sc_h_startp;
		vd2_vpp_path_reg[i++] = p_vpp_path_size_reg->vd2_sc_h_endp;
		vd2_vpp_path_reg[i++] = p_vpp_path_size_reg->vd2_sc_v_startp;
		vd2_vpp_path_reg[i++] = p_vpp_path_size_reg->vd2_sc_v_endp;
		vd2_vpp_path_reg[i++] = p_vpp_path_size_reg->blend_vd2_h_start_end;
		vd2_vpp_path_reg[i++] = p_vpp_path_size_reg->blend_vd2_v_start_end;
		for (i = 0; i < VD2_VPP_PATH_SIZE_REG_CNT; i++) {
			reg_addr = vd2_vpp_path_reg[i];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, vd2_reg_name[i]);
#else
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
		}
		if (cur_dev->display_module == T6W_DISPLAY_MODULE &&
			video_is_after_meson_t6x_cpu()) {
			reg_addr = p_vpp_path_size_reg->vd2_sco_fifo_ctrl;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("vd2_sco_fifo_ctrl[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
		}
	} else if (glayer_info[2].layer_support) {
		i = 0;
		vd3_vpp_path_reg[i++] = p_vpp_path_size_reg->vd3_hdr_in_size;
		vd3_vpp_path_reg[i++] = p_vpp_path_size_reg->vd3_sc_h_startp;
		vd3_vpp_path_reg[i++] = p_vpp_path_size_reg->vd3_sc_h_endp;
		vd3_vpp_path_reg[i++] = p_vpp_path_size_reg->vd3_sc_v_startp;
		vd3_vpp_path_reg[i++] = p_vpp_path_size_reg->vd3_sc_v_endp;
		vd3_vpp_path_reg[i++] = p_vpp_path_size_reg->blend_vd3_h_start_end;
		vd3_vpp_path_reg[i++] = p_vpp_path_size_reg->blend_vd3_v_start_end;
		for (i = 0; i < VD3_VPP_PATH_SIZE_REG_CNT; i++) {
			reg_addr = vd3_vpp_path_reg[i];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, vd3_reg_name[i]);
#else
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
		}
	}
	if (cur_dev->sr0_support) {
		reg_addr = p_vpp_path_size_reg->sr0_in_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("sr0_in_size[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	} else if (cur_dev->sr1_support) {
		reg_addr = p_vpp_path_size_reg->sr1_in_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("sr1_in_size[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
	if (cur_dev->aisr_support) {
		reg_addr = p_vpp_path_size_reg->aisr_post_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("aisr_post_size[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

#define VD1_VPP_MISC_REG 8

static void dump_vpp_misc_reg(void)
{
	u32 i = 0;
	u32 reg_addr, reg_val = 0;
	u32 vd1_vpp_misc_reg[VD1_VPP_MISC_REG];
#if DUMP_REG_NAME
	static const char * const vd1_reg_name[] = {
		"mali_afbcd_top_ctrl",
		"mali_afbcd1_top_ctrl",
		"mali_afbcd2_top_ctrl",
		"vpp_vd1_top_ctrl",
		"vd_path_misc_ctrl",
		"path_start_sel",
		"vpp_misc",
		"vpp_misc1",
	};
#endif

	if (legacy_vpp)
		return;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.mali_afbcd_top_ctrl;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.mali_afbcd1_top_ctrl;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.mali_afbcd2_top_ctrl;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.vpp_vd1_top_ctrl;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.vd_path_misc_ctrl;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.path_start_sel;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.vpp_misc;
	vd1_vpp_misc_reg[i++] = viu_misc_reg.vpp_misc1;
	pr_info("vpp misc reg:\n");
	if (glayer_info[0].layer_support) {
		for (i = 0; i < VD1_VPP_MISC_REG; i++) {
			reg_addr = vd1_vpp_misc_reg[i];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, vd1_reg_name[i]);
#else
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
		}
	} else if (glayer_info[1].layer_support) {
		reg_addr = viu_misc_reg.vpp_vd2_top_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("vpp_vd2_top_ctrl[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	} else if (glayer_info[2].layer_support) {
		reg_addr = viu_misc_reg.vpp_vd3_top_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("vpp_vd3_top_ctrl[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

#define ZORDER_REG_CNT 4

static void dump_zorder_reg(void)
{
	u32 i = 0;
	u32 reg_addr, reg_val = 0;
	u32 vd1_vpp_misc_reg[ZORDER_REG_CNT];
#if DUMP_REG_NAME
	static const char * const vd1_reg_name[] = {
		"VD1_BLEND_SRC_CTRL",
		"VD2_BLEND_SRC_CTRL",
		"OSD1_BLEND_SRC_CTRL",
		"OSD2_BLEND_SRC_CTRL",
	};
#endif
	if (legacy_vpp)
		return;
	pr_info("vpp zorder reg:\n");
	if (glayer_info[0].layer_support) {
		vd1_vpp_misc_reg[i++] = VD1_BLEND_SRC_CTRL;
		vd1_vpp_misc_reg[i++] = VD2_BLEND_SRC_CTRL;
		vd1_vpp_misc_reg[i++] = OSD1_BLEND_SRC_CTRL;
		vd1_vpp_misc_reg[i++] = OSD2_BLEND_SRC_CTRL;
		for (i = 0; i < ZORDER_REG_CNT; i++) {
			reg_addr = vd1_vpp_misc_reg[i];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, vd1_reg_name[i]);
#else
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
		}
	} else if (glayer_info[1].layer_support) {
		reg_addr = VD2_BLEND_SRC_CTRL;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("VD2_BLEND_SRC_CTRL[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	} else if (glayer_info[2].layer_support) {
		reg_addr = VD3_BLEND_SRC_CTRL;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("VD3_BLEND_SRC_CTRL[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
	}
}

#define VPPX_BLEND_REG_CNT 7

static void dump_vppx_blend_reg(void)
{
	u32 i, j = 0;
	u32 reg_addr, reg_val = 0;
	u32 vppx_blend_reg[2][VPPX_BLEND_REG_CNT];
#if DUMP_REG_NAME
	static const char * const vppx_blend_reg_name[] = {
		"vpp_bld_din0_hscope",
		"vpp_bld_din0_vscope",
		"vpp_bld_out_size",
		"vpp_bld_ctrl",
		"vpp_bld_dummy_data",
		"vpp_bld_dummy_alpha",
		"vpp_blend_ctrl",
	};
#endif
	if (cur_dev->display_module == OLD_DISPLAY_MODULE)
		return;

	for (i = 0; i < 2; i++) {
		if (video_is_meson_a9_cpu()) {
			vppx_blend_reg[i][j++] = vppx_blend_reg_a9_array[i].vpp_bld_din0_hscope;
			vppx_blend_reg[i][j++] = vppx_blend_reg_a9_array[i].vpp_bld_din0_vscope;
			vppx_blend_reg[i][j++] = vppx_blend_reg_a9_array[i].vpp_bld_out_size;
			vppx_blend_reg[i][j++] = vppx_blend_reg_a9_array[i].vpp_bld_ctrl;
			vppx_blend_reg[i][j++] = vppx_blend_reg_a9_array[i].vpp_bld_dummy_data;
			vppx_blend_reg[i][j++] = vppx_blend_reg_a9_array[i].vpp_bld_dummy_alpha;
			vppx_blend_reg[i][j++] = vppx_blend_reg_a9_array[i].vpp_blend_ctrl;
			j = 0;
		} else {
			vppx_blend_reg[i][j++] = vppx_blend_reg_array[i].vpp_bld_din0_hscope;
			vppx_blend_reg[i][j++] = vppx_blend_reg_array[i].vpp_bld_din0_vscope;
			vppx_blend_reg[i][j++] = vppx_blend_reg_array[i].vpp_bld_out_size;
			vppx_blend_reg[i][j++] = vppx_blend_reg_array[i].vpp_bld_ctrl;
			vppx_blend_reg[i][j++] = vppx_blend_reg_array[i].vpp_bld_dummy_data;
			vppx_blend_reg[i][j++] = vppx_blend_reg_array[i].vpp_bld_dummy_alpha;
			vppx_blend_reg[i][j++] = vppx_blend_reg_array[i].vpp_blend_ctrl;
			j = 0;
		}
	}
	if (cur_dev->has_vpp1) {
		pr_info("vpp1 blend regs:\n");
		for (i = 0; i < VPPX_BLEND_REG_CNT; i++) {
			reg_addr = vppx_blend_reg[0][i];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, vppx_blend_reg_name[i]);
#else
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
		}
	}
	if (cur_dev->has_vpp2) {
		pr_info("vpp2 blend regs:\n");
		for (i = 0; i < VPPX_BLEND_REG_CNT; i++) {
			reg_addr = vppx_blend_reg[1][i];
			if (!reg_addr)
				continue;
			reg_val = READ_VCBUS_REG(reg_addr);
#if DUMP_REG_NAME
			pr_info("[0x%x] = 0x%X [%s]\n",
				   reg_addr, reg_val, vppx_blend_reg_name[i]);
#else
			pr_info("[0x%x] = 0x%X\n",
				   reg_addr, reg_val);
#endif
		}
	}
}

static void dump_mosaic_reg(void)
{
	int i;
	u32 reg_addr, reg_val = 0;

	if (cur_dev->display_module == T6W_DISPLAY_MODULE &&
	    cur_dev->mosaic_support &&
	    vd_layer[0].mosaic_mode) {
		pr_info("--- mosaic reg dump start ---\n");
		reg_addr = mosaic_misc_reg.player_2x2_ctrl;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("player_2x2_ctrl[0x%x] = 0x%X (2x2 enable:bit28 0xf)\n",
			   reg_addr, reg_val);
		reg_addr = mosaic_misc_reg.vfcd_top_ctrl4;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("vfcd_top_ctrl4[0x%x] = 0x%X (bit1)\n",
			   reg_addr, reg_val);
		reg_addr = mosaic_misc_reg.vfcd_top_ctrl4_1;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("vfcd_top_ctrl4_1[0x%x] = 0x%X (bit1)\n",
			   reg_addr, reg_val);
		reg_addr = VD1_BLEND_SRC_CTRL;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("VD1_BLEND_SRC_CTRL[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = VD2_BLEND_SRC_CTRL;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("VD2_BLEND_SRC_CTRL[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = viu_misc_reg.vpp_misc;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("vpp_misc[0x%x] = 0x%X (preblend en:bit6 1)\n",
			   reg_addr, reg_val);
		dump_mif_reg_t6w(true);
		dump_vfcd_reg_t6w(true);
		dump_pps_reg(true);
		for (i = 0; i < SLICE_NUM; i++) {
			pr_info("window%d axis reg:\n", i);
			reg_addr = g_mosaic_frame[i].reg.vpp_blend_reg.preblend_h_start_end;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("preblend_h_start_end[0x%x] = 0x%X\n", reg_addr, reg_val);
			reg_addr = g_mosaic_frame[i].reg.vpp_blend_reg.preblend_v_start_end;
			reg_val = READ_VCBUS_REG(reg_addr);
			pr_info("preblend_v_start_end[0x%x] = 0x%X\n", reg_addr, reg_val);
		}
		reg_addr = vd_layer[0].vpp_blend_reg.preblend_h_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("preblend_h_size[0x%x] = 0x%X\n", reg_addr, reg_val);
		reg_addr = vpp_path_size_reg.vpp_postblend_h_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("postblend_h_size[0x%x] = 0x%X\n", reg_addr, reg_val);
		reg_addr = vd_layer[0].vpp_blend_reg.postblend_h_start_end;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("postblend_h_start_end[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vd_layer[0].vpp_blend_reg.postblend_v_start_end;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("postblend_v_start_end[0x%x] = 0x%X\n",
			   reg_addr, reg_val);
		reg_addr = vpp_path_size_reg.vpp_out_h_v_size;
		reg_val = READ_VCBUS_REG(reg_addr);
		pr_info("vpp_out_h_v_size[0x%x] = 0x%X\n", reg_addr, reg_val);
		pr_info("--- mosaic reg dump end ---\n");
	}
}

ssize_t reg_dump_store(const struct class *cla,
				 const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	if (ret) {
		pr_err("kstrtoint err\n");
		return -EINVAL;
	}

	if (res) {
		if (cur_dev->display_module == S5_DISPLAY_MODULE) {
			dump_s5_vd_proc_regs();
			dump_vpp_post_reg();
		} else if (cur_dev->display_module == C3_DISPLAY_MODULE) {
			dump_mif_reg();
			if (!video_is_meson_c3_cpu())
				dump_pps_reg(false);
			dump_vout_blend_reg();
		} else {
			if (cur_dev->display_module == T6W_DISPLAY_MODULE) {
				dump_mif_reg_t6w(false);
				dump_vfcd_reg_t6w(false);
			} else {
				dump_mif_reg();
				dump_afbc_reg();
			}
			dump_pps_reg(false);
			dump_vpp_blend_reg();
			dump_vppx_blend_reg();
			dump_vpp_path_size_reg();
			dump_vpp_misc_reg();
			dump_zorder_reg();
			dump_fgrain_reg();
			if (cur_dev->aisr_support)
				dump_aisr_reg();
			dump_mosaic_reg();
		}
	}
	return count;
}

#else
ssize_t reg_dump_store(const struct class *cla,
				 const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	if (ret) {
		pr_err("kstrtoint err\n");
		return -EINVAL;
	}

	if (res) {
		dump_mif_reg();
		dump_afbc_reg();
		dump_pps_reg(false);
		dump_vpp_blend_reg();
	}
	return count;
}
#endif

bool frc_n2m_worked(void)
{
	bool ret = false;

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		/* frc_get_n2m_setting 1 : n2m is 1:1; 2 :n2m is 1:2 */
		/* 3: n2m is 1:2.5; 4: n2m is 1:4; 5: n2m is 1:5 */
		/* frc_is_on() 1: means frc really worked */
		if ((frc_get_n2m_setting() >= 2) && frc_is_on())
			ret = true;
#endif
	return ret;
}

u32 frc_get_n2m_ratio(void)
{
	/* FRC_RATIO_1_1 */
	u32 ratio = 10;

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	int ret = 0;

	ret = frc_get_n2m_setting();
	switch (ret) {
	case 2:
		/* FRC_RATIO_1_2 */
		ratio = 20;
		break;
	case 3:
		/* FRC_RATIO_2_5 */
		ratio = 25;
		break;
	case 4:
		/* FRC_RATIO_1_4 */
		ratio = 40;
		break;
	case 5:
		/* FRC_RATIO_1_5 */
		ratio = 50;
		break;
	}
#endif
	return ratio;
}

bool frc_n2m_1st_frame_worked(struct video_layer_s *layer)
{
	static int last_status;
	bool ret = false;

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	if (debug_common_flag & DEBUG_FLAG_COMMON_FRC)
		pr_info("%s:frc_is_on()=%d, frc_drv_get_1st_frm=%d, frc_n2m_1st_frame=%d\n",
			__func__,
			frc_is_on(),
			frc_drv_get_1st_frm(),
			layer->frc_n2m_1st_frame);

	if (!frc_is_on())
		return ret;
#endif
	if (layer->frc_n2m_1st_frame)
		return last_status;
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		/* frc_get_n2m_setting 1 : n2m is 1:1; 2 :n2m is 1:2 */
		/* frc_is_on() 1: means frc really worked */
		if ((frc_get_n2m_setting() == 2) && frc_is_on() &&
			frc_drv_get_1st_frm()) {
			layer->frc_n2m_1st_frame = true;
			ret = true;
			last_status = ret;
		}
#endif
	return ret;
}

bool frc_n2m_is_stable(struct video_layer_s *layer)
{
	if (cur_dev->vsync_2to1_enable)
		return frc_n2m_1st_frame_worked(layer);
	else if (cur_dev->pre_vsync_enable)
		return frc_n2m_worked();
	else
		return false;
}

bool check_aisr_need_disable(struct video_layer_s *layer)
{
	bool ret = false;
	u32 layer_width, layer_height;
	struct disp_info_s *disp_layer = &glayer_info[layer->layer_id];
	const struct vinfo_s *info = NULL;

	if (layer->slice_num >= 2)
		return true;
	info = get_current_vinfo();
	if (info) {
		layer_width = disp_layer->layer_width;
		layer_height = disp_layer->layer_height;
		/* 1/4 full screen aisr disabled */
		if (layer_width < info->width * aisr_size_threshold / 100 &&
			layer_height < info->height * aisr_size_threshold / 100)
			ret = true;
		else
			ret = false;
		if (debug_common_flag & DEBUG_FLAG_COMMON_AISR)
			pr_info("%s:ret=%d width(%d, %d), height(%d,%d)\n",
				__func__,
				ret,
				layer_width,
				info->width * aisr_size_threshold / 100,
				layer_height,
				info->height * aisr_size_threshold / 100);
	}
	return ret;
}

bool is_aisr_enable(struct video_layer_s *layer)
{
	bool ret = false;
	struct aisr_setting_s *aisr_mif_setting = &layer->aisr_mif_setting;

	if (!aisr_mif_setting->aisr_enable ||
		!cur_dev->aisr_enable)
		ret = false;
	else
		ret = true;
	if (debug_common_flag & DEBUG_FLAG_COMMON_AISR)
		pr_info("%s:ret=%d(%d, %d)\n",
			__func__,
			ret,
			aisr_mif_setting->aisr_enable,
			cur_dev->aisr_enable);
	return ret;
}

#ifndef CONFIG_AMLOGIC_VIDEO_COMPOSER
bool get_lowlatency_mode(void)
{
	return 0;
}
#endif

