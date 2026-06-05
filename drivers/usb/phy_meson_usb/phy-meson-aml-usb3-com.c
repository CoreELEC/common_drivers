// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
//#define AML_U3P_DBG_DUMP
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/clk/clk-conf.h>
#include <linux/amlogic/usbtype.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/typec_mux_meson.h>
#include <linux/usb/typec.h>
#include "phy-meson-usb.h"

static inline bool  meson_aml_u3phy_is_sw_on(struct aml_usb3_phy *phy)
{
	return !phy->off ? true : false;
}

static inline bool  meson_aml_u3phy_is_hw_on(struct aml_usb3_phy *phy)
{
	return !READ_ONCE(phy->suspend);
}

static inline void  meson_aml_u3phy_set_hw_on(struct aml_usb3_phy *phy, bool on)
{
	WRITE_ONCE(phy->suspend, !on);
}

static inline phys_addr_t  meson_aml_u3phy_virt_to_phy(struct aml_usb3_phy *phy,
										void __iomem *reg)
{
	if (reg >= phy->ctrl_reg && reg < phy->ctrl_reg + phy->ctrl_reg_size)
		return phy->ctrl_reg_phy + (phys_addr_t)reg - (phys_addr_t)phy->ctrl_reg;

	if (reg >= phy->cfg_reg && reg < phy->cfg_reg + phy->cfg_reg_size)
		return phy->cfg_reg_phy + (phys_addr_t)reg - (phys_addr_t)phy->cfg_reg;

	if (reg >= phy->reset_reg && reg < phy->reset_reg + phy->reset_reg_size)
		return phy->reset_reg_phy + (phys_addr_t)reg - (phys_addr_t)phy->reset_reg;

	if (reg >= phy->trim_reg && reg < phy->trim_reg + phy->trim_reg_size)
		return phy->trim_reg_phy + (phys_addr_t)reg - (phys_addr_t)phy->trim_reg;

	if (reg >= phy->combo_phy_mux_reg && reg < phy->combo_phy_mux_reg + 4)
		return phy->combo_phy_mux_reg_phy + (phys_addr_t)reg -
			(phys_addr_t)phy->combo_phy_mux_reg;

	return (phys_addr_t)(-1);
}

inline unsigned int  meson_aml_u3phy_read_reg32(struct aml_usb3_phy *phy, void __iomem *reg)
{
	phys_addr_t pa = meson_aml_u3phy_virt_to_phy(phy, reg);
	u32 val = readl(reg);

	dev_dbg(phy->dev, "read: %pa, 0x%x.\n", &pa, val);
	pa = 0;

	return val;
}

inline void  meson_aml_u3phy_modify_reg32(struct aml_usb3_phy *phy,
			void __iomem *reg, u32 clear_mask, u32 set_mask)
{
	phys_addr_t pa = meson_aml_u3phy_virt_to_phy(phy, reg);

	dev_dbg(phy->dev, "initial: %pa, 0x%x.\n", &pa, readl(reg));
	writel((readl(reg) & ~clear_mask) | set_mask, reg);
	dev_dbg(phy->dev, "modified: %pa, 0x%x.\n", &pa, readl(reg));
	pa = 0;
}

inline void  meson_aml_u3phy_write_reg32(struct aml_usb3_phy *phy,
								u32 val, void __iomem *reg)
{
	phys_addr_t pa = meson_aml_u3phy_virt_to_phy(phy, reg);

	dev_dbg(phy->dev, "initial: %pa, 0x%x.\n", &pa, readl(reg));
	writel(val, reg);
	dev_dbg(phy->dev, "written: %pa, 0x%x.\n", &pa, readl(reg));
	pa = 0;
}

#ifdef AML_U3P_DBG_DUMP
#define DUMP_CDR_INIT
#define DUMP_CDR_CODE
#define DUMP_DFE
#define DUMP_OFST
#define DUMP_EQ
#define DUMP_EYE

