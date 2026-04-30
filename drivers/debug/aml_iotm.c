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
#include <linux/of_reserved_mem.h>
#include <linux/of_device.h>
#include <linux/sched/clock.h>
#include <linux/sysrq.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/amlogic/gki_module.h>
#include <trace/hooks/traps.h>
#include <linux/amlogic/aml_iotm.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/panic_notifier.h>
#include <linux/timer.h>
#include <linux/syscore_ops.h>
#include <linux/hardirq.h>
#include <asm/arch_timer.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/sort.h>
#include "iotm_hw.h"

#define IOTM_DUMP_TRACE_CNT			3000
#define ETB_SIZE				(8 * 1024)
#define TRACE_BUF_SIZE				512
#define IOTM_MONITOR_RANGE_MAX			0xFFFFFFFF
#define IOTM_MONITOR_RANGE_MIN			0xE0000000
#define IOTM_V1					0x1
#define IOTM_V2					0x2
#define DDR_TRACE_MAGIC				"IOTM_trace:"
#define SIZE_OF_TRACE_MAGIC			48
#define NUM_OF_IRQ				1000
#define ETB_BUF_FULL				0x7FF
#define NUM_OF_UNRELIABLE			10

struct iotm iotm = {
	.monitor_mode = AXI_MODE,
	.dump_cnt = IOTM_DUMP_TRACE_CNT,
	.dump_mode = IOTM_DUMP_WATCHDOG,
};

static int iotm_en = IOTM_ENABLE;

const char *sw_record_name[] = {
	[IOTM_SW_SCHED_BEGIN] = "IOTM_SCHED_SWITCH_BEGIN",
	[IOTM_SW_SCHED_END] = "IOTM_SCHED_SWITCH_END",
	[IOTM_SW_IRQ_IN] = "IOTM_ISR_IN",
	[IOTM_SW_IRQ_OUT] = "IOTM_ISR_OUT",
	[IOTM_SW_SMC_IN] = "IOTM_SMC_IN",
	[IOTM_SW_SMC_OUT] = "IOTM_SMC_OUT",
	[IOTM_SW_SMC_NORET_IN] = "IOTM_SMC_NORET_IN",
	[IOTM_SW_TIME] = "IOTM_SW_TIME",
};

static struct reg_entry reg_table_public[] __initdata = {
	{0xF7000000, 0xF703AFFF, "axi_sram"},
	{0xF7100000, 0xF71FFFFF, "NIC_SYS_GPV"},
	{0xFC000000, 0xFCFFFFFF, "ddrphy_and_pll_0"},
	{0xFE000000, 0xFE001FFF, "clk_ctrl"},
	{0xFE002000, 0xFE003FFF, "reset_ctrl"},
	{0xFE004000, 0xFE005FFF, "pad_ctrl"},
	{0xFE006000, 0xFE007FFF, "mailbox"},
	{0xFE008000, 0xFE009FFF, "analog_ctrl"},
	{0xFE00A000, 0xFE00BFFF, "irq_ctrl"},
	{0xFE00C000, 0xFE00DFFF, "pwr_ctrl"},
	{0xFE00E000, 0xFE00FFFF, "cpu_ctrl"},
	{0xFE010000, 0xFE011FFF, "sys_ctrl"},
	{0xFE012000, 0xFE013FFF, "capu"},
	{0xFE01A000, 0xFE01BFFF, "acodec_ctrl"},
	{0xFE01C000, 0xFE01DFFF, "temp_sensor_top"},
	{0xFE01E000, 0xFE01FFFF, "hdmi20_aes"},
	{0xFE022000, 0xFE023FFF, "temp_sensor_cpu"},
	{0xFE026000, 0xFE027FFF, "sar_adc"},
	{0xFE02C000, 0xFE02DFFF, "i2c_mon"},
	{0xFE02E000, 0xFE02FFFF, "startup"},
	{0xFE038000, 0xFE039FFF, "Smart Card"},
	{0xFE044000, 0xFE045FFF, "cec"},
	{0xFE048000, 0xFE049FFF, "msr_clk"},
	{0xFE050000, 0xFE051FFF, "spicc(spi_sg_comb)"},
	{0xFE058000, 0xFE059FFF, "pwm_(a-j)"},
	{0xFE066000, 0xFE067FFF, "i2c_m(0-4)"},
	{0xFE072000, 0xFE073FFF, "tvfe"},
	{0xFE074000, 0xFE075FFF, "led_ctrl"},
	{0xFE078000, 0xFE079FFF, "uart(a-d)"},
	{0xFE080000, 0xFE081FFF, "ci_plus"},
	{0xFE084000, 0xFE085FFF, "ir_top"},
	{0xFE08C000, 0xFE08DFFF, "nand_emmc_C"},
	{0xFE090000, 0xFE091FFF, "axi_srama"},
	{0xFE092000, 0xFE093FFF, "amfc"},
	{0xFE09C000, 0xFE09DFFF, "aocpu"},
	{0xFE0BC000, 0xFE0BDFFF, "adec"},
	{0xFE0BE000, 0xFE0BFFFF, "atv_demod"},
	{0xFE310000, 0xFE31FFFF, "demod"},
	{0xFE320000, 0xFE32FFFF, "dos"},
	{0xFE330000, 0xFE33FFFF, "audio"},
	{0xFE380000, 0xFE38FFFF, "eth_top"},
	{0xFE390000, 0xFE39FFFF, "hdmirx"},
	{0xFE3A0000, 0xFE3AFFFF, "hdmirx_core"},
	{0xFE3B0000, 0xFE3BFFFF, "tcon"},
	{0xFE400000, 0xFE43FFFF, "mali"},
	{0xFE440000, 0xFE47FFFF, "SECTOP"},
	{0xFE800000, 0xFF7FFFFF, "a53_dbg"},
	{0xFF800000, 0xFF83FFFF, "vpu"},
	{0xFF840000, 0xFF84FFFF, "ge2d"},
	{0xFFF00000, 0xFFF07FFF, "gic"},
	{0xFFFD8000, 0xFFFF1FFF, "ROM_MAIN"},
	{0xFFFF2000, 0xFFFF7FFF, "ROM_DFU"},
	{0xFFFF8000, 0xFFFFFFFF, "ROM_MMU"},
};

