// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/vfm/vframe.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/video_processor/video_pp_common.h>
#include "videodisplay.h"
#include <linux/amlogic/media/media_proxy/AmlVideoUserdata.h>
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
#include <linux/amlogic/media/frc/frc_common.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/media/di/di.h>
#endif

static struct timeval vsync_time[MAX_VD_LAYERS];
static DEFINE_MUTEX(video_display_mutex);

static int get_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
static unsigned int continue_vsync_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
int actual_delay_count[MAX_VD_LAYERS];
static struct timeval start_time[MAX_VD_LAYERS];
static struct timeval end_time[MAX_VD_LAYERS];
static int start_vsync_count[MAX_VD_LAYERS];
static int end_vsync_count[MAX_VD_LAYERS];
u32 vd_vsync_pts_inc_scale[MAX_VD_LAYERS];
u32 vd_vsync_pts_inc_scale_base[MAX_VD_LAYERS];

#define PATTERN_32_DETECT_RANGE 7
#define PATTERN_22_DETECT_RANGE 7
#define PATTERN_11_DETECT_RANGE 6
#define PATTERN_22323_DETECT_RANGE 5
#define PATTERN_44_DETECT_RANGE 5
#define PATTERN_55_DETECT_RANGE 5

#define ROTATION_180_DEGREES BIT(2)

static void vc_vf_put(struct vframe_s *vf, void *op_arg);

/*if add new patten, need change dev->patten[X]*/
enum video_refresh_pattern {
	PATTERN_11 = 0,
	PATTERN_32,
	PATTERN_22,
	PATTERN_22323,
	PATTERN_44,
	PATTERN_55,
	MAX_NUM_PATTERNS
};

static int patten_trace[MAX_VIDEO_COMPOSER_INSTANCE_NUM];
static int vsync_count[MAX_VIDEO_COMPOSER_INSTANCE_NUM];

void video_display_para_reset(int layer_index)
{
	actual_delay_count[layer_index] = 0;
}

static void calculate_real_fps_and_vsync(struct timeval *start, struct timeval *end,
	u32 *start_count, u32 *end_count, int i)
{
	u64 diff_time = 0;

	diff_time = (1000000L * (u64)(end->tv_sec - start->tv_sec) +
		(end->tv_usec - start->tv_usec));
	vd_test_vsync_val[i] = div64_u64(100 * diff_time, (*end_count - *start_count));
	vd_test_fps_val[i] = div64_u64((u64)100000 * 100000 * 1000, vd_test_vsync_val[i]);
}

void vsync_notify_video_composer(u8 layer_id,
	u32 vpp_vsync_pts_inc_scale, u32 vpp_vsync_pts_inc_scale_base)
{
	vsync_count[layer_id]++;
	get_count[layer_id] = 0;
	continue_vsync_count[layer_id]++;
	patten_trace[layer_id]++;

	vd_vsync_pts_inc_scale[layer_id] = vpp_vsync_pts_inc_scale;
	vd_vsync_pts_inc_scale_base[layer_id] = vpp_vsync_pts_inc_scale_base;
	do_gettimeofday(&vsync_time[layer_id]);

	if (layer_id == 0 && get_count[0] > 0)
		vpp_drop_count += (get_count[0] - 1);

	switch (vd_test_fps[layer_id]) {
	case 0:
		break;
	case 1: {
		start_time[layer_id] = vsync_time[layer_id];
		start_vsync_count[layer_id] = vsync_count[layer_id];
		vd_test_fps[layer_id] = 0;
		break;
	}
	case 2: {
		end_time[layer_id] = vsync_time[layer_id];
		end_vsync_count[layer_id] = vsync_count[layer_id];
		calculate_real_fps_and_vsync(&start_time[layer_id], &end_time[layer_id],
			&start_vsync_count[layer_id], &end_vsync_count[layer_id], layer_id);
		vd_test_fps[layer_id] = 0;
		break;
	}
	default:
		break;
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
static void vc_notify_msg_to_mediaproxy(struct vframe_s *vf, struct composer_dev *dev,
	int event, struct timespec64 ts)
{
	struct aml_video_user_data *msg;

	if (!dev)
		return;

	msg = &dev->user_data_msg;
	memset(msg, 0, sizeof(*msg));
	msg->data.frame_info.bitstreamid = div_u64(vf->timestamp, NSEC_PER_SEC);
	msg->data.frame_info.vd_instid = dev->index;
	msg->data.frame_info.decoder_instid = vf->decoder_instid;
	msg->data.frame_info.frame_index = vf->frame_index;
	msg->data.frame_info.time = (u64)ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
	msg->message_type = event;

	notify_msg_to_mediaproxy(mediaproxy_display_info[dev->index].k_producer_session, 1, msg);
}
#endif

static bool vd_vf_is_tvin(struct vframe_s *vf)
{
	if (!vf)
		return false;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		return true;
	return false;
}

bool check_frc_n2m_status(void)
{
	bool ret = false;

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		/* frc_get_n2m_setting 1 : n2m is 1:1; 2 :n2m is 1:2 */
		/* frc_is_on() 1: means frc really worked */
		if ((frc_get_n2m_setting() >= 2) && frc_is_on())
			ret = true;
#endif
	return ret;
}

static void vf_pop_display_q(struct composer_dev *dev, struct vframe_s *vf)
{
	struct vframe_s *dis_vf = NULL;

	int k = kfifo_len(&dev->display_q);

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			if (dis_vf == vf)
				break;
			if (!kfifo_put(&dev->display_q, dis_vf))
				vc_print(dev->index, PRINT_ERROR,
					 "display_q is full!\n");
		}
		k--;
		if (k < 0) {
			vc_print(dev->index, PRINT_ERROR,
				 "can find vf in display_q\n");
			break;
		}
	}
}

