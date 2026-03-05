/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __USB_V2_COMMON_HEADER_
#define __USB_V2_COMMON_HEADER_

#include <linux/usb/phy.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/amlogic/usb-v2.h>

enum aml_usb_phy_mode {
	AML_USB_PHY_MODE_INVALID,
	AML_USB_PHY_MODE_USB_HOST,
	AML_USB_PHY_MODE_USB_DEVICE,
	AML_USB_PHY_MODE_USB_LS,
	AML_USB_PHY_MODE_USB_FS,
	AML_USB_PHY_MODE_USB_HS,
	AML_USB_PHY_MODE_USB_HSP,
	AML_USB_PHY_MODE_USB_SS,
	AML_USB_PHY_MODE_USB_OTG,
};

enum meson_uphy_mode {
	MESON_USB_MODE_HOST,
	MESON_USB_MODE_DEVICE,
	MESON_USB_MODE_OTG,
};

struct meson_uphy_configure_opts {
#define MESON_USB_DEVICE_TEST_MODE_UNKNOWN	0
#define MESON_USB_DEVICE_TEST_MODE_COMPL	1
#define	MESON_USB_DEVICE_TEST_JK_COMPL		2
	u32 test_mode;
	u32 cfg_tune:1;
	u32 tune:1;
	u32 otg_init:1;
};

struct amlogic_otg_helper {
	struct u2p_aml_regs_m_v2 __iomem *u2p_aml_regs;
	struct delayed_work     work;
	struct delayed_work     set_mode_work;
	struct delayed_work		id_gpio_work;
	struct gpio_desc *idgpiodesc;
	struct regulator *usb_regulator_ao1v8;
	struct regulator *usb_regulator_ao3v3;
	struct regulator *usb_regulator_vcc5v;
	unsigned int irqline;
	/*otg_mutex should be taken amlogic_crg_otg_work and
	 *amlogic_crg_otg_set_m_work
	 */
	struct mutex		*otg_mutex;
	int mode_work_flag;
	int controller_type;
	struct pm_reg_store {
		/* host/device mode. */
		union u2p_r0_v2 u2p_r0;
		/* iddig irq state. */
		union u2p_r2_v2 u2p_r2;
		union u2p_r1_v2 usb_r1;
	} pm_buf;
	struct notifier_block pm_notifier;
	/* Is this an otg port? */
	bool otg_port;
	/* HW otg irq? */
	bool hwotg;
	enum {
		MESON_USB_OTG_T_PHY,
		MESON_USB_OTG_T_GPIO,
	} hwotg_type;
	/* For legacy aml usb phy. 0: iddig bits in the usb2 phy set.
	 * 1: in the usb3 phy set.
	 */
	u32 iddig_type;
};

#define USB_PHY_MAX_NUMBER  0x8
#define AML_USB_PHY_MAX_CLK_NUMBER  0x3

struct amlogic_usb_v2 {
	struct usb_phy		phy;
	struct device		*dev;
	const void			*pdata;
	void __iomem	*regs;
	void __iomem	*reset_regs;
	void __iomem	*phy_cfg[4];
	/* For usb2 phy, only used in otg driver for m31 phy
	 * or legacy synopsis phy.
	 */
	void __iomem	*phy3_cfg;
	void __iomem	*phy3_cfg_r1;
	void __iomem	*phy3_cfg_r2;
	void __iomem	*phy3_cfg_r4;
	void __iomem	*phy3_cfg_r5;
	/* TODO: Can we remove these phy31 stuffs? */
	void __iomem	*phy31_cfg;
	void __iomem	*phy31_cfg_r1;
	void __iomem	*phy31_cfg_r2;
	void __iomem	*phy31_cfg_r4;
	void __iomem	*phy31_cfg_r5;
	void __iomem	*usb2_phy_cfg;
	void __iomem	*xhci_port_a_addr;
	struct u2p_aml_regs_m_v2 __iomem *u2p_aml_regs[USB_PHY_MAX_NUMBER];
	/* TODO: Remove usb_aml_regs once all drivers uses u2p_aml_regs. */
	struct usb_aml_regs_m_v2 __iomem *usb_aml_regs;
	u32 pll_setting[8];
	/* TODO: Remove all soc specific params. */
	u32 pll_ver;
	u32 ic_ver;
	u32 pll_dis_thred_enhance;
	int phy_cfg_state[4];
	int phy_trim_initvalue[8];
	int phy_0xc_initvalue[8];
	int phy_trim_state[4];
	/* Set VBus Power though GPIO */
	int vbus_power_pin;
	int vbus_power_pin_work_mask;
	int otg;
	struct amlogic_otg_helper otg_helper;
	enum meson_uphy_mode current_mode;
#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
	struct usb_role_switch	*role_sw;
	u32 role_switch_default_mode;
#endif
	u32 version;
	u32 portspeed;
	struct delayed_work	work;
	struct delayed_work	id_gpio_work;
	struct gpio_desc *usb_gpio_desc;
	struct gpio_desc *idgpiodesc;

