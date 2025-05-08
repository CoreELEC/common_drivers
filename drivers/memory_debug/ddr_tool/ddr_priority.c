// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include "ddr_port.h"
#include "ddr_bandwidth.h"

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
static struct ddr_priority_v1 ddr_priority_s4[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 5, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0		},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x98 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x98 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xac << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xac << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 16, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 17, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x64 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x64 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 18, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x68 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x68 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 19, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 20, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x70 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x70 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 21, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 23, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x7c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x7c << 2), .r_bit_s = 16, .r_width = 0x7	}
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct ddr_priority_v1 ddr_priority_t7[] __initdata = {
	{ .port_id = 1, .reg_base = 0xfe704100,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe703100,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 3, .reg_base = 0xfe701100,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe702100,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0xf	},

	//{ .port_id = 8, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .force_enable = 0x1000000,
	//	.w_offset = 0, .w_bit_s = 28, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	//{ .port_id = 9, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .force_enable = 0x10000,
	//	.w_offset = 0, .w_bit_s = 20, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 20, .r_width = 0xf	},

	//{ .port_id = 10, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .force_enable = 0x100,
	//	.w_offset = 0, .w_bit_s = 12, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 12, .r_width = 0xf	},

	//{ .port_id = 11, .reg_base = 0xfe374014,
	//	.reg_mode = 1, .force_enable = 0x1,
	//	.w_offset = 0, .w_bit_s = 4, .w_width = 0xf,
	//	.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 17, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 18, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 19, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 24, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 25, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 26, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 27, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 28, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 29, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 34, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 35, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 36, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 37, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 38, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 39, .reg_base = 0xfe0104d4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 48, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 49, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 50, .reg_base = 0xfe0104d8,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x7,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x7	},

	{ .port_id = 51, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0x7,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0x7	},

	{ .port_id = 52, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 53, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 54, .reg_base = 0xfe0104dc,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 55, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 63, .reg_base = 0xfe0104e4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0x3	},

	{ .port_id = 64, .reg_base = 0xfe3b3000,
		.reg_mode = 0, .force_enable = 0x10000,
		.w_offset = 0xac4, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0xac0, .r_bit_s = 0, .r_width = 0xf},

	{ .port_id = 65, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 66, .reg_base = 0xfe3b5000,
		.reg_mode = 0, .force_enable = 0x10000,
		.w_offset = 0xac4, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0xac0, .r_bit_s = 0, .r_width = 0xf},

	{ .port_id = 67, .reg_base = 0xfe3b3000,
		.reg_mode = 0, .force_enable = 0x10000,
		.w_offset = 0xec4, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0xec0, .r_bit_s = 0, .r_width = 0xf},

	{ .port_id = 68, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 16, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 69, .reg_base = 0xfe0104e0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 24, .w_width = 0xf,
		.r_offset = 0, .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 72, .reg_base = 0xfe0104e4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 73, .reg_base = 0xfe0104e4,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 8, .w_width = 0x3,
		.r_offset = 0, .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 80, .reg_base = 0xff009000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0xcc8, .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 81, .reg_base = 0xff009000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0xcc8, .w_bit_s = 16, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 82, .reg_base = 0xff009000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0xcc4, .r_bit_s = 0, .r_width = 0xffff}
};

static struct ddr_priority_v1 ddr_priority_s5[] __initdata = {
	{ .port_id = 2, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x130 << 2), .w_bit_s = 20, .w_width = 0xf0f,
		.r_offset = (0x130 << 2), .r_bit_s = 16, .r_width = 0xf0f	},

	{ .port_id = 4, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x130 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x130 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x15b << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x15b << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x15b << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x15b << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0		},

	{ .port_id = 24, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 25, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 26, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 28, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 29, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 33, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = (0x139 << 2), .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 35, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 22, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 22, .r_width = 0xf	},

	{ .port_id = 36, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 37, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 4, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 38, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 54, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 56, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 57, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 4, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 58, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 59, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 60, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 64, .reg_base = 0xff809000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0xcc8, .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 66, .reg_base = 0xff809000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0xcc8, .w_bit_s = 16, .w_width = 0xffff,
		.r_offset = 0xcc0, .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 68, .reg_base = 0xff809000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0xcc4, .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 70, .reg_base = 0xff809000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0xcc4, .r_bit_s = 16, .r_width = 0xffff},

	{ .port_id = 72, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 9, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 6, .r_width = 0x7	},

	{ .port_id = 80, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 15, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 12, .r_width = 0x7	},

	{ .port_id = 81, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 21, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 18, .r_width = 0x7	},

	{ .port_id = 88, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 27, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 24, .r_width = 0x7	},

	{ .port_id = 96, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 3, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 0, .r_width = 0x7	},

	{ .port_id = 104, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x133 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x133 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 106, .reg_base = 0xfe03e000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0x4c4, .w_bit_s = 16, .w_width = 0xffff,
		.r_offset = 0x4c8, .r_bit_s = 0, .r_width = 0xfffff	},

	{ .port_id = 108, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x133 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x133 << 2), .r_bit_s = 28, .r_width = 0xf	},
};

static struct ddr_priority_v1 ddr_priority_s6[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x88 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x88 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 3, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x98 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x98 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0xa4 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa4 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0xac << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xac << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 12, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0xb0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 13, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0xb4 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb4 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 14, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0xb8 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb8 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 15, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0xbc << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xbc << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 17, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x64 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x64 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 18, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x68 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x68 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 19, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = BIT(15),
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0xf	},

};

static struct ddr_priority_v1 ddr_priority_c3[] __initdata = {
	{ .port_id = 2, .reg_base = 0xffe42000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 8, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 10, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0xff010800,
		.reg_mode = 1, .force_enable = 0x10000,
		.w_offset = (0xb1 << 2), .w_bit_s = 0, .w_width = 0xffff,
		.r_offset = (0xb0 << 2), .r_bit_s = 0, .r_width = 0xffff},

	{ .port_id = 17, .reg_base = 0xff010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x05 << 2), .w_bit_s = 8, .w_width = 0xff,
		.r_offset = (0x05 << 2), .r_bit_s = 0, .r_width = 0xff	},

	{ .port_id = 18, .reg_base = 0xff010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x05 << 2), .w_bit_s = 16, .w_width = 0xff,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0		},

	{ .port_id = 19, .reg_base = 0xfe350010,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 4, .r_width = 0xff	},

	{ .port_id = 40, .reg_base = 0xfd342000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 41, .reg_base = 0xfd343000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 42, .reg_base = 0xfd344000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 43, .reg_base = 0xfd346000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 44, .reg_base = 0xfd345000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 48, .reg_base = 0xffe48000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 72, .reg_base = 0xffe44000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 73, .reg_base = 0xffe44000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 74, .reg_base = 0xffe44000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 80, .reg_base = 0xffe47000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 81, .reg_base = 0xffe45000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 82, .reg_base = 0xffe4a000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 83, .reg_base = 0xffe46000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 85, .reg_base = 0xffe49000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = 0x104, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf	},
};

static struct ddr_priority_v1 ddr_priority_t5m[] __initdata = {
	{ .port_id = 2, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0xf		},

	{ .port_id = 4, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x135 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 24, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 17, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 24, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 18, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 24, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 19, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 24, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 32, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 4, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 6, .r_width = 0x3	},

	{ .port_id = 33, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 4, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 6, .r_width = 0x3	},

	{ .port_id = 36, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x134 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x134 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 37, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x134 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x134 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 38, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 20, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 24, .r_width = 0xf	},

	{ .port_id = 54, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 2, .r_width = 0x3	},

	{ .port_id = 56, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 57, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 58, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 59, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 60, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 0, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 61, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 62, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 10, .r_width = 0x3	},

	{ .port_id = 64, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 12, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 14, .r_width = 0x3	},

	{ .port_id = 66, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 18, .r_width = 0x3	},

	{ .port_id = 68, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 70, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 72, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 74, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x137 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x137 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 82, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 0, .w_width = 0x3,
		.r_offset = (0x138 << 2), .r_bit_s = 4, .r_width = 0x3	},

	{ .port_id = 90, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x137 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x137 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 98, .reg_base = 0xfe010000,
		.reg_mode = 1, .force_enable = 0,
		.w_offset = (0x137 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x137 << 2), .r_bit_s = 4, .r_width = 0xf	},
};

static struct ddr_priority_v1 ddr_priority_txhd2[] __initdata = {
	{ .port_id = 0, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 1, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 2, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x88 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x88 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 8, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 16, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 17, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x64 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x64 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 18, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x68 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x68 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 19, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 20, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x70 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x70 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 21, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 23, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x7c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x7c << 2), .r_bit_s = 16, .r_width = 0x7	},
};

static struct ddr_priority_v1 ddr_priority_t3x[] __initdata = {
	{ .port_id = 2, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x131 << 2), .w_bit_s = 20, .w_width = 0xf0f,
		.r_offset = (0x131 << 2), .r_bit_s = 16, .r_width = 0xf0f},

	{ .port_id = 4, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x131 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x131 << 2), .r_bit_s =  8, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x15b << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x15b << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x15b << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x15b << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 16, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 19, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 21, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 24, .r_width = 0xf	},

	{ .port_id = 22, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 28, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 23, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 4, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 32, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 20, .w_width = 0x7,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0x7	},

	{ .port_id = 33, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 35, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 28, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 36, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 37, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 4, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 38, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 54, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 56, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 57, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 0, .r_width = 0xf	},

	{ .port_id = 58, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 8, .r_width = 0xf	},

	{ .port_id = 59, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 12, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 60, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 61, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 62, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 63, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 20, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 64, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 66, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 68, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 70, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 72, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 9, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 6, .r_width = 0x7	},

	{ .port_id = 80, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 15, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 12, .r_width = 0x7	},

	{ .port_id = 81, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 21, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 18, .r_width = 0x7	},

	{ .port_id = 88, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 27, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 24, .r_width = 0x7	},

	{ .port_id = 96, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x158 << 2), .w_bit_s = 3, .w_width = 0x7,
		.r_offset = (0x158 << 2), .r_bit_s = 0, .r_width = 0x7	},

	{ .port_id = 104, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x0133 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x0133 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 106, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x0133 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x0133 << 2), .r_bit_s = 28, .r_width = 0xf},

	{ .port_id = 116, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x0133 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x0133 << 2), .r_bit_s = 12, .r_width = 0xf},

	{ .port_id = 117, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 119, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 121, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 123, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},
};

static struct ddr_priority_v1 ddr_priority_t3[] __initdata = {
	{ .port_id = 2, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x14c << 2), .w_bit_s = 21, .w_width = 0x5,
		.r_offset = (0x14c << 2), .r_bit_s = 20, .r_width = 0x5	},

	{ .port_id = 16, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x135 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 17, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 18, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 19, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x135 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 20, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 27, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 28, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 34, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 34, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 35, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 36, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 37, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 38, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 39, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 48, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x137 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x137 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 51, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 0, .w_width = 0x7,
		.r_offset = (0x138 << 2), .r_bit_s = 4, .r_width = 0x7	},

	{ .port_id = 52, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x137 << 2), .w_bit_s = 24, .w_width = 0x3,
		.r_offset = (0x137 << 2), .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 53, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x137 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x137 << 2), .r_bit_s = 20, .r_width = 0x3	},

	{ .port_id = 54, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x137 << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x137 << 2), .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 63, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 4, .r_width = 0x3	},

	{ .port_id = 68, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x136 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 70, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x138 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 72, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 73, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x139 << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 12, .r_width = 0x3	},

	{ .port_id = 80, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 81, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 82, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 83, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 84, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},

	{ .port_id = 85, .reg_base = 0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0	},
};

static struct ddr_priority_v1 ddr_priority_a4[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x80  << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x80  << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x88  << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x88  << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x90  << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x90  << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x98  << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x98  << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x9c  << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x9c  << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 40, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x69  << 2), .w_bit_s = 0, .w_width = 0x3,
		.r_offset = (0x69  << 2), .r_bit_s = 2, .r_width = 0x3	},

	{ .port_id = 41, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x69  << 2), .w_bit_s = 12, .w_width = 0x1,
		.r_offset = (0x69  << 2), .r_bit_s = 13, .r_width = 0x1	},

	{ .port_id = 42, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x69  << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x69  << 2), .r_bit_s = 10, .r_width = 0x3	},

	{ .port_id = 43, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x69  << 2), .w_bit_s = 14, .w_width = 0x3,
		.r_offset = (0x69  << 2), .r_bit_s = 16, .r_width = 0x3	},

	{ .port_id = 45, .reg_base = 0xfe010000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x69  << 2), .w_bit_s = 18, .w_width = 0x3,
		.r_offset = (0x69  << 2), .r_bit_s = 20, .r_width = 0x3	},
};

static struct ddr_priority_v1 ddr_priority_s7[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x88 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x88 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x98 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x98 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xac << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xac << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 10, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xb4 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb4 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xb8 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb8 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 12, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xbc << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xbc << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 13, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xc0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xc0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 32, .reg_base = 0xffe46000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 33, .reg_base = 0xffe47000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 34, .reg_base = 0xffe48000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 39, .reg_base = 0xffe4b000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 36, .reg_base = 0xffe45000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 39, .reg_base = 0xffe4b000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 40, .reg_base = 0x0,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = 0x0, .w_bit_s = 0, .w_width = 0x0,
		.r_offset = 0x0, .r_bit_s = 0, .r_width = 0x0		},

	{ .port_id = 41, .reg_base = 0xffe49000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 42, .reg_base = 0xffe4a000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},

	{ .port_id = 43, .reg_base = 0xffe4b000,
		.reg_mode = 1, .force_enable = 0x0,
		.w_offset = 0x100, .w_bit_s = 0, .w_width = 0xf,
		.r_offset = 0x100, .r_bit_s = 0, .r_width = 0xf		},
};

static struct ddr_priority_v1 ddr_priority_s7d[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x85 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x85 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x8a << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8a << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x95 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x95 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa5 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa5 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xaa << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xaa << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xb0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 10, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xb5 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb5 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xba << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xba << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 12, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xc0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xc0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 13, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xc5 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xc5 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 14, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xca << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xca << 2), .r_bit_s = 16, .r_width = 0xf	},
};

static struct ddr_priority_v1 ddr_priority_sc2[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 2, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x88 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x88 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x98 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x98 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 8, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 9, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0xa4 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa4 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0xac << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xac << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 12, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0xb0 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xb0 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 16, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 17, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x64 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x64 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 18, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x68 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x68 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 19, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 20, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x70 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x70 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 21, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 22, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x78 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x78 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 23, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x7c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x7c << 2), .r_bit_s = 16, .r_width = 0x7	},
};

static struct ddr_priority_v1 ddr_priority_t5w[] __initdata = {
	{ .port_id = 0, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 1, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 6, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x98 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x98 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 8, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 9, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0xa4 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa4 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 10, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0xa8 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa8 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 16, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 17, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x64 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x64 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 18, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x68 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x68 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 19, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 20, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x70 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x70 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 21, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 23, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0x0,
		.w_offset = (0x7c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x7c << 2), .r_bit_s = 16, .r_width = 0x7	},
};

static struct ddr_priority_v1 ddr_priority_g12b[] __initdata = {
	{ .port_id = 0, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 1, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 2, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x84 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x84 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 5, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x94 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x94 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 6, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x98 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x98 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 8, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 9, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xa4 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa4 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 10, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xa8 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa8 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 11, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xac << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xac << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 12, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xae << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xae << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 13, .reg_base = 0x0,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = 0, .w_bit_s = 0, .w_width = 0,
		.r_offset = 0, .r_bit_s = 0, .r_width = 0		},

	{ .port_id = 16, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 17, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x64 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x64 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 18, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x68 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x68 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 19, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 20, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x70 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x70 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 21, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 22, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x78 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x78 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 23, .reg_base = 0xff638000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x7c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x7c << 2), .r_bit_s = 16, .r_width = 0x7	}
};

static struct ddr_priority_v1 ddr_priority_gxlx3[] __initdata = {
	{ .port_id = 0, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xb0 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xb0 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 1, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xba << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xba << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 2, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xc4 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xc4 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xce << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xce << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xd8 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xd8 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xf6 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xf6 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 8, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 9, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x6a << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6a << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 10, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 11, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x7e << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x7e << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 12, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x88 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x88 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 12, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x88 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x88 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 13, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x92 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x92 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 14, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 15, .reg_base = 0xc8838000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xa6 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xa6 << 2), .r_bit_s = 16, .r_width = 0x7	},
};

static struct ddr_priority_v1 ddr_priority_t6d[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x85 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x85 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x8a << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8a << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 3, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x95 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x95 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa5 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa5 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xaa << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xaa << 2), .r_bit_s = 16, .r_width = 0xf	},
};

static struct ddr_priority_v1 ddr_priority_t6w[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 1, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x85 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x85 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x8a << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8a << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 3, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x95 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x95 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 6, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 7, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa5 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa5 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xaa << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xaa << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 9, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xb0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 10, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xb5 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xb5 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 11, .reg_base = 0xfe340000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xba << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xba << 2), .r_bit_s = 16, .r_width = 0xf	},
};

