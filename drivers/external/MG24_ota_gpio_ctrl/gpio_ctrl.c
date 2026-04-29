// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

//#define pr_fmt(fmt)	"gpio_ctrl: " fmt

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/gpio/driver.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/slab.h>

#include "gpio_ctrl.h"

#define GPIO_CTRL_DRIVER_NAME	"gpio_ctl"

#define GPIO_CTRL_DEBUG_ENABLE

#ifdef GPIO_CTRL_DEBUG_ENABLE
//#define GPIO_CTRL_DEBUG(format, ...) printk("%s() -- %d " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define GPIO_CTRL_DEBUG(format, ...) pr_info("%s() -- %d " format, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#elif
#define GPIO_CTRL_DEBUG(format, ...)
#endif
static ssize_t gpio_ctrl_value_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_ctrl_dev *ctrl_dev = platform_get_drvdata(pdev);
	int value;
	GPIO_CTRL_DEBUG("Enter");

	value = gpiod_get_value(ctrl_dev->gpio_ctrl);

	GPIO_CTRL_DEBUG("ctrl gpio level: %d\n", value);
	return sprintf(buf, "%d\n", value);
}

static ssize_t gpio_ctrl_value_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct gpio_ctrl_dev *ctrl_dev = platform_get_drvdata(pdev);
	int value;
	GPIO_CTRL_DEBUG("Enter");

	if (kstrtoint(buf, 0, &value))
		return -EINVAL;

	gpiod_set_value(ctrl_dev->gpio_ctrl, value);
	GPIO_CTRL_DEBUG("ctrl gpio set level: %d\n", value);

	return count;
}

static DEVICE_ATTR(value, 0664, gpio_ctrl_value_show, gpio_ctrl_value_store);

static struct attribute *gpio_ctrl_attrs[] = {
	&dev_attr_value.attr,
	NULL,
};

static const struct attribute_group gpio_ctrl_group = {
	.attrs = gpio_ctrl_attrs,
};

static int gpio_ctrl_dt_parse(struct platform_device *pdev)
{
	struct device_node *node;
	struct gpio_ctrl_dev *ctrl_dev;
	int ret = 0;

	GPIO_CTRL_DEBUG("Enter\n");

	ctrl_dev = platform_get_drvdata(pdev);
	if (ctrl_dev == NULL)
		pr_info("ctrl_dev is null");

	node = pdev->dev.of_node;
	if (!node) {
		pr_err("failed to find node for %s\n", GPIO_CTRL_DRIVER_NAME);
		return -ENODEV;
	}

	ctrl_dev->gpio_ctrl = devm_gpiod_get(&pdev->dev, "ctrl", GPIOD_OUT_HIGH);
	if (IS_ERR(ctrl_dev->gpio_ctrl)) {
		pr_err("ctrl is not valid\n");
	} else {
		//gpiod_direction_output(ctrl_dev->typec_5v12v_en->gpio_ctrl, 0);
		pr_info("ctrl is true\n");
	}

	ret = of_property_read_u32(node, "mode", &ctrl_dev->mode);
	if (ret) {
		ctrl_dev->mode = 0;
		pr_info("%s] not config mode, default: %d\n", __func__, ctrl_dev->mode);
	} else {
		pr_info("...%s...config mode = %d\n", __func__, ctrl_dev->mode);
	}

	GPIO_CTRL_DEBUG("Exit\n");
	return 0;
}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>

static void gpio_ctrl_device_early_suspend(struct early_suspend *h)
{
	struct gpio_ctrl_dev *ctrl_dev = (struct gpio_ctrl_dev *)h->param;
	GPIO_CTRL_DEBUG("Enter\n");

	if (ctrl_dev == NULL) {
		pr_err("ctrl_dev is null\n");
		return;
	}
}

