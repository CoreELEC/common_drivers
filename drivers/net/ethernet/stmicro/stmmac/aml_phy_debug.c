// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/skbuff.h>
#include <linux/ethtool.h>
#include <linux/if_ether.h>
#include <linux/crc32.h>
#include <linux/mii.h>
#include <linux/if.h>
#include <linux/if_vlan.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/prefetch.h>
#include <linux/pinctrl/consumer.h>
#include <linux/net_tstamp.h>
#include <linux/reset.h>
#include <linux/of_mdio.h>
#include <linux/amlogic/aml_phy_debug.h>
#include <linux/mutex.h>
#include "stmmac.h"

#define ETH_MAC_0_Configuration			(0x0000)
#define ETH_MMC_rxicmp_err_octets		(0x0284)
#define ETH_DMA_0_Bus_Mode			(0x1000)
#define ETH_DMA_21_Curr_Host_Re_Buffer_Addr	(0x1058)

#define ETH_LEGACY_INTERFACE_SUPPORT		1

static DEFINE_MUTEX(phy_dbg_mutex);
static int phy_dbg_count;
#if defined(ETH_LEGACY_INTERFACE_SUPPORT)
static struct device *select_dev;
#endif /* ETH_LEGACY_INTERFACE_SUPPORT */

/* Get aml_eth_priv by phydev */
struct aml_eth_priv *aml_get_eth_priv_by_pdev(struct phy_device *phydev)
{
	struct net_device *dev = NULL;
	struct net_device *d;
	struct stmmac_priv *priv;
	struct device_node *phy_node;
	struct aml_eth_priv *eth_priv;

	if (phydev->phylink) {
		dev = phydev->phylink->netdev;
	} else {
		/* When no phylink is present, seek matching relationships from the DT. */
		rcu_read_lock();
		for_each_netdev_rcu(&init_net, d) {
			pr_debug("NS=%p: %s (ifindex=%d)\n", &init_net, d->name, d->ifindex);
			if (!d->dev.parent)
				continue;

			pr_debug("netdev %s -> pdev: %s\n", d->name, dev_name(d->dev.parent));
			phy_node = of_parse_phandle(d->dev.parent->of_node, "phy-handle", 0);
			if (!phy_node)
				continue;
			if (phy_node == phydev->mdio.dev.of_node)
				dev = d;
			of_node_put(phy_node);

			if (dev)
				break;
		}
		rcu_read_unlock();
	}

	if (!dev)
		return ERR_PTR(-ENODEV);

	priv = netdev_priv(dev);
	eth_priv = &priv->eth_priv;

	return eth_priv;
}
EXPORT_SYMBOL(aml_get_eth_priv_by_pdev);

/* Get aml_eth_priv by net_device */
struct aml_eth_priv *aml_get_eth_priv_by_ndev(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct stmmac_priv *priv = netdev_priv(ndev);
	struct aml_eth_priv *eth_priv = &priv->eth_priv;

	return eth_priv;
}
EXPORT_SYMBOL(aml_get_eth_priv_by_ndev);

struct device *aml_get_ethmac_dev_by_priv(struct aml_eth_priv *eth_priv)
{
	struct stmmac_priv *priv = container_of(eth_priv, struct stmmac_priv, eth_priv);
	struct device *ethmac_dev = priv->device;

	return ethmac_dev;
}
EXPORT_SYMBOL(aml_get_ethmac_dev_by_priv);

static void am_net_dump_phyreg(struct aml_eth_priv *eth_priv)
{
	int reg = 0;
	int val = 0;

	if (!eth_priv->phydev)
		return;

	pr_info("========== ETH PHY new regs ==========\n");
	for (reg = 0; reg < 32; reg++) {
		val = phy_read(eth_priv->phydev, reg);
		pr_info("[reg_%d] 0x%x\n", reg, val);
	}
}

static int am_net_read_phyreg(int argc, char **argv, struct aml_eth_priv *eth_priv)
{
	int reg = 0;
	int val = 0;
	int r = 0;
	char *separator;
	int devad = 0;
	int len = 0;
	char devad_str[16], reg_str[16];

	if (!eth_priv->phydev)
		return -1;
	if (argc < 2 || !argv || !argv[0] || !argv[1]) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	/* Check for MMD format: devad:reg */
	separator = strchr(argv[1], ':');
	if (separator) {
		len = min((int)(separator - argv[1]), (int)(sizeof(devad_str) - 1));
		strncpy(devad_str, argv[1], len);
		devad_str[len] = '\0';
		len = min((int)strlen(separator + 1), (int)(sizeof(reg_str) - 1));
		strncpy(reg_str, separator + 1, len);
		reg_str[len] = '\0';

		r = kstrtoint(devad_str, 0, &devad);
		if (r) {
			pr_err("kstrtoint mmd devad failed\n");
			return -1;
		}
		r = kstrtoint(reg_str, 0, &reg);
		if (r) {
			pr_err("kstrtoint mmd reg failed\n");
			return -1;
		}

		val = phy_read_mmd(eth_priv->phydev, devad, reg);
		pr_info("read phy [mmd 0x%x reg 0x%x] 0x%x\n", devad, reg, val);
	} else {
		/* Standard PHY register read */
		r = kstrtoint(argv[1], 0, &reg);
		if (r) {
			pr_err("kstrtoint failed\n");
			return -1;
		}

		if (reg >= 0 && reg <= 31) {
			val = phy_read(eth_priv->phydev, reg);
			pr_info("read phy [reg_%d] 0x%x\n", reg, val);
		} else if (reg == 32) {
			// pr_info("phy features:0x%x\n", eth_priv->phydev->drv->features);
		} else {
			pr_info("Invalid parameter\n");
		}
	}

	return 0;
}

