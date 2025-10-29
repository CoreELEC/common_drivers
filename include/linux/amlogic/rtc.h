/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/*******************************************************************/
#include <linux/types.h>
#include <linux/regmap.h>
#include <linux/alarmtimer.h>
#include <linux/clk-provider.h>

/* rtc oscillator rate */
#define OSC_32K			(32768)
#define OSC_24M			(24000000)

#define RTC_CTRL_REG		(0x0 << 2)	 /* Control RTC -RW*/
#define RTC_ALRM0_EN		BIT(0)
#define RTC_ALRM1_EN		BIT(1)
#define RTC_ALRM2_EN		BIT(2)
#define RTC_ALRM3_EN		BIT(3)
#define RTC_OSC_SEL		BIT(8)
#define RTC_ENABLE		BIT(12)

#define RTC_COUNTER_REG		(0x1 << 2)  /* Program RTC counter initial value -RW*/

#define RTC_ALARM0_REG		(0x2 << 2)  /* Program RTC alarm0 value -RW*/

#define RTC_ALARM1_REG		(0x3 << 2)  /* Program RTC alarm1 value -RW*/

#define RTC_ALARM2_REG		(0x4 << 2)  /* Program RTC alarm2 value -RW*/

#define RTC_ALARM3_REG		(0x5 << 2)  /* Program RTC alarm3 value -RW*/

#define RTC_SEC_ADJUST_REG	(0x6 << 2)  /* Control second-based timing adjustment -RW*/
#define RTC_MATCH_COUNTER	GENMASK(18, 0)
#define RTC_SEC_ADJUST_CTRL	GENMASK(20, 19)
#define RTC_ADJ_VALID		BIT(23)
#define RTC_DIV256_ADJ_VAL	BIT(24)
#define RTC_DIV256_ADJ_DSR	BIT(25)

#define RTC_INT_MASK		(0x8 << 2)  /* RTC interrupt mask -RW*/
#define RTC_ALRM0_IRQ_MSK	BIT(0)
#define RTC_ALRM1_IRQ_MSK	BIT(1)
#define RTC_ALRM2_IRQ_MSK	BIT(2)
#define RTC_ALRM3_IRQ_MSK	BIT(3)

#define RTC_INT_CLR		(0x9 << 2)  /* Clear RTC interrupt -RW*/
#define RTC_ALRM0_IRQ_CLR	BIT(0)
#define RTC_ALRM1_IRQ_CLR	BIT(1)
#define RTC_ALRM2_IRQ_CLR	BIT(2)
#define RTC_ALRM3_IRQ_CLR	BIT(3)

#define RTC_OSCIN_CTRL0		(0xa << 2) /* Control RTC clk from 24M -RW*/
#define RTC_OSCIN_CTRL1		(0xb << 2) /* Control RTC clk from 24M -RW*/
#define RTC_OSCIN_IN_EN		BIT(31)
#define RTC_OSCIN_OUT_CFG	GENMASK(29, 28)
#define RTC_OSCIN_OUT_N0M0	GENMASK(11, 0)
#define RTC_OSCIN_OUT_N1M1	GENMASK(23, 12)

#define RTC_INT_STATUS		(0xc << 2) /* RTC interrupt status -R*/
#define RTC_ALRM0_IRQ_STATUS	BIT(0)
#define RTC_ALRM1_IRQ_STATUS	BIT(1)
#define RTC_ALRM2_IRQ_STATUS	BIT(2)
#define RTC_ALRM3_IRQ_STATUS	BIT(3)

#define RTC_REAL_TIME		(0xd << 2) /* RTC counter value -R*/

#define RTC_OSCIN_OUT_32K_N0	0x2dc
#define RTC_OSCIN_OUT_32K_N1	0x2db
#define RTC_OSCIN_OUT_32K_M0	0x1
#define RTC_OSCIN_OUT_32K_M1	0x2

#define RTC_SWALLOW_SECOND	0x2
#define RTC_INSERT_SECOND	0x3

/* vrtc alarm reg */
#define VRTC_ALARM_REG		(0x0 << 2)

static struct regmap_config aml_rtc_regmap_config = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = 4,
	.name = "aml_rtc",
};

struct aml_rtc_data {
	struct regmap *map;
	struct rtc_device *rtc_dev;
	struct mbox_chan *mbox_chan;
	int irq;
	bool rtc_virtual;
	struct clk *rtc_clk;
	struct clk *sys_clk;
	bool alarm_enabled;
	bool wakealrm_enabled;
	bool time_storage_format;
	bool need_mbox_chan;
};