/* For t6x */
void aml_usb3_phy_dump_status(struct aml_usb3_phy *phy)
{
	u32 val;

	if (!meson_aml_u3phy_is_hw_on(phy))
		return;

#ifdef DUMP_CDR_INIT
	val = readl(phy->ctrl_reg + 0x14);
	val &= ~(u32)GENMASK(23, 22);
	val |= 0x2 << 22;
	writel(val, phy->ctrl_reg + 0x14);
	val = readl(phy->ctrl_reg + 0x10);
	val &= ~(u32)BIT(31);
	val |= (u32)BIT(19);
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("cdr0_init %s:%s0x%x.\n",
			readl(phy->ctrl_reg + 0x154) & (u32)BIT(7) ? "O" : "X",
			readl(phy->ctrl_reg + 0x154) & (u32)BIT(6) ? "-" : "+",
			readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(5, 0));
#endif

#ifdef DUMP_CDR_CODE
	val = readl(phy->ctrl_reg + 0x10);
	val &= ~(u32)BIT(19);
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("cdr0_code 0x%x.\n", readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
#endif

#ifdef DUMP_DFE
	/* DFE */
	val = readl(phy->ctrl_reg + 0x14);
	val &= ~(u32)GENMASK(23, 22);
	writel(val, phy->ctrl_reg + 0x14);
	val = readl(phy->ctrl_reg + 0x10);
	val &= ~(u32)BIT(31);
	val &= ~(u32)GENMASK(30, 28);
	val |= 0 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("dfe0_tap0 0x%x.\n", readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
	val &= ~(u32)GENMASK(30, 28);
	val |= 1 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("dfe0_tap1 0x%x.\n", readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
	val &= ~(u32)GENMASK(30, 28);
	val |= 2 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("dfe0_tap2 0x%x.\n", readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
	val &= ~(u32)GENMASK(30, 28);
	val |= 3 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("dfe0_tap3 0x%x.\n", readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
	val &= ~(u32)GENMASK(30, 28);
	val |= 4 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("dfe0_tap4 0x%x.\n", readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
	/* 5,6,7,8 not used. */
#endif

#ifdef DUMP_OFST
	/* OFST */
	val = readl(phy->ctrl_reg + 0x14);
	val &= ~(u32)GENMASK(23, 22);
	val |= 0x1 << 22;
	writel(val, phy->ctrl_reg + 0x14);
	val = readl(phy->ctrl_reg + 0x10);
	val &= ~(u32)BIT(31);
	val |= (u32)BIT(19);
	val &= ~(u32)GENMASK(30, 28);
	val |= 0 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("sli0_ofst0 0x%x.\n",
		readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
	val &= ~(u32)GENMASK(30, 28);
	val |= 1 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("sli0_ofst1 0x%x.\n",
		readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
	val &= ~(u32)GENMASK(30, 28);
	val |= 2 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("sli0_ofst2 0x%x.\n",
		readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));
#endif

#ifdef DUMP_EQ
	pr_debug("upcrx_dbg_eq 0x%x.\n",
		(readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(17, 13)) >> 13);
#endif

#ifdef DUMP_EYE
	/* force upcrx_eyem_en setting. */
	val = readl(phy->ctrl_reg + 0x20);
	if (!(val & BIT(13))) {
		val |= BIT(13);
		writel(val, phy->ctrl_reg + 0x20);
	}

	usleep_range(200, 300);
	/* upcrx_eyem_en->1 */
	val = readl(phy->ctrl_reg + 0x28);
	val |= BIT(19);
	writel(val, phy->ctrl_reg + 0x28);

	val = readl(phy->ctrl_reg + 0x14);
	val &= ~(u32)GENMASK(23, 22);
	val |= 0x0 << 22;
	writel(val, phy->ctrl_reg + 0x14);

	val = readl(phy->ctrl_reg + 0x10);
	val |= (u32)BIT(31);
	writel(val, phy->ctrl_reg + 0x10);

	val = readl(phy->ctrl_reg + 0x10);
	val &= ~(u32)GENMASK(30, 28);
	val |= 2 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("eye0_eh_p 0x%x.\n",
		readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));

	val = readl(phy->ctrl_reg + 0x10);
	val &= ~(u32)GENMASK(30, 28);
	val |= 3 << 28;
	writel(val, phy->ctrl_reg + 0x10);
	pr_debug("eye0_eh_n 0x%x.\n",
		readl(phy->ctrl_reg + 0x154) & (u32)GENMASK(7, 0));

	/* upcrx_eyem_en->0 */
	val = readl(phy->ctrl_reg + 0x28);
	val &= ~(BIT(19));
	writel(val, phy->ctrl_reg + 0x28);
#endif
}

static int aml_usb3_phy_rthread(void *data)
{
	struct aml_usb3_phy *phy = data;

	if (!meson_aml_u3phy_is_hw_on(phy))
		return 0;

	while (!kthread_should_stop() && phy->dbg_thread)
		aml_usb3_phy_dump_status(phy);

	return 0;
}

static ssize_t dump_show(struct device *_dev, struct device_attribute *attr, char *buf)
{
	struct meson_uphy_instance *instance = dev_get_drvdata(_dev);
	struct aml_usb3_phy *phy = instance->meson_uphy;

	phy->dbg_thread = false;
	if (phy->dbg_dump_task) {
		kthread_stop(phy->dbg_dump_task);
		phy->dbg_dump_task = NULL;
	}
	aml_usb3_phy_dump_status(phy);

	return sprintf(buf, "done.\n");
}

static DEVICE_ATTR_RO(dump);

static ssize_t rtdump_show(struct device *_dev, struct device_attribute *attr, char *buf)
{
	struct meson_uphy_instance *instance = dev_get_drvdata(_dev);
	struct aml_usb3_phy *phy = instance->meson_uphy;

	return sprintf(buf, "thread on: %d.\n", phy->dbg_thread);
}

static ssize_t rtdump_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct meson_uphy_instance *instance = dev_get_drvdata(dev);
	struct aml_usb3_phy *phy = instance->meson_uphy;
	int ret;

	ret = kstrtobool(buf, &phy->dbg_thread);
	if (ret)
		return ret;

	if (phy->dbg_thread) {
		if (!phy->dbg_dump_task) {
			phy->dbg_dump_task = kthread_run(aml_usb3_phy_rthread,
							phy, "aml_usb3_phy_dbg_dump");
			if (IS_ERR(phy->dbg_dump_task))
				phy->dbg_dump_task = NULL;
		}
	} else {
		if (phy->dbg_dump_task) {
			kthread_stop(phy->dbg_dump_task);
			phy->dbg_dump_task = NULL;
		}
	}

	return count;
}

static DEVICE_ATTR_RW(rtdump);
#endif

int meson_u3phy_set_mode(struct aml_usb3_phy *phy, enum meson_uphy_mode mode)
{
	if (!meson_aml_u3phy_is_hw_on(phy))
		return -ENODEV;

	/* This is not setting the mode, but providing the pipe clk for the controller
	 * in U2 only device mode.
	 */
	if (mode == MESON_USB_MODE_DEVICE) {
		phy->priv->secondary_pipe_clk_set(phy, true);
		dev_dbg(phy->dev, "%s  mode %d enable secondary pipe clk.\n", __func__, mode);
	}
	return 0;
}

/* TODO: Move secondary_pipe_clk_set() in the typec related functions to platform-specific
 * driver if the pipe clk is fixed(mentioned in phy init).
 */
static inline int meson_aml_u3phy_secondary_pipe_clk_set(struct aml_usb3_phy *phy, bool on)
{
	int ret = 0;

	if (!phy->priv->secondary_pipe_clk_set)
		return ret;

	ret = phy->priv->secondary_pipe_clk_set(phy, on);
	if (ret)
		dev_err(phy->dev, "%s secondary_pipe_clk_set %d failed %d.\n", __func__, on, ret);

	return ret;
}

static int  meson_aml_u3phy_set_clk(struct aml_usb3_phy *phy, bool on)
{
	int ret = 0;

	if (on) {
		ret = clk_bulk_prepare_enable(phy->clk_num, phy->clks);
		if (ret) {
			dev_err(phy->dev, "Failed to enable usb phy bus clock at %d\n",
								__LINE__);
			return ret;
		}
	} else {
		clk_bulk_disable_unprepare(phy->clk_num, phy->clks);
	}

	return ret;
}

static bool meson_aml_u3phy_wait_pll_locked(struct aml_usb3_phy *phy)
{
	bool ret = false;
	int plldone_i;
	u32 pll_locked_mask = 0;

#define U3P2PLL1_PLL_LOCK_FLAG	24
#define U3P2PLL0_PLL_DONE 25
#define PLL_STAT_REG_OFF	0X154

	pll_locked_mask = BIT(U3P2PLL1_PLL_LOCK_FLAG);
	for (plldone_i = 10; plldone_i > 0; plldone_i--) {
		usleep_range(100, 200);
		if ((readl(phy->ctrl_reg + PLL_STAT_REG_OFF) & pll_locked_mask)
			== pll_locked_mask) {
			ret = true;
			break;
		}
	}

	if (!ret)
		dev_warn(phy->dev, "0x%x PLL init failed. Checkout timeout val?\n",
				readl(phy->ctrl_reg + PLL_STAT_REG_OFF));
	else
		dev_dbg(phy->dev, "0x%x PLL init done.\n",
			readl(phy->ctrl_reg + PLL_STAT_REG_OFF));

	return ret;
}

static int meson_aml_u3phy_pll_hw_init(struct aml_usb3_phy *phy)
{
	int ret = 0;

	meson_aml_u3phy_write_reg32(phy, BIT(phy->usb3_phy_reset_bit),
			phy->reset_reg);
	/* Reset usb3_phy_reset_bit trigger HWs pll cfg. Wait for it. */
	if (phy->priv->wait_pll_locked) {
		if (!phy->priv->wait_pll_locked(phy)) {
			ret = -ETIMEDOUT;
			dev_err(phy->dev, "Pre Wait HW PLL init failed.\n");
		}
	} else {
		if (!meson_aml_u3phy_wait_pll_locked(phy)) {
			ret = -ETIMEDOUT;
			dev_err(phy->dev, "Pre Wait HW PLL init failed.\n");
		}
	}

	return ret;
}

/* RESET_reg, write each bit to 1 can reset related module, the reset will auto-cover to 0 by HW.
 * RESETn_module(low active) = (~RESET_reg & RESET_LEVEL) | RESET_MASK.
 */
static inline void  meson_aml_u3phy_reset(struct aml_usb3_phy *phy)
{
	/* PHY PLL HW init needs controller reset deasserted. */
	meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift,
			BIT(phy->usb3_controller_reset_bit), BIT(phy->usb3_controller_reset_bit));

	meson_aml_u3phy_write_reg32(phy, BIT(phy->usb3_apb_reset_bit),
			phy->reset_reg + phy->apb_reset_offset);
	meson_aml_u3phy_write_reg32(phy, BIT(phy->usb3_phy_reset_bit),
			phy->reset_reg);

	/* usb3 phy with lane muxed by usbcdp should trigger hw pll init after
	 * typec plug is insert and flip state is detected.
	 */
	if (phy->lane_muxed)
		return;

	/* Reset usb3_phy_reset_bit trigger HWs pll cfg. Wait for it. */
	if (phy->priv->wait_pll_locked) {
		if (!phy->priv->wait_pll_locked(phy))
			dev_err(phy->dev, "Pre Wait HW PLL init failed.\n");
	} else {
		if (!meson_aml_u3phy_wait_pll_locked(phy))
			dev_err(phy->dev, "Pre Wait HW PLL init failed.\n");
	}
}

/* The u3phy power is comprised of dynamic part & static part
 * and controlled by the reg values.
 */
static void  meson_aml_u3phy_set_power(struct aml_usb3_phy *phy, bool on)
{
	/* The vbus power is controlled by the companion usb2 phy. */
	if (on) {
		/* Make sure reset is not held. */
		meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift,
				BIT(phy->usb3_phy_reset_bit), BIT(phy->usb3_phy_reset_bit));
		meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift +
				phy->apb_reset_offset, BIT(phy->usb3_apb_reset_bit),
				BIT(phy->usb3_apb_reset_bit));
	} else {
		/* Hold reset to power off. */
		meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift,
				BIT(phy->usb3_phy_reset_bit), 0);
		meson_aml_u3phy_modify_reg32(phy, phy->reset_reg + phy->reset_level_shift +
				phy->apb_reset_offset, BIT(phy->usb3_apb_reset_bit), 0);
		usleep_range(100, 200);
	}
}

static int meson_aml_u3phy_pll_init(struct aml_usb3_phy *phy)
{
	int ret = 0;

	if (phy->priv->pll_init) {
		ret = phy->priv->pll_init(phy);
		if (ret)
			return ret;
	} else {
		/* Common pll init flow. the pll init should be done by hw, which is
		 * triggered by usb3_phy_reset in meson_aml_u3phy_pre_reset().
		 */
		WARN(phy->pll_sw_cfg, "does not support sw pll flow!\n");
	}

	if (phy->priv && phy->priv->wait_pll_locked) {
		if (!phy->priv->wait_pll_locked(phy))
			ret = -ETIMEDOUT;
	} else {
		if (!meson_aml_u3phy_wait_pll_locked(phy))
			ret = -ETIMEDOUT;
	}
	return ret;
}

static void meson_aml_u3phy_trim(struct aml_usb3_phy *phy)
{
	u32 raw = meson_aml_u3phy_read_reg32(phy, phy->trim_reg);
	u32 val = 0;

	if (!(raw & BIT(6))) {
		dev_err(phy->dev, "No TX termination impedance trim? Skip.\n");
	} else {
		/* software force value, need to set upctx_pdio_main en in tx_cfg0. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xbc,
				BIT(14), BIT(14));
		/* TX termination impedance trim: upctx_pdio_main. */
		val = raw & (u32)GENMASK(2, 0) >> 0;
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xc0,
			(u32)GENMASK(28, 26), val << 26);

		/* software force value, need to set upctx_puio_main en in tx_cfg0. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xbc,
			BIT(17), BIT(17));
		/* TX termination impedance trim: upctx_puio_main */
		val = (raw & (u32)GENMASK(5, 3)) >> 3;
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xc0,
			(u32)GENMASK(25, 23), val << 23);
	}

	if (!(raw & BIT(11))) {
		dev_err(phy->dev, "No TX amplitude trim? Skip.\n");
	} else {
		/* software force analog value, need to set upctx_boost_en en in tx_cfg0. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xbc,
				BIT(0), BIT(0));
		/* TX amplitude trim: upctx_boost_en. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xc0,
			BIT(0), BIT(0));

		/* software force value, need to set upctx_boost_ctrl en in tx_cfg0. */
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xbc,
			BIT(1), BIT(1));
		/* TX amplitude trim: upctx_boost_ctrl. */
		val = (raw & (u32)GENMASK(10, 7)) >> 7;
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0xc0,
			(u32)GENMASK(4, 1), val << 1);
	}

	if (!(raw & BIT(16))) {
		dev_err(phy->dev, "No RX termination impedance trim? Skip.\n");
	} else {
		/* RX impedance trim: rterm_cal_cn field of reg_dcha_afe */
		val = (raw & (u32)GENMASK(15, 12)) >> 12;
		meson_aml_u3phy_modify_reg32(phy, phy->ctrl_reg + 0x4,
			(u32)GENMASK(7, 4), val << 4);
	}
}


