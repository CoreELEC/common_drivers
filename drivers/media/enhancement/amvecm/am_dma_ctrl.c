// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include "am_dma_ctrl.h"
#include "reg_helper.h"

int am_dma_ctrl_dbg;

#define pr_am_dma(fmt, args...)\
	do {\
		if (am_dma_ctrl_dbg & 0x1) {\
			pr_info("am_dma_ctrl: " fmt, ## args);\
		} \
	} while (0)\

#define pr_am_dma_vi(fmt, args...)\
	do {\
		if (am_dma_ctrl_dbg & 0x2) {\
			pr_info("am_dma_ctrl: " fmt, ## args);\
		} \
	} while (0)\

#define pr_am_dma_hdr2(fmt, args...)\
	do {\
		if (am_dma_ctrl_dbg & 0x4) {\
			pr_info("am_dma_ctrl: " fmt, ## args);\
		} \
	} while (0)\

#define pr_am_dma_lc(fmt, args...)\
	do {\
		if (am_dma_ctrl_dbg & 0x8) {\
			pr_info("am_dma_ctrl: " fmt, ## args);\
		} \
	} while (0)\

#define pr_am_dma_lut3d(fmt, args...)\
	(do {\
		if (am_dma_ctrl_dbg & 0x10) {\
			pr_info("am_dma_ctrl: " fmt, ## args);\
		} \
	} while (0))\

#define ADDR_PARAM(page, reg)  (((page) << 8) | (reg))
#define UNIT_SIZE (16) /*128/8*/

#define LUT_DMA_AUTO_ADDR_STEP_WR_0  0x2808

#define VPU_DMA_RDMIF0_CTRL   0x2750
#define VPU_DMA_RDMIF1_CTRL   0x2751
#define VPU_DMA_RDMIF2_CTRL   0x2752
#define VPU_DMA_RDMIF3_CTRL   0x2753
#define VPU_DMA_RDMIF4_CTRL   0x2754
#define VPU_DMA_RDMIF5_CTRL   0x2755
#define VPU_DMA_RDMIF6_CTRL   0x2756
#define VPU_DMA_RDMIF7_CTRL   0x2757

#define VPU_DMA_RDMIF0_BADR0  0x2758
#define VPU_DMA_RDMIF0_BADR1  0x2759
#define VPU_DMA_RDMIF0_BADR2  0x275a
#define VPU_DMA_RDMIF0_BADR3  0x275b
#define VPU_DMA_RDMIF1_BADR0  0x275c
#define VPU_DMA_RDMIF1_BADR1  0x275d
#define VPU_DMA_RDMIF1_BADR2  0x275e
#define VPU_DMA_RDMIF1_BADR3  0x275f
#define VPU_DMA_RDMIF2_BADR0  0x2760
#define VPU_DMA_RDMIF2_BADR1  0x2761
#define VPU_DMA_RDMIF2_BADR2  0x2762
#define VPU_DMA_RDMIF2_BADR3  0x2763
#define VPU_DMA_RDMIF3_BADR0  0x2764
#define VPU_DMA_RDMIF3_BADR1  0x2765
#define VPU_DMA_RDMIF3_BADR2  0x2766
#define VPU_DMA_RDMIF3_BADR3  0x2767
#define VPU_DMA_RDMIF4_BADR0  0x2768
#define VPU_DMA_RDMIF4_BADR1  0x2769
#define VPU_DMA_RDMIF4_BADR2  0x276a
#define VPU_DMA_RDMIF4_BADR3  0x276b
#define VPU_DMA_RDMIF5_BADR0  0x276c
#define VPU_DMA_RDMIF5_BADR1  0x276d
#define VPU_DMA_RDMIF5_BADR2  0x276e
#define VPU_DMA_RDMIF5_BADR3  0x276f
#define VPU_DMA_RDMIF6_BADR0  0x2770
#define VPU_DMA_RDMIF6_BADR1  0x2771
#define VPU_DMA_RDMIF6_BADR2  0x2772
#define VPU_DMA_RDMIF6_BADR3  0x2773
#define VPU_DMA_RDMIF7_BADR0  0x2774
#define VPU_DMA_RDMIF7_BADR1  0x2775
#define VPU_DMA_RDMIF7_BADR2  0x2776
#define VPU_DMA_RDMIF7_BADR3  0x2777
#define VPU_DMA_RDMIF_SEL     0x2778

#define VPU_DMA_WRMIF_CTRL1   0x27d1
#define VPU_DMA_WRMIF_CTRL2   0x27d2
#define VPU_DMA_WRMIF_CTRL3   0x27d3
#define VPU_DMA_WRMIF_BADDR0  0x27d4
#define VPU_DMA_WRMIF_RO_STAT 0x27d7
#define VPU_DMA_RDMIF_CTRL    0x27d8
#define VPU_DMA_RDMIF_BADDR1  0x27d9
#define VPU_DMA_RDMIF_BADDR2  0x27da
#define VPU_DMA_RDMIF_BADDR3  0x27db
#define VPU_DMA_WRMIF_CTRL    0x27dc
#define VPU_DMA_WRMIF_BADDR1  0x27dd
#define VPU_DMA_WRMIF_BADDR2  0x27de
#define VPU_DMA_WRMIF_BADDR3  0x27df

#define VPU_DMA_RDMIF_CTRL1   0x27ca
#define VPU_DMA_RDMIF_CTRL2   0x27cb
#define VPU_DMA_RDMIF_RO_STAT 0x27d0

struct vpu_lut_dma_wr_s {
	enum lut_dma_wr_id_e dma_wr_id;
	u32 stride;
	u64 baddr0;
	u64 baddr1;
	u32 addr_mode;
	u32 rpt_num;
	u32 step;
	int auto_addr_en;
};

struct vpu_lut_dma_rd_s {
	enum lut_dma_rd_id_e dma_rd_id;
	u32 stride;
	u64 baddr0;
	u64 baddr1;
	u64 baddr2;
	u64 baddr3;
};

struct _dma_reg_cfg_s {
	unsigned char page;
	unsigned char reg_wrmif_ctrl;
	unsigned char reg_wrmif0_ctrl;
	unsigned char reg_wrmif0_badr0;
	unsigned char reg_wrmif0_badr1;
	unsigned char reg_wrmif_sel;
	unsigned char reg_sr_mode_l2c1;
	unsigned char reg_rdmif0_ctrl;
	unsigned char reg_rdmif0_badr0;
	unsigned char reg_rdmif0_badr1;
	unsigned char reg_rdmif0_badr2;
	unsigned char reg_rdmif0_badr3;
	unsigned char reg_rdmif_sel;
	unsigned char reg_wrmif_ctrl2;
};

struct _viu_dma_reg_cfg_s {
	unsigned char page;
	unsigned char reg_ctrl0;
	unsigned char reg_ctrl1;
};

static struct _dma_reg_cfg_s dma_reg_cfg = {
	.page = 0x27,
	.reg_wrmif_ctrl = 0xd5,
	.reg_wrmif0_ctrl = 0xd6,
	.reg_wrmif0_badr0 = 0xde,
	.reg_wrmif0_badr1 = 0xdf,
	.reg_wrmif_sel = 0xee,
	.reg_sr_mode_l2c1 = 0xa2,
	.reg_rdmif0_ctrl = 0x50,
	.reg_rdmif0_badr0 = 0x58,
	.reg_rdmif0_badr1 = 0x59,
	.reg_rdmif0_badr2 = 0x5a,
	.reg_rdmif0_badr3 = 0x5b,
	.reg_rdmif_sel = 0x78,
	.reg_wrmif_ctrl2 = 0xce,
};

static struct _viu_dma_reg_cfg_s viu_dma_reg_cfg = {
	0x1a,
	0x28,
	0x29,
};

static struct device vecm_dev;
static ulong alloc_size[EN_DMA_WR_ID_MAX];
static dma_addr_t dma_paddr[EN_DMA_WR_ID_MAX];
static dma_addr_t dma_rd_paddr[EN_DMA_RD_ID_MAX];
static void *dma_vaddr[EN_DMA_WR_ID_MAX];
static void *dma_rd_vaddr[EN_DMA_RD_ID_MAX];
static struct vpu_lut_dma_wr_s lut_dma_wr[EN_DMA_WR_ID_MAX];
static struct vpu_lut_dma_rd_s lut_dma_rd[EN_DMA_RD_ID_MAX];
static unsigned int dma_count[EN_DMA_WR_ID_MAX] = {
	12 * 8 * 4, 12 * 8 * 4,
	22, 22,
	16, 16,
	26, 26, 26,
	8 * 80,
};

static unsigned int dma_rd_count[EN_DMA_RD_ID_MAX];

static void _set_vpu_lut_dma_mif_wr_unit(int enable,
	struct vpu_lut_dma_wr_s *cfg_data)
{
	unsigned int addr;
	unsigned int val;
	int wr_sel;
	int offset;
	unsigned int addr_offset = 0;
	unsigned int start_offset = 0;

	if (!cfg_data) {
		pr_info("%s: cfg data is NULL.\n", __func__);
		return;
	}

	if (cfg_data->dma_wr_id < 8) {
		wr_sel = 0;
		offset = cfg_data->dma_wr_id;
	} else {
		wr_sel = 1;
		offset = cfg_data->dma_wr_id - 8;
	}

	/*
	 * Bit 31 ro_lut_wr_hs_r unsigned, RO, default = 0
	 * Bit 30:27 ro_lut_wr_id unsigned, RO, default = 0
	 * Bit 26 pls_hold_clr unsigned, pls, default = 0
	 * Bit 25 ro_hold_timeout unsigned, RO, default = 0
	 * Bit 24:12 ro_wrmif_cnt unsigned, RO, default = 0
	 * Bit 11:4 reg_hold_line unsigned, RW, default = 0xff
	 * Bit 3 ro_lut_wr_hs_s unsigned, RO, default = 0
	 * Bit 2 wrmif_hw_reset unsigned, RW, default = 0
	 * Bit 1 lut_wr_cnt_sel unsigned, RW, default = 0
	 * Bit 0 lut_wr_reg_sel unsigned, RW, default = 0,
	 *       0: sel lut0-7; 1: sel lut8-15
	 */
	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif_sel);
	val = 0xff0 + wr_sel;
	WRITE_VPP_REG_S5(addr, val);

	pr_am_dma("%s: reg = %x, val = %x\n",
		__func__, addr, val);

	/*
	 * Bit 31 reserved
	 * Bit 30 reg_wr0_clr_fcnt unsigned, RW, default = 0
	 * Bit 29:28 reg_wr0_addr_mode unsigned, RW, default = 0
	 *           3 = only addr0, 0 = addr0/1 alternate
	 * Bit 27 reg_wr0_frm_set unsigned, RW, default = 0
	 * Bit 26 reg_wr0_frm_force unsigned, RW, default = 0
	 * Bit 25:18 reg_wr0_rpt_num unsigned, RW, default = 0
	 * Bit 17 reserved
	 * Bit 16 reg_wr0_enable unsigned, RW, default = 0 channel0
	 * Bit 15 reserved
	 * Bit 14 reg_wr0_swap_64bit unsigned, RW, default = 0
	 * Bit 13 reg_wr0_little_endian unsigned, RW, default = 0
	 * Bit 12:0 reg_wr0_stride unsigned, RW, default = 0x200
	 */
	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif0_ctrl) + offset;
	val = READ_VPP_REG_S5(addr);

	val |= ((cfg_data->addr_mode & 0x3) << 28) |
		((cfg_data->rpt_num & 0xff) << 18) |
		((enable & 0x1) << 16) |
		(cfg_data->stride & 0x1fff);
	WRITE_VPP_REG_BITS_S5(addr, cfg_data->addr_mode, 28, 2);
	WRITE_VPP_REG_BITS_S5(addr, cfg_data->rpt_num, 18, 8);
	WRITE_VPP_REG_BITS_S5(addr, enable, 16, 1);
	WRITE_VPP_REG_BITS_S5(addr, cfg_data->stride, 0, 13);

	pr_am_dma("%s: reg = %x, val = %x\n",
		__func__, addr, val);

	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif0_badr0) + (offset << 1);
	WRITE_VPP_REG_S5(addr, cfg_data->baddr0);

	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif0_badr1) + (offset << 1);
	WRITE_VPP_REG_S5(addr, cfg_data->baddr1);

	if (chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x) {
		addr_offset = cfg_data->dma_wr_id >> 1;
		start_offset = 16 * (cfg_data->dma_wr_id & 0x1);
		addr = LUT_DMA_AUTO_ADDR_STEP_WR_0 + addr_offset;
		WRITE_VPP_REG_BITS_S5(addr, cfg_data->step, start_offset, 15);
		WRITE_VPP_REG_BITS_S5(addr, cfg_data->auto_addr_en, 31, 1);
	}
}

static void _set_vpu_lut_dma_mif_rd_unit(int enable,
	struct vpu_lut_dma_rd_s *cfg_data)
{
	unsigned int addr;
	unsigned int val;
	int wr_sel;
	int offset;

	if (!cfg_data) {
		pr_info("%s: cfg data is NULL.\n", __func__);
		return;
	}

	if (cfg_data->dma_rd_id < 8) {
		wr_sel = 0;
		offset = cfg_data->dma_rd_id;
	} else {
		wr_sel = 1;
		offset = cfg_data->dma_rd_id - 8;
	}

	/*
	 * Bit 0 lut_reg_sel unsigned, RW, default = 0,
	 *       0: sel lut0-7; 1: sel lut8-15
	 */
	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif_sel);
	val =  wr_sel;
	WRITE_VPP_REG_S5(addr, val);

	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif0_ctrl) + offset;
	val = READ_VPP_REG_S5(addr);
	val |=  ((enable & 0xff) << 16) |
		(cfg_data->stride & 0x1fff);
	WRITE_VPP_REG_BITS_S5(addr, enable, 16, 8);
	WRITE_VPP_REG_BITS_S5(addr, cfg_data->stride, 0, 13);

	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif0_badr0) + (offset << 2);
	WRITE_VPP_REG_S5(addr, cfg_data->baddr0);
	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif0_badr1) + (offset << 2);
	WRITE_VPP_REG_S5(addr, cfg_data->baddr1);
	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif0_badr2) + (offset << 2);
	WRITE_VPP_REG_S5(addr, cfg_data->baddr2);
	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif0_badr3) + (offset << 2);
	WRITE_VPP_REG_S5(addr, cfg_data->baddr3);

    /*gamut0 dma and dolby dma use different trigger, gamut0 dma (lut0) has
     *a higher priority than amldolby dma (lut3, lut4). so err_rst needs to be 0
     */
	//if (cfg_data->dma_rd_id == EN_DMA_RD_ID_GAMUT0 && enable)
		//WRITE_VPP_REG_BITS_S5(VPU_DMA_RDMIF_CTRL2, 0, 29, 1);
}