void video_display_push_ready(struct composer_dev *dev, struct vframe_s *vf)
{
	u32 vsync_index = vsync_count[dev->index];

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	if (!dev->enable_pulldown && (vd_pulldown_level && frc_get_video_latency())) {
		dev->enable_pulldown = true;
		vc_print(dev->index, PRINT_OTHER, "%s: enable pulldown\n", __func__);
	}
#endif

	if (vf && vf->vc_private) {
		vf->vc_private->vsync_index = vsync_index;
		vc_print(dev->index, PRINT_OTHER,
			"set vsync_index =%d\n", vsync_index);
	}
}

static void vd_vsync_video_pattern(struct composer_dev *dev, int pattern, struct vframe_s *vf)
{
	int factor1 = 0, factor2 = 0;
	int pattern_range = 0;

	if (pattern >= MAX_NUM_PATTERNS)
		return;

	if (pattern == PATTERN_11) {
		factor1 = 1;
		factor2 = 1;
		pattern_range = PATTERN_11_DETECT_RANGE;
	} else if (pattern == PATTERN_22) {
		factor1 = 2;
		factor2 = 2;
		pattern_range =  PATTERN_22_DETECT_RANGE;
	} else if (pattern == PATTERN_32) {
		factor1 = 3;
		factor2 = 2;
		pattern_range =  PATTERN_32_DETECT_RANGE;
	} else if (pattern == PATTERN_44) {
		factor1 = 4;
		factor2 = 4;
		pattern_range =  PATTERN_44_DETECT_RANGE;
	} else if (pattern == PATTERN_55) {
		factor1 = 5;
		factor2 = 5;
		pattern_range =  PATTERN_55_DETECT_RANGE;
	} else if (pattern >= MAX_NUM_PATTERNS) {
		pr_err("not support pattern %d\n", pattern);
		return;
	}

	/* update 3:2 or 2:2 mode detection */
	if ((dev->pre_pat_trace == factor1 && patten_trace[dev->index] == factor2) ||
	    (dev->pre_pat_trace == factor2 && patten_trace[dev->index] == factor1)) {
		if (dev->pattern[pattern] < pattern_range) {
			dev->pattern[pattern]++;
			if (dev->pattern[pattern] == pattern_range) {
				dev->pattern_enter_cnt++;
				dev->pattern_detected = pattern;
				vc_print(dev->index, PRINT_PATTERN,
					"patten: video %d:%d mode detected\n",
					factor1, factor2);
			}
		}
	} else if (dev->pattern[pattern] == pattern_range) {
		if (pattern == PATTERN_11 &&
			patten_trace[dev->index] == 2 &&
			dev->pre_pat_trace == 1 &&
			vf->frame_index == dev->last_vf_index + 1) {
			patten_trace[dev->index] = 1;
			vc_print(dev->index, PRINT_PATTERN,
				"patten_11: video %d:%d mode force unbroken, pre_pat=%d, %d, index=%d, %d\n",
				factor1, factor2, dev->pre_pat_trace,
				patten_trace[dev->index],
				vf->frame_index, dev->last_vf_index);
			return;
		} else if (pattern == PATTERN_22 &&
			patten_trace[dev->index] == 3 &&
			dev->pre_pat_trace == 2 &&
			vf->frame_index == dev->last_vf_index + 1) {
			patten_trace[dev->index] = 2;
			vc_print(dev->index, PRINT_PATTERN,
				"patten_22: video %d:%d mode force unbroken, pre_pat=%d, %d, index=%d, %d\n",
				factor1, factor2, dev->pre_pat_trace,
				patten_trace[dev->index],
				vf->frame_index, dev->last_vf_index);
			return;
		}
		dev->pattern[pattern] = 0;
		dev->pattern_exit_cnt++;
		vc_print(dev->index, PRINT_PATTERN,
			"patten: video %d:%d mode broken, pre_pat=%d, patten =%d, index=%d, %d\n",
			factor1, factor2, dev->pre_pat_trace, patten_trace[dev->index],
			vf->frame_index, dev->last_vf_index);
	} else {
		vc_print(dev->index, PRINT_PATTERN,
			"pattern:%d invalid case, reset to 0.\n", pattern);
		dev->pattern[pattern] = 0;
	}
}

static void vd_vsync_video_pattern_22323(struct composer_dev *dev, struct vframe_s *vf)
{
	int factor1 = 2;
	int factor2 = 2;
	int factor3 = 3;
	int factor4 = 2;
	int factor5 = 3;
	int pattern = PATTERN_22323;
	int pattern_range = PATTERN_22323_DETECT_RANGE;
	bool check_ok = false;
	int i = 0;
	int index_1;
	int index_2;
	int index_3;
	int index_4;
	int index_5;
	int cur_factor_index = dev->patten_factor_index;
	int vsync_pts_inc = 16 * 90000 *
		vd_vsync_pts_inc_scale[dev->index] / vd_vsync_pts_inc_scale_base[dev->index];
	int vframe_duration = vf->duration * 15;

	if (vsync_pts_inc * 12 != vframe_duration * 5)
		return;

	for (i = 0; i < PATTEN_FACTOR_MAX; i++) {
		index_1 = i;

		index_2 = i + 1;
		if (index_2 >= PATTEN_FACTOR_MAX)
			index_2 -= PATTEN_FACTOR_MAX;

		index_3 = i + 2;
		if (index_3 >= PATTEN_FACTOR_MAX)
			index_3 -= PATTEN_FACTOR_MAX;

		index_4 = i + 3;
		if (index_4 >= PATTEN_FACTOR_MAX)
			index_4 -= PATTEN_FACTOR_MAX;

		index_5 = i + 4;
		if (index_5 >= PATTEN_FACTOR_MAX)
			index_5 -= PATTEN_FACTOR_MAX;

		if (dev->patten_factor[index_1] == factor1 &&
			dev->patten_factor[index_2] == factor2 &&
			dev->patten_factor[index_3] == factor3 &&
			dev->patten_factor[index_4] == factor4 &&
			dev->patten_factor[index_5] == factor5) {
			check_ok = true;
			if (cur_factor_index == index_1)
				dev->next_factor = factor2;
			else if (cur_factor_index == index_2)
				dev->next_factor = factor3;
			else if (cur_factor_index == index_3)
				dev->next_factor = factor4;
			else if (cur_factor_index == index_4)
				dev->next_factor = factor5;
			else if (cur_factor_index == index_5)
				dev->next_factor = factor1;
			break;
		}
	}

	if (check_ok) {
		if (dev->pattern[pattern] < pattern_range) {
			dev->pattern[pattern]++;
			if (dev->pattern[pattern] == pattern_range) {
				dev->pattern_enter_cnt++;
				dev->pattern_detected = pattern;
				vc_print(dev->index, PRINT_PATTERN,
					"patten: video 22323 mode detected\n");
			}
		}
	} else if (dev->pattern[pattern] == pattern_range) {
		dev->pattern[pattern] = 0;
		dev->pattern_exit_cnt++;
		vc_print(dev->index, PRINT_PATTERN,
			"patten: video 22323 mode broken, pre_pat=%d, patten =%d, index=%d, %d\n",
			dev->pre_pat_trace, patten_trace[dev->index],
			vf->frame_index, dev->last_vf_index);
	} else {
		dev->pattern[pattern] = 0;
	}
}

