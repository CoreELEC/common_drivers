// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/errno.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vout_notify.h>

#include "../amvecm/arch/vpp_regs.h"
#include "../amvecm/arch/vpp_hdr_regs.h"
#include "../amvecm/arch/vpp_dolbyvision_regs.h"
#include "../amvecm/amcsc.h"
#include "../amvecm/reg_helper.h"
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/iomap.h>
#include "md_config.h"
#include <linux/of.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/arm-smccc.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include <linux/amlogic/media/lut_dma/lut_dma.h>

#include "amdv.h"
#include "amdv_hw5.h"
#include "amdv_hw5_t6w.h"
#include "amdv_regs_hw5_t6w.h"
#include "amdv_regs_hw5_t6x.h"
#include "mmu_config_t6w.h"
#if ENABLE_DPSS
#include <linux/amlogic/media/di/dpss_interface.h>
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif

static bool new_top1_toggle;
static int last_int_top2 = 0x10;/*bit4 out_frm_wr_done*/

static u32 last_top2_py_level = PY_NO_LEVEL;
static u32 top2_err_cnt;

bool bypass_shadow = true;
bool fake_top1_on;
bool pre_set_done;
bool hw5_clk_on;
u32 test_hold_line;

#define PR_DONE_CONTINUE_CNT 3
#define PR_ON_DELAY_CNT 3

u32 debug_val;/*bit0-17 top1 int;bit20-27 top2 int*/
u32 roi_final[2][4];/*2ppc,4slice final axis:right<<16 + left, based on slice*/
u32 roi_final_y;/*bottom<<16 + top*/

DEFINE_SPINLOCK(cp_lock);

static enum dolby_work_mode last_dolby5_mode;
static bool last_dpss_dct_mode;

/*if pyramid is enable in cfg, we force enable pyramid for top1+top1b due to*/
/*hw not support bypass top1b*/
/*top1b vdr_res is 0, so need to calculate top1b size according top1 size*/
static u32 calc_top1b_size(u32 top1_size)
{
	u32 width;
	u32 height;
	u32 new_size;

	width = top1_size & 0xFFFF;
	height = (top1_size >> 16) & 0xFFFF;

	if (width * height > 1024 * 576) {
		width = width >> 1;
		height = height >> 1;
	}
	new_size =  (width & 0xFFFF) | ((height & 0xFFFF) << 16);
	return new_size;
}

static void dolby5_ahb_reg_config(u32 *reg_baddr,
	u32 core_sel, u32 reg_num, bool reset,
	u32 core2_slice_num,
	u32 core2_h_slc[4],
	u32 corep_h_slc[4],
	u32 py_off,
	bool slice_update,
	bool update_roi)
{
	int i;
	int reg_val, reg_addr;
	static u32 ves_top1 = 0x21c03c0;
	u32 tmp;
	u32 *p_last_baddr = NULL;
	u32 p_last_val;
	u32 core2_byp_shadow = core2_slice_num > 1 ? 1 : 0;/*make sure byp_shadow=1 in slice case*/
	u32 top2_cvm_ctrl;
	static u32 last_top2_0_cvm_ctrl;/*bit20 skip_lut*/
	static u32 last_top2_1_cvm_ctrl;/*bit20 skip_lut*/
	u32 slc_ofst = T6X_CORE2_SLICE_OFF;
	u32 reg_ofst_s0 = 0x213; /* 0x84c >>2,for t6x*/
	u32 reg_ofst_s1 = 0x227; /* 0x89c >>2,for t6x*/

	if (is_aml_t6w())/*t6w no add offset for roi*/
		reg_ofst_s0 = 0;

	if (!reg_baddr)
		return;

	if (bypass_shadow)
		core2_byp_shadow = 1;

	if (last_tv_hw5_setting) {
		if (core_sel == 0)
			p_last_baddr = (u32 *)(last_tv_hw5_setting->top1_reg);
		else if (core_sel == 1)
			p_last_baddr = (u32 *)(last_tv_hw5_setting->top1b_reg);
		else
			p_last_baddr = (u32 *)(last_tv_hw5_setting->top2_reg);
	}

	for (i = 0; i < reg_num; i = i + 1) {
		reg_val = reg_baddr[i * 2];
		reg_addr = reg_baddr[i * 2 + 1];
		reg_addr = reg_addr & 0xffff;
		reg_val = reg_val & 0xffffffff;

		if (p_last_baddr) {
			p_last_val = p_last_baddr[i * 2];
			p_last_val = p_last_val & 0xffffffff;
		} else {
			p_last_val = 0;
		}
		if (i < 6)
			if (debug_dolby & 0x10000000)
				pr_dv_dbg("=== addr: 0x%x val:0x%x ===%x\n",
				reg_addr, reg_val, last_int_top2);

		reg_addr = reg_addr >> 2;
		if (core_sel == 0) {//core1
			if (reg_addr == 1 && !py_enabled && l1l4_enabled) {
				if (debug_dolby & 0x200000)
					pr_dv_dbg("update top1 reg_addr 0x%x value from 0x%x to 0x%x\n",
						reg_addr << 2, reg_val, reg_val & ~(0x4));
				reg_val = reg_val & ~(0x4); /*CNTRL_REGADDR:force enable intensity*/
			}
			if (reg_addr == 1 && bypass_shadow)/*bypass shadow*/
				reg_val = reg_val | (1 << 29);
			/*0x10 clear interrupt*/
			if (reg_addr == 4)/*clear interrupt*/
				reg_val = reg_val | 0x3e;
			if (reg_addr == 5) {
				if (py_off)/*only enable L1L4 interrupt*/
					reg_val = 1 << 5;
			}
			if (reg_addr == 1 || reg_addr == 2 || reg_addr == 3 ||
				reg_addr == 4 || reg_val != p_last_val || reset) {
				VSYNC_WR_TOP1_REG(T6W_DOLBY5_CORE1_REG_BASE + reg_addr, reg_val);
				if (debug_dolby & 0x200000)
					pr_dv_dbg("update top1 %x from %x to %x\n",
						reg_addr, p_last_val, reg_val);
			}
			if (reg_addr == 0x25)/*VDR_RES_REGADDR*/
				ves_top1 = reg_val;
		} else if (core_sel == 1) {//core1b
			if (reg_addr == 1 && !py_enabled && l1l4_enabled) {
				if (debug_dolby & 0x200000)
					pr_dv_dbg("update top1b reg_addr 0x%x from %x to 0xe6a\n",
						reg_addr << 2, reg_val);
				reg_val = 0xe6a; /*CNTRL_REGADDR:force enable pyramid/intensity*/
			}
			if (reg_addr == 1 && bypass_shadow)/*bypass shadow*/
				reg_val = reg_val | (1 << 29);

			/*0x10 clear interrupt*/
			if (reg_addr == 4)/*clear interrupt*/
				reg_val = reg_val | (1 << 3);

			if (reg_addr == 6 && reg_val == 0 && !py_enabled && l1l4_enabled) {
				tmp = calc_top1b_size(ves_top1);/*VDR_RES_REGADDR*/
				if (debug_dolby & 0x200000)
					pr_dv_dbg("update top1b reg_addr 0x%x from %x to %x\n",
						reg_addr << 2, reg_val, tmp);
				reg_val = tmp;
			}
			if (reg_addr == 1 || reg_addr == 2 || reg_addr == 3 ||
				reg_addr == 4 || reg_val != p_last_val || reset)
				VSYNC_WR_TOP1_REG(T6W_DOLBY5_CORE1B_REG_BASE + reg_addr, reg_val);
		} else if (core_sel == 2) { //core2
			/*enable crc cntrl (0xf3d-0xd00)*/
			if ((dolby_vision_flags & FLAG_CERTIFICATION) && reg_addr == 573)
				reg_val = 1;

			if (reg_addr == 1) {
				/*aml_ver|src1_gated |ab_inbuf_en| slice_num|ab_buf_th |byp_shadow*/
				reg_val = reg_val | (3 << 9) | (1 << 11) | (1 << 12) |
						((core2_slice_num >> 1) << 18) | (0x8 << 24) |
						(core2_byp_shadow << 29) | (dither_en << 6);
			}
			/*bypass precision rendering*/
			if (reg_addr == 1 &&
				((test_dv & DEBUG_FORCE_BYPASS_PRECISION_RENDERING) ||
				cfg_info[cur_pic_mode].bypass_pd_from_user ||
				force_bypass_precision_once ||
				force_bypass_precision ||
				force_bypass_pd_level0 ||
				force_bypass_pd_in_game ||
				miss_top1_and_bypass_pr_once ||
				(efuse_mode & 0x2) ||
				device_disable_pd))
				reg_val = reg_val | (1 << 3);

			if (reg_addr == 0x119 && dithering_out_depth)
				reg_val = reg_val | (dithering_out_depth << 12);
			if (reg_addr == 0x40 && rgb2yuv_offset0)
				reg_val = rgb2yuv_offset0;
			if (reg_addr == 0x41 && rgb2yuv_offset1)
				reg_val = rgb2yuv_offset1;
			if (reg_addr == 0x42 && rgb2yuv_offset2)
				reg_val = rgb2yuv_offset2;

			/*0x10 clear interrupt*/
			if (reg_addr == 4) {
				reg_val = reg_val | 0x3e;
				last_int_top2 = reg_val;
			}
			if (reg_addr == 0x53) {
				if (core2_slice_num >= 2)
					top2_cvm_ctrl = reg_val | (3 << 20);//slice skip lut setting
				else
					top2_cvm_ctrl = reg_val;
				if (top2_cvm_ctrl != last_top2_0_cvm_ctrl || reset) {
					VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + reg_addr,
							top2_cvm_ctrl);
					last_top2_0_cvm_ctrl = top2_cvm_ctrl;
				}
			} else if (reg_addr == 1 || reg_addr == 2 || reg_addr == 3 ||
				reg_addr == 4 || reg_val != p_last_val || reset) {
				/*0x8 md start,0xc md end must write,others only update for change*/
				/*core2 s0 roi*/
				if (is_aml_t6x() && reg_addr >= 0x5d && reg_addr <= 0x70)
					VSYNC_WR_TOP2_REG
					(T6W_DOLBY5_CORE2_REG_BASE0 + reg_addr +
					reg_ofst_s0, reg_val);
				else
					VSYNC_WR_TOP2_REG
					(T6W_DOLBY5_CORE2_REG_BASE0 + reg_addr, reg_val);
				if (debug_dolby & 0x200000)
					pr_dv_dbg("update top2 %x from %x to %x\n",
						reg_addr, p_last_val, reg_val);
			}
			if (reg_addr == 0x266) {
				if (debug_dolby & 0x200000)
					pr_dv_dbg("write:%x=%x,%x=%x,%x=%x,%x=%x;%x=%x,%x=%x,%x=%x,%x=%x,%x=%x,%d,%d\n",
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x268,
					(corep_h_slc[1] << 16) | (corep_h_slc[0] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x269,
					(corep_h_slc[3] << 16) | (corep_h_slc[2] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x26a,
					(core2_h_slc[1] << 16) | (core2_h_slc[0] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 +  0x26b,
					(core2_h_slc[3] << 16) | (core2_h_slc[2] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x58, roi_final_y,
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x5d + reg_ofst_s0,
					roi_final[0][0],
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x62 + reg_ofst_s0,
					roi_final[0][1],
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x67 + reg_ofst_s0,
					roi_final[0][2],
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x6c + reg_ofst_s0,
					roi_final[0][3],
					slice_update,
					update_roi);
				if ((debug_dolby & 0x200000) && prm_dolby.core2_slc_mode &&
					prm_dolby.core2_prl_mode)
					pr_dv_dbg("write:%x=%x,%x=%x,%x=%x,%x=%x\n",
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x5d + reg_ofst_s1,
					roi_final[1][0],
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x62 + reg_ofst_s1,
					roi_final[1][1],
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x67 + reg_ofst_s1,
					roi_final[1][2],
					T6W_DOLBY5_CORE2_REG_BASE0 + 0x6c + reg_ofst_s1,
					roi_final[1][3]);
				if (reset || slice_update) {
					VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 0x268,
						(corep_h_slc[1] << 16) | (corep_h_slc[0] << 0));
					if (core2_slice_num >= 2)
						VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 +
						0x269,
						(corep_h_slc[3] << 16) | (corep_h_slc[2] << 0));
					VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 0x26a,
						(core2_h_slc[1] << 16) | (core2_h_slc[0] << 0));
					if (core2_slice_num >= 2)
						VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 +
						0x26b,
						(core2_h_slc[3] << 16) | (core2_h_slc[2] << 0));
				}
				if (reset || slice_update || update_roi) {
					/*roi windows y*/
					VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x58, roi_final_y);
					/*roi windows x for slice0*/
					VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x5d + reg_ofst_s0,
						roi_final[0][0]);
					if (core2_slice_num >= 2) {
						/*roi windows x for slice1-3*/
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x62 + reg_ofst_s0,
						roi_final[0][1]);
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x67 + reg_ofst_s0,
						roi_final[0][2]);
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x6c + reg_ofst_s0,
						roi_final[0][3]);
					}
				}
				if ((reset || slice_update || update_roi) &&
					prm_dolby.core2_slc_mode && prm_dolby.core2_prl_mode) {
					/*roi windows x for slice0*/
					VSYNC_WR_TOP2_REG
					(T6W_DOLBY5_CORE2_REG_BASE0 + 0x5d +
					reg_ofst_s1, roi_final[1][0]);
					if (core2_slice_num >= 2) {
						/*roi windows x for slice1-3*/
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x62 +
						reg_ofst_s1, roi_final[1][1]);
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x67 +
						reg_ofst_s1, roi_final[1][2]);
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + 0x6c +
						reg_ofst_s1, roi_final[1][3]);
					}
				}
			}
		} else if (core_sel == 3) {
			if ((dolby_vision_flags & FLAG_CERTIFICATION) && reg_addr == 573)
				reg_val = 1;
			if (reg_addr == 1) {
				reg_val = reg_val | (3 << 9) | (1 << 11) | (1 << 12) |
						((core2_slice_num >> 1) << 18) | (0x8 << 24) |
						(core2_byp_shadow << 29) | (dither_en << 6);
			}
			/*bypass precision rendering*/
			if (reg_addr == 1 &&
				((test_dv & DEBUG_FORCE_BYPASS_PRECISION_RENDERING) ||
				cfg_info[cur_pic_mode].bypass_pd_from_user ||
				force_bypass_precision_once ||
				force_bypass_precision ||
				force_bypass_pd_level0 ||
				force_bypass_pd_in_game ||
				miss_top1_and_bypass_pr_once ||
				(efuse_mode & 0x2) ||
				device_disable_pd))
				reg_val = reg_val | (1 << 3);

			if (reg_addr == 0x119 && dithering_out_depth)
				reg_val = reg_val | (dithering_out_depth << 12);
			if (reg_addr == 0x40 && rgb2yuv_offset0)
				reg_val = rgb2yuv_offset0;
			if (reg_addr == 0x41 && rgb2yuv_offset1)
				reg_val = rgb2yuv_offset1;
			if (reg_addr == 0x42 && rgb2yuv_offset2)
				reg_val = rgb2yuv_offset2;

			/*0x10 clear interrupt*/
			if (reg_addr == 4) {
				reg_val = reg_val | 0x3e;
				last_int_top2 = reg_val;
			}
			if (reg_addr == 0x53) {
				if (core2_slice_num >= 2)
					top2_cvm_ctrl = reg_val | (3 << 20);
				else
					top2_cvm_ctrl = reg_val;
				if (top2_cvm_ctrl != last_top2_1_cvm_ctrl || reset) {
					VSYNC_WR_TOP2_REG
					(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst +
					reg_addr, top2_cvm_ctrl);
					last_top2_1_cvm_ctrl = top2_cvm_ctrl;
				}
			} else if (reg_addr == 1 || reg_addr == 2 ||
				reg_addr == 3 || reg_addr == 4 ||
				reg_val != p_last_val || reset) {
				if (is_aml_t6x() && reg_addr >= 0x5d && reg_addr <= 0x70)
					VSYNC_WR_TOP2_REG
					(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst +
					reg_addr + reg_ofst_s1, reg_val);
				else
					VSYNC_WR_TOP2_REG
					(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst +
					reg_addr, reg_val);
				if (debug_dolby & 0x200000)
					pr_dv_dbg("update top2_1 %x from %x to %x\n",
						reg_addr, p_last_val, reg_val);
			}
			if (reg_addr == 0x266) {
				if (debug_dolby & 0x200000)
					pr_dv_dbg("write:%x=%x,%x=%x;%x=%x,%x=%x,%x=%x,%x=%x,%x=%x,%x=%x,%x=%x\n",
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x268,
					(corep_h_slc[1] << 16) | (corep_h_slc[0] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x269,
					(corep_h_slc[3] << 16) | (corep_h_slc[2] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x26a,
					(core2_h_slc[1] << 16) | (core2_h_slc[0] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x26b,
					(core2_h_slc[3] << 16) | (core2_h_slc[2] << 0),
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x58, roi_final_y,
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x5d + reg_ofst_s1,
					roi_final[1][0],
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x62 + reg_ofst_s1,
					roi_final[1][1],
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x67 + reg_ofst_s1,
					roi_final[1][2],
					T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x6c + reg_ofst_s1,
					roi_final[1][3]);
				if (reset || slice_update) {
					VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 +
						slc_ofst + 0x268,
						(corep_h_slc[1] << 16) |
						(corep_h_slc[0] << 0));
					if (core2_slice_num >= 2)
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 +
						slc_ofst + 0x269,
						(corep_h_slc[3] << 16) |
						(corep_h_slc[2] << 0));

					VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 +
						slc_ofst + 0x26a,
						(core2_h_slc[1] << 16) |
						(core2_h_slc[0] << 0));
					if (core2_slice_num >= 2)
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 +
						slc_ofst + 0x26b,
						(core2_h_slc[3] << 16) |
						(core2_h_slc[2] << 0));
				}
				if (reset || slice_update || update_roi) {
					/*roi windows y*/
					VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x58,
						roi_final_y);
					/*roi windows x for slice0*/
					VSYNC_WR_TOP2_REG
					(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x5d +
					reg_ofst_s1, roi_final[1][0]);
					if (core2_slice_num >= 2) {
						/*roi windows x for slice1-3*/
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x62 +
						reg_ofst_s1, roi_final[1][1]);
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x67 +
						reg_ofst_s1, roi_final[1][2]);
						VSYNC_WR_TOP2_REG
						(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 0x6c +
						reg_ofst_s1, roi_final[1][3]);
					}
				}
			}
		}
	}
}

#define FORCE_REG_MODE1 1

/*dolby path, only set once*/

/*IPATH_SEL select, should sync with top1*/
static void cfg_dolby_path(struct prm_dolby_top *prm_dolby)
{
	if (!prm_dolby)
		return;
	enum dolby_work_mode path_mode;
	u32 core2_byps = prm_dolby->byps_mode == 2;
/*dv_path_sel*/
	bool dolby_vdin_mode;
	bool dolby_vd1_mode;
	bool dolby_di_mode;
	bool dolby_dpss_mode;
	u32 core2_slc_mode;
	u32 vfcd_dv_path;
	u32 dv_sel_in;
	u32 reg_yuvmif_reg_mode = 1;/*default reg_mode=1*/
	u32 reg_wdma_reg_mode = 1;/*default reg_mode=1*/
	u32 corep_wrmif_reg_mode = 1;/*default reg_mode=1*/
	bool dpss_dct_ahead_dv = prm_dolby->dpss_dct_ahead_dv;

	if ((path_sel & 0xf) && !top_dd_work_mode)
		prm_dolby->dolby5_mode = (path_sel & 0xf);

	if (is_aml_t6x() && !enable_2ppc) {
		if (prm_dolby->dolby5_mode == DOLBY5_VD1_MODE_2PPC)
			prm_dolby->dolby5_mode = DOLBY5_VD1_MODE;
		if (prm_dolby->dolby5_mode == DOLBY5_DPSS_MODE_2PPC)
			prm_dolby->dolby5_mode = DOLBY5_DPSS_MODE;
	}

	path_mode = prm_dolby->dolby5_mode;
	dolby_vdin_mode = (path_mode == DOLBY5_VDIN_MODE) ||
							(path_mode == DOLBY5_VDIN_MODE_2PPC);
	dolby_vd1_mode  = (path_mode == DOLBY5_VD1_MODE) ||
							(path_mode == DOLBY5_VD1_MODE_2PPC);
	dolby_di_mode   = (path_mode == DOLBY5_DPSS_DI_MODE) ||
							(path_mode == DOLBY5_DPSS_DI_MODE_2PPC);
	dolby_dpss_mode = (path_mode == DOLBY5_DPSS_MODE) ||
							(path_mode == DOLBY5_DPSS_MODE_2PPC);
	core2_slc_mode =
						(path_mode == DOLBY5_VD1_MODE_2PPC) ||
						(path_mode == DOLBY5_VDIN_MODE_2PPC) ||
						(path_mode == DOLBY5_DPSS_MODE_2PPC) ||
						(path_mode == DOLBY5_DPSS_PRL_MODE_2PPC) ||
						(path_mode == DOLBY5_DPSS_DI_MODE_2PPC);

	vfcd_dv_path  = dolby_vd1_mode ? 2 :
						dolby_di_mode ? 3 :
						dolby_dpss_mode ? 1 : 0;
	dv_sel_in = dolby_vd1_mode ? 0x3 :
					dolby_dpss_mode ? (dpss_dct_ahead_dv ? 0x4 : 0x1) :
					dolby_vdin_mode ? 0x0 : 0xf;

	if (is_aml_t6x())
		prm_dolby->core2_prl_mode = (prl_mode && core2_slc_mode) ? true : false;
	else
		prm_dolby->core2_prl_mode = false;

	last_dolby5_mode = prm_dolby->dolby5_mode;
	last_dpss_dct_mode = prm_dolby->dpss_dct_ahead_dv;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("[%s]:path_mode %d,%d %d,vfcd_dv_path %d,prl %d,byps %d,dct %d\n",
				__func__, path_mode, dolby_vd1_mode, core2_slc_mode,
				vfcd_dv_path, prm_dolby->core2_prl_mode, core2_byps,
				dpss_dct_ahead_dv);

	if (is_aml_t6w())
		VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_WRAP_CTRL, core2_byps, 31, 1);
	else if (is_aml_t6x())
		VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_WRAP_CTRL, core2_byps, 27, 1);

	if (is_aml_t6w()) {
		if (path_sel & 0xf00)
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IPATH_SEL,
				(path_sel & 0xf00) >> 8, 0, 4);
		else
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IPATH_SEL, dv_sel_in, 0, 4);
		if (dpss_dct_ahead_dv)
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IPATH_SEL, 1, 20, 2);
		else
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IPATH_SEL, 0, 20, 2);
		if (debug_dolby & 0x80000)
			pr_dv_dbg("[%s]:path 0x%x, 0x%x\n", __func__,
				READ_VPP_DV_REG(T6W_VPU_DOLBY_IPATH_SEL),
				READ_VPP_DV_REG(T6W_VPU_DOLBY_OPATH_SEL));
	} else if (is_aml_t6x()) {
		if (path_sel & 0xf00)
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_IPATH_SEL,
				(path_sel & 0xf00) >> 8, 0, 4);
		else
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_IPATH_SEL, dv_sel_in, 0, 4);
		if (dpss_dct_ahead_dv)
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_IPATH_SEL, 1, 20, 2);
		else
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_IPATH_SEL, 0, 20, 2);
		if (debug_dolby & 0x80000)
			pr_dv_dbg("[%s]:path 0x%x, 0x%x\n", __func__,
				READ_VPP_DV_REG(T6X_VPU_DOLBY_IPATH_SEL),
				READ_VPP_DV_REG(T6X_VPU_DOLBY_OPATH_SEL));
	}
	//==========dv_top2_slc_mode======
	if (is_aml_t6x()) {
		VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_WRAP_CTRL,
			core2_slc_mode, 28, 1);//reg_top2_slice_mode

		/*driver can only write one set of regs in core2_prl mode*/
		VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_WRAP_CTRL,
			core2_slc_mode && prm_dolby->core2_prl_mode, 4, 1);
	}

	/*dv_mif_reg_mode_sel*/
	if (dolby_dpss_mode == 1) {
#if FORCE_REG_MODE1 /*always reg_mode=1*/
		reg_yuvmif_reg_mode = 1;/*maybe use core1 yuv addr provided by dpss*/
		reg_wdma_reg_mode = 1;/*still use hist addr alloc by dolby*/
		corep_wrmif_reg_mode = 1;/*still use up_scla addr alloc by dolby*/
#else
		reg_yuvmif_reg_mode = 0;/*control by dpss*/
		reg_wdma_reg_mode = 0;/*control by dpss*/
		corep_wrmif_reg_mode = 0;/*control by dpss*/
#endif
	}
	if (dolby_dpss_mode == 1) {
		/*dae+top1 rdma trigger*/
		WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP1_RDMA_TRIG_CTRL, 1, 1, 1);
		/*dpe+top2 rdma trigger*/
		//WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP2_RDMA_TRIG_CTRL, 1, 1,1);

		/*if without top1, need block top1 inter interrupt,*/
		/*replace it with 1 to trigger dae*/
		if (!enable_top1) {
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IRQ_CTRL0, 0, 0, 1);/*disable wrap irq*/
			//WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP1_CTRL2, 0x3f, 0, 6);
			//WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP2_PYRD_CTRL, 1, 12, 1);
		} else {
			WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP1_CTRL2, 0, 0, 6);
			WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP2_PYRD_CTRL, 0, 12, 1);
		}
	} else {
		/*restore dae+top1 rdma trigger*/
		WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP1_RDMA_TRIG_CTRL, 0, 1, 1);
		/*restore dpe+top2 rdma trigger*/
		//WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP2_RDMA_TRIG_CTRL, 0, 1, 1);
		//restore top1 reg_int_sel
		WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP1_CTRL2, 0, 0, 6);
		WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP2_PYRD_CTRL, 0, 12, 1);
	}

	VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP1_CTRL3, reg_yuvmif_reg_mode, 2, 1);
	VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP1_CTRL3, reg_wdma_reg_mode, 0, 2);
	VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP2_PYUP_WMIF_CTRL2, corep_wrmif_reg_mode, 26, 4);/*corep*/
	VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL2, corep_wrmif_reg_mode, 26, 4);/*core2*/
	if (is_aml_t6x()) {
		if (core2_slc_mode)
			VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL2 + T6X_CORE2_SLICE_OFF,
				corep_wrmif_reg_mode, 26, 4);/*core2 slice1*/

		if ((path_mode == DOLBY5_VD1_MODE || path_mode == DOLBY5_VD1_MODE_2PPC) &&
			prm_dolby->mosaic_mode) {
			/*bit 29:30,set 1 choose vd1-1, set 2 choose vd1-2*/
			WRITE_VPP_DV_REG_BITS(T6X_VPU_AXIRD_DV2X2_SIZE0, 1, 29, 2);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_AXIRD_DV2X2_SIZE0,
			(prm_dolby->core2_hsize) << 16 | prm_dolby->core2_vsize, 0, 28);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_AXIRD_DV2X2_SIZE1,
			(prm_dolby->core2_hsize) << 16 | prm_dolby->core2_vsize, 0, 28);
		} else {
			WRITE_VPP_DV_REG_BITS(T6X_VPU_AXIRD_DV2X2_SIZE0, 0, 29, 2);
		}
	}
}

