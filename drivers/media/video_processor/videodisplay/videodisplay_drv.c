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
#include <linux/of.h>
#include <uapi/linux/sched/types.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/media_proxy/AmlVideoUserdata.h>
#include "videodisplay_process.h"
#include "videodisplay_drv.h"

#define VIDEODISPLAY_DEVICE_NAME   "videodisplay"
static const char videodisplay_group_name0[] = "vd0_debug";
static const char videodisplay_group_name1[] = "vd1_debug";
static const char videodisplay_group_name2[] = "vd2_debug";
static u32 videodisplay_instance_num;

static int videodisplay_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int videodisplay_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long videodisplay_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long videodisplay_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = videodisplay_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations videodisplay_fops = {
	.owner = THIS_MODULE,
	.open = videodisplay_open,
	.release = videodisplay_release,
	.unlocked_ioctl = videodisplay_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = videodisplay_compat_ioctl,
#endif
	.poll = NULL,
};

static int vd_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}

	return n;
}

static ssize_t print_flag_show(const struct class *cla,
			const struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, 80,
			"current print_flag: vd[0]=%d, vd[1]=%d, vd[2]=%d.\n",
			get_print_flag(0),
			get_print_flag(1),
			get_print_flag(2));
}

static ssize_t print_flag_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	long index, val;
	char *buf_orig, *parm[8] = {NULL};
	int num = 0;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	num = vd_parse_param(buf_orig, (char **)&parm);
	if (num == 1) {
		if (kstrtol(parm[0], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		set_print_flag(0, val);
		set_print_flag(1, val);
		set_print_flag(2, val);
	} else if (num == 2) {
		if (kstrtol(parm[0], 10, &index) < 0 || kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		set_print_flag(index, val);
	} else {
		pr_info("ERROR invalid param!\n");
	}

	kfree(buf_orig);
	return count;
}

static ssize_t debug_crop_pip_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current debug_crop_pip is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_CROP_PIP));
}

static ssize_t debug_crop_pip_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_CROP_PIP, tmp);
	return count;
}

static ssize_t debug_axis_pip_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current debug_axis_pip is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_AXIX_PIP));
}

static ssize_t debug_axis_pip_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_AXIX_PIP, tmp);
	return count;
}

static ssize_t dump_vframe_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"dump_vframe: %d.\n",
			get_debug_flag_val(VD_DEBUG_CLASS_DUMP_VFRAME));
}

static ssize_t dump_vframe_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_DUMP_VFRAME, tmp);
	return count;
}

static ssize_t force_composer_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current debug_force_composer is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER));
}

static ssize_t force_composer_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER, tmp);
	return count;
}

static ssize_t force_composer_pip_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current debug_force_composer_pip is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER_PIP));
}

static ssize_t force_composer_pip_store(const struct class *cla,
					const struct class_attribute *attr,
					const char *buf,
					size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER_PIP, tmp);
	return count;
}

static ssize_t transform_show(const struct class *cla,
			const struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, 80,
			"current transform is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_TRANSFORM));
}

static ssize_t transform_store(const struct class *cla,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_TRANSFORM, tmp);
	return count;
}

static ssize_t vidc_debug_show(const struct class *cla,
			const struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, 80,
			"current vidc_debug is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_VIDC));
}

static ssize_t vidc_debug_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_VIDC, tmp);
	return count;
}

static ssize_t vidc_pattern_debug_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current vidc_pattern_debug is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_VIDC_PATTERN));
}

static ssize_t vidc_pattern_debug_store(const struct class *cla,
					const struct class_attribute *attr,
					const char *buf,
					size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_VIDC_PATTERN, tmp);
	return count;
}

static ssize_t full_axis_show(const struct class *cla,
			const struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, 80,
			"current full_axis is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_FULL_AXIS));
}

static ssize_t full_axis_store(const struct class *cla,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_FULL_AXIS, tmp);
	return count;
}

static ssize_t print_close_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current print_close is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_PRINT_CLOSE));
}

static ssize_t print_close_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_PRINT_CLOSE, tmp);
	return count;
}

static ssize_t receive_wait_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current receive_wait is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_WAIT));
}

static ssize_t receive_wait_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_WAIT, tmp);
	return count;
}

