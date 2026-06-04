// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/sched/clock.h>
#include <linux/timer.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/ctype.h>
#include <linux/sort.h>

#include <base.h>
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/aml_ddr_tool.h>
#include "ddr_bandwidth.h"
#include "dmc.h"

#define PXP_DEBUG	1
#if PXP_DEBUG
static unsigned long pxp_debug_dmc_freq, pxp_debug_ddr_freq;
#endif

// #define DEBUG
#define T_BUF_SIZE	(1024 * 1024 * 50)

static struct hrtimer ddr_hrtimer_timer;

struct ddr_bandwidth *aml_db;

static int init_ots_level = -1;
static int ots_level_setup(char *str)
{
	int val;

	if (kstrtoint(str, 10, &val)) {
		pr_err("set ots_level error: %s\n", str);
		return 1;
	}

	init_ots_level = val;

	return 1;
}
__setup("ots_level=", ots_level_setup);

static int init_poll_cpu = 1;
static int poll_cpu_setup(char *str)
{
	int val;

	if (kstrtoint(str, 10, &val)) {
		pr_err("set db_poll_cpu error: %s\n", str);
		return 1;
	}

	if (val < num_possible_cpus())
		init_poll_cpu = val;

	return 1;
}
__setup("db_poll_cpu=", poll_cpu_setup);

/* run time should be short */
static enum hrtimer_restart  ddr_hrtimer_handler(struct hrtimer *timer)
{
	static u64 index;

	hrtimer_start(&ddr_hrtimer_timer,
					ktime_set(0, aml_db->increase_tool.t_ns),
					HRTIMER_MODE_REL);
	memset(aml_db->increase_tool.buf_addr + index, 0, aml_db->increase_tool.once_size);

	index += aml_db->increase_tool.once_size;
	if ((index + aml_db->increase_tool.once_size) > T_BUF_SIZE)
		index = 0;

	return HRTIMER_RESTART;
}

static int __init ddr_hrtimer_init(void)
{
	hrtimer_init(&ddr_hrtimer_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ddr_hrtimer_timer.function = ddr_hrtimer_handler;
	return 0;
}

static void ddr_hrtimer_cancel(void)
{
	hrtimer_cancel(&ddr_hrtimer_timer);
}

static int dmc_pll_is_sec(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & PLL_IS_SEC))
		return 1;
	return 0;
}

static int dmc_dev_is_byte(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & DDR_DEVICE_8BIT))
		return 1;
	return 0;
}

static int ddr_fill_port_map_table(struct ddr_bandwidth *db, struct ddr_port_map *map)
{
	DECLARE_BITMAP(port_bitmap, MAX_MAP_ID);
	unsigned int rp = db->real_ports;
	struct ddr_port_desc *desc = db->port_desc;
	int idx = 0;
	int i, j;

	bitmap_zero(port_bitmap, MAX_MAP_ID);
	if (!desc || !rp)
		return 0;

	for (i = 0; i < (int)rp; i++) {
		if (desc[i].sub_id != -1)
			continue;
		if (test_bit(desc[i].port_id, port_bitmap))
			continue;
		set_bit(desc[i].port_id, port_bitmap);

		if (map) {
			map[idx].type = DDR_PORT_MAP_MASTER;
			map[idx].port_id = desc[i].port_id;
			map[idx].sub_id = -1;
			map[idx].rw = 0;
			map[idx].desc = &desc[i];
		}
		idx++;

		for (j = 0; j < (int)rp; j++) {
			if (desc[j].port_id != desc[i].port_id || desc[j].sub_id == -1)
				continue;
			if (map) {
				map[idx].type = DDR_PORT_MAP_DESC_SUB;
				map[idx].port_id = desc[j].port_id;
				map[idx].sub_id = desc[j].sub_id;
				map[idx].rw = 0;
				map[idx].desc = &desc[j];
			}
			idx++;
		}
	}

	return idx;
}

const char *ddr_port_map_name(const struct ddr_port_map *map)
{
	if (!map)
		return NULL;

	switch (map->type) {
	case DDR_PORT_MAP_MASTER:
	case DDR_PORT_MAP_DESC_SUB:
		if (map->desc)
			return map->desc->port_name;
		return NULL;
	case DDR_PORT_MAP_SUB_R:
	case DDR_PORT_MAP_SUB_W:
		if (map->sub_port && map->sub_port->name)
			return map->sub_port->name;
		return NULL;
	default:
		return NULL;
	}
}

void ddr_free_port_map_table(struct ddr_bandwidth *db)
{
	if (!db)
		return;
	kvfree(db->port_map_table);
	db->port_map_table = NULL;
	db->port_map_count = 0;
}

int ddr_init_port_map_table(struct ddr_bandwidth *db)
{
	struct ddr_port_map *map_table;
	int count;

	ddr_free_port_map_table(db);
	if (!db || !db->port_desc || !db->real_ports)
		return 0;

	count = ddr_fill_port_map_table(db, NULL);
	if (count <= 0) {
		pr_err("port map table count is invalid\n");
		return -EINVAL;
	}

	map_table = kvmalloc_array(count, sizeof(*map_table), GFP_KERNEL);
	if (!map_table)
		return -ENOMEM;

	count = ddr_fill_port_map_table(db, map_table);
	if (count <= 0) {
		kvfree(map_table);
		pr_err("port map table fill failed\n");
		return -EINVAL;
	}

	db->port_map_table = map_table;
	db->port_map_count = count;
	bitmap_zero(db->mapid_select, MAX_MAP_ID * MAX_CHANNEL);

	return 0;
}

int port_to_map_id(int port, int sub_id)
{
	int i;

	for (i = 0; i < aml_db->port_map_count; i++) {
		if (aml_db->port_map_table[i].port_id == port &&
			aml_db->port_map_table[i].sub_id == sub_id)
			return i;
	}
	return -EINVAL;
}

static int ddr_width_is_16bit(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & DDR_WIDTH_IS_16BIT))
		return 1;
	return 0;
}

static int ddr_width_is_64bit(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & DDR_WIDTH_IS_64BIT))
		return 1;
	return 0;
}

static int dmc_is_asymmetry(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & DMC_ASYMMETRY))
		return 1;
	return 0;
}

static int ddr_is_poll(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & DDR_IS_POLL))
		return 1;
	return 0;
}

static int dev_is_mdc(struct ddr_bandwidth *db)
{
	if (db && (db->soc_feature & DEV_IS_MDC))
		return 1;
	return 0;
}

static int get_ddr_number(void)
{
	if (dev_is_mdc(aml_db))
		return aml_db->mdc.number;
	else
		return aml_db->dmc_number;
}

static void cal_ddr_usage_single(struct ddr_bandwidth *db)
{
	u64 mul; /* avoid overflow */
	int i, j;
	unsigned long cnt;
	char label[] = "ddr_bw  total (MB/S)";
	char chann_names[] = "ddr_bw0 ch 0 (MB/S)";
	static unsigned long freq[MAX_DMC_NUM];
	static u64 mbw[MAX_DMC_NUM];

	cnt  = db->clock_count;
	for (i = 0; i < get_ddr_number(); i++) {
		/* mbw = (ddr_freq * 2 * (data_bus_width/8)) */
		if (unlikely(freq[i] != db->data_extern[i].ddr_freq)) {
			freq[i] = db->data_extern[i].ddr_freq;
			if (db->ddr_type == LPDDR5)
				freq[i] = freq[i] * 4;
			if (db->bus_width[i])
				mbw[i] = (u64)freq[i] * (db->bus_width[i] >> 2);
			else
				mbw[i] = (u64)freq[i] * (db->data_extern[i].data_bus_width >> 2);
			mbw[i] /= 1024;
		}
		mul  = db->data_extern[i].dg.all_grant;
		mul *= db->data_extern[i].dmc_freq;
		mul /= 1024;
		do_div(mul, cnt);
		if (unlikely(mul >= mbw[i])) {
			/* sample may overflow if irq tick changed, ignore it */
			pr_emerg("dmc%d, bandwidth:%lld large than max :%lld\n", i, mul, mbw[i]);
		}
		db->data_extern[i].cur_sample.total_bandwidth = mul;
		mul *= 10000ULL;
		do_div(mul, mbw[i]);
		db->data_extern[i].cur_sample.total_usage = mul;
		db->data_extern[i].cur_sample.tick = db->cur_sample.tick;
		for (j = 0; j < db->channels; j++) {
			mul  =  db->data_extern[i].dg.channel_grant[j];
			mul *= db->data_extern[i].dmc_freq;
			mul /= 1024;
			do_div(mul, cnt);
			db->data_extern[i].cur_sample.bandwidth[j] = mul;
		}

		label[6] = '0' + i;
		chann_names[6] = '0' + i;
		ATRACE_COUNTER(label, db->data_extern[i].cur_sample.total_bandwidth / 1000);

		for (j = 0; j < db->channels; j++) {
			/* ddr_bw<i> ch <j> (MB/S) */
			chann_names[11] = '0' + j;
			ATRACE_COUNTER(chann_names,
					db->data_extern[i].cur_sample.bandwidth[j] / 1000);
		}

		if (db->stat_flag)
			continue;

		/* update max sample */
		if (db->data_extern[i].cur_sample.total_bandwidth >
			db->data_extern[i].max_sample.total_bandwidth)
			memcpy(&db->data_extern[i].max_sample, &db->data_extern[i].cur_sample,
				sizeof(struct ddr_bandwidth_sample));

		/* update usage statistics */
		db->data_extern[i].usage_stat[db->data_extern[i].cur_sample.total_usage / 1000]++;

		/* collect for average bandwidth calculate */
		db->data_extern[i].avg.avg_bandwidth +=
						db->data_extern[i].cur_sample.total_bandwidth;
		db->data_extern[i].avg.avg_usage += db->data_extern[i].cur_sample.total_usage;
		for (j = 0; j < db->channels; j++) {
			db->data_extern[i].avg.avg_port[j]  +=
						db->data_extern[i].cur_sample.bandwidth[j];
			if (db->data_extern[i].cur_sample.bandwidth[j] >
						db->data_extern[i].avg.max_bandwidth[j])
				db->data_extern[i].avg.max_bandwidth[j] =
							db->data_extern[i].cur_sample.bandwidth[j];
		}
		db->data_extern[i].avg.sample_count++;
		db->data_extern[i].prev_sample = db->data_extern[i].cur_sample;
	}
}

static u64 get_theoretic_bandwidth(struct ddr_bandwidth *db)
{
	static u64 mbw;
	static unsigned long dmc_freq;
	static unsigned long ddr_freq;
	unsigned int bus_width = 0;
	u64 freq;
	int i;

	if (db->dmc_freq != dmc_freq || db->ddr_freq != ddr_freq) {
		dmc_freq = db->dmc_freq;
		ddr_freq = db->ddr_freq;

		/* calculate in KB */
		if (dmc_is_asymmetry(aml_db)) {
			for (i = 0; i < get_ddr_number(); i++) {
				freq = (u64)db->data_extern[i].ddr_freq;
				if (db->ddr_type == LPDDR5)
					freq = freq * 4;
				if (db->bus_width[i])
					mbw += freq * (db->bus_width[i] >> 2);
				else
					mbw += freq * (db->data_extern[i].data_bus_width >> 2);
			}
		} else {
			/* theoretic max bandwidth =  ddr_freq * 2 * width / 8 */
			if (db->ddr_type == LPDDR5)
				mbw = (u64)db->ddr_freq * 4;
			else
				mbw = (u64)db->ddr_freq;

			for (i = 0; i < db->dmc_number; i++)
				bus_width += db->bus_width[i];

			if (bus_width)
				mbw = (u64)mbw * bus_width / 4;
			else if (ddr_width_is_16bit(db))
				mbw = (u64)mbw * 2 * 2;
			else if (ddr_width_is_64bit(db))
				mbw = (u64)mbw * 2 * 8;
			else /* default is 32 */
				mbw = (u64)mbw * 2 * 4;
		}
		mbw /= 1024;	/* theoretic max bandwidth */
	}

	return mbw;
}

static void cal_ddr_usage(struct ddr_bandwidth *db, struct ddr_grant *dg)
{
	u64 mul, mul_tmp, mbw; /* avoid overflow */
	unsigned long cnt, flags;
	int i;
	char chann_names[] = "ddr_bw ch 0 (MB/S)";
	static int err_count;

	if (unlikely(db->mode == MODE_AUTODETECT)) { /* ignore mali bandwidth */
		static int count;
		unsigned int grant = dg->all_grant;

		if (db->mali_port[0] >= 0)
			grant -= dg->channel_grant[0];
		if (db->mali_port[1] >= 0)
			grant -= dg->channel_grant[1];
		if (grant > db->threshold) {
			if (count >= 2) {
				if (db->busy == 0) {
					db->busy = 1;
					schedule_work(&db->work_bandwidth);
				}
			} else {
				count++;
			}
		} else if (count > 0) {
			if (count >= 2) {
				db->busy = 0;
				schedule_work(&db->work_bandwidth);
			}
			count = 0;
		}
		return;
	}

	if (unlikely(db->mode != MODE_ONLY_DDR)) {
		if (db->ops && db->ops->vpu_status)
			db->ops->vpu_status(0);
	}

	if (unlikely(db->mode == MODE_ONLY_VPU))
		return;

	if (unlikely((!db->dmc_freq || !db->ddr_freq) && !err_count)) {
		pr_err("warning: dmc_freq=%ld ddr_freq=%ld\n", db->dmc_freq, db->ddr_freq);
		err_count = 1;
		return;
	}

	db->cur_sample.tick = sched_clock();
	cnt  = db->clock_count;
	mbw = get_theoretic_bandwidth(db);
	if (unlikely(!mbw)) {
		pr_err("warning: theoretic max bandwidth is zero\n");
		return;
	}
	mul = 0;
	if (unlikely(dmc_is_asymmetry(aml_db))) {
		for (i = 0; i < get_ddr_number(); i++) {
			mul_tmp = db->data_extern[i].dg.all_grant;
			mul_tmp *= db->data_extern[i].dmc_freq;
			mul += mul_tmp;
		}
	} else {
		mul = dg->all_grant;
		if (!ddr_is_poll(db))
			mul *= db->dmc_freq;
	}
	if (!ddr_is_poll(db)) {
		mul /= 1024;
		do_div(mul, cnt);
	} else {
		mul *= 1000000000;
		mul /= 1024;
		do_div(mul, db->poll_time);
	}
	if (unlikely(mul >= mbw)) {
		/* sample may overflow if irq tick changed, ignore it */
		pr_info("bandwidth:%lld large than max :%lld\n", mul, mbw);
		return;
	}
	db->cur_sample.total_bandwidth = mul;
	mul *= 10000ULL;
	do_div(mul, mbw);
	db->cur_sample.total_usage = mul;
	for (i = 0; i < db->channels; i++) {
		mul  = dg->channel_grant[i];
		if (!ddr_is_poll(db)) {
			mul *= db->dmc_freq;
			mul /= 1024;
			do_div(mul, cnt);
		} else {
			mul *= 1000000000;
			mul /= 1024;
			do_div(mul, db->poll_time);
		}
		db->cur_sample.bandwidth[i] = mul;
	}

	ATRACE_COUNTER("ddr_bw total (MB/S)", db->cur_sample.total_bandwidth / 1000);
	for (i = 0; i < db->channels; i++) {
		/*  ddr_bw ch <i> (MB/S) */
		chann_names[11] = '0' + i;
		ATRACE_COUNTER(chann_names, db->cur_sample.bandwidth[i] / 1000);
	}

	raw_spin_lock_irqsave(&db->lock, flags);
	if (!db->stat_flag) {			/* stop update usage stat if flag set */
		/* update max sample */
		if (db->cur_sample.total_bandwidth > db->max_sample.total_bandwidth)
			memcpy(&db->max_sample, &db->cur_sample,
			       sizeof(struct ddr_bandwidth_sample));

		/* update usage statistics */
		db->usage_stat[db->cur_sample.total_usage / 1000]++;

		/* collect for average bandwidth calculate */
		db->avg.avg_bandwidth += db->cur_sample.total_bandwidth;
		db->avg.avg_usage     += db->cur_sample.total_usage;
		for (i = 0; i < db->channels; i++) {
			db->avg.avg_port[i]  += db->cur_sample.bandwidth[i];
			if (db->cur_sample.bandwidth[i] > db->avg.max_bandwidth[i])
				db->avg.max_bandwidth[i] = db->cur_sample.bandwidth[i];
		}
		db->avg.sample_count++;

		db->prev_sample = db->cur_sample;
	}

	/* calculate single dmc bandwidth*/
	if (unlikely(dmc_is_asymmetry(db)))
		cal_ddr_usage_single(db);
	raw_spin_unlock_irqrestore(&db->lock, flags);
}

