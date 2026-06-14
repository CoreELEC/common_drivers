// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "vpq_drv.h"
#include "vpq_printk.h"
#include "vpq_debug.h"
#include "vpq_ioctl.h"
#include "vpq_vfm.h"
#include "vpq_table_logic.h"

struct vpq_dev_s *vpq_dev;

struct vpq_dev_s *get_vpq_dev(void)
{
	return vpq_dev;
}

const struct class_attribute vpq_class_attr[] = {
	__ATTR(debug, 0644,
		vpq_debug_cmd_show, vpq_debug_cmd_store),

	__ATTR(log_lev, 0644,
		vpq_log_lev_show, vpq_log_lev_store),

	__ATTR(chip_infor, 0644,
		vpq_chip_infor_show, vpq_chip_infor_store),

	__ATTR(reg_table_ver, 0644,
		vpq_reg_table_ver_show, vpq_reg_table_ver_store),

	__ATTR(module_cfg, 0644,
		vpq_pq_module_cfg_show, vpq_pq_module_cfg_store),

	__ATTR(module_status, 0644,
		vpq_pq_module_status_show, vpq_pq_module_status_store),

	__ATTR(src_infor, 0644,
		vpq_src_infor_show, vpq_src_infor_store),

	__ATTR(other_infor, 0644,
		vpq_other_infor_show, vpq_other_infor_store),

	__ATTR(his_avg, 0644,
		vpq_hist_avg_show, vpq_hist_avg_store),

	__ATTR(pq_setting, 0644,
		vpq_pq_setting_show, vpq_pq_setting_store),

	__ATTR(dump_table, 0644,
		vpq_dump_show, vpq_dump_store),

	__ATTR_NULL,
};

const struct vpq_match_data_s vpq_t5w_match = {
	.chip_type = VPQ_CHIP_TV,
	.chip_id = VPQ_CHIP_T5W,
};

const struct vpq_match_data_s vpq_t3_match = {
	.chip_type = VPQ_CHIP_TV,
	.chip_id = VPQ_CHIP_T3,
};

const struct vpq_match_data_s vpq_t6w_match = {
	.chip_type = VPQ_CHIP_TV,
	.chip_id = VPQ_CHIP_T6W,
};

const struct vpq_match_data_s vpq_t6x_match = {
	.chip_type = VPQ_CHIP_TV,
	.chip_id = VPQ_CHIP_T6X,
};

const struct of_device_id vpq_dts_match[] = {
	{
		.compatible = "amlogic, vpq-t5w",
		.data = &vpq_t5w_match,
	},
	{
		.compatible = "amlogic, vpq-t3",
		.data = &vpq_t3_match,
	},
	{
		.compatible = "amlogic, vpq-t6w",
		.data = &vpq_t6w_match,
	},
	{
		.compatible = "amlogic, vpq-t6x",
		.data = &vpq_t6x_match,
	},
	{},
};

static int vpq_open(struct inode *inode, struct file *file)
{
	struct vpq_dev_s *vpq_devp;

	vpq_devp = container_of(inode->i_cdev, struct vpq_dev_s, vpq_cdev);
	file->private_data = vpq_devp;
	return 0;
}

static int vpq_release(struct inode *inode, struct file *file)
{
	file->private_data = NULL;
	return 0;
}

static long vpq_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = -1;

	ret = vpq_ioctl_process(file, cmd, arg);
	return ret;
}

#ifdef CONFIG_COMPAT
static long vpq_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	//vpq_pr_info("file:%px\n", file);

	arg = (unsigned long)compat_ptr(arg);
	ret = vpq_ioctl(file, cmd, arg);
	return ret;
}
#endif

static unsigned int vpq_poll(struct file *file, poll_table *wait)
{
	struct vpq_dev_s *devp = file->private_data;
	unsigned int mask = 0;

	//("start\n");

	poll_wait(file, &devp->queue, wait);

	mask = (POLLIN | POLLRDNORM);

	return mask;
}

const struct file_operations vpq_fops = {
	.owner = THIS_MODULE,
	.open = vpq_open,
	.release = vpq_release,
	.unlocked_ioctl = vpq_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vpq_compat_ioctl,
#endif
	.poll = vpq_poll,
};

static int vpq_dts_parse(struct vpq_dev_s *vpq_devp)
{
	int ret = 0;
	const struct of_device_id *of_id;
	struct platform_device *pdev = vpq_devp->pdev;

	//vpq_pr_info("start\n");

	of_id = of_match_device(vpq_dts_match, &pdev->dev);
	if (!of_id) {
		vpq_pr_err("no matching compatible string found for device\n");
		return -ENODEV;
	}

	vpq_pr_info("%s compatible\n", of_id->compatible);
	vpq_devp->pm_data = (struct vpq_match_data_s *)of_id->data;

	return ret;
}

static void vpq_event_work(struct work_struct *work)
{
	//vpq_pr_info("start\n");

	struct delayed_work *dwork = to_delayed_work(work);
	struct vpq_dev_s *devp =
		container_of(dwork, struct vpq_dev_s, event_dwork);
	if (!devp) {
		vpq_pr_info("dwork error\n");
		return;
	}

	//uevent message
	//console:/ # find /sys -name uevent | grep vpq
	//            /sys/devices/virtual/vpq/vpq/uevent
	//console:/ # cat sys/devices/virtual/vpq/vpq/uevent
	//            DEVNAME=vpq

	int ret = 0;
	char env[UEVENT_LEN_MAX];
	char devtype[] = "DEVTYPE=vpqevent";
	char *envp[3];

	memset(env, 0, sizeof(env));
	snprintf(env, UEVENT_LEN_MAX, "VPQ_EVENT_INFO=%d", devp->event_info);
	envp[0] = devtype;
	envp[1] = env;
	envp[2] = NULL;
	ret = kobject_uevent_env(&devp->dev->kobj, KOBJ_CHANGE, envp);
	//vpq_pr_info("event_info:%d, ret:%d\n", devp->event_info, ret);
}

