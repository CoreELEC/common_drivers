/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DEBUG_IOTM_HW_H_
#define __DEBUG_IOTM_HW_H_

#include <linux/list.h>

#define AML_IOTM_SMC_CMD			0x8200007A
#define AML_IOTM_INIT_SMC_ARG			0x1
#define AML_IOTM_RANGE_SMC_ARG			0x2
#define AML_IOTM_JTAG_SMC_ARG			0x3
#define AML_IOTM_SHUTDOWN_SMC_ARG		0x4
#define AML_IOTM_JTAG_DISABLE_SMC_ARG		0x0
#define AML_IOTM_JTAG_ENABLE_SMC_ARG		0x1
#define AML_IOTM_INIT_CLK_SMC_ARG		0x0
#define AML_IOTM_INIT_HW_SMC_ARG		0x1
#define CONTINUOUS_PULSE_TIME                   10
#define CONTINUOUS_PULSE                        20

#define REPLICAPTER_LOCKACCESS			0x1fb0
#define TPIU_ITATBCTR2				0x2ef0
#define TPIU_ITCTRL				0x2f00
#define TPIU_LOCKACCESS				0x2fb0
#define ETB_DEPTH				0x3004
#define ETB_STATUS				0x300c
#define ETB_RRD					0x3010
#define ETB_RRP					0x3014
#define ETB_RWP					0x3018
#define ETB_CTL					0x3020
#define ETB_FFCR				0x3304
#define ETB_LOCKACCESS				0x3fb0
#define FUNNEL_CTRL_REG				0x4000
#define FUNNUL_LOCKACCESS			0x4fb0

#define ADDR_RANGE0_BEGIN			(0x7000 + (0x00 << 2))
#define ADDR_RANGE0_END				(0x7000 + (0x01 << 2))
#define ADDR_RANGE1_BEGIN			(0x7000 + (0x02 << 2))
#define ADDR_RANGE1_END				(0x7000 + (0x03 << 2))
#define ADDR_RANGE2_BEGIN			(0x7000 + (0x04 << 2))
#define ADDR_RANGE2_END				(0x7000 + (0x05 << 2))
#define ADDR_RANGE3_BEGIN			(0x7000 + (0x06 << 2))
#define ADDR_RANGE3_END				(0x7000 + (0x07 << 2))
#define ADDR_RANGE4_BEGIN			(0x7000 + (0x08 << 2))
#define ADDR_RANGE4_END				(0x7000 + (0x09 << 2))
#define ADDR_RANGE5_BEGIN			(0x7000 + (0x0a << 2))
#define ADDR_RANGE5_END				(0x7000 + (0x0b << 2))
#define ADDR_RANGE6_BEGIN			(0x7000 + (0x0c << 2))
#define ADDR_RANGE6_END				(0x7000 + (0x0d << 2))
#define ADDR_RANGE7_BEGIN			(0x7000 + (0x0e << 2))
#define ADDR_RANGE7_END				(0x7000 + (0x0f << 2))
#define MONITOR_RANGE_INCLUDE			(0x7000 + (0x10 << 2))
#define SW_DATA_STREAM0				(0x7000 + (0x11 << 2))
#define SW_DATA_STREAM1				(0x7000 + (0x12 << 2))
#define VAPB4_WAIT_TIME				(0x7000 + (0X13 << 2))
#define MONITOR_CTRL				(0x7000 + (0x14 << 2))
#define MONITOR_BUF_BASEADDR_LOW		(0x7000 + (0x15 << 2))
#define MONITOR_BUF_SIZE_LOW			(0x7000 + (0x16 << 2))
#define MONITOR_STATUS				(0x7000 + (0x17 << 2))
#define IOTM_PACK_FEED_DATA			(0x7000 + (0x18 << 2))
#define IOTM_IRQ_CTRL				(0x7000 + (0x19 << 2))
#define CAPU_WAIT_TIME				(0x7000 + (0x1a << 2))
#define CAPU_MONITOR_ADDR			(0x7000 + (0x1b << 2))
#define CAPU_MONITOR_DATA			(0x7000 + (0x1c << 2))
#define VAPB4_MONITOR_ADDR			(0x7000 + (0x1d << 2))
#define VAPB4_MONITOR_DATA			(0x7000 + (0x1e << 2))
#define IOTM_ARB_QOS_WEIGHT			(0x7000 + (0x1f << 2))
#define IOTM_CTRL_MODE				(0x7000 + (0x20 << 2))
#define IOTM_AXI_ADDR				(0x7000 + (0x21 << 2))
#define IOTM_AXI_SIZE				(0x7000 + (0x22 << 2))
#define IOTM_FAIL_READ_DATA			(0x7000 + (0x23 << 2))
#define IOTM_WTDRST_MAGIC_DATA			(0x7000 + (0x24 << 2))
#define IOTM_SW_DEBUG0				(0x7000 + (0x25 << 2))
#define IOTM_SW_DEBUG1				(0x7000 + (0x26 << 2))
#define IOTM_SW_DEBUG2				(0x7000 + (0x27 << 2))
#define IOTM_SW_DEBUG3				(0x7000 + (0x28 << 2))
#define NUM_OF_IOTM_REGISTERS			(((IOTM_SW_DEBUG3 - ADDR_RANGE0_BEGIN) >> 2) + 1)

