// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "vpu_reg.h"
#include "vpu.h"
#include "vpu_arb.h"

struct vpu_arb_table_s *vpu_rdarb_0_2_level1_tables;
struct vpu_arb_table_s *vpu_rdarb_0_2_level2_tables;
struct vpu_arb_table_s *vpu_rdarb_1_tables;
struct vpu_arb_table_s *vpu_rdarb_2_tables;
struct vpu_arb_table_s *vpu_rdarb_3_tables;

struct vpu_arb_table_s *vpu_wrarb_0_tables;
struct vpu_arb_table_s *vpu_wrarb_1_tables;
struct vpu_arb_table_s *vpu_wrarb_2_tables;
struct vpu_urgent_table_s *vpu_urgent_rd_0_2_level2_tbl;
struct vpu_urgent_table_s *vpu_urgent_rd_1_tbl;
struct vpu_urgent_table_s *vpu_urgent_wr_0_tbl;
struct vpu_urgent_table_s *vpu_urgent_wr_1_tbl;
struct vpu_super_urgent_ctl_s *vpu_super_urgent_table;
/*
 *vpu_read/write module info
 *vpu_read0/read2 has two level,level1 module mux to vpp_arb0 and vpp_arb1
 *READ0_2 ARB
 */
static struct vpu_arb_table_s vpu_rdarb0_2_level1_t7[] = {
	/* vpu module,        reg,             bit, len, bind_port,        name */
	{VPU_ARB_OSD1,        VPP_RDARB_MODE,  20,  1,   VPU_ARB_VPP_ARB0,  "osd1",
		/*slv_reg,             bit,        len*/
		VPP_RDARB_REQEN_SLV,   0,          2},
	{VPU_ARB_OSD2,        VPP_RDARB_MODE,  21,  1,   VPU_ARB_VPP_ARB1,  "osd2",
		VPP_RDARB_REQEN_SLV,   2,          2},
	{VPU_ARB_VD1,         VPP_RDARB_MODE,  22,  1,   VPU_ARB_VPP_ARB0,  "vd1",
		VPP_RDARB_REQEN_SLV,   4,          2},
	{VPU_ARB_VD2,         VPP_RDARB_MODE,  23,  1,   VPU_ARB_VPP_ARB1,  "vd2",
		VPP_RDARB_REQEN_SLV,   6,          2},
	{VPU_ARB_OSD3,        VPP_RDARB_MODE,  24,  1,   VPU_ARB_VPP_ARB0,  "osd3",
		VPP_RDARB_REQEN_SLV,   8,          2},
	{VPU_ARB_OSD4,        VPP_RDARB_MODE,  25,  1,   VPU_ARB_VPP_ARB1,  "osd4",
		VPP_RDARB_REQEN_SLV,   10,          2},
	{VPU_ARB_AMDOLBY0,    VPP_RDARB_MODE,  26,  1,   VPU_ARB_VPP_ARB0,  "amdolby0",
		VPP_RDARB_REQEN_SLV,   12,          2},
	{VPU_ARB_MALI_AFBCD,  VPP_RDARB_MODE,  27,  1,   VPU_ARB_VPP_ARB1,  "mali_afbc",
		VPP_RDARB_REQEN_SLV,   14,          2},
	{VPU_ARB_VD3,         VPP_RDARB_MODE,  28,  1,   VPU_ARB_VPP_ARB0,  "vd3",
		VPP_RDARB_REQEN_SLV,   16,          2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb0_2_level1_t5m_rev_a[] = {
		/* vpu module,        reg,             bit, len, bind_port,        name */
	{VPU_ARB_OSD1,        VPP_RDARB_MODE,  20,  1,   VPU_ARB_VPP_ARB1,  "osd1",
		/*slv_reg,             bit,        len*/
		VPP_RDARB_REQEN_SLV,   0,          2},
	{VPU_ARB_OSD2,        VPP_RDARB_MODE,  21,  1,   VPU_ARB_VPP_ARB1,  "osd2",
		VPP_RDARB_REQEN_SLV,   2,          2},
	{VPU_ARB_VD1,         VPP_RDARB_MODE,  22,  1,   VPU_ARB_VPP_ARB0,  "vd1",
		VPP_RDARB_REQEN_SLV,   4,          2},
	{VPU_ARB_VD2,         VPP_RDARB_MODE,  23,  1,   VPU_ARB_VPP_ARB0,  "vd2",
		VPP_RDARB_REQEN_SLV,   6,          2},
	{VPU_ARB_OSD3,        VPP_RDARB_MODE,  24,  1,   VPU_ARB_VPP_ARB1,  "osd3",
		VPP_RDARB_REQEN_SLV,   8,          2},
	{VPU_ARB_OSD4,        VPP_RDARB_MODE,  25,  1,   VPU_ARB_VPP_ARB1,  "osd4",
		VPP_RDARB_REQEN_SLV,   10,          2},
	{VPU_ARB_AMDOLBY0,    VPP_RDARB_MODE,  26,  1,   VPU_ARB_VPP_ARB0,  "amdolby0",
		VPP_RDARB_REQEN_SLV,   12,          2},
	{VPU_ARB_MALI_AFBCD,  VPP_RDARB_MODE,  27,  1,   VPU_ARB_VPP_ARB1,  "mali_afbc",
		VPP_RDARB_REQEN_SLV,   14,          2},
	{VPU_ARB_VD3,         VPP_RDARB_MODE,  28,  1,   VPU_ARB_VPP_ARB0,  "vd3",
		VPP_RDARB_REQEN_SLV,   16,          2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb0_2_level1_t5m_rev_b[] = {
		/* vpu module,        reg,             bit, len, bind_port,        name */
	{VPU_ARB_OSD1,        VPP_RDARB_MODE,  20,  1,   VPU_ARB_VPP_ARB0,  "osd1",
		/*slv_reg,             bit,        len*/
		VPP_RDARB_REQEN_SLV,   0,          2},
	{VPU_ARB_OSD2,        VPP_RDARB_MODE,  21,  1,   VPU_ARB_VPP_ARB0,  "osd2",
		VPP_RDARB_REQEN_SLV,   2,          2},
	{VPU_ARB_VD1,         VPP_RDARB_MODE,  22,  1,   VPU_ARB_VPP_ARB0,  "vd1",
		VPP_RDARB_REQEN_SLV,   4,          2},
	{VPU_ARB_VD2,         VPP_RDARB_MODE,  23,  1,   VPU_ARB_VPP_ARB0,  "vd2",
		VPP_RDARB_REQEN_SLV,   6,          2},
	{VPU_ARB_OSD3,        VPP_RDARB_MODE,  24,  1,   VPU_ARB_VPP_ARB0,  "osd3",
		VPP_RDARB_REQEN_SLV,   8,          2},
	{VPU_ARB_OSD4,        VPP_RDARB_MODE,  25,  1,   VPU_ARB_VPP_ARB0,  "osd4",
		VPP_RDARB_REQEN_SLV,   10,          2},
	{VPU_ARB_AMDOLBY0,    VPP_RDARB_MODE,  26,  1,   VPU_ARB_VPP_ARB0,  "amdolby0",
		VPP_RDARB_REQEN_SLV,   12,          2},
	{VPU_ARB_MALI_AFBCD,  VPP_RDARB_MODE,  27,  1,   VPU_ARB_VPP_ARB0,  "mali_afbc",
		VPP_RDARB_REQEN_SLV,   14,          2},
	{VPU_ARB_VD3,         VPP_RDARB_MODE,  28,  1,   VPU_ARB_VPP_ARB0,  "vd3",
		VPP_RDARB_REQEN_SLV,   16,          2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb_0_2_level1_txhd2[] = {
	/* vpu module,        reg,             bit, len, bind_port,        name */
	{VPU_ARB_OSD1,        VPP_RDARB_MODE,  20,  1,   VPU_ARB_VPP_ARB0,  "osd1",
		/*slv_reg,             bit,        len*/
		VPP_RDARB_REQEN_SLV,   0,          2},
	{VPU_ARB_OSD2,        VPP_RDARB_MODE,  21,  1,   VPU_ARB_VPP_ARB1,  "osd2",
		VPP_RDARB_REQEN_SLV,   2,          2},
	{VPU_ARB_VD1_RDMIF,   VPP_RDARB_MODE,  22,  1,   VPU_ARB_VPP_ARB0,  "vd1_rdmif",
		VPP_RDARB_REQEN_SLV,   4,          2},
	{VPU_ARB_VD1_AFBCD,   VPP_RDARB_MODE,  23,  1,   VPU_ARB_VPP_ARB1,  "vd1_afbcd",
		VPP_RDARB_REQEN_SLV,   6,          2},
	{VPU_ARB_OSD3,        VPP_RDARB_MODE,  24,  1,   VPU_ARB_VPP_ARB0,  "osd3",
		VPP_RDARB_REQEN_SLV,   8,          2},
	{VPU_ARB_AMDOLBY0,    VPP_RDARB_MODE,  26,  1,   VPU_ARB_VPP_ARB0,  "amdolby0",
		VPP_RDARB_REQEN_SLV,   12,          2},
	{VPU_ARB_MALI_AFBCD,  VPP_RDARB_MODE,  27,  1,   VPU_ARB_VPP_ARB1,  "mali_afbc",
		VPP_RDARB_REQEN_SLV,   14,          2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb0_2_level2_t7[] = {
	/* vpu module,          reg,                  bit, len, bind_port,  name */
	{VPU_ARB_VPP_ARB0,      VPU_RDARB_MODE_L2C1,  16,  1,   VPU_READ0,  "vpp_arb0",
		/*slv_reg,                 bit,        len*/
		VPP_RDARB_REQEN_SLV_L2C1,   0,          2},
	{VPU_ARB_VPP_ARB1,      VPU_RDARB_MODE_L2C1,  17,  1,   VPU_READ0,  "vpp_arb1",
		VPP_RDARB_REQEN_SLV_L2C1,   2,          2},
	{VPU_ARB_RDMA_READ,     VPU_RDARB_MODE_L2C1,  18,  1,   VPU_READ0,  "rdma_read",
		VPP_RDARB_REQEN_SLV_L2C1,   4,          2},
	{VPU_ARB_LDIM_RD,       VPU_RDARB_MODE_L2C1,  23,  1,   VPU_READ2,  "ldim_rd",
		VPP_RDARB_REQEN_SLV_L2C1,   14,          2},
	{VPU_ARB_VDIN_AFBCE_RD, VPU_RDARB_MODE_L2C1,  24,  1,   VPU_READ0,  "vdin_afbce_rd",
		VPP_RDARB_REQEN_SLV_L2C1,   16,          2},
	{VPU_ARB_VPU_DMA,       VPU_RDARB_MODE_L2C1,  25,  1,   VPU_READ0,  "vpu_dma",
		VPP_RDARB_REQEN_SLV_L2C1,   18,          2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb0_2_level2_t5m_rev_a[] = {
	/* vpu module,			reg,			bit, len,   bind_port, name */
	{VPU_ARB_VPP_ARB0,      VPU_RDARB_MODE_L2C1, 16, 1, VPU_READ0, "vpp_arb0",
		/*slv_reg,                 bit,       len*/
		VPP_RDARB_REQEN_SLV_L2C1,	0,			2},
	{VPU_ARB_VPP_ARB1,      VPU_RDARB_MODE_L2C1, 17, 1, VPU_READ2, "vpp_arb1",
		VPP_RDARB_REQEN_SLV_L2C1,	2,			2},
	{VPU_ARB_RDMA_READ,     VPU_RDARB_MODE_L2C1, 18, 1, VPU_READ0, "rdma_read",
		VPP_RDARB_REQEN_SLV_L2C1,	4,			2},
	{VPU_ARB_VIU2,          VPU_RDARB_MODE_L2C1, 19, 1, VPU_READ0, "viu2",
		VPP_RDARB_REQEN_SLV_L2C1,	6,			2},
	{VPU_ARB_TCON_P1,       VPU_RDARB_MODE_L2C1, 20, 1, VPU_READ0, "tcon_p1",
		VPP_RDARB_REQEN_SLV_L2C1,	8,			2},
	{VPU_ARB_TVFE_READ,     VPU_RDARB_MODE_L2C1, 21, 1, VPU_READ0, "tvfe_read",
		VPP_RDARB_REQEN_SLV_L2C1,	10,			2},
	{VPU_ARB_TCON_P2,       VPU_RDARB_MODE_L2C1, 22, 1, VPU_READ0, "tcon_p2",
		VPP_RDARB_REQEN_SLV_L2C1,	12,			2},
	{VPU_ARB_LDIM_RD,       VPU_RDARB_MODE_L2C1, 23, 1, VPU_READ0, "ldim_rd",
		VPP_RDARB_REQEN_SLV_L2C1,	14,			2},
	{VPU_ARB_VDIN_AFBCE_RD, VPU_RDARB_MODE_L2C1, 24, 1, VPU_READ0, "vdin_afbce_rd",
		VPP_RDARB_REQEN_SLV_L2C1,	16,			2},
	{VPU_ARB_VPU_DMA,       VPU_RDARB_MODE_L2C1, 25, 1, VPU_READ0, "vpu_dma",
		VPP_RDARB_REQEN_SLV_L2C1,	18,			2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb0_2_level2_t5m_rev_b[] = {
	/* vpu module,			reg,			bit, len,   bind_port, name */
	{VPU_ARB_VPP_ARB0,      VPU_RDARB_MODE_L2C1, 16, 1, VPU_READ0, "vpp_arb0",
		/*slv_reg,                 bit,       len*/
		VPP_RDARB_REQEN_SLV_L2C1,	0,			2},
	{VPU_ARB_VPP_ARB1,      VPU_RDARB_MODE_L2C1, 17, 1, VPU_READ0, "vpp_arb1",
		VPP_RDARB_REQEN_SLV_L2C1,	2,			2},
	{VPU_ARB_RDMA_READ,     VPU_RDARB_MODE_L2C1, 18, 1, VPU_READ0, "rdma_read",
		VPP_RDARB_REQEN_SLV_L2C1,	4,			2},
	{VPU_ARB_VIU2,          VPU_RDARB_MODE_L2C1, 19, 1, VPU_READ0, "viu2",
		VPP_RDARB_REQEN_SLV_L2C1,	6,			2},
	{VPU_ARB_TCON_P1,       VPU_RDARB_MODE_L2C1, 20, 1, VPU_READ0, "tcon_p1",
		VPP_RDARB_REQEN_SLV_L2C1,	8,			2},
	{VPU_ARB_TVFE_READ,     VPU_RDARB_MODE_L2C1, 21, 1, VPU_READ0, "tvfe_read",
		VPP_RDARB_REQEN_SLV_L2C1,	10,			2},
	{VPU_ARB_TCON_P2,       VPU_RDARB_MODE_L2C1, 22, 1, VPU_READ0, "tcon_p2",
		VPP_RDARB_REQEN_SLV_L2C1,	12,			2},
	{VPU_ARB_LDIM_RD,       VPU_RDARB_MODE_L2C1, 23, 1, VPU_READ0, "ldim_rd",
		VPP_RDARB_REQEN_SLV_L2C1,	14,			2},
	{VPU_ARB_VDIN_AFBCE_RD, VPU_RDARB_MODE_L2C1, 24, 1, VPU_READ0, "vdin_afbce_rd",
		VPP_RDARB_REQEN_SLV_L2C1,	16,			2},
	{VPU_ARB_VPU_DMA,       VPU_RDARB_MODE_L2C1, 25, 1, VPU_READ0, "vpu_dma",
		VPP_RDARB_REQEN_SLV_L2C1,	18,			2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb_0_2_level2_txhd2[] = {
	/* vpu module,          reg,                  bit, len, bind_port,  name */
	{VPU_ARB_VPP_ARB0,      VPU_RDARB_MODE_L2C1,  16,  1,   VPU_READ0,  "vpp_arb0",
		/*slv_reg,                 bit,        len*/
		VPP_RDARB_REQEN_SLV_L2C1,   0,          2},
	{VPU_ARB_VPP_ARB1,      VPU_RDARB_MODE_L2C1,  17,  1,   VPU_READ0,  "vpp_arb1",
		VPP_RDARB_REQEN_SLV_L2C1,   2,          2},
	{VPU_ARB_RDMA_READ,     VPU_RDARB_MODE_L2C1,  18,  1,   VPU_READ0,  "rdma_read",
		VPP_RDARB_REQEN_SLV_L2C1,   4,          2},
	{VPU_ARB_TCON_P1,       VPU_RDARB_MODE_L2C1,  20,  1,   VPU_READ0,  "tcon_p1",
		VPP_RDARB_REQEN_SLV_L2C1,   8,          2},
	{VPU_ARB_TVFE_READ,     VPU_RDARB_MODE_L2C1,  21,  1,   VPU_READ0,  "tvfe",
		VPP_RDARB_REQEN_SLV_L2C1,   10,          2},
	{VPU_ARB_TCON_P2,       VPU_RDARB_MODE_L2C1,  22,  1,   VPU_READ0,  "tcon_p2",
		VPP_RDARB_REQEN_SLV_L2C1,   12,          2},
		{}
};

/*
 *UNBINDABLE READ0/2
 */
static struct vpu_arb_table_s vpu_rdarb_0_level1_t6w[] = {
	/* vpu module,        reg,             bit, len, bind_port,         name */
	{VPU_ARB_OSD1,		  VPP_RDARB_MODE,  20,	1,	 VPU_ARB_VPP_ARB0,	"osd1",
	/*slv_reg,                 bit,        len*/
		VPP_RDARB_REQEN_SLV,   0,		   2},
	{VPU_ARB_OSD2,		  VPP_RDARB_MODE,  21,	1,	 VPU_ARB_VPP_ARB1,	"osd2",
		VPP_RDARB_REQEN_SLV,   2,		   2},
	{VPU_ARB_MC_SUB2,     VPP_RDARB_MODE,  22,	1,	 VPU_ARB_VPP_ARB0,	"mc_sub2",
		VPP_RDARB_REQEN_SLV,   4,		   2},
	{VPU_ARB_MC_SUB3,     VPP_RDARB_MODE,  23,	1,	 VPU_ARB_VPP_ARB1,	"mc_sub3",
		VPP_RDARB_REQEN_SLV,   6,		   2},
	{VPU_ARB_OSD3,		  VPP_RDARB_MODE,  24,	1,	 VPU_ARB_VPP_ARB0,	"osd3",
		VPP_RDARB_REQEN_SLV,   8,		   2},
	{VPU_ARB_AMDOLBY0,    VPP_RDARB_MODE,  26,	1,	 VPU_ARB_VPP_ARB0,	"amdolby0",
		VPP_RDARB_REQEN_SLV,   12,		   2},
	{VPU_ARB_MALI_AFBCD,  VPP_RDARB_MODE,  27,	1,	 VPU_ARB_VPP_ARB1,	"mali_afbc",
		VPP_RDARB_REQEN_SLV,   14,			2},
		{}
};

static struct vpu_arb_table_s vpu_rdarb_0_level2_t6w[] = {
	/* vpu module,        reg,				  bit, len, bind_port,	name */
	{VPU_ARB_VPP_ARB0,	  VPU_RDARB_MODE_L2C1,  16,  1,	VPU_READ0,	"vpp_arb0",
	/*slv_reg,                      bit,        len*/
		VPP_RDARB_REQEN_SLV_L2C1,	0,			2},
	{VPU_ARB_VPP_ARB1,	  VPU_RDARB_MODE_L2C1,  17,  1,	VPU_READ0,	"vpp_arb1",
		VPP_RDARB_REQEN_SLV_L2C1,	2,			2},
	{VPU_ARB_RDMA_READ,   VPU_RDARB_MODE_L2C1,  18,  1, VPU_READ0,  "rdma_read",
		VPP_RDARB_REQEN_SLV_L2C1,   4,          2},
	{VPU_ARB_LDIM_RD,     VPU_RDARB_MODE_L2C1,  19,  1, VPU_READ0,  "ldim_rd",
		VPP_RDARB_REQEN_SLV_L2C1,   6,          2},
	{VPU_ARB_TCON_P1,     VPU_RDARB_MODE_L2C1,  20,  1, VPU_READ0,  "tcon_p1",
		VPP_RDARB_REQEN_SLV_L2C1,   8,          2},
	{VPU_ARB_AMDOLBY0,	  VPU_RDARB_MODE_L2C1,  21,  1, VPU_READ0,  "amdolby",
		VPP_RDARB_REQEN_SLV_L2C1,   10,			2},
	{VPU_ARB_TCON_P2,	  VPU_RDARB_MODE_L2C1,  22,  1, VPU_READ0,  "tcon_p2",
		VPP_RDARB_REQEN_SLV_L2C1,   12,			2},
	{VPU_ARB_LUT_DMA,	  VPU_RDARB_MODE_L2C1,  23,  1, VPU_READ0,  "lut_dma",
		VPP_RDARB_REQEN_SLV_L2C1,   14,			2},
	{VPU_ARB_MC_SUB,	  VPU_RDARB_MODE_L2C1,  24,  1, VPU_READ0,  "mc_sub",
		VPP_RDARB_REQEN_SLV_L2C1,   16,			2},
		{}
};

/*
 *READ1 ARB
 */
static struct vpu_arb_table_s vpu_rdarb_1_t7[] = {
	/* vpu module,          reg,                    bit, len, bind_port,  name */
	{VPU_ARB_DI_IF1,        DI_RDARB_MODE_L1C1,     16,  1,   VPU_READ1,  "di_if1",
		/*slv_reg,                 bit,        len*/
		DI_RDARB_REQEN_SLV_L1C1,   0,          1},
	{VPU_ARB_DI_MEM,        DI_RDARB_MODE_L1C1,     17,  1,   VPU_READ1,  "di_mem",
		DI_RDARB_REQEN_SLV_L1C1,   1,          1},
	{VPU_ARB_DI_INP,        DI_RDARB_MODE_L1C1,     18,  1,   VPU_READ1,  "di_inp",
		DI_RDARB_REQEN_SLV_L1C1,   2,          1},
	{VPU_ARB_DI_CHAN2,      DI_RDARB_MODE_L1C1,     19,  1,   VPU_READ1,  "di_chan2",
		DI_RDARB_REQEN_SLV_L1C1,   3,          1},
	{VPU_ARB_DI_SUBAXI,     DI_RDARB_MODE_L1C1,     20,  1,   VPU_READ1,  "di_subaxi",
		DI_RDARB_REQEN_SLV_L1C1,   4,          1},
	{VPU_ARB_DI_IF2,        DI_RDARB_MODE_L1C1,     21,  1,   VPU_READ1,  "di_if2",
		DI_RDARB_REQEN_SLV_L1C1,   5,          1},
	{VPU_ARB_DI_IF0,        DI_RDARB_MODE_L1C1,     22,  1,   VPU_READ1,  "di_if0",
		DI_RDARB_REQEN_SLV_L1C1,   6,          1},
	{VPU_ARB_DI_AFBCD,      DI_RDARB_MODE_L1C1,     23,  1,   VPU_READ1,  "di_afbcd",
		DI_RDARB_REQEN_SLV_L1C1,   7,          1},
		{}
};

static struct vpu_arb_table_s vpu_rdarb_1_txhd2[] = {
	/* vpu module,          reg,                    bit, len, bind_port,  name */
	{VPU_ARB_DI_IF1,        DI_RDARB_MODE_L1C1,     16,  1,   VPU_READ1,  "di_if1",
		/*slv_reg,                 bit,        len*/
		DI_RDARB_REQEN_SLV_L1C1,   0,          1},
	{VPU_ARB_DI_MEM,        DI_RDARB_MODE_L1C1,     17,  1,   VPU_READ1,  "di_mem",
		DI_RDARB_REQEN_SLV_L1C1,   1,          1},
	{VPU_ARB_DI_INP,        DI_RDARB_MODE_L1C1,     18,  1,   VPU_READ1,  "di_inp",
		DI_RDARB_REQEN_SLV_L1C1,   2,          1},
	{VPU_ARB_DI_CHAN2,      DI_RDARB_MODE_L1C1,     19,  1,   VPU_READ1,  "di_chan2",
		DI_RDARB_REQEN_SLV_L1C1,   3,          1},
	{VPU_ARB_DI_SUBAXI,     DI_RDARB_MODE_L1C1,     20,  1,   VPU_READ1,  "di_subaxi",
		DI_RDARB_REQEN_SLV_L1C1,   4,          1},
	{VPU_ARB_DI_IF2,        DI_RDARB_MODE_L1C1,     21,  1,   VPU_READ1,  "di_if2",
		DI_RDARB_REQEN_SLV_L1C1,   5,          1},
	{VPU_ARB_DI_IF0,        DI_RDARB_MODE_L1C1,     22,  1,   VPU_READ1,  "di_if0",
		DI_RDARB_REQEN_SLV_L1C1,   6,          1},
		{}
};

static struct vpu_arb_table_s vpu_rdarb_1_t6w[] = {
	/* vpu module,        reg,                    bit, len, bind_port,  name */
	{VPU_ARB_DAE_RD,      VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_READ1,  "dae_rd0-11",
		/*slv_reg,                 bit,        len*/
		VPU_ARB_UNBINDABLE_REG,   0,           0},
	{VPU_ARB_INP_RD,      VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_READ1,  "inp_rd0-1",
		VPU_ARB_UNBINDABLE_REG,   0,           0},
	{VPU_ARB_RDMA_RD,     VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_READ1,  "rdma_rd",
		VPU_ARB_UNBINDABLE_REG,   0,           0},
		{}
};

/*
 *READ2 ARB
 */

static struct vpu_arb_table_s vpu_rdarb_2_t6w[] = {
	/* vpu module,        reg,                    bit, len, bind_port,  name */
	{VPU_ARB_NR_VFCD_RD,  VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_READ2,  "nr_vfcd0-3",
		/*slv_reg,                 bit,        len*/
		VPU_ARB_UNBINDABLE_REG,   0,           0},
		{}
};

static struct vpu_arb_table_s vpu_rdarb_2_t6x[] = {
	/* vpu module,        reg,                    bit, len, bind_port,  name */
	{VPU_ARB_NR_VFCD_RD,  VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_READ2,  "nr_vfcd0-3",
		/*slv_reg,                 bit,        len*/
		VPU_ARB_UNBINDABLE_REG,   0,           0},
	{VPU_ARB_DAE_RD,	  VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_READ2,  "dae_rd0-11",
		VPU_ARB_UNBINDABLE_REG,   0,           0},
	{VPU_ARB_INP_RD,	  VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_READ2,  "inp_rd0-1",
		VPU_ARB_UNBINDABLE_REG,   0,           0},
	{VPU_ARB_RDMA_RD,	  VPU_ARB_UNBINDABLE_REG,  0,  0,	VPU_READ2,	"rdma_rd",
		VPU_ARB_UNBINDABLE_REG,   0,		   0},
		{}
};

/*
 *READ3 ARB
 */

static struct vpu_arb_table_s vpu_rdarb_3_t6w[] = {
	/* vpu module,        reg,                  bit,len,bind_port, name */
	{VPU_ARB_MC_VFCD_RD, VPU_ARB_UNBINDABLE_REG, 0, 0, VPU_READ3, "mc_vfcd0-2(vd mif/afbc/rc)",
		/*slv_reg,                 bit,        len*/
		VPU_ARB_UNBINDABLE_REG,   0,           0},
	{VPU_ARB_MMU_RD,      VPU_ARB_UNBINDABLE_REG,  0,  0, VPU_READ3,  "mmu",
		VPU_ARB_UNBINDABLE_REG,   0,		   0},
		{}
};

/*
 *WRITE0 ARB
 */
static struct vpu_arb_table_s vpu_wrarb_0_t7[] = {
	/* vpu module,          reg,                    bit, len, bind_port,  name */
	{VPU_ARB_VDIN_WR,       VPU_WRARB_MODE_L2C1,    16,  1,   NO_PORT,    "vdin_wr",
		/*slv_reg,                 bit,        len*/
		VPU_WRARB_REQEN_SLV_L2C1,   0,          1},
	{VPU_ARB_RDMA_WR,       VPU_WRARB_MODE_L2C1,    17,  1,   VPU_WRITE0, "rdma_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   1,          1},
	{VPU_ARB_TVFE_WR,       VPU_WRARB_MODE_L2C1,    18,  1,   NO_PORT,    "tvfe_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   2,          1},
	{VPU_ARB_TCON1_WR,      VPU_WRARB_MODE_L2C1,    19,  1,   NO_PORT,    "tcon1_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   3,          1},
	{VPU_ARB_TCON2_WR,      VPU_WRARB_MODE_L2C1,    20,  1,   NO_PORT,    "tcon2_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   4,          1},
	{VPU_ARB_TCON3_WR,      VPU_WRARB_MODE_L2C1,    21,  1,   NO_PORT,    "tcon3_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   5,          1},
	{VPU_ARB_VPU_DMA_WR,    VPU_WRARB_MODE_L2C1,    22,  1,   NO_PORT,    "vpu_dma_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   6,          1},
		{}
};

static struct vpu_arb_table_s vpu_wrarb_0_txhd2[] = {
	/* vpu module,          reg,                    bit, len, bind_port,  name */
	{VPU_ARB_VDIN_WR,       VPU_WRARB_MODE_L2C1,    16,  1,   NO_PORT,    "vdin_wr",
		/*slv_reg,                 bit,        len*/
		VPU_WRARB_REQEN_SLV_L2C1,   0,          1},
	{VPU_ARB_RDMA_WR,       VPU_WRARB_MODE_L2C1,    17,  1,   VPU_WRITE0, "rdma_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   1,          1},
	{VPU_ARB_TVFE_WR,       VPU_WRARB_MODE_L2C1,    18,  1,   NO_PORT,    "tvfe_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   2,          1},
	{VPU_ARB_TCON1_WR,      VPU_WRARB_MODE_L2C1,    19,  1,   NO_PORT,    "tcon1_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   3,          1},
	/*
	 *{VPU_ARB_DI_AXI1_WR,	VPU_WRARB_MODE_L2C1,	20,  1,  NO_PORT,	  "tcon1_wr",
	 *		VPU_WRARB_REQEN_SLV_L2C1,	4,			1},
	 */
		{}
};

static struct vpu_arb_table_s vpu_wrarb_vpu0_t6w[] = {
	/* vpu module,          reg,                 bit, len, bind_port, name */
	{VPU_ARB_VDIN_WR,       VPU_WRARB_MODE_L2C1,  16, 1,  NO_PORT,   "vdin_wr",
		/*slv_reg,                 bit,        len*/
		VPU_WRARB_REQEN_SLV_L2C1,   0,          1},
	{VPU_ARB_RDMA_WR,       VPU_WRARB_MODE_L2C1,  17, 1, VPU_WRITE0, "rdma_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   1,          1},
	{VPU_ARB_AMDOLBY_WR,    VPU_WRARB_MODE_L2C1,  18, 1,  NO_PORT,   "tvfe_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   2,          1},
	{VPU_ARB_TCON1_WR,      VPU_WRARB_MODE_L2C1,  19, 1,  NO_PORT,   "tcon1_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   3,          1},
	{VPU_ARB_TCON2_WR,      VPU_WRARB_MODE_L2C1,  20, 1,  NO_PORT,   "tcon2_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   4,          1},
	{VPU_ARB_LDIM_WR,       VPU_WRARB_MODE_L2C1,  21, 1,  NO_PORT,   "ldim_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   5,          1},
	{VPU_ARB_VPU_DMA_WR,    VPU_WRARB_MODE_L2C1,  22, 1,  NO_PORT,   "vpu_dma_wr",
		VPU_WRARB_REQEN_SLV_L2C1,   6,          1},
		{}
};

/*
 *WRITE1 ARB
 */
static struct vpu_arb_table_s vpu_wrarb_1_t7[] = {
	/* vpu module,          reg,                    bit, len, bind_port,  name */
	{VPU_ARB_NR_WR,         DI_WRARB_MODE_L1C1,     16,  1,   VPU_WRITE1,    "nr_wr",
			/*slv_reg,             bit,        len*/
		DI_WRARB_REQEN_SLV_L1C1,   0,          1},
	{VPU_ARB_DI_WR,         DI_WRARB_MODE_L1C1,     17,  1,   VPU_WRITE1,    "di_wr",
		DI_WRARB_REQEN_SLV_L1C1,   1,          1},
	{VPU_ARB_DI_SUBAXI_WR,  DI_WRARB_MODE_L1C1,     18,  1,   VPU_WRITE1,    "di_subaxi_wr",
		DI_WRARB_REQEN_SLV_L1C1,   2,          1},
	{VPU_ARB_DI_AFBCE0,     DI_WRARB_MODE_L1C1,     19,  1,   VPU_WRITE1,    "di_afbce0",
		DI_WRARB_REQEN_SLV_L1C1,   3,          1},
	{VPU_ARB_DI_AFBCE1,     DI_WRARB_MODE_L1C1,     20,  1,   VPU_WRITE1,    "di_afbce1",
		DI_WRARB_REQEN_SLV_L1C1,   4,          1},
		{}
};

static struct vpu_arb_table_s vpu_wrarb_1_txhd2[] = {
	/* vpu module,          reg,                    bit, len, bind_port,  name */
	{VPU_ARB_NR_WR,         DI_WRARB_MODE_L1C1,     16,  1,   VPU_WRITE1,    "nr_wr",
			/*slv_reg,             bit,        len*/
		DI_WRARB_REQEN_SLV_L1C1,   0,          1},
	{VPU_ARB_DI_WR,         DI_WRARB_MODE_L1C1,     17,  1,   VPU_WRITE1,    "di_wr",
		DI_WRARB_REQEN_SLV_L1C1,   1,          1},
	{VPU_ARB_DI_SUBAXI_WR,  DI_WRARB_MODE_L1C1,     18,  1,   VPU_WRITE1,    "di_subaxi_wr",
		DI_WRARB_REQEN_SLV_L1C1,   2,          1},
		{}
};

static struct vpu_arb_table_s vpu_wrarb_1_t6w[] = {
	/* vpu module,        reg,                    bit, len, bind_port,  name */
	{VPU_ARB_DAE_WR,  VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_WRITE1,  "dae0-8/inp_wr/rdma_wr",
		/*slv_reg,                 bit,        len*/
		VPU_ARB_UNBINDABLE_REG,   0,           0},
		{}
};

/*
 *WRITE2 ARB
 */
static struct vpu_arb_table_s vpu_wrarb_2_t6w[] = {
	/* vpu module,        reg,                    bit, len, bind_port,  name */
	{VPU_ARB_VFCE_WR,  VPU_ARB_UNBINDABLE_REG,  0,  0,   VPU_WRITE2,  "vfce/wrmif/ds_wrmif...",
		/*slv_reg,                 bit,        len*/
		VPU_ARB_UNBINDABLE_REG,   0,           0},
		{}
};

static struct vpu_arb_table_s vpu_wrarb_2_t6x[] = {
	/* vpu module,        reg,                    bit, len, bind_port,  name */
	{VPU_ARB_VFCE_WR, VPU_ARB_UNBINDABLE_REG, 0,  0, VPU_WRITE2,  "vfce/wrmif/dae_wr/rdma...",
		/*slv_reg,                 bit,        len*/
		VPU_ARB_UNBINDABLE_REG,   0,           0},
};

/*
 *vpu_read/write urgent info
 *vpu_read0/read2 urgent by setting level2 module
 *READ0_2 URGENT
 */
static struct vpu_urgent_table_s vpu_urgent_rd_0_2_level2_t7[] = {
	/*vpu module,           reg,                port,     val, start, len, name*/
	{VPU_ARB_VPP_ARB0,      VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   0,    2,  "vpp_arb0"},
	{VPU_ARB_VPP_ARB1,      VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   2,    2,  "vpp_arb1"},
	{VPU_ARB_RDMA_READ,     VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   4,    2,  "rdma_read"},
	{VPU_ARB_VIU2,          VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   6,    2,  "viu2(no use)"},
	{VPU_ARB_TCON_P1,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   8,    2,  "tcon1(no use)"},
	{VPU_ARB_TVFE_READ,     VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   10,   2,  "tvfe_read(no use)"},
	{VPU_ARB_TCON_P2,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   12,   2,  "tcon_p2(no use)"},
	{VPU_ARB_LDIM_RD,       VPU_RDARB_UGT_L2C1, VPU_READ2, 3,   14,   2,  "ldim_rd"},
	{VPU_ARB_VDIN_AFBCE_RD, VPU_RDARB_UGT_L2C1, VPU_READ0, 0,   16,   2,  "vdin_afbce_rd"},
	{VPU_ARB_VPU_DMA,       VPU_RDARB_UGT_L2C1, VPU_READ0, 0,   18,   2,  "vpu_dma"},
		{}
};

static struct vpu_urgent_table_s vpu_urgent_table_rd_0_2_level2_txhd2[] = {
	/*vpu module,           reg,                port,     val, start, len, name*/
	{VPU_ARB_VPP_ARB0,      VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   0,    2,  "vpp_arb0"},
	{VPU_ARB_VPP_ARB1,      VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   2,    2,  "vpp_arb1"},
	{VPU_ARB_RDMA_READ,     VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   4,    2,  "rdma_read"},
	{VPU_ARB_VIU2,          VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   6,    2,  "viu2(no use)"},
	{VPU_ARB_TCON_P1,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   8,    2,  "tcon1"},
	{VPU_ARB_TVFE_READ,     VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   10,   2,  "tvfe_read"},
	{VPU_ARB_TCON_P2,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   12,   2,  "tcon_p2"},
	{VPU_ARB_LDIM_RD,       VPU_RDARB_UGT_L2C1, VPU_READ2, 3,   14,   2,  "ldim_rd(no use)"},
	{VPU_ARB_VDIN_AFBCE_RD, VPU_RDARB_UGT_L2C1, VPU_READ0, 0,   16,   2,  "vdin_afbce(no use)"},
	{VPU_ARB_VPU_DMA,       VPU_RDARB_UGT_L2C1, VPU_READ0, 0,   18,   2,  "vpu_dma(no use)"},
		{}
};

static struct vpu_urgent_table_s vpu_urgent_table_rd_vpu0_t6w[] = {
	/*vpu module,           reg,                port,     val, start, len, name*/
	{VPU_ARB_VPP_ARB0,      VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   0,    2,  "vpp_arb0"},
	{VPU_ARB_VPP_ARB1,      VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   2,    2,  "vpp_arb1"},
	{VPU_ARB_RDMA_READ,     VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   4,    2,  "rdma_read"},
	{VPU_ARB_LDIM_RD,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   6,    2,  "ldim read"},
	{VPU_ARB_TCON_P1,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   8,    2,  "tcon_p1"},
	{VPU_ARB_AMDOLBY,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   10,   2,  "amdolby0"},
	{VPU_ARB_TCON_P2,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   12,   2,  "tcon_p2"},
	{VPU_ARB_VPU_DMA,       VPU_RDARB_UGT_L2C1, VPU_READ0, 3,   14,   2,  "vpu dma"},
		{}
};

/*
 *READ1 URGENT
 */
static struct vpu_urgent_table_s vpu_urgent_table_rd_1_t7[] = {
	/*vpu module,           reg,                 port,       val, start, len,  name*/
	{VPU_ARB_DI_IF1,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   0,     2,    "di_if1"},
	{VPU_ARB_DI_MEM,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   2,     2,    "di_mem"},
	{VPU_ARB_DI_INP,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   4,     2,    "di_inp"},
	{VPU_ARB_DI_CHAN2,      DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   6,     2,    "di_chan2"},
	{VPU_ARB_DI_SUBAXI,     DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   8,     2,    "di_subaxi"},
	{VPU_ARB_DI_IF2,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   10,    2,    "di_if2"},
	{VPU_ARB_DI_IF0,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   12,    2,    "di_if0"},
	{VPU_ARB_DI_AFBCD,      DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   14,    2,    "di_afbcd"},
		{}
};

static struct vpu_urgent_table_s vpu_urgent_table_rd_1_txhd2[] = {
	/*vpu module,           reg,                 port,       val, start, len,  name*/
	{VPU_ARB_DI_IF1,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   0,     2,    "di_if1"},
	{VPU_ARB_DI_MEM,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   2,     2,    "di_mem"},
	{VPU_ARB_DI_INP,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   4,     2,    "di_inp"},
	{VPU_ARB_DI_CHAN2,      DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   6,     2,    "di_chan2"},
	{VPU_ARB_DI_SUBAXI,     DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   8,     2,    "di_subaxi"},
	{VPU_ARB_DI_IF2,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   10,    2,    "di_if2"},
	{VPU_ARB_DI_IF0,        DI_RDARB_UGT_L1C1,   VPU_READ1,  1,   12,    2,    "di_if0"},
		{}
};

/*
 *WRITE0 URGENT
 */
static struct vpu_urgent_table_s vpu_urgent_table_wr_0_t7[] = {
	/*vpu module,           reg,                 port,       val, start, len,  name*/
	{VPU_ARB_VDIN_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   0,    2,    "vdin_wr"},
	{VPU_ARB_RDMA_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   2,    2,    "rdma_wr"},
	{VPU_ARB_TVFE_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   4,    2,    "tvfe_wr"},
	{VPU_ARB_TCON1_WR,      VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   6,    2,    "tcon1_wr"},
	{VPU_ARB_TCON2_WR,      VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   8,    2,    "tcon2_wr"},
	{VPU_ARB_TCON3_WR,      VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   10,   2,    "tcon3_wr"},
	{VPU_ARB_VPU_DMA_WR,    VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   12,   2,    "vpu_dma_wr"},
		{}
};

static struct vpu_urgent_table_s vpu_urgent_table_wr_0_txhd2[] = {
	/*vpu module,           reg,                 port,       val, start, len,  name*/
	{VPU_ARB_VDIN_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   0,    2,    "vdin_wr"},
	{VPU_ARB_RDMA_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   2,    2,    "rdma_wr"},
	{VPU_ARB_TVFE_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   4,    2,    "tvfe_wr"},
	{VPU_ARB_TCON1_WR,      VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   6,    2,    "tcon1_wr"},
	{VPU_ARB_DI_AXI1_WR,    VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   8,    2,    "di_axi1_wr"},
		{}
};

static struct vpu_urgent_table_s vpu_urgent_table_wr_vpu0_t6w[] = {
	/*vpu module,           reg,                 port,       val, start, len,  name*/
	{VPU_ARB_VDIN_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  3,   0,    2,    "vdin_wr"},
	{VPU_ARB_RDMA_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   2,    2,    "rdma_wr"},
	{VPU_ARB_AMDOLBY_WR,    VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   4,    2,    "amdolby_wr"},
	{VPU_ARB_TCON1_WR,      VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   6,    2,    "tcon1_wr"},
	{VPU_ARB_TCON2_WR,      VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   8,    2,    "tcon2_wr"},
	{VPU_ARB_LDIM_WR,       VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   10,   2,    "ldim_wr"},
	{VPU_ARB_VPU_DMA_WR,    VPU_WRARB_UGT_L2C1,  VPU_WRITE0,  0,   12,   2,    "vpu_dma_wr"},
		{}
};

/*
 *WRITE1 URGENT
 */
static struct vpu_urgent_table_s vpu_urgent_table_wr_1_t7[] = {
	/*vpu module,           reg,                 port,       val, start, len,  name*/
	{VPU_ARB_NR_WR,         DI_WRARB_UGT_L1C1,  VPU_WRITE0,  1,   0,    2,    "nr_wr"},
	{VPU_ARB_DI_WR,         DI_WRARB_UGT_L1C1,  VPU_WRITE0,  1,   2,    2,    "di_wr"},
	{VPU_ARB_DI_SUBAXI_WR,  DI_WRARB_UGT_L1C1,  VPU_WRITE0,  1,   4,    2,    "di_subaxi_wr"},
	{VPU_ARB_DI_AFBCE0,     DI_WRARB_UGT_L1C1,  VPU_WRITE0,  1,   6,    2,    "di_afbce0"},
	{VPU_ARB_DI_AFBCE1,     DI_WRARB_UGT_L1C1,  VPU_WRITE0,  1,   8,    2,    "di_afbce1"},
		{}
};

static struct vpu_urgent_table_s vpu_urgent_table_wr_1_txhd2[] = {
	/*vpu module,           reg,                 port,    val,start,len,  name*/
	{VPU_ARB_NR_WR,         DI_WRARB_UGT_L1C1,  VPU_WRITE0, 1, 0,    2,  "nr_wr"},
	{VPU_ARB_DI_WR,         DI_WRARB_UGT_L1C1,  VPU_WRITE0, 1, 2,    2,  "di_wr"},
	{VPU_ARB_DI_SUBAXI_WR,  DI_WRARB_UGT_L1C1,  VPU_WRITE0, 1, 4,    2,  "di_subaxi_wr"},
	{VPU_ARB_DI_AFBCE0,     DI_WRARB_UGT_L1C1,  VPU_WRITE0, 1, 6,    2,  "di_afbce0(no use)"},
	{VPU_ARB_DI_AFBCE1,     DI_WRARB_UGT_L1C1,  VPU_WRITE0, 1, 8,    2,  "di_afbce1(no use)"},
		{}
};

static struct vpu_super_urgent_ctl_s vpu_super_urgent_t6w[] = {
	/*port       reg          en_val en_bit en_len val bit ofst_len name*/
	{VPU_READ0,  VPU_ARB_URG_CTRL1, 1,   0,   1,   3,  16,  2,   "vpu_read0(port_idx:0)"},
	{VPU_READ1,  VPU_ARB_URG_CTRL1, 0,   1,   1,   0,  18,  2,   "vpu_read1(port_idx:1)"},
	{VPU_READ2,  VPU_ARB_URG_CTRL1, 0,   2,   1,   0,  20,  2,   "vpu_read2(port_idx:2)"},
	{VPU_READ3,  VPU_ARB_URG_CTRL1, 1,   3,   1,   3,  22,  2,   "vpu_read3(port_idx:3)"},
	{VPU_WRITE0, VPU_ARB_URG_CTRL1, 0,   4,   1,   0,  24,  2,   "vpu_write0(port_idx:4)"},
	{VPU_WRITE1, VPU_ARB_URG_CTRL1, 0,   5,   1,   0,  26,  2,   "vpu_write1(port_idx:5)"},
	{VPU_WRITE2, VPU_ARB_URG_CTRL1, 0,   6,   1,   0,  28,  2,   "vpu_write2(port_idx:6)"},
	{}
};

static struct vpu_super_urgent_ctl_s vpu_super_urgent_t6x[] = {
	/*port       reg           en_val en_bit en_len val bit len name*/
	{VPU_READ0,  VPU_ARB_URG_CTRL1, 1,   0,   1,    3,  16,  2,  "vpu_read0(port_idx:0)"},
	{VPU_READ2,  VPU_ARB_URG_CTRL1, 0,   2,   1,    0,  20,  2,  "vpu_read2(port_idx:2)"},
	{VPU_WRITE0, VPU_ARB_URG_CTRL1, 0,   4,   1,    0,  24,  2,  "vpu_write0(port_idx:4)"},
	{VPU_WRITE2, VPU_ARB_URG_CTRL1, 0,   6,   1,    0,  28,  2,  "vpu_write2(port_idx:6)"},
	{}
};

static struct vpu_arb_info vpu_arb_ins[ARB_MODULE_MAX];

//static struct vpu_urgent_table_s vpu_urgent_table_rd_vpu0_2_level2_s1a[] = {
//{VPU_VPP_ARB0,		VPU_RDARB_UGT_L2C1, VPU_READ0, 3,	0, 2,	"vpp_arb0"},
//{VPU_VPP_ARB1,		VPU_RDARB_UGT_L2C1, VPU_READ0, 3,	2, 2,	"vpp_arb1"},
//{}
//};

int vpu_rdarb0_2_bind_l1(enum vpu_arb_mod_e level1_module, enum vpu_arb_mod_e level2_module)
{
	u32 val;
	struct vpu_arb_table_s *vpu0_2_rdarb_level1_module = vpu_rdarb_0_2_level1_tables;

	if (!vpu0_2_rdarb_level1_module)
		return -1;

	while (vpu0_2_rdarb_level1_module->vmod) {
		if (level1_module == vpu0_2_rdarb_level1_module->vmod)
			break;
		vpu0_2_rdarb_level1_module++;
	}
	if (!vpu0_2_rdarb_level1_module->vmod) {
		VPUERR("level1_table no such module\n");
		return -1;
	}
	switch (level2_module) {
	case VPU_ARB_VPP_ARB0:
		vpu_vcbus_setb(vpu0_2_rdarb_level1_module->reg, 0,
				vpu0_2_rdarb_level1_module->bit,
				vpu0_2_rdarb_level1_module->len);
		vpu0_2_rdarb_level1_module->bind_port = VPU_ARB_VPP_ARB0;
		vpu_pr_debug(MODULE_ARB_MODE, "%s bind to vpp_arb0\n",
				vpu0_2_rdarb_level1_module->name);
		break;
	case VPU_ARB_VPP_ARB1:
		vpu_vcbus_setb(vpu0_2_rdarb_level1_module->reg, 1,
				vpu0_2_rdarb_level1_module->bit,
				vpu0_2_rdarb_level1_module->len);
		vpu0_2_rdarb_level1_module->bind_port = VPU_ARB_VPP_ARB1;
		vpu_pr_debug(MODULE_ARB_MODE, "%s bind to vpp_arb1\n",
				vpu0_2_rdarb_level1_module->name);
		break;
	default:
		break;
	}
	if (vpu0_2_rdarb_level1_module->reqen_slv_len == 1)
		vpu_vcbus_setb(vpu0_2_rdarb_level1_module->reqen_slv_reg, 1,
			vpu0_2_rdarb_level1_module->reqen_slv_bit,
			vpu0_2_rdarb_level1_module->reqen_slv_len);
	else if (vpu0_2_rdarb_level1_module->reqen_slv_len == 2)
		vpu_vcbus_setb(vpu0_2_rdarb_level1_module->reqen_slv_reg, 3,
			vpu0_2_rdarb_level1_module->reqen_slv_bit,
			vpu0_2_rdarb_level1_module->reqen_slv_len);
	val = vpu_vcbus_read(vpu0_2_rdarb_level1_module->reg);
	vpu_pr_debug(MODULE_ARB_MODE, "VPU_RDARB_MOD[0x%x] = 0x%x\n",
		vpu0_2_rdarb_level1_module->reg, val);
	return 0;
}

int vpu_rdarb0_2_bind_l2(enum vpu_arb_mod_e level2_module, u32 vpu_read_port)
{
	u32 val;
	struct vpu_arb_table_s *vpu0_2_rdarb_level2_module = vpu_rdarb_0_2_level2_tables;

	if (!vpu0_2_rdarb_level2_module ||
		vpu_read_port > VPU_WRITE1)
		return -1;
	if (vpu_conf.data->vpu_arb_type == ARB_RD0123_WR012 &&
		vpu_read_port == VPU_READ2) {
		VPUERR("vpu read0 and read2 separate can not mux\n");
		return -1;
	}
	while (vpu0_2_rdarb_level2_module->vmod) {
		if (level2_module == vpu0_2_rdarb_level2_module->vmod)
			break;
		vpu0_2_rdarb_level2_module++;
	}
	if (!vpu0_2_rdarb_level2_module->vmod) {
		VPUERR("level2_table no such module\n");
		return -1;
	}
	switch (vpu_read_port) {
	case VPU_READ0:
		vpu_vcbus_setb(vpu0_2_rdarb_level2_module->reg, 0,
				vpu0_2_rdarb_level2_module->bit,
				vpu0_2_rdarb_level2_module->len);
		vpu0_2_rdarb_level2_module->bind_port = VPU_READ0;
		vpu_pr_debug(MODULE_ARB_MODE, "%s bind to vpu_read0\n",
				vpu0_2_rdarb_level2_module->name);
		break;
	case VPU_READ2:
		vpu_vcbus_setb(vpu0_2_rdarb_level2_module->reg, 1,
				vpu0_2_rdarb_level2_module->bit,
				vpu0_2_rdarb_level2_module->len);
		vpu0_2_rdarb_level2_module->bind_port = VPU_READ2;
		vpu_pr_debug(MODULE_ARB_MODE, "%s bind to vpu_read2\n",
				vpu0_2_rdarb_level2_module->name);
		break;
	default:
		break;
	}
	if (vpu0_2_rdarb_level2_module->reqen_slv_len == 1)
		vpu_vcbus_setb(vpu0_2_rdarb_level2_module->reqen_slv_reg, 1,
			vpu0_2_rdarb_level2_module->reqen_slv_bit,
			vpu0_2_rdarb_level2_module->reqen_slv_len);
	else if (vpu0_2_rdarb_level2_module->reqen_slv_len == 2)
		vpu_vcbus_setb(vpu0_2_rdarb_level2_module->reqen_slv_reg, 3,
			vpu0_2_rdarb_level2_module->reqen_slv_bit,
			vpu0_2_rdarb_level2_module->reqen_slv_len);
	val = vpu_vcbus_read(vpu0_2_rdarb_level2_module->reg);
	vpu_pr_debug(MODULE_ARB_MODE, "VPU_RDARB_MOD[0x%x] = 0x%x\n",
		vpu0_2_rdarb_level2_module->reg, val);
	return 0;
}

int vpu_rdarb1_bind(enum vpu_arb_mod_e rdarb1_module, u32 vpu_read_port)
{
	struct vpu_arb_table_s *vpu1_rdarb_module = vpu_rdarb_1_tables;

	if (!vpu1_rdarb_module ||
		vpu_read_port != VPU_READ1)
		return -1;
	while (vpu1_rdarb_module->vmod) {
		if (rdarb1_module == vpu1_rdarb_module->vmod)
			break;
		vpu1_rdarb_module++;
	}
	if (!vpu1_rdarb_module->vmod) {
		VPUERR("rdarb1_table no such module\n");
		return -1;
	}
	if (vpu1_rdarb_module->reqen_slv_len == 1)
		vpu_vcbus_setb(vpu1_rdarb_module->reqen_slv_reg, 1,
			vpu1_rdarb_module->reqen_slv_bit,
			vpu1_rdarb_module->reqen_slv_len);
	else if (vpu1_rdarb_module->reqen_slv_len == 2)
		vpu_vcbus_setb(vpu1_rdarb_module->reqen_slv_reg, 3,
			vpu1_rdarb_module->reqen_slv_bit,
			vpu1_rdarb_module->reqen_slv_len);
	/*DI only 1r1w so set to 0*/
	vpu_vcbus_setb(vpu1_rdarb_module->reg, 0,
				vpu1_rdarb_module->bit,
				vpu1_rdarb_module->len);
	vpu1_rdarb_module->bind_port = VPU_READ1;
	vpu_pr_debug(MODULE_ARB_MODE, "%s bind to vpu_read1\n",
		vpu1_rdarb_module->name);
	return 0;
}

int vpu_wrarb0_bind(enum vpu_arb_mod_e wrarb0_module, u32 vpu_write_port)
{
	struct vpu_arb_table_s *vpu0_wrarb_module = vpu_wrarb_0_tables;

	if (!vpu0_wrarb_module ||
		vpu_write_port != VPU_WRITE0)
		return -1;
	while (vpu0_wrarb_module->vmod) {
		if (wrarb0_module == vpu0_wrarb_module->vmod)
			break;
		vpu0_wrarb_module++;
	}
	if (!vpu0_wrarb_module->vmod) {
		VPUERR("rdarb1_table no such module\n");
		return -1;
	}
	if (vpu0_wrarb_module->reqen_slv_len == 1)
		vpu_vcbus_setb(vpu0_wrarb_module->reqen_slv_reg, 1,
			vpu0_wrarb_module->reqen_slv_bit,
			vpu0_wrarb_module->reqen_slv_len);
	else if (vpu0_wrarb_module->reqen_slv_len == 2)
		vpu_vcbus_setb(vpu0_wrarb_module->reqen_slv_reg, 3,
			vpu0_wrarb_module->reqen_slv_bit,
			vpu0_wrarb_module->reqen_slv_len);
	vpu_vcbus_setb(vpu0_wrarb_module->reg, 0,
				vpu0_wrarb_module->bit,
				vpu0_wrarb_module->len);
	vpu0_wrarb_module->bind_port = VPU_WRITE0;
	vpu_pr_debug(MODULE_ARB_MODE, "%s bind to vpu_write0\n",
		vpu0_wrarb_module->name);
	return 0;
}

int vpu_wrarb1_bind(enum vpu_arb_mod_e wrarb1_module, u32 vpu_write_port)
{
	struct vpu_arb_table_s *vpu1_wrarb_module = vpu_wrarb_1_tables;

	if (!vpu1_wrarb_module ||
		vpu_write_port != VPU_WRITE1)
		return -1;
	while (vpu1_wrarb_module->vmod) {
		if (wrarb1_module == vpu1_wrarb_module->vmod)
			break;
		vpu1_wrarb_module++;
	}
	if (!vpu1_wrarb_module->vmod) {
		VPUERR("rdarb1_table no such module\n");
		return -1;
	}
	if (vpu1_wrarb_module->reqen_slv_len == 1)
		vpu_vcbus_setb(vpu1_wrarb_module->reqen_slv_reg, 1,
			vpu1_wrarb_module->reqen_slv_bit, vpu1_wrarb_module->reqen_slv_len);
	else if (vpu1_wrarb_module->reqen_slv_len == 2)
		vpu_vcbus_setb(vpu1_wrarb_module->reqen_slv_reg, 3,
			vpu1_wrarb_module->reqen_slv_bit, vpu1_wrarb_module->reqen_slv_len);
	vpu_vcbus_setb(vpu1_wrarb_module->reg, 0,
				vpu1_wrarb_module->bit,
				vpu1_wrarb_module->len);
	vpu1_wrarb_module->bind_port = VPU_WRITE1;
	vpu_pr_debug(MODULE_ARB_MODE, "%s bind to vpu_write1\n",
		vpu1_wrarb_module->name);
	return 0;
}


void dump_module_info(int port, struct vpu_arb_table_s *vpu_arb_table)
{
	struct vpu_arb_table_s *arb_table = vpu_arb_table;

	if (!arb_table) {
		VPUERR("table is NULL:\n");
		return;
	}
	while (arb_table->vmod) {
		vpu_pr_debug(MODULE_ARB_MODE, "module_name:%-20s module_index:%d (bind_reg:[0x%x],bit%d)\n",
			arb_table->name,
			arb_table->vmod,
			arb_table->reg,
			arb_table->bit);
		arb_table++;
	}
}

void get_module_info_by_port(int port)
{
	struct vpu_arb_table_s *vpu_arb_table;

	switch (port) {
	case VPU_READ0:
	case VPU_READ2:
		if (port == VPU_READ0 ||
			(vpu_conf.data->vpu_arb_type != ARB_RD0123_WR012 &&
			vpu_conf.data->vpu_arb_type != ARB_RD02_WR02 &&
			port == VPU_READ2)) {
			vpu_arb_table = vpu_rdarb_0_2_level1_tables;
			dump_module_info(port, vpu_arb_table);
			vpu_arb_table = vpu_rdarb_0_2_level2_tables;
			dump_module_info(port, vpu_arb_table);
		} else {
			vpu_arb_table = vpu_rdarb_2_tables;
			dump_module_info(port, vpu_arb_table);
		}
		break;
	case VPU_READ1:
		vpu_arb_table = vpu_rdarb_1_tables;
		dump_module_info(port, vpu_arb_table);
		break;
	case VPU_READ3:
		vpu_arb_table = vpu_rdarb_3_tables;
		dump_module_info(port, vpu_arb_table);
		break;
	case VPU_WRITE0:
		vpu_arb_table = vpu_wrarb_0_tables;
		dump_module_info(port, vpu_arb_table);
		break;
	case VPU_WRITE1:
		vpu_arb_table = vpu_wrarb_1_tables;
		dump_module_info(port, vpu_arb_table);
		break;
	case VPU_WRITE2:
		vpu_arb_table = vpu_wrarb_2_tables;
		dump_module_info(port, vpu_arb_table);
		break;
	default:
		break;
	}
}

struct vpu_qos_table_s {
	unsigned int vmod;
	unsigned int reg;
	unsigned int urgent;
	unsigned int val;
	unsigned int start_bit;
	unsigned int len;
	char *name;
};

//struct vpu_qos_table_s vpu_qos_table[] = {
//	/*vpu module,    reg,                urgent, val,  start, len,  name*/
//	{VPU_READ0,      VPU_RDARB_UGT_L2C1,  0,     0x3,  0,     4,    "vpu_read0"},
//	{VPU_READ1,      DI_RDARB_UGT_L1C1,   1,     0x7,  20,    4,    "vpu_read1"},
//	{VPU_READ2,      VPU_RDARB_UGT_L2C1,  2,     0xb,  8,     4,    "vpu_read2"},
//	{VPU_WRITE0,     DI_RDARB_UGT_L1C1,   3,     0xf,  12,    4,    "vpu_write0"},
//	{VPU_WRITE1,     DI_RDARB_UGT_L1C1,   3,     0xf,  28,    4,    "vpu_write1"},
//		{}
	//VPU_AXI_QOS_RD1,
	//VPU_AXI_QOS_WR0
//};

//static int vpu_qos_table_set(u32 vpu_port, u32 urgent_level, u32 urgent_value);
//static int vpu_qos_table_get(u32 vpu_port, u32 urgent_level, u32 *urgent_value);

struct vpu_urgent_ctrl_s {
	unsigned int vpu_port;
	unsigned int vmod;
	unsigned int urgent;
};

void super_urgent_set(int port, u32 enable, u32 urgent_value)
{
	struct vpu_super_urgent_ctl_s *super_urgent_table = vpu_super_urgent_table;

	while (super_urgent_table->name) {
		if (super_urgent_table->port == port)
			break;
		super_urgent_table++;
	}
	if (!super_urgent_table->name) {
		VPUERR("vpu port error\n");
		return;
	}
	vpu_vcbus_setb(VPU_ARB_URG_CTRL, 1, 29, 1);
	super_urgent_table->port_en_val = enable;
	vpu_vcbus_setb(super_urgent_table->port_sp_ugt_reg, enable,
		super_urgent_table->port_en_bit, super_urgent_table->port_en_len);
	vpu_pr_debug(MODULE_ARB_URGENT, "%s super urgent enable set to %d\n",
			super_urgent_table->name, enable);
	super_urgent_table->port_offset_val = urgent_value;
	vpu_vcbus_setb(super_urgent_table->port_sp_ugt_reg, urgent_value,
		super_urgent_table->port_offset_bit, super_urgent_table->port_offset_len);
	vpu_pr_debug(MODULE_ARB_URGENT, "%s super urgent val set to %d\n",
			super_urgent_table->name, urgent_value);
}

ssize_t show_super_urgent_status(char *buf)
{
	ssize_t len = 0;

	struct vpu_super_urgent_ctl_s *super_urgent_table =
			vpu_super_urgent_table;
	if (!super_urgent_table) {
		sprintf(buf, "super urgent not init\n");
		return len;
	}
	while (super_urgent_table->name) {
		len += sprintf(buf + len, "%-10s super_urgent_status:%d super_urgent_val = %d\n",
					super_urgent_table->name,
					super_urgent_table->port_en_val,
					super_urgent_table->port_offset_val);
		super_urgent_table++;
	}
	return len;
}

int arb_urgent_set(enum vpu_arb_mod_e vmod, struct vpu_urgent_table_s *urgent_tbl, u32 urgent_value)
{
	u32 val;
	struct vpu_urgent_table_s *urgent_tables = urgent_tbl;
	struct vpu_urgent_table_s *set_urgent_module = NULL;

	if (!urgent_tables) {
		VPUERR("urgent tables is NULL\n");
		return -1;
	}
	while (urgent_tables->vmod) {
		if (urgent_tables->vmod == vmod)
			set_urgent_module = urgent_tables;
		urgent_tables++;
	}
	if (!set_urgent_module) {
		VPUERR("urgent table no such module\n");
		return -1;
	}
	set_urgent_module->val = urgent_value;
	vpu_vcbus_setb(set_urgent_module->reg, urgent_value,
			set_urgent_module->start_bit, set_urgent_module->len);
	vpu_pr_debug(MODULE_ARB_URGENT, "%s urgent set to %d\n",
			set_urgent_module->name, urgent_value);
	val = vpu_vcbus_read(set_urgent_module->reg);
	vpu_pr_debug(MODULE_ARB_URGENT, "urgent_reg[0x%x] = 0x%x\n",
			set_urgent_module->reg, val);
	return 0;
}

int vpu_urgent_set(enum vpu_arb_mod_e vmod, u32 urgent_value)
{
	u32 ret = -1;

	if (vmod < VPU_ARB_VPP_ARB0 ||
		vmod >= ARB_MODULE_MAX) {
		VPUERR("module error!\n");
		return ret;
	}
	if (urgent_value > 3) {
		VPUERR("urgent value error!\n");
		return ret;
	}
	if (vmod >= VPU_ARB_VPP_ARB0 &&
		vmod <= VPU_ARB_VPU_DMA)
		ret = arb_urgent_set(vmod, vpu_urgent_rd_0_2_level2_tbl, urgent_value);
	else if (vmod >= VPU_ARB_DI_IF1 &&
			vmod <= VPU_ARB_DI_AFBCD)
		ret = arb_urgent_set(vmod, vpu_urgent_rd_1_tbl, urgent_value);
	else if (vmod >= VPU_ARB_VDIN_WR &&
			vmod <= VPU_ARB_VPU_DMA_WR)
		ret = arb_urgent_set(vmod, vpu_urgent_wr_0_tbl, urgent_value);
	else if (vmod >= VPU_ARB_NR_WR &&
			vmod <= VPU_ARB_DI_AFBCE1)
		ret = arb_urgent_set(vmod, vpu_urgent_wr_1_tbl, urgent_value);
	return ret;
}

int arb_urgent_get(enum vpu_arb_mod_e vmod, struct vpu_urgent_table_s *urgent_tbl)
{
	u32 urgent_val;
	struct vpu_urgent_table_s *urgent_tables = urgent_tbl;
	struct vpu_urgent_table_s *get_urgent_module = NULL;

	if (!urgent_tables) {
		VPUERR("urgent tables is NULL\n");
		return -1;
	}
	while (urgent_tables->vmod) {
		if (urgent_tables->vmod == vmod)
			get_urgent_module = urgent_tables;
		urgent_tables++;
	}
	if (!get_urgent_module) {
		VPUERR("urgent no such module\n");
		return -1;
	}
	urgent_val = get_urgent_module->val;
	return urgent_val;
}

int vpu_urgent_get(enum vpu_arb_mod_e vmod)
{
	u32 urgent_value = -1;

	if (vmod < VPU_ARB_VPP_ARB0 ||
		vmod >= ARB_MODULE_MAX) {
		VPUERR("module error!\n");
		return urgent_value;
	}

	if (vmod >= VPU_ARB_VPP_ARB0 &&
		vmod <= VPU_ARB_VPU_DMA)
		urgent_value = arb_urgent_get(vmod, vpu_urgent_rd_0_2_level2_tbl);
	else if (vmod >= VPU_ARB_DI_IF1 &&
			vmod <= VPU_ARB_DI_AFBCD)
		urgent_value = arb_urgent_get(vmod, vpu_urgent_rd_1_tbl);
	else if (vmod >= VPU_ARB_VDIN_WR &&
			vmod <= VPU_ARB_VPU_DMA_WR)
		urgent_value = arb_urgent_get(vmod, vpu_urgent_wr_0_tbl);
	else if (vmod >= VPU_ARB_NR_WR &&
		vmod <= VPU_ARB_DI_AFBCE1)
		urgent_value = arb_urgent_get(vmod, vpu_urgent_wr_1_tbl);

	return urgent_value;
}

/*
 *DI module use clk1,other vpu module use clk2
 *clk1_rdarb corresponds to vpu_read1
 *clk2_rdarb corresponds to vpu_read0 and vpu_read2
 */

void dump_vpu_rdarb_0_2_bind_table(void)
{
	u32 val;
	struct vpu_arb_table_s *vpu0_2_rdarb_level1_tables =
					vpu_rdarb_0_2_level1_tables;
	struct vpu_arb_table_s *vpu0_2_rdarb_level2_tables =
					vpu_rdarb_0_2_level2_tables;

	if (!vpu0_2_rdarb_level1_tables ||
		!vpu0_2_rdarb_level2_tables) {
		VPUERR("rdarb table error!\n");
		return;
	}
	/*
	 *1.VPU_READ0
	 * 1.1 VPP_ARB0/1->VPU_READ0
	 */
	while (vpu0_2_rdarb_level2_tables->vmod) {
		if (vpu0_2_rdarb_level2_tables->bind_port ==
				VPU_READ0) {
			if (vpu0_2_rdarb_level2_tables->vmod ==
					VPU_ARB_VPP_ARB0) {
			/*VPU_VPP_ARB0*/
				while (vpu0_2_rdarb_level1_tables->vmod) {
					if (vpu0_2_rdarb_level1_tables->bind_port ==
							VPU_ARB_VPP_ARB0) {
						vpu_pr_debug(MODULE_ARB_MODE, "%-10s ---> vpp_arb0 ---> vpu_read0\n",
							vpu0_2_rdarb_level1_tables->name);
					}
					vpu0_2_rdarb_level1_tables++;
				}
				vpu0_2_rdarb_level1_tables = vpu_rdarb_0_2_level1_tables;
			} else if (vpu0_2_rdarb_level2_tables->vmod ==
							VPU_ARB_VPP_ARB1) {
			/*VPU_VPP_ARB1*/
				while (vpu0_2_rdarb_level1_tables->vmod) {
					if (vpu0_2_rdarb_level1_tables->bind_port ==
							VPU_ARB_VPP_ARB1) {
						vpu_pr_debug(MODULE_ARB_MODE, "%-10s ---> vpp_arb1 ---> vpu_read0\n",
							vpu0_2_rdarb_level1_tables->name);
					}
					vpu0_2_rdarb_level1_tables++;
				}
				vpu0_2_rdarb_level1_tables = vpu_rdarb_0_2_level1_tables;
			}
		}
		vpu0_2_rdarb_level2_tables++;
	}
	vpu0_2_rdarb_level2_tables = vpu_rdarb_0_2_level2_tables;

	/* 1.2 level2 other module-> VPU_READ0*/
	while (vpu0_2_rdarb_level2_tables->vmod) {
		if (vpu0_2_rdarb_level2_tables->bind_port ==
				VPU_READ0) {
			if (vpu0_2_rdarb_level2_tables->vmod !=
					VPU_ARB_VPP_ARB0 &&
				vpu0_2_rdarb_level2_tables->vmod !=
					VPU_ARB_VPP_ARB1) {
				/*level2 other module*/
				vpu_pr_debug(MODULE_ARB_MODE, "%24s ---> vpu_read0\n",
					vpu0_2_rdarb_level2_tables->name);
				}
			}
		vpu0_2_rdarb_level2_tables++;
	}
	vpu0_2_rdarb_level2_tables = vpu_rdarb_0_2_level2_tables;

	/*
	 *2.VPU_READ2
	 * 2.1 VPP_ARB0/1->VPU_READ2
	 */
	while (vpu0_2_rdarb_level2_tables->vmod) {
		if (vpu0_2_rdarb_level2_tables->bind_port ==
				VPU_READ2) {
			if (vpu0_2_rdarb_level2_tables->vmod ==
					VPU_ARB_VPP_ARB0) {
			/*VPU_VPP_ARB0*/
				while (vpu0_2_rdarb_level1_tables->vmod) {
					if (vpu0_2_rdarb_level1_tables->bind_port ==
							VPU_ARB_VPP_ARB0) {
						vpu_pr_debug(MODULE_ARB_MODE, "%-10s ---> vpp_arb0 ---> vpu_read2\n",
							vpu0_2_rdarb_level1_tables->name);
					}
					vpu0_2_rdarb_level1_tables++;
				}
				vpu0_2_rdarb_level1_tables = vpu_rdarb_0_2_level1_tables;
			} else if (vpu0_2_rdarb_level2_tables->vmod ==
					VPU_ARB_VPP_ARB1) {
			/*VPU_VPP_ARB1*/
				while (vpu0_2_rdarb_level1_tables->vmod) {
					if (vpu0_2_rdarb_level1_tables->bind_port ==
							VPU_ARB_VPP_ARB1) {
						vpu_pr_debug(MODULE_ARB_MODE, "%-10s ---> vpp_arb1 ---> vpu_read2\n",
							vpu0_2_rdarb_level1_tables->name);
					}
					vpu0_2_rdarb_level1_tables++;
				}
				vpu0_2_rdarb_level1_tables = vpu_rdarb_0_2_level1_tables;
			}
		}
		vpu0_2_rdarb_level2_tables++;
	}
	vpu0_2_rdarb_level2_tables = vpu_rdarb_0_2_level2_tables;

	/* 2.2 level2 other module-> VPU_READ2*/
	while (vpu0_2_rdarb_level2_tables->vmod) {
		if (vpu0_2_rdarb_level2_tables->bind_port ==
				VPU_READ2) {
			if (vpu0_2_rdarb_level2_tables->vmod !=
					VPU_ARB_VPP_ARB0 &&
				vpu0_2_rdarb_level2_tables->vmod !=
					VPU_ARB_VPP_ARB1) {
				vpu_pr_debug(MODULE_ARB_MODE, "%24s ---> vpu_read2\n",
					vpu0_2_rdarb_level2_tables->name);
				}
			}
		vpu0_2_rdarb_level2_tables++;
	}
	vpu0_2_rdarb_level2_tables = vpu_rdarb_0_2_level2_tables;
	val = vpu_vcbus_read(vpu_rdarb_0_2_level1_tables[0].reg);
	vpu_pr_debug(MODULE_ARB_MODE, "level1_bind_reg[0x%x] = 0x%x\n",
		vpu_rdarb_0_2_level1_tables[0].reg, val);
	val = vpu_vcbus_read(vpu_rdarb_0_2_level2_tables[0].reg);
	vpu_pr_debug(MODULE_ARB_MODE, "level2_bind_reg[0x%x] = 0x%x\n",
		vpu_rdarb_0_2_level2_tables[0].reg, val);
}

void dump_arb_module_bind_info(int prot, struct vpu_arb_table_s *vpu_arb_tables)
{
	u32 val;
	struct vpu_arb_table_s *arb_table = vpu_arb_tables;
	static const char * const port_str[] = {
		"vpu_read0", "vpu_read1", "vpu_read2", "vpu_read3",
		"vpu_write0", "vpu_write1", "vpu_write2"
	};
	if (!arb_table) {
		VPUERR("table is NULL!\n");
		return;
	}
	while (arb_table->vmod) {
		vpu_pr_debug(MODULE_ARB_MODE, "%15s ---> %s\n",
			arb_table->name, port_str[prot]);
		arb_table++;
	}
	if (vpu_arb_tables->reg != VPU_ARB_UNBINDABLE_REG) {
		val = vpu_vcbus_read(vpu_arb_tables->reg);
		vpu_pr_debug(MODULE_ARB_MODE, "bind_reg[0x%x] = 0x%x\n",
				vpu_arb_tables->reg, val);
	}
}

void get_module_bind_info_by_port(int port)
{
	struct vpu_arb_table_s *vpu_arb_table;

	switch (port) {
	case VPU_READ0:
	case VPU_READ2:
		if (port == VPU_READ0 ||
			(vpu_conf.data->vpu_arb_type != ARB_RD0123_WR012 &&
			vpu_conf.data->vpu_arb_type != ARB_RD02_WR02 &&
			port == VPU_READ2)) {
			dump_vpu_rdarb_0_2_bind_table();
		} else {
			vpu_arb_table = vpu_rdarb_2_tables;
			dump_arb_module_bind_info(port, vpu_arb_table);
		}
		break;
	case VPU_READ1:
		vpu_arb_table = vpu_rdarb_1_tables;
		dump_arb_module_bind_info(port, vpu_arb_table);
		break;
	case VPU_READ3:
		vpu_arb_table = vpu_rdarb_3_tables;
		dump_arb_module_bind_info(port, vpu_arb_table);
		break;
	case VPU_WRITE0:
		vpu_arb_table = vpu_wrarb_0_tables;
		dump_arb_module_bind_info(port, vpu_arb_table);
		break;
	case VPU_WRITE1:
		vpu_arb_table = vpu_wrarb_1_tables;
		dump_arb_module_bind_info(port, vpu_arb_table);
		break;
	case VPU_WRITE2:
		vpu_arb_table = vpu_wrarb_2_tables;
		dump_arb_module_bind_info(port, vpu_arb_table);
		break;
	default:
		break;
	}
}

static void sort_urgent_table(struct vpu_urgent_table_s *urgent_table)
{
	int i, j;
	int array_size = 0;
	struct vpu_urgent_table_s temp;
	struct vpu_urgent_table_s *urgent_module = urgent_table;

	while (urgent_module->vmod) {
		array_size++;
		urgent_module++;
	}
	urgent_module = urgent_table;

	for (i = 0; i < array_size - 1; i++) {
		for (j = 0; j < array_size - i - 1; j++) {
			if (urgent_module[j].val < urgent_module[j + 1].val) {
				temp = urgent_module[j];
				urgent_module[j] = urgent_module[j + 1];
				urgent_module[j + 1] = temp;
			}
		}
	}
	for (i = 0; i < array_size; i++)
		urgent_table[i] = urgent_module[i];
}

static void dump_vpu_arb_urgent_info(int port, struct vpu_urgent_table_s *vpu_urgent_table)
{
	u32 val;
	struct vpu_urgent_table_s *urgent_table =
			vpu_urgent_table;
	static const char * const port_str[] = {
		"vpu_read0", "vpu_read1", "vpu_read2", "vpu_read3",
		"vpu_write0", "vpu_write1", "vpu_write2"
	};
	if (!urgent_table) {
		VPUERR("urgent table is error\n");
		return;
	}
	vpu_pr_debug(MODULE_ARB_URGENT, "%s urgent table\n", port_str[port]);
	sort_urgent_table(urgent_table);
	while (urgent_table->vmod) {
		vpu_pr_debug(MODULE_ARB_URGENT, "module name:%17s urgent_value: %d\n",
			urgent_table->name,
			urgent_table->val);
		urgent_table++;
	}
	val = vpu_vcbus_read(vpu_urgent_table->reg);
	vpu_pr_debug(MODULE_ARB_URGENT, "urgent_reg[0x%x] = 0x%x\n",
		vpu_urgent_table->reg, val);
}

void get_urgent_info_by_port(int port)
{
	struct vpu_urgent_table_s *vpu_urgent_table;

	switch (port) {
	case VPU_READ0:
	case VPU_READ2:
		if (port == VPU_READ0 ||
			(vpu_conf.data->vpu_arb_type != ARB_RD0123_WR012 &&
			port == VPU_READ2)) {
			vpu_urgent_table = vpu_urgent_rd_0_2_level2_tbl;
			dump_vpu_arb_urgent_info(port, vpu_urgent_table);
		}
		break;
	case VPU_READ1:
		vpu_urgent_table = vpu_urgent_rd_1_tbl;
		dump_vpu_arb_urgent_info(port, vpu_urgent_table);
		break;
	case VPU_WRITE0:
		vpu_urgent_table = vpu_urgent_wr_0_tbl;
		dump_vpu_arb_urgent_info(port, vpu_urgent_table);
		break;
	case VPU_WRITE1:
		vpu_urgent_table = vpu_urgent_wr_1_tbl;
		dump_vpu_arb_urgent_info(port, vpu_urgent_table);
		break;
	default:
		break;
	}
}

int vpu_arb_register(enum vpu_arb_mod_e module, void *cb)
{
	struct vpu_arb_info *ins = &vpu_arb_ins[module];
	struct mutex *lock = NULL;

	if (module > ARB_MODULE_MAX) {
		VPUERR("%s failed, module = %d\n", __func__, module);
		return -1;
	}
	if (!ins->registered) {
		lock = &ins->vpu_arb_lock;
		mutex_lock(lock);
		ins->registered = 1;
		ins->arb_cb = cb;
		mutex_unlock(lock);
	}
	pr_info("%s module=%d vpu_arb register ok\n", __func__, module);
	return 0;
}
EXPORT_SYMBOL(vpu_arb_register);

int vpu_arb_unregister(enum vpu_arb_mod_e module)
{
	struct vpu_arb_info *ins = &vpu_arb_ins[module];
	struct mutex *lock = NULL;

	if (module > ARB_MODULE_MAX) {
		VPUERR("%s failed, module = %d\n", __func__, module);
		return -1;
	}
	if (ins->registered) {
		lock = &ins->vpu_arb_lock;
		mutex_lock(lock);
		ins->registered = 0;
		ins->arb_cb = NULL;
		mutex_unlock(lock);
	}
	return 0;
}
EXPORT_SYMBOL(vpu_arb_unregister);

int vpu_arb_config(enum vpu_arb_mod_e module, u32 urgent_value)
{
	struct vpu_arb_info *ins = &vpu_arb_ins[module];
	u32 pre_value;

	if (module > ARB_MODULE_MAX) {
		VPUERR("%s failed, module = %d\n", __func__, module);
		return -1;
	}
	if (ins->registered) {
		pre_value = vpu_urgent_get(module);
		vpu_urgent_set(module, urgent_value);
		if (ins->arb_cb)
			ins->arb_cb(module, pre_value, urgent_value);
	}
	return 0;
}
EXPORT_SYMBOL(vpu_arb_config);

void init_read0_2_bind(void)
{
	struct vpu_arb_table_s *vpu_rdarb_vpu0_2_level1 = vpu_rdarb_0_2_level1_tables;
	struct vpu_arb_table_s *vpu_rdarb_vpu0_2_level2 = vpu_rdarb_0_2_level2_tables;

	if (!vpu_rdarb_vpu0_2_level1 || !vpu_rdarb_vpu0_2_level2) {
		VPUERR("%s table error\n", __func__);
		return;
	}
	//level1 bind
	while (vpu_rdarb_vpu0_2_level1->vmod &&
		vpu_rdarb_vpu0_2_level1->bind_port != NO_PORT) {
		vpu_rdarb0_2_bind_l1(vpu_rdarb_vpu0_2_level1->vmod,
			vpu_rdarb_vpu0_2_level1->bind_port);
		vpu_rdarb_vpu0_2_level1++;
	}
	//level2 bind
	while (vpu_rdarb_vpu0_2_level2->vmod &&
		vpu_rdarb_vpu0_2_level2->bind_port != NO_PORT) {
		vpu_rdarb0_2_bind_l2(vpu_rdarb_vpu0_2_level2->vmod,
			vpu_rdarb_vpu0_2_level2->bind_port);
		vpu_rdarb_vpu0_2_level2++;
	}
}

void init_read1_bind(void)
{
	struct vpu_arb_table_s *vpu_rdarb_vpu1 = vpu_rdarb_1_tables;

	if (!vpu_rdarb_vpu1) {
		VPUERR("%s table error\n", __func__);
		return;
	}
	while (vpu_rdarb_vpu1->vmod && vpu_rdarb_vpu1->bind_port != NO_PORT) {
		vpu_rdarb1_bind(vpu_rdarb_vpu1->vmod,
			vpu_rdarb_vpu1->bind_port);
		vpu_rdarb_vpu1++;
	}
}

void init_write0_bind(void)
{
	struct vpu_arb_table_s *vpu_wrarb_vpu0 = vpu_wrarb_0_tables;

	if (!vpu_wrarb_vpu0) {
		VPUERR("%s table error\n", __func__);
		return;
	}
	while (vpu_wrarb_vpu0->vmod && vpu_wrarb_vpu0->bind_port != NO_PORT) {
		vpu_wrarb0_bind(vpu_wrarb_vpu0->vmod,
			vpu_wrarb_vpu0->bind_port);
		vpu_wrarb_vpu0++;
	}
}

void init_write1_bind(void)
{
	struct vpu_arb_table_s *vpu_wrarb_vpu1 = vpu_wrarb_1_tables;

	if (!vpu_wrarb_vpu1) {
		VPUERR("%s table error\n", __func__);
		return;
	}
	while (vpu_wrarb_vpu1->vmod && vpu_wrarb_vpu1->bind_port != NO_PORT) {
		vpu_wrarb0_bind(vpu_wrarb_vpu1->vmod,
			vpu_wrarb_vpu1->bind_port);
		vpu_wrarb_vpu1++;
	}
}

void init_read0_2_write0_urgent(void)
{
	struct vpu_urgent_table_s *vpu_urgent_table_rd_vpu0_2 =
				vpu_urgent_rd_0_2_level2_tbl;
	struct vpu_urgent_table_s *vpu_urgent_table_wr_vpu0 =
				vpu_urgent_wr_0_tbl;

	if (!vpu_urgent_table_rd_vpu0_2 ||
		!vpu_urgent_table_wr_vpu0) {
		VPUERR("%s urgent table error\n", __func__);
		return;
	}
	while (vpu_urgent_table_rd_vpu0_2->vmod) {
		vpu_urgent_set(vpu_urgent_table_rd_vpu0_2->vmod,
			vpu_urgent_table_rd_vpu0_2->val);
		vpu_urgent_table_rd_vpu0_2++;
	}
	while (vpu_urgent_table_wr_vpu0->vmod) {
		vpu_urgent_set(vpu_urgent_table_wr_vpu0->vmod,
			vpu_urgent_table_wr_vpu0->val);
		vpu_urgent_table_wr_vpu0++;
	}
}

void init_read1_write1_urgent(void)
{
	struct vpu_urgent_table_s *vpu_urgent_table_rd_vpu1 =
				vpu_urgent_rd_1_tbl;
	struct vpu_urgent_table_s *vpu_urgent_table_wr_vpu1 =
				vpu_urgent_wr_1_tbl;
	if (!vpu_urgent_table_rd_vpu1 ||
		!vpu_urgent_table_wr_vpu1) {
		VPUERR("%s urgent table error\n", __func__);
		return;
	}
	while (vpu_urgent_table_rd_vpu1->vmod) {
		vpu_urgent_set(vpu_urgent_table_rd_vpu1->vmod,
			vpu_urgent_table_rd_vpu1->val);
		vpu_urgent_table_rd_vpu1++;
	}
	while (vpu_urgent_table_wr_vpu1->vmod) {
		vpu_urgent_set(vpu_urgent_table_wr_vpu1->vmod,
			vpu_urgent_table_wr_vpu1->val);
		vpu_urgent_table_wr_vpu1++;
	}
}

void init_di_arb_urgent(void)
{
	if (vpu_conf.data->chip_type != VPU_CHIP_T7 &&
		vpu_conf.data->chip_type != VPU_CHIP_S7 &&
		vpu_conf.data->chip_type != VPU_CHIP_S7D &&
		vpu_conf.data->chip_type != VPU_CHIP_TXHD2)
		return;
	init_read1_bind();
	init_write1_bind();
	init_read1_write1_urgent();
}

void init_super_urgent(void)
{
	int sideband_fifo_cctrl = 0xffff;

	struct vpu_super_urgent_ctl_s *super_urgent_table =
				vpu_super_urgent_table;
	if (!super_urgent_table) {
		VPUERR("%s super urgent table error\n", __func__);
		return;
	}
	/*enable super urgent top control*/
	vpu_vcbus_setb(VPU_ARB_URG_CTRL, 1, 29, 1);
	vpu_vcbus_setb(VPU_ARB_URG_CTRL, sideband_fifo_cctrl, 0, 16);
	while (super_urgent_table->name) {
		super_urgent_set(super_urgent_table->port,
			super_urgent_table->port_en_val,
			super_urgent_table->port_offset_val);
		super_urgent_table++;
	}
}

int init_arb_urgent_table(void)
{
	int i;
	if (vpu_conf.data->chip_type == VPU_CHIP_T7 ||
		vpu_conf.data->chip_type == VPU_CHIP_S6 ||
		vpu_conf.data->chip_type == VPU_CHIP_T5M) {
		if (vpu_conf.data->chip_type == VPU_CHIP_T5M) {
			/*
			 * T5M revA osd to dmc1
			 * T5M revB osd to dmc0
			 */
			if (is_meson_rev_a()) {
				vpu_rdarb_0_2_level1_tables = vpu_rdarb0_2_level1_t5m_rev_a;
				vpu_rdarb_0_2_level2_tables = vpu_rdarb0_2_level2_t5m_rev_a;
			} else {
				vpu_rdarb_0_2_level1_tables = vpu_rdarb0_2_level1_t5m_rev_b;
				vpu_rdarb_0_2_level2_tables = vpu_rdarb0_2_level2_t5m_rev_b;
			}
		} else {
			vpu_rdarb_0_2_level1_tables = vpu_rdarb0_2_level1_t7;
			vpu_rdarb_0_2_level2_tables = vpu_rdarb0_2_level2_t7;
		}
		vpu_rdarb_1_tables = vpu_rdarb_1_t7;
		vpu_wrarb_0_tables = vpu_wrarb_0_t7;
		vpu_wrarb_1_tables = vpu_wrarb_1_t7;

		vpu_urgent_rd_0_2_level2_tbl = vpu_urgent_rd_0_2_level2_t7;
		vpu_urgent_rd_1_tbl = vpu_urgent_table_rd_1_t7;
		vpu_urgent_wr_0_tbl = vpu_urgent_table_wr_0_t7;
		vpu_urgent_wr_1_tbl = vpu_urgent_table_wr_1_t7;
	} else if (vpu_conf.data->vpu_arb_type == ARB_RD01_WR01) {
		vpu_rdarb_0_2_level1_tables = vpu_rdarb_0_2_level1_txhd2;
		vpu_rdarb_0_2_level2_tables = vpu_rdarb_0_2_level2_txhd2;
		vpu_rdarb_1_tables = vpu_rdarb_1_txhd2;
		vpu_wrarb_0_tables = vpu_wrarb_0_txhd2;
		vpu_wrarb_1_tables = vpu_wrarb_1_txhd2;

		vpu_urgent_rd_0_2_level2_tbl = vpu_urgent_table_rd_0_2_level2_txhd2;
		vpu_urgent_rd_1_tbl = vpu_urgent_table_rd_1_txhd2;
		vpu_urgent_wr_0_tbl = vpu_urgent_table_wr_0_txhd2;
		vpu_urgent_wr_1_tbl = vpu_urgent_table_wr_1_txhd2;
	} else if (vpu_conf.data->vpu_arb_type == ARB_RD0123_WR012) {
		/*read0 and read2 separate*/
		vpu_rdarb_0_2_level1_tables = vpu_rdarb_0_level1_t6w;
		vpu_rdarb_0_2_level2_tables = vpu_rdarb_0_level2_t6w;
		vpu_rdarb_1_tables = vpu_rdarb_1_t6w;
		vpu_rdarb_2_tables = vpu_rdarb_2_t6w;
		vpu_rdarb_3_tables = vpu_rdarb_3_t6w;
		vpu_wrarb_0_tables = vpu_wrarb_vpu0_t6w;
		vpu_wrarb_1_tables = vpu_wrarb_1_t6w;
		vpu_wrarb_2_tables = vpu_wrarb_2_t6w;

		vpu_urgent_rd_0_2_level2_tbl = vpu_urgent_table_rd_vpu0_t6w;
		vpu_urgent_wr_0_tbl = vpu_urgent_table_wr_vpu0_t6w;
		vpu_super_urgent_table = vpu_super_urgent_t6w;
	} else if (vpu_conf.data->vpu_arb_type == ARB_RD02_WR02) {
		vpu_rdarb_0_2_level1_tables = vpu_rdarb_0_level1_t6w;
		vpu_rdarb_0_2_level2_tables = vpu_rdarb_0_level2_t6w;
		vpu_rdarb_2_tables = vpu_rdarb_2_t6x;
		vpu_wrarb_0_tables = vpu_wrarb_vpu0_t6w;
		vpu_wrarb_2_tables = vpu_wrarb_2_t6x;

		vpu_urgent_rd_0_2_level2_tbl = vpu_urgent_table_rd_vpu0_t6w;
		vpu_urgent_wr_0_tbl = vpu_urgent_table_wr_vpu0_t6w;
		vpu_super_urgent_table = vpu_super_urgent_t6x;
	}

	for (i = 1; i < ARB_MODULE_MAX; i++)
		mutex_init(&vpu_arb_ins[i].vpu_arb_lock);
	init_read0_2_bind();
	init_write0_bind();
	init_read0_2_write0_urgent();
	/*t6w unbindable need control vd at read3 urgent control need by top super urgent*/
	if (vpu_conf.data->vpu_arb_type == ARB_RD0123_WR012 ||
		vpu_conf.data->vpu_arb_type == ARB_RD02_WR02)
		init_super_urgent();
	//temply add for a9
	/*enable super urgent top control*/
	if (vpu_conf.data->chip_type == VPU_CHIP_A9) {
		vpu_vcbus_setb(VPU_ARB_URG_CTRL, 1, 29, 1);
		vpu_vcbus_setb(VPU_ARB_URG_CTRL, 0x9, 12, 6);
		vpu_vcbus_setb(VPU_ARB_URG_CTRL, 3, 18, 2);
		vpu_vcbus_setb(VPU_ARB_URG_CTRL, 3, 24, 2);
	}
	return 0;
}
