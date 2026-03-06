// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux headers */
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cma.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/dma-map-ops.h>
#include <linux/delay.h>
#include <linux/sched/signal.h>
#include <linux/sched.h>

/* Amlogic headers */
#include <linux/amlogic/media/vfm/vframe.h>
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
#include <linux/amlogic/tee.h>
#endif

/* Local headers */
#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_canvas.h"
#include "vdin_ctl.h"
#include "vdin_hdr.h"
#include "vdin_mem_scatter.h"
#include "vdin_afbce.h"

/*the value depending on dts config mem limit
 *for skip two vframe case,need +2
 */
static unsigned int max_buf_width = VDIN_CANVAS_MAX_WIDTH_HD;
static unsigned int max_buf_height = VDIN_CANVAS_MAX_HEIGHT;
/* one frame max metadata size:32x280 bits = 1120bytes(0x460) */
unsigned int dolby_size_byte = K_DV_META_BUFF_SIZE;

const unsigned int vdin_canvas_ids[VDIN_MAX_DEVS][VDIN_CANVAS_MAX_CNT] = {
	{	0x22, 0x23, 0x24, 0x25,
		0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b,
		0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31
	},
	{
		0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x38, 0x39,
	},
	{	/* fake canvas ids for vdin2 on t3x */
		0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
		0x38, 0x39,
	},
};

unsigned int vdin_get_canvas_num(struct vdin_dev_s *devp)
{
	return (devp->hw_core == VDIN_HW_CORE_NORMAL) ?
			VDIN0_CANVAS_MAX_CNT : VDIN1_CANVAS_MAX_CNT;
}

//#define TEE_MEM_TYPE_VDIN	0x7

/*function:
 *	1.set canvas_max_w & canvas_max_h
 *	2.set canvas_max_size & canvas_max_num
 *	3.set canvas_id & canvas_addr
 */
void vdin_canvas_init(struct vdin_dev_s *devp)
{
	int i, canvas_id;
	unsigned int canvas_addr;
	int canvas_max_w = 0;
	int canvas_max_h = VDIN_CANVAS_MAX_HEIGHT;

	canvas_max_w = VDIN_CANVAS_MAX_WIDTH_HD << 1;

	devp->canvas_max_size = PAGE_ALIGN(canvas_max_w * canvas_max_h);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	if (devp->canvas_max_num > VDIN_CANVAS_MAX_CNT)
		devp->canvas_max_num = VDIN_CANVAS_MAX_CNT;

	devp->mem_start = roundup(devp->mem_start, devp->canvas_align);
	pr_info("vdin.%d canvas initial table:\n", devp->index);
	for (i = 0; i < devp->canvas_max_num; i++) {
		canvas_id = vdin_canvas_ids[devp->index][i];
		canvas_addr = devp->mem_start + devp->canvas_max_size * i;

		canvas_config(canvas_id, canvas_addr,
			      canvas_max_w, canvas_max_h,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		pr_info("\t%d: 0x%x-0x%x  %dx%d (%d KB)\n",
			canvas_id, canvas_addr,
			(canvas_addr + devp->canvas_max_size),
			canvas_max_w, canvas_max_h,
			(devp->canvas_max_size >> 10));
	}
}

void vdin_cal_canvas_w(struct vdin_dev_s *devp)
{
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = devp->dts_config.max_buf_num;
	unsigned int h_active, v_active;

	if (devp->vf_mem_size_small) {
		h_active = devp->h_shrink_out;
		v_active = devp->v_shrink_out;
	} else {
		if (devp->hw_core == VDIN_HW_CORE_LITE) {
			if (vdin_support_axis_change(devp)) {
				h_active = devp->dest_h_active;
				v_active = devp->dest_v_active;
			} else {
				h_active = devp->h_active;
				v_active = devp->v_active;
			}
		} else {
			h_active = devp->h_active + devp->crop_h;
			v_active = devp->v_active + devp->crop_v;
		}
	}
	if (devp->set_canvas_manual || devp->cfg_dma_buf) //pixels align up to 64 in keystone
		h_active = roundup(h_active, devp->canvas_align);

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
	case VDIN_FORMAT_CONVERT_YUV_GBR:
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH ||
		    devp->local_var.is_rgba_en)
			devp->canvas_w = h_active * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = h_active * VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		/* screencap case:S4/S4D cannot use the same canvas id */
		if (devp->dtdata->hw_ver == VDIN_HW_S4 ||
			devp->dtdata->hw_ver == VDIN_HW_S4D)
			canvas_num = vdin_get_canvas_num(devp);
		/* screencap case:end */
		if (!devp->baddr_en)
			canvas_num = canvas_num / 2;

		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) {
			if (devp->dtdata->hw_ver >= VDIN_HW_T6D &&
			    devp->dtdata->hw_ver <= VDIN_HW_A9)
				devp->canvas_w = (h_active * 10) / 8;
			else
				devp->canvas_w = (h_active * 3) / 2;
		} else {
			devp->canvas_w = h_active;
		}
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) {
			if ((devp->dtdata->hw_ver == VDIN_HW_T6W ||
				devp->dtdata->hw_ver == VDIN_HW_T6X) &&
				devp->yuv422_2plane_en)
				devp->canvas_w = (h_active * 10) / 8;
			else if (devp->full_pack == VDIN_422_FULL_PK_EN)
				devp->canvas_w = (h_active * 5) / 2;
			else if (devp->full_pack == VDIN_422_FULL_PK_DIS)
				devp->canvas_w = h_active * VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
		} else {
			devp->canvas_w = h_active * VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
		}

		/* dw only support 8bit mode */
		if (devp->double_wr)
			devp->canvas_w = h_active * VDIN_YUV422_8BIT_PER_PIXEL_BYTE;

		break;
	default:
		break;
	}
	if (devp->double_wr && (devp->dv_hw5.hw5_ctl & BIT3)) {
		devp->canvas_w = h_active * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		pr_info("dw_wrmif_444_10bit,%dx%d;canvas_w:%d\n",
			h_active, v_active, devp->canvas_w);
	}
	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, devp->canvas_align);
	devp->canvas_h = v_active;

	if (vdin_is_convert_to_nv21(devp->format_convert))
		devp->chroma_size = devp->canvas_w * devp->canvas_h / 2;
	else if (vdin_is_convert_to_422(devp->format_convert) &&
		(devp->dtdata->hw_ver == VDIN_HW_T6W || devp->dtdata->hw_ver == VDIN_HW_T6X) &&
			devp->yuv422_2plane_en)
		devp->chroma_size = devp->canvas_w * devp->canvas_h;

	devp->canvas_max_size =
		PAGE_ALIGN(devp->canvas_w * devp->canvas_h + devp->chroma_size);
	/* devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size; */

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);
	if (devp->canvas_max_num < devp->vf_mem_max_cnt) {
		pr_err("vdin%d canvas_max_num %d less vf_mem_max_cnt %d\n",
			devp->index, devp->canvas_max_num, devp->vf_mem_max_cnt);
	}
	devp->vf_mem_max_cnt = min(devp->vf_mem_max_cnt, devp->canvas_max_num);
}

