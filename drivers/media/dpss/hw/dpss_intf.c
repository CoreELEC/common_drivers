// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "define.h"
#include "dpss_intf.h"
#include "dpss_param.h"
#include "dpss_aa_pps.h"
#include "vfcd.h"
#include <linux/amlogic/media/di/dpss_interface.h>
#include "../dpss_base.h"
#include "../dpss_s.h"
#include "../dpss_sys.h"
#include "../dpss_func.h"

#include <linux/amlogic/media/dpss/dpss_frc.h>

//==============================================================//
//cfg_vfcd();
//cfg_vfce();
//==============================================================//
void dpss_mif_stride(struct DPSS_MIF_TYPE *dpss_mif, u32 *stride)
{
	u32 pic_hsize = dpss_mif->buf_hsize;

	switch ((dpss_mif->buf_full_pack << 12) |
		(dpss_mif->buf_separate << 8) | (dpss_mif->buf_bits_mode << 4) |
		(dpss_mif->buf_fmt_mode)) {
	case 0x1100:
		stride[0] = (pic_hsize * 8 + 127) / 128;
		stride[1] = (pic_hsize / 2 * 16 + 127) / 128;
		stride[2] = 0;
		break;		//full_pack,2plane,8bit ,420  nv21/nv12
	case 0x1200:
		stride[0] = (pic_hsize * 8 + 127) / 128;
		stride[1] = (pic_hsize / 2 * 8 + 127) / 128;
		stride[2] = stride[1];
		break;		//full_pack,3plane,8bit ,420  /
	case 0x1110:
		stride[0] = (pic_hsize * 12 + 127) / 128;
		stride[1] = (pic_hsize / 2 * 20 + 127) / 128;
		stride[2] = 0;
		break;		//full_pack,2plane,10bit,420  nv21/12
	case 0x0210:
		stride[0] = (pic_hsize * 12 + 127) / 128;
		stride[1] = (pic_hsize / 2 * 12 + 127) / 128;
		stride[2] = stride[1];
		break;		//old_pack ,3plane,10bit 420  /
	case 0x1130:
		stride[0] = (pic_hsize * 16 + 127) / 128;
		stride[1] = (pic_hsize / 2 * 32 + 127) / 128;
		stride[2] = 0;
		break;		//full_pack,2plane,16bit,420  nv21/12
	case 0x1001:
		stride[0] = (pic_hsize * 16 + 127) / 128;
		stride[1] = 0;
		stride[2] = 0;
		break;		//full_pack,1plane,8bit ,422  /
	case 0x1002:
		stride[0] = (pic_hsize * 24 + 127) / 128;
		stride[1] = 0;
		stride[2] = 0;
		break;		//full_pack,1plane,8bit ,444  /
	case 0x1011:
		stride[0] = (pic_hsize * 24 + 127) / 128;
		stride[1] = 0;
		stride[2] = 0;
		break;		//full_pack,1plane,10bit,422  /
	case 0x1012:
		stride[0] = (pic_hsize * 32 + 127) / 128;
		stride[1] = 0;
		stride[2] = 0;
		break;		//full_pack,1plane,10bit,444  /
	default:
		stride[0] = (pic_hsize * 24 + 127) / 128;
		stride[1] = 0;
		stride[2] = 0;
		break;		//full_pack,1plane,8bit,444   /
	}
	stride[0] = ((stride[0] + 3) >> 2) << 2;	//now need 64bytes aligned
	stride[1] = ((stride[1] + 3) >> 2) << 2;	//now need 64bytes aligned
	stride[2] = ((stride[2] + 3) >> 2) << 2;	//now need 64bytes aligned
}

s32 get_vfcd_regs_ofst(enum dpss_mif_e mif_index)
{
	s32 regs_ofst_dpe0 = (0x65 - 0x65) * 256;	//ary addr change *4
	s32 regs_ofst_dpe1 = (0x66 - 0x65) * 256;
	s32 regs_ofst_dpe2 = (0x67 - 0x65) * 256;
	s32 regs_ofst_dpe3 = (0x68 - 0x65) * 256;
	s32 regs_ofst_mc0 = (0x69 - 0x65) * 256;	//todo
	s32 regs_ofst_mc1 = (0x6a - 0x65) * 256;	//todo
	s32 regs_ofst_vd2 = (0x6b - 0x65) * 256;	//todo

	s32 regs_ofst;

	switch (mif_index) {
	case DPSS_RMIF_DPE0:
		regs_ofst = regs_ofst_dpe0;
		break;
	case DPSS_RMIF_DPE1:
		regs_ofst = regs_ofst_dpe1;
		break;
	case DPSS_RMIF_DPE2:
		regs_ofst = regs_ofst_dpe2;
		break;
	case DPSS_RMIF_DPE3:
		regs_ofst = regs_ofst_dpe3;
		break;
	case DPSS_RMIF_MC0:
		regs_ofst = regs_ofst_mc0;
		break;
	case DPSS_RMIF_MC1:
		regs_ofst = regs_ofst_mc1;
		break;
	case DPSS_RMIF_VD2:
		regs_ofst = regs_ofst_vd2;
		break;
	default:
		regs_ofst = regs_ofst_dpe0;
		break;
	}

	return regs_ofst;
}

void cfg_vfcd_rdmif_ext(enum dpss_mif_e mif_index, struct PRM_INTF_TYPE *prm_mif)
{
	s32 regs_ofst;
	u32 slc0_x_st_luma = prm_mif->slc_x_st[0];
	u32 slc0_x_ed_luma = prm_mif->slc_x_ed[0];
	u32 slc0_y_st_luma = prm_mif->slc_y_st[0];
	u32 slc0_y_ed_luma = prm_mif->slc_y_ed[0];
	u32 slc1_x_st_luma = prm_mif->slc_x_st[1];
	u32 slc1_x_ed_luma = prm_mif->slc_x_ed[1];
	u32 slc1_y_st_luma = prm_mif->slc_y_st[1];
	u32 slc1_y_ed_luma = prm_mif->slc_y_ed[1];
	u32 slc2_x_st_luma = prm_mif->slc_x_st[2];
	u32 slc2_x_ed_luma = prm_mif->slc_x_ed[2];
	u32 slc2_y_st_luma = prm_mif->slc_y_st[2];
	u32 slc2_y_ed_luma = prm_mif->slc_y_ed[2];
	u32 slc3_x_st_luma = prm_mif->slc_x_st[3];
	u32 slc3_x_ed_luma = prm_mif->slc_x_ed[3];
	u32 slc3_y_st_luma = prm_mif->slc_y_st[3];
	u32 slc3_y_ed_luma = prm_mif->slc_y_ed[3];

	u32 slc0_x_st_chrm = prm_mif->src_fmt != YUV444 ?
	    slc0_x_st_luma >> 1 : slc0_x_st_luma;
	u32 slc0_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
	    slc0_x_ed_luma >> 1 : slc0_x_ed_luma;
	u32 slc0_y_st_chrm = prm_mif->src_fmt == YUV420 ?
	    slc0_y_st_luma >> 1 : slc0_y_st_luma;
	u32 slc0_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
	    slc0_y_ed_luma >> 1 : slc0_y_ed_luma;
	u32 slc1_x_st_chrm = prm_mif->src_fmt != YUV444 ?
	    slc1_x_st_luma >> 1 : slc1_x_st_luma;
	u32 slc1_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
	    slc1_x_ed_luma >> 1 : slc1_x_ed_luma;
	u32 slc1_y_st_chrm = prm_mif->src_fmt == YUV420 ?
	    slc1_y_st_luma >> 1 : slc1_y_st_luma;
	u32 slc1_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
	    slc1_y_ed_luma >> 1 : slc1_y_ed_luma;
	u32 slc2_x_st_chrm = prm_mif->src_fmt != YUV444 ?
	    slc2_x_st_luma >> 1 : slc2_x_st_luma;
	u32 slc2_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
	    slc2_x_ed_luma >> 1 : slc2_x_ed_luma;
	u32 slc2_y_st_chrm = prm_mif->src_fmt == YUV420 ?
	    slc2_y_st_luma >> 1 : slc2_y_st_luma;
	u32 slc2_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
	    slc2_y_ed_luma >> 1 : slc2_y_ed_luma;
	u32 slc3_x_st_chrm = prm_mif->src_fmt != YUV444 ?
	    slc3_x_st_luma >> 1 : slc3_x_st_luma;
	u32 slc3_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
	    slc3_x_ed_luma >> 1 : slc3_x_ed_luma;
	u32 slc3_y_st_chrm = prm_mif->src_fmt == YUV420 ?
	    slc3_y_st_luma >> 1 : slc3_y_st_luma;
	u32 slc3_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
	    slc3_y_ed_luma >> 1 : slc3_y_ed_luma;

	regs_ofst = get_vfcd_regs_ofst(mif_index);

	wr_vc(regs_ofst + LUMA_RMIF_SCOPE_X, slc0_x_ed_luma << 16 | slc0_x_st_luma);
	//ch0 slc0 scope_x
	wr_vc(regs_ofst + LUMA_RMIF_SCOPE_Y, slc0_y_ed_luma << 16 | slc0_y_st_luma);
	//ch0 slc0 scope_y
	wr_vc(regs_ofst + LUMA_SLC1_XSIZE, slc1_x_ed_luma << 16 | slc1_x_st_luma);
	//ch0 slc1 scope_x
	wr_vc(regs_ofst + LUMA_SLC1_YSIZE, slc1_y_ed_luma << 16 | slc1_y_st_luma);
	//ch0 slc1 scope_y
	wr_vc(regs_ofst + LUMA_SLC2_XSIZE, slc2_x_ed_luma << 16 | slc2_x_st_luma);
	//ch0 slc2 scope_x
	wr_vc(regs_ofst + LUMA_SLC2_YSIZE, slc2_y_ed_luma << 16 | slc2_y_st_luma);
	//ch0 slc2 scope_y
	wr_vc(regs_ofst + LUMA_SLC3_XSIZE, slc3_x_ed_luma << 16 | slc3_x_st_luma);
	//ch0 slc2 scope_x
	wr_vc(regs_ofst + LUMA_SLC3_YSIZE, slc3_y_ed_luma << 16 | slc3_y_st_luma);
	//ch0 slc2 scope_y

	wr_vc(regs_ofst + CHRM_RMIF_SCOPE_X,
		slc0_x_ed_chrm << 16 | slc0_x_st_chrm);
	//ch1 slc0 scope_x
	wr_vc(regs_ofst + CHRM_RMIF_SCOPE_Y,
		slc0_y_ed_chrm << 16 | slc0_y_st_chrm);
	//ch1 slc0 scope_y
	wr_vc(regs_ofst + CHRM_SLC1_XSIZE,
		slc1_x_ed_chrm << 16 | slc1_x_st_chrm);
	//ch1 slc1 scope_x
	wr_vc(regs_ofst + CHRM_SLC1_YSIZE,
		slc1_y_ed_chrm << 16 | slc1_y_st_chrm);
	//ch1 slc1 scope_y
	wr_vc(regs_ofst + CHRM_SLC2_XSIZE,
		slc2_x_ed_chrm << 16 | slc2_x_st_chrm);
	//ch1 slc2 scope_x
	wr_vc(regs_ofst + CHRM_SLC2_YSIZE,
		slc2_y_ed_chrm << 16 | slc2_y_st_chrm);
	//ch1 slc2 scope_y
	wr_vc(regs_ofst + CHRM_SLC3_XSIZE,
		slc3_x_ed_chrm << 16 | slc3_x_st_chrm);
	//ch1 slc2 scope_x
	wr_vc(regs_ofst + CHRM_SLC3_YSIZE,
		slc3_y_ed_chrm << 16 | slc3_y_st_chrm);
	//ch1 slc2 scope_ya
}

void ena_vfcd_rdmif(enum dpss_mif_e mif_index, u32 on_off)
{
	s32 regs_ofst;

	regs_ofst = get_vfcd_regs_ofst(mif_index);

	w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL1, on_off, 19, 1);
	//ch0 reg_mif_en
	w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL1, on_off, 19, 1);
	//ch1 reg_mif_en
}

//ary add for lcevc:
void cfg_vfcd_rdmif_2ch_update_addr(enum dpss_mif_e mif_index, unsigned int y_addr,
					unsigned int u_addr) //have shift 4
{
	s32 regs_ofst;

	regs_ofst = get_vfcd_regs_ofst(mif_index);

	wr_vc(regs_ofst + MIF0_RMIF_CTRL4, y_addr);
	wr_vc(regs_ofst + MIF1_RMIF_CTRL4, u_addr);//ch1 baddr
}

