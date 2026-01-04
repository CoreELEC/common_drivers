// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/delay.h>
#include "vpu_reg.h"
#include "vpu.h"

enum vpu_map_e {
	VPU_MAP_CLK = 0,
	VPU_MAP_PWRCTRL,
	VPU_MAP_CBUS,
	VPU_MAP_AOBUS,
	VPU_MAP_VCBUS,
	VPU_MAP_DMC0BUS,
	VPU_MAP_DMC1BUS,
	VPU_MAP_MAX,
};

int vpu_reg_table[] = {
	VPU_MAP_CLK,
	VPU_MAP_VCBUS,
	VPU_MAP_CBUS,
	VPU_MAP_AOBUS,
	VPU_MAP_MAX,
};

int vpu_reg_table_new[] = {
	VPU_MAP_CLK,
	VPU_MAP_PWRCTRL,
	VPU_MAP_VCBUS,
	VPU_MAP_DMC0BUS,
	VPU_MAP_DMC1BUS,
	VPU_MAP_MAX,
};

int vpu_reg_table_c3[] = {
	VPU_MAP_CLK,
	VPU_MAP_VCBUS,
	VPU_MAP_MAX
};

int vpu_reg_table_a4[] = {
	VPU_MAP_CLK,
	VPU_MAP_PWRCTRL,
	VPU_MAP_VCBUS,
	VPU_MAP_MAX
};

struct vpu_reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

static spinlock_t vpu_vcbus_reg_lock;
static struct vpu_reg_map_s *vpu_reg_map;

int vpu_ioremap(struct platform_device *pdev, int *reg_map_table)
{
	int i = 0;
	int *table;
	struct resource *res;
	u32 vpu_io_max = VPU_MAP_MAX;

	//for t7 and t3x and dmc0 and dmc1
	if (vpu_conf.data->chip_type == VPU_CHIP_T7 ||
		vpu_conf.data->chip_type == VPU_CHIP_T3X ||
		vpu_conf.data->chip_type == VPU_CHIP_T3)
		vpu_io_max = VPU_MAP_MAX;
	//for t6w dmc0
	else if (vpu_conf.data->chip_type == VPU_CHIP_T6W ||
			vpu_conf.data->chip_type == VPU_CHIP_TXHD2 ||
			vpu_conf.data->chip_type == VPU_CHIP_T5W ||
			vpu_conf.data->chip_type == VPU_CHIP_S7 ||
			vpu_conf.data->chip_type == VPU_CHIP_S7D)
		vpu_io_max = VPU_MAP_MAX - 1;
	else if (vpu_conf.data->chip_type == VPU_CHIP_C3 ||
			vpu_conf.data->chip_type == VPU_CHIP_C5)
		vpu_io_max = VPU_MAP_MAX - 4;
	else
		vpu_io_max = VPU_MAP_MAX - 2;
	if (!reg_map_table) {
		VPUERR("%s: reg_map_table is null\n", __func__);
		return -1;
	}
	table = reg_map_table;

	vpu_reg_map = devm_kcalloc(&pdev->dev, vpu_io_max,
				   sizeof(struct vpu_reg_map_s),
				   GFP_KERNEL);
	if (!vpu_reg_map)
		return -ENOMEM;

	spin_lock_init(&vpu_vcbus_reg_lock);
	while (i < vpu_io_max) {
		if (table[i] == vpu_io_max)
			break;

		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			VPUERR("%s: vpu_reg resource %d get error\n", __func__, i);
			break;
			//vpu_reg_map = NULL;
			//return -ENOMEM;
		}
		vpu_reg_map[table[i]].base_addr = res->start;
		vpu_reg_map[table[i]].size = resource_size(res);
		vpu_reg_map[table[i]].p =
			devm_ioremap(&pdev->dev, res->start, vpu_reg_map[table[i]].size);
		if (!vpu_reg_map[table[i]].p) {
			vpu_reg_map[table[i]].flag = 0;
			VPUERR("%s: reg map failed: 0x%x\n",
			       __func__, vpu_reg_map[table[i]].base_addr);
			vpu_reg_map = NULL;
			return -ENOMEM;
		}
		vpu_reg_map[table[i]].flag = 1;
		if (vpu_debug_print_flag) {
			VPUPR("%s: reg mapped: 0x%x -> 0x%px, size:0x%x\n",
			      __func__, vpu_reg_map[table[i]].base_addr,
			      vpu_reg_map[table[i]].p, vpu_reg_map[table[i]].size);
		}

		i++;
	}

	return 0;
}