static int am_net_write_phyreg(int argc, char **argv, struct aml_eth_priv *eth_priv)
{
	int reg = 0;
	int val = 0;
	int r = 0;
	char *separator;
	int devad = 0;
	int len = 0;
	char devad_str[16], reg_str[16];

	if (!eth_priv->phydev)
		return -1;

	if (argc < 3 || !argv || !argv[0] || !argv[1] || !argv[2]) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r = kstrtoint(argv[2], 0, &val);
	if (r) {
		pr_err("kstrtoint val failed\n");
		return -1;
	}

	/* Check for MMD format: devad:reg */
	separator = strchr(argv[1], ':');
	if (separator) {
		len = min((int)(separator - argv[1]), (int)(sizeof(devad_str) - 1));
		strncpy(devad_str, argv[1], len);
		devad_str[len] = '\0';
		len = min((int)strlen(separator + 1), (int)(sizeof(reg_str) - 1));
		strncpy(reg_str, separator + 1, len);
		reg_str[len] = '\0';

		r = kstrtoint(devad_str, 0, &devad);
		if (r) {
			pr_err("kstrtoint mmd devad failed\n");
			return -1;
		}
		r = kstrtoint(reg_str, 0, &reg);
		if (r) {
			pr_err("kstrtoint mmd reg failed\n");
			return -1;
		}

		phy_write_mmd(eth_priv->phydev, devad, reg, val);
		pr_info("write phy [mmd 0x%x reg 0x%x] 0x%x, 0x%x\n",
			devad, reg, val,
			phy_read_mmd(eth_priv->phydev, devad, reg));
	} else {
		/* Standard PHY register write */
		r = kstrtoint(argv[1], 0, &reg);
		if (r) {
			pr_err("kstrtoint reg failed\n");
			return -1;
		}

		if (reg >= 0 && reg <= 31) {
			phy_write(eth_priv->phydev, reg, val);
			pr_info("write phy [reg_%d] 0x%x, 0x%x\n",
				reg, val, phy_read(eth_priv->phydev, reg));
		} else if (reg == 32) {
			// pr_info("phy features:0x%x\n", eth_priv->phydev->drv->features);
		} else if (reg == 33) {
			if (val > 255) {
				pr_info("Invalid parameter\n");
				return 0;
			}
			// eth_priv->phydev->drv->features &= 0xff00;
			// eth_priv->phydev->drv->features |= val;
		} else {
			pr_info("Invalid parameter\n");
		}
	}

	return 0;
}

static void am_net_dump_phy_wol_reg(struct aml_eth_priv *eth_priv)
{
	int reg;
	int val;
	int val15;
	int val16;

	if (!eth_priv->phydev)
		return;

	pr_info("========== ETH PHY wol regs ==========\n");
	for (reg = 0; reg < 13; reg++) {
		/*Bit 15: READ*/
		/*Bit 14: Write*/
		/*Bit 12:11: BANK_SEL (0: DSP, 1: WOL, 3: BIST)*/
		/*Bit 10: Test Mode*/
		/*Bit 9:5: Read Address*/
		/*Bit 4:0: Write Address*/
		val = (1 << 15) | (1 << 11) | (1 << 10) | (reg << 5);
		phy_write(eth_priv->phydev, 0x14, val);
		val15 = phy_read(eth_priv->phydev, 0x15);
		val16 = phy_read(eth_priv->phydev, 0x16);
		val = val16 * 65536 + val15;
		pr_info("wol [reg_%d] 0x%x\n", reg, val);
	}
}

static void am_net_dump_phy_bist_reg(struct aml_eth_priv *eth_priv)
{
	int reg;
	int val;
	int val15;
	int val16;

	if (!eth_priv->phydev)
		return;

	pr_info("========== ETH PHY bist regs ==========\n");
	for (reg = 0; reg < 32; reg++) {
		/*Bit 15: READ*/
		/*Bit 14: Write*/
		/*Bit 12:11: BANK_SEL (0: DSP, 1: WOL, 3: BIST)*/
		/*Bit 10: Test Mode*/
		/*Bit 9:5: Read Address*/
		/*Bit 4:0: Write Address*/

		val = ((1 << 15) | (1 << 12) | (1 << 11) |
			(1 << 10) | (reg << 5));
		phy_write(eth_priv->phydev, 0x14, val);
		val15 = phy_read(eth_priv->phydev, 0x15);
		val16 = phy_read(eth_priv->phydev, 0x16);
		val = val16 * 65536 + val15;
		pr_info("bist [reg_%d] 0x%x\n", reg, val);
	}
}

