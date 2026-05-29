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
#include <linux/platform_device.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include "amdv_regs_hw5.h"
#include "../amvecm/arch/vpp_regs.h"
#include "../amvecm/arch/vpp_hdr_regs.h"
#include "../amvecm/arch/vpp_dolbyvision_regs.h"
#include "../amvecm/amcsc.h"
#include "../amvecm/reg_helper.h"
#include <linux/amlogic/media/registers/regs/viu_regs.h>
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#include <linux/amlogic/media/di/dpss_interface.h>
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
#include <linux/amlogic/media/canvas/canvas.h>
#include "amdv.h"
#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif
#include "amdv_hw5_t6w.h"
#include "amdv_regs_hw5_t6w.h"
#include "amdv_regs_hw5_t6x.h"

static struct dv_atsc p_atsc_md;

bool enable_top1_scale = true;
bool force_corep_max_level6;
u32 hw5_reg_from_file;

u32 force_update_top2 = true;/*must update DOLBY5_CORE2_REG_BASE0+2,3,4*/

struct dv5_top1_vd_info top1_vd_info;
struct vframe_s *vf_tmp;
static u32 last_vf_valid_crc_top1;
struct vframe_s *dpss_cur_vf;
#define signal_cuva ((vf->signal_type >> 31) & 1)
#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)

struct dynamic_cfg_s dynamic_config_new;
struct dynamic_cfg_s dynamic_darkdetail = {16, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0};
struct dynamic_cfg_s dynamic_gd = {128, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0};

const char level_str[4][10] = {
	"6",
	"7",
	"0",
	"invalid"
};

static char meta_buf[1024];

bool update_top2_cfg;
struct prm_dolby_top prm_dolby;
enum dolby_work_mode top_dd_work_mode;
bool ignore_top_path_change;
u32 delay_update_path;
bool update_path_done;
bool enable_2ppc = true;
u32 force_src_chroma_format;
u32 force_src_bdp;
bool dither_en;
u32 dithering_out_depth;
u32 rgb2yuv_offset0;
u32 rgb2yuv_offset1;
u32 rgb2yuv_offset2;

const char dolby_work_mode_str[12][20] = {
	"wrap-byps",
	"core-byps",
	"vd1",
	"vd1-2ppc",
	"vdin",
	"vdin-2ppc",
	"dpss",
	"dpss-2ppc",
	"dpss-prl",
	"dpss-prl 2ppc",
	"dpss-di",
	"dpss-di-2ppc"
};

void init_prm_dolby(struct dpss_info_s *dpss_info)
{
	cfg_dolby_ini(&prm_dolby, dpss_info);

	if (dpss_info)/*for dpss mode, set clk before top1/top2*/
		cfg_dolby_pre_set(&prm_dolby);
}

void update_dd_dpss_info(struct dpss_info_s *dpss_info)
{
	if (dpss_info) {
		if (prm_dolby.frm_hsize_sel != dpss_info->frm_hsize_sel)
			prm_dolby.frm_hsize_sel = dpss_info->frm_hsize_sel;
		if (prm_dolby.core2_slice_num != dpss_info->slice_num)
			prm_dolby.core2_slice_num = dpss_info->slice_num;
		if (prm_dolby.core2_pad_mode != dpss_info->pad_mode)
			prm_dolby.core2_pad_mode = dpss_info->pad_mode;
		if (prm_dolby.dpss_tbc_mode != dpss_info->tbc_mode)
			prm_dolby.dpss_tbc_mode = dpss_info->tbc_mode;
		if (prm_dolby.dpss_direct_mode != dpss_info->direct_mode)
			prm_dolby.dpss_direct_mode = dpss_info->direct_mode;
		if (prm_dolby.dpss_dct_ahead_dv != dpss_info->dct_ahead_dv_mode)
			prm_dolby.dpss_dct_ahead_dv = dpss_info->dct_ahead_dv_mode;

		pr_dv_dbg("update dpss_info %d %d %d %d %d %d\n",
			 dpss_info->frm_hsize_sel, dpss_info->slice_num,
			 dpss_info->pad_mode, dpss_info->tbc_mode,
			 dpss_info->direct_mode, dpss_info->dct_ahead_dv_mode);
	}
}

void update_dd_mode(enum dolby_work_mode mode)
{
	if (ignore_top_path_change) {
		update_path_done = true;
		delay_update_path = 0;
		return;
	}

	if (is_aml_t6x() && enable_2ppc) {
		if (mode == DOLBY5_DPSS_MODE)
			mode = DOLBY5_DPSS_MODE_2PPC;
		if (mode == DOLBY5_VD1_MODE)
			mode = DOLBY5_VD1_MODE_2PPC;
	}
	if (debug_dolby & 0x1)
		pr_dv_dbg("set top_dd_work_mode %s,cur:%s,%s,delay %d,on %d\n",
			dolby_work_mode_str[mode],
			dolby_work_mode_str[top_dd_work_mode],
			dolby_work_mode_str[prm_dolby.dolby5_mode],
			delay_update_path, dolby_vision_on);

	if (delay_update_path &&
		(mode == DOLBY5_DPSS_MODE || mode == DOLBY5_DPSS_MODE_2PPC)) {
		/*switch from vd1 to dpss is in process,ignore*/
		return;
	}

	if (mode != prm_dolby.dolby5_mode &&
		dolby_vision_on &&
		(mode == DOLBY5_DPSS_MODE || mode == DOLBY5_DPSS_MODE_2PPC)) {
		/*switch to dpss during playing, should delay update*/
		/*delay, update in vsync*/
		delay_update_path = 1;
		update_path_done = false;
		pr_dv_dbg("delay update path\n");
		return;
	}
	if ((prm_dolby.dolby5_mode == DOLBY5_DPSS_MODE ||
		prm_dolby.dolby5_mode == DOLBY5_DPSS_MODE_2PPC) &&
		dolby_vision_on &&
		(mode == DOLBY5_VD1_MODE || mode == DOLBY5_VD1_MODE_2PPC)) {
		/*switch to VD1 during playing, bypass dv immediately*/
		pr_dv_dbg("dpss switch to VD1 during playing, bypass first\n");
		amdolby_vision_process_hw5(NULL, NULL, 0, 0);
	}

	/*ignore bypass cmd from dpss_process, bypass should based on frame*/
	if (mode != DOLBY5_WRAP_BYPS) {
		top_dd_work_mode = mode;
		prm_dolby.dolby5_mode = mode;
	}

	update_path_done = true;
	delay_update_path = 0;
}

bool get_dd_mode_update_status(void)
{
	if (debug_dolby & 0x80000)
		pr_dv_dbg("get update path done %d\n", update_path_done);

	return update_path_done ? true : false;
}

void dump_top1_frame(int force_w, int force_h)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	loff_t pos;
#endif
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	u16 *data_y_16;
	u16 *data_uv_16;
	int i;
	struct vframe_s *vf = vf_tmp;
	int data_w = 1;

#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	snprintf(name_buf, sizeof(name_buf), "/data/src_vframe.yuv");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
#endif

	if (vf && vf->canvas0_config[0].phy_addr) {
		pr_d_log(1, "vf %px: size %d %d %d %d %d %d\n", vf, force_w, force_h,
			vf->canvas0_config[0].width, vf->canvas0_config[0].height,
			vf->canvas0_config[1].width, vf->canvas0_config[1].height);
		if (vf->canvas0_config[0].block_mode)
			pr_d_log(1, "data in block mode, maybe disaplay error\n");

		data_w = vf->canvas0_config[0].bit_depth ? 2 : 1;/*420-10bit or 8bit*/

		if (force_w > 0 && force_w <= vf->canvas0_config[0].width &&
			force_h > 0 && force_h <= vf->canvas0_config[0].height)
			data_size_y = force_w * force_h * data_w;
		else
			data_size_y = vf->canvas0_config[0].width *
				vf->canvas0_config[0].height * data_w;

		data_size_uv = data_size_y / 2;

		data_y = codec_mm_vmap(vf->canvas0_config[0].phy_addr, data_size_y);
		data_uv = codec_mm_vmap(vf->canvas0_config[1].phy_addr, data_size_uv);
		data_y_16 = (u16 *)data_y;
		data_uv_16 = (u16 *)data_uv;
		if (!data_y || !data_uv) {
			pr_d_log(1, "error map data\n");
			return;
		}
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
		pos = fp->f_pos;
		kernel_write(fp, data_y, data_size_y, &pos);
		fp->f_pos = pos;
		pos = fp->f_pos;
		kernel_write(fp, data_uv, data_size_uv, &pos);
		fp->f_pos = pos;
		filp_close(fp, NULL);
#endif
		if (debug_dolby & 0x80000) {
			pr_d_log(0x80000, "y8===>size %d\n", data_size_y);
			for (i = 0; i < 1200; i += 8)
				pr_d_log(0x80000, "%02x %02x %02x %02x %02x %02x %02x %02x\n",
				data_y[i],
				data_y[i + 1],
				data_y[i + 2],
				data_y[i + 3],
				data_y[i + 4],
				data_y[i + 5],
				data_y[i + 6],
				data_y[i + 7]);
			pr_d_log(0x80000, "y16===>size %d\n", data_size_y);
			for (i = 0; i < 600; i += 8)
				pr_d_log(0x80000, "%02x %02x %02x %02x %02x %02x %02x %02x\n",
				data_y_16[i],
				data_y_16[i + 1],
				data_y_16[i + 2],
				data_y_16[i + 3],
				data_y_16[i + 4],
				data_y_16[i + 5],
				data_y_16[i + 6],
				data_y_16[i + 7]);
			pr_d_log(0x80000, "uv16===>size %d\n", data_size_uv);
			for (i = 0; i < 600; i += 8)
				pr_d_log(0x80000, "%02x %02x %02x %02x %02x %02x %02x %02x\n",
				data_uv_16[i],
				data_uv_16[i + 1],
				data_uv_16[i + 2],
				data_uv_16[i + 3],
				data_uv_16[i + 4],
				data_uv_16[i + 5],
				data_uv_16[i + 6],
				data_uv_16[i + 7]);
		}
		codec_mm_unmap_phyaddr(data_y);
		codec_mm_unmap_phyaddr(data_uv);
	}
}

static void get_vf_size(struct vframe_s *vf, u32 *h_size, u32 *v_size)
{
	if (vf && h_size && v_size) {
		if (vf->type & VIDTYPE_COMPRESS) {
			if (is_src_crop_valid(vf->src_crop) && !is_amdv_dpss_path()) {
				*h_size = vf->compWidth -
					vf->src_crop.left - vf->src_crop.right;
				*v_size = vf->compHeight -
					vf->src_crop.top - vf->src_crop.bottom;
			} else {
				*h_size = vf->compWidth;
				*v_size = vf->compHeight;
			}
		} else {
			*h_size = vf->width;
			*v_size = vf->height;
		}
	}
}

static void get_top1_vd_info(struct vframe_s *vf,
	struct dv5_top1_vd_info *top1_vd_info)
{
	u32 burst_len;
	u32 canvas_w;

	if (!vf)
		return;

	top1_vd_info->width = vf->width;
	top1_vd_info->height = vf->height;
	if (is_src_crop_valid(vf->src_crop) && !is_amdv_dpss_path()) {
		top1_vd_info->compWidth = vf->compWidth -
			vf->src_crop.left - vf->src_crop.right;
		top1_vd_info->compHeight = vf->compHeight -
			vf->src_crop.top - vf->src_crop.bottom;
	} else {
		top1_vd_info->compWidth = vf->compWidth;
		top1_vd_info->compHeight = vf->compHeight;
	}
	if (prm_dolby.vds_4k1k_en) {
		top1_vd_info->height = top1_vd_info->height >> 1;
		top1_vd_info->compHeight = top1_vd_info->compHeight >> 1;
	}
	top1_vd_info->type = vf->type;
	top1_vd_info->bitdepth = vf->canvas0_config[0].bit_depth == 1 ? 10 : 8;/*1:P010*/
	top1_vd_info->plane = vf->plane_num;
	top1_vd_info->canvasaddr[0] = vf->canvas0_config[0].phy_addr;
	top1_vd_info->canvasaddr[1] = vf->canvas0_config[1].phy_addr;
	top1_vd_info->canvasaddr[2] = vf->canvas0_config[2].phy_addr;
	top1_vd_info->blk_mode = vf->canvas0_config[0].block_mode;
	top1_vd_info->buf_width = vf->canvas0_config[0].width;
	top1_vd_info->buf_height = vf->canvas0_config[0].height;
	top1_vd_info->linear_mode = (vf->flag & VFRAME_FLAG_VIDEO_LINEAR) ? 1 : 0;
	top1_vd_info->endian = vf->canvas0_config[0].endian;/*decoder is not set in all case*/

	/* vd mif burst len is 2 as default */
	burst_len = 2;
	if (vf->canvas0Addr != (u32)-1)
		canvas_w = canvas_get_width(vf->canvas0Addr & 0xff);
	else
		canvas_w = vf->canvas0_config[0].width;

	if (canvas_w % 32)
		burst_len = 0;
	else if (canvas_w % 64)
		burst_len = 1;
	if (vf->canvas0_config[0].block_mode)
		burst_len = vf->canvas0_config[0].block_mode;
	top1_vd_info->burst_len = burst_len;

	/*all hdmi idk cases are 444-10bit */
	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		top1_vd_info->buf_width = top1_vd_info->width;/*no canvas0_config for hdmi*/
		top1_vd_info->buf_height = top1_vd_info->height;
		top1_vd_info->bitdepth = 10;
		top1_vd_info->blk_mode = 0;
		top1_vd_info->type &= (~VIDTYPE_VIU_NV21);
		top1_vd_info->type &= (~VIDTYPE_VIU_NV12);
		top1_vd_info->type &= (~VIDTYPE_VIU_422);
		top1_vd_info->type |= VIDTYPE_VIU_444;
	}

	if (debug_dolby & 0x80000) {
		pr_info("vf %px,w,h:%d,%d,cw,ch:%d,%d,%d,%d,%d,%d,%d,%d,type:%x,%x,%x,f %x,bdp:%d,%d,%d,p:%d,addr:%lx,%lx,%lx,b %d %d %d\n",
			vf,
			top1_vd_info->width,
			top1_vd_info->height,
			top1_vd_info->compWidth,
			top1_vd_info->compHeight,
			vf->canvas0_config[0].width,
			vf->canvas0_config[0].height,
			vf->canvas0_config[1].width,
			vf->canvas0_config[1].height,
			vf->canvas0_config[2].width,
			vf->canvas0_config[2].height,
			vf->type,
			vf->type_ext,
			top1_vd_info->type,
			vf->flag,
			vf->bitdepth,
			vf->bitdepth_dw,
			top1_vd_info->bitdepth,
			top1_vd_info->plane,
			top1_vd_info->canvasaddr[0],
			top1_vd_info->canvasaddr[1],
			top1_vd_info->canvasaddr[2],
			top1_vd_info->blk_mode,
			top1_vd_info->burst_len,
			vf->canvas0_config[0].endian);
	}
}

bool update_num_downsamplers(u32 w, u32 h)
{
	u8 dw = 4;/*default*/
	bool ret = true;

	if ((w == top1_vd_info.width * 4 && h == top1_vd_info.height * 4) ||
		((((w + 7) >> 3) << 3) == top1_vd_info.width * 4 &&
		(((h + 7) >> 3) << 3) == top1_vd_info.height * 4)) {
		num_downsamplers = 2;/*number of times video was downscaled before top1*/
		dw = 4;/*1/4 * 1/4 dw*/
	} else if ((w == top1_vd_info.width * 2 && h == top1_vd_info.height * 2) ||
		((((w + 7) >> 3) << 3) == top1_vd_info.width * 2 &&
		(((h + 7) >> 3) << 3) == top1_vd_info.height * 2)) {
		num_downsamplers = 1;
		dw = 2;/*1/2 * 1/2 dw*/
	} else if ((w == top1_vd_info.width && h == top1_vd_info.height) ||
		((((w + 7) >> 3) << 3) == top1_vd_info.width &&
		(((h + 7) >> 3) << 3) == top1_vd_info.height)) {
		num_downsamplers = 0;
		dw = 0;/*no dw*/
	} else {
		/*top2 size is not a multiple of top1 size*/
		num_downsamplers = 2;
		/*ret = false;*/
	}
	if (debug_dolby & 0x80000)
		pr_info("dw %d,set num_ds %d,size %d %d %d %d\n", dw, num_downsamplers,
		w, h, top1_vd_info.width, top1_vd_info.height);

	if (debug_dolby & 0x80000)
		pr_info("dw %d,set num_ds %d\n", dw, num_downsamplers);
	if (top1_scale) {
		if (num_downsamplers == 0)
			num_downsamplers = 1;/*1:1 dw + top1 scaler*/
		else if (num_downsamplers == 1)
			num_downsamplers = 2;/*1/2*1/2 dw + top1 scaler*/
		else if (num_downsamplers == 2)
			num_downsamplers = 3;/*1/4*1/4 dw + top1 scaler*/
		if (debug_dolby & 0x80000)
			pr_info("top1 scaler, update num_ds to %d\n", num_downsamplers);
	}
	return ret;
}

/*return true: need top1 rdmif scale, false: no scale*/
bool check_top1_scale(u32 w, u32 h)
{
	u8 decoder_scale = 2;/*default*/
	bool scale = false;

	if (!enable_top1_scale)
		return false;

	if ((w == top1_vd_info.width * 4 && h == top1_vd_info.height * 4) ||
		((((w + 7) >> 3) << 3) == top1_vd_info.width * 4 &&
		(((h + 7) >> 3) << 3) == top1_vd_info.height * 4)) {
		decoder_scale = 2;
	} else if ((w == top1_vd_info.width * 2 && h == top1_vd_info.height * 2) ||
		((((w + 7) >> 3) << 3) == top1_vd_info.width * 2 &&
		(((h + 7) >> 3) << 3) == top1_vd_info.height * 2)) {
		decoder_scale = 1;
	} else if ((w == top1_vd_info.width && h == top1_vd_info.height) ||
		((((w + 7) >> 3) << 3) == top1_vd_info.width &&
		(((h + 7) >> 3) << 3) == top1_vd_info.height)) {
		decoder_scale = 0;
	}
	if ((decoder_scale > 0 &&
		top1_vd_info.width < 960 && top1_vd_info.height < 540 &&
		top1_vd_info.width > 512 && top1_vd_info.height > 288) &&
		is_aml_t3x()) {
		/*1. T3X:Some special res, pyramid start line too large top2 stuck at level7*/
		/*need work with level6, so need downscaler once more*/
		scale = true;
	} else if (decoder_scale == 0 && top1_vd_info.width > 512 &&
		top1_vd_info.width <= 1024) {
		/*2. For 1024 >= width > 512, ko force pyramid setting with level 6*/
		/*if dw is 1:1,need top1 scale*/
		scale = true;
	} else {
		scale = false;
	}
	if (is_aml_t6w() || is_aml_t6x()) {/*currently, rdmif scale only for t3x*/
		if (is_aml_t6x() && scale)
			force_corep_max_level6 = true;/*t6x force level6 in special resolution*/
		else
			force_corep_max_level6 = false;

		if (debug_dolby & 0x80000)
			pr_dv_dbg("scale %d %d,force %d\n",
			decoder_scale, scale, force_corep_max_level6);

		return false;
	}
	return scale;
}

