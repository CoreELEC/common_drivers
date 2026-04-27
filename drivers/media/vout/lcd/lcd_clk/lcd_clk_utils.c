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
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/clk.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "lcd_clk_config.h"
#include "lcd_clk_ctrl.h"
#include "lcd_clk_utils.h"
#include "../connectors/lcd_connector.h"

unsigned int tcon_div[5][3] = {
	/* div_mux, div2/4_sel, div4_bypass */
	{1, 0, 1},	/* div1 */
	{0, 0, 1},	/* div2 */
	{0, 1, 1},	/* div4 */
	{0, 0, 0},	/* div8 */
	{0, 1, 0},	/* div16 */
};

#define LCD_PLL_SEL_PHY 0
#define LCD_PLL_SEL_PIX 1

static unsigned int edp_div0_table[15] = {1, 2, 3, 4, 5, 7, 8, 9, 11, 13, 17, 19, 23, 29, 31};
static unsigned int edp_div1_table[10] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 13};

const unsigned int od_table[6] = {1, 2, 4, 8, 16, 32};
const unsigned int od_table_div_5_7[4] = {1, 5, 1, 7};
const unsigned int od_fb_table[2] = {1, 2};
const unsigned int tcon_div_table[5] = {1, 2, 4, 8, 16};

struct lcd_clk_div_table_s lcd_clk_div_table[] = {
	/* name, num, den, shift_sel, shift_val    divider */
	{"1",     1,   1,    0,       0xffff},  //CLK_DIV_SEL_1,
	{"2",     1,   2,    0,       0x0aaa},  //CLK_DIV_SEL_2,
	{"3",     1,   3,    0,       0x0db6},  //CLK_DIV_SEL_3,
	{"3.5",   2,   7,    1,       0x36cc},  //CLK_DIV_SEL_3p5,
	{"3.75",  4,   15,   2,       0x6666},  //CLK_DIV_SEL_3p75,
	{"4",     1,   4,    0,       0x0ccc},  //CLK_DIV_SEL_4,
	{"5",     1,   5,    2,       0x739c},  //CLK_DIV_SEL_5,
	{"6",     1,   6,    0,       0x0e38},  //CLK_DIV_SEL_6,
	{"6.25",  4,   25,   3,       0x0000},  //CLK_DIV_SEL_6p25,
	{"7",     1,   7,    1,       0x3c78},  //CLK_DIV_SEL_7,
	{"7.5",   2,   15,   2,       0x78f0},  //CLK_DIV_SEL_7p5,
	{"12",    1,   12,   0,       0x0fc0},  //CLK_DIV_SEL_12,
	{"14",    1,   14,   1,       0x3f80},  //CLK_DIV_SEL_14,
	{"15",    1,   15,   2,       0x7f80},  //CLK_DIV_SEL_15,
	{"2.5",   2,   5,    2,       0x5294},  //CLK_DIV_SEL_2p5,
	{"4.67",  3,   14,   1,       0x0ccc},  //CLK_DIV_SEL_4p67,
	{"2.33",  3,   7,    1,       0x1aaa},  //CLK_DIV_SEL_2p33,
	{"2.22",  9,   20,   4,       0xa554a}, //CLK_DIV_SEL_2p22,
	{"2.25",  4,   9,    5,       0x2aa5549},  //CLK_DIV_SEL_2p25,
};

static unsigned char lcd_ss_freq_dep_opt[] = {
/*             freq, cnt, dep values       */
/* 0-29.5k */	0,    3,   4, 7, 10,
/* 1-31.5k */	1,    3,   3, 8, 11,
/* 2-50.0k */	2,    3,   5, 6, 11,
/* 3-75.0k */	3,    3,   4, 7, 11,
/* 4-100k  */	4,    5,   3, 5, 6, 8, 11,
/* 5-150k  */	5,    5,   2, 4, 7, 9, 11,
/* 6-200k  */	6,    12,  1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
/* 7-0 end */   7,    0,
};

/* ****************************************************
 * lcd pll & clk operation
 * ****************************************************
 */

int lcd_clk_msr_check(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int encl_clk_msr;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return 0;

	if (cconf->data->enc_clk_msr_id == -1)
		return 0;

	encl_clk_msr = lcd_encl_clk_msr(pdrv);
	if (lcd_u32_diff(cconf->fout, encl_clk_msr) >= PLL_CLK_CHECK_MAX) {
		LCD_ERR(pdrv, "encl clk: exp:%d, msr:%d", cconf->fout, encl_clk_msr);
		return -1;
	}

	return 0;
}

void lcd_ss_optimize_print(struct aml_lcd_drv_s *pdrv)
{
	unsigned int dep_cnt = 0, len = 0, i = 0, dep_base;
	unsigned char *freq_dep = lcd_ss_freq_dep_opt;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	char *buf;

	if (!cconf)
		return;

	if (cconf->data->ss_freq_dep_opt)
		freq_dep = cconf->data->ss_freq_dep_opt;

	buf = kmalloc(512, GFP_KERNEL);
	if (!buf)
		return;
	dep_base = cconf->data->ss_dep_base;
	len += sprintf(buf + len, "str_m: 1~10 dep_base:%d\n", dep_base);
	for (; ; freq_dep += (freq_dep[1] + 2)) {
		if (freq_dep[1] == 0) {
			pr_info("%s\n", buf);
			break;
		}
		dep_cnt = freq_dep[1];
		len += sprintf(buf + len, "freq %d stable dep_sel:", freq_dep[0]);
		for (i = 0; i < dep_cnt; i++)
			len += sprintf(buf + len, " %d", freq_dep[i + 2]);
		len += sprintf(buf + len, "\n");
	}
	kfree(buf);
}

int lcd_pll_ss_level_generate_optimized(struct lcd_clk_config_s *cconf)
{
	int dep_sel, str_m, target, ss_ppm, dep_base, err = 0, done = 0;
	unsigned int freq, dep_cnt = 0, i = 0;
	unsigned char *freq_dep = lcd_ss_freq_dep_opt;

	if (!cconf)
		return -1;

	if (cconf->data->ss_freq_dep_opt)
		freq_dep = cconf->data->ss_freq_dep_opt;

	target = cconf->ss_level;
	target *= 1000;
	dep_base = cconf->data->ss_dep_base;
	freq = cconf->ss_freq;
	for (; freq != (unsigned int)freq_dep[0]; freq_dep += (freq_dep[1] + 2)) {
		if (freq_dep[1] == 0) {
			LCD_PR(NULL, "no optimized ssc freq matched");
			return 0;
		}
	}
	dep_cnt = freq_dep[1];
	freq_dep += 2;
	for (i = 0; i < dep_cnt; i++) {
		dep_sel = freq_dep[i];
		for (str_m = 1; str_m <= cconf->data->ss_str_m_max; str_m++) {
			ss_ppm = dep_sel * str_m * dep_base;

			err = target - ss_ppm;
			if (err <= dep_base && err >= -dep_base) {
				cconf->ss_dep_sel = dep_sel;
				cconf->ss_str_m = str_m;
				cconf->ss_ppm = ss_ppm;
				cconf->ss_freq_stable = 1;
				done = 1;
				if (err == 0)
					return 1;
			}
		}
	}

	return done;
}

int lcd_pll_ss_level_generate(struct lcd_clk_config_s *cconf)
{
	unsigned int dep_sel, str_m, err = 0, min = 0, done = 0;
	unsigned long long target, ss_ppm, dep_base;

	if (!cconf)
		return -1;

	if (lcd_pll_ss_level_generate_optimized(cconf) > 0)
		goto lcd_pll_ss_level_generate_exit;

	target = cconf->ss_level;
	target *= 1000;
	min = cconf->data->ss_dep_base * 10;
	dep_base = cconf->data->ss_dep_base;
	for (str_m = 1; str_m <= cconf->data->ss_str_m_max; str_m++) { //str_m
		for (dep_sel = 1; dep_sel <= cconf->data->ss_dep_sel_max; dep_sel++) { //dep_sel
			ss_ppm = dep_sel * str_m * dep_base;
			if (ss_ppm > target)
				break;
			err = target - ss_ppm;
			if (err < min) {
				min = err;
				cconf->ss_dep_sel = dep_sel;
				cconf->ss_str_m = str_m;
				cconf->ss_ppm = ss_ppm;
				done++;
			}
		}
	}
	if (done == 0) {
		LCD_ERR(NULL, "pll gen ss: invalid ss_level %d", cconf->ss_level);
		return -1;
	}
	cconf->ss_freq_stable = 0;

lcd_pll_ss_level_generate_exit:
	LCD_DBG_ADV(NULL, "pll gen ss: dep_sel=%d, str_m=%d, error=%d",
		cconf->ss_dep_sel, cconf->ss_str_m, min);

	return 0;
}

void lcd_pll_ss_init_dft(struct lcd_clk_config_s *cconf)
{
	int ret;

	if (!cconf)
		return;

	if (cconf->ss_level > 0) {
		ret = lcd_pll_ss_level_generate(cconf);
		if (ret == 0)
			cconf->ss_en = 1;
	} else {
		cconf->ss_en = 0;
	}
}

int lcd_pll_wait_lock(int id, unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = PLL_WAIT_LOCK_CNT; /* 200 */
	int ret = 0;

	do {
		usleep_range(40, 60);
		pll_lock = lcd_ana_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (pll_lock == 0)
		ret = -1;
	LCD_PR(NULL, "PLL[%d]: lock=%d, wait_loop=%d",
			id, pll_lock, (PLL_WAIT_LOCK_CNT - wait_loop));

	return ret;
}

int lcd_pll_wait_lock_hiu(unsigned int reg, unsigned int lock_bit)
{
	unsigned int pll_lock;
	int wait_loop = PLL_WAIT_LOCK_CNT; /* 200 */
	int ret = 0;

	do {
		usleep_range(100, 120);
		pll_lock = lcd_hiu_getb(reg, lock_bit, 1);
		wait_loop--;
	} while ((pll_lock == 0) && (wait_loop > 0));
	if (pll_lock == 0)
		ret = -1;
	LCD_PR(NULL, "PLL: lock=%d, wait_loop=%d",
		pll_lock, (PLL_WAIT_LOCK_CNT - wait_loop));

	return ret;
}

/* ****************************************************
 * lcd clk parameters calculate
 * ****************************************************
 */
unsigned long long clk_vid_pll_div_calc(unsigned long long clk, unsigned int div_sel, int dir)
{
	unsigned long long clk_ret, num, den;

	if (div_sel > CLK_DIV_SEL_MAX) {
		LCD_ERR(NULL, "clk_div_sel: Invalid parameter");
		return 0;
	}

	if (dir == CLK_DIV_I2O) {
		num = lcd_clk_div_table[div_sel].num;
		den = lcd_clk_div_table[div_sel].den;
	} else {
		num = lcd_clk_div_table[div_sel].den;
		den = lcd_clk_div_table[div_sel].num;
	}
	clk_ret = div_around(clk * num, den);

	return clk_ret;
}

static inline unsigned long long lcd_pll_real_fvco_calc(unsigned long long pll_fvco,
							struct lcd_pll_config_s *pll_config,
							struct lcd_pll_data_s *pll_data)
{
	pll_fvco = lcd_do_div(pll_fvco, od_fb_table[pll_data->pll_od_fb]);
	if (pll_data->pll_0_5_div_en)
		pll_fvco = pll_fvco * 2;
	return pll_fvco;
}

int lcd_pll_get_frac(struct lcd_clk_config_s *cconf, int pll_sel, unsigned long long pll_fvco)
{
	unsigned int frac_range, frac, m;
	unsigned long long fvco_calc, temp;
	struct lcd_pll_config_s *pll_config = &cconf->pll_config[pll_sel];
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];
	unsigned long long fvco_diff;

	fvco_diff = pll_fvco >= pll_config->pll_fvco ?
			pll_fvco - pll_config->pll_fvco :
			pll_config->pll_fvco - pll_fvco;
	if (fvco_diff > (2 * cconf->fin)) {
		LCD_ERR(NULL, "pll get frac: pll changing %lldHz too much", fvco_diff);
		return -1;
	}

	frac_range = pll_data->pll_frac_range;
	fvco_calc = lcd_pll_real_fvco_calc(pll_fvco, pll_config, pll_data);
	m = lcd_do_div(fvco_calc, cconf->fin);
	temp = cconf->fin;
	temp *= m;
	temp = fvco_calc - temp;
	frac = lcd_do_div((temp * pll_data->pll_frac_range * 10), cconf->fin) + 5;
	frac /= 10;

	if (cconf->pll_mode & LCD_PLL_MODE_FRAC_SHIFT) {
		if ((frac == (frac_range >> 1)) || (frac == (frac_range >> 2))) {
			frac |= 0x66;
			pll_config->pll_frac_half_shift = 1;
		} else {
			pll_config->pll_frac_half_shift = 0;
		}
	}
	pll_config->pll_m = m;
	pll_config->pll_frac = frac;
	LCD_DBG(NULL, "pll get frac: 0x%x", pll_config->pll_frac);

	return 0;
}

