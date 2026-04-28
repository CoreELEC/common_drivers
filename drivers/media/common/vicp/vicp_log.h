/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _VICP_LOG_H_
#define _VICP_LOG_H_

#include <linux/printk.h>

#define VICP_ERROR		    0X0
#define VICP_INFO		    0X1
#define VICP_AFBCD		    0X2
#define VICP_RDMIF		    0X4
#define VICP_SCALER		    0X8
#define VICP_HDR2		    0X10
#define VICP_CROP		    0X20
#define VICP_SHRINK		    0X40
#define VICP_WRMIF		    0X80
#define VICP_AFBCE		    0X100
#define VICP_RDMA		    0X200
#define VICP_FGRAIN		    0X400
#define VICP_DUMP_REG		    0X800
#define VICP_DEBUG		    0X1000

int vicp_print(int debug_flag, const char *fmt, ...);
#endif
