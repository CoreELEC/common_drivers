// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "main.h"

static int __init global_timer_main_init(void)
{
	pr_debug("### %s() start\n", __func__);
	glb_module_init(core);
	glb_module_init(isp);
	glb_module_init(input);
	glb_module_init(output);
	pr_debug("### %s() end\n", __func__);
	return 0;
}

static void __exit global_timer_main_exit(void)
{
	glb_module_exit(isp);
	glb_module_exit(input);
	glb_module_exit(output);
	glb_module_exit(core);
}

module_init(global_timer_main_init);
module_exit(global_timer_main_exit);
MODULE_LICENSE("GPL v2");