static struct dmc_priority dmc_t6x[] __initdata = {
	{ .bus_id = 0, .reg_base = 0xfe036000, .offset = (0x458 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 1, .reg_base = 0xfe032000, .offset = (0x458 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 2, .reg_base = 0xfe030000, .offset = (0x458 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 3, .reg_base = 0xfe034000, .offset = (0x458 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 4, .reg_base = 0xfe034000, .offset = (0x45c << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 5, .reg_base = 0xfe034000, .offset = (0x460 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 6, .reg_base = 0xfe034000, .offset = (0x464 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 7, .reg_base = 0xfe034000, .offset = (0x480 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 8, .reg_base = 0xfe034000, .offset = (0x484 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},

	{ .bus_id = 9, .reg_base = 0xfe034000, .offset = (0x488 << 2),
	  .force_enable = 0x8000, .w_bit_s = 16, .r_bit_s = 16, .width = 0xf	},
};

static struct bus_dev_priority bus_dev_t6x[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 0,
		.w_offset = (0x135 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x135 << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 8, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 1,
		.w_offset = (0x139 << 2), .w_bit_s = 8, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 10, .r_width = 0x3	},

	{ .port_id = 16, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 2,
		.w_offset = (0x139 << 2), .w_bit_s = 16, .w_width = 0x3,
		.r_offset = (0x139 << 2), .r_bit_s = 18, .r_width = 0x3	},

	{ .port_id = 24, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 3,
		.w_offset = (0x135 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x135 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 25, .reg_base = 0x0, .bus_id = 4,			},

	{ .port_id = 26, .reg_base = 0xfe010000,
		.reg_mode = 0,  .bus_id = 5,
		.w_offset = (0x137 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x137 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 28, .reg_base = 0xfe010000,
		.reg_mode = 0,  .bus_id = 7,
		.w_offset = (0x138 << 2), .w_bit_s = 24, .w_width = 0x7,
		.r_offset = (0x138 << 2), .r_bit_s = 28, .r_width = 0x7	},

	{ .port_id = 29, .reg_base = 0x0, .bus_id = 8,			},

	{ .port_id = 30, .reg_base = 0x0, .bus_id = 9,			},

	{ .port_id = 32, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x138 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 34, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x138 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 35, .reg_base = 0x0, .bus_id = 6,			},

	{ .port_id = 36, .reg_base = 0x0, .bus_id = 6,			},

	{ .port_id = 37, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x139 << 2), .w_bit_s = 20, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 24, .r_width = 0xf,
		.num = 1, .share_dev = { 53 }, },

	{ .port_id = 38, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x136 << 2), .w_bit_s = 24, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 28, .r_width = 0xf	},

	{ .port_id = 39, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x139 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x139 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 40, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x135 << 2), .w_bit_s = 0, .w_width = 0x3,
		.r_offset = 0x0, .r_bit_s = 0, .r_width = 0x0		},

	{ .port_id = 41, .reg_base = 0x0, .bus_id = 6,			},

	{ .port_id = 42, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x136 << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 43, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x136 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 44, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x136 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x136 << 2), .r_bit_s = 20, .r_width = 0xf	},

	{ .port_id = 45, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = 0x0, .w_bit_s = 0, .w_width = 0x0,
		.r_offset = (0x135 << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 46, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x135 << 2), .w_bit_s = 24, .w_width = 0x3,
		.r_offset = (0x135 << 2), .r_bit_s = 28, .r_width = 0x3	},

	{ .port_id = 47, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x13b << 2), .w_bit_s = 0, .w_width = 0xf,
		.r_offset = (0x13b << 2), .r_bit_s = 4, .r_width = 0xf	},

	{ .port_id = 48, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = (0x138 << 2), .w_bit_s = 8, .w_width = 0xf,
		.r_offset = (0x138 << 2), .r_bit_s = 12, .r_width = 0xf	},

	{ .port_id = 51, .reg_base = 0xfe010000,
		.reg_mode = 0, .bus_id = 6,
		.w_offset = 0x0, .w_bit_s = 0, .w_width = 0x0,
		.r_offset = (0x13b << 2), .r_bit_s = 8, .r_width = 0x7	},
};

static struct demod_priority demod_t6x __initdata = {
	.port_id = 46,
	.allow_access = 0,
	.reg_base = 0xfe31f000,
	.mode_reg1 = (0x3c00 << 2),
	.dtmb_mode_bit = 0,
	.isdbt_mode_bit = 1,
	.mode_reg2 = (0x3c0c << 2),
	.dvb_t2_mode_bit = 16,
	.isdbt_name = "ISDBT",
	.isdbt_reg = (0x0802 << 2),
	.dvb_t2_select_reg = 0xf040,
	.dvb_t2_select_mode = 0x182,
	.dvb_t2_reg = (0xd84 << 2),
	.dvb_t2_bit = 31,
	.dvb_t2_name = "DVB_T2",
	.dtmb_reg = (0x47 << 2),
	.dtmb_bit = 21,
	.dtmb_name = "DTMB",
};

static struct demux_priority demux_t6x __initdata = {
	.port_id = 36,
	.allow_access = 0,
	.reg = ((0x0434 << 2) + 0xfe444000),
	.w_bit_s = 18,
	.r_bit_s = 16,
	.w_width = 0x3,
	.r_width = 0x3,
};

static struct audio_priority audio_t6x __initdata = {
	.port_id = 35,
	.allow_access = 0,
	.reg_base = 0xfe330000,
	.num = 4,
	.audio_x[0] = { .id = 0, . name = "audio_a", .reg = (0x0040 << 2) },
	.audio_x[1] = { .id = 1, . name = "audio_b", .reg = (0x0050 << 2) },
	.audio_x[2] = { .id = 2, . name = "audio_c", .reg = (0x0060 << 2) },
	.audio_x[3] = { .id = 3, . name = "audio_d", .reg = (0x0210 << 2) },
	.reg = (0x00a5 << 2),
	.w_bit_s = {  0,  4,  8, 12 },
	.r_bit_s = { 16, 20, 24, 28 },
	.width = 0xf,
};

static struct bcon_priority bcon_t6x __initdata = {
	.port_id = 51,
	.allow_access = 0,
	.reg = 0xfe014084,
	.auto_bit = 15,
	.urgent_bit = 16,
	.low_level_s = 0,
	.high_level_s = 4,
	.level_width = 0xf,
};

static struct id_name hevc_write_t6x[] __initdata = {
	{ .id = 0x08, .name = "Amrisc LMEM Write",			},
	{ .id = 0x12, .name = "Swap Write",				},
	{ .id = 0x15, .name = "VP9 Process Write",			},
	{ .id = 0x16, .name = "VP9 Map Process Write",			},
	{ .id = 0x1b, .name = "AV1 Context  Write",			},
	{ .id = 0x1c, .name = "AV1 Segment Write",			},
	{ .id = 0x1d, .name = "AV1 TOP Write",				},
	{ .id = 0x1e, .name = "AV1 GMC Write",				},
	{ .id = 0x20, .name = "IPP Linebuf WR Cache",			},
	{ .id = 0x32, .name = "Mpred Write",				},
	{ .id = 0x60, .name = "Compress FIFO2 Write",			},
	{ .id = 0x61, .name = "Compress Tile Write",			},
	{ .id = 0x62, .name = "Compress Header Write",			},
	{ .id = 0x63, .name = "Compress Body Write",			},
	{ .id = 0x74, .name = "OW SAO Write Data0",			},
	{ .id = 0x75, .name = "OW SAO Write Data1",			},
	{ .id = 0x76, .name = "OW SAO Write Data2",			},
	{ .id = 0x77, .name = "OW SAO Write Data3",			},
	{ .id = 0x80, .name = "XIO TOP Write / Pscale Write",		},
	{ .id = 0x81, .name = "XIO LEFT Write",				},
	{ .id = 0x90, .name = "VLD MEM Write",				},
	{ .id = 0xa0, .name = "H264 TOP Buff Write",			},
	{ .id = 0xb1, .name = "CO_MB Write",				},
	{ .id = 0xc0, .name = "IQIDCT Canvas Write",			},
	{ .id = 0xd0, .name = "MC_MBBOT Write",				},
	{ .id = 0xe0, .name = "EXTIF BUF0 Write",			},
	{ .id = 0xe1, .name = "EXTIF BUF1 Write",			},
	{ .id = 0xe2, .name = "EXTIF BUF2 Write",			},
	{ .id = 0xe3, .name = "EXTIF BUF3 Write",			},
	{ .id = 0xf0, .name = "VC1 OST Write",				},
	{ .id = 0x80, .name = "XIO XOT Write",				},
	{ .id = 0x81, .name = "XIO XOL Write",				},
	{ .id = 0x90, .name = "CPI Write",				},
	{ .id = 0x91, .name = "CPI Write",				},
	{ .id = 0xa0, .name = "Evan Write",				},
};

static struct id_name hevc_read_t6x[] __initdata = {
	{ .id = 0x00, .name = "Amrisc IMEM Read",			},
	{ .id = 0x08, .name = "Amrisc LMEM Read",			},
	{ .id = 0x11, .name = "Bitstream Read",				},
	{ .id = 0x12, .name = "Swap Read",				},
	{ .id = 0x13, .name = "Map Read",				},
	{ .id = 0x15, .name = "VP9 Process Read",			},
	{ .id = 0x16, .name = "VP9 Map Process Read",			},
	{ .id = 0x1b, .name = "AV1 Context Read",			},
	{ .id = 0x1c, .name = "AV1 Segment Read",			},
	{ .id = 0x1d, .name = "AV1 TOP Read",				},
	{ .id = 0x20, .name = "IPP Linebuf RD Cache",			},
	{ .id = 0x31, .name = "Mpred Read",				},
	{ .id = 0x50, .name = "MCR Data0 Read",				},
	{ .id = 0x51, .name = "MCR Data1 Read",				},
	{ .id = 0x52, .name = "MCR Header Read",			},
	{ .id = 0x61, .name = "Compress Tile Read",			},
	{ .id = 0x62, .name = "Compress Header Read",			},
	{ .id = 0x80, .name = "XIO Read / Pscale Read",			},
	{ .id = 0x90, .name = "VLD MEM Read",				},
	{ .id = 0xa0, .name = "H264 TOP Buff READ",			},
	{ .id = 0xb1, .name = "CO_MB Read",				},
	{ .id = 0xc0, .name = "IQIDCT Canvas Read",			},
	{ .id = 0xe0, .name = "EXTIF BUF0 Read",			},
	{ .id = 0xe1, .name = "EXTIF BUF1 Read",			},
	{ .id = 0xe2, .name = "EXTIF BUF2 Read",			},
	{ .id = 0xe3, .name = "EXTIF BUF3 Read",			},
	{ .id = 0xf0, .name = "VC1 OST Read",				},
	{ .id = 0x80, .name = "XIO Read",				},
	{ .id = 0x90, .name = "CPI Read",				},
	{ .id = 0x91, .name = "CPI Read",				},
	{ .id = 0xa0, .name = "Evan Read",				},
};

static struct hevc_hcodec_priority hevc_t6x __initdata = {
	.port_id = 25,
	.allow_access = 0,
	.reg_base = 0xfe320000,
	.def_w_bit_s = 2, .def_w_width = 3,
	.def_r_bit_s = 0, .def_r_width = 3,
	.def_offset = (0x3fa3 << 2),

	.w_lv1_offset = (0x3fa4 << 2),
	.w_lv2_offset = (0x3fa8 << 2),
	.w_lv3_offset = (0x3fac << 2),
	.r_lv1_offset = (0x3fa6 << 2),
	.r_lv2_offset = (0x3faa << 2),
	.r_lv3_offset = (0x3fae << 2),

	.w_num = ARRAY_SIZE(hevc_write_t6x),
	.w_id_name = hevc_write_t6x,
	.r_num = ARRAY_SIZE(hevc_read_t6x),
	.r_id_name = hevc_read_t6x,
};

static struct id_name hcodec_write_t6x[] __initdata = {
	{ .id = 0x0d, .name = "MCRCC Write",				},
	{ .id = 0x14, .name = "LMEM Write",				},
	{ .id = 0x1c, .name = "Encoded Jpeg (VLC) Write",		},
	{ .id = 0x26, .name = "IPRED Write",				},
	{ .id = 0x28, .name = "MC MBBOT Write",				},
};

static struct id_name hcodec_read_t6x[] __initdata = {
	{ .id = 0x0c, .name = "MCRCC Read",				},
	{ .id = 0x0d, .name = "DBLK Read",				},
	{ .id = 0x10, .name = "IMEM Read",				},
	{ .id = 0x14, .name = "LMEM Read",				},
	{ .id = 0x26, .name = "IPRED Read",				},
	{ .id = 0x2c, .name = "MFDIN Read",				},
};

static struct hevc_hcodec_priority hcodec_t6x __initdata = {
	.port_id = 29,
	.allow_access = 0,
	.reg_base = 0xfe320000,
	.def_w_bit_s = 2, .def_w_width = 3,
	.def_r_bit_s = 0, .def_r_width = 3,
	.def_offset = (0x3fa1 << 2),

	.w_lv1_offset = (0x3fb0 << 2),
	.w_lv2_offset = (0x3fb4 << 2),
	.w_lv3_offset = (0x3fb8 << 2),
	.r_lv1_offset = (0x3fb2 << 2),
	.r_lv2_offset = (0x3fb6 << 2),
	.r_lv3_offset = (0x3fba << 2),

	.w_num = ARRAY_SIZE(hcodec_write_t6x),
	.w_id_name = hcodec_write_t6x,
	.r_num = ARRAY_SIZE(hcodec_read_t6x),
	.r_id_name = hcodec_read_t6x,
};

static struct vpu_super_axi vpu_super_axi_t6x[] __initdata = {
	{ .axi = 0, .rw = 0, .en_bit = 0, .ur_bit_s = 16, .ur_bit_w = 3, },
	{ .axi = 0, .rw = 1, .en_bit = 4, .ur_bit_s = 24, .ur_bit_w = 3, },
	{ .axi = 2, .rw = 0, .en_bit = 2, .ur_bit_s = 20, .ur_bit_w = 3, },
	{ .axi = 2, .rw = 1, .en_bit = 6, .ur_bit_s = 28, .ur_bit_w = 3, },
};

static struct vpu_super_urgent vpu_super_urgent_t6x __initdata = {
	.exist = 1,
	.addr = (0x2735 << 2),
	.sec = 0,
	.num = ARRAY_SIZE(vpu_super_axi_t6x),
	.axi = vpu_super_axi_t6x,
};

static struct vpu_urgent vpu0_rd0_urgent_t6x[] __initdata = {
	{ .id = 0x00, .level = 0x00000000, .addr = (0x1a2b << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 1 },
	{ .id = 0x01, .level = 0x00000001, .addr = (0x1a4b << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 1 },
	{ .id = 0x04, .level = 0x00000002, .addr = (0x3d9c << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 1 },
	{ .id = 0x07, .level = 0x00000003, .addr = 0x0,           .sec = 0, .ur_bit_s = 0xfd, .ur_bit_w = 0 },	// Not fixed
};

static struct vpu_urgent vpu0_rd1_urgent_t6x[] __initdata = {
	{ .id = 0x00, .level = 0x00000000, .addr = (0x1a2b << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 1 },
	{ .id = 0x01, .level = 0x00000001, .addr = (0x1a4b << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 1 },
	{ .id = 0x04, .level = 0x00000002, .addr = (0x3d9c << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 1 },
	{ .id = 0x07, .level = 0x00000003, .addr = 0x0,           .sec = 0, .ur_bit_s = 0xfd, .ur_bit_w = 0 },	// Not fixed
};

static struct vpu_urgent vpu0_rd2_urgent_t6x[] __initdata = {
	{ .id = 0x02, .level = 0x00000000, .addr = 0x0,           .sec = 0, .ur_bit_s = 0xfe, .ur_bit_w = 0 },	// Floating priority
	{ .id = 0x03, .level = 0x00000001, .addr = 0x0,           .sec = 0, .ur_bit_s = 0xfe, .ur_bit_w = 0 },	// Floating priority
	{ .id = 0x0a, .level = 0x00000002, .addr = (0x1100 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x0b, .level = 0x00000003, .addr = (0x5d95 << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 3 },
	{ .id = 0x1b, .level = 0x00000013, .addr = (0x5d95 << 2), .sec = 0, .ur_bit_s = 2,    .ur_bit_w = 3 },
	{ .id = 0x2b, .level = 0x00000023, .addr = (0x5d95 << 2), .sec = 0, .ur_bit_s = 4,    .ur_bit_w = 3 },
	{ .id = 0x3b, .level = 0x00000033, .addr = (0x5d95 << 2), .sec = 0, .ur_bit_s = 6,    .ur_bit_w = 3 },
	{ .id = 0x4b, .level = 0x00000043, .addr = (0x5d95 << 2), .sec = 0, .ur_bit_s = 8,    .ur_bit_w = 3 },
	{ .id = 0x0d, .level = 0x00000004, .addr = (0x27cb << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x0e, .level = 0x00000005, .addr = (0x6c61 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
};

static struct vpu_urgent vpu0_rd3_urgent_t6x[] __initdata = {
	{ .id = 0x09, .level = 0x00000000, .addr = 0x0,    .sec = 0, .ur_bit_s = 0xff, .ur_bit_w = 0 },	// Force 0 priority
};

static struct vpu_urgent vpu0_rd4_urgent_t6x[] __initdata = {
	{ .id = 0x0f, .level = 0x00000000, .addr = (0x4622 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x1f, .level = 0x00000001, .addr = (0x4622 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x2f, .level = 0x00000002, .addr = (0x464b << 2), .sec = 0, .ur_bit_s = 31,   .ur_bit_w = 1 },
	{ .id = 0x3f, .level = 0x00000003, .addr = (0x4812 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
};

static struct vpu_urgent vpu0_rd5_urgent_t6x[] __initdata = {
	{ .id = 0x05, .level = 0x00000000, .addr = (0x6c44 << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 3 },
	{ .id = 0x06, .level = 0x00000001, .addr = (0x6c44 << 2), .sec = 0, .ur_bit_s = 2,    .ur_bit_w = 3 },
	{ .id = 0x08, .level = 0x00000002, .addr = (0x6c44 << 2), .sec = 0, .ur_bit_s = 4,    .ur_bit_w = 3 },
};

static struct vpu_top_map vpu_top_map_axi0_rd_t6x[] __initdata = {
	{ .ur_bit_s = 0,  .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_rd0_urgent_t6x), .dev = vpu0_rd0_urgent_t6x, },
	{ .ur_bit_s = 2,  .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_rd1_urgent_t6x), .dev = vpu0_rd1_urgent_t6x, },
	{ .ur_bit_s = 6,  .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_rd2_urgent_t6x), .dev = vpu0_rd2_urgent_t6x, },
	{ .ur_bit_s = 8,  .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_rd3_urgent_t6x), .dev = vpu0_rd3_urgent_t6x, },
	{ .ur_bit_s = 10, .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_rd4_urgent_t6x), .dev = vpu0_rd4_urgent_t6x, },
	{ .ur_bit_s = 18, .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_rd5_urgent_t6x), .dev = vpu0_rd5_urgent_t6x, },
};

static struct vpu_urgent vpu0_wr0_urgent_t6x[] __initdata = {
	{ .id = 0x00, .level = 0x00000000, .addr = (0x43d3 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x10, .level = 0x00000010, .addr = (0x43d8 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x01, .level = 0x00000001, .addr = (0x44d3 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x11, .level = 0x00000011, .addr = (0x44d8 << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x02, .level = 0x00000002, .addr = (0x43f4 << 2), .sec = 0, .ur_bit_s = 29,   .ur_bit_w = 1 },
	{ .id = 0x03, .level = 0x00000003, .addr = 0x0,           .sec = 0, .ur_bit_s = 0xfe, .ur_bit_w = 0 },	// Floating priority
};

static struct vpu_urgent vpu0_wr1_urgent_t6x[] __initdata = {
	{ .id = 0x0e, .level = 0x00000000, .addr = 0x0,    .sec = 0, .ur_bit_s = 0xfe, .ur_bit_w = 0 },	// Floating priority
};

static struct vpu_urgent vpu0_wr2_urgent_t6x[] __initdata = {
	{ .id = 0x06, .level = 0x00000000, .addr = (0x5d98 << 2), .sec = 0, .ur_bit_s = 0,    .ur_bit_w = 3 },
	{ .id = 0x16, .level = 0x00000010, .addr = (0x5d98 << 2), .sec = 0, .ur_bit_s = 2,    .ur_bit_w = 3 },
	{ .id = 0x26, .level = 0x00000020, .addr = (0x5d98 << 2), .sec = 0, .ur_bit_s = 4,    .ur_bit_w = 3 },
	{ .id = 0x36, .level = 0x00000001, .addr = (0x1100 << 2), .sec = 0, .ur_bit_s = 7,    .ur_bit_w = 1 },
	{ .id = 0x46, .level = 0x00000002, .addr = (0x27ce << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x56, .level = 0x00000003, .addr = (0x460e << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
	{ .id = 0x66, .level = 0x00000013, .addr = (0x460d << 2), .sec = 0, .ur_bit_s = 31,   .ur_bit_w = 1 },
	{ .id = 0x76, .level = 0x00000023, .addr = (0x465a << 2), .sec = 0, .ur_bit_s = 16,   .ur_bit_w = 1 },
};

static struct vpu_urgent vpu0_wr3_urgent_t6x[] __initdata = {
	{ .id = 0x05, .level = 0x00000000, .addr = 0x0,    .sec = 0, .ur_bit_s = 0xff, .ur_bit_w = 0 },	// Force 0 priority
};

static struct vpu_top_map vpu_top_map_axi0_wr_t6x[] __initdata = {
	{ .ur_bit_s = 0,  .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_wr0_urgent_t6x), .dev = vpu0_wr0_urgent_t6x, },
	{ .ur_bit_s = 2,  .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_wr1_urgent_t6x), .dev = vpu0_wr1_urgent_t6x, },
	{ .ur_bit_s = 6,  .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_wr2_urgent_t6x), .dev = vpu0_wr2_urgent_t6x, },
	{ .ur_bit_s = 10, .ur_bit_w = 3, .num = ARRAY_SIZE(vpu0_wr3_urgent_t6x), .dev = vpu0_wr3_urgent_t6x, },
};

static struct vpu_top_map vpu_top_map_axi2_rd_t6x[] __initdata = {
	{ .ur_bit_s = 0,  .ur_bit_w = 3, .num = 1, .id = { 0x00 }, },
	{ .ur_bit_s = 2,  .ur_bit_w = 3, .num = 1, .id = { 0x01 }, },
	{ .ur_bit_s = 4,  .ur_bit_w = 3, .num = 1, .id = { 0x02 }, },
	{ .ur_bit_s = 6,  .ur_bit_w = 3, .num = 1, .id = { 0x03 }, },
	{ .ur_bit_s = 8,  .ur_bit_w = 3, .num = 3, .id = { 0x04, 0x05, 0x36 }, },
};

static struct vpu_top vpu_top_t6x[] __initdata = {
	{ .port_id = 8,  .axi = 0, .rw = 0, .addr = (0x27c2 << 2), .sec = 0, .num = ARRAY_SIZE(vpu_top_map_axi0_rd_t6x), .map = vpu_top_map_axi0_rd_t6x},
	{ .port_id = 8,  .axi = 0, .rw = 1, .addr = (0x27c3 << 2), .sec = 0, .num = ARRAY_SIZE(vpu_top_map_axi0_wr_t6x), .map = vpu_top_map_axi0_wr_t6x},
	{ .port_id = 16, .axi = 2, .rw = 0, .addr = (0x6c24 << 2), .sec = 0, .num = ARRAY_SIZE(vpu_top_map_axi2_rd_t6x), .map = vpu_top_map_axi2_rd_t6x},
};

static struct vpu_bit_id_map vpu0_rd_t6x[] __initdata = {
	{ .id = 0x00, .bit = 20, },
	{ .id = 0x01, .bit = 21, },
	{ .id = 0x04, .bit = 24, },
	{ .id = 0x07, .bit = 27, },
};

static struct vpu_dev_bus_routing routing_t6x[] __initdata = {
	{
		.addr = (0x3978 << 2),
		.sec = 0,
		.axi = 0,
		.rw = 0,
		.idx0 = 0,
		.idx1 = 1,
		.num = ARRAY_SIZE(vpu0_rd_t6x),
		.map = vpu0_rd_t6x,
	}
};

static struct vpu_priority vpu_t6x __initdata = {
	.allow_access = 1,
	.reg_base = 0xff800000,
	.sur = &vpu_super_urgent_t6x,
	.routing_num = ARRAY_SIZE(routing_t6x),
	.routing = routing_t6x,
	.num = ARRAY_SIZE(vpu_top_t6x),
	.top = vpu_top_t6x,
};

static struct ddr_priority_v2 ddr_priority_t6x __initdata = {
	.dmc_num = ARRAY_SIZE(dmc_t6x),
	.dmc = dmc_t6x,
	.bus_dev_num = ARRAY_SIZE(bus_dev_t6x),
	.bus_dev = bus_dev_t6x,
	.demod = &demod_t6x,
	.demux = &demux_t6x,
	.audio = &audio_t6x,
	.bcon = &bcon_t6x,
	.hevc = &hevc_t6x,
	.hcodec = &hcodec_t6x,
	.vpu = &vpu_t6x,
};
static struct ddr_priority_v1 ddr_priority_c4[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 1, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x85 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x85 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 2, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x8a << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x8a << 2), .r_bit_s = 16, .r_width = 0xf	},
	{ .port_id = 3, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0x95 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0x95 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 6, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa0 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa0 << 2), .r_bit_s = 16, .r_width = 0xf	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0x8000,
		.w_offset = (0xa5 << 2), .w_bit_s = 16, .w_width = 0xf,
		.r_offset = (0xa5 << 2), .r_bit_s = 16, .r_width = 0xf	},
};
#endif
static struct ddr_priority_v1 ddr_priority_s1a[] __initdata = {
	{ .port_id = 0, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x80 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x80 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 3, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x8c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x8c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 4, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x90 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x90 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 7, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x9c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x9c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 11, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0xac << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0xac << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 16, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x60 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x60 << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 19, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x6c << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x6c << 2), .r_bit_s = 16, .r_width = 0x7	},

	{ .port_id = 21, .reg_base = 0xfe036000,
		.reg_mode = 0, .force_enable = 0,
		.w_offset = (0x74 << 2), .w_bit_s = 16, .w_width = 0x7,
		.r_offset = (0x74 << 2), .r_bit_s = 16, .r_width = 0x7	},
};

static struct ddr_priority ddr_priority;
struct ddr_priority_v2 * __init ddr_priority_v2_init(struct ddr_priority_v2 *v2)
{
	unsigned int size;
	unsigned int dmc_offset;
	unsigned int bus_dev_offset;
	unsigned int demod_offset;
	unsigned int demux_offset;
	unsigned int audio_offset;
	unsigned int bcon_offset;
	unsigned int hevc_offset;
	unsigned int hevc_w_id_name_offset;
	unsigned int hevc_r_id_name_offset;
	unsigned int hcodec_offset;
	unsigned int hcodec_w_id_name_offset;
	unsigned int hcodec_r_id_name_offset;
	unsigned int vpu_offset;
	unsigned int vpu_sur_offset;
	unsigned int vpu_sur_axi_offset;
	unsigned int routing_offset;
	unsigned int routing_map_offset;
	unsigned int vpu_top_offset;
	unsigned int vpu_map_offset;
	unsigned int vpu_urgent_offset;
	unsigned int i, j, n;
	struct ddr_priority_v2 *temp;

	dmc_offset = sizeof(struct ddr_priority_v2);
	bus_dev_offset = dmc_offset + v2->dmc_num * sizeof(struct dmc_priority);
	demod_offset = bus_dev_offset + v2->bus_dev_num * sizeof(struct bus_dev_priority);
	demux_offset = demod_offset + sizeof(struct demod_priority);
	audio_offset = demux_offset + sizeof(struct demux_priority);
	bcon_offset = audio_offset + sizeof(struct audio_priority);
	hevc_offset = bcon_offset + sizeof(struct bcon_priority);
	hevc_w_id_name_offset = hevc_offset + sizeof(struct hevc_hcodec_priority);
	hevc_r_id_name_offset = hevc_w_id_name_offset + v2->hevc->w_num * sizeof(struct id_name);
	hcodec_offset = hevc_r_id_name_offset + v2->hevc->r_num * sizeof(struct id_name);
	hcodec_w_id_name_offset = hcodec_offset + sizeof(struct hevc_hcodec_priority);
	hcodec_r_id_name_offset = hcodec_w_id_name_offset + v2->hcodec->w_num * sizeof(struct id_name);
	vpu_offset = hcodec_r_id_name_offset + v2->hcodec->r_num * sizeof(struct id_name);
	vpu_sur_offset = vpu_offset + sizeof(struct vpu_priority);
	vpu_sur_axi_offset = vpu_sur_offset + sizeof(struct vpu_super_urgent);
	routing_offset = vpu_sur_axi_offset + v2->vpu->sur->num * sizeof(struct vpu_super_axi);
	routing_map_offset = routing_offset + v2->vpu->routing_num * sizeof(struct vpu_dev_bus_routing);
	for (i = 0, n = 0; i < v2->vpu->routing_num; i++)
		n += v2->vpu->routing[i].num;
	vpu_top_offset = routing_map_offset + n * sizeof(struct vpu_bit_id_map);	// ->top
	vpu_map_offset = vpu_top_offset + v2->vpu->num * sizeof(struct vpu_top);
	for (i = 0, n = 0; i < v2->vpu->num; i++)
		n += v2->vpu->top[i].num;
	vpu_urgent_offset = vpu_map_offset + n * sizeof(struct vpu_top_map);
	for (i = 0, n = 0; i < v2->vpu->num; i++)
		for (j = 0; j < v2->vpu->top[i].num; j++)
			if (v2->vpu->top[i].map[j].dev)
				n += v2->vpu->top[i].map[j].num;
	size = vpu_urgent_offset + n * sizeof(struct vpu_urgent);

	temp = kzalloc(size, GFP_KERNEL);
	if (!temp)
		return NULL;

	memcpy(temp, v2, sizeof(struct ddr_priority_v2));

	temp->dmc = (struct dmc_priority *)((char *)temp + dmc_offset);
	memcpy(temp->dmc, v2->dmc, v2->dmc_num * sizeof(struct dmc_priority));

	temp->bus_dev = (struct bus_dev_priority *)((char *)temp + bus_dev_offset);
	memcpy(temp->bus_dev, v2->bus_dev, v2->bus_dev_num * sizeof(struct bus_dev_priority));

	temp->demod = (struct demod_priority *)((char *)temp + demod_offset);
	memcpy(temp->demod, v2->demod, sizeof(struct demod_priority));

	temp->demux = (struct demux_priority *)((char *)temp + demux_offset);
	memcpy(temp->demux, v2->demux, sizeof(struct demux_priority));

	temp->audio = (struct audio_priority *)((char *)temp + audio_offset);
	memcpy(temp->audio, v2->audio, sizeof(struct audio_priority));

	temp->bcon = (struct bcon_priority *)((char *)temp + bcon_offset);
	memcpy(temp->bcon, v2->bcon, sizeof(struct bcon_priority));

	temp->hevc = (struct hevc_hcodec_priority *)((char *)temp + hevc_offset);
	memcpy(temp->hevc, v2->hevc, sizeof(struct hevc_hcodec_priority));

	temp->hevc->w_id_name = (struct id_name *)((char *)temp + hevc_w_id_name_offset);
	memcpy(temp->hevc->w_id_name, v2->hevc->w_id_name, v2->hevc->w_num * sizeof(struct id_name));

	temp->hevc->r_id_name = (struct id_name *)((char *)temp + hevc_r_id_name_offset);
	memcpy(temp->hevc->r_id_name, v2->hevc->r_id_name, v2->hevc->r_num * sizeof(struct id_name));

	temp->hcodec = (struct hevc_hcodec_priority *)((char *)temp + hcodec_offset);
	memcpy(temp->hcodec, v2->hcodec, sizeof(struct hevc_hcodec_priority));

	temp->hcodec->w_id_name = (struct id_name *)((char *)temp + hcodec_w_id_name_offset);
	memcpy(temp->hcodec->w_id_name, v2->hcodec->w_id_name, v2->hcodec->w_num * sizeof(struct id_name));

	temp->hcodec->r_id_name = (struct id_name *)((char *)temp + hcodec_r_id_name_offset);
	memcpy(temp->hcodec->r_id_name, v2->hcodec->r_id_name, v2->hcodec->r_num * sizeof(struct id_name));

	temp->vpu = (struct vpu_priority *)((char *)temp + vpu_offset);
	memcpy(temp->vpu, v2->vpu, sizeof(struct vpu_priority));

	temp->vpu->sur = (struct vpu_super_urgent *)((char *)temp + vpu_sur_offset);
	memcpy(temp->vpu->sur, v2->vpu->sur, sizeof(struct vpu_super_urgent));

	temp->vpu->sur->axi = (struct vpu_super_axi *)((char *)temp + vpu_sur_axi_offset);
	memcpy(temp->vpu->sur->axi, v2->vpu->sur->axi, v2->vpu->sur->num * sizeof(struct vpu_super_axi));

	temp->vpu->routing = (struct vpu_dev_bus_routing *)((char *)temp + routing_offset);
	memcpy(temp->vpu->routing, v2->vpu->routing, v2->vpu->routing_num * sizeof(struct vpu_dev_bus_routing));

	for (i = 0, n = 0; i < v2->vpu->routing_num; i++) {
		temp->vpu->routing[i].map = (struct vpu_bit_id_map *)((char *)temp + routing_map_offset) + n;
		memcpy(temp->vpu->routing[i].map, v2->vpu->routing[i].map, v2->vpu->routing[i].num * sizeof(struct vpu_bit_id_map));
		n += v2->vpu->routing[i].num;
	}

	temp->vpu->top = (struct vpu_top *)((char *)temp + vpu_top_offset);
	memcpy(temp->vpu->top, v2->vpu->top, v2->vpu->num * sizeof(struct vpu_top));

	for (i = 0, n = 0; i < v2->vpu->num; i++) {
		temp->vpu->top[i].map = (struct vpu_top_map *)((char *)temp + vpu_map_offset) + n;
		memcpy(temp->vpu->top[i].map, v2->vpu->top[i].map, v2->vpu->top[i].num * sizeof(struct vpu_top_map));
		n += v2->vpu->top[i].num;
	}

	for (i = 0, n = 0; i < v2->vpu->num; i++) {
		for (j = 0; j < v2->vpu->top[i].num; j++) {
			if (v2->vpu->top[i].map[j].dev) {
				temp->vpu->top[i].map[j].dev = (struct vpu_urgent *)((char *)temp + vpu_urgent_offset) + n;
				memcpy(temp->vpu->top[i].map[j].dev, v2->vpu->top[i].map[j].dev, v2->vpu->top[i].map[j].num * sizeof(struct vpu_urgent));
				n += v2->vpu->top[i].map[j].num;
			}
		}
	}

	temp->simplify_buf = kmalloc(0x4000, GFP_KERNEL);
	if (!temp->simplify_buf)
		return NULL;

	pr_debug("dmc test: offset=%x\n", temp->dmc[temp->dmc_num - 1].offset);
	pr_debug("bus_dev test: port_id=%d\n", temp->bus_dev[temp->bus_dev_num - 1].port_id);
	pr_debug("demod test: dtmb_name=%s\n", temp->demod->dtmb_name);
	pr_debug("demux test: w_bit_s=%d\n", temp->demux->w_bit_s);
	pr_debug("audio test: audio_x[3].name=%s\n", temp->audio->audio_x[3].name);
	pr_debug("bcon test: auto_bit=%d\n", temp->bcon->auto_bit);
	pr_debug("hevc test: r_lv1_offset=%x w_name=%s r_name=%s\n",
		temp->hevc->r_lv1_offset, temp->hevc->w_id_name[temp->hevc->w_num - 1].name,
		temp->hevc->r_id_name[temp->hevc->r_num - 1].name);
	pr_debug("hcodec test: r_lv1_offset=%x w_name=%s r_name=%s\n",
		temp->hcodec->r_lv1_offset, temp->hcodec->w_id_name[temp->hcodec->w_num - 1].name,
		temp->hcodec->r_id_name[temp->hcodec->r_num - 1].name);
	pr_debug("vpu test: num=%d\n", temp->vpu->num);
	pr_debug("vpu sur test: addr=%x, ur_bit_s=%d\n", temp->vpu->sur->addr,
		temp->vpu->sur->axi[temp->vpu->sur->num - 1].ur_bit_s);
	pr_debug("vpu routing test: addr=%x, bit=%d\n",
		temp->vpu->routing[temp->vpu->routing_num - 1].addr,
		temp->vpu->routing[temp->vpu->routing_num - 1].map[temp->vpu->routing[temp->vpu->routing_num - 1].num - 1].bit);
	pr_debug("vpu top test: addr=%x num=%d, map_ur_bit_s=%d, id=%d\n",
		temp->vpu->top[0].addr, temp->vpu->top[0].num,
		temp->vpu->top[0].map[temp->vpu->top[0].num - 1].ur_bit_s,
		temp->vpu->top[0].map[temp->vpu->top[0].num - 1].dev[2].id);
	pr_debug("vpu top test: axi=%d id[1]=%d\n", temp->vpu->top[temp->vpu->num - 1].axi,
		temp->vpu->top[temp->vpu->num - 1].map[temp->vpu->top[temp->vpu->num - 1].num - 1].id[1]);

	return temp;
}

struct ddr_priority * __init get_ddr_priority(int cpu_type)
{
	struct ddr_priority_v1 *v1 = NULL;
	struct ddr_priority_v2 *v2 = NULL;
	int v1_num;

	switch (cpu_type) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_T7:
		v1 = ddr_priority_t7;
		v1_num = ARRAY_SIZE(ddr_priority_t7);
		break;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	case DMC_TYPE_S4:
		v1 = ddr_priority_s4;
		v1_num = ARRAY_SIZE(ddr_priority_s4);
		break;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_S5:
		v1 = ddr_priority_s5;
		v1_num = ARRAY_SIZE(ddr_priority_s5);
		break;
	case DMC_TYPE_S6:
		v1 = ddr_priority_s6;
		v1_num = ARRAY_SIZE(ddr_priority_s6);
		break;
	case DMC_TYPE_C3:
		v1 = ddr_priority_c3;
		v1_num = ARRAY_SIZE(ddr_priority_c3);
		break;
	case DMC_TYPE_T5M:
		v1 = ddr_priority_t5m;
		v1_num = ARRAY_SIZE(ddr_priority_t5m);
		break;
	case DMC_TYPE_TXHD2:
		v1 = ddr_priority_txhd2;
		v1_num = ARRAY_SIZE(ddr_priority_txhd2);
		break;
	case DMC_TYPE_T3X:
		v1 = ddr_priority_t3x;
		v1_num = ARRAY_SIZE(ddr_priority_t3x);
		break;
	case DMC_TYPE_T3:
		v1 = ddr_priority_t3;
		v1_num = ARRAY_SIZE(ddr_priority_t3);
		break;
	case DMC_TYPE_A4:
		v1 = ddr_priority_a4;
		v1_num = ARRAY_SIZE(ddr_priority_a4);
		break;
	case DMC_TYPE_S7:
		v1 = ddr_priority_s7;
		v1_num = ARRAY_SIZE(ddr_priority_s7);
		break;
	case DMC_TYPE_S7D:
		v1 = ddr_priority_s7d;
		v1_num = ARRAY_SIZE(ddr_priority_s7d);
		break;
	case DMC_TYPE_SC2:
		v1 = ddr_priority_sc2;
		v1_num = ARRAY_SIZE(ddr_priority_sc2);
		break;
	case DMC_TYPE_T5W:
		v1 = ddr_priority_t5w;
		v1_num = ARRAY_SIZE(ddr_priority_t5w);
		break;
	case DMC_TYPE_G12B:
		v1 = ddr_priority_g12b;
		v1_num = ARRAY_SIZE(ddr_priority_g12b);
		break;
	case DMC_TYPE_GXLX3:
		v1 = ddr_priority_gxlx3;
		v1_num = ARRAY_SIZE(ddr_priority_gxlx3);
		break;
	case DMC_TYPE_T6D:
		v1 = ddr_priority_t6d;
		v1_num = ARRAY_SIZE(ddr_priority_t6d);
		break;
	case DMC_TYPE_T6W:
		v1 = ddr_priority_t6w;
		v1_num = ARRAY_SIZE(ddr_priority_t6w);
		break;
	case DMC_TYPE_T6X:
		v2 = &ddr_priority_t6x;
		break;
	case DMC_TYPE_C4:
		v1 = ddr_priority_c4;
		v1_num = ARRAY_SIZE(ddr_priority_c4);
		break;
#endif
	case DMC_TYPE_S1A:
		v1 = ddr_priority_s1a;
		v1_num = ARRAY_SIZE(ddr_priority_s1a);
		break;
	default:
		return NULL;
	}
	if (v1) {
		ddr_priority.v1_buf = kmalloc(4096, GFP_KERNEL);
		if (!ddr_priority.v1_buf)
			return NULL;
		ddr_priority.v1 = kcalloc(v1_num, sizeof(struct ddr_priority_v1), GFP_KERNEL);
		if (!ddr_priority.v1)
			return NULL;
		memcpy(ddr_priority.v1, v1, sizeof(struct ddr_priority_v1) * v1_num);
		ddr_priority.ver = 1;
		ddr_priority.v1_num = v1_num;
		return &ddr_priority;
	} else if (v2) {
		ddr_priority.v2 = ddr_priority_v2_init(v2);
		if (!ddr_priority.v2)
			return NULL;
		ddr_priority.ver = 2;
		return &ddr_priority;
	}

	return NULL;
}

static int ddr_priority_get_info(unsigned char port_id)
{
	unsigned int i;

	if (!aml_db->priority->v1) {
		pr_info("ddr priority null\n");
		return -EINVAL;
	}

	for (i = 0; i < aml_db->priority->v1_num; i++) {
		if (aml_db->priority->v1[i].port_id == port_id)
			return i;
	}

	return -EINVAL;
}

static int ddr_priority_config(struct ddr_priority_v1 info,
				int *priority, unsigned char type)
{
	void __iomem *vaddr = 0;
	unsigned int reg_addr, reg_value;
	unsigned char bit_s;
	unsigned int width;
	int set_priority = 0;

	/* type define
	 * 0x00: read r_priority  0x10: read w_priority
	 * 0x01: write r_priority  0x11: write w_priority
	 */
	if (type & 0x10) {
		if (info.w_width) {
			reg_addr = info.reg_base + info.w_offset;
			bit_s = info.w_bit_s;
			width = info.w_width;
		} else {
			pr_err("port id : %d w_priority not exit\n", info.port_id);
			*priority = -1;
			return -EINVAL;
		}

	} else {
		if (info.r_width) {
			reg_addr = info.reg_base + info.r_offset;
			bit_s = info.r_bit_s;
			width = info.r_width;
		} else {
			pr_err("port id : %d r_priority not exit\n", info.port_id);
			*priority = -1;
			return -EINVAL;
		}
	}

	/*
	 * current only c3 and t7 use isp module reg to ctrl priority,
	 * so need isp clk and power be normal;
	 * after will be use dmc reg to ctrl, can ignore clk and power.
	 */
	if (aml_db->cpu_type == DMC_TYPE_T7 || aml_db->cpu_type == DMC_TYPE_C3) {
		if (strstr(priority_find_port_name(info.port_id), "ISP")) {
			if (!(aml_db->priority->flag & DDR_PRIORITY_POWER)) {
				*priority = -1;
				return -EINVAL;
			}
		}
	}

	if (aml_db->priority->flag & DDR_PRIORITY_DEBUG)
		pr_info("port %d, name %s, addr %x mode:%d\n",
			info.port_id,
			priority_find_port_name(info.port_id),
			reg_addr,
			info.reg_mode);

	set_priority = width & ((*priority) | (*priority) << 8 |
			(*priority) << 16 | (*priority) << 24);

	if (info.reg_mode) {/* secure mode */
		reg_value = dmc_rw(reg_addr, 0, DMC_READ);
		if (type & 0x01) {
			reg_value &= ~(width << bit_s);
			reg_value |= (set_priority << bit_s);
			reg_value |= info.force_enable;
			dmc_rw(reg_addr, reg_value, DMC_WRITE);
		} else {
			*priority = (reg_value >> bit_s) & width;
		}
	} else {
		vaddr = ioremap(reg_addr, 0x4);
		if (!vaddr) {
			pr_err("reg_addr %x ioremap failed\n", reg_addr);
			return -EINVAL;
		}
		reg_value = readl_relaxed(vaddr);
		if (type & 0x01) {
			reg_value &= ~(width << bit_s);
			reg_value |= (set_priority << bit_s);
			reg_value |= info.force_enable;
			writel_relaxed(reg_value, vaddr);
		} else {
			*priority = (reg_value >> bit_s) & width;
		}
	}

	if (vaddr)
		iounmap(vaddr);

	return 0;
}

int ddr_priority_rw(unsigned char port_id, int *priority_r,
			int *priority_w, unsigned char control)
{
	int index;
	struct ddr_priority_v1 info;

	index = ddr_priority_get_info(port_id);
	if (index < 0) {
		pr_err("port_id: %d not exit\n", port_id);
		return -EINVAL;
	}
	info = aml_db->priority->v1[index];

	switch (control) {
	case 0:
		ddr_priority_config(info, priority_r, 0x00);
		ddr_priority_config(info, priority_w, 0x10);
		break;
	case 1:
		ddr_priority_config(info, priority_r, 0x01);
		ddr_priority_config(info, priority_w, 0x11);
		break;
	case 2:
		ddr_priority_config(info, priority_r, 0x01);
		break;
	case 3:
		ddr_priority_config(info, priority_w, 0x11);
		break;
	default:
		break;
	}

	return 0;
}

static struct ddr_port_desc *priority_port_list;
static unsigned int priority_port_list_num;
void __init ddr_priority_port_list(void)
{
	struct ddr_port_desc *desc = NULL;
	int pcnt = 0;

	if (aml_db->cpu_type != DMC_TYPE_C3) {
		priority_port_list = aml_db->port_desc;
		priority_port_list_num = aml_db->real_ports;
	} else {
		pcnt = ddr_find_port_desc_type(aml_db->cpu_type, &desc, 0);
		if (pcnt < 0) {
			pr_err("can't find priority_port_list, cpu:%d\n", aml_db->cpu_type);
		} else {
			priority_port_list = desc;
			priority_port_list_num = pcnt;
		}
	}
}

char *priority_find_port_name(int id)
{
	int i;

	if (!priority_port_list || !priority_port_list_num)
		return NULL;

	for (i = 0; i < priority_port_list_num; i++) {
		if (priority_port_list[i].port_id == id)
			return priority_port_list[i].port_name;
	}
	return NULL;
}

static int reg_access(unsigned int reg, char sec, char rw, unsigned int *value)
{
	if (sec) {
		if (rw == READ)
			*value = dmc_rw(reg, 0, DMC_READ);
		else
			dmc_rw(reg, *value, DMC_WRITE);
	} else {
		void __iomem *vaddr;

		vaddr = ioremap(reg, 0x4);
		if (!vaddr) {
			pr_err("reg_addr %x ioremap failed\n", reg);
			return -EINVAL;
		}
		if (rw == READ)
			*value = readl_relaxed(vaddr);
		else
			writel_relaxed(*value, vaddr);
		iounmap(vaddr);
	}
	return 0;
}

static char *get_name_by_port_id(unsigned char port_id)
{
	int i;
	struct ddr_port_desc *port_desc;
	unsigned short real_ports;

	port_desc = aml_db->port_desc;
	real_ports = aml_db->real_ports;
	for (i = 0; i < real_ports; i++) {
		if (port_desc[i].port_id == port_id)
			return port_desc[i].port_name;
	}
	return NULL;
}

static int dmc_priority_display(int i, char *buf)
{
	int s = 0;
	unsigned int value;
	struct dmc_priority *dmc;

	dmc = &aml_db->priority->v2->dmc[i];
	reg_access(dmc->reg_base + dmc->offset, 0, READ, &value);

	aml_db->priority->v2->cur_dmc.force_enable = !!(value & dmc->force_enable);
	aml_db->priority->v2->cur_dmc.w_priority = (value >> dmc->w_bit_s) & dmc->width;
	aml_db->priority->v2->cur_dmc.r_priority = (value >> dmc->r_bit_s) & dmc->width;

	s += sprintf(buf + s, "%2d %x %x:%x %x:%x\n",
			dmc->bus_id,
			!!(value & dmc->force_enable),
			(value >> dmc->r_bit_s) & dmc->width,
			dmc->width,
			(value >> dmc->w_bit_s) & dmc->width,
			dmc->width);
	return s;
}

static char hex_digit(unsigned char val)
{
	if (val < 10)
		return '0' + val;
	else if (val <= 0xf && val >= 10)
		return 'a' + val - 10;
	else
		return val;
}

#define DEV_PRIORITY_PRE "\t\t"
static int get_demod_priority_info(char *buf)
{
	int s = 0;
	unsigned int value;
	struct demod_priority *dm;
	int priority;
	unsigned int dvb_t2_mode;
	unsigned char w_bit_s;
	unsigned char r_bit_s;
	struct bus_dev_priority *bus_dev;
	struct ddr_priority_v2 *v2;

	dm = aml_db->priority->v2->demod;
	if (!dm->allow_access) {
		s += sprintf(buf + s, DEV_PRIORITY_PRE "\t\t\tinaccessible\n");
		return s;
	}

	v2 = aml_db->priority->v2;
	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	w_bit_s = hweight8(bus_dev->w_width);
	r_bit_s = hweight8(bus_dev->r_width);
	reg_access(dm->reg_base + dm->mode_reg1, 0, READ, &value);
	if (value & BIT(dm->dtmb_mode_bit)) {
		reg_access(dm->reg_base + dm->dtmb_reg, 0, READ, &value);
		priority = !!(value & BIT(dm->dtmb_bit));
		s += sprintf(buf + s, DEV_PRIORITY_PRE "%x:%x %x:%x\t\t\t%x:%x %s\n",
				bus_dev->r_priority | (priority << r_bit_s),
				bus_dev->r_width | (1 << r_bit_s),
				bus_dev->w_priority | (priority << w_bit_s),
				bus_dev->w_width | (1 << w_bit_s),
				priority,
				1,
				dm->dtmb_name);
		if (v2->cur_dmc.force_enable)
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x \t%s\n",
				v2->cur_dmc.r_priority,
				v2->cur_dmc.w_priority,
				dm->port_id,
				dm->dtmb_name);
		else
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x \t%s\n",
				bus_dev->r_priority | (priority << r_bit_s),
				bus_dev->w_priority | (priority << w_bit_s),
				dm->port_id,
				dm->dtmb_name);
		return s;
	} else if (value & BIT(dm->isdbt_mode_bit)) {
		reg_access(dm->reg_base + dm->isdbt_reg, 0, READ, &value);
		priority = !!(value & BIT(dm->isdbt_bit));
		s += sprintf(buf + s, DEV_PRIORITY_PRE "%x:%x %x:%x\t\t\t%x:%x %s\n",
				bus_dev->r_priority | (priority << r_bit_s),
				bus_dev->r_width | (1 << r_bit_s),
				bus_dev->w_priority | (priority << w_bit_s),
				bus_dev->w_width | (1 << w_bit_s),
				priority,
				1,
				dm->isdbt_name);
		if (v2->cur_dmc.force_enable)
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x \t%s\n",
				v2->cur_dmc.r_priority,
				v2->cur_dmc.w_priority,
				dm->port_id,
				dm->isdbt_name);
		else
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x \t%s\n",
				bus_dev->r_priority | (priority << r_bit_s),
				bus_dev->w_priority | (priority << w_bit_s),
				dm->port_id,
				dm->isdbt_name);
		return s;
	}

	reg_access(dm->reg_base + dm->mode_reg2, 0, READ, &value);
	if (value & BIT(dm->dvb_t2_mode_bit)) {
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, READ, &dvb_t2_mode);
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, WRITE, &dm->dvb_t2_select_mode);
		reg_access(dm->reg_base + dm->dvb_t2_reg, 0, READ, &value);
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, WRITE, &dvb_t2_mode);
		priority = !!(value & BIT(dm->dvb_t2_bit));
		s += sprintf(buf + s, DEV_PRIORITY_PRE "%x:%x %x:%x\t\t\t%x:%x %s\n",
				bus_dev->r_priority | (priority << r_bit_s),
				bus_dev->r_width | (1 << r_bit_s),
				bus_dev->w_priority | (priority << w_bit_s),
				bus_dev->w_width | (1 << w_bit_s),
				priority,
				1,
				dm->dvb_t2_name);
		if (v2->cur_dmc.force_enable)
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x \t%s\n",
				v2->cur_dmc.r_priority,
				v2->cur_dmc.w_priority,
				dm->port_id,
				dm->dvb_t2_name);
		else
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x \t%s\n",
				bus_dev->r_width | (1 << r_bit_s),
				bus_dev->w_priority | (priority << w_bit_s),
				dm->port_id,
				dm->dvb_t2_name);
		return s;
	}

#define DEMOD_INACTIVE_DDR_MSG \
	DEV_PRIORITY_PRE "\t\t\tonly ISDBT, DVB-T2, and DTMB modes access DDR, " \
	"currently none of these modes is active.\n"

	s += sprintf(buf + s, DEMOD_INACTIVE_DDR_MSG);
	return s;
}

static int get_demux_priority_info(char *buf)
{
	int s = 0;
	unsigned int value;
	struct demux_priority *demux;
	unsigned char w_priority;
	unsigned char r_priority;
	struct bus_dev_priority *bus_dev;
	struct ddr_priority_v2 *v2;

	demux = aml_db->priority->v2->demux;
	if (!demux->allow_access) {
		s += sprintf(buf + s, DEV_PRIORITY_PRE "\t\t\tinaccessible\n");
		return s;
	}

	v2 = aml_db->priority->v2;
	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	reg_access(demux->reg, 0, READ, &value);
	w_priority = (value >> demux->w_bit_s) & demux->w_width;
	r_priority = (value >> demux->r_bit_s) & demux->r_width;
	s += sprintf(buf + s, DEV_PRIORITY_PRE "%x:%x %x:%x\t\t\t%x:%x\t%x:%x\n",
			r_priority,
			demux->r_width,
			w_priority,
			demux->w_width,
			r_priority,
			demux->r_width,
			w_priority,
			demux->w_width);
	if (v2->cur_dmc.force_enable)
		v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
			"%x %x \t%2x \t%s\n",
			v2->cur_dmc.r_priority,
			v2->cur_dmc.w_priority,
			demux->port_id,
			get_name_by_port_id(demux->port_id));
	else
		v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
			"%x %x \t%2x \t%s\n",
			r_priority,
			w_priority,
			demux->port_id,
			get_name_by_port_id(demux->port_id));
	return s;
}

static int get_audio_priority_info(char *buf)
{
	int s = 0;
	int i;
	unsigned int value;
	unsigned int audio_x;
	struct audio_priority *audio;
	struct bus_dev_priority *bus_dev;
	struct ddr_priority_v2 *v2;

	audio = aml_db->priority->v2->audio;
	if (!audio->allow_access) {
		s += sprintf(buf + s, DEV_PRIORITY_PRE "\t\t\tinaccessible\n");
		return s;
	}

	v2 = aml_db->priority->v2;
	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	reg_access(audio->reg_base + audio->reg, 0, READ, &value);
	for (i = 0; i < audio->num; i++) {
		reg_access(audio->reg_base + audio->audio_x[i].reg, 0, READ, &audio_x);
		s += sprintf(buf + s, DEV_PRIORITY_PRE "%x:%x %x:%x\t\t\t%x:%x\t%x:%x %d %s\n",
			(value >> audio->r_bit_s[audio_x & BIT(0)]) & audio->width,
			audio->width,
			(value >> audio->w_bit_s[audio_x & BIT(0)]) & audio->width,
			audio->width,
			(value >> audio->r_bit_s[audio_x & BIT(0)]) & audio->width,
			audio->width,
			(value >> audio->w_bit_s[audio_x & BIT(0)]) & audio->width,
			audio->width,
			audio->audio_x[i].id,
			audio->audio_x[i].name);
		if (v2->cur_dmc.force_enable)
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x %3x\t%s\n",
				v2->cur_dmc.r_priority,
				v2->cur_dmc.w_priority,
				audio->port_id,
				i,
				audio->audio_x[i].name);
		else
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %x \t%2x %3x\t%s\n",
				(value >> audio->r_bit_s[audio_x & BIT(0)]) & audio->width,
				(value >> audio->w_bit_s[audio_x & BIT(0)]) & audio->width,
				audio->port_id,
				i,
				audio->audio_x[i].name);
	}
	return s;
}

static int get_bcon_priority_info(char *buf)
{
	int s = 0;
	unsigned int value;
	struct bcon_priority *bcon;
	struct bus_dev_priority *bus_dev;
	unsigned char priority;
	unsigned char r_bit_s;
	struct ddr_priority_v2 *v2;

	bcon = aml_db->priority->v2->bcon;
	if (!bcon->allow_access) {
		s += sprintf(buf + s, DEV_PRIORITY_PRE "\t\t\tinaccessible\n");
		return s;
	}

	v2 = aml_db->priority->v2;
	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	r_bit_s = hweight8(bus_dev->r_width);
	reg_access(bcon->reg, 0, READ, &value);
	if (value & BIT(bcon->auto_bit)) {
		s += sprintf(buf + s, DEV_PRIORITY_PRE
			"\t\t\tauto urgent, 0: fifo len >= %x, 1: fifo len < %x\n",
			(value >> bcon->high_level_s) & bcon->level_width,
			(value >> bcon->low_level_s) & bcon->level_width);
	} else {
		priority = !!(value >> BIT(bcon->urgent_bit));
		s += sprintf(buf + s, DEV_PRIORITY_PRE "%x:%x %c:%c\t\t\t%x:%x\n",
				bus_dev->r_priority | (priority << r_bit_s),
				bus_dev->r_width | (1 << r_bit_s),
				'-',
				'-',
				priority,
				1);
		if (v2->cur_dmc.force_enable)
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %c \t%2x \t%s\n",
				v2->cur_dmc.r_priority,
				'-',
				bcon->port_id,
				get_name_by_port_id(bcon->port_id));
		else
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%x %c \t%2x \t%s\n",
				bus_dev->r_priority | (priority << r_bit_s),
				'-',
				bcon->port_id,
				get_name_by_port_id(bcon->port_id));
	}
	return s;
}