static void  meson_aml_u3phy_cali(struct aml_usb3_phy *phy)
{
	if (phy->priv->trim)
		phy->priv->trim(phy);
	else
		meson_aml_u3phy_trim(phy);

	phy->priv->txrx_cali(phy);
}

static void meson_aml_u3phy_typec_post_init(struct aml_usb3_phy *phy)
{
	meson_aml_u3phy_pll_hw_init(phy);

	if (!WARN_ON(!phy->priv->ssc_set))
		phy->priv->ssc_set(phy, true);

	/* Disable secondary pipe clk. */
	meson_aml_u3phy_secondary_pipe_clk_set(phy, false);
}

int meson_aml_u3phy_init(struct aml_usb3_phy *phy)
{
	int ret = 0;

	if (!meson_aml_u3phy_is_sw_on(phy)) {
		/* Safe to set here - pipe clk config is in the u2phy comb domain, not u3phy. */
		meson_aml_u3phy_secondary_pipe_clk_set(phy, true);
		return 0;
	}

	if (meson_aml_u3phy_is_hw_on(phy))
		return 0;

	ret = meson_aml_u3phy_set_clk(phy, true);
	if (ret)
		goto exit;

	if (phy->priv->set_power)
		phy->priv->set_power(phy, true);
	else
		meson_aml_u3phy_set_power(phy, true);

	/* PHY parameters may not be perceived by the PHY state machine
	 * if it is set before the combo phy is switched to USB mode.
	 */
	if (phy->priv->combo_mux_set)
		phy->priv->combo_mux_set(phy, true);

	/* Reset phy to default and trigger HW PLL init.
	 * Make sure reset is done here even if previous step have already
	 * done it i.e. power on after power off.
	 */
	meson_aml_u3phy_reset(phy);

	/* usb3 phy with lane muxed by usbcdp should trigger hw pll init after
	 * typec plug is insert and flip state is detected.
	 */
	if (phy->lane_muxed)
		goto pll_done;

	ret = meson_aml_u3phy_pll_init(phy);
	if (ret)
		goto cleanup;

	if (phy->priv->ssc_set)
		phy->priv->ssc_set(phy, true);

pll_done:
	meson_aml_u3phy_cali(phy);

	meson_aml_u3phy_set_hw_on(phy, true);

	/* Reordering hw_on after pll_hw_init may cause pll init lost. */
	smp_mb();
	if (phy->lane_muxed && READ_ONCE(phy->tc_plug_connected))
		meson_aml_u3phy_typec_post_init(phy);
	/* U3 PHY pipe clk is off in a9 when the lane is not muxed to the U3PHY, but this is
	 * the default state after system power on, which means the dwc3 won't be able to run.
	 * Workaround it by enabling 2nd pipe clk when typec plug is unplugged, and disable it
	 * when it is plugged(real pipe clk is on).
	 */
	else if (phy->lane_muxed)
		meson_aml_u3phy_secondary_pipe_clk_set(phy, true);

	return ret;

cleanup:
	if (phy->priv->set_power)
		phy->priv->set_power(phy, false);
	else
		meson_aml_u3phy_set_power(phy, false);
	meson_aml_u3phy_set_clk(phy, false);

exit:
	return ret;
}

