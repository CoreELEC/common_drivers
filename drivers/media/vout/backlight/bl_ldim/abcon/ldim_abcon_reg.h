/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __LDIM_ABCON_REG_H__
#define __LDIM_ABCON_REG_H__

struct reg_map_s {
	unsigned int base_addr;
	unsigned int size;
	void __iomem *p;
	char flag;
};

/* CLKCTRL_BCON_CLK_CNTL
 * [5:0] clk_div
 * [6] clk_en
 * [9:7] clk_sel
 */
#define CLKCTRL_BCON_CLK_CNTL  0x88	//((0x0088  << 2) + 0xfe000000)

//bcon base addr
#define BCON_APB_BASE_ADDR 0xfe014000
#define BCON_APB_BASE_ADDR_END 0xfe015fff

#define ABCON_REG_OFFSET(reg)                   (((reg) << 2))
#define ABCON_REG_OFFSET_BYTE(reg)              ((reg))

int abcon_ioremap(struct platform_device *pdev);
void abcon_wr_reg(unsigned int addr, unsigned int val);
unsigned int abcon_rd_reg(unsigned int addr);
void abcon_wr_reg_bits(unsigned int addr, unsigned int val,
			unsigned int start, unsigned int len);
unsigned int abcon_rd_reg_bits(unsigned int addr,
				unsigned int start, unsigned int len);

void abcon_clk_wr_reg(unsigned int addr, unsigned int val);
unsigned int abcon_clk_rd_reg(unsigned int addr);

void abcon_wr_gpio(unsigned int addr, unsigned int val);
unsigned int abcon_rd_gpio(unsigned int addr);
void abcon_wr_gpio_bits(unsigned int addr, unsigned int val,
			unsigned int start, unsigned int len);
#endif//__LDIM_ABCON_REG_H__

