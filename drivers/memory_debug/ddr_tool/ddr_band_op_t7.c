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

#define DMC_CMD_FILTER_CTRL3		((0x0042 << 2))

#define DMC_CHAN_STS_OFFSET		((0x00bc  << 2))
static unsigned int axi_chan_sts_offset[] __initdata = {
	(0xa0 << 2), (0xa1 << 2), (0xa2 << 2), (0xa3 << 2),
	(0xa4 << 2), (0xa5 << 2), (0xa6 << 2), (0xa7 << 2)
};

static unsigned int axi_chan_sts_offset_p1[] __initdata = {
	(0xa0 << 2), (0xa1 << 2), (0xa2 << 2),
	(0xa3 << 2), (0xa4 << 2), (0xa5 << 2),
	(0xa6 << 2), (0xa7 << 2), (0xa8 << 2)

};

static void t7_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int val, off;
	int i;
	void *io;

	for (i = 0; i < db->dmc_number; i++) {
		io = db->ddr_reg[i];
		off = DMC_MON0_CTRL + channel * 16;
		if (port < 0) {
			/* clear all port mask */
			writel(0, io + off);
		} else {
			val  =  (1 << 31) | (0x03 << 29) | (0x7 << 24);
			val |= port;
			writel(val, io + off);	/* DMC_MON*_CTRL */
		}
	}
}

static void t7_dmc_range_config(struct ddr_bandwidth *db, int channel,
				unsigned long start, unsigned long end)
{
	unsigned int off = 0;
	int i;
	void *io;

	start >>= 12;
	end   >>= 12;
	for (i = 0; i < db->dmc_number; i++) {
		io = db->ddr_reg[i];
		off = DMC_MON0_STA + channel * 16;
		writel(start, io + off);	/* DMC_MON*_STA */
		writel(end, io + off + 4);	/* DMC_MON*_EDA */
	}
}

static unsigned long t7_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	unsigned int val;
	unsigned int n, m, od1;
	unsigned int od_div = 0xfff;
	unsigned long freq = 0;

	val = readl(db->pll_reg);

	val = val & 0xfffff;
	if (db->cpu_type == DMC_TYPE_P1) {
		switch ((val >> 16) & 7) {
		case 0:
			od_div = 1;
			break;

		case 1:
			od_div = 2;
			break;

		case 2:
			od_div = 3;
			break;

		case 3:
			od_div = 4;
			break;

		case 4:
			od_div = 6;
			break;

		case 5:
			od_div = 8;
			break;

		default:
			break;
		}
	} else {
		switch ((val >> 16) & 7) {
		case 0:
			od_div = 2;
			break;

		case 1:
			od_div = 3;
			break;

		case 2:
			od_div = 4;
			break;

		case 3:
			od_div = 6;
			break;

		case 4:
			od_div = 8;
			break;

		default:
			break;
		}
	}
	m = val & 0x1ff;
	n = ((val >> 10) & 0x1f);
	od1 = (((val >> 19) & 0x1)) == 1 ? 2 : 1;
	freq = DEFAULT_XTAL_FREQ / 1000;	/* avoid overflow */
	if (n) {
		freq = ((((freq * m) / n) >> od1) / od_div) * 1000;
	}

	db->dmc_freq = freq;

	if (db->cpu_type == DMC_TYPE_P1)
		db->ddr_freq = db->dmc_freq * 4;
	else
		db->ddr_freq = db->dmc_freq * 2;

	return freq;
}

static void t7_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int i;
	unsigned long val;
	void *io;

	for (i = db->dmc_number; i > 0; i--) {
		io = db->ddr_reg[i - 1];
		val = db->mode ?  BIT(31) | DMC_QOS_IRQ : DMC_QOS_IRQ;
		val |= (readl(io + DMC_MON_CTRL0) & ~BIT(31));
		writel(val, io + DMC_MON_CTRL0);
	}
}

