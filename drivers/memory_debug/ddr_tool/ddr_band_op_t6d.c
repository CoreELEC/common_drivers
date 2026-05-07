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

#define DMC_MON0_STA			((0x0020 << 2))
#define DMC_MON0_EDA			((0x0021 << 2))
#define DMC_MON0_CTRL			((0x0022 << 2))
#define DMC_MON0_BW			((0x0023 << 2))

#define DMC_MON1_STA			((0x0024 << 2))
#define DMC_MON1_EDA			((0x0025 << 2))
#define DMC_MON1_CTRL			((0x0026 << 2))
#define DMC_MON1_BW			((0x0027 << 2))

#define DMC_MON2_STA			((0x0028 << 2))
#define DMC_MON2_EDA			((0x0029 << 2))
#define DMC_MON2_CTRL			((0x002a << 2))
#define DMC_MON2_BW			((0x002b << 2))

#define DMC_MON3_STA			((0x002c << 2))
#define DMC_MON3_EDA			((0x002d << 2))
#define DMC_MON3_CTRL			((0x002e << 2))
#define DMC_MON3_BW			((0x002f << 2))

#define DMC_MON4_STA			((0x0030 << 2))
#define DMC_MON4_EDA			((0x0031 << 2))
#define DMC_MON4_CTRL			((0x0032 << 2))
#define DMC_MON4_BW			((0x0033 << 2))

#define DMC_MON5_STA			((0x0034 << 2))
#define DMC_MON5_EDA			((0x0035 << 2))
#define DMC_MON5_CTRL			((0x0036 << 2))
#define DMC_MON5_BW			((0x0037 << 2))

#define DMC_MON6_STA			((0x0038 << 2))
#define DMC_MON6_EDA			((0x0039 << 2))
#define DMC_MON6_CTRL			((0x003a << 2))
#define DMC_MON6_BW			((0x003b << 2))

#define DMC_MON7_STA			((0x003c << 2))
#define DMC_MON7_EDA			((0x003d << 2))
#define DMC_MON7_CTRL			((0x003e << 2))
#define DMC_MON7_BW			((0x003f << 2))

#define DMC_MON0_CTRL1			((0x0018 << 2))
#define DMC_MON1_CTRL1			((0x0019 << 2))
#define DMC_MON2_CTRL1			((0x001a << 2))
#define DMC_MON3_CTRL1			((0x001b << 2))
#define DMC_MON4_CTRL1			((0x001c << 2))
#define DMC_MON5_CTRL1			((0x001d << 2))
#define DMC_MON6_CTRL1			((0x001e << 2))
#define DMC_MON7_CTRL1			((0x001f << 2))

#define DMC_CMD_FILTER_CTRL3		((0x0042  << 2))

#define DMC_CHAN_STS_OFFSET		((0x00cf  << 2))
static unsigned int axi_chan_sts_offset[] __initdata = {
	(0x84 << 2), (0x88 << 2), (0x8d << 2),
	(0x93 << 2), (0x98 << 2), (0x9c << 2),
	(0xa3 << 2), (0xa8 << 2), (0xad << 2)
};

static unsigned int ots_regs_offset[] __initdata = {
	(0x81 << 2), (0x86 << 2), (0x8b << 2),
	(0x91 << 2), (0x96 << 2), (0x9b << 2),
	(0xa1 << 2), (0xa6 << 2), (0xab << 2)
};

static unsigned int ots_levels[] __initdata = {
	0x02010201, 0x04020402, 0x06040604, 0x08050805,
	0x0C080C08, 0x120C120C, 0x18131813, 0x1C161C16,
	0x20192019, 0x241C241C, 0x28202820, 0x2C232C23,
	0x30263026, 0x34293429, 0x382C382C, 0x3C2F3C2F
};

