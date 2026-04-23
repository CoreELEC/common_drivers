/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPSS_RDMA_FRC_H__
#define __DPSS_RDMA_FRC_H__

#define RDMA_CHANNEL 3

#ifndef PAGE_SIZE
# define PAGE_SIZE 4096
#endif

/*RDMA total memory size is 1M*/
#define DPSS_RDMA_SIZE (64 * (PAGE_SIZE))
#define DPSS_FRC_ERR_CNT 5

struct rdma_irq_reg_s {
	u32 reg;
	u32 start;
	u32 len;
};

struct rdma_regadr_s {
	u32 rdma_ahb_start_addr;
	u32 rdma_ahb_start_addr_msb;
	u32 rdma_ahb_end_addr;
	u32 rdma_ahb_end_addr_msb;
	u32 trigger_mask_reg;
	u32 trigger_mask_reg_bitpos;
	u32 addr_inc_reg;
	u32 addr_inc_reg_bitpos;
	u32 rw_flag_reg;
	u32 rw_flag_reg_bitpos;
	u32 clear_irq_bitpos;
	u32 irq_status_bitpos;
};

struct dpss_rdma_info {
	ulong rdma_table_phy_addr;
	u32 *rdma_table_addr;
	int rdma_table_size;
	u8 is_64bit_addr;
	u8 rdma_reg;
	u8 clr_mask_reg;
	u32 index_flag;
	int rdma_item_count;
	int rdma_write_count;
	u32 *tmp_table;
	struct rdma_regadr_s *rdma_regadr;
};

//dbg
void dpss_rdma_force_chg_st(u32 en);
void dpss_rdma_dbg_level(u32 level);
int show_dpss_rdma_dbg_level(void);
int show_dpss_rdma_buf_alloc_st(void);
int show_dpss_rdma_reset_status(void);
void set_dpss_rdma_reset_enable(int enable);

void irq_rdma(void);
int  dpss_rdma_init(void);
void dpss_rdma_exit(void);
void dpss_rdma_man_wr_test(unsigned int addr, unsigned int val);
void dpss_rdma_man_wr_clear(void);
void dpss_rdma_man_wr_tri(void);

int rdma_enable(void);
void pre_vsync_signal_to_dpss_rdma(void);
// void dpss_rdma_auto_wr_reg(unsigned int addr, unsigned int val);
void dpss_rdma_auto_wr_tri(u32 handle);
void DPSS_RDMA_WR_PRE_VS(u32 addr, u32 val);
void DPSS_RDMA_WR_BIT_PRE_VS(u32 addr, u32 val, u32 start, u32 len);
void DPSS_RDMA_WR_VS(u32 addr, u32 val);
void DPSS_RDMA_WR_BIT_VS(u32 addr, u32 val, u32 start, u32 len);

#endif //__DPSS_RDMA_FRC_H__
