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

#define DMC_MON_CTRL0			((0x0020  << 2))
#define DMC_MON_TIMER			((0x0021  << 2))
#define DMC_MON_ALL_IDLE_CNT		((0x0022  << 2))
#define DMC_MON_ALL_BW			((0x0023  << 2))
#define DMC_MON_ALL16_BW		((0x0024  << 2))

#define DMC_MON0_CTRL			((0x0030  << 2))
#define DMC_MON0_CTRL1			((0x0031  << 2))
#define DMC_MON0_CTRL2			((0x0032  << 2))
#define DMC_MON0_BW			((0x0033  << 2))

#define DMC_MON1_CTRL			((0x0034  << 2))
#define DMC_MON1_CTRL1			((0x0035  << 2))
#define DMC_MON1_CTRL2			((0x0036  << 2))
#define DMC_MON1_BW			((0x0037  << 2))

#define DMC_MON2_CTRL			((0x0038  << 2))
#define DMC_MON2_CTRL1			((0x0039  << 2))
#define DMC_MON2_CTRL2			((0x003a  << 2))
#define DMC_MON2_BW			((0x003b  << 2))

#define DMC_MON3_CTRL			((0x003c  << 2))
#define DMC_MON3_CTRL1			((0x003d  << 2))
#define DMC_MON3_CTRL2			((0x003e  << 2))
#define DMC_MON3_BW			((0x003f  << 2))

#define DMC_MON4_CTRL			((0x00c0  << 2))
#define DMC_MON4_CTRL1			((0x00c1  << 2))
#define DMC_MON4_CTRL2			((0x00c2  << 2))
#define DMC_MON4_BW			((0x00c3  << 2))

#define DMC_MON5_CTRL			((0x00c4  << 2))
#define DMC_MON5_CTRL1			((0x00c5  << 2))
#define DMC_MON5_CTRL2			((0x00c6  << 2))
#define DMC_MON5_BW			((0x00c7  << 2))

#define DMC_MON6_CTRL			((0x00c8  << 2))
#define DMC_MON6_CTRL1			((0x00c9  << 2))
#define DMC_MON6_CTRL2			((0x00ca  << 2))
#define DMC_MON6_BW			((0x00cb  << 2))

#define DMC_MON7_CTRL			((0x00cc  << 2))
#define DMC_MON7_CTRL1			((0x00cd  << 2))
#define DMC_MON7_CTRL2			((0x00ce  << 2))
#define DMC_MON7_BW			((0x00cf  << 2))

#define DMC_CMD_FILTER_CTRL3		((0x0042  << 2))

#define DMC_CHAN_STS_OFFSET		((0x00bc  << 2))

static void t5_dmc_port_config(struct ddr_bandwidth *db, int channel, int map_id)
{
	unsigned int val;
	unsigned int off = 0;
	int port, sub;

	if (channel < 4)
		off = channel * 16 + DMC_MON0_CTRL;
	else
		off = (channel - 4) * 16 + DMC_MON4_CTRL;

	/* clear all port mask */
	if (map_id < 0) {
		writel(0, db->ddr_reg[0] + off + 4);	/* DMC_MON*_CTRL1 */
		writel(0, db->ddr_reg[0] + off + 8);	/* DMC_MON*_CTRL2 */
		return;
	}

	port = db->port_map_table[map_id].port_id;
	sub = db->port_map_table[map_id].sub_id;

	if (sub < 0) {
		val = readl(db->ddr_reg[0] + off + 4);
		val |=  (1 << port);
		writel(val, db->ddr_reg[0] + off + 4);	/* DMC_MON*_CTRL1 */
		val = 0xffff;
		writel(val, db->ddr_reg[0] + off + 8);	/* DMC_MON*_CTRL2 */
	} else {
		val = (0x1 << port);	/* select major port */
		writel(val, db->ddr_reg[0] + off + 4);
		val = readl(db->ddr_reg[0] + off + 8);
		val |= (1 << sub);
		writel(val, db->ddr_reg[0] + off + 8);
	}
}

static void t5_dmc_range_config(struct ddr_bandwidth *db, int channel,
				unsigned long start, unsigned long end)
{
	unsigned int val;
	unsigned int off = 0;

	if (channel < 4)
		off = channel * 16 + DMC_MON0_CTRL;
	else
		off = (channel - 4) * 16 + DMC_MON4_CTRL;

	val = (start >> 16) | (end & 0xffff0000);
	writel(val, db->ddr_reg[0] + off);			/* DMC_MON*_CTRL */
}

