// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/bitfield.h>
#include <linux/rtc.h>
#include <linux/clk.h>
#include <linux/time64.h>
#include <linux/delay.h>
#include <linux/amlogic/rtc.h>
#include <linux/amlogic/aml_mbox.h>
#include "main.h"

static inline u32 gray_to_binary(u32 gray)
{
	u32 bcd = gray;
	int size = sizeof(bcd) * 8;
	int i;

	for (i = 0; (1 << i) < size; i++)
		bcd ^= bcd >> (1 << i);

	return bcd;
}

static inline u32 binary_to_gray(u32 bcd)
{
	return bcd ^ (bcd >> 1);
}

static void meson_set_time(struct aml_rtc_data *rtc, u32 time_sec)
{
	u32 time_val = time_sec;
	int ret;

	if (rtc->rtc_virtual) {
		ret = aml_mbox_transfer_data(rtc->mbox_chan, MBOX_CMD_SET_RTC,
			&time_val, sizeof(time_val), NULL, 0, MBOX_SYNC);
		if (ret < 0)
			dev_err(&rtc->rtc_dev->dev, "set vrtc time failed\n");
	} else {
		if (rtc->time_storage_format)
			time_val = binary_to_gray(time_val);
		regmap_write(rtc->map, RTC_COUNTER_REG, time_val);
	}
}

static u32 meson_read_time(struct aml_rtc_data *rtc)
{
	u32 time_val = 0;
	int ret;

	if (rtc->rtc_virtual) {
		ret = aml_mbox_transfer_data(rtc->mbox_chan, MBOX_CMD_GET_RTC,
			NULL, 0, &time_val, sizeof(time_val), MBOX_SYNC);
		if (ret < 0)
			dev_err(&rtc->rtc_dev->dev, "get vrtc time failed\n");
	} else {
		regmap_read(rtc->map, RTC_REAL_TIME, &time_val);
		if (rtc->time_storage_format)
			time_val = gray_to_binary(time_val);
	}

	return time_val;
}

static u32 meson_read_alarm(struct aml_rtc_data *rtc)
{
	u32 alarm_val = 0, time_val;

	if (rtc->rtc_virtual) {
		regmap_read(rtc->map, VRTC_ALARM_REG, &alarm_val);
	} else {
		regmap_read(rtc->map, RTC_ALARM0_REG, &alarm_val);
		time_val = meson_read_time(rtc);
		if (rtc->time_storage_format)
			alarm_val = gray_to_binary(alarm_val);
		alarm_val = (alarm_val > time_val) ? (alarm_val - time_val) : 0;
	}

	return alarm_val;
}

static void meson_set_alarm(struct aml_rtc_data *rtc, u32 alarm_sec)
{
	u32 alarm_val = alarm_sec;
	int ret = 0;
	bool enable;

	if (rtc->rtc_virtual) {
		regmap_write(rtc->map, VRTC_ALARM_REG, alarm_val);

		enable = !!alarm_val;
		ret = aml_mbox_transfer_data(rtc->mbox_chan, MBOX_CMD_RTC_ENABLE_ALRM,
			&enable, sizeof(enable), NULL, 0, MBOX_SYNC);
		if (ret < 0)
			dev_err(&rtc->rtc_dev->dev, "enable vrtc timer failed\n");
	} else {
		if (alarm_val) {
			alarm_val += meson_read_time(rtc);
			if (rtc->time_storage_format)
				alarm_val = binary_to_gray(alarm_val);
		}
		regmap_write(rtc->map, RTC_ALARM0_REG, alarm_val);
	}
}

static void meson_enable_alarm(struct aml_rtc_data *rtc, bool enable)
{
	int ret = 0;

	if (rtc->rtc_virtual && !enable) {
		ret = aml_mbox_transfer_data(rtc->mbox_chan, MBOX_CMD_RTC_ENABLE_ALRM,
			&enable, sizeof(enable), NULL, 0, MBOX_SYNC);
		if (ret < 0)
			dev_err(&rtc->rtc_dev->dev, "disable vrtc timer failed\n");
	} else {
		if (enable) {
			regmap_update_bits(rtc->map, RTC_CTRL_REG,
					   RTC_ALRM0_EN, RTC_ALRM0_EN);
			/* wait for alarm enable to take effect(for vlsi) */
			usleep_range(100, 200);
		} else {
			regmap_update_bits(rtc->map, RTC_CTRL_REG, RTC_ALRM0_EN, 0);
		}
	}
}