/* run time should be short */
static enum hrtimer_restart ddr_poll_handler(struct hrtimer *timer)
{
	struct ddr_bandwidth *db;
	struct ddr_grant dg = {0};

	db = aml_db;

	if (db->ops && db->ops->handle_irq)
		if (!db->ops->handle_irq(db, &dg))
			cal_ddr_usage(db, &dg);

	if (db->mode) {
		hrtimer_set_expires(timer, ktime_add(ktime_get(),
				    ns_to_ktime(aml_db->ddr_poll_ns)));
		return HRTIMER_RESTART;
	} else {
		return HRTIMER_NORESTART;
	}
}

static int __init ddr_poll_init(void)
{
	if (!aml_db) {
		return -1;
	}

	aml_db->poll_cpu = init_poll_cpu;
	hrtimer_init(&aml_db->ddr_poll_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	aml_db->ddr_poll_timer.function = ddr_poll_handler;
	return 0;
}

void ddr_poll_start(void *info)
{
	if (!aml_db)
		return;

	if (aml_db->mode) {
		hrtimer_cancel(&aml_db->ddr_poll_timer);
		hrtimer_start(&aml_db->ddr_poll_timer,
				ns_to_ktime(aml_db->ddr_poll_ns), HRTIMER_MODE_REL);
	}
}

static void ddr_poll_cancel(void)
{
	if (!aml_db)
		return;

	hrtimer_cancel(&aml_db->ddr_poll_timer);
	/* mdc clear */
	if (aml_db->ops && aml_db->ops->bandwidth_enable)
		aml_db->ops->bandwidth_enable(aml_db);
}

static irqreturn_t dmc_irq_handler(int irq, void *dev_instance)
{
	struct ddr_bandwidth *db;
	struct ddr_grant dg = {0};

	db = (struct ddr_bandwidth *)dev_instance;
	if (db->ops && db->ops->handle_irq) {
		if (!db->ops->handle_irq(db, &dg))
			cal_ddr_usage(db, &dg);
	}

	return IRQ_HANDLED;
}

unsigned int aml_get_ddr_usage(void)
{
	unsigned int ret = 0;

	if (aml_db)
		ret = aml_db->cur_sample.total_usage;

	return ret;
}
EXPORT_SYMBOL(aml_get_ddr_usage);

void aml_get_all_channel_grant(u64 *channel_grant)
{
	u64 mul = 0;
	int i;

	if (aml_db && aml_db->dmc_freq) {
		for (i = 0; i < aml_db->channels; i++) {
			mul = aml_db->avg.avg_port[i];
			mul *= aml_db->clock_count;
			mul *= 1024;
			do_div(mul, aml_db->dmc_freq);
			channel_grant[i] = mul;
		}
	}
}
EXPORT_SYMBOL(aml_get_all_channel_grant);

static void clear_bandwidth_statistics(void)
{
	unsigned long flags;
	int i;

	/* clear flag and start statistics */
	raw_spin_lock_irqsave(&aml_db->lock, flags);
	memset(&aml_db->max_sample, 0, sizeof(struct ddr_bandwidth_sample));
	memset(aml_db->usage_stat, 0, 10 * sizeof(int));
	memset(&aml_db->avg, 0, sizeof(struct ddr_avg_bandwidth));
	if (dmc_is_asymmetry(aml_db)) {
		for (i = 0; i < get_ddr_number(); i++) {
			memset(&aml_db->data_extern[i].max_sample, 0,
							sizeof(struct ddr_bandwidth_sample));
			memset(aml_db->data_extern[i].usage_stat, 0, 10 * sizeof(int));
			memset(&aml_db->data_extern[i].avg, 0, sizeof(struct ddr_avg_bandwidth));
		}
	}
	raw_spin_unlock_irqrestore(&aml_db->lock, flags);
}

static void ddr_copy_ch_mapid(unsigned long *mask, int ch)
{
	unsigned int start = (unsigned int)ch * MAX_MAP_ID;

	bitmap_copy_clear_tail(mask, aml_db->mapid_select + BIT_WORD(start), MAX_MAP_ID);
}

static int format_port(char *buf, int ch)
{
	DECLARE_BITMAP(mask_bitmap, MAX_MAP_ID);
	unsigned int index;
	const char *name;
	int size = 0;

	ddr_copy_ch_mapid(mask_bitmap, ch);

	if (bitmap_empty(mask_bitmap, MAX_MAP_ID))
		return 0;

	for_each_set_bit(index, mask_bitmap, MAX_MAP_ID) {
		name = ddr_port_map_name(&aml_db->port_map_table[index]);
		if (!name)
			continue;
		size += sprintf(buf + size, "      %s\n", name);
	}

	return size;
}

int ddr_set_map_select(int ch, int map_id)
{
	const struct ddr_port_map *map;

	if (map_id < 0) {
		bitmap_clear(aml_db->mapid_select, ch * MAX_MAP_ID, MAX_MAP_ID);
		return 0;
	}

	if (map_id > aml_db->port_map_count) {
		pr_info("invalid map_id:%d\n", map_id);
		return -ERANGE;
	}

	map = &aml_db->port_map_table[map_id];
	if (map->type != DDR_PORT_MAP_MASTER && map->type != DDR_PORT_MAP_DESC_SUB) {
		pr_info("invalid map_id:%d\n", map_id);
		return -ERANGE;
	}
	set_bit(map_id + ch * MAX_MAP_ID, aml_db->mapid_select);

	return 0;
}

unsigned int ddr_get_ch_set_num(int ch)
{
	DECLARE_BITMAP(mask_bitmap, MAX_MAP_ID);

	ddr_copy_ch_mapid(mask_bitmap, ch);

	return bitmap_weight(mask_bitmap, MAX_MAP_ID);
}

static int ddr_port_set_byte(struct ddr_bandwidth *db, int ch, int map_id)
{
	if (map_id >= 0) {
		if (ddr_get_ch_set_num(ch) >= 3) {
			pr_err("each channel only support 3 ports\n");
			return -ERANGE;
		}
	}

	return ddr_set_map_select(ch, map_id);
}

int get_bus_num(void)
{
	if (dev_is_mdc(aml_db))
		return aml_db->mdc.port_num;
	else
		return aml_db->bus_num;
}
EXPORT_SYMBOL(get_bus_num);

int get_bus_ots_value(int bus)
{
	if (bus >= get_bus_num())
		return -1;
	if (!aml_db->ops->outstanding)
		return -1;

	return aml_db->ops->outstanding(aml_db, bus, 0, OUTSTANDING_GET);
}
EXPORT_SYMBOL(get_bus_ots_value);

int set_bus_ots_by_value(int bus, int value)
{
	if (bus >= get_bus_num())
		return -1;

	if (!aml_db->ops->outstanding)
		return -1;

	aml_db->ops->outstanding(aml_db, bus, value, OUTSTANDING_SET);
	return 0;
}
EXPORT_SYMBOL(set_bus_ots_by_value);

int set_bus_ots_by_level(int bus, unsigned int level)
{
	unsigned int val;

	if (bus >= get_bus_num())
		return -1;

	if (level >= aml_db->ost.levels.count)
		return -1;

	if (!aml_db->ops->outstanding)
		return -1;

	val = aml_db->ost.levels.value[level];
	aml_db->ops->outstanding(aml_db, bus, val, OUTSTANDING_SET);

	return 0;
}
EXPORT_SYMBOL(set_bus_ots_by_level);

int get_ots_level(void)
{
	return aml_db->ost.levels.cur_level;
}
EXPORT_SYMBOL(get_ots_level);

int set_all_ots_by_level(unsigned int level)
{
	int i;

	aml_db->ost.levels.cur_level = level;

	for (i = 0; i < get_bus_num(); i++)
		set_bus_ots_by_level(i, level);

	return 0;
}
EXPORT_SYMBOL(set_all_ots_by_level);

static ssize_t outstanding_display(char *buf)
{
	ssize_t len = 0;
	int i, j, count;
	char c;

	if (!aml_db->ost.regs)
		return 0;

	/* show usage */
	len += sprintf(buf + len, "Usage: echo <index> <value> > ots\n");
	len += sprintf(buf + len, "parm:\n\tindex:\tindex_num or ignore(set all bus)\n");
	len += sprintf(buf + len, "\tvalue:\tlevel_num or reg_val or -1(default val)\n");

	/* show levels list */
	len += sprintf(buf + len, "\nOutstanding levels list: cur_level:%d\n",
						aml_db->ost.levels.cur_level);
	for (i = 0; i < aml_db->ost.levels.count; i++) {
		if (i == aml_db->ost.levels.cur_level)
			c = '>';
		else
			c = ' ';
		len += sprintf(buf + len, "%c[%2d]:\t0x%08x\n", c, i, aml_db->ost.levels.value[i]);
	}

	/* show bus outstanding */
	len += sprintf(buf + len, "\noutstanding bit[ 7: 0]: read  hold release num\t");
	len += sprintf(buf + len, "bit[15: 8]: read  hold num\n");
	len += sprintf(buf + len, "outstanding bit[23:16]: write hold release num\t");
	len += sprintf(buf + len, "bit[31:24]: write hold num\n");
	len += sprintf(buf + len, " index\toutstanding (default)\thosts\n");
	for (i = 0; i < get_bus_num(); i++) {
		count = 0;
		for (j = 0, c = 0; j < aml_db->real_ports; j++) {
			if ((dev_is_mdc(aml_db) &&
			  aml_db->port_desc[i].bus == aml_db->port_desc[j].bus &&
			  aml_db->port_desc[i].mdc_bus == aml_db->port_desc[j].mdc_bus) ||
			  (!dev_is_mdc(aml_db) && aml_db->port_desc[j].bus == i)) {
				count++;
				if (count == 1) {
					len += sprintf(buf + len, " [%2d]\t0x%08x (0x%08x)\t%s",
						i,
						get_bus_ots_value(i),
						aml_db->ost.regs[i].def_val,
						aml_db->port_desc[j].port_name);
				} else if (count == 2 || ((count - 1) % 6 == 0)) {
					len += sprintf(buf + len, "\n\t\t\t\t%s ",
							aml_db->port_desc[j].port_name);
				} else {
					len += sprintf(buf + len, "%s ",
							aml_db->port_desc[j].port_name);
				}
			}
		}
		if (count)
			len += sprintf(buf + len, "\n");
	}

	return len;
}

static ssize_t ots_show(const struct class *cla,
			 const struct class_attribute *attr, char *buf)
{
	return outstanding_display(buf);
}

static ssize_t ots_store(const struct class *cla,
			  const struct class_attribute *attr,
			  const char *buf, size_t count)
{
	int bus, i;
	long long val;

	if (sscanf(buf, "%d %lli", &bus, &val) != 2) {
		bus = -1;
		if (kstrtoll(buf, 0, &val)) {
			return count;
		}
	}

	if (bus < 0) {
		for (i = 0; i < get_bus_num(); i++) {
			if (val >= 0 && val <= aml_db->ost.levels.count) {
				aml_db->ost.levels.cur_level = val;
				set_bus_ots_by_level(i, val);
			} else {
				aml_db->ost.levels.cur_level = -1;
				set_bus_ots_by_value(i, val);
			}
		}
	} else {
		if (val >= 0 && val <= aml_db->ost.levels.count)
			set_bus_ots_by_level(bus, val);
		else
			set_bus_ots_by_value(bus, val);
	}

	return count;
}
static CLASS_ATTR_RW(ots);

static ssize_t ots_level_show(const struct class *cla,
			 const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;
	int i;
	char c;

	/* show levels list */
	len += sprintf(buf + len, "Outstanding levels list: cur_level:%d\n",
						aml_db->ost.levels.cur_level);
	for (i = 0; i < aml_db->ost.levels.count; i++) {
		if (i == aml_db->ost.levels.cur_level)
			c = '>';
		else
			c = ' ';
		len += sprintf(buf + len, "%c[%2d]:\t0x%08x\n", c, i, aml_db->ost.levels.value[i]);
	}

	return len;
}

static ssize_t ots_level_store(const struct class *cla,
			  const struct class_attribute *attr,
			  const char *buf, size_t count)
{
	int level, val;

	if (sscanf(buf, "%d %x", &level, &val) == 2) {
		aml_db->ost.levels.value[level] = val;
		return count;
	}

	if (kstrtouint(buf, 0, &level)) {
		pr_info("invalid input: %s\n", buf);
		return -EINVAL;
	}

	set_all_ots_by_level(level);

	return count;
}
static CLASS_ATTR_RW(ots_level);

static ssize_t port_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	DECLARE_BITMAP(mask_bitmap, MAX_MAP_ID);
	int s = 0, i;

	for (i = 0; i < aml_db->channels; i++) {
		ddr_copy_ch_mapid(mask_bitmap, i);

		if (aml_db->ops && !aml_db->ops->config_range) {
			s += sprintf(buf + s, "ch %d: ports: %*pbl\n",
					i, MAX_MAP_ID, mask_bitmap);
		} else {
			s += sprintf(buf + s, "ch %d, [%08lx-%08lx], ports: %*pbl\n",
					i, aml_db->range[i].start, aml_db->range[i].end,
					MAX_MAP_ID, mask_bitmap);
		}
		s += format_port(buf + s, i);
	}
	return s;
}

