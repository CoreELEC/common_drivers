// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * PWM controller driver for Amlogic Meson SoCs.
 *
 * This PWM is only a set of Gates, Dividers and Counters:
 * PWM output is achieved by calculating a clock that permits calculating
 * two periods (low and high). The counter then has to be set to switch after
 * N cycles for the first half period.
 * The hardware has no "polarity" setting. This driver reverses the period
 * cycles (the low length is inverted with the high length) for
 * PWM_POLARITY_INVERSED. This means that .get_state cannot read the polarity
 * from the hardware.
 * Setting the duty cycle will disable and re-enable the PWM output.
 * Disabling the PWM stops the output immediately (without waiting for the
 * current period to complete first).
 *
 * The public S912 (GXM) datasheet contains some documentation for this PWM
 * controller starting on page 543:
 * https://dl.khadas.com/Hardware/VIM2/Datasheet/S912_Datasheet_V0.220170314publicversion-Wesion.pdf
 * An updated version of this IP block is found in S922X (G12B) SoCs. The
 * datasheet contains the description for this IP block revision starting at
 * page 1084:
 * https://dn.odroid.com/S922X/ODROID-N2/Datasheet/S922X_Public_Datasheet_V0.2.pdf
 *
 * Copyright (c) 2016 BayLibre, SAS.
 * Author: Neil Armstrong <narmstrong@baylibre.com>
 * Copyright (C) 2014 Amlogic, Inc.
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

#ifdef CONFIG_AMLOGIC_MODIFY
#include <linux/amlogic/pwm-meson.h>

//#define FOR_BACK_TRACE
//#define M_DEBUG
#ifdef M_DEBUG
#define DEBUG
#define PWM_DBG(fmt, ...) pr_info("%s ," fmt, "[MESON PWM]", ##__VA_ARGS__)
#else
#define PWM_DBG(fmt, ...)
#endif /*endif M_DEBUG*/

#else
#define REG_PWM_A		0x0
#define REG_PWM_B		0x4

#define PWM_LOW_MASK		GENMASK(15, 0)
#define PWM_HIGH_MASK		GENMASK(31, 16)

#define REG_MISC_AB		0x8
#define MISC_B_CLK_EN		BIT(23)
#define MISC_A_CLK_EN		BIT(15)
#define MISC_CLK_DIV_MASK	0x7f
#define MISC_B_CLK_DIV_SHIFT	16
#define MISC_A_CLK_DIV_SHIFT	8
#define MISC_B_CLK_SEL_SHIFT	6
#define MISC_A_CLK_SEL_SHIFT	4
#define MISC_CLK_SEL_MASK	0x3
#define MISC_B_EN		BIT(1)
#define MISC_A_EN		BIT(0)

#define MESON_NUM_PWMS		2

struct meson_pwm_channel {
	unsigned int hi;
	unsigned int lo;
	u8 pre_div;

	struct clk *clk_parent;
	struct clk_mux mux;
	struct clk *clk;
};

struct meson_pwm_data {
	const char * const *parent_names;
	unsigned int num_parents;
};

struct meson_pwm {
	struct pwm_chip chip;
	const struct meson_pwm_data *data;
	struct meson_pwm_channel channels[MESON_NUM_PWMS];
	void __iomem *base;
	/*
	 * Protects register (write) access to the REG_MISC_AB register
	 * that is shared between the two PWMs.
	 */
	spinlock_t lock;
};
#endif /*endif CONFIG_AMLOGIC_MODIFY*/