unsigned int vdin_422_hsize_canvas_alignment(struct vdin_dev_s *devp, int h_size)
{
	/* 422 mode, 4k up to 444 8bit mode,
	 * other up to 10 bit mode
	 */
	if (devp->hw_core == VDIN_HW_CORE_NORMAL) {
		if (vdin_is_8bit_needed(devp) && !vdin_is_dolby_signal_in(devp)) {
			/*up to 444 8bit*/
			h_size = roundup(h_size * VDIN_YUV444_8BIT_PER_PIXEL_BYTE,
					 devp->canvas_align);
			devp->canvas_align_w = h_size / VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		} else {
			if (devp->force_malloc_yuv_422_to_444) {
				if (vdin_is_8bit_needed(devp)) {
					h_size = roundup(h_size * VDIN_YUV444_8BIT_PER_PIXEL_BYTE,
					 devp->canvas_align);
					devp->canvas_align_w = h_size /
						VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
				} else {
					/*up to 444 10bit*/
					h_size = roundup(h_size * VDIN_YUV444_10BIT_PER_PIXEL_BYTE,
							 devp->canvas_align);
					devp->canvas_align_w = h_size /
						VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
				}
			} else {
				if (devp->full_pack == VDIN_422_FULL_PK_EN) {
					h_size = roundup((h_size * 5) / 2,
							 devp->canvas_align);
					devp->canvas_align_w = (h_size * 2) / 5;
				} else {
					h_size = h_size * VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
					h_size = roundup(h_size, devp->canvas_align);
					devp->canvas_align_w = h_size /
						VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
				}
			}
		}
	} else if (devp->hw_core == VDIN_HW_CORE_LITE) {
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) {
			if (devp->full_pack == VDIN_422_FULL_PK_EN) {
				h_size = roundup((h_size * 5) / 2, devp->canvas_align);
				devp->canvas_align_w = (h_size * 2) / 5;
			} else {
				h_size = roundup(h_size * VDIN_YUV422_10BIT_PER_PIXEL_BYTE,
						 devp->canvas_align);
				devp->canvas_align_w = h_size /
					VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
			}
		} else {
			h_size = roundup(h_size * VDIN_YUV422_8BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_align_w = h_size / VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
		}
	}
	return h_size;
}

unsigned int get_vdin_req_mem(struct vdin_dev_s *devp)
{
	unsigned int mem_size;

	vdin_dv_hw5_init(devp);
	vdin_set_decimation(devp); //get origin_hv
	vdin_set_cutwin(devp, 0); //get cutwin_hv
	vdin_get_format_convert(devp);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/* check if need enable afbce */
	vdin_afbce_mode_init(devp);
#endif
	vdin_double_write_confirm(devp);//get dw status
	vdin_set_bitdepth(devp, 0);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	vdin_mem_init(devp); //get scatter on_off
#endif
	vdin_get_hv_shrink(devp); //get hv_shrink_size
	mem_size = count_vdin_req_mem(devp);
	devp->get_vdin_mem_size_flag = 0;
	if (devp->debug.vdin_dbg_en)
		pr_info("count_vdin_req_mem:(%d)B\n", mem_size);

	return mem_size;
}