static int aml_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	u32 time_sec;

	time_sec = meson_read_time(rtc);
	rtc_time64_to_tm(time_sec, tm);
	dev_dbg(dev, "read time = %us\n", time_sec);

	return 0;
}

static int aml_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	u32 time_sec;

	time_sec = rtc_tm_to_time64(tm);
	dev_dbg(dev, "set time = %us\n", time_sec);
	meson_set_time(rtc, time_sec);

	return 0;
}

static int aml_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	u32 time_sec = 0;
	ktime_t alarm_sec = 0;
	time64_t alarm_sec64;

	if (rtc->wakealrm_enabled) {
		alarm_sec64 = rtc_tm_to_time64(&alarm->time);
		if (alarm_sec64 > U32_MAX) {
			dev_err(dev, "alarm value invalid!\n");
			return -EINVAL;
		}
		time_sec = meson_read_time(rtc);
		if (alarm_sec64 > time_sec) {
			alarm_sec = alarm_sec64 - time_sec;
			/* for rtc, need to enable alarm first, then set alarm */
			meson_enable_alarm(rtc, alarm->enabled);
			meson_set_alarm(rtc, alarm_sec);
			rtc->alarm_enabled = alarm->enabled;

			dev_dbg(dev, "alarm->enabled=%d time=%u alarm=%lld\n",
				alarm->enabled, time_sec, alarm_sec);
		} else {
			dev_info(dev, "alarm already expired!\n");
		}
	} else {
		rtc->alarm_enabled = 0;
		dev_info(dev, "rtc alarm is disabled\n");
	}

	return 0;
}

static int aml_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alarm)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	u32 alarm_sec = 0;

	if (rtc->wakealrm_enabled)
		alarm_sec = meson_read_alarm(rtc);
	rtc_time64_to_tm((time64_t)alarm_sec, &alarm->time);
	alarm->enabled = rtc->alarm_enabled;
	dev_dbg(dev, "alarm->enabled=%d alarm=%u\n", alarm->enabled, alarm_sec);

	return 0;
}

static irqreturn_t aml_rtc_alarm_handler(int irq, void *data)
{
	struct aml_rtc_data *rtc = (struct aml_rtc_data *)data;

	/* Clear alarm0 & disable alarm0 */
	meson_set_alarm(rtc, 0);
	meson_enable_alarm(rtc, 0);
	if (!rtc->rtc_virtual) {
		/* Clear alarm0 int status */
		if (regmap_test_bits(rtc->map, RTC_INT_STATUS, RTC_ALRM0_IRQ_STATUS))
			regmap_update_bits(rtc->map, RTC_INT_CLR,
					   RTC_ALRM0_IRQ_CLR, RTC_ALRM0_IRQ_CLR);
	}
	rtc_update_irq(rtc->rtc_dev, 1, RTC_AF | RTC_IRQF);

	return IRQ_HANDLED;
}

/* rtc adjust function, using rtc_ops implementation */
static int aml_rtc_read_offset(struct device *dev, long *offset)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	u32 reg_val;
	long val = 0;
	int sign, match_counter, enable;

	if (!rtc->rtc_virtual) {
		regmap_read(rtc->map, RTC_SEC_ADJUST_REG, &reg_val);
		enable = FIELD_GET(RTC_ADJ_VALID, reg_val);
		if (!enable) {
			val = 0;
		} else {
			sign = FIELD_GET(RTC_SEC_ADJUST_CTRL, reg_val);
			match_counter = FIELD_GET(RTC_MATCH_COUNTER, reg_val);
			val = 1000000000 / (match_counter + 1);
			if (sign == RTC_SWALLOW_SECOND)
				val = -val;
		}
	} else {
		dev_info(dev, "virtual rtc donot support read offset\n");
		return -ENODEV;
	}
	*offset = val;

	return 0;
}

