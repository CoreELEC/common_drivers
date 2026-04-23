// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include "sys_def.h"
#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/vmalloc.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <linux/amlogic/media/vpu/vpu.h>
/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>
#endif //RUN_ON_ARM
#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_hw.h"
#include "dpss_func.h"
#include "dpss_rdma_frc.h"

static struct rdma_irq_reg_s irq_status;
static unsigned int rdma_ctrl;

static unsigned int dpss_rdma_en;
static unsigned int buf_alloced;
static struct dpss_rdma_info rdma_info[RDMA_CHANNEL]; // 0: man 1:pre vsync 2:vsync

static unsigned int RDMA_IRQ_PRE_VSYNC = 0x1;
static unsigned int RDMA_IRQ_VSYNC = 0x2;
static unsigned int rdma_handle_irq_pre_vs = 0x2; // pre vs tri bit:1
static unsigned int rdma_handle_irq_vs = 0x1; //vs tri bit:0 update 25/11/12
static unsigned int frc_pv_rdma_err_cnt;
static unsigned int frc_vs_rdma_err_cnt;
static int dpss_rdma_reset_enable;
//dbg
static unsigned int rdma_dbg_en;

#define pr_rdma(level, fmt, arg...)			\
	do {						\
		if (rdma_dbg_en >= (level))	\
			pr_info("dpss_rdma: " fmt, ## arg);	\
	} while (0)

void dpss_rdma_dbg_level(u32 level)
{
	rdma_dbg_en = level;
}

int show_dpss_rdma_dbg_level(void)
{
	return rdma_dbg_en;
}

int show_dpss_rdma_reset_status(void)
{
	return dpss_rdma_reset_enable;
}

void set_dpss_rdma_reset_enable(int enable)
{
	dpss_rdma_reset_enable = enable;
}

void dpss_rdma_force_chg_st(u32 en)
{
	dpss_rdma_en = en;
}

int rdma_enable(void)
{
	if (dpss_rdma_en)
		return 1;
	else
		return 0;
}

int show_dpss_rdma_buf_alloc_st(void)
{
	return buf_alloced;
}

static struct rdma_regadr_s rdma_regadr_t6w[RDMA_CHANNEL] = {
	{
		DPSS_RDMA_AHB_START_ADDR_MAN,
		DPSS_RDMA_AHB_START_ADDR_MAN_MSB,
		DPSS_RDMA_AHB_END_ADDR_MAN,
		DPSS_RDMA_AHB_END_ADDR_MAN_MSB,
		DPSS_RDMA_ACCESS_MAN, 0,
		DPSS_RDMA_ACCESS_MAN, 1,
		DPSS_RDMA_ACCESS_MAN, 2,
		16, 4
	},
	{
		DPSS_RDMA_AHB_START_ADDR_1,
		DPSS_RDMA_AHB_START_ADDR_1_MSB,
		DPSS_RDMA_AHB_END_ADDR_1,
		DPSS_RDMA_AHB_END_ADDR_1_MSB,
		DPSS_RDMA_AUTO_SRC1_SEL,  0,
		DPSS_RDMA_ACCESS_AUTO,  1,
		DPSS_RDMA_ACCESS_AUTO,  5,
		17, 5
	},
	{
		DPSS_RDMA_AHB_START_ADDR_2,
		DPSS_RDMA_AHB_START_ADDR_2_MSB,
		DPSS_RDMA_AHB_END_ADDR_2,
		DPSS_RDMA_AHB_END_ADDR_2_MSB,
		DPSS_RDMA_AUTO_SRC2_SEL,  0,
		DPSS_RDMA_ACCESS_AUTO,  2,
		DPSS_RDMA_ACCESS_AUTO,  6,
		18, 6
	}
};

void dpss_rdma_man_wr_clear(void)
{
	rdma_info[0].rdma_item_count = 0;
}

void dpss_rdma_man_wr_test(unsigned int addr, unsigned int val)
{
	unsigned int i;

	pr_rdma(1, "%s addr:%#x, val:%#x\n", "man", addr, val);

	i = rdma_info[0].rdma_item_count;

	if ((i + 1) * 8 > rdma_info->rdma_table_size) {
		pr_rdma(1, "dpss rdma in buffer overflow\n");
		return;
	}

	rdma_info[0].rdma_table_addr[i * 2] = addr & 0xffffffff;
	rdma_info[0].rdma_table_addr[i * 2 + 1] = val & 0xffffffff;

	rdma_info[0].rdma_item_count++;
}

void dpss_rdma_man_wr_tri(void)
{
	// unsigned int i;
	struct rdma_regadr_s *rdma_regadr;

	pr_rdma(1, "%s man count:%d\n", "man", rdma_info[0].rdma_item_count);

	rdma_regadr = (struct rdma_regadr_s *)rdma_info[0].rdma_regadr;

	if (1) {// (handle == 0) { //man
		// manual RDMA
		wr(rdma_regadr->trigger_mask_reg,
			rd(rdma_regadr->trigger_mask_reg) & (~1));
		#ifdef CONFIG_ARM64
		wr(rdma_regadr->rdma_ahb_start_addr,
			rdma_info[0].rdma_table_phy_addr & 0xffffffff);
		wr(rdma_regadr->rdma_ahb_start_addr_msb,
			(rdma_info[0].rdma_table_phy_addr >> 32) & 0xf);
		wr(rdma_regadr->rdma_ahb_end_addr,
			(rdma_info[0].rdma_table_phy_addr +
			rdma_info[0].rdma_item_count * 8 - 1) & 0xffffffff);
		wr(rdma_regadr->rdma_ahb_end_addr_msb,
			((rdma_info[0].rdma_table_phy_addr +
			rdma_info[0].rdma_item_count * 8 - 1) >> 32) & 0xf);
		#else
		wr(rdma_regadr->rdma_ahb_start_addr,
			rdma_info[0].rdma_table_phy_addr & 0xffffffff);
		wr(rdma_regadr->rdma_ahb_end_addr,
			(rdma_info[0].rdma_table_phy_addr +
			rdma_info[0].rdma_item_count * 8 - 1) & 0xffffffff);
		#endif

		w_reg_bit(rdma_regadr->addr_inc_reg, 0,
			rdma_regadr->addr_inc_reg_bitpos, 1);
		w_reg_bit(rdma_regadr->rw_flag_reg, 1,   //1:write
			rdma_regadr->rw_flag_reg_bitpos, 1);

		wr(rdma_regadr->trigger_mask_reg,
			rd(rdma_regadr->trigger_mask_reg) | 1);

		pr_rdma(1, "config manual write done\n");
		rdma_info[0].rdma_item_count = 0;
	}
}

static int rdma_find_table_adr_pre_vs(u32 addr)
{
	int i;
	int index = -1;
	struct dpss_rdma_info *rdma_info_pre_vs;

	rdma_info_pre_vs = &rdma_info[RDMA_IRQ_PRE_VSYNC];
	if (rdma_info_pre_vs->rdma_item_count == 0)
		return index;

	for (i = (rdma_info_pre_vs->rdma_item_count - 1) * 2; i >= 0; i -= 2) {
		if (rdma_info_pre_vs->tmp_table[i] == addr) {
			index = i;
			break;
		}
	}

	return index;
}

static int rdma_find_table_adr_vs(u32 addr)
{
	int i;
	int index = -1;
	struct dpss_rdma_info *rdma_info_vs;

	rdma_info_vs = &rdma_info[RDMA_IRQ_VSYNC];

	if (rdma_info_vs->rdma_item_count == 0)
		return index;

	for (i = (rdma_info_vs->rdma_item_count - 1) * 2; i >= 0; i -= 2) {
		if (rdma_info_vs->tmp_table[i] == addr) {
			index = i;
			break;
		}
	}

	return index;
}

// pre_vs
static void dpss_rdma_pre_vs_table_config(unsigned int addr, unsigned int val)
{
	unsigned int i;
	struct dpss_rdma_info *rdma_info_pre_vs;

	rdma_info_pre_vs = &rdma_info[RDMA_IRQ_PRE_VSYNC];
	i = rdma_info_pre_vs->rdma_item_count; // pre_vs

	if ((i + 1) * 8 > rdma_info_pre_vs->rdma_table_size) {
		pr_rdma(1, "dpss rdma in buffer overflow\n");
		i = 0;
		rdma_info_pre_vs->rdma_item_count = 0;
	}

	rdma_info_pre_vs->tmp_table[i * 2] = addr & 0xffffffff;
	rdma_info_pre_vs->tmp_table[i * 2 + 1] = val & 0xffffffff;

	rdma_info_pre_vs->rdma_item_count++;
}

// vsync
static void dpss_rdma_vs_table_config(unsigned int addr, unsigned int val)
{
	unsigned int i;
	struct dpss_rdma_info *rdma_info_vs;

	rdma_info_vs = &rdma_info[RDMA_IRQ_VSYNC];
	i = rdma_info_vs->rdma_item_count; // vs

	if ((i + 1) * 8 > rdma_info_vs->rdma_table_size) {
		pr_rdma(1, "dpss rdma in buffer overflow\n");
		i = 0;
		rdma_info_vs->rdma_item_count = 0;
	}

	rdma_info_vs->tmp_table[i * 2] = addr & 0xffffffff;
	rdma_info_vs->tmp_table[i * 2 + 1] = val & 0xffffffff;

	rdma_info_vs->rdma_item_count++;
}

static int _dpss_rdma_wr_reg_pre_vs(u32 addr, u32 val)
{
	// if (get_frc_devp()->power_on_flag == 0)
	// return 0;

	if (rdma_enable()) {
		dpss_rdma_pre_vs_table_config(addr, val);
		pr_rdma(2, "pre vsync(rdma), addr:0x%04x, val:0x%08x\n",
			addr, val);
	} else {
		wr(addr, val);
		pr_rdma(2, "pre vsync(cpu), addr:0x%04x, val:0x%08x\n",
			addr, val);
	}

	return 0;
}

static int _dpss_rdma_wr_reg_vs(u32 addr, u32 val)
{
	// if (get_frc_devp()->power_on_flag == 0)
	// return 0;

	if (rdma_enable()) {
		dpss_rdma_vs_table_config(addr, val);
		pr_rdma(2, "vsync(rdma), addr:0x%04x, val:0x%08x\n",
			addr, val);
	} else {
		wr(addr, val);
		pr_rdma(2, "vsync(cpu), addr:0x%04x, val:0x%08x\n",
			addr, val);
	}

	return 0;
}

static int _dpss_rdma_wr_bit_reg_pre_vs(u32 addr, u32 val, u32 start, u32 len)
{
	int index = -1;
	u32 read_val, write_val, mask;

	if (rdma_enable()) {
		pr_rdma(2, "pre vsync(rdma), addr:%x, val:%d, start:%d, len:%d\n",
			addr, val, start, len);
		index = rdma_find_table_adr_pre_vs(addr);
		if (index == -1) {
			read_val = rd(addr);
			mask = (((1L << len) - 1) << start);
			write_val  = read_val & ~mask;
			write_val |= (val << start) & mask;
			dpss_rdma_pre_vs_table_config(addr, write_val);
		} else {
			// update reg val
			read_val = rdma_info[RDMA_IRQ_PRE_VSYNC].tmp_table[index + 1];
			mask = (((1L << len) - 1) << start);
			write_val  = read_val & ~mask;
			write_val |= (val << start) & mask;
			rdma_info[RDMA_IRQ_PRE_VSYNC].tmp_table[index + 1] = write_val;
		}
		pr_rdma(2, "pre vsync update bits:addr:0x%04x, reg_val<0x%08x->0x%08x>\n",
				addr, read_val, write_val);
	} else {
		w_reg_bit(addr, val, start, len);
		pr_rdma(2, "pre vsync(cpu), addr:0x%04x, val:0x%08x\n",
			addr, val);
	}
	return 0;
}

static int _dpss_rdma_wr_bit_reg_vs(u32 addr, u32 val, u32 start, u32 len)
{
	int index = -1;
	u32 read_val, write_val, mask;

	if (rdma_enable()) {
		pr_rdma(2, "vsync(rdma), addr:%x, val:%d, start:%d, len:%d\n",
			addr, val, start, len);
		index = rdma_find_table_adr_vs(addr);
		if (index == -1) {
			read_val = rd(addr);
			mask = (((1L << len) - 1) << start);
			write_val  = read_val & ~mask;
			write_val |= (val << start) & mask;
			dpss_rdma_vs_table_config(addr, write_val);
		} else {
			// update reg val
			read_val = rdma_info[RDMA_IRQ_VSYNC].tmp_table[index + 1];
			mask = (((1L << len) - 1) << start);
			write_val  = read_val & ~mask;
			write_val |= (val << start) & mask;
			rdma_info[RDMA_IRQ_VSYNC].tmp_table[index + 1] = write_val;
		}
		pr_rdma(2, "vsync update bits:addr:0x%04x, reg_val<0x%08x->0x%08x>\n",
				addr, read_val, write_val);
	} else {
		w_reg_bit(addr, val, start, len);
		pr_rdma(2, "vsync(cpu), addr:0x%04x, val:0x%08x\n",
			addr, val);
	}
	return 0;
}

void DPSS_RDMA_WR_PRE_VS(u32 addr, u32 val)
{
	if (unlikely(addr < 0x8000 || addr > 0xffff)) {
		DBG_ERR("rdma not support addr:%x\n", addr);
		return;
	}

	_dpss_rdma_wr_reg_pre_vs(addr, val);
}
EXPORT_SYMBOL(DPSS_RDMA_WR_PRE_VS);

void DPSS_RDMA_WR_BIT_PRE_VS(u32 addr, u32 val, u32 start, u32 len)
{
	if (unlikely(addr < 0x8000 || addr > 0xffff)) {
		DBG_ERR("rdma not support addr:%x\n", addr);
		return;
	}
	_dpss_rdma_wr_bit_reg_pre_vs(addr, val, start, len);
}
EXPORT_SYMBOL(DPSS_RDMA_WR_BIT_PRE_VS);

void DPSS_RDMA_WR_VS(u32 addr, u32 val)
{
	if (unlikely(addr < 0x8000 || addr > 0xffff)) {
		DBG_ERR("rdma not support addr:%x\n", addr);
		return;
	}

	_dpss_rdma_wr_reg_vs(addr, val);
}
EXPORT_SYMBOL(DPSS_RDMA_WR_VS);

void DPSS_RDMA_WR_BIT_VS(u32 addr, u32 val, u32 start, u32 len)
{
	if (unlikely(addr < 0x8000 || addr > 0xffff)) {
		DBG_ERR("rdma not support addr:%x\n", addr);
		return;
	}
	_dpss_rdma_wr_bit_reg_vs(addr, val, start, len);
}
EXPORT_SYMBOL(DPSS_RDMA_WR_BIT_VS);

void irq_rdma(void)
{
	unsigned int i, rdma_status;
	struct rdma_regadr_s *info_regadr;
	//struct frc_rdma_s *frc_rdma = get_frc_rdma();

	rdma_status = rd(irq_status.reg);

	pr_rdma(1, "rdma status[0x%x]\n", rdma_status);

	for (i = 0; i < RDMA_CHANNEL; i++) {
		info_regadr = (struct rdma_regadr_s *)rdma_info[i].rdma_regadr;
		if (rdma_status & (0x1 << (i + irq_status.start))) {
			wr(rdma_ctrl,
				0x1 << info_regadr->clear_irq_bitpos);
			pr_rdma(1, "channel:%d rdma irq done\n", i);
			break;
		}
	}
}

static void dpss_frc_rdma_irq_src_clear(u8 ch)
{
	struct dpss_rdma_info *rdma_info_p;
	struct rdma_regadr_s *rdma_regadr;

	if (ch != RDMA_IRQ_PRE_VSYNC && ch != RDMA_IRQ_VSYNC)
		return;

	rdma_info_p = &rdma_info[ch];
	rdma_regadr = rdma_info_p->rdma_regadr;
	if (!rdma_regadr)
		return;
	wr(rdma_regadr->trigger_mask_reg, 0);
	rdma_info_p->rdma_reg = 0;
	pr_rdma(2, "frc rdma ch:%d clear done\n", ch);
}

void dpss_frc_rdma_unreg(void)
{
	dpss_frc_rdma_irq_src_clear(RDMA_IRQ_PRE_VSYNC);
	dpss_frc_rdma_irq_src_clear(RDMA_IRQ_VSYNC);
	rdma_info[RDMA_IRQ_PRE_VSYNC].rdma_item_count = 0;
	rdma_info[RDMA_IRQ_VSYNC].rdma_item_count = 0;
}

void dpss_frc_rdma_reg(void)
{
	rdma_info[RDMA_IRQ_PRE_VSYNC].rdma_reg = 1;
	rdma_info[RDMA_IRQ_PRE_VSYNC].clr_mask_reg = 0;
	rdma_info[RDMA_IRQ_VSYNC].rdma_reg = 1;
	rdma_info[RDMA_IRQ_VSYNC].clr_mask_reg = 0;
}

void pre_vsync_signal_to_dpss_rdma(void)
{
	u32 read_cnt;
	struct dpss_rdma_info *rdma_info_p;
	struct rdma_regadr_s *rdma_regadr;

	if (!rdma_enable())
		return;
	rdma_info_p = &rdma_info[RDMA_IRQ_PRE_VSYNC];
	rdma_regadr = rdma_info_p->rdma_regadr;
	if (!rdma_regadr)
		return;
	if (rdma_info_p->rdma_reg == 0) {
		if (rdma_info_p->clr_mask_reg == 0) {
			wr(rdma_regadr->trigger_mask_reg, 0);
			rdma_info_p->clr_mask_reg = 1;
		}
		return;
	}
	read_cnt = rd(FRC_REG_TOP_RESERVE11);
	if (rdma_info_p->rdma_reg == 1) {
		read_cnt = 0;
		rdma_info_p->index_flag = 0;
		rdma_info_p->rdma_reg = 2;
	}
	if (frc_pv_rdma_err_cnt == DPSS_FRC_ERR_CNT && rdma_info_p->rdma_reg == 2)
		rdma_info_p->index_flag = read_cnt;
	if (read_cnt != rdma_info_p->index_flag) {
		frc_pv_rdma_err_cnt++;
		pr_rdma(1, "pre vsync rdma abnormal:%d,%d,%d\n",
			read_cnt, rdma_info_p->index_flag, frc_pv_rdma_err_cnt);
		return;
	}
	frc_pv_rdma_err_cnt = 0;
	rdma_info_p->index_flag++;
	DPSS_RDMA_WR_BIT_PRE_VS(FRC_REG_TOP_RESERVE11, rdma_info_p->index_flag, 0, 32);

	dpss_rdma_auto_wr_tri(RDMA_IRQ_PRE_VSYNC);
}

void post_vsync_signal_to_dpss_rdma(void)
{
//#ifdef USE_FRC_VS_RDMA
	u32 read_cnt;
	struct dpss_rdma_info *rdma_info_p;
	struct rdma_regadr_s *rdma_regadr;

	vsync_signal_to_game_frc(); // for game frc

	if (!rdma_enable())
		return;
	rdma_info_p = &rdma_info[RDMA_IRQ_VSYNC];
	rdma_regadr = rdma_info_p->rdma_regadr;
	if (!rdma_regadr)
		return;
	if (rdma_info_p->rdma_reg == 0) {
		if (rdma_info_p->clr_mask_reg == 0) {
			wr(rdma_regadr->trigger_mask_reg, 0);
			rdma_info_p->clr_mask_reg = 1;
		}
		return;
	}
	read_cnt = rd(FRC_REG_TOP_RESERVE12);
	if (rdma_info_p->rdma_reg == 1) {
		read_cnt = 0;
		rdma_info_p->index_flag = 0;
		rdma_info_p->rdma_reg = 2;
	}
	if (frc_vs_rdma_err_cnt == DPSS_FRC_ERR_CNT && rdma_info_p->rdma_reg == 2)
		rdma_info_p->index_flag = read_cnt;
	if (read_cnt != rdma_info_p->index_flag) {
		frc_vs_rdma_err_cnt++;
		pr_rdma(1, "vsync rdma abnormal:%d,%d,%d\n",
			read_cnt, rdma_info_p->index_flag, frc_vs_rdma_err_cnt);
		return;
	}
	frc_vs_rdma_err_cnt = 0;
	rdma_info_p->index_flag++;
	DPSS_RDMA_WR_BIT_VS(FRC_REG_TOP_RESERVE12, rdma_info_p->index_flag, 0, 32);
	dpss_rdma_auto_wr_tri(RDMA_IRQ_VSYNC);
//#endif
}

void dpss_rdma_auto_wr_tri(u32 handle)
{
	unsigned int handle_irq = 0x2;
	unsigned int handle_irq_rd = 0;
	unsigned int table_size;
	struct rdma_regadr_s *rdma_regadr;
	struct dpss_rdma_info *rdma_info_p;

	rdma_info_p = &rdma_info[handle];
	rdma_regadr = rdma_info_p->rdma_regadr;

	if (!rdma_enable())
		return;

	if (handle == RDMA_IRQ_PRE_VSYNC)
		handle_irq = rdma_handle_irq_pre_vs;
	else if (handle == RDMA_IRQ_VSYNC)
		handle_irq = rdma_handle_irq_vs;

	dbg_f1("before: 0xf120 =:%#x\n", rd(DPSS_RDMA_ACCESS_AUTO));
	table_size = rdma_info_p->rdma_table_size;
	if (rdma_info_p->rdma_item_count > 0) {
		//auto RDMA
		// w_reg_bit(rdma_regadr->trigger_mask_reg, 0,
		// rdma_regadr->trigger_mask_reg_bitpos, 1);
		wr(rdma_regadr->trigger_mask_reg, 0);
		memcpy(rdma_info_p->rdma_table_addr, rdma_info_p->tmp_table, table_size);

		#ifdef CONFIG_ARM64
		wr(rdma_regadr->rdma_ahb_start_addr,
			rdma_info_p->rdma_table_phy_addr & 0xffffffff);
		wr(rdma_regadr->rdma_ahb_start_addr_msb,
			(rdma_info_p->rdma_table_phy_addr >> 32) & 0xf);
		wr(rdma_regadr->rdma_ahb_end_addr,
			(rdma_info_p->rdma_table_phy_addr +
			rdma_info_p->rdma_item_count * 8 - 1) & 0xffffffff);
		wr(rdma_regadr->rdma_ahb_end_addr_msb,
			((rdma_info_p->rdma_table_phy_addr +
			rdma_info_p->rdma_item_count * 8 - 1) >> 32) & 0xf);
		#else
		wr(rdma_regadr->rdma_ahb_start_addr,
			rdma_info_p->rdma_table_phy_addr & 0xffffffff);
		wr(rdma_regadr->rdma_ahb_end_addr,
			(rdma_info_p->rdma_table_phy_addr +
			rdma_info_p->rdma_item_count * 8 - 1) & 0xffffffff);
		#endif

		w_reg_bit(rdma_regadr->addr_inc_reg, 0,
			rdma_regadr->addr_inc_reg_bitpos, 1);
		w_reg_bit(rdma_regadr->rw_flag_reg, 1,  // 1:write
			rdma_regadr->rw_flag_reg_bitpos, 1);

		// w_reg_bit(rdma_regadr->trigger_mask_reg, rdma_handle_irq,
		// rdma_regadr->trigger_mask_reg_bitpos, 31);
		wr(rdma_regadr->trigger_mask_reg, handle_irq);
		handle_irq_rd = rd(rdma_regadr->trigger_mask_reg);

		pr_rdma(2, "config auto done rdma_handle_irq:%d,%d,count:%d\n",
			handle_irq_rd, handle_irq, rdma_info_p->rdma_item_count);
		rdma_info_p->rdma_item_count = 0;
	}
	dbg_f1("after: 0xf120 =:%#x\n", rd(DPSS_RDMA_ACCESS_AUTO));

}

static void rdma_alloc_buf(struct dpss_dev_s *de_devp)
{
	int i;
	phys_addr_t dma_handle;
	// struct platform_device *pdev;
	// struct frc_rdma_info *rdma_info;

	if (!de_devp)
		return;

	buf_alloced = 1;

	// pdev = (struct platform_device *)de_devp->pdev;
	// pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	// pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	for (i = 0; i < RDMA_CHANNEL; i++) {
		rdma_info[i].rdma_table_addr =
			dma_alloc_coherent(&de_devp->pdev->dev,
			rdma_info[i].rdma_table_size, &dma_handle, 0);

		if (!rdma_info[i].rdma_table_addr)
			goto buf_err;

		rdma_info[i].rdma_table_phy_addr = dma_handle;
		if (sizeof(rdma_info[i].rdma_table_phy_addr) > 4)
			rdma_info[i].is_64bit_addr = 1; // 64bit addr
		else
			rdma_info[i].is_64bit_addr = 0;

		rdma_info[i].tmp_table = vmalloc(rdma_info[i].rdma_table_size);

		if (!rdma_info[i].tmp_table)
			goto buf_err;
	}

	return;

buf_err:
	while (i-- >= 0) {
		if (rdma_info[i].tmp_table) {
			vfree(rdma_info[i].tmp_table);
			rdma_info[i].tmp_table = NULL;
		}

		if (rdma_info[i].rdma_table_addr) {
			dma_free_coherent(&de_devp->pdev->dev,
				rdma_info[i].rdma_table_size, rdma_info[i].rdma_table_addr,
				(dma_addr_t)rdma_info[i].rdma_table_phy_addr);
			rdma_info[i].rdma_table_addr = NULL;
		}
	}
	buf_alloced = 0;
	DBG_ERR("dpss frc rdma buf alloc fail\n");
}

