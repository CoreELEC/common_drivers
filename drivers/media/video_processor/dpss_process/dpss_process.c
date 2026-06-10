// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/sysfs.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/of.h>
#include <uapi/linux/sched/types.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/sched/clock.h>
#include <linux/sync_file.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <linux/amlogic/aml_sync_api.h>
#include <linux/compat.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif

#ifdef CONFIG_AMLOGIC_DPSS_THERMAL
#include "dpss_cooling.h"
#endif
#include "dpss_process.h"
#include "dpss_proc_file.h"
#include "../../common/uvm_process/meson_uvm_lcevc_processor.h"
#include <linux/amlogic/media/video_processor/video_pipe_adapter.h>

#define DPSS_PROCESS_DEVICE_NAME   "di_process"
#define WAIT_THREAD_STOPPED_TIMEOUT 20
#define WAIT_READY_Q_TIMEOUT 100
#define MAX_RECEIVE_WAIT_TIME 15  /*15ms*/
#define CONTINUE_VD1_TOGGLE_NUM 10
#define DPSS_P_MEM_USAGE 127
#define DPSS_I_MEM_USAGE 88
#define DPSS_PPS_I_MEM_USAGE 41

#define signal_color_primaries ((vf->signal_type >> 16) & 0xff)
#define signal_transfer_characteristic ((vf->signal_type >> 8) & 0xff)
#define signal_cuva ((vf->signal_type >> 31) & 1)

static u32 dpss_process_instance_num = DPSS_INSTANCE_COUNT;
static u32 total_get_count;
static u32 total_put_count;
static u32 total_src_get_count;
static u32 total_src_put_count;
static u32 total_fill_count;
static u32 total_fill_done_count;
static u32 total_empty_count;
static u32 total_empty_done_count;
static void *timeline[DPSS_INSTANCE_COUNT];
static u32 cur_streamline_val[DPSS_INSTANCE_COUNT];
static u32 q_dropped = 1;
u32 dpss_buf_mgr_print_flag;
static u32 force_width;
static u32 force_height;
static int dpss_bypass = -1;
static u32 work_mode_ctl;
static u32 work_mode_ctl_pip;
static int continue_dump_num;
static u32 continue_dump_top_num;
static u32 continue_dump_bottom_num;
static bool is_start_dump;
static u32 direct_mode_flag;
static enum direct_mode_override force_direct_mode;
static bool is_main_channel_enabled;
static bool is_dual_channel_enabled;
static bool is_dec_set_direct_mode;
static u32 temperature_control_en;
#ifdef CONFIG_AMLOGIC_DPSS_THERMAL
struct dpss_cooling_device *dpss_cdev_global;
#endif
static u32 force_num_to_vd1;
static atomic_t input_eos_enabled = ATOMIC_INIT(0);
static u32 last_instance_frame_index;
static u32 dpss_out_axis[4];
static u32 display_scaling_factor[4] = {20, 48, 84, 200};
static u32 dpss_rotate_width = 1280;
static u32 dpss_rotate_height = 720;
static u32 rotate_dump_flag;
static u32 dpss_rotate_en = 1;
static u32 hdr_state_flag; /*0:normal 1:need send flag to force pq unmute*/
static DEFINE_MUTEX(dpss_process_mutex);
static DEFINE_MUTEX(play_speed_mutex);
static struct display_speed_t display_speed[DPSSPR_POOL_SIZE];

static struct dpss_process_port_s ports[] = {
	{
		.name = "di_process.0",
		.index = 0,
		.open_count = 0,
	},
	{
		.name = "di_process.1",
		.index = 1,
		.open_count = 0,
	},
	{
		.name = "di_process.2",
		.index = 2,
		.open_count = 0,
	},
};

static void dp_timeline_create(int index)
{
	const char *tl_name = "dp_timeline_0";

	if (index == 0)
		tl_name = "dp_timeline_0";
	else if (index == 1)
		tl_name = "dp_timeline_1";

	cur_streamline_val[index] = 0;
	timeline[index] = aml_sync_create_timeline(tl_name);
	dp_print(index, PRINT_FENCE,
		 "timeline create tlName =%s, timeline=%p\n",
		 tl_name, timeline[index]);
}

static int dp_timeline_create_fence(struct dpss_process_dev *dev)
{
	int out_fence_fd;
	u32 pt_val = 0;

	pt_val = cur_streamline_val[dev->index] + 1;
	dp_print(dev->index, PRINT_FENCE, "creat fence: pt_val %d", pt_val);

	out_fence_fd = aml_sync_create_fence(timeline[dev->index], pt_val);
	if (out_fence_fd >= 0) {
		cur_streamline_val[dev->index]++;
		dev->fence_creat_count++;
	} else {
		dp_print(dev->index, PRINT_ERROR,
			 "create fence returned %d", out_fence_fd);
	}
	return out_fence_fd;
}

static void dp_timeline_increase(struct dpss_process_dev *dev,
				    unsigned int value)
{
	aml_sync_inc_timeline(timeline[dev->index], value);
	dev->fence_release_count += value;
	dp_print(dev->index, PRINT_FENCE,
		"release fence: fen_creat_cnt=%lld,fen_release_cnt=%lld\n",
		dev->fence_creat_count,
		dev->fence_release_count);
}

void dpss_total_fill_count_increase(void)
{
	total_fill_count++;
}
EXPORT_SYMBOL(dpss_total_fill_count_increase);

void dpss_direct_mode_ctrl(bool on)
{
	is_dec_set_direct_mode = on;
}
EXPORT_SYMBOL(dpss_direct_mode_ctrl);

static struct file_private_data *dp_get_file_private(struct dpss_process_dev *dev,
						      struct file *file_vf)
{
	struct file_private_data *file_private_data;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR, "%s: NULL param.\n", __func__);
		return NULL;
	}

	if (is_v4lvideo_buf_file(file_vf)) {
		file_private_data = (struct file_private_data *)(file_vf->private_data);
	} else {
		uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
			VF_PROCESS_V4LVIDEO);
		if (!uhmod) {
			dp_print(dev->index, PRINT_ERROR, "hook mod is NULL\n");
			file_private_data =  NULL;
		} else {
			file_private_data = uhmod->arg;
			uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
				VF_PROCESS_V4LVIDEO);
		}
	}

	if (IS_ERR_OR_NULL(file_private_data))
		dp_print(dev->index, PRINT_ERROR, "dma file file_private_data is NULL\n");
	else
		dp_print(dev->index, PRINT_OTHER, "%s: file_private_data is %px.\n",
			__func__, file_private_data);

	return file_private_data;
}

static struct vframe_s *get_vf_from_file(struct dpss_process_dev *dev,
					 struct file *file_vf)
{
	struct vframe_s *vf = NULL;
	bool is_dec_vf = false;
	bool is_v4l_vf = false;
	struct file_private_data *file_private_data = NULL;

	if (IS_ERR_OR_NULL(dev) || IS_ERR_OR_NULL(file_vf)) {
		dp_print(dev->index, PRINT_ERROR,
			"%s: invalid param.\n",
			__func__);
		return vf;
	}

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		dp_print(dev->index, PRINT_OTHER, "vf is from decoder\n");
		vf =
		dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		if (!vf) {
			dp_print(dev->index, PRINT_ERROR, "vf is NULL.\n");
			return vf;
		}

		dp_print(dev->index, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n",
			vf->type,
			vf->flag);
		dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));
		if (vf->frame_index == 0 && vf->index_disp != 0)
			vf->frame_index = vf->index_disp;
	} else if (is_v4l_vf) {
		dp_print(dev->index, PRINT_MORE, "vf is from v4lvideo\n");
		file_private_data = dp_get_file_private(dev, file_vf);
		if (!file_private_data)
			dp_print(dev->index, PRINT_ERROR,
				 "invalid fd: no uvm, no v4lvideo!!\n");
		else
			vf = &file_private_data->vf;
	}
	return vf;
}

static void video_wait_decode_fence(struct dpss_process_dev *dev,
				    struct vframe_s *vf)
{
	if (vf && vf->fence) {
		u64 timestamp = local_clock();
		s32 ret = dma_fence_wait_timeout(vf->fence, false, 2000);

		dp_print(dev->index, PRINT_FENCE,
			 "%s, fence %lx, state: %d, wait cost time: %lld ns\n",
			 __func__, (ulong)vf->fence, ret,
			 local_clock() - timestamp);
		vf->fence = NULL;
	} else {
		dp_print(dev->index, PRINT_MORE,
			 "decoder fence is NULL\n");
	}
}

static struct file *dp_get_file(struct dpss_process_dev *dev, int fd)
{
	struct file *file_vf = NULL;

	file_vf = fget(fd);
	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR, "fget fd fail\n");
		return NULL;
	}

	dp_print(dev->index, PRINT_OTHER, "%s:get_file=%px, file_count=%ld\n",
		__func__, file_vf, file_count(file_vf));
	total_get_count++;
	total_src_get_count++;
	dev->fget_count++;

	dp_print(dev->index, PRINT_OTHER, "%s:fget_count=%lld.\n",
		__func__, dev->fget_count);
	return file_vf;
}

void dpss_put_file_ext(int dev_index, struct file *file_vf)
{
	if (!file_vf) {
		pr_err("file is NULL!!!\n");
		return;
	}

	dp_print(dev_index, PRINT_OTHER, "%s:put_file=%px, file_count=%ld\n",
		__func__, file_vf, file_count(file_vf));

	fput(file_vf);
	total_put_count++;
	total_src_put_count++;
}
EXPORT_SYMBOL(dpss_put_file_ext);

static void dp_put_file(struct dpss_process_dev *dev, struct file *file_vf)
{
	if (!file_vf) {
		pr_err("file is NULL!!!\n");
		return;
	}

	dpss_put_file_ext(dev->index, file_vf);
	dev->fput_count++;
	dp_print(dev->index, PRINT_OTHER, "%s:fput_count=%lld.\n",
		__func__, dev->fput_count);
}

static int get_received_frame_free_index(struct dpss_process_dev *dev)
{
	int wait_count = 0;
	int i = 0;
	const int wait_min_us = 1000 * MAX_RECEIVE_WAIT_TIME;
	const int wait_max_us = 1000 * (MAX_RECEIVE_WAIT_TIME + 1);

	while (wait_count <= WAIT_READY_Q_TIMEOUT) {
		for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
			if (!atomic_read(&dev->received_frame[i].on_use))
				return i;
		}

		usleep_range(wait_min_us, wait_max_us);
		dp_print(dev->index, PRINT_ERROR,
			"receive_q is full, waiting... (%d)\n", wait_count);
		wait_count++;
	}

	dp_print(dev->index, PRINT_ERROR,
		"receive_q is full, wait timeout after %d attempts!\n",
		wait_count);
	return -1;
}

int get_dpss_out_buf_free_index(struct dpss_process_dev *dev)
{
	int wait_count = 0;
	int i = 0;
	const int wait_min_us = 1000 * MAX_RECEIVE_WAIT_TIME;
	const int wait_max_us = 1000 * (MAX_RECEIVE_WAIT_TIME + 1);

	while (wait_count <= WAIT_READY_Q_TIMEOUT) {
		for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
			if (!atomic_read(&dev->dpss_out_buf[i].on_use))
				return i;
		}

		usleep_range(wait_min_us, wait_max_us);
		dp_print(dev->index, PRINT_ERROR,
			"dpss_out_buf is full, wait count = %d\n", wait_count);
		wait_count++;
	}

	dp_print(dev->index, PRINT_ERROR,
		"dpss_out_buf is full, wait timeout after %d attempts!\n",
		wait_count);
	return -1;
}

static void pop_dpss_out_q(struct dpss_process_dev *dev, struct vframe_s *vf)
{
	struct dpss_out_buf_t *dpss_out_buf = NULL;
	int i = 0;

	mutex_lock(&dev->mutex_dpss_out);
	i = kfifo_len(&dev->dpss_out_q);
	while (i > 0) {
		if (kfifo_get(&dev->dpss_out_q, &dpss_out_buf)) {
			if (vf == dpss_out_buf->vf) {
				atomic_set(&dpss_out_buf->on_use, false);
				break;
			}
			if (!kfifo_put(&dev->dpss_out_q, dpss_out_buf))
				dp_print(dev->index, PRINT_ERROR, "dpss_out_q is full!\n");
		}
		i--;
	}
	mutex_unlock(&dev->mutex_dpss_out);
	if (i == 0)
		dp_print(dev->index, PRINT_ERROR, "not find dpss_buf in dpss_out_q\n");
}

static struct vframe_s *get_enhance_vf_pointer(struct dpss_process_dev *dev,
	struct vframe_s *vf)
{
	int i;
	struct vframe_s *enhance_vf;

	for (i = 0; i < LCEVC_POOL_SIZE; i++) {
		if (!atomic_read(&dev->lcevc_buf[i].on_use))
			break;
	}
	if (i == LCEVC_POOL_SIZE) {
		dp_print(dev->index, PRINT_ERROR,
			 "task: free_q is empty, can not provide enhance_vf\n");
		enhance_vf = NULL;

	} else {
		enhance_vf = &dev->lcevc_buf[i].lcevc_vf;
		enhance_vf->frame_index = vf->frame_index;
		dev->lcevc_buf[i].dec_vf = vf;
		atomic_set(&dev->lcevc_buf[i].on_use, true);
		dp_print(dev->index, PRINT_OTHER,
			 "task: get_enhance_vf:%px\n", enhance_vf);
	}

	return enhance_vf;
}

static void free_enhance_vf_pointer(struct dpss_process_dev *dev, struct vframe_s *dec_vf)
{
	int i;

	if (dec_vf) {
		for (i = 0; i < LCEVC_POOL_SIZE; i++) {
			if (dec_vf == dev->lcevc_buf[i].dec_vf)
				break;
		}
	} else {
		dp_print(dev->index, PRINT_ERROR,
			 "%s: input param err!\n", __func__);
		return;
	}

	if (i == LCEVC_POOL_SIZE) {
		dp_print(dev->index, PRINT_ERROR,
			 "%s: enhance_vf is not use!\n", __func__);
		return;
	}
	if (!atomic_read(&dev->lcevc_buf[i].on_use)) {
		dp_print(dev->index, PRINT_ERROR,
			 "%s: enhance_vf is in use but flag not set!\n", __func__);
		return;
	}
	dp_print(dev->index, PRINT_OTHER,
		 "%s: free enhance_vf %px!\n", __func__, &dev->lcevc_buf[i].lcevc_vf);

	dev->lcevc_buf[i].dec_vf = NULL;
	memset(&dev->lcevc_buf[i].lcevc_vf, 0, sizeof(dev->lcevc_buf[i].lcevc_vf));
	atomic_set(&dev->lcevc_buf[i].on_use, false);
}

static struct  uvm_lcevc_frame_info *vc_get_lcevc_data(struct dpss_process_dev *dev,
						     struct file *file_vf)
{
	struct uvm_lcevc_hook_data *hook_data;
	struct uvm_lcevc_frame_info *lcevc_data;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR, "vc get lcevc data fail\n");
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 PROCESS_LCEVC);
	if (!uhmod) {
		dp_print(dev->index, PRINT_OTHER, "%s:dma file file_private_data is NULL 1\n",
			__func__);
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		dp_print(dev->index, PRINT_ERROR, "%s: dma file file_private_data is NULL 2\n",
			__func__);
		return NULL;
	}
	hook_data = uhmod->arg;
	lcevc_data = &hook_data->lcevc_vframe;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
			 PROCESS_LCEVC);

	return lcevc_data;
}

static bool set_vf_lcevc_data(struct dpss_process_dev *dev,
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
		dp_print(dev->index, PRINT_OTHER, "get Residual data type: Y10bitPacked\n");
		enhance_vf->type = VIDTYPE_VIU_FIELD
			| VIDTYPE_VIU_444
			| VIDTYPE_VIU_SINGLE_PLANE;
		enhance_vf->type_ext |= VIDTYPE_EXT_LUMA_ONLY;
		enhance_vf->bitdepth = BITDEPTH_Y10;
	} else if (lcevc_data->frame_type == LCEVC_RESIDUAL_UYVY10BIT_PACKED) {
		dp_print(dev->index, PRINT_OTHER, "get Residual data type: UYVY10bitPacked\n");
		enhance_vf->type = VIDTYPE_VIU_FIELD |
			VIDTYPE_VIU_422 |
			VIDTYPE_VIU_SINGLE_PLANE;
		enhance_vf->bitdepth = BITDEPTH_Y10 |
			BITDEPTH_U10 |
			BITDEPTH_V10 |
			FULL_PACK_422_MODE;
	} else {
		dp_print(dev->index, PRINT_OTHER,
			"get unknown Residual data type: %d\n",
			lcevc_data->frame_type);
		return false;
	}

	memcpy(&enhance_vf->scaler_coeff, &lcevc_data->upsample_kernel, sizeof(struct vf_lcevc_t));
	len = enhance_vf->scaler_coeff.len;
	dp_print(dev->index, PRINT_OTHER, "task: coeff len:%d", len);
	for (i = 0; i < len; i++) {
		dp_print(dev->index, PRINT_OTHER,
				"task: coeff[%d] %d %d\n",
				i,
				enhance_vf->scaler_coeff.k[0][i],
				enhance_vf->scaler_coeff.k[1][i]);
	}
	dp_print(dev->index, PRINT_OTHER,
		"task: enhance_vf width:%d, height:%d, enhance_vf yuv addr:%px\n",
		enhance_vf->width,
		enhance_vf->height,
		(void *)enhance_vf->canvas0_config[0].phy_addr);

	return true;
}

