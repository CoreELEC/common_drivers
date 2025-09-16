// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include "ddr_bandwidth.h"
#include <linux/io.h>
#include <linux/slab.h>

#define DMC_MON_CTRL0			((0x0010 << 2))
#define DMC_MON_TIMER			((0x0011 << 2))
#define DMC_MON_ALL_IDLE_CNT		((0x0012 << 2))
#define DMC_MON_ALL_BW			((0x0013 << 2))
#define DMC_MON_ALL16_BW		((0x0014 << 2))

#define DMC_AXI0_READ_BW		((0x0020 << 2))
#define DMC_CMD_FILTER_CTRL3		((0x0053 << 2))

#define DDR_TYPE			((0x0305 << 2))

static unsigned int ots_regs_offset[] __initdata = {
	(0x81 << 2), (0x85 << 2), (0x89 << 2), (0x8d << 2),
	(0x91 << 2), (0x95 << 2), (0x99 << 2), (0x9d << 2),
	(0xa1 << 2), (0xa5 << 2), (0xa9 << 2), (0xad << 2),
	(0xb1 << 2), (0xb5 << 2), (0xb9 << 2), (0xbd << 2),
	(0x61 << 2), (0x65 << 2), (0x69 << 2), (0x6d << 2),
	(0x71 << 2), (0x75 << 2), (0x79 << 2), (0x7d << 2)
};

static unsigned int ots_levels[] __initdata = {
	0x02010201, 0x04020402, 0x06040604, 0x08050805,
	0x0C080C08, 0x120C120C, 0x18131813, 0x1C161C16,
	0x20192019, 0x241C241C, 0x28202820, 0x2C232C23,
	0x30263026, 0x34293429, 0x382C382C, 0x3C2F3C2F
};

static void a9_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	pr_info("A9 can not set port, default be set\n");
}

static unsigned long a9_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	if ((readl(db->ddr_reg1 + DDR_TYPE) & 0xff00) == 0x600) {
		db->ddr_type = LPDDR5;
		db->dmc_freq = (readl(db->pll_reg) & 0xffff) * 1000000;
		db->ddr_freq = db->dmc_freq;
	} else {
		db->ddr_freq = (readl(db->pll_reg) & 0xffff) * 1000000;
		db->dmc_freq = db->ddr_freq >> 2;
	}

	return db->dmc_freq;
}

static void a9_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	val = db->mode << 31;
	val |= (readl(db->ddr_reg1 + DMC_MON_CTRL0) & ~BIT(31));
	writel(val, db->ddr_reg1 + DMC_MON_CTRL0);
}

static void a9_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i, val;

	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg1 + DMC_MON_TIMER);
	a9_dmc_bandwidth_enable(db);

	/* a9 can not set port, default be set */
	for (i = 0; i < db->channels; i++) {
		db->port[i] = 1 << i;
		db->range[i].start = 0;
		db->range[i].end   = DEFAULT_RANGE_LARGE_DDR;
	}

	val = readl(db->ddr_reg1 + DMC_MON_CTRL0);
	val |= 0xffffff;
	writel(val,  db->ddr_reg1 + DMC_MON_CTRL0);
}