static struct meson_pwm_channel_data {
	u8		reg_offset;
	u8		clk_sel_shift;
	u8		clk_div_shift;
	u32		clk_en_mask;
	u32		pwm_en_mask;
#ifdef CONFIG_AMLOGIC_MODIFY
	/*external clk*/
	u8		ext_clk_div_shift;
	u32		ext_clk_en_mask;
} meson_pwm_per_channel_data[] = {
#else
} meson_pwm_per_channel_data[MESON_NUM_PWMS] = { /*need add external clk*/
#endif
	{
		.reg_offset	= REG_PWM_A,
		.clk_sel_shift	= MISC_A_CLK_SEL_SHIFT,
		.clk_div_shift	= MISC_A_CLK_DIV_SHIFT,
		.clk_en_mask	= MISC_A_CLK_EN,
		.pwm_en_mask	= MISC_A_EN,
#ifdef CONFIG_AMLOGIC_MODIFY
		.ext_clk_div_shift	= EXT_CLK_A_DIV_SHIFT,
		.ext_clk_en_mask	= EXT_CLK_A_EN,
#endif
	},
	{
		.reg_offset	= REG_PWM_B,
		.clk_sel_shift	= MISC_B_CLK_SEL_SHIFT,
		.clk_div_shift	= MISC_B_CLK_DIV_SHIFT,
		.clk_en_mask	= MISC_B_CLK_EN,
		.pwm_en_mask	= MISC_B_EN,
#ifdef CONFIG_AMLOGIC_MODIFY
		.ext_clk_div_shift	= EXT_CLK_B_DIV_SHIFT,
		.ext_clk_en_mask	= EXT_CLK_B_EN,
#endif
	},
#ifdef CONFIG_AMLOGIC_MODIFY
	{
		.reg_offset	= REG_PWM_A2,
		.clk_sel_shift	= MISC_A_CLK_SEL_SHIFT,
		.clk_div_shift	= MISC_A_CLK_DIV_SHIFT,
		.clk_en_mask	= MISC_A_CLK_EN,
		.pwm_en_mask	= MISC_A2_EN,
		.ext_clk_div_shift	= EXT_CLK_A_DIV_SHIFT,
		.ext_clk_en_mask	= EXT_CLK_A_EN,
	},
	{
		.reg_offset	= REG_PWM_B2,
		.clk_sel_shift	= MISC_B_CLK_SEL_SHIFT,
		.clk_div_shift	= MISC_B_CLK_DIV_SHIFT,
		.clk_en_mask	= MISC_B_CLK_EN,
		.pwm_en_mask	= MISC_B2_EN,
		.ext_clk_div_shift	= EXT_CLK_B_DIV_SHIFT,
		.ext_clk_en_mask	= EXT_CLK_B_EN,
	},
#endif
};

#ifdef CONFIG_AMLOGIC_MODIFY
struct meson_pwm *to_meson_pwm(struct pwm_chip *chip)
#else
static inline struct meson_pwm *to_meson_pwm(struct pwm_chip *chip)
#endif
{
	return container_of(chip, struct meson_pwm, chip);
}
EXPORT_SYMBOL(to_meson_pwm);

static int meson_pwm_request(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm *meson = to_meson_pwm(chip);
	struct meson_pwm_channel *channel;
	struct device *dev = chip->dev;
	int err;

	channel = pwm_get_chip_data(pwm);
	if (channel)
		return 0;

	channel = &meson->channels[pwm->hwpwm];
#ifdef CONFIG_AMLOGIC_MODIFY
	if (!meson->data->extern_clk)
#endif
		if (channel->clk_parent) {
			err = clk_set_parent(channel->clk, channel->clk_parent);
			if (err < 0) {
				dev_err(dev, "failed to set parent %s for %s: %d\n",
					__clk_get_name(channel->clk_parent),
					__clk_get_name(channel->clk), err);
					return err;
			}
		}

#ifdef CONFIG_AMLOGIC_MODIFY
	if (meson->data->extern_clk) {
		return pwm_set_chip_data(pwm, channel);
	}
#endif

	err = clk_prepare_enable(channel->clk);
	if (err < 0) {
		dev_err(dev, "failed to enable clock %s: %d\n",
			__clk_get_name(channel->clk), err);
		return err;
	}

#ifdef CONFIG_AMLOGIC_MODIFY
	channel->clk_rate = clk_get_rate(channel->clk);
	if (FCLK_DIV4_CLK - 100 <= channel->clk_rate && channel->clk_rate <= FCLK_DIV4_CLK + 100)
		channel->clk_rate = FCLK_DIV4_CLK;
#endif

	return pwm_set_chip_data(pwm, channel);
}

static void meson_pwm_free(struct pwm_chip *chip, struct pwm_device *pwm)
{
	struct meson_pwm *meson = to_meson_pwm(chip);
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);

	if (meson->data->extern_clk)
		return;
	if (channel)
		clk_disable_unprepare(channel->clk);

}

static int meson_pwm_calc(struct meson_pwm *meson, struct pwm_device *pwm,
			  const struct pwm_state *state)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);
	unsigned int duty, period, pre_div, cnt, duty_cnt;
	unsigned long fin_freq = -1;

	duty = state->duty_cycle;
	period = state->period;

	if (state->polarity == PWM_POLARITY_INVERSED)
		duty = period - duty;

#ifdef CONFIG_AMLOGIC_MODIFY
	fin_freq = channel->clk_rate;
#else
	/*clk_get_rate()not use in Interrupt context*/
	fin_freq = clk_get_rate(channel->clk);
#endif

	if (fin_freq == 0) {
		dev_err(meson->chip.dev, "invalid source clock frequency\n");
		return -EINVAL;
	}

	dev_dbg(meson->chip.dev, "fin_freq: %lu Hz\n", fin_freq);

	pre_div = DIV64_U64_ROUND_CLOSEST(fin_freq * (u64)period, NSEC_PER_SEC * 0xffffLL);
	if (pre_div > MISC_CLK_DIV_MASK) {
		dev_err(meson->chip.dev, "unable to get period pre_div\n");
		return -EINVAL;
	}

