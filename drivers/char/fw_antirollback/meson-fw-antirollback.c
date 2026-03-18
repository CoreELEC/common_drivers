// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/arm-smccc.h>

#define FW_ARB_DEVICE_NAME   "fw_antirollback"
#define FW_ARB_DRIVER_NAME   "fw_antirollback"
#define FW_ARB_CLASS_NAME    "fw_antirollback"

static int major_number;
static struct class *fw_arb_class;
static struct device *fw_arb_device;

#define IOCTL_CHECK_FW_ARB _IOR('S', 1, unsigned long)
#define IOCTL_UPDATE_FW_ARB _IOR('S', 2, unsigned long)

struct fw_device_data {
	struct device *dev;
	struct cdev cdev;
	dev_t devno;
	u32 fw_arb_cmd;
	u32 sub_check_cmd;
	u32 sub_update_cmd;
};

static long fw_arb_smc_call(unsigned long function_id, unsigned long arg1,
				unsigned long arg2, unsigned long arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg1, 0, 0, 0, 0, 0, 0, &res);

	return res.a0;
}

static int fw_arb_device_open(struct inode *inode, struct file *file)
{
	struct fw_device_data *dev_data;

	dev_data = container_of(inode->i_cdev, struct fw_device_data, cdev);
	file->private_data = dev_data;

	return 0;
}

static int fw_arb_device_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long fw_arb_device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct fw_device_data *dev_data = file->private_data;

	switch (cmd) {
	case IOCTL_CHECK_FW_ARB:
		ret = fw_arb_smc_call(dev_data->fw_arb_cmd, dev_data->sub_check_cmd, 0, 0);
		break;

	case IOCTL_UPDATE_FW_ARB:
		ret = fw_arb_smc_call(dev_data->fw_arb_cmd, dev_data->sub_update_cmd, 0, 0);
		break;

	default:
		pr_warn("unknown IOCTL cmd: 0x%x\n", cmd);
		return -ENOTTY;
	}

	return ret;
}

static const struct file_operations fops = {
	.open = fw_arb_device_open,
	.release = fw_arb_device_release,
	.unlocked_ioctl = fw_arb_device_ioctl,
	.compat_ioctl = compat_ptr_ioctl,
	.owner = THIS_MODULE,
};

static int fw_antirollback_driver_probe(struct platform_device *pdev)
{
	struct fw_device_data *dev_data;
	struct device *dev = &pdev->dev;
	int ret;

	dev_data = devm_kzalloc(dev, sizeof(*dev_data), GFP_KERNEL);
	if (!dev_data)
		return -ENOMEM;

	dev_data->dev = dev;

	ret = of_property_read_u32(dev->of_node, "fw_arb_cmd",
								&dev_data->fw_arb_cmd);
	if (ret) {
		dev_err(dev, "Failed to read fw_arb_cmd\n");
		dev_data->fw_arb_cmd = 0x0;
	}

	ret = of_property_read_u32(dev->of_node, "sub_check_cmd",
								&dev_data->sub_check_cmd);
	if (ret) {
		dev_err(dev, "Failed to read sub_check_cmd\n");
		dev_data->sub_check_cmd = 0x0;
	}

	ret = of_property_read_u32(dev->of_node, "sub_update_cmd",
								&dev_data->sub_update_cmd);
	if (ret) {
		dev_err(dev, "Failed to read sub_update_cmd\n");
		dev_data->sub_update_cmd = 0x0;
	}

	major_number = register_chrdev(0, FW_ARB_DEVICE_NAME, &fops);
	if (major_number < 0) {
		pr_err("Fail to register chrdev: %d\n", major_number);
		return major_number;
	}

	dev_data->devno = MKDEV(major_number, 0);
	cdev_init(&dev_data->cdev, &fops);
	dev_data->cdev.owner = THIS_MODULE;

	ret = cdev_add(&dev_data->cdev, dev_data->devno, 1);
	if (ret) {
		dev_err(dev, "Failed to add cdev: %d\n", ret);
		unregister_chrdev(major_number, FW_ARB_DEVICE_NAME);
		return ret;
	}

	fw_arb_class = class_create(FW_ARB_CLASS_NAME);
	if (IS_ERR(fw_arb_class)) {
		unregister_chrdev(major_number, FW_ARB_DEVICE_NAME);
		pr_err("Fail to create class\n");
		return PTR_ERR(fw_arb_class);
	}

	fw_arb_device = device_create(fw_arb_class, NULL, dev_data->devno,
									NULL, FW_ARB_DEVICE_NAME);
	if (IS_ERR(fw_arb_device)) {
		class_destroy(fw_arb_class);
		unregister_chrdev(major_number, FW_ARB_DEVICE_NAME);
		pr_err("Fail to create class devices\n");
		return PTR_ERR(fw_arb_device);
	}
	platform_set_drvdata(pdev, dev_data);

	return 0;
}

static void fw_antirollback_driver_remove(struct platform_device *pdev)
{
	struct fw_device_data *dev_data = platform_get_drvdata(pdev);

	device_destroy(fw_arb_class, dev_data->devno);
	class_destroy(fw_arb_class);
	cdev_del(&dev_data->cdev);
	unregister_chrdev(major_number, FW_ARB_DEVICE_NAME);
}

static const struct of_device_id fw_antirollback_driver_of_match[] = {
	{ .compatible = "amlogic, fw_antirollback" },
	{ },
};
MODULE_DEVICE_TABLE(of, fw_antirollback_driver_of_match);

static struct platform_driver fw_antirollback_driver = {
	.probe = fw_antirollback_driver_probe,
	.remove = fw_antirollback_driver_remove,
	.driver = {
		.name = FW_ARB_DRIVER_NAME,
		.of_match_table = fw_antirollback_driver_of_match,
		.owner = THIS_MODULE,
	},
};

module_platform_driver(fw_antirollback_driver)

MODULE_DESCRIPTION("FW antirollback check and update function");
MODULE_VERSION("1.0");