/*axi path select and opath sel, should sync with top2 for vpp mode*/
static inline void cfg_dolby_axi_path(enum dolby_work_mode path_mode, bool dpss_dct_ahead_dv)
{
/*dv_path_sel*/
	u32 vfcd_dv_path =
	((path_mode == DOLBY5_VD1_MODE) || (path_mode == DOLBY5_VD1_MODE_2PPC)) ? 2 :
	((path_mode == DOLBY5_DPSS_DI_MODE) || (path_mode == DOLBY5_DPSS_DI_MODE_2PPC)) ? 3 :
	(((path_mode == DOLBY5_DPSS_MODE) || (path_mode == DOLBY5_DPSS_MODE_2PPC)) &&
		!dpss_dct_ahead_dv) ? 1 : 0;
	bool dolby_dpss_prl = (path_mode == DOLBY5_DPSS_PRL_MODE) ||
							(path_mode == DOLBY5_DPSS_PRL_MODE_2PPC);
	u32 axi_path = 0;
	u32 dd_path_ctrl = 0;
	bool dolby_vdin_mode = (path_mode == DOLBY5_VDIN_MODE) ||
							(path_mode == DOLBY5_VDIN_MODE_2PPC);
	bool dolby_vd1_mode  = (path_mode == DOLBY5_VD1_MODE) ||
							(path_mode == DOLBY5_VD1_MODE_2PPC);
	bool dolby_dpss_mode = (path_mode == DOLBY5_DPSS_MODE) ||
							(path_mode == DOLBY5_DPSS_MODE_2PPC);
	u32 dv_sel_out = 0x0;//0:dolby,1:hdr2,2:primesl

	if (path_sel & 0xf0) {
		if (path_sel & 0x30)
			axi_path = 0;
		else if (path_sel & 0xc0)
			axi_path = (path_sel & 0xc0) >> 2;
	} else {
		axi_path = vfcd_dv_path;
	}

	if (is_aml_t6w()) {
		if (path_sel & 0xffff000) {
			WRITE_VPP_DV_REG(T6W_VPU_DOLBY_OPATH_SEL, (path_sel & 0xffff000) >> 12);
		} else if (dolby_vdin_mode) {
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, dv_sel_out, 0, 4);
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xff, 8, 8);
		} else if (dolby_vd1_mode) {
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xf, 0, 4);
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, dv_sel_out, 8, 4);
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xff, 12, 8);
		} else if (dolby_dpss_mode) {
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xf, 0, 4);
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xf, 8, 4);
			if (!dpss_dct_ahead_dv) {
				WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, dv_sel_out, 12, 4);
			} else {
				WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xf, 12, 4);
				WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, dv_sel_out, 16, 4);
			}
		}
		if (path_mode == DOLBY5_VD1_MODE)
			VSYNC_WR_TOP2_BITS(T6W_VPU_AXIRD_PATH_CTRL, axi_path, 8, 2);
		else
			WRITE_VPP_DV_REG_BITS(T6W_VPU_AXIRD_PATH_CTRL, axi_path, 8, 2);

		update_dd_axi_path_mode(axi_path);
		if (debug_dolby & 0x80000) {
			pr_dv_dbg("[%s]:path 0x%x, 0x%x\n", __func__,
					READ_VPP_DV_REG(T6W_VPU_DOLBY_IPATH_SEL),
					READ_VPP_DV_REG(T6W_VPU_DOLBY_OPATH_SEL));
		pr_dv_dbg("[%s]:path_mode %d,vfcd_dv_path %d %d,path_sel %d,axipath 0x%x 0x%x 0x%x\n",
				__func__, path_mode, vfcd_dv_path, axi_path, path_sel,
				READ_VPP_DV_REG(T6W_VPU_AXIRD_PATH_CTRL),
				VSYNC_RD_DV_REG(T6W_VPU_AXIRD_PATH_CTRL),
				VSYNC_RD_TOP2_REG(T6W_VPU_AXIRD_PATH_CTRL));
		}
	} else if (is_aml_t6x()) {
		if (path_sel & 0xffff000) {
			WRITE_VPP_DV_REG(T6X_VPU_DOLBY_OPATH_SEL, (path_sel & 0xffff000) >> 12);
		} else if (dolby_vdin_mode) {
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, dv_sel_out, 0, 4);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xff, 8, 8);
		} else if (dolby_vd1_mode) {
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xf, 0, 4);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, dv_sel_out, 8, 4);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xff, 12, 8);
		} else if (dolby_dpss_mode) {
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xf, 0, 4);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xf, 8, 4);
			if (!dpss_dct_ahead_dv) {
				WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, dv_sel_out, 12, 4);
			} else {
				WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xf, 12, 4);
				WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, dv_sel_out, 16, 4);
			}
		}
		dd_path_ctrl = (1 << 3) | (dolby_dpss_prl << 2) | axi_path;

		if (path_mode == DOLBY5_VD1_MODE || path_mode == DOLBY5_VD1_MODE_2PPC)
			VSYNC_WR_TOP2_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL, dd_path_ctrl);
		else
			WRITE_VPP_DV_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL, dd_path_ctrl);

		if (debug_dolby & 0x80000) {
			pr_dv_dbg("[%s]:path 0x%x, 0x%x\n", __func__,
					READ_VPP_DV_REG(T6X_VPU_DOLBY_IPATH_SEL),
					READ_VPP_DV_REG(T6X_VPU_DOLBY_OPATH_SEL));
		pr_dv_dbg("[%s]:path_mode %d,vfcd_dv_path %d %d,path_sel %d,axipath 0x%x 0x%x 0x%x\n",
				__func__, path_mode, vfcd_dv_path, dd_path_ctrl, path_sel,
				READ_VPP_DV_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL),
				VSYNC_RD_DV_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL),
				VSYNC_RD_TOP2_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL));
		}
	}
}

void cfg_dd_path_off(void)
{
	if (is_aml_t6w()) {
		if (is_amdv_dpss_path()) {
			WRITE_VPP_DV_REG_BITS(T6W_VPU_AXIRD_PATH_CTRL, 0, 8, 2);
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IPATH_SEL, 0xf, 0, 4);
			/*ensure dct+dv mode off*/
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IPATH_SEL, 0, 20, 2);
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xff, 8, 8);
		} else {
			VSYNC_WR_DV_REG_BITS(T6W_VPU_AXIRD_PATH_CTRL, 0, 8, 2);
			VSYNC_WR_DV_REG_BITS(T6W_VPU_DOLBY_IPATH_SEL, 0xf, 0, 4);/*bit0-3*/
			VSYNC_WR_DV_REG_BITS(T6W_VPU_DOLBY_OPATH_SEL, 0xfff, 8, 12);/*bit8-19*/
		}
		if (debug_dolby & 0x80000)
			pr_dv_dbg("[%s]:path off 0x%x, 0x%x, 0x%x\n", __func__,
				READ_VPP_DV_REG(T6W_VPU_AXIRD_PATH_CTRL),
				READ_VPP_DV_REG(T6W_VPU_DOLBY_IPATH_SEL),
				READ_VPP_DV_REG(T6W_VPU_DOLBY_OPATH_SEL));
	} else if (is_aml_t6x()) {
		if (is_amdv_dpss_path()) {
			WRITE_VPP_DV_REG_BITS(T6X_VPU_AXIRD_DOLBY_PATH_CTRL, 0, 0, 4);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_IPATH_SEL, 0xf, 0, 4);
			/*ensure dct+dv mode off*/
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_IPATH_SEL, 0, 20, 2);
			WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xff, 8, 8);
		} else {
			VSYNC_WR_DV_REG_BITS(T6X_VPU_AXIRD_DOLBY_PATH_CTRL, 0, 0, 4);
			VSYNC_WR_DV_REG_BITS(T6X_VPU_DOLBY_IPATH_SEL, 0xf, 0, 4);/*bit0-3*/
			VSYNC_WR_DV_REG_BITS(T6X_VPU_DOLBY_OPATH_SEL, 0xfff, 8, 12);/*bit8-19*/
		}
		if (debug_dolby & 0x80000)
			pr_dv_dbg("[%s]:path off 0x%x, 0x%x, 0x%x\n", __func__,
				READ_VPP_DV_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL),
				READ_VPP_DV_REG(T6X_VPU_DOLBY_IPATH_SEL),
				READ_VPP_DV_REG(T6X_VPU_DOLBY_OPATH_SEL));
	}
}

/*true: dpss auto mode, need sw trigger; false: tbc mode, auto trigger*/
static inline bool sw_trigger_top1(struct prm_dolby_top *prm_dolby)
{
	bool flag = false;

	if (prm_dolby && is_amdv_dpss_path()) {
		if (!(test_dv & DEBUG_VSYNC_TRIGGER_TOP1_RST) && !prm_dolby->dpss_tbc_mode)
			flag = true;
	}
	return flag;
}

/*true: dpss auto mode, need sw trigger; false: tbc mode, auto trigger*/
static inline bool sw_trigger_top2(struct prm_dolby_top *prm_dolby)
{
	bool flag = false;

	if (prm_dolby && is_amdv_dpss_path()) {
		if (!(test_dv & DEBUG_VSYNC_TRIGGER_TOP2_RST) && !prm_dolby->dpss_tbc_mode)
			flag = true;
	}
	return flag;
}

static void cfg_viu_pix_rdmif(struct prm_intf_type *prm_mif, int32_t mif_index, bool need_update)
{
	u32 frm_hsize;
	u32 frm_vsize;
	u32 src_bit;
	u32 stride_y;
	u32 stride_c;
	u32 rev_x;
	u32 rev_y;
	u32 y_baddr;
	u32 u_baddr;
	u32 slc0_x_st_luma;
	u32 slc0_x_ed_luma;
	u32 slc0_y_st_luma;
	u32 slc0_y_ed_luma;
	u32 slc0_x_st_chrm;
	u32 slc0_x_ed_chrm;
	u32 slc0_y_st_chrm;
	u32 slc0_y_ed_chrm;
	u32 slc_hsize;
	u32 slc_vsize;
	u32 fmt;
	u32 plan;
	u32 bit;
	u32 y_pack_mode;//0:1x 1:2x  2:4x 3:8x  4:16x  5:32x 6:64x
	u32 y_bits_mode;//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	u32 cb_pack_mode;//0:1x 1:2x  2:4x  3:8x  4:16x  5:32x 6:64x
	u32 cb_bits_mode;//1:8b 2:16b 3:32b 4:64b 5:128b 9:24b a:40b d:48b
	bool hfmt_en;
	bool vfmt_en;
	u32 mif1_en;
	u32 skip_en;
	u32 skip_line_luma_t;
	u32 skip_line_luma;
	u32 skip_line_chrm;
	u32 vert_skip_uv = 0;
	u32 vert_skip_y  = 0;
	//u32 horz_skip_uv = 0;
	//u32 horz_skip_y  = 0;
	u32 uv_vsize_in;
	u32 vt_yc_ratio = 0;
	u32 hz_yc_ratio = 0;
	u32 vt_phase_step;
	u32 vfmt_w;
	u32 reg_win_vsel;
	u32 uv_vsize_in_t;
	u32 luma_vsize;
	u32 luma_win_yend;
	u32 chrm_vsize;
	u32 chrm_win_yend;
	u32 uv_swap = 0;
	u32 pic_32byte_aligned = 0;
	u32 value = 0;
	u32 burst_size = 1;
	u32 burst_len = 2;
	u32 little_endian = 1;/*default little endian*/
	u32 swap_64bit = 0;
	enum input_mode_enum input_mode = IN_MODE_OTT;

	if (mif_index != 3 || !prm_mif)
		return;

	if (tv_hw5_setting)
		input_mode = tv_hw5_setting->top1.input_mode;

	if (prm_mif->blk_mode) {
		pic_32byte_aligned = 1;/*case5345 dw=16: non-mmu, h264 core output blkmode*/
		burst_len = ((prm_mif->burst_len) & 0x7);
	}
	if (prm_mif->linear_mode/*prm_mif->endian == 0*/) {
		little_endian = 1;
		swap_64bit = 0;
	} else {
		little_endian = 0;
		swap_64bit = 1;
	}

	src_bit   = prm_mif->src_bit == BIT_8 ? 8
				: prm_mif->src_bit == BIT_10 ? 10
				: prm_mif->src_bit == BIT_P010 ? 16 : 12;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("prm_mif %dx%d,%d,bit %d,plan %d,fmt %d,skip %d,src_baddr %x %x\n",
				prm_mif->src_hsize,
				prm_mif->src_vsize,
				top1_vd_info.buf_width, src_bit,
				prm_mif->src_plan, prm_mif->src_fmt,
				prm_mif->skip_line,
				prm_mif->src_baddr[0], prm_mif->src_baddr[1]);

	y_baddr  = prm_mif->src_baddr[0];
	u_baddr  = prm_mif->src_baddr[1];

	if (need_update) {
		frm_hsize = top1_vd_info.buf_width;//prm_mif->src_hsize;/*use decoder align width*/
		frm_vsize = prm_mif->src_vsize;
		hfmt_en = prm_mif->src_fmt != YUV444;
		vfmt_en = prm_mif->src_fmt == YUV420;
		mif1_en = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;
		skip_en = prm_mif->skip_line > 0 ? 1 : 0;
		skip_line_luma_t = prm_mif->skip_line;
		skip_line_luma = skip_line_luma_t == 0 ? 0 : skip_line_luma_t - 1;

		if (prm_mif->src_plan == PLANAR_X1) {
			if (prm_mif->src_fmt == YUV444) {
				if (src_bit == 10)
					stride_y = (frm_hsize * (3 * src_bit + 2) + 511) / 512 * 4;
				else
					stride_y = (frm_hsize * src_bit * 3 + 511) / 512 * 4;
			} else {
				stride_y = (frm_hsize * src_bit * 2 + 511) / 512 * 4;
			}
		} else {
			stride_y  = (frm_hsize * src_bit + 511) / 512 * 4;
		}
		if (debug_rdmif & 0xfff000)
			stride_y = (debug_rdmif >> 12) & 0xfff;
		stride_c = stride_y;

		rev_x = prm_mif->reverse[0];
		rev_y = prm_mif->reverse[1];
		slc0_x_st_luma = prm_mif->slc_x_st[0];
		slc0_x_ed_luma = prm_mif->slc_x_ed[0];
		slc0_y_st_luma = prm_mif->slc_y_st[0];
		slc0_y_ed_luma = prm_mif->slc_y_ed[0];
		slc0_x_st_chrm = prm_mif->src_fmt != YUV444 ? slc0_x_st_luma >> 1 : slc0_x_st_luma;
		slc0_x_ed_chrm = prm_mif->src_fmt != YUV444 ? slc0_x_ed_luma >> 1 : slc0_x_ed_luma;
		slc0_y_st_chrm = prm_mif->src_fmt == YUV420 ? slc0_y_st_luma >> 1 : slc0_y_st_luma;
		slc0_y_ed_chrm = prm_mif->src_fmt == YUV420 ? slc0_y_ed_luma >> 1 : slc0_y_ed_luma;
		slc_hsize = slc0_x_ed_luma - slc0_x_st_luma + 1;
		slc_vsize = slc0_y_ed_luma - slc0_y_st_luma + 1;

		fmt =  prm_mif->src_fmt  == YUV444 ? 0 : prm_mif->src_fmt == YUV422 ? 1 : 2;
		plan = prm_mif->src_plan == PLANAR_X1 ? 0 : 1;
		bit =   prm_mif->src_bit == BIT_8 ? 0 :
				prm_mif->src_bit == BIT_10 ? 1 :
				prm_mif->src_bit == BIT_P010 ? 3 : 2;

		switch ((plan << 8) | (fmt << 4) | bit) {
		case 0x000:
			y_bits_mode = 9;
			y_pack_mode = 1;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;//444_1p_8b
		case 0x001:
			y_bits_mode = 3;
			y_pack_mode = 1;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;//444_1p_10b
		case 0x003:
			y_bits_mode = 13;
			y_pack_mode = 1;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;//444_1p_16b
		case 0x010:
			y_bits_mode = 2;
			y_pack_mode = 2;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;//422_1p_8b
		case 0x011:
			y_bits_mode = 10;
			y_pack_mode = 1;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;//422_1p_10b
		case 0x012:
			y_bits_mode = 9;
			y_pack_mode = 2;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;//422_1p_12b
		case 0x013:
			y_bits_mode = 3;
			y_pack_mode = 2;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;//422_1p_16b
		case 0x110:
			y_bits_mode = 1;
			y_pack_mode = 1;
			cb_bits_mode = 2;
			cb_pack_mode = 1;
			break;//422_2p_8b
		case 0x111:
			y_bits_mode = 10;
			y_pack_mode = 0;
			cb_bits_mode = 10;
			cb_pack_mode = 0;
			break;//422_2p_10b
		case 0x112:
			y_bits_mode = 9;
			y_pack_mode = 0;
			cb_bits_mode = 9;
			cb_pack_mode = 1;
			break;//422_2p_12b
		case 0x113:
			y_bits_mode = 2;
			y_pack_mode = 1;
			cb_bits_mode = 3;
			cb_pack_mode = 1;
			break;//422_2p_16b
		case 0x120:
			y_bits_mode = 1;
			y_pack_mode = 1;
			cb_bits_mode = 2;
			cb_pack_mode = 1;
			break;//420_2p_8b
		case 0x121:
			y_bits_mode = 10;
			y_pack_mode = 0;
			cb_bits_mode = 10;
			cb_pack_mode = 0;
			break;//420_2p_10b
		case 0x122:
			y_bits_mode = 9;
			y_pack_mode = 0;
			cb_bits_mode = 9;
			cb_pack_mode = 1;
			break;//420_2p_12b
		case 0x123:
			y_bits_mode  = 2;
			y_pack_mode = 1;
			cb_bits_mode = 3;
			cb_pack_mode = 1;
			break;//420_2p_16b
		default:
			y_bits_mode = 9;
			y_pack_mode = 1;
			cb_bits_mode = 0;
			cb_pack_mode = 0;
			break;
		}
		if (dolby_vision_flags & FLAG_CERTIFICATION)
			uv_swap = (input_mode == IN_MODE_HDMI) ? 0 : 1;

		if (debug_rdmif & 0xfff) {
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, debug_rdmif & 0xfff, 0, 12);
		} else {
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, plan, 4, 2);
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, fmt, 6, 2);
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, bit, 8, 2);
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, rev_x, 0, 1);
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, rev_y, 1, 1);
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, uv_swap, 11, 1);
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, little_endian, 2, 1);
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_TOP_CTRL, swap_64bit, 3, 1);
		}

		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF0_RMIF_CTRL1, 1, 19, 1);//ch0 reg_mif_en

		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF0_RMIF_CTRL3, stride_y, 0, 13);//reg_stride
		//out_pack_mode
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF0_RMIF_CTRL5, y_pack_mode, 0, 3);
		//out_bits_mode
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF0_RMIF_CTRL5, y_bits_mode, 3, 4);

		VSYNC_WR_TOP1_REG(T6W_DOLBY_INP_LUMA_RMIF_SCOPE_X,
			slc0_x_ed_luma << 16 | slc0_x_st_luma);//ch0 slc0 scope_x
		VSYNC_WR_TOP1_REG(T6W_DOLBY_INP_LUMA_RMIF_SCOPE_Y,
			slc0_y_ed_luma << 16 | slc0_y_st_luma);//ch0 slc0 scope_y

		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF1_RMIF_CTRL1, mif1_en, 19, 1);//ch1 reg_mif_en
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF1_RMIF_CTRL3, stride_c, 0, 13);//reg_stride
		//out_pack_mode
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF1_RMIF_CTRL5, cb_pack_mode, 0, 3);
		//out_bits_mode
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF1_RMIF_CTRL5, cb_bits_mode, 3, 4);

		VSYNC_WR_TOP1_REG(T6W_DOLBY_INP_CHRM_RMIF_SCOPE_X,
		slc0_x_ed_chrm << 16 | slc0_x_st_chrm);//ch1 slc0 scope_x

		VSYNC_WR_TOP1_REG(T6W_DOLBY_INP_CHRM_RMIF_SCOPE_Y,
		slc0_y_ed_chrm << 16 | slc0_y_st_chrm);//ch1 slc0 scope_y

		skip_line_chrm = fmt == 2 ? skip_line_luma >> 1 : skip_line_luma;

		if (debug_dolby & 0x80000)
			pr_dv_dbg("rdmif 0x%x,%d %d,skip_en %d,stride %d,skip %d,%d %d %d %d\n",
					((plan << 8) | (fmt << 4) | bit),
					slc0_x_ed_luma, slc0_y_ed_luma, skip_en,
					stride_y, skip_line_luma,
					y_bits_mode, y_pack_mode, cb_bits_mode, cb_pack_mode);

		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_F0_LUMA_RPT_PAT, skip_en, 3, 1);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_F0_LUMA_RPT_PAT, skip_line_luma, 0, 3);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_F0_CHRM_RPT_PAT, skip_en, 3, 1);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_F0_CHRM_RPT_PAT, skip_line_chrm, 0, 3);

		if (fmt == 2) { //420
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
				//if (horz_skip_uv != 0)
				//	hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
				//else
				hz_yc_ratio = 1;
			} else {
				hz_yc_ratio = 0;
			}
		}	else if (fmt == 1) {//422
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
				//if (horz_skip_uv != 0)
					//hz_yc_ratio = horz_skip_y == 0 ? 2 : 1;
				//else
				hz_yc_ratio = 1;
			} else {
				hz_yc_ratio = 0;
			}
		}	else if (fmt == 0) {//444
			if (vfmt_en) {//vert_skip_y==0,vert_skip_uv!=0
				uv_vsize_in = (slc_vsize >> 1);
				vt_yc_ratio = 1;
			} else {
				uv_vsize_in = slc_vsize;
				vt_yc_ratio = 0;
			}
			if (hfmt_en)//horz_skip_y==0,horz_skip_uv!=0
				hz_yc_ratio = 1;
			else
				hz_yc_ratio = 0;
		}
		vt_phase_step = (16 >> vt_yc_ratio);
		vfmt_w = (slc_hsize >> hz_yc_ratio);

		reg_win_vsel = (prm_mif->skip_line > 0) ? 1 : 0;
		if (prm_mif->skip_line > 0) {
			uv_vsize_in_t = uv_vsize_in / (prm_mif->skip_line + 1);
			luma_vsize = slc_vsize / (prm_mif->skip_line + 1);
		} else {
			uv_vsize_in_t = uv_vsize_in;
			luma_vsize = slc_vsize;
		}
		luma_win_yend = luma_vsize - 1;
		chrm_vsize = uv_vsize_in_t;
		chrm_win_yend = uv_vsize_in_t - 1;

		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_VD_CFMT_H, reg_win_vsel, 29, 1);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_WIN_LUMA_SIZE, luma_vsize, 0, 13);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_WIN_LUMA_Y, luma_win_yend, 0, 13);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_WIN_CHRM_SIZE, chrm_vsize, 0, 13);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_WIN_CHRM_Y, chrm_win_yend, 0, 13);

		VSYNC_WR_TOP1_REG((T6W_DOLBY_INP_VD_CFMT_CTRL),
			(1 << 28) | //hz rpt pixel
			(0 << 24) | //hz ini phase
			(0 << 23) | //repeat p0 enable
			(hz_yc_ratio << 21) | //hz yc ratio
			(hfmt_en << 20) | //hz enable
			(1 << 19) | //cvfmt_always_en
			(1 << 17) | //nrpt_phase0 enable
			(0 << 16) | //repeat l0 enable
			(0 << 12) | //skip line num
			(0 << 8) | //vt ini phase
			(vt_phase_step << 1) | //vt phase step (3.4)
			(vfmt_en << 0)//vt enable
			);

		VSYNC_WR_TOP1_REG((T6W_DOLBY_INP_VD_CFMT_W),
			(slc_hsize << 16) | //hz format width
			(vfmt_w << 0)//vt format width
			);
		VSYNC_WR_TOP1_REG((T6W_DOLBY_INP_VD_CFMT_H), uv_vsize_in_t);
		//cfg mmu mode:
		if (prm_mif->mmu_mode_en)
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_RMIF_MMU_CTRL, 1, 0, 1);

		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF0_RMIF_CTRL1, burst_len, 0, 3); //burst8
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF1_RMIF_CTRL1, burst_len, 0, 3); //burst8

		//set burst len in viu_pix_rdmif
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF0_RMIF_CTRL1, burst_size, 3, 3);
		//set burst len in viu_pix_rdmif
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF1_RMIF_CTRL1, burst_size, 3, 3);
		//write block_mode and bit22 32 aligned
		value = pic_32byte_aligned << 2 | prm_mif->blk_mode;
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF0_RMIF_CTRL1, value, 20, 3);
		//write block_mode and bit22 32 aligned
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_INP_MIF1_RMIF_CTRL1, value, 20, 3);
	}

	VSYNC_WR_TOP1_REG(T6W_DOLBY_INP_MIF0_RMIF_CTRL4, y_baddr >> 4);//ch0 baddr
	VSYNC_WR_TOP1_REG(T6W_DOLBY_INP_MIF1_RMIF_CTRL4, u_baddr >> 4);//ch1 baddr
}

