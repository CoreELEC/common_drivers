// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "main.h"

static int __init gpio_main_init(void)
{
	pr_debug("### %s() start\n", __func__);
	call_sub_init(gpiolib_module_init);
	call_sub_init(gpiolib_sysfs_module_init);
	call_sub_init(meson_gpio_irq_init);
	call_sub_init(meson_pmic6b_gpio_init);
	pr_debug("### %s() end\n", __func__);
	return 0;
}

static void __exit gpio_main_exit(void)
{
	meson_gpio_irq_exit();
	gpiolib_sysfs_module_exit();
	gpiolib_module_exit();
	meson_pmic6b_gpio_exit();
}
#ifndef MODULE
arch_initcall(gpio_main_init);
#else
module_init(gpio_main_init);
#endif
module_exit(gpio_main_exit);

MODULE_LICENSE("GPL v2");
