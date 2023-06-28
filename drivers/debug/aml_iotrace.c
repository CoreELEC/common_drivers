// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define SKIP_LOCKUP_CHECK
#include <linux/init.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/of_address.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/math.h>
#include <asm/page.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/trace_clock.h>
#include <asm-generic/div64.h>
#include <linux/amlogic/aml_iotrace.h>
#include <linux/amlogic/gki_module.h>
#include <linux/workqueue.h>
#define AML_PERSISTENT_RAM_SIG (0x4c4d41) /* AML */

static int ramoops_ftrace_en;

/*
 * bit0: io_trace
 * bit1: sched_trace
 * bit2: irq_trace
 * bit3: smc_trace
 * bit4: other_trace
 * disable bits will forbit record this type log
 * record all type log as default
 */
int ramoops_trace_mask = 0x1f;
EXPORT_SYMBOL(ramoops_trace_mask);

static int ramoops_trace_mask_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &ramoops_trace_mask)) {
		pr_err("ramoops_trace_mask error: %s\n", buf);
		return -EINVAL;
	}

	return 0;
}
__setup("ramoops_trace_mask=", ramoops_trace_mask_setup);

int ramoops_io_skip = 1;

static int ramoops_io_skip_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &ramoops_io_skip)) {
		pr_err("ramoops_io_skip error: %s\n", buf);
		return -EINVAL;
	}

	return 0;
}
__setup("ramoops_io_skip=", ramoops_io_skip_setup);

int ramoops_io_en;
EXPORT_SYMBOL(ramoops_io_en);

static int ramoops_io_en_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &ramoops_io_en)) {
		pr_err("ramoops_io_en error: %s\n", buf);
		return -EINVAL;
	}

	return 0;
}
__setup("ramoops_io_en=", ramoops_io_en_setup);

int ramoops_io_dump;

static int ramoops_io_dump_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &ramoops_io_dump)) {
		pr_err("ramoops_io_dump error: %s\n", buf);
		return -EINVAL;
	}

	return 0;
}
__setup("ramoops_io_dump=", ramoops_io_dump_setup);

int meson_clk_debug;

static int meson_clk_debug_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &meson_clk_debug)) {
		pr_err("meson_clk_debug error: %s\n", buf);
		return -EINVAL;
	}

	return 0;
}
__setup("meson_clk_debug=", meson_clk_debug_setup);

int meson_pd_debug;

static int meson_pd_debug_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &meson_pd_debug)) {
		pr_err("meson_pd_debug error: %s\n", buf);
		return -EINVAL;
	}

	return 0;
}
__setup("meson_pd_debug=", meson_pd_debug_setup);

/* ramoops_io_dump_delay_secs : iotrace dump delayed time, s */
static int ramoops_io_dump_delay_secs = 10; /* default : 10s */

static struct delayed_work iotrace_work;

static int ramoops_io_dump_delay_secs_setup(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (kstrtoint(buf, 0, &ramoops_io_dump_delay_secs)) {
		pr_err("ramoops_io_dump_delay_secs error: %s\n", buf);
		return -EINVAL;
	}

	return 0;
}
__setup("ramoops_io_dump_delay_secs=", ramoops_io_dump_delay_secs_setup);

struct prz_record_iter {
	void *ptr;
	unsigned long size;
	unsigned long total_size;
	int cpu;
	enum aml_pstore_type_id type;
	bool over;
};

const char *record_name[] = {
	"IO-R",
	"IO-W",
	"IO-R-E",
	"IO-W-E",
};

struct percpu_trace_data {
	void *ptr;
	unsigned long total_size;
};

struct record_head {
	u32 magic;		/* 0xabcdef */
	int pid;
	u64 time;
	char comm[16];
	ssize_t size;
};

struct aml_persistent_ram_buffer {
	u32			sig;
	atomic_t	start;
	atomic_t	size;
	u8			data[];
};

struct aml_persistent_ram_zone {
	phys_addr_t paddr;
	size_t size;
	void *vaddr;
	enum aml_pstore_type_id type;
	int cpu;
	struct aml_persistent_ram_buffer *buffer;
	size_t buffer_size;
	char *old_log;
	size_t old_log_size;
};