static void dolby5_top1_ini(struct prm_dolby_top *prm_dolby,
	u64 *top1_reg, u64 *top1b_reg, bool reset, bool force_update)
{
	u32 core1_hsize = prm_dolby->core1_src_hsize;
	u32 core1_vsize = prm_dolby->core1_src_vsize;
	u32 vsync_sel = 2;//dolby5_top1->vsync_sel;//select input vsync,todo
	u32 *p_reg_top1;
	u32 *p_reg_top1b;
	int top1_ahb_num;
	int top1b_ahb_num;
	//u32 top1_ctrl;
	u32 top1_hsize = prm_dolby->core1_src_hsize >> top1_scale;/*size after rdmif downscale*/
	u32 top1_vsize = prm_dolby->core1_src_vsize >> top1_scale;
	//u32 write_val;
	u32 py_off = prm_dolby->core1_py_level == 2;
	u32 top1_end_mask = py_off ? 0xe : 0;
	u32 pywr_en       = py_off ? 0   : 1;
	struct prm_intf_type dolby_yuv_mif;
	u32 top1_ctrl0;
	/*core1b and corep connect directly, no need pingpang buf*/
	dma_addr_t py_baddr[7] = {prm_dolby->core1_pyr_wbaddr[0],
							prm_dolby->core1_pyr_wbaddr[1],
							prm_dolby->core1_pyr_wbaddr[2],
							prm_dolby->core1_pyr_wbaddr[3],
							prm_dolby->core1_pyr_wbaddr[4],
							prm_dolby->core1_pyr_wbaddr[5],
							prm_dolby->core1_pyr_wbaddr[6]};
	/*4k2k ds to 4k1k t6x may set 1*/
	u32 nr_4k1k_vds_en = prm_dolby->vds_4k1k_en;

	memset((void *)(&dolby_yuv_mif), 0, sizeof(struct prm_intf_type));

	if (debug_dolby & 0x80000)
		pr_dv_dbg("core1 size %dx%d,hist_id %d\n",
					core1_hsize, core1_vsize,
					l1l4_wr_index);

	if (force_top1_vskip)
		top1_vsize = top1_vsize >> 1;

	if (hw5_reg_from_file || (test_dv & DEBUG_FIXED_REG) /*fixed reg*/) {
		p_reg_top1 = (u32 *)top1_reg_buf;
		top1_ahb_num = top1_reg_num;
		p_reg_top1b = (u32 *)top1b_reg_buf;
		top1b_ahb_num = top1b_reg_num;
	} else {
		p_reg_top1 = (u32 *)top1_reg;
		top1_ahb_num = TOP1_REG_NUM;
		p_reg_top1b = (u32 *)top1b_reg;
		top1b_ahb_num = TOP1B_REG_NUM;
	}
	if (py_enabled && p_reg_top1 && p_reg_top1b)
		check_pr_enabled_in_reg(p_reg_top1, p_reg_top1b);

	/*config core1/1b_ahb_reg before top reg*/
	if (p_reg_top1)
		dolby5_ahb_reg_config(p_reg_top1, 0, top1_ahb_num, reset, 0, 0, 0, py_off, 0, 0);
	if (p_reg_top1b)
		dolby5_ahb_reg_config(p_reg_top1b, 1, top1b_ahb_num, reset, 0, 0, 0, py_off, 0, 0);

	if (reset || force_update) {
		/*top1, config size after skip*/
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PIC_SIZE, (top1_vsize << 16) | top1_hsize);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP1_PYWR_CTRL,
		(prm_dolby->core1_py_level & 0x1), 0, 1);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP1_CTRL2, top1_end_mask, 0, 6);//reg_int_sel
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP1_CTRL2, pywr_en, 10, 1);//reg_pywr_en
	}
	if (reset) {
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_BADDR1, py_baddr[0] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_BADDR2, py_baddr[1] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_BADDR3, py_baddr[2] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_BADDR4, py_baddr[3] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_BADDR5, py_baddr[4] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_BADDR6, py_baddr[5] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_BADDR7, py_baddr[6] >> 4);

		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_STRIDE12, (40 << 16) | 80);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_STRIDE34, (12 << 16) | 20);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_STRIDE56, (4 << 16) | 8);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_PYWR_STRIDE7, 4);/*4*/

		/*hist addr, fixed index by default, no pingpang*/
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_WDMA_BADDR0, prm_dolby->core1_wdma_wbaddr >> 4);
	}

	dolby_yuv_mif.src_hsize = core1_hsize;
	dolby_yuv_mif.src_vsize = core1_vsize;
	dolby_yuv_mif.src_bit = prm_dolby->core1_src_bit;
	dolby_yuv_mif.src_plan = prm_dolby->core1_src_plane;//5:plane0;6:plane0,1;7:plane0,1,2
	dolby_yuv_mif.src_fmt = prm_dolby->core1_src_fmt;//0:444 1:422 2:420
	dolby_yuv_mif.src_baddr[0] = prm_dolby->core1_src_rbaddr[0];
	dolby_yuv_mif.src_baddr[1] = prm_dolby->core1_src_rbaddr[1];
	dolby_yuv_mif.slc_x_st[0] = 0;
	dolby_yuv_mif.slc_x_ed[0] = core1_hsize - 1;
	dolby_yuv_mif.slc_y_st[0] = 0;
	dolby_yuv_mif.slc_y_ed[0] = (nr_4k1k_vds_en ? core1_vsize * 2 : core1_vsize) - 1;
	dolby_yuv_mif.skip_line = nr_4k1k_vds_en ? 1 : 0;
	dolby_yuv_mif.mmu_mode_en = prm_dolby->core1_mif_mmu_ena;
	dolby_yuv_mif.blk_mode = top1_vd_info.blk_mode;
	dolby_yuv_mif.burst_len = top1_vd_info.burst_len;
	dolby_yuv_mif.endian = top1_vd_info.endian;
	dolby_yuv_mif.linear_mode = top1_vd_info.linear_mode;

	cfg_viu_pix_rdmif(&dolby_yuv_mif, 3, reset || force_update);

	if (reset) {
		if (sw_trigger_top1(prm_dolby))
			vsync_sel = 1;
		top1_ctrl0 = VSYNC_RD_TOP1_REG(T6W_DOLBY_TOP1_CTRL0);
		top1_ctrl0 &= 0xFCFFFFFF;
		top1_ctrl0 |= vsync_sel << 24;
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_CTRL0, top1_ctrl0);
	}
	//if(vsync_sel==1)
	//	  VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP1_CTRL0, Rd(T6W_DOLBY_TOP1_CTRL0)|
	//						   (dolby5_top1->reg_frm_rst<<30)|
	//						   (vsync_sel<<24));
}

static void dolby5_corep_ini(struct prm_dolby_top *prm_dolby, bool reset, bool force_update)
{
	u32 corep_hsize = prm_dolby->corep_hsize;
	u32 corep_vsize = prm_dolby->corep_vsize;
	u32 core2_hsize = prm_dolby->core2_hsize_of_corep;
	u32 core2_vsize = prm_dolby->core2_vsize_of_corep;
	/*24bit each pixel,align to 64byte(512bit).how many 128bit each line*/
	u32 pyramid_stride = ((24 * corep_hsize + 511) >> 9) * 4;
	u32 corep_up_wbaddr = prm_dolby->corep_up_wbaddr[0];/*pingpang buf*/
	u32 corep_py_level = prm_dolby->byps_mode == 1 ?
						2 : prm_dolby->corep_py_level;
	/*core1b and corep connect directly, no need pingpang buf*/
	dma_addr_t py_baddr[7] = {prm_dolby->core1_pyr_wbaddr[0],
							prm_dolby->core1_pyr_wbaddr[1],
							prm_dolby->core1_pyr_wbaddr[2],
							prm_dolby->core1_pyr_wbaddr[3],
							prm_dolby->core1_pyr_wbaddr[4],
							prm_dolby->core1_pyr_wbaddr[5],
							prm_dolby->core1_pyr_wbaddr[6]};
	bool py_start_sel = corep_py_level < 2; //1:hw 0:reg;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("corep size %d %d %d %d,byps_mode %d,wbaddr %x,level %s,%s\n",
					corep_hsize, corep_vsize,
					core2_hsize, core2_vsize,
					prm_dolby->byps_mode,
					corep_up_wbaddr,
					prm_dolby->corep_py_level == 0 ?
					"6" : (prm_dolby->corep_py_level == 1 ? "7" : "0"),
					corep_py_level == 0 ?
					"6" : (corep_py_level == 1 ? "7" : "0"));

	VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYUP_WMIF_CTRL4, corep_up_wbaddr >> 4);

	if (reset) {
		//(reg_corep_md_pgm_over & 0x1) << 1 | reg_corep_cfg_pyramid_en
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYUP_CTRL0, 0x800003);
		//py weight0,weight1
		//1  256     1024
		//2  256     1024
		//3  256     1024
		//4  256     1024
		//5  256     1024
		//6  256     1024
		//7   64     1024
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYR1_WGT, prm_dolby->py_weight[0]);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYR2_WGT, prm_dolby->py_weight[1]);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYR3_WGT, prm_dolby->py_weight[2]);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYR4_WGT, prm_dolby->py_weight[3]);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYR5_WGT, prm_dolby->py_weight[4]);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYR6_WGT, prm_dolby->py_weight[5]);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYR7_WGT, prm_dolby->py_weight[6]);

		//pyd_stride[7] = {80,40,20,12,8,4,4}
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_STRIDE12, (40 << 16) | 80);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_STRIDE34, (12 << 16) | 20);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_STRIDE56, (4 << 16) | 8);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_STRIDE7, 4);
		//rdmif addr
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_BADDR1, py_baddr[0] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_BADDR2, py_baddr[1] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_BADDR3, py_baddr[2] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_BADDR4, py_baddr[3] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_BADDR5, py_baddr[4] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_BADDR6, py_baddr[5] >> 4);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRD_BADDR7, py_baddr[6] >> 4);
	}
	if (reset || force_update) {
		//size
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRES, (corep_vsize << 16) | corep_hsize);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_VDRES, (core2_vsize << 16) | core2_hsize);
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP2_PYUP_WMIF_CTRL3, pyramid_stride, 0, 13);

		//py_lvl
		VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP2_PYRD_CTRL,
		(py_start_sel << 2) | corep_py_level, 0, 3);//{reg_py_int_tri[2],reg_py_level[1:0]}
	}
}

