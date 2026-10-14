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

#define SUB_DEV(soc, name, port_id_rw, parent_id_rw)				\
	static struct sub_dev name##_sub_##soc __initdata = {			\
		.sub_r = {							\
			.port_id = port_id_rw,					\
			.parent_sub_id = parent_id_rw,				\
			.num = ARRAY_SIZE(name##_sub_rw_##soc),			\
			.list = name##_sub_rw_##soc,				\
		},								\
		.sub_w = {							\
			.port_id = port_id_rw,					\
			.parent_sub_id = parent_id_rw,				\
			.num = ARRAY_SIZE(name##_sub_rw_##soc),			\
			.list = name##_sub_rw_##soc,				\
		},								\
	}

#define SUB_DEV_RW(soc, name, port_id_r, port_id_w, parent_id)			\
	static struct sub_dev name##_sub_##soc __initdata = {			\
		.sub_r = {							\
			.port_id = port_id_r,					\
			.parent_sub_id = parent_id,				\
			.num = ARRAY_SIZE(name##_sub_r_##soc),			\
			.list = name##_sub_r_##soc,				\
		},								\
		.sub_w = {							\
			.port_id = port_id_w,					\
			.parent_sub_id = parent_id,				\
			.num = ARRAY_SIZE(name##_sub_w_##soc),			\
			.list = name##_sub_w_##soc,				\
		},								\
	}

#define SUB_DEV_RO(soc, name, port_id_r, parent_id)				\
	static struct sub_dev name##_sub_##soc __initdata = {			\
		.sub_r = {							\
			.port_id = port_id_r,					\
			.parent_sub_id = parent_id,				\
			.num = ARRAY_SIZE(name##_sub_r_##soc),			\
			.list = name##_sub_r_##soc,				\
		},								\
		.sub_w = {							\
			.port_id = -1,						\
			.parent_sub_id = -1,					\
			.num = 0,						\
			.list = NULL,						\
		},								\
	}

#define SUB_DEV_WO(soc, name, port_id_w, parent_id)				\
	static struct sub_dev name##_sub_##soc __initdata = {			\
		.sub_r = {							\
			.port_id = -1,						\
			.parent_sub_id = -1,					\
			.num = 0,						\
			.list = NULL,						\
		},								\
		.sub_w = {							\
			.port_id = port_id_w,					\
			.parent_sub_id = parent_id,				\
			.num = ARRAY_SIZE(name##_sub_w_##soc),			\
			.list = name##_sub_w_##soc,				\
		},								\
	}

#define SUB_DEV_DESC_INIT(soc, ...)						\
	static struct sub_dev_desc sub_dev_desc_##soc __initdata = {		\
		.num = sizeof((struct sub_dev *[]){ __VA_ARGS__ })		\
		      / sizeof(struct sub_dev *),				\
		.subs = { __VA_ARGS__ },					\
	}

#define SUB_PORT(_id, _name) \
	{ .id = (_id), .name = (_name)}


#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static struct sub_port_list vpu0_sub_r_txlx[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "AFBC_DEC"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "TVFE"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xD, "TCON_P3"),
	SUB_PORT(0xE, "AFBC_ENC"),
};

static struct sub_port_list vpu0_sub_w_txlx[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "AFBC_ENC"),
	SUB_PORT(0x4, "TVFE"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "TCON3"),
};

static struct sub_port_list vpu1_sub_r_txlx[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
};

static struct sub_port_list vpu1_sub_w_txlx[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
};

#define vpu2_sub_r_txlx		vpu0_sub_r_txlx
SUB_DEV_RW(txlx, vpu0, 8, 11, -1);
SUB_DEV_RW(txlx, vpu1, 9, 12, -1);
SUB_DEV_RO(txlx, vpu2, 10, -1);
SUB_DEV_DESC_INIT(txlx, &vpu0_sub_txlx, &vpu1_sub_txlx, &vpu2_sub_txlx);

//static struct sub_dev_desc sub_dev_desc_txl[] __initdata = sub_dev_desc_txlx[]
//static struct sub_dev_desc sub_dev_desc_txhd[] __initdata = sub_dev_desc_txlx[]
static struct sub_port_list vpu0_sub_r_tl1[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "AFBC_DEC"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "TVFE"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xD, "TCON_P3"),
	SUB_PORT(0xE, "AFBC_ENC"),
};

static struct sub_port_list vpu0_sub_w_tl1[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "AFBC_ENC"),
	SUB_PORT(0x4, "TVFE"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "TCON3"),
};

static struct sub_port_list vpu1_sub_r_tl1[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_tl1[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
};

#define vpu2_sub_r_tl1		vpu0_sub_r_tl1
SUB_DEV_RW(tl1, vpu0, 16, 19, -1);
SUB_DEV_RW(tl1, vpu1, 17, 20, -1);
SUB_DEV_RO(tl1, vpu2, 18, -1);
SUB_DEV_DESC_INIT(tl1, &vpu0_sub_tl1, &vpu1_sub_tl1, &vpu2_sub_tl1);
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct sub_port_list vpu0_sub_r_sm1[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0xA, "RDMA"),
};

static struct sub_port_list vpu0_sub_w_sm1[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x6, "RDMA"),
};

static struct sub_port_list vpu1_sub_r_sm1[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
};

static struct sub_port_list vpu1_sub_w_sm1[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
};

#define vpu2_sub_r_sm1		vpu0_sub_r_sm1
SUB_DEV_RW(sm1, vpu0, 16, 19, -1);
SUB_DEV_RW(sm1, vpu1, 17, 20, -1);
SUB_DEV_RO(sm1, vpu2, 18, -1);
SUB_DEV_DESC_INIT(sm1, &vpu0_sub_sm1, &vpu1_sub_sm1, &vpu2_sub_sm1);

// static struct sub_dev_desc sub_dev_desc_g12b[] __initdata = sub_dev_desc_sm1[]
// static struct sub_dev_desc sub_dev_desc_g12a[] __initdata = sub_dev_desc_sm1[]
static struct sub_port_list vpu0_sub_r_tm2[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "AFBC_DEC"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "TVFE"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xD, "TCON_P3"),
	SUB_PORT(0xE, "VDIN_AFBCE"),
	SUB_PORT(0xF, "VPU_DMA"),
};