#if defined(CONFIG_AMLOGIC_MODIFY) && defined(FOR_BACK_TRACE)
	if (fin_freq == FCLK_DIV4_CLK)
		/*set it hard to div 5 then period cnt could be divided evenly
		 * 500Mhz/5 = 100MHz
		 */
		pre_div = 4;
	PWM_DBG("%s, fin_freq=%lu, pre_div=%d", __func__, fin_freq, pre_div + 1);
#endif

	cnt = DIV64_U64_ROUND_CLOSEST(fin_freq * (u64)period, NSEC_PER_SEC * (pre_div + 1));
	if (cnt > 0xffff) {
		dev_err(meson->chip.dev, "unable to get period cnt\n");
		return -EINVAL;
	}

	dev_dbg(meson->chip.dev, "period=%u pre_div=%u cnt=%u\n", period,
		pre_div, cnt);

	if (duty == period) {
		channel->pre_div = pre_div;
		channel->hi = cnt;
		channel->lo = 0;
	} else if (duty == 0) {
		channel->pre_div = pre_div;
		channel->hi = 0;
		channel->lo = cnt;
	} else {
		/* Then check is we can have the duty with the same pre_div */
		duty_cnt = DIV64_U64_ROUND_CLOSEST(fin_freq * (u64)duty,
				     NSEC_PER_SEC * (pre_div + 1));
		if (duty_cnt > 0xffff) {
			dev_err(meson->chip.dev, "unable to get duty cycle\n");
			return -EINVAL;
		}

		dev_dbg(meson->chip.dev, "duty=%u pre_div=%u duty_cnt=%u\n",
			duty, pre_div, duty_cnt);

		channel->pre_div = pre_div;
#ifndef CONFIG_AMLOGIC_MODIFY
		channel->hi = duty_cnt;
		channel->lo = cnt - duty_cnt;
#else
		if (duty_cnt == 0)
			duty_cnt++;

		channel->hi = duty_cnt - 1;
		channel->lo = cnt - duty_cnt - 1;
#endif
	}

#ifdef CONFIG_AMLOGIC_MODIFY
	/*
	 * duty_cycle equal 0% and 100%,constant should be enabled,
	 * high and low count will not incease one;
	 * otherwise, high and low count increase one.
	 */
	if (duty == period || duty == 0)
		pwm_constant_enable(meson, pwm->hwpwm);
	else
		pwm_constant_disable(meson, pwm->hwpwm);
#endif

	return 0;
}

static void meson_pwm_enable(struct meson_pwm *meson, struct pwm_device *pwm)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);
	struct meson_pwm_channel_data *channel_data;
	unsigned long flags;
	u32 value;

	channel_data = &meson_pwm_per_channel_data[pwm->hwpwm];

	spin_lock_irqsave(&meson->lock, flags);

	/*following filed are invalid on new soc*/
	value = readl(meson->base + REG_MISC_AB);
	value &= ~(MISC_CLK_DIV_MASK << channel_data->clk_div_shift);
	value |= channel->pre_div << channel_data->clk_div_shift;
	value |= channel_data->clk_en_mask;
	writel(value, meson->base + REG_MISC_AB);
	/*upper filed are invalid on new soc*/

	value = FIELD_PREP(PWM_HIGH_MASK, channel->hi) |
		FIELD_PREP(PWM_LOW_MASK, channel->lo);
	writel(value, meson->base + channel_data->reg_offset);

	value = readl(meson->base + REG_MISC_AB);
	value |= channel_data->pwm_en_mask;
	writel(value, meson->base + REG_MISC_AB);
#ifdef CONFIG_AMLOGIC_MODIFY
	PWM_DBG("%s,set pwm hi 0x%x, lo 0x%x\n", __func__, channel->hi, channel->lo);
	if (meson->data->extern_clk) {
		value = readl(meson->ext_clk_base);
		value &= ~(EXT_CLK_DIV_MASK << channel_data->ext_clk_div_shift);
		value |= channel->pre_div << channel_data->ext_clk_div_shift;
		value |= channel_data->ext_clk_en_mask;
		writel(value, meson->ext_clk_base);
		channel->clk_div = channel->pre_div;
	}
#endif
	spin_unlock_irqrestore(&meson->lock, flags);
}