/***** calculate pll_vco m,n,frac *****/
int check_vco(struct lcd_clk_config_s *cconf, int pll_sel, unsigned long long pll_fvco)
{
	struct lcd_pll_config_s *pll_config = &cconf->pll_config[pll_sel];
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];
	unsigned int m, n, pll_frac;
	unsigned long long fvco_calc, temp;
	int done = 0;

	if (pll_fvco < pll_data->pll_vco_fmin || pll_fvco > pll_data->pll_vco_fmax) {
		LCD_DBG_CLK(NULL, "vco %lld out of range", pll_fvco);
		return done;
	}

	pll_config->pll_fvco = pll_fvco;
	n = 1;
	fvco_calc = lcd_pll_real_fvco_calc(pll_fvco, pll_config, pll_data);
	m = lcd_do_div(fvco_calc, cconf->fin);
	temp = cconf->fin;
	temp *= m;
	temp = fvco_calc - temp;
	pll_frac = lcd_do_div((temp * pll_data->pll_frac_range * 10), cconf->fin) + 5;
	pll_frac /= 10;
	pll_config->pll_m = m;
	pll_config->pll_n = n;
	pll_config->pll_frac = pll_frac;
	if (cconf->pll_mode & LCD_PLL_MODE_FRAC_SHIFT) {
		if ((pll_frac == (pll_data->pll_frac_range >> 1)) ||
		    (pll_frac == (pll_data->pll_frac_range >> 2))) {
			pll_frac |= 0x66;
			pll_config->pll_frac_half_shift = 1;
		} else {
			pll_config->pll_frac_half_shift = 0;
		}
	}
	LCD_DBG_CLK(NULL, "m=%d, n=%d, frac=0x%x, vco=%lld", m, n, pll_frac, pll_fvco);
	done = 1;

	return done;
}

/******* calculate 1od and 3od setting @pll_od_setting_generate *******/
int generate_pll_3od_setting(struct lcd_clk_config_s *cconf,
					int pll_sel, unsigned long long pll_fout)
{
	struct lcd_pll_config_s *pll_config = &cconf->pll_config[pll_sel];
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];
	unsigned int od1_sel, od2_sel, od3_sel, od1, od2, od3;
	unsigned long long pll_fod2_in, pll_fod3_in, pll_fvco;
	int done;

	done = 0;
	if (pll_fout > pll_data->pll_out_fmax ||
	    pll_fout < pll_data->pll_out_fmin) {
		return done;
	}

	for (od3_sel = pll_data->pll_od_sel_max; od3_sel > 0; od3_sel--) {
		od3 = od_table[od3_sel - 1];
		pll_fod3_in = pll_fout * od3;
		for (od2_sel = od3_sel; od2_sel > 0; od2_sel--) {
			od2 = od_table[od2_sel - 1];
			pll_fod2_in = pll_fod3_in * od2;
			for (od1_sel = od2_sel; od1_sel > 0; od1_sel--) {
				od1 = od_table[od1_sel - 1];
				pll_fvco = pll_fod2_in * od1;
				LCD_DBG_CLK(NULL, "od1=%d od2=%d od3=%d vco=%lld",
					(od1_sel - 1), (od2_sel - 1), (od3_sel - 1), pll_fvco);
				done = check_vco(cconf, pll_sel, pll_fvco);
				if (done) {
					pll_config->pll_od1_sel = od1_sel - 1;
					pll_config->pll_od2_sel = od2_sel - 1;
					pll_config->pll_od3_sel = od3_sel - 1;
					pll_config->pll_fout = pll_fout;
					return done;
				}
			}
		}
	}
	return done;
}

static int generate_pll_1od_setting(struct lcd_clk_config_s *cconf,
					int pll_sel, unsigned long long pll_fout)
{
	struct lcd_pll_config_s *pll_config = &cconf->pll_config[pll_sel];
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];
	unsigned int od_sel, od;
	unsigned long long pll_fvco;
	int done = 0;

	if (pll_fout > pll_data->pll_out_fmax || pll_fout < pll_data->pll_out_fmin)
		return done;

	for (od_sel = pll_data->pll_od_sel_max; od_sel > 0; od_sel--) {
		od = od_table[od_sel - 1];
		pll_fvco = pll_fout * od;
		LCD_DBG_CLK(NULL, "od_sel=%d pll_fvco=%lld", (od_sel - 1), pll_fvco);
		done = check_vco(cconf, pll_sel, pll_fvco);
		if (done) {
			pll_config->pll_od1_sel = od_sel - 1;
			pll_config->pll_fout = pll_fout;
			break;
		}
	}
	return done;
}

static int generate_pll_0od_setting(struct lcd_clk_config_s *cconf,
					int pll_sel, unsigned long long pll_fout)
{
	struct lcd_pll_config_s *pll_config = &cconf->pll_config[pll_sel];
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];
	unsigned long long pll_fvco;
	int done;

	if (pll_fout > pll_data->pll_out_fmax || pll_fout < pll_data->pll_out_fmin)
		return 0;
	pll_fvco = pll_fout;

	done = check_vco(cconf, pll_sel, pll_fvco);
	if (done)
		pll_config->pll_fout = pll_fout;

	return 1;
}

static int pll_od_setting_generate(struct lcd_clk_config_s *cconf,
				int pll_sel, unsigned long long pll_fout)
{
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];

	if (pll_data->od_cnt == 3)
		return generate_pll_3od_setting(cconf, pll_sel, pll_fout);
	else if (pll_data->od_cnt == 0)
		return generate_pll_0od_setting(cconf, pll_sel, pll_fout);
	else
		return generate_pll_1od_setting(cconf, pll_sel, pll_fout);
}

#ifdef CONFIG_AMLOGIC_LCD_TCON
static int check_3od(struct lcd_clk_config_s *cconf, int pll_sel, unsigned long long pll_fout)
{
	struct lcd_pll_config_s *pll_config = &cconf->pll_config[pll_sel];
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];
	unsigned int od1_sel, od2_sel, od3_sel, od1, od2, od3;
	unsigned long long pll_fod2_in, pll_fod3_in, pll_fvco;
	int done = 0;

	if (pll_fout > pll_data->pll_out_fmax || pll_fout < pll_data->pll_out_fmin)
		return done;

	for (od3_sel = pll_data->pll_od_sel_max; od3_sel > 0; od3_sel--) {
		od3 = od_table[od3_sel - 1];
		pll_fod3_in = pll_fout * od3;
		for (od2_sel = od3_sel; od2_sel > 0; od2_sel--) {
			od2 = od_table[od2_sel - 1];
			pll_fod2_in = pll_fod3_in * od2;
			for (od1_sel = od2_sel; od1_sel > 0; od1_sel--) {
				od1 = od_table[od1_sel - 1];
				pll_fvco = pll_fod2_in * od1;
				if (pll_fvco < pll_data->pll_vco_fmin ||
				    pll_fvco > pll_data->pll_vco_fmax) {
					continue;
				}
				if (pll_fvco == pll_config->pll_fvco) {
					pll_config->pll_od1_sel = od1_sel - 1;
					pll_config->pll_od2_sel = od2_sel - 1;
					pll_config->pll_od3_sel = od3_sel - 1;
					pll_config->pll_fout = pll_fout;
					LCD_DBG_CLK(NULL, "od1=%d od2=%d od3=%d",
						 (od1_sel - 1),  (od2_sel - 1), (od3_sel - 1));
					done = 1;
					break;
				}
			}
		}
	}
	return done;
}

static int lcd_clk_generate_p2p_with_tcon_div(struct lcd_clk_config_s *cconf,
		unsigned long long bit_rate)
{
	unsigned long long pll_fout, pll_fvco, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd, tcon_div_sel = 0;
	int done = 0;

	for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
		pll_fvco = bit_rate * tcon_div_table[tcon_div_sel];
		done = check_vco(cconf, LCD_PLL_SEL_PHY, pll_fvco);
		if (done == 0)
			continue;
		for (xd = 1; xd <= cconf->data->xd_max; xd++) {
			clk_div_out = cconf->fout * xd;
			if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
				continue;
			LCD_DBG_CLK(NULL, "fout=%d, xd=%d, clk_div_out=%d, tcon_div_sel=%d",
					cconf->fout, xd, clk_div_out, tcon_div_sel);
			for (clk_div_sel = CLK_DIV_SEL_1;
				clk_div_sel <= cconf->data->pll_data[0]->div_sel_max;
				clk_div_sel++) {
				clk_div_in = clk_vid_pll_div_calc(clk_div_out,
						clk_div_sel, CLK_DIV_O2I);
				if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
					continue;
				cconf->xd = xd;
				cconf->pll_config[0].div_sel = clk_div_sel;
				cconf->pll_config[0].pll_div_fout = clk_div_out;
				cconf->pll_tcon_div_sel = tcon_div_sel;
				cconf->phy_clk = bit_rate;
				pll_fout = clk_div_in;
				LCD_DBG_CLK(NULL, "clk_div_sel=%s(%d) pll_fout=%lld",
						lcd_clk_div_table[clk_div_sel].name,
						clk_div_sel, pll_fout);
				done = check_3od(cconf, LCD_PLL_SEL_PHY, pll_fout);
				if (done)
					goto generate_phy_clk_od_div_5_7_done;
			}
		}
	}

