/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _PHY_MESON_USB2_H
#define _PHY_MESON_USB2_H

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/amlogic/usb-v2.h>
#include <linux/amlogic/usb-v2-common.h>
#include <linux/phy/phy.h>

#define MESON_UPHY_XHCI_PORT_A_MEM_SIZE 4UL

enum meson_uphy_port_speed {
	MESON_USB_SPEED_HIGH,
	MESON_USB_SPEED_HIGH_PLUS, /* Meson's private enhanced 960M. */
	MESON_USB_SPEED_SUPER,
};

/* meson phy struct to struct phy glue layer. */
struct meson_uphy_instance {
	union {
		u32 port_speed;
		enum meson_uphy_port_speed speed;
	};
	struct phy *phy;
	u32 index;
	struct meson_uphy_pm_ops {
		int (*prepare)(struct meson_uphy_instance *instance);
		void (*complete)(struct meson_uphy_instance *instance);
		int (*suspend)(struct meson_uphy_instance *instance);
		int (*resume)(struct meson_uphy_instance *instance);
		int (*freeze)(struct meson_uphy_instance *instance);
		int (*thaw)(struct meson_uphy_instance *instance);
		int (*poweroff)(struct meson_uphy_instance *instance);
		int (*restore)(struct meson_uphy_instance *instance);
		int (*suspend_late)(struct meson_uphy_instance *instance);
		int (*resume_early)(struct meson_uphy_instance *instance);
		int (*freeze_late)(struct meson_uphy_instance *instance);
		int (*thaw_early)(struct meson_uphy_instance *instance);
		int (*poweroff_late)(struct meson_uphy_instance *instance);
		int (*restore_early)(struct meson_uphy_instance *instance);
		int (*suspend_noirq)(struct meson_uphy_instance *instance);
		int (*resume_noirq)(struct meson_uphy_instance *instance);
		int (*freeze_noirq)(struct meson_uphy_instance *instance);
		int (*thaw_noirq)(struct meson_uphy_instance *instance);
		int (*poweroff_noirq)(struct meson_uphy_instance *instance);
		int (*restore_noirq)(struct meson_uphy_instance *instance);
		int (*runtime_suspend)(struct meson_uphy_instance *instance);
		int (*runtime_resume)(struct meson_uphy_instance *instance);
		int (*runtime_idle)(struct meson_uphy_instance *instance);
	} pm_ops;
	struct meson_uphy_pool *domain_pool;
	void *meson_uphy;
};

#define gphy_to_amlusbv2phy(phy)\
	((struct amlogic_usb_v2 *)((struct meson_uphy_instance *)\
	phy_get_drvdata(phy))->meson_uphy)\

#define gphy_to_amlusb3phy(phy)\
	((struct aml_usb3_phy *)((struct meson_uphy_instance *)\
	phy_get_drvdata(phy))->meson_uphy)\

#define gphy_to_m31usb3phy(phy)\
	((struct amlogic_usb_m31 *)((struct meson_uphy_instance *)\
	phy_get_drvdata(phy))->meson_uphy)\

struct meson_uphy_pool {
	struct device *dev;
	const struct meson_uphy_pdata *pdata;
	struct meson_uphy_instance **phys;
	int nphys;
};

struct meson_uphy_ops {
	int	(*init)(void *phy);
	int	(*exit)(void *phy);
	int	(*power_on)(void *phy);
	int	(*power_off)(void *phy);
	int	(*set_mode)(void *phy, enum meson_uphy_mode mode);
	int	(*configure)(void *phy, struct meson_uphy_configure_opts *opts);
	int	(*validate)(void *phy, enum meson_uphy_mode mode,
				struct meson_uphy_configure_opts *opts);
	int	(*calibrate)(void *phy);
	int	(*connect)(void *phy, int port);
	int	(*disconnect)(void *phy, int port);
};

struct meson_uphy_pdata {
	struct meson_uphy_ops *u2phy_ops;
	struct meson_uphy_ops *u3phy_ops;
	int (*u2phy_parse)(struct device *dev, struct meson_uphy_instance *instance);
	int (*u3phy_parse)(struct device *dev, struct meson_uphy_instance *instance);
	int (*otg_parse)(struct device *dev, struct meson_uphy_instance *instance);
	void (*u2phy_remove)(struct meson_uphy_instance *instance);
	void (*u3phy_remove)(struct meson_uphy_instance *instance);
	/* For now, the otg stuffs are only kept in the u2phy struct. */
	void (*otg_remove)(struct meson_uphy_instance *instance);
};

