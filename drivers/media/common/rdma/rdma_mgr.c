// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "rdma: " fmt

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/dma-mapping.h>

#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/ctype.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/clk.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/reset.h>
#include <linux/delay.h>

#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#include <linux/amlogic/media/utils/am_com.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/video_sink/video.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "rdma.h"

#define DRIVER_NAME "amlogic-rdma"
#define MODULE_NAME "amlogic-rdma"
#define DEVICE_NAME "rdma"
#define CLASS_NAME  "rdma-class"

#define pr_dbg(fmt, args...)   pr_info("RDMA: " fmt, ## args)
#define pr_error(fmt, args...) pr_err("RDMA: " fmt, ## args)

#define rdma_io_read(addr)      readl(addr)
#define rdma_io_write(addr, val) writel((val), addr)

/* #define SKIP_OSD_CHANNEL */
#define RDMA_NUM        16
#define RDMA_MBUF_NUM   16
#define MAX_APPEND_SIZE  256
int has_multi_vpp;
int read_rdma_en;
int read_rdma_trigger = RDMA_TRIGGER_VSYNC_INPUT;
int rdma_mgr_irq_request;
int rdma_reset_trigger_flag[RDMA_NUM];

struct reset_control *rdma_rst;
static u8 rdma_done_cpuid;
static int debug_flag;
/* burst size 0=16; 1=24; 2=32; 3=48.*/
static int ctrl_ahb_rd_burst_size = 3;
static int ctrl_ahb_wr_burst_size = 3;
static int rdma_watchdog = 20;
static int reset_count;
static int rdma_watchdog_count[RDMA_NUM];
static int rdma_force_reset = -1;

#define RDMA_TABLE_SIZE (8 * (PAGE_SIZE))
#define MAX_TRACE_NUM  16
#define RDMA_MGR_CLASS_NAME  "rdma_mgr"
static int rdma_trace_num;
static int rdma_trace_enable;
static int rdma_trace_channel = -1;
static u32 rdma_trace_reg[MAX_TRACE_NUM];
static int rdma_table_size = RDMA_TABLE_SIZE;

static int g_vsync_rdma_item_count;
static int g_vsync_rdma_item_count_max;
static int enable[RDMA_NUM];
int rdma_configured[RDMA_NUM];
ulong rdma_config_us[RDMA_NUM];
int enc_num_configed[RDMA_NUM] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static struct video_module_reg_s video_reg;

/* bit0~bit3 already used */
#define DEBUG_RDMA_DONE_SKIP      0x10
#define DEBUG_RDMA_REG_CONFLICT   0x20
#define DEBUG_RDMA_WATCHDOG       0x40
#define DEBUG_RDMA_CONFIG         0x80
#define DEBUG_RDMA_DONE           0x100
#define DEBUG_RDMA_VIDEO_REG_CONFLICT  0x200
#define DEBUG_RDMA_VIDEO_REG_CONFLICT_STACK  0x400
#define RDMA_PARTITION_TABLE_CNT     2048
#define RDMA_PARTITION_TABLE_SIZE (RDMA_PARTITION_TABLE_CNT * 8)

#define VIU_OSD3_TCOLOR_AG1       0x3d85
#define RDMA_MBUF_DUMMY_REG       VIU_OSD3_TCOLOR_AG1

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
	u32 rdma_buf_idx_clr;
	u32 rdma_buf_idx_clr_bitpos;
};

struct rdma_mbuf_regadr_s {
	u32 rdma_buf_ctrl;
	u32 rdma_buf_cfg_en;
	u32 rdma_buf_length_0;
	u32 rdma_buf_length_1;
	u32 rdma_buf_length_2;
	u32 rdma_buf_length_3;
	u32 rdma_buf_ro;
};

struct rdma_instance_s {
	int not_process;
	struct rdma_regadr_s *rdma_regadr;
	struct rdma_mbuf_regadr_s *rdma_mbuf_regadr;
	struct rdma_op_s *op;
	void *op_arg;
	int rdma_table_size;
	u32 *reg_buf;
	u32 *reg_mbuf[RDMA_MBUF_NUM];
	dma_addr_t dma_handle;
	u32 *rdma_table_addr;
	u32 *rdma_table_mirror;
	u32 *rdma_table_mbuf_mirror[RDMA_MBUF_NUM];
	ulong rdma_table_phy_addr;
	int rdma_item_count;
	int rdma_write_count;
	unsigned char keep_buf;
	unsigned char used;
	int prev_trigger_type;
	int prev_read_count;
	struct reg_handle reg_handle;
	int lock_flag;
	int irq_count;
	int rdma_config_count;
	int rdma_empty_config_count;
	int mbuf_num;
	u32 mbuf_step;
	u64 trigger_type;
	int rdma_mbuf_item_count[RDMA_MBUF_NUM];
	int rdma_mbuf_write_count[RDMA_MBUF_NUM];
	u32 mbuf_count[RDMA_MBUF_NUM];
	u32 mbuf_length[RDMA_MBUF_NUM];
	u32 sft_bit[RDMA_MBUF_NUM];
	u32 append_size[RDMA_MBUF_NUM];
};

#define MAX_CONFLICT 32
struct rdma_conflict_regs_s {
	u32 adr[MAX_CONFLICT];
	u32 val[MAX_CONFLICT];
};

struct rdma_device_info {
	const char *device_name;
	struct platform_device *rdma_dev;
	struct rdma_instance_s rdma_ins[RDMA_NUM];
	struct rdma_conflict_regs_s rdma_reg;
	struct rdma_partition_ins_s rdma_part_ins[RDMA_VPP_MAX][PARTITION_TABLE_NUM];
};

static struct rdma_device_data_s rdma_meson_dev;
static struct rdma_irq_reg_s irq_status;
static int vdisp_async_hold_ctrl_val;
static int vpuarb2_async_hold_ctrl_val;
static int reset4_register_val;
static int rdma_ctrl_val;
static int rdma_ctrl;
static int rdma_access_man;
int rdma_status_reg;
static int support_64bit_addr;
static int support_mbuf;
static u32 *append_buf_info;
static DEFINE_SPINLOCK(rdma_lock);

static struct rdma_device_info rdma_info;
#ifdef CONFIG_AMLOGIC_VPU
static struct vpu_dev_s *rdma_vpu_dev;
#endif
static struct rdma_regadr_s rdma_regadr[] = {
	{RDMA_AHB_START_ADDR_MAN,
		0,
		RDMA_AHB_END_ADDR_MAN,
		0,
		0, 0,
		RDMA_ACCESS_MAN, 1,
		RDMA_ACCESS_MAN, 2,
		24, 24
	},
	{RDMA_AHB_START_ADDR_1,
		0,
		RDMA_AHB_END_ADDR_1,
		0,
		RDMA_ACCESS_AUTO,  8,
		RDMA_ACCESS_AUTO,  1,
		RDMA_ACCESS_AUTO,  5,
		25, 25
	},
	{RDMA_AHB_START_ADDR_2,
		0,
		RDMA_AHB_END_ADDR_2,
		0,
		RDMA_ACCESS_AUTO,  16,
		RDMA_ACCESS_AUTO,  2,
		RDMA_ACCESS_AUTO,  6,
		26, 26
	},
	{RDMA_AHB_START_ADDR_3,
		0,
		RDMA_AHB_END_ADDR_3,
		0,
		RDMA_ACCESS_AUTO,  24,
		RDMA_ACCESS_AUTO,  3,
		RDMA_ACCESS_AUTO,  7,
		27, 27
	},
	{RDMA_AHB_START_ADDR_4,
		0,
		RDMA_AHB_END_ADDR_4,
		0,
		RDMA_ACCESS_AUTO3, 0,
		RDMA_ACCESS_AUTO2, 0,
		RDMA_ACCESS_AUTO2, 4,
		28, 28
	},
	{RDMA_AHB_START_ADDR_5,
		0,
		RDMA_AHB_END_ADDR_5,
		0,
		RDMA_ACCESS_AUTO3, 8,
		RDMA_ACCESS_AUTO2, 1,
		RDMA_ACCESS_AUTO2, 5,
		29, 29
	},
	{RDMA_AHB_START_ADDR_6,
		0,
		RDMA_AHB_END_ADDR_6,
		0,
		RDMA_ACCESS_AUTO3, 16,
		RDMA_ACCESS_AUTO2, 2,
		RDMA_ACCESS_AUTO2, 6,
		30, 30
	},
	{RDMA_AHB_START_ADDR_7,
		0,
		RDMA_AHB_END_ADDR_7,
		0,
		RDMA_ACCESS_AUTO3, 24,
		RDMA_ACCESS_AUTO2, 3,
		RDMA_ACCESS_AUTO2, 7,
		31, 31
	}
};