generate_phy_clk_od_div_5_7_done:
	return done;
}

static int lcd_clk_generate_p2p_without_tcon_div(struct lcd_clk_config_s *cconf,
		unsigned long long bit_rate)
{
	unsigned long long pll_fout, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd;
	int done = 0;

	if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL) {
		cconf->phy_clk = bit_rate;
		cconf->pll_config[0].pll_fout = cconf->phy_clk;
		//fix phy pll_div for clkmsr phy_clk
		cconf->pll_config[0].div_sel = CLK_DIV_SEL_5;
		clk_div_in = cconf->pll_config[0].pll_fout;
		clk_div_out = clk_vid_pll_div_calc(clk_div_in,
					cconf->pll_config[0].div_sel, CLK_DIV_I2O);
		cconf->pll_config[0].pll_div_fout = clk_div_out;
		//store final encl_clk for clkmsr check
		LCD_DBG_CLK(NULL, "cconf: clk_div_sel=%s(%d) phy_clk=%lld clk_div_out=%u",
				lcd_clk_div_table[cconf->pll_config[0].div_sel].name,
				cconf->pll_config[0].div_sel, cconf->phy_clk,
				cconf->pll_config[0].pll_div_fout);

		done = pll_od_setting_generate(cconf, LCD_PLL_SEL_PHY, cconf->phy_clk);
		if (done == 0) {
			LCD_ERR(NULL, "clk gen p2p: wrong phy_clk %lldHz", cconf->phy_clk);
			goto p2p_clk_without_tcon_div_done;
		}
	} else {
		for (xd = 1; xd <= cconf->data->xd_max; xd++) {
			clk_div_out = cconf->fout * xd;
			if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
				continue;
			LCD_DBG_CLK(NULL, "fout=%d xd=%d clk_div_out=%d",
					cconf->fout, xd, clk_div_out);
			for (clk_div_sel = CLK_DIV_SEL_1;
				clk_div_sel <= cconf->data->pll_data[0]->div_sel_max;
				clk_div_sel++) {
				clk_div_in = clk_vid_pll_div_calc(clk_div_out,
						clk_div_sel, CLK_DIV_O2I);
				if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
					continue;
				LCD_DBG_CLK(NULL,
					"clk_div_sel=%s(%d) clk_div_in=%lld bit_rate=%lld",
					lcd_clk_div_table[clk_div_sel].name,
					clk_div_sel, clk_div_in, bit_rate);
				if (clk_div_in == bit_rate) {
					cconf->xd = xd;
					cconf->pll_config[0].div_sel = clk_div_sel;
					cconf->pll_config[0].pll_div_fout = clk_div_out;
					cconf->phy_clk = bit_rate;
					pll_fout = clk_div_in;
					LCD_DBG_CLK(NULL, "pll_fout=%lld", pll_fout);

					done = pll_od_setting_generate(cconf,
						LCD_PLL_SEL_PHY, pll_fout);
					if (done)
						goto p2p_clk_without_tcon_div_done;
				}
			}
		}
	}

p2p_clk_without_tcon_div_done:
	return done;
}
#endif

static int check_3od_div_5_7(struct lcd_clk_config_s *cconf,
				int pll_sel, unsigned long long pll_fout)
{
	struct lcd_pll_config_s *pll_config = &cconf->pll_config[pll_sel];
	struct lcd_pll_data_s *pll_data = cconf->data->pll_data[pll_sel];
	unsigned int od1_sel, od2_sel, od3_sel, od1, od2, od3;
	unsigned long long pll_fod2_in, pll_fod3_in, pll_fvco;
	int done = 0;

	if (pll_fout > pll_data->pll_out_fmax || pll_fout < pll_data->pll_out_fmin)
		return done;

	for (od3_sel = pll_data->pll_od_sel_max; od3_sel > 0; od3_sel--) {
		od3 = od_table[od3_sel - 1];
		pll_fod3_in = pll_fout * od3;
		for (od2_sel = pll_data->pll_od_sel_max; od2_sel > 0; od2_sel--) {
			od2 = od_table_div_5_7[od2_sel - 1];
			pll_fod2_in = pll_fod3_in * od2;
			for (od1_sel = od3_sel; od1_sel > 0; od1_sel--) {
				od1 = od_table[od1_sel - 1];
				pll_fvco = pll_fod2_in * od1;
				if (pll_fvco < pll_data->pll_vco_fmin ||
				    pll_fvco > pll_data->pll_vco_fmax) {
					continue;
				}
				if (pll_fvco == pll_config->pll_fvco) {
					pll_config->pll_od1_sel = od1_sel - 1;
					pll_config->pll_od2_sel = od2_sel - 1;
					pll_config->pll_od3_sel = od3_sel - 1;
					pll_config->pll_fout = pll_fout;
					if (lcd_debug_print_flag & LCD_DBG_PR_CLK) {
						LCDPR("od1=%d, od2=%d, od3=%d\n",
						      (od1_sel - 1), (od2_sel - 1),
						      (od3_sel - 1));
					}
					done = 1;
					break;
				}
			}
		}
	}
	return done;
}

static int lcd_clk_generate_phy_clk_od_div_5_7(struct lcd_clk_config_s *cconf,
		unsigned long long bit_rate)
{
	unsigned long long pll_fout, pll_fvco, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd, tcon_div_sel = 0;
	int done = 0;

	for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
		pll_fvco = bit_rate * tcon_div_table[tcon_div_sel];
		done = check_vco(cconf, LCD_PLL_SEL_PHY, pll_fvco);
		if (done == 0)
			continue;
		for (xd = 1; xd <= cconf->data->xd_max; xd++) {
			clk_div_out = cconf->fout * xd;
			if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
				continue;
			if (lcd_debug_print_flag & LCD_DBG_PR_CLK) {
				LCDPR("fout=%d, xd=%d, clk_div_out=%d, tcon_div_sel=%d\n",
					cconf->fout, xd, clk_div_out, tcon_div_sel);
			}
			for (clk_div_sel = CLK_DIV_SEL_1;
				clk_div_sel <= cconf->data->pll_data[0]->div_sel_max;
				clk_div_sel++) {
				clk_div_in = clk_vid_pll_div_calc(clk_div_out,
						clk_div_sel, CLK_DIV_O2I);
				if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
					continue;
				cconf->xd = xd;
				cconf->pll_config[0].div_sel = clk_div_sel;
				cconf->pll_config[0].pll_div_fout = clk_div_out;
				cconf->pll_tcon_div_sel = tcon_div_sel;
				cconf->phy_clk = bit_rate;
				pll_fout = clk_div_in;
				if (lcd_debug_print_flag & LCD_DBG_PR_CLK) {
					LCDPR("clk_div_sel=%s(%d), pll_fout=%lld\n",
						lcd_clk_div_table[clk_div_sel].name,
						clk_div_sel, pll_fout);
				}
				done = check_3od_div_5_7(cconf, LCD_PLL_SEL_PHY, pll_fout);
				if (done)
					goto generate_phy_clk_od_div_5_7_done;
			}
		}
	}

generate_phy_clk_od_div_5_7_done:
	return done;
}

