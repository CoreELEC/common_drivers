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
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/vout/vclk_serve.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include "edptx_common.h"
#include "edptx_reg_op.h"

#define eDPTX_REG_MAP_PERIPHS     0
#define eDPTX_REG_MAP_EDP_A       1
#define eDPTX_REG_MAP_EDP_B       2
#define eDPTX_REG_MAP_COMBO_DPHY  3
#define eDPTX_REG_MAP_RST         4
#define eDPTX_REG_MAP_ANALOG      5
#define eDPTX_REG_MAP_MAX         6

int edptx_reg_t7[] = {
	eDPTX_REG_MAP_COMBO_DPHY,
	eDPTX_REG_MAP_EDP_A,
	eDPTX_REG_MAP_EDP_B,
	eDPTX_REG_MAP_RST,
	eDPTX_REG_MAP_PERIPHS,
	eDPTX_REG_MAP_ANALOG,
	eDPTX_REG_MAP_MAX,
};

int edptx_reg_a9[] = {
	eDPTX_REG_MAP_ANALOG,
	eDPTX_REG_MAP_EDP_A,
	eDPTX_REG_MAP_MAX,
};

#define eDPTX_REG_OFFSET(reg)                   (((reg) << 2))

/* for lcd reg access */
spinlock_t edptx_reg_spinlock; // edp reg io

int edptx_ioremap(struct edptx_drv_s *edp, struct platform_device *pdev)
{
	struct edptx_reg_map_s *reg_map = NULL;
	struct resource *res;
	int *table;
	int i = 0;

	if (!edp->data) {
		eDP_ERR(edp, "%s: reg_map table is null", __func__);
		return -1;
	}

	switch (edp->data->chip_type) {
	case eDPTX_CHIP_T7:
		table = edptx_reg_t7;
		break;
	case eDPTX_CHIP_A9:
		table = edptx_reg_a9;
		break;
	default:
		eDP_ERR(edp, "%s: unknown chip", __func__);
		return -1;
	}

	edp->reg_map = kcalloc(eDPTX_REG_MAP_MAX, sizeof(struct edptx_reg_map_s), GFP_KERNEL);
	if (!edp->reg_map)
		return -1;
	reg_map = edp->reg_map;

	spin_lock_init(&edptx_reg_spinlock);

	for (i = 0; i < eDPTX_REG_MAP_MAX; i++) {
		if (table[i] == eDPTX_REG_MAP_MAX)
			break;
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			eDP_ERR(edp, "%s: get resource %d error", __func__, i);
			goto edptx_ioremap_err;
		}

		if (res->start == 0)
			continue;

		reg_map[table[i]].base_addr = res->start;
		reg_map[table[i]].size = resource_size(res);
		reg_map[table[i]].p = devm_ioremap(&pdev->dev, res->start, reg_map[table[i]].size);
		if (!reg_map[table[i]].p) {
			reg_map[table[i]].flag = 0;
			eDP_ERR(edp, "%s: reg %d failed: 0x%x 0x%x", __func__, i,
			       reg_map[table[i]].base_addr, reg_map[table[i]].size);
			goto edptx_ioremap_err;
		}
		reg_map[table[i]].flag = 1;
		eDP_DBG(edp, "%s: reg %d: 0x%x -> 0x%px, size: 0x%x", __func__, i,
			reg_map[table[i]].base_addr, reg_map[table[i]].p, reg_map[table[i]].size);
	}

	return 0;

edptx_ioremap_err:
	kfree(reg_map);
	edp->reg_map = NULL;
	return -1;
}

static int edptx_check_ioremap(struct edptx_drv_s *edp, u32 n)
{
	if (!edp->reg_map) {
		eDP_ERR(edp, "%s: reg_map is null", __func__);
		return -1;
	}
	if (n >= eDPTX_REG_MAP_MAX)
		return -1;
	if (edp->reg_map[n].flag)
		return 0;

	eDP_ERR(edp, "%s: reg map:%u error", __func__, n);
	return -1;
}