static void meson_pwm_disable(struct meson_pwm *meson, struct pwm_device *pwm)
{
	unsigned long flags;
	u32 value;

	spin_lock_irqsave(&meson->lock, flags);

	value = readl(meson->base + REG_MISC_AB);
	value &= ~meson_pwm_per_channel_data[pwm->hwpwm].pwm_en_mask;
	writel(value, meson->base + REG_MISC_AB);

	spin_unlock_irqrestore(&meson->lock, flags);
}

static int meson_pwm_apply(struct pwm_chip *chip, struct pwm_device *pwm,
			   const struct pwm_state *state)
{
	struct meson_pwm_channel *channel = pwm_get_chip_data(pwm);
	struct meson_pwm *meson = to_meson_pwm(chip);
	int err = 0;

	if (!state)
		return -EINVAL;

	if (!state->enabled) {
		if (state->polarity == PWM_POLARITY_INVERSED) {
			/*
			 * This IP block revision doesn't have an "always high"
			 * setting which we can use for "inverted disabled".
			 * Instead we achieve this using the same settings
			 * that we use a pre_div of 0 (to get the shortest
			 * possible duration for one "count") and
			 * "period == duty_cycle". This results in a signal
			 * which is LOW for one "count", while being HIGH for
			 * the rest of the (so the signal is HIGH for slightly
			 * less than 100% of the period, but this is the best
			 * we can achieve).
			 */
			channel->pre_div = 0;
			channel->hi = ~0;
			channel->lo = 0;

			meson_pwm_enable(meson, pwm);
		} else {
			meson_pwm_disable(meson, pwm);
		}
	} else {
		err = meson_pwm_calc(meson, pwm, state);
		if (err < 0)
			return err;
#ifdef CONFIG_AMLOGIC_MODIFY
		PWM_DBG("%s, calc pwm state period: %llu ns, duty: %llu ns\n",
						__func__, state->period, state->duty_cycle);
#endif
		meson_pwm_enable(meson, pwm);
	}

	return 0;
}

static unsigned int meson_pwm_cnt_to_ns(struct pwm_chip *chip,
					struct pwm_device *pwm, u32 cnt)
{
	struct meson_pwm *meson = to_meson_pwm(chip);
	struct meson_pwm_channel *channel;
	unsigned long fin_freq;
	u32 fin_ns;
#ifdef CONFIG_AMLOGIC_MODIFY
	unsigned long div_fin_freq;

	/* to_meson_pwm() can only be used after .get_state() is called */
	channel = &meson->channels[pwm->hwpwm];
	if (meson->data->extern_clk)
		fin_freq = channel->clk_rate;
	else
		fin_freq = clk_get_rate(channel->clk);
	div_fin_freq = DIV_ROUND_CLOSEST_ULL(fin_freq, channel->pre_div + 1);
	fin_ns = DIV_ROUND_CLOSEST_ULL(NSEC_PER_SEC, div_fin_freq);
	PWM_DBG("%s,the final clk rate:%uns pr_div:%d", __func__, fin_ns, channel->pre_div + 1);

	return cnt * fin_ns;
#else
	/* to_meson_pwm() can only be used after .get_state() is called */
	channel = &meson->channels[pwm->hwpwm];
	fin_freq = clk_get_rate(channel->clk);
	if (fin_freq == 0)
		return 0;

	fin_ns = DIV_ROUND_CLOSEST_ULL(NSEC_PER_SEC, fin_freq);

	return cnt * fin_ns * (channel->pre_div + 1);
#endif
}