#ifdef CONFIG_AMLOGIC_LCD_DPI
static unsigned char lcd_clk_generate_DPI(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned long long pll_fout, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd;
	int done = 0;

	if (!cconf)
		return 0;

	clk_div_sel = CLK_DIV_SEL_1;
	for (xd = 1; xd <= cconf->data->xd_max; xd++) {
		clk_div_out = cconf->fout * xd;
		if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
			continue;
		LCD_DBG_CLK(pdrv, "fout=%u xd=%d div_out=%u", cconf->fout, xd, clk_div_out);
		clk_div_in = clk_vid_pll_div_calc(clk_div_out, clk_div_sel, CLK_DIV_O2I);
		if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
			continue;
		cconf->xd = xd;
		cconf->pll_config[0].div_sel = clk_div_sel;
		cconf->pll_config[0].pll_div_fout = clk_div_out;
		pll_fout = clk_div_in;
		LCD_DBG_CLK(pdrv, "div_sel=%s(%u) pll_fout=%lld",
			lcd_clk_div_table[clk_div_sel].name, clk_div_sel, pll_fout);

		done = pll_od_setting_generate(cconf, LCD_PLL_SEL_PHY, pll_fout);
		if (done)
			return 1;
	}
	return 0;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_LVDS
static unsigned char lcd_clk_generate_lvds(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned long long pll_fout, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd, tcon_div_sel = 0;
	unsigned int od1, od2, od3;
	unsigned long long phy_div = 1;
	int done = 0;

	if (!cconf)
		return 0;

	if (pdrv->data->chip_type == LCD_CHIP_T3X) {
		if (pconf->control.lvds_cfg.dual_port)
			clk_div_sel = CLK_DIV_SEL_3p5;
		else
			clk_div_sel = CLK_DIV_SEL_7;
		phy_div = 1;
	} else {
		if (pconf->control.lvds_cfg.dual_port)
			phy_div = 2;
		else
			phy_div = 1;
		clk_div_sel = CLK_DIV_SEL_7;
	}
	xd = 1;
	clk_div_out = cconf->fout * xd;
	if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
		return 0;
	LCD_DBG_CLK(pdrv, "fout=%u xd=%d clk_div_out=%u", cconf->fout, xd, clk_div_out);

	clk_div_in = clk_vid_pll_div_calc(clk_div_out, clk_div_sel, CLK_DIV_O2I);
	if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
		return 0;

	cconf->xd = xd;
	cconf->pll_config[0].div_sel = clk_div_sel;
	cconf->pll_config[0].pll_div_fout = clk_div_out;
	pll_fout = clk_div_in;
	LCD_DBG_CLK(pdrv, "clk_div_sel=%s(%d) pll_fout=%lld",
		lcd_clk_div_table[clk_div_sel].name, clk_div_sel, pll_fout);

	done = pll_od_setting_generate(cconf, LCD_PLL_SEL_PHY, pll_fout);
	if (done == 0)
		return 0;

	if (cconf->data->pll_data[0]->have_tcon_div) {
		done = 0;
		if (cconf->data->pll_data[0]->od_cnt == 3) {
			od1 = od_table[cconf->pll_config[0].pll_od1_sel];
			od2 = od_table[cconf->pll_config[0].pll_od2_sel];
			od3 = od_table[cconf->pll_config[0].pll_od3_sel];
		} else {
			od1 = od_table[cconf->pll_config[0].pll_od1_sel];
			od2 = 1;
			od3 = 1;
		}
		for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
			if (tcon_div_table[tcon_div_sel] == phy_div * od1 * od2 * od3) {
				cconf->pll_tcon_div_sel = tcon_div_sel;
				cconf->phy_clk = lcd_do_div(cconf->pll_config[0].pll_fvco,
							tcon_div_table[tcon_div_sel]);
				done = 1;
				break;
			}
		}
	} else {
		cconf->phy_clk = cconf->pll_config[0].pll_fout;
	}
	pconf->timing.bit_rate = cconf->phy_clk;
	return done;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_VBYONE
static unsigned char lcd_clk_generate_vbyone(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned long long clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd, tcon_div_sel = 0;
	unsigned int od1, od2, od3;
	int done = 0;
	unsigned int tmp_clk;

	if (!cconf)
		return 0;

	clk_div_in = pconf->timing.bit_rate;
	if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
		return 0;

	clk_div_sel = CLK_DIV_SEL_1;
	for (; clk_div_sel <= cconf->data->pll_data[0]->div_sel_max; clk_div_sel++) {
		clk_div_out = clk_vid_pll_div_calc(clk_div_in, clk_div_sel, CLK_DIV_I2O);
		if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
			continue;
		LCD_DBG_CLK(pdrv, "clk_div_out=%u, clk_div_sel=%s(%d)",
			clk_div_out, lcd_clk_div_table[clk_div_sel].name, clk_div_sel);

		done = 0;
		for (xd = 1; xd <= cconf->data->xd_max; xd++) {
			tmp_clk = cconf->fout * xd;
			if (tmp_clk > clk_div_out)
				break;
			if (tmp_clk == clk_div_out) {
				cconf->xd = xd;
				cconf->pll_config[0].div_sel = clk_div_sel;
				cconf->pll_config[0].pll_div_fout = clk_div_out;
				done = 1;
				LCD_DBG_CLK(pdrv, "fout=%u, xd=%d", cconf->fout, xd);
				break;
			}
		}

		if (done)
			break;
	}

	done = pll_od_setting_generate(cconf, LCD_PLL_SEL_PHY, clk_div_in);
	if (done == 0)
		return 0;

	if (cconf->data->pll_data[0]->have_tcon_div) {
		done = 0;
		if (cconf->data->pll_data[0]->od_cnt == 3) {
			od1 = od_table[cconf->pll_config[0].pll_od1_sel];
			od2 = od_table[cconf->pll_config[0].pll_od2_sel];
			od3 = od_table[cconf->pll_config[0].pll_od3_sel];
		} else {
			od1 = od_table[cconf->pll_config[0].pll_od1_sel];
			od2 = 1;
			od3 = 1;
		}
		for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
			if (tcon_div_table[tcon_div_sel] == od1 * od2 * od3) {
				cconf->pll_tcon_div_sel = tcon_div_sel;
				cconf->phy_clk =
					lcd_do_div(cconf->pll_config[0].pll_fvco,
						tcon_div_table[tcon_div_sel]);
				done = 1;
				break;
			}
		}
	} else {
		cconf->phy_clk = cconf->pll_config[0].pll_fout;
		done = 1;
	}
	return done;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
#define DSI_CLK_TB_SIZE 32
/********************** DSI 1PLL model **********************/
/* PLL_VCO / OD[1/3] / PLL_CLK_DIV(optional) == VID_PLL_CLK */
/* VID_PLL_CLK --> / enc_xd  == ENCL_clk                    */
/*      '--------> / PHY_div == PHY HS clk                  */
/************************************************************/
static unsigned char lcd_clk_generate_DSI_1PLL(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);

	if (!cconf)
		return 0;

	struct dsi_clk_tb_s {
		unsigned long long pllout;
		unsigned long long phy_clk;
		unsigned short enc_xd;
		unsigned char phy_n;
		unsigned char frac_sel;
	};

	struct dsi_config_s *dconf = &pdrv->curr_dev->dev_cfg.control.mipi_cfg;
	unsigned long long pll_out, phy_clk;
	unsigned short enc_xd, phy_N;
	unsigned char done, tb_idx = 0, x, new_high_bitrate, frac_sel;
	struct dsi_clk_tb_s *clk_div_tb;

	unsigned long long bitrate_min, bitrate_max;
	u8 port_cnt = dconf->multi_port_cfg & BIT(0) ? 2 : 1;

	bitrate_min = lcd_dsi_get_min_bitrate(pdrv);
	bitrate_max = dconf->bit_rate_target;
	bitrate_max = bitrate_max * 1000000;

	clk_div_tb = kcalloc(DSI_CLK_TB_SIZE, sizeof(struct dsi_clk_tb_s), GFP_KERNEL);
	if (!clk_div_tb)
		return 0;

	cconf->pll_tcon_div_sel = 2;
	cconf->pll_config[0].div_sel = CLK_DIV_SEL_1;

	for (enc_xd = 1; enc_xd < cconf->data->xd_max; enc_xd++) {
		pll_out = enc_xd;
		pll_out = pll_out * cconf->fout;
		if (pll_out > cconf->data->pll_data[0]->div_out_fmax)
			continue;
		for (frac_sel = CLK_DIV_SEL_1;
			frac_sel <= cconf->data->pll_data[0]->div_sel_max; frac_sel++) {
			pll_out = clk_vid_pll_div_calc(pll_out, frac_sel, CLK_DIV_O2I);

			if (pll_out > cconf->data->pll_data[0]->pll_out_fmax ||
			    pll_out < cconf->data->pll_data[0]->pll_out_fmin)
				continue;

			for (phy_N = 1; phy_N < cconf->data->phy_div_max; phy_N++) {
				phy_clk = clk_vid_pll_div_calc(pll_out, frac_sel, CLK_DIV_I2O);
				phy_clk = div_around(phy_clk, phy_N);
				if (phy_clk > bitrate_max || phy_clk < bitrate_min)
					continue;

				new_high_bitrate = 1;
				for (x = 0; x < tb_idx; x++) {
					if (phy_clk < clk_div_tb[x].phy_clk)
						new_high_bitrate = 0;
				}
				if (!new_high_bitrate)
					continue;

				if (tb_idx == DSI_CLK_TB_SIZE) {
					LCD_ERR(pdrv, "dsi clk table full!");
					goto dsi_clk_tabel_buffer_full;
				}

				clk_div_tb[tb_idx].pllout = pll_out;
				clk_div_tb[tb_idx].enc_xd = enc_xd;
				clk_div_tb[tb_idx].phy_n = phy_N;
				clk_div_tb[tb_idx].phy_clk = phy_clk;
				clk_div_tb[tb_idx].frac_sel = frac_sel;
				tb_idx++;
			}
		}
	}

	if (!tb_idx) {
		LCD_ERR(pdrv, "dsi clk div: pll_out:(%lluHz~%lluHz) bit_rate:(%lluHz~%uMHz)",
			cconf->data->pll_data[0]->pll_out_fmin,
			cconf->data->pll_data[0]->pll_out_fmax, bitrate_min,
			dconf->bit_rate_target);
		kfree(clk_div_tb);
		return 0;
	}

dsi_clk_tabel_buffer_full:
	x = tb_idx - 1;
	while (1) {
		done = pll_od_setting_generate(cconf, LCD_PLL_SEL_PHY, clk_div_tb[x].pllout);
		if (done || x == 0)
			break;
		x--;
	}
	if (!done) {
		LCD_ERR(pdrv, "DSI_PLL: no setting");
		kfree(clk_div_tb);
		return 0;
	}

	LCD_PR(pdrv, "vco=%lluHz pll_out:%lluHz div[%s] xd[%hu]->fout=%uhz div[%hu]->phy=%lluhz",
		cconf->pll_config[0].pll_fvco, clk_div_tb[x].pllout,
		lcd_clk_div_table[clk_div_tb[x].frac_sel].name, clk_div_tb[x].enc_xd, cconf->fout,
		clk_div_tb[x].phy_n, clk_div_tb[x].phy_clk);

	cconf->phy_clk = clk_div_tb[x].phy_clk;
	pdrv->curr_dev->dev_cfg.timing.bit_rate = clk_div_tb[x].phy_clk;

	cconf->phy_div = clk_div_tb[x].phy_n;
	cconf->xd = clk_div_tb[x].enc_xd; //PLL2enc
	cconf->pll_config[0].div_sel = clk_div_tb[x].frac_sel;
	// should lane_byte_clk = (be phy_clk == phy_bitrate / 2) / 4
	dconf->lane_byte_clk = div_around(clk_div_tb[x].phy_clk, 8);

	// should lane_byte_clk = (be phy_clk == phy_bitrate / 2) / pclk
	dconf->factor_numerator = cconf->xd * port_cnt;
	dconf->factor_denominator = cconf->phy_div * 8;

	kfree(clk_div_tb);
	return 1;
}

/*
 *  DSI_VCO(2.8G) --- 3od --- dsi_phy_clk(2div)
 *        |                        '--- host clk
 *        '--- /enc_xd --- encl_clk
 * -----------------------------------------------------
 * SW arch:
 * PLL: 1.4~2.8G VCO, 0 od
 * 3 od + dsi_phy_clk(2div) as special for phy div
 */
static unsigned char lcd_dsi_generate_DSI_PLL_s6_model(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);

	if (!cconf)
		return 0;

	struct dsi_clk_tb_s {
		unsigned long long pll_out;
		unsigned long long phy_clk;
		unsigned short enc_xd;
		unsigned short phy_div;
		unsigned char frac_div_sel;
	};

	unsigned char vco_to_phy_div_table[5][2] = {
		{1, 0x0}, {2, 0x1}, {4, 0x2}, {8, 0x3}, {16, 0x4}
	};

	struct dsi_config_s *dconf = &pdrv->curr_dev->dev_cfg.control.mipi_cfg;
	unsigned long long pll_out, phy_clk;
	unsigned short enc_xd, phy_N;
	unsigned char tb_idx = 0, x, new_high_bitrate, phy_div, frac_sel;
	struct dsi_clk_tb_s *clk_div_tb;

	unsigned long long bitrate_min, bitrate_max;

	bitrate_min = lcd_dsi_get_min_bitrate(pdrv);
	bitrate_max = dconf->bit_rate_target;
	bitrate_max = bitrate_max * 1000000;

	clk_div_tb = kcalloc(32, sizeof(struct dsi_clk_tb_s), GFP_KERNEL);
	if (!clk_div_tb)
		return 0;

	memset(clk_div_tb, 0, 32 * sizeof(struct dsi_clk_tb_s));

	cconf->pll_tcon_div_sel = 2;

	for (enc_xd = 1; enc_xd < cconf->data->xd_max; enc_xd++) {
		for (frac_sel = CLK_DIV_SEL_1; frac_sel <= cconf->data->pll_data[0]->div_sel_max;
			frac_sel++) {
			pll_out = enc_xd;
			pll_out = pll_out * cconf->fout;
			pll_out = clk_vid_pll_div_calc(pll_out, frac_sel, CLK_DIV_O2I);

			if (pll_out > cconf->data->pll_data[0]->pll_vco_fmax ||
			    pll_out < cconf->data->pll_data[0]->pll_vco_fmin)
				continue;
			for (phy_div = 0; phy_div < ARRAY_SIZE(vco_to_phy_div_table); phy_div++) {
				phy_N = vco_to_phy_div_table[phy_div][0];
				phy_clk = div_around(pll_out, phy_N);
				if (phy_clk > bitrate_max || phy_clk < bitrate_min)
					continue;

				new_high_bitrate = 1;
				for (x = 0; x < tb_idx; x++) {
					if (phy_clk <= clk_div_tb[x].phy_clk)
						new_high_bitrate = 0;
				}
				if (!new_high_bitrate)
					continue;

				if (tb_idx == 32) {
					LCD_ERR(pdrv, "dsi clk table full!");
					goto dsi_clk_tabel_buffer_full;
				}

				clk_div_tb[tb_idx].pll_out = pll_out;
				clk_div_tb[tb_idx].enc_xd = enc_xd;
				clk_div_tb[tb_idx].phy_div = phy_div;
				clk_div_tb[tb_idx].phy_clk = phy_clk;
				clk_div_tb[tb_idx].frac_div_sel = frac_sel;
				tb_idx++;
			}
		}
	}

	if (!tb_idx) {
		LCD_ERR(pdrv, "dsi clk div: pll_out:(%lluHz~%lluHz) bit_rate:(%lluHz~%uMHz)",
			cconf->data->pll_data[0]->pll_out_fmin,
			cconf->data->pll_data[0]->pll_out_fmax, bitrate_min,
			dconf->bit_rate_target);
		kfree(clk_div_tb);
		return 0;
	}