void cfg_vfcd_rdmif_2ch(enum dpss_mif_e mif_index, struct PRM_INTF_TYPE *prm_mif,
			u32 fmt444_out)
{
	s32 regs_ofst;
	u32 frm_hsize = prm_mif->src_hsize;
	u32 frm_vsize = prm_mif->src_vsize;
	u32 src_bit = prm_mif->src_bit == BIT_008 ? 8
	    : prm_mif->src_bit == BIT_010 ? 10
	    : prm_mif->src_bit == BIT_P010 ? 16 : 12;
	u32 h_buf_size = 0;	//ary add
	u32 stride_y;
	u32 uv_swap, swap_64bit, little_endian;
	u32 plan_count = 0;
	u32 vfcd_nr_flag;
	bool vfcd_mc_flag;
	u32 pic_32byte_aligned;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	vfcd_nr_flag = mif_index == DPSS_RMIF_DPE0 ||
		mif_index == DPSS_RMIF_DPE1 ||
		mif_index == DPSS_RMIF_DPE2 ||
		mif_index == DPSS_RMIF_DPE3;

	vfcd_mc_flag = mif_index == DPSS_RMIF_MC0 || mif_index == DPSS_RMIF_MC1;

	if (prm_mif->src_plan == PLANAR_X1)
		plan_count = 1;
	else if (prm_mif->src_plan == PLANAR_X2)
		plan_count = 2;
	else if (prm_mif->src_plan == PLANAR_X3)
		plan_count = 3;

	if (prm_mif->canvas_hsize != 0) {
		if (prm_mif->src_fmt == YUV444) {
			if (plan_count == 1)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8 / 3;
			else if (plan_count == 3)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8;
		} else if (prm_mif->src_fmt == YUV422) {
			if (plan_count == 1)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8 / 2;
			else if (plan_count == 2)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8;
		} else if (prm_mif->src_fmt == YUV420) {
			if (plan_count == 2)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8;
		}
	}

	if (h_buf_size == 0)
		h_buf_size = frm_hsize;
	else
		dbg_h2("%s: index=%d: canvas_hsize=%d, plan=%d, src_bit=%d, h_buf_size=%d\n",
			__func__, mif_index, prm_mif->canvas_hsize,
			plan_count, src_bit, h_buf_size);

	dbg_h2("%s : fmt444_out=%d\n", __func__, fmt444_out);

	regs_ofst = get_vfcd_regs_ofst(mif_index);

	if (prm_mif->h_buf_size) {
		//h_buf_size = prm_mif->h_buf_size;
		dbg_h1("have h_buf_size:%d, %d\n", h_buf_size, frm_hsize);
	}

	if (prm_mif->src_plan == PLANAR_X1) {
		if (prm_mif->src_fmt == YUV444) {
			if (src_bit == 10)
				stride_y =
				(h_buf_size * (3 * src_bit + 2) + 511) / 512 * 4;
			else
				stride_y =
				(h_buf_size * src_bit * 3 + 511) / 512 * 4;
		} else {
			stride_y = (h_buf_size * src_bit * 2 + 511) / 512 * 4;
		}
	} else {
		stride_y = (h_buf_size * src_bit + 511) / 512 * 4;	//burst
	}

	u32 stride_c = stride_y;

	dbg_h2("%s:%d:stride:<0x%x>,%x.%d %d\n", __func__, mif_index, stride_y,
			prm_mif->src_plan, prm_mif->src_fmt, h_buf_size);
	dbg_h2("\t src_bit=%d, frm_hsize = %d\n", src_bit, frm_hsize);
	u32 y_baddr = prm_mif->src_baddr[0];
	u32 u_baddr = prm_mif->src_baddr[1];
	//DBG_INF("%s:addr:mif_index:%d,yaddr:0x%x\n", __func__,
	//mif_index, y_baddr);

	u32 slc0_x_st_luma = prm_mif->slc_x_st[0];
	u32 slc0_x_ed_luma = prm_mif->slc_x_ed[0];
	u32 slc0_y_st_luma = prm_mif->slc_y_st[0];
	u32 slc0_y_ed_luma = prm_mif->slc_y_ed[0];
	u32 slc1_x_st_luma = prm_mif->slc_x_st[1];
	u32 slc1_x_ed_luma = prm_mif->slc_x_ed[1];
	u32 slc1_y_st_luma = prm_mif->slc_y_st[1];
	u32 slc1_y_ed_luma = prm_mif->slc_y_ed[1];
	u32 slc2_x_st_luma = prm_mif->slc_x_st[2];
	u32 slc2_x_ed_luma = prm_mif->slc_x_ed[2];
	u32 slc2_y_st_luma = prm_mif->slc_y_st[2];
	u32 slc2_y_ed_luma = prm_mif->slc_y_ed[2];
	u32 slc3_x_st_luma = prm_mif->slc_x_st[3];
	u32 slc3_x_ed_luma = prm_mif->slc_x_ed[3];
	u32 slc3_y_st_luma = prm_mif->slc_y_st[3];
	u32 slc3_y_ed_luma = prm_mif->slc_y_ed[3];

	u32 slc0_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc0_x_st_luma >> 1 : slc0_x_st_luma;
	u32 slc0_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc0_x_ed_luma >> 1 : slc0_x_ed_luma;
	u32 slc0_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc0_y_st_luma >> 1 : slc0_y_st_luma;
	u32 slc0_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc0_y_ed_luma >> 1 : slc0_y_ed_luma;
	u32 slc1_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc1_x_st_luma >> 1 : slc1_x_st_luma;
	u32 slc1_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc1_x_ed_luma >> 1 : slc1_x_ed_luma;
	u32 slc1_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc1_y_st_luma >> 1 : slc1_y_st_luma;
	u32 slc1_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc1_y_ed_luma >> 1 : slc1_y_ed_luma;
	u32 slc2_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc2_x_st_luma >> 1 : slc2_x_st_luma;
	u32 slc2_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc2_x_ed_luma >> 1 : slc2_x_ed_luma;
	u32 slc2_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc2_y_st_luma >> 1 : slc2_y_st_luma;
	u32 slc2_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc2_y_ed_luma >> 1 : slc2_y_ed_luma;
	u32 slc3_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc3_x_st_luma >> 1 : slc3_x_st_luma;
	u32 slc3_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc3_x_ed_luma >> 1 : slc3_x_ed_luma;
	u32 slc3_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc3_y_st_luma >> 1 : slc3_y_st_luma;
	u32 slc3_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc3_y_ed_luma >> 1 : slc3_y_ed_luma;
	dbg_h2("rdmif slc0_x_ed=%4d rdmif slc0_y_ed=%4d\n", slc0_x_ed_luma,
		slc0_y_ed_luma);

	u32 fmt = prm_mif->src_fmt == YUV444 ?
		0 : prm_mif->src_fmt == YUV422 ? 1 : 2;
	u32 plan = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;
	u32 bit = prm_mif->src_bit == BIT_008 ?
		0 : prm_mif->src_bit == BIT_010 ?
		1 : prm_mif->src_bit == BIT_P010 ? 3 : 2;
	u32 y_pack_mode;	//0:1x 1:2x  2:4x  3:8x  4:16x  5:32x 6:64x
	u32 y_bits_mode;	//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	u32 cb_pack_mode;	//0:1x 1:2x  2:4x  3:8x  4:16x  5:32x 6:64x
	u32 cb_bits_mode;	//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	u32 mif_x_rev = prm_mif->reverse[0];
	u32 mif_y_rev = prm_mif->reverse[1];

	switch ((plan << 8) | (fmt << 4) | bit) {
	case 0x000:
		y_bits_mode = 9;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_8b
	case 0x001:
		y_bits_mode = 3;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_10b
	case 0x003:
		y_bits_mode = 13;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_16b
	case 0x010:
		y_bits_mode = 2;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_8b
	case 0x011:
		y_bits_mode = 10;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_10b
	case 0x012:
		y_bits_mode = 9;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_12b
	case 0x013:
		y_bits_mode = 3;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_16b
	case 0x110:
		y_bits_mode = 1;
		y_pack_mode = 1;
		cb_bits_mode = 2;
		cb_pack_mode = 1;
		break;		//422_2p_8b
	case 0x111:
		y_bits_mode = 10;
		y_pack_mode = 0;
		cb_bits_mode = 10;
		cb_pack_mode = 0;
		break;		//422_2p_10b
	case 0x112:
		y_bits_mode = 9;
		y_pack_mode = 0;
		cb_bits_mode = 9;
		cb_pack_mode = 1;
		break;		//422_2p_12b
	case 0x113:
		y_bits_mode = 2;
		y_pack_mode = 1;
		cb_bits_mode = 3;
		cb_pack_mode = 1;
		break;		//422_2p_16b
	case 0x120:
		y_bits_mode = 1;
		y_pack_mode = 1;
		cb_bits_mode = 2;
		cb_pack_mode = 1;
		break;		//420_2p_8b
	case 0x121:
		y_bits_mode = 10;
		y_pack_mode = 0;
		cb_bits_mode = 10;
		cb_pack_mode = 0;
		break;		//420_2p_10b
	case 0x122:
		y_bits_mode = 9;
		y_pack_mode = 0;
		cb_bits_mode = 9;
		cb_pack_mode = 1;
		break;		//420_2p_12b
	case 0x123:
		y_bits_mode = 2;
		y_pack_mode = 1;
		cb_bits_mode = 3;
		cb_pack_mode = 1;
		break;		//420_2p_16b
	default:
		y_bits_mode = 9;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;
	}

	bool hfmt_en = (fmt444_out == 1)  ? prm_mif->src_fmt != YUV444 : 0;
	bool vfmt_en = (fmt444_out != 0) ? prm_mif->src_fmt == YUV420 : 0;

	u32 mif1_en = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;
	// u32 luma_fifo_size = (frm_hsize>2048) ? 128  : 64;
	// u32 chrm_fifo_size = (frm_hsize>2048) ? 64   : 32;
	u32 luma_fifo_size;
	u32 chrm_fifo_size;
	u32 u_ram_ofst;
	u32 v_ram_ofst;
	u32 burst_len;

	luma_fifo_size = vfcd_mc_flag ? 128 : 64;
	chrm_fifo_size = vfcd_mc_flag ? 64 : 32;
	if (pchip_st && pchip_st->chip == ID_T6X) {
		u_ram_ofst = 64;
		v_ram_ofst = 96;
		burst_len = 4;
	} else {
		u_ram_ofst = vfcd_mc_flag ? 128 : 64;
		v_ram_ofst = vfcd_mc_flag ? 192 : 96;
		burst_len = 2 ;// axi_256 bst2  axi128 bst4
	}

	uv_swap = prm_mif->uv_swap ? 1 : 0;
	swap_64bit = prm_mif->swap_64bit ? 1 : 0;
	little_endian = prm_mif->little_endian ? 1 : 0;
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, u_ram_ofst, 10, 10);
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, v_ram_ofst, 20, 10);
	wr_vc(regs_ofst + RMIF_FIFO_SIZE,
		chrm_fifo_size << 16 | luma_fifo_size);
	// dbg_h2("cfg_vfcd_rdmif_2ch_reg_ofst:%d\n",regs_ofst);
	w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, mif_x_rev, 0, 1);
	w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, mif_y_rev, 1, 1);
	w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, little_endian, 2, 1);	//ary add
	w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, swap_64bit, 3, 1);
	w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, plan, 4, 2);
	w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, fmt, 6, 2);
	w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, bit, 8, 2);
	//w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, uv_swap, 11, 1); //ary add
	//ary for uv_swap use VFCD_AFBC_VD_CFMT_H
	w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL1, 1, 19, 1);
	w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL1, burst_len, 0, 3);
	//ch0 reg_mif_en
	w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL3, stride_y, 0, 13);
	//ch0 reg_stride
	w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL5, y_pack_mode, 0, 3);
	//ch0 out_pack_mode
	w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL5, y_bits_mode, 3, 4);
	//ch0 out_bits_mode
	wr_vc(regs_ofst + MIF0_RMIF_CTRL4, y_baddr >> 4);
	//ch0 baddr
	wr_vc(regs_ofst + LUMA_RMIF_SCOPE_X, slc0_x_ed_luma << 16 |
		slc0_x_st_luma);
	//ch0 slc0 scope_x
	wr_vc(regs_ofst + LUMA_RMIF_SCOPE_Y, slc0_y_ed_luma << 16 |
		slc0_y_st_luma);
	//ch0 slc0 scope_y
	wr_vc(regs_ofst + LUMA_SLC1_XSIZE, slc1_x_ed_luma << 16 |
		slc1_x_st_luma);
	//ch0 slc1 scope_x
	wr_vc(regs_ofst + LUMA_SLC1_YSIZE, slc1_y_ed_luma << 16 |
		slc1_y_st_luma);
	//ch0 slc1 scope_y
	wr_vc(regs_ofst + LUMA_SLC2_XSIZE, slc2_x_ed_luma << 16 |
		slc2_x_st_luma);
	//ch0 slc2 scope_x
	wr_vc(regs_ofst + LUMA_SLC2_YSIZE, slc2_y_ed_luma << 16 |
		slc2_y_st_luma);
	//ch0 slc2 scope_y
	wr_vc(regs_ofst + LUMA_SLC3_XSIZE, slc3_x_ed_luma << 16 |
		slc3_x_st_luma);
	//ch0 slc2 scope_x
	wr_vc(regs_ofst + LUMA_SLC3_YSIZE, slc3_y_ed_luma << 16 |
		slc3_y_st_luma);
	//ch0 slc2 scope_y
	w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL1, mif1_en, 19, 1);
	//ch1 reg_mif_en
	w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL3, stride_c, 0, 13);
	w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL1, burst_len, 0, 3);
	//ch1 reg_stride
	w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL5, cb_pack_mode, 0, 3);
	//ch1 out_pack_mode
	w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL5, cb_bits_mode, 3, 4);
	//ch1 out_bits_mode
	wr_vc(regs_ofst + MIF1_RMIF_CTRL4, u_baddr >> 4);	//ch1 baddr
	wr_vc(regs_ofst + CHRM_RMIF_SCOPE_X, slc0_x_ed_chrm << 16 | slc0_x_st_chrm);
	//ch1 slc0 scope_x
	wr_vc(regs_ofst + CHRM_RMIF_SCOPE_Y, slc0_y_ed_chrm << 16 | slc0_y_st_chrm);
	//ch1 slc0 scope_y
	wr_vc(regs_ofst + CHRM_SLC1_XSIZE, slc1_x_ed_chrm << 16 | slc1_x_st_chrm);
	//ch1 slc1 scope_x
	wr_vc(regs_ofst + CHRM_SLC1_YSIZE, slc1_y_ed_chrm << 16 | slc1_y_st_chrm);
	//ch1 slc1 scope_y
	wr_vc(regs_ofst + CHRM_SLC2_XSIZE, slc2_x_ed_chrm << 16 | slc2_x_st_chrm);
	//ch1 slc2 scope_x
	wr_vc(regs_ofst + CHRM_SLC2_YSIZE, slc2_y_ed_chrm << 16 | slc2_y_st_chrm);
	//ch1 slc2 scope_y
	wr_vc(regs_ofst + CHRM_SLC3_XSIZE, slc3_x_ed_chrm << 16 | slc3_x_st_chrm);
	//ch1 slc2 scope_x
	wr_vc(regs_ofst + CHRM_SLC3_YSIZE, slc3_y_ed_chrm << 16 | slc3_y_st_chrm);
	//ch1 slc2 scope_y
	// wr(regs_ofst+VFCD_SLC0_PIC_XPOS, slc0_x_st<<16|slc0_x_ed);
	// wr(regs_ofst+VFCD_SLC0_PIC_YPOS, slc0_y_st<<16|slc0_y_ed);
	// wr(regs_ofst+VFCD_SLC1_PIC_XPOS, slc1_x_st<<16|slc1_x_ed);
	// wr(regs_ofst+VFCD_SLC1_PIC_YPOS, slc1_y_st<<16|slc1_y_ed);
	// wr(regs_ofst+VFCD_SLC2_PIC_XPOS, slc2_x_st<<16|slc2_x_ed);
	// wr(regs_ofst+VFCD_SLC2_PIC_YPOS, slc2_y_st<<16|slc2_y_ed);
	// wr(regs_ofst+VFCD_SLC3_PIC_XPOS, slc3_x_st<<16|slc3_x_ed);
	// wr(regs_ofst+VFCD_SLC3_PIC_YPOS, slc3_y_st<<16|slc3_y_ed);
	if (prm_mif->block_mode && (mif_index == DPSS_RMIF_DPE0 ||
			mif_index == DPSS_RMIF_DPE1)) {
		pic_32byte_aligned = 1;
		w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL1, pic_32byte_aligned << 2 |
			prm_mif->block_mode, 20, 3);
		w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL1, pic_32byte_aligned << 2 |
			prm_mif->block_mode, 20, 3);
		w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL1, 0, 0, 3);
		w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL1, 0, 0, 3);
	} else {
		w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL1, 0, 20, 3);
		w_reg_bit_vc(regs_ofst + MIF1_RMIF_CTRL1, 0, 20, 3);
	}

	if (mif_index == DPSS_RMIF_DPE0 || mif_index == DPSS_RMIF_DPE1)
		w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 0, 27, 2);	//patch-0404
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL0, 2, 16, 2);	//reg_afbc_vd_sel
	w_reg_bit_vc(regs_ofst + VFCD_AFBC_CONV_CTRL, fmt, 12, 2);	//fmt_mode
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, 0x2a, 4, 6);	//compbits_yuv
	w_reg_bit_vc(regs_ofst + VFCD_TOP_CTRL2, 1, 31, 1);	//reg_vfcd_en

	u32 lbuf_en = ((mif_index == DPSS_RMIF_DPE2) ||
			(mif_index == DPSS_RMIF_DPE3)) ? 0 : (fmt == 2) && vfmt_en;
	u32 lbuf_depth = lbuf_en ? (frm_hsize + 31) / 32 : 0;

	wr_vc((regs_ofst + VFCD_CONV_BUF_LENS), ((0 & 0xfff) << 16) |
		// cbuf_depth
		((0 & 0xfff) << 0));
	// ybuf_depth
	wr_vc((regs_ofst + VFCD_AFBC_LBUF_DEPTH), ((lbuf_depth & 0xfff) << 16));
	// y_delaybuf_depth
	//    w_reg_bit(regs_ofst+VFCD_AFBC_VD_CFMT_CTRL   ,1,21,2);
	//chfmt_yc_ratio
	//    w_reg_bit(regs_ofst+VFCD_AFBC_VD_CFMT_CTRL   ,chfmt_en,20,1);
	//chfmt_en
	//    w_reg_bit(regs_ofst+VFCD_AFBC_VD_CFMT_CTRL   ,1,28,1);
	//chfmt_rpt_pix
	u32 vert_skip_uv = 0;
	u32 vert_skip_y = 0;
	u32 horz_skip_uv = 0;
	u32 horz_skip_y = 0;
	u32 uv_vsize_in = 0, vt_yc_ratio = 0, hz_yc_ratio = 0;
	//  u32 vfmt_en = fmt_mode==2 ? 1 :0;
	//  u32 hfmt_en = fmt_mode==0 ? 0 :1;

	if (fmt == 2) {		//420
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (frm_vsize >> 2);
				vt_yc_ratio = vert_skip_y == 0 ? 2 : 1;
			} else {
				uv_vsize_in = (frm_vsize >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = frm_vsize;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt == 1) {	//422
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (frm_vsize >> 1);
				vt_yc_ratio = vert_skip_y == 0 ? 1 : 0;
			} else {
				uv_vsize_in = (frm_vsize >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = frm_vsize;
			vt_yc_ratio = 0;
		}
		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt == 0) {	//444
		if (vfmt_en) {	//vert_skip_y==0,vert_skip_uv!=0
			uv_vsize_in = (frm_vsize >> 1);
			vt_yc_ratio = 1;
		} else {
			uv_vsize_in = frm_vsize;
			vt_yc_ratio = 0;
		}
		if (hfmt_en) {	//horz_skip_y==0,horz_skip_uv!=0
			hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	}
	//dbg_h2("===uv_vsize_in= %0x\n",uv_vsize_in );
	//dbg_h2("===vt_yc_ratio= %0x\n",vt_yc_ratio );
	//dbg_h2("===hz_yc_ratio= %0x\n",hz_yc_ratio );

	u32 vt_phase_step = (16 >> vt_yc_ratio);
	u32 vfmt_w = (frm_hsize >> hz_yc_ratio);

	wr_vc((regs_ofst + VFCD_POST_LUMA_SIZE), (frm_hsize & 0x1fff) << 16 |
		(frm_vsize & 0x1fff));

	if (vfcd_nr_flag)
		wr_vc((regs_ofst + VFCD_AFBC_VD_CFMT_CTRL), (0 << 28) |	//hz rpt pixel
			(0 << 24) |	//hz ini phase
			(0 << 23) |	//repeat p0 enable
			(hz_yc_ratio << 21) |	//hz yc ratio
			(hfmt_en << 20) |	//hz enable
			(0 << 19) |	//cvfmt_always_en
			(0 << 18) |	//cvfmt_lst_ln_rsp_dis
			(0 << 17) |	//nrpt_phase0 enable
			(0 << 16) |	//repeat l0 enable
			(0 << 12) |	//skip line num
			(0 << 8) |	//vt ini phase
			(vt_phase_step << 1) |	//vt phase step (3.4)
			(vfmt_en << 0));	//vt enable
	else
		wr_vc((regs_ofst + VFCD_AFBC_VD_CFMT_CTRL), (1 << 28) |	//hz rpt pixel
			(0 << 24) |	//hz ini phase
			(0 << 23) |	//repeat p0 enable
			(hz_yc_ratio << 21) |	//hz yc ratio
			(hfmt_en << 20) |	//hz enable
			(1 << 19) |	//cvfmt_always_en
			(0 << 18) |	//cvfmt_lst_ln_rsp_dis
			(1 << 17) |	//nrpt_phase0 enable
			(0 << 16) |	//repeat l0 enable
			(0 << 12) |	//skip line num
			(0 << 8) |	//vt ini phase
			(vt_phase_step << 1) |	//vt phase step (3.4)
			(vfmt_en << 0));	//vt enable

	wr_vc((regs_ofst + VFCD_AFBC_VD_CFMT_W), (frm_hsize << 16) | //hz format width
		(vfmt_w << 0));	//vt format width

	//wr((regs_ofst+VFCD_AFBC_VD_CFMT_H), uv_vsize_in);
	w_reg_bit_vc(regs_ofst + VFCD_AFBC_VD_CFMT_H, uv_vsize_in, 0, 13);
	w_reg_bit_vc(regs_ofst + VFCD_AFBC_VD_CFMT_H, uv_swap, 13, 1);

	//disable fmt size sw_mode when mc link
	w_reg_bit_vc(regs_ofst + VFCD_AFBC_VD_CFMT_H, 0, 18, 1);

/*
 *	u32 win_yout_hsize = slc0_x_ed_luma - slc0_x_st_luma + 1;
 *	u32 win_yout_vsize = slc0_y_ed_luma - slc0_y_st_luma + 1;
 *	u32 win_uvout_hsize =
 *		hfmt_en ? win_yout_hsize : slc0_x_ed_chrm - slc0_x_st_chrm + 1;
 *	u32 win_uvout_vsize =
 *		vfmt_en ? win_yout_vsize : slc0_y_ed_chrm - slc0_y_st_chrm + 1;
 */
	u32 win_yout_hsize;
	u32 win_yout_vsize;
//	u32 win_uvout_hsize;
	u32 win_uvout_vsize;

	u32 lst_overlap = prm_mif->lst_overlap;

	if (vfcd_nr_flag)
		win_yout_hsize = prm_mif->slc_num == 1 ? slc0_x_ed_luma - slc0_x_st_luma + 1 :
			prm_mif->slc_num == 2 ? slc1_x_ed_luma - slc1_x_st_luma + 1 :
			slc3_x_ed_luma - slc3_x_st_luma + 1;
	else
		win_yout_hsize = slc0_x_ed_luma - slc0_x_st_luma + 1;

	win_yout_hsize = win_yout_hsize - lst_overlap;
	win_yout_vsize = slc0_y_ed_luma - slc0_y_st_luma + 1;
#ifdef _HIS_CODE_ //2025-12-26 2290 stuck
	if (vfcd_nr_flag)
		win_uvout_hsize = hfmt_en ? win_yout_hsize :
			prm_mif->slc_num == 1 ? slc0_x_ed_chrm - slc0_x_st_chrm + 1 :
			prm_mif->slc_num == 2 ? slc1_x_ed_chrm - slc1_x_st_chrm + 1 :
				slc3_x_ed_chrm - slc3_x_st_chrm + 1;
	else
		win_uvout_hsize = slc0_x_ed_chrm - slc0_x_st_chrm + 1;
#endif
	win_uvout_vsize =
		vfmt_en ? win_yout_vsize : slc0_y_ed_chrm - slc0_y_st_chrm + 1;

	if (prm_mif->skip_line == 1) {
		win_yout_vsize = win_yout_vsize / 2;
		win_uvout_vsize = win_uvout_vsize / 2;
	}
	dbg_h2("%d %d %d\n", win_yout_hsize, win_yout_vsize, win_uvout_vsize);

	u32 pad_ydout_hofst = 0;
	u32 pad_ydout_vofst = 0;
	u32 pad_ydout_hsize;
	u32 pad_ydout_vsize;

	pad_ydout_hsize =
		prm_mif->pad_en ? (prm_mif->pad_hmode ==
				0 ? (win_yout_hsize +
					7) / 8 * 8 : (win_yout_hsize +
						  15) / 16 *
			       16) : win_yout_hsize;
	pad_ydout_hsize += lst_overlap;

	if (vfcd_nr_flag || vfcd_mc_flag)
		pad_ydout_vsize = win_yout_vsize;
	else
		pad_ydout_vsize = prm_mif->pad_en ? (prm_mif->pad_vmode == 0 ?
			(win_yout_vsize + 7) / 8 * 8 : (win_yout_vsize + 15) / 16 * 16) :
			win_yout_vsize;

	u32 pad_cdout_hofst = 0;
	u32 pad_cdout_vofst = 0;
	u32 pad_cdout_hsize;
	u32 pad_cdout_vsize;
#ifdef _HIS_CODE_ //12-26 2290 stuck
	if (vfcd_mc_flag)
		pad_cdout_hsize = prm_mif->pad_en ? (prm_mif->pad_hmode == 0 ?
				(win_uvout_hsize + 3) / 4 * 4 :
				(win_uvout_hsize + 7) / 8 * 8) : win_uvout_hsize;
	else
		pad_cdout_hsize = prm_mif->pad_en ? (prm_mif->pad_hmode == 0 ?
				(win_uvout_hsize + 7) / 8 * 8 :
				(win_uvout_hsize + 15) / 16 * 16) : win_uvout_hsize;
#endif
	pad_cdout_hsize = (prm_mif->src_fmt != YUV444) && (!hfmt_en) ?
			pad_ydout_hsize >> 1 : pad_ydout_hsize;

	//u32 pad_cdout_vsize =
	//    prm_mif->pad_en ? (prm_mif->pad_vmode ==
	//			0 ? (win_uvout_vsize +
	//				7) / 8 * 8 : (win_uvout_vsize +
	//					15) / 16 *
	//			16) : win_uvout_vsize;

	if (vfcd_nr_flag || vfcd_mc_flag)
		pad_cdout_vsize = win_uvout_vsize;
	else
		pad_cdout_vsize = prm_mif->pad_en ? (prm_mif->pad_vmode == 0 ?
			(win_uvout_vsize + 7) / 8 * 8 : (win_uvout_vsize + 15) / 16 * 16) :
			win_uvout_vsize;

	wr_vc((regs_ofst + VFCD_LUMA_PAD_SIZE),
		prm_mif->pad_en << 29 |
		((pad_ydout_vsize & 0x1fff) << 16) |
		(pad_ydout_hsize & 0x1fff));
	wr_vc((regs_ofst + VFCD_LUMA_PAD_OFST),
		pad_ydout_vofst << 16 | pad_ydout_hofst);

	wr_vc((regs_ofst + VFCD_CHRM_PAD_SIZE),
		pad_cdout_vsize << 16 | pad_cdout_hsize);
	wr_vc((regs_ofst + VFCD_CHRM_PAD_OFST),
		pad_cdout_vofst << 16 | pad_cdout_hofst);

	u32 index = regs_ofst / (256);	//ary addr change *4: //regs_ofst/(256*4)

	dbg_h2("intf idx=%d, pad_y_size=[%d, %d], pad_c_size=[%d, %d]\n",
		index, pad_ydout_hsize, pad_ydout_vsize,
		pad_cdout_hsize, pad_cdout_vsize);
	switch (index) {
	case 0:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12bit  {luma_data,chrm_data}
	case 1:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 2:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 3:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 4:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	case 5:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 6:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	default:
		wr_vc((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;
	}

	//cfg mmu mode:
	if (prm_mif->mmu_mode_en)
		w_reg_bit_vc(regs_ofst + VFCD_MMU_CTRL, 1, 0, 1);
	//yu.zong 2024-12-06 begin:

	//skip line enable
	u32 skip_en = prm_mif->skip_line > 0 ? 1 : 0;
	u32 skip_line_luma, skip_line_luma_t;

	skip_line_luma_t = prm_mif->skip_line;
	skip_line_luma = skip_line_luma_t == 0 ? 0 : skip_line_luma_t - 1;
	u32 skip_line_chrm = 0;

	skip_line_chrm = fmt == 2 ? skip_line_luma >> 1 : skip_line_luma;

	dbg_h2("skip_line =%4d\n", skip_line_luma);

	w_reg_bit_vc(regs_ofst + RMIF_F0_LUMA_RPT_PAT, skip_en, 3, 1);
	w_reg_bit_vc(regs_ofst + RMIF_F0_LUMA_RPT_PAT, skip_line_luma, 0, 3);
	w_reg_bit_vc(regs_ofst + RMIF_F0_CHRM_RPT_PAT, skip_en, 3, 1);
	w_reg_bit_vc(regs_ofst + RMIF_F0_CHRM_RPT_PAT, skip_line_chrm, 0, 3);
	//yu.zong 2024-12-06 end -----

	if (prm_mif->cut_win_en == 1 || skip_en)
//yu.zong 2024-12-06 add condition skip_en
		w_reg_bit_vc(regs_ofst + MIF0_RMIF_CTRL1, 1, 27, 2);
	dbg_h2("RMIF_TOP_CTRL 0x%x =%x\n", regs_ofst + RMIF_TOP_CTRL,
						rd_vc(regs_ofst + RMIF_TOP_CTRL));
	dbg_h2("MIF0_RMIF_CTRL1 0x%x =%x\n", regs_ofst + MIF0_RMIF_CTRL1,
						rd_vc(regs_ofst + MIF0_RMIF_CTRL1));
	dbg_h2("MIF0_RMIF_CTRL3 0x%x =%x\n", regs_ofst + MIF0_RMIF_CTRL3,
						rd_vc(regs_ofst + MIF0_RMIF_CTRL3));
	dbg_h2("MIF0_RMIF_CTRL5 0x%x =%x\n", regs_ofst + MIF0_RMIF_CTRL5,
						rd_vc(regs_ofst + MIF0_RMIF_CTRL5));
	dbg_h2("MIF1_RMIF_CTRL5 0x%x =%x\n", regs_ofst + MIF1_RMIF_CTRL5,
					rd_vc(regs_ofst + MIF1_RMIF_CTRL5));
	dbg_h2("VFCD_AFBC_VD_CFMT_H 0x%x =%x\n", regs_ofst + VFCD_AFBC_VD_CFMT_H,
					rd_vc(regs_ofst + VFCD_AFBC_VD_CFMT_H));
};

void frc_cfg_vfcd_rdmif_2ch(enum dpss_mif_e mif_index, struct PRM_INTF_TYPE *prm_mif,
			u32 fmt444_out)
{
	s32 regs_ofst;
	u32 frm_hsize = prm_mif->src_hsize;
	u32 frm_vsize = prm_mif->src_vsize;
	u32 src_bit = prm_mif->src_bit == BIT_008 ? 8
	    : prm_mif->src_bit == BIT_010 ? 10
	    : prm_mif->src_bit == BIT_P010 ? 16 : 12;
	u32 h_buf_size = 0;	//ary add
	u32 stride_y;
	u32 uv_swap, swap_64bit, little_endian;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	h_buf_size = frm_hsize;

	dbg_h2("%s : fmt444_out=%d\n", __func__, fmt444_out);

	regs_ofst = get_vfcd_regs_ofst(mif_index);

	dbg_h1("have h_buf_size:%d, %d\n", h_buf_size, frm_hsize);

	if (prm_mif->src_plan == PLANAR_X1) {
		if (prm_mif->src_fmt == YUV444) {
			if (src_bit == 10)
				stride_y =
				(h_buf_size * (3 * src_bit + 2) + 511) / 512 * 4;
			else
				stride_y =
				(h_buf_size * src_bit * 3 + 511) / 512 * 4;
		} else {
			stride_y = (h_buf_size * src_bit * 2 + 511) / 512 * 4;
		}
	} else {
		stride_y = (h_buf_size * src_bit + 511) / 512 * 4;	//burst
	}

	u32 stride_c = stride_y;

	dbg_h2("%s:%d:stride:<0x%x>,%x.%d %d\n", __func__, mif_index, stride_y,
			prm_mif->src_plan, prm_mif->src_fmt, h_buf_size);
	dbg_h2("\t src_bit=%d, frm_hsize = %d\n", src_bit, frm_hsize);
//	u32 y_baddr = prm_mif->src_baddr[0];
//	u32 u_baddr = prm_mif->src_baddr[1];
	//DBG_INF("%s:addr:mif_index:%d,yaddr:0x%x\n", __func__,
	//mif_index, y_baddr);

	u32 slc0_x_st_luma = prm_mif->slc_x_st[0];
	u32 slc0_x_ed_luma = prm_mif->slc_x_ed[0];
	u32 slc0_y_st_luma = prm_mif->slc_y_st[0];
	u32 slc0_y_ed_luma = prm_mif->slc_y_ed[0];
	u32 slc1_x_st_luma = prm_mif->slc_x_st[1];
	u32 slc1_x_ed_luma = prm_mif->slc_x_ed[1];
	u32 slc1_y_st_luma = prm_mif->slc_y_st[1];
	u32 slc1_y_ed_luma = prm_mif->slc_y_ed[1];
	u32 slc2_x_st_luma = prm_mif->slc_x_st[2];
	u32 slc2_x_ed_luma = prm_mif->slc_x_ed[2];
	u32 slc2_y_st_luma = prm_mif->slc_y_st[2];
	u32 slc2_y_ed_luma = prm_mif->slc_y_ed[2];
	u32 slc3_x_st_luma = prm_mif->slc_x_st[3];
	u32 slc3_x_ed_luma = prm_mif->slc_x_ed[3];
	u32 slc3_y_st_luma = prm_mif->slc_y_st[3];
	u32 slc3_y_ed_luma = prm_mif->slc_y_ed[3];

	u32 slc0_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc0_x_st_luma >> 1 : slc0_x_st_luma;
	u32 slc0_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc0_x_ed_luma >> 1 : slc0_x_ed_luma;
	u32 slc0_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc0_y_st_luma >> 1 : slc0_y_st_luma;
	u32 slc0_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc0_y_ed_luma >> 1 : slc0_y_ed_luma;
	u32 slc1_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc1_x_st_luma >> 1 : slc1_x_st_luma;
	u32 slc1_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc1_x_ed_luma >> 1 : slc1_x_ed_luma;
	u32 slc1_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc1_y_st_luma >> 1 : slc1_y_st_luma;
	u32 slc1_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc1_y_ed_luma >> 1 : slc1_y_ed_luma;
	u32 slc2_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc2_x_st_luma >> 1 : slc2_x_st_luma;
	u32 slc2_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc2_x_ed_luma >> 1 : slc2_x_ed_luma;
	u32 slc2_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc2_y_st_luma >> 1 : slc2_y_st_luma;
	u32 slc2_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc2_y_ed_luma >> 1 : slc2_y_ed_luma;
	u32 slc3_x_st_chrm = prm_mif->src_fmt != YUV444 ?
		slc3_x_st_luma >> 1 : slc3_x_st_luma;
	u32 slc3_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
		slc3_x_ed_luma >> 1 : slc3_x_ed_luma;
	u32 slc3_y_st_chrm = prm_mif->src_fmt == YUV420 ?
		slc3_y_st_luma >> 1 : slc3_y_st_luma;
	u32 slc3_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
		slc3_y_ed_luma >> 1 : slc3_y_ed_luma;
	dbg_h2("rdmif slc0_x_ed=%4d rdmif slc0_y_ed=%4d\n", slc0_x_ed_luma,
		slc0_y_ed_luma);

	u32 fmt = prm_mif->src_fmt == YUV444 ?
		0 : prm_mif->src_fmt == YUV422 ? 1 : 2;
	u32 plan = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;
	u32 bit = prm_mif->src_bit == BIT_008 ?
		0 : prm_mif->src_bit == BIT_010 ?
		1 : prm_mif->src_bit == BIT_P010 ? 3 : 2;
	u32 y_pack_mode;	//0:1x 1:2x  2:4x  3:8x  4:16x  5:32x 6:64x
	u32 y_bits_mode;	//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	u32 cb_pack_mode;	//0:1x 1:2x  2:4x  3:8x  4:16x  5:32x 6:64x
	u32 cb_bits_mode;	//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	u32 mif_x_rev = prm_mif->reverse[0];
	u32 mif_y_rev = prm_mif->reverse[1];

	switch ((plan << 8) | (fmt << 4) | bit) {
	case 0x000:
		y_bits_mode = 9;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_8b
	case 0x001:
		y_bits_mode = 3;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_10b
	case 0x003:
		y_bits_mode = 13;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_16b
	case 0x010:
		y_bits_mode = 2;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_8b
	case 0x011:
		y_bits_mode = 10;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_10b
	case 0x012:
		y_bits_mode = 9;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_12b
	case 0x013:
		y_bits_mode = 3;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_16b
	case 0x110:
		y_bits_mode = 1;
		y_pack_mode = 1;
		cb_bits_mode = 2;
		cb_pack_mode = 1;
		break;		//422_2p_8b
	case 0x111:
		y_bits_mode = 10;
		y_pack_mode = 0;
		cb_bits_mode = 10;
		cb_pack_mode = 0;
		break;		//422_2p_10b
	case 0x112:
		y_bits_mode = 9;
		y_pack_mode = 0;
		cb_bits_mode = 9;
		cb_pack_mode = 1;
		break;		//422_2p_12b
	case 0x113:
		y_bits_mode = 2;
		y_pack_mode = 1;
		cb_bits_mode = 3;
		cb_pack_mode = 1;
		break;		//422_2p_16b
	case 0x120:
		y_bits_mode = 1;
		y_pack_mode = 1;
		cb_bits_mode = 2;
		cb_pack_mode = 1;
		break;		//420_2p_8b
	case 0x121:
		y_bits_mode = 10;
		y_pack_mode = 0;
		cb_bits_mode = 10;
		cb_pack_mode = 0;
		break;		//420_2p_10b
	case 0x122:
		y_bits_mode = 9;
		y_pack_mode = 0;
		cb_bits_mode = 9;
		cb_pack_mode = 1;
		break;		//420_2p_12b
	case 0x123:
		y_bits_mode = 2;
		y_pack_mode = 1;
		cb_bits_mode = 3;
		cb_pack_mode = 1;
		break;		//420_2p_16b
	default:
		y_bits_mode = 9;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;
	}

	bool hfmt_en = (fmt444_out == 1)  ? prm_mif->src_fmt != YUV444 : 0;
	bool vfmt_en = (fmt444_out != 0) ? prm_mif->src_fmt == YUV420 : 0;

	u32 mif1_en = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;

	u32 luma_fifo_size;
	u32 chrm_fifo_size;
	u32 u_ram_ofst;
	u32 v_ram_ofst;
	u32 burst_len;

	luma_fifo_size = 128;
	chrm_fifo_size = 64;

	if (pchip_st && pchip_st->chip == ID_T6X) {
		u_ram_ofst = 64;
		v_ram_ofst = 96;
		burst_len = 4;
	} else {
		u_ram_ofst = 128;
		v_ram_ofst = 192;
		burst_len = 2;
	}

	uv_swap = prm_mif->uv_swap ? 1 : 0;
	swap_64bit = prm_mif->swap_64bit ? 1 : 0;
	little_endian = prm_mif->little_endian ? 1 : 0;
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, u_ram_ofst, 10, 10);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, v_ram_ofst, 20, 10);
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + RMIF_FIFO_SIZE,
		chrm_fifo_size << 16 | luma_fifo_size);
	// dbg_h2("cfg_vfcd_rdmif_2ch_reg_ofst:%d\n",regs_ofst);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_TOP_CTRL, mif_x_rev, 0, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_TOP_CTRL, mif_y_rev, 1, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_TOP_CTRL, little_endian, 2, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_TOP_CTRL, swap_64bit, 3, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_TOP_CTRL, plan, 4, 2);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_TOP_CTRL, fmt, 6, 2);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_TOP_CTRL, bit, 8, 2);
	//w_reg_bit_vc(regs_ofst + RMIF_TOP_CTRL, uv_swap, 11, 1); //ary add
	//ary for uv_swap use VFCD_AFBC_VD_CFMT_H
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF0_RMIF_CTRL1, 1, 19, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF0_RMIF_CTRL1, burst_len, 0, 3);
	//ch0 reg_mif_en
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF0_RMIF_CTRL3, stride_y, 0, 13);
	//ch0 reg_stride
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF0_RMIF_CTRL5, y_pack_mode, 0, 3);
	//ch0 out_pack_mode
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF0_RMIF_CTRL5, y_bits_mode, 3, 4);
	//ch0 out_bits_mode
	//VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + MIF0_RMIF_CTRL4, y_baddr >> 4);
	//ch0 baddr
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_RMIF_SCOPE_X, slc0_x_ed_luma << 16 |
		slc0_x_st_luma);
	//ch0 slc0 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_RMIF_SCOPE_Y, slc0_y_ed_luma << 16 |
		slc0_y_st_luma);
	//ch0 slc0 scope_y
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_SLC1_XSIZE, slc1_x_ed_luma << 16 |
		slc1_x_st_luma);
	//ch0 slc1 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_SLC1_YSIZE, slc1_y_ed_luma << 16 |
		slc1_y_st_luma);
	//ch0 slc1 scope_y
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_SLC2_XSIZE, slc2_x_ed_luma << 16 |
		slc2_x_st_luma);
	//ch0 slc2 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_SLC2_YSIZE, slc2_y_ed_luma << 16 |
		slc2_y_st_luma);
	//ch0 slc2 scope_y
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_SLC3_XSIZE, slc3_x_ed_luma << 16 |
		slc3_x_st_luma);
	//ch0 slc2 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + LUMA_SLC3_YSIZE, slc3_y_ed_luma << 16 |
		slc3_y_st_luma);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF1_RMIF_CTRL1, mif1_en, 19, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF1_RMIF_CTRL3, stride_c, 0, 13);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF1_RMIF_CTRL1, burst_len, 0, 3);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF1_RMIF_CTRL5, cb_pack_mode, 0, 3);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF1_RMIF_CTRL5, cb_bits_mode, 3, 4);
	//VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + MIF1_RMIF_CTRL4, u_baddr >> 4);	//ch1 baddr
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_RMIF_SCOPE_X,
		slc0_x_ed_chrm << 16 | slc0_x_st_chrm);
	//ch1 slc0 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_RMIF_SCOPE_Y,
		slc0_y_ed_chrm << 16 | slc0_y_st_chrm);
	//ch1 slc0 scope_y
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_SLC1_XSIZE,
		slc1_x_ed_chrm << 16 | slc1_x_st_chrm);
	//ch1 slc1 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_SLC1_YSIZE,
		slc1_y_ed_chrm << 16 | slc1_y_st_chrm);
	//ch1 slc1 scope_y
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_SLC2_XSIZE,
		slc2_x_ed_chrm << 16 | slc2_x_st_chrm);
	//ch1 slc2 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_SLC2_YSIZE,
		slc2_y_ed_chrm << 16 | slc2_y_st_chrm);
	//ch1 slc2 scope_y
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_SLC3_XSIZE,
		slc3_x_ed_chrm << 16 | slc3_x_st_chrm);
	//ch1 slc2 scope_x
	VSYNC_WR_VIDEO_TABLE_REG(regs_ofst + CHRM_SLC3_YSIZE,
		slc3_y_ed_chrm << 16 | slc3_y_st_chrm);

	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL0, 2, 16, 2);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_AFBC_CONV_CTRL, fmt, 12, 2);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, 0x2a, 4, 6);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_TOP_CTRL2, 1, 31, 1);

	u32 lbuf_en = ((mif_index == DPSS_RMIF_DPE2) ||
			(mif_index == DPSS_RMIF_DPE3)) ? 0 : (fmt == 2) && vfmt_en;
	u32 lbuf_depth = lbuf_en ? (frm_hsize + 31) / 32 : 0;

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CONV_BUF_LENS), ((0 & 0xfff) << 16) |
		((0 & 0xfff) << 0));
	// ybuf_depth
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_LBUF_DEPTH), ((lbuf_depth & 0xfff) << 16));

	u32 vert_skip_uv = 0;
	u32 vert_skip_y = 0;
	u32 horz_skip_uv = 0;
	u32 horz_skip_y = 0;
	u32 uv_vsize_in = 0, vt_yc_ratio = 0, hz_yc_ratio = 0;
	//  u32 vfmt_en = fmt_mode==2 ? 1 :0;
	//  u32 hfmt_en = fmt_mode==0 ? 0 :1;

	if (fmt == 2) {		//420
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (frm_vsize >> 2);
				vt_yc_ratio = vert_skip_y == 0 ? 2 : 1;
			} else {
				uv_vsize_in = (frm_vsize >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = frm_vsize;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt == 1) {	//422
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (frm_vsize >> 1);
				vt_yc_ratio = vert_skip_y == 0 ? 1 : 0;
			} else {
				uv_vsize_in = (frm_vsize >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = frm_vsize;
			vt_yc_ratio = 0;
		}
		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt == 0) {	//444
		if (vfmt_en) {	//vert_skip_y==0,vert_skip_uv!=0
			uv_vsize_in = (frm_vsize >> 1);
			vt_yc_ratio = 1;
		} else {
			uv_vsize_in = frm_vsize;
			vt_yc_ratio = 0;
		}
		if (hfmt_en) {	//horz_skip_y==0,horz_skip_uv!=0
			hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	}
	//dbg_h2("===uv_vsize_in= %0x\n",uv_vsize_in );
	//dbg_h2("===vt_yc_ratio= %0x\n",vt_yc_ratio );
	//dbg_h2("===hz_yc_ratio= %0x\n",hz_yc_ratio );

	u32 vt_phase_step = (16 >> vt_yc_ratio);
	u32 vfmt_w = (frm_hsize >> hz_yc_ratio);

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_POST_LUMA_SIZE), (frm_hsize & 0x1fff) << 16 |
		(frm_vsize & 0x1fff));

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_VD_CFMT_CTRL), (1 << 28) |
		(0 << 24) |	//hz ini phase
		(0 << 23) |	//repeat p0 enable
		(hz_yc_ratio << 21) |	//hz yc ratio
		(hfmt_en << 20) |	//hz enable
		(1 << 19) |	//cvfmt_always_en
		(0 << 18) |	//cvfmt_lst_ln_rsp_dis
		(1 << 17) |	//nrpt_phase0 enable
		(0 << 16) |	//repeat l0 enable
		(0 << 12) |	//skip line num
		(0 << 8) |	//vt ini phase
		(vt_phase_step << 1) |	//vt phase step (3.4)
		(vfmt_en << 0));	//vt enable

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_AFBC_VD_CFMT_W), (frm_hsize << 16) |
		(vfmt_w << 0));	//vt format width

	//wr((regs_ofst+VFCD_AFBC_VD_CFMT_H), uv_vsize_in);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_AFBC_VD_CFMT_H, uv_vsize_in, 0, 13);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_AFBC_VD_CFMT_H, uv_swap, 13, 1);
	//disable fmt size sw_mode when mc link
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_AFBC_VD_CFMT_H, 0, 18, 1);

	u32 win_yout_hsize;
	u32 win_yout_vsize;
	u32 win_uvout_hsize;
	u32 win_uvout_vsize;
	u32 vfcd_nr_flag;
	bool vfcd_mc_flag;

	vfcd_nr_flag = mif_index == DPSS_RMIF_DPE0 ||
		mif_index == DPSS_RMIF_DPE1 ||
		mif_index == DPSS_RMIF_DPE2 ||
		mif_index == DPSS_RMIF_DPE3;

	vfcd_mc_flag = mif_index == DPSS_RMIF_MC0 || mif_index == DPSS_RMIF_MC1;

	if (vfcd_nr_flag)
		win_yout_hsize = prm_mif->slc_num == 1 ? slc0_x_ed_luma - slc0_x_st_luma + 1 :
			prm_mif->slc_num == 2 ? slc1_x_ed_luma - slc1_x_st_luma + 1 :
			slc3_x_ed_luma - slc3_x_st_luma + 1;
	else
		win_yout_hsize = slc0_x_ed_luma - slc0_x_st_luma + 1;

	win_yout_vsize = slc0_y_ed_luma - slc0_y_st_luma + 1;

	if (vfcd_nr_flag)
		win_uvout_hsize = hfmt_en ? win_yout_hsize :
			prm_mif->slc_num == 1 ? slc0_x_ed_chrm - slc0_x_st_chrm + 1 :
			prm_mif->slc_num == 2 ? slc1_x_ed_chrm - slc1_x_st_chrm + 1 :
				slc3_x_ed_chrm - slc3_x_st_chrm + 1;
	else
		win_uvout_hsize = slc0_x_ed_chrm - slc0_x_st_chrm + 1;

	win_uvout_vsize =
		vfmt_en ? win_yout_vsize : slc0_y_ed_chrm - slc0_y_st_chrm + 1;

	if (prm_mif->skip_line == 1) {
		win_yout_vsize = win_yout_vsize / 2;
		win_uvout_vsize = win_uvout_vsize / 2;
	}
	dbg_h2("%d %d %d %d\n", win_yout_hsize, win_yout_vsize, win_uvout_hsize, win_uvout_vsize);

	u32 pad_ydout_hofst = 0;
	u32 pad_ydout_vofst = 0;
	u32 pad_ydout_hsize;
	u32 pad_ydout_vsize;

	pad_ydout_hsize =
		prm_mif->pad_en ? (prm_mif->pad_hmode ==
				0 ? (win_yout_hsize +
					7) / 8 * 8 : (win_yout_hsize +
						  15) / 16 *
			       16) : win_yout_hsize;

	if (vfcd_nr_flag || vfcd_mc_flag)
		pad_ydout_vsize = win_yout_vsize;
	else
		pad_ydout_vsize = prm_mif->pad_en ? (prm_mif->pad_vmode == 0 ?
			(win_yout_vsize + 7) / 8 * 8 : (win_yout_vsize + 15) / 16 * 16) :
			win_yout_vsize;

	u32 pad_cdout_hofst = 0;
	u32 pad_cdout_vofst = 0;
	u32 pad_cdout_hsize;
	u32 pad_cdout_vsize;

	if (vfcd_mc_flag)
		pad_cdout_hsize = prm_mif->pad_en ? (prm_mif->pad_hmode == 0 ?
				(win_uvout_hsize + 3) / 4 * 4 :
				(win_uvout_hsize + 7) / 8 * 8) : win_uvout_hsize;
	else
		pad_cdout_hsize = prm_mif->pad_en ? (prm_mif->pad_hmode == 0 ?
				(win_uvout_hsize + 7) / 8 * 8 :
				(win_uvout_hsize + 15) / 16 * 16) : win_uvout_hsize;

	if (vfcd_nr_flag || vfcd_mc_flag)
		pad_cdout_vsize = win_uvout_vsize;
	else
		pad_cdout_vsize = prm_mif->pad_en ? (prm_mif->pad_vmode == 0 ?
			(win_uvout_vsize + 7) / 8 * 8 : (win_uvout_vsize + 15) / 16 * 16) :
			win_uvout_vsize;

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_LUMA_PAD_SIZE),
		prm_mif->pad_en << 29 |
		((pad_ydout_vsize & 0x1fff) << 16) |
		(pad_ydout_hsize & 0x1fff));
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_LUMA_PAD_OFST),
		pad_ydout_vofst << 16 | pad_ydout_hofst);

	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CHRM_PAD_SIZE),
		pad_cdout_vsize << 16 | pad_cdout_hsize);
	VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_CHRM_PAD_OFST),
		pad_cdout_vofst << 16 | pad_cdout_hofst);

	u32 index = regs_ofst / (256);

	dbg_h2("intf idx=%0x, pad_ysize=[%d, %d], pad_csize=[%d, %d]\n",
		index, pad_ydout_hsize, pad_ydout_vsize,
		pad_cdout_hsize, pad_cdout_vsize);

	switch (index) {
	case 0:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12bit  {luma_data,chrm_data}
	case 1:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 2:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 3:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 4:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	case 5:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (512 & 0xfff));
		break;		//10
	case 6:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;		//12
	default:
		VSYNC_WR_VIDEO_TABLE_REG((regs_ofst + VFCD_PAD_DUMY_DATA),
			0 << 16 | (2048 & 0xfff));
		break;
	}

	//cfg mmu mode:
	if (prm_mif->mmu_mode_en)
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + VFCD_MMU_CTRL, 1, 0, 1);
	//yu.zong 2024-12-06 begin:

	//skip line enable
	u32 skip_en = prm_mif->skip_line > 0 ? 1 : 0;
	u32 skip_line_luma, skip_line_luma_t;

	skip_line_luma_t = prm_mif->skip_line;
	skip_line_luma = skip_line_luma_t == 0 ? 0 : skip_line_luma_t - 1;
	u32 skip_line_chrm = 0;

	skip_line_chrm = fmt == 2 ? skip_line_luma >> 1 : skip_line_luma;

	dbg_h2("skip_line =%4d\n", skip_line_luma);

	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_F0_LUMA_RPT_PAT, skip_en, 3, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_F0_LUMA_RPT_PAT, skip_line_luma, 0, 3);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_F0_CHRM_RPT_PAT, skip_en, 3, 1);
	VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + RMIF_F0_CHRM_RPT_PAT, skip_line_chrm, 0, 3);
	//yu.zong 2024-12-06 end -----

	if (prm_mif->cut_win_en == 1 || skip_en)
		VSYNC_WR_VIDEO_TABLE_REG_BITS(regs_ofst + MIF0_RMIF_CTRL1, 1, 27, 2);
};