	int portnum;
	int suspend_flag;
	int phy_version;
	u32 phy_reset_level_bit[USB_PHY_MAX_NUMBER];
	u32 phy_reg_reset_level_bit[USB_PHY_MAX_NUMBER];
	u32 phy_reg_reset_bit;
	u32 usb_reset_bit;
	u32 usb_comb_reset_bit;
	u32 phy_apb_reset_bit;
	u32 pm_controller:1;
	u32 sw_hsp:1;
	u32 u3_companinon:1;
	u32 otg_phy_index;
	u32 reset_level;
	struct clk_bulk_data clks[AML_USB_PHY_MAX_CLK_NUMBER];
	int clk_num;
	/* TODO: Remove all clk stuffs. */
	u32	clk_mux;
	struct clk		*clk;
	struct clk		*usb_clk;
	struct clk		*gate0_clk;
	struct clk		*gate1_clk;
	struct clk		*hcsl_clk;
	struct clk		*pcie_bgp;
	u32 portconfig_31;
	u32 portconfig_30;
	void __iomem	*usb_phy_trim_reg;
	u32 phy_id;
	struct clk		*general_clk;
	u32 usb3_apb_reset_bit;
	u32 usb3_phy_reset_bit;
	u32 usb3_reset_shift;
	void	(*resume_xhci_p_a)(void);
	void	(*disable_port_a)(void);
	void	(*usb2_phy_init)(void);
	int	(*usb2_get_mode)(void);
	void (*phy_trim_tuning)(struct usb_phy *x,
		int port, int default_val);
	void (*set_usb_pll)(struct amlogic_usb_v2 *phy,
				void __iomem *reg);
};

struct aml_usb3_phy {
	struct device		*dev;
	struct clk_bulk_data clks[AML_USB_PHY_MAX_CLK_NUMBER];
	int clk_num;
	void __iomem	*cfg_reg;
	void __iomem	*ctrl_reg;
	void __iomem	*reset_reg;
	void __iomem	*trim_reg;
	struct meson_aml_u3phy_priv {
		void (*set_power)(struct aml_usb3_phy *phy, bool on);
		void (*trim)(struct aml_usb3_phy *phy);
		void (*txrx_cali)(struct aml_usb3_phy *phy);
		int (*pll_init)(struct aml_usb3_phy *phy);
		bool (*wait_pll_locked)(struct aml_usb3_phy *phy);
		void (*ssc_set)(struct aml_usb3_phy *phy, bool on);
	} *priv;
	u64 cfg_reg_phy;
	u64 ctrl_reg_phy;
	u64 reset_reg_phy;
	u64 trim_reg_phy;
	u64 cfg_reg_size;
	u64 ctrl_reg_size;
	u64 reset_reg_size;
	u64 trim_reg_size;
	u32 reset_level_shift;
	u32 apb_reset_offset;
	u8 phy_id;
	/* Reset static regs to default.
	 * Edge trigger/level reset.
	 */
	u8 usb3_apb_reset_bit;
	/* Reset dynamic regs to default.
	 * Level  reset.
	 */
	u8 usb3_phy_reset_bit;
	u8 usb3_controller_reset_bit;

	u32 ic_ver;
	bool off;
	bool pll_sw_cfg;
	bool suspend;
};

static inline void
usb_phy_trim_tuning(struct usb_phy *x, int port, int default_val)
{
	struct amlogic_usb_v2	*aml_phy;

	if (x) {
		if (x->label)
			if (!strncmp(x->label, "amlogic-usbm31phy3", 18) ||
				!strncmp(x->label, "amlogic-usb2-m31-phy", 20))
				return;
		aml_phy = container_of(x, struct amlogic_usb_v2, phy);

		if (aml_phy->phy_trim_tuning)
			aml_phy->phy_trim_tuning(x, port, default_val);
	}
}

void aml_new_usb3_get_phy(struct amlogic_usb_v2 *phy);

#if IS_ENABLED(CONFIG_USB_ROLE_SWITCH)
#define ROLE_SWITCH_FLAG 1
#else
#define ROLE_SWITCH_FLAG 0
#define dwc3_setup_role_switch(x) 0
#endif

#endif
