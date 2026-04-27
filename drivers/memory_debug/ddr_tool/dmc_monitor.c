// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
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
#include <linux/of_device.h>
#include <linux/irqreturn.h>
#include <linux/module.h>
#include <linux/mm.h>

#include <linux/cpu.h>
#include <linux/smp.h>
#include <linux/kallsyms.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/highmem.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include "dmc_monitor.h"
#include "ddr_port.h"
#include <linux/amlogic/gki_module.h>
#include <trace/hooks/mm.h>
#include <trace/hooks/fault.h>
#include <asm/module.h>
#include <linux/mmzone.h>
#include <trace/hooks/traps.h>
#include <linux/amlogic/aml_ddr_tool.h>
#include <linux/amlogic/user_fault.h>
#include <linux/sched/cputime.h>
#include <linux/sched/clock.h>
#include <linux/string.h>

#define CREATE_TRACE_POINTS
#include "dmc_trace.h"

// #define DEBUG
#define DMC_VERSION		"1.11.5"

#define IRQ_CHECK		0
#define IRQ_CLEAR		1

#define DMC_SEC_RATELIMIT_BURST	15
#define DMC_RATELIMIT_BURST	30
#define dmc_pr_crit(fmt, addr, val, status, port, subport, page_flags, lru, trace, order, time, rw, title, invalid, same, total, rs)	\
({																	\
	if (__ratelimit(rs))														\
		pr_crit(fmt, addr, val, status, port, subport, page_flags, lru, trace, order, time, rw, title, invalid, same, total);	\
})

struct dmc_monitor *dmc_mon;

/*if it not be filter, use tvp secure soc will be serror due dmc read addr value*/
char exception_filter_sym[] = "alloc_from_cma";
char default_filter_dev[] = "USB,ETH,EMMC,TCON";
char default_filter_sym[] = "__dma_direct_alloc_pages,alloc_page_boost_work_func,dma_direct_alloc";

static enum dmc_mode_type init_dmc_mode = DMC_MODE_RESERVED;
static unsigned long init_dev_mask;
static unsigned long init_start_addr;
static unsigned long init_end_addr;
static unsigned long init_dmc_debug;

static int dmc_original_debug;

/* irq thread enabled */
static unsigned char init_dmc_irq_thread_en = 1;
/* ns, irq check time, default is 5000000*/
static unsigned long init_dmc_irq_check_ns = 5000000;
/* default 5ms check once, if thread run over ratio(default 32/128), thread will be sleep */
static unsigned char init_dmc_irq_ratio = 32;
/* sleep time(us) when irq too much */
static unsigned long init_dmc_irq_usleep = 500;
/* thread irq recheck time */
static unsigned long init_thread_recheck_ns = 5000;

unsigned long get_recheck_ns(void)
{
	return init_thread_recheck_ns;
}

static int early_dmc_param(char *buf)
{
	unsigned long s_addr, e_addr, mask, debug = 0;
	/*
	 * Patten:  dmc_monitor=[start_addr],[end_addr],[mask]
	 * Example: dmc_monitor=0x00000000,0x20000000,0x7fce
	 * debug: cat /sys/class/dmc_monitor/debug
	 */
	if (!buf)
		return -EINVAL;

	if (strstr(buf, "default") == buf) {
		init_dmc_mode = DMC_MODE_DEFAULT;
		pr_info("dmc enabled default mode\n");
		return 1;
	} else if (sscanf(buf, "%lx,%lx,%lx,%lx", &s_addr, &e_addr, &mask, &debug) != 4) {
		if (sscanf(buf, "%lx,%lx,%lx", &s_addr, &e_addr, &mask) != 3)
			return -EINVAL;
	}

	init_dmc_mode = DMC_MODE_NORMAL;
	init_start_addr = s_addr;
	init_end_addr   = e_addr;
	init_dev_mask   = mask;
	init_dmc_debug = debug;

	pr_debug("%s, buf:%s, %lx-%lx, %lx, %lx\n",
		__func__, buf, s_addr, e_addr, mask, debug);

	return 1;
}
__setup("dmc_monitor=", early_dmc_param);

/* only used to dmc filter set on uboot cmdline */
static char dmc_filter_early_buf[1024];
static int early_dmc_filter(char *buf)
{
	/*
	 * set dev name or symbol will be ignore print when access
	 * Patten:  dmc_filter=[dev_name],[or symbol_name]
	 * Example: dmc_monitor=USB,ETH,__dma_direct_alloc_pages
	 * debug: cat /sys/class/dmc_monitor/filter
	 */
	snprintf(dmc_filter_early_buf, 1024, "%s", buf);
	pr_debug("dmc_filter, buf:%s\n", buf);

	return 1;
}
__setup("dmc_filter=", early_dmc_filter);

static int early_dmc_irq_thread(char *buf)
{
	int ret;
	unsigned char irq_thread_en = 0, irq_ratio = 0;
	unsigned long dmc_irq_check_time = 0, dmc_irq_usleep = 0;
	unsigned long thread_recheck_ns = 0;

	ret = sscanf(buf, "%hhd,%ld,%hhd,%ld, %ld",
		     &irq_thread_en, &dmc_irq_check_time, &irq_ratio,
		     &dmc_irq_usleep, &thread_recheck_ns);
	if (ret < 0)
		return -EINVAL;

	init_dmc_irq_thread_en = irq_thread_en;

	if (dmc_irq_check_time)
		init_dmc_irq_check_ns = dmc_irq_check_time;

	if (irq_ratio)
		init_dmc_irq_ratio = irq_ratio;

	if (dmc_irq_usleep)
		init_dmc_irq_usleep = dmc_irq_usleep;

	if (thread_recheck_ns)
		init_thread_recheck_ns = thread_recheck_ns;

	pr_debug("dmc_irq_thread, en:%d, check time:%ld, ratio:%d, usleep_time:%ld, thread recheck:%ld\n",
			init_dmc_irq_thread_en, init_dmc_irq_check_ns,
			init_dmc_irq_ratio, init_dmc_irq_usleep, init_thread_recheck_ns);

	return 1;
}
__setup("dmc_irq_thread=", early_dmc_irq_thread);

static int dmc_filter_remove(char *p)
{
	int i = 0, del_num = 0;
	unsigned int size;

	for (i = 0; i < dmc_mon->filter.num; i++) {
		if (!strcmp(p + 1, dmc_mon->filter.dev[i].name)) {
			size = (DMC_FILTER_MAX - i - 1) * sizeof(struct dmc_filter_dev);
			if (dmc_mon->filter.dev[i].is_default) {
				dmc_mon->filter.dev[i].status = 0;
			} else {
				memmove(&dmc_mon->filter.dev[i],
					&dmc_mon->filter.dev[i + 1],
					size);
				del_num++;
			}
		}
	}

	return del_num;
}

static int set_dmc_filter(unsigned char *buf, unsigned char is_defalut)
{
	int i = 0, del_num, exit;
	static const char c[] = ",";
	char *buffer, *p;

	buffer = kstrdup(buf, GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;

	do {
		p = strsep(&buffer, c);
		if (p) {
			if (p[0] == '!') {
				del_num = dmc_filter_remove(p);
				dmc_mon->filter.num -= del_num;
				continue;
			}

			exit = 0;
			for (i = 0; i < dmc_mon->filter.num; i++) {
				if (!strcmp(p, dmc_mon->filter.dev[i].name)) {
					exit = 1;
					dmc_mon->filter.dev[i].status = 1;
				}
			}

			if (exit)
				continue;

			if (dmc_mon->filter.num >= DMC_FILTER_MAX) {
				pr_warn("dmc_filter %s set over max:%d!\n", p, DMC_FILTER_MAX);
					continue;
			}

			sprintf(dmc_mon->filter.dev[i].name, "%s", p);
			dmc_mon->filter.dev[i].status = 1;
			dmc_mon->filter.dev[i].is_default = is_defalut;
			dmc_mon->filter.num++;
		}
	} while (p);

	kfree(buffer);

	return 0;
}

#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
#ifdef CONFIG_AMLOGIC_PAGE_TRACE_INLINE
struct page_trace *dmc_find_page_base(struct page *page)
{
	struct page_trace *trace;

	trace = (struct page_trace *)&page->trace;
	return trace;
}
#else
struct page_trace *dmc_trace_buffer;
static unsigned int dmc_trace_step;
void get_page_trace_buf_hook(void *data, unsigned int migratetype,
		unsigned long *bypass)
{
	struct pagetrace_vendor_param *param;

	if (migratetype != 1024 || dmc_trace_buffer)
		return;

	param = (struct pagetrace_vendor_param *)bypass;
	dmc_trace_buffer = param->trace_buf;
	dmc_trace_step = param->trace_step;
	pr_info("dmc_trace_buf: %px\n", dmc_trace_buffer);
}

#ifdef CONFIG_ARM64
#define PAGE_TRACE_OFFSET	(_PAGE_END(CONFIG_ARM64_VA_BITS))
#else
#define PAGE_TRACE_OFFSET	PAGE_OFFSET
#endif

static struct pglist_data *next_online_pgdat_dmc(struct pglist_data *pgdat)
{
	int nid = next_online_node(pgdat->node_id);

	if (nid == MAX_NUMNODES)
		return NULL;
	/*
	 * This code is copy from upstream, please ignore the problem.
	 */
	/* coverity[dead_error_line:SUPPRESS] */
	return NODE_DATA(nid);
}

/*
 * next_zone - helper magic for for_each_zone()
 */
static struct zone *next_zone_dmc(struct zone *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES - 1) {
		zone++;
	} else {
		pgdat = next_online_pgdat_dmc(pgdat);
		if (pgdat)
			zone = pgdat->node_zones;
		else
			zone = NULL;
	}
	return zone;
}

