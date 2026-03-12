// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/gfp.h>
#include <linux/kallsyms.h>
#include <linux/mmzone.h>
#include <linux/memblock.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/sort.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/memcontrol.h>
#include <linux/mm_inline.h>
#include <linux/proc_fs.h>
#include <linux/sched/clock.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/dma-map-ops.h>
#include <linux/cma.h>
#include <cma.h>
#include <linux/amlogic/page_trace.h>
#include <linux/amlogic/aml_cma.h>
#include <asm/stacktrace.h>
#include <asm/sections.h>
#include <linux/moduleparam.h>
#include <trace/hooks/mm.h>
#include <trace/events/kmem.h>
#include <linux/mm_types.h>
#include <linux/oom.h>
#ifdef CONFIG_KASAN
#include <kasan/kasan.h>
#endif

#define DEBUG_PAGE_TRACE	0

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
#include <linux/nodemask.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>

#include <asm/irq.h>
#include <asm/stack_pointer.h>
#include <asm/stacktrace.h>

#endif

#ifdef CONFIG_NUMA
#define COMMON_CALLER_SIZE	64	/* more function names if NUMA */
#else
#define COMMON_CALLER_SIZE	64
#endif

#ifdef CONFIG_ARM64
#define PAGE_TRACE_OFFSET	(_PAGE_END(CONFIG_ARM64_VA_BITS))
#else
#define PAGE_TRACE_OFFSET	PAGE_OFFSET
#endif

static const char * const aml_migratetype_names[MIGRATE_TYPES] = {
	"Unmovable",
	"Movable",
	"Reclaimable",
#ifdef CONFIG_CMA
	"CMA",
#endif
	"HighAtomic",
#ifdef CONFIG_MEMORY_ISOLATION
	"Isolate",
#endif
};

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
unsigned long *free_pages_bitmap;
#endif

/*
 * this is a driver which will hook during page alloc/free and
 * record caller of each page to a buffer. Detailed information
 * of page allocate statistics can be find in /proc/pagetrace
 *
 */
static bool merge_function = 1;
static int page_trace_filter = 64; /* not print size < page_trace_filter */
static int page_trace_filter_slab;
static int page_trace_filter_vmalloc;
static int page_trace_filter_large_slab;
static struct proc_dir_entry *d_pagetrace;
static struct proc_dir_entry *d_profile_default_cma;
struct page_trace *aml_trace_buffer;
#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
struct pagetrace_vendor_param trace_param;
EXPORT_SYMBOL(aml_trace_buffer);
#endif
static unsigned long ptrace_size;
unsigned int aml_trace_step = 1;
#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
EXPORT_SYMBOL(aml_trace_step);
#endif
static bool page_trace_disable;

struct alloc_caller {
	unsigned long func_start_addr;
	unsigned long size;
	int flag;
};

struct fun_symbol {
	const char *name;
	int flag;
};

static struct alloc_caller common_caller[COMMON_CALLER_SIZE];

/*
 * following functions are common API from page allocate, they should not
 * be record in page trace, parse for back trace should keep from these
 * functions
 */
static struct fun_symbol common_func[] = {
	{"__netdev_alloc_skb",			0},
	{"kmalloc_reserve",			0},
	{"__alloc_skb",				0},
	{"sk_prot_alloc",			0},
	{"sk_alloc",				0},
	{"__traceiter_android_vh_cma_alloc_bypass",	0},
	{"devm_kmalloc",			0},
	{"cma_alloc",				0},
	{"cma_alloc_aligned",			0},
	{"__cma_alloc",				0},
	{"__kasan_slab_alloc",			0},
	{"alloc_pages_bulk_noprof",		0},
	{"shmem_alloc_and_add_folio",		0},
	{"allocate_slab",			0},
	{"alloc_slab_page",			0},
	{"___slab_alloc",			0},
	{"__slab_alloc",			0},
	{"__slab_alloc_node",			0},
	{"slab_alloc_node",			0},
	{"new_slab",				0},
	{"kmem_cache_alloc_bulk_noprof",	0},
	{"__kmalloc_cache_node_noprof",		0},
	{"__kmalloc_cache_noprof",		0},
	{"__kmalloc_node_track_caller_noprof",	0},
	{"__kmalloc_noprof",			0},
	{"__kmalloc_node_noprof",		0},
	{"__kmalloc_large_node_noprof",		2},
	{"___kmalloc_large_node",		2},
	{"__kmalloc_large_noprof",		2},
	{"kmem_cache_alloc_node_noprof",	0},
	{"kmem_cache_alloc_lru_noprof",		0},
	{"kmem_cache_alloc_noprof",		0},
	{"alloc_pages_exact_nid_noprof",	0},
	{"alloc_pages_exact_noprof",		0},
	{"get_zeroed_page_noprof",		0},
	{"get_free_pages_noprof",		0},
	{"__folio_alloc_noprof",		0},
	{"__alloc_pages_noprof",		0},
	{"__filemap_get_folio",			0},
	{"__get_node_page",			0},
	{"generic_perform_write",		0},
	{"vzalloc_node_noprof",			1},
	{"vzalloc_noprof",			1},
	{"vmalloc_noprof",			1},
	{"__vmalloc_noprof",			1},
	{"__vmalloc_node_noprof",		1},
	{"__vmalloc_node_range_noprof",		1},
	{"trace_raw_output_kmem_cache_alloc",	0},
	{"trace_event_raw_event_kmem_cache_alloc",	0},
	{"__traceiter_mm_page_alloc",		0},
	{"__probestub_kmem_cache_alloc",	0},
	{"__traceiter_kmem_cache_alloc",	0},
	{"__vmalloc_array_noprof",		1},
	{"__kvmalloc_node_noprof",		1},
	{"kvmemdup",				0},
	{"kstrdup_const",			0},
	{"__kretprobe_trampoline_handler",	0},
	{"dma_alloc_contiguous",		0},
	{"dma_alloc_from_contiguous",		0},
	{"__dma_direct_alloc_pages",		0},
	{"dma_direct_alloc",			0},
	{"dma_alloc_pages",			0},
	{"dma_alloc_attrs",			0},
	{"aml_cma_alloc",			0},
	{"aml_cma_alloc_post_hook",		0},
	{"system_heap_allocate",		0},
	{"update_cma_page_trace",		0},
	{"aml_slub_alloc_large",		2},
#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
	{"alloc_pages_ret_handler",		0},
	{"page_alloc_callback",			0},
	{"set_page_trace",			0},
	{"find_back_trace",			0},
#endif
#ifdef CONFIG_ARM
	{"__dma_alloc",				},
	{"arm_dma_alloc",			},
	{"__alloc_from_contiguous",		},
#endif
	{}		/* tail */
};

/* ----------------------------------- */

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
#define MAX_SYMBOL_LEN	64
static char sym_lookup_off[MAX_SYMBOL_LEN] = "kallsyms_lookup_size_offset";
int (*aml_kallsyms_lookup_size_offset)(unsigned long addr,
					unsigned long *symbolsize,
					unsigned long *offset);

/* For each probe you need to allocate a kprobe structure */
static struct kprobe kp_lookup_off = {
	.symbol_name	= sym_lookup_off,
};

static char sym_lookup_name[MAX_SYMBOL_LEN] = "kallsyms_lookup_name";
unsigned long (*aml_syms_lookup)(const char *name);

int (*aml_kallsyms_on_each_symbol)(int (*fn)(void *, const char *, unsigned long),
			    void *data);

/* For each probe you need to allocate a kprobe structure */
static struct kprobe kp_lookup_name = {
	.symbol_name	= sym_lookup_name,
};

/* ----------------------------------- */

static void __maybe_unused page_free_callback(void *data, struct page *page,
		unsigned int order)
{
	reset_page_trace(page, order);
}

static void __maybe_unused page_alloc_callback(void *data, struct page *page,
			unsigned int order, gfp_t gfp_flags, int migratetype)
{
	set_page_trace(page, order, gfp_flags, NULL);
}

static void __maybe_unused migrate_folio_callback(void *data, struct folio *old_folio,
			struct folio *new_folio)
{
	replace_page_trace(folio_page(new_folio, 0), folio_page(old_folio, 0));
}
#endif

/* ----------------------------------- */

static inline bool page_trace_invalid(struct page_trace *trace)
{
	return trace->order == IP_INVALID;
}

#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
static int early_page_trace_param(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (strcmp(buf, "off") == 0)
		page_trace_disable = true;
	else if (strcmp(buf, "on") == 0)
		page_trace_disable = false;

	pr_info("page_trace %sabled\n", page_trace_disable ? "dis" : "en");

	return 0;
}

early_param("page_trace", early_page_trace_param);

static int early_page_trace_step(char *buf)
{
	if (!buf)
		return -EINVAL;

	if (!kstrtouint(buf, 10, &aml_trace_step))
		pr_info("page trace_step:%d\n", aml_trace_step);

	return 0;
}

early_param("page_trace_step", early_page_trace_step);
#endif