static int init_rotate_buffer(struct dpss_process_dev *dev, bool is_tvp, size_t usage)
{
	int ret = 0;
	int i, flags;
	u32 buf_width, buf_height, buf_size;

	switch (dev->rotate_buf_status) {
	case UNINITIAL:/*not config*/
		break;
	case INIT_SUCCESS:/*config before , return ok*/
		return 0;
	case INIT_ERROR:/*config fail, won't retry , return failure*/
		return -1;
	default:
		return -1;
	}

	buf_width = dpss_rotate_width;
	buf_height = (dpss_rotate_height + 0x3f) & ~0x3f;
	buf_size = buf_width * buf_height * 3 / 2;
	buf_size = PAGE_ALIGN(buf_size);

	if (is_tvp)
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_TVP;
	else
		flags = CODEC_MM_FLAGS_DMA | CODEC_MM_FLAGS_CMA_CLEAR;

	for (i = 0; i < ROTATE_POOL_SIZE; i++) {
		if (dev->rotate_buf[i].phy_addr == 0)
			dev->rotate_buf[i].phy_addr = codec_mm_alloc_for_dma(ports[dev->index].name,
				buf_size / PAGE_SIZE, 0, flags);
		dp_print(dev->index, PRINT_ERROR,
			"%s: alloc cma addr is %lx, size is %x.\n",
			ports[dev->index].name,
			dev->rotate_buf[i].phy_addr,
			(unsigned int)buf_size);

		if (dev->rotate_buf[i].phy_addr == 0) {
			dev->rotate_buf_status = INIT_ERROR;
			dp_print(dev->index, PRINT_ERROR, "cma memory config fail\n");
			return -1;
		}
		dev->rotate_buf[i].index = i;
		dev->rotate_buf[i].dirty = false;
		dev->rotate_buf[i].buf_w = buf_width;
		dev->rotate_buf[i].buf_h = buf_height;
		dev->rotate_buf[i].buf_size = buf_size;
		dev->rotate_buf[i].is_tvp = is_tvp;
		dev->rotate_buf[i].buf_used = USED_BY_GE2D;
		memset(&dev->rotate_buf[i].frame, 0, sizeof(struct vframe_s));

		if (!kfifo_put(&dev->dpss_rotate_q, &dev->rotate_buf[i]))
			dp_print(dev->index, PRINT_ERROR, "init buffer free_q is full\n");
	}

	if (IS_ERR_OR_NULL(dev->ge2d_para.context))
		ret = init_ge2d(dev->index, &dev->ge2d_para);

	if (ret < 0) {
		dp_print(dev->index, PRINT_ERROR, "create ge2d context failed.\n");
		return -1;
	}

	dev->rotate_buf_status = INIT_SUCCESS;
	return 0;
}

static void uninit_rotate_buffer(struct dpss_process_dev *dev)
{
	int i;
	int ret = 0;

	if (dev->rotate_buf_status == UNINITIAL)
		return;

	if (!IS_ERR_OR_NULL(dev->ge2d_para.context)) {
		ret = uninit_ge2d(dev->index, &dev->ge2d_para);
		if (ret < 0)
			dp_print(dev->index, PRINT_ERROR, "uninit ge2d composer failed!\n");
		dev->ge2d_para.context = NULL;
	}

	dev->rotate_buf_status = UNINITIAL;
	for (i = 0; i < ROTATE_POOL_SIZE; i++) {
		if (dev->rotate_buf[i].phy_addr != 0) {
			dp_print(dev->index, PRINT_ERROR,
				"%s: free cma addr is %lx.\n",
				ports[dev->index].name,
				(unsigned long)dev->rotate_buf[i].phy_addr);
			codec_mm_free_for_dma(ports[dev->index].name, dev->rotate_buf[i].phy_addr);
			dev->rotate_buf[i].phy_addr = 0;
		}
	}

	INIT_KFIFO(dev->dpss_rotate_q);
	kfifo_reset(&dev->dpss_rotate_q);
}

static struct vframe_s *vframe_rotate(struct dpss_process_dev *dev, struct file *file_vf,
	int transform)
{
	struct vframe_s *src_vf = NULL;
	size_t usage = 0;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;
	int ret = 0;
	struct vframe_s *dst_vf = NULL;
	struct rotate_buf_t *dst_buf = NULL;
	struct src_data_para src_data = {0};
	bool is_tvp = false;
	u32 transform_tmp;

	if (!dev || !file_vf)
		return NULL;

	src_vf = get_vf_from_file(dev, file_vf);
	if (!src_vf)
		goto exit;

	do_gettimeofday(&begin_time);

	if (src_vf->flag & VFRAME_FLAG_VIDEO_SECURE)
		is_tvp = true;
#ifdef CONFIG_AMLOGIC_UVM_CORE
	if (!file_vf->private_data)
		goto exit;

	if (dmabuf_is_uvm(file_vf->private_data) &&
		meson_uvm_get_usage(file_vf->private_data, &usage) < 0)
		dp_print(dev->index, PRINT_ERROR, "rotate: meson_uvm_get_usage fail.\n");
#endif
	dev->ge2d_para.format = GE2D_FORMAT_M24_NV21;
	dev->ge2d_para.plane_num = 2;
	dev->ge2d_para.is_tvp = is_tvp;
	dev->ge2d_para.canvas0_addr = -1;

	ret = init_rotate_buffer(dev, is_tvp, usage);
	if (ret != 0) {
		dp_print(dev->index, PRINT_ERROR, "init buffer failed!\n");
		uninit_rotate_buffer(dev);
		goto exit;
	}

	ret = kfifo_get(&dev->dpss_rotate_q, &dst_buf);
	if (!ret || !dst_buf) {
		dp_print(dev->index, PRINT_ERROR, "get dst_buf failed.\n");
		goto exit;
	}

	dst_vf = &dst_buf->frame;
	dst_buf->src_file = file_vf;
	memset(dst_vf, 0, sizeof(struct vframe_s));
	dp_print(dev->index, PRINT_ROTATE, "rotate:dst_buf[%d]:file:%px, addr:0x%lx, w*h:%d*%d.\n",
		dst_buf->index,
		dst_buf->src_file,
		dst_buf->phy_addr,
		dst_buf->buf_w,
		dst_buf->buf_h);

	dev->ge2d_para.phy_addr[0] = dst_buf->phy_addr;
	dev->ge2d_para.buffer_w = dst_buf->buf_w;
	dev->ge2d_para.buffer_h = dst_buf->buf_h;

	if (dst_buf->dirty) {
		ret = ge2d_fill_black(dev->index, &dev->ge2d_para);
		if (ret < 0)
			dp_print(dev->index, PRINT_ERROR, "ge2d fill black failed\n");
		dst_buf->dirty = false;
	}

	src_data.buf_format = NV21;
	ret = config_ge2d(dev->index, src_vf, 0, 0, 0, 0, 0, 0, 0, src_vf->width, src_vf->height,
		&src_data);
	if (ret < 0)
		goto exit;

	transform_tmp = transform;
	if (src_vf->flag & VFRAME_FLAG_MIRROR_H)
		transform_tmp ^= DPSS_TRANSFORM_FLIP_H;

	if (src_vf->flag & VFRAME_FLAG_MIRROR_V)
		transform_tmp ^= DPSS_TRANSFORM_FLIP_V;

	dev->ge2d_para.angle = transform_tmp;
	dev->ge2d_para.position_left = 0;
	dev->ge2d_para.position_top = 0;
	dev->ge2d_para.position_width = dpss_rotate_width;
	dev->ge2d_para.position_height = dpss_rotate_height;
	dev->ge2d_para.buffer_w = dst_buf->buf_w;
	dev->ge2d_para.buffer_h = dst_buf->buf_h;

	ret = ge2d_data_process(dev->index, &src_data, &dev->ge2d_para);
	if (ret < 0)
		goto exit;

	dst_vf->frame_index = src_vf->frame_index;
	dst_vf->ready_jiffies64 = src_vf->ready_jiffies64;
	dst_vf->duration = src_vf->duration;
	dst_vf->src_fmt = src_vf->src_fmt;
	dst_vf->signal_type = src_vf->signal_type;
	dst_vf->source_type = src_vf->source_type;
	if (src_vf->flag & VFRAME_FLAG_PC_MODE)
		dst_vf->flag |= VFRAME_FLAG_PC_MODE;
	if (src_vf->flag & VFRAME_FLAG_GAME_MODE)
		dst_vf->flag |= VFRAME_FLAG_GAME_MODE;
	if (is_tvp)
		dst_vf->flag |= VFRAME_FLAG_VIDEO_SECURE;
	dst_vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
	dst_vf->bitdepth = (BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8);
	dst_vf->type = (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_NV21);
	dst_vf->dpss_flg |= DPSS_FLG_ROTATE_DONE;
	dst_vf->width = dpss_rotate_width;
	dst_vf->height = dpss_rotate_height;
	dst_vf->canvas0Addr = -1;
	dst_vf->canvas1Addr = -1;
	dst_vf->canvas0_config[0].phy_addr = dst_buf->phy_addr;
	dst_vf->canvas0_config[0].width = dst_buf->buf_w;
	dst_vf->canvas0_config[0].height = dst_buf->buf_h;
	dst_vf->canvas0_config[0].block_mode = 0;
	dst_vf->canvas0_config[1].phy_addr = dst_buf->phy_addr + dst_buf->buf_w * dst_buf->buf_h;
	dst_vf->canvas0_config[1].width = dst_buf->buf_w;
	dst_vf->canvas0_config[1].height = dst_buf->buf_h >> 1;
	dst_vf->canvas0_config[1].block_mode = 0;
	dst_vf->plane_num = 2;

	dp_print(dev->index, PRINT_ROTATE,
		"dst_vf: canvas0_addr: 0x%lx, canvas0_w: %d, canvas0_h: %d.\n",
		dst_vf->canvas0_config[0].phy_addr,
		dst_vf->canvas0_config[0].width,
		dst_vf->canvas0_config[0].height);
	dp_print(dev->index, PRINT_ROTATE,
		"dst_vf: canvas1_addr:  0x%lx, canvas1_w: %d, canvas1_h: %d.\n",
		dst_vf->canvas0_config[1].phy_addr,
		dst_vf->canvas0_config[1].width,
		dst_vf->canvas0_config[1].height);

	do_gettimeofday(&end_time);
	cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
		+ (end_time.tv_usec - begin_time.tv_usec));
	dp_print(dev->index, PRINT_ROTATE, "rotate cost: %d us\n", cost_time);

	return dst_vf;

exit:
	dp_print(dev->index, PRINT_ERROR, "rotate: invalid param.\n");
	return NULL;
}

static int check_dropped(struct dpss_process_dev *dev, struct uvm_di_mgr_t *uvm_di_mgr,
	struct vframe_s *vf)
{
	int index_dpssff = 0;
	struct dp_buf_mgr_t *buf_mgr;

	if (uvm_di_mgr) {
		buf_mgr = uvm_di_mgr->buf_mgr;
	} else {
		dp_print(dev->index, PRINT_ERROR, "uvm_di_mgr is NULL.\n");
		return 0;
	}

	if (dev->last_dec_type != buf_mgr->dec_type ||
		dev->last_instance_id != buf_mgr->instance_id ||
		dev->last_buf_mgr_reset_id != buf_mgr->reset_id) {
		dp_print(dev->index, PRINT_OTHER, "dec instance changed\n");

		dev->last_dec_type = buf_mgr->dec_type;
		dev->last_instance_id = buf_mgr->instance_id;
		dev->last_buf_mgr_reset_id = buf_mgr->reset_id;
		dev->last_frame_index = vf->frame_index;
	} else {
		index_dpssff = vf->frame_index - dev->last_frame_index;

		dev->last_dec_type = buf_mgr->dec_type;
		dev->last_instance_id = buf_mgr->instance_id;
		dev->last_buf_mgr_reset_id = buf_mgr->reset_id;
		dev->last_frame_index = vf->frame_index;
		return index_dpssff - 1;
	}
	return 0;
}

static void dump_vf(struct vframe_s *vf)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	struct file *fp;
	char name_buf[32];
	int write_size;
	u8 *data = NULL;
	loff_t pos;
	struct canvas_s cs0;
	struct canvas_s cs1;
	int flags;

	if (!vf)
		return;

	snprintf(name_buf, sizeof(name_buf), "/data/src.yuv");
	if (is_start_dump) {
		flags = O_CREAT | O_WRONLY | O_TRUNC;
		is_start_dump = 0;
	} else {
		flags = O_CREAT | O_RDWR | O_APPEND;
	}
	fp = filp_open(name_buf, flags, 0644);
	if (IS_ERR(fp))
		return;

	pr_info("dpss:dump frame_index=%d, type=%x, flag=%x, plane_num=%d, %s\n",
		vf->frame_index, vf->type, vf->flag, vf->plane_num,
		(vf->bitdepth & BITDEPTH_Y10) ? "10bit" : "8bit");

	if (vf->canvas0Addr == (u32)-1) {
		write_size = vf->canvas0_config[0].width * vf->canvas0_config[0].height;
		data = codec_mm_vmap(vf->canvas0_config[0].phy_addr, write_size);
		if (!data) {
			filp_close(fp, NULL);
			return;
		}
		pos = vfs_llseek(fp, 0, SEEK_END);
		kernel_write(fp, data, write_size, &pos);
		pr_info("dpss: dump plane1 %u bytes to file at %p\n",
			write_size, data);
		codec_mm_unmap_phyaddr(data);

		if (vf->plane_num == 2) {
			if (vf->type & VIDTYPE_VIU_422) {
				write_size = vf->canvas0_config[1].width *
					vf->canvas0_config[1].height;
			} else if (vf->type & VIDTYPE_VIU_NV21 || vf->type & VIDTYPE_VIU_NV12) {
				write_size = vf->canvas0_config[1].width *
					vf->canvas0_config[1].height / 2;
			} else {
				pr_info("dpss:dump format not support.\n");
				return;
			}
			data = codec_mm_vmap(vf->canvas0_config[1].phy_addr, write_size);
			if (!data) {
				filp_close(fp, NULL);
				return;
			}
			pos = vfs_llseek(fp, 0, SEEK_END);
			kernel_write(fp, data, write_size, &pos);
			pr_info("dpss: dump plane2 %u bytes to file at %p\n",
				write_size, data);
			codec_mm_unmap_phyaddr(data);
		}
		pr_info("dpss:dump phy_addr0=%lx, %d %d, phy_addr1=%lx, %d %d\n",
			vf->canvas0_config[0].phy_addr,
			vf->canvas0_config[0].width,
			vf->canvas0_config[0].height,
			vf->canvas0_config[1].phy_addr,
			vf->canvas0_config[1].width,
			vf->canvas0_config[1].height);
	} else {
		canvas_read(vf->canvas0Addr & 0xff, &cs0);
		canvas_read((vf->canvas0Addr >> 8) & 0xff, &cs1);

		write_size = cs0.width * cs0.height;
		data = codec_mm_vmap(cs0.addr, write_size);
		if (!data) {
			filp_close(fp, NULL);
			return;
		}
		pos = vfs_llseek(fp, 0, SEEK_END);
		kernel_write(fp, data, write_size, &pos);
		pr_info("dpss: dump %u bytes to file at %p\n",
			write_size, data);
		codec_mm_unmap_phyaddr(data);

		if (vf->plane_num == 2) {
			if (vf->type & VIDTYPE_VIU_422) {
				write_size = cs1.width * cs1.height;
			} else if (vf->type & VIDTYPE_VIU_NV21 || vf->type & VIDTYPE_VIU_NV12) {
				write_size = cs1.width * cs1.height / 2;
			} else {
				pr_info("dpss:dump format not support.\n");
				return;
			}
			data = codec_mm_vmap(cs1.addr, write_size);
			if (!data) {
				filp_close(fp, NULL);
				return;
			}
			pos = vfs_llseek(fp, 0, SEEK_END);
			kernel_write(fp, data, write_size, &pos);
			pr_info("dpss: dump uv %u bytes to file at %p\n",
				write_size, data);
			codec_mm_unmap_phyaddr(data);
		}

		pr_info("dpss: dump phy_addr0=%lx, %d %d, phy_addr1=%lx, %d %d\n",
			(unsigned long)cs0.addr, cs0.width, cs0.height,
			(unsigned long)cs1.addr, cs1.width, cs1.height);
	}

	filp_close(fp, NULL);
#endif
}