static struct sub_port_list vpu0_sub_w_tm2[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "VDIN_AFBCE"),
	SUB_PORT(0x4, "TVFE"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "TCON3"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_tm2[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
};

static struct sub_port_list vpu1_sub_w_tm2[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
};

#define vpu2_sub_r_tm2		vpu0_sub_r_tm2
SUB_DEV_RW(tm2, vpu0, 16, 19, -1);
SUB_DEV_RW(tm2, vpu1, 17, 20, -1);
SUB_DEV_RO(tm2, vpu2, 18, -1);
SUB_DEV_DESC_INIT(tm2, &vpu0_sub_tm2, &vpu1_sub_tm2, &vpu2_sub_tm2);

static struct sub_port_list vpu0_sub_r_t5[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "AFBC_DEC"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "TVFE"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xD, "DCNTR_GRID"),
	SUB_PORT(0xE, "AFBC_ENC"),
	SUB_PORT(0xF, "VPU_DMA"),
};

static struct sub_port_list vpu0_sub_w_t5[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "VDIN_AFBCE"),
	SUB_PORT(0x4, "DCNTR_GRID"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "LDIM"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_t5[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_t5[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x3, "DI_AFBCE0"),
	SUB_PORT(0x4, "DI_AFBCE1"),
};

#define vpu2_sub_r_t5		vpu0_sub_r_t5
SUB_DEV_RW(t5, vpu0, 16, 19, -1);
SUB_DEV_RW(t5, vpu1, 17, 20, -1);
SUB_DEV_RO(t5, vpu2, 18, -1);
SUB_DEV_DESC_INIT(t5, &vpu0_sub_t5, &vpu1_sub_t5, &vpu2_sub_t5);

static struct sub_port_list vpu0_sub_r_t5d[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "AFBC_DEC"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "TVFE"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xE, "AFBC_ENC"),
	SUB_PORT(0xF, "VPU_DMA"),
};

