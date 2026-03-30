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

#define VPU_SUB_RW(soc, index, port_id_r, port_id_w)				\
	static struct vpu_sub vpu##index##_sub_##soc __initdata = {		\
		.sub_r = {							\
			.port_id = port_id_r,					\
			.num = ARRAY_SIZE(vpu##index##_sub_r_##soc),		\
			.list = vpu##index##_sub_r_##soc,			\
		},								\
		.sub_w = {							\
			.port_id = port_id_w,					\
			.num = ARRAY_SIZE(vpu##index##_sub_w_##soc),		\
			.list = vpu##index##_sub_w_##soc,			\
		},								\
	}

#define VPU_SUB_RO(soc, index, port_id_r)					\
	static struct vpu_sub vpu##index##_sub_##soc __initdata = {		\
		.sub_r = {							\
			.port_id = port_id_r,					\
			.num = ARRAY_SIZE(vpu##index##_sub_r_##soc),		\
			.list = vpu##index##_sub_r_##soc,			\
		},								\
		.sub_w = {							\
			.port_id = -1,						\
			.num = 0,						\
			.list = NULL,						\
		},								\
	}

#define VPU_SUB_WO(soc, index, port_id_w)					\
	static struct vpu_sub vpu##index##_sub_##soc __initdata = {		\
		.sub_r = {							\
			.port_id = -1,						\
			.num = 0,						\
			.list = NULL,						\
		},								\
		.sub_w = {							\
			.port_id = port_id_w,					\
			.num = ARRAY_SIZE(vpu##index##_sub_w_##soc),		\
			.list = vpu##index##_sub_w_##soc,			\
		},								\
	}

#define VPU_SUB_DESC_INIT(soc, p0, p1, p2, p3)					\
	static struct vpu_sub_desc vpu_sub_desc_##soc __initdata = {		\
		.vpu0_sub = (p0),						\
		.vpu1_sub = (p1),						\
		.vpu2_sub = (p2),						\
		.vpu3_sub = (p3),						\
	}									\


#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct vpu_sub_list vpu0_sub_r_txlx[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_txlx[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_txlx[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct vpu_sub_list vpu1_sub_w_txlx[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_txlx		vpu0_sub_r_txlx
VPU_SUB_RW(txlx, 0, 8, 11);
VPU_SUB_RW(txlx, 1, 9, 12);
VPU_SUB_RO(txlx, 2, 10);
VPU_SUB_DESC_INIT(txlx, &vpu0_sub_txlx, &vpu1_sub_txlx, &vpu2_sub_txlx, NULL);