void am_dma_init(void)
{
	if (chip_type_id == chip_t6x) {
		/*dma packet: cm2_hist+lc_hist+lc_curve*/
		dma_count[EN_DMA_WR_ID_CM2_LC] = 16 + 384 + 48;
		lut_dma_wr[EN_DMA_WR_ID_CM2_LC].stride = 0;
		/*1 = disable alternate, 0 = addr0/1 alternate*/
		lut_dma_wr[EN_DMA_WR_ID_CM2_LC].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_CM2_LC].rpt_num = 0;
		lut_dma_wr[EN_DMA_WR_ID_CM2_LC].step = 0;
		lut_dma_wr[EN_DMA_WR_ID_CM2_LC].auto_addr_en = 0;

		/*dma packet: vd1/vd2 hdr_hist*/
		dma_count[EN_DMA_WR_ID_VD1_HDR_HIST] = 26; //2*2
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].stride = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].rpt_num = 0;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].step = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].auto_addr_en = 0;

		dma_count[EN_DMA_WR_ID_VD2_HDR_HIST] = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR_HIST].stride = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR_HIST].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR_HIST].rpt_num = 0;
		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR_HIST].step = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR_HIST].auto_addr_en = 0;

		dma_rd_count[EN_DMA_RD_ID_GAMUT0] = 57;
		lut_dma_rd[EN_DMA_RD_ID_GAMUT0].stride = 57;

		dma_rd_count[EN_DMA_RD_ID_GAMUT1] = 38;
		lut_dma_rd[EN_DMA_RD_ID_GAMUT1].stride = 38;

		dma_rd_count[EN_DMA_RD_ID_3DLUT] = 1953;
		lut_dma_rd[EN_DMA_RD_ID_3DLUT].stride = 1953;
	} else if (chip_type_id == chip_t6w) {
		/*dma packet: vd1 hdr_hist*/
		dma_count[EN_DMA_WR_ID_VD1_HDR_HIST] = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].stride = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].addr_mode = 0;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].rpt_num = 0;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].step = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_HIST].auto_addr_en = 0;
	} else if (chip_type_id == chip_t3x) {
		/*lut_dma_wr initial*/
		lut_dma_wr[EN_DMA_WR_ID_LC_STTS_0].stride = 12;/*24;*/
		lut_dma_wr[EN_DMA_WR_ID_LC_STTS_0].addr_mode = 1;
		lut_dma_wr[EN_DMA_WR_ID_LC_STTS_0].rpt_num = 32;/*16;*/

		lut_dma_wr[EN_DMA_WR_ID_LC_STTS_1].stride = 12;
		lut_dma_wr[EN_DMA_WR_ID_LC_STTS_1].addr_mode = 1;
		lut_dma_wr[EN_DMA_WR_ID_LC_STTS_1].rpt_num = 32;

		lut_dma_wr[EN_DMA_WR_ID_VI_HIST_SPL_0].stride = 22;/*2;*/
		lut_dma_wr[EN_DMA_WR_ID_VI_HIST_SPL_0].addr_mode = 3;/*1;*/
		lut_dma_wr[EN_DMA_WR_ID_VI_HIST_SPL_0].rpt_num = 0;/*11;*/

		lut_dma_wr[EN_DMA_WR_ID_VI_HIST_SPL_1].stride = 22;
		lut_dma_wr[EN_DMA_WR_ID_VI_HIST_SPL_1].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_VI_HIST_SPL_1].rpt_num = 0;

		lut_dma_wr[EN_DMA_WR_ID_CM2_HIST_0].stride = 12;
		lut_dma_wr[EN_DMA_WR_ID_CM2_HIST_0].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_CM2_HIST_0].rpt_num = 0;

		lut_dma_wr[EN_DMA_WR_ID_CM2_HIST_1].stride = 12;
		lut_dma_wr[EN_DMA_WR_ID_CM2_HIST_1].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_CM2_HIST_1].rpt_num = 0;

		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_0].stride = 26;/*2;*/
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_0].addr_mode = 3;/*1;*/
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_0].rpt_num = 0;/*13;*/

		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_1].stride = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_1].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_VD1_HDR_1].rpt_num = 0;

		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR].stride = 26;
		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR].addr_mode = 3;
		lut_dma_wr[EN_DMA_WR_ID_VD2_HDR].rpt_num = 0;
	}
}