static struct sub_port_list vpu0_sub_w_t5d[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "AFBC_ENC"),
	SUB_PORT(0x4, "TVFE"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_t5d[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_t5d[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x3, "DI_AFBCE"),
};

#define vpu2_sub_r_t5d		vpu0_sub_r_t5d
SUB_DEV_RW(t5d, vpu0, 16, 19, -1);
SUB_DEV_RW(t5d, vpu1, 17, 20, -1);
SUB_DEV_RO(t5d, vpu2, 18, -1);
SUB_DEV_DESC_INIT(t5d, &vpu0_sub_t5d, &vpu1_sub_t5d, &vpu2_sub_t5d);

static struct sub_port_list vpu0_sub_r_t7[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "OSD4"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VD3"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xD, "LDIM"),
	SUB_PORT(0xE, "VDIN_AFBCE"),
	SUB_PORT(0xF, "VPU_DMA"),
};

static struct sub_port_list vpu0_sub_w_t7[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "VDIN_AFBCE"),
	SUB_PORT(0x4, "DCNTR_GRID"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "LDIM"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_t7[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_t7[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x4, "DI_AFBCE1"),
};

#define vpu2_sub_r_t7		vpu0_sub_r_t7
SUB_DEV_RW(t7, vpu0, 80, 80, -1);
SUB_DEV_RW(t7, vpu1, 81, 81, -1);
SUB_DEV_RO(t7, vpu2, 82, -1);
SUB_DEV_DESC_INIT(t7, &vpu0_sub_t7, &vpu1_sub_t7, &vpu2_sub_t7);

static struct sub_port_list vpu0_sub_r_t3[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "OSD4"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "NPU"),
	SUB_PORT(0x9, "TCON"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "DCNTR_GRID"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xF, "VPU_SUBRD"),
};

static struct sub_port_list vpu0_sub_w_t3[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "VDIN_AFBCE"),
	SUB_PORT(0x4, "DCNTR_GRID"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "LDIM"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_t3[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_t3[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x3, "DI_AFBCE0"),
	SUB_PORT(0x4, "DI_AFBCE1"),
	SUB_PORT(0x5, "AISR_HPF"),
};

#define vpu2_sub_r_t3		vpu0_sub_r_t3
SUB_DEV_RW(t3, vpu0, 80, 80, -1);
SUB_DEV_RW(t3, vpu1, 81, 81, -1);
SUB_DEV_RO(t3, vpu2, 82, -1);
SUB_DEV_DESC_INIT(t3, &vpu0_sub_t3, &vpu1_sub_t3, &vpu2_sub_t3);
#endif

// static struct sub_dev_desc sub_dev_desc_s4d[] __initdata = sub_dev_desc_s4[]
static struct sub_port_list vpu0_sub_r_s4[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "AFBC_DEC"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "TVFE"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xD, "TCON_P3"),
	SUB_PORT(0xE, "AFBC_ENC"),
	SUB_PORT(0xF, "VPU_DMA"),
};

static struct sub_port_list vpu0_sub_w_s4[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "AFBC_ENC"),
	SUB_PORT(0x4, "TVFE"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "TCON3"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_s4[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_s4[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x3, "DI_AFBCE0"),
	SUB_PORT(0x4, "DI_AFBCE1"),
};

#define vpu2_sub_r_s4		vpu0_sub_r_s4
SUB_DEV_RW(s4, vpu0, 16, 19, -1);
SUB_DEV_RW(s4, vpu1, 17, 20, -1);
SUB_DEV_RO(s4, vpu2, 18, -1);
SUB_DEV_DESC_INIT(s4, &vpu0_sub_s4, &vpu1_sub_s4, &vpu2_sub_s4);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct sub_port_list vpu0_sub_r_sc2[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "OSD4"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "VIU2"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "TVFE"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xD, "TCON_P3"),
	SUB_PORT(0xE, "VDIN_AFBCE"),
	SUB_PORT(0xF, "VPU_DMA"),
};

static struct sub_port_list vpu0_sub_w_sc2[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "VDIN_AFBCE"),
	SUB_PORT(0x4, "TVFE"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "TCON3"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_sc2[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_sc2[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x3, "DI_AFBCE0"),
	SUB_PORT(0x4, "DI_AFBCE1"),
};

#define vpu2_sub_r_sc2	vpu0_sub_r_sc2
SUB_DEV_RW(sc2, vpu0, 16, 19, -1);
SUB_DEV_RW(sc2, vpu1, 17, 20, -1);
SUB_DEV_RO(sc2, vpu2, 18, -1);
SUB_DEV_DESC_INIT(sc2, &vpu0_sub_sc2, &vpu1_sub_sc2, &vpu2_sub_sc2);

static struct sub_port_list vpu0_sub_r_t5w[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "AFBC_DEC"),
	SUB_PORT(0x6, "DOLBY0"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "NPU"),
	SUB_PORT(0x9, "TCON1"),
	SUB_PORT(0xA, "RDMA"),
	SUB_PORT(0xB, "DCNTR_GRID"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xF, "VPU_SUBRD"),
};

static struct sub_port_list vpu0_sub_w_t5w[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "AFBC_ENC"),
	SUB_PORT(0x4, "DCNTR_GRID"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "LDIM"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_t5w[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_t5w[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x3, "DI_AFBCE0"),
	SUB_PORT(0x4, "DI_AFBCE1"),
	SUB_PORT(0x5, "ASIR_HPF"),
};

#define vpu2_sub_r_t5w	vpu0_sub_r_t5w
SUB_DEV_RW(t5w, vpu0, 16, 19, -1);
SUB_DEV_RW(t5w, vpu1, 17, 20, -1);
SUB_DEV_RO(t5w, vpu2, 18, -1);
SUB_DEV_DESC_INIT(t5w, &vpu0_sub_t5w, &vpu1_sub_t5w, &vpu2_sub_t5w);

static struct sub_port_list vpu0_sub_r_s5[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "OSD3"),
	SUB_PORT(0x5, "OSD4"),
	SUB_PORT(0x6, "VD3"),
	SUB_PORT(0x7, "MALI_AFBCD"),
	SUB_PORT(0x8, "NPU READ"),
	SUB_PORT(0x9, "VD4"),
	SUB_PORT(0xA, "VD5"),
	SUB_PORT(0xB, "NOUSE"),
	SUB_PORT(0xC, "DCNTR_GRID"),
	SUB_PORT(0xD, "TCON_P1"),
	SUB_PORT(0xE, "TCON_P2"),
	SUB_PORT(0xF, "VPU_SUBRD"),
};

static struct sub_port_list vpu0_sub_w_s5[] __initdata = {
	SUB_PORT(0x0, "VDIN0"),
	SUB_PORT(0x1, "VDIN1"),
	SUB_PORT(0x2, "VDIN_DOLBY"),
	SUB_PORT(0x3, "VDIN_AFBCE"),
	SUB_PORT(0x4, "DCNTR_GRID"),
	SUB_PORT(0x5, "TCON1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON2"),
	SUB_PORT(0x8, "LDIM"),
	SUB_PORT(0x9, "VDIN2"),
	SUB_PORT(0xA, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_s5[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUB"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCD"),
};

static struct sub_port_list vpu1_sub_w_s5[] __initdata = {
	SUB_PORT(0x0, "NR"),
	SUB_PORT(0x1, "DI"),
	SUB_PORT(0x2, "DI_SUB"),
	SUB_PORT(0x3, "DI_AFBCE0"),
	SUB_PORT(0x4, "DI_AFBCE1"),
	SUB_PORT(0x5, "ASIR_HPF"),
};

#define vpu2_sub_r_s5	vpu0_sub_r_s5
#define vpu3_sub_r_s5	vpu0_sub_r_s5
SUB_DEV_RW(s5, vpu0, 64, 64, -1);
SUB_DEV_RW(s5, vpu1, 66, 66, -1);
SUB_DEV_RO(s5, vpu2, 68, -1);
SUB_DEV_RO(s5, vpu3, 70, -1);
SUB_DEV_DESC_INIT(s5, &vpu0_sub_s5, &vpu1_sub_s5, &vpu2_sub_s5, &vpu3_sub_s5);

static struct sub_port_list vpu0_sub_r_s7d[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "VPU_DMA"),
	SUB_PORT(0x5, "RDMA"),
	SUB_PORT(0x7, "MALI_AFBCD_RD"),
	SUB_PORT(0x8, "DI_IF1"),
	SUB_PORT(0x9, "DI_MEM"),
	SUB_PORT(0xA, "DI_INP"),
	SUB_PORT(0xB, "DI_CHAN2"),
	SUB_PORT(0xC, "DI_SUBAXI"),
	SUB_PORT(0xD, "DI_IF2"),
	SUB_PORT(0xE, "DI_IF0"),
};

static struct sub_port_list vpu0_sub_w_s7d[] __initdata = {
	SUB_PORT(0x0, "NR_WR"),
	SUB_PORT(0x1, "DI_WR"),
	SUB_PORT(0x2, "DI_SUBAXI"),
	SUB_PORT(0x7, "VDIN1_WR"),
	SUB_PORT(0x8, "RDMA"),
	SUB_PORT(0x9, "VPU_DMA"),
};

static struct sub_port_list vpu1_sub_r_s7d[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUBAXI"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
};

static struct sub_port_list vpu1_sub_w_s7d[] __initdata = {
	SUB_PORT(0x0, "NR_WR"),
	SUB_PORT(0x1, "DI_WR"),
	SUB_PORT(0x2, "DI_SUBAXI"),
};

SUB_DEV_RW(s7d, vpu0, 1, 4, -1);
SUB_DEV_RW(s7d, vpu1, 2, 6, -1);
SUB_DEV_DESC_INIT(s7d, &vpu0_sub_s7d, &vpu1_sub_s7d);

static struct sub_port_list vpu0_sub_r_s6[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x4, "VPU_DMA"),
	SUB_PORT(0x5, "RDMA"),
	SUB_PORT(0x6, "VIU2"),
	SUB_PORT(0x7, "MALI_AFBCD_RD"),
	SUB_PORT(0x8, "DI_IF1"),
	SUB_PORT(0x9, "DI_MEM"),
	SUB_PORT(0xA, "DI_INP"),
	SUB_PORT(0xB, "DI_CHAN2"),
	SUB_PORT(0xC, "CONTP_RD"),
	SUB_PORT(0xD, "DI_IF2"),
	SUB_PORT(0xE, "DI_IF0"),
	SUB_PORT(0xF, "DI_AFBCE"),
};

static struct sub_port_list vpu0_sub_w_s6[] __initdata = {
	SUB_PORT(0x0, "NR_WR"),
	SUB_PORT(0x1, "DI_WR"),
	SUB_PORT(0x2, "DI_SUBAXI"),
	SUB_PORT(0x4, "VDIN0_WR"),
	SUB_PORT(0x5, "VDIN1_WR"),
	SUB_PORT(0x8, "RDMA"),
	SUB_PORT(0x9, "VPU_DMA"),
	SUB_PORT(0xD, "VDIN2_WR"),
};

static struct sub_port_list vpu1_sub_r_s6[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "CONTP_RD"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
	SUB_PORT(0x7, "DI_AFBCE"),
};

static struct sub_port_list vpu1_sub_w_s6[] __initdata = {
	SUB_PORT(0x0, "NR_WR"),
	SUB_PORT(0x1, "DI_WR"),
	SUB_PORT(0x2, "DI_SUBAXI"),
};

