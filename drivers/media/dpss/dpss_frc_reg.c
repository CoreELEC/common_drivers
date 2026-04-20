// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#include "register_t6w.h"
#include "sys_def.h" //ary add for sim

#ifdef RUN_ON_ARM
#include <linux/string.h>
#include <linux/ctype.h>
#endif

// #include "../video_sink/video_reg.h"

//#include "dpss_sys.h"

#define DPSS_MAX_LAYER  2

struct dpss_hw_vfcd_reg_s {
	/* mif */
	u32 rmif_top_ctrl;
	u32 mif0_rmif_ctrl1;
	u32 mif0_rmif_ctrl2;
	u32 mif0_rmif_ctrl3;
	u32 mif0_rmif_ctrl4;
	u32 mif0_rmif_ctrl5;
	u32 mif1_rmif_ctrl1;
	u32 mif1_rmif_ctrl2;
	u32 mif1_rmif_ctrl3;
	u32 mif1_rmif_ctrl4;
	u32 mif1_rmif_ctrl5;
	u32 mif2_rmif_ctrl;
	u32 mif2_rmif_baddr;
	u32 mif2_rmif_stride;
	u32 luma_rmif_scope_x;
	u32 luma_rmif_scope_y;
	u32 chrm_rmif_scope_x;
	u32 chrm_rmif_scope_y;
	u32 rmif_dummy_pixel;
	u32 rmif_rpt_loop;
	u32 rmif_f0_luma_rpt_pat;
	u32 rmif_f0_chrm_rpt_pat;
	u32 rmif_f1_luma_rpt_pat;
	u32 rmif_f1_chrm_rpt_pat;
	u32 rmif_f1_luma_ysize;
	u32 rmif_f1_chrm_ysize;
	u32 rmif_f1_mif0_baddr;
	u32 rmif_f1_mif1_baddr;
	u32 rmif_f1_mif2_baddr;
	u32 rmif_f1_stride0;
	u32 rmif_f1_stride1;
	u32 rmif_fifo_size;
	u32 rmif_chrm_psel;
	/* afbcd common */
	u32 vfcd_top_ctrl0;
	u32 vfcd_top_ctrl2;
	u32 vfcd_mmu_ctrl;
	u32 vfcd_afrc1_param;
	u32 vfcd_afbc_mode;
	u32 vfcd_conv_buf_lens;
	u32 vfcd_afbc_lbuf_depth;
	u32 vfcd_afbc_conv_ctrl;
	u32 vfcd_frm_pic_size;
	u32 vfcd_luma_pic_xpos;
	u32 vfcd_luma_pic_ypos;
	u32 vfcd_afbc_vd_cfmt_ctrl;
	u32 vfcd_afbc_vd_cfmt_w;
	u32 vfcd_afbc_vd_cfmt_h;
	/* afbc */
	u32 vfcd_afbc_iquant_enable;
	u32 vfcd_afbc_burst_ctrl;
	u32 vfcd_afbc_dec_def_color;
	u32 vfcd_afbc_head_baddr;
	u32 vfcd_afbc_body_baddr;
	u32 vfcd_afbc_enable;
	/* afrc */
	u32 vfcd_chrm_pic_xpos;
	u32 vfcd_chrm_pic_ypos;
	u32 vfcd_slc1_luma_pic_xpos;
	u32 vfcd_slc1_chrm_pic_xpos;
	u32 vfcd_slc2_luma_pic_xpos;
	u32 vfcd_slc2_chrm_pic_xpos;
	u32 vfcd_slc3_luma_pic_xpos;
	u32 vfcd_slc3_chrm_pic_xpos;
	u32 vfcd_post_luma_size;
	u32 vfcd_post_luma_win_h;
	u32 vfcd_post_chrm_win_h;
	u32 vfcd_post_luma_win_v;
	u32 vfcd_post_chrm_win_v;
	u32 vfcd_luma_pad_size;
	u32 vfcd_luma_pad_ofst;
	u32 vfcd_chrm_pad_size;
	u32 vfcd_chrm_pad_ofst;
	u32 vfcd_pad_dumy_data;
	u32 vfcd_mif1_urgent;
	u32 vfcd_mmu_baddr0;
	u32 vfcd_mmu_baddr1;
	u32 vfcd_afrc_ctrl0;
	u32 vfcd_afrc0_ctrl;
	u32 vfcd_afrc_ctrl1;
	u32 vfcd_afrc1_ctrl;
	u32 vfcd_afrc_chrm_head_baddr;
	u32 vfcd_afrc_chrm_body_baddr;
};

struct dpss_hw_vfcd_reg_s vfcd_reg_t6w_array[DPSS_MAX_LAYER] = {
	{
		// mif
		0x400 + RMIF_TOP_CTRL,
		0x400 + MIF0_RMIF_CTRL1,//plane0
		0x400 + MIF0_RMIF_CTRL2,
		0x400 + MIF0_RMIF_CTRL3,
		0x400 + MIF0_RMIF_CTRL4,
		0x400 + MIF0_RMIF_CTRL5,
		0x400 + MIF1_RMIF_CTRL1,//plane1
		0x400 + MIF1_RMIF_CTRL2,
		0x400 + MIF1_RMIF_CTRL3,
		0x400 + MIF1_RMIF_CTRL4,
		0x400 + MIF1_RMIF_CTRL5,
		0x400 + MIF2_RMIF_CTRL,//plane2
		0x400 + MIF2_RMIF_BADDR,
		0x400 + MIF2_RMIF_STRIDE,
		0x400 + LUMA_RMIF_SCOPE_X,
		0x400 + LUMA_RMIF_SCOPE_Y,
		0x400 + CHRM_RMIF_SCOPE_X,
		0x400 + CHRM_RMIF_SCOPE_Y,
		0x400 + RMIF_DUMMY_PIXEL,
		0x400 + RMIF_RPT_LOOP,
		0x400 + RMIF_F0_LUMA_RPT_PAT,
		0x400 + RMIF_F0_CHRM_RPT_PAT,
		0x400 + RMIF_F1_LUMA_RPT_PAT,//for 3d
		0x400 + RMIF_F1_CHRM_RPT_PAT,
		0x400 + RMIF_F1_LUMA_YSIZE,
		0x400 + RMIF_F1_CHRM_YSIZE,
		0x400 + RMIF_F1_MIF0_BADDR,
		0x400 + RMIF_F1_MIF1_BADDR,
		0x400 + RMIF_F1_MIF2_BADDR,
		0x400 + RMIF_F1_STRIDE0,
		0x400 + RMIF_F1_STRIDE1,
		0x400 + RMIF_FIFO_SIZE,
		0x400 + RMIF_CHRM_PSEL,
		// afbc common
		0x400 + VFCD_TOP_CTRL0,
		0x400 + VFCD_TOP_CTRL2,
		0x400 + VFCD_MMU_CTRL,
		0x400 + VFCD_AFRC1_PARAM,
		0x400 + VFCD_AFBC_MODE,
		0x400 + VFCD_CONV_BUF_LENS,
		0x400 + VFCD_AFBC_LBUF_DEPTH,
		0x400 + VFCD_AFBC_CONV_CTRL,
		0x400 + VFCD_FRM_PIC_SIZE,
		0x400 + VFCD_LUMA_PIC_XPOS,
		0x400 + VFCD_LUMA_PIC_YPOS,
		0x400 + VFCD_AFBC_VD_CFMT_CTRL,
		0x400 + VFCD_AFBC_VD_CFMT_W,
		0x400 + VFCD_AFBC_VD_CFMT_H,
		/* afbc */
		0x400 + VFCD_AFBC_IQUANT_ENABLE,
		0x400 + VFCD_AFBC_BURST_CTRL,
		0x400 + VFCD_AFBC_DEC_DEF_COLOR,
		0x400 + VFCD_AFBC_HEAD_BADDR,
		0x400 + VFCD_AFBC_BODY_BADDR,
		0x400 + VFCD_AFBC_ENABLE,
		/* for afrc */
		0x400 + VFCD_CHRM_PIC_XPOS,
		0x400 + VFCD_CHRM_PIC_YPOS,
		0x400 + VFCD_SLC1_LUMA_PIC_XPOS,
		0x400 + VFCD_SLC1_CHRM_PIC_XPOS,
		0x400 + VFCD_SLC2_LUMA_PIC_XPOS,
		0x400 + VFCD_SLC2_CHRM_PIC_XPOS,
		0x400 + VFCD_SLC3_LUMA_PIC_XPOS,
		0x400 + VFCD_SLC3_CHRM_PIC_XPOS,
		0x400 + VFCD_POST_LUMA_SIZE,
		0x400 + VFCD_POST_LUMA_WIN_H,
		0x400 + VFCD_POST_CHRM_WIN_H,
		0x400 + VFCD_POST_LUMA_WIN_V,
		0x400 + VFCD_POST_CHRM_WIN_V,
		0x400 + VFCD_LUMA_PAD_SIZE,
		0x400 + VFCD_LUMA_PAD_OFST,
		0x400 + VFCD_CHRM_PAD_SIZE,
		0x400 + VFCD_CHRM_PAD_OFST,
		0x400 + VFCD_PAD_DUMY_DATA,
		0x400 + VFCD_MIF1_URGENT,
		0x400 + VFCD_MMU_BADDR0,
		0x400 + VFCD_MMU_BADDR1,
		0x400 + VFCD_AFRC_CTRL0,
		0x400 + VFCD_AFRC0_CTRL,
		0x400 + VFCD_AFRC_CTRL1,
		0x400 + VFCD_AFRC1_CTRL,
		0x400 + VFCD_AFRC_CHRM_HEAD_BADDR,
		0x400 + VFCD_AFRC_CHRM_BODY_BADDR,
	},
	{
		// mif
		0x500 + RMIF_TOP_CTRL,
		0x500 + MIF0_RMIF_CTRL1,//plane0
		0x500 + MIF0_RMIF_CTRL2,
		0x500 + MIF0_RMIF_CTRL3,
		0x500 + MIF0_RMIF_CTRL4,
		0x500 + MIF0_RMIF_CTRL5,
		0x500 + MIF1_RMIF_CTRL1,//plane1
		0x500 + MIF1_RMIF_CTRL2,
		0x500 + MIF1_RMIF_CTRL3,
		0x500 + MIF1_RMIF_CTRL4,
		0x500 + MIF1_RMIF_CTRL5,
		0x500 + MIF2_RMIF_CTRL,//plane2
		0x500 + MIF2_RMIF_BADDR,
		0x500 + MIF2_RMIF_STRIDE,
		0x500 + LUMA_RMIF_SCOPE_X,
		0x500 + LUMA_RMIF_SCOPE_Y,
		0x500 + CHRM_RMIF_SCOPE_X,
		0x500 + CHRM_RMIF_SCOPE_Y,
		0x500 + RMIF_DUMMY_PIXEL,
		0x500 + RMIF_RPT_LOOP,
		0x500 + RMIF_F0_LUMA_RPT_PAT,
		0x500 + RMIF_F0_CHRM_RPT_PAT,
		0x500 + RMIF_F1_LUMA_RPT_PAT,//for 3d
		0x500 + RMIF_F1_CHRM_RPT_PAT,
		0x500 + RMIF_F1_LUMA_YSIZE,
		0x500 + RMIF_F1_CHRM_YSIZE,
		0x500 + RMIF_F1_MIF0_BADDR,
		0x500 + RMIF_F1_MIF1_BADDR,
		0x500 + RMIF_F1_MIF2_BADDR,
		0x500 + RMIF_F1_STRIDE0,
		0x500 + RMIF_F1_STRIDE1,
		0x500 + RMIF_FIFO_SIZE,
		0x500 + RMIF_CHRM_PSEL,
		// afbc common
		0x500 + VFCD_TOP_CTRL0,
		0x500 + VFCD_TOP_CTRL2,
		0x500 + VFCD_MMU_CTRL,
		0x500 + VFCD_AFRC1_PARAM,
		0x500 + VFCD_AFBC_MODE,
		0x500 + VFCD_CONV_BUF_LENS,
		0x500 + VFCD_AFBC_LBUF_DEPTH,
		0x500 + VFCD_AFBC_CONV_CTRL,
		0x500 + VFCD_FRM_PIC_SIZE,
		0x500 + VFCD_LUMA_PIC_XPOS,
		0x500 + VFCD_LUMA_PIC_YPOS,
		0x500 + VFCD_AFBC_VD_CFMT_CTRL,
		0x500 + VFCD_AFBC_VD_CFMT_W,
		0x500 + VFCD_AFBC_VD_CFMT_H,
		/* for afbc */
		0x500 + VFCD_AFBC_IQUANT_ENABLE,
		0x500 + VFCD_AFBC_BURST_CTRL,
		0x500 + VFCD_AFBC_DEC_DEF_COLOR,
		0x500 + VFCD_AFBC_HEAD_BADDR,
		0x500 + VFCD_AFBC_BODY_BADDR,
		0x500 + VFCD_AFBC_ENABLE,
		/* for afrc */
		0x500 + VFCD_CHRM_PIC_XPOS,
		0x500 + VFCD_CHRM_PIC_YPOS,
		0x500 + VFCD_SLC1_LUMA_PIC_XPOS,
		0x500 + VFCD_SLC1_CHRM_PIC_XPOS,
		0x500 + VFCD_SLC2_LUMA_PIC_XPOS,
		0x500 + VFCD_SLC2_CHRM_PIC_XPOS,
		0x500 + VFCD_SLC3_LUMA_PIC_XPOS,
		0x500 + VFCD_SLC3_CHRM_PIC_XPOS,
		0x500 + VFCD_POST_LUMA_SIZE,
		0x500 + VFCD_POST_LUMA_WIN_H,
		0x500 + VFCD_POST_CHRM_WIN_H,
		0x500 + VFCD_POST_LUMA_WIN_V,
		0x500 + VFCD_POST_CHRM_WIN_V,
		0x500 + VFCD_LUMA_PAD_SIZE,
		0x500 + VFCD_LUMA_PAD_OFST,
		0x500 + VFCD_CHRM_PAD_SIZE,
		0x500 + VFCD_CHRM_PAD_OFST,
		0x500 + VFCD_PAD_DUMY_DATA,
		0x500 + VFCD_MIF1_URGENT,
		0x500 + VFCD_MMU_BADDR0,
		0x500 + VFCD_MMU_BADDR1,
		0x500 + VFCD_AFRC_CTRL0,
		0x500 + VFCD_AFRC0_CTRL,
		0x500 + VFCD_AFRC_CTRL1,
		0x500 + VFCD_AFRC1_CTRL,
		0x500 + VFCD_AFRC_CHRM_HEAD_BADDR,
		0x500 + VFCD_AFRC_CHRM_BODY_BADDR,
	},
};

