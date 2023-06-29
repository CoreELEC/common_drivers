// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/phy.h>
#include <linux/netdevice.h>
#include <linux/bitfield.h>
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
#include <linux/amlogic/aml_phy_debug.h>
#endif

#define TSTCNTL		20
#define  TSTCNTL_READ		BIT(15)
#define  TSTCNTL_WRITE		BIT(14)
#define  TSTCNTL_REG_BANK_SEL	GENMASK(12, 11)
#define  TSTCNTL_TEST_MODE	BIT(10)
#define  TSTCNTL_READ_ADDRESS	GENMASK(9, 5)
#define  TSTCNTL_WRITE_ADDRESS	GENMASK(4, 0)
#define TSTREAD1	21
#define TSTWRITE	23
#define INTSRC_FLAG	29
#define  INTSRC_ANEG_PR		BIT(1)
#define  INTSRC_PARALLEL_FAULT	BIT(2)
#define  INTSRC_ANEG_LP_ACK	BIT(3)
#define  INTSRC_LINK_DOWN	BIT(4)
#define  INTSRC_REMOTE_FAULT	BIT(5)
#define  INTSRC_ANEG_COMPLETE	BIT(6)
#define INTSRC_MASK	30

#define BANK_ANALOG_DSP		0
#define BANK_WOL		1
#define BANK_BIST		3

/* WOL Registers */
#define LPI_STATUS	0xc
#define  LPI_STATUS_RSV12	BIT(12)

/* BIST Registers */
#define FR_PLL_CONTROL	0x1b
#define FR_PLL_DIV0	0x1c
#define FR_PLL_DIV1	0x1d

static int meson_gxl_open_banks(struct phy_device *phydev)
{
	int ret;

	/* Enable Analog and DSP register Bank access by
	 * toggling TSTCNTL_TEST_MODE bit in the TSTCNTL register
	 */
	ret = phy_write(phydev, TSTCNTL, 0);
	if (ret)
		return ret;
	ret = phy_write(phydev, TSTCNTL, TSTCNTL_TEST_MODE);
	if (ret)
		return ret;
	ret = phy_write(phydev, TSTCNTL, 0);
	if (ret)
		return ret;
	return phy_write(phydev, TSTCNTL, TSTCNTL_TEST_MODE);
}

static void meson_gxl_close_banks(struct phy_device *phydev)
{
	phy_write(phydev, TSTCNTL, 0);
}

static int meson_gxl_read_reg(struct phy_device *phydev,
			      unsigned int bank, unsigned int reg)
{
	int ret;

	ret = meson_gxl_open_banks(phydev);
	if (ret)
		goto out;

	ret = phy_write(phydev, TSTCNTL, TSTCNTL_READ |
			FIELD_PREP(TSTCNTL_REG_BANK_SEL, bank) |
			TSTCNTL_TEST_MODE |
			FIELD_PREP(TSTCNTL_READ_ADDRESS, reg));
	if (ret)
		goto out;

	ret = phy_read(phydev, TSTREAD1);
out:
	/* Close the bank access on our way out */
	meson_gxl_close_banks(phydev);
	return ret;
}

static int meson_gxl_write_reg(struct phy_device *phydev,
			       unsigned int bank, unsigned int reg,
			       uint16_t value)
{
	int ret;

	ret = meson_gxl_open_banks(phydev);
	if (ret)
		goto out;

	ret = phy_write(phydev, TSTWRITE, value);
	if (ret)
		goto out;

	ret = phy_write(phydev, TSTCNTL, TSTCNTL_WRITE |
			FIELD_PREP(TSTCNTL_REG_BANK_SEL, bank) |
			TSTCNTL_TEST_MODE |
			FIELD_PREP(TSTCNTL_WRITE_ADDRESS, reg));

out:
	/* Close the bank access on our way out */
	meson_gxl_close_banks(phydev);
	return ret;
}

static int meson_gxl_config_init(struct phy_device *phydev)
{
	int ret;

	/* Enable fractional PLL */
	ret = meson_gxl_write_reg(phydev, BANK_BIST, FR_PLL_CONTROL, 0x5);
	if (ret)
		return ret;

	/* Program fraction FR_PLL_DIV1 */
	ret = meson_gxl_write_reg(phydev, BANK_BIST, FR_PLL_DIV1, 0x029a);
	if (ret)
		return ret;

	/* Program fraction FR_PLL_DIV1 */
	ret = meson_gxl_write_reg(phydev, BANK_BIST, FR_PLL_DIV0, 0xaaaa);
	if (ret)
		return ret;

	return 0;
}