bool process_aoi(u32 core2_slice_num,
					bool core2_slc_mode,
					u32 core2_v,
					u32 core2_h,
					u32 core2_h_d4,
					u32 core2_h_slc[4],
					u32 core2_hbgn_slc[4],
					u32 core2_hend_slc[4],
					u32 core2_h_slc_s0[4],
					u32 core2_h_slc_s1[4],
					u32 dv_pad_lft[4],
					u32 dv_pad_rgt[4],
					u32 ext_pad)
{
	u32 roi[4];/*original roi axis based on frame:top, bottom, left, right*/
	static u32 last_roi[4];
	u32 roi_l[2][4];/*4slice left axis based on slice*/
	u32 roi_r[2][4];/*4slice right axis based on slice*/
	bool need_update_roi = false;
	u32 core2_h_d4_x2 = 0;
	u32 core2_h_d4_x3 = 0;
	u32 core2_h_d8 = 0;
	u32 core2_h_d8_x3 = 0;
	u32 core2_h_d8_x5 = 0;

	core2_h_d4_x2 = (core2_h_d4 << 1);
	core2_h_d4_x3 = (core2_h_d4 << 1) + core2_h_d4;
	core2_h_d8 = core2_h_d4 >> 1;
	core2_h_d8_x3 = core2_h_d8 * 3;
	core2_h_d8_x5 = core2_h_d8 * 5;

	/*roi slc*/
	roi[0] = 0;
	roi[1] = core2_v;
	roi[2] = 0;
	roi[3] = core2_h;
	roi_l[0][0] = 0;
	roi_l[0][1] = 0;
	roi_l[0][2] = 0;
	roi_l[0][3] = 0;
	roi_r[0][0] = core2_h_slc_s0[0] - 1;
	roi_r[0][1] = core2_h_slc_s0[1] - 1;
	roi_r[0][2] = core2_h_slc_s0[2] - 1;
	roi_r[0][3] = core2_h_slc_s0[3] - 1;
	roi_l[1][0] = 0;
	roi_l[1][1] = 0;
	roi_l[1][2] = 0;
	roi_l[1][3] = 0;
	roi_r[1][0] = core2_h_slc_s1[0] - 1;
	roi_r[1][1] = core2_h_slc_s1[1] - 1;
	roi_r[1][2] = core2_h_slc_s1[2] - 1;
	roi_r[1][3] = core2_h_slc_s1[3] - 1;
	if (tv_hw5_setting && debug_disable_aoi == 0) {
		/*get ori size*/
		roi[0] = (tv_hw5_setting->top2_reg[89]) & 0xffff;/*top*/
		roi[1] = (tv_hw5_setting->top2_reg[89] >> 16) & 0xffff;/*bottom*/
		roi[2] = (tv_hw5_setting->top2_reg[94]) & 0xffff;/*left*/
		roi[3] = (tv_hw5_setting->top2_reg[94] >> 16) & 0xffff;/*right*/

		if (debug_dolby & 0x400000)
			pr_info("ori-roi: top %d,bottom %d,left %d,right %d\n",
				roi[0], roi[1], roi[2], roi[3]);

		if (debug_h_aoi != 0xFFFFFFFF) {
			roi[2] = debug_h_aoi & 0xffff;
			roi[3] = (debug_h_aoi & 0xffff0000) >> 16;
			need_update_roi = true;
			if (debug_dolby & 0x400000)
				pr_info("debug-roi: left %d,right %d\n",
					roi[2], roi[3]);
		}
		if (debug_v_aoi != 0xFFFFFFFF) {
			roi[0] = debug_v_aoi & 0xffff;
			roi[1] = (debug_v_aoi & 0xffff0000) >> 16;
			need_update_roi = true;
			if (debug_dolby & 0x400000)
				pr_info("debug-roi: top %d, bottom %d\n",
					roi[0], roi[1]);
		}
		/*always update slice roi value in slice/2ppc mode*/
		if ((core2_slice_num || core2_slc_mode)/* &&*/
			/*(roi[2] != 0 || roi[3] != core2_h - 1 ||*/
			/*roi[2] != last_roi[2] || roi[3] != last_roi[3])*/) {
			need_update_roi = true;
		}
		roi_l[0][0] = roi[2];/*default value*/
		roi_l[0][1] = roi[2];
		roi_l[0][2] = roi[2];
		roi_l[0][3] = roi[2];
		roi_l[1][0] = roi[2];
		roi_l[1][1] = roi[2];
		roi_l[1][2] = roi[2];
		roi_l[1][3] = roi[2];
		roi_r[0][0] = roi[3];
		roi_r[0][1] = roi[3];
		roi_r[0][2] = roi[3];
		roi_r[0][3] = roi[3];
		roi_r[1][0] = roi[3];
		roi_r[1][1] = roi[3];
		roi_r[1][2] = roi[3];
		roi_r[1][3] = roi[3];

		last_roi[2] = roi[2];
		last_roi[3] = roi[3];
	}
	if (need_update_roi) {
		if (core2_slc_mode == 0) {/*1ppc*/
			if (core2_slice_num == 2) {
				/*left*/
				if (roi[2] >= core2_h_d4_x2) {/*left line in s1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s1 core2_0\n");
					roi_l[0][0] = core2_h_slc[0] - 1;
					roi_l[0][1] = roi[2] - core2_hbgn_slc[1];
				} else {/*left line in s0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_0\n");
					roi_l[0][0] = roi[2] - core2_hbgn_slc[0];
					roi_l[0][1] = 0;
				}
				/*right*/
				if (roi[3] >= core2_h_d4_x2) {/*right line in s1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s1 core2_0\n");
					roi_r[0][0] = core2_h_slc[0] - 1;
					roi_r[0][1] = core2_h_slc[1] - 1 -
						(core2_hend_slc[1] - roi[3]);
				} else {/*right line in s0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_0\n");
					roi_r[0][0] = core2_h_slc[0] - 1 -
						(core2_hend_slc[0] - roi[3]);
					roi_r[0][1] = 0;
				}
			} else if (core2_slice_num == 4) {
				/*left*/
				if (roi[2] >= core2_h_d4_x3) {/*left line in s3*/
					if (debug_dolby & 0x400000)
						pr_info("left in s3 core2_0\n");
					roi_l[0][0] = core2_h_slc[0] - 1;
					roi_l[0][1] = core2_h_slc[1] - 1;
					roi_l[0][2] = core2_h_slc[2] - 1;
					roi_l[0][3] = roi[2] - core2_hbgn_slc[3];
				} else if (roi[2] >= core2_h_d4_x2) {/*left line in s2*/
					if (debug_dolby & 0x400000)
						pr_info("left in s2 core2_0\n");
					roi_l[0][0] = core2_h_slc[0] - 1;
					roi_l[0][1] = core2_h_slc[1] - 1;
					roi_l[0][2] = roi[2] - core2_hbgn_slc[2];
					roi_l[0][3] = 0;
				} else if (roi[2] >= core2_h_d4) {/*left line in s1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s1 core2_0\n");
					roi_l[0][0] = core2_h_slc[0] - 1;
					roi_l[0][1] = roi[2] - core2_hbgn_slc[1];
					roi_l[0][2] = 0;
					roi_l[0][3] = 0;
				} else {/*left line in s0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_0\n");
					roi_l[0][0] = roi[2] - core2_hbgn_slc[0];
					roi_l[0][1] = 0;
					roi_l[0][2] = 0;
					roi_l[0][3] = 0;
				}
				/*right*/
				if (roi[3] >= core2_h_d4_x3) {/*right line in s3*/
					if (debug_dolby & 0x400000)
						pr_info("right in s3 core2_0\n");
					roi_r[0][0] = core2_h_slc[0] - 1;
					roi_r[0][1] = core2_h_slc[1] - 1;
					roi_r[0][2] = core2_h_slc[2] - 1;
					roi_r[0][3] = core2_h_slc[3] - 1 -
						(core2_hend_slc[3] - roi[3]);
				} else if (roi[3] >= core2_h_d4_x2) {/*right line in s2*/
					if (debug_dolby & 0x400000)
						pr_info("right in s2 core2_0\n");
					roi_r[0][0] = core2_h_slc[0] - 1;
					roi_r[0][1] = core2_h_slc[1] - 1;
					roi_r[0][2] = core2_h_slc[2] - 1 -
						(core2_hend_slc[2] - roi[3]);
					roi_r[0][3] = 0;
				} else if (roi[3] >= core2_h_d4) {/*right line in s1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s1 core2_0\n");
					roi_r[0][0] = core2_h_slc[0] - 1;
					roi_r[0][1] = core2_h_slc[1] - 1 -
						(core2_hend_slc[1] - roi[3]);
					roi_r[0][2] = 0;
					roi_r[0][3] = 0;
				} else {/*right line in s0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_0\n");
					roi_r[0][0] = core2_h_slc[1] - 1 -
						(core2_hend_slc[0] - roi[3]);
					roi_r[0][1] = 0;
					roi_r[0][2] = 0;
					roi_r[0][3] = 0;
				}
			}
		} else {/*2ppc*/
			if (core2_slice_num == 1) {/*2ppc overlap*/
				/*left*/
				if (roi[2] >= core2_h_d4_x2) {/*left line in s0 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_1\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = roi[2] - core2_h_d4_x2 + 96;
				} else {/*left line in s0 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_0\n");
					roi_l[0][0] = roi[2];
					roi_l[1][0] = 0;
				}
				/*right*/
				if (roi[3] >= core2_h_d4_x2) {/*right line in s0 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_1\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - (core2_h - roi[3]);
				} else {/*right line in s0 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_0\n");
					roi_r[0][0] = roi[3];
					roi_r[1][0] = 0;
				}
			} else if (core2_slice_num == 2) {/*slice overlap+2ppc overlap*/
				/*left*/
				if (roi[2] >= core2_h - 1 - core2_h_slc[1] / 2) {
					/*left in s1 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s1 core2_1\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = core2_h_slc_s0[1] - 1;
					roi_l[1][1] = roi[2] -
						(core2_h - 1 - core2_h_slc[1] / 2) + 96;
				} else if (roi[2] >= core2_h_d4_x2) {/*left in s1 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s1 core2_0\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = roi[2] - core2_h_d4_x2 + dv_pad_lft[1] +
						ext_pad;
					roi_l[1][1] = 0;
				} else if (roi[2] >= core2_h_slc[0] / 2) {/*left in s0 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_1\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = roi[2] - core2_h_slc[0] / 2 + 96;
					roi_l[0][1] = 0;
					roi_l[1][1] = 0;
				} else {/*left in s0 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_0\n");
					roi_l[0][0] = roi[2];
					roi_l[1][0] = 0;
					roi_l[0][1] = 0;
					roi_l[1][1] = 0;
				}
				/*right*/
				if (roi[3] >= core2_h - 1 - core2_h_slc[1] / 2) {
					/*right in s1 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s1 core2_1\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1;
					roi_r[1][1] = core2_h_slc_s1[1] - 1 -
						(core2_h - 1 - roi[3]);
				} else if (roi[3] >= core2_h_d4_x2) {/*right in s1 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s1 core2_0\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1 -
					(core2_h - 1 - roi[3] - core2_h_slc[1] / 2) - 96;
					roi_r[1][1] = 0;
				} else if (roi[3] >= core2_h_slc[0] / 2) {/*right in s0 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_1\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = roi[3] - core2_h_slc[0] / 2 + 96;
					roi_r[0][1] = 0;
					roi_r[1][1] = 0;
				} else {/*right in s0 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_0\n");
					roi_r[0][0] = roi[3];
					roi_r[1][0] = 0;
					roi_r[0][1] = 0;
					roi_r[1][1] = 0;
				}
			} else if (core2_slice_num == 4) {/*slice overlap+2ppc overlap*/
				if (roi[2] >= core2_h - 1 - core2_h_slc[3] / 2) {
					/*left in s3 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s3 core2_1\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = core2_h_slc_s0[1] - 1;
					roi_l[1][1] = core2_h_slc_s1[1] - 1;
					roi_l[0][2] = core2_h_slc_s0[2] - 1;
					roi_l[1][2] = core2_h_slc_s1[2] - 1;
					roi_l[0][3] = core2_h_slc_s0[3] - 1;
					roi_l[1][3] = roi[2] -
						(core2_h - 1 - core2_h_slc[3] / 2) + 96;
				} else if (roi[2] >= core2_h_d4_x3) {/*left in s3 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s3 core2_0\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = core2_h_slc_s0[1] - 1;
					roi_l[1][1] = core2_h_slc_s1[1] - 1;
					roi_l[0][2] = core2_h_slc_s0[2] - 1;
					roi_l[1][2] = core2_h_slc_s1[2] - 1;
					roi_l[0][3] = roi[2] - core2_h_d4_x3 + dv_pad_lft[3] +
						ext_pad;
					roi_l[1][3] = 0;
				} else if (roi[2] >= core2_h_d8_x5) {/*left in s2 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s2 core2_1\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = core2_h_slc_s0[1] - 1;
					roi_l[1][1] = core2_h_slc_s1[1] - 1;
					roi_l[0][2] = core2_h_slc_s0[2] - 1;
					roi_l[1][2] = roi[2] - core2_h_d8_x5 + 96;
					roi_l[0][3] = 0;
					roi_l[1][3] = 0;
				} else if (roi[2] >= core2_h_d4_x2) {/*left in s2 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s2 core2_0\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = core2_h_slc_s0[1] - 1;
					roi_l[1][1] = core2_h_slc_s1[1] - 1;
					roi_l[0][2] = roi[2] - core2_h_d4_x2 + dv_pad_lft[2] +
						ext_pad;
					roi_l[1][2] = 0;
					roi_l[0][3] = 0;
					roi_l[1][3] = 0;
				} else if (roi[2] >= core2_h_d8_x3) {/*left in s1 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s1 core2_1\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = core2_h_slc_s0[1] - 1;
					roi_l[1][1] = roi[2] - core2_h_d8_x3 + 96;
					roi_l[0][2] = 0;
					roi_l[1][2] = 0;
					roi_l[0][3] = 0;
					roi_l[1][3] = 0;
				} else if (roi[2] >= core2_h_d4) {/*left in s1 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s1 core2_0\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = core2_h_slc_s1[0] - 1;
					roi_l[0][1] = roi[2] - core2_h_d4 + dv_pad_lft[1] + ext_pad;
					roi_l[1][1] = 0;
					roi_l[0][2] = 0;
					roi_l[1][2] = 0;
					roi_l[0][3] = 0;
					roi_l[1][3] = 0;
				} else if (roi[2] >= core2_h_slc[0] / 2) {/*left in s0 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_1\n");
					roi_l[0][0] = core2_h_slc_s0[0] - 1;
					roi_l[1][0] = roi[2] - core2_h_slc[0] / 2 + 96;
					roi_l[0][1] = 0;
					roi_l[1][1] = 0;
					roi_l[0][2] = 0;
					roi_l[1][2] = 0;
					roi_l[0][3] = 0;
					roi_l[1][3] = 0;
				} else {/*left in s0 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("left in s0 core2_0\n");
					roi_l[0][0] = roi[2];
					roi_l[1][0] = 0;
					roi_l[0][1] = 0;
					roi_l[1][1] = 0;
					roi_l[0][2] = 0;
					roi_l[1][2] = 0;
					roi_l[0][3] = 0;
					roi_l[1][3] = 0;
				}
				if (roi[3] >= core2_h - 1 - core2_h_slc[3] / 2) {
					/*right in s3 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s3 core2_1\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1;
					roi_r[1][1] = core2_h_slc_s1[1] - 1;
					roi_r[0][2] = core2_h_slc_s0[2] - 1;
					roi_r[1][2] = core2_h_slc_s1[2] - 1;
					roi_r[0][3] = core2_h_slc_s0[3] - 1;
					roi_r[1][3] = core2_h_slc_s1[3] - 1 -
						(core2_h - 1 - roi[3]);
				} else if (roi[3] >= core2_h_d4_x3) {/*right in s3 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s3 core2_0\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1;
					roi_r[1][1] = core2_h_slc_s1[1] - 1;
					roi_r[0][2] = core2_h_slc_s0[2] - 1;
					roi_r[1][2] = core2_h_slc_s1[2] - 1;
					roi_r[0][3] = core2_h_slc_s0[3] - 1 -
					(core2_h - 1 - roi[3] - core2_h_slc[3] / 2) - 96;
					roi_r[1][3] = 0;
				} else if (roi[3] >= core2_h_d8_x5) {/*right in s2 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s2 core2_1\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1;
					roi_r[1][1] = core2_h_slc_s1[1] - 1;
					roi_r[0][2] = core2_h_slc_s0[2] - 1;
					roi_r[1][2] = core2_h_slc_s1[2] - 1 -
					(core2_h_d4_x3 - 1  - roi[3]) - dv_pad_rgt[2] - ext_pad;
					roi_r[0][3] = 0;
					roi_r[1][3] = 0;
				} else if (roi[3] >= core2_h_d4_x2) {/*right in s2 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s2 core2_0\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1;
					roi_r[1][1] = core2_h_slc_s1[1] - 1;
					roi_r[0][2] = core2_h_slc_s0[0] - 1 -
						(core2_h_d8_x5 - 1 - roi[3]) - 96;
					roi_r[1][2] = 0;
					roi_r[0][3] = 0;
					roi_r[1][3] = 0;
				} else if (roi[3] >= core2_h_d8_x3) {/*right in s1 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s1 core2_1\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1;
					roi_r[1][1] = core2_h_slc_s1[1] - 1 -
					(core2_h_d4_x2 - 1 - roi[3]) - dv_pad_rgt[1] - ext_pad;
					roi_r[0][2] = 0;
					roi_r[1][2] = 0;
					roi_r[0][3] = 0;
					roi_r[1][3] = 0;
				} else if (roi[3] >= core2_h_d4) {/*right line in s1 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s1 core2_0\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = core2_h_slc_s1[0] - 1;
					roi_r[0][1] = core2_h_slc_s0[1] - 1 -
						(core2_h_d8_x3 - 1 - roi[3]) - 96;
					roi_r[1][1] = 0;
					roi_r[0][2] = 0;
					roi_r[1][2] = 0;
					roi_r[0][3] = 0;
					roi_r[1][3] = 0;
				} else if (roi[3] >= core2_h_slc[0] / 2) {/*r in s0 core2_1*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_1\n");
					roi_r[0][0] = core2_h_slc_s0[0] - 1;
					roi_r[1][0] = roi[3] - core2_h_slc[0] / 2 + 96;
					roi_r[0][1] = 0;
					roi_r[1][1] = 0;
					roi_r[0][2] = 0;
					roi_r[1][2] = 0;
					roi_r[0][3] = 0;
					roi_r[1][3] = 0;
				} else {/*right line in s0 core2_0*/
					if (debug_dolby & 0x400000)
						pr_info("right in s0 core2_0\n");
					roi_r[0][0] = roi[3];
					roi_r[1][0] = 0;
					roi_r[0][1] = 0;
					roi_r[1][1] = 0;
					roi_r[0][2] = 0;
					roi_r[1][2] = 0;
					roi_r[0][3] = 0;
					roi_r[1][3] = 0;
				}
			}
		}
	}
	roi_final[0][0] = roi_r[0][0] << 16 | roi_l[0][0];
	roi_final[0][1] = roi_r[0][1] << 16 | roi_l[0][1];
	roi_final[0][2] = roi_r[0][2] << 16 | roi_l[0][2];
	roi_final[0][3] = roi_r[0][3] << 16 | roi_l[0][3];
	roi_final[1][0] = roi_r[1][0] << 16 | roi_l[1][0];
	roi_final[1][1] = roi_r[1][1] << 16 | roi_l[1][1];
	roi_final[1][2] = roi_r[1][2] << 16 | roi_l[1][2];
	roi_final[1][3] = roi_r[1][3] << 16 | roi_l[1][3];
	roi_final_y = roi[1] << 16 | roi[0];

	if (debug_dolby & 0x400000) {
		pr_info("core2_0 roi:ori(%d-%d-%d-%d),lft:(%d-%d-%d-%d),rgt:(%d-%d-%d-%d),fin:(%x-%x-%x-%x)\n",
			roi[0], roi[1], roi[2], roi[3],
			roi_l[0][0], roi_l[0][1], roi_l[0][2], roi_l[0][3],
			roi_r[0][0], roi_r[0][1], roi_r[0][2], roi_r[0][3],
			roi_final[0][0], roi_final[0][1], roi_final[0][2], roi_final[0][3]);
		if (core2_slc_mode)
			pr_info("core2_1 roi:ori(%d-%d-%d-%d),lft:(%d-%d-%d-%d),rgt:(%d-%d-%d-%d),fin:(%x-%x-%x-%x)\n",
				roi[0], roi[1], roi[2], roi[3],
				roi_l[1][0], roi_l[1][1], roi_l[1][2], roi_l[1][3],
				roi_r[1][0], roi_r[1][1], roi_r[1][2], roi_r[1][3],
				roi_final[1][0], roi_final[1][1], roi_final[1][2], roi_final[1][3]);
	}
	return need_update_roi;
}

static void dolby5_top2_he2(struct prm_dolby_top *prm_dolby,
		u64 *reg_data, bool reset)
{
	//calc slice_start & end
	u32 rdma_size[12] = {166, 258, 258};
	u32 rdma0_num = 1;
	u32 rdma1_num = 1;
	u32 core2_hsize_slc[4];
	u32 core2_hbgn_slc[4];
	u32 core2_hend_slc[4];
	u32 corep_hsize_slc[4];
	u32 corep_hbgn_slc[4];
	u32 corep_hend_slc[4];
	u32 core2_win_hbgn_slc[4];
	u32 core2_win_hend_slc[4];
	u32 dv_pad_lft[4];
	u32 dv_pad_rgt[4];
	/*0:nonr+noaa;1:no_nr+aa;2:nr+no_aa;3:nr+aa*/
	u32 core2_ext_ovlp = prm_dolby->core2_pad_mode;
	u32 core2_slice_num = prm_dolby->core2_slice_num;
	u32 core2_hsize = prm_dolby->core2_hsize;
	u32 core2_vsize = prm_dolby->core2_vsize;
	bool dolby_slice_mode = (core2_slice_num > 1);
	u32 corep_hsize = prm_dolby->corep_hsize;
	u32 corep_vsize = prm_dolby->corep_vsize;
	u32 corep_baddr = prm_dolby->corep_up_wbaddr[1];
	u32 core2_py_level = prm_dolby->core2_py_level;
	u32 corep_scale = (core2_hsize + corep_hsize / 2) / corep_hsize;
	u32 corep_ratio;
	u32 amdv_pad  = dolby_slice_mode ? 96             : 0;//todo dv slc mode ? 32 : 0
	u32 ext_pad = dolby_slice_mode ? core2_ext_ovlp : 0;//todo dv_slc mode & nr/di off
	u32 core2_hsize_d4     = (core2_hsize + 3) / 4;
	u32 core2_hsize_d4_x2 = 0;//(core2_hsize_d4 << 1);
	//u32 core2_hsize_d4_x2 = core2_hsize_d4_d64<<1;
	u32 core2_hsize_d4_x3 = 0;//(core2_hsize_d4 << 1) + core2_hsize_d4;
	//u32 core2_hsize_d4_x3 = core2_hsize_d4_d64<<1+core2_hsize_d4_d64;
	u32 pyramid_stride = ((24 * corep_hsize + 511) >> 9) * 4;
	u32 pyramid_rd_en  = core2_py_level == 2 ? 0 : 1;
	//u32 corep_sft = corep_ratio == 4 ? 2 : corep_ratio == 2 ? 1 : 0;
	u32 *p_reg;
	u64 *p_reg_64;
	int top2_ahb_num = TOP2_REG_NUM;
	static u32 last_core2_hsize;
	static u32 last_core2_vsize;
	static u32 last_corep_hsize;
	static u32 last_corep_vsize;
	static u32 last_core2_slice_num;
	static u32 last_core2_ext_ovlp;
	static u32 last_core2_py_level = 2;
	bool need_update = false;
	u32 top2_ctrl0;
	bool need_update_roi = false;

	if (prm_dolby->frm_hsize_sel == 1)/*16 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 15) / 16 * 16;
	else if (prm_dolby->frm_hsize_sel == 2)/*32 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 31) / 32 * 32;
	else if (prm_dolby->frm_hsize_sel == 3)/*64 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 63) / 64 * 64;
	else if (prm_dolby->frm_hsize_sel == 4)/*128 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 127) / 128 * 128;

	core2_hsize_d4_x2 = (core2_hsize_d4 << 1);
	core2_hsize_d4_x3 = (core2_hsize_d4 << 1) + core2_hsize_d4;

	corep_ratio = corep_scale == 1 ? 0 : corep_scale == 2 ? 1 : corep_scale == 4 ? 2 : 3;

	if (last_core2_hsize != core2_hsize ||
		last_core2_vsize != core2_vsize ||
		last_corep_hsize != corep_hsize ||
		last_corep_vsize != corep_vsize ||
		last_core2_slice_num != core2_slice_num ||
		last_core2_ext_ovlp != core2_ext_ovlp ||
		last_core2_py_level != core2_py_level ||
		!top2_info.core_on || reset)
		need_update = true;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("core2 %dx%d,corep %dx%d,wbaddr %x,update %d,reset %d,ratio %d %d,sel %d\n",
					core2_hsize, core2_vsize, corep_hsize, corep_vsize,
					corep_baddr, need_update, reset, corep_scale, corep_ratio,
					prm_dolby->frm_hsize_sel);

	dv_pad_lft[0] = 0;
	dv_pad_lft[1] = core2_slice_num == 4 ? amdv_pad >> 1 : amdv_pad;
	dv_pad_lft[2] = amdv_pad >> 1;
	dv_pad_lft[3] = amdv_pad;

	dv_pad_rgt[0] = amdv_pad;
	dv_pad_rgt[1] = core2_slice_num == 4 ? amdv_pad >> 1 : 0;
	dv_pad_rgt[2] = amdv_pad >> 1;
	dv_pad_rgt[3] = 0;
	//slc0
	core2_hbgn_slc[0] = 0;
	core2_hend_slc[0] = core2_slice_num == 4 ?
					core2_hsize_d4 + dv_pad_rgt[0] + ext_pad - 1 :
					core2_slice_num == 2 ?
					core2_hsize_d4_x2 + dv_pad_rgt[0] + ext_pad - 1 :
					core2_hsize - 1;
	core2_hsize_slc[0] = core2_hend_slc[0] - core2_hbgn_slc[0] + 1;
	core2_win_hbgn_slc[0] = 0;
	core2_win_hend_slc[0] = core2_hsize_slc[0] - dv_pad_rgt[0] - 1;
	//slc1
	core2_hbgn_slc[1] = core2_slice_num == 4 ?
				core2_hsize_d4 - dv_pad_lft[1] - ext_pad :
				core2_hsize_d4_x2 - dv_pad_lft[1] - ext_pad;
	core2_hend_slc[1] = core2_slice_num == 4 ?
				core2_hsize_d4_x2 + dv_pad_rgt[1] + ext_pad - 1 :
				core2_hsize - 1;
	core2_hsize_slc[1]    = core2_hend_slc[1] - core2_hbgn_slc[1] + 1;
	core2_win_hbgn_slc[1] = dv_pad_lft[1];
	core2_win_hend_slc[1] = core2_hsize_slc[1] - dv_pad_rgt[1] - 1;
	//slc2
	core2_hbgn_slc[2]     = core2_hsize_d4_x2 - dv_pad_lft[2] - ext_pad;
	core2_hend_slc[2]     = core2_hsize_d4_x3 + dv_pad_rgt[2] + ext_pad - 1;
	core2_hsize_slc[2]    = core2_hend_slc[2] - core2_hbgn_slc[2] + 1;
	core2_win_hbgn_slc[2] = dv_pad_lft[2];
	core2_win_hend_slc[2] = core2_hsize_slc[2] - dv_pad_rgt[2] - 1;
	//slc3
	core2_hbgn_slc[3]     = core2_hsize_d4_x3 - dv_pad_lft[3] - ext_pad;
	core2_hend_slc[3]     = core2_hsize - 1;
	core2_hsize_slc[3]    = core2_hend_slc[3] - core2_hbgn_slc[3] + 1;
	core2_win_hbgn_slc[3] = dv_pad_lft[3];
	core2_win_hend_slc[3] = core2_hsize_slc[3] - dv_pad_rgt[3] - 1;

	//pyramid slc
	corep_hsize_slc[0] = core2_hsize_slc[0] >> corep_ratio;
	corep_hsize_slc[1] = core2_hsize_slc[1] >> corep_ratio;
	corep_hsize_slc[2] = core2_hsize_slc[2] >> corep_ratio;
	corep_hsize_slc[3] = core2_hsize_slc[3] >> corep_ratio;

	corep_hbgn_slc[0]  = core2_hbgn_slc[0] >> corep_ratio;
	corep_hbgn_slc[1]  = core2_hbgn_slc[1] >> corep_ratio;
	corep_hbgn_slc[2]  = core2_hbgn_slc[2] >> corep_ratio;
	corep_hbgn_slc[3]  = core2_hbgn_slc[3] >> corep_ratio;

	corep_hend_slc[0]  = core2_hend_slc[0] >> corep_ratio;
	corep_hend_slc[1]  = core2_hend_slc[1] >> corep_ratio;
	corep_hend_slc[2]  = core2_hend_slc[2] >> corep_ratio;
	corep_hend_slc[3]  = core2_hend_slc[3] >> corep_ratio;

	need_update_roi = process_aoi(core2_slice_num, 0, core2_vsize,
			core2_hsize, core2_hsize_d4, core2_hsize_slc,
			core2_hbgn_slc, core2_hend_slc, core2_hsize_slc,
			core2_hsize_slc, dv_pad_lft, dv_pad_rgt, ext_pad);

	if (debug_dolby & 0x400000) {
		pr_info("core2_hsize_d4 %d,pad %d %d,lft(%d-%d-%d-%d),rgt(%d-%d-%d-%d)\n",
			core2_hsize_d4, amdv_pad, ext_pad,
			dv_pad_lft[0], dv_pad_lft[1], dv_pad_lft[2], dv_pad_lft[3],
			dv_pad_rgt[0], dv_pad_rgt[1], dv_pad_rgt[2], dv_pad_rgt[3]);
		pr_info("core2 hsize s0:%d(%d-%d) s1:%d(%d-%d) s2:%d(%d-%d) s3:%d(%d-%d)\n",
			core2_hsize_slc[0], core2_hbgn_slc[0], core2_hend_slc[0],
			core2_hsize_slc[1], core2_hbgn_slc[1], core2_hend_slc[1],
			core2_hsize_slc[2], core2_hbgn_slc[2], core2_hend_slc[2],
			core2_hsize_slc[3], core2_hbgn_slc[3], core2_hend_slc[3]);
		pr_info("corep hsize s0:%d(%d-%d) s1:%d(%d-%d) s2:%d(%d-%d) s3:%d(%d-%d)\n",
			corep_hsize_slc[0], corep_hbgn_slc[0], corep_hend_slc[0],
			corep_hsize_slc[1], corep_hbgn_slc[1], corep_hend_slc[1],
			corep_hsize_slc[2], corep_hbgn_slc[2], corep_hend_slc[2],
			corep_hsize_slc[3], corep_hbgn_slc[3], corep_hend_slc[3]);
	}
	if (need_update) {
		VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WRAP_HSIZE_0,
		((core2_hsize_slc[1] & 0x1fff) << 16) | ((core2_hsize_slc[0] & 0x1fff)));
		VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WRAP_HSIZE_1,
		((core2_hsize_slc[3] & 0x1fff) << 16) | ((core2_hsize_slc[2] & 0x1fff)));
		VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WRAP_CTRL1,
		((dolby_slice_mode & 0x1) << 31) | ((core2_vsize & 0x1fff) << 16));
		if (dolby_slice_mode) {
			VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WIN_CUT_WIN_0,
			((core2_win_hbgn_slc[1] & 0x1fff) << 16) |
			(core2_win_hbgn_slc[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WIN_CUT_WIN_1,
			((core2_win_hbgn_slc[3] & 0x1fff) << 16) |
			(core2_win_hbgn_slc[2] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WIN_CUT_WIN_2,
			((core2_win_hend_slc[1] & 0x1fff) << 16) |
			(core2_win_hend_slc[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WIN_CUT_WIN_3,
			((core2_win_hend_slc[3] & 0x1fff) << 16) |
			(core2_win_hend_slc[2] & 0x1fff));
		}
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_SIZE, (core2_vsize << 16));
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_HSIZE_S0,
			((core2_hsize_slc[1] & 0x1fff) << 16) | ((core2_hsize_slc[0] & 0x1fff)));
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_HSIZE_S1,
			((core2_hsize_slc[3] & 0x1fff) << 16) | ((core2_hsize_slc[2] & 0x1fff)));

		if (enable_top1) {
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RD_PYRES, (corep_vsize << 16));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYRES_SLC_0,
			((corep_hsize_slc[1] & 0x1fff) << 16) | (corep_hsize_slc[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYRES_SLC_1,
			((corep_hsize_slc[3] & 0x1fff) << 16) | (corep_hsize_slc[2] & 0x1fff));

			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HSTART_0,
			((corep_hbgn_slc[1] & 0x1fff) << 16) | (corep_hbgn_slc[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HSTART_1,
			((corep_hbgn_slc[3] & 0x1fff) << 16) | (corep_hbgn_slc[2] & 0x1fff));

			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HEND_0,
			((corep_hend_slc[1] & 0x1fff) << 16) | (corep_hend_slc[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HEND_1,
			((corep_hend_slc[3] & 0x1fff) << 16) | (corep_hend_slc[2] & 0x1fff));

			VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL3, pyramid_stride, 0, 13);
		}
		/*VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_CTRL0, pyramid_rd_en, 5, 1);*/
		top2_ctrl0 = VSYNC_RD_TOP2_REG(T6W_DOLBY_TOP2_CTRL0);
		//pr_dv_dbg("read top2_ctrl0 %x\n", top2_ctrl0);
		top2_ctrl0 &= 0x3FFFFFDF;
		top2_ctrl0 |= pyramid_rd_en << 5;
		//pr_dv_dbg("2   write top2_ctrl0 %x\n", top2_ctrl0);
		/*only write pyramid_rd_en without bit30-31*/
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL0, top2_ctrl0);

		VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_RDMA_CTRL,
			((rdma1_num << 8) | (rdma0_num & 0x3)), 0, 12);
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE0, (rdma_size[0] << 16) | rdma_size[1]);
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE1, (rdma_size[2] << 16) | rdma_size[3]);
	}
	if (enable_top1)
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL4, corep_baddr >> 4);

	if ((dolby_vision_flags & FLAG_CERTIFICATION) || load_fixed_setting) {/*crc*/
		if (load_fixed_setting)
			p_reg_64 = (u64 *)top2_reg_buf;
		else
			p_reg_64 = reg_data;
		if (p_reg_64[574] == 0x000008f400000000)
			p_reg_64[574] = 0x000008f400000001;
	}

	if (hw5_reg_from_file || (test_dv & DEBUG_FIXED_REG) /*fixed reg*/) {
		p_reg = (u32 *)top2_reg_buf;
		top2_ahb_num = top2_reg_num;
	} else {
		p_reg = (u32 *)reg_data;
	}

	dolby5_ahb_reg_config(p_reg, 2, top2_ahb_num, reset, core2_slice_num,
			core2_hsize_slc, corep_hsize_slc, 1, need_update, need_update_roi);

	last_core2_hsize = core2_hsize;
	last_core2_vsize = core2_vsize;
	last_corep_hsize = corep_hsize;
	last_corep_vsize = corep_vsize;
	last_core2_slice_num = core2_slice_num;
	last_core2_ext_ovlp = core2_ext_ovlp;
	last_core2_py_level = core2_py_level;
}

/*for t6x 2core2*/
static void dolby5_top2_he2_slc(struct prm_dolby_top *prm_dolby,
		u64 *reg_data, bool reset)
{
	//calc slice_start & end
	u32 rdma_size[12] = {166, 258, 258};
	u32 rdma0_num = 1;
	u32 rdma1_num = 1;
	u32 core2_hsize_slc[4];
	u32 core2_hbgn_slc[4];
	u32 core2_hend_slc[4];
	u32 corep_hsize_slc[4];
	u32 corep_hbgn_slc[4];
	u32 corep_hend_slc[4];
	u32 core2_win_hbgn_slc[4];
	u32 core2_win_hend_slc[4];
	u32 dv_pad_lft[4];
	u32 dv_pad_rgt[4];
	/*0:nonr+noaa;1:no_nr+aa;2:nr+no_aa;3:nr+aa*/
	u32 core2_ext_ovlp = prm_dolby->core2_pad_mode;
	u32 core2_slice_num = prm_dolby->core2_slice_num;
	u32 core2_hsize = prm_dolby->core2_hsize;
	u32 core2_vsize = prm_dolby->core2_vsize;
	bool dolby_slice_mode = (core2_slice_num > 1);
	u32 corep_hsize = prm_dolby->corep_hsize;
	u32 corep_vsize = prm_dolby->corep_vsize;
	u32 corep_baddr = prm_dolby->corep_up_wbaddr[1];
	u32 core2_py_level = prm_dolby->core2_py_level;
	bool direct_mode = prm_dolby->dpss_direct_mode;
	u32 corep_scale = (core2_hsize + corep_hsize / 2) / corep_hsize;
	u32 corep_ratio;
	u32 amdv_pad  = dolby_slice_mode ? 96             : 0;//todo dv slc mode ? 32 : 0
	u32 ext_pad = dolby_slice_mode ? core2_ext_ovlp : 0;//todo dv_slc mode & nr/di off
	u32 core2_hsize_d4     = (core2_hsize + 3) / 4;
	u32 core2_hsize_d4_x2 = 0;//(core2_hsize_d4 << 1);
	//u32 core2_hsize_d4_x2 = core2_hsize_d4_d64<<1;
	u32 core2_hsize_d4_x3 = 0;//(core2_hsize_d4 << 1) + core2_hsize_d4;
	//u32 core2_hsize_d4_x3 = core2_hsize_d4_d64<<1+core2_hsize_d4_d64;
	u32 pyramid_stride = ((24 * corep_hsize + 511) >> 9) * 4;
	u32 pyramid_rd_en  = core2_py_level == 2 ? 0 : 1;
	//u32 corep_sft = corep_ratio == 4 ? 2 : corep_ratio == 2 ? 1 : 0;
	u32 *p_reg;
	u64 *p_reg_64;
	int top2_ahb_num = TOP2_REG_NUM;
	static u32 last_core2_hsize;
	static u32 last_core2_vsize;
	static u32 last_corep_hsize;
	static u32 last_corep_vsize;
	static u32 last_core2_slice_num;
	static u32 last_core2_ext_ovlp;
	static u32 last_core2_py_level = 2;
	static bool last_direct_mode;
	bool need_update = false;
	u32 top2_ctrl0;
	bool core2_slc_mode = prm_dolby->core2_slc_mode;
	u32 slc_ofst = T6X_CORE2_SLICE_OFF;
	u32 core2_hsize_slc_s0[4];/*for t6x*/
	u32 corep_hsize_slc_s0[4];
	u32 core2_hsize_slc_s1[4];
	u32 corep_hsize_slc_s1[4];
	u32 core2_win_hbgn_slc_s0[4];
	u32 core2_win_hbgn_slc_s1[4];
	u32 core2_win_hend_slc_s0[4];
	u32 core2_win_hend_slc_s1[4];
	u32 corep_hbgn_slc_s0[4];
	u32 corep_hend_slc_s0[4];
	u32 corep_hbgn_slc_s1[4];
	u32 corep_hend_slc_s1[4];
	bool need_update_roi = false;
	bool wrap_ctrl1_bit31;

	if (prm_dolby->frm_hsize_sel == 1)/*16 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 15) / 16 * 16;
	else if (prm_dolby->frm_hsize_sel == 2)/*32 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 31) / 32 * 32;
	else if (prm_dolby->frm_hsize_sel == 3)/*64 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 63) / 64 * 64;
	else if (prm_dolby->frm_hsize_sel == 4)/*128 align*/
		core2_hsize_d4 = (core2_hsize_d4 + 127) / 128 * 128;

	core2_hsize_d4_x2 = (core2_hsize_d4 << 1);
	core2_hsize_d4_x3 = (core2_hsize_d4 << 1) + core2_hsize_d4;

	corep_ratio = corep_scale == 1 ? 0 : corep_scale == 2 ? 1 : corep_scale == 4 ? 2 : 3;

	if (last_core2_hsize != core2_hsize ||
		last_core2_vsize != core2_vsize ||
		last_corep_hsize != corep_hsize ||
		last_corep_vsize != corep_vsize ||
		last_core2_slice_num != core2_slice_num ||
		last_core2_ext_ovlp != core2_ext_ovlp ||
		last_core2_py_level != core2_py_level ||
		last_direct_mode != direct_mode ||
		!top2_info.core_on || reset)
		need_update = true;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("core2_slc %dx%d,corep %dx%d,wbaddr %x,update %d,reset %d,scale %d,dir %d,sel %d\n",
					core2_hsize, core2_vsize, corep_hsize, corep_vsize,
					corep_baddr, need_update, reset, corep_scale, direct_mode,
					prm_dolby->frm_hsize_sel);

	dv_pad_lft[0] = 0;
	dv_pad_lft[1] = core2_slice_num == 4 ? amdv_pad >> 1 : amdv_pad;
	dv_pad_lft[2] = amdv_pad >> 1;
	dv_pad_lft[3] = amdv_pad;

	dv_pad_rgt[0] = amdv_pad;
	dv_pad_rgt[1] = core2_slice_num == 4 ? amdv_pad >> 1 : 0;
	dv_pad_rgt[2] = amdv_pad >> 1;
	dv_pad_rgt[3] = 0;
	//slc0
	core2_hbgn_slc[0] = 0;
	core2_hend_slc[0] = core2_slice_num == 4 ?
					core2_hsize_d4 + dv_pad_rgt[0] + ext_pad - 1 :
					core2_slice_num == 2 ?
					core2_hsize_d4_x2 + dv_pad_rgt[0] + ext_pad - 1 :
					core2_hsize - 1;
	core2_hsize_slc[0] = core2_hend_slc[0] - core2_hbgn_slc[0] + 1;
	core2_win_hbgn_slc[0] = 0;
	core2_win_hend_slc[0] = core2_hsize_slc[0] - dv_pad_rgt[0] - 1;
	//slc1
	core2_hbgn_slc[1] = core2_slice_num == 4 ?
				core2_hsize_d4 - dv_pad_lft[1] - ext_pad :
				core2_hsize_d4_x2 - dv_pad_lft[1] - ext_pad;
	core2_hend_slc[1] = core2_slice_num == 4 ?
				core2_hsize_d4_x2 + dv_pad_rgt[1] + ext_pad - 1 :
				core2_hsize - 1;
	core2_hsize_slc[1]    = core2_hend_slc[1] - core2_hbgn_slc[1] + 1;
	core2_win_hbgn_slc[1] = dv_pad_lft[1];
	core2_win_hend_slc[1] = core2_hsize_slc[1] - dv_pad_rgt[1] - 1;
	//slc2
	core2_hbgn_slc[2]     = core2_hsize_d4_x2 - dv_pad_lft[2] - ext_pad;
	core2_hend_slc[2]     = core2_hsize_d4_x3 + dv_pad_rgt[2] + ext_pad - 1;
	core2_hsize_slc[2]    = core2_hend_slc[2] - core2_hbgn_slc[2] + 1;
	core2_win_hbgn_slc[2] = dv_pad_lft[2];
	core2_win_hend_slc[2] = core2_hsize_slc[2] - dv_pad_rgt[2] - 1;
	//slc3
	core2_hbgn_slc[3]     = core2_hsize_d4_x3 - dv_pad_lft[3] - ext_pad;
	core2_hend_slc[3]     = core2_hsize - 1;
	core2_hsize_slc[3]    = core2_hend_slc[3] - core2_hbgn_slc[3] + 1;
	core2_win_hbgn_slc[3] = dv_pad_lft[3];
	core2_win_hend_slc[3] = core2_hsize_slc[3] - dv_pad_rgt[3] - 1;

	//pyramid slc
	corep_hsize_slc[0] = core2_hsize_slc[0] >> corep_ratio;
	corep_hsize_slc[1] = core2_hsize_slc[1] >> corep_ratio;
	corep_hsize_slc[2] = core2_hsize_slc[2] >> corep_ratio;
	corep_hsize_slc[3] = core2_hsize_slc[3] >> corep_ratio;

	corep_hbgn_slc[0]  = core2_hbgn_slc[0] >> corep_ratio;
	corep_hbgn_slc[1]  = core2_hbgn_slc[1] >> corep_ratio;
	corep_hbgn_slc[2]  = core2_hbgn_slc[2] >> corep_ratio;
	corep_hbgn_slc[3]  = core2_hbgn_slc[3] >> corep_ratio;

	corep_hend_slc[0]  = core2_hend_slc[0] >> corep_ratio;
	corep_hend_slc[1]  = core2_hend_slc[1] >> corep_ratio;
	corep_hend_slc[2]  = core2_hend_slc[2] >> corep_ratio;
	corep_hend_slc[3]  = core2_hend_slc[3] >> corep_ratio;

	core2_hsize_slc_s0[0] = core2_slc_mode ?
		(core2_hsize_slc[0] >> 1) + 96 : core2_hsize_slc[0];
	core2_hsize_slc_s0[1] = core2_slc_mode ?
		(core2_hsize_slc[1] >> 1) + 96 : core2_hsize_slc[1];
	core2_hsize_slc_s0[2] = core2_slc_mode ?
		(core2_hsize_slc[2] >> 1) + 96 : core2_hsize_slc[2];
	core2_hsize_slc_s0[3] = core2_slc_mode ?
		(core2_hsize_slc[3] >> 1) + 96 : core2_hsize_slc[3];

	corep_hsize_slc_s0[0] = core2_slc_mode ?
		(corep_hsize_slc[0] >> 1) + (96 >> corep_ratio) : corep_hsize_slc[0];
	corep_hsize_slc_s0[1] = core2_slc_mode ?
		(corep_hsize_slc[1] >> 1) + (96 >> corep_ratio) : corep_hsize_slc[1];
	corep_hsize_slc_s0[2] = core2_slc_mode ?
		(corep_hsize_slc[2] >> 1) + (96 >> corep_ratio) : corep_hsize_slc[2];
	corep_hsize_slc_s0[3] = core2_slc_mode ?
		(corep_hsize_slc[3] >> 1) + (96 >> corep_ratio) : corep_hsize_slc[3];

	core2_hsize_slc_s1[0] = core2_hsize_slc_s0[0];
	core2_hsize_slc_s1[1] = core2_hsize_slc_s0[1];
	core2_hsize_slc_s1[2] = core2_hsize_slc_s0[2];
	core2_hsize_slc_s1[3] = core2_hsize_slc_s0[3];
	corep_hsize_slc_s1[0] = corep_hsize_slc_s0[0];
	corep_hsize_slc_s1[1] = corep_hsize_slc_s0[1];
	corep_hsize_slc_s1[2] = corep_hsize_slc_s0[2];
	corep_hsize_slc_s1[3] = corep_hsize_slc_s0[3];

	core2_win_hbgn_slc_s0[0] = 0;//core2_win_hbgn_slc[0];
	core2_win_hbgn_slc_s0[1] = 0;//core2_win_hbgn_slc[1];
	core2_win_hbgn_slc_s0[2] = 0;//core2_win_hbgn_slc[2];
	core2_win_hbgn_slc_s0[3] = 0;//core2_win_hbgn_slc[3];
	core2_win_hend_slc_s0[0] = (core2_hsize_slc[0] >> 1) - 1;
	core2_win_hend_slc_s0[1] = (core2_hsize_slc[1] >> 1) - 1;
	core2_win_hend_slc_s0[2] = (core2_hsize_slc[2] >> 1) - 1;
	core2_win_hend_slc_s0[3] = (core2_hsize_slc[3] >> 1) - 1;

	core2_win_hbgn_slc_s1[0] = 96;
	core2_win_hbgn_slc_s1[1] = 96;
	core2_win_hbgn_slc_s1[2] = 96;
	core2_win_hbgn_slc_s1[3] = 96;
	core2_win_hend_slc_s1[0] = (core2_hsize_slc[0] >> 1) + 95;
	core2_win_hend_slc_s1[1] = (core2_hsize_slc[1] >> 1) + 95;
	core2_win_hend_slc_s1[2] = (core2_hsize_slc[2] >> 1) + 95;
	core2_win_hend_slc_s1[3] = (core2_hsize_slc[3] >> 1) + 95;

	corep_hbgn_slc_s0[0] = corep_hbgn_slc[0];
	corep_hbgn_slc_s0[1] = corep_hbgn_slc[1];
	corep_hbgn_slc_s0[2] = corep_hbgn_slc[2];
	corep_hbgn_slc_s0[3] = corep_hbgn_slc[3];

	corep_hend_slc_s0[0] = core2_slc_mode ?
		corep_hbgn_slc[0] + corep_hsize_slc_s1[0] - 1 : corep_hend_slc[0];
	corep_hend_slc_s0[1] = core2_slc_mode ?
		corep_hbgn_slc[1] + corep_hsize_slc_s1[1] - 1 : corep_hend_slc[1];
	corep_hend_slc_s0[2] = core2_slc_mode ?
		corep_hbgn_slc[2] + corep_hsize_slc_s1[2] - 1 : corep_hend_slc[2];
	corep_hend_slc_s0[3] = core2_slc_mode ?
		corep_hbgn_slc[3] + corep_hsize_slc_s1[3] - 1 : corep_hend_slc[3];

	corep_hbgn_slc_s1[0] = corep_hbgn_slc[0] +
					(corep_hsize_slc[0] >> 1) - (96 >> corep_ratio);
	corep_hbgn_slc_s1[1] = corep_hbgn_slc[1] +
					(corep_hsize_slc[1] >> 1) - (96 >> corep_ratio);
	corep_hbgn_slc_s1[2] = corep_hbgn_slc[2] +
					(corep_hsize_slc[2] >> 1) - (96 >> corep_ratio);
	corep_hbgn_slc_s1[3] = corep_hbgn_slc[3] +
					(corep_hsize_slc[3] >> 1) - (96 >> corep_ratio);

	corep_hend_slc_s1[0] = corep_hend_slc[0];
	corep_hend_slc_s1[1] = corep_hend_slc[1];
	corep_hend_slc_s1[2] = corep_hend_slc[2];
	corep_hend_slc_s1[3] = corep_hend_slc[3];

	need_update_roi = process_aoi(core2_slice_num, core2_slc_mode,
				core2_vsize, core2_hsize, core2_hsize_d4, core2_hsize_slc,
				core2_hbgn_slc, core2_hend_slc, core2_hsize_slc_s0,
				core2_hsize_slc_s1, dv_pad_lft, dv_pad_rgt, ext_pad);

	if (debug_dolby & 0x400000) {
		pr_info("core2_hsize_d4 %d,pad %d %d,lft(%d-%d-%d-%d),rgt(%d-%d-%d-%d)\n",
			core2_hsize_d4, amdv_pad, ext_pad,
			dv_pad_lft[0], dv_pad_lft[1], dv_pad_lft[2], dv_pad_lft[3],
			dv_pad_rgt[0], dv_pad_rgt[1], dv_pad_rgt[2], dv_pad_rgt[3]);
		pr_info("core2 hsize s0:%d(%d-%d) s1:%d(%d-%d) s2:%d(%d-%d) s3:%d(%d-%d)\n",
			core2_hsize_slc[0], core2_hbgn_slc[0], core2_hend_slc[0],
			core2_hsize_slc[1], core2_hbgn_slc[1], core2_hend_slc[1],
			core2_hsize_slc[2], core2_hbgn_slc[2], core2_hend_slc[2],
			core2_hsize_slc[3], core2_hbgn_slc[3], core2_hend_slc[3]);
		pr_info("corep hsize s0:%d(%d-%d) s1:%d(%d-%d) s2:%d(%d-%d) s3:%d(%d-%d)\n",
			corep_hsize_slc[0], corep_hbgn_slc[0], corep_hend_slc[0],
			corep_hsize_slc[1], corep_hbgn_slc[1], corep_hend_slc[1],
			corep_hsize_slc[2], corep_hbgn_slc[2], corep_hend_slc[2],
			corep_hsize_slc[3], corep_hbgn_slc[3], corep_hend_slc[3]);
		pr_info("core2_0 hsize s0:%d s1:%d s2:%d s3:%d\n",
			core2_hsize_slc_s0[0],
			core2_hsize_slc_s0[1],
			core2_hsize_slc_s0[2],
			core2_hsize_slc_s0[3]);
		pr_info("core2_1 hsize s0:%d s1:%d s2:%d s3:%d\n",
			core2_hsize_slc_s1[0],
			core2_hsize_slc_s1[1],
			core2_hsize_slc_s1[2],
			core2_hsize_slc_s1[3]);
		pr_info("corep_0 hsize s0:%d s1:%d s2:%d s3:%d\n",
			corep_hsize_slc_s0[0],
			corep_hsize_slc_s0[1],
			corep_hsize_slc_s0[2],
			corep_hsize_slc_s0[3]);
	}

	if (need_update) {
		VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WRAP_HSIZE_0,
		((core2_hsize_slc[1] & 0x1fff) << 16) | ((core2_hsize_slc[0] & 0x1fff)));
		VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WRAP_HSIZE_1,
		((core2_hsize_slc[3] & 0x1fff) << 16) | ((core2_hsize_slc[2] & 0x1fff)));
		wrap_ctrl1_bit31 = direct_mode ? 0 : dolby_slice_mode;
		VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WRAP_CTRL1,
		((wrap_ctrl1_bit31 & 0x1) << 31) |
		((core2_slc_mode   & 0x1) << 30) |
		((core2_slc_mode   & 0x1) << 29) |
		((core2_vsize & 0x1fff) << 16));
		if (dolby_slice_mode) {
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_0,
			((core2_win_hbgn_slc[1] & 0x1fff) << 16) |
			(core2_win_hbgn_slc[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_1,
			((core2_win_hbgn_slc[3] & 0x1fff) << 16) |
			(core2_win_hbgn_slc[2] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_2,
			((core2_win_hend_slc[1] & 0x1fff) << 16) |
			(core2_win_hend_slc[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_3,
			((core2_win_hend_slc[3] & 0x1fff) << 16) |
			(core2_win_hend_slc[2] & 0x1fff));
		}
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_SIZE, (core2_vsize << 16));
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_HSIZE_S0,
			((core2_hsize_slc_s0[1] & 0x1fff) << 16) |
			((core2_hsize_slc_s0[0] & 0x1fff)));
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_HSIZE_S1,
			((core2_hsize_slc_s0[3] & 0x1fff) << 16) |
			((core2_hsize_slc_s0[2] & 0x1fff)));

		if (enable_top1) {
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RD_PYRES, (corep_vsize << 16));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYRES_SLC_0,
				((corep_hsize_slc_s0[1] & 0x1fff) << 16) |
				(corep_hsize_slc_s0[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYRES_SLC_1,
				((corep_hsize_slc_s0[3] & 0x1fff) << 16) |
				(corep_hsize_slc_s0[2] & 0x1fff));

			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HSTART_0,
			((corep_hbgn_slc_s0[1] & 0x1fff) << 16) | (corep_hbgn_slc_s0[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HSTART_1,
			((corep_hbgn_slc_s0[3] & 0x1fff) << 16) | (corep_hbgn_slc_s0[2] & 0x1fff));

			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HEND_0,
			((corep_hend_slc_s0[1] & 0x1fff) << 16) | (corep_hend_slc_s0[0] & 0x1fff));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HEND_1,
			((corep_hend_slc_s0[3] & 0x1fff) << 16) | (corep_hend_slc_s0[2] & 0x1fff));

			VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL3, pyramid_stride, 0, 13);
		}
		if (core2_slc_mode) {
			//cfg win_cut
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S0_0,
				((core2_win_hbgn_slc_s0[1] & 0x1fff) << 16) |
				((core2_win_hbgn_slc_s0[0] & 0x1fff)));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S0_1,
				((core2_win_hbgn_slc_s0[3] & 0x1fff) << 16) |
				((core2_win_hbgn_slc_s0[2] & 0x1fff)));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S0_2,
				((core2_win_hend_slc_s0[1] & 0x1fff) << 16) |
				((core2_win_hend_slc_s0[0] & 0x1fff)));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S0_3,
				((core2_win_hend_slc_s0[3] & 0x1fff) << 16) |
				((core2_win_hend_slc_s0[2] & 0x1fff)));

			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S1_0,
				((core2_win_hbgn_slc_s1[1] & 0x1fff) << 16) |
				((core2_win_hbgn_slc_s1[0] & 0x1fff)));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S1_1,
				((core2_win_hbgn_slc_s1[3] & 0x1fff) << 16) |
				((core2_win_hbgn_slc_s1[2] & 0x1fff)));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S1_2,
				((core2_win_hend_slc_s1[1] & 0x1fff) << 16) |
				((core2_win_hend_slc_s1[0] & 0x1fff)));
			VSYNC_WR_TOP2_REG(T6X_VPU_DOLBY_WIN_CUT_WIN_S1_3,
				((core2_win_hend_slc_s1[3] & 0x1fff) << 16) |
				((core2_win_hend_slc_s1[2] & 0x1fff)));

			//cfg top2_slc_1 regs
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_SIZE + slc_ofst,
				(core2_vsize << 16));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_HSIZE_S0 + slc_ofst,
				((core2_hsize_slc_s1[1] & 0x1fff) << 16) |
				((core2_hsize_slc_s1[0] & 0x1fff)));
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PIC_HSIZE_S1 + slc_ofst,
				((core2_hsize_slc_s1[3] & 0x1fff) << 16) |
				((core2_hsize_slc_s1[2] & 0x1fff)));
			if (enable_top1) {
				VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RD_PYRES + slc_ofst,
					(corep_vsize << 16));
				VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYRES_SLC_0 + slc_ofst,
					((corep_hsize_slc_s1[1] & 0x1fff) << 16) |
					((corep_hsize_slc_s1[0] & 0x1fff)));
				VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYRES_SLC_1 + slc_ofst,
					((corep_hsize_slc_s1[3] & 0x1fff) << 16) |
					((corep_hsize_slc_s1[2] & 0x1fff)));

				VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HSTART_0 + slc_ofst,
					((corep_hbgn_slc_s1[1] & 0x1fff) << 16) |
					((corep_hbgn_slc_s1[0] & 0x1fff)));
				VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HSTART_1 + slc_ofst,
					((corep_hbgn_slc_s1[3] & 0x1fff) << 16) |
					((corep_hbgn_slc_s1[2] & 0x1fff)));

				VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HEND_0 + slc_ofst,
					((corep_hend_slc_s1[1] & 0x1fff) << 16) |
					((corep_hend_slc_s1[0] & 0x1fff)));
				VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_SLC_HEND_1 + slc_ofst,
					((corep_hend_slc_s1[3] & 0x1fff) << 16) |
					((corep_hend_slc_s1[2] & 0x1fff)));
				VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL3 + slc_ofst,
					pyramid_stride, 0, 13);
			}
		}
		/*VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_CTRL0, pyramid_rd_en, 5, 1);*/
		top2_ctrl0 = VSYNC_RD_TOP2_REG(T6W_DOLBY_TOP2_CTRL0);
		//pr_dv_dbg("read top2_ctrl0 %x\n", top2_ctrl0);
		top2_ctrl0 &= 0x3FFFFFDF;
		top2_ctrl0 |= pyramid_rd_en << 5;
		//pr_dv_dbg("2   write top2_ctrl0 %x\n", top2_ctrl0);
		/*only write pyramid_rd_en without bit30-31*/
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL0, top2_ctrl0);
		VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_RDMA_CTRL,
			((rdma1_num << 8) | (rdma0_num & 0x3)), 0, 12);
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE0, (rdma_size[0] << 16) | rdma_size[1]);
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE1, (rdma_size[2] << 16) | rdma_size[3]);

		if (core2_slc_mode) {
			top2_ctrl0 = VSYNC_RD_TOP2_REG(T6W_DOLBY_TOP2_CTRL0 + slc_ofst);
			top2_ctrl0 &= 0x3FFFFFFF;
			top2_ctrl0 |= pyramid_rd_en << 5;
			/*only write pyramid_rd_en without bit30-31*/
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL0 + slc_ofst, top2_ctrl0);
			VSYNC_WR_TOP2_BITS(T6W_DOLBY_TOP2_RDMA_CTRL  + slc_ofst,
				((rdma1_num << 8) | (rdma0_num & 0x3)), 0, 12);
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE0 + slc_ofst,
				(rdma_size[0] << 16) | rdma_size[1]);
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE1 + slc_ofst,
				(rdma_size[2] << 16) | rdma_size[3]);
		}
	}
	if (enable_top1) {
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL4, corep_baddr >> 4);
		if (core2_slc_mode)
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_PYUP_RMIF_CTRL4 + slc_ofst,
				corep_baddr >> 4);
	}

	if ((dolby_vision_flags & FLAG_CERTIFICATION) || load_fixed_setting) {/*crc*/
		if (load_fixed_setting)
			p_reg_64 = (u64 *)top2_reg_buf;
		else
			p_reg_64 = reg_data;
		if (p_reg_64[574] == 0x000008f400000000)
			p_reg_64[574] = 0x000008f400000001;
	}

	if (hw5_reg_from_file || (test_dv & DEBUG_FIXED_REG) /*fixed reg*/) {
		p_reg = (u32 *)top2_reg_buf;
		top2_ahb_num = top2_reg_num;
	} else {
		p_reg = (u32 *)reg_data;
	}

	dolby5_ahb_reg_config(p_reg, 2, top2_ahb_num, reset, core2_slice_num,
			core2_hsize_slc_s0, corep_hsize_slc_s0, 1, need_update, need_update_roi);
	if (core2_slc_mode && !prm_dolby->core2_prl_mode) /*only set reg/lut once in prl mode*/
		dolby5_ahb_reg_config(p_reg, 3, top2_ahb_num, reset, core2_slice_num,
			core2_hsize_slc_s1, corep_hsize_slc_s1, 1,
			need_update, need_update_roi);

	last_core2_hsize = core2_hsize;
	last_core2_vsize = core2_vsize;
	last_corep_hsize = corep_hsize;
	last_corep_vsize = corep_vsize;
	last_core2_slice_num = core2_slice_num;
	last_core2_ext_ovlp = core2_ext_ovlp;
	last_core2_py_level = core2_py_level;
	last_direct_mode = direct_mode;
}