static ssize_t port_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ch = 0, map_id = 0, paras;
	unsigned long start = 0xffffffff, end;

	paras = sscanf(buf, "%d:%d %lx-%lx", &ch, &map_id, &start, &end);
	if (paras < 4) {
		paras = sscanf(buf, "%d:%d", &ch, &map_id);
		if (paras < 2) {
			return count;
		}
	}

	if (ch >= MAX_CHANNEL || ch < 0 ||
	    (ch && aml_db->cpu_type < DMC_TYPE_GXTVBB) ||
	    map_id >= aml_db->port_map_count) {
		pr_info("invalid channel %d or map_id %d\n", ch, map_id);
		return count;
	}

	if (aml_db->ops && aml_db->ops->config_port) {
		if (dmc_dev_is_byte(aml_db)) {
			if (ddr_port_set_byte(aml_db, ch, map_id))
				return -ERANGE;
		}

		if (ddr_set_map_select(ch, map_id))
			return count;

		aml_db->ops->config_port(aml_db, ch, map_id);
	}

	if (paras == 4 && aml_db->ops &&
		aml_db->ops->config_range && (start <= end)) {
		aml_db->ops->config_range(aml_db, ch, start, end);
		aml_db->range[ch].start = start;
		aml_db->range[ch].end   = end;
	}

	return count;
}
static CLASS_ATTR_RW(port);

static ssize_t busy_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", aml_db->busy);
}
static CLASS_ATTR_RO(busy);

static ssize_t threshold_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n",
			aml_db->threshold / aml_db->bytes_per_cycle
			/ (aml_db->clock_count / 10000));
}

static ssize_t threshold_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}

	if (val > 10000)
		val = 10000;
	aml_db->threshold = val * aml_db->bytes_per_cycle
			* (aml_db->clock_count / 10000);
	return count;
}
static CLASS_ATTR_RW(threshold);

static ssize_t mode_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	if (aml_db->mode == MODE_DISABLE)
		return sprintf(buf, "%d: disable\n", aml_db->mode);
	else if (aml_db->mode == MODE_ENABLE)
		return sprintf(buf, "%d: enable\n", aml_db->mode);
	else if (aml_db->mode == MODE_AUTODETECT)
		return sprintf(buf, "%d: auto detect\n", aml_db->mode);
	else if (aml_db->mode == MODE_ONLY_DDR)
		return sprintf(buf, "%d: only ddr bandwidth\n", aml_db->mode);
	else if (aml_db->mode == MODE_ONLY_VPU)
		return sprintf(buf, "%d: only vpu data\n", aml_db->mode);
	return sprintf(buf, "\n");
}

static int ddr_irq_enabled(void)
{
	static bool is_request;
	int affinity_cpu = 1;

	if (!ddr_is_poll(aml_db) && !is_request) {
		int r = request_irq(aml_db->irq_num, dmc_irq_handler,
				IRQF_SHARED | IRQF_ONESHOT,
				"ddr_bandwidth", (void *)aml_db);
		if (r < 0) {
			pr_info("ddr bandwidth request irq failed\n");
			return r;
		}

		if (num_online_cpus() > 2)
			affinity_cpu = num_online_cpus() - 1;
		irq_set_affinity_hint(aml_db->irq_num, get_cpu_mask(affinity_cpu));
		is_request = true;
	}

	return 0;
}

static ssize_t mode_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	long val = 0;

	if (kstrtoul(buf, 10, &val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}

	if (val > MODE_TYPE_MAX || val < MODE_DISABLE) {
		pr_info("invalid mode type:%ld, max:%d\n", val, MODE_TYPE_MAX);
		return 0;
	}

	if (!aml_db->ops) {
		pr_err("aml_db->ops is null\n");
		return 0;
	}

	if (aml_db->mode == MODE_DISABLE && val != MODE_DISABLE) {
		/* only mode enabled set, dmc irq will share with aml_ddr */
		ddr_irq_enabled();
		if (aml_db->ops->init) {
			aml_db->mode = val;
			aml_db->ops->init(aml_db);
		}
	} else if ((aml_db->mode != MODE_DISABLE) && (val == MODE_DISABLE)) {
		if (ddr_is_poll(aml_db)) {
			aml_db->mode = val;
			ddr_poll_cancel();
		}
		aml_db->cur_sample.total_usage = 0;
		aml_db->cur_sample.total_bandwidth = 0;
		aml_db->busy = 0;
		clear_bandwidth_statistics();
	}
	if (val == MODE_AUTODETECT && aml_db->ops->config_port && !dmc_dev_is_byte(aml_db)) {
		if (aml_db->mali_port[0] >= 0) {
			aml_db->ops->config_port(aml_db, 0, aml_db->mali_port[0]);
			ddr_set_map_select(0, port_to_map_id(aml_db->mali_port[0], -1));
		}
		if (aml_db->mali_port[1] >= 0) {
			aml_db->ops->config_port(aml_db, 1, aml_db->mali_port[1]);
			ddr_set_map_select(1, port_to_map_id(aml_db->mali_port[1], -1));
		}
	}
	aml_db->mode = val;

	return count;
}
static CLASS_ATTR_RW(mode);

static ssize_t sampling_freq_show(const struct class *class,
				  const struct class_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "%ld Hz\n", 1000000000 / aml_db->ddr_poll_ns);
}

static ssize_t sampling_freq_store(const struct class *class,
				   const struct class_attribute *attr,
				   const char *buf, size_t count)
{
	long freq;
	long val;

	if (kstrtoul(buf, 10, &freq)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}
	aml_db->ddr_poll_ns = 1000000000 / freq;
	val = aml_db->dmc_freq / freq;
	aml_db->threshold /= (aml_db->clock_count / 10000);
	aml_db->threshold *= (val / 10000);
	aml_db->clock_count = val;
	if (aml_db->ops && aml_db->ops->init)
		aml_db->ops->init(aml_db);
	return count;
}
static CLASS_ATTR_RW(sampling_freq);

static ssize_t usage_stat_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int d = -1;

	if (kstrtoint(buf, 10, &d))
		return count;

	aml_db->stat_flag = d;
	if (d)
		return count;

	clear_bandwidth_statistics();
	return count;
}

static ssize_t usage_stat_show_single(char *buf, ssize_t offset)
{
	DECLARE_BITMAP(mask_bitmap, MAX_MAP_ID);
	ssize_t s = offset;
	int percent, rem, i, j;
	unsigned long long tick;
	unsigned long total_count = 0;
	struct ddr_avg_bandwidth tmp;
#define MAX_PREFIX "MAX bandwidth: %8d KB/s, usage: %2d.%02d%%"
#define AVG_PREFIX "AVG bandwidth: %8lld KB/s, usage: %2d.%02d%%"

	for (i = 0; i < get_ddr_number(); i++) {
		s += sprintf(buf + s, "DMC%d:\n", i);

		total_count = aml_db->data_extern[i].avg.sample_count;

		/* show for max bandwidth */
		percent = aml_db->data_extern[i].max_sample.total_usage / 100;
		rem     = aml_db->data_extern[i].max_sample.total_usage % 100;
		tick    = aml_db->data_extern[i].max_sample.tick;
		do_div(tick, 1000);
		s      += sprintf(buf + s, MAX_PREFIX ", tick:%lld us\n",
				aml_db->data_extern[i].max_sample.total_bandwidth,
				percent, rem, tick);

		/* show for average bandwidth */
		memcpy(&tmp, &aml_db->data_extern[i].avg, sizeof(tmp));
		do_div(tmp.avg_bandwidth, total_count);
		do_div(tmp.avg_usage,     total_count);
		for (j = 0; j < aml_db->channels; j++)
			do_div(tmp.avg_port[j], total_count);

		rem     = do_div(tmp.avg_usage, 100);
		percent = tmp.avg_usage;
		s      += sprintf(buf + s, AVG_PREFIX ", samples:%ld\n",
				tmp.avg_bandwidth,
				percent, rem, total_count);

		s += sprintf(buf + s, "\nbandwidth status for each channel\n");
		s += sprintf(buf + s, "ch, \t%s, avg bw(KB/s), max bw(KB/s), %s\n",
			     "port mask", "bw@max sample(KB/s)");
		for (j = 0; j < aml_db->channels; j++) {
			ddr_copy_ch_mapid(mask_bitmap, j);
			s += sprintf(buf + s,
				"%2d, \t%*pbl,     %8lld,     %8ld,          %8d\n",
				j, MAX_MAP_ID, mask_bitmap,
				tmp.avg_port[j],
				tmp.max_bandwidth[j],
				aml_db->data_extern[i].max_sample.bandwidth[j]);
		}

		/* show for usage statistics */
		s += sprintf(buf + s, "\nusage distribution:\n");
		s += sprintf(buf + s, "range,         count,  proportion\n");
		for (j = 0; j < 10; j++) {
			percent = aml_db->data_extern[i].usage_stat[j] * 10000 / total_count;
			rem     = percent % 100;
			percent = percent / 100;
			s += sprintf(buf + s, "%2d%% ~ %3d%%: %8d,  %3d.%02d%%\n",
				j * 10, (j + 1) * 10,
				aml_db->data_extern[i].usage_stat[j], percent, rem);
		}
		s += sprintf(buf + s, "\n");
	}

	return s;
}

static ssize_t usage_stat_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	DECLARE_BITMAP(mask_bitmap, MAX_MAP_ID);
	size_t s = 0;
	int percent, rem, i;
	unsigned long long tick;
	unsigned long total_count = 0;
	struct ddr_avg_bandwidth tmp;
#define MAX_PREFIX "MAX bandwidth: %8d KB/s, usage: %2d.%02d%%"
#define AVG_PREFIX "AVG bandwidth: %8lld KB/s, usage: %2d.%02d%%"

	if (aml_db->mode != MODE_ENABLE)
		return sprintf(buf, "set mode to enable(1) first.\n");

	if (dmc_is_asymmetry(aml_db))
		s += sprintf(buf + s, "DMC ALL:\n");

	total_count = aml_db->avg.sample_count;
	if (!total_count)
		return sprintf(buf, "No sample, please wait...\n");

	/* show for max bandwidth */
	percent = aml_db->max_sample.total_usage / 100;
	rem     = aml_db->max_sample.total_usage % 100;
	tick    = aml_db->max_sample.tick;
	do_div(tick, 1000);
	s      += sprintf(buf + s, MAX_PREFIX ", tick:%lld us\n",
			  aml_db->max_sample.total_bandwidth,
			  percent, rem, tick);

	/* show for average bandwidth */
	memcpy(&tmp, &aml_db->avg, sizeof(tmp));
	do_div(tmp.avg_bandwidth, total_count);
	do_div(tmp.avg_usage,     total_count);
	for (i = 0; i < aml_db->channels; i++)
		do_div(tmp.avg_port[i], total_count);

	rem     = do_div(tmp.avg_usage, 100);
	percent = tmp.avg_usage;
	s      += sprintf(buf + s, AVG_PREFIX ", samples:%ld\n",
			  tmp.avg_bandwidth,
			  percent, rem, total_count);

	s += sprintf(buf + s, "\nbandwidth status for each channel\n");
	s += sprintf(buf + s, "ch, \t%s, avg bw(KB/s), max bw(KB/s), %s\n",
		     "port mask", "bw@max sample(KB/s)");
	for (i = 0; i < aml_db->channels; i++) {
		ddr_copy_ch_mapid(mask_bitmap, i);
		s += sprintf(buf + s,
			     "%2d, \t%*pbl,     %8lld,     %8ld,          %8d\n",
			     i, MAX_MAP_ID, mask_bitmap,
			     tmp.avg_port[i],
			     tmp.max_bandwidth[i],
			     aml_db->max_sample.bandwidth[i]);
	}

	/* show for usage statistics */
	s += sprintf(buf + s, "\nusage distribution:\n");
	s += sprintf(buf + s, "range,         count,  proportion\n");
	for (i = 0; i < 10; i++) {
		percent = aml_db->usage_stat[i] * 10000 / total_count;
		rem     = percent % 100;
		percent = percent / 100;
		s += sprintf(buf + s, "%2d%% ~ %3d%%: %8d,  %3d.%02d%%\n",
			     i * 10, (i + 1) * 10,
			     aml_db->usage_stat[i], percent, rem);
	}
	s += sprintf(buf + s, "\n");

	if (dmc_is_asymmetry(aml_db))
		s = usage_stat_show_single(buf, s);

	return s;
}
static CLASS_ATTR_RW(usage_stat);

static ssize_t bandwidth_show_single(char *buf, ssize_t offset)
{
	DECLARE_BITMAP(mask_bitmap, MAX_MAP_ID);
	size_t s = offset;
	int percent, rem, i, j;
	unsigned long long tick;
#define BANDWIDTH_PREFIX "Total bandwidth: %8d KB/s, usage: %2d.%02d%%\n"

	for (i = 0; i < get_ddr_number(); i++) {
		percent = aml_db->data_extern[i].cur_sample.total_usage / 100;
		rem     = aml_db->data_extern[i].cur_sample.total_usage % 100;
		tick    = aml_db->data_extern[i].cur_sample.tick;
		do_div(tick, 1000);

		s += sprintf(buf + s, "DMC%d:\n", i);
		s      += sprintf(buf + s, BANDWIDTH_PREFIX,
				aml_db->data_extern[i].cur_sample.total_bandwidth,
				percent, rem);

		for (j = 0; j < aml_db->channels; j++) {
			ddr_copy_ch_mapid(mask_bitmap, j);
			s += sprintf(buf + s, "ch %d: %8d KB/s \tport: %*pbl\n",
				j, aml_db->data_extern[i].cur_sample.bandwidth[j],
				MAX_MAP_ID, mask_bitmap);
		}
	}
	return s;
}