#define IOTM_CTRL_MODE_ACCURATE			GENMASK(21, 18)
#define IOTM_CTRL_MODE_ACCURATE_16		(0x4 << 18)
#define IOTM_CTRL_MODE_TS			GENMASK(17, 13)
#define IOTM_CTRL_MODE_TS_27			(0x4 << 13)
#define IOTM_CTRL_MODE_TRACE_ENABLE		BIT(2)
#define IOTM_CTRL_MODE_CAPU_ENABLE		BIT(3)
#define IOTM_CTRL_MODE_VAPB4_ENABLE		BIT(4)
#define IOTM_CTRL_MODE_TRACE_DISABLE		BIT(5)
#define ETB_LOCK_ACCESS_ALL			0xc5acce55
#define FUNNEL_CTRL_REG_VAL_V1			(BIT(9) | BIT(8) | BIT(4))
#define FUNNEL_CTRL_REG_VAL_V2			(BIT(9) | BIT(8) | BIT(5))
#define TPIU_ITCTRL_DISABLE			BIT(0)
#define TPIU_ITATBCTR2_ATREADY			BIT(0)
#define ETB_FFCR_DISABLE_FORMATTING		0U
#define ETB_CTL_ENABLE_CAPTURE			BIT(0)
#define MONITOR_RANGE_INCLUDE_RST_MASK		BIT(15)
#define MONITOR_STATUS_SEC_WDT_RESET		BIT(31)
#define MONITOR_STATUS_WDT_RESET		BIT(30)
#define IOTM_IRQ_CTRL_WDT_IRQ			BIT(20)
#define IOTM_IRQ_CTRL_SEC_WDT_IRQ		BIT(17)
#define IOTM_IRQ_CTRL_PACK_IRQ			BIT(14)
#define IOTM_IRQ_CTRL_PACK_IRQ_CLEAR		BIT(13)
#define IOTM_IRQ_CTRL_PACK_IRQ_MASK		BIT(12)
#define IOTM_IRQ_CTRL_VAPB4_TIMEOUT		BIT(11)
#define IOTM_IRQ_CTRL_VAPB4_FULL		BIT(10)
#define IOTM_IRQ_CTRL_VAPB4_TIMEOUT_CLEAR	BIT(9)
#define IOTM_IRQ_CTRL_VAPB4_FULL_CLEAR		BIT(8)
#define IOTM_IRQ_CTRL_VAPB4_FULL_MASK		BIT(6)
#define IOTM_IRQ_CTRL_CAPU_TIMEOUT		BIT(5)
#define IOTM_IRQ_CTRL_CAPU_FULL			BIT(4)
#define IOTM_IRQ_CTRL_CAPU_TIMEOUT_CLEAR	BIT(3)
#define IOTM_IRQ_CTRL_CAPU_FULL_CLEAR		BIT(2)
#define IOTM_IRQ_CTRL_CAPU_FULL_MASK		BIT(0)
#define SW_DATA_STREAM1_WRITE			BIT(31)

#define ADDR_OFFSET				0xE0000000
#define AOCPU_TRACE				1
#define SW_TRACE				2
#define MAX_MONITOR_RANGE			8
#define MAX_TS					0x7ffffff
//NSEC_PER_IOTM_TS:TS1 * NSEC_PER_IOTM_TS = NS
#define NSEC_PER_IOTM_TS			666
#define THOUSAND_NSEC_PER_IOTM_TS		666666