static void rdma_release_buf(void)
{
	int i;
	struct dpss_dev_s *de_devp = dpss_get_devp();

	if (!de_devp)
		return;

	if (!buf_alloced)
		return;

	for (i = 0; i < RDMA_CHANNEL; i++) {
		if (rdma_info[i].tmp_table)
			vfree(rdma_info[i].tmp_table);
		dma_free_coherent(&de_devp->pdev->dev,
			rdma_info[i].rdma_table_size, rdma_info[i].rdma_table_addr,
			(dma_addr_t)rdma_info[i].rdma_table_phy_addr);
	}
	buf_alloced = 0;
}

static void dpss_rdma_info_init(void)
{
	int i;

	for (i = 0; i < RDMA_CHANNEL; i++) {
		rdma_info[i].rdma_table_addr = NULL;
		rdma_info[i].rdma_table_size = DPSS_RDMA_SIZE / 16;
		//if (i > 2) // dbg, rdma read
			//rdma_info[i].rdma_table_size = DPSS_RDMA_SIZE / 16;
		rdma_info[i].is_64bit_addr = 0;
		rdma_info[i].rdma_reg = 0;
		rdma_info[i].clr_mask_reg = 0;
		rdma_info[i].rdma_item_count = 0;
		rdma_info[i].rdma_write_count = 0;
		rdma_info[i].tmp_table = NULL;
		rdma_info[i].rdma_regadr = NULL;
	}
}