static ssize_t margin_time_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current margin_time is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_MARGIN_TIME));
}

static ssize_t margin_time_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_MARGIN_TIME, tmp);
	return count;
}

static ssize_t max_width_show(const struct class *cla,
			const struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, 80,
			"current max_width is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_MAX_WIDTH));
}

static ssize_t max_width_store(const struct class *cla,
			const struct class_attribute *attr,
			const char *buf,
			size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_MAX_WIDTH, tmp);
	return count;
}

static ssize_t max_height_show(const struct class *cla,
			const struct class_attribute *attr,
			char *buf)
{
	return snprintf(buf, 80,
			"current max_height is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_MAX_HEIGHT));
}

static ssize_t max_height_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_MAX_HEIGHT, tmp);
	return count;
}

static ssize_t rotate_width_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current rotate_width is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_ROTATE_WIDTH));
}

static ssize_t rotate_width_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_ROTATE_WIDTH, tmp);
	return count;
}

static ssize_t rotate_height_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current rotate_height is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_ROTATE_HEIGHT));
}

static ssize_t rotate_height_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_ROTATE_HEIGHT, tmp);
	return count;
}

static ssize_t dewarp_rotate_width_show(const struct class *cla,
					const struct class_attribute *attr,
					char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_rotate_width is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_DEWARP_ROTATE_WIDTH));
}

static ssize_t dewarp_rotate_width_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_DEWARP_ROTATE_WIDTH, tmp);
	return count;
}

static ssize_t dewarp_rotate_height_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current dewarp_rotate_height is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_ROTATE_HEIGHT));
}

static ssize_t dewarp_rotate_height_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_ROTATE_HEIGHT, tmp);
	return count;
}

static ssize_t close_black_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current close_black is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_CLOSE_BLACK));
}

static ssize_t close_black_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_CLOSE_BLACK, tmp);
	return count;
}

static ssize_t composer_use_444_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", get_debug_flag_val(VD_DEBUG_CLASS_COMPOSER_USE_444));
}

static ssize_t composer_use_444_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	set_debug_flag_val(VD_DEBUG_CLASS_COMPOSER_USE_444, val);
	return count;
}

static ssize_t reset_drop_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "reset_drop: %d\n", get_debug_flag_val(VD_DEBUG_CLASS_RESET_DROP));
}

static ssize_t reset_drop_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	set_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER, val);
	return count;
}

static ssize_t drop_cnt_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int receive_count, receive_new_count, last_frame_index, drop_count, vpp_drop_count;

	receive_count = get_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_COUNT);
	receive_new_count = get_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_NEW_COUNT);
	last_frame_index = get_debug_flag_val(VD_DEBUG_CLASS_LAST_FRAME_INDEX);
	drop_count = get_debug_flag_val(VD_DEBUG_CLASS_DROP_COUNT);
	vpp_drop_count = get_debug_flag_val(VD_DEBUG_CLASS_VPP_DROP_COUNT);

	return sprintf(buf,
		"rec_cnt: %d, frame_index: %d, valid_cnt: %d, player_drop_cnt: %d, vpp_drop_cnt: %d, total_drop_cnt: %d\n",
		receive_count,
		last_frame_index,
		receive_new_count,
		drop_count,
		vpp_drop_count,
		drop_count + vpp_drop_count);
}

static ssize_t drop_cnt_pip_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return -EINVAL;
}

static ssize_t receive_count_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", get_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_COUNT));
}

static ssize_t receive_count_pip_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", get_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_COUNT_PIP));
}

static ssize_t receive_new_count_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", get_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_NEW_COUNT));
}

static ssize_t receive_new_count_pip_show(const struct class *class,
					const struct class_attribute *attr,
					char *buf)
{
	return sprintf(buf, "%d\n", get_debug_flag_val(VD_DEBUG_CLASS_RECEIVE_NEW_COUNT_PIP));
}

static ssize_t total_get_count_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", get_debug_flag_val(VD_DEBUG_CLASS_TOTAL_GET_COUNT));
}

static ssize_t total_put_count_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "%d\n", get_debug_flag_val(VD_DEBUG_CLASS_TOTAL_PUT_COUNT));
}

