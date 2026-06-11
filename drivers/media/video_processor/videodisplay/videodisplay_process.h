/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_COMPOSER_H
#define VIDEO_COMPOSER_H

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
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>

#include <linux/kfifo.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/video_processor/video_composer_ext.h>
#include <linux/dma-mapping.h>
#include <linux/dma-mapping.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/media_proxy/AmlVideoUserdata.h>

#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_VIDEO_COMPOSER
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
#include <trace/events/meson_atrace.h>
#endif
#include "videodisplay_drv.h"
#include "videodisplay_util.h"

/* disable video_composer mode */
#define VIDEO_COMPOSER_ENABLE_NONE    0
#define VIDEO_COMPOSER_ENABLE_NORMAL  1
#define SOURCE_DTV_FIX_TUNNEL	0x1
#define SOURCE_HWC_CREAT_ION	0x2
#define SOURCE_PIC_MODE		0x4

#define PATTERN_32_DETECT_RANGE 7
#define PATTERN_22_DETECT_RANGE 7
#define PATTERN_11_DETECT_RANGE 6
#define PATTERN_22323_DETECT_RANGE 5
#define PATTERN_44_DETECT_RANGE 5
#define PATTERN_55_DETECT_RANGE 5
#define ROTATION_180_DEGREES BIT(2)

/*if add new patten, need change dev->patten[X]*/
enum video_refresh_pattern {
	PATTERN_UNDETECTED = 0,
	PATTERN_11,
	PATTERN_32,
	PATTERN_22,
	PATTERN_22323,
	PATTERN_44,
	PATTERN_55,
	MAX_NUM_PATTERNS
};

enum videodisplay_debug_class_type {
	VD_DEBUG_CLASS_AXIX_PIP = 0,
	VD_DEBUG_CLASS_CROP_PIP,
	VD_DEBUG_CLASS_FORCE_COMPOSER,
	VD_DEBUG_CLASS_FORCE_COMPOSER_PIP,
	VD_DEBUG_CLASS_TRANSFORM,
	VD_DEBUG_CLASS_VIDC,
	VD_DEBUG_CLASS_VIDC_PATTERN,
	VD_DEBUG_CLASS_FULL_AXIS,
	VD_DEBUG_CLASS_PRINT_CLOSE,
	VD_DEBUG_CLASS_RECEIVE_WAIT,
	VD_DEBUG_CLASS_MARGIN_TIME,
	VD_DEBUG_CLASS_MAX_WIDTH,
	VD_DEBUG_CLASS_MAX_HEIGHT,
	VD_DEBUG_CLASS_ROTATE_WIDTH,
	VD_DEBUG_CLASS_ROTATE_HEIGHT,
	VD_DEBUG_CLASS_DEWARP_ROTATE_WIDTH,
	VD_DEBUG_CLASS_DEWARP_ROTATE_HEIGHT,
	VD_DEBUG_CLASS_CLOSE_BLACK,
	VD_DEBUG_CLASS_COMPOSER_USE_444,
	VD_DEBUG_CLASS_RESET_DROP,
	VD_DEBUG_CLASS_DROP_COUNT,
	VD_DEBUG_CLASS_VPP_DROP_COUNT,
	VD_DEBUG_CLASS_DROP_COUNT_PIP,
	VD_DEBUG_CLASS_RECEIVE_COUNT,
	VD_DEBUG_CLASS_RECEIVE_COUNT_PIP,
	VD_DEBUG_CLASS_RECEIVE_NEW_COUNT,
	VD_DEBUG_CLASS_RECEIVE_NEW_COUNT_PIP,
	VD_DEBUG_CLASS_TOTAL_GET_COUNT,
	VD_DEBUG_CLASS_TOTAL_PUT_COUNT,
	VD_DEBUG_CLASS_NN_NEED_TIME,
	VD_DEBUG_CLASS_NN_MARGIN_TIME,
	VD_DEBUG_CLASS_NN_BYPASS,
	VD_DEBUG_CLASS_FENCE_CREATE_COUNT,
	VD_DEBUG_CLASS_DUMP_VFRAME,
	VD_DEBUG_CLASS_PULLDOWN_LEVEL,
	VD_DEBUG_CLASS_MAX_HOLD_COUNT,
	VD_DEBUG_CLASS_SET_FRAME_DELAY,
	VD_DEBUG_CLASS_VD_DUMP_VFRAME,
	VD_DEBUG_CLASS_ACTUAL_DELAY_COUNT,
	VD_DEBUG_CLASS_VICP_OUTPUT_DEV,
	VD_DEBUG_CLASS_VICP_SHRINK_MODE,
	VD_DEBUG_CLASS_VICP_MAX_WIDTH,
	VD_DEBUG_CLASS_VICP_MAX_HEIGHT,
	VD_DEBUG_CLASS_COMPOSER_DEV_CHOICE,
	VD_DEBUG_CLASS_FORCE_COMPOSER_WIDTH,
	VD_DEBUG_CLASS_FORCE_COMPOSER_HEIGHT,
	VD_DEBUG_CLASS_VD_TEST_FPS,
	VD_DEBUG_CLASS_DEWARP_LOAD_FLAG,
	VD_DEBUG_CLASS_LOSSY_COMPRESS_RATE,
	VD_DEBUG_CLASS_FRC_PATTERN_EN,
	VD_DEBUG_CLASS_BUF_STATUS,
	VD_DEBUG_CLASS_USE_LOW_LATENCY,
	VD_DEBUG_CLASS_LAST_FRAME_INDEX,
	VD_DEBUG_CLASS_VD_COMPOSER_EN,
	VD_DEBUG_CLASS_DROP_ORG_I,
	VD_DEBUG_CLASS_DROP_HDMI_COUNT,
	VD_DEBUG_CLASS_CONTINUE_DUMP_NUM,
	VD_DEBUG_CLASS_MAX_COUNT_HOLD_VSYNC,
	VD_DEBUG_CLASS_MAX
};