#define vpu2_sub_r_s6	vpu0_sub_r_s6
SUB_DEV_RW(s6, vpu0, 1, 1, -1);
SUB_DEV_RW(s6, vpu1, 2, 2, -1);
SUB_DEV_RO(s6, vpu2, 3, -1);
SUB_DEV_DESC_INIT(s6, &vpu0_sub_s6, &vpu1_sub_s6, &vpu2_sub_s6);

static struct sub_port_list vpu0_sub_r_t6d[] __initdata = {
	SUB_PORT(0x0, "OSD1"),
	SUB_PORT(0x1, "OSD2"),
	SUB_PORT(0x2, "VD1"),
	SUB_PORT(0x3, "VD2"),
	SUB_PORT(0x7, "MALI_AFBCD_RD"),
	SUB_PORT(0x9, "TCON_P1"),
	SUB_PORT(0xA, "RDMA_RD"),
	SUB_PORT(0xB, "DCGRD"),
	SUB_PORT(0xC, "TCON_P2"),
	SUB_PORT(0xF, "DMA_RD"),
};

static struct sub_port_list vpu0_sub_w_t6d[] __initdata = {
	SUB_PORT(0x0, "VDIN0_WR"),
	SUB_PORT(0x1, "VDIN1_WR"),
	SUB_PORT(0x4, "DCGRD"),
	SUB_PORT(0x5, "TCON_P1"),
	SUB_PORT(0x6, "RDMA"),
	SUB_PORT(0x7, "TCON_P2"),
	SUB_PORT(0xA, "DMA"),
};

static struct sub_port_list vpu1_sub_r_t6d[] __initdata = {
	SUB_PORT(0x0, "DI_IF1"),
	SUB_PORT(0x1, "DI_MEM"),
	SUB_PORT(0x2, "DI_INP"),
	SUB_PORT(0x3, "DI_CHAN2"),
	SUB_PORT(0x4, "DI_SUBAXI"),
	SUB_PORT(0x5, "DI_IF2"),
	SUB_PORT(0x6, "DI_IF0"),
};

static struct sub_port_list vpu1_sub_w_t6d[] __initdata = {
	SUB_PORT(0x0, "NR_WR"),
	SUB_PORT(0x1, "DI_WR"),
	SUB_PORT(0x2, "DI_SUBAXI"),
};

SUB_DEV_RW(t6d, vpu0, 2, 2, -1);
SUB_DEV_RW(t6d, vpu1, 3, 3, -1);
SUB_DEV_DESC_INIT(t6d, &vpu0_sub_t6d, &vpu1_sub_t6d);

static struct sub_port_list vpu0_sub_r_t5m[] __initdata = {
	SUB_PORT(0x00, "OSD1"),
	SUB_PORT(0x01, "OSD2"),
	SUB_PORT(0x02, "VD1"),
	SUB_PORT(0x03, "VD2"),
	SUB_PORT(0x04, "OSD3"),
	SUB_PORT(0x05, "OSD4"),
	SUB_PORT(0x06, "DOLBY0"),
	SUB_PORT(0x07, "MALI_AFBCD"),
	SUB_PORT(0x08, "NPU_READ"),
	SUB_PORT(0x09, "TCON_P1"),
	SUB_PORT(0x0A, "RDMA_READ"),
	SUB_PORT(0x0B, "DCNTR_GRID_RD"),
	SUB_PORT(0x0C, "TCON_P2"),
	SUB_PORT(0x0F, "VPU_DMA_RD"),
	SUB_PORT(0x1F, "DOLBY_TV_RD"),
	SUB_PORT(0x2F, "LDIM_RD"),
	SUB_PORT(0x3F, "AFBCE_RD"),
};

static struct sub_port_list vpu1_sub_r_t5m[] __initdata = {
	SUB_PORT(0x00, "DI_IF1"),
	SUB_PORT(0x01, "DI_MEM"),
	SUB_PORT(0x02, "DIVR"),
	SUB_PORT(0x12, "YDS"),
	SUB_PORT(0x22, "CDS"),
	SUB_PORT(0x32, "GRID"),
	SUB_PORT(0x42, "DI_INP"),
	SUB_PORT(0x03, "DI_CHAN2"),
	SUB_PORT(0x04, "CONTP_RD"),
	SUB_PORT(0x14, "CONTP2_RD"),
	SUB_PORT(0x24, "MTN_RD"),
	SUB_PORT(0x34, "MCVEC_RD"),
	SUB_PORT(0x44, "MCINF_RD"),
	SUB_PORT(0x05, "DI_IF2"),
	SUB_PORT(0x06, "DI_IF0"),
	SUB_PORT(0x07, "DI_AFBCD"),
};

static struct sub_port_list vpu2_sub_r_t5m[] __initdata = {
	SUB_PORT(0x00, "OSD1"),
	SUB_PORT(0x01, "OSD2"),
	SUB_PORT(0x02, "VD1"),
	SUB_PORT(0x03, "VD2"),
	SUB_PORT(0x04, "OSD3"),
	SUB_PORT(0x05, "OSD4"),
	SUB_PORT(0x06, "DOLBY0"),
	SUB_PORT(0x07, "MALI_AFBCD"),
	SUB_PORT(0x08, "NPU_READ"),
	SUB_PORT(0x09, "TCON_P1"),
	SUB_PORT(0x0A, "RDMA_READ"),
	SUB_PORT(0x0B, "DCNTR_GRID_RD"),
	SUB_PORT(0x0C, "TCON_P2"),
	SUB_PORT(0x0F, "VPU_DMA_RD"),
	SUB_PORT(0x1F, "DOLBY_TV_RD"),
	SUB_PORT(0x2F, "LDIM_RD"),
	SUB_PORT(0x3F, "AFBCE_RD"),
};

static struct sub_port_list vpu0_sub_w_t5m[] __initdata = {
	SUB_PORT(0x00, "VDIN0_WR"),
	SUB_PORT(0x01, "VDIN1_WR"),
	SUB_PORT(0x02, "VDIN_DOLBY"),
	SUB_PORT(0x03, "VDIN_AFBCE"),
	SUB_PORT(0x04, "DCNTR_GRID_WR"),
	SUB_PORT(0x05, "TCON1_WR"),
	SUB_PORT(0x06, "RDMA_WR"),
	SUB_PORT(0x07, "TCON2_WR"),
	SUB_PORT(0x08, "LDIM_WR"),
	SUB_PORT(0x09, "VDIN2_WR"),
	SUB_PORT(0x0A, "VPU_DMA_WR"),
};

