// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/types.h>
#include <uapi/linux/sched/types.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vfm/vframe.h>

#include "videodisplay_process.h"
#include "videodisplay_interface.h"

int video_display_control(int layer_index, int is_enable)
{
	return vd_set_enable(layer_index, is_enable);
}
EXPORT_SYMBOL(video_display_control);

int video_display_setframe(int layer_index, struct video_display_frame_info_t *frame_info,
			int flags)
{
	struct frames_info_t frames_info;

	memset(&frames_info, 0, sizeof(struct frames_info_t));
	frames_info.frame_count = 1;
	frames_info.layer_index = layer_index;
	frames_info.disp_zorder = frame_info->zorder;
	frames_info.frame_info[layer_index].dmabuf = frame_info->dmabuf;
	frames_info.frame_info[layer_index].input_fence = frame_info->input_fence;
	frames_info.frame_info[layer_index].release_fence = frame_info->release_fence;
	frames_info.frame_info[layer_index].present_fence = frame_info->present_fence;
	frames_info.frame_info[layer_index].phy_addr[0] = frame_info->phy_addr[0];
	frames_info.frame_info[layer_index].phy_addr[1] = frame_info->phy_addr[1];
	frames_info.frame_info[layer_index].byte_stride = frame_info->byte_stride;
	frames_info.frame_info[layer_index].buffer_w = frame_info->buffer_w;
	frames_info.frame_info[layer_index].buffer_h = frame_info->buffer_h;
	frames_info.frame_info[layer_index].dst_x = frame_info->dst_x;
	frames_info.frame_info[layer_index].dst_y = frame_info->dst_y;
	frames_info.frame_info[layer_index].dst_w = frame_info->dst_w;
	frames_info.frame_info[layer_index].dst_h = frame_info->dst_h;
	frames_info.frame_info[layer_index].crop_x = frame_info->crop_x;
	frames_info.frame_info[layer_index].crop_y = frame_info->crop_y;
	frames_info.frame_info[layer_index].crop_w = frame_info->crop_w;
	frames_info.frame_info[layer_index].crop_h = frame_info->crop_h;
	frames_info.frame_info[layer_index].zorder = frame_info->zorder;
	frames_info.frame_info[layer_index].signal_fmt = frame_info->signal_fmt;
	frames_info.frame_info[layer_index].type = frame_info->type;
	frames_info.frame_info[layer_index].bitdepth = frame_info->bitdepth;
	frames_info.frame_info[layer_index].rotation = frame_info->rotation;
	frames_info.frame_info[layer_index].private_flag = frame_info->private_flag;
	return vd_set_frames(layer_index, &frames_info);
}
EXPORT_SYMBOL(video_display_setframe);

int mbd_video_display_setframe(int layer_index, struct mbd_video_frame_info_t *frame_info,
			int flags)
{
	return 0;
}
EXPORT_SYMBOL(mbd_video_display_setframe);

void vsync_notify_videodisplay(u8 layer_id, u32 vpp_vsync_pts_inc_scale,
			u32 vpp_vsync_pts_inc_scale_base)
{
	return vd_vsync_notify(layer_id, vpp_vsync_pts_inc_scale, vpp_vsync_pts_inc_scale_base);
}
EXPORT_SYMBOL(vsync_notify_videodisplay);

int mediasync_set_callback(struct mediasync_cb *func)
{
	return vd_set_mediasync_callback(func);
}
EXPORT_SYMBOL(mediasync_set_callback);