struct aml_ramoops_context {
	struct aml_persistent_ram_zone **io_przs;		/* io message zones */
	struct aml_persistent_ram_zone **sched_przs;	/* sched message zones */
	struct aml_persistent_ram_zone **irq_przs;		/* irq message zones */
	struct aml_persistent_ram_zone **smc_przs;		/* smc message zones */
	struct aml_persistent_ram_zone **misc_przs;	/* other message zones */
	struct prz_record_iter record_iter[AML_PSTORE_TYPE_MAX][8]; // max possible cpu = 8
	struct prz_record_iter *curr_record_iter;
	unsigned int seq_file_overflow;
	/* percpu sched/irq/smc/other/io size */
	unsigned int io_size;
	unsigned int sched_size;
	unsigned int irq_size;
	unsigned int smc_size;
	unsigned int misc_size;
	phys_addr_t phys_addr;
	unsigned long size;
};

static struct aml_ramoops_context aml_oops_cxt;

static int aml_ramoops_parse_dt(void)
{
	struct aml_ramoops_context *cxt = &aml_oops_cxt;
	struct device_node *node;
	struct resource res;
	int ret, possible_cpu;

	possible_cpu = num_possible_cpus();

	node = of_find_node_by_path("/reserved-memory/linux,iotrace");

	if (!node)
		return -EINVAL;

	ret = of_address_to_resource(node, 0, &res);
	if (ret)
		return -EINVAL;

	cxt->phys_addr = res.start;
	cxt->size = res.end - res.start + 1;

	of_property_read_u32(node, "io-size", &cxt->io_size);
	of_property_read_u32(node, "sched-size", &cxt->sched_size);
	of_property_read_u32(node, "irq-size", &cxt->irq_size);
	of_property_read_u32(node, "smc-size", &cxt->smc_size);
	of_property_read_u32(node, "misc-size", &cxt->misc_size);

	if (((cxt->io_size + cxt->sched_size + cxt->irq_size + cxt->smc_size +
			cxt->misc_size) * possible_cpu) > cxt->size) {
		pr_err("Please check linux,iotrace dts configuration\n");
		return -EINVAL;
	}

	return 0;
}

static void *aml_persistent_ram_vmap(phys_addr_t start, size_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_writecombine(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}

	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);

	/*
	 * Since vmap() uses page granularity, we must add the offset
	 * into the page here, to get the byte granularity address
	 * into the mapping to represent the actual "start" location.
	 */
	return vaddr + offset_in_page(start);
}

static inline size_t buffer_size(struct aml_persistent_ram_zone *prz)
{
	return atomic_read(&prz->buffer->size);
}

static inline size_t buffer_start(struct aml_persistent_ram_zone *prz)
{
	return atomic_read(&prz->buffer->start);
}

static char reboot_mode[16];
static int reboot_mode_setup(char *s)
{
	if (s)
		snprintf(reboot_mode, sizeof(reboot_mode), "%s", s);

	return 0;
}
__setup("reboot_mode=", reboot_mode_setup);

static bool is_shutdown_reboot(void)
{
	return !strcmp(reboot_mode, "shutdown_reboot");
}

static bool is_cold_boot(void)
{
	return !strcmp(reboot_mode, "cold_boot");
}

static void aml_persistent_ram_save_old(struct aml_persistent_ram_zone *prz)
{
	struct aml_persistent_ram_buffer *buffer = prz->buffer;
	size_t size = buffer_size(prz);
	size_t start = buffer_start(prz);

	if (is_shutdown_reboot() || is_cold_boot())
		return;

	if (!size)
		return;

	if (!prz->old_log)
		prz->old_log = kmalloc(size, GFP_KERNEL);

	if (!prz->old_log)
		return;

	prz->old_log_size = size;
	memcpy_fromio(prz->old_log, &buffer->data[start], size - start);
	memcpy_fromio(prz->old_log + size - start, &buffer->data[0], start);
}

static int aml_persistent_ram_post_init(struct aml_persistent_ram_zone *prz)
{
	u32 sig = AML_PERSISTENT_RAM_SIG;

	if (prz->buffer->sig == sig) {
		if (buffer_size(prz) == 0) {
			pr_debug("found existing empty buffer\n");
			return 0;
		}

		if (buffer_size(prz) > prz->buffer_size ||
			buffer_start(prz) > buffer_size(prz)) {
			pr_info("found existing invalid buffer, size %zu, start %zu\n",
				buffer_size(prz), buffer_start(prz));
		} else {
			pr_debug("found existing buffer, size %zu, start %zu\n",
				 buffer_size(prz), buffer_start(prz));
			aml_persistent_ram_save_old(prz);
		}
	} else {
		pr_debug("no valid data in buffer (sig = 0x%08x)\n",
			 prz->buffer->sig);
		prz->buffer->sig = sig;
	}

	atomic_set(&prz->buffer->start, 0);
	atomic_set(&prz->buffer->size, 0);

	return 0;
}

