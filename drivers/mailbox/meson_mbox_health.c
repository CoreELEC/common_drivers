// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/kdebug.h>
#include <linux/notifier.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/of.h>
#include <linux/mailbox_controller.h>
#include <dt-bindings/mailbox/amlogic,mbox.h>
#include <linux/amlogic/aml_mbox.h>
#include "meson_mbox_comm.h"

static bool mbox_aocpu_hang;
static bool mbox_dspa_hang;
static bool mbox_dspb_hang;

static bool mbox_aocpu_health_check(struct aocpu_health_info *health_info)
{
	bool hang = false;

	health_info->cur_cnt = readl(health_info->health_reg);

	pr_debug("[%s][pre_cnt: %u, cur_cnt: %u]\n", __func__,
			health_info->pre_cnt, health_info->cur_cnt);

	if (health_info->cur_cnt == health_info->pre_cnt)
		hang = true;

	health_info->pre_cnt = health_info->cur_cnt;
	return hang;
}

static void mbox_aocpu_health_monitor(struct work_struct *work)
{
	struct aocpu_health_info *health_info;
	struct device *dev;

	health_info = container_of((struct delayed_work *)work,
			struct aocpu_health_info, aocpu_monitor_work);
	dev = health_info->dev;

	if (mbox_aocpu_health_check(health_info)) {
		mbox_aocpu_hang = true;
		dev_dbg(dev, "[%s]: AOCPU hang\n", __func__);
	} else {
		mbox_aocpu_hang = false;
	}

	queue_delayed_work(health_info->aocpu_health_wq,
			&health_info->aocpu_monitor_work,
			msecs_to_jiffies(health_info->health_polling_ms));
}

int mbox_aocpu_health_monitor_start(struct aocpu_health_info *health_info)
{
	if (IS_ERR_OR_NULL(health_info) || IS_ERR_OR_NULL(health_info->health_reg)) {
		pr_err("Error: invalid health info\n");
		return -EINVAL;
	}

	health_info->pre_cnt = 0;
	health_info->cur_cnt = 0;
	mbox_aocpu_hang = false;
	health_info->aocpu_health_wq = create_workqueue("aocpu_health_wq");
	INIT_DEFERRABLE_WORK(&health_info->aocpu_monitor_work,
			mbox_aocpu_health_monitor);
	queue_delayed_work(health_info->aocpu_health_wq,
			&health_info->aocpu_monitor_work,
			msecs_to_jiffies(health_info->health_polling_ms));

	return 0;
}

int mbox_aocpu_health_monitor_stop(struct aocpu_health_info *health_info)
{
	if (IS_ERR_OR_NULL(health_info)) {
		pr_err("Error: invalid health info\n");
		return -EINVAL;
	}

	cancel_delayed_work_sync(&health_info->aocpu_monitor_work);
	flush_workqueue(health_info->aocpu_health_wq);
	destroy_workqueue(health_info->aocpu_health_wq);

	return 0;
}

static bool mbox_aocpu_hang_status(void)
{
	return mbox_aocpu_hang;
}

static bool mbox_dspa_hang_status(void)
{
	return mbox_dspa_hang;
}

static bool mbox_dspb_hang_status(void)
{
	return mbox_dspb_hang;
}

bool mbox_check_ip_hang(struct mbox_chan *mbox_chan)
{
	struct aml_mbox_chan *aml_chan;
	bool ip_hang = false;
	u32 dest;

	aml_chan = mbox_chan->con_priv;
	dest = aml_chan->dest;
	if (dest == MAILBOX_AOCPU && aml_chan->aocpu_hang_detect)
		ip_hang = mbox_aocpu_hang_status();

	if (dest == MAILBOX_DSPA && aml_chan->dspa_hang_detect)
		ip_hang = mbox_dspa_hang_status();

	if (dest == MAILBOX_DSPB && aml_chan->dspb_hang_detect)
		ip_hang = mbox_dspb_hang_status();

	return ip_hang;
}
EXPORT_SYMBOL_GPL(mbox_check_ip_hang);

void mbox_set_dsp_hang_status(bool hang, u32 dsp_id)
{
	if (dsp_id == DSPA_ID)
		mbox_dspa_hang = hang;
	else
		mbox_dspb_hang = hang;
}
EXPORT_SYMBOL_GPL(mbox_set_dsp_hang_status);