static struct rdma_regadr_s rdma_regadr_tl1[] = {
	{RDMA_AHB_START_ADDR_MAN,
		0,
		RDMA_AHB_END_ADDR_MAN,
		0,
		0, 0,
		RDMA_ACCESS_MAN, 1,
		RDMA_ACCESS_MAN, 2,
		24, 24
	},
	{RDMA_AHB_START_ADDR_1,
		0,
		RDMA_AHB_END_ADDR_1,
		0,
		RDMA_AUTO_SRC1_SEL,  0,
		RDMA_ACCESS_AUTO,  1,
		RDMA_ACCESS_AUTO,  5,
		25, 25
	},
	{RDMA_AHB_START_ADDR_2,
		0,
		RDMA_AHB_END_ADDR_2,
		0,
		RDMA_AUTO_SRC2_SEL,  0,
		RDMA_ACCESS_AUTO,  2,
		RDMA_ACCESS_AUTO,  6,
		26, 26
	},
	{RDMA_AHB_START_ADDR_3,
		0,
		RDMA_AHB_END_ADDR_3,
		0,
		RDMA_AUTO_SRC3_SEL,  0,
		RDMA_ACCESS_AUTO,  3,
		RDMA_ACCESS_AUTO,  7,
		27, 27
	},
	{RDMA_AHB_START_ADDR_4,
		0,
		RDMA_AHB_END_ADDR_4,
		0,
		RDMA_AUTO_SRC4_SEL, 0,
		RDMA_ACCESS_AUTO2, 0,
		RDMA_ACCESS_AUTO2, 4,
		28, 28
	},
	{RDMA_AHB_START_ADDR_5,
		0,
		RDMA_AHB_END_ADDR_5,
		0,
		RDMA_AUTO_SRC5_SEL, 0,
		RDMA_ACCESS_AUTO2, 1,
		RDMA_ACCESS_AUTO2, 5,
		29, 29
	},
	{RDMA_AHB_START_ADDR_6,
		0,
		RDMA_AHB_END_ADDR_6,
		0,
		RDMA_AUTO_SRC6_SEL, 0,
		RDMA_ACCESS_AUTO2, 2,
		RDMA_ACCESS_AUTO2, 6,
		30, 30
	},
	{RDMA_AHB_START_ADDR_7,
		0,
		RDMA_AHB_END_ADDR_7,
		0,
		RDMA_AUTO_SRC7_SEL, 0,
		RDMA_ACCESS_AUTO2, 3,
		RDMA_ACCESS_AUTO2, 7,
		31, 31
	}
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct rdma_regadr_s rdma_regadr_t7[] = {
	{RDMA_AHB_START_ADDR_MAN,
		RDMA_AHB_START_ADDR_MAN_MSB,
		RDMA_AHB_END_ADDR_MAN,
		RDMA_AHB_END_ADDR_MAN_MSB,
		0, 0,
		RDMA_ACCESS_MAN, 1,
		RDMA_ACCESS_MAN, 2,
		24, 24
	},
	{RDMA_AHB_START_ADDR_1,
		RDMA_AHB_START_ADDR_1_MSB,
		RDMA_AHB_END_ADDR_1,
		RDMA_AHB_END_ADDR_1_MSB,
		RDMA_AUTO_SRC1_SEL,  0,
		RDMA_ACCESS_AUTO,  1,
		RDMA_ACCESS_AUTO,  5,
		25, 25
	},
	{RDMA_AHB_START_ADDR_2,
		RDMA_AHB_START_ADDR_2_MSB,
		RDMA_AHB_END_ADDR_2,
		RDMA_AHB_END_ADDR_2_MSB,
		RDMA_AUTO_SRC2_SEL,  0,
		RDMA_ACCESS_AUTO,  2,
		RDMA_ACCESS_AUTO,  6,
		26, 26
	},
	{RDMA_AHB_START_ADDR_3,
		RDMA_AHB_START_ADDR_3_MSB,
		RDMA_AHB_END_ADDR_3,
		RDMA_AHB_END_ADDR_3_MSB,
		RDMA_AUTO_SRC3_SEL,  0,
		RDMA_ACCESS_AUTO,  3,
		RDMA_ACCESS_AUTO,  7,
		27, 27
	},
	{RDMA_AHB_START_ADDR_4,
		RDMA_AHB_START_ADDR_4_MSB,
		RDMA_AHB_END_ADDR_4,
		RDMA_AHB_END_ADDR_4_MSB,
		RDMA_AUTO_SRC4_SEL, 0,
		RDMA_ACCESS_AUTO2, 0,
		RDMA_ACCESS_AUTO2, 4,
		28, 28
	},
	{RDMA_AHB_START_ADDR_5,
		RDMA_AHB_START_ADDR_5_MSB,
		RDMA_AHB_END_ADDR_5,
		RDMA_AHB_END_ADDR_5_MSB,
		RDMA_AUTO_SRC5_SEL, 0,
		RDMA_ACCESS_AUTO2, 1,
		RDMA_ACCESS_AUTO2, 5,
		29, 29
	},
	{RDMA_AHB_START_ADDR_6,
		RDMA_AHB_START_ADDR_6_MSB,
		RDMA_AHB_END_ADDR_6,
		RDMA_AHB_END_ADDR_6_MSB,
		RDMA_AUTO_SRC6_SEL, 0,
		RDMA_ACCESS_AUTO2, 2,
		RDMA_ACCESS_AUTO2, 6,
		30, 30
	},
	{RDMA_AHB_START_ADDR_7,
		RDMA_AHB_START_ADDR_7_MSB,
		RDMA_AHB_END_ADDR_7,
		RDMA_AHB_END_ADDR_7_MSB,
		RDMA_AUTO_SRC7_SEL, 0,
		RDMA_ACCESS_AUTO2, 3,
		RDMA_ACCESS_AUTO2, 7,
		31, 31
	}
};

static struct rdma_regadr_s rdma_regadr_t3x[] = {
	{T3X_RDMA_AHB_START_ADDR_MAN,
		T3X_RDMA_AHB_START_ADDR_MAN_MSB,
		T3X_RDMA_AHB_END_ADDR_MAN,
		T3X_RDMA_AHB_END_ADDR_MAN_MSB,
		0, 0,
		T3X_RDMA_ACCESS_MAN, 1,
		T3X_RDMA_ACCESS_MAN, 2,
		16, 4
	},
	{T3X_RDMA_AHB_START_ADDR_1,
		T3X_RDMA_AHB_START_ADDR_1_MSB,
		T3X_RDMA_AHB_END_ADDR_1,
		T3X_RDMA_AHB_END_ADDR_1_MSB,
		T3X_RDMA_AUTO_SRC1_SEL,  0,
		T3X_RDMA_ACCESS_AUTO,  1,
		T3X_RDMA_ACCESS_AUTO,  5,
		17, 5
	},
	{T3X_RDMA_AHB_START_ADDR_2,
		T3X_RDMA_AHB_START_ADDR_2_MSB,
		T3X_RDMA_AHB_END_ADDR_2,
		T3X_RDMA_AHB_END_ADDR_2_MSB,
		T3X_RDMA_AUTO_SRC2_SEL,  0,
		T3X_RDMA_ACCESS_AUTO,  2,
		T3X_RDMA_ACCESS_AUTO,  6,
		18, 6
	},
	{T3X_RDMA_AHB_START_ADDR_3,
		T3X_RDMA_AHB_START_ADDR_3_MSB,
		T3X_RDMA_AHB_END_ADDR_3,
		T3X_RDMA_AHB_END_ADDR_3_MSB,
		T3X_RDMA_AUTO_SRC3_SEL,  0,
		T3X_RDMA_ACCESS_AUTO,  3,
		T3X_RDMA_ACCESS_AUTO,  7,
		19, 7
	},
	{T3X_RDMA_AHB_START_ADDR_4,
		T3X_RDMA_AHB_START_ADDR_4_MSB,
		T3X_RDMA_AHB_END_ADDR_4,
		T3X_RDMA_AHB_END_ADDR_4_MSB,
		T3X_RDMA_AUTO_SRC4_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 0,
		T3X_RDMA_ACCESS_AUTO2, 4,
		20, 8
	},
	{T3X_RDMA_AHB_START_ADDR_5,
		T3X_RDMA_AHB_START_ADDR_5_MSB,
		T3X_RDMA_AHB_END_ADDR_5,
		T3X_RDMA_AHB_END_ADDR_5_MSB,
		T3X_RDMA_AUTO_SRC5_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 1,
		T3X_RDMA_ACCESS_AUTO2, 5,
		21, 9
	},
	{T3X_RDMA_AHB_START_ADDR_6,
		T3X_RDMA_AHB_START_ADDR_6_MSB,
		T3X_RDMA_AHB_END_ADDR_6,
		T3X_RDMA_AHB_END_ADDR_6_MSB,
		T3X_RDMA_AUTO_SRC6_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 2,
		T3X_RDMA_ACCESS_AUTO2, 6,
		22, 10
	},
	{T3X_RDMA_AHB_START_ADDR_7,
		T3X_RDMA_AHB_START_ADDR_7_MSB,
		T3X_RDMA_AHB_END_ADDR_7,
		T3X_RDMA_AHB_END_ADDR_7_MSB,
		T3X_RDMA_AUTO_SRC7_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 3,
		T3X_RDMA_ACCESS_AUTO2, 7,
		23, 11
	},
	{T3X_RDMA_AHB_START_ADDR_8,
		T3X_RDMA_AHB_START_ADDR_8_MSB,
		T3X_RDMA_AHB_END_ADDR_8,
		T3X_RDMA_AHB_END_ADDR_8_MSB,
		T3X_RDMA_AUTO_SRC8_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 0,
		T3X_RDMA_ACCESS_AUTO3, 8,
		24, 12
	},
	{T3X_RDMA_AHB_START_ADDR_9,
		T3X_RDMA_AHB_START_ADDR_9_MSB,
		T3X_RDMA_AHB_END_ADDR_9,
		T3X_RDMA_AHB_END_ADDR_9_MSB,
		T3X_RDMA_AUTO_SRC9_SEL,  0,
		T3X_RDMA_ACCESS_AUTO3,  1,
		T3X_RDMA_ACCESS_AUTO3,  9,
		25, 13
	},
	{T3X_RDMA_AHB_START_ADDR_10,
		T3X_RDMA_AHB_START_ADDR_10_MSB,
		T3X_RDMA_AHB_END_ADDR_10,
		T3X_RDMA_AHB_END_ADDR_10_MSB,
		T3X_RDMA_AUTO_SRC10_SEL,  0,
		T3X_RDMA_ACCESS_AUTO3,  2,
		T3X_RDMA_ACCESS_AUTO3,  10,
		26, 14
	},
	{T3X_RDMA_AHB_START_ADDR_11,
		T3X_RDMA_AHB_START_ADDR_11_MSB,
		T3X_RDMA_AHB_END_ADDR_11,
		T3X_RDMA_AHB_END_ADDR_11_MSB,
		T3X_RDMA_AUTO_SRC11_SEL,  0,
		T3X_RDMA_ACCESS_AUTO3,  3,
		T3X_RDMA_ACCESS_AUTO3,  11,
		27, 15
	},
	{T3X_RDMA_AHB_START_ADDR_12,
		T3X_RDMA_AHB_START_ADDR_12_MSB,
		T3X_RDMA_AHB_END_ADDR_12,
		T3X_RDMA_AHB_END_ADDR_12_MSB,
		T3X_RDMA_AUTO_SRC12_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 4,
		T3X_RDMA_ACCESS_AUTO3, 12,
		28, 16
	},
	{T3X_RDMA_AHB_START_ADDR_13,
		T3X_RDMA_AHB_START_ADDR_13_MSB,
		T3X_RDMA_AHB_END_ADDR_13,
		T3X_RDMA_AHB_END_ADDR_13_MSB,
		T3X_RDMA_AUTO_SRC13_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 5,
		T3X_RDMA_ACCESS_AUTO3, 13,
		29, 17
	},
	{T3X_RDMA_AHB_START_ADDR_14,
		T3X_RDMA_AHB_START_ADDR_14_MSB,
		T3X_RDMA_AHB_END_ADDR_14,
		T3X_RDMA_AHB_END_ADDR_14_MSB,
		T3X_RDMA_AUTO_SRC14_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 6,
		T3X_RDMA_ACCESS_AUTO3, 14,
		30, 18
	},
	{T3X_RDMA_AHB_START_ADDR_15,
		T3X_RDMA_AHB_START_ADDR_15_MSB,
		T3X_RDMA_AHB_END_ADDR_15,
		T3X_RDMA_AHB_END_ADDR_15_MSB,
		T3X_RDMA_AUTO_SRC15_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 7,
		T3X_RDMA_ACCESS_AUTO3, 15,
		31, 19
	}
};

static struct rdma_regadr_s rdma_regadr_t6w[] = {
	{T3X_RDMA_AHB_START_ADDR_MAN,
		T3X_RDMA_AHB_START_ADDR_MAN_MSB,
		T3X_RDMA_AHB_END_ADDR_MAN,
		T3X_RDMA_AHB_END_ADDR_MAN_MSB,
		0, 0,
		T3X_RDMA_ACCESS_MAN, 1,
		T3X_RDMA_ACCESS_MAN, 2,
		16, 4,
		RDMA_BUF_IDX_CLR, 0
	},
	{T3X_RDMA_AHB_START_ADDR_1,
		T3X_RDMA_AHB_START_ADDR_1_MSB,
		T3X_RDMA_AHB_END_ADDR_1,
		T3X_RDMA_AHB_END_ADDR_1_MSB,
		T3X_RDMA_AUTO_SRC1_SEL,  0,
		T3X_RDMA_ACCESS_AUTO,  1,
		T3X_RDMA_ACCESS_AUTO,  5,
		17, 5,
		RDMA_BUF_IDX_CLR, 1
	},
	{T3X_RDMA_AHB_START_ADDR_2,
		T3X_RDMA_AHB_START_ADDR_2_MSB,
		T3X_RDMA_AHB_END_ADDR_2,
		T3X_RDMA_AHB_END_ADDR_2_MSB,
		T3X_RDMA_AUTO_SRC2_SEL,  0,
		T3X_RDMA_ACCESS_AUTO,  2,
		T3X_RDMA_ACCESS_AUTO,  6,
		18, 6,
		RDMA_BUF_IDX_CLR, 2
	},
	{T3X_RDMA_AHB_START_ADDR_3,
		T3X_RDMA_AHB_START_ADDR_3_MSB,
		T3X_RDMA_AHB_END_ADDR_3,
		T3X_RDMA_AHB_END_ADDR_3_MSB,
		T3X_RDMA_AUTO_SRC3_SEL,  0,
		T3X_RDMA_ACCESS_AUTO,  3,
		T3X_RDMA_ACCESS_AUTO,  7,
		19, 7,
		RDMA_BUF_IDX_CLR, 3
	},
	{T3X_RDMA_AHB_START_ADDR_4,
		T3X_RDMA_AHB_START_ADDR_4_MSB,
		T3X_RDMA_AHB_END_ADDR_4,
		T3X_RDMA_AHB_END_ADDR_4_MSB,
		T3X_RDMA_AUTO_SRC4_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 0,
		T3X_RDMA_ACCESS_AUTO2, 4,
		20, 8,
		RDMA_BUF_IDX_CLR, 4
	},
	{T3X_RDMA_AHB_START_ADDR_5,
		T3X_RDMA_AHB_START_ADDR_5_MSB,
		T3X_RDMA_AHB_END_ADDR_5,
		T3X_RDMA_AHB_END_ADDR_5_MSB,
		T3X_RDMA_AUTO_SRC5_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 1,
		T3X_RDMA_ACCESS_AUTO2, 5,
		21, 9,
		RDMA_BUF_IDX_CLR, 5
	},
	{T3X_RDMA_AHB_START_ADDR_6,
		T3X_RDMA_AHB_START_ADDR_6_MSB,
		T3X_RDMA_AHB_END_ADDR_6,
		T3X_RDMA_AHB_END_ADDR_6_MSB,
		T3X_RDMA_AUTO_SRC6_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 2,
		T3X_RDMA_ACCESS_AUTO2, 6,
		22, 10,
		RDMA_BUF_IDX_CLR, 6
	},
	{T3X_RDMA_AHB_START_ADDR_7,
		T3X_RDMA_AHB_START_ADDR_7_MSB,
		T3X_RDMA_AHB_END_ADDR_7,
		T3X_RDMA_AHB_END_ADDR_7_MSB,
		T3X_RDMA_AUTO_SRC7_SEL, 0,
		T3X_RDMA_ACCESS_AUTO2, 3,
		T3X_RDMA_ACCESS_AUTO2, 7,
		23, 11,
		RDMA_BUF_IDX_CLR, 7
	},
	{T3X_RDMA_AHB_START_ADDR_8,
		T3X_RDMA_AHB_START_ADDR_8_MSB,
		T3X_RDMA_AHB_END_ADDR_8,
		T3X_RDMA_AHB_END_ADDR_8_MSB,
		T3X_RDMA_AUTO_SRC8_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 0,
		T3X_RDMA_ACCESS_AUTO3, 8,
		24, 12,
		RDMA_BUF_IDX_CLR, 8
	},
	{T3X_RDMA_AHB_START_ADDR_9,
		T3X_RDMA_AHB_START_ADDR_9_MSB,
		T3X_RDMA_AHB_END_ADDR_9,
		T3X_RDMA_AHB_END_ADDR_9_MSB,
		T3X_RDMA_AUTO_SRC9_SEL,  0,
		T3X_RDMA_ACCESS_AUTO3,  1,
		T3X_RDMA_ACCESS_AUTO3,  9,
		25, 13,
		RDMA_BUF_IDX_CLR, 9
	},
	{T3X_RDMA_AHB_START_ADDR_10,
		T3X_RDMA_AHB_START_ADDR_10_MSB,
		T3X_RDMA_AHB_END_ADDR_10,
		T3X_RDMA_AHB_END_ADDR_10_MSB,
		T3X_RDMA_AUTO_SRC10_SEL,  0,
		T3X_RDMA_ACCESS_AUTO3,  2,
		T3X_RDMA_ACCESS_AUTO3,  10,
		26, 14,
		RDMA_BUF_IDX_CLR, 10
	},
	{T3X_RDMA_AHB_START_ADDR_11,
		T3X_RDMA_AHB_START_ADDR_11_MSB,
		T3X_RDMA_AHB_END_ADDR_11,
		T3X_RDMA_AHB_END_ADDR_11_MSB,
		T3X_RDMA_AUTO_SRC11_SEL,  0,
		T3X_RDMA_ACCESS_AUTO3,  3,
		T3X_RDMA_ACCESS_AUTO3,  11,
		27, 15,
		RDMA_BUF_IDX_CLR, 11
	},
	{T3X_RDMA_AHB_START_ADDR_12,
		T3X_RDMA_AHB_START_ADDR_12_MSB,
		T3X_RDMA_AHB_END_ADDR_12,
		T3X_RDMA_AHB_END_ADDR_12_MSB,
		T3X_RDMA_AUTO_SRC12_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 4,
		T3X_RDMA_ACCESS_AUTO3, 12,
		28, 16,
		RDMA_BUF_IDX_CLR, 12
	},
	{T3X_RDMA_AHB_START_ADDR_13,
		T3X_RDMA_AHB_START_ADDR_13_MSB,
		T3X_RDMA_AHB_END_ADDR_13,
		T3X_RDMA_AHB_END_ADDR_13_MSB,
		T3X_RDMA_AUTO_SRC13_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 5,
		T3X_RDMA_ACCESS_AUTO3, 13,
		29, 17,
		RDMA_BUF_IDX_CLR, 13
	},
	{T3X_RDMA_AHB_START_ADDR_14,
		T3X_RDMA_AHB_START_ADDR_14_MSB,
		T3X_RDMA_AHB_END_ADDR_14,
		T3X_RDMA_AHB_END_ADDR_14_MSB,
		T3X_RDMA_AUTO_SRC14_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 6,
		T3X_RDMA_ACCESS_AUTO3, 14,
		30, 18,
		RDMA_BUF_IDX_CLR, 14
	},
	{T3X_RDMA_AHB_START_ADDR_15,
		T3X_RDMA_AHB_START_ADDR_15_MSB,
		T3X_RDMA_AHB_END_ADDR_15,
		T3X_RDMA_AHB_END_ADDR_15_MSB,
		T3X_RDMA_AUTO_SRC15_SEL, 0,
		T3X_RDMA_ACCESS_AUTO3, 7,
		T3X_RDMA_ACCESS_AUTO3, 15,
		31, 19,
		RDMA_BUF_IDX_CLR, 15
	}
};

static struct rdma_mbuf_regadr_s rdma_mbuf_regadr_t6w[] = {
	{RDMA_BUF_CTRL_MAN,
		RDMA_BUF_CFG_EN_MAN,
		RDMA_BUF_LENGTH_MAN_0,
		RDMA_BUF_LENGTH_MAN_1,
		RDMA_BUF_LENGTH_MAN_2,
		RDMA_BUF_LENGTH_MAN_3,
		RDMA_BUF_RO_MAN,
	},
	{RDMA_BUF_CTRL_1,
		RDMA_BUF_CFG_EN_1,
		RDMA_BUF_LENGTH_1_0,
		RDMA_BUF_LENGTH_1_1,
		RDMA_BUF_LENGTH_1_2,
		RDMA_BUF_LENGTH_1_3,
		RDMA_BUF_RO_1,
	},
	{RDMA_BUF_CTRL_2,
		RDMA_BUF_CFG_EN_2,
		RDMA_BUF_LENGTH_2_0,
		RDMA_BUF_LENGTH_2_1,
		RDMA_BUF_LENGTH_2_2,
		RDMA_BUF_LENGTH_2_3,
		RDMA_BUF_RO_2,
	},
	{RDMA_BUF_CTRL_3,
		RDMA_BUF_CFG_EN_3,
		RDMA_BUF_LENGTH_3_0,
		RDMA_BUF_LENGTH_3_1,
		RDMA_BUF_LENGTH_3_2,
		RDMA_BUF_LENGTH_3_3,
		RDMA_BUF_RO_3,
	},
	{RDMA_BUF_CTRL_4,
		RDMA_BUF_CFG_EN_4,
		RDMA_BUF_LENGTH_4_0,
		RDMA_BUF_LENGTH_4_1,
		RDMA_BUF_LENGTH_4_2,
		RDMA_BUF_LENGTH_4_3,
		RDMA_BUF_RO_4,
	},
	{RDMA_BUF_CTRL_5,
		RDMA_BUF_CFG_EN_5,
		RDMA_BUF_LENGTH_5_0,
		RDMA_BUF_LENGTH_5_1,
		RDMA_BUF_LENGTH_5_2,
		RDMA_BUF_LENGTH_5_3,
		RDMA_BUF_RO_5,
	},
	{RDMA_BUF_CTRL_6,
		RDMA_BUF_CFG_EN_6,
		RDMA_BUF_LENGTH_6_0,
		RDMA_BUF_LENGTH_6_1,
		RDMA_BUF_LENGTH_6_2,
		RDMA_BUF_LENGTH_6_3,
		RDMA_BUF_RO_6,
	},
	{RDMA_BUF_CTRL_7,
		RDMA_BUF_CFG_EN_7,
		RDMA_BUF_LENGTH_7_0,
		RDMA_BUF_LENGTH_7_1,
		RDMA_BUF_LENGTH_7_2,
		RDMA_BUF_LENGTH_7_3,
		RDMA_BUF_RO_7,
	},
	{RDMA_BUF_CTRL_8,
		RDMA_BUF_CFG_EN_8,
		RDMA_BUF_LENGTH_8_0,
		RDMA_BUF_LENGTH_8_1,
		RDMA_BUF_LENGTH_8_2,
		RDMA_BUF_LENGTH_8_3,
		RDMA_BUF_RO_8,
	},
	{RDMA_BUF_CTRL_9,
		RDMA_BUF_CFG_EN_9,
		RDMA_BUF_LENGTH_9_0,
		RDMA_BUF_LENGTH_9_1,
		RDMA_BUF_LENGTH_9_2,
		RDMA_BUF_LENGTH_9_3,
		RDMA_BUF_RO_9,
	},
	{RDMA_BUF_CTRL_10,
		RDMA_BUF_CFG_EN_10,
		RDMA_BUF_LENGTH_10_0,
		RDMA_BUF_LENGTH_10_1,
		RDMA_BUF_LENGTH_10_2,
		RDMA_BUF_LENGTH_10_3,
		RDMA_BUF_RO_10,
	},
	{RDMA_BUF_CTRL_11,
		RDMA_BUF_CFG_EN_11,
		RDMA_BUF_LENGTH_11_0,
		RDMA_BUF_LENGTH_11_1,
		RDMA_BUF_LENGTH_11_2,
		RDMA_BUF_LENGTH_11_3,
		RDMA_BUF_RO_11,
	},
	{RDMA_BUF_CTRL_12,
		RDMA_BUF_CFG_EN_12,
		RDMA_BUF_LENGTH_12_0,
		RDMA_BUF_LENGTH_12_1,
		RDMA_BUF_LENGTH_12_2,
		RDMA_BUF_LENGTH_12_3,
		RDMA_BUF_RO_12,
	},
	{RDMA_BUF_CTRL_13,
		RDMA_BUF_CFG_EN_13,
		RDMA_BUF_LENGTH_13_0,
		RDMA_BUF_LENGTH_13_1,
		RDMA_BUF_LENGTH_13_2,
		RDMA_BUF_LENGTH_13_3,
		RDMA_BUF_RO_13,
	},
	{RDMA_BUF_CTRL_14,
		RDMA_BUF_CFG_EN_14,
		RDMA_BUF_LENGTH_14_0,
		RDMA_BUF_LENGTH_14_1,
		RDMA_BUF_LENGTH_14_2,
		RDMA_BUF_LENGTH_14_3,
		RDMA_BUF_RO_14,
	},
	{RDMA_BUF_CTRL_15,
		RDMA_BUF_CFG_EN_15,
		RDMA_BUF_LENGTH_15_0,
		RDMA_BUF_LENGTH_15_1,
		RDMA_BUF_LENGTH_15_2,
		RDMA_BUF_LENGTH_15_3,
		RDMA_BUF_RO_15,
	},
};
#endif

static int handle_trans_vpp(int handle)
{
	int ret;
	int rdma_type = -1;

	rdma_type = get_rdma_type(handle);
	if (rdma_type < 0 || rdma_type > EX_VSYNC_RDMA)
		return -1;
	switch (rdma_type) {
	case VSYNC_RDMA:
		ret = RDMA_VPP0;
		break;
	case VSYNC_RDMA_VPP1:
		ret = RDMA_VPP1;
		break;
	case VSYNC_RDMA_VPP2:
		ret = RDMA_VPP2;
		break;
	case PRE_VSYNC_RDMA:
		ret = RDMA_PRE_VSYNC;
		break;
	case EX_VSYNC_RDMA:
		ret = RDMA_LOWLATENCY;
		break;
	}
	return ret;
}

static int rdma_ins_setting(int channel, void *op_arg, int table_size)
{
	struct rdma_device_info *info = &rdma_info;
	dma_addr_t dma_handle;
	int i = channel;

	if (i < rdma_meson_dev.channel_num && i >= 0) {
		info->rdma_ins[i].not_process = 0;
		info->rdma_ins[i].op_arg = op_arg;
		info->rdma_ins[i].rdma_item_count = 0;
		info->rdma_ins[i].rdma_write_count = 0;
		info->rdma_ins[i].prev_read_count = 0;
		if (info->rdma_ins[i].rdma_table_size == 0) {
			info->rdma_ins[i].rdma_table_addr =
				dma_alloc_coherent
				(&info->rdma_dev->dev, table_size,
				&dma_handle, GFP_KERNEL);
			info->rdma_ins[i].rdma_table_phy_addr =
				(ulong)(dma_handle);
			info->rdma_ins[i].reg_buf =
				kmalloc(table_size, GFP_KERNEL);
			info->rdma_ins[i].rdma_table_mirror =
				kmalloc(table_size, GFP_KERNEL);
			pr_debug("%s, rdma_table_addr %lx phy: %lx reg_buf %lx\n",
				__func__,
				(unsigned long)
				info->rdma_ins[i].rdma_table_addr,
				info->rdma_ins[i].rdma_table_phy_addr,
				(unsigned long)info->rdma_ins[i].reg_buf);
			info->rdma_ins[i].rdma_table_size = table_size;
		}

		if (!info->rdma_ins[i].rdma_table_addr ||
		    !info->rdma_ins[i].reg_buf ||
		    !info->rdma_ins[i].rdma_table_mirror) {
			if (!info->rdma_ins[i].keep_buf) {
				kfree(info->rdma_ins[i].reg_buf);
				info->rdma_ins[i].reg_buf = NULL;
				kfree(info->rdma_ins[i].rdma_table_mirror);
				info->rdma_ins[i].rdma_table_mirror = NULL;
			}
			if (info->rdma_ins[i].rdma_table_addr) {
				dma_free_coherent
				(&info->rdma_dev->dev,
				 table_size,
				 info->rdma_ins[i].rdma_table_addr,
				 (dma_addr_t)
				 info->rdma_ins[i].rdma_table_phy_addr);
				 info->rdma_ins[i].rdma_table_addr = NULL;
			}
			info->rdma_ins[i].rdma_table_size  = 0;
			info->rdma_ins[i].op = NULL;
			info->rdma_ins[i].used = 0;
			i = -1;
			pr_info("%s: memory allocate fail\n",
				__func__);
		} else {
			pr_debug("%s success, handle %d table_size %d\n",
				__func__, i, table_size);
		}
		return i;
	}

	return -1;
}

int rdma_register_manual(struct rdma_op_s *rdma_op, void *op_arg, int table_size)
{
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;

	spin_lock_irqsave(&rdma_lock, flags);
	if (!info->rdma_ins[0].op &&
	    info->rdma_ins[0].used == 0) {
		info->rdma_ins[0].op = rdma_op;
		info->rdma_ins[0].irq_count = 0;
		info->rdma_ins[0].rdma_config_count = 0;
		info->rdma_ins[0].rdma_empty_config_count = 0;
		info->rdma_ins[0].used = 1;
	} else {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_error("manual channel is already used\n");

		return -1;
	}
	spin_unlock_irqrestore(&rdma_lock, flags);

	return rdma_ins_setting(0, op_arg, table_size);
}
EXPORT_SYMBOL(rdma_register_manual);

int rdma_register(struct rdma_op_s *rdma_op, void *op_arg, int table_size)
{
	int i;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;

	spin_lock_irqsave(&rdma_lock, flags);
	for (i = 1; i < rdma_meson_dev.channel_num; i++) {
		/* 0 is reserved for RDMA MANUAL */
		if (!info->rdma_ins[i].op &&
		    info->rdma_ins[i].used == 0) {
			info->rdma_ins[i].op = rdma_op;
			info->rdma_ins[i].irq_count = 0;
			info->rdma_ins[i].rdma_config_count = 0;
			info->rdma_ins[i].rdma_empty_config_count = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&rdma_lock, flags);

	return rdma_ins_setting(i, op_arg, table_size);
}
EXPORT_SYMBOL(rdma_register);

void rdma_unregister(int i)
{
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;

	pr_info("%s(%d)\r\n", __func__, i);

	if (i <= 0 || i >= rdma_meson_dev.channel_num)
		return;
		/*rdma_clear(i);*/
	spin_lock_irqsave(&rdma_lock, flags);

	if (!info->rdma_ins[i].op) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		return;
	}

	info->rdma_ins[i].op = NULL;
	info->rdma_ins[i].op_arg = NULL;
	spin_unlock_irqrestore(&rdma_lock, flags);

	if (!info->rdma_ins[i].keep_buf) {
		kfree(info->rdma_ins[i].reg_buf);
		info->rdma_ins[i].reg_buf = NULL;
		kfree(info->rdma_ins[i].rdma_table_mirror);
		info->rdma_ins[i].rdma_table_mirror = NULL;
	}
	if (info->rdma_ins[i].rdma_table_addr) {
		dma_free_coherent
		(&info->rdma_dev->dev,
		 info->rdma_ins[i].rdma_table_size,
		 info->rdma_ins[i].rdma_table_addr,
		 (dma_addr_t)
		 info->rdma_ins[i].rdma_table_phy_addr);
		 info->rdma_ins[i].rdma_table_addr = NULL;
	}
	info->rdma_ins[i].rdma_table_size = 0;
}
EXPORT_SYMBOL(rdma_unregister);
static void rdma_reset(unsigned char external_reset)
{
	if (debug_flag & 4)
		pr_info("%s(%d)\n",
			__func__, external_reset);

	if (external_reset) {
		if (rdma_meson_dev.cpu_type >= CPU_SC2)
			reset_control_reset(rdma_rst);
		else
			WRITE_MPEG_REG
				(RESET4_REGISTER,
				(1 << 5));
	} else {
		WRITE_VCBUS_REG(rdma_ctrl, (0x1 << 1));
		WRITE_VCBUS_REG(rdma_ctrl, (0x1 << 1));
		WRITE_VCBUS_REG
		(rdma_ctrl,
		 (ctrl_ahb_wr_burst_size << 4) |
		 (ctrl_ahb_rd_burst_size << 2) |
		 (0x0 << 1));
	}
	reset_count++;
}

unsigned int rdma_hw_done_bit(void)
{
	u32 rdma_status, done_bit;

	rdma_status = READ_VCBUS_REG(irq_status.reg);
	done_bit = rdma_status >> irq_status.start;

	return done_bit;
}

static int rdma_isr_count;
irqreturn_t rdma_mgr_isr(int irq, void *dev_id)
{
	struct rdma_device_info *info = &rdma_info;
	int retry_count = 0;
	u32 rdma_status, mask;
	int i;
	u32 read_val;
	u32 cur_enc_line_start = 0, cur_enc_line_end = 0;

	if (debug_flag & DEBUG_RDMA_DONE_SKIP)
		return IRQ_HANDLED;
	rdma_done_cpuid = smp_processor_id();
	rdma_isr_count++;
QUERY:
	retry_count++;
	rdma_status = READ_VCBUS_REG(irq_status.reg);
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	cur_enc_line_start = get_cur_enc_line();
#endif
	if ((debug_flag & 4) && ((rdma_isr_count % 30) == 0))
		pr_info("%s: %x\r\n", __func__, rdma_status);
	for (i = 0; i < rdma_meson_dev.channel_num; i++) {
		struct rdma_instance_s *ins = &info->rdma_ins[i];

		if (ins->not_process)
			continue;
		if (!(rdma_status & (1 << (i + irq_status.start))))
			continue;
		/*bypass osd rdma done case */
#ifdef SKIP_OSD_CHANNEL
		if (i == 3)
			continue;
#endif
		if (rdma_status & (1 << ins->rdma_regadr->irq_status_bitpos)) {
			if (debug_flag & 2)
				pr_info("%s: process %d\r\n", __func__, i);

			if (ins->op && ins->op->irq_cb) {
				ins->op->irq_cb(ins->op->arg);
				ins->irq_count++;
			}

			WRITE_VCBUS_REG
				(rdma_ctrl,
				(1 << ins->rdma_regadr->clear_irq_bitpos));
		}
	}
	rdma_status = READ_VCBUS_REG(irq_status.reg);
#ifdef SKIP_OSD_CHANNEL
	if ((rdma_status & 0xf7000000) && retry_count < 100)
		goto QUERY;
#else
	mask = ((1L << irq_status.len) - 1) << irq_status.start;
	if ((rdma_status & mask) && retry_count < 100) {
		if (retry_count >= 3)
			pr_info("rdma_status=0x%x, retry_count=%d\n",
				rdma_status, retry_count);
		goto QUERY;
	}
#endif
	for (i = 0; i < MAX_CONFLICT; i++) {
		if (info->rdma_reg.adr[i]) {
			read_val = READ_VCBUS_REG(info->rdma_reg.adr[i]);
			if (read_val == info->rdma_reg.val[i])
				info->rdma_reg.adr[i] = 0;
		}
	}
	cur_enc_line_end = get_cur_enc_line();
	if (debug_flag & DEBUG_RDMA_DONE)
		pr_info("%s: enc_line(%d, %d)\n",
			__func__,
			cur_enc_line_start,
			cur_enc_line_end);
	return IRQ_HANDLED;
}

void rdma_stop(int handle)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];

	WRITE_VCBUS_REG_BITS(ins->rdma_regadr->trigger_mask_reg,
			     0, ins->rdma_regadr->trigger_mask_reg_bitpos,
			     rdma_meson_dev.trigger_mask_len);
}