struct page_trace *dmc_find_page_base(struct page *page)
{
	unsigned long pfn, zone_offset = 0, offset;
	struct zone *zone;
	struct page_trace *p;

	if (!dmc_trace_buffer)
		return NULL;

	pfn = page_to_pfn(page);
	for (zone = (NODE_DATA(first_online_node))->node_zones; zone;
			zone = next_zone_dmc(zone)) {
		if (!populated_zone(zone)) {
			; /* do nothing */
		} else {
			/* pfn is in this zone */
			if (pfn <= zone_end_pfn(zone) &&
				pfn >= zone->zone_start_pfn) {
				offset = pfn - zone->zone_start_pfn;
				p = dmc_trace_buffer;
				return p + ((offset + zone_offset) * dmc_trace_step);
			}
			/* next zone */
			zone_offset += zone->spanned_pages;
		}
	}
	return NULL;
}
#endif

static unsigned long dmc_unpack_ip(struct page_trace *trace)
{
	unsigned long text = PAGE_TRACE_OFFSET;

	if (trace->order == IP_INVALID)
		return 0;

#ifdef CONFIG_ARM
	if (trace->module_flag)
		text = MODULES_VADDR;

	return text + ((trace->ret_ip) << 3);
#else
	return text + ((trace->ret_ip) << 2);
#endif
}

unsigned long dmc_get_page_trace(struct page *page)
{
	struct page_trace *trace;

	trace = dmc_find_page_base(page);
	if (trace)
		return dmc_unpack_ip(trace);

	return 0;
}
#elif IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
struct page_trace *dmc_find_page_base(struct page *page)
{
	return find_page_base(page);
}

unsigned long dmc_get_page_trace(struct page *page)
{
	return get_page_trace(page);
}

static unsigned long dmc_unpack_ip(struct page_trace *trace)
{
	return unpack_ip(trace);
}
#endif

/* Return a pointer to the bitmap storing bits affecting a block of pages */
static inline unsigned long *get_pageblock_bitmap(const struct page *page,
							unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	return section_to_usemap(__pfn_to_section(pfn));
#else
	return page_zone(page)->pageblock_flags;
#endif /* CONFIG_SPARSEMEM */
}

static inline int pfn_to_bitidx(const struct page *page, unsigned long pfn)
{
#ifdef CONFIG_SPARSEMEM
	pfn &= (PAGES_PER_SECTION - 1);
#else
	pfn = pfn - pageblock_start_pfn(page_zone(page)->zone_start_pfn);
#endif /* CONFIG_SPARSEMEM */
	return (pfn >> pageblock_order) * NR_PAGEBLOCK_BITS;
}

static unsigned long aml_get_pfnblock_flags_mask(const struct page *page,
					unsigned long pfn, unsigned long mask)
{
	unsigned long *bitmap;
	unsigned long bitidx, word_bitidx;
	unsigned long word;

	bitmap = get_pageblock_bitmap(page, pfn);
	bitidx = pfn_to_bitidx(page, pfn);
	word_bitidx = bitidx / BITS_PER_LONG;
	bitidx &= (BITS_PER_LONG - 1);
	/*
	 * This races, without locks, with set_pfnblock_flags_mask(). Ensure
	 * a consistent read of the memory array, so that results, even though
	 * racy, are not corrupted.
	 */
	word = READ_ONCE(bitmap[word_bitidx]);
	return (word >> bitidx) & mask;
}

void dmc_vio_check_page(void *data)
{
	struct page *page;
	struct page_trace *trace;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	page = phys_to_page(mon_comm->addr);

#if defined(CONFIG_ARM64) || IS_ENABLED(CONFIG_AMLOGIC_DMC_MONITOR_BREAK_GKI)
	if (!pfn_is_map_memory(__phys_to_pfn(mon_comm->addr))) {
#else
	if (!pfn_valid(__phys_to_pfn(mon_comm->addr))) {
#endif
		mon_comm->trace.ip_data = IP_INVALID;
		mon_comm->page_flags = 0;
		mon_comm->mapping = 0xffff;
		mon_comm->migratetype = 0xffff;
	} else {
		trace = dmc_find_page_base(page);
		if (trace)
			mon_comm->trace = *trace;
		else
			mon_comm->trace.ip_data = IP_INVALID;
		mon_comm->page_flags = page->flags & PAGEFLAGS_MASK;
		mon_comm->mapping = (unsigned long)page->mapping;
		mon_comm->migratetype = aml_get_pfnblock_flags_mask(page,
					page_to_pfn(page), MIGRATETYPE_MASK);
	}
}

unsigned long __no_sanitize_address read_violation_mem(unsigned long addr, char rw)
{
	struct page *page;
	unsigned long *p, *q;
	unsigned long val;

	if (!(dmc_mon->debug & DMC_DEBUG_VALUE))
		return 0xdeaddead;

#if defined(CONFIG_ARM64) || IS_ENABLED(CONFIG_AMLOGIC_DMC_MONITOR_BREAK_GKI)
	if (!pfn_is_map_memory(__phys_to_pfn(addr)))
#else
	if (!pfn_valid(__phys_to_pfn(addr)))
#endif
		return 0xdeaddead;

	if (rw == 'r')
		return 0x0;

	page = phys_to_page(addr);
	p = kmap_atomic(page);

	if (!p)
		return 0xdeaddead;

	q = p + ((addr & (PAGE_SIZE - 1)) / sizeof(*p));
	val = *q;

	kunmap_atomic(p);

	return val;
}

void show_violation_mem_printk(char *title, void *data)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	static DEFINE_RATELIMIT_STATE(dmc_rs, HZ, DMC_RATELIMIT_BURST);

	dmc_pr_crit(DMC_TAG " addr=%09lx val=%016lx s=%08lx port=%s sub=%s f:%08lx lru:%d a:%ps(%d), t:%lld rw:%c%s(%d/%d/%d)\n",
		mon_comm->addr, read_violation_mem(mon_comm->addr, mon_comm->rw),
		mon_comm->status,
		mon_comm->port.name ? mon_comm->port.name : mon_comm->port.id,
		mon_comm->sub.name ? mon_comm->sub.name : mon_comm->sub.id,
		mon_comm->page_flags,
		test_bit(PG_lru, &mon_comm->page_flags),
		(void *)dmc_unpack_ip(&mon_comm->trace),
		(&mon_comm->trace)->order,
		mon_comm->time, mon_comm->rw, title,
		mon_comm->prot_buf.invalid,
		mon_comm->prot_buf.same,
		mon_comm->prot_buf.total, &dmc_rs);
}

void show_violation_mem_trace_event(char *title, void *data)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	char *port = mon_comm->port.name ? mon_comm->port.name : mon_comm->port.id;
	char *sub = mon_comm->sub.name ? mon_comm->sub.name : mon_comm->sub.id;

	trace_dmc_violation(title, mon_comm, port, sub,
			    dmc_unpack_ip(&mon_comm->trace),
			    read_violation_mem(mon_comm->addr, mon_comm->rw));
}

unsigned long dmc_prot_rw(void  __iomem *base,
			  long off, unsigned long value, int rw)
{
	if (base) {
		if (rw == DMC_WRITE) {
			writel(value, base + off);
			return 0;
		} else {
			return readl(base + off);
		}
	} else {
		return dmc_rw(off + dmc_mon->sec_base, value, rw);
	}
}

