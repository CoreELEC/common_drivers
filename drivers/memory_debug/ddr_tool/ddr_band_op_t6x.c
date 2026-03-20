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
#include <linux/sched/clock.h>

#define MDC_MON_CTRL0			((0x0413 << 2))
#define MDC_MON_TIMER			((0x0414 << 2))
#define MDC_MON_ALL_IDLE_CNT		((0x0415 << 2))
#define MDC_MON_ALL_BW			((0x0416 << 2))
#define MDC_MON_ALL16_BW		((0x0417 << 2))

#define MDC_CLKG_CTRL0			 ((0x0411 << 2))
#define MDC_SOFT_RST			 ((0x048f << 2))

#define DMC_CMD_FILTER_CTRL3		((0x0043  << 2))

#define DMC_CHAN_STS_OFFSET		((0x00cf  << 2))
static unsigned int axi_chan_sts_offset[] __initdata = {
	(0x84 << 2), (0x88 << 2), (0x8d << 2),
	(0x93 << 2), (0x98 << 2), (0x9c << 2),
	(0xa3 << 2), (0xa8 << 2), (0xad << 2),
	(0xb3 << 2), (0xb8 << 2), (0xbd << 2)
};

static unsigned int MDC_MON_STA[] = {
	(0x0418 << 2), (0x041d << 2), (0x0422 << 2), (0x0427 << 2),
	(0x0444 << 2), (0x0449 << 2), (0x044e << 2), (0x0453 << 2)
};

static unsigned int MDC_MON_EDA[] = {
	(0x0419 << 2), (0x041e << 2), (0x0423 << 2), (0x0440 << 2),
	(0x0445 << 2), (0x044a << 2), (0x044f << 2), (0x0454 << 2)
};

static unsigned int MDC_MON_CTRL[] = {
	(0x041a << 2), (0x041f << 2), (0x0424 << 2), (0x0441 << 2),
	(0x0446 << 2), (0x044b << 2), (0x0450 << 2), (0x0455 << 2)
};

static unsigned int MDC_MON_CTRL1[] = {
	(0x041b << 2), (0x0420 << 2), (0x0425 << 2), (0x0442 << 2),
	(0x0447 << 2), (0x044c << 2), (0x0451 << 2), (0x0456 << 2)
};

static unsigned int MDC_MON_BW[] = {
	(0x041c << 2), (0x0421 << 2), (0x0426 << 2), (0x0443 << 2),
	(0x0448 << 2), (0x044d << 2), (0x0452 << 2), (0x0457 << 2)
};

static unsigned int ots_regs_offset[] __initdata = {
	(0x0459 << 2), (0x0459 << 2), (0x0459 << 2),
	(0x0459 << 2), (0x045d << 2), (0x0461 << 2),
	(0x0465 << 2), (0x0481 << 2), (0x0485 << 2), (0x0489 << 2),
};

static unsigned int ots_levels[] __initdata = {
	0x02010201, 0x04020402, 0x06040604, 0x08050805,
	0x0C080C08, 0x120C120C, 0x18131813, 0x1C161C16,
	0x20192019, 0x241C241C, 0x28202820, 0x2C232C23,
	0x30263026, 0x34293429, 0x382C382C, 0x3C2F3C2F
};

static void t6x_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int val, index, mdc_select = 0;
	int i, subport = -1, id = -1;
	void *io;

	if (port >= 0) {
		if (port >= PORT_MAJOR) {
			id = 3;
			subport = port - PORT_MAJOR;
		} else {
			id = (port >> 3) & 0x3;
		}
		mdc_select = 1;
	}

	/* clear all port mask */
	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];

		if (port < 0) {
			writel(0, io + MDC_MON_CTRL[channel]);	/* MDC_MON*_CTRL */
			writel(0, io + MDC_MON_CTRL1[channel]);	/* MDC_MON*_CTRL1 */
		} else {
			if (mdc_select && i != id)	/* select mdc */
				continue;

			if (subport < 0) {
				val = readl(io + MDC_MON_CTRL[channel]);
				index = port & 0x7;  /* real mdc port id */
				val |= (1 << index);
				writel(val, io + MDC_MON_CTRL[channel]);	/* MDC_MON*_CTRL */
				val = 0xffffffff;
				writel(val, io + MDC_MON_CTRL1[channel]);	/* MDC_MON*_CTRL1 */
			} else {
				val = (0x1 << 3);	/* select mdc device */
				writel(val, io + MDC_MON_CTRL[channel]);
				val = readl(io + MDC_MON_CTRL1[channel]);
				val |= (1 << subport);
				writel(val, io + MDC_MON_CTRL1[channel]);
			}
		}
	}
}