static int aml_rtc_set_offset(struct device *dev, long offset)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	int sign = 0, match_counter = 0, enable = 0;
	u32 reg_val;

	if (!rtc->rtc_virtual) {
		if (offset) {
			enable = 1;
			sign = offset < 0 ? RTC_SWALLOW_SECOND : RTC_INSERT_SECOND;
			match_counter = 1000000000 / abs(offset) - 1;
			if (match_counter < 0 || match_counter > RTC_MATCH_COUNTER)
				return -EINVAL;
		}

		reg_val = FIELD_PREP(RTC_ADJ_VALID, enable) |
			FIELD_PREP(RTC_SEC_ADJUST_CTRL, sign) |
			FIELD_PREP(RTC_MATCH_COUNTER, match_counter);
		regmap_update_bits(rtc->map, RTC_SEC_ADJUST_REG, RTC_MATCH_COUNTER |
				   RTC_SEC_ADJUST_CTRL | RTC_ADJ_VALID, reg_val);
	} else {
		dev_info(dev, "virtual rtc donot support set offset\n");
		return -ENODEV;
	}

	return 0;
}

static int aml_rtc_alarm_enable(struct device *dev, unsigned int enabled)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);

	if (!enabled)
		meson_enable_alarm(rtc, enabled);
	rtc->alarm_enabled = enabled;

	return 0;
}

static ssize_t div256_adj_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev->parent);
	u32 reg_val;
	int adj_val, adj_dsr;

	if (rtc->rtc_virtual) {
		dev_info(dev, "virtual rtc donot support div256_adj\n");
		return -ENODEV;
	}

	regmap_read(rtc->map, RTC_SEC_ADJUST_REG, &reg_val);
	adj_dsr = FIELD_GET(RTC_DIV256_ADJ_DSR, reg_val);
	adj_val = FIELD_GET(RTC_DIV256_ADJ_VAL, reg_val);

	return sprintf(buf, "%d %d\n", adj_dsr, adj_val);
}

/*
 * this node looks at the action of writing, and writes once remove/insert 1/128s.
 */
static ssize_t div256_adj_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev->parent);
	int val;
	int adj_val, adj_dsr;
	unsigned int reg_val = 0;

	if (rtc->rtc_virtual) {
		dev_info(dev, "virtual rtc donot support div256_adj\n");
		return -ENODEV;
	}

	if (kstrtouint(buf, 0, &val)) {
		dev_err(dev, "invalid div256_daj parm=%d\n", val);
		return -EINVAL;
	}

	adj_val = val % 10;
	adj_dsr = val / 10;
	reg_val |= (FIELD_PREP(RTC_DIV256_ADJ_DSR, adj_dsr) |
		    FIELD_PREP(RTC_DIV256_ADJ_VAL, adj_val));
	regmap_write_bits(rtc->map, RTC_SEC_ADJUST_REG,
		RTC_DIV256_ADJ_DSR | RTC_DIV256_ADJ_VAL, reg_val);
	/* rtc need about 30ms to adjust its time after write reg*/
	mdelay(30);

	return count;
}
static DEVICE_ATTR_RW(div256_adj);

static ssize_t wakealrm_enabled_show(struct device *dev,
	struct device_attribute *attr,
	char *buf)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev->parent);
	int enabled = rtc->wakealrm_enabled;

	return sprintf(buf, "%d\n", enabled);
}

static ssize_t wakealrm_enabled_store(struct device *dev,
	 struct device_attribute *attr,
	 const char *buf, size_t count)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev->parent);
	int enabled;

	if (kstrtoint(buf, 0, &enabled)) {
		dev_err(dev, "invalid wakealrm_enabled=%d\n", enabled);
		return -EINVAL;
	}
	rtc->wakealrm_enabled = enabled;

	return count;
}
static DEVICE_ATTR_RW(wakealrm_enabled);

