// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vfm/vframe.h>
#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
#include <video_processor/videodisplay/videodisplay_interface.h>
#endif
#include "meson_vpu_pipeline.h"
#include "meson_crtc.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"
#include "meson_drv.h"
#include "meson_plane.h"

static u32 video_bpp_get(u32 pixel_format)
{
	u32 bpp = 8;

	switch (pixel_format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
		bpp = 8;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		bpp = 32;
		break;
	case DRM_FORMAT_VUY888:
		bpp = 24;
		break;
	default:
		DRM_INFO("no support pixel format:0x%x, set default bpp 8\n", pixel_format);
		break;
	}
	return bpp;
}

static void video_vfm_convert_to_vfminfo(struct meson_vpu_video_state *mvvs,
	struct video_display_frame_info_t *vf_info)
{
	vf_info->dmabuf[0] = mvvs->dmabuf[0];
	vf_info->dmabuf[1] = mvvs->dmabuf[1];
	vf_info->dst_x = mvvs->dst_x;
	vf_info->dst_y = mvvs->dst_y;
	vf_info->dst_w = mvvs->dst_w;
	vf_info->dst_h = mvvs->dst_h;
	vf_info->crop_x = mvvs->src_x;
	vf_info->crop_y = mvvs->src_y;
	vf_info->crop_w = mvvs->src_w;
	vf_info->crop_h = mvvs->src_h;
	vf_info->buffer_w = mvvs->fb_w;
	vf_info->buffer_h = mvvs->fb_h;
	vf_info->zorder = mvvs->zorder;
	vf_info->byte_stride = mvvs->byte_stride;
	vf_info->signal_fmt = mvvs->signal_fmt;
	vf_info->private_flag = mvvs->frame_flag;
	vf_info->rotation = mvvs->rotation;
}

static u32 video_type_get(u32 pixel_format)
{
	u32 vframe_type = 0;

	switch (pixel_format) {
	case DRM_FORMAT_NV12:
		vframe_type = VIDTYPE_VIU_NV12 | VIDTYPE_VIU_FIELD |
				VIDTYPE_PROGRESSIVE;
		break;
	case DRM_FORMAT_NV21:
		vframe_type = VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD |
				VIDTYPE_PROGRESSIVE;
		break;
	case DRM_FORMAT_NV16:
		vframe_type = VIDTYPE_VIU_NV16 | VIDTYPE_VIU_FIELD |
				VIDTYPE_PROGRESSIVE;
		break;
	case DRM_FORMAT_NV61:
		vframe_type = VIDTYPE_VIU_NV61 | VIDTYPE_VIU_FIELD |
				VIDTYPE_PROGRESSIVE;
		break;
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
		vframe_type = VIDTYPE_VIU_422 | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_SINGLE_PLANE;
		break;
	case DRM_FORMAT_VUY888:
		vframe_type = VIDTYPE_VIU_444 | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_SINGLE_PLANE;
		break;
	case DRM_FORMAT_YUVX1010102:
		vframe_type = VIDTYPE_VIU_444 | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_SINGLE_PLANE;
		break;
	default:
		DRM_INFO("no support pixel format:0x%x\n", pixel_format);
		break;
	}
	return vframe_type;
}

static u32 video_bitdepth_get(u32 pixel_format)
{
	u32 bitdepth_type = 0;

	switch (pixel_format) {
	case DRM_FORMAT_NV12:
	case DRM_FORMAT_NV21:
	case DRM_FORMAT_YUYV:
	case DRM_FORMAT_YVYU:
	case DRM_FORMAT_UYVY:
	case DRM_FORMAT_VYUY:
	case DRM_FORMAT_VUY888:
	case DRM_FORMAT_NV16:
	case DRM_FORMAT_NV61:
		bitdepth_type = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		break;
	case DRM_FORMAT_YUVX1010102:
		bitdepth_type = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
		break;
	default:
		bitdepth_type = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		DRM_INFO("no support pixel format:0x%x\n", pixel_format);
		break;
	}
	return bitdepth_type;
}