void dump_lut_t6w(void)
{
	int i;

	if ((debug_dolby & 0x40000) && enable_top1) {
		WRITE_VPP_DV_REG(T6W_DOLBY5_CORE1_ICSCLUT_DBG_RD, 0x1);
		for (i = 0; i < 595; i++) {
			WRITE_VPP_DV_REG(T6W_DOLBY5_CORE1_ICSCLUT_RDADDR, i);
			if (i == 0)/*discard last data*/
				READ_VPP_DV_REG(T6W_DOLBY5_CORE1_ICSCLUT_RDDATA);
			if (i >= 1)
				pr_d_log(1, "top1: ICSC[%d]: %x",
					i - 1, READ_VPP_DV_REG(T6W_DOLBY5_CORE1_ICSCLUT_RDDATA));
			if (i == 594) {
				WRITE_VPP_DV_REG(T6W_DOLBY5_CORE1_ICSCLUT_RDADDR, i);
				pr_d_log(1, "top1: ICSC[%d]: %x",
					i, READ_VPP_DV_REG(T6W_DOLBY5_CORE1_ICSCLUT_RDDATA));
			}
		}
	}

	if ((debug_dolby & 0x40000) && top2_info.core_on) {
		/*bit 3-0: ocsc-icsc-cvmsmi-cvmtai*/
		WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_DEBUG_LUT_CNTRL, 0x4);
		for (i = 0; i < 595; i++) {
			WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_ICSCLUT_RDADDR, i);
			if (i == 0)/*discard last data*/
				READ_VPP_DV_REG(T6W_DOLBY5_CORE2_ICSCLUT_RDDATA);
			if (i >= 1)
				pr_d_log(1, "ICSC[%d]: %x",
					i - 1, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_ICSCLUT_RDDATA));
			if (i == 594) {
				WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_ICSCLUT_RDADDR, i);
				pr_d_log(1, "ICSC[%d]: %x",
					i, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_ICSCLUT_RDDATA));
			}
		}
		WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_DEBUG_LUT_CNTRL, 0x8);
		for (i = 0; i < 129; i++) {
			WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_OCSCLUT_RDADDR, i);
			if (i == 0)
				READ_VPP_DV_REG(T6W_DOLBY5_CORE2_OCSCLUT_RDDATA);/*discard last*/
			if (i >= 1)
				pr_d_log(1, "OCSC[%d]: %x",
					i - 1, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_OCSCLUT_RDDATA));
			if (i == 128) {
				WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_OCSCLUT_RDADDR, i);
				pr_d_log(1, "OCSC[%d]: %x",
					i, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_OCSCLUT_RDDATA));
			}
		}
		WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_DEBUG_LUT_CNTRL, 0x1);
		for (i = 0; i < 513; i++) {
			WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_TAILUT_RDADDR, i);
			if (i == 0)
				READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_TAILUT_RDDATA);/*discard last*/
			if (i >= 1)
				pr_d_log(1, "CVM TAI[%d]: %x",
					i - 1, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_TAILUT_RDDATA));
			if (i == 512) {
				WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_TAILUT_RDDATA, i);
				pr_d_log(1, "CVM TAI[%d]: %x",
					i, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_TAILUT_RDDATA));
			}
		}
		WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_DEBUG_LUT_CNTRL, 0x2);
		for (i = 0; i < 513; i++) {
			if (i == 0)
				READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_SMILUT_RDDATA);
			if (i >= 1)
				pr_d_log(1, "CVM SMI[%d]: %x",
				i - 1, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_SMILUT_RDDATA));
			if (i == 512) {
				WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_SMILUT_RDADDR, i);
				pr_d_log(1, "CVM SMI[%d]: %x",
				i, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_SMILUT_RDDATA));
			}
			pr_d_log(1, "CVM SMI[%d]: %x",
				i, READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CVM_SMILUT_RDDATA));
		}
	}
}