static ssize_t nn_need_time_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "nn_need_time: %d\n",
		get_debug_flag_val(VD_DEBUG_CLASS_NN_NEED_TIME));
}

static ssize_t nn_need_time_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	set_debug_flag_val(VD_DEBUG_CLASS_NN_NEED_TIME, val);
	return count;
}

static ssize_t nn_margin_time_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "nn_margin_time: %d\n",
		get_debug_flag_val(VD_DEBUG_CLASS_NN_MARGIN_TIME));
}

static ssize_t nn_margin_time_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	set_debug_flag_val(VD_DEBUG_CLASS_NN_MARGIN_TIME, val);
	return count;
}

static ssize_t nn_bypass_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "nn_bypass: %d\n", get_debug_flag_val(VD_DEBUG_CLASS_NN_BYPASS));
}

static ssize_t nn_bypass_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	set_debug_flag_val(VD_DEBUG_CLASS_NN_BYPASS, val);
	return count;
}

static ssize_t tv_fence_creat_count_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "tv_fence_creat_count: %d\n",
		get_debug_flag_val(VD_DEBUG_CLASS_FENCE_CREATE_COUNT));
}

static ssize_t vd_pulldown_level_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "vd_pulldown_level: %d\n",
		get_debug_flag_val(VD_DEBUG_CLASS_PULLDOWN_LEVEL));
}

static ssize_t vd_pulldown_level_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	set_debug_flag_val(VD_DEBUG_CLASS_PULLDOWN_LEVEL, val);
	return count;
}

static ssize_t vd_max_hold_count_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "vd_max_hold_count: %d\n",
		get_debug_flag_val(VD_DEBUG_CLASS_MAX_HOLD_COUNT));
}

static ssize_t vd_max_hold_count_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;
	set_debug_flag_val(VD_DEBUG_CLASS_MAX_HOLD_COUNT, val);
	return count;
}

static ssize_t vd_set_frame_delay_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return -EINVAL;
}

static ssize_t vd_set_frame_delay_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	return -EINVAL;
}

static ssize_t vd_dump_vframe_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return sprintf(buf, "vd_dump_vframe: %d\n",
		get_debug_flag_val(VD_DEBUG_CLASS_VD_DUMP_VFRAME));
}

static ssize_t vd_dump_vframe_store(const struct class *class,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	ssize_t r;
	int val;

	r = kstrtoint(buf, 0, &val);
	if (r < 0)
		return -EINVAL;

	set_debug_flag_val(VD_DEBUG_CLASS_VD_DUMP_VFRAME, val);

	return count;
}

static ssize_t actual_delay_count_show(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return -EINVAL;
}

static ssize_t vicp_output_dev_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
		"1 mif, 2 fbc, 3 mif+fbc. current choice is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_VICP_OUTPUT_DEV));
}

static ssize_t vicp_output_dev_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_VICP_OUTPUT_DEV, tmp);
	return count;
}

static ssize_t vicp_shrink_mode_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
		"0 2x, 1 4x, 2 8x. current choice is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_VICP_SHRINK_MODE));
}

static ssize_t vicp_shrink_mode_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_VICP_SHRINK_MODE, tmp);
	return count;
}

static ssize_t vicp_max_width_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "current vicp_max_width is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_VICP_MAX_WIDTH));
}

static ssize_t vicp_max_width_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_VICP_MAX_WIDTH, tmp);
	return count;
}

static ssize_t vicp_max_height_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "current vicp_max_height is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_VICP_MAX_HEIGHT));
}

static ssize_t vicp_max_height_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_VICP_MAX_HEIGHT, tmp);
	return count;
}

static ssize_t composer_dev_choice_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
		"1 ge2d, 2 dewarp, 3 vicp. current choice is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_COMPOSER_DEV_CHOICE));
}

static ssize_t composer_dev_choice_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_COMPOSER_DEV_CHOICE, tmp);
	return count;
}

static ssize_t force_comp_w_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
		"force_comp_w %d.\n", get_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER_WIDTH));
}

static ssize_t force_comp_w_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER_WIDTH, tmp);
	return count;
}

static ssize_t force_comp_h_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
		"force_comp_h %d.\n", get_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER_HEIGHT));
}

static ssize_t force_comp_h_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_FORCE_COMPOSER_HEIGHT, tmp);
	return count;
}