void cfg_dpe_pix_wrmif(enum dpss_mif_e mif_index, struct PRM_INTF_TYPE *prm_mif)
{
	dbg_h2("cfg_dpe_pix_wr_mif start\n");
	s32 regs_ofst = mif_index == DPSS_WMIF_DPE0 ? 0x0
	//wr0   //ary addr change *4
		: mif_index == DPSS_WMIF_DPE1 ? 0x20	//wr0
		: mif_index == DPSS_WMIF_DPE2 ? 0x40	//wr0
		: mif_index == DPSS_WMIF_DPE3 ? 0x60	//ds0
		: mif_index == DPSS_WMIF_DPE4 ? 0x80 : 0xa0;

	u32 frm_hsize = prm_mif->src_hsize;
	//ary no use u32 frm_vsize = prm_mif->src_vsize;
	//u32 comp_num = 3;
	u32 comp_bits = prm_mif->src_bit == BIT_008 ? 8
		: prm_mif->src_bit == BIT_010 ? 10
		: prm_mif->src_bit == BIT_P010 ? 16 : 12;
	u32 bits_mode = prm_mif->src_bit == BIT_008 ? 0
		: prm_mif->src_bit == BIT_010 ? 1
		: prm_mif->src_bit == BIT_P010 ? 3 : 2;

	// reg_pix_fmt: 0 444, 1: 422, 2:420
	u32 fmt_mode = prm_mif->src_fmt == YUV444 ? 0
		: prm_mif->src_fmt == YUV422 ? 1 : 2;
	bool pix_separate = prm_mif->src_plan != PLANAR_X1;
	u32 luma_xbgn = prm_mif->luma_wr_x_st[0];
	u32 luma_xend = prm_mif->luma_wr_x_ed[0];
	u32 luma_ybgn = prm_mif->luma_wr_y_st[0];
	u32 luma_yend = prm_mif->luma_wr_y_ed[0];
	u32 luma_xbgn_slc1 = prm_mif->luma_wr_x_st[1];
	u32 luma_xend_slc1 = prm_mif->luma_wr_x_ed[1];
	u32 luma_ybgn_slc1 = prm_mif->luma_wr_y_st[1];
	u32 luma_yend_slc1 = prm_mif->luma_wr_y_ed[1];
	u32 luma_xbgn_slc2 = prm_mif->luma_wr_x_st[2];
	u32 luma_xend_slc2 = prm_mif->luma_wr_x_ed[2];
	u32 luma_ybgn_slc2 = prm_mif->luma_wr_y_st[2];
	u32 luma_yend_slc2 = prm_mif->luma_wr_y_ed[2];
	u32 luma_xbgn_slc3 = prm_mif->luma_wr_x_st[3];
	u32 luma_xend_slc3 = prm_mif->luma_wr_x_ed[3];
	u32 luma_ybgn_slc3 = prm_mif->luma_wr_y_st[3];
	u32 luma_yend_slc3 = prm_mif->luma_wr_y_ed[3];
	u32 chrm_xbgn = prm_mif->chrm_wr_x_st[0];
	u32 chrm_xend = prm_mif->chrm_wr_x_ed[0];
	u32 chrm_ybgn = prm_mif->chrm_wr_y_st[0];
	u32 chrm_yend = prm_mif->chrm_wr_y_ed[0];
	u32 chrm_xbgn_slc1 = prm_mif->chrm_wr_x_st[1];
	u32 chrm_xend_slc1 = prm_mif->chrm_wr_x_ed[1];
	u32 chrm_ybgn_slc1 = prm_mif->chrm_wr_y_st[1];
	u32 chrm_yend_slc1 = prm_mif->chrm_wr_y_ed[1];
	u32 chrm_xbgn_slc2 = prm_mif->chrm_wr_x_st[2];
	u32 chrm_xend_slc2 = prm_mif->chrm_wr_x_ed[2];
	u32 chrm_ybgn_slc2 = prm_mif->chrm_wr_y_st[2];
	u32 chrm_yend_slc2 = prm_mif->chrm_wr_y_ed[2];
	u32 chrm_xbgn_slc3 = prm_mif->chrm_wr_x_st[3];
	u32 chrm_xend_slc3 = prm_mif->chrm_wr_x_ed[3];
	u32 chrm_ybgn_slc3 = prm_mif->chrm_wr_y_st[3];
	u32 chrm_yend_slc3 = prm_mif->chrm_wr_y_ed[3];
	u32 stride_y = 0;
	u32 stride_cb = 0;
	u32 stride_cr = 0;

	dbg_h2("%s:mif_index=%d,regs_ofst=0x%x\n", __func__,
		mif_index, regs_ofst);
	if (pix_separate == 0) {
		if (fmt_mode == 0) {
			if (comp_bits == 10)
				stride_y =
				(frm_hsize * (3 * comp_bits + 2) + 127) >> 7;
			else
				stride_y =
				(frm_hsize * 3 * comp_bits + 127) >> 7;
		} else {
			stride_y = (frm_hsize * 2 * comp_bits + 127) >> 7;
		}
		stride_cb = 0;
		stride_cr = 0;
	} else if (pix_separate == 1) {
		stride_y = (frm_hsize * comp_bits + 127) >> 7;	//burst
		stride_cb = (frm_hsize * comp_bits + 127) >> 7;
		stride_cr = 0;
	}

	stride_y = ((stride_y + 3) >> 2) << 2;	//now need 64bytes aligned
	stride_cb = ((stride_cb + 3) >> 2) << 2;	//now need 64bytes aligned
	stride_cr = ((stride_cr + 3) >> 2) << 2;	//now need 64bytes aligned
	dbg_h2("%s:stride:<0x%x,0x%x>\n", __func__, stride_y, stride_cb);
	dbg_h2("\t hsize:%d, %d, %d, %d\n", frm_hsize,
		pix_separate, fmt_mode, comp_bits);
	u32 hmode = 0;
	u32 vmode = fmt_mode == 2 ? 1 : 2;
	//prm_mif->src_fmt==YUV420 ? 0 : 2;
	// 0: chrm use odd line 1:chrm use even line 2:chrm use all line
	wr(regs_ofst + VID_WMIF_CTRL1, (0 << 0)
	// reg_descramble_ctrl.
		| ((hmode & 0x3) << 20)    // reg_hmode
		| ((vmode & 0x3) << 22)    // reg_vmode
		| (prm_mif->uv_swap << 24)// reg_uvswap
		| ((pix_separate & 0x3) << 25)
		// reg_pix_separate 1:2plane 0:1plane
		| ((fmt_mode     & 0x3) << 26)
		// reg_pix_fmt: 0 444, 1: 422, 2:420, 3: 422 10bit no bubble
		| ((bits_mode    & 0x3) << 28));
		// reg_bits : 0: 8, 1: 10, 2: 12, 3: 16
	wr(regs_ofst + VID_WMIF_LUMA_CTRL0,
			(prm_mif->little_endian << 24)
			| (prm_mif->swap_64bit << 25));
	wr(regs_ofst + VID_WMIF_CHRM_CTRL0,
			(prm_mif->little_endian << 24)
			| (prm_mif->swap_64bit << 25));
	w_reg_bit(regs_ofst + VID_WMIF_CTRL0, 2, 4, 2);  // 2:burst8

	dbg_h2("wrmif pix_separate=%4d  wrmif fmt_mode=%4d\n", pix_separate,
		fmt_mode);
	dbg_h2("wrmif bits_mode=%4d\n", bits_mode);

	u32 luma_baddr;
	//  u32 luma_xbgn;
	//  u32 luma_xend;
	//  u32 luma_ybgn;
	//  u32 luma_yend;
	u32 chrm_baddr;
	//  u32 chrm_xbgn;
	//  u32 chrm_xend;
	//  u32 chrm_ybgn;
	//  u32 chrm_yend;

	luma_baddr = prm_mif->src_baddr[0];
	chrm_baddr = prm_mif->src_baddr[1];

	dbg_h2("wrmif luma_baddr=%x wrmif chrm_baddr=%x\n", luma_baddr,
		chrm_baddr);
	w_reg_bit(regs_ofst + VID_WMIF_CHRM_CTRL1, stride_cb, 0, 13);
	//reg_chrm_stride
	w_reg_bit(regs_ofst + VID_WMIF_LUMA_CTRL1, stride_y, 0, 13);
	//reg_luma_stride
	wr(regs_ofst + VID_WMIF_LUMA_BADDR, luma_baddr);
	//reg_luma_baddr
	wr(regs_ofst + VID_WMIF_LUMA_X, luma_xend << 16 | luma_xbgn);
	//reg_luma_x_start
	wr(regs_ofst + VID_WMIF_LUMA_Y, luma_yend << 16 | luma_ybgn);
	//reg_luma_y_start
	wr(regs_ofst + VID_WMIF_CHRM_BADDR, chrm_baddr);
	//reg_chrm_baddr
	wr(regs_ofst + VID_WMIF_CHRM_X, chrm_xend << 16 | chrm_xbgn);
	//reg_chrm_x_start
	wr(regs_ofst + VID_WMIF_CHRM_Y, chrm_yend << 16 | chrm_ybgn);
	//reg_chrm_y_start
	wr(regs_ofst + VID_WMIF_LUMA_X_SLC1, luma_xend_slc1 << 16 | luma_xbgn_slc1);
	//reg_luma_x_start
	wr(regs_ofst + VID_WMIF_LUMA_Y_SLC1, luma_yend_slc1 << 16 | luma_ybgn_slc1);
	//reg_luma_y_start
	wr(regs_ofst + VID_WMIF_CHRM_X_SLC1, chrm_xend_slc1 << 16 | chrm_xbgn_slc1);
	//reg_chrm_x_start
	wr(regs_ofst + VID_WMIF_CHRM_Y_SLC1, chrm_yend_slc1 << 16 | chrm_ybgn_slc1);
	//reg_chrm_y_start
	wr(regs_ofst + VID_WMIF_LUMA_X_SLC2, luma_xend_slc2 << 16 | luma_xbgn_slc2);
	//reg_luma_x_start
	wr(regs_ofst + VID_WMIF_LUMA_Y_SLC2, luma_yend_slc2 << 16 | luma_ybgn_slc2);
	//reg_luma_y_start
	wr(regs_ofst + VID_WMIF_CHRM_X_SLC2, chrm_xend_slc2 << 16 | chrm_xbgn_slc2);
	//reg_chrm_x_start
	wr(regs_ofst + VID_WMIF_CHRM_Y_SLC2, chrm_yend_slc2 << 16 | chrm_ybgn_slc2);
	//reg_chrm_y_start
	wr(regs_ofst + VID_WMIF_LUMA_X_SLC3, luma_xend_slc3 << 16 | luma_xbgn_slc3);
	//reg_luma_x_start
	wr(regs_ofst + VID_WMIF_LUMA_Y_SLC3, luma_yend_slc3 << 16 | luma_ybgn_slc3);
	//reg_luma_y_start
	wr(regs_ofst + VID_WMIF_CHRM_X_SLC3, chrm_xend_slc3 << 16 | chrm_xbgn_slc3);
	//reg_chrm_x_start
	wr(regs_ofst + VID_WMIF_CHRM_Y_SLC3, chrm_yend_slc3 << 16 | chrm_ybgn_slc3);
	//reg_chrm_y_start
	//cfg mmu mode:

	if (prm_mif->mmu_mode_en)
		w_reg_bit(regs_ofst + VID_WMIF_MMU_CTRL, 1, 0, 1);
	dbg_h2("VID_WMIF_CTRL1 0x%x =%x\n", regs_ofst + VID_WMIF_CTRL1,
					rd(regs_ofst + VID_WMIF_CTRL1));
	dbg_h2("VID_WMIF_LUMA_CTRL0 0x%x =%x\n", regs_ofst + VID_WMIF_LUMA_CTRL0,
					rd(regs_ofst + VID_WMIF_LUMA_CTRL0));
	dbg_h2("VID_WMIF_LUMA_BADDR 0x%x =%x\n", regs_ofst + VID_WMIF_LUMA_BADDR,
						rd(regs_ofst + VID_WMIF_LUMA_BADDR));
	dbg_h2("VID_WMIF_LUMA_X 0x%x =%x\n", regs_ofst + VID_WMIF_LUMA_X,
					rd(regs_ofst + VID_WMIF_LUMA_X));
	dbg_h2("VID_WMIF_LUMA_Y 0x%x =%x\n", regs_ofst + VID_WMIF_LUMA_Y,
						rd(regs_ofst + VID_WMIF_LUMA_Y));
	dbg_h2("VID_WMIF_LUMA_CTRL1 0x%x =%x\n", regs_ofst + VID_WMIF_LUMA_CTRL1,
					rd(regs_ofst + VID_WMIF_LUMA_CTRL1));
	dbg_h2("VID_WMIF_CHRM_CTRL0 0x%x =%x\n", regs_ofst + VID_WMIF_CHRM_CTRL0,
					rd(regs_ofst + VID_WMIF_CHRM_CTRL0));
	dbg_h2("VID_WMIF_CHRM_BADDR 0x%x =%x\n", regs_ofst + VID_WMIF_CHRM_BADDR,
					rd(regs_ofst + VID_WMIF_CHRM_BADDR));
	dbg_h2("VID_WMIF_CHRM_X 0x%x =%x\n", regs_ofst + VID_WMIF_CHRM_X,
					rd(regs_ofst + VID_WMIF_CHRM_X));
	dbg_h2("VID_WMIF_CHRM_Y 0x%x =%x\n", regs_ofst + VID_WMIF_CHRM_Y,
					rd(regs_ofst + VID_WMIF_CHRM_Y));
	dbg_h2("VID_WMIF_CHRM_CTRL1 0x%x =%x\n", regs_ofst + VID_WMIF_CHRM_CTRL1,
				rd(regs_ofst + VID_WMIF_CHRM_CTRL1));
};