unsigned int count_vdin_req_mem(struct vdin_dev_s *devp)
{
	unsigned int mem_size, h_size, v_size, frame_size = 0, afrc_body_y_size = 0;
	unsigned int max_buffer_num = devp->dts_config.min_buf_num;
	unsigned int sct_body_size, cpr_body_size;
	unsigned int h_tmp, v_tmp, blk_num;

	if (devp->rdma_enable)
		max_buffer_num++;
	/*todo: need update if vf_skip_cnt used by other port*/
	if (devp->vfp->skip_vf_num &&
	    (IS_HDMI_SRC(devp->parm.port) || IS_TVAFE_SRC(devp->parm.port)))
		max_buffer_num += devp->vfp->skip_vf_num;

	if (max_buffer_num > devp->dts_config.max_buf_num)
		max_buffer_num = devp->dts_config.max_buf_num;

	/*
	 * if screencap one buffer, set buff num to 1,
	 * else vfmem_cfg_num define in dts, use dts's setting
	 */
	if (devp->hw_core == VDIN_HW_CORE_LITE && devp->is_one_buffer) {
		max_buffer_num = 1;
		pr_info("screencap one buffer\n");
	} else if (devp->frame_buff_num >= devp->dts_config.min_buf_num &&
		devp->frame_buff_num <= devp->dts_config.max_buf_num) {
		if (devp->afbce_valid && (devp->cma_config_flag & MEM_ALLOC_CODEC_SCT)) {
			/* need extra one buffer for sct mem */
			if (devp->frame_buff_num_bak < VDIN_CANVAS_MAX_CNT)
				devp->frame_buff_num = devp->frame_buff_num_bak + 1;
		}
		max_buffer_num = devp->frame_buff_num;
	}

	if (!devp->index && devp->frame_buff_num < VDIN_CANVAS_INTERLACED_MIN_CNT &&
	    devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED)
		max_buffer_num = VDIN_CANVAS_INTERLACED_MIN_CNT;

	devp->canvas_max_num = max_buffer_num;
	devp->vf_mem_max_cnt = max_buffer_num;

	/*pixels*/
	if (devp->hw_core == VDIN_HW_CORE_LITE) {
		if (vdin_support_axis_change(devp)) {
			h_size = devp->dest_h_active;
			v_size = devp->dest_v_active;
		} else {
			h_size = devp->h_active;
			v_size = devp->v_active;
		}
	} else {
		h_size = devp->h_active + devp->crop_h;
		v_size = devp->v_active + devp->crop_v;
	}

	if (devp->set_canvas_manual || devp->cfg_dma_buf) //pixels align up to 64 in keystone
		h_size = roundup(h_size, devp->canvas_align);

	if (devp->dts_config.canvas_config_mode == 1) {
		h_size = max_buf_width;
		v_size = max_buf_height;
	}

	if (vdin_is_convert_to_444(devp->format_convert)) {
		if (devp->local_var.is_rgba_en) {
			h_size = roundup(h_size * VDIN_RGBA8888_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_align_w = h_size / VDIN_RGBA8888_PER_PIXEL_BYTE;
		} else if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH ||
		    devp->dv_hw5.hw5_ctl & BIT3) && !vdin_is_8bit_needed(devp)) {
			h_size = roundup(h_size * VDIN_YUV444_10BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_align_w = h_size / VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		} else {
			/* before t6x, 4k is not support 10 bit mode in order to save memory
			 * up to 4k 444 8bit mode
			 */
			h_size = roundup(h_size * VDIN_YUV444_8BIT_PER_PIXEL_BYTE,
				devp->canvas_align);
			devp->canvas_align_w = h_size / VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		}
	} else if (vdin_is_convert_to_nv21(devp->format_convert)) {
		/* nv21/nv12 only t3x have 8/10/12 bit mode */
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) {
			if (devp->dtdata->hw_ver >= VDIN_HW_T6D &&
			    devp->dtdata->hw_ver <= VDIN_HW_A9)
				h_size = roundup((h_size * 10) / 8, devp->canvas_align);
			else
				h_size = roundup((h_size * 3) / 2, devp->canvas_align);
		} else {
			h_size = roundup(h_size, devp->canvas_align);
		}
		devp->canvas_align_w = h_size;
		/*todo change with canvas alloc!!*/
	} else {
		h_size = vdin_422_hsize_canvas_alignment(devp, h_size);
	}

	/*1 frame bytes*/
	mem_size = h_size * v_size;
	/* for almost uncompressed pattern,garbage at bottom
	 * 1024x1658 is the worst case,each page wast 2160x3x256byte for 4096
	 * since every block must not be separated by 2 pages
	 */
	if (devp->afbce_valid)
		mem_size += 1024 * 1658;

	if (vdin_is_convert_to_nv21(devp->format_convert))
		mem_size = (mem_size * 3) / 2;
	devp->vf_mem_size = PAGE_ALIGN(mem_size)/* + dolby_size_byte*/;
	devp->vf_mem_size = roundup(devp->vf_mem_size, PAGE_SIZE);

	if (devp->double_wr || K_FORCE_HV_SHRINK) {
		if (devp->h_shrink_out < devp->h_active &&
		    devp->v_shrink_out < devp->v_active)
			devp->vf_mem_size_small = devp->vf_mem_size /
				devp->h_shrink_times / devp->v_shrink_times;
		else if (devp->h_shrink_out < devp->h_active)
			devp->vf_mem_size_small =
				devp->vf_mem_size / devp->h_shrink_times;
		else if (devp->v_shrink_out < devp->v_active)
			devp->vf_mem_size_small =
				devp->vf_mem_size / devp->v_shrink_times;
		else /* dw size:1:1 */
			devp->vf_mem_size_small = devp->vf_mem_size;
		if (devp->dv_hw5.hw5_ctl & BIT3) {
			devp->vf_mem_size_small = devp->h_shrink_out * devp->v_shrink_out *
				VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
			pr_info("vdin%d,dw_wrmif_444_10bit,%dx%d",
				devp->index, devp->h_shrink_out, devp->v_shrink_out);
		}
		devp->canvas_max_stride = devp->vf_mem_size_small / devp->v_shrink_out;
	} else {
		devp->vf_mem_size_small = 0;
		devp->canvas_max_stride = h_size;
	}

	if (devp->vf_mem_size_small)
		devp->vf_mem_size_small = roundup(devp->vf_mem_size_small, PAGE_SIZE);

	/* frame is consist of the following part
	 * 1st, small frame for dw
	 * 2nd, big frame for dw, or normal frame for non-dw
	 * 3rd, header if afbce enabled
	 * 4th, table if afbce enabled
	 * ---------------------
	 * |    small frame    |
	 * |                   |
	 * |-------------------|
	 * |                   |
	 * |    normal frame   |
	 * |                   |
	 * |-------------------|
	 * |      header       |
	 * |-------------------|
	 * |      table        |
	 * |-------------------|
	 */
	if (devp->afbce_info) {
		if (devp->afbce_valid) {
			/* allocate mem according to resolution
			 * each block contains 32 * 4 pixels
			 * one block associated to one header(4 bytes)
			 * dolby has one page size, & each vframe aligned to page size
			 * (((h(align 32 pixel) * v(align 4 pixel)) / (32 * 4)) * 4)  + dolby
			 * block should align to,  H:2 blocks, V: 16 blocks
			 */
			devp->afbce_info->frame_head_size =
			PAGE_ALIGN((roundup(devp->h_active, 64) *
				    roundup(devp->v_active, 64)) / 32
				    /* + dolby_size_byte*/);
			/*((h * v * byte_per_pixel + dolby) / page_size) * 4(one address size)
			 * total max_buffer_num
			 */
			devp->afbce_info->frame_table_size =
				roundup(((devp->vf_mem_size * 4) / PAGE_SIZE), 4);
			devp->afbce_info->frame_body_size = devp->vf_mem_size;
			if (devp->is_vfce_en) {
				h_tmp = roundup(devp->h_active, 64);
				v_tmp = roundup(devp->v_active, 8);
				blk_num = h_tmp * v_tmp / (16 * 4);

				devp->afbce_info->frame_head_size_y = blk_num * 2;
				devp->afbce_info->frame_head_size_y =
					PAGE_ALIGN(devp->afbce_info->frame_head_size_y);

				afrc_body_y_size = blk_num *
					devp->cr_lossy_param.cr_lossy_target_byte;
				afrc_body_y_size += VDIN_AFRC_EXT_MEM;
				afrc_body_y_size = roundup(afrc_body_y_size, 16 * 4096);
				devp->afbce_info->frame_body_size_y = afrc_body_y_size;
				devp->afbce_info->frame_table_size_y = afrc_body_y_size / 1024;

				if (vdin_is_convert_to_444(devp->format_convert)) {
					devp->afbce_info->frame_head_size =
						devp->afbce_info->frame_head_size_y * 3;
					devp->afbce_info->frame_body_size = afrc_body_y_size * 3;
				} else if (vdin_is_convert_to_422(devp->format_convert)) {
					devp->afbce_info->frame_head_size =
						devp->afbce_info->frame_head_size_y * 2;
					devp->afbce_info->frame_body_size = afrc_body_y_size * 2;
				} else {
					devp->afbce_info->frame_head_size =
						(devp->afbce_info->frame_head_size_y * 3) / 2;
					devp->afbce_info->frame_body_size =
						(afrc_body_y_size * 3) / 2;
				}
				devp->afbce_info->frame_body_size =
					roundup(devp->afbce_info->frame_body_size, 16 * 4096);
				/* table phy address should be 128-bit alignment(about DDR read) */
				devp->afbce_info->frame_table_size =
					devp->afbce_info->frame_body_size / 1024;/* *4/4096 */
			}
			frame_size = devp->vf_mem_size_small +
				devp->afbce_info->frame_body_size +
				devp->afbce_info->frame_head_size +
				devp->afbce_info->frame_table_size;

			if (devp->secure_en == VDIN_SECURE_FLAGS)
				frame_size -= devp->afbce_info->frame_table_size;

			if (devp->mem_type == VDIN_MEM_TYPE_SCT)
				frame_size -= devp->afbce_info->frame_body_size;
		} else {
			devp->afbce_info->frame_head_size = 0;
			devp->afbce_info->frame_table_size = 0;
			devp->afbce_info->frame_body_size = 0;
			frame_size = devp->vf_mem_size_small + devp->vf_mem_size;
		}
		pr_info("vdin%d small = %#x, vf_mem = %#x, head:%#x,table:%#x,frame_size:%#x.mem_size:%#x\n",
			devp->index, devp->vf_mem_size_small, devp->vf_mem_size,
			devp->afbce_info->frame_head_size, devp->afbce_info->frame_table_size,
			frame_size, mem_size);
	} else {
		frame_size = devp->vf_mem_size_small + devp->vf_mem_size;
	}

	if (devp->afbce_info && devp->afbce_valid && devp->mem_type == VDIN_MEM_TYPE_SCT) {
		/* for vdin_cma_alloc calculation use */
		devp->frame_size = PAGE_ALIGN(frame_size);
		/* codec_mm_scatter_source_check (0):no secure (1):secure. seurce todo
		 * codec_mm_scatter_source_check 2:codec_alloc,0/1/3: system_alloc, 4:error
		 */
		if (codec_mm_scatter_source_check(0) == 2) {
			sct_body_size = devp->afbce_info->frame_body_size;
			/* compression ratio 60% */
			cpr_body_size = (unsigned int)(sct_body_size  * 3 / 5);
			mem_size = PAGE_ALIGN(frame_size + sct_body_size) * 3 +
				PAGE_ALIGN(frame_size + cpr_body_size) * (max_buffer_num - 3);
		} else if (codec_mm_scatter_source_check(0) == 0 ||
			codec_mm_scatter_source_check(0) == 1 ||
			codec_mm_scatter_source_check(0) == 3) {
			mem_size = PAGE_ALIGN(frame_size) * max_buffer_num;
		} else {
			pr_info("codec_mm_scatter_source_check err:%d/n",
				codec_mm_scatter_source_check(0));
			return -1;
		}
	} else {
		devp->frame_size = PAGE_ALIGN(frame_size);
		/*total frames bytes*/
		mem_size = PAGE_ALIGN(frame_size) * max_buffer_num;
	}

	devp->frame_size_bak = devp->frame_size; /* backup */
	mem_size = roundup(mem_size, PAGE_SIZE);
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
	/* must align to 64k for secure protection
	 * always align for switch secure mode dynamically
	 */
	mem_size = roundup(mem_size, 64 * 1024);
