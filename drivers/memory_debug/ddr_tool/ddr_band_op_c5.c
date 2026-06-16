// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2024 Amlogic, Inc. All rights reserved.
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
#define DMC_AXI0_WRITE_BW		((0x0021 << 2))
#define DMC_CMD_FILTER_CTRL3		((0x0053 << 2))

#define DDR_TYPE			((0x0305 << 2))

#define DMC_CHAN_STS_OFFSET		((0x00d8  << 2))
static unsigned int axi_chan_sts_offset[] __initdata = {
	(0xc0 << 2), (0xc1 << 2), (0xc2 << 2), (0xc3 << 2),
	(0xc4 << 2), (0xc5 << 2), (0xc6 << 2), (0xc7 << 2),
	(0xc8 << 2), (0xc9 << 2), (0xca << 2)
};

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

static void c5_dmc_port_config(struct ddr_bandwidth *db, int channel, int map_id)
{
	unsigned int i, val;

	pr_info("C5 can not set port, default be set\n");

	/* c5 can not se port, default be set */
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

static unsigned long c5_get_dmc_freq_quick(struct ddr_bandwidth *db)
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

static void c5_dmc_bandwidth_enable(struct ddr_bandwidth *db)
{
	unsigned int val;

	val = db->mode << 31;
	val |= (readl(db->ddr_reg[0] + DMC_MON_CTRL0) & ~BIT(31));
	writel(val, db->ddr_reg[0] + DMC_MON_CTRL0);
}

static void c5_dmc_bandwidth_init(struct ddr_bandwidth *db)
{
	writel(db->clock_count, db->ddr_reg[0] + DMC_MON_TIMER);
	c5_dmc_bandwidth_enable(db);

	c5_dmc_port_config(db, -1, -1);
}

static int c5_handle_irq(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	unsigned int i, val, off;
	int ret = -1;

	val = readl(db->ddr_reg[0] + DMC_MON_CTRL0);
	if (val & DMC_QOS_IRQ) {
		dg->all_grant    += readl(db->ddr_reg[0] + DMC_MON_ALL_BW);
		dg->all_grant16  += readl(db->ddr_reg[0] + DMC_MON_ALL16_BW);

		for (i = 0; i < db->channels; i++) {
			off = i * 8 + DMC_AXI0_READ_BW;
			dg->channel_grant[i] += readl(db->ddr_reg[0] + off);
			dg->channel_grant[i] += readl(db->ddr_reg[0] + off + 4);
		}
		ret = 0;
	}

	if (!ret) {
		/* clear irq flags */
		c5_dmc_bandwidth_enable(db);

		/* each register */
		dg->all_grant   *= 16;
		dg->all_grant16 *= 16;
		for (i = 0; i < db->channels; i++)
			dg->channel_grant[i] *= 16;
	}

	return ret;
}

#if DDR_BANDWIDTH_DEBUG
static int c5_dump_reg(struct ddr_bandwidth *db, char *buf)
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

	for (i = 0; i < db->channels; i++) {
		off = i * 8 + DMC_AXI0_READ_BW;
		r  = readl(db->ddr_reg[0] + off);
		s += sprintf(buf + s, "DMC_AXI%d_READ_BW:          %08x\n", i, r);
		r  = readl(db->ddr_reg[0] + off + 4);
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

static int __init c5_axi_sts_init(struct ddr_bandwidth *db)
{
	return ddr_axi_sts_alloc(db, DMC_CHAN_STS_OFFSET,
				 axi_chan_sts_offset, ARRAY_SIZE(axi_chan_sts_offset));
}

struct ddr_bandwidth_ops c5_ddr_bw_ops = {
	.init             = c5_dmc_bandwidth_init,
	.config_port      = c5_dmc_port_config,
	.get_freq         = c5_get_dmc_freq_quick,
	.handle_irq       = c5_handle_irq,
	.bandwidth_enable = c5_dmc_bandwidth_enable,
	.outstanding_init = outstanding_init,
	.outstanding      = outstanding_handle,
#if DDR_BANDWIDTH_DEBUG
	.dump_reg         = c5_dump_reg,
#endif
	.property_access  = property_access,
	.side_band	  = side_band,
	.axi_sts_init     = c5_axi_sts_init,
};

static int set_vpu_sub_dev_priority(unsigned char axi, char rw,
					unsigned int id, unsigned char priority)
{
	struct ddr_priority_node *node;
	int is_w;

	if (rw == 'r')
		is_w = 0;
	else if (rw == 'w')
		is_w = 1;
	else
		return -EINVAL;

	node = get_dev_tree_prio_by_path(rw, 0x01, id);
	if (IS_ERR(node))
		return PTR_ERR(node);

	return write_node_layer(node, 'd', (char)priority, is_w, -1, NULL);
}

static int hevc_hcodec_sub_get(unsigned int base, signed char id)
{
	int i, j;
	u32 reg_h[3], reg_l[3];
	unsigned char val;

	for (i = 0; i < 3; i++) {
		reg_access(base + ((4 * i) << 2), 0, DMC_READ, &reg_h[i]);
		reg_access(base + ((4 * i + 1) << 2), 0, DMC_READ, &reg_l[i]);

		for (j = 0; j < 4; j++) {
			val = (reg_h[i] >> (j * 8)) & 0xff;
			if (val == id)
				return i + 1;
			val = (reg_l[i] >> (j * 8)) & 0xff;
			if (val == id)
				return i + 1;
		}
	}

	return -ENODEV;
}

static int hevc_hcodec_sub_set(unsigned int base, signed char id,
				unsigned char val, unsigned char index)
{
	unsigned char off = val - 1;
	u32 reg_h, reg_l;

	if (val < 1 || val > 3)
		return -EINVAL;

	if (index > 7)
		return -EINVAL;

	reg_access(base + ((4 * off) << 2), 0, DMC_READ, &reg_h);
	reg_access(base + ((4 * off + 1) << 2), 0, DMC_READ, &reg_l);

	if (index <= 3) {
		reg_l &= ~(0xff << (index * 8));
		reg_l |= (id << (index * 8));
		reg_access(base + ((4 * off + 1) << 2), 0, DMC_WRITE, &reg_l);
	} else {
		reg_h &= ~(0xff << ((index - 4) * 8));
		reg_h |= (id << ((index - 4) * 8));
		reg_access(base + ((4 * off) << 2), 0, DMC_WRITE, &reg_h);
	}

	return 0;
}

#define AW_HEVC_MAP_LV1_H                ((0x3fa4  << 2) + 0xfe320000)
#define AW_HEVC_MAP_LV1_L                ((0x3fa5  << 2) + 0xfe320000)
#define AR_HEVC_MAP_LV1_H                ((0x3fa6  << 2) + 0xfe320000)
#define AR_HEVC_MAP_LV1_L                ((0x3fa7  << 2) + 0xfe320000)
#define AW_HEVC_MAP_LV2_H                ((0x3fa8  << 2) + 0xfe320000)
#define AW_HEVC_MAP_LV2_L                ((0x3fa9  << 2) + 0xfe320000)
#define AR_HEVC_MAP_LV2_H                ((0x3faa  << 2) + 0xfe320000)
#define AR_HEVC_MAP_LV2_L                ((0x3fab  << 2) + 0xfe320000)
#define AW_HEVC_MAP_LV3_H                ((0x3fac  << 2) + 0xfe320000)
#define AW_HEVC_MAP_LV3_L                ((0x3fad  << 2) + 0xfe320000)
#define AR_HEVC_MAP_LV3_H                ((0x3fae  << 2) + 0xfe320000)
#define AR_HEVC_MAP_LV3_L                ((0x3faf  << 2) + 0xfe320000)

int c5_hevc_handle_get(void *node, int is_w)
{
	struct ddr_priority_node *n;
	s16 subid;

	if (!node)
		return -ENODEV;

	n = (struct ddr_priority_node *)node;
	subid = n->sub_id;

	if (is_w)
		return hevc_hcodec_sub_get(AW_HEVC_MAP_LV1_H, subid);
	else
		return hevc_hcodec_sub_get(AR_HEVC_MAP_LV1_H, subid);
}

int c5_hevc_handle_set(void *node, unsigned int value, int is_w, void *data)
{
	struct ddr_priority_node *n;
	s16 subid;
	unsigned char index = data ? *(unsigned char *)data : 0;

	if (!node)
		return -ENODEV;

	n = (struct ddr_priority_node *)node;
	subid = n->sub_id;

	if (is_w)
		return hevc_hcodec_sub_set(AW_HEVC_MAP_LV1_H, subid, value, index);
	else
		return hevc_hcodec_sub_set(AR_HEVC_MAP_LV1_H, subid, value, index);
}

static int set_hevc_level_id(char rw, unsigned char level, unsigned char i, unsigned char id)
{
	struct ddr_priority_node *node;
	int is_w;

	if (rw == 'r')
		is_w = 0;
	else if (rw == 'w')
		is_w = 1;
	else
		return -EINVAL;

	node = get_dev_tree_prio_by_path(is_w, 0x9, id);
	if (IS_ERR(node))
		return PTR_ERR(node);

	if (!node->dev)
		return -ENODEV;

	return c5_hevc_handle_set(node->dev, level, is_w, &i);
}

#define AW_HCODEC_MAP_LV1_H              ((0x3fb0  << 2) + 0xfe320000)
#define AW_HCODEC_MAP_LV1_L              ((0x3fb1  << 2) + 0xfe320000)
#define AR_HCODEC_MAP_LV1_H              ((0x3fb2  << 2) + 0xfe320000)
#define AR_HCODEC_MAP_LV1_L              ((0x3fb3  << 2) + 0xfe320000)
#define AW_HCODEC_MAP_LV2_H              ((0x3fb4  << 2) + 0xfe320000)
#define AW_HCODEC_MAP_LV2_L              ((0x3fb5  << 2) + 0xfe320000)
#define AR_HCODEC_MAP_LV2_H              ((0x3fb6  << 2) + 0xfe320000)
#define AR_HCODEC_MAP_LV2_L              ((0x3fb7  << 2) + 0xfe320000)
#define AW_HCODEC_MAP_LV3_H              ((0x3fb8  << 2) + 0xfe320000)
#define AW_HCODEC_MAP_LV3_L              ((0x3fb9  << 2) + 0xfe320000)
#define AR_HCODEC_MAP_LV3_H              ((0x3fba  << 2) + 0xfe320000)
#define AR_HCODEC_MAP_LV3_L              ((0x3fbb  << 2) + 0xfe320000)

int c5_hcodec_handle_get(void *node, int is_w)
{
	struct ddr_priority_node *n;
	s16 subid;

	if (!node)
		return -ENODEV;

	n = (struct ddr_priority_node *)node;
	subid = n->module_id;

	if (is_w)
		return hevc_hcodec_sub_get(AW_HCODEC_MAP_LV1_H, subid);
	else
		return hevc_hcodec_sub_get(AR_HCODEC_MAP_LV1_H, subid);
}

int c5_hcodec_handle_set(void *node, unsigned int value, int is_w, void *data)
{
	struct ddr_priority_node *n;
	s16 subid;
	unsigned char index = data ? *(unsigned char *)data : 0;

	if (!node)
		return -ENODEV;

	n = (struct ddr_priority_node *)node;
	subid = n->module_id;

	if (is_w)
		return hevc_hcodec_sub_set(AW_HCODEC_MAP_LV1_H, subid, value, index);
	else
		return hevc_hcodec_sub_set(AR_HCODEC_MAP_LV1_H, subid, value, index);
}

static int set_hcodec_level_id(char rw, unsigned char level, unsigned char i, unsigned char id)
{
	struct ddr_priority_node *node;
	int is_w;

	if (rw == 'r')
		is_w = 0;
	else if (rw == 'w')
		is_w = 1;
	else
		return -EINVAL;

	node = get_dev_tree_prio_by_path(is_w, 0x8, 0x0, id);
	if (IS_ERR(node))
		return PTR_ERR(node);

	if (!node->dev)
		return -ENODEV;

	return c5_hcodec_handle_set(node->dev, level, is_w, &i);
}

static int set_hevc_low_priority(char rw, unsigned char priority)
{
	struct ddr_priority_node *node;
	int is_w;

	if (rw == 'r')
		is_w = 0;
	else if (rw == 'w')
		is_w = 1;
	else
		return -EINVAL;

	node = get_dev_tree_prio_by_path(is_w, 0x9);
	if (IS_ERR(node))
		return PTR_ERR(node);

	return write_node_layer(node, 'd', (char)priority, is_w, -1, NULL);
}

static int set_hcodec_low_priority(char rw, unsigned char priority)
{
	struct ddr_priority_node *node;
	int is_w;

	if (rw == 'r')
		is_w = 0;
	else if (rw == 'w')
		is_w = 1;
	else
		return -EINVAL;

	node = get_dev_tree_prio_by_path(is_w, 0x8, 0x0);
	if (IS_ERR(node))
		return PTR_ERR(node);

	return write_node_layer(node, 'd', (char)priority, is_w, -1, NULL);
}

static void show_hevc_hcodec_id_map(u32 *reg_h, u32 *reg_l,
					unsigned int port_id, int *sub_path, int depth, int is_w)
{
	int i, j;
	unsigned char id;
	struct ddr_priority_node *node;

	if (is_w)
		pr_info("write low level priority  3\n");
	else
		pr_info("read low level priority  3\n");
	pr_info("lv  idx  id  name\n");

	for (i = 0; i < 3; i++) {
		for (j = 0; j < 4; j++) {
			id = (reg_l[i] >> (j * 8)) & 0xff;
			sub_path[depth] = id;
			node = find_node_by_dev_path(aml_db->priority->tree,
							port_id, sub_path, depth + 1, is_w);
			if (node && node->name)
				pr_info("%-2d  %-3d  %-2x  %s\n", i + 1, j, id, node->name);
			else
				pr_info("%-2d  %-3d  %-2x\n", i + 1, j, id);
		}
		for (j = 0; j < 4; j++) {
			id = (reg_h[i] >> (j * 8)) & 0xff;
			sub_path[depth] = id;
			node = find_node_by_dev_path(aml_db->priority->tree,
							port_id, sub_path, depth + 1, is_w);
			if (node && node->name)
				pr_info("%-2d  %-3d  %-2x  %s\n", i + 1, j + 4, id, node->name);
			else
				pr_info("%-2d  %-3d  %-2x\n", i + 1, j + 4, id);
		}
	}
}

static int show_hevc_priority_setting(void)
{
	int i;
	u32 reg_wh[3], reg_wl[3], reg_rh[3], reg_rl[3], base;
	int sub_path[4] = {-1};

	base = AW_HEVC_MAP_LV1_H;
	for (i = 0; i < 3; i++) {
		reg_access(base + ((4 * i) << 2), 0, DMC_READ, &reg_wh[i]);
		reg_access(base + ((4 * i + 1) << 2), 0, DMC_READ, &reg_wl[i]);
		reg_access(base + ((4 * i + 2) << 2), 0, DMC_READ, &reg_rh[i]);
		reg_access(base + ((4 * i + 3) << 2), 0, DMC_READ, &reg_rl[i]);
	}

	show_hevc_hcodec_id_map(reg_wh, reg_wl, 0x9, sub_path, 0, 1);
	show_hevc_hcodec_id_map(reg_rh, reg_rl, 0x9, sub_path, 0, 0);

	return 0;
}

static int show_hcodec_priority_setting(void)
{
	int i;
	u32 reg_wh[3], reg_wl[3], reg_rh[3], reg_rl[3], base;
	int sub_path[4] = {0x0};

	base = AW_HCODEC_MAP_LV1_H;
	for (i = 0; i < 3; i++) {
		reg_access(base + ((4 * i) << 2), 0, DMC_READ, &reg_wh[i]);
		reg_access(base + ((4 * i + 1) << 2), 0, DMC_READ, &reg_wl[i]);
		reg_access(base + ((4 * i + 2) << 2), 0, DMC_READ, &reg_rh[i]);
		reg_access(base + ((4 * i + 3) << 2), 0, DMC_READ, &reg_rl[i]);
	}

	show_hevc_hcodec_id_map(reg_wh, reg_wl, 0x8, sub_path, 1, 1);
	show_hevc_hcodec_id_map(reg_rh, reg_rl, 0x8, sub_path, 1, 0);

	return 0;
}

static int set_device_accessible(char *dev, unsigned char access)
{
	return 0;
}

struct ddr_priority_ops c5_ddr_prio_ops = {
	.set_vpu_sub_dev_priority = set_vpu_sub_dev_priority,
	.set_hevc_level_id = set_hevc_level_id,
	.set_hcodec_level_id = set_hcodec_level_id,
	.set_hevc_low_priority = set_hevc_low_priority,
	.set_hcodec_low_priority = set_hcodec_low_priority,
	.show_hevc_priority_setting = show_hevc_priority_setting,
	.show_hcodec_priority_setting = show_hcodec_priority_setting,
	.set_device_accessible = set_device_accessible,
};