static void vd_vsync_video_pattern_13213(struct composer_dev *dev, struct vframe_s *vf)
{
	int i = 0, sum = 0, ave = 0;
	int vsync_pts_inc = 16 * 90000 *
		vd_vsync_pts_inc_scale[dev->index] / vd_vsync_pts_inc_scale_base[dev->index];
	int vframe_duration = vf->duration * 15;

	if (vsync_pts_inc * 2 != vframe_duration) {
		vc_print(dev->index, PRINT_PATTERN, "%s: not 13213 condition.\n", __func__);
		return;
	}

	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		sum += dev->patten_factor[i];

	ave = sum / PATTEN_FACTOR_MAX;
	if (ave == 2) {
		dev->pattern_detected = PATTERN_22;
		dev->pattern[PATTERN_22] = PATTERN_22_DETECT_RANGE;
		dev->pattern_enter_cnt++;
		vc_print(dev->index, PRINT_PATTERN, "%s: video 13213 mode detected\n", __func__);
	} else {
		vc_print(dev->index, PRINT_PATTERN, "%s: not 13213 mode.\n", __func__);
	}
}

static void vd_vsync_video_pattern_53(struct composer_dev *dev, struct vframe_s *vf)
{
	int i = 0, sum = 0, ave = 0;
	int vsync_pts_inc = 16 * 90000 *
		vd_vsync_pts_inc_scale[dev->index] / vd_vsync_pts_inc_scale_base[dev->index];
	int vframe_duration = vf->duration * 15;

	if (vsync_pts_inc * 4 != vframe_duration) {
		vc_print(dev->index, PRINT_PATTERN, "%s: not 53 condition.\n", __func__);
		return;
	}

	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		sum += dev->patten_factor[i];

	ave = sum / PATTEN_FACTOR_MAX;
	if (ave == 4) {
		dev->pattern_detected = PATTERN_44;
		dev->pattern[PATTERN_44] = PATTERN_44_DETECT_RANGE;
		dev->pattern_enter_cnt++;
		vc_print(dev->index, PRINT_PATTERN, "%s: video 53 mode detected\n", __func__);
	} else {
		vc_print(dev->index, PRINT_PATTERN, "%s: not 53 mode.\n", __func__);
	}
}

static void vd_vsync_video_pattern_11120(struct composer_dev *dev, struct vframe_s *vf)
{
	int i = 0, sum = 0, ave = 0;

	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		sum += dev->patten_factor[i];

	ave = sum / PATTEN_FACTOR_MAX;
	if (ave == 1) {
		dev->pattern_detected = PATTERN_11;
		dev->pattern[PATTERN_11] = PATTERN_11_DETECT_RANGE;
		dev->pattern_enter_cnt++;
		vc_print(dev->index, PRINT_PATTERN, "%s: video 11 mode detected\n", __func__);
	} else {
		vc_print(dev->index, PRINT_PATTERN, "%s: not 11 mode.\n", __func__);
	}
}

static void vsync_video_pattern(struct composer_dev *dev, struct vframe_s *vf)
{
	vd_vsync_video_pattern(dev, PATTERN_11, vf);
	vd_vsync_video_pattern(dev, PATTERN_32, vf);
	vd_vsync_video_pattern(dev, PATTERN_22, vf);
	vd_vsync_video_pattern_22323(dev, vf);
	vd_vsync_video_pattern(dev, PATTERN_44, vf);
	vd_vsync_video_pattern(dev, PATTERN_55, vf);
	if (dev->pattern_detected != PATTERN_11 ||
		(dev->pattern[PATTERN_11] != PATTERN_11_DETECT_RANGE))
		vd_vsync_video_pattern_11120(dev, vf);
	if (dev->pattern_detected != PATTERN_22 ||
		(dev->pattern[PATTERN_22] != PATTERN_22_DETECT_RANGE))
		vd_vsync_video_pattern_13213(dev, vf);
	if (dev->pattern_detected != PATTERN_44 ||
		(dev->pattern[PATTERN_44] != PATTERN_44_DETECT_RANGE))
		vd_vsync_video_pattern_53(dev, vf);
	/*vd_vsync_video_pattern(dev, PTS_41_PATTERN);*/
}