static inline void __iomem *edptx_check_periphs_reg(struct edptx_drv_s *edp, u32 reg)
{
	if (edptx_check_ioremap(edp, eDPTX_REG_MAP_PERIPHS))
		return NULL;
	if (eDPTX_REG_OFFSET(reg) >= edp->reg_map[eDPTX_REG_MAP_PERIPHS].size) {
		eDP_ERR(edp, "invalid periphs reg offset: 0x%04x", reg);
		return NULL;
	}
	return edp->reg_map[eDPTX_REG_MAP_PERIPHS].p + eDPTX_REG_OFFSET(reg);
}

static inline void __iomem *edptx_check_IP_reg(struct edptx_drv_s *edp, u8 port, u32 reg)
{
	u8 ip_reg = port == 1 ? eDPTX_REG_MAP_EDP_B : eDPTX_REG_MAP_EDP_A;

	if (edptx_check_ioremap(edp, ip_reg))
		return NULL;

	if (reg >= edp->reg_map[ip_reg].size) {
		eDP_ERR(edp, "invalid dptx reg offset: 0x%04x", reg);
		return NULL;
	}
	return edp->reg_map[ip_reg].p + reg;
}

static inline void __iomem *edptx_check_combo_dphy_reg(struct edptx_drv_s *edp, u32 reg)
{
	if (edptx_check_ioremap(edp, eDPTX_REG_MAP_COMBO_DPHY))
		return NULL;
	if (eDPTX_REG_OFFSET(reg) >= edp->reg_map[eDPTX_REG_MAP_COMBO_DPHY].size) {
		eDP_ERR(edp, "invalid combo dphy reg offset: 0x%04x", reg);
		return NULL;
	}
	return edp->reg_map[eDPTX_REG_MAP_COMBO_DPHY].p + eDPTX_REG_OFFSET(reg);
}

static inline void __iomem *edptx_check_reset_reg(struct edptx_drv_s *edp, u32 reg)
{
	if (edptx_check_ioremap(edp, eDPTX_REG_MAP_RST))
		return NULL;
	if (eDPTX_REG_OFFSET(reg) >= edp->reg_map[eDPTX_REG_MAP_RST].size) {
		eDP_ERR(edp, "invalid reset reg offset: 0x%04x\n", reg);
		return NULL;
	}
	return edp->reg_map[eDPTX_REG_MAP_RST].p + eDPTX_REG_OFFSET(reg);
}

static inline void __iomem *edptx_check_analog_reg(struct edptx_drv_s *edp, u32 reg)
{
	if (edptx_check_ioremap(edp, eDPTX_REG_MAP_ANALOG))
		return NULL;
	if (eDPTX_REG_OFFSET(reg) >= edp->reg_map[eDPTX_REG_MAP_ANALOG].size) {
		eDP_ERR(edp, "invalid analog reg offset: 0x%04x\n", reg);
		return NULL;
	}
	return edp->reg_map[eDPTX_REG_MAP_ANALOG].p + eDPTX_REG_OFFSET(reg);
}
/******************************************************/
u32 edp_vcbus_read(u32 reg)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	temp = aml_read_vcbus(reg);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [0x%08x]=0x%08x\n", __func__, reg, temp);

	return temp;
};

void edp_vcbus_write(u32 reg, u32 value)
{
	unsigned long flags = 0;

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [0x%08x]=0x%08x\n", __func__, reg, value);

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	aml_write_vcbus(reg, value);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
};

void edp_vcbus_setb(u32 reg, u32 value, u32 start, u32 len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	aml_write_vcbus(reg, ((aml_read_vcbus(reg) &
		(~(((1L << len) - 1) << start))) |
		((value & ((1L << len) - 1)) << start)));
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

u32 edp_vcbus_getb(u32 reg, u32 start, u32 len)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	temp = (aml_read_vcbus(reg) >> start) & ((1L << len) - 1);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
}

u32 edp_clk_read(u32 reg)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	temp =  vclk_clk_reg_read(reg);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [0x%08x]=0x%08x\n", __func__, reg, temp);

	return temp;
};