static inline int dmc_dev_is_byte(struct dmc_monitor *mon)
{
	return mon->configs & DMC_DEVICE_8BIT;
}

static inline int dmc_use_remapid(struct dmc_monitor *mon)
{
	return mon->configs & DMC_USE_DEV_REMAPID;
}

static int dev_name_to_id(const char *dev_name)
{
	int i, len;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].sub_id != -1)
			continue;
		len = strlen(dmc_mon->port[i].port_name);
		if (!strncmp(dmc_mon->port[i].port_name, dev_name, len))
			return dmc_mon->port[i].port_id;
	}
	return -1;
}

char *to_ports(int id)
{
	char *name;
	int i;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].port_id == id && dmc_mon->port[i].sub_id == -1) {
			name =  dmc_mon->port[i].port_name;
			return name;
		}
	}
	return NULL;
}

char *to_sub_ports_name(int mid, int sid, char rw)
{
	char *name = NULL;
	int i;
	char *port_name = to_ports(mid);

	if (!port_name)
		return NULL;

	if (strstr(port_name, "VPU")) {
		name = vpu_to_sub_port(mid, sid, rw);
	} else {
		sid &= 0x1f;
		for (i = 0; i < dmc_mon->port_num; i++) {
			if (dmc_mon->port[i].port_id == mid && dmc_mon->port[i].sub_id == sid) {
				name = dmc_mon->port[i].port_name;
				break;
			}
		}
	}
	return name;
}

void set_port_to_mon_comm(void *data, int port, int subport)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	mon_comm->port.name = to_ports(port);
	if (!mon_comm->port.name) {
		mon_comm->port.name = NULL;
		sprintf(mon_comm->port.id, "%d", port);
	}

	mon_comm->sub.name = to_sub_ports_name(port, subport, mon_comm->rw);
	if (!mon_comm->sub.name) {
		mon_comm->sub.name = NULL;
		sprintf(mon_comm->sub.id, "%d", subport);
	}
}

u64 get_all_dev_mask(void)
{
	u64 ret = 0;
	int i;

	if (dmc_dev_is_byte(dmc_mon))	/* not supported */
		return 0;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].sub_id != -1)
			continue;
		ret |= (1ULL << dmc_mon->port[i].port_id);
	}
	return ret;
}

static u64 get_reserved_mode_mask(void)
{
	u64 ret = 0;
	int i;

	if (dmc_dev_is_byte(dmc_mon))	/* not supported */
		return 0;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].sub_id != -1)
			continue;
		if (strstr(dmc_mon->port[i].port_name, "ARM"))
			continue;
		ret |= (1ULL << dmc_mon->port[i].port_id);
	}
	return ret;
}

static u64 get_other_dev_mask(void)
{
	u64 ret = 0;
	int i;

	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].sub_id != -1)
			continue;

		/*
		 * we don't want id with arm mali and device
		 * because these devices can access all ddr range
		 * and generate value-less report
		 */
		if (strstr(dmc_mon->port[i].port_name, "ARM")  ||
		    strstr(dmc_mon->port[i].port_name, "MALI") ||
		    strstr(dmc_mon->port[i].port_name, "DEVICE") ||
		    strstr(dmc_mon->port[i].port_name, "USB") ||
		    strstr(dmc_mon->port[i].port_name, "ETH") ||
		    strstr(dmc_mon->port[i].port_name, "EMMC") ||
		    strstr(dmc_mon->port[i].port_name, "TEST") ||
		    strstr(dmc_mon->port[i].port_name, "AMFC") ||
		    strstr(dmc_mon->port[i].port_name, "MTE") ||
		    strstr(dmc_mon->port[i].port_name, "NNA"))
			continue;

		ret |= (1ULL << dmc_mon->port[i].port_id);
	}
	return ret;
}

static void output_violation(struct dmc_monitor *mon, void *data, char *title)
{
	unsigned long vio_bit = 0;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;

	/* get port subport and vio_bit */
	if (dmc_mon->ops && dmc_mon->ops->vio_to_port)
		dmc_mon->ops->vio_to_port(data, &vio_bit);

	if (!(mon_comm->status & vio_bit))
		goto irq_finish;

	if (mon_comm->addr > mon->addr_end)
		goto irq_finish;

	/* ignore violation */
	if (dmc_violation_ignore(title, data, vio_bit))
		goto irq_finish;

	/* violation print */
#if IS_ENABLED(CONFIG_EVENT_TRACING)
	if (mon->debug & DMC_DEBUG_TRACE) {
		show_violation_mem_trace_event(title, data);
		goto irq_finish;
	}
#endif

	show_violation_mem_printk(title, data);

	/* sleep if the irq too much */
irq_finish:
	if (mon->debug & DMC_DEBUG_IRQ_THREAD)
		dmc_irq_sleep(data);
}

void dmc_recheck_invalid(struct dmc_mon_comm *mon_comm,
			 unsigned long addr, int port, int sub,
			 unsigned int *same, unsigned int *invalid)
{
	if (mon_comm->port.number == port &&
	    mon_comm->sub.number == sub &&
	    (PHYS_PFN(addr) <= PHYS_PFN(mon_comm->addr) + 2) &&
	    (PHYS_PFN(addr) >= PHYS_PFN(mon_comm->addr) - 1)) {
		(*same)++;
		if (!dmc_get_page_trace(phys_to_page(addr)))
			(*invalid)++;
	}
}

void dmc_common_recheck(struct dmc_monitor *mon, void *data)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	struct dmc_mon_comm tmp = *mon_comm;
	unsigned long long t;
	unsigned int count = 0, same = 0, invalid = 0;

	if (dmc_mon->ops && dmc_mon->ops->handle_irq) {
		dmc_mon->ops->handle_irq(mon, data, IRQ_CLEAR);
		t = sched_clock();
		do {
			if (!dmc_mon->ops->handle_irq(mon, &tmp, IRQ_CHECK)) {
				dmc_mon->ops->handle_irq(mon, data, IRQ_CLEAR);
				count++;
				dmc_recheck_invalid(mon_comm, tmp.addr,
						    tmp.port.number, tmp.sub.number,
						    &same, &invalid);
			}
		} while (sched_clock() - t < init_thread_recheck_ns);
	}
	mon_comm->prot_buf.total = count;
	mon_comm->prot_buf.same = same;
	mon_comm->prot_buf.invalid = invalid;
}

void dmc_output_violation(struct dmc_monitor *mon, void *data)
{
	char title[10];

	/* if not new violation, print old info, otherwise new info*/
	output_violation(mon, data, title);

	if (dmc_mon->ops && dmc_mon->ops->handle_irq)
		/* clear violation and flush */
		dmc_mon->ops->handle_irq(mon, data, IRQ_CLEAR);
}

int dmc_violation_ignore(char *title, void *data, unsigned long vio_bit)
{
	int i, is_ignore = 0;
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	unsigned long status = mon_comm->status | vio_bit;
	struct page_trace *trace = &mon_comm->trace;
#ifdef CONFIG_KALLSYMS
	char sym[KSYM_SYMBOL_LEN];
#endif

	/* if trace invalid , should not be ignore*/
	if (trace && trace->order == IP_INVALID) {
		/* if page status is free and recheck not found again, ignore*/
		if ((dmc_mon->debug & DMC_DEBUG_FREE_IGNORE) && !mon_comm->prot_buf.invalid)
			is_ignore = 1;
		goto dmc_ignore;
	}

	/* ignore cma driver pages */
	if ((is_migrate_cma(mon_comm->migratetype) ||
			mon_comm->migratetype == MIGRATE_ISOLATE) && !mon_comm->mapping) {
		if (dmc_mon->debug & DMC_DEBUG_CMA) {
			sprintf(title, "%s", "_cma");
		} else {
			is_ignore = 1;
			goto dmc_ignore;
		}
	}

	/* ignore violation on same page/same port */
	if ((mon_comm->addr & PAGE_MASK) == mon_comm->last_addr &&
	     status == mon_comm->last_status &&
	     trace->ip_data == mon_comm->last_trace) {
		if (dmc_mon->debug & DMC_DEBUG_SAME) {
			sprintf(title, "%s", "_same");
		} else {
			is_ignore = 1;
			goto dmc_ignore;
		}
	}

#ifdef CONFIG_KALLSYMS
	sprint_symbol_no_offset(sym, dmc_unpack_ip(&mon_comm->trace));
#endif

	/* ignore black dev or symbol filter */
	for (i = 0; i < dmc_mon->filter.num; i++) {
		if (!dmc_mon->filter.dev[i].status)
			continue;
		if (mon_comm->port.name) {
			if (strstr(mon_comm->port.name, dmc_mon->filter.dev[i].name)) {
				is_ignore = 1;
				goto dmc_ignore;
			}
		}
		if (mon_comm->sub.name) {
			if (strstr(mon_comm->sub.name, dmc_mon->filter.dev[i].name)) {
				is_ignore = 1;
				goto dmc_ignore;
			}
		}
	#ifdef CONFIG_KALLSYMS
		if (strstr(sym, dmc_mon->filter.dev[i].name) == sym) {
			is_ignore = 1;
			goto dmc_ignore;
		}
	#endif
	}

dmc_ignore:
	mon_comm->last_trace = trace ? trace->ip_data : 0xdeaddead;
	mon_comm->last_addr = mon_comm->addr & PAGE_MASK;
	mon_comm->last_status = status;

	return is_ignore;
}

