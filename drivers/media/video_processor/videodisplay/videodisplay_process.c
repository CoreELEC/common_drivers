// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
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
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/aml_sync_api.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <../../video_sink/video_priv.h>

#ifdef CONFIG_AMLOGIC_MEDIA_CODEC_MM
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#endif
#include <linux/dma-buf.h>
#include <linux/amlogic/ion.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/sched/clock.h>
#include <linux/sync_file.h>
#include <linux/ctype.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vfm/amlogic_fbc_hook_v1.h>
#include <linux/amlogic/media/media_proxy/AmlVideoUserdata.h>
#include <linux/amlogic/media/resource_mgr/resourcemanage.h>
#include "../../gdc/inc/api/gdc_api.h"
#include "../common/video_pp_common.h"
#include "../../common/uvm_process/meson_uvm_lcevc_processor.h"
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#include <linux/amlogic/media/di/di.h>
#endif
#include "videodisplay_process.h"
#ifdef CONFIG_AMLOGIC_MEDIA_VRR
#include <linux/amlogic/media/vrr/vrr.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
#include <linux/amlogic/media/frc/frc_common.h>
#endif
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
#include <linux/amlogic/media/dpss/dpss_frc.h>
#endif

#define WAIT_THREAD_STOPPED_TIMEOUT 20
#define WAIT_READY_Q_TIMEOUT 100
#define ADDR_VALUE_8G    0x200000000

static unsigned int force_composer;
static unsigned int force_composer_pip;
static int transform = -1;
static unsigned int vidc_debug;
static unsigned int vidc_pattern_debug;
static int last_index[MAX_VD_LAYERS];
static int last_frame_index;
static u32 full_axis = 1;
static u32 print_close;
static u32 receive_wait = 15;
static u32 margin_time = 2000;
static u32 max_width = 2560;
static u32 max_height = 1440;
static u32 rotate_width = 1280;
static u32 rotate_height = 720;
static u32 dewarp_rotate_width = 3840;
static u32 dewarp_rotate_height = 2160;
static u32 vicp_max_width = 3840;
static u32 vicp_max_height = 2160;
static u32 close_black;
static u32 debug_axis_pip;
static u32 debug_crop_pip;
static u32 composer_use_444;
static u32 reset_drop;
static u32 drop_cnt;
static u32 drop_cnt_pip;
static u32 vpp_drop_cnt;
static u32 receive_count;
static u32 receive_count_pip;
static u32 receive_new_count;
static u32 receive_new_count_pip;
static u32 total_get_count;
static u32 total_put_count;
static u64 nn_need_time = 15000;
static u64 nn_margin_time = 9000;
static u32 nn_bypass;
static u32 tv_fence_creat_count;
static u32 dump_vframe;
static u32 vicp_output_dev = 2; /*1 mif, 2 fbc, 3 fbc+mif*/
static u32 vicp_shrink_mode = 1; /*0 2x, 1 4x, 2 8x*/
static u32 force_comp_w;
static u32 force_comp_h;
static u32 lossy_compress_rate;//0: 100% copress; 1: 67% compress; 2: 83% compress
static u32 enable_frc_pattern;
static u32 low_latency_en;
static enum vc_fence_status last_buffer_status;
static struct vframe_s *last_normal_vf;
static struct videodisplay_dev *dev_array[MAX_VD_LAYERS];
static u32 vd_test_fps[MAX_VD_LAYERS];
static u64 vd_test_fps_val[MAX_VD_LAYERS] = {1, 1, 1};
static u64 vd_test_vsync_val[MAX_VD_LAYERS] = {1, 1, 1};
static struct timeval start_time[MAX_VD_LAYERS];
static struct timeval end_time[MAX_VD_LAYERS];
static int start_vsync_count[MAX_VD_LAYERS];
static int end_vsync_count[MAX_VD_LAYERS];
static u32 vd_pulldown_level = 2;
static u32 vd_max_hold_count = 300;
static u32 vd_set_frame_delay[MAX_VIDEODISPLAY_INSTANCE_NUM];
static u32 vpp_drop_count;
static u32 vd_dump_vframe;
static u32 composer_dev_choice; /*1 ge2d, 2 dewarp, 3 vicp*/
static struct vframe_s *current_display_vf;
static u32 dewarp_load_flag; /*0 dynamic load, 1 load bin file*/
static u32 new_afr_pulldown;
static int get_count[MAX_VIDEODISPLAY_INSTANCE_NUM];
static unsigned int continue_vsync_count[MAX_VIDEODISPLAY_INSTANCE_NUM];
static int actual_delay_count[MAX_VD_LAYERS];
static u32 vsync_pts_inc_scale[MAX_VD_LAYERS];
static u32 vsync_pts_inc_scale_base[MAX_VD_LAYERS];
static struct timeval vsync_time[MAX_VD_LAYERS];
static int patten_trace[MAX_VIDEODISPLAY_INSTANCE_NUM];
static int vsync_count[MAX_VIDEODISPLAY_INSTANCE_NUM];
static struct videodisplay_dev *mdev[MAX_VIDEODISPLAY_INSTANCE_NUM];
static bool vd_composer_en;
static u32 vd_drop_max_time = 200;
static u32 vd_drop_hdmi_count = 3;
static u32 vd_drop_org_i = 0x8; /*1:vdin, 2:dtv, 4:dec, 8:dpss*/
static bool mute_vpp_vd1;
static u32 mute_vsync_count;
static bool enable_new_afr_pattern;
static struct mediasync_cb *mediasync_callback;
static DEFINE_MUTEX(videodisplay_mutex);

#define to_dst_buf(vf)	container_of(vf, struct dst_buf_t, frame)
#define IS_DI_PRELINK_BYPASS(di_flag) ((di_flag) & DI_FLAG_DI_PVPPLINK_BYPASS)

static void vd_dump_afbc_vf(u8 *data_y, u8 *data_uv, struct vframe_s *vf, int flag)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	loff_t pos;

	if (!vf)
		return;

	/*use flag to distinguish src and dst vframe*/
	if (flag == 0)
		snprintf(name_buf, sizeof(name_buf), "/sdcard/src_afbc_vframe.yuv");
	else
		snprintf(name_buf, sizeof(name_buf), "/sdcard/dst_afbc_vframe.yuv");

	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vf->compWidth * vf->compHeight;
	data_size_uv = vf->compWidth * vf->compHeight / 2;
	pr_info("dump: data_size_y =%d, data_size_uv=%d\n", data_size_y, data_size_uv);

	if (!data_y || !data_uv) {
		pr_err("%s: vmap failed.\n", __func__);
		return;
	}
	pos = fp->f_pos;
	kernel_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	pos = fp->f_pos;
	kernel_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	filp_close(fp, NULL);
#endif
}

static void vd_dump_vf(struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp = NULL;
	char name_buf[32];
	int data_size_y, data_size_uv;
	u8 *data_y;
	u8 *data_uv;
	loff_t pos;

	if (!vf)
		return;

	snprintf(name_buf, sizeof(name_buf), "/sdcard/dst_vframe.yuv");
	fp = filp_open(name_buf, O_CREAT | O_RDWR, 0644);
	if (IS_ERR(fp))
		return;
	data_size_y = vf->canvas0_config[0].width *
			vf->canvas0_config[0].height;
	data_size_uv = vf->canvas0_config[1].width *
			vf->canvas0_config[1].height;
	data_y = codec_mm_vmap(vf->canvas0_config[0].phy_addr, data_size_y);
	data_uv = codec_mm_vmap(vf->canvas0_config[1].phy_addr, data_size_uv);
	if (!data_y || !data_uv) {
		pr_err("%s: vmap failed.\n", __func__);
		return;
	}
	pos = fp->f_pos;
	kernel_write(fp, data_y, data_size_y, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_y, data_y);
	codec_mm_unmap_phyaddr(data_y);
	pos = fp->f_pos;
	kernel_write(fp, data_uv, data_size_uv, &pos);
	fp->f_pos = pos;
	pr_info("%s: write %u size to addr%p\n",
		__func__, data_size_uv, data_uv);
	codec_mm_unmap_phyaddr(data_uv);
	filp_close(fp, NULL);
#endif
}