int meson_aml_u3phy_exit(struct aml_usb3_phy *phy)
{
	if (!meson_aml_u3phy_is_hw_on(phy))
		return -ENODEV;

#ifdef AML_U3P_DBG_DUMP
	device_remove_file(dev, &dev_attr_dump);
	device_remove_file(dev, &dev_attr_rtdump);

	if (phy->dbg_dump_task) {
		kthread_stop(phy->dbg_dump_task);
		phy->dbg_dump_task = NULL;
	}
#endif
	if (phy->priv->set_power)
		phy->priv->set_power(phy, false);
	else
		meson_aml_u3phy_set_power(phy, false);
	meson_aml_u3phy_set_clk(phy, false);
	meson_aml_u3phy_set_hw_on(phy, false);

	dev_dbg(phy->dev, "reset level final: 0x%x.\n",
				readl(phy->reset_reg + phy->reset_level_shift));

	return 0;
}

static int meson_aml_u3phy_typec_mux_notifier(struct notifier_block *nb,
				  unsigned long action, void *data)
{
	struct aml_usb3_phy *phy = container_of(nb, struct aml_usb3_phy, typec_mux_nb);
	enum typec_orientation orient = 0;

	dev_info(phy->dev, "notify from typec mux: action = %lu\n", action);
	switch (action) {
	case TYPEC_MESON_ORIENT:
		orient = *(enum typec_orientation *)data;
		phy->orient = orient;
		if (orient == TYPEC_ORIENTATION_NONE) {
			WRITE_ONCE(phy->tc_plug_connected, 0);
			meson_aml_u3phy_secondary_pipe_clk_set(phy, true);
		} else {
			WRITE_ONCE(phy->tc_plug_connected, 1);
			/* Pairing with smp_mb() in meson_aml_u3phy_init(). */
			smp_mb();
			/* connected & orient change & phy on => pll reinit. */
			if (meson_aml_u3phy_is_hw_on(phy))
				meson_aml_u3phy_typec_post_init(phy);
		}
		dev_info(phy->dev, "tc_plug_connected = %d\n", phy->tc_plug_connected);
		break;
	case TYPEC_MESON_PIN_STATE:
		break;
	case TYPEC_MESON_DP_STATE_DATA:
		break;
	default:
		break;
	}

	return NOTIFY_OK;
}