static int queue_input_to_dpss(struct dpss_process_dev *dev, struct vframe_s *vf,
	bool dropped, struct file *src_file, bool dummy)
{
	struct dpss_in_buf_t *dpss_in_buf = NULL;
	struct pp_info_t *pp_info;
	int ret;
	struct timeval begin_time;
	struct timeval end_time;
	int cost_time;

	if (!kfifo_get(&dev->dpss_input_free_q, &dpss_in_buf)) {
		dp_print(dev->index, PRINT_ERROR, "get dpss_input_free_q err !!!\n");
		return -1;
	}

	/*tmp code for dpss decontour, need remove in the future;*/
	/*1080p video afbc 1920*1088, dw 960*540, dct not support*/
	if (is_src_crop_valid(vf->src_crop)) {
		if (vf->type & VIDTYPE_COMPRESS) {
			dp_print(dev->index, PRINT_OTHER,
				"%s: compWidth =%d, compHeight=%d,crop_right=%d, crop_bottom=%d\n",
				__func__,
				vf->compWidth,
				vf->compHeight,
				vf->src_crop.right,
				vf->src_crop.bottom);
			vf->compHeight -= vf->src_crop.bottom;
			vf->compWidth -= vf->src_crop.right;
			vf->src_crop.bottom = 0;
			vf->src_crop.right = 0;
		}
	}

	struct vframe_s *enhance_vf;

	enhance_vf = (struct vframe_s *)vf->enhance_vf;
	if (enhance_vf) {
		if (is_src_crop_valid(enhance_vf->src_crop)) {
			if (enhance_vf->type & VIDTYPE_COMPRESS) {
				dp_print(dev->index, PRINT_OTHER,
					"%s:compWidth =%d, compHeight=%d,crop_right=%d, crop_bottom=%d\n",
					__func__,
					enhance_vf->compWidth,
					enhance_vf->compHeight,
					enhance_vf->src_crop.right,
					enhance_vf->src_crop.bottom);
				enhance_vf->compHeight -= enhance_vf->src_crop.bottom;
				enhance_vf->compWidth -= enhance_vf->src_crop.right;
				enhance_vf->src_crop.bottom = 0;
				enhance_vf->src_crop.right = 0;
			}
		}
	}

	if (force_width != 0)
		vf->width = force_width;

	if (force_height != 0)
		vf->height = force_height;

	//memcpy(dpss_in_buf->vf, vf, sizeof(struct vframe_s));
	dpss_in_buf->vf = *vf;

	pp_info = &dpss_in_buf->pp_info;

	memset((void *)pp_info, 0, sizeof(struct pp_info_t));
	pp_info->main_index = 0;

	pp_info->flag = 0;
	pp_info->queued = true;
	pp_info->dropped = dropped;
	pp_info->dummy = dummy;
	pp_info->src_file = src_file;
	pp_info->status = DPSS_INPUT_VF_IN_DPSS;
	pp_info->dpss_flag = vf->dpss_flg;
	dpss_in_buf->vf.pp_info = (void *)pp_info;
	dpss_in_buf->vf.dpss_data = NULL;

	if (vf->dpss_flg & DPSS_FLG_USE_DW) {
		dpss_in_buf->vf.type &= ~VIDTYPE_COMPRESS;
		dpss_in_buf->vf.compWidth = 0;
		dpss_in_buf->vf.compHeight = 0;
		dpss_in_buf->vf.bitdepth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
		if (vf->height & 1) {
			dp_print(dev->index, PRINT_OTHER, "height not 2 aligned.\n");
			dpss_in_buf->vf.height -= 1;
		}
	}

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, magic_code=0x%x, ud_addr=%p, ud_len=%d, type=%x, flag=%x, compWidth=%d, compHeight=%d, width=%d, height=%d.\n",
		__func__,
		vf->frame_index,
		vf->vf_ud_param.magic_code,
		vf->vf_ud_param.ud_param.pbuf_addr,
		vf->vf_ud_param.ud_param.buf_len,
		vf->type,
		vf->flag,
		vf->compWidth,
		vf->compHeight,
		vf->width,
		vf->height);

	if (vf->type & VIDTYPE_FORCE_SIGN_IP_JOINT) {
		vf->type &= ~VIDTYPE_FORCE_SIGN_IP_JOINT;
		dp_print(dev->index, PRINT_OTHER, "rm IP_JOINT type\n");
	}

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, queued=%d, dropped=%d, dummy=%d, file:%px.\n",
		__func__,
		vf->frame_index,
		pp_info->queued,
		pp_info->dropped,
		pp_info->dummy,
		pp_info->src_file);

	ret = dpss_empty_input_buffer(dev->dpss_index, &dpss_in_buf->vf);
	if (ret != 0) {
		pp_info->queued = false;
		dp_print(dev->index, PRINT_ERROR,
			"%s err: ret=%d, dpss_index=%d\n", __func__, ret, dev->dpss_index);
		return ret;
	}
	do_gettimeofday(&begin_time);

	if (continue_dump_num) {
		dp_print(dev->index, PRINT_OTHER, "continue_dump_num=%d\n",
			continue_dump_num);
		dump_vf(&dpss_in_buf->vf);
		continue_dump_num--;
	}
	if (continue_dump_top_num &&
		(vf->type & VIDTYPE_INTERLACE_BOTTOM) == VIDTYPE_INTERLACE_TOP) {
		dp_print(dev->index, PRINT_OTHER, "continue_dump_top_num=%d\n",
			continue_dump_top_num);
		dump_vf(&dpss_in_buf->vf);
		continue_dump_top_num--;
	}
	if (continue_dump_bottom_num &&
		(vf->type & VIDTYPE_INTERLACE_BOTTOM) == VIDTYPE_INTERLACE_BOTTOM) {
		dp_print(dev->index, PRINT_OTHER, "continue_dump_bottom_num=%d\n",
			continue_dump_bottom_num);
		dump_vf(&dpss_in_buf->vf);
		continue_dump_bottom_num--;
	}
	do_gettimeofday(&end_time);

	cost_time = (1000000 * (end_time.tv_sec - begin_time.tv_sec)
		+ (end_time.tv_usec - begin_time.tv_usec));
	dp_print(dev->index, PRINT_OTHER, "dump cost: %d us\n", cost_time);

	dev->empty_count++;
	total_empty_count++;

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, empty_count = %lld, %d\n",
		__func__,
		vf->frame_index,
		dev->empty_count,
		total_empty_count);

	return 0;
}

static enum DPSS_ERRORTYPE queue_outbuf_to_dpss(struct dpss_process_dev *dev, struct vframe_s *vf)
{
	enum DPSS_ERRORTYPE ret = 0;
	int frame_index = vf->frame_index;

	ret = dpss_fill_output_buffer(dev->dpss_index, vf);

	dev->fill_count++;
	total_fill_count++;
	dp_print(dev->index, PRINT_OTHER,
		"qbuf done: fill_count=%lld, total_fill_count=%d, frame_index=%d\n",
		dev->fill_count, total_fill_count, frame_index);
	return ret;
}

static void check_window_config(struct dpss_process_dev *dev, struct vframe_s *vf,
	struct received_frame_t *received_frame)
{
	int axis[4] = {0, 0, 0, 0};
	int used_vf_with = 0, used_vf_height = 0;

	if (!dev || !vf || !received_frame) {
		pr_err("window_config: param is invalid.\n");
		return;
	}

	if (vf->type & VIDTYPE_COMPRESS) {
		used_vf_with = vf->compWidth;
		used_vf_height = vf->compHeight;
	} else {
		used_vf_with = vf->width;
		used_vf_height = vf->height;
	}

	if (dpss_out_axis[3]) {
		dp_print(dev->index, PRINT_OTHER, "window_config:use debug mode param.\n");
		axis[0] = dpss_out_axis[0];
		axis[1] = dpss_out_axis[1];
		axis[2] = dpss_out_axis[2];
		axis[3] = dpss_out_axis[3];
	} else {
		axis[0] = received_frame->dst_x;
		axis[1] = received_frame->dst_y;
		axis[2] = received_frame->dst_w;
		axis[3] = received_frame->dst_h;
	}

	if (axis[3] * 100 > display_scaling_factor[1] * used_vf_height &&
		axis[3] * 100 < display_scaling_factor[2] * used_vf_height) {
		dp_print(dev->index, PRINT_OTHER, "window_config: nr need skip.\n");
		vf->dpss_flg |= DPSS_FLG_NEED_SKIP | DPSS_FLG_PPS_SKIP;
	} else if (axis[3] * 100 > display_scaling_factor[0] * used_vf_height &&
		axis[3] * 100 <= display_scaling_factor[1] * used_vf_height) {
		dp_print(dev->index, PRINT_OTHER, "window_config: nr need use dw.\n");
		vf->dpss_flg |= DPSS_FLG_USE_DW;
	} else {
		dp_print(dev->index, PRINT_OTHER, "window_config:normal dpss flow.\n");
		if (axis[3] * 100 < display_scaling_factor[3] * used_vf_height) {
			dp_print(dev->index, PRINT_OTHER, "need bypass pps.\n");
			vf->dpss_flg |= DPSS_FLG_PPS_SKIP;
		}
	}
}

static void dpss_process_task(struct dpss_process_dev *dev)
{
	struct vframe_s *vf = NULL;
	struct file *file_vf = NULL;
	struct received_frame_t *received_frame = NULL;
	struct uvm_di_mgr_t *uvm_di_mgr = NULL;
	int drop_count;
	bool is_i_frame;
	struct vframe_s *vf_1;
	struct vframe_s *vf_2;
	struct file *file_1 = NULL;
	struct file *file_2 = NULL;
	int ret;
	struct dpss_in_buf_t *dpss_in_buf = NULL;
	bool need_q_drop = true;
	int need_process_count;
	bool dummy;
	struct vframe_s *enhance_vf = NULL;
	struct uvm_lcevc_frame_info *lcevc_data;
	bool is_tv_path = false;

	while (kfifo_len(&dev->dpss_out_free_q) > 0) {
		if (kfifo_get(&dev->dpss_out_free_q, &vf)) {
			ret = queue_outbuf_to_dpss(dev, vf);
			if (ret != DPSS_ERR_NONE)
				dp_print(dev->index, PRINT_OTHER,
					"%s: failed, release\n", __func__);
		}
	}

	if (!kfifo_peek(&dev->dpss_input_free_q, &dpss_in_buf)) {
		dp_print(dev->index, PRINT_ERROR, "dpss_input_free_q empty\n");
		usleep_range(2000, 3000);
		return;
	}

	if (!kfifo_get(&dev->receive_q, &received_frame)) {
		dp_print(dev->index, PRINT_ERROR, "task: get frame failed\n");
		usleep_range(2000, 3000);
		return;
	}

	if (IS_ERR_OR_NULL(received_frame)) {
		dp_print(dev->index, PRINT_ERROR,
			 "task: get received_frames is NULL\n");
		return;
	}

	file_vf = received_frame->file_vf;
	dummy = received_frame->dummy;

	if (received_frame->transform == 4 || received_frame->transform == 7) {
		dp_print(dev->index, PRINT_OTHER, "rotate frame.\n");
		dev->rotate_en = true;
		vf = vframe_rotate(dev, file_vf, received_frame->transform);
	} else  {
		vf = received_frame->vf;
	}

	if (!vf) {
		dp_print(dev->index, PRINT_ERROR, "%s:vf is NULL\n", __func__);
		return;
	}

	if (!file_vf && !dummy)
		dp_print(dev->index, PRINT_ERROR, "file_vf is NULL\n");

	is_i_frame = vf->type & VIDTYPE_INTERLACE_TOP;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		is_tv_path = true;
	if (!is_tv_path)
		check_window_config(dev, vf, received_frame);
	if (dev->direct_mode_en && is_meson_t6w_cpu())
		vf->dpss_flg |= DPSS_FLG_HIGH_BANDWIDTH;

	if (dummy) {
		drop_count = 0;
	} else {
		uvm_di_mgr = get_uvm_di_mgr(file_vf);
		drop_count = check_dropped(dev, uvm_di_mgr, vf);
	}

	need_process_count = DPSSPR_POOL_SIZE - kfifo_len(&dev->dpss_input_free_q);
	dp_print(dev->index, PRINT_OTHER, "need_process_count %d\n", need_process_count);
	/*   dpss cannot drop interlace frame for now.
	 *if (need_process_count > 0) {
	 *	need_q_drop = false;
	 *	dp_print(dev->index, PRINT_OTHER, "too many buf need process %d, so not q_drop\n",
	 *		need_process_count);
	 *}
	 */

	if (is_i_frame && drop_count && q_dropped && need_q_drop) {
		dp_print(dev->index, PRINT_OTHER, "drop_count %d\n", drop_count);
		buf_mgr_file_lock(uvm_di_mgr);
		ret = di_get_ref_vf(file_vf, &vf_1, &vf_2, &file_1, &file_2);
		if (ret)
			dp_print(dev->index, PRINT_ERROR, "di_get_ref_vf failed.\n");

		/*err frame not to dpss*/
		if (vf_1 && (vf_1->frame_type & V4L2_BUF_FLAG_ERROR)) {
			dp_print(dev->index, PRINT_OTHER, "vf_1 is error frame, skip\n");
			vf_1 = NULL;
			file_1 = NULL;
		}
		if (vf_2 && (vf_2->frame_type & V4L2_BUF_FLAG_ERROR)) {
			dp_print(dev->index, PRINT_OTHER, "vf_2 is error frame, skip\n");
			vf_2 = NULL;
			file_2 = NULL;
		}

		/*this time file_1 or file_2 has been free, it will panic when get file*/
		if (drop_count == 1 || (drop_count > 2 && drop_count % 2)) {
			if (vf_1 && file_1) {
				get_file(file_1);
				buf_mgr_file_unlock(uvm_di_mgr);
				video_wait_decode_fence(dev, vf_1);
				ret = queue_input_to_dpss(dev, vf_1, true, file_1, false);
				if (ret != 0)
					fput(file_1);
			} else {
				buf_mgr_file_unlock(uvm_di_mgr);
				dp_print(dev->index, PRINT_ERROR, "drop 1 but not find ref\n");
			}
		} else if (drop_count == 2) {
			if (vf_1 && vf_2 && file_1 && file_2) {
				get_file(file_1);
				get_file(file_2);
				buf_mgr_file_unlock(uvm_di_mgr);
				video_wait_decode_fence(dev, vf_2);
				ret = queue_input_to_dpss(dev, vf_2, true, file_2, false);
				if (ret != 0)
					fput(file_1);
				video_wait_decode_fence(dev, vf_1);
				ret = queue_input_to_dpss(dev, vf_1, true, file_1, false);
				if (ret != 0)
					fput(file_2);
			} else if (vf_1 && file_1 && (!vf_2 || !file_2)) {
				get_file(file_1);
				buf_mgr_file_unlock(uvm_di_mgr);
				video_wait_decode_fence(dev, vf_1);
				ret = queue_input_to_dpss(dev, vf_1, true, file_1, false);
				if (ret != 0)
					fput(file_1);
			} else {
				buf_mgr_file_unlock(uvm_di_mgr);
				dp_print(dev->index, PRINT_ERROR, "drop 2 but not find ref\n");
			}
		} else {
			buf_mgr_file_unlock(uvm_di_mgr);
			dp_print(dev->index, PRINT_OTHER, "drop too many frame %d\n", drop_count);
		}
	}

	received_frame->file_vf = NULL;
	received_frame->vf = NULL;
	received_frame->dummy = false;
	received_frame->transform = 0;
	received_frame->dst_x = 0;
	received_frame->dst_y = 0;
	received_frame->dst_w = 0;
	received_frame->dst_h = 0;
	atomic_set(&received_frame->on_use, false);

	if (!dummy) {
		video_wait_decode_fence(dev, vf);

		if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
			lcevc_data = vc_get_lcevc_data(dev, file_vf);
			if (!lcevc_data)
				dp_print(dev->index, PRINT_ERROR,
					"task: lcevc data is NULL!\n");
			else
				enhance_vf = get_enhance_vf_pointer(dev, vf);
			ret = set_vf_lcevc_data(dev, enhance_vf, lcevc_data);
			if (ret) {
				vf->enhance_vf = enhance_vf;
				enhance_vf = vf;
				vf = vf->enhance_vf;
				vf->enhance_vf = enhance_vf;
			} else {
				dp_print(dev->index, PRINT_ERROR,
					"task: set lcevc data failed\n");
				vf->type_ext &= ~VIDTYPE_EXT_LCEVC;
				free_enhance_vf_pointer(dev, vf);
			}
		}

		queue_input_to_dpss(dev, vf, false, file_vf, false);
	} else {
		queue_input_to_dpss(dev, vf, true, file_vf, true);
	}
}

static void dpss_process_wait_event(struct dpss_process_dev *dev)
{
	wait_event_interruptible_timeout(dev->wq,
					 kfifo_len(&dev->receive_q) > 0 ||
					 dev->thread_need_stop,
					 msecs_to_jiffies(5000));
}

static int dpss_process_thread(void *data)
{
	struct dpss_process_dev *dev = data;

	dp_print(dev->index, PRINT_OTHER, "thread: started\n");

	dev->thread_stopped = 0;
	while (1) {
		if (kthread_should_stop())
			break;

		if (kfifo_len(&dev->receive_q) == 0)
			dpss_process_wait_event(dev);

		if (kthread_should_stop())
			break;

		if (kfifo_len(&dev->receive_q) > 0)
			dpss_process_task(dev);
	}
	dev->thread_stopped = 1;
	dp_print(dev->index, PRINT_OTHER, "thread: exit\n");
	return 0;
}

static enum DPSS_ERRORTYPE dp_empty_input_done(void *arg, struct vframe_s *vf)
{
	struct dpss_process_dev *dev = (struct dpss_process_dev *)arg;
	struct pp_info_t *pp_info;
	struct dpss_in_buf_t *dpss_in_buf = NULL;
	struct vframe_s *enhance_vf;
	int ret = 0;
	struct dma_buf *dmabuf;
	struct file_private_data *private_data = NULL;
	struct rotate_buf_t *rotate_buf = NULL;
	int i = 0;

	if (!vf || !dev) {
		pr_err("%s: NULL param.\n", __func__);
		return 0;
	}

	pp_info = (struct pp_info_t *)vf->pp_info;
	if (!pp_info) {
		dp_print(dev->index, PRINT_ERROR, "%s: pp_info is NULL\n", __func__);
		return 0;
	}
	dp_print(dev->index, PRINT_OTHER, "%s: dpss_flg=%x\n",
		__func__, vf->dpss_flg);
	//if (buf->flag & DPSS_FLAG_EOS)
	//	dp_print(dev->index, PRINT_ERROR, "%s: eos\n", __func__);