#endif
	devp->mem_size_bak = mem_size;
	devp->canvas_max_num_bak = max_buffer_num;/* backup */

	pr_info("max_buf: %u frame_size:%#x.mem_size:%#x, codec_sct_src_chk:%d\n",
		max_buffer_num, frame_size, mem_size, codec_mm_scatter_source_check(0));

	if (mem_size > devp->cma_mem_size &&
	    !(devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) &&
	    !devp->set_canvas_manual &&
	    !devp->cfg_dma_buf) {
		pr_err("vdin[%d] warning: cma_mem_size (need 0x%x, cur 0x%x) is not enough!!!\n",
		       devp->index, mem_size, devp->cma_mem_size);
		/*mem_size = devp->cma_mem_size;*/
		/*devp->cma_mem_alloc = 0;*/
		/*return 1;*/
	}
	if (devp->debug.vdin_manual_req_mem_size)
		mem_size = devp->debug.vdin_manual_req_mem_size;

	return mem_size;
}

/*function:canvas_config when canvas_config_mode=1
 *	1.set canvas_w and canvas_h
 *	2.set canvas_max_size and canvas_max_num
 *	3.when dest_cfmt is TVIN_NV12/TVIN_NV21,
 *		buf width add canvas_w*canvas_h
 *based on parameters:
 *	format_convert/	source_bitdepth/
 *	v_active color_depth_mode/	prop.dest_cfmt
 */
