// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/workqueue.h>

#include "meson_tx_task_mgr.h"

struct tx_task {
	struct delayed_work base;
	char *name;
	u32 delay;
	u32 state;
	u32 flags;
	void *para;
	tx_task_callback fn;
	enum task_type type;
	enum task_queue_type queue_type;
};

struct tx_task_manager {
	struct tx_task *tasks[MAX_TASK];
	struct workqueue_struct *queues[MAX_TASK_QUEUE];
};

static void meson_work_handle(struct work_struct *work)
{
	struct delayed_work *d_work = (struct delayed_work *)work;
	struct tx_task *task = container_of(d_work,
						struct tx_task, base);

	task->fn(task->para);
}

static struct tx_task *get_task_by_type(struct tx_task_manager *mgr, enum task_type type)
{
	if (!mgr)
		return NULL;

	if (mgr->tasks[type])
		return mgr->tasks[type];
	else
		return NULL;
}

int tx_task_mgr_setup_task(struct tx_task_manager *mgr, struct tx_task_info *info, void *para)
{
	struct tx_task *task;
	struct workqueue_struct *workqueue;

	task = kzalloc(sizeof(*task), GFP_KERNEL);
	if (!task)
		return -ENOMEM;

	task->name = info->name;
	task->para = para;
	task->fn = info->fn;
	task->type = info->type;
	task->queue_type = info->queue_type;
	task->flags = info->flag;

	if (!mgr->queues[task->queue_type] && info->queue_name[0]) {
		workqueue = alloc_ordered_workqueue(info->queue_name,
					info->queue_flag);
		if (!workqueue)
			return -ENOMEM;
		mgr->queues[task->queue_type] = workqueue;
	}

	INIT_DELAYED_WORK(&task->base, meson_work_handle);

	mgr->tasks[task->type] = task;

	return 0;
}

int tx_task_mgr_queue_task(struct tx_task_manager *mgr, u32 type, u32 delay)
{
	struct tx_task *task;
	int ret = 0;

	task = get_task_by_type(mgr, type);
	if (!task)
		return 0;

	ret = queue_delayed_work(mgr->queues[task->queue_type],
			   &task->base, msecs_to_jiffies(delay));
	if (!ret)
		pr_err("%s is already in the queue\n", task->name);

	return ret;
}

int tx_task_mgr_cancel_task(struct tx_task_manager *mgr, u32 type, bool sync)
{
	struct tx_task *task;
	int ret = 0;

	task = get_task_by_type(mgr, type);
	if (!task)
		return 0;

	if (sync)
		ret = cancel_delayed_work_sync(&task->base);
	else
		ret = cancel_delayed_work(&task->base);

	if (ret)
		pr_debug("%s is pending and canceled\n", task->name);
	else
		pr_err("%s is not pending\n", task->name);

	return ret;
}

struct tx_task_manager *tx_task_mgr_init(void)
{
	struct tx_task_manager *mgr;

	mgr = kzalloc(sizeof(*mgr), GFP_KERNEL);
	if (!mgr)
		return NULL;

	return mgr;
}

void tx_task_mgr_release(struct tx_task_manager *mgr)
{
	int i;

	for (i = 0; i < MAX_TASK; i++) {
		if (mgr->tasks[i])
			tx_task_mgr_cancel_task(mgr, i, true);
		kfree(mgr->tasks[i]);
	}

	for (i = 0; i < MAX_TASK_QUEUE; i++) {
		if (mgr->queues[i])
			destroy_workqueue(mgr->queues[i]);
	}

	kfree(mgr);
}