static void dmc_set_default(struct dmc_monitor *mon)
{
	if (dmc_dev_is_byte(mon)) {
		switch (mon->chip) {
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T7
		case DMC_TYPE_T7:
			set_dmc_filter("MALI", 1);
			mon->device = 0x01020305;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
		case DMC_TYPE_T3:
			/* NNA ration is high if be used*/
			mon->device = 0x02041c40;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
		case DMC_TYPE_P1:
			mon->device = 0x02324c52;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C3
		case DMC_TYPE_C3:
			mon->device = 0x02495051;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T5M
		case DMC_TYPE_T5M:
			mon->device = 0x0204253a;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
	#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S5
		case DMC_TYPE_S5:
			mon->device = 0x02040809;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
		case DMC_TYPE_T3X:
			mon->device = 0x02040809;
			mon->debug &= ~DMC_DEBUG_INCLUDE;
			break;
	#endif
		}
	} else {
		mon->device = get_other_dev_mask();
	}

	mon->addr_start = 0x0;
	mon->addr_end = get_num_physpages() << PAGE_SHIFT;
	mon->debug |= DMC_DEBUG_FREE_IGNORE;

	dmc_set_monitor(mon->addr_start, mon->addr_end, mon->device, DMC_MODE_DEFAULT, 1);
}

static void dmc_clear_reserved_memory(struct dmc_monitor *mon)
{
	if (init_dmc_mode == DMC_MODE_RESERVED) {
		mon->debug = dmc_original_debug;
		mon->device = 0;
		mon->addr_start = 0;
		mon->addr_end = 0;
		init_dmc_mode = DMC_MODE_NORMAL;
	}
}

static void dmc_enabled_reserved_memory(struct platform_device *pdev, struct dmc_monitor *mon)
{
	struct device_node *node;
	struct reserved_mem *rmem;

	dmc_original_debug = mon->debug;

	node = of_parse_phandle(pdev->dev.of_node, "memory-region", 0);
	if (!node)
		return;

	rmem = of_reserved_mem_lookup(node);
	if (!rmem) {
		pr_info("no such reserved mem of dmc_monitor\n");
		return;
	}

	if (!rmem->size)
		return;

	mon->addr_start = rmem->base;
	mon->addr_end = rmem->base + rmem->size - 1;

	if (dmc_dev_is_byte(mon)) {
		mon->device = 0x0102;
		mon->debug &= ~DMC_DEBUG_INCLUDE;
	} else {
		mon->device = get_reserved_mode_mask();
	}

	mon->debug &= ~DMC_DEBUG_TRACE;
	mon->debug |= DMC_DEBUG_CMA;
	mon->debug |= DMC_DEBUG_SAME;

	dmc_set_monitor(mon->addr_start, mon->addr_end, mon->device, DMC_MODE_RESERVED, 1);
}

size_t dump_dmc_reg(char *buf)
{
	size_t sz = 0, i;
	unsigned char dev;
	u64 devices;

	if (dmc_mon->ops && dmc_mon->ops->dump_reg)
		sz += dmc_mon->ops->dump_reg(buf);
	sz += sprintf(buf + sz, "SEC_BASE:0x%lx\n", dmc_mon->sec_base);
	sz += sprintf(buf + sz, "DMC_DEBUG:0x%x\n", dmc_mon->debug);
	sz += sprintf(buf + sz, "RANGE:0x%lx - 0x%lx\n",
		      dmc_mon->addr_start, dmc_mon->addr_end);
	sz += sprintf(buf + sz, "MONITOR DEVICE(%s):\n",
		dmc_mon->debug & DMC_DEBUG_INCLUDE ? "include" : "exclude");
	if (!dmc_mon->device)
		return sz;

	if (dmc_dev_is_byte(dmc_mon)) {
		devices = dmc_mon->device;
		for (i = 0; i < sizeof(dmc_mon->device); i++) {
			dev = devices & 0xff;
			devices >>= 8ULL;
			if (dev)
				sz += sprintf(buf + sz, "    %s\n", to_ports(dev));
		}
	} else {
		for (i = 0; i < sizeof(dmc_mon->device) * 8; i++) {
			if (dmc_mon->device & (1ULL << i))
				sz += sprintf(buf + sz, "    %s\n", to_ports(i));
		}
	}

	return sz;
}

int dmc_sec_save_info(char *output, unsigned char index, struct dmc_mon_comm *data)
{
	int count = 0;

	set_port_to_mon_comm(data, data->port.number, data->sub.number);

	count += sprintf(output + count,
			"DMC%d %s %s CHECK ERROR: addr:0x%lx, port:%s, subport:%s\n",
			index,
			data->addr > (get_num_physpages() << PAGE_SHIFT) ? "ADDR" : "SEC",
			data->rw == 'r' ? "READ" : "WRITE",
			data->addr,
			data->port.name ? data->port.name : data->port.id,
			data->sub.name ? data->sub.name : data->sub.id);

	return count;
}

int dmc_sec_save_reg(char *output, unsigned char index, unsigned long reg_status,
		     unsigned long *reg, int number)
{
	int count = 0, print = 0, i;

	if (!(reg_status & 0x3))
		count += sprintf(output + count, "DMC%d SEC CHECK PASS.\n", index);

	for (i = 0; i < number; i++) {
		if (reg[i] != 0) {
			print = 1;
			break;
		}
	}

	if (reg_status || print) {
		count += sprintf(output + count,
					"DMC%d_SEC_STATUS:%lx\n", index, reg_status);
		for (i = 0; i < number; i++)
			count += sprintf(output + count,
						"DMC%d_VIO_ADDR%d:%lx\n", index, i, reg[i]);
	}

	if (dmc_mon->debug & DMC_DEBUG_SEC_DUMP)
		dump_stack();

	return count;
}

static void dmc_sec_reg_check(char *info, unsigned char index)
{
	int len = 0, i;
	bool flag = 0;
	static char buf[2048] = {0};
	static DEFINE_RATELIMIT_STATE(dmc_sec_rs, HZ, DMC_SEC_RATELIMIT_BURST);

	if (__ratelimit(&dmc_sec_rs))
		flag = 1;

	if (info && flag)
		len += snprintf(buf, sizeof(buf), info);

	if (dmc_mon->ops && dmc_mon->ops->reg_control) {
		if (index != 0xff) {
			if (flag)
				len += dmc_mon->ops->reg_control(&index, 'd', buf + len);
			dmc_mon->ops->reg_control(&index, 'c', NULL);
		} else {
			for (i = 0; i < dmc_mon->mon_number; i++) {
				index = i;
				if (flag)
					len += dmc_mon->ops->reg_control(&index, 'd', buf + len);
				dmc_mon->ops->reg_control(&index, 'c', NULL);
			}
		}
	}

	if (flag)
		pr_emerg("%s", buf);
}

#ifdef ARM
static int dmc_sec_dump_arm(void)
{
	dmc_sec_reg_check("FROM ARM32 -- ", 0xff);
	return 0;
}
#endif

static irqreturn_t __nocfi dmc_sec_irq_handler(int irq, void *dev_instance)
{
	unsigned char index = (struct dmc_mon_comm *)dev_instance - dmc_mon->mon_comm;

	dmc_sec_reg_check("FROM IRQ -- ", index);

	return IRQ_HANDLED;
}

static irqreturn_t __nocfi dmc_prot_irq_handler(int irq, void *dev_instance)
{
	if (dmc_mon->ops && dmc_mon->ops->handle_irq)
		dmc_mon->ops->handle_irq(dmc_mon, dev_instance, IRQ_CHECK);

	if (dmc_mon->ops && dmc_mon->ops->recheck_violation)
		dmc_mon->ops->recheck_violation(dev_instance);
	else
		dmc_common_recheck(dmc_mon, dev_instance);

	if (dmc_mon->debug & DMC_DEBUG_IRQ_THREAD)
		return IRQ_WAKE_THREAD;

	dmc_output_violation(dmc_mon, dev_instance);

	return IRQ_HANDLED;
}