static int vd_vframe_afbc_soft_decode(struct vframe_s *vf, int flag)
{
	int ret, i, j, y_size, free_cnt;
	short *planes[4];
	short *y_src, *u_src, *v_src, *s2c, *s2c1;
	u8 *tmp, *tmp1;
	u8 *y_dst, *vu_dst;
	int bit_10;
	struct timeval start, end;
	unsigned long time_use = 0;
	struct fbc_decoder_param param;

	if ((vf->bitdepth & BITDEPTH_YMASK)  == BITDEPTH_Y10)
		bit_10 = 1;
	else
		bit_10 = 0;

	u32 p_data_size = vf->compWidth * vf->compHeight  * 3 / 2;
	u8 *p = vmalloc(p_data_size);

	if (!p)
		return -1;

	y_size = vf->compWidth * vf->compHeight * sizeof(short);
	pr_info("width: %d, height: %d, compWidth: %u, compHeight: %u.\n",
		 vf->width, vf->height, vf->compWidth, vf->compHeight);
	for (i = 0; i < 4; i++) {
		planes[i] = vmalloc(y_size);
		if (!planes[i]) {
			free_cnt = i;
			pr_err("vmalloc fail in %s\n", __func__);
			vfree(p);
			goto free;
		}
		pr_info("plane %d size: %d, vmalloc addr: %p.\n",
			i, y_size, planes[i]);
	}
	free_cnt = 4;

	do_gettimeofday(&start);
	param.compHeadAddr = vf->compHeadAddr;
	param.compWidth = vf->compWidth;
	param.compHeight = vf->compHeight;
	param.bitdepth = vf->bitdepth;
#ifdef CONFIG_AMLOGIC_UVM_CORE
	ret = AMLOGIC_FBC_vframe_decoder_v1((void **)planes, &param, 0, 0);
#else
	ret = -1;
#endif
	if (ret < 0) {
		pr_err("amlogic_fbc_lib.ko error %d", ret);
		vfree(p);
		goto free;
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("FBC Decompress time: %ldms\n", time_use);

	y_src = planes[0];
	u_src = planes[1];
	v_src = planes[2];

	y_dst = p;
	vu_dst = p + vf->compWidth * vf->compHeight;

	do_gettimeofday(&start);
	for (i = 0; i < vf->compHeight; i++) {
		for (j = 0; j < vf->compWidth; j++) {
			s2c = y_src + j;
			tmp = (u8 *)(s2c);
			if (bit_10)
				*(y_dst + j) = *s2c >> 2;
			else
				*(y_dst + j) = tmp[0];
		}

			y_dst += vf->compWidth;
			y_src += vf->compWidth;
	}

	for (i = 0; i < (vf->compHeight / 2); i++) {
		for (j = 0; j < vf->compWidth; j += 2) {
			s2c = v_src + j / 2;
			s2c1 = u_src + j / 2;
			tmp = (u8 *)(s2c);
			tmp1 = (u8 *)(s2c1);

			if (bit_10) {
				*(vu_dst + j) = *s2c >> 2;
				*(vu_dst + j + 1) = *s2c1 >> 2;
			} else {
				*(vu_dst + j) = tmp[0];
				*(vu_dst + j + 1) = tmp1[0];
			}
		}
		vu_dst += vf->compWidth;
		u_src += (vf->compWidth / 2);
		v_src += (vf->compWidth / 2);
	}

	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	pr_debug("bitblk time: %ldms\n", time_use);

	y_dst = p;
	vu_dst = p + vf->compWidth * vf->compHeight;
	vd_dump_afbc_vf(y_dst, vu_dst, vf, flag);
	vfree(p);
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
	return 0;

free:
	for (i = 0; i < free_cnt; i++)
		vfree(planes[i]);
	return -1;
}

static void calculate_fps_and_vsync(struct timeval *start, struct timeval *end,
	u32 *start_count, u32 *end_count, int i)
{
	u64 diff_time = 0;

	diff_time = (1000000L * (end->tv_sec - start->tv_sec) +
		(end->tv_usec - start->tv_usec));
	vd_test_vsync_val[i] = div64_u64(100 * diff_time, (*end_count - *start_count));
	vd_test_fps_val[i] = div64_u64((u64)100000 * 100000 * 1000, vd_test_vsync_val[i]);
}

static void ext_controls(void)
{
	if (current_display_vf->type & VIDTYPE_COMPRESS)
		vd_vframe_afbc_soft_decode(current_display_vf, 1);
	else
		vd_dump_vf(current_display_vf);
}

void set_debug_flag_val(enum videodisplay_debug_class_type debug_type, int value)
{
	switch (debug_type) {
	case VD_DEBUG_CLASS_AXIX_PIP:
		debug_axis_pip = value;
		break;
	case VD_DEBUG_CLASS_CROP_PIP:
		debug_crop_pip = value;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER:
		force_composer = value;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER_PIP:
		force_composer_pip = value;
		break;
	case VD_DEBUG_CLASS_TRANSFORM:
		transform = value;
		break;
	case VD_DEBUG_CLASS_VIDC:
		vidc_debug = value;
		break;
	case VD_DEBUG_CLASS_VIDC_PATTERN:
		vidc_pattern_debug = value;
		break;
	case VD_DEBUG_CLASS_FULL_AXIS:
		full_axis = value;
		break;
	case VD_DEBUG_CLASS_PRINT_CLOSE:
		print_close = value;
		break;
	case VD_DEBUG_CLASS_RECEIVE_WAIT:
		receive_wait = value;
		break;
	case VD_DEBUG_CLASS_MARGIN_TIME:
		margin_time = value;
		break;
	case VD_DEBUG_CLASS_MAX_WIDTH:
		max_width = value;
		break;
	case VD_DEBUG_CLASS_MAX_HEIGHT:
		max_height = value;
		break;
	case VD_DEBUG_CLASS_ROTATE_WIDTH:
		rotate_width = value;
		break;
	case VD_DEBUG_CLASS_ROTATE_HEIGHT:
		rotate_height = value;
		break;
	case VD_DEBUG_CLASS_DEWARP_ROTATE_WIDTH:
		dewarp_rotate_width = value;
		break;
	case VD_DEBUG_CLASS_DEWARP_ROTATE_HEIGHT:
		dewarp_rotate_height = value;
		break;
	case VD_DEBUG_CLASS_CLOSE_BLACK:
		close_black = value;
		break;
	case VD_DEBUG_CLASS_COMPOSER_USE_444:
		composer_use_444 = value;
		break;
	case VD_DEBUG_CLASS_RESET_DROP:
		reset_drop = value;
		break;
	case VD_DEBUG_CLASS_NN_NEED_TIME:
		nn_need_time = value;
		break;
	case VD_DEBUG_CLASS_NN_MARGIN_TIME:
		nn_margin_time = value;
		break;
	case VD_DEBUG_CLASS_NN_BYPASS:
		nn_bypass = value;
		break;
	case VD_DEBUG_CLASS_DUMP_VFRAME:
		dump_vframe = value;
		break;
	case VD_DEBUG_CLASS_PULLDOWN_LEVEL:
		vd_pulldown_level = value;
		break;
	case VD_DEBUG_CLASS_MAX_HOLD_COUNT:
		vd_max_hold_count = value;
		break;
	case VD_DEBUG_CLASS_SET_FRAME_DELAY:
		//vd_set_frame_delay = value;
		break;
	case VD_DEBUG_CLASS_VD_DUMP_VFRAME:
		if (vd_dump_vframe != value) {
			ext_controls();
			vd_dump_vframe = value;
		}
		break;
	case VD_DEBUG_CLASS_ACTUAL_DELAY_COUNT:
		//actual_delay_count = value;
		break;
	case VD_DEBUG_CLASS_VICP_OUTPUT_DEV:
		vicp_output_dev = value;
		break;
	case VD_DEBUG_CLASS_VICP_SHRINK_MODE:
		vicp_shrink_mode = value;
		break;
	case VD_DEBUG_CLASS_VICP_MAX_WIDTH:
		vicp_max_width = value;
		break;
	case VD_DEBUG_CLASS_VICP_MAX_HEIGHT:
		vicp_max_height = value;
		break;
	case VD_DEBUG_CLASS_COMPOSER_DEV_CHOICE:
		composer_dev_choice = value;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER_WIDTH:
		force_comp_w = value;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER_HEIGHT:
		force_comp_h = value;
		break;
	case VD_DEBUG_CLASS_VD_TEST_FPS:
		//vd_test_fps = value;
		break;
	case VD_DEBUG_CLASS_DEWARP_LOAD_FLAG:
		dewarp_load_flag = value;
		break;
	case VD_DEBUG_CLASS_LOSSY_COMPRESS_RATE:
		lossy_compress_rate = value;
		break;
	case VD_DEBUG_CLASS_FRC_PATTERN_EN:
		enable_frc_pattern = value;
		break;
	case VD_DEBUG_CLASS_BUF_STATUS:
		//buf_status = value;
		break;
	case VD_DEBUG_CLASS_USE_LOW_LATENCY:
		low_latency_en = value;
		break;
	case VD_DEBUG_CLASS_VD_COMPOSER_EN:
		vd_composer_en = value;
		break;
	case VD_DEBUG_CLASS_DROP_ORG_I:
		vd_drop_org_i = value;
		break;
	case VD_DEBUG_CLASS_DROP_HDMI_COUNT:
		vd_drop_hdmi_count = value;
		break;
	default:
		pr_info("%s: invalid debug type.\n", __func__);
		break;
	}
}

int get_debug_flag_val(enum videodisplay_debug_class_type debug_type)
{
	int ret = -1;

	switch (debug_type) {
	case VD_DEBUG_CLASS_AXIX_PIP:
		ret = debug_axis_pip;
		break;
	case VD_DEBUG_CLASS_CROP_PIP:
		ret = debug_crop_pip;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER:
		ret = force_composer;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER_PIP:
		ret = force_composer_pip;
		break;
	case VD_DEBUG_CLASS_TRANSFORM:
		ret = transform;
		break;
	case VD_DEBUG_CLASS_VIDC:
		ret = vidc_debug;
		break;
	case VD_DEBUG_CLASS_VIDC_PATTERN:
		ret = vidc_pattern_debug;
		break;
	case VD_DEBUG_CLASS_FULL_AXIS:
		ret = full_axis;
		break;
	case VD_DEBUG_CLASS_PRINT_CLOSE:
		ret = print_close;
		break;
	case VD_DEBUG_CLASS_RECEIVE_WAIT:
		ret = receive_wait;
		break;
	case VD_DEBUG_CLASS_MARGIN_TIME:
		ret = margin_time;
		break;
	case VD_DEBUG_CLASS_MAX_WIDTH:
		ret = max_width;
		break;
	case VD_DEBUG_CLASS_MAX_HEIGHT:
		ret = max_height;
		break;
	case VD_DEBUG_CLASS_ROTATE_WIDTH:
		ret = rotate_width;
		break;
	case VD_DEBUG_CLASS_ROTATE_HEIGHT:
		ret = rotate_height;
		break;
	case VD_DEBUG_CLASS_DEWARP_ROTATE_WIDTH:
		ret = dewarp_rotate_width;
		break;
	case VD_DEBUG_CLASS_DEWARP_ROTATE_HEIGHT:
		ret = dewarp_rotate_height;
		break;
	case VD_DEBUG_CLASS_CLOSE_BLACK:
		ret = close_black;
		break;
	case VD_DEBUG_CLASS_COMPOSER_USE_444:
		ret = composer_use_444;
		break;
	case VD_DEBUG_CLASS_RESET_DROP:
		ret = reset_drop;
		break;
	case VD_DEBUG_CLASS_DROP_COUNT:
		ret = drop_cnt;
		break;
	case VD_DEBUG_CLASS_VPP_DROP_COUNT:
		ret = vpp_drop_cnt;
		break;
	case VD_DEBUG_CLASS_LAST_FRAME_INDEX:
		ret = last_frame_index;
		break;
	case VD_DEBUG_CLASS_DROP_COUNT_PIP:
		ret = drop_cnt_pip;
		break;
	case VD_DEBUG_CLASS_RECEIVE_COUNT:
		ret = receive_count;
		break;
	case VD_DEBUG_CLASS_RECEIVE_COUNT_PIP:
		ret = receive_count_pip;
		break;
	case VD_DEBUG_CLASS_RECEIVE_NEW_COUNT:
		ret = receive_new_count;
		break;
	case VD_DEBUG_CLASS_RECEIVE_NEW_COUNT_PIP:
		ret = receive_new_count_pip;
		break;
	case VD_DEBUG_CLASS_TOTAL_GET_COUNT:
		ret = total_get_count;
		break;
	case VD_DEBUG_CLASS_TOTAL_PUT_COUNT:
		ret = total_put_count;
		break;
	case VD_DEBUG_CLASS_NN_NEED_TIME:
		ret = nn_need_time;
		break;
	case VD_DEBUG_CLASS_NN_MARGIN_TIME:
		ret = nn_margin_time;
		break;
	case VD_DEBUG_CLASS_NN_BYPASS:
		ret = nn_bypass;
		break;
	case VD_DEBUG_CLASS_FENCE_CREATE_COUNT:
		ret = tv_fence_creat_count;
		break;
	case VD_DEBUG_CLASS_DUMP_VFRAME:
		ret = dump_vframe;
		break;
	case VD_DEBUG_CLASS_PULLDOWN_LEVEL:
		ret = vd_pulldown_level;
		break;
	case VD_DEBUG_CLASS_MAX_HOLD_COUNT:
		ret = vd_max_hold_count;
		break;
	case VD_DEBUG_CLASS_SET_FRAME_DELAY:
		//ret = vd_set_frame_delay;
		break;
	case VD_DEBUG_CLASS_VD_DUMP_VFRAME:
		ret = vd_dump_vframe;
		break;
	case VD_DEBUG_CLASS_ACTUAL_DELAY_COUNT:
		//ret = actual_delay_count;
		break;
	case VD_DEBUG_CLASS_VICP_OUTPUT_DEV:
		ret = vicp_output_dev;
		break;
	case VD_DEBUG_CLASS_VICP_SHRINK_MODE:
		ret = vicp_shrink_mode;
		break;
	case VD_DEBUG_CLASS_VICP_MAX_WIDTH:
		ret = vicp_max_width;
		break;
	case VD_DEBUG_CLASS_VICP_MAX_HEIGHT:
		ret = vicp_max_height;
		break;
	case VD_DEBUG_CLASS_COMPOSER_DEV_CHOICE:
		ret = composer_dev_choice;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER_WIDTH:
		ret = force_comp_w;
		break;
	case VD_DEBUG_CLASS_FORCE_COMPOSER_HEIGHT:
		ret = force_comp_h;
		break;
	case VD_DEBUG_CLASS_VD_TEST_FPS:
		//ret = vd_test_fps;
		break;
	case VD_DEBUG_CLASS_DEWARP_LOAD_FLAG:
		ret = dewarp_load_flag;
		break;
	case VD_DEBUG_CLASS_LOSSY_COMPRESS_RATE:
		ret = lossy_compress_rate;
		break;
	case VD_DEBUG_CLASS_FRC_PATTERN_EN:
		ret = enable_frc_pattern;
		break;
	case VD_DEBUG_CLASS_BUF_STATUS:
		//ret = buf_status;
		break;
	case VD_DEBUG_CLASS_USE_LOW_LATENCY:
		ret = low_latency_en;
		break;
	case VD_DEBUG_CLASS_VD_COMPOSER_EN:
		ret = vd_composer_en;
		break;
	case VD_DEBUG_CLASS_DROP_ORG_I:
		ret = vd_drop_org_i;
		break;
	case VD_DEBUG_CLASS_DROP_HDMI_COUNT:
		ret = vd_drop_hdmi_count;
		break;
	default:
		pr_info("%s: invalid debug type.\n", __func__);
		break;
	}

	return ret;
}

#ifndef CONFIG_AMLOGIC_UVM_CORE
int dmabuf_put_vframe(struct dma_buf *dmabuf)
{
	return 0;
}

bool is_valid_mod_type(struct dma_buf *dmabuf,
		       enum uvm_hook_mod_type type)
{
	return false;
}

int uvm_put_hook_mod(struct dma_buf *dmabuf, int type)
{
	return 0;
}

struct uvm_hook_mod *uvm_get_hook_mod(struct dma_buf *dmabuf,
				      int type)
{
	return NULL;
}
#endif

#ifndef CONFIG_AMLOGIC_MEDIA_GE2D
struct ge2d_context_s *create_ge2d_work_queue(void)
{
	return NULL;
}

int ge2d_context_config_ex(struct ge2d_context_s *context,
			   struct config_para_ex_s *ge2d_config)
{
	return -1;
}
#endif

static void detect_vf_type(struct frame_info_t *frame_info, bool *is_dec_vf_ptr,
				bool *is_v4l_vf_ptr)
{
	if (!is_dec_vf_ptr || !is_v4l_vf_ptr) {
		pr_info("is_dec_vf or is_v4l_vf err.\n");
		return;
	}
	if (frame_info->source_type == SOURCE_HWC_CREAT_ION) {
		*is_dec_vf_ptr = false;
		*is_v4l_vf_ptr = false;
		return;
	}
	*is_dec_vf_ptr = is_valid_mod_type(frame_info->dmabuf, VF_SRC_DECODER);
	*is_v4l_vf_ptr = is_valid_mod_type(frame_info->dmabuf, VF_PROCESS_V4LVIDEO);
}

static void *video_timeline_create(struct videodisplay_dev *dev)
{
	const char *tl_name = "videodisplay_timeline_0";

	if (dev->index == 0)
		tl_name = "videodisplay_timeline_0";
	else if (dev->index == 1)
		tl_name = "videodisplay_timeline_1";
	else if (dev->index == 2)
		tl_name = "videodisplay_timeline_2";

	if (IS_ERR_OR_NULL(dev->video_timeline)) {
		dev->cur_streamline_val = 0;
		dev->video_timeline = aml_sync_create_timeline(tl_name);
		vd_print(dev->index, PRINT_FENCE,
			 "timeline create tlName =%s, video_timeline=%p\n",
			 tl_name, dev->video_timeline);
	}

	return dev->video_timeline;
}

static void video_timeline_increase(struct videodisplay_dev *dev,
				    unsigned int value)
{
	aml_sync_inc_timeline(dev->video_timeline, value);
	dev->fence_release_count += value;
	vd_print(dev->index, PRINT_FENCE,
		"receive_cnt=%lld,new_cnt=%lld,fen_creat_cnt=%lld,fen_release_cnt=%lld\n",
		dev->received_count,
		dev->received_new_count,
		dev->fence_creat_count,
		dev->fence_release_count);
}

static void video_timeline_update(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	int normal_frame_count = 0;
	struct vframe_s *new_display_vf;
	enum vc_fence_status buffer_status;
	bool rendered;
	int repeat_count;

	if (!vf)
		return;

	rendered = vf->rendered;
	repeat_count = vf->repeat_count;

	switch (vf->dec_fence_status) {
	case DEC_FENCE_SUCCESS:
		vd_print(dev->index, PRINT_FENCE,
			"%s: dec fence success, ready to put dec fence:%px\n",
			__func__, vf->fence);
		dma_fence_put(vf->fence);
		if (last_buffer_status == VC_FENCE_DEC_ERR)
			buffer_status = VC_FENCE_RELEASED;
		else if (rendered)
			buffer_status = VC_FENCE_NORMAL;
		else
			buffer_status = VC_FENCE_WAIT;
		break;
	case DEC_FENCE_INVALID:
		if (rendered)
			buffer_status = VC_FENCE_NORMAL;
		else
			buffer_status = VC_FENCE_WAIT;
		break;
	case DEC_FENCE_ERR:
		buffer_status = VC_FENCE_DEC_ERR;
		break;
	default:
		buffer_status = VC_FENCE_INVALID;
		break;
	}

	switch (buffer_status) {
	case VC_FENCE_DEC_ERR:
		//release the normal vf when the first error vf appears
		if (last_normal_vf == current_display_vf) {
			new_display_vf = NULL;
		} else {
			last_normal_vf = current_display_vf;
			new_display_vf = last_normal_vf;
		}
		if (new_display_vf && new_display_vf->dec_fence_status == DEC_FENCE_SUCCESS) {
			normal_frame_count = 1 + new_display_vf->repeat_count;
			vd_print(dev->index, PRINT_OTHER,
				"err vf, need drop frame_index:%d, count:%d",
				new_display_vf->frame_index, normal_frame_count);
		}
		vd_print(dev->index, PRINT_OTHER,
			"put: frame_index:%d error, fence released\n",
			vf->frame_index);
		video_timeline_increase(dev, repeat_count + normal_frame_count +
			1 + dev->drop_frame_count);
		dev->drop_frame_count = 0;
		break;
	case VC_FENCE_RELEASED:
		vd_print(dev->index, PRINT_PERFORMANCE | PRINT_FENCE,
			 "put: frame_index: %d, err frame already put fence\n", vf->frame_index);
		break;
	case VC_FENCE_NORMAL:
		video_timeline_increase(dev, repeat_count
					+ 1 + dev->drop_frame_count);
		dev->drop_frame_count = 0;
		break;
	case VC_FENCE_WAIT:
		dev->drop_frame_count += repeat_count + 1;
		vd_print(dev->index, PRINT_PERFORMANCE | PRINT_FENCE,
			 "put: drop repeat_count=%d\n", repeat_count);
		break;
	default:
		vd_print(dev->index, PRINT_ERROR, "error, fence status unknown\n");
		break;
	}

	last_buffer_status = buffer_status;
}

static int vd_init_dewarp_buffer(struct videodisplay_dev *dev)
{
	if (IS_ERR_OR_NULL(dev)) {
		pr_info("%s: dev is NULL.\n", __func__);
		return -1;
	}

	return 0;
}

static int vd_init_vicp_buffer(struct videodisplay_dev *dev)
{
	if (IS_ERR_OR_NULL(dev)) {
		pr_info("%s: dev is NULL.\n", __func__);
		return -1;
	}

	return 0;
}

static int vd_init_ge2d_buffer(struct videodisplay_dev *dev)
{
	if (IS_ERR_OR_NULL(dev)) {
		pr_info("%s: dev is NULL.\n", __func__);
		return -1;
	}

	return 0;
}

static int video_composer_init_buffer(struct videodisplay_dev *dev)
{
	int ret = -1;

	switch (dev->buffer_status) {
	case UNINITIAL:/*not config*/
		break;
	case INIT_SUCCESS:/*config before , return ok*/
		return 0;
	case INIT_ERROR:/*config fail, won't retry , return failure*/
		return -1;
	default:
		return -1;
	}

	//do buf init
	if (dev->dev_choice == COMPOSER_WITH_DEWARP) {
		ret = vd_init_dewarp_buffer(dev);
	} else if (dev->dev_choice == COMPOSER_WITH_VICP) {
		ret = vd_init_vicp_buffer(dev);
	} else if (dev->dev_choice == COMPOSER_WITH_GE2D) {
		ret = vd_init_ge2d_buffer(dev);
	} else {
		vd_print(dev->index, PRINT_ERROR, "composer device choice error!\n");
		ret = -1;
	}

	if (ret < 0)
		vd_print(dev->index, PRINT_ERROR, "config vc buf failed!\n");
	else
		dev->buffer_status = INIT_SUCCESS;

	return ret;
}

static void video_composer_uninit_buffer(struct videodisplay_dev *dev)
{
	struct videodisplay_port_s *vd_port;

	if (dev->buffer_status == UNINITIAL) {
		vd_print(dev->index, PRINT_OTHER, "%s already finished!\n", __func__);
		return;
	}

	vd_port = videodisplay_get_port(dev->index);
	if (!vd_port) {
		vd_print(dev->index, PRINT_ERROR, "get vd port failed.\n");
		return;
	}

	//do buf release

	dev->dev_choice = COMPOSER_WITH_UNINITIAL;
	dev->last_dst_vf = NULL;

	INIT_KFIFO(dev->free_q);
	kfifo_reset(&dev->free_q);
}

static struct file_private_data *vc_get_file_private(struct videodisplay_dev *dev,
						      struct file *file_vf)
{
	struct file_private_data *file_private_data;
	struct uvm_hook_mod *uhmod;
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
	bool is_v4lvideo_fd = false;
#endif

	if (!file_vf) {
		vd_print(dev->index, PRINT_ERROR, "get_file_private_data fail\n");
		return NULL;
	}
#ifdef CONFIG_AMLOGIC_V4L_VIDEO3
	if (is_v4lvideo_buf_file(file_vf))
		is_v4lvideo_fd = true;

	if (is_v4lvideo_fd) {
		file_private_data = (struct file_private_data *)(file_vf->private_data);
		return file_private_data;
	}
#endif

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data), VF_PROCESS_V4LVIDEO);
	if (!uhmod) {
		vd_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL\n");
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vd_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL\n");
		return NULL;
	}
	file_private_data = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data), VF_PROCESS_V4LVIDEO);

	return file_private_data;
}

static void vd_private_q_init(struct videodisplay_dev *dev)
{
	int i;

	INIT_KFIFO(dev->vc_private_q);
	kfifo_reset(&dev->vc_private_q);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++) {
		dev->vc_private[i].index = i;
		dev->vc_private[i].flag = 0;
		dev->vc_private[i].srout_data = NULL;
		dev->vc_private[i].src_vf = NULL;
		dev->vc_private[i].vsync_index = 0;
		dev->vc_private[i].dalton_info = NULL;
		dev->vc_private[i].aiface_info = NULL;
		dev->vc_private[i].aicolor_info = NULL;
		dev->vc_private[i].present_fence = NULL;
		if (!kfifo_put(&dev->vc_private_q, &dev->vc_private[i]))
			vd_print(dev->index, PRINT_ERROR, "vc_private_q is full!\n");
	}
}

static void vd_private_q_recycle(struct videodisplay_dev *dev,
				struct video_composer_private *vc_private)
{
	if (!vc_private)
		return;

	memset(vc_private, 0, sizeof(struct video_composer_private));

	if (!kfifo_put(&dev->vc_private_q, vc_private))
		vd_print(dev->index, PRINT_ERROR, "vc_private_q is full!\n");
}

static struct video_composer_private *vd_private_q_pop(struct videodisplay_dev *dev)
{
	struct video_composer_private *vc_private = NULL;

	if (!kfifo_get(&dev->vc_private_q, &vc_private)) {
		vd_print(dev->index, PRINT_ERROR, "%s: get vc_private_q failed\n", __func__);
		vc_private = NULL;
	} else {
		vc_private->flag = 0;
		vc_private->srout_data = NULL;
		vc_private->src_vf = NULL;
		vc_private->dalton_info = NULL;
		vc_private->aiface_info = NULL;
		vc_private->aicolor_info = NULL;
		vc_private->present_fence = NULL;
	}

	return vc_private;
}

static void vf_pop_display_q(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	struct vframe_s *dis_vf = NULL;
	int k = kfifo_len(&dev->display_q);

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dis_vf)) {
			if (dis_vf == vf)
				break;
			if (!kfifo_put(&dev->display_q, dis_vf))
				vd_print(dev->index, PRINT_ERROR,
					"%s: display_q is full!\n",
					__func__);
		}
		k--;
		if (k < 0) {
			vd_print(dev->index, PRINT_ERROR,
				"%s: can find vf in display_q.\n",
				__func__);
			break;
		}
	}
}

static void display_q_uninit(struct videodisplay_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *dst_vf = NULL;
	int i, repeat_count;
	struct file *file_vf;
	struct vd_prepare_s *vd_prepare;
	struct mbp_buffer_info_t *mpb_buf = NULL;

	vd_print(dev->index, PRINT_QUEUE_STATUS,
		"%s: display_q len=%d\n",
		__func__,
		kfifo_len(&dev->display_q));

	while (kfifo_len(&dev->display_q) > 0) {
		if (kfifo_get(&dev->display_q, &dst_vf)) {
			if (dst_vf->type_ext & VIDTYPE_EXT_LCEVC)
				vf = dst_vf->enhance_vf;
			else
				vf = dst_vf;
			vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
			if (IS_ERR_OR_NULL(vd_prepare)) {
				vd_print(dev->index, PRINT_ERROR,
					"%s: prepare is NULL.\n",
					__func__);
				return;
			}

			if (vf->type_ext & VIDTYPE_EXT_MOSAIC_22) {
				vd_print(dev->index, PRINT_OTHER, "%s: mosaic_22 mode\n", __func__);
				for (i = 0; i < 4; i++) {
					file_vf = vf->vc_private->mosaic_vf[i]->file_vf;
					if (file_vf) {
						fput(file_vf);
						total_put_count++;
						dev->fput_count++;
					} else {
						vd_print(dev->index, PRINT_ERROR,
							"%s error: i=%d fput fail\n",
							__func__,
							i);
					}
				}

				continue;
			}

			repeat_count = vf->repeat_count;
			if (vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS) {
				file_vf = vf->file_vf;
				vd_print(dev->index, PRINT_FENCE,
					"%s: frame_index=%d, repeat_count=%d.\n",
					__func__,
					vf->frame_index,
					repeat_count);
				for (i = 0; i <= repeat_count; i++) {
					if (vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA) {
						vd_print(dev->index, PRINT_FENCE,
							"%s:put dma buffer!!!\n",
							__func__);
						mpb_buf = (struct mbp_buffer_info_t *)file_vf;
						vf->vc_private->unlock_buffer_cb(mpb_buf);

					} else {
						vd_print(dev->index, PRINT_FENCE,
							"%s: release_fence = %px\n",
							__func__,
							vd_prepare->release_fence);
						dma_buf_put((struct dma_buf *)file_vf);
						dma_fence_signal(vd_prepare->release_fence);
						dma_fence_put(vd_prepare->release_fence);
					}

					dev->fput_count++;
					total_put_count++;
				}
			} else if (!(vf->flag & VFRAME_FLAG_VIDEO_COMPOSER)) {
				vd_print(dev->index, PRINT_ERROR,
					"%s: flag is null, frame_index=%d\n",
					__func__,
					vf->frame_index);
			}
		}
	}
}

static void receive_q_uninit(struct videodisplay_dev *dev)
{
	int i = 0;
	int queue_count = 0;
	int num = 0;
	struct received_frames_t *received_frames = NULL;
	struct dma_fence *release_fence;

	queue_count = kfifo_len(&dev->receive_q);
	vd_print(dev->index, PRINT_QUEUE_STATUS, "%s: receive_q len=%d\n", __func__, queue_count);

	while (kfifo_len(&dev->receive_q) > 0) {
		vd_print(dev->index, PRINT_QUEUE_STATUS, "%s: receive_q len=%d\n",
			__func__, kfifo_len(&dev->receive_q));
		if (kfifo_get(&dev->receive_q, &received_frames)) {
			num = received_frames->frames_info.layer_index;
			release_fence = received_frames->frames_info.frame_info[num].release_fence;
			dma_fence_signal(release_fence);
			dma_fence_put(release_fence);
		}
	}

	for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++)
		atomic_set(&dev->received_frames[i].on_use, false);
}

