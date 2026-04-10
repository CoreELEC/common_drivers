// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/of_device.h>
#include <linux/mm.h>
#include <linux/amlogic/gki_module.h>
#include <linux/timer.h>
#include <linux/hardirq.h>
#include <asm/arch_timer.h>
#include <linux/amlogic/aml_mbox.h>
#include <linux/of_platform.h>
#include <linux/amlogic/aml_aocpu_monitor.h>

static struct aocpu_work_data *aocpu_data;
static struct mbox_chan *mbox_chan;

static int send_message_to_aocpu(unsigned int *cmd, unsigned int size)
{
	unsigned int msg;

	aml_mbox_transfer_data(mbox_chan, MBOX_CMD_SEND_ARM_ALIVE_MSG,
				cmd, size, &msg, sizeof(msg), MBOX_SYNC);

	if (msg == 1)
		return 0;
	else
		return -1;
}

static void do_aocpu_ping_work(struct kthread_work *work)
{
	struct aocpu_work_data *data = container_of(work, struct aocpu_work_data, monitor_work);

	if (!data->enabled) {
		data->alive_check_cmd[0] = ARM_ALIVE_CHECK_EN;
		data->alive_check_cmd[1] = 0;

		if (send_message_to_aocpu(data->alive_check_cmd,
					  sizeof(data->alive_check_cmd)) < 0)
			goto FAIL;

		data->alive_check_cmd[0] = ARM_ALIVE_CHECK_TIMESET;
		data->alive_check_cmd[1] = AOCPU_TIMEOUT;
		send_message_to_aocpu(data->alive_check_cmd, sizeof(data->alive_check_cmd));

		data->enabled = true;
		pr_info("enable aocpu monitor!\n");
	} else {
		data->alive_check_cmd[0] = ARM_ALIVE_CHECK_TICK;
		data->alive_check_cmd[1] = 0;
		send_message_to_aocpu(data->alive_check_cmd, sizeof(data->alive_check_cmd));
	}

	return;

FAIL:
	del_timer_sync(&aocpu_data->aocpu_timer);
	pr_info("fail to enable aocpu monitor!\n");
}

static void aocpu_timer_work(struct timer_list *timer)
{
	struct aocpu_work_data *data = container_of(timer, struct aocpu_work_data, aocpu_timer);

	kthread_queue_work(&data->monitor_worker, &data->monitor_work);
	mod_timer(&data->aocpu_timer, jiffies + msecs_to_jiffies(AOCPU_TIMEOUT * 200));
}

static int __ref aocpu_monitor_probe(struct platform_device *pdev)
{
	mbox_chan = aml_mbox_request_channel_byidx(&pdev->dev, 0);

	if (IS_ERR_OR_NULL(mbox_chan))
		return PTR_ERR(mbox_chan);

	aocpu_data = kzalloc(sizeof(*aocpu_data), GFP_KERNEL);
	if (!aocpu_data)
		return -ENOMEM;

	kthread_init_worker(&aocpu_data->monitor_worker);
	aocpu_data->monitor_task = kthread_run(kthread_worker_fn,
					&aocpu_data->monitor_worker, "aocpu_monitor_thread");

	if (!IS_ERR(aocpu_data->monitor_task)) {
		struct sched_param param = { .sched_priority = MAX_RT_PRIO - 99 };

		sched_setscheduler(aocpu_data->monitor_task, SCHED_FIFO, &param);
	}

	kthread_init_work(&aocpu_data->monitor_work, do_aocpu_ping_work);
	timer_setup(&aocpu_data->aocpu_timer, aocpu_timer_work, 0);
	mod_timer(&aocpu_data->aocpu_timer, jiffies + msecs_to_jiffies(AOCPU_TIMEOUT * 200));

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id aocpu_monitor_match[] = {
	{
		.compatible = "amlogic, aocpu_monitor",
	},
	{}
};
#endif

static struct platform_driver aocpu_monitor_driver = {
	.driver = {
		.name  = "aocpu_monitor",
		.owner = THIS_MODULE,
	#ifdef CONFIG_OF
		.of_match_table = aocpu_monitor_match,
	#endif
	},
	.probe = aocpu_monitor_probe,
};

int aocpu_monitor_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&aocpu_monitor_driver);

	return ret;
}