static struct reg_entry reg_table_t6d[] __initdata = {
	{0xFE036000, 0xFE037FFF, "dmc0"},
	{0xFE064000, 0xFE065FFF, "i2c_s_a"},
	{0xFE07A000, 0xFE07BFFF, "uart_b"},
	{0xFE07C000, 0xFE07DFFF, "uart_c"},
	{0xFE480000, 0xFE4BFFFF, "usb_wrapper"},
};

static struct reg_entry reg_table_t6w[] __initdata = {
	{0xFE014000, 0xFE015FFF, "bcon"},
	{0xFE016000, 0xFE017FFF, "vx1_lvds"},
	{0xFE018000, 0xFE019FFF, "apb2btc"},
	{0xFE036000, 0xFE037FFF, "dmc0"},
	{0xFE064000, 0xFE065FFF, "i2c_s_a"},
	{0xFE07A000, 0xFE07BFFF, "uart_b"},
	{0xFE07C000, 0xFE07DFFF, "uart_c"},
	{0xFE09E000, 0xFE09FFFF, "aucpu"},
	{0xFE340000, 0xFE34FFFF, "dmc0"},
	{0xFE370000, 0xFE37FFFF, "usb2_2"},
	{0xFE480000, 0xFE4BFFFF, "usb_wrapper"},
	{0xFF850000, 0xFF88FFFF, "dpss"},
};

static struct reg_entry reg_table_t6x[] __initdata = {
	{0xFB000000, 0xFBFFFFFF, "ddrphy_and_pll_1"},
	{0xFE014000, 0xFE015FFF, "bcon"},
	{0xFE016000, 0xFE017FFF, "vx1_lvds"},
	{0xFE030000, 0xFE031FFF, "mdc_vpu1"},
	{0xFE032000, 0xFE033FFF, "mdc_vpu0"},
	{0xFE034000, 0xFE035FFF, "mdc_sys"},
	{0xFE036000, 0xFE037FFF, "mdc_cpu"},
	{0xFE068000, 0xFE069FFF, "i2c_tcon"},
	{0xFE094000, 0xFE095FFF, "ddr_test"},
	{0xFE09E000, 0xFE09FFFF, "aucpu"},
	{0xFE0B8000, 0xFE0B9FFF, "dsc_dec"},
	{0xFE300000, 0xFE30FFFF, "dmc1"},
	{0xFE340000, 0xFE34FFFF, "dmc0"},
	{0xFE350000, 0xFE35FFFF, "usb3_0"},
	{0xFE360000, 0xFE36FFFF, "usb2_1"},
	{0xFE370000, 0xFE37FFFF, "usb2_2"},
	{0xFF850000, 0xFF88FFFF, "dpss"},
};

const char *find_register_node(phys_addr_t addr)
{
	int left = 0, right = iotm.reg_table_size - 1;

	while (left <= right) {
		int mid = left + (right - left) / 2;

		if (addr < iotm.reg_table[mid].reg_start)
			right = mid - 1;
		else if (addr > iotm.reg_table[mid].reg_end)
			left = mid + 1;
		else
			return iotm.reg_table[mid].reg_name;
	}

	return "Unknown";
}

static int range_cmp(const void *a, const void *b)
{
	const struct reg_entry *ra = a;
	const struct reg_entry *rb = b;

	if (ra->reg_start < rb->reg_start)
		return -1;
	else if (ra->reg_start > rb->reg_start)
		return 1;

	return 0;
}

static void confirm_chip_mmap(void)
{
	iotm.reg_table_size = ARRAY_SIZE(reg_table_public);

	switch (iotm.cpu_type) {
	case IOTM_TYPE_T6D:
		iotm.reg_table_size += ARRAY_SIZE(reg_table_t6d);
		iotm.reg_table = kmalloc(sizeof(reg_table_public) + sizeof(reg_table_t6d),
								GFP_KERNEL);
		if (iotm.reg_table) {
			memcpy(iotm.reg_table, reg_table_public, sizeof(reg_table_public));
			memcpy((char *)(iotm.reg_table) + sizeof(reg_table_public),
					reg_table_t6d, sizeof(reg_table_t6d));
		}
		break;
	case IOTM_TYPE_T6W:
		iotm.reg_table_size += ARRAY_SIZE(reg_table_t6w);
		iotm.reg_table = kmalloc(sizeof(reg_table_public) + sizeof(reg_table_t6w),
								GFP_KERNEL);
		if (iotm.reg_table) {
			memcpy(iotm.reg_table, reg_table_public, sizeof(reg_table_public));
			memcpy((char *)(iotm.reg_table) + sizeof(reg_table_public),
					reg_table_t6w, sizeof(reg_table_t6w));
		}
		break;
	case IOTM_TYPE_T6X:
		iotm.reg_table_size += ARRAY_SIZE(reg_table_t6x);
		iotm.reg_table = kmalloc(sizeof(reg_table_public) + sizeof(reg_table_t6x),
								GFP_KERNEL);
		if (iotm.reg_table) {
			memcpy(iotm.reg_table, reg_table_public, sizeof(reg_table_public));
			memcpy((char *)(iotm.reg_table) + sizeof(reg_table_public),
					reg_table_t6x, sizeof(reg_table_t6x));
		}
		break;
	default:
		break;
	}

	if (iotm.reg_table) {
		sort(iotm.reg_table, iotm.reg_table_size,
				sizeof(struct reg_entry), range_cmp, NULL);
	}
}

/* Obtain the operating mode of the IOTM */
int iotm_monitor_mode_get(void)
{
	return iotm.monitor_mode;
}
EXPORT_SYMBOL(iotm_monitor_mode_get);

/* Obtain the DDR size and status of the IOTM */
int iotm_en_ddr_size_get(int *iotm_ddr_size)
{
	*iotm_ddr_size = iotm.buf_end - iotm.buf_start + 1;
	return iotm.supported;
}
EXPORT_SYMBOL(iotm_en_ddr_size_get);

