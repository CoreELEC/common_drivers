// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/amlogic/iomap.h>
#include "ldim_abcon.h"
#include "ldim_abcon_reg.h"
// #include "../../../lcd_common.h"

#define	ABCON_MAP_BCON_CLK 0
#define ABCON_MAP_BCON 1
#define ABCON_MAP_GPIO 2
#define ABCON_MAP_MAX 3

int abcon_reg[] = {
	ABCON_MAP_BCON_CLK,
	ABCON_MAP_BCON,
	ABCON_MAP_GPIO,
	ABCON_MAP_MAX,
};

/* for bcon reg access */
spinlock_t abcon_reg_spinlock;

int abcon_ioremap(struct platform_device *pdev)
{
	struct reg_map_s *reg_map = NULL;
	struct resource *res;
	int *table;
	int i = 0;

	table = &abcon_reg[0];

	reg_map = kcalloc(ABCON_MAP_MAX, sizeof(struct reg_map_s), GFP_KERNEL);
	if (!reg_map)
		return -1;
	abcon->reg_map = reg_map;

	spin_lock_init(&abcon_reg_spinlock);

	for (i = 0; i < ABCON_MAP_MAX; i++) {
		if (table[i] == ABCON_MAP_MAX)
			break;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			ABCONERR(" %s: get resource %d error\n", __func__, i);
			goto abcon_ioremap_err;
		}

		if (res->start == 0)
			continue;

		reg_map[table[i]].base_addr = res->start;
		reg_map[table[i]].size = resource_size(res);
		reg_map[table[i]].p = devm_ioremap(&pdev->dev, res->start, reg_map[table[i]].size);
		if (!reg_map[table[i]].p) {
			reg_map[table[i]].flag = 0;
			ABCONERR("%s: reg %d failed: 0x%x 0x%x\n",
			       __func__, i,
			       reg_map[table[i]].base_addr,
			       reg_map[table[i]].size);
			goto abcon_ioremap_err;
		}
		reg_map[table[i]].flag = 1;
		if (bl_debug_print_flag & BL_DBG_PR_NORMAL) {
			ABCONERR("%s: reg %d: 0x%x -> 0x%px, size: 0x%x\n",
			      __func__, i,
			      reg_map[table[i]].base_addr,
			      reg_map[table[i]].p,
			      reg_map[table[i]].size);
		}
	}

	return 0;

abcon_ioremap_err:
	kfree(reg_map);
	abcon->reg_map = NULL;
	return -1;
}

static int check_abcon_ioremap(unsigned int n)
{
	if (!abcon->reg_map) {
		BLERR(0, "%s: reg_map is null\n", __func__);
		return -1;
	}
	if (n >= ABCON_MAP_MAX)
		return -1;
	if (abcon->reg_map[n].flag == 0) {
		BLERR(0, "%s: reg 0x%x mapped error\n",
		    __func__, abcon->reg_map[n].base_addr);
		return -1;
	}
	return 0;
}

void __iomem *check_abcon_reg(unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = ABCON_MAP_BCON;
	if (check_abcon_ioremap(reg_bus))
		return NULL;

	reg_offset = ABCON_REG_OFFSET(reg);

	if (reg_offset >= abcon->reg_map[reg_bus].size) {
		BLERR(0, "invalid abcon reg offset: 0x%04x\n", reg);
		return NULL;
	}
	p = abcon->reg_map[reg_bus].p + reg_offset;
	return p;
}

void __iomem *check_abcon_clk_reg(unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = ABCON_MAP_BCON_CLK;
	if (check_abcon_ioremap(reg_bus))
		return NULL;

	reg_offset = ABCON_REG_OFFSET(reg);

	if (reg_offset >= abcon->reg_map[reg_bus].size) {
		BLERR(0, "invalid abcon reg offset: 0x%04x\n", reg);
		return NULL;
	}
	p = abcon->reg_map[reg_bus].p + reg_offset;
	return p;
}

void __iomem *check_abcon_gpio_reg(unsigned int reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = ABCON_MAP_GPIO;
	if (check_abcon_ioremap(reg_bus))
		return NULL;

	reg_offset = ABCON_REG_OFFSET(reg);

	if (reg_offset >= abcon->reg_map[reg_bus].size) {
		BLERR(0, "invalid abcon reg offset: 0x%04x\n", reg);
		return NULL;
	}
	p = abcon->reg_map[reg_bus].p + reg_offset;
	return p;
}

void abcon_wr_reg(unsigned int addr, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_reg(addr);
	if (p)
		writel(val, p);

	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);
};

unsigned int abcon_rd_reg(unsigned int addr)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_reg(addr);
	if (p)
		temp = readl(p);

	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);

	return temp;
};

void abcon_wr_reg_bits(unsigned int addr, unsigned int val,
			unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_reg(addr);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((val & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);
};

unsigned int abcon_rd_reg_bits(unsigned int addr,
				unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_reg(addr);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);

	return temp;
};

void abcon_clk_wr_reg(unsigned int addr, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_clk_reg(addr);
	if (p)
		writel(val, p);

	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);
};

unsigned int abcon_clk_rd_reg(unsigned int addr)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_clk_reg(addr);
	if (p)
		temp = readl(p);

	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);

	return temp;
};

void abcon_wr_gpio(unsigned int addr, unsigned int val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_gpio_reg(addr);
	if (p)
		writel(val, p);

	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);
};

unsigned int abcon_rd_gpio(unsigned int addr)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_gpio_reg(addr);
	if (p)
		temp = readl(p);

	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);

	return temp;
};

void abcon_wr_gpio_bits(unsigned int addr, unsigned int val,
			unsigned int start, unsigned int len)
{
	void __iomem *p;
	unsigned int temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&abcon_reg_spinlock, flags);
	p = check_abcon_gpio_reg(addr);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((val & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&abcon_reg_spinlock, flags);
};