static inline int vd_perform_pulldown(struct composer_dev *dev,
					 struct vframe_s *vf,
					 bool *expired)
{
	int pattern_range, expected_curr_interval;
	int expected_prev_interval;

	/* Dont do anything if we have invalid data */
	if (!vf || !vf->vc_private)
		return -1;
	if (vf->vc_private->vsync_index == 0)
		return -1;

	if (vd_vf_is_tvin(vf))
		return -1;

	switch (dev->pattern_detected) {
	case PATTERN_11:
		pattern_range =  PATTERN_11_DETECT_RANGE;
		expected_prev_interval = 1;
		expected_curr_interval = 1;
		break;
	case PATTERN_32:
		pattern_range = PATTERN_32_DETECT_RANGE;
		switch (dev->pre_pat_trace) {
		case 3:
			expected_prev_interval = 3;
			expected_curr_interval = 2;
			break;
		case 2:
			expected_prev_interval = 2;
			expected_curr_interval = 3;
			break;
		default:
			return -1;
		}
		break;
	case PATTERN_22:
		if (dev->pre_pat_trace != 2)
			vc_print(dev->index, PRINT_PATTERN,
				"patten:: pre_pat_trace =%d",
				dev->pre_pat_trace);

		pattern_range =  PATTERN_22_DETECT_RANGE;
		expected_prev_interval = 2;
		expected_curr_interval = 2;
		break;
	case PATTERN_22323:
		pattern_range =  PATTERN_22323_DETECT_RANGE;
		expected_curr_interval = dev->next_factor;
		break;
	case PATTERN_44:
		pattern_range =  PATTERN_44_DETECT_RANGE;
		expected_prev_interval = 4;
		expected_curr_interval = 4;
		break;
	case PATTERN_55:
		pattern_range =  PATTERN_55_DETECT_RANGE;
		expected_prev_interval = 5;
		expected_curr_interval = 5;
		break;
	default:
		return -1;
	}

	vc_print(dev->index, PRINT_PATTERN,
		"patten: detected %d, pattern =%d, patten_trace=%d, expected=%d expired=%d\n",
		dev->pattern_detected,
		dev->pattern[dev->pattern_detected],
		patten_trace[dev->index],
		expected_curr_interval,
		*expired);

	/* We do nothing if we dont have enough data*/
	if (dev->pattern[dev->pattern_detected] != pattern_range)
		return -1;

	if (*expired) {
		if (patten_trace[dev->index] < expected_curr_interval) {
			/* 2323232323..2233..2323, prev=2, curr=3,*/
			/* check if next frame will toggle after 3 vsync */
			/* 22222...22222 -> 222..2213(2)22...22 */
			/* check if next frame will toggle after 3 vsync */
			*expired = false;
			vc_print(dev->index, PRINT_PATTERN,
				"patten:hold frame for pattern: %d",
				dev->pattern_detected);
		}
	} else {
		if (patten_trace[dev->index] >= expected_curr_interval) {
			/* 23232323..233223...2323 curr=2, prev=3 */
			/* check if this frame will expire next vsync and */
			/* next frame will expire after 3 vsync */
			/* 22222...22222 -> 222..223122...22 */
			/* check if this frame will expire next vsync and */
			/* next frame will expire after 2 vsync */
			*expired = true;
			vc_print(dev->index, PRINT_PATTERN,
				"patten: pull frame for pattern: %d",
				dev->pattern_detected);
		}
	}
	return 0;
}

int find_nearest_duration(struct composer_dev *dev, int duration_val)
{
	int min = INT_MAX;
	int duration_arr[] = {200, 266, 282, 290, 320, 333, 400, 533, 565, 582, 640, 667, 800, 801,
		960, 1600, 1601, 1920, 3200, 3203, 3840, 4000, 4004};
	int i = 0, num = 0, diff = 0;
	int recy_count = sizeof(duration_arr) / sizeof(int);

	for (i = 0; i < recy_count; i++) {
		diff = abs(duration_val - duration_arr[i]);
		if (diff < min) {
			min = diff;
			num = i;
		}
	}

	vc_print(dev->index, PRINT_PATTERN, "The nearest duration is %d.\n", duration_arr[num]);
	return duration_arr[num];
}

static bool pulldown_support_vf(struct composer_dev *dev, u32 duration_val)
{
	bool support = false;
	int duration = 0;
	/*duration: 800(120fps) 801(119.88fps) 960(100fps) 1600(60fps) 1920(50fps)*/
	/*3200(30fps) 3203(29.97) 3840(25fps) 4000(24fps) 4004(23.976fps)*/

	duration = find_nearest_duration(dev, duration_val);

	if (new_afr_pulldown)
		support = true;
	if (vd_vsync_pts_inc_scale[dev->index] == 1 &&
		vd_vsync_pts_inc_scale_base[dev->index] == 48) {
		/*48hz for 24fps 23.976fps*/
		if (duration == 4004 || duration == 4000)
			support = true;
	} else if (vd_vsync_pts_inc_scale[dev->index] == 1 &&
		vd_vsync_pts_inc_scale_base[dev->index] == 50) {
		/*50hz for 25fps 50fps*/
		if (duration == 3840 || duration == 1920)
			support = true;
	} else if (vd_vsync_pts_inc_scale[dev->index] == 1001 &&
		vd_vsync_pts_inc_scale_base[dev->index] == 60000) {
		/*59.94hz for 23.976, 29.97*/
		if (duration == 4004 || duration == 3203)
			support = true;
	} else if (vd_vsync_pts_inc_scale[dev->index] == 1 &&
		vd_vsync_pts_inc_scale_base[dev->index] == 60) {
		/*60hz for 23.976, 24, 25, 29.97, 30*/
		if (duration == 4004 ||
			duration == 4000 ||
			duration == 3840 ||  /*22323 patten for projector, that vout always 60*/
			duration == 3203 ||
			duration == 3200)
			support = true;
	} else if (vd_vsync_pts_inc_scale[dev->index] == 1 &&
		vd_vsync_pts_inc_scale_base[dev->index] == 100) {
		/*100hz for 25fps, 50fps*/
		if (duration == 3840 || duration == 1920)
			support = true;
	} else if (vd_vsync_pts_inc_scale[dev->index] == 1001 &&
		vd_vsync_pts_inc_scale_base[dev->index] == 120000) {
		/*119.88hz for 23.976,29.97,59.94*/
		if (duration == 4004 ||
			duration == 3203 ||
			duration == 1601)
			support = true;
	} else if (vd_vsync_pts_inc_scale[dev->index] == 1 &&
		vd_vsync_pts_inc_scale_base[dev->index] == 120) {
		/*120hz for 23.976, 24, 29.97, 30, 59.94, 60*/
		if (duration == 4004 ||
			duration == 4000 ||
			duration == 3203 ||
			duration == 3200 ||
			duration == 1601 ||
			duration == 1600)
			support = true;
	}

	return support;
}