unsigned int dma_cm_en;
unsigned int dma_lc_en;

int check_dma_status(enum lut_dma_wr_id_e dma_wr_id)
{
	unsigned int reg_val = 0;
	unsigned int ro_hold_timeout = 0;
	unsigned int ro_lut_wr_id = 0;
	unsigned int ro_wrmif_cnt = 0;
	unsigned int cnt_not_match = 0;

	reg_val = READ_VPP_REG(0x27ee);
	ro_lut_wr_id = reg_val >> 27 & 0xf;
	ro_hold_timeout = reg_val >> 25 & 0x1;
	ro_wrmif_cnt = reg_val >> 12 & 0x1fff;

	if (ro_lut_wr_id == dma_wr_id &&
		ro_wrmif_cnt != lut_dma_wr[dma_wr_id].stride)
		cnt_not_match = 1;

//	if (ro_hold_timeout | cnt_not_match)
//		pr_am_dma("%s: reg_val = 0x%x, stride = %d\n",
//			__func__, reg_val, lut_dma_wr[dma_wr_id].stride);

	return ro_hold_timeout | cnt_not_match;
}

void am_dma_update_mif_wr_cm_lc(enum wr_md_e md)
{
	enum lut_dma_wr_id_e dma_wr_id = EN_DMA_WR_ID_CM2_LC;
	unsigned int stride = 0;
	unsigned int step = 0;
	unsigned int enable = 0;
	unsigned int addr;
	unsigned int val;
	int wr_sel;
	int offset;
	unsigned int addr_offset = 0;
	unsigned int start_offset = 0;
	struct vpu_lut_dma_wr_s *cfg_data;

	if (dma_cm_en) {
		stride = 16;
		step = 16;
	}
	if (dma_lc_en) {
		stride += 432;
		step += 432;
	}

	lut_dma_wr[dma_wr_id].stride = stride;
	lut_dma_wr[dma_wr_id].step = step;
	lut_dma_wr[dma_wr_id].dma_wr_id = EN_DMA_WR_ID_CM2_LC;
	enable = stride == 0 ? 0 : 1;

	cfg_data = &lut_dma_wr[dma_wr_id];
	if (cfg_data->dma_wr_id < 8) {
		wr_sel = 0;
		offset = cfg_data->dma_wr_id;
	} else {
		wr_sel = 1;
		offset = cfg_data->dma_wr_id - 8;
	}

	switch (md) {
	case WR_VCB:
		addr = ADDR_PARAM(dma_reg_cfg.page,
			dma_reg_cfg.reg_wrmif_sel);
		val = 0xff0 + wr_sel;
		WRITE_VPP_REG_S5(addr, val);

		addr = ADDR_PARAM(dma_reg_cfg.page,
			dma_reg_cfg.reg_wrmif0_ctrl) + offset;
		WRITE_VPP_REG_BITS_S5(addr, cfg_data->stride, 0, 13);
		WRITE_VPP_REG_BITS_S5(addr, enable, 16, 1);

		if (chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x) {
			addr_offset = cfg_data->dma_wr_id >> 1;
			start_offset = 16 * (cfg_data->dma_wr_id & 0x1);
			addr = LUT_DMA_AUTO_ADDR_STEP_WR_0 + addr_offset;
			WRITE_VPP_REG_BITS_S5(addr, cfg_data->step, start_offset, 15);
			WRITE_VPP_REG_BITS_S5(addr, cfg_data->auto_addr_en, 31, 1);
		}
		addr = ADDR_PARAM(dma_reg_cfg.page,
			dma_reg_cfg.reg_wrmif_ctrl2);//VPU_DMA_WRMIF_CTRL2
		WRITE_VPP_REG_BITS(addr, 1, 28, 1); //soft_rst
		break;
	case WR_DMA:
		addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif_sel);
		val = 0xff0 + wr_sel;
		VSYNC_WRITE_VPP_REG(addr, val);

		addr = ADDR_PARAM(dma_reg_cfg.page,
			dma_reg_cfg.reg_wrmif0_ctrl) + offset;
		VSYNC_WRITE_VPP_REG_BITS(addr, cfg_data->stride, 0, 13);
		VSYNC_WRITE_VPP_REG_BITS(addr, enable, 16, 1);

		if (chip_type_id == chip_t6w ||
			chip_type_id == chip_t6x) {
			addr_offset = cfg_data->dma_wr_id >> 1;
			start_offset = 16 * (cfg_data->dma_wr_id & 0x1);
			addr = LUT_DMA_AUTO_ADDR_STEP_WR_0 + addr_offset;
			VSYNC_WRITE_VPP_REG_BITS(addr, cfg_data->step, start_offset, 15);
			VSYNC_WRITE_VPP_REG_BITS(addr, cfg_data->auto_addr_en, 31, 1);
		}
		addr = ADDR_PARAM(dma_reg_cfg.page,
			dma_reg_cfg.reg_wrmif_ctrl2);//VPU_DMA_WRMIF_CTRL2
		VSYNC_WRITE_VPP_REG_BITS(addr, 1, 28, 1);//soft_rst
		break;
	}
}

void am_dma_buffer_malloc(struct platform_device *pdev,
	enum lut_dma_wr_id_e dma_wr_id)
{

	vecm_dev = pdev->dev;

	if (dma_wr_id == EN_DMA_WR_ID_MAX) {
		return;
	} else {
		alloc_size[dma_wr_id] = dma_count[dma_wr_id] * UNIT_SIZE;
		dma_vaddr[dma_wr_id] = dma_alloc_coherent(&vecm_dev,
			alloc_size[dma_wr_id], &dma_paddr[dma_wr_id], GFP_KERNEL);
		lut_dma_wr[dma_wr_id].dma_wr_id = dma_wr_id;

		if (dma_vaddr[dma_wr_id])
			lut_dma_wr[dma_wr_id].baddr0 =
				(u32)(dma_paddr[dma_wr_id]) >> 4;

		if (dma_wr_id == EN_DMA_WR_ID_VD1_HDR_HIST ||
			dma_wr_id == EN_DMA_WR_ID_VD2_HDR_HIST) {
			dma_vaddr[dma_wr_id + 3] = dma_alloc_coherent(&vecm_dev,
				alloc_size[dma_wr_id], &dma_paddr[dma_wr_id + 3], GFP_KERNEL);
			lut_dma_wr[dma_wr_id].baddr1 =
				(u32)(dma_paddr[dma_wr_id + 3]) >> 4;
		}
	}
}

void am_dma_rd_buffer_malloc(struct platform_device *pdev,
	enum lut_dma_rd_id_e dma_rd_id)
{
	vecm_dev = pdev->dev;

	if (dma_rd_id != EN_DMA_RD_ID_MAX) {
		dma_rd_vaddr[dma_rd_id] = dma_alloc_coherent(&vecm_dev,
			dma_rd_count[dma_rd_id] * UNIT_SIZE,
			&dma_rd_paddr[dma_rd_id], GFP_KERNEL);
		lut_dma_rd[dma_rd_id].dma_rd_id = dma_rd_id;

		if (dma_rd_vaddr[dma_rd_id]) {
			lut_dma_rd[dma_rd_id].baddr0 =
				(u32)(dma_rd_paddr[dma_rd_id]) >> 4;
			lut_dma_rd[dma_rd_id].baddr1 =
				(u32)(dma_rd_paddr[dma_rd_id]) >> 4;
			lut_dma_rd[dma_rd_id].baddr2 =
				(u32)(dma_rd_paddr[dma_rd_id]) >> 4;
			lut_dma_rd[dma_rd_id].baddr3 =
				(u32)(dma_rd_paddr[dma_rd_id]) >> 4;
		}
	}
}

void am_dma_buffer_free(struct platform_device *pdev,
	enum lut_dma_wr_id_e dma_wr_id)
{
	vecm_dev = pdev->dev;

	if (dma_wr_id != EN_DMA_WR_ID_MAX) {
		dma_free_coherent(&vecm_dev, alloc_size[dma_wr_id],
			dma_vaddr[dma_wr_id], dma_paddr[dma_wr_id]);
	}
}

void am_dma_rd_buffer_free(struct platform_device *pdev,
	enum lut_dma_rd_id_e dma_rd_id)
{
	vecm_dev = pdev->dev;

	if (dma_rd_id != EN_DMA_RD_ID_MAX) {
		dma_free_coherent(&vecm_dev, dma_rd_count[dma_rd_id] * UNIT_SIZE,
			dma_rd_vaddr[dma_rd_id], dma_rd_paddr[dma_rd_id]);
	}
}

void am_dma_set_mif_wr_status(int enable)
{
	unsigned int addr;

	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif_ctrl);
	WRITE_VPP_REG_BITS_S5(addr, enable, 13, 1);

	pr_am_dma("%s: reg = %x, enable = %d\n",
		__func__, addr, enable);

	if (chip_type_id == chip_t3x) {
		addr = ADDR_PARAM(dma_reg_cfg.page,
			dma_reg_cfg.reg_sr_mode_l2c1);
		WRITE_VPP_REG_BITS_S5(addr, 1, 22, 1);
	}
}