static struct sub_port_list vpu1_sub_w_t5m[] __initdata = {
	SUB_PORT(0x00, "NR_WR"),
	SUB_PORT(0x01, "DI_WR"),
	SUB_PORT(0x02, "CONT_WR"),
	SUB_PORT(0x12, "MTN_WR"),
	SUB_PORT(0x22, "MCVEC_WR"),
	SUB_PORT(0x32, "MCINF_WR"),
	SUB_PORT(0x42, "NRDS_WR"),
	SUB_PORT(0x03, "DI_AFBCE0"),
	SUB_PORT(0x04, "DI_AFBCE1"),
	SUB_PORT(0x05, "AISR_HPF_WR"),
};

static struct sub_port_list vpu2_sub_w_t5m[] __initdata = {
	SUB_PORT(0x00, "VDIN0_WR"),
	SUB_PORT(0x01, "VDIN1_WR"),
	SUB_PORT(0x02, "VDIN_DOLBY"),
	SUB_PORT(0x03, "VDIN_AFBCE"),
	SUB_PORT(0x04, "DCNTR_GRID_WR"),
	SUB_PORT(0x05, "TCON1_WR"),
	SUB_PORT(0x06, "RDMA_WR"),
	SUB_PORT(0x07, "TCON2_WR"),
	SUB_PORT(0x08, "LDIM_WR"),
	SUB_PORT(0x09, "VDIN2_WR"),
	SUB_PORT(0x0A, "VPU_DMA_WR"),
};

SUB_DEV_RW(t5m, vpu0, 62, 62, -1);
SUB_DEV_RW(t5m, vpu1, 64, 64, -1);
SUB_DEV_RW(t5m, vpu2, 66, 66, -1);
SUB_DEV_DESC_INIT(t5m, &vpu0_sub_t5m, &vpu1_sub_t5m, &vpu2_sub_t5m);

static struct sub_port_list vpu0_sub_r_t6w[] __initdata = {
	SUB_PORT(0x00, "OSD1"),
	SUB_PORT(0x01, "OSD2"),
	SUB_PORT(0x04, "OSD3"),
	SUB_PORT(0x07, "MALI_AFBC_RD"),
	SUB_PORT(0x09, "TCON_P1"),
	SUB_PORT(0x0a, "RDMA_RD"),
	SUB_PORT(0x0c, "TCON_P2"),
	SUB_PORT(0x0f, "DMA_RD"),
};

static struct sub_port_list vpu0_sub_w_t6w[] __initdata = {
	SUB_PORT(0x00, "VDIN0_WR"),
	SUB_PORT(0x10, "VDIN0_WR"),
	SUB_PORT(0x01, "VDIN1_WR"),
	SUB_PORT(0x11, "VDIN1_WR"),
	SUB_PORT(0x02, "VDIN_DOLBY"),
	SUB_PORT(0x03, "VDIN_AFBCE"),
	SUB_PORT(0x13, "VDIN_AFBCE"),
	SUB_PORT(0x23, "VDIN_AFBCE"),
	SUB_PORT(0x33, "VDIN_AFBCE"),
	SUB_PORT(0x05, "TCON_P1"),
	SUB_PORT(0x06, "RDMA"),
	SUB_PORT(0x08, "LDIM"),
	SUB_PORT(0x0a, "DMA"),
};

static struct sub_port_list vpu1_sub_r_t6w[] __initdata = {
	SUB_PORT(0x00, "DAE_RD0"),
	SUB_PORT(0x10, "DAE_RD1"),
	SUB_PORT(0x20, "DAE_RD2"),
	SUB_PORT(0x30, "DAE_RD3"),
	SUB_PORT(0x40, "DAE_RD4"),
	SUB_PORT(0x50, "DAE_RD5"),
	SUB_PORT(0x60, "DAE_RD6"),
	SUB_PORT(0x70, "DAE_RD7"),
	SUB_PORT(0x80, "DAE_RD8"),
	SUB_PORT(0x90, "DAE_RD9"),
	SUB_PORT(0xa0, "DAE_RD10"),
	SUB_PORT(0xb0, "DAE_RD11"),
	SUB_PORT(0xc0, "DAE_RD11"),
	SUB_PORT(0xd0, "DAE_RD11"),
	SUB_PORT(0x01, "INP_RD0"),
	SUB_PORT(0x11, "INP_RD0"),
	SUB_PORT(0x21, "INP_RD0"),
	SUB_PORT(0x41, "INP_RD1"),
	SUB_PORT(0x32, "RDMA_RD"),
};

static struct sub_port_list vpu1_sub_w_t6w[] __initdata = {
	SUB_PORT(0x00, "DAE_WR0"),
	SUB_PORT(0x10, "DAE_WR1"),
	SUB_PORT(0x20, "DAE_WR2"),
	SUB_PORT(0x30, "DAE_WR3"),
	SUB_PORT(0x40, "DAE_WR4"),
	SUB_PORT(0x50, "DAE_WR5"),
	SUB_PORT(0x60, "DAE_WR6"),
	SUB_PORT(0x70, "DAE_WR7"),
	SUB_PORT(0x80, "DAE_WR8"),
	SUB_PORT(0x01, "INP_WR"),
	SUB_PORT(0x42, "RDMA_WR"),
};

static struct sub_port_list vpu2_sub_r_t6w[] __initdata = {
	SUB_PORT(0x00, "NR_VFCD0"),
	SUB_PORT(0x10, "NR_VFCD0"),
	SUB_PORT(0x20, "NR_VFCD0"),
	SUB_PORT(0x01, "NR_VFCD1"),
	SUB_PORT(0x11, "NR_VFCD1"),
	SUB_PORT(0x21, "NR_VFCD1"),
	SUB_PORT(0x02, "NR_VFCD2"),
	SUB_PORT(0x12, "NR_VFCD2"),
	SUB_PORT(0x22, "NR_VFCD2"),
	SUB_PORT(0x03, "NR_VFCD3"),
	SUB_PORT(0x13, "NR_VFCD3"),
	SUB_PORT(0x23, "NR_VFCD3"),
};