static int get_hevc_hcodec_priority_info(struct hevc_hcodec_priority *h, char *buf)
{
	int s = 0;
	int i, j, k;
	unsigned int def;
	unsigned char *id;
	unsigned int wl[3][2];
	unsigned int rl[3][2];
	unsigned char def_w_priority;
	unsigned char def_r_priority;
	unsigned char bit_s;
	unsigned char handled;
	struct bus_dev_priority *bus_dev;
	struct ddr_priority_v2 *v2;

	if (!h->allow_access) {
		s += sprintf(buf + s, DEV_PRIORITY_PRE "\t\t\tinaccessible\n");
		return s;
	}

	v2 = aml_db->priority->v2;
	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	bit_s = hweight8(h->def_w_width);
	reg_access(h->reg_base + h->def_offset, 0, READ, &def);
	def_w_priority = (def >> h->def_w_bit_s) & h->def_w_width;
	def_r_priority = (def >> h->def_r_bit_s) & h->def_r_width;
	reg_access(h->reg_base + h->w_lv1_offset, 0, READ, &wl[0][0]);
	reg_access(h->reg_base + h->w_lv1_offset + 4, 0, READ, &wl[0][1]);
	reg_access(h->reg_base + h->w_lv2_offset, 0, READ, &wl[1][0]);
	reg_access(h->reg_base + h->w_lv2_offset + 4, 0, READ, &wl[1][1]);
	reg_access(h->reg_base + h->w_lv3_offset, 0, READ, &wl[2][0]);
	reg_access(h->reg_base + h->w_lv3_offset + 4, 0, READ, &wl[2][1]);
	reg_access(h->reg_base + h->r_lv1_offset, 0, READ, &rl[0][0]);
	reg_access(h->reg_base + h->r_lv1_offset + 4, 0, READ, &rl[0][1]);
	reg_access(h->reg_base + h->r_lv2_offset, 0, READ, &rl[1][0]);
	reg_access(h->reg_base + h->r_lv2_offset + 4, 0, READ, &rl[1][1]);
	reg_access(h->reg_base + h->r_lv3_offset, 0, READ, &rl[2][0]);
	reg_access(h->reg_base + h->r_lv3_offset + 4, 0, READ, &rl[2][1]);

	for (i = 0; i < h->r_num; i++) {
		handled = 0;
		for (j = 0; j < 3; j++) {
			id = (unsigned char *)&rl[j][0];
			for (k = 0; k < 8; k++) {
				if (id[k] == h->r_id_name[i].id) {
					s += sprintf(buf + s, DEV_PRIORITY_PRE
							"%x:%x %c:%c\t\t\t%x:%x %3x %s\n",
							((j + 1) << bit_s) | def_r_priority,
							0xf,
							'-',
							'-',
							((j + 1) << bit_s) | def_r_priority,
							0xf,
							h->r_id_name[i].id,
							h->r_id_name[i].name);
					if (v2->cur_dmc.force_enable)
						v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
							"%x %c \t%2x %3x\t%s\n",
							v2->cur_dmc.r_priority,
							'-',
							h->port_id,
							h->r_id_name[i].id,
							h->r_id_name[i].name);
					else
						v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
							"%x %c \t%2x %3x\t%s\n",
							((j + 1) << bit_s) | def_r_priority,
							'-',
							h->port_id,
							h->r_id_name[i].id,
							h->r_id_name[i].name);
					handled = 1;
					break;
				}
			}
			if (handled)
				break;
		}
		if (handled == 0) {
			s += sprintf(buf + s, DEV_PRIORITY_PRE
					"%x:%x %c:%c\t\t\t%x:%x %3x %s\n",
					def_r_priority,
					0xf,
					'-',
					'-',
					def_r_priority,
					0xf,
					h->r_id_name[i].id,
					h->r_id_name[i].name);
			if (v2->cur_dmc.force_enable)
				v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
					"%x %c \t%2x %3x\t%s\n",
					v2->cur_dmc.r_priority,
					'-',
					h->port_id,
					h->r_id_name[i].id,
					h->r_id_name[i].name);
			else
				v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
					"%x %c \t%2x %3x\t%s\n",
					def_r_priority,
					'-',
					h->port_id,
					h->r_id_name[i].id,
					h->r_id_name[i].name);
		}
	}
	for (i = 0; i < h->w_num; i++) {
		handled = 0;
		for (j = 0; j < 3; j++) {
			id = (unsigned char *)&wl[j][0];
			for (k = 0; k < 8; k++) {
				if (id[k] == h->w_id_name[i].id) {
					s += sprintf(buf + s, DEV_PRIORITY_PRE
							"%c:%c %x:%x\t\t\t%x:%x %3x %s\n",
							'-',
							'-',
							((j + 1) << bit_s) | def_w_priority,
							0xf,
							((j + 1) << bit_s) | def_w_priority,
							0xf,
							h->w_id_name[i].id,
							h->w_id_name[i].name);
					if (v2->cur_dmc.force_enable)
						v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
							"%c %x \t%2x %3x\t%s\n",
							'-',
							v2->cur_dmc.w_priority,
							h->port_id,
							h->w_id_name[i].id,
							h->w_id_name[i].name);
					else
						v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
							"%c %x \t%2x %3x\t%s\n",
							'-',
							((j + 1) << bit_s) | def_w_priority,
							h->port_id,
							h->w_id_name[i].id,
							h->w_id_name[i].name);
					handled = 1;
					break;
				}
			}
			if (handled)
				break;
		}
		if (handled == 0) {
			s += sprintf(buf + s, DEV_PRIORITY_PRE
					"%c:%c %x:%x\t\t\t%x:%x %3x %s\n",
					'-',
					'-',
					def_w_priority,
					0xf,
					def_w_priority,
					0xf,
					h->w_id_name[i].id,
					h->w_id_name[i].name);
			if (v2->cur_dmc.force_enable)
				v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
					"%c %x \t%2x %3x\t%s\n",
					'-',
					v2->cur_dmc.w_priority,
					h->port_id,
					h->w_id_name[i].id,
					h->w_id_name[i].name);
			else
				v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
					"%c %x \t%2x %3x\t%s\n",
					'-',
					def_w_priority,
					h->port_id,
					h->w_id_name[i].id,
					h->w_id_name[i].name);
		}
	}
	return s;
}