static void t6x_range_config(struct ddr_bandwidth *db, int channel,
				unsigned long start, unsigned long end)
{
	int i;
	void *io;

	start >>= 12;
	end   >>= 12;

	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];

		writel(start, io + MDC_MON_STA[channel]);	/* MDC_MON*_STA */
		writel(end, io + MDC_MON_EDA[channel]);		/* MDC_MON*_EDA */
	}
}

static unsigned long t6x_get_freq_quick(struct ddr_bandwidth *db)
{
	db->ddr_freq = (readl(db->pll_reg) & 0xffff) * 1000000;
	db->dmc_freq = db->ddr_freq >> 1;

	return db->dmc_freq;
}

static void t6x_bus_width_init(struct ddr_bandwidth *db)
{
	int i;
	unsigned int val;

	for (i = 0; i < db->dmc_number; i++) {
		db->bus_width[i] = 0;
		if (db->bus_width_reg[i].io_addr) {
			val = readl(db->bus_width_reg[i].io_addr);
			if (val & 0xf0000000)
				db->bus_width[i] += 16;
			if (val & 0x0f000000)
				db->bus_width[i] += 16;
			if (val & 0x00f00000)
				db->bus_width[i] += 16;
			if (val & 0x000f0000)
				db->bus_width[i] += 16;
		}
		pr_debug("dmc%d bus width is %d\n", i, db->bus_width[i]);
	}
}

static void t6x_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val, i;
	void *io;

	/*  all mdc reset before */
	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];

		/* reset mon */
		val = readl(io + MDC_SOFT_RST);
		val &= ~BIT(1);
		writel(val, io + MDC_SOFT_RST);
		val |= BIT(1);
		writel(val, io + MDC_SOFT_RST);
	}

	/*  all mdc enabled again */
	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];

		if (db->mode) {
			/* enable all channel */
			val = BIT(31) | BIT(30) | 0xff;
			writel(val, io + MDC_MON_CTRL0);
		} else {
			/* disable all channel */
			val = BIT(30);
			writel(val, io + MDC_MON_CTRL0);
		}
	}
	db->last_poll_time = sched_clock();
}

static void t6x_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;
	void *io;

	for (i = 0; i < db->channels; i++) {
		if (!db->port[i])
			t6x_port_config(db, i, -1);
		t6x_range_config(db, i, 0, DEFAULT_RANGE_LARGE_DDR);
		db->range[i].start = 0;
		db->range[i].end   = DEFAULT_RANGE_LARGE_DDR;
	}

	/* set timer trigger clock_cnt */
	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];
		writel(0xffffffff, io + MDC_MON_TIMER);
	}
	t6x_bandwidth_enable(db);
	if (db->soc_feature & DDR_IS_POLL)
		smp_call_function_single(db->poll_cpu, ddr_poll_start, NULL, 1);
}