#if DEBUG_PAGE_TRACE
static inline bool range_ok(struct page_trace *trace)
{
	unsigned long offset;

#ifdef CONFIG_ARM
	offset = (trace->ret_ip << 3);
#else
	offset = (trace->ret_ip << 2);
#endif
	if (trace->module_flag) {
		if (offset >= MODULES_END)
			return false;
	} else {
		if (offset >= ((unsigned long)_end - aml_text))
			return false;
	}
	return true;
}

static bool check_trace_valid(struct page_trace *trace)
{
	unsigned long offset;

	if (trace->order == IP_INVALID)
		return true;

	if (trace->order >= 10 ||
	    trace->migrate_type >= MIGRATE_TYPES ||
	    !range_ok(trace)) {
		offset = (unsigned long)((trace - aml_trace_buffer));
		pr_err("bad trace:%p, %x, pfn:%lx, ip:%ps\n", trace,
			*((unsigned int *)trace),
			offset / sizeof(struct page_trace),
			(void *)_RET_IP_);
		return false;
	}
	return true;
}
#endif /* DEBUG_PAGE_TRACE */

static void push_ip(struct page_trace *base, struct page_trace *ip)
{
	int i;
	unsigned long end;

	for (i = aml_trace_step - 1; i > 0; i--)
		base[i] = base[i - 1];

	/* debug check */
#if DEBUG_PAGE_TRACE
	check_trace_valid(base);
#endif
	end = (((unsigned long)aml_trace_buffer) + ptrace_size);
	WARN_ON((unsigned long)(base + aml_trace_step - 1) >= end);

	base[0] = *ip;
}

/*
 * set up information for common caller in memory allocate API
 */
static int __nocfi setup_common_caller(unsigned long kaddr, int flag)
{
	unsigned long size, offset;
	int i = 0, ret;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		/* find a empty caller */
		if (!common_caller[i].func_start_addr)
			break;
	}
	if (i >= COMMON_CALLER_SIZE) {
		pr_err("%s, out of memory\n", __func__);
		return -1;
	}

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
	ret = aml_kallsyms_lookup_size_offset(kaddr, &size, &offset);
#else
	ret = kallsyms_lookup_size_offset(kaddr, &size, &offset);
#endif
	if (ret) {
		common_caller[i].func_start_addr = kaddr;
		common_caller[i].size = size;
		common_caller[i].flag = flag;
		pr_debug("setup %d caller:%lx + %lx, %ps\n",
			i, kaddr, size, (void *)kaddr);
		return 0;
	}

	pr_err("can't find symbol %ps\n", (void *)kaddr);
	return -1;
}

static int max_high;

static void dump_common_caller(int limit)
{
	int i;

	for (i = 0; i < COMMON_CALLER_SIZE; i++) {
		if (!common_caller[i].func_start_addr)
			break;

		if (i < limit)
			pr_info("dump: %2d, addr:%lx + %4lx, %ps, %d\n", i,
				common_caller[i].func_start_addr,
				common_caller[i].size,
				(void *)common_caller[i].func_start_addr,
				common_caller[i].flag);
	}
	max_high = i - 1;
	pr_info("max high:%d, filter size:%d\n", max_high, (int)ARRAY_SIZE(common_func));
}

static int  sym_cmp(const void *x1, const void *x2)
{
	struct alloc_caller *p1, *p2;

	p1 = (struct alloc_caller *)x1;
	p2 = (struct alloc_caller *)x2;

	/* descending order */
	return p1->func_start_addr < p2->func_start_addr ? 1 : -1;
}

static int match_common_caller(void *data, const char *name,
				       unsigned long addr)
{
	int i, ret;
	struct fun_symbol *s;

	if (!strcmp(name, "_etext"))	/* end of text */
		return -1;

	for (i = 0; i < ARRAY_SIZE(common_func); i++) {
		s = &common_func[i];
		if (!s->name)
			break;		/* end */
		if (!strcmp(name, s->name)) {	/* strict match */
			ret = setup_common_caller(addr, s->flag);
			break;
		}
	}
	return 0;
}

static int is_common_caller(struct alloc_caller *caller, unsigned long pc, void *arg)
{
	int ret = 0;
	int low = 0, high = max_high, mid;
	unsigned long add_l, add_h;

	while (1) {
		mid = (high + low) / 2;
		add_l = caller[mid].func_start_addr;
		add_h = caller[mid].func_start_addr + caller[mid].size;
		if (pc >= add_l && pc < add_h) {
			if (caller[mid].flag)
				*(int *)arg = caller[mid].flag;
			ret = 1;
			break;
		}

		if (low >= high)	/* still not match */
			break;

		if (pc < add_l)		/* caller is descending order */
			low = mid + 1;
		else
			high = mid - 1;

		/* fix range */
		if (high < 0)
			high = 0;
		if (low > max_high)
			low = max_high;
	}
	return ret;
}

unsigned long unpack_ip(struct page_trace *trace)
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

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
EXPORT_SYMBOL(unpack_ip);
#endif

#ifdef CONFIG_ARM64
/*
 * We can only safely access per-cpu stacks from current in a non-preemptible
 * context.
 */
#if CONFIG_AMLOGIC_KERNEL_VERSION < 14515
static bool aml_on_accessible_stack(const struct task_struct *tsk,
				unsigned long sp, unsigned long size,
				struct stack_info *info)
{
	if (info)
		info->type = STACK_TYPE_UNKNOWN;

	if (on_task_stack(tsk, sp, size, info))
		return true;
	if (tsk != current || preemptible())
		return false;
	if (on_irq_stack(sp, size, info))
		return true;
	if (on_overflow_stack(sp, size, info))
		return true;
	if (on_sdei_stack(sp, size, info))
		return true;

	return false;
}

/*
 * Unwind from one frame record (A) to the next frame record (B).
 *
 * We terminate early if the location of B indicates a malformed chain of frame
 * records (e.g. a cycle), determined based on the location and fp value of A
 * and the location (but not the fp value) of B.
 */
static int notrace aml_unwind_frame(struct task_struct *tsk, struct stackframe *frame)
{
	unsigned long fp = frame->fp;
	struct stack_info info;

	if (!tsk)
		tsk = current;

	/* Final frame; nothing to unwind */
	if (fp == (unsigned long)task_pt_regs(tsk)->stackframe)
		return -ENOENT;

	if (fp & 0x7)
		return -EINVAL;

	if (!aml_on_accessible_stack(tsk, fp, 16, &info))
		return -EINVAL;

	if (test_bit(info.type, frame->stacks_done))
		return -EINVAL;

	/*
	 * As stacks grow downward, any valid record on the same stack must be
	 * at a strictly higher address than the prior record.
	 *
	 * Stacks can nest in several valid orders, e.g.
	 *
	 * TASK -> IRQ -> OVERFLOW -> SDEI_NORMAL
	 * TASK -> SDEI_NORMAL -> SDEI_CRITICAL -> OVERFLOW
	 *
	 * ... but the nesting itself is strict. Once we transition from one
	 * stack to another, it's never valid to unwind back to that first
	 * stack.
	 */
	if (info.type == frame->prev_type) {
		if (fp <= frame->prev_fp)
			return -EINVAL;
	} else {
		set_bit(frame->prev_type, frame->stacks_done);
	}

	/*
	 * Record this frame record's values and location. The prev_fp and
	 * prev_type are only meaningful to the next unwind_frame() invocation.
	 */
	frame->fp = READ_ONCE_NOCHECK(*(unsigned long *)(fp));
	frame->pc = READ_ONCE_NOCHECK(*(unsigned long *)(fp + 8));
	frame->prev_fp = fp;
	frame->prev_type = info.type;

#if defined(CONFIG_FUNCTION_GRAPH_TRACER) && IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
	if (tsk->ret_stack &&
		(ptrauth_strip_insn_pac(frame->pc) == (unsigned long)return_to_handler)) {
		struct ftrace_ret_stack *ret_stack;
		/*
		 * This is a case where function graph tracer has
		 * modified a return address (LR) in a stack frame
		 * to hook a function return.
		 * So replace it to an original value.
		 */
		ret_stack = ftrace_graph_get_ret_stack(tsk, frame->graph++);
		if (WARN_ON_ONCE(!ret_stack))
			return -EINVAL;
		frame->pc = ret_stack->ret;
	}
#endif /* CONFIG_FUNCTION_GRAPH_TRACER */

	frame->pc = ptrauth_strip_insn_pac(frame->pc);

	return 0;
}
#endif
#endif

#ifdef CONFIG_MODULES
static const char *kallsyms_symbol_name(struct mod_kallsyms *kallsyms, unsigned int symnum)
{
	return kallsyms->strtab + kallsyms->symtab[symnum].st_name;
}

static char * const filter_module[] = {
	"aml_cma",
	"system_heap",
};

static int is_filter_module(struct module *mod)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(filter_module); i++) {
		if (!strncmp(mod->name, filter_module[i], strlen(filter_module[i])))
			return 0;
	}

	return 1;
}