/* -----------------------------------------------------------------
 *           provider operations
 * -----------------------------------------------------------------
 */
static struct vframe_s *vc_vf_peek(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	struct timeval time1;
	struct timeval time2;
	u64 time_vsync;
	u64 interval_time;
	bool expired = true;
	bool expired_tmp = true;
	bool open_pulldown = false;
	bool special_case = false;
	int ready_len;
	u32 vsync_index = 0;
	int ret;
	int max_delay_count = 2;
	int input_fps, output_fps, output_pts_inc_scale = 0, output_pts_inc_scale_base = 0;
	int aisr_delay_vsync;
	int total_delay_vsync;
	struct timespec64 cur_spec_time;

	time1 = dev->start_time;
	time2 = vsync_time[dev->index];

	if (kfifo_peek(&dev->ready_q, &vf)) {
		if (get_lowlatency_mode())
			return vf;

		if (is_video_process_in_thread() && dev->low_latency_case == 2) {
			ktime_get_ts64(&cur_spec_time);
			vc_print(dev->index, PRINT_OTHER,
				"cur_spec_time=%llu, next_isr_spec_time=%llu, allow_vpp_to_get=%d\n",
				cur_spec_time.tv_sec * 1000000000LL + cur_spec_time.tv_nsec,
				next_isr_spec_time, allow_vpp_to_get);
			if ((cur_spec_time.tv_sec * 1000000000LL +
				cur_spec_time.tv_nsec < next_isr_spec_time) && !allow_vpp_to_get) {
				vc_print(dev->index, PRINT_OTHER, "vpp to get, not allow to get\n");
				return NULL;
			}
		}

		if (dev->index == 0 && is_meson_t3x_cpu() && check_frc_n2m_status())
			aisr_delay_vsync = 1;
		else
			aisr_delay_vsync = 0;
		total_delay_vsync = aisr_delay_vsync + vd_set_frame_delay[dev->index];
		if (vf->vc_private && total_delay_vsync > 0) {
			vsync_index = vf->vc_private->vsync_index;
			vc_print(dev->index, PRINT_OTHER,
				"peek: vsync_index =%d, delay_count=%d, vsync_count=%d\n",
				vsync_index, total_delay_vsync,
				vsync_count[dev->index]);
			if (vsync_index + total_delay_vsync
				>= vsync_count[dev->index] &&
				vsync_index < vsync_count[dev->index])
				return NULL;
		} else {
			vc_print(dev->index, PRINT_OTHER,
				"peek: vf->vc_private is NULL\n");
		}

		if ((vf->flag & VFRAME_FLAG_GAME_MODE) && vd_vf_is_tvin(vf) &&
			(dev->index == 1 && dev->video_render_index == 5) &&
			vf->vc_private) {
			vsync_index = vf->vc_private->vsync_index;
			if (vsync_index + 1 >= vsync_count[dev->index])
				return NULL;
		}
		if (vf->flag & VFRAME_FLAG_GAME_MODE)
			return vf;

		input_fps = vf->duration * 15;
		get_output_pcrscr_info(&output_pts_inc_scale, &output_pts_inc_scale_base);
		output_fps = 90000 * 16 * (u64)output_pts_inc_scale;
		if (!output_pts_inc_scale_base)
			return NULL;
		output_fps = div64_u64(output_fps, output_pts_inc_scale_base);
		vc_print(dev->index, PRINT_OTHER,
			"peek: input_fps=%d, output_fps=%d.\n", input_fps, output_fps);
		/*apk/sf drop 0/3 4; vc receive 1 2 5 in one vsync*/
		/*apk queue 5 and wait 1, it will fence timeout*/
		/* dev->video_render_index == 5 means T7 dual screen mode */
		/*input 120hz with 60hz output or input 100hz with 50hz output no need check*/
		if (vd_vf_is_tvin(vf) && (input_fps * 3 < output_fps * 2) && input_fps <= 14400)
			special_case = true;
		if (!special_case && (get_count[dev->index] == 2 && dev->video_render_index != 5)) {
			vc_print(dev->index, PRINT_OTHER,
				 "has already get 2, can not get more, video_render.%d",
				 dev->video_render_index);
			return NULL;
		}

		time_vsync = (u64)1000000
			* (time2.tv_sec - time1.tv_sec)
			+ time2.tv_usec - time1.tv_usec;

		/*dv video on TV platform tog more then 2ms, if hwc set frame after HW vsync 1ms,*/
		/*this vf will be get by current vsync;*/
		/*only enable for android, if linux set frame also set pts_us64, we can enable it */
		if (!is_meson_t7_cpu() && !dev->is_drm_enable) {
			if (vf->pts_us64 >= time_vsync && vf->pts_us64 < (time_vsync + 10000)) {
				vc_print(dev->index, PRINT_PATTERN,
					 "display next vsync: pts_us64=%lld, time_vsync=%lld\n",
					 vf->pts_us64, time_vsync);
				return NULL;
			}
		}

		if (get_count[dev->index] > 0 &&
			!(vf->flag & VFRAME_FLAG_GAME_MODE)) {
			interval_time = abs(time_vsync - vf->pts_us64);
			vc_print(dev->index, PRINT_PERFORMANCE,
				 "time_vsync=%lld, vf->pts_us64=%lld\n",
				 time_vsync, vf->pts_us64);
			//TODO
			if (interval_time < 2000/*margin_time*/) {
				vc_print(dev->index, PRINT_PATTERN,
					 "display next vsync\n");
				return NULL;
			}
		}

		if (dev->enable_pulldown && pulldown_support_vf(dev, vf->duration)) {
			open_pulldown = true;
			ready_len = kfifo_len(&dev->ready_q);
			if ((ready_len > 1 && vd_pulldown_level == 1) ||
				(ready_len > 2 && vd_pulldown_level > 1)) {
				open_pulldown = false;
				vc_print(dev->index, PRINT_PATTERN,
					"ready_q len=%d\n", ready_len);
			}
		}

		if (open_pulldown &&
			!vd_vf_is_tvin(vf) &&
			!(vf->flag & VFRAME_FLAG_FAKE_FRAME)) {
			if (vf->vc_private) {
				vsync_index = vf->vc_private->vsync_index;
				vc_print(dev->index, PRINT_PATTERN,
					"peek: vsync_index: %d, vsync_count:%d, frame_index=%d\n",
					vf->vc_private->vsync_index, vsync_count[dev->index],
					vf->frame_index);
				if (vsync_index + 1 >= vsync_count[dev->index] &&
					dev->pattern_detected != PATTERN_11)
					expired = false;
			}
			expired_tmp = expired;
			ret = vd_perform_pulldown(dev, vf, &expired);
			if (!expired) {
				if (vd_pulldown_level > 1)
					max_delay_count += vd_pulldown_level - 1;
				if (vsync_index + max_delay_count < vsync_count[dev->index]) {
					vc_print(dev->index, PRINT_PATTERN,
						"need disp, vsync_index =%d, vsync_count=%d\n",
						vsync_index, vsync_count[dev->index]);
					expired = true;
				} else if (expired_tmp) {
					if (dev->last_hold_index + 1 == vf->frame_index)
						dev->continue_hold_count++;
					else if (dev->last_hold_index != vf->frame_index)
						dev->continue_hold_count = 1;
					vc_print(dev->index, PRINT_PATTERN,
						"patten: hold, frame_index =%d, continue_count=%d\n",
						vf->frame_index, dev->continue_hold_count);
					dev->last_hold_index = vf->frame_index;
					if (dev->continue_hold_count >= vd_max_hold_count) {
						expired = true;
						vc_print(dev->index, PRINT_PATTERN,
						"patten: can not hold too many vf\n");
					}
				}
				if (!expired)
					return NULL;
			}
		}

		if (dev->is_drm_enable)
			return vf;
		else
			return videocomposer_vf_peek(op_arg);
	} else {
		return NULL;
	}
}