int dpss_rdma_init(void)
{
	u32 i, data32;
	struct dpss_dev_s *de_devp = dpss_get_devp();

	// init frc rdma
	dpss_rdma_info_init();
	// alloc buf
	rdma_alloc_buf(de_devp);

	if (buf_alloced) {
		dpss_rdma_en = 1;			  // drv rdma en
		//frc_rdma->rdma_alg_en = 0;		  // alg rdma default off
		// fw_data->frc_top_type.dpss_rdma_en = 0;
	} else {
		DBG_ERR("alloc dpss rdma buffer failed\n");
		return 0;
	}

	for (i = 0; i < RDMA_CHANNEL; i++)
		rdma_info[i].rdma_regadr = &rdma_regadr_t6w[i];
	irq_status.reg = DPSS_RDMA_STATUS1;
	irq_status.start = 4;
	irq_status.len = 16;
	rdma_ctrl = DPSS_RDMA_CTRL;

	// rdma read init
	// frc_rdma_rd_table_init(devp);

	data32  = 0;
	data32 |= 0 << 7; /* write ddr urgent */
	data32 |= 1 << 6; /* read ddr urgent */
	// data32 |= 0 << 4;
	// data32 |= 0 << 2;
	// data32 |= 0 << 1;
	// data32 |= 0 << 0;
	if (dpss_rdma_reset_enable)
		data32 |= 1 << 1; /* reset */
	// init clk
	// w_reg_bit(FRC_RDMA_SYNC_CTRL, 1, 2, 1);
	// w_reg_bit(FRC_RDMA_SYNC_CTRL, 1, 6, 1);
	// rdma config
	wr(rdma_ctrl, data32);
	w_reg_bit(rdma_ctrl, 0, 1, 1);
	pr_rdma(2, "dpss rdma init done\n");
	return 1;
}

void dpss_rdma_exit(void)
{
	// struct frc_rdma_s *frc_rdma = get_frc_rdma();

	// close auto
	// w_reg_bit(FRC_RDMA_SYNC_CTRL, 0, 2, 1);
	// w_reg_bit(FRC_RDMA_SYNC_CTRL, 0, 6, 1);
	//wr(FRC_RDMA_AUTO_SRC1_SEL_T3X, 0);
	//wr(FRC_RDMA_AUTO_SRC2_SEL_T3X, 0);
	wr(rdma_ctrl, 0);
	rdma_release_buf();
	dpss_rdma_en = 0;

	pr_rdma(2, "dpss rdma exit\n");
}
