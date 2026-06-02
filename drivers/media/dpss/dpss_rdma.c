// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include "sys_def.h"		//ary add for sim

#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/sched/clock.h>
#include <linux/platform_device.h>

#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/tee.h>
#include <linux/dma-mapping.h>	//for rdma
#include <linux/debugfs.h>

#endif				/* RUN_ON_ARM */
#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_func.h"
#include "dpss_hw.h"

#define DPSS_RDMA_NUM        16
#define RDMA_MANUAL_MODE 0 /*rdma manual mode not enable in dpss tbc mode*/

enum EDPSS_RDMA_SRC_BIT {
	E_DPSS_RDMA_DISPLAY_GO_FIELD = 0,
	E_DPSS_RDMA_PRE_VS = 1,
	E_DPSS_RDMA_DPE_FRM = 2,
	//...to-do
};

struct dpss_rdma_regadr_s {	//rdma_regadr_s //for register table
	u32 ahb_start_addr;
	u32 ahb_start_addr_msb;
	u32 ahb_end_addr;
	u32 ahb_end_addr_msb;
	u32 int_src_man_trig;	//for auto is int src, for man is trig register
	u32 buf_ctr;
	u32 buf_len_0_3;
	u32 buf_len_4_7;
	u32 buf_len_8_11;
	u32 buf_len_12_15;
	u32 buf_cfg_en;
	u32 ro_buf_idx;
	//bit control register, need used with bit
	u32 reg_addr_inc;
	u32 reg_rw_flg;
//      u32 reg_clear;
	//----------------
	u8 ch_id;
	u8 b_addr_inc_pos;	//only one bit
	u8 b_rw_flg_pos;	//only one bit read / write
};

struct reg_test {
	u32 addr;
	u32 value;
};

struct dpss_rdma_info_s {	//frc_rdma_info //control ?
//----------------------
// new:2025-03-10:
	bool used;
	char *owner;
	struct rdma_op_s *op;
	//void *op_arg;
	unsigned int irq_count;
//      u32 *reg_buf;
//----------------------
	u64 phy_addr;
	u32 *v_addr;
	u32 tab_size;		//for total size
	u32 item_cnt;
	u32 int_src;
	u32 step;		// option for multi_buf;
	u32 length[DPSS_RDMA_BUF_NUB_MAX];	//option for multi_buf;
	u32 shift_sub[DPSS_RDMA_BUF_NUB_MAX];
	u32 shift;
	u8 other[DPSS_RDMA_BUF_NUB_MAX];
	u8 reg_len[DPSS_RDMA_BUF_NUB_MAX];	//this is used with shift
	u8 buf_status;
	u8 buf_nub;		// 1 ~ 16
	u8 buf_nub_used;
	bool rd_wr;		// 0 :rd, 1: wr
	bool incl;		// auto inc;
//      int rdma_write_count;
};

struct dpss_rdma_info_s *rdma_ram_tab[DPSS_RDMA_RAM_NUB];
struct dpss_rdma_info_s rdma_ram_tab_r[DPSS_RDMA_RAM_NUB];
void print_s_rdma_info(struct dpss_rdma_info_s *p, char *name)
{
	sprint("%s:\n", name);
	sprint("phy_addr     =0x%llx\n", p->phy_addr);
	sprint("v_addr       =0x%px\n", p->v_addr);
	sprint("tab_size     =%d\n", p->tab_size);
	sprint("buf_status   =0x%x\n", p->buf_status);
	sprint("item_cnt     =%d\n", p->item_cnt);
	sprint("int_src     =0x%08x\n", p->int_src);
	sprint("step     =0x%08x\n", p->step);
	sprint("buf_nub  =%d\n", p->buf_nub);
	sprint("buf_nub  =%d\n", p->buf_nub_used);
	sprint("rd_wr    =%d\n", p->rd_wr);
	sprint("incl     =%d\n", p->incl);
	sprint("====================================\n");
}

void print_s_rdma_reg(struct dpss_rdma_regadr_s *p, char *name)
{
	sprint("%s:\n", name);
	sprint("ch_id              =%d\n", p->ch_id);
	sprint("ahb_start_addr     =0x%04x\n", p->ahb_start_addr);
	sprint("ahb_start_addr_msb =0x%04x\n", p->ahb_start_addr_msb);
	sprint("ahb_end_addr       =0x%04x\n", p->ahb_end_addr);
	sprint("ahb_end_addr_msb   =0x%04x\n", p->ahb_end_addr_msb);
	sprint("int_src_man_trig   =0x%04x\n", p->int_src_man_trig);
	sprint("buf_ctr            =0x%04x\n", p->buf_ctr);
	sprint("buf_len_0_3        =0x%04x\n", p->buf_len_0_3);
	sprint("buf_len_4_7        =0x%04x\n", p->buf_len_4_7);
	sprint("buf_len_8_11       =0x%04x\n", p->buf_len_8_11);
	sprint("buf_len_12_15      =0x%04x\n", p->buf_len_12_15);
	sprint("buf_cfg_en         =0x%04x\n", p->buf_cfg_en);
	sprint("ro_buf_idx         =0x%04x\n", p->ro_buf_idx);
	sprint("reg_addr_inc       =0x%04x\n", p->reg_addr_inc);
	sprint("reg_rw_flg         =0x%04x\n", p->reg_rw_flg);
//      sprint("reg_clear          =0x%08x\n", p->reg_clear);
	sprint("b_addr_inc_pos     =%d\n", p->b_addr_inc_pos);
	sprint("b_rw_flg_pos	   =%d\n", p->b_rw_flg_pos);
	sprint("====================================\n");
}

//no sub buffer:
void print_rdma_data(struct dpss_rdma_info_s *p, char *name)
{
	int i;
	unsigned int cnt;

	if (!p || p->item_cnt <= 0)
		return;
	if (p->buf_nub > 1) {
		DBG_WARN("%s:has sub tab, do nothing:%d\n", __func__,
			 p->buf_nub);
		return;
	}
	sprint("%s:rw[%d], inc[%d], cnt[%d]\n", name,
	       p->rd_wr, p->incl, p->item_cnt);
	cnt = p->item_cnt;
	if (p->rd_wr && !p->incl) {
		// wr, not inc
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x = 0x%x\n", i, p->v_addr[2 * i],
			       p->v_addr[2 * i + 1]);
	} else if (p->rd_wr && p->incl) {
		// wr, inc
		sprint("\taddr:0x%x\n", p->v_addr[0]);
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x\n", i, p->v_addr[1 + i]);
	} else if (!p->rd_wr && !p->incl) {
		//rd: not inc:
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x = 0x%x\n", i, p->v_addr[i],
			       p->v_addr[i + cnt]);
	} else {
		//rd: inc:
		sprint("\taddr:0x%x\n", p->v_addr[0]);
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x\n", i, p->v_addr[1 + i]);
	}
	sprint("====================================\n");
}

static struct dpss_rdma_regadr_s rdma_regadr_dpss_t6w[DPSS_RDMA_NUM] = {
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_MAN,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_MAN_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_MAN,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_MAN_MSB,
	 .int_src_man_trig = DPSS_RDMA_ACCESS_MAN,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_MAN,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_MAN_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_MAN_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_MAN_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_MAN_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_MAN,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_MAN,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_MAN,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_MAN,
	 .ch_id = 0,
	 .b_addr_inc_pos = 1,
	 .b_rw_flg_pos = 2,
	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_1,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_1_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_1,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_1_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC1_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_1,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_1_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_1_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_1_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_1_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_1,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_1,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO,
	 .ch_id = 1,
	 .b_addr_inc_pos = 1,
	 .b_rw_flg_pos = 5,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_2,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_2_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_2,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_2_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC2_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_2,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_2_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_2_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_2_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_2_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_2,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_2,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO,
	 .ch_id = 2,
	 .b_addr_inc_pos = 2,
	 .b_rw_flg_pos = 6,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_3,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_3_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_3,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_3_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC3_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_3,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_3_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_3_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_3_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_3_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_3,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_3,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO,
	 .ch_id = 3,
	 .b_addr_inc_pos = 3,
	 .b_rw_flg_pos = 7,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_4,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_4_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_4,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_4_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC4_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_4,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_4_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_4_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_4_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_4_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_4,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_4,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO2,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO2,
	 .ch_id = 4,
	 .b_addr_inc_pos = 0,
	 .b_rw_flg_pos = 4,
	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_5,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_5_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_5,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_5_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC5_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_5,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_5_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_5_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_5_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_5_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_5,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_5,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO2,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO2,
	 .ch_id = 5,
	 .b_addr_inc_pos = 1,
	 .b_rw_flg_pos = 5,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_6,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_6_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_6,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_6_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC6_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_6,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_6_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_6_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_6_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_6_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_6,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_6,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO2,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO2,
	 .ch_id = 6,
	 .b_addr_inc_pos = 2,
	 .b_rw_flg_pos = 6,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_7,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_7_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_7,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_7_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC7_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_7,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_7_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_7_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_7_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_7_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_7,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_7,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO2,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO2,
	 .ch_id = 7,
	 .b_addr_inc_pos = 3,
	 .b_rw_flg_pos = 7,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_8,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_8_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_8,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_8_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC8_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_8,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_8_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_8_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_8_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_8_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_8,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_8,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 8,
	 .b_addr_inc_pos = 0,
	 .b_rw_flg_pos = 8,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_9,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_9_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_9,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_9_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC9_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_9,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_9_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_9_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_9_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_9_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_9,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_9,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 9,
	 .b_addr_inc_pos = 1,
	 .b_rw_flg_pos = 9,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_10,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_10_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_10,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_10_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC10_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_10,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_10_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_10_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_10_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_10_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_10,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_10,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 10,
	 .b_addr_inc_pos = 2,
	 .b_rw_flg_pos = 10,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_11,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_11_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_11,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_11_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC11_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_11,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_11_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_11_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_11_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_11_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_11,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_11,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 11,
	 .b_addr_inc_pos = 3,
	 .b_rw_flg_pos = 11,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_12,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_12_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_12,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_12_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC12_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_12,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_12_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_12_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_12_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_12_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_12,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_12,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 12,
	 .b_addr_inc_pos = 4,
	 .b_rw_flg_pos = 12,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_13,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_13_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_13,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_13_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC13_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_13,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_13_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_13_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_13_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_13_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_13,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_13,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 13,
	 .b_addr_inc_pos = 5,
	 .b_rw_flg_pos = 13,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_14,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_14_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_14,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_14_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC14_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_14,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_14_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_14_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_14_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_14_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_14,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_14,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 14,
	 .b_addr_inc_pos = 6,
	 .b_rw_flg_pos = 14,

	  },
	{
	 .ahb_start_addr = DPSS_RDMA_AHB_START_ADDR_15,
	 .ahb_start_addr_msb = DPSS_RDMA_AHB_START_ADDR_15_MSB,
	 .ahb_end_addr = DPSS_RDMA_AHB_END_ADDR_15,
	 .ahb_end_addr_msb = DPSS_RDMA_AHB_END_ADDR_15_MSB,
	 .int_src_man_trig = DPSS_RDMA_AUTO_SRC15_SEL,
	 .buf_ctr = DPSS_RDMA_BUF_CTRL_15,
	 .buf_len_0_3 = DPSS_RDMA_BUF_LENGTH_15_0,
	 .buf_len_4_7 = DPSS_RDMA_BUF_LENGTH_15_1,
	 .buf_len_8_11 = DPSS_RDMA_BUF_LENGTH_15_2,
	 .buf_len_12_15 = DPSS_RDMA_BUF_LENGTH_15_3,
	 .buf_cfg_en = DPSS_RDMA_BUF_CFG_EN_15,
	 .ro_buf_idx = DPSS_RDMA_BUF_RO_15,
	 .reg_addr_inc = DPSS_RDMA_ACCESS_AUTO3,
	 .reg_rw_flg = DPSS_RDMA_ACCESS_AUTO3,
	 .ch_id = 15,
	 .b_addr_inc_pos = 7,
	 .b_rw_flg_pos = 15,

	  },
};

