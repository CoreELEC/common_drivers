/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef _HDMI_RX_T6X_H
#define _HDMI_RX_T6X_H

/* 1st 2.0 PHY base addr: 0xfe39c000 */
/* 2st 2.0 PHY base addr: 0xfe39c400 */
/* 1st 2.1 PHY base addr: 0xfe39c800 */
/* 2st 2.0 PHY base addr: 0xfe39cc00 */

/* T6X HDMI2.0 PHY register */
#define T6X_HDMIRX20PLL_CTRL0			(0x000 << 2)
#define T6X_HDMIRX20PLL_CTRL1			(0x001 << 2)
#define T6X_HDMIRX20PHY_DCHA_AFE		(0x002 << 2)
	#define T6X_20_LEQ_HYPER_GAIN_CH0		_BIT(3)
	#define T6X_20_LEQ_HYPER_GAIN_CH1		_BIT(7)
	#define T6X_20_LEQ_HYPER_GAIN_CH2		_BIT(11)
	#define T6X_20_LEQ_BUF_GAIN            MSK(3, 16)
	#define T6X_20_LEQ_POLE                MSK(3, 12)
#define T6X_HDMIRX20PHY_DCHA_DFE		(0x003 << 2)
	#define T6X_20_SLICER_OFSTCAL_START	_BIT(13)
#define T6X_HDMIRX20PHY_DCHD_CDR		(0x004 << 2)
	#define T6X_20_EHM_DBG_SEL			_BIT(31)
	#define T6X_20_OFSET_CAL_START		_BIT(27)
	#define T6X_20_CDR_LKDET_EN		_BIT(14)
	#define T6X_20_CDR_RSTB			_BIT(13)
	#define T6X_20_CDR_EN              _BIT(12)
	#define T6X_20_CDR_FR_EN				_BIT(6)
	#define T6X_20_MUX_CDR_DBG_SEL		_BIT(19)
	#define T6X_20_CDR_OS_RATE			MSK(2, 8)
	#define T6X_20_DFE_OFST_DBG_SEL		MSK(3, 28)
	#define T6X_20_ERROR_CNT			0X0
	#define T6X_20_20_SCAN_STATE			0X1
	#define T6X_20_POSITIVE_EYE_HEIGHT		0x2
	#define T6X_20_NEGATIVE_EYE_HEIGHT		0x3
	#define T6X_20_LEFT_EYE_WIDTH		0x4
	#define T6X_20_RIGHT_EYE_WIDTH		0x5
#define T6X_HDMIRX20PHY_DCHD_EQ			(0x005 << 2)
	#define T6X_20_BYP_TAP0_EN			_BIT(30)
	#define T6X_20_BYP_TAP_EN			_BIT(19)
	#define T6X_20_DFE_HOLD_EN			_BIT(18)
	#define T6X_20_DFE_RSTB			_BIT(17)
	#define T6X_20_DFE_EN			_BIT(16)
	#define T6X_20_EHM_SW_SCAN_EN		_BIT(15)
	#define T6X_20_EHM_HW_SCAN_EN		_BIT(14)
	#define T6X_20_EQ_RSTB			_BIT(13)
	#define T6X_20_EQ_EN			_BIT(12)
	#define T6X_20_EN_BYP_EQ			_BIT(5)
	#define T6X_20_BYP_EQ			MSK(5, 0)
	#define T6X_20_EQ_MODE			MSK(2, 8)
	#define T6X_20_STATUS_MUX_SEL		MSK(2, 22)
#define T6X_HDMIRX20PHY_DCHA_MISC1		(0x006 << 2)
	#define T6X_20_SQ_RSTN			_BIT(26)
	#define T6X_20_VCO_TMDS_EN			_BIT(20)
	#define RTERM_VAL_T6X_20			MSK(4, 12)
	#define RTERM_FLAG_T6X_20	_BIT(0)
#define T6X_HDMIRX20PHY_DCHA_MISC2		(0x007 << 2)
	#define T6X_20_TMDS_VALID_SEL		_BIT(10)
	#define T6X_20_PLL_CLK_SEL			_BIT(9)
#define T6X_HDMIRX20PHY_DCHD_STAT       (0x009 << 2)
//#define T6X_HDMIRX_EARCTX_CNTL0         (0x040 << 2)
//#define T6X_HDMIRX_EARCTX_CNTL1         (0x041 << 2)
#define T6X_HDMIRX_PHY_PROD_TEST0       (0x020 << 2)
#define T6X_HDMIRX_PHY_PROD_TEST1       (0x021 << 2)