void part_table_combine_to_reg_buf(int handle)
{
	int i;
	int vpp_index = -1;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;
	u32 *temp_combine_item;

	vpp_index = handle_trans_vpp(handle);
	if (vpp_index == -1)
		return;
	ins = &info->rdma_ins[handle];
	if (vpp_index == RDMA_VPP0 ||
		vpp_index == RDMA_PRE_VSYNC ||
		vpp_index == RDMA_LOWLATENCY) {
		temp_combine_item = ins->reg_buf + ins->rdma_item_count * 2;
		for (i = 0; i < PARTITION_TABLE_NUM; i++) {
			if (info->rdma_part_ins[vpp_index][i].flag) {
				if (!info->rdma_part_ins[vpp_index][i].rdma_item_count)
					continue;
				if ((ins->rdma_item_count +
					info->rdma_part_ins[vpp_index][i].rdma_item_count) * 2 >=
					ins->rdma_table_size / sizeof(u32)) {
					pr_info("err:%s, line=%d, vpp_index=%d, i=%d, temp_combine_item=%p, rdma_item_count=%d, %d\n",
						__func__, __LINE__,
						vpp_index, i,
						temp_combine_item,
						ins->rdma_item_count,
						info->rdma_part_ins[vpp_index][i].rdma_item_count);
					break;
				}
				memcpy(temp_combine_item,
					info->rdma_part_ins[vpp_index][i].rdma_item,
					info->rdma_part_ins[vpp_index][i].rdma_item_count *
					2 * sizeof(u32));
				temp_combine_item +=
					info->rdma_part_ins[vpp_index][i].rdma_item_count * 2;
				ins->rdma_item_count +=
					info->rdma_part_ins[vpp_index][i].rdma_item_count;
				info->rdma_part_ins[vpp_index][i].rdma_item_count = 0;
			}
		}
	}
}

/*
 *	trigger_type:
 *		0, stop,
 *		0x1~0xff, interrupt input trigger mode
 *		0x100, RDMA_TRIGGER_MANUAL
 *		> 0x100, debug mode
 *
 *	return:
 *		-1, fail
 *		0, rdma table is empty, will not have rdma irq
 *		1, success
 */
int rdma_config(int handle, u64 trigger_type)
{
	int ret = 0;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;
	bool auto_start = false;
	bool rdma_read = false;
	int buffer_lock = 0;
	u64 trigger_type_backup = trigger_type;
	int rdma_type;
	u32 cur_enc_line_start = 0, cur_enc_line_end = 0;
	bool configured = 0;

	if (handle < 0 || handle >= rdma_meson_dev.channel_num) {
		pr_info
		("%s error, (handle == %d) not allowed\n",
		 __func__, handle);
		return -1;
	}

	if (!enable[handle])
		return 0;

	spin_lock_irqsave(&rdma_lock, flags);
	ins = &info->rdma_ins[handle];
	if (!ins->op) {
		spin_unlock_irqrestore(&rdma_lock, flags);

		pr_info("%s: handle (%d) not register\n",
			__func__, handle);
		return -1;
	}

	cur_enc_line_start = get_cur_enc_line();
	if (!(trigger_type & RDMA_TRIGGER_OMIT_LOCK))
		buffer_lock = ins->lock_flag;

	if (trigger_type & RDMA_READ_MASK)
		rdma_read = true;

	trigger_type &= ~RDMA_READ_MASK;

	if (trigger_type & RDMA_AUTO_START_MASK)
		auto_start = true;

	trigger_type &= ~RDMA_AUTO_START_MASK;
	trigger_type &= ~RDMA_TRIGGER_OMIT_LOCK;

	part_table_combine_to_reg_buf(handle);

	if (debug_flag & DEBUG_RDMA_CONFIG)
		pr_info("%s: ins->rdma_item_count %d, ins->rdma_write_count %d, trigger_type 0x%llx\n",
			__func__, ins->rdma_item_count, ins->rdma_write_count, trigger_type);
	if (auto_start) {
		WRITE_VCBUS_REG_BITS
			(ins->rdma_regadr->trigger_mask_reg,
			 0,
			 ins->rdma_regadr->trigger_mask_reg_bitpos,
			 rdma_meson_dev.trigger_mask_len);

		WRITE_VCBUS_REG_BITS
			(ins->rdma_regadr->addr_inc_reg,
			 0,
			 ins->rdma_regadr->addr_inc_reg_bitpos,
			 1);
		WRITE_VCBUS_REG_BITS
			(ins->rdma_regadr->rw_flag_reg,
			 rdma_read ? 0 : 1,
			 ins->rdma_regadr->rw_flag_reg_bitpos,
			 1);
		WRITE_VCBUS_REG_BITS
			(ins->rdma_regadr->trigger_mask_reg,
			 trigger_type & 0xffffffff,
			 ins->rdma_regadr->trigger_mask_reg_bitpos,
			 rdma_meson_dev.trigger_mask_len);
		ret = 1;
		ins->rdma_write_count = 0;
	} else if (ins->rdma_item_count <= 0 ||
		trigger_type == 0 ||
		buffer_lock == 1) {
		if (trigger_type == RDMA_TRIGGER_MANUAL)
			WRITE_VCBUS_REG
			(rdma_access_man,
			 READ_VCBUS_REG(rdma_access_man) & (~1));
		if (debug_flag & DEBUG_RDMA_CONFIG) {
			pr_info("%s: handle=%d trigger_type 0x%llx : %d buffer_lock:%d\r\n",
				__func__,
				handle,
				trigger_type_backup,
				ins->rdma_item_count,
				buffer_lock);
		}
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->trigger_mask_reg,
		 0, ins->rdma_regadr->trigger_mask_reg_bitpos,
		 rdma_meson_dev.trigger_mask_len);
		ins->rdma_write_count = 0;
		ins->prev_read_count = 0;
		ins->rdma_empty_config_count++;
		ret = 0;
	} else {
		if (use_rdma_done_detect &&
		    handle == get_rdma_handle(PRE_VSYNC_RDMA)) {
			rdma_done_detect_cnt++;
			rdma_write_reg(handle, rdma_done_detect_reg,
				       rdma_done_detect_cnt);
		}
		memcpy(ins->rdma_table_addr, ins->reg_buf,
		       ins->rdma_item_count *
		       (rdma_read ? 1 : 2) * sizeof(u32));
		/* cp to mirror buf */
		memcpy(ins->rdma_table_mirror, ins->reg_buf,
		       ins->rdma_item_count *
		       (rdma_read ? 1 : 2) * sizeof(u32));
		ins->prev_read_count = ins->rdma_item_count;

		if ((trigger_type > 0 &&
		    trigger_type <= BIT(rdma_meson_dev.trigger_mask_len - 1)) ||
		    trigger_type == RDMA_TRIGGER_MANUAL) {
			ins->rdma_write_count = ins->rdma_item_count;
			ins->prev_trigger_type = trigger_type;
			if (trigger_type == RDMA_TRIGGER_MANUAL) {
				/*manual RDMA */
				struct rdma_instance_s *man_ins =
					&info->rdma_ins[0];

				WRITE_VCBUS_REG
				(rdma_access_man,
				 READ_VCBUS_REG(rdma_access_man) & (~1));
				if (support_64bit_addr) {
					#ifdef CONFIG_ARM64
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_start_addr,
					 ins->rdma_table_phy_addr & 0xffffffff);
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_start_addr_msb,
					(ins->rdma_table_phy_addr >> 32) & 0xffffffff);
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_end_addr,
					(ins->rdma_table_phy_addr + ins->rdma_item_count * 8 - 1)
					& 0xffffffff);
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_end_addr_msb,
					((ins->rdma_table_phy_addr
					+ ins->rdma_item_count * 8 - 1) >> 32) & 0xffffffff);
					#else
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_start_addr,
					 ins->rdma_table_phy_addr & 0xffffffff);
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_start_addr_msb,
					0);
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_end_addr,
					(ins->rdma_table_phy_addr +
					ins->rdma_item_count * 8 - 1)
					& 0xffffffff);
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_end_addr_msb,
					0);
					#endif
				} else {
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_start_addr,
					 ins->rdma_table_phy_addr & 0xffffffff);
					WRITE_VCBUS_REG
					(man_ins->rdma_regadr->rdma_ahb_end_addr,
					(ins->rdma_table_phy_addr & 0xffffffff)
					+ ins->rdma_item_count * 8 - 1);
				}

				WRITE_VCBUS_REG_BITS
				(man_ins->rdma_regadr->addr_inc_reg,
				 0,
				 man_ins->rdma_regadr->addr_inc_reg_bitpos,
				 1);
				WRITE_VCBUS_REG_BITS
				(man_ins->rdma_regadr->rw_flag_reg,
				 rdma_read ? 0 : 1,
				 man_ins->rdma_regadr->rw_flag_reg_bitpos,
				 1);
				/* Manual-start RDMA*/
				WRITE_VCBUS_REG
				(rdma_access_man,
				 READ_VCBUS_REG(rdma_access_man) | 1);

				if (debug_flag & 2)
					pr_info("%s: manual config %d:\r\n",
						__func__, ins->rdma_item_count);
			} else {
				/* interrupt input trigger RDMA */
				if (debug_flag & 2 ||
					(rdma_trace_enable &&
					rdma_trace_channel == handle))
					pr_info("%s: handle=%d case 3 : %d:\r\n",
					__func__, handle, ins->rdma_item_count);
				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->trigger_mask_reg,
				 0,
				 ins->rdma_regadr->trigger_mask_reg_bitpos,
				 rdma_meson_dev.trigger_mask_len);
				if (support_64bit_addr) {
					#ifdef CONFIG_ARM64
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_start_addr,
					ins->rdma_table_phy_addr & 0xffffffff);
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_start_addr_msb,
					(ins->rdma_table_phy_addr >> 32) & 0xffffffff);
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_end_addr,
					 (ins->rdma_table_phy_addr +
					 ins->rdma_item_count * 8 - 1) & 0xffffffff);
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_end_addr_msb,
					 ((ins->rdma_table_phy_addr +
					 ins->rdma_item_count * 8 - 1) >> 32) & 0xffffffff);
					#else
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_start_addr,
					ins->rdma_table_phy_addr & 0xffffffff);
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_start_addr_msb,
					0);
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_end_addr,
					(ins->rdma_table_phy_addr +
					ins->rdma_item_count * 8 - 1) &
					0xffffffff);
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_end_addr_msb,
					0);
					#endif
				} else {
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_start_addr,
					ins->rdma_table_phy_addr & 0xffffffff);
					WRITE_VCBUS_REG
					(ins->rdma_regadr->rdma_ahb_end_addr,
					 (ins->rdma_table_phy_addr & 0xffffffff) +
					 ins->rdma_item_count * 8 - 1);
				}
				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->addr_inc_reg,
				 0,
				 ins->rdma_regadr->addr_inc_reg_bitpos,
				 1);
				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->rw_flag_reg,
				 rdma_read ? 0 : 1,
				 ins->rdma_regadr->rw_flag_reg_bitpos,
				 1);
				WRITE_VCBUS_REG_BITS
				(ins->rdma_regadr->trigger_mask_reg,
				 trigger_type & 0xffffffff,
				 ins->rdma_regadr->trigger_mask_reg_bitpos,
				 rdma_meson_dev.trigger_mask_len);
			}
		} else if (trigger_type == 0x81000000) {	/* debug mode */
			int i;

			for (i = 0; i < ins->rdma_item_count; i++) {
				WRITE_VCBUS_REG
					(ins->rdma_table_addr[i << 1],
					 ins->rdma_table_addr[(i << 1) + 1]);
				if (debug_flag & 1)
					pr_info("WR(%x)<=%x\n",
						ins->rdma_table_addr[i << 1],
						ins->rdma_table_addr
						[(i << 1) + 1]);
			}
			ins->rdma_write_count = 0;
		} else if (trigger_type == 0x82000000) { /* debug mode */
			int i;

			for (i = 0; i < ins->rdma_item_count; i++) {
				WRITE_VCBUS_REG(ins->reg_buf[i << 1],
						ins->reg_buf[(i << 1) + 1]);
				if (debug_flag & 1)
					pr_info("WR(%x)<=%x\n",
						ins->reg_buf[i << 1],
						ins->reg_buf[(i << 1) + 1]);
			}
			ins->rdma_write_count = 0;
		}
		ret = 1;
	}

	rdma_type = get_rdma_type(handle);
	if (rdma_type >= 0) {
		configured = ret ? 1 : 0;
		if (configured) {
			struct timeval t;

			do_gettimeofday(&t);
			rdma_config_us[rdma_type] =
				t.tv_sec * 1000000 + t.tv_usec;
			enc_num_configed[rdma_type] = get_cur_enc_num();
		} else {
			enc_num_configed[rdma_type] = 0xff;
		}
		rdma_configured[rdma_type] = configured;
	}
	/* don't reset rdma_item_count for read function */
	if ((!rdma_read && !buffer_lock) || handle == 0) {
		if (handle == get_rdma_handle(VSYNC_RDMA)) {
			g_vsync_rdma_item_count = ins->rdma_item_count;
			if (g_vsync_rdma_item_count >= g_vsync_rdma_item_count_max)
				g_vsync_rdma_item_count_max = g_vsync_rdma_item_count;
		}
		ins->rdma_item_count = 0;
	}
	cur_enc_line_end = get_cur_enc_line();
	spin_unlock_irqrestore(&rdma_lock, flags);

	if (debug_flag & 2 ||
		debug_flag & DEBUG_RDMA_CONFIG ||
		(rdma_trace_enable &&
		rdma_trace_channel == handle))
		pr_info("%s: (%d 0x%llx) rdma_item_count=%d, ret %d, enc line(%d, %d)\r\n",
			__func__, handle, trigger_type_backup, ins->rdma_item_count, ret,
			cur_enc_line_start, cur_enc_line_end);
	ins->rdma_config_count++;
	return ret;
}
EXPORT_SYMBOL(rdma_config);

int rdma_clear(int handle)
{
	int ret = 0;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;

	spin_lock_irqsave(&rdma_lock, flags);
	if (handle < 0 ||
	    handle >= rdma_meson_dev.channel_num) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_info("%s error, handle (%d) not register\n",
			__func__, handle);
		return -1;
	}
	ins = &info->rdma_ins[handle];
	if (!ins->op) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_info("%s error, handle (%d) op:%p\n",
			__func__, handle, ins->op);
		return -2;
	}
	WRITE_VCBUS_REG_BITS(ins->rdma_regadr->trigger_mask_reg,
			     0, ins->rdma_regadr->trigger_mask_reg_bitpos,
			     rdma_meson_dev.trigger_mask_len);
	if (debug_flag & 2 ||
		(rdma_trace_enable &&
		rdma_trace_channel == handle))
		pr_info("%s: handle=%d : write count %d item count %d\n",
			__func__, handle,
			ins->rdma_write_count, ins->rdma_item_count);
	ins->rdma_write_count = 0;
	ins->rdma_item_count = 0;
	spin_unlock_irqrestore(&rdma_lock, flags);
	return ret;
}
EXPORT_SYMBOL(rdma_clear);