static struct attribute *meson_rtc_attrs[] = {
	&dev_attr_div256_adj.attr,
	&dev_attr_wakealrm_enabled.attr,
	NULL
};

static const struct attribute_group meson_rtc_sysfs_files = {
	.attrs	= meson_rtc_attrs,
};

static const struct rtc_class_ops aml_rtc_ops = {
	.read_time = aml_rtc_read_time,
	.set_time = aml_rtc_set_time,
	.read_alarm = aml_rtc_read_alarm,
	.set_alarm = aml_rtc_set_alarm,
	.alarm_irq_enable = aml_rtc_alarm_enable,
	.read_offset = aml_rtc_read_offset,
	.set_offset = aml_rtc_set_offset,
};

static void aml_rtc_init(struct device *dev, struct aml_rtc_data *rtc)
{
	u32 reg_val = 0;

	if (!regmap_test_bits(rtc->map, RTC_CTRL_REG, RTC_ENABLE)) {
		if (clk_get_rate(rtc->rtc_clk) == OSC_24M) {
			/* select 24M oscillator */
			regmap_write_bits(rtc->map, RTC_CTRL_REG, RTC_OSC_SEL, RTC_OSC_SEL);

			/*
			 * Set RTC oscillator to freq_out to freq_in/((N0*M0+N1*M1)/(M0+M1))
			 * Enable clock_in gate of oscillator 24MHz
			 * Set N0 to 733, N1 to 732
			 */
			reg_val = FIELD_PREP(RTC_OSCIN_IN_EN, 1)
				  | FIELD_PREP(RTC_OSCIN_OUT_CFG, 1)
				  | FIELD_PREP(RTC_OSCIN_OUT_N0M0, RTC_OSCIN_OUT_32K_N0)
				  | FIELD_PREP(RTC_OSCIN_OUT_N1M1, RTC_OSCIN_OUT_32K_N1);
			regmap_write_bits(rtc->map, RTC_OSCIN_CTRL0, RTC_OSCIN_IN_EN
					  | RTC_OSCIN_OUT_CFG | RTC_OSCIN_OUT_N0M0
					  | RTC_OSCIN_OUT_N1M1, reg_val);

			/* Set M0 to 2, M1 to 3, so freq_out = 32768 Hz*/
			reg_val = FIELD_PREP(RTC_OSCIN_OUT_N0M0, RTC_OSCIN_OUT_32K_M0)
				  | FIELD_PREP(RTC_OSCIN_OUT_N1M1, RTC_OSCIN_OUT_32K_M1);
			regmap_write_bits(rtc->map, RTC_OSCIN_CTRL1, RTC_OSCIN_OUT_N0M0
					  | RTC_OSCIN_OUT_N1M1, reg_val);
		} else {
			/* select 32K oscillator */
			regmap_write_bits(rtc->map, RTC_CTRL_REG, RTC_OSC_SEL, 0);
		}
		/* enable rtc */
		regmap_write_bits(rtc->map, RTC_CTRL_REG, RTC_ENABLE, RTC_ENABLE);
		usleep_range(100, 200);
	}
	/* unmask alarm0 irq */
	regmap_write_bits(rtc->map, RTC_INT_MASK, RTC_ALRM0_IRQ_MSK, 0);
	/* disable alarm0 */
	regmap_write_bits(rtc->map, RTC_CTRL_REG, RTC_ALRM0_EN, 0);
}