void vdin_canvas_start_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned int chroma_size = 0;
	unsigned int canvas_step = 1;
	unsigned int canvas_num = VDIN_CANVAS_MAX_CNT;
	unsigned int max_buffer_num = devp->dts_config.max_buf_num;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
	case VDIN_FORMAT_CONVERT_YUV_GBR:
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH)
			devp->canvas_w = max_buf_width * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = max_buf_height * VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
		break;
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		devp->canvas_w = max_buf_width;
		canvas_num = canvas_num / 2;
		canvas_step = 2;
		break;
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH &&
		    devp->full_pack == VDIN_422_FULL_PK_EN)
			devp->canvas_w = (max_buf_width * 5) / 2;
		else if ((devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH) &&
			 (devp->full_pack == VDIN_422_FULL_PK_DIS))
			devp->canvas_w = max_buf_width * VDIN_YUV422_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = max_buf_width * VDIN_YUV422_8BIT_PER_PIXEL_BYTE;
		break;
	default:
		break;
	}

	/*backup before roundup*/
	devp->canvas_active_w = devp->canvas_w;
	if (devp->force_yuv444_malloc == 1) {
		/* 4k is not support 10 bit mode in order to save memory */
		if (devp->source_bitdepth > VDIN_MIN_SOURCE_BITDEPTH &&
		    !(vdin_is_8bit_needed(devp) && !vdin_is_dolby_signal_in(devp)))
			devp->canvas_w = devp->h_active * VDIN_YUV444_10BIT_PER_PIXEL_BYTE;
		else
			devp->canvas_w = devp->h_active * VDIN_YUV444_8BIT_PER_PIXEL_BYTE;
	}
	/*canvas_w must ensure divided exact by 256bit(32byte)*/
	devp->canvas_w = roundup(devp->canvas_w, devp->canvas_align);
	devp->canvas_h = devp->v_active;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		chroma_size = devp->canvas_w * devp->canvas_h / 2;
		break;
	default:
		break;
	}

	devp->canvas_max_size = PAGE_ALIGN(devp->canvas_w * devp->canvas_h + chroma_size);
	devp->canvas_max_num  = devp->mem_size / devp->canvas_max_size;

	devp->canvas_max_num = min(devp->canvas_max_num, canvas_num);
	devp->canvas_max_num = min(devp->canvas_max_num, max_buffer_num);

	if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x100)) {
		/*use_reserved_mem or alloc_from_contiguous*/
		devp->mem_start = roundup(devp->mem_start, devp->canvas_align);
#ifdef VDIN_DEBUG
		pr_info("vdin%d canvas start configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			canvas_id = vdin_canvas_ids[devp->index][i * canvas_step];
			canvas_addr = devp->mem_start +
				devp->canvas_max_size * i;
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id + 1,
					      canvas_addr +
					      devp->canvas_w * devp->canvas_h,
					      devp->canvas_w,
					      devp->canvas_h / 2,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	} else if (devp->cma_config_flag & 0x100) {
#ifdef VDIN_DEBUG
		pr_info("vdin%d canvas start configuration table:\n",
			devp->index);
#endif
		for (i = 0; i < devp->canvas_max_num; i++) {
			devp->vf_mem_start[i] =
				roundup(devp->vf_mem_start[i], devp->canvas_align);
			canvas_id = vdin_canvas_ids[devp->index][i * canvas_step];
			canvas_addr = devp->vf_mem_start[i];
			canvas_config(canvas_id, canvas_addr,
				devp->canvas_w, devp->canvas_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
			if (chroma_size)
				canvas_config(canvas_id + 1,
					      canvas_addr +
					      devp->canvas_w * devp->canvas_h,
					      devp->canvas_w,
					      devp->canvas_h / 2,
					      CANVAS_ADDR_NOWRAP,
					      CANVAS_BLKMODE_LINEAR);
#ifdef VDIN_DEBUG
			pr_info("\t%3d: 0x%lx-0x%lx %ux%u\n",
				canvas_id, canvas_addr,
				canvas_addr + devp->canvas_max_size,
				devp->canvas_w, devp->canvas_h);
#endif
		}
	}
}

/*
 *this function used for configure canvas when canvas_config_mode=2
 *base on the input format
 *also used for input resolution over 1080p such as camera input 200M,500M
 *YUV422-8BIT:1pixel = 2byte;
 *YUV422-10BIT:1pixel = 3byte;
 *YUV422-10BIT-FULL_PACK:1pixel = 2.5byte;
 *YUV444-8BIT:1pixel = 3byte;
 *YUV444-10BIT:1pixel = 4byte
 */
void vdin_canvas_auto_config(struct vdin_dev_s *devp)
{
	int i = 0;
	int canvas_id;
	unsigned long canvas_addr;
	unsigned long chroma_canvas_addr;
	unsigned int canvas_step = 1;

	vdin_cal_canvas_w(devp);

	if (devp->baddr_en) /* use phy addr */
		return;

	if (vdin_is_convert_to_nv21(devp->format_convert))
		canvas_step = 2;

	for (i = 0; i < devp->canvas_max_num; i++) {
		devp->vf_mem_start[i] =
			roundup(devp->vf_mem_start[i], devp->canvas_align);
		canvas_id = vdin_canvas_ids[devp->index][i * canvas_step];
		canvas_addr = devp->vf_mem_start[i];
		canvas_config(canvas_id, canvas_addr,
			      devp->canvas_max_stride, devp->canvas_h,
			      CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		/* vdin v4l2:two non contiguous planes */
		if (devp->work_mode == VDIN_WORK_MD_V4L &&
			devp->v4l2_fmt.fmt.pix_mp.num_planes == 2) {
			chroma_canvas_addr = devp->vf_mem_c_start[i];
			if (devp->vf_mem_c_start[i] == 0)
				pr_err("vdin%d,addr for chroma is NULL!\n", devp->index);
		} else {
			chroma_canvas_addr = canvas_addr + devp->canvas_max_stride * devp->canvas_h;
		}
		if (devp->chroma_size)
			canvas_config(canvas_id + 1,
				      chroma_canvas_addr,
				      devp->canvas_max_stride,
				      devp->canvas_h / 2,
				      CANVAS_ADDR_NOWRAP,
				      CANVAS_BLKMODE_LINEAR);
	}
	if (devp->debug.vdin_dbg_en)
		pr_info("vdin%d,canvas:%d %d %d;chroma_size:%#x,max_num:%d\n", devp->index,
			devp->canvas_max_stride, devp->canvas_w, devp->canvas_h,
			devp->chroma_size, devp->canvas_max_num);
}

#ifdef CONFIG_CMA

void vdin_mem_memset(struct vdin_dev_s *devp)
{
	int highmem_flag = 0;
	void *buf = NULL;
	void *buf_1 = NULL;
	unsigned int j = 0;
	unsigned int cnt = 0;
	int i = 0;
	int h = 0;
	int len = 0;
	int byt_index = 0;
	unsigned int count, span, width;
	unsigned int frame_size = 0;
	unsigned char *ptr = NULL;
	unsigned char *p = NULL;
	unsigned char yuv422_10l[5] = {0x1, 0x20, 0x10, 0x0, 0x2};/*limit 10bit YUV422 black*/
	unsigned char yuv422_10f[5] = {0x0, 0x20, 0x0, 0x0, 0x2};/*full 10bit YUV422 black*/
	unsigned char yuv422_8l[4] = {0x80, 0x10, 0x80, 0x10};/*limit 8bit YUV422 black*/
	unsigned char yuv422_8f[4] = {0x80, 0x0, 0x80, 0x0};/*full 8bit YUV422 black*/
	unsigned char byt_1[40] = {0};
	unsigned char rgb_8f[3] = {0};/*full 8bit rgb black*/
	unsigned char rgb_8l[3] = {0x10, 0x10, 0x10};/*limit 8bit rgb black*/
	unsigned int yuv422_10l_len = ARRAY_SIZE(yuv422_10l);
	unsigned int yuv422_8l_len = ARRAY_SIZE(yuv422_8l);
	unsigned int rgb8_len = ARRAY_SIZE(rgb_8l);
	unsigned int byt1_len = ARRAY_SIZE(byt_1);
	u32 val;

	if (!(devp->vdin_function_sel & VDIN_MEM_MEMSET_EN))
		return;
	if (!(vdin_is_convert_to_422(devp->format_convert) ||
		vdin_is_convert_to_rgb(devp->format_convert)) ||
		devp->hw_core == VDIN_HW_CORE_NORMAL) {
		return;
	}
	if (devp->debug.vdin_memset_en != 1) {
		memcpy(devp->debug.yuv422_8f, yuv422_8f, yuv422_8l_len);
		memcpy(devp->debug.yuv422_8l, yuv422_8l, yuv422_8l_len);
		memcpy(devp->debug.yuv422_10f, yuv422_10f, yuv422_10l_len);
		memcpy(devp->debug.yuv422_10l, yuv422_10l, yuv422_10l_len);
		memcpy(devp->debug.rgb_8f, rgb_8f, rgb8_len);
		memcpy(devp->debug.rgb_8l, rgb_8l, rgb8_len);
	}
	val = vdin_matrix_range_chk(devp);
	for (i = 0; i < devp->canvas_max_num; i++) {
		if (devp->cma_config_flag & 0x100)
			highmem_flag =
				PageHighMem(phys_to_page(devp->vf_mem_start[0]));
		else
			highmem_flag = PageHighMem(phys_to_page(devp->mem_start));
		count = devp->canvas_h;
		span = devp->canvas_active_w;
		width = devp->canvas_w;
		frame_size = count * width;
		if (highmem_flag)
			buf = vdin_vmap(devp->vf_mem_start[i], frame_size);
		else
			buf = phys_to_virt(devp->vf_mem_start[i]);
		if (!buf)
			return;
		p = (unsigned char *)buf;
		for (h = 0; h < count; h++) {
			buf_1 = buf + h  * width;
			ptr = (unsigned char *)buf_1;
			if (vdin_is_convert_to_422(devp->format_convert) &&
				devp->source_bitdepth == VDIN_COLOR_DEEPS_8BIT) {
				for (j = 0; j < span; j += yuv422_8l_len) {
					len = min(yuv422_8l_len, span - j);
					if (val)
						memcpy(ptr, devp->debug.yuv422_8f, len);
					else
						memcpy(ptr, devp->debug.yuv422_8l, len);
					ptr += len;
				}
			} else if (vdin_is_convert_to_rgb(devp->format_convert) &&
				devp->source_bitdepth == VDIN_COLOR_DEEPS_8BIT) {
				for (j = 0; j < span; j += rgb8_len) {
					len = min(rgb8_len, span - j);
					if (val)
						memcpy(ptr, devp->debug.rgb_8f, len);
					else
						memcpy(ptr, devp->debug.rgb_8l, len);
					ptr += len;
				}
			} else if (vdin_is_convert_to_422(devp->format_convert) &&
				devp->source_bitdepth == VDIN_COLOR_DEEPS_10BIT) {
				while (j < 40) {
					if (j > 0 && j % 8 == 0)
						cnt--;
					byt_index = cnt % yuv422_10l_len;
					if (val)
						byt_1[j] = devp->debug.yuv422_10f[byt_index];
					else
						byt_1[j] = devp->debug.yuv422_10l[byt_index];
					j++;
					cnt++;
				}
				for (j = 0; j < span; j += byt1_len) {
					len = min(byt1_len, span - j);
					memcpy(ptr, byt_1, len);
					ptr += len;
				}
			}
			if (devp->debug.vdin_memset_dbg_en == 1 && p) {
				pr_info("buf[%d], 8 byte\n", i);
				for (j = 0; j < 10; j++)
					pr_info("0x%02X ", p[j]);
				pr_info("\n");
				devp->debug.vdin_memset_dbg_en = 0;
			}
		}
		vdin_dma_flush(devp, buf, frame_size, DMA_TO_DEVICE);
		if (highmem_flag)
			vdin_unmap_phyaddr(buf);
		if (devp->debug.vdin_dbg_en & 0x10)
			pr_info("vdin%d,buf[%d] mem_start = 0x%lx, mem_size = 0x%x, highmem_flag = %d, color_range = %d\n",
				devp->index, i, devp->vf_mem_start[i],
				devp->frame_size, highmem_flag, val);
	}
}

/* need to be static for pointer use in codec_mm */
static char vdin_name[6];

int vdin_alloc_table_mem(struct vdin_dev_s *devp)
{
	bool task_pending = false;
	int i, j;
	int flags = CODEC_MM_FLAGS_CMA_FIRST | CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_DMA;
	unsigned int table_size;
	struct codec_mm_s *mem = NULL;

	table_size = devp->afbce_info->frame_table_size;

	if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
		for (i = 0; i < devp->vf_mem_max_cnt; i++) {
			/*add for 1g config, codec can't release mem in time*/
			for (j = 0; j < 20; j++) {
				if (fatal_signal_pending(current)) {
					task_pending = true;
					break;
				}
				mem = codec_mm_alloc(devp->vdin_name,
					table_size, 16, flags, -1);
				if (mem) {
					devp->afbce_info->fm_table_paddr[i] = mem->phy_addr;
				} else {
					devp->afbce_info->fm_table_paddr[i] = 0;
					//devp->afbce_info->fm_head_paddr[i] = 0;
				}

				if (!devp->afbce_info->fm_table_paddr[i]) {
					msleep(50);
					pr_err("alloc mem[%d] fail:50*%dms\n", i, j);
				} else {
					devp->vf_codec_mem_table[i] = mem;
					break;
				}
			}
			if (j >= 20 || task_pending) {
				pr_err("vdin%d buf[%d]codec alloc fail!!!\n",
					devp->index, i);
				//todo ;release the already alloced buffers
				break;
			}
		}
	}

	return 0;
}

/* return val:1: fail;0: ok */
/* combined canvas and afbce memory */
unsigned int vdin_cma_alloc(struct vdin_dev_s *devp)
{
	unsigned int mem_size, frame_size;
	int flags = CODEC_MM_FLAGS_CMA_FIRST | CODEC_MM_FLAGS_CMA_CLEAR |
		CODEC_MM_FLAGS_DMA;
	unsigned int max_buffer_num = devp->dts_config.min_buf_num;
	unsigned int i, j;
	int ret = 0;
	bool task_pending = false;
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
	unsigned int res = 0;
#endif
	struct codec_mm_s *mem = NULL;

	if (devp->cma_config_en == 0 || devp->cma_mem_alloc == 1) {
		pr_info("vdin%d use_reserved mem or cma already allocated (%d,%d)!!!\n",
			devp->index, devp->cma_config_en,
			devp->cma_mem_alloc);
		return 0;
	}

	count_vdin_req_mem(devp);
	mem_size = devp->mem_size_bak;
	max_buffer_num = devp->canvas_max_num_bak;
	frame_size = devp->frame_size_bak;

	if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC)
		snprintf(vdin_name, sizeof(vdin_name), "vdin%d", devp->index);

	if (devp->set_canvas_manual || devp->cfg_dma_buf) {
		for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
			if (vdin_set_canvas_addr[i].dma_buffer == 0)
				break;
			vdin_set_canvas_addr[i].paddr =
				roundup(vdin_set_canvas_addr[i].paddr,
					devp->canvas_align);
			if (devp->cfg_dma_buf)
				devp->mem_start = vdin_set_canvas_addr[i].paddr;
			devp->vf_mem_start[i] = vdin_set_canvas_addr[i].paddr;
			if (devp->debug.vdin_dbg_en)
				pr_info("vdin%d buf[%d] mem_start = 0x%lx, mem_size = 0x%x\n",
					devp->index, i,
					devp->vf_mem_start[i],
					vdin_set_canvas_addr[i].size);
		}
		/*real buffer number*/
		max_buffer_num = i;
		devp->canvas_max_num = max_buffer_num;
		devp->vf_mem_max_cnt = max_buffer_num;
		/*update to real total frames size*/
		if (i > 0)
			mem_size =
				vdin_set_canvas_addr[0].size * max_buffer_num;
		devp->mem_size = mem_size;
		devp->cma_mem_alloc = 1;
		pr_info("vdin%d canvas alloc %d buffers ok! (keystone:%d, drm:%d)\n",
			devp->index, devp->vf_mem_max_cnt,
			devp->set_canvas_manual, devp->cfg_dma_buf);
		return 0;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	vdin_get_secure_state(devp);
#endif

	if (devp->work_mode == VDIN_WORK_MD_V4L) {
		max_buffer_num       = devp->v4l2_req_buf_num;
		devp->canvas_max_num = devp->v4l2_req_buf_num;
		devp->vf_mem_max_cnt  = devp->v4l2_req_buf_num;
		pr_info("vdin%d v4l2 max_buffer_num = %d!\n",
			devp->index, devp->vf_mem_max_cnt);
	#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
		if (devp->secure_video && devp->mem_registered) {
			devp->secure_mem_size = devp->secure_mem_size * devp->v4l2_req_buf_num;
			res = tee_protect_mem_by_type(devp->secure_type,
							devp->secure_mem_start,
							devp->secure_mem_size,
							&devp->secure_handle);
			if (res)
				devp->mem_protected = 0;
			else
				devp->mem_protected = 1;
			if (devp->debug.vdin_dbg_en)
				pr_info("vdin%d v4l2 secure mem protect: %d (addr:[0x%lx,0x%x]\n",
					devp->index, devp->mem_protected,
					devp->secure_mem_start, devp->secure_mem_size);
		}
	#endif
		return 0;
	}

	if (devp->secure_en == VDIN_SECURE_FLAGS) {
		devp->mem_protected = 1;
		if (devp->afbce_valid)
			vdin_alloc_table_mem(devp);

		flags = CODEC_MM_FLAGS_TVP | CODEC_MM_FLAGS_CMA_CLEAR |
			CODEC_MM_FLAGS_DMA;
		pr_info("vdin%d flags = %#x!\n", devp->index, flags);
	}

	if (devp->cma_config_flag & MEM_ALLOC_DISCRETE) {
		for (i = 0; i < max_buffer_num; i++) {
			if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
				/*add for 1g config, codec can't release mem in time*/
				for (j = 0; j < 20; j++) {
					if (fatal_signal_pending(current)) {
						task_pending = true;
						break;
					}
					mem = codec_mm_alloc(vdin_name,
						(frame_size / PAGE_SIZE) << PAGE_SHIFT, 16, flags,
							-1);
					if (mem)
						devp->vf_mem_start[i] = mem->phy_addr;
					else
						devp->vf_mem_start[i] = 0;

					if (devp->vf_mem_start[i] == 0) {
						msleep(50);
						pr_err("alloc mem fail:50*%dms\n", j);
					} else {
						devp->vf_codec_mem[i] = mem;
						break;
					}
				}

				if (j >= 20 || task_pending) {
					pr_err("vdin%d buf[%d]codec alloc fail! try:%d, task:%d\n",
					       devp->index, i, j, task_pending);
					/*real buffer number*/
					max_buffer_num = i;
					devp->canvas_max_num = i;
					devp->vf_mem_max_cnt = i;
					/*update to real total frames size*/
					mem_size = PAGE_ALIGN(frame_size) * max_buffer_num;
					break;
				}
			} else {
				devp->vf_venc_pages[i] =
					dma_alloc_from_contiguous(&devp->this_pdev->dev,
								  frame_size >> PAGE_SHIFT, 0, 0);
				if (!devp->vf_venc_pages[i]) {
					pr_err("vdin%d cma mem undefined2.\n", devp->index);
					/*real buffer number*/
					max_buffer_num = i;
					devp->canvas_max_num = i;
					devp->vf_mem_max_cnt = i;
					/*update to real total frames size*/
					mem_size = PAGE_ALIGN(frame_size) * max_buffer_num;
					break;
				}
				devp->vf_mem_start[i] = page_to_phys(devp->vf_venc_pages[i]);
			}
			if (devp->debug.vdin_dbg_en)
				pr_info("vdin%d buf[%d] mem_start = 0x%lx, mem_size = 0x%x\n",
					devp->index, i,	devp->vf_mem_start[i], frame_size);
		}
	} else {
		if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
			devp->mem_start =
				codec_mm_alloc_for_dma(vdin_name, mem_size / PAGE_SIZE, 16, flags);

			if (devp->mem_start == 0) {
				ret = -2;
				devp->cma_mem_alloc = 0;
				goto mem_alloc_error;
			}
		} else {
			devp->venc_pages =
				dma_alloc_from_contiguous(&devp->this_pdev->dev,
							  mem_size >> PAGE_SHIFT, 0, 0);
			if (!devp->venc_pages) {
				ret = -3;
				devp->cma_mem_alloc = 0;
				goto mem_alloc_error;
			}
			devp->mem_start = page_to_phys(devp->venc_pages);
		}