u32 rdma_read_reg(int handle, u32 adr)
{
	int i, j = 0;
	u32 *write_table;
	int match = 0;
	int match_oth = 0, match_mirror = 0, match_lowlatency = 0;
	int read_from = 0;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 read_val = READ_VCBUS_REG(adr);

	for (i = 0; i < MAX_CONFLICT; i++) {
		if (info->rdma_reg.adr[i] == adr) {
			read_val = info->rdma_reg.val[i];
			match_oth = 1;
			read_from = 3;
			break;
		}
	}
	if (!match_oth) {
		for (i = (ins->rdma_item_count - 1); i >= 0; i--) {
			if (ins->reg_buf[i << 1] == adr) {
				read_val = ins->reg_buf[(i << 1) + 1];
				match = 1;
				read_from = 1;
				break;
			}
		}
	}
	/* changed to read from rdma_table_adr to mirror for optimize */
	if (!match) {
		write_table = ins->rdma_table_mirror;
		for (i = (ins->rdma_write_count - 1);
			i >= 0; i--) {
			if (write_table[i << 1] == adr) {
				read_val =
					write_table[(i << 1) + 1];
				match_mirror = 1;
				read_from = 2;
				break;
			}
		}
	}
	if (!match_mirror && !match) {
		/* changed to read from rdma_table_adr to lowlatency channel for optimize */
		if (handle != lowlatency_reg.rdma_handle &&
			(lowlatency_reg.rdma_handle > 0 &&
			lowlatency_reg.rdma_handle < rdma_meson_dev.channel_num)) {
			struct rdma_instance_s *ins_lowlatency =
				&info->rdma_ins[lowlatency_reg.rdma_handle];

			write_table = ins_lowlatency->rdma_table_mirror;
			for (i = (ins_lowlatency->rdma_write_count - 1);
				i >= 0; i--) {
				if (write_table[i << 1] == adr) {
					read_val =
						write_table[(i << 1) + 1];
					match_lowlatency = 1;
					read_from = 4;
					if (rdma_trace_enable)
						pr_info("(%s) handle %d, %04x=0x%08x from lowlatency table(%d), cur_val:0x%x\n",
							__func__,
							lowlatency_reg.rdma_handle, adr,
							read_val,
							i,
							READ_VCBUS_REG(adr));
					break;
				}
			}
		}
	}
	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				if (read_from == 3)
					pr_info("(%s) handle %d, %04x=0x%08x from conflict table(%d), cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 2)
					pr_info("(%s) handle %d, %04x=0x%08x from write table(%d), cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 1)
					pr_info("(%s) handle %d, %04x=0x%08x from item table(%d), cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						ins->rdma_item_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 4)
					pr_info("(%s) handle %d, %04x=0x%08x from lowlatency table(%d), cur_val:0x%x\n",
						__func__,
						lowlatency_reg.rdma_handle, adr,
						read_val,
						i,
						READ_VCBUS_REG(adr));
				else
					pr_info("(%s) handle %d, %04x=0x%08x from real reg, cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						READ_VCBUS_REG(adr));
			}
		}
	}
	return read_val;
}
EXPORT_SYMBOL(rdma_read_reg);

u32 rdma_read_reg_write_table(int handle, u32 adr, bool *matched)
{
	int i;
	u32 *write_table;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 read_val = 0;

	/* changed to read from rdma_table_adr to mirror for optimize */
	write_table = ins->rdma_table_mirror;
	for (i = (ins->rdma_write_count - 1);
		i >= 0; i--) {
		if (write_table[i << 1] == adr) {
			read_val =
				write_table[(i << 1) + 1];
			*matched = true;
			break;
		}
	}

	return read_val;
}
EXPORT_SYMBOL(rdma_read_reg_write_table);

int rdma_buffer_lock(int handle)
{
	int ret = 0;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;

	spin_lock_irqsave(&rdma_lock, flags);
	if (handle < 0 ||
		handle >= rdma_meson_dev.channel_num) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_info("%s error, handle (%d) not register\n",
			__func__, handle);
		return -1;
	}
	ins = &info->rdma_ins[handle];
	if (!ins->op) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_info("%s error, handle (%d) op:%p\n",
			__func__, handle, ins->op);
		return -2;
	}

	ins->lock_flag = 1;
	spin_unlock_irqrestore(&rdma_lock, flags);
	return ret;
}
EXPORT_SYMBOL(rdma_buffer_lock);

int rdma_buffer_unlock(int handle)
{
	int ret = 0;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;

	spin_lock_irqsave(&rdma_lock, flags);
	if (handle < 0 ||
		handle >= rdma_meson_dev.channel_num) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_info("%s error, handle (%d) not register\n",
			__func__, handle);
		return -1;
	}
	ins = &info->rdma_ins[handle];
	if (!ins->op) {
		spin_unlock_irqrestore(&rdma_lock, flags);
		pr_info("%s error, handle (%d) op:%p\n",
			__func__, handle, ins->op);
		return -2;
	}

	ins->lock_flag = 0;
	spin_unlock_irqrestore(&rdma_lock, flags);
	return ret;
}
EXPORT_SYMBOL(rdma_buffer_unlock);

int rdma_watchdog_setting(int flag, int handle)
{
	int ret = 0;

	if (handle < 0 || handle >= RDMA_NUM) {
		pr_info("%s, flag:%d handle:%d out of range[0, %d)\n",
			__func__, flag, handle, RDMA_NUM);
		return 0;
	}

	if (flag == 0)
		rdma_watchdog_count[handle] = 0;
	else
		rdma_watchdog_count[handle]++;

	if (debug_flag & 8) {
		rdma_force_reset = 1;
		debug_flag = 0;
	}
	if (debug_flag & DEBUG_RDMA_WATCHDOG)
		pr_info("%s, flag:%d handle:%d rdma_watchdog_count:%d\n",
			__func__, flag, handle, rdma_watchdog_count[handle]);

	if ((rdma_watchdog > 0 &&
	     rdma_watchdog_count[handle] > rdma_watchdog) ||
	     rdma_force_reset > 0) {
		pr_info("%s rdma reset: %d, force flag:%d handle:%d\n",
			__func__,
			rdma_watchdog_count[handle],
			rdma_force_reset,
			handle);
		rdma_watchdog_count[handle] = 0;
		rdma_force_reset = 0;
		rdma_reset(1);
		rdma_reset_trigger_flag[handle] = 1;
		ret = 1;
	}
	return ret;
}
EXPORT_SYMBOL(rdma_watchdog_setting);

static bool rdma_check_conflict(int handle, u32 adr, u32 *read_val)
{
	struct rdma_instance_s *oth_ins;
	int i, j, k, n;

	for (i = 0; i < MAX_CONFLICT; i++) {
		if (rdma_info.rdma_reg.adr[i] == adr) {
			if (read_val)
				*read_val = rdma_info.rdma_reg.val[i];
			return true;
		}
	}

	for (i = 0; i < rdma_meson_dev.channel_num; i++) {
		oth_ins = &rdma_info.rdma_ins[i];
		if (!oth_ins->rdma_table_size ||
		    !oth_ins->rdma_item_count ||
		    i == handle)
			continue;
		for (j = 0; j < oth_ins->rdma_item_count; j++) {
			if (oth_ins->reg_buf[j << 1] != adr)
				continue;
			for (n = 0; n < rdma_trace_num; n++) {
				if (adr == rdma_trace_reg[n] ||
				    (debug_flag & DEBUG_RDMA_REG_CONFLICT))
					pr_info("(%s) handle %d, conflict write %04x=0x%08x (oth handle %d), cur_val:0x%x\n",
						__func__,
						handle, adr,
						oth_ins->reg_buf[(j << 1) + 1],
						i,
						READ_VCBUS_REG(adr));
			}
			for (k = 0; k < MAX_CONFLICT; k++) {
				if (!rdma_info.rdma_reg.adr[i]) {
					rdma_info.rdma_reg.adr[k] = adr;
					rdma_info.rdma_reg.val[k] =
						oth_ins->reg_buf[(j << 1) + 1];
					if (read_val)
						*read_val = oth_ins->reg_buf
							[(j << 1) + 1];
					return true;
				}
			}
		}
	}
	return false;
}

static void rdma_update_conflict(u32 adr, u32 val)
{
	int i;

	for (i = 0; i < MAX_CONFLICT; i++) {
		if (rdma_info.rdma_reg.adr[i] == adr) {
			rdma_info.rdma_reg.val[i] = val;
			if (debug_flag & DEBUG_RDMA_REG_CONFLICT)
				pr_info("(%s) %04x=0x%08x\n",
					__func__,
					adr, val);
			break;
		}
	}
}

int rdma_write_reg(int handle, u32 adr, u32 val)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	int j = 0;
	bool part_conflict_check = true;
	int vpp_index = handle_trans_vpp(handle);

	if (vpp_index == -1)
		part_conflict_check = false;
	if (ins->rdma_table_size == 0)
		return -1;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	u8 cur_cpuid = smp_processor_id();
	if ((get_rdma_handle(VSYNC_RDMA) == handle) &&
		(cur_cpuid == 0) &&
		(cur_cpuid != rdma_done_cpuid ||
		(!is_in_vsync_isr(cur_cpuid) &&
#ifdef CONFIG_AMLOGIC_LCD
#ifdef CONFIG_AMLOGIC_BL_LDIM
		!is_in_ldim_vsync_isr(cur_cpuid) &&
#endif
#ifdef CONFIG_AMLOGIC_AMBILIGHT
		!is_in_amblt_vsync_isr(cur_cpuid) &&
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
		!is_in_tcon_vsync_isr(cur_cpuid) &&
#endif
#endif
		!is_in_pre_vsync_isr(cur_cpuid) &&
		!is_in_vsync_isr_viu2(cur_cpuid) &&
		!is_in_vsync_isr_viu3(cur_cpuid)
		))) {
		dump_stack();
		pr_info("rdma_write(%d)(%s) %d(%x)<=%x\n",
			handle, current->comm, ins->rdma_item_count, adr, val);
	}
#endif
	if (adr == 0) {
		pr_info("rdma_write(%d)(%s) write zero addr = %x, count:%d\n",
			handle, current->comm, val, ins->rdma_item_count);
		dump_stack();
	}
	/*
	 *only video use partition table can use this debug function
	 *to trace video reg write by pq/db
	 */
	if ((debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT) && part_conflict_check &&
		info->rdma_part_ins[vpp_index][VIDEO_PARTITION_TABLE].flag) {
		for (j = 0; j < video_reg.reg_count; j++) {
			if (adr == video_reg.reg_table[j]) {
				pr_info("ERROR: video reg: 0x%x be write by other module\n", adr);
				if (debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT_STACK)
					dump_stack();
			}
		}
	}
	if (debug_flag & 1 ||
		(rdma_trace_enable &&
		rdma_trace_channel == handle))
		pr_info("rdma_write(%d)(%s) %d(%x)<=%x\n",
			handle, current->comm, ins->rdma_item_count, adr, val);
	if (rdma_check_conflict(handle, adr, NULL))
		rdma_update_conflict(adr, val);

	if (((ins->rdma_item_count << 1) + 1) <
		(ins->rdma_table_size / sizeof(u32))) {
		ins->reg_buf[ins->rdma_item_count << 1] = adr;
		ins->reg_buf[(ins->rdma_item_count << 1) + 1] = val;
		ins->rdma_item_count++;
	} else {
		int i;

		pr_info("%s(%s)(%d, %x, %x ,%d) buf overflow, ins->rdma_item_count=%d\n",
			__func__, current->comm, rdma_watchdog_count[handle],
			handle, adr, val,
			ins->rdma_item_count);
		for (i = 0; i < ins->rdma_item_count; i++)
			WRITE_VCBUS_REG(ins->reg_buf[i << 1],
					ins->reg_buf[(i << 1) + 1]);
		ins->rdma_item_count = 0;
		ins->rdma_write_count = 0;
		ins->reg_buf[ins->rdma_item_count << 1] = adr;
		ins->reg_buf[(ins->rdma_item_count << 1) + 1] = val;
		ins->rdma_item_count++;
	}

	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				pr_info("(%s) handle %d(%s), %04x=0x%08x (%d), cur_val:0x%x\n",
					__func__,
					handle, current->comm, adr,
					val,
					ins->rdma_item_count,
					READ_VCBUS_REG(adr));
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(rdma_write_reg);

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
void video_reg_write_check_table_init(void)
{
	int table_size = 2048 * 8;

	video_reg.reg_table = kmalloc(table_size, GFP_KERNEL);
	if (!video_reg.reg_table) {
		kfree(video_reg.reg_table);
		video_reg.reg_table = NULL;
	} else {
		video_reg.reg_count =
			init_rdma_check_video_reg_table(video_reg.reg_table);
	}

	video_reg.conflict_reg_table = kmalloc(table_size, GFP_KERNEL);
	if (!video_reg.conflict_reg_table) {
		kfree(video_reg.conflict_reg_table);
		video_reg.conflict_reg_table = NULL;
	} else {
		video_reg.conflict_reg_count =
			init_conflict_reg_table(video_reg.conflict_reg_table);
	}
}
#endif

bool get_part_flag_status(int vpp_index, int tbl_index)
{
	bool ret;
	struct rdma_device_info *info = &rdma_info;

	if (info->rdma_part_ins[vpp_index][tbl_index].flag)
		ret = true;
	else
		ret = false;
	return ret;
}

void set_part_flag_status(int vpp_index, int tbl_index, bool set_flag)
{
	struct rdma_device_info *info = &rdma_info;

	if (info->rdma_part_ins[vpp_index][tbl_index].flag != set_flag)
		info->rdma_part_ins[vpp_index][tbl_index].flag = set_flag;
	else
		pr_info("vpp:%d tbl_index:%d flag is %d",
			vpp_index, tbl_index, info->rdma_part_ins[vpp_index][tbl_index].flag);
}

struct rdma_partition_ins_s *get_part_table_ins(int vpp_index, int tbl_index)
{
	struct rdma_device_info *info = &rdma_info;

	return &info->rdma_part_ins[vpp_index][tbl_index];
}

int rdma_part_table_register(struct rdma_partition_ins_s *rdma_part_ins)
{
	int table_size;
	int tbl_index;
	int table_cnt;
	int vpp_index;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_partition_ins_s *ins;
	struct rdma_partition_ins_s *ins_lowlatency;

	if (!rdma_part_ins->flag ||
		rdma_part_ins->table_index >= PARTITION_TABLE_NUM) {
		pr_info("tbl_index register rdma part table fail\n");
		return -1;
	}
	tbl_index = rdma_part_ins->table_index;
	vpp_index = rdma_part_ins->vpp_index;
	table_cnt = rdma_part_ins->max_reg_cnt;
	if (vpp_index == RDMA_VPP0) {
		memcpy(&info->rdma_part_ins[RDMA_LOWLATENCY][tbl_index],
			rdma_part_ins, sizeof(struct rdma_partition_ins_s));
		ins_lowlatency = &info->rdma_part_ins[RDMA_LOWLATENCY][tbl_index];
		ins_lowlatency->vpp_index = RDMA_LOWLATENCY;
	}
	ins = &info->rdma_part_ins[vpp_index][tbl_index];

	if (table_cnt == 0) {
		table_size = RDMA_PARTITION_TABLE_SIZE;
		ins->max_reg_cnt = RDMA_PARTITION_TABLE_CNT;
	} else {
		table_size = table_cnt * 8;
		ins->max_reg_cnt = rdma_part_ins->max_reg_cnt;
	}
	ins->flag = rdma_part_ins->flag;
	ins->table_index = rdma_part_ins->table_index;
	ins->rdma_item_count = 0;
	ins->reg_range_check = rdma_part_ins->reg_range_check;
	ins->check_start_addr = rdma_part_ins->check_start_addr;
	ins->check_end_addr = rdma_part_ins->check_end_addr;

	if (ins->flag)
		ins->rdma_item = kmalloc(table_size, GFP_KERNEL);
	if (!ins->rdma_item) {
		kfree(ins->rdma_item);
		ins->rdma_item = NULL;
		return -1;
	}
	ins->vpp_index = rdma_part_ins->vpp_index;
	if (vpp_index == RDMA_VPP0) {
		ins_lowlatency->max_reg_cnt = ins->max_reg_cnt;
		ins_lowlatency->flag = ins->flag;
		ins_lowlatency->table_index = ins->table_index;
		ins_lowlatency->rdma_item_count = 0;
		ins_lowlatency->reg_range_check = ins->reg_range_check;
		ins_lowlatency->check_start_addr = ins->check_start_addr;
		ins_lowlatency->check_end_addr = ins->check_end_addr;
		if (ins_lowlatency->flag)
			ins_lowlatency->rdma_item = kmalloc(table_size, GFP_KERNEL);
		if (!ins_lowlatency->rdma_item) {
			kfree(ins_lowlatency->rdma_item);
			ins_lowlatency->rdma_item = NULL;
			return -1;
		}
	}
	return tbl_index;
}
EXPORT_SYMBOL(rdma_part_table_register);

void free_part_table_memory(void)
{
	int i, j;
	struct rdma_device_info *info = &rdma_info;

	for (i = 0; i < RDMA_VPP_MAX; i++) {
		for (j = 0; j < PARTITION_TABLE_NUM; j++) {
			if (info->rdma_part_ins[i][j].flag) {
				kfree(info->rdma_part_ins[i][j].rdma_item);
				info->rdma_part_ins[i][j].rdma_item = NULL;
			}
		}
	}
	if (!video_reg.reg_table) {
		kfree(video_reg.reg_table);
		video_reg.reg_table = NULL;
	}
	if (!video_reg.conflict_reg_table) {
		kfree(video_reg.conflict_reg_table);
		video_reg.conflict_reg_table = NULL;
	}
}

u32 rdma_part_read_reg(int tbl_index, int handle, u32 adr)
{
	int i, j = 0;
	int match = 0;
	u32 *write_table;
#ifdef DEBUG_RDMA_OPTIMIZE
	int match_oth = 0;
#endif
	int read_from = 0;
	int vpp_index = handle_trans_vpp(handle);

	if (vpp_index == -1) {
		pr_info("%s %d vpp_index error\n", __func__, __LINE__);
		return 0;
	}
	for (i = 0; i < video_reg.conflict_reg_count; i++) {
		if (video_reg.conflict_reg_table[i] != 0 &&
			adr == video_reg.conflict_reg_table[i])
			return rdma_read_reg(handle, adr);
	}
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	struct rdma_partition_ins_s *part_ins = &info->rdma_part_ins[vpp_index][tbl_index];
	u32 read_val = READ_VCBUS_REG(adr);
#ifdef DEBUG_RDMA_OPTIMIZE
	for (i = 0; i < MAX_CONFLICT; i++) {
		if (info->rdma_reg.adr[i] == adr) {
			read_val = info->rdma_reg.val[i];
			match_oth = 1;
			read_from = 3;
			break;
		}
	}

	if (!match_oth) {
#endif
		for (i = part_ins->rdma_item_count - 1;
				i >= 0; i--) {
			if (part_ins->rdma_item[i << 1] == adr) {
				read_val = part_ins->rdma_item[(i << 1) + 1];
				match = 1;
				read_from = 1;
				break;
			}
		}
#ifdef DEBUG_RDMA_OPTIMIZE
	}
#endif
	/* changed to read from rdma_table_adr to mirror for optimize */
	if (!match) {
		write_table = ins->rdma_table_mirror;
		for (i = (ins->rdma_write_count - 1);
			i >= 0; i--) {
			if (write_table[i << 1] == adr) {
				read_val =
					write_table[(i << 1) + 1];
				read_from = 2;
				break;
			}
		}
	}
	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				if (read_from == 3)
					pr_info("(%s) handle %d, %04x=0x%08x from conflict table(%d), cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 2)
					pr_info("(%s) handle %d, %04x=0x%08x from write table(%d), cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 1)
					pr_info("(%s) handle %d, %04x=0x%08x from partition table:%d (%d), cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						tbl_index,
						part_ins->rdma_item_count,
						READ_VCBUS_REG(adr));
				else
					pr_info("(%s) handle %d, %04x=0x%08x from real reg, cur_val:0x%x\n",
						__func__,
						handle, adr,
						read_val,
						READ_VCBUS_REG(adr));
			}
		}
	}
	return read_val;
}
EXPORT_SYMBOL(rdma_part_read_reg);

