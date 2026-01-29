// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Amlogic, Inc. All rights reserved.
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
#include <linux/sched/clock.h>
#include <linux/amlogic/page_trace.h>
#include "ddr_port.h"
#include "dmc_monitor.h"

#define DMC_PROT0_STA			((0x00e0  << 2))
#define DMC_PROT0_EDA			((0x00e1  << 2))
#define DMC_PROT0_CTRL			((0x00e2  << 2))
#define DMC_PROT0_CTRL1			((0x00e3  << 2))

#define DMC_PROT1_STA			((0x00e4  << 2))
#define DMC_PROT1_EDA			((0x00e5  << 2))
#define DMC_PROT1_CTRL			((0x00e6  << 2))
#define DMC_PROT1_CTRL1			((0x00e7  << 2))

#define DMC_PROT_VIO_0			((0x00e8  << 2))
#define DMC_PROT_VIO_1			((0x00e9  << 2))
#define DMC_PROT_VIO_2			((0x00ea  << 2))
#define DMC_PROT_VIO_3			((0x00eb  << 2))
#define DMC_PROT_VIO_4			((0x00ec  << 2))
#define DMC_PROT_IRQ_CTRL_STS		((0x00ed  << 2))

#define DMC_VIO_PROT0			BIT(17)
#define DMC_VIO_PROT1			BIT(18)
#define DMC_PROT_VIO_AXADDR_MASK	0x03
#define DMC_PROT_VIO_AXADDR_OFFSET	19

#define DMC_SEC_STATUS			((0x051a  << 2))
#define DMC_VIO_ADDR0			((0x051b  << 2))
#define DMC_VIO_ADDR1			((0x051c  << 2))
#define DMC_VIO_ADDR2			((0x051d  << 2))
#define DMC_VIO_ADDR3			((0x051e  << 2))
#define DMC_VIO_AXADDR_MASK		0x03
#define DMC_VIO_AXADDR_OFFSET		24

/* STS0 save addr, STS1 save prot id, */
#define DMC_PROT_STS0			(0x0380 << 2)
#define DMC_PROT_STS1			(0x0381 << 2)
#define DMC_PROT_STS127			(0x03ff << 2)