static inline void meson_aml_u3phy_register_nb(struct aml_usb3_phy *phy)
{
	phy->typec_mux_nb.notifier_call = meson_aml_u3phy_typec_mux_notifier;
	typec_mux_meson_register_notify(&phy->typec_mux_nb);
}

int meson_aml_u3phy_parse(struct device *dev, struct meson_uphy_instance *instance,
				struct meson_aml_u3phy_priv *priv)
{
	struct aml_usb3_phy *phy;
	int i, cnt, addr_i = 0, ret = 0;
	bool pll_sw_cfg = true;
	void __iomem *cfg_reg = NULL;
	void __iomem *reset_reg = NULL;
	void __iomem *ctrl_reg = NULL;
	void __iomem *trim_reg = NULL;
	u32 reset_level_shift = 0;
	u32 apb_reset_offset = 0;
	u8 usb3_apb_reset_bit = 0;
	u8 usb3_phy_reset_bit = 0;
	u8 usb3_controller_reset_bit = 0;
	u32 ic_ver = get_cpu_type();

	phy = kzalloc(sizeof(*phy), GFP_KERNEL);
	if (!phy)
		return -ENOMEM;

	phy->phy_id = instance->index;
	phy->dev = dev;
	dev_dbg(dev, "phy_id %d.\n", phy->phy_id);
	instance->meson_uphy = phy;
	get_device(dev);

