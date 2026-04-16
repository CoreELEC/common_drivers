// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "c4.h"

static int __init c4_main_init(void)
{
	call_sub_init(c4_clk_init);
	call_sub_init(c4_aoclk_init);

	return 0;
}

static void __exit c4_main_exit(void)
{
	c4_aoclk_exit();
	c4_clk_exit();
}

#if IS_ENABLED(CONFIG_AMLOGIC_LINUX_S_OPTIMIZE)
subsys_initcall_sync(c4_main_init);
#else
module_init(c4_main_init);
#endif
module_exit(c4_main_exit);

MODULE_LICENSE("GPL v2");