	if (vf->type_ext & VIDTYPE_EXT_LCEVC) {
		dp_print(dev->index, PRINT_OTHER, "empty enhance vf.\n");
		enhance_vf = (struct vframe_s *)vf->enhance_vf;
		dp_print(dev->index, PRINT_OTHER,
			"%s: enhance_vf:frame_index:%d, w*h=%d*%d.\n",
			__func__,
			enhance_vf->frame_index,
			enhance_vf->width,
			enhance_vf->height);
		free_enhance_vf_pointer(dev, enhance_vf);
	}
	if (vf->dpss_flg & DPSS_FLG_BYPASS) {
		dp_print(dev->index, PRINT_OTHER, "%s: dpss driver bypass\n", __func__);
		/*need release fence*/
		/*need copy vf*/

		if (pp_info->dummy) {
			dev->first_out = true;
			dp_print(dev->index, PRINT_OTHER, "bypass: dummy frame out\n");
		} else {
			if (!kfifo_get(&dev->file_wait_q, &dmabuf)) {
				dp_print(dev->index, PRINT_ERROR, "get file wait fail!!!\n");
				return -EINVAL;
			}
			private_data = v4lvideo_get_file_private_data(dmabuf->file, false);
			if (!private_data) {
				dp_print(dev->index, PRINT_ERROR,
					"%s:private_data is null\n", __func__);
				return 0;
			}
			dp_print(dev->index, PRINT_OTHER,
				"%s: dmabuf =%px, dmabuf->file =%px, private_data =%px, vf=%px\n",
				__func__,
				dmabuf,
				dmabuf->file,
				private_data, vf);

			private_data->vf = *vf;
			private_data->vf_p = vf;
			//private_data->is_keep = true;
			private_data->flag = V4LVIDEO_FLAG_DI_BYPASS;
			dp_timeline_increase(dev, 1);
		}
	}

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, queued=%d, dropped=%d, dummy=%d,file:%px.\n",
		__func__,
		vf->frame_index,
		pp_info->queued,
		pp_info->dropped,
		pp_info->dummy,
		pp_info->src_file);

	if (pp_info->dropped && !pp_info->dummy) {
		dp_print(dev->index, PRINT_OTHER, "%s: fput drop file %px\n",
			__func__, pp_info->src_file);
		fput(pp_info->src_file);
	}

	dev->empty_done_count++;
	total_empty_done_count++;

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, empty_done_count=%lld, total_empty_done_count=%d\n",
		__func__,
		vf->frame_index,
		dev->empty_done_count,
		total_empty_done_count);

	pp_info->queued = false;
	pp_info->status = DPSS_INPUT_VF_OUT_DPSS;

	if (!pp_info->dropped) {
		ret = di_processed_checkin(pp_info->src_file);
		if (ret)
			dp_print(dev->index, PRINT_ERROR, "%s: processed_checkin fail\n", __func__);

		dp_put_file(dev, pp_info->src_file);
	}

	if (dev->rotate_en) {
		for (i = 0; i < ROTATE_POOL_SIZE; i++) {
			if (dev->rotate_buf[i].src_file == pp_info->src_file)
				break;
		}

		if (i == ROTATE_POOL_SIZE) {
			dp_print(dev->index, PRINT_ERROR, "invalid vf.\n");
		} else {
			rotate_buf = &dev->rotate_buf[i];
			dp_print(dev->index, PRINT_ROTATE, "%s: buf[%d]: addr:0x%lx, w*h:%d*%d.\n",
				__func__,
				rotate_buf->index,
				rotate_buf->phy_addr,
				rotate_buf->buf_w,
				rotate_buf->buf_h);
			if (!kfifo_put(&dev->dpss_rotate_q, rotate_buf))
				dp_print(dev->index, PRINT_ERROR, "put dpss_rotate_q fail.\n");
		}
	}

	dpss_in_buf = container_of(vf, struct dpss_in_buf_t, vf);

	if (!kfifo_put(&dev->dpss_input_free_q, dpss_in_buf))
		dp_print(dev->index, PRINT_ERROR, "%s: put dpss_input_free_q fail\n", __func__);

	return ret;
}

static enum DPSS_ERRORTYPE dp_fill_output_done(void *arg, struct vframe_s *vf)
{
	struct dpss_process_dev *dev = (struct dpss_process_dev *)arg;
	struct dma_buf *dmabuf;
	struct file_private_data *private_data = NULL;
	int dpss_out_index;
	enum DPSS_ERRORTYPE ret = 0;
	struct pp_info_t *pp_info;
	struct dpss_out_vf_info dpss_out_info;

	if (!vf || !dev) {
		pr_err("%s: NULL param.\n", __func__);
		return 0;
	}

	dev->fill_done_count++;
	total_fill_done_count++;

	if (dev->first_out)
		dp_print(dev->index, PRINT_OTHER, "%s: DPSS output first frame\n", __func__);

	pp_info = (struct pp_info_t *)vf->pp_info;
	if (!pp_info) {
		dp_print(dev->index, PRINT_ERROR, "%s: pp_info is NULL\n", __func__);
		return 0;
	}

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, queued=%d, dropped=%d, dummy=%d,file:%px.\n",
		__func__,
		vf->frame_index,
		pp_info->queued,
		pp_info->dropped,
		pp_info->dummy,
		pp_info->src_file);

	if (pp_info->dummy) {
		dev->first_out = true;
		dp_print(dev->index, PRINT_OTHER, "dummy frame out\n");
	}

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, fill_done_count =%lld, %d\n",
		__func__,
		vf->frame_index,
		dev->fill_done_count,
		total_fill_done_count);

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, magic_code=0x%x, ud_addr=%p, ud_len=%d.\n",
		__func__,
		vf->frame_index,
		vf->vf_ud_param.magic_code,
		vf->vf_ud_param.ud_param.pbuf_addr,
		vf->vf_ud_param.ud_param.buf_len);

	if (pp_info->dropped) {
		dp_print(dev->index, PRINT_OTHER, "%s:dropped_\n", __func__);
		if (!kfifo_put(&dev->dpss_out_free_q, vf))
			dp_print(dev->index, PRINT_ERROR, "put dpss_out_q fail\n");
		wake_up_interruptible(&dev->wq);
		return 0;
	}

	memset(&dpss_out_info, 0, sizeof(struct dpss_out_vf_info));
	ret = dpss_get_vf_info(vf, &dpss_out_info);
	if (ret != 0)
		dp_print(dev->index, PRINT_OTHER, "dpss_get_vf_info err %d\n", ret);
	dp_print(dev->index, PRINT_OTHER, "idx_m idx_s (%d,%d)\n",
		dpss_out_info.idx_m, dpss_out_info.idx_s);

	if (!kfifo_get(&dev->file_wait_q, &dmabuf)) {
		dp_print(dev->index, PRINT_ERROR, "get file wait fail!!!\n");
		return -EINVAL;
	}
	private_data = v4lvideo_get_file_private_data(dmabuf->file, false);
	if (!private_data) {
		dp_print(dev->index, PRINT_ERROR, "%s:private_data is null\n", __func__);
		return 0;
	}
	dp_print(dev->index, PRINT_OTHER,
		"%s: dmabuf =%px, dmabuf->file =%px, private_data =%px, vf=%px\n",
		__func__,
		dmabuf,
		dmabuf->file,
		private_data, vf);

	if (is_ud_param_valid(vf->vf_ud_param))
		vf->vf_ud_param.ud_param.pbuf_addr = private_data->p_ud_param;
	else
		dp_print(dev->index, PRINT_OTHER, "%s: no ud.\n", __func__);

	if (dev->dpss_parm.dps_work_mode == DPSS_WORK_MODE_FRC)
		vf->dpss_flg |= DPSS_FLG_OUT_BYPASS | DPSS_FLG_SET_DPSS_PROCESS;
	else
		vf->dpss_flg |= DPSS_FLG_OUT_DONE;

	if (pp_info->dpss_flag & DPSS_FLG_NEED_SKIP)
		vf->dpss_flg |= DPSS_FLG_NEED_SKIP;

	if (pp_info->dpss_flag & DPSS_FLG_USE_DW)
		vf->dpss_flg |= DPSS_FLG_USE_DW;

	if (pp_info->dpss_flag & DPSS_FLG_PPS_SKIP)
		vf->dpss_flg |= DPSS_FLG_PPS_SKIP;

	if (pp_info->dpss_flag & DPSS_FLG_HIGH_BANDWIDTH)
		vf->dpss_flg |= DPSS_FLG_HIGH_BANDWIDTH;

	if (pp_info->dpss_flag & DPSS_FLG_ROTATE_DONE)
		vf->dpss_flg |= DPSS_FLG_ROTATE_DONE;

	private_data->vf = *vf;
	private_data->vf_p = vf;
	private_data->vf.vf_ext2 = &private_data->vf_ext;
	private_data->vf.flag_ext |= VFRAME_FLAG_EXT_CONTAIN_SRC;

	private_data->flag = V4LVIDEO_FLAG_DI_V3;
	//private_data->private2 = (void *)buf;
	private_data->is_keep = true;

	if (pp_info->status == DPSS_INPUT_VF_IN_DPSS) {
		//dp_get_file_ext(dev, pp_info->src_file);
		update_di_process_state(pp_info->src_file);
	}

	dpss_out_index = get_dpss_out_buf_free_index(dev);
	if (dpss_out_index < 0) {
		dp_print(dev->index, PRINT_ERROR, "%s: get free buf index fail.\n", __func__);
		return 0;
	}

	atomic_set(&dev->dpss_out_buf[dpss_out_index].on_use, true);
	dev->dpss_out_buf[dpss_out_index].vf = vf;
	dev->dpss_out_buf[dpss_out_index].private_data = private_data;
	mutex_lock(&dev->mutex_dpss_out);
	if (!kfifo_put(&dev->dpss_out_q, &dev->dpss_out_buf[dpss_out_index]))
		dp_print(dev->index, PRINT_ERROR, "put dpss_out_q fail\n");
	mutex_unlock(&dev->mutex_dpss_out);

	dp_timeline_increase(dev, 1);
	return 0;
}

enum DPSS_ERRORTYPE get_input_vf_info(void *arg, struct vframe_s *vf,
	struct dpss_in_vf_info *status)
{
	struct dpss_process_dev *dev = (struct dpss_process_dev *)arg;
	struct pp_info_t *pp_info;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return 0;
	}

	if (!vf) {
		dp_print(dev->index, PRINT_ERROR, "%s: vf is NULL\n", __func__);
		return 0;
	}

	pp_info = (struct pp_info_t *)vf->pp_info;
	if (!pp_info) {
		dp_print(dev->index, PRINT_ERROR, "%s: pp_info is NULL\n", __func__);
		return 0;
	}
	status->idx_m = pp_info->main_index;

	return 0;
}

int get_dpss_process_need_mem(int width, int height, int source_type)
{
	int dpss_need_mem = 0;

	if (source_type == 0 && !is_main_channel_enabled) {
		dpss_need_mem = DPSS_P_MEM_USAGE;
	} else if (source_type == 1) {
		dpss_need_mem = DPSS_I_MEM_USAGE;
		if (is_meson_t6x_cpu() && !is_main_channel_enabled)
			dpss_need_mem += DPSS_PPS_I_MEM_USAGE;
	}
	return dpss_need_mem;
}
EXPORT_SYMBOL(get_dpss_process_need_mem);

static void file_q_init(struct dpss_process_dev *dev)
{
	int i;
	struct dma_buf *dmabuf;

	INIT_KFIFO(dev->file_wait_q);
	kfifo_reset(&dev->file_wait_q);

	INIT_KFIFO(dev->file_free_q);
	kfifo_reset(&dev->file_free_q);

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		dmabuf = uvm_alloc_dmabuf(SZ_4K, 0, 0);
		if (!dmabuf_is_uvm(dmabuf))
			dp_print(dev->index, PRINT_ERROR, "dmabuf is not uvm\n");
		dp_print(dev->index, PRINT_OTHER, "dmabuf is uvm:dmabuf=%px\n", dmabuf);

		dev->out_dmabuf[i] = dmabuf;
		if (!kfifo_put(&dev->file_free_q, dev->out_dmabuf[i]))
			dp_print(dev->index, PRINT_ERROR,
				"file_free_q put fail\n");
	}
}

static void file_q_uninit(struct dpss_process_dev *dev)
{
	int i;

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		if (dev->out_dmabuf[i])
			dma_buf_put(dev->out_dmabuf[i]);
		else
			dp_print(dev->index, PRINT_ERROR, "unreg: dmabuf is NULL\n");
		dev->out_dmabuf[i] = NULL;
	}
}

static void dpss_input_free_q_init(struct dpss_process_dev *dev)
{
	int i = 0;

	INIT_KFIFO(dev->dpss_input_free_q);
	kfifo_reset(&dev->dpss_input_free_q);

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		memset((void *)&dev->dpss_in_buf[i], 0, sizeof(struct dpss_in_buf_t));
		dev->dpss_in_buf[i].index = i;
		kfifo_put(&dev->dpss_input_free_q, &dev->dpss_in_buf[i]);
	}
}

static void receive_q_init(struct dpss_process_dev *dev)
{
	int i = 0;

	INIT_KFIFO(dev->receive_q);
	kfifo_reset(&dev->receive_q);

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		dev->received_frame[i].index = i;
		dev->received_frame[i].file_vf = NULL;
		atomic_set(&dev->received_frame[i].on_use, false);
	}
}

static void receive_q_uninit(struct dpss_process_dev *dev)
{
	int i = 0;
	struct received_frame_t *received_frame = NULL;

	dp_print(dev->index, PRINT_OTHER, "unit receive_q len=%d\n",
		 kfifo_len(&dev->receive_q));
	while (kfifo_len(&dev->receive_q) > 0) {
		if (kfifo_get(&dev->receive_q, &received_frame))
			dp_put_file(dev, received_frame->file_vf);
	}

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		atomic_set(&dev->received_frame[i].on_use, false);
		dev->received_frame[i].file_vf = NULL;
	}
}

static void dpss_out_q_init(struct dpss_process_dev *dev)
{
	int i = 0;

	mutex_lock(&dev->mutex_dpss_out);
	INIT_KFIFO(dev->dpss_out_q);
	kfifo_reset(&dev->dpss_out_q);

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		dev->dpss_out_buf[i].index = i;
		dev->dpss_out_buf[i].vf = NULL;
		atomic_set(&dev->dpss_out_buf[i].on_use, false);
	}
	mutex_unlock(&dev->mutex_dpss_out);
}

static void dpss_out_q_uninit(struct dpss_process_dev *dev)
{
	int i = 0;
	int keep_id = 0;
	int keep_head_id = 0, keep_table_id = 0, keep_dw_id = 0;
	struct dpss_out_buf_t *dpss_out_buf = NULL;
	struct vframe_s *vf;

	mutex_lock(&dev->mutex_dpss_out);
	dp_print(dev->index, PRINT_OTHER, "%s: len=%d\n", __func__, kfifo_len(&dev->dpss_out_q));

	while (kfifo_len(&dev->dpss_out_q) > 0) {
		if (kfifo_get(&dev->dpss_out_q, &dpss_out_buf)) {
			if (!dpss_out_buf || !dpss_out_buf->private_data || !dpss_out_buf->vf) {
				dp_print(dev->index, PRINT_OTHER, "dpss_out_buf is NULL.\n");
				continue;
			}

			vf = dpss_out_buf->vf;
			if (vf->type & VIDTYPE_COMPRESS) {
				video_keeper_keep_mem(vf->mem_head_handle, MEM_TYPE_CODEC_MM,
					&keep_head_id);
				if (keep_head_id > 0) {
					dp_print(dev->index, PRINT_OTHER,
						"keep ok id=%d, mem_head_handle=%px\n",
						keep_head_id, vf->mem_head_handle);
					dpss_out_buf->private_data->keep_head_id =
						keep_head_id;
					dpss_out_buf->private_data->is_keep = true;
				} else {
					dp_print(dev->index, PRINT_ERROR,
						"keep fail id=%d\n", keep_head_id);
				}

				video_keeper_keep_mem(vf->mem_handle_1, MEM_TYPE_CODEC_MM,
					&keep_table_id);
				if (keep_table_id > 0) {
					dp_print(dev->index, PRINT_OTHER,
						"keep ok id=%d, mem_table_handle=%px\n",
						keep_table_id, vf->mem_handle_1);
					dpss_out_buf->private_data->keep_id_1 =
						keep_table_id;
					dpss_out_buf->private_data->is_keep = true;
				} else {
					dp_print(dev->index, PRINT_ERROR,
						"keep fail id=%d\n", keep_table_id);
				}

				video_keeper_keep_mem(vf->mem_dw_handle, MEM_TYPE_CODEC_MM,
					&keep_dw_id);
				if (keep_dw_id > 0) {
					dp_print(dev->index, PRINT_OTHER,
						"keep ok id=%d, mem_dw_handle=%px\n",
						keep_dw_id, vf->mem_dw_handle);
					dpss_out_buf->private_data->keep_dw_id = keep_dw_id;
					dpss_out_buf->private_data->is_keep = true;
				} else {
					dp_print(dev->index, PRINT_ERROR,
						"keep fail id=%d\n", keep_dw_id);
				}
			}

			video_keeper_keep_mem(vf->mem_handle, MEM_TYPE_CODEC_MM, &keep_id);
			if (keep_id > 0) {
				dp_print(dev->index, PRINT_OTHER,
					"keep ok id=%d, mem_handle=%px\n",
					keep_id, vf->mem_handle);
				dpss_out_buf->private_data->keep_id = keep_id;
				dpss_out_buf->private_data->is_keep = true;
			} else {
				dp_print(dev->index, PRINT_ERROR,
					"keep fail id=%d\n", keep_id);
			}
		}
	}

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		dev->dpss_out_buf[i].index = i;
		dev->dpss_out_buf[i].vf = NULL;
		atomic_set(&dev->dpss_out_buf[i].on_use, false);
	}
	mutex_unlock(&dev->mutex_dpss_out);
}