	phy->priv = priv;
	if (!phy->priv || !phy->priv->txrx_cali) {
		dev_err(phy->dev, "No necessary callback(s).\n");
		return -EINVAL;
	}

	ret = of_property_read_reg(dev->of_node, addr_i++, &phy->cfg_reg_phy, &phy->cfg_reg_size);
	if (ret) {
		dev_err(dev, "failed to get address %d(id-%d)\n",
			addr_i, phy->phy_id);
		return ret;
	}

	cfg_reg = devm_ioremap(dev, (resource_size_t)phy->cfg_reg_phy,
						(resource_size_t)phy->cfg_reg_size);
	if (IS_ERR(cfg_reg))
		return PTR_ERR(cfg_reg);

	dev_dbg(dev, "USB3 phy probe:cfg_reg_phy:%llx, iomap cfg_reg:%px, s:%llx\n",
			phy->cfg_reg_phy, cfg_reg, phy->cfg_reg_size);

	ret = of_property_read_reg(dev->of_node, addr_i++, &phy->reset_reg_phy,
						&phy->reset_reg_size);
	if (ret) {
		dev_err(dev, "failed to get address %d(id-%d)\n",
			addr_i, phy->phy_id);
		return ret;
	}

	reset_reg = devm_ioremap(dev, (resource_size_t)phy->reset_reg_phy,
						(resource_size_t)phy->reset_reg_size);
	if (IS_ERR(reset_reg))
		return PTR_ERR(reset_reg);