static irqreturn_t dmc_irq_thread(int irq, void *dev_instance)
{
	dmc_output_violation(dmc_mon, dev_instance);

	return IRQ_HANDLED;
}

void dmc_irq_sleep(void *data)
{
	struct dmc_mon_comm *mon_comm = (struct dmc_mon_comm *)data;
	unsigned long long sys_clock, task_time;
	u64 t_ns = 0;

	/* get system and thread runtime from before irq to current */
	sys_clock = sched_clock();

	/* old soc may creat a secure irq when irq set but irq thread not be init */
	if (!mon_comm->irq_thread_task)
		return;

#if CONFIG_AMLOGIC_KERNEL_VERSION >= 14515
	/* this can be delete when 14-5.15 task_sched_runtime symbol commit */
	task_time = mon_comm->irq_thread_task->se.sum_exec_runtime;
#else
	task_time = task_sched_runtime(mon_comm->irq_thread_task);
#endif
	if (sys_clock - mon_comm->sys_run_time >= init_dmc_irq_check_ns * 2) {
	/* If the time between two interruptions is more than 10ms, refresh and recalculate */
		mon_comm->sys_run_time = sys_clock;
		mon_comm->irq_run_time = task_time;
	} else if (sys_clock - mon_comm->sys_run_time >= init_dmc_irq_check_ns) {
		t_ns = task_time - mon_comm->irq_run_time;
		/* if system run than 5ms, and thread runtime than ratio, sleep */
		if (t_ns >= (init_dmc_irq_check_ns >> 7) * init_dmc_irq_ratio)
			usleep_range(init_dmc_irq_usleep, init_dmc_irq_usleep + 100);

		mon_comm->sys_run_time = sys_clock;
		mon_comm->irq_run_time = task_time;
	}
}

static int dmc_irq_set_affinity(void)
{
	int i = 0, affinity_cpu = 1, irq_prot, irq_sec;
	struct irq_desc *desc;

	if (!dmc_mon)
		return -EINVAL;

	for (i = 0; i < dmc_mon->mon_number; i++) {
		irq_prot = dmc_mon->mon_comm[i].irq;
		irq_sec = dmc_mon->mon_comm[i].irq_sec;

		if (affinity_cpu > num_online_cpus())
			affinity_cpu = 1;

		if (irq_prot)
			irq_set_affinity_hint(irq_prot, get_cpu_mask(affinity_cpu));
		if (irq_sec)
			irq_set_affinity_hint(irq_sec, get_cpu_mask(affinity_cpu));

		affinity_cpu++;

		desc = irq_to_desc(irq_prot);

		if (!desc || !desc->action)
			dmc_mon->mon_comm[i].irq_thread_task = NULL;
		else
			dmc_mon->mon_comm[i].irq_thread_task = desc->action->thread;
	}

	return 0;
}

static int dmc_irq_request(struct device_node *node)
{
	int irq, r = 0, i = 0, index;

	do {
		irq = of_irq_get(node, i);
		if (irq > 0) {
			if (i < dmc_mon->mon_number) {	/* prot vio irq*/
				index = i;
				dmc_mon->mon_comm[index].irq = irq;
				r = request_threaded_irq(irq, dmc_prot_irq_handler,
							dmc_irq_thread,
							IRQF_SHARED | IRQF_ONESHOT,
							"dmc_monitor",
							&dmc_mon->mon_comm[index]);
			} else {			/* sec vio irq */
				index = i - dmc_mon->mon_number;
				dmc_mon->mon_comm[index].irq_sec = irq;
				r = request_irq(irq, dmc_sec_irq_handler,
							IRQF_SHARED,
							"dmc_monitor_sec",
							&dmc_mon->mon_comm[index]);
			}
			if (r < 0) {
				pr_err("request irq fail:%d, r:%d\n", irq, r);
				dmc_mon = NULL;
				return -EINVAL;
			}
		}
		i++;
	} while (irq > 0);

	dmc_irq_set_affinity();

	return 0;
}

static int dmc_regulation_dev(unsigned long dev, int add)
{
	unsigned char *p, cur;
	int i, set;

	if (dmc_dev_is_byte(dmc_mon)) {
		/* dev is a set of 8 bit user id index */
		while (dev) {
			cur = dev & 0xff;
			set = 0;
			p   = (unsigned char *)&dmc_mon->device;
			for (i = 0; i < sizeof(dmc_mon->device); i++) {
				if (p[i] == cur) {	/* already set */
					if (add)
						set = 1;
					else		/* clear it */
						p[i] = 0;
					break;
				}

				if (p[i] == 0 && add) {	/* find empty one */
					p[i] = (dev & 0xff);
					set = 1;
					break;
				}
			}
			if (i == sizeof(dmc_mon->device) && !set && add) {
				pr_err("monitor device full\n");
				return -EINVAL;
			}
			dev >>= 8;
		}
	} else {
		if (add) /* dev is bit mask */
			dmc_mon->device |= dev;
		else
			dmc_mon->device &= ~(dev);
	}
	return 0;
}

int dmc_set_monitor(unsigned long start, unsigned long end,
		    unsigned long dev_mask, enum dmc_mode_type mode, int en)
{
	char buf[256] = "";
	unsigned int i, count = 0;

	if (!dmc_mon)
		return -EINVAL;

	set_dmc_filter(default_filter_dev, 1);
	set_dmc_filter(default_filter_sym, 1);
	set_dmc_filter(exception_filter_sym, 1);
	if (strlen(dmc_filter_early_buf))
		set_dmc_filter(dmc_filter_early_buf, 0);

	dmc_mon->addr_start = start;
	dmc_mon->addr_end   = end;
	dmc_regulation_dev(dev_mask, en);
	init_dmc_mode = mode;

	pr_info("dmc set: mode:%d, device=%llx, range:%lx-%lx, debug=%x\n",
			mode, dmc_mon->device, dmc_mon->addr_start,
			dmc_mon->addr_end, dmc_mon->debug);

	for (i = 0; i < dmc_mon->filter.num; i++)
		if (dmc_mon->filter.dev[i].status)
			count += sprintf(buf + count, " %s", dmc_mon->filter.dev[i].name);
	pr_info("dmc filter:%s\n", buf);

	if (start < end && dmc_mon->ops && dmc_mon->ops->set_monitor)
		return dmc_mon->ops->set_monitor(dmc_mon);
	return -EINVAL;
}
EXPORT_SYMBOL(dmc_set_monitor);

int dmc_set_monitor_by_name(unsigned long start, unsigned long end,
			    const char *port_name, int en)
{
	long id;

	dmc_clear_reserved_memory(dmc_mon);
	id = dev_name_to_id(port_name);
	if (id >= 0 && dmc_dev_is_byte(dmc_mon))
		return dmc_set_monitor(start, end, id, DMC_MODE_NORMAL, en);
	else if (id < 0 || id >= BITS_PER_LONG)
		return -EINVAL;
	return dmc_set_monitor(start, end, 1UL << id, DMC_MODE_NORMAL, en);
}
EXPORT_SYMBOL(dmc_set_monitor_by_name);

void dmc_monitor_disable(void)
{
	memset(&dmc_mon->filter, 0, sizeof(dmc_mon->filter));

	if (dmc_mon->ops && dmc_mon->ops->disable)
		return dmc_mon->ops->disable(dmc_mon);
}
EXPORT_SYMBOL(dmc_monitor_disable);

static ssize_t range_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return 0;
	}

	return sprintf(buf, "%08lx - %08lx\n",
		       dmc_mon->addr_start, dmc_mon->addr_end);
}

static ssize_t range_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	unsigned long start, end;

	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return count;
	}

	dmc_clear_reserved_memory(dmc_mon);

	ret = sscanf(buf, "%lx %lx", &start, &end);
	if (ret != 2) {
		return count;
	}
	dmc_set_monitor(start, end, dmc_mon->device, DMC_MODE_NORMAL, 1);
	return count;
}
static CLASS_ATTR_RW(range);