void update_top1_onoff(struct vframe_s *vf)
{
	u32 w;
	u32 h;
	u32 ori_w;
	u32 ori_h;
	static u32 last_py_level = PY_LEVEL_INVALID;
	u32 cfg_enable_top1 = 0;
	bool bypass_pr = false;

	if (vf) {
		if (vf->type & VIDTYPE_COMPRESS) {
			if (is_src_crop_valid(vf->src_crop) && !is_amdv_dpss_path()) {
				w = vf->compWidth -
					vf->src_crop.left - vf->src_crop.right;
				h = vf->compHeight -
					vf->src_crop.top - vf->src_crop.bottom;
			} else {
				w = vf->compWidth;
				h = vf->compHeight;
			}
			ori_w = vf->compWidth;
			ori_h = vf->compHeight;
		} else {
			w = vf->width;
			h = vf->height;
			ori_w = vf->width;
			ori_h = vf->height;
		}
		if (is_amdv_dpss_path()) {
			w = prm_dolby.dpss_real_h;
			h = prm_dolby.dpss_real_v;
			if (prm_dolby.vds_4k1k_en) {
				h = h >> 1;
				ori_h = ori_h >> 1;
			}
		}

		cfg_enable_top1 = check_cfg_enabled_top1();
		if (cfg_enable_top1) {
			get_top1_vd_info(vf, &top1_vd_info);
			if (check_top1_scale(w, h)) {
				if (debug_dolby & 0x80000)
					pr_dv_dbg("need top1 mif scale, %dx%d\n",
						top1_vd_info.width, top1_vd_info.height);
				top1_scale = 1;
			} else {
				top1_scale = 0;
			}
			enable_top1 = true;/*cur cfg enable pyramid*/
			if (!update_num_downsamplers(w, h))
				enable_top1 = false;/*size not align, disable enable_top1*/
			vf->src_fmt.downsamplers = num_downsamplers;
		} else {
			enable_top1 = false;
		}
		if (vf->flag & VFRAME_FLAG_GAME_MODE) {
			if (debug_dolby & 0x80000)
				pr_dv_dbg("game mode,flag %x,bypass top1\n", vf->flag);
			/*enable_top1 = false;*/
			/*game need lowlatency, not support PD*/
			/*If UI switch to IQ when source is game, need force bypss PD*/
			force_bypass_pd_in_game = true;
		} else {
			force_bypass_pd_in_game = false;
		}

		if (force_top1_enable == 1) {
			if (cfg_enable_top1 == CFG_NONE) {
				if (debug_dolby & 1)
					pr_dv_dbg("cfg not enable top1,not allow enable!\n");
			} else {
				enable_top1 = true;
			}
		} else if (force_top1_enable == 2) {
			enable_top1 = false;
		} else if (force_top1_enable == 3) {
			enable_top1 = true;
		}

		if (enable_top1) {/*rtl only consider xres > 512*/
			if (top1_vd_info.width > 512/*&& top1_vd_info.height > 288*/&& !bypass_pr) {
				vf->src_fmt.py_level = PY_SEVEN_LEVEL;
				if (top1_vd_info.width >> top1_scale > 512/* &&*/
					/*top1_vd_info.height >> top1_scale > 288*/)
					vf->src_fmt.py_level = PY_SEVEN_LEVEL;
				else
					vf->src_fmt.py_level = PY_SIX_LEVEL;
			} else if (top1_vd_info.width > 256/* && top1_vd_info.height > 144*/ &&
				!bypass_pr) {
				vf->src_fmt.py_level = PY_SIX_LEVEL;
			} else {
				vf->src_fmt.py_level = PY_NO_LEVEL;
			}

			/*make sure not odd res for top1*/
			if (((top1_vd_info.width >> top1_scale) % 2) ||
				((top1_vd_info.height >> top1_scale) % 2)) {
				vf->src_fmt.py_level = PY_NO_LEVEL;
				enable_top1 = false;
				if (debug_dolby & 0x80000)
					pr_dv_dbg("top1 size %dx%d not align, bypass top1\n",
					top1_vd_info.width, top1_vd_info.height);
			} else if ((w <= h || ori_w <= ori_h) && w > 1920) {
				/*hw bug, need disable top1 when w>1920 && w<=h*/
				vf->src_fmt.py_level = PY_NO_LEVEL;
				enable_top1 = false;
			}
			/*make sure not odd res for top2*/
			if (ori_h % 2 || ori_w % 2) {
				vf->src_fmt.py_level = PY_NO_LEVEL;
				enable_top1 = false;
				if (debug_dolby & 0x80000)
					pr_dv_dbg("top2 size %dx%d odd, bypass pd\n",
					ori_w, ori_h);
			}
		} else {
			vf->src_fmt.py_level = PY_NO_LEVEL;
			fake_top1_on = false;
		}
		if (force_level != PY_LEVEL_INVALID) {
			pr_dv_dbg("force py level %d\n", force_level);
			vf->src_fmt.py_level = force_level;
		}
		top1_info.py_level = vf->src_fmt.py_level;

		if (debug_dolby & 0x80000)
			pr_dv_dbg("update enable_top1 %d %d,level %s,last %s,wxh: %dx%d %dx%d,ds %d\n",
				cfg_enable_top1, enable_top1,
				level_str[vf->src_fmt.py_level],
				level_str[last_py_level],
				w, h, ori_w, ori_h, num_downsamplers);

		if (cfg_enable_top1) {
			if (!enable_top1 ||
				(last_py_level != vf->src_fmt.py_level &&
				vf->src_fmt.py_level == PY_NO_LEVEL)) {
				if (debug_dolby & 0x80000)
					pr_dv_dbg("top1 py_level changed off %s->%s\n",
					level_str[last_py_level],
					level_str[vf->src_fmt.py_level]);
			} else if (enable_top1 && last_py_level != vf->src_fmt.py_level &&
				last_py_level == PY_NO_LEVEL) {
				if (debug_dolby & 0x80000)
					pr_dv_dbg("top1 py_level changed on %s->%s\n",
					level_str[last_py_level],
					level_str[vf->src_fmt.py_level]);
			}
		}
		last_py_level = vf->src_fmt.py_level;
	}
}

/*val=0: top1 disabled*/
/*val=1: top1 enabled but no need get one more frame in advance*/
/*val=3: top1 enabled and need get one more frame in advance*/
u32 get_top1_onoff(void)
{
	u32 val = 0;

	if (!is_amdv_enable())
		return 0;

	if ((efuse_mode & 2) || device_disable_pd)
		return 0;

	/*todo for precision disabled but L1L4 enable*/

	if (is_amdv_on() && is_aml_hw5() && enable_top1) {
		val = 3;
		if (cfg_info[cur_pic_mode].bypass_pd_from_user)
			val = 1;/*remove bit1*/
		if (!py_enabled && l1l4_enabled)
			val = 1;/*no PD,remove bit1*/
		if ((dolby_vision_flags & FLAG_CERTIFICATION))
			val = 3;
	}
	if (load_fixed_setting && fix_data && !is_amdv_dpss_path())
		val = 0;
	if (load_fixed_setting && fix_data && is_amdv_dpss_path() && force_top1_enable == 3)
		val = 1;

	if (debug_dolby & 0x80000)
		pr_dv_dbg("get top1_onoff %d\n", val);
	return val;
}
EXPORT_SYMBOL(get_top1_onoff);

/*ret bit0: top1 start*/
/*ret bit1: top1 done*/
/*ret bit2: top2 start*/
/*ret bit3: top2 done*/
u32 get_hw5_status(struct vframe_s *vf)
{
	u32 val = 0;
	u8 i;

	if (is_amdv_on() && (is_aml_t6w() || is_aml_t6x())) {
		for (i = 0; i < TOP_BUF_CNT; i++) {
			if (top_info.vf_info[i].vf == vf) {
				val |= top_info.vf_info[i].top1_start;
				val |= top_info.vf_info[i].top1_done << 1;
				val |= top_info.vf_info[i].top2_start << 2;
				val |= top_info.vf_info[i].top2_done << 3;
				break;
			}
		}
	}
	return val;
}
EXPORT_SYMBOL(get_hw5_status);

static bool prepare_parser(int reset_flag, struct video_inst_s *v_inst_info)
{
	bool parser_ready = false;

	if (!v_inst_info->metadata_parser) {
		if (p_funcs_tv && p_funcs_tv->multi_mp_init) {
			v_inst_info->metadata_parser =
			p_funcs_tv->multi_mp_init
				(dolby_vision_flags
				 & FLAG_CHANGE_SEQ_HEAD
				 ? 1 : 0);
			p_funcs_tv->multi_mp_reset(v_inst_info->metadata_parser, 1);
		}
		if (v_inst_info->metadata_parser) {
			parser_ready = true;
			if (debug_dolby & 1)
				pr_dv_dbg("metadata parser init OK\n");
		}
	} else {
		//} else if (p_funcs_tv && p_funcs_tv->multi_mp_reset) {
		//	if (p_funcs_tv->multi_mp_reset
		//	    (v_inst_info->metadata_parser,
		//		reset_flag | metadata_parser_reset_flag) == 0)
		//		metadata_parser_reset_flag = 0;*/ //no need
		parser_ready = true;
	}
	return parser_ready;
}

void update_src_format_hw5(enum signal_format_enum src_format, struct vframe_s *vf)
{
	enum signal_format_enum cur_format = top2_v_info.amdv_src_format;

	if (src_format == FORMAT_DOVI ||
		src_format == FORMAT_DOVI_LL) {
		top2_v_info.amdv_src_format = 3;
	} else {
		if (vf) {
			if (is_cuva_frame(vf)) {
				if ((signal_transfer_characteristic == 14 ||
				     signal_transfer_characteristic == 18) &&
				    signal_color_primaries == 9)
					top2_v_info.amdv_src_format = 9;
				else if (signal_transfer_characteristic == 16)
					top2_v_info.amdv_src_format = 8;
			} else if (is_primesl_frame(vf)) {
				/* need check prime_sl before hdr and sdr */
				top2_v_info.amdv_src_format = 4;
			} else if (vf_is_hdr10_plus(vf)) {
				top2_v_info.amdv_src_format = 2;
			} else if (vf_is_hdr10(vf)) {
				top2_v_info.amdv_src_format = 1;
			} else if (vf_is_hlg(vf)) {
				top2_v_info.amdv_src_format = 5;
			} else if (is_mvc_frame(vf)) {
				top2_v_info.amdv_src_format = 7;
			} else {
				top2_v_info.amdv_src_format = 6;
			}
		}
	}
	if (cur_format != top2_v_info.amdv_src_format) {
		update_pwm_control();
		if ((debug_dolby & 1) || (debug_dolby & 0x100))
			pr_dv_dbg
			("update src fmt: %s=>%s,signal_type 0x%x,src fmt %d\n",
			input_str[cur_format],
			input_str[top2_v_info.amdv_src_format],
			vf ? vf->signal_type : 0,
			src_format);
		cur_format = top2_v_info.amdv_src_format;
	}
}

/*id=0=>top1 instance paser, id=1=>top2 instance parser*/
int parse_sei_and_meta_ext_hw5(struct vframe_s *vf,
					 char *aux_buf,
					 int aux_size,
					 int *total_comp_size,
					 int *total_md_size,
					 void *fmt,
					 int *ret_flags,
					 char *md_buf,
					 char *comp_buf,
					 int id)
{
	int i;
	char *p;
	unsigned int size = 0u;
	unsigned int type = 0;
	int md_size = 0;
	int comp_size = 0;
	int ret = 2;
	bool parser_overflow = false;
	int rpu_ret = 0;
	u32 reset_flag = 0;
	unsigned int rpu_size = 0;
	enum signal_format_enum *src_format = (enum signal_format_enum *)fmt;
	static int parse_process_count;
	struct video_inst_s *v_inst_info;

	if (id == 0) /*top1*/
		v_inst_info = &top1_v_info;
	else
		v_inst_info = &top2_v_info;

	if (!aux_buf || aux_size == 0 || !fmt || !md_buf || !comp_buf ||
	    !total_comp_size || !total_md_size || !ret_flags)
		return 1;

	parse_process_count++;
	if (parse_process_count > 1) {
		pr_d_log(1, "not support multi instance\n");
		ret = 1;
		goto parse_err;
	}
	if (!p_funcs_tv) {
		ret = 1;
		goto parse_err;
	}

	/* release metadata_parser when new playing */
	if (vf && vf->src_fmt.play_id != v_inst_info->last_play_id) {
		if (top1_v_info.metadata_parser) {
			//if (p_funcs_tv && p_funcs_tv->multi_mp_release)/*idk5.1*/
			//	p_funcs_tv->multi_mp_release(&top1_v_info.metadata_parser);
			//top1_v_info.metadata_parser = NULL;

			if (p_funcs_tv && p_funcs_tv->multi_mp_reset)/*idk5.1*/
				p_funcs_tv->multi_mp_reset(top1_v_info.metadata_parser, 1);
		}
		if (top2_v_info.metadata_parser) {
			//if (p_funcs_tv && p_funcs_tv->multi_mp_release)/*idk5.1*/
			//	p_funcs_tv->multi_mp_release(&top2_v_info.metadata_parser);
			//top2_v_info.metadata_parser = NULL;

			if (p_funcs_tv && p_funcs_tv->multi_mp_reset)/*idk5.1*/
				p_funcs_tv->multi_mp_reset(top2_v_info.metadata_parser, 1);
			amdv_clear_buf(0);
			if (debug_dolby & 0x100)
				pr_dv_dbg("new play %d,release parser\n", vf->src_fmt.play_id);
		}
		v_inst_info->last_play_id = vf->src_fmt.play_id;
		pr_d_log(2, "update play id=%d:\n", v_inst_info->last_play_id);
	}
	p = aux_buf;
	while (p < aux_buf + aux_size - 8) {
		size = *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		size = (size << 8) | *p++;
		type = *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		type = (type << 8) | *p++;
		if (debug_dolby & 4)
			pr_dv_dbg("metadata type=%08x, size=%d:\n",
				     type, size);
		if (size == 0 || size > aux_size) {
			pr_dv_dbg("invalid aux size %d\n", size);
			ret = 1;
			goto parse_err;
		}
		if (type == DV_SEI || /* hevc t35 sei */
			((type & 0xffff0000) == AV1_SEI && is_av1_amdv(p))) {/*av1 t35 obu*/
			*total_comp_size = 0;
			*total_md_size = 0;

			if ((type & 0xffff0000) == AV1_SEI) {
				/* AV1 dv meta in obu */
				*src_format = FORMAT_DOVI;
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				meta_buf[3] = 0x01;
				meta_buf[4] = 0x19;
				if (p[11] & 0x10) {
					rpu_size = 0x100;
					rpu_size |= (p[11] & 0x0f) << 4;
					rpu_size |= (p[12] >> 4) & 0x0f;
					if (p[12] & 0x08) {
						pr_dv_error
							("rpu in obu exceed 512 bytes\n");
						break;
					}
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[12 + i] & 0x07) << 5;
						meta_buf[5 + i] |=
							(p[13 + i] >> 3) & 0x1f;
					}
					rpu_size += 5;
				} else {
					rpu_size = (p[10] & 0x1f) << 3;
					rpu_size |= (p[11] >> 5) & 0x07;
					for (i = 0; i < rpu_size; i++) {
						meta_buf[5 + i] =
							(p[11 + i] & 0x0f) << 4;
						meta_buf[5 + i] |=
							(p[12 + i] >> 4) & 0x0f;
					}
					rpu_size += 5;
				}
			} else {
				/* HEVC dv meta in sei */
				*src_format = FORMAT_DOVI;
				if (size > (sizeof(meta_buf) - 3))
					size = (sizeof(meta_buf) - 3);
				meta_buf[0] = 0;
				meta_buf[1] = 0;
				meta_buf[2] = 0;
				memcpy(&meta_buf[3], p + 1, size - 1);
				rpu_size = size + 2;
			}
			if ((debug_dolby & 4) && dump_enable_f(0)) {
				pr_dv_dbg("metadata(%d):\n", rpu_size);
				for (i = 0; i < size; i += 16)
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7],
						meta_buf[i + 8],
						meta_buf[i + 9],
						meta_buf[i + 10],
						meta_buf[i + 11],
						meta_buf[i + 12],
						meta_buf[i + 13],
						meta_buf[i + 14],
						meta_buf[i + 15]);
			}

			/* prepare metadata parser */
			if (!prepare_parser(reset_flag, v_inst_info)) {
				if (vf)
					pr_dv_error
					("meta(%d), pts(%lld)->init fail\n",
						rpu_size, vf->pts_us64);
				ret = 1;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			if (p_funcs_tv && p_funcs_tv->multi_mp_process)
				rpu_ret =
				p_funcs_tv->multi_mp_process
				(v_inst_info->metadata_parser, meta_buf, rpu_size,
				 comp_buf + *total_comp_size,
				 &comp_size, md_buf + *total_md_size,
				 &md_size, true, DV_TYPE_DOVI);
			if (rpu_ret < 0) {
				if (vf)
					pr_dv_error
					("meta(%d), pts(%lld)->process fail\n",
					 rpu_size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			if (parser_overflow) {
				ret = 2;
				break;
			}
			/*dv type just appears once in metadata
			 *after parsing dv type,breaking the
			 *circulation directly
			 */
			break;
		}
		p += size;
	}

	/*continue to check atsc/dvb dv */
	if (*src_format != FORMAT_DOVI) {
		struct dv_vui_parameters vui_param = {0};
		static u32 len_2086_sei;
		u32 len_2094_sei = 0;
		static u8 payload_2086_sei[MAX_LEN_2086_SEI];
		u8 payload_2094_sei[MAX_LEN_2094_SEI];
		unsigned char nal_type;
		unsigned char sei_payload_type = 0;
		unsigned char sei_payload_size = 0;

		if (vf) {
			vui_param.video_fmt_i = (vf->signal_type >> 26) & 7;
			vui_param.video_fullrange_b = (vf->signal_type >> 25) & 1;
			vui_param.color_description_b = (vf->signal_type >> 24) & 1;
			vui_param.color_primaries_i = (vf->signal_type >> 16) & 0xff;
			vui_param.trans_characteristic_i =
							(vf->signal_type >> 8) & 0xff;
			vui_param.matrix_coeff_i = (vf->signal_type) & 0xff;
			if (debug_dolby & 2)
				pr_dv_dbg("vui_param %d,%d,%d,%d,%d,%d\n",
					vui_param.video_fmt_i,
					vui_param.video_fullrange_b,
					vui_param.color_description_b,
					vui_param.color_primaries_i,
					vui_param.trans_characteristic_i,
					vui_param.matrix_coeff_i);
		}
		p = aux_buf;
		if ((debug_dolby & 0x200) && dump_enable_f(0)) {
			pr_dv_dbg("aux_buf(%d):\n", aux_size);
			for (i = 0; i < aux_size; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					p[i],
					p[i + 1],
					p[i + 2],
					p[i + 3],
					p[i + 4],
					p[i + 5],
					p[i + 6],
					p[i + 7]);
		}
		while (p < aux_buf + aux_size - 8) {
			size = *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			size = (size << 8) | *p++;
			type = *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			type = (type << 8) | *p++;
			if (debug_dolby & 2)
				pr_dv_dbg("type: 0x%x\n", type);

			/*4 byte size + 4 byte type*/
			/*1 byte nal_type + 1 byte (layer_id+temporal_id)*/
			/*1 byte payload type + 1 byte size + payload data*/
			if (type == 0x02000000) {
				nal_type = ((*p) & 0x7E) >> 1; /*nal unit type*/
				if (debug_dolby & 2)
					pr_dv_dbg("nal_type: %d\n",
						     nal_type);

				if (nal_type == PREFIX_SEI_NUT_NAL ||
					nal_type == SUFFIX_SEI_NUT_NAL) {
					sei_payload_type = *(p + 2);
					sei_payload_size = *(p + 3);
					if (debug_dolby & 2)
						pr_dv_dbg("type %d, size %d\n",
							     sei_payload_type,
							     sei_payload_size);
					if (sei_payload_type ==
					SEI_TYPE_MASTERING_DISP_COLOUR_VOLUME) {
						len_2086_sei =
							sei_payload_size;
						memcpy(payload_2086_sei, p + 4,
						       len_2086_sei);
					} else if (sei_payload_type == SEI_ITU_T_T35 &&
						sei_payload_size >= 8 && check_atsc_dvb(p)) {
						len_2094_sei = sei_payload_size;
						memcpy(payload_2094_sei, p + 4,
						       len_2094_sei);
					}
					if (len_2086_sei > 0 &&
					    len_2094_sei > 0)
						break;
				}
			}
			p += size;
		}
		if (len_2094_sei > 0) {
			/* source is VS10 */
			*total_comp_size = 0;
			*total_md_size = 0;
			*src_format = FORMAT_DOVI;
			p_atsc_md.vui_param = vui_param;
			p_atsc_md.len_2086_sei = len_2086_sei;
			memcpy(p_atsc_md.sei_2086, payload_2086_sei,
			       len_2086_sei);
			p_atsc_md.len_2094_sei = len_2094_sei;
			memcpy(p_atsc_md.sei_2094, payload_2094_sei,
			       len_2094_sei);
			size = sizeof(struct dv_atsc);
			if (size > sizeof(meta_buf))
				size = sizeof(meta_buf);
			memcpy(meta_buf, (unsigned char *)(&p_atsc_md), size);
			if ((debug_dolby & 4) && dump_enable_f(0)) {
				pr_dv_dbg("metadata(%d):\n", size);
				for (i = 0; i < size; i += 8)
					pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
						meta_buf[i],
						meta_buf[i + 1],
						meta_buf[i + 2],
						meta_buf[i + 3],
						meta_buf[i + 4],
						meta_buf[i + 5],
						meta_buf[i + 6],
						meta_buf[i + 7]);
			}
			/* prepare metadata parser */
			reset_flag = 2; /*flag: bit0 flag, bit1 0->dv, 1->atsc*/
			if (!prepare_parser(reset_flag, v_inst_info)) {
				if (vf)
					pr_dv_error
					("meta(%d), pts(%lld)->init fail\n",
					 size, vf->pts_us64);
				ret = 1;
				goto parse_err;
			}

			md_size = 0;
			comp_size = 0;

			if (p_funcs_tv && p_funcs_tv->multi_mp_process)
				rpu_ret =
				p_funcs_tv->multi_mp_process
				(v_inst_info->metadata_parser, meta_buf, size,
				comp_buf + *total_comp_size,
				&comp_size, md_buf + *total_md_size,
				&md_size, true, DV_TYPE_ATSC);

			if (rpu_ret < 0) {
				if (vf)
					pr_dv_error
					("meta(%d), pts(%lld)->process fail\n",
					size, vf->pts_us64);
				ret = 3;
			} else {
				if (*total_comp_size + comp_size
					< COMP_BUF_SIZE)
					*total_comp_size += comp_size;
				else
					parser_overflow = true;

				if (*total_md_size + md_size
					< MD_BUF_SIZE)
					*total_md_size += md_size;
				else
					parser_overflow = true;
				if (rpu_ret == 1)
					*ret_flags = 1;
				ret = 0;
			}
			if (parser_overflow)
				ret = 2;
		} else {
			len_2086_sei = 2;
		}
	}

	if (*total_md_size) {
		if ((debug_dolby & 1) && vf)
			pr_dv_dbg
			("meta(%d), pts(%lld) -> md(%d), comp(%d)\n",
			 size, vf->pts_us64,
			 *total_md_size, *total_comp_size);
		if ((debug_dolby & 4) && dump_enable_f(0))  {
			pr_dv_dbg("parsed md(%d):\n", *total_md_size);
			for (i = 0; i < *total_md_size + 7; i += 8) {
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					md_buf[i],
					md_buf[i + 1],
					md_buf[i + 2],
					md_buf[i + 3],
					md_buf[i + 4],
					md_buf[i + 5],
					md_buf[i + 6],
					md_buf[i + 7]);
			}
		}
	}
