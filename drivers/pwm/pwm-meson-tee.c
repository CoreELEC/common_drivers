// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/bitfield.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/amlogic/arm-smccc.h>
#include <linux/amlogic/pwm-meson-tee.h>
#include <linux/amlogic/secure_pwm_i2c.h>
#include <linux/pinctrl/consumer.h>

#define TEE_RW_NODE				1
#define DOUBLE_CHAN_COMPAT

struct meson_pwm_tee *to_meson_pwm_tee(struct pwm_chip *chip)
{
	return pwmchip_get_drvdata(chip);
}
EXPORT_SYMBOL(to_meson_pwm_tee);

static int meson_pwm_tee_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm_tee *meson = to_meson_pwm_tee(chip);
	struct meson_pwm_tee_channel *channel = &meson->channels[pwm->hwpwm];
	struct device *dev = &chip->dev;
	int err;

#ifdef DOUBLE_CHAN_COMPAT
	if (pwm->hwpwm != CHANNEL_MAIN) {
		channel->rate = clk_get_rate(channel->clk);
		return 0;
	}
#endif
	err = clk_prepare_enable(channel->clk);
	if (err < 0) {
		dev_err(dev, "failed to enable clock %s: %d\n",
			__clk_get_name(channel->clk), err);
		return err;
	}
	channel->rate = clk_get_rate(channel->clk);
	if (channel->rate == 0) {
		dev_err(&meson->chip->dev, "invalid source clock frequency\n");
		return -EINVAL;
	}
	return 0;
}

static void meson_pwm_tee_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm_tee *meson = to_meson_pwm_tee(chip);
	struct meson_pwm_tee_channel *channel = &meson->channels[pwm->hwpwm];
#ifdef DOUBLE_CHAN_COMPAT
		if (pwm->hwpwm != CHANNEL_MAIN)
			return;
#endif
	clk_disable_unprepare(channel->clk);
}

static int pwm_constant_enable_tee(struct meson_pwm_tee *meson, struct pwm_device *pwm)
{
#ifndef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
	struct arm_smccc_res res;
	struct meson_pwm_tee_channel *channel = &meson->channels[pwm->hwpwm];

	arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
				 SECID_PWM_CONSTANT_EN, FIELD_PREP(PWM_HIGH_MASK, channel->hi) |
		FIELD_PREP(PWM_LOW_MASK, channel->lo), 0, 0, 0, &res);
#else
	writel(readl(meson->base + PWM_MISC_REG) | MISC_CONSTANT,
		meson->base + PWM_MISC_REG);
#endif
	return 0;
}

static int pwm_constant_disable_tee(struct meson_pwm_tee *meson, struct pwm_device *pwm)
{
#ifndef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
	struct arm_smccc_res res;
	struct meson_pwm_tee_channel *channel = &meson->channels[pwm->hwpwm];

	arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
			SECID_PWM_CONSTANT_DIS, FIELD_PREP(PWM_HIGH_MASK, channel->hi) |
		FIELD_PREP(PWM_LOW_MASK, channel->lo), 0, 0, 0, &res);
#else
	writel(readl(meson->base + PWM_MISC_REG) & (~MISC_CONSTANT),
		meson->base + PWM_MISC_REG);

#endif
	return 0;
}

static int meson_pwm_tee_calc(struct meson_pwm_tee *meson, struct pwm_device *pwm,
			  const struct pwm_state *state)
{
	struct meson_pwm_tee_channel *channel = &meson->channels[pwm->hwpwm];
	unsigned int cnt, duty_cnt;
	unsigned long fin_freq;
	u64 duty, period;

	duty = state->duty_cycle;
	period = state->period;

	/*
	 * Note this is wrong. The result is an output wave that isn't really
	 * inverted and so is wrongly identified by .get_state as normal.
	 * Fixing this needs some care however as some machines might rely on
	 * this.
	 */
	if (state->polarity == PWM_POLARITY_INVERSED)
		duty = period - duty;
	fin_freq = channel->rate;
	if (fin_freq == 0) {
		dev_err(&meson->chip->dev, "invalid source clock frequency\n");
		return -EINVAL;
	}

	dev_dbg(&meson->chip->dev, "fin_freq: %lu Hz\n", fin_freq);
	if (meson->no_change_clk)
		channel->pre_div = 0;
	else
		channel->pre_div = DIV64_U64_ROUND_CLOSEST((u64)fin_freq * (u64)period,
							(u64)NSEC_PER_SEC * 0xffffLL);
	cnt = DIV64_U64_ROUND_CLOSEST((u64)fin_freq * (u64)period,
			(u64)NSEC_PER_SEC * (u64)(channel->pre_div + 1));
	if (cnt > 0xffff) {
		dev_err(&meson->chip->dev, "unable to get period cnt\n");
		return -EINVAL;
	}

	dev_dbg(&meson->chip->dev, "period=%llu cnt=%u\n", period, cnt);

	if (duty == period) {
		channel->hi = cnt;
		channel->lo = 0;
	} else if (duty == 0) {
		channel->hi = 0;
		channel->lo = cnt;
	} else {
		duty_cnt = DIV64_U64_ROUND_CLOSEST(fin_freq * duty,
			NSEC_PER_SEC * (channel->pre_div + 1));

		dev_dbg(&meson->chip->dev, "duty=%llu duty_cnt=%u\n", duty, duty_cnt);
		if (duty_cnt == 0)
			duty_cnt++;

		channel->hi = duty_cnt - 1;
		channel->lo = cnt - duty_cnt - 1;
	}

	return 0;
}

