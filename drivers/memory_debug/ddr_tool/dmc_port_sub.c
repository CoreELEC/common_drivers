// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/slab.h>
#include "ddr_port.h"
#include "dmc_monitor.h"
#include "ddr_bandwidth.h"

#define SUB_DEV_RW(soc, name, port_id_r, port_id_w)				\
	static struct sub_dev name##_sub_##soc __initdata = {			\
		.sub_r = {							\
			.port_id = port_id_r,					\
			.num = ARRAY_SIZE(name##_sub_r_##soc),			\
			.list = name##_sub_r_##soc,				\
		},								\
		.sub_w = {							\
			.port_id = port_id_w,					\
			.num = ARRAY_SIZE(name##_sub_w_##soc),			\
			.list = name##_sub_w_##soc,				\
		},								\
	}

#define SUB_DEV_RO(soc, name, port_id_r)					\
	static struct sub_dev name##_sub_##soc __initdata = {			\
		.sub_r = {							\
			.port_id = port_id_r,					\
			.num = ARRAY_SIZE(name##_sub_r_##soc),			\
			.list = name##_sub_r_##soc,				\
		},								\
		.sub_w = {							\
			.port_id = -1,						\
			.num = 0,						\
			.list = NULL,						\
		},								\
	}

#define SUB_DEV_WO(soc, name, port_id_w)					\
	static struct sub_dev name##_sub_##soc __initdata = {			\
		.sub_r = {							\
			.port_id = -1,						\
			.num = 0,						\
			.list = NULL,						\
		},								\
		.sub_w = {							\
			.port_id = port_id_w,					\
			.num = ARRAY_SIZE(name##_sub_w_##soc),			\
			.list = name##_sub_w_##soc,				\
		},								\
	}

#define SUB_DEV_DESC_INIT(soc, ...)						\
	static struct sub_dev_desc sub_dev_desc_##soc __initdata = {		\
		.num = sizeof((struct sub_dev *[]){ __VA_ARGS__ })		\
		      / sizeof(struct sub_dev *),				\
		.subs = { __VA_ARGS__ },					\
	}									\


#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct sub_port_list vpu0_sub_r_txlx[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "AFBC_DEC"			},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "TVFE"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xD, .name = "TCON_P3"				},
	{ .id = 0xE, .name = "AFBC_ENC"			},
};

static struct sub_port_list vpu0_sub_w_txlx[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "AFBC_ENC"				},
	{ .id = 0x4, .name = "TVFE"				},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "TCON3"				},
};

static struct sub_port_list vpu1_sub_r_txlx[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct sub_port_list vpu1_sub_w_txlx[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_txlx		vpu0_sub_r_txlx
SUB_DEV_RW(txlx, vpu0, 8, 11);
SUB_DEV_RW(txlx, vpu1, 9, 12);
SUB_DEV_RO(txlx, vpu2, 10);
SUB_DEV_DESC_INIT(txlx, &vpu0_sub_txlx, &vpu1_sub_txlx, &vpu2_sub_txlx);

//static struct sub_dev_desc sub_dev_desc_txl[] __initdata = sub_dev_desc_txlx[]
//static struct sub_dev_desc sub_dev_desc_txhd[] __initdata = sub_dev_desc_txlx[]
static struct sub_port_list vpu0_sub_r_tl1[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "AFBC_DEC"			},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "TVFE"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xD, .name = "TCON_P3"				},
	{ .id = 0xE, .name = "AFBC_ENC"			},
};

static struct sub_port_list vpu0_sub_w_tl1[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "AFBC_ENC"				},
	{ .id = 0x4, .name = "TVFE"				},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "TCON3"				},
};

static struct sub_port_list vpu1_sub_r_tl1[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_tl1[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_tl1		vpu0_sub_r_tl1
SUB_DEV_RW(tl1, vpu0, 16, 19);
SUB_DEV_RW(tl1, vpu1, 17, 20);
SUB_DEV_RO(tl1, vpu2, 18);
SUB_DEV_DESC_INIT(tl1, &vpu0_sub_tl1, &vpu1_sub_tl1, &vpu2_sub_tl1);
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct sub_port_list vpu0_sub_r_sm1[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0xA, .name = "RDMA"				},
};

static struct sub_port_list vpu0_sub_w_sm1[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x6, .name = "RDMA"				},
};