static struct vframe_s *vc_vf_get(void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vframe_s *vf = NULL;
	u32 vsync_index_diff = 0;
	bool enable_prelink = false;
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	struct timespec64 ts;
#endif

	if (kfifo_get(&dev->ready_q, &vf)) {
		if (!vf)
			return NULL;

		if (vf->flag & VFRAME_FLAG_FAKE_FRAME)
			return vf;

		if (!kfifo_put(&dev->display_q, vf))
			vc_print(dev->index, PRINT_ERROR,
				 "display_q is full!\n");
		if (!(vf->flag
		      & VFRAME_FLAG_VIDEO_COMPOSER)) {
			pr_err("vc: vf_get: flag is null\n");
		}

		get_count[dev->index]++;
		dev->fget_count++;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
		enable_prelink = dim_get_pre_link();
#endif
		if (vf->vc_private) {
			vsync_index_diff = vf->vc_private->vsync_index - dev->last_vsync_index;
			dev->last_vsync_index = vf->vc_private->vsync_index;
			if (vf->frame_index < dev->last_vf_index) {
				vc_print(dev->index, PRINT_PATTERN,
					 "change source\n");
				vf->vc_private->flag |= VC_FLAG_FIRST_FRAME;
			}
		}

		vc_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			 "get:frame_index=%d, index_disp=%d, get=%d, total_get=%lld, vsync =%d, diff=%d, duration=%d\n",
			 vf->frame_index,
			 vf->index_disp,
			 get_count[dev->index],
			 dev->fget_count,
			 continue_vsync_count[dev->index],
			 vsync_index_diff,
			 vf->duration);

		vc_print(dev->index, PRINT_OTHER,
			"%s: prelink_en=%d, vf=%px(%px), frame_index=%d, vf_type=0x%x, vf_flag=0x%x, vf->timestamp: %lld.di_flag=%x, dpss_flg:%x\n",
			__func__,
			enable_prelink,
			vf,
			vf->vf_ext,
			vf->frame_index,
			vf->type,
			vf->flag,
			div_u64(vf->timestamp, 1000000000),
			vf->di_flag,
			vf->dpss_flg);

		vc_print(dev->index, PRINT_OTHER,
			"%s: fbc:headaddr=0x%lx, bodyaddr=0x%lx, width=%d, height=%d.\n",
			__func__,
			vf->compHeadAddr,
			vf->compBodyAddr,
			vf->compWidth,
			vf->compHeight);

		vc_print(dev->index, PRINT_OTHER,
			"%s: mif:canvasID=%d, y_addr=0x%lx, uv_addr=0x%lx, width=%d, height=%d.\n",
			__func__,
			vf->canvas0Addr,
			vf->canvas0_config[0].phy_addr,
			vf->canvas0_config[1].phy_addr,
			vf->width,
			vf->height);

		vc_print(dev->index, PRINT_FENCE,
			"%s: frame_index=%d, frm_irq:%d,magic_code=0x%x, ud_addr=%p, ud_len=%d.\n",
			__func__,
			vf->frame_index, vf->frame_irq_cnt,
			vf->vf_ud_param.magic_code,
			vf->vf_ud_param.ud_param.pbuf_addr,
			vf->vf_ud_param.ud_param.buf_len);

		vc_print(dev->index, PRINT_AXIS,
			 "get:crop: %d %d %d %d, axis: %d %d %d %d.\n",
			 vf->crop[0], vf->crop[1], vf->crop[2], vf->crop[3],
			 vf->axis[0], vf->axis[1], vf->axis[2], vf->axis[3]);
		vc_print(dev->index, PRINT_DEWARP,
			 "get:canvas_w: %d, canvas_h: %d\n",
			  vf->canvas0_config[0].width, vf->canvas0_config[0].height);

		if (vf->vc_private) {
			vf->vc_private->last_disp_count = continue_vsync_count[dev->index];
			actual_delay_count[dev->index] = vsync_count[dev->index]
				- vf->vc_private->vsync_index + 1;
		}

		if (vf->dec_fence_status == DEC_FENCE_SUCCESS) {
			vc_print(dev->index, PRINT_OTHER,
				"%s: normal vframe, frame_index:%d fence:%px\n",
				__func__, vf->frame_index, vf->fence);
			dma_fence_get(vf->fence);
		} else if (vf->dec_fence_status == DEC_FENCE_ERR) {
			vc_print(dev->index, PRINT_OTHER, "error vframe, frame_index:%d\n",
			       vf->frame_index);
			vc_vf_put(vf, (void *)dev);
			return NULL;
		}

		if (dev->enable_pulldown) {
			dev->patten_factor_index++;
			if (dev->patten_factor_index == PATTEN_FACTOR_MAX)
				dev->patten_factor_index = 0;
			dev->patten_factor[dev->patten_factor_index] = patten_trace[dev->index];
			vsync_video_pattern(dev, vf);
			dev->pre_pat_trace = patten_trace[dev->index];
			patten_trace[dev->index] = 0;
		}

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
		ktime_get_real_ts64(&ts);
		vc_notify_msg_to_mediaproxy(vf, dev, MEDIA_VIDEO_METRICS_FRAME_TOGGLE_INFO, ts);
#endif
		continue_vsync_count[dev->index] = 0;
		dev->last_vf_index = vf->frame_index;
		current_display_vf = vf;
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
		ATRACE_COUNTER("video_composer_get_vf_frame_index", vf->frame_index);
		ATRACE_COUNTER("video_composer_get_vf_frame_index", 0);
		ATRACE_COUNTER("video_composer_get_vf_timestamp",
			div_u64(vf->timestamp, 1000000000));
		ATRACE_COUNTER("video_composer_get_vf_timestamp", 0);
#endif
		if (allow_vpp_to_get)
			allow_vpp_to_get = 0;
		return vf;
	} else {
		return NULL;
	}
}

