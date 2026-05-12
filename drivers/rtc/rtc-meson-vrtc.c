// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2019 BayLibre, SAS
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2015 Amlogic, Inc. All rights reserved.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/rtc.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/time64.h>

#include <linux/mailbox_controller.h>
#include <linux/amlogic/aml_mbox.h>
#include <linux/mailbox_client.h>
#include <linux/delay.h>
#include "main.h"
/*
 * Seconds sinc epoch time
 */
static u64 vrtc_init_date;
static u64 last_jiffies;

struct meson_vrtc_data {
	void __iomem *io_alarm;
	struct rtc_device *rtc;
	s64 alarm_time;
	struct timer_list alarm;
	bool find_mboxes;
	/* boot time when suspend */
	struct timespec64 sus_time;
	struct mbox_chan *mbox_chan;
	bool enabled;
};

static int meson_vrtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct timespec64 time;
	struct timespec64 boot_time;

	dev_dbg(dev, "%s\n", __func__);
	ktime_get_boottime_ts64(&boot_time);
	time.tv_sec = boot_time.tv_sec + vrtc_init_date;
	rtc_time64_to_tm(time.tv_sec, tm);

	return 0;
}

static int meson_vrtc_set_time(struct device *dev, struct rtc_time *tm)
{
	time64_t time;
	struct timespec64 boot_time;

	time = rtc_tm_to_time64(tm);
	if (time < 0)
		return -EINVAL;

	ktime_get_boottime_ts64(&boot_time);
	if (boot_time.tv_sec < 0 || time < boot_time.tv_sec)
		return -EINVAL;

	vrtc_init_date = time - boot_time.tv_sec;

	return 0;
}

static void meson_vrtc_adjust_alarm(struct meson_vrtc_data *vrtc,
				       unsigned long time)
{
	struct timespec64 res_time;
	struct timespec64 delta_alarm;
	unsigned long delta = 0;

	if (time > 0) {
		ktime_get_boottime_ts64(&vrtc->sus_time);
	} else {
		ktime_get_boottime_ts64(&res_time);
		vrtc->sus_time.tv_sec += vrtc->alarm_time;
		delta_alarm = timespec64_sub(vrtc->sus_time, res_time);
		if (delta_alarm.tv_sec > 0) {
			delta = timespec64_to_jiffies(&delta_alarm);
			mod_timer(&vrtc->alarm, jiffies + delta);
			last_jiffies = jiffies;
			vrtc->alarm_time = delta_alarm.tv_sec;
		} else {
			mod_timer(&vrtc->alarm, jiffies);
			vrtc->alarm_time = 0;
		}
		dev_info(vrtc->rtc->dev.parent, "%s: remaining alarm time = %llu s\n",
				__func__, vrtc->alarm_time);
	}
}

static void meson_vrtc_set_wakeup_time(struct meson_vrtc_data *vrtc,
				       unsigned long time)
{
	meson_vrtc_adjust_alarm(vrtc, time);
	writel_relaxed(time, vrtc->io_alarm);
}

static int meson_vrtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);
	u64 expires;
	struct timespec64 boot_time;

	del_timer(&vrtc->alarm);
	if (alarm->enabled) {
		ktime_get_boottime_ts64(&boot_time);
		vrtc->alarm_time = rtc_tm_to_time64(&alarm->time)
			- boot_time.tv_sec - vrtc_init_date;
		last_jiffies = jiffies;
		expires = vrtc->alarm_time * HZ + jiffies;
		vrtc->alarm.expires = expires;
		add_timer(&vrtc->alarm);
	} else {
		vrtc->alarm_time = 0;
	}
	vrtc->enabled = alarm->enabled;

	dev_dbg(dev, "%s: alarm->enabled=%d alarm=0x%llx vrtc=0x%llx\n", __func__,
		alarm->enabled, rtc_tm_to_time64(&alarm->time), vrtc->alarm_time);

	return 0;
}

static int meson_vrtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);
	time64_t local_alarm = 0;
	struct timespec64 boot_time;

	if (vrtc->enabled) {
		ktime_get_boottime_ts64(&boot_time);
		local_alarm = vrtc->alarm_time - div64_u64(jiffies - last_jiffies, HZ)
			      + vrtc_init_date + boot_time.tv_sec;
	}
	rtc_time64_to_tm(local_alarm, &alarm->time);
	alarm->enabled = vrtc->enabled;

	dev_dbg(dev, "%s: alarm->enabled=%d alarm=0x%llx\n", __func__, alarm->enabled, local_alarm);

	return 0;
}

static void meson_vrtc_alarm_handler(struct timer_list *t)
{
	struct meson_vrtc_data *vrtc = from_timer(vrtc, t, alarm);

	rtc_update_irq(vrtc->rtc, 1, RTC_AF | RTC_IRQF);
}

static int meson_vrtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);

	if (!enabled)
		del_timer(&vrtc->alarm);
	vrtc->enabled = enabled;

	return 0;
}

