/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_TASK_H
#define __MESON_TX_TASK_H

#include <linux/workqueue.h>

#include "meson_tx_hw_task.h"

struct tx_task_manager;

#define TASK_FLAG_INIT   BIT(0)
#define TASK_FLAG_QUEUE  BIT(1)
#define TASK_FLAG_DELAY_WORK BIT(2)
#define TASK_FLAG_WORK   BIT(3)

#define QUEUE_NAME_MAX_LEN 64

typedef void (*tx_task_callback)(void *);

enum task_queue_type {
	TASK_QUEUE_HPD,
	TASK_QUEUE_CORE,
	TASK_QUEUE_HPD_IRQ,
	TASK_QUEUE_TIMER,
	TASK_QUEUE_LOW,
	TASK_QUEUE_HIGH,
	TASK_QUEUE_SYSTEM,
	TASK_QUEUE_SENDFIFO,
	TASK_QUEUE_HDCP,
	MAX_TASK_QUEUE,
};

struct tx_task_info {
	char *name;
	void *para;
	tx_task_callback fn;
	enum task_type type;
	enum task_queue_type queue_type;
	u32 flag;
	char *init_queue_name;
	char queue_name[QUEUE_NAME_MAX_LEN];
	u32 queue_flag;
};

struct tx_task_manager *tx_task_mgr_init(void);
void tx_task_mgr_release(struct tx_task_manager *mgr);
int tx_task_mgr_setup_task(struct tx_task_manager *mgr, struct tx_task_info *info, void *para);
int tx_task_mgr_queue_task(struct tx_task_manager *mgr, u32 type, u32 delay);
int tx_task_mgr_cancel_task(struct tx_task_manager *mgr, u32 type, bool sync);

#endif
