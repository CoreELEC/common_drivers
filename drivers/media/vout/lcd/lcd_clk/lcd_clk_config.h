/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LCD_CLK_CONFIG_H
#define _LCD_CLK_CONFIG_H

#include <linux/types.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

/* **********************************
 * clk config
 * **********************************
 */
/*bit7:0 from panel parameter pll_flag*/
#define LCD_PLL_MODE_DEFAULT         BIT(0)
#define LCD_PLL_MODE_SPECIAL_CNTL    BIT(1)
#define LCD_PLL_MODE_FRAC_SHIFT      BIT(2)
/*pll control*/
#define LCD_PLL_MODE_DUAL_PLL        BIT(8)

#define PLL_RETRY_MAX		20
#define LCD_CLK_CTRL_EN      0
#define LCD_CLK_CTRL_RST     1
#define LCD_CLK_CTRL_M	     2
#define LCD_CLK_CTRL_FRAC    3
#define LCD_CLK_CTRL_END     0xffff

#define LCD_SSC_LEVEL        BIT(0)
#define LCD_SSC_FREQ         BIT(1)
#define LCD_SSC_MODE         BIT(2)

#define LCD_CLK_REG_END      0xffff
#define LCD_CLK_CTRL_CNT_MAX 10
struct lcd_clk_ctrl_s {
	unsigned int flag;
	unsigned int reg;
	unsigned int bit;
	unsigned int len;
};

#define LCD_PRBS_MODE_LVDS    BIT(0)
#define LCD_PRBS_MODE_VX1     BIT(1)
#define LCD_PRBS_MODE_FREQ    BIT(2)
#define LCD_PRBS_MODE_MAX     3
#define LCD_CLK_MSR_INVALID   0xffffffff

struct lcd_clktree_s {
	unsigned char clk_gate_state;

	struct clk *encl_top_gate;
	struct clk *encl_int_gate;

	struct clk *dsi_host_gate;
	struct clk *dsi_phy_gate;
	struct clk *dsi_meas;
	struct clk *mipi_enable_gate;
	struct clk *mipi_bandgap_gate;
	struct clk *gp0_pll;
	struct clk *tcon_gate;
	struct clk *tcon_clk;
};

#define MAX_CLKTREE_GATE 6
enum clktree_type {
	CLKTREE_GP0_PLL = 1,
	CLKTREE_ENCL_TOP_GATE,
	CLKTREE_ENCL_INT_GATE,
	CLKTREE_DSI_A_HOST_GATE,
	CLKTREE_DSI_A_PHY_GATE,
	CLKTREE_DSI_A_MEAS,
	CLKTREE_TCON_GATE,
	CLKTREE_TCON,
};

struct lcd_clk_config_s;
// reference to: https://confluence.amlogic.com/display/SW/LCD+CLK+porting+note
struct lcd_pll_data_s {
	/*pll para limit*/
	unsigned int pll_od_fb;
	unsigned int pll_div_0p5_en;
	unsigned int pll_m_max;
	unsigned int pll_m_min;
	unsigned int pll_n_max;
	unsigned int pll_n_min;
	unsigned int pll_frac_range;
	unsigned int pll_frac_sign_bit;
	unsigned int pll_offset;

	/*od and tcon_div*/
	unsigned int od_cnt;
	unsigned int pll_od_sel_max;
	unsigned int have_tcon_div;
	unsigned int pll_od_div_5_7;

	/*pll freq limit*/
	unsigned int pll_ref_fmax;
	unsigned int pll_ref_fmin;
	unsigned long long pll_vco_fmax;
	unsigned long long pll_vco_fmin;
	unsigned long long pll_out_fmax;
	unsigned long long pll_out_fmin;
	/*vid pll limit*/
	unsigned long long div_in_fmax;
	unsigned int div_out_fmax;
	unsigned int div_sel_max;

};

// reference to: https://confluence.amlogic.com/display/SW/LCD+CLK+porting+note
struct lcd_clk_data_s {
	/*pll data*/
	struct lcd_pll_data_s *pll_data[2];
	/* clk path node parameters */
	unsigned int xd_out_fmax;
	//0:pll_clk_phase, 1:pll_clk2, 2:vid_pll_clk
	unsigned int phy_clk_location;

