/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef RDMA_MGR_H_
#define RDMA_MGR_H_

struct rdma_op_s {
	void (*irq_cb)(void *arg);
	void *arg;
};

enum {
	VIDEO_PARTITION_TABLE    = 0,
	AMVECM_PARTITION_TABLE_0 = 1,
	AMVECM_PARTITION_TABLE_1 = 2,
	AMVECM_PARTITION_TABLE_2 = 3,
	AMVECM_PARTITION_TABLE_3 = 4,
	AMLDV_PARTITION_TABLE_0  = 5,
	AMLDV_PARTITION_TABLE_1  = 6,
	AMLDV_PARTITION_TABLE_2  = 7,
	AMLDV_PARTITION_TABLE_3  = 8,
	AMLDV_PARTITION_TABLE_4  = 9,
	AMLDV_PARTITION_TABLE_5  = 10,
	AMLDV_PARTITION_TABLE_6  = 11,
	PARTITION_TABLE_NUM      = 12,
};

struct rdma_partition_ins_s {
	int table_index;
	int max_reg_cnt;
	bool flag;
	u32 *rdma_item;
	bool reg_range_check;
	u32 check_start_addr;
	u32 check_end_addr;
	u32 rdma_item_count;
	u32 vpp_index;
};

#define RDMA_TRIGGER_VSYNC_INPUT 0x1
#define RDMA_TRIGGER_VDIN0_INPUT BIT(1)
#define RDMA_TRIGGER_LINE_INPUT  BIT(8)
#define RDMA_TRIGGER_VPP1_VSYNC_INPUT BIT(9)
#define RDMA_TRIGGER_VPP2_VSYNC_INPUT BIT(19)
#define RDMA_TRIGGER_PRE_VSYNC_INPUT  BIT(24)
#define RDMA_TRIGGER_PRE_VSYNC_INPUT_T3X  BIT(10)

/* new for t6w */
#define RDMA_TRIGGER_DOLDY_CORE1  BIT(4)
#define RDMA_TRIGGER_DOLDY_CORE2  BIT(6)
#define RDMA_TRIGGER_DPSS_DPE_FRM_0  BIT(22)
#define RDMA_TRIGGER_DPSS_DPE_FRM_1  BIT(23)
#define RDMA_TRIGGER_DPSS_DPE_FRM_2  BIT(24)
#define RDMA_TRIGGER_DPSS_DPE_FRM_3  BIT(28)
#define RDMA_TRIGGER_DPSS_DPE_FRM_4  BIT(29)
#define RDMA_TRIGGER_DPSS_DPE_FRM_5  BIT(30)
#define RDMA_TRIGGER_DPSS_PVSYNC     BIT(31)

/* for t6w need expand to 64bit */
#define RDMA_TRIGGER_MANUAL      ((u64)1 << 60)
#define RDMA_TRIGGER_OMIT_LOCK   ((u64)1 << 61)
/* rdma write: bit[62] = 0
 * rdma read:  bit[62] = 1
 */
#define RDMA_READ_MASK           ((u64)1 << 62)
#define RDMA_AUTO_START_MASK     ((u64)1 << 63)

#define RDMA_TRIGGER_DEBUG1      0x81000000
#define RDMA_TRIGGER_DEBUG2      0x82000000


enum rdma_ver_e {
	RDMA_VER_1,
	RDMA_VER_2,
	RDMA_VER_3,
	RDMA_VER_4,
	RDMA_VER_5,
	RDMA_VER_6,
};

enum cpu_ver_e {
	CPU_NORMAL,
	CPU_G12B_REVA,
	CPU_G12B_REVB,
	CPU_TL1,
	CPU_SC2,
	CPU_T7,
	CPU_T6W,
};

enum rdma_vpp_e {
	RDMA_VPP0,
	RDMA_VPP1,
	RDMA_VPP2,
	RDMA_PRE_VSYNC,
	RDMA_LOWLATENCY,
	RDMA_VPP_MAX,
};