void am_dma_set_mif_wr(enum lut_dma_wr_id_e dma_wr_id,
	int enable)
{
	if (dma_wr_id != EN_DMA_WR_ID_MAX) {
		if (dma_vaddr[dma_wr_id])
			_set_vpu_lut_dma_mif_wr_unit(enable,
				&lut_dma_wr[dma_wr_id]);
	}
}

void am_dma_set_mif_rd(enum lut_dma_rd_id_e dma_rd_id,
	int enable)
{
	if (dma_rd_id != EN_DMA_RD_ID_MAX) {
		if (dma_rd_vaddr[dma_rd_id])
			_set_vpu_lut_dma_mif_rd_unit(enable,
				&lut_dma_rd[dma_rd_id]);
	}
}

int am_dma_get_mif_data_lc_stts(int index,
	unsigned int *data, unsigned int length)
{
	int i = 0;
	int j = 0;
	int k = 0;
	unsigned int tmp = 0;
	unsigned int size = length;
	unsigned int *val = NULL;
	unsigned int min_max = 0;
	unsigned int dma_id = EN_DMA_WR_ID_CM2_LC;
	unsigned int offset = 0;
	unsigned int data_offset = 64;//cm + lc : 64, lc:0


	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return 1;
	}

	if (index > 1)
		index = 1;

	if (size > 12 * 8 * 17)
		size = 1632;

	if (chip_type_id == chip_t6x) {
		dma_id = EN_DMA_WR_ID_CM2_LC;
		data_offset = dma_cm_en ? 64 : 0;
	} else if (chip_type_id == chip_t3x) {
		dma_id = EN_DMA_WR_ID_LC_STTS_0 + index;
		data_offset = 0;
	} else {
		return 1;
	}

	if (!dma_vaddr[dma_id]) {
		pr_am_dma("%s: dma_vaddr %d is NULL.\n",
			__func__, dma_id);
		return 1;
	}

	val = (unsigned int *)dma_vaddr[dma_id] + data_offset;

	for (i = 0; i < 12 * 8; i++) {
		for (j = 0; j < 16; j++) {
			data[k] = val[i * 16 + j] & 0xffffff;
			k++;

			if (j == 3 || j == 7 || j == 11 || j == 15) {
				tmp = (val[i * 16 + j] >> 24) & 0x1f;
				min_max |= (tmp << offset);

				if (j == 15) {
					data[k] = min_max;
					min_max = 0;
					offset = 0;
					k++;
				} else {
					offset += 5;
				}
			}

			if (k == size)
				break;
		}
	}

	return 0;
}

int am_dma_get_mif_data_lc_curve(unsigned int *data,
	unsigned int h_num, unsigned int v_num)
{
	int i = 0;
	int j = 0;
	int k = 0;
	unsigned int tmp = 0;
	unsigned int *val;
	unsigned int dma_id = EN_DMA_WR_ID_CM2_LC;
	unsigned int data_offset = 16 * 4 + 384 * 4;// cm+lcstts+ curve

	if (chip_type_id != chip_t6x)
		return 1;

	if (!data ||
		h_num == 0 || v_num == 0) {
		pr_am_dma("%s: data or size not fit.\n",
			__func__);
		return 1;
	}

	if (h_num > 12)
		h_num = 12;

	if (v_num > 8)
		v_num = 8;

	if (!dma_vaddr[dma_id]) {
		pr_am_dma("%s: dma_vaddr %d is NULL.\n",
			__func__, dma_id);
		return 1;
	}

	data_offset =  dma_cm_en ? (16 * 4 + 384 * 4) : (384 * 4);
	val = (unsigned int *)dma_vaddr[dma_id] + data_offset;

	for (i = 0; i < v_num; i++) {
		for (j = 0; j < h_num; j++) {
			tmp = val[i * 2 + j * 16];
			data[k * 6 + 0] =
				tmp & 0x3ff;/*bit0:9*/
			data[k * 6 + 1] =
				(tmp >> 10) & 0x3ff;/*bit10:19*/
			data[k * 6 + 2] =
				(tmp >> 20) & 0x3ff;/*bit20:29*/

			tmp = val[i * 2 + j * 16 + 1];
			data[k * 6 + 3] =
				tmp & 0x3ff;/*bit0:9*/
			data[k * 6 + 4] =
				(tmp >> 10) & 0x3ff;/*bit10:19*/
			data[k * 6 + 5] =
				(tmp >> 20) & 0x3ff;/*bit20:29*/

			k++;
		}
	}
	return 0;
}

void am_dma_get_mif_data_vi_hist(int index,
	unsigned short *data, unsigned int length)
{
	int i = 0;
	unsigned int size = length;
	unsigned short *val;

	if (chip_type_id != chip_t3x)
		return;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return;
	}

	if (index > 1)
		index = 1;

	if (size > 64)
		size = 64;

	if (!dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0 + index]) {
		pr_am_dma("%s: dma_vaddr %d is NULL.\n",
			__func__, index);
		return;
	}

	val = (unsigned short *)dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0 + index];

	for (i = 0; i < size; i++) {
		data[i] = val[i];
		pr_am_dma_vi("%s: val[%d] is %d.\n",
			__func__, i, val[i]);
	}
}

void am_dma_get_mif_data_vi_hist_low(int index,
	unsigned int *data, unsigned int length)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int offset = 0;
	unsigned short tmp = 0;
	unsigned int size = length;
	unsigned char *val;

	if (chip_type_id != chip_t3x)
		return;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return;
	}

	if (index > 1)
		index = 1;

	if (size > 64)
		size = 64;

	if (!dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0 + index]) {
		pr_am_dma("%s: dma_vaddr %d is NULL.\n",
			__func__, index);
		return;
	}

	val = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0 + index] +
		144;

	for (i = 0; i < 13; i++) {
		offset = 16 * i;
		for (j = 0; j < 5; j++) {
			data[k] = val[offset + 3 * j];
			tmp = val[offset + 3 * j + 1];
			tmp = tmp << 8;
			data[k] |= tmp;
			/*tmp = val[offset + 3 * j + 2];*/
			/*tmp = tmp << 16;*/
			/*data[k] |= tmp;*/
			k++;

			if (k == size)
				break;
		}
	}
}

int am_dma_get_mif_data_cm2_hist_hue(int index,
	unsigned int *data, unsigned int length)
{
	int i = 0;
	unsigned int size = length;
	unsigned int *val;
	unsigned int dma_id = EN_DMA_WR_ID_CM2_LC;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return 1;
	}

	if (index > 1)
		index = 1;

	if (size > 32)
		size = 32;

	if (chip_type_id == chip_t6x)
		dma_id = EN_DMA_WR_ID_CM2_LC;
	else if (chip_type_id == chip_t3x)
		dma_id = EN_DMA_WR_ID_CM2_HIST_0 + index;
	else
		return 1;

	if (!dma_vaddr[dma_id]) {
		pr_am_dma("%s: dma_vaddr %d is NULL.\n",
			__func__, dma_id);
		return 1;
	}

	val = (unsigned int *)dma_vaddr[dma_id];

	for (i = 0; i < size; i++)
		data[i] = val[i];

	return 0;
}

int am_dma_get_mif_data_cm2_hist_sat(int index,
	unsigned int *data, unsigned int length)
{
	int i = 0;
	unsigned int size = length;
	unsigned int *val;
	unsigned int dma_id = EN_DMA_WR_ID_CM2_LC;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return 1;
	}

	if (index > 1)
		index = 1;

	if (size > 32)
		size = 32;

	if (chip_type_id == chip_t6x)
		dma_id = EN_DMA_WR_ID_CM2_LC;
	else if (chip_type_id == chip_t3x)
		dma_id = EN_DMA_WR_ID_CM2_HIST_0 + index;
	else
		return 1;

	if (!dma_vaddr[dma_id]) {
		pr_am_dma("%s: dma_vaddr %d is NULL.\n",
			__func__, dma_id);
		return 1;
	}

	val = (unsigned int *)dma_vaddr[dma_id] + 32;

	for (i = 0; i < size; i++)
		data[i] = val[i];

	return 0;
}

void am_dma_updat_hdr2_hist(int mosaic_mode)
{
	enum lut_dma_wr_id_e dma_wr_id = EN_DMA_WR_ID_VD1_HDR_HIST;
	unsigned int addr;
	unsigned int val;
	int wr_sel;
	int offset;
	int i = 0;
	static int pre_mosaic_mode;
	struct vpu_lut_dma_wr_s *cfg_data;

	if (pre_mosaic_mode != mosaic_mode)
		pre_mosaic_mode = mosaic_mode;
	else
		return;

	for (i = 0; i < 2; i++) {
		cfg_data = &lut_dma_wr[dma_wr_id + i];
		cfg_data->addr_mode = mosaic_mode ? 0 : 3;
		if (cfg_data->dma_wr_id < 8) {
			wr_sel = 0;
			offset = cfg_data->dma_wr_id;
		} else {
			wr_sel = 1;
			offset = cfg_data->dma_wr_id - 8;
		}
		addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif_sel);
		val = 0xff0 + wr_sel;
		VSYNC_WRITE_VPP_REG(addr, val);

		addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif0_ctrl) + offset;
		VSYNC_WRITE_VPP_REG_BITS(addr, cfg_data->addr_mode, 28, 2);
		pr_am_dma(" reg = %x, cfg_data->addr_mode = %d\n",
			addr, cfg_data->addr_mode);
	}
}