static u32 video_signal_fmt_cov(u32 signal_fmt)
{
	u32 vframe_signal_fmt;

	switch (signal_fmt) {
	case SIGNAL_FMT_SDR:
		vframe_signal_fmt = VFRAME_SIGNAL_FMT_SDR;
		break;
	case SIGNAL_FMT_HDR10:
		vframe_signal_fmt = VFRAME_SIGNAL_FMT_HDR10;
		break;
	case SIGNAL_FMT_HDR10PRIME:
		vframe_signal_fmt = VFRAME_SIGNAL_FMT_HDR10PRIME;
		break;
	case SIGNAL_FMT_CUVA_HDR:
		vframe_signal_fmt = VFRAME_SIGNAL_FMT_HDR10PRIME;
		break;
	default:
		vframe_signal_fmt = VFRAME_SIGNAL_FMT_SDR;
		DRM_INFO("%s default format:0x%x\n", __func__, vframe_signal_fmt);
		break;
	}

	return vframe_signal_fmt;
}

/*background data R[32:47] G[16:31] B[0:15] alpha[48:63]->
 *dummy data Y[16:23] Cb[8:15] Cr[0:7]
 */
void video_dummy_data_set(u64 crtc_bgcolor, bool crtc_bgcolor_flag)
{
	int r, g, b, alpha, y, u, v;
	u32 vpp_index = 0;

	if (!crtc_bgcolor_flag)
		return;

	b = (crtc_bgcolor & 0xffff) / 256;
	g = ((crtc_bgcolor >> 16) & 0xffff) / 256;
	r = ((crtc_bgcolor >> 32) & 0xffff) / 256;
	alpha = ((crtc_bgcolor >> 48) & 0xffff) / 256;

	y = ((47 * r + 157 * g + 16 * b + 128) >> 8) + 16;
	u = ((-26 * r - 87 * g + 113 * b + 128) >> 8) + 128;
	v = ((112 * r - 102 * g - 10 * b + 128) >> 8) + 128;

	set_post_blend_dummy_data(vpp_index, 1 << 24 | y << 16 | u << 8 | v, alpha);
}

static int video_check_state(struct meson_vpu_block *vblk,
			     struct meson_vpu_block_state *state,
		struct meson_video_sub_pipeline_state *mvps)
{
	struct meson_vpu_video_layer_info *plane_info;
	struct meson_vpu_video *video = to_video_block(vblk);
	struct meson_vpu_video_state *mvvs = to_video_state(state);
	struct meson_vpu_block_state *old_mvbs = priv_to_block_state(vblk->obj.state);
	struct meson_vpu_video_state *old_mvvs = to_video_state(old_mvbs);

	if (state->checked) {
		MESON_DRM_BLOCK("have checked done!\n");
		return 0;
	}

	state->checked = true;

	if (!mvvs || mvvs->plane_index >= MESON_MAX_VIDEO) {
		DRM_INFO("mvvs is NULL or plane_index invalid, mvvs = %p!\n", mvvs);
		return -1;
	}
	MESON_DRM_BLOCK("%s check_state called.\n", video->base.name);
	plane_info = &mvps->video_plane_info[vblk->index];
	mvvs->src_x = plane_info->src_x;
	mvvs->src_y = plane_info->src_y;
	mvvs->src_w = plane_info->src_w;
	mvvs->src_h = plane_info->src_h;
	mvvs->dst_x = plane_info->dst_x;
	mvvs->dst_y = plane_info->dst_y;
	mvvs->dst_w = plane_info->dst_w;
	mvvs->dst_h = plane_info->dst_h;
	mvvs->fb_w = plane_info->fb_w;
	mvvs->fb_h = plane_info->fb_h;
	mvvs->byte_stride = plane_info->byte_stride;
	mvvs->plane_index = plane_info->plane_index;
	mvvs->phy_addr[0] = plane_info->phy_addr[0];
	mvvs->phy_addr[1] = plane_info->phy_addr[1];
	mvvs->zorder = plane_info->zorder;
	mvvs->pixel_format = plane_info->pixel_format;
	mvvs->fb_size[0] = plane_info->fb_size[0];
	mvvs->fb_size[1] = plane_info->fb_size[1];
	mvvs->vf = plane_info->vf;
	mvvs->is_uvm = plane_info->is_uvm;
	mvvs->dmabuf[0] = plane_info->dmabuf[0];
	mvvs->dmabuf[1] = plane_info->dmabuf[1];
	mvvs->crtc_index = plane_info->crtc_index;
	mvvs->rotation = plane_info->rotation;
	mvvs->signal_fmt = video_signal_fmt_cov(plane_info->signal_fmt);
	mvvs->frame_flag = plane_info->frame_flag;
	mvvs->in_fence = plane_info->in_fence;