void dump_pyr_up(void)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	//todo
#else
	u8 *data_addr;
	u16 data_addr_16_1;
	u16 data_addr_16_2;
	unsigned int data_size;
	int i;

	data_addr = (u8 *)top_info.up_buf[0].pyup_vaddr;
	data_size = top_info.up_buf[0].pyup_size;

	/*for 480x270*/
	pr_d_log(1, "dump pyr_up line 0\n");
	for (i = 0; i < 1440; i += 3) {/*480*3*/
		data_addr_16_1 = ((data_addr[i + 1] & 0xf) << 8) | data_addr[i];/*12bit*/
		data_addr_16_2 = (data_addr[i + 2] << 4) | (data_addr[i + 1] >> 4);/*12bit*/
		pr_d_log(1, "%x %x\n", data_addr_16_1, data_addr_16_2);
	}
	pr_d_log(1, "dump pyr_up line 1\n");
	for (i = 1472; i < 1472 + 1440; i += 3) {
		data_addr_16_1 = ((data_addr[i + 1] & 0xf) << 8) | data_addr[i];
		data_addr_16_2 = (data_addr[i + 2] << 4) | (data_addr[i + 1] >> 4);
		pr_d_log(1, "%x %x\n", data_addr_16_1, data_addr_16_2);
	}
	pr_d_log(1, "dump pyr_up line 2\n");
	for (i = 1472 * 2; i < 1472 * 2 + 1440; i += 3) {
		data_addr_16_1 = ((data_addr[i + 1] & 0xf) << 8) | data_addr[i];
		data_addr_16_2 = (data_addr[i + 2] << 4) | (data_addr[i + 1] >> 4);
		pr_d_log(1, "%x %x\n", data_addr_16_1, data_addr_16_2);
	}
	pr_d_log(1, "dump pyr_up line 100\n");
	for (i = 1472 * 100; i < 1472 * 100 + 1440; i += 3) {
		data_addr_16_1 = ((data_addr[i + 1] & 0xf) << 8) | data_addr[i];
		data_addr_16_2 = (data_addr[i + 2] << 4) | (data_addr[i + 1] >> 4);
		pr_d_log(1, "%x %x\n", data_addr_16_1, data_addr_16_2);
	}
	pr_d_log(1, "dump pyr_up line 268\n");
	for (i = 1472 * 268; i < 1472 * 268 + 1440; i += 3) {
		data_addr_16_1 = ((data_addr[i + 1] & 0xf) << 8) | data_addr[i];
		data_addr_16_2 = (data_addr[i + 2] << 4) | (data_addr[i + 1] >> 4);
		pr_d_log(1, "%x %x\n", data_addr_16_1, data_addr_16_2);
	}
	pr_d_log(1, "dump pyr_up line 269\n");
	for (i = 1472 * 269; i < 1472 * 269 + 1440; i += 3) {
		data_addr_16_1 = ((data_addr[i + 1] & 0xf) << 8) | data_addr[i];
		data_addr_16_2 = (data_addr[i + 2] << 4) | (data_addr[i + 1] >> 4);
		pr_d_log(1, "%x %x\n", data_addr_16_1, data_addr_16_2);
	}
#endif
}

static void cfg_dolby_rdma(bool update_top1, bool update_top2)
{
	u32 rdma0_baddr = lut_dma_info[cur_dmabuf_id].dma_paddr >> 4;
	u32 rdma1_baddr = lut_dma_info[cur_dmabuf_id].dma_paddr_top2 >> 4;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("lut dma:baddr %x, %x,id %d,update %d %d\n",
				rdma0_baddr, rdma1_baddr, cur_dmabuf_id, update_top1, update_top2);

	//cur_dmabuf_id = cur_dmabuf_id ^ 1;//todo
	//dma0
	WRITE_VPP_DV_REG_BITS(VPU_DMA_RDMIF_SEL, 0, 0, 1);
	if (update_top1) {
		VSYNC_WR_TOP1_REG(VPU_DMA_RDMIF3_BADR0, rdma0_baddr);
		VSYNC_WR_TOP1_REG(VPU_DMA_RDMIF3_BADR1, rdma0_baddr);
		VSYNC_WR_TOP1_REG(VPU_DMA_RDMIF3_BADR2, rdma0_baddr);
		VSYNC_WR_TOP1_REG(VPU_DMA_RDMIF3_BADR3, rdma0_baddr);
		VSYNC_WR_TOP1_BITS(VPU_DMA_RDMIF3_CTRL, 149, 0, 13);//reg_rd0_stride
		//Bit23:16 reg_rd0_enable_int sel tri2
		VSYNC_WR_TOP1_BITS(VPU_DMA_RDMIF3_CTRL, 1, 18, 1);
		VSYNC_WR_TOP1_BITS(VPU_DMA_RDMIF3_CTRL, 0, 13, 1);//Bit13 little_endian
		VSYNC_WR_TOP1_BITS(VPU_DMA_RDMIF3_CTRL, 0, 14, 1);//Bit14 swap_64bit
	}

	//dma1
	if (update_top2) {
		VSYNC_WR_TOP2_REG(VPU_DMA_RDMIF4_BADR0, rdma1_baddr);
		VSYNC_WR_TOP2_REG(VPU_DMA_RDMIF4_BADR1, rdma1_baddr);
		VSYNC_WR_TOP2_REG(VPU_DMA_RDMIF4_BADR2, rdma1_baddr);
		VSYNC_WR_TOP2_REG(VPU_DMA_RDMIF4_BADR3, rdma1_baddr);
		VSYNC_WR_TOP2_BITS(VPU_DMA_RDMIF4_CTRL, 424, 0, 13);  //reg_rd0_stride
		//Bit23:16 reg_rd0_enable_int sel tri7
		VSYNC_WR_TOP2_BITS(VPU_DMA_RDMIF4_CTRL, 1, 23, 1);
		VSYNC_WR_TOP2_BITS(VPU_DMA_RDMIF4_CTRL, 0, 13, 1);//Bit13 little_endian
		VSYNC_WR_TOP2_BITS(VPU_DMA_RDMIF4_CTRL, 0, 14, 1);//Bit14 swap_64bit
	}
	if (update_top1)
		VSYNC_WR_TOP1_REG(VPU_LUT_DMA_INTR_SEL, 0xf0);//vpu_dma_intr_sel,bit4-5 for top1
	else if (update_top2)
		VSYNC_WR_TOP2_REG(VPU_LUT_DMA_INTR_SEL, 0xf0);//vpu_dma_intr_sel,bit6-7 for top2
}

static inline u32 top1_stride_rdmif(u32 buffr_width, u8 plane_bits)
{
	u32 line_stride;

	/* input: buffer width not hsize */
	/* 1 stride = 16 byte, DDR 64bytes alignment */
	line_stride = (((buffr_width * plane_bits + 127) / 128 + 3) >> 2) << 2;
	return line_stride;
}

static void dolby5_mmu_config(u8 *baddr, int num)
{
	u32 mmu_lut;
	u32 lut0;
	u32 lut1;
	u32 lut2;
	u32 lut3;
	u32 lut4;
	u32 lut5;
	u32 lut6;
	u32 lut7;
	int i;

	//num = 568 + 1143;/*py_level==0 ? 567 : 1143;*//*for t6w, config both level6+7*/

	VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRMIF_MMU_ADDR, 0);

	for (i = 0; i < (num + 7) >> 3; i = i + 1) {
		mmu_lut = 0;
		lut0 = (baddr[i * 8]) & 0x7;
		lut1 = (baddr[i * 8 + 1]) & 0x7;
		lut2 = (baddr[i * 8 + 2]) & 0x7;
		lut3 = (baddr[i * 8 + 3]) & 0x7;
		mmu_lut = (lut3 << 9) | (lut2 << 6) | (lut1 << 3) | lut0;
		lut4 = (baddr[i * 8 + 4]) & 0x7;
		lut5 = (baddr[i * 8 + 5]) & 0x7;
		lut6 = (baddr[i * 8 + 6]) & 0x7;
		lut7 = (baddr[i * 8 + 7]) & 0x7;
		mmu_lut = ((lut7 << 21) | (lut6 << 18) | (lut5 << 15) | (lut4 << 12)) | mmu_lut;

		if (i < 2)
			if (debug_dolby & 1)
				pr_dv_dbg("mmu %d: %x %x %x %x %x %x %x %x, mmu_lut %x\n",
					i, lut0, lut1, lut2, lut3, lut4, lut5, lut6, lut7, mmu_lut);

		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP2_PYRMIF_MMU_DATA, mmu_lut);
	}
}

int t6w_top1_set(struct prm_dolby_top *prm_dolby,
				u64 *top1_reg,
				u64 *top1b_reg,
				bool reset,
				int video_enable,
				bool toggle)
{
	static u32 last_core1_hsize;
	static u32 last_core1_vsize;
	static u32 last_core1_py_level;
	bool force_update = false;
	u32 top1_ctrl0;

	if (!prm_dolby)
		return 0;

	if (last_core1_hsize != prm_dolby->core1_src_hsize ||
		last_core1_vsize != prm_dolby->core1_src_vsize ||
		last_core1_py_level != prm_dolby->core1_py_level) {
		last_core1_hsize = prm_dolby->core1_src_hsize;
		last_core1_vsize = prm_dolby->core1_src_vsize;
		last_core1_py_level = prm_dolby->core1_py_level;
		force_update = true;
	}

	if (debug_dolby & 1)
		pr_dv_dbg("top1_set:on %d %d,reset %d %d,toggle %d,video %d,level %s,py_id %d\n",
				   top1_info.core_on, top1_info.core_on_cnt,
				   reset, force_update, toggle, video_enable,
				   prm_dolby->core1_py_level == 0 ?
				   "6" : (prm_dolby->core1_py_level == 1 ? "7" : "0"),
				   top1_info.py_id);

	dolby5_top1_ini(prm_dolby, top1_reg, top1b_reg, reset, force_update);

	if (reset) {
		if (debug_val & 0x3ffff)
			VSYNC_WR_TOP1_REG(T6W_VPU_DOLBY_IRQ_CTRL0, (debug_val & 0x3ffff));
		else//bit0:total top1;
			VSYNC_WR_TOP1_BITS(T6W_VPU_DOLBY_IRQ_CTRL0, 1, 0, 1);

		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_RDMA_CTRL,
			(1 << 30) | //open top1 rdma
			(1 << 16) |
			0x95);
	}

	//if (!top1_info.core_on && wait_first_frame_top1) {/*update lut data when top2 not on*/
		set_dovi_setting_update_flag(true);
		amdv_update_setting(true, false);
		cfg_dolby_rdma(true, false);
	//}
	VSYNC_WR_TOP1_REG(VPU_LUT_DMA_INTR_SEL, 0x1f0);//trigger core1 dma

	dolby5_corep_ini(prm_dolby, reset, force_update);

	if (sw_trigger_top1(prm_dolby)) {
		top1_ctrl0 = VSYNC_RD_TOP1_REG(T6W_DOLBY_TOP1_CTRL0);
		top1_ctrl0 &= 0x3FFFFFFF;
		top1_ctrl0 |= 1 << 30;
		pr_dv_dbg("2 write top1_ctrl0 %x\n", top1_ctrl0);
		VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_CTRL0, top1_ctrl0);/*sw trigger*/
	}

	return 0;
}

static int t6w_top2_set(struct prm_dolby_top *prm_dolby,
			     u64 *reg_data,
			     int video_enable,
			     bool reset,
			     bool toggle,
			     bool pr_done)
{
	/*int i;*/
	static u32 last_size;
	static u32 ttt;/*just for debug rdma write status*/
	static u32 last_top2_bys_mode;
	u32 top2_ctrl0;
	u32 top2_ctrl1;
	u32 slc_ofst = T6X_CORE2_SLICE_OFF;

	if (!prm_dolby)
		return 0;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("top2_set:size %dx%d,pad_mode %d,level %s,slice %d\n",
					prm_dolby->core2_hsize, prm_dolby->core2_vsize,
					prm_dolby->core2_pad_mode,
					prm_dolby->core2_py_level == 0 ?
					"6" : (prm_dolby->core2_py_level == 1 ? "7" : "0"),
					prm_dolby->core2_slice_num);

	if (dolby_vision_on &&
		(dolby_vision_flags & FLAG_DISABE_CORE_SETTING)) {
		if (top2_info.core_on) {/*set every vsync*/
			VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 2, 1);/*MD Program start*/
			VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 4, last_int_top2);/*clear*/
			VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 3, 1);/*MD Program end */

			if (lut_trigger_by_reg) {
				/*use reg to trigger lut, should after DOLBY_TOP2_RDMA set */
				VSYNC_WR_TOP2_REG(VPU_LUT_DMA_INTR_SEL, 0x2f0);//trigger core2 dma
			}
			ttt++;
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE5, ttt);
		}
		return 0;
	}

	dump_lut_t6w();

	if (top1_info.core_on && top1_done)
		top1_done = false; //todo

	if (debug_dolby & 1)
		pr_dv_dbg("top2_set:on %d %d,reset %d,toggle %d,video %d,level %s,py_id %d,%d\n",
				  top2_info.core_on, top2_info.core_on_cnt,
				  reset, toggle, video_enable,
				  top2_info.py_level == 0 ?
				  "6" : (top2_info.py_level == 1 ? "7" : "0"),
				  top2_info.py_id, ttt);

	if ((dolby_vision_flags & FLAG_CERTIFICATION) || hw5_reg_from_file) {
		VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_CRC_CNTRL, 1);
		if (is_aml_t6w()) {
			VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_WRAP_CTRL, 1, 22, 1);
			if (prm_dolby->core2_slice_num == 1) {
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_CRC_ACT_WIN_0, 0, 16, 13);
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_CRC_ACT_WIN_2,
					prm_dolby->core2_hsize - 1, 0, 13);
			} else if (prm_dolby->core2_slice_num == 2) {
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_CRC_ACT_WIN_0, 64 + 96, 16, 13);
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_CRC_ACT_WIN_2, 960 - 1, 0, 13);
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_CRC_ACT_WIN_2, 64 + 96 + 960 - 1,
						16, 13);
			}
		} else if (is_aml_t6x()) {
			VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_WRAP_CTRL, 1, 22, 1);
			if (prm_dolby->core2_slice_num == 1) {
				VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_CRC_ACT_WIN_0, 0, 16, 13);
				VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_CRC_ACT_WIN_2,
					prm_dolby->core2_hsize - 2, 0, 13);
			} else if (prm_dolby->core2_slice_num == 2) {
				VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_CRC_ACT_WIN_0, 64 + 96, 16, 13);
				VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_CRC_ACT_WIN_2, 960 - 2, 0, 13);
				VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_CRC_ACT_WIN_2, 64 + 96 + 960 - 2,
						16, 13);
			}
		}
	}

	/*TB49 and IDK include RGB cases*/
	if ((dv_unique_drm || (test_dv & DEBUG_5065_RGB_BUG)) &&
		tv_hw5_setting &&
		tv_hw5_setting->top2.color_format == CP_RGB &&
		tv_hw5_setting->top2_reg[23] == 0x00000058000002c0)
		tv_hw5_setting->top2_reg[23] = 0x00000058000002c1;/*bit0 change from yuv to rgb*/

	//update_top2_reg(vd1_slice0_hsize, vsize);

	if (sw_trigger_top2(prm_dolby)) {
		top2_ctrl0 = VSYNC_RD_TOP2_REG(T6W_DOLBY_TOP2_CTRL0);
		top2_ctrl0 &= 0x3FFFFFFF;
		top2_ctrl0 |= 1 << 31;
		//pr_dv_dbg("1 write top2_ctrl0 %x\n", top2_ctrl0);
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL0, top2_ctrl0);/*bit31=1 reset core2*/
		if (prm_dolby->core2_slc_mode)/*bit31=1 reset core2_0*/
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL0 + slc_ofst, top2_ctrl0);
		top2_ctrl1 = VSYNC_RD_TOP2_REG(T6W_DOLBY_TOP2_CTRL1);
		top2_ctrl1 &= 0xFCFFFFFF;
		top2_ctrl1 |= 1 << 24;
		//pr_dv_dbg("1 write top2_ctrl1 %x\n", top2_ctrl1);
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL1, top2_ctrl1);/*bit24=1 sw control frm_rst */
		if (prm_dolby->core2_slc_mode)/*bit31=1 reset core2_1*/
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL1 + slc_ofst, top2_ctrl1);
	}
	if (!top2_info.core_on)
		last_top2_bys_mode = 0;

	if (last_top2_bys_mode !=
		((test_dv & DEBUG_FORCE_BYPASS_TOP2) || prm_dolby->byps_mode)) {
		if ((test_dv & DEBUG_FORCE_BYPASS_TOP2) || prm_dolby->byps_mode) {
			if (is_aml_t6w())
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_WRAP_CTRL, 1, 31, 1);
			else if (is_aml_t6x())
				VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_WRAP_CTRL, 1, 27, 1);
		} else {
			if (is_aml_t6w())
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_WRAP_CTRL, 0, 31, 1);
			else if (is_aml_t6x())
				VSYNC_WR_TOP2_BITS(T6X_VPU_DOLBY_WRAP_CTRL, 0, 27, 1);
		}
		last_top2_bys_mode = ((test_dv & DEBUG_FORCE_BYPASS_TOP2) || prm_dolby->byps_mode);
	}

	if (reset) {
		/*t3x from ucode 0x152b,it is error*/
		if (prm_dolby->core2_detunnel) {
			if (vdin_vd1_detunnel_tunnel_mode)
				VSYNC_WR_DV_REG(T6W_VPU_DOLBY_WRAP_DTNL,
				(prm_dolby->core2_hsize << 18) | 0x8762);
			else
				VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_WRAP_DTNL,
					(prm_dolby->core2_hsize << 18) | 0x2c2d0);
			/*hdmi STD, LL and Unique_422/420_12bit DV: need detunnel*/
			if (prm_dolby->core2_slc_mode)
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_WRAP_DTNL_CTRL, 3, 2, 2);
			else
				VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_WRAP_DTNL_CTRL, 1, 2, 1);
		} else {
			VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_WRAP_DTNL_CTRL, 0, 2, 2);
		}
		if (((debug_val & 0xff00000) >> 20) & 0xff)
			VSYNC_WR_TOP2_REG(T6W_VPU_DOLBY_IRQ_CTRL2,
			((debug_val & 0xff00000) >> 20) & 0xff);
		else
			VSYNC_WR_TOP2_BITS(T6W_VPU_DOLBY_IRQ_CTRL2, 1, 0, 1);//top2 dolby int enable
	}

	if (reset || toggle) {/*no need set these regs when no toggle*/
		if (is_aml_t6w())
			dolby5_top2_he2(prm_dolby, reg_data, reset);
		else if (is_aml_t6x())
			dolby5_top2_he2_slc(prm_dolby, reg_data, reset);
	} else { /*set every vsync*/
		VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 2, 1);/*Metadata Program start*/
		VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 4, last_int_top2);/*clear interrupt*/
		VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 3, 1);/*Metadata Program end */

		if (prm_dolby->core2_slc_mode) {
			VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 2, 1);
			VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 4, last_int_top2);
			VSYNC_WR_TOP2_REG(T6W_DOLBY5_CORE2_REG_BASE0 + slc_ofst + 3, 1);
		}
	}

	if (reset || toggle) {/*no need update lut data when no toggle*/
		set_dovi_setting_update_flag(true);
		amdv_update_setting(false, true);
		cfg_dolby_rdma(false, true);
	}
	if (lut_trigger_by_reg) {/*set every vsync*/
		/*use reg to trigger lut, should after DOLBY_TOP2_RDMA set */
		VSYNC_WR_TOP2_REG(VPU_LUT_DMA_INTR_SEL, 0x2f0);//todo trigger core2 dma
	}
	if (sw_trigger_top2(prm_dolby)) {
		top2_ctrl0 = VSYNC_RD_TOP2_REG(T6W_DOLBY_TOP2_CTRL0);
		//pr_dv_dbg("read top2_ctrl0 %x\n", top2_ctrl0);
		top2_ctrl0 &= 0x3FFFFFFF;
		top2_ctrl0 |= 1 << 30;
		//pr_dv_dbg("3 write top2_ctrl0 %x\n", top2_ctrl0);
		/*bit30=1 trigger top2 frm_rst,without bit31=1*/
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL0, top2_ctrl0);
		if (prm_dolby->core2_slc_mode)
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL0 + slc_ofst, top2_ctrl0);
		top2_ctrl1 &= 0xFCFFFFFF;
		top2_ctrl1 |= 2 << 24;
		//pr_dv_dbg("2 write top2_ctrl1 %x\n", top2_ctrl1);
		/*bit24=2 vsync control frm_rst*/
		VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL1, top2_ctrl1);
		if (prm_dolby->core2_slc_mode)
			VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_CTRL1 + slc_ofst, top2_ctrl1);
	}
	ttt++;
	VSYNC_WR_TOP2_REG(T6W_DOLBY_TOP2_RDMA_SIZE5, ttt);/*just for debug*/
	last_size = (prm_dolby->core2_hsize << 16) | prm_dolby->core2_vsize;
	return 0;
}