static void t7_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;
	void *io;

	/* set timer trigger clock_cnt */
	for (i = 0; i < db->dmc_number; i++) {
		io = db->ddr_reg[i];
		writel(db->clock_count, io + DMC_MON_TIMER);
	}
	t7_dmc_bandwidth_enable(db);

	for (i = 0; i < db->channels; i++) {
		if (!db->port[i])
			t7_dmc_port_config(db, i, -1);
		t7_dmc_range_config(db, i, 0, DEFAULT_RANGE_LARGE_DDR);
		db->range[i].start = 0;
		db->range[i].end   = DEFAULT_RANGE_LARGE_DDR;
	}
}

static int t7_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, val, off, d;
	void *io;
	int ret = -1;

	for (d = 0; d < db->dmc_number; d++) {
		io = db->ddr_reg[d];
		do {
			val = readl(io + DMC_MON_CTRL0);
			if (val & DMC_QOS_IRQ)
				break;
		} while (1);

		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant    += readl(io + DMC_MON_ALL_BW);
		dg->all_grant16  += readl(io + DMC_MON_ALL16_BW);
		for (i = 0; i < db->channels; i++) {
			off = i * 16 + DMC_MON0_BW;
			dg->channel_grant[i] += readl(io + off);
		}

		ret = 0;
	}
	t7_dmc_bandwidth_enable(db);

	/* each register */
	dg->all_grant   *= 16;
	dg->all_grant16 *= 16;
	for (i = 0; i < db->channels; i++)
		dg->channel_grant[i] *= 16;

	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int t7_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i, d;
	unsigned int r, off;
	void *io;

	for (d = 0; d < db->dmc_number; d++) {
		io = db->ddr_reg[d];
		s += sprintf(buf + s, "\nDMC%d:\n", d);

		r  = readl(io + DMC_MON_CTRL0);
		s += sprintf(buf + s, "DMC_MON_CTRL0:        %08x\n", r);
		r  = readl(io + DMC_MON_TIMER);
		s += sprintf(buf + s, "DMC_MON_TIMER:        %08x\n", r);
		r  = readl(io + DMC_MON_ALL_IDLE_CNT);
		s += sprintf(buf + s, "DMC_MON_ALL_IDLE_CNT: %08x\n", r);
		r  = readl(io + DMC_MON_ALL_BW);
		s += sprintf(buf + s, "DMC_MON_ALL_BW:       %08x\n", r);
		r  = readl(io + DMC_MON_ALL16_BW);
		s += sprintf(buf + s, "DMC_MON_ALL16_BW:     %08x\n", r);

		for (i = 0; i < 8; i++) {
			off = i * 16 + DMC_MON0_STA;
			r  = readl(io + off);
			s += sprintf(buf + s, "DMC_MON%d_STA:         %08x\n", i, r);
			r  = readl(io + off + 4);
			s += sprintf(buf + s, "DMC_MON%d_EDA:         %08x\n", i, r);
			r  = readl(io + off + 8);
			s += sprintf(buf + s, "DMC_MON%d_CTRL:        %08x\n", i, r);
			r  = readl(io + off + 12);
			s += sprintf(buf + s, "DMC_MON%d_BW:          %08x\n", i, r);
		}
	}
	return s;
}
#endif

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
#define SIDE_BAND_BLOCK_RW_OFFSET	16
#define SIDE_BAND_BLOCK_OFFSET		0
#define BUS_COUNT			8
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

static int __init t7_axi_sts_init(struct ddr_bandwidth *db)
{
	if (db->cpu_type == DMC_TYPE_P1) {
		return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET,
				 axi_chan_sts_offset_p1, ARRAY_SIZE(axi_chan_sts_offset_p1));
	} else {
		return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET,
				 axi_chan_sts_offset, ARRAY_SIZE(axi_chan_sts_offset));
	}
}

struct ddr_bandwidth_ops t7_ddr_bw_ops = {
	.init             = t7_dmc_bandwidth_init,
	.config_port      = t7_dmc_port_config,
	.config_range     = t7_dmc_range_config,
	.get_freq         = t7_get_dmc_freq_quick,
	.handle_irq       = t7_handle_irq,
	.bandwidth_enable = t7_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = t7_dump_reg,
#endif
	.property_access  = property_access,
	.side_band	  = side_band,
	.axi_sts_init     = t7_axi_sts_init,
};