static void dpss_out_free_q_init(struct dpss_process_dev *dev)
{
	INIT_KFIFO(dev->dpss_out_free_q);
	kfifo_reset(&dev->dpss_out_free_q);
}

static void dpss_out_free_q_uninit(struct dpss_process_dev *dev)
{
	struct vframe_s *vf;

	dp_print(dev->index, PRINT_OTHER, "unit out_free_q len=%d\n",
		 kfifo_len(&dev->dpss_out_free_q));
	while (kfifo_len(&dev->dpss_out_free_q) > 0) {
		if (kfifo_get(&dev->dpss_out_free_q, &vf))
			dp_print(dev->index, PRINT_OTHER, "%s: vf: %px\n", __func__, vf);
	}
}

static void dpss_lcevc_buf_uninit(struct dpss_process_dev *dev)
{
	int i;

	for (i = 0; i < LCEVC_POOL_SIZE; i++) {
		atomic_set(&dev->lcevc_buf[i].on_use, false);
		dev->lcevc_buf[i].dec_vf = NULL;
		memset(&dev->lcevc_buf[i].lcevc_vf, 0, sizeof(dev->lcevc_buf[i].lcevc_vf));
	}
}

static void dpss_rotate_q_init(struct dpss_process_dev *dev)
{
	INIT_KFIFO(dev->dpss_rotate_q);
	kfifo_reset(&dev->dpss_rotate_q);
}

static int find_standard_duration(struct dpss_process_dev *dev, int duration_val)
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

	dp_print(dev->index, PRINT_OTHER, "The nearest duration is %d.\n", duration_arr[num]);
	return duration_arr[num];
}

static int get_output_duration(struct dpss_process_dev *dev)
{
	struct vinfo_s *vinfo;
	u64 output_fps;
	int ret = 1600;

	vinfo = get_current_vinfo();
	if (IS_ERR_OR_NULL(vinfo)) {
		dp_print(dev->index, PRINT_ERROR, "%s: get display vinfo err!!\n", __func__);
		return ret;
	}

	output_fps = div64_u64(vinfo->sync_duration_num, vinfo->sync_duration_den);
	if (output_fps > 0 && output_fps < 24)
		ret = 4004;
	else if (output_fps == 24)
		ret = 4000;
	else if (output_fps == 25)
		ret = 3840;
	else if (output_fps > 25 && output_fps < 30)
		ret = 3203;
	else if (output_fps == 30)
		ret = 3200;
	else if (output_fps == 50)
		ret = 1920;
	else if (output_fps > 50 && output_fps < 60)
		ret = 1601;
	else if (output_fps == 60)
		ret = 1600;
	else if (output_fps == 100)
		ret = 960;
	else if (output_fps > 100 && output_fps < 120)
		ret = 801;
	else if (output_fps == 120)
		ret = 800;
	else if (output_fps == 144)
		ret = 667;
	else if (output_fps == 165)
		ret = 582;
	else if (output_fps > 165)
		ret = (int)div64_u64(96000ULL, output_fps);
	else
		ret = 1600;

	dp_print(dev->index, PRINT_OTHER, "%s: output:fps %lld, duration %d.\n",
		__func__,
		output_fps,
		ret);

	return ret;
}

static int dpss_config_work_mode(int dev_index)
{
	u32 dps_work_mode;

	if (dev_index == 0) {
		if (work_mode_ctl)
			dps_work_mode = work_mode_ctl;
		else
			dps_work_mode = DPSS_WORK_MODE_NR |
				DPSS_WORK_MODE_DDD |
				DPSS_WORK_MODE_LC_EVC |
				DPSS_WORK_MODE_DCT |
				DPSS_WORK_MODE_FRC |
				DPSS_WORK_MODE_HDR |
				DPSS_WORK_MODE_DI |
				DPSS_WORK_MODE_MAIN;
		is_main_channel_enabled = true;
	} else {
		if (work_mode_ctl_pip)
			dps_work_mode = work_mode_ctl_pip;
		else
			dps_work_mode = DPSS_WORK_MODE_NR |
				DPSS_WORK_MODE_DCT |
				DPSS_WORK_MODE_DI;

		is_dual_channel_enabled = true;
	}

	return dps_work_mode;
}

static void dpss_direct_mode_switch(struct dpss_process_dev *dev, struct vframe_s *vf)
{
	struct dpss_cmd_a_s dpss_para;
	bool is_interlace = false;
	bool is_high_fps = false;
	bool is_high_bandwidth = false;
	bool is_4k = false;
	bool need_enable_direct_mode = false;
	u32 duration = 0;
	u32 src_w, src_h;

	if (!dev || !vf) {
		pr_err("dp:[0]:%s:NULL param.\n", __func__);
		return;
	}

	if (dev->index != 0) {
		dp_print(dev->index, PRINT_OTHER, "%s: only main support direct mode.\n", __func__);
		return;
	}

	if (vf->type & VIDTYPE_INTERLACE_BOTTOM)
		is_interlace = true;

	if (vf->flag & VFRAME_FLAG_HIGH_BANDWIDTH)
		is_high_bandwidth = true;

	duration = find_standard_duration(dev, vf->duration);
	if (duration >= 667 && duration < 1600)
		is_high_fps = true;

	src_w = (vf->type & VIDTYPE_COMPRESS) ? vf->compWidth : vf->width;
	src_h = (vf->type & VIDTYPE_COMPRESS) ? vf->compHeight : vf->height;
	is_4k = (src_w > 2560 || src_h > 1440);

	need_enable_direct_mode = (is_4k && is_high_fps) ||
		is_high_bandwidth ||
		is_dual_channel_enabled ||
		temperature_control_en ||
		is_dec_set_direct_mode;

	if (force_direct_mode == DIRECT_MODE_FORCED_ENABLE)
		direct_mode_flag = 1;
	else if (force_direct_mode == DIRECT_MODE_FORCED_DISABLE)
		direct_mode_flag = 0;
	else
		direct_mode_flag = (need_enable_direct_mode && !is_interlace) ? 1 : 0;

	memset(&dpss_para, 0, sizeof(struct dpss_cmd_a_s));
	if (dev->direct_mode_en == 0 && direct_mode_flag == 1) {
		dp_print(dev->index, PRINT_OTHER, "%s:enable direct mode.\n", __func__);
		dpss_para.para |= DPSS_DIRECT_MODE;
		dpss_cmd_asy(dev->dpss_index, DPSS_CMD_ASY_2_WORKMODE, &dpss_para);
		dev->direct_mode_en = 1;
	} else if (dev->direct_mode_en == 1 && direct_mode_flag == 0) {
		dp_print(dev->index, PRINT_OTHER, "%s: disable direct mode.\n", __func__);
		dpss_para.para &= ~DPSS_DIRECT_MODE;
		dev->direct_mode_en = 0;
		dpss_cmd_asy(dev->dpss_index, DPSS_CMD_ASY_2_WORKMODE, &dpss_para);
	} else {
		dp_print(dev->index, PRINT_OTHER, "%s: keep direct mode status :%d.\n",
			__func__, direct_mode_flag);
	}
}

static void dpss_frc_enable_switch(struct dpss_process_dev *dev, int enable)
{
	struct dpss_cmd_a_s dpss_para;

	if (!dev) {
		pr_err("dp:[0]:%s:NULL param.\n", __func__);
		return;
	}

	if (dev->index != 0) {
		dp_print(dev->index, PRINT_OTHER, "%s: only main support frc.\n", __func__);
		return;
	}

	memset(&dpss_para, 0, sizeof(struct dpss_cmd_a_s));
	if (dev->frc_en == 0 && enable == 1) {
		dp_print(dev->index, PRINT_OTHER, "%s:enable frc.\n", __func__);
		dpss_para.para = 1;
		dpss_cmd_asy(dev->dpss_index, DPSS_CMD_ENABLE_FRC, &dpss_para);
		dev->frc_en = 1;
	} else if (dev->frc_en == 1 && enable == 0) {
		dp_print(dev->index, PRINT_OTHER, "%s: disable frc.\n", __func__);
		dpss_para.para = 0;
		dev->frc_en = 0;
		dpss_cmd_asy(dev->dpss_index, DPSS_CMD_ENABLE_FRC, &dpss_para);
	} else {
		dp_print(dev->index, PRINT_OTHER, "%s: keep frc status :%d.\n", __func__, enable);
	}
}

static int update_display_speed_param(int sync_ins_id, int speed, bool is_exit)
{
	int i = 0;

	mutex_lock(&play_speed_mutex);
	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		if (display_speed[i].sync_id == sync_ins_id)
			break;
	}

	dp_print(0, PRINT_OTHER, "update sync id(%d) speed to %d, i=%d.\n", sync_ins_id, speed, i);

	if (i == DPSSPR_POOL_SIZE) {
		dp_print(0, PRINT_OTHER, "update speed: invalid sync_id.\n");
		mutex_unlock(&play_speed_mutex);
		return -1;
	}

	if (speed != DISPLAY_SPEED_MASK)
		display_speed[i].speed = speed;

	if (is_exit)
		atomic_set(&display_speed[i].on_use, false);
	else
		atomic_set(&display_speed[i].on_use, true);
	mutex_unlock(&play_speed_mutex);

	return 0;
}

static void display_speed_param_init(void)
{
	int i = 0;

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		display_speed[i].sync_id = i;
		display_speed[i].speed = DISPLAY_SPEED_DEFAULT;
		atomic_set(&display_speed[i].on_use, false);
	}
}

static void config_play_speed_for_frc(struct dpss_process_dev *dev, struct vframe_s *vf)
{
	int i = 0;

	if (!dev || !vf)
		return;

	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		dp_print(dev->index, PRINT_OTHER, "config speed: display_speed[%d].syncid=%d.\n",
			i, display_speed[i].sync_id);
		if (vf->syncid == display_speed[i].sync_id)
			break;
	}

	if (i == DPSSPR_POOL_SIZE) {
		dp_print(dev->index, PRINT_OTHER, "config speed: invalid sync_id.\n");
		return;
	}

	if (display_speed[i].speed != DISPLAY_SPEED_DEFAULT && !dev->trick_play_en) {
		dp_print(dev->index, PRINT_OTHER, "enable trick_play.\n");
		dp_dynamic_detect_play_speed(true);
		dev->trick_play_en = true;
	} else if (display_speed[i].speed == DISPLAY_SPEED_DEFAULT && dev->trick_play_en) {
		dp_print(dev->index, PRINT_OTHER, "disable trick_play.\n");
		dp_dynamic_detect_play_speed(false);
		dev->trick_play_en = false;
	} else {
		dp_print(dev->index, PRINT_OTHER, "no need update speed.\n");
	}
}

int set_display_speed(int sync_ins_id, int speed)
{
	dp_print(0, PRINT_OTHER, "set syncid(%d) speed to %d.\n", sync_ins_id, speed);
	return update_display_speed_param(sync_ins_id, speed, false);
}
EXPORT_SYMBOL(set_display_speed);

static void connect_to_dpss(struct dpss_process_dev *dev, struct vframe_s *vf, int rotate)
{
	int max_width = 0, max_height = 0;

	if (!dev || !vf) {
		pr_err("%s: NULL param.\n", __func__);
		return;
	}

	dev->transform = 0;
	dev->dpss_is_tvp = false;
	dev->cur_is_i = false;
	dev->cur_is_frcgame = false;

	max_width = vf->compWidth >= vf->width ? vf->compWidth : vf->width;
	max_height = vf->compHeight >= vf->height ? vf->compHeight : vf->height;

	if (is_src_crop_valid(vf->src_crop) && (vf->type & VIDTYPE_COMPRESS)) {
		dp_print(dev->index, PRINT_OTHER, "%s: vf has src crop.\n", __func__);
		max_height -= vf->src_crop.bottom;
	}

	memset(&dev->dpss_parm, 0, sizeof(dev->dpss_parm));

	if (vf->type & VIDTYPE_INTERLACE_BOTTOM) {
		dev->dpss_parm.di_parm.is_interlace = 1;
		dev->cur_is_i = true;
	} else {
		dev->dpss_parm.di_parm.is_interlace = 0;
	}

	dev->dpss_parm.dps_work_mode = dpss_config_work_mode(dev->index);
	if (check_need_do_game_frc(&dev->dpss_parm, vf)) {
		dev->is_start_with_dpss = false;
		dev->dpss_parm.dps_work_mode = DPSS_WORK_MODE_FRC;
		dev->cur_is_frcgame = true;
	}
	dev->dpss_parm.di_parm.buffer_mode = DPSS_BUFFER_MODE_ALLOC_SELF;
	dev->dpss_parm.di_parm.output_format = DPSS_OUTPUT_BY_DI_DEFINE;
	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE) {
		dp_print(dev->index, PRINT_OTHER, "%s: secure vf.\n", __func__);
		dev->dpss_parm.di_parm.output_format |= DPSS_OUTPUT_TVP;
		dev->dpss_is_tvp = true;
	}
	dev->dpss_parm.di_parm.width = max_width;
	dev->dpss_parm.di_parm.height = max_height;
	if (rotate == 3) {
		dev->transform = 3;
		dp_print(dev->index, PRINT_OTHER, "%s: need rotate 180.\n", __func__);
		dev->dpss_parm.di_parm.rotate_flag = VFRAME_FLAG_MIRROR_H | VFRAME_FLAG_MIRROR_V;
	}
	dev->dpss_parm.di_parm.duration = vf->duration;
	dev->dpss_parm.ops.arg = dev;
	dev->dpss_parm.ops.empty_input_done = dp_empty_input_done;
	dev->dpss_parm.ops.fill_output_done = dp_fill_output_done;
	dev->dpss_parm.ops.get_input_vf_info = get_input_vf_info;

	dev->dpss_index = dpss_create_instance(&dev->dpss_parm);
	if (dev->dpss_index < 0) {
		dp_print(dev->index, PRINT_ERROR,
			"creat dpss fail, dpss_index=%d\n",
			dev->dpss_index);
		return;
	}

	dp_print(dev->index, PRINT_OTHER,
		"%s: dpss_index: %d, work_mode: 0x%x.\n",
		__func__,
		dev->dpss_index,
		dev->dpss_parm.dps_work_mode);
}

static int dpss_process_init(struct dpss_process_dev *dev)
{
	if (!dev)
		return 0;

	dp_print(dev->index, PRINT_OTHER, "%s: dev index = %d.\n", __func__, dev->index);

	dev->dpss_index = -1;
	dev->dpss_is_tvp = false;
	dev->inited = true;
	dev->fput_count = 0;
	dev->fget_count = 0;
	dev->empty_count = 0;
	dev->fill_count = 0;
	dev->empty_done_count = 0;
	dev->fill_done_count = 0;
	dev->fence_creat_count = 0;
	dev->fence_release_count = 0;

	dev->last_dec_type = DEC_TYPE_MAX;
	dev->last_instance_id = 0xFFFFFFFF;
	dev->last_buf_mgr_reset_id = 0xFFFFFFFF;
	dev->last_frame_index = 0xFFFFFFFF;
	dev->last_vf.type = 0;
	dev->last_vf.compWidth = 0;
	dev->last_vf.compHeight = 0;
	dev->dpss_module_bypass = false;
	dev->first_out = false;
	dev->q_dummy_frame_done = false;
	dev->last_frame_bypass = false;
	dev->cur_is_i = false;
	dev->last_buf_mgr = NULL;
	dev->cur_is_frcgame = false;
	dev->last_file = NULL;
	dev->direct_mode_en = 0;
	dev->frc_en = 0;
	dev->is_start_with_dpss = true;
	if (dev->index == 0)
		dev->need_check_hdr_state = hdr_state_flag;
	dev->last_frame_vd1_toggle = false;
	dev->continue_to_vd1_num = 0;
	dev->should_on_vd1 = 0;
	dev->is_first_frame = false;
	dev->is_switch_first_frame = false;
	dev->sync_id = 0;
	dev->trick_play_en = false;
	atomic_set(&input_eos_enabled, 0);
	dev->rotate_en = false;
	dev->ge2d_para.context = NULL;
	dev->ge2d_para.count = 0;
	dev->ge2d_para.canvas_dst[0] = -1;
	dev->ge2d_para.canvas_dst[1] = -1;
	dev->ge2d_para.canvas_dst[2] = -1;
	dev->ge2d_para.canvas_scr[0] = -1;
	dev->ge2d_para.canvas_scr[1] = -1;
	dev->ge2d_para.canvas_scr[2] = -1;
	dev->ge2d_para.plane_num = 2;
	dev->rotate_buf_status = UNINITIAL;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	amdv_mem_alloc_control(module_dpss, 1);
#endif
	receive_q_init(dev);
	dpss_input_free_q_init(dev);
	file_q_init(dev);
	dpss_out_q_init(dev);
	dpss_out_free_q_init(dev);
	dpss_rotate_q_init(dev);

	return 0;
}