static struct sub_port_list vpu2_sub_w_t6w[] __initdata = {
	SUB_PORT(0x00, "VFCE_WRMIF2"),
	SUB_PORT(0x01, "VFCE_WRMIF2"),
	SUB_PORT(0x02, "VFCE_WRMIF2"),
	SUB_PORT(0x03, "VFCE_WRMIF2"),
	SUB_PORT(0x04, "DS_WRMIF"),
	SUB_PORT(0x05, "DS_WRMIF"),
	SUB_PORT(0x06, "WRMIF0"),
	SUB_PORT(0x07, "WRMIF0"),
	SUB_PORT(0x08, "WRMIF1"),
	SUB_PORT(0x09, "WRMIF1"),
	SUB_PORT(0x0a, "DMSQ_WR"),
	SUB_PORT(0x0b, "NR_WR0"),
	SUB_PORT(0x0c, "NR_WR1"),
};

static struct sub_port_list vpu3_sub_r_t6w[] __initdata = {
	SUB_PORT(0x00, "MC_VFCD0"),
	SUB_PORT(0x10, "MC_VFCD0"),
	SUB_PORT(0x20, "MC_VFCD0"),
	SUB_PORT(0x01, "MC_VFCD1"),
	SUB_PORT(0x11, "MC_VFCD1"),
	SUB_PORT(0x21, "MC_VFCD1"),
	SUB_PORT(0x02, "MC_VFCD2"),
	SUB_PORT(0x12, "MC_VFCD2"),
	SUB_PORT(0x22, "MC_VFCD2"),
	SUB_PORT(0x03, "NR_RD0"),
	SUB_PORT(0x23, "NR_RD2"),
	SUB_PORT(0x33, "NR_RD3"),
	SUB_PORT(0x43, "NR_RD4"),
	SUB_PORT(0x53, "NR_RD5"),
	SUB_PORT(0x63, "NR_RD6"),
	SUB_PORT(0x73, "NR_RD7"),
	SUB_PORT(0x83, "NR_RD8"),
	SUB_PORT(0x93, "NR_RD9"),
	SUB_PORT(0xa3, "NR_RD10"),
	SUB_PORT(0xb3, "NR_RD10"),
	SUB_PORT(0xc3, "NR_RD10"),
	SUB_PORT(0x04, "MC_RD0"),
	SUB_PORT(0x14, "MC_RD1"),
	SUB_PORT(0x24, "MC_RD2"),
	SUB_PORT(0x34, "MC_RD3"),
	SUB_PORT(0x05, "MMU"),
};

SUB_DEV_RW(t6w, vpu0, 2, 2, -1);
SUB_DEV_RW(t6w, vpu1, 3, 3, -1);
SUB_DEV_RW(t6w, vpu2, 6, 4, -1);
SUB_DEV_RO(t6w, vpu3, 7, -1);
SUB_DEV_DESC_INIT(t6w, &vpu0_sub_t6w, &vpu1_sub_t6w, &vpu2_sub_t6w, &vpu3_sub_t6w);

static struct sub_port_list vpu0_sub_r_t6x[] __initdata = {
	SUB_PORT(0x00, "OSD1"),
	SUB_PORT(0x01, "OSD2"),
	SUB_PORT(0x02, "NR_SUB"),
	SUB_PORT(0x03, "MC_SUB"),
	SUB_PORT(0x04, "OSD3"),
	SUB_PORT(0x05, "MC_VFCD0"),
	SUB_PORT(0x15, "MC_VFCD0"),
	SUB_PORT(0x25, "MC_VFCD0"),
	SUB_PORT(0x06, "MC_VFCD1"),
	SUB_PORT(0x16, "MC_VFCD1"),
	SUB_PORT(0x26, "MC_VFCD1"),
	SUB_PORT(0x07, "MALI_AFBCD"),
	SUB_PORT(0x08, "MC_VFCD2"),
	SUB_PORT(0x18, "MC_VFCD2"),
	SUB_PORT(0x28, "MC_VFCD2"),
	SUB_PORT(0x09, "TCON_P1"),
	SUB_PORT(0x0A, "RDMA"),
	SUB_PORT(0x0B, "LOCAL_DIMMING"),
	SUB_PORT(0x0D, "LUT_DMA_RD"),
	SUB_PORT(0x0E, "MC_MMU"),
	SUB_PORT(0x0F, "DOLBY_TV"),
};

static struct sub_port_list vpu0_sub_w_t6x[] __initdata = {
	SUB_PORT(0x00, "VDIN0"),
	SUB_PORT(0x01, "VDIN1"),
	SUB_PORT(0x02, "VDIN_DOLBY"),
	SUB_PORT(0x03, "VDIN_AFBCE"),
	SUB_PORT(0x05, "TCON_P1"),
	SUB_PORT(0x06, "LDIM_WR"),
	SUB_PORT(0x16, "LDIM_WR"),
	SUB_PORT(0x26, "LDIM_WR"),
	SUB_PORT(0x36, "RDMA_WR"),
	SUB_PORT(0x46, "LUT_DMA_WR"),
	SUB_PORT(0x56, "DOLBY_TV"),
	SUB_PORT(0x66, "DOLBY_TV"),
	SUB_PORT(0x76, "DOLBY_TV"),
	SUB_PORT(0x0E, "DPE_WR"),
};

static struct sub_port_list vpu2_sub_r_t6x[] __initdata = {
	SUB_PORT(0x00, "NR_VFCD0"),
	SUB_PORT(0x01, "NR_VFCD1"),
	SUB_PORT(0x02, "NR_VFCD2"),
	SUB_PORT(0x03, "NR_VFCD3"),
	SUB_PORT(0x04, "DAE_RD0"),
	SUB_PORT(0x05, "INP_RD"),
	SUB_PORT(0x36, "RDMA_RD"),
};

static struct sub_port_list vpu2_sub_w_t6x[] __initdata = {
	SUB_PORT(0x00, "VFCE_WRMIF2"),
	SUB_PORT(0x01, "VFCE_WRMIF2"),
	SUB_PORT(0x02, "VFCE_WRMIF2"),
	SUB_PORT(0x03, "VFCE_WRMIF2"),
	SUB_PORT(0x04, "DS_WRMIF"),
	SUB_PORT(0x05, "WRMIF"),
	SUB_PORT(0x06, "WRMIF0"),
	SUB_PORT(0x07, "WRMIF0"),
	SUB_PORT(0x08, "WRMIF1"),
	SUB_PORT(0x09, "WRMIF1"),
	SUB_PORT(0x0A, "DMSQ_WR"),
	SUB_PORT(0x0B, "NR_WR0"),
	SUB_PORT(0x0C, "NR_WR1"),
	SUB_PORT(0x0D, "DAE_WR"),
	SUB_PORT(0x0E, "INP_WR"),
	SUB_PORT(0x4F, "RDMA_WR"),
};

