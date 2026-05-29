/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __PCIE_MAIN_H_
#define __PCIE_MAIN_H_

#if IS_ENABLED(CONFIG_AMLOGIC_PCIE_V2_HOST)
int aml_pcie_rc_v2_init(void);
void aml_pcie_rc_v2_exit(void);
#else
static inline int aml_pcie_rc_v2_init(void)
{
	return 0;
}

static inline void aml_pcie_rc_v2_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_PCIE_V3_HOST)
int aml_pcie_rc_v3_init(void);
void aml_pcie_rc_v3_exit(void);
#else
static inline int aml_pcie_rc_v3_init(void)
{
	return 0;
}

static inline void aml_pcie_rc_v3_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_PCIE_V3_EP)
int aml_pcie_ep_v3_init(void);
void aml_pcie_ep_v3_exit(void);

int aml_pcie_ep_test_init(void);
void aml_pcie_ep_test_exit(void);
#else
static inline int aml_pcie_ep_v3_init(void)
{
	return 0;
}

static inline void aml_pcie_ep_v3_exit(void)
{
}

static inline int aml_pcie_ep_test_init(void)
{
	return 0;
}

static inline void aml_pcie_ep_test_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_PCIE_V4_HOST)
int aml_pcie_rc_v4_init(void);
void aml_pcie_rc_v4_exit(void);
#else
static inline int aml_pcie_rc_v4_init(void)
{
	return 0;
}

static inline void aml_pcie_rc_v4_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_PCIE_PHY_A9)
int aml_pcie_phy_a9_init(void);
void aml_pcie_phy_a9_exit(void);
#else
static inline int aml_pcie_phy_a9_init(void)
{
	return 0;
}

static inline void aml_pcie_phy_a9_exit(void)
{
}
#endif

#if IS_ENABLED(CONFIG_AMLOGIC_PCIE_PHY_C5)
int aml_pcie_phy_c5_init(void);
void aml_pcie_phy_c5_exit(void);
#else
static inline int aml_pcie_phy_c5_init(void)
{
	return 0;
}

static inline void aml_pcie_phy_c5_exit(void)
{
}
#endif

#endif /*__PCIE_MAIN_H_*/