/* This function is provided to cope with the possible failures of this phy
 * during aneg process. When aneg fails, the PHY reports that aneg is done
 * but the value found in MII_LPA is wrong:
 *  - Early failures: MII_LPA is just 0x0001. if MII_EXPANSION reports that
 *    the link partner (LP) supports aneg but the LP never acked our base
 *    code word, it is likely that we never sent it to begin with.
 *  - Late failures: MII_LPA is filled with a value which seems to make sense
 *    but it actually is not what the LP is advertising. It seems that we
 *    can detect this using a magic bit in the WOL bank (reg 12 - bit 12).
 *    If this particular bit is not set when aneg is reported being done,
 *    it means MII_LPA is likely to be wrong.
 *
 * In both case, forcing a restart of the aneg process solve the problem.
 * When this failure happens, the first retry is usually successful but,
 * in some cases, it may take up to 6 retries to get a decent result
 */
static int meson_gxl_read_status(struct phy_device *phydev)
{
	int ret, wol, lpa, exp;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		ret = genphy_aneg_done(phydev);
		if (ret < 0)
			return ret;
		else if (!ret)
			goto read_status_continue;

		/* Aneg is done, let's check everything is fine */
		wol = meson_gxl_read_reg(phydev, BANK_WOL, LPI_STATUS);
		if (wol < 0)
			return wol;

		lpa = phy_read(phydev, MII_LPA);
		if (lpa < 0)
			return lpa;

		exp = phy_read(phydev, MII_EXPANSION);
		if (exp < 0)
			return exp;

		if (!(wol & LPI_STATUS_RSV12) ||
		    ((exp & EXPANSION_NWAY) && !(lpa & LPA_LPACK))) {
			/* Looks like aneg failed after all */
			phydev_dbg(phydev, "LPA corruption - aneg restart\n");
			return genphy_restart_aneg(phydev);
		}
	}

read_status_continue:
	return genphy_read_status(phydev);
}

static int meson_gxl_ack_interrupt(struct phy_device *phydev)
{
	int ret = phy_read(phydev, INTSRC_FLAG);

	return ret < 0 ? ret : 0;
}

static int meson_gxl_config_intr(struct phy_device *phydev)
{
	u16 val;
	int ret;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED) {
		/* Ack any pending IRQ */
		ret = meson_gxl_ack_interrupt(phydev);
		if (ret)
			return ret;

		val = INTSRC_ANEG_PR
			| INTSRC_PARALLEL_FAULT
			| INTSRC_ANEG_LP_ACK
			| INTSRC_LINK_DOWN
			| INTSRC_REMOTE_FAULT
			| INTSRC_ANEG_COMPLETE;
		ret = phy_write(phydev, INTSRC_MASK, val);
	} else {
		val = 0;
		ret = phy_write(phydev, INTSRC_MASK, val);

		/* Ack any pending IRQ */
		ret = meson_gxl_ack_interrupt(phydev);
	}

	return ret;
}

static irqreturn_t meson_gxl_handle_interrupt(struct phy_device *phydev)
{
	int irq_status;

	irq_status = phy_read(phydev, INTSRC_FLAG);
	if (irq_status < 0) {
		phy_error(phydev);
		return IRQ_NONE;
	}

	if (irq_status == 0)
		return IRQ_NONE;

	phy_trigger_machine(phydev);

	return IRQ_HANDLED;
}

#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
/*tx_amp*/