	MESON_DRM_BLOCK("video->dmabuf-[%px %px], plane_info->dmabuf-[%px %px]\n",
		video->dmabuf[0], video->dmabuf[1],
		plane_info->dmabuf[0], plane_info->dmabuf[1]);
	if (video->dmabuf[0] != plane_info->dmabuf[0] && plane_info->dmabuf[0]) {
		video->dmabuf[0] = plane_info->dmabuf[0];
		video->dmabuf[1] = plane_info->dmabuf[1];
	}

	MESON_DRM_BLOCK("mvvs->dmabuf-[%px %px], old_mvvs->dmabuf-[%px %px]\n",
		mvvs->dmabuf[0], mvvs->dmabuf[1], old_mvvs->dmabuf[0], old_mvvs->dmabuf[1]);
	if (mvvs->dmabuf[0] != old_mvvs->dmabuf[0])
		mvvs->repeat_frame = 0;
	else
		mvvs->repeat_frame = 1;

	if (mvvs->repeat_frame == 1)
		MESON_DRM_FENCE("check,video repeat frame! dma_buf-[%px %px]\n",
		mvvs->dmabuf[0], mvvs->dmabuf[1]);
	return 0;
}

static void video_set_state(struct meson_vpu_block *vblk,
			    struct meson_vpu_block_state *state,
			    struct meson_vpu_block_state *old_state,
			    struct meson_video_sub_pipeline_state *mvsps)
{
	struct meson_vpu_video *video = to_video_block(vblk);
	struct meson_vpu_video_state *mvvs = to_video_state(state);
	struct video_display_frame_info_t vf_info;
	int ret;
	struct vframe_s *vf = NULL, *dec_vf = NULL, *vfp = NULL;
	u32 pixel_format, src_h, byte_stride, pic_w, pic_h, fb_h;
	u32 new_src_w, new_src_h;
	u64 phy_addr, phy_addr2 = 0;
	u32 bpp;
	bool is_process_drm_vf = false;

	MESON_DRM_BLOCK("IN, fb[%dx%d], src[(%d,%d),%dx%d], dst[(%d,%d),%dx%d]\n",
		mvvs->fb_w, mvvs->fb_h,
		mvvs->src_x, mvvs->src_y, mvvs->src_w, mvvs->src_h,
		mvvs->dst_x, mvvs->dst_y, mvvs->dst_w, mvvs->dst_h);

	if (!vblk || !mvvs->dmabuf[0]) {
		DRM_ERROR("set_state break for NULL.vblk %p, dmabuf[0] %px\n",
			vblk, mvvs->dmabuf[0]);
		return;
	}

	memset(&vf_info, 0, sizeof(vf_info));
	src_h = mvvs->src_h;
	byte_stride = mvvs->byte_stride;
	fb_h = mvvs->fb_h;
	phy_addr = mvvs->phy_addr[0];
	phy_addr2 = mvvs->phy_addr[1];
	pixel_format = mvvs->pixel_format;