static size_t c5_dmc_dump_reg(char *buf)
{
	int sz = 0, i;
	unsigned long val;
	void *io = dmc_mon->mon_comm[0].io_mem;

	val = dmc_prot_rw(io, DMC_PROT0_STA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_STA:%lx\n", val);
	val = dmc_prot_rw(io, DMC_PROT0_EDA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_EDA:%lx\n", val);
	val = dmc_prot_rw(io, DMC_PROT0_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_CTRL:%lx\n", val);
	val = dmc_prot_rw(io, DMC_PROT0_CTRL1, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT0_CTRL1:%lx\n", val);

	val = dmc_prot_rw(io, DMC_PROT1_STA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_STA:%lx\n", val);
	val = dmc_prot_rw(io, DMC_PROT1_EDA, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_EDA:%lx\n", val);
	val = dmc_prot_rw(io, DMC_PROT1_CTRL, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_CTRL:%lx\n", val);
	val = dmc_prot_rw(io, DMC_PROT1_CTRL1, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT1_CTRL1:%lx\n", val);

	val = dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, 0, DMC_READ);
	sz += sprintf(buf + sz, "DMC_PROT_IRQ_CTRL_STS:%lx\n", val);

	for (i = 0; i < 5; i++) {
		val = dmc_prot_rw(io, DMC_PROT_VIO_0 + (i << 2), 0, DMC_READ);
		sz += sprintf(buf + sz, "DMC_PROT_VIO_%d:%lx\n", i, val);
	}

	return sz;
}

static int check_violation(struct dmc_monitor *mon, void *data)
{
	int ret = -1;
	unsigned long irqreg;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	/* irq write */
	irqreg = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL_STS, 0, DMC_READ);

	/* clear prot cache */
	dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL_STS, 0xc, DMC_WRITE);

	if (irqreg & DMC_WRITE_VIOLATION) {
		/* combine address */
		mon_comm->time = sched_clock();
		mon_comm->status = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_1, 0, DMC_READ);
		mon_comm->addr = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_0, 0, DMC_READ);
		mon_comm->rw = 'w';
		ret = 0;
	} else if (irqreg & DMC_READ_VIOLATION) {
		/* irq read */
		mon_comm->time = sched_clock();
		mon_comm->status = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_3, 0, DMC_READ);
		/* combine address */
		mon_comm->addr = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_VIO_2, 0, DMC_READ);
		mon_comm->rw = 'r';
		ret = 0;
	}

	if (!ret) {
		mon_comm->port.number = (mon_comm->status >> 24) & 0x3f;
		mon_comm->sub.number = mon_comm->status & 0xfff;
		dmc_vio_check_page(data);
	}

	return ret;
}

static int axi_id_to_port(int axi_id, int subid)
{
	static const int subid_map[64] = {
		[0b100000] = 15,
		[0b110001] = 16,
		[0b100011] = 17,
		[0b100100] = 18,
		[0b100001] = 19,
		[0b110000] = 20,
		[0b100010] = 21,
		[0b100101] = 22,
		[0b010101] = 23,
		[0b100110] = 24,
		[0b100111] = 25,
		[0b101000] = 26,
	};

	int port_id = axi_id;

	switch (axi_id) {
	case 3:
		port_id = (0x4 | (subid & 0x3));
		break;
	case 4:
	case 5:
	case 6:
	case 7:
		port_id = axi_id + 4;
		break;
	case 8:
		if (subid & BIT(8))
			port_id = 13;
		else
			port_id = 12;
		break;
	case 9:
		port_id = 14;
		break;
	case 10:
		port_id = subid_map[subid & 0x3f];
		break;
	default:
		break;
	}

	return port_id;
}

static void c5_recheck_violation(void *data)
{
	unsigned int i, count = 0, same = 0, invalid = 0, reg0, reg1, port, subport, axi_id;
	unsigned long addr;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	void *io = mon_comm->io_mem;
	unsigned long long t;

	count = dmc_prot_rw(io, DMC_PROT_VIO_4, 0, DMC_READ);
	mon_comm->prot_buf.total = count;

	t = sched_clock();
	for (i = 0; i < count; i++) {
		reg0 = dmc_prot_rw(io, DMC_PROT_STS0 + ((2 * i) << 2), 0, DMC_READ);
		reg1 = dmc_prot_rw(io, DMC_PROT_STS1 + ((2 * i) << 2), 0, DMC_READ);
		addr = ((reg1 & 0x1ffff) << 15) | ((reg0 >> 17) & 0x7fff);
		subport = reg0 & 0xfff;
		axi_id = (reg0 >> 12) & 0x1f;
		port = axi_id_to_port(axi_id, subport);
		dmc_recheck_invalid(mon_comm, addr, port, subport, &same, &invalid);

		if (sched_clock() - t >= get_recheck_ns())
			break;
	}
	mon_comm->prot_buf.same = same;
	mon_comm->prot_buf.invalid = invalid;
}

static int c5_dmc_mon_irq(struct dmc_monitor *mon, void *data, char clear)
{
	unsigned long irqreg;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	if (clear) {
		/* clear irq */
		irqreg = dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL_STS, 0, DMC_READ);
		irqreg |= 0x04;		/* en */
		irqreg |= BIT(3);	/* clear prot cache */
		dmc_prot_rw(mon_comm->io_mem, DMC_PROT_IRQ_CTRL_STS, irqreg, DMC_WRITE);
	} else {
		return check_violation(mon, data);
	}

	return 0;
}

static void c5_dmc_vio_to_port(void *data, unsigned long *vio_bit)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	*vio_bit = DMC_VIO_PROT1 | DMC_VIO_PROT0;
	set_port_to_mon_comm(data, mon_comm->port.number, mon_comm->sub.number);
}

static int c5_dmc_mon_set(struct dmc_monitor *mon)
{
	unsigned long start, end, value = 0;
	void *io = dmc_mon->mon_comm[0].io_mem;

	start = mon->addr_start >> 12;
	end   = ALIGN_DOWN(mon->addr_end, PAGE_SIZE);
	end   = end >> 12;
	dmc_prot_rw(io, DMC_PROT0_STA, start, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT0_EDA, end, DMC_WRITE);

	if (dmc_mon->debug & DMC_DEBUG_WRITE)
		value |= (1 << 30);
	/* if set, will be crash when read access */
	if (dmc_mon->debug & DMC_DEBUG_READ)
		value |= (1 << 29);
	value |= (mon->device >> 32);
	dmc_prot_rw(io, DMC_PROT0_CTRL1, value, DMC_WRITE);

	value = mon->device & 0xffffffff;
	dmc_prot_rw(io, DMC_PROT0_CTRL, value, DMC_WRITE);

	value = 0x7;
	dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, value, DMC_WRITE);

	pr_emerg("range:%08lx - %08lx, device:%llx\n",
		 mon->addr_start, mon->addr_end, mon->device);
	return 0;
}

void c5_dmc_mon_disable(struct dmc_monitor *mon)
{
	void *io = dmc_mon->mon_comm[0].io_mem;

	dmc_prot_rw(io, DMC_PROT_IRQ_CTRL_STS, 0, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT0_STA, 0, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT0_EDA, 0, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT0_CTRL, 0, DMC_WRITE);
	dmc_prot_rw(io, DMC_PROT0_CTRL1, 0, DMC_WRITE);
	mon->device     = 0;
	mon->addr_start = 0;
	mon->addr_end   = 0;
}