static const struct rtc_class_ops meson_vrtc_ops = {
	.read_time = meson_vrtc_read_time,
	.set_time = meson_vrtc_set_time,
	.read_alarm = meson_vrtc_read_alarm,
	.set_alarm = meson_vrtc_set_alarm,
	.alarm_irq_enable = meson_vrtc_alarm_irq_enable,
};

static int meson_vrtc_probe(struct platform_device *pdev)
{
	struct meson_vrtc_data *vrtc;
	int ret;
	u32 vrtc_val;

	vrtc = devm_kzalloc(&pdev->dev, sizeof(*vrtc), GFP_KERNEL);
	if (!vrtc)
		return -ENOMEM;

	vrtc->io_alarm = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(vrtc->io_alarm))
		return PTR_ERR(vrtc->io_alarm);

	device_init_wakeup(&pdev->dev, 1);

	platform_set_drvdata(pdev, vrtc);

	vrtc->rtc = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(vrtc->rtc))
		return PTR_ERR(vrtc->rtc);

	vrtc->rtc->ops = &meson_vrtc_ops;

	vrtc->mbox_chan = aml_mbox_request_channel_byidx(&pdev->dev, 0);
	if (IS_ERR_OR_NULL(vrtc->mbox_chan)) {
		vrtc_val = 0;
	} else {
		ret = aml_mbox_transfer_data(vrtc->mbox_chan, MBOX_CMD_GET_RTC,
				       NULL, 0, &vrtc_val, sizeof(vrtc_val),
				       MBOX_SYNC);
		if (ret < 0) {
			vrtc_val = 0;
			dev_err(&pdev->dev, "Fail to send mbox message\n");
		}
	}
	vrtc_init_date = vrtc_val;

	timer_setup(&vrtc->alarm, meson_vrtc_alarm_handler, 0);
	vrtc->alarm.expires = 0;

	ret = devm_rtc_register_device(vrtc->rtc);
	if (ret)
		return ret;

	return 0;
}

static int meson_vrtc_suspend(struct device *dev)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);
	if (vrtc->alarm_time && vrtc->enabled) {
		unsigned long local_time;
		struct timespec64 time;

		ktime_get_raw_ts64(&time);
		local_time = time.tv_sec;
		vrtc->alarm_time = vrtc->alarm_time - div64_u64(jiffies - last_jiffies, HZ);
		dev_info(dev, "alarm_time = %llus, local_time=%lus\n",
			vrtc->alarm_time, local_time);

		if (vrtc->alarm_time > 0) {
			meson_vrtc_set_wakeup_time(vrtc, vrtc->alarm_time);
			dev_dbg(dev, "wakeup alarm %llds\n", vrtc->alarm_time);
		} else {
			dev_err(dev, "alarm time already passed: %llds.\n",
				vrtc->alarm_time);
		}
	}

	return 0;
}

static int meson_vrtc_resume(struct device *dev)
{
	struct meson_vrtc_data *vrtc = dev_get_drvdata(dev);

	dev_dbg(dev, "%s\n", __func__);
	meson_vrtc_set_wakeup_time(vrtc, 0);

	return 0;
}

static SIMPLE_DEV_PM_OPS(meson_vrtc_pm_ops,
			 meson_vrtc_suspend, meson_vrtc_resume);

static void meson_vrtc_shutdown(struct platform_device *pdev)
{
	struct timespec64 now;
	struct meson_vrtc_data *vrtc = dev_get_drvdata(&pdev->dev);

	ktime_get_real_ts64(&now);
	aml_mbox_transfer_data(vrtc->mbox_chan, MBOX_CMD_SET_RTC,
			       &now.tv_sec, sizeof(now.tv_sec),
			       NULL, 0, MBOX_SYNC);

	if (vrtc->rtc->aie_timer.enabled) {
		vrtc->alarm_time = vrtc->alarm_time
			   - div64_u64(jiffies - last_jiffies, HZ);
		if (vrtc->alarm_time > 0) {
			meson_vrtc_set_wakeup_time(vrtc, vrtc->alarm_time);
			dev_info(&pdev->dev, "%s: system will wakeup %llus later\n",
					__func__, vrtc->alarm_time);
		}
	}
}

static const struct of_device_id meson_vrtc_dt_match[] = {
	{ .compatible = "amlogic,meson-vrtc"},
	{},
};
MODULE_DEVICE_TABLE(of, meson_vrtc_dt_match);

static struct platform_driver meson_vrtc_driver = {
	.probe = meson_vrtc_probe,
	.driver = {
		.name = "meson-vrtc",
		.of_match_table = meson_vrtc_dt_match,
		.pm = &meson_vrtc_pm_ops,
	},
	.shutdown = meson_vrtc_shutdown,
};

int __init vrtc_init(void)
{
	return platform_driver_register(&meson_vrtc_driver);
}

void __exit vrtc_exit(void)
{
	platform_driver_unregister(&meson_vrtc_driver);
}