static int read_tst_cntl_reg(int argc, char **argv, struct aml_eth_priv *eth_priv)
{
	int rd_data_hi;
	int rd_addr;
	int rd_data = 0;

	if (argc < 2 || !argv || !argv[0] || !argv[1]) {
		pr_info("Invalid syntax\n");
		return -1;
	}
	if (kstrtoint(argv[1], 16, &rd_addr) < 0)
		return -1;

	if (rd_addr >= 0 && rd_addr <= 31) {
		phy_write(eth_priv->phydev, 20,
			((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));

		rd_data = phy_read(eth_priv->phydev, 21);
		rd_data_hi = phy_read(eth_priv->phydev, 22);
		rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;
		pr_info("read tstcntl phy [reg_%d] 0x%x\n", rd_addr, rd_data);
	} else {
		pr_info("Invalid parameter\n");
	}

	return rd_data;
}

static int return_write_val(int rd_addr, struct aml_eth_priv *eth_priv)
{
	int rd_data;
	int rd_data_hi;

	phy_write(eth_priv->phydev, 20,
		((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));
	rd_data = phy_read(eth_priv->phydev, 21);
	rd_data_hi = phy_read(eth_priv->phydev, 22);
	rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;

	return rd_data;
}

static int write_tst_cntl_reg(int argc, char **argv, struct aml_eth_priv *eth_priv)
{
	int wr_addr, wr_data;

	if (argc < 3 || !argv || !argv[0] || !argv[1] || !argv[2]) {
		pr_info("Invalid syntax\n");
		return -1;
	}

	if (kstrtoint(argv[1], 16, &wr_addr) < 0)
		return -1;

	if (kstrtoint(argv[2], 16, &wr_data) < 0)
		return -1;

	if (wr_addr >= 0 && wr_addr <= 31) {
		phy_write(eth_priv->phydev, 23, (wr_data & 0xffff));

		phy_write(eth_priv->phydev, 20,
			((1 << 14) | (1 << 10) | ((wr_addr << 0) & 0x1f)));

		pr_info("write phy tstcntl [reg_%d] 0x%x, 0x%x\n",
			wr_addr, wr_data, return_write_val(wr_addr, eth_priv));
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

static void tstcntl_dump_phyreg(struct aml_eth_priv *eth_priv)
{
	int rd_addr;
	int rd_data;
	int rd_data_hi;

	pr_info("========== ETH TST PHY regs ==========\n");
	for (rd_addr = 0; rd_addr < 32; rd_addr++) {
		phy_write(eth_priv->phydev, 20,
			((1 << 15) | (1 << 10) | ((rd_addr & 0x1f) << 5)));

		rd_data = phy_read(eth_priv->phydev, 21);
		rd_data_hi = phy_read(eth_priv->phydev, 22);
		rd_data = ((rd_data_hi & 0xffff) << 16) | rd_data;
		pr_info("tstcntl phy [reg_%d] 0x%x\n", rd_addr, rd_data);
	}
}

static void am_net_dump_phy_extended_reg(struct aml_eth_priv *eth_priv)
{
	int reg;
	int val;
	int val15;
	int val16;

	if (!eth_priv->phydev)
		return;

	pr_info("========== ETH PHY extended regs ==========\n");
	for (reg = 0; reg < 32; reg++) {
		/*Bit 15: READ*/
		/*Bit 14: Write*/
		/*Bit 12:11: BANK_SEL (0: DSP, 1: WOL, 3: BIST)*/
		/*Bit 10: Test Mode*/
		/*Bit 9:5: Read Address*/
		/*Bit 4:0: Write Address*/

		val = ((1 << 15) | (1 << 10) | (reg << 5));
		phy_write(eth_priv->phydev, 0x14, val);
		val15 = phy_read(eth_priv->phydev, 0x15);
		val16 = phy_read(eth_priv->phydev, 0x16);
		val = (val16 << 16) + val15;
		pr_info("extended [reg_%d] 0x%x\n", reg, val);
	}
}

static void am_net_dump_macreg(struct aml_eth_priv *eth_priv)
{
	int reg;
	int val;

	pr_info("========== ETH_MAC regs ==========\n");
	for (reg = ETH_MAC_0_Configuration;
		reg <= ETH_MMC_rxicmp_err_octets; reg += 0x4) {
		val = readl(eth_priv->ioaddr + reg);
		pr_info("[0x%04x] 0x%x\n", reg, val);
	}

	pr_info("========== ETH_DMA regs ==========\n");
	for (reg = ETH_DMA_0_Bus_Mode;
		reg <= ETH_DMA_21_Curr_Host_Re_Buffer_Addr; reg += 0x4) {
		val = readl(eth_priv->ioaddr + reg);
		pr_info("[0x%04x] 0x%x\n", reg, val);
	}
}

static int am_net_read_macreg(int argc, char **argv, struct aml_eth_priv *eth_priv)
{
	int reg = 0;
	int val = 0;
	int r = 0;

	if (argc < 2 || !argv || !argv[0] || !argv[1]) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r  = kstrtoint(argv[1], 0, &reg);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	if (reg >= 0 && reg <= ETH_DMA_21_Curr_Host_Re_Buffer_Addr) {
		val = readl(eth_priv->ioaddr + reg);
		pr_info("read mac [0x4%x] 0x%x\n", reg, val);
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

static int am_net_write_macreg(int argc, char **argv, struct aml_eth_priv *eth_priv)
{
	int reg;
	int val;
	int r;

	if (argc < 3 || !argv || !argv[0] || !argv[1] || !argv[2]) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r = kstrtoint(argv[1], 0, &reg);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	r = kstrtoint(argv[2], 0, &val);
	if (r) {
		pr_err("kstrtoint failed\n");
		return -1;
	}

	if (reg >= 0 && reg <= ETH_DMA_21_Curr_Host_Re_Buffer_Addr) {
		writel(val, (eth_priv->ioaddr + reg));
		pr_info("write mac [0x%x] 0x%x, 0x%x\n",
			reg, val, readl(eth_priv->ioaddr + reg));
	} else {
		pr_info("Invalid parameter\n");
	}

	return 0;
}

static ssize_t info_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct aml_eth_priv *eth_priv = dev_get_drvdata(dev);
	struct stmmac_priv *priv = container_of(eth_priv, struct stmmac_priv, eth_priv);
	struct net_device *ndev = priv->dev;
	struct phy_device *phydev = eth_priv->phydev;

	return sysfs_emit(buf, "net=%s\nmdio=%s\n", netdev_name(ndev),
			  dev_name(phydev->mdio.bus->dev.parent));
}

static const char *phyreg_help = {
	"Usage:\n"
	"    echo d > phyreg;                    // dump ethernet phy reg\n"
	"    echo r [devad:]reg > phyreg;        // read ethernet phy reg\n"
	"    echo w [devad:]reg val > phyreg;    // write ethernet phy reg\n"
};

static ssize_t phyreg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", phyreg_help);
}

static inline void am_init_tst_mode(struct aml_eth_priv *eth_priv)
{
	phy_write(eth_priv->phydev, 20, 0x0000);
	phy_write(eth_priv->phydev, 20, 0x0400);
	phy_write(eth_priv->phydev, 20, 0x0000);
	phy_write(eth_priv->phydev, 20, 0x0400);
}

static inline void am_close_tst_mode(struct aml_eth_priv *eth_priv)
{
	phy_write(eth_priv->phydev, 20, 0x0000);
}

static void am_net_adc_show(struct aml_eth_priv *eth_priv)
{
	int rd_data = 0;
	int rd_data_hi = 0;
	int i, j, v;
	char buf[128];

	pr_info("%s, %d enter\n", __func__, __LINE__);

	am_init_tst_mode(eth_priv);

	for (i = 0; i < (32 * 32); i++) {
		v = (1 << 15) | (1 << 10) | ((16 & 0x1f) << 5);
		phy_write(eth_priv->phydev, 20, v);
		rd_data = phy_read(eth_priv->phydev, 21);
		rd_data_hi = phy_read(eth_priv->phydev, 22);
		eth_priv->adc_data[i] = rd_data & 0x3f;
	}

	am_close_tst_mode(eth_priv);

	for (i = 0; i < 32; i++) {
		for (j = 0; j < 32; j++)
			pr_info("%02x ", eth_priv->adc_data[i * 32 + j]);

		pr_info("\n");
	}

	for (i = 0; i < 64; i++)
		eth_priv->adc_freq[i] = 0;

	for (i = 0; i < 32; i++) {
		for (j = 0; j < 32; j++) {
			if (eth_priv->adc_data[i * 32 + j] > 31)
				eth_priv->adc_freq[eth_priv->adc_data[i * 32 + j] - 32]++;
			else
				eth_priv->adc_freq[32 + eth_priv->adc_data[i * 32 + j]]++;

			pr_info("%02x ", eth_priv->adc_data[i * 32 + j]);
		}
		pr_info("\n");
	}

	for (i = 0; i < 64; i++) {
		pr_info("%d(%04d):\t", i - 32, eth_priv->adc_freq[i]);
		if (eth_priv->adc_freq[i] > 128)
			eth_priv->adc_freq[i] = 128;

		for (j = 0; j < eth_priv->adc_freq[i]; j++)
			buf[j] = '#';

		buf[j] = '\0';

		pr_info("%s\n", buf);
	}
}

static void am_net_eye_pattern_on(struct aml_eth_priv *eth_priv)
{
	unsigned int value;

	eth_priv->phydev->autoneg = AUTONEG_DISABLE;
	/*stop check wol when doing eye pattern, otherwise it will reset phy*/
//	enable_wol_check = 0;
	phy_write(eth_priv->phydev, 0, 0x2100);
	value = phy_read(eth_priv->phydev, 17);
	pr_info("0x11 %x\n", value);
	phy_write(eth_priv->phydev, 17, (value | 0x0004));
}

static void am_net_eye_pattern_off(struct aml_eth_priv *eth_priv)
{
	unsigned int value;

	eth_priv->phydev->autoneg = AUTONEG_ENABLE;
//	enable_wol_check = 1;
	phy_write(eth_priv->phydev, 0, 0x1000);
	value = phy_read(eth_priv->phydev, 17);
	pr_info("0x11 %x\n", value);
	phy_write(eth_priv->phydev, 17, (value & ~0x0004));
}

static void am_net_debug_mode(struct aml_eth_priv *eth_priv)
{	/*disable autoneg*/
	eth_priv->phydev->autoneg = AUTONEG_DISABLE;
	/*disable wol check*/
//	enable_wol_check = 0;
}

static ssize_t phyreg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[4];
	char cmd;
	struct aml_eth_priv *eth_priv = dev_get_drvdata(dev);

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argc] = para;
	}
	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		am_net_read_phyreg(argc, argv, eth_priv);
		break;
	case 'w':
	case 'W':
		am_net_write_phyreg(argc, argv, eth_priv);
		break;
	case 'd':
	case 'D':
		am_net_dump_phyreg(eth_priv);
		break;
#ifdef CONFIG_MESON_ETH_WOL /* FIXED ME */
	case 'p':
		wol_test(phydev);
		break;
#endif
	case 'e':
		am_net_dump_phy_extended_reg(eth_priv);
		am_net_dump_phy_wol_reg(eth_priv);
		am_net_dump_phy_bist_reg(eth_priv);
		break;
	case 'c':
	case 'C':
		am_net_adc_show(eth_priv);
		break;
	case 't':
	case 'T':
		am_init_tst_mode(eth_priv);

		if (argv[0][1] == 'w' || argv[0][1] == 'W')
			write_tst_cntl_reg(argc, argv, eth_priv);

		if (argv[0][1] == 'r' || argv[0][1] == 'R')
			read_tst_cntl_reg(argc, argv, eth_priv);

		if (argv[0][1] == 'd' || argv[0][1] == 'D')
			tstcntl_dump_phyreg(eth_priv);

		am_close_tst_mode(eth_priv);
		break;
	case 'o':
	case 'O':
		if (argv[0][1] == 'n' || argv[0][1] == 'N') {
			am_net_eye_pattern_on(eth_priv);
			break;
		}
		if (argv[0][1] == 'f' || argv[0][1] == 'F') {
			am_net_eye_pattern_off(eth_priv);
			break;
		}
		if (argv[0][1] == 'd' || argv[0][1] == 'D') {
			am_net_debug_mode(eth_priv);
			break;
		}
		break;
	default:
		goto end;
	}

	return count;

end:
	kfree(buff);
	return 0;
}

