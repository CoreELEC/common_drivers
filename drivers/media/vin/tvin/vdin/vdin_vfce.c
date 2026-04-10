// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/******************** READ ME ************************
 *
 * at afbce mode, 1 block = 32 * 4 pixel
 * there is a header in one block.
 * for example at 1080p,
 * header numbers = block numbers = 1920 * 1080 / (32 * 4)
 *
 * table map(only at non-mmu mode):
 * afbce data was saved at "body" region,
 * body region has been divided for every 4K(4096 bytes) and 4K unit,
 * table map contents is : (body addr >> 12)
 *
 * at non-mmu mode(just vdin non-mmu mode):
 * ------------------------------
 *          header
 *     (can analysis body addr)
 * ------------------------------
 *          table map
 *     (save body addr)
 * ------------------------------
 *          body
 *     (save afbce data)
 * ------------------------------
 */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/slab.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include "../tvin_global.h"
#include "../tvin_format_table.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_drv.h"
#include "vdin_vf.h"
#include "vdin_canvas.h"
#include "vdin_afbce.h"
#include "vdin_mem_scatter.h"
#include "vdin_regs_t6w.h"

/* fixed config mif by default */
void vdin_vfce_mif_config_init(struct vdin_dev_s *devp)
{
	if (devp->index == 0) {
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			    1, VDIN0_MIF_ENABLE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			    0, VDIN0_OUT_AFBCE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			    1, VDIN0_OUT_MIF_BIT, 1);
	} else {
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			    1, VDIN1_MIF_ENABLE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			    0, VDIN1_OUT_AFBCE_BIT, 1);
		W_VCBUS_BIT(VDIN_MISC_CTRL,
			    1, VDIN1_OUT_MIF_BIT, 1);
	}
}

/* only support init vdin0 mif/afbce */
void vdin_vfce_write_mif_or_afbce_init(struct vdin_dev_s *devp)
{
	enum vdin_output_mif_e sel;

	if (((devp->afbce_flag & VDIN_AFBCE_EN) == 0) || devp->index == 1 ||
	    devp->double_wr)
		return;

	if (devp->afbce_mode == 0)
		sel = VDIN_OUTPUT_TO_MIF;
	else
		sel = VDIN_OUTPUT_TO_AFBCE;

	if (sel == VDIN_OUTPUT_TO_MIF) {
		W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 0x0, 0, 1);
		W_VCBUS_BIT(VFCE_CHNL1_ENABLE, 0x0, 0, 1);
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
				    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
				    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			W_VCBUS_BIT(VPU_AXI_WR_PROTECT, 0x8000,
				    HOLD_NUM_BIT, HOLD_NUM_WID);
			W_VCBUS_BIT(VPU_AXI_WR_PROTECT, 1,
				    PROTECT_EN1_BIT, PROTECT_EN_WID);
			W_VCBUS_BIT(VPU_AXI_WR_PROTECT, 1,
				    PROTECT_EN21_BIT, PROTECT_EN_WID);
		} else {
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_OUT_AFBCE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_OUT_MIF_BIT, 1);
		}
	} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
				    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			W_VCBUS_BIT(VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
				    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			W_VCBUS(VPU_AXI_WR_PROTECT, 0);
		} else {
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_MIF_ENABLE_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 0, VDIN0_OUT_MIF_BIT, 1);
			W_VCBUS_BIT(VDIN_MISC_CTRL, 1, VDIN0_OUT_AFBCE_BIT, 1);
		}

		W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 0x0, 0, 1);
	}
}

/* only support config vdin0 mif/afbce dynamically */
void vdin_vfce_write_mif_or_afbce(struct vdin_dev_s *devp,
			     enum vdin_output_mif_e sel)
{
	if (((devp->afbce_flag & VDIN_AFBCE_EN) == 0) || devp->double_wr)
		return;

	if (sel == VDIN_OUTPUT_TO_MIF) {
		W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 0x0, 0, 1);

		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
					    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
					    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			rdma_write_reg_bits(devp->rdma_handle,
					    VPU_AXI_WR_PROTECT, 0x8000,
					    HOLD_NUM_BIT, HOLD_NUM_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VPU_AXI_WR_PROTECT, 1,
					    PROTECT_EN1_BIT, PROTECT_EN_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VPU_AXI_WR_PROTECT, 1,
					    PROTECT_EN21_BIT, PROTECT_EN_WID);
		} else {
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    0, VDIN0_OUT_AFBCE_BIT, 1);
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    1, VDIN0_OUT_MIF_BIT, 1);
		}
	} else if (sel == VDIN_OUTPUT_TO_AFBCE) {
		if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_DIS,
					    MIF0_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);
			rdma_write_reg_bits(devp->rdma_handle,
					    VDIN_TOP_DOUBLE_CTRL, WR_SEL_VDIN0_NOR,
					    AFBCE_OUT_SEL_BIT, VDIN_REORDER_SEL_WID);

			/* axi write protection
			 * for HDMI cable plug/unplug crash issue
			 */
			rdma_write_reg(devp->rdma_handle, VPU_AXI_WR_PROTECT,
				       0);
		} else {
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    0, VDIN0_OUT_MIF_BIT, 1);
			rdma_write_reg_bits(devp->rdma_handle, VDIN_MISC_CTRL,
					    1, VDIN0_OUT_AFBCE_BIT, 1);
		}
		W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 0x0, 0, 1);
	}
}