int am_dma_get_mif_data_hdr2_hist(int index,
	unsigned int *data, unsigned int length)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int offset = 0;
	unsigned int tmp = 0;
	unsigned int size = length;
	unsigned char *val;
	unsigned int dma_id = EN_DMA_WR_ID_VD1_HDR_HIST;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return 1;
	}

	if (chip_type_id == chip_t6x) {
		if (index > 4)
			index = 4;
	} else {
		if (index > 2)
			index = 2;
	}

	if (size > 128)
		size = 128;

	if (chip_type_id == chip_t6x)
		dma_id = EN_DMA_WR_ID_VD1_HDR_HIST + index;
	else if (chip_type_id == chip_t6w)
		dma_id = EN_DMA_WR_ID_VD1_HDR_HIST;
	else if (chip_type_id == chip_t3x)
		dma_id = EN_DMA_WR_ID_CM2_HIST_0 + index;
	else
		return 1;

	if (!dma_vaddr[dma_id]) {
		pr_am_dma("%s: dma_vaddr %d is NULL.\n",
			__func__, dma_id);
		return 1;
	}

	val = (unsigned char *)dma_vaddr[dma_id];
	for (i = 0; i < 26; i++) {
		offset = 16 * i;
		for (j = 0; j < 5; j++) {
			data[k] = val[offset + 3 * j];
			tmp = val[offset + 3 * j + 1];
			tmp = tmp << 8;
			data[k] |= tmp;
			tmp = val[offset + 3 * j + 2];
			tmp = tmp << 16;
			data[k] |= tmp;
			k++;

			if (k == size)
				break;
		}
	}

	return 0;
}

void am_dma_get_blend_vi_hist(unsigned short *data,
	unsigned int length)
{
	int i = 0;
	unsigned int size = length;
	unsigned short *val_0;
	unsigned short *val_1;

	if (chip_type_id != chip_t3x)
		return;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return;
	}

	if (size > 64)
		size = 64;

	if (!dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0] ||
		!dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_1]) {
		pr_am_dma("%s: dma_vaddr is NULL.\n",
			__func__);
		return;
	}

	val_0 = (unsigned short *)dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0];
	val_1 = (unsigned short *)dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_1];

	for (i = 0; i < size; i++) {
		data[i] = val_0[i] + val_1[i];
		pr_am_dma_vi("%s: dma_val_0/1[%d] = %d/%d\n",
			__func__, i, val_0[i], val_1[i]);
	}
}

void am_dma_get_blend_vi_hist_low(unsigned int *data,
	unsigned int length)
{
	int i = 0;
	unsigned int size = length;
	unsigned char *val_0;
	unsigned char *val_1;
	unsigned short tmp = 0;
	unsigned short tmp_0 = 0;
	unsigned short tmp_1 = 0;

	if (chip_type_id != chip_t3x)
		return;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return;
	}

	if (size > 64)
		size = 64;

	if (!dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0] ||
		!dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_1]) {
		pr_am_dma("%s: dma_vaddr is NULL.\n",
			__func__);
		return;
	}

	val_0 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_0] +
		144;
	val_1 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_VI_HIST_SPL_1] +
		144;

	for (i = 0; i < size; i++) {
		tmp_0 = val_0[3 * i];
		tmp = val_0[3 * i + 1];
		tmp = tmp << 8;
		tmp_0 |= tmp;
		/*tmp = val_0[3 * i + 2];*/
		/*tmp = tmp << 16;*/
		/*tmp_0 |= tmp;*/

		tmp_1 = val_1[3 * i];
		tmp = val_1[3 * i + 1];
		tmp = tmp << 8;
		tmp_1 |= tmp;
		/*tmp = val_1[3 * i + 2];*/
		/*tmp = tmp << 16;*/
		/*tmp_1 |= tmp;*/

		data[i] = tmp_0 + tmp_1;
		pr_am_dma_vi("%s: val_0/1[%d] = %d/%d\n",
			__func__, i, tmp_0, tmp_1);
	}
}

void am_dma_get_blend_cm2_hist_hue(unsigned int *data,
	unsigned int length)
{
	int i = 0;
	unsigned int size = length;
	unsigned char *val_0;
	unsigned char *val_1;
	unsigned int tmp = 0;
	unsigned int tmp_0 = 0;
	unsigned int tmp_1 = 0;

	if (chip_type_id != chip_t3x)
		return;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return;
	}

	if (size > 32)
		size = 32;

	if (!dma_vaddr[EN_DMA_WR_ID_CM2_HIST_0] ||
		!dma_vaddr[EN_DMA_WR_ID_CM2_HIST_1]) {
		pr_am_dma("%s: dma_vaddr is NULL.\n",
			__func__);
		return;
	}

	val_0 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_CM2_HIST_0];
	val_1 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_CM2_HIST_1];

	for (i = 0; i < size; i++) {
		tmp_0 = val_0[3 * i];
		tmp = val_0[3 * i + 1];
		tmp = tmp << 8;
		tmp_0 |= tmp;
		tmp = val_0[3 * i + 2];
		tmp = tmp << 16;
		tmp_0 |= tmp;

		tmp_1 = val_1[3 * i];
		tmp = val_1[3 * i + 1];
		tmp = tmp << 8;
		tmp_1 |= tmp;
		tmp = val_1[3 * i + 2];
		tmp = tmp << 16;
		tmp_1 |= tmp;

		data[i] = tmp_0 + tmp_1;
	}
}

void am_dma_get_blend_cm2_hist_sat(unsigned int *data,
	unsigned int length)
{
	int i = 0;
	unsigned int size = length;
	unsigned char *val_0;
	unsigned char *val_1;
	unsigned int tmp = 0;
	unsigned int tmp_0 = 0;
	unsigned int tmp_1 = 0;

	if (chip_type_id != chip_t3x)
		return;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return;
	}

	if (size > 32)
		size = 32;

	if (!dma_vaddr[EN_DMA_WR_ID_CM2_HIST_0] ||
		!dma_vaddr[EN_DMA_WR_ID_CM2_HIST_1]) {
		pr_am_dma("%s: dma_vaddr is NULL.\n",
			__func__);
		return;
	}

	val_0 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_CM2_HIST_0] +
		96;
	val_1 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_CM2_HIST_1] +
		96;

	for (i = 0; i < size; i++) {
		tmp_0 = val_0[3 * i];
		tmp = val_0[3 * i + 1];
		tmp = tmp << 8;
		tmp_0 |= tmp;
		tmp = val_0[3 * i + 2];
		tmp = tmp << 16;
		tmp_0 |= tmp;

		tmp_1 = val_1[3 * i];
		tmp = val_1[3 * i + 1];
		tmp = tmp << 8;
		tmp_1 |= tmp;
		tmp = val_1[3 * i + 2];
		tmp = tmp << 16;
		tmp_1 |= tmp;

		data[i] = tmp_0 + tmp_1;
	}
}

void am_dma_get_blend_hdr2_hist(unsigned int *data,
	unsigned int length)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int offset = 0;
	unsigned int size = length;
	unsigned char *val_0;
	unsigned char *val_1;
	unsigned int tmp = 0;
	unsigned int tmp_0 = 0;
	unsigned int tmp_1 = 0;

	if (chip_type_id != chip_t3x)
		return;

	if (!data || length == 0) {
		pr_am_dma("%s: data or length not fit.\n",
			__func__);
		return;
	}

	if (size > 128)
		size = 128;

	if (!dma_vaddr[EN_DMA_WR_ID_VD1_HDR_0] ||
		!dma_vaddr[EN_DMA_WR_ID_VD1_HDR_1]) {
		pr_am_dma("%s: dma_vaddr is NULL.\n",
			__func__);
		return;
	}

	val_0 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_VD1_HDR_0];
	val_1 = (unsigned char *)dma_vaddr[EN_DMA_WR_ID_VD1_HDR_1];

	for (i = 0; i < 26; i++) {
		offset = 16 * i;
		for (j = 0; j < 5; j++) {
			tmp_0 = val_0[offset + 3 * j];
			tmp = val_0[offset + 3 * j + 1];
			tmp = tmp << 8;
			tmp_0 |= tmp;
			tmp = val_0[offset + 3 * j + 2];
			tmp = tmp << 16;
			tmp_0 |= tmp;

			tmp_1 = val_1[offset + 3 * j];
			tmp = val_1[offset + 3 * j + 1];
			tmp = tmp << 8;
			tmp_1 |= tmp;
			tmp = val_1[offset + 3 * j + 2];
			tmp = tmp << 16;
			tmp_1 |= tmp;

			data[k] = tmp_0 + tmp_1;
			k++;

			pr_am_dma_hdr2("%s: val_0/1[%d] = %d/%d\n",
				__func__, i, tmp_0, tmp_1);

			if (k == size)
				break;
		}
	}
}