static int iotm_en_set(const char *val, const struct kernel_param *kp)
{
	u32 iotm_ctrl_mode_val;
	struct arm_smccc_res res;

	if (!iotm.supported) {
		pr_err("IOTM:not support in this board\n");
		return -EINVAL;
	}

	if (kstrtoint(val, 0, &iotm_en)) {
		return -EINVAL;
	}
	iotm_ctrl_mode_val = readl(iotm.cssys_base + IOTM_CTRL_MODE);

	switch (iotm_en) {
	case IOTM_DISABLE:
		iotm_ctrl_mode_val |= IOTM_CTRL_MODE_TRACE_DISABLE;

		pr_info("IOTM:has been disabled\n");
		break;
	case IOTM_ENABLE:
	/* monitor vapb4 and capu bus then start trace data */
		iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_JTAG_SMC_ARG,
			AML_IOTM_JTAG_ENABLE_SMC_ARG, 0, 0, res);
		iotm_ctrl_mode_val |= (IOTM_CTRL_MODE_CAPU_ENABLE |
			IOTM_CTRL_MODE_VAPB4_ENABLE | IOTM_CTRL_MODE_TRACE_ENABLE);

		pr_info("IOTM:has been enabled and can record trace.\n");
		break;
	case IOTM_ENABLE_NO_TRACE:
		iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_JTAG_SMC_ARG,
			AML_IOTM_JTAG_DISABLE_SMC_ARG, 0, 0, res);
		iotm_ctrl_mode_val |= (IOTM_CTRL_MODE_CAPU_ENABLE |
			IOTM_CTRL_MODE_VAPB4_ENABLE | IOTM_CTRL_MODE_TRACE_ENABLE);

		pr_info("IOTM:can't record trace, just trigger timeout\n");
		break;
	default:
		pr_err("IOTM:iotm_en error: %s\n", val);
		break;
	}

	writel(iotm_ctrl_mode_val, iotm.cssys_base + IOTM_CTRL_MODE);

	return 0;
}

static const struct kernel_param_ops iotm_en_ops = {
	.set = iotm_en_set,
	.get = param_get_int,
};
module_param_cb(iotm_en, &iotm_en_ops, &iotm_en, 0644);

static int iotm_en_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &iotm_en)) {
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_en=", iotm_en_setup);

static int iotm_dump_mode_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 0, &iotm.dump_mode) || iotm.dump_mode > 2) {
		pr_err("IOTM:iotm.dump_mode error: %s\n", buf);
		iotm.dump_mode = IOTM_DUMP_WATCHDOG;
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_dump_mode=", iotm_dump_mode_setup);

static int iotm_monitor_mode_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 0, &iotm.monitor_mode) || iotm.monitor_mode > 2) {
		pr_err("IOTM:iotm_monitor_mode error: %s\n", buf);
		iotm.monitor_mode = 0;
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_monitor_mode=", iotm_monitor_mode_setup);

static int iotm_dump_cnt_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtouint(buf, 0, &iotm.dump_cnt)) {
		pr_err("IOTM:iotm.dump_cnt error: %s\n", buf);
		return -EINVAL;
	}

	return 1;
}
__setup("iotm_dump_cnt=", iotm_dump_cnt_setup);

/* interface for the software to record data */
void iotm_sw_record_write(u32 sw_type, u32 val1, u32 val2)
{
	unsigned long flags;

	if (iotm_en != IOTM_ENABLE || !iotm.supported)
		return;

	spin_lock_irqsave(&iotm.record_lock, flags);
	iotm.ops->sw_record_write(sw_type, val1, val2);
	spin_unlock_irqrestore(&iotm.record_lock, flags);
}
EXPORT_SYMBOL(iotm_sw_record_write);

static void print_ddr_buf(int *cnt, int trace_cnt, void *trace_start, void *trace_end)
{
	char *buf = kzalloc(TRACE_BUF_SIZE, GFP_KERNEL);

	if (!buf)
		return;

	while (trace_start < trace_end) {
		if (*cnt > trace_cnt) {
			iotm.ops->print_single_trace(trace_start, buf);
			pr_err("%s", buf);
		}

		(*cnt)++;
		trace_start += iotm.bytes_per_trace;
	}
	kfree(buf);
}

/* Ddr buffer can't be confirmed to have been overwritten.
 * So the data has been overwritten defaultly when data is dumped.
 * ddr size needs to be set to a multiple of 48 to confirm corrected.
 */
static void auto_dump_ddr_buf(void)
{
	/* the first data is DDR_TRACE_MAGIC, need to be excluded  */
	u32 trace_buf_start = iotm.buf_start + SIZE_OF_TRACE_MAGIC;
	u32 iotm_ddr_size = iotm.buf_end - trace_buf_start + 1;
	u32 iotm_axi_addr_val = readl(iotm.cssys_base + IOTM_AXI_ADDR);
	u32 last_seg_size = iotm_axi_addr_val - trace_buf_start;
	int trace_cnt = iotm_ddr_size / iotm.bytes_per_trace;
	void *buf_start_vaddr;
	void *trace_start, *trace_end;
	u32 offset = 0;
	int cnt = 0;
	u64 prev_time = 0;

	buf_start_vaddr = phys_to_virt(trace_buf_start);
	if (!buf_start_vaddr) {
		pr_err("IOTM: NOMEM\n");
		return;
	}

	offset = last_seg_size % iotm.bytes_per_trace;
	if (offset)
		last_seg_size += iotm.bytes_per_trace - offset;

	if (iotm_axi_addr_val < trace_buf_start || iotm_axi_addr_val > iotm.buf_end + 1) {
		pr_err("IOTM:dump error!IOTM_AXI_ADDR=0x%x, ddr_buf=[0x%x, 0x%x]\n",
				iotm_axi_addr_val, trace_buf_start, iotm.buf_end);
		return;
	}

	iotm.ops->clean_buf();
	iotm.ops->trace_time_loop_check(buf_start_vaddr + last_seg_size,
		buf_start_vaddr + iotm_ddr_size, &prev_time);
/*
 * Due to the offset of IOTM_AXI_ADDR, the last few data entries are unreliable,
 * and when calculating whether the time overflows, these entries need to be filtered out.
 */
	iotm.ops->trace_time_loop_check(buf_start_vaddr,
		buf_start_vaddr + last_seg_size - NUM_OF_UNRELIABLE * iotm.bytes_per_trace,
		&prev_time);

	trace_cnt = trace_cnt - iotm.dump_cnt;
	if (iotm.ops->is_trace_loop()) {
		trace_start = buf_start_vaddr + last_seg_size;
		trace_end = buf_start_vaddr + iotm_ddr_size;
		print_ddr_buf(&cnt, trace_cnt, trace_start, trace_end);
	}

	trace_start = buf_start_vaddr;
	trace_end = buf_start_vaddr + last_seg_size;
	print_ddr_buf(&cnt, trace_cnt, trace_start, trace_end);
}

