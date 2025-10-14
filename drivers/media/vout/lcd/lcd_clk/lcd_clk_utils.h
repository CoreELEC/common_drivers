/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LCD_CLK_UTILS_H
#define _LCD_CLK_UTILS_H

#include "lcd_clk_config.h"
// #include <linux/amlogic/media/vout/lcd/lcd_vout.h>

/* ****************************************************
 * lcd pll & clk operation
 * ****************************************************
 */
#define PLL_CLK_CHECK_MAX    2000000 /* Hz */
int lcd_clk_msr_check(struct aml_lcd_drv_s *pdrv);
int lcd_pll_ss_level_generate(struct lcd_clk_config_s *cconf);
void lcd_pll_ss_init_dft(struct lcd_clk_config_s *cconf);
int lcd_pll_wait_lock(int id, unsigned int reg, unsigned int lock_bit);
int lcd_pll_wait_lock_hiu(unsigned int reg, unsigned int lock_bit);

/* ****************************************************
 * lcd clk parameters calculate
 * ****************************************************
 */
#define PLL_FVCO_ERR_MAX    2000 /* Hz */
unsigned long long clk_vid_pll_div_calc(unsigned long long clk, unsigned int div_sel, int dir);
int lcd_pll_get_frac(struct lcd_clk_config_s *cconf, int pll_sel, unsigned long long pll_fvco);
int check_vco(struct lcd_clk_config_s *cconf, int pll_sel, unsigned long long pll_fvco);

/* ****************************************************
 * lcd clk chip default func
 * ****************************************************
 */
int lcd_clk_config_print_dft(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
void lcd_pll_frac_generate_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_init_print_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_generate_dft(struct aml_lcd_drv_s *pdrv);
void lcd_clk_generate_prbs_clk(struct aml_lcd_drv_s *pdrv,
		unsigned int enc_clk, unsigned long long bit_rate);
void lcd_set_vid_pll_div_dft(struct aml_lcd_drv_s *pdrv);

void lcd_clktree_bind(struct aml_lcd_drv_s *pdrv, unsigned char status);
void lcd_clktree_gate_switch(struct aml_lcd_drv_s *pdrv, unsigned char status);

/* ****************************************************
 * lcd clk chip init help func
 * ****************************************************
 */
struct lcd_clk_config_s *lcd_clk_config_chip_init_c3(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_g12a(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_g12b(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t7(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t3(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_tm2(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t5(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t5d(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_txhd2(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t5w(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t3x(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_s6(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t6d(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t6w(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_t6x(struct aml_lcd_drv_s *pdrv);
struct lcd_clk_config_s *lcd_clk_config_chip_init_a9(struct aml_lcd_drv_s *pdrv);

/* ****************************************************
 * lcd clk prbs func
 * ****************************************************
 */
extern unsigned int lcd_prbs_flag, lcd_prbs_freq, lcd_prbs_performed, lcd_prbs_err;
int lcd_prbs_clk_check(unsigned int encl_clk, unsigned int encl_msr_id,
			unsigned int fifo_clk, unsigned int fifo_msr_id, unsigned int cnt);

#endif