static const char *macreg_help = {
	"Usage:\n"
	"    echo d > macreg;            //dump ethernet mac reg\n"
	"    echo r reg > macreg;        //read ethernet mac reg\n"
	"    echo w reg val > macreg;    //read ethernet mac reg\n"
};

static ssize_t macreg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", macreg_help);
}

static ssize_t macreg_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[4];
	char cmd;
	struct aml_eth_priv *eth_priv = dev_get_drvdata(dev);

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
	for (argc = 0; argc < 4; argc++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argc] = para;
	}
	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
	switch (cmd) {
	case 'r':
	case 'R':
		am_net_read_macreg(argc, argv, eth_priv);
		break;
	case 'w':
	case 'W':
		am_net_write_macreg(argc, argv, eth_priv);
		break;
	case 'd':
	case 'D':
		am_net_dump_macreg(eth_priv);
		break;
	default:
		goto end;
	}

	return count;

end:
	kfree(buff);
	return 0;
}

static ssize_t linkspeed_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int ret;
	char buff[100];
	struct aml_eth_priv *eth_priv = dev_get_drvdata(dev);

	if (eth_priv->phydev) {
		phy_print_status(eth_priv->phydev);

		ret = genphy_update_link(eth_priv->phydev);
		if (ret)
			pr_err("genphy_update_link error: %d\n", ret);

		if (eth_priv->phydev->link)
			strcpy(buff, "link status: link\n");
		else
			strcpy(buff, "link status: unlink\n");
	} else {
		strcpy(buff, "link status: unlink\n");
	}

	ret = sprintf(buf, "%s\n", buff);

	return ret;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static ssize_t wol_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct aml_eth_priv *eth_priv = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_AMLOGIC_WOL)
	struct stmmac_priv *priv = container_of(eth_priv, struct stmmac_priv, eth_priv);
	struct device *ethmac_dev = priv->device;