static int aml_rtc_probe(struct platform_device *pdev)
{
	struct aml_rtc_data *rtc;
	void __iomem *base;
	struct resource *res;
	u32 rtc_time = 0;
	int ret;

	rtc = devm_kzalloc(&pdev->dev, sizeof(*rtc), GFP_KERNEL);
	if (!rtc)
		return -ENOMEM;

	base = devm_platform_get_and_ioremap_resource(pdev, 0, &res);
	if (IS_ERR(base)) {
		dev_err_probe(&pdev->dev, PTR_ERR(base), "resource ioremap failed\n");
		return PTR_ERR(base);
	}
	aml_rtc_regmap_config.max_register = resource_size(res) - 4;
	rtc->map = devm_regmap_init_mmio(&pdev->dev, base, &aml_rtc_regmap_config);
	if (IS_ERR(rtc->map)) {
		dev_err_probe(&pdev->dev, PTR_ERR(rtc->map), "regmap init failed\n");
		return PTR_ERR(rtc->map);
	}

	if (of_property_read_bool(pdev->dev.of_node, "time-storage-format-gray"))
		rtc->time_storage_format = true;
	else
		rtc->time_storage_format = false;

	/* rtc supports setting alarm flag */
	if (of_property_read_bool(pdev->dev.of_node, "wakealarm-disabled"))
		rtc->wakealrm_enabled = false;
	else
		rtc->wakealrm_enabled = true;

	if (of_property_read_bool(pdev->dev.of_node, "rtc-virtual"))
		rtc->rtc_virtual = true;
	else
		rtc->rtc_virtual = false;

	if (rtc->rtc_virtual) {
		rtc->mbox_chan = aml_mbox_request_channel_byidx(&pdev->dev, 0);
		if (IS_ERR_OR_NULL(rtc->mbox_chan))
			return dev_err_probe(&pdev->dev, PTR_ERR(rtc->mbox_chan),
					     "failed to get mbox chan\n");
	} else {
		rtc->rtc_clk = devm_clk_get(&pdev->dev, "osc");
		if (IS_ERR(rtc->rtc_clk))
			return dev_err_probe(&pdev->dev, PTR_ERR(rtc->rtc_clk),
					     "failed to get rtc clock\n");
		if (clk_get_rate(rtc->rtc_clk) != OSC_32K &&
		    clk_get_rate(rtc->rtc_clk) != OSC_24M) {
			dev_err(&pdev->dev, "Invalid clock configuration\n");
			return -EINVAL;
		}

		rtc->sys_clk = devm_clk_get(&pdev->dev, "sys");
		if (IS_ERR(rtc->sys_clk))
			return dev_err_probe(&pdev->dev, PTR_ERR(rtc->sys_clk),
					     "failed to get rtc sys clk\n");
		ret = clk_prepare_enable(rtc->sys_clk);
		if (ret)
			return dev_err_probe(&pdev->dev, ret, "Failed to enable clk\n");

		aml_rtc_init(&pdev->dev, rtc);

		rtc->mbox_chan = aml_mbox_request_channel_byidx(&pdev->dev, 0);
		if (!IS_ERR_OR_NULL(rtc->mbox_chan)) {
			ret = aml_mbox_transfer_data(rtc->mbox_chan, MBOX_CMD_GET_RTC,
				NULL, 0, &rtc_time, sizeof(rtc_time), MBOX_SYNC);
			if (ret < 0) {
				dev_err_probe(&pdev->dev, ret, "Fail to get rtc init time\n");
				goto err_clk;
			}
			dev_dbg(&pdev->dev, "Successfully restored rtc time %u\n", rtc_time);
			meson_set_time(rtc, rtc_time);
		} else {
			dev_info(&pdev->dev, "No need mailbox channel\n");
			rtc->mbox_chan = NULL;
		}
	}

	rtc->irq = platform_get_irq(pdev, 0);
	if (rtc->irq < 0) {
		ret = dev_err_probe(&pdev->dev, rtc->irq, "Failed to get rtc irq\n");
		goto err_clk;
	} else {
		ret = devm_request_threaded_irq(&pdev->dev, rtc->irq, NULL, aml_rtc_alarm_handler,
				       IRQF_ONESHOT, "aml-rtc alarm", rtc);
		if (ret) {
			dev_err_probe(&pdev->dev, ret, "Failed to request irq\n");
			goto err_clk;
		}
	}

	device_init_wakeup(&pdev->dev, true);
	platform_set_drvdata(pdev, rtc);

	rtc->rtc_dev = devm_rtc_allocate_device(&pdev->dev);
	if (IS_ERR(rtc->rtc_dev)) {
		ret = PTR_ERR(rtc->rtc_dev);
		goto err_clk;
	}

	rtc->rtc_dev->ops = &aml_rtc_ops;

	ret = rtc_add_group(rtc->rtc_dev, &meson_rtc_sysfs_files);
	if (ret) {
		dev_err_probe(&pdev->dev, ret, "Failed to add RTC group: %d\n", ret);
		goto err_clk;
	}

	ret = devm_rtc_register_device(rtc->rtc_dev);
	if (ret) {
		dev_err_probe(&pdev->dev, ret, "Failed to register RTC device: %d\n", ret);
		goto err_clk;
	}

	return 0;
err_clk:
	if (!rtc->rtc_virtual)
		clk_disable_unprepare(rtc->sys_clk);

	return ret;
}

