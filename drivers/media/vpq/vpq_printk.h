/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPQ_PRINTK_H__
#define __VPQ_PRINTK_H__

#include <linux/kernel.h>

extern unsigned int log_lev;

#define lev_dbg      (1) // log level of debug cmd layer
#define lev_ioc      (2) // log level of ioctl layer
#define lev_tab      (3) // log level of table logic layer
#define lev_mod      (4) // log level of modules layer
#define lev_vfm      (5) // log level of vfm layer
#define lev_proc     (6) // log level of processor layer

#define vpq_pr_dbg(lev, fmt, args...)\
do {\
	if ((lev) <= log_lev)\
		pr_debug("vpq: " fmt, ##args);\
} while (0)

#define vpq_pr_err(fmt, args ...)   pr_err("vpq: " fmt, ##args)
#define vpq_pr_info(fmt, args ...)  pr_info("vpq: " fmt, ##args)

#endif // __VPQ_PRINTK_H__