#ifdef CONFIG_AMLOGIC_MODIFY
static void meson_v2_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				   struct pwm_state *state)
{
	struct meson_pwm *meson = to_meson_pwm(chip);
	struct meson_pwm_channel *channel;
	struct meson_pwm_channel_data *channel_data;
	u32 value, tmp_value, en_mask, constant_mask;
	bool constant_enabled;

	channel = &meson->channels[pwm->hwpwm];
	channel_data = &meson_pwm_per_channel_data[pwm->hwpwm];
	if (!state)
		return;

	switch (pwm->hwpwm) {
	case 0:
		en_mask = MISC_A_EN;
		constant_mask = MISC_A_CONSTANT;
		break;

	case 1:
		en_mask = MISC_B_EN;
		constant_mask = MISC_B_CONSTANT;
		break;

	case 2:
		en_mask = MISC_A2_EN;
		constant_mask = MISC_A_CONSTANT;
		break;

	case 3:
		en_mask = MISC_B2_EN;
		constant_mask = MISC_B_CONSTANT;

	default:
		return;
	}
	value = readl(meson->base + REG_MISC_AB);
	tmp_value = value;
	state->enabled = (value & en_mask) != 0;
	constant_enabled = (value & constant_mask) != 0;
#ifdef FOR_BACK_TRACE
	value = readl(meson->base + channel_data->reg_offset);
	channel->lo = FIELD_GET(PWM_LOW_MASK, value);
	channel->hi = FIELD_GET(PWM_HIGH_MASK, value);
	PWM_DBG("%s, get pwm state hi 0x%x, li 0x%x\n", __func__, channel->hi, channel->lo);
	if (meson->data->extern_clk) {
		value = readl(meson->ext_clk_base);
		tmp_value = value >> channel_data->ext_clk_div_shift;
		channel->pre_div = FIELD_GET(EXT_CLK_DIV_MASK, tmp_value);
	}
	if (channel->lo == 0) {
		if (constant_enabled) {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->hi);
			state->duty_cycle = state->period;
		} else {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->hi + 2);
			state->duty_cycle = meson_pwm_cnt_to_ns(chip, pwm, channel->hi + 1);
		}
	} else if (channel->hi == 0) {
		if (constant_enabled) {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->lo);
			state->duty_cycle = 0;
		} else {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->lo + 2);
			state->duty_cycle = meson_pwm_cnt_to_ns(chip, pwm, channel->hi + 1);
		}
	} else {
		state->period = meson_pwm_cnt_to_ns(chip, pwm,
						    channel->lo + channel->hi + 2);
		state->duty_cycle = meson_pwm_cnt_to_ns(chip, pwm,
							channel->hi + 1);
	}
	PWM_DBG("%s, get pwm state period: %lluns, duty: %lluns\n",
					__func__, state->period, state->duty_cycle);
#endif /*FOR_BACK_TRACE*/
}
#endif

static void meson_pwm_get_state(struct pwm_chip *chip, struct pwm_device *pwm,
				struct pwm_state *state)
{
	struct meson_pwm *meson = to_meson_pwm(chip);
	struct meson_pwm_channel_data *channel_data;
	struct meson_pwm_channel *channel;
	u32 value, tmp;
#ifdef CONFIG_AMLOGIC_MODIFY
	bool constant_enabled;
	u32 constant_mask;
#endif

	if (!state)
		return;

#ifdef CONFIG_AMLOGIC_MODIFY
	if (meson->data->extern_clk)
		return meson_v2_pwm_get_state(chip, pwm, state);
	switch (pwm->hwpwm) {
	case 0:
		constant_mask = MISC_A_CONSTANT;
		break;

	case 1:
		constant_mask = MISC_B_CONSTANT;
		break;

	case 2:
		constant_mask = MISC_A_CONSTANT;
		break;

	case 3:
		constant_mask = MISC_B_CONSTANT;

	default:
		return;
	}
#endif

	channel = &meson->channels[pwm->hwpwm];
	channel_data = &meson_pwm_per_channel_data[pwm->hwpwm];

	value = readl(meson->base + REG_MISC_AB);

	tmp = channel_data->pwm_en_mask | channel_data->clk_en_mask;
	state->enabled = (value & tmp) == tmp;
#ifdef CONFIG_AMLOGIC_MODIFY
	constant_enabled = (value & constant_mask) != 0;
#endif
	tmp = value >> channel_data->clk_div_shift;
	channel->pre_div = FIELD_GET(MISC_CLK_DIV_MASK, tmp);

	value = readl(meson->base + channel_data->reg_offset);

	channel->lo = FIELD_GET(PWM_LOW_MASK, value);
	channel->hi = FIELD_GET(PWM_HIGH_MASK, value);
#if defined(CONFIG_AMLOGIC_MODIFY) && defined(FOR_BACK_TRACE)
	PWM_DBG("%s, get pwm state hi 0x%x, li 0x%x\n", __func__, channel->hi, channel->lo);
	if (channel->lo == 0) {
		if (constant_enabled) {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->hi);
			state->duty_cycle = state->period;
		} else {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->hi + 2);
			state->duty_cycle = meson_pwm_cnt_to_ns(chip, pwm, channel->hi + 1);
		}
	} else if (channel->hi == 0) {
		if (constant_enabled) {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->lo);
			state->duty_cycle = 0;
		} else {
			state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->lo + 2);
			state->duty_cycle = meson_pwm_cnt_to_ns(chip, pwm, channel->hi + 1);
		}
	} else {
		state->period = meson_pwm_cnt_to_ns(chip, pwm,
							channel->lo + channel->hi + 2);
		state->duty_cycle = meson_pwm_cnt_to_ns(chip, pwm,
							channel->hi + 1);
	}
	PWM_DBG("%s, get pwm state period: %lluns, duty: %lluns\n",
					__func__, state->period, state->duty_cycle);