parse_err:
	parse_process_count--;
	return ret;
}

int parse_sei_and_meta_hw5(struct vframe_s *vf,
				      void *frame_req,
				      int *total_comp_size,
				      int *total_md_size,
				      enum signal_format_enum *src_format,
				      int *ret_flags, u32 inst_id)
{
	int ret = 1;
	char *p_md_buf;
	char *p_comp_buf;
	int next_id;
	struct provider_aux_req_s *req = (struct provider_aux_req_s *)frame_req;
	struct video_inst_s *v_inst_info;

	if (inst_id == 0) /*top1*/
		v_inst_info = &top1_v_info;
	else
		v_inst_info = &top2_v_info;

	if (!req->aux_buf || req->aux_size == 0 || !is_aml_hw5())
		return 1;

	next_id = v_inst_info->current_id ^ 1;
	p_md_buf = v_inst_info->md_buf[next_id];
	p_comp_buf = v_inst_info->comp_buf[next_id];

	ret = parse_sei_and_meta_ext_hw5(vf,
				     req->aux_buf,
				     req->aux_size,
				     total_comp_size,
				     total_md_size,
				     src_format,
				     ret_flags,
				     p_md_buf,
				     p_comp_buf,
				     inst_id);

	if (ret == 0) {
		v_inst_info->current_id = next_id;
		v_inst_info->last_total_comp_size = *total_comp_size;
		v_inst_info->last_total_md_size = *total_md_size;
	} else { /*parser err, use backup md and comp*/
		*total_comp_size = v_inst_info->last_total_comp_size;
		*total_md_size = v_inst_info->last_total_md_size;
	}

	return ret;
}

void set_vf_crc_valid_top1(int val)
{
	last_vf_valid_crc_top1 = val;
}

/*for hdmi cert: not run cp when vf not change*/
/*sometimes  even if frame is same but  frame crc changed, maybe ucd or rx not stable,*/
/*so need to check  few more frames*/
bool check_vf_changed_top1(struct vframe_s *vf)
{
//#define MAX_VF_CRC_CHECK_COUNT_TOP1 0

	static u32 new_vf_crc;
	static u32 vf_crc_repeat_cnt;
	bool changed = false;

	if (!vf)
		return true;

	pr_d_log(1, "top1 vf %px, crc %x, last valid crc %x, rpt %d\n",
				vf, vf->crc, last_vf_valid_crc_top1,
				vf_crc_repeat_cnt);

	//vf->src_fmt.hdmi_new_frame = false;

	if (vf->crc == 0) {
		/*invalid crc, maybe vdin dropped last frame*/
		return changed;
	}
	if (last_vf_valid_crc_top1 == 0) {
		last_vf_valid_crc_top1 = vf->crc;
		changed = true;
		pr_d_log(1, "top1 first new frame\n");
	} else if (vf->crc != last_vf_valid_crc_top1) {
		if (vf->crc != new_vf_crc)
			vf_crc_repeat_cnt = 0;
		else
			++vf_crc_repeat_cnt;

		//if (vf_crc_repeat_cnt >= MAX_VF_CRC_CHECK_COUNT_TOP1) {
		vf_crc_repeat_cnt = 0;
		last_vf_valid_crc_top1 = vf->crc;
		changed = true;
		pr_d_log(1, "top1 new frame\n");
		//}
	} else {
		vf_crc_repeat_cnt = 0;
	}
	new_vf_crc = vf->crc;
	return changed;
}

/*for top1 parser + controlpath*/
int amdv_parse_metadata_hw5_top1(struct vframe_s *vf)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	int flag = 0;
	enum signal_format_enum src_format = FORMAT_SDR;
	int total_md_size = 0;
	int total_comp_size = 0;
	int meta_flag_bl = 1;
	int src_chroma_format = 0;
	int src_bdp = 12;
	bool video_frame = false;
	int i;
	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	int ret_flags = 0;
	int ret = -1;
	bool mel_flag = false;
	int vsem_size = 0;
	int vsem_if_size = 0;
	bool dump_emp = false;
	bool dv_vsem = false;
	bool hdr10_src_primary_changed = false;
	unsigned long time_use = 0;
	struct timeval start;
	struct timeval end;
	char *pic_mode;
	bool run_control_path = true;
	bool vf_changed = true;
	struct dynamic_cfg_s *p_ambient = NULL;
	u32 cur_id = 0;
	struct video_inst_s *v_inst_info = &top1_v_info;
	u32 new_w;
	u32 new_h;

	if (!p_funcs_tv || !p_funcs_tv->tv_hw5_control_path_analyzer ||
		!tv_hw5_setting || delay_update_path)
		return -1;

	if (vf) {
		update_top1_onoff(vf);
		vf_tmp = vf;
	}

	if (!enable_top1)
		return -1;

	memset(&req, 0, (sizeof(struct provider_aux_req_s)));
	memset(&el_req, 0, (sizeof(struct provider_aux_req_s)));

	if (vf)
		video_frame = true;

	if (is_aml_tvmode() && vf &&
	    vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		if (vf->src_fmt.sei_magic_code != HDMI_SEI_MAGIC_CODE) {
			req.vf = vf;
			req.bot_flag = 0;
			req.aux_buf = NULL;
			req.aux_size = 0;
			req.dv_enhance_exist = 0;
			req.low_latency = 0;
			req.is_dv_unique_drm = 0;
			vf_notify_provider_by_name("dv_vdin",
						VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
						(void *)&req);
		} else {
			req.vf = vf;
			req.bot_flag = vf->src_fmt.bot_flag;
			req.aux_buf = vf->src_fmt.md_buf;
			req.aux_size = vf->src_fmt.md_size;
			req.low_latency = vf->src_fmt.low_latency;
			req.is_dv_unique_drm = vf->src_fmt.is_dv_unique_drm;
		}
		input_mode = IN_MODE_HDMI;

		if ((dolby_vision_flags & FLAG_CERTIFICATION) && enable_vf_check)
			vf_changed = check_vf_changed_top1(vf);

		/* meta */
		if ((dolby_vision_flags & FLAG_RX_EMP_VSEM) &&
			vf->emp.size > 0) {
			vsem_size = vf->emp.size * VSEM_PKT_SIZE;
			memcpy(vsem_if_buf, vf->emp.addr, vsem_size);
			if (vsem_if_buf[0] == 0x7f &&
			    vsem_if_buf[10] == 0x46 &&
			    vsem_if_buf[11] == 0xd0) {
				dv_vsem = true;
				if (!vsem_check(vsem_if_buf, vsem_md_buf)) {
					vsem_if_size = vsem_size;
					if (!vsem_md_buf[10]) {
						req.aux_buf =
							&vsem_md_buf[13];
						req.aux_size =
							(vsem_md_buf[5] << 8)
							+ vsem_md_buf[6]
							- 6 - 4;
						/* cancel vsem, use md */
						/* vsem_if_size = 0; */
					} else {
						req.low_latency = 1;
					}
				} else {
					/* emp error, use previous md */
					pr_dv_dbg("EMP err %d\n", vf->emp.size);
					dump_emp = true;
					vsem_if_size = 0;
					req.aux_buf = NULL;
					req.aux_size = v_inst_info->last_total_md_size;
				}
			} else if (debug_dolby & 4) {
				/*EMP packet not DV vsem*/
				dump_emp = true;
			}
			if ((debug_dolby & 4) || dump_emp) {
				pr_info("vsem pkt size %d\n", vf->emp.size);
				for (i = 0; i < vsem_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],	vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],	vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],	vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],	vsem_if_buf[i + 7]);
				}
			}
		}
		dv_unique_drm = is_dv_unique_drm(vf);

		/* w/t vsif and no dv_vsem */
		if (vf->vsif.size && !dv_vsem) {
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			memcpy(vsem_if_buf, vf->vsif.addr, vf->vsif.size);
			vsem_if_size = vf->vsif.size;
			if (debug_dolby & 4) {
				pr_info("vsif size %d\n", vf->vsif.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],	vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],	vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],	vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],	vsem_if_buf[i + 7]);
				}
			}
		}  else if (dv_unique_drm && !dv_vsem) { /* dv unique drm and no dv_vsem */
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			if (force_hdmin_fmt >= 3 && force_hdmin_fmt <= 9) {
				memcpy(vsem_if_buf, force_drm, 32);
				vsem_if_size = 32;
			} else if (vf->drm_if.size > 0 && vf->drm_if.addr) {
				memcpy(vsem_if_buf, vf->drm_if.addr, vf->drm_if.size);
				vsem_if_size = vf->drm_if.size;
			}
			if (debug_dolby & 4) {
				pr_info("drm size %d\n", vf->drm_if.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}
		if ((debug_dolby & 1) && (dv_vsem || vsem_if_size))
			pr_dv_dbg("vdin get %s:%d,md:%p %d,ll:%d,unique:%d,bit %x,type %x\n",
				dv_vsem ? "vsem" : dv_unique_drm ? "drm" : "vsif",
				dv_vsem ? vsem_size : vsem_if_size,
				req.aux_buf, req.aux_size,
				req.low_latency,
				req.is_dv_unique_drm,
				vf->bitdepth, vf->source_type);

		/*check vsem_if_buf */
		if (!dv_unique_drm && vsem_if_size &&
			vsem_if_buf[0] != 0x81) {
			/*not vsif, continue to check vsem*/
			if (!(vsem_if_buf[0] == 0x7F &&
				vsem_if_buf[1] == 0x80 &&
				vsem_if_buf[10] == 0x46 &&
				vsem_if_buf[11] == 0xd0 &&
				vsem_if_buf[12] == 0x00)) {
				vsem_if_size = 0;
				pr_dv_dbg("invalid buf %x %x %x %x %x %x %x %x %x %x %x %x\n",
					vsem_if_buf[0], vsem_if_buf[1], vsem_if_buf[2],
					vsem_if_buf[3],	vsem_if_buf[4],	vsem_if_buf[5],
					vsem_if_buf[6],	vsem_if_buf[7],	vsem_if_buf[8],
					vsem_if_buf[9],	vsem_if_buf[10], vsem_if_buf[11]);
			}
		}
		src_chroma_format = 2;/*fixed CF_P444 for hdmi dw data*/;
		if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
		    req.low_latency == 1) {
			src_format = FORMAT_DOVI_LL;
			for (i = 0; i < 2; i++) {
				if (v_inst_info->md_buf[i])
					memset(v_inst_info->md_buf[i], 0, MD_BUF_SIZE);
				if (v_inst_info->comp_buf[i])
					memset(v_inst_info->comp_buf[i], 0, COMP_BUF_SIZE);
			}
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else if (req.aux_size) {
			if (req.aux_buf) {
				v_inst_info->current_id = v_inst_info->current_id ^ 1;
				memcpy(v_inst_info->md_buf[v_inst_info->current_id],
				       req.aux_buf, req.aux_size);
			}
			src_format = FORMAT_DOVI;
		} else if (dv_unique_drm) {
			src_format = FORMAT_DOVI_LL;
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else {
			src_format =  tv_hw5_setting->top1.src_format;
			p_mdc =	&vf->prop.master_display_colour;
			if (is_hdr10_frame(vf) || force_hdmin_fmt == 1) {
				src_format = FORMAT_HDR10;
				/* prepare parameter from hdmi for hdr10 */
				prepare_hdr10_param(p_mdc, &v_inst_info->hdr10_param, input_mode);
				req.dv_enhance_exist = 0;
				src_bdp = 10;
			}
			if (src_format != FORMAT_DOVI &&
				(is_hlg_frame(vf) || force_hdmin_fmt == 2)) {
				src_format = FORMAT_HLG;
				src_bdp = 10;
			}
			if (src_format == FORMAT_SDR && force_sdr10 == 1)
				src_format = FORMAT_SDR10;

			if (src_format == FORMAT_SDR10)
				src_bdp = 10;
		}
		if ((debug_dolby & 4) && req.aux_size) {
			pr_dv_dbg("metadata(%d):\n", req.aux_size);
			for (i = 0; i < req.aux_size + 8; i += 8)
				pr_info("\t%02x %02x %02x %02x %02x %02x %02x %02x\n",
					v_inst_info->md_buf[v_inst_info->current_id][i],
					v_inst_info->md_buf[v_inst_info->current_id][i + 1],
					v_inst_info->md_buf[v_inst_info->current_id][i + 2],
					v_inst_info->md_buf[v_inst_info->current_id][i + 3],
					v_inst_info->md_buf[v_inst_info->current_id][i + 4],
					v_inst_info->md_buf[v_inst_info->current_id][i + 5],
					v_inst_info->md_buf[v_inst_info->current_id][i + 6],
					v_inst_info->md_buf[v_inst_info->current_id][i + 7]);
		}
		total_md_size = req.aux_size;
		total_comp_size = 0;
		meta_flag_bl = 0;
		if (req.aux_buf && req.aux_size) {
			v_inst_info->last_total_md_size = total_md_size;
			v_inst_info->last_total_comp_size = total_comp_size;
		}
		if (debug_dolby & 1)
			pr_dv_dbg("top1 frame %d,%p,pts %lld,fmt: %s,type %x\n",
			v_inst_info->frame_count, vf, vf->pts_us64,
			(src_format == FORMAT_HDR10) ? "HDR10" :
			((src_format == FORMAT_DOVI) ? "DOVI" :
			((src_format == FORMAT_DOVI_LL) ? "DOVI_LL" :
			((src_format == FORMAT_HLG) ? "HLG" : "SDR"))),
			vf->type);

	} else if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		enum vframe_signal_fmt_e fmt;

		input_mode = IN_MODE_OTT;
		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;

		/* check source format */
		fmt = get_vframe_src_fmt(vf);
		if ((fmt == VFRAME_SIGNAL_FMT_DOVI ||
		    fmt == VFRAME_SIGNAL_FMT_INVALID ||
		    vf->src_fmt.sei_magic_code != SEI_MAGIC_CODE) &&
		    !vf->discard_dv_data) {
			vf_notify_provider_by_name
				(dv_provider[0],
				 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				  (void *)&req);
		}
		/* use callback aux date first, if invalid, use sei_ptr */
		if ((!req.aux_buf || !req.aux_size) &&
		    fmt == VFRAME_SIGNAL_FMT_DOVI) {
			u32 sei_size = 0;
			char *sei;

			sei = (char *)get_sei_from_src_fmt(vf, &sei_size);
			if (sei && sei_size) {
				req.aux_buf = sei;
				req.aux_size = sei_size;
				req.dv_enhance_exist =
					vf->src_fmt.dual_layer;
			}
		}
		if (debug_dolby & 1)
			pr_dv_dbg("top1:%s get vf %px %p(%d,index %d),fmt %d,aux %p %d,el %d,type %x\n",
				     dv_provider[0], vf, vf, vf->discard_dv_data,
				     vf->frame_index, fmt, req.aux_buf, req.aux_size,
				     req.dv_enhance_exist, vf->type);
		/* parse meta in base layer */
		ret = get_md_from_src_fmt(vf);
		if (ret == 1) { /*parse succeeded*/
			meta_flag_bl = 0;
			src_format = FORMAT_DOVI;
			v_inst_info->current_id = v_inst_info->current_id ^ 1;
			cur_id = v_inst_info->current_id;
			memcpy(v_inst_info->md_buf[cur_id],
			       vf->src_fmt.md_buf,
			       vf->src_fmt.md_size);
			memcpy(v_inst_info->comp_buf[cur_id],
			       vf->src_fmt.comp_buf,
			       vf->src_fmt.comp_size);
			total_md_size =  vf->src_fmt.md_size;
			total_comp_size =  vf->src_fmt.comp_size;
			ret_flags = vf->src_fmt.parse_ret_flags;

			v_inst_info->last_total_md_size = total_md_size;
			v_inst_info->last_total_comp_size = total_comp_size;

			if ((debug_dolby & 4) && dump_enable_f(0)) {
				pr_dv_dbg("get md_buf %p, size(%d):\n",
					vf->src_fmt.md_buf, vf->src_fmt.md_size);
				for (i = 0; i < total_md_size; i += 8)
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						v_inst_info->md_buf[cur_id][i],
						v_inst_info->md_buf[cur_id][i + 1],
						v_inst_info->md_buf[cur_id][i + 2],
						v_inst_info->md_buf[cur_id][i + 3],
						v_inst_info->md_buf[cur_id][i + 4],
						v_inst_info->md_buf[cur_id][i + 5],
						v_inst_info->md_buf[cur_id][i + 6],
						v_inst_info->md_buf[cur_id][i + 7]);
			}
		} else {  /*no parse in advance or parse failed*/
			if (get_vframe_src_fmt(vf) ==
			    VFRAME_SIGNAL_FMT_HDR10PRIME)
				src_format = FORMAT_PRIMESL;
			else
				meta_flag_bl =
				parse_sei_and_meta_hw5
					(vf, &req,
					 &total_comp_size,
					 &total_md_size,
					 &src_format,
					 &ret_flags, 0);
		}
		if (ret_flags && req.dv_enhance_exist) {
			if (!strcmp(dv_provider[0], "dvbldec"))
				vf_notify_provider_by_name
				(dv_provider[0],
				 VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL,
				 (void *)&req);
			amdv_el_disable = 1;
			pr_dv_dbg("bypass mel\n");
		}
		if (ret_flags == 1)
			mel_flag = true;
		if (is_aml_tvmode() &&
			(req.dv_enhance_exist && !mel_flag)) {
			src_format = FORMAT_SDR;
			/* dovi_setting.src_format = src_format; */
			total_comp_size = 0;
			total_md_size = 0;
			src_bdp = 10;
		}

		if (src_format != FORMAT_DOVI && is_primesl_frame(vf)) {
			src_format = FORMAT_PRIMESL;
			src_bdp = 10;
		}
		if (src_format != FORMAT_DOVI && is_hdr10_frame(vf)) {
			src_format = FORMAT_HDR10;
			/* prepare parameter from SEI for hdr10 */
			p_mdc =	&vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &v_inst_info->hdr10_param, input_mode);
			/* for 962x with v1.4 or stb with v2.3 may use 12 bit */
			src_bdp = 10;
			req.dv_enhance_exist = 0;
		}

		if (src_format != FORMAT_DOVI && is_hlg_frame(vf)) {
			src_format = FORMAT_HLG;
			src_bdp = 10;
		}
		if (src_format != FORMAT_DOVI && is_hdr10plus_frame(vf))
			src_format = FORMAT_HDR10PLUS;

		if (src_format != FORMAT_DOVI && is_mvc_frame(vf))
			src_format = FORMAT_MVC;

		if (src_format != FORMAT_DOVI && is_cuva_frame(vf))
			src_format = FORMAT_CUVA;

		if (src_format == FORMAT_SDR && force_sdr10 == 1)
			src_format = FORMAT_SDR10;

		if (src_format == FORMAT_SDR10 && !req.dv_enhance_exist)
			src_bdp = 10;

		if (src_format == FORMAT_SDR &&	!req.dv_enhance_exist)
			src_bdp = 8;

		if (debug_dolby & 1)
			pr_info
			("top1:[%d,%lld,%d,%s,%d,%d]\n",
			 v_inst_info->frame_count, vf->pts_us64, src_bdp,
			 (src_format == FORMAT_HDR10) ? "HDR10" :
			 (src_format == FORMAT_DOVI ? "DOVI" :
			 (src_format == FORMAT_HLG ? "HLG" :
			 (src_format == FORMAT_HDR10PLUS ? "HDR10+" :
			 (src_format == FORMAT_CUVA ? "CUVA" :
			 (src_format == FORMAT_PRIMESL ? "PRIMESL" :
			 (req.dv_enhance_exist ? "DOVI (el meta)" : "SDR")))))),
			 req.aux_size, req.dv_enhance_exist);
		if (src_format != FORMAT_DOVI && !req.dv_enhance_exist)
			memset(&req, 0, sizeof(req));

		if (meta_flag_bl) {
			total_md_size = v_inst_info->last_total_md_size;
			total_comp_size = v_inst_info->last_total_comp_size;
			mel_flag = v_inst_info->last_mel_mode;
			meta_flag_bl = 0;
		}

		if (src_format != FORMAT_DOVI)
			mel_flag = 0;
	}
	if (src_format == FORMAT_DOVI && meta_flag_bl) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		pr_dv_dbg("no meta or meta err!\n");
		return -1;
	}

	current_mode = dolby_vision_mode;
	if (amdv_policy_process
		(vf, &current_mode, src_format)) {
		amdv_set_toggle_flag(1);
		if (debug_dolby & 1)
			pr_dv_dbg("top1 parse_metadata:output chg %d=>%d(%px, %d)\n",
			     dolby_vision_mode, current_mode,
			     vf, src_format);
		amdv_target_mode = current_mode;
		dolby_vision_mode = current_mode;
	}
	if (vf && (debug_dolby & 8))
		pr_dv_dbg("top1 parse_metadata:vf %px(index %d), mode %d\n",
			      vf, vf->frame_index, dolby_vision_mode);

	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
		if (amdv_target_mode == AMDV_OUTPUT_MODE_BYPASS)
			amdv_wait_on = false;
		if (debug_dolby & 8)
			pr_dv_dbg("now bypass mode, target %d, wait %d\n",
				  amdv_target_mode,
				  amdv_wait_on);
		if (get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_BYPASS)
			return 1;
		return -1;
	}

	if (src_format != tv_hw5_setting->top1.src_format ||
		(dolby_vision_flags & FLAG_CERTIFICATION)) {
		pq_config_set_flag = false;
		best_pq_config_set_flag = false;
	}
	if (!pq_config_set_flag) {
		if (debug_dolby & 2)
			pr_dv_dbg("update tgt cfg\n");
		if (!get_load_config_status()) {
			memcpy(&(((struct pq_config_dvp *)pq_config_dvp_fake_top1)->tdc),
			       &def_tgt_dvp_cfg,
			       sizeof(def_tgt_dvp_cfg));//todo
		}
		pq_config_set_flag = true;
	}
	if (force_best_pq && !best_pq_config_set_flag) {
		//memcpy(&(((struct pq_config_dvp *)
		//	pq_config_dvp_fake)->tdc),
		//	&def_tgt_display_cfg_bestpq,
		//	sizeof(def_tgt_display_cfg_bestpq));//todo
		best_pq_config_set_flag = true;

		p_funcs_tv->tv_hw5_control_path_analyzer(invalid_hw5_setting);
	}
	calculate_panel_max_pq(src_format, vinfo,
			       &(((struct pq_config_dvp *)
			       pq_config_dvp_fake_top1)->tdc));

	((struct pq_config_dvp *)
		pq_config_dvp_fake_top1)->tdc.tuning_mode =
		amdv_tuning_mode;
	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER) {
		((struct pq_config_dvp *)pq_config_dvp_fake_top1)
			->tdc.tuning_mode |=
			TUNING_MODE_EL_FORCE_DISABLE;
	} else {
		((struct pq_config_dvp *)pq_config_dvp_fake_top1)
			->tdc.tuning_mode &=
			(~TUNING_MODE_EL_FORCE_DISABLE);
	}
	if ((dolby_vision_flags & FLAG_CERTIFICATION) && sdr_ref_mode) {
		((struct pq_config_dvp *)
		pq_config_dvp_fake_top1)->tdc.ambient_config.ambient =
		0;
		((struct pq_config_dvp *)pq_config_dvp_fake_top1)
			->tdc.ref_mode_dark_id = 0;
	}
	if (is_hdr10_src_primary_changed())
		hdr10_src_primary_changed = true;

	new_w = (top1_vd_info.width << num_downsamplers) >> top1_scale;
	new_h = (top1_vd_info.height << num_downsamplers) >> top1_scale;
	if (force_top1_vskip)
		new_h = new_h >> 1;
	if (src_format != tv_hw5_setting->top1.src_format ||
		tv_hw5_setting->top1.video_width != new_w ||
		tv_hw5_setting->top1.video_height != new_h ||
		hdr10_src_primary_changed) {
		if (debug_dolby & 0x100)
			pr_dv_dbg("reset cp_analyze fmt %d->%d,w %d->%d,h %d->%d,hdr_c %d\n",
				tv_hw5_setting->top1.src_format, src_format,
				tv_hw5_setting->top1.video_width, new_w,
				tv_hw5_setting->top1.video_height, new_h,
				hdr10_src_primary_changed);
		/*for hdmi in cert*/
		if (dolby_vision_flags & FLAG_CERTIFICATION)
			vf_changed = true;
		p_funcs_tv->tv_hw5_control_path_analyzer(invalid_hw5_setting);
	}
	pic_mode = get_cur_pic_mode_name();
	if (!(dolby_vision_flags & FLAG_CERTIFICATION) && pic_mode &&
	    (strstr(pic_mode, "dark") ||
	    strstr(pic_mode, "Dark") ||
	    strstr(pic_mode, "DARK"))) {
		memcpy(tv_input_info,
		       brightness_off,
		       sizeof(brightness_off));
		/*for HDR10/HLG, only has DM4, ko only use value from tv_input_info[3][1]*/
		/*and tv_input_info[4][1]. To avoid ko code changed, we reuse these*/
		/*parameter for both HDMI and OTT mode, that means need copy HDR10 to */
		/*tv_input_info[3][1] and copy HLG to tv_input_info[4][1] for HDMI mode*/
		if (input_mode == IN_MODE_HDMI) {
			tv_input_info->brightness_off[3][1] = brightness_off[3][0];
			tv_input_info->brightness_off[4][1] = brightness_off[4][0];
		}
	} else {
		memset(tv_input_info, 0, sizeof(brightness_off));
	}
	/*config source fps and gd_rf_adjust, dmcfg_id*/
	tv_input_info->content_fps = 24 * (1 << 16);
	tv_input_info->gd_rf_adjust = gd_rf_adjust;
	tv_input_info->tid = get_pic_mode();
	if (debug_dolby & 0x400)
		do_gettimeofday(&start);
	/*for hdmi in cert, only run control_path for different frame*/
	if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
	    !vf_changed && input_mode == IN_MODE_HDMI) {
		run_control_path = false;
	}

	if (ambient_update) {
		/*only if cfg enables darkdetail we allow the API to set values*/
		if (((struct pq_config_dvp *)pq_config_dvp_fake_top1)->
			tdc.ambient_config.dark_detail) {
			dynamic_config_new.dark_detail =
			cfg_info[cur_pic_mode].dark_detail;
		}
		p_ambient = &dynamic_config_new;
	} else {
		if (((struct pq_config_dvp *)pq_config_dvp_fake_top1)->
			tdc.ambient_config.dark_detail) {
			/*only if cfg enables darkdetail we allow the API to set*/
			dynamic_darkdetail.dark_detail =
				cfg_info[cur_pic_mode].dark_detail;
			p_ambient = &dynamic_darkdetail;
		}
	}
	if ((dolby_vision_flags & FLAG_CERTIFICATION)) {
		if (ambient_test_mode == 1 &&
		    v_inst_info->frame_count < AMBIENT_CFG_FRAMES) {
			p_ambient = &dynamic_test_cfg[v_inst_info->frame_count];
		} else if (ambient_test_mode == 2 &&
			   v_inst_info->frame_count < AMBIENT_CFG_FRAMES) {
			p_ambient = &dynamic_test_cfg_2[v_inst_info->frame_count];
		} else if (ambient_test_mode == 3 &&
			   hdmi_frame_count < AMBIENT_CFG_FRAMES) {
			p_ambient = &dynamic_test_cfg_3[hdmi_frame_count];
		} else if (ambient_test_mode == 4 &&
			   v_inst_info->frame_count < AMBIENT_CFG_FRAMES_2) {
			p_ambient = &dynamic_test_cfg_4[v_inst_info->frame_count];
		}
	}

	if (debug_dolby & 0x200)
		pr_dv_dbg("[count %d %d]dark_detail from cfg:%d,from api:%d\n",
			     hdmi_frame_count, v_inst_info->frame_count,
			     ((struct pq_config_dvp *)pq_config_dvp_fake_top1)->
			     tdc.ambient_config.dark_detail,
			     cfg_info[cur_pic_mode].dark_detail);

	v_inst_info->src_format = src_format;
	v_inst_info->input_mode = input_mode;
	v_inst_info->video_width = new_w;
	v_inst_info->video_height = new_h;

	tv_hw5_setting->top1.src_format = src_format;
	tv_hw5_setting->top1.video_width = new_w;
	tv_hw5_setting->top1.video_height = new_h;

	tv_hw5_setting->top1.input_mode = input_mode;
	tv_hw5_setting->top1.in_md = v_inst_info->md_buf[v_inst_info->current_id];
	tv_hw5_setting->top1.in_md_size = (src_format == FORMAT_DOVI) ? total_md_size : 0;
	tv_hw5_setting->top1.in_comp = v_inst_info->comp_buf[v_inst_info->current_id];
	tv_hw5_setting->top1.in_comp_size = (src_format == FORMAT_DOVI) ? total_comp_size : 0;
	tv_hw5_setting->top1.set_bit_depth = src_bdp;
	tv_hw5_setting->top1.set_chroma_format = src_chroma_format;
	tv_hw5_setting->top1.set_yuv_range = SIGNAL_RANGE_SMPTE;
	tv_hw5_setting->top1.color_format = (vf && (vf->type & VIDTYPE_RGB_444)) ? CP_RGB : CP_YUV;
	tv_hw5_setting->top1.vsem_if = vsem_if_buf;
	tv_hw5_setting->top1.vsem_if_size = vsem_if_size;
	tv_hw5_setting->hdr10_param = &v_inst_info->hdr10_param;
	tv_hw5_setting->pq_config = (struct pq_config_dvp *)pq_config_dvp_fake_top1;
	tv_hw5_setting->menu_param = &menu_param;
	tv_hw5_setting->dynamic_cfg = p_ambient;
	tv_hw5_setting->input_info = tv_input_info;
	tv_hw5_setting->enable_debug = debug_ko;
	tv_hw5_setting->dither_bdp = 0;//dither bitdepth,0=>no dither
	tv_hw5_setting->L1L4_distance = l1l4_distance;
	tv_hw5_setting->num_ext_downsamplers = num_downsamplers;//todo
	tv_hw5_setting->frame_rate = content_fps;//24000
	if (vf) {
		vf->src_fmt.py_id = py_id++;
		if (is_aml_t3x()) {
			py_id = py_id % TOP_BUF_CNT_VD1;/*% PYRAMID_BUF_CNT*/
		} else if (is_aml_t6w() || is_aml_t6x()) {
			if (is_amdv_dpss_path())
				py_id = py_id % TOP_BUF_CNT;
			else
				py_id = py_id % TOP_BUF_CNT_VD1;
		}
		vf->src_fmt.pr_done = false;
	}

	if (run_control_path) {
		/*step1: top1 frame N*/
		tv_hw5_setting->analyzer = 1;
		flag = p_funcs_tv->tv_hw5_control_path_analyzer(tv_hw5_setting);

		if (debug_dolby & 0x400) {
			do_gettimeofday(&end);
			time_use = (end.tv_sec - start.tv_sec) * 1000000 +
				(end.tv_usec - start.tv_usec);

			pr_info("cp time: %5ld us\n", time_use);
		}
		if (flag >= 0) {
			top1_info.amdv_setting_video_flag = video_frame;
			v_inst_info->tv_dovi_setting_change_flag = true;

			dump_tv_setting(tv_hw5_setting,
				v_inst_info->frame_count, debug_dolby);
			ret = 0; /* setting updated */
		}
		if (debug_dolby & 1) {
			pr_dv_dbg
			("top1 tv setting %s-%d:flag=%x,md=%d,comp=%d\n",
				 input_mode == IN_MODE_HDMI ?
				 "hdmi" : "ott",
				 src_format,
				 flag,
				 total_md_size,
				 total_comp_size);
		}
	} else { /*for cert: vf no change, not run cp*/
		v_inst_info->tv_dovi_setting_change_flag = true;
		ret = 0;
	}
	(top1_v_info.frame_count)++;
	if (debug_dolby & 0x20000)
		pr_dv_dbg("update top1 frame_count %d,ret %d\n",
		top1_v_info.frame_count, ret);
	return ret;
}