struct rdma_device_data_s {
	enum cpu_ver_e cpu_type;
	enum rdma_ver_e rdma_ver;
	u32 trigger_mask_len;
	u32 channel_num;
};

struct reg_handle {
	u32 offset;   /* read-only, offset in register table */
	u32 reg_addr; /* VCBus register address offset */
	struct list_head list;
};

u32 is_meson_g12b_revb(void);
/*
 *	rdma_read_reg(), rdma_write_reg(), rdma_clear() can only be called
 *	after rdma_register() is called and
 *	before rdma_unregister() is called
 */
int rdma_register(struct rdma_op_s *rdma_op, void *op_arg, int table_size);

/*
 *	if keep_buf is 0, rdma_unregister can only be called in its irq_cb.
 *	in normal case, keep_buf is 1, so rdma_unregister can be called anywhere
 */
void rdma_unregister(int i);

int rdma_config(int handle, u64 trigger_type);

u32 rdma_read_reg(int handle, u32 adr);

int rdma_write_reg(int handle, u32 adr, u32 val);

int rdma_write_reg_bits(int handle, u32 adr, u32 val, u32 start, u32 len);

int rdma_part_table_register(struct rdma_partition_ins_s *rdma_part_ins);

void video_reg_write_check_table_init(void);

void set_part_flag_status(int vpp_index, int tbl_index, bool set_flag);

u32 VSYNC_RD_TABLE_REG(int tbl_idx, u32 adr);

int VSYNC_WR_TABLE_REG(int tbl_idx, u32 adr, u32 val);

int VSYNC_WR_TABLE_REG_BITS(int tbl_idx, u32 adr, u32 val, u32 start, u32 len);

u32 PRE_VSYNC_RD_TABLE_REG(int tbl_idx, u32 adr);

int PRE_VSYNC_WR_TABLE_REG(int tbl_idx, u32 adr, u32 val);

inline int VSYNC_WR_TABLE_REG_SIMPLE(int tbl_idx, u32 adr, u32 val);

int PRE_VSYNC_WR_TABLE_REG_BITS(int tbl_idx, u32 adr, u32 val, u32 start, u32 len);

struct rdma_partition_ins_s *get_part_table_ins(int vpp_index, int tbl_index);

int rdma_update_reg_buf(int handle, u32 *rdma_item, u32 count);

u32 rdma_read_reg_write_table(int handle, u32 adr, bool *is_found);

int rdma_clear(int handle);

s32 rdma_add_read_reg(int handle, struct reg_handle *reg_hnd);

s32 rdma_remove_read_reg(int handle, struct reg_handle *reg_hnd, u32 count);

u32 *rdma_get_read_back_addr(int handle, struct reg_handle *reg_hnd);

struct reg_handle *rdma_query_read_handle(int handle, u32 index);

int rdma_buffer_unlock(int handle);
int rdma_buffer_lock(int handle);

int rdma_start_addr(int handle);

int rdma_start_addr_msb(int handle);

int rdma_end_addr(int handle);

int rdma_end_addr_msb(int handle);

//rdma mbuf api
int rdma_mbuf_register(struct rdma_op_s *rdma_op, void *op_arg,
	int mbuf_num, int buf_step);
void rdma_mbuf_unregister(int handle);

int rdma_mbuf_write_reg(int handle, int mbuf_index, u32 adr, u32 val);
int rdma_mbuf_write_reg_bits(int handle, int mbuf_index, u32 adr, u32 val, u32 start, u32 len);
int rdma_mbuf_config(int handle, int mbuf_index, u64 trigger_type);
int mbuf_get_current_index(int handle);
void rdma_mbuf_idx_clr(int handle);
void rdma_mbuf_reconfig_length(int handle);
inline int VSYNC_WR_TABLE_REG_SIMPLE(int tbl_idx, u32 adr, u32 val);
#endif