static ssize_t bandwidth_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	DECLARE_BITMAP(mask_bitmap, MAX_MAP_ID);
	size_t s = 0;
	int percent, rem, i;
	unsigned long long tick;
#define BANDWIDTH_PREFIX "Total bandwidth: %8d KB/s, usage: %2d.%02d%%\n"

	if (aml_db->mode != MODE_ENABLE)
		return sprintf(buf, "set mode to enable(1) first.\n");

	percent = aml_db->cur_sample.total_usage / 100;
	rem     = aml_db->cur_sample.total_usage % 100;
	tick    = aml_db->cur_sample.tick;
	do_div(tick, 1000);

	if (dmc_is_asymmetry(aml_db))
		s += sprintf(buf + s, "DMC ALL:\n");

	s      += sprintf(buf + s, BANDWIDTH_PREFIX,
			  aml_db->cur_sample.total_bandwidth,
			  percent, rem);

	for (i = 0; i < aml_db->channels; i++) {
		ddr_copy_ch_mapid(mask_bitmap, i);
		s += sprintf(buf + s, "ch %d: %8d KB/s \tport: %*pbl\n",
			     i, aml_db->cur_sample.bandwidth[i],
			     MAX_MAP_ID, mask_bitmap);
	}

	if (dmc_is_asymmetry(aml_db))
		s = bandwidth_show_single(buf, s);

	return s;
}
static CLASS_ATTR_RO(bandwidth);

#if PXP_DEBUG
static ssize_t freq_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int i = 0;
	unsigned long ddr_freq = 0, dmc_freq = 0;

	if (sscanf(buf, "%ld-%ld", &ddr_freq, &dmc_freq) != 2)
		return count;
	pxp_debug_ddr_freq = ddr_freq;
	pxp_debug_dmc_freq = dmc_freq;

	if (pxp_debug_dmc_freq && pxp_debug_ddr_freq) {
		aml_db->ddr_freq = pxp_debug_ddr_freq;
		aml_db->dmc_freq = pxp_debug_dmc_freq;
		if (dmc_is_asymmetry(aml_db)) {
			for (i = 0; i < get_ddr_number(); i++) {
				aml_db->data_extern[i].ddr_freq = pxp_debug_ddr_freq;
				aml_db->data_extern[i].dmc_freq = pxp_debug_dmc_freq;
			}
		}
	}

	return count;
}

static ssize_t freq_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int i = 0;
	size_t s = 0;

	if (!pxp_debug_dmc_freq || !pxp_debug_ddr_freq) {
		if (aml_db->ops && aml_db->ops->get_freq)
			aml_db->ops->get_freq(aml_db);
	}

	if (dmc_is_asymmetry(aml_db)) {
		for (i = 0; i < get_ddr_number(); i++) {
			s += sprintf(buf + s, "DMC%d: DMC_FREQ %ld MHz, DDR_FREQ %ld MHz\n", i,
				aml_db->data_extern[i].dmc_freq / 1000000,
				aml_db->data_extern[i].ddr_freq / 1000000);
		}
	} else {
		s += sprintf(buf + s, "DMC_FREQ %ld MHz, DDR_FREQ %ld MHz\n",
				aml_db->dmc_freq / 1000000,
				aml_db->ddr_freq / 1000000);
	}

	return s;
}
static CLASS_ATTR_RW(freq);
#else
static ssize_t freq_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int i = 0;
	size_t s = 0;
	unsigned long clk = 0;

	if (aml_db->ops && aml_db->ops->get_freq)
		clk = aml_db->ops->get_freq(aml_db);

	if (dmc_is_asymmetry(aml_db)) {
		for (i = 0; i < get_ddr_number(); i++) {
			s += sprintf(buf + s, "DMC%d: DMC_FREQ %ld MHz, DDR_FREQ %ld MHz\n", i,
				aml_db->data_extern[i].dmc_freq / 1000000,
				aml_db->data_extern[i].ddr_freq / 1000000);
		}
	} else {
		s += sprintf(buf + s, "DMC_FREQ %ld MHz, DDR_FREQ %ld MHz\n",
				aml_db->dmc_freq / 1000000,
				aml_db->ddr_freq / 1000000);
	}

	return s;
}
static CLASS_ATTR_RO(freq);
#endif

void dmc_set_urgent(unsigned int port, unsigned int type)
{
	unsigned int val = 0, addr = 0;

	if (aml_db->cpu_type < DMC_TYPE_G12A) {
		unsigned int port_reg[16] = {
			DMC_AXI0_CHAN_CTRL, DMC_AXI1_CHAN_CTRL,
			DMC_AXI2_CHAN_CTRL, DMC_AXI3_CHAN_CTRL,
			DMC_AXI4_CHAN_CTRL, DMC_AXI5_CHAN_CTRL,
			DMC_AXI6_CHAN_CTRL, DMC_AXI7_CHAN_CTRL,
			DMC_AM0_CHAN_CTRL, DMC_AM1_CHAN_CTRL,
			DMC_AM2_CHAN_CTRL, DMC_AM3_CHAN_CTRL,
			DMC_AM4_CHAN_CTRL, DMC_AM5_CHAN_CTRL,
			DMC_AM6_CHAN_CTRL, DMC_AM7_CHAN_CTRL,};

		if (port >= 16)
			return;
		addr = port_reg[port];
	} else {
		unsigned int port_reg[24] = {
			DMC_AXI0_G12_CHAN_CTRL, DMC_AXI1_G12_CHAN_CTRL,
			DMC_AXI2_G12_CHAN_CTRL, DMC_AXI3_G12_CHAN_CTRL,
			DMC_AXI4_G12_CHAN_CTRL, DMC_AXI5_G12_CHAN_CTRL,
			DMC_AXI6_G12_CHAN_CTRL, DMC_AXI7_G12_CHAN_CTRL,
			DMC_AXI8_G12_CHAN_CTRL, DMC_AXI9_G12_CHAN_CTRL,
			DMC_AXI10_G12_CHAN_CTRL, DMC_AXI1_G12_CHAN_CTRL,
			DMC_AXI12_G12_CHAN_CTRL, 0, 0, 0,
			DMC_AM0_G12_CHAN_CTRL, DMC_AM1_G12_CHAN_CTRL,
			DMC_AM2_G12_CHAN_CTRL, DMC_AM3_G12_CHAN_CTRL,
			DMC_AM4_G12_CHAN_CTRL, DMC_AM5_G12_CHAN_CTRL,
			DMC_AM6_G12_CHAN_CTRL, DMC_AM7_G12_CHAN_CTRL,};

		if (port >= 24 || port_reg[port] == 0)
			return;
		addr = port_reg[port];
	}

	/**
	 *bit 18. force this channel all request to be super urgent request.
	 *bit 17. force this channel all request to be urgent request.
	 *bit 16. force this channel all request to be non urgent request.
	 */
	val = readl(aml_db->ddr_reg[0] + addr);
	val &= (~(0x7 << 16));
	val |= ((type & 0x7) << 16);
	writel(val, aml_db->ddr_reg[0] + addr);
}
EXPORT_SYMBOL(dmc_set_urgent);

ssize_t priority_read(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf,
			 loff_t pos, size_t count)
{
	if (!aml_db->priority) {
		pr_info("Current soc not support ddr priority\n");
		return -EINVAL;
	}

	return priority_display(buf, pos, count);
}

ssize_t priority_write(struct file *filp, struct kobject *kobj,
			  struct bin_attribute *bin_attr, char *buf,
			  loff_t pos, size_t count)
{
	int ret;

	if (!aml_db->priority) {
		pr_info("Current soc not support ddr priority\n");
		return -EINVAL;
	}

	if (aml_db->priority->ver == 2) {
		set_priority_display(buf);
		return count;
	}

	if (!strncmp(buf, "debug", 5)) {
		aml_db->priority->flag |= DDR_PRIORITY_DEBUG;
		return count;
	}

	if (!strncmp(buf, "power", 5)) {
		aml_db->priority->flag |= DDR_PRIORITY_POWER;
		return count;
	}

	ret = priority_tree_write(buf, count);
	if (ret < 0)
		return ret;
	return count;
}
static BIN_ATTR(priority, 0644, priority_read, priority_write, 0);

#if DDR_BANDWIDTH_DEBUG
static ssize_t dump_reg_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int s = 0;

	if (aml_db->ops && aml_db->ops->dump_reg)
		return aml_db->ops->dump_reg(aml_db, buf);

	return s;
}
static CLASS_ATTR_RO(dump_reg);
#endif

static ssize_t cpu_type_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int cpu_type;

	cpu_type = aml_db->cpu_type;
	return sprintf(buf, "%x\n", cpu_type);
}
static CLASS_ATTR_RO(cpu_type);

static ssize_t ddr_port_map_table_show(struct ddr_bandwidth *db, char *buf,
				       bool show_vpu)
{
	unsigned int rid;
	unsigned int mid;
	ssize_t s = 0;
	const struct ddr_port_map *map;
	const char *name = NULL;

	if (!db->port_map_table || !db->port_map_count)
		return 0;

	if (!dmc_dev_is_byte(db)) {
		s += sprintf(buf + s,
			     "Note: sub devices under the same port must set same channel only\n"
			     "And other master not set this channel\n\n");
	}

	s += sprintf(buf + s,
		     " map_id  port_id  sub_id  name\n"
		     " ------  -------  ------  ----\n");

	for (rid = 0; rid < db->port_map_count; rid++) {
		map = &db->port_map_table[rid];
		if (!show_vpu && (map->type == DDR_PORT_MAP_SUB_R ||
				  map->type == DDR_PORT_MAP_SUB_W))
			continue;

		mid = rid;
		name = ddr_port_map_name(map);

		switch (map->type) {
		case DDR_PORT_MAP_MASTER:
			s += sprintf(buf + s,
				     "  %5u  %7u  %-6s  %s\n",
				     mid, (unsigned int)map->port_id, "-",
				     name ? name : "");
			break;
		case DDR_PORT_MAP_DESC_SUB:
			s += sprintf(buf + s,
				     "  %5u  %*s %6d       %s\n",
				     mid, 7, "",
				     (int)map->sub_id,
				     name ? name : "");
			break;
		case DDR_PORT_MAP_SUB_R:
			s += sprintf(buf + s,
				     "  %5u  %*s %6u      R:%s\n",
				     mid, 7, "",
				     (unsigned int)(u8)map->sub_id,
				     name ? name : "");
			break;
		case DDR_PORT_MAP_SUB_W:
			s += sprintf(buf + s,
				     "  %5u  %*s %6u      W:%s\n",
				     mid, 7, "",
				     (unsigned int)(u8)map->sub_id,
				     name ? name : "");
			break;
		default:
			break;
		}
	}

	return s;
}

static ssize_t name_of_ports_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	if (aml_db->port_map_table && aml_db->port_map_count)
		return ddr_port_map_table_show(aml_db, buf, false);

	return 0;
}
static CLASS_ATTR_RO(name_of_ports);

static ssize_t increase_tool_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	unsigned long width_MB = 0;
	u64 min_width = 0;
	unsigned long t_s, t_e;
	u64 t_ns;
	u64 once_size;

	if (sscanf(buf, "%ld %lld", &width_MB, &t_ns) != 2) {
		if (kstrtoul(buf, 0, &width_MB)) {
			pr_info("invalid input:%s\n", buf);
			return -EINVAL;
		}
	} else {
		aml_db->increase_tool.t_ns = t_ns;
	}

	if (aml_db->increase_tool.t_ns == 0)
		aml_db->increase_tool.t_ns = 10000000; //default timer 10ms

	min_width = 1000000000;
	do_div(min_width, aml_db->increase_tool.t_ns);
	if (width_MB) {
		if (min_width > width_MB)
			pr_info("set width_MB too small, min:%lld\n", min_width);

		once_size = width_MB * 1024ULL * 1024;
		do_div(once_size, min_width);

		if (aml_db->increase_tool.increase_MB == 0) {
			aml_db->increase_tool.buf_addr = vmalloc(T_BUF_SIZE);
			if (IS_ERR(aml_db->increase_tool.buf_addr)) {
				pr_err("increase_tool vmalloc failed.\n");
				return count;
			}
		}

		t_s = sched_clock();
		memset(aml_db->increase_tool.buf_addr, 0x55, once_size);
		t_e = sched_clock();
		if ((t_e - t_s) > aml_db->increase_tool.t_ns) {
			pr_info("once copy time %lld than max %ld ns\n",
						aml_db->increase_tool.t_ns,
						t_e - t_s);
			return count;
		}
		pr_info("once copy time %ld ns, size %lld\n", t_e - t_s, once_size);
		aml_db->increase_tool.once_size = once_size;

		hrtimer_cancel(&ddr_hrtimer_timer);
		hrtimer_start(&ddr_hrtimer_timer,
						ktime_set(0, aml_db->increase_tool.t_ns),
						HRTIMER_MODE_REL);
	} else {
		hrtimer_cancel(&ddr_hrtimer_timer);
		aml_db->increase_tool.t_ns = 0;
		aml_db->increase_tool.once_size = 0;

		if (aml_db->increase_tool.increase_MB)
			vfree(aml_db->increase_tool.buf_addr);
	}

	aml_db->increase_tool.increase_MB = width_MB;
	pr_info("ddr will be increase %d MB/s, timer cycle:%lld ns\n",
						aml_db->increase_tool.increase_MB,
						aml_db->increase_tool.t_ns);
	return count;
}

static ssize_t increase_tool_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "current set:%d MB/s, timer cycle:%lld ns\n",
							aml_db->increase_tool.increase_MB,
							aml_db->increase_tool.t_ns);
}
static CLASS_ATTR_RW(increase_tool);

static struct bin_attribute *aml_ddr_tool_bin_attrs[] = {
	&bin_attr_priority,
	NULL
};

static const struct attribute_group aml_ddr_tool_bin_group = {
	.bin_attrs = aml_ddr_tool_bin_attrs,
};

static struct attribute *aml_ddr_tool_attrs[] = {
	&class_attr_port.attr,
	&class_attr_sampling_freq.attr,
	&class_attr_threshold.attr,
	&class_attr_mode.attr,
	&class_attr_busy.attr,
	&class_attr_bandwidth.attr,
	&class_attr_freq.attr,
	&class_attr_cpu_type.attr,
	&class_attr_usage_stat.attr,
	&class_attr_name_of_ports.attr,
	&class_attr_increase_tool.attr,
	&class_attr_ots.attr,
	&class_attr_ots_level.attr,
#if DDR_BANDWIDTH_DEBUG
	&class_attr_dump_reg.attr,
#endif
	NULL
};