void vdin_vfce_update(struct vdin_dev_s *devp)
{
	unsigned int reg_offset = 0, i;
	unsigned int reg_fmt444_comb;
	unsigned int reg_inp_format = 0, reg_enc_format;
	unsigned int reg_comp_yc_map;
	unsigned int uncmp_size, uncmp_bits, cmp_bits;
	unsigned int ram_ram_size_mode = 1;

	if (vdin_is_convert_to_nv21(devp->afbc_format_convert)) {
		reg_enc_format = 2;/*420*/
		uncmp_bits = 12;
	} else if (vdin_is_convert_to_422(devp->afbc_format_convert)) {
		reg_enc_format = 1;/*422*/
		uncmp_bits = 16;
	} else {
		reg_enc_format = 0;/*444*/
		uncmp_bits = 24;
	}

	reg_fmt444_comb = vdin_chk_is_comb_mode(devp);
	cmp_bits   = devp->source_bitdepth;
	if (devp->is_422_12bit_enabled) {
		reg_enc_format = 1;/*422*/
		uncmp_bits = 16;
		reg_inp_format = 0;/* always 444 input */
		reg_fmt444_comb = 0;
	}
	/* t6x:Should NOT set the comb and the ram 4k 444 10-bit simultaneously */
	if (reg_fmt444_comb)
		ram_ram_size_mode = 0;

	rdma_write_reg_bits(devp->rdma_handle,
		VFCE_TOP0_CTRL_2, ram_ram_size_mode, 4, 1);//ram_ram_size_mode

	//bit size of uncompressed mode
	uncmp_size = (((((16 * cmp_bits * uncmp_bits) + 7) >> 3) + 31)
		      / 32) << 1;
	reg_comp_yc_map = cmp_bits == 8  ? 0 :
			  cmp_bits == 10 ? 1 :
			  cmp_bits == 12 ? 2 :
			  cmp_bits == 14 ? 3 : 4;
	for (i = 0; i <= 1; i++) { //channel0 & channel1 have the same regs
		reg_offset = (VFCE_CHNL1_MODE - VFCE_CHNL0_MODE) * i;

		rdma_write_reg_bits(devp->rdma_handle,
			reg_offset + VFCE_CHNL0_MODE, reg_fmt444_comb, 0, 1);//reg_fmt444_comb
		rdma_write_reg_bits(devp->rdma_handle,
			reg_offset + VFCE_CHNL0_MODE, reg_enc_format, 4, 3);	//reg_enc_format
		rdma_write_reg_bits(devp->rdma_handle,
			reg_offset + VFCE_CHNL0_MODE, reg_comp_yc_map, 7, 3);//comp_c_map
		rdma_write_reg_bits(devp->rdma_handle,
			reg_offset + VFCE_CHNL0_MODE, reg_comp_yc_map, 10, 3);//comp_c_map
		rdma_write_reg_bits(devp->rdma_handle,
			reg_offset + VFCE_CHNL0_AFBC_MODE_0, uncmp_size, 5, 8);//reg_uncmp_size

		if (devp->is_vfce_en) {
			rdma_write_reg_bits(devp->rdma_handle,
				VFCE_AFRCE_CORE_PIXEL_DEPTH, cmp_bits, 8, 4);//plane 1 bits
			rdma_write_reg_bits(devp->rdma_handle,
				VFCE_AFRCE_CORE_PIXEL_DEPTH, cmp_bits, 12, 4);//plane 0 bits
		} else {
			rdma_write_reg_bits(devp->rdma_handle,
				VFCE_AFBCE_FORMAT, reg_enc_format, 8, 2);//reg_uncmp_size
			rdma_write_reg_bits(devp->rdma_handle,
				VFCE_AFBCE_FORMAT, cmp_bits, 0, 4);//reg_comp_bits_y
			rdma_write_reg_bits(devp->rdma_handle,
				VFCE_AFBCE_FORMAT, cmp_bits, 4, 4);//reg_comp_bits_c
		}
	}
}