void am_dma_set_dma_mif_rd_en(enum lut_dma_rd_id_e dma_rd_id,
	int enable, int vpp_index)
{
	unsigned int addr;
	unsigned int val;
	int wr_sel;
	int offset;
	struct vpu_lut_dma_rd_s *cfg_data;

	if (dma_rd_id < 8) {
		wr_sel = 0;
		offset = dma_rd_id;
	} else {
		wr_sel = 1;
		offset = dma_rd_id - 8;
	}

	cfg_data = &lut_dma_rd[dma_rd_id];

	/*
	 * Bit 0 lut_reg_sel unsigned, RW, default = 0,
	 *       0: sel lut0-7; 1: sel lut8-15
	 */
	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif_sel);
	val = wr_sel;
	VSYNC_WRITE_VPP_REG_VPP_SEL(addr, val, vpp_index);

	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_rdmif0_ctrl) + offset;
	val = ((enable & 0x1) << 16) |
		(cfg_data->stride & 0x1fff);
	VSYNC_WRITE_VPP_REG_VPP_SEL(addr, val, vpp_index);

	pr_info("dma_rd_id = %d, enable =%d, [%x]= %x\n",
		dma_rd_id, enable, addr, val);
}

int am_dma_set_mif_data_gamut0(int *eotf, int *oetf, int *ootf)
{
	int i;
	enum lut_dma_rd_id_e dma_id = EN_DMA_RD_ID_GAMUT0;
	unsigned int *addr;
	unsigned int k = 0;
	unsigned int flag = 0xf2000000;

	if (!eotf || !oetf || !ootf) {
//		pr_am_dma("%s: lut is NULL.\n", __func__);
		return 1;
	}

	if (!dma_rd_vaddr[dma_id]) {
//		pr_am_dma("%s: dma_rd_vaddr %d is NULL.\n",
//			__func__, dma_id);
		return 1;
	}
	addr = (unsigned int *)dma_rd_vaddr[dma_id];
//	pr_info("%s:dma_vaddr = %p, addr = %p\n", __func__,
//		dma_rd_vaddr[dma_id], addr);

	for (i = 0; i < 3; i++)
		addr[k++] = 0;
	addr[k++] = flag;
	for (i = 0; i < 143 / 2; i++)
		addr[k++] = (eotf[i * 2 + 1] << 16) | eotf[i * 2];
	addr[k++] = eotf[142];

	for (i = 0; i < 3; i++)
		addr[k++] = 0;
	addr[k++] = flag;
	for (i = 0; i < 144 / 2; i++)
		addr[k++] = (oetf[i * 2 + 1] << 16) | oetf[i * 2];

	for (i = 0; i < 3; i++)
		addr[k++] = 0;
	addr[k++] = flag;
	for (i = 0; i < 144 / 2; i++)
		addr[k++] = (ootf[i * 2 + 1] << 16) | ootf[i * 2];

	return 0;
}

int am_dma_set_mif_data_gamut1(int *eotf, int *oetf)
{
	int i;
	enum lut_dma_rd_id_e dma_id = EN_DMA_RD_ID_GAMUT1;
	unsigned int *addr;
	unsigned int k = 0;
	unsigned int flag = 0xf2000000;

	if (!eotf || !oetf) {
//		pr_am_dma("%s: lut is NULL.\n", __func__);
		return 1;
	}

	if (!dma_rd_vaddr[dma_id]) {
//		pr_am_dma("%s: dma_rd_vaddr %d is NULL.\n",
//			__func__, dma_id);
		return 1;
	}

	addr = (unsigned int *)dma_rd_vaddr[dma_id];
//	pr_info("%s: dma_vaddr = %p, addr = %p\n", __func__,
//		dma_rd_vaddr[dma_id], addr);

	for (i = 0; i < 3; i++)
		addr[k++] = 0;
	addr[k++] = flag;
	for (i = 0; i < 143 / 2; i++)
		addr[k++] = (eotf[i * 2 + 1] << 16) | eotf[i * 2];
	addr[k++] = eotf[142];

	for (i = 0; i < 3; i++)
		addr[k++] = 0;
	addr[k++] = flag;
	for (i = 0; i < 144 / 2; i++)
		addr[k++] = (oetf[i * 2 + 1] << 16) | oetf[i * 2];

	return 0;
}

long long *ram[8];
int lut3d_malloc_ok;

int am_dma_3dlut_buffer_init(void)
{
	int i = 0;
	int j = 0;

	if (chip_type_id != chip_t6x ||
		lut3d_malloc_ok)
		return 0;

	for (i = 0; i < 8; i++) {
		ram[i] = vmalloc(730 * sizeof(long long));
		if (!ram[i]) {
			for (j = 0; j < i; j++)
				vfree(ram[j]);
			return -1;
		}
	}

	lut3d_malloc_ok = 1;

	return 0;
}

void am_dma_3dlut_buffer_release(void)
{
	int i = 0;

	if (chip_type_id != chip_t6x ||
		!lut3d_malloc_ok)
		return;

	for (i = 0; i < 8; i++)
		vfree(ram[i]);

	lut3d_malloc_ok = 0;
}