static const struct attribute_group aml_ddr_tool_group = {
	.attrs = aml_ddr_tool_attrs,
};

static const struct attribute_group *aml_ddr_tool_groups[] = {
	&aml_ddr_tool_bin_group,
	&aml_ddr_tool_group,
	NULL
};

static struct class aml_ddr_class = {
	.name = "aml_ddr",
	.class_groups = aml_ddr_tool_groups,
};

#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
unsigned long ddr_ssr_access(unsigned long addr, unsigned long value, int rw, int is_sec)
{
	void __iomem *vaddr;
	unsigned int ret = 0;

	if (is_sec)
		return dmc_rw(addr, value, rw);

	addr = round_down(addr, 0x3);
	vaddr = ioremap(addr, 0x4);
	if (!vaddr) {
		return 0;
	}

	if (rw == DMC_READ)
		ret = readl_relaxed(vaddr);
	else if (rw == DMC_WRITE)
		writel_relaxed(value, vaddr);

	iounmap(vaddr);

	return ret;
}

static ssize_t ddr_ssr_enable_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int value;

	if (aml_db->ops && aml_db->ops->ddr_ssr_control) {
		value = aml_db->ops->ddr_ssr_control(0, ENABLE_GET);
		if (value < 0)
			return -EINVAL;
	} else {
		pr_err("current soc not support ddr ssr set\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", value);
}

static ssize_t ddr_ssr_enable_store(struct kobject *kobj, struct kobj_attribute *attr,
				    const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value)) {
		pr_info("invalid input:%s\n", buf);
		return -EINVAL;
	}

	if (aml_db->ops && aml_db->ops->ddr_ssr_control)
		aml_db->ops->ddr_ssr_control(value, ENABLE_SET);
	else
		pr_err("current soc not support ddr ssr set\n");

	return count;
}

static ssize_t ddr_ssr_fmod_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int value;

	if (aml_db->ops && aml_db->ops->ddr_ssr_control) {
		value = aml_db->ops->ddr_ssr_control(0, FMOD_GET);
		if (value < 0)
			return -EINVAL;
	} else {
		pr_err("current soc not support ddr ssr set\n");
		return -EINVAL;
	}

		return sprintf(buf, "%d\n", value);
}

static ssize_t ddr_ssr_fmod_store(struct kobject *kobj, struct kobj_attribute *attr,
				  const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value)) {
		pr_info("invalid input:%s\n", buf);
		return -EINVAL;
	}

	if (aml_db->ops && aml_db->ops->ddr_ssr_control)
		aml_db->ops->ddr_ssr_control(value, FMOD_SET);
	else
		pr_err("current soc not support ddr ssr set\n");

	return count;
}

static ssize_t ddr_ssr_amplitude_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	int value;

	if (aml_db->ops && aml_db->ops->ddr_ssr_control) {
		value = aml_db->ops->ddr_ssr_control(0, AMPLITUDE_GET);
		if (value < 0)
			return -EINVAL;
	} else {
		pr_err("current soc not support ddr ssr set\n");
		return -EINVAL;
	}

	return sprintf(buf, "%d\n", value);
}

static ssize_t ddr_ssr_amplitude_store(struct kobject *kobj, struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	unsigned long value;

	if (kstrtoul(buf, 0, &value)) {
		pr_info("invalid input:%s\n", buf);
		return -EINVAL;
	}

	if (aml_db->ops && aml_db->ops->ddr_ssr_control)
		aml_db->ops->ddr_ssr_control(value, AMPLITUDE_SET);
	else
		pr_err("current soc not support ddr ssr set\n");

	return count;
}

static struct kobj_attribute enable_attr = __ATTR(enable, 0664, ddr_ssr_enable_show,
						  ddr_ssr_enable_store);
static struct kobj_attribute fmod_attr = __ATTR(fmod, 0664, ddr_ssr_fmod_show,
						ddr_ssr_fmod_store);
static struct kobj_attribute amplitude_attr = __ATTR(amplitude, 0664, ddr_ssr_amplitude_show,
						     ddr_ssr_amplitude_store);

static struct attribute *ddr_ssr_attrs[] = {
	&enable_attr.attr,
	&fmod_attr.attr,
	&amplitude_attr.attr,
	NULL,
};

static struct attribute_group ddr_ssr_attr_group = {
	.attrs = ddr_ssr_attrs,
};
#endif

