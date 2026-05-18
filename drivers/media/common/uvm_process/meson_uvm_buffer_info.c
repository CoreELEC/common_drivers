// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include "meson_uvm_buffer_info.h"

#define LOG_ERROR   1
#define LOG_WARNING 2
#define LOG_DEBUG   3

static int mubi_debug_level;
module_param(mubi_debug_level, int, 0644);
#define MUBI_PRINTK(level, fmt, arg...) \
	do { \
		if (mubi_debug_level >= (level)) \
			pr_info("uvm_buffer_info: [%s] " fmt, __func__, ## arg); \
	} while (0)

static bool is_dv_video(struct vframe_s *vfp)
{
	/* dolby vision: bit 30 */
	/*
	 *u32 dv_flag = (1 << 30);
	 *
	 *if (vfp->signal_type & dv_flag)
	 *return true;
	 */

	if (!vfp->discard_dv_data)
		return (is_amdv_frame(vfp) > 0) ? true : false;

	return false;
}

static bool is_secure_video(const struct vframe_s *vfp)
{
	if (vfp->flag & VFRAME_FLAG_VIDEO_SECURE)
		return true;

	return false;
}

static bool is_afbc_video(const struct vframe_s *vfp)
{
	if (vfp->compWidth > 0 && vfp->compHeight > 0)
		return true;

	return false;
}

static enum uvm_video_type get_format_vframe(const struct vframe_s *vfp)
{
	enum uvm_video_type type = AM_VIDEO_NORMAL;
	u32 cuva = ((vfp->signal_type >> 31) & 1);
	u32 color_primaries = ((vfp->signal_type >> 16) & 0xff);
	u32 transfer_characteristic = ((vfp->signal_type >> 8) & 0xff);

	if (vfp->src_fmt.sei_magic_code != SEI_MAGIC_CODE) {
		/* invalid src fmt case */
		if ((transfer_characteristic == 14 || transfer_characteristic == 18) &&
				color_primaries == 9) {
			type = AM_VIDEO_HLG;
		} else if ((transfer_characteristic == 16) &&
				((color_primaries == 9) || (color_primaries == 2))) {
			if (cuva)
				type = AM_VIDEO_HDR10_PLUS;
			else
				type = AM_VIDEO_HDR;
		}
	} else {
		if (vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HDR10)
			type = AM_VIDEO_HDR;
		else if (vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HDR10PLUS ||
				vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HDR10PRIME)
			type = AM_VIDEO_HDR10_PLUS;
		else if (vfp->src_fmt.fmt == VFRAME_SIGNAL_FMT_HLG)
			type = AM_VIDEO_HLG;
	}

	return type;
}

static enum uvm_video_type get_resolution_vframe(const struct vframe_s *vfp)
{
	u32 h, w;

	if ((vfp->type & VIDTYPE_COMPRESS) != 0) {
		w = vfp->compWidth;
		h = vfp->compHeight;
	} else {
		w = vfp->width;
		h = vfp->height;
	}

	if (h > 2304 || w > 4096)
		return AM_VIDEO_8K;
	else if (h > 1088 || w > 1920)
		return AM_VIDEO_4K;

	return AM_VIDEO_NORMAL;
}

static struct vframe_s *get_vf_from_fd(int fd)
{
	struct file *file_vf = NULL;
	struct vframe_s *vf = NULL;
	bool is_dec_vf = false;
	bool is_v4lvideo_fd = false;
	struct file_private_data *file_private_data = NULL;
	struct uvm_hook_mod *uhmod;

	file_vf = fget(fd);
	if (!file_vf) {
		MUBI_PRINTK(LOG_ERROR, "fget fd fail\n");
		goto exit_null;
	}

	MUBI_PRINTK(LOG_DEBUG, "%s fd:%d, buffer_file: %px\n", __func__, fd, file_vf);

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
	if (is_dec_vf) {
		vf = dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		if (vf) {
			MUBI_PRINTK(LOG_DEBUG, "vframe_type = 0x%x, vframe_flag = 0x%x.\n",
				vf->type, vf->flag);
			dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));
		} else {
			MUBI_PRINTK(LOG_ERROR, "vf is NULL.\n");
		}
	} else {
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
		if (is_v4lvideo_buf_file(file_vf))
			is_v4lvideo_fd = true;
#endif

		if (is_v4lvideo_fd) {
			file_private_data = (struct file_private_data *)(file_vf->private_data);
		} else {
			uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
						 VF_PROCESS_V4LVIDEO);
			if (!uhmod) {
				MUBI_PRINTK(LOG_ERROR, "uhmod is NULL\n");
				goto exit_null;
			}
			if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
				MUBI_PRINTK(LOG_ERROR, "file_private_data is NULL.\n");
				goto exit_null;
			}

			file_private_data = uhmod->arg;
			uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
					 VF_PROCESS_V4LVIDEO);
		}

		if (!file_private_data) {
			MUBI_PRINTK(LOG_ERROR, "invalid fd: no uvm, no v4lvideo!!\n");
			goto exit_null;
		} else {
			vf = &file_private_data->vf;
		}
	}

exit_null:
	if (file_vf)
		fput(file_vf);
	return vf;
}

int get_uvm_video_info(const int fd, struct uvm_fd_info *info)
{
	struct vframe_s *vfp;
	enum uvm_video_type type = AM_VIDEO_NORMAL;
	int ret = 0;

	vfp = get_vf_from_fd(fd);
	if (IS_ERR_OR_NULL(vfp)) {
		MUBI_PRINTK(LOG_ERROR, "vframe is null. maybe is DMA buffer\n");
		ret = -EINVAL;
		goto exit_ret;
	}

	if (is_dv_video(vfp))
		type |= AM_VIDEO_DV;

	if (is_secure_video(vfp))
		type |= AM_VIDEO_SECURE;

	if (is_afbc_video(vfp))
		type |= AM_VIDEO_AFBC;

	type |= get_format_vframe(vfp);
	type |= get_resolution_vframe(vfp);
	info->type = (int)type;

	if ((vfp->type & VIDTYPE_COMPRESS) != 0) {
		info->buf_width = vfp->compWidth;
		info->buf_height = vfp->compHeight;
		info->crop_right = info->buf_width;
		info->crop_bottom = info->buf_height;
		if (is_src_crop_valid(vfp->src_crop)) {
			info->crop_right -= vfp->src_crop.right;
			info->crop_bottom -= vfp->src_crop.bottom;
		}
	} else {
		info->buf_width = vfp->width;
		info->buf_height = vfp->height;
		info->crop_right = info->buf_width;
		info->crop_bottom = info->buf_height;
	}
	info->timestamp = vfp->timestamp;
	info->buf_stride = info->buf_width;
	info->priority = vfp->priority;
	if (vfp->source_type == VFRAME_SOURCE_TYPE_HDMI ||
			vfp->source_type == VFRAME_SOURCE_TYPE_CVBS ||
			vfp->source_type == VFRAME_SOURCE_TYPE_TUNER) {
		info->fps = -1;
	} else {
		info->fps = (s32)vfp->duration;
	}

	MUBI_PRINTK(LOG_DEBUG, "type:0x%x timestamp:%lld wxh:%dx%d crop:(%d %d %d %d)\n",
			info->type, info->timestamp, info->buf_width, info->buf_height,
			info->crop_left, info->crop_top, info->crop_right, info->crop_bottom);
exit_ret:
	return ret;
}