//static struct vpu_sub_desc vpu_sub_desc_txl[] __initdata = vpu_sub_desc_txlx[]
//static struct vpu_sub_desc vpu_sub_desc_txhd[] __initdata = vpu_sub_desc_txlx[]
static struct vpu_sub_list vpu0_sub_r_tl1[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_tl1[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_tl1[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_tl1[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_tl1		vpu0_sub_r_tl1
VPU_SUB_RW(tl1, 0, 16, 19);
VPU_SUB_RW(tl1, 1, 17, 20);
VPU_SUB_RO(tl1, 2, 18);
VPU_SUB_DESC_INIT(tl1, &vpu0_sub_tl1, &vpu1_sub_tl1, &vpu2_sub_tl1, NULL);
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct vpu_sub_list vpu0_sub_r_sm1[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_sm1[] __initdata = {
	{ .id = 0x0, .name = "VDIN0"				},
	{ .id = 0x1, .name = "VDIN1"				},
	{ .id = 0x2, .name = "VDIN_DOLBY"			},
	{ .id = 0x6, .name = "RDMA"				},
};

static struct vpu_sub_list vpu1_sub_r_sm1[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct vpu_sub_list vpu1_sub_w_sm1[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_sm1		vpu0_sub_r_sm1
VPU_SUB_RW(sm1, 0, 16, 19);
VPU_SUB_RW(sm1, 1, 17, 20);
VPU_SUB_RO(sm1, 2, 18);
VPU_SUB_DESC_INIT(sm1, &vpu0_sub_sm1, &vpu1_sub_sm1, &vpu2_sub_sm1, NULL);

// static struct vpu_sub_desc vpu_sub_desc_g12b[] __initdata = vpu_sub_desc_sm1[]
// static struct vpu_sub_desc vpu_sub_desc_g12a[] __initdata = vpu_sub_desc_sm1[]
static struct vpu_sub_list vpu0_sub_r_tm2[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_tm2[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_tm2[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct vpu_sub_list vpu1_sub_w_tm2[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
};

#define vpu2_sub_r_tm2		vpu0_sub_r_tm2
VPU_SUB_RW(tm2, 0, 16, 19);
VPU_SUB_RW(tm2, 1, 17, 20);
VPU_SUB_RO(tm2, 2, 18);
VPU_SUB_DESC_INIT(tm2, &vpu0_sub_tm2, &vpu1_sub_tm2, &vpu2_sub_tm2, NULL);

static struct vpu_sub_list vpu0_sub_r_t5[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t5[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_t5[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_t5[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_t5		vpu0_sub_r_t5
VPU_SUB_RW(t5, 0, 16, 19);
VPU_SUB_RW(t5, 1, 17, 20);
VPU_SUB_RO(t5, 2, 18);
VPU_SUB_DESC_INIT(t5, &vpu0_sub_t5, &vpu1_sub_t5, &vpu2_sub_t5, NULL);

static struct vpu_sub_list vpu0_sub_r_t5d[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t5d[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_t5d[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_t5d[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE"				},
};

#define vpu2_sub_r_t5d		vpu0_sub_r_t5d
VPU_SUB_RW(t5d, 0, 16, 19);
VPU_SUB_RW(t5d, 1, 17, 20);
VPU_SUB_RO(t5d, 2, 18);
VPU_SUB_DESC_INIT(t5d, &vpu0_sub_t5d, &vpu1_sub_t5d, &vpu2_sub_t5d, NULL);

static struct vpu_sub_list vpu0_sub_r_t7[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t7[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_t7[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_t7[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_t7		vpu0_sub_r_t7
VPU_SUB_RW(t7, 0, 80, 80);
VPU_SUB_RW(t7, 1, 81, 81);
VPU_SUB_RO(t7, 2, 82);
VPU_SUB_DESC_INIT(t7, &vpu0_sub_t7, &vpu1_sub_t7, &vpu2_sub_t7, NULL);

static struct vpu_sub_list vpu0_sub_r_t3[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t3[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_t3[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_t3[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
	{ .id = 0x5, .name = "AISR_HPF"			},
};

#define vpu2_sub_r_t3		vpu0_sub_r_t3
VPU_SUB_RW(t3, 0, 80, 80);
VPU_SUB_RW(t3, 1, 81, 81);
VPU_SUB_RO(t3, 2, 82);
VPU_SUB_DESC_INIT(t3, &vpu0_sub_t3, &vpu1_sub_t3, &vpu2_sub_t3, NULL);
#endif

// static struct vpu_sub_desc vpu_sub_desc_s4d[] __initdata = vpu_sub_desc_s4[]
static struct vpu_sub_list vpu0_sub_r_s4[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_s4[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_s4[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_s4[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_s4		vpu0_sub_r_s4
VPU_SUB_RW(s4, 0, 16, 19);
VPU_SUB_RW(s4, 1, 17, 20);
VPU_SUB_RO(s4, 2, 18);
VPU_SUB_DESC_INIT(s4, &vpu0_sub_s4, &vpu1_sub_s4, &vpu2_sub_s4, NULL);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct vpu_sub_list vpu0_sub_r_sc2[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_sc2[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_sc2[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_sc2[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
};

#define vpu2_sub_r_sc2	vpu0_sub_r_sc2
VPU_SUB_RW(sc2, 0, 16, 19);
VPU_SUB_RW(sc2, 1, 17, 20);
VPU_SUB_RO(sc2, 2, 18);
VPU_SUB_DESC_INIT(sc2, &vpu0_sub_sc2, &vpu1_sub_sc2, &vpu2_sub_sc2, NULL);

static struct vpu_sub_list vpu0_sub_r_t5w[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t5w[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_t5w[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_t5w[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
	{ .id = 0x5, .name = "ASIR_HPF"				},
};

#define vpu2_sub_r_t5w	vpu0_sub_r_t5w
VPU_SUB_RW(t5w, 0, 16, 19);
VPU_SUB_RW(t5w, 1, 17, 20);
VPU_SUB_RO(t5w, 2, 18);
VPU_SUB_DESC_INIT(t5w, &vpu0_sub_t5w, &vpu1_sub_t5w, &vpu2_sub_t5w, NULL);

static struct vpu_sub_list vpu0_sub_r_s5[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_s5[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_s5[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUB"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCD"				},
};

static struct vpu_sub_list vpu1_sub_w_s5[] __initdata = {
	{ .id = 0x0, .name = "NR"				},
	{ .id = 0x1, .name = "DI"				},
	{ .id = 0x2, .name = "DI_SUB"				},
	{ .id = 0x3, .name = "DI_AFBCE0"			},
	{ .id = 0x4, .name = "DI_AFBCE1"			},
	{ .id = 0x5, .name = "ASIR_HPF"				},
};

#define vpu2_sub_r_s5	vpu0_sub_r_s5
#define vpu3_sub_r_s5	vpu0_sub_r_s5
VPU_SUB_RW(s5, 0, 64, 64);
VPU_SUB_RW(s5, 1, 66, 66);
VPU_SUB_RO(s5, 2, 68);
VPU_SUB_RO(s5, 3, 70);
VPU_SUB_DESC_INIT(s5, &vpu0_sub_s5, &vpu1_sub_s5, &vpu2_sub_s5, &vpu3_sub_s5);

static struct vpu_sub_list vpu0_sub_r_s7d[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_s7d[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
	{ .id = 0x7, .name = "VDIN1_WR"				},
	{ .id = 0x8, .name = "RDMA"				},
	{ .id = 0x9, .name = "VPU_DMA"				},
};

static struct vpu_sub_list vpu1_sub_r_s7d[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUBAXI"			},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct vpu_sub_list vpu1_sub_w_s7d[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
};

VPU_SUB_RW(s7d, 0, 1, 4);
VPU_SUB_RW(s7d, 1, 2, 6);
VPU_SUB_DESC_INIT(s7d, &vpu0_sub_s7d, &vpu1_sub_s7d, NULL, NULL);

static struct vpu_sub_list vpu0_sub_r_s6[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_s6[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
	{ .id = 0x4, .name = "VDIN0_WR"				},
	{ .id = 0x5, .name = "VDIN1_WR"				},
	{ .id = 0x8, .name = "RDMA"				},
	{ .id = 0x9, .name = "VPU_DMA"				},
	{ .id = 0xD, .name = "VDIN2_WR"				},
};

static struct vpu_sub_list vpu1_sub_r_s6[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "CONTP_RD"				},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
	{ .id = 0x7, .name = "DI_AFBCE"				},
};

static struct vpu_sub_list vpu1_sub_w_s6[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
};

#define vpu2_sub_r_s6	vpu0_sub_r_s6
VPU_SUB_RW(s6, 0, 1, 1);
VPU_SUB_RW(s6, 1, 2, 2);
VPU_SUB_RO(s6, 2, 3);
VPU_SUB_DESC_INIT(s6, &vpu0_sub_s6, &vpu1_sub_s6, &vpu2_sub_s6, NULL);

static struct vpu_sub_list vpu0_sub_r_t6d[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t6d[] __initdata = {
	{ .id = 0x0, .name = "VDIN0_WR"				},
	{ .id = 0x1, .name = "VDIN1_WR"				},
	{ .id = 0x4, .name = "DCGRD"				},
	{ .id = 0x5, .name = "TCON_P1"				},
	{ .id = 0x6, .name = "RDMA"				},
	{ .id = 0x7, .name = "TCON_P2"				},
	{ .id = 0xA, .name = "DMA"				},
};

static struct vpu_sub_list vpu1_sub_r_t6d[] __initdata = {
	{ .id = 0x0, .name = "DI_IF1"				},
	{ .id = 0x1, .name = "DI_MEM"				},
	{ .id = 0x2, .name = "DI_INP"				},
	{ .id = 0x3, .name = "DI_CHAN2"				},
	{ .id = 0x4, .name = "DI_SUBAXI"			},
	{ .id = 0x5, .name = "DI_IF2"				},
	{ .id = 0x6, .name = "DI_IF0"				},
};

static struct vpu_sub_list vpu1_sub_w_t6d[] __initdata = {
	{ .id = 0x0, .name = "NR_WR"				},
	{ .id = 0x1, .name = "DI_WR"				},
	{ .id = 0x2, .name = "DI_SUBAXI"			},
};

VPU_SUB_RW(t6d, 0, 2, 2);
VPU_SUB_RW(t6d, 1, 3, 3);
VPU_SUB_DESC_INIT(t6d, &vpu0_sub_t6d, &vpu1_sub_t6d, NULL, NULL);

static struct vpu_sub_list vpu0_sub_r_t5m[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_t5m[] __initdata = {
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

static struct vpu_sub_list vpu2_sub_r_t5m[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t5m[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_w_t5m[] __initdata = {
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

static struct vpu_sub_list vpu2_sub_w_t5m[] __initdata = {
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

VPU_SUB_RW(t5m, 0, 62, 62);
VPU_SUB_RW(t5m, 1, 64, 64);
VPU_SUB_RW(t5m, 2, 66, 66);
VPU_SUB_DESC_INIT(t5m, &vpu0_sub_t5m, &vpu1_sub_t5m, &vpu2_sub_t5m, NULL);

static struct vpu_sub_list vpu0_sub_r_t6w[] __initdata = {
	{ .id = 0x00, .name = "OSD1"			},
	{ .id = 0x01, .name = "OSD2"			},
	{ .id = 0x04, .name = "OSD3"			},
	{ .id = 0x07, .name = "MALI_AFBC_RD"		},
	{ .id = 0x09, .name = "TCON_P1"			},
	{ .id = 0x0a, .name = "RDMA_RD"			},
	{ .id = 0x0c, .name = "TCON_P2"			},
	{ .id = 0x0f, .name = "DMA_RD"			},
};

static struct vpu_sub_list vpu0_sub_w_t6w[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_t6w[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_w_t6w[] __initdata = {
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

static struct vpu_sub_list vpu2_sub_r_t6w[] __initdata = {
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

static struct vpu_sub_list vpu2_sub_w_t6w[] __initdata = {
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

static struct vpu_sub_list vpu3_sub_r_t6w[] __initdata = {
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

VPU_SUB_RW(t6w, 0, 2, 2);
VPU_SUB_RW(t6w, 1, 3, 3);
VPU_SUB_RW(t6w, 2, 6, 4);
VPU_SUB_RO(t6w, 3, 7);
VPU_SUB_DESC_INIT(t6w, &vpu0_sub_t6w, &vpu1_sub_t6w, &vpu2_sub_t6w, &vpu3_sub_t6w);

static struct vpu_sub_list vpu0_sub_r_t6x[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_t6x[] __initdata = {
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

static struct vpu_sub_list vpu2_sub_r_t6x[] __initdata = {
	{ .id = 0x00, .name = "NR_VFCD0"		},
	{ .id = 0x01, .name = "NR_VFCD1"		},
	{ .id = 0x02, .name = "NR_VFCD2"		},
	{ .id = 0x03, .name = "NR_VFCD3"		},
	{ .id = 0x04, .name = "DAE_RD0"			},
	{ .id = 0x05, .name = "INP_RD"			},
	{ .id = 0x36, .name = "RDMA_RD"			},
};

static struct vpu_sub_list vpu2_sub_w_t6x[] __initdata = {
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

VPU_SUB_RW(t6x, 0, 8, 8);
VPU_SUB_RW(t6x, 2, 16, 16);
VPU_SUB_DESC_INIT(t6x, &vpu0_sub_t6x, NULL, &vpu2_sub_t6x, NULL);

static struct vpu_sub_list vpu0_sub_r_s7[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_r_s7[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_s7[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_w_s7[] __initdata = {
	{ .id = 0x00, .name = "NR_WR"			},
	{ .id = 0x01, .name = "DI_WR"			},
	{ .id = 0x02, .name = "CONT_WR"			},
	{ .id = 0x12, .name = "MTN_WR"			},
	{ .id = 0x22, .name = "MCINF_WR"		},
	{ .id = 0x32, .name = "MCVEC_WR"		},
	{ .id = 0x42, .name = "NRDS_WR"			},
};

VPU_SUB_RW(s7, 0, 1, 4);
VPU_SUB_RW(s7, 1, 2, 6);
VPU_SUB_DESC_INIT(s7, &vpu0_sub_s7, &vpu1_sub_s7, NULL, NULL);

static struct vpu_sub_list vpu0_sub_r_a9[] __initdata = {
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

static struct vpu_sub_list vpu0_sub_w_a9[] __initdata = {
	{ .id = 0x00, .name = "VDIN0_WR"		},
	{ .id = 0x01, .name = "VDIN1_WR"		},
	{ .id = 0x02, .name = "VDIN_DOLBY_WR"		},
	{ .id = 0x03, .name = "VDIN0_AFBCE_WR"		},
	{ .id = 0x06, .name = "RDMA"			},
	{ .id = 0x09, .name = "VDIN1_AFBCE_WR"		},
	{ .id = 0x0A, .name = "VPU_DMA"			},
};

static struct vpu_sub_list vpu1_sub_r_a9[] __initdata = {
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

static struct vpu_sub_list vpu1_sub_w_a9[] __initdata = {
	{ .id = 0x00, .name = "NR_WR"			},
	{ .id = 0x01, .name = "DI_WR"			},
	{ .id = 0x02, .name = "CONT_WR"			},
	{ .id = 0x12, .name = "MTN_WR"			},
	{ .id = 0x22, .name = "MCINF_WR"		},
	{ .id = 0x32, .name = "MCVEC_WR"		},
	{ .id = 0x42, .name = "NRDS_WR"			},
};

VPU_SUB_RW(a9, 0, 2, 2);
VPU_SUB_RW(a9, 1, 3, 3);
VPU_SUB_DESC_INIT(a9, &vpu0_sub_a9, &vpu1_sub_a9, NULL, NULL);
#endif

static int __init vpu_sub_desc_alloc(struct vpu_sub_desc **dst, struct vpu_sub_desc *src)
{
	struct vpu_sub_desc *desc;
	struct vpu_sub *src_sub;
	struct vpu_sub *dst_sub;
	struct vpu_sub **src_subs;
	struct vpu_sub **dst_subs;
	int i;

	if (!src)
		return 0;

	/* Check if all NULL */
	src_subs = (struct vpu_sub **)&src->vpu0_sub;
	for (i = 0; i < VPU_SUB_CNT; i++) {
		if (src_subs[i])
			break;
	}
	if (i >= VPU_SUB_CNT)
		return 0;

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;

	dst_subs = &desc->vpu0_sub;

	for (i = 0; i < VPU_SUB_CNT; i++) {
		if (!src_subs[i])
			continue;

		src_sub = src_subs[i];
		dst_sub = kzalloc(sizeof(*dst_sub), GFP_KERNEL);
		if (!dst_sub)
			goto err;

		dst_sub->sub_r.port_id = src_sub->sub_r.port_id;
		dst_sub->sub_r.num = src_sub->sub_r.num;
		if (src_sub->sub_r.num && src_sub->sub_r.list) {
			dst_sub->sub_r.list = kmemdup(src_sub->sub_r.list,
				src_sub->sub_r.num * sizeof(struct vpu_sub_list), GFP_KERNEL);
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
				src_sub->sub_w.num * sizeof(struct vpu_sub_list), GFP_KERNEL);
			if (!dst_sub->sub_w.list) {
				kfree(dst_sub->sub_r.list);
				kfree(dst_sub);
				goto err;
			}
		} else {
			dst_sub->sub_w.list = NULL;
		}

		dst_subs[i] = dst_sub;
	}

	*dst = desc;
	return 0;

err:
	for (i = 0; i < VPU_SUB_CNT; i++) {
		if (dst_subs[i]) {
			kfree(dst_subs[i]->sub_r.list);
			kfree(dst_subs[i]->sub_w.list);
			kfree(dst_subs[i]);
		}
	}
	kfree(desc);
	return -ENOMEM;
}

static struct vpu_sub_desc *__init get_vpu_sub_desc(int cpu_type)
{
	switch (cpu_type) {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	case DMC_TYPE_TXL:
	case DMC_TYPE_TXLX:
	case DMC_TYPE_TXHD:
		return &vpu_sub_desc_txlx;
	case DMC_TYPE_TL1:
		return &vpu_sub_desc_tl1;
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_G12A:
	case DMC_TYPE_G12B:
	case DMC_TYPE_SM1:
		return &vpu_sub_desc_sm1;
	case DMC_TYPE_TM2:
		return &vpu_sub_desc_tm2;
	case DMC_TYPE_T5:
		return &vpu_sub_desc_t5;
	case DMC_TYPE_T5D:
		return &vpu_sub_desc_t5d;
	case DMC_TYPE_T7:
		return &vpu_sub_desc_t7;
	case DMC_TYPE_T3:
		return &vpu_sub_desc_t3;
#endif
	case DMC_TYPE_S4:
		return &vpu_sub_desc_s4;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	case DMC_TYPE_SC2:
		return &vpu_sub_desc_sc2;
	case DMC_TYPE_T5W:
		return &vpu_sub_desc_t5w;
	case DMC_TYPE_S5:
		return &vpu_sub_desc_s5;
	case DMC_TYPE_S7D:
		return &vpu_sub_desc_s7d;
	case DMC_TYPE_S6:
		return &vpu_sub_desc_s6;
	case DMC_TYPE_T6D:
		return &vpu_sub_desc_t6d;
	case DMC_TYPE_T5M:
		return &vpu_sub_desc_t5m;
	case DMC_TYPE_T6W:
		return &vpu_sub_desc_t6w;
	case DMC_TYPE_T6X:
		return &vpu_sub_desc_t6x;
	case DMC_TYPE_S7:
		return &vpu_sub_desc_s7;
	case DMC_TYPE_A9:
		return &vpu_sub_desc_a9;
#endif
	default:
		return NULL;
	}
}

static struct vpu_sub_desc *vpu_sub_desc_cache;
static int vpu_sub_save_type = -1;

int __init vpu_sub_desc_init(int cpu_type, struct vpu_sub_desc **desc)
{
	struct vpu_sub_desc *init_desc;
	int ret;

	if (!desc)
		return -EINVAL;

	*desc = NULL;

	/* Return cached copy if same cpu_type */
	if (vpu_sub_save_type == cpu_type) {
		*desc = vpu_sub_desc_cache;
		return 0;
	}

	init_desc = get_vpu_sub_desc(cpu_type);
	if (!init_desc)
		return -EINVAL;

	ret = vpu_sub_desc_alloc(desc, init_desc);
	if (ret)
		return ret;

	/* Cache for reuse */
	vpu_sub_desc_cache = *desc;
	vpu_sub_save_type = cpu_type;
	return 0;
}

char *vpu_to_sub_port(unsigned char port_id, int sid, char rw)
{
	struct vpu_sub_desc *desc;
	struct vpu_sub **vpu_subs;
	struct vpu_sub *sub;
	struct vpu_sub_port *port;
	int i, j;

	desc = dmc_mon ? dmc_mon->vpu_sub : (aml_db ? aml_db->vpu_sub : NULL);
	if (!desc)
		return NULL;

	sid &= 0xff;
	vpu_subs = (struct vpu_sub **)&desc->vpu0_sub;

	for (j = 0; j < VPU_SUB_CNT; j++) {
		sub = vpu_subs[j];
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

