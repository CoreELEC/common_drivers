/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEODISPLAY_INTERFACE_H
#define VIDEODISPLAY_INTERFACE_H

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/delay.h>
#include <linux/kfifo.h>
#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/video_processor/video_composer_ext.h>

struct video_display_frame_info_t {
	struct dma_buf *dmabuf[2];
	struct dma_fence *input_fence;
	struct dma_fence *release_fence;
	struct dma_fence *present_fence;
	u64 phy_addr[2];
	u32 byte_stride;
	u32 buffer_w;
	u32 buffer_h;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
	u32 crop_x;
	u32 crop_y;
	u32 crop_w;
	u32 crop_h;
	u32 zorder;
	u32 signal_fmt;
	u32 type;
	u32 bitdepth;
	u32 rotation;
	/*
	 * private_flag: for private parameters outside of the DRM framework, such as hwc
	 * bit0: is hwc create black frame
	 */
	u32 private_flag;
	u32 reserved[9];
};

struct mbp_buffer_info_t {
	u32 pool_handle;
	u32 user_id;
	u64 phys_addr[3];
};

struct mbd_video_frame_info_t {
	struct mbp_buffer_info_t *buffer_info;
	u32 type;
	u32 stride[3];
	u32 buffer_w;
	u32 buffer_h;
	u32 dst_x;
	u32 dst_y;
	u32 dst_w;
	u32 dst_h;
	u32 crop_x;
	u32 crop_y;
	u32 crop_w;
	u32 crop_h;
	u32 zorder;
	u32 timeref;
	u64 pts;
	void (*lock_buffer_cb)(void *arg);
	void (*unlock_buffer_cb)(void *arg);
	u32 reserved[10];
};

int video_display_control(int layer_index, int is_enable);
int video_display_setframe(int layer_index,
			struct video_display_frame_info_t *frame_info,
			int flags);
int mbd_video_display_setframe(int layer_index,
			struct mbd_video_frame_info_t *frame_info,
			int flags);
void vsync_notify_videodisplay(u8 layer_id, u32 vpp_vsync_pts_inc_scale,
			u32 vpp_vsync_pts_inc_scale_base);
#endif
