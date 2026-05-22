/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __REG_HELPER_H
#define __REG_HELPER_H

//#include "arch/vpp_regs.h"
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "arch/ve_regs.h"
#include "arch/cm_regs.h"
#include "arch/vpp_s7d_sr_regs.h"
#include <linux/amlogic/media/rdma/rdma_mgr.h>

#define CLR_BIT(x) (~(0x01 << (x)))
#define CLR_BITS(x, y) ((~((0x01 << (y)) - 1)) << (x))
#define SET_BIT(x) (0x01 << (x))
#define GET_BIT(x) (0x01 << (x))
#define GET_BITS(x, y) (((0x01 << (y)) - 1) << (x))

#define srsharp0_sharp_hvsize 0x3e00
#define srsharp0_pkosht_vsluma_lut_h 0x3e81
#define srsharp1_sharp_hvsize 0x3f00
#define srsharp1_pkosht_vsluma_lut_h 0x3f81
#define srsharp1_lc_input_mux 0x3fb1
#define srsharp1_lc_map_ram_data 0x3ffe

/* useful inline functions to handle different offset */
static inline bool cpu_after_eq_t7(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_T7);
}

static inline bool cpu_after_eq_tm2b(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_TM2);
}

static inline bool cpu_after_eq_tl1(void)
{
	return cpu_after_eq(MESON_CPU_MAJOR_ID_TL1);
}

static inline bool is_sr0_reg(u32 addr)
{
	return (addr >= srsharp0_sharp_hvsize &&
		addr <= srsharp0_pkosht_vsluma_lut_h);
}

static inline bool is_sr1_reg(u32 addr)
{
	return (addr >= srsharp1_sharp_hvsize &&
		addr <= srsharp1_pkosht_vsluma_lut_h);
}

static inline bool is_lc_reg(u32 addr)
{
	return (addr >= srsharp1_lc_input_mux &&
		addr <= srsharp1_lc_map_ram_data);
}

static inline bool is_sr0_dnlpv2_reg(u32 addr)
{
	/*because s5 have no sr0 dnlp
	 *old sr0 reg overlap with slice3 hdr reg
	 */
	if (chip_type_id == chip_s5)
		return 0;

	return (addr >= SRSHARP0_DNLP2_00 &&
		addr <= SRSHARP0_DNLP2_31);
}

static inline bool is_sr1_dnlpv2_reg(u32 addr)
{
	return (addr >= SRSHARP1_DNLP2_00 &&
		addr <= SRSHARP1_DNLP2_31);
}

static inline u32 get_sr0_offset(void)
{
	/*sr0  register shfit*/
	if (cpu_after_eq_t7())
		return 0x1200;
	else if (cpu_after_eq_tm2b())
		return 0x1200;
	else if (cpu_after_eq_tl1())
		return 0x0;
	else if (is_meson_g12a_cpu() ||
		 is_meson_g12b_cpu() ||
		is_meson_sm1_cpu())
		return 0x0;

	return 0xfffff400 /*-0xc00*/;
}

static inline u32 get_sr1_offset(void)
{
	/*sr1 register shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;
	else if (cpu_after_eq_tl1())
		return 0x0;
	else if (is_meson_g12a_cpu() ||
		 is_meson_g12b_cpu() ||
		is_meson_sm1_cpu())
		return 0x0;

	return 0xfffff380; /*-0xc80*/;
}

static inline u32 get_lc_offset(void)
{
	/* lc register shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;

	return 0;
}

static inline u32 get_sr0_dnlp2_offset(void)
{
	/* SHARP0_DNLP_00 shfit*/
	if (cpu_after_eq_t7())
		return 0x1200;
	else if (cpu_after_eq_tm2b())
		return 0x1200;

	return 0;
}

static inline u32 get_sr1_dnlp2_offset(void)
{
	/* SHARP1_DNLP_00 shfit*/
	if (cpu_after_eq_t7())
		return 0x1300;
	else if (cpu_after_eq_tm2b())
		return 0x1300;

	return 0;
}

