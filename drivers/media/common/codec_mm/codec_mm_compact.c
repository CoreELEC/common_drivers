// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/cma.h>
#include <linux/sched/signal.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/kthread.h>
#include <linux/printk.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <uapi/linux/sched/types.h>
#include <linux/amlogic/aml_cma.h>
#include <linux/amlogic/gki_module.h>
#include <linux/kprobes.h>
#include <cma.h>

#include "codec_mm_common.h"

#define COMPACT_STEP_SIZE_4K (4UL * SZ_1M) /* 4MB */
#define COMPACT_STEP_SIZE_2K (1UL * SZ_1M) /* 1MB */

#define COMPACT_DEFAULT_SIZE (64) /* 64MB */

enum cma_compact_state {
	CMA_COMPACT_STOP = 0,
	CMA_COMPACT_RUN = 1,
};

struct cma_compact_ctrl {
	atomic_t state;        /* RUN or STOP */
	int rounds_left;       /* >0: remaining rounds, -1: run indefinitely */
	struct mutex compact_lock;   /* mutex to serialize compaction and CMA allocation */
	wait_queue_head_t kthread_wq;  /* compact kthread sleeps here waiting to be triggered */
	int compact_step;      /* size of memory processed per compaction step */

	spinlock_t state_lock;     /* protect compact_active / compact_start / compact_end */
	bool compact_active;		 /* true while page migration is in progress */
	unsigned long compact_start; /* start PFN of the current compaction window [start, end) */
	unsigned long compact_end;	 /* end PFN of the current compaction window (exclusive) */

	wait_queue_head_t alloc_wq;  /* CMA alloc wait when PFN range overlaps with compaction */
};

static int codec_mm_compaction_enable = 1;  // enabled by default

static int compact_step_size;
module_param(compact_step_size, int, 0644);
static int once_sleep_time = 100;
module_param(once_sleep_time, int, 0644);
static int loop_sleep_time = 2000;
module_param(loop_sleep_time, int, 0644);
static int compact_size = COMPACT_DEFAULT_SIZE;
module_param(compact_size, int, 0644);
static int print_debug;
module_param(print_debug, int, 0644);

static struct cma_compact_ctrl cma_ctrl;

/*
 * pagemap_done: completed once get_mte_sync_tags_hook_kprobe finishes its
 * one-shot cma_alloc + pagemap setup.  codec_mm_compaction_init waits on
 * this so the compaction kthread never races with the full-CMA allocation.
 */
static DECLARE_COMPLETION(pagemap_done);

void codec_mm_pagemap_done_signal(void)
{
	complete(&pagemap_done);
}

#if IS_BUILTIN(CONFIG_AMLOGIC_CMA)
int (*aml_compact_alloc_f)(unsigned long start, unsigned long end,
	unsigned int migrate_type, gfp_t gfp_mask);

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

static struct cma_compact_ctrl *get_compact_ctrl(void)
{
	return &cma_ctrl;
}

int codec_mm_compact_trylock(void)
{
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();

	return mutex_trylock(&ctrl->compact_lock);
}

void codec_mm_compact_lock(void)
{
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();

	mutex_lock(&ctrl->compact_lock);
}

void codec_mm_compact_unlock(void)
{
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();

	mutex_unlock(&ctrl->compact_lock);
}

static bool cma_find_alloc_pfn_range(struct cma *cma, unsigned long page_count,
				     unsigned int align,
				     unsigned long *pfn_start,
				     unsigned long *pfn_end)
{
	unsigned long mask, offset, bitmap_maxno, bitmap_count, bitmap_no;

	if (!cma || !cma->bitmap)
		return false;

	/* replicate the helper logic from aml_cma.c */
	if (align <= cma->order_per_bit)
		mask = 0;
	else
		mask = (1UL << (align - cma->order_per_bit)) - 1;

	offset = (cma->base_pfn >> cma->order_per_bit) & mask;

	bitmap_maxno = cma_bitmap_maxno(cma);
	bitmap_count = ALIGN(page_count, 1UL << cma->order_per_bit)
			>> cma->order_per_bit;

	if (!bitmap_count || bitmap_count > bitmap_maxno)
		return false;

	spin_lock_irq(&cma->lock);
	bitmap_no = bitmap_find_next_zero_area_off(cma->bitmap,
						   bitmap_maxno, 0,
						   bitmap_count, mask, offset);
	spin_unlock_irq(&cma->lock);

	if (bitmap_no >= bitmap_maxno)
		return false;

	*pfn_start = cma->base_pfn + (bitmap_no << cma->order_per_bit);
	*pfn_end   = *pfn_start + page_count;
	return true;
}