static int dpss_process_uninit(struct dpss_process_dev *dev)
{
	int ret = 0;
	int i = 0;
	struct dpss_in_buf_t *buf;

	if (!dev)
		return 0;

	dp_print(dev->index, PRINT_OTHER, "%s: index = %d.\n", __func__, dev->index);
	dev->inited = false;

	dpss_out_q_uninit(dev);

	if (is_dual_channel_enabled) {
		dp_print(dev->index, PRINT_OTHER, "exit need change direct mode to 0.\n");
		is_dual_channel_enabled = 0;
	}

	if (dev->index == 0) {
		dp_print(dev->index, PRINT_OTHER, "exit main channel.\n");
		is_main_channel_enabled = 0;
	}

	if (dev->dpss_index >= 0) {
		ret = dpss_destroy_instance(dev->dpss_index);
		if (ret != 0)
			dp_print(dev->index, PRINT_ERROR,
				  "destroy dpss fail, dpss_index=%d\n",
				  dev->dpss_index);
		dev->dpss_index = -1;
	}
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	amdv_mem_alloc_control(module_dpss, 0);
#endif

	receive_q_uninit(dev);
	file_q_uninit(dev);
	dpss_out_free_q_uninit(dev);
	dpss_lcevc_buf_uninit(dev);
	if (dev->rotate_en)
		uninit_rotate_buffer(dev);
	/*input buffer in dpss side need fput file*/
	for (i = 0; i < DPSSPR_POOL_SIZE; i++) {
		buf = &dev->dpss_in_buf[i];
		if (buf->pp_info.queued) {
			dp_print(dev->index, PRINT_OTHER,
				  "%s frame_index=%d\n", __func__, buf->vf.frame_index);
			if (!buf->pp_info.dropped) {
				ret = di_processed_checkin(buf->pp_info.src_file);
				if (ret != 0)
					dp_print(dev->index, PRINT_ERROR,
						" uninit not empty done buf failed.\n");
				dp_put_file(dev, buf->pp_info.src_file);
			}

			if (!kfifo_put(&dev->dpss_input_free_q, buf))
				dp_print(dev->index, PRINT_ERROR,
				"%s: put dpss_input_free_q fail\n", __func__);

			buf->pp_info.queued = false;
		}
	}

	if (dev->fget_count != dev->fput_count)
		dp_print(dev->index, PRINT_OTHER,
			  "file leak!!!, fget_count=%lld, fput_count=%lld\n",
			  dev->fget_count, dev->fput_count);

	if (dev->fence_creat_count != dev->fence_release_count) {
		dp_print(dev->index, PRINT_OTHER,
			"fence_creat_count =%lld, fence_release_count=%lld\n",
			dev->fence_creat_count, dev->fence_release_count);
		dp_timeline_increase(dev, dev->fence_creat_count
				- dev->fence_release_count);
	}

	if (dev->index == 0)
		hdr_state_flag = dev->need_check_hdr_state;

	dp_print(dev->index, PRINT_OTHER,
		"%s: fill_done/fill: cur: %lld/%lld, total: %d/%d\n",
		__func__,
		dev->fill_done_count,
		dev->fill_count,
		total_fill_done_count,
		total_fill_count);

	return ret;
}

static bool is_dd_vframe(struct dpss_process_dev *dev, struct vframe_s *vf)
{
	bool ret = false;

	if (IS_ERR_OR_NULL(vf)) {
		dp_print(dev->index, PRINT_ERROR, "%s: NULL param.\n", __func__);
		return -1;
	}

	if (!is_amdv_enable())
		return false;

	if (vf->signal_type & (1 << 30) || is_amdv_frame(vf))
		ret = true;

	return ret;
}

static bool check_need_do_dpss(struct dpss_process_dev *dev, struct vframe_s *vf,
	int transform)
{
	bool need_do_dpss = true;
	bool is_qms_case = false;
	bool need_dpss_rotate = false;
	char do_dpss_type = 0;
	int used_vf_with = 0, used_vf_height = 0;
	int inp_std_duration;

	if (!dev || !vf) {
		pr_err("%s: param is invalid.\n", __func__);
		return need_do_dpss;
	}

	/*all the check flow need done beforce this debug node control*/
	/*force bypass dpss*/
	if (dpss_bypass == 1) {
		dp_print(dev->index, PRINT_OTHER, "force bypass dpss.\n");
		return false;
	} else if (dpss_bypass == 0) {
		dp_print(dev->index, PRINT_OTHER, "force do dpss.\n");
		return true;
	}

	inp_std_duration = find_standard_duration(dev, vf->duration);
	do_dpss_type = dpss_is_bypass(&dev->dpss_parm, vf);
	if (do_dpss_type) {
		dp_print(dev->index, PRINT_OTHER,
			"need bypass due to dpss, reason=%d.\n", do_dpss_type);
		return false;
	}

	if (vf->type & VIDTYPE_COMPRESS) {
		used_vf_with = vf->compWidth;
		used_vf_height = vf->compHeight;
		if (is_src_crop_valid(vf->src_crop)) {
			dp_print(dev->index, PRINT_OTHER, "need check src_crop.\n");
			used_vf_height -= vf->src_crop.bottom;
			used_vf_with -= vf->src_crop.right;
		}
	} else {
		used_vf_with = vf->width;
		used_vf_height = vf->height;
	}

	/*rotate frame*/
	if (transform == 4 || transform == 7) {
		if (!dpss_rotate_en) {
			need_do_dpss = false;
		} else {
			need_dpss_rotate = true;
		}
	}

	/*4090*2160 no need do dpss*/
	if (!need_dpss_rotate && used_vf_with > 3840) {
		dp_print(dev->index, PRINT_OTHER, "over 3840, no need do dpss.\n");
		need_do_dpss = false;
	}

	/*over 144fps no need do dps*/
	if (inp_std_duration < 667) {
		dp_print(dev->index, PRINT_OTHER, "over 144fps, no need do dpss.\n");
		need_do_dpss = false;
	}

	/*pip check*/
	if (dev->index > 0) {
		if (!(vf->type & VIDTYPE_INTERLACE)) {
			dp_print(dev->index, PRINT_OTHER, "pip dpss only support I source.\n");
			need_do_dpss = false;
		}
	}

	/*height > 2160 no need do dpss*/
	if (!need_dpss_rotate && used_vf_height > 2160) {
		dp_print(dev->index, PRINT_OTHER, "height > 2160, no need do dpss.\n");
		need_do_dpss = false;
	}

	is_qms_case = vf->vf_vrr_param.qms_en || vf->vf_vrr_param.qms_plus_en;
	if (vf->duration < 1600 && inp_std_duration < dev->output_duration && !is_qms_case) {
		/*input fps more than output fps*/
		dp_print(dev->index, PRINT_OTHER,
			"input fps more than output, no need do dpss.\n");
		need_do_dpss = false;
	}

	/*dd input*/
	if (is_dd_vframe(dev, vf) && used_vf_with > 1920 && !IS_ALIGNED(used_vf_with, 16)) {
		dp_print(dev->index, PRINT_OTHER, "special dd frame bypass dpss.\n");
		need_do_dpss = false;
	}

	/*dpss switch vd1, vd1 need toggle at least 10 frames, then allow switch to dpss*/
	if (dev->should_on_vd1 && dev->continue_to_vd1_num < CONTINUE_VD1_TOGGLE_NUM) {
		dp_print(dev->index, PRINT_OTHER, "vd1 toggle less 10 frames,continue bypass.\n");
		need_do_dpss = false;
	}

	/*JPEG image*/
	if (vf->codec_vfmt == VFORMAT_JPEG) {
		dp_print(dev->index, PRINT_OTHER, "JPEG image, no need so dpss.\n");
		need_do_dpss = false;
	}

	return need_do_dpss;
}

#ifdef CONFIG_AMLOGIC_DPSS_THERMAL
int set_dpss_cooling_state(int enable)
{
	if (temperature_control_en != enable)
		temperature_control_en =  enable;

	pr_info("dp:[0]: set temperature_control_en to %d.\n", enable);

	return 0;
}

void register_dpss_cooling(struct device_node *np)
{
	struct thermal_cooling_device *ret = NULL;
	struct dpss_cooling_device *dpss_cdev;

	dpss_cdev = kzalloc(sizeof(*dpss_cdev), GFP_KERNEL);
	if  (!dpss_cdev)
		return;

	dpss_cdev->maxstep = 1;
	dpss_cdev->set_dpss_cooling_state = set_dpss_cooling_state;
	ret = dpss_cooling_register(dpss_cdev, np);
	if (!ret) {
		pr_err("failed to allocate major number\n");
		goto fail_dpss_cooling_register;
	}
	dpss_cdev->cool_dev = ret;
	dpss_cdev_global = dpss_cdev;
	return;

fail_dpss_cooling_register:
	kfree(dpss_cdev);
	dpss_cdev = NULL;
}

void unregister_dpss_cooling(void)
{
	struct dpss_cooling_device *dpss_cdev;

	if  (!dpss_cdev_global) {
		pr_err("NULL dev, no need unreg.\n");
		return;
	}
	dpss_cdev = dpss_cdev_global;
	dpss_cdev_global = NULL;

	if (dpss_cdev->cool_dev)
		dpss_cooling_unregister(dpss_cdev->cool_dev);
	kfree(dpss_cdev);
}
#endif

void buf_mgr_set_eos(int print_flag, struct vf_ref_t *vf_ref)
{
	struct vframe_s *vf = NULL;

	if (!vf_ref) {
		pr_err("buf_mgr: vf_ref is NULL.\n");
		return;
	}

	vf = &vf_ref->vf;

	if (is_dual_channel_enabled == 0) {
		atomic_set(&input_eos_enabled, 1);
		last_instance_frame_index =  vf->frame_index;
		if (print_flag & 0x1)
			pr_info("frame_index=%d , eos flag set!\n", vf->frame_index);
	}
}

static int display_original_frame(struct dpss_process_dev *dev,
	struct frame_info_t *frame_info, struct file *file_vf, struct vframe_s *vf)
{
	if (!dev || !frame_info) {
		pr_err("%s: param is invalid.\n", __func__);
		return -EINVAL;
	}

	frame_info->out_fd = -1;
	frame_info->out_fence_fd = -1;
	frame_info->is_i = vf->type & VIDTYPE_INTERLACE;
	frame_info->frame_index = vf->frame_index;
	frame_info->need_bypass = true;
	dev->last_file = file_vf;
	dev->last_frame_bypass = true;
	dp_put_file(dev, file_vf);

	return 1;
}

static enum vframe_signal_fmt_e get_src_signal_format(struct vframe_s *vf)
{
	enum vframe_signal_fmt_e signal_fmt = VFRAME_SIGNAL_FMT_INVALID;

	if (!vf) {
		pr_err("vf param is invalid.\n");
		return -EINVAL;
	}

	signal_fmt = vf->src_fmt.fmt;

	if (vf->src_fmt.fmt == VFRAME_SIGNAL_FMT_INVALID) {
		if (signal_transfer_characteristic == 18 &&
			signal_color_primaries == 9) {
			if (signal_cuva)
				signal_fmt = VFRAME_SIGNAL_FMT_CUVA_HLG;
			else
				signal_fmt = VFRAME_SIGNAL_FMT_HLG;
		} else if ((signal_transfer_characteristic == 0x30) &&
				 (signal_color_primaries == 9)) {
			signal_fmt = VFRAME_SIGNAL_FMT_HDR10;
		} else if ((signal_transfer_characteristic == 16) &&
				 ((signal_color_primaries == 9) ||
				  (signal_color_primaries == 2))) {
			if (signal_cuva)
				signal_fmt = VFRAME_SIGNAL_FMT_CUVA_HDR;
			else
				signal_fmt = VFRAME_SIGNAL_FMT_HDR10;
		} else if (signal_transfer_characteristic == 14 ||
			signal_transfer_characteristic == 1) {
			if (signal_color_primaries == 9)
				signal_fmt = VFRAME_SIGNAL_FMT_SDR_2020;
			else
				signal_fmt = VFRAME_SIGNAL_FMT_SDR;
		} else {
			signal_fmt = VFRAME_SIGNAL_FMT_SDR;
		}
	}
	return signal_fmt;
}