static void aml_persistent_ram_free_old(struct aml_persistent_ram_zone *prz)
{
	kfree(prz->old_log);
	prz->old_log = NULL;
	prz->old_log_size = 0;
}

static void aml_persistent_ram_free(struct aml_persistent_ram_zone *prz)
{
	if (!prz)
		return;

	if (prz->vaddr) {
		vunmap(prz->vaddr - offset_in_page(prz->paddr));
		prz->vaddr = NULL;
	}

	aml_persistent_ram_free_old(prz);
	kfree(prz);
}

static struct aml_persistent_ram_zone *aml_persistent_ram_new(phys_addr_t start, size_t size,
			enum aml_pstore_type_id type, int cpu)
{
	struct aml_persistent_ram_zone *prz;
	int ret = -ENOMEM;

	prz = kzalloc(sizeof(*prz), GFP_KERNEL);
	if (!prz)
		goto err;

	/* Initialize general buffer state. */
	prz->type = type;
	prz->size = size;
	prz->cpu = cpu;
	prz->paddr = start + cpu * size;
	prz->size = size;

	prz->vaddr = aml_persistent_ram_vmap(prz->paddr, prz->size);
	if (!prz->vaddr)
		goto err;

	prz->buffer = prz->vaddr;
	prz->buffer_size = size - sizeof(struct aml_persistent_ram_buffer);

	ret = aml_persistent_ram_post_init(prz);
	if (ret)
		goto err;

	return prz;
err:
	aml_persistent_ram_free(prz);
	return ERR_PTR(ret);
}

static int aml_ramoops_init_przs(struct aml_persistent_ram_zone ***przs,
								  enum aml_pstore_type_id type)
{
	int cpu;
	int possible_cpu = num_possible_cpus();
	struct aml_persistent_ram_zone **prz_ar;
	struct aml_ramoops_context *cxt = &aml_oops_cxt;
	phys_addr_t phys_base;
	ssize_t phys_size;

	prz_ar = NULL;

	switch (type) {
	case AML_PSTORE_TYPE_IO:
		phys_base = cxt->phys_addr;
		phys_size = cxt->io_size;
		break;
	case AML_PSTORE_TYPE_SCHED:
		phys_base = cxt->phys_addr + possible_cpu * cxt->io_size;
		phys_size = cxt->sched_size;
		break;
	case AML_PSTORE_TYPE_IRQ:
		phys_base = cxt->phys_addr + possible_cpu * (cxt->io_size + cxt->sched_size);
		phys_size = cxt->irq_size;
		break;
	case AML_PSTORE_TYPE_SMC:
		phys_base = cxt->phys_addr + possible_cpu * (cxt->io_size + cxt->sched_size
						+ cxt->irq_size);
		phys_size = cxt->smc_size;
		break;
	case AML_PSTORE_TYPE_MISC:
		phys_base = cxt->phys_addr + possible_cpu * (cxt->io_size + cxt->sched_size
						+ cxt->irq_size + cxt->smc_size);
		phys_size = cxt->misc_size;
		break;
	default:
		pr_err("Wrong ram zone type\n");
		return -ENOMEM;
	}

	prz_ar = kcalloc(possible_cpu, sizeof(**przs), GFP_KERNEL);
	if (!prz_ar)
		return -ENOMEM;

	for (cpu = 0; cpu < possible_cpu; cpu++) {
		prz_ar[cpu] = aml_persistent_ram_new(phys_base, phys_size, type, cpu);
		if (IS_ERR(prz_ar[cpu])) {
			while (cpu > 0) {
				cpu--;
				aml_persistent_ram_free(prz_ar[cpu]);
			}
			kfree(prz_ar);
			return -ENOMEM;
		}
	}

	*przs = prz_ar;
	return 0;
}

static size_t aml_buffer_start_add(struct aml_persistent_ram_zone *prz, size_t a)
{
	int old;
	int new;

	old = atomic_read(&prz->buffer->start);
	new = old + a;
	while (unlikely(new >= prz->buffer_size))
		new -= prz->buffer_size;
	atomic_set(&prz->buffer->start, new);

	return old;
}

static void aml_buffer_size_add(struct aml_persistent_ram_zone *prz, size_t a)
{
	size_t old;
	size_t new;

	old = atomic_read(&prz->buffer->size);
	if (old == prz->buffer_size)
		return;

	new = old + a;
	if (new > prz->buffer_size)
		new = prz->buffer_size;
	atomic_set(&prz->buffer->size, new);
}

