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

#define CLKTREE_DMC_CLK_CTRL	((0x003e  << 2))
#define DMC_CMD_FILTER_CTRL3	((0x0042  << 2))

#define DMC_CHAN_STS_OFFSET		((0x0036  << 2))

static void a1_dmc_port_config(struct ddr_bandwidth *db, int channel, int port)
{
	unsigned int val;
	unsigned int rp[MAX_CHANNEL] = {DMC_MON_G12_CTRL1, DMC_MON_G12_CTRL3};
	unsigned int rs[MAX_CHANNEL] = {DMC_MON_G12_CTRL2, DMC_MON_G12_CTRL4};
	int subport = -1;

	/* clear all port mask */
	if (port < 0) {
		writel(0, db->ddr_reg[0] + rp[channel]);
		writel(0, db->ddr_reg[0] + rs[channel]);
		return;
	}

	if (port >= PORT_MAJOR)
		subport = port - PORT_MAJOR;
	if (subport < 0) {	/* using main port */
		val = readl(db->ddr_reg[0] + rp[channel]);
		val |=  (1 << port);
		writel(val, db->ddr_reg[0] + rp[channel]);
		val = 0xff;
		writel(val, db->ddr_reg[0] + rs[channel]);
	} else {
		val = (0x1 << 4);	/* select device */
		writel(val, db->ddr_reg[0] + rp[channel]);
		val = readl(db->ddr_reg[0] + rs[channel]);
		val |= (1 << subport);
		writel(val, db->ddr_reg[0] + rs[channel]);
	}
}

static unsigned long a1_get_dmc_freq_quick(struct ddr_bandwidth *db)
{
	unsigned long fclk, val, div;
	unsigned long freq = 0;

	val = readl(db->pll_reg + CLKTREE_DMC_CLK_CTRL);
	if (val & BIT(15))
		return DEFAULT_XTAL_FREQ;

	switch ((val >> 9) & 0x03) {	/* fixed clock is 1536 MHz */
	case 0:
		fclk = 768000000UL;	/* div 2 */
		break;
	case 1:
		fclk = 512000000UL;	/* div 3 */
		break;
	case 2:
		fclk = 307200000UL;	/* div 5 */
		break;
	default:			/* invalid */
		return 0;
	}
	if (val & 0x100) {		/* gate enabled */
		div = (val & 0xff) + 1;
		freq = fclk / div;
		pr_debug("%s, val:%lx, freq:%ld\n", __func__, val, freq);
	}

	db->dmc_freq = freq;
	db->ddr_freq = freq * 2;

	return freq;
}

static void a1_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	/* enable all channel */
	val =  (db->mode << 31) |	/* enable bit */
	       (0x01 << 20) |	/* use timer  */
	       (0x03 <<  0);

	val |= (readl(db->ddr_reg[0] + DMC_MON_G12_CTRL0) & ~BIT(31));
	writel(val, db->ddr_reg[0] + DMC_MON_G12_CTRL0);
}

static void a1_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	unsigned int i;

	/* set timer trigger clock_cnt */
	writel(db->clock_count, db->ddr_reg[0] + DMC_MON_G12_TIMER);
	a1_dmc_bandwidth_enable(db);

	/* clear each port setting */
	for (i = 0; i < db->channels; i++) {
		if (!db->port[i])
			a1_dmc_port_config(db, i, -1);
	}
}

static int a1_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int reg, i, val;
	int ret = -1;

	val = readl(db->ddr_reg[0] + DMC_MON_G12_CTRL0);
	if (val & DMC_QOS_IRQ) {
		/*
		 * get total bytes by each channel, each cycle 16 bytes;
		 */
		dg->all_grant = readl(db->ddr_reg[0] + DMC_MON_G12_ALL_GRANT_CNT);
		dg->all_req   = readl(db->ddr_reg[0] + DMC_MON_G12_ALL_REQ_CNT);
		dg->all_grant *= 16;
		dg->all_req   *= 16;
		for (i = 0; i < db->channels; i++) {
			reg = DMC_MON_G12_ONE_GRANT_CNT + (i << 2);
			dg->channel_grant[i] = readl(db->ddr_reg[0] + reg) * 16;
		}
		/* clear irq flags */
		a1_dmc_bandwidth_enable(db);

		ret = 0;
	}
	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int a1_dump_reg(struct ddr_bandwidth *db, char *buf)
{
	int s = 0, i;
	unsigned int r;

	for (i = 0; i < 5; i++) {
		r  = readl(db->ddr_reg[0] + (DMC_MON_G12_CTRL0 + (i << 2)));
		s += sprintf(buf + s, "DMC_MON_CTRL%d:        %08x\n", i, r);
	}
	r  = readl(db->ddr_reg[0] + DMC_MON_G12_ALL_REQ_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_REQ_CNT:  %08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_G12_ALL_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ALL_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_G12_ONE_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_ONE_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_G12_SEC_GRANT_CNT);
	s += sprintf(buf + s, "DMC_MON_SEC_GRANT_CNT:%08x\n", r);
	r  = readl(db->ddr_reg[0] + DMC_MON_G12_TIMER);
	s += sprintf(buf + s, "DMC_MON_TIMER:        %08x\n", r);

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

static int __init a1_axi_sts_init(struct ddr_bandwidth *db)
{
	return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET, NULL, 0);
}

struct ddr_bandwidth_ops a1_ddr_bw_ops = {
	.init             = a1_dmc_bandwidth_init,
	.config_port      = a1_dmc_port_config,
	.get_freq         = a1_get_dmc_freq_quick,
	.handle_irq       = a1_handle_irq,
	.bandwidth_enable = a1_dmc_bandwidth_enable,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = a1_dump_reg,
#endif
	.property_access  = property_access,
	.axi_sts_init     = a1_axi_sts_init,
};
