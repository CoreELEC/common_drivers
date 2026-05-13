// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/clk_measure.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif

#include <linux/clk.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "./edptx_clk_ctrl.h"
#include "../edptx_common.h"
#include "../edptx_reg_op.h"

struct dptx_clk_div_table_s dptx_clk_div_table[CLK_DIV_SEL_MAX] = {
	/* name,    divider,         num, den, shift_sel, shift_val*/
	{"1",       CLK_DIV_SEL_1,    1,   1,    0,       0xffff},
	{"2",       CLK_DIV_SEL_2,    1,   2,    0,       0x0aaa},
	{"3",       CLK_DIV_SEL_3,    1,   3,    0,       0x0db6},
	{"3.5",     CLK_DIV_SEL_3p5,  2,   7,    1,       0x36cc},
	{"3.75",    CLK_DIV_SEL_3p75, 4,   15,   2,       0x6666},
	{"4",       CLK_DIV_SEL_4,    1,   4,    0,       0x0ccc},
	{"5",       CLK_DIV_SEL_5,    1,   5,    2,       0x739c},
	{"6",       CLK_DIV_SEL_6,    1,   6,    0,       0x0e38},
	{"6.25",    CLK_DIV_SEL_6p25, 4,   25,   3,       0x0000},
	{"7",       CLK_DIV_SEL_7,    1,   7,    1,       0x3c78},
	{"7.5",     CLK_DIV_SEL_7p5,  2,   15,   2,       0x78f0},
	{"12",      CLK_DIV_SEL_12,   1,   12,   0,       0x0fc0},
	{"14",      CLK_DIV_SEL_14,   1,   14,   1,       0x3f80},
	{"15",      CLK_DIV_SEL_15,   1,   15,   2,       0x7f80},
	{"2.5",     CLK_DIV_SEL_2p5,  2,   5,    2,       0x5294},
	{"4.67",    CLK_DIV_SEL_4p67, 3,   14,   1,       0x0ccc},
	{"2.33",    CLK_DIV_SEL_2p33, 3,   7,    1,       0x1aaa},
	{"invalid", CLK_DIV_SEL_MAX,  1,   1,    0,       0xffff},
};

//static const unsigned int od_fb_table[2] = {1, 2};
//static const unsigned int od_table[6] = {1, 2, 4, 8, 16, 32};

/* **********************************
 * lcd controller operation
 * **********************************/
u8 dptx_clk_msr_check(u32 msr_id, u32 freq)
{
	unsigned int clk_msrd;

	if (msr_id == -1)
		return 0;

	clk_msrd = meson_clk_measure(msr_id);
	if (dptx_diff(freq, clk_msrd) >= PLL_CLK_CHECK_MAX) {
		eDP_ERR(NULL, "%s[%d]: exp:%d, msr:%d", __func__, msr_id, freq, clk_msrd);
		return 1;
	}

	return 0;
}

u8 dptx_pll_wait_lock(u32 reg, u8 lock_bit)
{
	u16 pll_lock, wait_loop = PLL_WAIT_LOCK_CNT; /* 200 */

	do {
		dptx_delay_us(50);
		pll_lock = edp_ana_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (pll_lock == 0)
		return 0;

	return 1;
}

/* ****************************************************
 * lcd clk parameters calculate
 * ****************************************************
 */
unsigned long long dptx_clk_pll_div_calc(unsigned long long clk, u8 div_sel, u8 dir)
{
	unsigned long long clk_ret, num, den;

	if (div_sel >= CLK_DIV_SEL_MAX) {
		eDP_ERR(NULL, "clk_div_sel: Invalid parameter\n");
		return 0;
	}

	if (dir == CLK_DIV_I2O) {
		num = dptx_clk_div_table[div_sel].num;
		den = dptx_clk_div_table[div_sel].den;
	} else {
		num = dptx_clk_div_table[div_sel].den;
		den = dptx_clk_div_table[div_sel].num;
	}
	clk_ret = dptx_div_around(clk * num, den);

	return clk_ret;
}

static inline unsigned long long edptx_pll_fvco_calc(unsigned long long pll_fvco,
							struct edptx_clk_cfg_s *pll_config,
							struct dptx_pll_data_s *pll_data)
{
	static const u8 od_fb_table[2] = {1, 2};

	pll_fvco = dptx_div(pll_fvco, od_fb_table[pll_data->pll_od_fb]);
	if (pll_data->pll_0_5_div_en)
		pll_fvco = pll_fvco * 2;
	return pll_fvco;
}

void edptx_check_vco(struct edptx_clk_cfg_s *cconf, unsigned long long pll_fvco)
{
	struct dptx_pll_data_s *pll_data = (struct dptx_pll_data_s *)cconf->pll_data;
	unsigned int m, n;
	unsigned int pll_frac;
	unsigned long long temp;

	if (!pll_data)
		return;

	if (pll_fvco < pll_data->pll_vco_range[0] || pll_fvco > pll_data->pll_vco_range[1]) {
		eDP_PR(0, LOG_E, "pll_fvco %lld is out of range\n", pll_fvco);
		return;
	}

	cconf->pll_fvco = pll_fvco;
	n = 1;
	pll_fvco = edptx_pll_fvco_calc(pll_fvco, cconf, pll_data);
	m = dptx_div(pll_fvco, pll_data->fin_base);
	temp = pll_data->fin_base;
	temp *= m;
	temp = pll_fvco - temp;
	pll_frac = dptx_div_around((temp * pll_data->pll_frac_range * 10), cconf->fin);
	pll_frac = dptx_div_around(pll_frac, 10);
	cconf->pll_m = m;
	cconf->pll_n = n;
	cconf->pll_frac = pll_frac;
	if (cconf->pll_mode & LCD_PLL_MODE_FRAC_SHIFT) {
		if (pll_frac == (pll_data->pll_frac_range >> 1) ||
		    pll_frac == (pll_data->pll_frac_range >> 2)) {
			pll_frac |= 0x66;
			cconf->pll_frac_half_shift = 1;
		} else {
			cconf->pll_frac_half_shift = 0;
		}
	}
	pr_info("m=%d, n=%d, frac=0x%x, pll_fvco=%lld", m, n, pll_frac, pll_fvco);
}