SUB_DEV_RW(t6x, vpu0, 8, 8, -1);
SUB_DEV_RW(t6x, vpu2, 16, 16, -1);
SUB_DEV_DESC_INIT(t6x, &vpu0_sub_t6x, &vpu2_sub_t6x);

static struct sub_port_list vpu0_sub_r_s7[] __initdata = {
	SUB_PORT(0x00, "OSD1"),
	SUB_PORT(0x01, "OSD2"),
	SUB_PORT(0x02, "VD1"),
	SUB_PORT(0x03, "VD2"),
	SUB_PORT(0x04, "DMA_RD"),
	SUB_PORT(0x05, "RDMA_RD"),
	SUB_PORT(0x07, "MALI_AFBCD_RD"),
	SUB_PORT(0x08, "DI_IF1"),
	SUB_PORT(0x09, "DI_MEM"),
	SUB_PORT(0x0A, "DI_INP"),
	SUB_PORT(0x0B, "DI_CHAN2"),
	SUB_PORT(0x0C, "CONTP_RD"),
	SUB_PORT(0x1C, "CONTP2_RD"),
	SUB_PORT(0x2C, "MTN_RD"),
	SUB_PORT(0x3C, "MCVEC_RD"),
	SUB_PORT(0x4C, "MCINF_RD"),
	SUB_PORT(0x0D, "DI_IF2"),
	SUB_PORT(0x0E, "DI_IF0"),
};

static struct sub_port_list vpu1_sub_r_s7[] __initdata = {
	SUB_PORT(0x00, "DI_IF1"),
	SUB_PORT(0x01, "DI_MEM"),
	SUB_PORT(0x02, "DI_INP"),
	SUB_PORT(0x03, "DI_CHAN2"),
	SUB_PORT(0x04, "CONTP_RD"),
	SUB_PORT(0x04, "CONTP2_RD"),
	SUB_PORT(0x04, "MTN_RD"),
	SUB_PORT(0x04, "MCVEC_RD"),
	SUB_PORT(0x04, "MCINF_RD"),
	SUB_PORT(0x05, "DI_IF2"),
	SUB_PORT(0x06, "DI_IF0"),
};

static struct sub_port_list vpu0_sub_w_s7[] __initdata = {
	SUB_PORT(0x00, "NR_WR"),
	SUB_PORT(0x01, "DI_WR"),
	SUB_PORT(0x02, "CONT_WR"),
	SUB_PORT(0x12, "MTN_WR"),
	SUB_PORT(0x22, "MCINF_WR"),
	SUB_PORT(0x32, "MCVEC_WR"),
	SUB_PORT(0x42, "NRDS_WR"),
	SUB_PORT(0x05, "VDIN1_WR"),
	SUB_PORT(0x08, "RDMA"),
};

static struct sub_port_list vpu1_sub_w_s7[] __initdata = {
	SUB_PORT(0x00, "NR_WR"),
	SUB_PORT(0x01, "DI_WR"),
	SUB_PORT(0x02, "CONT_WR"),
	SUB_PORT(0x12, "MTN_WR"),
	SUB_PORT(0x22, "MCINF_WR"),
	SUB_PORT(0x32, "MCVEC_WR"),
	SUB_PORT(0x42, "NRDS_WR"),
};

SUB_DEV_RW(s7, vpu0, 1, 4, -1);
SUB_DEV_RW(s7, vpu1, 2, 6, -1);
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

SUB_DEV_RW(a9, vpu0, 2, 2, -1);
SUB_DEV_RW(a9, vpu1, 3, 3, -1);
SUB_DEV_DESC_INIT(a9, &vpu0_sub_a9, &vpu1_sub_a9);

static struct sub_port_list vpu0_sub_r_c5[] __initdata = {
	SUB_PORT(0x00, "VD0"),
	SUB_PORT(0x01, "OSD1"),
};

static struct sub_port_list hevc_sub_w_c5[] __initdata = {
	SUB_PORT(0x08, "Amrisc LMEM Write"),
	SUB_PORT(0x12, "Swap Write"),
	SUB_PORT(0x15, "VP9 Process Write"),
	SUB_PORT(0x16, "VP9 Map Process Write"),
	SUB_PORT(0x1B, "AV1 Context Write"),
	SUB_PORT(0x1C, "AV1 Segment Write"),
	SUB_PORT(0x1D, "AV1 TOP Write"),
	SUB_PORT(0x1E, "AV1 GMC Write"),
	SUB_PORT(0x20, "IPP Linebuf WR Cache"),
	SUB_PORT(0x32, "Mpred Write"),
	SUB_PORT(0x50, "MPP COMV Write"),
	SUB_PORT(0x51, "MPP COMV Write"),
	SUB_PORT(0x52, "MPP COMV Write"),
	SUB_PORT(0x53, "MPP COMV Write"),
	SUB_PORT(0x54, "MPP COMV Write"),
	SUB_PORT(0x55, "MPP COMV Write"),
	SUB_PORT(0x56, "MPP COMV Write"),
	SUB_PORT(0x57, "MPP COMV Write"),
	SUB_PORT(0x58, "MPP COMV Write"),
	SUB_PORT(0x59, "MPP COMV Write"),
	SUB_PORT(0x5A, "MPP COMV Write"),
	SUB_PORT(0x5B, "MPP COMV Write"),
	SUB_PORT(0x5C, "MPP COMV Write"),
	SUB_PORT(0x5D, "MPP COMV Write"),
	SUB_PORT(0x5E, "MPP COMV Write"),
	SUB_PORT(0x5F, "MPP COMV Write"),
	SUB_PORT(0x60, "Compress FIFO2 Write"),
	SUB_PORT(0x61, "Compress Tile Write"),
	SUB_PORT(0x62, "Compress Header Write"),
	SUB_PORT(0x63, "Compress Body Write"),
	SUB_PORT(0x74, "OW SAO Write Data0"),
	SUB_PORT(0x75, "OW SAO Write Data1"),
	SUB_PORT(0x76, "OW SAO Write Data2"),
	SUB_PORT(0x77, "OW SAO Write Data3"),
	SUB_PORT(0x80, "Pscale/XIO TOP/XIO XOT Write"),
	SUB_PORT(0x81, "XIO LEFT/XOL Write"),
	SUB_PORT(0x90, "VLD MEM/CPI Write"),
	SUB_PORT(0xA0, "H264 TOP Buff/Evan Write"),
	SUB_PORT(0xB1, "CO_MB Write"),
	SUB_PORT(0xC0, "OW SAO Write Data2"),
	SUB_PORT(0xD0, "OW SAO Write Data3"),
	SUB_PORT(0xE0, "Pscale/XIO TOP Write"),
	SUB_PORT(0xE1, "XIO LEFT Write"),
	SUB_PORT(0xE2, "VLD MEM Write"),
	SUB_PORT(0xE3, "H264 TOP Buff Write"),
	SUB_PORT(0xF0, "CO_MB Write"),
	SUB_PORT(0x91, "CPI Write"),
};