#endif

	if (!eth_priv->phydev)
		return 0;

#if IS_ENABLED(CONFIG_AMLOGIC_WOL)
	if (eth_priv->wol_ops.not_empty) {
		return sysfs_emit(buf, "%s wol 0x%x\n",
				  eth_priv->internal_phy != 2 ? "inphy" : "exphy",
				  (int)eth_priv->wol_ops.not_empty(ethmac_dev));
	}
#endif

	if (eth_priv->internal_phy != 2)
		return sprintf(buf, "inphy wol 0x%x\n", eth_priv->wol_switch_from_user);
	else
		return sprintf(buf, "exphy wol 0x%x\n", eth_priv->support_gpio_wol);
}

static ssize_t wol_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	unsigned int tmp, r;
	struct aml_eth_priv *eth_priv = dev_get_drvdata(dev);
#if IS_ENABLED(CONFIG_AMLOGIC_WOL)
	struct stmmac_priv *priv = container_of(eth_priv, struct stmmac_priv, eth_priv);
	struct device *ethmac_dev = priv->device;
#endif

	if (!eth_priv->phydev)
		return 0;

	r = kstrtoint(buf, 16, &tmp);

#if IS_ENABLED(CONFIG_AMLOGIC_WOL)
	if (eth_priv->wol_ops.set_all && eth_priv->wol_ops.clr_all) {
		if (tmp)
			eth_priv->wol_ops.set_all(ethmac_dev);
		else
			eth_priv->wol_ops.clr_all(ethmac_dev);

		return count;
	}