static void ready_q_uninit(struct videodisplay_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *dst_vf = NULL;
	struct file *file_vf;
	struct vd_prepare_s *vd_prepare;
	struct mbp_buffer_info_t *mpb_buf = NULL;
	int i, repeat_count;

	vd_print(dev->index, PRINT_QUEUE_STATUS, "%s: ready_q len=%d\n",
		__func__,
		kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &dst_vf)) {
			if (!dst_vf) {
				vd_print(dev->index, PRINT_ERROR, "%s: dis_vf is NULL\n", __func__);
				break;
			}

			if (dst_vf->type_ext & VIDTYPE_EXT_LCEVC)
				vf = dst_vf->enhance_vf;
			else
				vf = dst_vf;

			vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
			if (IS_ERR_OR_NULL(vd_prepare)) {
				vd_print(dev->index, PRINT_ERROR,
					"%s: prepare is NULL.\n",
					__func__);
				return;
			}

			if (vf->vc_private && vf->vc_private->srout_data) {
				if (vf->vc_private->srout_data->nn_status == NN_DONE)
					vf->vc_private->srout_data->nn_status = NN_DISPLAYED;
			}

			if (!(vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_BYPASS)) {
				vd_print(dev->index, PRINT_OTHER,
					"%s: no VD flag, frame_index=%d\n",
					__func__,
					vf->frame_index);
				continue;
			}

			file_vf = vf->file_vf;
			repeat_count = vf->repeat_count;
			vd_print(dev->index, PRINT_FENCE,
				"%s: frame_index=%d, repeat_count=%d.\n",
				__func__,
				vf->frame_index,
				repeat_count);
			for (i = 0; i <= repeat_count; i++) {
				if (vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA) {
					vd_print(dev->index, PRINT_FENCE,
						"%s:put dma buffer!!!\n",
						__func__);
					mpb_buf = (struct mbp_buffer_info_t *)file_vf;
					if (vf->vc_private)
						vf->vc_private->unlock_buffer_cb(mpb_buf);
					else
						vd_print(dev->index, PRINT_FENCE,
							"%s: vc_private is NULL.\n",
							__func__);
				} else {
					vd_print(dev->index, PRINT_FENCE,
						"%s: release_fence = %px\n",
						__func__,
						vd_prepare->release_fence);
					dma_fence_put(vd_prepare->release_fence);
					dma_fence_signal(vd_prepare->release_fence);
					dma_buf_put((struct dma_buf *)file_vf);
				}

				dev->fput_count++;
				total_put_count++;
			}
		}
	}
}

static void vd_prepare_data_q_put(struct videodisplay_dev *dev,
	struct vd_prepare_s *vd_prepare)
{
	if (!vd_prepare)
		return;

	if (!kfifo_put(&dev->vc_prepare_data_q, vd_prepare))
		vd_print(dev->index, PRINT_ERROR,
			"%s: vc_prepare_data_q is full!\n",
			__func__);
}

static struct vd_prepare_s *vd_prepare_data_q_get(struct videodisplay_dev *dev)
{
	struct vd_prepare_s *vd_prepare = NULL;

	if (!kfifo_get(&dev->vc_prepare_data_q, &vd_prepare)) {
		vd_print(dev->index, PRINT_ERROR, "%s: get vc_prepare_data failed\n", __func__);
		vd_prepare = NULL;
	} else {
		vd_prepare->src_frame = NULL;
		memset(&vd_prepare->dst_frame, 0, sizeof(struct vframe_s));
		vd_prepare->release_fence = NULL;
	}

	return vd_prepare;
}

static void videocom_vf_put(struct vframe_s *vf, struct videodisplay_dev *dev)
{
	struct dst_buf_t *dst_buf;

	if (IS_ERR_OR_NULL(vf)) {
		vd_print(dev->index, PRINT_ERROR, "vf is NULL\n");
		return;
	}

	dst_buf = to_dst_buf(vf);
	if (IS_ERR_OR_NULL(dst_buf)) {
		vd_print(dev->index, PRINT_ERROR, "dst_buf is NULL\n");
		return;
	}

	if (IS_ERR_OR_NULL(dev)) {
		vd_print(dev->index, PRINT_ERROR, "dev is NULL\n");
		return;
	}

	if (!kfifo_put(&dev->free_q, vf))
		vd_print(dev->index, PRINT_ERROR, "put free_q is full\n");
	vd_print(dev->index, PRINT_OTHER,
		 "%s free buffer count: %d %d\n",
		 __func__, kfifo_len(&dev->free_q), __LINE__);

	if (kfifo_is_full(&dev->free_q)) {
		dev->need_free_buffer = true;
		vd_print(dev->index, PRINT_ERROR, "free_q is full, could uninit buffer!\n");
	}
	vd_print(dev->index, PRINT_PATTERN, "put: vf=%p\n", vf);
	wake_up_interruptible(&dev->wq);
}

static struct vframe_s *videocomposer_vf_peek(void *op_arg)
{
	struct videodisplay_dev *dev = (struct videodisplay_dev *)op_arg;
	struct vframe_s *vf = NULL;
	struct timeval now_time;
	struct timeval nn_start_time;
	u64 nn_used_time;
	bool canbe_peek = true;
	u32 nn_status;
	u32 nn_mode;
	bool bypass_nn = false;

	if (kfifo_peek(&dev->ready_q, &vf)) {
		if (!vf)
			return NULL;

		if (!vf->vc_private) {
			vd_print(dev->index, PRINT_OTHER, "peek: vf->vc_private is NULL\n");
			return vf;
		}

		if (vf->vc_private->flag & VC_FLAG_AI_SR) {
			nn_status = vf->vc_private->srout_data->nn_status;
			nn_mode = vf->vc_private->srout_data->nn_mode;

			vd_print(dev->index, PRINT_NN,
				"peek:nn_status=%d, nn_index=%d, nn_mode=%d, PHY=%llx, nn out:%d*%d, hf:%d*%d,hf_align:%d*%d\n",
				vf->vc_private->srout_data->nn_status,
				vf->vc_private->srout_data->nn_index,
				vf->vc_private->srout_data->nn_mode,
				vf->vc_private->srout_data->nn_out_phy_addr,
				vf->vc_private->srout_data->nn_out_width,
				vf->vc_private->srout_data->nn_out_height,
				vf->vc_private->srout_data->hf_width,
				vf->vc_private->srout_data->hf_height,
				vf->vc_private->srout_data->hf_align_w,
				vf->vc_private->srout_data->hf_align_h);
			if (nn_status != NN_DONE) {
				if (nn_status == NN_INVALID) {
					vf->vc_private->flag &= ~VC_FLAG_AI_SR;
					vd_print(dev->index, PRINT_NN | PRINT_OTHER,
						"nn status is invalid, need bypass");
					return vf;
				} else if (nn_status == NN_WAIT_DOING) {
					vd_print(dev->index, PRINT_FENCE | PRINT_NN,
						"peek: nn wait doing, nn_index =%d, frame_index=%d, nn_status=%d,srout_data=%px\n",
						vf->vc_private->srout_data->nn_index,
						vf->frame_index,
						vf->vc_private->srout_data->nn_status,
						vf->vc_private->srout_data);
					return NULL;
				} else if (nn_status == NN_DISPLAYED) {
					vd_print(dev->index, PRINT_ERROR,
						"peek: nn_status err, nn_index =%d, frame_index=%d, nn_status=%d\n",
						vf->vc_private->srout_data->nn_index,
						vf->frame_index,
						vf->vc_private->srout_data->nn_status);
					return vf;
				}

				if (!(vf->type_original & VIDTYPE_INTERLACE)) {
					if (!(dev->nn_mode_flag & 0x1) && nn_mode == 1) {
						dev->nn_mode_flag |= 0x1;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x2) && nn_mode == 2) {
						dev->nn_mode_flag |= 0x2;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x4) && nn_mode == 3) {
						dev->nn_mode_flag |= 0x4;
						bypass_nn = true;
					}
				} else {
					if (!(dev->nn_mode_flag & 0x100) && nn_mode == 1) {
						dev->nn_mode_flag |= 0x100;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x200) && nn_mode == 2) {
						dev->nn_mode_flag |= 0x200;
						bypass_nn = true;
					} else if (!(dev->nn_mode_flag & 0x400) && nn_mode == 3) {
						dev->nn_mode_flag |= 0x400;
						bypass_nn = true;
					}
				}

				if (bypass_nn) {
					vf->vc_private->flag &= ~VC_FLAG_AI_SR;
					vd_print(dev->index, PRINT_NN,
						"nn mode change, bypass first frame\n");
					return vf;
				}

				do_gettimeofday(&now_time);
				nn_start_time = vf->vc_private->srout_data->start_time;
				nn_used_time = (u64)1000000 *
					(now_time.tv_sec - nn_start_time.tv_sec)
					+ now_time.tv_usec - nn_start_time.tv_usec;

				if (nn_used_time < (nn_need_time - nn_margin_time))
					canbe_peek = false;
				vd_print(dev->index, PRINT_FENCE | PRINT_NN,
					"peek: nn not done, nn_index = %d, frame_index = %d, nn_status = %d, nn_used_time = %lld, canbe_peek = %d.\n",
					vf->vc_private->srout_data->nn_index,
					vf->frame_index,
					vf->vc_private->srout_data->nn_status,
					nn_used_time,
					canbe_peek);
				if (!canbe_peek) {
					vd_print(dev->index, PRINT_FENCE | PRINT_NN,
					"peek:fail: nn not done, nn_index =%d, frame_index=%d, nn_status=%d, nn_used_time=%lld canbe_peek=%d\n",
						vf->vc_private->srout_data->nn_index,
						vf->frame_index,
						vf->vc_private->srout_data->nn_status,
						nn_used_time,
						canbe_peek);
					return NULL;
				}
			}
		}
		if (vf->vc_private->flag & VC_FLAG_AI_COLOR) {
			vd_print(dev->index, PRINT_NN, "peek: aicolor is enable\n");
			nn_status = vf->vc_private->aicolor_info->nn_status;
			vd_print(dev->index, PRINT_NN, "peek: aicolor_status=%d", nn_status);
			if (nn_status != NN_DONE) {
				if (nn_status == NN_INVALID) {
					vf->vc_private->flag &= ~VC_FLAG_AI_COLOR;
					vd_print(dev->index, PRINT_NN | PRINT_OTHER,
						"aicolor status is invalid, need bypass");
					return vf;
				} else if (nn_status == NN_WAIT_DOING) {
					vd_print(dev->index, PRINT_FENCE | PRINT_NN,
						"peek: aicolor wait doing, frame_index=%d, aicolor_status=%d\n",
						vf->frame_index,
						vf->vc_private->aicolor_info->nn_status);
					return NULL;
				} else if (nn_status == NN_START_DOING) {
					vd_print(dev->index, PRINT_FENCE | PRINT_NN,
						"peek: aicolor start doing, frame_index=%d, aicolor_status=%d\n",
						vf->frame_index,
						vf->vc_private->aicolor_info->nn_status);
					return NULL;
				} else if (nn_status == NN_DISPLAYED) {
					vd_print(dev->index, PRINT_ERROR,
						"peek: aicolor_status err, frame_index=%d, aicolor_status=%d\n",
						vf->frame_index,
						vf->vc_private->aicolor_info->nn_status);
					return vf;
				}
			}
		}
		return vf;
	} else {
		return NULL;
	}
}

static void videocomposer_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct videodisplay_dev *dev = (struct videodisplay_dev *)op_arg;
	int repeat_count;
	int frame_index;
	int index_disp;
	bool is_composer;
	bool is_mosaic_22;
	int i;
	struct file *file_vf[4] = {NULL};
	int src_frame_index[4] = {0};
	int dst_frame_index[4] = {0};
	struct vd_prepare_s *vd_prepare;

	if (!vf)
		return;

	repeat_count = vf->repeat_count;
	frame_index = vf->frame_index;
	index_disp = vf->index_disp;
	is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
	is_mosaic_22 = vf->type_ext & VIDTYPE_EXT_MOSAIC_22;

	if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
		vd_print(dev->index, PRINT_OTHER, "put: fake frame\n");
		return;
	}

	if (vf->vc_private && vf->vc_private->srout_data) {
		if (vf->vc_private->srout_data->nn_status == NN_DONE)
			vf->vc_private->srout_data->nn_status = NN_DISPLAYED;
	}
	if (vf->vc_private && vf->vc_private->aicolor_info) {
		if (vf->vc_private->aicolor_info->nn_status == NN_DONE)
			vf->vc_private->aicolor_info->nn_status = NN_DISPLAYED;
	}
	vd_print(dev->index, PRINT_FENCE,
		 "put: repeat_count =%d, frame_index=%d, index_disp=%x\n",
		 repeat_count, frame_index, index_disp);
	if (!is_composer && !is_mosaic_22) {
		if (vf->vc_private) {
			vd_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}

		vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare)) {
			vd_print(dev->index, PRINT_ERROR, "%s: prepare is NULL.\n", __func__);
			return;
		}

		if (IS_ERR_OR_NULL(vd_prepare->src_frame)) {
			vd_print(dev->index, PRINT_ERROR,
				"%s: vd_prepare->src_frame is NULL.\n",
				__func__);
			return;
		}

		file_vf[0] = vd_prepare->src_frame->file_vf;
		src_frame_index[0] = vd_prepare->src_frame->frame_index;
		dst_frame_index[0] = vd_prepare->dst_frame.frame_index;
		vd_prepare_data_q_put(dev, vd_prepare);
		if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
			vd_print(dev->index, PRINT_OTHER, "put enhance vf:%px\n", vf->enhance_vf);
			if (!kfifo_put(&dev->free_q, vf->enhance_vf))
				vd_print(dev->index, PRINT_ERROR, "put free_q is full\n");
		}
	} else if (is_mosaic_22) {
		vd_prepare = container_of(vf, struct vd_prepare_s, dst_frame);
		for (i = 0; i < 4; i++) {
			if (IS_ERR_OR_NULL(vf->vc_private)) {
				vd_print(dev->index, PRINT_ERROR, "put mosaic no private!!!\n");
				break;
			}

			if (IS_ERR_OR_NULL(vf->vc_private->mosaic_vf[i])) {
				vd_print(dev->index, PRINT_ERROR, "mosaic_vf is NULL.\n");
				break;
			}

			file_vf[i] = vf->vc_private->mosaic_vf[i]->file_vf;
			src_frame_index[i] = vf->vc_private->mosaic_src_vf[i]->frame_index,
			src_frame_index[i] = vf->vc_private->mosaic_dst_vf[i].frame_index;
		}

		if (vf->vc_private) {
			vd_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}

		vd_prepare_data_q_put(dev, vd_prepare);
	} else {
		if (vf->vc_private) {
			vd_private_q_recycle(dev, vf->vc_private);
			vf->vc_private = NULL;
		}
	}

	//all the vframe param used must before fence release
	video_timeline_update(dev, vf);

	if (!is_composer && !is_mosaic_22) {
		for (i = 0; i <= repeat_count; i++) {
			if (file_vf[0]) {
				fput(file_vf[0]);
				total_put_count++;
				dev->fput_count++;
			} else {
				vd_print(dev->index, PRINT_ERROR,
					"%s error:src_index=%d,dst_index=%d.\n",
					__func__,
					src_frame_index[0],
					dst_frame_index[0]);
			}
		}

	} else if (is_mosaic_22) {
		for (i = 0; i < 4; i++) {
			if (file_vf[i]) {
				fput(file_vf[i]);
				total_put_count++;
				dev->fput_count++;
			} else {
				vd_print(dev->index, PRINT_ERROR,
					"%s error: i=%d,src_index=%d,dst_index=%d.\n",
					__func__,
					i,
					src_frame_index[i],
					dst_frame_index[i]);
			}
		}
	} else {
		videocom_vf_put(vf, dev);
	}
}

static struct vframe_s *get_dst_vframe_buffer(struct videodisplay_dev *dev)
{
	struct vframe_s *dst_vf;

	if (!kfifo_get(&dev->free_q, &dst_vf)) {
		vd_print(dev->index, PRINT_QUEUE_STATUS, "free q is empty\n");
		return NULL;
	}
	return dst_vf;
}

static bool vf_is_pre_link(struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	if (vf && pvpp_check_vf(vf) > 0 && vf->vf_ext)
		return true;
#endif
	return false;
}

static struct vframe_s *get_vf_from_file(struct videodisplay_dev *dev,
					 struct file *file_vf, bool need_dw)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	bool is_dec_vf = false;
	struct file_private_data *file_private_data = NULL;
	bool enable_prelink = false;
	bool dec_is_i = false;
	struct uvm_hook_mod *uhmod = NULL;
	struct dma_buf *dmabuf = NULL;
	struct vframe_s *dma_di_vf = NULL;
	bool dma_has_di_vf = false;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(file_vf)) {
		vd_print(dev->index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);

	if (is_dec_vf) {
		vd_print(dev->index, PRINT_OTHER, "vf is from decoder\n");
		vf =
		dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		if (!vf) {
			vd_print(dev->index, PRINT_ERROR, "vf is NULL.\n");
			return vf;
		}

		di_vf = vf->vf_ext;
		vd_print(dev->index, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n",
			vf->type,
			vf->flag);
		dec_is_i = vf->type & VIDTYPE_INTERLACE;

		dmabuf = (struct dma_buf *)(file_vf->private_data);
		uhmod = uvm_get_hook_mod(dmabuf, VF_PROCESS_DI);
		if (!IS_ERR_OR_NULL(uhmod)) {
			dma_has_di_vf = true;
			dma_di_vf = (struct vframe_s *)uhmod->arg;
		}

		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vd_print(dev->index, PRINT_OTHER,
				"dma_has_di_vf=%d, dma_di_vf=%px\n",
				dma_has_di_vf, dma_di_vf);
			if (!(dma_has_di_vf && di_vf == dma_di_vf)) {
				vd_print(dev->index, PRINT_ERROR,
					"di vf err: file_vf=%px, dmabuf=%px, uhmod=%px, vf=%px\n",
					file_vf, dmabuf, uhmod, vf);
				vd_print(dev->index, PRINT_ERROR,
					"di_vf=%px, dma_di_vf=%px, frame_index=%d\n",
					di_vf, dma_di_vf, vf->frame_index);
				di_vf = NULL;
			}
		}

		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
			enable_prelink = dim_get_pre_link();
#endif
			vd_print(dev->index, PRINT_OTHER,
				"di_vf->type = 0x%x, di_vf->org = 0x%x, enable_prelink = %d\n",
				di_vf->type,
				di_vf->type_original,
				enable_prelink);
			if (!need_dw ||
			    (need_dw && di_vf->width != 0 &&
				di_vf->canvas0_config[0].phy_addr != 0 &&
				((!dec_is_i && !enable_prelink) || dec_is_i))) {
				vd_print(dev->index, PRINT_OTHER,
					"use di vf\n");
				/* link uvm vf into di_vf->vf_ext */
				if (!di_vf->vf_ext)
					di_vf->vf_ext = vf;
				/* link uvm vf into vf->uvm_vf */
				di_vf->uvm_vf = vf;
				vf = di_vf;
			}
		}
		if (vf->frame_index == 0 && vf->index_disp != 0)
			vf->frame_index = vf->index_disp;

		if (dma_has_di_vf)
			uvm_put_hook_mod(dmabuf, VF_PROCESS_DI);
		dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));

	} else {
		vd_print(dev->index, PRINT_OTHER, "vf is from v4lvideo\n");
		file_private_data = vc_get_file_private(dev, file_vf);
		if (!file_private_data) {
			vd_print(dev->index, PRINT_ERROR,
				 "invalid fd: no uvm, no v4lvideo!!\n");
		} else {
			vf = &file_private_data->vf;
			if (need_dw && (vf->flag & VFRAME_FLAG_DOUBLE_FRAM) && vf->vf_ext) {
				if (vf->width == 0 ||
					vf->canvas0_config[0].phy_addr == 0 ||
					vf_is_pre_link(vf)) {
					vf = vf->vf_ext;
					vd_print(dev->index, PRINT_OTHER, "use dec vf.\n");
				}
			}
		}
	}
	return vf;
}

static struct vframe_s *vd_get_vf_from_buf(struct videodisplay_dev *dev, struct dma_buf *buf)
{
	struct vframe_s *vf = NULL;
	struct vframe_s *di_vf = NULL;
	bool is_dec_vf = false;
	struct uvm_hook_mod *uhmod;
	struct file_private_data *temp_file;