static void gpio_ctrl_device_late_resume(struct early_suspend *h)
{
	struct gpio_ctrl_dev *ctrl_dev = (struct gpio_ctrl_dev *)h->param;
	GPIO_CTRL_DEBUG("Enter\n");

	if (ctrl_dev == NULL) {
		pr_err("ctrl_dev is null\n");
		return;
	}
}

#endif

static int gpio_ctrl_device_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct gpio_ctrl_dev *ctrl_dev = platform_get_drvdata(pdev);
	GPIO_CTRL_DEBUG("gpio_ctrl_device true suspend\n");

	if (ctrl_dev == NULL) {
		pr_err("ctrl_dev is null\n");
	}

	return 0;
}

static int gpio_ctrl_device_resume(struct platform_device *pdev)
{
	struct gpio_ctrl_dev *ctrl_dev = platform_get_drvdata(pdev);
	GPIO_CTRL_DEBUG("gpio_ctrl_device true resume\n");

	if (ctrl_dev == NULL) {
		pr_err("ctrl_dev is null\n");
	}

	return 0;
}

static int gpio_ctrl_device_probe(struct platform_device *pdev)
{
	struct gpio_ctrl_dev *ctl_dev = NULL;
	int ret;
	GPIO_CTRL_DEBUG("module probe start\n");

	ctl_dev = devm_kzalloc(&pdev->dev, sizeof(struct gpio_ctrl_dev), GFP_KERNEL);
	if (ctl_dev == NULL) {
		pr_err("failed to allocate memory for device\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, ctl_dev);

	ret = gpio_ctrl_dt_parse(pdev);
	if (ret)
		return ret;

	ret = sysfs_create_group(&pdev->dev.kobj, &gpio_ctrl_group);
	if (ret) {
		pr_err("Failed to create sysfs group\n");
		return ret;
	}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	//gpio_ctrl_device_early_suspend_handler.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN - 10;
	ctl_dev->gpio_ctrl_device_early_suspend_handler.suspend = gpio_ctrl_device_early_suspend;
	ctl_dev->gpio_ctrl_device_early_suspend_handler.resume = gpio_ctrl_device_late_resume;
	ctl_dev->gpio_ctrl_device_early_suspend_handler.param = ctl_dev;
	register_early_suspend(&ctl_dev->gpio_ctrl_device_early_suspend_handler);
#endif

	GPIO_CTRL_DEBUG("module probed ok\n");
	return 0;
}

static void  gpio_ctrl_device_remove(struct platform_device *pdev)
{
	GPIO_CTRL_DEBUG("module removed ok\n");
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	struct gpio_ctrl_dev *ctl_dev = platform_get_drvdata(pdev);
	unregister_early_suspend(&ctl_dev->gpio_ctrl_device_early_suspend_handler);
#endif
	sysfs_remove_group(&pdev->dev.kobj, &gpio_ctrl_group);
	return;
}

static const struct of_device_id gpio_ctrl_device_dt_match[] = {
	{ .compatible = "sei, gpio_ctrl", },
	{},
};

static struct platform_driver gpio_ctrl_dev = {
	.driver = {
		.name = GPIO_CTRL_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = gpio_ctrl_device_dt_match,
	},
	.probe = gpio_ctrl_device_probe,
	.remove = gpio_ctrl_device_remove,
	.suspend = gpio_ctrl_device_suspend,
	.resume = gpio_ctrl_device_resume,
};

static int __init gpio_ctrl_init(void)
{
	GPIO_CTRL_DEBUG("module init\n");

	if (platform_driver_register(&gpio_ctrl_dev)) {
		GPIO_CTRL_DEBUG("failed to register driver\n");
		return -ENODEV;
	}

	return 0;
}

static void gpio_ctrl_exit(void)
{
	GPIO_CTRL_DEBUG("module exit\n");

	platform_driver_unregister(&gpio_ctrl_dev);
}

module_init(gpio_ctrl_init);
module_exit(gpio_ctrl_exit);

MODULE_DESCRIPTION("common control driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("sei, Inc.");