void range_conflict_check_wait(void *handle, size_t page_count,
	unsigned int align)
{
	struct cma *cma = (struct cma *)handle;
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();
	unsigned long req_start, req_end;
	unsigned long cpt_start, cpt_end;

	/* Fast path: no compaction in progress, nothing to wait for. */
	spin_lock(&ctrl->state_lock);
	if (!READ_ONCE(ctrl->compact_active)) {
		spin_unlock(&ctrl->state_lock);
		return;
	}
	cpt_start = ctrl->compact_start;
	cpt_end   = ctrl->compact_end;
	spin_unlock(&ctrl->state_lock);

	if (!cma_find_alloc_pfn_range(cma, page_count, align,
				      &req_start, &req_end))
		return; /* bitmap full — cma_alloc will fail on its own */

	if (req_end <= cpt_start || req_start >= cpt_end) {
		if (print_debug)
			pr_info("no need wait compact [%lx %lx] -- [%lx %lx]\n",
				req_start, req_end, cpt_start, cpt_end);
		return; /* no overlap, proceed immediately */
	}

	if (print_debug)
		pr_info("cma alloc [%lx %lx] currently wait compact work [%lx %lx]\n",
			req_start, req_end, cpt_start, cpt_end);
	if (!wait_event_timeout(ctrl->alloc_wq,
			!READ_ONCE(ctrl->compact_active) ||
			READ_ONCE(ctrl->compact_end) <= req_start ||
			READ_ONCE(ctrl->compact_start) >= req_end,
			msecs_to_jiffies(3000)))
		pr_info("codec_mm: timed out waiting for compact [%lx-%lx) to finish\n",
				cpt_start, cpt_end);
}

static bool __nocfi codec_mm_compact_range(struct cma *cma, unsigned long base_pfn,
	unsigned long end_pfn)
{
	unsigned long i;
	struct cma *codec_cma = cma;
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();
	unsigned long nr_pages = end_pfn - base_pfn;
	int rc;

	if (!codec_cma) {
		pr_err("invalid param, base: %lx, %lx\n", base_pfn, nr_pages);
		return false;
	}

	spin_lock_irq(&codec_cma->lock);
	for (i = 0; i < nr_pages; i++) {
		if (test_bit(i + base_pfn - codec_cma->base_pfn, codec_cma->bitmap)) {
			spin_unlock_irq(&codec_cma->lock);
			return false;
		}
	}
	spin_unlock_irq(&codec_cma->lock);

	if (!codec_mm_compact_trylock()) {
		if (print_debug)
			pr_info("cam alloc is working\n");
		return false;
	}

	spin_lock(&ctrl->state_lock);
	WRITE_ONCE(ctrl->compact_active, true);
	WRITE_ONCE(ctrl->compact_start, base_pfn);
	WRITE_ONCE(ctrl->compact_end, end_pfn);
	spin_unlock(&ctrl->state_lock);
	codec_mm_compact_unlock();

#if IS_BUILTIN(CONFIG_AMLOGIC_CMA)
	rc = aml_compact_alloc_f(base_pfn, end_pfn, MIGRATE_CMA, GFP_KERNEL);
#else
	rc = aml_compact_alloc(base_pfn, end_pfn, MIGRATE_CMA, GFP_KERNEL);
#endif
	if (rc == 0)
		free_contig_range(base_pfn, nr_pages);

	spin_lock(&ctrl->state_lock);
	WRITE_ONCE(ctrl->compact_active, false);
	spin_unlock(&ctrl->state_lock);

	wake_up_all(&ctrl->alloc_wq);

	return true;
}

/* Start the compaction thread (single, multiple, or infinite rounds) */
void codec_mm_compaction_start(int rounds)
{
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();

	if (rounds == 0)
		return;

	/* must be STOP -> RUN */
	if (atomic_cmpxchg(&ctrl->state,
		CMA_COMPACT_STOP, CMA_COMPACT_RUN) != CMA_COMPACT_STOP)
		return;

	ctrl->rounds_left = rounds;  /* >0 or -1 */
	wake_up(&ctrl->kthread_wq);
	if (print_debug)
		pr_info("start cma_compaction, rounds %d\n", ctrl->rounds_left);
}