static bool need_dump(void)
{
	unsigned int val;

	/* before reboot */
	if (iotm.supported) {
		val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);
		if (iotm.timeout_irq_handled || (val & (IOTM_IRQ_CTRL_CAPU_TIMEOUT |
			IOTM_IRQ_CTRL_VAPB4_TIMEOUT)))
			return true;
	}

	/* after reboot */
	switch (iotm.dump_mode) {
	case IOTM_DUMP_NONE:
		return false;
	case IOTM_DUMP_WATCHDOG:
		return iotm.ops->is_watchdog();
	case IOTM_DUMP_ALL:
		return true;
	default:
		break;
	}

	return false;
}

static void auto_dump_etb_buf(void)
{
	u32 write_ptr = 0;
	u32 depth;
	int i, j;
	int words_per_trace = iotm.bytes_per_trace / sizeof(u32);
	u32 iotm_trace[4];
	char *buf = kzalloc(TRACE_BUF_SIZE, GFP_KERNEL);

	if (!buf)
		return;

	// stop etb trace
	writel(0x0, iotm.cssys_base + ETB_CTL);

	depth = readl(iotm.cssys_base + ETB_DEPTH);

	/* -4: T6D/T6W chip designs that RWP will take 4 more steps. */
	if (!(readl(iotm.cssys_base + ETB_STATUS) & 0x1)) {
		/* etb buffer is not full, need to read from 0 */
		iotm.etb_buf_words_cnt = readl(iotm.cssys_base + ETB_RWP) - 4;
		writel(0x0, iotm.cssys_base + ETB_RRP);
	} else {
		/*
		 * etb buffer is full. need to read from ETB_RWP.
		 * (depth % words_per_trace): Loop buffer has been overwritten,
		 * and the oldest data needs to be found.
		 * +words_per_trace: the lastest data is [1,0,0] and useless.
		 */
		write_ptr = readl(iotm.cssys_base + ETB_RWP);
		write_ptr += (depth % words_per_trace) + words_per_trace - 4;
		iotm.etb_buf_words_cnt = depth;
		writel(write_ptr, iotm.cssys_base + ETB_RRP);
	}

	/* 8K */
	for (i = 0; i < iotm.etb_buf_words_cnt; i += words_per_trace) {
		for (j = 0; j < words_per_trace; j++) {
			iotm_trace[j] = readl(iotm.cssys_base + ETB_RRD);

			if (!iotm.timeout_irq_handled)
				iotm.etb_buf[i + j] = iotm_trace[j];
		}

		iotm.ops->print_single_trace(iotm_trace, buf);

		if (need_dump() && (i > iotm.etb_buf_words_cnt - iotm.dump_cnt * words_per_trace))
			pr_err("%s", buf);
	}
	kfree(buf);
}

static void dump_register_info(void *buf)
{
	int i = 0;
	int pos = 0;
	u32 *reg_base = NULL;

	if (iotm.saved_trace_show)
		/* cat /proc/aml_iotm_trace */
		reg_base = iotm.saved_trace;
	else
		reg_base = iotm.cssys_base + ADDR_RANGE0_BEGIN;

//monitor_range
	pos += sprintf(buf + pos, "IOTM:include monitor range:[%x, %x]\n",
			reg_base[(ADDR_RANGE7_BEGIN - ADDR_RANGE0_BEGIN) >> 2],
			reg_base[(ADDR_RANGE7_END - ADDR_RANGE0_BEGIN) >> 2]);
	pos += sprintf(buf + pos, "IOTM:exclude monitor range:");

	for (i = 0; i < MAX_MONITOR_RANGE * 2; i += 2)
		if (reg_base[i] != 0)
			pos += sprintf(buf + pos, "[%x, %x] ",
						reg_base[i], reg_base[i + 1]);
		else
			break;

	pos += sprintf(buf + pos, "\n");

//ddr range
	if (iotm.monitor_mode == AXI_MODE)
		iotm.ops->ddr_range_get(reg_base, buf, &pos);
	else if (iotm.monitor_mode == ETB_MODE)
		pos += sprintf(buf + pos, "IOTM:ETB\n");

//print cnt
	pos += sprintf(buf + pos, "IOTM:trace_print cnt = %d\n", iotm.dump_cnt);

//others
	pos += sprintf(buf + pos, "IOTM:MONITOR_STATUS:0x%x IOTM_CTRL_MODE = 0x%x\n",
				reg_base[(MONITOR_STATUS - ADDR_RANGE0_BEGIN) >> 2],
				reg_base[(IOTM_CTRL_MODE - ADDR_RANGE0_BEGIN) >> 2]);

	pos += sprintf(buf + pos, "IOTM:VAPB4_MONITOR_ADDR:0x%x VAPB4_MONITOR_DATA = 0x%x\n",
				reg_base[(VAPB4_MONITOR_ADDR - ADDR_RANGE0_BEGIN) >> 2],
				reg_base[(VAPB4_MONITOR_DATA - ADDR_RANGE0_BEGIN) >> 2]);

	pos += sprintf(buf + pos, "IOTM:CAPU_MONITOR_ADDR:0x%x CAPU_MONITOR_DATA = 0x%x\n",
				reg_base[(CAPU_MONITOR_ADDR - ADDR_RANGE0_BEGIN) >> 2],
				reg_base[(CAPU_MONITOR_DATA - ADDR_RANGE0_BEGIN) >> 2]);

	pos += sprintf(buf + pos, "IOTM:IOTM_IRQ_CTRL:0x%x\n",
				reg_base[(IOTM_IRQ_CTRL - ADDR_RANGE0_BEGIN) >> 2]);
}

static void dump_trace(void)
{
	char *buf = kzalloc(TRACE_BUF_SIZE, GFP_KERNEL);

	if (!buf)
		return;
	bool is_need_dump = need_dump();

	if (is_need_dump) {
		pr_err("IOTM:dump trace start==========\n");

		dump_register_info(buf);
		pr_err("%s", buf);
	}

	if (iotm.monitor_mode == AXI_MODE && is_need_dump)
		auto_dump_ddr_buf();
	else if (iotm.monitor_mode == ETB_MODE)
		auto_dump_etb_buf();

	if (is_need_dump)
		pr_err("IOTM:dump trace end==========\n");
	kfree(buf);
}