#endif

	if (eth_priv->internal_phy != 2)
		eth_priv->wol_switch_from_user = tmp;
	else
		eth_priv->support_gpio_wol = tmp;

	return count;
}
#endif /* CONFIG_PM_SLEEP */

static int auto_cali(struct aml_eth_priv *eth_priv)
{
	unsigned int value;
	int I1, I2, I3, I4, I5;
	int count = 99;
	char problem[20] = {0};
	char path[20] = {0};
	int cali_rise = 0;
	int cali_sel = 0;

	pr_info("auto test cali\n");
	for (cali_sel = 0; cali_sel < 4; cali_sel++) {
		readl(eth_priv->PREG_ETH_REG1);
		strcpy(problem, "no clock delay");
		value = readl(eth_priv->PREG_ETH_REG0) & (~(0x1f << 25));
		writel(value, eth_priv->PREG_ETH_REG0);

		I1 = 0;
		I2 = 0;
		I3 = 0;
		I4 = 0;
		I5 = 0;

		for (cali_rise = 0; cali_rise <= 1; cali_rise++) {
			count = 99;
			value = (readl(eth_priv->PREG_ETH_REG0) | (1 << 25) |
				(cali_rise << 26) | (cali_sel << 27));
			writel(value, eth_priv->PREG_ETH_REG0);
			while (count >= 0) {
				value = readl(eth_priv->PREG_ETH_REG1);
				if ((value >> 15) & 0x1) {
					count--;
					switch (value & 0x1f) {
					case 0x0:
						I1++;
						break;
					case 0x1:
						I2++;
						break;
					case 0x2:
						I3++;
						break;
					case 0x3:
						I4++;
						break;
					case 0x4:
						I5++;
						break;
					}
				}
			}
		pr_info(" I1 = %d; I2 = %d; I3 = %d; I4 = %d; I5 = %d;\n",
			I1, I2, I3, I4, I5);

		if (I1 > 0 && I2 > 0 && I3 > 0 &&
			I4 > 0 && I5 > 0)
			strcpy(problem, "clock delay");

		pr_info(" RXDATA Line %d have %s problem\n",
			cali_sel, problem);

		strcpy(path, (I2 + I1 + I3) > (I5 + I4 + I3) ?
			"positive" : "opposite");

		if (!strncmp(problem, "clock delay", 11))
			pr_info("Need debug to  delay %s direction\n", path);
		}
	}
	return 0;
}

static int am_net_cali(int argc, char **argv, int gate, struct aml_eth_priv *eth_priv)
{
	unsigned int value;
	int cali_rise = 0;
	int cali_sel = 0;
	int cali_start = 0;
	int cali_time = 0;
	int ii = 0;
	int r = 0;

	cali_start = gate;
	if (gate == 3) {
		auto_cali(eth_priv);
		return 0;
	}

	if (argc < 4 || !argv || !argv[0] ||
		!argv[1] || !argv[2] || !argv[3]) {
		pr_err("Invalid syntax\n");
		return -1;
	}

	r = kstrtoint(argv[1], 0, &cali_rise);
	r = kstrtoint(argv[2], 0, &cali_sel);
	r = kstrtoint(argv[3], 0, &cali_time);

	readl(eth_priv->PREG_ETH_REG1);
	value = readl(eth_priv->PREG_ETH_REG0) & (~(0x1f << 25));
	writel(value, eth_priv->PREG_ETH_REG0);

	value = readl(eth_priv->PREG_ETH_REG0) | (cali_start << 25) |
		(cali_rise << 26) | (cali_sel << 27);
	writel(value, eth_priv->PREG_ETH_REG0);

	pr_info("rise :%d   sel: %d  time: %d   start:%d  cbus2050 = %x\n",
		cali_rise, cali_sel, cali_time, cali_start,
			readl(eth_priv->PREG_ETH_REG0));
	for (ii = 0; ii < cali_time; ii++) {
		mdelay(100);
		value = readl(eth_priv->PREG_ETH_REG1);
		if ((value >> 15) & 0x1) {
			pr_info
			("value = %x,len = %d,idx = %d,sel=%d,rise = %d\n",
			value, (value >> 5) & 0x1f, (value & 0x1f),
			(value >> 11) & 0x7, (value >> 14) & 0x1);
			ii++;
		}
	}

	return 0;
}