dsi_clk_tabel_buffer_full:
	x = tb_idx - 1;

	LCD_PR(pdrv, "DSI_PLL: pll_out:%lluHz: xd[%hu]*frac[%s]->fout=%uhz div[%hu]->phy=%lluhz",
		clk_div_tb[x].pll_out, clk_div_tb[x].enc_xd,
		lcd_clk_div_table[clk_div_tb[x].frac_div_sel].name,
		cconf->fout, clk_div_tb[x].phy_div, clk_div_tb[x].phy_clk);

	cconf->pll_config[0].pll_fout = clk_div_tb[x].pll_out;

	cconf->xd = clk_div_tb[x].enc_xd; //PLL2enc
	cconf->pll_config->div_sel = clk_div_tb[x].frac_div_sel;

	cconf->pll_config[0].pll_od1_sel = vco_to_phy_div_table[clk_div_tb[x].phy_div][1];
	// should lane_byte_clk = (be phy_clk == phy_bitrate / 2) / 4
	cconf->phy_div = 1;
	cconf->phy_clk = clk_div_tb[x].phy_clk;
	pdrv->curr_dev->dev_cfg.timing.bit_rate = clk_div_tb[x].phy_clk;
	dconf->lane_byte_clk = div_around(clk_div_tb[x].phy_clk, 8);

	// should lane_byte_clk = (be phy_clk == phy_bitrate / 2) / pclk
	dconf->factor_numerator = cconf->xd * lcd_clk_div_table[cconf->pll_config->div_sel].den;
	dconf->factor_denominator = vco_to_phy_div_table[clk_div_tb[x].phy_div][0] * 8 *
				    lcd_clk_div_table[cconf->pll_config->div_sel].num;

	kfree(clk_div_tb);
	return 1;
}

#endif

#ifdef CONFIG_AMLOGIC_LCD_TABLET
/* Func: lcd_DP_1PLL_clk_para_cal, strategy: keep phy clk, find the closest pclk */
/************************* eDP 1PLL model T7 ***********************/
/* PLL_VCO / tcon_div / edp_div0&1 / PLL_CLK_DIV == VID_PLL_CLK */
/*              '--> eDP_PHY_clk   .---------------------'      */
/*                                 '--> / enc_xd == ENCL_clk    */
/****************************************************************/
static unsigned char lcd_clk_generate_DP_1PLL(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	u8 clk_level_sel = 0;
	unsigned int edp_div0, edp_div1, tmp_fout, tmp_div;
	unsigned int min_err_div0 = 0, min_err_div1 = 0, min_err_div = 0;
	unsigned long long error, min_err = 1000000000;
	unsigned long long eDP_PLL_setting_t7[2][3] = {
		{135, 3240000000ULL, 1620000000ULL},
		{225, 5400000000ULL, 2700000000ULL},
	};

	if (!cconf)
		return 0;

	if (pdrv->curr_dev->dev_cfg.control.edp_cfg.link_rate == 0x0a) //2.7G
		clk_level_sel = 1;
	else
		clk_level_sel = 0;

	cconf->pll_config->pll_m = eDP_PLL_setting_t7[clk_level_sel][0];
	cconf->pll_config->pll_fvco = eDP_PLL_setting_t7[clk_level_sel][1];
	cconf->pll_config->pll_fout = eDP_PLL_setting_t7[clk_level_sel][2];
	cconf->phy_clk = eDP_PLL_setting_t7[clk_level_sel][2];
	cconf->pll_config->pll_n = 1;
	cconf->pll_config->pll_frac = 0;
	cconf->pll_config->pll_od1_sel = 1;
	cconf->pll_config->pll_od2_sel = 0;
	cconf->pll_config->pll_od3_sel = 0;
	cconf->pll_tcon_div_sel = 1;
	cconf->pll_config->pll_frac_half_shift = 0;
	cconf->pll_config->div_sel = CLK_DIV_SEL_1;
	cconf->xd = 1;

	for (edp_div0 = 0; edp_div0 < 15; edp_div0++) {
		for (edp_div1 = 0; edp_div1 < 10; edp_div1++) {
			tmp_div = edp_div0_table[edp_div0] * edp_div1_table[edp_div1];
			tmp_fout = lcd_do_div(cconf->pll_config->pll_fout, tmp_div);
			error = lcd_u32_diff(tmp_fout, cconf->fout);
			if (error >= min_err)
				continue;

			min_err = error;
			min_err_div0 = edp_div0;
			min_err_div1 = edp_div1;
			min_err_div = tmp_div;
		}
	}
	cconf->err_fmin = min_err;
	cconf->edp_div0 = min_err_div0;
	cconf->edp_div1 = min_err_div1;

	cconf->fout = lcd_do_div(cconf->pll_config->pll_fout, min_err_div);
	pdrv->curr_dev->dev_cfg.timing.enc_clk = cconf->fout;

	LCD_PR(pdrv, "PLL_out=%llu div=%u [%u, %u] fout:%u enc_clk=%u error=%llu",
		cconf->pll_config->pll_fout, min_err_div,
		edp_div0_table[min_err_div0], edp_div1_table[min_err_div1],
		cconf->fout, cconf->fout, min_err);
	return 1;
}
#endif

#ifdef CONFIG_AMLOGIC_LCD_TCON
static unsigned char lcd_clk_generate_mlvds(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	struct lcd_clk_config_s *cconf = get_lcd_clk_config(pdrv);
	unsigned long long pll_fout, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd, tcon_div_sel = 0;
	int done = 0;
	unsigned long long pll_fvco;

	if (!cconf)
		return 0;
	/* must go through div4 for clk phase */
	if (cconf->data->pll_data[0]->have_tcon_div == 0) {
		LCD_ERR(pdrv, "clK gen: no tcon_div for minilvds");
		return 0;
	}
	for (tcon_div_sel = 3; tcon_div_sel < 5; tcon_div_sel++) {
		pll_fvco = pconf->timing.bit_rate * tcon_div_table[tcon_div_sel];
		done = check_vco(cconf, LCD_PLL_SEL_PHY, pll_fvco);
		if (done == 0)
			continue;
		for (xd = 1; xd <= cconf->data->xd_max; xd++) {
			clk_div_out = cconf->fout * xd;
			if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
				continue;
			LCD_DBG_CLK(pdrv, "fout=%u, xd=%d, clk_div_out=%u",
					cconf->fout, xd, clk_div_out);
			clk_div_sel = CLK_DIV_SEL_1;
			for (; clk_div_sel <= cconf->data->pll_data[0]->div_sel_max;
				clk_div_sel++) {
				clk_div_in = clk_vid_pll_div_calc(clk_div_out,
							clk_div_sel, CLK_DIV_O2I);
				if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
					continue;
				cconf->xd = xd;
				cconf->pll_config[0].div_sel = clk_div_sel;
				cconf->pll_tcon_div_sel = tcon_div_sel;
				cconf->pll_config[0].pll_div_fout = clk_div_out;
				cconf->phy_clk = lcd_do_div(pll_fvco,
							tcon_div_table[tcon_div_sel]);
				pll_fout = clk_div_in;
				LCD_DBG_CLK(NULL,
					"clk_div_sel=%s(%d) pll_fout=%lld tcon_div_sel=%d",
					lcd_clk_div_table[clk_div_sel].name, clk_div_sel,
					pll_fout, tcon_div_sel);
				done = check_3od(cconf, LCD_PLL_SEL_PHY, pll_fout);
				if (done)
					return 1;
			}
		}
	}
	return 0;
}
#endif

static int lcd_pll_frac_generate_phy(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned long long pll_fout, pll_fvco, clk_div_in;
	unsigned int enc_clk, clk_div_out, clk_div_sel;
	unsigned int od1 = 1, od2 = 1, od3 = 1;
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	enc_clk = pconf->timing.enc_clk;
	clk_div_sel = cconf->pll_config[0].div_sel;
	if (cconf->data->pll_data[0]->od_cnt == 3) {
		od1 = od_table[cconf->pll_config[0].pll_od1_sel];
		if (cconf->data->pll_data[0]->pll_od_div_5_7)
			od2 = od_table_div_5_7[cconf->pll_config[0].pll_od2_sel];
		else
			od2 = od_table[cconf->pll_config[0].pll_od2_sel];
		od3 = od_table[cconf->pll_config[0].pll_od3_sel];
		LCD_DBG_CLK(pdrv, "m=%d, od1=%d, od2=%d, od3=%d, clk_div_sel=%s(%d), xd=%d",
			cconf->pll_config[0].pll_m, cconf->pll_config[0].pll_od1_sel,
			cconf->pll_config[0].pll_od2_sel, cconf->pll_config[0].pll_od3_sel,
			lcd_clk_div_table[clk_div_sel].name,
			clk_div_sel, cconf->xd);
	} else {
		od1 = od_table[cconf->pll_config[0].pll_od1_sel];
		LCD_DBG_CLK(pdrv, "m=%d, od=%d, clk_div_sel=%s(%d), xd=%d",
			cconf->pll_config[0].pll_m, cconf->pll_config[0].pll_od1_sel,
			lcd_clk_div_table[clk_div_sel].name, clk_div_sel, cconf->xd);
	}
	if (enc_clk > cconf->data->xd_out_fmax) {
		LCD_ERR(pdrv, "pll frac phy: wrong enc_clk %uHz", enc_clk);
		return -1;
	}
	LCD_DBG_CLK(pdrv, "pll frac phy: enc_clk=%d", enc_clk);

	pll_fvco = pconf->timing.bit_rate * tcon_div_table[cconf->pll_tcon_div_sel];
	if (pll_fvco < cconf->data->pll_data[0]->pll_vco_fmin ||
	    pll_fvco > cconf->data->pll_data[0]->pll_vco_fmax) {
		LCD_ERR(pdrv, "pll frac phy: wrong pll_fvco %lldHz", pll_fvco);
		return -1;
	}
	LCD_DBG_CLK(pdrv, "pll frac phy: pll_fvco=%lld", pll_fvco);

	if (!(cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)) {
		clk_div_in = lcd_do_div(pll_fvco, od1 * od2 * od3);
		if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax) {
			LCD_ERR(pdrv, "pll frac phy: wrong clk_div_in %lldHz", clk_div_in);
			return -1;
		}

		pll_fout = clk_div_in;
		if (pll_fout > cconf->data->pll_data[0]->pll_out_fmax ||
		pll_fout < cconf->data->pll_data[0]->pll_out_fmin) {
			LCD_ERR(pdrv, "pll frac phy: wrong pll_fout %lldHz", pll_fout);
			return -1;
		}
		LCD_DBG_CLK(pdrv, "pll frac phy: pll_fout=%lld", pll_fout);

		clk_div_out = clk_vid_pll_div_calc(clk_div_in, clk_div_sel, CLK_DIV_I2O);
		if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax) {
			LCD_ERR(pdrv, "pll frac phy: wrong clk_div_out %uHz", clk_div_out);
			return -1;
		}
	}

	ret = lcd_pll_get_frac(cconf, LCD_PLL_SEL_PHY, pll_fvco);
	if (ret == 0) {
		cconf->fout = enc_clk;
		cconf->pll_config[0].pll_div_fout = clk_div_out;
		cconf->pll_config[0].pll_fout = pll_fout;
		cconf->pll_config[0].pll_fvco = pll_fvco;
		cconf->phy_clk = lcd_do_div(pll_fvco, tcon_div_table[cconf->pll_tcon_div_sel]);

		pconf->timing.clk_ctrl &= ~(0x1ffffff);
		pconf->timing.clk_ctrl |=
			(cconf->pll_config[0].pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_config[0].pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
	}

	return ret;
}