int am_dma_set_mif_data_3dlut(int *plut3d)
{
	int i;
	int ram0_cnt = 0;
	int ram1_cnt = 0;
	int ram2_cnt = 0;
	int ram3_cnt = 0;
	int ram4_cnt = 0;
	int ram5_cnt = 0;
	int ram6_cnt = 0;
	int ram7_cnt = 0;
	int d0, d1, d2, d3, d4, d5;
	int index = 0;
	int k = 0;
	enum lut_dma_rd_id_e dma_id = EN_DMA_RD_ID_3DLUT;
	unsigned int temp0 = 0;
	unsigned int temp1 = 0;
	unsigned int *addr;

	if (!plut3d || !dma_rd_vaddr[dma_id] || !lut3d_malloc_ok) {
//		pr_am_dma("set_3dlut: dma_rd_vaddr %d is NULL.\n", dma_id);
		return 1;
	}

	for (d0 = 0; d0 < 17; d0++) {
		for (d1 = 0; d1 < 17; d1++) {
			for (d2 = 0; d2 < 17; d2++) {
				index = (d0 * 17 * 17 * 3) + (d1 * 17 * 3) + d2 * 3;
				d3 = d0 / 2;
				d4 = d1 / 2;
				d5 = d2 / 2;
				if (d0 % 2 == 0 && d1 % 2 == 0 && d2 % 2 == 0) { //ram0
					if (d3 != 8 && d4 != 8 && d5 != 8)//000
						ram0_cnt = d3 * 8 * 8 + d4 * 8 + d5;
					else if ((d3 != 8) && (d4 != 8) && (d5 == 8)) //001
						ram0_cnt = 512 + d3 * 8 + d4;
					else if ((d3 != 8) && (d4 == 8) && (d5 != 8)) //010
						ram0_cnt = 576 + d3 * 8 + d5;
					else if ((d3 == 8) && (d4 != 8) && (d5 != 8)) //100
						ram0_cnt = 640 + d5 * 8 + d4;
					else if ((d3 == 8) && (d4 == 8) && (d5 != 8)) //110
						ram0_cnt = 704 + d5;
					else if ((d3 == 8) && (d4 != 8) && (d5 == 8)) //101
						ram0_cnt = 712 + d4;
					else if ((d3 != 8) && (d4 == 8) && (d5 == 8)) //011
						ram0_cnt = 720 + d3;
					else
						ram0_cnt = 728;

					ram[0][ram0_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
				} else if (d0 % 2 == 0 && d1 % 2 == 0 && d2 % 2 != 0) {//ram1
					if (d3 != 8 && d4 != 8)           //00
						ram1_cnt = d3 * 8 * 8 + d4 * 8 + d5;
					else if ((d3 != 8) && (d4 == 8))      //01
						ram1_cnt = 512 + d3 * 8 + d5;
					else if ((d3 == 8) && (d4 != 8))      //10
						ram1_cnt = 576 + d4 * 8 + d5;
					else                              //11
						ram1_cnt = 640 + d5;

					ram[1][ram1_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
				} else if (d0 % 2 == 0 && d1 % 2 != 0 && d2 % 2 == 0) { //ram2
					if (d3 != 8 && d5 != 8)           //00
						ram2_cnt = d3 * 8 * 8 + d5 * 8 + d4;
					else if ((d3 != 8) && (d5 == 8))      //01
						ram2_cnt = 512 + d3 * 8 + d4;
					else if ((d3 == 8) && (d5 != 8))      //10
						ram2_cnt = 576 + d5 * 8 + d4;
					else                              //11
						ram2_cnt = 640 + d4;

					ram[2][ram2_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
				} else if (d0 % 2 == 0 && d1 % 2 != 0 && d2 % 2 != 0) { //ram3
					if (d3 != 8)
						ram3_cnt = d3 * 8 * 8 + d4 * 8 + d5;
					else
						ram3_cnt = 512 + d4 * 8 + d5;

					ram[3][ram3_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
				} else if (d0 % 2 != 0 && d1 % 2 == 0 && d2 % 2 == 0) {//ram4
					if (d4 != 8 && d5 != 8)           //00
						ram4_cnt = d4 * 8 * 8 + d5 * 8 + d3;
					else if ((d4 != 8) && (d5 == 8))      //01
						ram4_cnt = 512 + d4 * 8 + d3;
					else if ((d4 == 8) && (d5 != 8))      //10
						ram4_cnt = 576 + d5 * 8 + d3;
					else                              //11
						ram4_cnt = 640 + d3;

					ram[4][ram4_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
				} else if (d0 % 2 != 0 && d1 % 2 == 0 && d2 % 2 != 0) { //ram5
					if (d4 != 8)
						ram5_cnt = d4 * 8 * 8 + d3 * 8 + d5;
					else
						ram5_cnt = 512 + d3 * 8 + d5;

					ram[5][ram5_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
				} else if (d0 % 2 != 0 && d1 % 2 != 0 && d2 % 2 == 0) { //ram6
					if (d5 != 8)
						ram6_cnt = d5 * 8 * 8 + d3 * 8 + d4;
					else
						ram6_cnt = 512 + d3 * 8 + d4;

					ram[6][ram6_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
				} else if (d0 % 2 != 0 && d1 % 2 != 0 && d2 % 2 != 0) { //ram7
					ram7_cnt = d3 * 8 * 8 + d4 * 8 + d5;

					ram[7][ram7_cnt] =
						(long long)(plut3d[index] & 0xfff) << 24 |
						(long long)(plut3d[index + 1] & 0xfff) << 12 |
						(long long)(plut3d[index + 2] & 0xfff);
					}
			}   //for (d2
		}   //for (d1
	} //for (d0

	addr = (unsigned int *)dma_rd_vaddr[dma_id];

	for (i = 0; i < 648; i++) {
		temp0  = ram[1][i] & 0xffffffff;
		addr[k++] = temp0;

		temp0 = (ram[1][i] >> 32) & 0xf;
		temp1 = (ram[2][i] & 0xfffffff) << 4;
		addr[k++] = temp0 + temp1;

		temp0 = (ram[2][i] >> 28) & 0xff;
		temp1 = (ram[4][i] & 0xffffff) << 8;
		addr[k++] = temp0 + temp1;

		temp0 = (ram[4][i] >> 24) & 0xfff;
		addr[k++] = temp0;
	}

	for (i = 0; i < 576; i++) {
		temp0  = ram[3][i] & 0xffffffff;
		addr[k++] = temp0;

		temp0 = (ram[3][i] >> 32) & 0xf;
		temp1 = (ram[5][i] & 0xfffffff) << 4;
		addr[k++] = temp0 + temp1;

		temp0 = (ram[5][i] >> 28) & 0xff;
		temp1 = (ram[6][i] & 0xffffff) << 8;
		addr[k++] = temp0 + temp1;

		temp0 = (ram[6][i] >> 24) & 0xfff;
		addr[k++] = temp0;
	}

	for (i = 0; i < 512; i++) {
		temp0  = ram[0][i] & 0xffffffff;
		addr[k++] = temp0;

		temp0 = (ram[0][i] >> 32) & 0xf;
		temp1 = (ram[7][i] & 0xfffffff) << 4;
		addr[k++] = temp0 + temp1;

		temp0 = (ram[7][i] >> 28) & 0xff;
		addr[k++] = temp0;

		temp0 = 0;
		addr[k++] = temp0;
	}

	for (i = 0; i < 217; i++) {
		temp0  = ram[0][i + 512] & 0xffffffff;
		addr[k++] = temp0;

		temp0 = (ram[0][i + 512] >> 32) & 0xf;
		addr[k++] = temp0;

		temp0 = 0;
		addr[k++] = temp0;

		temp0 = 0;
		addr[k++] = temp0;
	}

	return 0;
}

#define DMA_SIZE_TOTAL_LUT3D (17 * 17 * 17)

enum lut_dma_id_e {
	EN_DMA_ID_LDIM_STTS = 0,
	EN_DMA_ID_DI_FILM,
	EN_DMA_ID_VD1_S0_FILM,
	EN_DMA_ID_VD1_S1_FILM,
	EN_DMA_ID_VD1_S2_FILM,
	EN_DMA_ID_VD1_S3_FILM, /*5*/
	EN_DMA_ID_VD2_FILM,
	EN_DMA_ID_TCON,
	EN_DMA_ID_LUT3D,
	EN_DMA_ID_HDR,
};

struct vpu_lut_dma_type_s {
	enum lut_dma_id_e dma_id;
	/*reg_hdr_dma_sel:*/
	u32 reg_hdr_dma_sel_vd1s0; /*4bits, default 1*/
	u32 reg_hdr_dma_sel_vd1s1; /*4bits, default 2*/
	u32 reg_hdr_dma_sel_vd1s2; /*4bits, default 3*/
	u32 reg_hdr_dma_sel_vd1s3; /*4bits, default 4*/
	u32 reg_hdr_dma_sel_vd2;   /*4bits, default 5*/
	u32 reg_hdr_dma_sel_osd1;  /*4bits, default 6*/
	u32 reg_hdr_dma_sel_osd2;  /*4bits, default 7*/
	u32 reg_hdr_dma_sel_osd3;  /*4bits, default 8*/
	/*reg_dma_mode : 1: cfg hdr2 lut with lut_dma, 0: with cbus mode*/
	u32 reg_vd1s0_hdr_dma_mode;  /*1bit*/
	u32 reg_vd1s1_hdr_dma_mode;  /*1bit*/
	u32 reg_vd1s2_hdr_dma_mode;  /*1bit*/
	u32 reg_vd1s3_hdr_dma_mode;  /*1bit*/
	u32 reg_vd2_hdr_dma_mode;    /*1bit*/
	u32 reg_osd1_hdr_dma_mode;   /*1bit*/
	u32 reg_osd2_hdr_dma_mode;   /*1bit*/
	u32 reg_osd3_hdr_dma_mode;   /*1bit*/

	/*mif info : TODO more params*/
	u32 rd_wr_sel;
	u32 mif_baddr[16][4];
	u32 chan_rd_bytes_num[16]; /*rdmif rd num*/
	u32 chan_sel_src_num[16];  /*channel select interrupt source num*/
	u32 chan_little_endian[16];
	u32 chan_swap_64bit[16];
};

static struct device vecm_dev_lut3d;
static ulong alloc_size_lut3d;
static void *dma_vaddr_lut3d;
static dma_addr_t dma_paddr_lut3d;

static void _init_vpu_lut_dma(struct vpu_lut_dma_type_s *vpu_lut_dma)
{
	memset((void *)vpu_lut_dma, 0,
		sizeof(struct vpu_lut_dma_type_s));

	/*default: must identical to coef data in DDR BACKDOOR*/
	vpu_lut_dma->reg_hdr_dma_sel_vd1s0 = 1;
	vpu_lut_dma->reg_hdr_dma_sel_vd1s1 = 2;
	vpu_lut_dma->reg_hdr_dma_sel_vd1s2 = 3;
	vpu_lut_dma->reg_hdr_dma_sel_vd1s3 = 4;
	vpu_lut_dma->reg_hdr_dma_sel_vd2 = 5;
	vpu_lut_dma->reg_hdr_dma_sel_osd1 = 6;
	vpu_lut_dma->reg_hdr_dma_sel_osd2 = 7;
	vpu_lut_dma->reg_hdr_dma_sel_osd3 = 8;
}

static void _set_vpu_lut_dma_mif(struct vpu_lut_dma_type_s      *vpu_lut_dma)
{
	u32 mif_num = 0;
	u32 lut_reg_sel8_15 = 0;
	u32 reg_badr0 = 0;
	u32 reg_badr1 = 0;
	u32 reg_badr2 = 0;
	u32 reg_badr3 = 0;
	u32 reg_ctrl = 0;

	if (vpu_lut_dma->dma_id == EN_DMA_ID_LDIM_STTS) {
		mif_num = 0;
		reg_badr0 = VPU_DMA_RDMIF0_BADR0;
		reg_badr1 = VPU_DMA_RDMIF0_BADR1;
		reg_badr2 = VPU_DMA_RDMIF0_BADR2;
		reg_badr3 = VPU_DMA_RDMIF0_BADR3;
		reg_ctrl = VPU_DMA_RDMIF0_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_DI_FILM) {
		mif_num = 1;
		reg_badr0 = VPU_DMA_RDMIF1_BADR0;
		reg_badr1 = VPU_DMA_RDMIF1_BADR1;
		reg_badr2 = VPU_DMA_RDMIF1_BADR2;
		reg_badr3 = VPU_DMA_RDMIF1_BADR3;
		reg_ctrl = VPU_DMA_RDMIF1_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_VD1_S0_FILM) {
		mif_num = 2;
		reg_badr0 = VPU_DMA_RDMIF2_BADR0;
		reg_badr1 = VPU_DMA_RDMIF2_BADR1;
		reg_badr2 = VPU_DMA_RDMIF2_BADR2;
		reg_badr3 = VPU_DMA_RDMIF2_BADR3;
		reg_ctrl = VPU_DMA_RDMIF2_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_VD1_S1_FILM) {
		mif_num = 3;
		reg_badr0 = VPU_DMA_RDMIF3_BADR0;
		reg_badr1 = VPU_DMA_RDMIF3_BADR1;
		reg_badr2 = VPU_DMA_RDMIF3_BADR2;
		reg_badr3 = VPU_DMA_RDMIF3_BADR3;
		reg_ctrl = VPU_DMA_RDMIF3_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_VD1_S2_FILM) {
		mif_num = 4;
		reg_badr0 = VPU_DMA_RDMIF4_BADR0;
		reg_badr1 = VPU_DMA_RDMIF4_BADR1;
		reg_badr2 = VPU_DMA_RDMIF4_BADR2;
		reg_badr3 = VPU_DMA_RDMIF4_BADR3;
		reg_ctrl = VPU_DMA_RDMIF4_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_VD1_S3_FILM) {
		mif_num = 5;
		reg_badr0 = VPU_DMA_RDMIF5_BADR0;
		reg_badr1 = VPU_DMA_RDMIF5_BADR1;
		reg_badr2 = VPU_DMA_RDMIF5_BADR2;
		reg_badr3 = VPU_DMA_RDMIF5_BADR3;
		reg_ctrl = VPU_DMA_RDMIF5_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_VD2_FILM) {
		mif_num = 6;
		reg_badr0 = VPU_DMA_RDMIF6_BADR0;
		reg_badr1 = VPU_DMA_RDMIF6_BADR1;
		reg_badr2 = VPU_DMA_RDMIF6_BADR2;
		reg_badr3 = VPU_DMA_RDMIF6_BADR3;
		reg_ctrl = VPU_DMA_RDMIF6_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_TCON) {
		mif_num = 7;
		reg_badr0 = VPU_DMA_RDMIF7_BADR0;
		reg_badr1 = VPU_DMA_RDMIF7_BADR1;
		reg_badr2 = VPU_DMA_RDMIF7_BADR2;
		reg_badr3 = VPU_DMA_RDMIF7_BADR3;
		reg_ctrl = VPU_DMA_RDMIF7_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_LUT3D) {
		mif_num = 8;
		lut_reg_sel8_15 = 1;
		reg_badr0 = VPU_DMA_RDMIF0_BADR0;
		reg_badr1 = VPU_DMA_RDMIF0_BADR1;
		reg_badr2 = VPU_DMA_RDMIF0_BADR2;
		reg_badr3 = VPU_DMA_RDMIF0_BADR3;
		reg_ctrl = VPU_DMA_RDMIF0_CTRL;
	} else if (vpu_lut_dma->dma_id == EN_DMA_ID_HDR) {
		mif_num = 9;
		lut_reg_sel8_15 = 1;
		reg_badr0 = VPU_DMA_RDMIF1_BADR0;
		reg_badr1 = VPU_DMA_RDMIF1_BADR1;
		reg_badr2 = VPU_DMA_RDMIF1_BADR2;
		reg_badr3 = VPU_DMA_RDMIF1_BADR3;
		reg_ctrl = VPU_DMA_RDMIF1_CTRL;
	} else {
		return;
	}

	WRITE_VPP_REG_BITS_S5(VPU_DMA_RDMIF_SEL,
		lut_reg_sel8_15, 0, 1);

	WRITE_VPP_REG_S5(reg_badr0,
		vpu_lut_dma->mif_baddr[mif_num][0]);
	WRITE_VPP_REG_S5(reg_badr1,
		vpu_lut_dma->mif_baddr[mif_num][1]);
	WRITE_VPP_REG_S5(reg_badr2,
		vpu_lut_dma->mif_baddr[mif_num][2]);
	WRITE_VPP_REG_S5(reg_badr3,
		vpu_lut_dma->mif_baddr[mif_num][3]);

	/*reg_rd0_stride*/
	WRITE_VPP_REG_BITS_S5(reg_ctrl,
		vpu_lut_dma->chan_rd_bytes_num[mif_num], 0, 13);

	/*Bit 13 little_endian*/
	WRITE_VPP_REG_BITS_S5(reg_ctrl,
		vpu_lut_dma->chan_little_endian[mif_num], 13, 1);

	/*Bit 14 swap_64bit*/
	WRITE_VPP_REG_BITS_S5(reg_ctrl,
		vpu_lut_dma->chan_swap_64bit[mif_num], 14, 1);

	/*Bit 23:16*/
	/*reg_rd0_enable_int,*/
	/*channel0 select interrupt source*/
	WRITE_VPP_REG_BITS_S5(reg_ctrl,
		vpu_lut_dma->chan_sel_src_num[mif_num], 16, 8);
}

static void _set_vpu_lut_dma(struct vpu_lut_dma_type_s      *vpu_lut_dma)
{
	unsigned int val;
	unsigned int addr;

	addr = ADDR_PARAM(viu_dma_reg_cfg.page,
		viu_dma_reg_cfg.reg_ctrl0);
	val = READ_VPP_REG_S5(addr);
	val |= vpu_lut_dma->reg_vd1s0_hdr_dma_mode << 0 |
		vpu_lut_dma->reg_vd1s1_hdr_dma_mode << 1 |
		vpu_lut_dma->reg_vd1s2_hdr_dma_mode << 2 |
		vpu_lut_dma->reg_vd1s3_hdr_dma_mode << 3 |
		vpu_lut_dma->reg_vd2_hdr_dma_mode << 4 |
		vpu_lut_dma->reg_osd1_hdr_dma_mode << 6 |
		vpu_lut_dma->reg_osd2_hdr_dma_mode << 7 |
		vpu_lut_dma->reg_osd3_hdr_dma_mode << 8;
	WRITE_VPP_REG_S5(addr, val);

	addr = ADDR_PARAM(viu_dma_reg_cfg.page,
		viu_dma_reg_cfg.reg_ctrl1);
	val = READ_VPP_REG_S5(addr);
	val |= vpu_lut_dma->reg_hdr_dma_sel_vd1s0 << 0  |
		vpu_lut_dma->reg_hdr_dma_sel_vd1s1 << 4 |
		vpu_lut_dma->reg_hdr_dma_sel_vd1s2 << 8 |
		vpu_lut_dma->reg_hdr_dma_sel_vd1s3 << 12 |
		vpu_lut_dma->reg_hdr_dma_sel_vd2 << 16 |
		vpu_lut_dma->reg_hdr_dma_sel_osd1 << 20 |
		vpu_lut_dma->reg_hdr_dma_sel_osd2 << 24 |
		vpu_lut_dma->reg_hdr_dma_sel_osd3 << 28;
	WRITE_VPP_REG_S5(addr, val);

	_set_vpu_lut_dma_mif(vpu_lut_dma);
}

void am_dma_reset_lc(int enable, int rdma_mode, int vpp_index)
{
	unsigned int addr;
	unsigned int val;
	unsigned int rpt_num;
	unsigned int stride_num;

	if (enable) {
		rpt_num = 1;
		stride_num = 4;
	} else {
		rpt_num = 8;
		stride_num = 48;
	}

	//pr_am_dma("%s: rpt_num = %d, stride_num = %d, rdma_mode = %d, vpp_index = %d\n",
	//pr_info("%s: rpt_num = %d, stride_num = %d, rdma_mode = %d, vpp_index = %d\n",
	//	__func__, rpt_num, stride_num, rdma_mode, vpp_index);

	addr = ADDR_PARAM(dma_reg_cfg.page,
		dma_reg_cfg.reg_wrmif0_ctrl) + EN_DMA_WR_ID_LC_STTS_0;
	val = READ_VPP_REG_S5(addr);
	val = (val & 0xfc03e000) |
		((rpt_num & 0xff) << 18) |
		(stride_num & 0x1fff);
	if (rdma_mode)
		VSYNC_WRITE_VPP_REG_VPP_SEL(addr, val, vpp_index);
	else
		WRITE_VPP_REG_S5(addr, val);
	//pr_am_dma("%s: enable = %d, reg = 0x%x, val = 0x%08x\n",
	//pr_info("%s: enable = %d, reg = 0x%x, val = 0x%08x\n",
	//	__func__, enable, addr, val);
}

static void _fill_dma_data_lut3d(void *dma_vaddr,
	int *lut_data, int length)
{
	if (!dma_vaddr || !lut_data)
		return;
}

void am_dma_lut3d_buffer_malloc(struct platform_device *pdev)
{
	vecm_dev_lut3d = pdev->dev;
	alloc_size_lut3d = DMA_SIZE_TOTAL_LUT3D;
	dma_vaddr_lut3d = dma_alloc_coherent(&vecm_dev_lut3d,
		alloc_size_lut3d, &dma_paddr_lut3d, GFP_KERNEL);
}

void am_dma_lut3d_buffer_free(struct platform_device *pdev)
{
	vecm_dev_lut3d = pdev->dev;
	dma_free_coherent(&vecm_dev_lut3d, alloc_size_lut3d,
		dma_vaddr_lut3d, dma_paddr_lut3d);
}

void am_dma_lut3d_set_data(int *data, int length)
{
	u32 dma_id_int;
	struct vpu_lut_dma_type_s vpu_lut_dma;

	if (!data || length <= 0)
		return;

	_fill_dma_data_lut3d(NULL, NULL, 0);

	_init_vpu_lut_dma(&vpu_lut_dma);

	vpu_lut_dma.dma_id = EN_DMA_ID_LUT3D;
	dma_id_int = (u32)(vpu_lut_dma.dma_id);
	/*0:config wr_mif, 1:config rd_mif*/
	vpu_lut_dma.rd_wr_sel = 1;
	vpu_lut_dma.mif_baddr[dma_id_int][0] = (u32)(dma_paddr_lut3d) >> 4;
	vpu_lut_dma.mif_baddr[dma_id_int][1] = (u32)(dma_paddr_lut3d) >> 4;
	vpu_lut_dma.mif_baddr[dma_id_int][2] = (u32)(dma_paddr_lut3d) >> 4;
	vpu_lut_dma.mif_baddr[dma_id_int][3] = (u32)(dma_paddr_lut3d) >> 4;
	vpu_lut_dma.chan_rd_bytes_num[dma_id_int] = DMA_SIZE_TOTAL_LUT3D;
	/*select viu_vsync_int_i[0]*/
	vpu_lut_dma.chan_sel_src_num[dma_id_int] = 1;

	_set_vpu_lut_dma(&vpu_lut_dma);
}

void am_dma_lut3d_get_data(int *data, int length)
{
	if (!data || length <= 0)
		return;
}

