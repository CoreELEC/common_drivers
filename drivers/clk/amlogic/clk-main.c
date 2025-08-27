// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/amlogic/module_merge.h>

#include "clk-basic.h"
#include "clk-dualdiv-mux.h"
#include "clk-generals.h"
#include "clk-noglitch.h"
#include "clk-pll.h"

static int __init clk_main_init(void)
{
	call_sub_init(clk_basic_init);
	call_sub_init(clk_dualdiv_mux_init);
	call_sub_init(clk_general_init);
	call_sub_init(clk_noglitch_init);
	call_sub_init(clk_pll_init);

	return 0;
}

static void __exit clk_main_exit(void)
{
	clk_basic_exit();
	clk_dualdiv_mux_exit();
	clk_general_exit();
	clk_noglitch_exit();
	clk_pll_exit();
}

module_init(clk_main_init);
module_exit(clk_main_exit);

MODULE_LICENSE("GPL v2");