static int lcd_pll_frac_generate_pix(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned long long pll_fout, pll_fvco, clk_div_in;
	unsigned int enc_clk, clk_div_out, clk_div_sel;
	unsigned int od1 = 1, od2 = 1, od3 = 1;
	int ret;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return -1;

	if (!(cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)) {
		LCD_DBG_CLK(pdrv, "Pix-PLL frac: not dual mode");
		return 0;
	}

	enc_clk = pconf->timing.enc_clk;
	clk_div_sel = cconf->pll_config[1].div_sel;
	if (cconf->data->pll_data[1]->od_cnt == 3) {
		od1 = od_table[cconf->pll_config[1].pll_od1_sel];
		od2 = od_table[cconf->pll_config[1].pll_od2_sel];
		od3 = od_table[cconf->pll_config[1].pll_od3_sel];
		LCD_DBG_CLK(pdrv, "cconf m=%d, od1=%d, od2=%d, od3=%d, clk_div_sel=%s(%d), xd=%d",
			cconf->pll_config[1].pll_m, cconf->pll_config[1].pll_od1_sel,
			cconf->pll_config[1].pll_od2_sel, cconf->pll_config[1].pll_od3_sel,
			lcd_clk_div_table[clk_div_sel].name, clk_div_sel, cconf->xd);
	} else if (cconf->data->pll_data[1]->od_cnt == 2) {
		od1 = od_table[cconf->pll_config[1].pll_od1_sel];
		od2 = od_table[cconf->pll_config[1].pll_od2_sel];
		LCD_DBG_CLK(pdrv, "cconf m=%d, od1=%d, od2=%d, od3=%d, clk_div_sel=%s(%d), xd=%d",
			cconf->pll_config[1].pll_m, cconf->pll_config[1].pll_od1_sel,
			cconf->pll_config[1].pll_od2_sel, cconf->pll_config[1].pll_od3_sel,
			lcd_clk_div_table[clk_div_sel].name, clk_div_sel, cconf->xd);

	} else {
		od1 = od_table[cconf->pll_config[1].pll_od1_sel];
		LCD_DBG_CLK(pdrv, "cconf m=%d, od=%d, clk_div_sel=%s(%d), xd=%d",
			cconf->pll_config[1].pll_m, cconf->pll_config[1].pll_od1_sel,
			lcd_clk_div_table[clk_div_sel].name, clk_div_sel, cconf->xd);
	}
	LCD_DBG_CLK(pdrv, "m=%d, od=%d, clk_div_sel=%s(%d), xd=%d",
		cconf->pll_config[1].pll_m, cconf->pll_config[1].pll_od1_sel,
		lcd_clk_div_table[clk_div_sel].name, clk_div_sel, cconf->xd);
	if (enc_clk > cconf->data->xd_out_fmax) {
		LCD_ERR(pdrv, "pll frac pix: wrong enc_clk %uHz", enc_clk);
		return -1;
	}
	LCD_DBG_CLK(pdrv, "pll frac pix: enc_clk=%d", enc_clk);

	clk_div_out = enc_clk * cconf->xd;
	if (clk_div_out > cconf->data->pll_data[1]->div_out_fmax) {
		LCD_ERR(pdrv, "pll frac pix: wrong clk_div_out %uHz", clk_div_out);
		return -1;
	}

	clk_div_in = clk_vid_pll_div_calc(clk_div_out, clk_div_sel, CLK_DIV_O2I);
	if (clk_div_in > cconf->data->pll_data[1]->div_in_fmax) {
		LCD_ERR(pdrv, "pll frac pix: wrong clk_div_in %lldHz", clk_div_in);
		return -1;
	}

	pll_fout = clk_div_in;
	if (pll_fout > cconf->data->pll_data[1]->pll_out_fmax ||
	    pll_fout < cconf->data->pll_data[1]->pll_out_fmin) {
		LCD_ERR(pdrv, "pll frac pix: wrong pll_fout %lldHz", pll_fout);
		return -1;
	}
	LCD_DBG_CLK(pdrv, "pll frac pix: pll_fout=%lld", pll_fout);

	if (cconf->data->pll_data[1]->od_cnt == 3)
		pll_fvco = pll_fout * od1 * od2 * od3;
	else
		pll_fvco = pll_fout * od1;

	if (pll_fvco < cconf->data->pll_data[1]->pll_vco_fmin ||
	    pll_fvco > cconf->data->pll_data[1]->pll_vco_fmax) {
		LCD_ERR(pdrv, "pll frac pix: wrong pll_fvco %lldHz", pll_fvco);
		return -1;
	}
	LCD_DBG_CLK(pdrv, "pll frac pix: pll_fvco=%lld", pll_fvco);

	ret = lcd_pll_get_frac(cconf, LCD_PLL_SEL_PIX, pll_fvco);
	if (ret == 0) {
		cconf->fout = enc_clk;
		cconf->pll_config[1].pll_div_fout = clk_div_out;
		cconf->pll_config[1].pll_fout = pll_fout;
		cconf->pll_config[1].pll_fvco = pll_fvco;
		pconf->timing.clk_ctrl &= ~(0x1ffffff);
		pconf->timing.clk_ctrl |=
			(cconf->pll_config[1].pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_config[1].pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
	}
	return ret;
}

void lcd_pll_frac_generate_dft(struct aml_lcd_drv_s *pdrv)
{
	int ret;

	ret = lcd_pll_frac_generate_phy(pdrv);
	if (ret)
		LCD_ERR(pdrv, "PLL-frac: phy gen failed");

	ret = lcd_pll_frac_generate_pix(pdrv);
	if (ret)
		LCD_ERR(pdrv, "PLL-frac: pix gen failed");
}

static int lcd_clk_generate_pix_clk(struct lcd_clk_config_s *cconf)
{
	unsigned long long pll_fout, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd;
	int done = 0;

	for (xd = 1; xd <= cconf->data->xd_max; xd++) {
		clk_div_out = cconf->fout * xd;
		if (clk_div_out > cconf->data->pll_data[1]->div_out_fmax)
			continue;
		LCD_DBG_CLK(NULL, "fout=%d, xd=%d, clk_div_out=%d", cconf->fout, xd, clk_div_out);

		for (clk_div_sel = CLK_DIV_SEL_1;
			clk_div_sel <= cconf->data->pll_data[1]->div_sel_max;
			clk_div_sel++) {
			clk_div_in = clk_vid_pll_div_calc(clk_div_out,
					clk_div_sel, CLK_DIV_O2I);
			if (clk_div_in > cconf->data->pll_data[1]->div_in_fmax)
				continue;
			LCD_DBG_CLK(NULL, "clk_div_sel=%s(%d), clk_div_in=%lld",
					lcd_clk_div_table[clk_div_sel].name,
					clk_div_sel, clk_div_in);
			cconf->xd = xd;
			cconf->pll_config[1].div_sel = clk_div_sel;
			cconf->pll_config[1].pll_div_fout = clk_div_out;
			pll_fout = clk_div_in;
			LCD_DBG_CLK(NULL, "pll_fout=%lld", pll_fout);

			done = pll_od_setting_generate(cconf, LCD_PLL_SEL_PIX, pll_fout);
			if (done)
				goto clk_generate_pix_clk_done;
		}
	}

clk_generate_pix_clk_done:
	return done;
}

void lcd_clk_generate_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	int done = 0;
	int done_pix = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->pll_mode |= pconf->timing.pll_flag;
	cconf->fout = pconf->timing.enc_clk;
	if (cconf->fout > cconf->data->xd_out_fmax) {
		LCD_ERR(pdrv, "Clk-Gen: enc_clk %uHz out of %uHz",
			cconf->fout, cconf->data->xd_out_fmax);
		goto generate_clk_dft_done;
	}

	switch (pconf->basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_DPI
	case LCD_RGB:
		done = lcd_clk_generate_DPI(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_LVDS
	case LCD_LVDS:
		done = lcd_clk_generate_lvds(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		done = lcd_clk_generate_vbyone(pdrv);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
		done = lcd_clk_generate_mlvds(pdrv);
		break;
	case LCD_P2P:
		if (cconf->data->pll_data[0]->have_tcon_div)
			done = lcd_clk_generate_p2p_with_tcon_div(cconf, pconf->timing.bit_rate);
		else
			done = lcd_clk_generate_p2p_without_tcon_div(cconf, pconf->timing.bit_rate);
		if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)
			done_pix = lcd_clk_generate_pix_clk(cconf);
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		switch (pdrv->data->chip_type) {
		case LCD_CHIP_S6:
			done = lcd_dsi_generate_DSI_PLL_s6_model(pdrv);
			if (done)
				done = pll_od_setting_generate(cconf,
					LCD_PLL_SEL_PHY, cconf->pll_config[0].pll_fout);
			break;
		default:
			done = lcd_clk_generate_DSI_1PLL(pdrv);
			// common DSI PLL model already had pll_od_setting_generate
			break;
		}
		break;
#endif
	default:
		break;
	}

generate_clk_dft_done:
	if (done) {
		pconf->timing.pll_ctrl =
			(cconf->pll_config[0].pll_od1_sel << PLL_CTRL_OD1) |
			(cconf->pll_config[0].pll_od2_sel << PLL_CTRL_OD2) |
			(cconf->pll_config[0].pll_od3_sel << PLL_CTRL_OD3) |
			(cconf->pll_config[0].pll_n << PLL_CTRL_N)         |
			(cconf->pll_config[0].pll_m << PLL_CTRL_M);
		pconf->timing.div_ctrl =
			(cconf->pll_config[0].div_sel << DIV_CTRL_DIV_SEL) |
			(cconf->xd << DIV_CTRL_XD);
		pconf->timing.clk_ctrl =
			(cconf->pll_config[0].pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_config[0].pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
		cconf->pll_config[0].done = 1;
	} else {
		pconf->timing.pll_ctrl = 0;
		pconf->timing.div_ctrl = 0;
		pconf->timing.clk_ctrl = 0;
		cconf->pll_config[0].done = 0;
		LCD_ERR(NULL, "clk gen: Out of clock range");
	}
	if (done_pix && cconf->pll_conf_num > 1) {
		pconf->timing.pll_ctrl2 =
			(cconf->pll_config[1].pll_od1_sel << PLL_CTRL_OD1) |
			(cconf->pll_config[1].pll_od2_sel << PLL_CTRL_OD2) |
			(cconf->pll_config[1].pll_od3_sel << PLL_CTRL_OD3) |
			(cconf->pll_config[1].pll_n << PLL_CTRL_N)         |
			(cconf->pll_config[1].pll_m << PLL_CTRL_M);
		pconf->timing.div_ctrl2 =
			(cconf->pll_config[1].div_sel << DIV_CTRL_DIV_SEL) |
			(cconf->xd << DIV_CTRL_XD);
		pconf->timing.clk_ctrl2 =
			(cconf->pll_config[1].pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_config[1].pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
		cconf->pll_config[1].done = 1;
	} else {
		pconf->timing.pll_ctrl = 0;
		pconf->timing.div_ctrl = 0;
		pconf->timing.clk_ctrl = 0;
		if (cconf->pll_conf_num > 1)
			cconf->pll_config[1].done = 0;
		if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)
			LCD_ERR(pdrv, "clk gen: Out of clock range");
	}
}

void lcd_clk_generate_od_div_5_7(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_config_s *pconf = &pdrv->curr_dev->dev_cfg;
	unsigned long long bit_rate = 0;
	int done = 0, done_pix = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->pll_mode |= pconf->timing.pll_flag;
	cconf->fout = pconf->timing.enc_clk;
	if (cconf->fout > cconf->data->xd_out_fmax) {
		LCDERR("enc_clk %uHz out of %uHz\n",
			cconf->fout, cconf->data->xd_out_fmax);
		goto generate_clk_dft_done;
	}
	bit_rate = pconf->timing.bit_rate;

	switch (pconf->basic.lcd_type) {
	case LCD_LVDS:
	case LCD_VBYONE:
			done = lcd_clk_generate_phy_clk_od_div_5_7(cconf, bit_rate);
		if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)
			done_pix = lcd_clk_generate_pix_clk(cconf);
		break;
	default:
		break;
	}
generate_clk_dft_done:
	if (done) {
		pconf->timing.pll_ctrl =
			(cconf->pll_config[0].pll_od1_sel << PLL_CTRL_OD1) |
			(cconf->pll_config[0].pll_od2_sel << PLL_CTRL_OD2) |
			(cconf->pll_config[0].pll_od3_sel << PLL_CTRL_OD3) |
			(cconf->pll_config[0].pll_n << PLL_CTRL_N)         |
			(cconf->pll_config[0].pll_m << PLL_CTRL_M);
		pconf->timing.div_ctrl =
			(cconf->pll_config[0].div_sel << DIV_CTRL_DIV_SEL) |
			(cconf->xd << DIV_CTRL_XD);
		pconf->timing.clk_ctrl =
			(cconf->pll_config[0].pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_config[0].pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
		cconf->pll_config[0].done = 1;
	} else {
		pconf->timing.pll_ctrl = 0;
		pconf->timing.div_ctrl = 0;
		pconf->timing.clk_ctrl = 0;
		cconf->pll_config[0].done = 0;
		LCDERR("[%d]: Out of phy clock range\n", pdrv->index);
	}
	if (done_pix && cconf->pll_conf_num > 1) {
		pconf->timing.pll_ctrl2 =
			(cconf->pll_config[1].pll_od1_sel << PLL_CTRL_OD1) |
			(cconf->pll_config[1].pll_od2_sel << PLL_CTRL_OD2) |
			(cconf->pll_config[1].pll_od3_sel << PLL_CTRL_OD3) |
			(cconf->pll_config[1].pll_n << PLL_CTRL_N)         |
			(cconf->pll_config[1].pll_m << PLL_CTRL_M);
		pconf->timing.div_ctrl2 =
			(cconf->pll_config[1].div_sel << DIV_CTRL_DIV_SEL) |
			(cconf->xd << DIV_CTRL_XD);
		pconf->timing.clk_ctrl2 =
			(cconf->pll_config[1].pll_frac << CLK_CTRL_FRAC) |
			(cconf->pll_config[1].pll_frac_half_shift << CLK_CTRL_FRAC_SHIFT);
		cconf->pll_config[1].done = 1;
	} else {
		pconf->timing.pll_ctrl = 0;
		pconf->timing.div_ctrl = 0;
		pconf->timing.clk_ctrl = 0;
		if (cconf->pll_conf_num > 1)
			cconf->pll_config[1].done = 0;
		if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)
			LCDERR("[%d]: Out of pix clock range\n", pdrv->index);
	}
}

void lcd_clk_generate_prbs_clk(struct aml_lcd_drv_s *pdrv,
		unsigned int enc_clk, unsigned long long bit_rate)
{
	struct lcd_clk_config_s *cconf;
	unsigned long long pll_fout, clk_div_in;
	unsigned int clk_div_out, clk_div_sel, xd, tcon_div_sel = 0;
	unsigned int od1, od2, od3, tmp_clk;
	int done = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	cconf->fout = enc_clk;
	pll_fout = bit_rate;
	clk_div_in = pll_fout;
	if (clk_div_in > cconf->data->pll_data[0]->div_in_fmax)
		goto lcd_clk_generate_prbs_clk_done;
	LCD_DBG_CLK(pdrv, "enc_clk=%d, pll_fout=%lld, clk_div_in=%lld",
		cconf->fout, pll_fout, clk_div_in);

	for (clk_div_sel = CLK_DIV_SEL_1;
		clk_div_sel <= cconf->data->pll_data[0]->div_sel_max; clk_div_sel++) {
		clk_div_out = clk_vid_pll_div_calc(clk_div_in, clk_div_sel, CLK_DIV_I2O);
		if (clk_div_out > cconf->data->pll_data[0]->div_out_fmax)
			continue;
		LCD_DBG_CLK(pdrv, "clk_div_out=%u clk_div_sel=%s(%d)",
			clk_div_out, lcd_clk_div_table[clk_div_sel].name, clk_div_sel);

		done = 0;
		for (xd = 1; xd <= cconf->data->xd_max; xd++) {
			tmp_clk = cconf->fout * xd;
			if (tmp_clk > clk_div_out)
				break;
			if (tmp_clk == clk_div_out) {
				cconf->xd = xd;
				cconf->pll_config[0].div_sel = clk_div_sel;
				cconf->pll_config[0].pll_div_fout = clk_div_out;
				done = 1;
				LCD_DBG_CLK(pdrv, "fout=%u, xd=%d", cconf->fout, xd);
				break;
			}
		}

		if (done)
			break;
	}
	if (done == 0)
		goto lcd_clk_generate_prbs_clk_done;

	done = pll_od_setting_generate(cconf, LCD_PLL_SEL_PHY, pll_fout);
	if (done == 0)
		goto lcd_clk_generate_prbs_clk_done;

	if (cconf->data->pll_data[0]->have_tcon_div) {
		done = 0;
		if (cconf->data->pll_data[0]->od_cnt == 3) {
			od1 = od_table[cconf->pll_config[0].pll_od1_sel];
			od2 = od_table[cconf->pll_config[0].pll_od2_sel];
			od3 = od_table[cconf->pll_config[0].pll_od3_sel];
		} else {
			od1 = od_table[cconf->pll_config[0].pll_od1_sel];
			od2 = 1;
			od3 = 1;
		}
		for (tcon_div_sel = 0; tcon_div_sel < 5; tcon_div_sel++) {
			if (tcon_div_table[tcon_div_sel] == od1 * od2 * od3) {
				cconf->pll_tcon_div_sel = tcon_div_sel;
					cconf->phy_clk = lcd_do_div(cconf->pll_config[0].pll_fvco,
							tcon_div_table[tcon_div_sel]);
				done = 1;
				break;
			}
		}
	} else {
		cconf->phy_clk = cconf->pll_config[0].pll_fout;
	}

lcd_clk_generate_prbs_clk_done:
	if (done) {
		cconf->pll_mode &= ~LCD_PLL_MODE_DUAL_PLL;
		cconf->pll_config[0].done = 1;
	} else {
		cconf->pll_config[0].done = 0;
		LCD_ERR(pdrv, "clk gen prbs: Out of clock range");
	}
}

void lcd_set_vid_pll_div_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	unsigned int shift_val, shift_sel;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	// lcd_clk_setb(HHI_VIID_CLK_CNTL, 0, VCLK2_EN, 1);
	// udelay(5);

	/* Disable the div output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 19, 1);
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);

	if (cconf->data->pll_data[0]->div_sel_max == CLK_DIV_SEL_1 ||
	    cconf->pll_config[0].div_sel > cconf->data->pll_data[0]->div_sel_max ||
	    cconf->pll_config[0].div_sel >= ARRAY_SIZE(lcd_clk_div_table)) {
		LCD_ERR(pdrv, "invalid clk divider");
		return;
	}

	shift_val = lcd_clk_div_table[cconf->pll_config[0].div_sel].shift_val;
	shift_sel = lcd_clk_div_table[cconf->pll_config[0].div_sel].shift_sel;

	if (shift_val == 0xffff) { /* if divide by 1 */
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 18, 1);
	} else {
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 18, 1);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 16, 2);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 0, 14);

		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, shift_sel, 16, 2);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 15, 1);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, shift_val, 0, 15);
		lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 0, 15, 1);
	}
	/* Enable the final output clock */
	lcd_ana_setb(HHI_VID_PLL_CLK_DIV, 1, 19, 1);
}