	if (IS_ERR_OR_NULL(buf)) {
		vd_print(dev->index, PRINT_ERROR,
			"%s: dma_buf is NULL.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(buf, VF_SRC_DECODER);
	if (is_dec_vf) {
		vd_print(dev->index, PRINT_OTHER, "vf is from decoder.\n");
		vf = dmabuf_get_vframe(buf);
		vd_print(dev->index, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n",
			vf->type,
			vf->flag);

		di_vf = vf->vf_ext;
		if (di_vf && (vf->flag & VFRAME_FLAG_CONTAIN_POST_FRAME)) {
			vd_print(dev->index, PRINT_OTHER,
				"di_vf->type = 0x%x, di_vf->org = 0x%x.\n",
				di_vf->type,
				di_vf->type_original);
			vd_print(dev->index, PRINT_OTHER, "use di vf.\n");
			/* link uvm vf into di_vf->vf_ext */
			if (!di_vf->vf_ext)
				di_vf->vf_ext = vf;
			/* link uvm vf into vf->uvm_vf */
			di_vf->uvm_vf = vf;
			vf = di_vf;
		}
		dmabuf_put_vframe(buf);
	} else {
		vd_print(dev->index, PRINT_OTHER, "vf is from v4lvideo.\n");
		uhmod = uvm_get_hook_mod(buf, VF_PROCESS_V4LVIDEO);
		if (!uhmod) {
			vd_print(dev->index, PRINT_ERROR,
				"get vframe from v4lvideo failed.\n");
			return vf;
		}

		if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
			vd_print(dev->index, PRINT_ERROR,
				 "vframe in v4lvideo is NULL.\n");
			return vf;
		}
		temp_file = uhmod->arg;
		vf = &temp_file->vf;
		uvm_put_hook_mod(buf, VF_PROCESS_V4LVIDEO);
	}

	return vf;
}

static int video_wait_dma_fence(struct videodisplay_dev *dev,
				   struct dma_fence *fence_file)
{
	int ret = 1;
	u64 timestamp;
	u64 time_cost;

	if (IS_ERR_OR_NULL(fence_file)) {
		vd_print(dev->index, PRINT_FENCE, "wait: fence_file is NULL\n");
		return 1;
	}

	vd_print(dev->index, PRINT_FENCE, "wait: fence_file is %px\n", fence_file);
	timestamp = local_clock();
	ret = dma_fence_wait_timeout(fence_file, false, msecs_to_jiffies(3000));
	if (ret == 0) {
		vd_print(dev->index, PRINT_ERROR, "fence wait timeout\n");
		return 0;
	}

	time_cost = local_clock() - timestamp;
	dev->fence_wait_time_total += time_cost;
	dev->fence_wait_count++;
	if (dev->fence_wait_count == 100) {
		vd_print(dev->index, PRINT_FENCE,
			"wait fence avg=%lldns\n",
			div64_u64(dev->fence_wait_time_total, dev->fence_wait_count));
			dev->fence_wait_count = 0;
			dev->fence_wait_time_total = 0;
	}

	vd_print(dev->index, PRINT_FENCE,
		 "wait fence, state: %d, wait cost time:%lldms\n",
		 ret,
		 div64_u64(time_cost, 1000000));

	dma_fence_put(fence_file);
	return 1;
}

static void vframe_do_mosaic_22(struct videodisplay_dev *dev,
				struct received_frames_t *received_frames)
{
	struct vd_prepare_s *vd_prepare = NULL;
	struct vframe_s *vf = NULL;
	struct vframe_s *scr_vf = NULL;
	struct vframe_s *mosaic_vf = NULL;
	struct vframe_s *vf_ext = NULL;
	struct frames_info_t *frames_info = NULL;
	struct file *file_vf = NULL;
	int i;
	bool is_dec_vf = false, is_v4l_vf = false;
	struct video_composer_private *vc_private;
	struct frame_info_t *frame_info = NULL;
	u32 pic_w;
	u32 pic_h;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(received_frames)) {
		vd_print(dev->index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return;
	}

	vd_prepare = vd_prepare_data_q_get(dev);
	if (!vd_prepare) {
		vd_print(dev->index, PRINT_ERROR, "%s: get prepare_data failed.\n", __func__);
		return;
	}

	vf = &vd_prepare->dst_frame;
	memset(vf, 0, sizeof(struct vframe_s));

	if (!kfifo_get(&dev->receive_q, &received_frames)) {
		vd_print(dev->index, PRINT_ERROR, "com: get failed\n");
		return;
	}

	vc_private = vd_private_q_pop(dev);
	if (!vc_private) {
		vd_print(dev->index, PRINT_ERROR, "%s: get vc_private failed.\n", __func__);
		return;
	}

	vc_private->flag |= VC_FLAG_MOSAIC_22;
	vf->vc_private = vc_private;

	frames_info = &received_frames->frames_info;

	for (i = 0; i < 4; i++) {
		frame_info = &frames_info->frame_info[i];
		scr_vf = NULL;
		file_vf = received_frames->file_vf[i];
		is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
		is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

		if (is_dec_vf || is_v4l_vf) {
			vd_print(dev->index, PRINT_OTHER, "%s dma buffer is vf\n", __func__);
			scr_vf = get_vf_from_file(dev, file_vf, false);
			if (!scr_vf) {
				vd_print(dev->index, PRINT_ERROR, "get vf NULL\n");
				continue;
			}
		} else {
			vd_print(dev->index, PRINT_ERROR, "%s dma buffer not vf\n", __func__);
		}

		if (!scr_vf) {
			vd_print(dev->index, PRINT_ERROR, "%s:no vf\n", __func__);
			return;
		}
		vc_private->mosaic_src_vf[i] = scr_vf;
		vc_private->mosaic_dst_vf[i] = *scr_vf;
		vc_private->mosaic_vf[i] = &vc_private->mosaic_dst_vf[i];
		mosaic_vf = vc_private->mosaic_vf[i];

		mosaic_vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
			| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		mosaic_vf->axis[0] = frame_info->dst_x;
		mosaic_vf->axis[1] = frame_info->dst_y;
		mosaic_vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
		mosaic_vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;
		mosaic_vf->crop[0] = frame_info->crop_y;
		mosaic_vf->crop[1] = frame_info->crop_x;
		if ((mosaic_vf->type & VIDTYPE_COMPRESS) != 0) {
			pic_w = mosaic_vf->compWidth;
			pic_h = mosaic_vf->compHeight;
		} else {
			pic_w = mosaic_vf->width;
			pic_h = mosaic_vf->height;
		}
		mosaic_vf->crop[2] = pic_h - frame_info->crop_h - frame_info->crop_y;
		mosaic_vf->crop[3] = pic_w - frame_info->crop_w - frame_info->crop_x;

		mosaic_vf->zorder = frame_info->zorder;
		mosaic_vf->file_vf = file_vf;

		if (mosaic_vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
			vf_ext = mosaic_vf->vf_ext;
			if (vf_ext) {
				vf_ext->axis[0] = mosaic_vf->axis[0];
				vf_ext->axis[1] = mosaic_vf->axis[1];
				vf_ext->axis[2] = mosaic_vf->axis[2];
				vf_ext->axis[3] = mosaic_vf->axis[3];
				vf_ext->crop[0] = mosaic_vf->crop[0];
				vf_ext->crop[1] = mosaic_vf->crop[1];
				vf_ext->crop[2] = mosaic_vf->crop[2];
				vf_ext->crop[3] = mosaic_vf->crop[3];
				vf_ext->zorder = mosaic_vf->zorder;
				vf_ext->flag |= VFRAME_FLAG_VIDEO_COMPOSER
					| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
			} else {
				vd_print(dev->index, PRINT_ERROR, "vf_ext is null\n");
			}
		}
	}

	vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;

	vf->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);

	vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
	vf->type = (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	vf->type_ext |= VIDTYPE_EXT_MOSAIC_22;

	vf->axis[0] = 0;
	vf->axis[1] = 0;
	vf->axis[2] = dev->vinfo_w - 1;
	vf->axis[3] = dev->vinfo_h - 1;

	vf->crop[0] = 0;
	vf->crop[1] = 0;
	vf->crop[2] = 0;
	vf->crop[3] = 0;

	vf->zorder = frames_info->disp_zorder;
	vf->canvas0Addr = -1;
	vf->canvas1Addr = -1;

	vf->width = dev->vinfo_w;
	vf->height = dev->vinfo_h;

	vd_print(dev->index, PRINT_DEWARP,
			 "composer:vf_w: %d, vf_h: %d\n", vf->width, vf->height);

	vf->canvas0_config[0].phy_addr = 0;
	vf->canvas0_config[0].width = dev->vinfo_w;
	vf->canvas0_config[0].height = dev->vinfo_h;
	vf->canvas0_config[0].block_mode = 0;

	vf->canvas0_config[1].phy_addr = 0;
	vf->canvas0_config[1].width = dev->vinfo_w;
	vf->canvas0_config[1].height = dev->vinfo_h >> 1;
	vf->canvas0_config[1].block_mode = 0;
	vf->plane_num = 2;

	vf->repeat_count = 0;
	dev->vd_prepare_last = vd_prepare;
	dev->fake_vf = *vf;

	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)vf))
		vd_print(dev->index, PRINT_ERROR, "ready_q is full\n");

	atomic_set(&received_frames->on_use, false);
	dev->last_file = NULL;
}

static void empty_ready_queue(struct videodisplay_dev *dev)
{
	int repeat_count;
	int frame_index;
	bool is_composer;
	int i;
	struct file *file_vf;
	struct vframe_s *vf = NULL;

	vd_print(dev->index, PRINT_OTHER, "vc: empty ready_q len=%d\n",
		 kfifo_len(&dev->ready_q));

	while (kfifo_len(&dev->ready_q) > 0) {
		if (kfifo_get(&dev->ready_q, &vf)) {
			if (!vf)
				break;
			repeat_count = vf->repeat_count;
			frame_index = vf->frame_index;
			is_composer = vf->flag & VFRAME_FLAG_COMPOSER_DONE;
			file_vf = vf->file_vf;
			vd_print(dev->index, PRINT_OTHER,
				 "empty: repeat_count =%d, frame_index=%d\n",
				 repeat_count, frame_index);
			video_timeline_increase(dev, repeat_count + 1);
			if (!is_composer) {
				for (i = 0; i <= repeat_count; i++) {
					fput(file_vf);
					total_put_count++;
					dev->fput_count++;
				}
			} else {
				videocom_vf_put(vf, dev);
			}
		}
	}
}

static bool check_vf_has_afbc(struct videodisplay_dev *dev, struct file *file_vf)
{
	struct vframe_s *vf = NULL;

	vf = get_vf_from_file(dev, file_vf, false);
	if (!vf)
		return false;

	if (vf->type & VIDTYPE_COMPRESS)
		return true;

	return false;
}

static bool check_mosaic_22(struct videodisplay_dev *dev,
				struct received_frames_t *received_frames)
{
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	int a[4];
	int i = 0;
	struct frames_info_t *f = &received_frames->frames_info;
	struct frame_info_t *frame_info;
	int half_w;
	int half_h;

	if (!dev->support_mosaic)
		return false;

	if (received_frames->frames_info.frame_count != 4)
		return false;

	if (dev->vinfo_w == 0) {
		video_composer_vinfo = get_current_vinfo();
		if (IS_ERR_OR_NULL(video_composer_vinfo))
			video_composer_vinfo = &vinfo;

		dev->vinfo_w = video_composer_vinfo->width;
		dev->vinfo_h = video_composer_vinfo->height;
	}

	if (dev->vinfo_w == 0 || dev->vinfo_h == 0)
		return false;

	half_w = dev->vinfo_w >> 1;
	half_h = dev->vinfo_h >> 1;

	for (i = 0; i < 4; i++) {
		frame_info = &f->frame_info[i];
		vd_print(dev->index, PRINT_AXIS,
			"check mosaic: i=%d: %d %d %d %d\n",
			i,
			frame_info->dst_x,
			frame_info->dst_y,
			frame_info->dst_w,
			frame_info->dst_h);
	}

	/*check all w h <= 1/2 vinfo*/
	for (i = 0; i < 4; i++) {
		if (f->frame_info[i].dst_w > half_w || f->frame_info[i].dst_h > half_h)
			return false;
		if (!check_vf_has_afbc(dev, received_frames->file_vf[i])) {
			vd_print(dev->index, PRINT_AXIS, "vf has no afbc\n");
			return false;
		}
	}

	for (i = 0; i < 4; i++) {
		if (f->frame_info[i].dst_x < half_w && f->frame_info[i].dst_y < half_h)
			a[0] = i;
		if (f->frame_info[i].dst_x >= half_w && f->frame_info[i].dst_y < half_h)
			a[1] = i;
		if (f->frame_info[i].dst_x < half_w && f->frame_info[i].dst_y >= half_h)
			a[2] = i;
		if (f->frame_info[i].dst_x >= half_w && f->frame_info[i].dst_y >= half_h)
			a[3] = i;
	}

	/*check quadrant 1*/
	frame_info = &f->frame_info[a[0]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w)
		return false;
	if (frame_info->dst_x % 8 != 0)
		return false;

	/*check quadrant 2*/
	frame_info = &f->frame_info[a[1]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w + dev->vinfo_w)
		return false;
	if ((frame_info->dst_x - half_w) % 8 != 0)
		return false;

	/*check quadrant 3*/
	frame_info = &f->frame_info[a[2]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w)
		return false;
	if (frame_info->dst_x % 8 != 0)
		return false;

	/*check quadrant 4*/
	frame_info = &f->frame_info[a[3]];
	if (frame_info->dst_x * 2 + frame_info->dst_w != half_w + dev->vinfo_w)
		return false;
	if ((frame_info->dst_x - half_w) % 8 != 0)
		return false;

	vd_print(dev->index, PRINT_AXIS, "check mosaic ok\n");

	return true;
}

static struct vframe_s *get_enhance_vf_pointer(struct videodisplay_dev *dev,
	struct vframe_s *vf)
{
	int i;
	struct vframe_s *enhance_vf;

	if (dev->kfifo_need_initialize) {
		vd_print(dev->index, PRINT_QUEUE_STATUS, "init buffer free_q for lcevc\n");
		dev->kfifo_need_initialize = false;
		for (i = 0; i < DMA_BUF_COUNT; i++) {
			if (!kfifo_put(&dev->free_q, &dev->enhance_vf[i]))
				vd_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
		}
	}

	if (!kfifo_get(&dev->free_q, &enhance_vf)) {
		vd_print(dev->index, PRINT_ERROR,
			 "task: free_q is empty, can not provide enhance_vf\n");
		enhance_vf = NULL;
	} else {
		vd_print(dev->index, PRINT_OTHER,
			 "task: get_enhance_vf:%px\n", enhance_vf);
		memcpy(enhance_vf, vf, sizeof(struct vframe_s));
	}

	return enhance_vf;
}

static struct  uvm_lcevc_frame_info *vc_get_lcevc_data(struct videodisplay_dev *dev,
						     struct dma_buf *src_dmabuf)
{
	struct uvm_lcevc_hook_data *hook_data;
	struct uvm_lcevc_frame_info *lcevc_data;
	struct uvm_hook_mod *uhmod;

	if (!src_dmabuf) {
		vd_print(dev->index, PRINT_ERROR, "vc get lcevc data fail\n");
		return NULL;
	}

	uhmod = uvm_get_hook_mod(src_dmabuf, PROCESS_LCEVC);
	if (!uhmod) {
		vd_print(dev->index, PRINT_OTHER, "%s:dma file file_private_data is NULL 1\n",
			__func__);
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		vd_print(dev->index, PRINT_ERROR, "%s: dma file file_private_data is NULL 2\n",
			__func__);
		return NULL;
	}
	hook_data = uhmod->arg;
	lcevc_data = &hook_data->lcevc_vframe;
	uvm_put_hook_mod(src_dmabuf, PROCESS_LCEVC);

	return lcevc_data;
}

static bool set_vf_lcevc_data(struct videodisplay_dev *dev,
	struct vframe_s *enhance_vf, struct uvm_lcevc_frame_info *lcevc_data)
{
	int len, i;

	if (!enhance_vf || !lcevc_data)
		return false;

	enhance_vf->width = lcevc_data->width;
	enhance_vf->height = lcevc_data->height;
	enhance_vf->canvas0Addr = -1;
	enhance_vf->canvas0_config[0].phy_addr = lcevc_data->y_physical_addr;
	enhance_vf->canvas1Addr = -1;
	enhance_vf->canvas0_config[1].phy_addr = lcevc_data->uv_physical_addr;

	enhance_vf->canvas0_config[0].width = lcevc_data->stride;
	enhance_vf->canvas0_config[0].height = enhance_vf->height;
	enhance_vf->canvas0_config[1].width = lcevc_data->stride;
	enhance_vf->canvas0_config[1].height = enhance_vf->canvas0_config[0].height;

	enhance_vf->plane_num = 1;
	enhance_vf->type_ext |= VIDTYPE_EXT_LCEVC;
	enhance_vf->flag = VFRAME_FLAG_VIDEO_LINEAR
		| VFRAME_FLAG_VIDEO_COMPOSER
		| VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;

	if (lcevc_data->frame_type == LCEVC_RESIDUAL_Y10BIT_PACKED) {
		vd_print(dev->index, PRINT_INDEX_DISP, "get Residual data type: Y10bitPacked\n");
		enhance_vf->type = VIDTYPE_VIU_FIELD
			| VIDTYPE_VIU_444
			| VIDTYPE_VIU_SINGLE_PLANE;
		enhance_vf->type_ext |= VIDTYPE_EXT_LUMA_ONLY;
		enhance_vf->bitdepth = BITDEPTH_Y10;
	} else if (lcevc_data->frame_type == LCEVC_RESIDUAL_UYVY10BIT_PACKED) {
		vd_print(dev->index, PRINT_INDEX_DISP, "get Residual data type: UYVY10bitPacked\n");
		enhance_vf->type = VIDTYPE_VIU_FIELD |
			VIDTYPE_VIU_422 |
			VIDTYPE_VIU_SINGLE_PLANE;
		enhance_vf->bitdepth = BITDEPTH_Y10 |
			BITDEPTH_U10 |
			BITDEPTH_V10 |
			FULL_PACK_422_MODE;
	} else {
		vd_print(dev->index, PRINT_INDEX_DISP,
			"get unknown Residual data type: %d\n",
			lcevc_data->frame_type);
		return false;
	}

	memcpy(&enhance_vf->scaler_coeff, &lcevc_data->upsample_kernel, sizeof(struct vf_lcevc_t));
	len = enhance_vf->scaler_coeff.len;
	vd_print(dev->index, PRINT_INDEX_DISP, "task: coeff len:%d", len);
	for (i = 0; i < len; i++) {
		vd_print(dev->index, PRINT_INDEX_DISP,
				"task: coeff[%d] %d %d\n",
				i,
				enhance_vf->scaler_coeff.k[0][i],
				enhance_vf->scaler_coeff.k[1][i]);
	}
	vd_print(dev->index, PRINT_INDEX_DISP,
		"task: enhance_vf width:%d, height:%d, enhance_vf yuv addr:%px\n",
		enhance_vf->width,
		enhance_vf->height,
		(void *)enhance_vf->canvas0_config[0].phy_addr);

	return true;
}

static bool detect_vf_usage(struct videodisplay_dev *dev,
	struct received_frames_t *received_frames, bool *need_composer_ptr, bool *mosaic_22_ptr)
{
	struct vframe_s *vf = NULL;
	int count;
	int num;
	u32 frame_transform = 0;
	struct frames_info_t *frames_info = NULL;
	struct frame_info_t *frame_info = NULL;
	bool is_dec_vf = false, is_v4l_vf = false;

	count = received_frames->frames_info.frame_count;
	if (count == 1) {
		if ((dev->index == 0 && force_composer) || (dev->index == 1 && force_composer_pip))
			*need_composer_ptr = true;
		frame_transform = received_frames->frames_info.frame_info[0].rotation;
		if (frame_transform == VC_TRANSFORM_ROT_90 ||
			frame_transform == VC_TRANSFORM_ROT_270 ||
			frame_transform == VC_TRANSFORM_FLIP_H_ROT_90 ||
			frame_transform == VC_TRANSFORM_FLIP_V_ROT_90) {
			*need_composer_ptr = true;
			dev->need_rotate = true;
		} else {
			dev->need_rotate = false;
		}
	} else {
		dev->need_rotate = false;
		if (check_mosaic_22(dev, received_frames)) {
			*need_composer_ptr = false;
			*mosaic_22_ptr = true;
		} else {
			*need_composer_ptr = true;
		}
	}

