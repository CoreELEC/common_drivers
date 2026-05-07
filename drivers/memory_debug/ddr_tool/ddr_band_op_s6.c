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

#define DMC_CHAN_STS_OFFSET		((0x00d8  << 2))
static unsigned int axi_chan_sts_offset[] __initdata = {
	(0xc0 << 2), (0xc1 << 2), (0xc2 << 2), (0xc3 << 2),
	(0xc4 << 2), (0xc5 << 2), (0xc6 << 2), (0xc7 << 2),
	(0xc8 << 2), (0xc9 << 2), (0xca << 2), (0xcb << 2),
	(0xcc << 2), (0xcd << 2), (0xce << 2), (0xcf << 2),
	(0xd0 << 2), (0xd1 << 2), (0xd2 << 2), (0xd3 << 2),
	(0xd4 << 2), (0xd5 << 2), (0xd6 << 2), (0xd7 << 2)
};

static void s6_dmc_port_config(struct ddr_bandwidth *db, int channel, int map_id)
{
	unsigned int i, val;

	pr_info("S6 can not set port, default be set\n");

	/* s6 can not se port, default be set */
	for (i = 0; i < db->channels; i++) {
		ddr_set_map_select(i, -1);
		ddr_set_map_select(i, port_to_map_id(i, -1));
		db->range[i].start = 0;
		db->range[i].end   = DEFAULT_RANGE_LARGE_DDR;
	}

	val = readl(db->ddr_reg[0] + DMC_MON_CTRL0);
	val |= 0xffffff;
	writel(val,  db->ddr_reg[0] + DMC_MON_CTRL0);
}

static unsigned long s6_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	if ((readl(db->ddr_reg[0] + DDR_TYPE) & 0xff00) == 0x600) {
		db->ddr_type = LPDDR5;
		db->dmc_freq = (readl(db->pll_reg) & 0xffff) * 1000000;
		db->ddr_freq = db->dmc_freq;
	} else {
		db->ddr_freq = (readl(db->pll_reg) & 0xffff) * 1000000;
		db->dmc_freq = db->ddr_freq >> 2;
	}

	return db->dmc_freq;
}

static void s6_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	void *io;
	unsigned int i, val;

	for (i = 0; i < db->dmc_number; i++) {
		io = db->ddr_reg[i];
		val = db->mode ? BIT(31) : 0;
		val |= (readl(io + DMC_MON_CTRL0) & ~BIT(31));
		writel(val, io + DMC_MON_CTRL0);
	}
}

static void s6_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;
	void *io;

	/* set timer trigger clock_cnt */
	for (i = 0; i < db->dmc_number; i++) {
		io = db->ddr_reg[i];
		writel(db->clock_count, io + DMC_MON_TIMER);
	}
	s6_dmc_bandwidth_enable(db);

	s6_dmc_port_config(db, -1, -1);
}

static int s6_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, val, off, d;
	void *io;
	int ret = -1;

	for (d = 0; d < db->dmc_number; d++) {
		io = db->ddr_reg[d];
		val = readl(io + DMC_MON_CTRL0);
		if (val & DMC_QOS_IRQ) {
			/*
			 * get total bytes by each channel, each cycle 16 bytes;
			 */
			dg->all_grant    += readl(io + DMC_MON_ALL_BW);
			dg->all_grant16  += readl(io + DMC_MON_ALL16_BW);
			for (i = 0; i < db->channels; i++) {
				off = i * 8 + DMC_AXI0_READ_BW;
				dg->channel_grant[i] += readl(io + off);
				dg->channel_grant[i] += readl(io + off + 4);
			}
			ret = 0;
		}
	}

	if (!ret) {
		/* clear irq flags */
		s6_dmc_bandwidth_enable(db);

		/* each register */
		dg->all_grant   *= 16;
		dg->all_grant16 *= 16;
		for (i = 0; i < db->channels; i++)
			dg->channel_grant[i] *= 16;
	}

	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int s6_dump_reg(struct ddr_bandwidth *db, char *buf)
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

		for (i = 0; i < db->channels; i++) {
			off = i * 8 + DMC_AXI0_READ_BW;
			r  = readl(io + off);
			s += sprintf(buf + s, "DMC_AXI%d_READ_BW:          %08x\n", i, r);
			r  = readl(io + off + 4);
			s += sprintf(buf + s, "DMC_AXI%d_WRITE_BW:          %08x\n", i, r);
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
#define SIDE_BAND_BLOCK_RW_OFFSET	28
#define SIDE_BAND_BLOCK_OFFSET		8
#define BUS_COUNT			20
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

static int __init s6_axi_sts_init(struct ddr_bandwidth *db)
{
	return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET,
				 axi_chan_sts_offset, ARRAY_SIZE(axi_chan_sts_offset));
}

struct ddr_bandwidth_ops s6_ddr_bw_ops = {
	.init             = s6_dmc_bandwidth_init,
	.config_port      = s6_dmc_port_config,
	.get_freq         = s6_get_dmc_freq_quick,
	.handle_irq       = s6_handle_irq,
	.bandwidth_enable = s6_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = s6_dump_reg,
#endif
	.property_access  = property_access,
	.side_band	  = side_band,
	.axi_sts_init     = s6_axi_sts_init,
};