static void t6d_dmc_port_config(struct ddr_bandwidth *db, int channel, int map_id)
{
	unsigned int val;
	unsigned int off = 0, off1 = 0;
	int port, sub;

	off = DMC_MON0_CTRL + channel * 16;
	off1 = DMC_MON0_CTRL1 + channel * 4;

	/* clear all port mask */
	if (map_id < 0) {
		writel(0, db->ddr_reg[0] + off);	/* DMC_MON*_CTRL */
		writel(0, db->ddr_reg[0] + off1);	/* DMC_MON*_CTRL1 */
		return;
	}

	port = db->port_map_table[map_id].port_id;
	sub = db->port_map_table[map_id].sub_id;

	if (sub < 0) {
		val = readl(db->ddr_reg[0] + off);
		val |=  (1 << port);
		writel(val, db->ddr_reg[0] + off);	/* DMC_MON*_CTRL */
		val = 0xffff;
		writel(val, db->ddr_reg[0] + off1);	/* DMC_MON*_CTRL1 */
	} else {
		val = (0x1 << port);	/* select major port */
		writel(val, db->ddr_reg[0] + off);
		val = readl(db->ddr_reg[0] + off1);
		val |= (1 << sub);
		writel(val, db->ddr_reg[0] + off1);
	}
}

static void t6d_dmc_range_config(struct ddr_bandwidth *db, int channel,
				unsigned long start, unsigned long end)
{
	unsigned int off = 0;

	start >>= 12;
	end   >>= 12;

	off = DMC_MON0_STA + channel * 16;
	writel(start, db->ddr_reg[0] + off);	/* DMC_MON*_STA */
	writel(end, db->ddr_reg[0] + off + 4);	/* DMC_MON*_EDA */
}

static unsigned long t6d_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	db->ddr_freq = (readl(db->pll_reg) & 0xffff) * 1000000;
	db->dmc_freq = db->ddr_freq >> 1;

	return db->dmc_freq;
}

#define CONFIG_DDR0_16BIT_CH0			1
#define CONFIG_DDR0_16BIT_RANK01_CH0		4
static void t6d_bus_width_init(struct ddr_bandwidth *db)
{
	int i;
	unsigned int val;

	for (i = 0; i < db->dmc_number; i++) {
		if (db->bus_width_reg[i].io_addr) {
			val = readl(db->bus_width_reg[i].io_addr);
			val = val >> 16;
			if (val == CONFIG_DDR0_16BIT_CH0 || val == CONFIG_DDR0_16BIT_RANK01_CH0)
				db->bus_width[i] = 16;
			else
				db->bus_width[i] = 32;
		}
		pr_debug("dmc%d bus width is %d\n", i, db->bus_width[i]);
	}
}

static void t6d_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	/* enable all channel */
	val =  (db->mode ? BIT(31) : 0) |	/* enable bit */
	       (0xff <<  0);
	writel(val, db->ddr_reg[0] + DMC_MON_CTRL0);
}

static void t6d_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;

	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg[0] + DMC_MON_TIMER);

	t6d_dmc_bandwidth_enable(db);

	for (i = 0; i < db->channels; i++) {
		if (!ddr_get_ch_set_num(i))
			t6d_dmc_port_config(db, i, -1);
		t6d_dmc_range_config(db, i, 0, 0xffffffff);
		db->range[i].start = 0;
		db->range[i].end   = 0xffffffff;
	}
}