static void notrace aml_persistent_ram_update(struct aml_persistent_ram_zone *prz,
	const void *s, unsigned int start, unsigned int count)
{
	struct aml_persistent_ram_buffer *buffer = prz->buffer;

	memcpy_toio(buffer->data + start, s, count);
}

static int notrace aml_persistent_ram_write(struct aml_persistent_ram_zone *prz,
	const void *s, unsigned int count)
{
	int rem;
	int c = count;
	size_t start;

	if (unlikely(c > prz->buffer_size)) {
		s += c - prz->buffer_size;
		c = prz->buffer_size;
	}

	aml_buffer_size_add(prz, c);

	start = aml_buffer_start_add(prz, c);

	rem = prz->buffer_size - start;
	if (unlikely(rem < c)) {
		aml_persistent_ram_update(prz, s, start, rem);
		s += rem;
		c -= rem;
		start = 0;
	}
	aml_persistent_ram_update(prz, s, start, c);

	return count;
}

static void notrace aml_ramoops_pstore_write(int cpu, enum aml_pstore_type_id type,
				char *buf, ssize_t size)
{
	switch (type) {
	case AML_PSTORE_TYPE_IO:
		aml_persistent_ram_write(aml_oops_cxt.io_przs[cpu], buf, size);
		break;
	case AML_PSTORE_TYPE_SCHED:
		aml_persistent_ram_write(aml_oops_cxt.sched_przs[cpu], buf, size);
		break;
	case AML_PSTORE_TYPE_IRQ:
		aml_persistent_ram_write(aml_oops_cxt.irq_przs[cpu], buf, size);
		break;
	case AML_PSTORE_TYPE_SMC:
		aml_persistent_ram_write(aml_oops_cxt.smc_przs[cpu], buf, size);
		break;
	case AML_PSTORE_TYPE_MISC:
		aml_persistent_ram_write(aml_oops_cxt.misc_przs[cpu], buf, size);
		break;
	default:
		pr_info("Error record type %d\n", type);
		break;
	}
}

void aml_pstore_write(enum aml_pstore_type_id type, char *buf, unsigned long size)
{
	struct record_head head;
	char buf_tmp[1024];
	int cpu;
	unsigned long flags = 0;

	if (!ramoops_ftrace_en || !ramoops_io_en)
		return;

	if (!size)
		size = strlen(buf);

	local_irq_save(flags);
	memset(buf_tmp, 0, sizeof(buf_tmp));
	head.time = trace_clock_local();
	head.pid = current->pid;
	head.magic = 0xabcdef;
	head.size = roundup(size, 8); // round up to 8
	strscpy(head.comm, current->comm, sizeof(head.comm));
	memcpy(buf_tmp, &head, sizeof(struct record_head));
	memcpy(buf_tmp + sizeof(struct record_head), buf, size);

	if (head.size + sizeof(struct record_head) + 1 > sizeof(buf_tmp))
		head.size = sizeof(buf_tmp) - sizeof(struct record_head);

	cpu = smp_processor_id();
	aml_ramoops_pstore_write(cpu, type, buf_tmp, head.size + sizeof(struct record_head));
	local_irq_restore(flags);
}
EXPORT_SYMBOL(aml_pstore_write);

static int aml_ramoops_init(void)
{
	int ret = 0;

	ret = aml_ramoops_parse_dt();
	if (ret)
		return -EINVAL;

	ret = aml_ramoops_init_przs(&aml_oops_cxt.io_przs, AML_PSTORE_TYPE_IO);
	if (ret)
		return -ENOMEM;

	ret = aml_ramoops_init_przs(&aml_oops_cxt.sched_przs, AML_PSTORE_TYPE_SCHED);
	if (ret)
		return -ENOMEM;

	ret = aml_ramoops_init_przs(&aml_oops_cxt.irq_przs, AML_PSTORE_TYPE_IRQ);
	if (ret)
		return -ENOMEM;

	ret = aml_ramoops_init_przs(&aml_oops_cxt.smc_przs, AML_PSTORE_TYPE_SMC);
	if (ret)
		return -ENOMEM;

	ret = aml_ramoops_init_przs(&aml_oops_cxt.misc_przs, AML_PSTORE_TYPE_MISC);
	if (ret)
		return -ENOMEM;

	return ret;
}

static void *percpu_trace_start(struct seq_file *s, loff_t *pos)
{
	struct aml_persistent_ram_zone *prz = (struct aml_persistent_ram_zone *)s->private;
	struct percpu_trace_data *data;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return NULL;

	data->ptr = (void *)prz->old_log + *pos;
	data->total_size = prz->old_log_size;

	while (*(u32 *)data->ptr != 0xabcdef) {
		(data->ptr)++;
		(*pos)++;

		if (*pos + sizeof(struct record_head) > data->total_size) {
			kfree(data);
			return NULL;
		}
	}

	if (*pos + sizeof(struct record_head) +
			((struct record_head *)data->ptr)->size > data->total_size) {
		kfree(data);
		return NULL;
	}

	return data;
}