	frames_info = &received_frames->frames_info;
	num = frames_info->layer_index;
	frame_info = &frames_info->frame_info[num];
	detect_vf_type(frame_info, &is_dec_vf, &is_v4l_vf);
	if (is_dec_vf || is_v4l_vf) {
		vf = vd_get_vf_from_buf(dev, frame_info->dmabuf);
		if (!vf) {
			vd_print(dev->index, PRINT_ERROR, "get NULL vf!!\n");
			return false;
		}
		if (dev->last_vf_from_dpss && !(vf->dpss_flg & 0x40))
			vf->dpss_flg |= 0x08;
		else if (!(vf->dpss_flg & 0x08))
			vf->dpss_flg |= 0x10;
	}
	if (vf && (vf->type & VIDTYPE_DI_PW || vf->di_flag & DI_FLAG_DI_PVPPLINK)) {
		vd_print(dev->index, PRINT_OTHER, "di_vf=%px type_ext=%x.\n", vf, vf->type_ext);
		if (vf->type_ext & VIDTYPE_EXT_DI_DO_ROTATE && count == 1) {
			*need_composer_ptr = false;
			dev->need_rotate = false;
			vd_print(dev->index, PRINT_OTHER, "di already do rotate, vc needn't do.\n");
		}
	}
	if (dev->output_duration >= 240 && dev->vinfo_w > 1920 && !*need_composer_ptr) {
		if (vf && (vf->flag & VFRAME_FLAG_GAME_MODE)) {
			vd_print(dev->index, PRINT_OTHER, "game mode no need force composer.\n");
			return true;
		}
		vd_print(dev->index, PRINT_AXIS, "fps > 240, need composer.\n");
		if (is_dec_vf || is_v4l_vf) {
			vd_print(dev->index, PRINT_OTHER,
				"%s vf_height:%d vf_com_height:%d\n",
				__func__,
				vf->height,
				vf->compHeight);
			if (vf->height > 1088 || vf->compHeight > 1088)
				*need_composer_ptr = true;
		} else {
			vd_print(dev->index, PRINT_OTHER,
				"%s: frame_info->height:%d.\n",
				 __func__,
				 frame_info->buffer_h);
			if (frame_info->buffer_h > 1088)
				*need_composer_ptr = true;
		}
	}

	if (!vd_composer_en) {
		*need_composer_ptr = false;
		*mosaic_22_ptr = false;
	}
	return true;
}

static void video_display_para_reset(int layer_index)
{
	actual_delay_count[layer_index] = 0;
}

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
static void vc_notify_msg_to_mediaproxy(struct videodisplay_dev *dev,
	struct vframe_s *vf, int event, struct timespec64 ts)
{
	struct aml_video_user_data msg[MAX_COMPOSER_COUNT] = {0};
	struct input_mediaproxy_info_s *input_vf_info;
	int count = 0;
	int i;

	if (!dev || !vf) {
		pr_info("%s: invalid param.\n", __func__);
		return;
	}

	if (!(vf->flag & VFRAME_FLAG_COMPOSER_DONE)) {
		msg[0].data.frame_info.bitstreamid = div64_u64(vf->timestamp, 1000000000);
		msg[0].data.frame_info.vd_instid = dev->index;
		msg[0].data.frame_info.decoder_instid = vf->decoder_instid;
		msg[0].data.frame_info.frame_index = vf->frame_index;
		msg[0].data.frame_info.time = 1000000000L * ts.tv_sec + ts.tv_nsec;
		msg[0].message_type = event;

		notify_msg_to_mediaproxy(vd_mediaproxy_display_info[dev->index].k_producer_session,
					1,
					&msg);
	} else {
		if (!vf->composer_info) {
			vd_print(dev->index, PRINT_ERROR,
				"%s:vc_private is null!\n", __func__);
			return;
		}
		count = vf->composer_info->count;
		if (count > MAX_COMPOSER_COUNT) {
			vd_print(dev->index, PRINT_ERROR,
				"%s:count (%d) exceeds MAX_COMPOSER_COUNT(%d)\n",
				__func__, count, MAX_COMPOSER_COUNT);
			count = MAX_COMPOSER_COUNT;
		}
		for (i = 0; i < count; i++) {
			input_vf_info = &vf->composer_info->input_info[i];
			msg[i].data.frame_info.bitstreamid =
				div64_u64(input_vf_info->timestamp, 1000000000);
			msg[i].data.frame_info.vd_instid = dev->index;
			msg[i].data.frame_info.decoder_instid =  input_vf_info->decoder_instid;
			msg[i].data.frame_info.frame_index = input_vf_info->frame_index;
			msg[i].data.frame_info.time = 1000000000L * ts.tv_sec + ts.tv_nsec;
			msg[i].message_type = event;
		}
		notify_msg_to_mediaproxy(vd_mediaproxy_display_info[dev->index].k_producer_session,
					count,
					&msg);
	}
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

static bool check_frc_n2m_status(void)
{
	bool ret = false;

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
		/* frc_get_n2m_setting 1 : n2m is 1:1; 2 :n2m is 1:2 */
		/* frc_is_on() 1: means frc really worked */
		if ((frc_get_n2m_setting() == 2) && frc_is_on())
			ret = true;
#endif
	return ret;
}

static void video_display_push_ready(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	u32 vsync_index = vsync_count[dev->index];

#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	if (!dev->enable_pulldown && (vd_pulldown_level && frc_get_video_latency())) {
		dev->enable_pulldown = true;
		vd_print(dev->index, PRINT_OTHER, "%s: enable pulldown\n", __func__);
	}
#endif

	if (vf && vf->vc_private) {
		vf->vc_private->vsync_index = vsync_index;
		vd_print(dev->index, PRINT_OTHER, "set vsync_index =%d\n", vsync_index);
	}
}

static void vd_vsync_video_pattern(struct videodisplay_dev *dev, int pattern,
	struct vframe_s *vf)
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
				vd_print(dev->index, PRINT_PATTERN,
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
			vd_print(dev->index, PRINT_PATTERN,
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
			vd_print(dev->index, PRINT_PATTERN,
				"patten_22: video %d:%d mode force unbroken, pre_pat=%d, %d, index=%d, %d\n",
				factor1, factor2, dev->pre_pat_trace,
				patten_trace[dev->index],
				vf->frame_index, dev->last_vf_index);
			return;
		}
		dev->pattern[pattern] = 0;
		dev->pattern_exit_cnt++;
		vd_print(dev->index, PRINT_PATTERN,
			"patten: video %d:%d mode broken, pre_pat=%d, patten =%d, index=%d, %d\n",
			factor1, factor2, dev->pre_pat_trace, patten_trace[dev->index],
			vf->frame_index, dev->last_vf_index);
	} else {
		vd_print(dev->index, PRINT_PATTERN,
			"pattern:%d invalid case, reset to 0.\n", pattern);
		dev->pattern[pattern] = 0;
	}
}

static void vd_vsync_video_pattern_22323(struct videodisplay_dev *dev, struct vframe_s *vf)
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
		vsync_pts_inc_scale[dev->index] / vsync_pts_inc_scale_base[dev->index];
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
				vd_print(dev->index, PRINT_PATTERN,
					"patten: video 22323 mode detected\n");
			}
		}
	} else if (dev->pattern[pattern] == pattern_range) {
		dev->pattern[pattern] = 0;
		dev->pattern_exit_cnt++;
		vd_print(dev->index, PRINT_PATTERN,
			"patten: video 22323 mode broken, pre_pat=%d, patten =%d, index=%d, %d\n",
			dev->pre_pat_trace, patten_trace[dev->index],
			vf->frame_index, dev->last_vf_index);
	} else {
		dev->pattern[pattern] = 0;
	}
}

static void vd_vsync_video_pattern_13213(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	int i = 0, sum = 0, ave = 0;
	int vsync_pts_inc = 16 * 90000 *
		vsync_pts_inc_scale[dev->index] / vsync_pts_inc_scale_base[dev->index];
	int vframe_duration = vf->duration * 15;

	if (vsync_pts_inc * 2 != vframe_duration) {
		vd_print(dev->index, PRINT_PATTERN, "%s: not 13213 condition.\n", __func__);
		return;
	}

	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		sum += dev->patten_factor[i];

	ave = sum / PATTEN_FACTOR_MAX;
	if (ave == 2) {
		dev->pattern_detected = PATTERN_22;
		dev->pattern[PATTERN_22] = PATTERN_22_DETECT_RANGE;
		dev->pattern_enter_cnt++;
		vd_print(dev->index, PRINT_PATTERN, "%s: video 13213 mode detected\n", __func__);
	} else {
		vd_print(dev->index, PRINT_PATTERN, "%s: not 13213 mode.\n", __func__);
	}
}

static void vd_vsync_video_pattern_53(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	int i = 0, sum = 0, ave = 0;
	int vsync_pts_inc = 16 * 90000 *
		vsync_pts_inc_scale[dev->index] / vsync_pts_inc_scale_base[dev->index];
	int vframe_duration = vf->duration * 15;

	if (vsync_pts_inc * 4 != vframe_duration) {
		vd_print(dev->index, PRINT_PATTERN, "%s: not 53 condition.\n", __func__);
		return;
	}

	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		sum += dev->patten_factor[i];

	ave = sum / PATTEN_FACTOR_MAX;
	if (ave == 4) {
		dev->pattern_detected = PATTERN_44;
		dev->pattern[PATTERN_44] = PATTERN_44_DETECT_RANGE;
		dev->pattern_enter_cnt++;
		vd_print(dev->index, PRINT_PATTERN, "%s: video 53 mode detected\n", __func__);
	} else {
		vd_print(dev->index, PRINT_PATTERN, "%s: not 53 mode.\n", __func__);
	}
}

static void vd_vsync_video_pattern_11120(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	int i = 0, sum = 0, ave = 0;
	int vsync_pts_inc = 16 * 90000 *
		vsync_pts_inc_scale[dev->index] / vsync_pts_inc_scale_base[dev->index];
	int vframe_duration = vf->duration * 15;

	if (abs(vsync_pts_inc - vframe_duration) >= 30) {
		vd_print(dev->index, PRINT_PATTERN, "%s: not 11 condition.\n", __func__);
		return;
	}

	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		sum += dev->patten_factor[i];

	ave = 2 * sum / PATTEN_FACTOR_MAX;
	if (ave == 2) {
		dev->pattern_detected = PATTERN_11;
		dev->pattern[PATTERN_11] = PATTERN_11_DETECT_RANGE;
		dev->pattern_enter_cnt++;
		vd_print(dev->index, PRINT_PATTERN, "%s: video 11 mode detected\n", __func__);
	} else {
		vd_print(dev->index, PRINT_PATTERN, "%s: not 11 mode.\n", __func__);
	}
}

static void vsync_video_pattern(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	vd_vsync_video_pattern(dev, PATTERN_11, vf);
	vd_vsync_video_pattern(dev, PATTERN_32, vf);
	vd_vsync_video_pattern(dev, PATTERN_22, vf);
	vd_vsync_video_pattern_22323(dev, vf);
	vd_vsync_video_pattern(dev, PATTERN_44, vf);
	vd_vsync_video_pattern(dev, PATTERN_55, vf);
	if (dev->pattern_detected != PATTERN_11 ||
		(dev->pattern_detected == PATTERN_11 &&
			dev->pattern[PATTERN_11] != PATTERN_11_DETECT_RANGE))
		vd_vsync_video_pattern_11120(dev, vf);
	if (dev->pattern_detected != PATTERN_22 ||
		(dev->pattern_detected == PATTERN_22 &&
			dev->pattern[PATTERN_22] != PATTERN_22_DETECT_RANGE))
		vd_vsync_video_pattern_13213(dev, vf);
	if (dev->pattern_detected != PATTERN_44 ||
		(dev->pattern_detected == PATTERN_44 &&
			dev->pattern[PATTERN_44] != PATTERN_44_DETECT_RANGE))
		vd_vsync_video_pattern_53(dev, vf);
	/*vd_vsync_video_pattern(dev, PTS_41_PATTERN);*/
}

static inline int vd_perform_pulldown(struct videodisplay_dev *dev, struct vframe_s *vf,
					 bool *expired)
{
	int pattern_range, expected_curr_interval;
	int expected_prev_interval;

	/* Dont do anything if we have invalid data */
	if (!vf || !vf->vc_private)
		return -1;
	if (vf->vc_private->vsync_index == 0)
		return -1;

	if (vf->vf_vrr_param.qms_plus_en || vf->vf_vrr_param.qms_en)
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
			vd_print(dev->index, PRINT_PATTERN,
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

	vd_print(dev->index, PRINT_PATTERN,
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
			vd_print(dev->index, PRINT_PATTERN,
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
			vd_print(dev->index, PRINT_PATTERN,
				"patten: pull frame for pattern: %d",
				dev->pattern_detected);
		}
	}
	return 0;
}

static int find_nearest_duration(struct videodisplay_dev *dev, int duration_val)
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

	vd_print(dev->index, PRINT_PATTERN, "The nearest duration is %d.\n", duration_arr[num]);
	return duration_arr[num];
}

static bool pulldown_support_vf(struct videodisplay_dev *dev, u32 duration_val)
{
	bool support = false;
	int duration = 0;
	/*duration: 800(120fps) 801(119.88fps) 960(100fps) 1600(60fps) 1920(50fps)*/
	/*3200(30fps) 3203(29.97) 3840(25fps) 4000(24fps) 4004(23.976fps)*/

	duration = find_nearest_duration(dev, duration_val);
	enable_new_afr_pattern = false;

	if (new_afr_pulldown)
		support = true;
	if (vsync_pts_inc_scale[dev->index] == 1 &&
		vsync_pts_inc_scale_base[dev->index] == 48) {
		/*48hz for 24fps 23.976fps*/
		if (duration == 4004 || duration == 4000)
			support = true;
	} else if (vsync_pts_inc_scale[dev->index] == 1 &&
		vsync_pts_inc_scale_base[dev->index] == 50) {
		/*50hz for 25fps 50fps*/
		if (duration == 3840 || duration == 1920) {
			enable_new_afr_pattern = false;
			support = true;
		}
	} else if (vsync_pts_inc_scale[dev->index] == 1001 &&
		vsync_pts_inc_scale_base[dev->index] == 60000) {
		/*59.94hz for 23.976, 29.97*/
		if (duration == 4004 || duration == 3203)
			support = true;
	} else if (vsync_pts_inc_scale[dev->index] == 1 &&
		vsync_pts_inc_scale_base[dev->index] == 60) {
		/*60hz for 23.976, 24, 25, 29.97, 30*/
		if (duration == 4004 ||
			duration == 4000 ||
			duration == 3840 ||  /*22323 patten for projector, that vout always 60*/
			duration == 3203 ||
			duration == 3200)
			support = true;
	} else if (vsync_pts_inc_scale[dev->index] == 1 &&
		vsync_pts_inc_scale_base[dev->index] == 100) {
		/*100hz for 25fps, 50fps, 100fps*/
		if (duration == 3840 || duration == 1920 || duration == 960)
			support = true;
	} else if (vsync_pts_inc_scale[dev->index] == 1001 &&
		vsync_pts_inc_scale_base[dev->index] == 120000) {
		/*119.88hz for 23.976,29.97,59.94*/
		if (duration == 4004 ||
			duration == 3203 ||
			duration == 1601 ||
			duration == 801)
			support = true;
	} else if (vsync_pts_inc_scale[dev->index] == 1 &&
		vsync_pts_inc_scale_base[dev->index] == 120) {
		/*120hz for 23.976, 24, 29.97, 30, 59.94, 60*/
		if (duration == 4004 ||
			duration == 4000 ||
			duration == 3203 ||
			duration == 3200 ||
			duration == 1601 ||
			duration == 1600 ||
			duration == 801 ||
			duration == 800)
			support = true;
	}

	return support;
}

static void vframe_composer(struct videodisplay_dev *dev,
				struct received_frames_t *received_frames)
{
	if (!dev || !received_frames) {
		pr_info("%s: NULL param.\n", __func__);
		return;
	}

	video_composer_init_buffer(dev);
	//do composer

	get_dst_vframe_buffer(dev);

	dev->last_file = NULL;
	dev->vd_prepare_last = NULL;
}

#ifndef CONFIG_AMLOGIC_VIDEO_COMPOSER
void vpp_vsync_in(void)
{
	vd_print(0, PRINT_OTHER, "enter %s, %d\n",
		__func__, next_vsync_wakeup_vpp_to_get);
	if (is_video_process_in_thread() && next_vsync_wakeup_vpp_to_get) {
		allow_vpp_to_get = 1;
		vd_print(0, PRINT_OTHER,
			"vc start wake vpp to get,allow_vpp_to_get=%d\n", allow_vpp_to_get);
		vpp_lowlatency_wakeup();
		next_vsync_wakeup_vpp_to_get = 0;
	}
}
#endif

static int is_pulldown_1_1(struct videodisplay_dev *dev, int duration)
{
	int ret = 0;
	int vsync_pts = 0;

	if (vsync_pts_inc_scale[dev->index])
		vsync_pts = vsync_pts_inc_scale_base[dev->index] /
			vsync_pts_inc_scale[dev->index];

	vd_print(dev->index, PRINT_OTHER, "vsync_pts=%d, duration=%d.\n",
		vsync_pts, duration);

	switch (duration) {
	case 800:
	case 801:
		if (vsync_pts == 120)
			ret = 1;
		break;
	case 960:
		if (vsync_pts == 100)
			ret = 1;
		break;
	case 1600:
	case 1601:
		if (vsync_pts == 60)
			ret = 1;
		break;
	case 1920:
		if (vsync_pts == 50)
			ret = 1;
		break;
	default:
		ret = 0;
		break;
	}
	return ret;
}

static void calculate_low_latency_case(struct videodisplay_dev *dev, struct vframe_s *vf)
{
	bool enable_prelink = false;
	int vsync_pts = 0;
	u64 elapsed_ns = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	enable_prelink = dim_get_pre_link();
#endif

	if (vsync_pts_inc_scale[dev->index])
		vsync_pts = vsync_pts_inc_scale_base[dev->index] /
			vsync_pts_inc_scale[dev->index];

	if (dev->is_tv_path) {
		if (get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_DOVI && vsync_pts >= 100) {
			dev->low_latency_case = 2;
			vd_print(dev->index, PRINT_OTHER,
				"case2:need next vsync to get.\n");
		} else if (is_pulldown_1_1(dev, vf->duration)) {
			vpp_lowlatency_wakeup();
			dev->low_latency_case = 1;
			vd_print(dev->index, PRINT_OTHER,
				"case1:tv path 1:1 mode use low latency2 mode.\n");
		} else if ((vf->flag & VFRAME_FLAG_GAME_MODE)) {
			dev->low_latency_case = 2;
			vd_print(dev->index, PRINT_OTHER,
				"case2:need next vsync to get.\n");
		} else {
			dev->low_latency_case = 0;
			vd_print(dev->index, PRINT_OTHER,
				"case3:tv path normal to get.\n");
		}
	} else {
		if (get_vframe_src_fmt(vf) == VFRAME_SIGNAL_FMT_DOVI && vsync_pts >= 100) {
			dev->low_latency_case = 2;
			vd_print(dev->index, PRINT_OTHER,
				"case2:need next vsync to get.\n");
		} else if ((enable_prelink &&
			!(vf->type_original & VIDTYPE_TYPEMASK)) ||
			(!is_panel_output() && !(vf->type_original & VIDTYPE_TYPEMASK) &&
			get_cpu_type() != MESON_CPU_MAJOR_ID_S5)) {
			vpp_lowlatency_wakeup();
			dev->low_latency_case = 1;
			vd_print(dev->index, PRINT_OTHER,
				"case1:P and prelink vc use low latency2 mode.\n");
		}
	}
	if (dev->low_latency_case == 2)
		next_vsync_wakeup_vpp_to_get = 1;
	if (vsync_pts_inc_scale_base[dev->index])
		elapsed_ns = div_u64(1000000000LL *
			vsync_pts_inc_scale[dev->index],
			vsync_pts_inc_scale_base[dev->index]);
	next_isr_spec_time = isr_spec_time.tv_sec * 1000000000LL +
		isr_spec_time.tv_nsec + elapsed_ns;
	vd_print(dev->index, PRINT_OTHER,
		"elapsed_ns=%llu, next_isr_spec_time=%llu\n",
		elapsed_ns, next_isr_spec_time);
}