static struct sub_port_list hevc_sub_r_c5[] __initdata = {
	SUB_PORT(0x00, "Amrisc IMEM Read"),
	SUB_PORT(0x08, "Amrisc LMEM Read"),
	SUB_PORT(0x11, "Bitstream Read"),
	SUB_PORT(0x12, "Swap Read"),
	SUB_PORT(0x13, "Map Read"),
	SUB_PORT(0x15, "VP9 Process Read"),
	SUB_PORT(0x16, "VP9 Map Process Read"),
	SUB_PORT(0x1B, "AV1 Context Read"),
	SUB_PORT(0x1C, "AV1 Segment Read"),
	SUB_PORT(0x1D, "AV1 TOP Read"),
	SUB_PORT(0x20, "IPP Linebuf RD Cache"),
	SUB_PORT(0x31, "Mpred Read"),
	SUB_PORT(0x50, "MCR Data0 Read"),
	SUB_PORT(0x51, "MCR Data1 Read"),
	SUB_PORT(0x52, "MCR Header Read"),
	SUB_PORT(0x61, "Compress Tile Read"),
	SUB_PORT(0x62, "Compress Header Read"),
	SUB_PORT(0x80, "XIO/Pscale Read"),
	SUB_PORT(0x90, "VLD MEM/CPI Read"),
	SUB_PORT(0xA0, "H264 TOP Buff READ"),
	SUB_PORT(0xB1, "CO_MB Read"),
	SUB_PORT(0xC0, "IQIDCT Canvas Read"),
	SUB_PORT(0xE0, "EXTIF BUF0 Read"),
	SUB_PORT(0xE1, "EXTIF BUF1 Read"),
	SUB_PORT(0xE2, "EXTIF BUF2 Read"),
	SUB_PORT(0xE3, "EXTIF BUF3 Read"),
	SUB_PORT(0xF0, "VC1 OST Read"),
	SUB_PORT(0x91, "CPI Read"),
	SUB_PORT(0xA0, "Evan Read"),
};

static struct sub_port_list hcodec_sub_w_c5[] __initdata = {
	SUB_PORT(0x0D, "MCRCC Write"),
	SUB_PORT(0x14, "LMEM Write"),
	SUB_PORT(0x1C, "Encoded Jpeg(VLC) Write"),
	SUB_PORT(0x26, "IPRED Write"),
	SUB_PORT(0x28, "MC MBBOT Write"),
};
static struct sub_port_list hcodec_sub_r_c5[] __initdata = {
	SUB_PORT(0x0C, "MCRCC Read"),
	SUB_PORT(0x0D, "DBLK Read"),
	SUB_PORT(0x10, "IMEM Read"),
	SUB_PORT(0x14, "LMEM Read"),
	SUB_PORT(0x26, "IPRED Read"),
	SUB_PORT(0x2C, "MFDIN Read"),
};

SUB_DEV_RO(c5, vpu0, 1, -1);
SUB_DEV_RW(c5, hevc, 9, 9, -1);
SUB_DEV_RW(c5, hcodec, 8, 8, 0);
SUB_DEV_DESC_INIT(c5, &vpu0_sub_c5, &hevc_sub_c5, &hcodec_sub_c5);
#endif

static void __init sub_port_list_tree_free(struct sub_port_list *list, int num)
{
	int i;

	if (!list)
		return;

	for (i = 0; i < num; i++)
		sub_port_list_tree_free(list[i].children, list[i].nr_children);
	kfree(list);
}

static int __init sub_port_list_tree_copy(struct sub_port_list **dst,
					  struct sub_port_list *src, int num)
{
	struct sub_port_list *list;
	int i;

	if (!src || num <= 0) {
		*dst = NULL;
		return 0;
	}

	list = kmemdup(src, num * sizeof(*list), GFP_KERNEL);
	if (!list)
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		if (list[i].children && list[i].nr_children) {
			if (sub_port_list_tree_copy(&list[i].children,
						    src[i].children,
						    list[i].nr_children)) {
				while (--i >= 0)
					sub_port_list_tree_free(list[i].children,
								list[i].nr_children);
				kfree(list);
				return -ENOMEM;
			}
		}
	}

	*dst = list;
	return 0;
}

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
		dst_sub->sub_r.parent_sub_id = src_sub->sub_r.parent_sub_id;
		dst_sub->sub_r.num = src_sub->sub_r.num;
		if (sub_port_list_tree_copy(&dst_sub->sub_r.list, src_sub->sub_r.list,
					    src_sub->sub_r.num)) {
			kfree(dst_sub);
			goto err;
		}

		dst_sub->sub_w.port_id = src_sub->sub_w.port_id;
		dst_sub->sub_w.parent_sub_id = src_sub->sub_w.parent_sub_id;
		dst_sub->sub_w.num = src_sub->sub_w.num;
		if (src_sub->sub_r.list == src_sub->sub_w.list) {
			dst_sub->sub_w.list = dst_sub->sub_r.list;
		} else if (sub_port_list_tree_copy(&dst_sub->sub_w.list,
						   src_sub->sub_w.list,
						   src_sub->sub_w.num)) {
			sub_port_list_tree_free(dst_sub->sub_r.list, dst_sub->sub_r.num);
			kfree(dst_sub);
			goto err;
		}

		desc->subs[i] = dst_sub;
	}

	*dst = desc;
	return 0;

err:
	for (i = 0; i < desc->num; i++) {
		if (desc->subs[i]) {
			sub_port_list_tree_free(desc->subs[i]->sub_w.list,
						desc->subs[i]->sub_w.num);
			if (desc->subs[i]->sub_r.list != desc->subs[i]->sub_w.list)
				sub_port_list_tree_free(desc->subs[i]->sub_r.list,
							desc->subs[i]->sub_r.num);
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

	desc = NULL;
#if IS_ENABLED(CONFIG_AMLOGIC_DMC_MONITOR)
	if (dmc_mon)
		desc = dmc_mon->sub_dev;
#endif
#if IS_ENABLED(CONFIG_AMLOGIC_DDR_BANDWIDTH)
	if (!desc && aml_db)
		desc = aml_db->sub_dev;
#endif
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
