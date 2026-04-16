// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/amlogic/module_merge.h>
#include "main.h"

static int __init secmon_main_init(void)
{
	pr_debug("### %s() start\n", __func__);
	call_sub_init(meson_secmon_init);
	call_sub_init(dolby_fw_init);
	pr_debug("### %s() end\n", __func__);

	return 0;
}

static void __exit secmon_main_exit(void)
{
	dolby_fw_exit();
}

#if IS_ENABLED(CONFIG_AMLOGIC_LINUX_S_OPTIMIZE)
subsys_initcall_sync(secmon_main_init);
#else
module_init(secmon_main_init);
#endif
module_exit(secmon_main_exit);
MODULE_LICENSE("GPL v2");
