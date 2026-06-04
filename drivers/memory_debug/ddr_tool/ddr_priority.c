// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
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
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/bits.h>
#include <linux/bitfield.h>
#include <linux/overflow.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include <linux/stdarg.h>
#include "ddr_port.h"
#include "ddr_bandwidth.h"

#define GROUP_IS_W		BIT(31)
#define GROUP_LEVEL3		BIT(30)
#define GROUP_LEVEL4		BIT(29)
#define GROUP_LEVEL5		BIT(28)
#define GROUP_LEVEL_MASK	(GROUP_LEVEL3 | GROUP_LEVEL4 | GROUP_LEVEL5)

#define DEV_L3_R(port, sub, dev_id)					\
	.port_id = (port), .sub_id = (sub),				\
	.group_id = GROUP_LEVEL3 | (sub), .module_id = (dev_id)

#define DEV_L3_W(port, sub, dev_id)					\
	.port_id = (port), .sub_id = (sub),				\
	.group_id = GROUP_IS_W | GROUP_LEVEL3 | (sub), .module_id = (dev_id)

#define DDR_PRIORITY_FORCE_RW(_bus, _mask, _off, _lsb, _nbits)				\
	{										\
		.bus = (_bus),								\
		.r_force_mask = (_mask),						\
		.w_force_mask = (_mask),						\
		.r_force_mask_offset = (_off),						\
		.w_force_mask_offset = (_off),						\
		.r_offset = (_off),							\
		.w_offset = (_off),							\
		.r_lsb = (_lsb),							\
		.w_lsb = (_lsb),							\
		.r_nbits = (_nbits),							\
		.w_nbits = (_nbits),							\
	}										\