void dpss_dpe_rdmif_cfg(u32 hsize, u32 baddr, u32 mode, u32 idx, u32 reg_grd,
			u32 xbgn_slc0, u32 xend_slc0, u32 ybgn_slc0,
			u32 yend_slc0, u32 xbgn_slc1, u32 xend_slc1,
			u32 ybgn_slc1, u32 yend_slc1, u32 xbgn_slc2,
			u32 xend_slc2, u32 ybgn_slc2, u32 yend_slc2,
			u32 xbgn_slc3, u32 xend_slc3, u32 ybgn_slc3,
			u32 yend_slc3)
{
	u32 reg_ofst = (idx << 5);	//ary addr change *4
	u32 bits_mode;
	u32 pack_mode;
	u32 stride;

	dbg_h2("%s:idx[%d],mode=%d, addr:0x%x\n", __func__, idx, mode, baddr);
	switch ((mode << 4) | idx) {	//mode=0:mc mode=1:nr
	case 0x00:
		bits_mode = 12;
		pack_mode = 4;
		stride = (((hsize * 1 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x01:
		bits_mode = 12;
		pack_mode = 4;
		stride = (((hsize * 1 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x02:
		bits_mode = 4;
		pack_mode = 4;
		stride = (((hsize * 64 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x03:
		bits_mode = 4;
		pack_mode = 4;
		stride = (((hsize * 64 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x04:
		bits_mode = 12;
		pack_mode = 0;
		stride = (((hsize * 1 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x05:
		bits_mode = 0;
		pack_mode = 0;
		stride = (((hsize * 1 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x06:
		bits_mode = 0;
		pack_mode = 0;
		stride = (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x07:
		bits_mode = 1;
		pack_mode = 1;
		stride = (((hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x08:
		bits_mode = 1;
		pack_mode = 1;
		stride = (((hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x09:
		bits_mode = 2;
		pack_mode = 2;
		stride = (((hsize * 16 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x0a:
		bits_mode = 5;
		pack_mode = 5;
		stride = (reg_grd * 8 * 128 + 511) >> 9 << 2;
		break;
	case 0x10:
		bits_mode = 9;
		pack_mode = 0;
		stride = (((hsize * 24 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x11:
		bits_mode = 9;
		pack_mode = 0;
		stride = (((hsize * 24 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x12:
		bits_mode = 4;
		pack_mode = 4;
		stride = (((hsize * 64 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x13:
		bits_mode = 4;
		pack_mode = 4;
		stride = (((hsize * 64 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x14:
		bits_mode = 0;
		pack_mode = 0;
		stride = (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x15:
		bits_mode = 0;
		pack_mode = 0;
		stride = (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x16:
		bits_mode = 0;
		pack_mode = 0;
		stride = (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x17:
		bits_mode = 1;
		pack_mode = 1;
		stride = (((hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x18:
		bits_mode = 1;
		pack_mode = 1;
		stride = (((hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x19:
		bits_mode = 2;
		pack_mode = 2;
		stride = (((hsize * 16 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x1a:
		bits_mode = 5;
		pack_mode = 5;
		stride = (reg_grd * 8 * 128 + 511) >> 9 << 2;
		break;
	default:
		bits_mode = 12;
		pack_mode = 4;
		stride = (((hsize * 1 + 127) >> 7) + 3) >> 2 << 2;
		break;
	}

	wr(VPU_VPSS_RMIF_SCOPE_X + reg_ofst, (xend_slc0 << 16) | xbgn_slc0);
	//RMIF_SCOPE_X
	wr(VPU_VPSS_RMIF_SCOPE_Y + reg_ofst, (yend_slc0 << 16) | ybgn_slc0);
	//RMIF_SCOPE_Y
	wr(VPU_VPSS_SLC1_XSIZE + reg_ofst, (xend_slc1 << 16) | xbgn_slc1);
	//RMIF_SCOPE_X
	wr(VPU_VPSS_SLC1_YSIZE + reg_ofst, (yend_slc1 << 16) | ybgn_slc1);
	//RMIF_SCOPE_Y
	wr(VPU_VPSS_SLC2_XSIZE + reg_ofst, (xend_slc2 << 16) | xbgn_slc2);
	//RMIF_SCOPE_X
	wr(VPU_VPSS_SLC2_YSIZE + reg_ofst, (yend_slc2 << 16) | ybgn_slc2);
	//RMIF_SCOPE_Y
	wr(VPU_VPSS_SLC3_XSIZE + reg_ofst, (xend_slc3 << 16) | xbgn_slc3);
	//RMIF_SCOPE_X
	wr(VPU_VPSS_SLC3_YSIZE + reg_ofst, (yend_slc3 << 16) | ybgn_slc3);
	//RMIF_SCOPE_Y

	wr(VPU_VPSS_RMIF_CTRL4 + reg_ofst, baddr >> 4);	//baddr
	w_reg_bit(VPU_VPSS_RMIF_CTRL3 + reg_ofst, stride, 0, 13);	//stride
	w_reg_bit(VPU_VPSS_RMIF_CTRL5 + reg_ofst,
		((bits_mode << 3) | pack_mode), 0, 7);	//bits_mode
	//w_reg_bit(VPU_VPSS_RMIF_CTRL5   + reg_ofst,
	//bits_mode,3 ,4 );//bits_mode
	//w_reg_bit(VPU_VPSS_RMIF_CTRL5   + reg_ofst,
	//pack_mode,0 ,3 );//pack_mode

	//dbg_h2("[dpss_dpe_rdmif_cfg]: VPU_VPSS_RMIF_SCOPE_X = %08x\n",
	//VPU_VPSS_RMIF_SCOPE_X);
	//dbg_h2("[dpss_dpe_rdmif_cfg]: VPU_VPSS_RMIF_SCOPE_X = %08x\n",
	//VPU_VPSS_RMIF_SCOPE_X + reg_ofst);
	//
	w_reg_bit(VPU_VPSS_RMIF_CTRL1 + reg_ofst, 2, 8, 2);
	//burst_len, 1: burst4, 2: brst8, 3: brst16
	dbg_h2("VPU_VPSS_RMIF_CTRL1 0x%x =%x\n", reg_ofst + VPU_VPSS_RMIF_CTRL1,
						rd(reg_ofst + VPU_VPSS_RMIF_CTRL1));
	dbg_h2("VPU_VPSS_RMIF_CTRL3 0x%x =%x\n", reg_ofst + VPU_VPSS_RMIF_CTRL3,
						rd(reg_ofst + VPU_VPSS_RMIF_CTRL3));
	dbg_h2("VPU_VPSS_RMIF_CTRL5 0x%x =%x\n", reg_ofst + VPU_VPSS_RMIF_CTRL5,
						rd(reg_ofst + VPU_VPSS_RMIF_CTRL5));
	dbg_h2("VPU_VPSS_RMIF_SCOPE_X 0x%x =%x\n", reg_ofst + VPU_VPSS_RMIF_SCOPE_X,
						rd(reg_ofst + VPU_VPSS_RMIF_SCOPE_X));
	dbg_h2("VPU_VPSS_RMIF_SCOPE_Y 0x%x =%x\n", reg_ofst + VPU_VPSS_RMIF_SCOPE_Y,
						rd(reg_ofst + VPU_VPSS_RMIF_SCOPE_Y));
}

void dpss_dpe_wrmif_cfg(u32 hsize, u32 baddr, u32 idx,
			u32 xbgn_slc0, u32 xend_slc0,
			u32 ybgn_slc0, u32 yend_slc0,
			u32 xbgn_slc1, u32 xend_slc1,
			u32 ybgn_slc1, u32 yend_slc1,
			u32 xbgn_slc2, u32 xend_slc2,
			u32 ybgn_slc2, u32 yend_slc2,
			u32 xbgn_slc3, u32 xend_slc3,
			u32 ybgn_slc3, u32 yend_slc3)
{
	u32 reg_ofst = (idx << 5);	//ary addr change *4
	u32 bits_mode;
	u32 pack_mode;
	u32 stride;

	dbg_h2("%s:idx[%d],addr=0x%x\n", __func__, idx, baddr);
	switch (idx) {
	case 0x0:
		bits_mode = 0;
		pack_mode = 0;
		stride = (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x1:
		bits_mode = 3;
		pack_mode = 3;
		stride = (((hsize * 32 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x2:
		bits_mode = 3;
		pack_mode = 3;
		stride = (((hsize * 32 + 127) >> 7) + 3) >> 2 << 2;
		break;
	case 0x3:
		bits_mode = 1;
		pack_mode = 1;
		stride = (((hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
		break;
	default:
		bits_mode = 0;
		pack_mode = 0;
		stride = (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
		break;
	}

	dbg_h2("%s:%d stride:<0x%x>\n", __func__, idx, stride);

	wr(VPU_VPSS_WMIF_SCOPE_X + reg_ofst, (xend_slc0 << 16) | xbgn_slc0);
	wr(VPU_VPSS_WMIF_SCOPE_Y + reg_ofst, (yend_slc0 << 16) | ybgn_slc0);
	wr(VPU_VPSS_WSLC1_XSIZE + reg_ofst, (xend_slc1 << 16) | xbgn_slc1);
	wr(VPU_VPSS_WSLC1_YSIZE + reg_ofst, (yend_slc1 << 16) | ybgn_slc1);
	wr(VPU_VPSS_WSLC2_XSIZE + reg_ofst, (xend_slc2 << 16) | xbgn_slc2);
	wr(VPU_VPSS_WSLC2_YSIZE + reg_ofst, (yend_slc2 << 16) | ybgn_slc2);
	wr(VPU_VPSS_WSLC3_XSIZE + reg_ofst, (xend_slc3 << 16) | xbgn_slc3);
	wr(VPU_VPSS_WSLC3_YSIZE + reg_ofst, (yend_slc3 << 16) | ybgn_slc3);

	wr(VPU_VPSS_WMIF_CTRL4  + reg_ofst, baddr >> 4);//baddr
	w_reg_bit(VPU_VPSS_WMIF_CTRL3  + reg_ofst, stride, 0, 13);//stride
	w_reg_bit(VPU_VPSS_WMIF_CTRL5  + reg_ofst,
			((bits_mode << 3) | pack_mode), 0, 6);//bits_mode
	//Wr_reg_bits(VPU_VPSS_WMIF_CTRL5 + reg_ofst, bits_mode, 3, 4 );//bits_mode
	//Wr_reg_bits(VPU_VPSS_WMIF_CTRL5 + reg_ofst, pack_mode, 0, 3 );//pack_mode

	w_reg_bit(VPU_VPSS_WMIF_CTRL1 + reg_ofst, 2, 8, 2);
	//burst_len : 1: brst2, 2: burst4
	dbg_h2("VPU_VPSS_WMIF_CTRL1 0x%x =%x\n", reg_ofst + VPU_VPSS_WMIF_CTRL1,
						rd(reg_ofst + VPU_VPSS_WMIF_CTRL1));
	dbg_h2("VPU_VPSS_WMIF_CTRL3 0x%x =%x\n", reg_ofst + VPU_VPSS_WMIF_CTRL3,
						rd(reg_ofst + VPU_VPSS_WMIF_CTRL3));
	dbg_h2("VPU_VPSS_WMIF_CTRL5 0x%x =%x\n", reg_ofst + VPU_VPSS_WMIF_CTRL5,
						rd(reg_ofst + VPU_VPSS_WMIF_CTRL5));
	dbg_h2("VPU_VPSS_WMIF_SCOPE_X 0x%x =%x\n", reg_ofst + VPU_VPSS_WMIF_SCOPE_X,
						rd(reg_ofst + VPU_VPSS_WMIF_SCOPE_X));
	dbg_h2("VPU_VPSS_WMIF_SCOPE_Y 0x%x =%x\n", reg_ofst + VPU_VPSS_WMIF_SCOPE_Y,
						rd(reg_ofst + VPU_VPSS_WMIF_SCOPE_Y));
}

void dpss_dae_wrmif_cfg_brst_len(u32 burst_len)	//12-05 add
{
	u32 reg_ofst;
	int idx;

	for (idx = 0; idx < 8; idx++) {
		reg_ofst = ((idx << 5)) - (0x0b00);	//ary addr / 4
		w_reg_bit(VPU_VPSS_WMIF_CTRL1 + reg_ofst, burst_len, 8, 2);
		//burst_len
	}
	w_reg_bit(VPU_VPSS_WMIF_CTRL1 - (0x0a00), burst_len, 8, 2);
	//burst_len  MEVP_AXIRD_ARBX8_BADDR1 ??
}

void dpss_dae_rdmif_cfg_brst_len(u32 burst_len)	//12-05 add
{
	u32 reg_ofst;
	u32 dae_ofst = (0xc100 - 0xb400);	//ary addr / 4
	u32 i;

	for (i = 0; i < 11; i++) {
		reg_ofst = (0x10 * i);	//ary addr / 4
		w_reg_bit(VPU_VPSS_RMIF_CTRL1 - dae_ofst + reg_ofst,
			burst_len, 8, 2);	//burst_len
	}
}

//ary no use ?
void dpss_dae_rdmif_cfg(u32 hsize, u32 vsize, u32 *baddr, u32 mode)
{
	u32 reg_ofst;
	u32 bits_mode = 0;
	u32 pack_mode;
	u32 stride;
	u32 dae_ofst = (0xc100 - 0xb400); //ary addr change *4   (0xc100-0xb400)<<2

	u32 me_hsize = (hsize > 960) ? hsize >> 1 : hsize;
	u32 me_vsize = vsize;
	u32 me_mv_hsize = (me_hsize + 3) / 4;
	u32 me_mv_vsize = (me_vsize + 3) / 4;
	u32 me_mv_hsize_d8 = (me_hsize / 4 + 7) / 8;
	u32 me_mv_hsize_3 = me_mv_hsize * 3;
	u32 me_hsize_p5d8 = (me_hsize * 5 + 7) / 8;
	u32 me_hsize_p6d8 = (me_hsize * 6 + 7) / 8;
	u32 xbgn_slc0 = 0;
	u32 xend_slc0;
	u32 ybgn_slc0 = 0;
	u32 yend_slc0;

	dbg_h2("%s:addr:0x%x\n", __func__, baddr[0]);
	u32 i;

	for (i = 0; i < 11; i++) {
		if (mode == 0) {	//frc
			switch (i) {	//mode=0:frc mode=1:nr
			case 0x0:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x1:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x2:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x3:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x4:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x5:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x6:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x7:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x8:
				pack_mode = 1;
				stride = (((me_hsize_p6d8 * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize_p6d8 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x9:
				pack_mode = 1;
				stride = (((me_hsize_p5d8 * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize_p5d8 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0xa:
				pack_mode = 1;
				stride = (((me_mv_hsize_d8 * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_d8 - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			//default:
			//	pack_mode = 0;
			//	stride = (((hsize * 4 + 127) >> 7) + 3) >>
			//	    2 << 2;
			//	xend_slc0 = me_hsize - 1;
			//	yend_slc0 = me_vsize - 1;
			//	break;
			}
		} else if (mode == 1) {	//nr
			switch (i) {	//mode=0:frc mode=1:nr //
			case 0x0:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x1:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x2:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x3:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x4:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 22;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x5:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x6:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x7:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x8:
				pack_mode = 1;
				stride = (((me_mv_hsize_3 * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_3 - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x9:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_hsize - 1;
				break;
			case 0xa:
				pack_mode = 1;
				stride = (((me_mv_hsize_d8 * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_d8 - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			//default:
			//	pack_mode = 0;
			//	stride = (((hsize * 4 + 127) >> 7) +
			//		  3) >> 2 << 2;
			//	xend_slc0 = me_hsize - 1;
			//	yend_slc0 = me_vsize - 1;
			//	break;
			}
		} else {	//di
			switch (i) {
			case 0x0:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x1:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x2:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x3:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x4:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x5:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 22;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x6:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x7:
				pack_mode = 4;
				stride = (((me_mv_hsize * 64 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x8:
				pack_mode = 1;
				stride = (((me_mv_hsize_3 * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_3 - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x9:
				pack_mode = 1;
				stride = (((me_hsize * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_hsize - 1;
				break;
			case 0xa:
				pack_mode = 1;
				stride = (((me_mv_hsize_d8 * 8 + 127) >> 7) +
					  3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_d8 - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			//default:
			//	pack_mode = 0;
			//	stride = (((hsize * 4 + 127) >> 7) +
			//		  3) >> 2 << 2;
			//	xend_slc0 = me_hsize - 1;
			//	yend_slc0 = me_vsize - 1;
			//	break;
			}
		}

		reg_ofst = (0x10 * i);	//ary addr change *4
		wr(VPU_VPSS_RMIF_SCOPE_X - dae_ofst + reg_ofst,
			(xend_slc0 << 16) | xbgn_slc0);	//RMIF_SCOPE_X
		wr(VPU_VPSS_RMIF_SCOPE_Y - dae_ofst + reg_ofst,
			(yend_slc0 << 16) | ybgn_slc0);	//RMIF_SCOPE_Y
		wr(VPU_VPSS_RMIF_CTRL4 - dae_ofst + reg_ofst, baddr[i]); //baddr
		w_reg_bit(VPU_VPSS_RMIF_CTRL3 - dae_ofst + reg_ofst, stride, 0, 13);
		//stride
		w_reg_bit(VPU_VPSS_RMIF_CTRL5 - dae_ofst + reg_ofst, bits_mode, 3, 4);
		//bits_mode
		w_reg_bit(VPU_VPSS_RMIF_CTRL5 - dae_ofst + reg_ofst, pack_mode, 0, 3);
		//pack_mode
		w_reg_bit(VPU_VPSS_RMIF_CTRL1 - dae_ofst + reg_ofst, 2, 8, 2);
		//burst_len: 2: burst4
	}
}

//ary no use
void dpss_dae_wrmif_cfg(u32 hsize, u32 vsize,
			u32 tfbf_hsize, u32 tfbf_vsize, u32 *baddr, u32 mode)
{
	u32 reg_ofst;
	u32 bits_mode = 0;
	u32 pack_mode;
	u32 stride;
	u32 dae_ofst = (0xc000 - 0xb500);	//ary addr change *4
	u32 xbgn_slc0 = 0;
	u32 xend_slc0;
	u32 ybgn_slc0 = 0;
	u32 yend_slc0;
	u32 i;

	u32 me_hsize = (hsize > 960) ? hsize >> 1 : hsize;
	u32 me_vsize = vsize;
	u32 me_hsize_d2 = (me_hsize + 1) / 2;
	u32 me_hsize_d16 = (me_hsize + 15) / 16;
	u32 me_hsize_p5d8 = (me_hsize * 5 + 7) / 8;
	u32 me_hsize_p6d8 = (me_hsize * 6 + 7) / 8;
	u32 me_mv_hsize = (me_hsize + 3) / 4;
	u32 me_mv_vsize = (me_vsize + 3) / 4;
	u32 me_mv_hsize_3 = me_mv_hsize * 3;
	u32 me_mv_hsize_8 = (me_hsize / 4 + 7) / 8;

	dbg_h2("%s:addr:0x%x\n", __func__, baddr[0]);

	for (i = 0; i < 9; i++) {
		if (mode == 0) {	//frc
			switch (i) {	//mode=0:frc mode=1:nr //
			case 0x0:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x1:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x2:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x3:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x4:
				pack_mode = 1;
				stride =
				    (((me_hsize_p6d8 * 8 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_hsize_p6d8 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x5:
				pack_mode = 1;
				stride =
				    (((me_hsize_p5d8 * 8 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_hsize_p5d8 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x6:
				pack_mode = 2;
				stride =
				    (((me_hsize_d2 * 16 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_hsize_d2 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x7:
				pack_mode = 2;
				stride =
				    (((me_hsize_d16 * 16 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_hsize_d16 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x8:
				pack_mode = 1;
				stride =
				    (((me_mv_hsize_8 * 8 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_8 - 1;
				yend_slc0 = me_mv_vsize;
				break;
			//default:
			//	pack_mode = 0;
			//	stride =
			//	    (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
			//	xend_slc0 = me_hsize - 1;
			//	yend_slc0 = me_vsize - 1;
			//	break;
			}
		} else if (mode == 1) {	//nr
			switch (i) {	//mode=0:frc mode=1:nr                        //
			case 0x0:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x1:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x2:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x3:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x4:
				pack_mode = 1;
				stride =
				    (((me_mv_hsize_3 * 8 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_3 - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x5:
				pack_mode = 1;
				stride =
				    (((me_hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x6:
				pack_mode = 2;
				stride =
				    (((me_hsize_d2 * 16 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_hsize_d2 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x7:
				pack_mode = 2;
				stride =
				    (((me_hsize_d16 * 16 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_hsize_d16 - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x8:
				pack_mode = 1;
				stride =
				    (((hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
				xend_slc0 = hsize - 1;
				yend_slc0 = vsize - 1;
				break;
			//default:
			//	pack_mode = 0;
			//	stride =
			//	    (((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
			//	xend_slc0 = me_hsize - 1;
			//	yend_slc0 = me_vsize - 1;
			//	break;
			}
		} else {	//di
			switch (i) {	//mode=0:frc mode=1:nr//
			case 0x0:
				pack_mode = 4;
				stride =
				    (((me_mv_hsize * 64 + 127) >> 7) +
				     3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x1:
				pack_mode = 2;
				stride =
					(((hsize * 16 + 127) >> 7) + 3) >> 2 << 2;
				xend_slc0 = hsize - 1;
				yend_slc0 = vsize - 1;
				break;
			case 0x2:
				pack_mode = 0;
				stride =
				    (((me_hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x3:
				pack_mode = 4;
				stride =
				(((me_mv_hsize * 64 + 127) >> 7) +
					3) >> 2 << 2;
				xend_slc0 = me_mv_hsize - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x4:
				pack_mode = 1;
				stride =
					(((me_mv_hsize_3 * 8 + 127) >> 7) +
					3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_3 - 1;
				yend_slc0 = me_mv_vsize - 1;
				break;
			case 0x5:
				pack_mode = 1;
				stride =
				(((me_hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x6:
				pack_mode = 1;
				stride =
				(((me_hsize * 8 + 127) >> 7) + 3) >> 2 << 2;
				xend_slc0 = me_hsize - 1;
				yend_slc0 = me_vsize - 1;
				break;
			case 0x7:
				pack_mode = 1;
				stride =
				(((tfbf_hsize * 8 + 127) >> 7) +
				3) >> 2 << 2;
				xend_slc0 = tfbf_hsize - 1;
				yend_slc0 = tfbf_vsize - 1;
				break;
			case 0x8:
				pack_mode = 1;
				stride =
				(((me_mv_hsize_8 * 8 + 127) >> 7) +
				3) >> 2 << 2;
				xend_slc0 = me_mv_hsize_8 - 1;
				yend_slc0 = me_mv_vsize;
				break;
			//default:
			//	pack_mode = 0;
			//	stride =
			//	(((hsize * 4 + 127) >> 7) + 3) >> 2 << 2;
			//	xend_slc0 = me_hsize - 1;
			//	yend_slc0 = me_vsize - 1;
			//	break;
			}
		}
		reg_ofst = (0x20 * (i % 8));	//ary addr change *4
		if (i == 8)
			dae_ofst = dae_ofst - (0x100);	//ary addr change *4
		wr(VPU_VPSS_WMIF_SCOPE_X - dae_ofst + reg_ofst,
		   (xend_slc0 << 16) | xbgn_slc0);
		wr(VPU_VPSS_WMIF_SCOPE_Y - dae_ofst + reg_ofst,
		   (yend_slc0 << 16) | ybgn_slc0);
		wr(VPU_VPSS_WMIF_CTRL4 - dae_ofst + reg_ofst, baddr[i]); //baddr
		w_reg_bit(VPU_VPSS_WMIF_CTRL3 - dae_ofst + reg_ofst, stride, 0, 13);
		//stride
		w_reg_bit(VPU_VPSS_WMIF_CTRL5 - dae_ofst + reg_ofst, bits_mode, 3, 4);
		//bits_mode
		w_reg_bit(VPU_VPSS_WMIF_CTRL5 - dae_ofst + reg_ofst, pack_mode, 0, 3);
		//pack_mode
		w_reg_bit(VPU_VPSS_WMIF_CTRL1 - dae_ofst + reg_ofst, 3, 8, 2);
		//burst_len
	}
}

s32 get_pix_rdmif_regs_ofst(s32 mif_index)
{
	s32 regs_ofst_inp = 0;
	s32 regs_ofst_dae = (0xb4c0 - 0x9800);
	//TODO  //ary addr change *4  0x1cc0
	s32 regs_ofst_dct = (0xd7c0 - 0x9800);
	//TODO  //ary addr change *4  0x3fc0
	s32 regs_ofst;

	switch (mif_index) {
	case 0:
		regs_ofst = regs_ofst_inp;
		break;
	case 1:
		regs_ofst = regs_ofst_dae;
		break;
	case 2:
		regs_ofst = regs_ofst_dct;
		break;
	default:
		regs_ofst = regs_ofst_inp;
		break;
	}

	return regs_ofst;
}

void cfg_viu_pix_rdmif_baddr(s32 mif_index,
			     u32 ybaddr, u32 cbbaddr, u32 crbaddr)
{
	s32 regs_ofst;

	dbg_h2("%s:mif_index:%d, addr:0x%x\n", __func__, mif_index, ybaddr);

	regs_ofst = get_pix_rdmif_regs_ofst(mif_index);
	wr(regs_ofst + INP_MIF0_RMIF_CTRL4, ybaddr);
	wr(regs_ofst + INP_MIF1_RMIF_CTRL4, cbbaddr);
	//wr(regs_ofst+INP_MIF2_RMIF_CTRL4,crbaddr);
}

void dpss_cfg_viu_pix_rdmif_di_update(unsigned int y, unsigned int u)
{
	unsigned int regs_ofst = (0xb4c0 - 0x9800);

	wr(regs_ofst + INP_LUMA_RMIF_SCOPE_Y, y);
	wr(regs_ofst + INP_CHRM_RMIF_SCOPE_Y, u);
}

void dpss_cfg_viu_pix_rdmif(struct PRM_INTF_TYPE *prm_mif, s32 mif_index)
{
	//s32 regs_ofst_inp = (0x98 - 0x98)*256*4;
	//s32 regs_ofst_dae = (0xb4 - 0x98)*256*4+0xc0;   //TODO
	s32 regs_ofst_inp = 0;
	s32 regs_ofst_dae = (0xb4c0 - 0x9800);
	//TODO  //ary addr change *4 = 0x1cc0
	s32 regs_ofst_dct = (0xd7c0 - 0x9800);
	//TODO  //ary addr change *4 0x3fc0
	s32 regs_ofst_dv = (0x11880 - 0x76000);
	//((0x4620<<2) - (0x50000+(0x9800<<2)) //set_need_check /4 ?
	s32 regs_ofst;
	u32 frm_hsize = prm_mif->src_hsize;
	//ary no use u32 frm_vsize = prm_mif->src_vsize;
	u32 src_bit = prm_mif->src_bit == BIT_008 ? 8
		: prm_mif->src_bit == BIT_010 ? 10
		: prm_mif->src_bit == BIT_P010 ? 16 : 12;
	u32 stride_y;
	u32 h_buf_size = 0;
	u32 plan_count = 0;

	dbg_h2("%s:start:mif_index:%d\n", __func__, mif_index);

	switch (mif_index) {
	case 0:
		regs_ofst = regs_ofst_inp;
		break;
	case 1:
		regs_ofst = regs_ofst_dae;
		break;
	case 2:
		regs_ofst = regs_ofst_dct;
		break;
	case 3:
		regs_ofst = regs_ofst_dv;
		break;
	default:
		regs_ofst = regs_ofst_inp;
		break;
	}
	if (prm_mif->src_plan == PLANAR_X1)
		plan_count = 1;
	else if (prm_mif->src_plan == PLANAR_X2)
		plan_count = 2;
	else if (prm_mif->src_plan == PLANAR_X3)
		plan_count = 3;

	if (prm_mif->canvas_hsize != 0) {
		if (prm_mif->src_fmt == YUV444) {
			if (plan_count == 1)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8 / 3;
			else if (plan_count == 3)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8;
		} else if (prm_mif->src_fmt == YUV422) {
			if (plan_count == 1)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8 / 2;
			else if (plan_count == 2)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8;
		} else if (prm_mif->src_fmt == YUV420) {
			if (plan_count == 2)
				h_buf_size = prm_mif->canvas_hsize / src_bit * 8;
		}
	}

	if (h_buf_size == 0)
		h_buf_size = frm_hsize;
	else
		dbg_h2("%s: index=%d: canvas_hsize=%d, plan=%d, src_bit=%d, h_buf_size=%d\n",
			__func__, mif_index, prm_mif->canvas_hsize,
			plan_count, src_bit, h_buf_size);

	if (prm_mif->src_plan == PLANAR_X1) {
		if (prm_mif->src_fmt == YUV444) {
			if (src_bit == 10)
				stride_y =
				(frm_hsize * (3 * src_bit + 2) + 511) / 512 * 4;
			else
				stride_y =
				(h_buf_size * src_bit * 3 + 511) / 512 * 4;
		} else {
			stride_y = (h_buf_size * src_bit * 2 + 511) / 512 * 4;
		}

	} else {
		stride_y = (h_buf_size * src_bit + 511) / 512 * 4;
	}

	u32 stride_c = stride_y;
	u32 rev_x = prm_mif->reverse[0];
	u32 rev_y = prm_mif->reverse[1];
	u32 y_baddr = prm_mif->src_baddr[0];
	u32 u_baddr = prm_mif->src_baddr[1];
	u32 slc0_x_st_luma = prm_mif->slc_x_st[0];
	u32 slc0_x_ed_luma = prm_mif->slc_x_ed[0];
	u32 slc0_y_st_luma = prm_mif->slc_y_st[0];
	u32 slc0_y_ed_luma = prm_mif->slc_y_ed[0];
	//   u32 slc1_x_st_luma= prm_mif->slc_x_st[1];
	//   u32 slc1_x_ed_luma= prm_mif->slc_x_ed[1];
	//   u32 slc1_y_st_luma= prm_mif->slc_y_st[1];
	//   u32 slc1_y_ed_luma= prm_mif->slc_y_ed[1];
	//   u32 slc2_x_st_luma= prm_mif->slc_x_st[2];
	//   u32 slc2_x_ed_luma= prm_mif->slc_x_ed[2];
	//   u32 slc2_y_st_luma= prm_mif->slc_y_st[2];
	//   u32 slc2_y_ed_luma= prm_mif->slc_y_ed[2];
	//   u32 slc3_x_st_luma= prm_mif->slc_x_st[3];
	//   u32 slc3_x_ed_luma= prm_mif->slc_x_ed[3];
	//   u32 slc3_y_st_luma= prm_mif->slc_y_st[3];
	//   u32 slc3_y_ed_luma= prm_mif->slc_y_ed[3];

	u32 slc0_x_st_chrm = prm_mif->src_fmt != YUV444 ?
	    slc0_x_st_luma >> 1 : slc0_x_st_luma;
	u32 slc0_x_ed_chrm = prm_mif->src_fmt != YUV444 ?
	    slc0_x_ed_luma >> 1 : slc0_x_ed_luma;
	u32 slc0_y_st_chrm = prm_mif->src_fmt == YUV420 ?
	    slc0_y_st_luma >> 1 : slc0_y_st_luma;
	u32 slc0_y_ed_chrm = prm_mif->src_fmt == YUV420 ?
	    slc0_y_ed_luma >> 1 : slc0_y_ed_luma;
	//   u32 slc1_x_st_chrm= prm_mif->src_fmt!=YUV444 ?
	//slc1_x_st_luma>>1 : slc1_x_st_luma;
	//   u32 slc1_x_ed_chrm= prm_mif->src_fmt!=YUV444 ?
	//slc1_x_ed_luma>>1 : slc1_x_ed_luma;
	//   u32 slc1_y_st_chrm= prm_mif->src_fmt==YUV420 ?
	//slc1_y_st_luma>>1 : slc1_y_st_luma;
	//   u32 slc1_y_ed_chrm= prm_mif->src_fmt==YUV420 ?
	//slc1_y_ed_luma>>1 : slc1_y_ed_luma;
	//   u32 slc2_x_st_chrm= prm_mif->src_fmt!=YUV444 ?
	//slc2_x_st_luma>>1 : slc2_x_st_luma;
	//   u32 slc2_x_ed_chrm= prm_mif->src_fmt!=YUV444 ?
	//slc2_x_ed_luma>>1 : slc2_x_ed_luma;
	//   u32 slc2_y_st_chrm= prm_mif->src_fmt==YUV420 ?
	//slc2_y_st_luma>>1 : slc2_y_st_luma;
	//   u32 slc2_y_ed_chrm= prm_mif->src_fmt==YUV420 ?
	//slc2_y_ed_luma>>1 : slc2_y_ed_luma;
	//   u32 slc3_x_st_chrm= prm_mif->src_fmt!=YUV444 ?
	//slc3_x_st_luma>>1 : slc3_x_st_luma;
	//   u32 slc3_x_ed_chrm= prm_mif->src_fmt!=YUV444 ?
	//slc3_x_ed_luma>>1 : slc3_x_ed_luma;
	//   u32 slc3_y_st_chrm= prm_mif->src_fmt==YUV420 ?
	//slc3_y_st_luma>>1 : slc3_y_st_luma;
	//   u32 slc3_y_ed_chrm= prm_mif->src_fmt==YUV420 ?
	//slc3_y_ed_luma>>1 : slc3_y_ed_luma;
	u32 slc_hsize = slc0_x_ed_luma - slc0_x_st_luma + 1;
	u32 slc_vsize = slc0_y_ed_luma - slc0_y_st_luma + 1;

	dbg_h2("rdmif slc0_x_ed=%4d rdmif slc0_y_ed=%4d\n", slc0_x_ed_luma,
			slc0_y_ed_luma);
	u32 fmt = prm_mif->src_fmt == YUV444 ? 0 : prm_mif->src_fmt == YUV422 ?
	    1 : 2;
	u32 plan = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;
	u32 bit =
	    prm_mif->src_bit == BIT_008 ? 0 : prm_mif->src_bit ==
	    BIT_010 ? 1 : prm_mif->src_bit == BIT_P010 ? 3 : 2;
	u32 y_pack_mode;	//0:1x 1:2x  2:4x  3:8x  4:16x  5:32x 6:64x
	u32 y_bits_mode;	//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	u32 cb_pack_mode;	//0:1x 1:2x  2:4x  3:8x  4:16x  5:32x 6:64x
	u32 cb_bits_mode;	//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	bool hfmt_en = prm_mif->src_fmt != YUV444;
	bool vfmt_en = prm_mif->src_fmt == YUV420;
	u32 mif1_en = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;

	switch ((plan << 8) | (fmt << 4) | bit) {
	case 0x000:
		y_bits_mode = 9;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_8b
	case 0x001:
		y_bits_mode = 3;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_10b
	case 0x003:
		y_bits_mode = 13;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//444_1p_16b
	case 0x010:
		y_bits_mode = 2;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_8b
	case 0x011:
		y_bits_mode = 10;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_10b
	case 0x012:
		y_bits_mode = 9;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_12b
	case 0x013:
		y_bits_mode = 3;
		y_pack_mode = 2;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;		//422_1p_16b
	case 0x110:
		y_bits_mode = 1;
		y_pack_mode = 1;
		cb_bits_mode = 2;
		cb_pack_mode = 1;
		break;		//422_2p_8b
	case 0x111:
		y_bits_mode = 10;
		y_pack_mode = 0;
		cb_bits_mode = 10;
		cb_pack_mode = 0;
		break;		//422_2p_10b
	case 0x112:
		y_bits_mode = 9;
		y_pack_mode = 0;
		cb_bits_mode = 9;
		cb_pack_mode = 1;
		break;		//422_2p_12b
	case 0x113:
		y_bits_mode = 2;
		y_pack_mode = 1;
		cb_bits_mode = 3;
		cb_pack_mode = 1;
		break;		//422_2p_16b
	case 0x120:
		y_bits_mode = 1;
		y_pack_mode = 1;
		cb_bits_mode = 2;
		cb_pack_mode = 1;
		break;		//420_2p_8b
	case 0x121:
		y_bits_mode = 10;
		y_pack_mode = 0;
		cb_bits_mode = 10;
		cb_pack_mode = 0;
		break;		//420_2p_10b
	case 0x122:
		y_bits_mode = 9;
		y_pack_mode = 0;
		cb_bits_mode = 9;
		cb_pack_mode = 1;
		break;		//420_2p_12b
	case 0x123:
		y_bits_mode = 2;
		y_pack_mode = 1;
		cb_bits_mode = 3;
		cb_pack_mode = 1;
		break;		//420_2p_16b
	default:
		y_bits_mode = 9;
		y_pack_mode = 1;
		cb_bits_mode = 0;
		cb_pack_mode = 0;
		break;
	}

	dbg_h2("%s:mif_index:%d(0:inp;1:dae;dct),y_baddr:0x%x\n", __func__,
	       mif_index, y_baddr);
	dbg_h2("%s:addr: INP_MIF0_RMIF_CTRL4 + regs_ofset=y_baddr\n", __func__);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, plan, 4, 2);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, fmt, 6, 2);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, bit, 8, 2);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, rev_x, 0, 1);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, rev_y, 1, 1);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, prm_mif->little_endian, 2, 1);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, prm_mif->swap_64bit, 3, 1);
	w_reg_bit(regs_ofst + INP_RMIF_TOP_CTRL, prm_mif->uv_swap, 11, 1);
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL1, 1, 19, 1);
	//ch0 reg_mif_en
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL3, stride_y, 0, 13);
	//ch0 reg_stride
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL5, y_pack_mode, 0, 3);
	//ch0 out_pack_mode
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL5, y_bits_mode, 3, 4);
	//ch0 out_bits_mode
	wr(regs_ofst + INP_MIF0_RMIF_CTRL4, y_baddr >> 4);
	//ch0 baddr //ary, for dae: DAE_MIF0_RMIF_CTRL4
	wr(regs_ofst + INP_LUMA_RMIF_SCOPE_X,
	   slc0_x_ed_luma << 16 | slc0_x_st_luma);
	//ch0 slc0 scope_x
	wr(regs_ofst + INP_LUMA_RMIF_SCOPE_Y,
	   slc0_y_ed_luma << 16 | slc0_y_st_luma);
	prm_mif->back_1_y = slc0_y_ed_luma << 16 | slc0_y_st_luma;
	//ch0 slc0 scope_y
	//    wr(DAE_LUMA_SLC1_XSIZE,slc1_x_ed_luma<<16|slc1_x_st_luma);
	//ch0 slc1 scope_x
	//    wr(DAE_LUMA_SLC1_YSIZE,slc1_y_ed_luma<<16|slc1_y_st_luma);
	//ch0 slc1 scope_y
	//    wr(DAE_LUMA_SLC2_XSIZE,slc2_x_ed_luma<<16|slc2_x_st_luma);
	//ch0 slc2 scope_x
	//    wr(DAE_LUMA_SLC2_YSIZE,slc2_y_ed_luma<<16|slc2_y_st_luma);
	//ch0 slc2 scope_y
	//    wr(DAE_LUMA_SLC3_XSIZE,slc3_x_ed_luma<<16|slc3_x_st_luma);
	//ch0 slc2 scope_x
	//    wr(DAE_LUMA_SLC3_YSIZE,slc3_y_ed_luma<<16|slc3_y_st_luma);
	//ch0 slc2 scope_y

	w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL1, mif1_en, 19, 1);
	//ch1 reg_mif_en
	w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL3, stride_c, 0, 13);
	//ch1 reg_stride
	w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL5, cb_pack_mode, 0, 3);
	//ch1 out_pack_mode
	w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL5, cb_bits_mode, 3, 4);
	//ch1 out_bits_mode
	wr(regs_ofst + INP_MIF1_RMIF_CTRL4, u_baddr >> 4);
	//ch1 baddr
	wr(regs_ofst + INP_CHRM_RMIF_SCOPE_X,
	   slc0_x_ed_chrm << 16 | slc0_x_st_chrm);
	//ch1 slc0 scope_x
	wr(regs_ofst + INP_CHRM_RMIF_SCOPE_Y,
	   slc0_y_ed_chrm << 16 | slc0_y_st_chrm);
	prm_mif->back_1_u = slc0_y_ed_chrm << 16 | slc0_y_st_chrm; //ary
	//ch1 slc0 scope_y
	//   wr(DAE_CHRM_SLC1_XSIZE,slc1_x_ed_chrm<<16|slc1_x_st_chrm);
	//ch1 slc1 scope_x
	//   wr(DAE_CHRM_SLC1_YSIZE,slc1_y_ed_chrm<<16|slc1_y_st_chrm);
	//ch1 slc1 scope_y
	//   wr(DAE_CHRM_SLC2_XSIZE,slc2_x_ed_chrm<<16|slc2_x_st_chrm);
	//ch1 slc2 scope_x
	//   wr(DAE_CHRM_SLC2_YSIZE,slc2_y_ed_chrm<<16|slc2_y_st_chrm);
	//ch1 slc2 scope_y
	//   wr(DAE_CHRM_SLC3_XSIZE,slc3_x_ed_chrm<<16|slc3_x_st_chrm);
	//ch1 slc2 scope_x
	//   wr(DAE_CHRM_SLC3_YSIZE,slc3_y_ed_chrm<<16|slc3_y_st_chrm);
	//ch1 slc2 scope_y

	u32 skip_en = prm_mif->skip_line > 0 ? 1 : 0;
	u32 skip_line_luma, skip_line_luma_t;

	skip_line_luma_t = prm_mif->skip_line;
	skip_line_luma = skip_line_luma_t == 0 ? 0 : skip_line_luma_t - 1;
	u32 skip_line_chrm = 0;

	skip_line_chrm = fmt == 2 ? skip_line_luma >> 1 : skip_line_luma;

	dbg_h2("skip_line =%4d\n", skip_line_luma);

	w_reg_bit(regs_ofst + INP_RMIF_F0_LUMA_RPT_PAT, skip_en, 3, 1);
	w_reg_bit(regs_ofst + INP_RMIF_F0_LUMA_RPT_PAT, skip_line_luma, 0, 3);
	w_reg_bit(regs_ofst + INP_RMIF_F0_CHRM_RPT_PAT, skip_en, 3, 1);
	w_reg_bit(regs_ofst + INP_RMIF_F0_CHRM_RPT_PAT, skip_line_chrm, 0, 3);

	////yu.zong 2024-12-06 begin:
	u32 field_en = prm_mif->field_mode > 0 ? 1 : 0;
	u32 field_mode = (prm_mif->field_mode == 2) ? 1 : 0;

	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL1, field_en, 30, 1);
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL2, field_en, 25, 1);
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL2, field_en, 23, 1);
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL2, field_mode, 24, 1);
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL2, field_mode, 22, 1);
	//yu.zong 2024-12-06 end

	//add for interlace: field mode:
	w_reg_bit(regs_ofst + INP_RMIF_F0_LUMA_RPT_PAT,
		field_en | skip_en, 3, 1);
	w_reg_bit(regs_ofst + INP_RMIF_F0_CHRM_RPT_PAT,
		field_en | skip_en, 3, 1);

	u32 vert_skip_uv = 0;
	u32 vert_skip_y = 0;
	u32 horz_skip_uv = 0;
	u32 horz_skip_y = 0;
	u32 uv_vsize_in = 0, vt_yc_ratio = 0, hz_yc_ratio = 0;

	if (fmt == 2) {		//420
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (slc_vsize >> 2);
				vt_yc_ratio = vert_skip_y == 0 ? 2 : 1;
			} else {
				uv_vsize_in = (slc_vsize >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = slc_vsize;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt == 1) {	//422
		if (vfmt_en) {
			if (vert_skip_uv != 0) {
				uv_vsize_in = (slc_vsize >> 1);
				vt_yc_ratio = vert_skip_y == 0 ? 1 : 0;
			} else {
				uv_vsize_in = (slc_vsize >> 1);
				vt_yc_ratio = 1;
			}
		} else {
			uv_vsize_in = slc_vsize;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {
			if (horz_skip_uv != 0)
				hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
			else
				hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	} else if (fmt == 0) {	//444
		if (vfmt_en) {	//vert_skip_y==0,vert_skip_uv!=0
			uv_vsize_in = (slc_vsize >> 1);
			vt_yc_ratio = 1;
		} else {
			uv_vsize_in = slc_vsize;
			vt_yc_ratio = 0;
		}

		if (hfmt_en) {	//horz_skip_y==0,horz_skip_uv!=0
			hz_yc_ratio = 1;
		} else {
			hz_yc_ratio = 0;
		}
	}

	u32 vt_phase_step = (16 >> vt_yc_ratio);
	u32 vfmt_w = (slc_hsize >> hz_yc_ratio);
	//    u32 skip_en = 1 ; //TODO
	u32 reg_win_vsel = (prm_mif->skip_line > 0) ? 1 : 0;
	u32 uv_vsize_in_t = (prm_mif->skip_line > 0) ?
	    uv_vsize_in / (prm_mif->skip_line + 1) : uv_vsize_in;
	u32 luma_vsize = (prm_mif->skip_line > 0) ?
	    slc_vsize / (prm_mif->skip_line + 1) : slc_vsize;
	u32 luma_win_yend = luma_vsize - 1;
	u32 chrm_vsize = uv_vsize_in_t;
	u32 chrm_win_yend = uv_vsize_in_t - 1;

	w_reg_bit(regs_ofst + INP_VD_CFMT_H, reg_win_vsel, 29, 1);
	w_reg_bit(regs_ofst + INP_WIN_LUMA_SIZE, luma_vsize, 0, 13);
	w_reg_bit(regs_ofst + INP_WIN_LUMA_Y, luma_win_yend, 0, 13);
	w_reg_bit(regs_ofst + INP_WIN_CHRM_SIZE, chrm_vsize, 0, 13);
	w_reg_bit(regs_ofst + INP_WIN_CHRM_Y, chrm_win_yend, 0, 13);

	wr((regs_ofst + INP_VD_CFMT_CTRL), (0 << 28) |	//hz rpt pixel
	   (0 << 24) |		//hz ini phase
	   (0 << 23) |		//repeat p0 enable
	   (hz_yc_ratio << 21) |	//hz yc ratio
	   (hfmt_en << 20) |	//hz enable
	   (0 << 19) |		//cvfmt_always_en
	   (0 << 17) |		//nrpt_phase0 enable
	   (0 << 16) |		//repeat l0 enable
	   (0 << 12) |		//skip line num
	   (0 << 8) |		//vt ini phase
	   (vt_phase_step << 1) |	//vt phase step (3.4)
	   (vfmt_en << 0)	//vt enable
	    );

	wr((regs_ofst + INP_VD_CFMT_W), (slc_hsize << 16) |	//hz format width
	   (vfmt_w << 0)	//vt format width
	    );

	wr((regs_ofst + INP_VD_CFMT_H), uv_vsize_in_t);

	//cfg mmu mode:
	if (prm_mif->mmu_mode_en)
		w_reg_bit(regs_ofst + INP_RMIF_MMU_CTRL, 1, 0, 1);
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL1, 3, 0, 3);	//burst8
	w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL1, 3, 0, 3);	//burst8
	w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL1, prm_mif->reg_mode, 27, 2);

	if (prm_mif->block_mode) {
		w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL1, 1 << 2 |
			prm_mif->block_mode, 20, 3);
		w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL1, 1 << 2 |
			prm_mif->block_mode, 20, 3);
		w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL1, 0, 0, 3);
		w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL1, 0, 0, 3);
	} else {
		w_reg_bit(regs_ofst + INP_MIF0_RMIF_CTRL1, 0, 20, 3);
		w_reg_bit(regs_ofst + INP_MIF1_RMIF_CTRL1, 0, 20, 3);
	}

	//mif_reg_mode,0-win_size,1-baddr //yu.zong 2024-12-06
};

void cfg_mc_sub_rdmif(struct PRM_INTF_TYPE *prm_mif, s32 mif_index,
		      s32 only_change_addr)
{
	s32 regs_ofst_sub0 = (0xe000 - 0xc100);	//ary addr change *4
	s32 regs_ofst_sub1 = (0xe020 - 0xc100);	//ary addr change *4
	s32 regs_ofst_sub2 = (0xe040 - 0xc100);	//ary addr change *4
	s32 regs_ofst_sub3 = (0xe060 - 0xc100);	//ary addr change *4
	s32 regs_ofst;

	switch (mif_index) {
	case 0:
		regs_ofst = regs_ofst_sub0;
		break;
	case 1:
		regs_ofst = regs_ofst_sub1;
		break;
	case 2:
		regs_ofst = regs_ofst_sub2;
		break;
	case 3:
		regs_ofst = regs_ofst_sub3;
		break;
	default:
		regs_ofst = regs_ofst_sub0;
		break;
	}

	s32 stride = prm_mif->stride;
	s32 baddr = prm_mif->src_baddr[0];
	s32 x_start = prm_mif->slc_x_st[0];
	s32 x_end = prm_mif->slc_x_ed[0];
	s32 y_start = prm_mif->slc_y_st[0];
	s32 y_end = prm_mif->slc_y_ed[0];
	s32 bits_mode = prm_mif->bits_mode;
	s32 pack_mode = prm_mif->pack_mode;

	if (only_change_addr == 1) {
		wr(regs_ofst + VPU_VPSS_RMIF_CTRL4, baddr);
	} else {
		w_reg_bit(regs_ofst + VPU_VPSS_RMIF_CTRL3, stride, 0, 13);
		wr(regs_ofst + VPU_VPSS_RMIF_CTRL4, baddr);
		wr(regs_ofst + VPU_VPSS_RMIF_SCOPE_X, (x_end << 16 | x_start));
		wr(regs_ofst + VPU_VPSS_RMIF_SCOPE_Y, (y_end << 16 | y_start));
		w_reg_bit(regs_ofst + VPU_VPSS_RMIF_CTRL5,
			  (bits_mode << 3 | pack_mode), 0, 7);
	}
	w_reg_bit(regs_ofst + VPU_VPSS_RMIF_CTRL1, prm_mif->burst_len, 8, 2);
	//burst_len, 2: burst4

	dbg_h2("cfg_mc_sub_rd_mif end\n");
}

void hw_cfg_mc_sub_rdmif(struct PRM_INTF_TYPE *prm_mif, s32 mif_index,
		      s32 only_change_addr)
{
	s32 regs_ofst_sub0 = (0xe000 - 0xc100);	//ary addr change *4
	s32 regs_ofst_sub1 = (0xe020 - 0xc100);	//ary addr change *4
	s32 regs_ofst_sub2 = (0xe040 - 0xc100);	//ary addr change *4
	s32 regs_ofst_sub3 = (0xe060 - 0xc100);	//ary addr change *4
	s32 regs_ofst;

	switch (mif_index) {
	case 0:
		regs_ofst = regs_ofst_sub0;
		break;
	case 1:
		regs_ofst = regs_ofst_sub1;
		break;
	case 2:
		regs_ofst = regs_ofst_sub2;
		break;
	case 3:
		regs_ofst = regs_ofst_sub3;
		break;
	default:
		regs_ofst = regs_ofst_sub0;
		break;
	}

	s32 stride = prm_mif->stride;
	s32 baddr = prm_mif->src_baddr[0];
	s32 x_start = prm_mif->slc_x_st[0];
	s32 x_end = prm_mif->slc_x_ed[0];
	s32 y_start = prm_mif->slc_y_st[0];
	s32 y_end = prm_mif->slc_y_ed[0];
	s32 bits_mode = prm_mif->bits_mode;
	s32 pack_mode = prm_mif->pack_mode;
	u32 tmp_reg_value;

	if (only_change_addr == 1) {
		DPSS_RDMA_WR_VS(regs_ofst + VPU_VPSS_RMIF_CTRL4, baddr);
	} else {
		tmp_reg_value = rd(regs_ofst + VPU_VPSS_RMIF_CTRL3);
		tmp_reg_value = update_reg_val(tmp_reg_value, stride, 0, 13);
		DPSS_RDMA_WR_VS(regs_ofst + VPU_VPSS_RMIF_CTRL3, tmp_reg_value);
		DPSS_RDMA_WR_VS(regs_ofst + VPU_VPSS_RMIF_CTRL4, baddr);
		DPSS_RDMA_WR_VS(regs_ofst + VPU_VPSS_RMIF_SCOPE_X, (x_end << 16 | x_start));
		DPSS_RDMA_WR_VS(regs_ofst + VPU_VPSS_RMIF_SCOPE_Y, (y_end << 16 | y_start));
		tmp_reg_value = rd(regs_ofst + VPU_VPSS_RMIF_CTRL5);
		tmp_reg_value = update_reg_val(tmp_reg_value, bits_mode << 3 | pack_mode, 0, 7);
		DPSS_RDMA_WR_VS(regs_ofst + VPU_VPSS_RMIF_CTRL5, tmp_reg_value);
	}
}

void cfg_lcevc_top(u32 lcevc_en, u32 src1_frm_hsize,	//normal_afbcd_hsize, input
		u32 src1_frm_vsize, u32 src1_head_ybaddr,
		u32 src1_body_ybaddr, u32 src1_head_cbaddr,
		u32 src1_body_cbaddr, u32 src1_is_cmpr, u32 src1_bit,
		u32 src2_frm_hsize,	//luma_only_hsize, input
		u32 src2_frm_vsize, u32 src2_ybaddr, u32 src2_cbaddr,
		u32 frm_hsize_out, u32 frm_vsize_out, u32 inst_sel,
		//reg cfg---0:aa_pps 1:lcevc
		u32 apply_mode,
		//0:zoom up 2path out 1:zoom down 2path out 2:zoom up/down 1path out
		u32 ds_mode,	//0:aa mode 1:pps mode
		u32 slc_mode,	//0:frame 1:slice
		u32 slc_num,	//2/4 slice
		u32 bld_din0_premult_en,
		u32 bld_din1_premult_en,
		u32 bld_din0_alpha, u32 bld_din1_alpha, u32 bld_src0_sel,
		//1:lay1 chose src0  2:lay1 chose src1  else :close blend lay1
		u32 bld_src1_sel,
		u32 dbg_cfg)
		//1:lay2 chose src0  2:lay2 chose src1  else :close blend lay2
{
	int i;
	u32 pps_slc_out_size;
	//aru np use u32 reg_hds_sel;
	//ary no use u32 reg_vds_sel;
	//ary no use u32 reg_hvds_sel;
	//ary no use u32 skip_dsx;
	//ary no use u32 skip_dsy;
	//ary set and no use u32 luma_only_hsize;
	//ary set and no use u32 bld_en;

	u32 vfcd0_slc_in_size;
	u32 vfcd0_slc_in_bgn[4];	//13 bits slice in bgn
	u32 vfcd0_slc_in_end[4];	//13 bits slice in end
	u32 rdmif_slc_in_size;
	u32 rdmif_slc_in_bgn[4];	//13 bits slice in bgn
	u32 rdmif_slc_in_end[4];	//13 bits slice in end
	u32 fmt444_out = 1;

	pps_slc_out_size = frm_hsize_out / slc_num;
	pps_slc_out_size = (pps_slc_out_size & 0x1) + pps_slc_out_size;

	vfcd0_slc_in_size = src1_frm_hsize / slc_num;
	vfcd0_slc_in_size = (vfcd0_slc_in_size & 0x1) + vfcd0_slc_in_size;

	rdmif_slc_in_size = src2_frm_hsize / slc_num;
	rdmif_slc_in_size = (rdmif_slc_in_size & 0x1) + rdmif_slc_in_size;

	//ary set and no use    luma_only_hsize = src2_frm_hsize;
	//ary set and no use    bld_en          = lcevc_en      ;

	if (lcevc_en) {
		w_reg_bit(DPSS_DPE_MIF_CTRL1, 1, 16, 1);  //jintao copy from rtl sim test.c
		w_reg_bit(DPSS_DPE_PATH_CTRL, 5, 16, 4);  //jintao copy from rtl sim test.c
		w_reg_bit_vc(VPU_AXIRD_SECURE_EN, 1, 8, 1);
		//axird_intf_top  lcevc_enable
		//config lcevc_pps
		struct AA_PPS_TOP_TYPE lcevc_pps_cfg;

		lcevc_pps_cfg.frm_hsize_in = src1_frm_hsize;
		lcevc_pps_cfg.frm_vsize_in = src1_frm_vsize;
		lcevc_pps_cfg.frm_hsize_out = frm_hsize_out;
		lcevc_pps_cfg.frm_vsize_out = frm_vsize_out;

		lcevc_pps_cfg.inst_sel = 1;
		lcevc_pps_cfg.apply_mode = apply_mode;
		// 0:zoom up 2path out 1:zoom down 2path out 2:zoom up
		//down 1path out
		lcevc_pps_cfg.slc_mode = 1;	// 0:frame mode 1:slice mode;
		lcevc_pps_cfg.ds_mode = 1;	// 0:aa mode; 1:pps mode;
		lcevc_pps_cfg.slc_num = slc_num;

		lcevc_pps_cfg.slc_act_out_bgn[0] = 0 * pps_slc_out_size;
		lcevc_pps_cfg.slc_act_out_bgn[1] = 1 * pps_slc_out_size;
		lcevc_pps_cfg.slc_act_out_bgn[2] = 2 * pps_slc_out_size;
		lcevc_pps_cfg.slc_act_out_bgn[3] = 3 * pps_slc_out_size;

		lcevc_pps_cfg.slc_act_out_end[0] = 1 * pps_slc_out_size;
		lcevc_pps_cfg.slc_act_out_end[1] = 2 * pps_slc_out_size;
		lcevc_pps_cfg.slc_act_out_end[2] = 3 * pps_slc_out_size;
		lcevc_pps_cfg.slc_act_out_end[3] = frm_hsize_out;

		//reg_hds_sel = 3;    nouse
		//reg_vds_sel = 1;    nouse
		//reg_hvds_sel = 1;   nouse
		//skip_dsx = 1;       nouse
		//skip_dsy = 1;       nouse
		aa_pps_top(&lcevc_pps_cfg);

		//config lcevc_bld vd1 & vd2 scope
		w_reg_bit_vc(LCEVC_TOP_MISC_CTRL, (bld_src0_sel & 0x3) << 0 |
		(bld_src1_sel & 0x3) << 2 | (slc_num & 0x7) << 4 |
		(bld_din0_premult_en & 0x1) << 8 |
		(bld_din1_premult_en & 0x1) << 9 |
		(lcevc_en & 0x1) << 10, 0, 11);	//lcevc_bld_en

		//wr(LCEVC_TOP_BLD_SIZE, (frm_hsize_out & 0x1fff ) << 0 |
		//                       (frm_vsize_out & 0x1fff ) << 16);

		if (dbg_cfg & C_BIT3)
			bld_din0_alpha = 0;
		if (dbg_cfg & C_BIT4)
			bld_din1_alpha = 0;

		wr_vc(LCEVC_TOP_BLD_ALPHA, (bld_din0_alpha & 0x1ff) << 0 |
		      (bld_din1_alpha & 0x1ff) << 12);

		//wr(LCEVC_TOP_VD1_HSCOPE_0, (bld_din0_hend_0   & 0x1fff ) << 0 |
		//                           (bld_din0_hstart_0 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD1_HSCOPE_1, (bld_din0_hend_1   & 0x1fff ) << 0 |
		//                           (bld_din0_hstart_1 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD1_HSCOPE_2, (bld_din0_hend_2   & 0x1fff ) << 0 |
		//                           (bld_din0_hstart_2 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD1_HSCOPE_3, (bld_din0_hend_3   & 0x1fff ) << 0 |
		//                           (bld_din0_hstart_3 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD1_VSCOPE_0, (bld_din0_vend_0   & 0x1fff ) << 0 |
		//                           (bld_din0_vstart_0 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD2_HSCOPE_0, (bld_din1_hend_0   & 0x1fff ) << 0 |
		//                           (bld_din1_hstart_0 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD2_HSCOPE_1, (bld_din1_hend_1   & 0x1fff ) << 0 |
		//                           (bld_din1_hstart_1 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD2_HSCOPE_2, (bld_din1_hend_2   & 0x1fff ) << 0 |
		//                           (bld_din1_hstart_2 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD2_HSCOPE_3, (bld_din1_hend_3   & 0x1fff ) << 0 |
		//                           (bld_din1_hstart_3 & 0x1fff ) << 16);

		//wr(LCEVC_TOP_VD2_VSCOPE_0, (bld_din1_vend_0   & 0x1fff ) << 0 |
		//                           (bld_din1_vstart_0 & 0x1fff ) << 16);

		//config vd1 normal_afbcd path, dep0_afbcd
		vfcd0_slc_in_bgn[0] = lcevc_pps_cfg.slc_in_bgn[0];
		vfcd0_slc_in_bgn[1] = lcevc_pps_cfg.slc_in_bgn[1];
		vfcd0_slc_in_bgn[2] = lcevc_pps_cfg.slc_in_bgn[2];
		vfcd0_slc_in_bgn[3] = lcevc_pps_cfg.slc_in_bgn[3];

		vfcd0_slc_in_end[0] = lcevc_pps_cfg.slc_in_end[0] - 1;
		vfcd0_slc_in_end[1] = lcevc_pps_cfg.slc_in_end[1] - 1;
		vfcd0_slc_in_end[2] = lcevc_pps_cfg.slc_in_end[2] - 1;
		vfcd0_slc_in_end[3] = lcevc_pps_cfg.slc_in_end[3] - 1;

		if (src1_is_cmpr) {
			struct VFCD_t vfcd0;

			init_dpss_vfcd(0, &vfcd0);
			//DPE_NR_BYPS mode
			vfcd0.luma_head_baddr = src1_head_ybaddr;
			vfcd0.luma_body_baddr = src1_body_ybaddr;
			vfcd0.chrm_head_baddr = src1_head_cbaddr;
			vfcd0.chrm_body_baddr = src1_body_cbaddr;

			vfcd0.fmt444_out = 1;
			vfcd0.slc_num = slc_num;
			vfcd0.film_grain_en = 0;
			vfcd0.pad_en = 0;
			vfcd0.pad_hmode = 0;
			vfcd0.pad_vmode = 0;
			vfcd0.mmu_page_mode = 0;

			vfcd0.src_hsize = src1_frm_hsize;
			vfcd0.src_vsize = src1_frm_vsize;
			for (i = 0; i < 4; i++) {
				vfcd0.win_bgn_h[i] = vfcd0_slc_in_bgn[i];
				vfcd0.win_end_h[i] = vfcd0_slc_in_end[i];
				vfcd0.win_bgn_v[i] = 0;
				vfcd0.win_end_v[i] = src1_frm_vsize - 1;
			}
			vfcd0.mmu_mode_en = 0;
			if (dbg_cfg & C_BIT0)
				vfcd0.mmu_mode_en            = 1;//0; //ary
			vfcd0.afbcd.fmt444_comb = 0;
			vfcd0.y_h_skip_en = 0;
			vfcd0.y_v_skip_en = 0;
			vfcd0.c_h_skip_en = 0;
			vfcd0.c_v_skip_en = 0;
			vfcd0.rev_mode = 0;
			vfcd0.cmpr_sel = 1;	//sel afbcd

			vfcd0.hfmt_en = 1;
			vfcd0.vfmt_en = 1;
			vfcd0.src_fmt = 2;	//force fmt_420
			vfcd0.compbits_y = 10;
			vfcd0.compbits_c = 10;
			if (src1_bit & BITDEPTH_Y10) {
				vfcd0.compbits_y = 10;
				vfcd0.compbits_c = 10;
			} else if (src1_bit & BITDEPTH_Y12) {
				vfcd0.compbits_y = 12;
				vfcd0.compbits_c = 12;
			} else {
				vfcd0.compbits_y = 8;
				vfcd0.compbits_c = 8;
			}

			vfcd0.afbcd.dos_uncomp = 1;

			dbg_h2("vfcd0.compbits_y=%d mmu_mode_en=%d\n",
				vfcd0.compbits_y, vfcd0.mmu_mode_en);
			cfg_vfcd_dec(0, &vfcd0);	//nr din cur

		} else {	//MIF SRC

			struct PRM_INTF_TYPE pix_rmif0;

			memset((void *)(&pix_rmif0), 0, sizeof(struct PRM_INTF_TYPE));

			pix_rmif0.src_fmt = YUV422;	//YUV444/YUV422/YUV420/RGB
			pix_rmif0.src_plan = PLANAR_X2;
			pix_rmif0.src_bit = BIT_010;
			if (src1_bit & BITDEPTH_Y10)
				pix_rmif0.src_bit = BIT_010;
			else if (src1_bit & BITDEPTH_Y12)
				pix_rmif0.src_bit = BIT_012;
			else
				pix_rmif0.src_bit = BIT_008;
			pix_rmif0.src_cmpr = CMPR_UN;	//un/afbc/afrc
			pix_rmif0.interlace = IS_PSRC;	//IS_PSRC/IS_ISRC

			pix_rmif0.src_hsize = src1_frm_hsize;
			pix_rmif0.src_vsize = src1_frm_vsize;
			pix_rmif0.reverse[0] = 0;	//[0]:x      [1]:y
			pix_rmif0.reverse[1] = 0;	//[0]:x      [1]:y

			pix_rmif0.src_baddr[0] = src1_body_ybaddr << 4;	//luma
			pix_rmif0.src_baddr[1] = src1_body_cbaddr << 4;	//chrm

			pix_rmif0.slc_x_st[0] = lcevc_pps_cfg.slc_in_bgn[0];
			pix_rmif0.slc_x_st[1] = lcevc_pps_cfg.slc_in_bgn[1];
			pix_rmif0.slc_x_st[2] = lcevc_pps_cfg.slc_in_bgn[2];
			pix_rmif0.slc_x_st[3] = lcevc_pps_cfg.slc_in_bgn[3];
			pix_rmif0.slc_y_st[0] = 0;
			pix_rmif0.slc_y_st[1] = 0;
			pix_rmif0.slc_y_st[2] = 0;
			pix_rmif0.slc_y_st[3] = 0;

			pix_rmif0.slc_x_ed[0] = lcevc_pps_cfg.slc_in_end[0] - 1;
			pix_rmif0.slc_x_ed[1] = lcevc_pps_cfg.slc_in_end[1] - 1;
			pix_rmif0.slc_x_ed[2] = lcevc_pps_cfg.slc_in_end[2] - 1;
			pix_rmif0.slc_x_ed[3] = lcevc_pps_cfg.slc_in_end[3] - 1;
			pix_rmif0.slc_y_ed[0] = src1_frm_vsize - 1;
			pix_rmif0.slc_y_ed[1] = src1_frm_vsize - 1;
			pix_rmif0.slc_y_ed[2] = src1_frm_vsize - 1;
			pix_rmif0.slc_y_ed[3] = src1_frm_vsize - 1;

			dbg_h2("rdmif0 slice0 in bgn %d end %d\n",
			       pix_rmif0.slc_x_st[0], pix_rmif0.slc_x_ed[0]);
			dbg_h2("rdmif0 slice1 in bgn %d end %d\n",
			       pix_rmif0.slc_x_st[1], pix_rmif0.slc_x_ed[1]);
			dbg_h2("rdmif0 slice2 in bgn %d end %d\n",
			       pix_rmif0.slc_x_st[2], pix_rmif0.slc_x_ed[2]);
			dbg_h2("rdmif0 slice3 in bgn %d end %d\n",
			       pix_rmif0.slc_x_st[3], pix_rmif0.slc_x_ed[3]);
			cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE0, &pix_rmif0,
					   fmt444_out);
		}

		//nr cur din vfcd mode
		if (src1_is_cmpr)
			cfg_vfcd_cmpr_enable(0, 1);	//vfcd_chose afbc
		else
			cfg_vfcd_cmpr_enable(0, 0);	//vfcd_chose mif

		//config vd2 luma_only path, dep2_rdmif
		rdmif_slc_in_bgn[0] = 0 * rdmif_slc_in_size;
		rdmif_slc_in_bgn[1] = 1 * rdmif_slc_in_size;
		rdmif_slc_in_bgn[2] = 2 * rdmif_slc_in_size;
		rdmif_slc_in_bgn[3] = 3 * rdmif_slc_in_size;

		rdmif_slc_in_end[0] = 1 * rdmif_slc_in_size - 1;
		rdmif_slc_in_end[1] = 2 * rdmif_slc_in_size - 1;
		rdmif_slc_in_end[2] = 3 * rdmif_slc_in_size - 1;
		//rdmif_slc_in_end[3] = src1_frm_hsize-1;
		rdmif_slc_in_end[3] = src2_frm_hsize - 1;

		struct PRM_INTF_TYPE pix_rmif;

		memset((void *)(&pix_rmif), 0, sizeof(struct PRM_INTF_TYPE));

		pix_rmif.src_fmt = YUV444;
		if (dbg_cfg & C_BIT1)
			pix_rmif.src_fmt = YUV420;
		else if (dbg_cfg & C_BIT2)
			pix_rmif.src_fmt = YUV422;
		pix_rmif.src_plan = PLANAR_X1;	//PLANAR_X1/X2
		pix_rmif.src_bit = BIT_010;	//BIT_8/10
		pix_rmif.src_cmpr = CMPR_UN;	//un/afbc/afrc
		pix_rmif.interlace = IS_PSRC;	//IS_PSRC/IS_ISRC
		dbg_h2("src_fmt:0x%x\n", pix_rmif.src_fmt);
		pix_rmif.src_hsize = src2_frm_hsize;
		pix_rmif.src_vsize = src2_frm_vsize;
		pix_rmif.reverse[0] = 0;
		pix_rmif.reverse[1] = 0;

		pix_rmif.mmu_mode_en = 0;
		pix_rmif.pad_en = 0;
		pix_rmif.pad_hmode = 0;
		pix_rmif.pad_vmode = 0;

		//u32 dpe_src0_idx = rd(DPSS_FBUF_DPE_LOOP_IDX);
		//u32 dpe_src1_idx = rd(DPSS_REGOFST_VDICTRL+DPSS_FBUF_DPE_LOOP_IDX);
		//u32 dpe_src0_cur0_rd_idx = (dpe_src0_idx >> 24) & 0xf;
		//u32 dpe_src0_pre1_rd_idx = (dpe_src0_idx >> 20) & 0xf;
		//u32 dpe_src0_pre2_rd_idx = (dpe_src0_idx >> 16) & 0xf;
		//u32 dpe_src0_nr_wr_idx   = (dpe_src0_idx >> 4 ) & 0xf;
		//u32 dpe_src1_cur0_rd_idx = (dpe_src1_idx >> 24) & 0xf;
		//u32 dpe_src1_pre1_rd_idx = (dpe_src1_idx >> 20) & 0xf;
		//u32 dpe_src1_pre2_rd_idx = (dpe_src1_idx >> 16) & 0xf;
		//u32 dpe_src1_nr_wr_idx   = (dpe_src1_idx >> 4 ) & 0xf;
		//u32 dpe_src1_di_wr_idx   = (dpe_src1_idx >> 0 ) & 0xf;

		//DPE_NR_BYPS:
		pix_rmif.src_baddr[0] = src2_ybaddr << 4;
		pix_rmif.src_baddr[1] = src2_ybaddr << 4;

		for (i = 0; i < 4; i++) {
			pix_rmif.slc_x_st[i] = rdmif_slc_in_bgn[i];
			pix_rmif.slc_x_ed[i] = rdmif_slc_in_end[i];
			pix_rmif.slc_y_st[i] = 0;
			pix_rmif.slc_y_ed[i] = src2_frm_vsize - 1;
			dbg_h2("rdmif2 slice in bgn %d end %d\n",
			rdmif_slc_in_bgn[i], rdmif_slc_in_end[i]);
		}
		cfg_vfcd_rdmif_2ch(DPSS_RMIF_DPE2, &pix_rmif, fmt444_out);
		u32 stride_y;

		stride_y = (src2_frm_hsize * 10 + 511) / 512 * 4;	//burst

		w_reg_bit_vc(RMIF_TOP_CTRL + (0x67 - 0x65) * 256, 1, 31, 1);

		/*jintao, hard code,soft dec always 1 plane and little endian*/
		w_reg_bit_vc(RMIF_TOP_CTRL + (0x67 - 0x65) * 256, 0, 6, 2);//luma_only need set 0
		w_reg_bit_vc(RMIF_TOP_CTRL + (0x67 - 0x65) * 256, 0, 4, 2);//1 plane
		w_reg_bit_vc(RMIF_TOP_CTRL + (0x67 - 0x65) * 256, 0, 3, 1);//little endian
		w_reg_bit_vc(RMIF_TOP_CTRL + (0x67 - 0x65) * 256, 1, 2, 1);//little endian

		w_reg_bit_vc(MIF0_RMIF_CTRL3 + (0x67 - 0x65) * 256, stride_y, 0, 13);
		w_reg_bit_vc(MIF0_RMIF_CTRL5 + (0x67 - 0x65) * 256, 0, 0, 3);
		//ary x 4
		w_reg_bit_vc(MIF0_RMIF_CTRL5 + (0x67 - 0x65) * 256, 10, 3, 4);
		//ary x 4
		w_reg_bit_vc(MIF0_RMIF_CTRL1 + (0x67 - 0x65) * 256, 2, 27, 2);
		//vfcd2.common mif use pab baddr        //ary x 4
		w_reg_bit_vc(VFCD_AFBC_VD_CFMT_CTRL + (0x67 - 0x65) * 256, 1, 20, 1);
		//ary x 4
		w_reg_bit_vc(VFCD_AFBC_VD_CFMT_CTRL + (0x67 - 0x65) * 256, 1, 21, 2);
		//ary x 4
		w_reg_bit_vc(VFCD_TOP_CTRL0, 3, 27, 2);	//vfcd0 use apb addr

		//config pps top size
		u32 pps_slc_in_hsize[4];	//13 bits slice in bgn

		pps_slc_in_hsize[0] = lcevc_pps_cfg.slc_in_end[0] -
			lcevc_pps_cfg.slc_in_bgn[0];	// 1*vfcd0_slc_in_size;
		pps_slc_in_hsize[1] = lcevc_pps_cfg.slc_in_end[1] -
			lcevc_pps_cfg.slc_in_bgn[1];	// 2*vfcd0_slc_in_size;
		pps_slc_in_hsize[2] = lcevc_pps_cfg.slc_in_end[2] -
			lcevc_pps_cfg.slc_in_bgn[2];	// 3*vfcd0_slc_in_size;
		pps_slc_in_hsize[3] = lcevc_pps_cfg.slc_in_end[3] -
			lcevc_pps_cfg.slc_in_bgn[3];	// src1_frm_hsize;

		wr_vc(LCEVC_TOP_PPS_IN_HSIZE0,
			(pps_slc_in_hsize[1] & 0x1fff) << 0 | (pps_slc_in_hsize[0]
								& 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_IN_HSIZE1,
			(pps_slc_in_hsize[3] & 0x1fff) << 0 | (pps_slc_in_hsize[2]
								& 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_VSIZE0, (frm_vsize_out & 0x1fff) << 0 |
			(src1_frm_vsize & 0x1fff) << 16);

		//config pps_cut_win
		u32 slc_final_out_hsize;

		slc_final_out_hsize = frm_hsize_out / slc_num;
		slc_final_out_hsize =
		    (slc_final_out_hsize & 0x1) + slc_final_out_hsize;

		u32 pps_win_cut_slc_in_bgn[4];	//13 bits slice in bgn
		u32 pps_win_cut_slc_in_end[4];	//13 bits slice in end

		pps_win_cut_slc_in_bgn[0] = lcevc_pps_cfg.slc_act_out_bgn[0];
		pps_win_cut_slc_in_bgn[1] = lcevc_pps_cfg.slc_act_out_bgn[1];
		pps_win_cut_slc_in_bgn[2] = lcevc_pps_cfg.slc_act_out_bgn[2];
		pps_win_cut_slc_in_bgn[3] = lcevc_pps_cfg.slc_act_out_bgn[3];

		pps_win_cut_slc_in_end[0] =
		    lcevc_pps_cfg.slc_act_out_end[0] - 1;
		pps_win_cut_slc_in_end[1] =
		    lcevc_pps_cfg.slc_act_out_end[1] - 1;
		pps_win_cut_slc_in_end[2] =
		    lcevc_pps_cfg.slc_act_out_end[2] - 1;
		pps_win_cut_slc_in_end[3] =
		    lcevc_pps_cfg.slc_act_out_end[3] - 1;

		u32 pps_win_cut_slc_out_bgn[4];	//13 bits slice in bgn
		u32 pps_win_cut_slc_out_end[4];	//13 bits slice in end
		//
		pps_win_cut_slc_out_bgn[0] = lcevc_pps_cfg.slc_act_out_bgn[0];
		pps_win_cut_slc_out_bgn[1] = lcevc_pps_cfg.slc_act_out_bgn[1];
		pps_win_cut_slc_out_bgn[2] = lcevc_pps_cfg.slc_act_out_bgn[2];
		pps_win_cut_slc_out_bgn[3] = lcevc_pps_cfg.slc_act_out_bgn[3];

		pps_win_cut_slc_out_end[0] =
		    lcevc_pps_cfg.slc_act_out_end[0] - 1;
		pps_win_cut_slc_out_end[1] =
		    lcevc_pps_cfg.slc_act_out_end[1] - 1;
		pps_win_cut_slc_out_end[2] =
		    lcevc_pps_cfg.slc_act_out_end[2] - 1;
		pps_win_cut_slc_out_end[3] =
		    lcevc_pps_cfg.slc_act_out_end[3] - 1;

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_0,
		      (pps_win_cut_slc_in_end[0] & 0x1fff) << 0 |
		      (pps_win_cut_slc_in_bgn[0] & 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_1,
		      (pps_win_cut_slc_in_end[1] & 0x1fff) << 0 |
		      (pps_win_cut_slc_in_bgn[1] & 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_2,
		      (pps_win_cut_slc_in_end[2] & 0x1fff) << 0 |
		      (pps_win_cut_slc_in_bgn[2] & 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_3,
		      (pps_win_cut_slc_in_end[3] & 0x1fff) << 0 |
		      (pps_win_cut_slc_in_bgn[3] & 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_0,
		      (pps_win_cut_slc_out_end[0] & 0x1fff) << 0 |
		      (pps_win_cut_slc_out_bgn[0] & 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_1,
		      (pps_win_cut_slc_out_end[1] & 0x1fff) << 0 |
		      (pps_win_cut_slc_out_bgn[1] & 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_2,
		      (pps_win_cut_slc_out_end[2] & 0x1fff) << 0 |
		      (pps_win_cut_slc_out_bgn[2] & 0x1fff) << 16);

		wr_vc(LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_3,
		      (pps_win_cut_slc_out_end[3] & 0x1fff) << 0 |
		      (pps_win_cut_slc_out_bgn[3] & 0x1fff) << 16);
	}
}

void lcevc_update_addr(unsigned int y_addr, unsigned int u_addr) //have shift 4
{
	dbg_h2("%s:y=0x%x, u= 0x%x\n", __func__, y_addr, u_addr);
	cfg_vfcd_rdmif_2ch_update_addr(DPSS_RMIF_DPE2, y_addr, u_addr);
}