static void print_timeout_data(void)
{
	unsigned int val;

	val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);

	/* capu timeout, time is setted in bl31. */
	if (val & IOTM_IRQ_CTRL_CAPU_TIMEOUT) {
		pr_err("IOTM:capu timeout(50ms) addr:%x data:%x monitor_status:%x\n",
			readl(iotm.cssys_base + CAPU_MONITOR_ADDR),
			readl(iotm.cssys_base + CAPU_MONITOR_DATA),
			readl(iotm.cssys_base + MONITOR_STATUS));
	}

	/* vapb4 timeout, time is setted in bl31. */
	if (val & IOTM_IRQ_CTRL_VAPB4_TIMEOUT) {
		pr_err("IOTM:vapb timeout(50ms) addr:%x data:%x monitor_status:%x\n",
			readl(iotm.cssys_base + VAPB4_MONITOR_ADDR),
			readl(iotm.cssys_base + VAPB4_MONITOR_DATA),
			readl(iotm.cssys_base + MONITOR_STATUS));
	}
}

static int iotm_exception_handler(struct notifier_block *nb,
			unsigned long action, void *data)
{
	unsigned int val;
	unsigned int iotm_ctrl_mode_val;

	iotm_ctrl_mode_val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
	iotm_ctrl_mode_val |= IOTM_CTRL_MODE_TRACE_DISABLE;
	writel(iotm_ctrl_mode_val, iotm.cssys_base + IOTM_CTRL_MODE);
	udelay(5);

	val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);

	/* panic has nothing with IOTM */
	if (iotm.timeout_irq_handled == 0 && !(val & (IOTM_IRQ_CTRL_CAPU_TIMEOUT |
		IOTM_IRQ_CTRL_VAPB4_TIMEOUT)))
		return 0;

	print_timeout_data();

	dump_trace();

	return 0;
}

static struct notifier_block iotm_panic_notifier = {
	.notifier_call  = iotm_exception_handler,
};

static irqreturn_t iotm_irq_handler(int irq, void *data)
{
	unsigned int val;
	int iotm_en_saved = iotm_en;

	iotm_en = IOTM_DISABLE;

	val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);

	if (val & (IOTM_IRQ_CTRL_CAPU_TIMEOUT | IOTM_IRQ_CTRL_VAPB4_TIMEOUT)) {
		pr_err("IOTM:timeout,IOTM_IRQ_CTRL = 0x%x\n", val);
		print_timeout_data();
		iotm.timeout_irq_handled = 1;
		mb(); /* make sure we only clear irq after iotm.timeout_irq_handled is set */
		writel(val | IOTM_IRQ_CTRL_VAPB4_TIMEOUT_CLEAR | IOTM_IRQ_CTRL_CAPU_TIMEOUT_CLEAR,
				iotm.cssys_base + IOTM_IRQ_CTRL);
	}

	if (val & (IOTM_IRQ_CTRL_PACK_IRQ | IOTM_IRQ_CTRL_VAPB4_FULL | IOTM_IRQ_CTRL_CAPU_FULL)) {
		pr_err("IOTM:full irq! IOTM_IRQ_CTRL = 0x%x, MONITOR_STATUS = 0x%x, IOTM_CTRL_MODE = 0x%x, IOTM_AXI_ADDR = 0x%x\n",
			val,
			readl(iotm.cssys_base + MONITOR_STATUS),
			readl(iotm.cssys_base + IOTM_CTRL_MODE),
			readl(iotm.cssys_base + IOTM_AXI_ADDR));
		writel(val | IOTM_IRQ_CTRL_PACK_IRQ_CLEAR | IOTM_IRQ_CTRL_VAPB4_FULL_CLEAR |
				IOTM_IRQ_CTRL_CAPU_FULL_CLEAR, iotm.cssys_base + IOTM_IRQ_CTRL);

		if (iotm.monitor_mode == AXI_MODE) {
			int i = NUM_OF_IRQ;

			/* read iotm status NUM_OF_IRQ times to wait for trace to be brushed out. */
			while (i-- &&
			(readl(iotm.cssys_base + MONITOR_STATUS) & ETB_BUF_FULL) == ETB_BUF_FULL)
				;

			if (i < 0) {
				val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);
				val |= (IOTM_IRQ_CTRL_PACK_IRQ_MASK |
					IOTM_IRQ_CTRL_VAPB4_FULL_MASK |
					IOTM_IRQ_CTRL_CAPU_FULL_MASK |
					IOTM_IRQ_CTRL_PACK_IRQ_CLEAR |
					IOTM_IRQ_CTRL_VAPB4_FULL_CLEAR |
					IOTM_IRQ_CTRL_CAPU_FULL_CLEAR);
				writel(val, iotm.cssys_base + IOTM_IRQ_CTRL);

				/* stop iotm. */
				val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
				val |= IOTM_CTRL_MODE_TRACE_DISABLE;
				writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

				pr_err("IOTM:clear irq,close iotm!IOTM_IRQ_CTRL = 0x%x, MONITOR_STATUS = 0x%x, IOTM_CTRL_MODE = 0x%x, IOTM_AXI_ADDR = 0x%x\n",
					readl(iotm.cssys_base + IOTM_IRQ_CTRL),
					readl(iotm.cssys_base + MONITOR_STATUS),
					readl(iotm.cssys_base + IOTM_CTRL_MODE),
					readl(iotm.cssys_base + IOTM_AXI_ADDR));

				del_timer(&iotm.ts_to_kernel_timer);

				return IRQ_HANDLED;
			}
		}

		/* restart trace data */
		val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
		val |= (IOTM_CTRL_MODE_CAPU_ENABLE | IOTM_CTRL_MODE_VAPB4_ENABLE |
			IOTM_CTRL_MODE_TRACE_ENABLE);
		writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

		iotm_en = iotm_en_saved;
	}

	return IRQ_HANDLED;
}