void cfg_dolby_ini(struct prm_dolby_top *prm_dolby, struct dpss_info_s *dpss_info)
{
	int i;

	if (!prm_dolby)
		return;

	prm_dolby->fst_frm_ini         = 1;
	prm_dolby->fst_frm_ini_top2    = 1;
	prm_dolby->byps_mode           = 0;//1:core_byps 2:wrap_byps
	prm_dolby->mosaic_mode         = 0;
	if (dpss_info) {/*dpss init*/
		prm_dolby->dolby5_mode = DOLBY5_DPSS_MODE;
		if (is_aml_t6x() && enable_2ppc)
			prm_dolby->dolby5_mode = DOLBY5_DPSS_MODE_2PPC;
		prm_dolby->core2_slice_num     = dpss_info->slice_num;
		prm_dolby->core2_pad_mode      = dpss_info->pad_mode;//0:nonr+noaa,1:aa 2:nr+aa
		prm_dolby->frm_hsize_sel       = dpss_info->frm_hsize_sel;
		prm_dolby->dpss_tbc_mode       = dpss_info->tbc_mode;
		prm_dolby->dpss_direct_mode = dpss_info->direct_mode;
		prm_dolby->vds_4k1k_en = dpss_info->vds_4k1k_en;
		prm_dolby->dpss_dct_ahead_dv    = dpss_info->dct_ahead_dv_mode;
		prm_dolby->dpss_real_h = dpss_info->real_h;
		prm_dolby->dpss_real_v = dpss_info->real_v;
		pr_dv_dbg("init dpss_info %d %d %d %d %d %d,real size %d %d\n",
			 dpss_info->frm_hsize_sel, dpss_info->slice_num,
			 dpss_info->pad_mode, dpss_info->tbc_mode,
			 dpss_info->direct_mode, dpss_info->dct_ahead_dv_mode,
			 dpss_info->real_h, dpss_info->real_v);
	} else {/*reset after playing*/
		prm_dolby->dolby5_mode         = DOLBY5_VD1_MODE;
		if (is_aml_t6x() && enable_2ppc)
			prm_dolby->dolby5_mode         = DOLBY5_VD1_MODE_2PPC;
		prm_dolby->core2_slice_num     = 1;
		prm_dolby->core2_pad_mode      = 0;//0:nonr+noaa,1:aa 2:nr+aa
		prm_dolby->frm_hsize_sel = 0;
		prm_dolby->dpss_tbc_mode = 0;
		prm_dolby->dpss_direct_mode = false;
		prm_dolby->vds_4k1k_en = 0;
		prm_dolby->dpss_dct_ahead_dv = 0;
	}
	prm_dolby->core1_py_level      = 2;//0:6py 1:7py 2:nopy
	for (i = 0; i < 7; i++)
		prm_dolby->core1_pyr_wbaddr[i] = 0;
	prm_dolby->core1_wdma_wbaddr   = 0;
	prm_dolby->core1_src_hsize     = 360;
	prm_dolby->core1_src_vsize     = 240;
	prm_dolby->core1_src_bit       = BIT_10;
	prm_dolby->core1_src_fmt       = YUV444;
	prm_dolby->core1_src_plane     = PLANAR_X1;
	prm_dolby->core1_mif_mmu_ena   = 0;
	prm_dolby->corep_py_level      = 2;//0:6py 1:7py 2:nopy
	prm_dolby->corep_up_wbaddr[0]  = 0;//pyr_up_wr
	prm_dolby->corep_up_wbaddr[1]  = 0;//pyr_up_wr
	prm_dolby->corep_hsize         = 360;
	prm_dolby->corep_vsize         = 240;
	prm_dolby->core2_detunnel = 0;
	prm_dolby->core2_hsize    = 720;
	prm_dolby->core2_vsize    = 480;
	prm_dolby->core2_py_level = 2;//0:6poy 1:7py 2:nopy

	pre_set_done = false;
	prm_dolby->core2_slc_mode =
			(prm_dolby->dolby5_mode == DOLBY5_VD1_MODE_2PPC) ||
			(prm_dolby->dolby5_mode == DOLBY5_VDIN_MODE_2PPC) ||
			(prm_dolby->dolby5_mode == DOLBY5_DPSS_MODE_2PPC) ||
			(prm_dolby->dolby5_mode == DOLBY5_DPSS_PRL_MODE_2PPC) ||
			(prm_dolby->dolby5_mode == DOLBY5_DPSS_DI_MODE_2PPC);

	for (i = 0; i < TOP_BUF_CNT; i++) {
		top_info.vf_info[i].top1_start = false;
		top_info.vf_info[i].top1_done = false;
		top_info.vf_info[i].top2_start = false;
		top_info.vf_info[i].top2_done = false;
		top_info.vf_info[i].vf = NULL;
	}
	if (is_aml_t6x())/*prl mode only work in 2 core2 mode*/
		prm_dolby->core2_prl_mode = (prl_mode && prm_dolby->core2_slc_mode) ? true : false;
	else
		prm_dolby->core2_prl_mode = false;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("prm_dolby init:slice %d,pad %d,sel %d,tbc %d,mode %d %d,prl %d,4k1k_en %d\n",
		prm_dolby->core2_slice_num,
		prm_dolby->core2_pad_mode,
		prm_dolby->frm_hsize_sel,
		prm_dolby->dpss_tbc_mode,
		prm_dolby->dolby5_mode,
		top_dd_work_mode,
		prm_dolby->core2_prl_mode,
		prm_dolby->vds_4k1k_en);
}

void cfg_dolby_update(struct prm_dolby_top *prm_dolby,
		bool enable_detunnel,
		u32 core2_hsize_for_corep, u32 core2_vsize_for_corep,
		u32 core2_hsize, u32 core2_vsize, u8 update, bool reset)
{
	u32 core1b_hsize;
	u32 core1b_vsize;
	static u32 last_width;
	static u32 last_height;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	bool change_flag = false;

	if (!prm_dolby)
		return;

	if (tv_hw5_setting)
		input_mode = tv_hw5_setting->top1.input_mode;

	if ((path_sel & 0xf) && !top_dd_work_mode)
		prm_dolby->dolby5_mode = (path_sel & 0xf);

	/*update core2_slc_mode*/
	prm_dolby->core2_slc_mode =
					(prm_dolby->dolby5_mode == DOLBY5_VD1_MODE_2PPC) ||
					(prm_dolby->dolby5_mode == DOLBY5_VDIN_MODE_2PPC) ||
					(prm_dolby->dolby5_mode == DOLBY5_DPSS_MODE_2PPC) ||
					(prm_dolby->dolby5_mode == DOLBY5_DPSS_PRL_MODE_2PPC) ||
					(prm_dolby->dolby5_mode == DOLBY5_DPSS_DI_MODE_2PPC);

	/*switch path during playing or dct change after init*/
	if ((last_dolby5_mode != prm_dolby->dolby5_mode &&
		top2_info.core_on) ||
		last_dpss_dct_mode != prm_dolby->dpss_dct_ahead_dv) {
		pr_dv_dbg("dolby5 mode change %d=>%d, dct %d=>%d\n",
			last_dolby5_mode, prm_dolby->dolby5_mode,
			last_dpss_dct_mode, prm_dolby->dpss_dct_ahead_dv);

		prm_dolby->fst_frm_ini = 1;/*need update cfg_dolby_path*/
		prm_dolby->fst_frm_ini_top2 = 1;/*need update cfg_dolby_axi_path*/
	}

	if (prm_dolby->fst_frm_ini) {/*update path*/
		cfg_dolby_path(prm_dolby);
		prm_dolby->fst_frm_ini = 0;
		change_flag = true;
	}
	if (prm_dolby->fst_frm_ini_top2 &&
		(prm_dolby->dolby5_mode == DOLBY5_DPSS_MODE ||
		prm_dolby->dolby5_mode == DOLBY5_DPSS_MODE_2PPC)) {
		/*AXIRD_PATH should sync with top2 in vpp mode*/
		cfg_dolby_axi_path(prm_dolby->dolby5_mode, prm_dolby->dpss_dct_ahead_dv);
		prm_dolby->fst_frm_ini_top2 = 0;
	}
	if (last_width != core2_hsize_for_corep ||
		last_height != core2_vsize_for_corep) {
		change_flag = true;
		last_width = core2_hsize_for_corep;
		last_height = core2_vsize_for_corep;
	}

	if (enable_top1) {
		if (update & 1) {
			if (fix_data == CASE1080p_TOP1_READFROM_FILE) {
				top1_info.py_level = PY_SEVEN_LEVEL;
				top1_vd_info.canvasaddr[0] = fixed_y_buf_paddr;
				top1_vd_info.canvasaddr[1] = 0;//no use in one plane mode
				top1_vd_info.width = 960;
				top1_vd_info.height = 540;
				top1_vd_info.bitdepth = 10;
				top1_vd_info.type = VIDTYPE_VIU_444;
				top1_vd_info.blk_mode = 0;
				if (get_support_max_level() == PY_SIX_LEVEL)
					top2_info.py_level = PY_SIX_LEVEL;
				else
					top2_info.py_level = PY_SEVEN_LEVEL;
			}
			if (change_flag || reset) {
				prm_dolby->core1_py_level = top1_info.py_level;//0:6py 1:7py 2:nopy
				/*py_addr use fixed index 0, core1b and corep connect directly*/
				prm_dolby->core1_pyr_wbaddr[0] = py_addr[0].top1_py_paddr[0];
				prm_dolby->core1_pyr_wbaddr[1] = py_addr[0].top1_py_paddr[1];
				prm_dolby->core1_pyr_wbaddr[2] = py_addr[0].top1_py_paddr[2];
				prm_dolby->core1_pyr_wbaddr[3] = py_addr[0].top1_py_paddr[3];
				prm_dolby->core1_pyr_wbaddr[4] = py_addr[0].top1_py_paddr[4];
				prm_dolby->core1_pyr_wbaddr[5] = py_addr[0].top1_py_paddr[5];
				prm_dolby->core1_pyr_wbaddr[6] = py_addr[0].top1_py_paddr[6];

				prm_dolby->core1_wdma_wbaddr   = dv5_md_hist.hist_paddr[0];
				prm_dolby->core1_src_hsize     = top1_vd_info.width;
				prm_dolby->core1_src_vsize     = top1_vd_info.height;
				if (top1_vd_info.bitdepth == 10) {
					prm_dolby->core1_src_bit =
						(input_mode == IN_MODE_HDMI) ? BIT_10 : BIT_P010;
				} else if (top1_vd_info.bitdepth == 8) {
					prm_dolby->core1_src_bit = BIT_8;
				} else {
					prm_dolby->core1_src_bit = BIT_8;
					if (debug_dolby & 0x80000)
						pr_dv_dbg("err bitdepth, pls check!\n");
				}

				if ((top1_vd_info.type & VIDTYPE_VIU_NV21) ||
					(top1_vd_info.type & VIDTYPE_VIU_NV12)) {
					prm_dolby->core1_src_fmt = YUV420;
					prm_dolby->core1_src_plane = PLANAR_X2;
				} else if (top1_vd_info.type & VIDTYPE_VIU_422) {
					prm_dolby->core1_src_fmt = YUV422;
					prm_dolby->core1_src_plane = PLANAR_X1;
				} else if (top1_vd_info.type & VIDTYPE_VIU_444) {
					prm_dolby->core1_src_fmt = YUV444;
					prm_dolby->core1_src_plane = PLANAR_X1;
				} else {
					prm_dolby->core1_src_fmt = YUV420;
					prm_dolby->core1_src_plane = PLANAR_X3;
					if (debug_dolby & 0x80000)
						pr_dv_dbg("err type, pls check !\n");
				}
				if (fix_data == CASE1080p_TOP1_READFROM_FILE) {
					prm_dolby->core1_src_plane = PLANAR_X1;
					prm_dolby->core1_src_bit = BIT_10;/*read txt, not P010*/
				}

				prm_dolby->core1_mif_mmu_ena = 0;
				if (get_support_max_level() == PY_SIX_LEVEL)
					prm_dolby->corep_py_level = top1_info.py_level ==
						PY_NO_LEVEL ? PY_NO_LEVEL : PY_SIX_LEVEL;//6 or 0
				else
					prm_dolby->corep_py_level = top1_info.py_level;

				if (top1_vd_info.width > 1024 || top1_vd_info.height > 576) {
					core1b_hsize = top1_vd_info.width >> 1;
					core1b_vsize = top1_vd_info.height >> 1;
				} else {
					core1b_hsize = top1_vd_info.width;
					core1b_vsize = top1_vd_info.height;
				}
				if ((core1b_hsize > 512 || core1b_vsize > 288) &&
					prm_dolby->corep_py_level == PY_SIX_LEVEL) {
					/*level 6, corep max 512x288*/
					prm_dolby->corep_hsize = core1b_hsize >> 1;
					prm_dolby->corep_vsize = core1b_vsize >> 1;
				} else {
					prm_dolby->corep_hsize = core1b_hsize;
					prm_dolby->corep_vsize = core1b_vsize;
				}
				/*corep need corresponding core2 size*/
				prm_dolby->core2_hsize_of_corep = core2_hsize_for_corep;
				prm_dolby->core2_vsize_of_corep = core2_vsize_for_corep;
			}
			prm_dolby->core1_src_rbaddr[0] = top1_vd_info.canvasaddr[0];/*core1 read*/
			prm_dolby->core1_src_rbaddr[1] = top1_vd_info.canvasaddr[1];
			prm_dolby->corep_up_wbaddr[0] =
			top_info.up_buf[top1_info.py_id].pyup_paddr;/*for corep write*/
		}
	} else {
		prm_dolby->core1_py_level = PY_NO_LEVEL;
		prm_dolby->corep_py_level = PY_NO_LEVEL;
	}

	if (update & 2) {/*update top2*/
		prm_dolby->core2_hsize = core2_hsize;
		prm_dolby->core2_vsize = core2_vsize;
		prm_dolby->core2_detunnel = enable_detunnel;
		prm_dolby->core2_py_level = top2_info.py_level;//0:6poy 1:7py 2:nopy
		if (slice_num)
			prm_dolby->core2_slice_num = slice_num;
		if (pad_mode & 0x10)
			prm_dolby->core2_pad_mode = pad_mode & 0xF;//0:nonr+noaa,1:aa 2:nr+aa

		/*for core2 read*/
		prm_dolby->corep_up_wbaddr[1] = top_info.up_buf[top2_info.py_id].pyup_paddr;
	}

	if (debug_dolby & 0x80000)
		pr_dv_dbg("update_%d:core1 %dx%d,level %s,corep %dx%d,core2 %dx%d,s %d %d,py_id %d %d,reset %d %d %d\n",
					update,
					prm_dolby->core1_src_hsize, prm_dolby->core1_src_vsize,
					prm_dolby->core1_py_level == 0 ?
					"6" : (prm_dolby->core1_py_level == 1 ? "7" : "0"),
					prm_dolby->corep_hsize, prm_dolby->corep_vsize,
					prm_dolby->core2_hsize, prm_dolby->core2_vsize,
					prm_dolby->core2_slice_num, prm_dolby->core2_slc_mode,
					top1_info.py_id, top2_info.py_id, reset, change_flag,
					prm_dolby->fst_frm_ini);
}

void force_top1_done(void)
{
	/*if without top1, need block top1 inter interrupt,*/
	/*replace it with 1 to trigger dae*/
	if (!enable_top1 && !fake_top1_on) {
		WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IRQ_CTRL0, 0, 0, 1);/*disable wrap irq*/
		WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP1_CTRL2, 0x3f, 0, 6);
		WRITE_VPP_DV_REG_BITS(T6W_DOLBY_TOP2_PYRD_CTRL, 1, 12, 1);

		fake_top1_on = true;

		if (debug_dolby & 0x80000)
			pr_dv_dbg("fake top1 on\n");
	}
}

/*set before other regs*/
void cfg_dolby_pre_set(struct prm_dolby_top *prm_dolby)
{
	u32 top1_ctrl0 = 0;

	if (debug_dolby & 0x80000)
		pr_info("pre_set_done %d\n", pre_set_done);

	if (!pre_set_done) {
		if (!hw5_clk_on) {
			//enable tvcore clk only once
			vpu_module_clk_enable(0, DV_TVCORE, 1);
			vpu_module_clk_enable(0, DV_TVCORE, 0);
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_WRAP_GCLK, 63, 16, 6); //reg_sw_rst
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_WRAP_GCLK, 1, 0, 1); //dolby_wrap_clk_en
			WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_WRAP_GCLK, 0, 16, 6); //dolby reg_sw_rst
			hw5_clk_on = true;
			if (debug_dolby & 0x80000)
				pr_info("enable clk\n");
		}

		//VSYNC_WR_DV_REG(VPU_RDARB_MODE_L2C1, 0);//modify in uboot
		if (prm_dolby) {
			cfg_dolby_path(prm_dolby);
			prm_dolby->fst_frm_ini = 0;
			if (prm_dolby->dolby5_mode == DOLBY5_DPSS_MODE ||
				prm_dolby->dolby5_mode == DOLBY5_DPSS_MODE_2PPC) {
				cfg_dolby_axi_path(prm_dolby->dolby5_mode,
					prm_dolby->dpss_dct_ahead_dv);
				prm_dolby->fst_frm_ini_top2 = 0;
			}
		}
		if (test_hold_line)
			WRITE_VPP_DV_REG(T6W_DOLBY_TOP1_CTRL1, test_hold_line);

		if (sw_trigger_top1(prm_dolby)) {
			top1_ctrl0 = VSYNC_RD_TOP1_REG(T6W_DOLBY_TOP1_CTRL0);
			top1_ctrl0 &= 0xFCFFFFFF;
			top1_ctrl0 |= 1 << 24;
			VSYNC_WR_TOP1_REG(T6W_DOLBY_TOP1_CTRL0, top1_ctrl0);
		}
		pre_set_done = true;
	}
}

