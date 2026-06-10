/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PANEL_UTILS_H_
#define _PANEL_UTILS_H_

#define PANEL_UTILS_DBG_PR_NORMAL   BIT(0)
#define PANEL_UTILS_DBG_PR_MEM      BIT(1)
#define PANEL_UTILS_DBG_PR_MODEL    BIT(2)

extern u8 panel_debug_print_flag;

#define PANEL_PR(fmt, args...)     \
	do { \
		if (panel_debug_print_flag & PANEL_UTILS_DBG_PR_NORMAL) \
			pr_info("[Panel] " fmt "\n", ## args); \
	} while (0)

#define PANEL_ERR(fmt, args...)    pr_err("[Panel](err) " fmt "\n", ## args)

#define LRMPR(fmt, args...)     pr_info("[Panel-Lrm] " fmt "\n", ## args)
#define LRMERR(fmt, args...)    pr_err("[Panel-Lrm](err) " fmt "\n", ## args)

#define MODEL_PR(fmt, args...)     pr_info("[Panel-Model] " fmt "\n", ## args)
#define MODEL_ERR(fmt, args...)     pr_err("[Panel-Model](err) " fmt "\n", ## args)

#endif