static int iotm_dt_init(struct platform_device *pdev)
{
	struct device_node *node;
	struct reserved_mem *rmem;
	u32 iotm_ddr_size;
	int ret = 0;
	struct resource *res;
	struct device_node *dn = pdev->dev.of_node;
	const __be32 *exclude_reg, *include_reg;
	int len, i;

	/* get the version of iotm */
	if (of_property_read_u32(dn, "version", &iotm.version)) {
		return -EINVAL;
	}

	switch (iotm.version) {
	case IOTM_V1:
		iotm.ops = &iotm_v1_ops;
		iotm.bytes_per_trace = sizeof(struct iotm_record_v1);
		break;
	case IOTM_V2:
		iotm.ops = &iotm_v2_ops;
		iotm.bytes_per_trace = sizeof(struct iotm_record_v2);
		break;
	default:
		break;
	}

	/* get registers of a53_debug */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;

	iotm.cssys_base = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (IS_ERR(iotm.cssys_base)) {
		return PTR_ERR(iotm.cssys_base);
	}

	/* get registers of monitor_range */
	exclude_reg = of_get_property(dn, "exclude-reg", &len);
	if (exclude_reg && !(len % (sizeof(u32) * 2)))
		for (i = 0; i < len / (sizeof(u32) * 2); i++) {
			iotm.range[i].start = be32_to_cpup(exclude_reg++);
			iotm.range[i].end = be32_to_cpup(exclude_reg++);
		}

	include_reg = of_get_property(dn, "include-reg", &len);
	if (include_reg && !(len % (sizeof(u32) * 2))) {
		iotm.range[7].start = be32_to_cpup(include_reg++);
		iotm.range[7].end = be32_to_cpup(include_reg++);
	}

	/* get ddr_range buffer */
	if (iotm.monitor_mode == AXI_MODE) {
		node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
		if (!node) {
			return -ENODEV;
		}

		rmem = of_reserved_mem_lookup(node);
		if (!rmem) {
			return -ENODEV;
		}

		ret = of_property_read_u32(node, "iotm-size", &iotm_ddr_size);
		if (ret) {
			return ret;
		}

		iotm.buf_start = (rmem->base + rmem->size - iotm_ddr_size + 1) & PAGE_MASK;
		iotm.buf_end = iotm.buf_start + iotm_ddr_size - 1;
	}

	return 0;
}

static void etb_coresight_init(void)
{
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + FUNNUL_LOCKACCESS);
	iotm.ops->etb_coresight_clk();
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + REPLICAPTER_LOCKACCESS);
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + TPIU_LOCKACCESS);
	writel(ETB_LOCK_ACCESS_ALL, iotm.cssys_base + ETB_LOCKACCESS);
	writel(TPIU_ITCTRL_DISABLE, iotm.cssys_base + TPIU_ITCTRL);
	writel(TPIU_ITATBCTR2_ATREADY, iotm.cssys_base + TPIU_ITATBCTR2);
	writel(ETB_FFCR_DISABLE_FORMATTING, iotm.cssys_base + ETB_FFCR);
	writel(ETB_CTL_ENABLE_CAPTURE, iotm.cssys_base + ETB_CTL);
}

void get_boot_time(struct timer_list *t)
{
	u64 ns_time, pct;
	unsigned long flags;

	local_irq_save(flags);
	pct = __arch_counter_get_cntpct();
	ns_time = sched_clock();
	local_irq_restore(flags);

	iotm.ops->boot_time_record(pct, ns_time);

	if (t)
		mod_timer(t, jiffies + msecs_to_jiffies(10000));
}

static int ddr_trace_magic_set(void)
{
	struct page **pages;
	int page_count, i;
	phys_addr_t phys = iotm.buf_start;
	size_t size = iotm.bytes_per_trace;
	void *vaddr;

	page_count = DIV_ROUND_UP(size, PAGE_SIZE);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	for (i = 0; i < page_count; i++)
		pages[i] = pfn_to_page((phys + i * PAGE_SIZE) >> PAGE_SHIFT);

	vaddr = vmap(pages, page_count, VM_MAP, pgprot_writecombine(PAGE_KERNEL));
	kfree(pages);

	if (!vaddr)
		return -ENOMEM;

	memset(vaddr, 0, size);
	memcpy(vaddr, DDR_TRACE_MAGIC, strlen(DDR_TRACE_MAGIC) + 1);

	vunmap(vaddr);
	vaddr = NULL;

	return 0;
}

static int iotm_hw_init(void)
{
	int ret;
	unsigned int val, i;
	struct arm_smccc_res res;
	unsigned int trace_buf_start;

	/* init iotm */
	iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_INIT_SMC_ARG,
			AML_IOTM_INIT_HW_SMC_ARG, 0, 0, res);

	/* Whether iotm not record trace data */
	if (iotm_en == IOTM_ENABLE_NO_TRACE) {
		iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_JTAG_SMC_ARG,
			AML_IOTM_JTAG_DISABLE_SMC_ARG, 0, 0, res);
		pr_info("IOTM:can't record trace, just trigger timeout\n");
	}

	/* do monitor registers */
	for (i = 0; i < MAX_MONITOR_RANGE; i++) {
		if (iotm.range[i].start != 0)
			iotm_smccc_smc(AML_IOTM_SMC_CMD, AML_IOTM_RANGE_SMC_ARG, i,
				iotm.range[i].start, iotm.range[i].end, res);
	}

	iotm.ops->update_monitor_mode(&iotm);

	if (iotm.monitor_mode == AXI_MODE) {
		ret = ddr_trace_magic_set();

		if (ret)
			return ret;

		trace_buf_start = iotm.buf_start + SIZE_OF_TRACE_MAGIC;

		/* Allocate DDR reserved space to MONITOR_BUF_SIZE_LOW. */
		writel(trace_buf_start, iotm.cssys_base + MONITOR_BUF_BASEADDR_LOW);
		iotm.ops->ddr_range_set(trace_buf_start);
	} else if (iotm.monitor_mode == ETB_MODE) {
		/* init coresight and set iotm_mode*/
		etb_coresight_init();

		val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
		val |= ETB_MODE;
		writel(val, iotm.cssys_base + IOTM_CTRL_MODE);
	}

	/* Obtain the TS and kernel timestamps */
	get_boot_time(NULL);

	/* enable iotm */
	val = readl(iotm.cssys_base + IOTM_CTRL_MODE);

	/* ts1_accurate_sel=4: TS1 = (soc timestamp)/ (2^ts1_accurate_sel) */
	val &= ~IOTM_CTRL_MODE_ACCURATE;
	val |= IOTM_CTRL_MODE_ACCURATE_16;
	/* crtl_iotm_ts=4: TS1:[31:5],TS0: [4:1] */
	val &= ~IOTM_CTRL_MODE_TS;
	val |= IOTM_CTRL_MODE_TS_27;
	writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

	if (iotm.monitor_mode == AXI_MODE) {
		ret = iotm.ops->axi_mode_enable(&iotm, iotm_en);

		if (ret) {
			pr_err("IOTM: iotm trace enable fail\n");
			return -1;
		}
	} else {
		/* monitor vapb4 and capu bus then start trace data */
		val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
		val |= (IOTM_CTRL_MODE_CAPU_ENABLE | IOTM_CTRL_MODE_VAPB4_ENABLE |
				IOTM_CTRL_MODE_TRACE_ENABLE);
		writel(val, iotm.cssys_base + IOTM_CTRL_MODE);
	}

	return 0;
}