#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
		if (devp->secure_video && devp->support_secure) {
			devp->secure_handle = 0;
			res = tee_protect_mem_by_type(devp->secure_type,
						      devp->mem_start,
						      mem_size,
						      &devp->secure_handle);
			if (res)
				devp->mem_protected = 0;
			else
				devp->mem_protected = 1;
			if (devp->debug.vdin_dbg_en)
				pr_info("vdin%d secure mem protect: %d (addr:0x%lx, size:0x%x)\n",
					devp->index, devp->mem_protected,
					devp->mem_start, mem_size);
		}
#endif
		for (i = 0; i < max_buffer_num; i++) {
			devp->vf_mem_start[i] = devp->mem_start + frame_size * i;

			if (devp->debug.vdin_dbg_en)
				pr_info("vdin%d buf[%d] mem_start = 0x%lx, mem_size = 0x%x\n",
					devp->index, i,
					devp->vf_mem_start[i], frame_size);
		}

		pr_info("vdin%d mem_start = 0x%lx, mem_size = 0x%x\n",
			devp->index, devp->mem_start, mem_size);
	}

	devp->mem_size = mem_size;
	devp->cma_mem_alloc = 1;

	if (devp->afbce_info && devp->afbce_valid) {
		for (i = 0; i < max_buffer_num; i++) {
			if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
				devp->afbce_info->fm_body_paddr[i] = 0;
				devp->afbce_info->fm_head_paddr[i] =
					devp->vf_mem_start[i] + devp->vf_mem_size_small;
			} else {
				devp->afbce_info->fm_body_paddr[i] =
					devp->vf_mem_start[i] + devp->vf_mem_size_small;
				devp->afbce_info->fm_head_paddr[i] =
					devp->vf_mem_start[i] + devp->vf_mem_size_small +
					devp->afbce_info->frame_body_size;
			}
			if (devp->secure_en != VDIN_SECURE_FLAGS) {
				devp->afbce_info->fm_table_paddr[i] =
					devp->afbce_info->fm_head_paddr[i] +
					devp->afbce_info->frame_head_size;
			}

			if (devp->debug.vdin_dbg_en) {
				pr_info("vdin%d fm_head_paddr[%d] = 0x%lx, frame_head_size = 0x%x,y:0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_head_paddr[i],
					devp->afbce_info->frame_head_size,
					devp->afbce_info->frame_head_size_y);
				pr_info("vdin%d fm_table_paddr[%d]=0x%lx, frame_table_size = 0x%x,y:0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_table_paddr[i],
					devp->afbce_info->frame_table_size,
					devp->afbce_info->frame_table_size_y);
				pr_info("vdin%d fm_body_paddr[%d]=0x%lx, frame_body_size = 0x%x,y:0x%x\n",
					devp->index, i,
					devp->afbce_info->fm_body_paddr[i],
					devp->afbce_info->frame_body_size,
					devp->afbce_info->frame_body_size_y);
			}
		}
	}

	if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
		//table
		int highmem_flag = 0;
		unsigned long *table;
		//void *buf_table = NULL;
		if (devp->afbce_info) {
			table = &devp->afbce_info->fm_table_paddr[0];
		} else {
			ret = -4;
			goto mem_alloc_error;
		}
		highmem_flag = PageHighMem(phys_to_page(table[0]));
		if (highmem_flag == 0) {
			/*low mem area*/
			pr_info("low mem area,flag:%#x\n", devp->cma_config_flag);
			if ((devp->cma_config_flag & 0xffff) == 0x101) {
				for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
					devp->afbce_info->fm_table_vir_paddr[i] =
						codec_mm_phys_to_virt(table[i]);
					//pr_info("table_paddr=0x%lx\n", (table[i]));
				}
			} else if ((devp->cma_config_flag & 0xffff) == 0) {
				for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
					devp->afbce_info->fm_table_vir_paddr[i] =
						phys_to_virt(table[i]);
					//pr_info("table_paddr=0x%lx\n", (table[0]));
				}
			}
		} else {
			/*high mem area*/
			pr_info("high mem area\n");
			for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
				devp->afbce_info->fm_table_vir_paddr[i] =
					vdin_vmap(table[i],
					      devp->afbce_info->frame_table_size);
				//pr_info("table_paddr=0x%lx\n", (table[0]));
			}
		}
	}

	pr_info("vdin%d cma alloc %d buffers ok!\n", devp->index,
		devp->vf_mem_max_cnt);

	if (devp->vf_mem_max_cnt < devp->dts_config.min_buf_num && !devp->is_one_buffer) {
		ret = -6;
		vdin_cma_release(devp);
		goto mem_alloc_error;
	}

	return 0;