#define iotm_smccc_smc(cmd, arg0, arg1, arg2, arg3, res) \
__arm_smccc_smc(cmd, arg0, arg1, arg2, arg3, 0, 0, 0, &(res), NULL)

/* Combine two words into double_word */
#define PACK_U32_TO_U64(high, low)	\
	((((u64)(high)) << 32) | ((u64)(low)))

enum iotm_en_mode {
	IOTM_DISABLE,
	IOTM_ENABLE,
	IOTM_ENABLE_NO_TRACE,
};

enum iotm_mode {
	AXI_MODE,
	TPIU_MODE,
	ETB_MODE,
};

enum iotm_type {
	IOTM_TYPE_T6D,
	IOTM_TYPE_T6W,
	IOTM_TYPE_T6X,
};

enum iotm_dump {
	IOTM_DUMP_NONE,
	IOTM_DUMP_WATCHDOG,
	IOTM_DUMP_ALL,
};

struct iotm_record_v1 {
	struct {
		u32 rw:1;
		u32 idx:4;
		u32 ts:27;
	} stream0;
	union {
		struct {
			u32 cpu:3;
			u32 sw_type:3;
			u32 reserved:23;
			u32 mid:2;
			u32 fail:1;
		} sw_stream1;
		struct {
			u32 addr:29;
			u32 mid:2;
			u32 fail:1;
		} io_stream1;
		u32 stream1;
	};
	u32 stream2;
};

struct iotm_record_v2 {
	u32 stream0;
	struct {
		u32 mid:1;
		u32 idx:4;
		u32 ts:27;
	} stream1;
	union {
		struct {
			u32 cpu:3;
			u32 sw_type:3;
			u32 reserved:25;
			u32 mid:1;
		} sw_stream2;
		struct {
			u32 addr:29;
			u32 fail:1;
			u32 rw:1;
			u32 mid:1;
		} io_stream2;
		u32 stream2;
	};
	u32 stream3;
};

extern const char *sw_record_name[];

struct monitor_range {
	phys_addr_t start;
	phys_addr_t end;
};

struct reg_entry {
	phys_addr_t reg_start;
	phys_addr_t reg_end;
	const char *reg_name;
};

struct iotm {
	int cpu_type;
	int supported;
	int en_saved;
	int dump_cnt;
	int bytes_per_trace;
	unsigned int dump_mode;
	void __iomem *cssys_base;
	unsigned int monitor_mode;
	unsigned int version;
	struct monitor_range range[MAX_MONITOR_RANGE];
	unsigned int buf_start;
	unsigned int buf_end;
	void *saved_trace;
	u32 *etb_buf;
	int etb_buf_words_cnt;
	/* Protects access to the IOTM trace records */
	spinlock_t record_lock;
	struct timer_list ts_to_kernel_timer;
	int timeout_irq_handled;
	struct iotm_ops *ops;
	struct reg_entry *reg_table;
	int reg_table_size;
	int saved_trace_show;
};

struct iotm_ops {
	void (*ddr_range_set)(u32 trace_buf_start);
	void (*ddr_range_get)(u32 *reg_base, void *buf, int *offset);
	void (*etb_coresight_clk)(void);
	void (*coresight_clk_enable)(void);
	void (*boot_time_record)(u64 pct, u64 ns_time);
	void (*boot_timer_setup)(void);
	bool (*is_watchdog)(void);
	bool (*is_trace_loop)(void);
	void (*print_single_trace)(void *ptr, char *buf);
	void (*sw_record_write)(u32 sw_type, u32 val1, u32 val2);
	void (*trace_time_loop_check)(void *trace_start, void *trace_end, u64 *prev_time);
	void (*clean_buf)(void);
	int  (*axi_mode_enable)(struct iotm *iotm, int iotm_en);
	void (*update_monitor_mode)(struct iotm *iotm);
};

extern struct iotm iotm;
extern struct iotm_ops iotm_v1_ops;
extern struct iotm_ops iotm_v2_ops;
const char *find_register_node(phys_addr_t addr);
void get_boot_time(struct timer_list *t);

#endif  /* __DEBUG_IOTM_HW_H_ */