static int iotm_syscore_suspend(void)
{
	unsigned int iotm_ctrl_mode_val;

	if (!iotm.supported || iotm_en == IOTM_DISABLE)
		return 0;

	iotm.en_saved = iotm_en;
	iotm_en = IOTM_DISABLE;

	iotm_ctrl_mode_val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
	iotm_ctrl_mode_val |= IOTM_CTRL_MODE_TRACE_DISABLE;
	writel(iotm_ctrl_mode_val, iotm.cssys_base + IOTM_CTRL_MODE);

	udelay(5);

	return 0;
}

static void iotm_syscore_resume(void)
{
	int ret;

	if (!iotm.supported || iotm.en_saved == IOTM_DISABLE)
		return;

	ret = iotm_hw_init();
	if (!ret)
		iotm_en = iotm.en_saved;

	iotm.en_saved = IOTM_DISABLE;
}

static void iotm_syscore_shutdown(void)
{
	unsigned int count = 0;

	iotm_syscore_suspend();

	count = readl(iotm.cssys_base + MONITOR_STATUS) & 0x1FFF;
	if (count)
		pr_err("IOTM: %d byte left in fifo", count);
}

static struct syscore_ops iotm_syscore_ops = {
	.suspend = iotm_syscore_suspend,
	.resume = iotm_syscore_resume,
	.shutdown = iotm_syscore_shutdown,
};

static int trace_show(struct seq_file *m, void *v)
{
	void *ptr, *ptr_end = NULL;
	u8 *buf = kzalloc(TRACE_BUF_SIZE, GFP_KERNEL);

	if (!buf)
		return -1;
	u64 prev_time = 0;

	iotm.saved_trace_show = 1;
	seq_puts(m, "IOTM:dump trace start==========\n");

	dump_register_info(buf);
	seq_printf(m, "%s", buf);

	ptr = iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int);
	if (iotm.monitor_mode == AXI_MODE)
		ptr_end = ptr + iotm.buf_end - iotm.buf_start - SIZE_OF_TRACE_MAGIC + 1;
	else if (iotm.monitor_mode == ETB_MODE)
		ptr_end = ptr + iotm.etb_buf_words_cnt * sizeof(int);

	iotm.ops->clean_buf();
	iotm.ops->trace_time_loop_check(ptr,
		ptr_end - NUM_OF_UNRELIABLE * iotm.bytes_per_trace, &prev_time);
	while (ptr < ptr_end) {
		iotm.ops->print_single_trace(ptr, buf);
		seq_printf(m, "%s", buf);
		ptr += iotm.bytes_per_trace;
	}

	seq_puts(m, "IOTM:dump trace end==========\n");
	iotm.saved_trace_show = 0;
	kfree(buf);

	return 0;
}

static void iotm_proc_init(void)
{
	struct proc_dir_entry *aml_iotm_trace;
	unsigned int ddr_size = iotm.buf_end - iotm.buf_start + 1;
	u32 saved_trace_size = 0;
	int i;

	if (iotm.monitor_mode == AXI_MODE)
		saved_trace_size = ddr_size + NUM_OF_IOTM_REGISTERS * sizeof(int);
	else if (iotm.monitor_mode == ETB_MODE)
		saved_trace_size = (iotm.etb_buf_words_cnt + NUM_OF_IOTM_REGISTERS) * sizeof(int);

	iotm.saved_trace = vmalloc(saved_trace_size);
	if (!iotm.saved_trace)
		return;

	aml_iotm_trace = proc_create_single_data("aml_iotm_trace",
					0400, NULL, trace_show, NULL);
	if (!aml_iotm_trace) {
		vfree(iotm.saved_trace);
		return;
	}

	for (i = 0; i < NUM_OF_IOTM_REGISTERS; i++)
		((u32 *)iotm.saved_trace)[i] =
			readl(iotm.cssys_base + ADDR_RANGE0_BEGIN + (i << 2));

	if (iotm.monitor_mode == AXI_MODE) {
		u32 iotm_axi_addr_val = readl(iotm.cssys_base + IOTM_AXI_ADDR);
		u32 trace_buf_start = iotm.buf_start + SIZE_OF_TRACE_MAGIC;
		u32 iotm_ddr_size = iotm.buf_end - trace_buf_start + 1;
		u32 offset = 0;
		u32 last_seg_size = iotm_axi_addr_val - trace_buf_start;

		offset = last_seg_size % iotm.bytes_per_trace;
		if (offset)
			last_seg_size += iotm.bytes_per_trace - offset;

		if (iotm_axi_addr_val < trace_buf_start || iotm_axi_addr_val > iotm.buf_end + 1) {
			vfree(iotm.saved_trace);
			return;
		}

		memcpy_fromio(iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int),
			phys_to_virt(trace_buf_start + last_seg_size),
						iotm_ddr_size - last_seg_size);
		memcpy_fromio(iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int) +
			iotm_ddr_size - last_seg_size, phys_to_virt(trace_buf_start),
			iotm_axi_addr_val - trace_buf_start);
	} else if (iotm.monitor_mode == ETB_MODE) {
		memcpy_fromio(iotm.saved_trace + NUM_OF_IOTM_REGISTERS * sizeof(int),
				(void *)iotm.etb_buf, iotm.etb_buf_words_cnt * sizeof(int));
	}
}