/*for top2 parser + controlpath*/
int amdv_parse_metadata_hw5(struct vframe_s *vf,
					      u8 toggle_mode)
{
	const struct vinfo_s *vinfo = get_current_vinfo();
	struct provider_aux_req_s req;
	struct provider_aux_req_s el_req;
	enum signal_format_enum src_format = FORMAT_SDR;
	enum signal_format_enum check_format;
	int total_md_size = 0;
	int total_comp_size = 0;
	bool el_flag = 0;
	u32 w = 0xffff;
	u32 h = 0xffff;
	int meta_flag_bl = 1;
	int src_chroma_format = 0; /* 0:CF_P420, 1:CF_UYVY, 2:CF_P444, 3:CF_I444*/
	int src_bdp = 12;
	bool video_frame = false;
	int i;
	struct vframe_master_display_colour_s *p_mdc;
	unsigned int current_mode = dolby_vision_mode;
	enum input_mode_enum input_mode = IN_MODE_OTT;
	int ret_flags = 0;
	int ret = -1;
	bool mel_flag = false;
	int vsem_size = 0;
	int vsem_if_size = 0;
	bool dump_emp = false;
	bool dv_vsem = false;
	bool hdr10_src_primary_changed = false;
	char *pic_mode;
	bool run_control_path = true;
	bool vf_changed = true;
	struct dynamic_cfg_s *p_ambient = NULL;
	u32 cur_id = 0;
	struct video_inst_s *v_inst_info = &top2_v_info;
	u32 test_count = 0;
	static u32 last_top2_level = PY_NO_LEVEL;

	if (!p_funcs_tv || !p_funcs_tv->tv_hw5_control_path ||
		!tv_hw5_setting || delay_update_path)
		return -1;

#if ENABLE_DPSS
	/*path switch from dpss to vpp during play, should check if top2 already done for cur vf*/
	if (!is_amdv_dpss_path()) {
		if ((debug_dolby & 0x80000) && vf)
			pr_dv_dbg("parse_metadata:vf[%px](index %d) dpss_flg 0x%x\n",
			vf, vf->frame_index, vf->dpss_flg);
		if (vf && (vf->dpss_flg & DPSS_FLG_DDD))
			return -1;
	}
#endif

	memset(&req, 0, (sizeof(struct provider_aux_req_s)));
	memset(&el_req, 0, (sizeof(struct provider_aux_req_s)));

	if (vf) {
		video_frame = true;
		if (vf->type & VIDTYPE_COMPRESS) {
			if (is_src_crop_valid(vf->src_crop) && !is_amdv_dpss_path()) {
				w = vf->compWidth -
					vf->src_crop.left - vf->src_crop.right;
				h = vf->compHeight -
					vf->src_crop.top - vf->src_crop.bottom;
			} else {
				w = vf->compWidth;
				h = vf->compHeight;
			}
		} else {
			w = vf->width;
			h = vf->height;
		}
		if (is_amdv_dpss_path()) {
			w = prm_dolby.dpss_real_h;
			h = prm_dolby.dpss_real_v;
			if (prm_dolby.vds_4k1k_en)
				h = h >> 1;
		}

		if (enable_top1 || check_cfg_enabled_top1()) {
			if (vf->src_fmt.py_level != last_top2_level) {
				if (debug_dolby & 0x80000)
					pr_dv_dbg("top2 py_level chg %s->%s\n",
					level_str[last_top2_level],
					level_str[vf->src_fmt.py_level]);
			}
			if (vf->src_fmt.py_level == PY_NO_LEVEL)
				force_bypass_pd_level0 = true;
			else
				force_bypass_pd_level0 = false;

			/*cfg pd off->on: top2 cfg update after enable_top1*/
			if (update_top2_cfg) {
				update_cp_cfg_hw5(false, false, false);
				update_top2_cfg = false;
			}
		}
		last_top2_level = vf->src_fmt.py_level;
	}

	calculate_gdbs_delay(vf, w, h);

	if (is_aml_tvmode() && vf &&
	    vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
		if (vf->src_fmt.sei_magic_code != HDMI_SEI_MAGIC_CODE) {
			req.vf = vf;
			req.bot_flag = 0;
			req.aux_buf = NULL;
			req.aux_size = 0;
			req.dv_enhance_exist = 0;
			req.low_latency = 0;
			req.is_dv_unique_drm = 0;
			vf_notify_provider_by_name("dv_vdin",
						VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
						(void *)&req);
		} else {
			req.vf = vf;
			req.bot_flag = vf->src_fmt.bot_flag;
			req.aux_buf = vf->src_fmt.md_buf;
			req.aux_size = vf->src_fmt.md_size;
			req.low_latency = vf->src_fmt.low_latency;
			req.is_dv_unique_drm = vf->src_fmt.is_dv_unique_drm;
		}
		input_mode = IN_MODE_HDMI;

		if ((dolby_vision_flags & FLAG_CERTIFICATION) && enable_vf_check)
			vf_changed = check_vf_changed(vf);

		/* meta */
		if ((dolby_vision_flags & FLAG_RX_EMP_VSEM) &&
			vf->emp.size > 0) {
			vsem_size = vf->emp.size * VSEM_PKT_SIZE;
			memcpy(vsem_if_buf, vf->emp.addr, vsem_size);
			if (vsem_if_buf[0] == 0x7f &&
			    vsem_if_buf[10] == 0x46 &&
			    vsem_if_buf[11] == 0xd0) {
				dv_vsem = true;
				if (!vsem_check(vsem_if_buf, vsem_md_buf)) {
					vsem_if_size = vsem_size;
					if (!vsem_md_buf[10]) {
						req.aux_buf =
							&vsem_md_buf[13];
						req.aux_size =
							(vsem_md_buf[5] << 8)
							+ vsem_md_buf[6]
							- 6 - 4;
						/* cancel vsem, use md */
						/* vsem_if_size = 0; */
					} else {
						req.low_latency = 1;
					}
				} else {
					/* emp error, use previous md */
					dump_emp = true;
					vsem_if_size = 0;
					req.aux_buf = NULL;
					req.aux_size = v_inst_info->last_total_md_size;
				}
			} else if (debug_dolby & 4) {
				dump_emp = true;
			}
			if ((debug_dolby & 4) || dump_emp) {
				pr_info("vsem pkt size %d\n", vf->emp.size);
				for (i = 0; i < vsem_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],	vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],	vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],	vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],	vsem_if_buf[i + 7]);
				}
			}
		}
		dv_unique_drm = is_dv_unique_drm(vf);

		/* w/t vsif and no dv_vsem */
		if (vf->vsif.size && !dv_vsem) {
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			memcpy(vsem_if_buf, vf->vsif.addr, vf->vsif.size);
			vsem_if_size = vf->vsif.size;
			if (debug_dolby & 4) {
				pr_info("vsif size %d\n", vf->vsif.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],	vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],	vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],	vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],	vsem_if_buf[i + 7]);
				}
			}
		} else if (dv_unique_drm && !dv_vsem) { /* dv unique drm and no dv_vsem */
			memset(vsem_if_buf, 0, VSEM_IF_BUF_SIZE);
			if (force_hdmin_fmt >= 3 && force_hdmin_fmt <= 9) {
				memcpy(vsem_if_buf, force_drm, 32);
				vsem_if_size = 32;
			} else if (vf->drm_if.size > 0 && vf->drm_if.addr) {
				memcpy(vsem_if_buf, vf->drm_if.addr, vf->drm_if.size);
				vsem_if_size = vf->drm_if.size;
			}
			if (debug_dolby & 4) {
				pr_info("drm size %d\n", vf->drm_if.size);
				for (i = 0; i < vsem_if_size; i += 8) {
					pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
						vsem_if_buf[i],
						vsem_if_buf[i + 1],
						vsem_if_buf[i + 2],
						vsem_if_buf[i + 3],
						vsem_if_buf[i + 4],
						vsem_if_buf[i + 5],
						vsem_if_buf[i + 6],
						vsem_if_buf[i + 7]);
				}
			}
		}
		if ((debug_dolby & 1) && (dv_vsem || vsem_if_size))
			pr_dv_dbg("vdin get %s:%d,md:%p %d,ll:%d,unique:%d,bit %x,type %x %x\n",
				dv_vsem ? "vsem" : dv_unique_drm ? "drm" : "vsif",
				dv_vsem ? vsem_size : vsem_if_size,
				req.aux_buf, req.aux_size,
				req.low_latency,
				req.is_dv_unique_drm,
				vf->bitdepth, vf->source_type, vf->type);

		/*check vsem_if_buf */
		if (!dv_unique_drm && vsem_if_size &&
			vsem_if_buf[0] != 0x81) {
			/*not vsif, continue to check vsem*/
			if (!(vsem_if_buf[0] == 0x7F &&
				vsem_if_buf[1] == 0x80 &&
				vsem_if_buf[10] == 0x46 &&
				vsem_if_buf[11] == 0xd0 &&
				vsem_if_buf[12] == 0x00)) {
				vsem_if_size = 0;
				pr_dv_dbg("invalid buf %x %x %x %x %x %x %x %x %x %x %x %x\n",
					vsem_if_buf[0], vsem_if_buf[1], vsem_if_buf[2],
					vsem_if_buf[3],	vsem_if_buf[4],	vsem_if_buf[5],
					vsem_if_buf[6],	vsem_if_buf[7],	vsem_if_buf[8],
					vsem_if_buf[9],	vsem_if_buf[10], vsem_if_buf[11]);
			}
		}
		if ((dolby_vision_flags & FLAG_FORCE_DV_LL) ||
		    req.low_latency == 1) {
			src_format = FORMAT_DOVI_LL;
			src_chroma_format = 1;
			for (i = 0; i < 2; i++) {
				if (v_inst_info->md_buf[i])
					memset(v_inst_info->md_buf[i], 0, MD_BUF_SIZE);
				if (v_inst_info->comp_buf[i])
					memset(v_inst_info->comp_buf[i], 0, COMP_BUF_SIZE);
			}
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else if (req.aux_size) {
			if (req.aux_buf) {
				v_inst_info->current_id = v_inst_info->current_id ^ 1;
				memcpy(v_inst_info->md_buf[v_inst_info->current_id],
				       req.aux_buf, req.aux_size);
			}
			src_format = FORMAT_DOVI;
		} else if (dv_unique_drm) {
			if ((vf->type & VIDTYPE_VIU_444) ||
				(vf->type & VIDTYPE_RGB_444))
				src_chroma_format = 3;
			else if (vf->type & VIDTYPE_VIU_422)
				src_chroma_format = 1;
			else
				src_chroma_format = 0;
			if (vf->bitdepth & BITDEPTH_Y12)
				src_bdp = 12;
			else if (vf->bitdepth & BITDEPTH_Y10)
				src_bdp = 10;
			else
				src_bdp = 8;
			src_format = FORMAT_DOVI_LL;
			req.aux_size = 0;
			req.aux_buf = NULL;
		} else {
			if (toggle_mode == 2)
				src_format =  tv_hw5_setting->top2.src_format;
			if (vf->type & VIDTYPE_VIU_422)
				src_chroma_format = 1;
			else if (vf->type & VIDTYPE_VIU_444)
				src_chroma_format = 3;
			else
				src_chroma_format = 0;
			p_mdc =	&vf->prop.master_display_colour;
			if (is_hdr10_frame(vf) || force_hdmin_fmt == 1) {
				src_format = FORMAT_HDR10;
				/* prepare parameter from hdmi for hdr10 */
				prepare_hdr10_param(p_mdc, &v_inst_info->hdr10_param, input_mode);
				req.dv_enhance_exist = 0;
				src_bdp = 10;
			}
			if (src_format != FORMAT_DOVI &&
				(is_hlg_frame(vf) || force_hdmin_fmt == 2)) {
				src_format = FORMAT_HLG;
				src_bdp = 10;
			}
			if (src_format == FORMAT_SDR && force_sdr10 == 1)
				src_format = FORMAT_SDR10;

			if (src_format == FORMAT_SDR10)
				src_bdp = 10;
		}
		if ((debug_dolby & 4) && req.aux_size) {
			pr_dv_dbg("metadata(%d):\n", req.aux_size);
			for (i = 0; i < req.aux_size + 8; i += 8)
				pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
					v_inst_info->md_buf[v_inst_info->current_id][i],
					v_inst_info->md_buf[v_inst_info->current_id][i + 1],
					v_inst_info->md_buf[v_inst_info->current_id][i + 2],
					v_inst_info->md_buf[v_inst_info->current_id][i + 3],
					v_inst_info->md_buf[v_inst_info->current_id][i + 4],
					v_inst_info->md_buf[v_inst_info->current_id][i + 5],
					v_inst_info->md_buf[v_inst_info->current_id][i + 6],
					v_inst_info->md_buf[v_inst_info->current_id][i + 7]);
		}
		total_md_size = req.aux_size;
		total_comp_size = 0;
		meta_flag_bl = 0;
		if (req.aux_buf && req.aux_size) {
			v_inst_info->last_total_md_size = total_md_size;
			v_inst_info->last_total_comp_size = total_comp_size;
		} else if (toggle_mode == 2) {
			total_md_size = v_inst_info->last_total_md_size;
			total_comp_size = v_inst_info->last_total_comp_size;
		}
		if (debug_dolby & 1)
			pr_dv_dbg("top2 frame %d, %px,pts %lld,fmt:%s,type %x\n",
			v_inst_info->frame_count, vf, vf->pts_us64,
			(src_format == FORMAT_HDR10) ? "HDR10" :
			((src_format == FORMAT_DOVI) ? "DOVI" :
			((src_format == FORMAT_DOVI_LL) ? "DOVI_LL" :
			((src_format == FORMAT_HLG) ? "HLG" : "SDR"))),
			vf->type);

		if (toggle_mode == 1)
			amdvdolby_vision_vf_add(vf, NULL);
	} else if (vf && (vf->source_type == VFRAME_SOURCE_TYPE_OTHERS)) {
		enum vframe_signal_fmt_e fmt;

		input_mode = IN_MODE_OTT;

		req.vf = vf;
		req.bot_flag = 0;
		req.aux_buf = NULL;
		req.aux_size = 0;
		req.dv_enhance_exist = 0;

		/* check source format */
		fmt = get_vframe_src_fmt(vf);
		if ((fmt == VFRAME_SIGNAL_FMT_DOVI ||
		    fmt == VFRAME_SIGNAL_FMT_INVALID ||
		    vf->src_fmt.sei_magic_code != SEI_MAGIC_CODE) &&
		    !vf->discard_dv_data) {
			vf_notify_provider_by_name
				(dv_provider[0],
				 VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				  (void *)&req);
		}
		/* use callback aux date first, if invalid, use sei_ptr */
		if ((!req.aux_buf || !req.aux_size) &&
		    fmt == VFRAME_SIGNAL_FMT_DOVI) {
			u32 sei_size = 0;
			char *sei;

			sei = (char *)get_sei_from_src_fmt(vf, &sei_size);
			if (sei && sei_size) {
				req.aux_buf = sei;
				req.aux_size = sei_size;
				req.dv_enhance_exist =
					vf->src_fmt.dual_layer;
			}
		}
		if (debug_dolby & 1)
			pr_dv_dbg("top2:%s get %px %p(%d,index %d),fmt %d,aux %px %d,el %d,type %x\n",
				     dv_provider[0], vf, vf, vf->discard_dv_data,
				     vf->frame_index, fmt, req.aux_buf, req.aux_size,
				     req.dv_enhance_exist, vf->type);
		/* parse meta in base layer */
		if (toggle_mode != 2) {
			ret = get_md_from_src_fmt(vf);
			if (ret == 1) { /*parse succeeded*/
				meta_flag_bl = 0;
				src_format = FORMAT_DOVI;
				v_inst_info->current_id = v_inst_info->current_id ^ 1;
				cur_id = v_inst_info->current_id;
				memcpy(v_inst_info->md_buf[cur_id],
				       vf->src_fmt.md_buf,
				       vf->src_fmt.md_size);
				memcpy(v_inst_info->comp_buf[cur_id],
				       vf->src_fmt.comp_buf,
				       vf->src_fmt.comp_size);
				total_md_size =  vf->src_fmt.md_size;
				total_comp_size =  vf->src_fmt.comp_size;
				ret_flags = vf->src_fmt.parse_ret_flags;

				v_inst_info->last_total_md_size = total_md_size;
				v_inst_info->last_total_comp_size = total_comp_size;

				if ((debug_dolby & 4) && dump_enable_f(0)) {
					pr_dv_dbg("top2:get md_buf %px, size(%d):\n",
						vf->src_fmt.md_buf, vf->src_fmt.md_size);
					for (i = 0; i < total_md_size; i += 8)
						pr_info("%02x %02x %02x %02x %02x %02x %02x %02x\n",
							v_inst_info->md_buf[cur_id][i],
							v_inst_info->md_buf[cur_id][i + 1],
							v_inst_info->md_buf[cur_id][i + 2],
							v_inst_info->md_buf[cur_id][i + 3],
							v_inst_info->md_buf[cur_id][i + 4],
							v_inst_info->md_buf[cur_id][i + 5],
							v_inst_info->md_buf[cur_id][i + 6],
							v_inst_info->md_buf[cur_id][i + 7]);
				}
			} else {  /*no parse in advance or parse failed*/
				if (get_vframe_src_fmt(vf) ==
				    VFRAME_SIGNAL_FMT_HDR10PRIME)
					src_format = FORMAT_PRIMESL;
				else if (load_fixed_setting)
					src_format = FORMAT_SDR;
				else
					meta_flag_bl =
					parse_sei_and_meta_hw5
						(vf, &req,
						 &total_comp_size,
						 &total_md_size,
						 &src_format,
						 &ret_flags, 1);
			}
			if (force_mel)
				ret_flags = 1;

			if (ret_flags && req.dv_enhance_exist) {
				if (!strcmp(dv_provider[0], "dvbldec"))
					vf_notify_provider_by_name
					(dv_provider[0],
					 VFRAME_EVENT_RECEIVER_DOLBY_BYPASS_EL,
					 (void *)&req);
				amdv_el_disable = 1;
				pr_dv_dbg("bypass mel\n");
			}
			if (ret_flags == 1)
				mel_flag = true;
			if (is_aml_tvmode() &&
				(req.dv_enhance_exist && !mel_flag)) {
				src_format = FORMAT_SDR;
				/* dovi_setting.src_format = src_format; */
				total_comp_size = 0;
				total_md_size = 0;
				src_bdp = 10;
				if (debug_dolby & 1)
					pr_dv_dbg("tv: bypass fel\n");
			}
		} else {
			src_format = tv_hw5_setting->top2.src_format;
		}

		if (src_format != FORMAT_DOVI && is_primesl_frame(vf)) {
			src_format = FORMAT_PRIMESL;
			src_bdp = 10;
		}
		if (src_format != FORMAT_DOVI && is_hdr10_frame(vf)) {
			src_format = FORMAT_HDR10;
			/* prepare parameter from SEI for hdr10 */
			p_mdc =	&vf->prop.master_display_colour;
			prepare_hdr10_param(p_mdc, &v_inst_info->hdr10_param, input_mode);
			/* for 962x with v1.4 or stb with v2.3 may use 12 bit */
			src_bdp = 10;
			req.dv_enhance_exist = 0;
		}

		if (src_format != FORMAT_DOVI && is_hlg_frame(vf)) {
			src_format = FORMAT_HLG;
			src_bdp = 10;
		}
		if (src_format != FORMAT_DOVI && is_hdr10plus_frame(vf))
			src_format = FORMAT_HDR10PLUS;

		if (src_format != FORMAT_DOVI && is_mvc_frame(vf))
			src_format = FORMAT_MVC;

		if (src_format != FORMAT_DOVI && is_cuva_frame(vf))
			src_format = FORMAT_CUVA;

		if (src_format == FORMAT_SDR && force_sdr10 == 1)
			src_format = FORMAT_SDR10;

		if (src_format == FORMAT_SDR10 && !req.dv_enhance_exist)
			src_bdp = 10;

		if (src_format == FORMAT_SDR &&	!req.dv_enhance_exist)
			src_bdp = 8;

		if (((debug_dolby & 1) || ((debug_dolby & 0x100) &&
			v_inst_info->frame_count == 0)) && toggle_mode == 1)
			pr_info
			("top2:[%d,%lld,%d,%s,%d,%d]\n",
			 v_inst_info->frame_count, vf->pts_us64, src_bdp,
			 (src_format == FORMAT_HDR10) ? "HDR10" :
			 (src_format == FORMAT_DOVI ? "DOVI" :
			 (src_format == FORMAT_HLG ? "HLG" :
			 (src_format == FORMAT_HDR10PLUS ? "HDR10+" :
			 (src_format == FORMAT_CUVA ? "CUVA" :
			 (src_format == FORMAT_PRIMESL ? "PRIMESL" :
			 (req.dv_enhance_exist ? "DOVI (el meta)" : "SDR")))))),
			 req.aux_size, req.dv_enhance_exist);
		if (src_format != FORMAT_DOVI && !req.dv_enhance_exist)
			memset(&req, 0, sizeof(req));

		if (toggle_mode == 1)
			amdvdolby_vision_vf_add(vf, NULL);

		if (req.dv_enhance_exist)
			el_flag = 1;

		if (toggle_mode != 2) {
			v_inst_info->last_mel_mode = mel_flag;
		} else if (meta_flag_bl) {
			total_md_size = v_inst_info->last_total_md_size;
			total_comp_size = v_inst_info->last_total_comp_size;
			el_flag = tv_hw5_setting->top2.el_flag;
			mel_flag = v_inst_info->last_mel_mode;
			if (debug_dolby & 2)
				pr_dv_dbg("update el_flag %d, melFlag %d\n",
					     el_flag, mel_flag);
			meta_flag_bl = 0;
		}

		if (el_flag && !mel_flag &&
		    ((dolby_vision_flags & FLAG_CERTIFICATION) == 0)) {
			el_flag = 0;
			amdv_el_disable = 1;
		}
		if (src_format != FORMAT_DOVI) {
			el_flag = 0;
			mel_flag = 0;
		}
	}
	if (src_format == FORMAT_DOVI && meta_flag_bl) {
		/* dovi frame no meta or meta error */
		/* use old setting for this frame   */
		pr_dv_dbg("no meta or meta err!\n");
		return -1;
	}

	check_format = src_format;
	if (vf)
		update_src_format(check_format, vf);

	if (dolby_vision_request_mode != 0xff) {
		dolby_vision_mode = dolby_vision_request_mode;
		dolby_vision_request_mode = 0xff;
	}
	current_mode = dolby_vision_mode;
	if (amdv_policy_process
		(vf, &current_mode, check_format)) {
		if (!v_inst_info->amdv_wait_init)
			amdv_set_toggle_flag(1);
		if (debug_dolby & 1)
			pr_dv_dbg("parse_metadata:output chg %d=>%d(%d,%px,%d)\n",
			     dolby_vision_mode, current_mode,
			     toggle_mode, vf, src_format);
		amdv_target_mode = current_mode;
		dolby_vision_mode = current_mode;
		amdv_wait_on = true;
	} else {
		/*not clear target mode when:*/
		/*no mode change && no vf && target is not bypass */
		if (!vf && amdv_target_mode != dolby_vision_mode &&
		    amdv_target_mode != AMDV_OUTPUT_MODE_BYPASS) {
			/*not update target mode*/
		} else if (amdv_target_mode != dolby_vision_mode) {
			amdv_target_mode = dolby_vision_mode;
			if (debug_dolby & 8)
				pr_dv_dbg("update target mode %d\n",
					     amdv_target_mode);
		}
	}

	if (vf && (debug_dolby & 8))
		pr_dv_dbg("parse_metadata: vf %px(index %d), mode %d\n",
			      vf, vf->frame_index, dolby_vision_mode);

	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
		if (amdv_target_mode == AMDV_OUTPUT_MODE_BYPASS)
			amdv_wait_on = false;
		if (debug_dolby & 8)
			pr_dv_dbg("now bypass mode,tgt %d,wait %d\n",
				  amdv_target_mode,
				  amdv_wait_on);
		if (get_hdr_module_status(VD1_PATH, VPP_TOP0) == HDR_MODULE_BYPASS)
			return 1;
		return -1;
	}

	if (src_format != tv_hw5_setting->top2.src_format ||
		(dolby_vision_flags & FLAG_CERTIFICATION)) {
		pq_config_set_flag = false;
		best_pq_config_set_flag = false;
	}
	if (!pq_config_set_flag) {
		if (debug_dolby & 2)
			pr_dv_dbg("update tgt cfg\n");
		if (!get_load_config_status()) {
			memcpy(&(((struct pq_config_dvp *)pq_config_dvp_fake)->tdc),
			       &def_tgt_dvp_cfg,
			       sizeof(def_tgt_dvp_cfg));//todo
		}
		pq_config_set_flag = true;
	}
	if (force_best_pq && !best_pq_config_set_flag) {
		//memcpy(&(((struct pq_config_dvp *)
		//	pq_config_dvp_fake)->tdc),
		//	&def_tgt_display_cfg_bestpq,
		//	sizeof(def_tgt_display_cfg_bestpq));//todo
		best_pq_config_set_flag = true;

		p_funcs_tv->tv_hw5_control_path(invalid_hw5_setting);
	}
	calculate_panel_max_pq(src_format, vinfo,
			       &(((struct pq_config_dvp *)
			       pq_config_dvp_fake)->tdc));

	((struct pq_config_dvp *)
		pq_config_dvp_fake)->tdc.tuning_mode =
		amdv_tuning_mode;
	if (dolby_vision_flags & FLAG_DISABLE_COMPOSER) {
		((struct pq_config_dvp *)pq_config_dvp_fake)
			->tdc.tuning_mode |=
			TUNING_MODE_EL_FORCE_DISABLE;
	} else {
		((struct pq_config_dvp *)pq_config_dvp_fake)
			->tdc.tuning_mode &=
			(~TUNING_MODE_EL_FORCE_DISABLE);
	}
	if ((dolby_vision_flags & FLAG_CERTIFICATION) && sdr_ref_mode) {
		((struct pq_config_dvp *)
		pq_config_dvp_fake)->tdc.ambient_config.ambient =
		0;
		((struct pq_config_dvp *)pq_config_dvp_fake)
			->tdc.ref_mode_dark_id = 0;
	}
	if (is_hdr10_src_primary_changed())
		hdr10_src_primary_changed = true;

	if (src_format != tv_hw5_setting->top2.src_format ||
		hdr10_src_primary_changed) {
		if (debug_dolby & 0x100)
			pr_dv_dbg("reset cp fmt %d->%d,hdr_c %d\n",
				tv_hw5_setting->top2.src_format, src_format,
				hdr10_src_primary_changed);
		/*for hdmi in cert*/
		if (dolby_vision_flags & FLAG_CERTIFICATION)
			vf_changed = true;
		p_funcs_tv->tv_hw5_control_path(invalid_hw5_setting);
	}
	pic_mode = get_cur_pic_mode_name();
	if (!(dolby_vision_flags & FLAG_CERTIFICATION) && pic_mode &&
	    (strstr(pic_mode, "dark") ||
	    strstr(pic_mode, "Dark") ||
	    strstr(pic_mode, "DARK"))) {
		memcpy(tv_input_info,
		       brightness_off,
		       sizeof(brightness_off));
		/*for HDR10/HLG, only has DM4, ko only use value from tv_input_info[3][1]*/
		/*and tv_input_info[4][1]. To avoid ko code changed, we reuse these*/
		/*parameter for both HDMI and OTT mode, that means need copy HDR10 to */
		/*tv_input_info[3][1] and copy HLG to tv_input_info[4][1] for HDMI mode*/
		if (input_mode == IN_MODE_HDMI) {
			tv_input_info->brightness_off[3][1] = brightness_off[3][0];
			tv_input_info->brightness_off[4][1] = brightness_off[4][0];
		}
	} else {
		memset(tv_input_info, 0, sizeof(brightness_off));
	}
	/*config source fps and gd_rf_adjust, dmcfg_id*/
	tv_input_info->content_fps = 24 * (1 << 16);
	tv_input_info->gd_rf_adjust = gd_rf_adjust;
	tv_input_info->tid = get_pic_mode();

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		/*for hdmi in cert, only run control_path for different frame*/
		if (!vf_changed && input_mode == IN_MODE_HDMI)
			run_control_path = false;
		else if (toggle_mode != 1)
			run_control_path = false;
	}

	if (lightsense_test_mode == 1 && toggle_mode == 1) {
		if ((top2_info.core_on_cnt % 100) < 50)
			p_ambient = &lightsense_test_cfg_hw5[0];
		else
			p_ambient = &lightsense_test_cfg_hw5[1];
	} else if (ambient_update) {
		/*only if cfg enables darkdetail we allow the API to set values*/
		if (((struct pq_config_dvp *)pq_config_dvp_fake)->
			tdc.ambient_config.dark_detail) {
			dynamic_config_new.dark_detail =
			cfg_info[cur_pic_mode].dark_detail;
		}
		p_ambient = &dynamic_config_new;
	} else if (((struct pq_config_dvp *)pq_config_dvp_fake)->
		tdc.ambient_config.ambient) {/*user set ambient*/
		if (((struct pq_config_dvp *)pq_config_dvp_fake)->
			tdc.ambient_config.dark_detail) {
			dynamic_config_new.update_flag |= 0x10;
			dynamic_config_new.dark_detail =
				cfg_info[cur_pic_mode].dark_detail;
		}
		/*only if cfg enables ambient we allow use light sense feature*/
		/*light sense: update rear and front*/
		p_ambient = &dynamic_config_new;
		update_ambient_lightsense_hw5(p_ambient);
	} else {/*only darkdetail*/
		if (((struct pq_config_dvp *)pq_config_dvp_fake)->
			tdc.ambient_config.dark_detail) {
			/*only if cfg enables darkdetail we allow the API to set*/
			dynamic_darkdetail.dark_detail =
				cfg_info[cur_pic_mode].dark_detail;
			p_ambient = &dynamic_darkdetail;
		}
	}

	if (((struct pq_config_dvp *)pq_config_dvp_fake)->
		tdc.gd_config.global_dimming) {
		/*only if cfg enables global_dimming we allow the API to set*/
		dynamic_gd.global_dimming =
			cfg_info[cur_pic_mode].global_dimming;
		p_ambient = &dynamic_gd;
	}

	if ((dolby_vision_flags & FLAG_CERTIFICATION)) {
		if (vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI)
			test_count = hdmi_frame_count;
		else if (vf && vf->source_type == VFRAME_SOURCE_TYPE_OTHERS &&
			v_inst_info)
			test_count = v_inst_info->frame_count;
		if (ambient_test_mode == 1 && toggle_mode == 1 &&
			test_count < AMBIENT_CFG_FRAMES) {
			p_ambient = &dynamic_test_cfg[test_count];
		} else if (ambient_test_mode == 2 && toggle_mode == 1 &&
			   test_count < AMBIENT_CFG_FRAMES) {
			p_ambient = &dynamic_test_cfg_2[test_count];
		} else if (ambient_test_mode == 3 && toggle_mode == 1 &&
			   test_count < AMBIENT_CFG_FRAMES) {
			p_ambient = &dynamic_test_cfg_3[test_count];
		} else if (ambient_test_mode == 4 && toggle_mode == 1 &&
			   test_count < AMBIENT_CFG_FRAMES_2) {
			p_ambient = &dynamic_test_cfg_4[test_count];
		}
	}

	if (variable_fps_mode == 1 && toggle_mode == 1 &&
		vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
		hdmi_frame_count < VARIABLE_FPS_COUNT) {
		content_fps = variable_fps[hdmi_frame_count];
		pr_d_log(1, "variable_fps %d\n", content_fps);
	} else if ((dolby_vision_flags & FLAG_CERTIFICATION) && (test_dv & FORCE_ONE_SLICE)) {
		content_fps = 60000;
	}
	if (debug_dolby & 0x200)
		pr_dv_dbg("[count %d %d]dark_detail from cfg:%d,from api:%d\n",
			     hdmi_frame_count, v_inst_info->frame_count,
			     ((struct pq_config_dvp *)pq_config_dvp_fake)->
			     tdc.ambient_config.dark_detail,
			     cfg_info[cur_pic_mode].dark_detail);

	if (vf && enable_top1 && !(test_dv & DEBUG_FORCE_NO_L1L4)) {
		if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
			(test_dv & HDMI_ONLY_UPDATE_HIST_FOR_NEW_FRAME) &&
			vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI) {
			if (debug_dolby & 1)
				pr_dv_dbg("hdmi in, hdmi_new_frame %d\n",
				vf->src_fmt.hdmi_new_frame);
			if (vf->src_fmt.hdmi_new_frame)
				get_l1l4_hist();
		} else {
			get_l1l4_hist();
		}
	} else {
		tv_hw5_setting->top1_stats.enable = false;
	}

	if (force_src_chroma_format)
		src_chroma_format = force_src_chroma_format;
	if (force_src_bdp)
		src_bdp = force_src_bdp;

	v_inst_info->src_format = src_format;
	v_inst_info->input_mode = input_mode;
	v_inst_info->video_width = w;
	v_inst_info->video_height = h;

	tv_hw5_setting->top2.src_format = src_format;
	tv_hw5_setting->top2.video_width = w;
	tv_hw5_setting->top2.video_height = h;

	tv_hw5_setting->top2.input_mode = input_mode;
	tv_hw5_setting->top2.in_md = v_inst_info->md_buf[v_inst_info->current_id];
	tv_hw5_setting->top2.in_md_size = (src_format == FORMAT_DOVI) ? total_md_size : 0;
	tv_hw5_setting->top2.in_comp = v_inst_info->comp_buf[v_inst_info->current_id];
	tv_hw5_setting->top2.in_comp_size = (src_format == FORMAT_DOVI) ? total_comp_size : 0;
	tv_hw5_setting->top2.set_bit_depth = src_bdp;
	tv_hw5_setting->top2.set_chroma_format = src_chroma_format;
	tv_hw5_setting->top2.set_yuv_range = SIGNAL_RANGE_SMPTE;
	tv_hw5_setting->top2.color_format = (vf && (vf->type & VIDTYPE_RGB_444)) ? CP_RGB : CP_YUV;
	tv_hw5_setting->top2.vsem_if = vsem_if_buf;
	tv_hw5_setting->top2.vsem_if_size = vsem_if_size;
	tv_hw5_setting->top2.el_flag = el_flag;
	tv_hw5_setting->hdr10_param = &v_inst_info->hdr10_param;
	tv_hw5_setting->pq_config = (struct pq_config_dvp *)pq_config_dvp_fake;
	tv_hw5_setting->menu_param = &menu_param;
	tv_hw5_setting->dynamic_cfg = p_ambient;
	tv_hw5_setting->input_info = tv_input_info;
	tv_hw5_setting->enable_debug = debug_ko;
	tv_hw5_setting->dither_bdp = 0;//dither bitdepth,0=>no dither
	tv_hw5_setting->L1L4_distance = l1l4_distance;
	tv_hw5_setting->num_ext_downsamplers = vf ?
		vf->src_fmt.downsamplers : num_downsamplers;//todo
	tv_hw5_setting->frame_rate = content_fps;

	top2_info.amdv_setting_video_flag = video_frame;
	if (vf && !enable_top1) {
		vf->src_fmt.py_id = py_id++;
		if (is_aml_t3x()) {
			py_id = py_id % TOP_BUF_CNT_VD1;/*% PYRAMID_BUF_CNT*/
		} else if (is_aml_t6w() || is_aml_t6x()) {
			if (is_amdv_dpss_path())
				py_id = py_id % TOP_BUF_CNT;
			else
				py_id = py_id % TOP_BUF_CNT_VD1;
		}
	}
	v_inst_info->tv_dovi_setting_change_flag = true;
	v_inst_info->last_mel_mode = mel_flag;

	if (run_control_path)
		ret = 0;
	else /*for cert: vf no change, not run cp*/
		ret = 2;

	return ret;
}