static void meson_pwm_tee_enable(struct meson_pwm_tee *meson, struct pwm_device *pwm)
{
	struct meson_pwm_tee_channel *channel = &meson->channels[pwm->hwpwm];
	u32 value;
#ifndef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
	struct arm_smccc_res res;
#endif
	unsigned long set_clk;
	u32 err;

	/*when tee is locked, clk rate fixed and should not be changed*/
	// err = clk_set_rate(channel->clk, channel->rate);
	// if (err)
	// dev_err(&meson->chip->dev, "setting clock rate failed\n");
	value = FIELD_PREP(PWM_HIGH_MASK, channel->hi) |
		FIELD_PREP(PWM_LOW_MASK, channel->lo);
#ifdef DOUBLE_CHAN_COMPAT
	switch (pwm->hwpwm) {
	case CHANNEL_MAIN:
	#ifdef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
		writel(value, meson->base + REG_PWM);
		writel(readl(meson->base + PWM_MISC_REG) | MISC_EN,
			meson->base + PWM_MISC_REG);
	#else
		arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
				 SECID_PWM_ENABLE_MAIN, value, 0, 0, 0, &res);
	#endif
	break;
	case CHANNEL_SUB:
	#ifdef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
		writel(value, meson->base + REG_PWM_SUB);
		writel(readl(meson->base + PWM_MISC_REG) | MISC_EN_SUB,
			meson->base + PWM_MISC_REG);
	#else
		arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
				 SECID_PWM_ENABLE_SUB, value, 0, 0, 0, &res);
	#endif
		break;
	default:
		break;
	}
#else
	#ifdef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
	writel(value, meson->base + REG_PWM);
	#else
	arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
				 SECID_PWM_ENABLE_MAIN, value, 0, 0, 0, &res);
	#endif
#endif

	if (meson->no_change_clk)
		return;
	/*get_clk_rate()not use in Interrupt context*/
	if (channel->clk_div == channel->pre_div)
		return;

	set_clk = channel->rate;
	if (set_clk == 0)
		dev_err(&meson->chip->dev, "invalid source clock frequency\n");

	set_clk /= (channel->pre_div + 1);
	err = clk_set_rate(channel->clk, set_clk);
	if (err) {
		pr_err("%s: error in setting pwm clk rate!\n", __func__);
		return;
	}
	channel->clk_div = channel->pre_div;
	channel->rate = set_clk;
}

static void meson_pwm_tee_disable(struct meson_pwm_tee *meson, struct pwm_device *pwm)
{
#ifndef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
	struct arm_smccc_res res;
#endif

#ifdef DOUBLE_CHAN_COMPAT
	switch (pwm->hwpwm) {
	case CHANNEL_MAIN:
	#ifdef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
		writel(readl(meson->base + PWM_MISC_REG) & (~MISC_EN),
			meson->base + PWM_MISC_REG);
	#else
		arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
				 SECID_PWM_DISABLE_MAIN,  0, 0, 0, 0, &res);
	#endif
		break;
	case CHANNEL_SUB:
	#ifdef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
		writel(readl(meson->base + PWM_MISC_REG) & (~MISC_EN_SUB),
			meson->base + PWM_MISC_REG);
	#else
		arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
				 SECID_PWM_DISABLE_SUB,  0, 0, 0, 0, &res);
		break;
	#endif
	default:
		break;
	}
#else
	#ifdef CONFIG_AMLOGIC_PWM_MESON_TEE_NO_SMC
		writel(readl(meson->base + PWM_MISC_REG) & (~MISC_EN),
			meson->base + PWM_MISC_REG);
	#else
	arm_smccc_smc(meson->smc_id, SECID_PWM, meson->tee_id,
				 SECID_PWM_DISABLE_MIAN, 0, 0, 0, 0, &res);
	#endif
#endif
}