void vdin_vfce_chnl(struct vdin_dev_s *devp)
{
	bool reg_core_sel = 0; //0-afbce,1:afrc
	unsigned int reg_offset = 0, i;
	//unsigned int blk_out_end_h;//output blk scope
	//unsigned int blk_out_bgn_h;//output blk scope
	//unsigned int blk_out_end_v;//output blk scope
	//unsigned int blk_out_bgn_v;//output blk scope
	unsigned int enc_win_bgn_h;//input scope
	unsigned int enc_win_end_h;//input scope
	unsigned int enc_win_bgn_v;//input scope
	unsigned int enc_win_end_v;//input scope
	unsigned int comp_target_byte = 32;
	//0:R  1:RG  2:RGB  3:RGBA  4:Y/U/V  5: Y_UV  6:bayer 4 plane  7:bayer rg 4x16
	//8:bayer2x2 rg 8x16
	unsigned int reg_pixel_format;
	unsigned int reg_rgb2yuv_en;
	unsigned int reg_pixel_is_diff_chn;
	unsigned int reg_input_is_rgba;
	unsigned int reg_pixel_type_0;
	unsigned int reg_pixel_type_1;
	unsigned int reg_fmt444_comb;
	unsigned int reg_inp_format = 0, reg_enc_format;
	unsigned int reg_comp_yc_map;
	unsigned int uncmp_size, uncmp_bits, cmp_bits;

	enc_win_bgn_h = 0;
	enc_win_end_h = devp->h_active - 1;
	enc_win_bgn_v = 0;
	enc_win_end_v = devp->v_active - 1;

	comp_target_byte = devp->cr_lossy_param.cr_lossy_target_byte;

	if (vdin_is_convert_to_rgb(devp->afbc_format_convert)) { //rgb
		reg_pixel_format	= 2;
		reg_pixel_type_0	= 0;
		reg_pixel_type_1	= 0;
		reg_rgb2yuv_en		= 1;
		reg_pixel_is_diff_chn	= 1;
		reg_input_is_rgba	= 1;
	} else { //yuv
		reg_pixel_format	= 5;
		reg_pixel_type_0	= 3;
		reg_pixel_type_1	= 4;
		reg_rgb2yuv_en		= 0;
		reg_pixel_is_diff_chn	= 0;
		reg_input_is_rgba	= 0;
	}
	if (vdin_is_convert_to_nv21(devp->afbc_format_convert)) {
		reg_enc_format = 2;/*420*/
		uncmp_bits = 12;
	} else if (vdin_is_convert_to_422(devp->afbc_format_convert)) {
		reg_enc_format = 1;/*422*/
		uncmp_bits = 16;
	} else {
		reg_enc_format = 0;/*444*/
		uncmp_bits = 24;
	}

	if (devp->afbce_flag & VDIN_AFBCE_EN_LOSSY)
		reg_core_sel = 1;
	else
		reg_core_sel = 0;

	reg_fmt444_comb = vdin_chk_is_comb_mode(devp);
	cmp_bits   = devp->source_bitdepth;
	if (devp->is_422_12bit_enabled) {
		reg_enc_format = 1;/*422*/
		uncmp_bits = 16;
		reg_inp_format = 0;/* always 444 input */
		reg_fmt444_comb = 0;
	}
	//bit size of uncompressed mode
	uncmp_size = (((((16 * cmp_bits * uncmp_bits) + 7) >> 3) + 31)
		      / 32) << 1;
	reg_comp_yc_map = cmp_bits == 8  ? 0 :
			  cmp_bits == 10 ? 1 :
			  cmp_bits == 12 ? 2 :
			  cmp_bits == 14 ? 3 : 4;
	for (i = 0; i <= 1; i++) { //channel0 & channel1 have the same regs
		reg_offset = (VFCE_CHNL1_MODE - VFCE_CHNL0_MODE) * i;
		W_VCBUS(reg_offset + VFCE_CHNL0_MODE,//0x2001
		((0            & 0x7)    << 20) | //reg_444to422_mode
		((0            & 0x7)    << 17) | //reg_422to420_mode
		((reg_core_sel & 0x1)    << 16) | //reg_core_sel
		((0            & 0x3)    << 14) | //reg_bayer_input_plane
		((1            & 0x1)    << 13) | //align_en
		((reg_comp_yc_map & 0x7) << 10) | //comp_y_map
		((reg_comp_yc_map & 0x7) << 7)  | //comp_c_map
		((reg_enc_format  & 0x7) << 4)  | //reg_enc_format
		((reg_inp_format  & 0x7) << 1)  | //reg_inp_format
		((reg_fmt444_comb & 0x1) << 0)    //reg_fmt444_comb
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_AFBC_MODE_0,//0x288
		((uncmp_size & 0xff) << 5) |  // uncmp size
		((2 & 0x7) << 2) |  // head adj value brst_len_add_value
		((0 & 0x1) << 1) |  // burst 4 ofset_brst4_en
		((0 & 0x1) << 0)    // head adj brst_len_add_en
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_AFRC_MODE_0,
		((0x1 & 0x1) << 31) |  // afrc head mode
		(((!reg_input_is_rgba) & 0x7) << 28) |  // input format
		((reg_pixel_format             & 0xf) << 24) |  // pixel format
		((0   & 0x3) <<  22) |  // bayer mode
		((comp_target_byte & 0x7f) << 15) |  // plane 0 target byte
		((comp_target_byte & 0x7f) << 8) |  // plane 1 target byte
		((reg_pixel_type_0 & 0xf) << 4) |  // plane 0 pixel type
		((reg_pixel_type_1 & 0xf) << 0)    // plane 1 pixel type
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_CONV_BUF_OFST,//0x0
		((0            & 0x3fff) << 16) |  // reg_cbuf_1_offset
		((0            & 0x3fff) << 0)    // reg_cbuf_0_offset
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_DS_BUF_OFST,//0x0
		((0            & 0x3fff) << 0)    // reg_ds_buf_offset
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_HEAD_BUF_OFST,//0x0
		((0            & 0x3fff) << 16) |  // reg_head_1_offset
		((0            & 0x3fff) << 0)    // reg_head_0_offset
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_BODY_BUF_OFST,//0x0
		((0            & 0x7ff) << 16) |  // reg_body_1_offset
		((0            & 0x7ff) << 0)    // reg_body_0_offset
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_SIZE_IN,//0x7800438
		((devp->h_active & 0xffff) << 16) |  // hsize_in of afbc input
		((devp->v_active & 0xffff) << 0)    // vsize_in of afbc input
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_PIXEL_IN_HOR_SCOPE,//0x077f0000
		((enc_win_end_h & 0xffff) << 16) |  // scope of hsize_in
		((enc_win_bgn_h & 0xffff) << 0)    // scope of vsize_in
		);

		W_VCBUS(reg_offset + VFCE_CHNL0_PIXEL_IN_VER_SCOPE,//0x04370000
		((enc_win_end_v & 0xffff) << 16) |  // scope of hsize_in
		((enc_win_bgn_v & 0xffff) << 0)    // scope of vsize_in
		);
		//head addr of compressed data
		W_VCBUS(reg_offset + VFCE_CHNL0_HEAD0_BADDR,
			devp->afbce_info->fm_head_paddr[0] >> 4);
		//body addr of compressed data
		W_VCBUS(reg_offset + VFCE_CHNL0_BODY0_BADDR,
			devp->afbce_info->fm_table_paddr[0] >> 4);
		//W_VCBUS(reg_offset + VFCE_CHNL0_BODY1_BADDR,prm->body_baddr_1>>4);
		if (devp->is_vfce_en) {
			W_VCBUS(reg_offset + VFCE_CHNL0_HEAD1_BADDR,
				(devp->afbce_info->fm_head_paddr[0] +
				 devp->afbce_info->frame_head_size_y) >> 4);
			W_VCBUS(reg_offset + VFCE_CHNL0_BODY1_BADDR,
				(devp->afbce_info->fm_table_paddr[0] +
				 devp->afbce_info->frame_table_size_y) >> 4);
		}

		W_VCBUS(reg_offset + VFCE_CHNL0_CTRL,
		((0xf                & 0xf) << 24) |  //reg top force en
		((0x0                & 0x1) << 23) |  //v slice mode en
		((0x0                & 0x1) << 22) |  //h slice mode en
		((0x3                & 0xf) << 18) |  //plane x raw mode en
		((0x3                & 0xf) << 14) |  //plane x force raw mode
		((0x0                & 0x1) << 13) |  //v slice cfg cnt clear
		((0x0                & 0x1) << 12) |  //h slice cfg cnt clear
		((0x0 & 0x1) << 10) |  //pip init en
		((0x0 & 0x1) << 9)  |  //pip mode en
		((0x6f               & 0xff) << 1) |  //trans ctrl thr
		((0x1                & 0x1) << 0)    //trans ctrl en
		);

		W_VCBUS_BIT(reg_offset + VFCE_CHNL0_ENABLE, 0x0, 0, 1);//enable vfce
	}
}