struct dpss_rdma_regadr_s *dpss_rdma_get_ch_tab(unsigned int id)
{
	if (id < DPSS_RDMA_NUM)
		return &rdma_regadr_dpss_t6w[id];
	return NULL;
}

void dpss_rdma_ram_tab_clear(struct dpss_rdma_info_s *r_tab)
{
	int i;
	unsigned int cnt;

	if (!r_tab || r_tab->tab_size <= 0) {
		DBG_WARN("%s:null?\n", __func__);
		return;
	}
	cnt = r_tab->tab_size / (sizeof(u32));	// /4
	for (i = 0; i < cnt; i++)
		r_tab->v_addr[i] = 0;
	r_tab->item_cnt = 0;
	// dbg_a2("%s:done, cnt =%d\n", __func__, cnt);
}

void dpss_ram_tab_init(struct dpss_ch_s *pch)
{
	int i;
	struct dpss_rdma_info_s *r_tab;
	unsigned long ph_addr;
	unsigned int *v_addr;

	if (!(dpss_en_rdma & C_BIT16))
		return;
	ph_addr = pch->c.addr_rdma_base;
	v_addr = pch->c.addr_rdma_v;

	for (i = 0; i < DPSS_RDMA_RAM_NUB; i++) {
		rdma_ram_tab[i] = &rdma_ram_tab_r[i];	//init
		r_tab = rdma_ram_tab[i];
		memset(r_tab, 0, sizeof(*r_tab));
		r_tab->phy_addr = ph_addr + DPSS_RDMA_RAM_ONE_SIZE * i;
		r_tab->v_addr = v_addr + (DPSS_RDMA_RAM_ONE_SIZE / 4) * i;
		r_tab->tab_size = DPSS_RDMA_RAM_ONE_SIZE;
		r_tab->buf_status = C_BIT0;
		pch->c.addr_rdma[i] = r_tab->phy_addr;
		//dbg_a2("ram:%d:0x%llx,0x%px\n",
		//       i, r_tab->phy_addr, r_tab->v_addr);
	}

	for (i = 0; i < DPSS_RDMA_RAM_NUB; i++) {
		r_tab = rdma_ram_tab[i];
		dpss_rdma_ram_tab_clear(r_tab);
	}

	dbg_a2("%s:done %d\n", __func__, i);
}

void dpss_ram_tab_rcfg_mode(struct dpss_rdma_info_s *r_tab,
			    bool rd_wr, bool incl, u8 buf_nub, u32 step)
{
	u64 phy_addr;
	u32 *v_addr;
	u32 tab_size;		//for total size

	//check:
	if (buf_nub * step > r_tab->tab_size) {
		DBG_WARN("%s:overflow: 0x%x, 0x%x\n", __func__,
			 buf_nub * step, r_tab->tab_size);
		return;
	}
	dbg_a1("%s:buf_nub[%d],step[0x%x]\n", __func__, buf_nub, step);
	//back:
	phy_addr = r_tab->phy_addr;
	v_addr = r_tab->v_addr;
	tab_size = r_tab->tab_size;
	//clear:
	memset(r_tab, 0, sizeof(*r_tab));
	//
	r_tab->phy_addr = phy_addr;
	r_tab->v_addr = v_addr;
	r_tab->tab_size = tab_size;
	//
	r_tab->rd_wr = rd_wr;
	r_tab->incl = incl;
	r_tab->buf_nub = buf_nub;
	r_tab->buf_nub_used = buf_nub;
	r_tab->step = step;
	r_tab->buf_status = C_BIT1;
}

//ary add register and value to frc_rdma_s table.
//frc_rdma_table_config
void dpss_ram_tab_add_w(struct dpss_rdma_info_s *r_tab, u32 addr, u32 val)
{
	int i;

	i = r_tab->item_cnt;

	dbg_a2("i:%d, addr:%x, val:%x\n", i, addr, val);

	if ((i + 1) * 8 > r_tab->tab_size) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__,
			i, r_tab->tab_size);
		return;
	}

	r_tab->v_addr[i * 2] = addr;
	r_tab->v_addr[i * 2 + 1] = val;

	r_tab->item_cnt++;
}

//num is cnt of data, not total pval
void dpss_ram_tab_add_w_i(struct dpss_rdma_info_s *r_tab, u32 *pval, u32 num)
{
	int i = 0, start, addr_num = 0;

	if (r_tab->item_cnt == 0) {
		start = 0;
		addr_num = 1;
	} else {
		start = r_tab->item_cnt + 1;
		addr_num = 0;
	}

	if ((start + num + addr_num) * 4 > r_tab->tab_size) {
		DBG_ERR("%s:overflow: %d:%d:size=%d\n", __func__,
			start, num, r_tab->tab_size);
		return;
	}
	if (addr_num)
		r_tab->v_addr[0] = pval[0];

	for (i = 0; i < num; i++) {
		r_tab->v_addr[i + start + addr_num] = pval[i + addr_num];
		r_tab->item_cnt++;
	}
}

void dpss_ram_tab_add_r(struct dpss_rdma_info_s *r_tab, u32 addr)
{
	int i;

	i = r_tab->item_cnt;

	dbg_a2("i:%d, addr:%x\n", i, addr);

	if ((i + 1) * 8 > r_tab->tab_size) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__,
			i, r_tab->tab_size);
		return;
	}

	r_tab->v_addr[i] = addr;
	r_tab->item_cnt++;
}

void dpss_ram_tab_add_r_i(struct dpss_rdma_info_s *r_tab, u32 addr, u32 nub)
{
	if ((1 + nub) * 4 > r_tab->tab_size) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__,
			nub, r_tab->tab_size);
		return;
	}

	r_tab->v_addr[0] = addr;
	r_tab->item_cnt = nub;
}

void dpss_rdma_count_shift_val_ot(unsigned int inv, u8 *shift, u8 *val,
				  u8 *other)
{
	u8 sft = 0, v = 0, o = 0;
	u32 tmp;

	if (inv < C_BIT8)
		sft = 0;
	else if (inv >= C_BIT8 && inv < C_BIT9)
		sft = 1;
	else if (inv >= C_BIT9 && inv < C_BIT10)
		sft = 2;
	else if (inv >= C_BIT10 && inv < C_BIT11)
		sft = 3;
	else if (inv >= C_BIT11 && inv < C_BIT12)
		sft = 4;
	else if (inv >= C_BIT12 && inv < C_BIT13)
		sft = 5;
	else if (inv >= C_BIT13 && inv < C_BIT14)
		sft = 6;
	else if (inv >= C_BIT14 && inv < C_BIT15)
		sft = 7;
	else if (inv >= C_BIT15 && inv < C_BIT16)
		sft = 8;
	else if (inv >= C_BIT16 && inv < C_BIT17)
		sft = 9;
	else if (inv >= C_BIT17 && inv < C_BIT18)
		sft = 10;
	else if (inv >= C_BIT18 && inv < C_BIT19)
		sft = 11;
	else if (inv >= C_BIT19 && inv < C_BIT20)
		sft = 12;
	else if (inv >= C_BIT20 && inv < C_BIT21)
		sft = 13;
	else if (inv >= C_BIT21 && inv < C_BIT22)
		sft = 14;
	else
		sft = 15;

	v = (inv + ((1 << sft) - 1)) >> sft;
	tmp = v << sft;
	o = tmp - inv;

	*shift = sft;
	*val = v;
	*other = o;
	dbg_a2("%s:in=%d:%d,%d,%d\n", __func__, inv, sft, v, o);
}

//
u32 *dpss_ram_sbuf_get_addr(struct dpss_rdma_info_s *r_tab,
			    unsigned int buf_idx)
{
	return r_tab->v_addr + buf_idx * r_tab->step / sizeof(u32);
}

//ary add register and value to frc_rdma_s table.
//frc_rdma_table_config
void dpss_ram_tab_sbuf_add_w(struct dpss_rdma_info_s *r_tab,
			     unsigned int buf_idx, u32 addr, u32 val)
{
	int i;
	u32 *buf = NULL;

	if (!r_tab || buf_idx >= r_tab->buf_nub) {
		DBG_WARN("%s:overflow:0x%p, %d\n", __func__, r_tab, buf_idx);
		return;
	}

	i = r_tab->length[buf_idx];
	buf = dpss_ram_sbuf_get_addr(r_tab, buf_idx);

	//dbg_a2("i:%d, addr:%x, val:%x\n", i, addr, val);

	if ((i + 1) * 8 > r_tab->step) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__, i, r_tab->step);
		return;
	}

	buf[i * 2] = addr;
	buf[i * 2 + 1] = val;

	r_tab->length[buf_idx]++;
}

//num is cnt of data, not total pval
void dpss_ram_tab_sbuf_add_w_i(struct dpss_rdma_info_s *r_tab,
			       unsigned int buf_idx, u32 *pval, u32 num)
{
	int i = 0, start, addr_num = 0;
	u32 *buf = NULL;
	u32 cnt;

	if (!r_tab || buf_idx >= r_tab->buf_nub) {
		DBG_WARN("%s:overflow:0x%p, %d\n", __func__, r_tab, buf_idx);
		return;
	}

	cnt = r_tab->length[buf_idx];
	if (cnt == 0) {
		start = 0;
		addr_num = 1;
	} else {
		start = cnt + 1;
		addr_num = 0;
	}

	if ((start + num + addr_num) * 4 > r_tab->step) {
		DBG_ERR("%s:overflow: %d:%d:size=%d\n", __func__,
			start, num, r_tab->step);
		return;
	}
	buf = dpss_ram_sbuf_get_addr(r_tab, buf_idx);

	if (addr_num)
		buf[0] = pval[0];

	for (i = 0; i < num; i++) {
		buf[i + start + addr_num] = pval[i + addr_num];
		r_tab->length[buf_idx]++;
	}
}

void dpss_ram_tab_sbuf_add_r(struct dpss_rdma_info_s *r_tab,
			     unsigned int buf_idx, u32 addr)
{
	int i;
	u32 *buf = NULL;

	if (!r_tab || buf_idx >= r_tab->buf_nub) {
		DBG_WARN("%s:overflow:0x%p, %d\n", __func__, r_tab, buf_idx);
		return;
	}

	i = r_tab->length[buf_idx];

	dbg_a2("i:%d, addr:%x\n", i, addr);

	if ((i + 1) * 8 > r_tab->step) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__, i, r_tab->step);
		return;
	}
	buf = dpss_ram_sbuf_get_addr(r_tab, buf_idx);

	buf[i] = addr;
	r_tab->length[buf_idx]++;
}