static void vc_vf_put(struct vframe_s *vf, void *op_arg)
{
	int repeat_count;
	struct file *file_vf;
	struct vframe_s *display_vf = vf;
	struct composer_dev *dev = (struct composer_dev *)op_arg;
	struct vd_prepare_s *vd_prepare_tmp;
	struct mbp_buffer_info_t *mpb_buf = NULL;
	bool enable_prelink = false;
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	struct timespec64 ts;
#endif

	if (!vf)
		return;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	enable_prelink = dim_get_pre_link();
#endif

	if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
		vc_print(dev->index, PRINT_OTHER,
			"%s: enhance_vf=%px, type:0x%x, flag=0x%x, y_addr=0x%lx, width=%d, height=%d\n",
			__func__,
			vf,
			vf->type,
			vf->flag,
			vf->canvas0_config[0].phy_addr,
			vf->width,
			vf->height);
		vf = vf->enhance_vf;
		display_vf = vf->enhance_vf;
		vf->rendered = display_vf->rendered;
	}

	vc_print(dev->index, PRINT_OTHER,
		"%s: prelink_en=%d, vf=%px(%px), frame_index=%d, vf_type=0x%x, vf_flag=0x%x, dpss_flg=%x, vf->timestamp: %lld.\n",
		__func__,
		enable_prelink,
		vf,
		vf->vf_ext,
		vf->frame_index,
		vf->type,
		vf->flag,
		vf->dpss_flg,
		div_u64(vf->timestamp, 1000000000));

	vc_print(dev->index, PRINT_OTHER,
		"%s: fbc:headaddr=0x%lx, bodyaddr=0x%lx, width=%d, height=%d.\n",
		__func__,
		vf->compHeadAddr,
		vf->compBodyAddr,
		vf->compWidth,
		vf->compHeight);

	vc_print(dev->index, PRINT_OTHER,
		"%s: mif:canvasID=%d, y_addr=0x%lx, uv_addr=0x%lx, width=%d, height=%d.\n",
		__func__,
		vf->canvas0Addr,
		vf->canvas0_config[0].phy_addr,
		vf->canvas0_config[1].phy_addr,
		vf->width,
		vf->height);

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	ktime_get_real_ts64(&ts);
	vc_notify_msg_to_mediaproxy(vf, dev, MEDIA_VIDEO_METRICS_FRAME_SIGNAFENCE_INFO, ts);
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
	ATRACE_COUNTER("video_composer_put_vf_frame_index", vf->frame_index);
	ATRACE_COUNTER("video_composer_put_vf_frame_index", 0);
	ATRACE_COUNTER("video_composer_put_vf_timestamp", div_u64(vf->timestamp, 1000000000));
	ATRACE_COUNTER("video_composer_put_vf_timestamp", 0);