int amdv_wait_metadata_hw5(struct vframe_s *vf)
{
	int ret = 0;
	int tmp = 0;
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	bool vd1_on = false;
	bool temp_done = false;
	static struct timeval start_wait_time;
	struct timeval cur_time;
	unsigned long wait_us = 0;
	static bool is_waiting;
	static u32 last_hsize;
	static u32 last_vsize;
	u32 w;
	u32 h;
	bool size_changed = false;

	if (load_fixed_setting && fix_data)
		return 0;

	get_vf_size(vf, &w, &h);
	if (w != last_hsize || h != last_vsize) {
		size_changed = true;
		last_hsize = w;
		last_vsize = h;
	}

	/*only for first frame. next frame will update in parser_metadata_hw5_top1*/
	if (!top2_info.core_on || pic_mode_changed || size_changed) {
		update_top1_onoff(vf);
		pic_mode_changed = false;
	}

	if (single_step_enable_v2(0, VD1_PATH)) {
		if (dolby_vision_flags & FLAG_SINGLE_STEP)
			/* wait fake el for "step" */
			return 1;
		if (dolby_vision_flags & FLAG_CERTIFICATION)
			temp_done = true;
		dolby_vision_flags |= FLAG_SINGLE_STEP;
	}

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		bool ott_mode = true;

		if (is_aml_tvmode() && tv_hw5_setting)
			ott_mode = tv_hw5_setting->top2.input_mode !=
				IN_MODE_HDMI;
		if (debug_dolby & 0x1000)
			pr_dv_dbg("setting_cnt %d,crc_cnt %d,flag %x,top1_on %d %d\n",
				setting_update_count, crc_count, dolby_vision_flags,
				top1_info.core_on, top1_done);
		if (setting_update_count > crc_count &&
			!(dolby_vision_flags & FLAG_DISABLE_CRC)) {
			if (ott_mode)
				return 1;
		}
	}

	if (!top2_v_info.amdv_wait_init && !top2_info.core_on) {
		ret = is_amdv_frame(vf);
		if (ret) {
			check_format = FORMAT_DOVI;
			ret = 0;
		} else if (is_dv_unique_drm(vf)) {
			check_format = FORMAT_DOVI_LL;
		} else if (is_primesl_frame(vf)) {
			check_format = FORMAT_PRIMESL;
		} else if (is_hdr10_frame(vf)) {
			check_format = FORMAT_HDR10;
		} else if (is_hlg_frame(vf)) {
			check_format = FORMAT_HLG;
		} else if (is_hdr10plus_frame(vf)) {
			check_format = FORMAT_HDR10PLUS;
		} else if (is_mvc_frame(vf)) {
			check_format = FORMAT_MVC;
		} else if (is_cuva_frame(vf)) {
			check_format = FORMAT_CUVA;
		} else {
			check_format = FORMAT_SDR;
		}
		/*wait the first frame top1 only for cert*/
		if (enable_top1 && !hw5_reg_from_file && wait_first_frame_top1) {
			if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS)
				tmp = amdv_policy_process(vf, &mode, check_format);

			/*wait top1 only when dv will on*/
			if (dolby_vision_mode != AMDV_OUTPUT_MODE_BYPASS ||
				(tmp == 1 && mode != AMDV_OUTPUT_MODE_BYPASS)) {
				if (!top1_info.core_on) {
					if (vf && (debug_dolby & 8))
						pr_d_log(8, "wait top1 and need do top1\n");

					do_gettimeofday(&cur_time);
					is_waiting = true;
					start_wait_time = cur_time;
					return 4;
				}
				if (top1_info.core_on &&
					(!top1_done && !ignore_top1_result &&
					!force_ignore_top1_result) &&
					top1_info.py_level != PY_NO_LEVEL) {
					if (vf && (debug_dolby & 8))
						pr_d_log(8, "wait top1\n");

					do_gettimeofday(&cur_time);

					wait_us = (cur_time.tv_sec - start_wait_time.tv_sec) *
						1000000 +
						(cur_time.tv_usec - start_wait_time.tv_usec);

					if (!is_waiting)
						start_wait_time = cur_time;

					if (wait_us > 2 * 1000 * 1000) {
						pr_d_log(8, "top1 timeout after %5ld us\n",
						wait_us);
					} else {
						is_waiting = true;
						return 5;
					}
				}
				is_waiting = false;
			}
		}

		if (vf)
			update_src_format(check_format, vf);

		if (amdv_policy_process(vf, &mode, check_format)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS) {
				top2_v_info.amdv_wait_init = true;
				amdv_target_mode = mode;
				amdv_wait_on = true;
				if (debug_dolby & 1)
					pr_dv_dbg("amdv wait src=%d mode=%d\n",
						check_format, mode);
			}
		}
		if (is_aml_t3x()) {
			if (READ_VPP_DV_REG(T3X_VD1_BLEND_SRC_CTRL) & (0xf << 0))
				vd1_on = true;
		} else if (is_aml_t6w() || is_aml_t6x()) {
			if ((READ_VPP_REG(VD1_BLEND_SRC_CTRL) >> 8 & 0xf) == 1)/*1dfb for t6w*/
				vd1_on = true;
		}
		/* don't use run mode when sdr -> dv and vd1 not disable */
		if (/*top2_v_info.amdv_wait_init && */vd1_on && !force_runmode)
			top2_info.run_mode_count = amdv_run_mode_delay + 1;
	}

	if (vf && (debug_dolby & 8))
		pr_dv_dbg("wait ret %d,vf %px(index %d),runcount %d,vd1 %d\n",
			      ret, vf, vf->frame_index, top2_info.run_mode_count, vd1_on);

	return ret;
}

