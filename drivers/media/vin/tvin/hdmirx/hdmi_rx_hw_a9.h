/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_A9_H
#define _HDMI_RX_A9_H

/* A9 PHY register */
#define A9_HDMIRX20PLL_CTRL0			(0x000 << 2)
#define A9_HDMIRX20PLL_CTRL1			(0x001 << 2)
#define A9_HDMIRX20PHY_DCHA_AFE		(0x002 << 2)
	#define A9_ITUNE_AFE_POLY		MSK(2, 30)
	#define A9_ITUNE_AFE_PLAT		MSK(2, 28)
	#define A9_LEQ_HYPER_GAIN_CH0		_BIT(3)
	#define A9_LEQ_HYPER_GAIN_CH1		_BIT(7)
	#define A9_LEQ_HYPER_GAIN_CH2		_BIT(11)
	#define A9_BUF_BST					MSK(3, 25)
	#define A9_LEQ_BUF_GAIN            MSK(2, 17)
	#define A9_LEQ_POLE                MSK(3, 12)
	#define A9_VGA_GAIN				MSK(12, 0)
#define A9_HDMIRX20PHY_DCHA_DFE		(0x003 << 2)
	#define A9_PI_BAND_SEL	MSK(2, 26)
	#define A9_PI_DIV_SEL	MSK(2, 24)
	#define A9_TMDS_DIV_SEL	MSK(3, 20)
	#define A9_DCHA_DFE				MSK(5, 12)
	#define A9_PI_CFG					MSK(8, 20)
	#define A9_DFE_OFST_CAL_START		_BIT(9)
#define A9_HDMIRX20PHY_DCHD_CDR		(0x004 << 2)
#define A9_EHM_DBG_SEL			_BIT(31)
#define A9_DFE_OFST_DBG_SEL		MSK(3, 28)
	#define A9_OFSET_CAL_START		_BIT(27)
	#define A9_CDR_LKDET_EN		_BIT(14)
	#define A9_CDR_RSTB			_BIT(13)
	#define A9_CDR_EN              _BIT(12)
	#define A9_CDR_FR_EN				_BIT(6)
	#define A9_MUX_CDR_DBG_SEL		_BIT(19)
	#define A9_CDR_PI_DIV			MSK(2, 10)
	#define A9_CDR_OS_RATE			MSK(2, 8)
	#define A9_CDR_PH_DIV	MSK(3, 0)
	#define A9_DFE_OFST_DBG_SEL		MSK(3, 28)
	#define A9_ERROR_CNT			0X0
	#define A9_SCAN_STATE			0X1
	#define A9_POSITIVE_EYE_HEIGHT		0x2
	#define A9_NEGATIVE_EYE_HEIGHT		0x3
	#define A9_LEFT_EYE_WIDTH		0x4
	#define A9_RIGHT_EYE_WIDTH		0x5
#define A9_HDMIRX20PHY_DCHD_EQ			(0x005 << 2)
	#define A9_BW_MAX_EN			_BIT(31)
	#define A9_BYP_TAP0_EN			_BIT(30)
	#define A9_BYP_TAP_EN			_BIT(19)
	#define A9_DFE_HOLD_EN			_BIT(18)
	#define A9_DFE_RSTB			_BIT(17)
	#define A9_DFE_EN			_BIT(16)
	#define A9_EHM_SW_SCAN_EN		_BIT(15)
	#define A9_EHM_HW_SCAN_EN		_BIT(14)
	#define A9_EQ_RSTB			_BIT(13)
	#define A9_EQ_EN			_BIT(12)
	#define A9_EN_BYP_EQ			_BIT(5)
	#define A9_BYP_EQ			MSK(5, 0)
	#define A9_BYP_EN			_BIT(5)
	#define A9_BYP_EQ_VAL			MSK(5, 0)
	#define A9_EQ_MODE			MSK(2, 8)
	#define A9_EQ_ERR_MODE			MSK(2, 10)
	#define A9_STATUS_MUX_SEL		MSK(2, 22)