static int __init init_chip_config(int cpu, struct ddr_bandwidth *band)
{
	/* default init dmc_number is 1 */
	band->dmc_number = 1;
	band->mdc.number = 0;

	switch (cpu) {
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A1
	case DMC_TYPE_A1:
		band->ops = &a1_ddr_bw_ops;
		band->channels = 2;
		band->mali_port[0] = -1; /* no mali */
		band->mali_port[1] = -1;
		band->bytes_per_cycle = 8;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GX
	case DMC_TYPE_GXBB:
	case DMC_TYPE_GXTVBB:
		band->ops = &gx_ddr_bw_ops;
		band->channels = 1;
		band->mali_port[0] = 2;
		band->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_GXL
	case DMC_TYPE_GXL:
	case DMC_TYPE_GXM:
	case DMC_TYPE_TXL:
	case DMC_TYPE_TXLX:
	case DMC_TYPE_AXG:
	case DMC_TYPE_GXLX:
	case DMC_TYPE_TXHD:
		band->ops = &gxl_ddr_bw_ops;
		band->channels = 4;
		if (cpu == DMC_TYPE_AXG) {
			band->mali_port[0] = -1; /* no mali */
			band->mali_port[1] = -1;
		} else if (cpu == DMC_TYPE_GXM) {
			band->mali_port[0] = 1; /* port1: mali */
			band->mali_port[1] = -1;
		} else {
			band->mali_port[0] = 1; /* port1: mali0 */
			band->mali_port[1] = 2; /* port2: mali1 */
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_G12
	case DMC_TYPE_G12A:
	case DMC_TYPE_G12B:
	case DMC_TYPE_SM1:
	case DMC_TYPE_TL1:
	case DMC_TYPE_TM2:
	case DMC_TYPE_C1:
	case DMC_TYPE_C2:
	case DMC_TYPE_SC2:
		band->ops = &g12_ddr_bw_ops;
		band->channels = 4;
		if (cpu == DMC_TYPE_C1 || cpu == DMC_TYPE_C2) {
			band->mali_port[0] = -1; /* no mali */
			band->mali_port[1] = -1;
		} else {
			band->mali_port[0] = 1; /* port1: mali */
			band->mali_port[1] = -1;
		}
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T7
	case DMC_TYPE_T7:
	case DMC_TYPE_T3:
		band->ops            = &t7_ddr_bw_ops;
		band->channels     = 8;
		band->dmc_number   = 2;
		band->soc_feature |= DDR_DEVICE_8BIT;
		band->soc_feature |= DDR_WIDTH_IS_64BIT;
		band->mali_port[0] = 3; /* port3: mali */
		band->mali_port[1] = 4;
		break;
	case DMC_TYPE_P1:
		band->ops            = &t7_ddr_bw_ops;
		band->channels     = 8;
		band->dmc_number   = 4;
		band->soc_feature |= DDR_DEVICE_8BIT;
		band->soc_feature |= DDR_WIDTH_IS_64BIT;
		band->mali_port[0] = 3; /* port3: mali */
		band->mali_port[1] = 4;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T5
	case DMC_TYPE_T5:
	case DMC_TYPE_T5D:
		band->ops = &t5_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 1; /* port1: mali */
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S4
	case DMC_TYPE_S4:
	case DMC_TYPE_T5W:
		band->ops = &s4_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 1; /* port1: mali */
		aml_db->mali_port[1] = -1;
		break;
	case DMC_TYPE_A5:
		band->ops = &s4_ddr_bw_ops;
		aml_db->channels = 8;
		band->soc_feature |= PLL_IS_SEC;
		aml_db->mali_port[0] = -1; /* port1: mali */
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_C3
	case DMC_TYPE_C3:
		band->ops = &c3_ddr_bw_ops;
		band->soc_feature |= PLL_IS_SEC;
		aml_db->channels = 8;
		aml_db->mali_port[0] = -1;
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T5M
	case DMC_TYPE_T5M:
		band->ops          = &t5m_ddr_bw_ops;
		band->channels     = 8;
		band->dmc_number   = 2;
		band->soc_feature |= DDR_DEVICE_8BIT;
		band->soc_feature |= DMC_ASYMMETRY;
		band->mali_port[0] = 4;
		band->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S5
	case DMC_TYPE_S5:
		band->ops            = &s5_ddr_bw_ops;
		band->channels     = 8;
		band->dmc_number   = 4;
		band->soc_feature |= DDR_DEVICE_8BIT;
		band->soc_feature |= DDR_WIDTH_IS_64BIT;
		band->mali_port[0] = 4;
		band->mali_port[1] = -1;
		break;
	case DMC_TYPE_T3X:
		band->ops            = &s5_ddr_bw_ops;
		band->channels     = 8;
		band->dmc_number   = 2;
		band->soc_feature |= DDR_DEVICE_8BIT;
		band->mali_port[0] = 4; /* port3: mali */
		band->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S6
	case DMC_TYPE_S6:
		band->ops          = &s6_ddr_bw_ops;
		band->channels     = 24;
		band->dmc_number   = 1;
		band->mali_port[0] = 12;
		band->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_TXHD2
	case DMC_TYPE_TXHD2:
		band->ops = &txhd2_ddr_bw_ops;
		band->soc_feature |= DDR_WIDTH_IS_16BIT;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 1;
		aml_db->mali_port[1] = 2;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S1A
	case DMC_TYPE_S1A:
		band->ops = &s1a_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = -1;
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A4
	case DMC_TYPE_A4:
		band->ops = &a4_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = -1;
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_S7
	case DMC_TYPE_S7:
	case DMC_TYPE_S7D:
		band->ops = &s7_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 12;
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T6D
	case DMC_TYPE_T6D:
		band->ops = &t6d_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 1;
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T6W
	case DMC_TYPE_T6W:
		band->ops = &t6w_ddr_bw_ops;
		aml_db->channels = 8;
		aml_db->mali_port[0] = 1;
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_T6X
	case DMC_TYPE_T6X:
		band->soc_feature |= DDR_IS_POLL;
		band->soc_feature |= DEV_IS_MDC;
		band->ops          = &t6x_ddr_bw_ops;
		band->channels     = 8;
		band->dmc_number   = 2;
		band->mdc.number   = 4;		/* mdc number*/
		band->mali_port[0] = 24;
		band->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_A9
	case DMC_TYPE_A9:
		band->ops          = &a9_ddr_bw_ops;
		band->channels     = 16;
		band->dmc_number   = 1;
		band->mali_port[0] = 12;
		band->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_C4
	case DMC_TYPE_C4:
		band->ops = &c4_ddr_bw_ops;
		band->soc_feature |= DDR_WIDTH_IS_16BIT;
		aml_db->channels = 8;
		aml_db->mali_port[0] = -1;
		aml_db->mali_port[1] = -1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DDR_BANDWIDTH_C5
	case DMC_TYPE_C5:
		band->ops          = &c5_ddr_bw_ops;
		band->prio_ops     = &c5_ddr_prio_ops;
		band->channels     = 11;
		band->dmc_number   = 1;
		band->mali_port[0] = -1;
		band->mali_port[1] = -1;
		break;
#endif
	default:
		pr_err("Can't find ops for chip:%x\n", cpu);
		return -1;
	}
	return 0;
}

static void single_bus_width_init(struct ddr_bandwidth *band)
{
	if (dmc_is_asymmetry(band)) {
		if (band->cpu_type == DMC_TYPE_T5M) {
			band->data_extern[0].data_bus_width = 32;
			band->data_extern[1].data_bus_width = 16;
			band->data_extern[2].data_bus_width = 0;
			band->data_extern[3].data_bus_width = 0;
		}
	}
}

static void get_ddr_external_bus_width(struct ddr_bandwidth *db,
				       unsigned long sec_base)
{
	unsigned long reg, base;

	if (db->cpu_type == DMC_TYPE_TM2)
		base = sec_base + (0x0100 << 2);
	else
		base = sec_base + (0x00da << 2);

	/* each axi cycle transfer 4 external bus */
	reg = dmc_rw(base, 0, DMC_READ);
	if (reg & BIT(16))	/* 16bit external bus width, 2 * 4 = 8*/
		db->bytes_per_cycle = 8;
	else			/* 32bit external bus width, 4 * 4 = 16 */
		db->bytes_per_cycle = 16;
	pr_info("bytes_per_cycle:%d, reg:%lx\n", db->bytes_per_cycle, reg);
}

static int reg_field_access(unsigned char dmc, void *io, u32 *val, int rw,
			    unsigned int reg, unsigned int offset, unsigned int bits_width)
{
	u32 reg_value;
	u32 mask1, mask2;

	mask1 = (1 << bits_width) - 1;
	mask2 = mask1 << offset;
	if (rw == WRITE) {
		if (*val & ~mask1) {
			pr_err("out of range. Max=0x%x, Attempted=0x%x\n",
			       mask1, *val);
			return -1;
		}
		reg_value = readl(io + reg);
		reg_value &= ~mask2;
		reg_value |= ((*val & mask1) << offset);
		writel(reg_value, io + reg);
	} else {
		reg_value = readl(io + reg);
		reg_value &= mask2;
		reg_value = reg_value >> offset;
		*val = reg_value;
		pr_debug("DMC%d reg[0x%04x << 2][%d...%d] = 0x%x\n",
			 dmc, reg >> 2, offset, offset + bits_width - 1, reg_value);
	}

	return 0;
}

int one_dmc_reg_field_access(struct ddr_bandwidth *db, unsigned char dmc, u32 *val, int rw,
			     unsigned int reg, unsigned int offset, unsigned int bits_width)
{
	void *io;

	if (dmc >= MAX_DMC_NUM)
		return -1;

	io = db->ddr_reg[dmc];
	return reg_field_access(dmc, io, val, rw, reg, offset, bits_width);
}

int all_dmc_reg_field_access(struct ddr_bandwidth *db, u32 *val, int rw,
		     unsigned int reg, unsigned int offset, unsigned int bits_width)
{
	unsigned char i;
	void *io;
	int ret;

	for (i = 0; i < db->dmc_number; i++) {
		if (i >= MAX_DMC_NUM)
			return -1;

		io = db->ddr_reg[i];
		ret = reg_field_access(i, io, val, rw, reg, offset, bits_width);
		if (ret < 0)
			return ret;
	}

	return 0;
}

int ddr_axi_sts_alloc(struct ddr_bandwidth *db,
		      unsigned int dmc_chan_sts_offset,
		      unsigned int *axi_chan_sts_offset,
		      unsigned int axi_chan_sts_num)
{
	unsigned int size;

	size = db->dmc_number * sizeof(unsigned int);

	if (size && dmc_chan_sts_offset) {
		db->axi_sts.dmc_chan_sts_offset = dmc_chan_sts_offset;
		db->axi_sts.dmc_chan_sts_value = kzalloc(size, GFP_KERNEL);
		if (!db->axi_sts.dmc_chan_sts_value) {
			pr_err("Failed to allocate memory for dmc chan sts\n");
			return -ENOMEM;
		}
	}

	if (axi_chan_sts_offset) {
		db->axi_sts.axi_chan_sts_num = axi_chan_sts_num;
		size = axi_chan_sts_num * sizeof(unsigned int);
		db->axi_sts.axi_chan_sts_offset = kzalloc(size, GFP_KERNEL);
		if (!db->axi_sts.axi_chan_sts_offset) {
			pr_err("Failed to allocate memory for axi chan sts offset\n");
			goto err_axi_sts;
		}
		memcpy(db->axi_sts.axi_chan_sts_offset, axi_chan_sts_offset, size);

		size = db->axi_sts.axi_chan_sts_num * db->dmc_number * sizeof(unsigned int);
		db->axi_sts.axi_chan_sts_value = kzalloc(size, GFP_KERNEL);
		if (!db->axi_sts.axi_chan_sts_value) {
			pr_err("Failed to allocate memory for axi chan sts value\n");
			goto err_axi_sts;
		}
	}

	return 0;

err_axi_sts:
	kfree(db->axi_sts.dmc_chan_sts_value);
	kfree(db->axi_sts.axi_chan_sts_offset);
	return -ENOMEM;
}

int ddr_axi_sts_init(void)
{
	if (!aml_db->ops->axi_sts_init) {
		pr_err("axi sts is not supported\n");
		return -EINVAL;
	}

	return aml_db->ops->axi_sts_init(aml_db);
}

void ddr_axi_sts_free(void)
{
	aml_db->axi_sts.axi_chan_sts_num = 0;
	aml_db->axi_sts.dmc_chan_sts_offset = 0;

	kfree(aml_db->axi_sts.dmc_chan_sts_value);
	kfree(aml_db->axi_sts.axi_chan_sts_offset);
	kfree(aml_db->axi_sts.axi_chan_sts_value);
}

/* 0: dmc sts, 1: axi sts */
int ddr_axi_sts_read(struct ddr_bandwidth *db, unsigned int type)
{
	int i, j, index, offset;
	void __iomem *base;

	for (i = 0; i < db->dmc_number; i++) {
		base = db->ddr_reg[i];
		if (type) {
			for (j = 0; j < db->axi_sts.axi_chan_sts_num; j++) {
				index = i * db->axi_sts.axi_chan_sts_num + j;
				offset = db->axi_sts.axi_chan_sts_offset[j];
				db->axi_sts.axi_chan_sts_value[index] = __raw_readl(base + offset);
			}
		} else {
			offset = db->axi_sts.dmc_chan_sts_offset;
			db->axi_sts.dmc_chan_sts_value[i] = __raw_readl(base + offset);
		}
	}

	return 0;
}

unsigned int *dmc_chan_sts_get(unsigned int *dmc_num)
{
	if (!aml_db->axi_sts.dmc_chan_sts_value) {
		*dmc_num = 0;
		return NULL;
	}

	*dmc_num = aml_db->dmc_number;
	ddr_axi_sts_read(aml_db, 0);

	return aml_db->axi_sts.dmc_chan_sts_value;
}

unsigned int *axi_chan_sts_get(unsigned int *axi_num)
{
	if (!aml_db->axi_sts.axi_chan_sts_value) {
		*axi_num = 0;
		return NULL;
	}

	*axi_num = aml_db->axi_sts.axi_chan_sts_num * aml_db->dmc_number;
	ddr_axi_sts_read(aml_db, 1);

	return aml_db->axi_sts.axi_chan_sts_value;
}

static int property_access(u64 *val, enum property_type type, int rw)
{
	if (aml_db->ops->property_access) {
		aml_db->ops->property_access(aml_db, val, type, rw);
	} else {
		pr_err("This chip currently does not support the feature.\n");
		*val = 0;
	}
	return 0;
}

static int wbuf_mid_level_get(void *data, u64 *val)
{
	*val = 0;
	property_access(val, WBUF_M, READ);

	return 0;
}

static int wbuf_mid_level_set(void *data, u64 val)
{
	property_access(&val, WBUF_M, WRITE);

	return 0;
}
DEFINE_DEBUGFS_ATTRIBUTE(wbuf_mid_level_fops, wbuf_mid_level_get, wbuf_mid_level_set, "%llu\n");

static void dmc_bus_data_init(int dmc)
{
	int i, j, k, l;
	unsigned char bus, max_bus;
	struct device_id *dd;
	int device_num = 0;
	unsigned char vpu;
	char vpu_name[] = "VPU";

	max_bus = 0;
	for (i = 0; i < aml_db->real_ports; i++) {
		bus = (aml_db->port_desc[i].bus >> (8 * dmc)) & 0xff;
		if (aml_db->async_dmc_num == 0 || (bus & 0x80)) {
			bus &= 0x7f;
			if (bus > max_bus)
				max_bus = bus;
			device_num++;
		}
	}
	aml_db->dmc_bus[dmc].num = max_bus + 1;
	l = sizeof(struct bus_devices) * aml_db->dmc_bus[dmc].num +
		sizeof(struct device_id) * device_num;
	aml_db->dmc_bus[dmc].bus = kzalloc(l, GFP_KERNEL);
	dd = (struct device_id *)(aml_db->dmc_bus[dmc].bus + aml_db->dmc_bus[dmc].num);
	for (i = 0, l = 0; i < aml_db->dmc_bus[dmc].num; i++) {
		k = 0;
		vpu = 0;
		for (j = 0; j < aml_db->real_ports; j++) {
			bus = (aml_db->port_desc[j].bus >> (8 * dmc)) & 0xff;
			if (aml_db->async_dmc_num == 0 || (bus & 0x80)) {
				bus &= 0x7f;
				if (i == bus) {
					dd[l].id = aml_db->port_desc[j].port_id;
					dd[l].name = aml_db->port_desc[j].port_name;
					if (strstr(aml_db->port_desc[j].port_name, vpu_name)) {
						aml_db->vpu_bus_num++;
						vpu = 1;
					}
					l++;
					k++;
				}
			}
		}
		aml_db->dmc_bus[dmc].bus[i].bus = i;
		aml_db->dmc_bus[dmc].bus[i].vpu = vpu;
		aml_db->dmc_bus[dmc].bus[i].num = k;
		aml_db->dmc_bus[dmc].bus[i].device = dd + (l - k);
		if (vpu)
			mutex_init(&aml_db->dmc_bus[dmc].bus[i].side_band.lock);
	}
}

static void dmc_bus_init(void)
{
	int i;
	int num = 0;

	for (i = 0; i < aml_db->real_ports; i++) {
		if (aml_db->port_desc[i].bus & 0x80)
			num |= 1;
		if (aml_db->port_desc[i].bus & 0x8000)
			num |= 2;
		if (aml_db->port_desc[i].bus & 0x800000)
			num |= 4;
		if (aml_db->port_desc[i].bus & 0x80000000)
			num |= 8;
		if (num == 0x0f)
			break;
	}
	aml_db->async_dmc_num = num;
	for (i = 0;
	     ((1 << i) & aml_db->async_dmc_num) || (i == 0 && aml_db->async_dmc_num == 0);
	     i++)
		dmc_bus_data_init(i);
}

static ssize_t bus_devices_display(struct ddr_bandwidth *aml_db, char *buf)
{
	int i, j, k, l;
	ssize_t len = 0;

	for (i = 0, l = 0;
	     ((1 << i) & aml_db->async_dmc_num) || (i == 0 && aml_db->async_dmc_num == 0);
	     i++) {
		len += sprintf(buf + len, "-------------------dmc[%d]-------------------\n", i);
		len += sprintf(buf + len, "bus:\t\tport id:\tdevice\n");
		for (j = 0; j < aml_db->dmc_bus[i].num; j++) {
			for (k = 0; k < aml_db->dmc_bus[i].bus[j].num; k++) {
				if (k == 0) {
					if (aml_db->dmc_bus[i].bus[j].vpu)
						len += sprintf(buf + len, "\033[31;1m");
					len += sprintf(buf + len, "%2d\t\t%3d\t\t%s",
						       j, aml_db->dmc_bus[i].bus[j].device[k].id,
						       aml_db->dmc_bus[i].bus[j].device[k].name);
					if (aml_db->dmc_bus[i].bus[j].vpu &&
					    aml_db->dmc_bus[i].bus[j].num == 1)
						len += sprintf(buf + len, "\033[0m");
					len += sprintf(buf + len, "\n");
				} else {
					len += sprintf(buf + len, "\t\t%3d\t\t%s",
						       aml_db->dmc_bus[i].bus[j].device[k].id,
						       aml_db->dmc_bus[i].bus[j].device[k].name);
					if (aml_db->dmc_bus[i].bus[j].vpu &&
					    ((k + 1) == aml_db->dmc_bus[i].bus[j].num))
						len += sprintf(buf + len, "\033[0m");
					len += sprintf(buf + len, "\n");
				}
			}
		}
	}

	return len;
}

static ssize_t bus_devices_read(struct file *file,
				char __user *to, size_t count, loff_t *ppos)
{
	struct ddr_bandwidth *aml_db;
	int len = 0;
	char *buf;

	if (!file || !file->private_data)
		return 0;

	buf = kvzalloc(2048, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	aml_db = file->private_data;

	len = bus_devices_display(aml_db, buf);

	len = simple_read_from_buffer(to, count, ppos, buf, len);
	kvfree(buf);

	return len;
}

static const struct file_operations bus_devices_fops = {
	.open = simple_open,
	.read = bus_devices_read,
	.llseek = default_llseek,
};

/**
 * parse_side_band_string - Parse side_band format string
 * @input: Input string (e.g., "1:2:rw:3 4 5 6")
 * @dmc:    Output for part1 (unsigned char)
 * @bus:    Output for part2 (unsigned char)
 * @rw:     Output for part3 (1=r, 2=w, 3=rw)
 * @block_bus: Output array for blocks
 * @max_block_num: Maximum capacity of block_bus array
 * @block_num: Actual number of blocks parsed
 *
 * Return: 0 on success, negative error code on failure
 */
int parse_side_band_string(const char *input,
		     unsigned char *dmc,
		     unsigned char *bus,
		     unsigned char *rw,
		     unsigned char *block_bus,
		     unsigned char max_block_num,
		     unsigned char *block_num)
{
	/* Variable declarations */
	char *str = NULL;
	char *cur = NULL;
	char *token = NULL;
	char *parts[4] = { NULL };
	int part_count = 0;
	int i;
	int ret = 0;
	unsigned long val;
	char *p = NULL;
	char *ptr = NULL;
	unsigned char count = 0;
	int valid_block_found = 0;
	char *trimmed = NULL;
	size_t len = 0;
	/* Removed unused endp variable */
	char *dmc_str = NULL;
	char *bus_str = NULL;
	char *rw_str = NULL;
	char *block_str = NULL;
	char *temp = NULL;

	/* Added variables for block parsing */
	char *num_start = NULL;
	char *num_end = NULL;
	char saved_char;
	int parse_ret;

	/* Parameter validation */
	if (!input || !dmc || !bus || !rw || !block_bus || !block_num) {
		pr_err("Invalid parameters (NULL pointer)\n");
		return -EINVAL;
	}

	if (max_block_num == 0) {
		pr_err("max_block_num cannot be zero\n");
		return -EINVAL;
	}

	pr_debug("%s: Parsing input: '%s'\n", __func__, input);

	/* Duplicate input string */
	str = kstrdup(input, GFP_KERNEL);
	if (!str)
		return -ENOMEM;

	cur = str;

	/* Split string by colon and collect non-empty parts */
	for (i = 0; i < 4; i++) {
		token = strsep(&cur, ":");
		if (!token)
			break;

		trimmed = strim(token);
		if (*trimmed) {
			if (part_count < 4) {
				parts[part_count] = trimmed;
				pr_debug("%s: Part %d: '%s'\n", __func__, part_count, trimmed);
				part_count++;
			} else {
				pr_warn("Ignoring extra part: '%s'\n", trimmed);
			}
		} else {
			pr_debug("%s: Skipping empty part\n", __func__);
		}
	}

	/* Initialize defaults */
	*dmc = 0;
	*bus = 0;
	*rw = 3;
	*block_num = 0;

	pr_debug("%s: Found %d valid parts\n", __func__, part_count);

	/* Validate minimum required parts */
	if (part_count < 2) {
		pr_err("Invalid format - at least 2 parts required\n");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Fixed field assignment based on position */
	switch (part_count) {
	case 4:
		dmc_str = parts[0];
		bus_str = parts[1];
		rw_str = parts[2];
		block_str = parts[3];
		break;
	case 3:
		/* Could be: <dmc>:<bus>:<blocks> OR <bus>:<rw>:<blocks> */
		if (isdigit(*parts[0]) && isdigit(*parts[1])) {
			dmc_str = parts[0];
			bus_str = parts[1];
			block_str = parts[2];
		} else {
			bus_str = parts[0];
			rw_str = parts[1];
			block_str = parts[2];
		}
		break;
	case 2:
		bus_str = parts[0];
		block_str = parts[1];
		break;
	default:
		pr_err("Unsupported number of parts: %d\n", part_count);
		ret = -EINVAL;
		goto cleanup;
	}

	pr_debug("%s: Assigned fields: dmc=%s, bus=%s, rw=%s, blocks=%s\n",
		 __func__,
		 dmc_str ? dmc_str : "NULL",
		 bus_str ? bus_str : "NULL",
		 rw_str ? rw_str : "NULL",
		 block_str ? block_str : "NULL");

	/* Parse dmc if present */
	if (dmc_str) {
		p = skip_spaces(dmc_str);
		if (*p) {
			if (kstrtoul(p, 10, &val) == 0 && val <= U8_MAX) {
				*dmc = (unsigned char)val;
				pr_debug("%s: Dmc value: %u\n", __func__, *dmc);
			} else {
				pr_err("Invalid dmc number: '%s'\n", p);
				ret = -EINVAL;
				goto cleanup;
			}
		} else {
			pr_err("Dmc field is empty\n");
			ret = -EINVAL;
			goto cleanup;
		}
	}

	/* Parse bus (must be present) */
	if (bus_str) {
		p = skip_spaces(bus_str);
		if (*p) {
			if (kstrtoul(p, 10, &val) == 0 && val <= U8_MAX) {
				*bus = (unsigned char)val;
				pr_debug("%s: Bus value: %u\n", __func__, *bus);
			} else {
				pr_err("Invalid bus number: '%s'\n", p);
				ret = -EINVAL;
				goto cleanup;
			}
		} else {
			pr_err("Bus field is empty\n");
			ret = -EINVAL;
			goto cleanup;
		}
	} else {
		pr_err("Bus field is missing\n");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Parse rw if present */
	if (rw_str) {
		p = skip_spaces(rw_str);
		len = strlen(p);

		if (len == 1) {
			if (*p == 'r') {
				*rw = 1;
				pr_debug("%s: Rw value: r (1)\n", __func__);
			} else if (*p == 'w') {
				*rw = 2;
				pr_debug("%s: Rw value: w (2)\n", __func__);
			} else {
				pr_err("Invalid rw value: '%s'\n", p);
				ret = -EINVAL;
				goto cleanup;
			}
		} else if (len == 2) {
			if (strcmp(p, "rw") == 0) {
				*rw = 3;
				pr_debug("%s: Rw value: rw (3)\n", __func__);
			} else {
				pr_err("Invalid rw value: '%s'\n", p);
				ret = -EINVAL;
				goto cleanup;
			}
		} else {
			pr_err("Invalid rw value: '%s'\n", p);
			ret = -EINVAL;
			goto cleanup;
		}
	}

	/* Parse block_bus array (must be present) */
	if (!block_str) {
		pr_err("Block_bus field is missing\n");
		ret = -EINVAL;
		goto cleanup;
	}

	pr_debug("%s: Parsing block_bus (max: %u)\n", __func__, max_block_num);
	ptr = block_str;
	count = 0;
	valid_block_found = 0;

	/* Check for special case: entire block_str is exactly "-1" (with whitespace allowed) */
	temp = kstrdup(block_str, GFP_KERNEL);
	if (temp) {
		char *trimmed_block = strim(temp);

		if (strcmp(trimmed_block, "-1") == 0) {
			block_bus[0] = (unsigned char)-1; // 0xFF (255)
			*block_num = 1;
			valid_block_found = 1;
			pr_debug("%s: Special case: block_bus set to -1 (255)\n", __func__);
			kfree(temp);
			goto block_parse_done;
		}
		kfree(temp);
	}

	/* Check for invalid "-1" with additional data */
	if (strstr(block_str, "-1")) {
		pr_err("Invalid format: '-1' must be the only element in block_bus\n");
		ret = -EINVAL;
		goto cleanup;
	}

	/* Normal block parsing */
	while (*ptr && count < max_block_num) {
		/* Skip leading spaces */
		ptr = skip_spaces(ptr);
		if (!*ptr)
			break;

		/* Initialize variables for this iteration */
		num_start = ptr;
		num_end = ptr;
		saved_char = '\0';
		parse_ret = -EINVAL;
		val = 0;

		/* Find the end of the current number */
		while (*num_end && !isspace(*num_end))
			num_end++;

		/* Temporarily null-terminate the number string */
		saved_char = *num_end;
		if (saved_char != '\0')
			*num_end = '\0';

		/* Parse the number */
		parse_ret = kstrtoul(ptr, 10, &val);

		/* Restore the original character */
		if (saved_char != '\0')
			*num_end = saved_char;

		/* Move pointer to the end of the number */
		ptr = num_end;

		/* Check parsing result */
		if (parse_ret == 0) {
			if (val <= U8_MAX) {
				block_bus[count] = (unsigned char)val;
				pr_debug("%s: Block_bus[%u] = %u\n",
					 __func__, count, block_bus[count]);
				count++;
				valid_block_found = 1;
				continue;
			} else {
				pr_warn("Number out of range (0-255): %lu\n", val);
			}
		} else {
			pr_warn("Invalid number: '%.*s'\n",
				(int)(num_end - num_start), num_start);
		}

		/* Skip to next space (if not already there) */
		if (*ptr && !isspace(*ptr))
			ptr++;
	}

	if (!valid_block_found) {
		pr_err("No valid numbers in block_bus: '%s'\n", block_str);
		ret = -EINVAL;
		goto cleanup;
	}

	*block_num = count;

block_parse_done:
	pr_debug("%s: Found %u valid block values\n", __func__, *block_num);

	pr_debug("%s: Parse successful: dmc=%u, bus=%u, rw=%u, blocks=%u\n",
		__func__, *dmc, *bus, *rw, *block_num);

cleanup:
	kfree(str);

	if (ret != 0)
		pr_err("Parse failed with error %d\n", ret);

	return ret;
}

static int compare_uchar(const void *a, const void *b)
{
	return *(const unsigned char *)a - *(const unsigned char *)b;
}

static int set_side_band(struct ddr_bandwidth *aml_db, unsigned char dmc,
			 unsigned char bus, unsigned char rw,
			 unsigned char *block_bus, unsigned char block_num)
{
	int ret;
	int bus_index;
	int i, j;

	if (!((1 << dmc) & aml_db->async_dmc_num) && !(dmc == 0 && aml_db->async_dmc_num == 0)) {
		pr_err("Invalid dmc: %d\n", dmc);
		return -1;
	}

	for (i = 0; i < aml_db->dmc_bus[dmc].num; i++)
		if (aml_db->dmc_bus[dmc].bus[i].bus == bus)
			break;
	if (i == aml_db->dmc_bus[dmc].num) {
		pr_err("Invalid bus: %d\n", bus);
		return -1;
	}
	if (!aml_db->dmc_bus[dmc].bus[i].vpu) {
		pr_err("Bus %d invalid: not VPU bus, cannot be sideband channel.\n", bus);
		return -1;
	}
	bus_index = i;

	if (block_num == 1 && block_bus[0] == (unsigned char)-1) {
		mutex_lock(&aml_db->dmc_bus[dmc].bus[i].side_band.lock);
		aml_db->dmc_bus[dmc].bus[i].side_band.flags = 0;
		aml_db->dmc_bus[dmc].bus[i].side_band.rw = 0;
		aml_db->dmc_bus[dmc].bus[i].side_band.block_num = 0;
		ret = aml_db->ops->side_band(aml_db, dmc, i);
		mutex_unlock(&aml_db->dmc_bus[dmc].bus[i].side_band.lock);
	} else {
		if (rw == 0) {
			pr_err("Invalid rw = 0\n");
			return -1;
		}
		for (i = 0; i < block_num; i++) {
			for (j = 0; j < aml_db->dmc_bus[dmc].num; j++)
				if (aml_db->dmc_bus[dmc].bus[j].bus == block_bus[i])
					break;
			if (j == aml_db->dmc_bus[dmc].num)
				break;
		}
		if (i != block_num) {
			pr_err("Invalid block_bus: %d\n", block_bus[i]);
			return -1;
		}
		i = bus_index;

		mutex_lock(&aml_db->dmc_bus[dmc].bus[i].side_band.lock);
		aml_db->dmc_bus[dmc].bus[i].side_band.flags = 1;
		aml_db->dmc_bus[dmc].bus[i].side_band.rw = rw;
		memcpy(aml_db->dmc_bus[dmc].bus[i].side_band.block_bus, block_bus, block_num);
		aml_db->dmc_bus[dmc].bus[i].side_band.block_num = block_num;
		sort(aml_db->dmc_bus[dmc].bus[i].side_band.block_bus, block_num,
		     sizeof(unsigned char), compare_uchar, NULL);
		ret = aml_db->ops->side_band(aml_db, dmc, i);
		mutex_unlock(&aml_db->dmc_bus[dmc].bus[i].side_band.lock);
	}

	return ret;
}

static ssize_t side_band_write(struct file *file,
			       const char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	struct ddr_bandwidth *aml_db;
	char *buf;
	unsigned char dmc, bus, rw, block_num;
	unsigned char block_bus[32];
	int ret;

	if (!file || !file->private_data)
		return 0;

	aml_db = file->private_data;

	if (!aml_db->ops->side_band) {
		pr_err("The driver currently does not support the side band functionality of the chip\n");
		return -1;
	}

	buf = memdup_user(user_buf, count);
	buf[count - 1] = '\0';

	ret = parse_side_band_string(buf, &dmc, &bus, &rw,
				     block_bus, sizeof(block_bus), &block_num);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	ret = set_side_band(aml_db, dmc, bus, rw, block_bus, block_num);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	kfree(buf);
	return count;
}

static ssize_t side_band_display(struct ddr_bandwidth *aml_db, char *buf)
{
	ssize_t len = 0;
	int i, j, k, l;

	if (!aml_db || !buf)
		return 0;

	for (i = 0, k = 0;
	     ((1 << i) & aml_db->async_dmc_num) || (i == 0 && aml_db->async_dmc_num == 0);
	     i++) {
		len += sprintf(buf + len, "-------------------dmc[%d]-------------------\n", i);
		for (j = 0; j < aml_db->dmc_bus[i].num; j++) {
			if (!aml_db->dmc_bus[i].bus[j].vpu)
				continue;
			mutex_lock(&aml_db->dmc_bus[i].bus[j].side_band.lock);
			if (aml_db->dmc_bus[i].bus[j].side_band.flags) {
				len += sprintf(buf + len, "[%d] %d:", k, j);
				if (aml_db->dmc_bus[i].bus[j].side_band.rw & 1)
					len += sprintf(buf + len, "r");
				if (aml_db->dmc_bus[i].bus[j].side_band.rw & 2)
					len += sprintf(buf + len, "w");
				if (aml_db->dmc_bus[i].bus[j].side_band.rw)
					len += sprintf(buf + len, ":");
				for (l = 0;
				     l < aml_db->dmc_bus[i].bus[j].side_band.block_num - 1;
				     l++)
					len += sprintf(buf + len, "%d ",
						aml_db->dmc_bus[i].bus[j].side_band.block_bus[l]);
				len += sprintf(buf + len, "%d\n",
					       aml_db->dmc_bus[i].bus[j].side_band.block_bus[l]);
				k++;
			}
			mutex_unlock(&aml_db->dmc_bus[i].bus[j].side_band.lock);
		}
	}

	return len;
}

static ssize_t side_band_read(struct file *file,
			      char __user *to, size_t count, loff_t *ppos)
{
	struct ddr_bandwidth *aml_db;
	int len = 0, ret;
	char *buf;

	if (!file || !file->private_data)
		return 0;

	buf = kvzalloc(2048, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	aml_db = file->private_data;

	len = side_band_display(aml_db, buf);

	ret = simple_read_from_buffer(to, count, ppos, buf, len);
	kvfree(buf);

	return ret;
}

static const struct file_operations side_band_fops = {
	.open = simple_open,
	.read = side_band_read,
	.write = side_band_write,
	.llseek = default_llseek,
};

static unsigned long long side_band_enable_count;
static unsigned long long side_band_disable_count;
int enable_side_band(struct dmc_side_band *sb)
{
	int len = 0, ret;
	char buf[1800];
	static unsigned long warn_time;

	ret = set_side_band(aml_db, sb->dmc, sb->bus, sb->rw, sb->block_bus, sb->block_num);

	side_band_enable_count++;
	if (printk_timed_ratelimit(&warn_time, 10000)) {
		len = side_band_display(aml_db, buf);
		pr_info("side band enable count %llu, disable count %llu current status:\n %s\n",
			side_band_enable_count, side_band_disable_count, buf);
	}
	return ret;
}
EXPORT_SYMBOL(enable_side_band);

int disable_side_band(unsigned char dmc, unsigned char bus)
{
	unsigned char block_bus = -1;

	side_band_disable_count++;
	return set_side_band(aml_db, dmc, bus, 0, &block_bus, 1);
}
EXPORT_SYMBOL(disable_side_band);

int get_vpu_bus_num(void)
{
	return aml_db->vpu_bus_num;
}
EXPORT_SYMBOL(get_vpu_bus_num);

int get_side_band(struct dmc_side_band *sb, unsigned char num)
{
	int i, j, k;

	for (i = 0, k = 0;
	     ((1 << i) & aml_db->async_dmc_num) || (i == 0 && aml_db->async_dmc_num == 0);
	     i++) {
		for (j = 0; j < aml_db->dmc_bus[i].num && k < num; j++) {
			mutex_lock(&aml_db->dmc_bus[i].bus[j].side_band.lock);
			if (aml_db->dmc_bus[i].bus[j].side_band.flags) {
				sb->dmc = i;
				sb->bus = j;
				sb->rw = aml_db->dmc_bus[i].bus[j].side_band.rw;
				sb->block_num = aml_db->dmc_bus[i].bus[j].side_band.block_num;
				memcpy(sb->block_bus,
				       aml_db->dmc_bus[i].bus[j].side_band.block_bus,
				       sb->block_num);
				k++;
			}
			mutex_unlock(&aml_db->dmc_bus[i].bus[j].side_band.lock);
		}
	}
	return k;
}
EXPORT_SYMBOL(get_side_band);

static unsigned long tmp_smc_reg;
static ssize_t smc_rw_read(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos)
{
	int len = 0;
	char buf[128];

	len += snprintf(buf + len, sizeof(buf), "[0x%lx]:0x%lx\n",
			tmp_smc_reg, dmc_rw(tmp_smc_reg, 0, 0));

	return simple_read_from_buffer(user_buf, count, ppos, buf, len);
}

static ssize_t smc_rw_write(struct file *file, const char __user *user_buf,
			size_t count, loff_t *ppos)
{
	char buf[256];
	unsigned long addr, value;

	if (count > sizeof(buf) - 1)
		return -EFAULT;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	if (sscanf(buf, "%lx %lx", &addr, &value) != 2) {
		if (kstrtoul_from_user(user_buf, count, 0, &addr)) {
			pr_info("invalid input:%s\n", buf);
			return -EINVAL;
		}
		tmp_smc_reg = addr;
		pr_info("set addr:%08lx\n", tmp_smc_reg);
		return count;
	}
	tmp_smc_reg = addr;
	dmc_rw(tmp_smc_reg, value, 1);

	return count;
}

static const struct file_operations smc_rw_fops = {
	.open = simple_open,
	.read = smc_rw_read,
	.write = smc_rw_write,
	.llseek = default_llseek,
};

#if IS_ENABLED(CONFIG_AMLOGIC_CLASS_DEBUG)
static ssize_t wbuf_mid_level_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	u64 val;

	if (kstrtoul(buf, 10, (unsigned long *)&val)) {
		pr_info("invalid input:%s\n", buf);
		return 0;
	}

	property_access(&val, WBUF_M, WRITE);
	return count;
}

static ssize_t wbuf_mid_level_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	u64 val;

	property_access(&val, WBUF_M, READ);

	return sprintf(buf, "%llu\n", val);
}

static struct kobj_attribute wbuf_mid_level_attr = __ATTR_RW(wbuf_mid_level);

static ssize_t bus_device_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return bus_devices_display(aml_db, buf);
}

static struct kobj_attribute bus_device_attr = __ATTR_RO(bus_device);

static ssize_t side_band_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buffer, size_t count)
{
	char *buf;
	unsigned char dmc, bus, rw, block_num;
	unsigned char block_bus[32];
	int ret;

	if (!aml_db->ops->side_band) {
		pr_err("The driver currently does not support the side band functionality of the chip\n");
		return -1;
	}

	buf = kmemdup(buffer, count, GFP_KERNEL);
	buf[count - 1] = '\0';

	ret = parse_side_band_string(buf, &dmc, &bus, &rw,
				     block_bus, sizeof(block_bus), &block_num);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	ret = set_side_band(aml_db, dmc, bus, rw, block_bus, block_num);
	if (ret < 0) {
		kfree(buf);
		return ret;
	}

	kfree(buf);

	return count;
}

static ssize_t side_band_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	return side_band_display(aml_db, buf);
}