static unsigned long t5_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	unsigned int val;
	unsigned int n, m, od1;
	unsigned int od_div = 0xfff;
	unsigned long freq = 0;

	val = readl(db->pll_reg);
	val = val & 0xfffff;
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

	m = val & 0x1ff;
	n = ((val >> 10) & 0x1f);
	od1 = (((val >> 19) & 0x1)) == 1 ? 2 : 1;
	freq = DEFAULT_XTAL_FREQ / 1000;	/* avoid overflow */
	if (n)
		freq = ((((freq * m) / n) >> od1) / od_div) * 1000;

	db->dmc_freq = freq;
	db->ddr_freq = freq * 2;

	return freq;
}

static void t5_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	/* enable all channel */
	val =  (db->mode << 31) |	/* enable bit */
	       (0x01 << 20) |	/* use timer  */
	       (0xff <<  0);

	val |= (readl(db->ddr_reg[0] + DMC_MON_CTRL0) & ~BIT(31));
	writel(val, db->ddr_reg[0] + DMC_MON_G12_CTRL0);
}

static void t5_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;

	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg[0] + DMC_MON_TIMER);
	t5_dmc_bandwidth_enable(db);

	for (i = 0; i < db->channels; i++) {
		if (!ddr_get_ch_set_num(i))
			t5_dmc_port_config(db, i, -1);
		t5_dmc_range_config(db, i, 0, 0xffffffff);
		db->range[i].start = 0;
		db->range[i].end   = 0xffffffff;
	}
}

static int t5_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
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
			if (i < 4)
				off = i * 16 + DMC_MON0_BW;
			else
				off = (i - 4) * 16 + DMC_MON4_BW;
			dg->channel_grant[i] = readl(db->ddr_reg[0] + off) * 16;
		}
		/* clear irq flags */
		t5_dmc_bandwidth_enable(db);

		ret = 0;
	}
	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int t5_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i;
	unsigned int r, off;

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
		if (i < 4)
			off = i * 16 + DMC_MON0_CTRL;
		else
			off = (i - 4) * 16 + DMC_MON4_CTRL;

		r  = readl(db->ddr_reg[0] + off);
		s += sprintf(buf + s, "DMC_MON%d_CTRL:        %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off + 4);
		s += sprintf(buf + s, "DMC_MON%d_CTRL1:       %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off + 8);
		s += sprintf(buf + s, "DMC_MON%d_CTRL2:       %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off + 12);
		s += sprintf(buf + s, "DMC_MON%d_BW:          %08x\n", i, r);
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

#define AM_REGISTER_COUNT		((4 << 2))
#define SIDE_BAND_REG			((2 << 2))
#define SIDE_BAND_BLOCK_ENABLE		31
#define BUS_COUNT			24
static int side_band(struct ddr_bandwidth *db, unsigned char dmc, unsigned char bus)
{
	int i;
	u32 val;
	unsigned int reg;

	reg = DMC_AM0_CHAN_CTRL + AM_REGISTER_COUNT * (bus - 16) + SIDE_BAND_REG;
	if (db->dmc_bus[dmc].bus[bus].side_band.flags) {
		u32 val0 = 0;

		for (i = 0, val = 0; i < db->dmc_bus[dmc].bus[bus].side_band.block_num; i++)
			val |= 1 << db->dmc_bus[dmc].bus[bus].side_band.block_bus[i];

		if (db->dmc_bus[dmc].bus[bus].side_band.rw & 1)
			all_dmc_reg_field_access(db, &val, WRITE, reg, 0, BUS_COUNT);
		else
			all_dmc_reg_field_access(db, &val0, WRITE, reg, 0, BUS_COUNT);
		if (db->dmc_bus[dmc].bus[bus].side_band.rw & 2)
			all_dmc_reg_field_access(db, &val, WRITE, reg + 4, 0, BUS_COUNT);
		else
			all_dmc_reg_field_access(db, &val0, WRITE, reg + 4, 0, BUS_COUNT);

		val = 1;
		all_dmc_reg_field_access(db, &val, WRITE, reg, SIDE_BAND_BLOCK_ENABLE, 1);
	} else {
		val = 0;
		all_dmc_reg_field_access(db, &val, WRITE, reg, SIDE_BAND_BLOCK_ENABLE, 1);
		all_dmc_reg_field_access(db, &val, WRITE, reg, 0, BUS_COUNT);
		all_dmc_reg_field_access(db, &val, WRITE, reg + 4, 0, BUS_COUNT);
	}

	return 0;
}

static int __init t5_axi_sts_init(struct ddr_bandwidth *db)
{
	return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET, NULL, 0);
}

struct ddr_bandwidth_ops t5_ddr_bw_ops = {
	.init             = t5_dmc_bandwidth_init,
	.config_port      = t5_dmc_port_config,
	.config_range     = t5_dmc_range_config,
	.get_freq         = t5_get_dmc_freq_quick,
	.handle_irq       = t5_handle_irq,
	.bandwidth_enable = t5_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = t5_dump_reg,
#endif
	.property_access  = property_access,
	.side_band	  = side_band,
	.axi_sts_init     = t5_axi_sts_init,
};