unsigned int voltage_phy;
EXPORT_SYMBOL_GPL(voltage_phy);
static unsigned int return_write_val(struct phy_device *phy_dev, int rd_addr)
{
	int rd_data;
	int rd_data_hi;

	phy_write(phy_dev, 20,
		((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));
	rd_data = phy_read(phy_dev, 21);
	rd_data_hi = phy_read(phy_dev, 22);
	rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;

	return rd_data;
}

static unsigned int phy_tst_write(struct phy_device *phy_dev, unsigned int wr_addr,
					unsigned int wr_data)
{	/*init*/
	phy_write(phy_dev, 20, 0x0000);
	phy_write(phy_dev, 20, 0x0400);
	phy_write(phy_dev, 20, 0x0000);
	phy_write(phy_dev, 20, 0x0400);

	if (wr_addr <= 31) {
		phy_write(phy_dev, 23, (wr_data & 0xffff));

		phy_write(phy_dev, 20,
			((1 << 14) | (1 << 10) | ((wr_addr << 0) & 0x1f)));

		pr_info("write phy tstcntl [reg_%d] 0x%x, 0x%x\n",
			wr_addr, wr_data, return_write_val(phy_dev, wr_addr));
	} else {
		pr_info("Invalid parameter\n");
	}
	return 0;
}

static unsigned int phy_tst_read(struct phy_device *phy_dev, unsigned int rd_addr)
{
	unsigned int rd_data_hi;
	unsigned int rd_data = 0;

	/*init*/
	phy_write(phy_dev, 20, 0x0000);
	phy_write(phy_dev, 20, 0x0400);
	phy_write(phy_dev, 20, 0x0000);
	phy_write(phy_dev, 20, 0x0400);

	if (rd_addr <= 31) {
		phy_write(phy_dev, 20,
			((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));

		rd_data = phy_read(phy_dev, 21);
		rd_data_hi = phy_read(phy_dev, 22);
		rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;
		pr_info("read tstcntl phy [reg_%d] 0x%x\n", rd_addr, rd_data);
	} else {
		pr_info("Invalid parameter\n");
	}

	return rd_data;
}
unsigned int tx_amp_bl2;
EXPORT_SYMBOL_GPL(tx_amp_bl2);
static int custom_internal_config(struct phy_device *phydev)
{
	unsigned int efuse_valid = 0;
	unsigned int efuse_amp = 0;
	unsigned int setup_amp = 0;

	efuse_amp = tx_amp_bl2;
	efuse_valid = ((efuse_amp >> 4) & 0x3);
	setup_amp = efuse_amp & 0xf;

	if (efuse_valid) {
		/*Enable Analog and DSP register Bank access by*/
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x14, 0x0000);
		phy_write(phydev, 0x14, 0x0400);
		phy_write(phydev, 0x17, setup_amp);
		phy_write(phydev, 0x14, 0x4418);
		pr_info("set phy setup_amp = %d\n", setup_amp);
	} else {
		/*env not set, efuse not valid return*/
		pr_info("env not set, efuse also invalid\n");
	}

	/*voltage phy*/
	/*t3x*/
	if (voltage_phy == 1) {
	//	phy_tst_write(phydev, 0x18, 0x8);
		/*set A4 bit[12:14] as 0, all the setting from mail 2023-4-7 title:T3X Ethernet */
		phy_tst_write(phydev, 0x15, phy_tst_read(phydev, 0x15) & 0x8fff);
		pr_info("setup voltage phy %x\n", phy_tst_read(phydev, 0x15));
	}
	/*txhd2*/
	if (voltage_phy == 2) {
		phy_tst_write(phydev, 0x16, 0x8400);
		pr_debug("setup voltage phy %x\n", phy_tst_read(phydev, 0x16));
	}

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int gxl_suspend(struct phy_device *phydev)
{
	int rtn = 0;

	if (wol_switch_from_user == 0)
		rtn = genphy_suspend(phydev);

	return rtn;
}

static int gxl_resume(struct phy_device *phydev)
{
	int rtn = 0;

	if (wol_switch_from_user == 0)
		rtn = genphy_resume(phydev);

	return rtn;
}
#endif
#endif
static struct phy_driver meson_gxl_phy[] = {
	{
		PHY_ID_MATCH_EXACT(0x01814400),
		.name		= "Meson GXL Internal PHY",
		/* PHY_BASIC_FEATURES */
		.flags		= PHY_IS_INTERNAL,
		.soft_reset     = genphy_soft_reset,
		.config_init	= meson_gxl_config_init,
		.read_status	= meson_gxl_read_status,
		.config_intr	= meson_gxl_config_intr,
		.handle_interrupt = meson_gxl_handle_interrupt,
		.suspend        = genphy_suspend,
		.resume         = genphy_resume,
	}, {
		PHY_ID_MATCH_EXACT(0x01803301),
		.name		= "Meson G12A Internal PHY",
		/* PHY_BASIC_FEATURES */
		.flags		= PHY_IS_INTERNAL,
		.soft_reset     = genphy_soft_reset,
		.config_intr	= meson_gxl_config_intr,
		.handle_interrupt = meson_gxl_handle_interrupt,
#if IS_ENABLED(CONFIG_AMLOGIC_ETH_PRIVE)
		.config_init	= custom_internal_config,
#ifdef CONFIG_PM_SLEEP
		.suspend        = gxl_suspend,
		.resume         = gxl_resume,
#endif
#else
		.suspend        = genphy_suspend,
		.resume         = genphy_resume,
#endif
	},
};

static struct mdio_device_id __maybe_unused meson_gxl_tbl[] = {
	{ PHY_ID_MATCH_VENDOR(0x01814400) },
	{ PHY_ID_MATCH_VENDOR(0x01803301) },
	{ }
};

module_phy_driver(meson_gxl_phy);

MODULE_DEVICE_TABLE(mdio, meson_gxl_tbl);

MODULE_DESCRIPTION("Amlogic Meson GXL Internal PHY driver");
MODULE_AUTHOR("Baoqi wang");
MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_AUTHOR("Jerome Brunet <jbrunet@baylibre.com>");
MODULE_LICENSE("GPL");