void vdin_vfce_mif(struct vdin_dev_s *devp)
{
	unsigned int mmu_mode_en = 1;
	unsigned int mmu_page_mode = 0;
	unsigned int loop_mode_en = 0;
	unsigned int loop_num = 128;
	unsigned int mif_burst_mode = 3;

	mmu_mode_en = devp->cr_lossy_param.mmu_en;
	//plane0
	W_VCBUS_BIT(VFCE_MIF_PLANE0_MODE, mmu_mode_en & 0x1, 4, 1);//mmu mode en
	W_VCBUS_BIT(VFCE_MIF_PLANE0_MODE, mmu_page_mode & 0x1, 5, 1);//page size

	W_VCBUS_BIT(VFCE_MIF_PLANE0_LOOP, loop_mode_en & 0x1, 0, 1);//loop mode en
	W_VCBUS_BIT(VFCE_MIF_PLANE0_LOOP, loop_num & 0xfff, 2, 12);//loop num

	W_VCBUS_BIT(VFCE_MIF_PLANE0_MODE, mif_burst_mode & 0x7, 0, 3);//reg_burst_mode

	//plane1
	W_VCBUS_BIT(VFCE_MIF_PLANE1_MODE, mmu_mode_en & 0x1, 4, 1);//mmu mode en
	W_VCBUS_BIT(VFCE_MIF_PLANE1_MODE, mmu_page_mode & 0x1, 5, 1);//page size

	W_VCBUS_BIT(VFCE_MIF_PLANE1_LOOP, loop_mode_en & 0x1, 0, 1);//loop mode en
	W_VCBUS_BIT(VFCE_MIF_PLANE1_LOOP, loop_num & 0xfff, 2, 12);//loop num

	W_VCBUS_BIT(VFCE_MIF_PLANE1_MODE, mif_burst_mode & 0x7, 0, 3);//reg_burst_mode
}

void vdin_vfce_afrce_config(struct vdin_dev_s *devp)
{
	unsigned int reg_offset = 0;
	//0:R  1:RG  2:RGB  3:RGBA  4:Y/U/V  5: Y_UV  6:bayer 4 plane  7:bayer rg 4x16
	//8:bayer2x2 rg 8x16
	unsigned int reg_pixel_format;
	unsigned int reg_rgb2yuv_en;
	unsigned int reg_pixel_is_diff_chn;
	unsigned int reg_input_is_rgba;
	unsigned int reg_pixel_type_0;
	unsigned int reg_pixel_type_1;
	unsigned int comp_target_byte = 32;
	unsigned int header_last_extend_en;
	unsigned int raw_last_th;
	unsigned int max_ac_depth;

	//rtl only support yuv input
	reg_pixel_format	= 5;
	reg_pixel_type_0	= 3;
	reg_pixel_type_1	= 4;
	reg_rgb2yuv_en		= 0;
	reg_pixel_is_diff_chn	= 0;
	reg_input_is_rgba	= 0;

	if (devp->source_bitdepth > 10) {
		raw_last_th  = 12;
		max_ac_depth = 12;
		header_last_extend_en = 1;
	} else {
		raw_last_th  = 9;
		max_ac_depth = 9;
		header_last_extend_en = 0;
	}
	comp_target_byte = devp->cr_lossy_param.cr_lossy_target_byte;
	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_PIXEL_TYPE,
		((reg_pixel_type_0 & 0xf) << 12) | //plane 0 pixel type
		((reg_pixel_type_1 & 0xf) << 8)    //plane 1 pixel type
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_PIXEL_DEPTH,
		((devp->source_bitdepth & 0xf) << 12) | //plane 0 bits
		((devp->source_bitdepth & 0xf) << 8)    //plane 1 bits
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_TARGET_SIZE,
		((comp_target_byte & 0xff) << 24) | //plane 0 target byte
		((comp_target_byte & 0xff) << 16)   //plane 1 target byte
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_LAST_MAX_AC,
		((9 & 0xf) << 28) |  //plane 0 raw last th
		((9 & 0xf) << 24) |  //plane 1 raw last th
		((9 & 0xf) << 12) |  //plane 0 max ac depth
		((9 & 0xf) << 8)     //plane 1 max ac depth
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_HEADER_CTRL,
		((0xf & 0xf) << 16) |	//plane x raw mode en
		((0x0 & 0xf) << 12) |	//plane x force raw mode
		((0x0 & 0x1) << 11) |	//plane 0 pack mode
		((0x0 & 0x1) << 10) |	//plane 1 pack mode
		((0x3 & 0x3) << 8)  |	//plane x pack mode
		((header_last_extend_en & 0x1) << 7) |	//plane 0 header last extend
		((header_last_extend_en & 0x1) << 6) |	//plane 1 header last extend
		((0x0 & 0x3) << 4)  |	//plane x header last extend
		((0x0 & 0xf) << 0)	//plane x header mode
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_PACK_MODE_MIN_BYTE,
		((4 & 0x3f) << 24) | //plane 0 min byte
		((4 & 0x3f) << 16)   //plane 1 min byte
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_DICT_ERROR_TH,
		((25 & 0xff) << 24) |  //plane 0 dict error
		((25 & 0xff) << 16)    //plane 1 dict error
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_DICT_ENTER_TH,
		((0 & 0xff) << 24) | //plane 0 enter th
		((0 & 0xff) << 16)   //plane 1 enter th
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_CORE_DICT_CTRL,
		((1 & 0x7) << 13) |  //plane 0 diff th
		((1 & 0x7) << 10) |  //plane 1 diff th
		((0 & 0x1) << 3) |  //plane 0 dict en
		((0 & 0x1) << 2)    //plane 1 dict en
		);

	W_VCBUS(reg_offset + VFCE_AFRCE_INPUT_CTRL,
		((reg_rgb2yuv_en        & 0x1) << 26) | // rgb2yuv transform en
		((reg_pixel_is_diff_chn & 0x1) << 25) | // rgb2yuv dct en
		((reg_input_is_rgba     & 0x1) << 24) | // rgb or yuv header
		(((!reg_input_is_rgba)  & 0x7) << 20) | // input format
		((reg_pixel_format      & 0xf) << 16) | // pixel format
		((1  & 0x3) << 8)  // bayer mode
		);
}