int amdv_update_src_format_hw5(struct vframe_s *vf, u8 toggle_mode)
{
	unsigned int mode = dolby_vision_mode;
	enum signal_format_enum check_format;
	int ret = 0;

	if (!is_amdv_enable() || !vf)
		return -1;

	/* src_format is valid, need not re-init */
	if (top2_v_info.amdv_src_format != 0)
		return 0;

	/* vf is not in the dv queue, new frame case */
	if (amdv_vf_check(vf))
		return 0;

	ret = is_amdv_frame(vf);
	if (ret)
		check_format = FORMAT_DOVI;
	else if (is_primesl_frame(vf))
		check_format = FORMAT_PRIMESL;
	else if (is_hdr10_frame(vf))
		check_format = FORMAT_HDR10;
	else if (is_hlg_frame(vf))
		check_format = FORMAT_HLG;
	else if (is_hdr10plus_frame(vf))
		check_format = FORMAT_HDR10PLUS;
	else if (is_mvc_frame(vf))
		check_format = FORMAT_MVC;
	else if (is_cuva_frame(vf))
		check_format = FORMAT_CUVA;
	else
		check_format = FORMAT_SDR;

	if (vf)
		update_src_format(check_format, vf);

	if (!top2_v_info.amdv_wait_init &&
	    !top2_info.core_on &&
	    top2_v_info.amdv_src_format != 0) {
		if (amdv_policy_process
			(vf, &mode, check_format)) {
			if (mode != AMDV_OUTPUT_MODE_BYPASS &&
			    dolby_vision_mode ==
			    AMDV_OUTPUT_MODE_BYPASS) {
				top2_v_info.amdv_wait_init = true;
				amdv_target_mode = mode;
				amdv_wait_on = true;
				pr_dv_dbg
					("src=%d mode=%d\n",
					 check_format, mode);
			}
		}
	}
	pr_dv_dbg
		("update_src_format vf:%p, src=%d\n",
		vf, top2_v_info.amdv_src_format);
	return 1;
}

/*vf: display on vd1*/
/* ret 0: setting generated for this frame */
/* ret -1: do nothing */
int amdv_hw5_control_path(struct vframe_s *vf, struct vd_proc_info_t *vd_proc_info)
{
	int flag;
	u32 w = 0xffff;
	u32 h = 0xffff;
	int ret = -1;
	unsigned long time_use = 0;
	struct timeval start;
	struct timeval end;
	struct video_inst_s *v_inst_info = &top2_v_info;

	if (!is_amdv_enable() || !module_installed || !tv_hw5_setting)
		return -1;

	if (vf) {
		if (vf->type & VIDTYPE_COMPRESS) {
			if (is_src_crop_valid(vf->src_crop) && !is_amdv_dpss_path()) {
				w = vf->compWidth -
					vf->src_crop.left - vf->src_crop.right;
				h = vf->compHeight -
					vf->src_crop.top - vf->src_crop.bottom;
			} else {
				w = vf->compWidth;
				h = vf->compHeight;
			}
		} else {
			w = vf->width;
			h = vf->height;
		}
		if (is_amdv_dpss_path()) {
			w = prm_dolby.dpss_real_h;
			h = prm_dolby.dpss_real_v;
			if (prm_dolby.vds_4k1k_en)
				h = h >> 1;
		}

		if (vd_proc_info) {
			if (debug_dolby & 0x8)
				pr_dv_dbg("cp vf[%px],wxh: %dx%d,vd1 size:%dx%d\n",
					vf, w, h, vd_proc_info->vd1_in_hsize,
					vd_proc_info->vd1_in_vsize);
			/*1.if size not align to 2 or 4(2slice), vpp will align*/
			/*2.if play 4k video with 4k240hz output, vpp will vskip*/
			/*need set real vd1 size for controlpath*/
			if (w != vd_proc_info->vd1_in_hsize || h != vd_proc_info->vd1_in_vsize) {
				w = vd_proc_info->vd1_in_hsize;
				h = vd_proc_info->vd1_in_vsize;
			}
			if (is_aml_t6w() || is_aml_t6x()) {
				/*t6w not support 4k120hz*/
				/*t6x,2ppc mode,dolby internal reg slice-num=1, control path*/
				/*fixed value 1. use wrap reg to control slice size*/
				tv_hw5_setting->force_num_slices = 1;
			} else {
				if (!(dolby_vision_flags & FLAG_CERTIFICATION)) {
					if (vd_proc_info->slice_num == 2)
						tv_hw5_setting->force_num_slices = 2;
					else
						tv_hw5_setting->force_num_slices = 1;
				} else {
					tv_hw5_setting->force_num_slices = 0;
				}
			}
		} else {
			tv_hw5_setting->force_num_slices = 1;
		}

		tv_hw5_setting->top2.video_width = w;
		tv_hw5_setting->top2.video_height = h;
		v_inst_info->video_width = w;
		v_inst_info->video_height = h;

		if (debug_cp_res > 0) {
			tv_hw5_setting->top2.video_width = (debug_cp_res & 0xffff0000) >> 16;
			tv_hw5_setting->top2.video_height = debug_cp_res & 0xffff;
			v_inst_info->video_width = (debug_cp_res & 0xffff0000) >> 16;
			v_inst_info->video_height = debug_cp_res & 0xffff;
		}
	}
	/*top2,no analyzer*/
	tv_hw5_setting->analyzer = 0;
	if ((dolby_vision_flags & FLAG_CERTIFICATION) &&
		vf && vf->source_type == VFRAME_SOURCE_TYPE_HDMI &&
		hdmi_frame_count == 0)
		p_funcs_tv->tv_hw5_control_path(invalid_hw5_setting);

	if (debug_dolby & 0x400)
		do_gettimeofday(&start);

	flag = p_funcs_tv->tv_hw5_control_path(tv_hw5_setting);

	if (apo_value.content_type != tv_hw5_setting->input_info->content_type ||
		apo_value.white_point != tv_hw5_setting->input_info->white_point) {
		apo_value.content_type = tv_hw5_setting->input_info->content_type;
		apo_value.white_point = tv_hw5_setting->input_info->white_point;
		apo_value.L11_byte2 = tv_hw5_setting->input_info->L11_byte2;
		apo_value.L11_byte3 = tv_hw5_setting->input_info->L11_byte3;
		set_amdv_apo_enable(true);
	}

	if (debug_dolby & 0x200)
		pr_dv_dbg("content type=%d %d %d %d\n",
			tv_hw5_setting->input_info->content_type,
			tv_hw5_setting->input_info->white_point,
			tv_hw5_setting->input_info->L11_byte2,
			tv_hw5_setting->input_info->L11_byte3);

	if (debug_dolby & 0x400) {
		do_gettimeofday(&end);
		time_use = (end.tv_sec - start.tv_sec) * 1000000 +
			(end.tv_usec - start.tv_usec);
		pr_info("cp time: %5ld us\n", time_use);
	}
	if (debug_dolby & 1) {
		pr_dv_dbg
		("tv setting %s-%d:flag=%x,md=%d,comp=%d,force num=%d,bl %d\n",
			 tv_hw5_setting->top2.input_mode == IN_MODE_HDMI ?
			 "hdmi" : "ott",
			 tv_hw5_setting->top2.src_format,
			 flag,
			 tv_hw5_setting->top2.in_md_size,
			 tv_hw5_setting->top2.in_comp_size,
			 tv_hw5_setting->force_num_slices,
			 tv_hw5_setting->backlight);
	}
	if (flag < 0) {/**/
		pr_dv_dbg("cp(%d) failed %d,try sdr setting to avoid stuck\n",
		       tv_hw5_setting->top2.src_format, flag);
		tv_hw5_setting->top2.src_format = FORMAT_SDR;
		flag = p_funcs_tv->tv_hw5_control_path(tv_hw5_setting);
	}
	if (flag >= 0) {
		dump_tv_setting(tv_hw5_setting,
			v_inst_info->frame_count, debug_dolby);
		ret = 0; /* setting updated */
		top2_v_info.tv_dovi_setting_change_flag = true;
	} else {
		pr_dv_error("tv_hw5_control_path(%d) failed %d\n",
		       tv_hw5_setting->top2.src_format, flag);
		tv_hw5_setting->top2.video_width = 0;
		tv_hw5_setting->top2.video_height = 0;
	}

	return ret;
}