void dpss_ram_tab_sbuf_add_r_i(struct dpss_rdma_info_s *r_tab,
			       unsigned int buf_idx, u32 addr, u32 nub)
{
	u32 *buf = NULL;

	if (!r_tab || buf_idx >= r_tab->buf_nub) {
		DBG_WARN("%s:overflow:0x%p, %d\n", __func__, r_tab, buf_idx);
		return;
	}
	if ((1 + nub) * 4 > r_tab->step) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__,
			nub, r_tab->step);
		return;
	}
	buf = dpss_ram_sbuf_get_addr(r_tab, buf_idx);

	buf[0] = addr;
	r_tab->length[buf_idx] = nub;
}

void print_rdma_s_data(struct dpss_rdma_info_s *p, unsigned int buf_idx,
		       char *name)
{
	int i;
	unsigned int cnt;
	u32 *buf = NULL;

	if (!p || buf_idx >= p->buf_nub) {
		DBG_WARN("%s:overflow:0x%p, %d\n", __func__, p, buf_idx);
		return;
	}
	if (p->buf_nub <= buf_idx) {
		DBG_WARN("%s:over flow, do nothing:%d,%d\n", __func__, buf_idx,
			 p->buf_nub);
		return;
	}

	if (p->buf_nub > 1) {
		if (p->length[buf_idx] <= 0) {
			DBG_INF("%s:%d:length is 0\n", __func__, buf_idx);
			return;
		}
		cnt = p->length[buf_idx];
	} else {
		cnt = p->item_cnt;
	}

	sprint("%s:buf_idx[%d],rw[%d],inc[%d],cnt[%d]\n", name,
	       buf_idx, p->rd_wr, p->incl, p->length[buf_idx]);
	buf = dpss_ram_sbuf_get_addr(p, buf_idx);
	sprint("buf:0x%px\n", buf);

	if (p->rd_wr && !p->incl) {
		// wr, not inc
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x = 0x%x\n", i, buf[2 * i],
			       buf[2 * i + 1]);
	} else if (p->rd_wr && p->incl) {
		// wr, inc
		sprint("\taddr:0x%x\n", buf[0]);
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x\n", i, buf[1 + i]);
	} else if (!p->rd_wr && !p->incl) {
		//rd: not inc:
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x = 0x%x\n", i, buf[i], buf[i + cnt]);
	} else {
		//rd: inc:
		sprint("\taddr:0x%x\n", buf[0]);
		for (i = 0; i < cnt; i++)
			sprint("\t%d:0x%x\n", i, buf[1 + i]);
	}
	sprint("====================================\n");
}

void print_rdma_s_data_all(struct dpss_rdma_info_s *p, char *name)
{
	int i;

	for (i = 0; i < p->buf_nub; i++)
		print_rdma_s_data(p, i, "sub buffer");
}

void sprint_rdma_s_data(struct seq_file *s,
			struct dpss_rdma_info_s *p, unsigned int buf_idx,
			char *name)
{
	int i;
	unsigned int cnt;
	u32 *buf = NULL;

	if (!p || buf_idx >= p->buf_nub) {
		seq_printf(s, "warn:%s:overflow:0x%p, %d\n", __func__, p,
			   buf_idx);
		return;
	}
	if (p->buf_nub <= buf_idx) {
		seq_printf(s, "warn:%s:over flow, do nothing:%d,%d\n", __func__,
			   buf_idx, p->buf_nub);
		return;
	}

	if (p->buf_nub > 1) {
		if (p->length[buf_idx] <= 0) {
			seq_printf(s, "%s:%d:length is 0\n", __func__, buf_idx);
			return;
		}
		cnt = p->length[buf_idx];
	} else {
		cnt = p->item_cnt;
	}

	seq_printf(s, "%s:buf_idx[%d],rw[%d],inc[%d],cnt[%d]\n", name,
		   buf_idx, p->rd_wr, p->incl, p->length[buf_idx]);
	buf = dpss_ram_sbuf_get_addr(p, buf_idx);
	seq_printf(s, "buf:0x%px\n", buf);

	if (p->rd_wr && !p->incl) {
		// wr, not inc
		for (i = 0; i < cnt; i++)
			seq_printf(s, "\t%d:0x%x = 0x%x\n", i, buf[2 * i],
				   buf[2 * i + 1]);
	} else if (p->rd_wr && p->incl) {
		// wr, inc
		seq_printf(s, "\taddr:0x%x\n", buf[0]);
		for (i = 0; i < cnt; i++)
			seq_printf(s, "\t%d:0x%x\n", i, buf[1 + i]);
	} else if (!p->rd_wr && !p->incl) {
		//rd: not inc:
		for (i = 0; i < cnt; i++)
			seq_printf(s, "\t%d:0x%x = 0x%x\n", i, buf[i],
				   buf[i + cnt]);
	} else {
		//rd: inc:
		seq_printf(s, "\taddr:0x%x\n", buf[0]);
		for (i = 0; i < cnt; i++)
			seq_printf(s, "\t%d:0x%x\n", i, buf[1 + i]);
	}
	seq_puts(s, "====================================\n");
}

void sprint_s_rdma_info(struct seq_file *s, struct dpss_rdma_info_s *p,
			char *name)
{
	int i;

	seq_printf(s, "%s:\n", name);
	seq_printf(s, "phy_addr     =0x%llx\n", p->phy_addr);
	seq_printf(s, "v_addr       =0x%px\n", p->v_addr);
	seq_printf(s, "tab_size     =%d\n", p->tab_size);
	seq_printf(s, "buf_status   =0x%x\n", p->buf_status);
	seq_printf(s, "item_cnt     =%d\n", p->item_cnt);
	seq_printf(s, "int_src     =0x%08x\n", p->int_src);
	seq_printf(s, "step     =0x%08x\n", p->step);
	seq_printf(s, "buf_nub  =%d\n", p->buf_nub);
	seq_printf(s, "buf_nub_used  =%d\n", p->buf_nub_used);
	seq_printf(s, "shift  =%d\n", p->shift);
	for (i = 0; i < p->buf_nub; i++) {
		seq_printf(s, "sub  =%d\n", i);
		seq_printf(s, "\tcnt  =%d\n", p->length[i]);
		seq_printf(s, "\tsft_sub  =%d\n", p->shift_sub[i]);
		seq_printf(s, "\treg_len  =%d\n", p->reg_len[i]);
	}

	seq_printf(s, "rd_wr    =%d\n", p->rd_wr);
	seq_printf(s, "incl     =%d\n", p->incl);
	seq_puts(s, "====================================\n");
}

void dbg_rdma_tst_register_clr(void)
{
	int i;
	u32 data;

	for (i = 0; i < 16; i++)
		wr(FRC_REG_TOP_RESERVE0 + i, 0);
	//check:
	for (i = 0; i < 16; i++) {
		data = rd(FRC_REG_TOP_RESERVE0 + i);
		if (!data)
			DBG_WARN("register 0x%x = 0x%x\n",
				 FRC_REG_TOP_RESERVE0 + i, data);
	}
}

void dbg_rdma_tst_register_print(void)
{
	int i;
	u32 data;

	for (i = 0; i < 16; i++) {
		data = rd(FRC_REG_TOP_RESERVE0 + i);
		DBG_INF("\tregister 0x%x = 0x%x\n",
			FRC_REG_TOP_RESERVE0 + i, data);
	}
}

void dbg_rd_rdma_reg_val(unsigned int reg_id)
{
	struct dpss_rdma_regadr_s *p;

	if (reg_id >= DPSS_RDMA_NUM) {
		DBG_ERR("%s:overflow %d\n", __func__, reg_id);
		return;
	}
	p = &rdma_regadr_dpss_t6w[reg_id];

	sprint("%s:%d:ch_id   =%d\n", __func__, reg_id, p->ch_id);
	sprint("DPSS_RDMA_ACCESS_AUTO4   =0x%x\n", rd(DPSS_RDMA_ACCESS_AUTO4));
	sprint("ahb_start_addr     =0x%04x = 0x%08x\n", p->ahb_start_addr,
	       rd(p->ahb_start_addr));
	sprint("ahb_start_addr_msb =0x%04x = 0x%08x\n", p->ahb_start_addr_msb,
	       rd(p->ahb_start_addr_msb));
	sprint("ahb_end_addr       =0x%04x = 0x%08x\n", p->ahb_end_addr,
	       rd(p->ahb_end_addr));
	sprint("ahb_end_addr_msb   =0x%04x = 0x%08x\n", p->ahb_end_addr_msb,
	       rd(p->ahb_end_addr_msb));
	sprint("int_src_man_trig   =0x%04x = 0x%08x\n", p->int_src_man_trig,
	       rd(p->int_src_man_trig));
	sprint("buf_ctr            =0x%04x = 0x%08x\n", p->buf_ctr,
	       rd(p->buf_ctr));
	sprint("buf_len_0_3        =0x%04x = 0x%08x\n", p->buf_len_0_3,
	       rd(p->buf_len_0_3));
	sprint("buf_len_4_7        =0x%04x = 0x%08x\n", p->buf_len_4_7,
	       rd(p->buf_len_4_7));
	sprint("buf_len_8_11       =0x%04x = 0x%08x\n", p->buf_len_8_11,
	       rd(p->buf_len_8_11));
	sprint("buf_len_12_15      =0x%04x = 0x%08x\n", p->buf_len_12_15,
	       rd(p->buf_len_12_15));
	sprint("buf_cfg_en         =0x%04x = 0x%08x\n", p->buf_cfg_en,
	       rd(p->buf_cfg_en));
	sprint("ro_buf_idx         =0x%04x = 0x%08x\n", p->ro_buf_idx,
	       rd(p->ro_buf_idx));
	sprint("reg_addr_inc wr    =0x%04x = 0x%08x\n", p->reg_addr_inc,
	       rd(p->reg_addr_inc));
	sprint("====================================\n");
}

#ifdef DPSS_RDMA_HAND_TEST
//frc_rdma_isr
void dpss_rdma_isr(void)
{
	int i;
	u32 rdma_status;
	u32 off_done = 4, off;
	u32 off_clear = 16;
	bool flg_find = false;

	rdma_status = rd(DPSS_RDMA_STATUS1);	//[19:4] is done flag

	dbg_h2("rdma status:[0x%x][0x%x][0x%x][0x%x]\n",
	       rd(DPSS_RDMA_STATUS),
	       rdma_status, rd(DPSS_RDMA_STATUS2), rd(DPSS_RDMA_STATUS3));

	if (rdma_status & 0xffff0) {
		for (i = 0; i < DPSS_RDMA_NUM; i++) {
			off = i + off_done;
			if (rdma_status & (0x1 << off)) {
				dbg_h2("rdma ch %d\n", i);
				w_reg_bit(DPSS_RDMA_CTRL, 0x1, off_clear + i,
					  1);
				flg_find = true;
				break;
			}
		}
	}
	if (!flg_find)
		DBG_WARN("rdma:can't find irq\n");
}
#endif
//is_rdma_enable
bool dpss_is_rdma_enable(void)
{
	return true;
}