void vdin_vfce_afbce_config(struct vdin_dev_s *devp)
{
	//int hold_line_num = VDIN_AFBCE_HOLD_LINE_NUM;
	//int lbuf_depth = 256;
	bool lossy_luma_en = 0;
	bool lossy_chrm_en = 0;
	//int cur_mmu_used = 0;
	int reg_format_mode;//0:444 1:422 2:420
	int reg_fmt444_comb;
	int bits_num;
	int uncompress_bits;
	int uncompress_size;
	int def_color_0 = 0x3ff;
	int def_color_1 = 0x200;
	int def_color_2 = 0x200;
	int def_color_3 = 0;
	//int h_blk_size_out = (devp->h_active + 31) >> 5;
	//int v_blk_size_out = (devp->v_active + 3)  >> 2;
	int blk_out_end_h;//output blk scope
	int blk_out_bgn_h;//output blk scope
	int blk_out_end_v;//output blk scope
	int blk_out_bgn_v;//output blk scope
	int enc_win_bgn_h;//input scope
	int enc_win_end_h;//input scope
	int enc_win_bgn_v;//input scope
	int enc_win_end_v;//input scope
	int reg_fmt444_rgb_en = 0;
	enum vdin_format_convert_e vdin_out_fmt;
	unsigned int bit_mode_shift = 0;
	unsigned int ram_offset_mode = 0;
	int ram_ram_size_mode = 1;

	if (!devp->afbce_info)
		return;

//	if (devp->h_active > 1024)
//		ram_offset_mode = 1;//for dpss only
//	else
//		ram_offset_mode = 0;

	enc_win_bgn_h = 0;
	enc_win_end_h = devp->h_active - 1;
	enc_win_bgn_v = 0;
	enc_win_end_v = devp->v_active - 1;

	blk_out_end_h	=  enc_win_bgn_h      >> 5 ;//output blk scope
	blk_out_bgn_h	= (enc_win_end_h + 31)	>> 5 ;//output blk scope
	blk_out_end_v	=  enc_win_bgn_v      >> 2 ;//output blk scope
	blk_out_bgn_v	= (enc_win_end_v + 3) >> 2 ;//output blk scope

	vdin_out_fmt = devp->afbc_format_convert;
	if (vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_RGB ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_GBR ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_YUV_BRG ||
	    vdin_out_fmt == VDIN_FORMAT_CONVERT_RGB_RGB)
		reg_fmt444_rgb_en = 1;

	reg_fmt444_comb = vdin_chk_is_comb_mode(devp);
	if (vdin_is_convert_to_nv21(devp->afbc_format_convert)) {
		reg_format_mode = 2;/*420*/
	    bits_num = 12;
	} else if (vdin_is_convert_to_422(devp->afbc_format_convert)) {
		reg_format_mode = 1;/*422*/
	    bits_num = 16;
	} else {
		reg_format_mode = 0;/*444*/
	    bits_num = 24;
	}
	if (reg_fmt444_comb) /* t6x */
		ram_ram_size_mode = 0;
	uncompress_bits = devp->source_bitdepth;
	if (devp->is_422_12bit_enabled) {
		uncompress_bits = 12;
		reg_format_mode = 1;/*422*/
		bits_num = 16;
		if (devp->afbce_valid && vdin_is_dolby_signal_in(devp)) {
			/* detunnel hsize */
			W_VCBUS_BIT(devp->addr_offset + VDIN_DETUNNEL_HSIZE, devp->h_active, 0, 13);
			/* reg_detunnel_sel */
			W_VCBUS_BIT(devp->addr_offset + VDIN_DETUNNEL_CTRL, 0x2c2d0, 2, 18);
			/* enable detunnel */
			W_VCBUS_BIT(devp->addr_offset + VDIN_DETUNNEL_CTRL, 1, 0, 1);
		}
	} else {
		/* disable detunnel */
		W_VCBUS_BIT(devp->addr_offset + VDIN_DETUNNEL_CTRL, 0, 0, 1);
	}

	if (devp->detunnel_enabled) {
		/* detunnel hsize */
		W_VCBUS_BIT(devp->addr_offset + VDIN_DETUNNEL_HSIZE, devp->h_active, 0, 13);
		/* reg_detunnel_sel */
		W_VCBUS_BIT(devp->addr_offset + VDIN_DETUNNEL_CTRL, 0x2c2d0, 2, 18);
		/* enable detunnel */
		W_VCBUS_BIT(devp->addr_offset + VDIN_DETUNNEL_CTRL, 1, 0, 1);
	}

	//bit size of uncompressed mode
	uncompress_size = (((((16 * uncompress_bits * bits_num) + 7) >> 3) + 31)
		      / 32) << 1;
	pr_info("%s fmt_convert:%d %d, comb:%d,uncompress_bits:%d %d %d\n", __func__,
		devp->format_convert, devp->afbc_format_convert,
		reg_fmt444_comb, uncompress_bits,
		reg_format_mode, bits_num);
//	W_VCBUS_BIT(AFBCE_MODE_EN, 1, 18, 1);/* disable order mode */
//
//	W_VCBUS_BIT(VDIN_WRARB_REQEN_SLV, 0x1, 3, 1);//vpu arb axi_enable
//	W_VCBUS_BIT(VDIN_WRARB_REQEN_SLV, 0x1, 7, 1);//vpu arb axi_enable
//
//	W_VCBUS(AFBCE_MODE,
//		(0 & 0x7) << 29 | (0 & 0x3) << 26 | (3 & 0x3) << 24 |
//		(hold_line_num & 0x7f) << 16 |
//		(2 & 0x3) << 14 | (reg_fmt444_comb & 0x1));

	if (devp->afbce_flag & VDIN_AFBCE_EN_LOSSY) {
		lossy_luma_en = TRUE;
		lossy_chrm_en = TRUE;
		pr_info("afrc afbce use lossy compression mode\n");
	}
	W_VCBUS(VFCE_AFBCE_QUANT_ENABLE, (0x3 << 10) | //chrm & luma quant expand en
		(lossy_chrm_en << 4) | (lossy_luma_en << 1));//lossy enable

//	W_VCBUS(AFBCE_SIZE_IN,
//		((devp->h_active & 0x1fff) << 16) |  // hsize_in of afbc input
//		((devp->v_active & 0x1fff) << 0)    // vsize_in of afbc input
//		);

//	W_VCBUS(AFBCE_BLK_SIZE_IN,
//		((h_blk_size_out & 0x1fff) << 16) |  // out blk hsize
//		((v_blk_size_out & 0x1fff) << 0)	  // out blk vsize
//		);

	//head addr of compressed data
//	W_VCBUS(AFBCE_HEAD_BADDR,
//		devp->afbce_info->fm_head_paddr[0] >> 4);

//	W_VCBUS_BIT(AFBCE_MIF_SIZE, (uncompress_size & 0x1fff), 16, 5);//uncompress_size

	/* how to set reg when we use crop ? */
	// scope of hsize_in ,should be a integer multiple of 32
	// scope of vsize_in ,should be a integer multiple of 4
//	W_VCBUS(AFBCE_PIXEL_IN_HOR_SCOPE,
//		((enc_win_end_h & 0x1fff) << 16) |
//		((enc_win_bgn_h & 0x1fff) << 0));

	// scope of hsize_in ,should be a integer multiple of 32
	// scope of vsize_in ,should be a integer multiple of 4
//	W_VCBUS(AFBCE_PIXEL_IN_VER_SCOPE,
//		((enc_win_end_v & 0x1fff) << 16) |
//		((enc_win_bgn_v & 0x1fff) << 0));
//
//	W_VCBUS(AFBCE_CONV_CTRL, lbuf_depth);//fix 256
//
//	W_VCBUS(AFBCE_MIF_HOR_SCOPE,
//		((blk_out_bgn_h & 0x3ff) << 16) |  // scope of out blk h_size
//		((blk_out_end_h & 0xfff) << 0)	  // scope of out blk v_size
//		);
//
//	W_VCBUS(AFBCE_MIF_VER_SCOPE,
//		((blk_out_bgn_v & 0x3ff) << 16) |  // scope of out blk h_size
//		((blk_out_end_v & 0xfff) << 0)	  // scope of out blk v_size
//		);

	W_VCBUS(VFCE_AFBCE_FORMAT,
		(2 & 0x7) << 12 | // reg_burst_length_add_value
		(reg_format_mode  & 0x3) << 8 |
		(uncompress_bits & 0xf) << 4 |
		(uncompress_bits & 0xf));

	W_VCBUS(VFCE_AFBCE_DEF_COLOR_1,
		((def_color_3 & 0xfff) << 12) |  // def_color_a
		((def_color_0 & 0xfff) << 0)	// def_color_y
		);

	if (devp->source_bitdepth >= VDIN_COLOR_DEEPS_8BIT &&
	    devp->source_bitdepth <= VDIN_COLOR_DEEPS_12BIT)
		bit_mode_shift = devp->source_bitdepth - VDIN_COLOR_DEEPS_8BIT;
	/*def_color_v*/
	/*def_color_u*/
	W_VCBUS(VFCE_AFBCE_DEF_COLOR_2,
		(((def_color_2 << bit_mode_shift) & 0xfff) << 12) |
		(((def_color_1 << bit_mode_shift) & 0xfff) << 0));
//	if (devp->dtdata->hw_ver >= VDIN_HW_T7)
//		W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4,
//			    devp->afbce_info->fm_table_paddr[0] >> 4, 0, 32);
//	else
//		W_VCBUS_BIT(AFBCE_MMU_RMIF_CTRL4,
//			    devp->afbce_info->fm_table_paddr[0], 0, 32);

//	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, cur_mmu_used, 0, 12);
	/*for almost uncompressed pattern,garbage at bottom
	 *(h_active * v_active * bytes per pixel + 3M) / page_size - 1
	 *where 3M is the rest bytes of block,since every block must not be\
	 *separated by 2 pages
	 */
//	W_VCBUS_BIT(AFBCE_MMU_RMIF_SCOPE_X, 0x1c4f, 16, 13);

//	W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_WORK_MD_BIT, AFBCE_WORK_MD_WID);

//	if (devp->double_wr)
//		W_VCBUS_BIT(AFBCE_ENABLE, 1, AFBCE_EN_BIT, AFBCE_EN_WID);
//	else
//		W_VCBUS_BIT(AFBCE_ENABLE, 0, AFBCE_EN_BIT, AFBCE_EN_WID);

//	if (devp->debug.dbg_print_cntl & VDIN_ADDRESS_DBG &&
//	    devp->irq_cnt < VDIN_DBG_PRINT_CNT)
//		pr_err("%s %#x:%#x(%#lx) %#x:%#x(%#lx)\n",
//			__func__, AFBCE_HEAD_BADDR, rd(0, AFBCE_HEAD_BADDR),
//			devp->afbce_info->fm_head_paddr[0],
//			AFBCE_MMU_RMIF_CTRL4, rd(0, AFBCE_MMU_RMIF_CTRL4),
//			devp->afbce_info->table_paddr);
	W_VCBUS(VFCE_TOP0_CTRL_2,
		(ram_ram_size_mode	 << 4) | // 1:for afbce 4k 444 10bit
		((0x0 & 0x7)		 << 1) | // ram arb rst
		((ram_offset_mode & 0x1) << 0)   // ram offset mode
		);
	if (devp->prop.polarity_vs == 1) /* positive */
		W_VCBUS(VFCE_TRIGGER, 0x8);//hold line,default value
	else
		W_VCBUS(VFCE_TRIGGER, 0x2);//hold line
}