void lcd_clk_config_init_print_dft(struct aml_lcd_drv_s *pdrv)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clk_data_s *data;
	struct lcd_pll_data_s *pll_data;
	struct lcd_pll_config_s *pll_config = NULL;
	int i;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;

	data = cconf->data;
	LCD_PR(pdrv, "clk data init:\n"
		"xd_out_fmax:    %d\n"
		"ss_level_max:   %d\n"
		"ss_dep_base:    %d\n"
		"ss_dep_sel_max: %d\n"
		"ss_str_m_max:   %d\n"
		"ss_freq_max:    %d\n"
		"ss_mode_max:    %d\n",
		data->xd_out_fmax, data->ss_level_max,
		data->ss_dep_base, data->ss_dep_sel_max,
		data->ss_str_m_max,
		data->ss_freq_max, data->ss_mode_max);
	for (i = 0; i < cconf->pll_conf_num; i++) {
		if (!cconf->data->pll_data[i]) {
			LCD_ERR(pdrv, "cconf[%d] data is NULL", i);
			return;
		}
		pll_config = &cconf->pll_config[i];
		pll_data = cconf->data->pll_data[i];
		LCD_PR(pdrv, "pll[%d] data init:\n"
			"pll_m:       [%3d %3d]\n"
			"pll_n:       [%3d %3d]\n"
			"pll_od_fb:      %d\n"
			"pll_0_5_div_en: %d\n"
			"pll_frac_range: %d\n"
			"pll_od_sel_max: %d\n"
			"pll_ref:     [%d %d]\n"
			"pll_vco:     [%lld %lld]\n"
			"pll_out:     [%lld %lld]\n"
			"div_in_fmax:  %lld\n"
			"div_out_fmax: %d\n",
			pll_config->pll_id,
			pll_data->pll_m_max, pll_data->pll_m_min,
			pll_data->pll_n_max, pll_data->pll_n_min,
			pll_data->pll_od_fb, pll_data->pll_0_5_div_en,
			pll_data->pll_frac_range, pll_data->pll_od_sel_max,
			pll_data->pll_ref_fmax, pll_data->pll_ref_fmin,
			pll_data->pll_vco_fmax, pll_data->pll_vco_fmin,
			pll_data->pll_out_fmax, pll_data->pll_out_fmin,
			pll_data->div_in_fmax, pll_data->div_out_fmax);
	}
}