/* need int : frc_rdma_s */
//frc_rdma_config
int dpss_rdma_cfg(struct dpss_rdma_regadr_s *ctr_tab,
		  struct dpss_rdma_info_s *ram_tab)
{
	u64 end_addr;
	bool rw, inc;
	u32 len;

	if (!ram_tab || !ctr_tab) {
		DBG_WARN("%s:no ram_tab\n", __func__);
		return 0;
	}

	rw = ram_tab->rd_wr;
	inc = ram_tab->incl;

	if (inc == 0)
		len = ram_tab->item_cnt * 8;
	else
		len = (1 + ram_tab->item_cnt) * 4;

	end_addr = ram_tab->phy_addr + len - 1;
	dbg_a1("%s:len=0x%x,ph end=0x%llx, ch_id:0x%x\n", __func__, len,
	       end_addr, ctr_tab->ch_id);

	//disable irq:
	if (ctr_tab->ch_id == 0) {	//man: only set bit
		w_reg_bit(ctr_tab->int_src_man_trig, 0, 0, 1);	//ctrl_start_man
	} else {		//auto mode:set sel src
		//w_reg_bit(DPSS_RDMA_ACCESS_AUTO4, 0, ctr_tab->ch_id, 1);
		wr(ctr_tab->int_src_man_trig, 0);
	}

	//start addr
	wr(ctr_tab->ahb_start_addr,
	   (unsigned int)(ram_tab->phy_addr & 0xffffffff));

	wr(ctr_tab->ahb_start_addr_msb,
	   (unsigned int)((ram_tab->phy_addr >> 32) & 0xffffffff));

	//end addr
	wr(ctr_tab->ahb_end_addr, end_addr & 0xffffffff);
	wr(ctr_tab->ahb_end_addr_msb,
	   (unsigned int)((end_addr >> 32) & 0xffffffff));

	w_reg_bit(ctr_tab->reg_addr_inc, inc, ctr_tab->b_addr_inc_pos, 1);
	w_reg_bit(ctr_tab->reg_rw_flg, rw, ctr_tab->b_rw_flg_pos, 1);

	if (ctr_tab->ch_id == 0) {
		w_reg_bit(ctr_tab->int_src_man_trig, 1, 0, 1);	//ctrl_start_man
	} else {		//int src sel:
		//w_reg_bit(DPSS_RDMA_ACCESS_AUTO4, 1, ctr_tab->ch_id, 1);
		wr(ctr_tab->int_src_man_trig, ram_tab->int_src);
	}

	return 0;
}

void dpss_rdma_man_trig(void)
{
	w_reg_bit(DPSS_RDMA_ACCESS_MAN, 1, 0, 1);	//ctrl_start_man
}

int dpss_rdma_cfg_multi_buf(struct dpss_rdma_regadr_s *ctr_tab,
			    struct dpss_rdma_info_s *ram_tab)
{
	bool rw, inc;
	u32 len_tmp;
	u8 a, b;		//tmp
	u32 len[4];
	int i;
	u32 buf_ctr_val = 0;
	u8 sft, val, other;

	//shift : 512, 1024, 2048, 4096, 8192, 16384,
	if (!ram_tab || !ctr_tab || ram_tab->buf_nub_used <= 1) {
		DBG_WARN("%s:no ram_tab\n", __func__);
		return 0;
	}
//--------------------
	//cnt shift:
	for (i = 0; i < ram_tab->buf_nub_used; i++) {
		dpss_rdma_count_shift_val_ot(ram_tab->length[i], &sft, &val,
					     &other);
		if (other) {
			DBG_ERR("%s:need fill null %i\n", __func__, i);
			ram_tab->other[i] = other;
		}
		ram_tab->reg_len[i] = val;
		ram_tab->shift_sub[i] = sft + 3;
	}
	sft = ram_tab->shift_sub[0];
	for (i = 0; i < ram_tab->buf_nub_used; i++) {
		//check shift:
		if (sft != ram_tab->shift_sub[i])
			DBG_ERR("%s:shift is not map %i\n", __func__, i);
	}
	ram_tab->shift = sft;
//--------------------
	rw = ram_tab->rd_wr;
	inc = ram_tab->incl;

	//disable irq:
	if (ctr_tab->ch_id == 0) {	//man: only set bit
		w_reg_bit(ctr_tab->int_src_man_trig, 0, 0, 1);	//ctrl_start_man
	} else {		//auto mode:set sel src
		//w_reg_bit(DPSS_RDMA_ACCESS_AUTO4, 0, ctr_tab->ch_id, 1);
		wr(ctr_tab->int_src_man_trig, 0);
	}
	//clr:
	w_reg_bit(DPSS_RDMA_BUF_IDX_CLR, 1, ctr_tab->ch_id, 1);	//2025-03-21
	dbg_a2("%s:clr:%d\n", __func__, ctr_tab->ch_id);
	//start addr
	wr(ctr_tab->ahb_start_addr,
	   (unsigned int)(ram_tab->phy_addr & 0xffffffff));

	wr(ctr_tab->ahb_start_addr_msb,
	   (unsigned int)((ram_tab->phy_addr >> 32) & 0xffffffff));

	//inc and rw
	w_reg_bit(ctr_tab->reg_addr_inc, inc, ctr_tab->b_addr_inc_pos, 1);
	w_reg_bit(ctr_tab->reg_rw_flg, rw, ctr_tab->b_rw_flg_pos, 1);

	//buf_contrl:
	buf_ctr_val = ram_tab->step << 16 |
	    ((sft & 0xf) << 8) | (ram_tab->buf_nub_used & 0x1f);
	wr(ctr_tab->buf_ctr, buf_ctr_val);
	dbg_a2("buf_ctr:0x%x = 0x%x, 0x%x\n", ctr_tab->buf_ctr,
			rd(ctr_tab->buf_ctr), buf_ctr_val);
	//buf length
	for (i = 0; i < 4; i++)
		len[i] = 0;
	for (i = 0; i < ram_tab->buf_nub_used; i++) {
		len_tmp = ram_tab->reg_len[i];
#ifdef MOV
		if (inc == 0)
			len_tmp = cnt * 8;
		else
			len_tmp = (1 + cnt) * 4;
#endif
		//inc is 1 ? to-do

		a = i / 4;
		b = i % 4;
		len[a] |= len_tmp << (8 * b);
	}

	wr(ctr_tab->buf_len_0_3, len[0]);
	wr(ctr_tab->buf_len_4_7, len[1]);
	wr(ctr_tab->buf_len_8_11, len[2]);
	wr(ctr_tab->buf_len_12_15, len[3]);

	wr(ctr_tab->buf_cfg_en, 1);
	if (ctr_tab->ch_id == 0) {
		w_reg_bit(ctr_tab->int_src_man_trig, 1, 0, 1);	//ctrl_start_man
	} else {		//int src sel:
		//w_reg_bit(DPSS_RDMA_ACCESS_AUTO4, 1, ctr_tab->ch_id, 1);
		wr(ctr_tab->int_src_man_trig, ram_tab->int_src);
	}

	return 0;
}

//wr, no incr, ddr mode
void dbg_rdma_prepare_tst1(unsigned char reg_ch,
			   unsigned char ram_tab_idx,
			   unsigned int start_val, unsigned int int_src)
{
	struct dpss_rdma_regadr_s *ctr_tab;
	struct dpss_rdma_info_s *ram_tab;
	int i;

	DBG_INF("%s start_val 0x%x\n", __func__, start_val);

	if (reg_ch >= DPSS_RDMA_NUM || ram_tab_idx >= DPSS_RDMA_RAM_NUB) {
		DBG_WARN("%s:overflow: %d, %d\n", __func__, reg_ch,
			 ram_tab_idx);
		return;
	}
	//set ctr tab
	ctr_tab = &rdma_regadr_dpss_t6w[reg_ch];

	//set
	ram_tab = rdma_ram_tab[ram_tab_idx];
	dpss_ram_tab_rcfg_mode(ram_tab, 1,	//rw: w
			       0,	//inc: no inc
			       1,	//buf_nub
			       0);

	//clear ram table:
	dpss_rdma_ram_tab_clear(ram_tab);	//dbg
	//set ram table:
	for (i = 0; i < 16; i++)
		dpss_ram_tab_add_w(ram_tab, FRC_REG_TOP_RESERVE0 + i,
				   start_val + i);

	if (ctr_tab->ch_id != 0 && int_src != 0xffff)
		ram_tab->int_src = int_src;

	dpss_rdma_cfg(ctr_tab, ram_tab);
}

//wr, incr, ddr mode
void dbg_rdma_prepare_tst2(unsigned char reg_ch,
			   unsigned char ram_tab_idx,
			   unsigned int start_val, unsigned int int_src)
{
	struct dpss_rdma_regadr_s *ctr_tab;
	struct dpss_rdma_info_s *ram_tab;
	int i;
	u32 w_tab[17];

	DBG_INF("%s\n", __func__);

	if (reg_ch >= DPSS_RDMA_NUM || ram_tab_idx >= DPSS_RDMA_RAM_NUB) {
		DBG_WARN("%s:overflow: %d, %d\n", __func__, reg_ch,
			 ram_tab_idx);
		return;
	}
	//set ctr tab
	ctr_tab = &rdma_regadr_dpss_t6w[reg_ch];

	//set
	ram_tab = rdma_ram_tab[ram_tab_idx];
	dpss_ram_tab_rcfg_mode(ram_tab, 1,	//rw: w
			       1,	//inc: inc
			       1,	//buf_nub
			       0);

	//clear ram table:
	dpss_rdma_ram_tab_clear(ram_tab);	//dbg
	//set ram table:
	w_tab[0] = FRC_REG_TOP_RESERVE0;

	for (i = 0; i < 16; i++)
		w_tab[i + 1] = start_val + i;

	dpss_ram_tab_add_w_i(ram_tab, &w_tab[0], 16);
	if (ctr_tab->ch_id != 0 && int_src != 0xffff)
		ram_tab->int_src = int_src;

	dpss_rdma_cfg(ctr_tab, ram_tab);
}

//rd : no inc
void dbg_rdma_prepare_tst3(unsigned char reg_ch,
			   unsigned char ram_tab_idx,
			   unsigned int start_val, unsigned int int_src)
{
	struct dpss_rdma_regadr_s *ctr_tab;
	struct dpss_rdma_info_s *ram_tab;
	int i;
//      u32 w_tab[17];

	DBG_INF("%s reg_ch %d\n", __func__, reg_ch);

	if (reg_ch >= DPSS_RDMA_NUM || ram_tab_idx >= DPSS_RDMA_RAM_NUB) {
		DBG_WARN("%s:overflow: %d, %d\n", __func__, reg_ch,
			 ram_tab_idx);
		return;
	}
	//set ctr tab
	ctr_tab = &rdma_regadr_dpss_t6w[reg_ch];

	//set
	ram_tab = rdma_ram_tab[ram_tab_idx];
	dpss_ram_tab_rcfg_mode(ram_tab, 0,	//rw: rd
			       0,	//inc: no inc
			       1,	//buf_nub
			       0);

	//clear ram table:
	dpss_rdma_ram_tab_clear(ram_tab);	//dbg
	//set ram table:
	for (i = 0; i < 16; i++)
		dpss_ram_tab_add_r(ram_tab, FRC_REG_TOP_RESERVE0 + i);

	if (ctr_tab->ch_id != 0 && int_src != 0xffff)
		ram_tab->int_src = int_src;

	dpss_rdma_cfg(ctr_tab, ram_tab);
}