void edp_clk_write(u32 reg, u32 val)
{
	unsigned long flags = 0;

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [0x%08x]=0x%08x\n", __func__, reg, val);

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	vclk_clk_reg_write(reg, val);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
};

void edp_clk_setb(u32 reg, u32 val, u32 start, u32 len)
{
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	vclk_clk_reg_write(reg, ((vclk_clk_reg_read(reg) &
			   ~(((1L << (len)) - 1) << (start))) |
			   (((val) & ((1L << (len)) - 1)) << (start))));
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

u32 edp_clk_getb(u32 reg, u32 start, u32 len)
{
	u32 temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	temp =  (vclk_clk_reg_read(reg) >> (start)) & ((1L << (len)) - 1);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
}

u32 edp_ana_read(struct edptx_drv_s *edp, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_analog_reg(edp, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [0x%08x]=0x%08x\n", __func__, reg, temp);

	return temp;
}

void edp_ana_write(struct edptx_drv_s *edp, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [0x%08x]=0x%08x\n", __func__, reg, val);

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_analog_reg(edp, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

void edp_ana_setb(struct edptx_drv_s *edp, u32 reg, u32 val, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_analog_reg(edp, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((val & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

u32 edp_ana_getb(struct edptx_drv_s *edp, u32 reg, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_analog_reg(edp, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_periphs_read(struct edptx_drv_s *edp, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_periphs_reg(edp, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
};

void dptx_periphs_write(struct edptx_drv_s *edp, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_periphs_reg(edp, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
};

u32 edp_reg_read(struct edptx_drv_s *edp, u8 port, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_IP_reg(edp, port, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [%d][0x%04x]=0x%08x\n", __func__, edp->idx, reg, temp);

	return temp;
};

void edp_reg_write(struct edptx_drv_s *edp, u8 port, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [%d][0x%04x]=0x%08x\n", __func__, edp->idx, reg, val);

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_IP_reg(edp, port, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
};

void edp_reg_setb(struct edptx_drv_s *edp, u8 port, u32 reg, u32 value, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_IP_reg(edp, port, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

u32 edp_reg_getb(struct edptx_drv_s *edp, u8 port, u32 reg, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_IP_reg(edp, port, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_combo_dphy_read(struct edptx_drv_s *edp, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_combo_dphy_reg(edp, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [%d][0x%04x]=0x%08x\n", __func__, edp->idx, reg, temp);

	return temp;
};

void dptx_combo_dphy_write(struct edptx_drv_s *edp, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	if (edptx_print_level >= LOG_A)
		pr_info("%s: [%d][0x%04x]=0x%08x\n", __func__, edp->idx, reg, val);

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_combo_dphy_reg(edp, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
};

void dptx_combo_dphy_setb(struct edptx_drv_s *edp, u32 reg, u32 value, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_combo_dphy_reg(edp, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

u32 dptx_combo_dphy_getb(struct edptx_drv_s *edp, u32 reg, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_combo_dphy_reg(edp, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
}

u32 dptx_reset_read(struct edptx_drv_s *edp, u32 reg)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_reset_reg(edp, reg);
	if (p)
		temp = readl(p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
};

void dptx_reset_write(struct edptx_drv_s *edp, u32 reg, u32 val)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_reset_reg(edp, reg);
	if (p)
		writel(val, p);
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
};

void dptx_reset_setb(struct edptx_drv_s *edp, u32 reg, u32 value, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_reset_reg(edp, reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << len) - 1) << start))) |
			((value & ((1L << len) - 1)) << start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);
}

u32 dptx_reset_getb(struct edptx_drv_s *edp, u32 reg, u32 start, u32 len)
{
	void __iomem *p;
	u32 temp = 0;
	unsigned long flags = 0;

	spin_lock_irqsave(&edptx_reg_spinlock, flags);
	p = edptx_check_reset_reg(edp, reg);
	if (p) {
		temp = readl(p);
		temp = (temp >> start) & ((1L << len) - 1);
	}
	spin_unlock_irqrestore(&edptx_reg_spinlock, flags);

	return temp;
}