#endif

	if (dev->is_drm_enable) {
		if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
			vc_print(dev->index, PRINT_OTHER, "put: fake frame\n");
			return;
		}

		vd_prepare_tmp = container_of(vf, struct vd_prepare_s, dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare_tmp)) {
			vc_print(dev->index, PRINT_ERROR, "%s: prepare is NULL.\n", __func__);
			return;
		}

		repeat_count = vf->repeat_count;
		file_vf = vf->file_vf;

		vf_pop_display_q(dev, vf);
		if (!file_vf)
			vc_print(dev->index, PRINT_ERROR, "put: file error!!!\n");

		if (vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA) {
			vc_print(dev->index, PRINT_FENCE, "put dma bufer!!!\n");
			mpb_buf = (struct mbp_buffer_info_t *)file_vf;
			if (IS_ERR_OR_NULL(mpb_buf)) {
				vc_print(dev->index, PRINT_ERROR,
					"%s: mpb_buf is NULL.\n",
					__func__);
			}
			if (vf->vc_private) {
				vf->vc_private->unlock_buffer_cb(mpb_buf);
				vc_private_q_recycle(dev, vf->vc_private);
				vf->vc_private = NULL;
			}
		} else {
			if (vf->vc_private) {
				vc_private_q_recycle(dev, vf->vc_private);
				vf->vc_private = NULL;
			}
			dma_buf_put((struct dma_buf *)file_vf);
			dma_fence_signal(vd_prepare_tmp->release_fence);
			dma_fence_put(vd_prepare_tmp->release_fence);
			vc_print(dev->index, PRINT_FENCE,
				"%s: release_fence = %px\n",
				__func__,
				vd_prepare_tmp->release_fence);
		}

		dev->fput_count++;
		vd_prepare_data_q_put(dev, vd_prepare_tmp);
	} else {
		vf_pop_display_q(dev, display_vf);
		videocomposer_vf_put(vf, op_arg);
	}
	//all the vframe param used must beforce fence release

	vc_print(dev->index, PRINT_FENCE, "%s end, put_count=%lld.\n", __func__, dev->fput_count);
}

static int vc_event_cb(int type, void *data, void *private_data)
{
	return 0;
}

static int vc_vf_states(struct vframe_states *states, void *op_arg)
{
	struct composer_dev *dev = (struct composer_dev *)op_arg;

	states->vf_pool_size = COMPOSER_READY_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = COMPOSER_READY_POOL_SIZE
		- kfifo_len(&dev->ready_q);
	states->buf_avail_num = kfifo_len(&dev->ready_q);
	return 0;
}

static const struct vframe_operations_s vc_vf_provider = {
	.peek = vc_vf_peek,
	.get = vc_vf_get,
	.put = vc_vf_put,
	.event_cb = vc_event_cb,
	.vf_states = vc_vf_states,
};

void vd_prepare_data_q_put(struct composer_dev *dev,
				struct vd_prepare_s *vd_prepare)
{
	if (!vd_prepare)
		return;

	if (!kfifo_put(&dev->vc_prepare_data_q, vd_prepare))
		vc_print(dev->index, PRINT_ERROR,
			"%s: vc_prepare_data_q is full!\n",
			__func__);
}

struct vd_prepare_s *vd_prepare_data_q_get(struct composer_dev *dev)
{
	struct vd_prepare_s *vd_prepare = NULL;

	if (!kfifo_get(&dev->vc_prepare_data_q, &vd_prepare)) {
		vc_print(dev->index, PRINT_ERROR,
			 "%s: get vc_prepare_data failed\n",
			 __func__);
		vd_prepare = NULL;
	} else {
		vd_prepare->src_frame = NULL;
		memset(&vd_prepare->dst_frame, 0, sizeof(struct vframe_s));
		vd_prepare->release_fence = NULL;
	}

	return vd_prepare;
}

int vd_render_index_get(struct composer_dev *dev)
{
	int receiver_id = 0;
	int render_index = 0;

	if (!dev) {
		pr_info("%s: dev is null.\n", __func__);
	} else {
		if (dev->index >= MAX_VD_LAYERS) {
			vc_print(dev->index, PRINT_ERROR,
				"%s: invalid param.\n",
				__func__);
		} else {
			receiver_id = get_receiver_id(dev->index);
			if (receiver_id >= 5)
				render_index = receiver_id - 3;
			else if (receiver_id <= 3)
				render_index = receiver_id - 2;
			else
				render_index = 0;
		}
		vc_print(dev->index, PRINT_ERROR,
			"%s: render_index is %d.\n",
			__func__, render_index);
	}
	return render_index;
}

int video_display_create_path(struct composer_dev *dev)
{
	int i = 0;
	char render_layer[16] = "";

	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	snprintf(dev->vfm_map_chain, VCOM_MAP_NAME_SIZE,
		 "%s %s", dev->vf_provider_name, render_layer);

	snprintf(dev->vfm_map_id, VCOM_MAP_NAME_SIZE,
		 "vcom-map-%d", dev->index);

	if (vfm_map_add(dev->vfm_map_id, dev->vfm_map_chain) < 0) {
		vc_print(dev->index, PRINT_ERROR,
			"%s: vcom pipeline map creation failed %s.\n",
			__func__, dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
		return -ENOMEM;
	}

	vf_provider_init(&dev->vc_vf_prov, dev->vf_provider_name,
			 &vc_vf_provider, dev);

	vf_reg_provider(&dev->vc_vf_prov);

	vf_notify_receiver(dev->vf_provider_name,
			   VFRAME_EVENT_PROVIDER_START, NULL);

	vsync_count[dev->index] = 0;
	dev->last_vsync_index = 0;
	dev->last_vf_index = 0xffffffff;
	dev->enable_pulldown = false;
	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		dev->patten_factor[i] = 0;
	dev->patten_factor_index = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	if (vd_pulldown_level && frc_get_video_latency()) {
		dev->enable_pulldown = true;
		vc_print(dev->index, PRINT_OTHER, "%s: enable pulldown\n", __func__);
	}
#endif
	return 0;
}

int video_display_release_path(struct composer_dev *dev)
{
	vf_unreg_provider(&dev->vc_vf_prov);

	if (dev->vfm_map_id[0]) {
		vfm_map_remove(dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
	}
	return 0;
}