//rd : inc
void dbg_rdma_prepare_tst4(unsigned char reg_ch,
			   unsigned char ram_tab_idx,
			   unsigned int start_val, unsigned int int_src)
{
	struct dpss_rdma_regadr_s *ctr_tab;
	struct dpss_rdma_info_s *ram_tab;
//      int i;
//      u32 w_tab[17];

	DBG_INF("%s reg_ch %d\n", __func__, reg_ch);

	if (reg_ch >= DPSS_RDMA_NUM || ram_tab_idx >= DPSS_RDMA_RAM_NUB) {
		DBG_WARN("%s:overflow: %d, %d\n", __func__, reg_ch,
			 ram_tab_idx);
		return;
	}
	ctr_tab = &rdma_regadr_dpss_t6w[reg_ch];
	//set
	ram_tab = rdma_ram_tab[ram_tab_idx];
	dpss_ram_tab_rcfg_mode(ram_tab, 0,	//rw: rd
			       1,	//inc: inc
			       1,	//buf_nub
			       0);

	//clear ram table:
	dpss_rdma_ram_tab_clear(ram_tab);	//dbg
	//set ram table:
	dpss_ram_tab_add_r_i(ram_tab, FRC_REG_TOP_RESERVE0, 16);

	if (ctr_tab->ch_id != 0 && int_src != 0xffff)
		ram_tab->int_src = int_src;

	dpss_rdma_cfg(ctr_tab, ram_tab);
}

//multi-buf: wr, no incr, ddr mode
void dbg_rdma_prepare_tst11(unsigned char reg_ch,
			    unsigned char ram_tab_idx,
			    unsigned int start_val, unsigned int int_src)
{
	struct dpss_rdma_regadr_s *ctr_tab;
	struct dpss_rdma_info_s *ram_tab;
	int i, idx;
	unsigned int buf_nub = 4;

	DBG_INF("%s %d\n", __func__, reg_ch);

	if (reg_ch >= DPSS_RDMA_NUM || ram_tab_idx >= DPSS_RDMA_RAM_NUB) {
		DBG_WARN("%s:overflow: %d, %d\n", __func__, reg_ch,
			 ram_tab_idx);
		return;
	}
	//set ctr tab

	ctr_tab = &rdma_regadr_dpss_t6w[reg_ch];

	//set
	ram_tab = rdma_ram_tab[ram_tab_idx];

	dpss_ram_tab_rcfg_mode(ram_tab, 1,	//rw: w
			       0,	//inc: no inc
			       buf_nub,	//buf_nub
			       DPSS_RDMA_RAM_ONE_SIZE / buf_nub);

	//clear ram table:
	dpss_rdma_ram_tab_clear(ram_tab);	//dbg

	if (ctr_tab->ch_id != 0 && int_src != 0xffff)
		ram_tab->int_src = int_src;

	idx = 0;
	//set ram table:
	for (i = 0; i < 10; i++)
		dpss_ram_tab_sbuf_add_w(ram_tab, idx, FRC_REG_TOP_RESERVE0 + i,
					start_val + i);

	idx = 1;
	//set ram table:
	for (i = 0; i < 12; i++)
		dpss_ram_tab_sbuf_add_w(ram_tab, idx, FRC_REG_TOP_RESERVE0 + i,
					start_val + 0x1000 + i);

	idx = 2;
	//set ram table:
	for (i = 0; i < 13; i++)
		dpss_ram_tab_sbuf_add_w(ram_tab, idx, FRC_REG_TOP_RESERVE0 + i,
					start_val + 0x2000 + i);

	idx = 3;
	//set ram table:
	for (i = 0; i < 16; i++)
		dpss_ram_tab_sbuf_add_w(ram_tab, idx, FRC_REG_TOP_RESERVE0 + i,
					start_val + 0x3000 + i);

	dpss_rdma_cfg_multi_buf(ctr_tab, ram_tab);
}

void print_last_tab(unsigned char reg_ch,
		    unsigned char ram_tab_idx, unsigned char cmd)
{
	struct dpss_rdma_regadr_s *ctr_tab;
	struct dpss_rdma_info_s *ram_tab;

	if (reg_ch >= DPSS_RDMA_NUM || ram_tab_idx >= DPSS_RDMA_RAM_NUB) {
		DBG_WARN("%s:overflow: %d, %d\n", __func__, reg_ch,
			 ram_tab_idx);
		return;
	}
	ctr_tab = &rdma_regadr_dpss_t6w[reg_ch];
	ram_tab = rdma_ram_tab[ram_tab_idx];

	print_s_rdma_reg(ctr_tab, "ctr_tab");	//dbg
	print_s_rdma_info(ram_tab, "ram_tab_info");

	if (cmd == 1)
		print_rdma_data(ram_tab, "ram_tab_data");
	else if (cmd == 2)
		print_rdma_s_data_all(ram_tab, "ram_tab_data");
}

/***************************************************
 * test 1
 *	man:
 *		step : clear test register | ram table
 *		step : trig man
 *			wait irq then
 *		step : print test
 * cmd, 0: test base
 *	para 1: ch_id =
 */
static unsigned int dpss_rdma_dbg_last_info;
void dbg_rdma_process(u32 *para, u32 cnt)
{
//      int i;
	u32 cmd, val_start, src_int, last_md;
	u8 ch_id, ram_id, src_bit;

	if (cnt > 4 || cnt < 1) {
		DBG_WARN("%s:cnt[%d]overflow\n", __func__, cnt);
		return;
	}
	cmd = para[0];
	DBG_INF("%s:cmd = %d\n", __func__, cmd);
	if (cmd == 0) {
		para[1] = dpss_rdma_dbg_last_info;
	} else if (cmd >= 1 && cmd <= 4) {
		dpss_rdma_dbg_last_info = para[1];
		dpss_rdma_dbg_last_info |= (1 << 24);
	} else if (cmd == 11) {
		dpss_rdma_dbg_last_info = para[1];
		dpss_rdma_dbg_last_info |= (2 << 24);
	}
//      DBG_INF("%s:last_info=0x%x\n", __func__, dpss_rdma_dbg_last_info);
	ch_id = (u8)(para[1] & 0xff);
	ram_id = (u8)((para[1] >> 8) & 0xff);
	src_bit = (u8)((para[1] >> 16) & 0xff);
	if (src_bit > 31)
		src_int = 0xffff;
	else
		src_int = 1 << src_bit;

	val_start = para[2];

	DBG_INF("ctr_id[%d], ram_id[%d], val_start[0x%x], src_int[0x%x]\n",
		ch_id, ram_id, val_start, src_int);

	switch (cmd) {
	case 0:		//rd last status:
		last_md = (dpss_rdma_dbg_last_info >> 24) & 0xff;
		DBG_INF("mode = %d\n", last_md);
		print_last_tab(ch_id, ram_id, last_md);

		break;
	case 1:		//common rdma:
		dbg_rdma_prepare_tst1(ch_id, ram_id, val_start, src_int);
		break;
	case 2:
		dbg_rdma_prepare_tst2(ch_id, ram_id, val_start, src_int);

		break;
	case 3:
		dbg_rdma_prepare_tst3(ch_id, ram_id, val_start, src_int);

		break;
	case 4:
		dbg_rdma_prepare_tst4(ch_id, ram_id, val_start, src_int);

		break;
	case 10:
		dpss_rdma_man_trig();
		break;
	case 11:		//common rdma:
		dbg_rdma_prepare_tst11(ch_id, ram_id, val_start, src_int);

		break;
	case 20:		//rd test register:
		dbg_rdma_tst_register_print();
		break;
	case 21:		//
		dbg_rdma_tst_register_clr();
		break;
	case 22:
		dbg_rd_rdma_reg_val(ch_id);
		break;
	case 23:
		dpss_rdma_irq_src_clear(ch_id);
		break;
	default:
		DBG_INF("%s:cmd=%d not define\n", __func__, cmd);
		break;
	}
}

//----------------------------------------------------------------------
void tst_0_check_rmda_ch(void)
{
	int i;
	struct dpss_rdma_regadr_s *ctr_tab;

	for (i = 0; i < DPSS_RDMA_NUM; i++) {
		ctr_tab = dpss_rdma_get_ch_tab(i);
		if (ctr_tab)
			print_s_rdma_reg(ctr_tab, "ctr_tab");
	}
}

//----------------------------------------------------------------------
#define DPSS_RDMA_CH_NUB	(16)

struct dpss_rdma_mng_s {
	unsigned int ch_num;
	struct dpss_rdma_info_s info[DPSS_RDMA_CH_NUB];
};

static DEFINE_SPINLOCK(dpss_rdma_lock);

struct dpss_rdma_mng_s g_dpss_rdma_mng;
struct dpss_rdma_pre_s *g_dpss_rdma_pre_tab_dpss;
struct dpss_rdma_pre_s *g_dpss_rdma_pre_tab_vc;

static struct dpss_rdma_mng_s *dpss_get_rdma_mng(void)
{
	return &g_dpss_rdma_mng;
}

void sprint_rdma_s_data_top(struct seq_file *s, unsigned int para)
{
	struct dpss_rdma_info_s *p;
	//char *name;
	unsigned int ch, rdma_ch, buf_idx;
	struct dpss_rdma_mng_s *mng;
	struct dpss_ch_s *pch;

	ch = para & 0xff;
	rdma_ch = (para & 0xff00) >> 8;
	buf_idx = (para & 0xff0000) >> 16;

	seq_printf(s, "ch =%d, rdma_ch = %d, buf_idx = %d\n", ch, rdma_ch,
		   buf_idx);
	mng = dpss_get_rdma_mng();
	pch = dpss_get_ch(ch);
	if (ch >= DPSS_CHANNEL_MAX || rdma_ch >= DPSS_RDMA_CH_NUB ||
		buf_idx >= 16) {
		seq_printf(s, "%s\n", "over flow");
		return;
	}
	seq_printf(s, "handle = %d, %d, %d %d\n",
		   pch->c.rdma_handle_dae,
		   pch->c.rdma_handle_dpe[0],
		   pch->c.vpp_rdma_handle_dpe[0],
		   pch->c.vpp_rdma_handle_dpe[1]);
	p = &mng->info[rdma_ch];
	sprint_rdma_s_data(s, p, buf_idx, "buf");
}

