/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DOMAIN_MAIN_H__
#define _DOMAIN_MAIN_H__

#if IS_ENABLED(CONFIG_AMLOGIC_POWER)
int sec_power_domain_init(void);
void sec_power_domain_exit(void);
#else
static inline int sec_power_domain_init(void)
{
	return 0;
}

static inline void sec_power_domain_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_POWER)
int meson_power_domain_init(void);
void meson_power_domain_exit(void);
#else
static inline int meson_power_domain_init(void)
{
	return 0;
}

static inline void meson_power_domain_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_SM)
int meson_sm_init(void);
void meson_sm_exit(void);
#else
static inline int meson_sm_init(void)
{
	return 0;
}

static inline void meson_sm_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_POWER_EE)
int power_ee_domain_init(void);
void power_ee_domain_exit(void);
#else
static inline int power_ee_domain_init(void)
{
	return 0;
}

static inline void power_ee_domain_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_BATTERY_PMIC6B)
int meson_pmic6b_bat_init(void);
void meson_pmic6b_bat_exit(void);
#else
static inline int meson_pmic6b_bat_init(void)
{
	return 0;
}

static inline void meson_pmic6b_bat_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_CHARGER_SGM415XX)
int sgm415xx_charger_init(void);
void sgm415xx_charger_exit(void);
#else
static inline int sgm415xx_charger_init(void)
{
	return 0;
}

static inline void sgm415xx_charger_exit(void)
{
}
#endif //CONFIG_AMLOGIC_CHARGER_SGM415XX

#if IS_ENABLED(CONFIG_AMLOGIC_BATTERY_CW2015)
int cw2015_battery_init(void);
void cw2015_battery_exit(void);
#else
static inline int cw2015_battery_init(void)
{
	return 0;
}

static inline void cw2015_battery_exit(void)
{
}
#endif //CONFIG_AMLOGIC_BATTERY_CW2015

#endif /* _DOMAIN_MAIN_H__ */