static int get_vpu_super_urgent_info(unsigned char axi, unsigned char rw, char *buf)
{
	int s = 0;
	int i;
	struct vpu_super_urgent *sur;
	unsigned int value;
	unsigned int reg_base;
	unsigned char bit_s;
	unsigned char width;
	unsigned char priority;
	struct bus_dev_priority *bus_dev;

	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	sur = aml_db->priority->v2->vpu->sur;
	bit_s = hweight8(bus_dev->w_width);
	width = bus_dev->w_width;

	reg_base = aml_db->priority->v2->vpu->reg_base;
	reg_access(reg_base + sur->addr, 0, READ, &value);
	for (i = 0; i < sur->num; i++) {
		if (axi == sur->axi[i].axi && rw == sur->axi[i].rw) {
			sur->cur = i;
			if (value & BIT(sur->axi[i].en_bit)) {
				priority = (value >> sur->axi[i].ur_bit_s) & sur->axi[i].ur_bit_w;
				sur->axi[i].priority = priority;
				sur->axi[i].enable = 1;
				s += sprintf(buf + s, DEV_PRIORITY_PRE
						"%c:%c %c:%c\t\t\t%x:%x super vpu%d_%c\n",
						rw ? '-' : hex_digit(bus_dev->r_priority | (priority << bit_s)),
						rw ? '-' : hex_digit(width | (sur->axi[i].ur_bit_w << bit_s)),
						rw ? hex_digit(bus_dev->w_priority | (priority << bit_s)) : '-',
						rw ? hex_digit(width | (sur->axi[i].ur_bit_w << bit_s)) : '-',
						priority,
						sur->axi[i].ur_bit_w,
						axi,
						rw ? 'w' : 'r');
			} else {
				sur->axi[i].priority = 0;
				sur->axi[i].enable = 0;
				s += sprintf(buf + s, DEV_PRIORITY_PRE
						"%c:%c %c:%c\t\t\t%x:%x super vpu%d_%c\n",
						rw ? '-' : hex_digit(bus_dev->r_priority),
						rw ? '-' : hex_digit(width),
						rw ? hex_digit(bus_dev->w_priority) : '-',
						rw ? hex_digit(width) : '-',
						0,
						sur->axi[i].ur_bit_w,
						axi,
						rw ? 'w' : 'r');
			}
			return s;
		}
	}
	return s;
}