static ssize_t vd_test_fps_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	return count;
}

static ssize_t vd_test_fps_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return -EINVAL;
}

static ssize_t dewarp_load_flag_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "current dewarp_load_flag is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_DEWARP_LOAD_FLAG));
}

static ssize_t dewarp_load_flag_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	pr_info("set dewarp_load_flag to %ld.\n", tmp);
	set_debug_flag_val(VD_DEBUG_CLASS_DEWARP_LOAD_FLAG, tmp);
	return count;
}

static ssize_t lossy_compress_rate_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "current lossy_compress_rate is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_LOSSY_COMPRESS_RATE));
}

static ssize_t lossy_compress_rate_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_err("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	set_debug_flag_val(VD_DEBUG_CLASS_LOSSY_COMPRESS_RATE, tmp);
	return count;
}

static ssize_t enable_frc_pattern_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80,
			"current print_close is %d\n",
			get_debug_flag_val(VD_DEBUG_CLASS_FRC_PATTERN_EN));
}

static ssize_t enable_frc_pattern_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_info("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_FRC_PATTERN_EN, tmp);
	return count;
}

static ssize_t buffer_status_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return -EINVAL;
}

static ssize_t use_low_latency_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_err("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	set_debug_flag_val(VD_DEBUG_CLASS_USE_LOW_LATENCY, tmp);
	return count;
}

static ssize_t use_low_latency_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "use_low_latency is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_USE_LOW_LATENCY));
}

static ssize_t vd_composer_en_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_err("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	set_debug_flag_val(VD_DEBUG_CLASS_VD_COMPOSER_EN, tmp);
	return count;
}

static ssize_t vd_composer_en_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "vd_composer_en is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_VD_COMPOSER_EN));
}

static ssize_t vd_drop_org_i_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "drop_org_i is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_DROP_ORG_I));
}

static ssize_t vd_drop_org_i_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_err("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	set_debug_flag_val(VD_DEBUG_CLASS_DROP_ORG_I, tmp);
	return count;
}

static ssize_t vd_drop_hdmi_count_show(const struct class *cla,
	const struct class_attribute *attr, char *buf)
{
	return snprintf(buf, 80, "drop_max_time is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_DROP_HDMI_COUNT));
}

static ssize_t vd_drop_hdmi_count_store(const struct class *cla,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_err("ERROR converting %s to long int!\n", buf);
		return ret;
	}
	set_debug_flag_val(VD_DEBUG_CLASS_DROP_HDMI_COUNT, tmp);

	return count;
}

static ssize_t continue_dump_num_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0) {
		pr_err("ERROR converting %s to long int!\n", buf);
		return ret;
	}

	set_debug_flag_val(VD_DEBUG_CLASS_CONTINUE_DUMP_NUM, tmp);
	return count;
}

static ssize_t continue_dump_num_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "continue_dump_num is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_CONTINUE_DUMP_NUM));
}

static ssize_t max_count_hold_vsync_store(const struct class *cla,
				const struct class_attribute *attr,
				const char *buf,
				size_t count)
{
	long tmp;
	int ret;

	ret = kstrtol(buf, 0, &tmp);
	if (ret != 0)
		return ret;

	set_debug_flag_val(VD_DEBUG_CLASS_MAX_COUNT_HOLD_VSYNC, tmp);
	return count;
}

static ssize_t max_count_hold_vsync_show(const struct class *cla,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 80, "max count_hold_vsync is %d.\n",
		get_debug_flag_val(VD_DEBUG_CLASS_MAX_COUNT_HOLD_VSYNC));
}