	MESON_DRM_BLOCK("is_uvm[%d], vf[%px], pixel_format[0x%x]\n",
		mvvs->is_uvm, mvvs->vf, mvvs->pixel_format);
	if (mvvs->is_uvm && mvvs->vf) {
		dec_vf = mvvs->vf;
		vf = mvvs->vf;
		MESON_DRM_BLOCK("dec vf, flag[0x%x], type[0x%x], comp[%u, %u][%u, %u]\n",
			  dec_vf->flag, dec_vf->type,
			  dec_vf->compWidth, dec_vf->compHeight,
			  dec_vf->width, dec_vf->height);
		if (mvvs->vf->vf_ext && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vf = mvvs->vf->vf_ext;
			MESON_DRM_BLOCK("DI vf, flag[0x%x], type[0x%x], comp[%u, %u][%u, %u]\n",
				  vf->flag, vf->type,
				  vf->compWidth, vf->compHeight,
				  vf->width, vf->height);
		}
		vf->axis[0] = mvvs->dst_x;
		vf->axis[1] = mvvs->dst_y;
		vf->axis[2] = mvvs->dst_x + mvvs->dst_w - 1;
		vf->axis[3] = mvvs->dst_y + mvvs->dst_h - 1;
		vf->crop[0] = mvvs->src_y;/*crop top*/
		vf->crop[1] = mvvs->src_x;/*crop left*/

		is_process_drm_vf = is_valid_mod_type(mvvs->dmabuf[0], PROCESS_DRM);
		if (!is_process_drm_vf) {
			new_src_w = mvvs->src_w;
			new_src_h = mvvs->src_h;
			if (vf->type & VIDTYPE_COMPRESS) {
				pic_w = vf->compWidth;
				pic_h = vf->compHeight;
			} else {
				pic_w = vf->width;
				pic_h = vf->height;
			}
		} else {
			bpp = video_bpp_get(mvvs->pixel_format);
			new_src_w = mvvs->src_w;
			new_src_h = mvvs->src_h;
			pic_w = mvvs->byte_stride * 8 / bpp;
			pic_h = mvvs->fb_h;

			/* overlay dec vframe parameters*/
			if (drm_rotation_90_or_270(mvvs->rotation))
				vf->ratio_control = 0;

			vf->type &= (~VIDTYPE_COMPRESS);
			vf->compWidth = 0;
			vf->compHeight = 0;
			vf->width = pic_w;
			vf->height = pic_h;
			vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
			vf->plane_num = 1;
			vf->canvas0Addr = -1;
			vf->canvas0_config[0].phy_addr = phy_addr;
			vf->canvas0_config[0].width = byte_stride;
			vf->canvas0_config[0].height = pic_h;
			vf->canvas0_config[0].endian = 0;
			vf->canvas1Addr = -1;

			if (pixel_format == DRM_FORMAT_NV12 ||
				pixel_format == DRM_FORMAT_NV21) {
				if (!phy_addr2)
					phy_addr2 = phy_addr + byte_stride * src_h;
				vf->plane_num = 2;
				vf->canvas0_config[1].phy_addr = phy_addr2;
				vf->canvas0_config[1].width = byte_stride;
				vf->canvas0_config[1].height = pic_h / 2;
				dec_vf->canvas0_config[1].block_mode =
					CANVAS_BLKMODE_LINEAR;
				/*big endian default support*/
				vf->canvas0_config[1].endian = 0;
				vf->plane_num = 2;
			}
			vf->type = video_type_get(pixel_format);
			vf->bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			MESON_DRM_BLOCK("rotation overlay, rotation:%d\n", mvvs->rotation);
		}

		if ((pic_w == 0 || pic_h == 0) && dec_vf->vf_ext) {
			vfp = dec_vf->vf_ext;
			pic_w = vfp->width;
			pic_h = vfp->height;
			MESON_DRM_BLOCK("vf_ext:pic_w = %d, pic_h = %d\n", pic_w, pic_h);
		}

		vf_info.release_fence = video->fence;
		vf_info.dmabuf[0] = mvvs->dmabuf[0];
		vf_info.dmabuf[1] = mvvs->dmabuf[1];
		vf_info.dst_x = mvvs->dst_x;
		vf_info.dst_y = mvvs->dst_y;
		vf_info.dst_w = mvvs->dst_w;
		vf_info.dst_h = mvvs->dst_h;
		vf_info.crop_x = vf->crop[1];
		vf_info.crop_y = vf->crop[0];
		vf_info.crop_w = new_src_w;
		vf_info.crop_h = new_src_h;
		vf_info.buffer_w = pic_w;
		vf_info.buffer_h = pic_h;
		vf_info.zorder = mvvs->zorder;
		vf_info.rotation = mvvs->rotation;
		vf_info.input_fence = mvvs->in_fence;
		vf_info.reserved[0] = 0;
		vf_info.phy_addr[0] = mvvs->phy_addr[0];
		vf_info.phy_addr[1] = mvvs->phy_addr[1];
		vf_info.private_flag = mvvs->frame_flag;

		if (mvvs->repeat_frame) {
			dma_fence_get(vf_info.release_fence);
		} else {
			/*
			 * The present fence indicates the buff's on-screen time.
			 * For repeat frames, since the buff has already been
			 * displayed on the screen in the previous frame,
			 * DRM does not process it.
			 */
			vf_info.present_fence = video->present_fence;
			dma_resv_lock(vf_info.dmabuf[0]->resv, NULL);

			ret = dma_resv_reserve_fences(vf_info.dmabuf[0]->resv, 1);
			if (!ret && vf_info.release_fence)
				dma_resv_add_fence(vf_info.dmabuf[0]->resv, vf_info.release_fence,
							DMA_RESV_USAGE_WRITE);

			dma_resv_unlock(vf_info.dmabuf[0]->resv);
			video->frame_cnt++;
		}

		MESON_DRM_FENCE("dmabuf(%px-%px), in-%px, release_fence(%px-%d)\n",
				vf_info.dmabuf[0], vf_info.dmabuf[1], vf_info.input_fence,
				vf_info.release_fence,
				vf_info.release_fence ?
				kref_read(&vf_info.release_fence->refcount) : -1);

		MESON_DRM_BLOCK("vf-info dst:%u,%u,%ux%u, crop:%u,%u,%ux%u, buffer:%ux%u\n",
				vf_info.dst_x, vf_info.dst_y, vf_info.dst_w, vf_info.dst_h,
				vf_info.crop_x, vf_info.crop_y, vf_info.crop_w, vf_info.crop_h,
				vf_info.buffer_w, vf_info.buffer_h);

#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
		video_display_setframe(vblk->index, &vf_info, 0);
#endif
	} else {
		vf_info.release_fence = video->fence;
		video_vfm_convert_to_vfminfo(mvvs, &vf_info);
		vf_info.phy_addr[0] = mvvs->phy_addr[0];
		if (pixel_format == DRM_FORMAT_NV12 ||
			pixel_format == DRM_FORMAT_NV21 ||
			pixel_format == DRM_FORMAT_NV16 ||
			pixel_format == DRM_FORMAT_NV61) {
			if (!mvvs->phy_addr[1])
				vf_info.phy_addr[1] = phy_addr + byte_stride * fb_h;
			else
				vf_info.phy_addr[1] = mvvs->phy_addr[1];
		}

		vf_info.type = video_type_get(pixel_format);
		vf_info.bitdepth = video_bitdepth_get(pixel_format);
		vf_info.input_fence = mvvs->in_fence;
		if (mvvs->repeat_frame) {
			dma_fence_get(vf_info.release_fence);
		} else {
			/*
			 * The present fence indicates the buff's on-screen time.
			 * For repeat frames, since the buff has already been
			 * displayed on the screen in the previous frame,
			 * DRM does not process it.
			 */
			vf_info.present_fence = video->present_fence;
			dma_resv_lock(vf_info.dmabuf[0]->resv, NULL);

			ret = dma_resv_reserve_fences(vf_info.dmabuf[0]->resv, 1);
			if (!ret && vf_info.release_fence)
				dma_resv_add_fence(vf_info.dmabuf[0]->resv, vf_info.release_fence,
							DMA_RESV_USAGE_WRITE);

			dma_resv_unlock(vf_info.dmabuf[0]->resv);
			video->frame_cnt++;
		}
		MESON_DRM_FENCE("dmabuf(%px-%px), in-%px, release_fence(%px-%d)\n",
				vf_info.dmabuf[0], vf_info.dmabuf[1], vf_info.input_fence,
				vf_info.release_fence,
				vf_info.release_fence ?
				kref_read(&vf_info.release_fence->refcount) : -1);

		MESON_DRM_BLOCK("vf-info dst:%u,%u,%ux%u,crop:%u,%u,%ux%u,buffer:%ux%u\n",
			vf_info.dst_x, vf_info.dst_y, vf_info.dst_w, vf_info.dst_h,
			vf_info.crop_x, vf_info.crop_y, vf_info.crop_w, vf_info.crop_h,
			vf_info.buffer_w, vf_info.buffer_h);
#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
		video_display_setframe(vblk->index, &vf_info, 0);
#endif
	}