static void percpu_trace_stop(struct seq_file *s, void *v)
{
	kfree(v);
}

static void *percpu_trace_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct percpu_trace_data *data = v;
	struct record_head *head = (struct record_head *)data->ptr;

	data->ptr += sizeof(struct record_head) + head->size;
	*pos += sizeof(struct record_head) + head->size;

	if (*pos + sizeof(struct record_head) > data->total_size)
		return NULL;

	if (*pos + sizeof(struct record_head) +
			((struct record_head *)data->ptr)->size > data->total_size)
		return NULL;

	return data;
}

static int percpu_trace_show(struct seq_file *s, void *v)
{
	char buf[1024];
	unsigned long sec = 0, us = 0;
	unsigned long long time;
	struct aml_persistent_ram_zone *prz;
	struct record_head *head;
	struct percpu_trace_data *data = v;

	if (!data)
		return 0;

	prz = (struct aml_persistent_ram_zone *)s->private;
	head = (struct record_head *)data->ptr;

	time = head->time;
	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;

	if (prz->type == AML_PSTORE_TYPE_IO) {
		struct io_trace_data *io_data =
		(struct io_trace_data *)(data->ptr + sizeof(struct record_head));
		seq_printf(s, "[%04ld.%06ld@%d] <%s-%d> <%6s %08x-%8x>  <%ps <- %pS>\n",
			sec, us, prz->cpu, head->comm, head->pid, record_name[io_data->flag],
			io_data->reg, io_data->val, (void *)(io_data->ip & ~(0x3)),
			(void *)(io_data->parent_ip));
	} else {
		memset(buf, 0, sizeof(buf));
		memcpy(buf, (void *)head + sizeof(struct record_head), head->size);
		seq_printf(s, "[%04ld.%06ld@%d] <%s-%d> <%s>\n",
					sec, us, prz->cpu, head->comm, head->pid, buf);
	}

	return 0;
}

static const struct seq_operations percpu_trace_ops = {
	.start = percpu_trace_start,
	.next = percpu_trace_next,
	.stop = percpu_trace_stop,
	.show = percpu_trace_show,
};

static int percpu_trace_open(struct inode *inode, struct file *file)
{
	struct seq_file *sf;
	int err;

	err = seq_open(file, &percpu_trace_ops);
	if (err < 0)
		return err;

	sf = file->private_data;
	sf->private = PDE_DATA(inode);

	return 0;
}