static int meson_pwm_tee_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			   const struct pwm_state *state)
{
	struct meson_pwm_tee *meson = to_meson_pwm_tee(chip);
	struct meson_pwm_tee_channel *channel = &meson->channels[pwm->hwpwm];
	int err = 0;

	if (!state->enabled) {
		if (state->polarity == PWM_POLARITY_INVERSED) {
			/*
			 * This IP block revision doesn't have an "always high"
			 * setting which we can use for "inverted disabled".
			 * Instead we achieve this by setting mux parent with
			 * highest rate and minimum divider value, resulting
			 * in the shortest possible duration for one "count"
			 * and "period == duty_cycle". This results in a signal
			 * which is LOW for one "count", while being HIGH for
			 * the rest of the (so the signal is HIGH for slightly
			 * less than 100% of the period, but this is the best
			 * we can achieve).
			 */
			channel->rate = ULONG_MAX;
			channel->hi = ~0;
			channel->lo = 0;

			meson_pwm_tee_enable(meson, pwm);
		} else {
			meson_pwm_tee_disable(meson, pwm);
		}
	} else {
		err = meson_pwm_tee_calc(meson, pwm, state);
		if (err < 0)
			return err;

		meson_pwm_tee_enable(meson, pwm);
		if (state->duty_cycle == state->period || state->duty_cycle == 0)
			pwm_constant_enable_tee(meson, pwm);
		else
			pwm_constant_disable_tee(meson, pwm);
	}

	return 0;
}

static u64 meson_pwm_tee_cnt_to_ns(struct pwm_chip *chip, struct pwm_device *pwm,
			       u32 cnt)
{
	struct meson_pwm_tee *meson = to_meson_pwm_tee(chip);
	struct meson_pwm_tee_channel *channel;
	unsigned long fin_freq;

	/* to_meson_pwm_tee() can only be used after .get_state() is called */
	channel = &meson->channels[pwm->hwpwm];

	fin_freq = clk_get_rate(channel->clk);
	if (fin_freq == 0)
		return 0;

	return DIV_ROUND_CLOSEST_ULL(NSEC_PER_SEC * (u64)cnt, fin_freq);
}

static int meson_pwm_tee_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
			       struct pwm_state *state)
{
	struct meson_pwm_tee *meson = to_meson_pwm_tee(chip);
	struct meson_pwm_tee_channel *channel;
	u32 value;
	bool constant_enabled;

#ifdef DOUBLE_CHAN_COMPAT
		if (pwm->hwpwm != 0)
			return -EINVAL;
#endif
	if (!state)
		return -EINVAL;

	channel = &meson->channels[pwm->hwpwm];

	value = readl(meson->base + PWM_MISC_REG);
	state->enabled = value & MISC_EN;
	constant_enabled = (value & MISC_CONSTANT) != 0;
	value = readl(meson->base + REG_PWM);
	channel->lo = FIELD_GET(PWM_LOW_MASK, value);
	channel->hi = FIELD_GET(PWM_HIGH_MASK, value);
	state->period = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->lo + channel->hi);
	state->duty_cycle = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->hi);
	if (channel->lo == 0) {
		if (constant_enabled) {
			state->period = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->hi);
			state->duty_cycle = state->period;
		} else {
			state->period = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->hi + 2);
			state->duty_cycle = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->hi + 1);
		}
	} else if (channel->hi == 0) {
		if (constant_enabled) {
			state->period = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->lo);
			state->duty_cycle = 0;
		} else {
			state->period = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->lo + 2);
			state->duty_cycle = meson_pwm_tee_cnt_to_ns(chip, pwm, channel->hi + 1);
		}
	} else {
		state->period = meson_pwm_tee_cnt_to_ns(chip, pwm,
							channel->lo + channel->hi + 2);
		state->duty_cycle = meson_pwm_tee_cnt_to_ns(chip, pwm,
							channel->hi + 1);
	}
	state->polarity = PWM_POLARITY_NORMAL;
	dev_dbg(&chip->dev, "%s, get pwm state period: %llu ns, duty: %llu ns\n",
					__func__, state->period, state->duty_cycle);
	return 0;
}

static const struct pwm_ops meson_pwm_tee_ops = {
	.request = meson_pwm_tee_request,
	.free = meson_pwm_tee_free,
	.apply = meson_pwm_tee_apply,
	.get_state = meson_pwm_tee_get_state,
};

static const struct of_device_id meson_pwm_tee_matches[] = {
	{
		.compatible = "amlogic,meson-pwm-tee",
	},
	{},
};
MODULE_DEVICE_TABLE(of, meson_pwm_tee_matches);