	dev_dbg(dev, "USB3 phy probe:reset_phy:%llx, iomap reset_reg:%px, s:%llx\n",
			phy->reset_reg_phy, reset_reg, phy->reset_reg_size);

	ret = of_property_read_reg(dev->of_node, addr_i++, &phy->ctrl_reg_phy, &phy->ctrl_reg_size);
	if (ret) {
		dev_err(dev, "failed to get address %d(id-%d)\n",
			addr_i, phy->phy_id);
		return ret;
	}

	ctrl_reg = devm_ioremap(dev, (resource_size_t)phy->ctrl_reg_phy,
							(resource_size_t)phy->ctrl_reg_size);
	if (IS_ERR(ctrl_reg))
		return PTR_ERR(ctrl_reg);

	dev_dbg(dev, "USB3 phy probe:ctrl_phy:%llx, iomap ctrl_reg:%px, s:%llx\n",
			phy->ctrl_reg_phy, ctrl_reg, phy->ctrl_reg_size);

	ret = of_property_read_reg(dev->of_node, addr_i++, &phy->trim_reg_phy, &phy->trim_reg_size);
	if (ret) {
		dev_err(dev, "failed to get address %d(id-%d), This usb phy has no trim reg?\n",
			addr_i, phy->phy_id);
		return ret;
	}
	trim_reg = devm_ioremap(dev, (resource_size_t)phy->trim_reg_phy,
						(resource_size_t)phy->trim_reg_size);
	if (IS_ERR(trim_reg))
		return PTR_ERR(trim_reg);

	dev_dbg(dev, "USB3 phy probe:trim_phy:%llx, iomap trim_reg:%px, s:%llx\n",
			phy->trim_reg_phy, trim_reg, phy->trim_reg_size);

	phy->combo_phy = of_property_read_bool(dev->of_node, "combo-phy");
	if (!phy->off)
		phy->off = of_property_read_bool(dev->of_node, "off");

	phy->lane_muxed = of_property_read_bool(dev->of_node, "lane-muxed");

	pll_sw_cfg = of_property_read_bool(dev->of_node, "pll-sw-cfg");

	ret = of_property_read_u32(dev->of_node, "reset-level-shift", &reset_level_shift);
	if (ret)
		return ret;

	ret = of_property_read_u32(dev->of_node, "apb-reset-offset", &apb_reset_offset);
	if (ret)
		apb_reset_offset = 0;

	ret = of_property_read_u8(dev->of_node, "usb3-apb-reset-bit", &usb3_apb_reset_bit);
	if (ret)
		return ret;

	ret = of_property_read_u8(dev->of_node, "usb3-phy-reset-bit", &usb3_phy_reset_bit);
	if (ret)
		return ret;

