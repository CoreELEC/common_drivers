/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PWM_MESON_TEE_H
#define _PWM_MESON_TEE_H
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
#include <linux/amlogic/secure_pwm_i2c.h>

#define PWM_MESON_TEE_DRV_CMP			"amlogic,meson-pwm-tee"

enum sec_pwm {
	SECID_PWM_ENABLE_MAIN = 0,
	SECID_PWM_ENABLE_SUB,
	SECID_PWM_DISABLE_MAIN,
	SECID_PWM_DISABLE_SUB,
	SECID_PWM_CONSTANT_EN,
	SECID_PWM_CONSTANT_DIS,
	SECID_PWM_TIMES_MAIN,
	SECID_PWM_TIMES_SUB,
};

#define REG_PWM			0x0
#define PWM_LOW_MASK		GENMASK(15, 0)
#define PWM_HIGH_MASK		GENMASK(31, 16)
#define PWM_MISC_REG		0x8
#define MISC_EN			BIT(0)
#define MISC_EN_SUB			BIT(25)
#define MISC_CONSTANT		BIT(28)
#define CHANNEL_MAIN		0
#define REG_TIME			0x10
#define TIME_MASK_MAIN			0xff000000
#define TIME_MASK_SUB			0x00ff0000
#define TIME_MAIN			24
#define TIME_SUB			16
#define REG_PWM_SUB			0x14
#define DOUBLE_CHAN_COMPAT
#ifdef DOUBLE_CHAN_COMPAT
#define MESON_NUM_PWMS_TEE		2
#define CHANNEL_SUB			1
#else
#define MESON_NUM_PWMS_TEE		1
#endif

struct meson_pwm_tee_channel {
	unsigned long rate;
	unsigned int hi;
	unsigned int lo;
	struct clk *clk;
	u8 pre_div;
	u8 clk_div;
};

/*pwm register att*/
struct meson_pwm_variant_n {
	unsigned int times;
	unsigned int constant;
	unsigned int blink_enable;
	unsigned int blink_times;
};

struct meson_pwm_tee {
	struct pwm_chip *chip;
	struct meson_pwm_tee_channel channels[MESON_NUM_PWMS_TEE];
	struct meson_pwm_variant_n variant;
	void __iomem *base;
	u32 tee_id;
	/*
	 * Protects register (write) access to the PWM_MISC_REG register
	 * that is shared between the two PWMs.
	 */
	spinlock_t lock;
#ifdef CONFIG_HIBERNATION
#define PWM_TEE_REG_NUMS			8
	u32 regs_restore[PWM_TEE_REG_NUMS];
#endif
};

int meson_pwm_sysfs_init(struct device *dev, bool tee_pwm);
struct meson_pwm_tee *to_meson_pwm_tee(struct pwm_chip *chip);
int pwm_set_times_tee(struct meson_pwm_tee *meson,
		  unsigned int index, unsigned int value);
#endif   /* _PWM_MESON_TEE_H_ */