extern struct meson_uphy_pdata meson_uphy_sm1_pdata;
extern struct meson_uphy_pdata meson_uphy_sc2_pdata;
extern struct meson_uphy_pdata meson_uphy_s4_pdata;
extern struct meson_uphy_pdata meson_uphy_t7c_pdata;
extern struct meson_uphy_pdata meson_uphy_a5_pdata;
extern struct meson_uphy_pdata meson_uphy_s5_aml_pdata;
extern struct meson_uphy_pdata meson_uphy_s5_m31_pdata;
extern struct meson_uphy_pdata meson_uphy_t5m_aml_pdata;
extern struct meson_uphy_pdata meson_uphy_t5m_m31_pdata;
extern struct meson_uphy_pdata meson_uphy_a4_pdata;
extern struct meson_uphy_pdata meson_uphy_txhd2_pdata;
extern struct meson_uphy_pdata meson_uphy_s7_pdata;
extern struct meson_uphy_pdata meson_uphy_s7d_pdata;
extern struct meson_uphy_pdata meson_uphy_s6_pdata;
extern struct meson_uphy_pdata meson_uphy_t6d_pdata;
extern struct meson_uphy_pdata meson_uphy_t6w_pdata;
extern struct meson_uphy_pdata meson_uphy_t6x_pdata;
extern struct meson_uphy_pdata meson_u2phy_c4_pdata;

struct meson_u2phy_priv {
	int (*set_mode)(struct amlogic_usb_v2 *phy, enum meson_uphy_mode mode);
	void (*cali)(struct amlogic_usb_v2 *phy);
	int (*set_pll)(struct amlogic_usb_v2 *phy);
	int (*enable_clock_src)(struct amlogic_usb_v2 *phy);
	int (*set_clock_src)(struct amlogic_usb_v2 *phy);
};

extern bool meson_u2phy_960m;

inline bool meson_uphy_of_device_pci_available(void);
int meson_u2phy_usb_reset(struct amlogic_usb_v2 *phy);
void meson_u2phy_set_reset_count(void);
//int meson_u2phy_usb_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int meson_u2phy_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int meson_u2phy_reset_phycfg(struct amlogic_usb_v2 *phy);
int meson_u2phy_reg_reset(struct amlogic_usb_v2 *phy);
int meson_u2phy_reg_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int meson_u2phy_apb_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int meson_u2phy_comb_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int __maybe_unused meson_u2phy_usb_hold_reset(struct amlogic_usb_v2 *phy, bool on);
int meson_u2phy_apb_reset(struct amlogic_usb_v2 *phy);
void meson_u2phy_set_vbus_power(struct amlogic_usb_v2 *phy, bool is_power_on);
void meson_u2phy_phy_legacy_device_tuning(struct amlogic_usb_v2 *phy, bool tune);
void meson_u2phy_legacy_force_disable_xhci_port_a(struct amlogic_usb_v2 *phy);
void meson_u2phy_legacy_resume_xhci_port_a(struct amlogic_usb_v2 *phy);
int meson_usb2phy_legacy_set_pll(struct amlogic_usb_v2 *phy);
void meson_usb2phy_legacy_cali_disc_squelch(struct amlogic_usb_v2 *phy);
void meson_usb2phy_legacy_cali_disc_squelch_n(struct amlogic_usb_v2 *phy);
void meson_usb2phy_legacy_cali(struct amlogic_usb_v2 *phy);
void meson_usb2phy_legacy_cali_n(struct amlogic_usb_v2 *phy);
int meson_u2phy_legacy_set_mode(struct amlogic_usb_v2 *phy, enum meson_uphy_mode);
void meson_usb2phy_set_calibration_trim(struct amlogic_usb_v2 *phy);
int meson_u2phy_set_mode(struct amlogic_usb_v2 *phy, enum meson_uphy_mode mode);
int meson_usb2phy_wait_ready(struct amlogic_usb_v2 *phy, unsigned int timeout);
int meson_u2phy_exit(struct amlogic_usb_v2 *phy);
int meson_u2phy_power_on(struct amlogic_usb_v2 *phy);
int meson_u2phy_power_off(struct amlogic_usb_v2 *phy);
int meson_u2phy_u3drdu2o_set(struct amlogic_usb_v2 *phy, bool on);
int meson_u2phy_aml_init(struct amlogic_usb_v2 *phy,  struct meson_u2phy_priv *priv);
int meson_u2phy_aml_2t_init(struct amlogic_usb_v2 *phy, struct meson_u2phy_priv *priv);
int meson_aml_u2phy_parse(struct device *dev, struct meson_uphy_instance *instance);
int meson_synopsis_u3phy_init(struct amlogic_usb_v2 *phy);
int meson_synopsis_u3phy_exit(struct amlogic_usb_v2 *phy);
int meson_synopsis_u3phy_set_mode(struct amlogic_usb_v2 *phy, enum meson_uphy_mode mode);
int meson_synopsis_u3phy_parse(struct device *dev, struct meson_uphy_instance *instance);
inline unsigned int  meson_aml_u3phy_read_reg32(struct aml_usb3_phy *phy, void __iomem *reg);
inline void  meson_aml_u3phy_modify_reg32(struct aml_usb3_phy *phy,
			void __iomem *reg, u32 clear_mask, u32 set_mask);