static u32 offset_addr(u32 addr)
{
	unsigned int lc_offset = 0;

	if (chip_type_id == chip_s5)
		return addr;

	if (chip_type_id == chip_txhd2)
		lc_offset = 0x200;

	if (chip_type_id == chip_t6x) {
		if (addr >= VPP_SR_DIR_MIN2SAD_GAMMA_THD &&
			addr <= VPP_HLTI_OS_ADP_LUT_F)
			return addr + 0x1b;
		else if (addr >= VPP_HTI_OS_UP_DN_GAIN &&
			addr <= VPP_HCTI_OS_ADP_LUT_F)
			return addr + 0x1e;
		else if (addr >= VPP_HTI_FINAL_GAIN &&
			addr <= VPP_VLTI_OS_ADP_LUT_F)
			return addr + 0x21;
		else if (addr >= VPP_VTI_OS_UP_DN_GAIN &&
			addr <= VPP_VCTI_BST_GAIN_CORE)
			return addr + 0x24;
		else if (addr >= VPP_HVTI_BLEND_ALPHA_0 &&
			addr <= VPP_PK_DIR_HBP_BLD_ALP_LUT_F)
			return addr + 0x27;
		else if (addr >= VPP_PK_CIR_HBP_BLD_ALP_LUT_0 &&
			addr <= VPP_PK_CIR_HBP_BLD_ALP_LUT_F)
			return addr + 0x2a;
		else if (addr >= VPP_PK_DIR_GAIN_LUT_0 &&
			addr <= VPP_PK_DIR_CIR_BLD_ALP_GAIN)
			return addr + 0x2d;
		else if (addr >= VPP_PK_GAIN_VSLUMA_LUT_0 &&
			addr <= VPP_PK_COLOR_PRCT_LUT_30)
			return addr + 0x2e;
		else if (addr >= VPP_PK_COLOR_PRCT_LUT_31 &&
			addr <= VPP_PK_COLOR_PRCT_GAIN)
			return addr + 0x272e;
		else if (addr == VPP_PK_OS_EN_SEL_MODE)
			return VPP_OS_EN_SEL_MODE;
		else if (addr >= VPP_PK_OS_ADP_LUT_0 &&
			addr <= VPP_PK_OS_EDGE_GAIN_THD)
			return addr + 0x2738;
		else if (addr >= VPP_PK_OS_GAIN_VSLUMA_LUT_0 &&
			addr <= VPP_PK_OS_UP_DN)
			return addr + 0x272e;
		else if (addr >= VPP_PKTI_EDGE_STR_GAIN_0 &&
			addr <= VPP_PKTI_EDGE_STR_GAIN_F)
			return addr + 0x2724;
		else if (addr >= VPP_SR_CC_EN &&
			addr <= VPP_SR_DEBUG_DEMO_WND_COEF_0)
			return addr + 0x2765;
	}

	if (is_sr0_reg(addr))
		return addr + get_sr0_offset();
	else if (is_sr1_reg(addr))
		return addr + get_sr1_offset();
	else if (is_sr0_dnlpv2_reg(addr))
		return addr + get_sr0_dnlp2_offset();
	else if (is_sr1_dnlpv2_reg(addr))
		return addr + get_sr1_dnlp2_offset();
	else if (is_lc_reg(addr))
		return addr + get_lc_offset() - lc_offset;

	return addr;
}

static inline void WRITE_VPP_REG(u32 reg,
				 const u32 value)
{
	if (!reg)
		return;

	aml_write_vcbus_s(offset_addr(reg), value);
}

static inline void WRITE_VPP_REG_S5(u32 reg,
				 const u32 value)
{
	aml_write_vcbus(reg, value);
}

static inline u32 READ_VPP_REG(u32 reg)
{
	if (!reg)
		return 0;

	return aml_read_vcbus_s(offset_addr(reg));
}

static inline u32 READ_VPP_REG_S5(u32 reg)
{
	return aml_read_vcbus(reg);
}

static inline void WRITE_VPP_REG_EX(u32 reg,
				    const u32 value,
				    bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);

	if (!reg)
		return;

	aml_write_vcbus_s(reg, value);
}

static inline u32 READ_VPP_REG_EX(u32 reg,
				  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);

	if (!reg)
		return 0;

	return aml_read_vcbus_s(reg);
}

static inline void WRITE_VPP_REG_BITS(u32 reg,
				      const u32 value,
		const u32 start,
		const u32 len)
{
	if (!reg)
		return;

	aml_vcbus_update_bits_s(offset_addr(reg), value, start, len);
}