#else
	if (channel->lo == 0) {
		state->period = meson_pwm_cnt_to_ns(chip, pwm, channel->hi);
		state->duty_cycle = state->period;
	} else if (channel->lo >= channel->hi) {
		state->period = meson_pwm_cnt_to_ns(chip, pwm,
						    channel->lo + channel->hi);
		state->duty_cycle = meson_pwm_cnt_to_ns(chip, pwm,
							channel->hi);
	} else {
		state->period = 0;
		state->duty_cycle = 0;
	}
#endif
}

static const struct pwm_ops meson_pwm_ops = {
	.request = meson_pwm_request,
	.free = meson_pwm_free,
	.apply = meson_pwm_apply,
	.get_state = meson_pwm_get_state,
	.owner = THIS_MODULE,
};

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
static char *pwm_meson8b_parent_names[] __initdata = {
	"xtal", "vid_pll", "fclk_div4", "fclk_div3"
};

static struct meson_pwm_data pwm_meson8b_data __refdata = {
	.parent_names = pwm_meson8b_parent_names,
	.num_parents = ARRAY_SIZE(pwm_meson8b_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = false,
#endif
};

static char *pwm_gxbb_parent_names[] __initdata = {
	"xtal", "hdmi_pll", "fclk_div4", "fclk_div3"
};

static struct meson_pwm_data pwm_gxbb_data __refdata = {
	.parent_names = pwm_gxbb_parent_names,
	.num_parents = ARRAY_SIZE(pwm_gxbb_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = false,
#endif
};

/*
 * Only the 2 first inputs of the GXBB AO PWMs are valid
 * The last 2 are grounded
 */
static char *pwm_gxbb_ao_parent_names[] __initdata = {
	"xtal", "clk81"
};

static struct meson_pwm_data pwm_gxbb_ao_data __refdata = {
	.parent_names = pwm_gxbb_ao_parent_names,
	.num_parents = ARRAY_SIZE(pwm_gxbb_ao_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = false,
#endif
};

static char *pwm_axg_ee_parent_names[] __initdata = {
	"xtal", "fclk_div5", "fclk_div4", "fclk_div3"
};

static struct meson_pwm_data pwm_axg_ee_data __refdata = {
	.parent_names = pwm_axg_ee_parent_names,
	.num_parents = ARRAY_SIZE(pwm_axg_ee_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = true,
#endif
};

static char *pwm_axg_ao_parent_names[] __initdata = {
	"xtal", "aoclk81", "fclk_div4", "fclk_div5"
};

static struct meson_pwm_data pwm_axg_ao_data __refdata = {
	.parent_names = pwm_axg_ao_parent_names,
	.num_parents = ARRAY_SIZE(pwm_axg_ao_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = true,
#endif
};
#endif

static char *pwm_g12a_ao_ab_parent_names[] __initdata = {
	"xtal", "aoclk81", "fclk_div4", "fclk_div5"
};

static struct meson_pwm_data pwm_g12a_ao_ab_data __refdata = {
	.parent_names = pwm_g12a_ao_ab_parent_names,
	.num_parents = ARRAY_SIZE(pwm_g12a_ao_ab_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = true,
#endif
};

static char *pwm_g12a_ao_cd_parent_names[] __initdata = {
	"xtal", "aoclk81",
};

static struct meson_pwm_data pwm_g12a_ao_cd_data __refdata = {
	.parent_names = pwm_g12a_ao_cd_parent_names,
	.num_parents = ARRAY_SIZE(pwm_g12a_ao_cd_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = true,
#endif
};

static char *pwm_g12a_ee_parent_names[] __initdata = {
	"xtal", "hdmi_pll", "fclk_div4", "fclk_div3"
};

static struct meson_pwm_data pwm_g12a_ee_data __refdata = {
	.parent_names = pwm_g12a_ee_parent_names,
	.num_parents = ARRAY_SIZE(pwm_g12a_ee_parent_names),
#ifdef CONFIG_AMLOGIC_MODIFY
	.double_channel = true,
#endif
};

#ifdef CONFIG_AMLOGIC_MODIFY
static  char *pwm_t5d_parent_names[] __initdata = {
	"xtal", "clk81", "fclk_div4", "fclk_div5"
};

static struct meson_pwm_data pwm_t5d_data __refdata = {
	.parent_names = pwm_t5d_parent_names,
	.num_parents = ARRAY_SIZE(pwm_t5d_parent_names),
	.double_channel = true,
};

static struct meson_pwm_data pwm_v2_data __initdata = {
	.double_channel = true,
	.extern_clk = true,
};
#endif

static const struct of_device_id meson_pwm_matches[] = {
#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	{
		.compatible = "amlogic,meson8b-pwm",
		.data = &pwm_meson8b_data
	},
	{
		.compatible = "amlogic,meson-gxbb-pwm",
		.data = &pwm_gxbb_data
	},
	{
		.compatible = "amlogic,meson-gxbb-ao-pwm",
		.data = &pwm_gxbb_ao_data
	},
	{
		.compatible = "amlogic,meson-axg-ee-pwm",
		.data = &pwm_axg_ee_data
	},
	{
		.compatible = "amlogic,meson-axg-ao-pwm",
		.data = &pwm_axg_ao_data
	},
#endif
	{
		.compatible = "amlogic,meson-g12a-ee-pwm",
		.data = &pwm_g12a_ee_data
	},
	{
		.compatible = "amlogic,meson-g12a-ao-pwm-ab",
		.data = &pwm_g12a_ao_ab_data
	},
	{
		.compatible = "amlogic,meson-g12a-ao-pwm-cd",
		.data = &pwm_g12a_ao_cd_data
	},
#ifdef CONFIG_AMLOGIC_MODIFY
	{
		.compatible = "amlogic,meson-tm2-ee-pwm",
		.data = &pwm_g12a_ee_data
	},
	{
		.compatible = "amlogic,meson-tm2-ao-pwm-ab",
		.data = &pwm_g12a_ao_ab_data
	},
	{
		.compatible = "amlogic,meson-tm2-ao-pwm-cd",
		.data = &pwm_g12a_ao_cd_data
	},
	{
		.compatible = "amlogic,meson-t5d-ee-pwm",
		.data = &pwm_t5d_data
	},
	{
		.compatible = "amlogic,meson-v2-pwm",
		.data = &pwm_v2_data
	},
#endif
	{},
};
MODULE_DEVICE_TABLE(of, meson_pwm_matches);

static int meson_pwm_init_channels(struct meson_pwm *meson)
{
	struct device *dev = meson->chip.dev;
	struct clk_init_data init;
	unsigned int i;
	char name[255];
	int err;

	for (i = 0; i < meson->chip.npwm; i++) {
		struct meson_pwm_channel *channel = &meson->channels[i];

		snprintf(name, sizeof(name), "%s#mux%u", dev_name(dev), i);

		init.name = name;
		init.ops = &clk_mux_ops;
		init.flags = 0;
		init.parent_names = (const char *const *)meson->data->parent_names;
		init.num_parents = meson->data->num_parents;

		channel->mux.reg = meson->base + REG_MISC_AB;
		channel->mux.shift =
				meson_pwm_per_channel_data[i].clk_sel_shift;
		channel->mux.mask = MISC_CLK_SEL_MASK;
		channel->mux.flags = 0;
		channel->mux.lock = &meson->lock;
		channel->mux.table = NULL;
		channel->mux.hw.init = &init;

		channel->clk = devm_clk_register(dev, &channel->mux.hw);
		if (IS_ERR(channel->clk)) {
			err = PTR_ERR(channel->clk);
			dev_err(dev, "failed to register %s: %d\n", name, err);
			return err;
		}

		snprintf(name, sizeof(name), "clkin%u", i);

		channel->clk_parent = devm_clk_get_optional(dev, name);
		if (IS_ERR(channel->clk_parent))
			return PTR_ERR(channel->clk_parent);
	}

	return 0;
}

#ifdef CONFIG_AMLOGIC_MODIFY
static int meson_pwm_v2_init_channels(struct meson_pwm *meson)
{
	struct meson_pwm_channel *channels = meson->channels;
	struct device *dev = meson->chip.dev;
	unsigned int i;
	char name[255];

	for (i = 0; i < (meson->chip.npwm / 2); i++) {
		snprintf(name, sizeof(name), "clkin%u", i);
		(channels + i)->clk = devm_clk_get(dev, name);
		if (IS_ERR((channels + i)->clk)) {
			dev_err(meson->chip.dev, "can't get device clock\n");
			return PTR_ERR((channels + i)->clk);
		}
		(channels + i)->clk_rate = clk_get_rate((channels + i)->clk);
		if (FCLK_DIV4_CLK - 100 <= (channels + i)->clk_rate &&
					(channels + i)->clk_rate <= FCLK_DIV4_CLK + 100)
			(channels + i)->clk_rate = FCLK_DIV4_CLK;
		PWM_DBG("get clock sel%d freq= %u\n", i, (channels + i)->clk_rate);
		(channels + i + 2)->clk = (channels + i)->clk;
		(channels + i + 2)->clk_rate = (channels + i)->clk_rate;
	}
	return 0;
}

static struct regmap_config meson_pwm_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
};
#endif

static int meson_pwm_probe(struct platform_device *pdev)
{
	struct meson_pwm *meson;
	struct meson_pwm_data *match;
	struct resource *regs;
	int err;
	int i;
#ifdef CONFIG_AMLOGIC_MODIFY
	struct resource *ext_clk_regs;
#endif

	meson = devm_kzalloc(&pdev->dev, sizeof(*meson), GFP_KERNEL);
	if (!meson)
		return -ENOMEM;
	meson->data = devm_kzalloc(&pdev->dev, sizeof(*meson->data), GFP_KERNEL);
	if (!meson->data)
		return -ENOMEM;
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	meson->base = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(meson->base))
		return PTR_ERR(meson->base);

#ifdef CONFIG_AMLOGIC_MODIFY
	meson_pwm_regmap_config.max_register = resource_size(regs) - 4;
	meson_pwm_regmap_config.name = devm_kasprintf(&pdev->dev,
						      GFP_KERNEL, "%s", "pwm");
	meson->regmap_base = devm_regmap_init_mmio(&pdev->dev,
						   meson->base,
						   &meson_pwm_regmap_config);
#endif
	spin_lock_init(&meson->lock);
	meson->chip.dev = &pdev->dev;
	meson->chip.ops = &meson_pwm_ops;
	meson->chip.base = -1;
	match = (struct meson_pwm_data *)of_device_get_match_data(&pdev->dev);
	meson->data->num_parents = match->num_parents;
	meson->data->double_channel = match->double_channel;
	meson->data->extern_clk = match->extern_clk;
	meson->data->parent_names = devm_kzalloc(&pdev->dev, sizeof(char *) * (match->num_parents),
			GFP_KERNEL);
	if (!meson->data)
		return -ENOMEM;
#ifdef CONFIG_AMLOGIC_MODIFY
	if (meson->data->extern_clk) {
		ext_clk_regs = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		PWM_DBG("get clk reg start:0x%p end:0x%p\n", (void *)(ext_clk_regs->start),
			 (void *)(ext_clk_regs->end));
		meson->ext_clk_base = devm_ioremap(&pdev->dev, ext_clk_regs->start, 4);
		if (IS_ERR(meson->ext_clk_base))
			return PTR_ERR(meson->ext_clk_base);
	}
#endif
	for (i = 0; i < match->num_parents; i++)
		meson->data->parent_names[i] = devm_kstrdup(&pdev->dev, match->parent_names[i],
				GFP_KERNEL);

#ifndef CONFIG_AMLOGIC_MODIFY
	meson->chip.npwm = MESON_NUM_PWMS;
#else
	if (meson->data->double_channel)
		meson->chip.npwm = MESON_DOUBLE_NUM_PWMS;
	else
		meson->chip.npwm = MESON_NUM_PWMS;
#endif
	meson->chip.of_xlate = of_pwm_xlate_with_flags;
	meson->chip.of_pwm_n_cells = 3;
#ifdef CONFIG_AMLOGIC_MODIFY
	if (meson->data->extern_clk)
		err = meson_pwm_v2_init_channels(meson);
	else
#endif
		err = meson_pwm_init_channels(meson);
	if (err < 0)
		return err;