static int dpss_process_set_frame(struct dpss_process_dev *dev, struct frame_info_t *frame_info)
{
	int i;
	struct file *file_vf = NULL;
	struct vframe_s *vf;
	struct dma_buf *dmabuf;
	int out_fd;
	int out_fence_fd;
	struct file_private_data *private_data = NULL;
	u32 is_repeat = false;
	u32 frame_index = 0;
	u32 max_width_new = 0, max_width_last = 0;
	bool ip_switch = false, tvp_switch = false, rotate180_switch = false, game_switch = false;
	bool need_do_dummy = false;
	bool vf_from_vdin = false;
	struct dp_buf_mgr_t *cur_buf_mgr;
	struct dpss_status dpss_state;
	bool is_dtv_src = false;
	bool is_pp_swtch = false;
	int ret = 0;

	if (!dev || !frame_info) {
		pr_err("%s: param is invalid.\n", __func__);
		return -EINVAL;
	}

	if (!dev->inited) {
		dp_print(dev->index, PRINT_ERROR,
			 "set_frame but not inited\n");
		return -EINVAL;
	}

	frame_info->need_bypass = false;
	file_vf = dp_get_file(dev, frame_info->in_fd);
	if (!file_vf) {
		dp_print(dev->index, PRINT_ERROR,
			 "%s: fd is invalid!!!\n", __func__);
		return -EINVAL;
	}

	/*need add: check file has vf, if not return bypass*/
	vf = get_vf_from_file(dev, file_vf);
	if (!vf) {
		dp_print(dev->index, PRINT_OTHER, "fd no vf, bypass\n");
		dp_put_file(dev, file_vf);
		return -EINVAL;
	}

	if (vf->flag_ext & VFRAME_FLAG_EXT_DTV_CASE)
		is_dtv_src = true;
	vf->dpss_flg |= DPSS_FLG_SET_DPSS_PROCESS;
	dp_print(dev->index, PRINT_OTHER,
		"%s:len=%d,fd=%d,frame_index=%d,file=%px,file_count=%ld,transform=%d vf_ext:%d.\n",
		__func__,
		kfifo_len(&dev->receive_q),
		frame_info->in_fd,
		vf->frame_index,
		file_vf,
		file_count(file_vf),
		frame_info->transform,
		vf->flag_ext);

	dp_print(dev->index, PRINT_OTHER,
		"frame_index=%d,type=0x%x,flag=0x%x,compWidth*Height=%dX%d,width*height=%dX%d,duration=%d.\n",
		vf->frame_index,
		vf->type,
		vf->flag,
		vf->compWidth,
		vf->compHeight,
		vf->width,
		vf->height,
		vf->duration);

	if (dev->dpss_index == -1) {
		if (dev->index == 0 || (dev->index == 1 && (vf->type & VIDTYPE_INTERLACE))) {
			connect_to_dpss(dev, vf, frame_info->transform);
			if (dev->dpss_index == -1) {
				dp_print(dev->index, PRINT_ERROR, "connect to dpss fail.\n");
				dp_put_file(dev, file_vf);
				return -EINVAL;
			}
		}
		if (vf->type & VIDTYPE_INTERLACE)
			dev->cur_is_i = true;
		else
			dev->cur_is_i = false;
	}

	if (vf->source_type == VFRAME_SOURCE_TYPE_CVBS &&
		vf->duration < 1600 && (vf->type & VIDTYPE_INTERLACE)) {
		dp_print(dev->index, PRINT_OTHER,
			"AV case, but duration=%d more than 1600, modify.\n", vf->duration);
		vf->duration = 1600;
	}

	cur_buf_mgr = get_buf_mgr(file_vf);
	if (!dev->last_buf_mgr) {
		dev->last_buf_mgr = cur_buf_mgr;
		dev->is_first_frame = true;
		atomic_set(&input_eos_enabled, 0);
		dp_print(dev->index, PRINT_OTHER, "new stream is on, first frame is set\n");
	} else if (cur_buf_mgr != dev->last_buf_mgr) {
		dev->last_buf_mgr = cur_buf_mgr;
		dev->is_switch_first_frame = true;
		atomic_set(&input_eos_enabled, 0);
		dp_print(dev->index, PRINT_OTHER, "stream changed, need notify processor\n");
		ret = 3;
	}

	if (dev->index == 0) {
		if (temperature_control_en)
			dpss_frc_enable_switch(dev, 0);
		else
			dpss_frc_enable_switch(dev, 1);

		dpss_direct_mode_switch(dev, vf);

		if (dev->sync_id != vf->syncid) {
			dp_print(dev->index, PRINT_OTHER, "reset syncid(%d) speed.\n", vf->syncid);
			update_display_speed_param(dev->sync_id, DISPLAY_SPEED_DEFAULT, true);
			update_display_speed_param(vf->syncid, DISPLAY_SPEED_MASK, false);
			dev->sync_id = vf->syncid;
		} else {
			update_display_speed_param(dev->sync_id, DISPLAY_SPEED_MASK, false);
		}

		config_play_speed_for_frc(dev, vf);
	}

	dev->output_duration = get_output_duration(dev);

	if (!check_need_do_dpss(dev, vf, frame_info->transform) || force_num_to_vd1) {
		dev->is_start_with_dpss = false;
		dev->should_on_vd1 = 1;
		if (force_num_to_vd1 > 0)
			force_num_to_vd1--;

		if (!dev->last_frame_bypass && dev->last_vf.type) {
			dp_print(dev->index, PRINT_OTHER, "no bypass to bypass.\n");
			dev->last_vf.type = 0;
			dp_put_file(dev, file_vf);
			return 1;
		}

		dev->continue_to_vd1_num++;

		if (dev->need_check_hdr_state) {
			vf->dpss_flg |= DPSS_FLG_MODULE_BYPASS;
			dev->need_check_hdr_state = false;
			dp_print(dev->index, PRINT_OTHER,
				"dv/hdr core switch dpss not ready, next to vd1 frame_index:%d.\n",
				vf->frame_index);
		}

		display_original_frame(dev, frame_info, file_vf, vf);
		if (vf->type & VIDTYPE_INTERLACE)
			dev->cur_is_i = true;
		else
			dev->cur_is_i = false;
		dev->last_vf = *vf;
		dev->last_frame_vd1_toggle = true;

		return ret;
	}
	dev->should_on_vd1 = 0;
	dev->continue_to_vd1_num = 0;

	if (ret == 3 && !(dev->last_vf.type & VIDTYPE_TYPEMASK) && !(vf->type & VIDTYPE_TYPEMASK)) {
		is_pp_swtch = true;
		dp_print(dev->index, PRINT_OTHER, "P-->P switch\n");
	}

	/*vf need check switch*/
	if ((vf->flag & VFRAME_FLAG_VIDEO_SECURE) && !dev->dpss_is_tvp) {
		dp_print(dev->index, PRINT_ERROR, "need up layer reinit to tvp.\n");
		tvp_switch = true;
	} else if (!(vf->flag & VFRAME_FLAG_VIDEO_SECURE) && dev->dpss_is_tvp) {
		dp_print(dev->index, PRINT_ERROR, "need up layer reinit to non-tvp.\n");
		tvp_switch = true;
	} else {
		tvp_switch = false;
	}

	/*need check I/P switch*/
	if ((vf->type & VIDTYPE_INTERLACE) && !dev->cur_is_i) {
		dp_print(dev->index, PRINT_ERROR, "need up layer reinit to I.\n");
		ip_switch = true;
	} else if (!(vf->type & VIDTYPE_INTERLACE) && dev->cur_is_i) {
		dp_print(dev->index, PRINT_ERROR, "need up layer reinit to P.\n");
		ip_switch = true;
	} else {
		ip_switch = false;
	}

	/*need check 180 rotate*/
	if ((frame_info->transform == 3 && dev->transform != 3) ||
		(frame_info->transform != 3 && dev->transform == 3)) {
		dp_print(dev->index, PRINT_OTHER, "rotate change, need up layer reinit.\n");
		rotate180_switch = true;
	}

	/*need check game mode switch*/
	if (check_need_do_game_frc(&dev->dpss_parm, vf) && !dev->cur_is_frcgame) {
		dp_print(dev->index, PRINT_ERROR, "need up layer reinit to game.\n");
		game_switch = true;
	} else if (!check_need_do_game_frc(&dev->dpss_parm, vf) && dev->cur_is_frcgame) {
		dp_print(dev->index, PRINT_ERROR, "need up layer reinit to other.\n");
		game_switch = true;
	} else {
		game_switch = false;
	}

	if (tvp_switch || ip_switch || rotate180_switch || game_switch) {
		dev->last_vf.type = 0;
		dp_put_file(dev, file_vf);
		return 1;
	}

	//start play and first send to dpss
	if (dev->index == 0 && dev->is_start_with_dpss) {
		dp_print(dev->index, PRINT_OTHER, "start play with dpss.\n");
		hdr_path_switch_to_dpss(3);
		dev->need_check_hdr_state = true;
		dev->is_start_with_dpss = false;
	}

	memset(&dpss_state, 0, sizeof(struct dpss_status));
	if (dpss_get_state(dev->dpss_index, DPSS_STATE_BUF, NULL, &dpss_state) != 0)
		dp_print(dev->index, PRINT_OTHER, "get buf state failed.\n");

	if (dpss_state.buf_status != DPSS_BUF_STATE_READY) {
		dp_print(dev->index, PRINT_OTHER, "dpss buf not ready.\n");
		if (is_dtv_src && (get_src_signal_format(vf) == VFRAME_SIGNAL_FMT_SDR))
			vf->dpss_flg |= DPSS_FLG_OUT_DONE | DPSS_FLG_FORCE_DPSS_DONE;
		else
			vf->type_ext |= VIDTYPE_EXT_DPSS_DROP;
		display_original_frame(dev, frame_info, file_vf, vf);
		return 0;
	}

	if (dev->dpss_parm.dps_work_mode == DPSS_WORK_MODE_FRC) {
		if (dpss_state.game_mode && !dpss_state.game_frc_locked) {
			dp_print(dev->index, PRINT_OTHER, "dpss game not ready.\n");
			vf->type_ext |= VIDTYPE_EXT_DPSS_DROP;
			display_original_frame(dev, frame_info, file_vf, vf);
			return 0;
		}
	}

	//VD1 switch to DPSS
	if (dev->index == 0 && dev->last_frame_vd1_toggle &&
		!check_need_do_game_frc(&dev->dpss_parm, vf)) {
		dp_print(dev->index, PRINT_OTHER, "vd1 switch to dpss\n");
		hdr_path_switch_to_dpss(1);
		dev->need_check_hdr_state = true;
		dev->last_frame_vd1_toggle = false;
	}

	if (dev->need_check_hdr_state && !hdr_path_delink_status()) {
		dp_print(dev->index, PRINT_ERROR, "need do dpss but hdr not ready, bypass.\n");
		if (is_dtv_src && (get_src_signal_format(vf) == VFRAME_SIGNAL_FMT_SDR))
			vf->dpss_flg |= DPSS_FLG_OUT_DONE | DPSS_FLG_FORCE_DPSS_DONE;
		else
			vf->type_ext |= VIDTYPE_EXT_DPSS_DROP;
		display_original_frame(dev, frame_info, file_vf, vf);
		return ret;
	}

	dev->need_check_hdr_state = false;

	if (vf->type & VIDTYPE_INTERLACE)
		dev->cur_is_i = true;
	else
		dev->cur_is_i = false;

	dev->transform = frame_info->transform;

	if (vf->flag & VFRAME_FLAG_VIDEO_SECURE)
		frame_info->is_tvp = true;
	else
		frame_info->is_tvp = false;

	if (vf->source_type == VFRAME_SOURCE_TYPE_HDMI ||
		vf->source_type == VFRAME_SOURCE_TYPE_CVBS ||
		vf->source_type == VFRAME_SOURCE_TYPE_TUNER)
		vf_from_vdin = true;

	if (atomic_read(&input_eos_enabled) == 1 && !vf_from_vdin) {
		if (last_instance_frame_index < vf->frame_index) {
			atomic_set(&input_eos_enabled, 0);
			dp_print(dev->index, PRINT_OTHER, "frame_index exceeded, eos canceled\n");
		} else {
			vf->type_ext |= VIDTYPE_EXT_DPSS_EOS;
			dp_print(dev->index, PRINT_OTHER, "frame_index=%d, input eos detected\n",
				vf->frame_index);
		}
	}

	if (check_need_do_game_frc(&dev->dpss_parm, vf))
		dev->cur_is_frcgame = true;
	else
		dev->cur_is_frcgame = false;

	dp_print(dev->index, PRINT_OTHER,
		"%s: frame_index=%d, magic_code=0x%x, ud_addr=%p, ud_len=%d.\n",
		__func__,
		vf->frame_index,
		vf->vf_ud_param.magic_code,
		vf->vf_ud_param.ud_param.pbuf_addr,
		vf->vf_ud_param.ud_param.buf_len);

	/*1080p->1080i; 4k->1080i*/
	max_width_new = vf->compWidth >= vf->width ? vf->compWidth : vf->width;
	max_width_last = dev->last_vf.compWidth >= dev->last_vf.width
		? dev->last_vf.compWidth : dev->last_vf.width;
	if (!(dev->last_vf.type & VIDTYPE_INTERLACE) && (vf->type & VIDTYPE_INTERLACE)) {
		dp_print(dev->index, PRINT_OTHER, "fmt change\n");
		need_do_dummy = true;
	}

	if (dev->first_out && need_do_dummy) {
		dev->first_out = false;
		dev->q_dummy_frame_done = false;
	}

	frame_index = vf->frame_index;
	dev->last_vf = *vf;

	if (dev->last_file == file_vf) {
		if (dev->last_frame_bypass) {
			dp_print(dev->index, PRINT_OTHER, "repeat:last bypass, continue bypass\n");
			display_original_frame(dev, frame_info, file_vf, vf);
			return 0;
		}

		dp_put_file(dev, file_vf);
		dmabuf = dev->last_dmabuf;
		dp_print(dev->index, PRINT_OTHER, "repeat frame\n");
		is_repeat = true;
		out_fence_fd = -1;
	} else {
		i = get_received_frame_free_index(dev);
		if (i < 0) {
			dp_print(dev->index, PRINT_ERROR, "%s: get free index fail.\n", __func__);
			dp_put_file(dev, file_vf);
			return -EINVAL;
		}

		if (!kfifo_get(&dev->file_free_q, &dmabuf)) {
			dp_print(dev->index, PRINT_ERROR, "peek free dma_buf fail!!!\n");
			dp_put_file(dev, file_vf);
			return -EINVAL;
		}

		private_data = dpss_proc_get_file_private_data(dmabuf->file, true);
		if (!private_data) {
			dp_print(dev->index, PRINT_ERROR, "%s: get private data fail.\n", __func__);
			dp_put_file(dev, file_vf);
			return -EINVAL;
		}

		if ((frame_info->transform == 4 || frame_info->transform == 7) &&
			(dev->rotate_buf_status == INIT_SUCCESS &&
			kfifo_len(&dev->dpss_rotate_q) == 0)) {
			dp_print(dev->index, PRINT_ERROR, "dpss_rotate_q is full.\n");
			vf->type_ext |= VIDTYPE_EXT_DPSS_DROP;
			kfifo_put(&dev->file_free_q, dmabuf);
			dp_put_file(dev, file_vf);
			return -1;
		}

		private_data->vf_p = NULL;
		private_data->vf.frame_index = vf->frame_index;
		private_data->vf.index_disp = vf->index_disp;
		private_data->file = file_vf;
		frame_index = vf->frame_index;
		private_data->vf_ext = *vf;
		private_data->vf_ext_p = vf;
		if (is_ud_param_valid(vf->vf_ud_param)) {
			dp_print(dev->index, PRINT_OTHER, "%s: has ud.\n", __func__);
			if (vf->vf_ud_param.ud_param.pbuf_addr &&
				vf->vf_ud_param.ud_param.buf_len > 0 &&
				vf->vf_ud_param.ud_param.buf_len <= VF_UD_MAX_SIZE) {
				dp_print(dev->index, PRINT_OTHER, "src_ud=%px, new_ud=%px\n",
					vf->vf_ud_param.ud_param.pbuf_addr,
					private_data->p_ud_param);
				memcpy(private_data->p_ud_param,
					vf->vf_ud_param.ud_param.pbuf_addr,
					vf->vf_ud_param.ud_param.buf_len);
			} else {
				dp_print(dev->index, PRINT_OTHER,
					"%s: invalid ud_param.\n", __func__);
			}
		} else {
			dp_print(dev->index, PRINT_OTHER, "%s: no ud\n", __func__);
		}

		out_fence_fd = dp_timeline_create_fence(dev);

		if (!kfifo_put(&dev->file_wait_q, dmabuf))
			dp_print(dev->index, PRINT_ERROR, "put file_wait fail\n");

		dev->received_frame[i].file_vf = file_vf;
		dev->received_frame[i].vf = vf;
		dev->received_frame[i].dummy = false;
		dev->received_frame[i].transform = frame_info->transform;
		dev->received_frame[i].dst_x = frame_info->dst_x;
		dev->received_frame[i].dst_y = frame_info->dst_y;
		dev->received_frame[i].dst_w = frame_info->dst_w;
		dev->received_frame[i].dst_h = frame_info->dst_h;
		atomic_set(&dev->received_frame[i].on_use, true);

		if (!kfifo_put(&dev->receive_q, &dev->received_frame[i]))
			dp_print(dev->index, PRINT_ERROR, "put ready fail\n");

		wake_up_interruptible(&dev->wq);
	}

	if ((dev->is_first_frame || dev->is_switch_first_frame) && !is_dtv_src && dev->cur_is_i) {
		vf->type_ext |= VIDTYPE_EXT_DPSS_DROP;
		dp_print(dev->index, PRINT_ERROR, "first i, need drop\n");
	} else if ((dev->is_switch_first_frame || dev->is_first_frame) &&
		is_dtv_src && dev->cur_is_i &&
		(get_src_signal_format(vf) == VFRAME_SIGNAL_FMT_SDR)) {
		vf->dpss_flg |= DPSS_FLG_OUT_DONE | DPSS_FLG_FORCE_DPSS_DONE;
		dp_print(dev->index, PRINT_OTHER,
			"first i, set dpss done flag, then vd1 toggle\n");
	}

	if (dev->is_switch_first_frame && is_pp_swtch) {
		vf->type_ext |= VIDTYPE_EXT_DPSS_EOS;
		dp_print(dev->index, PRINT_OTHER, "frame_index=%d, need eos flag\n",
			vf->frame_index);
	}

	frame_info->out_fence_fd = out_fence_fd;

	out_fd = get_unused_fd_flags(O_CLOEXEC);
	if (out_fd < 0) {
		dp_print(dev->index, PRINT_ERROR, "not fd!!!\n");
		return -ENOMEM;
	}
	fd_install(out_fd, dmabuf->file);

	dma_buf_get(out_fd);
	frame_info->out_fd = out_fd;
	frame_info->is_repeat = is_repeat;
	frame_info->is_i = vf->type & VIDTYPE_INTERLACE;
	frame_info->frame_index = frame_index;

	dp_print(dev->index, PRINT_OTHER,
		"%s done: dmabuf =%px, dmabuf->file=%px, out_fd=%d, out_fence_fd =%d, is_i=%d\n",
		__func__, dmabuf, dmabuf->file, out_fd, out_fence_fd, frame_info->is_i);

	dev->last_file = file_vf;
	dev->last_dmabuf = dmabuf;
	dev->last_frame_bypass = false;
	dev->is_first_frame = false;
	dev->is_switch_first_frame = false;

	return ret;
}

static int dpss_process_q_output(struct dpss_process_dev *dev, u32 fd)
{
	struct file *file_vf = NULL;
	struct file_private_data *private_data = NULL;
	struct dma_buf *dmabuf;
	struct vframe_s *vf = NULL;
	int frame_index = -1;
	enum DPSS_ERRORTYPE ret = 0;

	file_vf = fget(fd);
	if (!dev || !file_vf || !file_vf->private_data) {
		pr_info("q_output: NULL param.\n");
		return 0;
	}

	if (!dev->inited) {
		dp_print(dev->index, PRINT_ERROR, "q_output but not inited.\n");
		return 0;
	}

	total_get_count++;
	dp_print(dev->index, PRINT_OTHER,
		"%s: fd=%d, file_vf=%px, file_vf->private_data=%px\n",
		__func__, fd, file_vf, file_vf->private_data);

	private_data = dpss_proc_get_file_private_data(file_vf, false);
	if (!private_data || !private_data->vf_p) {
		dp_print(dev->index, PRINT_ERROR,
			"%s: private_data null, put file=%px.\n",
			__func__, file_vf);
		fput(file_vf);
		total_put_count++;
		return 0;
	}
	/*if dpss bypass, not need queue dpss_buffer to dpss, it is input dpss_buffer*/
	if (private_data->flag & V4LVIDEO_FLAG_DI_V3) {
		dp_print(dev->index, PRINT_OTHER,
			"%s: no bypss need put file=%px.\n",
			__func__, private_data->file);
		vf = private_data->vf_p;
		/*dpss vf has dec vf, need put dec file*/

		ret = queue_outbuf_to_dpss(dev, vf);
		if (ret != DPSS_ERR_NONE)
			dp_print(dev->index, PRINT_ERROR,
				"%s: queue_outbuf failed:%x.\n",
				__func__, ret);
		pop_dpss_out_q(dev, vf);
	} else if (private_data->flag & V4LVIDEO_FLAG_DI_BYPASS) {
		/*dpss bypass, need put dec file*/
		/*decoder vf maybe free, so should not to use vf struct*/
		vf = (struct vframe_s *)(private_data->vf_p);
		dp_print(dev->index, PRINT_OTHER,
			"%s: bypss need put file=%px.\n",
			__func__, private_data->file);
		if (vf && private_data->file) {
			frame_index = vf->frame_index;
			//dp_put_file(dev, private_data->file);
		} else {
			dp_print(dev->index, PRINT_ERROR,
				"%s: dpss bypass, but dec vf/file is null vf=%px.\n",
				__func__, vf);
		}
		private_data->file = NULL;
	}

	dp_print(dev->index, PRINT_OTHER, "%s: vf=%px, frame_index=%d.\n",
		__func__, vf, frame_index);
	private_data->vf_p = NULL;
	private_data->is_keep = false;
	dmabuf = (struct dma_buf *)file_vf->private_data;
	if (!kfifo_put(&dev->file_free_q, dmabuf))
		dp_print(dev->index, PRINT_ERROR, "%s: file_free_q put fail\n", __func__);
	fput(file_vf);
	total_put_count++;
	return 0;
}

static int parse_param(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (token) {
		len = strlen(token);
		do {
			token = strsep(&params, " ");
			if (!token)
				break;
			while (token &&
			       (isspace(*token) ||
				!isgraph(*token)) && len) {
				token++;
				len--;
			}
			if (len == 0)
				break;
			ret = kstrtoint(token, 0, &res);
			if (ret < 0)
				break;
			len = strlen(token);
			*out++ = res;
			count++;
		} while ((count < para_num) && (len > 0));
	}

	kfree(params_base);
	return count;
}