static ssize_t device_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int i, is_add;

	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return count;
	}

	dmc_clear_reserved_memory(dmc_mon);

	if (!strncmp(buf, "none", 4)) {
		dmc_monitor_disable();
		return count;
	}

	if (!strncmp(buf, "default", 7)) {
		dmc_set_default(dmc_mon);
		return count;
	}

	if (!strncmp(buf, "all", 3) && !dmc_dev_is_byte(dmc_mon)) {
		dmc_mon->device = get_all_dev_mask();
	} else {
		if (buf[0] == '!') {
			i = dev_name_to_id(buf + 1);
			is_add = 0;
		} else {
			i = dev_name_to_id(buf);
			is_add = 1;
		}

		if (i < 0) {
			pr_info("bad device:%s\n", buf);
			return -EINVAL;
		}

		if (!dmc_dev_is_byte(dmc_mon)) {
			if (i >= 32)
				return -EINVAL;
			i = 1UL << i;
		}

		if (dmc_regulation_dev(i, is_add))
			return -EINVAL;
	}

	dmc_set_monitor(dmc_mon->addr_start, dmc_mon->addr_end,
				dmc_mon->device, DMC_MODE_NORMAL, 1);
	return count;
}

static ssize_t device_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int i, s = 0;

	if (!dmc_mon->ops) {
		pr_err("Can't find ops for chip:%x\n", dmc_mon->chip);
		return 0;
	}

	s += sprintf(buf + s, "supported device:\n");
	for (i = 0; i < dmc_mon->port_num; i++) {
		if (dmc_mon->port[i].sub_id != -1)
			continue;
		s += sprintf(buf + s, "%2d : %s\n",
			dmc_mon->port[i].port_id,
			dmc_mon->port[i].port_name);
	}
	return s;
}
static CLASS_ATTR_RW(device);

static ssize_t dump_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return dump_dmc_reg(buf);
}
static CLASS_ATTR_RO(dump);

static ssize_t debug_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int val;
	char string[128];

	if (sscanf(buf, "%s %d", string, &val) != 2) {
		pr_info("invalid input:%s\n", buf);
		return count;
	}

	if (strstr(string, "write") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_WRITE;
		else
			dmc_mon->debug &= ~DMC_DEBUG_WRITE;
	} else if (strstr(string, "read") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_READ;
		else
			dmc_mon->debug &= ~DMC_DEBUG_READ;
	} else if (strstr(string, "cma") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_CMA;
		else
			dmc_mon->debug &= ~DMC_DEBUG_CMA;
	} else if (strstr(string, "same") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_SAME;
		else
			dmc_mon->debug &= ~DMC_DEBUG_SAME;
	} else if (strstr(string, "include") == string) {
		if (dmc_dev_is_byte(dmc_mon)) {
			if (val)
				dmc_mon->debug |= DMC_DEBUG_INCLUDE;
			else
				dmc_mon->debug &= ~DMC_DEBUG_INCLUDE;
		} else {
			pr_emerg("dmc not support include set\n");
		}
	} else if (strstr(string, "trace") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_TRACE;
		else
			dmc_mon->debug &= ~DMC_DEBUG_TRACE;
	} else if (strstr(string, "serror") == string) {
		dmc_sec_reg_check("FROM USER -- ", 0xff);
	} else if (strstr(string, "irq") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_IRQ_THREAD;
		else
			dmc_mon->debug &= ~DMC_DEBUG_IRQ_THREAD;
	} else if (strstr(string, "value") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_VALUE;
		else
			dmc_mon->debug &= ~DMC_DEBUG_VALUE;
	} else if (strstr(string, "free") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_FREE_IGNORE;
		else
			dmc_mon->debug &= ~DMC_DEBUG_FREE_IGNORE;
	} else if (strstr(string, "sec_dump") == string) {
		if (val)
			dmc_mon->debug |= DMC_DEBUG_SEC_DUMP;
		else
			dmc_mon->debug &= ~DMC_DEBUG_SEC_DUMP;
	} else {
		pr_info("invalid param name:%s\n", buf);
		return count;
	}

	dmc_set_monitor(dmc_mon->addr_start, dmc_mon->addr_end,
				dmc_mon->device, DMC_MODE_NORMAL, 1);

	return count;
}

static ssize_t debug_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int s = 0;

	s += sprintf(buf + s, "dmc version: %s\n", DMC_VERSION);
	s += sprintf(buf + s, "debug value: 0x%02x\n", dmc_mon->debug);
	s += sprintf(buf + s, "write:   write monitor:%d\n",
			dmc_mon->debug & DMC_DEBUG_WRITE ? 1 : 0);
	s += sprintf(buf + s, "read:    read monitor :%d\n",
			dmc_mon->debug & DMC_DEBUG_READ ? 1 : 0);
	s += sprintf(buf + s, "cma:     cma access show :%d\n",
			dmc_mon->debug & DMC_DEBUG_CMA ? 1 : 0);
	s += sprintf(buf + s, "same:    same access show :%d\n",
			dmc_mon->debug & DMC_DEBUG_SAME ? 1 : 0);
	s += sprintf(buf + s, "include: include mode :%d\n",
			dmc_mon->debug & DMC_DEBUG_INCLUDE ? 1 : 0);
	s += sprintf(buf + s, "trace:   trace print :%d\n",
			dmc_mon->debug & DMC_DEBUG_TRACE ? 1 : 0);
	s += sprintf(buf + s, "serror:  show sec vio reg and clear\n");
	s += sprintf(buf + s, "irq:     irq_thread :%d, time:%ld, ratio:%d, irq_usleep:%ld, recheck:%ld\n",
				dmc_mon->debug & DMC_DEBUG_IRQ_THREAD ? 1 : 0,
				init_dmc_irq_check_ns, init_dmc_irq_ratio,
				init_dmc_irq_usleep, init_thread_recheck_ns);
	s += sprintf(buf + s, "pr_limit: printk ratelimit :%d\n", DMC_RATELIMIT_BURST);
	s += sprintf(buf + s, "value:   read violation value :%d\n",
			dmc_mon->debug & DMC_DEBUG_VALUE ? 1 : 0);
	s += sprintf(buf + s, "free:     page free ignore :%d\n",
			dmc_mon->debug & DMC_DEBUG_FREE_IGNORE ? 1 : 0);
	s += sprintf(buf + s, "sec_dump: dump stack when sec vio check :%d\n",
			dmc_mon->debug & DMC_DEBUG_SEC_DUMP ? 1 : 0);

	return s;
}
static CLASS_ATTR_RW(debug);

#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
static ssize_t dev_access_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	unsigned long addr = 0, size = 0;
	int id = 0;

	ret = sscanf(buf, "%d %lx %lx", &id, &addr, &size);
	if (ret != 3) {
		pr_info("input param num should be 3 (id addr size)\n");
		return count;
	}
	dmc_dev_access(id, addr, size);

	return count;
}

static ssize_t dev_access_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return show_dmc_notifier_list(buf);
}
static CLASS_ATTR_RW(dev_access);
#endif

static ssize_t cmdline_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int count = 0;
	int i = 0, first = 0, flag = 0;

#define FILTER_NOTICE_INFO "\nIf you want to delete it, 'echo !<value> > /sys/class/dmc_monitor/filter' or uboot cmdline as 'dmc_filter=!USB,!EMMC'\n\n"
	count += sprintf(buf + count, "Current filter list:\n\t");
	if (dmc_mon->filter.num) {
		count += sprintf(buf + count, "%s", dmc_mon->filter.dev[0].name);
		for (i = 1; i < dmc_mon->filter.num; i++)
			if (dmc_mon->filter.dev[i].status)
				count += sprintf(buf + count, ",%s", dmc_mon->filter.dev[i].name);
	}
	count += sprintf(buf + count, FILTER_NOTICE_INFO);

	if (init_dmc_mode == DMC_MODE_DEFAULT) {
		count += sprintf(buf + count, "setenv initargs $initargs dmc_monitor=default");
	} else {
		count += sprintf(buf + count,
				"setenv initargs $initargs dmc_monitor=0x%lx,0x%lx,0x%llx,0x%x",
				dmc_mon->addr_start, dmc_mon->addr_end,
				dmc_mon->device, dmc_mon->debug);
	}

	for (i = 0; i < dmc_mon->filter.num; i++) {
		if (!dmc_mon->filter.dev[i].status)
			flag = 1;
		if (!dmc_mon->filter.dev[i].is_default)
			flag = 2;

		if (flag == 1) {
			if (!first) {
				count += sprintf(buf + count, "%s!%s",
						" dmc_filter=",
						dmc_mon->filter.dev[i].name);
				first = 1;
			} else {
				count += sprintf(buf + count, ",!%s", dmc_mon->filter.dev[i].name);
			}
			flag = 0;
		}

		if (flag == 2) {
			if (!first) {
				count += sprintf(buf + count, "%s%s",
						" dmc_filter=",
						dmc_mon->filter.dev[i].name);
				first = 1;
			} else {
				count += sprintf(buf + count, ",%s", dmc_mon->filter.dev[i].name);
			}
			flag = 0;
		}
	}

	count += sprintf(buf + count, ";saveenv;reset;\n");
	return count;
}
static CLASS_ATTR_RO(cmdline);