static struct kobj_attribute side_band_attr = __ATTR_RW(side_band);

static ssize_t smc_rw_store(struct kobject *kobj, struct kobj_attribute *attr,
			    const char *buf, size_t count)
{
	unsigned long addr, value;

	if (sscanf(buf, "%lx %lx", &addr, &value) != 2) {
		if (kstrtoul(buf, 0, &addr)) {
			pr_info("invalid input:%s\n", buf);
			return -EINVAL;
		}
		tmp_smc_reg = addr;
		pr_info("set addr:%08lx\n", tmp_smc_reg);
		return count;
	}
	tmp_smc_reg = addr;
	dmc_rw(tmp_smc_reg, value, 1);
	pr_info("write addr:%08lx, value:%08lx\n", tmp_smc_reg, value);
	return count;
}

static ssize_t smc_rw_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "[%lx]:%lx\n", tmp_smc_reg, dmc_rw(tmp_smc_reg, 0, 0));
}

static struct kobj_attribute smc_rw_attr = __ATTR_RW(smc_rw);

static struct attribute *aml_ddr_attrs[] = {
	&wbuf_mid_level_attr.attr,
	&bus_device_attr.attr,
	&side_band_attr.attr,
	&smc_rw_attr.attr,
	NULL,
};

static const struct attribute_group aml_ddr_group = {
	.name = "aml_ddr",
	.attrs = aml_ddr_attrs,
};
#endif