extern struct timespec64 isr_spec_time;
extern u32 next_vsync_wakeup_vpp_to_get;
extern u32 allow_vpp_to_get;
extern u64 next_isr_spec_time;

#ifndef MAX
#define MAX(a, b) ({ \
	typeof(a) _a = a; \
	typeof(b) _b = b; \
	_a > _b ? _a : _b; \
	})
#endif // MAX

#ifndef MIN
#define MIN(a, b) ({ \
			typeof(a) _a = a; \
			typeof(b) _b = b; \
			_a < _b ? _a : _b; \
		})
#endif // MIN

enum vc_transform_t {
	/* flip source image horizontally */
	VC_TRANSFORM_FLIP_H = 1,
	/* flip source image vertically */
	VC_TRANSFORM_FLIP_V = 2,
	/* rotate source image 90 degrees clock-wise */
	VC_TRANSFORM_ROT_90 = 4,
	/* rotate source image 180 degrees */
	VC_TRANSFORM_ROT_180 = 3,
	/* rotate source image 270 degrees clock-wise */
	VC_TRANSFORM_ROT_270 = 7,
	/* flip source image horizontally, the rotate 90 degrees clock-wise */
	VC_TRANSFORM_FLIP_H_ROT_90 = VC_TRANSFORM_FLIP_H | VC_TRANSFORM_ROT_90,
	/* flip source image vertically, the rotate 90 degrees clock-wise */
	VC_TRANSFORM_FLIP_V_ROT_90 = VC_TRANSFORM_FLIP_V | VC_TRANSFORM_ROT_90,
};

enum vc_fence_status {
	VC_FENCE_INVALID = 0,
	VC_FENCE_DEC_ERR,
	VC_FENCE_RELEASED,
	VC_FENCE_NORMAL,
	VC_FENCE_WAIT,
};

struct output_axis {
	int left;
	int top;
	int width;
	int height;
};

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
extern struct mediaproxy_info_t vd_mediaproxy_display_info[];
#endif

int vd_set_enable(int index, u32 val);
int vd_set_frames(int index, struct frames_info_t *frames_info);
void vd_vsync_notify(u8 layer_id, u32 vpp_vsync_pts_inc_scale,
	u32 vpp_vsync_pts_inc_scale_base);
void set_debug_flag_val(enum videodisplay_debug_class_type debug_type, int value);
int get_debug_flag_val(enum videodisplay_debug_class_type debug_type);
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
int media_proxy_produce_deinit(void *handle);
int media_proxy_produce_init(void **handle, char *modulename, u32 msg_type);
int notify_msg_to_mediaproxy(void *handle, int num, void *data);
#endif
int is_video_process_in_thread(void);
void vpp_lowlatency_wakeup(void);
int vd_set_mediasync_callback(struct mediasync_cb *func);
#endif