static inline void WRITE_VPP_REG_BITS_S5(u32 reg,
				      const u32 value,
		const u32 start,
		const u32 len)
{
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

static inline u32 READ_VPP_REG_BITS(u32 reg,
				    const u32 start,
				    const u32 len)
{
	u32 val;
	u32 reg1 = offset_addr(reg);

	if (!reg)
		return 0;

	val = ((aml_read_vcbus_s(reg1) >> (start)) & ((1L << (len)) - 1));

	return val;
}

static inline void WRITE_VPP_REG_SEL(u32 reg,
				const u32 value,
				const u32 vpp_sel)
{
	aml_write_vcbus_s(offset_addr(reg), value);
}

static inline u32 READ_VCBUS_REG_SEL(u32 reg, const u32 vpp_sel)
{
	return aml_read_vcbus_s(offset_addr(reg));
}

static inline void WRITE_VCBUS_REG_BITS_SEL(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len,
		const u32 vpp_sel)
{
	aml_vcbus_update_bits_s(offset_addr(reg), value, start, len);
}

#ifndef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#define VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VPP_REG_BITS(adr, val, start, len)

#define _VSYNC_WR_MPEG_REG(adr, val) WRITE_VPP_REG(adr, val)
#define _VSYNC_RD_MPEG_REG(adr) READ_VPP_REG(adr)
#define _VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
		WRITE_VPP_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP1(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP1(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP1(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP2(adr, val) WRITE_VCBUS_REG(adr, val)
#define VSYNC_RD_MPEG_REG_VPP2(adr) READ_VCBUS_REG(adr)
#define VSYNC_WR_MPEG_REG_BITS_VPP2(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define PRE_VSYNC_WR_MPEG_REG(adr, val) WRITE_VCBUS_REG(adr, val)
#define PRE_VSYNC_RD_MPEG_REG(adr) READ_VCBUS_REG(adr)
#define PRE_VSYNC_WR_MPEG_REG_BITS(adr, val, start, len) \
	WRITE_VCBUS_REG_BITS(adr, val, start, len)

#define VSYNC_WR_MPEG_REG_VPP_SEL(adr, val, vpp_sel) \
	WRITE_VPP_REG_SEL(adr, val, vpp_sel)
#define VSYNC_RD_MPEG_REG_VPP_SEL(adr, vpp_sel) \
	READ_VCBUS_REG_SEL(adr, vpp_sel)
#define VSYNC_WR_MPEG_REG_BITS_VPP_SEL(adr, val, start, len, vpp_sel) \
	WRITE_VCBUS_REG_BITS_SEL(adr, val, start, len, vpp_sel)

#else
int VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG(u32 adr);
int VSYNC_WR_MPEG_REG(u32 adr, u32 val);

int _VSYNC_WR_MPEG_REG(u32 adr, u32 val);
int _VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 _VSYNC_RD_MPEG_REG(u32 adr);

int VSYNC_WR_MPEG_REG_BITS_VPP1(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP1(u32 adr);
int VSYNC_WR_MPEG_REG_VPP1(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP2(u32 adr, u32 val, u32 start, u32 len);
u32 VSYNC_RD_MPEG_REG_VPP2(u32 adr);
int VSYNC_WR_MPEG_REG_VPP2(u32 adr, u32 val);

int PRE_VSYNC_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 PRE_VSYNC_RD_MPEG_REG(u32 adr);
int PRE_VSYNC_WR_MPEG_REG(u32 adr, u32 val);

int VSYNC_WR_MPEG_REG_BITS_VPP_SEL(u32 adr, u32 val, u32 start, u32 len, int vpp_sel);
u32 VSYNC_RD_MPEG_REG_VPP_SEL(u32 adr, int vpp_sel);
int VSYNC_WR_MPEG_REG_VPP_SEL(u32 adr, u32 val, int vpp_sel);

#endif

#define DPSS_RMDA (0)

#if DPSS_RMDA
int DPSS_B_WR_MPEG_REG(u32 adr, u32 val);
int DPSS_B_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len);
u32 DPSS_B_RD_MPEG_REG(u32 adr);
#endif

/* table1:hdr; table2:sr dnlp lc;table3:other modules*/
static int index_rdma_part_ins(u32 reg)
{
	int table_index = 3;

	if ((reg >= 0x3800 && reg <= 0x384c) ||
		(reg >= 0x3850 && reg <= 0x389c) ||
		(reg >= 0x5930 && reg <= 0x5971) ||
		(reg >= 0x6000 && reg <= 0x603f) ||
		(reg >= 0x6200 && reg <= 0x624c) ||
		(reg >= 0x6280 && reg <= 0x62cc) ||
		(reg >= 0x38f0 && reg <= 0x38fd) ||
		(reg >= 0x38a0 && reg <= 0x38df) ||
		(reg >= 0x5b00 && reg <= 0x5b3f) ||
		(reg >= 0x5b50 && reg <= 0x5b8f) ||
		(reg >= 0x3290 && reg <= 0x329d) ||
		(reg >= 0x39a0 && reg <= 0x39ad) ||
		(reg >= 0x32b0 && reg <= 0x32bd) ||
		(reg >= 0x5990 && reg <= 0x599d) ||
		(reg >= 0x59d0 && reg <= 0x59dd) ||
		(reg >= 0x3d60 && reg <= 0x3d6d) ||
		(reg >= 0x0259 && reg <= 0x0266) ||
		(reg >= 0x3920 && reg <= 0x392d) ||
		(reg >= 0x4c00 && reg <= 0x4c4f))
		table_index = 1;

	if ((reg >= 0x5000 && reg <= 0x5081) ||
		(reg >= 0x5200 && reg <= 0x5281) ||
		(reg >= 0x5090 && reg <= 0x50af) ||
		(reg >= 0x5290 && reg <= 0x52af) ||
		(reg >= 0x52b1 && reg <= 0x52fe) ||
		(reg >= 0x3e00 && reg <= 0x3e81) ||
		(reg >= 0x3f00 && reg <= 0x3f81) ||
		(reg >= 0x3e90 && reg <= 0x3eaf) ||
		(reg >= 0x3f90 && reg <= 0x3faf) ||
		(reg >= 0x3fb1 && reg <= 0x3ffe) ||
		(reg >= 0x50b1 && reg <= 0x50fe) ||
		(reg >= 0x7500 && reg <= 0x75b0) ||
		reg == 0x508a || reg == 0x508b ||
		reg == 0x5134 || reg == 0x5334 ||
		(reg == 0x52c1 || reg == 0x77c1 ||
		reg == 0x52e2 || reg == 0x77e2 ||
		reg == 0x52e9 || reg == 0x77e9 ||
		reg == 0x52c0 || reg == 0x77c0 ||
		reg == 0x5a40 || reg == 0x5a80 ||
		reg == 0x5a41 || reg == 0x5a81 ||
		reg == 0x5a56 || reg == 0x5a96 ||
		reg == 0x5a57 || reg == 0x5a97 ||
		reg == 0x5ae9 || reg == 0x5aea ||
		reg == 0x5ad9 || reg == 0x5ada ||
		reg == 0x5ad7 || reg == 0x5ad8 ||
		reg == 0x52fc || reg == 0x77fc ||
		reg == 0x52fd || reg == 0x77fd ||
		reg == 0x52fe || reg == 0x77fe) ||
		(reg >= 0x7700 && reg <= 0x77b0))
		table_index = 2;

	if (reg == 0x2880 || reg == 0x2980 ||
		reg == 0x2a80 || reg == 0x32a0 ||
		reg == 0x31a0 || reg == 0x2ba0 ||
		reg == 0x19a0 || reg == 0x3280 ||
		reg == 0x2e80 || reg == 0x1d40 ||
		(reg >= 0x1d6a && reg <= 0x1d6e) ||
		(reg >= 0x39d4 && reg <= 0x39d6) ||
		(reg >= 0x05b4 && reg <= 0x05b6) ||
		(reg >= 0x14e9 && reg <= 0x14eb) ||
		(reg >= 0x1400 && reg <= 0x1402) ||
		(reg >= 0x14b4 && reg <= 0x14b6) ||
		(reg >= 0x1da0 && reg <= 0x1da3) ||
		reg == 0x2e63 || reg == 0x2863 ||
		reg == 0x2763 || reg == 0x2663 ||
		reg == 0x1d70 ||
		reg == 0x1d71 || reg == 0x20f ||
		(reg >= 0x200 && reg <= 0x20a) ||
		reg == 0x39d0 || reg == 0x39d1 ||
		reg == 0x39d2 || reg == 0x39d3)
		table_index = 3;

	if (chip_type_id == chip_t6x) {
		if ((reg >= 0x4c00 && reg <= 0x4cd2) || //vd1 && vd1_1 hdr
			(reg >= 0x6300 && reg <= 0x63d2) || //vd2 && vd2_2 hdr
			(reg >= 0x38a0 && reg <= 0x38ef) || //osd1
			(reg >= 0x5b00 && reg <= 0x5b4f) || //osd2
			(reg >= 0x5b50 && reg <= 0x5b9f)) //osd3
			table_index = 1;
		else if ((reg >= 0x5040 && reg <= 0x5302) ||//pi,safa,sr
			(reg >= 0x7a00 && reg <= 0x7a54))//os,cc
			table_index = 2;
		else if ((reg >= 0x5303 && reg <= 0x5382) ||//dnlp,lc
			(reg >= 0x4000 && reg <= 0x403c))//lc_curve&&stts
			table_index = 4;
	}

	/*use VIDEO_PARTITION_TABLE*/
	if (chip_type_id >= chip_s7d &&
		(reg == 0x5048 || reg == 0x5049 ||
		reg == 0x504d || reg == 0x5100))
		table_index = 0;

	if ((chip_type_id == chip_s7d ||
		chip_type_id == chip_s6) &&
		reg == 0x511e)
		table_index = 0;

	if (chip_type_id == chip_s6 &&
		reg == 0x518a)
		table_index = 0;

	if (chip_type_id == chip_t6d &&
		reg == 0x5125)
		table_index = 0;

	if ((chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) &&
		(reg == 0x5126 ||
		reg == 0x5192 ||
		reg == 0x6d02))
		table_index = 0;

	if (reg == 0x1d26)
		table_index = 0;

	if (chip_type_id == chip_a9 &&
		(reg == 0x5126 ||
		reg == 0x5192))
		table_index = 0;

	return table_index;
}

/* vsync for vpp_top0 */
static inline void VSYNC_WRITE_VPP_REG(u32 reg,
				       const u32 value)
{
	int index;
	u32 reg_offset;

	reg_offset = offset_addr(reg);

	/*table*/
	index = index_rdma_part_ins(reg_offset);

	if (pq_rdma_init)
		VSYNC_WR_TABLE_REG(index, reg_offset, value);
	else
		VSYNC_WR_MPEG_REG(reg_offset, value);
}

static inline u32 VSYNC_READ_VPP_REG(u32 reg)
{
	int index;
	u32 reg_offset;

	reg_offset = offset_addr(reg);

	index = index_rdma_part_ins(reg_offset);

	if (pq_rdma_init)
		return VSYNC_RD_TABLE_REG(index, reg_offset);
	else
		return VSYNC_RD_MPEG_REG(reg_offset);
}

static inline void VSYNC_WRITE_VPP_REG_EX(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);

	if (pq_rdma_init)
		VSYNC_WR_TABLE_REG(index, reg, value);
	else
		VSYNC_WR_MPEG_REG(reg, value);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_EX(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len,
		bool add_offset)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);
	index = index_rdma_part_ins(reg);

	if (pq_rdma_init)
		VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
}

static inline u32 VSYNC_READ_VPP_REG_EX(u32 reg,
					bool add_offset)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);
	if (pq_rdma_init)
		return VSYNC_RD_TABLE_REG(index, reg);
	else
		return VSYNC_RD_MPEG_REG(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	int index;

	reg = offset_addr(reg);
	index = index_rdma_part_ins(reg);

	if (pq_rdma_init)
		VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
	else
		VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
}

/* vsync for vpp_top1 */
static inline void VSYNC_WRITE_VPP_REG_VPP1(u32 reg,
				       const u32 value)
{
	VSYNC_WR_MPEG_REG_VPP1(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG_VPP1(u32 reg)
{
	return VSYNC_RD_MPEG_REG_VPP1(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP1(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	VSYNC_WR_MPEG_REG_VPP1(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP1(u32 reg,
					bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	return VSYNC_RD_MPEG_REG_VPP1(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP1(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	VSYNC_WR_MPEG_REG_BITS_VPP1(offset_addr(reg), value, start, len);
}

/* vsync for vpp_top2 */
static inline void VSYNC_WRITE_VPP_REG_VPP2(u32 reg,
				       const u32 value)
{
	VSYNC_WR_MPEG_REG_VPP2(offset_addr(reg), value);
}

static inline u32 VSYNC_READ_VPP_REG_VPP2(u32 reg)
{
	return VSYNC_RD_MPEG_REG_VPP2(offset_addr(reg));
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP2(u32 reg,
					  const u32 value,
					  bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	VSYNC_WR_MPEG_REG_VPP2(reg, value);
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP2(u32 reg,
					bool add_offset)
{
	if (add_offset)
		reg = offset_addr(reg);
	return VSYNC_RD_MPEG_REG_VPP2(reg);
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP2(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len)
{
	VSYNC_WR_MPEG_REG_BITS_VPP2(offset_addr(reg), value, start, len);
}

static inline void VSYNC_WR_MPEG_REG_BITS_S5(u32 reg,
		      const u32 value,
		      const u32 start,
		      const u32 len)
{
	int index;

	index = index_rdma_part_ins(reg);
	if (pq_rdma_init)
		VSYNC_WR_TABLE_REG(index, reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
	else
		VSYNC_WR_MPEG_REG(reg, ((aml_read_vcbus(reg) &
			     ~(((1L << (len)) - 1) << (start))) |
			    (((value) & ((1L << (len)) - 1)) << (start))));
}

/* vsync for vpp_top_sel */
static inline void VSYNC_WRITE_VPP_REG_VPP_SEL(u32 reg,
				       const u32 value, int vpp_sel)
{
	int index;
	u32 reg1;

	reg1 = offset_addr(reg);
	index = index_rdma_part_ins(reg1);

	if (vpp_sel == 0xff) {
		aml_write_vcbus_s(reg1, value);
	} else if (vpp_sel == 0xfe) {
		aml_write_vcbus(reg, value);
	} else if (vpp_sel == 3) {
		if (pq_rdma_init)
			PRE_VSYNC_WR_TABLE_REG(index, reg1, value);
		else
			PRE_VSYNC_WR_MPEG_REG(reg1, value);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_VPP2(reg1, value);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_VPP1(reg1, value);
#if DPSS_RMDA
	} else if (vpp_sel == 4) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		DPSS_B_WR_MPEG_REG(offset_addr(reg), value);
#endif
#endif
	} else {
		if (pq_rdma_init)
			VSYNC_WR_TABLE_REG(index, reg1, value);
		else
			VSYNC_WR_MPEG_REG(reg1, value);
	}
}

static inline void VSYNC_WRITE_VPP_REG_VPP_SEL_LUT(u32 reg,
				       const u32 value, int vpp_sel)
{
	int index;
	u32 reg1;

	reg1 = offset_addr(reg);
	index = index_rdma_part_ins(reg1);

	if (vpp_sel == 0xff) {
		aml_write_vcbus_s(reg1, value);
	} else if (vpp_sel == 0xfe) {
		aml_write_vcbus(reg, value);
	} else if (vpp_sel == 3) {
		if (pq_rdma_init)
			PRE_VSYNC_WR_TABLE_REG(index, reg1, value);
		else
			PRE_VSYNC_WR_MPEG_REG(reg1, value);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_VPP2(reg1, value);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_VPP1(reg1, value);
	} else {
		if (pq_rdma_init)
			VSYNC_WR_TABLE_REG_SIMPLE(index, reg1, value);
		else
			VSYNC_WR_MPEG_REG(reg1, value);
	}
}

static inline u32 VSYNC_READ_VPP_REG_VPP_SEL(u32 reg, int vpp_sel)
{
	int index;
	u32 reg1;
	u32 ret = 0;

	reg1 = offset_addr(reg);
	index = index_rdma_part_ins(reg1);

	if (vpp_sel == 0xff) {
		ret = aml_read_vcbus_s(reg1);
	} else if (vpp_sel == 0xfe) {
		ret = aml_read_vcbus(reg);
	} else if (vpp_sel == 3) {
		if (pq_rdma_init)
			ret = PRE_VSYNC_RD_TABLE_REG(index, reg1);
		else
			ret = PRE_VSYNC_RD_MPEG_REG(reg1);
	} else if (vpp_sel == 2) {
		ret = VSYNC_RD_MPEG_REG_VPP2(reg1);
	} else if (vpp_sel == 1) {
		ret = VSYNC_RD_MPEG_REG_VPP1(reg1);
#if DPSS_RMDA
	} else if (vpp_sel == 4) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		ret = DPSS_B_RD_MPEG_REG(offset_addr(reg));
#endif
#endif
	} else {
		if (pq_rdma_init)
			ret = VSYNC_RD_TABLE_REG(index, reg1);
		else
			ret = VSYNC_RD_MPEG_REG(reg1);
	}

	return ret;
}

static inline void VSYNC_WRITE_VPP_REG_BITS_VPP_SEL(u32 reg,
					    const u32 value,
		const u32 start,
		const u32 len, int vpp_sel)
{
	int index;
	u32 reg1;

	reg1 = offset_addr(reg);
	index = index_rdma_part_ins(reg1);

	if (vpp_sel == 0xff) {
		aml_vcbus_update_bits_s(reg1, value, start, len);
	} else if (vpp_sel == 0xfe) {
		VSYNC_WR_MPEG_REG_BITS_S5(reg, value, start, len);
	} else if (vpp_sel == 3) {
		if (pq_rdma_init)
			PRE_VSYNC_WR_TABLE_REG_BITS(index, reg1, value, start, len);
		else
			PRE_VSYNC_WR_MPEG_REG_BITS(reg1, value, start, len);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_BITS_VPP2(reg1, value, start, len);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_BITS_VPP1(reg1, value, start, len);
#if DPSS_RMDA
	} else if (vpp_sel == 4) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		DPSS_B_WR_MPEG_REG_BITS(offset_addr(reg), value, start, len);
#endif
#endif
	} else {
		if (pq_rdma_init)
			VSYNC_WR_TABLE_REG_BITS(index, reg1, value, start, len);
		else
			VSYNC_WR_MPEG_REG_BITS(reg1, value, start, len);
	}
}

static inline void VSYNC_WRITE_VPP_REG_EX_VPP_SEL(u32 reg,
					  const u32 value,
					  bool add_offset, int vpp_sel)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);

	if (vpp_sel == 3) {
		if (pq_rdma_init)
			PRE_VSYNC_WR_TABLE_REG(index, reg, value);
		else
			PRE_VSYNC_WR_MPEG_REG(reg, value);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_VPP2(reg, value);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_VPP1(reg, value);
#if DPSS_RMDA
	} else if (vpp_sel == 4) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		DPSS_B_WR_MPEG_REG(reg, value);
#endif
#endif
	} else {
		if (pq_rdma_init)
			VSYNC_WR_TABLE_REG(index, reg, value);
		else
			VSYNC_WR_MPEG_REG(reg, value);
	}
}

static inline u32 VSYNC_READ_VPP_REG_EX_VPP_SEL(u32 reg,
					bool add_offset, int vpp_sel)
{
	int index;
	u32 ret = 0;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);

	if (vpp_sel == 3) {
		if (pq_rdma_init)
			ret = PRE_VSYNC_RD_TABLE_REG(index, reg);
		else
			ret = PRE_VSYNC_RD_MPEG_REG(reg);
	} else if (vpp_sel == 2) {
		ret = VSYNC_RD_MPEG_REG_VPP2(reg);
	} else if (vpp_sel == 1) {
		ret = VSYNC_RD_MPEG_REG_VPP1(reg);
#if DPSS_RMDA
	} else if (vpp_sel == 4) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		ret = DPSS_B_RD_MPEG_REG(reg);
#endif
#endif
	} else {
		if (pq_rdma_init)
			ret = VSYNC_RD_TABLE_REG(index, reg);
		else
			ret = VSYNC_RD_MPEG_REG(reg);
	}

	return ret;
}

static inline void VSYNC_WRITE_VPP_REG_BITS_EX_VPP_SEL(u32 reg,
		const u32 value,
		const u32 start,
		const u32 len,
		bool add_offset,
		int vpp_sel)
{
	int index;

	if (add_offset)
		reg = offset_addr(reg);

	index = index_rdma_part_ins(reg);
	if (vpp_sel == 3) {
		if (pq_rdma_init)
			PRE_VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
		else
			PRE_VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
	} else if (vpp_sel == 2) {
		VSYNC_WR_MPEG_REG_BITS_VPP2(reg, value, start, len);
	} else if (vpp_sel == 1) {
		VSYNC_WR_MPEG_REG_BITS_VPP1(reg, value, start, len);
#if DPSS_RMDA
	} else if (vpp_sel == 4) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		DPSS_B_WR_MPEG_REG_BITS(reg, value, start, len);
#endif
#endif
	} else {
		if (pq_rdma_init)
			VSYNC_WR_TABLE_REG_BITS(index, reg, value, start, len);
		else
			VSYNC_WR_MPEG_REG_BITS(reg, value, start, len);
	}
}

#endif