static CLASS_ATTR_RW(debug_axis_pip);
static CLASS_ATTR_RW(debug_crop_pip);
static CLASS_ATTR_RW(force_composer);
static CLASS_ATTR_RW(force_composer_pip);
static CLASS_ATTR_RW(transform);
static CLASS_ATTR_RW(vidc_debug);
static CLASS_ATTR_RW(vidc_pattern_debug);
static CLASS_ATTR_RW(print_flag);
static CLASS_ATTR_RW(full_axis);
static CLASS_ATTR_RW(print_close);
static CLASS_ATTR_RW(receive_wait);
static CLASS_ATTR_RW(margin_time);
static CLASS_ATTR_RW(max_width);
static CLASS_ATTR_RW(max_height);
static CLASS_ATTR_RW(rotate_width);
static CLASS_ATTR_RW(rotate_height);
static CLASS_ATTR_RW(dewarp_rotate_width);
static CLASS_ATTR_RW(dewarp_rotate_height);
static CLASS_ATTR_RW(close_black);
static CLASS_ATTR_RW(composer_use_444);
static CLASS_ATTR_RW(reset_drop);
static CLASS_ATTR_RO(drop_cnt);
static CLASS_ATTR_RO(drop_cnt_pip);
static CLASS_ATTR_RO(receive_count);
static CLASS_ATTR_RO(receive_count_pip);
static CLASS_ATTR_RO(receive_new_count);
static CLASS_ATTR_RO(receive_new_count_pip);
static CLASS_ATTR_RO(total_get_count);
static CLASS_ATTR_RO(total_put_count);
static CLASS_ATTR_RW(nn_need_time);
static CLASS_ATTR_RW(nn_margin_time);
static CLASS_ATTR_RW(nn_bypass);
static CLASS_ATTR_RO(tv_fence_creat_count);
static CLASS_ATTR_RW(dump_vframe);
static CLASS_ATTR_RW(vd_pulldown_level);
static CLASS_ATTR_RW(vd_max_hold_count);
static CLASS_ATTR_RW(vd_set_frame_delay);
static CLASS_ATTR_RW(vd_dump_vframe);
static CLASS_ATTR_RO(actual_delay_count);
static CLASS_ATTR_RW(vicp_output_dev);
static CLASS_ATTR_RW(vicp_shrink_mode);
static CLASS_ATTR_RW(vicp_max_width);
static CLASS_ATTR_RW(vicp_max_height);
static CLASS_ATTR_RW(composer_dev_choice);
static CLASS_ATTR_RW(force_comp_w);
static CLASS_ATTR_RW(force_comp_h);
static CLASS_ATTR_RW(vd_test_fps);
static CLASS_ATTR_RW(dewarp_load_flag);
static CLASS_ATTR_RW(lossy_compress_rate);
static CLASS_ATTR_RW(enable_frc_pattern);
static CLASS_ATTR_RO(buffer_status);
static CLASS_ATTR_RW(use_low_latency);
static CLASS_ATTR_RW(vd_composer_en);
static CLASS_ATTR_RW(vd_drop_org_i);
static CLASS_ATTR_RW(vd_drop_hdmi_count);
static CLASS_ATTR_RW(continue_dump_num);
static CLASS_ATTR_RW(max_count_hold_vsync);

static struct attribute *videodisplay_class_attrs[] = {
	&class_attr_debug_crop_pip.attr,
	&class_attr_debug_axis_pip.attr,
	&class_attr_force_composer.attr,
	&class_attr_force_composer_pip.attr,
	&class_attr_transform.attr,
	&class_attr_vidc_debug.attr,
	&class_attr_vidc_pattern_debug.attr,
	&class_attr_print_flag.attr,
	&class_attr_full_axis.attr,
	&class_attr_print_close.attr,
	&class_attr_receive_wait.attr,
	&class_attr_margin_time.attr,
	&class_attr_max_width.attr,
	&class_attr_max_height.attr,
	&class_attr_rotate_width.attr,
	&class_attr_rotate_height.attr,
	&class_attr_dewarp_rotate_width.attr,
	&class_attr_dewarp_rotate_height.attr,
	&class_attr_close_black.attr,
	&class_attr_composer_use_444.attr,
	&class_attr_reset_drop.attr,
	&class_attr_drop_cnt.attr,
	&class_attr_drop_cnt_pip.attr,
	&class_attr_receive_count.attr,
	&class_attr_receive_count_pip.attr,
	&class_attr_receive_new_count.attr,
	&class_attr_receive_new_count_pip.attr,
	&class_attr_total_get_count.attr,
	&class_attr_total_put_count.attr,
	&class_attr_nn_need_time.attr,
	&class_attr_nn_margin_time.attr,
	&class_attr_nn_bypass.attr,
	&class_attr_tv_fence_creat_count.attr,
	&class_attr_dump_vframe.attr,
	&class_attr_vd_pulldown_level.attr,
	&class_attr_vd_max_hold_count.attr,
	&class_attr_vd_set_frame_delay.attr,
	&class_attr_vd_dump_vframe.attr,
	&class_attr_actual_delay_count.attr,
	&class_attr_vicp_output_dev.attr,
	&class_attr_vicp_shrink_mode.attr,
	&class_attr_vicp_max_width.attr,
	&class_attr_vicp_max_height.attr,
	&class_attr_composer_dev_choice.attr,
	&class_attr_force_comp_w.attr,
	&class_attr_force_comp_h.attr,
	&class_attr_vd_test_fps.attr,
	&class_attr_dewarp_load_flag.attr,
	&class_attr_lossy_compress_rate.attr,
	&class_attr_enable_frc_pattern.attr,
	&class_attr_buffer_status.attr,
	&class_attr_use_low_latency.attr,
	&class_attr_vd_composer_en.attr,
	&class_attr_vd_drop_org_i.attr,
	&class_attr_vd_drop_hdmi_count.attr,
	&class_attr_continue_dump_num.attr,
	&class_attr_max_count_hold_vsync.attr,
	NULL
};
ATTRIBUTE_GROUPS(videodisplay_class);