static char *get_vpu_name(unsigned char axi, unsigned char rw, unsigned char sid)
{
	char buf[5];
	char *name;

	sprintf(buf, "VPU%d", axi);
	buf[4] = 0;
	name = vpu_to_sub_port(buf, rw ? 'w' : 'r', sid);
	if (!name)
		name = vpu_to_sub_port(buf, rw ? 'w' : 'r', sid & 0x0f);
	return name;
}

static void handle_routing(struct vpu_top *top)
{
	int i, j;
	unsigned int value;
	unsigned int reg_base;
	unsigned char routing_num;
	struct vpu_dev_bus_routing *routing;

	reg_base = aml_db->priority->v2->vpu->reg_base;
	routing_num = aml_db->priority->v2->vpu->routing_num;
	routing = aml_db->priority->v2->vpu->routing;
	for (i = 0; i < routing_num; i++) {
		if (top->axi != routing[i].axi || top->rw != routing[i].rw)
			continue;
		reg_access(reg_base + routing[i].addr, 0, READ, &value);
		for (j = 0; j < routing[i].num; j++) {
			if (value & BIT(routing[i].map[j].bit)) {
				top->map[routing[i].idx0].dev[j].flag &= ~BIT(VPU_URGENT_MUX);
				top->map[routing[i].idx1].dev[j].flag |= BIT(VPU_URGENT_MUX);
			} else {
				top->map[routing[i].idx0].dev[j].flag &= ~BIT(VPU_URGENT_MUX);
				top->map[routing[i].idx1].dev[j].flag |= BIT(VPU_URGENT_MUX);
			}
		}
	}
}