	unsigned short xd_max;
	unsigned short phy_div_max;

	/*only pll for phy need ss, so put it here */
	unsigned int ss_support;
	unsigned int ss_level_max;
	unsigned int ss_freq_max;
	unsigned int ss_mode_max;
	unsigned int ss_dep_base;
	unsigned int ss_dep_sel_max;
	unsigned int ss_str_m_max;
	unsigned char *ss_freq_dep_opt;

	unsigned char vclk_sel;
	unsigned int enc_clk_msr_id;
	unsigned int fifo_clk_msr_id;
	unsigned int tcon_clk_msr_id;

	void (*clktree_set)(struct aml_lcd_drv_s *pdrv);
	unsigned char clktree_index[MAX_CLKTREE_GATE];

	//for some parameter changed to different lcd interface
	void (*clk_parameter_init)(struct aml_lcd_drv_s *pdrv);
	void (*clk_generate_parameter)(struct aml_lcd_drv_s *pdrv);
	void (*pll_frac_generate)(struct aml_lcd_drv_s *pdrv);
	void (*set_ss)(struct aml_lcd_drv_s *pdrv, unsigned int ss_flag);
	void (*clk_ss_enable)(struct aml_lcd_drv_s *pdrv, int status);
	void (*clk_ss_init)(struct lcd_clk_config_s *cconf);
	void (*pll_frac_set)(struct aml_lcd_drv_s *pdrv, unsigned int frac);
	void (*pll_m_set)(struct aml_lcd_drv_s *pdrv, unsigned int m);
	unsigned long long (*pll_hz_get)(struct aml_lcd_drv_s *pdrv);
	void (*pll_reset)(struct aml_lcd_drv_s *pdrv);
	void (*clk_set)(struct aml_lcd_drv_s *pdrv);
	void (*vclk_crt_set)(struct aml_lcd_drv_s *pdrv);
	void (*clk_set_dummy)(struct aml_lcd_drv_s *pdrv);
	void (*clk_disable)(struct aml_lcd_drv_s *pdrv);
	int (*mlvds_clk_phase_set)(struct aml_lcd_drv_s *pdrv);
	void (*clk_config_init_print)(struct aml_lcd_drv_s *pdrv);
	int (*clk_config_print)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	int (*clk_reg_print)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	void (*prbs_test)(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag);
};

struct lcd_pll_config_s {
	/* pll parameters */
	unsigned int pll_id;
	unsigned int pll_m;
	unsigned int pll_n;
	unsigned long long pll_fvco;
	unsigned int pll_od1_sel;
	unsigned int pll_od2_sel;
	unsigned int pll_od3_sel;
	unsigned int pll_level;
	unsigned int pll_frac;
	unsigned int pll_frac_half_shift;
	unsigned long long pll_fout;
	unsigned int pll_div_fout;
	unsigned int div_sel;
	unsigned int done;
};

struct lcd_clk_config_s { /* unit: Hz */
	/* IN-OUT parameters */
	unsigned int fin;
	unsigned int fout;
	unsigned int prbs_mode;
	unsigned int pll_mode;

	unsigned char pll_conf_num;
	struct lcd_pll_config_s *pll_config;

	unsigned int pll_tcon_div_sel;
	unsigned long long phy_clk;
	unsigned int ss_level;
	unsigned int ss_dep_sel;
	unsigned int ss_str_m;
	unsigned int ss_ppm;
	unsigned int ss_freq;
	unsigned int ss_mode;
	unsigned int ss_en;
	unsigned int ss_freq_stable;

	unsigned int edp_div0;
	unsigned int edp_div1;
	unsigned int xd;
	unsigned int phy_div;
	unsigned int err_fmin;

	struct lcd_clktree_s clktree;
	struct lcd_clk_data_s *data;
	void (*clk_path_change)(struct aml_lcd_drv_s *pdrv, int sel);
};

enum lcd_clk_mode_e {
	LCD_BIT_RATE_FIXED = 0,  /* pclk and phy use same pll */
	LCD_BIT_RATE_ADAPT,    /* pclk and phy use different pll */
	LCD_CLK_MODE_MAX,
};

struct lcd_clk_config_s *get_lcd_clk_config(struct aml_lcd_drv_s *pdrv);

#endif