inline void  meson_aml_u3phy_write_reg32(struct aml_usb3_phy *phy,
								u32 val, void __iomem *reg);
int meson_aml_u3phy_init(struct aml_usb3_phy *phy);
int meson_aml_u3phy_exit(struct aml_usb3_phy *phy);
int meson_aml_u3phy_parse(struct device *dev, struct meson_uphy_instance *instance,
				struct meson_aml_u3phy_priv *priv);
int meson_m31_u2phy_parse(struct device *dev, struct meson_uphy_instance *instance);
int meson_m31_u3phy_parse(struct device *dev, struct meson_uphy_instance *instance);
int meson_m31_u3phy_init(struct amlogic_usb_m31 *phy);
int meson_m31_u3phy_exit(struct amlogic_usb_m31 *phy);
int meson_m31_u3phy_set_mode(struct amlogic_usb_m31 *phy, enum meson_uphy_mode mode);
int meson_u2phy_m31_set_test_mode(struct amlogic_usb_v2 *phy, u32 test_mode);
int meson_u2phy_crg_otg_parse(struct device *dev, struct meson_uphy_instance *instance);
void meson_u2phy_crg_otg_remove(struct meson_uphy_instance *instance);
int meson_u2phy_crg_otg_v2_parse(struct device *dev, struct meson_uphy_instance *instance);
void meson_u2phy_crg_otg_v2_remove(struct meson_uphy_instance *instance);
int meson_uphy_rtmux_otg_parse(struct device *dev, struct meson_uphy_instance *instance);
int meson_uphy_rtmux_otg_init(struct amlogic_usb_v2 *phy);
void meson_uphy_rtmux_otg_remove(struct meson_uphy_instance *instance);
int meson_uphy_rtmux_otg_register_notifier(struct notifier_block *nb);
int meson_uphy_rtmux_otg_unregister_notifier(struct notifier_block *nb);
#if IS_ENABLED(CONFIG_AMLOGIC_CRG)
void crg_exit(void);
int crg_init(void);
void crg_gadget_exit(void);
int crg_gadget_init(void);
const char *crg_udc_get_UDC_name(void);
int crg_otg_write_UDC(const char *udc_name);
#else
static inline void crg_exit(void)
{
}

static inline int crg_init(void)
{
	return -1;
}

static inline void crg_gadget_exit(void)
{
}

static inline int crg_gadget_init(void)
{
	return -1;
}

static inline const char *crg_udc_get_UDC_name(void)
{
	return ERR_PTR(-ENODEV);
}

static inline int crg_otg_write_UDC(const char *udc_name)
{
	return -1;
}
#endif /* CONFIG_AMLOGIC_CRG */
#endif/* _PHY_MESON_USB2_H */
