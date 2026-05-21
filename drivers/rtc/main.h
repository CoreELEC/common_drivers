/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __RTC_MAIN_H_
#define __RTC_MAIN_H_

#if IS_ENABLED(CONFIG_AMLOGIC_RTC_DRV_MESON_VRTC)
int __init vrtc_init(void);
void __exit vrtc_exit(void);
#else
static inline int vrtc_init(void)
{
	return 0;
}

static inline void vrtc_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_MESON_RTC)
int __init rtc_init(void);
void __exit rtc_exit(void);
#else
static inline int rtc_init(void)
{
	return 0;
}

static inline void rtc_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_RTC_DRV_AMLOGIC_A4)
int __init aml_rtc_dev_init(void);
void __exit aml_rtc_dev_exit(void);
#else
static inline int aml_rtc_dev_init(void)
{
	return 0;
}

static inline void aml_rtc_dev_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_RTC_PMIC6B)
int __init meson_pmic6b_rtc_init(void);
void __exit meson_pmic6b_rtc_exit(void);
#else
static inline int meson_pmic6b_rtc_init(void)
{
	return 0;
}

static inline void meson_pmic6b_rtc_exit(void)
{
}
#endif

#endif /*_RTC_MAIN_H__*/