//
void sprint_rdma_info(struct seq_file *s, unsigned int para)
{
	struct dpss_rdma_info_s *p;
	//char *name;
	unsigned int ch, rdma_ch;
	struct dpss_rdma_mng_s *mng;
	struct dpss_ch_s *pch;

	ch = para & 0xff;
	rdma_ch = (para & 0xff00) >> 8;
	//buf_idx = (para  & 0xff0000) >> 16;

	seq_printf(s, "%s ch =%d, rdma_ch = %d\n", __func__, ch, rdma_ch);
	mng = dpss_get_rdma_mng();
	pch = dpss_get_ch(ch);
	if (ch >= DPSS_CHANNEL_MAX || rdma_ch >= DPSS_RDMA_CH_NUB) {
		seq_printf(s, "%s\n", "over flow");
		return;
	}
	seq_printf(s, "handle = %d, %d, %d:%d\n",
		   pch->c.rdma_handle_dae,
		   pch->c.rdma_handle_dpe[0],
		   pch->c.vpp_rdma_handle_dpe[0],
		   pch->c.vpp_rdma_handle_dpe[1]);
	p = &mng->info[rdma_ch];
	sprint_s_rdma_info(s, p, "rdma_info");
}

void dpss_rdma_probe(void)
{
	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();

	memset(mng, 0, sizeof(*mng));
	mng->ch_num = DPSS_RDMA_CH_NUB;
}

#ifndef DPSS_RDMA_HAND_TEST
//frc_rdma_isr
void dpss_rdma_isr(void)
{
	int i;
	u32 rdma_status;
	u32 off_done = 4, off;
	u32 off_clear = 16;
	bool flg_find = false;
	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();
	struct dpss_rdma_info_s *info = NULL;

	rdma_status = rd(DPSS_RDMA_STATUS1);	//[19:4] is done flag

	dbg_a2(" 2 rdma status:[0x%x][0x%x][0x%x][0x%x]\n",
	       rd(DPSS_RDMA_STATUS),
	       rdma_status, rd(DPSS_RDMA_STATUS2), rd(DPSS_RDMA_STATUS3));

	if (rdma_status & 0xffff0) {
		for (i = 0; i < DPSS_RDMA_NUM; i++) {
			off = i + off_done;
			if (rdma_status & (0x1 << off)) {
				dbg_a2("rdma ch %d\n", i);
				info = &mng->info[i];
				//dbg_a2("%s:info:%p\n", __func__, info);
				if (info && info->op && info->op->irq_cb) {
					//dbg_a2("%s:arg:%p\n", __func__, info->op->arg);
					info->op->irq_cb(info->op->arg);
					info->irq_count++;
				}
				w_reg_bit(DPSS_RDMA_CTRL, 0x1, off_clear + i,
					  1);
				flg_find = true;
				break;
			}
		}
	}
	if (!flg_find)
		DBG_WARN("rdma:can't find irq\n");
}
#endif

//----------------------------------------------------------------------

int dpss_rdma_register(struct rdma_op_s *rdma_op, void *op_ara,
		       unsigned int mbuf_num,
		       unsigned int buf_step, unsigned int rdma_mode)
{
	int i = -1;
	unsigned long flags;
	dma_addr_t dma_handle;
	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();	//tmp
	unsigned int buf_size;
	struct dpss_dev_s *de_devp = dpss_get_devp();
	struct dpss_rdma_info_s *info = NULL;
	unsigned int irq_src = 0;

	if (!(rdma_mode & DPSS_RDMA_MODE_IRQ_SRC_MAN))
		irq_src = 1 << ((rdma_mode & DPSS_RDMA_MODE_IRQ_SRC) >> 8);
	spin_lock_irqsave(&dpss_rdma_lock, flags);
	/* ch 0 for man */
	if (rdma_mode & DPSS_RDMA_MODE_IRQ_SRC_MAN) {
		info = &mng->info[0];
		if (!info->op && !info->used) {
			info->op = rdma_op;
			//info->op_arg = op_ara;
			info->irq_count = 0;
			info->used = true;
			//dbg_a2("%s:man:op:%p,%p,arg=%p\n",
			//__func__, info->op, info->op->irq_cb, info->op->arg);
		} else {
			info = NULL;
		}
	} else {
		for (i = 1; i < mng->ch_num; i++) {
			info = &mng->info[i];
			if (!info->op && !info->used) {
				info->op = rdma_op;
				//info->op_arg = op_ara;
				info->irq_count = 0;
				info->used = true;
				info->int_src = irq_src;
				//dbg_a2("%s:%d:op:%p,%p,arg=%p\n",
				//__func__, i, info->op, info->op->irq_cb, info->op->arg);
				break;
			}
			info = NULL;
		}
	}
	spin_unlock_irqrestore(&dpss_rdma_lock, flags);

	if (!info) {
		DBG_WARN("%s:register failed\n", __func__);
		return -1;
	}

	buf_size = mbuf_num * buf_step;

	if (info->tab_size == 0) {
		info->v_addr = dma_alloc_coherent(&de_devp->pdev->dev,
						  buf_size,
						  &dma_handle, GFP_KERNEL);
		info->phy_addr = (u64)dma_handle;
		//mng->info[i].reg_buf = kmalloc(buf_size, GFP_KERNEL);

		info->tab_size = buf_size;
		info->step = buf_step;
		info->buf_nub = mbuf_num;
		info->buf_nub_used = mbuf_num;
		info->buf_status = C_BIT0;
		//rd or wr:
		if (rdma_mode & DPSS_RDMA_MODE_RD_WR)
			info->rd_wr = true;
		else
			info->rd_wr = false;
		if (rdma_mode & DPSS_RDMA_MODE_INCL)
			info->incl = true;
		else
			info->incl = false;
		dbg_a2("%s:alloc\n", __func__);
	}

	if (!info->v_addr) {
		dma_free_coherent(&de_devp->pdev->dev,
				  buf_size,
				  info->v_addr, (dma_addr_t)info->phy_addr);
		info->v_addr = NULL;

		info->tab_size = 0;
		info->op = NULL;
		i = -1;
		DBG_WARN("%s: memory allocate fail\n", __func__);
	} else {
		if (dpss_dbg & DPSS_M_RDMA && (dpss_dbg & 3) > 2) {	//dbg only
			print_s_rdma_info(info, "register rdma");
		}
		dbg_a0("%s success, handle %d table_size %d\n",
		       __func__, i, buf_size);
	}
	return i;
}

void dpss_rdma_para_chg_irq_src(int handle, unsigned int irq_src_bit)
{
	struct dpss_rdma_info_s *info;
	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();	//tmp

	if (handle < 0 || handle >= mng->ch_num) {
		DBG_WARN("%s:overflow:%d\n", __func__, handle);
		return;
	}
	info = &mng->info[handle];

	info->int_src = 1 << irq_src_bit;

	dbg_a2("%s:handle, irq_src=0x%x\n", __func__, info->int_src);
}

void dpss_rdma_para_chg_used_nub(int handle, unsigned int used_nub)
{
	struct dpss_rdma_info_s *info;
	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();	//tmp

	if (handle < 0 || handle >= mng->ch_num) {
		DBG_WARN("%s:overflow:%d\n", __func__, handle);
		return;
	}
	info = &mng->info[handle];

	if (used_nub <= info->buf_nub)
		info->buf_nub_used = used_nub;
	else
		DBG_WARN("nub:overflow:%d %d\n", used_nub, info->buf_nub);

	dbg_a2("%s:handle buf_nub_used=0x%x\n", __func__, info->buf_nub_used);
}

void dpss_rdma_irq_src_clear(int handle)
{
	struct dpss_rdma_info_s *info;
	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();	//tmp
	struct dpss_rdma_regadr_s *ctr_tab;

	if (handle < 0 || handle >= mng->ch_num) {
		DBG_WARN("%s:overflow:%d\n", __func__, handle);
		return;
	}
	info = &mng->info[handle];

	ctr_tab = &rdma_regadr_dpss_t6w[handle];
	//w_reg_bit(DPSS_RDMA_ACCESS_AUTO4, 0, ctr_tab->ch_id, 1);
	wr(ctr_tab->int_src_man_trig, 0);

	dbg_a2("%s:handle=%d\n", __func__, handle);
}

void dpss_rdma_unregister(int handle)
{
	unsigned long flags;
	struct dpss_dev_s *de_devp = dpss_get_devp();
	struct dpss_rdma_info_s *info;

	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();	//tmp

	if (handle < 0 || handle >= mng->ch_num) {
		DBG_WARN("%s:overflow:%d\n", __func__, handle);
		return;
	}
	dpss_rdma_irq_src_clear(handle);

	info = &mng->info[handle];

	/*rdma_clear(i); */
//      info->op_arg = NULL;

	if (info->v_addr) {
		dma_free_coherent
		    (&de_devp->pdev->dev,
		     info->tab_size, info->v_addr, (dma_addr_t)info->phy_addr);
		info->v_addr = NULL;
	}
	info->tab_size = 0;
	spin_lock_irqsave(&dpss_rdma_lock, flags);
	info->op = NULL;
	memset(info, 0, sizeof(*info));
	spin_unlock_irqrestore(&dpss_rdma_lock, flags);
}

void dpss_pre_tab_add_w(struct dpss_rdma_pre_s *pre_tab, u32 addr, u32 val)
{
	int i;

	i = pre_tab->cnt;

//      dbg_a2("i:%d, addr:%x, val:%x\n", i, addr, val);

	if ((i + 1) * 8 > pre_tab->t_size) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__,
			i, pre_tab->t_size);
		return;
	}

	pre_tab->buf[i * 2] = addr;
	pre_tab->buf[i * 2 + 1] = val;

	pre_tab->cnt++;
}

void dpss_pre_tab_fill_null(struct dpss_rdma_pre_s *pre_tab, u32 nub)
{
	int i;

	dbg_a2("%s:%d\n", __func__, nub);
	for (i = 0; i < nub; i++)
		dpss_pre_tab_add_w(pre_tab, FRC_REG_TOP_RESERVE0, i);
}

void dpss_pre_tab_chg(struct dpss_rdma_pre_s *pre_tab, u32 addr, u32 val)
{
	int i, j;
	//unsigned int read_val = 0, val_new = 0;
	unsigned int match = 0;

	i = pre_tab->cnt;

	dbg_a2("i:%d, addr:%x, val:%x\n", i, addr, val);
	//find:
	for (j = (pre_tab->cnt - 1); j >= 0; j--) {
		if (pre_tab->buf[j << 1] == addr) {
			match = 1;
			//read_val = pre_tab->buf[(j << 1) + 1];
			break;
		}
	}

	if (!match) {
		dbg_a2("%s:warn:can't find\n", __func__);
		return;
	}
	pre_tab->buf[(j << 1) + 1] = val;
}

void dpss_pre_tab_chg_bits(struct dpss_rdma_pre_s *pre_tab,
			   u32 addr, u32 val, u32 start, u32 len)
{
	int i, j;
	unsigned int read_val = 0, val_new = 0;
	unsigned int match = 0;

	i = pre_tab->cnt;

	dbg_a2("i:%d, addr:%x, val:%x\n", i, addr, val);
	//find:
	for (j = (pre_tab->cnt - 1); j >= 0; j--) {
		if (pre_tab->buf[j << 1] == addr) {
			match = 1;
			read_val = pre_tab->buf[(j << 1) + 1];
			break;
		}
	}

	if (!match) {
		dbg_a2("%s:warn:can't find\n", __func__);
		return;
	}
	val_new = dpss_uint_up_bits(read_val, val, start, len);

	pre_tab->buf[(j << 1) + 1] = val_new;
}