void dump_vfcd_reg_t6w(void)
{
	int i;
	u32 reg_addr, reg_val = 0;

	for (i = 0; i < DPSS_MAX_LAYER; i++) {
		// if (!glayer_info[i].afbc_support)
		pr_info("mc%d mif regs:\n", i);
		reg_addr = vfcd_reg_t6w_array[i].rmif_top_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_top_ctrl]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif0_rmif_ctrl1;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif0_rmif_ctrl1]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif0_rmif_ctrl2;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif0_rmif_ctrl2]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif0_rmif_ctrl3;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif0_rmif_ctrl3]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif0_rmif_ctrl4;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif0_rmif_ctrl4]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif0_rmif_ctrl5;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif0_rmif_ctrl5]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif1_rmif_ctrl1;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif1_rmif_ctrl1]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif1_rmif_ctrl2;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif1_rmif_ctrl2]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif1_rmif_ctrl3;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif1_rmif_ctrl3]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif1_rmif_ctrl4;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif1_rmif_ctrl4]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif1_rmif_ctrl5;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif1_rmif_ctrl5]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif2_rmif_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif2_rmif_ctrl]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif2_rmif_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif2_rmif_baddr]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].mif2_rmif_stride;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [mif2_rmif_stride]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].luma_rmif_scope_x;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [luma_rmif_scope_x]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].luma_rmif_scope_y;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [luma_rmif_scope_y]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].chrm_rmif_scope_x;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [chrm_rmif_scope_x]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].chrm_rmif_scope_y;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [chrm_rmif_scope_y]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].rmif_dummy_pixel;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_dummy_pixel]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].rmif_rpt_loop;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_rpt_loop]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].rmif_f0_luma_rpt_pat;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f0_luma_rpt_pat]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].rmif_f0_chrm_rpt_pat;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f0_chrm_rpt_pat]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_luma_rpt_pat;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_luma_rpt_pat]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_chrm_rpt_pat;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_chrm_rpt_pat]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_luma_ysize;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_luma_ysize]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_chrm_ysize;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_chrm_ysize]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_mif0_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_mif0_baddr]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_mif1_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_mif1_baddr]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_mif2_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_mif2_baddr]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_stride0;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_stride0]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_f1_stride1;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_f1_stride1]\n",
			reg_addr, reg_val);

		reg_addr = vfcd_reg_t6w_array[i].rmif_fifo_size;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_fifo_size]\n",
			reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].rmif_chrm_psel;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [rmif_chrm_psel]\n",
			reg_addr, reg_val);
				// continue;
		pr_info("mc%d vfcd common regs:\n", i);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_top_ctrl0;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_top_ctrl0]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_top_ctrl2;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_top_ctrl2]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_mmu_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_mmu_ctrl]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afrc1_param;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afrc1_param]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_mode;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_mode]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_conv_buf_lens;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_conv_buf_lens]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_lbuf_depth;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_lbuf_depth]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_conv_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_conv_ctrl]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_frm_pic_size;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_frm_pic_size]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_luma_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_luma_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_luma_pic_ypos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_luma_pic_ypos]\n", reg_addr,
			reg_val);

		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_vd_cfmt_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_vd_cfmt_ctrl]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_vd_cfmt_w;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_vd_cfmt_w]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_vd_cfmt_h;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_vd_cfmt_h]\n", reg_addr,
			reg_val);

		pr_info("afbcd regs:\n");
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_iquant_enable;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_iquant_enable]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_burst_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_burst_ctrl]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_dec_def_color;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_dec_def_color]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_head_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_head_baddr]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_body_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_body_baddr]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_enable;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_enable]\n", reg_addr,
			reg_val);

		pr_info("vfbcd regs:\n");
		reg_addr = vfcd_reg_t6w_array[i].vfcd_chrm_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_chrm_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_chrm_pic_ypos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_chrm_pic_ypos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_slc1_luma_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_slc1_luma_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_slc1_chrm_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_slc1_chrm_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_slc2_luma_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_slc2_luma_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_slc2_chrm_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_slc2_chrm_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_slc3_luma_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_slc3_luma_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_slc3_chrm_pic_xpos;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_slc3_chrm_pic_xpos]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_post_luma_size;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_post_luma_size]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_post_luma_win_h;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_post_luma_win_h]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_post_chrm_win_h;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_post_chrm_win_h]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_post_luma_win_v;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_post_luma_win_v]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_post_chrm_win_v;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_post_chrm_win_v]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_luma_pad_size;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_luma_pad_size]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_luma_pad_ofst;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_luma_pad_ofst]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_chrm_pad_size;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_chrm_pad_size]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_chrm_pad_ofst;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_chrm_pad_ofst]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_pad_dumy_data;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_pad_dumy_data]\n", reg_addr,
			reg_val);

		reg_addr = vfcd_reg_t6w_array[i].vfcd_mif1_urgent;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_mif1_urgent]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_mmu_baddr0;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_mmu_baddr0]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_mmu_baddr1;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_mmu_baddr1]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afrc_ctrl0;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afrc_ctrl0]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afrc0_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afrc0_ctrl]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afrc_ctrl1;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afrc_ctrl1]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afrc1_ctrl;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afrc1_ctrl]\n", reg_addr, reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_head_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_head_baddr]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afbc_body_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afbc_body_baddr]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afrc_chrm_head_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afrc_chrm_head_baddr]\n", reg_addr,
			reg_val);
		reg_addr = vfcd_reg_t6w_array[i].vfcd_afrc_chrm_body_baddr;
		reg_val = rd_vc(reg_addr);
		pr_info("[0x%x] = 0x%X [vfcd_afrc_chrm_body_baddr]\n", reg_addr,
			reg_val);
	}
}

void dump_param_frc_init_apb(void)
{
	pr_info("---------dump frc init tab----------\n");

	pr_info("0x8100, 0x%08x\r\n", rd(0x8100));
	pr_info("0x8101, 0x%08x\r\n", rd(0x8101));
	pr_info("0x8102, 0x%08x\r\n", rd(0x8102));
	pr_info("0x8103, 0x%08x\r\n", rd(0x8103));
	pr_info("0x8104, 0x%08x\r\n", rd(0x8104));
	pr_info("0x8105, 0x%08x\r\n", rd(0x8105));
	pr_info("0x8106, 0x%08x\r\n", rd(0x8106));
	pr_info("0x8107, 0x%08x\r\n", rd(0x8107));
	pr_info("0x8108, 0x%08x\r\n", rd(0x8108));
	pr_info("0x8109, 0x%08x\r\n", rd(0x8109));
	pr_info("0x810a, 0x%08x\r\n", rd(0x810a));
	pr_info("0x810b, 0x%08x\r\n", rd(0x810b));
	pr_info("0x810c, 0x%08x\r\n", rd(0x810c));
	pr_info("0x810d, 0x%08x\r\n", rd(0x810d));
	pr_info("0x810e, 0x%08x\r\n", rd(0x810e));
	pr_info("0x810f, 0x%08x\r\n", rd(0x810f));
	pr_info("0x8110, 0x%08x\r\n", rd(0x8110));
	pr_info("0x8111, 0x%08x\r\n", rd(0x8111));
	pr_info("0x8112, 0x%08x\r\n", rd(0x8112));
	pr_info("0x8113, 0x%08x\r\n", rd(0x8113));
	pr_info("0x8114, 0x%08x\r\n", rd(0x8114));
	pr_info("0x8115, 0x%08x\r\n", rd(0x8115));
	pr_info("0x8116, 0x%08x\r\n", rd(0x8116));
	pr_info("0x8117, 0x%08x\r\n", rd(0x8117));
	pr_info("0x8118, 0x%08x\r\n", rd(0x8118));
	pr_info("0x8119, 0x%08x\r\n", rd(0x8119));
	pr_info("0x811a, 0x%08x\r\n", rd(0x811a));
	pr_info("0x811b, 0x%08x\r\n", rd(0x811b));
	pr_info("0x811c, 0x%08x\r\n", rd(0x811c));
	pr_info("0x811d, 0x%08x\r\n", rd(0x811d));
	pr_info("0x811e, 0x%08x\r\n", rd(0x811e));
	pr_info("0x811f, 0x%08x\r\n", rd(0x811f));
	pr_info("0x8120, 0x%08x\r\n", rd(0x8120));
	pr_info("0x8121, 0x%08x\r\n", rd(0x8121));
	pr_info("0x8122, 0x%08x\r\n", rd(0x8122));
	pr_info("0x8123, 0x%08x\r\n", rd(0x8123));
	pr_info("0x8124, 0x%08x\r\n", rd(0x8124));
	pr_info("0x8125, 0x%08x\r\n", rd(0x8125));
	pr_info("0x8126, 0x%08x\r\n", rd(0x8126));
	pr_info("0x8127, 0x%08x\r\n", rd(0x8127));
	pr_info("0x8128, 0x%08x\r\n", rd(0x8128));
	pr_info("0x8129, 0x%08x\r\n", rd(0x8129));
	pr_info("0x812a, 0x%08x\r\n", rd(0x812a));
	pr_info("0x812b, 0x%08x\r\n", rd(0x812b));
	pr_info("0x812c, 0x%08x\r\n", rd(0x812c));
	pr_info("0x812d, 0x%08x\r\n", rd(0x812d));
	pr_info("0x812e, 0x%08x\r\n", rd(0x812e));
	pr_info("0x812f, 0x%08x\r\n", rd(0x812f));
	pr_info("0x8130, 0x%08x\r\n", rd(0x8130));
	pr_info("0x8131, 0x%08x\r\n", rd(0x8131));
	pr_info("0x8132, 0x%08x\r\n", rd(0x8132));
	pr_info("0x8133, 0x%08x\r\n", rd(0x8133));
	pr_info("0x8134, 0x%08x\r\n", rd(0x8134));
	pr_info("0x8135, 0x%08x\r\n", rd(0x8135));
	pr_info("0x8136, 0x%08x\r\n", rd(0x8136));
	pr_info("0x8137, 0x%08x\r\n", rd(0x8137));
	pr_info("0x8138, 0x%08x\r\n", rd(0x8138));
	pr_info("0x8139, 0x%08x\r\n", rd(0x8139));
	pr_info("0x813a, 0x%08x\r\n", rd(0x813a));
	pr_info("0x813b, 0x%08x\r\n", rd(0x813b));
	pr_info("0x813c, 0x%08x\r\n", rd(0x813c));
	pr_info("0x813d, 0x%08x\r\n", rd(0x813d));
	pr_info("0x813e, 0x%08x\r\n", rd(0x813e));
	pr_info("0x813f, 0x%08x\r\n", rd(0x813f));
	pr_info("0x8140, 0x%08x\r\n", rd(0x8140));
	pr_info("0x8141, 0x%08x\r\n", rd(0x8141));
	pr_info("0x8142, 0x%08x\r\n", rd(0x8142));
	pr_info("0x8143, 0x%08x\r\n", rd(0x8143));
	pr_info("0x8144, 0x%08x\r\n", rd(0x8144));
	pr_info("0x8145, 0x%08x\r\n", rd(0x8145));
	pr_info("0x8146, 0x%08x\r\n", rd(0x8146));
	pr_info("0x8147, 0x%08x\r\n", rd(0x8147));
	pr_info("0x8148, 0x%08x\r\n", rd(0x8148));
	pr_info("0x8149, 0x%08x\r\n", rd(0x8149));
	pr_info("0x814a, 0x%08x\r\n", rd(0x814a));
	pr_info("0x814b, 0x%08x\r\n", rd(0x814b));
	pr_info("0x8150, 0x%08x\r\n", rd(0x8150));
	pr_info("0x8151, 0x%08x\r\n", rd(0x8151));
	pr_info("0x8152, 0x%08x\r\n", rd(0x8152));
	pr_info("0x8153, 0x%08x\r\n", rd(0x8153));
	pr_info("0x8154, 0x%08x\r\n", rd(0x8154));
	pr_info("0x8155, 0x%08x\r\n", rd(0x8155));
	pr_info("0x8156, 0x%08x\r\n", rd(0x8156));
	pr_info("0x8157, 0x%08x\r\n", rd(0x8157));
	pr_info("0x8158, 0x%08x\r\n", rd(0x8158));
	pr_info("0x8159, 0x%08x\r\n", rd(0x8159));
	pr_info("0x815a, 0x%08x\r\n", rd(0x815a));
	pr_info("0x815b, 0x%08x\r\n", rd(0x815b));
	pr_info("0x815c, 0x%08x\r\n", rd(0x815c));
	pr_info("0x815d, 0x%08x\r\n", rd(0x815d));
	pr_info("0x815e, 0x%08x\r\n", rd(0x815e));
	pr_info("0x815f, 0x%08x\r\n", rd(0x815f));
	pr_info("0x8160, 0x%08x\r\n", rd(0x8160));
	pr_info("0x8161, 0x%08x\r\n", rd(0x8161));
	pr_info("0x8162, 0x%08x\r\n", rd(0x8162));
	pr_info("0x8163, 0x%08x\r\n", rd(0x8163));
	pr_info("0x8164, 0x%08x\r\n", rd(0x8164));
	pr_info("0x8165, 0x%08x\r\n", rd(0x8165));
	pr_info("0x8166, 0x%08x\r\n", rd(0x8166));
	pr_info("0x8167, 0x%08x\r\n", rd(0x8167));
	pr_info("0x8168, 0x%08x\r\n", rd(0x8168));
	pr_info("0x8169, 0x%08x\r\n", rd(0x8169));
	pr_info("0x816a, 0x%08x\r\n", rd(0x816a));
	pr_info("0x816b, 0x%08x\r\n", rd(0x816b));
	pr_info("0x816c, 0x%08x\r\n", rd(0x816c));
	pr_info("0x816d, 0x%08x\r\n", rd(0x816d));
	pr_info("0x816e, 0x%08x\r\n", rd(0x816e));
	pr_info("0x816f, 0x%08x\r\n", rd(0x816f));
	pr_info("0x8170, 0x%08x\r\n", rd(0x8170));
	pr_info("0x8171, 0x%08x\r\n", rd(0x8171));
	pr_info("0x8172, 0x%08x\r\n", rd(0x8172));
	pr_info("0x8173, 0x%08x\r\n", rd(0x8173));
	pr_info("0x8174, 0x%08x\r\n", rd(0x8174));
	pr_info("0x8175, 0x%08x\r\n", rd(0x8175));
	pr_info("0x8176, 0x%08x\r\n", rd(0x8176));
	pr_info("0x8177, 0x%08x\r\n", rd(0x8177));
	pr_info("0x8178, 0x%08x\r\n", rd(0x8178));
	pr_info("0x8179, 0x%08x\r\n", rd(0x8179));
	pr_info("0x817a, 0x%08x\r\n", rd(0x817a));
	pr_info("0x817b, 0x%08x\r\n", rd(0x817b));
	pr_info("0x8180, 0x%08x\r\n", rd(0x8180));
	pr_info("0x8181, 0x%08x\r\n", rd(0x8181));
	pr_info("0x8182, 0x%08x\r\n", rd(0x8182));
	pr_info("0x8183, 0x%08x\r\n", rd(0x8183));
	pr_info("0x8184, 0x%08x\r\n", rd(0x8184));
	pr_info("0x8185, 0x%08x\r\n", rd(0x8185));
	pr_info("0x8186, 0x%08x\r\n", rd(0x8186));
	pr_info("0x8187, 0x%08x\r\n", rd(0x8187));
	pr_info("0x8188, 0x%08x\r\n", rd(0x8188));
	pr_info("0x8189, 0x%08x\r\n", rd(0x8189));
	pr_info("---------dump frc init tab end----------\n");
}