	MESON_DRM_STATE("video%d (\"%s\" pid=%d]) state(0x%p)\n"
		"mvps:0x%p dmabuf:0x%px src[%d %d %d %d] dst[%d %d %d %d]\n",
		vblk->index, current->comm, task_pid_nr(current),
		old_state ? old_state->obj.state : state->obj.state, mvsps, mvvs->dmabuf[0],
		mvvs->src_x, mvvs->src_y, mvvs->src_w, mvvs->src_h,
		mvvs->dst_x, mvvs->dst_y, mvvs->dst_w, mvvs->dst_h);

	MESON_DRM_BLOCK("plane_index=%d,HW-video=%d,stride=%d,rotate=%d,zorder=%d,\n"
		"signal_fmt=%d,frame_flag=%u\n",
		  mvvs->plane_index, vblk->index, mvvs->byte_stride,
		  mvvs->rotation, mvvs->zorder, mvvs->signal_fmt, mvvs->frame_flag);
	MESON_DRM_BLOCK("phy_addr=(0x%llx,0x%llx),phy_addr2=(0x%llx,0x%llx),\n"
		"repeat_frame=%d,frame_cnt=%llu\n",
		  phy_addr, vf_info.phy_addr[0], phy_addr2, vf_info.phy_addr[1],
		  mvvs->repeat_frame, video->frame_cnt);
	MESON_DRM_BLOCK("%s set_state done.\n", video->base.name);
}