static ssize_t cali_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int argc;
	char *buff, *p, *para;
	char *argv[5];
	char cmd;
	struct aml_eth_priv *eth_priv = dev_get_drvdata(dev);

	buff = kstrdup(buf, GFP_KERNEL);
	p = buff;
/*only support g12a by now, needn't this*/
/*if (get_cpu_type() < MESON_CPU_MAJOR_ID_M8B) {
 *		pr_err("Sorry ,this cpu is not support cali!\n");
 *		goto end;
 *	}
 */
	for (argc = 0; argc < 5; argc++) {
		para = strsep(&p, " ");
		if (!para)
			break;
		argv[argc] = para;
	}

	if (argc < 1 || argc > 4)
		goto end;

	cmd = argv[0][0];
		switch (cmd) {
		case 'e':
		case 'E':
			am_net_cali(argc, argv, 1, eth_priv);
			break;
		case 'd':
		case 'D':
			am_net_cali(argc, argv, 0, eth_priv);
			break;
		case 'a':
		case 'A':
			am_net_cali(argc, argv, 3, eth_priv);
			break;
		default:
			goto end;
		}
		return count;
end:
	kfree(buff);
	return 0;
}

static DEVICE_ATTR_RO(info);
static DEVICE_ATTR_RW(phyreg);
static DEVICE_ATTR_RW(macreg);
static DEVICE_ATTR_RO(linkspeed);
static DEVICE_ATTR_WO(cali);
#if IS_ENABLED(CONFIG_PM_SLEEP)
static DEVICE_ATTR_RW(wol);
#endif

static struct attribute *phy_dbg_dev_attrs[] = {
	&dev_attr_info.attr,
	&dev_attr_phyreg.attr,
	&dev_attr_macreg.attr,
	&dev_attr_linkspeed.attr,
	&dev_attr_cali.attr,
#if IS_ENABLED(CONFIG_PM_SLEEP)
	&dev_attr_wol.attr,
#endif
	NULL,
};

ATTRIBUTE_GROUPS(phy_dbg_dev);

#if defined(ETH_LEGACY_INTERFACE_SUPPORT)
/* Change the selected device. */
static ssize_t cfg_select_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t len)
{
	char dev_name[64];
	struct device *dev;

	snprintf(dev_name, sizeof(dev_name), "%s", buf);

	if (!strstrip(dev_name))
		return -EINVAL;

	mutex_lock(&phy_dbg_mutex);
	/* Select none */
	if (!strlen(dev_name)) {
		select_dev = NULL;
		goto out;
	}
	/* Select device by name */
	dev = class_find_device_by_name(class, dev_name);
	if (!dev) {
		mutex_unlock(&phy_dbg_mutex);
		return -ENODEV;
	}
	select_dev = dev;
out:
	mutex_unlock(&phy_dbg_mutex);

	return len;
}

/* Show the currently selected device. */
static ssize_t cfg_select_show(const struct class *class,
			const struct class_attribute *attr, char *buf)
{
	ssize_t len = 0;

	mutex_lock(&phy_dbg_mutex);
	if (select_dev)
		len = sysfs_emit(buf, "%s\n", dev_name(select_dev));
	mutex_unlock(&phy_dbg_mutex);

	return len;
}

/* List all available devices. */
static ssize_t cfg_list_show(const struct class *class,
			const struct class_attribute *attr, char *buf)
{
	struct class_dev_iter iter;
	struct device *dev;
	ssize_t len = 0;

	mutex_lock(&phy_dbg_mutex);
	class_dev_iter_init(&iter, class, NULL, NULL);
	while ((dev = class_dev_iter_next(&iter)))
		len += sysfs_emit_at(buf, len, "%s\n", dev_name(dev));
	class_dev_iter_exit(&iter);
	mutex_unlock(&phy_dbg_mutex);

	return len;
}

#define PHY_DBG_CLASS_FUNC_STORE(_name)							\
static ssize_t __##_name##_store(const struct class *class,				\
			const struct class_attribute *attr,				\
			const char *buf, size_t len)					\
{											\
	ssize_t ret = -EACCES;								\
											\
	mutex_lock(&phy_dbg_mutex);							\
	if (select_dev)									\
		ret = _name##_store(select_dev, NULL, buf, len);			\
	mutex_unlock(&phy_dbg_mutex);							\
											\
	return ret;									\
}

