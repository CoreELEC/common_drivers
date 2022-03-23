// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/mm.h>

#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/amlogic/page_trace.h>
#include "ddr_port.h"
#include "dmc_monitor.h"

#define DMC_PROT0_STA		((0x00d0  << 2))
#define DMC_PROT0_EDA		((0x00d1  << 2))
#define DMC_PROT0_CTRL		((0x00d2  << 2))
#define DMC_PROT0_CTRL1		((0x00d3  << 2))

#define DMC_PROT1_STA		((0x00d4  << 2))
#define DMC_PROT1_EDA		((0x00d5  << 2))
#define DMC_PROT1_CTRL		((0x00d6  << 2))
#define DMC_PROT1_CTRL1		((0x00d7  << 2))

#define DMC_PROT_VIO_0		((0x00d8  << 2))
#define DMC_PROT_VIO_1		((0x00d9  << 2))

#define DMC_PROT_VIO_2		((0x00da  << 2))
#define DMC_PROT_VIO_3		((0x00db  << 2))

#define DMC_PROT_IRQ_CTRL	((0x00dc  << 2))
#define DMC_IRQ_STS		((0x00cf  << 2))

#define DMC_VIO_PROT_RANGE0	BIT(20)
#define DMC_VIO_PROT_RANGE1	BIT(21)

static size_t c3_dmc_dump_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned long val;

	for (i = 0; i < 2; i++) {
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_STA + (i * 16), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_STA:%lx\n", i, val);
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_EDA + (i * 16), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_EDA:%lx\n", i, val);
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL + (i * 16), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_CTRL:%lx\n", i, val);
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1 + (i * 16), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT%zu_CTRL1:%lx\n", i, val);
	}
	for (i = 0; i < 4; i++) {
		val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_VIO_0 + (i << 2), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT_VIO_%zu:%lx\n", i, val);
	}
	val = dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT_IRQ_CTRL:%lx\n", val);
	val = dmc_prot_rw(dmc_mon->io_mem1, DMC_IRQ_STS, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_IRQ_STS:%lx\n", val);

	return sz;
}

static void check_violation(struct dmc_monitor *mon, void *io)
{
	char rw = 'n';
	char title[5] = "";
	char id_str[MAX_NAME];
	int port, subport;
	unsigned long addr;
	unsigned long status;
	struct page *page;
	struct page_trace *trace;

	/* irq write */
	status = dmc_prot_rw(io, DMC_PROT_VIO_1, 0, DMC_READ);
	if ((status & (DMC_VIO_PROT_RANGE0 | DMC_VIO_PROT_RANGE1))) {
		/* combine address */
		addr = dmc_prot_rw(io, DMC_PROT_VIO_0, 0, DMC_READ);
		rw = 'w';
	} else {
		/* irq read */
		status = dmc_prot_rw(io, DMC_PROT_VIO_3, 0, DMC_READ);
		if ((status & (DMC_VIO_PROT_RANGE0 | DMC_VIO_PROT_RANGE1))) {
			/* combine address */
			addr = dmc_prot_rw(io, DMC_PROT_VIO_2, 0, DMC_READ);
			rw = 'r';
		} else {
			return;
		}
	}

	if (addr > mon->addr_end)
		return;

	/* ignore violation on same page/same port */
	if ((addr & PAGE_MASK) == mon->last_addr &&
	    status == mon->last_status) {
		mon->same_page++;
		return;
	}

	/* ignore cma driver pages */
	page = phys_to_page(addr);
	trace = find_page_base(page);
	if (trace && trace->migrate_type == MIGRATE_CMA) {
		if (mon->debug & DMC_DEBUG_CMA)
			sprintf(title, "%s", "_CMA");
		else
			return;
	}

	port = status & 0xff;
	subport = (status >> 8) & 0x7f;
	pr_emerg(DMC_TAG "%s, addr:%08lx, s:%08lx, ID:%s, sub:%s, c:%ld, d:%p, rw:%c\n",
		 title, addr, status, to_ports(port),
		 to_sub_ports_name(port, subport, id_str, rw),
		 mon->same_page, io, rw);

	if (rw == 'w')
		show_violation_mem(addr);
	mon->same_page   = 0;
	mon->last_addr   = addr & PAGE_MASK;
	mon->last_status = status;
}

static void c3_dmc_mon_irq(struct dmc_monitor *mon, void *data)
{
	unsigned long value;

	value = dmc_prot_rw(dmc_mon->io_mem1, DMC_IRQ_STS, 0, DMC_READ);
	if (in_interrupt()) {
		if (value & (DMC_WRITE_VIOLATION | DMC_READ_VIOLATION))
			check_violation(mon, dmc_mon->io_mem1);

		/* check irq flags just after IRQ handler */
		mod_delayed_work(system_wq, &mon->work, 0);
	}
	/* clear irq */
	value &= 0x03;		/* irq flags */
	value |= 0x04;		/* en */
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, value, DMC_WRITE);
}

static int c3_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long start, end, val;
	void *io;
	unsigned int dev1, dev2;

	if (!dmc_mon->device)
		return 0;

	start = mon->addr_start >> PAGE_SHIFT;
	end   = ALIGN(mon->addr_end, PAGE_SIZE);
	end   = end >> PAGE_SHIFT;
	dev1  = mon->device & 0xffffffff;
	dev2  = mon->device >> 32;

	io = dmc_mon->io_mem1;

	if (dev1) {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_STA, start, DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_EDA, end, DMC_WRITE);
	} else {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_STA, 0, DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_EDA, 0, DMC_WRITE);
	}

	/* when set exclude, PROT1 can not be used */
	if (dev2 && (dmc_mon->configs & POLICY_INCLUDE)) {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_STA, start, DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_EDA, end, DMC_WRITE);
	} else {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_STA, 0UL, DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_EDA, 0UL, DMC_WRITE);
	}

	val = 0xf;
	if (dmc_mon->debug & DMC_DEBUG_WRITE)
		val |= (1 << 27);

	if (dmc_mon->debug & DMC_DEBUG_READ)
		val |= (1 << 26);

	if (dmc_mon->configs & POLICY_INCLUDE)
		val |= (1 << 8);
	else
		val &= ~(1 << 8);

	if (dev1) {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL,  val,  DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1, dev1, DMC_WRITE);
	} else {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL,  0UL,  DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1, 0UL, DMC_WRITE);
	}

	/* when set exclude, PROT1 can not be used */
	if (dev2 && (dmc_mon->configs & POLICY_INCLUDE)) {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_CTRL,  val,  DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_CTRL1, dev2, DMC_WRITE);
	} else {
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_CTRL,  0UL,  DMC_WRITE);
		dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_CTRL1, 0UL, DMC_WRITE);
	}

	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT_IRQ_CTRL, 0x07, DMC_WRITE);
	pr_emerg("range:%08lx - %08lx, device:%16llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void c3_dmc_mon_disable(struct dmc_monitor *mon)
{
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_STA,   0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_EDA,   0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_STA,   0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_EDA,   0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL,  0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT0_CTRL1, 0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_CTRL,  0, DMC_WRITE);
	dmc_prot_rw(dmc_mon->io_mem1, DMC_PROT1_CTRL1, 0, DMC_WRITE);
	mon->device     = 0;
	mon->addr_start = 0;
	mon->addr_end   = 0;
}

struct dmc_mon_ops c3_dmc_mon_ops = {
	.handle_irq = c3_dmc_mon_irq,
	.set_montor = c3_dmc_mon_set,
	.disable    = c3_dmc_mon_disable,
	.dump_reg   = c3_dmc_dump_reg,
};