static void video_hw_enable(struct meson_vpu_block *vblk,
			    struct meson_vpu_block_state *state)
{
	struct meson_vpu_video *video = to_video_block(vblk);

	if (!video) {
		MESON_DRM_BLOCK("enable break for NULL.\n");
		return;
	}

	if (!video->video_enabled) {
		MESON_DRM_BLOCK("set %s enable.\n", video->base.name);
#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
		video_display_control(vblk->index, 1);
		video->video_enabled = 1;
#endif
	}

	MESON_DRM_BLOCK("%s enable done.\n", video->base.name);
}

static void video_disable_work_func(struct work_struct *works)
{
	struct meson_vpu_disable_work *worker = container_of(works,
		struct meson_vpu_disable_work, work);

#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
		video_display_control(worker->idx, 0);
#endif

	if (worker->dmabuf[0])
		dma_buf_put(worker->dmabuf[0]);

	if (worker->dmabuf[1])
		dma_buf_put(worker->dmabuf[1]);
	worker->video->video_enabled = 0;
	DRM_INFO("%s worker disable done.\n", worker->video->base.name);
	worker->dmabuf[0] = NULL;
	worker->dmabuf[1] = NULL;
}

static int video_disable_thread_func(void *data)
{
	struct meson_vpu_video *video = data;

	while (!kthread_should_stop()) {
		wait_event_interruptible(video->disable_wq,
		atomic_read(&video->work_pending) ||
		kthread_should_stop());

		if (atomic_read(&video->work_pending)) {
			video_disable_work_func(&video->worker.work);
			atomic_set(&video->work_pending, 0);
		}
	}
	return 0;
}