static int check_vpu_ioremap(int n)
{
	int ret = 0;

	if (WARN_ON(!vpu_reg_map || n >= VPU_MAP_MAX ||
		    !vpu_reg_map[n].flag))
		ret = -1;
	return ret;
}

static inline void __iomem *check_vpu_clk_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_CLK;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = (_reg << 2);

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_pwrctrl_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_PWRCTRL;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = (_reg << 2);

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_cbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_CBUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = (_reg << 2);

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_aobus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_AOBUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = (_reg << 2);

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_vcbus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_VCBUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = (_reg << 2);

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_dmc0bus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_DMC0BUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = (_reg << 2);

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

static inline void __iomem *check_vpu_dmc1bus_reg(unsigned int _reg)
{
	void __iomem *p;
	int reg_bus;
	unsigned int reg_offset;

	reg_bus = VPU_MAP_DMC1BUS;
	if (check_vpu_ioremap(reg_bus))
		return NULL;

	reg_offset = (_reg << 2);

	if (WARN_ON(reg_offset >= vpu_reg_map[reg_bus].size))
		return NULL;

	p = vpu_reg_map[reg_bus].p + reg_offset;
	return p;
}

/* ********************************
 * register access api
 * *********************************
 */

unsigned int vpu_clk_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_clk_reg(_reg);
	if (p)
		return readl(p);
	else
		return 0;
};
EXPORT_SYMBOL(vpu_clk_read);

void vpu_clk_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_clk_reg(_reg);
	if (p)
		writel(_value, p);
};
EXPORT_SYMBOL(vpu_clk_write);

void vpu_clk_setb(unsigned int _reg, unsigned int _value,
		  unsigned int _start, unsigned int _len)
{
	vpu_clk_write(_reg, ((vpu_clk_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}
EXPORT_SYMBOL(vpu_clk_setb);

unsigned int vpu_clk_getb(unsigned int _reg, unsigned int _start,
			  unsigned int _len)
{
	return (vpu_clk_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

void vpu_clk_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_clk_write(_reg, (vpu_clk_read(_reg) | (_mask)));
}

void vpu_clk_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_clk_write(_reg, (vpu_clk_read(_reg) & (~(_mask))));
}

unsigned int vpu_pwrctrl_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned int ret = 0;

	p = check_vpu_pwrctrl_reg(_reg);
	if (p)
		ret = readl(p);
	else
		ret = 0;

	return ret;
};

unsigned int vpu_pwrctrl_getb(unsigned int _reg,
			      unsigned int _start, unsigned int _len)
{
	return (vpu_pwrctrl_read(_reg) >> (_start)) & ((1L << (_len)) - 1);
}

unsigned int vpu_vcbus_read(unsigned int _reg)
{
	void __iomem *p;
	unsigned long flags = 0;
	unsigned int ret = 0;

	spin_lock_irqsave(&vpu_vcbus_reg_lock, flags);
	p = check_vpu_vcbus_reg(_reg);
	if (p)
		ret = readl(p);
	else
		ret = 0;
	spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);

	return ret;
};
EXPORT_SYMBOL(vpu_vcbus_read);

void vpu_vcbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpu_vcbus_reg_lock, flags);
	p = check_vpu_vcbus_reg(_reg);
	if (p)
		writel(_value, p);
	spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);
};
EXPORT_SYMBOL(vpu_vcbus_write);