static void vpu_cal_dev_sur_from_top_priority(struct vpu_top *top, unsigned char top_priority,
				       unsigned char *sur_priority, unsigned char *dev_priority)
{
	unsigned char bit_s;
	struct vpu_super_urgent *sur;
	struct bus_dev_priority *bus_dev;

	sur = aml_db->priority->v2->vpu->sur;
	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	bit_s = hweight8(bus_dev->w_width);
	if (sur->axi[sur->cur].enable) {
		if (top_priority)
			if (top->axi == 2)
				*sur_priority = (sur->axi[sur->cur].priority + 1) >
						sur->axi[sur->cur].ur_bit_w ?
						sur->axi[sur->cur].ur_bit_w :
						(sur->axi[sur->cur].priority + 1);
			else
				*sur_priority = (sur->axi[sur->cur].priority + top_priority) >=
						sur->axi[sur->cur].ur_bit_w ?
						sur->axi[sur->cur].ur_bit_w :
						(sur->axi[sur->cur].priority + top_priority);

		else
			*sur_priority = sur->axi[sur->cur].priority;
		if (top->rw)
			*dev_priority = bus_dev->w_priority | (*sur_priority << bit_s);
		else
			*dev_priority = bus_dev->r_priority | (*sur_priority << bit_s);
	} else {
		*sur_priority = 0;
		if (top->rw)
			*dev_priority = bus_dev->w_priority | (top_priority << bit_s);
		else
			*dev_priority = bus_dev->r_priority | (top_priority << bit_s);
	}
}

static void vpu_cal_dev_sur_top(struct vpu_top *top, struct vpu_top_map *map,
				struct vpu_urgent *dev, unsigned char *priority,
				unsigned char *top_priority, unsigned char *sur_priority,
				unsigned char *dev_priority)
{
	unsigned int reg_base;
	unsigned int value;

	*top_priority = map->priority;
	reg_base = aml_db->priority->v2->vpu->reg_base;
	if (dev->ur_bit_s == 0xff) {
		*priority = 0;
	} else if (dev->ur_bit_s == 0xfe) {
		*priority = 'h';
	} else if (dev->ur_bit_s == 0xfd) {
		*priority = 's';
	} else {
		reg_access(reg_base + dev->addr, 0, READ, &value);
		*priority = (value >> dev->ur_bit_s) & dev->ur_bit_w;
		if (*priority)
			*top_priority = (*top_priority + 1) >  map->ur_bit_w ?
					map->ur_bit_w : (*top_priority + 1);
	}
	vpu_cal_dev_sur_from_top_priority(top, *top_priority, sur_priority, dev_priority);
}

static int get_vpu_dev_info(struct vpu_top *top, struct vpu_top_map *map, char *buf)
{
	int s = 0;
	int i, j, k, l;
	int n;
	unsigned char priority;
	unsigned char top_priority;
	unsigned char sur_priority;
	unsigned char dev_priority;
	unsigned char rw = top->rw;
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	for (i = 0; i < map->num; i++) {
		for (j = i; j < map->num; j++) {
			if (!(map->dev[j].flag & BIT(VPU_URGENT_HANDLED) || map->dev[j].flag & BIT(VPU_URGENT_MUX)))
				break;
		}
		if (j == map->num)
			break;
		l = j;
		k = map->dev[j].level & 0xf;
		n = 0;
		for (j = j + 1; j < map->num; j++) {
			if (!(map->dev[j].flag & BIT(VPU_URGENT_HANDLED) || map->dev[j].flag & BIT(VPU_URGENT_MUX)) &&
					((map->dev[j].level & 0xf) == k)) {
				n++;
				break;
			}
		}
		j = l;
		if (n == 1) {
			for (; j < map->num; j++) {
				if (!(map->dev[j].flag & BIT(VPU_URGENT_HANDLED) || map->dev[j].flag & BIT(VPU_URGENT_MUX)) &&
						((map->dev[j].level & 0xf) == k)) {
					vpu_cal_dev_sur_top(top, map, &map->dev[j], &priority, &top_priority,
							    &sur_priority, &dev_priority);
					s += sprintf(buf + s, DEV_PRIORITY_PRE
						"%c:%c %c:%c\t\t\t%x:%x\t%x:%x",
						rw ? '-' : hex_digit(dev_priority),
						rw ? '-' : hex_digit(0xf),
						rw ? hex_digit(dev_priority) : '-',
						rw ? hex_digit(0xf) : '-',
						sur_priority,
						3,
						top_priority,
						map->ur_bit_w);
					if (priority > map->dev[j].ur_bit_w)
						s += sprintf(buf + s, "\t\t%c:%x %2x %s\n",
							priority,
							map->dev[j].ur_bit_w,
							map->dev[j].id,
							get_vpu_name(top->axi, rw, map->dev[j].id));
					else
						s += sprintf(buf + s, "\t\t%x:%x %2x %s\n",
							priority,
							map->dev[j].ur_bit_w,
							map->dev[j].id,
							get_vpu_name(top->axi, rw, map->dev[j].id));
					if (v2->cur_dmc.force_enable)
						v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
							"%c %c \t%2x %3x\t%s\n",
							rw ? '-' : hex_digit(v2->cur_dmc.r_priority),
							rw ? hex_digit(v2->cur_dmc.w_priority) : '-',
							top->port_id,
							map->dev[j].id,
							get_vpu_name(top->axi, rw, map->dev[j].id));
					else if (priority <= map->dev[j].ur_bit_w)
						v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
							"%c %c \t%2x %3x\t%s\n",
							rw ? '-' : hex_digit(dev_priority),
							rw ? hex_digit(dev_priority) : '-',
							top->port_id,
							map->dev[j].id,
							get_vpu_name(top->axi, rw, map->dev[j].id));
					map->dev[j].flag |= BIT(VPU_URGENT_HANDLED);
				}
			}
		} else {
			vpu_cal_dev_sur_top(top, map, &map->dev[j], &priority, &top_priority,
					    &sur_priority, &dev_priority);
			s += sprintf(buf + s, DEV_PRIORITY_PRE
				"%c:%c %c:%c\t\t\t%x:%x\t%x:%x",
				rw ? '-' : hex_digit(dev_priority),
				rw ? '-' : 0xf,
				rw ? hex_digit(dev_priority) : '-',
				rw ? hex_digit(0xf) : '-',
				sur_priority,
				3,
				top_priority,
				map->ur_bit_w);
			if (priority > map->dev[j].ur_bit_w)
				s += sprintf(buf + s, "\t%c:%x %2x %s\n",
					priority,
					map->dev[j].ur_bit_w,
					map->dev[j].id,
					get_vpu_name(top->axi, rw, map->dev[j].id));
			else
				s += sprintf(buf + s, "\t%x:%x %2x %s\n",
					priority,
					map->dev[j].ur_bit_w,
					map->dev[j].id,
					get_vpu_name(top->axi, rw, map->dev[j].id));
			if (v2->cur_dmc.force_enable)
				v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
					"%c %c \t%2x %3x\t%s\n",
					rw ? '-' : hex_digit(v2->cur_dmc.r_priority),
					rw ? hex_digit(v2->cur_dmc.w_priority) : '-',
					top->port_id,
					map->dev[j].id,
					get_vpu_name(top->axi, rw, map->dev[j].id));
			else if (priority <= map->dev[j].ur_bit_w)
				v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
					"%c %c \t%2x %3x\t%s\n",
					rw ? '-' : hex_digit(dev_priority),
					rw ? hex_digit(dev_priority) : '-',
					top->port_id,
					map->dev[j].id,
					get_vpu_name(top->axi, rw, map->dev[j].id));
			map->dev[j].flag |= BIT(VPU_URGENT_HANDLED);
		}
	}
	for (i = 0; i < map->num; i++)
		map->dev[i].flag &= ~BIT(VPU_URGENT_HANDLED);

	return s;
}

static int get_vpu_top_info(struct vpu_top *top, char *buf)
{
	int s = 0;
	int i, j;
	unsigned int top_reg;
	unsigned int reg_base;
	unsigned char rw = top->rw;
	unsigned char top_priority;
	unsigned char sur_priority;
	unsigned char dev_priority;
	unsigned char bit_s;
	struct vpu_super_urgent *sur;
	struct bus_dev_priority *bus_dev;
	struct vpu_top_map *map;

	bus_dev = &aml_db->priority->v2->bus_dev[aml_db->priority->v2->cur_bus_dev];
	bit_s = hweight8(bus_dev->w_width);
	sur = aml_db->priority->v2->vpu->sur;
	reg_base = aml_db->priority->v2->vpu->reg_base;
	reg_access(reg_base + top->addr, 0, READ, &top_reg);
	for (i = 0; i < top->num; i++) {
		map = &top->map[i];
		top_priority = (top_reg >> map->ur_bit_s) & map->ur_bit_w;
		map->priority = top_priority;
		vpu_cal_dev_sur_from_top_priority(top, top_priority, &sur_priority, &dev_priority);
		s += sprintf(buf + s, DEV_PRIORITY_PRE
				"%c:%c %c:%c\t\t\t%x:%x\t%x:%x top%x\n",
				rw ? '-' : hex_digit(dev_priority),
				rw ? '-' : hex_digit(0xf),
				rw ? hex_digit(dev_priority) : '-',
				rw ? hex_digit(0xf) : '-',
				sur_priority,
				3,
				top_priority,
				map->ur_bit_w,
				i);
		if (!map->dev) {
			for (j = 0; j < map->num; j++) {
				s += sprintf(buf + s, DEV_PRIORITY_PRE
					"%c:%c %c:%c\t\t\t%x:%x\t%x:%x %2x %s\n",
					rw ? '-' : hex_digit(dev_priority),
					rw ? '-' : hex_digit(0xf),
					rw ? hex_digit(dev_priority) : '-',
					rw ? hex_digit(0xf) : '-',
					sur_priority,
					3,
					top_priority,
					map->ur_bit_w,
					map->id[j],
					get_vpu_name(top->axi, rw, map->id[j]));
			}
		} else {
			s += get_vpu_dev_info(top, map, buf + s);
		}
	}
	return s;
}

static int get_vpu_priority_info(unsigned char port_id, char *buf)
{
	int s = 0;
	int i;
	struct vpu_priority *vpu;
	struct vpu_super_urgent *sur;
	struct vpu_top *top;

	vpu = aml_db->priority->v2->vpu;
	sur = vpu->sur;
	top = vpu->top;
	for (i = 0; i < vpu->num; i++) {
		if (port_id == top[i].port_id) {
			if (!vpu->allow_access) {
				s += sprintf(buf + s, DEV_PRIORITY_PRE "\t\t\tinaccessible\n");
				return s;
			}
			handle_routing(&top[i]);
			s += get_vpu_super_urgent_info(top[i].axi, top[i].rw, buf + s);
			s += get_vpu_top_info(&top[i], buf + s);
		}
	}
	return s;
}

static int dev_priority_display(int port_id, char *buf)
{
	int s = 0;
	struct ddr_priority_v2 *v2;
	struct bus_dev_priority *bus_dev;

	v2 = aml_db->priority->v2;
	bus_dev = &v2->bus_dev[v2->cur_bus_dev];

	if (port_id == v2->demod->port_id) {
		s += get_demod_priority_info(buf + s);
		return s;
	}
	if (port_id == v2->demux->port_id) {
		s += get_demux_priority_info(buf + s);
		return s;
	}
	if (port_id == v2->audio->port_id) {
		s += get_audio_priority_info(buf + s);
		return s;
	}
	if (port_id == v2->bcon->port_id) {
		s += get_bcon_priority_info(buf + s);
		return s;
	}
	if (port_id == v2->hevc->port_id) {
		s += get_hevc_hcodec_priority_info(v2->hevc, buf + s);
		return s;
	}
	if (port_id == v2->hcodec->port_id) {
		s += get_hevc_hcodec_priority_info(v2->hcodec, buf + s);
		return s;
	}
	s += get_vpu_priority_info(port_id, buf + s);
	if (!s) {
		if (v2->cur_dmc.force_enable)
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%c %c \t%2x \t%s\n",
				bus_dev->r_width ? hex_digit(v2->cur_dmc.r_priority) : '-',
				bus_dev->w_width ? hex_digit(v2->cur_dmc.w_priority) : '-',
				port_id,
				get_name_by_port_id(port_id));
		else
			v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%c %c \t%2x \t%s\n",
				bus_dev->r_width ? hex_digit(bus_dev->r_priority) : '-',
				bus_dev->w_width ? hex_digit(bus_dev->w_priority) : '-',
				port_id,
				get_name_by_port_id(port_id));
	} else if (strstr(buf, "inaccessible"))
		v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
			"n n \t%2x \t%s\n", port_id, get_name_by_port_id(port_id));
	return s;
}

static int bus_dev_priority_display(int idx, char *buf)
{
	int i;
	int s = 0;
	unsigned int value = 0;
	struct bus_dev_priority *bus_dev;

	bus_dev = &aml_db->priority->v2->bus_dev[idx];
	aml_db->priority->v2->cur_bus_dev = idx;
	if (bus_dev->w_width) {
		reg_access(bus_dev->reg_base + bus_dev->w_offset, 0, READ, &value);
		bus_dev->w_priority = (value >> bus_dev->w_bit_s) & bus_dev->w_width;
	}
	if (bus_dev->r_width) {
		reg_access(bus_dev->reg_base + bus_dev->r_offset, 0, READ, &value);
		bus_dev->r_priority = (value >> bus_dev->r_bit_s) & bus_dev->r_width;
	}

	s += sprintf(buf + s, "\t\t%c:%c %c:%c %2x %-12s\n",
			bus_dev->r_width ? hex_digit(bus_dev->r_priority) : '-',
			bus_dev->r_width ? hex_digit(bus_dev->r_width) : '-',
			bus_dev->w_width ? hex_digit(bus_dev->w_priority) : '-',
			bus_dev->w_width ? hex_digit(bus_dev->w_width) : '-',
			bus_dev->port_id,
			get_name_by_port_id(bus_dev->port_id));
	s += dev_priority_display(bus_dev->port_id, buf + s);
	for (i = 0; i < bus_dev->num; i++) {
		s += sprintf(buf + s, "\t\t        %2x %-12s\n",
				bus_dev->share_dev[i],
				get_name_by_port_id(bus_dev->share_dev[i]));
		s += dev_priority_display(bus_dev->share_dev[i], buf + s);
	}

	return s;
}

#define SPLIT_LINE "------------------------------------------------------------------------------\n"
static int priority_v2_display(char *buf, loff_t pos, size_t count)
{
	int i, j, s = 0;
	struct ddr_priority_v2 *v2;
	struct ddr_port_desc *port_desc;
	unsigned short real_ports;
	char *detailed_buf;

	v2 = aml_db->priority->v2;
	if (!pos) {
		port_desc = aml_db->port_desc;
		real_ports = aml_db->real_ports;

		v2->simplify_index = 0;
		if (!v2->simplify_buf)
			return -ENOMEM;
		v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"version: %d\n", aml_db->priority->ver);
		v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				SPLIT_LINE);
		v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"r w \tid sid\tname\n");
		detailed_buf = kmalloc(0x3000, GFP_KERNEL);
		if (!detailed_buf)
			return -ENOMEM;
		s += sprintf(detailed_buf + s, SPLIT_LINE);
		s += sprintf(detailed_buf + s, "dmc\t\tdevice\t\t\tinside device\n");
		s += sprintf(detailed_buf + s, " b f r:m w:m\tr:m w:m\tid name\t\n");
		s += sprintf(detailed_buf + s, SPLIT_LINE);
		for (i = 0; i < v2->dmc_num; i++) {
			s += dmc_priority_display(i, detailed_buf + s);
			for (j = 0; j < v2->bus_dev_num; j++) {
				if (v2->dmc[i].bus_id == v2->bus_dev[j].bus_id)
					s += bus_dev_priority_display(j, detailed_buf + s);
			}
		}
		s += sprintf(detailed_buf + s, SPLIT_LINE);
		v2->simplify_index += sprintf(v2->simplify_buf + v2->simplify_index,
				"%s", detailed_buf);
		kfree(detailed_buf);
		s = memory_read_from_buffer(buf, count, &pos, v2->simplify_buf,
				v2->simplify_index);
	} else {
		s = memory_read_from_buffer(buf, count, &pos, v2->simplify_buf,
				v2->simplify_index);
	}
	return s;
}

int priority_display(char *buf, loff_t pos, size_t count)
{
	int i, s = 0, ret;
	int priority_w, priority_r;
	struct ddr_priority_v1 info;

	if (aml_db->priority->ver == 2)
		return priority_v2_display(buf, pos, count);

	if (!pos) {
		s += sprintf(ddr_priority.v1_buf + s, "\nusage Example:\n"
				"\techo 2 0xf (r) > priority\n"
				"\tparm0: port id\n"
				"\tparm1: priority value\n"
				"\tparm2: 'r' or 'w' priority (default set all)\n");

		s += sprintf(ddr_priority.v1_buf + s, "\tid\t name \t\tw_current \tw_max \t\tr_current \tr_max\n");
		for (i = 0; i < aml_db->priority->v1_num; i++) {
			info =  aml_db->priority->v1[i];
			ret = ddr_priority_rw(info.port_id, &priority_r, &priority_w, DMC_READ);

			s += sprintf(ddr_priority.v1_buf + s, "%10d\t %*s ",
					info.port_id,
					-12, priority_find_port_name(info.port_id));
			if (priority_w < 0) {
				s += sprintf(ddr_priority.v1_buf + s, "\t%*s \t%*s ",
						-8, "N/A", -8, "N/A");
			} else {
				s += sprintf(ddr_priority.v1_buf + s, "\t%-8x \t%-8x",
						priority_w, info.w_width & 0xf);
			}

			if (priority_r < 0) {
				s += sprintf(ddr_priority.v1_buf + s, "\t%*s \t%*s\n",
						-8, "N/A", -8, "N/A");
			} else {
				s += sprintf(ddr_priority.v1_buf + s, "\t%-8x \t%-8x\n",
						priority_r, info.r_width  & 0xf);
			}
		}
		ddr_priority.v1_buf_size = s;
		s = memory_read_from_buffer(buf, count, &pos, ddr_priority.v1_buf, ddr_priority.v1_buf_size);
	} else {
		s = memory_read_from_buffer(buf, count, &pos, ddr_priority.v1_buf, ddr_priority.v1_buf_size);
	}
	return s;
}