mem_alloc_error:
	pr_err("vdin%d mem alloc error:%d", devp->index, ret);
	return 1;
}

/*this function used for codec cma release
 *	1.call codec_mm_free_for_dma() or
 *	  dma_release_from_contiguous() to release cma;
 *	2.reset mem_start & mem_size & cma_mem_alloc to 0;
 */
void vdin_cma_release(struct vdin_dev_s *devp)
{
	char vdin_name[6];
	vdin_name[0] = '\0';
	unsigned int i;

	if (devp->cma_config_en == 0 || devp->cma_mem_alloc == 0) {
		if (devp->work_mode == VDIN_WORK_MD_V4L) {
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
			if (devp->mem_protected) {
				tee_unprotect_mem(devp->secure_handle);
				devp->mem_protected = 0;
				if (devp->debug.vdin_dbg_en)
					pr_info("vdin%d v4l2 secure mem unprotect\n",
						devp->index);
			}
#endif
		} else {
			pr_err("vdin%d fail for (%d,%d)!!!\n",
				devp->index, devp->cma_config_en,
				devp->cma_mem_alloc);
		}
		return;
	}

	if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
		if (devp->index == 0)
			strcpy(vdin_name, "vdin0");
		else if (devp->index == 1)
			strcpy(vdin_name, "vdin1");
	}

	if (devp->cma_config_flag & MEM_ALLOC_DISCRETE) {
		if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
			/* canvas or afbce paddr */
			for (i = 0; i < devp->vf_mem_max_cnt; i++) {
				codec_mm_release(devp->vf_codec_mem[i], vdin_name);
				devp->vf_codec_mem[i] = NULL;
				if (devp->secure_en == VDIN_SECURE_FLAGS) {
					codec_mm_release(devp->vf_codec_mem_table[i], vdin_name);
					devp->vf_codec_mem_table[i] = NULL;
				}
			}
			pr_info("vdin%d codec cma release ok!\n", devp->index);
		} else {
			for (i = 0; i < devp->vf_mem_max_cnt; i++)
				dma_release_from_contiguous(&devp->this_pdev->dev,
							    devp->vf_venc_pages[i],
							    devp->frame_size >> PAGE_SHIFT);

			pr_info("vdin%d cma release ok!\n", devp->index);
		}
	} else {
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
		if (devp->mem_protected) {
			tee_unprotect_mem(devp->secure_handle);
			devp->mem_protected = 0;
			if (devp->debug.vdin_dbg_en)
				pr_info("vdin%d secure mem unprotect\n", devp->index);
		}
#endif
		if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC) {
			codec_mm_free_for_dma(vdin_name, devp->mem_start);
			pr_info("vdin%d codec cma release ok!\n", devp->index);
		} else {
			if (devp->venc_pages && devp->mem_size)
				dma_release_from_contiguous(&devp->this_pdev->dev,
							    devp->venc_pages,
							    devp->mem_size >> PAGE_SHIFT);

			pr_info("vdin%d cma release ok!\n", devp->index);
		}
	}
	if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
		//table
		int highmem_flag = 0;
		//void *buf_table = NULL;
		highmem_flag = PageHighMem(phys_to_page(devp->afbce_info->fm_table_paddr[0]));
		if (highmem_flag) {
			/*high mem area*/
			for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++) {
				if (devp->afbce_info->fm_table_vir_paddr[i])
					vdin_unmap_phyaddr(devp->afbce_info->fm_table_vir_paddr[i]);
				devp->afbce_info->fm_table_vir_paddr[i] = NULL;
			}
		}
	}

	devp->mem_start = 0;
	devp->mem_size = 0;
	devp->cma_mem_alloc = 0;
}