int lcd_clk_config_print_dft(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct lcd_clk_config_s *cconf = NULL;
	struct lcd_pll_config_s *pll_config = NULL;
	int pll_num = 1, i = 0;
	int n, len = 0;

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf || !cconf->data)
		return -1;

	if (cconf->pll_mode & LCD_PLL_MODE_DUAL_PLL)
		pll_num = cconf->pll_conf_num;
	for (i = 0; i < pll_num; i++) {
		pll_config = &cconf->pll_config[i];
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"[%d]: pll[%d] config:\n"
			"  pll_offset: 0x%x\n"
			"  pll_m:      %d\n"
			"  pll_n:      %d\n"
			"  pll_frac:   0x%x\n"
			"  pll_frac_half_shift: %d\n"
			"  pll_fvco:   %lluHz\n"
			"  pll_od:     [%d %d %d]\n"
			"  pll_out:    %lldHz\n"
			"  div_sel:    %s(index %d)\n"
			"  pll_div_fout: %uHz\n\n",
			pdrv->index, pll_config->pll_id,
			pll_config->pll_offset,
			pll_config->pll_m, pll_config->pll_n,
			pll_config->pll_frac, pll_config->pll_frac_half_shift,
			pll_config->pll_fvco,
			pll_config->pll_od1_sel, pll_config->pll_od2_sel,
			pll_config->pll_od3_sel, pll_config->pll_fout,
			lcd_clk_div_table[pll_config->div_sel].name,
			pll_config->div_sel, pll_config->pll_div_fout);
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n,
		"[%d]: clk config:\n"
		"  pll_mode:   0x%x\n"
		"  pll_tcon_div_sel: %d\n"
		"  phy_clk:    %lldHz\n"
		"  edp_div0:   %d\n"
		"  edp_div1:   %d\n"
		"  xd:         %d\n"
		"  fout:       %uHz\n"
		"  vclk_sel:   %d\n",
		pdrv->index, cconf->pll_mode,
		cconf->pll_tcon_div_sel, cconf->phy_clk,
		edp_div0_table[cconf->edp_div0], edp_div1_table[cconf->edp_div1],
		cconf->xd, cconf->fout, cconf->data->vclk_sel);
	if (cconf->data->ss_support) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n,
			"  ss_level:   %d\n"
			"  ss_dep_sel: %d\n"
			"  ss_str_m:   %d\n"
			"  ss_ppm:     %d\n"
			"  ss_freq:    %d\n"
			"  ss_mode:    %d\n"
			"  ss_en:      %d\n\n",
			cconf->ss_level,
			cconf->ss_dep_sel, cconf->ss_str_m, cconf->ss_ppm,
			cconf->ss_freq, cconf->ss_mode, cconf->ss_en);
	}

	return len;
}

int lcd_prbs_clk_check(unsigned int encl_clk, unsigned int encl_msr_id,
			unsigned int fifo_clk, unsigned int fifo_msr_id, unsigned int cnt)
{
	unsigned long clk_check, clk_msr, temp;

	if (encl_msr_id == LCD_CLK_MSR_INVALID)
		goto lcd_prbs_clk_check_next;
	clk_check = encl_clk;
	clk_msr = meson_clk_measure(encl_msr_id);
	if (clk_check != clk_msr) {
		temp = lcd_u64_diff(clk_check, clk_msr);
		if (temp >= PLL_CLK_CHECK_MAX) {
			LCD_ERR(NULL, "encl_clk: chk %ld, msr %ld, cnt:%d",
				clk_check, clk_msr, cnt);
			return -1;
		}
	}

lcd_prbs_clk_check_next:
	if (fifo_msr_id == LCD_CLK_MSR_INVALID)
		return 0;
	clk_check = fifo_clk;
	clk_msr = meson_clk_measure(fifo_msr_id);
	if (clk_check != clk_msr) {
		temp = lcd_u64_diff(clk_check, clk_msr);
		if (temp >= PLL_CLK_CHECK_MAX) {
			LCD_ERR(NULL, "fifo_clk: chk %ld, msr %ld, cnt:%d",
				clk_check, clk_msr, cnt);
			return -1;
		}
	}

	return 0;
}

struct lcd_clktree_list_s {
	int type;
	const char *name;
	size_t offset;
};

static struct lcd_clktree_list_s clktree_list_table[] = {
	{
		.type = CLKTREE_GP0_PLL,
		.name = "gp0_pll",
		.offset = offsetof(struct lcd_clktree_s, gp0_pll)
	},
	{
		.type = CLKTREE_ENCL_TOP_GATE,
		.name = "encl_top_gate",
		.offset = offsetof(struct lcd_clktree_s, encl_top_gate)
	},
	{
		.type = CLKTREE_ENCL_INT_GATE,
		.name = "encl_int_gate",
		.offset = offsetof(struct lcd_clktree_s, encl_int_gate)
	},
	{
		.type = CLKTREE_DSI_A_HOST_GATE,
		.name = "dsi_host_gate",
		.offset = offsetof(struct lcd_clktree_s, dsi_host_gate)
	},
	{
		.type = CLKTREE_DSI_A_PHY_GATE,
		.name = "dsi_phy_gate",
		.offset = offsetof(struct lcd_clktree_s, dsi_phy_gate)
	},
	{
		.type = CLKTREE_DSI_A_MEAS,
		.name = "dsi_meas",
		.offset = offsetof(struct lcd_clktree_s, dsi_meas)
	},
	{
		.type = CLKTREE_TCON_GATE,
		.name = "tcon_gate",
		.offset = offsetof(struct lcd_clktree_s, tcon_gate)
	},
	{
		.type = CLKTREE_TCON,
		.name = "clk_tcon",
		.offset = offsetof(struct lcd_clktree_s, tcon_clk)
	},
};

static struct lcd_clktree_list_s *lcd_clktree_get(struct aml_lcd_drv_s *pdrv, int type)
{
	struct lcd_clktree_list_s *find = NULL;
	int list_size = ARRAY_SIZE(clktree_list_table);
	int i;

	for (i = 0; i < list_size; i++) {
		if (clktree_list_table[i].type == type) {
			find = &clktree_list_table[i];
			break;
		}
	}
	if (!find)
		LCD_ERR(pdrv, "clktree_type %d failed", type);

	return find;
}

void lcd_clktree_bind(struct aml_lcd_drv_s *pdrv, unsigned char status)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clktree_list_s *clk_list;
	unsigned char i, clk_type, cnt = 0, clk_use;
	void *clktree_base;
	struct clk **clk_temp;
	char clk_names[120];

	memset(clk_names, 0, sizeof(clk_names));

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	cconf->clktree.clk_gate_state = 0;
	clktree_base = (void *)&cconf->clktree;

	for (i = 0; i < MAX_CLKTREE_GATE; i++) {
		clk_type = cconf->data->clktree_index[i];
		if (!clk_type)
			break;

		clk_use = 0;
		switch (clk_type) {
		case CLKTREE_GP0_PLL:
			if (cconf->data->vclk_sel) //when G12A/B use GP0 PLL
				clk_use = 1;
			break;
		case CLKTREE_ENCL_TOP_GATE:
		case CLKTREE_ENCL_INT_GATE:
			clk_use = 1;
			break;
		case CLKTREE_DSI_A_HOST_GATE:
		case CLKTREE_DSI_A_PHY_GATE:
		case CLKTREE_DSI_A_MEAS:
			if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MIPI)
				clk_use = 1;
			break;
		case CLKTREE_TCON_GATE:
		case CLKTREE_TCON:
			if (pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_MLVDS ||
			    pdrv->curr_dev->dev_cfg.basic.lcd_type == LCD_P2P)
				clk_use = 1;
			break;
		default:
			break;
		}

		if (!clk_use)
			continue;
		clk_list = lcd_clktree_get(pdrv, clk_type);
		if (!clk_list)
			continue;

		clk_temp = (struct clk **)(clktree_base + clk_list->offset);
		if (status)
			*clk_temp = devm_clk_get(pdrv->dev, clk_list->name);
		else
			devm_clk_put(pdrv->dev, *clk_temp);

		if (IS_ERR_OR_NULL(*clk_temp) && status) {
			LCD_ERR(pdrv, "%s failed", clk_list->name);
			continue;
		}
		cnt += snprintf(clk_names + cnt, 120 - cnt, " %s", clk_list->name);
	}
	LCD_DBG(pdrv, "clktree %s:%s done", status ? "probe" : "remove", clk_names);
}

void lcd_clktree_gate_switch(struct aml_lcd_drv_s *pdrv, unsigned char status)
{
	struct lcd_clk_config_s *cconf;
	struct lcd_clktree_list_s *clk_list;
	unsigned char i, clk_type, cnt = 0;
	void *clktree_base;
	struct clk **clk_temp;
	char clk_names[120];

	memset(clk_names, 0, sizeof(clk_names));

	cconf = get_lcd_clk_config(pdrv);
	if (!cconf)
		return;
	cconf->clktree.clk_gate_state = status;
	clktree_base = (void *)&cconf->clktree;

	for (i = 0; i < MAX_CLKTREE_GATE; i++) {
		clk_type = cconf->data->clktree_index[i];
		if (!clk_type)
			break;
		clk_list = lcd_clktree_get(pdrv, clk_type);
		if (!clk_list)
			continue;

		clk_temp = (struct clk **)(clktree_base + clk_list->offset);
		if (IS_ERR_OR_NULL(*clk_temp))
			continue;

		if (status)
			clk_prepare_enable(*clk_temp);
		else
			clk_disable_unprepare(*clk_temp);

		cnt += snprintf(clk_names + cnt, 120 - cnt, " %s", clk_list->name);
	}
	LCD_DBG(pdrv, "clktree: %s -> %s", clk_names, status ? "on" : "off");
}