#define DDR_PRIORITY_NIC_FIXED(_port_id, _sub_id, _fixed_w, _fixed_r)			\
	{										\
		.port_id = _port_id,							\
		.sub_id = _sub_id,							\
		.prop_mode = PRIO_PROP_FIXED,						\
		.reg = {								\
			.ctrl_type = PRIO_CTRL_FIXED,					\
			.fixed_w = (_fixed_w),						\
			.fixed_r = (_fixed_r),						\
			.fixed_nbits_r = 4,						\
			.fixed_nbits_w = 4,						\
		},									\
	}										\

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
static struct ddr_priority_force ddr_priority_force_s4[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, -1, (0x80 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(1, -1, (0x84 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(3, -1, (0x8c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(4, -1, (0x90 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(6, -1, (0x98 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(7, -1, (0x9c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(11, -1, (0xac << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(16, -1, (0x60 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(17, -1, (0x64 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(18, -1, (0x68 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(19, -1, (0x6c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(20, -1, (0x70 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(21, -1, (0x74 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(23, -1, (0x7c << 2), 16, 3)
};

static struct ddr_priority_info ddr_priority_info_s4 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_s4),
	.dmc_force = ddr_priority_force_s4,
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct ddr_priority_dev ddr_priority_dev_t7[] __initdata = {
	{ .port_id = 1, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe704100, .reg_mode = 1,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 2, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe703100, .reg_mode = 1,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 3, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe701100, .reg_mode = 1,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 4, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe702100, .reg_mode = 1,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	//{ .port_id = 8, .sub_id = -1, .force_mask = BIT(24),
	//  .reg = { .ctrl_type = PRIO_CTRL_REG_RW },
	//	.reg_base = 0xfe374014, .reg_mode = 1,
	//	.w_offset = 0, .w_lsb = 28, .w_nbits = 4,
	//	.r_offset = 0, .r_lsb = 28, .r_nbits = 4,
	//},
	//{ .port_id = 9, .sub_id = -1, .force_mask = BIT(24),
	//  .reg = { .ctrl_type = PRIO_CTRL_REG_RW },
	//	.reg_base = 0xfe374014, .reg_mode = 1,
	//	.w_offset = 0, .w_lsb = 20, .w_nbits = 4,
	//	.r_offset = 0, .r_lsb = 20, .r_nbits = 4,
	//},
	//{ .port_id = 10, .sub_id = -1, .force_mask = BIT(24),
	//  .reg = { .ctrl_type = PRIO_CTRL_REG_RW },
	//	.reg_base = 0xfe374014, .reg_mode = 1,
	//	.w_offset = 0, .w_lsb = 12, .w_nbits = 4,
	//	.r_offset = 0, .r_lsb = 12, .r_nbits = 4,
	//},
	//{ .port_id = 11, .sub_id = -1, .force_mask = BIT(24),
	//  .reg = { .ctrl_type = PRIO_CTRL_REG_RW },
	//	.reg_base = 0xfe374014, .reg_mode = 1,
	//	.w_offset = 0, .w_lsb = 4, .w_nbits = 4,
	//	.r_offset = 0, .r_lsb = 4, .r_nbits = 4,
	//},
	{ .port_id = 24, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 25, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 26, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 27, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 28, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 29, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 34, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 35, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 36, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 37, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 38, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 39, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 50, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 51, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 52, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 53, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 54, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 63, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 64, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_POWER,
		.reg_base = 0xfe3b3000, .reg_mode = 0,
		.w_offset = 0xac4, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0xac0, .r_lsb = 0, .r_nbits = 4,
		.w_force_mask = BIT(16),
	  },
	},
	{ .port_id = 66, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_POWER,
		.reg_base = 0xfe3b5000, .reg_mode = 0,
		.w_offset = 0xac4, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0xac0, .r_lsb = 0, .r_nbits = 4,
		.w_force_mask = BIT(16),
	  },
	},
	{ .port_id = 67, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_POWER,
		.reg_base = 0xfe3b3000, .reg_mode = 0,
		.w_offset = 0xec4, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0xec0, .r_lsb = 0, .r_nbits = 4,
		.w_force_mask = BIT(16),
	  },
	},
	{ .port_id = 72, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 73, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},
	{ .port_id = 80, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff009000, .reg_mode = 1,
		.w_offset = 0xcc8, .w_lsb = 0, .w_nbits = 16,
		.r_offset = 0xcc0, .r_lsb = 0, .r_nbits = 16,
	  },
	},
	{ .port_id = 81, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff009000, .reg_mode = 1,
		.w_offset = 0xcc8, .w_lsb = 16, .w_nbits = 16,
		.r_offset = 0xcc0, .r_lsb = 16, .r_nbits = 16,
	  },
	},
	{ .port_id = 82, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff009000, .reg_mode = 1,
		.w_offset = 0xcc4, .w_lsb = 0, .w_nbits = 16,
		.r_offset = 0xcc4, .r_lsb = 0, .r_nbits = 16,
	  },
	},
};

static struct ddr_priority_nic ddr_priority_nic_t7[] __initdata = {
	{ .port_id = 16, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 17, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d8, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 18, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d8, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 19, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 24, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 24, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 25, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 26, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 27, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 28, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 29, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 34, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 35, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 36, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 37, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 38, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 39, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 48, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104dc, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 49, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d8, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 24, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 50, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104d8, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 3,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 51, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104e0, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 3,
		.r_offset = 0, .r_lsb = 4, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 52, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104dc, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 24, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 28, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 53, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104dc, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 54, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104dc, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 55, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104e0, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 63, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104e4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 4, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 68, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104e0, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 16, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 20, .r_nbits = 4,
	  },
	},
	{ .port_id = 69, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104e0, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 24, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 72, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104e4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 73, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe0104e4, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 8, .w_nbits = 2,
		.r_offset = 0, .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_t7[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x8c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(5, BIT(15), (0x94 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_t7 __initdata = {
	.dev_num = ARRAY_SIZE(ddr_priority_dev_t7),
	.dev = ddr_priority_dev_t7,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_t7),
	.nic = ddr_priority_nic_t7,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_t7),
	.dmc_force = ddr_priority_force_t7,
};

static struct ddr_priority_dev ddr_priority_dev_s5[] __initdata = {
	{ .port_id = 33, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 64, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff809000, .reg_mode = 0,
		.w_offset = 0xcc8, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0xcc0, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 66, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff809000, .reg_mode = 0,
		.w_offset = 0xcc8, .w_lsb = 16, .w_nbits = 4,
		.r_offset = 0xcc0, .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 68, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff809000, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 0,
		.r_offset = 0xcc4, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 70, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff809000, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 0,
		.r_offset = 0xcc4, .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 106, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xfe03e000, .reg_mode = 0,
		.w_offset = 0x4c4, .w_lsb = 16, .w_nbits = 4,
		.r_offset = 0x4c8, .r_lsb = 0, .r_nbits = 4,
	  },
	},
};

static struct ddr_priority_nic ddr_priority_nic_s5[] __initdata = {
	{ .port_id = 2, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x130 << 2), .w_lsb = 20, .w_nbits = 4,
		.r_offset = (0x130 << 2), .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 4, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x130 << 2), .w_lsb = 12, .w_nbits = 4,
		.r_offset = (0x130 << 2), .r_lsb = 8, .r_nbits = 4,
	  },
	},
	{ .port_id = 8, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x15b << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x15b << 2), .r_lsb = 20, .r_nbits = 4,
	  },
	},
	{ .port_id = 9, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x15b << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x15b << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 16, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = { .ctrl_type = PRIO_CTRL_NOT_SUPPORT,},
	},
	{ .port_id = 24, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 20, .w_nbits = 2,
		.r_offset = (0x138 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 25, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 20, .w_nbits = 2,
		.r_offset = (0x138 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 26, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 20, .w_nbits = 2,
		.r_offset = (0x138 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 28, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 20, .w_nbits = 2,
		.r_offset = (0x138 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 29, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 20, .w_nbits = 2,
		.r_offset = (0x138 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 35, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 22, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 22, .r_nbits = 4,
	  },
	},
	{ .port_id = 36, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 37, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 4, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 38, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 8, .r_nbits = 4,
	  },
	},
	{ .port_id = 54, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 12, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 56, .sub_id = -1,
	  .reg = { .ctrl_type = PRIO_CTRL_REG_RW,
	  .reg_base = 0xfe010000, .reg_mode = 0,
	  .w_offset = (0x138 << 2), .w_lsb = 0, .w_nbits = 4,
	  .r_offset = (0x138 << 2), .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 57, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 4, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 58, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 8, .r_nbits = 4,
	  },
	},
	{ .port_id = 59, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 12, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 60, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 72, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 9, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 6, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 80, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 15, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 12, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 81, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 21, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 18, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 88, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 27, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 24, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 96, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 3, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 0, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 104, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x133 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x133 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 108, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x133 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x133 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_s5[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x8c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(5, BIT(15), (0x94 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xaa << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_s5 __initdata = {
	.dev_num = ARRAY_SIZE(ddr_priority_dev_s5),
	.dev = ddr_priority_dev_s5,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_s5),
	.nic = ddr_priority_nic_s5,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_s5),
	.dmc_force = ddr_priority_force_s5,
};

static struct ddr_priority_force ddr_priority_force_s6[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x8c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xa0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(9, BIT(15), (0xa4 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(11, BIT(15), (0xac << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(12, BIT(15), (0xb0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(13, BIT(15), (0xb4 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(14, BIT(15), (0xb8 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(15, BIT(15), (0xbc << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(16, BIT(15), (0x60 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(17, BIT(15), (0x64 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(18, BIT(15), (0x68 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(19, BIT(15), (0x6c << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_s6 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_s6),
	.dmc_force = ddr_priority_force_s6,
};

struct ddr_priority_nic ddr_priority_nic_c3[] __initdata = {
	{ .port_id = 2, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe42000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 40, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfd342000, .reg_mode = 0,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 41, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfd343000, .reg_mode = 0,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 42, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfd344000, .reg_mode = 0,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 43, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfd346000, .reg_mode = 0,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 44, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfd345000, .reg_mode = 0,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 48, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe48000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 72, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe44000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 73, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe44000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 74, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe44000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 80, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe47000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 81, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe45000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 82, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe4a000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 83, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe46000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 85, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe49000, .reg_mode = 1,
		.w_offset = 0x104, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
};

static struct ddr_priority_dev ddr_priority_dev_c3[] __initdata = {
	{ .port_id = 16, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff010800, .reg_mode = 1, .w_force_mask = BIT(16),
		.w_offset = (0xb1 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0xb0 << 2), .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 17, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff010000, .reg_mode = 1,
		.w_offset = (0x05 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x05 << 2), .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 18, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xff010000, .reg_mode = 1,
		.w_offset = (0x05 << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = 0, .r_lsb = 0, .r_nbits = 0,
	  },
	},
	{ .port_id = 19, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_SW_AUTO,
		.reg_base = 0xfe350010, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 0,
		.r_offset = 0, .r_lsb = 4, .r_nbits = 4,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_c3[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x8c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_c3 __initdata = {
	.dev_num = ARRAY_SIZE(ddr_priority_dev_c3),
	.dev = ddr_priority_dev_c3,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_c3),
	.nic = ddr_priority_nic_c3,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_c3),
	.dmc_force = ddr_priority_force_c3,
};

static struct ddr_priority_nic ddr_priority_nic_t5m[] __initdata = {
	{ .port_id = 2, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 4,
	  },
	},
	{ .port_id = 4, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x135 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x135 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 16, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x135 << 2), .w_lsb = 24, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 28, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 17, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x135 << 2), .w_lsb = 24, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 28, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 18, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x135 << 2), .w_lsb = 24, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 28, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 19, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x135 << 2), .w_lsb = 24, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 28, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 32, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x139 << 2), .w_lsb = 4, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 6, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 33, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x139 << 2), .w_lsb = 4, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 6, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 36, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x134 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x134 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 37, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x134 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x134 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 38, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x139 << 2), .w_lsb = 20, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 24, .r_nbits = 4,
	  },
	},
	{ .port_id = 54, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x139 << 2), .w_lsb = 0, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 2, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 56, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x136 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 57, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x136 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 58, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x136 << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 20, .r_nbits = 4,
	  },
	},
	{ .port_id = 59, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x136 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 60, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x135 << 2), .w_lsb = 0, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 4, .r_nbits = 4,
		.cur_lsb_r = 0, .cur_lsb_w = 2, .cur_nbits_r = 4, .cur_nbits_w = 2,
		.child_lsb_r = 0, .child_lsb_w = 0, .child_nbits_r = 0, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 62, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x139 << 2), .w_lsb = 8, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 10, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 64, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x139 << 2), .w_lsb = 12, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 14, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 66, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x139 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 18, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 74, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x137 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x137 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 82, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x138 << 2), .w_lsb = 0, .w_nbits = 2,
		.r_offset = (0x138 << 2), .r_lsb = 4, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 90, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x137 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x137 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 98, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 1,
		.w_offset = (0x137 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x137 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_t5m[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x8c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(5, BIT(15), (0x94 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xaa << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_t5m __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_t5m),
	.nic = ddr_priority_nic_t5m,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_t5m),
	.dmc_force = ddr_priority_force_t5m,
};

static struct ddr_priority_force ddr_priority_force_txhd2[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, -1, (0x80 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(1, -1, (0x84 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(2, -1, (0x88 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(3, -1, (0x8c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(4, -1, (0x90 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(7, -1, (0x9c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(8, -1, (0xa0 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(16, -1, (0x60 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(17, -1, (0x64 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(18, -1, (0x68 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(19, -1, (0x6c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(20, -1, (0x70 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(21, -1, (0x74 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(23, -1, (0x7c << 2), 16, 3)
};

static struct ddr_priority_info ddr_priority_info_txhd2 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_txhd2),
	.dmc_force = ddr_priority_force_txhd2,
};

static struct ddr_priority_nic ddr_priority_nic_t3x[] __initdata = {
	{ .port_id = 2, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x131 << 2), .w_lsb = 20, .w_nbits = 4,
		.r_offset = (0x131 << 2), .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 4, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x131 << 2), .w_lsb = 12, .w_nbits = 4,
		.r_offset = (0x131 << 2), .r_lsb = 8, .r_nbits = 4,
	  },
	},
	{ .port_id = 8, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x15b << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x15b << 2), .r_lsb = 20, .r_nbits = 4,
	  },
	},
	{ .port_id = 9, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x15b << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x15b << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 16, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0, .reg_mode = 0,
		.w_offset = 0, .w_lsb = 0, .w_nbits = 0,
		.r_offset = 0, .r_lsb = 0, .r_nbits = 0,
	  },
	},
	{ .port_id = 19, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_NOT_SUPPORT,},
	},
	{ .port_id = 21, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 24, .r_nbits = 4,
	  },
	},
	{ .port_id = 22, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 28, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 23, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 4, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 32, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 20, .w_nbits = 3,
		.r_offset = (0x138 << 2), .r_lsb = 20, .r_nbits = 3,
	  },
	},
	{ .port_id = 35, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 28, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 36, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 37, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 4, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 38, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 8, .r_nbits = 4,
	  },
	},
	{ .port_id = 54, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 12, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 57, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 58, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 8, .r_nbits = 4,
	  },
	},
	{ .port_id = 59, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 12, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 60, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 62, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 16, .r_nbits = 4,
	  },
	},
	{ .port_id = 63, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 20, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 20, .r_nbits = 4,
	  },
	},
	{ .port_id = 72, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 9, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 6, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 80, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 15, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 12, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 81, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 21, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 18, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 88, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 27, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 24, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 96, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x158 << 2), .w_lsb = 3, .w_nbits = 3,
		.r_offset = (0x158 << 2), .r_lsb = 0, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 3, .cur_nbits_w = 3,
		.child_lsb_r = 3, .child_lsb_w = 3, .child_nbits_r = 1, .child_nbits_w = 1,
	  },
	},
	{ .port_id = 104, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x0133 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x0133 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 106, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x0133 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x0133 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 116, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x0133 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x0133 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_t3x[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x8c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(5, BIT(15), (0x94 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xaa << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_t3x __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_t3x),
	.nic = ddr_priority_nic_t3x,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_t3x),
	.dmc_force = ddr_priority_force_t3x,
};

static struct ddr_priority_nic ddr_priority_nic_t3[] __initdata = {
	{ .port_id = 2, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x139 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 4, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x14c << 2), .w_lsb = 21, .w_nbits = 1,
		.r_offset = (0x14c << 2), .r_lsb = 20, .r_nbits = 1,
	  },
	},
	{ .port_id = 16, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x135 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 17, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x136 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 18, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x136 << 2), .w_lsb = 8, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 12, .r_nbits = 4,
	  },
	},
	{ .port_id = 19, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x135 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 20, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x136 << 2), .w_lsb = 16, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 20, .r_nbits = 4,
	  },
	},
	{ .port_id = 27, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 8, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 28, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 8, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 34, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 34, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 35, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 36, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 37, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 38, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 39, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x135 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x135 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 48, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x137 << 2), .w_lsb = 0, .w_nbits = 4,
		.r_offset = (0x137 << 2), .r_lsb = 4, .r_nbits = 4,
	  },
	},
	{ .port_id = 51, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 0, .w_nbits = 3,
		.r_offset = (0x138 << 2), .r_lsb = 4, .r_nbits = 3,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 52, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x137 << 2), .w_lsb = 24, .w_nbits = 2,
		.r_offset = (0x137 << 2), .r_lsb = 28, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 53, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x137 << 2), .w_lsb = 16, .w_nbits = 2,
		.r_offset = (0x137 << 2), .r_lsb = 20, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 54, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x137 << 2), .w_lsb = 8, .w_nbits = 2,
		.r_offset = (0x137 << 2), .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 63, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 0, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 4, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 68, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x136 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x136 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 70, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x138 << 2), .w_lsb = 24, .w_nbits = 4,
		.r_offset = (0x138 << 2), .r_lsb = 28, .r_nbits = 4,
	  },
	},
	{ .port_id = 72, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 8, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
	{ .port_id = 73, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x139 << 2), .w_lsb = 8, .w_nbits = 2,
		.r_offset = (0x139 << 2), .r_lsb = 12, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_t3[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x8c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(5, BIT(15), (0x94 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_t3 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_t3),
	.nic = ddr_priority_nic_t3,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_t3),
	.dmc_force = ddr_priority_force_t3,
};

static struct ddr_priority_nic ddr_priority_nic_a4[] __initdata = {
	{ .port_id = 6, .sub_id = 8,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x69  << 2), .w_lsb = 0, .w_nbits = 2,
		.r_offset = (0x69  << 2), .r_lsb = 2, .r_nbits = 2,
	  },
	},
	{ .port_id = 6, .sub_id = 9,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x69  << 2), .w_lsb = 12, .w_nbits = 1,
		.r_offset = (0x69  << 2), .r_lsb = 13, .r_nbits = 1,
	  },
	},
	{ .port_id = 6, .sub_id = 10,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x69  << 2), .w_lsb = 8, .w_nbits = 2,
		.r_offset = (0x69  << 2), .r_lsb = 10, .r_nbits = 2,
	  },
	},
	{ .port_id = 6, .sub_id = 11,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x69  << 2), .w_lsb = 14, .w_nbits = 2,
		.r_offset = (0x69  << 2), .r_lsb = 16, .r_nbits = 2,
	  },
	},
	{ .port_id = 6, .sub_id = 13,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe010000, .reg_mode = 0,
		.w_offset = (0x69  << 2), .w_lsb = 18, .w_nbits = 2,
		.r_offset = (0x69  << 2), .r_lsb = 20, .r_nbits = 2,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_a4[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_a4 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_a4),
	.nic = ddr_priority_nic_a4,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_a4),
	.dmc_force = ddr_priority_force_a4,
};

static struct ddr_priority_nic ddr_priority_nic_s7[] __initdata = {
	{ .port_id = 10, .sub_id = 0,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe46000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe47000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 2,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe48000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 3,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe4b000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 4,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe45000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 7,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe4b000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 9,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe49000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 10,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe4a000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
	{ .port_id = 10, .sub_id = 11,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xffe4b000, .reg_mode = 1,
		.w_offset = 0x100, .w_lsb = 0, .w_nbits = 4,
		.r_offset = 0x100, .r_lsb = 0, .r_nbits = 4,
	  },
	},
};

static struct ddr_priority_force ddr_priority_force_s7[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x84 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x88 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0x98 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0x9c << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xac << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(10, BIT(15), (0xb4 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(11, BIT(15), (0xb8 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(12, BIT(15), (0xbc << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(13, BIT(15), (0xc0 << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_s7 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_s7),
	.nic = ddr_priority_nic_s7,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_s7),
	.dmc_force = ddr_priority_force_s7,
};

static struct ddr_priority_force ddr_priority_force_s7d[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x85 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x8a << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x95 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0xa0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0xa5 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xaa << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(9, BIT(15), (0xb0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(10, BIT(15), (0xb5 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(11, BIT(15), (0xba << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(12, BIT(15), (0xc0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(13, BIT(15), (0xc5 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(14, BIT(15), (0xca << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_s7d __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_s7d),
	.dmc_force = ddr_priority_force_s7d,
};

static struct ddr_priority_force ddr_priority_force_sc2[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, -1, (0x50 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(1, -1, (0x54 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(2, -1, (0x58 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(3, -1, (0x5c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(4, -1, (0x60 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(6, -1, (0x68 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(7, -1, (0x6c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(8, -1, (0x70 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(9, -1, (0x74 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(11, -1, (0x7c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(12, -1, (0x80 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(16, -1, (0x30 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(17, -1, (0x34 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(18, -1, (0x38 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(19, -1, (0x3c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(20, -1, (0x40 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(21, -1, (0x44 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(22, -1, (0x48 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(23, -1, (0x4c << 2), 16, 3)
};

static struct ddr_priority_info ddr_priority_info_sc2 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_sc2),
	.dmc_force = ddr_priority_force_sc2,
};

static struct ddr_priority_force ddr_priority_force_t5w[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, -1, (0x80 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(1, -1, (0x84 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(3, -1, (0x8c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(4, -1, (0x90 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(6, -1, (0x98 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(7, -1, (0x9c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(8, -1, (0xa0 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(9, -1, (0xa4 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(10, -1, (0xa8 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(16, -1, (0x60 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(17, -1, (0x64 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(18, -1, (0x68 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(19, -1, (0x6c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(20, -1, (0x70 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(21, -1, (0x74 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(23, -1, (0x7c << 2), 16, 3)
};

static struct ddr_priority_info ddr_priority_info_t5w __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_t5w),
	.dmc_force = ddr_priority_force_t5w,
};

static struct ddr_priority_force ddr_priority_force_g12b[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, -1, (0x80 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(1, -1, (0x84 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(2, -1, (0x88 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(3, -1, (0x8c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(4, -1, (0x90 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(5, -1, (0x94 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(6, -1, (0x98 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(7, -1, (0x9c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(8, -1, (0xa0 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(9, -1, (0xa4 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(10, -1, (0xa8 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(11, -1, (0xac << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(12, -1, (0xae << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(16, -1, (0x60 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(17, -1, (0x64 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(18, -1, (0x68 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(19, -1, (0x6c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(20, -1, (0x70 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(21, -1, (0x74 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(22, -1, (0x78 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(23, -1, (0x7c << 2), 16, 3)
};

static struct ddr_priority_info ddr_priority_info_g12b __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_g12b),
	.dmc_force = ddr_priority_force_g12b,
};

static struct ddr_priority_force ddr_priority_force_gxlx3[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, -1, (0xb0 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(1, -1, (0xba << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(2, -1, (0xc4 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(3, -1, (0xce << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(4, -1, (0xd8 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(7, -1, (0xf6 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(8, -1, (0x60 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(9, -1, (0x6a << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(10, -1, (0x74 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(11, -1, (0x7e << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(12, -1, (0x88 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(13, -1, (0x92 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(14, -1, (0x9c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(15, -1, (0xa6 << 2), 16, 3)
};

static struct ddr_priority_info ddr_priority_info_gxlx3 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_gxlx3),
	.dmc_force = ddr_priority_force_gxlx3,
};

static struct ddr_priority_force ddr_priority_force_t6d[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x85 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x8a << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x95 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0xa0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0xa5 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xaa << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_t6d __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_t6d),
	.dmc_force = ddr_priority_force_t6d,
};

static struct ddr_priority_force ddr_priority_force_t6w[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x85 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x8a << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x95 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0xa0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0xa5 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(8, BIT(15), (0xaa << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(9, BIT(15), (0xb0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(10, BIT(15), (0xb5 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(11, BIT(15), (0xba << 2), 16, 4)
};

static struct ddr_priority_info ddr_priority_info_t6w __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_t6w),
	.dmc_force = ddr_priority_force_t6w,
};

static struct ddr_priority_force ddr_priority_force_c4[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, BIT(15), (0x80 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(1, BIT(15), (0x85 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(2, BIT(15), (0x8a << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(3, BIT(15), (0x90 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(4, BIT(15), (0x95 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(6, BIT(15), (0xa0 << 2), 16, 4),
	DDR_PRIORITY_FORCE_RW(7, BIT(15), (0xa5 << 2), 16, 4),
};

static struct ddr_priority_info ddr_priority_info_c4 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_c4),
	.dmc_force = ddr_priority_force_c4,
};

static struct ddr_priority_force ddr_priority_force_a9[] __initdata = {
	{ .bus = 0,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x80 << 2), .w_force_mask_offset = (0x80 << 2),
	  .r_offset = (0x80 << 2),            .w_offset = (0x83 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 1,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x84 << 2), .w_force_mask_offset = (0x84 << 2),
	  .r_offset = (0x84 << 2),            .w_offset = (0x87 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 2,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x88 << 2), .w_force_mask_offset = (0x88 << 2),
	  .r_offset = (0x88 << 2),            .w_offset = (0x8b << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 3,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x8c << 2), .w_force_mask_offset = (0x8c << 2),
	  .r_offset = (0x8c << 2),            .w_offset = (0x8f << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 4,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x90 << 2), .w_force_mask_offset = (0x90 << 2),
	  .r_offset = (0x90 << 2),            .w_offset = (0x93 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 5,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x94 << 2), .w_force_mask_offset = (0x94 << 2),
	  .r_offset = (0x94 << 2),            .w_offset = (0x97 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 6,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x98 << 2), .w_force_mask_offset = (0x98 << 2),
	  .r_offset = (0x98 << 2),            .w_offset = (0x9b << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 7,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x9c << 2), .w_force_mask_offset = (0x9c << 2),
	  .r_offset = (0x9c << 2),            .w_offset = (0x9f << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 8,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xa0 << 2), .w_force_mask_offset = (0xa0 << 2),
	  .r_offset = (0xa0 << 2),            .w_offset = (0xa3 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 9,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xa4 << 2), .w_force_mask_offset = (0xa4 << 2),
	  .r_offset = (0xa4 << 2),            .w_offset = (0xa7 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 10,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xa8 << 2), .w_force_mask_offset = (0xa8 << 2),
	  .r_offset = (0xa8 << 2),            .w_offset = (0xab << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 11,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xac << 2), .w_force_mask_offset = (0xac << 2),
	  .r_offset = (0xac << 2),            .w_offset = (0xaf << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 12,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xb0 << 2), .w_force_mask_offset = (0xb0 << 2),
	  .r_offset = (0xb0 << 2),            .w_offset = (0xb3 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 13,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xb4 << 2), .w_force_mask_offset = (0xb4 << 2),
	  .r_offset = (0xb4 << 2),            .w_offset = (0xb7 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 14,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xb8 << 2), .w_force_mask_offset = (0xb8 << 2),
	  .r_offset = (0xb8 << 2),            .w_offset = (0xbb << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 15,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xbc << 2), .w_force_mask_offset = (0xbc << 2),
	  .r_offset = (0xbc << 2),            .w_offset = (0xbf << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	}
};

static struct ddr_priority_info ddr_priority_info_a9 __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_a9),
	.dmc_force = ddr_priority_force_a9,
};

static struct ddr_priority_dev ddr_priority_dev_c5[] __initdata = {
	/* level 1 */
	{ .port_id = 1, .sub_id = -1,
	  .prop_mode = PRIO_PROP_FULL,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 2, .sub_id = -1,
	  .prop_mode = PRIO_PROP_FULL,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 3, .sub_id = -1,
	  .prop_mode = PRIO_PROP_FULL,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 4, .sub_id = -1,
	  .prop_mode = PRIO_PROP_FULL,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 5, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = { .ctrl_type = PRIO_CTRL_NOT_SUPPORT,},
	},

	{ .port_id = 6, .sub_id = -1,
	  .prop_mode = PRIO_PROP_FULL,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 7, .sub_id = -1,
	  .prop_mode = PRIO_PROP_FULL,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 8, .sub_id = -1,
	  .prop_mode = PRIO_PROP_FULL,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 9, .sub_id = -1,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe320000, .reg_mode = 0,
		.w_offset = (0x3fa3 << 2), .w_lsb = 0, .w_nbits = 2,
		.r_offset = (0x3fa3 << 2), .r_lsb = 2, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},

	{ .port_id = 10, .sub_id = -1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	/* level 2 */
	{ .port_id = 1, .sub_id = 0,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = { .ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 1, .sub_id = 1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 3, .sub_id = 0,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 3, .sub_id = 1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 3, .sub_id = 2,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	{ .port_id = 8, .sub_id = 0,
	  .prop_mode = PRIO_PROP_PARTIAL,
	  .reg = {
		.ctrl_type = PRIO_CTRL_REG_RW,
		.reg_base = 0xfe320000, .reg_mode = 0,
		.w_offset = (0x3fa1 << 2), .w_lsb = 0, .w_nbits = 2,
		.r_offset = (0x3fa1 << 2), .r_lsb = 2, .r_nbits = 2,
		.cur_lsb_r = 0, .cur_lsb_w = 0, .cur_nbits_r = 2, .cur_nbits_w = 2,
		.child_lsb_r = 2, .child_lsb_w = 2, .child_nbits_r = 2, .child_nbits_w = 2,
	  },
	},

	{ .port_id = 8, .sub_id = 1,
	  .prop_mode = PRIO_PROP_NONE,
	  .reg = {.ctrl_type = PRIO_CTRL_HW_AUTO,},
	},

	/* HEVC: all sub-devices */
	{ .port_id = 9, .sub_id = SUB_ID_MATCH_ALL,
	  .prop_mode = PRIO_PROP_NONE,
	  .handle_get = c5_hevc_handle_get,
	  .handle_set = c5_hevc_handle_set,
	  .reg = {
		.ctrl_type = PRIO_CTRL_HANDLER,
		.w_nbits = 2, .r_nbits = 2,
	  },
	},

	/* level 3 */
	{
	  DEV_L3_R(8, 0, MODULE_ID_MATCH_ALL),
	  .prop_mode = PRIO_PROP_NONE,
	  .handle_get = c5_hcodec_handle_get,
	  .handle_set = c5_hcodec_handle_set,
	  .reg = {
		.ctrl_type = PRIO_CTRL_HANDLER,
		.w_nbits = 2, .r_nbits = 2,
	  },
	},

	{
	  DEV_L3_W(8, 0, MODULE_ID_MATCH_ALL),
	  .prop_mode = PRIO_PROP_NONE,
	  .handle_get = c5_hcodec_handle_get,
	  .handle_set = c5_hcodec_handle_set,
	  .reg = {
		.ctrl_type = PRIO_CTRL_HANDLER,
		.w_nbits = 2, .r_nbits = 2,
	  },
	},
};

static struct ddr_priority_nic ddr_priority_nic_c5[] __initdata = {
	DDR_PRIORITY_NIC_FIXED(0, -1, 15, 15),
	DDR_PRIORITY_NIC_FIXED(10, 34,  8,  8),
	DDR_PRIORITY_NIC_FIXED(10, 38,  8,  8),
	DDR_PRIORITY_NIC_FIXED(10, 32,  8,  8),
	DDR_PRIORITY_NIC_FIXED(10, 48,  9,  9),
	DDR_PRIORITY_NIC_FIXED(10, 49,  4,  4),
	DDR_PRIORITY_NIC_FIXED(10, 35,  9,  9),
	DDR_PRIORITY_NIC_FIXED(10, 36, 10, 10),
	DDR_PRIORITY_NIC_FIXED(10, 33, 10, 10),
	DDR_PRIORITY_NIC_FIXED(10, 37,  9,  9),
	DDR_PRIORITY_NIC_FIXED(10, 40,  0,  0),
	DDR_PRIORITY_NIC_FIXED(10, 21, 12, 12),
	DDR_PRIORITY_NIC_FIXED(10, 39,  5,  5),
};

static struct ddr_priority_force ddr_priority_force_c5[] __initdata = {
	{ .bus = 0,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x80 << 2), .w_force_mask_offset = (0x80 << 2),
	  .r_offset = (0x80 << 2),            .w_offset = (0x83 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 1,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x84 << 2), .w_force_mask_offset = (0x84 << 2),
	  .r_offset = (0x84 << 2),            .w_offset = (0x87 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 2,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x88 << 2), .w_force_mask_offset = (0x88 << 2),
	  .r_offset = (0x88 << 2),            .w_offset = (0x8b << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 3,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x8c << 2), .w_force_mask_offset = (0x8c << 2),
	  .r_offset = (0x8c << 2),            .w_offset = (0x8f << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 4,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x90 << 2), .w_force_mask_offset = (0x90 << 2),
	  .r_offset = (0x90 << 2),            .w_offset = (0x93 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 5,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x94 << 2), .w_force_mask_offset = (0x94 << 2),
	  .r_offset = (0x94 << 2),            .w_offset = (0x97 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 6,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x98 << 2), .w_force_mask_offset = (0x98 << 2),
	  .r_offset = (0x98 << 2),            .w_offset = (0x9b << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 7,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0x9c << 2), .w_force_mask_offset = (0x9c << 2),
	  .r_offset = (0x9c << 2),            .w_offset = (0x9f << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 8,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xa0 << 2), .w_force_mask_offset = (0xa0 << 2),
	  .r_offset = (0xa0 << 2),            .w_offset = (0xa3 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 9,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xa4 << 2), .w_force_mask_offset = (0xa4 << 2),
	  .r_offset = (0xa4 << 2),            .w_offset = (0xa7 << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	},
	{ .bus = 10,
	  .r_force_mask = BIT(15),            .w_force_mask = BIT(14),
	  .r_force_mask_offset = (0xa8 << 2), .w_force_mask_offset = (0xa8 << 2),
	  .r_offset = (0xa8 << 2),            .w_offset = (0xab << 2),
	  .r_lsb = 16,                        .w_lsb = 4,
	  .r_nbits = 4,                       .w_nbits = 4,
	}
};

static struct ddr_priority_info ddr_priority_info_c5 __initdata = {
	.dev_num = ARRAY_SIZE(ddr_priority_dev_c5),
	.dev = ddr_priority_dev_c5,
	.nic_num = ARRAY_SIZE(ddr_priority_nic_c5),
	.nic = ddr_priority_nic_c5,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_c5),
	.dmc_force = ddr_priority_force_c5,
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
	.reg_base = 0xfe310000,
	.mode_reg1 = 0xf000 + (0x00 << 2),
	.dtmb_mode_bit = 0,
	.isdbt_mode_bit = 1,
	.mode_reg2 = 0xf000 + (0x0c << 2),
	.dvb_t2_mode_bit = 16,
	.isdbt_name = "ISDBT",
	.isdbt_reg = 0x800 + (0x02 << 2),
	.isdbt_bit = 17,
	.dvb_t2_select_reg = 0xf040,
	.dvb_t2_select_mode = 0x182,
	.dvb_t2_reg = 0x3613, //byte rw
	.dvb_t2_bit = 7,
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
#endif
static struct ddr_priority_force ddr_priority_force_s1a[] __initdata = {
	DDR_PRIORITY_FORCE_RW(0, -1, (0x80 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(3, -1, (0x8c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(4, -1, (0x90 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(7, -1, (0x9c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(11, -1, (0xac << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(16, -1, (0x60 << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(19, -1, (0x6c << 2), 16, 3),
	DDR_PRIORITY_FORCE_RW(21, -1, (0x74 << 2), 16, 3)
};

static struct ddr_priority_info ddr_priority_info_s1a __initdata = {
	.dev_num = 0, .dev = NULL,
	.nic_num = 0, .nic = NULL,
	.mdc_num = 0, .mdc = NULL,
	.dmc_force_num = ARRAY_SIZE(ddr_priority_force_s1a),
	.dmc_force = ddr_priority_force_s1a,
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

static int __init ddr_priority_info_alloc(struct ddr_priority_info **dst,
						struct ddr_priority_info *src)
{
	struct ddr_priority_info *info;
	struct ddr_priority_dev *dev = NULL;
	struct ddr_priority_nic *nic = NULL;
	struct ddr_priority_mdc *mdc = NULL;
	struct ddr_priority_force *force = NULL;

	if (!dst || !src)
		return -EINVAL;

	info = kmemdup(src, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->dmc_force = NULL;

	if (src->dev_num > 0 && src->dev) {
		dev = kmemdup(src->dev, src->dev_num * sizeof(*dev), GFP_KERNEL);
		if (!dev)
			goto err;
	}
	if (src->nic_num > 0 && src->nic) {
		nic = kmemdup(src->nic, src->nic_num * sizeof(*nic), GFP_KERNEL);
		if (!nic)
			goto err;
	}
	if (src->mdc_num > 0 && src->mdc) {
		mdc = kmemdup(src->mdc, src->mdc_num * sizeof(*mdc), GFP_KERNEL);
		if (!mdc)
			goto err;
	}
	if (src->dmc_force_num > 0 && src->dmc_force) {
		force = kmemdup(src->dmc_force, src->dmc_force_num * sizeof(*force), GFP_KERNEL);
		if (!force)
			goto err;
	}

	info->dev = dev;
	info->nic = nic;
	info->mdc = mdc;
	info->dmc_force = force;
	*dst = info;
	return 0;

err:
	kfree(dev);
	kfree(nic);
	kfree(mdc);
	kfree(info->dmc_force);
	kfree(info);
	return -ENOMEM;
}

static int priority_tree_build(struct ddr_priority_info *info,
			       struct ddr_port_desc *port_desc,
			       int real_ports,
			       struct sub_dev_desc *sub_dev,
			       struct ddr_priority_tree *tree);

int __init ddr_priority_init(int cpu_type)
{
	struct ddr_priority_info *info = NULL;
	struct ddr_priority_v2 *v2 = NULL;
	int ret;

	switch (cpu_type) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_T7:
		info = &ddr_priority_info_t7;
		break;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	case DMC_TYPE_S4:
		info = &ddr_priority_info_s4;
		break;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_S5:
		info = &ddr_priority_info_s5;
		break;
	case DMC_TYPE_S6:
		info = &ddr_priority_info_s6;
		break;
	case DMC_TYPE_C3:
		info = &ddr_priority_info_c3;
		break;
	case DMC_TYPE_T5M:
		info = &ddr_priority_info_t5m;
		break;
	case DMC_TYPE_TXHD2:
		info = &ddr_priority_info_txhd2;
		break;
	case DMC_TYPE_T3X:
		info = &ddr_priority_info_t3x;
		break;
	case DMC_TYPE_T3:
		info = &ddr_priority_info_t3;
		break;
	case DMC_TYPE_A4:
		info = &ddr_priority_info_a4;
		break;
	case DMC_TYPE_S7:
		info = &ddr_priority_info_s7;
		break;
	case DMC_TYPE_S7D:
		info = &ddr_priority_info_s7d;
		break;
	case DMC_TYPE_SC2:
		info = &ddr_priority_info_sc2;
		break;
	case DMC_TYPE_T5W:
		info = &ddr_priority_info_t5w;
		break;
	case DMC_TYPE_G12B:
		info = &ddr_priority_info_g12b;
		break;
	case DMC_TYPE_GXLX3:
		info = &ddr_priority_info_gxlx3;
		break;
	case DMC_TYPE_T6D:
		info = &ddr_priority_info_t6d;
		break;
	case DMC_TYPE_T6W:
		info = &ddr_priority_info_t6w;
		break;
	case DMC_TYPE_T6X:
		v2 = &ddr_priority_t6x;
		break;
	case DMC_TYPE_C4:
		info = &ddr_priority_info_c4;
		break;
	case DMC_TYPE_A9:
		info = &ddr_priority_info_a9;
		break;
	case DMC_TYPE_C5:
		info = &ddr_priority_info_c5;
		break;
#endif
	case DMC_TYPE_S1A:
		info = &ddr_priority_info_s1a;
		break;
	default:
		return -EINVAL;
	}

	aml_db->priority = kzalloc(sizeof(*aml_db->priority), GFP_KERNEL);
	if (!aml_db->priority)
		return -ENOMEM;

	if (info) {
		ret = ddr_priority_info_alloc(&aml_db->priority->v1, info);
		if (ret) {
			kfree(aml_db->priority);
			aml_db->priority = NULL;
			return ret;
		}
		aml_db->priority->ver = 1;
		aml_db->priority->v1_num = 1;
		aml_db->priority->tree = kzalloc(sizeof(*aml_db->priority->tree),
						 GFP_KERNEL);
		if (aml_db->priority->tree) {
			priority_tree_build(aml_db->priority->v1,
					    aml_db->port_desc,
					    aml_db->real_ports,
					    aml_db->sub_dev,
					    aml_db->priority->tree);
		}
	} else if (v2) {
		aml_db->priority->v2 = ddr_priority_v2_init(v2);
		if (!aml_db->priority->v2) {
			kfree(aml_db->priority);
			aml_db->priority = NULL;
			return -ENOMEM;
		}
		aml_db->priority->ver = 2;
	}

	return 0;
}

int reg_access(unsigned int reg, char sec, char rw, unsigned int *value)
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

static int reg_access_byte(unsigned int reg, char sec, char rw, unsigned int *value)
{
	if (sec) {
		if (rw == READ)
			*value = dmc_rw(reg, 0, DMC_READ);
		else
			dmc_rw(reg, *value, DMC_WRITE);
	} else {
		void __iomem *vaddr;

		vaddr = ioremap(reg, 0x1);
		if (!vaddr) {
			pr_err("reg_addr %x ioremap failed\n", reg);
			return -EINVAL;
		}
		if (rw == READ)
			*value = readb_relaxed(vaddr);
		else
			writeb_relaxed(*value, vaddr);
		iounmap(vaddr);
	}
	return 0;
}

static struct ddr_priority_force *find_force_by_bus(struct ddr_priority_info *info,
						    unsigned int bus)
{
	int i;

	if (!info || !info->dmc_force)
		return NULL;
	for (i = 0; i < info->dmc_force_num; i++) {
		if (info->dmc_force[i].bus == bus)
			return &info->dmc_force[i];
	}
	return NULL;
}

static int write_force_by_bus(unsigned int bus, int value, int rw, int dmc)
{
	struct ddr_priority_force *f;
	void __iomem *base;
	u32 reg, en_reg;
	bool disable = (value < 0);
	unsigned int val = disable ? 0 : (unsigned int)value;
	int start, end, i;

	if (!aml_db || !aml_db->priority || !aml_db->priority->v1)
		return -EINVAL;

	f = find_force_by_bus(aml_db->priority->v1, bus);
	if (!f) {
		pr_info("dmc bus=%u not found\n", bus);
		return -ENODEV;
	}

	if (dmc >= 0 && dmc >= aml_db->dmc_number) {
		pr_info("dmc index %d out of range (0-%d)\n",
			dmc, aml_db->dmc_number - 1);
		return -EINVAL;
	}

	start = (dmc < 0) ? 0 : dmc;
	end   = (dmc < 0) ? aml_db->dmc_number : dmc + 1;

	for (i = start; i < end; i++) {
		base = aml_db->ddr_reg[i];
		if (!base)
			continue;

		if (rw < 0 || rw == 0) {
			reg = readl_relaxed(base + f->r_offset);
			reg &= ~GENMASK_U32(f->r_lsb + f->r_nbits - 1, f->r_lsb);
			reg |= (val & GENMASK_U32(f->r_nbits - 1, 0)) << f->r_lsb;
			if (f->r_force_mask > 0 && f->r_force_mask_offset == f->r_offset) {
				if (disable)
					reg &= ~f->r_force_mask;
				else
					reg |= f->r_force_mask;
			}
			writel_relaxed(reg, base + f->r_offset);

			if (f->r_force_mask > 0 && f->r_force_mask_offset != f->r_offset) {
				en_reg = readl_relaxed(base + f->r_force_mask_offset);
				if (disable)
					en_reg &= ~f->r_force_mask;
				else
					en_reg |= f->r_force_mask;
				writel_relaxed(en_reg, base + f->r_force_mask_offset);
			}
		}

		if (rw < 0 || rw == 1) {
			reg = readl_relaxed(base + f->w_offset);
			reg &= ~GENMASK_U32(f->w_lsb + f->w_nbits - 1, f->w_lsb);
			reg |= (val & GENMASK_U32(f->w_nbits - 1, 0)) << f->w_lsb;
			if (f->w_force_mask > 0 && f->w_force_mask_offset == f->w_offset) {
				if (disable)
					reg &= ~f->w_force_mask;
				else
					reg |= f->w_force_mask;
			}
			writel_relaxed(reg, base + f->w_offset);

			if (f->w_force_mask > 0 && f->w_force_mask_offset != f->w_offset) {
				en_reg = readl_relaxed(base + f->w_force_mask_offset);
				if (disable)
					en_reg &= ~f->w_force_mask;
				else
					en_reg |= f->w_force_mask;
				writel_relaxed(en_reg, base + f->w_force_mask_offset);
			}
		}
	}

	return 0;
}

static struct ddr_priority_node *find_root_by_port_id(struct ddr_priority_tree *tree,
						      unsigned int port_id)
{
	int i;

	if (!tree)
		return NULL;
	for (i = 0; i < tree->nr_roots; i++) {
		if (tree->roots[i] && tree->roots[i]->port_id == port_id)
			return tree->roots[i];
	}
	return NULL;
}

struct ddr_priority_node *find_node_by_dev_path(struct ddr_priority_tree *tree,
						       unsigned int port_id,
						       int *sub_path, int depth, int rw)
{
	int i, found;
	struct ddr_priority_node *n, *match = NULL;
	unsigned int group_id = 0, gid_r, gid_w;
	unsigned char module_id = 0;

	if (depth >= 2) {
		group_id = GROUP_LEVEL3 | ((unsigned int)sub_path[0] << 0);
		if (depth >= 3) {
			group_id |= GROUP_LEVEL4 | ((unsigned int)sub_path[1] << 8);
			if (depth >= 4)
				group_id |= GROUP_LEVEL5 | ((unsigned int)sub_path[2] << 16);
		}
		if (rw == 1)
			group_id |= GROUP_IS_W;
		module_id = sub_path[depth - 1];
	}

	for (i = 0; i < tree->nr_nodes; i++) {
		n = &tree->nodes[i];
		if (n->port_id != port_id)
			continue;
		if (depth >= 2) {
			if (n->group_id == group_id && n->module_id == module_id) {
				match = n;
				break;
			}
		} else if (depth == 1) {
			if (n->sub_id == sub_path[0] && !n->group_id) {
				match = n;
				break;
			}
		}
	}

	if (depth >= 2 && rw < 0) {
		gid_r = group_id & ~GROUP_IS_W;
		gid_w = group_id | GROUP_IS_W;
		found = 0;

		for (i = 0; i < tree->nr_nodes; i++) {
			n = &tree->nodes[i];

			if (n->port_id == port_id && n->module_id == module_id &&
			    (n->group_id == gid_r || n->group_id == gid_w))
				found++;
		}
		if (found > 1)
			return ERR_PTR(-EINVAL);
		if (found == 1) {
			for (i = 0; i < tree->nr_nodes; i++) {
				n = &tree->nodes[i];
				if (n->port_id == port_id && n->module_id == module_id &&
				    (n->group_id == gid_r || n->group_id == gid_w))
					return n;
			}
		}
	}

	return match;
}

static bool prio_reg_power_en(enum ddr_priority_ctrl ctrl_type)
{
	if (ctrl_type != PRIO_CTRL_POWER)
		return true;
	return aml_db->priority->flag & DDR_PRIORITY_POWER;
}

static int check_reg_ctrl(struct ddr_priority_reg *reg, int is_w, bool accessible)
{
	if (!reg)
		return -EINVAL;

	switch (reg->ctrl_type) {
	case PRIO_CTRL_NOT_SUPPORT:
	case PRIO_CTRL_HW_AUTO:
	case PRIO_CTRL_SW_AUTO:
		return -EOPNOTSUPP;
	case PRIO_CTRL_POWER:
		if (!accessible && !prio_reg_power_en(reg->ctrl_type))
			return -EPERM;
		return 0;
	case PRIO_CTRL_REG_WO:
		if (!is_w)
			return -EOPNOTSUPP;
		return 0;
	case PRIO_CTRL_REG_RW:
	case PRIO_CTRL_FIXED:
	case PRIO_CTRL_HANDLER:
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

int write_node_layer(struct ddr_priority_node *node, char type, int value,
						int rw, int dmc, void *data)
{
	struct ddr_priority_reg *reg_ptr;
	struct ddr_priority_dev *dev;
	void __iomem *base;
	unsigned long off;
	unsigned char lsb, nbits;
	u32 reg;
	int ret = 0;
	bool disable = (value < 0);
	unsigned int val = disable ? 0 : (unsigned int)value;

	if (!node)
		return -EINVAL;

	switch (type) {
	case 'n':
		if (!node->nic)
			return -ENODEV;
		reg_ptr = &node->nic->reg;
		break;
	case 'd':
		if (!node->dev)
			return -ENODEV;
		reg_ptr = &node->dev->reg;
		break;
	case 'm':
		if (!node->mdc)
			return -ENODEV;
		reg_ptr = &node->mdc->reg;
		break;
	default:
		return -EINVAL;
	}

	if (reg_ptr->ctrl_type == PRIO_CTRL_FIXED) {
		pr_info("%s: ctrl_type fixed, not writable\n", node->name);
		return -EOPNOTSUPP;
	}

	if (reg_ptr->ctrl_type == PRIO_CTRL_HANDLER) {
		dev = (type == 'd') ? node->dev : NULL;

		if (dev && dev->handle_set) {
			if (rw < 0 || rw == 0)
				ret = dev->handle_set(node, val, 0, data);
			if (rw < 0 || rw == 1)
				ret = dev->handle_set(node, val, 1, data);
			return ret;
		}
		pr_info("%s: ctrl_type handler, no handle_set\n", node->name);
		return -EOPNOTSUPP;
	}

	ret = check_reg_ctrl(reg_ptr, 1, node->accessible);
	if (ret) {
		pr_info("%s: ctrl_type %d not writable\n",
			node->name, reg_ptr->ctrl_type);
		return ret;
	}

	if (dmc >= 0 && dmc < aml_db->dmc_number)
		base = aml_db->ddr_reg[dmc];
	else
		base = aml_db->ddr_reg[0];

	if (!base)
		return -ENODEV;

	if (rw < 0 || rw == 0) {
		off = reg_ptr->r_offset;
		lsb = reg_ptr->r_lsb;
		nbits = reg_ptr->r_nbits;

		ret = reg_access(reg_ptr->reg_base + off, reg_ptr->reg_mode, DMC_READ, &reg);
		if (ret < 0)
			return ret;

		reg &= ~GENMASK_U32(lsb + nbits - 1, lsb);
		reg |= (val & GENMASK_U32(nbits - 1, 0)) << lsb;
		if (reg_ptr->r_force_mask > 0) {
			if (disable)
				reg &= ~reg_ptr->r_force_mask;
			else
				reg |= reg_ptr->r_force_mask;
		}

		ret = reg_access(reg_ptr->reg_base + off, reg_ptr->reg_mode, DMC_WRITE, &reg);
		if (ret < 0)
			return ret;
	}

	if (rw < 0 || rw == 1) {
		off = reg_ptr->w_offset;
		lsb = reg_ptr->w_lsb;
		nbits = reg_ptr->w_nbits;

		ret = reg_access(reg_ptr->reg_base + off, reg_ptr->reg_mode, DMC_READ, &reg);
		if (ret < 0)
			return ret;

		reg &= ~GENMASK_U32(lsb + nbits - 1, lsb);
		reg |= (val & GENMASK_U32(nbits - 1, 0)) << lsb;
		if (reg_ptr->w_force_mask > 0) {
			if (disable)
				reg &= ~reg_ptr->w_force_mask;
			else
				reg |= reg_ptr->w_force_mask;
		}

		ret = reg_access(reg_ptr->reg_base + off, reg_ptr->reg_mode, DMC_WRITE, &reg);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int priority_tree_write(const char *buf, size_t count)
{
	char line[128], line_copy[128], path_buf[128];
	char *argv[5], *p, *q, *tok, *next, *name, *first_num;
	int argc, i, depth, name_rw, match_count, err, tmp;
	char *p2;
	unsigned char data_val = 0;
	void *data_ptr = NULL;
	char type = 0;
	unsigned int id, port_id;
	int value, sub_path[4];
	int rw = -1;
	int dmc_idx = -1;
	struct ddr_priority_node *node, *match;
	struct ddr_priority_tree *tree;

	if (!aml_db || !aml_db->priority || !aml_db->priority->tree)
		return -EINVAL;

	if (count >= sizeof(line))
		return -EINVAL;
	memcpy(line, buf, count);
	line[count] = '\0';

	p = strim(line);
	memcpy(line_copy, line, strlen(line) + 1);
	argc = 0;
	for (i = 0; i < 5; i++) {
		tok = strsep(&p, " \t\n\r");
		if (!tok)
			break;
		if (*tok)
			argv[argc++] = tok;
	}

	if (!argc)
		return -EINVAL;

	if (argc < 2) {
		if (!strcmp(argv[0], "hevc")) {
			if (aml_db->prio_ops && aml_db->prio_ops->show_hevc_priority_setting)
				return aml_db->prio_ops->show_hevc_priority_setting();
		} else if (!strcmp(argv[0], "hcodec")) {
			if (aml_db->prio_ops && aml_db->prio_ops->show_hcodec_priority_setting)
				return aml_db->prio_ops->show_hcodec_priority_setting();
		}
		pr_info("param is few\n");
		return -EINVAL;
	}

	if (!strncmp(argv[0], "dmc", 3)) {
		type = 'f';
		if (argv[0][3] != '\0' && kstrtoint(argv[0] + 3, 0, &dmc_idx)) {
			pr_info("invalid dmc index: %s\n", argv[0]);
			return -EINVAL;
		}
	} else if (!strcmp(argv[0], "nic")) {
		type = 'n';
	} else if (!strcmp(argv[0], "dev")) {
		type = 'd';
	} else {
		tree = aml_db->priority->tree;
		name_rw = -1;
		match = NULL;
		match_count = 0;
		name = line_copy;
		first_num = NULL;

		p = name;
		while (*p) {
			if (*p == ' ' || *p == '\t') {
				q = p + 1;
				while (*q == ' ' || *q == '\t')
					q++;
				if (*q && (*q == '-' || *q == '+' || (*q >= '0' && *q <= '9'))) {
					*p = '\0';
					first_num = q;
					break;
				}
			}
			p++;
		}
		if (!first_num) {
			pr_info("usage: echo <name> <val> [r|w] [data]\n");
			return -EINVAL;
		}

		p = first_num;
		tok = strchr(p, ' ');
		if (!tok) {
			if (kstrtoint(p, 0, &value)) {
				pr_info("invalid value: %s\n", p);
				return -EINVAL;
			}
			goto do_match;
		}
		*tok = '\0';
		if (kstrtoint(p, 0, &value)) {
			pr_info("invalid value: %s\n", p);
			return -EINVAL;
		}
		p = tok + 1;
		while (*p == ' ' || *p == '\t')
			p++;

		if (p[0] && (p[1] == ' ' || p[1] == '\t' || p[1] == '\0') &&
		    (p[0] == 'r' || p[0] == 'R' || p[0] == 'w' || p[0] == 'W')) {
			name_rw = (p[0] == 'w' || p[0] == 'W') ? 1 : 0;
			p++;
			while (*p == ' ' || *p == '\t')
				p++;
		}

		if (*p && !kstrtoint(p, 0, &tmp) && tmp >= 0 && tmp <= 255) {
			data_val = tmp;
			data_ptr = &data_val;
		}

do_match:
		p2 = name + strlen(name);
		while (p2 > name && (*(p2 - 1) == ' ' || *(p2 - 1) == '\t'))
			p2--;
		*p2 = '\0';
		if (!*name) {
			pr_info("usage: echo <name> <val> [r|w] [data]\n");
			return -EINVAL;
		}

		for (i = 0; i < tree->nr_nodes; i++) {
			node = &tree->nodes[i];
			if (!node->name || strcmp(node->name, name))
				continue;
			if (name_rw >= 0 && node->rw >= 0 && node->rw != name_rw)
				continue;
			if (!node->dev || (node->prop_mode != PRIO_PROP_NONE &&
			    node->prop_mode != PRIO_PROP_PARTIAL))
				continue;
			match = node;
			match_count++;
		}

		if (!match_count) {
			pr_info("dev name '%s' not found or not writable\n", name);
			return -ENODEV;
		}
		if (match_count > 1 && name_rw < 0) {
			pr_info("dev name '%s' multiple, need to r or w\n", name);
			return -EEXIST;
		}
		if (name_rw < 0 && match->rw >= 0)
			name_rw = match->rw;
		return write_node_layer(match, 'd', value, name_rw, -1, data_ptr);
	}

	if (argc < 3) {
		pr_info("param is few\n");
		return -EINVAL;
	}

	if (type != 'd' && kstrtouint(argv[1], 0, &id)) {
		pr_info("invalid id: %s\n", argv[1]);
		return -EINVAL;
	}

	if (kstrtoint(argv[2], 0, &value)) {
		pr_info("invalid value: %s\n", argv[2]);
		return -EINVAL;
	}

	if (argc >= 4) {
		if (argv[3][0] == 'r' || argv[3][0] == 'R') {
			rw = 0;
		} else if (argv[3][0] == 'w' || argv[3][0] == 'W') {
			rw = 1;
		} else if (!kstrtoint(argv[3], 0, &tmp) && tmp >= 0 && tmp <= 255) {
			data_val = tmp;
			data_ptr = &data_val;
		} else {
			pr_info("invalid rw: %s, use r or w\n", argv[3]);
			return -EINVAL;
		}
	}

	if (argc >= 5 && type != 'f') {
		if (!kstrtoint(argv[4], 0, &tmp) && tmp >= 0 && tmp <= 255) {
			data_val = tmp;
			data_ptr = &data_val;
		}
	}

	if (type == 'f')
		return write_force_by_bus(id, value, rw, dmc_idx);

	if (type == 'n') {
		node = find_root_by_port_id(aml_db->priority->tree, id);
		if (!node) {
			pr_info("nic port_id=%u not found\n", id);
			return -ENODEV;
		}
		return write_node_layer(node, type, value, rw, -1, NULL);
	}

	depth = 0;
	strscpy(path_buf, argv[1], sizeof(path_buf));
	tok = path_buf;

	next = strchr(tok, ':');
	if (next) {
		*next = '\0';
		if (kstrtouint(tok, 0, &port_id))
			return -EINVAL;
		tok = next + 1;
		while (tok && depth < 4) {
			next = strchr(tok, ':');
			if (next)
				*next = '\0';
			if (kstrtoint(tok, 0, &sub_path[depth]))
				return -EINVAL;
			depth++;
			tok = next ? next + 1 : NULL;
		}
		node = find_node_by_dev_path(aml_db->priority->tree,
					     port_id, sub_path, depth, rw);
	} else {
		if (kstrtouint(argv[1], 0, &port_id))
			return -EINVAL;
		node = find_root_by_port_id(aml_db->priority->tree, port_id);
		if (node && node->prop_mode != PRIO_PROP_NONE &&
		    node->prop_mode != PRIO_PROP_PARTIAL) {
			pr_info("dev port_id=%u prop_mode=%d not writable\n",
				port_id, node->prop_mode);
			return -EOPNOTSUPP;
		}
	}

	if (IS_ERR(node)) {
		err = PTR_ERR(node);
		if (err == -EINVAL)
			pr_info("dev path %s has both R and W, must specify r or w\n",
				argv[1]);
		else
			pr_info("dev path %s not found\n", argv[1]);
		return err;
	}
	if (!node || !node->dev) {
		pr_info("dev %s has no register\n", argv[1]);
		return -ENODEV;
	}
	if (node->prop_mode != PRIO_PROP_NONE &&
	    node->prop_mode != PRIO_PROP_PARTIAL) {
		pr_info("dev %s prop_mode=%d not writable\n",
			argv[1], node->prop_mode);
		return -EOPNOTSUPP;
	}
	if (rw < 0 && node->rw >= 0)
		rw = node->rw;
	return write_node_layer(node, 'd', value, rw, -1, data_ptr);
}

static int decode_bus_for_dmc(int bus, int dmc_idx)
{
	int shift;
	unsigned char byte;

	if (!(aml_db->soc_feature & DMC_ASYMMETRY))
		return bus;

	shift = dmc_idx * 8;
	byte = (bus >> shift) & 0xFF;
	if (!(byte & 1))
		return -1;
	return (byte >> 1) & 0x7F;
}

static struct ddr_priority_nic *priority_find_nic(struct ddr_priority_info *info,
						  unsigned char port_id, signed char sub_id)
{
	int i;

	if (!info || !info->nic)
		return NULL;
	for (i = 0; i < info->nic_num; i++) {
		if (info->nic[i].port_id == port_id && info->nic[i].sub_id == sub_id)
			return &info->nic[i];
	}
	return NULL;
}

static struct ddr_priority_dev *priority_find_dev(struct ddr_priority_info *info,
						  unsigned char port_id, signed char sub_id,
						  unsigned int group_id, signed short module_id,
						  int rw)
{
	int i;
	struct ddr_priority_dev *wildcard_l2 = NULL;
	struct ddr_priority_dev *wildcard_l3 = NULL;

	if (!info || !info->dev)
		return NULL;
	for (i = 0; i < info->dev_num; i++) {
		if (info->dev[i].port_id != port_id)
			continue;

		/* Level 3+: match by group_id + module_id */
		if (module_id) {
			if (info->dev[i].group_id == group_id &&
			    info->dev[i].module_id == module_id)
				return &info->dev[i];

			if (info->dev[i].group_id == group_id &&
			    info->dev[i].module_id == MODULE_ID_MATCH_ALL)
				wildcard_l3 = &info->dev[i];
			continue;
		}

		/* Level 1/2: match by sub_id, skip level 3 entries */
		if (!info->dev[i].module_id) {
			if (info->dev[i].sub_id == sub_id)
				return &info->dev[i];

			if (info->dev[i].sub_id == SUB_ID_MATCH_ALL)
				wildcard_l2 = &info->dev[i];
			else if (info->dev[i].sub_id == SUB_ID_MATCH_R && rw == 0)
				wildcard_l2 = &info->dev[i];
			else if (info->dev[i].sub_id == SUB_ID_MATCH_W && rw == 1)
				wildcard_l2 = &info->dev[i];
		}
	}

	return wildcard_l2 ? wildcard_l2 : wildcard_l3;
}

static struct ddr_priority_mdc *priority_find_mdc(struct ddr_priority_info *info,
						  unsigned char port_id, signed char sub_id)
{
	int i;

	if (!info || !info->mdc)
		return NULL;
	for (i = 0; i < info->mdc_num; i++) {
		if (info->mdc[i].port_id == port_id && info->mdc[i].sub_id == sub_id)
			return &info->mdc[i];
	}
	return NULL;
}

static void populate_node_layers(struct ddr_priority_node *node, struct ddr_priority_info *info)
{
	struct ddr_priority_dev *dev;
	struct ddr_priority_nic *nic;
	struct ddr_priority_mdc *mdc;
	int dmc_num = aml_db->dmc_number;

	dev = priority_find_dev(info, node->port_id, node->sub_id,
				node->group_id, node->module_id, node->rw);
	nic = priority_find_nic(info, node->port_id, node->sub_id);
	mdc = priority_find_mdc(info, node->port_id, node->sub_id);

	if (node->bus >= 0) {
		node->dmc_force = kcalloc(dmc_num, sizeof(*node->dmc_force),
					  GFP_KERNEL);
		if (node->dmc_force) {
			int d, bus;

			for (d = 0; d < dmc_num; d++) {
				bus = decode_bus_for_dmc(node->bus, d);
				if (bus >= 0)
					node->dmc_force[d] =
						find_force_by_bus(info, bus);
			}
		}
	}

	node->accessible = true;

	if (dev) {
		node->dev = dev;
		node->prop_mode = dev->prop_mode;
		if (dev->reg.ctrl_type == PRIO_CTRL_POWER)
			node->accessible = false;
	}

	if (nic)
		node->nic = nic;

	if (mdc)
		node->mdc = mdc;
}

static int count_sub_port_children(struct sub_port_list *list, int num)
{
	int i, total = 0;

	for (i = 0; i < num; i++) {
		total++;
		if (list[i].children && list[i].nr_children > 0)
			total += count_sub_port_children(list[i].children,
							 list[i].nr_children);
	}
	return total;
}

static int count_tree_nodes(struct ddr_port_desc *port_desc, int real_ports,
			    struct sub_dev_desc *sub_dev)
{
	int i, j, total = 0;

	/* Count root nodes (sub_id == -1) and their level-2 children */
	for (i = 0; i < real_ports; i++) {
		if (port_desc[i].sub_id != -1)
			continue;
		total++; /* root */

		/* Count level-2 children from port_desc */
		for (j = 0; j < real_ports; j++) {
			if (j != i && port_desc[j].port_id == port_desc[i].port_id &&
			    port_desc[j].sub_id >= 0)
				total++;
		}

		/* Count level-3+ children from sub_dev_desc */
		if (sub_dev) {
			for (j = 0; j < sub_dev->num; j++) {
				struct sub_dev *sd = sub_dev->subs[j];

				if (!sd)
					continue;
				if (sd->sub_r.port_id == port_desc[i].port_id &&
				    sd->sub_r.list && sd->sub_r.num > 0)
					total += count_sub_port_children(sd->sub_r.list,
									sd->sub_r.num);
				if (sd->sub_w.port_id == port_desc[i].port_id &&
				    sd->sub_w.list && sd->sub_w.num > 0)
					total += count_sub_port_children(sd->sub_w.list,
									sd->sub_w.num);
			}
		}
	}
	return total;
}

static struct ddr_priority_node *build_sub_tree(struct sub_port_list *entry,
						struct ddr_priority_node *parent,
						struct ddr_priority_info *info,
						unsigned char port_id,
						int rw,
						struct ddr_priority_node *nodes,
						int *node_idx)
{
	struct ddr_priority_node *node;
	int i;

	if (!entry || !nodes || !node_idx)
		return NULL;

	if (parent->group_id || parent->sub_id >= 0) {
		unsigned int p_mask = parent->group_id & GROUP_LEVEL_MASK;

		if (p_mask == GROUP_LEVEL5) {
			pr_warn("ddr_priority: max depth (level 5) reached, skipping %s\n",
				entry->name);
			return NULL;
		}
		if (p_mask && p_mask != GROUP_LEVEL3 && p_mask != GROUP_LEVEL4) {
			pr_warn("ddr_priority: unknown parent level for %s\n",
				entry->name);
			return NULL;
		}
	}

	node = &nodes[*node_idx];
	(*node_idx)++;

	node->name = entry->name;
	node->port_id = port_id;
	node->sub_id = entry->id;
	node->bus = parent->bus;
	node->parent = parent;
	node->rw = rw;

	/* Level 3+: set group_id and module_id for DEV matching */
	if (parent->group_id || parent->sub_id >= 0) {
		unsigned int callbacks = parent->group_id & ~(1U << 31) & ~GROUP_LEVEL_MASK;
		unsigned int level_mark;

		if (!(parent->group_id & GROUP_LEVEL_MASK))
			level_mark = GROUP_LEVEL3;
		else if (parent->group_id & GROUP_LEVEL3)
			level_mark = GROUP_LEVEL4;
		else
			level_mark = GROUP_LEVEL5;

		node->group_id = callbacks | level_mark
			       | ((unsigned int)parent->sub_id << (callbacks ? 8 : 0))
			       | (rw ? (1U << 31) : 0);
		node->module_id = entry->id;
	}

	populate_node_layers(node, info);

	if (entry->children && entry->nr_children > 0) {
		node->nr_children = entry->nr_children;
		node->children = kcalloc(node->nr_children,
					 sizeof(struct ddr_priority_node *),
					 GFP_KERNEL);
		if (node->children) {
			for (i = 0; i < entry->nr_children; i++) {
				node->children[i] = build_sub_tree(&entry->children[i],
								   node, info, port_id,
								   rw, nodes, node_idx);
			}
		}
	}

	return node;
}

static int root_bus_cmp(const void *a, const void *b)
{
	const struct ddr_priority_node *na = *(const struct ddr_priority_node **)a;
	const struct ddr_priority_node *nb = *(const struct ddr_priority_node **)b;
	int ret = na->bus - nb->bus;

	if (ret)
		return ret;
	ret = na->port_id - nb->port_id;
	if (ret)
		return ret;
	return na->sub_id - nb->sub_id;
}

static int build_sub_dev_children(struct ddr_priority_tree *tree,
				  struct ddr_priority_node *node,
				  struct sub_dev_desc *sub_dev,
				  unsigned char port_id,
				  struct ddr_priority_info *info,
				  int *node_idx)
{
	int j, k, rw;
	struct sub_dev *sd;
	struct sub_dev_port *sdp;
	struct ddr_priority_node *parent;
	struct ddr_priority_node *child;
	struct ddr_priority_node **tmp;

	for (j = 0; j < sub_dev->num; j++) {
		sd = sub_dev->subs[j];
		if (!sd)
			continue;

		for (rw = 0; rw < 2; rw++) {
			sdp = rw ? &sd->sub_w : &sd->sub_r;

			if (sdp->port_id != port_id)
				continue;
			if (!sdp->list || sdp->num <= 0)
				continue;

			if (sdp->parent_sub_id == -1) {
				parent = node;
			} else {
				parent = NULL;
				for (k = 0; k < node->nr_children; k++) {
					if (node->children[k] &&
					    node->children[k]->sub_id == sdp->parent_sub_id) {
						parent = node->children[k];
						break;
					}
				}
				if (!parent)
					parent = node;
			}

			for (k = 0; k < sdp->num; k++) {
				child = build_sub_tree(&sdp->list[k],
						       parent, info,
						       sdp->port_id,
						       rw,
						       tree->nodes,
						       node_idx);
				if (child) {
					tmp = krealloc(parent->children,
						       (parent->nr_children + 1) *
						       sizeof(struct ddr_priority_node *),
						       GFP_KERNEL);
					if (!tmp) {
						parent->children = NULL;
						kfree(tree->nodes);
						kfree(tree->roots);
						return -ENOMEM;
					}
					parent->children = tmp;
					parent->children[parent->nr_children++] = child;
				}
			}
		}
	}

	return 0;
}

static int priority_tree_build(struct ddr_priority_info *info,
			       struct ddr_port_desc *port_desc,
			       int real_ports,
			       struct sub_dev_desc *sub_dev,
			       struct ddr_priority_tree *tree)
{
	int i, j, ret, node_idx = 0;
	int root_count = 0;
	struct ddr_priority_node *node;
	struct ddr_priority_node *child;
	struct ddr_priority_node **tmp;

	if (!info || !port_desc || !tree)
		return -EINVAL;

	tree->nr_nodes = count_tree_nodes(port_desc, real_ports, sub_dev);
	if (tree->nr_nodes == 0)
		return 0;

	tree->nodes = kcalloc(tree->nr_nodes, sizeof(struct ddr_priority_node), GFP_KERNEL);
	if (!tree->nodes)
		return -ENOMEM;

	for (i = 0; i < real_ports; i++) {
		if (port_desc[i].sub_id == -1)
			root_count++;
	}
	tree->nr_roots = root_count;
	tree->roots = kcalloc(root_count, sizeof(struct ddr_priority_node *), GFP_KERNEL);
	if (!tree->roots) {
		kfree(tree->nodes);
		return -ENOMEM;
	}

	root_count = 0;
	for (i = 0; i < real_ports; i++) {
		if (port_desc[i].sub_id != -1)
			continue;

		node = &tree->nodes[node_idx++];
		node->name = port_desc[i].port_name;
		node->port_id = port_desc[i].port_id;
		node->sub_id = -1;
		node->bus = port_desc[i].bus;
		node->rw = -1;

		populate_node_layers(node, info);

		tree->roots[root_count++] = node;

		for (j = 0; j < real_ports; j++) {
			if (j == i)
				continue;
			if (port_desc[j].port_id != port_desc[i].port_id)
				continue;
			if (port_desc[j].sub_id < 0)
				continue;

			child = &tree->nodes[node_idx++];
			child->name = port_desc[j].port_name;
			child->port_id = port_desc[j].port_id;
			child->sub_id = port_desc[j].sub_id;
			child->bus = port_desc[j].bus;
			child->parent = node;
			child->rw = -1;

			populate_node_layers(child, info);

			tmp = krealloc(node->children,
				       (node->nr_children + 1) *
				       sizeof(struct ddr_priority_node *),
				       GFP_KERNEL);
			if (!tmp) {
				node->children = NULL;
				kfree(tree->nodes);
				kfree(tree->roots);
				return -ENOMEM;
			}
			node->children = tmp;
			node->children[node->nr_children++] = child;
		}

		if (sub_dev) {
			ret = build_sub_dev_children(tree, node, sub_dev,
						     port_desc[i].port_id,
						     info, &node_idx);
			if (ret)
				return ret;
		}
	}

	sort(tree->roots, tree->nr_roots, sizeof(tree->roots[0]),
	     root_bus_cmp, NULL);

	return 0;
}

#define PRIO_V1_BUF_SZ		(1024 * 1024)
#define PRIO_COL_WIDTH		24
#define BUS_COL_WIDTH		16

#define MAX_BUS_PORT	32

static unsigned int prio_get_real_val(unsigned int val, unsigned char lsb,
					      unsigned char nbits)
{
	if (!nbits || nbits >= 32 || lsb + nbits > 32U)
		return 0;

	return (val >> lsb) & GENMASK_U32(nbits - 1, 0);
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
		reg_access_byte(dm->reg_base + dm->dvb_t2_reg, 0, READ, &value);
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
	struct sub_dev_desc *desc;
	struct sub_dev *sub;
	char rw_char = rw ? 'w' : 'r';
	char *name;
	unsigned char port_id;

	desc = aml_db ? aml_db->sub_dev : NULL;
	if (!desc || axi >= desc->num)
		return NULL;

	sub = desc->subs[axi];
	if (!sub)
		return NULL;

	if (rw)
		port_id = sub->sub_w.port_id;
	else
		port_id = sub->sub_r.port_id;

	name = sub_dev_to_port(port_id, sid, rw_char);
	if (!name)
		name = sub_dev_to_port(port_id, sid & 0x0f, rw_char);
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
static ssize_t priority_v2_display(char *buf, loff_t pos, size_t count)
{
	int i, j;
	ssize_t s = 0;
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

static DEFINE_MUTEX(prio_v1_buf_mutex);
static int format_layer_value(char *buf, size_t bufsz,
			      struct ddr_priority_node *node, char type, int is_w)
{
	unsigned int val;
	unsigned char nbits;
	unsigned long addr;
	int ret, en;
	char tmp[16];
	struct ddr_priority_reg *reg;
	struct ddr_priority_dev *dev = NULL;

	if (!node)
		return 0;

	switch (type) {
	case 'n':
		if (!node->nic)
			return 0;
		reg = &node->nic->reg;
		break;
	case 'd':
		if (!node->dev)
			return 0;
		reg = &node->dev->reg;
		dev = node->dev;
		break;
	case 'm':
		if (!node->mdc)
			return 0;
		reg = &node->mdc->reg;
		break;
	default:
		return 0;
	}

	switch (reg->ctrl_type) {
	case PRIO_CTRL_NOT_SUPPORT:
		return 0;
	case PRIO_CTRL_FIXED:
		val = is_w ? reg->fixed_w : reg->fixed_r;
		return scnprintf(buf, bufsz, "%x:-", val);
	case PRIO_CTRL_HANDLER:
		nbits = is_w ? reg->w_nbits : reg->r_nbits;
		if (dev && dev->handle_get) {
			ret = dev->handle_get(node, is_w);
			if (ret >= 0)
				return scnprintf(buf, bufsz, "%x:%x", ret,
							nbits ? (1U << nbits) - 1 : 0);
		}
		return scnprintf(buf, bufsz, "h:%x", nbits ? (1U << nbits) - 1 : 0);
	case PRIO_CTRL_HW_AUTO:
		return scnprintf(buf, bufsz, "h:-");
	case PRIO_CTRL_SW_AUTO:
		return scnprintf(buf, bufsz, "s:-");
	case PRIO_CTRL_POWER:
		if (!prio_reg_power_en(reg->ctrl_type)) {
			nbits = is_w ? reg->w_nbits : reg->r_nbits;
			return scnprintf(buf, bufsz, "p:%x",
					 nbits ? (1U << nbits) - 1 : 0);
		}
		fallthrough;
	case PRIO_CTRL_REG_WO:
		if (!is_w) {
			nbits = reg->r_nbits;
			return scnprintf(buf, bufsz, "wo:%x",
					 nbits ? (1U << nbits) - 1 : 0);
		}
		fallthrough;
	case PRIO_CTRL_REG_RW:
		if (!reg->reg_base) {
			nbits = is_w ? reg->w_nbits : reg->r_nbits;
			return scnprintf(buf, bufsz, "*:%x",
					 nbits ? (1U << nbits) - 1 : 0);
		}
		addr = reg->reg_base + (is_w ? reg->w_offset : reg->r_offset);
		ret = reg_access(addr, reg->reg_mode, DMC_READ, &val);
		if (ret < 0) {
			nbits = is_w ? reg->w_nbits : reg->r_nbits;
			return scnprintf(buf, bufsz, "*:%x",
					 nbits ? (1U << nbits) - 1 : 0);
		}
		if (is_w) {
			val = prio_get_real_val(val, reg->w_lsb, reg->w_nbits);
			nbits = reg->w_nbits;
		} else {
			val = prio_get_real_val(val, reg->r_lsb, reg->r_nbits);
			nbits = reg->r_nbits;
		}
		scnprintf(tmp, sizeof(tmp), "%x", val);
		if (reg->r_force_mask || reg->w_force_mask) {
			en = 0;
			if (is_w && reg->w_force_mask)
				en = (val & reg->w_force_mask) ? 1 : 0;
			else if (!is_w && reg->r_force_mask)
				en = (val & reg->r_force_mask) ? 1 : 0;
			return scnprintf(buf, bufsz, "%s:%x:%u",
					 tmp,
					 nbits ? (1U << nbits) - 1 : 0,
					 en);
		}
		return scnprintf(buf, bufsz, "%s:%x",
				 tmp, nbits ? (1U << nbits) - 1 : 0);
	default:
		return 0;
	}
}

static int format_col(char *buf, size_t bufsz, struct ddr_priority_node *node, char type)
{
	char r[16], w[16];

	r[0] = '\0';
	w[0] = '\0';

	format_layer_value(r, sizeof(r), node, type, 0);
	format_layer_value(w, sizeof(w), node, type, 1);
	if (r[0] || w[0])
		return scnprintf(buf, bufsz, "%s %s", r, w);
	return 0;
}

static bool is_force_enabled(struct ddr_priority_force *f, int is_w,
			     void __iomem *base, unsigned int val)
{
	unsigned int mask_val;

	if (!base || !f)
		return false;

	if (is_w) {
		if (f->w_force_mask != -1) {
			mask_val = readl_relaxed(base + f->w_force_mask_offset);
			return !!(mask_val & f->w_force_mask);
		}
		return !!val;
	}

	if (f->r_force_mask != -1) {
		mask_val = readl_relaxed(base + f->r_force_mask_offset);
		return !!(mask_val & f->r_force_mask);
	}
	return !!val;
}

static int format_force_col(char *buf, size_t bufsz,
			    struct ddr_priority_force *f, void __iomem *base)
{
	unsigned int val;
	unsigned char nbits;
	char r[16], w[16];

	r[0] = '\0';
	w[0] = '\0';

	if (base && f->r_nbits) {
		val = readl_relaxed(base + f->r_offset);
		val = (val >> f->r_lsb) & ((1U << f->r_nbits) - 1);
		nbits = f->r_nbits;
		scnprintf(r, sizeof(r), "%x:%x:%u", val,
			  nbits ? (1U << nbits) - 1 : 0,
			  is_force_enabled(f, 0, base, val));
	}

	if (base && f->w_nbits) {
		val = readl_relaxed(base + f->w_offset);
		val = (val >> f->w_lsb) & ((1U << f->w_nbits) - 1);
		nbits = f->w_nbits;
		scnprintf(w, sizeof(w), "%x:%x:%u", val,
			  nbits ? (1U << nbits) - 1 : 0,
			  is_force_enabled(f, 1, base, val));
	}

	if (r[0] || w[0])
		return scnprintf(buf, bufsz, "%s %s", r, w);
	return 0;
}

static int read_node_reg(struct ddr_priority_reg *reg, int is_w, unsigned int *val)
{
	unsigned long addr;
	int ret;
	unsigned char lsb, nbits;

	if (!reg)
		return -EINVAL;

	switch (reg->ctrl_type) {
	case PRIO_CTRL_FIXED:
		*val = is_w ? reg->fixed_w : reg->fixed_r;
		return 0;
	case PRIO_CTRL_HW_AUTO:
	case PRIO_CTRL_SW_AUTO:
	case PRIO_CTRL_HANDLER:
	case PRIO_CTRL_NOT_SUPPORT:
		return -EOPNOTSUPP;
	case PRIO_CTRL_POWER:
		return -EPERM;
	case PRIO_CTRL_REG_WO:
		if (!is_w)
			return -EOPNOTSUPP;
		fallthrough;
	case PRIO_CTRL_REG_RW:
		break;
	default:
		return -EINVAL;
	}

	if (!reg->reg_base)
		return -ENODEV;

	addr = reg->reg_base + (is_w ? reg->w_offset : reg->r_offset);
	ret = reg_access(addr, reg->reg_mode, DMC_READ, val);
	if (ret < 0)
		return ret;

	lsb = is_w ? reg->w_lsb : reg->r_lsb;
	nbits = is_w ? reg->w_nbits : reg->r_nbits;
	*val = (*val >> lsb) & ((1U << nbits) - 1);
	return 0;
}

static int read_node_layer(struct ddr_priority_node *node, char type,
			   int is_w, unsigned int *val)
{
	struct ddr_priority_reg *reg;
	struct ddr_priority_dev *dev = NULL;
	int ret;

	if (!node)
		return -EINVAL;

	switch (type) {
	case 'd':
		if (!node->dev)
			return -ENODEV;
		reg = &node->dev->reg;
		dev = node->dev;
		break;
	case 'n':
		if (!node->nic)
			return -ENODEV;
		reg = &node->nic->reg;
		break;
	case 'm':
		if (!node->mdc)
			return -ENODEV;
		reg = &node->mdc->reg;
		break;
	default:
		return -EINVAL;
	}

	ret = check_reg_ctrl(reg, is_w, node->accessible);
	if (ret)
		return ret;

	if (reg->ctrl_type == PRIO_CTRL_HANDLER) {
		if (dev && dev->handle_get) {
			ret = dev->handle_get(node, is_w);
			if (ret < 0)
				return -EOPNOTSUPP;
			*val = ret;
			return 0;
		}
		return -EOPNOTSUPP;
	}

	return read_node_reg(reg, is_w, val);
}

int get_final_prio_val(struct ddr_priority_node *node, int is_w,
				int dmc_idx, unsigned int *out_val);

static int get_dev_layer_val(struct ddr_priority_node *node,
			     int is_w, int dmc_idx, unsigned int *out_val)
{
	int ret, i, found;
	unsigned int val, chi_nb, cur_nb, chi_lsb, cur_lsb, chi_val, first_chi_val;

	if (!node->dev)
		return -ENODEV;

	switch (node->dev->prop_mode) {
	case PRIO_PROP_NONE:
		return read_node_layer(node, 'd', is_w, out_val);
	case PRIO_PROP_FULL:
	case PRIO_PROP_FIXED:
		if (node->nr_children == 0)
			return read_node_layer(node, 'd', is_w, out_val);
		found = 0;
		for (i = 0; i < node->nr_children; i++) {
			if (node->children[i] && (node->children[i]->rw == is_w ||
							node->children[i]->rw == -1)) {
				ret = get_final_prio_val(node->children[i],
							       is_w,
							       dmc_idx, &chi_val);
				if (ret)
					return ret;
				if (!found) {
					first_chi_val = chi_val;
					found = 1;
				} else if (chi_val != first_chi_val) {
					return -EINVAL;
				}
			}
		}
		if (found) {
			*out_val = first_chi_val;
			return 0;
		}
		return -ENODEV;
	case PRIO_PROP_PARTIAL:
		ret = read_node_layer(node, 'd', is_w, &val);
		if (ret)
			return ret;
		if (node->nr_children > 0) {
			cur_nb = is_w ? node->dev->reg.cur_nbits_w
					 : node->dev->reg.cur_nbits_r;
			cur_lsb = is_w ? node->dev->reg.cur_lsb_w
				       : node->dev->reg.cur_lsb_r;
			chi_nb = is_w ? node->dev->reg.child_nbits_w
					   : node->dev->reg.child_nbits_r;
			chi_lsb = is_w ? node->dev->reg.child_lsb_w
					 : node->dev->reg.child_lsb_r;
			found = 0;
			for (i = 0; i < node->nr_children; i++) {
				if (node->children[i] && (node->children[i]->rw == is_w ||
								node->children[i]->rw == -1)) {
					ret = get_final_prio_val(node->children[i],
									is_w,
									dmc_idx, &chi_val);
					if (ret)
						return ret;
					if (!found) {
						first_chi_val = chi_val;
						found = 1;
					} else if (chi_val != first_chi_val) {
						return -EINVAL;
					}
				}
			}
			if (found) {
				*out_val = ((val & ((1U << cur_nb) - 1)) << cur_lsb) |
					   ((first_chi_val & ((1U << chi_nb) - 1)) << chi_lsb);
				return 0;
			}
		}
		*out_val = val;
		return 0;
	default:
		return -EINVAL;
	}
}

int get_final_prio_val(struct ddr_priority_node *node,
				   int is_w, int dmc_idx, unsigned int *out_val)
{
	struct ddr_priority_force *f;
	void __iomem *base;
	unsigned int force_val;
	int ret;
	unsigned int val, child_nbits, cur_nbits, child_lsb, cur_lsb, chi_val;

	if (!node)
		return -EINVAL;

	if (node->dmc_force && dmc_idx >= 0) {
		f = node->dmc_force[dmc_idx];
		if (f) {
			base = (aml_db && dmc_idx < aml_db->dmc_number)
			       ? aml_db->ddr_reg[dmc_idx] : NULL;
			if (!base)
				return -ENODEV;
			if (is_w) {
				force_val = readl_relaxed(base + f->w_offset);
				val = (force_val >> f->w_lsb)
				      & ((1U << f->w_nbits) - 1);
			} else {
				force_val = readl_relaxed(base + f->r_offset);
				val = (force_val >> f->r_lsb)
				      & ((1U << f->r_nbits) - 1);
			}
			if (is_force_enabled(f, is_w, base, val)) {
				*out_val = val;
				return 0;
			}
		}
	}

	if (!node->nic && !node->dev)
		return -ENODEV;

	if (node->nic) {
		switch (node->nic->prop_mode) {
		case PRIO_PROP_NONE:
			return read_node_layer(node, 'n', is_w, out_val);
		case PRIO_PROP_FIXED:
		case PRIO_PROP_FULL:
			return get_dev_layer_val(node, is_w, dmc_idx, out_val);
		case PRIO_PROP_PARTIAL:
			ret = read_node_layer(node, 'n', is_w, &val);
			if (ret)
				return ret;
			cur_nbits = is_w ? node->nic->reg.cur_nbits_w
					 : node->nic->reg.cur_nbits_r;
			cur_lsb = is_w ? node->nic->reg.cur_lsb_w
				       : node->nic->reg.cur_lsb_r;
			child_nbits = is_w ? node->nic->reg.child_nbits_w
					   : node->nic->reg.child_nbits_r;
			child_lsb = is_w ? node->nic->reg.child_lsb_w
					 : node->nic->reg.child_lsb_r;
			ret = get_dev_layer_val(node, is_w, dmc_idx, &chi_val);
			if (ret)
				return ret;
			*out_val = ((val & ((1U << cur_nbits) - 1)) << cur_lsb)
				   | ((chi_val & ((1U << child_nbits) - 1)) << child_lsb);
			return 0;
		default:
			return -EINVAL;
		}
	}

	return get_dev_layer_val(node, is_w, dmc_idx, out_val);
}

static int build_id_path(struct ddr_priority_node *node, char *buf, int bufsz)
{
	struct ddr_priority_node *path[16];
	int depth = 0;
	int off = 0;
	int j;
	struct ddr_priority_node *p = node;

	while (p) {
		path[depth++] = p;
		p = p->parent;
	}

	for (j = depth - 1; j >= 0; j--) {
		if (j < depth - 1)
			buf[off++] = ':';
		if (path[j]->sub_id >= 0)
			off += scnprintf(buf + off, bufsz - off,
					 "%x", path[j]->sub_id);
		else
			off += scnprintf(buf + off, bufsz - off,
					 "%x", path[j]->port_id);
	}
	buf[off] = '\0';
	return off;
}

static void show_prio_summary(char *buf, ssize_t *poff, size_t bufsz,
			   struct ddr_priority_tree *tree, int dmc,
			   const struct prio_disp_cfg *cfg)
{
	unsigned int r_val, w_val;
	int ret_r, ret_w;
	int i;
	struct ddr_priority_node *node;
	char id_str[64];
	char r_str[4], w_str[4];
	char tmp_str[72];
	const char *suffix;

	*poff += scnprintf(buf + *poff, bufsz - *poff,
			   "version: 1\n");
	*poff += scnprintf(buf + *poff, bufsz - *poff,
			   "Usage:\n");
	*poff += scnprintf(buf + *poff, bufsz - *poff,
			   "\tdmc:\techo dmc <bus> <val> [r|w] > priority\n");
	*poff += scnprintf(buf + *poff, bufsz - *poff,
			   "\tnic:\techo nic <0xid> <val> [r|w] > priority\n");
	*poff += scnprintf(buf + *poff, bufsz - *poff,
			   "\tdev:\techo dev <0xid>[:<sub>[:<mod>]] <val> [r|w] > priority\n");
	*poff += scnprintf(buf + *poff, bufsz - *poff,
			   "\tdev:\techo <name> <val> [r|w] > priority\n");
	*poff += scnprintf(buf + *poff, bufsz - *poff, SPLIT_LINE);

	if (cfg->force_only)
		return;

	*poff += scnprintf(buf + *poff, bufsz - *poff,
			   "%-2s %-2s    %-20s%s\n", "r", "w", "id_path", "name");
	*poff += scnprintf(buf + *poff, bufsz - *poff, SPLIT_LINE);

	for (i = 0; i < tree->nr_nodes; i++) {
		node = &tree->nodes[i];

		ret_r = node->rw == 1 ? -1 : get_final_prio_val(node, 0, dmc, &r_val);
		ret_w = node->rw == 0 ? -1 : get_final_prio_val(node, 1, dmc, &w_val);
		if (ret_r && ret_w)
			continue;

		build_id_path(node, id_str, sizeof(id_str));

		if (ret_r)
			r_str[0] = '-', r_str[1] = '\0';
		else
			snprintf(r_str, sizeof(r_str), "%x", r_val);

		if (ret_w)
			w_str[0] = '-', w_str[1] = '\0';
		else
			snprintf(w_str, sizeof(w_str), "%x", w_val);

		if (node->rw == 0)
			suffix = ":r";
		else if (node->rw == 1)
			suffix = ":w";
		else
			suffix = "";

		if (*suffix) {
			snprintf(tmp_str, sizeof(tmp_str), "%s%s", id_str, suffix);
			*poff += scnprintf(buf + *poff, bufsz - *poff,
					   "%-2s %-2s    %-20s%s\n",
					   r_str, w_str,
					   tmp_str,
					   node->name ?: "");
		} else {
			*poff += scnprintf(buf + *poff, bufsz - *poff,
					   "%-2s %-2s    %-20s%s\n",
					   r_str, w_str,
					   id_str,
					   node->name ?: "");
		}
	}

	*poff += scnprintf(buf + *poff, bufsz - *poff, SPLIT_LINE);
}

static struct ddr_priority_node *find_child_on_path(struct ddr_priority_node *callback,
						    struct ddr_priority_node *node, int is_w)
{
	int i;
	struct ddr_priority_node *anc;
	struct ddr_priority_node *child;

	for (i = 0; i < callback->nr_children; i++) {
		child = callback->children[i];

		if (child->rw != -1 && child->rw != is_w)
			continue;
		if (child == node)
			return child;
		for (anc = node->parent; anc; anc = anc->parent) {
			if (anc == child)
				return child;
		}
	}
	return NULL;
}

static int get_callback_prio_val(struct ddr_priority_node *node,
				 struct ddr_priority_node *callback,
				 int is_w,
				 struct ddr_priority_node **path,
				 int path_count)
{
	int i, ret, chi_ret;
	struct ddr_priority_node *child;
	unsigned int val, chi_val;
	unsigned char cur_nbits, child_nbits, cur_lsb, child_lsb;

	for (i = 0; i < path_count; i++) {
		if (path[i] == callback)
			return -ELOOP;
	}

	switch (callback->prop_mode) {
	case PRIO_PROP_NONE:
		if (!callback->dev)
			return -ENODEV;
		ret = read_node_layer(callback, 'd', is_w, &val);
		return ret ? ret : (int)val;
	case PRIO_PROP_FULL:
		path[path_count] = callback;
		child = find_child_on_path(callback, node, is_w);
		if (!child)
			return -ENODEV;
		return get_callback_prio_val(node, child, is_w, path, path_count + 1);
	case PRIO_PROP_PARTIAL:
		if (!callback->dev)
			return -ENODEV;
		ret = read_node_layer(callback, 'd', is_w, &val);
		if (ret)
			return ret;
		cur_nbits = is_w ? callback->dev->reg.cur_nbits_w : callback->dev->reg.cur_nbits_r;
		cur_lsb = is_w ? callback->dev->reg.cur_lsb_w : callback->dev->reg.cur_lsb_r;
		child_nbits = is_w ? callback->dev->reg.child_nbits_w
				  : callback->dev->reg.child_nbits_r;
		child_lsb = is_w ? callback->dev->reg.child_lsb_w : callback->dev->reg.child_lsb_r;
		path[path_count] = callback;
		child = find_child_on_path(callback, node, is_w);
		if (child) {
			chi_ret = get_callback_prio_val(node, child, is_w, path, path_count + 1);
			if (chi_ret < 0)
				return chi_ret;
			chi_val = (unsigned int)chi_ret;
		} else {
			return -ENODEV;
		}
		return (int)(((val & ((1U << cur_nbits) - 1)) << cur_lsb)
		     | ((chi_val & ((1U << child_nbits) - 1)) << child_lsb));
	default:
		return -EINVAL;
	}
}

static int get_callback_prio_string(char *buf, size_t bufsz,
				      struct ddr_priority_node *node,
				      struct ddr_priority_node *callback,
				      int is_w,
				      struct ddr_priority_node **path,
				      int path_count)
{
	int i, ret, chi_ret;
	struct ddr_priority_node *child;
	unsigned int val, chi_val;
	unsigned char cur_nbits, child_nbits, cur_lsb, child_lsb, nbits;

	for (i = 0; i < path_count; i++) {
		if (path[i] == callback)
			return scnprintf(buf, bufsz, "-:-");
	}

	switch (callback->prop_mode) {
	case PRIO_PROP_NONE:
		if (!callback->dev)
			return scnprintf(buf, bufsz, "-:-");
		return format_layer_value(buf, bufsz, callback, 'd', is_w);
	case PRIO_PROP_FULL:
		path[path_count] = callback;
		child = find_child_on_path(callback, node, is_w);
		if (!child)
			return scnprintf(buf, bufsz, "-:-");
		return get_callback_prio_string(buf, bufsz, node, child,
						  is_w, path,
						  path_count + 1);
	case PRIO_PROP_PARTIAL:
		if (!callback->dev)
			return scnprintf(buf, bufsz, "-:-");
		ret = read_node_layer(callback, 'd', is_w, &val);
		if (ret)
			return scnprintf(buf, bufsz, "-:-");
		cur_nbits = is_w ? callback->dev->reg.cur_nbits_w : callback->dev->reg.cur_nbits_r;
		cur_lsb = is_w ? callback->dev->reg.cur_lsb_w : callback->dev->reg.cur_lsb_r;
		child_nbits = is_w ? callback->dev->reg.child_nbits_w
				  : callback->dev->reg.child_nbits_r;
		child_lsb = is_w ? callback->dev->reg.child_lsb_w : callback->dev->reg.child_lsb_r;
		nbits = cur_lsb + cur_nbits;
		if (child_lsb + child_nbits > nbits)
			nbits = child_lsb + child_nbits;
		path[path_count] = callback;
		child = find_child_on_path(callback, node, is_w);
		if (child) {
			chi_ret = get_callback_prio_val(node, child, is_w, path, path_count + 1);
			if (chi_ret < 0)
				return scnprintf(buf, bufsz, "*:%x",
						 (1U << nbits) - 1);
			chi_val = (unsigned int)chi_ret;
		} else {
			return scnprintf(buf, bufsz, "*:%x",
					 (1U << nbits) - 1);
		}
		val = ((val & ((1U << cur_nbits) - 1)) << cur_lsb)
		    | ((chi_val & ((1U << child_nbits) - 1)) << child_lsb);
		return scnprintf(buf, bufsz, "%x:%x", val, (1U << nbits) - 1);
	default:
		return scnprintf(buf, bufsz, "-:-");
	}
}

static void prio_disp_cfg_init(struct prio_disp_cfg *cfg,
				const struct ddr_priority_tree *tree)
{
	int i;

	cfg->has_nic = false;
	cfg->has_dev = false;
	for (i = 0; i < tree->nr_nodes; i++) {
		if (tree->nodes[i].nic)
			cfg->has_nic = true;
		if (tree->nodes[i].dev)
			cfg->has_dev = true;
	}
	cfg->force_only = !cfg->has_nic && !cfg->has_dev;
}

static void show_prio_tree(char *buf, ssize_t *poff, size_t bufsz,
			     struct ddr_priority_node *node,
			     const char *prefix, bool is_last, int dmc,
			     const struct prio_disp_cfg *cfg,
			     bool bus_header)
{
	void __iomem *dmc_base;
	char tmp[32];
	char r[16], w[16];
	char new_prefix[256];
	int i, d, len, pad, dmc_num, id_len, anc_count, name_w;
	bool last;
	struct ddr_priority_node *anc;
	struct ddr_priority_node *anc_list[16];
	struct ddr_priority_node *path[16];

	if (node->sub_id < 0) {
		dmc_num = aml_db ? aml_db->dmc_number : 1;

		if (bus_header) {
			/* Level 0: BUS + FORCE + DEV */
			if (cfg->has_dev) {
				if (node->bus >= 0)
					id_len = scnprintf(buf + *poff, bufsz - *poff,
							   "<%02u>",
							   (unsigned int)node->bus);
				else
					id_len = scnprintf(buf + *poff, bufsz - *poff,
							   "<%02u>",
							   (unsigned int)node->port_id);
			} else {
				name_w = BUS_COL_WIDTH - 5;
				if (node->bus >= 0)
					id_len = scnprintf(buf + *poff, bufsz - *poff,
							   "<%02u> %-*s",
							   (unsigned int)node->bus,
							   name_w,
							   node->name ?: "");
				else
					id_len = scnprintf(buf + *poff, bufsz - *poff,
							   "<%02u> %-*s",
							   (unsigned int)node->port_id,
							   name_w,
							   node->name ?: "");
			}
			*poff += id_len;
			pad = BUS_COL_WIDTH - id_len;
			if (pad > 0) {
				memset(buf + *poff, ' ', pad);
				*poff += pad;
			}
			for (d = 0; d < dmc_num; d++) {
				dmc_base = (aml_db && d < aml_db->dmc_number)
					   ? aml_db->ddr_reg[d] : NULL;
				if (node->dmc_force && node->dmc_force[d]) {
					len = format_force_col(tmp, sizeof(tmp),
							       node->dmc_force[d],
							       dmc_base);
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%-24s", len ? tmp : "");
				} else {
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%-24s", "");
				}
			}
			if (cfg->has_nic) {
				if (node->nic) {
					len = format_col(tmp, sizeof(tmp), node, 'n');
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%-24s", len ? tmp : "");
				} else {
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%-24s", "");
				}
			}
			if (cfg->has_dev) {
				if (node->dev) {
					len = format_col(tmp, sizeof(tmp), node, 'd');
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%s [%02x] %s",
							   len ? tmp : "-:- -:-",
							   node->port_id,
							   node->name ?: "");
				} else {
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "-:- -:- [%02x] %s",
							   node->port_id,
							   node->name ?: "");
				}
			}
		} else {
			/* Level 1 bus group member: skip BUS + FORCE, show NIC/DEV + name */
			*poff += scnprintf(buf + *poff, bufsz - *poff, "%-*s", BUS_COL_WIDTH, "");
			for (d = 0; d < dmc_num; d++)
				*poff += scnprintf(buf + *poff, bufsz - *poff, "%-24s", "");

			if (cfg->has_nic) {
				if (node->nic) {
					len = format_col(tmp, sizeof(tmp), node, 'n');
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%-24s", len ? tmp : "");
				} else {
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%-24s", "");
				}
			}

			if (cfg->has_dev) {
				if (node->dev) {
					len = format_col(tmp, sizeof(tmp), node, 'd');
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "%s [%02x] %s",
							   len ? tmp : "-:- -:-",
							   node->port_id,
							   node->name ?: "");
				} else {
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   "-:- -:- [%02x] %s",
							   node->port_id,
							   node->name ?: "");
				}
			}
		}
		*poff += scnprintf(buf + *poff, bufsz - *poff, "\n");
	} else if (cfg->force_only) {
		if (node->name)
			*poff += scnprintf(buf + *poff, bufsz - *poff,
					   "       %s", node->name);
		*poff += scnprintf(buf + *poff, bufsz - *poff, "\n");
	} else {
		anc_count = 0;

		dmc_num = aml_db ? aml_db->dmc_number : 1;
		if (cfg->has_nic && node->nic)
			pad = BUS_COL_WIDTH + dmc_num * PRIO_COL_WIDTH;
		else if (cfg->has_nic)
			pad = BUS_COL_WIDTH + (1 + dmc_num) * PRIO_COL_WIDTH;
		else
			pad = BUS_COL_WIDTH + dmc_num * PRIO_COL_WIDTH;
		scnprintf(buf + *poff, bufsz - *poff, "%*s", pad, "");
		*poff += pad;

		if (cfg->has_nic && node->nic) {
			len = format_col(tmp, sizeof(tmp), node, 'n');
			if (node->parent &&
			    node->parent->prop_mode == PRIO_PROP_NONE &&
			    !node->dev)
				*poff += scnprintf(buf + *poff, bufsz - *poff,
						   "%s", len ? tmp : "");
			else
				*poff += scnprintf(buf + *poff, bufsz - *poff,
						   "%-24s", len ? tmp : "");
		}

		anc = node->parent;
		while (anc && anc_count < 16) {
			if (anc->prop_mode != PRIO_PROP_NONE)
				anc_list[anc_count++] = anc;
			anc = anc->parent;
		}

		if (cfg->has_dev) {
			for (i = anc_count - 1; i >= 0; i--) {
				if (anc_list[i]->dev) {
					r[0] = '\0';
					w[0] = '\0';
					get_callback_prio_string(r, sizeof(r),
								node, anc_list[i],
								0, path, 0);
					get_callback_prio_string(w, sizeof(w),
								node, anc_list[i],
								1, path, 0);
					if (node->rw == 0)
						scnprintf(tmp, sizeof(tmp), "%s -:-", r);
					else if (node->rw == 1)
						scnprintf(tmp, sizeof(tmp), "-:- %s", w);
					else
						scnprintf(tmp, sizeof(tmp), "%s %s", r, w);
					*poff += scnprintf(buf + *poff, bufsz - *poff,
								"%-16s", tmp);
				} else {
					*poff += scnprintf(buf + *poff, bufsz - *poff,
								"%-16s", "-:- -:-");
				}
			}
		}

		if (cfg->has_dev && node->dev) {
			if (node->rw >= 0) {
				format_layer_value(tmp, sizeof(tmp), node, 'd', node->rw);
				*poff += scnprintf(buf + *poff, bufsz - *poff, "%s", tmp);
			} else {
				len = format_col(tmp, sizeof(tmp), node, 'd');
				*poff += scnprintf(buf + *poff, bufsz - *poff, "%s",
									len ? tmp : "");
			}
		} else if (cfg->has_dev &&
			   (!node->parent ||
			    node->parent->prop_mode != PRIO_PROP_NONE)) {
			if (node->rw >= 0)
				*poff += scnprintf(buf + *poff, bufsz - *poff, "-:-");
			else
				*poff += scnprintf(buf + *poff, bufsz - *poff, "-:- -:-");
		}

		if (node->name) {
			if (node->sub_id >= 0) {
				if (node->rw == 0)
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   " [%02x] R:%s",
							   node->sub_id, node->name);
				else if (node->rw == 1)
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   " [%02x] W:%s",
							   node->sub_id, node->name);
				else
					*poff += scnprintf(buf + *poff, bufsz - *poff,
							   " [%02x] %s",
							   node->sub_id, node->name);
			} else {
				*poff += scnprintf(buf + *poff, bufsz - *poff,
						   " %s", node->name);
			}
		}

		*poff += scnprintf(buf + *poff, bufsz - *poff, "\n");
	}

	for (i = 0; i < node->nr_children; i++) {
		last = (i == node->nr_children - 1);

		if (node->prop_mode == PRIO_PROP_NONE && !node->children[i]->nic)
			continue;

		if (node->sub_id < 0) {
			scnprintf(new_prefix, sizeof(new_prefix), "%s",
				  last ? "    " : "|   ");
		} else {
			scnprintf(new_prefix, sizeof(new_prefix), "%s%s",
				  prefix,
				  last ? "    " : "|   ");
		}

		show_prio_tree(buf, poff, bufsz, node->children[i],
				 new_prefix, last, dmc, cfg, false);
	}
}

static ssize_t priority_tree_show(char *buf)
{
	struct ddr_priority_tree *tree;
	struct prio_disp_cfg cfg;
	ssize_t off = 0;
	int i, d, dmc_num;
	bool is_header;

	if (!aml_db || !aml_db->priority || !aml_db->priority->tree)
		return -EINVAL;

	tree = aml_db->priority->tree;
	if (!tree->roots || tree->nr_roots == 0)
		return -EINVAL;

	prio_disp_cfg_init(&cfg, tree);
	dmc_num = aml_db->dmc_number;

	show_prio_summary(buf, &off, PRIO_V1_BUF_SZ, tree, 0, &cfg);

	off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "%-*s", BUS_COL_WIDTH, "BUS");
	for (d = 0; d < dmc_num; d++)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "DMC%-21d", d);
	if (cfg.has_nic)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "%-24s", "NIC");
	if (cfg.has_dev)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "%-24s", "DEV");
	off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "\n");

	off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "%-*s", BUS_COL_WIDTH, "");
	for (d = 0; d < dmc_num; d++)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "%-24s", "r:m:f w:m:f");
	if (cfg.has_nic)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "%-24s", "r:m w:m");
	if (cfg.has_dev)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "%-24s", "r:m w:m");
	off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "\n");

	off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off,
			 "%-*s", BUS_COL_WIDTH, "-----------------------");
	for (d = 0; d < dmc_num; d++)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off,
				 "%-24s", "-----------------------");
	if (cfg.has_nic)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off,
				 "%-24s", "-----------------------");
	if (cfg.has_dev)
		off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off,
				 "%-24s", "-----------------------");
	off += scnprintf(buf + off, PRIO_V1_BUF_SZ - off, "\n");

	for (i = 0; i < tree->nr_roots; i++) {
		is_header = (i == 0) || (tree->roots[i]->bus != tree->roots[i - 1]->bus);
		show_prio_tree(buf, &off, PRIO_V1_BUF_SZ,
				 tree->roots[i], "", true, 0, &cfg, is_header);
	}

	return off;
}

ssize_t priority_display(char *buf, loff_t pos, size_t count)
{
	ssize_t info_len;
	loff_t ppos = pos;

	if (!aml_db || !aml_db->priority)
		return -EINVAL;

	if (aml_db->priority->ver == 2)
		return priority_v2_display(buf, pos, count);

	mutex_lock(&prio_v1_buf_mutex);
	if (!ppos) {
		ddr_priority.v1_buf = NULL;
		ddr_priority.v1_buf_size = 0;

		ddr_priority.v1_buf = kvzalloc(PRIO_V1_BUF_SZ, GFP_KERNEL);
		if (!ddr_priority.v1_buf) {
			mutex_unlock(&prio_v1_buf_mutex);
			return -ENOMEM;
		}

		info_len = priority_tree_show(ddr_priority.v1_buf);

		if (info_len < 0) {
			kvfree(ddr_priority.v1_buf);
			ddr_priority.v1_buf = NULL;
			mutex_unlock(&prio_v1_buf_mutex);
			return info_len;
		}
		ddr_priority.v1_buf_size = info_len;
	} else if (!ddr_priority.v1_buf) {
		if (ddr_priority.v1_buf_size > 0 &&
		    pos >= (loff_t)ddr_priority.v1_buf_size) {
			mutex_unlock(&prio_v1_buf_mutex);
			return 0;
		}
		mutex_unlock(&prio_v1_buf_mutex);
		return -EIO;
	}

	info_len = memory_read_from_buffer(buf, count, &ppos, ddr_priority.v1_buf,
					   ddr_priority.v1_buf_size);
	if (info_len > 0 && ppos >= ddr_priority.v1_buf_size) {
		kvfree(ddr_priority.v1_buf);
		ddr_priority.v1_buf = NULL;
	}
	mutex_unlock(&prio_v1_buf_mutex);
	return info_len;
}

static int get_dmc_priority_v1(unsigned int bus_id, char rw)
{
	struct ddr_priority_force *f;
	void __iomem *base;
	u32 val[MAX_DMC_NUM] = {0};
	unsigned char lsb, nbits;
	int i;

	if (!aml_db || !aml_db->priority || !aml_db->priority->v1)
		return -EINVAL;

	f = find_force_by_bus(aml_db->priority->v1, bus_id);
	if (!f)
		return -ENODEV;

	if (rw != 'r' && rw != 'w')
		return -EINVAL;

	for (i = 0; i < aml_db->dmc_number; i++) {
		base = aml_db->ddr_reg[i];
		if (!base)
			continue;

		if (rw == 'r') {
			lsb = f->r_lsb;
			nbits = f->r_nbits;
			val[i] = readl_relaxed(base + f->r_offset);
		} else {
			lsb = f->w_lsb;
			nbits = f->w_nbits;
			val[i] = readl_relaxed(base + f->w_offset);
		}

		val[i] = (val[i] >> lsb) & ((1U << nbits) - 1);
		if (i && val[i] != val[i - 1])
			pr_info("dmc%d:%d not same dmc%d:%d\n", i, val[i], i - 1, val[i - 1]);
	}

	return val[0];
}

static int set_dmc_priority_v1(unsigned int bus_id, unsigned char priority, char rw)
{
	int dmc_rw;

	if (rw == 'r')
		dmc_rw = 0;
	else if (rw == 'w')
		dmc_rw = 1;
	else if (rw == 0)
		dmc_rw = -1;
	else
		return -EINVAL;

	return write_force_by_bus(bus_id, (char)priority, dmc_rw, -1);
}

int get_dmc_priority(unsigned int bus_id, char rw)
{
	unsigned int value;
	int i;
	struct ddr_priority_v2 *v2;
	struct dmc_priority *dmc;

	v2 = aml_db->priority->v2;
	if (!v2) {
		return get_dmc_priority_v1(bus_id, rw);
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
		return set_dmc_priority_v1(bus_id, priority, rw);
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

struct ddr_priority_node *__get_dev_tree_prio_by_path(char rw, unsigned int port_id, ...)
{
	struct ddr_priority_node *node;
	int sub_path[4];
	int depth = 0;
	va_list ap;
	int id, rw_int;

	if (!aml_db || !aml_db->priority || !aml_db->priority->tree)
		return ERR_PTR(-EINVAL);

	if (rw != 0 && rw != 'r' && rw != 'w')
		return ERR_PTR(-EINVAL);

	va_start(ap, port_id);
	while ((id = va_arg(ap, int)) != -1 && depth < 4)
		sub_path[depth++] = id;
	va_end(ap);

	if (depth == 0) {
		node = find_root_by_port_id(aml_db->priority->tree, port_id);
	} else {
		rw_int = (rw == 'w') ? 1 : (rw == 'r') ? 0 : -1;
		node = find_node_by_dev_path(aml_db->priority->tree,
					     port_id, sub_path, depth, rw_int);
	}

	if (IS_ERR(node))
		return node;
	if (!node || !node->dev)
		return ERR_PTR(-ENODEV);
	if (node->prop_mode != PRIO_PROP_NONE &&
	    node->prop_mode != PRIO_PROP_PARTIAL)
		return ERR_PTR(-EOPNOTSUPP);

	return node;
}

static int get_dev_priority_v1(unsigned int port_id, char rw)
{
	struct ddr_priority_node *node;
	unsigned int val;
	int ret;

	if (rw != 'r' && rw != 'w')
		return -EINVAL;

	node = get_dev_tree_prio_by_path(rw, port_id);
	if (IS_ERR(node))
		return PTR_ERR(node);

	ret = read_node_layer(node, 'd', rw == 'w', &val);
	if (ret)
		return ret;

	return val;
}

static int set_dev_priority_v1(unsigned int port_id, unsigned char priority, char rw)
{
	struct ddr_priority_node *node;

	node = get_dev_tree_prio_by_path(rw, port_id);
	if (IS_ERR(node))
		return PTR_ERR(node);

	return write_node_layer(node, 'd', (char)priority, rw, -1, NULL);
}

int get_dev_priority(unsigned int port_id, char rw)
{
	unsigned int value;
	int i;
	struct ddr_priority_v2 *v2;
	struct bus_dev_priority *bus_dev;

	v2 = aml_db->priority->v2;
	if (!v2) {
		return get_dev_priority_v1(port_id, rw);
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
		return set_dev_priority_v1(port_id, priority, rw);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_vpu_super_priority)
			return aml_db->prio_ops->set_vpu_super_priority(axi, rw, priority);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_vpu_top_priority)
			return aml_db->prio_ops->set_vpu_top_priority(axi, rw, top_i, priority);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_vpu_sub_dev_priority)
			return aml_db->prio_ops->set_vpu_sub_dev_priority(axi, rw, id, priority);
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
	unsigned int value = 0, value1 = 0;
	unsigned int dvb_t2_mode = 0;
	struct ddr_priority_v2 *v2;
	struct demod_priority *dm;

	v2 = aml_db->priority->v2;
	if (!v2) {
		if (aml_db->prio_ops && aml_db->prio_ops->set_demod_priority)
			return aml_db->prio_ops->set_demod_priority(priority);
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
	reg_access(dm->reg_base + dm->mode_reg2, 0, READ, &value1);
	if (value & BIT(dm->dtmb_mode_bit)) {
		reg_access(dm->reg_base + dm->dtmb_reg, 0, READ, &value);
		value &= ~BIT(dm->dtmb_bit);
		if (priority)
			value |= BIT(dm->dtmb_bit);
		reg_access(dm->reg_base + dm->dtmb_reg, 0, WRITE, &value);
		pr_err("set DTMB priority(%d)(reg 0x%x)\n", priority, value);
		return 0;
	} else if (value & BIT(dm->isdbt_mode_bit)) {
		reg_access(dm->reg_base + dm->isdbt_reg, 0, READ, &value);
		value &= ~BIT(dm->isdbt_bit);
		if (priority)
			value |= BIT(dm->isdbt_bit);
		reg_access(dm->reg_base + dm->isdbt_reg, 0, WRITE, &value);
		pr_err("set ISDBT priority(%d)(reg 0x%x)\n", priority, value);
		return 0;
	} else if (value1 & BIT(dm->dvb_t2_mode_bit)) {
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, READ, &dvb_t2_mode);
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, WRITE, &dm->dvb_t2_select_mode);
		reg_access_byte(dm->reg_base + dm->dvb_t2_reg, 0, READ, &value1);
		value1 &= ~BIT(dm->dvb_t2_bit);
		if (priority)
			value1 |= BIT(dm->dvb_t2_bit);
		reg_access_byte(dm->reg_base + dm->dvb_t2_reg, 0, WRITE, &value1);
		reg_access(dm->reg_base + dm->dvb_t2_select_reg, 0, WRITE, &dvb_t2_mode);
		pr_err("set DVB-T2 priority(%d)(reg 0x%x)\n", priority, value1);
		return 0;
	} else {
#define DEMOD_CANT_SET_PRIORITY_MSG \
	"the demod can only set its corresponding priority when operating in ISDB-T, DVB-T2, or DTMB mode"
		pr_err("%s\n", DEMOD_CANT_SET_PRIORITY_MSG);
	}

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
		if (aml_db->prio_ops && aml_db->prio_ops->set_demux_priority)
			return aml_db->prio_ops->set_demux_priority(priority, rw);
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

int set_audio_x_select_priority(char x, char ugt)
{
	unsigned int value;
	struct ddr_priority_v2 *v2;
	struct audio_priority *audio;
	int i;

	v2 = aml_db->priority->v2;
	if (!v2) {
		if (aml_db->prio_ops && aml_db->prio_ops->set_audio_x_select_priority)
			return aml_db->prio_ops->set_audio_x_select_priority(x, ugt);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_audio_priority)
			return aml_db->prio_ops->set_audio_priority(idx, priority, rw);
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
		if (aml_db->prio_ops && aml_db->prio_ops->show_audio_priority_setting)
			return aml_db->prio_ops->show_audio_priority_setting();
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_bcon_hw_urgent)
			return aml_db->prio_ops->set_bcon_hw_urgent(low, high);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_bcon_sw_urgent)
			return aml_db->prio_ops->set_bcon_sw_urgent(ugt);
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
		if (aml_db->prio_ops && aml_db->prio_ops->show_bcon_priority_setting)
			return aml_db->prio_ops->show_bcon_priority_setting();
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_hevc_level_id)
			return aml_db->prio_ops->set_hevc_level_id(rw, level, i, id);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_hcodec_level_id)
			return aml_db->prio_ops->set_hcodec_level_id(rw, level, i, id);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_hevc_low_priority)
			return aml_db->prio_ops->set_hevc_low_priority(rw, priority);
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_hcodec_low_priority)
			return aml_db->prio_ops->set_hcodec_low_priority(rw, priority);
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
		if (aml_db->prio_ops && aml_db->prio_ops->show_hevc_priority_setting)
			return aml_db->prio_ops->show_hevc_priority_setting();
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
		if (aml_db->prio_ops && aml_db->prio_ops->show_hcodec_priority_setting)
			return aml_db->prio_ops->show_hcodec_priority_setting();
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
		if (aml_db->prio_ops && aml_db->prio_ops->set_device_accessible)
			return aml_db->prio_ops->set_device_accessible(dev, access);
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