void vdin_set_mem_protect(struct vdin_dev_s *devp, unsigned int protect)
{
#if IS_ENABLED(CONFIG_AMLOGIC_TEE)
	unsigned int res = 0;

	if (protect) {
		res = tee_protect_mem_by_type(TEE_MEM_TYPE_VDIN,
					      devp->mem_start,
					      devp->mem_size,
					      &devp->secure_handle);
		if (res)
			devp->mem_protected = 0;
		else
			devp->mem_protected = 1;
	} else {
		tee_unprotect_mem(devp->secure_handle);
		devp->mem_protected = 0;
	}
	if (devp->debug.vdin_isr_monitor & VDIN_ISR_MONITOR_SECURE)
		pr_info("vdin%d secure mem protect:%d\n",
			devp->index, devp->mem_protected);
#endif
}

/*@20170823 new add for the case of csc change after signal stable*/
void vdin_cma_malloc_mode(struct vdin_dev_s *devp)
{
	unsigned int h_size, v_size, cma_mem_mode_flag = 0;

	h_size = devp->h_active;
	v_size = devp->v_active;

	switch (devp->format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		if (devp->force_malloc_yuv_422_to_444 && devp->cma_mem_mode)
			cma_mem_mode_flag = 1;
		break;
	default:
		if (devp->cma_mem_mode)
			cma_mem_mode_flag = 1;
		break;
	}

	if (cma_mem_mode_flag == 0)
		return;

	if (h_size <= VDIN_YUV444_MAX_CMA_WIDTH &&
	    v_size <= VDIN_YUV444_MAX_CMA_HEIGHT)
		devp->force_yuv444_malloc = 1;
	else
		devp->force_yuv444_malloc = 0;

	if (devp->work_mode == VDIN_WORK_MD_V4L) {
		pr_info("vdin v4l2 mode,set force_yuv444_malloc to 0\n");
		devp->force_yuv444_malloc = 0;
	}
}
#endif