int cfg_dolby_top(struct prm_dolby_top *prm_dolby,
				 u64 *top1_reg,
				 u64 *top1b_reg,
				 u64 *top2_reg,
			     int video_enable,
			     bool reset,
			     bool toggle,
			     bool pr_done,
			     bool set_top1,
			     bool set_top2)
{
	static bool last_pr = true;/*pr status*/
	bool cur_pr = false;
	static bool last_top2_status = true;
	bool cur_top2_status = false;
	static bool last_py_enabled = true;
	static u32 delay_cnt;
	bool tmp_reset = false;
	static int tmp_cnt;
	static bool delay_reset;
	static u32 last_debug_reg;
	static u32 pr_done_continue_cnt;
	static bool last_force_bypass_precision;
	bool sw_reset = false;
	u32 debug_reg = 0;
	static bool bit31_1;

	if (!prm_dolby)
		return 0;

	if (!top2_info.core_on) {
		last_py_enabled = py_enabled;
		last_top2_py_level = top2_info.py_level;
	}

	if ((force_vsync_id & 0x1F) > 0 && (force_vsync_id & 0x0F) < 4)
		vpp_vsync_id = force_vsync_id & 0x0F;
	else
		vpp_vsync_id = get_vpp_vsync_index(0);

	if (!top1_info.core_on && !top2_info.core_on) {
		force_bypass_precision_once = false;
		miss_top1_and_bypass_pr_once = false;
		force_bypass_precision = false;
		last_force_bypass_precision = false;
	}

	if (debug_dolby & 0x1)
		pr_info("READ:reset %d,%x,%x,4809=%x,%x,%llx,pr_done %d,top1_done %d,set %d %d\n",
				reset,
				is_aml_t6w() ? READ_VPP_DV_REG(T6W_VPU_AXIRD_PATH_CTRL) :
					READ_VPP_DV_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL),
				is_aml_t6w() ? READ_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_CTRL) :
					READ_VPP_DV_REG(T6X_VPU_DOLBY_WRAP_CTRL),
				READ_VPP_DV_REG(T6W_DOLBY_TOP2_RDMA_SIZE5),
				READ_VPP_DV_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 1),
				top2_reg ? (top2_reg[2] & 0xFFFFFFFF) : 0,
				pr_done, top1_done, set_top1, set_top2);

	/**************debug reset ***************/
	if (video_enable)
		tmp_cnt++;
	if (test_dv & DEBUG_HW5_RESET_EACH_VSYNC)
		tmp_reset = true;
	else if (tmp_cnt % 600 == 0)
		tmp_reset = true;

	if (test_dv & DEBUG_HW5_TOGGLE_EACH_VSYNC)
		toggle = true;

	if ((force_update_reg & 1) && tmp_reset)
		reset = true;
	/**************debug reset done***************/

	if (enable_top1) {
		check_pr_enabled_in_setting();
	} else {
		py_enabled = false;
		force_bypass_precision = true;
	}

	if (enable_top1 && set_top1) {
		if (!top1_info.core_on) {
			//if (!wait_first_frame_top1 && !top2_info.core_on)
			reset = true;/*first frame with top1, reset*/
			/*reset pyramid index*/
			l1l4_rd_index = 0;
			l1l4_wr_index = 0;
			top1_done = false;
			top_info.isr_top1_cnt = 0;
			if (!top2_info.core_on)
				top_info.isr_cnt = 0;
		}
		if (!top1_info.core_on || reset) {
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP2_PYRD_CTRL, 1, 30, 1);
			dolby5_mmu_config(pyrd_seq_lvl67, L67_MMU_NUM);
			bit31_1 = true;
		} else if (bit31_1) {
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP2_PYRD_CTRL, 0, 30, 1);/*clear rdma buf*/
			bit31_1 = false;
		}

		if (debug_val & 0x80000000) {
			VSYNC_WR_TOP1_BITS(T6W_DOLBY_TOP2_PYRD_CTRL, 1, 30, 1);
			dolby5_mmu_config(pyrd_seq_lvl67, L67_MMU_NUM);
		}

		t6w_top1_set(prm_dolby, top1_reg, top1b_reg, reset/* || sw_reset*/,
			video_enable, toggle);

		if (!top1_info.core_on) {
			top1_info.core_on_cnt = 0;
			top1_info.core_run_cnt = 0;
		}
		if (video_enable && toggle) {
			++top1_info.core_on_cnt;
			new_top1_toggle = true;
		}
		if (video_enable)
			++top1_info.core_run_cnt;
	}
	if (!enable_top1) {
		top1_info.core_on = false;
		top1_info.core_on_cnt = 0;
		py_enabled = false;
		if (!top2_info.core_on)
			top_info.isr_cnt = 0;
	}
	if (set_top2) {
		/*******check top2 status changed start********/
		if (test_dv & DEBUG_FORCE_BYPASS_TOP2)
			cur_top2_status = false;/*top2 disable*/
		else
			cur_top2_status = true;
		if (!top2_info.core_on)
			last_top2_status = cur_top2_status;

		if (cur_top2_status && !last_top2_status) {
			//reset = true;
			sw_reset = true;
			toggle = true;
			if (debug_dolby & 1)
				pr_dv_dbg("debug top2 status changed %d->%d\n",
					last_top2_status, cur_top2_status);
		} else if (!cur_top2_status && last_top2_status) {
			toggle = true;
		}
		last_top2_status = cur_top2_status;
		/*******check top2 status changed done********/
		if (enable_top1) {
			if (last_force_bypass_precision != force_bypass_precision) {
				//reset = true;
				sw_reset = true;
				toggle = true;
				last_force_bypass_precision = force_bypass_precision;
			}
			/**************** handle cfg top1 off->on case***************/
			if (py_enabled && !last_py_enabled && top2_info.core_on &&
				top1_info.core_on_cnt <= 1) {
				if (!is_amdv_dpss_path()) {
					/*off->on step1:bypass 2vsync,step2:reset,enable precision*/
					force_bypass_precision_once = true;
					delay_cnt = 0;
					delay_reset = true;
				}
				toggle = true;
				pr_dv_dbg("precision rendering status changed %d->%d\n",
							last_py_enabled, py_enabled);
			}
			/**************** handle cfg change case done***************/

			/*******check py_level status changed during play********/
			if (last_top2_py_level != top2_info.py_level && top2_info.core_on &&
				!force_bypass_precision_once && !force_bypass_precision &&
				!force_bypass_pd_level0 &&
				!force_bypass_pd_in_game &&
				!miss_top1_and_bypass_pr_once) {
				pr_dv_dbg("top2 py_level status changed %s->%s\n",
					level_str[last_top2_py_level],
					level_str[top2_info.py_level]);
				/*off->on step1: bypass for 2vsync, step2: reset, enable precision*/
				if (last_top2_py_level == PY_NO_LEVEL) {
					/*force_bypass_precision_once = true;*/
					/*delay_reset = true;*//*no need reset now*/
					/*delay_cnt = 0;*/
					sw_reset = true;
					toggle = true;
				} else {
					/*level 6/7 change during playing, reset without delay*/
					//reset = true;
					sw_reset = true;
					toggle = true;
				}
			} else if (top2_info.py_level != PY_NO_LEVEL) {
				/*******check precision status changed********/
				/*if not get a frame in advance,bypass pyramid once*/
				if (!pr_done &&
					!force_bypass_precision_once &&
					!force_bypass_precision &&
					!force_bypass_pd_level0 &&
					!force_bypass_pd_in_game &&
					!(dolby_vision_flags & FLAG_CERTIFICATION)) {
					if (debug_dolby & 1)
						pr_dv_dbg("missed top1, bypass precision once\n");
					miss_top1_and_bypass_pr_once = true;
					pr_done_continue_cnt = 0;
				} else {
					if (miss_top1_and_bypass_pr_once && pr_done)
						++pr_done_continue_cnt;
					/*check pr done for more frames except first frame*/
					if (pr_done_continue_cnt > PR_DONE_CONTINUE_CNT ||
						top2_info.core_on_cnt < 3)
						miss_top1_and_bypass_pr_once = false;
					if ((debug_dolby & 1) && pr_done_continue_cnt)
						pr_dv_dbg("pr_done_continue_cnt %d %d\n",
						pr_done_continue_cnt,
						miss_top1_and_bypass_pr_once);
				}
				if (!top2_info.core_on) {
					last_pr = true;
					pr_done_continue_cnt = 0;
				}
				/*******check precision status changed done********/

				/*******check precision bypass changed********/
				if ((test_dv & DEBUG_FORCE_BYPASS_PRECISION_RENDERING) ||
					miss_top1_and_bypass_pr_once ||
					cfg_info[cur_pic_mode].bypass_pd_from_user)
					cur_pr = false;/*pr disable*/
				else
					cur_pr = true;
				if (cur_pr && !last_pr && !force_bypass_precision_once &&
					!force_bypass_precision && !force_bypass_pd_level0 &&
					!force_bypass_pd_in_game) {
					//reset = true;
					sw_reset = true;
					toggle = true;
					pr_dv_dbg("precision changed %d->%d\n", last_pr, cur_pr);
				} else if (!cur_pr && last_pr) {
					toggle = true;
				}
				last_pr = cur_pr;
				/*******check precision bypass changed done********/

				/*******check rdma ************/
				debug_reg = READ_VPP_DV_REG(T6W_DOLBY_TOP2_RDMA_SIZE5);
				if (debug_reg != (last_debug_reg + 1) &&
					top2_info.core_on_cnt > 2 &&
					top1_info.core_on_cnt > 2) {
					if (debug_dolby & 0x40000000)
						pr_info("rdma error cur=%x last=%x\n",
								debug_reg, last_debug_reg);
					if (!force_bypass_precision_once &&
						!miss_top1_and_bypass_pr_once &&
						!force_bypass_precision &&
						!force_bypass_pd_level0 &&
						!force_bypass_pd_in_game)
						reset = true;/*need hw reset*/
					if (debug_dolby & 0x40000000)
						pr_info("rdma error\n");
					toggle = true;
				}
				last_debug_reg = debug_reg;
				/*******check rdma done********/

				/*check RO status*/
				if (enable_ro_check) {
					last_top1_ro6 = READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_6);
					last_top1_ro5 = READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_5);
					last_top1_ro4 = READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_4);
					last_top1_ro3 = READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_3);
					last_top1_ro2 = READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_2);
					last_top1_ro1 = READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_1);
					last_top1_ro0 = READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_0);
					last_top2_ro3 = READ_VPP_DV_REG(T6W_DOLBY_TOP2_RO_3);
					last_top2_ro2 = READ_VPP_DV_REG(T6W_DOLBY_TOP2_RO_2);
					last_top2_ro1 = READ_VPP_DV_REG(T6W_DOLBY_TOP2_RO_1);
					last_top2_ro0 = READ_VPP_DV_REG(T6W_DOLBY_TOP2_RO_0);
					if (!miss_top1_and_bypass_pr_once &&
						!force_bypass_precision_once &&
						!force_bypass_precision &&
						!force_bypass_pd_level0 &&
						!force_bypass_pd_in_game &&
						cur_pr && py_enabled && top2_info.core_on &&
						cur_top2_status &&
						(last_top2_ro4 != 0x120024 ||
						last_top2_ro3 != 0x480090))
						top2_err_cnt++;
					else
						top2_err_cnt = 0;
					if (top2_err_cnt > 3 &&
						(last_top2_ro5 & 0xff000000) == 0 &&
						(last_top1_ro6 & 0xffff0000) == 0) {
						/*reset only when axi=0*/
						pr_dv_dbg("top2 err %x %x %x %x for %d\n",
							last_top2_ro5,
							last_top2_ro4, last_top2_ro3,
							last_top1_ro6, top2_err_cnt);
						//reset = true;
						top2_err_cnt = 0;
					}
				}
			}
			if (delay_reset && force_bypass_precision_once) {
				if (++delay_cnt > PR_ON_DELAY_CNT && pr_done && top1_done) {
					force_bypass_precision_once = false;
					delay_reset = false;
					delay_cnt = 0;
					//reset = true;
					sw_reset = true;
					toggle = true;
				}
			}
			if (wait_first_frame_top1) {
				/*first frame with top2,reset to avoid blank(TOP2 RO err)*/
				//if (top1_info.core_on && !top2_info.core_on)
				//	reset = true;
			}
			if (debug_dolby & 8)
				pr_dv_dbg("last_py_enabled %d %d,%d %d,%d %d,%d %d,%d %d,%d,reset %d %d\n",
				last_py_enabled, py_enabled,
				pr_done, top1_done,
				force_bypass_precision_once, miss_top1_and_bypass_pr_once,
				force_bypass_precision, force_bypass_pd_level0,
				force_bypass_pd_in_game,
				cur_pr, cur_top2_status,
				reset, sw_reset);
		}

		if (!py_enabled && last_py_enabled)
			toggle = true;
		last_py_enabled = py_enabled;

		last_top2_py_level = top2_info.py_level;

		/*For cert: first frame with top1, not enable top2*/
		if (!wait_first_frame_top1 ||
			(!enable_top1 || top1_info.core_on || top2_info.core_on)) {
			if (top2_info.core_on_cnt == 0)
				reset = true;/*first frame with top2, reset*/

			if (prm_dolby->dolby5_mode != DOLBY5_DPSS_MODE &&
				prm_dolby->dolby5_mode != DOLBY5_DPSS_MODE_2PPC &&
				(prm_dolby->fst_frm_ini_top2 || !top2_info.core_on)) {
				cfg_dolby_axi_path(prm_dolby->dolby5_mode,
					prm_dolby->dpss_dct_ahead_dv);
				reset = true;
				prm_dolby->fst_frm_ini_top2 = 0;
			}
			t6w_top2_set(prm_dolby, top2_reg, video_enable,
					reset || sw_reset, toggle, pr_done);
			if (video_enable && toggle)
				++top2_info.core_on_cnt;
			if (video_enable)
				++top2_info.core_run_cnt;
		}
	}
	return 0;
}

/*for hdmi cert: not update index when hist not change*/
/*sometimes even if frame is same but hist changed*/
/*maybe vdin/rx not stable or relatedto core1 processing*/
/*so need to check few more hist*/
bool check_hist_changed(u8 *cur_hist)
{
#define MAX_HIST_CHECK_COUNT 3

	static u32 hist_repeat_cnt;
	static u8 last_valid_hist[256];
	bool changed = false;
	int i = 0;

	if (debug_dolby & 0x1000000) {
		pr_info("%s: hist_repeat_cnt %d\n", __func__, hist_repeat_cnt);
		for (i = 0; i < 32; i++)
			pr_info("cur hist: %d, %d, %d, %d, %d, %d, %d, %d\n",
				cur_hist[i * 8],
				cur_hist[i * 8 + 1],
				cur_hist[i * 8 + 2],
				cur_hist[i * 8 + 3],
				cur_hist[i * 8 + 4],
				cur_hist[i * 8 + 5],
				cur_hist[i * 8 + 6],
				cur_hist[i * 8 + 7]);
		for (i = 0; i < 32; i++)
			pr_info("last hist: %d, %d, %d, %d, %d, %d, %d, %d\n",
				dv5_md_hist.hist[i * 8],
				dv5_md_hist.hist[i * 8 + 1],
				dv5_md_hist.hist[i * 8 + 2],
				dv5_md_hist.hist[i * 8 + 3],
				dv5_md_hist.hist[i * 8 + 4],
				dv5_md_hist.hist[i * 8 + 5],
				dv5_md_hist.hist[i * 8 + 6],
				dv5_md_hist.hist[i * 8 + 7]);
		for (i = 0; i < 32; i++)
			pr_info("last valid hist: %d, %d, %d, %d, %d, %d, %d, %d\n",
				last_valid_hist[i * 8],
				last_valid_hist[i * 8 + 1],
				last_valid_hist[i * 8 + 2],
				last_valid_hist[i * 8 + 3],
				last_valid_hist[i * 8 + 4],
				last_valid_hist[i * 8 + 5],
				last_valid_hist[i * 8 + 6],
				last_valid_hist[i * 8 + 7]);
	}
	if (memcmp(cur_hist, last_valid_hist, 256)) {
		if (memcmp(cur_hist, &dv5_md_hist.hist[0], 256))
			hist_repeat_cnt = 0;
		else
			++hist_repeat_cnt;

		if (hist_repeat_cnt >= MAX_HIST_CHECK_COUNT) {
			hist_repeat_cnt = 0;
			memcpy(last_valid_hist, cur_hist, 256);
			changed = true;
			pr_dv_dbg("new hist\n");
		}
	} else {
		hist_repeat_cnt = 0;
	}
	return changed;
}

#define FOR_DEBUG 0

#if FOR_DEBUG
u16 histogram[128];
u8 hist[256];
#endif
static void set_l1l4_hist_t6w(void)
{
	u32 index;
	u32 i;
	u32 metadata0;
	u32 metadata1;
	u8 hist_test[256];
	static bool hist_changed;
	static u32 changed_count;

	if ((!tv_hw5_setting || !enable_top1) && !fix_data)
		return;

	metadata0 = READ_VPP_DV_REG(T6W_DOLBY5_CORE1_L1_MINMAX);
	metadata1 = READ_VPP_DV_REG(T6W_DOLBY5_CORE1_L1_MID_L4);

	if (new_top1_toggle) {
		new_top1_toggle = false;
		if (top1_info.core_on_cnt > 1) {
			if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
				(test_dv & HDMI_ONLY_UPDATE_HIST_FOR_NEW_FRAME)) {
				/*hdmi case, check hist and only update index for new frame*/
				memcpy(&hist_test[0], dv5_md_hist.hist_vaddr[0], 256);/*cur hist*/
				if (check_hist_changed(hist_test) &&
					top1_info.core_on_cnt > 4) {/*compare with last hist*/
					hist_changed = true;
					changed_count = 0;
					memcpy(&dv5_md_hist.hist[0],
						dv5_md_hist.hist_vaddr[0], 256);
					if (debug_dolby & 1)
						pr_info("hist change!\n");
					return;
				} else if (hist_changed) {
					/*update after 4 times because checking vf_crc repeat 3*/
					changed_count++;
					if (debug_dolby & 1)
						pr_info("changed_count %d\n", changed_count);
					if (changed_count > 4) {
						l1l4_wr_index = (l1l4_wr_index + 1) %
							HIST_BUF_COUNT;
						hist_changed = false;
					} else {
						return;
					}
				}
			} else {
				l1l4_wr_index = (l1l4_wr_index + 1) % HIST_BUF_COUNT;
			}
		}
	}

	index = l1l4_wr_index;

	dv5_md_hist.l1l4_md[index][0] = metadata0 & 0xffff;
	dv5_md_hist.l1l4_md[index][1] = metadata0 >> 16;
	dv5_md_hist.l1l4_md[index][2] = metadata1 & 0xffff;
	dv5_md_hist.l1l4_md[index][3] = metadata1 >> 16;

	memcpy(&dv5_md_hist.hist[0], dv5_md_hist.hist_vaddr[0], 256);

	for (i = 0; i < 256 / 2; i++)
		dv5_md_hist.histogram[index][i] = dv5_md_hist.hist[i * 2 + 0] |
			(dv5_md_hist.hist[i * 2 + 1] << 8);

	if (debug_dolby & 0x100000) {
		pr_info("set top1 hist[%d]:\n", index);
		for (i = 0; i < 128 / 8; i++)
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d\n",
				dv5_md_hist.histogram[index][i * 8],
				dv5_md_hist.histogram[index][i * 8 + 1],
				dv5_md_hist.histogram[index][i * 8 + 2],
				dv5_md_hist.histogram[index][i * 8 + 3],
				dv5_md_hist.histogram[index][i * 8 + 4],
				dv5_md_hist.histogram[index][i * 8 + 5],
				dv5_md_hist.histogram[index][i * 8 + 6],
				dv5_md_hist.histogram[index][i * 8 + 7]);
		pr_info("meta0: 0x%x, meta1: 0x%x, l1l4_index %d/%d\n",
			metadata0, metadata1, l1l4_rd_index, l1l4_wr_index);

		#if FOR_DEBUG
			index = index ^ 1;
			memcpy(&hist[0], dv5_md_hist.hist_vaddr[index], 256);
			for (i = 0; i < 256 / 2; i++)
				histogram[i] = hist[i * 2 + 0] |
					(hist[i * 2 + 1] << 8);
			pr_info("top1 hist[%d]:\n", index);
			for (i = 0; i < 3; i++)
				pr_info("%d, %d, %d, %d, %d, %d, %d, %d\n",
					histogram[i * 8],
					histogram[i * 8 + 1],
					histogram[i * 8 + 2],
					histogram[i * 8 + 3],
					histogram[i * 8 + 4],
					histogram[i * 8 + 5],
					histogram[i * 8 + 6],
					histogram[i * 8 + 7]);
		#endif
	}
}

irqreturn_t amdv_t6w_isr(int irq, void *dev_id)
{
	static struct timeval last_time;
	struct timeval cur_time;
	unsigned long time_use = 0;
	u32 buf_cnt = TOP_BUF_CNT_VD1;

	if (is_amdv_dpss_path())
		buf_cnt = TOP_BUF_CNT;

	top_info.isr_cnt++;
	if (debug_dolby & 0x1)
		pr_info("amdv_isr_cnt %d\n", top_info.isr_cnt);

	if (is_amdv_dpss_path()) {
		if (prm_dolby.core2_slice_num == 1)
			top_info.vf_info[(top_info.isr_cnt - 1) % buf_cnt].top2_done = true;
		else if (prm_dolby.core2_slice_num == 2 && (top_info.isr_cnt % 2 == 0))
			top_info.vf_info[((top_info.isr_cnt / 2) - 1) % buf_cnt].top2_done =
			true;
		else if (prm_dolby.core2_slice_num == 4 && (top_info.isr_cnt % 4 == 0))
			top_info.vf_info[((top_info.isr_cnt / 4) - 1) % buf_cnt].top2_done =
			true;
	} else {
		if (fix_data == CASE1080p_TOP1_READFROM_FILE) {
			/*vf_info:frame1 frame2, but the first vsync only for top1*/
			top_info.vf_info[(top_info.isr_cnt - 1) % buf_cnt].top2_done = true;
		} else if (wait_first_frame_top1) {
			/*vf_info:frame1 frame2*/
			/*isr cnt=1 is for frame1, it in at the front of vf_info*/
			top_info.vf_info[(top_info.isr_cnt - 1) % buf_cnt].top2_done = true;
		} else {
			/*vf_info:frame2 3, not contain frame1 top1 because frame top1 is bypassed*/
			/*isr cnt=2 is for frame2, it in at the front of vf_info*/
			if (top_info.isr_cnt >= 2)
				top_info.vf_info[(top_info.isr_cnt - 2) % buf_cnt].top2_done =
				true;
		}
	}
	if (debug_dolby & 0x4000) {
		pr_info("top2-isr: TOP1 RO %x %x %x %x %x %x, %x\n",
		READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_6),
		READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_5),
		READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_4),
		READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_3),
		READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_2),
		READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_1),
		READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_0));
		pr_info("top2-isr: corep RO %x, %x %x %x %x %x\n",
		READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO),
		READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_0),
		READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_1),
		READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_2),
		READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_3),
		READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_4));
	}
	if ((dolby_vision_flags & FLAG_CERTIFICATION) || hw5_reg_from_file) {
		pr_dv_dbg("crc %x %x %x %x\n",
		READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CRC_CNTRL),
		READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CRC_OUT_FRM),
		is_aml_t6w() ? READ_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_CTRL) :
			READ_VPP_DV_REG(T6X_VPU_DOLBY_WRAP_CTRL),
		is_aml_t6w() ? READ_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_RO_CRC) :
			READ_VPP_DV_REG(T6X_VPU_DOLBY_WRAP_RO_CRC));
	}

	if (trace_amdv_isr) {
		do_gettimeofday(&cur_time);
		if (top_info.isr_cnt > 1) {
			time_use = (cur_time.tv_sec - last_time.tv_sec) * 1000000 +
				(cur_time.tv_usec - last_time.tv_usec);
			pr_dv_dbg("amdv isr time: %5ld us\n", time_use);
			if (time_use > trace_amdv_isr * 1000 * 3 / 2)
				pr_dv_dbg("amdv isr too late: %5ld us\n", time_use);
		}
		last_time = cur_time;
	}
	WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IRQ_CTRL3, 1, 0, 1);//top2 int, clear,todo

	return IRQ_HANDLED;
}

irqreturn_t amdv_t6w_top1_isr(int irq, void *dev_id)
{
	static struct timeval last_time;
	struct timeval cur_time;
	unsigned long time_use = 0;
	struct vframe_s *cur_vf = NULL;
	u32 ori_w;
	u32 ori_h;
	u32 ret;
	u32 buf_cnt = TOP_BUF_CNT_VD1;

	if (is_amdv_dpss_path())
		buf_cnt = TOP_BUF_CNT;

	top_info.isr_top1_cnt++;
	top_info.vf_info[(top_info.isr_top1_cnt - 1) % buf_cnt].top1_done = true;
	cur_vf = top_info.vf_info[(top_info.isr_top1_cnt - 1) % buf_cnt].vf;

	if (debug_dolby & 0x1)
		pr_info("amdv_isr_top1_cnt %d, vf %px %px\n",
		top_info.isr_top1_cnt, dpss_cur_vf, cur_vf);

	if (trace_amdv_isr) {
		do_gettimeofday(&cur_time);
		if (top_info.isr_top1_cnt > 1) {
			time_use = (cur_time.tv_sec - last_time.tv_sec) * 1000000 +
				(cur_time.tv_usec - last_time.tv_usec);
			pr_dv_dbg("amdv top1 isr time: %5ld us\n", time_use);
			if (time_use > trace_amdv_isr * 1000 * 3 / 2)
				pr_dv_dbg("amdv top1 isr too late: %5ld us\n", time_use);
		}
		last_time = cur_time;
	}

	if (enable_top1) {
		top1_done = true;
		set_l1l4_hist_t6w();

		if (is_amdv_dpss_path() && cur_vf && (test_dv & MANUAL_DPE_START)) {
			spin_lock(&cp_lock);

			ori_w = (cur_vf->type & VIDTYPE_COMPRESS) ?
				cur_vf->compWidth : cur_vf->width;
			ori_h = (cur_vf->type & VIDTYPE_COMPRESS) ?
				cur_vf->compHeight : cur_vf->height;

			/*top2 parser+cp*/
			ret = amdv_update_metadata(cur_vf, 0, false);/*todo,*/
			if (ret == 0)
				amdv_set_toggle_flag(1);
			/*top2 reg set*/
			amdolby_vision_process_hw5(NULL, cur_vf,
					ori_w << 16 | ori_h, 1);
#if ENABLE_DPSS
			dpss_v_rdma_config_b();
#endif
			spin_unlock(&cp_lock);
		}

		if (debug_dolby & 0x4000) {
			pr_info("top1-isr: TOP1 RO %x %x %x %x %x %x, %x\n",
			READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_6),
			READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_5),
			READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_4),
			READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_3),
			READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_2),
			READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_1),
			READ_VPP_DV_REG(T6W_DOLBY_TOP1_RO_0));
			pr_info("top1-isr: corep RO %x, %x %x %x %x %x\n",
			READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO),
			READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_0),
			READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_1),
			READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_2),
			READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_3),
			READ_VPP_DV_REG(T6W_DOLBY_TOP2_PYUP_RO_4));
		}

		WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_IRQ_CTRL1, 1, 0, 1);//top1 int, clear,todo
	}
	return IRQ_HANDLED;
}