#define PHY_DBG_CLASS_FUNC_SHOW(_name)							\
static ssize_t __##_name##_show(const struct class *class,				\
			const struct class_attribute *attr, char *buf)			\
{											\
	ssize_t ret = -EACCES;								\
											\
	mutex_lock(&phy_dbg_mutex);							\
	if (select_dev)									\
		ret = _name##_show(select_dev, NULL, buf);				\
	mutex_unlock(&phy_dbg_mutex);							\
											\
	return ret;									\
}

#define PHY_DBG_CLASS_PROXY_RW(_name)							\
	PHY_DBG_CLASS_FUNC_STORE(_name)							\
	PHY_DBG_CLASS_FUNC_SHOW(_name)							\
	static struct class_attribute class_attr_##_name =				\
		__ATTR(_name, 0644, __##_name##_show, __##_name##_store)

#define PHY_DBG_CLASS_PROXY_RO(_name)							\
	PHY_DBG_CLASS_FUNC_SHOW(_name)							\
	static struct class_attribute class_attr_##_name = {				\
		.attr = { .name = __stringify(_name), .mode = 0444 },			\
		.show = __##_name##_show,						\
	}

#define PHY_DBG_CLASS_PROXY_WO(_name)							\
	PHY_DBG_CLASS_FUNC_STORE(_name)							\
	static struct class_attribute class_attr_##_name = {				\
		.attr = { .name = __stringify(_name), .mode = 0200 },			\
		.store = __##_name##_store,						\
	}

static CLASS_ATTR_RW(cfg_select);
static CLASS_ATTR_RO(cfg_list);

/* Compatibility with legacy interface */
PHY_DBG_CLASS_PROXY_RW(phyreg);
PHY_DBG_CLASS_PROXY_RW(macreg);
PHY_DBG_CLASS_PROXY_RO(linkspeed);
PHY_DBG_CLASS_PROXY_WO(cali);
#if IS_ENABLED(CONFIG_PM_SLEEP)
PHY_DBG_CLASS_PROXY_RW(wol);
#endif

static struct attribute *phy_dbg_class_attrs[] = {
	&class_attr_cfg_select.attr,
	&class_attr_cfg_list.attr,
	&class_attr_phyreg.attr,
	&class_attr_macreg.attr,
	&class_attr_linkspeed.attr,
	&class_attr_cali.attr,
#if IS_ENABLED(CONFIG_PM_SLEEP)
	&class_attr_wol.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(phy_dbg_class);
#endif /* ETH_LEGACY_INTERFACE_SUPPORT */

static struct class phy_dbg_class = {
	.name =		"ethernet",
#if defined(ETH_LEGACY_INTERFACE_SUPPORT)
	.class_groups = phy_dbg_class_groups,
#endif
};

int gmac_create_sysfs(struct aml_eth_priv *eth_priv)
{
	struct phy_device *phydev;
	struct device *dev;
	int ret = 0;

	mutex_lock(&phy_dbg_mutex);

	if (!eth_priv->phydev) {
		pr_info("phydev is null\n");
		ret = -ENODEV;
		goto err0;
	}
	phydev = eth_priv->phydev;

	/* Create a common class located in the /sys/class/ethernet directory. */
	if (!phy_dbg_count) {
		ret = class_register(&phy_dbg_class);
		if (ret)
			goto err0;
	}
	phy_dbg_count++;

	/* Create a separate group for each PHY. */
	dev = device_create_with_groups(&phy_dbg_class, NULL /* parent dev */, MKDEV(0, 0),
					eth_priv, phy_dbg_dev_groups, "%s", phydev_name(phydev));
	if (IS_ERR(dev)) {
		ret = PTR_ERR(dev);
		goto err1;
	}
	eth_priv->dev = dev;

#if defined(ETH_LEGACY_INTERFACE_SUPPORT)
	/* Set the first device as the default. */
	if (phy_dbg_count == 1)
		select_dev = dev;
#endif

	mutex_unlock(&phy_dbg_mutex);

	return 0;

err1:
	if (phy_dbg_count == 1) {
		class_unregister(&phy_dbg_class);
		phy_dbg_count--;
	}
err0:
	mutex_unlock(&phy_dbg_mutex);

	return ret;
}
EXPORT_SYMBOL(gmac_create_sysfs);

int gmac_remove_sysfs(struct aml_eth_priv *eth_priv)
{
	mutex_lock(&phy_dbg_mutex);

	if (eth_priv->dev) {
#if defined(ETH_LEGACY_INTERFACE_SUPPORT)
		if (select_dev == eth_priv->dev)
			select_dev = NULL;
#endif
		device_unregister(eth_priv->dev);
		eth_priv->dev = NULL;
	}

	if (phy_dbg_count) {
		phy_dbg_count--;
		if (!phy_dbg_count)
			class_unregister(&phy_dbg_class);
	}

	eth_priv->phydev = NULL;

	mutex_unlock(&phy_dbg_mutex);

	return 0;
}
EXPORT_SYMBOL(gmac_remove_sysfs);

MODULE_LICENSE("GPL");