static struct sub_port_list vpu1_sub_r_sm1[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct sub_port_list vpu1_sub_w_sm1[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_sm1		vpu0_sub_r_sm1
SUB_DEV_RW(sm1, vpu0, 16, 19);
SUB_DEV_RW(sm1, vpu1, 17, 20);
SUB_DEV_RO(sm1, vpu2, 18);
SUB_DEV_DESC_INIT(sm1, &vpu0_sub_sm1, &vpu1_sub_sm1, &vpu2_sub_sm1);

// static struct sub_dev_desc sub_dev_desc_g12b[] __initdata = sub_dev_desc_sm1[]
// static struct sub_dev_desc sub_dev_desc_g12a[] __initdata = sub_dev_desc_sm1[]
static struct sub_port_list vpu0_sub_r_tm2[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "AFBC_DEC"			},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "TVFE"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xD, .name = "TCON_P3"				},
	{ .id = 0xE, .name = "VDIN_AFBCE"			},
	{ .id = 0xF, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu0_sub_w_tm2[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "VDIN_AFBCE"			},
	{ .id = 0x4, .name = "TVFE"				},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "TCON3"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_tm2[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct sub_port_list vpu1_sub_w_tm2[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_tm2		vpu0_sub_r_tm2
SUB_DEV_RW(tm2, vpu0, 16, 19);
SUB_DEV_RW(tm2, vpu1, 17, 20);
SUB_DEV_RO(tm2, vpu2, 18);
SUB_DEV_DESC_INIT(tm2, &vpu0_sub_tm2, &vpu1_sub_tm2, &vpu2_sub_tm2);

static struct sub_port_list vpu0_sub_r_t5[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "AFBC_DEC"			},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "TVFE"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xD, .name = "DCNTR_GRID"			},
	{ .id = 0xE, .name = "AFBC_ENC"			},
	{ .id = 0xF, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu0_sub_w_t5[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "VDIN_AFBCE"			},
	{ .id = 0x4, .name = "DCNTR_GRID"			},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "LDIM"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_t5[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_t5[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_t5		vpu0_sub_r_t5
SUB_DEV_RW(t5, vpu0, 16, 19);
SUB_DEV_RW(t5, vpu1, 17, 20);
SUB_DEV_RO(t5, vpu2, 18);
SUB_DEV_DESC_INIT(t5, &vpu0_sub_t5, &vpu1_sub_t5, &vpu2_sub_t5);

static struct sub_port_list vpu0_sub_r_t5d[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "AFBC_DEC"			},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "TVFE"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xE, .name = "AFBC_ENC"			},
	{ .id = 0xF, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu0_sub_w_t5d[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "AFBC_ENC"				},
	{ .id = 0x4, .name = "TVFE"				},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_t5d[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_t5d[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE"				},
};

#define vpu2_sub_r_t5d		vpu0_sub_r_t5d
SUB_DEV_RW(t5d, vpu0, 16, 19);
SUB_DEV_RW(t5d, vpu1, 17, 20);
SUB_DEV_RO(t5d, vpu2, 18);
SUB_DEV_DESC_INIT(t5d, &vpu0_sub_t5d, &vpu1_sub_t5d, &vpu2_sub_t5d);

static struct sub_port_list vpu0_sub_r_t7[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "OSD4"				},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VD3"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xD, .name = "LDIM"				},
	{ .id = 0xE, .name = "VDIN_AFBCE"			},
	{ .id = 0xF, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu0_sub_w_t7[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "VDIN_AFBCE"			},
	{ .id = 0x4, .name = "DCNTR_GRID"			},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "LDIM"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_t7[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_t7[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_t7		vpu0_sub_r_t7
SUB_DEV_RW(t7, vpu0, 80, 80);
SUB_DEV_RW(t7, vpu1, 81, 81);
SUB_DEV_RO(t7, vpu2, 82);
SUB_DEV_DESC_INIT(t7, &vpu0_sub_t7, &vpu1_sub_t7, &vpu2_sub_t7);

static struct sub_port_list vpu0_sub_r_t3[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "OSD4"				},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "NPU"				},
	{ .id = 0x9, .name = "TCON"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "DCNTR_GRID"			},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xF, .name = "VPU_SUBRD"			},
};

static struct sub_port_list vpu0_sub_w_t3[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "VDIN_AFBCE"			},
	{ .id = 0x4, .name = "DCNTR_GRID"			},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "LDIM"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_t3[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_t3[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
	{ .id = 0x5, .name = "AISR_HPF"			},
};

#define vpu2_sub_r_t3		vpu0_sub_r_t3
SUB_DEV_RW(t3, vpu0, 80, 80);
SUB_DEV_RW(t3, vpu1, 81, 81);
SUB_DEV_RO(t3, vpu2, 82);
SUB_DEV_DESC_INIT(t3, &vpu0_sub_t3, &vpu1_sub_t3, &vpu2_sub_t3);
#endif

// static struct sub_dev_desc sub_dev_desc_s4d[] __initdata = sub_dev_desc_s4[]
static struct sub_port_list vpu0_sub_r_s4[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "AFBC_DEC"			},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "TVFE"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xD, .name = "TCON_P3"				},
	{ .id = 0xE, .name = "AFBC_ENC"			},
	{ .id = 0xF, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu0_sub_w_s4[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "AFBC_ENC"				},
	{ .id = 0x4, .name = "TVFE"				},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "TCON3"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_s4[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_s4[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_s4		vpu0_sub_r_s4
SUB_DEV_RW(s4, vpu0, 16, 19);
SUB_DEV_RW(s4, vpu1, 17, 20);
SUB_DEV_RO(s4, vpu2, 18);
SUB_DEV_DESC_INIT(s4, &vpu0_sub_s4, &vpu1_sub_s4, &vpu2_sub_s4);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct sub_port_list vpu0_sub_r_sc2[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "OSD4"				},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "VIU2"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "TVFE"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xD, .name = "TCON_P3"				},
	{ .id = 0xE, .name = "VDIN_AFBCE"			},
	{ .id = 0xF, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu0_sub_w_sc2[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "VDIN_AFBCE"			},
	{ .id = 0x4, .name = "TVFE"				},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "TCON3"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_sc2[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_sc2[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_sc2	vpu0_sub_r_sc2
SUB_DEV_RW(sc2, vpu0, 16, 19);
SUB_DEV_RW(sc2, vpu1, 17, 20);
SUB_DEV_RO(sc2, vpu2, 18);
SUB_DEV_DESC_INIT(sc2, &vpu0_sub_sc2, &vpu1_sub_sc2, &vpu2_sub_sc2);

static struct sub_port_list vpu0_sub_r_t5w[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "AFBC_DEC"				},
	{ .id = 0x6, .name = "DOLBY0"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "NPU"				},
	{ .id = 0x9, .name = "TCON1"				},
	{ .id = 0xA, .name = "RDMA"				},
	{ .id = 0xB, .name = "DCNTR_GRID"			},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xF, .name = "VPU_SUBRD"			},
};

static struct sub_port_list vpu0_sub_w_t5w[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "AFBC_ENC"				},
	{ .id = 0x4, .name = "DCNTR_GRID"			},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "LDIM"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_t5w[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_t5w[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
	{ .id = 0x5, .name = "ASIR_HPF"				},
};

#define vpu2_sub_r_t5w	vpu0_sub_r_t5w
SUB_DEV_RW(t5w, vpu0, 16, 19);
SUB_DEV_RW(t5w, vpu1, 17, 20);
SUB_DEV_RO(t5w, vpu2, 18);
SUB_DEV_DESC_INIT(t5w, &vpu0_sub_t5w, &vpu1_sub_t5w, &vpu2_sub_t5w);

static struct sub_port_list vpu0_sub_r_s5[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "OSD3"				},
	{ .id = 0x5, .name = "OSD4"				},
	{ .id = 0x6, .name = "VD3"				},
	{ .id = 0x7, .name = "MALI_AFBCD"			},
	{ .id = 0x8, .name = "NPU READ"			},
	{ .id = 0x9, .name = "VD4"				},
	{ .id = 0xA, .name = "VD5"				},
	{ .id = 0xB, .name = "NOUSE"				},
	{ .id = 0xC, .name = "DCNTR_GRID"			},
	{ .id = 0xD, .name = "TCON_P1"				},
	{ .id = 0xE, .name = "TCON_P2"				},
	{ .id = 0xF, .name = "VPU_SUBRD"			},
};

static struct sub_port_list vpu0_sub_w_s5[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x3, .name = "VDIN_AFBCE"			},
	{ .id = 0x4, .name = "DCNTR_GRID"			},
	{ .id = 0x5, .name = "TCON1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON2"				},
	{ .id = 0x8, .name = "LDIM"				},
	{ .id = 0x9, .name = "VDIN2"				},
	{ .id = 0xA, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_s5[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct sub_port_list vpu1_sub_w_s5[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
	{ .id = 0x5, .name = "ASIR_HPF"				},
};

#define vpu2_sub_r_s5	vpu0_sub_r_s5
#define vpu3_sub_r_s5	vpu0_sub_r_s5
SUB_DEV_RW(s5, vpu0, 64, 64);
SUB_DEV_RW(s5, vpu1, 66, 66);
SUB_DEV_RO(s5, vpu2, 68);
SUB_DEV_RO(s5, vpu3, 70);
SUB_DEV_DESC_INIT(s5, &vpu0_sub_s5, &vpu1_sub_s5, &vpu2_sub_s5, &vpu3_sub_s5);

static struct sub_port_list vpu0_sub_r_s7d[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "VPU_DMA"				},
	{ .id = 0x5, .name = "RDMA"				},
	{ .id = 0x7, .name = "MALI_AFBCD_RD"			},
	{ .id = 0x8, .name = "DI_IF1"				},
	{ .id = 0x9, .name = "DI_MEM"				},
	{ .id = 0xA, .name = "DI_INP"				},
	{ .id = 0xB, .name = "DI_CHAN2"				},
	{ .id = 0xC, .name = "DI_SUBAXI"			},
	{ .id = 0xD, .name = "DI_IF2"				},
	{ .id = 0xE, .name = "DI_IF0"				},
};

static struct sub_port_list vpu0_sub_w_s7d[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
	{ .id = 0x7, .name = "VDIN1_WR"				},
	{ .id = 0x8, .name = "RDMA"				},
	{ .id = 0x9, .name = "VPU_DMA"				},
};

static struct sub_port_list vpu1_sub_r_s7d[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUBAXI"			},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct sub_port_list vpu1_sub_w_s7d[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
};

SUB_DEV_RW(s7d, vpu0, 1, 4);
SUB_DEV_RW(s7d, vpu1, 2, 6);
SUB_DEV_DESC_INIT(s7d, &vpu0_sub_s7d, &vpu1_sub_s7d);

static struct sub_port_list vpu0_sub_r_s6[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x4, .name = "VPU_DMA"				},
	{ .id = 0x5, .name = "RDMA"				},
	{ .id = 0x6, .name = "VIU2"				},
	{ .id = 0x7, .name = "MALI_AFBCD_RD"			},
	{ .id = 0x8, .name = "DI_IF1"				},
	{ .id = 0x9, .name = "DI_MEM"				},
	{ .id = 0xA, .name = "DI_INP"				},
	{ .id = 0xB, .name = "DI_CHAN2"				},
	{ .id = 0xC, .name = "CONTP_RD"				},
	{ .id = 0xD, .name = "DI_IF2"				},
	{ .id = 0xE, .name = "DI_IF0"				},
	{ .id = 0xF, .name = "DI_AFBCE"				},
};

static struct sub_port_list vpu0_sub_w_s6[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
	{ .id = 0x4, .name = "VDIN0_WR"				},
	{ .id = 0x5, .name = "VDIN1_WR"				},
	{ .id = 0x8, .name = "RDMA"				},
	{ .id = 0x9, .name = "VPU_DMA"				},
	{ .id = 0xD, .name = "VDIN2_WR"				},
};

static struct sub_port_list vpu1_sub_r_s6[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "CONTP_RD"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCE"				},
};

static struct sub_port_list vpu1_sub_w_s6[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
};

#define vpu2_sub_r_s6	vpu0_sub_r_s6
SUB_DEV_RW(s6, vpu0, 1, 1);
SUB_DEV_RW(s6, vpu1, 2, 2);
SUB_DEV_RO(s6, vpu2, 3);
SUB_DEV_DESC_INIT(s6, &vpu0_sub_s6, &vpu1_sub_s6, &vpu2_sub_s6);

static struct sub_port_list vpu0_sub_r_t6d[] __initdata = {
	{ .id = 0x0, .name = "OSD1"				},
	{ .id = 0x1, .name = "OSD2"				},
	{ .id = 0x2, .name = "VD1"				},
	{ .id = 0x3, .name = "VD2"				},
	{ .id = 0x7, .name = "MALI_AFBCD_RD"			},
	{ .id = 0x9, .name = "TCON_P1"				},
	{ .id = 0xA, .name = "RDMA_RD"				},
	{ .id = 0xB, .name = "DCGRD"				},
	{ .id = 0xC, .name = "TCON_P2"				},
	{ .id = 0xF, .name = "DMA_RD"				},
};

static struct sub_port_list vpu0_sub_w_t6d[] __initdata = {
	{ .id = 0x0, .name = "VDIN0_WR"				},
	{ .id = 0x1, .name = "VDIN1_WR"				},
	{ .id = 0x4, .name = "DCGRD"				},
	{ .id = 0x5, .name = "TCON_P1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON_P2"				},
	{ .id = 0xA, .name = "DMA"				},
};

static struct sub_port_list vpu1_sub_r_t6d[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUBAXI"			},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct sub_port_list vpu1_sub_w_t6d[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
};

SUB_DEV_RW(t6d, vpu0, 2, 2);
SUB_DEV_RW(t6d, vpu1, 3, 3);
SUB_DEV_DESC_INIT(t6d, &vpu0_sub_t6d, &vpu1_sub_t6d);

static struct sub_port_list vpu0_sub_r_t5m[] __initdata = {
	{ .id = 0x00, .name = "OSD1"			},
	{ .id = 0x01, .name = "OSD2"			},
	{ .id = 0x02, .name = "VD1"			},
	{ .id = 0x03, .name = "VD2"			},
	{ .id = 0x04, .name = "OSD3"			},
	{ .id = 0x05, .name = "OSD4"			},
	{ .id = 0x06, .name = "DOLBY0"			},
	{ .id = 0x07, .name = "MALI_AFBCD"		},
	{ .id = 0x08, .name = "NPU_READ"		},
	{ .id = 0x09, .name = "TCON_P1"			},
	{ .id = 0x0A, .name = "RDMA_READ"		},
	{ .id = 0x0B, .name = "DCNTR_GRID_RD"		},
	{ .id = 0x0C, .name = "TCON_P2"			},
	{ .id = 0x0F, .name = "VPU_DMA_RD"		},
	{ .id = 0x1F, .name = "DOLBY_TV_RD"		},
	{ .id = 0x2F, .name = "LDIM_RD"			},
	{ .id = 0x3F, .name = "AFBCE_RD"		},
};

static struct sub_port_list vpu1_sub_r_t5m[] __initdata = {
	{ .id = 0x00, .name = "DI_IF1"			},
	{ .id = 0x01, .name = "DI_MEM"			},
	{ .id = 0x02, .name = "DIVR"			},
	{ .id = 0x12, .name = "YDS"			},
	{ .id = 0x22, .name = "CDS"			},
	{ .id = 0x32, .name = "GRID"			},
	{ .id = 0x42, .name = "DI_INP"			},
	{ .id = 0x03, .name = "DI_CHAN2"		},
	{ .id = 0x04, .name = "CONTP_RD"		},
	{ .id = 0x14, .name = "CONTP2_RD"		},
	{ .id = 0x24, .name = "MTN_RD"			},
	{ .id = 0x34, .name = "MCVEC_RD"		},
	{ .id = 0x44, .name = "MCINF_RD"		},
	{ .id = 0x05, .name = "DI_IF2"			},
	{ .id = 0x06, .name = "DI_IF0"			},
	{ .id = 0x07, .name = "DI_AFBCD"		},
};

static struct sub_port_list vpu2_sub_r_t5m[] __initdata = {
	{ .id = 0x00, .name = "OSD1"			},
	{ .id = 0x01, .name = "OSD2"			},
	{ .id = 0x02, .name = "VD1"			},
	{ .id = 0x03, .name = "VD2"			},
	{ .id = 0x04, .name = "OSD3"			},
	{ .id = 0x05, .name = "OSD4"			},
	{ .id = 0x06, .name = "DOLBY0"			},
	{ .id = 0x07, .name = "MALI_AFBCD"		},
	{ .id = 0x08, .name = "NPU_READ"		},
	{ .id = 0x09, .name = "TCON_P1"			},
	{ .id = 0x0A, .name = "RDMA_READ"		},
	{ .id = 0x0B, .name = "DCNTR_GRID_RD"		},
	{ .id = 0x0C, .name = "TCON_P2"			},
	{ .id = 0x0F, .name = "VPU_DMA_RD"		},
	{ .id = 0x1F, .name = "DOLBY_TV_RD"		},
	{ .id = 0x2F, .name = "LDIM_RD"			},
	{ .id = 0x3F, .name = "AFBCE_RD"		},
};

static struct sub_port_list vpu0_sub_w_t5m[] __initdata = {
	{ .id = 0x00, .name = "VDIN0_WR"		},
	{ .id = 0x01, .name = "VDIN1_WR"		},
	{ .id = 0x02, .name = "VDIN_DOLBY"		},
	{ .id = 0x03, .name = "VDIN_AFBCE"		},
	{ .id = 0x04, .name = "DCNTR_GRID_WR"		},
	{ .id = 0x05, .name = "TCON1_WR"		},
	{ .id = 0x06, .name = "RDMA_WR"			},
	{ .id = 0x07, .name = "TCON2_WR"		},
	{ .id = 0x08, .name = "LDIM_WR"			},
	{ .id = 0x09, .name = "VDIN2_WR"		},
	{ .id = 0x0A, .name = "VPU_DMA_WR"		},
};

static struct sub_port_list vpu1_sub_w_t5m[] __initdata = {
	{ .id = 0x00, .name = "NR_WR"			},
	{ .id = 0x01, .name = "DI_WR"			},
	{ .id = 0x02, .name = "CONT_WR"			},
	{ .id = 0x12, .name = "MTN_WR"			},
	{ .id = 0x22, .name = "MCVEC_WR"		},
	{ .id = 0x32, .name = "MCINF_WR"		},
	{ .id = 0x42, .name = "NRDS_WR"			},
	{ .id = 0x03, .name = "DI_AFBCE0"		},
	{ .id = 0x04, .name = "DI_AFBCE1"		},
	{ .id = 0x05, .name = "AISR_HPF_WR"		},
};

static struct sub_port_list vpu2_sub_w_t5m[] __initdata = {
	{ .id = 0x00, .name = "VDIN0_WR"		},
	{ .id = 0x01, .name = "VDIN1_WR"		},
	{ .id = 0x02, .name = "VDIN_DOLBY"		},
	{ .id = 0x03, .name = "VDIN_AFBCE"		},
	{ .id = 0x04, .name = "DCNTR_GRID_WR"		},
	{ .id = 0x05, .name = "TCON1_WR"		},
	{ .id = 0x06, .name = "RDMA_WR"			},
	{ .id = 0x07, .name = "TCON2_WR"		},
	{ .id = 0x08, .name = "LDIM_WR"			},
	{ .id = 0x09, .name = "VDIN2_WR"		},
	{ .id = 0x0A, .name = "VPU_DMA_WR"		},
};

SUB_DEV_RW(t5m, vpu0, 62, 62);
SUB_DEV_RW(t5m, vpu1, 64, 64);
SUB_DEV_RW(t5m, vpu2, 66, 66);
SUB_DEV_DESC_INIT(t5m, &vpu0_sub_t5m, &vpu1_sub_t5m, &vpu2_sub_t5m);

static struct sub_port_list vpu0_sub_r_t6w[] __initdata = {
	{ .id = 0x00, .name = "OSD1"			},
	{ .id = 0x01, .name = "OSD2"			},
	{ .id = 0x04, .name = "OSD3"			},
	{ .id = 0x07, .name = "MALI_AFBC_RD"		},
	{ .id = 0x09, .name = "TCON_P1"			},
	{ .id = 0x0a, .name = "RDMA_RD"			},
	{ .id = 0x0c, .name = "TCON_P2"			},
	{ .id = 0x0f, .name = "DMA_RD"			},
};

static struct sub_port_list vpu0_sub_w_t6w[] __initdata = {
	{ .id = 0x00, .name = "VDIN0_WR"		},
	{ .id = 0x10, .name = "VDIN0_WR"		},
	{ .id = 0x01, .name = "VDIN1_WR"		},
	{ .id = 0x11, .name = "VDIN1_WR"		},
	{ .id = 0x02, .name = "VDIN_DOLBY"		},
	{ .id = 0x03, .name = "VDIN_AFBCE"		},
	{ .id = 0x13, .name = "VDIN_AFBCE"		},
	{ .id = 0x23, .name = "VDIN_AFBCE"		},
	{ .id = 0x33, .name = "VDIN_AFBCE"		},
	{ .id = 0x05, .name = "TCON_P1"			},
	{ .id = 0x06, .name = "RDMA"			},
	{ .id = 0x08, .name = "LDIM"			},
	{ .id = 0x0a, .name = "DMA"			},
};

static struct sub_port_list vpu1_sub_r_t6w[] __initdata = {
	{ .id = 0x00, .name = "DAE_RD0"			},
	{ .id = 0x10, .name = "DAE_RD1"			},
	{ .id = 0x20, .name = "DAE_RD2"			},
	{ .id = 0x30, .name = "DAE_RD3"			},
	{ .id = 0x40, .name = "DAE_RD4"			},
	{ .id = 0x50, .name = "DAE_RD5"			},
	{ .id = 0x60, .name = "DAE_RD6"			},
	{ .id = 0x70, .name = "DAE_RD7"			},
	{ .id = 0x80, .name = "DAE_RD8"			},
	{ .id = 0x90, .name = "DAE_RD9"			},
	{ .id = 0xa0, .name = "DAE_RD10"		},
	{ .id = 0xb0, .name = "DAE_RD11"		},
	{ .id = 0xc0, .name = "DAE_RD11"		},
	{ .id = 0xd0, .name = "DAE_RD11"		},
	{ .id = 0x01, .name = "INP_RD0"			},
	{ .id = 0x11, .name = "INP_RD0"			},
	{ .id = 0x21, .name = "INP_RD0"			},
	{ .id = 0x41, .name = "INP_RD1"			},
	{ .id = 0x32, .name = "RDMA_RD"			},
};

static struct sub_port_list vpu1_sub_w_t6w[] __initdata = {
	{ .id = 0x00, .name = "DAE_WR0"			},
	{ .id = 0x10, .name = "DAE_WR1"			},
	{ .id = 0x20, .name = "DAE_WR2"			},
	{ .id = 0x30, .name = "DAE_WR3"			},
	{ .id = 0x40, .name = "DAE_WR4"			},
	{ .id = 0x50, .name = "DAE_WR5"			},
	{ .id = 0x60, .name = "DAE_WR6"			},
	{ .id = 0x70, .name = "DAE_WR7"			},
	{ .id = 0x80, .name = "DAE_WR8"			},
	{ .id = 0x01, .name = "INP_WR"			},
	{ .id = 0x42, .name = "RDMA_WR"			},
};

static struct sub_port_list vpu2_sub_r_t6w[] __initdata = {
	{ .id = 0x00, .name = "NR_VFCD0"		},
	{ .id = 0x10, .name = "NR_VFCD0"		},
	{ .id = 0x20, .name = "NR_VFCD0"		},
	{ .id = 0x01, .name = "NR_VFCD1"		},
	{ .id = 0x11, .name = "NR_VFCD1"		},
	{ .id = 0x21, .name = "NR_VFCD1"		},
	{ .id = 0x02, .name = "NR_VFCD2"		},
	{ .id = 0x12, .name = "NR_VFCD2"		},
	{ .id = 0x22, .name = "NR_VFCD2"		},
	{ .id = 0x03, .name = "NR_VFCD3"		},
	{ .id = 0x13, .name = "NR_VFCD3"		},
	{ .id = 0x23, .name = "NR_VFCD3"		},
};

static struct sub_port_list vpu2_sub_w_t6w[] __initdata = {
	{ .id = 0x00, .name = "VFCE_WRMIF2"		},
	{ .id = 0x01, .name = "VFCE_WRMIF2"		},
	{ .id = 0x02, .name = "VFCE_WRMIF2"		},
	{ .id = 0x03, .name = "VFCE_WRMIF2"		},
	{ .id = 0x04, .name = "DS_WRMIF"		},
	{ .id = 0x05, .name = "DS_WRMIF"		},
	{ .id = 0x06, .name = "WRMIF0"			},
	{ .id = 0x07, .name = "WRMIF0"			},
	{ .id = 0x08, .name = "WRMIF1"			},
	{ .id = 0x09, .name = "WRMIF1"			},
	{ .id = 0x0a, .name = "DMSQ_WR"			},
	{ .id = 0x0b, .name = "NR_WR0"			},
	{ .id = 0x0c, .name = "NR_WR1"			},
};

static struct sub_port_list vpu3_sub_r_t6w[] __initdata = {
	{ .id = 0x00, .name = "MC_VFCD0"		},
	{ .id = 0x10, .name = "MC_VFCD0"		},
	{ .id = 0x20, .name = "MC_VFCD0"		},
	{ .id = 0x01, .name = "MC_VFCD1"		},
	{ .id = 0x11, .name = "MC_VFCD1"		},
	{ .id = 0x21, .name = "MC_VFCD1"		},
	{ .id = 0x02, .name = "MC_VFCD2"		},
	{ .id = 0x12, .name = "MC_VFCD2"		},
	{ .id = 0x22, .name = "MC_VFCD2"		},
	{ .id = 0x03, .name = "NR_RD0"			},
	{ .id = 0x23, .name = "NR_RD2"			},
	{ .id = 0x33, .name = "NR_RD3"			},
	{ .id = 0x43, .name = "NR_RD4"			},
	{ .id = 0x53, .name = "NR_RD5"			},
	{ .id = 0x63, .name = "NR_RD6"			},
	{ .id = 0x73, .name = "NR_RD7"			},
	{ .id = 0x83, .name = "NR_RD8"			},
	{ .id = 0x93, .name = "NR_RD9"			},
	{ .id = 0xa3, .name = "NR_RD10"			},
	{ .id = 0xb3, .name = "NR_RD10"			},
	{ .id = 0xc3, .name = "NR_RD10"			},
	{ .id = 0x04, .name = "MC_RD0"			},
	{ .id = 0x14, .name = "MC_RD1"			},
	{ .id = 0x24, .name = "MC_RD2"			},
	{ .id = 0x34, .name = "MC_RD3"			},
	{ .id = 0x05, .name = "MMU"			},
};

SUB_DEV_RW(t6w, vpu0, 2, 2);
SUB_DEV_RW(t6w, vpu1, 3, 3);
SUB_DEV_RW(t6w, vpu2, 6, 4);
SUB_DEV_RO(t6w, vpu3, 7);
SUB_DEV_DESC_INIT(t6w, &vpu0_sub_t6w, &vpu1_sub_t6w, &vpu2_sub_t6w, &vpu3_sub_t6w);

static struct sub_port_list vpu0_sub_r_t6x[] __initdata = {
	{ .id = 0x00, .name = "OSD1"			},
	{ .id = 0x01, .name = "OSD2"			},
	{ .id = 0x02, .name = "NR_SUB"			},
	{ .id = 0x03, .name = "MC_SUB"			},
	{ .id = 0x04, .name = "OSD3"			},
	{ .id = 0x05, .name = "MC_VFCD0"		},
	{ .id = 0x15, .name = "MC_VFCD0"		},
	{ .id = 0x25, .name = "MC_VFCD0"		},
	{ .id = 0x06, .name = "MC_VFCD1"		},
	{ .id = 0x16, .name = "MC_VFCD1"		},
	{ .id = 0x26, .name = "MC_VFCD1"		},
	{ .id = 0x07, .name = "MALI_AFBCD"		},
	{ .id = 0x08, .name = "MC_VFCD2"		},
	{ .id = 0x18, .name = "MC_VFCD2"		},
	{ .id = 0x28, .name = "MC_VFCD2"		},
	{ .id = 0x09, .name = "TCON_P1"			},
	{ .id = 0x0A, .name = "RDMA"			},
	{ .id = 0x0B, .name = "LOCAL_DIMMING"		},
	{ .id = 0x0D, .name = "LUT_DMA_RD"		},
	{ .id = 0x0E, .name = "MC_MMU"			},
	{ .id = 0x0F, .name = "DOLBY_TV"		},
};

static struct sub_port_list vpu0_sub_w_t6x[] __initdata = {
	{ .id = 0x00, .name = "VDIN0"			},
	{ .id = 0x01, .name = "VDIN1"			},
	{ .id = 0x02, .name = "VDIN_DOLBY"		},
	{ .id = 0x03, .name = "VDIN_AFBCE"		},
	{ .id = 0x05, .name = "TCON_P1"			},
	{ .id = 0x06, .name = "LDIM_WR"			},
	{ .id = 0x16, .name = "LDIM_WR"			},
	{ .id = 0x26, .name = "LDIM_WR"			},
	{ .id = 0x36, .name = "RDMA_WR"			},
	{ .id = 0x46, .name = "LUT_DMA_WR"		},
	{ .id = 0x56, .name = "DOLBY_TV"		},
	{ .id = 0x66, .name = "DOLBY_TV"		},
	{ .id = 0x76, .name = "DOLBY_TV"		},
	{ .id = 0x0E, .name = "DPE_WR"			},
};

static struct sub_port_list vpu2_sub_r_t6x[] __initdata = {
	{ .id = 0x00, .name = "NR_VFCD0"		},
	{ .id = 0x01, .name = "NR_VFCD1"		},
	{ .id = 0x02, .name = "NR_VFCD2"		},
	{ .id = 0x03, .name = "NR_VFCD3"		},
	{ .id = 0x04, .name = "DAE_RD0"			},
	{ .id = 0x05, .name = "INP_RD"			},
	{ .id = 0x36, .name = "RDMA_RD"			},
};

static struct sub_port_list vpu2_sub_w_t6x[] __initdata = {
	{ .id = 0x00, .name = "VFCE_WRMIF2"		},
	{ .id = 0x01, .name = "VFCE_WRMIF2"		},
	{ .id = 0x02, .name = "VFCE_WRMIF2"		},
	{ .id = 0x03, .name = "VFCE_WRMIF2"		},
	{ .id = 0x04, .name = "DS_WRMIF"		},
	{ .id = 0x05, .name = "WRMIF"			},
	{ .id = 0x06, .name = "WRMIF0"			},
	{ .id = 0x07, .name = "WRMIF0"			},
	{ .id = 0x08, .name = "WRMIF1"			},
	{ .id = 0x09, .name = "WRMIF1"			},
	{ .id = 0x0A, .name = "DMSQ_WR"			},
	{ .id = 0x0B, .name = "NR_WR0"			},
	{ .id = 0x0C, .name = "NR_WR1"			},
	{ .id = 0x0D, .name = "DAE_WR"			},
	{ .id = 0x0E, .name = "INP_WR"			},
	{ .id = 0x4F, .name = "RDMA_WR"			},
};

SUB_DEV_RW(t6x, vpu0, 8, 8);
SUB_DEV_RW(t6x, vpu2, 16, 16);
SUB_DEV_DESC_INIT(t6x, &vpu0_sub_t6x, &vpu2_sub_t6x);

static struct sub_port_list vpu0_sub_r_s7[] __initdata = {
	{ .id = 0x00, .name = "OSD1"			},
	{ .id = 0x01, .name = "OSD2"			},
	{ .id = 0x02, .name = "VD1"			},
	{ .id = 0x03, .name = "VD2"			},
	{ .id = 0x04, .name = "DMA_RD"			},
	{ .id = 0x05, .name = "RDMA_RD"			},
	{ .id = 0x07, .name = "MALI_AFBCD_RD"		},
	{ .id = 0x08, .name = "DI_IF1"			},
	{ .id = 0x09, .name = "DI_MEM"			},
	{ .id = 0x0A, .name = "DI_INP"			},
	{ .id = 0x0B, .name = "DI_CHAN2"		},
	{ .id = 0x0C, .name = "CONTP_RD"		},
	{ .id = 0x1C, .name = "CONTP2_RD"		},
	{ .id = 0x2C, .name = "MTN_RD"			},
	{ .id = 0x3C, .name = "MCVEC_RD"		},
	{ .id = 0x4C, .name = "MCINF_RD"		},
	{ .id = 0x0D, .name = "DI_IF2"			},
	{ .id = 0x0E, .name = "DI_IF0"			},
};

static struct sub_port_list vpu1_sub_r_s7[] __initdata = {
	{ .id = 0x00, .name = "DI_IF1"			},
	{ .id = 0x01, .name = "DI_MEM"			},
	{ .id = 0x02, .name = "DI_INP"			},
	{ .id = 0x03, .name = "DI_CHAN2"		},
	{ .id = 0x04, .name = "CONTP_RD"		},
	{ .id = 0x04, .name = "CONTP2_RD"		},
	{ .id = 0x04, .name = "MTN_RD"			},
	{ .id = 0x04, .name = "MCVEC_RD"		},
	{ .id = 0x04, .name = "MCINF_RD"		},
	{ .id = 0x05, .name = "DI_IF2"			},
	{ .id = 0x06, .name = "DI_IF0"			},
};

static struct sub_port_list vpu0_sub_w_s7[] __initdata = {
	{ .id = 0x00, .name = "NR_WR"			},
	{ .id = 0x01, .name = "DI_WR"			},
	{ .id = 0x02, .name = "CONT_WR"			},
	{ .id = 0x12, .name = "MTN_WR"			},
	{ .id = 0x22, .name = "MCINF_WR"		},
	{ .id = 0x32, .name = "MCVEC_WR"		},
	{ .id = 0x42, .name = "NRDS_WR"			},
	{ .id = 0x05, .name = "VDIN1_WR"		},
	{ .id = 0x08, .name = "RDMA"			},
};

static struct sub_port_list vpu1_sub_w_s7[] __initdata = {
	{ .id = 0x00, .name = "NR_WR"			},
	{ .id = 0x01, .name = "DI_WR"			},
	{ .id = 0x02, .name = "CONT_WR"			},
	{ .id = 0x12, .name = "MTN_WR"			},
	{ .id = 0x22, .name = "MCINF_WR"		},
	{ .id = 0x32, .name = "MCVEC_WR"		},
	{ .id = 0x42, .name = "NRDS_WR"			},
};

SUB_DEV_RW(s7, vpu0, 1, 4);
SUB_DEV_RW(s7, vpu1, 2, 6);
SUB_DEV_DESC_INIT(s7, &vpu0_sub_s7, &vpu1_sub_s7);

static struct sub_port_list vpu0_sub_r_a9[] __initdata = {
	{ .id = 0x00, .name = "OSD1"			},
	{ .id = 0x01, .name = "OSD2"			},
	{ .id = 0x02, .name = "VD1"			},
	{ .id = 0x03, .name = "VD2"			},
	{ .id = 0x04, .name = "OSD3"			},
	{ .id = 0x05, .name = "OSD4"			},
	{ .id = 0x08, .name = "VD3"			},
	{ .id = 0x09, .name = "OSD5"			},
	{ .id = 0x0A, .name = "RDMA"			},
	{ .id = 0x0D, .name = "VDIN0_AFBCE"		},
	{ .id = 0x0E, .name = "VDIN1_AFBCE"		},
	{ .id = 0x0F, .name = "LUT_DMA"			},
};

static struct sub_port_list vpu0_sub_w_a9[] __initdata = {
	{ .id = 0x00, .name = "VDIN0_WR"		},
	{ .id = 0x01, .name = "VDIN1_WR"		},
	{ .id = 0x02, .name = "VDIN_DOLBY_WR"		},
	{ .id = 0x03, .name = "VDIN0_AFBCE_WR"		},
	{ .id = 0x06, .name = "RDMA"			},
	{ .id = 0x09, .name = "VDIN1_AFBCE_WR"		},
	{ .id = 0x0A, .name = "VPU_DMA"			},
};

static struct sub_port_list vpu1_sub_r_a9[] __initdata = {
	{ .id = 0x00, .name = "DI_IF1"			},
	{ .id = 0x01, .name = "DI_MEM"			},
	{ .id = 0x42, .name = "DI_INP"			},
	{ .id = 0x03, .name = "DI_CHAN2"		},
	{ .id = 0x04, .name = "CONTP_RD"		},
	{ .id = 0x14, .name = "CONTP2_RD"		},
	{ .id = 0x24, .name = "MTN_RD"			},
	{ .id = 0x34, .name = "MCVEC_RD"		},
	{ .id = 0x44, .name = "MCINF_RD"		},
	{ .id = 0x05, .name = "DI_IF2"			},
	{ .id = 0x06, .name = "DI_IF0"			},
};

static struct sub_port_list vpu1_sub_w_a9[] __initdata = {
	{ .id = 0x00, .name = "NR_WR"			},
	{ .id = 0x01, .name = "DI_WR"			},
	{ .id = 0x02, .name = "CONT_WR"			},
	{ .id = 0x12, .name = "MTN_WR"			},
	{ .id = 0x22, .name = "MCINF_WR"		},
	{ .id = 0x32, .name = "MCVEC_WR"		},
	{ .id = 0x42, .name = "NRDS_WR"			},
};

SUB_DEV_RW(a9, vpu0, 2, 2);
SUB_DEV_RW(a9, vpu1, 3, 3);
SUB_DEV_DESC_INIT(a9, &vpu0_sub_a9, &vpu1_sub_a9);

static struct sub_port_list vpu0_sub_r_c5[] __initdata = {
	{ .id = 0x00, .name = "VD0"		},
	{ .id = 0x01, .name = "OSD1"		},
};

SUB_DEV_RO(c5, 0, 1);
SUB_DEV_DESC_INIT(c5, &vpu0_sub_c5);
#endif

static int __init sub_dev_desc_alloc(struct sub_dev_desc **dst, struct sub_dev_desc *src)
{
	struct sub_dev_desc *desc;
	struct sub_dev *src_sub;
	struct sub_dev *dst_sub;
	int i;

	if (!src || !src->num)
		return 0;

	/* Check if all NULL */
	for (i = 0; i < src->num; i++) {
		if (src->subs[i])
			break;
	}
	if (i >= src->num)
		return 0;

	desc = kzalloc(sizeof(*desc) + src->num * sizeof(struct sub_dev *),
		       GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	desc->num = src->num;

	for (i = 0; i < src->num; i++) {
		src_sub = src->subs[i];
		if (!src_sub)
			continue;

		dst_sub = kzalloc(sizeof(*dst_sub), GFP_KERNEL);
		if (!dst_sub)
			goto err;

		dst_sub->sub_r.port_id = src_sub->sub_r.port_id;
		dst_sub->sub_r.num = src_sub->sub_r.num;
		if (src_sub->sub_r.num && src_sub->sub_r.list) {
			dst_sub->sub_r.list = kmemdup(src_sub->sub_r.list,
				src_sub->sub_r.num * sizeof(struct sub_port_list), GFP_KERNEL);
			if (!dst_sub->sub_r.list) {
				kfree(dst_sub);
				goto err;
			}
		} else {
			dst_sub->sub_r.list = NULL;
		}

		dst_sub->sub_w.port_id = src_sub->sub_w.port_id;
		dst_sub->sub_w.num = src_sub->sub_w.num;
		if (src_sub->sub_w.num && src_sub->sub_w.list) {
			dst_sub->sub_w.list = kmemdup(src_sub->sub_w.list,
				src_sub->sub_w.num * sizeof(struct sub_port_list), GFP_KERNEL);
			if (!dst_sub->sub_w.list) {
				kfree(dst_sub->sub_r.list);
				kfree(dst_sub);
				goto err;
			}
		} else {
			dst_sub->sub_w.list = NULL;
		}

		desc->subs[i] = dst_sub;
	}

	*dst = desc;
	return 0;

err:
	for (i = 0; i < desc->num; i++) {
		if (desc->subs[i]) {
			kfree(desc->subs[i]->sub_r.list);
			kfree(desc->subs[i]->sub_w.list);
			kfree(desc->subs[i]);
		}
	}
	kfree(desc);
	return -ENOMEM;
}

static struct sub_dev_desc *__init get_sub_dev_desc(int cpu_type)
{
	switch (cpu_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case DMC_TYPE_TXL:
	case DMC_TYPE_TXLX:
	case DMC_TYPE_TXHD:
		return &sub_dev_desc_txlx;
	case DMC_TYPE_TL1:
		return &sub_dev_desc_tl1;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_G12A:
	case DMC_TYPE_G12B:
	case DMC_TYPE_SM1:
		return &sub_dev_desc_sm1;
	case DMC_TYPE_TM2:
		return &sub_dev_desc_tm2;
	case DMC_TYPE_T5:
		return &sub_dev_desc_t5;
	case DMC_TYPE_T5D:
		return &sub_dev_desc_t5d;
	case DMC_TYPE_T7:
		return &sub_dev_desc_t7;
	case DMC_TYPE_T3:
		return &sub_dev_desc_t3;
#endif
	case DMC_TYPE_S4:
		return &sub_dev_desc_s4;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_SC2:
		return &sub_dev_desc_sc2;
	case DMC_TYPE_T5W:
		return &sub_dev_desc_t5w;
	case DMC_TYPE_S5:
		return &sub_dev_desc_s5;
	case DMC_TYPE_S7D:
		return &sub_dev_desc_s7d;
	case DMC_TYPE_S6:
		return &sub_dev_desc_s6;
	case DMC_TYPE_T6D:
		return &sub_dev_desc_t6d;
	case DMC_TYPE_T5M:
		return &sub_dev_desc_t5m;
	case DMC_TYPE_T6W:
		return &sub_dev_desc_t6w;
	case DMC_TYPE_T6X:
		return &sub_dev_desc_t6x;
	case DMC_TYPE_S7:
		return &sub_dev_desc_s7;
	case DMC_TYPE_A9:
		return &sub_dev_desc_a9;
	case DMC_TYPE_C5:
		return &sub_dev_desc_c5;
#endif
	default:
		return NULL;
	}
}

static struct sub_dev_desc *sub_dev_desc_cache;
static int sub_dev_save_type = -1;

int __init sub_dev_desc_init(int cpu_type, struct sub_dev_desc **desc)
{
	struct sub_dev_desc *init_desc;
	int ret;

	if (!desc)
		return -EINVAL;

	*desc = NULL;

	/* Return cached copy if same cpu_type */
	if (sub_dev_save_type == cpu_type) {
		*desc = sub_dev_desc_cache;
		return 0;
	}

	init_desc = get_sub_dev_desc(cpu_type);
	if (!init_desc)
		return -EINVAL;

	ret = sub_dev_desc_alloc(desc, init_desc);
	if (ret)
		return ret;

	/* Cache for reuse */
	sub_dev_desc_cache = *desc;
	sub_dev_save_type = cpu_type;
	return 0;
}

char *sub_dev_to_port(unsigned char port_id, int sid, char rw)
{
	struct sub_dev_desc *desc;
	struct sub_dev *sub;
	struct sub_dev_port *port;
	int i, j;

	desc = dmc_mon ? dmc_mon->sub_dev : (aml_db ? aml_db->sub_dev : NULL);
	if (!desc)
		return NULL;

	sid &= 0xff;

	for (j = 0; j < desc->num; j++) {
		sub = desc->subs[j];
		if (!sub)
			continue;

		if (rw == 'r') {
			if (sub->sub_r.port_id != port_id)
				continue;
			port = &sub->sub_r;
		} else {
			if (sub->sub_w.port_id != port_id)
				continue;
			port = &sub->sub_w;
		}

		if (!port->list)
			continue;

		for (i = 0; i < port->num; i++) {
			if (port->list[i].id == sid)
				return port->list[i].name;
		}
	}
	return NULL;
}