void vdin_vfce_config(struct vdin_dev_s *devp)
{
	vdin_vfce_afbce_config(devp);
	vdin_vfce_afrce_config(devp);
	vdin_vfce_chnl(devp);
	vdin_vfce_mif(devp);
}

void vdin_vfce_set_next_frame(struct vdin_dev_s *devp,
			       unsigned int rdma_enable, struct vf_entry *vfe)
{
	unsigned char vf_idx;
	unsigned long compHeadAddr;
	unsigned long compTableAddr;
	unsigned long compBodyAddr;

	if (!devp->afbce_info)
		return;

	if ((vfe->flag & VF_FLAG_ONE_BUFFER_MODE) &&
	     devp->af_num < VDIN_CANVAS_MAX_CNT)
		vf_idx = devp->af_num; //one buffer mode
	else
		vf_idx = vfe->af_num;

	compHeadAddr	= devp->afbce_info->fm_head_paddr[vf_idx];
	compBodyAddr	= devp->afbce_info->fm_body_paddr[vf_idx];
	if (devp->cr_lossy_param.mmu_en)
		compTableAddr	= devp->afbce_info->fm_table_paddr[vf_idx];
	else
		compTableAddr	= devp->afbce_info->fm_body_paddr[vf_idx];

	vfe->vf.compHeadAddr = compHeadAddr;
	vfe->vf.compBodyAddr = compBodyAddr;

	if (devp->is_vfce_en) {
		vfe->vf.afrc_info.luma_head_addr = compHeadAddr;
		vfe->vf.afrc_info.luma_body_addr = compBodyAddr;
		vfe->vf.afrc_info.luma_dict_en   = 0;
		vfe->vf.afrc_info.luma_comp_target =
			devp->cr_lossy_param.cr_lossy_target_byte;
		vfe->vf.afrc_info.luma_header_en = 1;

		vfe->vf.afrc_info.chrm_head_addr = compHeadAddr +
			devp->afbce_info->frame_head_size_y;
		vfe->vf.afrc_info.chrm_body_addr = compBodyAddr +
			devp->afbce_info->frame_body_size_y;
		vfe->vf.afrc_info.chrm_dict_en   = 0;
		vfe->vf.afrc_info.chrm_comp_target =
			devp->cr_lossy_param.cr_lossy_target_byte;
		vfe->vf.afrc_info.chrm_header_en = 1;

		vfe->vf.afrc_info.mmu_mode_en    = 1;
		vfe->vf.afrc_info.mmu_page_mode  = 0;
		vfe->vf.afrc_info.mmu_baddr0 = compTableAddr;
		vfe->vf.afrc_info.mmu_baddr1 = compTableAddr +
			devp->afbce_info->frame_table_size_y;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		rdma_write_reg(devp->rdma_handle, VFCE_CHNL0_HEAD0_BADDR,
			compHeadAddr >> 4);
		rdma_write_reg(devp->rdma_handle, VFCE_CHNL0_BODY0_BADDR,
			compTableAddr >> 4);
		rdma_write_reg(devp->rdma_handle, VFCE_CHNL1_HEAD0_BADDR,
			compHeadAddr >> 4);
		rdma_write_reg(devp->rdma_handle, VFCE_CHNL1_BODY0_BADDR,
			compTableAddr >> 4);
		//rdma_write_reg_bits(devp->rdma_handle, AFBCE_ENABLE, 1,
		//		AFBCE_START_PULSE_BIT, AFBCE_START_PULSE_WID);
		if (devp->is_vfce_en) {
			rdma_write_reg(devp->rdma_handle, VFCE_CHNL0_HEAD1_BADDR,
				(compHeadAddr + devp->afbce_info->frame_head_size_y) >> 4);
			rdma_write_reg(devp->rdma_handle, VFCE_CHNL1_HEAD1_BADDR,
				(compHeadAddr + devp->afbce_info->frame_head_size_y) >> 4);
			rdma_write_reg(devp->rdma_handle, VFCE_CHNL0_BODY1_BADDR,
				(compTableAddr + devp->afbce_info->frame_table_size_y) >> 4);
			rdma_write_reg(devp->rdma_handle, VFCE_CHNL1_BODY1_BADDR,
				(compTableAddr + devp->afbce_info->frame_table_size_y) >> 4);
		}

		if (devp->pause_dec || devp->debug.pause_afbce_dec || devp->pause_dec_once) {
			rdma_write_reg_bits(devp->rdma_handle, VFCE_CHNL0_ENABLE, 0, 0, 1);
			rdma_write_reg_bits(devp->rdma_handle, VFCE_CHNL1_ENABLE, 0, 0, 1);
		} else {
			rdma_write_reg_bits(devp->rdma_handle, VFCE_CHNL0_ENABLE, 1, 0, 1);
			rdma_write_reg_bits(devp->rdma_handle, VFCE_CHNL1_ENABLE, 1, 0, 1);
		}
	} else {
		W_VCBUS(VFCE_CHNL0_HEAD0_BADDR, compHeadAddr >> 4);
		W_VCBUS(VFCE_CHNL0_BODY0_BADDR, compTableAddr >> 4);
		W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 1, 0, 1);
		W_VCBUS(VFCE_CHNL1_HEAD0_BADDR, compHeadAddr >> 4);
		W_VCBUS(VFCE_CHNL1_BODY0_BADDR, compTableAddr >> 4);
		W_VCBUS_BIT(VFCE_CHNL1_ENABLE, 1, 0, 1);
		if (devp->is_vfce_en) {
			W_VCBUS(VFCE_CHNL0_HEAD1_BADDR,
				(compHeadAddr + devp->afbce_info->frame_head_size_y) >> 4);
			W_VCBUS(VFCE_CHNL1_HEAD1_BADDR,
				(compHeadAddr + devp->afbce_info->frame_head_size_y) >> 4);
			W_VCBUS(VFCE_CHNL0_BODY1_BADDR,
				(compTableAddr + devp->afbce_info->frame_table_size_y) >> 4);
			W_VCBUS(VFCE_CHNL1_BODY1_BADDR,
				(compTableAddr + devp->afbce_info->frame_table_size_y) >> 4);
		}
	}