void vpu_vcbus_setb(unsigned int _reg, unsigned int _value,
		    unsigned int _start, unsigned int _len)
{
	void __iomem *p;
	unsigned int temp;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpu_vcbus_reg_lock, flags);
	p = check_vpu_vcbus_reg(_reg);
	if (p) {
		temp = readl(p);
		temp = (temp & (~(((1L << _len) - 1) << _start))) |
			((_value & ((1L << _len) - 1)) << _start);
		writel(temp, p);
	}
	spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);
}
EXPORT_SYMBOL(vpu_vcbus_setb);

unsigned int vpu_vcbus_getb(unsigned int _reg, unsigned int _start,
			    unsigned int _len)
{
	void __iomem *p;
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpu_vcbus_reg_lock, flags);
	p = check_vpu_vcbus_reg(_reg);
	if (p)
		val = readl(p);
	else
		val = 0;
	val = (val >> _start) & ((1L << _len) - 1);
	spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);

	return val;
}
EXPORT_SYMBOL(vpu_vcbus_getb);

void vpu_vcbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	void __iomem *p;
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpu_vcbus_reg_lock, flags);
	p = check_vpu_vcbus_reg(_reg);
	if (!p) {
		spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);
		return;
	}

	val = readl(p);
	val |= _mask;
	writel(val, p);
	spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);
}
EXPORT_SYMBOL(vpu_vcbus_set_mask);

void vpu_vcbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	void __iomem *p;
	unsigned int val;
	unsigned long flags = 0;

	spin_lock_irqsave(&vpu_vcbus_reg_lock, flags);
	p = check_vpu_vcbus_reg(_reg);
	if (!p) {
		spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);
		return;
	}

	val = readl(p);
	val &= ~(_mask);
	writel(val, p);
	spin_unlock_irqrestore(&vpu_vcbus_reg_lock, flags);
}
EXPORT_SYMBOL(vpu_vcbus_clr_mask);

unsigned int vpu_cbus_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_cbus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void vpu_cbus_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_cbus_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_cbus_setb(unsigned int _reg, unsigned int _value,
		   unsigned int _start, unsigned int _len)
{
	vpu_cbus_write(_reg, ((vpu_cbus_read(_reg) &
			~(((1L << (_len)) - 1) << (_start))) |
			(((_value) & ((1L << (_len)) - 1)) << (_start))));
}

void vpu_cbus_set_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) | (_mask)));
}

void vpu_cbus_clr_mask(unsigned int _reg, unsigned int _mask)
{
	vpu_cbus_write(_reg, (vpu_cbus_read(_reg) & (~(_mask))));
}

unsigned int vpu_ao_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_aobus_reg(_reg);
	if (p)
		return readl(p);
	else
		return -1;
};

void vpu_ao_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_aobus_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_ao_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len)
{
	vpu_ao_write(_reg, ((vpu_ao_read(_reg) &
		     ~(((1L << (_len)) - 1) << (_start))) |
		     (((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vpu_dmc0_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_dmc0bus_reg(_reg);
	if (p)
		return readl(p);
	else
		return 0;
};

void vpu_dmc0_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_dmc0bus_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_dmc0_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len)
{
	vpu_dmc0_write(_reg, ((vpu_dmc0_read(_reg) &
		     ~(((1L << (_len)) - 1) << (_start))) |
		     (((_value) & ((1L << (_len)) - 1)) << (_start))));
}

unsigned int vpu_dmc1_read(unsigned int _reg)
{
	void __iomem *p;

	p = check_vpu_dmc1bus_reg(_reg);
	if (p)
		return readl(p);
	else
		return 0;
};

void vpu_dmc1_write(unsigned int _reg, unsigned int _value)
{
	void __iomem *p;

	p = check_vpu_dmc1bus_reg(_reg);
	if (p)
		writel(_value, p);
};

void vpu_dmc1_setb(unsigned int _reg, unsigned int _value,
		 unsigned int _start, unsigned int _len)
{
	vpu_dmc1_write(_reg, ((vpu_dmc1_read(_reg) &
		     ~(((1L << (_len)) - 1) << (_start))) |
		     (((_value) & ((1L << (_len)) - 1)) << (_start))));
}