/*get weight for corep weight reg*/
void get_py_weight(struct prm_dolby_top *prm_dolby, bool update_from_controlpath)
{
	u32 weight0[7];
	u32 weight1[7];
	u64 *pbuf;
	static u32 last_weight[7];
	u64 tmp_weight[7];
	u64 *temp;
	u32 i;
	struct pq_config_dvp *p_config;

	if (!prm_dolby || !enable_top1)
		return;

	for (i = 0; i < 7; i++)
		last_weight[i] = prm_dolby->py_weight[i];

	for (i = 0; i < 6; i++)/*default value*/
		tmp_weight[i] = (1024 << 16) | 256;//weight1<<16|weight0
	tmp_weight[6] = (1024 << 16) | 64;

	if (hw5_reg_from_file && top2_reg_buf) {
		temp = (u64 *)top2_reg_buf;
		pbuf = &temp[273];
	} else {
		if (update_from_controlpath) {/*update from core2 setting*/
			if (tv_hw5_setting) {
				pbuf = (u64 *)(&tv_hw5_setting->top2_reg[273]);
			} else {/*default*/
				pbuf = tmp_weight;
			}
		} else {/*update from cfg config*/
			if (tv_hw5_setting && tv_hw5_setting->pq_config) {
				p_config = tv_hw5_setting->pq_config;
				for (i = 0; i < 7; i++) {/*PYR1_EDGE_WT01 ~ PYR7_EDGE_WT01*/
					weight0[i] =
						p_config->tdc.pr_config.pyramid_weights[i] >> 2;
					weight1[i] = p_config->tdc.pr_config.pyramid_alpha[i];
					tmp_weight[i] = (weight1[i] << 16) | weight0[i];
				}
				pbuf = tmp_weight;
			} else {/*default*/
				pbuf = tmp_weight;
			}
		}
	}
	if (pbuf) {
		for (i = 0; i < 7; i++)/*PYR1_EDGE_WT01 ~ PYR7_EDGE_WT01*/
			prm_dolby->py_weight[i] = pbuf[i] & 0xFFFFFFFF;
	}

	if (debug_dolby & 1)
		if (memcmp(last_weight, prm_dolby->py_weight, 7 * sizeof(u32)) != 0)
			pr_info("weight change %x %x %x %x %x %x %x-%x %x %x %x %x %x %x\n",
				last_weight[0], last_weight[1], last_weight[2], last_weight[3],
				last_weight[4], last_weight[5], last_weight[6],
				prm_dolby->py_weight[0], prm_dolby->py_weight[1],
				prm_dolby->py_weight[2], prm_dolby->py_weight[3],
				prm_dolby->py_weight[4], prm_dolby->py_weight[5],
				prm_dolby->py_weight[6]);
}

static inline bool is_valid_py_id(u32 py_id)
{
	bool valid = true;
	u32 buf_cnt = TOP_BUF_CNT_VD1;

	if (is_aml_t3x()) {
		if (py_id >= PYRAMID_BUF_CNT)
			valid = false;
	} else if (is_aml_t6w() || is_aml_t6x()) {
		if (is_amdv_dpss_path())
			buf_cnt = TOP_BUF_CNT;
		if (py_id >= buf_cnt)
			valid = false;
	}
	return valid;
}

/*only process top1, no policy*/
int amdolby_vision_process_hw5_top1(struct vframe_s *vf_top1,
		u32 display_size)
{
	u32 h_size = (display_size >> 16) & 0xffff;
	u32 v_size = display_size & 0xffff;
	bool reset_flag = false;
	struct vframe_s *vf;

	if (!is_aml_hw5() || !vf_top1)
		return -1;

	if (!module_installed && !hw5_reg_from_file)
		return -1;

	vf = vf_top1;

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		if (vf && (vf->type & VIDTYPE_COMPRESS)) {
			if (is_src_crop_valid(vf->src_crop) && !is_amdv_dpss_path()) {
				h_size = vf->compWidth -
					vf->src_crop.left - vf->src_crop.right;
				v_size = vf->compHeight -
					vf->src_crop.top - vf->src_crop.bottom;
			} else {
				h_size = vf->compWidth;
				v_size = vf->compHeight;
			}
		} else if (vf) {
			h_size = vf->width;
			v_size = vf->height;
		}
	}
	if (prm_dolby.vds_4k1k_en)
		v_size = v_size >> 1;

	if (vf && (debug_dolby & 0x8))
		pr_dv_dbg("top1_proc: vf %px(index %d %d),mode %d,core_on %d %d,%d %d,flag %x\n",
				  vf, vf->frame_index, vf->src_fmt.py_id, dolby_vision_mode,
				  top1_info.core_on, top2_info.core_on,
				  h_size, v_size, dolby_vision_flags);

	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS)
		return 0;
	reset_flag =
		(amdv_reset & 1) &&
		(!top1_info.core_on) &&
		(top1_info.core_on_cnt == 0);

	top1_info.py_id = vf->src_fmt.py_id;/*top1 write*/
	if (!is_valid_py_id(top1_info.py_id)) {
		pr_err("err top1 py_id %d!\n", top1_info.py_id);
		top1_info.py_id = 0;
	}

	if (is_aml_t6w() || is_aml_t6x()) {
		top_info.vf_info[top1_info.py_id].vf = vf;
		top_info.vf_info[top1_info.py_id].top1_start = true;
		top_info.vf_info[top1_info.py_id].top1_done = false;
		top_info.vf_info[top1_info.py_id].top2_start = false;
		top_info.vf_info[top1_info.py_id].top2_done = false;

		cfg_dolby_pre_set(&prm_dolby);
		cfg_dolby_update(&prm_dolby, 0, h_size, v_size, 0, 0, 1, reset_flag);
		get_py_weight(&prm_dolby, false);
		if (tv_hw5_setting)
			cfg_dolby_top
			(&prm_dolby, tv_hw5_setting->top1_reg,
			 tv_hw5_setting->top1b_reg,
			 tv_hw5_setting->top2_reg,
			 top1_info.amdv_setting_video_flag, /* video enable */
			 reset_flag, true, false,
			 true, false);
		else if (hw5_reg_from_file)
			cfg_dolby_top
			(&prm_dolby, NULL, NULL, NULL,
			 top1_info.amdv_setting_video_flag, /* video enable */
			 reset_flag, true, false,
			 true, false);
	} else {
		if (tv_hw5_setting)
			tv_top_set
			(tv_hw5_setting->top1_reg,
			 tv_hw5_setting->top1b_reg,
			 tv_hw5_setting->top2_reg,
			 h_size, v_size,
			 top1_info.amdv_setting_video_flag, /* video enable */
			 false, reset_flag, true, false);
		else if (hw5_reg_from_file)
			tv_top_set
			(NULL, NULL, NULL,
			 h_size, v_size,
			 top1_info.amdv_setting_video_flag, /* video enable */
			 false, reset_flag, true, false);
	}

	enable_amdv(1);
	if (tv_hw5_setting)
		update_amdv_status(tv_hw5_setting->top1.src_format);
	else if (load_fixed_setting)
		update_amdv_status(FORMAT_SDR);
	if (tv_hw5_setting && last_tv_hw5_setting)
		memcpy(last_tv_hw5_setting, tv_hw5_setting,
			sizeof(*tv_hw5_setting));
	vf_top1->src_fmt.pr_done = true;
	return 0;
}

int amdolby_vision_process_hw5(struct vframe_s *vf_top1,
		struct vframe_s *vf_top2, u32 display_size, u8 toggle_mode)
{
	u32 h_size = (display_size >> 16) & 0xffff;
	u32 v_size = display_size & 0xffff;
	bool reset_flag = false;
	bool force_set = false;
	unsigned int mode = dolby_vision_mode;
	static bool video_turn_off = true;
	static bool video_on[VD_PATH_MAX];
	int video_status = 0;
	int policy_changed = 0;
	static u8 last_toggle_mode;
	static struct vframe_s *last_vf;
	struct vframe_s *vf;
	bool pr_done = false;
	u32 h_ori = h_size;
	u32 v_ori = v_size;
	struct vd_proc_info_t *vd_proc_info = NULL;
	unsigned int reg_value = 0;
	bool enable_detunnel = false;
	u32 core2_hsize_for_corep = 0;
	u32 core2_vsize_for_corep = 0;
	u8 tmp;
	bool force_mode_changed = false;
	u32 tmp1;
	u32 tmp2;
	u32 tmp3;
	u32 tmp4;
	u32 tmp5;

	if (!is_aml_tvmode())
		return -1;

	if (!module_installed && !hw5_reg_from_file)
		return -1;

	if (!amdv_check_mem() && (vf_top1 || vf_top2)) {
		pr_dv_dbg("amdv no mem!\n");
		return -1;
	}

	if (is_amdv_dpss_path()) {
		h_size = prm_dolby.dpss_real_h;
		v_size = prm_dolby.dpss_real_v;
		if (prm_dolby.vds_4k1k_en)
			v_size = v_size >> 1;
	}

	vf = vf_top2;
	if (vf_top2 && (debug_dolby & 0x8))
		pr_dv_dbg("proc:vf %px %px(index %d),mode %d,on %d %d,size %d %d,type %x,ext %x,flag %x\n",
			     vf_top1, vf_top2, vf_top2->frame_index, dolby_vision_mode,
			     top1_info.core_on, top2_info.core_on,
			     h_size, v_size, vf_top2->type, vf_top2->type_ext, vf_top2->flag);
	else if (vf_top1 && (debug_dolby & 0x8))
		pr_dv_dbg("proc:vf %px %px(index %d),mode %d,on %d %d,size %d %d,type %x,ext %x,flag %x\n",
			     vf_top1, vf_top2, vf_top1->frame_index, dolby_vision_mode,
			     top1_info.core_on, top2_info.core_on,
			     h_size, v_size, vf_top1->type, vf_top1->type_ext, vf_top1->flag);
	else if ((debug_dolby & 0x20000))
		pr_dv_dbg("proc:mode %d,on %d %d,size %d %d,flags 0x%x\n",
			     dolby_vision_mode,
			     top1_info.core_on, top2_info.core_on,
			     h_size, v_size, dolby_vision_flags);

	if ((debug_dolby & 0x80000) && dolby_vision_on && (is_aml_t6w() || is_aml_t6x())) {
		if (is_aml_t6w()) {
			tmp1 = READ_VPP_DV_REG(T6W_VPU_AXIRD_PATH_CTRL);
			tmp2 = VSYNC_RD_DV_REG(T6W_VPU_AXIRD_PATH_CTRL);
			tmp3 = VSYNC_RD_TOP2_REG(T6W_VPU_AXIRD_PATH_CTRL);
			tmp4 = READ_VPP_DV_REG(T6W_VPU_DOLBY_IPATH_SEL);
			tmp5 = READ_VPP_DV_REG(T6W_VPU_DOLBY_OPATH_SEL);
		} else if (is_aml_t6x()) {
			tmp1 = READ_VPP_DV_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL);
			tmp2 = VSYNC_RD_DV_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL);
			tmp3 = VSYNC_RD_TOP2_REG(T6X_VPU_AXIRD_DOLBY_PATH_CTRL);
			tmp4 = READ_VPP_DV_REG(T6X_VPU_DOLBY_IPATH_SEL);
			tmp5 = READ_VPP_DV_REG(T6X_VPU_DOLBY_OPATH_SEL);
		}
		pr_dv_dbg("delay %d,axipath 0x%x 0x%x 0x%x,path %x,%x,work mode %d %d %d\n",
				delay_update_path,
				tmp1, tmp2, tmp3, tmp4, tmp5,
				top_dd_work_mode, path_sel,
				prm_dolby.dolby5_mode);
	}

	/*path switch from vpp to dpss during play*/
	if (delay_update_path) {
		if (delay_update_path == 1 && dolby_vision_on) {
			if (amdv_policy_process(vf, &mode, FORMAT_SDR)) {
				pr_dv_dbg("disconnect vd1, mode %d->%d\n",
						dolby_vision_mode, mode);
				amdv_set_toggle_flag(1);
				amdv_target_mode = mode;
				dolby_vision_mode = mode;
			}
			delay_update_path += 1;
		} else if (delay_update_path == 2) {
			path_sel = path_sel & 0xFFFFFFF0;/*disable debug path mode*/
			prm_dolby.dolby5_mode = DOLBY5_DPSS_MODE;/*update after rdma reg update*/
			top_dd_work_mode = DOLBY5_DPSS_MODE;
			if (is_aml_t6x() && enable_2ppc) {
				prm_dolby.dolby5_mode = DOLBY5_DPSS_MODE_2PPC;
				top_dd_work_mode = DOLBY5_DPSS_MODE_2PPC;
			}
			delay_update_path = 0;
			update_path_done = true;
			if (debug_dolby & 0x80000)
				pr_dv_dbg("update path done\n");
			return 0;
		}
	}
#if ENABLE_DPSS
	/*path switch from dpss to vpp during play, should check if top2 already done for cur vf*/
	if (!is_amdv_dpss_path()) {
		if ((debug_dolby & 0x80000) && vf)
			pr_dv_dbg("proc:vf[%px]dpss_flg 0x%x\n", vf, vf->dpss_flg);
		if (vf && (vf->dpss_flg & DPSS_FLG_DDD))
			prm_dolby.byps_mode = 2;
		else
			prm_dolby.byps_mode = 0;
	}
