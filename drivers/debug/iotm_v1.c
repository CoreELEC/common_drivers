// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/of_device.h>
#include <linux/sched/clock.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/amlogic/gki_module.h>
#include <trace/hooks/traps.h>
#include <linux/amlogic/aml_iotm.h>
#include <linux/panic_notifier.h>
#include <linux/timer.h>
#include <linux/syscore_ops.h>
#include <linux/hardirq.h>
#include <asm/arch_timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include "iotm_hw.h"

struct iotm_time {
	int long_time_en;
	int trace_loop_cnt;
	int trace_loop_cnt_now;
	int trace_loop_confirm;
	u64 prev_time;
};

static struct iotm_time iotm_time;

static int iotm_long_time_en_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &iotm_time.long_time_en)) {
		pr_err("IOTM:iotm_long_time_en error: %s\n", buf);
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_long_time_en=", iotm_long_time_en_setup);

static void sw_record_write_v1(u32 sw_type, u32 val1, u32 val2)
{
	struct iotm_record_v1 record_v1;

	record_v1.sw_stream1.sw_type = sw_type;
	record_v1.sw_stream1.cpu = smp_processor_id();
	record_v1.sw_stream1.reserved = val1;
	record_v1.stream2 = val2;

	__raw_writel(record_v1.stream2,
		iotm.cssys_base + SW_DATA_STREAM0);
	__raw_writel(record_v1.stream1 | SW_DATA_STREAM1_WRITE,
		iotm.cssys_base + SW_DATA_STREAM1);
}

static u64 ts_to_kernel_time_v1(int ts)
{
	u64 kernel_time;
	int pct_time;
	long long sched_clock_time;
	u32 high, low;
	void *base_addr = NULL;

	if (iotm.saved_trace_show)
		/* cat /proc/aml_iotm_trace */
		base_addr = iotm.saved_trace - ADDR_RANGE0_BEGIN;
	else
		base_addr = iotm.cssys_base;

	pct_time = readl(base_addr + ADDR_RANGE5_END);
	high = readl(base_addr + ADDR_RANGE6_BEGIN);
	low = readl(base_addr + ADDR_RANGE6_END);
	sched_clock_time = (long long)PACK_U32_TO_U64(high, low);

	//pct_time and ts occupt [26:0].
	if (pct_time - ts > MAX_TS / 2)
		kernel_time = (long long)(ts + MAX_TS - pct_time) * NSEC_PER_IOTM_TS +
						sched_clock_time;
	else
		kernel_time = (long long)(ts - pct_time) * NSEC_PER_IOTM_TS +
						sched_clock_time;

	if (iotm_time.trace_loop_confirm && iotm_time.trace_loop_cnt_now > 0) {
		if (kernel_time < iotm_time.prev_time)
			iotm_time.trace_loop_cnt_now--;

		iotm_time.prev_time = kernel_time;

		kernel_time -= (u64)(iotm_time.trace_loop_cnt_now) * MAX_TS * NSEC_PER_IOTM_TS;
	}
	return kernel_time;
}

static void clean_buf_v1(void)
{
	iotm_time.trace_loop_cnt = 0;
	iotm_time.trace_loop_confirm = 0;
	iotm_time.prev_time = 0;
}

static void trace_time_loop_check_v1(void *trace_start, void *trace_end, u64 *prev_time)
{
	if (!iotm_time.long_time_en)
		return;

	while (trace_start < trace_end) {
		struct iotm_record_v1 *record = trace_start;
		int	ts = record->stream0.ts;
		u64 now_time = ts_to_kernel_time_v1(ts);

		if (now_time < *prev_time)
			iotm_time.trace_loop_cnt++;
		*prev_time = now_time;
		trace_start += iotm.bytes_per_trace;
	}
	iotm_time.trace_loop_confirm = 1;
	iotm_time.trace_loop_cnt_now = iotm_time.trace_loop_cnt;
}

static void print_single_trace_v1(void *ptr, char *buf)
{
	char sched_comm[7] = {0};
	int pos = 0;
	struct iotm_record_v1 *record = ptr;
	int ts = record->stream0.ts;
	u64 kernel_time = ts_to_kernel_time_v1(ts);
	u64 rem_nsec = do_div(kernel_time, NSEC_PER_SEC);
	u32 sched_stream1;
	u64 per_cpu_time, per_cpu_us_time;

	do_div(rem_nsec, 1000);
//	pos += sprintf(buf + pos, "%d", iotm_time.trace_loop_cnt_now);

//	pos += sprintf(buf + pos, "<%x %x %x>", *(int *)ptr, *((int *)ptr + 1), *((int *)ptr + 2));
	pos += sprintf(buf + pos, "[%05llu.%06llu] <%02d> ",
			kernel_time, rem_nsec, record->stream0.idx);

	if (record->sw_stream1.mid == SW_TRACE) {
		/* The source of the data is software */

		switch (record->sw_stream1.sw_type) {
		case IOTM_SW_IRQ_IN:
		case IOTM_SW_IRQ_OUT:
			pos += sprintf(buf + pos, "<%s %d> ",
					sw_record_name[record->sw_stream1.sw_type],
					record->stream2);
			break;
		case IOTM_SW_SMC_IN:
		case IOTM_SW_SMC_OUT:
		case IOTM_SW_SMC_NORET_IN:
			pos += sprintf(buf + pos, "<%s 0x%x 0x%x> ",
					sw_record_name[record->sw_stream1.sw_type],
					record->stream2,
					record->sw_stream1.reserved);
			break;
		case IOTM_SW_SCHED_BEGIN:
		case IOTM_SW_SCHED_END:
			sched_stream1 = record->sw_stream1.reserved;

			sched_comm[0] = ((char *)&sched_stream1)[0];
			sched_comm[1] = ((char *)&sched_stream1)[1];
			sched_comm[2] = ((char *)&record->stream2)[0];
			sched_comm[3] = ((char *)&record->stream2)[1];
			sched_comm[4] = ((char *)&record->stream2)[2];
			sched_comm[5] = ((char *)&record->stream2)[3];
			sched_comm[6] = '\0';

			pos += sprintf(buf + pos, "<%s next:%s> ",
				sw_record_name[record->sw_stream1.sw_type], sched_comm);
			break;
		case IOTM_SW_TIME:
			per_cpu_time = ((u64)(record->sw_stream1.reserved) << 32) +
							record->stream2;
			per_cpu_us_time = do_div(per_cpu_time, USEC_PER_SEC);
			pos += sprintf(buf + pos, "<%s kernel_time:%llu.%06llu> ",
				sw_record_name[record->sw_stream1.sw_type],
				per_cpu_time, per_cpu_us_time);
			break;
		default:
			break;
		}
		pos += sprintf(buf + pos, "<cpu%d>", record->sw_stream1.cpu);
	} else {
		const char *node_name =
			find_register_node(record->io_stream1.addr + ADDR_OFFSET);

		/* The source of the data is hardware */
		pos += sprintf(buf + pos, "<%s %s %08x-%08x> %s<%s>",
				record->stream0.rw ? "IOTM-W" : "IOTM-R",
				record->io_stream1.fail ? "fail" : "",
				record->io_stream1.addr + ADDR_OFFSET,
				record->stream2,
				record->io_stream1.mid == AOCPU_TRACE ? "AOCPU" : "",
				node_name);
	}
	pos += sprintf(buf + pos, "\n");
}

static bool is_trace_loop_v1(void)
{
	return true;
}

static inline void etb_coresight_clk_v1(void)
{
	writel(FUNNEL_CTRL_REG_VAL_V1, iotm.cssys_base + FUNNEL_CTRL_REG);
}

static inline void coresight_clk_enable_v1(void)
{
}

/*
 * Different versions have different status detections for watchdogs
 * if watchdog reset:
 * v1: wdt irq pull up
 * v2: status change
 */
static bool is_watchdog_v1(void)
{
	u32 iotm_irq_ctrl = readl(iotm.cssys_base + IOTM_IRQ_CTRL);

	if (iotm_irq_ctrl & IOTM_IRQ_CTRL_WDT_IRQ)
		return true;

	return false;
}

static void boot_timer_setup_v1(void)
{
	/* create timer to record kernel_time and iotm timestamp */
	timer_setup(&iotm.ts_to_kernel_timer, get_boot_time, 0);
	mod_timer(&iotm.ts_to_kernel_timer, jiffies + msecs_to_jiffies(10000));
}

static void boot_time_record_v1(u64 pct, u64 ns_time)
{
	__raw_writel((pct >> 4) & MAX_TS, iotm.cssys_base + ADDR_RANGE5_END);
	__raw_writel((u32)(ns_time >> 32), iotm.cssys_base + ADDR_RANGE6_BEGIN);
	__raw_writel((u32)ns_time, iotm.cssys_base + ADDR_RANGE6_END);
}

static void ddr_range_get_v1(u32 *reg_base, void *buf, int *pos)
{
	*pos += sprintf(buf + *pos, "IOTM:AXI:ddr_range:[%x, %x]\n",
		reg_base[(MONITOR_BUF_BASEADDR_LOW - ADDR_RANGE0_BEGIN) >> 2],
		reg_base[(MONITOR_BUF_BASEADDR_LOW - ADDR_RANGE0_BEGIN) >> 2] +
		(reg_base[(MONITOR_BUF_SIZE_LOW - ADDR_RANGE0_BEGIN) >> 2] << 4));
}

static void ddr_range_set_v1(u32 trace_buf_start)
{
	writel((iotm.buf_end - trace_buf_start + 1) >> 4,
		iotm.cssys_base + MONITOR_BUF_SIZE_LOW);
}

static int axi_mode_enable_v1(struct iotm *iotm, int iotm_en)
{
	unsigned int val, i;
	unsigned long flags;

	/* confirm IOTM_AXI_ADDR can point to the correct ddr address */
	for (i = 0; i < CONTINUOUS_PULSE; i++) {
		val = readl(iotm->cssys_base + IOTM_CTRL_MODE);
		val |= IOTM_CTRL_MODE_TRACE_ENABLE;
		writel(val, iotm->cssys_base + IOTM_CTRL_MODE);
		udelay(CONTINUOUS_PULSE_TIME);

		val = readl(iotm->cssys_base + IOTM_CTRL_MODE);
		val &= ~IOTM_CTRL_MODE_TRACE_ENABLE;
		writel(val, iotm->cssys_base + IOTM_CTRL_MODE);
	}

	local_irq_save(flags);
	/* monitor vapb4 and capu bus then start trace data */
	val = readl(iotm->cssys_base + IOTM_CTRL_MODE);
	val |= (IOTM_CTRL_MODE_CAPU_ENABLE | IOTM_CTRL_MODE_VAPB4_ENABLE |
			IOTM_CTRL_MODE_TRACE_ENABLE);
	writel(val, iotm->cssys_base + IOTM_CTRL_MODE);

	for (i = 0; i < CONTINUOUS_PULSE; i++)
		readl(iotm->cssys_base + IOTM_AXI_ADDR);

	/* When disable record trace, the axi_addr can't be changed. */
	if (iotm_en != IOTM_ENABLE_NO_TRACE) {
		/* Check whether 0x0 is written in the trace */
		val = readl(iotm->cssys_base + IOTM_AXI_ADDR);
		if (val < iotm->buf_start || val >= iotm->buf_end) {
			pr_err("IOTM:AXI_ADDR = 0x%x is wrong,close iotm\n", val);

			val = readl(iotm->cssys_base + IOTM_CTRL_MODE);
			val |= IOTM_CTRL_MODE_TRACE_DISABLE;
			writel(val, iotm->cssys_base + IOTM_CTRL_MODE);

			local_irq_restore(flags);
			return -1;
		}
	}
	local_irq_restore(flags);

	return 0;
}

static inline void update_monitor_mode_v1(struct iotm *iotm)
{
	struct device_node *dmc_monitor_reserved;
	struct reserved_mem *rmem;

	dmc_monitor_reserved = of_find_compatible_node(NULL, NULL, "amlogic,dmc_monitor-reserved");
	if (!dmc_monitor_reserved)
		goto FAIL;

	rmem = of_reserved_mem_lookup(dmc_monitor_reserved);
	if (!rmem || !rmem->size)
		goto FAIL;

	return;

FAIL:
	iotm->monitor_mode = ETB_MODE;
}

struct iotm_ops iotm_v1_ops = {
	.ddr_range_set = ddr_range_set_v1,
	.etb_coresight_clk = etb_coresight_clk_v1,
	.coresight_clk_enable = coresight_clk_enable_v1,
	.boot_timer_setup = boot_timer_setup_v1,
	.boot_time_record = boot_time_record_v1,
	.is_watchdog = is_watchdog_v1,
	.ddr_range_get = ddr_range_get_v1,
	.is_trace_loop = is_trace_loop_v1,
	.print_single_trace = print_single_trace_v1,
	.sw_record_write = sw_record_write_v1,
	.trace_time_loop_check = trace_time_loop_check_v1,
	.clean_buf = clean_buf_v1,
	.axi_mode_enable = axi_mode_enable_v1,
	.update_monitor_mode = update_monitor_mode_v1,
};
