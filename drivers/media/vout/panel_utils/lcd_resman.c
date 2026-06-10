// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/of_device.h>
#include <linux/mm.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#endif
#include <linux/amlogic/media/vout/panel_utils/lcd_resman.h>
#include <linux/amlogic/aml_mbox.h>
#include "panel_utils.h"

#define LCD_RESMAN_DEV_NAME "lcd_resman"
#define LCD_RESMAN_CLASS_NAME "lcd_resman"

#define LCD_RESMAN_DEVICE_NEED 0

struct lcd_resman_dev_s {
	dev_t           devt;
	dev_t		devno;
	struct cdev     cdev;
	struct device   *dev;
	struct class	*clsp;
};

static struct lcd_resman_dev_s *lcd_resman_dev;

static int lcd_resman_parse_param(char *buf_orig, char **parm, int max_parm)
{
	char *ps, *token;
	char str[3] = {' ', '\n', '\0'};
	unsigned int n = 0;

	ps = buf_orig;
	while (n < max_parm) {
		token = strsep(&ps, str);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	return n;
}

static ssize_t lcd_resman_debug_store(const struct class *cla,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
#define __MAX_PARAM 16
	char *buf_orig;
	char **parm = NULL;
	unsigned char *p;
	char *str;
	int size = 0, i, j, n;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return count;

	parm = kcalloc(__MAX_PARAM, sizeof(char *), GFP_KERNEL);
	if (!parm) {
		kfree(buf_orig);
		return count;
	}

	lcd_resman_parse_param(buf_orig, parm, __MAX_PARAM);

	if (strcmp(parm[0], "memory") == 0) {
		lrm_show();
	} else if (strcmp(parm[0], "bootargs") == 0) {
		if (!parm[1])
			goto lcd_resman_debug_bootargs_next;

		p = lrm_bootargs_get_data(parm[1], &size);
		if (!p)
			goto lcd_resman_debug_bootargs_next;
		str = kzalloc(64, GFP_KERNEL);
		if (str) {
			for (i = 0; i < size; i += 16) {
				n = 0;
				for (j = 0; j < 16; j++) {
					if ((i + j) >= size)
						break;
					n += sprintf(str + n, " %02x", p[i + j]);
				}
				pr_info("%04x:%s\n", i, str);
			}
			pr_info("\n");
			kfree(str);
		}

lcd_resman_debug_bootargs_next:
		lrm_bootargs_dump();
	} else if (strcmp(parm[0], "mbox") == 0) {
		pr_info("todo");
	}

	kfree(parm);
	kfree(buf_orig);
	return count;

#undef __MAX_PARAM
}

static const char *lcd_resman_dbg_str = {
	"usage:\n"
	"echo memory > /sys/class/lcd_resman/debug\n"
	"echo bootargs [name] > /sys/class/lcd_resman/debug\n"
};

static ssize_t lcd_resman_debug_show(const struct class *cla, const struct class_attribute *attr,
			     char *buf)
{
	return sprintf(buf, "%s\n", lcd_resman_dbg_str);
}

static struct class_attribute lcd_resman_class_attrs[] = {
	__ATTR(debug,    0644, lcd_resman_debug_show, lcd_resman_debug_store),
	__ATTR_NULL
};

#if (LCD_RESMAN_DEVICE_NEED)
static const struct file_operations lcd_resman_fops = {
	.owner   = THIS_MODULE,
	.open    = NULL,
	.release = NULL,
	.unlocked_ioctl   = NULL,
#ifdef CONFIG_COMPAT
	.compat_ioctl = NULL,
#endif
};
#endif

static int lcd_resman_probe(struct platform_device *pdev)
{
	int ret = 0, i = 0;
	struct lcd_resman_dev_s *devp;

	lcd_resman_dev = kzalloc(sizeof(*lcd_resman_dev), GFP_KERNEL);
	if (!lcd_resman_dev)
		return -1;
	devp = lcd_resman_dev;

	ret = alloc_chrdev_region(&devp->devno, 0, 1, LCD_RESMAN_DEV_NAME);
	if (ret < 0)
		return ret;

	devp->clsp = class_create(LCD_RESMAN_CLASS_NAME);
	if (IS_ERR(devp->clsp)) {
		ret = PTR_ERR(devp->clsp);
		goto fail_create_class;
	}

	for (i = 0; lcd_resman_class_attrs[i].attr.name; i++)
		if (class_create_file(devp->clsp, &lcd_resman_class_attrs[i]) < 0)
			LRMPR("fail to create: %s", lcd_resman_class_attrs[i].attr.name);

#if (LCD_RESMAN_DEVICE_NEED)
	cdev_init(&devp->cdev, &lcd_resman_fops);
	devp->cdev.owner = THIS_MODULE;
	ret = cdev_add(&devp->cdev, devp->devno, 1);
	if (ret)
		goto fail_add_cdev;

	devp->dev = device_create(devp->clsp, NULL, devp->devno, NULL, LCD_RESMAN_DEV_NAME);
	if (IS_ERR(devp->dev)) {
		ret = PTR_ERR(devp->dev);
		goto fail_create_device;
	}
#endif

	lcd_reserved_memory_init(pdev);
	return 0;

#if (LCD_RESMAN_DEVICE_NEED)
fail_create_device:
	LRMERR("%s device create error\n", __func__);
	cdev_del(&devp->cdev);

fail_add_cdev:
	for (i = 0; lcd_resman_class_attrs[i].attr.name; i++)
		class_remove_file(devp->clsp, &lcd_resman_class_attrs[i]);

	class_destroy(devp->clsp);
	pr_info("[amvecm.] : amvecm add device error.\n");
#endif
fail_create_class:
	LRMERR("%s class create error\n", __func__);
	unregister_chrdev_region(devp->devno, 1);
	return ret;
}

static void lcd_resman_remove(struct platform_device *pdev)
{
	int i = 0;
	struct lcd_resman_dev_s *devp = lcd_resman_dev;

	if (!devp)
		return;

#if (LCD_RESMAN_DEVICE_NEED)
	device_destroy(devp->clsp, devp->devno);
	cdev_del(&devp->cdev);
#endif
	for (i = 0; lcd_resman_class_attrs[i].attr.name; i++)
		class_remove_file(devp->clsp, &lcd_resman_class_attrs[i]);
	class_destroy(devp->clsp);
	unregister_chrdev_region(devp->devno, 1);
	kfree(devp);
	lcd_resman_dev = NULL;
}

static const struct of_device_id lcd_resman_match_table[] = {
	{
		.compatible = "amlogic, lcd-resman",
		.data = NULL,
	},
	{}
};

static struct platform_driver lcd_resman_platform_driver = {
	.probe = lcd_resman_probe,
	.remove = lcd_resman_remove,
	.suspend = NULL,
	.resume = NULL,
	.shutdown = NULL,
	.driver = {
		.name = "amlogic, lcd_resman",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(lcd_resman_match_table),
#endif
	},
};

int __init lcd_resman_init(void)
{
	if (platform_driver_register(&lcd_resman_platform_driver)) {
		LRMERR("failed to register lcd resman driver module\n");
		return -ENODEV;
	}

	return 0;
}

void __exit lcd_resman_exit(void)
{
	platform_driver_unregister(&lcd_resman_platform_driver);
}