static int vpq_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct vpq_dev_s *vpq_devp = NULL;

	//vpq_pr_info("start\n");

	vpq_dev = kzalloc(sizeof(*vpq_dev), GFP_KERNEL);
	if (!vpq_dev) {
		//vpq_pr_info("dev kzalloc error\n");
		ret = -1;
		goto fail_alloc_dev;
	}
	memset(vpq_dev, 0, sizeof(struct vpq_dev_s));
	vpq_devp = vpq_dev;

	ret = alloc_chrdev_region(&vpq_devp->devno, 0, VPQ_DEVNO, VPQ_NAME);
	if (ret < 0) {
		vpq_pr_err("devno alloc failed\n");
		goto fail_alloc_region;
	}

	vpq_devp->clsp = class_create(/*THIS_MODULE, */VPQ_CLS_NAME);
	if (IS_ERR(vpq_devp->clsp)) {
		vpq_pr_err("class create failed\n");
		ret = -1;
		goto fail_create_class;
	}

	for (i = 0; vpq_class_attr[i].attr.name; i++) {
		ret = class_create_file(vpq_devp->clsp, &vpq_class_attr[i]);
		if (ret < 0) {
			vpq_pr_err("class create file failed\n");
			goto fail_create_class_file;
		}
	}

	/* create cdev and register with sysfs */
	cdev_init(&vpq_devp->vpq_cdev, &vpq_fops);

	vpq_devp->vpq_cdev.owner = THIS_MODULE;
	ret = cdev_add(&vpq_devp->vpq_cdev, vpq_devp->devno, VPQ_DEVNO);
	if (ret < 0) {
		vpq_pr_err("add cdev failed\n");
		goto fail_add_cdev;
	}

	vpq_devp->dev = device_create(vpq_devp->clsp, NULL, vpq_devp->devno, vpq_devp, VPQ_NAME);
	if (!vpq_devp->dev) {
		vpq_pr_err("device_create failed\n");
		ret = -1;
		goto fail_create_dev;
	}

	dev_set_drvdata(vpq_devp->dev, vpq_devp);
	platform_set_drvdata(pdev, vpq_devp);
	vpq_devp->pdev = pdev;

	ret = vpq_dts_parse(vpq_devp);
	if (ret < 0) {
		vpq_pr_err("failed to parse device tree data\n");
		goto fail_create_dev;
	}

	vpq_vfm_init();
	vpq_table_init(vpq_devp);

	/*vpq event*/
	INIT_DELAYED_WORK(&vpq_devp->event_dwork, vpq_event_work);

	/*init queue*/
	init_waitqueue_head(&vpq_devp->queue);

	//vpq_pr_info("end\n");

	return ret;

fail_create_dev:
	cdev_del(&vpq_devp->vpq_cdev);
fail_add_cdev:
	for (i = 0; vpq_class_attr[i].attr.name; i++)
		class_remove_file(vpq_devp->clsp, &vpq_class_attr[i]);
fail_create_class_file:
	class_destroy(vpq_devp->clsp);
fail_create_class:
	unregister_chrdev_region(vpq_devp->devno, VPQ_DEVNO);
fail_alloc_region:
	kfree(vpq_dev);
	vpq_dev = NULL;
fail_alloc_dev:
	return ret;
}

static void vpq_remove(struct platform_device *pdev)
{
	int i;
	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	//vpq_pr_info("start\n");

	cancel_delayed_work(&vpq_devp->event_dwork);

	device_destroy(vpq_devp->clsp, vpq_devp->devno);
	cdev_del(&vpq_devp->vpq_cdev);
	for (i = 0; vpq_class_attr[i].attr.name; i++)
		class_remove_file(vpq_devp->clsp, &vpq_class_attr[i]);
	class_destroy(vpq_devp->clsp);
	unregister_chrdev_region(vpq_devp->devno, VPQ_DEVNO);

	vpq_table_deinit();

	kfree(vpq_dev);
	vpq_dev = NULL;
}

static void vpq_shutdown(struct platform_device *pdev)
{
	int i;
	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	//vpq_pr_info("start\n");

	device_destroy(vpq_devp->clsp, vpq_devp->devno);
	cdev_del(&vpq_devp->vpq_cdev);
	for (i = 0; vpq_class_attr[i].attr.name; i++)
		class_remove_file(vpq_devp->clsp, &vpq_class_attr[i]);
	class_destroy(vpq_devp->clsp);
	unregister_chrdev_region(vpq_devp->devno, VPQ_DEVNO);
	kfree(vpq_dev);
	vpq_dev = NULL;
}

static struct platform_driver vpq_driver = {
	.probe = vpq_probe,
	.remove = vpq_remove,
	.shutdown = vpq_shutdown,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = "aml_vpq",
		.owner = THIS_MODULE,
		.of_match_table = vpq_dts_match,
	},
};

int __init vpq_init(void)
{
	//vpq_pr_info("start\n");

	if (platform_driver_register(&vpq_driver)) {
		vpq_pr_err("module init failed\n");
		return -ENODEV;
	}

	return 0;
}

void __exit vpq_exit(void)
{
	//vpq_pr_info("start\n");

	platform_driver_unregister(&vpq_driver);
}