static bool is_dtv_source(struct dma_buf *dmabuf, struct vframe_s *vf)
{
	bool is_v4l_vf = false;
	bool is_tv_path;

	if (!dmabuf || !vf)
		return false;

	is_v4l_vf = is_valid_mod_type(dmabuf, VF_PROCESS_V4LVIDEO);
	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		is_tv_path = true;
	else
		is_tv_path = false;

	if (is_v4l_vf && !is_tv_path)
		return true;
	else
		return false;
}

static int videodisplay_drop_frame(struct videodisplay_dev *dev,
	struct vframe_s *vf, struct dma_buf *dmabuf)
{
	bool vf_from_vdin = false;
	bool vf_from_dtv = false;
	bool is_dec_vf = false;
	bool vdin_need_drop = false;
	bool dtv_need_drop = false;
	bool dec_need_drop = false;
	bool dpss_need_drop = false;
	struct timeval cur_time;
	unsigned long time = 0;

	if (vd_drop_org_i == 0)
		return false;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		vf_from_vdin = true;

	if (is_dtv_source(dmabuf, vf))
		vf_from_dtv = true;

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);

	if (vf_from_vdin && (vd_drop_org_i & 1))
		vdin_need_drop = true;
	if (vf_from_dtv && (vd_drop_org_i & 2))
		dtv_need_drop = true;
	if (is_dec_vf && (vd_drop_org_i & 4))
		dec_need_drop = true;
	if (vd_drop_org_i & 8)
		dpss_need_drop = true;

	vd_print(dev->index, PRINT_OTHER,
		"vf_type:%x vf_type_original:%x vf_type_ext:%x\n",
		vf->type, vf->type_original, vf->type_ext);

	vd_print(dev->index, PRINT_OTHER,
		"%s:vdin:%d dtv:%d dec:%d dpss:%d\n",
		__func__, vdin_need_drop, dtv_need_drop, dec_need_drop, dpss_need_drop);

	if (dpss_need_drop && (vf->type_ext & VIDTYPE_EXT_DPSS_DROP)) {
		vd_print(dev->index, PRINT_OTHER,
			"dpss drop: frame_index %d\n", vf->frame_index);
		return true;
	}

	if ((vf->type_ext & VIDTYPE_EXT_DI_BACKEND_DUMMY) &&
		(vf->type & VIDTYPE_INTERLACE) &&
		(vdin_need_drop || dec_need_drop)) {
		vd_print(dev->index, PRINT_OTHER,
				"drop no di process vf, frame_index=%d\n",
				vf->frame_index);

		if (dev->drop_org_i_count == 0) {
			do_gettimeofday(&dev->drop_start_time);
		} else {
			do_gettimeofday(&cur_time);
			time = (cur_time.tv_sec - dev->drop_start_time.tv_sec) * 1000 +
				(cur_time.tv_usec - dev->drop_start_time.tv_usec) / 1000;
			vd_print(dev->index, PRINT_OTHER, "time=%ldms, drop_count=%d\n",
				time, dev->drop_org_i_count);
			if (time > vd_drop_max_time) {
				vd_print(dev->index, PRINT_OTHER,
					"exit drop time =%ldms, drop_count=%d\n",
					time, dev->drop_org_i_count);
				return false;
			}
		}
		dev->drop_org_i_count++;
		return true;
	} else if ((vdin_need_drop || dtv_need_drop) &&
		(vf->type_original & VIDTYPE_INTERLACE) &&
		((vd_drop_hdmi_count + dev->drop_org_i_count) > dev->hdmi_drop_count)) {
		if (last_index[dev->index] != vf->frame_index) {
			dev->hdmi_drop_count++;
			last_index[dev->index] = vf->frame_index;
		}
		vd_print(dev->index, PRINT_OTHER,
			"drop frame_index %d\n", vf->frame_index);
		return true;
	}

	return false;
}

static void vframe_display(struct videodisplay_dev *dev,
				struct received_frames_t *received_frames)
{
	struct frames_info_t *frames_info = NULL;
	struct frame_info_t *frame_info = NULL;
	u32 num = 0;
	struct vframe_s *vf = NULL;
	struct vframe_s *enhance_vf = NULL;
	int ready_count = 0;
	bool is_dec_vf = false, is_v4l_vf = false, is_repeat_vf = false;
	struct vd_prepare_s *vd_prepare = NULL;
	u64 phy_addr2 = 0;
	u64 time_us64;
	struct vframe_s *vf_ext = NULL;
	int pic_w = 0, pic_h = 0;
	bool enable_prelink = false;
	struct uvm_lcevc_frame_info *lcevc_data;
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	struct timespec64 ts;
#endif

	if (!dev || !received_frames) {
		pr_info("%s: NULL param.\n", __func__);
		return;
	}

	frames_info = &received_frames->frames_info;
	num = frames_info->layer_index;
	frame_info = &frames_info->frame_info[num];
	time_us64 = received_frames->time_us64;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	enable_prelink = dim_get_pre_link();
#endif

	detect_vf_type(frame_info, &is_dec_vf, &is_v4l_vf);
	get_dma_buf(frame_info->dmabuf);
	if (dev->last_file == (struct file *)frame_info->dmabuf)
		is_repeat_vf = true;

	if (is_repeat_vf) {
		vd_prepare = dev->vd_prepare_last;
	} else {
		vd_prepare = vd_prepare_data_q_get(dev);
		if (!vd_prepare) {
			vd_print(dev->index, PRINT_ERROR,
				 "%s: get prepare_data failed.\n",
				 __func__);
			return;
		}

		if (is_dec_vf || is_v4l_vf) {
			vf = vd_get_vf_from_buf(dev, frame_info->dmabuf);
			if (!vf) {
				vd_print(dev->index, PRINT_ERROR, "%s: get vf failed.\n", __func__);
				return;
			}
			if (videodisplay_drop_frame(dev, vf, frame_info->dmabuf)) {
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
				ktime_get_real_ts64(&ts);
				vc_notify_msg_to_mediaproxy(dev, vf,
					MEDIA_VIDEO_METRICS_FRAME_TOGGLE_INFO, ts);
#endif
				dma_buf_put(frame_info->dmabuf);
				dma_fence_signal(frame_info->release_fence);
				dma_fence_put(frame_info->release_fence);
				atomic_set(&received_frames->on_use, false);
				vd_prepare_data_q_put(dev, vd_prepare);
				return;
			}
			vd_prepare->src_frame = vf;
			vd_prepare->dst_frame = *vf;
		} else {/*dma buf*/
			dev->drop_org_i_count = 0;
			vd_prepare->src_frame = &vd_prepare->dst_frame;
		}
		vd_prepare->release_fence = frame_info->release_fence;
		dev->vd_prepare_last = vd_prepare;
	}

	vf = &vd_prepare->dst_frame;

	vf->axis[0] = frame_info->dst_x;
	vf->axis[1] = frame_info->dst_y;
	vf->axis[2] = frame_info->dst_w + frame_info->dst_x - 1;
	vf->axis[3] = frame_info->dst_h + frame_info->dst_y - 1;

	if (vd_vf_is_tvin(vf))
		dev->is_tv_path = true;
	else
		dev->is_tv_path = false;
	if (is_dec_vf || is_v4l_vf) {
		if (vf->type & VIDTYPE_COMPRESS) {
			pic_w = vf->compWidth;
			pic_h = vf->compHeight;
		} else {
			pic_w = vf->width;
			pic_h = vf->height;
		}
	} else {
		pic_w = frame_info->buffer_w;
		pic_h = frame_info->buffer_h;
	}
	vf->crop[0] = frame_info->crop_y;
	vf->crop[1] = frame_info->crop_x;
	vf->crop[2] = (pic_h >> vf->dpss_pps_dsy) - frame_info->crop_h - frame_info->crop_y;
	vf->crop[3] = (pic_w >> vf->dpss_pps_dsx) - frame_info->crop_w - frame_info->crop_x;
	vf->zorder = frame_info->zorder;
	vf->flag |= VFRAME_FLAG_VIDEO_COMPOSER | VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
	if (frame_info->rotation == VC_TRANSFORM_ROT_180) {
		vd_print(dev->index, PRINT_AXIS, "%s: need rotate 180.\n", __func__);
		vf->flag |= (VFRAME_FLAG_MIRROR_H | VFRAME_FLAG_MIRROR_V);
	}
	if (!is_repeat_vf)
		vf->pts_us64 = time_us64;
	vf->disp_pts = 0;

	vd_print(dev->index, PRINT_AXIS,
		"org crop: %d %d %d %d\n", vf->crop[0], vf->crop[1], vf->crop[2], vf->crop[3]);
	vd_print(dev->index, PRINT_AXIS,
		"org w h: %d %d %d %d\n", vf->width, vf->height, vf->compWidth, vf->compHeight);
	vd_print(dev->index, PRINT_AXIS,
		"frame buffer: w:%d h:%d\n", frame_info->buffer_w, frame_info->buffer_h);
	vd_print(dev->index, PRINT_AXIS,
		"frame buffer: dst:%d %d %d %d\n", frame_info->dst_x, frame_info->dst_y,
		frame_info->dst_w, frame_info->dst_h);

	/*current all vf from non-tunnel, if vf from vt, should not reset vf->crop*/
	if (is_src_crop_valid(vf->src_crop)) {
		vd_print(dev->index, PRINT_AXIS,
			"src_crop: %d %d %d %d\n",
			vf->src_crop.top,
			vf->src_crop.left,
			vf->src_crop.bottom,
			vf->src_crop.right);
		if (vf->type & VIDTYPE_COMPRESS) {
			vf->crop[2] -= vf->src_crop.bottom;
			vf->crop[3] -= vf->src_crop.right;
			if ((int)vf->crop[2] < 0)
				vf->crop[2] = 0;
			if ((int)vf->crop[3] < 0)
				vf->crop[3] = 0;
		}
	}
	vd_print(dev->index, PRINT_AXIS,
		"final crop: %d %d %d %d\n", vf->crop[0], vf->crop[1], vf->crop[2], vf->crop[3]);

	if (last_index[dev->index] > vf->frame_index) {
		dev->received_new_count = vf->frame_index;
		dev->received_count = vf->frame_index;
		vpp_drop_count = 0;
		vd_print(dev->index, PRINT_PATTERN, "drop cnt reset!!\n");
	}

	if (last_index[dev->index] != vf->frame_index) {
		dev->received_new_count++;
		last_index[dev->index] = vf->frame_index;
	}

	if (dev->index == 0) {
		drop_cnt = vf->frame_index + 1 - dev->received_new_count;
		receive_new_count = dev->received_new_count;
		receive_count = dev->received_count + 1;
		last_frame_index = vf->frame_index;
	} else if (dev->index == 1) {
		drop_cnt_pip = vf->frame_index + 1 - dev->received_new_count;
		receive_new_count_pip = dev->received_new_count;
		receive_count_pip = dev->received_count + 1;
		last_frame_index = vf->frame_index;
	}

	if (is_repeat_vf) {
		vf->repeat_count++;
		ready_count = kfifo_len(&dev->ready_q);
		atomic_set(&received_frames->on_use, false);
		vd_print(dev->index, PRINT_OTHER,
			"%s: repeat frame, repeat_count is %d, ready_q count is %d.\n",
			__func__,
			vf->repeat_count,
			ready_count);
		return;
	}

	/* copy to uvm vf */
	vf_ext = vf->uvm_vf;
	if (vf_ext) {
		vd_print(dev->index, PRINT_OTHER, "%s: config vf_ext.\n", __func__);
		vf_ext->axis[0] = vf->axis[0];
		vf_ext->axis[1] = vf->axis[1];
		vf_ext->axis[2] = vf->axis[2];
		vf_ext->axis[3] = vf->axis[3];
		vf_ext->crop[0] = vf->crop[0];
		vf_ext->crop[1] = vf->crop[1];
		vf_ext->crop[2] = vf->crop[2];
		vf_ext->crop[3] = vf->crop[3];
		vf_ext->zorder = vf->zorder;
		vf_ext->flag |= VFRAME_FLAG_VIDEO_COMPOSER | VFRAME_FLAG_VIDEO_COMPOSER_BYPASS;
		vf_ext->disp_pts = 0;
	}

	if (!(is_dec_vf || is_v4l_vf)) {
		vd_print(dev->index, PRINT_OTHER, "%s: dma buf.\n", __func__);
		if (frame_info->rotation & ROTATION_180_DEGREES)
			vf->flag |= VFRAME_FLAG_MIRROR_H | VFRAME_FLAG_MIRROR_V;

		if (!(vf->dpss_flg & 0x08))
			vf->dpss_flg |= 0x10;
		vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		vf->plane_num = 1;
		vf->canvas0Addr = -1;
		vf->canvas0_config[0].phy_addr = frame_info->phy_addr[0];
		vf->canvas0_config[0].width = frame_info->byte_stride;
		vf->canvas0_config[0].height = frame_info->buffer_h;
		vf->canvas0_config[0].endian = 0;
		vf->canvas1Addr = -1;

		if ((frame_info->type & VIDTYPE_VIU_NV12) ||
			(frame_info->type & VIDTYPE_VIU_NV21)) {
			phy_addr2 = frame_info->phy_addr[1];
			vf->plane_num = 2;
			vf->canvas0_config[1].phy_addr = phy_addr2;
			vf->canvas0_config[1].width = frame_info->byte_stride;
			vf->canvas0_config[1].height = frame_info->buffer_h / 2;
			vf->canvas0_config[1].block_mode =
				CANVAS_BLKMODE_LINEAR;
			/*big endian default support*/
			vf->canvas0_config[1].endian = 0;
			vf->plane_num = 2;
		} else if ((frame_info->type & VIDTYPE_VIU_NV16) ||
			(frame_info->type & VIDTYPE_VIU_NV61)) {
			phy_addr2 = frame_info->phy_addr[1];
			vf->plane_num = 2;
			vf->canvas0_config[1].phy_addr = phy_addr2;
			vf->canvas0_config[1].width = frame_info->buffer_w;
			vf->canvas0_config[1].height = frame_info->buffer_h;
			vf->canvas0_config[1].block_mode =
				CANVAS_BLKMODE_LINEAR;
			/*big endian default support*/
			vf->canvas0_config[1].endian = 0;
			vf->plane_num = 2;
		}

		vf->width = frame_info->buffer_w;
		vf->height = frame_info->buffer_h;
		vf->src_fmt.fmt = (enum vframe_signal_fmt_e)frame_info->signal_fmt;
		vf->type = frame_info->type;
		vf->bitdepth = frame_info->bitdepth;
	}

	if (frame_info->private_flag & 0x1)
		vf->source_type = VFRAME_SOURCE_TYPE_HWC;
	vd_print(dev->index, PRINT_AXIS, "=========dst frame info:==========\n");
	vd_print(dev->index, PRINT_AXIS, "frame_index=%d.\n", vf->frame_index);
	vd_print(dev->index, PRINT_AXIS,
		 "frame aixs x,y,w,h: %d %d %d %d\n",
		 frame_info->dst_x,
		 frame_info->dst_y,
		 frame_info->dst_w,
		 frame_info->dst_h);
	vd_print(dev->index, PRINT_AXIS,
		 "frame crop t,l,b,r: %d %d %d %d\n",
		 frame_info->crop_y,
		 frame_info->crop_x,
		 frame_info->crop_h,
		 frame_info->crop_w);
	vd_print(dev->index, PRINT_AXIS,
		 "axis: %d %d %d %d, crop: %d %d %d %d\n",
		 vf->axis[0],
		 vf->axis[1],
		 vf->axis[2],
		 vf->axis[3],
		 vf->crop[0],
		 vf->crop[1],
		 vf->crop[2],
		 vf->crop[3]);
	vd_print(dev->index, PRINT_AXIS, "vf_width: %d, vf_height: %d\n", vf->width, vf->height);
	vd_print(dev->index, PRINT_AXIS,
		 "canvas0 width x height: %d x %d\n",
		 vf->canvas0_config[0].width,
		 vf->canvas0_config[0].height);
	vd_print(dev->index, PRINT_AXIS, "===============================\n");

	dev->last_file = (struct file *)frame_info->dmabuf;
	vf->vc_private = vd_private_q_pop(dev);
	vf->vc_private->present_fence = frame_info->present_fence;
	if (vf->vf_ext)
		((struct vframe_s *)vf->vf_ext)->vc_private = vf->vc_private;
	vf->file_vf = (struct file *)(frame_info->dmabuf);
	vf->repeat_count = 0;

	if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
		lcevc_data = vc_get_lcevc_data(dev, frame_info->dmabuf);
		if (!lcevc_data)
			vd_print(dev->index, PRINT_ERROR,
				"task: lcevc data is NULL!\n");
		else
			enhance_vf = get_enhance_vf_pointer(dev, vf);
		if (set_vf_lcevc_data(dev, enhance_vf, lcevc_data)) {
			vf->enhance_vf = enhance_vf;
			enhance_vf = vf;
			vf = vf->enhance_vf;
			vf->enhance_vf = enhance_vf;
		} else {
			vd_print(dev->index, PRINT_ERROR,
				"task: set lcevc data failed\n");
			vf->type_ext &= ~VIDTYPE_EXT_LCEVC;
		}
	}

	dev->vd_prepare_last = vd_prepare;

	video_display_push_ready(dev, vf);
	if (vf->vc_private)
		vf->vc_private->vsync_index = received_frames->vsync_index;

	if (vf->dpss_flg & 0x08)
		dev->last_vf_from_dpss = true;

	if (!kfifo_put(&dev->ready_q, (const struct vframe_s *)vf)) {
		vd_print(dev->index, PRINT_ERROR, "%s: put to ready_q failed.\n", __func__);
		return;
	}
	if (is_video_process_in_thread()) {
		calculate_low_latency_case(dev, vf);
	} else {
		dev->low_latency_case = 0;
		vd_print(dev->index, PRINT_OTHER,
			"latency2 mode and normal logic.\n");
	}

	ready_count = kfifo_len(&dev->ready_q);
	vd_print(dev->index, PRINT_OTHER, "%s: ready_q count is %d.\n", __func__, ready_count);
	atomic_set(&received_frames->on_use, false);

	if ((low_latency_en || (is_dec_vf && vf->flag & VFRAME_FLAG_GAME_MODE)) && dev->index == 0)
		proc_lowlatency_frame(0);
}

static void video_display_task(struct videodisplay_dev *dev)
{
	int num = 0;
	struct dma_fence *fence_file = NULL;
	struct received_frames_t *received_frames = NULL;
	bool need_composer = false;
	bool do_mosaic_22 = false;

	if (!kfifo_peek(&dev->receive_q, &received_frames)) {
		vd_print(dev->index, PRINT_ERROR, "%s: receive_q is NULL.\n", __func__);
		return;
	}

	if (!kfifo_get(&dev->receive_q, &received_frames)) {
		vd_print(dev->index, PRINT_ERROR, "%s: get received_frames failed\n", __func__);
		return;
	}

	if (IS_ERR_OR_NULL(received_frames)) {
		vd_print(dev->index, PRINT_ERROR, "%s: received_frames is NULL\n", __func__);
		return;
	}

	vd_print(dev->index, PRINT_OTHER,
		"%s:frames_num=%lld, is_drm=%d, is_tvp=%d.\n",
		__func__,
		received_frames->frames_num,
		received_frames->is_drm,
		received_frames->is_tvp);
	dev->is_drm_enable = received_frames->is_drm;
	num = received_frames->frames_info.layer_index;
	fence_file = received_frames->frames_info.frame_info[num].input_fence;
	vd_print(dev->index, PRINT_OTHER,
		"%s:dmabuf:%px, input_fence:%px, release_fence:%px.\n",
		__func__,
		received_frames->frames_info.frame_info[num].dmabuf,
		received_frames->frames_info.frame_info[num].input_fence,
		received_frames->frames_info.frame_info[num].release_fence);

	if (video_wait_dma_fence(dev, fence_file) == 0)
		return;

	if (!detect_vf_usage(dev, received_frames, &need_composer, &do_mosaic_22)) {
		vd_print(dev->index, PRINT_ERROR, "%s: fail to get vf usage.\n", __func__);
		return;
	}

	if (do_mosaic_22)
		vframe_do_mosaic_22(dev, received_frames);
	else if (need_composer)
		vframe_composer(dev, received_frames);
	else
		vframe_display(dev, received_frames);
}