#endif

	if (load_fixed_setting && (vf_top1 || vf_top2) &&
		dolby_vision_mode != AMDV_OUTPUT_MODE_BYPASS) {
		if (force_top1_enable == 3)
			enable_top1 = true;
		if (force_top1_enable == 2)
			enable_top1 = false;

		if (enable_top1) {
			if (prm_dolby.dolby5_mode == DOLBY5_DPSS_MODE ||
				prm_dolby.dolby5_mode == DOLBY5_DPSS_MODE_2PPC) {
				if (vf_top1) {
					vf_top1->src_fmt.py_level = PY_SEVEN_LEVEL;
					top1_info.py_level = PY_SEVEN_LEVEL;
					top1_vd_info.width = 960;
					top1_vd_info.height = 540;

					top1_info.amdv_setting_video_flag = true;
					wait_first_frame_top1 = true;
					vf_top1->src_fmt.py_id = py_id++;
					py_id = py_id % TOP_BUF_CNT;
					if (top1_info.core_on_cnt == 0) {
						vf_top1->src_fmt.py_id = 0;
						dpss_cur_vf = vf_top1;
						return amdolby_vision_process_hw5_top1(vf_top1,
							display_size);
					}
				}
			} else {
				if (!vf_top1)/*for vd1 path,video_provider test*/
					vf_top1 = vf_top2;
				vf_top1->src_fmt.py_level = PY_SEVEN_LEVEL;
				top1_info.py_level = PY_SEVEN_LEVEL;
				top1_vd_info.width = 960;
				top1_vd_info.height = 540;

				top1_info.amdv_setting_video_flag = true;
				wait_first_frame_top1 = true;
				vf_top1->src_fmt.py_id = py_id++;
				py_id = py_id % TOP_BUF_CNT_VD1;

				if (top1_info.core_on_cnt == 0) {
					vf_top1->src_fmt.py_id = 0;
					return amdolby_vision_process_hw5_top1(vf_top1,
					display_size);
				}
			}
		} else {
			if (vf) {
				vf->src_fmt.py_level = PY_NO_LEVEL;
				vf->src_fmt.py_id = py_id++;
				if (is_amdv_dpss_path())
					py_id = py_id % TOP_BUF_CNT;
				else
					py_id = py_id % TOP_BUF_CNT_VD1;
			}
			top1_info.py_level = PY_NO_LEVEL;
			top1_info.amdv_setting_video_flag = false;
			wait_first_frame_top1 = false;
		}
	}
	if (vf_top1 && !vf_top2) {
		/*only_top1*/
		if (prm_dolby.dolby5_mode == DOLBY5_DPSS_MODE ||
			prm_dolby.dolby5_mode == DOLBY5_DPSS_MODE_2PPC) {
			dpss_cur_vf = vf_top1;
			if (dolby_vision_mode != AMDV_OUTPUT_MODE_BYPASS)
				return amdolby_vision_process_hw5_top1(vf_top1, display_size);
		} else {
			return amdolby_vision_process_hw5_top1(vf_top1, display_size);
		}
	}

	if (vf && vf->type & VIDTYPE_COMPRESS) {
		if (is_src_crop_valid(vf->src_crop) && !is_amdv_dpss_path()) {
			h_ori = vf->compWidth -
				vf->src_crop.left - vf->src_crop.right;
			v_ori = vf->compHeight -
				vf->src_crop.top - vf->src_crop.bottom;
			if (debug_dolby & 0x8)
				pr_dv_dbg("size %d %d, crop %d %d %d %d\n",
						  vf->compWidth, vf->compHeight,
						  vf->src_crop.left, vf->src_crop.right,
						  vf->src_crop.top, vf->src_crop.bottom);
		} else {
			h_ori = vf->compWidth;
			v_ori = vf->compHeight;
		}
	} else if (vf) {
		h_ori = vf->width;
		v_ori = vf->height;
	}
	if (prm_dolby.vds_4k1k_en)
		v_ori = v_ori >> 1;

	if (!is_amdv_dpss_path()) {
		vd_proc_info = get_vd_proc_amdv_info();
		if (vd_proc_info && tv_hw5_setting &&
			is_amdv_on() && vf &&
			top2_info.amdv_setting_video_flag) {
			if (is_aml_t3x() &&/*check if slice changed*/
				vd_proc_info->slice_num != tv_hw5_setting->force_num_slices) {
				if (debug_dolby & 0x400000)
					pr_dv_dbg("slice change %d->%d\n",
						tv_hw5_setting->force_num_slices,
						vd_proc_info->slice_num);
				dolby_vision_flags |= FLAG_TOGGLE_FRAME;
				update_top2_control_path_flag = true;
				toggle_mode = true;
			}
			if (!toggle_mode &&/*check if display_size is changed for rpt frame*/
				(top2_info.core_disp_hsize != h_size ||
				top2_info.core_disp_vsize != v_size)) {
				if (debug_dolby & 0x400000)
					pr_dv_dbg("rpt frame disp size change %d %d -> %d %d\n",
						top2_info.core_disp_hsize,
						top2_info.core_disp_vsize,
						h_size, v_size);
				dolby_vision_flags |= FLAG_TOGGLE_FRAME;
				update_top2_control_path_flag = true;
				toggle_mode = true;
			}
		}
		if (is_aml_t6x() && vd_proc_info && vd_proc_info->mosaic_mode)
			prm_dolby.mosaic_mode = 1;
	}

	if (vf) {
		/*afbc+dw two data path,dw is after detunnel,so no need dv detunnel*/
		if (vd_proc_info && vd_proc_info->vd1_no_compress && (vf->type & VIDTYPE_COMPRESS))
			/*afbc on but vpp use dw*/
			disable_detunnel = true;
		/*only yuv data path,dw is after detunnel,so no need dv detunnel*/
		/*else if (!(vf->type & VIDTYPE_COMPRESS))*/
		/*disable_detunnel = true;*/
		/*unique dv rgb/yuv444,8/10/12bit and yuv420,8/10bit:no need dv detunnel*/
		/*unique dv yuv420/422,12bit: need dv detunnel*/
		else if (vf->type_ext & VIDTYPE_EXT_BYPASS_DETUNNEL)
			disable_detunnel = true;
		else
			disable_detunnel = false;

		if (vf->type_ext & VIDTYPE_EXT_DETUNNEL_ENABLED)
			vdin_vd1_detunnel_tunnel_mode = true;
		else
			vdin_vd1_detunnel_tunnel_mode = false;
		if (debug_dolby & 0x400000)
			pr_dv_dbg("vd1 no_compress %d,flag %x,type %x %x,vdin_vd1 %d\n",
				vd_proc_info ? vd_proc_info->vd1_no_compress : -1, vf->flag,
				vf->type, vf->type_ext,
				vdin_vd1_detunnel_tunnel_mode);
	}
	//if (update_top2_control_path_flag) {
	//	amdv_hw5_control_path(vf, vd_proc_info);
	//	update_top2_control_path_flag = false;
	//}

	if (dolby_vision_flags & FLAG_CERTIFICATION) {
		h_size = h_ori;
		v_size = v_ori;
		top2_info.run_mode_count = 1 + amdv_run_mode_delay;
	} else {
		//if (vf && vf != last_vf && tv_hw5_setting)
			//update_aoi_flag(vf, display_size);
		last_vf = vf;
	}

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		/* tv control path case */
		if (top2_info.core_disp_hsize != h_size ||
			top2_info.core_disp_vsize != v_size) {
			/* tvcore need force config for res change */
			force_set = true;
			if (debug_dolby & 8)
				pr_dv_dbg
				("tv update disp size %d %d -> %d %d, ori %d %d\n",
				 top2_info.core_disp_hsize,
				 top2_info.core_disp_vsize, h_size, v_size, h_ori, v_ori);
			top2_info.core_disp_hsize = h_size;
			top2_info.core_disp_vsize = v_size;
		}
		/*check if vd1 data same as vframe original size*/
		/*1.if there is a big diff,the precision effect may be misplaced,bypassing pr*/
		/*2.debug mode for force_top1_vskip, not bypassing pr, set vskip for top1 rdmif*/
		if (enable_top1 && dolby_vision_mode != AMDV_OUTPUT_MODE_BYPASS &&
			vf && !force_top1_vskip) {
			if (h_ori >= 1920) {
				if ((h_ori >  h_size && (h_ori - h_size) > 88) ||
					(v_ori > v_size && (v_ori - v_size) > 44))
					force_bypass_precision = true;
				else
					force_bypass_precision = false;
			} else if (h_ori >= 1280) {
				if ((h_ori >  h_size && (h_ori - h_size) > 64) ||
					(v_ori > v_size && (v_ori - v_size) > 36))
					force_bypass_precision = true;
				else
					force_bypass_precision = false;
			} else if (h_ori >= 720) {
				if ((h_ori >  h_size && (h_ori - h_size) > 44) ||
					(v_ori > v_size && (v_ori - v_size) > 32))
					force_bypass_precision = true;
				else
					force_bypass_precision = false;
			} else {
				if ((h_ori >  h_size && (h_ori - h_size) > 16) ||
					(v_ori > v_size && (v_ori - v_size) > 10))
					force_bypass_precision = true;
				else
					force_bypass_precision = false;
			}
			if (force_bypass_precision) {
				if (debug_dolby & 1)
					pr_dv_dbg("vd1 size big diff with ori,bypass pd! %dx%d %dx%d\n",
						h_size, v_size, h_ori, v_ori);
			}
		}

		if (!vf || toggle_mode != 1) {
			/* log to monitor if has dv toggles not needed */
			if (debug_dolby & 0x100)
				pr_dv_dbg("NULL/RPT frame %p,hdr %s,video %s\n",
				     vf,
				     get_hdr_module_status(VD1_PATH, VPP_TOP0)
				     == HDR_MODULE_ON ? "on" : "off",
				     get_video_enabled(VD1_PATH) ? "on" : "off");
		}
	}
	last_toggle_mode = toggle_mode;

	if ((dolby_vision_flags & FLAG_CERTIFICATION))
		calculate_crc();

	if (is_amdv_dpss_path()) {
		if (!vf_top1 && !vf_top2) {
			video_turn_off = true;
			if (debug_dolby & 0x100)
				pr_dv_dbg("video off\n");
		} else {
			video_turn_off = false;
		}
	} else {
		video_status = is_video_turn_on(video_on, VD1_PATH);
		if (video_status == -1) {
			video_turn_off = true;
			if (debug_dolby & 0x100)
				pr_dv_dbg("VD1 video off, video_status -1\n");
		} else if (video_status == 1) {
			if (debug_dolby & 0x100)
				pr_dv_dbg("VD1 video on, video_status 1\n");
			video_turn_off = false;
		}
	}
	if (dolby_vision_policy == AMDV_FORCE_OUTPUT_MODE &&
		force_mode != last_force_mode) {
		force_mode_changed = 1;
		last_force_mode = force_mode;
	}

	/* monitor policy changes */
	policy_changed = is_policy_changed();

	if (policy_changed || video_status == 1 || force_mode_changed) {
		amdv_set_toggle_flag(1);
		if (debug_dolby & 100)
			pr_dv_dbg("tv %s %s\n",
				policy_changed ? "policy changed" : "",
				video_status ? "video_status changed" : "");
	}

	if ((policy_changed || force_mode_changed ||
	    video_status == 1 || need_update_cfg) &&
	    !(dolby_vision_flags & FLAG_CERTIFICATION)) {
		if (debug_dolby & 1)
			pr_dv_dbg("video %s,osd %s,vf %p,toggle %d\n",
				     video_turn_off ? "off" : "on",
				     is_graphics_output_off() ? "off" : "on",
				     vf, toggle_mode);
		if (vf &&
		    !amdv_parse_metadata
		    (vf, VD1_PATH, 0, false, false)) {
			amdv_set_toggle_flag(1);
		}
		need_update_cfg = false;
	}

	if (debug_dolby & 2)
		pr_dv_dbg("vf[%px]turn_off %d,video_status %d,toggle %d,flag %x,size %d %d,st %d %d\n",
			vf, video_turn_off, video_status,
			toggle_mode, dolby_vision_flags,
			h_size, v_size, dolby_vision_status, dolby_vision_mode);

	if ((!vf && video_turn_off) ||
	    (video_status == -1)) {
		if (amdv_policy_process(vf, &mode, FORMAT_SDR)) {
			if (debug_dolby & 0x100)
				pr_dv_dbg("Fake SDR, mode->%d\n", mode);
			amdv_set_toggle_flag(1);
		}
		if ((dolby_vision_flags & FLAG_TOGGLE_FRAME) ||
		((video_status == -1) && top2_info.core_on)) {
			if (debug_dolby & 0x1)
				pr_dv_dbg("update when video off\n");
			amdv_parse_metadata
				(NULL, VD1_PATH, 1, false, false);
			amdv_set_toggle_flag(1);
		}
		if (!vf && video_turn_off &&
			!top2_info.core_on &&
			top2_v_info.amdv_src_format != 0) {
			if (debug_dolby & 0x1)
				pr_dv_dbg("update src_fmt when video off\n");
			top2_v_info.amdv_src_format = 0;
			top2_v_info.amdv_wait_init = false;
			amdv_wait_on = false;
		}
	}
	if (dolby_vision_mode == AMDV_OUTPUT_MODE_BYPASS) {
		if (dolby_vision_status != BYPASS_PROCESS)
			enable_amdv(0);
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		if (hw5_reg_from_file) {
			top1_info.amdv_setting_video_flag = false;
			top2_info.amdv_setting_video_flag = false;
		}
		return 0;
	}
	if ((dolby_vision_flags & FLAG_CERTIFICATION) ||
	    (dolby_vision_flags & FLAG_BYPASS_VPP))
		video_effect_bypass(1);

	if (!p_funcs_stb && (!p_funcs_tv && !hw5_reg_from_file)) {
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
		top2_v_info.tv_dovi_setting_change_flag = false;
		return 0;
	}

	if (load_fixed_setting) {
		if (!efuse_checked) {
			if (is_aml_t3x()) {
				reg_value = READ_VPP_DV_REG(DOLBY5_CORE2_REG_BASE0 + 1);
			} else if (is_aml_t6w() || is_aml_t6x()) {
				if (READ_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_GCLK) == 0)
					WRITE_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_GCLK, 1);

				pr_info("dolby warp clk: 0x%x\n",
					READ_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_GCLK));
				reg_value = READ_VPP_DV_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 1);
				pr_info("top2 reg_value = 0x%x\n", reg_value);
				reg_value = READ_VPP_DV_REG(T6W_DOLBY5_CORE2_REG_BASE0 + 1);
				pr_info("top2 reg_value = 0x%x\n", reg_value);
			}
			if ((reg_value & 0x10000) == 0)
				efuse_mode = 0;
			else
				efuse_mode = 1;
			efuse_checked = true;
		}
		if (efuse_mode) {
			pr_info("dv efuse!\n");
			return 0;
		}
		if (is_aml_t3x()) {
			/*read crc for slt test*/
			if (VSYNC_RD_DV_REG(DOLBY5_CORE2_CRC_CNTRL) == 0)
				VSYNC_WR_DV_REG(DOLBY5_CORE2_CRC_CNTRL, 1);

			pr_d_log(8, "read crc %x %x, %x %x\n",
				READ_VPP_DV_REG(DOLBY5_CORE2_CRC_OUT_FRM),
				READ_VPP_DV_REG(0x250b),/*vpp crc*/
				READ_VPP_DV_REG(VPU_DOLBY_WRAP_CTRL),
				READ_VPP_DV_REG(T3X_VD1_S0_DV_BYPASS_CTRL));
		} else if (is_aml_t6w() || is_aml_t6x()) {
			/*read crc for slt test*/
			if (READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CRC_CNTRL) == 0) {
				pr_d_log(8, "enable crc\n");
				WRITE_VPP_DV_REG(T6W_DOLBY5_CORE2_CRC_CNTRL, 1);
				if (is_aml_t6w()) {
					WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_WRAP_CTRL, 1, 22, 1);
					WRITE_VPP_DV_REG_BITS(T6W_VPU_DOLBY_CRC_ACT_WIN_2,
						h_size - 1, 0, 13);
				} else if (is_aml_t6x()) {
					WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_WRAP_CTRL, 1, 22, 1);
					WRITE_VPP_DV_REG_BITS(T6X_VPU_DOLBY_CRC_ACT_WIN_2,
						h_size - 2, 0, 13);
				}
			}
			pr_d_log(8, "read crc %x %x %x\n",
				READ_VPP_DV_REG(T6W_DOLBY5_CORE2_CRC_OUT_FRM),
				is_aml_t6w() ? READ_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_CTRL) :
				READ_VPP_DV_REG(T6X_VPU_DOLBY_WRAP_CTRL),
				is_aml_t6x() ?  READ_VPP_DV_REG(T6W_VPU_DOLBY_WRAP_RO_CRC) :
				READ_VPP_DV_REG(T6X_VPU_DOLBY_WRAP_RO_CRC));
			if (vf) {
				top2_v_info.tv_dovi_setting_change_flag = true;
				top2_info.amdv_setting_video_flag = true;
			}
			if (enable_top1)
				top1_info.amdv_setting_video_flag = true;
		}
		/*for slt, for toggle each vsync*/
		if (vf)
			dolby_vision_flags |= FLAG_TOGGLE_FRAME;
	}

	if (update_top2_control_path_flag) {
		amdv_hw5_control_path(vf, vd_proc_info);
		update_top2_control_path_flag = false;
	}

	pr_done = vf ? vf->src_fmt.pr_done : false;

	if (vf_top1 && !is_amdv_dpss_path()) {/*update top1 vf_info: only for vd1 top1+top2*/
		top1_info.py_id = vf_top1->src_fmt.py_id;/*top1 write*/
		if (!is_valid_py_id(top1_info.py_id)) {
			pr_err("check err top1 py_id %d!\n", top1_info.py_id);
			top1_info.py_id = 0;
		}
		if (is_aml_t6w() || is_aml_t6x()) {
			top_info.vf_info[top1_info.py_id].vf = vf_top1;
			top_info.vf_info[top1_info.py_id].top1_start = true;
			top_info.vf_info[top1_info.py_id].top1_done = false;
			top_info.vf_info[top1_info.py_id].top2_start = false;
			top_info.vf_info[top1_info.py_id].top2_done = false;
		}
	}

	if (vf) {/*update top2 vf_info:vd1 top2 or vd1 top1+top2 or dpss top2*/
		top2_info.py_level = vf->src_fmt.py_level;
		if (get_support_max_level() == PY_SIX_LEVEL) {
			if (top2_info.py_level == PY_SEVEN_LEVEL)
				top2_info.py_level = PY_SIX_LEVEL;/*no support 7*/
		}
		top2_info.py_id = pr_done ? vf->src_fmt.py_id : 0;
		if (!enable_top1)
			top2_info.py_id = vf->src_fmt.py_id;

		/*video_provider: only run one frame*/
		if (fix_data == CASE1080p_TOP1_READFROM_FILE) {
			if (is_amdv_dpss_path())
				top2_info.py_id = top1_info.py_id;
			else/*the first vsync only run top1*/
				top2_info.py_id =
				(top1_info.py_id + TOP_BUF_CNT_VD1 - 1) % TOP_BUF_CNT_VD1;
		}
		if (!is_valid_py_id(top2_info.py_id)) {
			pr_err("check err top2 py_id %d!\n", top2_info.py_id);
			top2_info.py_id = 0;
		}
		if ((is_aml_t6w() || is_aml_t6x()) &&
			!enable_top1) {/*for only top2, need update vf_info.vf*/
			top_info.vf_info[top2_info.py_id].vf = vf;
			top_info.vf_info[top2_info.py_id].top2_start = true;
		} else if ((is_aml_t6w() || is_aml_t6x()) && enable_top1) {/*for top1 + top2*/
			top_info.vf_info[top2_info.py_id].top2_start = true;
		}
	}
	if (test_dv & DEBUG_HW5_SIX_LEVEL)
		top2_info.py_level = PY_SIX_LEVEL;//temp debug, case0 frame1, 6 level
	if (test_dv & DEBUG_HW5_NO_LEVEL)
		top2_info.py_level = PY_NO_LEVEL; //temp debug, case0 frame0, 0 level
	if (test_dv & DEBUG_HW5_SEVEN_LEVEL)
		top2_info.py_level = PY_SEVEN_LEVEL; //temp debug,7 level

	if (debug_dolby & 0x20000)
		pr_dv_dbg("fixed_setting %d,flags 0x%x,%d\n",
			load_fixed_setting, dolby_vision_flags,
			top2_v_info.tv_dovi_setting_change_flag);

	if (tv_hw5_setting &&
		!disable_detunnel &&
		tv_hw5_setting->top2.input_mode == IN_MODE_HDMI &&
		(tv_hw5_setting->top2.src_format == FORMAT_DOVI ||
		tv_hw5_setting->top2.src_format == FORMAT_DOVI_LL))
		enable_detunnel = true;

	if (bypass_detunnel_debug == 1)
		enable_detunnel = false;
	else if (bypass_detunnel_debug == 2)
		enable_detunnel = true;

	if (dolby_vision_flags & FLAG_TOGGLE_FRAME) {
		if (force_set)
			reset_flag = true;
		if (enable_top1 && !is_amdv_dpss_path()) {
			if ((amdv_reset & 1) && !top1_info.core_on &&
				top1_info.core_on_cnt == 0 &&
				!top2_info.core_on)
				reset_flag = true;
		} else {
			if ((amdv_reset & 1) && !top2_info.core_on &&
				top2_info.core_on_cnt == 0)
				reset_flag = true;
		}
		if (is_aml_t6w() || is_aml_t6x()) {
			cfg_dolby_pre_set(&prm_dolby);
			if (enable_top1 && vf_top1) {
				if (vf_top1->type & VIDTYPE_COMPRESS) {
					if (is_src_crop_valid(vf_top1->src_crop) &&
						!is_amdv_dpss_path()) {
						core2_hsize_for_corep = vf_top1->compWidth -
							vf_top1->src_crop.left -
							vf_top1->src_crop.right;
						core2_vsize_for_corep = vf_top1->compHeight -
							vf_top1->src_crop.top -
							vf_top1->src_crop.bottom;
					} else {
						core2_hsize_for_corep = vf_top1->compWidth;
						core2_vsize_for_corep = vf_top1->compHeight;
					}
				} else if (vf_top1) {
					core2_hsize_for_corep = vf_top1->width;
					core2_vsize_for_corep = vf_top1->height;
				}
			}
			if (prm_dolby.vds_4k1k_en)
				core2_vsize_for_corep = core2_vsize_for_corep >> 1;

			/*for dpss, this func only process top2, for vpp, process top1+top2*/
			tmp = is_amdv_dpss_path() ? 2 : 3;
			cfg_dolby_update(&prm_dolby, enable_detunnel,
						core2_hsize_for_corep, core2_vsize_for_corep,
						h_size, v_size, tmp, reset_flag);
		}

		if (top2_v_info.tv_dovi_setting_change_flag || force_set ||
			(top2_info.core_on && force_update_top2)) {

			if (is_aml_t6w() || is_aml_t6x()) {
				if (enable_top1)
					get_py_weight(&prm_dolby, true);
				if (tv_hw5_setting)
					cfg_dolby_top
					(&prm_dolby, tv_hw5_setting->top1_reg,
					 tv_hw5_setting->top1b_reg,
					 tv_hw5_setting->top2_reg,
					 top2_info.amdv_setting_video_flag, /* video enable */
					 reset_flag, toggle_mode, pr_done,
					 vf_top1 ? true : false, true);
				else if (hw5_reg_from_file)
					cfg_dolby_top
					(&prm_dolby, NULL, NULL, NULL,
					 top2_info.amdv_setting_video_flag, /* video enable */
					 reset_flag, toggle_mode, pr_done,
					 vf_top1 ? true : false, true);
			} else {
				if (tv_hw5_setting)
					tv_top_set
					(tv_hw5_setting->top1_reg,
					 tv_hw5_setting->top1b_reg,
					 tv_hw5_setting->top2_reg,
					 h_size, v_size,
					 top2_info.amdv_setting_video_flag, /* video enable */
					 enable_detunnel, reset_flag, true, pr_done);
				else if (hw5_reg_from_file)
					tv_top_set
					(NULL, NULL, NULL,
					 h_size, v_size,
					 top2_info.amdv_setting_video_flag, /* video enable */
					 false, reset_flag, true, pr_done);
			}

			if (!h_size || !v_size)
				top2_info.amdv_setting_video_flag = false;
			if (top2_info.amdv_setting_video_flag &&
			    top2_info.core_on_cnt == 0) {
				if (debug_dolby & 0x100)
					pr_dv_dbg("first frame reset %d %d\n",
					     reset_flag, force_set);
			}
			enable_amdv(1);
			if (vf_top1) {
				if (debug_dolby & 0x2)
					pr_dv_dbg("update %px(index %d) pr_done %d=>1\n",
						vf_top1, vf_top1->frame_index,
						vf_top1->src_fmt.pr_done);
				vf_top1->src_fmt.pr_done = true;
			}
			if (tv_hw5_setting)
				update_amdv_status(tv_hw5_setting->top2.src_format);
			else if (load_fixed_setting)
				update_amdv_status(FORMAT_SDR);
			top2_v_info.tv_dovi_setting_change_flag = false;
			if (tv_hw5_setting && last_tv_hw5_setting)
				memcpy(last_tv_hw5_setting, tv_hw5_setting,
				sizeof(*tv_hw5_setting));
		}
		dolby_vision_flags &= ~FLAG_TOGGLE_FRAME;
	} else if (top2_info.core_on) {
		if (force_set || force_update_top2) {
			if (force_set)
				reset_flag = true;
			if (is_aml_t6w() || is_aml_t6x()) {
				if (tv_hw5_setting)
					cfg_dolby_top
					(&prm_dolby, tv_hw5_setting->top1_reg,
					 tv_hw5_setting->top1b_reg,
					 tv_hw5_setting->top2_reg,
					 top2_info.amdv_setting_video_flag, /* video enable */
					 reset_flag, toggle_mode, pr_done,
					 vf_top1 ? true : false, true);
				else if (hw5_reg_from_file)
					cfg_dolby_top
					(&prm_dolby, NULL, NULL, NULL,
					 top2_info.amdv_setting_video_flag, /* video enable */
					 reset_flag, toggle_mode, pr_done,
					 vf_top1 ? true : false, true);
			} else {
				if (tv_hw5_setting)
					tv_top_set
					(tv_hw5_setting->top1_reg,
					 tv_hw5_setting->top1b_reg,
					 tv_hw5_setting->top2_reg,
					 h_size, v_size,
					 top2_info.amdv_setting_video_flag, /* BL enable */
					 enable_detunnel, reset_flag, toggle_mode, pr_done);
				else if (hw5_reg_from_file)
					tv_top_set
					(NULL, NULL, NULL,
					 h_size, v_size,
					 top2_info.amdv_setting_video_flag, /* BL enable */
					 false, reset_flag, toggle_mode, pr_done);
			}
		}
	}
	if (top2_info.core_on) {
		if (top2_info.run_mode_count <= amdv_run_mode_delay + 1)
			top2_info.run_mode_count++;
	} else {
		top2_info.run_mode_count = 0;
	}
	return 0;
}

int get_support_max_level(void)
{
	if (is_aml_t6w()) {
		return PY_SIX_LEVEL;
	} else if (is_aml_t6x()) {
		if (force_corep_max_level6)
			return PY_SIX_LEVEL;
		else
			return PY_SEVEN_LEVEL;
	} else {
		return PY_SEVEN_LEVEL;
	}
}