#define A9_HDMIRX20PHY_DCHA_MISC1		(0x006 << 2)
	#define A9_SQ_RSTN			_BIT(26)
	#define A9_VCO_TMDS_EN			_BIT(20)
	#define A9_RTERM_CNTL			MSK(4, 12)
	#define A9_DCH_RSTN			MSK(3, 4)
	#define RTERM_FLAG_EFUSE_A9	_BIT(2)
#define A9_HDMIRX20PHY_DCHA_MISC2		(0x007 << 2)
	#define RTERM_EN_SEL	MSK(4, 28)
	#define A9_TMDS_VALID_SEL		_BIT(10)
	#define A9_PLL_CLK_SEL			_BIT(9)
#define A9_CABLE_CLK_SEL			_BIT(5)
#define A9_HDMIRX20PHY_DCHD_STAT       (0x009 << 2)
#define A9_HDMIRX_EARCTX_CNTL0         (0x040 << 2)
#define A9_HDMIRX_EARCTX_CNTL1         (0x041 << 2)
#define A9_HDMIRX_ARC_CNTL             (0x042 << 2)
#define A9_HDMIRX_PHY_PROD_TEST0       (0x080 << 2)
#define A9_HDMIRX_PHY_PROD_TEST1       (0x081 << 2)

#define A9_RG_RX20PLL_0		0x000
#define A9_RG_RX20PLL_1		0x004

/* i2c monitor reg */
#define A9_I2C_MONITOR_BASE		0x2c000

/* audio pll reg */
#define ANACTRL_AUD_PLL_CNTL_A9	(0x0080  << 2)
#define ANACTRL_AUD_PLL_CNTL2_A9	(0x0081  << 2)
#define ANACTRL_AUD_PLL_CNTL3_A9	(0x0082  << 2)
#define ANACTRL_AUD_PLL_STS_A9	(0x0083  << 2)
#define ANACTRL_AUD_PLL4X_SCAN_CNTL_A9	(0x0084  << 2)
#define ANACTRL_VDAC_CTRL0_A9	(0x01b0  << 2)
#define ANACTRL_VDAC_CTRL1_A9	(0x01b1  << 2)

extern int tapx_value;
extern int agc_enable;
extern u32 afe_value;
extern u32 dfe_value;
extern u32 cdr_value;
extern u32 eq_value;
extern u32 misc2_value;
extern u32 misc1_value;
/*--------------------------function declare------------------*/
/* A9 */
void aml_phy_init_a9(void);
void aml_eq_eye_monitor_a9(void);
void dump_reg_phy_a9(void);
void dump_aml_phy_sts_a9(void);
int aml_phy_short_bist_a9(void);
bool aml_get_tmds_valid_a9(void);
void aml_phy_power_off_a9(void);
void aml_phy_switch_port_a9(void);
void aml_phy_iq_skew_monitor_a9(void);
void dump_vsi_reg_a9(u8 port);
unsigned int rx_sec_hdcp_cfg_a9(void);
void rx_set_aud_output_a9(u32 param);
void rx_sw_reset_a9(int level);
void hdcp_init_a9(void);
void aml_phy_get_trim_val_a9(void);
void comb_val_a9(void (*p)(char *, unsigned int, int),
	     char *type, unsigned int val_0, unsigned int val_1,
		 unsigned int val_2, int len);
void get_flag_val_a9(char *temp, unsigned int val, int len);
void get_val_a9(char *temp, unsigned int val, int len);
void get_eq_val_a9(void);
void bubble_sort(u32 *sort_array);
void quick_sort2_a9(int arr[], int l, int r);
void clk_init_cor_a9(void);
void rx_dig_clk_en_a9(bool en);
void aml_phy_offset_cal_a9(void);
void rx_aud_pll_ctl_a9(bool en, u8 port);
void aml_phy_exbist_a9(u8 port, u8 ch);
void rx_i2c_mux_cfg_a9(u8 port);
bool rx_is_power_off_a9(void);
void rx_dump_pll_param_a9(void);
/*function declare end*/

#endif /*_HDMI_RX_A9_H*/