static void video_display_wait_event(struct videodisplay_dev *dev)
{
	wait_event_interruptible_timeout(dev->wq,
					 (kfifo_len(&dev->receive_q) > 0 &&
					 dev->enable) ||
					 dev->need_free_buffer ||
					 dev->need_unint_receive_q ||
					 dev->need_empty_ready ||
					 dev->thread_need_stop,
					 msecs_to_jiffies(5000));
}

static int video_display_thread(void *data)
{
	struct videodisplay_dev *dev = data;

	vd_print(dev->index, PRINT_OTHER, "thread: started\n");
	dev->thread_stopped = 0;
	while (1) {
		if (kthread_should_stop())
			break;

		if (kfifo_len(&dev->receive_q) == 0)
			video_display_wait_event(dev);

		if (dev->need_empty_ready) {
			vd_print(dev->index, PRINT_OTHER,
				 "empty_ready_queue\n");
			dev->need_empty_ready = false;
			empty_ready_queue(dev);
			dev->last_file = NULL;
			dev->fake_vf.flag |= VFRAME_FLAG_FAKE_FRAME;
			dev->fake_vf.vf_ext = NULL;
			dev->fake_vf.uvm_vf = NULL;
			dev->fake_back_vf = dev->fake_vf;
			if (!kfifo_put(&dev->ready_q,
				       &dev->fake_back_vf))
				vd_print(dev->index, PRINT_ERROR,
					 "by_pass ready_q is full\n");
		}

		if (dev->need_free_buffer) {
			dev->need_free_buffer = false;
			video_composer_uninit_buffer(dev);
			vd_print(dev->index, PRINT_OTHER,
				 "%s video composer release!\n", __func__);
			continue;
		}
		if (kthread_should_stop())
			break;

		if (!dev->enable && dev->need_unint_receive_q) {
			receive_q_uninit(dev);
			dev->need_unint_receive_q = false;
			ready_q_uninit(dev);
			complete(&dev->task_done);
			continue;
		}
		if (kfifo_len(&dev->receive_q) > 0 && dev->enable)
			video_display_task(dev);
	}
	dev->thread_stopped = 1;
	return 0;
}

static int vd_render_index_get(struct videodisplay_dev *dev)
{
	int receiver_id = 0;
	int render_index = 0;

	if (!dev) {
		pr_info("%s: dev is null.\n", __func__);
	} else {
		if (dev->index >= MAX_VIDEODISPLAY_INSTANCE_NUM) {
			pr_info("%s: index(%d) is invalid.\n", __func__, dev->index);
		} else {
			receiver_id = get_receiver_id(dev->index);
			if (receiver_id >= 5)
				render_index = receiver_id - 3;
			else if (receiver_id <= 3)
				render_index = receiver_id - 2;
			else
				render_index = 0;
			vd_print(dev->index, PRINT_ERROR,
				"%s: render_index is %d.\n",
				__func__, render_index);
		}
	}

	return render_index;
}

static void dev_get_vinfo(struct videodisplay_dev *dev)
{
	struct vinfo_s *video_composer_vinfo;
	struct vinfo_s vinfo = {.width = 1280, .height = 720, };
	u64 output_duration;

	video_composer_vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(video_composer_vinfo)) {
		vd_print(dev->index, PRINT_ERROR, "get display vinfo err!!\n");
		video_composer_vinfo = &vinfo;
	}
	output_duration = div64_u64(video_composer_vinfo->sync_duration_num,
		video_composer_vinfo->sync_duration_den);

	dev->vinfo_w = video_composer_vinfo->width;
	dev->vinfo_h = video_composer_vinfo->height;
	dev->output_duration = output_duration;
}

/* -----------------------------------------------------------------
 *           provider operations
 * -----------------------------------------------------------------
 */
static struct vframe_s *vd_vf_peek(void *op_arg)
{
	struct videodisplay_dev *dev = (struct videodisplay_dev *)op_arg;
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
	int di_delay_vsync;
	bool dpss_tvin_enable_pulldown;
	int total_delay_vsync;
	struct timespec64 cur_spec_time;
	int frc_delay = 0;

	time1 = dev->start_time;
	time2 = vsync_time[dev->index];