static ssize_t filter_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	char tmp[256];

	if (!strncmp(buf, "none", 4)) {
		memset(&dmc_mon->filter, 0, sizeof(dmc_mon->filter));
	} else {
		strncpy(tmp, buf, sizeof(tmp));
		if (tmp[strlen(tmp) - 1] == '\n')
			tmp[strlen(tmp) - 1] = '\0';
		set_dmc_filter(tmp, 0);
	}

	return count;
}

static ssize_t filter_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	unsigned int count = 0;
	int i;

	count += sprintf(buf + count, "filter list:\n");
	for (i = 0; i < dmc_mon->filter.num; i++)
		if (dmc_mon->filter.dev[i].status)
			count += sprintf(buf + count, "%s\n", dmc_mon->filter.dev[i].name);

	return count;
}
static CLASS_ATTR_RW(filter);

static ssize_t reg_analysis_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	char info[1024];
	int ret = 0;

	if (dmc_mon->ops && dmc_mon->ops->reg_control)
		ret = dmc_mon->ops->reg_control((char *)buf, 'a', info);
	if (ret)
		pr_emerg("%s", info);

	return count;
}
static CLASS_ATTR_WO(reg_analysis);

static struct attribute *dmc_monitor_attrs[] = {
	&class_attr_range.attr,
	&class_attr_device.attr,
	&class_attr_dump.attr,
	&class_attr_debug.attr,
	&class_attr_cmdline.attr,
	&class_attr_filter.attr,
	&class_attr_reg_analysis.attr,
#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
	&class_attr_dev_access.attr,
#endif
	NULL
};
ATTRIBUTE_GROUPS(dmc_monitor);

static struct class dmc_monitor_class = {
	.name = "dmc_monitor",
	.class_groups = dmc_monitor_groups,
};

static void __init get_dmc_ops(int chip, struct dmc_monitor *mon)
{
	/* set default parameters */
	mon->mon_number = 1;
	mon->debug |= DMC_DEBUG_INCLUDE;
	mon->debug |= DMC_DEBUG_WRITE;
	mon->debug &= ~DMC_DEBUG_TRACE;

	switch (chip) {
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_G12
	case DMC_TYPE_G12A:
	case DMC_TYPE_G12B:
	case DMC_TYPE_SM1:
	case DMC_TYPE_TL1:
		mon->ops = &g12_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C1
	case DMC_TYPE_C1:
		mon->ops = &c1_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C2
	case DMC_TYPE_C2:
		mon->ops = &c2_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_GX
	case DMC_TYPE_GXBB:
	case DMC_TYPE_GXTVBB:
	case DMC_TYPE_GXL:
	case DMC_TYPE_GXM:
	case DMC_TYPE_TXL:
	case DMC_TYPE_TXLX:
	case DMC_TYPE_AXG:
	case DMC_TYPE_GXLX:
	case DMC_TYPE_TXHD:
		mon->ops = &gx_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_TM2
	case DMC_TYPE_TM2:
	/* cpuinfo build module not used is_meson_rev_b() */
	#ifdef CONFIG_AMLOGIC_CPU_INFO
		if (is_meson_rev_b())
			mon->ops = &tm2_dmc_mon_ops;
		else
	#endif
		#ifdef CONFIG_AMLOGIC_DMC_MONITOR_G12
			mon->ops = &g12_dmc_mon_ops;
		#else
			#error need support for revA
		#endif
		break;

	case DMC_TYPE_SC2:
		mon->ops = &tm2_dmc_mon_ops;
		break;

	case DMC_TYPE_T5:
	case DMC_TYPE_T5D:
		mon->ops = &tm2_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T7
	case DMC_TYPE_T7:
	case DMC_TYPE_T3:
		mon->ops = &t7_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 2;
		break;
	case DMC_TYPE_P1:
		mon->ops = &t7_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 4;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S4
	case DMC_TYPE_S4:
	case DMC_TYPE_T5W:
	case DMC_TYPE_A5:
		mon->ops = &s4_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C3
	case DMC_TYPE_C3:
		mon->ops = &c3_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T5M
	case DMC_TYPE_T5M:
		mon->ops = &t5m_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 2;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S5
	case DMC_TYPE_S5:
		mon->ops = &s5_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 4;
		break;
	case DMC_TYPE_T3X:
		mon->ops = &s5_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 2;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S6
	case DMC_TYPE_S6:
		mon->ops = &s6_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_TXHD2
	case DMC_TYPE_TXHD2:
		mon->ops = &txhd2_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S1A
	case DMC_TYPE_S1A:
		mon->ops = &s1a_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_A4
	case DMC_TYPE_A4:
		mon->ops = &a4_dmc_mon_ops;
		mon->configs |= DMC_DEVICE_8BIT;
		mon->mon_number = 1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_S7
	case DMC_TYPE_S7:
	case DMC_TYPE_S7D:
		mon->ops = &s7_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T6D
	case DMC_TYPE_T6D:
		mon->ops = &t6d_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T6W
	case DMC_TYPE_T6W:
		mon->ops = &t6w_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_T6X
	case DMC_TYPE_T6X:
		mon->ops = &t6x_dmc_mon_ops;
		mon->mon_number = 2;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_C4
	case DMC_TYPE_C4:
		mon->ops = &c4_dmc_mon_ops;
		break;
#endif
#ifdef CONFIG_AMLOGIC_DMC_MONITOR_A9
	case DMC_TYPE_A9:
		mon->ops = &a9_dmc_mon_ops;
		break;
#endif
	default:
		pr_err("Can't find ops for chip:%x\n", chip);
		break;
	}
}

#if IS_ENABLED(CONFIG_ANDROID_VENDOR_HOOKS)
#if CONFIG_AMLOGIC_KERNEL_VERSION >= 14515
/* Asynchronous Serror*/
static void arm64_serror_panic(void *data, struct pt_regs *regs, unsigned long esr)
{
	int nmi = in_nmi();

	/*
	 * arm64 calls nmi_enter() before do_serror(),
	 * it will cause printk entered deferred mode
	 * and not outputs to console immediately.
	 */
	if (nmi)
		__preempt_count_sub(NMI_OFFSET + HARDIRQ_OFFSET);

	dmc_sec_reg_check("FROM ASYNC SERROR -- ", 0xff);

	if (nmi)
		__preempt_count_add(NMI_OFFSET + HARDIRQ_OFFSET);
}

/* Synchronous Serror*/
static void do_sea(void *data, unsigned long addr, unsigned long esr, struct pt_regs *regs)
{
	dmc_sec_reg_check("FROM SYNC SERROR -- ", 0xff);
}
#else
static void do_serror(void *data, struct pt_regs *regs, unsigned int esr, int *ret)
{
	/*
	 * arm64 calls nmi_enter() before do_serror(),
	 * it will cause printk entered deferred mode
	 * and not outputs to console immediately.
	 */
	if (in_nmi())
		__preempt_count_sub(NMI_OFFSET + HARDIRQ_OFFSET);

	dmc_sec_reg_check("FROM ASYNC SERROR -- ", 0xff);
	oops_in_progress++;
}
#endif
#endif

static int __init dmc_monitor_probe(struct platform_device *pdev)
{
	int r = 0, ports, i;
	unsigned int tmp;
	struct device_node *node;
	struct ddr_port_desc *desc = NULL;
	struct resource *res;
#if (IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)		&& \
	!IS_ENABLED(CONFIG_AMLOGIC_PAGE_TRACE_INLINE)	&& \
	IS_ENABLED(CONFIG_ANDROID_VENDOR_HOOKS))
	if (!dmc_trace_buffer)
		register_trace_android_vh_get_page_wmark(get_page_trace_buf_hook, NULL);