static void track_modules_update_filter_list(struct module *mod)
{
	struct mod_kallsyms *kallsyms;
	int i = 0;

	if (is_filter_module(mod))
		return;

	pr_debug("module name: %s\n", mod->name);

	/* Use rcu_dereference_sched() to remain compliant with the sparse tool */
	preempt_disable();
	kallsyms = rcu_dereference_sched(mod->kallsyms);
	preempt_enable();

	for (i = 0; i < kallsyms->num_symtab; i++) {
		const Elf_Sym *sym = &kallsyms->symtab[i];

		if (sym->st_shndx == SHN_UNDEF)
			continue;

		match_common_caller(NULL, kallsyms_symbol_name(kallsyms, i),
					kallsyms_symbol_value(sym));
	}

	sort(common_caller, COMMON_CALLER_SIZE, sizeof(struct alloc_caller),
		sym_cmp, NULL);

	/* must call this func to update max_high parameter */
	dump_common_caller(0);
}

static int track_modules_notify(struct notifier_block *self,
			       unsigned long val, void *data)
{
	struct module *mod = data;

	switch (val) {
	case MODULE_STATE_LIVE:
		track_modules_update_filter_list(mod);
		break;
	case MODULE_STATE_GOING:
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block track_modules_nb = {
	.notifier_call = track_modules_notify,
	.priority = 0,
};
#endif /* CONFIG_MODULES */

static int find_static_common_symbol(void *data)
{
#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
	memset(common_caller, 0, sizeof(common_caller));
#ifdef CONFIG_MODULES
	register_module_notifier(&track_modules_nb);
#endif

	kallsyms_on_each_symbol(match_common_caller, NULL);
#else
	/* match_common_caller(); */
	aml_kallsyms_on_each_symbol(match_common_caller, NULL);
#endif
	sort(common_caller, COMMON_CALLER_SIZE, sizeof(struct alloc_caller),
		sym_cmp, NULL);

	/* must call this func to update max_high parameter */
	dump_common_caller(0);

	return 0;
}

#if (CONFIG_AMLOGIC_KERNEL_VERSION >= 14515) && defined(CONFIG_ARM64)
#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
static int backtrace_skip_limit = 2;
#else
static int backtrace_skip_limit = 1;
#endif
module_param(backtrace_skip_limit, int, 0644);

static bool aml_dump_backtrace_entry(void *arg, unsigned long where)
{
	return is_common_caller(common_caller, where, arg);
}

/*
 * Kernel unwind state
 *
 * @common:      Common unwind state.
 * @task:        The task being unwound.
 * @graph_idx:   Used by ftrace_graph_ret_addr() for optimized stack unwinding.
 * @kr_cur:      When KRETPROBES is selected, holds the kretprobe instance
 *               associated with the most recently encountered replacement lr
 *               value.
 */
struct aml_kunwind_state {
	struct unwind_state common;
	struct task_struct *task;
	int graph_idx;
#ifdef CONFIG_KRETPROBES
	struct llist_node *kr_cur;
#endif
};

static __always_inline void
aml_kunwind_init(struct aml_kunwind_state *state,
	     struct task_struct *task)
{
	unwind_init_common(&state->common);
	state->task = task;
}

/*
 * Start an unwind from a caller.
 *
 * The unwind will begin at the caller of whichever function this is inlined
 * into.
 *
 * The function which invokes this must be noinline.
 */
static __always_inline void
kunwind_init_from_caller(struct aml_kunwind_state *state)
{
	aml_kunwind_init(state, current);

	state->common.fp = (unsigned long)__builtin_frame_address(1);
	state->common.pc = (unsigned long)__builtin_return_address(0);
}

/*
 * Unwind from one frame record (A) to the next frame record (B).
 *
 * We terminate early if the location of B indicates a malformed chain of frame
 * records (e.g. a cycle), determined based on the location and fp value of A
 * and the location (but not the fp value) of B.
 */
static __always_inline int
kunwind_next(struct aml_kunwind_state *state)
{
	struct task_struct *tsk = state->task;
	unsigned long fp = state->common.fp;
	int err;

	/* Final frame; nothing to unwind */
	if (fp == (unsigned long)task_pt_regs(tsk)->stackframe)
		return -ENOENT;

	err = unwind_next_frame_record(&state->common);
	if (err)
		return err;

	state->common.pc = ptrauth_strip_kernel_insn_pac(state->common.pc);

	return 0;
}

typedef bool (*aml_kunwind_consume_fn)(const struct aml_kunwind_state *state, void *cookie);

static __always_inline unsigned long
do_kunwind(struct aml_kunwind_state *state, aml_kunwind_consume_fn consume_state,
	   void *cookie)
{
	unsigned int backtrace_skip_t = 0;

	while (1) {
		int ret;

		if (!consume_state(state, cookie)) {
			backtrace_skip_t++;
			if (backtrace_skip_t > backtrace_skip_limit)
				return state->common.pc;
		}
		ret = kunwind_next(state);
		if (ret < 0)
			return 0;
	}
}

/*
 * Per-cpu stacks are only accessible when unwinding the current task in a
 * non-preemptible context.
 */
#define AML_STACKINFO_CPU(name)					\
	({							\
			stackinfo_get_unknown();		\
	})

/*
 * SDEI stacks are only accessible when unwinding the current task in an NMI
 * context.
 */
#define AML_STACKINFO_SDEI(name)					\
	({							\
			stackinfo_get_unknown();		\
	})

#define AML_STACKINFO_EFI						\
	({							\
			stackinfo_get_unknown();		\
	})

static __always_inline unsigned long
kunwind_stack_walk(aml_kunwind_consume_fn consume_state,
		   void *cookie, struct task_struct *task,
		   struct pt_regs *regs)
{
	struct stack_info stacks[] = {
		stackinfo_get_task(task),
		AML_STACKINFO_CPU(irq),
#if defined(CONFIG_VMAP_STACK)
		AML_STACKINFO_CPU(overflow),
#endif
#if defined(CONFIG_VMAP_STACK) && defined(CONFIG_ARM_SDE_INTERFACE)
		AML_STACKINFO_SDEI(normal),
		AML_STACKINFO_SDEI(critical),
#endif
#ifdef CONFIG_EFI
		AML_STACKINFO_EFI,
#endif
	};
	struct aml_kunwind_state state = {
		.common = {
			.stacks = stacks,
			.nr_stacks = ARRAY_SIZE(stacks),
		},
	};

	kunwind_init_from_caller(&state);

	return do_kunwind(&state, consume_state, cookie);
}

struct aml_kunwind_consume_entry_data {
	stack_trace_consume_fn consume_entry;
	void *cookie;
};

static __always_inline bool
arch_kunwind_consume_entry(const struct aml_kunwind_state *state, void *cookie)
{
	struct aml_kunwind_consume_entry_data *data = cookie;

	return data->consume_entry(data->cookie, state->common.pc);
}

static noinstr unsigned long aml_arch_stack_walk(stack_trace_consume_fn consume_entry,
			      void *cookie, struct task_struct *task,
			      struct pt_regs *regs)
{
	struct aml_kunwind_consume_entry_data data = {
		.consume_entry = consume_entry,
		.cookie = cookie,
	};

	return kunwind_stack_walk(arch_kunwind_consume_entry, &data, task, regs);
}

#endif

unsigned long __nocfi find_back_trace(void *arg)
{
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515 || defined(CONFIG_ARM)
	struct stackframe frame = {0};
	int ret, step = 0;
#endif

#ifdef CONFIG_ARM64
#if CONFIG_AMLOGIC_KERNEL_VERSION <= 14515
	frame.fp = (unsigned long)__builtin_frame_address(1);
	frame.pc = (unsigned long)__builtin_return_address(0);
	bitmap_zero(frame.stacks_done, __NR_STACK_TYPES);
	frame.prev_fp = 0;
	frame.prev_type = STACK_TYPE_UNKNOWN;
#endif

#if CONFIG_AMLOGIC_KERNEL_VERSION == 14515
	frame.task = current;
#else
#if defined(CONFIG_FUNCTION_GRAPH_TRACER) && IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
	frame.graph = 0;
#endif
#endif
#else
	frame.fp = (unsigned long)__builtin_frame_address(0);
	frame.sp = current_stack_pointer;
	frame.lr = (unsigned long)__builtin_return_address(0);
	frame.pc = (unsigned long)find_back_trace;
#ifdef CONFIG_KRETPROBES
	frame.tsk = current;
#endif
#endif

#if (CONFIG_AMLOGIC_KERNEL_VERSION >= 14515) && defined(CONFIG_ARM64)
	return aml_arch_stack_walk(aml_dump_backtrace_entry, arg, current, NULL);
#else
	while (1) {
	#ifdef CONFIG_ARM64
		ret = aml_unwind_frame(current, &frame);
	#elif defined(CONFIG_ARM)
		ret = unwind_frame(&frame);
	#else	/* not supported */
		ret = -1;
	#endif
		if (ret < 0)
			return 0;
	#ifdef CONFIG_ARM64
		frame.pc = ptrauth_strip_kernel_insn_pac(frame.pc);
	#endif
		step++;
		if (!is_common_caller(common_caller, frame.pc, arg) && step > 1)
			return frame.pc;
	}
#endif
}

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
EXPORT_SYMBOL(find_back_trace);
#endif

static struct pglist_data *aml_first_online_pgdat(void)
{
	return NODE_DATA(first_online_node);
}

static struct pglist_data *aml_next_online_pgdat(struct pglist_data *pgdat)
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
static struct zone *aml_next_zone(struct zone *zone)
{
	pg_data_t *pgdat = zone->zone_pgdat;

	if (zone < pgdat->node_zones + MAX_NR_ZONES - 1) {
		zone++;
	} else {
		pgdat = aml_next_online_pgdat(pgdat);
		if (pgdat)
			zone = pgdat->node_zones;
		else
			zone = NULL;
	}
	return zone;
}

#define aml_for_each_populated_zone(zone)			\
	for (zone = (aml_first_online_pgdat())->node_zones;	\
	     zone;						\
	     zone = aml_next_zone(zone))			\
		if (!populated_zone(zone))			\
			; /* do nothing */			\
		else

struct page_trace *find_page_base(struct page *page)
{
	unsigned long pfn, zone_offset = 0, offset;
	struct zone *zone;
	struct page_trace *p;

	if (!aml_trace_buffer)
		return NULL;

	pfn = page_to_pfn(page);
	aml_for_each_populated_zone(zone) {
		/* pfn is in this zone */
		if (pfn <= zone_end_pfn(zone) &&
		    pfn >= zone->zone_start_pfn) {
			offset = pfn - zone->zone_start_pfn;
			p = aml_trace_buffer;
			return p + ((offset + zone_offset) * aml_trace_step);
		}
		/* next zone */
		zone_offset += zone->spanned_pages;
	}
	return NULL;
}

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
EXPORT_SYMBOL(find_page_base);
#endif

unsigned long get_page_trace(struct page *page)
{
	struct page_trace *trace;

	trace = find_page_base(page);
	if (trace)
		return unpack_ip(trace);

	return 0;
}

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
EXPORT_SYMBOL(get_page_trace);
#endif

/* Convert GFP flags to their corresponding migrate type */
#define AML_GFP_MOVABLE_MASK (__GFP_RECLAIMABLE | __GFP_MOVABLE)
#define AML_GFP_MOVABLE_SHIFT 3

static int aml_gfp_migratetype(const gfp_t gfp_flags)
{
	VM_WARN_ON((gfp_flags & AML_GFP_MOVABLE_MASK) == AML_GFP_MOVABLE_MASK);
	BUILD_BUG_ON((1UL << AML_GFP_MOVABLE_SHIFT) != ___GFP_MOVABLE);
	BUILD_BUG_ON((___GFP_MOVABLE >> AML_GFP_MOVABLE_SHIFT) != MIGRATE_MOVABLE);

#ifdef CONFIG_ARM64
	if ((gfp_flags & GFP_ATOMIC) == GFP_ATOMIC)
		return MIGRATE_HIGHATOMIC;
#endif

	/* Group based on mobility */
	return (gfp_flags & AML_GFP_MOVABLE_MASK) >> AML_GFP_MOVABLE_SHIFT;
}

static int before_pagetrace_memory(void)
{
	return 0;
}

static void __init set_init_page_trace(struct page *page, unsigned int order, gfp_t flag)
{
	unsigned long text;
	unsigned long ip;
	struct page_trace trace = {0}, *base;

	if (page && aml_trace_buffer) {
		ip = (unsigned long)before_pagetrace_memory;
		text = PAGE_TRACE_OFFSET;

#ifdef CONFIG_ARM
		trace.ret_ip = (ip - text) >> 3;
#else
		trace.ret_ip = (ip - text) >> 2;
#endif
		trace.migrate_type = aml_gfp_migratetype(flag);
		trace.order = order;
		base = find_page_base(page);
		push_ip(base, &trace);
	}
}

#ifdef CONFIG_ARM
static inline int is_module_addr(unsigned long ip)
{
	if (ip >= MODULES_VADDR && ip < MODULES_END)
		return 1;
	return 0;
}
#endif

unsigned long pack_ip(unsigned long ip, unsigned int order, gfp_t flag, int vflag)
{
	unsigned long text = PAGE_TRACE_OFFSET;
	struct page_trace trace = {0};

#ifdef CONFIG_ARM
	if (is_module_addr(ip)) {
		text = MODULES_VADDR;
		trace.module_flag = 1;
	}

	trace.ret_ip = (ip - text) >> 3;
#else
	trace.ret_ip = (ip - text) >> 2;
#endif
	if (flag == __GFP_NO_CMA)
		trace.migrate_type = MIGRATE_CMA;
	else
		trace.migrate_type = aml_gfp_migratetype(flag);
	trace.order = order;
	trace.filter_flag = vflag;
#if DEBUG_PAGE_TRACE
	pr_info("%s, text: %lx, ip:%lx, o: %d, f: %x, ip:%lx, %ps\n", __func__,
	       text, (*((unsigned long *)&trace)), order, flag, ip, (void *)ip);
#endif
	return *((unsigned long *)&trace);
}

void set_page_trace(struct page *page, unsigned int order, gfp_t flag, void *func)
{
	unsigned long ip, val = 0;
	struct page_trace *base;
	unsigned int i;
	int vflag = 0;

	if (!page)
		return;
	if (!aml_trace_buffer)
		return;

	ip = PAGE_TRACE_OFFSET;
	if (!func)
		ip = find_back_trace(&vflag);
	else
		ip = (unsigned long)func;

	if (!ip) {
	#if !defined(CONFIG_ARM64)
		pr_debug("can't find backtrace for page:%lx\n",
			 page_to_pfn(page));
	#endif
		return;
	}

#if defined(CONFIG_ANDROID_VENDOR_HOOKS) && \
	IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
	trace_param.trace_buf = aml_trace_buffer;
	trace_param.trace_step = aml_trace_step;
	trace_android_vh_get_page_wmark(1024, (unsigned long *)&trace_param);
#endif

	val = pack_ip(ip, order, flag, vflag);
	base = find_page_base(page);
	push_ip(base, (struct page_trace *)&val);
	if (order) {
		/* in order to easy get trace for high order alloc */
		val = pack_ip(ip, 0, flag, vflag);
		for (i = 1; i < (1 << order); i++) {
			base += (aml_trace_step);
			push_ip(base, (struct page_trace *)&val);
		}
	}
}

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
EXPORT_SYMBOL(set_page_trace);
#endif

void reset_page_trace(struct page *page, unsigned int order)
{
	struct page_trace *base;
	int i, cnt;
#if DEBUG_PAGE_TRACE
	unsigned long end;
#endif

	if (page && aml_trace_buffer) {
		base = find_page_base(page);
		cnt = 1 << order;
	#if DEBUG_PAGE_TRACE
		check_trace_valid(base);
		end = ((unsigned long)aml_trace_buffer + ptrace_size);
		WARN_ON((unsigned long)(base + cnt * aml_trace_step - 1) >= end);
	#endif
		for (i = 0; i < cnt; i++) {
			base->order = IP_INVALID;
			base += (aml_trace_step);
		}
	}
}

void replace_page_trace(struct page *new, struct page *old)
{
	struct page_trace *old_trace, *new_trace;

	if (!new || !old)
		return;
	if (!aml_trace_buffer)
		return;

	old_trace  = find_page_base(old);
	new_trace  = find_page_base(new);
	*new_trace = *old_trace;
}

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
EXPORT_SYMBOL(replace_page_trace);
#endif

/*
 * move page out of buddy and make sure they are not malloced by
 * other module
 *
 * Note:
 * before call this functions, memory for page trace buffer are
 * freed to buddy.
 */
static int __init page_trace_pre_work(unsigned long size)
{
	struct page *page;
	void *paddr;
	void *pend;
	unsigned long maddr = PAGE_TRACE_OFFSET;

#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
	paddr = memblock_alloc_low(size, PAGE_SIZE);
	if (!paddr) {
		panic("%s: Failed to allocate %lu bytes.\n", __func__, size);
		return -ENOMEM;
	}
#else
	paddr = vzalloc(size);
	if (!paddr)
		return -ENOMEM;
#endif

	aml_trace_buffer = (struct page_trace *)paddr;
	pend = paddr + size;
	pr_info("%s, trace buffer:%px, size:%lx, end:%px, module: %lx\n",
			__func__, aml_trace_buffer, size, pend, maddr);
	memset(paddr, 0, size);

	for (; paddr < pend; paddr += PAGE_SIZE) {
#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
		page = virt_to_page(paddr);
#else
		page = vmalloc_to_page(paddr);
#endif
		set_init_page_trace(page, 0, GFP_KERNEL);
	#if DEBUG_PAGE_TRACE
		pr_info("%s, trace page:%p, %lx\n",
				__func__, page, page_to_pfn(page));
	#endif
	}
	return 0;
}

/*--------------------------sysfs node -------------------------------*/
#define LARGE	1024
#define SMALL	256

/* caller for unmovable are max */
#define MT_UNMOVABLE_IDX	0                            /* 0,UNMOVABLE   */
#define MT_MOVABLE_IDX		(MT_UNMOVABLE_IDX   + LARGE) /* 1,MOVABLE     */
#define MT_RECLAIMABLE_IDX	(MT_MOVABLE_IDX     + SMALL) /* 2,RECLAIMABLE */
#define MT_HIGHATOMIC_IDX	(MT_RECLAIMABLE_IDX + SMALL) /* 3,HIGHATOMIC  */
#define MT_CMA_IDX		(MT_HIGHATOMIC_IDX  + SMALL) /* 4,CMA         */
#define MT_ISOLATE_IDX		(MT_CMA_IDX         + SMALL) /* 5,ISOLATE     */

#define SHOW_CNT		(MT_ISOLATE_IDX)

static int mt_offset[] = {
	MT_UNMOVABLE_IDX,
	MT_MOVABLE_IDX,
	MT_RECLAIMABLE_IDX,
	MT_HIGHATOMIC_IDX,
	MT_CMA_IDX,
	MT_ISOLATE_IDX,
	MT_ISOLATE_IDX + SMALL
};

struct page_summary {
	struct rb_node entry;
	unsigned long ip;
	unsigned long cnt;
};

struct pagetrace_summary {
	struct page_summary *sum;
	unsigned long ticks;
	int mt_cnt[MIGRATE_TYPES];
	int filter_slab[MIGRATE_TYPES];
	int filter_vmalloc[MIGRATE_TYPES];
	int filter_large_slab[MIGRATE_TYPES];
};

static unsigned long __nocfi find_ip_base(unsigned long ip)
{
	unsigned long size, offset;
	int ret;

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
	ret = aml_kallsyms_lookup_size_offset(ip, &size, &offset);
#else
	ret = kallsyms_lookup_size_offset(ip, &size, &offset);
#endif
	if (ret)	/* find */
		return ip - offset;
	else		/* not find */
		return ip;
}

static int find_page_ip(struct page_trace *trace, struct page_summary *sum,
			int range, int *mt_cnt, struct rb_root *root)
{
	unsigned long ip;
	struct rb_node **link, *parent = NULL;
	struct page_summary *tmp;

	ip = unpack_ip(trace);
	if (!ip || !trace->ip_data)	/* invalid ip */
		return 0;

	link  = &root->rb_node;
	while (*link) {
		parent = *link;
		tmp = rb_entry(parent, struct page_summary, entry);
		if (ip == tmp->ip) { /* match */
			tmp->cnt++;
			return 0;
		} else if (ip < tmp->ip) {
			link = &tmp->entry.rb_left;
		} else {
			link = &tmp->entry.rb_right;
		}
	}
	/* not found, get a new page summary */
	if (mt_cnt[trace->migrate_type] >= range)
		return -ERANGE;
	tmp       = &sum[mt_cnt[trace->migrate_type]];
	tmp->ip   = ip;
	tmp->cnt++;
	mt_cnt[trace->migrate_type]++;
	rb_link_node(&tmp->entry, parent, link);
	rb_insert_color(&tmp->entry, root);
	return 0;
}

#define K(x)		((unsigned long)(x) << (PAGE_SHIFT - 10))
static int trace_cmp(const void *x1, const void *x2)
{
	struct page_summary *s1, *s2;

	s1 = (struct page_summary *)x1;
	s2 = (struct page_summary *)x2;
	return s2->cnt - s1->cnt;
}

static void show_page_trace(struct seq_file *m, struct zone *zone,
			    struct pagetrace_summary *pt_sum)
{
	int i, j;
	struct page_summary *p;
	unsigned long total_mt, total_used = 0;
	struct page_summary *sum = pt_sum->sum;
	int *mt_cnt = pt_sum->mt_cnt;

	seq_printf(m, "%s            %s, %s\n",
		   "count(KB)", "kaddr", "function");
	seq_puts(m, "------------------------------\n");
	for (j = 0; j < MIGRATE_TYPES; j++) {
		if (!mt_cnt[j])	/* this migrate type is empty */
			continue;

		p = sum + mt_offset[j];
		sort(p, mt_cnt[j], sizeof(*p), trace_cmp, NULL);

		total_mt = 0;
		for (i = 0; i < mt_cnt[j]; i++) {
			if (!p[i].cnt)	/* may be empty after merge */
				continue;

			if (K(p[i].cnt) >= page_trace_filter) {
				seq_printf(m, "%8ld, %16lx, %ps\n",
					   K(p[i].cnt), p[i].ip,
					   (void *)p[i].ip);
			}
			total_mt += p[i].cnt;
		}
		if (!total_mt)
			continue;
		seq_puts(m, "------------------------------\n");
		seq_printf(m, "total pages:%6ld, %9ld kB, type:%s\n",
			   total_mt, K(total_mt), aml_migratetype_names[j]);
		if (page_trace_filter_slab)
			seq_printf(m, "filter_slab pages:%6d, %9ld kB\n",
				pt_sum->filter_slab[j], K(pt_sum->filter_slab[j]));
		if (page_trace_filter_vmalloc)
			seq_printf(m, "filter_vmalloc pages:%6d, %9ld kB\n",
				pt_sum->filter_vmalloc[j], K(pt_sum->filter_vmalloc[j]));
		if (page_trace_filter_large_slab)
			seq_printf(m, "filter_large_slab pages:%6d, %9ld kB\n",
				pt_sum->filter_large_slab[j], K(pt_sum->filter_large_slab[j]));
		seq_puts(m, "------------------------------\n");
		total_used += total_mt;
	}
	seq_printf(m, "Zone %8s, managed:%6ld KB, free:%6ld kB, used:%6ld KB\n",
		   zone->name, K(atomic_long_read(&zone->managed_pages)),
		   K(zone_page_state(zone, NR_FREE_PAGES)), K(total_used));
	seq_puts(m, "------------------------------\n");
}

static int _merge_same_function(struct page_summary *p, int range)
{
	int j, k, real_used;

	/* first, replace all ip to entry of each function */
	for (j = 0; j < range; j++) {
		if (!p[j].ip)	/* Not used */
			break;
		p[j].ip = find_ip_base(p[j].ip);
	}

	real_used = j;
	/* second, loop and merge same ip */
	for (j = 0; j < real_used; j++) {
		for (k = j + 1; k < real_used; k++) {
			if (p[k].ip != (-1ul) &&
			    p[k].ip == p[j].ip) {
				p[j].cnt += p[k].cnt;
				p[k].ip  = (-1ul);
				p[k].cnt = 0;
			}
		}
	}
	return real_used;
}

static void merge_same_function(struct page_summary *sum, int *mt_cnt)
{
	int i, range;
	struct page_summary *p;

	for (i = 0; i < MIGRATE_TYPES; i++) {
		range = mt_cnt[i];
		p = sum + mt_offset[i];
		_merge_same_function(p, range);
	}
}

static int update_page_trace(struct seq_file *m, struct zone *zone,
			     struct pagetrace_summary *pt_sum)
{
	unsigned long pfn;
	unsigned long start_pfn = zone->zone_start_pfn;
	unsigned long end_pfn = start_pfn + zone->present_pages;
	int    ret = 0, mt = 0;
	struct page_trace *trace;
	struct page_summary *p;
	struct rb_root root[MIGRATE_TYPES];
	struct page_summary *sum = pt_sum->sum;
	int *mt_cnt = pt_sum->mt_cnt;

	memset(root, 0, sizeof(root));
	/* loop whole zone */
	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		struct page *page;

#ifdef CONFIG_ARM64
		if (!pfn_is_map_memory(pfn))
#else
		if (!pfn_valid(pfn))
#endif
			continue;

		page = pfn_to_page(pfn);

		trace = find_page_base(page);
	#if DEBUG_PAGE_TRACE
		check_trace_valid(trace);
	#endif
		if (page_trace_invalid(trace)) /* free pages */
			continue;

		if (!(*(unsigned int *)trace)) /* empty */
			continue;

		if (page_trace_filter_slab && PageSlab(page)) {
			pt_sum->filter_slab[mt]++;
			continue;
		}

		if (page_trace_filter_vmalloc && trace->filter_flag == 1) {
			pt_sum->filter_vmalloc[mt]++;
			continue;
		}

		if (page_trace_filter_large_slab && trace->filter_flag == 2) {
			pt_sum->filter_large_slab[mt]++;
			continue;
		}

		mt = trace->migrate_type;
		p  = sum + mt_offset[mt];
		ret = find_page_ip(trace, p, mt_offset[mt + 1] - mt_offset[mt],
				   mt_cnt, &root[mt]);
		if (ret) {
			pr_err("mt type:%d, out of range:%d\n",
			       mt, mt_offset[mt + 1] - mt_offset[mt]);
			break;
		}
	}
	if (merge_function)
		merge_same_function(sum, mt_cnt);
	return ret;
}

static int pagetrace_show(struct seq_file *m, void *arg)
{
	struct zone *zone;
	int ret, size = sizeof(struct page_summary) * SHOW_CNT;
	struct pagetrace_summary *sum;

	if (!aml_trace_buffer) {
		seq_puts(m, "page trace not enabled\n");
		return 0;
	}
	sum = kzalloc(sizeof(*sum), GFP_KERNEL);
	if (!sum)
		return -ENOMEM;

	m->private = sum;
	sum->sum = vzalloc(size);
	if (!sum->sum) {
		kfree(sum);
		m->private = NULL;
		return -ENOMEM;
	}

	/* update only once */
	seq_puts(m, "==============================\n");
	sum->ticks = sched_clock();
	aml_for_each_populated_zone(zone) {
		memset(sum->sum, 0, size);
		memset(sum->mt_cnt, 0, sizeof(int) * MIGRATE_TYPES);
		ret = update_page_trace(m, zone, sum);
		if (ret) {
			seq_printf(m, "Error %d in zone %8s\n",
				   ret, zone->name);
			continue;
		}
		show_page_trace(m, zone, sum);
	}
	sum->ticks = sched_clock() - sum->ticks;

	seq_printf(m, "SHOW_CNT:%d, buffer size:%d, tick:%ld ns\n",
		   SHOW_CNT, size, sum->ticks);
	seq_puts(m, "==============================\n");

	vfree(sum->sum);
	kfree(sum);

	return 0;
}

static int pagetrace_open(struct inode *inode, struct file *file)
{
	return single_open(file, pagetrace_show, NULL);
}

static ssize_t pagetrace_write(struct file *file, const char __user *buffer,
			       size_t count, loff_t *ppos)
{
	char *buf;
	unsigned long arg = 0;

	buf = kmalloc(count, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	if (copy_from_user(buf, buffer, count)) {
		kfree(buf);
		return -EINVAL;
	}

	if (!strncmp(buf, "merge=", 6)) {	/* option for 'merge=' */
		if (sscanf(buf, "merge=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		merge_function = arg ? 1 : 0;
		pr_info("set merge_function to %d\n", merge_function);
	}

	if (!strncmp(buf, "filter_slab=", 12)) {	/* option for 'filter_slab=' */
		if (sscanf(buf, "filter_slab=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		page_trace_filter_slab = arg;
		pr_info("set filter_slab to %d\n", page_trace_filter_slab);
	}

	if (!strncmp(buf, "filter_vmalloc=", 15)) {	/* option for 'filter_vmalloc=' */
		if (sscanf(buf, "filter_vmalloc=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		page_trace_filter_vmalloc = arg;
		pr_info("set filter_vmalloc to %d\n", page_trace_filter_vmalloc);
	}

	if (!strncmp(buf, "filter_large_slab=", 18)) {	/* option for 'filter_large_slab=' */
		if (sscanf(buf, "filter_large_slab=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		page_trace_filter_large_slab = arg;
		pr_info("set filter_large_slab to %d\n", page_trace_filter_large_slab);
	}

	if (!strncmp(buf, "filter=", 7)) {	/* option for 'filter=' */
		if (sscanf(buf, "filter=%ld", &arg) < 0) {
			kfree(buf);
			return -EINVAL;
		}
		page_trace_filter = arg;
		pr_info("set filter to %d KB\n", page_trace_filter);
	}

	if (!strncmp(buf, "show_caller", 11))
		dump_common_caller(COMMON_CALLER_SIZE);

	kfree(buf);

	return count;
}

static const struct proc_ops pagetrace_proc_ops = {
	.proc_open	= pagetrace_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_write	= pagetrace_write,
	.proc_release	= single_release,
};

static int profile_default_cma_show(struct seq_file *m, void *arg)
{
	int ret, size = sizeof(struct page_summary) * SHOW_CNT;
	struct pagetrace_summary *sum;
	struct zone *tmp_zone;
	struct cma *cma = dma_contiguous_default_area;

	if (!aml_trace_buffer) {
		seq_puts(m, "page trace not enabled\n");
		return 0;
	}
	sum = kzalloc(sizeof(*sum), GFP_KERNEL);
	if (!sum)
		return -ENOMEM;
	tmp_zone = kzalloc(sizeof(*tmp_zone), GFP_KERNEL);
	if (!tmp_zone) {
		kfree(sum);
		return -ENOMEM;
	}

	m->private = sum;
	sum->sum = vzalloc(size);
	if (!sum->sum) {
		kfree(sum);
		kfree(tmp_zone);
		m->private = NULL;
		return -ENOMEM;
	}

	/* update only once */
	seq_puts(m, "==============================\n");
	sum->ticks = sched_clock();
	memset(sum->sum, 0, size);
	memset(sum->mt_cnt, 0, sizeof(int) * MIGRATE_TYPES);
	tmp_zone->zone_start_pfn = cma->base_pfn;
	tmp_zone->present_pages = cma->count;
	ret = update_page_trace(m, tmp_zone, sum);
	if (ret)
		seq_printf(m, "Error %d.\n", ret);
	show_page_trace(m, tmp_zone, sum);
	sum->ticks = sched_clock() - sum->ticks;

	seq_printf(m, "SHOW_CNT:%d, buffer size:%d, tick:%ld ns\n",
			SHOW_CNT, size, sum->ticks);
	seq_puts(m, "==============================\n");

	vfree(sum->sum);
	kfree(sum);
	kfree(tmp_zone);

	return 0;
}

static int profile_default_cma_open(struct inode *inode, struct file *file)
{
	return single_open(file, profile_default_cma_show, NULL);
}

static const struct proc_ops profile_default_cma_proc_ops = {
	.proc_open	= profile_default_cma_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

char *hd_filter_list[] = {
	"before_pagetrace_memory",
	"_pte_",
	"_pmd_",
	"pud_alloc",
	"pgd_alloc",
	"f2fs_",
	"do_fault",
	"get_node_folio",
	"cache_folio",
	"_cgroup_",
	"binder_alloc",
	"tlb_remove_table",
};

int skip_list_f(char *buf, unsigned long ip)
{
	int i = 0;
	char *ptr = NULL;

	memset(buf, 0, 128);
	sprintf(buf, "%ps", (void *)ip);

	for (i = 0; i < ARRAY_SIZE(hd_filter_list); i++) {
		ptr = hd_filter_list[i];
		if (strstr(buf, ptr))
			return 1;
	}

	return 0;
}

static void show_hardware(struct seq_file *m, struct zone *zone,
			    struct pagetrace_summary *pt_sum)
{
	int i, j;
	struct page_summary *p;
	unsigned long total_mt, total_used = 0;
	struct page_summary *sum = pt_sum->sum;
	int *mt_cnt = pt_sum->mt_cnt;
	char *filter_buf = kzalloc(128, GFP_KERNEL);

	seq_printf(m, "%s            %s, %s\n",
		   "count(KB)", "kaddr", "function");
	seq_puts(m, "------------------------------\n");
	for (j = 0; j < MIGRATE_TYPES; j++) {
		if (j != MIGRATE_UNMOVABLE && j != MIGRATE_CMA)
			continue;
		if (!mt_cnt[j])	/* this migrate type is empty */
			continue;

		p = sum + mt_offset[j];
		sort(p, mt_cnt[j], sizeof(*p), trace_cmp, NULL);

		total_mt = 0;
		for (i = 0; i < mt_cnt[j]; i++) {
			if (!p[i].cnt)	/* may be empty after merge */
				continue;

			if (j == MIGRATE_UNMOVABLE && skip_list_f(filter_buf, p[i].ip))
				continue;

			if (K(p[i].cnt) >= page_trace_filter) {
				seq_printf(m, "%8ld, %16lx, %ps\n",
					   K(p[i].cnt), p[i].ip,
					   (void *)p[i].ip);
			}
			total_mt += p[i].cnt;
		}
		if (!total_mt)
			continue;
		seq_puts(m, "------------------------------\n");
		seq_printf(m, "total pages:%6ld, %9ld kB, type:%s\n",
			   total_mt, K(total_mt), aml_migratetype_names[j]);
		seq_puts(m, "------------------------------\n");
		total_used += total_mt;
	}
	seq_printf(m, "Zone %8s, managed:%6ld KB, free:%6ld kB, used:%6ld KB\n",
		   zone->name, K(atomic_long_read(&zone->managed_pages)),
		   K(zone_page_state(zone, NR_FREE_PAGES)), K(total_used));
	seq_puts(m, "------------------------------\n");
	kfree(filter_buf);
}

static int hardware_show(struct seq_file *m, void *arg)
{
	struct zone *zone;
	int ret, size = sizeof(struct page_summary) * SHOW_CNT;
	struct pagetrace_summary *sum;
	int old_slab_filter = page_trace_filter_slab;
	int old_vmalloc_filter = page_trace_filter_vmalloc;
	int old_large_slab_filter = page_trace_filter_large_slab;

	if (!aml_trace_buffer) {
		seq_puts(m, "page trace not enabled\n");
		return 0;
	}
	sum = kzalloc(sizeof(*sum), GFP_KERNEL);
	if (!sum)
		return -ENOMEM;

	m->private = sum;
	sum->sum = vzalloc(size);
	if (!sum->sum) {
		kfree(sum);
		m->private = NULL;
		return -ENOMEM;
	}

	page_trace_filter_slab = 1;
	page_trace_filter_vmalloc = 1;
	page_trace_filter_large_slab = 1;
	/* update only once */
	seq_puts(m, "==============================\n");
	sum->ticks = sched_clock();
	aml_for_each_populated_zone(zone) {
		memset(sum->sum, 0, size);
		memset(sum->mt_cnt, 0, sizeof(int) * MIGRATE_TYPES);
		ret = update_page_trace(m, zone, sum);
		if (ret) {
			seq_printf(m, "Error %d in zone %8s\n",
				   ret, zone->name);
			continue;
		}
		show_hardware(m, zone, sum);
	}
	sum->ticks = sched_clock() - sum->ticks;

	seq_printf(m, "SHOW_CNT:%d, buffer size:%d, tick:%ld ns\n",
		   SHOW_CNT, size, sum->ticks);
	seq_puts(m, "==============================\n");

	vfree(sum->sum);
	kfree(sum);
	page_trace_filter_slab = old_slab_filter;
	page_trace_filter_vmalloc = old_vmalloc_filter;
	page_trace_filter_large_slab = old_large_slab_filter;

	return 0;
}

static int hardware_open(struct inode *inode, struct file *file)
{
	return single_open(file, hardware_show, NULL);
}

static const struct proc_ops hardware_proc_ops = {
	.proc_open	= hardware_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

static int update_large_slab(struct seq_file *m, struct zone *zone,
			     struct pagetrace_summary *pt_sum)
{
	unsigned long pfn;
	unsigned long start_pfn = zone->zone_start_pfn;
	unsigned long end_pfn = start_pfn + zone->present_pages;
	int    ret = 0, mt = 0;
	struct page_trace *trace;
	struct page_summary *p;
	struct rb_root root[MIGRATE_TYPES];
	struct page_summary *sum = pt_sum->sum;
	int *mt_cnt = pt_sum->mt_cnt;

	memset(root, 0, sizeof(root));
	/* loop whole zone */
	for (pfn = start_pfn; pfn < end_pfn; pfn++) {
		struct page *page;

#ifdef CONFIG_ARM64
		if (!pfn_is_map_memory(pfn))
#else
		if (!pfn_valid(pfn))
#endif
			continue;

		page = pfn_to_page(pfn);

		trace = find_page_base(page);
	#if DEBUG_PAGE_TRACE
		check_trace_valid(trace);
	#endif
		if (page_trace_invalid(trace)) /* free pages */
			continue;

		if (!(*(unsigned int *)trace)) /* empty */
			continue;

		if (trace->filter_flag != 2)
			continue;

		mt = trace->migrate_type;
		p  = sum + mt_offset[mt];
		ret = find_page_ip(trace, p, mt_offset[mt + 1] - mt_offset[mt],
				   mt_cnt, &root[mt]);
		if (ret) {
			pr_err("mt type:%d, out of range:%d\n",
			       mt, mt_offset[mt + 1] - mt_offset[mt]);
			break;
		}
	}
	if (merge_function)
		merge_same_function(sum, mt_cnt);
	return ret;
}

static int large_slab_show(struct seq_file *m, void *arg)
{
	struct zone *zone;
	int ret, size = sizeof(struct page_summary) * SHOW_CNT;
	struct pagetrace_summary *sum;

	if (!aml_trace_buffer) {
		seq_puts(m, "page trace not enabled\n");
		return 0;
	}
	sum = kzalloc(sizeof(*sum), GFP_KERNEL);
	if (!sum)
		return -ENOMEM;

	m->private = sum;
	sum->sum = vzalloc(size);
	if (!sum->sum) {
		kfree(sum);
		m->private = NULL;
		return -ENOMEM;
	}

	/* update only once */
	seq_puts(m, "==============================\n");
	sum->ticks = sched_clock();
	aml_for_each_populated_zone(zone) {
		memset(sum->sum, 0, size);
		memset(sum->mt_cnt, 0, sizeof(int) * MIGRATE_TYPES);
		ret = update_large_slab(m, zone, sum);
		if (ret) {
			seq_printf(m, "Error %d in zone %8s\n",
				   ret, zone->name);
			continue;
		}
		show_page_trace(m, zone, sum);
	}
	sum->ticks = sched_clock() - sum->ticks;

	seq_printf(m, "SHOW_CNT:%d, buffer size:%d, tick:%ld ns\n",
		   SHOW_CNT, size, sum->ticks);
	seq_puts(m, "==============================\n");

	vfree(sum->sum);
	kfree(sum);

	return 0;
}

static int large_slab_open(struct inode *inode, struct file *file)
{
	return single_open(file, large_slab_show, NULL);
}

static const struct proc_ops large_slab_proc_ops = {
	.proc_open	= large_slab_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
struct page_summary *dump_sum;

static char sym_show_mem[32] = "__show_mem";

static struct kprobe kp_oom = {
		.symbol_name	= sym_show_mem,
};

static void show_page_trace2(struct zone *zone,
		struct pagetrace_summary *pt_sum)
{
	int i, j;
	struct page_summary *p;
	unsigned long total_mt, total_used = 0;
	struct page_summary *sum = pt_sum->sum;
	int *mt_cnt = pt_sum->mt_cnt;

	pr_info("%s            %s, %s\n",
			"count(KB)", "kaddr", "function");
	pr_info("------------------------------\n");
	for (j = 0; j < MIGRATE_TYPES; j++) {
		if (!mt_cnt[j])	/* this migrate type is empty */
			continue;

		p = sum + mt_offset[j];
		sort(p, mt_cnt[j], sizeof(*p), trace_cmp, NULL);

		total_mt = 0;
		for (i = 0; i < mt_cnt[j]; i++) {
			if (!p[i].cnt)	/* may be empty after merge */
				continue;

			if (K(p[i].cnt) >= 1024) {
				pr_info("%8ld, %16lx, %ps\n",
						K(p[i].cnt), p[i].ip,
						(void *)p[i].ip);
			}
			total_mt += p[i].cnt;
		}
		if (!total_mt)
			continue;
		pr_info("------------------------------\n");
		pr_info("total pages:%6ld, %9ld kB, type:%s\n",
				total_mt, K(total_mt), aml_migratetype_names[j]);
		if (page_trace_filter_slab)
			pr_info("filter_slab pages:%6d, %9ld kB\n",
					pt_sum->filter_slab[j], K(pt_sum->filter_slab[j]));
		if (page_trace_filter_vmalloc)
			pr_info("filter_vmalloc pages:%6d, %9ld kB\n",
					pt_sum->filter_vmalloc[j], K(pt_sum->filter_vmalloc[j]));
		if (page_trace_filter_large_slab)
			pr_info("filter_large_slab pages:%6d, %9ld kB\n",
				pt_sum->filter_large_slab[j], K(pt_sum->filter_large_slab[j]));
		pr_info("------------------------------\n");
		total_used += total_mt;
	}
	pr_info("Zone %8s, managed:%6ld KB, free:%6ld kB, used:%6ld KB\n",
			zone->name, K(atomic_long_read(&zone->managed_pages)),
			K(zone_page_state(zone, NR_FREE_PAGES)), K(total_used));
	pr_info("------------------------------\n");
}

/*
 * static void __maybe_unused oom_panic_callback(void *data, struct oom_control *oc, int *retc)
 */
static void __kprobes oom_panic_callback(struct kprobe *p,
				struct pt_regs *regs, unsigned long flags)
{
	struct zone *zone;
	int ret, size = sizeof(struct page_summary) * SHOW_CNT;
	struct pagetrace_summary *sum;
	/*
	 *struct oom_control oc = {
	 *	.gfp_mask = GFP_KERNEL,
	 *};
	 */

	if (!aml_trace_buffer) {
		pr_info("page trace not enabled\n");
		return;
	}

	sum = kzalloc(sizeof(*sum), GFP_KERNEL);
	if (!sum)
		return;

	sum->sum = dump_sum;
	if (!sum->sum) {
		kfree(sum);
		return;
	}

	/* update only once */
	pr_info("==============================\n");
	sum->ticks = sched_clock();
	aml_for_each_populated_zone(zone) {
		memset(sum->sum, 0, size);
		memset(sum->mt_cnt, 0, sizeof(int) * MIGRATE_TYPES);
		ret = update_page_trace(NULL, zone, sum);
		if (ret) {
			pr_info("Error %d in zone %8s\n",
					ret, zone->name);
			continue;
		}
		show_page_trace2(zone, sum);
	}
	sum->ticks = sched_clock() - sum->ticks;

	pr_info("SHOW_CNT:%d, buffer size:%d, tick:%ld ns\n",
			SHOW_CNT, size, sum->ticks);
	pr_info("==============================\n");

	kfree(sum);

	/*
	 *dump_tasks(&oc);
	 */
}

static void *get_symbol_addr(const char *symbol_name)
{
	struct kprobe kp = {
		.symbol_name = symbol_name,
	};
	int ret;

	ret = register_kprobe(&kp);
	if (ret < 0) {
		pr_err("register_kprobe:%s failed, returned %d\n", symbol_name, ret);
		return NULL;
	}
	pr_debug("symbol_name:%s addr=%px\n", symbol_name, kp.addr);
	unregister_kprobe(&kp);

	return kp.addr;
}
#endif

static int __init page_trace_module_init(void)
{
#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
	int ret;
	int size = sizeof(struct page_summary) * SHOW_CNT;
#endif

	if (!page_trace_disable) {
		d_pagetrace = proc_create("pagetrace", 0444,
					  NULL, &pagetrace_proc_ops);
		d_profile_default_cma = proc_create("profile_default_cma", 0444,
					  NULL, &profile_default_cma_proc_ops);
	}
	if (IS_ERR_OR_NULL(d_pagetrace) || IS_ERR_OR_NULL(d_profile_default_cma)) {
		pr_err("%s, create pagetrace failed\n", __func__);
		return -1;
	}
	d_pagetrace = proc_create("mem_hardware", 0444,
				NULL, &hardware_proc_ops);

	d_pagetrace = proc_create("mem_large_slab", 0444,
				NULL, &large_slab_proc_ops);

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
	ret = register_kprobe(&kp_lookup_off);
	if (ret < 0) {
		pr_err("register_kprobe failed, returned %d\n", ret);
		return ret;
	}
	pr_debug("kprobe lookup offset at %px\n", kp_lookup_off.addr);

	aml_kallsyms_lookup_size_offset = (int (*)(unsigned long addr,
				unsigned long *symbolsize,
				unsigned long *offset))kp_lookup_off.addr;

	ret = register_kprobe(&kp_lookup_name);
	if (ret < 0) {
		pr_err("register_kprobe failed, returned %d\n", ret);
		return ret;
	}
	pr_debug("kprobe lookup offset at %px\n", kp_lookup_name.addr);

	aml_syms_lookup = (unsigned long (*)(const char *name))kp_lookup_name.addr;
	aml_kallsyms_on_each_symbol = (int (*)(int (*fn)(void *, const char *, unsigned long),
				void *data))get_symbol_addr("kallsyms_on_each_symbol");

#if (CONFIG_AMLOGIC_KERNEL_VERSION >= 14515) && !defined(CONFIG_ARM64)
	if (symbol_fix()) {
		pr_info("%s, %d\n", __func__, __LINE__);
		return -EINVAL;
	}
#endif

	page_trace_mem_init();

	dump_sum = vzalloc(size);

	kp_oom.post_handler = oom_panic_callback;
	ret = register_kprobe(&kp_oom);
	if (ret < 0)
		pr_err("register_kprobe failed, returned %d\n", ret);

	/*
	 * ret = register_trace_android_vh_oom_check_panic(oom_panic_callback, NULL);
	 * if (ret < 0)
	 *	pr_err("register_trace_android_vh_oom_check_page fail ret=%d\n", ret);
	 */
#endif

	if (!aml_trace_buffer)
		return -ENOMEM;

#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
	ret = register_trace_mm_page_alloc(page_alloc_callback, NULL);
	if (ret < 0)
		pr_err("register_trace_mm_page_alloc fail ret=%d\n", ret);

	ret = register_trace_mm_page_free(page_free_callback, NULL);
	if (ret < 0)
		pr_err("register_trace_mm_page_free fail ret=%d\n", ret);

	ret = register_trace_android_vh_look_around_migrate_folio(migrate_folio_callback, NULL);
	if (ret < 0)
		pr_err("register_trace migrate page fail ret=%d\n", ret);
#endif

	return 0;
}

static void __exit page_trace_module_exit(void)
{
	if (d_pagetrace)
		proc_remove(d_pagetrace);
	if (d_profile_default_cma)
		proc_remove(d_profile_default_cma);
#if IS_MODULE(CONFIG_AMLOGIC_PAGE_TRACE)
	unregister_kprobe(&kp_lookup_off);
	pr_debug("kprobe at %p unregistered\n", kp_lookup_off.addr);

	unregister_kprobe(&kp_lookup_name);
	pr_debug("kprobe at %p unregistered\n", kp_lookup_name.addr);

	if (!dump_sum)
		vfree(dump_sum);
#endif

	if (aml_trace_buffer)
		vfree(aml_trace_buffer);

}
module_init(page_trace_module_init);
module_exit(page_trace_module_exit);

#if IS_BUILTIN(CONFIG_AMLOGIC_PAGE_TRACE)
void __init page_trace_mem_init(void)
{
	struct zone *zone;
	unsigned long total_page = 0;

	find_static_common_symbol(NULL);
#ifdef CONFIG_KASAN	/* open multi_shot for kasan */
#if IS_ENABLED(CONFIG_KASAN_KUNIT_TEST) || IS_ENABLED(CONFIG_KASAN_MODULE_TEST)
	kasan_save_enable_multi_shot();
#endif
#endif
	if (page_trace_disable)
		return;

	aml_for_each_populated_zone(zone) {
		total_page += zone->spanned_pages;
		pr_info("zone:%s, spaned pages:%ld, total:%ld\n",
			zone->name, zone->spanned_pages, total_page);
	}
	ptrace_size = total_page * sizeof(struct page_trace) * aml_trace_step;
	ptrace_size = PAGE_ALIGN(ptrace_size);
	if (page_trace_pre_work(ptrace_size)) {
		aml_trace_buffer = NULL;
		ptrace_size = 0;
		pr_err("%s reserve memory failed\n", __func__);
		return;
	}
}
#else
static void mark_free_pages(struct zone *zone)
{
	unsigned long pfn;
	unsigned long flags;
	unsigned int order, t;
	struct page *page;
	unsigned int count = 0;

	if (zone_is_empty(zone))
		return;

	spin_lock_irqsave(&zone->lock, flags);
	for_each_migratetype_order(order, t) {
		list_for_each_entry(page,
				&zone->free_area[order].free_list[t], lru) {
			pfn = page_to_pfn(page);
			bitmap_set(free_pages_bitmap, pfn, (1UL << order));
			count += (1UL << order);
		}
	}
	spin_unlock_irqrestore(&zone->lock, flags);
	pr_debug("free pages: %dkb\n", count * 4);
}

static int __init statistic_before_insmod_mem(void)
{
	struct zone *zone;
	struct page *page;
	unsigned long start_pfn, end_pfn, pfn;
	unsigned int count = 0;

	aml_for_each_populated_zone(zone) {
		mark_free_pages(zone);

		start_pfn = zone->zone_start_pfn;
		end_pfn = start_pfn + zone->present_pages;
		for (pfn = start_pfn; pfn < end_pfn; pfn++) {
#ifdef CONFIG_ARM64
			if (!pfn_is_map_memory(pfn))
#else
			if (!pfn_valid(pfn))
#endif
				continue;

			page = pfn_to_page(pfn);
			if (!test_bit(pfn, free_pages_bitmap)) {
				set_init_page_trace(page, 0, GFP_KERNEL);
				count++;
			}
		}
	}
	pr_debug("before insmod pagetrace used size: %dKb\n", count * 4);

	kfree(free_pages_bitmap);

	return 0;
}

void __init page_trace_mem_init(void)
{
	struct zone *zone;
	unsigned long total_page = 0;

	if (page_trace_disable)
		return;

	memset(common_caller, 0, sizeof(common_caller));
#ifdef CONFIG_MODULES
	register_module_notifier(&track_modules_nb);
#endif

	/* find_static_common_symbol(); */
	kthread_run(find_static_common_symbol, NULL, "PAGETRACE_TASK");

	aml_for_each_populated_zone(zone) {
		total_page += zone->spanned_pages;
		pr_info("zone:%s, spaned pages:%ld, total:%ld\n",
			zone->name, zone->spanned_pages, total_page);
	}
	ptrace_size = total_page * sizeof(struct page_trace) * aml_trace_step;
	ptrace_size = PAGE_ALIGN(ptrace_size);
	if (page_trace_pre_work(ptrace_size)) {
		aml_trace_buffer = NULL;
		ptrace_size = 0;
		pr_err("%s reserve memory failed\n", __func__);
		return;
	}

	free_pages_bitmap = kzalloc(total_page / 8, GFP_KERNEL);
	if (!free_pages_bitmap) {
		vfree(aml_trace_buffer);
		return;
	}
	statistic_before_insmod_mem();
}
#endif

MODULE_AUTHOR("Amlogic pagetrace owner");
MODULE_DESCRIPTION("Amlogic page trace driver for memory debug");
MODULE_LICENSE("GPL v2");