static void video_hw_disable(struct meson_vpu_block *vblk,
			     struct meson_video_sub_pipeline *sub_pipeline)
{
	struct meson_vpu_video *video = to_video_block(vblk);
	struct meson_vpu_disable_work *worker = &video->worker;
	struct meson_drm *priv;

	if (!video) {
		MESON_DRM_BLOCK("disable break for NULL.\n");
		return;
	}

	DRM_INFO("dmabuf(%px %px),release_fence(%px),present_fence(%px),\n"
		"video_name(%s),frame_cnt(%llu)\n",
		video->dmabuf[0], video->dmabuf[1], video->fence,
		video->present_fence, video->base.name, video->frame_cnt);
	video->frame_cnt = 0;
	priv = video->base.pipeline->priv;

	if (video->dmabuf[0])
		get_dma_buf(video->dmabuf[0]);

	if (video->dmabuf[1])
		get_dma_buf(video->dmabuf[1]);

	if (video->disable_thread && priv->of_conf.disable_thread_en) {
		worker->idx = vblk->index;
		worker->video = video;
		worker->dmabuf[0] = video->dmabuf[0];
		worker->dmabuf[1] = video->dmabuf[1];
		atomic_set(&video->work_pending, 1);
		wake_up(&video->disable_wq);
	} else {
#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
		video_display_control(vblk->index, 0);
#endif
		if (video->dmabuf[0])
			dma_buf_put(video->dmabuf[0]);

		if (video->dmabuf[1])
			dma_buf_put(video->dmabuf[1]);
		video->video_enabled = 0;
		DRM_INFO("%s disable done.\n", video->base.name);
	}

	video->fence = NULL;
	video->present_fence = NULL;
	video->dmabuf[0] = NULL;
	video->dmabuf[1] = NULL;
	priv->disable_video_plane = 1;

	MESON_DRM_BLOCK("%s disable done.\n", video->base.name);
}

static void video_dump_register(struct drm_printer *p,
				struct meson_vpu_block *vblk)
{
}

static void video_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_video *video = to_video_block(vblk);
	u32 w, h;

	if (!vblk || !video) {
		MESON_DRM_BLOCK("break for NULL.\n");
		return;
	}

	video->video_cap = video_get_layer_capability();

	get_video_src_min_buffer(vblk->index, &w, &h);
	video->src_min_size = w << 16 | h;

	get_video_src_max_buffer(vblk->index, &w, &h);
	video->src_max_size = w << 16 | h;

	init_waitqueue_head(&video->disable_wq);
	atomic_set(&video->work_pending, 0);
	video->disable_thread = kthread_create(video_disable_thread_func,
	video, "disable_kthread");

	if (!IS_ERR(video->disable_thread)) {
		set_user_nice(video->disable_thread, -20);
		wake_up_process(video->disable_thread);
	} else {
		video->disable_thread = NULL;
	}

	MESON_DRM_BLOCK("%s done\n", video->base.name);
}

static void video_hw_fini(struct meson_vpu_block *vblk)
{
	struct meson_vpu_video *video = to_video_block(vblk);

	if (video->disable_thread) {
		kthread_stop(video->disable_thread);
		video->disable_thread = NULL;
	}
	MESON_DRM_BLOCK("%s done.\n", video->base.name);
}

struct meson_vpu_block_ops video_ops = {
	.check_video_state = video_check_state,
	.update_video_state = video_set_state,
	.enable = video_hw_enable,
	.disable_video = video_hw_disable,
	.dump_register = video_dump_register,
	.init = video_hw_init,
	.fini = video_hw_fini,
};