int rdma_part_write_reg(int tbl_index, int handle, u32 adr, u32 val)
{
	struct rdma_device_info *info = &rdma_info;
	int vpp_index = handle_trans_vpp(handle);
	int i = 0, j = 0;

	if (vpp_index == -1) {
		pr_info("%s %d vpp_index error\n", __func__, __LINE__);
		return 0;
	}
	for (i = 0; i < video_reg.conflict_reg_count; i++) {
		if (video_reg.conflict_reg_table[i] != 0 &&
			adr == video_reg.conflict_reg_table[i])
			return rdma_write_reg(handle, adr, val);
	}
	struct rdma_partition_ins_s *part_ins = &info->rdma_part_ins[vpp_index][tbl_index];
	u32 rdma_tbl_count = part_ins->rdma_item_count;
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	u8 cur_cpuid = smp_processor_id();

	if ((get_rdma_handle(VSYNC_RDMA) == handle) &&
		(cur_cpuid != rdma_done_cpuid ||
		(!is_in_vsync_isr(cur_cpuid) &&
#ifdef CONFIG_AMLOGIC_BL_LDIM
		!is_in_ldim_vsync_isr(cur_cpuid) &&
#endif
#ifdef CONFIG_AMLOGIC_AMBILIGHT
		!is_in_amblt_vsync_isr(cur_cpuid) &&
#endif
		!is_in_pre_vsync_isr(cur_cpuid) &&
		!is_in_vsync_isr_viu2(cur_cpuid) &&
		!is_in_vsync_isr_viu3(cur_cpuid)
		))) {
		dump_stack();
		pr_info("%s(table_idx:%d)(%d)(%s) %d(%x)<=%x\n", __func__,
			tbl_index, handle, current->comm, rdma_tbl_count, adr, val);
	}
#endif
	/*
	 *only video use partition table can use this debug function
	 *to trace video reg write by pq/db
	 */
	if ((debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT) &&
		info->rdma_part_ins[vpp_index][VIDEO_PARTITION_TABLE].flag) {
		for (j = 0; j < video_reg.reg_count; j++) {
			if (adr == video_reg.reg_table[j] &&
				tbl_index != VIDEO_PARTITION_TABLE) {
				pr_info("ERROR: part table index:%d video reg: 0x%x\n",
						tbl_index, adr);
				if (debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT_STACK)
					dump_stack();
			}
		}
	}
	if (adr == 0) {
		pr_info("%s(table_idx:%d)(%d)(%s) write zero addr = %x, count:%d\n",
			__func__, tbl_index, handle, current->comm, val, rdma_tbl_count);
		dump_stack();
	}
	if (part_ins->reg_range_check &&
		(adr > part_ins->check_end_addr ||
		adr < part_ins->check_start_addr)) {
		pr_info("part index:%d write reg: 0x%x out of range:0x%x - 0x%x",
			tbl_index, adr, part_ins->check_start_addr,
			part_ins->check_end_addr);
	}
	if (debug_flag & 1 ||
		(rdma_trace_enable &&
		rdma_trace_channel == handle))
		pr_info("%s(table_idx:%d)(%d)(%s) %d(%x)<=%x\n", __func__,
			tbl_index, handle, current->comm, rdma_tbl_count, adr, val);
	if (rdma_check_conflict(handle, adr, NULL))
		rdma_update_conflict(adr, val);

	if (part_ins->flag) {
		if (part_ins->rdma_item_count < part_ins->max_reg_cnt) {
			part_ins->rdma_item[rdma_tbl_count << 1] = adr;
			part_ins->rdma_item[(rdma_tbl_count << 1) + 1] = val;
			part_ins->rdma_item_count++;
		} else {
			pr_info("%s(%s)(%d, %x, %x ,%d) buf overflow, tbl_index = %d rdma_item_count=%d\n",
				__func__, current->comm, rdma_watchdog_count[handle],
				handle, adr, val, tbl_index,
				part_ins->rdma_item_count);
			for (i = 0; i < part_ins->rdma_item_count; i++)
				WRITE_VCBUS_REG(part_ins->rdma_item[i << 1],
					part_ins->rdma_item[(i << 1) + 1]);
			part_ins->rdma_item_count = 0;
			rdma_tbl_count = part_ins->rdma_item_count;
			part_ins->rdma_item[rdma_tbl_count << 1] = adr;
			part_ins->rdma_item[(rdma_tbl_count << 1) + 1] = val;
			part_ins->rdma_item_count++;
		}
	}

	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				pr_info("table_idx:%d (%s) handle %d(%s), %04x=0x%08x (%d), cur_val:0x%x\n",
					tbl_index,
					__func__,
					handle, current->comm, adr,
					val,
					rdma_tbl_count,
					READ_VCBUS_REG(adr));
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(rdma_part_write_reg);

inline int rdma_part_write_reg_simple(int tbl_index, int handle, u32 adr, u32 val)
{
	struct rdma_device_info *info = &rdma_info;
	int vpp_index = handle_trans_vpp(handle);
	struct rdma_partition_ins_s *part_ins = NULL;
	u32 rdma_tbl_count = 0;
	int i = 0, j = 0;

	if (vpp_index == -1) {
		pr_info("%s %d vpp_index error\n", __func__, __LINE__);
		return 0;
	}
	part_ins = &info->rdma_part_ins[vpp_index][tbl_index];
	rdma_tbl_count = part_ins->rdma_item_count;

	if (part_ins->flag) {
		if (part_ins->rdma_item_count < part_ins->max_reg_cnt) {
			part_ins->rdma_item[rdma_tbl_count << 1] = adr;
			part_ins->rdma_item[(rdma_tbl_count << 1) + 1] = val;
			part_ins->rdma_item_count++;
		} else {
			pr_info("%s(%s)(%d, %x, %x ,%d) buf overflow, tbl_index = %d rdma_item_count=%d\n",
				__func__, current->comm, rdma_watchdog_count[handle],
				handle, adr, val, tbl_index,
				part_ins->rdma_item_count);
			for (i = 0; i < part_ins->rdma_item_count; i++)
				WRITE_VCBUS_REG(part_ins->rdma_item[i << 1],
					part_ins->rdma_item[(i << 1) + 1]);
			part_ins->rdma_item_count = 0;
			rdma_tbl_count = part_ins->rdma_item_count;
			part_ins->rdma_item[rdma_tbl_count << 1] = adr;
			part_ins->rdma_item[(rdma_tbl_count << 1) + 1] = val;
			part_ins->rdma_item_count++;
		}
	}

	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				pr_info("table_idx:%d (%s) handle %d(%s), %04x=0x%08x (%d), cur_val:0x%x\n",
					tbl_index,
					__func__,
					handle, current->comm, adr,
					val,
					rdma_tbl_count,
					READ_VCBUS_REG(adr));
			}
		}
	}
	return 0;
}
EXPORT_SYMBOL(rdma_part_write_reg_simple);

int rdma_part_write_reg_bits(int tbl_index, int handle, u32 adr, u32 val, u32 start, u32 len)
{
	int i, j = 0;
	u32 *write_table;
	int match_oth = 0;
#ifdef DEBUG_RDMA_OPTIMIZE
	u32 oth_val = 0;
#endif
	int match = 0;
	int read_from = 0;
	int vpp_index = handle_trans_vpp(handle);

	if (vpp_index == -1) {
		pr_info("%s %d vpp_index error\n", __func__, __LINE__);
		return 0;
	}
	for (i = 0; i < video_reg.conflict_reg_count; i++) {
		if (video_reg.conflict_reg_table[i] != 0 &&
			adr == video_reg.conflict_reg_table[i])
			return rdma_write_reg_bits(handle, adr, val, start, len);
	}
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	struct rdma_partition_ins_s *part_ins = &info->rdma_part_ins[vpp_index][tbl_index];
	u32 read_val = READ_VCBUS_REG(adr);
	u32 write_val;
#ifdef DEBUG_RDMA_OPTIMIZE
	if (ins->rdma_table_size == 0)
		return -1;

	if (rdma_check_conflict(handle, adr, &oth_val)) {
		match_oth = 1;
		read_val = oth_val;
		read_from = 3;
	}
#endif
	/*
	 *only video use partition table can use this debug function
	 *to trace video reg write by pq/db
	 */
	if ((debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT) &&
		info->rdma_part_ins[vpp_index][VIDEO_PARTITION_TABLE].flag) {
		for (j = 0; j < video_reg.reg_count; j++) {
			if (adr == video_reg.reg_table[j] &&
				tbl_index != VIDEO_PARTITION_TABLE) {
				pr_info("ERROR: part table index:%d video reg: 0x%x\n",
						tbl_index, adr);
				if (debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT_STACK)
					dump_stack();
			}
		}
	}

	if (part_ins->reg_range_check &&
		(adr > part_ins->check_end_addr ||
		adr < part_ins->check_start_addr)) {
		pr_info("part index:%d write reg: 0x%x out of range:0x%x - 0x%x",
			tbl_index, adr, part_ins->check_start_addr,
			part_ins->check_end_addr);
	}
	for (i = part_ins->rdma_item_count - 1; i >= 0 ; i--) {
		if (part_ins->rdma_item[(i << 1)] == adr) {
			match = 1;
			//if (!match_oth) {
				read_val = part_ins->rdma_item[(i << 1) + 1];
				read_from = 1;
			//}
			break;
		}
	}
	/* changed to read from rdma_table_adr to mirror for optimize */
	if (!match) {
		write_table = ins->rdma_table_mirror;
		for (i = (ins->rdma_write_count - 1);
			i >= 0; i--) {
			if (write_table[i << 1] == adr) {
				if (!match_oth) {
					read_val =
						write_table[(i << 1) + 1];
					read_from = 2;
				}
				break;
			}
		}
	}
	write_val = (read_val & ~(((1L << (len)) - 1) << (start))) |
			((unsigned int)(val) << (start));
#ifdef DEBUG_RDMA_OPTIMIZE
	if (match_oth)
		rdma_update_conflict(adr, write_val);
#endif
	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				if (read_from == 3)
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from conflict table(%d %d %d), cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						ins->rdma_write_count,
						match,
						match ? i : ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 2)
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from write table(%d %d %d), cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						ins->rdma_write_count,
						match,
						match ? i : ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 1)
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from part table:%d (%d %d %d), cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						tbl_index,
						part_ins->rdma_item_count,
						match,
						match ? i : part_ins->rdma_item_count,
						READ_VCBUS_REG(adr));
				else
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from real reg, cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						READ_VCBUS_REG(adr));
			}
		}
	}
	if (match) {
		if (debug_flag & 1 ||
			(rdma_trace_enable &&
			rdma_trace_channel == handle))
			pr_info("rdma_part_write_bits(%d)(%s) %d(%x)<=%x\n",
				handle, current->comm, part_ins->rdma_item_count,
				adr, val);
		part_ins->rdma_item[(i << 1) + 1] = write_val;
		return 0;
	}

	rdma_part_write_reg(tbl_index, handle, adr, write_val);
	return 0;
}
EXPORT_SYMBOL(rdma_part_write_reg_bits);

int rdma_write_reg_bits(int handle, u32 adr, u32 val, u32 start, u32 len)
{
	int i, j = 0;
	u32 *write_table;
	int match = 0;
	int match_oth = 0, match_mirror = 0, match_lowlatency = 0;
	int read_from = 0;
	bool part_conflict_check = true;
	int vpp_index = handle_trans_vpp(handle);

	if (vpp_index == -1)
		part_conflict_check = false;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 read_val = READ_VCBUS_REG(adr);
	u32 oth_val = 0;
	u32 write_val;

	if (ins->rdma_table_size == 0)
		return -1;
	/*
	 *only video use partition table can use this debug function
	 *to trace video reg write by pq/db
	 */
	if ((debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT) && part_conflict_check &&
		info->rdma_part_ins[vpp_index][VIDEO_PARTITION_TABLE].flag) {
		for (j = 0; j < video_reg.reg_count; j++) {
			if (adr == video_reg.reg_table[j]) {
				pr_info("ERROR: video reg: 0x%x be write by other module\n", adr);
				if (debug_flag & DEBUG_RDMA_VIDEO_REG_CONFLICT_STACK)
					dump_stack();
			}
		}
	}
	if (rdma_check_conflict(handle, adr, &oth_val)) {
		match_oth = 1;
		read_val = oth_val;
		read_from = 3;
	}

	for (i = (ins->rdma_item_count - 1); i >= 0; i--) {
		if (ins->reg_buf[i << 1] == adr) {
			match = 1;
			if (!match_oth) {
				read_val = ins->reg_buf[(i << 1) + 1];
				read_from = 1;
			}
			break;
		}
	}
	/* changed to read from rdma_table_adr to mirror for optimize */
	if (!match) {
		write_table = ins->rdma_table_mirror;
		for (i = (ins->rdma_write_count - 1);
			i >= 0; i--) {
			if (write_table[i << 1] == adr) {
				if (!match_oth) {
					read_val =
						write_table[(i << 1) + 1];
					match_mirror = 1;
					read_from = 2;
				}
				break;
			}
		}
	}
	if (!match_mirror && !match) {
		/* changed to read from rdma_table_adr to lowlatency channel for optimize */
		if (handle != lowlatency_reg.rdma_handle &&
			(lowlatency_reg.rdma_handle > 0 &&
			lowlatency_reg.rdma_handle < rdma_meson_dev.channel_num)) {
			struct rdma_instance_s *ins_lowlatency =
				&info->rdma_ins[lowlatency_reg.rdma_handle];

			write_table = ins_lowlatency->rdma_table_mirror;
			for (i = (ins_lowlatency->rdma_write_count - 1);
				i >= 0; i--) {
				if (write_table[i << 1] == adr) {
					read_val =
						write_table[(i << 1) + 1];
					match_lowlatency = 1;
					read_from = 4;
					if (rdma_trace_enable)
						pr_info("(%s) handle %d, %04x=0x%08x from lowlatency table(%d %d), cur_val:0x%x\n",
							__func__,
							lowlatency_reg.rdma_handle, adr,
							read_val,
							ins->rdma_write_count,
							i,
							READ_VCBUS_REG(adr));
					break;
				}
			}
		}
	}
	write_val = (read_val & ~(((1L << (len)) - 1) << (start))) |
			((unsigned int)(val) << (start));
	if (match_oth)
		rdma_update_conflict(adr, write_val);
	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				if (read_from == 3)
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from conflict table(%d %d %d), cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						ins->rdma_write_count,
						match,
						match ? i : ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 2)
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from write table(%d %d %d), cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						ins->rdma_write_count,
						match,
						match ? i : ins->rdma_write_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 1)
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from item table(%d %d %d), cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						ins->rdma_item_count,
						match,
						match ? i : ins->rdma_item_count,
						READ_VCBUS_REG(adr));
				else if (read_from == 4)
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from lowlatency table(%d %d), cur_val:0x%x\n",
						__func__,
						lowlatency_reg.rdma_handle, current->comm, adr,
						read_val,
						write_val,
						ins->rdma_write_count,
						i,
						READ_VCBUS_REG(adr));
				else
					pr_info("(%s) handle %d(%s), %04x=0x%08x->0x%08x from real reg, cur_val:0x%x\n",
						__func__,
						handle, current->comm, adr,
						read_val,
						write_val,
						READ_VCBUS_REG(adr));
			}
		}
	}
	if (match) {
		if (debug_flag & 1 ||
			(rdma_trace_enable &&
			rdma_trace_channel == handle))
			pr_info("rdma_write_bits(%d)(%s) %d(%x)<=%x\n",
				handle, current->comm, ins->rdma_item_count,
				adr, write_val);
		ins->reg_buf[(i << 1) + 1] = write_val;
		return 0;
	}

	rdma_write_reg(handle, adr, write_val);
	return 0;
}
EXPORT_SYMBOL(rdma_write_reg_bits);

s32 rdma_add_read_reg(int handle, struct reg_handle *reg_hnd)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;

	if (reg_hnd && handle > 0 && handle < RDMA_NUM) {
		ins = &info->rdma_ins[handle];
		if (((ins->rdma_item_count + 1) << 1) <
			(ins->rdma_table_size / sizeof(u32))) {
			ins->reg_buf[ins->rdma_item_count] = reg_hnd->reg_addr;
			reg_hnd->offset = ins->rdma_item_count;
			ins->rdma_item_count++;
			list_add_tail(&reg_hnd->list, &ins->reg_handle.list);
			return 0;
		}
		pr_err("%s: out of bound\n", __func__);
		return -1;
	}
	pr_err("%s: handle is error\n", __func__);
	return -1;
}
EXPORT_SYMBOL(rdma_add_read_reg);

int rdma_update_reg_buf(int handle, u32 *rdma_item, u32 count)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 adr, val;
	int i;

	if (ins->rdma_table_size == 0)
		return -1;

	if (!rdma_item || !count) {
		pr_info("handle(%d) invalid parameters!\n", handle);
		return -1;
	}

	for (i = 0; i < count; i++) {
		adr = rdma_item[i << 1];
		val = rdma_item[(i << 1) + 1];
		if (rdma_check_conflict(handle, adr, NULL))
			rdma_update_conflict(adr, val);
	}

	if (((count << 1) + 1) < (ins->rdma_table_size / sizeof(u32))) {
		memcpy(ins->reg_buf, rdma_item, count * 2 * sizeof(u32));
		ins->rdma_item_count = count;
		if (debug_flag & 2) {
			pr_info("%s: handle=%d rdma_item_count=%d\r\n",
				__func__, handle, ins->rdma_item_count);
		}
	} else {
		pr_info("handle%d update reg buf count=%d, overflow!\n", handle, count);
	}

	return 0;
}
EXPORT_SYMBOL(rdma_update_reg_buf);

s32 rdma_remove_read_reg(int handle, struct reg_handle *reg_hnd, u32 count)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;
	u32 i;
	struct list_head *tmp_list;
	struct reg_handle *tmp_hnd;

	if (reg_hnd && handle > 0 && handle < RDMA_NUM) {
		ins = &info->rdma_ins[handle];
		if (ins->rdma_item_count &&
		    (reg_hnd->offset + count) <= ins->rdma_item_count) {
			ins->rdma_item_count -= count;

			/* remove register(s) in reg_buf table */
			memmove(ins->reg_buf + reg_hnd->offset,
				ins->reg_buf + reg_hnd->offset + count,
				sizeof(*ins->reg_buf) *
				(ins->rdma_item_count - reg_hnd->offset));

			/* remove node(s) in read list */
			tmp_list = &reg_hnd->list;
			for (i = 0; i < count; i++) {
				tmp_hnd = list_entry(tmp_list,
				     struct reg_handle,
				     list);
				__list_del_entry(tmp_list);
				tmp_list = tmp_list->next;
			}
			for (i = 0; i < ins->rdma_item_count - reg_hnd->offset;
			     i++) {
				tmp_hnd = list_entry(tmp_list,
						     struct reg_handle,
						     list);
				tmp_hnd->offset -= count;
				tmp_list = tmp_list->next;
			}
			return 0;
		}
		if (debug_flag & 1)
			pr_err("%s: out of bound\n", __func__);
		return -1;
	}
	pr_err("%s: handle is error\n", __func__);
	return -1;
}
EXPORT_SYMBOL(rdma_remove_read_reg);

u32 *rdma_get_read_back_addr(int handle, struct reg_handle *reg_hnd)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;
	u32 *table;

	if (reg_hnd && handle > 0 && handle < RDMA_NUM) {
		ins = &info->rdma_ins[handle];
		table = ins->rdma_table_addr;
		if (debug_flag & 2)
			pr_info("%s, handle: %d, pre_count: %d\n",
				__func__, handle, ins->prev_read_count);

		return (table + ins->prev_read_count + reg_hnd->offset);
	}

	return NULL;
}
EXPORT_SYMBOL(rdma_get_read_back_addr);

int get_rdma_item_count(int handle)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;
	int rdma_item_count = 0;

	if (handle > 0 && handle < rdma_meson_dev.channel_num) {
		ins = &info->rdma_ins[handle];
		rdma_item_count = ins->rdma_item_count;
	}
	return rdma_item_count;
}
EXPORT_SYMBOL(get_rdma_item_count);

int rdma_mbuf_register(struct rdma_op_s *rdma_op, void *op_arg, int mbuf_num,
	int buf_step)
{
	int i, j;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	dma_addr_t dma_handle;
	u32 table_size;
	struct rdma_instance_s *ins;
	bool buf_alloc_fail = false;

	if (!support_mbuf)
		return -1;
	table_size = mbuf_num * buf_step;
	spin_lock_irqsave(&rdma_lock, flags);
	for (i = 1; i < rdma_meson_dev.channel_num; i++) {
		/* 0 is reserved for RDMA MANUAL */
		if (!info->rdma_ins[i].op &&
		    info->rdma_ins[i].used == 0) {
			info->rdma_ins[i].op = rdma_op;
			info->rdma_ins[i].irq_count = 0;
			//info->rdma_ins[i].rdma_config_count = 0;
			info->rdma_ins[i].rdma_empty_config_count = 0;
			break;
		}
	}
	spin_unlock_irqrestore(&rdma_lock, flags);

	if (i < rdma_meson_dev.channel_num) {
		ins = &info->rdma_ins[i];

		info->rdma_ins[i].not_process = 0;
		info->rdma_ins[i].op_arg = op_arg;
		if (info->rdma_ins[i].rdma_table_size == 0) {
			info->rdma_ins[i].rdma_table_addr =
				dma_alloc_coherent
				(&info->rdma_dev->dev, table_size,
				&dma_handle, GFP_KERNEL);
			info->rdma_ins[i].rdma_table_phy_addr =
				(ulong)(dma_handle);
			pr_info("%s, rdma_table_addr %lx phy: %lx table_size= %x\n",
				__func__,
				(unsigned long)
				info->rdma_ins[i].rdma_table_addr,
				info->rdma_ins[i].rdma_table_phy_addr,
				table_size);

			for (j = 0; j < mbuf_num; j++) {
				info->rdma_ins[i].reg_mbuf[j] =
					kmalloc(buf_step, GFP_KERNEL);
				info->rdma_ins[i].rdma_table_mbuf_mirror[j] =
					kmalloc(buf_step, GFP_KERNEL);
				info->rdma_ins[i].rdma_mbuf_item_count[j] = 0;
				info->rdma_ins[i].rdma_mbuf_write_count[j] = 0;
				//info->rdma_ins[i].prev_read_count = 0;
				pr_info("%s, j=%d, reg_mbuf %lx, rdma_table_mbuf_mirror %lx\n",
					__func__, j,
					(unsigned long)info->rdma_ins[i].reg_mbuf[j],
					(unsigned long)info->rdma_ins[i].rdma_table_mbuf_mirror[j]);
			}
			info->rdma_ins[i].rdma_table_size = table_size;
			info->rdma_ins[i].mbuf_num = mbuf_num;
			info->rdma_ins[i].mbuf_step = buf_step;
		}

		//for mbuf hw setting
		WRITE_VCBUS_REG_BITS
			(info->rdma_ins[i].rdma_mbuf_regadr->rdma_buf_ctrl,
			 (buf_step & 0xffff), 16, 16);

		WRITE_VCBUS_REG_BITS
			(info->rdma_ins[i].rdma_mbuf_regadr->rdma_buf_ctrl,
			 mbuf_num, 0, 5);

		WRITE_VCBUS_REG_BITS
			(info->rdma_ins[i].rdma_mbuf_regadr->rdma_buf_cfg_en,
			 1, 0, 1);

		if (!info->rdma_ins[i].rdma_table_addr)
			buf_alloc_fail = true;
		for (j = 0; j < mbuf_num; j++) {
			if (!info->rdma_ins[i].reg_mbuf[j] ||
				!info->rdma_ins[i].rdma_table_mbuf_mirror[j]) {
				buf_alloc_fail = true;
				break;
			}
		}

		if (buf_alloc_fail) {
			if (!info->rdma_ins[i].keep_buf) {
				for (j = 0; j < mbuf_num; j++) {
					kfree(info->rdma_ins[i].reg_mbuf[j]);
					info->rdma_ins[i].reg_mbuf[j] = NULL;
					kfree(info->rdma_ins[i].rdma_table_mbuf_mirror[j]);
					info->rdma_ins[i].rdma_table_mbuf_mirror[j] = NULL;
				}
			}
			if (info->rdma_ins[i].rdma_table_addr) {
				dma_free_coherent
				(&info->rdma_dev->dev,
				 table_size,
				 info->rdma_ins[i].rdma_table_addr,
				 (dma_addr_t)
				 info->rdma_ins[i].rdma_table_phy_addr);
				 info->rdma_ins[i].rdma_table_addr = NULL;
			}
			info->rdma_ins[i].rdma_table_size  = 0;
			info->rdma_ins[i].op = NULL;
			i = -1;
			pr_info("%s: memory allocate fail\n",
				__func__);
		} else {
			if (support_64bit_addr) {
#ifdef CONFIG_ARM64
				WRITE_VCBUS_REG
				(ins->rdma_regadr->rdma_ahb_start_addr,
				ins->rdma_table_phy_addr & 0xffffffff);
				WRITE_VCBUS_REG
				(ins->rdma_regadr->rdma_ahb_start_addr_msb,
				(ins->rdma_table_phy_addr >> 32) & 0xffffffff);
#else
				WRITE_VCBUS_REG
				(ins->rdma_regadr->rdma_ahb_start_addr,
				ins->rdma_table_phy_addr & 0xffffffff);
				WRITE_VCBUS_REG
				(ins->rdma_regadr->rdma_ahb_start_addr_msb,
				0);
#endif
			} else {
				WRITE_VCBUS_REG
				(ins->rdma_regadr->rdma_ahb_start_addr,
				ins->rdma_table_phy_addr & 0xffffffff);
			}

			pr_info("%s success, handle %d mbuf_num:%d, buf_step: %d, table_size %d\n",
				__func__, i, mbuf_num, buf_step, table_size);
		}
		return i;
	}
	return -1;
}