	err = pwmchip_add(&meson->chip);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to register PWM chip: %d\n", err);
		return err;
	}

	platform_set_drvdata(pdev, meson);
#ifdef CONFIG_AMLOGIC_MODIFY
	/*for constant,blinks functions*/
	if (meson->data->double_channel)
		meson_pwm_sysfs_init(&pdev->dev);
#endif

	return 0;
}

static int meson_pwm_remove(struct platform_device *pdev)
{
	struct meson_pwm *meson = platform_get_drvdata(pdev);
#ifdef CONFIG_AMLOGIC_MODIFY
	if (meson->data->double_channel)
		meson_pwm_sysfs_exit(&pdev->dev);
#endif
	pwmchip_remove(&meson->chip);

	return 0;
}

static struct platform_driver meson_pwm_driver = {
	.driver = {
		.name = "meson-pwm",
		.of_match_table = meson_pwm_matches,
	},
	.probe = meson_pwm_probe,
	.remove = meson_pwm_remove,
};

#ifdef CONFIG_AMLOGIC_MODIFY
static int __init meson_pwm_init(void)
{
	const struct of_device_id *match_id;
	int ret;

	match_id = meson_pwm_matches;
	meson_pwm_driver.driver.of_match_table = match_id;
	ret = platform_driver_register(&meson_pwm_driver);
	return ret;
}

static void __exit meson_pwm_exit(void)
{
	platform_driver_unregister(&meson_pwm_driver);
}

fs_initcall_sync(meson_pwm_init);
#else
module_platform_driver(meson_pwm_driver);
#endif
module_exit(meson_pwm_exit);

MODULE_DESCRIPTION("Amlogic Meson PWM Generator driver");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_LICENSE("Dual BSD/GPL");
