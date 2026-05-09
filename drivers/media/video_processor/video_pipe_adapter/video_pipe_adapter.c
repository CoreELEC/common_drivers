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
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/compat.h>
#include <linux/amlogic/media/vfm/vframe.h>
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
#include <linux/amlogic/media/frc/frc_common.h>
#endif
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
#include <linux/amlogic/media/dpss/dpss_frc.h>
#endif
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>
#include <linux/amlogic/media/video_processor/video_pipe_adapter.h>
#include "video_pipe_adapter_int.h"

#define DEVICE_NAME   "video_pipe_adapter"
static u32 print_flag;
static struct keeper_mgr keeper_mgr_private;

static ssize_t print_flag_show(const struct class *class,
		const struct class_attribute *attr, char *buf)
{
	return sprintf(buf, "current print_flag is %d.\n", print_flag);
}

static ssize_t print_flag_store(const struct class *class,
		const struct class_attribute *attr, const char *buf,
		size_t count)
{
	int val;
	ssize_t ret;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	if (val > 0)
		print_flag = val;
	else
		print_flag = 0;

	pr_info("set print_flag to %d.\n", print_flag);
	return count;
}

static CLASS_ATTR_RW(print_flag);

static struct attribute *video_pipe_adapter_class_attrs[] = {
	&class_attr_print_flag.attr,
	NULL
};
ATTRIBUTE_GROUPS(video_pipe_adapter_class);

static struct class video_pipe_adapter_class = {
	.name = DEVICE_NAME,
	.class_groups = video_pipe_adapter_class_groups,
};

static const struct of_device_id video_pipe_adapter_dt_match[] = {
	{.compatible = "amlogic, video_pipe_adapter",
	},
	{},
};

static int video_pipe_adapter_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int video_pipe_adapter_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long video_pipe_adapter_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;
	void __user *argp = (void __user *)arg;
	struct video_delay_arg_t video_delay_arg;

	switch (cmd) {
	case VIDEO_PIPE_ADAPTER_GET_VIDEO_DELAY:
		memset(&video_delay_arg, 0, sizeof(struct video_delay_arg_t));
		if (copy_from_user(&video_delay_arg, argp, sizeof(struct video_delay_arg_t)))
			return -EINVAL;
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
		video_delay_arg.video_delay =
			dpss_frc_get_video_latency_new(video_delay_arg.input_fps,
						video_delay_arg.output_fps);
#endif
		if (copy_to_user(argp, &video_delay_arg, sizeof(struct video_delay_arg_t)))
			ret = -EFAULT;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long video_pipe_adapter_compat_ioctl(struct file *file, unsigned int cmd, ulong arg)
{
	long ret = 0;

	ret = video_pipe_adapter_ioctl(file, cmd, (ulong)compat_ptr(arg));
	return ret;
}
#endif

static const struct file_operations video_pipe_adapter_fops = {
	.owner = THIS_MODULE,
	.open = video_pipe_adapter_open,
	.release = video_pipe_adapter_release,
	.unlocked_ioctl = video_pipe_adapter_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = video_pipe_adapter_compat_ioctl,
#endif
	.poll = NULL,
};

static struct keeper_mgr *get_keeper_mgr(void)
{
	return &keeper_mgr_private;
}

static void keeper_mgr_init(void)
{
	struct keeper_mgr *mgr = get_keeper_mgr();

	memset(mgr, 0, sizeof(struct keeper_mgr));
	spin_lock_init(&mgr->lock);
}

void video_keeper_keep_mem(void *mem_handle, int type, int *id)
{
	int ret;
	int old_id = *id;
	struct keeper_mgr *mgr = get_keeper_mgr();
	int i;
	unsigned long flags;
	int have_samed = 0;
	int keep_id = -1;
	int slot_i = -1;

	if (!mem_handle) {
		pr_err("video-adapter: mem handle is NULL.\n");
		return;
	}

	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (!mgr->keep_list[i].handle && slot_i < 0) {
			slot_i = i;
		} else if (mem_handle == mgr->keep_list[i].handle) {
			mgr->keep_list[i].count++;
			have_samed = true;
			keep_id = mgr->keep_list[i].keep_id;
			break;
		}
	}

	if (have_samed) {
		spin_unlock_irqrestore(&mgr->lock, flags);
		*id = keep_id;
		pr_info("video-adapter: keep same mem handle=%p, keep_id=%d\n",
			mem_handle, keep_id);
		return;
	}

	if (slot_i < 0) {
		spin_unlock_irqrestore(&mgr->lock, flags);
		keep_id = -1;
		pr_err("video-adapter: keep failed because keep_list fulled!!!\n");
		return;
	}

	mgr->keep_list[slot_i].count++;
	if (mgr->keep_list[slot_i].count != 1)
		pr_err("video-adapter: keep error mem handle=%p\n", mem_handle);
	mgr->keep_list[slot_i].handle = mem_handle;

	spin_unlock_irqrestore(&mgr->lock, flags);

	ret = codec_mm_keeper_mask_keep_mem(mem_handle,
					    type);
	if (ret > 0) {
		if (old_id > 0 && ret != old_id) {
			/* wait 80 ms for vsync post. */
			codec_mm_keeper_unmask_keeper(old_id, 0);
		}
		*id = ret;
	} else {
		spin_lock_irqsave(&mgr->lock, flags);
		mgr->keep_list[slot_i].count--;
		mgr->keep_list[slot_i].keep_id = -1;
		mgr->keep_list[slot_i].handle = NULL;
		spin_unlock_irqrestore(&mgr->lock, flags);
		pr_err("video-adapter: keep mask error ret=%d\n", ret);
		return;
	}
	mgr->keep_list[slot_i].keep_id = *id;
}
EXPORT_SYMBOL(video_keeper_keep_mem);