static long dpss_process_ioctl(struct file *file,
				 unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct frame_info_t frame_info;
	struct dpss_process_dev *dev = (struct dpss_process_dev *)file->private_data;
	int fd;

	switch (cmd) {
	case DPSS_PROCESS_IOCTL_INIT:
		ret = dpss_process_init(dev);
		break;
	case DPSS_PROCESS_IOCTL_UNINIT:
		ret = dpss_process_uninit(dev);
		break;
	case DPSS_PROCESS_IOCTL_SET_FRAME:
		if (copy_from_user(&frame_info, argp, sizeof(frame_info)) != 0)
			return -EFAULT;
		ret = dpss_process_set_frame(dev, &frame_info);
		if (ret != 0 && ret != 3)
			return ret;
		if (copy_to_user(argp, &frame_info, sizeof(struct frame_info_t)) != 0)
			return -EFAULT;
		break;
	case DPSS_PROCESS_IOCTL_Q_OUTPUT:
		if (copy_from_user(&fd, argp, sizeof(u32)) == 0)
			ret = dpss_process_q_output(dev, fd);
		else
			ret = -EFAULT;
		break;

	case DPSS_PROCESS_IOCTL_GET_ENABLE:
#ifdef CONFIG_AMLOGIC_BUF_MANAGER
		ret = get_di_proc_enable();
#else
		ret = 0;
#endif
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long dpss_process_compat_ioctl(struct file *file, unsigned int cmd,
					ulong arg)
{
	long ret = 0;

	ret = dpss_process_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static int dpss_process_open(struct inode *inode, struct file *file)
{
	struct dpss_process_dev *dev;
	struct dpss_process_port_s *port;
	int index;
	struct sched_param param = {.sched_priority = 2};

	index = iminor(inode);
	pr_info("%s iminor(inode) =%d\n", __func__, index);
	if (index < 0 || index >= ARRAY_SIZE(ports))
		return -ENODEV;

	port = &ports[index];

	mutex_lock(&dpss_process_mutex);

	if (port->open_count > 0) {
		mutex_unlock(&dpss_process_mutex);
		pr_err("dpss_process: instance %d is already opened",
		       port->index);
		return -EBUSY;
	}

	dev = vmalloc(sizeof(*dev));
	if (!dev) {
		mutex_unlock(&dpss_process_mutex);
		pr_err("dpss_process: instance %d alloc dev failed",
		       port->index);
		return -ENOMEM;
	}
	memset(dev, 0, sizeof(struct dpss_process_dev));

	file->private_data = dev;
	dev->index = port->index;
	port->open_count++;
	port->process_dev = dev;
	mutex_unlock(&dpss_process_mutex);

	dev->thread_need_stop = false;
	init_waitqueue_head(&dev->wq);
	mutex_init(&dev->mutex_dpss_out);
	dev->kthread = kthread_create(dpss_process_thread, dev, port->name);
	if (IS_ERR(dev->kthread)) {
		pr_err("dpss_process_thread creat failed\n");
		return -ENOMEM;
	}

	if (sched_setscheduler(dev->kthread, SCHED_FIFO, &param))
		dp_print(dev->index, PRINT_ERROR, "Could not set realtime priority.\n");

	wake_up_process(dev->kthread);

	return 0;
}

static int dpss_process_release(struct inode *inode, struct file *file)
{
	struct dpss_process_dev *dev = file->private_data;
	struct dpss_process_port_s *port;
	int index;
	int ret = 0, i = 0;

	index = iminor(inode);
	pr_info("%s iminor(inode) =%d\n", __func__, index);
	if (index < 0 || index >= ARRAY_SIZE(ports))
		return -ENODEV;

	if (!dev)
		return -ENOMEM;

	port = &ports[index];

	if (dev->kthread) {
		dev->thread_need_stop = true;
		kthread_stop(dev->kthread);
		wake_up_interruptible(&dev->wq);
		dev->kthread = NULL;
		dev->thread_need_stop = false;
	}

	while (1) {
		i++;
		if (dev->thread_stopped)
			break;
		usleep_range(9000, 10000);
		if (i > WAIT_THREAD_STOPPED_TIMEOUT) {
			pr_err("wait thread timeout\n");
			break;
		}
	}

	if (dev->inited) {
		dp_print(dev->index, PRINT_ERROR,
			"no uninit before close, so need uninit first\n");
		ret = dpss_process_uninit(dev);
		if (ret != 0)
			pr_err("%s, disable fail\n", __func__);
	}

	mutex_lock(&dpss_process_mutex);
	port->open_count--;
	port->process_dev = NULL;
	mutex_unlock(&dpss_process_mutex);

	vfree(dev);
	return 0;
}

static const struct file_operations dpss_process_fops = {
	.owner = THIS_MODULE,
	.open = dpss_process_open,
	.release = dpss_process_release,
	.unlocked_ioctl = dpss_process_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = dpss_process_compat_ioctl,
#endif
	.poll = NULL,
};

static ssize_t print_flag_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current print_flag is %d\n",
			get_dp_print_flag(0));
}

static ssize_t print_flag_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_dp_print_flag(0, tmp);
	return count;
}

static ssize_t total_get_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_get_count);
}

static ssize_t total_put_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_put_count);
}

static ssize_t total_src_get_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_src_get_count);
}

static ssize_t total_src_put_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_src_put_count);
}

static ssize_t total_fill_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_fill_count);
}

static ssize_t total_fill_done_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_fill_done_count);
}

static ssize_t total_empty_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_empty_count);
}

static ssize_t total_empty_done_count_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", total_empty_done_count);
}

static ssize_t buf_mgr_print_flag_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
#ifdef CONFIG_AMLOGIC_BUF_MANAGER
	return sprintf(buf, "%d\n", dpss_buf_mgr_print_flag);
#else
	return sprintf(buf, "%d\n", 0);
#endif
}

static ssize_t buf_mgr_print_flag_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
#ifdef CONFIG_AMLOGIC_BUF_MANAGER
	set_buf_mgr_print_flag(tmp);
#endif
	dpss_buf_mgr_print_flag = tmp;
	return count;
}

static ssize_t di_proc_enable_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
#ifdef CONFIG_AMLOGIC_BUF_MANAGER
	return sprintf(buf, "%d\n", get_di_proc_enable());
#else
	return sprintf(buf, "%d\n", 0);
#endif
}

static ssize_t di_proc_enable_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
#ifdef CONFIG_AMLOGIC_BUF_MANAGER
	set_di_proc_enable(tmp);
#endif
	return count;
}

static ssize_t q_dropped_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", q_dropped);
}

static ssize_t q_dropped_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	q_dropped = tmp;
	return count;
}

static ssize_t force_width_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", force_width);
}

static ssize_t force_width_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_width = tmp;
	return count;
}

static ssize_t force_height_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", force_height);
}

static ssize_t force_height_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	force_height = tmp;
	return count;
}

static ssize_t dpss_bypass_show(const struct class *class,
				      const struct class_attribute *attr,
				      char *buf)
{
	return sprintf(buf, "%d\n", dpss_bypass);
}

static ssize_t dpss_bypass_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	dpss_bypass = tmp;
	return count;
}

static ssize_t work_mode_ctl_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf, "0x01 NR, 0x02 DV, 0x04 LC_EVC, 0x08 DCT, 0x10 FRC, 0x20 HDR, 0x40 DI");

	len += sprintf(buf + len, ", 0x80 Main.\n");

	len += sprintf(buf + len, "current work mode is %d.\n", work_mode_ctl);

	return len;
}

static ssize_t work_mode_ctl_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp = 0;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	work_mode_ctl = tmp;
	return count;
}

static ssize_t work_mode_ctl_pip_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf, "0x01 NR, 0x08 DCT, 0x40 DI.\n");
	len += sprintf(buf + len, "current work mode is %d.\n", work_mode_ctl_pip);

	return len;
}

static ssize_t work_mode_ctl_pip_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	work_mode_ctl_pip = tmp;
	return count;
}

static ssize_t continue_dump_num_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current continue_dump_num is %d\n",
			continue_dump_num);
}

static ssize_t continue_dump_num_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	continue_dump_num = tmp;
	is_start_dump = 1;
	return count;
}

static ssize_t continue_dump_top_num_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current continue_dump_top_num is %d\n",
			continue_dump_top_num);
}

static ssize_t continue_dump_top_num_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	continue_dump_top_num = tmp;
	is_start_dump = 1;
	return count;
}

static ssize_t continue_dump_bottom_num_show(const struct class *cla,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 80,
			"current continue_dump_bottom_num is %d\n",
			continue_dump_bottom_num);
}

static ssize_t continue_dump_bottom_num_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	continue_dump_bottom_num = tmp;
	is_start_dump = 1;
	return count;
}

static ssize_t force_direct_mode_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf + len, "force_direct_mode is %d.\n", force_direct_mode);

	return len;
}

static ssize_t force_direct_mode_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	force_direct_mode = tmp;
	return count;
}

static ssize_t temperature_control_en_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf + len, "temperature_control_en is %d.\n", temperature_control_en);

	return len;
}

static ssize_t temperature_control_en_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	temperature_control_en = tmp;
	return count;
}
static ssize_t force_num_to_vd1_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf + len, "force_num_to_vd1 is %d.\n", force_num_to_vd1);

	return len;
}

static ssize_t force_num_to_vd1_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	force_num_to_vd1 = tmp;
	return count;
}

static ssize_t dpss_out_axis_show(const struct class *cla, const struct class_attribute *attr,
	char *buf)
{
	return snprintf(buf, 40, "%d %d %d %d\n",
		dpss_out_axis[0], dpss_out_axis[1], dpss_out_axis[2], dpss_out_axis[3]);
}

static ssize_t dpss_out_axis_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	int axis[4];

	if (likely(parse_param(buf, 4, axis) == 4)) {
		pr_info("set dpss out axis: x:%d,y:%d,w:%d,h:%d.\n ",
			axis[0], axis[1], axis[2], axis[3]);
		dpss_out_axis[0] = axis[0];
		dpss_out_axis[1] = axis[1];
		dpss_out_axis[2] = axis[2];
		dpss_out_axis[3] = axis[3];
	}

	return strnlen(buf, count);
}

static ssize_t display_scaling_factor_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 40, "display_scaling_factor:%d %d %d %d.\n",
		display_scaling_factor[0],
		display_scaling_factor[1],
		display_scaling_factor[2],
		display_scaling_factor[3]);
}

static ssize_t display_scaling_factor_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	int temp[4];

	if (likely(parse_param(buf, 4, temp) == 4)) {
		pr_info("set display_scaling_factor: %d %d %d %d.\n ",
			temp[0], temp[1], temp[2], temp[3]);
		display_scaling_factor[0] = temp[0];
		display_scaling_factor[1] = temp[1];
		display_scaling_factor[2] = temp[2];
		display_scaling_factor[3] = temp[3];
	}

	return strnlen(buf, count);
}

static ssize_t dpss_rotate_en_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf + len, "dpss_rotate_en is %d.\n", dpss_rotate_en);

	return len;
}

static ssize_t dpss_rotate_en_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	dpss_rotate_en = tmp;
	return count;
}

static ssize_t rotate_dump_flag_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len = sprintf(buf + len, "rotate_dump_flag is %d.\n", rotate_dump_flag);

	return len;
}

static ssize_t rotate_dump_flag_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	rotate_dump_flag = tmp;
	set_dump_flag(rotate_dump_flag);
	return count;
}

static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RO(total_get_count);
static CLASS_ATTR_RO(total_put_count);
static CLASS_ATTR_RO(total_src_get_count);
static CLASS_ATTR_RO(total_src_put_count);
static CLASS_ATTR_RO(total_fill_count);
static CLASS_ATTR_RO(total_fill_done_count);
static CLASS_ATTR_RO(total_empty_count);
static CLASS_ATTR_RO(total_empty_done_count);
static CLASS_ATTR_RW(buf_mgr_print_flag);
static CLASS_ATTR_RW(di_proc_enable);
static CLASS_ATTR_RW(q_dropped);
static CLASS_ATTR_RW(force_width);
static CLASS_ATTR_RW(force_height);
static CLASS_ATTR_RW(dpss_bypass);
static CLASS_ATTR_RW(work_mode_ctl);
static CLASS_ATTR_RW(work_mode_ctl_pip);
static CLASS_ATTR_RW(continue_dump_num);
static CLASS_ATTR_RW(continue_dump_top_num);
static CLASS_ATTR_RW(continue_dump_bottom_num);
static CLASS_ATTR_RW(force_direct_mode);
static CLASS_ATTR_RW(temperature_control_en);
static CLASS_ATTR_RW(force_num_to_vd1);
static CLASS_ATTR_RW(dpss_out_axis);
static CLASS_ATTR_RW(display_scaling_factor);
static CLASS_ATTR_RW(rotate_dump_flag);
static CLASS_ATTR_RW(dpss_rotate_en);

static struct attribute *dpss_process_class_attrs[] = {
	&class_attr_print_flag.attr,
	&class_attr_total_get_count.attr,
	&class_attr_total_put_count.attr,
	&class_attr_total_src_get_count.attr,
	&class_attr_total_src_put_count.attr,
	&class_attr_total_fill_count.attr,
	&class_attr_total_fill_done_count.attr,
	&class_attr_total_empty_count.attr,
	&class_attr_total_empty_done_count.attr,
	&class_attr_buf_mgr_print_flag.attr,
	&class_attr_di_proc_enable.attr,
	&class_attr_q_dropped.attr,
	&class_attr_force_width.attr,
	&class_attr_force_height.attr,
	&class_attr_dpss_bypass.attr,
	&class_attr_work_mode_ctl.attr,
	&class_attr_work_mode_ctl_pip.attr,
	&class_attr_continue_dump_num.attr,
	&class_attr_continue_dump_top_num.attr,
	&class_attr_continue_dump_bottom_num.attr,
	&class_attr_force_direct_mode.attr,
	&class_attr_temperature_control_en.attr,
	&class_attr_force_num_to_vd1.attr,
	&class_attr_dpss_out_axis.attr,
	&class_attr_display_scaling_factor.attr,
	&class_attr_rotate_dump_flag.attr,
	&class_attr_dpss_rotate_en.attr,
	NULL
};

ATTRIBUTE_GROUPS(dpss_process_class);

static struct class dpss_process_class = {
	.name = "di_process",
	.class_groups = dpss_process_class_groups,
};

static const struct of_device_id amlogic_dpss_process_dt_match[] = {
	{.compatible = "amlogic, dpss_process",
	},
	{},
};

static int dpss_process_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	struct dpss_process_port_s *st;
	int di_backend_support = 0;

	pr_debug("dpss_process probe\n");
	/*need read from dts*/

	ret = class_register(&dpss_process_class);
	if (ret < 0)
		return ret;
	ret = register_chrdev(DI_PROCESS_MAJOR, DPSS_PROCESS_DEVICE_NAME, &dpss_process_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for dpss_process device\n");
		goto error1;
	}

	of_property_read_u32(pdev->dev.of_node, "di_backend_support_en", &di_backend_support);
	if (di_backend_support)
		set_di_proc_enable(1);

	for (st = &ports[0], i = 0;
	     i < dpss_process_instance_num; i++, st++) {
		pr_debug("%s:ports[i].name=%s, i=%d\n", __func__,
		       ports[i].name, i);
		st->pdev = &pdev->dev;
		st->class_dev = device_create(&dpss_process_class, NULL,
					      MKDEV(DI_PROCESS_MAJOR, i),
					      NULL, ports[i].name);
		dp_timeline_create(i);
	}

#ifdef CONFIG_AMLOGIC_DPSS_THERMAL
	if (of_property_present(pdev->dev.of_node, "#cooling-cells"))
		register_dpss_cooling(pdev->dev.of_node);
#endif

	display_speed_param_init();
	pr_debug("%s num=%d\n", __func__, dpss_process_instance_num);
	return ret;

error1:
	pr_err("%s error\n", __func__);
	unregister_chrdev(DI_PROCESS_MAJOR, DPSS_PROCESS_DEVICE_NAME);
	class_unregister(&dpss_process_class);
	return ret;
}

static void dpss_process_remove(struct platform_device *pdev)
{
	int i;
	struct dpss_process_port_s *st;

	for (st = &ports[0], i = 0;
	     i < dpss_process_instance_num; i++, st++)
		device_destroy(&dpss_process_class,
			       MKDEV(DI_PROCESS_MAJOR, i));

	unregister_chrdev(DI_PROCESS_MAJOR, DPSS_PROCESS_DEVICE_NAME);
	class_destroy(&dpss_process_class);
#ifdef CONFIG_AMLOGIC_DPSS_THERMAL
	unregister_dpss_cooling();
#endif
};

static void dpss_process_shutdown(struct platform_device *pdev)
{
	int i = 0;
	struct dpss_process_dev *dev = NULL;

	for (i = 0; i < dpss_process_instance_num; i++) {
		dev = ports[i].process_dev;
		if (!dev)
			continue;

		mutex_lock(&dpss_process_mutex);
		dev->inited = false;
		if (dev->fence_creat_count != dev->fence_release_count) {
			dp_print(dev->index, PRINT_OTHER,
				"%s: fence_creat_count =%lld, fence_release_count=%lld\n",
				__func__,
				dev->fence_creat_count,
				dev->fence_release_count);
			dp_timeline_increase(dev, dev->fence_creat_count
				- dev->fence_release_count);
		}
		mutex_unlock(&dpss_process_mutex);
	}
}

static struct platform_driver dpss_process_driver = {
	.probe = dpss_process_probe,
	.remove = dpss_process_remove,
	.shutdown = dpss_process_shutdown,
	.driver = {
		.owner = THIS_MODULE,
		.name = "dpss_process",
		.of_match_table = amlogic_dpss_process_dt_match,
	}
};

int __init dpss_process_module_init(void)
{
	pr_debug("dpss process module init\n");
	return platform_driver_register(&dpss_process_driver);
}

void __exit dpss_process_module_exit(void)
{
	platform_driver_unregister(&dpss_process_driver);
}