static int t6x_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, j, val[4];
	int ret = -1;
	void *io;

	/* disabled all mdc clock */
	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];
		val[i] = readl(io + MDC_CLKG_CTRL0);
		val[i] |= BIT(21);
		writel(val[i], io + MDC_CLKG_CTRL0);
	}

	db->poll_time = sched_clock() - db->last_poll_time;
	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];

		dg->all_grant += readl(io + MDC_MON_ALL_BW) * 16;
		dg->all_grant16  += readl(io + MDC_MON_ALL16_BW) * 16;
		for (j = 0; j < db->channels; j++)
			dg->channel_grant[j] += readl(io + MDC_MON_BW[j]) * 16;
		ret = 0;
	}

	/* enabled all mdc clock */
	for (i = 0; i < db->mdc.number; i++) {
		io = db->mdc.reg_base[i];
		val[i] &= ~BIT(21);
		writel(val[i], io + MDC_CLKG_CTRL0);
	}

	/* each register */
	t6x_bandwidth_enable(db);

	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int t6x_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i, d;
	unsigned int r;
	void *io;

	for (d = 0; d < db->mdc.number; d++) {
		io = db->mdc.reg_base[d];

		s += sprintf(buf + s, "\nMDC%d:\n", d);

		r  = readl(io + MDC_MON_CTRL0);
		s += sprintf(buf + s, "MDC_MON_CTRL0:        %08x\n", r);
		r  = readl(io + MDC_MON_TIMER);
		s += sprintf(buf + s, "MDC_MON_TIMER:        %08x\n", r);
		r  = readl(io + MDC_MON_ALL_IDLE_CNT);
		s += sprintf(buf + s, "MDC_MON_ALL_IDLE_CNT: %08x\n", r);
		r  = readl(io + MDC_MON_ALL_BW);
		s += sprintf(buf + s, "MDC_MON_ALL_BW:       %08x\n", r);
		r  = readl(io + MDC_MON_ALL16_BW);
		s += sprintf(buf + s, "MDC_MON_ALL16_BW:     %08x\n", r);

		for (i = 0; i < db->channels; i++) {
			r  = readl(io + MDC_MON_CTRL[i]);
			s += sprintf(buf + s, "MDC_MON%d_CTRL:        %08x\n", i, r);
			r  = readl(io + MDC_MON_CTRL1[i]);
			s += sprintf(buf + s, "MDC_MON%d_CTRL1:        %08x\n", i, r);
			r  = readl(io + MDC_MON_BW[i]);
			s += sprintf(buf + s, "MDC_MON%d_BW:          %08x\n", i, r);
		}
	}
	return s;
}
#endif

static int __init outstanding_init(struct ddr_bandwidth *db)
{
	int i;
	size_t count;
	void *reg_base = NULL;

	db->mdc.port_num = ARRAY_SIZE(ots_regs_offset);
	db->ost.levels.count = ARRAY_SIZE(ots_levels);

	count = db->ost.levels.count * sizeof(unsigned int);
	count += db->mdc.port_num * sizeof(struct outstanding_reg);
	db->ost.levels.value = kzalloc(count, GFP_KERNEL);
	if (!db->ost.levels.value) {
		kfree(db->ost.levels.value);
		db->mdc.port_num = 0;
		db->ost.levels.count = 0;
		return -ENOMEM;
	}
	db->ost.regs = (struct outstanding_reg *)(db->ost.levels.value + db->ost.levels.count);

	for (i = 0; i < db->ost.levels.count; i++)
		db->ost.levels.value[i] = ots_levels[i];

	for (i = 0; i < db->mdc.port_num; i++) {
		db->ost.regs[i].offset = ots_regs_offset[i];
		reg_base = db->mdc.reg_base[db->port_desc[i].mdc_id];
		if (!reg_base) {
			pr_err("outstanding reg base error\n");
			return -1;
		}

		db->ost.regs[i].def_val = readl(reg_base + db->ost.regs[i].offset);
		if (db->ost.levels.cur_level >= 0 && db->ost.levels.count) {
			writel(db->ost.levels.value[db->ost.levels.cur_level],
				reg_base + db->ost.regs[i].offset);
		}
	}

	return 0;
}