static int c5_reg_analysis(char *input, char *output)
{
	unsigned long status, vio_reg0, vio_reg1, vio_reg2, vio_reg3;
	int port, subport, count = 0;
	unsigned long addr;
	char rw = 'n';

	if (sscanf(input, "%lx %lx %lx %lx %lx",
			 &status, &vio_reg0, &vio_reg1,
			 &vio_reg2, &vio_reg3) != 5) {
		return 0;
	}

	if (status & DMC_READ_VIOLATION) {	/* read */
		rw = 'r';
		addr  = (vio_reg3 >> DMC_PROT_VIO_AXADDR_OFFSET) & DMC_PROT_VIO_AXADDR_MASK;
		addr  = DMC_ADDR_HIGH(addr);
		addr |= vio_reg2;

		port = (vio_reg3 >> 24) & 0x3f;
		subport = vio_reg3 & 0xfff;

		count += sprintf(output + count, "DMC READ:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}

	if (status & DMC_WRITE_VIOLATION) {	/* write */
		rw = 'w';
		addr  = (vio_reg1 >> DMC_PROT_VIO_AXADDR_OFFSET) & DMC_PROT_VIO_AXADDR_MASK;
		addr  = DMC_ADDR_HIGH(addr);
		addr |= vio_reg0;

		port = (vio_reg1 >> 24) & 0x3f;
		subport = vio_reg1 & 0xfff;

		count += sprintf(output + count, "DMC WRITE:");
		count += sprintf(output + count, "addr=%09lx port=%s sub=%s\n",
				 addr, to_ports(port), to_sub_ports_name(port, subport, rw));
	}
	return count;
}

static int dmc_sec_check(char *output)
{
	struct dmc_mon_comm tmp;
	unsigned long dmc_vio_status, dmc_vio_reg[4];
	int count = 0, axi_id, i;

	tmp.rw = 'n';
	dmc_vio_status = dmc_prot_rw(NULL, DMC_SEC_STATUS, 0, DMC_READ);
	for (i = 0; i < 4; i++)
		dmc_vio_reg[i] = dmc_prot_rw(NULL, DMC_VIO_ADDR0 + (i << 2), 0, DMC_READ);

	if (dmc_vio_status & DMC_READ_VIOLATION) {
		tmp.rw = 'r';
		tmp.addr = dmc_vio_reg[3] >> DMC_VIO_AXADDR_OFFSET;
		tmp.addr &= DMC_PROT_VIO_AXADDR_MASK;
		tmp.addr = DMC_ADDR_HIGH(tmp.addr);
		tmp.addr |= dmc_vio_reg[2];

		tmp.sub.number = dmc_vio_reg[3] & 0xfff;
		axi_id = (dmc_vio_reg[3] >> 12) & 0x1f;
		tmp.port.number = axi_id_to_port(axi_id, tmp.sub.number);
		count += dmc_sec_save_info(output + count, 0, &tmp);
	}
	if (dmc_vio_status & DMC_WRITE_VIOLATION) {
		tmp.rw = 'w';
		tmp.addr = dmc_vio_reg[1] >> DMC_VIO_AXADDR_OFFSET;
		tmp.addr &= DMC_PROT_VIO_AXADDR_MASK;
		tmp.addr  = DMC_ADDR_HIGH(tmp.addr);
		tmp.addr |= dmc_vio_reg[0];

		tmp.sub.number = dmc_vio_reg[1] & 0xfff;
		axi_id = (dmc_vio_reg[1] >> 12) & 0x1f;
		tmp.port.number = axi_id_to_port(axi_id, tmp.sub.number);
		count += dmc_sec_save_info(output + count, 0, &tmp);
	}

	count += dmc_sec_save_reg(output + count, 0, dmc_vio_status, dmc_vio_reg, 4);

	return count;
}

static int c5_dmc_reg_control(char *input, char control, char *output)
{
	int count = 0;

	switch (control) {
	case 'a':	/* analysis prot vio reg */
		count = c5_reg_analysis(input, output);
		break;
	case 'c':	/* clear sec statue reg */
		dmc_prot_rw(NULL, DMC_SEC_STATUS, 0x3, DMC_WRITE);
		break;
	case 'd':	/* dump sec vio reg */
		count = dmc_sec_check(output);
		break;
	default:
		break;
	}

	return count;
}

struct dmc_mon_ops c5_dmc_mon_ops = {
	.handle_irq	= c5_dmc_mon_irq,
	.vio_to_port	= c5_dmc_vio_to_port,
	.set_monitor	= c5_dmc_mon_set,
	.disable	= c5_dmc_mon_disable,
	.dump_reg	= c5_dmc_dump_reg,
	.reg_control	= c5_dmc_reg_control,
	.recheck_violation = c5_recheck_violation,
};