void codec_mm_compaction_stop(void)
{
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();

	atomic_set(&ctrl->state, CMA_COMPACT_STOP);
	if (print_debug)
		pr_info("stop cma_compaction\n");
}

int codec_mm_compaction_thread(void *data)
{
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();
	phys_addr_t pos;
	phys_addr_t cma_base;
	unsigned long cma_size;
	struct cma *cma = (struct cma *)data;
	int step;
	int count;

	if (wait_for_completion_timeout(&pagemap_done, msecs_to_jiffies(3000)) == 0)
		pr_info("codec_mm: pagemap setup timed out, starting compaction anyway\n");

	cma_base = cma_get_base(cma);

	while (!kthread_should_stop()) {
		/* Wait for compaction trigger */
		wait_event_interruptible(ctrl->kthread_wq,
			atomic_read(&ctrl->state) == CMA_COMPACT_RUN || kthread_should_stop());

		if (kthread_should_stop())
			break;

		/* Enter the main compaction loop */
		while (atomic_read(&ctrl->state) == CMA_COMPACT_RUN) {
			cma_size = cma_get_size(cma);
			if (compact_size && compact_size * SZ_1M < cma_size)
				cma_size = compact_size * SZ_1M;

			step = compact_step_size ? (compact_step_size * SZ_1M) : ctrl->compact_step;
			count = 0;
			for (pos = cma_base; pos < cma_base + cma_size; pos += step) {
				if (atomic_read(&ctrl->state) != CMA_COMPACT_RUN)
					break;

				if (codec_mm_compact_range(cma, PHYS_PFN(pos),
					PHYS_PFN(min(pos + step, cma_base + cma_size)))) {
					count++;
					msleep(once_sleep_time);
				}
			}
			if (print_debug)
				pr_info("cma_compaction loop=%d start_compact base %lx size %lx count %d\n",
					ctrl->rounds_left, (unsigned long)cma_base,
					cma_size, count);

			if (atomic_read(&ctrl->state) != CMA_COMPACT_RUN)
				break;

			/* Decrement only for finite rounds */
			if (ctrl->rounds_left > 0)
				ctrl->rounds_left--;

			/* All rounds completed, switch back to STOP */
			if (ctrl->rounds_left == 0) {
				atomic_set(&ctrl->state, CMA_COMPACT_STOP);
				break;
			}

			msleep(loop_sleep_time);
		}
	}

	return 0;
}

void codec_mm_compaction_init(void *data)
{
	struct cma_compact_ctrl *ctrl = get_compact_ctrl();
	struct task_struct *task = NULL;

	ctrl->compact_step = COMPACT_STEP_SIZE_4K;
	if (is_2k_platform())
		ctrl->compact_step = COMPACT_STEP_SIZE_2K;

	init_waitqueue_head(&ctrl->kthread_wq);
	init_waitqueue_head(&ctrl->alloc_wq);
	spin_lock_init(&ctrl->state_lock);
	mutex_init(&ctrl->compact_lock);

	if (!codec_mm_compaction_enable) {
		pr_info("codec_mm compaction disabled by bootargs\n");
		return;
	}

#if IS_BUILTIN(CONFIG_AMLOGIC_CMA)
	// for gki1.0
	aml_compact_alloc_f = (int (*)(unsigned long start, unsigned long end,
		unsigned int migrate_type, gfp_t gfp_mask))get_symbol_addr("aml_compact_alloc");
	pr_debug("aml_compact_alloc_f: %px\n", aml_compact_alloc_f);
	if (!aml_compact_alloc_f) {
		pr_info("aml_compact_alloc not found, skip compaction hook!\n");
		return;
	}
#endif

	task = kthread_run(codec_mm_compaction_thread, data, "cma_compaction");
	if (!IS_ERR(task)) {
		set_user_nice(task, -1);
		codec_mm_compaction_start(-1);
		pr_debug("create codec_mm compaction task %p\n", task);
	} else {
		pr_err("create codec_mm compaction task fail:%p\n", task);
	}
}

static int codec_mm_compaction_enable_setup(char *str)
{
	if (!str)
		return 0;

	if (strcmp(str, "0") == 0)
		codec_mm_compaction_enable = 0;
	else
		codec_mm_compaction_enable = 1;

	return 1;
}
__setup("codec_mm_compaction_enable=", codec_mm_compaction_enable_setup);