#define T6X_RG_RX20PLL_0		0x000
#define T6X_RG_RX20PLL_1		0x004

/* T6X HDMI2.1 PHY register */
#define T6X_HDMIRX21PLL_CTRL0           (0x40 << 2)
#define T6X_HDMIRX21PLL_CTRL1           (0x41 << 2)
#define T6X_HDMIRX21PLL_CTRL2           (0x42 << 2)
#define T6X_HDMIRX21PLL_CTRL3           (0x43 << 2)
#define T6X_HDMIRX21PLL_CTRL4           (0x44 << 2)
#define T6X_HDMIRX21PHY_MISC0           (0x45 << 2)
	#define DCH_RSTN	MSK(4, 0)
	#define CCH_EN		MSK(2, 16)
#define T6X_HDMIRX21PHY_MISC1           (0x46 << 2)
	#define SQ_RSTN	_BIT(23)
	#define SQ_GATED	_BIT(29)
#define T6X_HDMIRX21PHY_MISC2           (0x47 << 2)
	#define RTERM_FLAG_T6X_21	_BIT(31)
	#define LDO_VTUNE MSK(4, 8)
	#define RTERM_VAL_T6X_21	MSK(4, 0)
#define T6X_HDMIRX21PHY_DCHA_AFE        (0x48 << 2)
	#define HYPER_GAIN	MSK(4, 16)
	#define BUF_BST		MSK(3, 28)
	#define LEQ_POLE	MSK(3, 20)
	#define BUF_GAIN	MSK(3, 24)
#define T6X_HDMIRX21PHY_DCHA_DFE        (0x49 << 2)
	#define VGA_GAIN	MSK(16, 0)
	#define DFE_TAP_EN	MSK(9, 16)
	#define DFE_OFST_CAL_START _BIT(27)
	#define DFE_H1_PD	_BIT(29)
#define T6X_HDMIRX21PHY_DCHA_PI         (0x4a << 2)
#define T6X_HDMIRX21PHY_DCHA_CTRL       (0x4b << 2)
#define T6X_HDMIRX21PHY_DCHD_CDR        (0x4c << 2)
	#define CDR_PH_DIV	MSK(3, 0)
	#define CDR_FR_EN	_BIT(6)
	#define CDR_OS_RATE	MSK(2, 8)
	#define CDR_PI_DIV	MSK(2, 10)
	#define CDR_RSTB	_BIT(13)
	#define CDR_LKDET_EN	_BIT(14)
	#define OFST_CAL_STAGE	MSK(2, 16)
	#define MUX_CDR_DBG_SEL	_BIT(19)
	#define CDR_PI_OFST	MSK(6, 20)
	#define OFST_CAL_START _BIT(27)
	#define MUX_DFE_OFST_EYE	MSK(3, 28)
	#define MUX_EYE_EN		_BIT(31)
#define T6X_HDMIRX21PHY_DCHD_EQ         (0x4d << 2)
	#define EQ_BYP_VAL1	MSK(5, 0)
	#define EQ_BYP_EN	_BIT(5)
	#define EQ_MODE	MSK(2, 8)
	#define EQ_OFST_CAL_START _BIT(11)
	#define EQ_RSTB	_BIT(13)
	#define EQ_EYE_EN_HW_SCAN _BIT(14)
	#define DFE_RSTB _BIT(17)
	#define DFE_HOLD _BIT(18)
	#define DFE_TAPS_DISABLE	_BIT(19)
	#define MUX_BLOCK_SEL		MSK(2, 22)
#define T6X_HDMIRX21PHY_DCH_STAT        (0x4e << 2)
#define T6X_HDMIRX21PHY_TEST			(0x4f << 2)
#define T6X_HDMIRX21PHY_PROD_TEST0      (0x60 << 2)
#define T6X_HDMIRX21PHY_PROD_TEST1      (0x61 << 2)
/* T6X HDMI2.1 PHY FPLL register */
#define T6X_ANACTRL_HDMI_PLL0_CTRL0		(0x0090  << 2)
#define T6X_ANACTRL_HDMI_PLL0_CTRL1		(0x0091  << 2)
#define T6X_ANACTRL_HDMI_PLL0_CTRL2		(0x0092  << 2)
#define T6X_ANACTRL_HDMI_PLL0_CTRL3		(0x0093  << 2)
#define T6X_ANACTRL_HDMI_PLL0_STS		(0x0094  << 2)
#define T6X_ANACTRL_HDMI_PLL1_CTRL0		(0x00a0  << 2)
#define T6X_ANACTRL_HDMI_PLL1_CTRL1		(0x00a1  << 2)
#define T6X_ANACTRL_HDMI_PLL1_CTRL2		(0x00a2  << 2)
#define T6X_ANACTRL_HDMI_PLL1_CTRL3		(0x00a3  << 2)
#define T6X_ANACTRL_HDMI_PLL1_STS		(0x00a4  << 2)