#endif
	vdin_afbce_clear_write_down_flag(devp);
}

void vdin_vfce_pause_write(struct vdin_dev_s *devp, unsigned int rdma_enable, bool pause_en)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (rdma_enable) {
		rdma_write_reg_bits(devp->rdma_handle, VFCE_CHNL0_ENABLE,
			!pause_en, 0, 1);
		rdma_write_reg_bits(devp->rdma_handle, VFCE_CHNL1_ENABLE,
			!pause_en, 0, 1);
	} else {
		W_VCBUS_BIT(VFCE_CHNL0_ENABLE, !pause_en, 0, 1);
		W_VCBUS_BIT(VFCE_CHNL1_ENABLE, !pause_en, 0, 1);
	}
#endif
}

/* frm_end will not pull up if using rdma IF to clear afbce flag */
void vdin_vfce_clear_write_down_flag(struct vdin_dev_s *devp)
{
	/* bit0:frm_end_clr;bit1:enc_error_clr */
	W_VCBUS_BIT(VFCE_CHNL0_CLR_FLAG, 3, 0, 2);
	W_VCBUS_BIT(VFCE_CHNL1_CLR_FLAG, 3, 0, 2);
}

/* return 1: write down */
int vdin_vfce_read_write_down_flag(struct vdin_dev_s *devp)
{
	int frm_end = -1;

	/* bit0-input,bit1-output;0-done,1-undone */
	frm_end = rd_bits(0, VFCE_CHNL0_STA0_FLAG, 0, 2);

	if (devp->debug.vdin_isr_monitor & VDIN_ISR_MONITOR_WRITE_DONE)
		pr_info("vfce sta0_flag:%#x,wr_done_ro:%#x\n",
			rd(0, VFCE_CHNL0_STA0_FLAG),
			rd(0, VDIN_WR_DONE_RO));

	if (frm_end == 0)
		return 1;
	else
		return 0;
}

void vdin_vfce_sw_reset(struct vdin_dev_s *devp)
{
	W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 0x0, 0, 1);
	W_VCBUS_BIT(VFCE_CHNL1_ENABLE, 0x0, 0, 1);
	//chnl_prot_sw_rst
	W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 1, 12, 1);
	W_VCBUS_BIT(VFCE_CHNL0_ENABLE, 0, 12, 1);
	//arb_sw_rst
	W_VCBUS_BIT(VFCE_CHNL0_WR_ARB_MISC, 1, 31, 1);
	W_VCBUS_BIT(VFCE_CHNL0_WR_ARB_MISC, 0, 31, 1);

	W_VCBUS_BIT(VFCE_CHNL1_ENABLE, 0x0, 0, 1);
	//chnl_prot_sw_rst
	W_VCBUS_BIT(VFCE_CHNL1_ENABLE, 1, 12, 1);
	W_VCBUS_BIT(VFCE_CHNL1_ENABLE, 0, 12, 1);
	//arb_sw_rst
	W_VCBUS_BIT(VFCE_CHNL1_WR_ARB_MISC, 1, 31, 1);
	W_VCBUS_BIT(VFCE_CHNL1_WR_ARB_MISC, 0, 31, 1);
}