void dpss_pre_tab_add_w_bits(struct dpss_rdma_pre_s *pre_tab, u32 addr, u32 val,
			     u32 start, u32 len)
{
	int i, j;
	unsigned int read_val = 0, val_new = 0;
	unsigned int match = 0;

	i = pre_tab->cnt;

//      dbg_a2("i:%d, addr:%x, val:%x\n", i, addr, val);

	if ((i + 1) * 8 > pre_tab->t_size) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__,
			i, pre_tab->t_size);
		return;
	}
	read_val = rd(addr);

	//find:
	for (j = (pre_tab->cnt - 1); j >= 0; j--) {
		if (pre_tab->buf[j << 1] == addr) {
			match = 1;
			read_val = pre_tab->buf[(j << 1) + 1];
			break;
		}
	}
//      dbg_a2("%s:match = %d\n", __func__,match);

	val_new = dpss_uint_up_bits(read_val, val, start, len);

	pre_tab->buf[i * 2] = addr;
	pre_tab->buf[i * 2 + 1] = val_new;
//      dbg_a2("\t 0x%x:0x%x:0x%x\n", addr, read_val, val_new);

	pre_tab->cnt++;
}

void dpss_pre_tab_add_w_bits_vc(struct dpss_rdma_pre_s *pre_tab, u32 addr,
				u32 val, u32 start, u32 len)
{
	int i, j;
	unsigned int read_val = 0, val_new = 0;
	unsigned int match = 0;

	i = pre_tab->cnt;

//      dbg_a2("i:%d, addr:%x, val:%x\n", i, addr, val);

	if ((i + 1) * 8 > pre_tab->t_size) {
		DBG_ERR("%s:overflow: %d:size=%d\n", __func__,
			i, pre_tab->t_size);
		return;
	}
	read_val = rd_vc(addr);

	//find:
	for (j = (pre_tab->cnt - 1); j >= 0; j--) {
		if (pre_tab->buf[j << 1] == addr) {
			match = 1;
			read_val = pre_tab->buf[(j << 1) + 1];
			break;
		}
	}
//      dbg_a2("%s:match = %d\n", __func__,match);

	val_new = dpss_uint_up_bits(read_val, val, start, len);

	pre_tab->buf[i * 2] = addr;
	pre_tab->buf[i * 2 + 1] = val_new;
//      dbg_a2("\t 0x%x:0x%x:0x%x\n", addr, read_val, val_new);

	pre_tab->cnt++;
}

void dpss_pre_tab_append(struct dpss_rdma_pre_s *pre_tab,
			 struct dpss_rdma_pre_s *sub)
{
	u8 *buf_t, *buf_f;
	unsigned int size;

	if (!pre_tab || !pre_tab->buf || !sub || !sub->buf || !sub->cnt) {
		DBG_WARN("%s:no input? %p, %p\n", __func__, pre_tab, sub);
		return;
	}
	size = sub->cnt * (sizeof(u32)) * 2;
	if ((pre_tab->cnt + sub->cnt) * (sizeof(u32)) * 2 > pre_tab->t_size) {
		DBG_WARN("%s:overflow:%d:%d:%d\n", __func__,
			 pre_tab->t_size, pre_tab->cnt, sub->cnt);
	}
	buf_t = (u8 *)(&pre_tab->buf[pre_tab->cnt * 2]);
	buf_f = (u8 *)(&sub->buf[0]);
	memcpy(buf_t, buf_f, size);
	pre_tab->cnt = pre_tab->cnt + sub->cnt;
}

int dpss_rdma_mbuf_fill_buf(int handle, int mbuf_index,
			    struct dpss_rdma_pre_s *pre_tab)
{
	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();
	struct dpss_rdma_info_s *info;
	u32 *buf = NULL;

	if (handle <= 0 ||
	    handle >= mng->ch_num ||
	    mbuf_index < 0 ||
	    mbuf_index >= mng->info[handle].buf_nub || !pre_tab) {
		DBG_WARN("%s (handle == %d) (mbuf_index = %d) not allowed\n",
			 __func__, handle, mbuf_index);
		return -1;
	}
	info = &mng->info[handle];
	buf = dpss_ram_sbuf_get_addr(info, mbuf_index);
	memcpy(buf, pre_tab->buf, pre_tab->cnt * sizeof(u32) * 2);

	if (info->buf_nub_used == 1) {
		info->item_cnt = pre_tab->cnt;
		info->length[0] = pre_tab->cnt;
	} else {
		info->length[mbuf_index] = pre_tab->cnt;
	}
#ifdef RUN_ON_PC
	if (dpss_dbg & DPSS_M_RDMA && (dpss_dbg & 0x03) > 2) {	//dbg only
		print_rdma_s_data(info, mbuf_index, "data table");
	}
#endif
	return 0;
}

#ifdef MOV
int dpss_rdma_mbuf_config(int handle, int mbuf_index,
			  struct dpss_rdma_pre_s *pre_tab)
{
	int ret = 0;
	unsigned long flags;
	struct dpss_rdma_regadr_s *ctr_tab;
	u32 *buf = NULL;

	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();
	struct dpss_rdma_info_s *info;

	if (handle <= 0 ||
	    handle >= mng->ch_num ||
	    mbuf_index < 0 ||
	    mbuf_index >= mng->info[handle].buf_nub || !pre_tab) {
		DBG_WARN("%s (handle == %d) (mbuf_index = %d) not allowed\n",
			 __func__, handle, mbuf_index);
		return -1;
	}
	spin_lock_irqsave(&dpss_rdma_lock, flags);
	//set ctr tab
	ctr_tab = &rdma_regadr_dpss_t6w[handle];
	info = &mng->info[handle];
	buf = dpss_ram_sbuf_get_addr(info, mbuf_index);
	memcpy(buf, pre_tab->buf, pre_tab->cnt * sizeof(u32) * 2);

	if (info->buf_nub == 1) {
		info->item_cnt = pre_tab->cnt;
		dpss_rdma_cfg(ctr_tab, info);
	} else {
		info->length[mbuf_index] = pre_tab->cnt;
		dpss_rdma_cfg_multi_buf(ctr_tab, info);
	}

	spin_unlock_irqrestore(&dpss_rdma_lock, flags);
#ifdef RUN_ON_PC
	if (dpss_dbg & DPSS_M_RDMA && (dpss_dbg & 0x03) > 2) {	//dbg only
//              print_s_rdma_reg(ctr_tab, "control:");
		dbg_rd_rdma_reg_val(handle);
		print_s_rdma_info(info, "register rdma");
		print_rdma_s_data(info, mbuf_index, "data table");
	}
#endif
	return ret;
}
#else

int dpss_rdma_mbuf_config(int handle)
{
	int ret = 0;
	unsigned long flags;
	struct dpss_rdma_regadr_s *ctr_tab;
//      u32 *buf = NULL;

	struct dpss_rdma_mng_s *mng = dpss_get_rdma_mng();
	struct dpss_rdma_info_s *info;

	if (handle <= 0 || handle >= mng->ch_num) {
		DBG_WARN("%s (handle == %d) not allowed\n", __func__, handle);
		return -1;
	}

	spin_lock_irqsave(&dpss_rdma_lock, flags);
	//set ctr tab
	ctr_tab = &rdma_regadr_dpss_t6w[handle];
	info = &mng->info[handle];

	if (info->buf_nub_used == 1)
		dpss_rdma_cfg(ctr_tab, info);
	else
		dpss_rdma_cfg_multi_buf(ctr_tab, info);

	spin_unlock_irqrestore(&dpss_rdma_lock, flags);
#ifdef RUN_ON_PC
	if (dpss_dbg & DPSS_M_RDMA && (dpss_dbg & 0x03) > 2) {	//dbg only
//              print_s_rdma_reg(ctr_tab, "control:");
		dbg_rd_rdma_reg_val(handle);
		print_s_rdma_info(info, "register rdma");
	}
#endif
	return ret;
}

#endif
void dpss_pre_tab_add_w_dpss(u32 addr, u32 val)
{
	struct dpss_rdma_pre_s *pre_tab = g_dpss_rdma_pre_tab_dpss;

	if (!pre_tab) {
		DBG_WARN("%s:no tab:0x%x, 0x%x\n", __func__, addr, val);
		return;
	}
	dpss_pre_tab_add_w(pre_tab, addr, val);
}

void dpss_pre_tab_add_wb_dpss(u32 addr, u32 val, u32 start, u32 len)
{
	struct dpss_rdma_pre_s *pre_tab = g_dpss_rdma_pre_tab_dpss;

	if (!pre_tab) {
		DBG_WARN("%s:no tab:0x%x, 0x%x\n", __func__, addr, val);
		return;
	}
	dpss_pre_tab_add_w_bits(pre_tab, addr, val, start, len);
}

void dpss_pre_tab_add_w_vc(u32 addr, u32 val)
{
	struct dpss_rdma_pre_s *pre_tab = g_dpss_rdma_pre_tab_vc;

	if (!pre_tab) {
		DBG_WARN("%s:no tab:0x%x, 0x%x\n", __func__, addr, val);
		return;
	}
	dpss_pre_tab_add_w(pre_tab, addr, val);
}

void dpss_pre_tab_add_wb_vc(u32 addr, u32 val, u32 start, u32 len)
{
	struct dpss_rdma_pre_s *pre_tab = g_dpss_rdma_pre_tab_vc;

	if (!pre_tab) {
		DBG_WARN("%s:no tab:0x%x, 0x%x\n", __func__, addr, val);
		return;
	}
	dpss_pre_tab_add_w_bits_vc(pre_tab, addr, val, start, len);
}

void dpss_rdma_set_v_pre_tab(struct dpss_rdma_pre_s *pre_tab)
{
	g_dpss_rdma_pre_tab_vc = pre_tab;
}

void dpss_rdma_set_d_pre_tab(struct dpss_rdma_pre_s *pre_tab)
{
	g_dpss_rdma_pre_tab_dpss = pre_tab;
}

void dpss_v_rdma_m_cp_tab(struct dpss_rdma_pre_s *pre_tab, unsigned int m_idx,
			  int handle)
{
#ifdef FUNC_EN_VPP_RDMA_M
	int i;

	for (i = 0; i < pre_tab->cnt; i++)
		rdma_mbuf_write_reg(handle, m_idx, pre_tab->buf[2 * i],
				    pre_tab->buf[2 * i + 1]);
#endif
}

void dpss_v_rdma_cp_tab(struct dpss_rdma_pre_s *pre_tab, int handle)
{
	int i;

	for (i = 0; i < pre_tab->cnt; i++)
		rdma_write_reg(handle, pre_tab->buf[2 * i],
			       pre_tab->buf[2 * i + 1]);
}

// rdma driver

//----------------------------------------------------------------------

//below:use VPP RDMA:
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA

static void _rdma_irq(void *arg)
{
	struct dpss_v_rdma_s *rdma_s = arg;
#ifdef MOV
	int ret;

	ret = rdma_config(rdma_s->handle,
			  (rdma_s->irq_src) ?
			  rdma_s->irq_src : RDMA_TRIGGER_MANUAL);
#endif
	dbg_w2("vpp_rdma:%s\n", rdma_s->name);
	rdma_s->irq_cnt++;
	rdma_s->undone_cnt = 0;
}