void rdma_mbuf_unregister(int handle)
{
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	int i = handle;
	int j;

	if (!support_mbuf)
		return;
	pr_info("%s(%d)\r\n", __func__, i);

	if (i > 0 && i < rdma_meson_dev.channel_num && info->rdma_ins[i].op) {
		//for mbuf hw setting
		WRITE_VCBUS_REG_BITS
			(info->rdma_ins[i].rdma_mbuf_regadr->rdma_buf_cfg_en,
			 0, 0, 1);
		WRITE_VCBUS_REG_BITS
		(info->rdma_ins[i].rdma_regadr->trigger_mask_reg,
		0,
		 info->rdma_ins[i].rdma_regadr->trigger_mask_reg_bitpos,
		 rdma_meson_dev.trigger_mask_len);

		/*rdma_clear(i);*/
		info->rdma_ins[i].op_arg = NULL;
		if (!info->rdma_ins[i].keep_buf) {
			for (j = 0; j < info->rdma_ins[i].mbuf_num; j++) {
				kfree(info->rdma_ins[i].reg_mbuf[j]);
				info->rdma_ins[i].reg_mbuf[j] = NULL;
				kfree(info->rdma_ins[i].rdma_table_mbuf_mirror[j]);
				info->rdma_ins[i].rdma_table_mbuf_mirror[j] = NULL;
			}
		}
		if (info->rdma_ins[i].rdma_table_addr) {
			dma_free_coherent
			(&info->rdma_dev->dev,
			 info->rdma_ins[i].rdma_table_size,
			 info->rdma_ins[i].rdma_table_addr,
			 (dma_addr_t)
			 info->rdma_ins[i].rdma_table_phy_addr);
			 info->rdma_ins[i].rdma_table_addr = NULL;
		}
		info->rdma_ins[i].rdma_table_size = 0;
		info->rdma_ins[i].rdma_table_size = 0;
		info->rdma_ins[i].mbuf_num = 0;
		info->rdma_ins[i].mbuf_step = 0;
		spin_lock_irqsave(&rdma_lock, flags);
		info->rdma_ins[i].op = NULL;
		spin_unlock_irqrestore(&rdma_lock, flags);
	}
}

int rdma_mbuf_write_reg(int handle, int mbuf_index, u32 adr, u32 val)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	int j = 0;

	if (ins->rdma_table_size == 0 || !support_mbuf)
		return -1;

	if (adr == 0) {
		pr_info("rdma_write(%d)(%s) mbuf_index(%d) write zero addr = %x, count:%d\n",
			handle, current->comm, mbuf_index, val,
			ins->rdma_mbuf_item_count[mbuf_index]);
		dump_stack();
	}
	if (debug_flag & 1 ||
		(rdma_trace_enable &&
		rdma_trace_channel == handle))
		pr_info("rdma_write(%d)(%s) mbuf_index(%d) %d(%x)<=%x\n",
			handle, current->comm, mbuf_index,
			ins->rdma_mbuf_item_count[mbuf_index], adr, val);
	//if (rdma_check_conflict(handle, adr, NULL))
	//	rdma_update_conflict(adr, val);

	if (((ins->rdma_mbuf_item_count[mbuf_index] << 1) + 1) <
		(ins->mbuf_step / sizeof(u32))) {
		ins->reg_mbuf[mbuf_index][ins->rdma_mbuf_item_count[mbuf_index] << 1] = adr;
		ins->reg_mbuf[mbuf_index][(ins->rdma_mbuf_item_count[mbuf_index] << 1) + 1] = val;
		ins->rdma_mbuf_item_count[mbuf_index]++;
	} else {
		int i;

		pr_info("%s(%s) mbuf_index(%d) (%d, %x, %x ,%d) buf overflow, ins->rdma_item_count=%d\n",
			__func__, current->comm, mbuf_index, rdma_watchdog_count[handle],
			handle, adr, val,
			ins->rdma_mbuf_item_count[mbuf_index]);
		for (i = 0; i < ins->rdma_mbuf_item_count[mbuf_index]; i++)
			WRITE_VCBUS_REG(ins->reg_mbuf[mbuf_index][i << 1],
					ins->reg_mbuf[mbuf_index][(i << 1) + 1]);
		ins->rdma_mbuf_item_count[mbuf_index] = 0;
		ins->rdma_mbuf_write_count[mbuf_index] = 0;
		ins->reg_mbuf[mbuf_index][ins->rdma_mbuf_item_count[mbuf_index] << 1] = adr;
		ins->reg_mbuf[mbuf_index][(ins->rdma_mbuf_item_count[mbuf_index] << 1) + 1] = val;
		ins->rdma_mbuf_item_count[mbuf_index]++;
	}
	if (rdma_trace_enable) {
		for (j = 0; j < rdma_trace_num; j++) {
			if (adr == rdma_trace_reg[j]) {
				pr_info("(%s) handle %d(%s), mbuf_index(%d) %04x=0x%08x (%d), cur_val:0x%x\n",
					__func__,
					handle, current->comm, mbuf_index, adr,
					val,
					ins->rdma_mbuf_item_count[mbuf_index],
					READ_VCBUS_REG(adr));
			}
		}
	}
	return 0;
}

int rdma_mbuf_write_reg_bits(int handle, int mbuf_index, u32 adr, u32 val, u32 start, u32 len)
{
	int i, j = 0;
	u32 *write_table;
	int match = 0;
	int match_oth = 0;
	int read_from = 0;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 read_val = READ_VCBUS_REG(adr);
	//u32 oth_val = 0;
	u32 write_val;

	if (ins->rdma_table_size == 0 || !support_mbuf)
		return -1;
#ifdef check_later
	if (rdma_check_conflict(handle, adr, &oth_val)) {
		match_oth = 1;
		read_val = oth_val;
		read_from = 3;
	}
#endif
	for (i = (ins->rdma_mbuf_item_count[mbuf_index] - 1); i >= 0; i--) {
		if (ins->reg_mbuf[mbuf_index][i << 1] == adr) {
			match = 1;
			if (!match_oth) {
				read_val = ins->reg_mbuf[mbuf_index][(i << 1) + 1];
				read_from = 1;
			}
			break;
		}
	}
	/* changed to read from rdma_table_adr to mirror for optimize */
	if (!match) {
		write_table = ins->rdma_table_mbuf_mirror[mbuf_index];
		for (i = (ins->rdma_mbuf_write_count[mbuf_index] - 1);
			i >= 0; i--) {
			if (write_table[i << 1] == adr) {
				if (!match_oth) {
					read_val =
						write_table[(i << 1) + 1];
					read_from = 2;
				}
				break;
			}
		}
	}
	write_val = (read_val & ~(((1L << (len)) - 1) << (start))) |
			((unsigned int)(val) << (start));
	//if (match_oth)
	//	rdma_update_conflict(adr, write_val);
	for (j = 0; j < rdma_trace_num; j++) {
		if (adr == rdma_trace_reg[j]) {
			if (read_from == 3)
				pr_info("(%s) handle %d(%s), mbuf_index(%d) %04x=0x%08x->0x%08x from conflict table(%d %d %d), cur_val:0x%x\n",
					__func__,
					handle, current->comm, mbuf_index, adr,
					read_val,
					write_val,
					ins->rdma_mbuf_write_count[mbuf_index],
					match,
					match ? i : ins->rdma_mbuf_write_count[mbuf_index],
					READ_VCBUS_REG(adr));
			else if (read_from == 2)
				pr_info("(%s) handle %d(%s), mbuf_index(%d) %04x=0x%08x->0x%08x from write table(%d %d %d %d), cur_val:0x%x\n",
					__func__,
					handle, current->comm, mbuf_index, adr,
					read_val,
					write_val,
					ins->rdma_mbuf_write_count[mbuf_index],
					ins->rdma_mbuf_item_count[mbuf_index],
					match,
					i,
					READ_VCBUS_REG(adr));
			else if (read_from == 1)
				pr_info("(%s) handle %d(%s), mbuf_index(%d) %04x=0x%08x->0x%08x from item table(%d %d %d), cur_val:0x%x\n",
					__func__,
					handle, current->comm, mbuf_index, adr,
					read_val,
					write_val,
					ins->rdma_mbuf_item_count[mbuf_index],
					match,
					match ? i : ins->rdma_mbuf_item_count[mbuf_index],
					READ_VCBUS_REG(adr));
			else
				pr_info("(%s) handle %d(%s), mbuf_index(%d) %04x=0x%08x->0x%08x from real reg, cur_val:0x%x\n",
					__func__,
					handle, current->comm, mbuf_index, adr,
					read_val,
					write_val,
					READ_VCBUS_REG(adr));
		}
	}
	if (match) {
		if (debug_flag & 1 ||
			(rdma_trace_enable &&
			rdma_trace_channel == handle))
			pr_info("rdma_write_bits(%d)(%s) mbuf_index(%d) %d(%x)<=%x\n",
				handle, current->comm, mbuf_index,
				ins->rdma_mbuf_item_count[mbuf_index],
				adr, val);
		ins->reg_mbuf[mbuf_index][(i << 1) + 1] = write_val;
		return 0;
	}

	rdma_mbuf_write_reg(handle, mbuf_index, adr, write_val);
	return 0;
}

static u32 sft_calc(int handle, int mbuf_index, u32 count)
{
	u32 mbuf_length, sft_bit_count = 0, sft_bit;
	u32 append_size;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];

	if (count <= 0xff) {
		sft_bit_count = 0;
	//0xff << 1 = 0x1fe
	} else if (count > 0xff && count <= 0x1fe) {
		sft_bit_count = 1;
	//0xff << 2 = 0x3fc
	} else if (count > 0x1fe && count <= 0x3fc) {
		sft_bit_count = 2;
	//0xff << 3 = 0x7f8
	} else if (count > 0x3fc && count <= 0x7f8) {
		sft_bit_count = 3;
	//0xff << 4 = 0xff0
	} else if (count > 0x7f8 && count <= 0xff0) {
		sft_bit_count = 4;
	//0xff << 5 = 0x1fe0
	} else if (count > 0xff0 && count <= 0x1fe0) {
		sft_bit_count = 5;
	} else {
		sft_bit_count = 5;
		pr_info("%s mbuf_index:%d, count:%d is too bigger(max 0x1fe0)\n",
			__func__,
			mbuf_index, count);
	}
	//sft_bit_count for count, sft_bit for length
	sft_bit = sft_bit_count + 3;
	//for align: (count + (1 << sft_bit_count) - 1), calc need mbuf_length for count
	mbuf_length = (count + (1 << sft_bit_count) - 1) >> sft_bit_count;
	//calc need append_size
	append_size = (mbuf_length << sft_bit) - (count << 3);
	ins->mbuf_count[mbuf_index] = count;
	ins->mbuf_length[mbuf_index] = mbuf_length;
	ins->sft_bit[mbuf_index] = sft_bit;
	ins->append_size[mbuf_index] = append_size;
	if (debug_flag & DEBUG_RDMA_CONFIG)
		pr_info("%s: mbuf_index:%d,count:0x%x,sft_bit_count:%d, sft_bit:%d, mbuf_length:0x%x: append_size=0x%x\n",
			__func__,
			mbuf_index,
			count, sft_bit_count, sft_bit,
			mbuf_length, append_size);

	return sft_bit;
}

static void set_sft_length(int handle, int mbuf_index, u32 count, u32 sft_bit)
{
	u32 sft_bit_count, mbuf_length, append_length;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];

	sft_bit_count = sft_bit - 3;
	mbuf_length = (count + (1 << sft_bit_count) - 1) >> sft_bit_count;
	append_length = (mbuf_length << sft_bit) - (count << 3);
	ins->mbuf_length[mbuf_index] = mbuf_length;
	ins->append_size[mbuf_index] = append_length;

	if (debug_flag & DEBUG_RDMA_CONFIG)
		pr_info("%s: mbuf_index=%d, mbuf_length=0x%x, append_length=0x%x, append_length=0x%x\n",
			__func__,
			mbuf_index,
			mbuf_length,
			append_length,
			append_length);
}

int rdma_mbuf_config(int handle, int mbuf_index, u64 trigger_type)
{
	int ret = 0;
	unsigned long flags;
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	bool rdma_read = false;
	u32 cur_enc_line_start = 0, cur_enc_line_end = 0;
	u32 length_block = 0, length_addr = 0, length_offset = 0, length_byte = 0;
	int i;

	if (!support_mbuf)
		return -1;

	if (handle <= 0 || handle >= rdma_meson_dev.channel_num ||
		mbuf_index < 0 || mbuf_index >= ins->mbuf_num) {
		pr_info
		("%s error, (handle == %d) (mbuf_index = %d) not allowed\n",
		 __func__, handle, mbuf_index);
		return -1;
	}

	if (!enable[handle])
		return 0;

	spin_lock_irqsave(&rdma_lock, flags);
	if (!ins->op) {
		spin_unlock_irqrestore(&rdma_lock, flags);

		pr_info("%s: handle (%d) (mbuf_index = %d) not register\n",
			__func__, handle, mbuf_index);
		return -1;
	}

	cur_enc_line_start = get_cur_enc_line();
	if (trigger_type & RDMA_READ_MASK)
		rdma_read = true;

	if (trigger_type == 0) {
		ins->trigger_type = trigger_type;
		//clear rdma source will stop rdma
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->trigger_mask_reg,
		 0,
		 ins->rdma_regadr->trigger_mask_reg_bitpos,
		 rdma_meson_dev.trigger_mask_len);
		if (debug_flag & 2) {
			pr_info("%s: handle=%d trigger_type %llx mbuf_index(%d): %d\r\n",
				__func__,
				handle,
				trigger_type,
				mbuf_index,
				ins->rdma_mbuf_item_count[mbuf_index]);
		}
		ins->rdma_mbuf_write_count[mbuf_index] = 0;
		ins->prev_read_count = 0;
		ins->rdma_empty_config_count++;
		ret = 0;
	} else if (trigger_type > 0 &&
		trigger_type <= BIT(rdma_meson_dev.trigger_mask_len - 1)) {
		ins->trigger_type = trigger_type;
		ins->rdma_mbuf_write_count[mbuf_index] =
			ins->rdma_mbuf_item_count[mbuf_index];
		/* interrupt input trigger RDMA */
		if (debug_flag & 2 ||
			(rdma_trace_enable &&
			rdma_trace_channel == handle))
			pr_info("%s: handle=%d mbuf_index(%d) case 3 : %d:\r\n",
			__func__, handle, mbuf_index,
			ins->rdma_mbuf_item_count[mbuf_index]);

		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->addr_inc_reg,
		 0,
		 ins->rdma_regadr->addr_inc_reg_bitpos,
		 1);

		//for trigger_type setting
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->trigger_mask_reg,
		 ins->trigger_type,
		 ins->rdma_regadr->trigger_mask_reg_bitpos,
		 rdma_meson_dev.trigger_mask_len);

		//cp reg_mbuf to rdma phy buf
		i = mbuf_index;
		memcpy((void *)((unsigned long)ins->rdma_table_addr +
			ins->mbuf_step * i),
			ins->reg_mbuf[i],
			ins->rdma_mbuf_item_count[i] *
			(rdma_read ? 1 : 2) * sizeof(u32));
		/* cp to mirror buf */
		memcpy(ins->rdma_table_mbuf_mirror[i], ins->reg_mbuf[i],
			ins->rdma_mbuf_item_count[i] *
			(rdma_read ? 1 : 2) * sizeof(u32));
		//ins->prev_read_count = ins->rdma_item_count;

		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->rw_flag_reg,
		rdma_read ? 0 : 1,
		ins->rdma_regadr->rw_flag_reg_bitpos,
		1);

		//every block have 4 mbuf length control
		length_block = mbuf_index / 4;
		switch (length_block) {
		case 0:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_0;
			break;
		case 1:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_1;
			break;
		case 2:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_2;
			break;
		case 3:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_3;
			break;
		}
		length_offset = (mbuf_index % 4) * 8;

		length_byte = ins->rdma_mbuf_item_count[mbuf_index] *
			(rdma_read ? 1 : 2) * sizeof(u32);
		sft_calc(handle, mbuf_index,
			ins->rdma_mbuf_item_count[mbuf_index]);
		//append reg info
		memcpy((void *)((unsigned long)ins->rdma_table_addr +
			ins->mbuf_step * i + length_byte),
			append_buf_info,
			ins->append_size[mbuf_index]);

		WRITE_VCBUS_REG_BITS(length_addr,
			ins->mbuf_length[mbuf_index], length_offset, 8);

		WRITE_VCBUS_REG_BITS(ins->rdma_mbuf_regadr->rdma_buf_ctrl,
			ins->sft_bit[mbuf_index], 8, 4);
		if (debug_flag & DEBUG_RDMA_CONFIG)
			pr_info("%s: (%d 0x%llx) mbuf_index(%d) length_addr 0x%x, length_byte 0x%x(0x%x), length_offset 0x%x, sft_bit:0x%x, append_size=%d\n",
				__func__, handle, trigger_type, mbuf_index,
				length_addr,
				ins->mbuf_length[mbuf_index],
				length_byte, length_offset,
				ins->sft_bit[mbuf_index],
				ins->append_size[mbuf_index]);

		if (debug_flag & DEBUG_RDMA_CONFIG)
			pr_info("%s: (%d 0x%llx) mbuf_index(%d) rdma_mbuf_item_count %d, rdma_mbuf_write_count %d\n",
				__func__, handle, trigger_type, mbuf_index,
				ins->rdma_mbuf_item_count[mbuf_index],
				ins->rdma_mbuf_write_count[mbuf_index]);
		ret = 1;
	} else {
		WRITE_VCBUS_REG_BITS
		(ins->rdma_regadr->trigger_mask_reg,
		0,
		ins->rdma_regadr->trigger_mask_reg_bitpos,
		rdma_meson_dev.trigger_mask_len);

		pr_info("%s, trigger_type(%llx) set fail\n",
			__func__, trigger_type);
		return -1;
	}
	/* don't reset rdma_item_count for read function */
	if (!rdma_read) {
		//after config,clear mbuf item count
		ins->rdma_mbuf_item_count[mbuf_index] = 0;
	}
	cur_enc_line_end = get_cur_enc_line();
	spin_unlock_irqrestore(&rdma_lock, flags);

	if (debug_flag & 2 ||
		debug_flag & DEBUG_RDMA_CONFIG ||
		(rdma_trace_enable &&
		rdma_trace_channel == handle))
		pr_info("%s: (%d 0x%llx) mbuf_index(%d) rdma_mbuf_item_count=%d, ret %d, enc line(%d, %d)\r\n",
			__func__, handle, trigger_type, mbuf_index,
			ins->rdma_mbuf_item_count[mbuf_index], ret,
			cur_enc_line_start, cur_enc_line_end);
	ins->rdma_config_count++;
	return ret;
}