#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
struct mediaproxy_info_t vd_mediaproxy_display_info[] = {
	{
		.k_producer_name = "videodisplay0"
	},
	{
		.k_producer_name = "videodisplay1"
	},
	{
		.k_producer_name = "videodisplay2"
	},
};
#endif

static struct class videodisplay_class = {
	.name = VIDEODISPLAY_DEVICE_NAME,
	.class_groups = videodisplay_class_groups,
};

static const struct of_device_id amlogic_videodisplay_dt_match[] = {
	{.compatible = "amlogic, videodisplay",
	},
	{},
};

struct videodisplay_port_s vd_ports[] = {
	{
		.name = "videodisplay.0",
		.index = 0,
		.open_count = 0,
	},
	{
		.name = "videodisplay.1",
		.index = 1,
		.open_count = 0,
	},
	{
		.name = "videodisplay.2",
		.index = 2,
		.open_count = 0,
	},
};

struct videodisplay_port_s *videodisplay_get_port(u32 index)
{
	int i = 0;

	if (index >= videodisplay_instance_num) {
		vd_print(index, PRINT_ERROR, "%s: invalid index.\n", __func__);
		return NULL;
	}

	for (i = 0; i < videodisplay_instance_num; i++) {
		if (index == vd_ports[i].index)
			break;
	}

	return &vd_ports[i];
}

static struct bin_attribute videodisplay_attr[] = {
	{
		.attr.name = "dump_yuv",
		.attr.mode = 0664,
		.private = NULL,
		.read = vd_dump_yuv_show,
		.write = NULL,
	},
};

static struct bin_attribute *videodisplay_bin_attrs0[] = {
	&videodisplay_attr[0],
	NULL,
};

static struct bin_attribute *videodisplay_bin_attrs1[] = {
	&videodisplay_attr[0],
	NULL,
};

static struct bin_attribute *videodisplay_bin_attrs2[] = {
	&videodisplay_attr[0],
	NULL,
};

static const struct attribute_group videodisplay_attr_group[] = {
	{
		.name = videodisplay_group_name0,
		.bin_attrs = videodisplay_bin_attrs0,
	},
	{
		.name = videodisplay_group_name1,
		.bin_attrs = videodisplay_bin_attrs1,
	},
	{
		.name = videodisplay_group_name2,
		.bin_attrs = videodisplay_bin_attrs2,
	},
};