#define T6X_CLKCTRL_AUD21_PLL_CTRL0		(0x02ea  << 2)
#define T6X_CLKCTRL_AUD21_PLL_CTRL1		(0x02eb  << 2)
#define T6X_CLKCTRL_AUD21_PLL_CTRL2		(0x02ec  << 2)
#define T6X_CLKCTRL_AUD21_PLL_CTRL3		(0x02ed  << 2)
#define T6X_CLKCTRL_AUD21_PLL_STS		(0x02ee  << 2)

/* i2c monitor reg */
#define T6X_I2C_MONITOR_BASE			0x14000

extern int dts_debug_flag_t6x_21;
extern int rlevel_t6x_21;
extern int rterm_trim_val_t6x_21;
extern int rterm_trim_flag_t6x_21;
extern int debug_port;
extern int bist_lvl;
extern int bist_mode;
extern int l_bist_en;
extern int frl_rate_id;
/*--------------------------function declare------------------*/
/* T6X */
void aml_phy_init_t6x(u8 port);
void aml_eq_eye_monitor_t6x(u8 port);
void dump_reg_phy_t6x(u8 port);
void dump_aml_phy_sts_t6x(u8 port);
void aml_phy_short_bist_t6x(u8 port);
bool aml_get_tmds_valid_t6x(u8 port);
void aml_phy_power_off_t6x(u8 port);
void aml_phy_switch_port_t6x(u8 port);
void aml_phy_iq_skew_monitor_t6x(void);
void get_val_t6x(char *temp, unsigned int val, int len);
unsigned int rx_sec_hdcp_cfg_t6x(void);
void rx_set_aud_output_t6x(u32 param, u8 port);
void rx_sw_reset_t6x(int level, u8 port);
void hdcp_init_t6x(u8 port);
void aml_phy_get_trim_val_t6x(void);
void dump_aml_phy_sts_t6x(u8 port);
void comb_val_t6x(void (*p)(char *, unsigned int, int),
	     char *type, unsigned int val_0, unsigned int val_1,
		 unsigned int val_2, int len);
void get_flag_val_t6x(char *temp, unsigned int val, int len);
void aml_phy_offset_cal_t6x(void);
void quick_sort2_t6x_20(int arr[], int l, int r);
void rx_pwrcntl_mem_pd_cfg(void);
void rx_long_bist_t6x(void);
void rx_t6x_prbs(void);
void dump_aud21_param_t6x(u8 port);
void rx_21_fpll_cfg_t6x(int f_rate, u8 port);
void audio_setting_for_aud21_t6x(int frl_rate, u8 port);
void clk_init_cor_t6x(void);
void rx_dig_clk_en_t6x(bool en);
void rx_lts_2_flt_ready(u8 port);
int rx_lts_p_syn_detect(u8 frl_rate, u8 port);
void aml_phy_init_t6x_21(u8 port);
int rx_lts_3_err_detect(u8 port);
bool hdmirx_flt_update_cleared_wait(u32 addr, u8 port);
void hdmirx_vga_gain_tuning_t6x(u8 port);
void rx_set_term_value_t6x(unsigned char port, bool value);
void aml_phy_power_off_t6x_20(u8 port);
void aml_phy_power_off_t6x_21(u8 port);
void rx_mute_t6x(bool en, u8 port_type);
bool rx_is_power_off_t6x(u8 port);
void rx_aud_pll_ctl_t6x(bool en, u8 port);
void hdmirx_fpll_recovery_t6x(u8 port);
void rx_21_dump_fpll_0_t6x(void);
void rx_21_dump_fpll_1_t6x(void);
void aml_phy_exbist_t6x_20(u8 port, u8 ch);
void aml_phy_exbist_t6x_21(u8 port, u8 ch);
void aml_phy_exbist_ber_check_t6x_21(u8 port);
void aml_phy_exbist_ber_rst_t6x_21(u8 port);
bool rx_check_tap0_t6x(void);
//void reset_pcs(void);

/*function declare end*/

#endif /*_HDMI_RX_T6X_H*/