void dump_src0_buf_regs(void)
{
	int i;

	pr_info("---------dump dpss buf regs addr tab----------\n");
	pr_info("0x8205, 0x%08x\r\n", rd(DPSS_INP_MBUF_ADDR0));
	pr_info("0x8206, 0x%08x\r\n", rd(DPSS_INP_MBUF_ADDR1));
	pr_info("0x8207, 0x%08x\r\n", rd(DPSS_FRC_ME_MBUF_ADDR));
	pr_info("0x8208, 0x%08x\r\n", rd(DPSS_SRC0_ME_NC_UNI_MV_ADDR));
	pr_info("0x8209, 0x%08x\r\n", rd(DPSS_SRC0_ME_CN_UNI_MV_ADDR));
	pr_info("0x8210, 0x%08x\r\n", rd(DPSS_SRC0_ME_PC_PHS_MV_ADDR));
	pr_info("0x8211, 0x%08x\r\n", rd(DPSS_SRC0_LOGO_IIR_BUF_ADDR));
	pr_info("0x8212, 0x%08x\r\n", rd(DPSS_SRC0_LOGO_SCC_BUF_ADDR));
	pr_info("0x8213, 0x%08x\r\n", rd(DPSS_SRC0_BLK_LOGO_ADDR));
	pr_info("0x8214, 0x%08x\r\n", rd(DPSS_SRC0_PIX_LOGO_ADDR));
	pr_info("0x8215, 0x%08x\r\n", rd(DPSS_SRC0_VP_MC_MV_ADDR));
	pr_info("0x8216, 0x%08x\r\n", rd(DPSS_SRC0_VP_MC_LOGO_ADDR));
	pr_info("0x8217, 0x%08x\r\n", rd(DPSS_SRC0_FRC_MERO_ADDR));

	pr_info("0x8246, 0x%08x\r\n", rd(DPSS_INP_MBUF_STEP));
	pr_info("0x8247, 0x%08x\r\n", rd(DPSS_FRC_ME_MBUF_STEP));
	pr_info("0x8248, 0x%08x\r\n", rd(DPSS_SRC0_ME_NC_UNI_MV_STEP));
	pr_info("0x8249, 0x%08x\r\n", rd(DPSS_SRC0_ME_CN_UNI_MV_STEP));
	pr_info("0x8250, 0x%08x\r\n", rd(DPSS_SRC0_ME_PC_PHS_MV_STEP));
	pr_info("0x8251, 0x%08x\r\n", rd(DPSS_SRC0_LOGO_IIR_BUF_STEP));
	pr_info("0x8252, 0x%08x\r\n", rd(DPSS_SRC0_LOGO_SCC_BUF_STEP));
	pr_info("0x8253, 0x%08x\r\n", rd(DPSS_SRC0_BLK_LOGO_STEP));
	pr_info("0x8254, 0x%08x\r\n", rd(DPSS_SRC0_PIX_LOGO_STEP));
	pr_info("0x8255, 0x%08x\r\n", rd(DPSS_SRC0_VP_MC_MV_STEP));
	pr_info("0x8256, 0x%08x\r\n", rd(DPSS_SRC0_VP_MC_LOGO_STEP));
	pr_info("0x8257, 0x%08x\r\n", rd(DPSS_FRC_MERO_STEP));

	pr_info("0x8400, 0x%08x\r\n", rd(DPSS_SRC0_MIX_PBUF_ADDR));
	pr_info("0x8401, 0x%08x\r\n", rd(DPSS_SRC0_MIX_CBUF_ADDR));
	pr_info("0x8402, 0x%08x\r\n", rd(DPSS_SRC0_ME_INFO_ADDR));
	pr_info("0x8403, 0x%08x\r\n", rd(DPSS_SRC0_ME_MTN_ADDR));
	pr_info("0x8404, 0x%08x\r\n", rd(DPSS_SRC0_ME_ALP_ADDR));
	pr_info("0x8405, 0x%08x\r\n", rd(DPSS_SRC0_ME_TFBF_ADDR));
	pr_info("0x8406, 0x%08x\r\n", rd(DPSS_SRC0_NRDI_MV_ADDR));
	pr_info("0x8407, 0x%08x\r\n", rd(DPSS_SRC0_DCG_HIST_ADDR));
	pr_info("0x8408, 0x%08x\r\n", rd(DPSS_SRC0_DCG_YDS_ADDR));
	pr_info("0x8409, 0x%08x\r\n", rd(DPSS_SRC0_DCG_CDS_ADDR));
	pr_info("0x840a, 0x%08x\r\n", rd(DPSS_SRC0_DV_PYMD_ADDR));
	pr_info("0x840b, 0x%08x\r\n", rd(DPSS_SRC0_DV_HIST_ADDR));
	pr_info("0x840c, 0x%08x\r\n", rd(DPSS_SRC0_DV_INTP_ADDR));
	pr_info("0x840d, 0x%08x\r\n", rd(DPSS_SRC0_NR_HDBLK_ADDR));
	pr_info("0x840e, 0x%08x\r\n", rd(DPSS_SRC0_NR_VDBLK_ADDR));
	pr_info("0x840f, 0x%08x\r\n", rd(DPSS_SRC0_NR_DMSQ_ADDR));
	pr_info("0x8410, 0x%08x\r\n", rd(DPSS_SRC0_NRODW_MBUF_ADDR));
	pr_info("0x8411, 0x%08x\r\n", rd(DPSS_SRC0_INP_YHEAD_ADDR));
	pr_info("0x8412, 0x%08x\r\n", rd(DPSS_SRC0_INP_CHEAD_ADDR));
	pr_info("0x8413, 0x%08x\r\n", rd(DPSS_SRC0_NROUT_YHEAD_ADDR));
	pr_info("0x8414, 0x%08x\r\n", rd(DPSS_SRC0_NROUT_CHEAD_ADDR));
	pr_info("0x8415, 0x%08x\r\n", rd(DPSS_SRC0_DIOUT_YHEAD_ADDR));
	pr_info("0x8416, 0x%08x\r\n", rd(DPSS_SRC0_DIOUT_CHEAD_ADDR));
	pr_info("0x8417, 0x%08x\r\n", rd(DPSS_SRC0_NR_MERO_ADDR));
	pr_info("0x8418, 0x%08x\r\n", rd(DPSS_SRC0_DPE_RDMA_ADDR));
	pr_info("0xd6d0, 0x%08x\r\n", rd(DPSS_DPE_DIN_HDBLK_BADDR));
	pr_info("0x8420, 0x%08x\r\n", rd(DPSS_SRC0_MIX_PBUF_STEP));
	pr_info("0x8421, 0x%08x\r\n", rd(DPSS_SRC0_MIX_CBUF_STEP));
	pr_info("0x8422, 0x%08x\r\n", rd(DPSS_SRC0_ME_INFO_STEP));
	pr_info("0x8423, 0x%08x\r\n", rd(DPSS_SRC0_ME_MTN_STEP));
	pr_info("0x8424, 0x%08x\r\n", rd(DPSS_SRC0_ME_ALP_STEP));
	pr_info("0x8425, 0x%08x\r\n", rd(DPSS_SRC0_ME_TFBF_STEP));
	pr_info("0x8426, 0x%08x\r\n", rd(DPSS_SRC0_NRDI_MV_STEP));
	pr_info("0x8427, 0x%08x\r\n", rd(DPSS_SRC0_DCG_HIST_STEP));
	pr_info("0x8428, 0x%08x\r\n", rd(DPSS_SRC0_DCG_YDS_STEP));
	pr_info("0x8429, 0x%08x\r\n", rd(DPSS_SRC0_DCG_CDS_STEP));
	pr_info("0x842a, 0x%08x\r\n", rd(DPSS_SRC0_DV_PYMD_STEP));
	pr_info("0x842b, 0x%08x\r\n", rd(DPSS_SRC0_DV_HIST_STEP));
	pr_info("0x842c, 0x%08x\r\n", rd(DPSS_SRC0_DV_INTP_STEP));
	pr_info("0x842d, 0x%08x\r\n", rd(DPSS_SRC0_NR_HDBLK_STEP));
	pr_info("0x842e, 0x%08x\r\n", rd(DPSS_SRC0_NR_VDBLK_STEP));
	pr_info("0x842f, 0x%08x\r\n", rd(DPSS_SRC0_NR_DMSQ_STEP));
	pr_info("0x8430, 0x%08x\r\n", rd(DPSS_SRC0_NRODW_MBUF_STEP));
	pr_info("0x8431, 0x%08x\r\n", rd(DPSS_SRC0_INP_YHEAD_STEP));
	pr_info("0x8432, 0x%08x\r\n", rd(DPSS_SRC0_INP_CHEAD_STEP));
	pr_info("0x8433, 0x%08x\r\n", rd(DPSS_SRC0_NROUT_YHEAD_STEP));
	pr_info("0x8434, 0x%08x\r\n", rd(DPSS_SRC0_NROUT_CHEAD_STEP));
	pr_info("0x8435, 0x%08x\r\n", rd(DPSS_SRC0_DIOUT_YHEAD_STEP));
	pr_info("0x8436, 0x%08x\r\n", rd(DPSS_SRC0_DIOUT_CHEAD_STEP));
	pr_info("0x8437, 0x%08x\r\n", rd(DPSS_SRC0_MERO_STEP));
	pr_info("0x8438, 0x%08x\r\n", rd(DPSS_SRC0_DPE_RDMA_STEP));

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x8460 + i,
			rd(DPSS_SRC0_FBUF_YADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x8470 + i,
			rd(DPSS_SRC0_FBUF_CADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x8480 + i,
			rd(DPSS_SRC0_DWBUF_YADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x8490 + i,
			rd(DPSS_SRC0_DWBUF_CADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x84a0 + i,
			rd(DPSS_SRC0_NRO_FBUF_YADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x84b0 + i,
			rd(DPSS_SRC0_DIO_FBUF_YADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x84c0 + i,
			rd(DPSS_SRC0_NRO_FBUF_CADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x84d0 + i,
			rd(DPSS_SRC0_DIO_FBUF_CADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x84e0 + i,
			rd(DPSS_SRC0_DPEO_DWBUF_YADDR0 + i));
	}

	for (i = 0; i <= 15; i++) {
		pr_info("0x%04x, 0x%08x\r\n", 0x84f0 + i,
			rd(DPSS_SRC0_DPEO_DWBUF_CADDR0 + i));
	}

	pr_info("---------dump dpss buf regs addr tab end----------\n");
}

void dump_frc_pd_regs_apb(void)
{
	pr_info("---------dump frc pd regs tab----------\n");
	pr_info("0x8d00, 0x%08x\r\n", rd(0x8d00));
	pr_info("0x8d01, 0x%08x\r\n", rd(0x8d01));
	pr_info("0x8d02, 0x%08x\r\n", rd(0x8d02));
	pr_info("0x8d03, 0x%08x\r\n", rd(0x8d03));
	pr_info("0x8d04, 0x%08x\r\n", rd(0x8d04));
	pr_info("0x8d05, 0x%08x\r\n", rd(0x8d05));
	pr_info("0x8d06, 0x%08x\r\n", rd(0x8d06));
	pr_info("0x8d07, 0x%08x\r\n", rd(0x8d07));
	pr_info("0x8d08, 0x%08x\r\n", rd(0x8d08));
	pr_info("0x8d09, 0x%08x\r\n", rd(0x8d09));
	pr_info("0x8d0a, 0x%08x\r\n", rd(0x8d0a));
	pr_info("0x8d0b, 0x%08x\r\n", rd(0x8d0b));
	pr_info("0x8d0c, 0x%08x\r\n", rd(0x8d0c));
	pr_info("0x8d0d, 0x%08x\r\n", rd(0x8d0d));
	pr_info("0x8d0e, 0x%08x\r\n", rd(0x8d0e));
	pr_info("0x8d0f, 0x%08x\r\n", rd(0x8d0f));
	pr_info("0x8d10, 0x%08x\r\n", rd(0x8d10));
	pr_info("0x8d11, 0x%08x\r\n", rd(0x8d11));
	pr_info("0x8d12, 0x%08x\r\n", rd(0x8d12));
	pr_info("0x8d13, 0x%08x\r\n", rd(0x8d13));
	pr_info("0x8d14, 0x%08x\r\n", rd(0x8d14));
	pr_info("0x8d15, 0x%08x\r\n", rd(0x8d15));
	pr_info("0x8d16, 0x%08x\r\n", rd(0x8d16));
	pr_info("0x8d17, 0x%08x\r\n", rd(0x8d17));
	pr_info("0x8d18, 0x%08x\r\n", rd(0x8d18));
	pr_info("0x8d19, 0x%08x\r\n", rd(0x8d19));
	pr_info("0x8d1a, 0x%08x\r\n", rd(0x8d1a));
	pr_info("0x8d1b, 0x%08x\r\n", rd(0x8d1b));
	pr_info("0x8d1c, 0x%08x\r\n", rd(0x8d1c));
	pr_info("0x8d1d, 0x%08x\r\n", rd(0x8d1d));
	pr_info("0x8d1e, 0x%08x\r\n", rd(0x8d1e));
	pr_info("0x8d1f, 0x%08x\r\n", rd(0x8d1f));
	pr_info("0x8d20, 0x%08x\r\n", rd(0x8d20));
	pr_info("0x8d21, 0x%08x\r\n", rd(0x8d21));
	pr_info("0x8d22, 0x%08x\r\n", rd(0x8d22));
	pr_info("0x8d23, 0x%08x\r\n", rd(0x8d23));
	pr_info("0x8d24, 0x%08x\r\n", rd(0x8d24));
	pr_info("0x8d25, 0x%08x\r\n", rd(0x8d25));
	pr_info("0x8d26, 0x%08x\r\n", rd(0x8d26));
	pr_info("0x8d27, 0x%08x\r\n", rd(0x8d27));
	pr_info("0x8d28, 0x%08x\r\n", rd(0x8d28));
	pr_info("0x8d29, 0x%08x\r\n", rd(0x8d29));
	pr_info("0x8d2a, 0x%08x\r\n", rd(0x8d2a));
	pr_info("0x8d2b, 0x%08x\r\n", rd(0x8d2b));
	pr_info("0x8d2c, 0x%08x\r\n", rd(0x8d2c));
	pr_info("0x8d2d, 0x%08x\r\n", rd(0x8d2d));
	pr_info("0x8d2e, 0x%08x\r\n", rd(0x8d2e));
	pr_info("0x8d2f, 0x%08x\r\n", rd(0x8d2f));
	pr_info("0x8d30, 0x%08x\r\n", rd(0x8d30));
	pr_info("0x8d31, 0x%08x\r\n", rd(0x8d31));
	pr_info("0x8d32, 0x%08x\r\n", rd(0x8d32));
	pr_info("0x8d33, 0x%08x\r\n", rd(0x8d33));
	pr_info("0x8d34, 0x%08x\r\n", rd(0x8d34));
	pr_info("0x8d35, 0x%08x\r\n", rd(0x8d35));
	pr_info("0x8d36, 0x%08x\r\n", rd(0x8d36));
	pr_info("0x8d37, 0x%08x\r\n", rd(0x8d37));
	pr_info("0x8d38, 0x%08x\r\n", rd(0x8d38));
	pr_info("0x8d39, 0x%08x\r\n", rd(0x8d39));
	pr_info("0x8d3a, 0x%08x\r\n", rd(0x8d3a));
	pr_info("0x8d3b, 0x%08x\r\n", rd(0x8d3b));
	pr_info("0x8d3c, 0x%08x\r\n", rd(0x8d3c));
	pr_info("0x8d3d, 0x%08x\r\n", rd(0x8d3d));
	pr_info("0x8d3e, 0x%08x\r\n", rd(0x8d3e));
	pr_info("0x8d3f, 0x%08x\r\n", rd(0x8d3f));
	pr_info("0x8d40, 0x%08x\r\n", rd(0x8d40));
	pr_info("0x8d41, 0x%08x\r\n", rd(0x8d41));
	pr_info("0x8d42, 0x%08x\r\n", rd(0x8d42));
	pr_info("0x8d43, 0x%08x\r\n", rd(0x8d43));
	pr_info("0x8d44, 0x%08x\r\n", rd(0x8d44));
	pr_info("0x8d45, 0x%08x\r\n", rd(0x8d45));
	pr_info("0x8d46, 0x%08x\r\n", rd(0x8d46));
	pr_info("0x8d47, 0x%08x\r\n", rd(0x8d47));
	pr_info("0x8d48, 0x%08x\r\n", rd(0x8d48));
	pr_info("0x8d49, 0x%08x\r\n", rd(0x8d49));
	pr_info("0x8d4a, 0x%08x\r\n", rd(0x8d4a));
	pr_info("0x8d4b, 0x%08x\r\n", rd(0x8d4b));
	pr_info("0x8d4c, 0x%08x\r\n", rd(0x8d4c));
	pr_info("0x8d4d, 0x%08x\r\n", rd(0x8d4d));
	pr_info("0x8d4e, 0x%08x\r\n", rd(0x8d4e));
	pr_info("0x8d4f, 0x%08x\r\n", rd(0x8d4f));
	pr_info("0x8d50, 0x%08x\r\n", rd(0x8d50));
	pr_info("0x8d51, 0x%08x\r\n", rd(0x8d51));
	pr_info("0x8d52, 0x%08x\r\n", rd(0x8d52));
	pr_info("0x8d53, 0x%08x\r\n", rd(0x8d53));
	pr_info("0x8d54, 0x%08x\r\n", rd(0x8d54));
	pr_info("0x8d55, 0x%08x\r\n", rd(0x8d55));
	pr_info("0x8d56, 0x%08x\r\n", rd(0x8d56));
	pr_info("0x8d57, 0x%08x\r\n", rd(0x8d57));
	pr_info("0x8d58, 0x%08x\r\n", rd(0x8d58));
	pr_info("0x8d59, 0x%08x\r\n", rd(0x8d59));
	pr_info("0x8d5a, 0x%08x\r\n", rd(0x8d5a));
	pr_info("0x8d5b, 0x%08x\r\n", rd(0x8d5b));
	pr_info("0x8d5c, 0x%08x\r\n", rd(0x8d5c));
	pr_info("0x8d5d, 0x%08x\r\n", rd(0x8d5d));
	pr_info("0x8d5e, 0x%08x\r\n", rd(0x8d5e));
	pr_info("0x8d5f, 0x%08x\r\n", rd(0x8d5f));
	pr_info("---------dump frc pd regs tab end----------\n");
}

void dump_iplogo_regs_apb(void)
{
	pr_info("---------dump iplogo regs tab----------\n");
	pr_info("0x8b00, 0x%08x\r\n", rd(0x8b00));
	pr_info("0x8b01, 0x%08x\r\n", rd(0x8b01));
	pr_info("0x8b02, 0x%08x\r\n", rd(0x8b02));
	pr_info("0x8b03, 0x%08x\r\n", rd(0x8b03));
	pr_info("0x8b04, 0x%08x\r\n", rd(0x8b04));
	pr_info("0x8b05, 0x%08x\r\n", rd(0x8b05));
	pr_info("0x8b06, 0x%08x\r\n", rd(0x8b06));
	pr_info("0x8b07, 0x%08x\r\n", rd(0x8b07));
	pr_info("0x8b08, 0x%08x\r\n", rd(0x8b08));
	pr_info("0x8b09, 0x%08x\r\n", rd(0x8b09));
	pr_info("0x8b0a, 0x%08x\r\n", rd(0x8b0a));
	pr_info("0x8b0b, 0x%08x\r\n", rd(0x8b0b));
	pr_info("0x8b0c, 0x%08x\r\n", rd(0x8b0c));
	pr_info("0x8b0d, 0x%08x\r\n", rd(0x8b0d));
	pr_info("0x8b0e, 0x%08x\r\n", rd(0x8b0e));
	pr_info("0x8b0f, 0x%08x\r\n", rd(0x8b0f));
	pr_info("0x8b10, 0x%08x\r\n", rd(0x8b10));
	pr_info("0x8b11, 0x%08x\r\n", rd(0x8b11));
	pr_info("0x8b12, 0x%08x\r\n", rd(0x8b12));
	pr_info("0x8b13, 0x%08x\r\n", rd(0x8b13));
	pr_info("0x8b14, 0x%08x\r\n", rd(0x8b14));
	pr_info("0x8b15, 0x%08x\r\n", rd(0x8b15));
	pr_info("0x8b16, 0x%08x\r\n", rd(0x8b16));
	pr_info("0x8b17, 0x%08x\r\n", rd(0x8b17));
	pr_info("0x8b18, 0x%08x\r\n", rd(0x8b18));
	pr_info("0x8b19, 0x%08x\r\n", rd(0x8b19));
	pr_info("0x8b1a, 0x%08x\r\n", rd(0x8b1a));
	pr_info("0x8b1b, 0x%08x\r\n", rd(0x8b1b));
	pr_info("0x8b1c, 0x%08x\r\n", rd(0x8b1c));
	pr_info("0x8b1d, 0x%08x\r\n", rd(0x8b1d));
	pr_info("0x8b1e, 0x%08x\r\n", rd(0x8b1e));
	pr_info("0x8b1f, 0x%08x\r\n", rd(0x8b1f));
	pr_info("0x8b20, 0x%08x\r\n", rd(0x8b20));
	pr_info("0x8b21, 0x%08x\r\n", rd(0x8b21));
	pr_info("0x8b22, 0x%08x\r\n", rd(0x8b22));
	pr_info("0x8b23, 0x%08x\r\n", rd(0x8b23));
	pr_info("0x8b24, 0x%08x\r\n", rd(0x8b24));
	pr_info("0x8b25, 0x%08x\r\n", rd(0x8b25));
	pr_info("0x8b26, 0x%08x\r\n", rd(0x8b26));
	pr_info("0x8b27, 0x%08x\r\n", rd(0x8b27));
	pr_info("0x8b28, 0x%08x\r\n", rd(0x8b28));
	pr_info("0x8b29, 0x%08x\r\n", rd(0x8b29));
	pr_info("0x8b2a, 0x%08x\r\n", rd(0x8b2a));
	pr_info("0x8b2b, 0x%08x\r\n", rd(0x8b2b));
	pr_info("0x8b2c, 0x%08x\r\n", rd(0x8b2c));
	pr_info("0x8b2d, 0x%08x\r\n", rd(0x8b2d));
	pr_info("0x8b2e, 0x%08x\r\n", rd(0x8b2e));
	pr_info("0x8b2f, 0x%08x\r\n", rd(0x8b2f));
	pr_info("0x8b30, 0x%08x\r\n", rd(0x8b30));
	pr_info("0x8b31, 0x%08x\r\n", rd(0x8b31));
	pr_info("0x8b32, 0x%08x\r\n", rd(0x8b32));
	pr_info("0x8b33, 0x%08x\r\n", rd(0x8b33));
	pr_info("0x8b34, 0x%08x\r\n", rd(0x8b34));
	pr_info("0x8b35, 0x%08x\r\n", rd(0x8b35));
	pr_info("0x8b36, 0x%08x\r\n", rd(0x8b36));
	pr_info("0x8b37, 0x%08x\r\n", rd(0x8b37));
	pr_info("0x8b38, 0x%08x\r\n", rd(0x8b38));
	pr_info("0x8b39, 0x%08x\r\n", rd(0x8b39));
	pr_info("0x8b3a, 0x%08x\r\n", rd(0x8b3a));
	pr_info("0x8b3b, 0x%08x\r\n", rd(0x8b3b));
	pr_info("0x8b3c, 0x%08x\r\n", rd(0x8b3c));
	pr_info("0x8b3d, 0x%08x\r\n", rd(0x8b3d));
	pr_info("0x8b3e, 0x%08x\r\n", rd(0x8b3e));
	pr_info("0x8b3f, 0x%08x\r\n", rd(0x8b3f));
	pr_info("0x8b40, 0x%08x\r\n", rd(0x8b40));
	pr_info("0x8b41, 0x%08x\r\n", rd(0x8b41));
	pr_info("0x8b42, 0x%08x\r\n", rd(0x8b42));
	pr_info("0x8b43, 0x%08x\r\n", rd(0x8b43));
	pr_info("0x8b44, 0x%08x\r\n", rd(0x8b44));
	pr_info("0x8b45, 0x%08x\r\n", rd(0x8b45));
	pr_info("0x8b46, 0x%08x\r\n", rd(0x8b46));
	pr_info("0x8b47, 0x%08x\r\n", rd(0x8b47));
	pr_info("0x8b48, 0x%08x\r\n", rd(0x8b48));
	pr_info("0x8b49, 0x%08x\r\n", rd(0x8b49));
	pr_info("0x8b4a, 0x%08x\r\n", rd(0x8b4a));
	pr_info("0x8b4b, 0x%08x\r\n", rd(0x8b4b));
	pr_info("0x8b4c, 0x%08x\r\n", rd(0x8b4c));
	pr_info("0x8b4d, 0x%08x\r\n", rd(0x8b4d));
	pr_info("0x8b4e, 0x%08x\r\n", rd(0x8b4e));
	pr_info("0x8b4f, 0x%08x\r\n", rd(0x8b4f));
	pr_info("0x8b50, 0x%08x\r\n", rd(0x8b50));
	pr_info("0x8b51, 0x%08x\r\n", rd(0x8b51));
	pr_info("0x8b52, 0x%08x\r\n", rd(0x8b52));
	pr_info("0x8b53, 0x%08x\r\n", rd(0x8b53));
	pr_info("0x8b54, 0x%08x\r\n", rd(0x8b54));
	pr_info("0x8b55, 0x%08x\r\n", rd(0x8b55));
	pr_info("0x8b56, 0x%08x\r\n", rd(0x8b56));
	pr_info("0x8b57, 0x%08x\r\n", rd(0x8b57));
	pr_info("0x8b58, 0x%08x\r\n", rd(0x8b58));
	pr_info("0x8b59, 0x%08x\r\n", rd(0x8b59));
	pr_info("0x8b5a, 0x%08x\r\n", rd(0x8b5a));
	pr_info("0x8b5b, 0x%08x\r\n", rd(0x8b5b));
	pr_info("0x8b5c, 0x%08x\r\n", rd(0x8b5c));
	pr_info("0x8b5d, 0x%08x\r\n", rd(0x8b5d));
	pr_info("0x8b5e, 0x%08x\r\n", rd(0x8b5e));
	pr_info("0x8b5f, 0x%08x\r\n", rd(0x8b5f));
	pr_info("0x8b60, 0x%08x\r\n", rd(0x8b60));
	pr_info("0x8b61, 0x%08x\r\n", rd(0x8b61));
	pr_info("0x8b62, 0x%08x\r\n", rd(0x8b62));
	pr_info("0x8b63, 0x%08x\r\n", rd(0x8b63));
	pr_info("0x8b64, 0x%08x\r\n", rd(0x8b64));
	pr_info("0x8b65, 0x%08x\r\n", rd(0x8b65));
	pr_info("0x8b66, 0x%08x\r\n", rd(0x8b66));
	pr_info("0x8b67, 0x%08x\r\n", rd(0x8b67));
	pr_info("0x8b68, 0x%08x\r\n", rd(0x8b68));
	pr_info("0x8b69, 0x%08x\r\n", rd(0x8b69));
	pr_info("0x8b6a, 0x%08x\r\n", rd(0x8b6a));
	pr_info("0x8b6b, 0x%08x\r\n", rd(0x8b6b));
	pr_info("0x8b6c, 0x%08x\r\n", rd(0x8b6c));
	pr_info("0x8b6d, 0x%08x\r\n", rd(0x8b6d));
	pr_info("0x8b6e, 0x%08x\r\n", rd(0x8b6e));
	pr_info("0x8b6f, 0x%08x\r\n", rd(0x8b6f));
	pr_info("0x8b70, 0x%08x\r\n", rd(0x8b70));
	pr_info("0x8b71, 0x%08x\r\n", rd(0x8b71));
	pr_info("0x8b72, 0x%08x\r\n", rd(0x8b72));
	pr_info("0x8b73, 0x%08x\r\n", rd(0x8b73));
	pr_info("0x8b74, 0x%08x\r\n", rd(0x8b74));
	pr_info("0x8b75, 0x%08x\r\n", rd(0x8b75));
	pr_info("0x8b76, 0x%08x\r\n", rd(0x8b76));
	pr_info("0x8b77, 0x%08x\r\n", rd(0x8b77));
	pr_info("0x8b78, 0x%08x\r\n", rd(0x8b78));
	pr_info("0x8b79, 0x%08x\r\n", rd(0x8b79));
	pr_info("0x8b7a, 0x%08x\r\n", rd(0x8b7a));
	pr_info("0x8b7b, 0x%08x\r\n", rd(0x8b7b));
	pr_info("0x8b7c, 0x%08x\r\n", rd(0x8b7c));
	pr_info("0x8b7d, 0x%08x\r\n", rd(0x8b7d));
	pr_info("0x8b7e, 0x%08x\r\n", rd(0x8b7e));
	pr_info("0x8b7f, 0x%08x\r\n", rd(0x8b7f));
	pr_info("0x8b80, 0x%08x\r\n", rd(0x8b80));
	pr_info("0x8b81, 0x%08x\r\n", rd(0x8b81));
	pr_info("0x8b82, 0x%08x\r\n", rd(0x8b82));
	pr_info("0x8b83, 0x%08x\r\n", rd(0x8b83));
	pr_info("0x8b84, 0x%08x\r\n", rd(0x8b84));
	pr_info("0x8b85, 0x%08x\r\n", rd(0x8b85));
	pr_info("0x8b86, 0x%08x\r\n", rd(0x8b86));
	pr_info("0x8b87, 0x%08x\r\n", rd(0x8b87));
	pr_info("0x8b88, 0x%08x\r\n", rd(0x8b88));
	pr_info("0x8b89, 0x%08x\r\n", rd(0x8b89));
	pr_info("0x8b8a, 0x%08x\r\n", rd(0x8b8a));
	pr_info("0x8b8b, 0x%08x\r\n", rd(0x8b8b));
	pr_info("0x8b8c, 0x%08x\r\n", rd(0x8b8c));
	pr_info("0x8b8d, 0x%08x\r\n", rd(0x8b8d));
	pr_info("0x8b8e, 0x%08x\r\n", rd(0x8b8e));
	pr_info("0x8b8f, 0x%08x\r\n", rd(0x8b8f));
	pr_info("---------dump iplogo regs tab end----------\n");
}

void dump_me_logo_regs_apb(void)
{
	pr_info("---------dump me logo regs tab----------\n");
	pr_info("0xb000, 0x%08x\r\n", rd(0xb000));
	pr_info("0xb001, 0x%08x\r\n", rd(0xb001));
	pr_info("0xb002, 0x%08x\r\n", rd(0xb002));
	pr_info("0xb003, 0x%08x\r\n", rd(0xb003));
	pr_info("0xb004, 0x%08x\r\n", rd(0xb004));
	pr_info("0xb005, 0x%08x\r\n", rd(0xb005));
	pr_info("0xb006, 0x%08x\r\n", rd(0xb006));
	pr_info("0xb007, 0x%08x\r\n", rd(0xb007));
	pr_info("0xb008, 0x%08x\r\n", rd(0xb008));
	pr_info("0xb009, 0x%08x\r\n", rd(0xb009));
	pr_info("0xb00a, 0x%08x\r\n", rd(0xb00a));
	pr_info("0xb00b, 0x%08x\r\n", rd(0xb00b));
	pr_info("0xb00c, 0x%08x\r\n", rd(0xb00c));
	pr_info("0xb00d, 0x%08x\r\n", rd(0xb00d));
	pr_info("0xb00e, 0x%08x\r\n", rd(0xb00e));
	pr_info("0xb00f, 0x%08x\r\n", rd(0xb00f));
	pr_info("0xb010, 0x%08x\r\n", rd(0xb010));
	pr_info("0xb011, 0x%08x\r\n", rd(0xb011));
	pr_info("0xb012, 0x%08x\r\n", rd(0xb012));
	pr_info("0xb013, 0x%08x\r\n", rd(0xb013));
	pr_info("0xb014, 0x%08x\r\n", rd(0xb014));
	pr_info("0xb015, 0x%08x\r\n", rd(0xb015));
	pr_info("0xb016, 0x%08x\r\n", rd(0xb016));
	pr_info("0xb017, 0x%08x\r\n", rd(0xb017));
	pr_info("---------dump me logo regs tab----------\n");
}

void dump_param_vp_init_apb(void)
{
	pr_info("---------dump vp init apb tab----------\n");
	pr_info("0xaf00, 0x%08x\r\n", rd(0xaf00));
	pr_info("0xaf01, 0x%08x\r\n", rd(0xaf01));
	pr_info("0xaf02, 0x%08x\r\n", rd(0xaf02));
	pr_info("0xaf03, 0x%08x\r\n", rd(0xaf03));
	pr_info("0xaf04, 0x%08x\r\n", rd(0xaf04));
	pr_info("0xaf05, 0x%08x\r\n", rd(0xaf05));
	pr_info("0xaf06, 0x%08x\r\n", rd(0xaf06));
	pr_info("0xaf07, 0x%08x\r\n", rd(0xaf07));
	pr_info("0xaf08, 0x%08x\r\n", rd(0xaf08));
	pr_info("0xaf1c, 0x%08x\r\n", rd(0xaf1c));
	pr_info("0xaf1d, 0x%08x\r\n", rd(0xaf1d));
	pr_info("0xaf1e, 0x%08x\r\n", rd(0xaf1e));
	pr_info("0xaf1f, 0x%08x\r\n", rd(0xaf1f));
	pr_info("0xaf20, 0x%08x\r\n", rd(0xaf20));
	pr_info("0xaf21, 0x%08x\r\n", rd(0xaf21));
	pr_info("0xaf22, 0x%08x\r\n", rd(0xaf22));
	pr_info("0xaf23, 0x%08x\r\n", rd(0xaf23));
	pr_info("0xaf24, 0x%08x\r\n", rd(0xaf24));
	pr_info("0xaf25, 0x%08x\r\n", rd(0xaf25));
	pr_info("0xaf26, 0x%08x\r\n", rd(0xaf26));
	pr_info("0xaf27, 0x%08x\r\n", rd(0xaf27));
	pr_info("0xaf28, 0x%08x\r\n", rd(0xaf28));
	pr_info("0xaf29, 0x%08x\r\n", rd(0xaf29));
	pr_info("0xaf2a, 0x%08x\r\n", rd(0xaf2a));
	pr_info("0xaf2b, 0x%08x\r\n", rd(0xaf2b));
	pr_info("0xaf2c, 0x%08x\r\n", rd(0xaf2c));
	pr_info("0xaf2d, 0x%08x\r\n", rd(0xaf2d));
	pr_info("0xaf2e, 0x%08x\r\n", rd(0xaf2e));
	pr_info("0xaf2f, 0x%08x\r\n", rd(0xaf2f));
	pr_info("0xaf30, 0x%08x\r\n", rd(0xaf30));
	pr_info("0xaf31, 0x%08x\r\n", rd(0xaf31));
	pr_info("0xaf32, 0x%08x\r\n", rd(0xaf32));
	pr_info("0xaf33, 0x%08x\r\n", rd(0xaf33));
	pr_info("0xaf34, 0x%08x\r\n", rd(0xaf34));
	pr_info("0xaf35, 0x%08x\r\n", rd(0xaf35));
	pr_info("0xaf36, 0x%08x\r\n", rd(0xaf36));
	pr_info("0xaf37, 0x%08x\r\n", rd(0xaf37));
	pr_info("0xaf38, 0x%08x\r\n", rd(0xaf38));
	pr_info("0xaf39, 0x%08x\r\n", rd(0xaf39));
	pr_info("0xaf3a, 0x%08x\r\n", rd(0xaf3a));
	pr_info("0xaf3b, 0x%08x\r\n", rd(0xaf3b));
	pr_info("0xaf3c, 0x%08x\r\n", rd(0xaf3c));
	pr_info("0xaf3d, 0x%08x\r\n", rd(0xaf3d));
	pr_info("0xaf3e, 0x%08x\r\n", rd(0xaf3e));
	pr_info("0xaf3f, 0x%08x\r\n", rd(0xaf3f));
	pr_info("0xaf40, 0x%08x\r\n", rd(0xaf40));
	pr_info("0xaf41, 0x%08x\r\n", rd(0xaf41));
	pr_info("0xaf42, 0x%08x\r\n", rd(0xaf42));
	pr_info("0xaf43, 0x%08x\r\n", rd(0xaf43));
	pr_info("0xaf44, 0x%08x\r\n", rd(0xaf44));
	pr_info("0xaf45, 0x%08x\r\n", rd(0xaf45));
	pr_info("0xaf46, 0x%08x\r\n", rd(0xaf46));
	pr_info("0xaf47, 0x%08x\r\n", rd(0xaf47));
	pr_info("0xaf48, 0x%08x\r\n", rd(0xaf48));
	pr_info("0xaf49, 0x%08x\r\n", rd(0xaf49));
	pr_info("0xaf4a, 0x%08x\r\n", rd(0xaf4a));
	pr_info("0xaf4b, 0x%08x\r\n", rd(0xaf4b));
	pr_info("0xaf4c, 0x%08x\r\n", rd(0xaf4c));
	pr_info("0xaf4d, 0x%08x\r\n", rd(0xaf4d));
	pr_info("0xaf4e, 0x%08x\r\n", rd(0xaf4e));
	pr_info("0xaf4f, 0x%08x\r\n", rd(0xaf4f));
	pr_info("0xaf50, 0x%08x\r\n", rd(0xaf50));
	pr_info("0xaf51, 0x%08x\r\n", rd(0xaf51));
	pr_info("0xaf52, 0x%08x\r\n", rd(0xaf52));
	pr_info("0xaf53, 0x%08x\r\n", rd(0xaf53));
	pr_info("0xaf54, 0x%08x\r\n", rd(0xaf54));
	pr_info("0xaf55, 0x%08x\r\n", rd(0xaf55));
	pr_info("0xaf56, 0x%08x\r\n", rd(0xaf56));
	pr_info("0xaf57, 0x%08x\r\n", rd(0xaf57));
	pr_info("0xaf58, 0x%08x\r\n", rd(0xaf58));
	pr_info("0xaf59, 0x%08x\r\n", rd(0xaf59));
	pr_info("0xaf5a, 0x%08x\r\n", rd(0xaf5a));
	pr_info("0xaf5b, 0x%08x\r\n", rd(0xaf5b));
	pr_info("0xaf93, 0x%08x\r\n", rd(0xaf93));
	pr_info("0xaf94, 0x%08x\r\n", rd(0xaf94));
	pr_info("0xaf95, 0x%08x\r\n", rd(0xaf95));
	pr_info("0xaf96, 0x%08x\r\n", rd(0xaf96));
	pr_info("0xaf97, 0x%08x\r\n", rd(0xaf97));
	pr_info("0xaf98, 0x%08x\r\n", rd(0xaf98));
	pr_info("---------dump vp init apb tab end----------\n");
}

void dump_param_mc_init_apb(void)
{
	pr_info("---------dump mc init apb tab----------\n");
	pr_info("0xe400, 0x%08x\r\n", rd(0xe400));
	pr_info("0xe401, 0x%08x\r\n", rd(0xe401));
	pr_info("0xe402, 0x%08x\r\n", rd(0xe402));
	pr_info("0xe403, 0x%08x\r\n", rd(0xe403));
	pr_info("0xe404, 0x%08x\r\n", rd(0xe404));
	pr_info("0xe405, 0x%08x\r\n", rd(0xe405));
	pr_info("0xe406, 0x%08x\r\n", rd(0xe406));
	pr_info("0xe407, 0x%08x\r\n", rd(0xe407));
	pr_info("0xe408, 0x%08x\r\n", rd(0xe408));
	pr_info("0xe409, 0x%08x\r\n", rd(0xe409));
	pr_info("0xe40a, 0x%08x\r\n", rd(0xe40a));
	pr_info("0xe40b, 0x%08x\r\n", rd(0xe40b));
	pr_info("0xe40c, 0x%08x\r\n", rd(0xe40c));
	pr_info("0xe40d, 0x%08x\r\n", rd(0xe40d));
	pr_info("0xe40e, 0x%08x\r\n", rd(0xe40e));
	pr_info("0xe410, 0x%08x\r\n", rd(0xe410));
	pr_info("0xe411, 0x%08x\r\n", rd(0xe411));
	pr_info("0xe412, 0x%08x\r\n", rd(0xe412));
	pr_info("0xe413, 0x%08x\r\n", rd(0xe413));
	pr_info("0xe414, 0x%08x\r\n", rd(0xe414));
	pr_info("0xe415, 0x%08x\r\n", rd(0xe415));
	pr_info("0xe416, 0x%08x\r\n", rd(0xe416));
	pr_info("0xe417, 0x%08x\r\n", rd(0xe417));
	pr_info("0xe418, 0x%08x\r\n", rd(0xe418));
	pr_info("0xe419, 0x%08x\r\n", rd(0xe419));
	pr_info("0xe41a, 0x%08x\r\n", rd(0xe41a));
	pr_info("0xe41b, 0x%08x\r\n", rd(0xe41b));
	pr_info("0xe41c, 0x%08x\r\n", rd(0xe41c));
	pr_info("0xe41d, 0x%08x\r\n", rd(0xe41d));
	pr_info("0xe41e, 0x%08x\r\n", rd(0xe41e));
	pr_info("0xe41f, 0x%08x\r\n", rd(0xe41f));
	pr_info("0xe420, 0x%08x\r\n", rd(0xe420));
	pr_info("0xe421, 0x%08x\r\n", rd(0xe421));
	pr_info("0xe422, 0x%08x\r\n", rd(0xe422));
	pr_info("0xe423, 0x%08x\r\n", rd(0xe423));
	pr_info("0xe424, 0x%08x\r\n", rd(0xe424));
	pr_info("0xe425, 0x%08x\r\n", rd(0xe425));
	pr_info("0xe426, 0x%08x\r\n", rd(0xe426));
	pr_info("0xe427, 0x%08x\r\n", rd(0xe427));
	pr_info("0xe428, 0x%08x\r\n", rd(0xe428));
	pr_info("0xe429, 0x%08x\r\n", rd(0xe429));
	pr_info("0xe42a, 0x%08x\r\n", rd(0xe42a));
	pr_info("0xe42b, 0x%08x\r\n", rd(0xe42b));
	pr_info("0xe42c, 0x%08x\r\n", rd(0xe42c));
	pr_info("0xe42d, 0x%08x\r\n", rd(0xe42d));
	pr_info("0xe42e, 0x%08x\r\n", rd(0xe42e));
	pr_info("0xe42f, 0x%08x\r\n", rd(0xe42f));
	pr_info("0xe430, 0x%08x\r\n", rd(0xe430));
	pr_info("0xe431, 0x%08x\r\n", rd(0xe431));
	pr_info("0xe432, 0x%08x\r\n", rd(0xe432));
	pr_info("0xe433, 0x%08x\r\n", rd(0xe433));
	pr_info("0xe434, 0x%08x\r\n", rd(0xe434));
	pr_info("0xe435, 0x%08x\r\n", rd(0xe435));
	pr_info("0xe436, 0x%08x\r\n", rd(0xe436));
	pr_info("0xe437, 0x%08x\r\n", rd(0xe437));
	pr_info("0xe438, 0x%08x\r\n", rd(0xe438));
	pr_info("0xe439, 0x%08x\r\n", rd(0xe439));
	pr_info("0xe43a, 0x%08x\r\n", rd(0xe43a));
	pr_info("0xe43b, 0x%08x\r\n", rd(0xe43b));
	pr_info("0xe43c, 0x%08x\r\n", rd(0xe43c));
	pr_info("0xe43d, 0x%08x\r\n", rd(0xe43d));
	pr_info("0xe43e, 0x%08x\r\n", rd(0xe43e));
	pr_info("0xe43f, 0x%08x\r\n", rd(0xe43f));
	pr_info("0xe440, 0x%08x\r\n", rd(0xe440));
	pr_info("0xe441, 0x%08x\r\n", rd(0xe441));
	pr_info("0xe442, 0x%08x\r\n", rd(0xe442));
	pr_info("0xe443, 0x%08x\r\n", rd(0xe443));
	pr_info("0xe444, 0x%08x\r\n", rd(0xe444));
	pr_info("0xe445, 0x%08x\r\n", rd(0xe445));
	pr_info("0xe446, 0x%08x\r\n", rd(0xe446));
	pr_info("0xe447, 0x%08x\r\n", rd(0xe447));
	pr_info("0xe448, 0x%08x\r\n", rd(0xe448));
	pr_info("0xe449, 0x%08x\r\n", rd(0xe449));
	pr_info("0xe44a, 0x%08x\r\n", rd(0xe44a));
	pr_info("0xe44b, 0x%08x\r\n", rd(0xe44b));
	pr_info("0xe44c, 0x%08x\r\n", rd(0xe44c));
	pr_info("0xe44d, 0x%08x\r\n", rd(0xe44d));
	pr_info("0xe44e, 0x%08x\r\n", rd(0xe44e));
	pr_info("0xe44f, 0x%08x\r\n", rd(0xe44f));
	pr_info("0xe450, 0x%08x\r\n", rd(0xe450));
	pr_info("0xe451, 0x%08x\r\n", rd(0xe451));
	pr_info("0xe452, 0x%08x\r\n", rd(0xe452));
	pr_info("0xe453, 0x%08x\r\n", rd(0xe453));
	pr_info("0xe454, 0x%08x\r\n", rd(0xe454));
	pr_info("0xe455, 0x%08x\r\n", rd(0xe455));
	pr_info("0xe456, 0x%08x\r\n", rd(0xe456));
	pr_info("0xe457, 0x%08x\r\n", rd(0xe457));
	pr_info("0xe458, 0x%08x\r\n", rd(0xe458));
	pr_info("0xe459, 0x%08x\r\n", rd(0xe459));
	pr_info("0xe45a, 0x%08x\r\n", rd(0xe45a));
	pr_info("0xe45b, 0x%08x\r\n", rd(0xe45b));
	pr_info("0xe45c, 0x%08x\r\n", rd(0xe45c));
	pr_info("0xe45d, 0x%08x\r\n", rd(0xe45d));
	pr_info("0xe45e, 0x%08x\r\n", rd(0xe45e));
	pr_info("0xe45f, 0x%08x\r\n", rd(0xe45f));
	pr_info("0xe460, 0x%08x\r\n", rd(0xe460));
	pr_info("0xe461, 0x%08x\r\n", rd(0xe461));
	pr_info("0xe462, 0x%08x\r\n", rd(0xe462));
	pr_info("0xe463, 0x%08x\r\n", rd(0xe463));
	pr_info("0xe464, 0x%08x\r\n", rd(0xe464));
	pr_info("0xe465, 0x%08x\r\n", rd(0xe465));
	pr_info("0xe466, 0x%08x\r\n", rd(0xe466));
	pr_info("0xe467, 0x%08x\r\n", rd(0xe467));
	pr_info("0xe468, 0x%08x\r\n", rd(0xe468));
	pr_info("0xe469, 0x%08x\r\n", rd(0xe469));
	pr_info("0xe46a, 0x%08x\r\n", rd(0xe46a));
	pr_info("0xe46b, 0x%08x\r\n", rd(0xe46b));
	pr_info("0xe46c, 0x%08x\r\n", rd(0xe46c));
	pr_info("0xe46d, 0x%08x\r\n", rd(0xe46d));
	pr_info("0xe46e, 0x%08x\r\n", rd(0xe46e));
	pr_info("0xe46f, 0x%08x\r\n", rd(0xe46f));
	pr_info("0xe470, 0x%08x\r\n", rd(0xe470));
	pr_info("0xe471, 0x%08x\r\n", rd(0xe471));
	pr_info("0xe472, 0x%08x\r\n", rd(0xe472));
	pr_info("0xe473, 0x%08x\r\n", rd(0xe473));
	pr_info("0xe474, 0x%08x\r\n", rd(0xe474));
	pr_info("0xe475, 0x%08x\r\n", rd(0xe475));
	pr_info("0xe476, 0x%08x\r\n", rd(0xe476));
	pr_info("0xe477, 0x%08x\r\n", rd(0xe477));
	pr_info("0xe478, 0x%08x\r\n", rd(0xe478));
	pr_info("0xe479, 0x%08x\r\n", rd(0xe479));
	pr_info("0xe47a, 0x%08x\r\n", rd(0xe47a));
	pr_info("0xe47b, 0x%08x\r\n", rd(0xe47b));
	pr_info("0xe47c, 0x%08x\r\n", rd(0xe47c));
	pr_info("0xe47d, 0x%08x\r\n", rd(0xe47d));
	pr_info("0xe47e, 0x%08x\r\n", rd(0xe47e));
	pr_info("0xe47f, 0x%08x\r\n", rd(0xe47f));
	pr_info("0xe480, 0x%08x\r\n", rd(0xe480));
	pr_info("0xe481, 0x%08x\r\n", rd(0xe481));
	pr_info("0xe482, 0x%08x\r\n", rd(0xe482));
	pr_info("0xe483, 0x%08x\r\n", rd(0xe483));
	pr_info("0xe484, 0x%08x\r\n", rd(0xe484));
	pr_info("0xe485, 0x%08x\r\n", rd(0xe485));
	pr_info("0xe486, 0x%08x\r\n", rd(0xe486));
	pr_info("0xe487, 0x%08x\r\n", rd(0xe487));
	pr_info("0xe488, 0x%08x\r\n", rd(0xe488));
	pr_info("0xe489, 0x%08x\r\n", rd(0xe489));
	pr_info("0xe48a, 0x%08x\r\n", rd(0xe48a));
	pr_info("0xe48b, 0x%08x\r\n", rd(0xe48b));
	pr_info("0xe48c, 0x%08x\r\n", rd(0xe48c));
	pr_info("0xe48d, 0x%08x\r\n", rd(0xe48d));
	pr_info("0xe48e, 0x%08x\r\n", rd(0xe48e));
	pr_info("0xe48f, 0x%08x\r\n", rd(0xe48f));
	pr_info("0xe490, 0x%08x\r\n", rd(0xe490));
	pr_info("0xe491, 0x%08x\r\n", rd(0xe491));
	pr_info("0xe492, 0x%08x\r\n", rd(0xe492));
	pr_info("0xe493, 0x%08x\r\n", rd(0xe493));
	pr_info("0xe494, 0x%08x\r\n", rd(0xe494));
	pr_info("0xe495, 0x%08x\r\n", rd(0xe495));
	pr_info("0xe496, 0x%08x\r\n", rd(0xe496));
	pr_info("0xe497, 0x%08x\r\n", rd(0xe497));
	pr_info("0xe498, 0x%08x\r\n", rd(0xe498));
	pr_info("0xe49a, 0x%08x\r\n", rd(0xe49a));
	pr_info("0xe49b, 0x%08x\r\n", rd(0xe49b));
	pr_info("0xe40f, 0x%08x\r\n", rd(0xe40f));
	pr_info("0xe499, 0x%08x\r\n", rd(0xe499));
	pr_info("0xe49c, 0x%08x\r\n", rd(0xe49c));
	pr_info("0xe49d, 0x%08x\r\n", rd(0xe49d));
	pr_info("0xe49e, 0x%08x\r\n", rd(0xe49e));
	pr_info("0xe49f, 0x%08x\r\n", rd(0xe49f));
	pr_info("0xe4a0, 0x%08x\r\n", rd(0xe4a0));
	pr_info("0xe4a1, 0x%08x\r\n", rd(0xe4a1));
	pr_info("0xe4a2, 0x%08x\r\n", rd(0xe4a2));
	pr_info("0xe4a3, 0x%08x\r\n", rd(0xe4a3));
	pr_info("0xe4a4, 0x%08x\r\n", rd(0xe4a4));
	pr_info("0xe4a5, 0x%08x\r\n", rd(0xe4a5));
	pr_info("0xe4a6, 0x%08x\r\n", rd(0xe4a6));
	pr_info("0xe4a7, 0x%08x\r\n", rd(0xe4a7));
	pr_info("0xe4a8, 0x%08x\r\n", rd(0xe4a8));
	pr_info("0xe4a9, 0x%08x\r\n", rd(0xe4a9));
	pr_info("0xe4aa, 0x%08x\r\n", rd(0xe4aa));
	pr_info("0xe4ab, 0x%08x\r\n", rd(0xe4ab));
	pr_info("0xe4ac, 0x%08x\r\n", rd(0xe4ac));
	pr_info("0xe4ad, 0x%08x\r\n", rd(0xe4ad));
	pr_info("0xe4ae, 0x%08x\r\n", rd(0xe4ae));
	pr_info("0xe4af, 0x%08x\r\n", rd(0xe4af));
	pr_info("0xe4b0, 0x%08x\r\n", rd(0xe4b0));
	pr_info("0xe4b1, 0x%08x\r\n", rd(0xe4b1));
	pr_info("0xe4b2, 0x%08x\r\n", rd(0xe4b2));
	pr_info("0xe4b3, 0x%08x\r\n", rd(0xe4b3));
	pr_info("0xe4b4, 0x%08x\r\n", rd(0xe4b4));
	pr_info("0xe4b5, 0x%08x\r\n", rd(0xe4b5));
	pr_info("0xe4b6, 0x%08x\r\n", rd(0xe4b6));
	pr_info("0xe4b7, 0x%08x\r\n", rd(0xe4b7));
	pr_info("0xe4b8, 0x%08x\r\n", rd(0xe4b8));
	pr_info("0xe4b9, 0x%08x\r\n", rd(0xe4b9));
	pr_info("0xe4ba, 0x%08x\r\n", rd(0xe4ba));
	pr_info("0xe4bb, 0x%08x\r\n", rd(0xe4bb));
	pr_info("0xe4bc, 0x%08x\r\n", rd(0xe4bc));
	pr_info("0xe4bd, 0x%08x\r\n", rd(0xe4bd));
	pr_info("0xe4be, 0x%08x\r\n", rd(0xe4be));
	pr_info("0xe4bf, 0x%08x\r\n", rd(0xe4bf));
	pr_info("0xe4c0, 0x%08x\r\n", rd(0xe4c0));
	pr_info("0xe4c1, 0x%08x\r\n", rd(0xe4c1));
	pr_info("0xe4c2, 0x%08x\r\n", rd(0xe4c2));
	pr_info("0xe4c3, 0x%08x\r\n", rd(0xe4c3));
	pr_info("0xe4c4, 0x%08x\r\n", rd(0xe4c4));
	pr_info("0xe4c5, 0x%08x\r\n", rd(0xe4c5));
	pr_info("0xe4c6, 0x%08x\r\n", rd(0xe4c6));
	pr_info("0xe4c7, 0x%08x\r\n", rd(0xe4c7));
	pr_info("0xe4c8, 0x%08x\r\n", rd(0xe4c8));
	pr_info("0xe4c9, 0x%08x\r\n", rd(0xe4c9));
	pr_info("0xe4ca, 0x%08x\r\n", rd(0xe4ca));
	pr_info("0xe4cb, 0x%08x\r\n", rd(0xe4cb));
	pr_info("0xe4cc, 0x%08x\r\n", rd(0xe4cc));
	pr_info("0xe4cd, 0x%08x\r\n", rd(0xe4cd));
	pr_info("0xe4ce, 0x%08x\r\n", rd(0xe4ce));
	pr_info("0xe4cf, 0x%08x\r\n", rd(0xe4cf));
	pr_info("0xe4d0, 0x%08x\r\n", rd(0xe4d0));
	pr_info("0xe4d1, 0x%08x\r\n", rd(0xe4d1));
	pr_info("0xe4d2, 0x%08x\r\n", rd(0xe4d2));
	pr_info("0xe4d3, 0x%08x\r\n", rd(0xe4d3));
	pr_info("0xe4d4, 0x%08x\r\n", rd(0xe4d4));
	pr_info("0xe4d5, 0x%08x\r\n", rd(0xe4d5));
	pr_info("0xe4d6, 0x%08x\r\n", rd(0xe4d6));
	pr_info("0xe4d7, 0x%08x\r\n", rd(0xe4d7));
	pr_info("0xe4d8, 0x%08x\r\n", rd(0xe4d8));
	pr_info("0xe4d9, 0x%08x\r\n", rd(0xe4d9));
	pr_info("0xe4da, 0x%08x\r\n", rd(0xe4da));
	pr_info("0xe4db, 0x%08x\r\n", rd(0xe4db));
	pr_info("0xe4dc, 0x%08x\r\n", rd(0xe4dc));
	pr_info("0xe4dd, 0x%08x\r\n", rd(0xe4dd));
	pr_info("0xe4de, 0x%08x\r\n", rd(0xe4de));
	pr_info("0xe4df, 0x%08x\r\n", rd(0xe4df));
	pr_info("0xe4e0, 0x%08x\r\n", rd(0xe4e0));
	pr_info("0xe4e1, 0x%08x\r\n", rd(0xe4e1));
	pr_info("0xe4e2, 0x%08x\r\n", rd(0xe4e2));
	pr_info("0xe4e3, 0x%08x\r\n", rd(0xe4e3));
	pr_info("0xe4e4, 0x%08x\r\n", rd(0xe4e4));
	pr_info("0xe4e5, 0x%08x\r\n", rd(0xe4e5));
	pr_info("0xe4e6, 0x%08x\r\n", rd(0xe4e6));
	pr_info("0xe4e7, 0x%08x\r\n", rd(0xe4e7));
	pr_info("0xe4e8, 0x%08x\r\n", rd(0xe4e8));
	pr_info("0xe4e9, 0x%08x\r\n", rd(0xe4e9));
	pr_info("0xe4ea, 0x%08x\r\n", rd(0xe4ea));
	pr_info("0xe4eb, 0x%08x\r\n", rd(0xe4eb));
	pr_info("0xe4ec, 0x%08x\r\n", rd(0xe4ec));
	pr_info("0xe4ed, 0x%08x\r\n", rd(0xe4ed));
	pr_info("0xe4ee, 0x%08x\r\n", rd(0xe4ee));
	pr_info("0xe4ef, 0x%08x\r\n", rd(0xe4ef));
	pr_info("0xe4f0, 0x%08x\r\n", rd(0xe4f0));
	pr_info("0xe4f1, 0x%08x\r\n", rd(0xe4f1));
	pr_info("0xe4f2, 0x%08x\r\n", rd(0xe4f2));
	pr_info("0xe4f3, 0x%08x\r\n", rd(0xe4f3));
	pr_info("0xe4f4, 0x%08x\r\n", rd(0xe4f4));
	pr_info("0xe4f5, 0x%08x\r\n", rd(0xe4f5));
	pr_info("0xe4f6, 0x%08x\r\n", rd(0xe4f6));
	pr_info("0xe4f7, 0x%08x\r\n", rd(0xe4f7));
	pr_info("0xe4f8, 0x%08x\r\n", rd(0xe4f8));
	pr_info("---------dump mc init apb tab end----------\n");
}

void dump_me_regs_apb(void)
{
	pr_info("---------dump me regs apb tab----------\n");
	pr_info("0xa100, 0x%08x\r\n", rd(0xa100));
	pr_info("0xa101, 0x%08x\r\n", rd(0xa101));
	pr_info("0xa102, 0x%08x\r\n", rd(0xa102));
	pr_info("0xa103, 0x%08x\r\n", rd(0xa103));
	pr_info("0xa104, 0x%08x\r\n", rd(0xa104));
	pr_info("0xa105, 0x%08x\r\n", rd(0xa105));
	pr_info("0xa106, 0x%08x\r\n", rd(0xa106));
	pr_info("0xa107, 0x%08x\r\n", rd(0xa107));
	pr_info("0xa108, 0x%08x\r\n", rd(0xa108));
	pr_info("0xa109, 0x%08x\r\n", rd(0xa109));
	pr_info("0xa10a, 0x%08x\r\n", rd(0xa10a));
	pr_info("0xa10b, 0x%08x\r\n", rd(0xa10b));
	pr_info("0xa10c, 0x%08x\r\n", rd(0xa10c));
	pr_info("0xa10d, 0x%08x\r\n", rd(0xa10d));
	pr_info("0xa10e, 0x%08x\r\n", rd(0xa10e));
	pr_info("0xa10f, 0x%08x\r\n", rd(0xa10f));
	pr_info("0xa110, 0x%08x\r\n", rd(0xa110));
	pr_info("0xa111, 0x%08x\r\n", rd(0xa111));
	pr_info("0xa112, 0x%08x\r\n", rd(0xa112));
	pr_info("0xa113, 0x%08x\r\n", rd(0xa113));
	pr_info("0xa114, 0x%08x\r\n", rd(0xa114));
	pr_info("0xa115, 0x%08x\r\n", rd(0xa115));
	pr_info("0xa116, 0x%08x\r\n", rd(0xa116));
	pr_info("0xa117, 0x%08x\r\n", rd(0xa117));
	pr_info("0xa118, 0x%08x\r\n", rd(0xa118));
	pr_info("0xa119, 0x%08x\r\n", rd(0xa119));
	pr_info("0xa11a, 0x%08x\r\n", rd(0xa11a));
	pr_info("0xa11b, 0x%08x\r\n", rd(0xa11b));
	pr_info("0xa11c, 0x%08x\r\n", rd(0xa11c));
	pr_info("0xa11d, 0x%08x\r\n", rd(0xa11d));
	pr_info("0xa11e, 0x%08x\r\n", rd(0xa11e));
	pr_info("0xa11f, 0x%08x\r\n", rd(0xa11f));
	pr_info("0xa120, 0x%08x\r\n", rd(0xa120));
	pr_info("0xa121, 0x%08x\r\n", rd(0xa121));
	pr_info("0xa122, 0x%08x\r\n", rd(0xa122));
	pr_info("0xa123, 0x%08x\r\n", rd(0xa123));
	pr_info("0xa124, 0x%08x\r\n", rd(0xa124));
	pr_info("0xa125, 0x%08x\r\n", rd(0xa125));
	pr_info("0xa126, 0x%08x\r\n", rd(0xa126));
	pr_info("0xa127, 0x%08x\r\n", rd(0xa127));
	pr_info("0xa128, 0x%08x\r\n", rd(0xa128));
	pr_info("0xa129, 0x%08x\r\n", rd(0xa129));
	pr_info("0xa12a, 0x%08x\r\n", rd(0xa12a));
	pr_info("0xa12b, 0x%08x\r\n", rd(0xa12b));
	pr_info("0xa12c, 0x%08x\r\n", rd(0xa12c));
	pr_info("0xa12d, 0x%08x\r\n", rd(0xa12d));
	pr_info("0xa12e, 0x%08x\r\n", rd(0xa12e));
	pr_info("0xa12f, 0x%08x\r\n", rd(0xa12f));
	pr_info("0xa130, 0x%08x\r\n", rd(0xa130));
	pr_info("0xa131, 0x%08x\r\n", rd(0xa131));
	pr_info("0xa132, 0x%08x\r\n", rd(0xa132));
	pr_info("0xa133, 0x%08x\r\n", rd(0xa133));
	pr_info("0xa134, 0x%08x\r\n", rd(0xa134));
	pr_info("0xa135, 0x%08x\r\n", rd(0xa135));
	pr_info("0xa136, 0x%08x\r\n", rd(0xa136));
	pr_info("0xa137, 0x%08x\r\n", rd(0xa137));
	pr_info("0xa138, 0x%08x\r\n", rd(0xa138));
	pr_info("0xa139, 0x%08x\r\n", rd(0xa139));
	pr_info("0xa13a, 0x%08x\r\n", rd(0xa13a));
	pr_info("0xa13b, 0x%08x\r\n", rd(0xa13b));
	pr_info("0xa13c, 0x%08x\r\n", rd(0xa13c));
	pr_info("0xa13d, 0x%08x\r\n", rd(0xa13d));
	pr_info("0xa13e, 0x%08x\r\n", rd(0xa13e));
	pr_info("0xa13f, 0x%08x\r\n", rd(0xa13f));
	pr_info("0xa140, 0x%08x\r\n", rd(0xa140));
	pr_info("0xa141, 0x%08x\r\n", rd(0xa141));
	pr_info("0xa142, 0x%08x\r\n", rd(0xa142));
	pr_info("0xa143, 0x%08x\r\n", rd(0xa143));
	pr_info("0xa144, 0x%08x\r\n", rd(0xa144));
	pr_info("0xa145, 0x%08x\r\n", rd(0xa145));
	pr_info("0xa146, 0x%08x\r\n", rd(0xa146));
	pr_info("0xa147, 0x%08x\r\n", rd(0xa147));
	pr_info("0xa148, 0x%08x\r\n", rd(0xa148));
	pr_info("0xa149, 0x%08x\r\n", rd(0xa149));
	pr_info("0xa14a, 0x%08x\r\n", rd(0xa14a));
	pr_info("0xa14b, 0x%08x\r\n", rd(0xa14b));
	pr_info("0xa14c, 0x%08x\r\n", rd(0xa14c));
	pr_info("0xa14d, 0x%08x\r\n", rd(0xa14d));
	pr_info("0xa14e, 0x%08x\r\n", rd(0xa14e));
	pr_info("0xa14f, 0x%08x\r\n", rd(0xa14f));
	pr_info("0xa150, 0x%08x\r\n", rd(0xa150));
	pr_info("0xa151, 0x%08x\r\n", rd(0xa151));
	pr_info("0xa152, 0x%08x\r\n", rd(0xa152));
	pr_info("0xa153, 0x%08x\r\n", rd(0xa153));
	pr_info("0xa154, 0x%08x\r\n", rd(0xa154));
	pr_info("0xa155, 0x%08x\r\n", rd(0xa155));
	pr_info("0xa156, 0x%08x\r\n", rd(0xa156));
	pr_info("0xa157, 0x%08x\r\n", rd(0xa157));
	pr_info("0xa158, 0x%08x\r\n", rd(0xa158));
	pr_info("0xa159, 0x%08x\r\n", rd(0xa159));
	pr_info("0xa15a, 0x%08x\r\n", rd(0xa15a));
	pr_info("0xa15b, 0x%08x\r\n", rd(0xa15b));
	pr_info("0xa15c, 0x%08x\r\n", rd(0xa15c));
	pr_info("0xa15d, 0x%08x\r\n", rd(0xa15d));
	pr_info("0xa15e, 0x%08x\r\n", rd(0xa15e));
	pr_info("0xa15f, 0x%08x\r\n", rd(0xa15f));
	pr_info("0xa160, 0x%08x\r\n", rd(0xa160));
	pr_info("0xa161, 0x%08x\r\n", rd(0xa161));
	pr_info("0xa162, 0x%08x\r\n", rd(0xa162));
	pr_info("0xa163, 0x%08x\r\n", rd(0xa163));
	pr_info("0xa164, 0x%08x\r\n", rd(0xa164));
	pr_info("0xa165, 0x%08x\r\n", rd(0xa165));
	pr_info("0xa166, 0x%08x\r\n", rd(0xa166));
	pr_info("0xa167, 0x%08x\r\n", rd(0xa167));
	pr_info("0xa168, 0x%08x\r\n", rd(0xa168));
	pr_info("0xa169, 0x%08x\r\n", rd(0xa169));
	pr_info("0xa16a, 0x%08x\r\n", rd(0xa16a));
	pr_info("0xa16b, 0x%08x\r\n", rd(0xa16b));
	pr_info("0xa16c, 0x%08x\r\n", rd(0xa16c));
	pr_info("0xa16d, 0x%08x\r\n", rd(0xa16d));
	pr_info("0xa16e, 0x%08x\r\n", rd(0xa16e));
	pr_info("0xa16f, 0x%08x\r\n", rd(0xa16f));
	pr_info("0xa170, 0x%08x\r\n", rd(0xa170));
	pr_info("0xa171, 0x%08x\r\n", rd(0xa171));
	pr_info("0xa172, 0x%08x\r\n", rd(0xa172));
	pr_info("0xa173, 0x%08x\r\n", rd(0xa173));
	pr_info("0xa174, 0x%08x\r\n", rd(0xa174));
	pr_info("0xa175, 0x%08x\r\n", rd(0xa175));
	pr_info("0xa176, 0x%08x\r\n", rd(0xa176));
	pr_info("0xa177, 0x%08x\r\n", rd(0xa177));
	pr_info("0xa178, 0x%08x\r\n", rd(0xa178));
	pr_info("0xa179, 0x%08x\r\n", rd(0xa179));
	pr_info("0xa17a, 0x%08x\r\n", rd(0xa17a));
	pr_info("0xa17b, 0x%08x\r\n", rd(0xa17b));
	pr_info("0xa17c, 0x%08x\r\n", rd(0xa17c));
	pr_info("0xa17d, 0x%08x\r\n", rd(0xa17d));
	pr_info("0xa17e, 0x%08x\r\n", rd(0xa17e));
	pr_info("0xa17f, 0x%08x\r\n", rd(0xa17f));
	pr_info("---------dump me regs apb tab end----------\n");
}

void dump_vfcd_page(void)
{
	u32 reg;

	pr_info("vfcd mc0:");
	for (reg = 0x400 + RMIF_TOP_CTRL; reg <= 0x400 + RMIF_RO_STAT; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + RMIF_DUMMY_PIXEL; reg <= 0x400 + RMIF_CHRM_PSEL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_AFBC_ENABLE; reg <= 0x400 + VFCD_URGENT_CTRL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_AFBC_IQUANT_ENABLE; reg <= 0x400 + VFCD_AFBC_LOSS_CTRL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_MIF1_URGENT; reg <= 0x400 + VFCD_AFRC_CTRL1; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_FRM_PIC_SIZE; reg <= 0x400 + VFCD_SLC3_CHRM_PIC_XPOS; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_TOP_CTRL0; reg <= 0x400 + VFCD_MMU_CTRL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_DBG_CORE_0; reg <= 0x400 + VFCD_DBG_INFO_3; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_POST_LUMA_SLC1_WIN_H; reg <= 0x400 + 0x65e8; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_POST_LUMA_SIZE; reg <= 0x400 + VFCD_POST_LUMA_SLC3_SIZE; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x400 + VFCD_AFBC_VD_CFMT_CTRL; reg <= 0x400 + VFCD_PAD_DUMY_DATA; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	pr_info("vfcd %x = %#8x\n", 0x400 + VFCD_POST_RO_0, rd_vc(0x400 + VFCD_POST_RO_0));
	pr_info("vfcd %x = %#8x\n", 0x400 + VFCD_POST_RO_1, rd_vc(0x400 + VFCD_POST_RO_1));

	pr_info("vfcd mc1:");
	for (reg = 0x500 + RMIF_TOP_CTRL; reg <= 0x500 + RMIF_RO_STAT; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + RMIF_DUMMY_PIXEL; reg <= 0x500 + RMIF_CHRM_PSEL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_AFBC_ENABLE; reg <= 0x500 + VFCD_URGENT_CTRL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_AFBC_IQUANT_ENABLE; reg <= 0x500 + VFCD_AFBC_LOSS_CTRL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_MIF1_URGENT; reg <= 0x500 + VFCD_AFRC_CTRL1; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_FRM_PIC_SIZE; reg <= 0x500 + VFCD_SLC3_CHRM_PIC_XPOS; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_TOP_CTRL0; reg <= 0x500 + VFCD_MMU_CTRL; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_DBG_CORE_0; reg <= 0x500 + VFCD_DBG_INFO_3; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_POST_LUMA_SLC1_WIN_H; reg <= 0x500 + 0x65e8; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_POST_LUMA_SIZE; reg <= 0x500 + VFCD_POST_LUMA_SLC3_SIZE; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	for (reg = 0x500 + VFCD_AFBC_VD_CFMT_CTRL; reg <= 0x500 + VFCD_PAD_DUMY_DATA; reg++)
		pr_info("vfcd %x = %#8x\n", reg, rd_vc(reg));
	pr_info("vfcd %x = %#8x\n", 0x500 + VFCD_POST_RO_0, rd_vc(0x500 + VFCD_POST_RO_0));
	pr_info("vfcd %x = %#8x\n", 0x500 + VFCD_POST_RO_1, rd_vc(0x500 + VFCD_POST_RO_1));
}