void video_keeper_free_mem(int keep_id, int delayms)
{
	struct keeper_mgr *mgr = get_keeper_mgr();
	int i;
	unsigned long flags;
	int need_continue_keep = 0;

	if (keep_id <= 0) {
		pr_err("invalid keepid %d\n", keep_id);
		return;
	}

	spin_lock_irqsave(&mgr->lock, flags);
	for (i = 0; i < MAX_KEEP_FRAME; i++) {
		if (keep_id == mgr->keep_list[i].keep_id) {
			mgr->keep_list[i].count--;
			if (mgr->keep_list[i].count > 0) {
				need_continue_keep = 1;
				break;
			} else if (mgr->keep_list[i].count == 0) {
				mgr->keep_list[i].keep_id = -1;
				mgr->keep_list[i].handle = NULL;
			} else {
				pr_err("video-adapter: free mem err count =%d\n",
				       mgr->keep_list[i].count);
			}
		}
	}
	spin_unlock_irqrestore(&mgr->lock, flags);

	if (need_continue_keep) {
		pr_info("video-adapter: need_continue_keep keep_id=%d\n", keep_id);
		return;
	}
	codec_mm_keeper_unmask_keeper(keep_id, delayms);
}
EXPORT_SYMBOL(video_keeper_free_mem);

static int video_pipe_adapter_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev;

	keeper_mgr_init();

	ret = class_register(&video_pipe_adapter_class);
	if (ret < 0) {
		pr_err("video_pipe_adapter class_register fail\n");
		return ret;
	}
	ret = register_chrdev(VIDEO_PIPE_ADAPTER_MAJOR, DEVICE_NAME, &video_pipe_adapter_fops);
	if (ret < 0) {
		ret = 1;
		goto error;
	}

	dev = device_create(&video_pipe_adapter_class,
			NULL,
			MKDEV(VIDEO_PIPE_ADAPTER_MAJOR, 0),
			NULL,
			DEVICE_NAME);

	if (IS_ERR_OR_NULL(dev)) {
		ret = 2;
		goto error;
	}

	return 0;
error:
	pr_err("video_pipe_adapter probe error(%d)\n", ret);
	unregister_chrdev(VIDEO_PIPE_ADAPTER_MAJOR, DEVICE_NAME);
	class_unregister(&video_pipe_adapter_class);
	return -1;
}

static void video_pipe_adapter_remove(struct platform_device *pdev)
{
	device_destroy(&video_pipe_adapter_class, MKDEV(VIDEO_PIPE_ADAPTER_MAJOR, 0));
	unregister_chrdev(VIDEO_PIPE_ADAPTER_MAJOR, DEVICE_NAME);
	class_destroy(&video_pipe_adapter_class);
}

static struct platform_driver video_pipe_adapter_driver = {
	.probe = video_pipe_adapter_probe,
	.remove = video_pipe_adapter_remove,
	.driver = {
		.owner = THIS_MODULE,
		.name = DEVICE_NAME,
		.of_match_table = video_pipe_adapter_dt_match,
	}
};

int __init video_pipe_adapter_module_init(void)
{
	return platform_driver_register(&video_pipe_adapter_driver);
}

void __exit video_pipe_adapter_module_exit(void)
{
	platform_driver_unregister(&video_pipe_adapter_driver);
}