static const struct proc_ops percpu_trace_file_ops = {
	.proc_open = percpu_trace_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

static void get_first_record_type(struct prz_record_iter **iter, int cpu,
				enum aml_pstore_type_id type, unsigned long long *min_time)
{
	switch (type) {
	case AML_PSTORE_TYPE_IO:
		aml_oops_cxt.record_iter[type][cpu].ptr = aml_oops_cxt.io_przs[cpu]->old_log;
		aml_oops_cxt.record_iter[type][cpu].total_size =
					aml_oops_cxt.io_przs[cpu]->old_log_size;
		break;

	case AML_PSTORE_TYPE_SCHED:
		aml_oops_cxt.record_iter[type][cpu].ptr = aml_oops_cxt.sched_przs[cpu]->old_log;
		aml_oops_cxt.record_iter[type][cpu].total_size =
					aml_oops_cxt.sched_przs[cpu]->old_log_size;
		break;

	case AML_PSTORE_TYPE_IRQ:
		aml_oops_cxt.record_iter[type][cpu].ptr = aml_oops_cxt.irq_przs[cpu]->old_log;
		aml_oops_cxt.record_iter[type][cpu].total_size =
					aml_oops_cxt.irq_przs[cpu]->old_log_size;
		break;

	case AML_PSTORE_TYPE_SMC:
		aml_oops_cxt.record_iter[type][cpu].ptr = aml_oops_cxt.smc_przs[cpu]->old_log;
		aml_oops_cxt.record_iter[type][cpu].total_size =
					aml_oops_cxt.smc_przs[cpu]->old_log_size;
		break;

	case AML_PSTORE_TYPE_MISC:
		aml_oops_cxt.record_iter[type][cpu].ptr = aml_oops_cxt.misc_przs[cpu]->old_log;
		aml_oops_cxt.record_iter[type][cpu].total_size =
					aml_oops_cxt.misc_przs[cpu]->old_log_size;
		break;
	default:
		pr_err("Error aml_pstore_type_id %d\n", type);
		return;
	}

	aml_oops_cxt.record_iter[type][cpu].type = type;
	aml_oops_cxt.record_iter[type][cpu].over = 0;
	aml_oops_cxt.record_iter[type][cpu].cpu = cpu;

	while (*(u32 *)aml_oops_cxt.record_iter[type][cpu].ptr != 0xabcdef) {
		aml_oops_cxt.record_iter[type][cpu].ptr++;
		aml_oops_cxt.record_iter[type][cpu].size++;
		if (aml_oops_cxt.record_iter[type][cpu].size + sizeof(struct record_head)
				> aml_oops_cxt.record_iter[type][cpu].total_size) {
			aml_oops_cxt.record_iter[type][cpu].over = 1;
			break;
		}
	}

	if (!aml_oops_cxt.record_iter[type][cpu].over &&
		((struct record_head *)aml_oops_cxt.record_iter[type][cpu].ptr)->time < *min_time) {
		*min_time = ((struct record_head *)aml_oops_cxt.record_iter[type][cpu].ptr)->time;
		*iter = &aml_oops_cxt.record_iter[type][cpu];
	}
}

static struct prz_record_iter *prz_record_iter_init(void)
{
	u64 min_time = ~0x0ULL;
	int possible_cpu = num_possible_cpus();
	struct prz_record_iter *iter = NULL;
	int cpu;

	memset(&aml_oops_cxt.record_iter[0][0], 0, sizeof(aml_oops_cxt.record_iter[0][0])
			* AML_PSTORE_TYPE_MAX * 8); // max possible cpu 8

	for (cpu = 0; cpu < possible_cpu; cpu++) {
		if (aml_oops_cxt.io_przs[cpu]->old_log)
			get_first_record_type(&iter, cpu, AML_PSTORE_TYPE_IO, &min_time);
		else
			aml_oops_cxt.record_iter[AML_PSTORE_TYPE_IO][cpu].over = 1;

		if (aml_oops_cxt.sched_przs[cpu]->old_log)
			get_first_record_type(&iter, cpu, AML_PSTORE_TYPE_SCHED, &min_time);
		else
			aml_oops_cxt.record_iter[AML_PSTORE_TYPE_SCHED][cpu].over = 1;

		if (aml_oops_cxt.irq_przs[cpu]->old_log)
			get_first_record_type(&iter, cpu, AML_PSTORE_TYPE_IRQ, &min_time);
		else
			aml_oops_cxt.record_iter[AML_PSTORE_TYPE_IRQ][cpu].over = 1;

		if (aml_oops_cxt.smc_przs[cpu]->old_log)
			get_first_record_type(&iter, cpu, AML_PSTORE_TYPE_SMC, &min_time);
		else
			aml_oops_cxt.record_iter[AML_PSTORE_TYPE_SMC][cpu].over = 1;

		if (aml_oops_cxt.misc_przs[cpu]->old_log)
			get_first_record_type(&iter, cpu, AML_PSTORE_TYPE_MISC, &min_time);
		else
			aml_oops_cxt.record_iter[AML_PSTORE_TYPE_MISC][cpu].over = 1;
	}
		return iter;
}

static void get_next_record_type(struct prz_record_iter **iter, int cpu,
		enum aml_pstore_type_id type, unsigned long long *min_time)
{
	if (!aml_oops_cxt.record_iter[type][cpu].over) {
		while (*(u32 *)aml_oops_cxt.record_iter[type][cpu].ptr != 0xabcdef) {
			aml_oops_cxt.record_iter[type][cpu].ptr++;
			aml_oops_cxt.record_iter[type][cpu].size++;
			if (aml_oops_cxt.record_iter[type][cpu].size +
				sizeof(struct record_head) >
				aml_oops_cxt.record_iter[type][cpu].total_size) {
				aml_oops_cxt.record_iter[type][cpu].over = 1;
				break;
				}
			}
			if (!aml_oops_cxt.record_iter[type][cpu].over &&
			((struct record_head *)aml_oops_cxt.record_iter[type][cpu].ptr)->time <
			*min_time) {
				*min_time =
			((struct record_head *)aml_oops_cxt.record_iter[type][cpu].ptr)->time;
				*iter = &aml_oops_cxt.record_iter[type][cpu];
			}
		}
}

static struct prz_record_iter *get_next_record(void)
{
	unsigned long long min_time = ~0x0ULL;
	struct prz_record_iter *iter;
	int possible_cpu = num_possible_cpus();
	int cpu;

	iter = NULL;

	for (cpu = 0; cpu < possible_cpu; cpu++) {
		get_next_record_type(&iter, cpu, AML_PSTORE_TYPE_IO, &min_time);
		get_next_record_type(&iter, cpu, AML_PSTORE_TYPE_SCHED, &min_time);
		get_next_record_type(&iter, cpu, AML_PSTORE_TYPE_IRQ, &min_time);
		get_next_record_type(&iter, cpu, AML_PSTORE_TYPE_SMC, &min_time);
		get_next_record_type(&iter, cpu, AML_PSTORE_TYPE_MISC, &min_time);
	}
		return iter;
}

static void trace_show_iter(struct seq_file *s, struct prz_record_iter *iter)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time;
	struct record_head *head = (struct record_head *)iter->ptr;
	char buf[1024];

	time = head->time;
	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;

	if (iter->type == AML_PSTORE_TYPE_IO) {
		struct io_trace_data *data =
		(struct io_trace_data *)(iter->ptr + sizeof(struct record_head));
		seq_printf(s, "[%04ld.%06ld@%d] <%s-%d> <%6s %08x-%8x>  <%ps <- %pS>\n",
			sec, us, iter->cpu, head->comm, head->pid, record_name[data->flag],
			data->reg, data->val, (void *)(data->ip & ~(0x3)),
			(void *)(data->parent_ip));
	} else {
		memset(buf, 0, sizeof(buf));
		memcpy(buf, (void *)head + sizeof(struct record_head), head->size);
		seq_printf(s, "[%04ld.%06ld@%d] <%s-%d> <%s>\n",
					sec, us, iter->cpu, head->comm, head->pid, buf);
	}
}

