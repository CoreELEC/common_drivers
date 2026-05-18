/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __VPU_SIDEBAND_H__
#define __VPU_SIDEBAND_H__

enum vpu_rw_type_e {
	VPU_RW = 0,
	VPU_RW0 = 0,
	VPU_RW1,
	VPU_WR2,
	VPU_RD2,
	VPU_RD3,
	VPU_RD0,
	VPU_RD1,
	VPU_WR0,
	VPU_WR1
};

enum vpu_dmc_bit_section_type_e {
	SIDEBAND_BLOCK_DEVICE = 0,
	SIDEBAND_RD_EN,
	SIDEBAND_WR_EN,
	SIDEBAND_BLOCK_DEVICE_RD,
	SIDEBAND_BLOCK_DEVICE_WR,
	SIDEBAND_EN,
	SIDEBAND_DEFAULT,
};

enum vpu_arb_port_type_e {
	RD0_ARB_VPP0 = 0,
	RD0_ARB_RDMA,
	RD0_ARB_VPP1,
	RD0_ARB_VPP2,
	RD1_ARB_VPP0,
	RD2_ARB_VPP0,
	RD2_ARB_RDMA,
	RD2_ARB_VPP1,
	RD2_ARB_VPP2,
	RD3_ARB_VPP0,
	RD3_ARB_RDMA,
	RD3_ARB_VPP1,
	RD3_ARB_VPP2,
	WR02_ARB_VDIN0,
	WR02_ARB_VDIN1,
};

struct vpu_sideband_ctrl_s {
	u32 type;
	u32 reg;
	u32 val;
	u32 bit;
	u32 len;
};

extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t7[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t3x[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t6w[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t6x[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_txhd2[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t5w[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_t3[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_s7[];
extern struct vpu_sideband_ctrl_s vpu_arb_urg_ctrl_a9[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t7[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t3x[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t6w[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t6x[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_txhd2[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t5w[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_t3[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_s7[];
extern struct vpu_sideband_ctrl_s vpp_ofifo_urg_ctrl_a9[];

void set_vpu_sideband_init(void);
ssize_t vpu_sideband_show(const struct class *class,
			const struct class_attribute *attr, char *buf);
ssize_t vpu_sideband_store(const struct class *class,
				     const struct class_attribute *attr,
				     const char *buf, size_t count);
ssize_t vpu_sideband_level_show(const struct class *class,
				    const struct class_attribute *attr, char *buf);
ssize_t vpu_sideband_level_store(const struct class *class,
				     const struct class_attribute *attr,
				     const char *buf, size_t count);
ssize_t vpu_sideband_block_device_show(const struct class *class,
				    const struct class_attribute *attr, char *buf);

ssize_t vpu_sideband_block_device_store(const struct class *class,
				     const struct class_attribute *attr,
				     const char *buf, size_t count);
#endif