	ret = of_property_read_u8(dev->of_node, "usb3-controller-reset-bit",
							&usb3_controller_reset_bit);
	if (ret)
		return ret;

	ret = of_property_read_u32(dev->of_node, "combo-phy-mux-reg", &phy->combo_phy_mux_reg_phy);
	if (ret)
		phy->combo_phy_mux_reg_phy = 0;

	dev_dbg(dev, "combo_phy_mux_reg_phy: 0x%x\n", phy->combo_phy_mux_reg_phy);

	if (phy->combo_phy_mux_reg_phy) {
		phy->combo_phy_mux_reg =
			devm_ioremap(dev, (resource_size_t)phy->combo_phy_mux_reg_phy,
				(resource_size_t)4);
		dev_dbg(dev, "combo_phy_mux_reg: %px\n", phy->combo_phy_mux_reg);

		if (IS_ERR(phy->combo_phy_mux_reg))
			return PTR_ERR(phy->combo_phy_mux_reg);
	}

	cnt = of_property_count_strings(dev->of_node, "clock-names");
	if (cnt < 0) {
		dev_err(dev, "no clks? exit.");
		return -EINVAL;
	} else if (cnt > AML_USB_PHY_MAX_CLK_NUMBER) {
		dev_err(dev, "too many clks. exit.");
		return -EOVERFLOW;
	}
	phy->clk_num = cnt;
	dev_dbg(dev, "clk num: %d\n", cnt);
	for (i = 0; i < cnt; i++) {
		ret = of_property_read_string_index(dev->of_node, "clock-names",
									i, &phy->clks[i].id);
		if (ret < 0) {
			dev_err(dev, "read clk-names idx:%d err", i);
			return -EINVAL;
		}
	}

	ret = devm_clk_bulk_get(dev, phy->clk_num, phy->clks);
	if (ret) {
		dev_dbg(dev, "Failed to get usb phy bus clocks\n");
		return ret;
	}

	for (i = 0; i < phy->clk_num; i++)
		dev_dbg(dev, "%s %px.\n", phy->clks[i].id, (void *)phy->clks[i].clk);

	ret = of_clk_set_defaults(phy->dev->of_node, false);
	if (ret < 0)
		dev_err(phy->dev, "set assigned clock parents and rates failed %d.\n", ret);

	dev_dbg(dev, "USB3 phy_off:%d, pll_sw_cfg:%d, phy_id:%d, clk_num:%d\n"
						 "reset_level_shift:0x%x, usb3-apb-reset-bit:%d\n"
						 "usb3-phy-reset-bit:%d, usb3-controller-reset-bit:%d\n"
						 "combo:%d, lane_muxed:%d\n",
						phy->off, pll_sw_cfg, phy->phy_id, phy->clk_num,
						reset_level_shift, usb3_apb_reset_bit,
						usb3_phy_reset_bit, usb3_controller_reset_bit,
						phy->combo_phy, phy->lane_muxed);

	phy->cfg_reg = cfg_reg;
	phy->ctrl_reg = ctrl_reg;
	phy->trim_reg = trim_reg;
	phy->reset_reg = reset_reg;
	phy->reset_level_shift = reset_level_shift;
	phy->usb3_apb_reset_bit = usb3_apb_reset_bit;
	phy->apb_reset_offset = apb_reset_offset;
	phy->usb3_phy_reset_bit = usb3_phy_reset_bit;
	phy->usb3_controller_reset_bit = usb3_controller_reset_bit;
	phy->pll_sw_cfg = pll_sw_cfg;
	phy->ic_ver = ic_ver;

	/* Sync state to hw off. */
	if (meson_aml_u3phy_is_sw_on(phy)) {
		meson_aml_u3phy_set_power(phy, false);
		meson_aml_u3phy_set_hw_on(phy, false);
		if (phy->lane_muxed)
			meson_aml_u3phy_register_nb(phy);
#ifdef AML_U3P_DBG_DUMP
		ret = device_create_file(dev, &dev_attr_dump);
		if (ret)
			return ret;
		ret = device_create_file(dev, &dev_attr_rtdump);
		if (ret) {
			device_remove_file(dev, &dev_attr_dump);
			return ret;
		}
#endif
	} else {
		/* For u3h u2h only mode. */
		meson_aml_u3phy_set_power(phy, false);
		meson_aml_u3phy_set_hw_on(phy, false);
	}

	return 0;
}