static void debugfs_init(void)
{
	aml_db->debugfs = debugfs_create_dir("aml_ddr", NULL);
	debugfs_create_file("wbuf_mid_level", 0660, aml_db->debugfs, aml_db, &wbuf_mid_level_fops);
	debugfs_create_file("bus_device", 0660, aml_db->debugfs, aml_db, &bus_devices_fops);
	debugfs_create_file("side_band", 0660, aml_db->debugfs, aml_db, &side_band_fops);
	debugfs_create_file("smc_rw", 0660, aml_db->debugfs, aml_db, &smc_rw_fops);
#if IS_ENABLED(CONFIG_AMLOGIC_CLASS_DEBUG)
	amlogic_class_debug_create_dir(&aml_ddr_group, 2);
#endif
}

/*
 * ddr_bandwidth_probe only executes before the init process starts
 * to run, so add __ref to indicate it is okay to call __init function
 * ddr_find_port_desc
 */
static int __init ddr_bandwidth_probe(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
	struct class_dev_iter iter;
	struct subsys_private *sp;
	struct kobject *ssr_kobj;
#endif
	int r = 0, i, count;
#ifdef CONFIG_OF
	struct device_node *node = pdev->dev.of_node;
	/*struct pinctrl *p;*/
	struct resource *res;
	resource_size_t *base;
	unsigned int sec_base = 0, freq_reg = 0, bus_width_reg[4];
	int io_idx = 0;
#endif
	struct ddr_port_desc *desc = NULL;
	int pcnt;

	pr_debug("%s, %d\n", __func__, __LINE__);
	aml_db = devm_kzalloc(&pdev->dev, sizeof(*aml_db), GFP_KERNEL);
	if (!aml_db)
		return -ENOMEM;

	aml_db->cpu_type = (unsigned long)of_device_get_match_data(&pdev->dev);
	pr_debug("chip type:0x%x\n", aml_db->cpu_type);
	if (aml_db->cpu_type < DMC_TYPE_M8B) {
		pr_info("unsupport chip type:%d\n", aml_db->cpu_type);
		aml_db = NULL;
		return -EINVAL;
	}

	/* find and configure port description */
	pcnt = ddr_find_port_desc_type(aml_db->cpu_type, &desc, 1);
	if (pcnt < 0) {
		pr_err("can't find port descriptor,cpu:%d\n", aml_db->cpu_type);
	} else {
		aml_db->port_desc = desc;
		aml_db->real_ports = pcnt;
	}

	sub_dev_desc_init(aml_db->cpu_type, &aml_db->sub_dev);

	ddr_init_port_map_table(aml_db);

	if (init_chip_config(aml_db->cpu_type, aml_db)) {
		aml_db = NULL;
		return -EINVAL;
	}

	ddr_priority_init(aml_db->cpu_type);

	single_bus_width_init(aml_db);

#ifdef CONFIG_OF
	/* resource 0 for ddr register base */
	for (i = 0; i < aml_db->dmc_number; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, io_idx);
		if (res) {
			base = ioremap(res->start, resource_size(res));
			aml_db->ddr_reg[i] = base;
		} else {
			pr_err("can't get dmc reg base\n");
			aml_db = NULL;
			return -EINVAL;
		}
		io_idx++;
	}

	for (i = 0; i < aml_db->mdc.number; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, io_idx);
		if (res) {
			aml_db->mdc.reg_base[i] = ioremap(res->start, resource_size(res));
		} else {
			pr_err("can't get mdc reg base\n");
			aml_db = NULL;
			return -EINVAL;
		}
		io_idx++;
	}

	/* next for pll register base */
	res = platform_get_resource(pdev, IORESOURCE_MEM, io_idx);
	if (res) {
		if (dmc_pll_is_sec(aml_db)) {
			aml_db->pll_reg = (void *)res->start;
		} else {
			base = ioremap(res->start, resource_size(res));
			aml_db->pll_reg = (void *)base;
		}
	} else {
		pr_err("can't get ddr reg %d base\n", io_idx);
		aml_db = NULL;
		return -EINVAL;
	}

	aml_db->irq_num = of_irq_get(node, 0);
	r = of_property_read_u32(node, "sec_base", &sec_base);
	if (r < 0) {
		aml_db->bytes_per_cycle = 16;
		pr_debug("can't find sec_base, set bytes_per_cycle to 16\n");
	} else {
		get_ddr_external_bus_width(aml_db, sec_base);
	}

	r = of_property_read_u32(node, "freq_reg", &freq_reg);
	if (r < 0)
		aml_db->freq_reg = (void *)0;
	else
		aml_db->freq_reg = (void *)ioremap(freq_reg, 4);

	count = of_property_count_u32_elems(node, "bus_width_reg");
	if (count) {
		r = of_property_read_u32_array(node, "bus_width_reg", bus_width_reg, count);
		for (i = 0; i < count; i++) {
			if (r < 0) {
				aml_db->bus_width_reg[i].io_addr = (void *)0;
				aml_db->bus_width_reg[i].addr = 0;
			} else {
				aml_db->bus_width_reg[i].io_addr =
						(void *)ioremap(bus_width_reg[i], 4);
				aml_db->bus_width_reg[i].addr = bus_width_reg[i];
			}
		}
	}
#endif
	if (aml_db->ops && aml_db->ops->get_freq)
		aml_db->ops->get_freq(aml_db);

	if (aml_db->ops && aml_db->ops->bus_width_init)
		aml_db->ops->bus_width_init(aml_db);

	if (aml_db->ops && aml_db->ops->vpu_status)
		aml_db->ops->vpu_status(1);

	if (aml_db->ops && aml_db->ops->chip_init)
		aml_db->ops->chip_init(aml_db);

	raw_spin_lock_init(&aml_db->lock);
	aml_db->clock_count = aml_db->ops->get_freq(aml_db) / DEFAULT_SAMPLING_FREQ; // 1000hz
	aml_db->ddr_poll_ns = 1000000000 / DEFAULT_SAMPLING_FREQ;	// 1ms
	aml_db->mode = MODE_DISABLE;
	aml_db->threshold = DEFAULT_THRESHOLD * aml_db->bytes_per_cycle *
			    (aml_db->clock_count / 1000);

	if (!aml_db->ops->config_port)
		return -EINVAL;

	aml_db->ost.levels.cur_level = init_ots_level;
	if (aml_db->ops && aml_db->ops->outstanding_init)
		aml_db->ops->outstanding_init(aml_db);

	r = class_register(&aml_ddr_class);

	dmc_bus_init();

	debugfs_init();

	ddr_hrtimer_init();
	ddr_poll_init();
#if IS_ENABLED(CONFIG_AMLOGIC_DDR_SSR)
	class_dev_iter_init(&iter, &aml_ddr_class, NULL, NULL);
	sp = iter.sp;
	class_dev_iter_exit(&iter);
	ssr_kobj = kobject_create_and_add("ddr_ssr", &sp->subsys.kobj);

	r = sysfs_create_group(ssr_kobj, &ddr_ssr_attr_group);
	if (r) {
		pr_info("ddr_ssr test creat failed %d\n", r);
		return r;
	}
	pr_emerg("NOTICE: ddr_ssr node created, registers can be modify\n");
#endif

	ddr_axi_sts_init();

	return 0;
}

static void ddr_bandwidth_remove(struct platform_device *pdev)
{
	if (aml_db) {
		int i;

		ddr_axi_sts_free();
		class_destroy(&aml_ddr_class);
		free_irq(aml_db->irq_num, aml_db);
		ddr_free_port_map_table(aml_db);
		kfree(aml_db->port_desc);
		for (i = 0; i < aml_db->dmc_number; i++)
			iounmap(aml_db->ddr_reg[i]);
		iounmap(aml_db->pll_reg);
		aml_db = NULL;
	}
}

#ifdef CONFIG_OF
static const struct of_device_id aml_ddr_bandwidth_dt_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,ddr-bandwidth-m8b",
		.data = (void *)DMC_TYPE_M8B,	/* chip id */
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gx",
		.data = (void *)DMC_TYPE_GXBB,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gxl",
		.data = (void *)DMC_TYPE_GXL,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gxm",
		.data = (void *)DMC_TYPE_GXM,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-gxlx",
		.data = (void *)DMC_TYPE_GXLX,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-axg",
		.data = (void *)DMC_TYPE_AXG,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-txl",
		.data = (void *)DMC_TYPE_TXL,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-txlx",
		.data = (void *)DMC_TYPE_TXLX,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-txhd",
		.data = (void *)DMC_TYPE_TXHD,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-tl1",
		.data = (void *)DMC_TYPE_TL1,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-c1",
		.data = (void *)DMC_TYPE_C1,
	},
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic,ddr-bandwidth-a1",
		.data = (void *)DMC_TYPE_A1,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-c2",
		.data = (void *)DMC_TYPE_C2,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-g12a",
		.data = (void *)DMC_TYPE_G12A,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-sm1",
		.data = (void *)DMC_TYPE_SM1,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-g12b",
		.data = (void *)DMC_TYPE_G12B,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-tm2",
		.data = (void *)DMC_TYPE_TM2,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t5",
		.data = (void *)DMC_TYPE_T5,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t5d",
		.data = (void *)DMC_TYPE_T5D,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t5w",
		.data = (void *)DMC_TYPE_T5W,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t7",
		.data = (void *)DMC_TYPE_T7,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t3",
		.data = (void *)DMC_TYPE_T3,
	},
#endif
	{
		.compatible = "amlogic,ddr-bandwidth-s4",
		.data = (void *)DMC_TYPE_S4,
	},
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic,ddr-bandwidth-sc2",
		.data = (void *)DMC_TYPE_SC2,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-p1",
		.data = (void *)DMC_TYPE_P1,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-c3",
		.data = (void *)DMC_TYPE_C3,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t5m",
		.data = (void *)DMC_TYPE_T5M,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-s5",
		.data = (void *)DMC_TYPE_S5,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-s6",
		.data = (void *)DMC_TYPE_S6,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t3x",
		.data = (void *)DMC_TYPE_T3X,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-txhd2",
		.data = (void *)DMC_TYPE_TXHD2,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-a4",
		.data = (void *)DMC_TYPE_A4,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-a5",
		.data = (void *)DMC_TYPE_A5,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-s7",
		.data = (void *)DMC_TYPE_S7,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-s7d",
		.data = (void *)DMC_TYPE_S7D,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t6d",
		.data = (void *)DMC_TYPE_T6D,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t6w",
		.data = (void *)DMC_TYPE_T6W,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-t6x",
		.data = (void *)DMC_TYPE_T6X,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-c4",
		.data = (void *)DMC_TYPE_C4,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-a9",
		.data = (void *)DMC_TYPE_A9,
	},
	{
		.compatible = "amlogic,ddr-bandwidth-c5",
		.data = (void *)DMC_TYPE_C5,
	},
#endif
	{
		.compatible = "amlogic,ddr-bandwidth-s1a",
		.data = (void *)DMC_TYPE_S1A,
	},
	{}
};
#endif

static struct platform_driver ddr_bandwidth_driver = {
	.driver = {
		.name = "amlogic,ddr-bandwidth",
		.owner = THIS_MODULE,
	},
	.remove = ddr_bandwidth_remove,
};

int __init ddr_bandwidth_init(void)
{
#ifdef CONFIG_OF
	const struct of_device_id *match_id;

	match_id = aml_ddr_bandwidth_dt_match;
	ddr_bandwidth_driver.driver.of_match_table = match_id;
#endif

	platform_driver_probe(&ddr_bandwidth_driver,
			      ddr_bandwidth_probe);

	return 0;
}

void ddr_bandwidth_exit(void)
{
	debugfs_remove(aml_db->debugfs);
	platform_driver_unregister(&ddr_bandwidth_driver);
	ddr_hrtimer_cancel();
	hrtimer_cancel(&aml_db->ddr_poll_timer);
}