int get_dmc_priority(unsigned int bus_id, char rw)
{
	unsigned int value;
	int i;
	struct ddr_priority_v2 *v2;
	struct dmc_priority *dmc;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	for (i = 0; i < v2->dmc_num; i++)
		if (v2->dmc[i].bus_id == bus_id)
			break;

	if (i == v2->dmc_num) {
		pr_err("the bus id is out of bounds\n");
		return -EINVAL;
	}
	if (!(rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 'r' or 'w'\n");
		return -EINVAL;
	}

	dmc = &aml_db->priority->v2->dmc[i];
	reg_access(dmc->reg_base + dmc->offset, 0, READ, &value);
	if (rw == 'w')
		value = (value >> dmc->w_bit_s) & dmc->w_bit_s;
	else if (rw == 'r')
		value = (value >> dmc->r_bit_s) & dmc->r_bit_s;

	return value;
}
EXPORT_SYMBOL(get_dmc_priority);

int set_dmc_priority(unsigned int bus_id, unsigned char priority, char rw)
{
	unsigned int value;
	int i;
	struct ddr_priority_v2 *v2;
	struct dmc_priority *dmc;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	for (i = 0; i < v2->dmc_num; i++)
		if (v2->dmc[i].bus_id == bus_id)
			break;

	if (i == v2->dmc_num) {
		pr_err("the bus id is out of bounds\n");
		return -EINVAL;
	}
	if (!(rw == 0 || rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 0, 'r', or 'w'\n");
		return -EINVAL;
	}

	dmc = &aml_db->priority->v2->dmc[i];
	reg_access(dmc->reg_base + dmc->offset, 0, READ, &value);

	if (priority == 0xff) {
		value &= ~dmc->force_enable;
		value &= ~(dmc->width << dmc->w_bit_s);
		value &= ~(dmc->width << dmc->r_bit_s);
	} else {
		if (priority > dmc->width) {
			pr_err("the maximum priority is %x\n", dmc->width);
			return -EINVAL;
		}
		value |= dmc->force_enable;
		if (rw == 'w') {
			value &= ~(dmc->width << dmc->w_bit_s);
			value |= (priority & dmc->width) << dmc->w_bit_s;
		} else if (rw == 'r') {
			value &= ~(dmc->width << dmc->r_bit_s);
			value |= (priority & dmc->width) << dmc->r_bit_s;
		} else if (rw == 0) {
			value &= ~(dmc->width << dmc->w_bit_s);
			value &= ~(dmc->width << dmc->r_bit_s);
			value |= (priority & dmc->width) << dmc->w_bit_s;
			value |= (priority & dmc->width) << dmc->r_bit_s;
		}
	}
	reg_access(dmc->reg_base + dmc->offset, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_dmc_priority);

int get_dev_priority(unsigned int port_id, char rw)
{
	unsigned int value;
	int i;
	struct ddr_priority_v2 *v2;
	struct bus_dev_priority *bus_dev;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	for (i = 0; i < v2->bus_dev_num; i++)
		if (v2->bus_dev[i].port_id == port_id)
			break;

	bus_dev = &aml_db->priority->v2->bus_dev[i];
	if (i == v2->bus_dev_num) {
		pr_err("the port id is out of bounds\n");
		return -EINVAL;
	}
	if (!(rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 'r', or 'w'\n");
		return -EINVAL;
	}

	if (rw == 'w' && bus_dev->w_width) {
		reg_access(bus_dev->reg_base + bus_dev->w_offset, 0, READ, &value);
		value = (value >> bus_dev->w_bit_s) & bus_dev->w_width;
	} else if (rw == 'r' && bus_dev->r_width) {
		reg_access(bus_dev->reg_base + bus_dev->r_offset, 0, READ, &value);
		value = (value >> bus_dev->r_bit_s) & bus_dev->r_width;
	} else {
		pr_err("this device does not support setting %c priority\n", rw);
		return -EINVAL;
	}

	return value;
}
EXPORT_SYMBOL(get_dev_priority);

int set_dev_priority(unsigned int port_id, unsigned char priority, char rw)
{
	unsigned int value;
	int i;
	struct ddr_priority_v2 *v2;
	struct bus_dev_priority *bus_dev;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	for (i = 0; i < v2->bus_dev_num; i++)
		if (v2->bus_dev[i].port_id == port_id)
			break;

	bus_dev = &aml_db->priority->v2->bus_dev[i];
	if (i == v2->bus_dev_num) {
		pr_err("the port id is out of bounds\n");
		return -EINVAL;
	}
	if (!(rw == 0 || rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 0, 'r', or 'w'\n");
		return -EINVAL;
	}

	if (rw == 0 || rw == 'w') {
		if (priority > bus_dev->w_width) {
			pr_err("the maximum write priority is %x\n", bus_dev->w_width);
			return -EINVAL;
		}
		reg_access(bus_dev->reg_base + bus_dev->w_offset, 0, READ, &value);
		value &= ~(bus_dev->w_width << bus_dev->w_bit_s);
		value |= (priority &  bus_dev->w_width) << bus_dev->w_bit_s;
		reg_access(bus_dev->reg_base + bus_dev->w_offset, 0, WRITE, &value);
	}

	if (rw == 0 || rw == 'r') {
		if (priority > bus_dev->r_width) {
			pr_err("the maximum write priority is %x\n", bus_dev->r_width);
			return -EINVAL;
		}
		reg_access(bus_dev->reg_base + bus_dev->r_offset, 0, READ, &value);
		value &= ~(bus_dev->r_width << bus_dev->r_bit_s);
		value |= (priority &  bus_dev->r_width) << bus_dev->r_bit_s;
		reg_access(bus_dev->reg_base + bus_dev->r_offset, 0, WRITE, &value);
	}

	return 0;
}
EXPORT_SYMBOL(set_dev_priority);

int set_vpu_super_priority(unsigned char axi, char rw, unsigned char priority)
{
	struct ddr_priority_v2 *v2;
	struct vpu_priority *vpu;
	struct vpu_super_urgent *sur;
	unsigned int value;
	int i;
	int rw_i;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	vpu = v2->vpu;
	sur = vpu->sur;

	if (!vpu->allow_access) {
		pr_err("the vpu is currently inaccessible\n");
		return -EINVAL;
	}
	if (!sur->exist)
		return -EINVAL;

	if (!(rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 'r', or 'w'\n");
		return -EINVAL;
	}
	if (rw == 'w')
		rw_i = 1;
	else
		rw_i = 0;

	for (i = 0; i < sur->num; i++)
		if (axi == sur->axi[i].axi && rw_i == sur->axi[i].rw)
			break;

	if (i == sur->num) {
		pr_err("no vpu device matches axi and rw with configurable super priority\n");
		return -EINVAL;
	}

	reg_access(vpu->reg_base + sur->addr, 0, READ, &value);
	if (priority == 0xff) {
		value &= ~BIT(sur->axi[i].en_bit);
		value &= ~(sur->axi[i].ur_bit_w << sur->axi[i].ur_bit_s);
	} else {
		if (priority > sur->axi[i].ur_bit_w) {
			pr_err("the maximum priority is %x\n", sur->axi[i].ur_bit_w);
			return -EINVAL;
		}
		value &= ~(sur->axi[i].ur_bit_w << sur->axi[i].ur_bit_s);
		value &= ~BIT(sur->axi[i].en_bit);
		value |= priority << sur->axi[i].ur_bit_s;
		value |= BIT(sur->axi[i].en_bit);
	}
	reg_access(vpu->reg_base + sur->addr, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_vpu_super_priority);

int set_vpu_top_priority(unsigned char axi, char rw, unsigned char top_i, unsigned char priority)
{
	struct ddr_priority_v2 *v2;
	struct vpu_priority *vpu;
	struct vpu_top *top;
	unsigned int value;
	int i;
	int rw_i;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	vpu = v2->vpu;
	if (!vpu->allow_access) {
		pr_err("the vpu is currently inaccessible\n");
		return -EINVAL;
	}
	if (!(rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 'r', or 'w'\n");
		return -EINVAL;
	}
	if (rw == 'w')
		rw_i = 1;
	else
		rw_i = 0;

	for (i = 0; i < vpu->num; i++)
		if (axi == vpu->top[i].axi && rw_i == vpu->top[i].rw)
			break;

	if (i == vpu->num) {
		pr_err("no vpu device matches AXI/RW with configurable priority\n");
		return -EINVAL;
	}

	top = &vpu->top[i];
	if (top_i >= top->num) {
		pr_err("The maximum top_i is %x\n", top->num);
		return -EINVAL;
	}
	i = top_i;

	reg_access(vpu->reg_base + top->addr, 0, READ, &value);
	if (priority > top->map[i].ur_bit_w) {
		pr_err("the maximum priority is %x\n", top->map[i].ur_bit_w);
		return -EINVAL;
	}
	value &= ~(top->map[i].ur_bit_w << top->map[i].ur_bit_s);
	value |= priority << top->map[i].ur_bit_s;
	reg_access(vpu->reg_base + top->addr, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_vpu_top_priority);

int set_vpu_sub_dev_priority(unsigned char axi, char rw, unsigned int id, unsigned char priority)
{
	struct ddr_priority_v2 *v2;
	struct vpu_priority *vpu;
	struct vpu_top *top;
	struct vpu_top_map *map;
	struct vpu_urgent *dev;
	unsigned int value;
	int i, j;
	int rw_i;
	int find = 0;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	vpu = v2->vpu;
	if (!vpu->allow_access) {
		pr_err("the vpu is currently inaccessible\n");
		return -EINVAL;
	}
	if (!(rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 'r', or 'w'\n");
		return -EINVAL;
	}
	if (rw == 'w')
		rw_i = 1;
	else
		rw_i = 0;

	for (i = 0; i < vpu->num; i++)
		if (axi == vpu->top[i].axi && rw_i == vpu->top[i].rw)
			break;

	if (i == vpu->num) {
		pr_err("no vpu device matches AXI/RW with configurable priority\n");
		return -EINVAL;
	}

	top = &vpu->top[i];
	for (i = 0; i < top->num; i++) {
		map = &top->map[i];
		for (j = 0; j < map->num; j++) {
			if (map->dev) {
				if (id == map->dev[j].id) {
					find = 1;
					dev = &map->dev[j];
					break;
				}
			} else {
				if (id == map->id[j]) {
					pr_err("the device doesn't support priority configuration\n");
					return -EINVAL;
				}
			}
		}
	}

	if (!find) {
		pr_err("no device matches the given axi, rw, and id");
		return -EINVAL;
	}

	if (dev->ur_bit_s == 0xff || dev->ur_bit_s == 0xfe || dev->ur_bit_s == 0xfd) {
		pr_err("the device doesn't support priority configuration\n");
		return -EINVAL;
	}

	if (priority > dev->ur_bit_w) {
		pr_err("the maximum priority is %x\n", dev->ur_bit_w);
		return -EINVAL;
	}

	reg_access(vpu->reg_base + dev->addr, 0, READ, &value);
	value &= ~(dev->ur_bit_w << dev->ur_bit_s);
	value |= priority << dev->ur_bit_s;
	reg_access(vpu->reg_base + dev->addr, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_vpu_sub_dev_priority);

int set_demod_priority(unsigned char priority)
{
	unsigned int value;
	unsigned int dvb_t2_mode;
	struct ddr_priority_v2 *v2;
	struct demod_priority *dm;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	dm = v2->demod;
	if (!dm->allow_access) {
		pr_err("the demod is currently inaccessible\n");
		return -EINVAL;
	}

	if (priority > 1) {
		pr_err("the maximum priority is 1\n");
		return -EINVAL;
	}

	reg_access(dm->reg_base + dm->mode_reg1, 0, READ, &value);
	if (value & BIT(dm->dtmb_mode_bit)) {
		reg_access(dm->reg_base + dm->dtmb_reg, 0, READ, &value);
		value &= ~BIT(dm->dtmb_bit);
		if (priority)
			value |= BIT(dm->dtmb_bit);
		reg_access(dm->reg_base + dm->dtmb_reg, 0, WRITE, &value);
		return 0;
	} else if (value & BIT(dm->isdbt_mode_bit)) {
		reg_access(dm->reg_base + dm->isdbt_reg, 0, READ, &value);
		value &= ~BIT(dm->isdbt_bit);
		if (priority)
			value |= BIT(dm->isdbt_bit);
		reg_access(dm->reg_base + dm->isdbt_reg, 0, WRITE, &value);
		return 0;
	}

	reg_access(dm->reg_base + dm->mode_reg2, 0, READ, &value);
	if (value & BIT(dm->dvb_t2_mode_bit)) {
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, READ, &dvb_t2_mode);
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, WRITE, &dm->dvb_t2_select_mode);
		reg_access(dm->reg_base + dm->dvb_t2_reg, 0, READ, &value);
		value &= ~BIT(dm->dvb_t2_bit);
		if (priority)
			value |= BIT(dm->dvb_t2_bit);
		reg_access(dm->reg_base + dm->dvb_t2_reg, 0, WRITE, &value);
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, WRITE, &dvb_t2_mode);
		return 0;
	}

#define DEMOD_CANT_SET_PRIORITY_MSG \
	"the demod can only set its corresponding priority when operating in ISDB-T, DVB-T2, or DTMB mode"
	pr_err("%s\n", DEMOD_CANT_SET_PRIORITY_MSG);

	return -EINVAL;
}
EXPORT_SYMBOL(set_demod_priority);

int set_demux_priority(unsigned char priority, char rw)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct demux_priority *demux;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	demux = v2->demux;
	if (!demux->allow_access) {
		pr_err("the demux is currently inaccessible\n");
		return -EINVAL;
	}

	if (!(rw == 0 || rw == 'w' || rw == 'r')) {
		pr_err("the value of rw must be either 0, 'r', or 'w'\n");
		return -EINVAL;
	}

	reg_access(demux->reg, 0, READ, &value);
	if (rw == 0 || rw == 'r') {
		if (priority > demux->r_width) {
			pr_err("the maximum read priority for demux is %d\n", demux->r_width);
			return -EINVAL;
		}
		value &= ~(demux->r_width << demux->r_bit_s);
		value |= priority << demux->r_bit_s;
	}
	if (rw == 0 || rw == 'w') {
		if (priority > demux->w_width) {
			pr_err("the maximum write priority for demux is %d\n", demux->w_width);
			return -EINVAL;
		}
		value &= ~(demux->w_width << demux->w_bit_s);
		value |= priority << demux->w_bit_s;
	}
	reg_access(demux->reg, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_demux_priority);

int set_audio_x_select_priority(char x, unsigned char ugt)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct audio_priority *audio;
	int i;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	audio = v2->audio;
	if (!audio->allow_access) {
		pr_err("the audio is currently inaccessible\n");
		return -EINVAL;
	}

	i = x - 'a';
	if (i < 0 || i > audio->num) {
		pr_err("audio supports only %d channels: a, b, c, and d, but input %c\n",
			audio->num, i);
		return -1;
	}

	if (ugt > 1) {
		pr_err("the maximum priority is 1\n");
		return -1;
	}

	reg_access(audio->reg_base + audio->audio_x[i].reg, 0, READ, &value);
	value &= ~BIT(0);
	value |= ugt;
	reg_access(audio->reg_base + audio->audio_x[i].reg, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_audio_x_select_priority);

int set_audio_priority(unsigned char idx, unsigned char priority, char rw)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct audio_priority *audio;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	audio = v2->audio;
	if (!audio->allow_access) {
		pr_err("audio is currently inaccessible\n");
		return -EINVAL;
	}
	if (idx >= 4) {
		pr_err("the maximum priority level is 3\n");
		return -EINVAL;
	}
	if (!(rw == 0 || rw == 'r' || rw == 'w')) {
		pr_err("the value of rw must be either 0, 'r', or 'w'\n");
		return -EINVAL;
	}

	if (priority > audio->width) {
		pr_err("the maximum audio priority is %x\n", audio->width);
		return -EINVAL;
	}

	reg_access(audio->reg_base + audio->reg, 0, READ, &value);
	if (rw == 0 || rw == 'r') {
		value &= ~((audio->width) << audio->r_bit_s[idx]);
		value |= priority << audio->r_bit_s[idx];
	}
	if (rw == 0 || rw == 'w') {
		value &= ~((audio->width) << audio->w_bit_s[idx]);
		value |= priority << audio->w_bit_s[idx];
	}
	reg_access(audio->reg_base + audio->reg, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_audio_priority);

int show_audio_priority_setting(void)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct audio_priority *audio;
	int i, j;
	char buf[256];
	int s = 0;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	audio = v2->audio;
	if (!audio->allow_access) {
		pr_err("audio is currently inaccessible\n");
		return -EINVAL;
	}

	pr_info("[i] r:m w:m name");
	for (i = 0; i < audio->num; i++) {
		reg_access(audio->reg_base + audio->audio_x[i].reg, 0, READ, &value);
		audio->audio_x[i].ugt = value & BIT(0);
	}

	reg_access(audio->reg_base + audio->reg, 0, READ, &value);
	for (i = 0; i < 4; i++) {
		s += sprintf(buf + s, "[%x] %x:%x %x:%x", i,
			(value >> audio->r_bit_s[i]) & audio->width, audio->width,
			(value >> audio->w_bit_s[i]) & audio->width, audio->width);
		for (j = 0; j < audio->num; j++) {
			if (i == audio->audio_x[j].ugt)
				s += sprintf(buf + s, " %s", audio->audio_x[j].name);
		}
		s +=  sprintf(buf + s, "\n");
	}
	pr_info("%s\n", buf);

	return 0;
}
EXPORT_SYMBOL(show_audio_priority_setting);

int set_bcon_hw_urgent(unsigned char low, unsigned char high)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct bcon_priority *bcon;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	bcon = v2->bcon;
	if (!bcon->allow_access) {
		pr_err("bcon is currently inaccessible\n");
		return -EINVAL;
	}

	if (!(low == 0 && high == 0)) {
		if (low >= high || low > bcon->level_width || high > bcon->level_width) {
			pr_err("the level parameter for bcon is out of range\n");
			return -EINVAL;
		}
	}

	reg_access(bcon->reg, 0, READ, &value);
	value |= BIT(bcon->auto_bit);
	if (!(low == 0 && high == 0)) {
		value &= ~(bcon->level_width << bcon->low_level_s);
		value |= low << bcon->low_level_s;
		value &= ~(bcon->level_width << bcon->high_level_s);
		value |= high << bcon->high_level_s;
	}
	reg_access(bcon->reg, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_bcon_hw_urgent);

int set_bcon_sw_urgent(unsigned char ugt)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct bcon_priority *bcon;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	bcon = v2->bcon;
	if (!bcon->allow_access) {
		pr_err("bcon is currently inaccessible\n");
		return -EINVAL;
	}

	if (ugt > 1) {
		pr_err("the maximum priority is 1\n");
		return -EINVAL;
	}

	reg_access(bcon->reg, 0, READ, &value);
	value &= ~BIT(bcon->auto_bit);
	value &= ~BIT(bcon->urgent_bit);
	if (ugt)
		value |= BIT(bcon->urgent_bit);
	reg_access(bcon->reg, 0, WRITE, &value);

	return 0;
}
EXPORT_SYMBOL(set_bcon_sw_urgent);

int show_bcon_priority_setting(void)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct bcon_priority *bcon;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	bcon = v2->bcon;
	reg_access(bcon->reg, 0, READ, &value);

	if (value & BIT(bcon->auto_bit))
		pr_info("bcon: hardware auto control urgent: %x-%x\n",
			(value >> bcon->low_level_s) & bcon->level_width,
			(value >> bcon->high_level_s) & bcon->level_width);
	else
		pr_info("bcon: software control urgent: %x\n",
			!!(value & BIT(bcon->urgent_bit)));
	return 0;
}
EXPORT_SYMBOL(show_bcon_priority_setting);

static int set_hevc_hcodec_level_id(struct hevc_hcodec_priority *h, char rw,
				    unsigned char level, unsigned char i, unsigned char id)
{
	unsigned int value;
	unsigned int lv_offset;

	if (!h->allow_access) {
		pr_err("currently inaccessible\n");
		return -EINVAL;
	}

	if (!(level == 1 || level == 2 || level == 3)) {
		pr_err("the level(%d) parameter is out of range(1 to 3)\n", level);
		return -EINVAL;
	}
	if (rw != 'r' && rw != 'w') {
		pr_err("the value of 'rw'(%c) is neither 'r' nor 'w'.\n", rw);
		return -EINVAL;
	}
	if (i >= 8) {
		pr_err("the index(%d) parameter is out of range(0 to 7)\n", i);
		return -EINVAL;
	}

	if (rw == 'r') {
		if (level == 1)
			lv_offset = h->r_lv1_offset;
		else if (level == 2)
			lv_offset = h->r_lv2_offset;
		else
			lv_offset = h->r_lv3_offset;
	} else {
		if (level == 1)
			lv_offset = h->w_lv1_offset;
		else if (level == 2)
			lv_offset = h->w_lv2_offset;
		else
			lv_offset = h->w_lv3_offset;
	}

	lv_offset += (i >> 2) << 2;
	reg_access(h->reg_base + lv_offset, 0, READ, &value);
	value &= ~(0xff << (i % 4) * 8);
	value |= id << (i % 4) * 8;
	reg_access(h->reg_base + lv_offset, 0, WRITE, &value);

	return 0;
}

int set_hevc_level_id(char rw, unsigned char level, unsigned char i, unsigned char id)
{
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	return set_hevc_hcodec_level_id(v2->hevc, rw, level, i, id);
}
EXPORT_SYMBOL(set_hevc_level_id);

int set_hcodec_level_id(char rw, unsigned char level, unsigned char i, unsigned char id)
{
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	return set_hevc_hcodec_level_id(v2->hcodec, rw, level, i, id);
}
EXPORT_SYMBOL(set_hcodec_level_id);

static int set_hevc_hcodec_low_priority(struct hevc_hcodec_priority *h, char rw,
					unsigned char priority)
{
	unsigned int value;

	if (!h->allow_access) {
		pr_err("currently inaccessible\n");
		return -EINVAL;
	}
	if (!(rw == 'r' || rw == 'w')) {
		pr_err("the value of 'rw'(%c) is neither 'r' nor 'w'.\n", rw);
		return -EINVAL;
	}
	if ((rw == 'r') && priority > h->def_r_width) {
		pr_err("the read priority(%x) parameter is out of range(0 to %x)\n",
			priority, h->def_r_width);
		return -EINVAL;
	}
	if ((rw == 'w') && priority > h->def_w_width) {
		pr_err("the write priority(%x) parameter is out of range(0 to %x)\n",
			priority, h->def_w_width);
		return -EINVAL;
	}

	reg_access(h->reg_base + h->def_offset, 0, READ, &value);
	if (rw == 'r') {
		value &= ~(h->def_r_width << h->def_r_bit_s);
		value |= priority << h->def_r_bit_s;
	} else {
		value &= ~(h->def_w_width << h->def_w_bit_s);
		value |= priority << h->def_w_bit_s;
	}
	reg_access(h->reg_base + h->def_offset, 0, WRITE, &value);

	return 0;
}

int set_hevc_low_priority(char rw, unsigned char priority)
{
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	return set_hevc_hcodec_low_priority(v2->hevc, rw, priority);
}
EXPORT_SYMBOL(set_hevc_low_priority);

int set_hcodec_low_priority(char rw, unsigned char priority)
{
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	return set_hevc_hcodec_low_priority(v2->hcodec, rw, priority);
}
EXPORT_SYMBOL(set_hcodec_low_priority);

static int show_hevc_hcodec_priority_setting(struct hevc_hcodec_priority *h)
{
	int i, j, k;
	unsigned int def;
	unsigned int wl[3][2];
	unsigned int rl[3][2];
	unsigned char def_w_priority;
	unsigned char def_r_priority;
	unsigned char *id;

	if (!h->allow_access) {
		pr_err("currently inaccessible\n");
		return -EINVAL;
	}
	reg_access(h->reg_base + h->def_offset, 0, READ, &def);
	def_w_priority = (def >> h->def_w_bit_s) & h->def_w_width;
	def_r_priority = (def >> h->def_r_bit_s) & h->def_r_width;
	reg_access(h->reg_base + h->w_lv1_offset, 0, READ, &wl[0][0]);
	reg_access(h->reg_base + h->w_lv1_offset + 4, 0, READ, &wl[0][1]);
	reg_access(h->reg_base + h->w_lv2_offset, 0, READ, &wl[1][0]);
	reg_access(h->reg_base + h->w_lv2_offset + 4, 0, READ, &wl[1][1]);
	reg_access(h->reg_base + h->w_lv3_offset, 0, READ, &wl[2][0]);
	reg_access(h->reg_base + h->w_lv3_offset + 4, 0, READ, &wl[2][1]);
	reg_access(h->reg_base + h->r_lv1_offset, 0, READ, &rl[0][0]);
	reg_access(h->reg_base + h->r_lv1_offset + 4, 0, READ, &rl[0][1]);
	reg_access(h->reg_base + h->r_lv2_offset, 0, READ, &rl[1][0]);
	reg_access(h->reg_base + h->r_lv2_offset + 4, 0, READ, &rl[1][1]);
	reg_access(h->reg_base + h->r_lv3_offset, 0, READ, &rl[2][0]);
	reg_access(h->reg_base + h->r_lv3_offset + 4, 0, READ, &rl[2][1]);

	pr_info("read low level priority %2x\n", def_r_priority);
	pr_info("\tl i id name\n");
	for (i = 0; i < 3; i++) {
		id = (unsigned char *)&rl[i][0];
		for (j = 0; j < 8; j++) {
			for (k = 0; k < h->r_num; k++) {
				if (id[j] == h->r_id_name[k].id)
					break;
			}
			if (k == h->r_num)
				pr_info("\t%d %d %2x\n", i + 1, j, id[j]);
			else
				pr_info("\t%d %d %2x %s\n", i + 1, j, id[j], h->r_id_name[k].name);
		}
	}

	pr_info("write low level priority %2x\n", def_w_priority);
	pr_info("\tl i id name\n");
	for (i = 0; i < 3; i++) {
		id = (unsigned char *)&wl[i][0];
		for (j = 0; j < 8; j++) {
			for (k = 0; k < h->w_num; k++) {
				if (id[j] == h->w_id_name[k].id)
					break;
			}
			if (k == h->w_num)
				pr_info("\t%d %d %2x\n", i + 1, j, id[j]);
			else
				pr_info("\t%d %d %2x %s\n", i + 1, j, id[j], h->w_id_name[k].name);
		}
	}

	return 0;
}

int show_hevc_priority_setting(void)
{
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	return show_hevc_hcodec_priority_setting(v2->hevc);
}
EXPORT_SYMBOL(show_hevc_priority_setting);

int show_hcodec_priority_setting(void)
{
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	return show_hevc_hcodec_priority_setting(v2->hcodec);
}
EXPORT_SYMBOL(show_hcodec_priority_setting);

int set_device_accessible(char *dev, unsigned char access)
{
	struct ddr_priority_v2 *v2;

	v2 = aml_db->priority->v2;
	if (!v2) {
		pr_err("this chip currently does not support v2 priority\n");
		return -EINVAL;
	}
	access = !!access;
	if (strcmp(dev, "demod") == 0) {
		v2->demod->allow_access = access;
	} else if (strcmp(dev, "demux") == 0) {
		v2->demux->allow_access = access;
	} else if (strcmp(dev, "audio") == 0) {
		v2->audio->allow_access = access;
	} else if (strcmp(dev, "bcon") == 0) {
		v2->bcon->allow_access = access;
	} else if (strcmp(dev, "hevc") == 0) {
		v2->hevc->allow_access = access;
	} else if (strcmp(dev, "hcodec") == 0) {
		v2->hcodec->allow_access = access;
	} else if (strcmp(dev, "vpu") == 0) {
		v2->vpu->allow_access = access;
	} else {
		pr_err("device name mismatch\n");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(set_device_accessible);

#define CMD_STR_ERR "setting %s priority command is error\n"
void set_priority_display(const char *buf)
{
	unsigned int id, priority, access;
	char mode;
	char rw = 0;

	if (strncmp(buf, "dmc ", 4) == 0) {
		buf = buf + 4;
		if ((sscanf(buf, "%d %x %c", &id, &priority, &rw) == 3 ||
				sscanf(buf, "%x %x", &id, &priority) == 2)) {
			set_dmc_priority(id, priority, rw);
			return;
		}
		pr_err(CMD_STR_ERR, "dmc");
		return;
	} else if (strncmp(buf, "dev ", 4) == 0) {
		buf = buf + 4;
		if (sscanf(buf, "%x %x %c", &id, &priority, &rw) == 3 ||
				sscanf(buf, "%x %x", &id, &priority) == 2) {
			set_dev_priority(id, priority, rw);
			return;
		}
		pr_err(CMD_STR_ERR, "device");
		return;
	} else if (strncmp(buf, "vpu ", 4) == 0) {
		unsigned int vpu_i;
		unsigned int top_i;

		buf = buf + 4;
		if ((sscanf(buf, "%c %x", &mode, &access) == 2) && (mode == 'p')) {
			set_device_accessible("vpu", !!access);
			return;
		}
		if (sscanf(buf, "%x %c %c", &vpu_i, &rw, &mode) == 3) {
			if (mode == 's') {
				buf = buf + 5;
				if (sscanf(buf, " %x", &priority) == 1) {
					set_vpu_super_priority(vpu_i, rw, priority);
					return;
				}
			} else if (mode == 't') {
				buf = buf + 5;
				if (sscanf(buf, " %x %x", &top_i, &priority) == 2) {
					set_vpu_top_priority(vpu_i, rw, top_i, priority);
					return;
				}
			} else {
				buf = buf + 3;
				if (sscanf(buf, " %x %x", &id, &priority) == 2) {
					set_vpu_sub_dev_priority(vpu_i, rw, id, priority);
					return;
				}
			}
		}
		pr_err(CMD_STR_ERR, "vpu");
		return;
	} else if (strncmp(buf, "demod", 5) == 0) {
		buf = buf + 5;
		if ((sscanf(buf, " %c %x", &mode, &access) == 2) && (mode == 'p')) {
			set_device_accessible("demod", !!access);
			return;
		} else if (sscanf(buf, " %x", &priority) == 1) {
			set_demod_priority(priority);
			return;
		}
		pr_err(CMD_STR_ERR, "demod");
		return;
	} else if (strncmp(buf, "demux", 5) == 0) {
		buf = buf + 5;
		if ((sscanf(buf, " %c %x", &mode, &access) == 2) && (mode == 'p')) {
			set_device_accessible("demux", !!access);
			return;
		} else if (sscanf(buf, " %x %c", &priority, &rw) == 2 ||
				sscanf(buf, " %x", &priority) == 1) {
			set_demux_priority(priority, rw);
			return;
		}
		pr_err(CMD_STR_ERR, "demux");
		return;
	} else if (strncmp(buf, "audio", 5) == 0) {
		char x;
		unsigned int ugt;
		unsigned int idx;

		buf = buf + 5;
		if ((sscanf(buf, " %c %x", &mode, &access) == 2) && (mode == 'p')) {
			set_device_accessible("audio", !!access);
			return;
		}
		if (sscanf(buf, " %c", &x) == 1) {
			buf = buf + 2;
			if (x == 's') {
				if (sscanf(buf, " %x %x %c", &idx, &priority, &rw) == 3 ||
						sscanf(buf, " %x %x", &idx, &priority) == 2) {
					set_audio_priority(idx, priority, rw);
					return;
				}
			} else if (sscanf(buf, " %x", &ugt) == 1) {
				set_audio_x_select_priority(x, ugt);
				return;
			}
		} else {
			show_audio_priority_setting();
			return;
		}
		pr_err(CMD_STR_ERR, "audio");
		return;
	} else if (strncmp(buf, "bcon", 4) == 0) {
		char mode;
		unsigned int low = 0;
		unsigned int high = 0;
		unsigned int ugt;

		buf += 4;
		if ((sscanf(buf, " %c %x", &mode, &access) == 2) && (mode == 'p')) {
			set_device_accessible("bcon", !!access);
			return;
		}
		if (sscanf(buf, " %c", &mode) == 1) {
			buf += 2;
			if (mode == 'h') {
				if (sscanf(buf, " %x %x", &low, &high) == 2) {
					set_bcon_hw_urgent(low, high);
					return;
				}
			} else if (mode == 's') {
				if (sscanf(buf, " %x", &ugt) == 1) {
					set_bcon_sw_urgent(ugt);
					return;
				}
			}
		} else {
			show_bcon_priority_setting();
			return;
		}
		pr_err(CMD_STR_ERR, "bcon");
		return;
	} else if (strncmp(buf, "hevc", 4) == 0) {
		unsigned int level;
		unsigned int i;
		struct hevc_hcodec_priority *h;

		buf = buf + 4;
		h = aml_db->priority->v2->hevc;
		if ((sscanf(buf, " %c %x", &mode, &access) == 2) && (mode == 'p')) {
			set_device_accessible("hevc", !!access);
			return;
		}
		if (sscanf(buf, " %c %x %x %x", &rw, &level, &i, &id) == 4)
			set_hevc_hcodec_level_id(h, rw, level, i, id);
		else if (sscanf(buf, " %c %x", &rw, &priority) == 2)
			set_hevc_hcodec_low_priority(h, rw, priority);
		else
			show_hevc_hcodec_priority_setting(h);
		return;
	} else if (strncmp(buf, "hcodec", 6) == 0) {
		unsigned int level;
		unsigned int i;
		struct hevc_hcodec_priority *h;

		buf = buf + 6;
		h = aml_db->priority->v2->hcodec;
		if ((sscanf(buf, " %c %x", &mode, &access) == 2) && (mode == 'p')) {
			set_device_accessible("hcodec", !!access);
			return;
		}
		if (sscanf(buf, " %c %x %x %x", &rw, &level, &i, &id) == 4)
			set_hevc_hcodec_level_id(h, rw, level, i, id);
		else if (sscanf(buf, " %c %x", &rw, &priority) == 2)
			set_hevc_hcodec_low_priority(h, rw, priority);
		else
			show_hevc_hcodec_priority_setting(h);
		return;
	}

	pr_err("the command is incorrect\n");
}