static int t6d_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, val, off;
	int ret = -1;

	val = readl(db->ddr_reg[0] + DMC_MON_CTRL0);
	if (val & DMC_QOS_IRQ) {
		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant    = readl(db->ddr_reg[0] + DMC_MON_ALL_BW);
		dg->all_grant16  = readl(db->ddr_reg[0] + DMC_MON_ALL16_BW);
		dg->all_grant   *= 16;
		dg->all_grant16 *= 16;
		for (i = 0; i < db->channels; i++) {
			off = i * 16 + DMC_MON0_BW;
			dg->channel_grant[i] = readl(db->ddr_reg[0] + off) * 16;
		}
		/* clear irq flags */
		writel(val, db->ddr_reg[0] + DMC_MON_CTRL0);
		t6d_dmc_bandwidth_enable(db);

		ret = 0;
	}
	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int t6d_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i;
	unsigned int r, off, off1;

	r  = readl(db->ddr_reg[0] + DMC_MON_CTRL0);
	s += sprintf(buf + s, "DMC_MON_CTRL0:        %08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_TIMER);
	s += sprintf(buf + s, "DMC_MON_TIMER:        %08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_ALL_IDLE_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_IDLE_CNT: %08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_ALL_BW);
	s += sprintf(buf + s, "DMC_MON_ALL_BW:       %08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_ALL16_BW);
	s += sprintf(buf + s, "DMC_MON_ALL16_BW:     %08x\n", r);

	for (i = 0; i < 8; i++) {
		off = i * 16 + DMC_MON0_STA;
		off1 = i * 4 + DMC_MON0_CTRL1;
		r  = readl(db->ddr_reg[0] + off);
		s += sprintf(buf + s, "DMC_MON%d_STA:         %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off + 4);
		s += sprintf(buf + s, "DMC_MON%d_EDA:         %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off + 8);
		s += sprintf(buf + s, "DMC_MON%d_CTRL:        %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off1);
		s += sprintf(buf + s, "DMC_MON%d_CTRL1:        %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off + 12);
		s += sprintf(buf + s, "DMC_MON%d_BW:          %08x\n", i, r);
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
		db->ost.regs[i].def_val = readl(db->ddr_reg[0] + db->ost.regs[i].offset);
		if (db->ost.levels.cur_level >= 0 && db->ost.levels.count) {
			writel(db->ost.levels.value[db->ost.levels.cur_level],
				db->ddr_reg[0] + db->ost.regs[i].offset);
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
		writel(db->ost.regs[bus].def_val, db->ddr_reg[0] + db->ost.regs[bus].offset);
	else
		writel(value, db->ddr_reg[0] + db->ost.regs[bus].offset);

	return 0;
}

static int outstanding_get(struct ddr_bandwidth *db, int bus)
{
	if (bus > db->bus_num)
		return -1;

	if (!db->ost.regs)
		return -1;

	return readl(db->ddr_reg[0] + db->ost.regs[bus].offset);
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
#define AXI_REGISTER_COUNT		((5 << 2))
#define SIDE_BAND_REG			((2 << 2))
#define SIDE_BAND_BLOCK_RW_OFFSET	8
#define SIDE_BAND_BLOCK_OFFSET		16
#define BUS_COUNT			9
static int side_band(struct ddr_bandwidth *db, unsigned char dmc, unsigned char bus)
{
	int i;
	u32 val;
	unsigned int reg;

	reg = DMC_AXI0_CHAN_CTRL + (((bus / 3) * 0x10) << 2) +
	      ((bus % 3) * AXI_REGISTER_COUNT) + SIDE_BAND_REG;
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

static int __init t6d_axi_sts_init(struct ddr_bandwidth *db)
{
	return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET,
				 axi_chan_sts_offset, ARRAY_SIZE(axi_chan_sts_offset));
}

struct ddr_bandwidth_ops t6d_ddr_bw_ops = {
	.init             = t6d_dmc_bandwidth_init,
	.config_port      = t6d_dmc_port_config,
	.config_range     = t6d_dmc_range_config,
	.get_freq         = t6d_get_dmc_freq_quick,
	.bus_width_init   = t6d_bus_width_init,
	.handle_irq       = t6d_handle_irq,
	.bandwidth_enable = t6d_dmc_bandwidth_enable,
	.outstanding_init = outstanding_init,
	.outstanding      = outstanding_handle,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = t6d_dump_reg,
#endif
	.property_access  = property_access,
	.side_band	  = side_band,
	.axi_sts_init     = t6d_axi_sts_init,
};