static void *trace_start(struct seq_file *seq, loff_t *pos)
{
	if (*pos && aml_oops_cxt.seq_file_overflow) {
		aml_oops_cxt.seq_file_overflow = 0;
		return aml_oops_cxt.curr_record_iter;
	}

	if (!*pos)
		aml_oops_cxt.curr_record_iter = prz_record_iter_init();
	else
		aml_oops_cxt.curr_record_iter = get_next_record();

	return aml_oops_cxt.curr_record_iter;
}

static void *trace_next(struct seq_file *seq, void *v, loff_t *pos)
{
	struct prz_record_iter *iter = (struct prz_record_iter *)v;
	struct record_head *head = iter->ptr;

	(*pos)++;
	iter->size += sizeof(struct record_head) + head->size;
	iter->ptr += sizeof(struct record_head) + head->size;

	aml_oops_cxt.curr_record_iter = get_next_record();

	return aml_oops_cxt.curr_record_iter;
}

static void trace_stop(struct seq_file *seq, void *v)
{
	aml_oops_cxt.seq_file_overflow = 1;
}

static int trace_show(struct seq_file *seq, void *v)
{
	trace_show_iter(seq, v);

	return 0;
}

static const struct seq_operations trace_ops = {
	.start  = trace_start,
	.next   = trace_next,
	.stop   = trace_stop,
	.show   = trace_show,
};

static int trace_open(struct inode *inode, struct file *filp)
{
	return seq_open(filp, &trace_ops);
}

static const struct proc_ops trace_file_ops = {
	.proc_open = trace_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = seq_release,
};