static int outstanding_set(struct ddr_bandwidth *db, int port_num, int value)
{
	unsigned char mdc_id;
	void *reg_base = NULL;

	if (port_num > db->mdc.port_num)
		return -1;

	if (!db->ost.regs)
		return -1;

	mdc_id = db->port_desc[port_num].mdc_id;
	reg_base = db->mdc.reg_base[mdc_id];

	if (!reg_base) {
		pr_err("outstanding reg base error\n");
		return -1;
	}

	if (value == -1)
		writel(db->ost.regs[port_num].def_val,
			reg_base + db->ost.regs[port_num].offset);
	else
		writel(value, reg_base + db->ost.regs[port_num].offset);

	return 0;
}

static int outstanding_get(struct ddr_bandwidth *db, int port_num)
{
	unsigned char mdc_id;
	void *reg_base = NULL;

	if (port_num > db->mdc.port_num)
		return -1;

	if (!db->ost.regs)
		return -1;

	mdc_id = db->port_desc[port_num].mdc_id;
	reg_base = db->mdc.reg_base[mdc_id];

	if (!reg_base) {
		pr_err("outstanding reg base error\n");
		return -1;
	}

	return readl(reg_base + db->ost.regs[port_num].offset);
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
#define BUS_COUNT			4
static int side_band(struct ddr_bandwidth *db, unsigned char dmc, unsigned char bus)
{
	int i;
	u32 val;
	unsigned int reg;

	reg = DMC_AXI0_CHAN_CTRL + (((bus / 3) * 0x10) << 2)
		+ ((bus % 3) * AXI_REGISTER_COUNT) + SIDE_BAND_REG;
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

#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
#define DMC0_DDR_PLL_CNTL1	(0xfe340000 + (0x301 << 2))
#define DMC0_DDR_PLL_CNTL2	(0xfe340000 + (0x302 << 2))
#define DMC0_DDR_PLL_CNTL6	(0xfe340000 + (0x306 << 2))
#define DMC1_DDR_PLL_CNTL1	(0xfe300000 + (0x301 << 2))
#define DMC1_DDR_PLL_CNTL2	(0xfe300000 + (0x302 << 2))
#define DMC1_DDR_PLL_CNTL6	(0xfe300000 + (0x306 << 2))

static int t6x_ddr_ssr_enable_set(unsigned char value)
{
	unsigned int dmc0_cnt1, dmc1_cnt1;

	dmc0_cnt1 = ddr_ssr_access_sec(DMC0_DDR_PLL_CNTL1, 0x0, DMC_READ);
	dmc1_cnt1 = ddr_ssr_access_sec(DMC1_DDR_PLL_CNTL1, 0x0, DMC_READ);

	if (value) {
		dmc0_cnt1 |= BIT(23);
		dmc1_cnt1 |= BIT(23);
	} else {
		dmc0_cnt1 &= ~BIT(23);
		dmc1_cnt1 &= ~BIT(23);
	}

	ddr_ssr_access_sec(DMC0_DDR_PLL_CNTL1, dmc0_cnt1, DMC_WRITE);
	ddr_ssr_access_sec(DMC1_DDR_PLL_CNTL1, dmc1_cnt1, DMC_WRITE);

	return 0;
}

static int __init t6x_ddr_ssr_enable_get(void)
{
	unsigned int dmc0_cnt1, dmc1_cnt1;

	dmc0_cnt1 = ddr_ssr_access_sec(DMC0_DDR_PLL_CNTL1, 0x0, DMC_READ);
	dmc1_cnt1 = ddr_ssr_access_sec(DMC1_DDR_PLL_CNTL1, 0x0, DMC_READ);

	if (dmc0_cnt1 & dmc1_cnt1 & BIT(23))
		return 1;

	return 0;
}

static int t6x_ddr_ssr_fmod_set(unsigned char value)
{
	return 0;
}

static int t6x_ddr_ssr_fmod_get(void)
{
	return 0;
}

static int t6x_ddr_ssr_amplitude_set(unsigned char value)
{
	unsigned int dmc0_cnt2, dmc1_cnt2, status;

	if (value < 1 || value > 10) {
		pr_err("amplitude set %d is invalid\n", value);
		return -EINVAL;
	}

	status = t6x_ddr_ssr_enable_get();
	if (status)
		t6x_ddr_ssr_enable_set(0);

	dmc0_cnt2 = ddr_ssr_access_sec(DMC0_DDR_PLL_CNTL2, 0x0, DMC_READ);
	dmc1_cnt2 = ddr_ssr_access_sec(DMC1_DDR_PLL_CNTL2, 0x0, DMC_READ);

	dmc0_cnt2 &= ~(0xf << 4);
	dmc1_cnt2 &= ~(0xf << 4);

	dmc0_cnt2 |= (0x2 << 4);
	dmc1_cnt2 |= (0x2 << 4);

	dmc0_cnt2 &= ~(0xf << 8);
	dmc1_cnt2 &= ~(0xf << 8);

	dmc0_cnt2 |= (value << 8);
	dmc1_cnt2 |= (value << 8);

	ddr_ssr_access_sec(DMC0_DDR_PLL_CNTL2, dmc0_cnt2, DMC_WRITE);
	ddr_ssr_access_sec(DMC1_DDR_PLL_CNTL2, dmc1_cnt2, DMC_WRITE);

	if (status)
		t6x_ddr_ssr_enable_set(1);

	return 0;
}

static int t6x_ddr_ssr_amplitude_get(void)
{
	unsigned int dmc0_cnt2, dmc1_cnt2, val0, val1;

	dmc0_cnt2 = ddr_ssr_access_sec(DMC0_DDR_PLL_CNTL2, 0x0, DMC_READ);
	dmc1_cnt2 = ddr_ssr_access_sec(DMC1_DDR_PLL_CNTL2, 0x0, DMC_READ);

	val0 = ((dmc0_cnt2 >> 4) & 0xf) * 500;
	val1 = ((dmc1_cnt2 >> 4) & 0xf) * 500;

	val0 = val0 * (dmc0_cnt2 >> 8 & 0xf) / 1000;
	val1 = val1 * (dmc1_cnt2 >> 8 & 0xf) / 1000;

	if (val0 != val1)
		pr_err("amplitude is not same, dmc0:%d, dmc1:%d\n", val0, val1);

	return val0;
}

static int t6x_ddr_ssr_control(unsigned char value, enum ddr_ssr_type type)
{
	int ret = 0;

	switch (type) {
	case ENABLE_SET:
		ret = t6x_ddr_ssr_enable_set(value);
		break;
	case FMOD_SET:
		ret = t6x_ddr_ssr_fmod_set(value);
		break;
	case AMPLITUDE_SET:
		ret = t6x_ddr_ssr_amplitude_set(value);
		break;
	case ENABLE_GET:
		ret = t6x_ddr_ssr_enable_get();
		break;
	case FMOD_GET:
		ret = t6x_ddr_ssr_fmod_get();
		break;
	case AMPLITUDE_GET:
		ret = t6x_ddr_ssr_amplitude_get();
		break;
	default:
		pr_err("invalid ssr handle\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}
#endif

static int __init t6x_axi_sts_init(struct ddr_bandwidth *db)
{
	return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET,
				 axi_chan_sts_offset, ARRAY_SIZE(axi_chan_sts_offset));
}

struct ddr_bandwidth_ops t6x_ddr_bw_ops = {
	.init             = t6x_bandwidth_init,
	.config_port      = t6x_port_config,
	.config_range     = t6x_range_config,
	.get_freq         = t6x_get_freq_quick,
	.bus_width_init   = t6x_bus_width_init,
	.handle_irq       = t6x_handle_irq,
	.bandwidth_enable = t6x_bandwidth_enable,
	.outstanding_init = outstanding_init,
	.outstanding      = outstanding_handle,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = t6x_dump_reg,
#endif
	.property_access  = property_access,
	.side_band	  = side_band,
#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
	.ddr_ssr_control  = t6x_ddr_ssr_control,
#endif
	.axi_sts_init     = t6x_axi_sts_init,
};
