/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PANEL_UTILS_PANEL_COMMON_H_
#define _PANEL_UTILS_PANEL_COMMON_H_

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

#define PANEL_INIT_LEVEL_NORMAL         0
#define PANEL_INIT_LEVEL_PWR_OFF        1
#define PANEL_INIT_LEVEL_KERNEL_ON      2
#define PANEL_INIT_LEVEL_KERNEL_OFF     3
#define PANEL_INIT_LEVEL_PREBOOT        4  // lcd on, bl off

void set_panel_init_level(u8 index, u8 init_level);
u8 get_panel_init_level(u8 index);

u8 *panel_init_table_load_array(char *name, u8 cmd_size,
					 u32 *buf, int buf_len, int tbl_max, int *tbl_cnt);

#endif