static void proc_init(void)
{
	struct proc_dir_entry *dir_aml_iotrace;
	struct proc_dir_entry *dir_aml_percpu;
	struct proc_dir_entry *dir_cpu_ar[8]; // max possible cpu 8
	int possible_cpu = num_possible_cpus();
	int cpu;
	char buf[10];

	dir_aml_iotrace = proc_mkdir("aml_iotrace", NULL);
	if (IS_ERR_OR_NULL(dir_aml_iotrace)) {
		pr_warn("failed to create /proc/aml_iotrace directory\n");
		dir_aml_iotrace = NULL;
		return;
	}

	/* proc/aml_iotrace/trace */
	proc_create_data("trace", S_IFREG | 0440,
				dir_aml_iotrace, &trace_file_ops, NULL);

	dir_aml_percpu = proc_mkdir("per_cpu", dir_aml_iotrace);
	if (IS_ERR_OR_NULL(dir_aml_percpu)) {
		pr_warn("failed to create /proc/per_cpu directory\n");
		dir_aml_percpu = NULL;
		return;
	}

	for (cpu = 0; cpu < possible_cpu; cpu++) {
		memset(buf, 0, sizeof(buf));
		sprintf(buf, "cpu%d", cpu);
		dir_cpu_ar[cpu] = proc_mkdir(buf, dir_aml_percpu);
		if (IS_ERR_OR_NULL(dir_cpu_ar[cpu])) {
			pr_warn("failed to create /proc/per_cpu/%s directory\n", buf);
			dir_aml_percpu = NULL;
			return;
		}

		if (aml_oops_cxt.io_przs[cpu]->old_log_size)
			proc_create_data("io_trace", S_IFREG | 0440, dir_cpu_ar[cpu],
				&percpu_trace_file_ops, aml_oops_cxt.io_przs[cpu]);

		if (aml_oops_cxt.sched_przs[cpu]->old_log_size)
			proc_create_data("sched_trace", S_IFREG | 0440, dir_cpu_ar[cpu],
				&percpu_trace_file_ops, aml_oops_cxt.sched_przs[cpu]);

		if (aml_oops_cxt.irq_przs[cpu]->old_log_size)
			proc_create_data("irq_trace", S_IFREG | 0440, dir_cpu_ar[cpu],
				&percpu_trace_file_ops, aml_oops_cxt.irq_przs[cpu]);

		if (aml_oops_cxt.smc_przs[cpu]->old_log_size)
			proc_create_data("smc_trace", S_IFREG | 0440, dir_cpu_ar[cpu],
				&percpu_trace_file_ops, aml_oops_cxt.smc_przs[cpu]);

		if (aml_oops_cxt.misc_przs[cpu]->old_log_size)
			proc_create_data("misc_trace", S_IFREG | 0440, dir_cpu_ar[cpu],
				&percpu_trace_file_ops, aml_oops_cxt.misc_przs[cpu]);
		}
}

static void trace_auto_show_iter(struct prz_record_iter *iter)
{
	unsigned long sec = 0, us = 0;
	unsigned long long time;
	struct record_head *head = (struct record_head *)iter->ptr;
	char buf[1024];

	time = head->time;
	do_div(time, 1000);
	us = (unsigned long)do_div(time, 1000000);
	sec = (unsigned long)time;

	if (iter->type == AML_PSTORE_TYPE_IO) {
		struct io_trace_data *data =
		(struct io_trace_data *)(iter->ptr + sizeof(struct record_head));
		pr_info("[%04ld.%06ld@%d] <%s-%d> <%6s %08x-%8x>  <%ps <- %pS>\n",
			sec, us, iter->cpu, head->comm, head->pid, record_name[data->flag],
			data->reg, data->val, (void *)(data->ip & ~(0x3)),
			(void *)(data->parent_ip));
	} else {
		memset(buf, 0, sizeof(buf));
		memcpy(buf, (void *)head + sizeof(struct record_head), head->size);
		pr_info("[%04ld.%06ld@%d] <%s-%d> <%s>\n",
					sec, us, iter->cpu, head->comm, head->pid, buf);
	}
	iter->size += sizeof(struct record_head) + head->size;
	iter->ptr += sizeof(struct record_head) + head->size;
}

void iotrace_auto_dump(void)
{
	aml_oops_cxt.curr_record_iter = prz_record_iter_init();
	if (!aml_oops_cxt.curr_record_iter)
		return;

	trace_auto_show_iter(aml_oops_cxt.curr_record_iter);

	while (1) {
		aml_oops_cxt.curr_record_iter = get_next_record();
		if (!aml_oops_cxt.curr_record_iter)
			break;

		trace_auto_show_iter(aml_oops_cxt.curr_record_iter);
	}
}

static void iotrace_work_func(struct work_struct *work)
{
	pr_info("ramoops_io_en:%d, ramoops_io_dump=%d, ramoops_io_skip=%d\n",
		ramoops_io_en, ramoops_io_dump, ramoops_io_skip);
	iotrace_auto_dump();
	cancel_delayed_work(&iotrace_work);
}

int __init aml_iotrace_init(void)
{
	int ret = 0;

	ret = aml_ramoops_init();
	if (ret) {
		pr_err("Fail to init ramoops\n");
		return 0;
	}

	proc_init();
	ftrace_ramoops_init();

	ramoops_ftrace_en = 1;
	pr_info("Amlogic debug-iotrace driver version V2\n");

	if (ramoops_io_dump) {
		INIT_DELAYED_WORK(&iotrace_work, iotrace_work_func);
		schedule_delayed_work(&iotrace_work, ramoops_io_dump_delay_secs * HZ);
	}

	return 0;
}

void __exit aml_iotrace_exit(void)
{
}

module_init(aml_iotrace_init);
module_exit(aml_iotrace_exit);
MODULE_LICENSE("GPL");