#endif
	pr_info("dmc version:%s\n", DMC_VERSION);
	dmc_mon = devm_kzalloc(&pdev->dev, sizeof(*dmc_mon), GFP_KERNEL);
	if (!dmc_mon)
		return -ENOMEM;

	tmp = (unsigned long)of_device_get_match_data(&pdev->dev);
	pr_debug("%s, chip type:%d\n", __func__, tmp);
	ports = ddr_find_port_desc(tmp, &desc);
	if (ports < 0) {
		pr_err("can't get port desc\n");
		dmc_mon = NULL;
		return -EINVAL;
	}
	dmc_mon->chip = tmp;
	dmc_mon->port_num = ports;
	dmc_mon->port = desc;
	get_dmc_ops(dmc_mon->chip, dmc_mon);

	vpu_sub_desc_init(tmp, &dmc_mon->vpu_sub);

	node = pdev->dev.of_node;
	r = of_property_read_u32(node, "reg_base", &tmp);
	if (r < 0) {
		pr_err("can't find iobase\n");
		dmc_mon = NULL;
		return -EINVAL;
	}

	dmc_mon->sec_base = tmp;

	/* for register not in secure world */
	for (i = 0; i < dmc_mon->mon_number; i++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (res) {
			dmc_mon->mon_comm[i].io_base = res->start;
			dmc_mon->mon_comm[i].io_mem = ioremap(res->start, resource_size(res));
		}
	}

	r = class_register(&dmc_monitor_class);
	if (r) {
		pr_err("regist dmc_monitor_class failed\n");
		dmc_mon = NULL;
		return -EINVAL;
	}

	/* clear reg before set it */
	if (dmc_mon->ops && dmc_mon->ops->disable)
		dmc_mon->ops->disable(dmc_mon);

	if (init_dmc_debug)
		dmc_mon->debug = init_dmc_debug;

	if (init_dmc_irq_thread_en)
		dmc_mon->debug |= DMC_DEBUG_IRQ_THREAD;

	if (init_dmc_mode == DMC_MODE_RESERVED)
		dmc_enabled_reserved_memory(pdev, dmc_mon);

	if (init_dmc_mode == DMC_MODE_DEFAULT)
		dmc_set_default(dmc_mon);

	if (init_dmc_mode == DMC_MODE_NORMAL) {
		dmc_set_monitor(init_start_addr,
				init_end_addr, init_dev_mask, DMC_MODE_NORMAL, 1);
	}

#if IS_ENABLED(CONFIG_ANDROID_VENDOR_HOOKS)
#if CONFIG_AMLOGIC_KERNEL_VERSION >= 14515
	register_trace_android_rvh_arm64_serror_panic(arm64_serror_panic, NULL);
	register_trace_android_rvh_do_sea(do_sea, NULL);
#else
	register_trace_android_rvh_do_serror(do_serror, NULL);
#endif
#endif

#ifdef ARM
	set_dump_dmc_func(&dmc_sec_dump_arm);
#endif
	/* clear prot vio reg */
	for (i = 0; i < dmc_mon->mon_number; i++) {
		if (dmc_mon->ops && dmc_mon->ops->handle_irq)
			dmc_mon->ops->handle_irq(dmc_mon, &dmc_mon->mon_comm[i], IRQ_CLEAR);
	}

	/* check dmc sec reg*/
	dmc_sec_reg_check("FROM PROBE -- ", 0xff);

	if (dmc_irq_request(node) < 0)
		pr_emerg("request dmc irq failed\n");

	return 0;
}

static void dmc_monitor_remove(struct platform_device *pdev)
{
	class_unregister(&dmc_monitor_class);
	dmc_mon = NULL;
}

#ifdef CONFIG_OF
static const struct of_device_id dmc_monitor_match[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,dmc_monitor-gxl",
		.data = (void *)DMC_TYPE_GXL,	/* chip id */
	},
	{
		.compatible = "amlogic,dmc_monitor-gxm",
		.data = (void *)DMC_TYPE_GXM,
	},
	{
		.compatible = "amlogic,dmc_monitor-gxlx",
		.data = (void *)DMC_TYPE_GXLX,
	},
	{
		.compatible = "amlogic,dmc_monitor-axg",
		.data = (void *)DMC_TYPE_AXG,
	},
	{
		.compatible = "amlogic,dmc_monitor-txl",
		.data = (void *)DMC_TYPE_TXL,
	},
	{
		.compatible = "amlogic,dmc_monitor-txlx",
		.data = (void *)DMC_TYPE_TXLX,
	},
	{
		.compatible = "amlogic,dmc_monitor-txhd",
		.data = (void *)DMC_TYPE_TXHD,
	},
	{
		.compatible = "amlogic,dmc_monitor-tl1",
		.data = (void *)DMC_TYPE_TL1,
	},
	{
		.compatible = "amlogic,dmc_monitor-c1",
		.data = (void *)DMC_TYPE_C1,
	},
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic,dmc_monitor-c2",
		.data = (void *)DMC_TYPE_C2,
	},
	{
		.compatible = "amlogic,dmc_monitor-g12a",
		.data = (void *)DMC_TYPE_G12A,
	},
	{
		.compatible = "amlogic,dmc_monitor-sm1",
		.data = (void *)DMC_TYPE_SM1,
	},
	{
		.compatible = "amlogic,dmc_monitor-g12b",
		.data = (void *)DMC_TYPE_G12B,
	},
	{
		.compatible = "amlogic,dmc_monitor-tm2",
		.data = (void *)DMC_TYPE_TM2,
	},
	{
		.compatible = "amlogic,dmc_monitor-t5",
		.data = (void *)DMC_TYPE_T5,
	},
	{
		.compatible = "amlogic,dmc_monitor-t5d",
		.data = (void *)DMC_TYPE_T5D,
	},
	{
		.compatible = "amlogic,dmc_monitor-t5w",
		.data = (void *)DMC_TYPE_T5W,
	},
	{
		.compatible = "amlogic,dmc_monitor-t7",
		.data = (void *)DMC_TYPE_T7,
	},
	{
		.compatible = "amlogic,dmc_monitor-t3",
		.data = (void *)DMC_TYPE_T3,
	},
	{
		.compatible = "amlogic,dmc_monitor-p1",
		.data = (void *)DMC_TYPE_P1,
	},
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	{
		.compatible = "amlogic,dmc_monitor-s4",
		.data = (void *)DMC_TYPE_S4,
	},
#endif
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	{
		.compatible = "amlogic,dmc_monitor-sc2",
		.data = (void *)DMC_TYPE_SC2,
	},
	{
		.compatible = "amlogic,dmc_monitor-c3",
		.data = (void *)DMC_TYPE_C3,
	},
	{
		.compatible = "amlogic,dmc_monitor-t5m",
		.data = (void *)DMC_TYPE_T5M,
	},
	{
		.compatible = "amlogic,dmc_monitor-s5",
		.data = (void *)DMC_TYPE_S5,
	},
	{
		.compatible = "amlogic,dmc_monitor-s6",
		.data = (void *)DMC_TYPE_S6,
	},
	{
		.compatible = "amlogic,dmc_monitor-t3x",
		.data = (void *)DMC_TYPE_T3X,
	},
	{
		.compatible = "amlogic,dmc_monitor-txhd2",
		.data = (void *)DMC_TYPE_TXHD2,
	},
	{
		.compatible = "amlogic,dmc_monitor-a4",
		.data = (void *)DMC_TYPE_A4,
	},
	{
		.compatible = "amlogic,dmc_monitor-a5",
		.data = (void *)DMC_TYPE_A5,
	},
	{
		.compatible = "amlogic,dmc_monitor-s7",
		.data = (void *)DMC_TYPE_S7,
	},
	{
		.compatible = "amlogic,dmc_monitor-s7d",
		.data = (void *)DMC_TYPE_S7D,
	},
	{
		.compatible = "amlogic,dmc_monitor-t6d",
		.data = (void *)DMC_TYPE_T6D,
	},
	{
		.compatible = "amlogic,dmc_monitor-t6w",
		.data = (void *)DMC_TYPE_T6W,
	},
	{
		.compatible = "amlogic,dmc_monitor-t6x",
		.data = (void *)DMC_TYPE_T6X,
	},
	{
		.compatible = "amlogic,dmc_monitor-c4",
		.data = (void *)DMC_TYPE_C4,
	},
	{
		.compatible = "amlogic,dmc_monitor-a9",
		.data = (void *)DMC_TYPE_A9,
	},
#endif
	{
		.compatible = "amlogic,dmc_monitor-s1a",
		.data = (void *)DMC_TYPE_S1A,
	},
	{}
};
#endif

static struct platform_driver dmc_monitor_driver = {
	.driver = {
		.name  = "dmc_monitor",
		.owner = THIS_MODULE,
	},
	.remove = dmc_monitor_remove,
};

int __init dmc_monitor_init(void)
{
#ifdef CONFIG_OF
	const struct of_device_id *match_id;

	match_id = dmc_monitor_match;
	dmc_monitor_driver.driver.of_match_table = match_id;
#endif

	platform_driver_probe(&dmc_monitor_driver, dmc_monitor_probe);
	return 0;
}

void dmc_monitor_exit(void)
{
	platform_driver_unregister(&dmc_monitor_driver);
}