static bool ddr_trace_valid(void)
{
	void *vaddr = phys_to_virt(iotm.buf_start);

	if (!vaddr) {
		pr_err("IOTM: NOMEM\n");
		return false;
	}

	if (!memcmp(vaddr, DDR_TRACE_MAGIC, strlen(DDR_TRACE_MAGIC)))
		return true;

	pr_err("IOTM: magic not match, not dump trace\n");
	return false;
}

static void watchdog_dump_trace(void)
{
	unsigned int val;
	unsigned int count = 0;

	count = readl(iotm.cssys_base + MONITOR_STATUS) & 0x1FFF;
	if (count)
		pr_err("IOTM: %d byte left in fifo\n", count);

	// stop iotm, manual flush
	val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
	writel(val | IOTM_CTRL_MODE_TRACE_DISABLE, iotm.cssys_base + IOTM_CTRL_MODE);

	/* init coresight to dump trace */
	if (iotm.monitor_mode == ETB_MODE) {
		etb_coresight_init();
		iotm.etb_buf = kmalloc(ETB_SIZE, GFP_KERNEL);
		if (!iotm.etb_buf)
			return;
	} else if (iotm.monitor_mode == AXI_MODE) {
		if (!ddr_trace_valid())
			return;
	}

	dump_trace();
	iotm_proc_init();

	if (iotm.monitor_mode == ETB_MODE)
		kfree(iotm.etb_buf);
}

__maybe_unused static void iotm_dump_exception(void)
{
	unsigned int val;
	char buf[300] = "";
	int pos = 0;

	if (!iotm.supported || iotm_en == IOTM_DISABLE)
		return;

	val = readl(iotm.cssys_base + IOTM_CTRL_MODE);
	pos += sprintf(buf + pos, "IOTM_CTRL_MODE: %x\n", val);

	val |= IOTM_CTRL_MODE_TRACE_DISABLE;
	writel(val, iotm.cssys_base + IOTM_CTRL_MODE);

	val = readl(iotm.cssys_base + MONITOR_STATUS);
	pos += sprintf(buf + pos, "MONITOR_STATUS: %x\n", val);

	val = readl(iotm.cssys_base + IOTM_IRQ_CTRL);
	pos += sprintf(buf + pos, "IOTM_IRQ_CTRL: %x\n", val);

	if (val & IOTM_IRQ_CTRL_CAPU_TIMEOUT)
		pos += sprintf(buf + pos, "IOTM:capu timeout(50ms) addr:%x data:%x\n",
				readl(iotm.cssys_base + CAPU_MONITOR_ADDR),
				readl(iotm.cssys_base + CAPU_MONITOR_DATA));
	else if (val & IOTM_IRQ_CTRL_VAPB4_TIMEOUT)
		pos += sprintf(buf + pos, "IOTM:vapb4 timeout(50ms) addr:%x data:%x\n",
				readl(iotm.cssys_base + VAPB4_MONITOR_ADDR),
				readl(iotm.cssys_base + VAPB4_MONITOR_DATA));

	val = readl(iotm.cssys_base + VAPB4_MONITOR_ADDR);
	pos += sprintf(buf + pos, "VAPB4_MONITOR_ADDR: %x\n", val);

	val = readl(iotm.cssys_base + VAPB4_MONITOR_DATA);
	pos += sprintf(buf + pos, "VAPB4_MONITOR_DATA: %x\n", val);

	val = readl(iotm.cssys_base + CAPU_MONITOR_ADDR);
	pos += sprintf(buf + pos, "CAPU_MONITOR_ADDR: %x\n", val);

	val = readl(iotm.cssys_base + CAPU_MONITOR_DATA);
	pos += sprintf(buf + pos, "CAPU_MONITOR_DATA: %x\n", val);

	iotm.timeout_irq_handled = 1;
	pr_err("%s", buf);
}

#if defined(CONFIG_ARM64) && defined(CONFIG_ANDROID_VENDOR_HOOKS)
static void iotm_async_dump(void *data, struct pt_regs *regs, unsigned long esr)
{
	iotm_dump_exception();
}
#elif defined(CONFIG_ARM)
__maybe_unused void dabt_arm_serror(const char *reason, struct pt_regs *regs)
{
	if (strstr(reason, "imprecise external abort"))
		iotm_dump_exception();
}
EXPORT_SYMBOL(dabt_arm_serror);
#endif

static int __ref iotm_probe(struct platform_device *pdev)
{
	int ret;
	int irq;

	iotm.cpu_type = (unsigned long)of_device_get_match_data(&pdev->dev);
	confirm_chip_mmap();

	ret = iotm_dt_init(pdev);
	if (ret)
		return ret;

	iotm.ops->coresight_clk_enable();

	watchdog_dump_trace();

	spin_lock_init(&iotm.record_lock);

	ret = iotm_hw_init();
	if (ret)
		return ret;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		pr_err("IOTM:fail to get iotm irq\n");
		return irq;
	}

	ret = devm_request_irq(&pdev->dev, irq, iotm_irq_handler, 0, "iotm", NULL);
	if (ret) {
		pr_err("IOTM:fail to request irq: %d\n", ret);
		return ret;
	}

	/* print iotm trace */
	atomic_notifier_chain_register(&panic_notifier_list, &iotm_panic_notifier);

#if defined(CONFIG_ARM64) && defined(CONFIG_ANDROID_VENDOR_HOOKS)
	register_trace_android_rvh_arm64_serror_panic(iotm_async_dump, NULL);
#endif
	register_syscore_ops(&iotm_syscore_ops);

	iotm.ops->boot_timer_setup();

	iotm.supported = 1;

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id iotm_match[] = {
	{
		.compatible = "amlogic, iotm-t6d",
		.data = (void *)IOTM_TYPE_T6D,
	},
	{
		.compatible = "amlogic, iotm-t6w",
		.data = (void *)IOTM_TYPE_T6W,
	},
	{
		.compatible = "amlogic, iotm-t6x",
		.data = (void *)IOTM_TYPE_T6X,
	},
	{}
};
#endif

static struct platform_driver iotm_driver = {
	.driver = {
		.name  = "iotm",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = iotm_match,
	#endif
	},
	.probe = iotm_probe,
};

int iotm_init(void)
{
	int ret = 0;

	if (iotm_en != IOTM_DISABLE)
		ret = platform_driver_register(&iotm_driver);

	return ret;
}