static int a9_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, val, off;
	int ret = -1;

	val = readl(db->ddr_reg1 + DMC_MON_CTRL0);
	if (val & DMC_QOS_IRQ) {
		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant    += readl(db->ddr_reg1 + DMC_MON_ALL_BW);
		dg->all_grant16  += readl(db->ddr_reg1 + DMC_MON_ALL16_BW);
		for (i = 0; i < db->channels; i++) {
			off = i * 8 + DMC_AXI0_READ_BW;
			dg->channel_grant[i] += readl(db->ddr_reg1 + off);
			dg->channel_grant[i] += readl(db->ddr_reg1 + off + 4);
		}
		ret = 0;
	}

	if (!ret) {
		/* clear irq flags */
		a9_dmc_bandwidth_enable(db);

		/* each register */
		dg->all_grant   *= 16;
		dg->all_grant16 *= 16;
		for (i = 0; i < db->channels; i++)
			dg->channel_grant[i] *= 16;
	}

	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int a9_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i;
	unsigned int r, off;

	r  = readl(db->ddr_reg1 + DMC_MON_CTRL0);
	s += sprintf(buf + s, "DMC_MON_CTRL0:        %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_TIMER);
	s += sprintf(buf + s, "DMC_MON_TIMER:        %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_ALL_IDLE_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_IDLE_CNT: %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_ALL_BW);
	s += sprintf(buf + s, "DMC_MON_ALL_BW:       %08x\n", r);
	r  = readl(db->ddr_reg1 + DMC_MON_ALL16_BW);
	s += sprintf(buf + s, "DMC_MON_ALL16_BW:     %08x\n", r);

	for (i = 0; i < db->channels; i++) {
		off = i * 8 + DMC_AXI0_READ_BW;
		r  = readl(db->ddr_reg1 + off);
		s += sprintf(buf + s, "DMC_AXI%d_READ_BW:          %08x\n", i, r);
		r  = readl(db->ddr_reg1 + off + 4);
		s += sprintf(buf + s, "DMC_AXI%d_WRITE_BW:          %08x\n", i, r);
	}

	return s;
}
#endif

static int __init outstanding_init(struct ddr_bandwidth *db)
{
	int i;
	size_t count;

	db->bus_num = ARRAY_SIZE(ots_regs_offset);
	db->ost.levels.count = ARRAY_SIZE(ots_levels);

	count = db->ost.levels.count * sizeof(unsigned int);
	count += db->bus_num * sizeof(struct outstanding_reg);
	db->ost.levels.value = kzalloc(count, GFP_KERNEL);
	if (!db->ost.levels.value) {
		kfree(db->ost.levels.value);
		db->bus_num = 0;
		db->ost.levels.count = 0;
		return -ENOMEM;
	}
	db->ost.regs = (struct outstanding_reg *)(db->ost.levels.value + db->ost.levels.count);

	for (i = 0; i < db->ost.levels.count; i++)
		db->ost.levels.value[i] = ots_levels[i];

	for (i = 0; i < db->bus_num; i++) {
		db->ost.regs[i].offset = ots_regs_offset[i];
		db->ost.regs[i].def_val = readl(db->ddr_reg1 + db->ost.regs[i].offset);
		if (db->ost.levels.cur_level >= 0 && db->ost.levels.count) {
			writel(db->ost.levels.value[db->ost.levels.cur_level],
				db->ddr_reg1 + db->ost.regs[i].offset);
		}
	}

	return 0;
}

static int outstanding_set(struct ddr_bandwidth *db, int bus, int value)
{
	if (bus > db->bus_num)
		return -1;

	if (!db->ost.regs)
		return -1;

	if (value == -1)
		writel(db->ost.regs[bus].def_val, db->ddr_reg1 + db->ost.regs[bus].offset);
	else
		writel(value, db->ddr_reg1 + db->ost.regs[bus].offset);

	return 0;
}

static int outstanding_get(struct ddr_bandwidth *db, int bus)
{
	if (bus > db->bus_num)
		return -1;

	if (!db->ost.regs)
		return -1;

	return readl(db->ddr_reg1 + db->ost.regs[bus].offset);
}

static int outstanding_handle(struct ddr_bandwidth *db, int bus,
					int value, enum outstanding_type type)
{
	int ret = 0;

	switch (type) {
	case OUTSTANDING_SET:
		ret = outstanding_set(db, bus, value);
		break;
	case OUTSTANDING_GET:
		ret = outstanding_get(db, bus);
		break;
	default:
		break;
	}
	return ret;
}

static int dmc_buf_level_handle(struct ddr_bandwidth *db, u32 *val,
				enum property_type type, int rw)
{
	switch (type) {
	case WBUF_EMPTY:
		all_dmc_reg_field_access(db, val, rw, DMC_CMD_FILTER_CTRL3, 31, 1);
		break;
	case WBUF_H:
		all_dmc_reg_field_access(db, val, rw, DMC_CMD_FILTER_CTRL3, 26, 5);
		break;
	case WBUF_M:
		all_dmc_reg_field_access(db, val, rw, DMC_CMD_FILTER_CTRL3, 21, 5);
		break;
	case WBUF_L:
		all_dmc_reg_field_access(db, val, rw, DMC_CMD_FILTER_CTRL3, 16, 5);
		break;
	case RBUF_H:
		all_dmc_reg_field_access(db, val, rw, DMC_CMD_FILTER_CTRL3, 10, 5);
		break;
	case RBUF_M:
		all_dmc_reg_field_access(db, val, rw, DMC_CMD_FILTER_CTRL3, 5, 5);
		break;
	case RBUF_L:
		all_dmc_reg_field_access(db, val, rw, DMC_CMD_FILTER_CTRL3, 0, 5);
		break;
	default:
		break;
	}

	return 0;
}

static int property_access(struct ddr_bandwidth *db, u64 *val,
			   enum property_type type, int rw)
{
	if (type >= WBUF_EMPTY && type <= RBUF_L)
		return dmc_buf_level_handle(db, (u32 *)val, type, rw);

	return -1;
}

#undef DMC_AXI0_CHAN_CTRL
#define DMC_AXI0_CHAN_CTRL		((0x0080  << 2))
#define AXI_REGISTER_COUNT		((4 << 2))
#define SIDE_BAND_REG			((2 << 2))
#define SIDE_BAND_BLOCK_RW_OFFSET	28
#define SIDE_BAND_BLOCK_OFFSET		8
#define BUS_COUNT			16
static int side_band(struct ddr_bandwidth *db, unsigned char dmc, unsigned char bus)
{
	int i;
	u32 val;
	unsigned int reg;

	reg = DMC_AXI0_CHAN_CTRL + AXI_REGISTER_COUNT * bus + SIDE_BAND_REG;
	if (db->dmc_bus[dmc].bus[bus].side_band.flags) {
		for (i = 0, val = 0; i < db->dmc_bus[dmc].bus[bus].side_band.block_num; i++)
			val |= 1 << db->dmc_bus[dmc].bus[bus].side_band.block_bus[i];

		all_dmc_reg_field_access(db, &val, WRITE, reg, SIDE_BAND_BLOCK_OFFSET, BUS_COUNT);
		val = db->dmc_bus[dmc].bus[bus].side_band.rw;
		all_dmc_reg_field_access(db, &val, WRITE, reg, SIDE_BAND_BLOCK_RW_OFFSET, 2);
	} else {
		val = 0;
		all_dmc_reg_field_access(db, &val, WRITE, reg, SIDE_BAND_BLOCK_RW_OFFSET, 2);
		all_dmc_reg_field_access(db, &val, WRITE, reg, SIDE_BAND_BLOCK_OFFSET, BUS_COUNT);
	}

	return 0;
}

struct ddr_bandwidth_ops a9_ddr_bw_ops = {
	.init             = a9_dmc_bandwidth_init,
	.config_port      = a9_dmc_port_config,
	.get_freq         = a9_get_dmc_freq_quick,
	.handle_irq       = a9_handle_irq,
	.bandwidth_enable = a9_dmc_bandwidth_enable,
	.outstanding_init = outstanding_init,
	.outstanding      = outstanding_handle,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = a9_dump_reg,
#endif
	.property_access  = property_access,
	.side_band	  = side_band,
};
