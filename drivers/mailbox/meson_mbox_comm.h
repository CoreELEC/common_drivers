/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _MESON_MBOX_COMM_H_
#define _MESON_MBOX_COMM_H_
#include <linux/cdev.h>

#define MAX_ACTIVE_WORK 5
#define MBOX_RX_QUEUE_LEN_DEFAULT 10

#define MBOX_DOMAIN(drv_id, mbox_id, flag)      \
{                                               \
		.flags = flag,                          \
		.drvid = drv_id,                        \
		.mboxid = mbox_id,                      \
}

struct mbox_cmd_t {
	u32 cmd:16;
	u32 size:9;
	u32 sync:7;
};

union mbox_stat {
	u32 set_cmd;
	struct mbox_cmd_t  mbox_cmd_t;
};

struct mbox_domain {
	u32 drvid;
	u32 mboxid;
	u32 flags;
};

struct mbox_domain_data {
	const struct mbox_domain *mbox_domains;
	u32 domain_counts;
};

struct aml_chan_priv {
	struct aml_mbox_chan *aml_chan;
	u32 mbox_nums;
	u32 mbox_rx_queue_len;
};

struct aml_mbox_rx_header {
	int chan_id;
	u32 cmd;
	u32 size;
} __packed;

struct aml_mbox_rx_packet {
	struct aml_mbox_rx_header rx_header;
	char buf[MBOX_DATA_SIZE];
} __packed;

struct aml_mbox_rx_msg {
	unsigned int msg_count;                 /* No. of rx mssg currently queued */
	unsigned int msg_free;                  /* index of next available mssg slot */
	struct aml_mbox_rx_packet *msg_data;    /* hook for rx data packet */
	unsigned int rx_queue_len;              /* queue length of rx data packet */
	spinlock_t lock;                        /* lock for message buffer */
	struct semaphore sem;                   /* semaphore for rx thread */
	struct task_struct *thread;             /* rx thread for mssg process */
};

struct aml_priv_data {
	struct mbox_controller *mbox_ctrl;
	struct mbox_domain_data domain_data;
	struct aml_mbox_rx_msg *rx_msg;
};

#endif