int mbuf_get_current_index(int handle)
{
	struct rdma_device_info *info = &rdma_info;

	if (!support_mbuf)
		return -1;

	/* 0 is reserved for RDMA MANUAL,so need -1*/
	return READ_VCBUS_REG(info->rdma_ins[handle].rdma_mbuf_regadr->rdma_buf_ro);
}
EXPORT_SYMBOL(mbuf_get_current_index);

void rdma_mbuf_idx_clr(int handle)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];

	if (!support_mbuf)
		return;
	return WRITE_VCBUS_REG_BITS(ins->rdma_regadr->rdma_buf_idx_clr,
		1, ins->rdma_regadr->rdma_buf_idx_clr_bitpos, 1);
}

void rdma_mbuf_reconfig_length(int handle)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = &info->rdma_ins[handle];
	u32 mbuf_num = ins->mbuf_num;
	u32 sft_bit, sft_bit_max = 0;
	u32 length_block, length_addr, length_offset, length_byte;
	int i;

	for (i = 0; i < mbuf_num; i++) {
		sft_bit = sft_calc(handle, i, ins->mbuf_count[i]);
		if (sft_bit > sft_bit_max)
			sft_bit_max = sft_bit;
	}
	if (debug_flag & DEBUG_RDMA_CONFIG)
		pr_info("%s: sft_bit_max:%d\n",
			__func__, sft_bit_max);
	WRITE_VCBUS_REG_BITS(ins->rdma_mbuf_regadr->rdma_buf_ctrl,
		sft_bit_max, 8, 4);

	for (i = 0; i < mbuf_num; i++) {
		set_sft_length(handle, i, ins->mbuf_count[i], sft_bit_max);

		//append reg info
		length_byte = ins->mbuf_count[i] << 3;
		memcpy((void *)((unsigned long)ins->rdma_table_addr +
			ins->mbuf_step * i + length_byte),
			append_buf_info,
			ins->append_size[i]);
	//every block have 4 mbuf length control
		length_block = i / 4;
		switch (length_block) {
		case 0:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_0;
			break;
		case 1:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_1;
			break;
		case 2:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_2;
			break;
		case 3:
			length_addr = ins->rdma_mbuf_regadr->rdma_buf_length_3;
			break;
		}
		length_offset = (i % 4) * 8;
		WRITE_VCBUS_REG_BITS(length_addr, ins->mbuf_length[i], length_offset, 8);
	}
}

struct reg_handle *rdma_query_read_handle(int handle, u32 index)
{
	struct rdma_device_info *info = &rdma_info;
	struct rdma_instance_s *ins = NULL;
	int i;
	struct list_head *tmp_list;

	if (handle > 0 && handle < RDMA_NUM) {
		ins = &info->rdma_ins[handle];
		if (index < ins->rdma_item_count) {
			tmp_list = &ins->reg_handle.list;
			for (i = 0; i <= index; i++)
				tmp_list = tmp_list->next;

			return list_entry(tmp_list, struct reg_handle, list);
		}
		pr_err("%s: out of bound\n", __func__);
	}

	return NULL;
}
EXPORT_SYMBOL(rdma_query_read_handle);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct rdma_device_data_s rdma_meson = {
	.cpu_type = CPU_NORMAL,
	.rdma_ver = RDMA_VER_1,
	.trigger_mask_len = 8,
	.channel_num = 8
};

static struct rdma_device_data_s rdma_g12b_reva = {
	.cpu_type = CPU_G12B_REVA,
	.rdma_ver = RDMA_VER_1,
	.trigger_mask_len = 8,
	.channel_num = 8
};

static struct rdma_device_data_s rdma_g12b_revb = {
	.cpu_type = CPU_G12B_REVB,
	.rdma_ver = RDMA_VER_1,
	.trigger_mask_len = 8,
	.channel_num = 8
};

static struct rdma_device_data_s rdma_tl1 = {
	.cpu_type = CPU_TL1,
	.rdma_ver = RDMA_VER_2,
	.trigger_mask_len = 16,
	.channel_num = 8
};
#endif

static struct rdma_device_data_s rdma_sc2 = {
	.cpu_type = CPU_SC2,
	.rdma_ver = RDMA_VER_2,
	.trigger_mask_len = 16,
	.channel_num = 8
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct rdma_device_data_s rdma_t7 = {
	.cpu_type = CPU_T7,
	.rdma_ver = RDMA_VER_3,
	.trigger_mask_len = 24,
	.channel_num = 8
};

static struct rdma_device_data_s rdma_t3 = {
	.cpu_type = CPU_T7,
	.rdma_ver = RDMA_VER_4,
	.trigger_mask_len = 25,
	.channel_num = 8
};

static struct rdma_device_data_s rdma_t3x = {
	.cpu_type = CPU_T7,
	.rdma_ver = RDMA_VER_5,
	.trigger_mask_len = 25,
	.channel_num = 16
};

static struct rdma_device_data_s rdma_t6w = {
	.cpu_type = CPU_T6W,
	.rdma_ver = RDMA_VER_6,
	.trigger_mask_len = 32,
	.channel_num = 16
};
#endif

static const struct of_device_id rdma_dt_match[] = {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, meson, rdma",
		.data = &rdma_meson,
	},
	{
		.compatible = "amlogic, meson-g12b-reva, rdma",
		.data = &rdma_g12b_reva,
	},
	{
		.compatible = "amlogic, meson-g12b-revb, rdma",
		.data = &rdma_g12b_revb,
	},
	{
		.compatible = "amlogic, meson-tl1, rdma",
		.data = &rdma_tl1,
	},
#endif
	{
		.compatible = "amlogic, meson-sc2, rdma",
		.data = &rdma_sc2,
	},
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic, meson-t7, rdma",
		.data = &rdma_t7,
	},
	{
		.compatible = "amlogic, meson-t3, rdma",
		.data = &rdma_t3,
	},
	{
		.compatible = "amlogic, meson-t3x, rdma",
		.data = &rdma_t3x,
	},
	{
		.compatible = "amlogic, meson-t6w, rdma",
		.data = &rdma_t6w,
	},

#endif
	{},
};

u32 is_meson_g12b_revb(void)
{
	if (rdma_meson_dev.cpu_type == CPU_G12B_REVB)
		return 1;
	else
		return 0;
}

void vpp1_vsync_rdma_register(void)
{
	int handle;

	handle = rdma_register(get_rdma_ops(VSYNC_RDMA_VPP1),
			       NULL, rdma_table_size);
	set_rdma_handle(VSYNC_RDMA_VPP1, handle);
}
EXPORT_SYMBOL(vpp1_vsync_rdma_register);

void vpp2_vsync_rdma_register(void)
{
	int handle;

	handle = rdma_register(get_rdma_ops(VSYNC_RDMA_VPP2),
			       NULL, rdma_table_size);
	set_rdma_handle(VSYNC_RDMA_VPP2, handle);
}
EXPORT_SYMBOL(vpp2_vsync_rdma_register);

void pre_vsync_rdma_register(void)
{
	int handle;

	handle = rdma_register(get_rdma_ops(PRE_VSYNC_RDMA),
			       NULL, rdma_table_size);
	set_rdma_handle(PRE_VSYNC_RDMA, handle);
}
EXPORT_SYMBOL(pre_vsync_rdma_register);

void ex_vsync_rdma_register(void)
{
	int handle;

	handle = rdma_register(get_rdma_ops(EX_VSYNC_RDMA),
			       NULL, rdma_table_size);
	set_rdma_handle(EX_VSYNC_RDMA, handle);
}
EXPORT_SYMBOL(ex_vsync_rdma_register);

int rdma_start_addr(int handle)
{
	struct rdma_regadr_s *rdma_regadr;

	if (handle >= rdma_meson_dev.channel_num) {
		handle = 0;
		pr_err("%s, wrong handle:%d\n", __func__, handle);
	}
	rdma_regadr = rdma_info.rdma_ins[handle].rdma_regadr;

	return rdma_regadr->rdma_ahb_start_addr;
}
EXPORT_SYMBOL(rdma_start_addr);

int rdma_start_addr_msb(int handle)
{
	struct rdma_regadr_s *rdma_regadr;

	if (handle >= rdma_meson_dev.channel_num) {
		handle = 0;
		pr_err("%s, wrong handle:%d\n", __func__, handle);
	}
	rdma_regadr = rdma_info.rdma_ins[handle].rdma_regadr;

	return rdma_regadr->rdma_ahb_start_addr_msb;
}
EXPORT_SYMBOL(rdma_start_addr_msb);

int rdma_end_addr(int handle)
{
	struct rdma_regadr_s *rdma_regadr;

	if (handle >= rdma_meson_dev.channel_num) {
		handle = 0;
		pr_err("%s, wrong handle:%d\n", __func__, handle);
	}
	rdma_regadr = rdma_info.rdma_ins[handle].rdma_regadr;

	return rdma_regadr->rdma_ahb_end_addr;
}
EXPORT_SYMBOL(rdma_end_addr);

int rdma_end_addr_msb(int handle)
{
	struct rdma_regadr_s *rdma_regadr;

	if (handle >= rdma_meson_dev.channel_num) {
		handle = 0;
		pr_err("%s, wrong handle:%d\n", __func__, handle);
	}
	rdma_regadr = rdma_info.rdma_ins[handle].rdma_regadr;

	return rdma_regadr->rdma_ahb_end_addr_msb;
}
EXPORT_SYMBOL(rdma_end_addr_msb);

static ssize_t show_debug_flag(const struct class *class,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, 40, "%d\n", debug_flag);
}

static ssize_t store_debug_flag(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("debug_flag: %d->%d\n", debug_flag, res);
	debug_flag = res;

	return count;
}

static ssize_t show_rdma_watchdog(const struct class *class,
				  const struct class_attribute *attr,
				  char *buf)
{
	return snprintf(buf, 40, "%d\n", rdma_watchdog);
}

static ssize_t store_rdma_watchdog(const struct class *class,
				   const struct class_attribute *attr,
				   const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("rdma_watchdog: %d->%d\n", rdma_watchdog, res);
	rdma_watchdog = res;

	return count;
}

static ssize_t show_reset_count(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	return snprintf(buf, 40, "%d\n", reset_count);
}

static ssize_t store_reset_count(const struct class *class,
				 const struct class_attribute *attr,
				 const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("reset_count: %d->%d\n", reset_count, res);
	reset_count = res;

	return count;
}

static ssize_t show_video_all_reg(const struct class *class,
				const struct class_attribute *attr,
				char *buf)
{
	int i;
	ssize_t ret = 0;

	for (i = 0; i < video_reg.reg_count; i++)
		if (video_reg.reg_table[i] != 0)
			pr_info("video_reg index: %d adr = 0x%x\n",
				i, video_reg.reg_table[i]);
	for (i = 0; i < video_reg.conflict_reg_count; i++)
		if (video_reg.conflict_reg_table[i] != 0)
			pr_info("conflict_reg adr: %d adr = %x\n",
				i, video_reg.conflict_reg_table[i]);
	return ret;
}

static ssize_t show_ctrl_ahb_rd_burst_size(const struct class *class,
					   const struct class_attribute *attr,
					   char *buf)
{
	return snprintf(buf, 40, "%d\n", ctrl_ahb_rd_burst_size);
}


static ssize_t store_ctrl_ahb_rd_burst_size(const struct class *class,
					    const struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("ctrl_ahb_rd_burst_size: %d->%d\n",
		ctrl_ahb_rd_burst_size, res);
	ctrl_ahb_rd_burst_size = res;

	return count;
}

static ssize_t show_ctrl_ahb_wr_burst_size(const struct class *class,
					   const struct class_attribute *attr,
					   char *buf)
{
	return snprintf(buf, 40, "%d\n", ctrl_ahb_wr_burst_size);
}

static ssize_t store_ctrl_ahb_wr_burst_size(const struct class *class,
					    const struct class_attribute *attr,
					    const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("ctrl_ahb_wr_burst_size: %d->%d\n",
		ctrl_ahb_wr_burst_size, res);
	ctrl_ahb_wr_burst_size = res;

	return count;
}

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (!token)
		return 0;
	len = strlen(token);
	do {
		token = strsep(&params, " ");
		while (token && (isspace(*token) ||
				 !isgraph(*token)) && len) {
			token++;
			len--;
		}
		if (len == 0 || !token)
			break;
		ret = kstrtoint(token, 0, &res);
		if (ret < 0)
			break;
		len = strlen(token);
		*out++ = res;
		count++;
	} while ((token) && (count < para_num) && (len > 0));

	kfree(params_base);
	return count;
}

static ssize_t rdma_mgr_trace_enable_show(const struct class *cla,
					  const struct class_attribute *attr,
					  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x\n", rdma_trace_enable);
}

static ssize_t rdma_mgr_trace_enable_store(const struct class *cla,
					   const struct class_attribute *attr,
					   const char *buf, size_t count)
{
	int ret = 0;

	ret = kstrtoint(buf, 0, &rdma_trace_enable);
	if (ret < 0)
		return -EINVAL;
	return count;
}

static ssize_t rdma_mgr_trace_reg_show(const struct class *cla,
				       const struct class_attribute *attr, char *buf)
{
	int i;
	char reg_info[16];
	char *trace_info = NULL;

	trace_info = kmalloc(rdma_trace_num * 16 + 1, GFP_KERNEL);
	if (!trace_info)
		return 0;
	for (i = 0; i < rdma_trace_num; i++) {
		sprintf(reg_info, "0x%x", rdma_trace_reg[i]);
		strcat(trace_info, reg_info);
		strcat(trace_info, " ");
	}
	i = snprintf(buf, PAGE_SIZE, "%s\n", trace_info);
	kfree(trace_info);
	trace_info = NULL;
	return i;
}

static ssize_t rdma_mgr_trace_reg_store(const struct class *cla,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	int parsed[MAX_TRACE_NUM];
	int i = 0, num = 0;

	for (i	= 0; i < MAX_TRACE_NUM; i++)
		rdma_trace_reg[i] = 0;
	num = parse_para(buf, MAX_TRACE_NUM, parsed);
	if (num <= MAX_TRACE_NUM) {
		rdma_trace_num = num;
		for (i  = 0; i < num; i++) {
			rdma_trace_reg[i] = parsed[i];
			pr_info("trace reg:0x%x\n", rdma_trace_reg[i]);
		}
	}
	return count;
}

static ssize_t rdma_partition_enable_show(const struct class *cla,
				       const struct class_attribute *attr, char *buf)
{
	ssize_t ret = 0;
	int i, j;
	struct rdma_device_info *info = &rdma_info;

	for (i = 0; i < RDMA_VPP_MAX; i++) {
		pr_info("vpp index = %d\n", i);
		for (j = 0; j < PARTITION_TABLE_NUM; j++) {
			pr_info("rdma partition table index %d flag status: %d", j,
				info->rdma_part_ins[i][j].flag);
		}
	}
	return ret;
}

static ssize_t rdma_partition_enable_store(const struct class *cla,
					const struct class_attribute *attr,
					const char *buf, size_t count)
{
	int parsed[3];
	int num = 0;
	int i;
	int tbl_idx;
	int vpp_index;
	bool set_flag;
	struct rdma_device_info *info = &rdma_info;

	num = parse_para(buf, 3, parsed);

	if (num == 3) {
		vpp_index = parsed[0];
		tbl_idx = parsed[1];
		set_flag = parsed[2] ? true : false;
		if (vpp_index >= RDMA_VPP_MAX || tbl_idx > PARTITION_TABLE_NUM) {
			pr_info("para1(vpp_index)%d  need < %d para2(table_index)%d need < %d",
				vpp_index, RDMA_VPP_MAX, tbl_idx, PARTITION_TABLE_NUM);
		} else {
			if (tbl_idx != PARTITION_TABLE_NUM) {
				info->rdma_part_ins[vpp_index][tbl_idx].flag = set_flag;
			} else {
				for (i = 0; i < PARTITION_TABLE_NUM; i++)
					info->rdma_part_ins[vpp_index][i].flag = set_flag;
			}
		}
	}
	return count;
}

static ssize_t show_ex_vsync_rdma(const struct class *class,
			       const struct class_attribute *attr,
			       char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
			get_ex_vsync_rdma_enable());
}

static ssize_t store_ex_vsync_rdma(const struct class *class,
				const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	pr_info("ex_vsync_rdma_enable: %d->%d\n", get_ex_vsync_rdma_enable(), res);
	set_ex_vsync_rdma_enable(res);

	return count;
}

static ssize_t rdma_irq_count_show(const struct class *cla,
				       const struct class_attribute *attr, char *buf)
{
	int i, len = 0;
	struct rdma_device_info *info = &rdma_info;

	len += snprintf(buf + len, PAGE_SIZE - len,
		"rdma_isr_count:%d\n", rdma_isr_count);

	for (i = 1; i < rdma_meson_dev.channel_num; i++) {
		if (info->rdma_ins[i].rdma_table_size) {
			len += snprintf(buf + len, PAGE_SIZE - len,
				"rdma handle=%d, cb irq_count=%d, empty_config=%d, rdma_config_count=%d",
				i, info->rdma_ins[i].irq_count,
				info->rdma_ins[i].rdma_empty_config_count,
				info->rdma_ins[i].rdma_config_count);

			if (len >= PAGE_SIZE)
				break;
		}
	}

	return len;
}

static ssize_t rdma_mgr_trace_channel_show(const struct class *cla,
					  const struct class_attribute *attr,
					  char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%x\n", rdma_trace_channel);
}

static ssize_t rdma_mgr_trace_channel_stroe(const struct class *cla,
					   const struct class_attribute *attr,
					   const char *buf, size_t count)
{
	int ret = 0;

	ret = kstrtoint(buf, 0, &rdma_trace_channel);
	if (ret < 0)
		return -EINVAL;
	return count;
}

static ssize_t store_rdma_reset(const struct class *class,
				 const struct class_attribute *attr,
				 const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	rdma_reset(res);

	return count;
}

static ssize_t show_enable(const struct class *class,
			   const struct class_attribute *attr,
			   char *buf)
{
	int i;
	int enable_flag = 0;

	for (i = 0; i < RDMA_NUM; i++)
		if (enable[i])
			enable_flag |= (1 << i);

	return snprintf(buf, PAGE_SIZE, "enable_flag: 0x%x\n",
			enable_flag);
}

static ssize_t store_enable(const struct class *class,
			    const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int i = 0;
	int enable_flag = 0, ret;

	ret = kstrtoint(buf, 0, &enable_flag);
	for (i = 0; i < RDMA_NUM; i++) {
		enable[i] = (enable_flag >> i) & 1;
		pr_info("enable[%d]=%d\n", i, enable[i]);
	}
	return count;
}

static ssize_t show_g_vsync_rdma_item_count(const struct class *class,
			   const struct class_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "g_vsync_rdma_item_count: %d\n",
			g_vsync_rdma_item_count);
}

static ssize_t store_g_vsync_rdma_item_count(const struct class *class,
			    const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int g_vsync_item_count = 0, ret;

	ret = kstrtoint(buf, 0, &g_vsync_item_count);
	pr_info("g_vsync_rdma_item_count %d -> %d\n",
		g_vsync_rdma_item_count, g_vsync_item_count);
	g_vsync_rdma_item_count = g_vsync_item_count;
	return count;
}

static ssize_t show_g_vsync_rdma_item_count_max(const struct class *class,
			   const struct class_attribute *attr,
			   char *buf)
{
	return snprintf(buf, PAGE_SIZE, "g_vsync_rdma_item_count_max: %d\n",
			g_vsync_rdma_item_count_max);
}

static ssize_t store_g_vsync_rdma_item_count_max(const struct class *class,
			    const struct class_attribute *attr,
			    const char *buf, size_t count)
{
	int g_vsync_item_count_max = 0, ret;

	ret = kstrtoint(buf, 0, &g_vsync_item_count_max);
	pr_info("g_vsync_rdma_item_count_max %d -> %d\n",
		g_vsync_rdma_item_count_max, g_vsync_item_count_max);
	g_vsync_rdma_item_count_max = g_vsync_item_count_max;
	return count;
}

