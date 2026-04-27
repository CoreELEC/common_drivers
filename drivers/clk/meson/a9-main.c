// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "a9.h"

static int __init a9_main_init(void)
{
	call_sub_init(a9_clk_init);
	call_sub_init(a9_aoclk_init);

	return 0;
}

static void __exit a9_main_exit(void)
{
	a9_aoclk_exit();
	a9_clk_exit();
}

module_init(a9_main_init);
module_exit(a9_main_exit);

MODULE_LICENSE("GPL v2");