static int videodisplay_probe(struct platform_device *pdev)
{
	pr_info("enter %s.\n", __func__);

	int ret = 0;
	int i = 0;
	u32 layer_cap = 0;
	struct videodisplay_port_s *port;

	videodisplay_instance_num = 0;
	ret = of_property_read_u32(pdev->dev.of_node, "videodisplay_instance_num",
				&videodisplay_instance_num);
	if (ret > 0)
		dev_info(&pdev->dev, "Read 'videodisplay_instance_num' from dts: %u\n",
				videodisplay_instance_num);

	if (videodisplay_instance_num == 0) {
		layer_cap = video_get_layer_capability();
		if (layer_cap & LAYER0_SCALER)
			videodisplay_instance_num++;
		if (layer_cap & LAYER1_SCALER)
			videodisplay_instance_num++;
		if (layer_cap & LAYER2_SCALER)
			videodisplay_instance_num++;
	}

	if (is_meson_c3_cpu())
		videodisplay_instance_num = 1;
	ret = class_register(&videodisplay_class);
	if (ret < 0) {
		pr_err("%s: class_register fail\n", __func__);
		return ret;
	}
	ret = register_chrdev(VIDEODISPLAY_MAJOR, VIDEODISPLAY_DEVICE_NAME, &videodisplay_fops);
	if (ret < 0) {
		pr_err("Can't allocate major for videodisplay device\n");
		goto error1;
	}

	pr_info("enter %s-%d.\n", __func__, __LINE__);

	for (port = &vd_ports[0], i = 0;
	     i < videodisplay_instance_num; i++, port++) {
		pr_debug("%s:ports[i].name=%s, i=%d\n", __func__, vd_ports[i].name, i);
		port->pdev = &pdev->dev;
		port->class_dev = device_create(&videodisplay_class, NULL,
					      MKDEV(VIDEODISPLAY_MAJOR, i),
					      NULL, vd_ports[i].name);
		ret = of_property_read_u32(pdev->dev.of_node,
					   "vpu_dma_mask", &port->vpu_dma_mask);
		if (ret) {
			pr_debug("videodisplay don't find vpu_dma_mask\n");
			port->vpu_dma_mask = 0;
		}
		if (port->vpu_dma_mask) {
			ret = dma_set_coherent_mask(&pdev->dev, DMA_BIT_MASK(36));
			if (ret < 0) {
				pr_err("dma_set_coherent_mask fail\n");
				goto error1;
			}
			ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(36));
			if (ret < 0) {
				pr_err("dma_set_mask fail\n");
				goto error1;
			}
		}
		ret = sysfs_create_group(&port->class_dev->kobj, &videodisplay_attr_group[i]);
		if (ret)
			pr_err("videodisplay creat dump node fail, ret=%d.\n", ret);
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
		if (!vd_mediaproxy_display_info[i].k_producer_session) {
			media_proxy_produce_init(&vd_mediaproxy_display_info[i].k_producer_session,
				vd_mediaproxy_display_info[i].k_producer_name,
				MEDIA_VIDEO_METRICS_FRAME_TOGGLE_INFO |
				MEDIA_VIDEO_METRICS_FRAME_SIGNAFENCE_INFO);
		}
#endif

	}
	pr_info("%s num=%d\n", __func__, videodisplay_instance_num);
	return 0;
error1:
	pr_err("%s error\n", __func__);
	unregister_chrdev(VIDEODISPLAY_MAJOR, VIDEODISPLAY_DEVICE_NAME);
	class_unregister(&videodisplay_class);
	return ret;
}

static void videodisplay_remove(struct platform_device *pdev)
{
	int i;
	struct videodisplay_port_s *port;

	for (port = &vd_ports[0], i = 0;
	     i < videodisplay_instance_num; i++, port++)
		device_destroy(&videodisplay_class, MKDEV(VIDEODISPLAY_MAJOR, i));

	unregister_chrdev(VIDEODISPLAY_MAJOR, VIDEODISPLAY_DEVICE_NAME);
	class_destroy(&videodisplay_class);
};

static struct platform_driver videodisplay_driver = {
	.probe = videodisplay_probe,
	.remove = videodisplay_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = "videodisplay",
		.of_match_table = amlogic_videodisplay_dt_match,
	}
};

int __init videodisplay_module_init(void)
{
	pr_info("enter %s.\n", __func__);
	if (platform_driver_register(&videodisplay_driver)) {
		pr_err("failed to register videodisplay module\n");
		return -ENODEV;
	}
	return 0;
}

void __exit videodisplay_module_exit(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_PROXY
	int i;

	for (i = 0; i < videodisplay_instance_num; i++) {
		if (vd_mediaproxy_display_info[i].k_producer_session)
			media_proxy_produce_deinit(vd_mediaproxy_display_info[i]
				.k_producer_session);
	}
#endif

	platform_driver_unregister(&videodisplay_driver);
}