void _v_rdma_config(struct dpss_v_rdma_s *rdma_s)
{
	int ret;

	if (!rdma_s || !rdma_s->flg_register) {
		DBG_WARN("%s:no rdma or not register\n", __func__);
		return;
	}
	ret = rdma_config(rdma_s->handle,
			  (rdma_s->irq_src) ?
			  rdma_s->irq_src : RDMA_TRIGGER_MANUAL);
	DBG_INF("%s:%s:%d\n", __func__, rdma_s->name, ret);
}

void _v_rdma_reg(struct dpss_v_rdma_s *rdma_s)
{
	//rdma_s = dpss_get_v_rdma_d();
	if (!rdma_s) {
		DBG_WARN("%s:no rdma_s\n", __func__);
		return;
	}

	if (!rdma_s->flg_register) {
		if (!rdma_s->irq_src) {
#if RDMA_MANUAL_MODE
			rdma_s->handle = rdma_register_manual(&rdma_s->rdma_op,
							      NULL,
							      rdma_s->size);
#endif
		} else {
			rdma_s->handle = rdma_register(&rdma_s->rdma_op,
						       NULL, rdma_s->size);
		}
		rdma_s->flg_register = true;
		DBG_INF("%s:register:%s,handle %d,src %d\n", __func__,
			rdma_s->name, rdma_s->handle, rdma_s->irq_src);
	} else {
		DBG_WARN("%s:%d: rdma not register\n", rdma_s->name,
			 rdma_s->handle);
	}
}

void _v_rdma_unreg(struct dpss_v_rdma_s *rdma_s)
{
	if (!rdma_s || !rdma_s->flg_register) {
		DBG_WARN("%s:no rdma_s\n", __func__);
		return;
	}
	rdma_unregister(rdma_s->handle);
	rdma_s->flg_register = false;
	rdma_s->handle = 0;
}

void _v_rdma_stop(struct dpss_v_rdma_s *rdma_s)
{
//      rdma_s = dpss_get_v_rdma_d();
	if (rdma_s->flg_register)
		rdma_clear(rdma_s->handle);
	else
		DBG_WARN("%s:no rdma\n", __func__);
}

void _v_rdma_set_flg(struct dpss_v_rdma_s *rdma_s)
{
	if (!rdma_s) {
		DBG_WARN("%s:no input\n", __func__);
		return;
	}
	rdma_s->have_driver = true;
}
#else

void _v_rdma_set_flg(struct dpss_v_rdma_s *rdma_s)
{
	if (!rdma_s) {
		DBG_WARN("%s:b:no input\n", __func__);
		return;
		rdma_s->have_driver = false;
	}
}

static void _rdma_irq(void *arg)
{
}

void _v_rdma_config(struct dpss_v_rdma_s *rdma_s)
{
}

void _v_rdma_reg(struct dpss_v_rdma_s *rdma_s)
{
}

void _v_rdma_unreg(struct dpss_v_rdma_s *rdma_s)
{
}

void _v_rdma_stop(struct dpss_v_rdma_s *rdma_s)
{
}

//int rdma_register(struct rdma_op_s *rdma_op, void *op_arg, int table_size)
//void rdma_unregister(int i)
//int rdma_clear(int handle)
//u32 rdma_read_reg(int handle, u32 adr)
//int rdma_write_reg(int handle, u32 adr, u32 val)
//int rdma_write_reg_bits(int handle, u32 adr, u32 val, u32 start, u32 len)
#endif				/* CONFIG_AMLOGIC_MEDIA_RDMA */

struct dpss_v_rdma_s *dpss_rdma_s0, *dpss_rdma_s1;
static unsigned int dpss_dbg_v_rdma;
module_param_named(dpss_dbg_v_rdma, dpss_dbg_v_rdma, uint, 0664);

void dpss_v_rdma_prob(void)
{
	struct dpss_v_rdma_s *rdma_s0, *rdma_s1;

	rdma_s0 = dpss_get_v_rdma_d_0();
	rdma_s1 = dpss_get_v_rdma_d_1();

	_v_rdma_set_flg(rdma_s0);
	_v_rdma_set_flg(rdma_s1);

	if (!rdma_s0->have_driver || !rdma_s1->have_driver) {
		DBG_WARN("%s:0:no driver:\n", __func__);
		dpss_rdma_s0 = NULL;
		dpss_rdma_s1 = NULL;
		return;
	}
	rdma_s0->size = 0x1000;
	rdma_s0->name = "s0";
	if (dpss_dbg_v_rdma & 0xff00) {
		rdma_s0->size =
		    0x1000 * ((dpss_dbg_v_rdma & 0xff00) >> 8);
		DBG_INF("%s:s0:size=0x%x\n", __func__, rdma_s0->size);
	}
	rdma_s0->irq_src = C_BIT4;	//for small picture
	if (dpss_dbg_v_rdma & 0x1f) {
		rdma_s0->irq_src = 1 << (dpss_dbg_v_rdma & 0x1f);
		DBG_INF("%s:s0:irq_src=0x%x\n", __func__,
			rdma_s0->irq_src);
	}
	rdma_s0->rdma_op.arg = rdma_s0;
	rdma_s0->rdma_op.irq_cb = _rdma_irq;
	_v_rdma_reg(rdma_s0);

	rdma_s1->size = 0x4000;
	rdma_s1->name = "s1";
	if (dpss_dbg_v_rdma & 0xff000000) {
		rdma_s1->size =
		    0x1000 * ((dpss_dbg_v_rdma & 0xff000000) >> 24);
		DBG_INF("%s:s1:size=0x%x\n", __func__, rdma_s1->size);
	}
#ifdef FUNC_EN_DD
	rdma_s1->irq_src = C_BIT23;	//dpe frm_rst
#else
	rdma_s1->irq_src = C_BIT6;
#endif
	if (dpss_dbg_v_rdma & 0x1f0000) {
		rdma_s1->irq_src =
		    1 << ((dpss_dbg_v_rdma & 0x1f0000) >> 16);
		DBG_INF("%s:s1:irq_src=0x%x\n", __func__,
			rdma_s1->irq_src);
	}
#if RDMA_MANUAL_MODE
	rdma_s1->irq_src = 0;
#endif
	rdma_s1->rdma_op.arg = rdma_s1;
	rdma_s1->rdma_op.irq_cb = _rdma_irq;
	_v_rdma_reg(rdma_s1);

	dpss_rdma_s0 = rdma_s0;
	dpss_rdma_s1 = rdma_s1;
	dbg_a1("%s\n", __func__);
}

void dpss_v_rdma_remove(void)
{
	struct dpss_v_rdma_s *rdma_s0, *rdma_s1;

	rdma_s0 = dpss_get_v_rdma_d_0();
	rdma_s1 = dpss_get_v_rdma_d_0();

	if (!rdma_s0->have_driver || !rdma_s1->have_driver) {
		DBG_WARN("%s:no driver:\n", __func__);
		return;
	}

	dpss_rdma_s0 = NULL;
	dpss_rdma_s1 = NULL;
	_v_rdma_unreg(rdma_s0);
	_v_rdma_unreg(rdma_s1);
}

void dpss_v_stop(void)
{
	struct dpss_v_rdma_s *rdma_s0, *rdma_s1;

	rdma_s0 = dpss_get_v_rdma_d_0();
	rdma_s1 = dpss_get_v_rdma_d_0();

	if (!rdma_s0->have_driver || !rdma_s1->have_driver) {
		DBG_WARN("%s:no driver:\n", __func__);
		return;
	}
	_v_rdma_stop(rdma_s0);
	_v_rdma_stop(rdma_s1);
}

#ifdef CONFIG_AMLOGIC_MEDIA_DPSS
void dpss_v_rdma_config_a(void)
{
	_v_rdma_config(dpss_rdma_s0);
}

void dpss_v_rdma_config_b(void)
{
	_v_rdma_config(dpss_rdma_s1);
}
EXPORT_SYMBOL(dpss_v_rdma_config_b);
#endif

int DPSS_A_WR_MPEG_REG(u32 adr, u32 val)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (dpss_rdma_s0 && dpss_rdma_s0->flg_register)
		rdma_write_reg(dpss_rdma_s0->handle, adr, val);
	else
		wr_vc(adr, val);
#else
	wr_vc(adr, val);
#endif
	return 0;
}
EXPORT_SYMBOL(DPSS_A_WR_MPEG_REG);

int DPSS_A_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (dpss_rdma_s0 && dpss_rdma_s0->flg_register) {
		rdma_write_reg_bits(dpss_rdma_s0->handle,
				    adr, val, start, len);
	} else {
		w_reg_bit_vc(adr, val, start, len);
	}
#else
	w_reg_bit_vc(adr, val, start, len);
#endif
	return 0;
}
EXPORT_SYMBOL(DPSS_A_WR_MPEG_REG_BITS);

u32 DPSS_A_RD_MPEG_REG(u32 adr)
{
	u32 read_val = rd_vc(adr);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA

	if (dpss_rdma_s0 && dpss_rdma_s0->flg_register)
		read_val = rdma_read_reg(dpss_rdma_s0->handle, adr);
#endif
	return read_val;
}
EXPORT_SYMBOL(DPSS_A_RD_MPEG_REG);

#ifdef CONFIG_AMLOGIC_MEDIA_DPSS
int DPSS_B_WR_MPEG_REG(u32 adr, u32 val)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (dpss_rdma_s1 && dpss_rdma_s1->flg_register)
		rdma_write_reg(dpss_rdma_s1->handle, adr, val);
	else
		wr_vc(adr, val);
#else
	wr_vc(adr, val);
#endif
	return 0;
}
EXPORT_SYMBOL(DPSS_B_WR_MPEG_REG);

int DPSS_B_WR_MPEG_REG_BITS(u32 adr, u32 val, u32 start, u32 len)
{
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	if (dpss_rdma_s1 && dpss_rdma_s1->flg_register) {
		rdma_write_reg_bits(dpss_rdma_s1->handle,
				    adr, val, start, len);
	} else {
		w_reg_bit_vc(adr, val, start, len);
	}
#else
	w_reg_bit_vc(adr, val, start, len);
#endif
	return 0;
}
EXPORT_SYMBOL(DPSS_B_WR_MPEG_REG_BITS);

u32 DPSS_B_RD_MPEG_REG(u32 adr)
{
	u32 read_val = rd_vc(adr);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA

	if (dpss_rdma_s1 && dpss_rdma_s1->flg_register)
		read_val = rdma_read_reg(dpss_rdma_s1->handle, adr);
#endif
	return read_val;
}
EXPORT_SYMBOL(DPSS_B_RD_MPEG_REG);
#endif

void dpss_dbg_v_rdma_step(unsigned int para)
{
	if (para == 1)
		dpss_v_rdma_prob();
	else if (para == 2)
		dpss_v_rdma_remove();
	else if (para == 3)
		dpss_v_stop();
#ifdef CONFIG_AMLOGIC_MEDIA_DPSS
	else if (para == 4)
		dpss_v_rdma_config_a();
	else if (para == 5)
		dpss_v_rdma_config_b();
#endif
	else
		DBG_INF("%s:no function\n", __func__);
}