static int meson_pwm_tee_init_channels(struct meson_pwm_tee *meson)
{
	struct device *dev = pwmchip_parent(meson->chip);
	char name[255];
	struct device_node *np = dev->of_node;

	snprintf(name, sizeof(name), "clkin%u", 0);
	meson->channels[CHANNEL_MAIN].clk = of_clk_get(np, 0);
	if (IS_ERR(meson->channels[CHANNEL_MAIN].clk)) {
		dev_err(&meson->chip->dev, "can't get device clock\n");
		return PTR_ERR(meson->channels[CHANNEL_MAIN].clk);
	}
#ifdef DOUBLE_CHAN_COMPAT
	meson->channels[CHANNEL_SUB].clk = meson->channels[CHANNEL_MAIN].clk;
#endif
	return 0;
}

static int meson_pwm_tee_probe(struct platform_device *pdev)
{
	struct meson_pwm_tee *meson;
	struct pwm_chip *chip;
	int err;
	struct resource *regs;

	chip = devm_pwmchip_alloc(&pdev->dev, MESON_NUM_PWMS_TEE, sizeof(*meson));
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	meson = to_meson_pwm_tee(chip);
	meson->chip = chip;
	meson->base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(meson->base))
		return PTR_ERR(meson->base);
	/* get pwm tee_id property */
	err = of_property_read_u32(pdev->dev.of_node, "tee_id",
			   &meson->tee_id);
	if (err) {
		dev_err(&pdev->dev, "not config tee_id\n");
		return err;
	}
	if (of_property_read_u32(pdev->dev.of_node, "arm,smc-id", &meson->smc_id))
		meson->smc_id = SECURE_PWM_I2C;
	if (fwnode_property_present(&pdev->dev.of_node->fwnode, "change-clk-intern"))
		meson->no_change_clk = 0;
	else
		meson->no_change_clk = 1;
	// meson->base = devm_platform_ioremap_resource(pdev, 0);

	spin_lock_init(&meson->lock);
	meson->chip->ops = &meson_pwm_tee_ops;
	meson->chip->atomic = true;

	err = meson_pwm_tee_init_channels(meson);
	if (err < 0)
		return err;

	err = pwmchip_add(meson->chip);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register PWM chip: %d\n", err);
		return err;
	}
	platform_set_drvdata(pdev, meson);
#ifdef DOUBLE_CHAN_COMPAT
	meson_pwm_sysfs_init(&pdev->dev, TEE_RW_NODE);
#endif

	return 0;
}

#ifdef CONFIG_HIBERNATION
static int meson_pwm_tee_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_pwm_tee *meson = platform_get_drvdata(pdev);
	int i;

	pinctrl_pm_select_sleep_state(dev);
	for (i = 0; i < PWM_TEE_REG_NUMS; i++) {
		meson->regs_restore[i] = readl(meson->base + 4 * i);
		pr_debug("pwm freeze, reg%d: 0x%x\n", i, meson->regs_restore[i]);
	}

	return 0;
}

static int meson_pwm_tee_thaw(struct device *dev)
{
	return 0;
}

static int meson_pwm_tee_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct meson_pwm_tee *meson = platform_get_drvdata(pdev);
	int i;

	pinctrl_pm_select_default_state(dev);
	for (i = 0; i < PWM_TEE_REG_NUMS; i++) {
		writel(meson->regs_restore[i], meson->base + 4 * i);
		pr_debug("pwm restore, reg%d: 0x%x\n", i, meson->regs_restore[i]);
	}

	return 0;
}

static int meson_pwm_tee_pm_suspend(struct device *dev)
{
	return 0;
}

static int meson_pwm_tee_pm_resume(struct device *dev)
{
	return 0;
}

const struct dev_pm_ops meson_pwm_tee_pm = {
	.freeze		= meson_pwm_tee_freeze,
	.thaw		= meson_pwm_tee_thaw,
	.restore	= meson_pwm_tee_restore,
	.suspend	= meson_pwm_tee_pm_suspend,
	.resume		= meson_pwm_tee_pm_resume,
};
#endif

static struct platform_driver meson_pwm_tee_driver = {
	.driver = {
		.name = "meson-pwm-tee",
		.of_match_table = meson_pwm_tee_matches,
#ifdef CONFIG_HIBERNATION
		.pm = &meson_pwm_tee_pm,
#endif
	},
	.probe = meson_pwm_tee_probe,
};

int __init pwm_meson_tee_init(void)
{
	return platform_driver_register(&meson_pwm_tee_driver);
}

void __exit pwm_meson_tee_exit(void)
{
	platform_driver_unregister(&meson_pwm_tee_driver);
}

MODULE_DESCRIPTION("Amlogic Meson PWM Generator driver");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("Dual BSD/GPL");