#define RDMA_MBUF_TEST
#ifdef RDMA_MBUF_TEST
static int mbuf_handle;

static void mbuf_rdma_irq(void *arg)
{
	if (debug_flag & DEBUG_RDMA_DONE)
		pr_info("%s done, current index=%d\n",
			__func__, mbuf_get_current_index(mbuf_handle));
}

static struct rdma_op_s mbuf_rdma_op = {
	mbuf_rdma_irq,
	NULL
};

static void rdma_mbuf_test(int test_case)
{
	int i;
	int mbuf_step = 3 * (PAGE_SIZE);
	int test_reg[] = {0x1a17, 0x1a18, 0x1a19, 0x1a37, 0x1a39, 0x1a3a};

	if (test_case == 1) {
		for (i = 0; i < 6; i++) {
			WRITE_VCBUS_REG(test_reg[i], 0Xffffffff);
			pr_info("## default reg[0x%x]:0x%x\n", test_reg[i],
				READ_VCBUS_REG(test_reg[i]));
		}
		if (mbuf_handle == 0) {
			/* 1. register. */
			mbuf_handle = rdma_mbuf_register(&mbuf_rdma_op, NULL, 6, mbuf_step);
			pr_info("channel num:%d\n", mbuf_handle);
			if (mbuf_handle < 0) {
				pr_info("rdma_mbuf_register failed\n");
				return;
			}
		}
		/* 2. write reg to rdma mbuf. */
		rdma_mbuf_write_reg(mbuf_handle, 0, test_reg[0], 0x550055);
		rdma_mbuf_write_reg(mbuf_handle, 1, test_reg[1], 0xaa00aa);
		rdma_mbuf_write_reg(mbuf_handle, 2, test_reg[2], 0x770077);
		rdma_mbuf_write_reg(mbuf_handle, 3, test_reg[3], 0x3333);
		rdma_mbuf_write_reg(mbuf_handle, 4, test_reg[4], 0x5555);
		rdma_mbuf_write_reg(mbuf_handle, 5, test_reg[5], 0xaaaa);

		/* 3. trigger rdma config, rdma start. */
		rdma_mbuf_config(mbuf_handle, 0, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_config(mbuf_handle, 1, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_config(mbuf_handle, 2, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_config(mbuf_handle, 3, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_config(mbuf_handle, 4, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_config(mbuf_handle, 5, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_reconfig_length(mbuf_handle);
		msleep(1000);
		/* verify */
		for (i = 0; i < 6; i++)
			pr_info("## reg[0x%x]:0x%x\n", test_reg[i], READ_VCBUS_REG(test_reg[i]));
	} else if (test_case == 2) {
		/* stop rdma */
		rdma_mbuf_config(mbuf_handle, 0, 0);
		rdma_mbuf_config(mbuf_handle, 1, 0);
		rdma_mbuf_config(mbuf_handle, 2, 0);
		rdma_mbuf_config(mbuf_handle, 3, 0);
		rdma_mbuf_config(mbuf_handle, 4, 0);
		rdma_mbuf_config(mbuf_handle, 5, 0);
	} else if (test_case == 3) {
		/* stop rdma */
		for (i = 0; i < 6; i++) {
			WRITE_VCBUS_REG(test_reg[i], 0Xffffffff);
			pr_info("## default reg[0x%x]:0x%x\n", test_reg[i],
				READ_VCBUS_REG(test_reg[i]));
		}

		if (mbuf_handle == 0) {
			/* 1. register. */
			mbuf_handle = rdma_mbuf_register(&mbuf_rdma_op, NULL, 6, mbuf_step);
			pr_info("channel num:%d\n", mbuf_handle);
			if (mbuf_handle < 0) {
				pr_info("rdma_mbuf_register failed\n");
				return;
			}
		}
		/* 2. write reg to rdma mbuf. */
		rdma_mbuf_write_reg(mbuf_handle, 0, test_reg[0], 0x550055);
		for (i = 0; i < 0x2fc; i++)
			rdma_mbuf_write_reg(mbuf_handle, 1, test_reg[1], 0xaa00aa);
		rdma_mbuf_write_reg(mbuf_handle, 1, test_reg[2], 0x770077);

		for (i = 0; i < 0x411; i++)
			rdma_mbuf_write_reg(mbuf_handle, 2, test_reg[3], 0x3333);
		rdma_mbuf_write_reg(mbuf_handle, 2, test_reg[4], 0x5555);

		/* 3. trigger rdma config, rdma start. */
		rdma_mbuf_config(mbuf_handle, 0, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_config(mbuf_handle, 1, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_config(mbuf_handle, 2, RDMA_TRIGGER_VSYNC_INPUT);
		rdma_mbuf_reconfig_length(mbuf_handle);
		msleep(1000);

		/* verify */
		for (i = 0; i < 6; i++)
			pr_info("## reg[0x%x]:0x%x\n", test_reg[i], READ_VCBUS_REG(test_reg[i]));
	}
}
#endif

static ssize_t store_rdma_mbuf_test(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int test_flag = 0, ret;

	ret = kstrtoint(buf, 0, &test_flag);
	rdma_mbuf_test(test_flag);
	return count;
}

static struct class_attribute rdma_mgr_attrs[] = {
	__ATTR(debug_flag, 0664,
	       show_debug_flag, store_debug_flag),
	__ATTR(rdma_watchdog, 0664,
	       show_rdma_watchdog, store_rdma_watchdog),
	__ATTR(reset_count, 0664,
	       show_reset_count, store_reset_count),
	__ATTR(ctrl_ahb_rd_burst_size, 0664,
	       show_ctrl_ahb_rd_burst_size, store_ctrl_ahb_rd_burst_size),
	__ATTR(ctrl_ahb_wr_burst_size, 0664,
	       show_ctrl_ahb_wr_burst_size, store_ctrl_ahb_wr_burst_size),
	__ATTR(trace_enable, 0664,
	       rdma_mgr_trace_enable_show, rdma_mgr_trace_enable_store),
	__ATTR(trace_reg, 0664,
	       rdma_mgr_trace_reg_show, rdma_mgr_trace_reg_store),
	__ATTR(video_reg_table, 0664,
	       show_video_all_reg, NULL),
	__ATTR(part_enable, 0664,
	       rdma_partition_enable_show, rdma_partition_enable_store),
	__ATTR(ex_vsync_rdma, 0664,
	       show_ex_vsync_rdma, store_ex_vsync_rdma),
	__ATTR(irq_count_stat, 0664,
		   rdma_irq_count_show, NULL),
	__ATTR(trace_channel, 0664,
	       rdma_mgr_trace_channel_show, rdma_mgr_trace_channel_stroe),
	__ATTR(reset, 0664,
	       NULL, store_rdma_reset),
	__ATTR(enable, 0664,
	       show_enable, store_enable),
	__ATTR(g_vsync_rdma_item_count, 0664,
	       show_g_vsync_rdma_item_count, store_g_vsync_rdma_item_count),
	__ATTR(g_vsync_rdma_item_count_max, 0664,
	       show_g_vsync_rdma_item_count_max, store_g_vsync_rdma_item_count_max),
	__ATTR(rdma_mbuf_test, 0664,
	       NULL, store_rdma_mbuf_test),
};

static struct class *rdma_mgr_class;
static int create_rdma_mgr_class(void)
{
	int i;

	rdma_mgr_class = class_create(RDMA_MGR_CLASS_NAME);
	if (IS_ERR_OR_NULL(rdma_mgr_class)) {
		pr_err("create rdma_mgr_class failed\n");
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(rdma_mgr_attrs); i++) {
		if (class_create_file(rdma_mgr_class,
				      &rdma_mgr_attrs[i])) {
			pr_err("create rdma mgr attribute %s failed\n",
			       rdma_mgr_attrs[i].attr.name);
		}
	}
	return 0;
}

static int remove_rdma_mgr_class(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(rdma_mgr_attrs); i++)
		class_remove_file(rdma_mgr_class, &rdma_mgr_attrs[i]);

	class_destroy(rdma_mgr_class);
	rdma_mgr_class = NULL;
	return 0;
}

/* static int __devinit rdma_probe(struct platform_device *pdev) */
static int __init rdma_probe(struct platform_device *pdev)
{
	int i;
	u32 data32;
	int int_rdma;
	int handle;
	const void *prop;
	struct rdma_device_info *info = &rdma_info;
	int line_n_rdma_en = 0;
#ifdef test
	int mbuf_step = 3 * (PAGE_SIZE);
#endif

	int_rdma = platform_get_irq_byname(pdev, "rdma");
	if (int_rdma == -ENXIO) {
		dev_err(&pdev->dev, "cannot get rdma irq resource\n");
		return -ENODEV;
	}

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		struct rdma_device_data_s *rdma_meson;
		struct device_node	*of_node = pdev->dev.of_node;

		match = of_match_node(rdma_dt_match, of_node);
		if (match) {
			rdma_meson = (struct rdma_device_data_s *)match->data;
			if (rdma_meson) {
				memcpy(&rdma_meson_dev, rdma_meson,
				       sizeof(struct rdma_device_data_s));
			} else {
				pr_err("%s data NOT match\n", __func__);
				return -ENODEV;
			}
		} else {
			pr_err("%s NOT match\n", __func__);
			return -ENODEV;
		}
	} else {
		pr_err("dev %s NOT found\n", __func__);
		return -ENODEV;
	}
	/* get rdma_table_size resource  */
	rdma_table_size = RDMA_TABLE_SIZE;
	if (cpu_after_eq(MESON_CPU_MAJOR_ID_G12A)) {
		rdma_table_size = 16 * PAGE_SIZE;
		prop = of_get_property(pdev->dev.of_node, "rdma_table_page_count", NULL);
		if (prop)
			rdma_table_size = of_read_ulong(prop, 1) * PAGE_SIZE;
	}

	prop = of_get_property(pdev->dev.of_node, "line_n_rdma", NULL);
	if (prop)
		line_n_rdma_en = of_read_ulong(prop, 1);

	prop = of_get_property(pdev->dev.of_node, "rdma_read_enable", NULL);
	if (prop)
		read_rdma_en = of_read_ulong(prop, 1);
	pr_info("line_n_rdma_en = %d, read_rdma_en = %d, read_trigger = %d\n",
		line_n_rdma_en, read_rdma_en, read_rdma_trigger);

	pr_info("%s,cpu_type:%d, ver:%d, len:%d,rdma_table_size:%d\n", __func__,
		rdma_meson_dev.cpu_type,
		rdma_meson_dev.rdma_ver, rdma_meson_dev.trigger_mask_len, rdma_table_size);
#ifdef CONFIG_AMLOGIC_VPU
	rdma_vpu_dev = vpu_dev_register(VPU_RDMA, "rdma");
	vpu_dev_mem_power_on(rdma_vpu_dev);
	WRITE_VCBUS_REG(VPU_VDISP_ASYNC_HOLD_CTRL, 0x18101810);
	WRITE_VCBUS_REG(VPU_VPUARB2_ASYNC_HOLD_CTRL, 0x18101810);
#endif
	rdma_mgr_irq_request = 0;

	memset((void *)&info->rdma_reg, 0, sizeof(struct rdma_conflict_regs_s));
	for (i = 0; i < rdma_meson_dev.channel_num; i++) {
		info->rdma_ins[i].rdma_table_size = 0;
		if (rdma_meson_dev.rdma_ver == RDMA_VER_1) {
			info->rdma_ins[i].rdma_regadr = &rdma_regadr[i];
		} else if (rdma_meson_dev.rdma_ver == RDMA_VER_2) {
			info->rdma_ins[i].rdma_regadr = &rdma_regadr_tl1[i];
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		} else if (rdma_meson_dev.rdma_ver == RDMA_VER_3) {
			info->rdma_ins[i].rdma_regadr = &rdma_regadr_t7[i];
			support_64bit_addr = 1;
			has_multi_vpp = 1;
		} else if (rdma_meson_dev.rdma_ver == RDMA_VER_4) {
			info->rdma_ins[i].rdma_regadr = &rdma_regadr_t7[i];
			support_64bit_addr = 1;
			has_multi_vpp = 1;
		} else if (rdma_meson_dev.rdma_ver == RDMA_VER_5) {
			info->rdma_ins[i].rdma_regadr = &rdma_regadr_t3x[i];
			support_64bit_addr = 1;
			has_multi_vpp = 1;
		} else if (rdma_meson_dev.rdma_ver == RDMA_VER_6) {
			info->rdma_ins[i].rdma_regadr = &rdma_regadr_t6w[i];
			info->rdma_ins[i].rdma_mbuf_regadr = &rdma_mbuf_regadr_t6w[i];
			support_64bit_addr = 1;
			has_multi_vpp = 1;
			support_mbuf = 1;
#endif
		} else {
			info->rdma_ins[i].rdma_regadr = &rdma_regadr[i];
		}
		info->rdma_ins[i].keep_buf = 0;
		/*do not change it in normal case */
		info->rdma_ins[i].used = 0;
		info->rdma_ins[i].prev_trigger_type = 0;
		info->rdma_ins[i].rdma_write_count = 0;
		INIT_LIST_HEAD(&info->rdma_ins[i].reg_handle.list);
		info->rdma_ins[i].lock_flag = 0;
		enable[i] = 1;
	}

	if (rdma_meson_dev.rdma_ver == RDMA_VER_5 ||
		rdma_meson_dev.rdma_ver == RDMA_VER_6) {
		irq_status.reg = T3X_RDMA_STATUS1;
		irq_status.start = 4;
		irq_status.len = 16;
		rdma_ctrl = T3X_RDMA_CTRL;
		rdma_access_man = T3X_RDMA_ACCESS_MAN;
		rdma_status_reg = T3X_RDMA_STATUS;
	} else {
		irq_status.reg = RDMA_STATUS;
		irq_status.start = 24;
		irq_status.len = 8;
		rdma_ctrl = RDMA_CTRL;
		rdma_access_man = RDMA_ACCESS_MAN;
		rdma_status_reg = RDMA_STATUS;
	}

	if (rdma_meson_dev.cpu_type >= CPU_SC2) {
		rdma_rst = devm_reset_control_get(&pdev->dev, "rdma");
		if (IS_ERR(rdma_rst)) {
			pr_err("failed to get reset: %ld\n", PTR_ERR(rdma_rst));
			return PTR_ERR(rdma_rst);
		}
		reset_control_reset(rdma_rst);
	} else {
		WRITE_MPEG_REG(RESET4_REGISTER, (1 << 5));
	}

#ifdef SKIP_OSD_CHANNEL
	info->rdma_ins[3].used = 1; /* OSD driver uses this channel */
#endif

	if (request_irq(int_rdma, &rdma_mgr_isr,
			IRQF_SHARED, "rdma", (void *)"rdma")) {
		dev_err(&pdev->dev, "can't request irq for rdma\n");
		return -ENODEV;
	}

	rdma_mgr_irq_request = 1;
	data32  = 0;
	data32 |= 1 << 7; /* write ddr urgent */
	data32 |= 1 << 6; /* read ddr urgent */
	data32 |= ctrl_ahb_wr_burst_size << 4;
	data32 |= ctrl_ahb_rd_burst_size << 2;
	data32 |= 0 << 1;
	data32 |= 0 << 0;
	WRITE_VCBUS_REG(rdma_ctrl, data32);

	info->rdma_dev = pdev;
	if (is_meson_t5m_cpu()) {
		/* rdma alloc in 1G */
		info->rdma_dev->dev.coherent_dma_mask = DMA_BIT_MASK(30);
		info->rdma_dev->dev.dma_mask = &info->rdma_dev->dev.coherent_dma_mask;
	}

	init_rdma_handle();
	handle = rdma_register(get_rdma_ops(VSYNC_RDMA),
			       NULL, rdma_table_size);
	set_rdma_handle(VSYNC_RDMA, handle);
	if (!has_multi_vpp || read_rdma_en) {
		handle = rdma_register(get_rdma_ops(VSYNC_RDMA_READ),
				       NULL, rdma_table_size);
		set_rdma_handle(VSYNC_RDMA_READ, handle);
	}
	/* T7 lack rdma channel. So do not alloc EX_VSYNC_RDMA channel for 3 vppout */
	if (rdma_meson_dev.rdma_ver != RDMA_VER_3) {
		handle = rdma_register(get_rdma_ops(EX_VSYNC_RDMA),
			NULL, rdma_table_size);
		set_rdma_handle(EX_VSYNC_RDMA, handle);
	}

	if (line_n_rdma_en) {
		handle = rdma_register(get_rdma_ops(LINE_N_INT_RDMA),
			NULL, 2 * rdma_table_size);
		set_rdma_handle(LINE_N_INT_RDMA, handle);
	}
#ifdef test
	mbuf_handle = rdma_mbuf_register(&mbuf_rdma_op, NULL, 6, mbuf_step);
	pr_info("channel num:%d\n", mbuf_handle);
	if (mbuf_handle < 0)
		pr_info("rdma_mbuf_register failed\n");
#endif
	create_rdma_mgr_class();
	meson_reg_shadow_init(&rdma_meson_dev);
	rdma_init();
	if (support_mbuf) {
		int i, count_max;

		append_buf_info =
			kmalloc(MAX_APPEND_SIZE, GFP_KERNEL);
		count_max = MAX_APPEND_SIZE >> 3;
		for (i = 0; i < count_max; i++) {
			append_buf_info[i << 1] = RDMA_MBUF_DUMMY_REG;
			append_buf_info[(i << 1) + 1] = RDMA_MBUF_DUMMY_REG;
		}
	}
	pr_info("%s %d success\n", __func__, __LINE__);
	return 0;
}

#ifdef CONFIG_PM
static int rdma_suspend(struct device *dev)
{
	set_rdma_channel_enable(0);
	return 0;
}

static int rdma_resume(struct device *dev)
{
	/* force configuration once to avoid overflow due to missing configuration. */
	force_config_once();

	set_rdma_channel_enable(1);
	return 0;
}

static int rdma_freeze(struct device *dev)
{
	vdisp_async_hold_ctrl_val = READ_VCBUS_REG(VPU_VDISP_ASYNC_HOLD_CTRL);
	vpuarb2_async_hold_ctrl_val = READ_VCBUS_REG(VPU_VPUARB2_ASYNC_HOLD_CTRL);
	if (rdma_meson_dev.cpu_type < CPU_SC2)
		reset4_register_val = READ_MPEG_REG(RESET4_REGISTER);
	rdma_ctrl_val = READ_VCBUS_REG(rdma_ctrl);
	return 0;
}

static int rdma_thaw(struct device *dev)
{
	return 0;
}

static int rdma_restore(struct device *dev)
{
	WRITE_VCBUS_REG(VPU_VDISP_ASYNC_HOLD_CTRL, vdisp_async_hold_ctrl_val);
	WRITE_VCBUS_REG(VPU_VPUARB2_ASYNC_HOLD_CTRL, vpuarb2_async_hold_ctrl_val);
	if (rdma_meson_dev.cpu_type < CPU_SC2)
		WRITE_MPEG_REG(RESET4_REGISTER, reset4_register_val);
	WRITE_VCBUS_REG(rdma_ctrl, rdma_ctrl_val);
	return 0;
}

static const struct dev_pm_ops rdma_pm_ops = {
	.freeze = rdma_freeze,
	.thaw = rdma_thaw,
	.restore = rdma_restore,
	.suspend = rdma_suspend,
	.resume = rdma_resume,
};

#endif

/* static int __devexit rdma_remove(struct platform_device *pdev) */
static void rdma_remove(struct platform_device *pdev)
{
	pr_error("RDMA driver removed.\n");
	if (support_mbuf) {
		kfree(append_buf_info);
		append_buf_info = NULL;
	}
	remove_rdma_mgr_class();
	free_part_table_memory();
	rdma_exit();
#ifdef CONFIG_AMLOGIC_VPU
	vpu_dev_mem_power_down(rdma_vpu_dev);
	vpu_dev_unregister(rdma_vpu_dev);
#endif
}

static void rdma_shutdown(struct platform_device *pdev)
{
	pr_debug("rdma shutdown\n");
	debug_flag = DEBUG_RDMA_DONE_SKIP;
	set_rdma_channel_enable(0);
}

static struct platform_driver rdma_driver = {
	.shutdown = rdma_shutdown,
	.remove = rdma_remove,
	.driver = {
		.name = "amlogic-rdma",
		.of_match_table = rdma_dt_match,
		.pm = &rdma_pm_ops,
	},
};

int __init amrdma_init(void)
{
	return platform_driver_probe(&rdma_driver, rdma_probe);
}

void __exit amrdma_exit(void)
{
	platform_driver_unregister(&rdma_driver);
}

//MODULE_DESCRIPTION("AMLOGIC RDMA management driver");
//MODULE_LICENSE("GPL");
//MODULE_AUTHOR("Rain Zhang <rain.zhang@amlogic.com>");