	if (kfifo_peek(&dev->ready_q, &vf)) {
		frc_delay = get_playback_delay_duration();
		if (dev->last_frc_delay != frc_delay &&
			mediasync_callback &&
			mediasync_callback->mediasync) {
			vd_print(dev->index, PRINT_OTHER, "frc delay(%d) update, callback.\n",
				frc_delay);
			mediasync_callback->mediasync(vf->syncid, 0, &frc_delay);
			dev->last_frc_delay = frc_delay;
		}
		if (is_video_process_in_thread() && dev->low_latency_case == 2) {
			ktime_get_ts64(&cur_spec_time);
			vd_print(dev->index, PRINT_OTHER,
				"cur_spec_time=%llu, next_isr_spec_time=%llu, allow_vpp_to_get=%d\n",
				cur_spec_time.tv_sec * 1000000000LL + cur_spec_time.tv_nsec,
				next_isr_spec_time, allow_vpp_to_get);
			if ((cur_spec_time.tv_sec * 1000000000LL +
				cur_spec_time.tv_nsec < next_isr_spec_time) && !allow_vpp_to_get) {
				vd_print(dev->index, PRINT_OTHER, "vpp to get, not allow to get\n");
				return NULL;
			}
		}

		if (dev->index == 0 && is_meson_t3x_cpu() && check_frc_n2m_status())
			aisr_delay_vsync = 1;
		else
			aisr_delay_vsync = 0;

		/*dpss need force dalay vsync*/
		if (dev->index == 0 && (vf->type_ext & VIDTYPE_EXT_422_10_2P_DPSS)) {
			dpss_tvin_enable_pulldown = true;
			di_delay_vsync = 1;
		} else {
			dpss_tvin_enable_pulldown = false;
			di_delay_vsync = 0;
		}

		total_delay_vsync = aisr_delay_vsync + di_delay_vsync
			+ vd_set_frame_delay[dev->index];

		if (vf->vc_private && total_delay_vsync > 0) {
			vsync_index = vf->vc_private->vsync_index;
			vd_print(dev->index, PRINT_OTHER,
				"peek: vsync_index =%d, delay_count=%d, vsync_count=%d\n",
				vsync_index, total_delay_vsync,
				vsync_count[dev->index]);
			if (vsync_index + total_delay_vsync
				>= vsync_count[dev->index] &&
				vsync_index < vsync_count[dev->index])
				return NULL;
		} else {
			vd_print(dev->index, PRINT_OTHER, "peek: vf->vc_private is NULL\n");
		}

		if ((vf->flag & VFRAME_FLAG_GAME_MODE) && vd_vf_is_tvin(vf) &&
			(dev->index == 1 && dev->video_render_index == 5) &&
			vf->vc_private) {
			vsync_index = vf->vc_private->vsync_index;
			if (vsync_index + 1 >= vsync_count[dev->index])
				return NULL;
		}

		if ((vf->vf_vrr_param.qms_plus_en || vf->vf_vrr_param.qms_en) &&
			get_count[dev->index] == 1) {
			vd_print(dev->index, PRINT_OTHER,
				"QMS only support get 1, can not get more.\n");
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
		vd_print(dev->index, PRINT_OTHER,
			"peek: input_fps=%d, output_fps=%d.\n", input_fps, output_fps);
		/*apk/sf drop 0/3 4; vc receive 1 2 5 in one vsync*/
		/*apk queue 5 and wait 1, it will fence timeout*/
		/* dev->video_render_index == 5 means T7 dual screen mode */
		/*input 120hz with 60hz output or input 100hz with 50hz output no need check*/
		if (vd_vf_is_tvin(vf) && (input_fps * 3 < output_fps * 2) && input_fps <= 14400)
			special_case = true;
		if (!special_case && (get_count[dev->index] == 2 && dev->video_render_index != 5)) {
			vd_print(dev->index, PRINT_ERROR,
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
		if (!is_meson_t7_cpu()) {
			vd_print(dev->index, PRINT_PATTERN,
				 "pts_us64=%lld, time_vsync=%lld\n",
				 vf->pts_us64, time_vsync);

			if (vf->pts_us64 >= time_vsync && vf->pts_us64 < (time_vsync + 10000)) {
				vd_print(dev->index, PRINT_PATTERN,
					 "display next vsync: pts_us64=%lld, time_vsync=%lld\n",
					 vf->pts_us64, time_vsync);
				return NULL;
			}
		}

		if (get_count[dev->index] > 0 &&
			!(vf->flag & VFRAME_FLAG_GAME_MODE)) {
			interval_time = abs(time_vsync - vf->pts_us64);
			vd_print(dev->index, PRINT_PERFORMANCE,
				 "time_vsync=%lld, vf->pts_us64=%lld\n",
				 time_vsync, vf->pts_us64);
			//TODO
			if (interval_time < 2000/*margin_time*/) {
				vd_print(dev->index, PRINT_PATTERN, "display next vsync\n");
				return NULL;
			}
		}

		if (pulldown_support_vf(dev, vf->duration) &&
			(dev->enable_pulldown || enable_new_afr_pattern)) {
			open_pulldown = true;
			ready_len = kfifo_len(&dev->ready_q);
			if ((ready_len > 1 && vd_pulldown_level == 1) ||
				(ready_len > 2 && vd_pulldown_level > 1)) {
				open_pulldown = false;
				vd_print(dev->index, PRINT_PATTERN, "ready_q len=%d\n", ready_len);
			}
		}

		if (open_pulldown &&
			(dpss_tvin_enable_pulldown || !vd_vf_is_tvin(vf)) &&
			!(vf->flag & VFRAME_FLAG_FAKE_FRAME)) {
			if (vf->vc_private) {
				vsync_index = vf->vc_private->vsync_index;
				vd_print(dev->index, PRINT_PATTERN,
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
					vd_print(dev->index, PRINT_PATTERN,
						"need disp, vsync_index =%d, vsync_count=%d\n",
						vsync_index, vsync_count[dev->index]);
					expired = true;
				} else if (expired_tmp && dev->pattern_detected != PATTERN_11) {
					if (dev->last_hold_index + 1 == vf->frame_index)
						dev->continue_hold_count++;
					else if (dev->last_hold_index != vf->frame_index)
						dev->continue_hold_count = 1;
					vd_print(dev->index, PRINT_PATTERN,
						"patten: hold, frame_index =%d, continue_count=%d\n",
						vf->frame_index, dev->continue_hold_count);
					dev->last_hold_index = vf->frame_index;
					if (dev->continue_hold_count >= vd_max_hold_count) {
						expired = true;
						vd_print(dev->index, PRINT_PATTERN,
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

static void vd_vf_put(struct vframe_s *vf, void *op_arg)
{
	struct file *file_vf;
	struct vframe_s *display_vf = vf;
	struct videodisplay_dev *dev = (struct videodisplay_dev *)op_arg;
	struct vd_prepare_s *vd_prepare_tmp;
	struct mbp_buffer_info_t *mpb_buf = NULL;
	bool enable_prelink = false;
	int i, repeat_count;
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	struct timespec64 ts;
#endif

	if (!vf) {
		vd_print(dev->index, PRINT_ERROR, "%s: NULL param.", __func__);
		return;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
	enable_prelink = dim_get_pre_link();
#endif

	if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
		vd_print(dev->index, PRINT_OTHER,
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

	vd_print(dev->index, PRINT_OTHER,
		"%s: prelink_en=%d, vf=%px(%px), frame_index=%d, vf_type=0x%x, vf_flag=0x%x, dpss_flg:%x, vf->timestamp: %lld.\n",
		__func__,
		enable_prelink,
		vf,
		vf->vf_ext,
		vf->frame_index,
		vf->type,
		vf->flag,
		vf->dpss_flg,
		div_u64(vf->timestamp, 1000000000));

	vd_print(dev->index, PRINT_OTHER,
		"%s: fbc:headaddr=0x%lx, bodyaddr=0x%lx, width=%d, height=%d.\n",
		__func__,
		vf->compHeadAddr,
		vf->compBodyAddr,
		vf->compWidth,
		vf->compHeight);

	vd_print(dev->index, PRINT_OTHER,
		"%s: mif:canvasID=%d, y_addr=0x%lx, uv_addr=0x%lx, width=%d, height=%d.\n",
		__func__,
		vf->canvas0Addr,
		vf->canvas0_config[0].phy_addr,
		vf->canvas0_config[1].phy_addr,
		vf->width,
		vf->height);
	repeat_count = vf->repeat_count;

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	ktime_get_real_ts64(&ts);
	if (vf->rendered)
		vc_notify_msg_to_mediaproxy(dev, vf, MEDIA_VIDEO_METRICS_FRAME_SIGNAFENCE_INFO, ts);
#endif
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
	ATRACE_COUNTER("video_composer_put_vf_frame_index", vf->frame_index);
	ATRACE_COUNTER("video_composer_put_vf_frame_index", 0);
	ATRACE_COUNTER("video_composer_put_vf_timestamp", div_u64(vf->timestamp, 1000000000));
	ATRACE_COUNTER("video_composer_put_vf_timestamp", 0);
#endif
	if (dev->is_drm_enable) {
		if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
			vd_print(dev->index, PRINT_OTHER, "put: fake frame\n");
			return;
		}

		vd_prepare_tmp = container_of(vf, struct vd_prepare_s, dst_frame);
		if (IS_ERR_OR_NULL(vd_prepare_tmp)) {
			vd_print(dev->index, PRINT_ERROR, "%s: prepare is NULL.\n", __func__);
			return;
		}

		file_vf = vf->file_vf;

		vf_pop_display_q(dev, display_vf);
		if (!file_vf)
			vd_print(dev->index, PRINT_ERROR, "put: file error!!!\n");

		if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
			vd_print(dev->index, PRINT_OTHER, "put enhance vf:%px\n", vf->enhance_vf);
			if (!kfifo_put(&dev->free_q, vf->enhance_vf))
				vd_print(dev->index, PRINT_ERROR, "put free_q is full\n");
		}

		vd_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			"%s: frame_index=%d, repeat_count=%d.\n",
			__func__, vf->frame_index, vf->repeat_count);

		for (i = 0; i <= repeat_count; i++) {
			if (vf->flag & VFRAME_FLAG_VIDEO_COMPOSER_DMA) {
				vd_print(dev->index, PRINT_FENCE, "put dma buffer!!!\n");
				mpb_buf = (struct mbp_buffer_info_t *)file_vf;
				if (IS_ERR_OR_NULL(mpb_buf)) {
					vd_print(dev->index, PRINT_ERROR,
						"%s: mpb_buf is NULL.\n",
						__func__);
				}
				if (vf->vc_private) {
					vf->vc_private->unlock_buffer_cb(mpb_buf);
					vd_private_q_recycle(dev, vf->vc_private);
					vf->vc_private = NULL;
				}
			} else {
				vd_print(dev->index, PRINT_FENCE,
					"%s: frame_index=%d, release_fence = %px\n",
					__func__,
					vf->frame_index,
					vd_prepare_tmp->release_fence);

				if (vf->vc_private) {
					vd_private_q_recycle(dev, vf->vc_private);
					vf->vc_private = NULL;
				}
				dma_buf_put((struct dma_buf *)file_vf);
				dma_fence_signal(vd_prepare_tmp->release_fence);
				dma_fence_put(vd_prepare_tmp->release_fence);
			}
			dev->fput_count++;
			total_put_count++;
		}
		//all the vframe param used must beforce fence release
		vd_prepare_data_q_put(dev, vd_prepare_tmp);
	} else {
		vf_pop_display_q(dev, display_vf);
		videocomposer_vf_put(vf, op_arg);
	}

	vd_print(dev->index, PRINT_FENCE, "put end, put_count=%lld.\n", dev->fput_count);
}

static struct vframe_s *vd_vf_get(void *op_arg)
{
	struct videodisplay_dev *dev = (struct videodisplay_dev *)op_arg;
	struct vframe_s *vf = NULL;
	u32 vsync_index_diff = 0;
	bool enable_prelink = false;
	bool is_di_postlink = false;
	bool need_mute_video = false;
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	struct timespec64 ts;
#endif

	if (kfifo_get(&dev->ready_q, &vf)) {
		if (!vf) {
			vd_print(dev->index, PRINT_ERROR, "%s: vf is NULL.", __func__);
			return NULL;
		}

		if (vf->flag & VFRAME_FLAG_FAKE_FRAME) {
			vd_print(dev->index, PRINT_OTHER, "%s: fake vf.", __func__);
			return vf;
		}

		if (!kfifo_put(&dev->display_q, vf))
			vd_print(dev->index, PRINT_ERROR, "%s: display_q is full!\n", __func__);
		if (!(vf->flag & VFRAME_FLAG_VIDEO_COMPOSER))
			pr_err("vd: vf_get: flag is null\n");

		if (vf->di_flag && (vf->di_flag & DI_FLAG_DI_PVPPLINK))
			is_di_postlink = true;

		if ((vd_drop_org_i & 1) && vd_vf_is_tvin(vf))
			need_mute_video = true;

		if (dev->index == 0 && dev->fget_count == 0 && is_di_postlink && need_mute_video) {
			set_video_mute(VC_MUTE_SET, true);
			vd_print(dev->index, PRINT_OTHER,
				"mute postlink first vsync\n");
			mute_vpp_vd1 = true;
			mute_vsync_count = 0;
		}

		get_count[dev->index]++;
		if (vf->source_type == VFRAME_SOURCE_TYPE_HWC && vf->width == 384) {
			dev->fget_count = 0;
			dev->hdmi_drop_count = 0;
		} else {
			dev->fget_count++;
		}
		total_get_count++;

#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
		enable_prelink = dim_get_pre_link();
#endif
		if (vf->vc_private) {
			vd_print(dev->index, PRINT_OTHER, "%s:vc_p:%px, present_fence:%px\n",
				__func__, vf->vc_private, vf->vc_private->present_fence);

			vsync_index_diff = vf->vc_private->vsync_index - dev->last_vsync_index;
			dev->last_vsync_index = vf->vc_private->vsync_index;
			if (vf->frame_index < dev->last_vf_index) {
				vd_print(dev->index, PRINT_PATTERN,
					 "change source\n");
				vf->vc_private->flag |= VC_FLAG_FIRST_FRAME;
			}
		}

		vd_print(dev->index, PRINT_OTHER | PRINT_PATTERN,
			 "get:frame_index=%d, index_disp=%d, get=%d, total_get=%lld, vsync =%d, diff=%d, duration=%d\n",
			 vf->frame_index,
			 vf->index_disp,
			 get_count[dev->index],
			 dev->fget_count,
			 continue_vsync_count[dev->index],
			 vsync_index_diff,
			 vf->duration);

		vd_print(dev->index, PRINT_OTHER,
			"%s: prelink_en=%d, vf=%px(%px), frame_index=%d, vf->source_type=0x%x, vf_type=0x%x, vf_flag=0x%x, vf->timestamp: %lld.di_flag=%x, dpss_flg:%x\n",
			__func__,
			enable_prelink,
			vf,
			vf->vf_ext,
			vf->frame_index,
			vf->source_type,
			vf->type,
			vf->flag,
			div_u64(vf->timestamp, 1000000000),
			vf->di_flag,
			vf->dpss_flg);

		vd_print(dev->index, PRINT_OTHER,
			"%s: fbc:headaddr=0x%lx, bodyaddr=0x%lx, width=%d, height=%d.\n",
			__func__,
			vf->compHeadAddr,
			vf->compBodyAddr,
			vf->compWidth,
			vf->compHeight);

		vd_print(dev->index, PRINT_OTHER,
			"%s: mif:canvasID=%d, y_addr=0x%lx, uv_addr=0x%lx, width=%d, height=%d.\n",
			__func__,
			vf->canvas0Addr,
			vf->canvas0_config[0].phy_addr,
			vf->canvas0_config[1].phy_addr,
			vf->width,
			vf->height);

		vd_print(dev->index, PRINT_AXIS,
			 "get:crop: %d %d %d %d, axis: %d %d %d %d.\n",
			 vf->crop[0], vf->crop[1], vf->crop[2], vf->crop[3],
			 vf->axis[0], vf->axis[1], vf->axis[2], vf->axis[3]);
		vd_print(dev->index, PRINT_DEWARP,
			 "get:canvas_w: %d, canvas_h: %d\n",
			  vf->canvas0_config[0].width, vf->canvas0_config[0].height);

		if (vf->vc_private) {
			vf->vc_private->last_disp_count = continue_vsync_count[dev->index];
			actual_delay_count[dev->index] = vsync_count[dev->index]
				- vf->vc_private->vsync_index + 1;
		}

		if (vf->dec_fence_status == DEC_FENCE_SUCCESS) {
			vd_print(dev->index, PRINT_OTHER,
				"%s: normal vframe, frame_index:%d fence:%px\n",
				__func__,
				vf->frame_index,
				vf->fence);
			dma_fence_get(vf->fence);
		} else if (vf->dec_fence_status == DEC_FENCE_ERR) {
			vd_print(dev->index, PRINT_OTHER, "error vframe, frame_index:%d\n",
				vf->frame_index);
			vd_vf_put(vf, (void *)dev);
			return NULL;
		}

		if (dev->enable_pulldown || enable_new_afr_pattern) {
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
		vc_notify_msg_to_mediaproxy(dev, vf,
			MEDIA_VIDEO_METRICS_FRAME_TOGGLE_INFO, ts);
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

static int vd_event_cb(int type, void *data, void *private_data)
{
	return 0;
}

static int vd_vf_states(struct vframe_states *states, void *op_arg)
{
	struct videodisplay_dev *dev = (struct videodisplay_dev *)op_arg;

	states->vf_pool_size = COMPOSER_READY_POOL_SIZE;
	states->buf_recycle_num = 0;
	states->buf_free_num = COMPOSER_READY_POOL_SIZE - kfifo_len(&dev->ready_q);
	states->buf_avail_num = kfifo_len(&dev->ready_q);
	return 0;
}

static const struct vframe_operations_s vd_vf_provider = {
	.peek = vd_vf_peek,
	.get = vd_vf_get,
	.put = vd_vf_put,
	.event_cb = vd_event_cb,
	.vf_states = vd_vf_states,
};

static int video_display_create_path(struct videodisplay_dev *dev)
{
	int i = 0;
	char render_layer[16] = "";

	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	snprintf(dev->vfm_map_chain, VCOM_MAP_NAME_SIZE,
		 "%s %s", dev->vf_provider_name, render_layer);

	snprintf(dev->vfm_map_id, VCOM_MAP_NAME_SIZE, "vcom-map-%d", dev->index);

	if (vfm_map_add(dev->vfm_map_id, dev->vfm_map_chain) < 0) {
		vd_print(dev->index, PRINT_ERROR,
			"%s: vcom pipeline map creation failed %s.\n",
			__func__, dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
		return -ENOMEM;
	}

	vf_provider_init(&dev->vc_vf_prov, dev->vf_provider_name, &vd_vf_provider, dev);

	vf_reg_provider(&dev->vc_vf_prov);

	vf_notify_receiver(dev->vf_provider_name,
			   VFRAME_EVENT_PROVIDER_START, NULL);

	vsync_count[dev->index] = 0;
	dev->fget_count = 0;
	dev->last_vsync_index = 0;
	dev->last_vf_index = 0xffffffff;
	dev->enable_pulldown = false;
	for (i = 0; i < PATTEN_FACTOR_MAX; i++)
		dev->patten_factor[i] = 0;
	dev->patten_factor_index = 0;
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
	if (vd_pulldown_level && frc_get_video_latency()) {
		dev->enable_pulldown = true;
		vd_print(dev->index, PRINT_OTHER, "%s: enable pulldown\n", __func__);
	}
#endif
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
	if (vd_pulldown_level && (is_meson_t6w_cpu() || is_meson_t6x_cpu())) {
		dev->enable_pulldown = true;
		vd_print(dev->index, PRINT_OTHER, "%s: enable pulldown\n", __func__);
	}
#endif
	return 0;
}

static int video_display_release_path(struct videodisplay_dev *dev)
{
	vf_unreg_provider(&dev->vc_vf_prov);

	if (dev->vfm_map_id[0]) {
		vfm_map_remove(dev->vfm_map_id);
		dev->vfm_map_id[0] = 0;
	}
	return 0;
}

static void disable_video_layer(struct videodisplay_dev *dev, int val)
{
	vd_print(dev->index, PRINT_OTHER, "dev->index =%d, val=%d", dev->index, val);
	if (dev->index == 0)
		_video_set_disable(val);
	else
		_videopip_set_disable(dev->index, val);
}

static int video_display_init(struct videodisplay_dev *dev)
{
	int ret;
	int i;
	char render_layer[16] = "";

	if (!dev)
		return -1;

	INIT_KFIFO(dev->ready_q);
	INIT_KFIFO(dev->receive_q);
	INIT_KFIFO(dev->free_q);
	INIT_KFIFO(dev->display_q);
	INIT_KFIFO(dev->vc_prepare_data_q);
	kfifo_reset(&dev->ready_q);
	kfifo_reset(&dev->receive_q);
	kfifo_reset(&dev->free_q);
	kfifo_reset(&dev->display_q);
	kfifo_reset(&dev->vc_prepare_data_q);

	for (i = 0; i < COMPOSER_READY_POOL_SIZE; i++)
		vd_prepare_data_q_put(dev, &dev->vd_prepare[i]);

	vd_private_q_init(dev);
	receive_count = 0;
	receive_new_count = 0;
	total_get_count = 0;
	total_put_count = 0;
	last_frame_index = -1;
	drop_cnt = 0;
	vpp_drop_cnt = 0;
	dev->received_count = 0;
	dev->received_new_count = 0;
	dev->fence_creat_count = 0;
	dev->fence_release_count = 0;
	dev->fput_count = 0;
	dev->last_dst_vf = NULL;
	dev->drop_frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev->last_file = NULL;
	dev->select_path_done = false;
	dev->vd_prepare_last = NULL;
	dev->dev_choice = COMPOSER_WITH_UNINITIAL;
	dev->kfifo_need_initialize = true;
	dev->fence_wait_time_total = 0;
	dev->fence_wait_count = 0;
	dev->last_vf_from_dpss = false;
	dev->drop_org_i_count = 0;
	dev->hdmi_drop_count = 0;
	dev->last_frc_delay = 0;
	dev_array[dev->index] = dev;
	init_completion(&dev->task_done);
	for (i = 0; i < MAX_VD_LAYERS; i++)
		last_index[i] = -1;
	disable_video_layer(dev, 2);
	video_set_global_output(dev->index, 1);

	ret = video_display_create_path(dev);
	sprintf(render_layer, "video_render.%d", dev->video_render_index);
	set_video_path_select(render_layer, dev->index);
	dev_get_vinfo(dev);
#ifdef CONFIG_AMLOGIC_MEDIA_RESMANAGE
	resman_register_debug_callback("Display_VC", set_vc_config);
#endif

	return ret;
}

static int video_display_uninit(struct videodisplay_dev *dev)
{
	int ret;
	int time_left = 0;

	if (dev->is_sideband) {
		if (dev->index == 0)
			set_video_path_select("auto", 0);
	} else {
		if (dev->index == 0)
			set_video_path_select("default", 0);
	}

	set_vdx_blackout_policy(dev->index, 1);
	disable_video_layer(dev, 1);
	video_set_global_output(dev->index, 0);
	ret = video_display_release_path(dev);

	dev->need_unint_receive_q = true;

	/* free buffer */
	dev->need_free_buffer = true;
	wake_up_interruptible(&dev->wq);

	time_left = wait_for_completion_timeout(&dev->task_done, msecs_to_jiffies(500));
	if (!time_left)
		vd_print(dev->index, PRINT_ERROR, "%s: wait timeout\n", __func__);
	else if (time_left < 100)
		vd_print(dev->index, PRINT_ERROR, "%s: wait time %d\n", __func__, time_left);

	display_q_uninit(dev);

	if (dev->fence_creat_count != dev->fput_count) {
		vd_print(dev->index, PRINT_ERROR,
			"%s: fence_r=%lld, fence_c=%lld\n",
			__func__,
			dev->fence_release_count,
			dev->fence_creat_count);
		vd_print(dev->index, PRINT_ERROR,
			"%s: received=%lld, new_cnt=%lld, fget=%lld, fput=%lld\n",
			__func__,
			dev->received_count,
			dev->received_new_count,
			dev->fget_count,
			dev->fput_count);
	}
	video_timeline_increase(dev, dev->fence_creat_count - dev->fence_release_count);
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev_array[dev->index] = NULL;
	video_display_para_reset(dev->index);

	if (dev->aiface_buf) {
		vfree(dev->aiface_buf);
		dev->aiface_buf = NULL;
	}

	return ret;
}

static int get_unused_received_frames_num(struct videodisplay_dev *dev)
{
	int i = 0, j = 0;

	if (!dev)
		return -1;

	while (1) {
		for (i = 0; i < FRAMES_INFO_POOL_SIZE; i++) {
			if (!atomic_read(&dev->received_frames[i].on_use))
				break;
		}
		if (i == FRAMES_INFO_POOL_SIZE) {
			j++;
			if (j > WAIT_READY_Q_TIMEOUT) {
				vd_print(dev->index, PRINT_ERROR,
					"receive_q is full, wait timeout!\n");
				return -1;
			}
			usleep_range(1000 * receive_wait,
				     1000 * (receive_wait + 1));
			vd_print(dev->index, PRINT_ERROR, "receive_q is full, need wait =%d\n", j);
			continue;
		} else {
			break;
		}
	}

	return i;
}

static int video_display_open(int index)
{
	struct videodisplay_dev *dev;
	struct videodisplay_port_s *port = videodisplay_get_port(index);
	struct sched_param param = {.sched_priority = 2};
	u32 layer_cap = 0;

	if (!port) {
		pr_info("%s: vd[%d] not opened.\n", __func__, index);
		return -ENODEV;
	}

	mutex_lock(&videodisplay_mutex);
	if (port->open_count > 0) {
		mutex_unlock(&videodisplay_mutex);
		vd_print(index, PRINT_ERROR, "%s: instance %d is already opened", __func__, index);
		return -EBUSY;
	}

	dev = vmalloc(sizeof(*dev));
	memset(dev, 0, sizeof(*dev));
	if (!dev) {
		mutex_unlock(&videodisplay_mutex);
		vd_print(index, PRINT_ERROR, "%s: instance %d alloc dev failed", __func__, index);
		return -ENOMEM;
	}

	port->video_render_index = dev->video_render_index;
	port->open_count++;
	dev->port = port;
	dev->index = port->index;
	dev->buffer_status = UNINITIAL;
	dev->need_free_buffer = false;
	dev->last_frames.frames_info.frame_count = 0;
	dev->is_sideband = false;
	dev->need_empty_ready = false;
	dev->thread_need_stop = false;
	dev->vframe_dump_flag = 0;

	memcpy(dev->vf_provider_name, port->name, strlen(port->name) + 1);
	dev->video_render_index = vd_render_index_get(dev);
	do_gettimeofday(&dev->start_time);

	mutex_unlock(&videodisplay_mutex);
	dev->kthread = kthread_create(video_display_thread,  dev, dev->port->name);
	if (IS_ERR(dev->kthread)) {
		vd_print(index, PRINT_ERROR, "video_display_thread creat failed\n");
		return -ENOMEM;
	}

	init_waitqueue_head(&dev->wq);
	if (sched_setscheduler_nocheck(dev->kthread, SCHED_FIFO, &param))
		vd_print(index, PRINT_ERROR, "%s: Could not set realtime priority.\n", __func__);

	wake_up_process(dev->kthread);

	video_timeline_create(dev);

	if (dev->index == 0) {
		layer_cap = video_get_layer_capability();
		if (layer_cap & MOSAIC_MODE)
			dev->support_mosaic = true;
	}

	mdev[index] = dev;
	return 0;
}

static int video_display_release(int index)
{
	struct videodisplay_dev *dev = NULL;
	struct videodisplay_port_s *port = NULL;
	int i = 0;

	if (index >= MAX_VIDEODISPLAY_INSTANCE_NUM) {
		pr_info("%s: index(%d) is invalid.\n", __func__, index);
		return -ENODEV;
	}

	dev = mdev[index];
	port = dev->port;

	if (dev->kthread) {
		dev->thread_need_stop = true;
		kthread_stop(dev->kthread);
		wake_up_interruptible(&dev->wq);
		dev->kthread = NULL;
		dev->thread_need_stop = false;
	}

	mutex_lock(&videodisplay_mutex);
	port->open_count--;
	mutex_unlock(&videodisplay_mutex);
	while (1) {
		i++;
		if (dev->thread_stopped)
			break;
		usleep_range(9000, 10000);
		if (i > WAIT_THREAD_STOPPED_TIMEOUT) {
			vd_print(index, PRINT_ERROR, "%s: wait thread timeout\n", __func__);
			break;
		}
	}
	vfree(dev);
	mdev[index] = NULL;
	return 0;
}

int vd_set_enable(int index, u32 val)
{
	int ret = 0;
	struct videodisplay_dev *dev = mdev[index];

	vd_print(index, PRINT_ERROR, "set enable index=%d, val=%d\n", index, val);

	if (val > VIDEO_COMPOSER_ENABLE_NORMAL) {
		vd_print(index, PRINT_ERROR, "%s: invalid param.\n", __func__);
		return -EINVAL;
	}

	if (!dev) {
		if (val == VIDEO_COMPOSER_ENABLE_NORMAL) {
			ret = video_display_open(index);
			if (ret) {
				vd_print(index, PRINT_ERROR, "%s: open Dev failed.\n", __func__);
				return -EINVAL;
			}

			dev = mdev[index];
			dev->enable = VIDEO_COMPOSER_ENABLE_NORMAL;
			ret = video_display_init(dev);
		} else {
			vd_print(index, PRINT_OTHER, "%s: not enable, no need set.\n", __func__);
		}
	} else {
		if (dev->enable == val) {
			vd_print(index, PRINT_OTHER, "%s: no need set again.\n", __func__);
			return ret;
		}

		dev->enable = val;

		if (val == VIDEO_COMPOSER_ENABLE_NORMAL) {
			ret = video_display_init(dev);
		} else if (val == VIDEO_COMPOSER_ENABLE_NONE) {
			wake_up_interruptible(&dev->wq);
			ret = video_display_uninit(dev);
			video_display_release(index);
		}
	}

	if (ret != 0)
		vd_print(dev->index, PRINT_ERROR, "%s failed\n", __func__);

	return ret;
}

int vd_set_frames(int index, struct frames_info_t *frames_info)
{
	struct videodisplay_dev *dev = mdev[index];
	int num = 0;
	struct timeval time1, time2;
	u64 time_us64;
	int i = 0;

	if (IS_ERR_OR_NULL(frames_info) || IS_ERR_OR_NULL(dev)) {
		vd_print(index, PRINT_ERROR, "%s: frame_info is NULL.\n", __func__);
		return -EINVAL;
	}

	vd_print(dev->index, PRINT_AXIS, "********src frame param********.\n");

	vd_print(dev->index, PRINT_AXIS,
		"%s: count:%d, index:%d, z:%d, dmabuf:%px, input_fence:%px, release_fence:%px.\n",
		__func__,
		frames_info->frame_count,
		frames_info->layer_index,
		frames_info->disp_zorder,
		frames_info->frame_info[index].dmabuf,
		frames_info->frame_info[index].input_fence,
		frames_info->frame_info[index].release_fence);

	vd_print(dev->index, PRINT_AXIS,
		"%s: buf: addr0:0x%llx, addr1:0x%llx, stride:%d, w:%d, h:%d.\n",
		__func__,
		frames_info->frame_info[index].phy_addr[0],
		frames_info->frame_info[index].phy_addr[1],
		frames_info->frame_info[index].byte_stride,
		frames_info->frame_info[index].buffer_w,
		frames_info->frame_info[index].buffer_h);

	vd_print(dev->index, PRINT_AXIS,
		"%s: axix: x:%d, y:%d,  w:%d, h:%d.\n",
		__func__,
		frames_info->frame_info[index].dst_x,
		frames_info->frame_info[index].dst_y,
		frames_info->frame_info[index].dst_w,
		frames_info->frame_info[index].dst_h);
	vd_print(dev->index, PRINT_AXIS,
		"%s: crop: x:%d, y:%d,	w:%d, h:%d.\n",
		__func__,
		frames_info->frame_info[index].crop_x,
		frames_info->frame_info[index].crop_y,
		frames_info->frame_info[index].crop_w,
		frames_info->frame_info[index].crop_h);
	vd_print(dev->index, PRINT_AXIS,
		"%s: option: z:%d, signal_fmt:%d, type:0x%x, bitdepth:%d, rotation:%d, pri_flag:%x.\n",
		__func__,
		frames_info->frame_info[index].zorder,
		frames_info->frame_info[index].signal_fmt,
		frames_info->frame_info[index].type,
		frames_info->frame_info[index].bitdepth,
		frames_info->frame_info[index].rotation,
		frames_info->frame_info[index].private_flag);
	vd_print(dev->index, PRINT_AXIS, "***********************************.\n");

	num = get_unused_received_frames_num(dev);
	if (num < 0) {
		vd_print(index, PRINT_ERROR, "%s: received_frames is full.\n", __func__);
		return -EINVAL;
	}

	time1 = dev->start_time;
	do_gettimeofday(&time2);
	time_us64 = (u64)1000000 * (time2.tv_sec - time1.tv_sec) + time2.tv_usec - time1.tv_usec;

	dev->received_frames[num].is_drm = true;
	dev->received_frames[num].frames_num = frames_info->frame_count;
	dev->received_frames[num].time_us64 = time_us64;
	for (i = 0; i < frames_info->frame_count; i++) {
		dev->received_frames[num].file_vf[i] = NULL;
		dev->received_frames[num].fence_file[i] = NULL;
		dev->received_frames[num].phy_addr[i] = 0;
	}

	dev->received_frames[num].frames_info = *frames_info;
	dev->received_frames[num].vsync_index = vsync_count[index];
	dev->received_frames[num].is_tvp = false;
	atomic_set(&dev->received_frames[num].on_use, true);
	dev->received_count++;
	receive_count++;

	if (!kfifo_put(&dev->receive_q, &dev->received_frames[num]))
		vd_print(dev->index, PRINT_ERROR, "%s: put to receive_q fail.\n", __func__);

	vd_print(dev->index, PRINT_OTHER,
		"%s: receive_count=%d, receive_q len is %d.\n",
		__func__,
		receive_count,
		kfifo_len(&dev->receive_q));
	wake_up_interruptible(&dev->wq);

	return 0;
}

void vd_vsync_notify(u8 layer_id, u32 vpp_vsync_pts_inc_scale, u32 vpp_vsync_pts_inc_scale_base)
{
	vsync_count[layer_id]++;
	get_count[layer_id] = 0;
	continue_vsync_count[layer_id]++;
	patten_trace[layer_id]++;

	vsync_pts_inc_scale[layer_id] = vpp_vsync_pts_inc_scale;
	vsync_pts_inc_scale_base[layer_id] = vpp_vsync_pts_inc_scale_base;
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
		calculate_fps_and_vsync(&start_time[layer_id], &end_time[layer_id],
			&start_vsync_count[layer_id], &end_vsync_count[layer_id], layer_id);
		vd_test_fps[layer_id] = 0;
		break;
	}
	default:
		break;
	}
	if (mute_vpp_vd1 && layer_id == 0) {
		mute_vsync_count++;
		vd_print(0, PRINT_OTHER,
			"mute_vsync_count=%d\n", mute_vsync_count);
		if (mute_vsync_count >= 1) {
			set_video_mute(VC_MUTE_SET, false);
			mute_vpp_vd1 = false;
		}
	}
}

int vd_set_mediasync_callback(struct mediasync_cb *func)
{
	if (!func) {
		vd_print(0, PRINT_ERROR, "%s: func is NULL.\n", __func__);
		return -EINVAL;
	}

	mediasync_callback = func;

	return 0;
}