static int __maybe_unused aml_rtc_suspend(struct device *dev)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	int ret = 0;

	if (!rtc->wakealrm_enabled)
		return ret;

	if (device_may_wakeup(dev))
		ret = enable_irq_wake(rtc->irq);

	return ret;
}

static int __maybe_unused aml_rtc_resume(struct device *dev)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(dev);
	int ret = 0;

	if (!rtc->wakealrm_enabled)
		return ret;

	if (device_may_wakeup(dev))
		ret = disable_irq_wake(rtc->irq);
	/* alarm fired, handler donot executed, trigger it munally */
	/* clean 'rtc->alarm' to allow a new .set_alarm to the upper RTC layer */
	if (!meson_read_alarm(rtc))
		aml_rtc_alarm_handler(rtc->irq, (void *)rtc);

	return ret;
}

static SIMPLE_DEV_PM_OPS(aml_rtc_pm_ops,
			 aml_rtc_suspend, aml_rtc_resume);

static void aml_rtc_shutdown(struct platform_device *pdev)
{
	struct aml_rtc_data *rtc = dev_get_drvdata(&pdev->dev);
	struct rtc_wkalrm aie_timer;
	u32 rtc_time;
	u64 alarm_sec = 0;
	int ret;

	/* Temporarily use: store rtc time when reboot, prevent rtc time being   */
	/* reset by reboot, it can be removed if rtc can preserve time in reboot */
	/* procedure, only rtc needed */
	rtc_time = meson_read_time(rtc);
	if (!rtc->rtc_virtual && rtc->mbox_chan) {
		ret = aml_mbox_transfer_data(rtc->mbox_chan, MBOX_CMD_SET_RTC,
			&rtc_time, sizeof(rtc_time), NULL, 0, MBOX_SYNC);
		if (ret < 0)
			dev_err(&pdev->dev, "Fail to set rtc time\n");
	}

	if (!rtc->wakealrm_enabled)
		return;

	/* only aie timer can wakeup system when poweroff */
	ret = rtc_read_alarm(rtc->rtc_dev, &aie_timer);
	if (ret) {
		dev_err(&pdev->dev, "Failed to read rtc alarm\n");
		return;
	}
	alarm_sec = rtc_tm_to_time64(&aie_timer.time);
	if (alarm_sec > rtc_time && aie_timer.enabled) {
		alarm_sec -= rtc_time;
		/* enable rtc alarm */
		meson_enable_alarm(rtc, 1);
		/* set alarm to reg */
		meson_set_alarm(rtc, alarm_sec);

		dev_info(&pdev->dev, "system will wakeup %llus later\n", alarm_sec);
	}
}

static const struct of_device_id meson_rtc_dt_match[] = {
	{ .compatible = "amlogic,meson-rtc"},
	{}
};
MODULE_DEVICE_TABLE(of, meson_rtc_dt_match);

static struct platform_driver meson_rtc_driver = {
	.probe = aml_rtc_probe,
	.driver = {
		.name = "meson-rtc",
		.of_match_table = meson_rtc_dt_match,
		.pm = &aml_rtc_pm_ops,
	},
	.shutdown = aml_rtc_shutdown,
};

int __init rtc_init(void)
{
	return platform_driver_register(&meson_rtc_driver);
}

void __exit rtc_exit(void)
{
	platform_driver_unregister(&meson_rtc_driver);
}
