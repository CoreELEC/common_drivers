// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/arm-smccc.h>
#include <linux/slab.h>
#include <linux/dma-mapping.h>
#include <linux/highmem.h>

/* Local Include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_hw_a9.h"

/* FT trim flag:1-valid, 0-not valid */
bool rterm_trim_flag_a9;
/* FT trim value 4 bits */
u32 rterm_trim_val_a9;
/* for A9 */
static const u32 phy_misc_a9[][2] = {
		/*  0x18	0x1c	*/
	{	 /* 24~35M */
		0xff330070, 0x11873000,
	},
	{	 /* 37~75M */
		0xff330070, 0x11873000,
	},
	{	 /* 75~115M */
		0xff330070, 0x11873000,
	},
	{	 /* 115~150M */
		0xff330070, 0x11873000,
	},
	{	 /* 150~340M */
		0xff330070, 0x11873000,
	},
	{	 /* 340~525M */
		0xff330070, 0x11873000,
	},
	{	 /* 525~600M */
		0xff330070, 0x11873000,
	},
};

static const u32 phy_dcha_a9[][2] = {
		 /* 0x08	 0x0c*/
		/* some bits default close,reopen when pll stable */
	{	 /* 24~45M */
		0x51827fff, 0x5f79f843,
	},
	{	 /* 35~75M */
		0x51827fff, 0x5739f843,
	},
	{	 /* 75~115M */
		0x51827333, 0x5629f843,
	},
	{	 /* 115~150M */
		0x51827333, 0x5629f843,
	},
	{	 /* 150~340M */
		0x51827333, 0x5519f843,
	},
	{	 /* 340~525M */
		0x51a22333, 0x5409f843,
	},
	{	 /* 525~600M */
		0x53c22333, 0x5409f843,
	},
};

static const u32 phy_dchd_a9[][2] = {
		/*  0x10	 0x14 */
		/* some bits default close,reopen when pll stable */
		/* 0x10:12,13,14,15;0x14:12,13,16,17 */
	{	 /* 24~35M */
		0x04007dd3, 0x302b3060,
	},
	{	 /* 35~75M */
		0x04007cd3, 0x302b3060,
	},
	{	 /* 75~115M */
		0x040078d3, 0x30233069,
	},
	{	 /* 115~150M */
		0x040078d3, 0x30233069,
	},
	{	 /* 140~340M */
		0x040074d3, 0x30233050,
	},
	{	 /* 340~525M */
		0x040070d3, 0x30233050,
	},
	{	 /* 525~600M */
		0x040070d3, 0x30233050,
	},
};

u32 a9_rlevel[] = {8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7};

/*Decimal to Gray code */
unsigned int decimaltogray_a9(unsigned int x)
{
	return x ^ (x >> 1);
}

/* Gray code to Decimal */
unsigned int graytodecimal_a9(unsigned int x)
{
	unsigned int y = x;

	while (x >>= 1)
		y ^= x;
	return y;
}

bool is_pll_lock_a9(void)
{
	return ((hdmirx_rd_amlphy(A9_HDMIRX20PLL_CTRL0) >> 31) & 0x1);
}

void a9_480p_pll_cfg(void)
{
	u8 port = rx_info.main_port;

	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0510a000);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x01415ea5);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0510a001);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0510a003);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0510a007);
	rx[port].phy.aud_div = 2;
}

void a9_720p_pll_cfg(void)
{
	u8 port = rx_info.main_port;

	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05105000);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x01415c95);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05105001);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05105003);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05105007);

	rx[port].phy.aud_div = 1;
}

void a9_1080p_pll_cfg(void)
{
	u8 port = rx_info.main_port;

	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102800);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x01415af5);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102801);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102803);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102807);
	rx[port].phy.aud_div = 0;
}

void a9_4k30_pll_cfg(void)
{
	u8 port = rx_info.main_port;

	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101400);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x014155f5);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101401);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101403);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101407);

	rx[port].phy.aud_div = 0;
}

void a9_4k60_pll_cfg(u32 clkrate)
{
	u8 port = rx_info.main_port;

	if (clkrate) {
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102800);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x01415af5);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102801);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102803);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05102807);
	} else {
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101410);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x01415af5);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101411);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101413);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x05101417);
	}

	rx[port].phy.aud_div = 0;
	usleep_range(10, 20);
}

void aml_pll_bw_cfg_a9(u32 cable_clk)
{
	u32 data32;
	u8 port = rx_info.main_port;
	u32 idx = rx[port].phy.pll_bw;
	u32 phy_bw = rx[port].phy.phy_bw;
	int pll_rst_cnt = 0;
	u32 clk_rate;

	clk_rate = rx_get_scdc_clkrate_sts(port);
	idx = aml_phy_pll_band(cable_clk, clk_rate, port);
	phy_bw = aml_cable_clk_band(cable_clk, clk_rate, port);
	if (!is_clk_stable(port) || !cable_clk)
		return;

	switch (idx) {
	case PLL_BW_0:
		a9_480p_pll_cfg();
		break;
	case PLL_BW_1:
		a9_720p_pll_cfg();
		break;
	case PLL_BW_2:
		a9_1080p_pll_cfg();
		break;
	case PLL_BW_3:
		a9_4k30_pll_cfg();
		break;
	case PLL_BW_4:
		a9_4k60_pll_cfg(clk_rate);
		break;
	}
	/* do 5 times when clk not stable within a interrupt */
	do {
		if (idx == PLL_BW_0)
			a9_480p_pll_cfg();
		if (idx == PLL_BW_1)
			a9_720p_pll_cfg();
		if (idx == PLL_BW_2)
			a9_1080p_pll_cfg();
		if (idx == PLL_BW_3)
			a9_4k30_pll_cfg();
		if (idx == PLL_BW_4)
			a9_4k60_pll_cfg(clk_rate);
		if (log_level & PHY_LOG)
			rx_pr("PLL0=0x%x\n", hdmirx_rd_amlphy(A9_RG_RX20PLL_0));
		if (pll_rst_cnt++ > pll_rst_max) {
			if (log_level & VIDEO_LOG)
				rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG) {
			rx_pr("sq=%d,pll_lock=%d",
			      hdmirx_rd_top(TOP_MISC_STAT0, port) & 0x1,
			      is_pll_lock_a9());
		}
	} while (!is_tmds_clk_stable(port) && is_clk_stable(port) && !aml_phy_pll_lock(port));

	if (rx_info.aml_phy.phy_bwth) {
		data32 = phy_dcha_a9[phy_bw][0];
		hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, data32);
		data32 = phy_dchd_a9[phy_bw][0];
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_OS_RATE,
			(data32 >> 8) & 0x3);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_PI_DIV,
			(data32 >> 10) & 0x3);
		data32 = phy_dchd_a9[phy_bw][1];
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_BYP_EQ,
			data32 & 0x1f);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_BYP_EN,
			(data32 >> 5) & 0x1);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_BYP_TAP_EN,
			(data32 >> 19) & 0x1);
		data32 = phy_dcha_a9[phy_bw][1];
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_PI_CFG,
			(data32 >> 20) & 0xff);
		if (log_level & PHY_LOG)
			rx_pr("phy bwth\n");
	}

	if (rx_info.aml_phy.vga_gain <= 0x777)
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_AFE, A9_VGA_GAIN,
			rx_info.aml_phy.vga_gain);
	if (rx_info.aml_phy.eq_stg1 < 0x1f) {
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_BYP_EQ,
			rx_info.aml_phy.eq_stg1);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EN_BYP_EQ, 0x1);
	}
	if (rx_info.aml_phy.eq_stg2 <= 0xf)
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_AFE, A9_BUF_BST,
			rx_info.aml_phy.eq_stg2);
}

void aml_eq_retry_a9(void)
{
	int data32 = 0;
	int eq_boost0, eq_boost1, eq_boost2;
	u8 port = rx_info.main_port;

	if (rx[port].phy.phy_bw >= PHY_BW_3) {
		if (rx_info.aml_phy.eq_hold)
			hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_MODE,
				rx_info.aml_phy.eq_hold);
		get_eq_val_a9();
		if (rx_info.aml_phy.eq_retry) {
			hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
			hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x3);
			usleep_range(100, 110);
			data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
			eq_boost0 = data32 & 0x1f;
			eq_boost1 = (data32 >> 8)  & 0x1f;
			eq_boost2 = (data32 >> 16)	& 0x1f;
			if (eq_boost0 == 0 || eq_boost0 == 31 ||
				eq_boost1 == 0 || eq_boost1 == 31 ||
				eq_boost2 == 0 || eq_boost2 == 31) {
				hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_MODE, 0);
				hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_RSTB, 0);
				usleep_range(10, 20);
				hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_RSTB, 1);
				usleep_range(10000, 10100);
				if (rx_info.aml_phy.eq_hold)
					hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_MODE,
						rx_info.aml_phy.eq_hold);
				get_eq_val_a9();
			}
		}
	}
}

void aml_dfe_en_a9(void)
{
	if (rx_info.aml_phy.dfe_en) {
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_HOLD_EN, 0);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_RSTB, 0);
		usleep_range(10, 20);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_RSTB, 1);
		usleep_range(100, 110);
		if (rx_info.aml_phy.dfe_hold) {
			usleep_range(1000, 1100);
			hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ,
					      A9_DFE_HOLD_EN, 1);
		}
		if (log_level & PHY_LOG)
			rx_pr("dfe\n");
	}
}

/* phy offset calibration based on different chip and board */
void aml_phy_offset_cal_a9(void)
{
	u32 data32;

	data32 = 0xfb330000;
	if (rx_info.aml_phy.rterm_flag) {
		data32 = ((data32 & (~((0xf << 12) | 0x4))) |
			(rx_info.aml_phy.rterm_val << 12) | rx_info.aml_phy.rterm_flag << 2);
	}
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, data32);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, 0x0873000);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, 0x53c22333);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_DFE, 0x5409f843);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x040050d3);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x30211050);
	usleep_range(10, 20);

	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, A9_DCH_RSTN, 0x7);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, A9_SQ_RSTN, 0x1);

	/* PLL */
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0500a700);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x21415ea5);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0500a701);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0500a703);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0500a707);
	usleep_range(100, 200);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_DCHA_DFE, 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_FR_EN, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_RSTB, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_LKDET_EN, 0x0);
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_RSTB, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_RSTB, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_DFE_OFST_CAL_START, 1);
	usleep_range(100, 200);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_OFSET_CAL_START, 0x1);
	usleep_range(100, 200);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_RSTB, 0x1);
	/* ofset stop */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_OFSET_CAL_START, 0x0);
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_DFE_OFST_CAL_START, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_DCHA_DFE, 0x1f);
	/* turn off pll*/
	hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x0);
	hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x0);
	if (log_level & PHY_LOG)
		rx_pr("ofst cal\n");
}

/* hardware eye monitor */
void aml_eq_eye_monitor_a9(void)
{
	u32 data32;
	u32 eye_height0, eye_height1, eye_height2;
	u32 eom_done0, eom_done1, eom_done2;

	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_HOLD_EN, 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, _BIT(18), 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EHM_HW_SCAN_EN, 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x1);
	usleep_range(5000, 5500);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	eye_height0 = data32 & 0x3f;
	eye_height1 = (data32 >> 8) & 0x3f;
	eye_height2 = (data32 >> 16) & 0x3f;
	eom_done0 = hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHD_STAT, _BIT(6));
	eom_done1 = hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHD_STAT, _BIT(14));
	eom_done2 = hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHD_STAT, _BIT(22));
	rx_pr("eom_ber < 4e-7\n");
	rx_pr("eom_done=[%d, %d, %d]\n", eom_done0, eom_done1, eom_done2);
	rx_pr("eom_eh = [%d, %d, %d]\n", eye_height0, eye_height1, eye_height2);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EHM_HW_SCAN_EN, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, _BIT(18), 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_HOLD_EN, 0x0);
}

void get_eq_val_a9(void)
{
	u32 data32 = 0;
	u32 eq_boost0, eq_boost1, eq_boost2;

	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	eq_boost0 = data32 & 0x1f;
	eq_boost1 = (data32 >> 8)  & 0x1f;
	eq_boost2 = (data32 >> 16)      & 0x1f;
	if (log_level & PHY_LOG)
		rx_pr("eq:%d-%d-%d\n", eq_boost0, eq_boost1, eq_boost2);
}

void dump_cdr_info_a9(void)
{
	u32 cdr0_lock, cdr1_lock, cdr2_lock;
	u32 cdr0_int, cdr1_int, cdr2_int;
	u32 cdr0_code, cdr1_code, cdr2_code;
	u32 data32;

	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x22);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_MUX_CDR_DBG_SEL, 0x0);
	usleep_range(10, 20);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(10, 20);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	cdr0_int = data32 & 0x7f;
	cdr1_int = (data32 >> 8) & 0x7f;
	cdr2_int = (data32 >> 16) & 0x7f;
	rx_pr("cdr_code=[%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code);
	rx_pr("cdr_lock=[%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock);
	comb_val_a9(get_val_a9, "cdr_int", cdr0_int, cdr1_int, cdr2_int, 7);
}

void aml_eq_cfg_a9(void)
{
	u8 port = rx_info.main_port;

	/* don't need to run eq if no sqo_clk or pll not lock */
	if (!is_clk_stable(port))
		return;
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_FR_EN, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_RSTB, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_LKDET_EN, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_MODE, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_ERR_MODE, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_RSTB, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_RSTB, 0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_HOLD_EN, 0);
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_RSTB, 1);
	if (rx_info.aml_phy.eq_sslms_en) {
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_HOLD_EN, 0x1);
		usleep_range(10, 20);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_RSTB, 0x1);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_ERR_MODE, 0x2);
	}
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_RSTB, 1);
	if (rx_info.aml_phy.cdr_fr_en) {
		udelay(rx_info.aml_phy.cdr_fr_en);
		/*cdr fr en*/
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_FR_EN, 1);
	}
	usleep_range(10000, 10100);
	aml_eq_retry_a9();
	aml_dfe_en_a9();
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_LKDET_EN, 1);
}

void aml_phy_get_trim_val_a9(void)
{
	if (rx_info.aml_phy.rterm_dbg_lvl)
		rx_info.aml_phy.rterm_dts_lvl =
			rx_info.aml_phy.rterm_dbg_lvl;
	if (rx_info.aml_phy.rterm_dts_lvl > 15)
		rx_info.aml_phy.rterm_dts_lvl = 15;
	rx_info.aml_phy.rterm_val = a9_rlevel[rx_info.aml_phy.rterm_dts_lvl];
}

void aml_phy_cfg_a9(u32 cable_clk)
{
	u32 data32 = 0;
	u32 clk_rate = 0;
	u8 port = rx_info.main_port;
	u32 idx = 0;

	if (rx_info.aml_phy.pre_int) {
		if (rx_info.aml_phy.ofst_en) {
			aml_phy_offset_cal_a9();
			usleep_range(1000, 1100);
		}
		clk_rate = rx_get_scdc_clkrate_sts(port);
		idx = aml_cable_clk_band(cable_clk, clk_rate, port);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_OFSET_CAL_START, 0x0);
		usleep_range(100, 110);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_DFE_OFST_CAL_START, 0x0);
		hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_DCHA_DFE, 0x1f);

		hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, phy_dcha_a9[idx][0]);
		hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_DFE, phy_dcha_a9[idx][1]);
		hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, phy_dchd_a9[idx][0]);
		hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, phy_dchd_a9[idx][1]);
		aml_phy_get_trim_val_a9();
		data32 = phy_misc_a9[idx][0];
		if (rx_info.aml_phy.phy_debug_en &&
			rx_info.aml_phy.misc1_value)
			data32 = rx_info.aml_phy.misc1_value;
		if (rx_info.aml_phy.rterm_flag) {
			data32 = ((data32 & (~((0xf << 12) | 0x4))) |
				(rx_info.aml_phy.rterm_val << 12) |
				rx_info.aml_phy.rterm_flag << 2);
		}
		hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, data32);
		data32 = phy_misc_a9[idx][1];

		/* port switch */
		data32 &= (~(0xf << 28));
		data32 |= (0xf << 28);
		data32 &= (~(0xf << 24));
		data32 |= ((1 << rx_info.main_port) << 24);
		hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, data32);
		if (!rx_info.aml_phy.pre_int_en)
			rx_info.aml_phy.pre_int = 0;
	}
	//rterm_en(dcha_misc0[22]) = 0x1
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, RTERM_EN_SEL,
		(0xf & (~rx_info.disable_port_num)));
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_FR_EN, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_RSTB, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_LKDET_EN, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_MODE, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_EQ_RSTB, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_RSTB, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_DFE_HOLD_EN, 0x0);
	if (log_level & PHY_LOG)
		rx_pr("phy_reset\n");
}

 /* For A9 */
void aml_phy_init_a9(void)
{
	u8 port = rx_info.main_port;
	u32 cable_clk = rx[port].clk.cable_clk;

	if (!rx_get_hdmi5v_sts())
		return;
	aml_phy_cfg_a9(cable_clk);
	usleep_range(10, 20);
	aml_pll_bw_cfg_a9(cable_clk);
	usleep_range(10, 20);
	aml_eq_cfg_a9();
}

void dump_reg_phy_a9(void)
{
	rx_pr("PLL Register:\n");
	rx_pr("RG_RX20PLL_0-0x0=0x%x\n", hdmirx_rd_amlphy(A9_RG_RX20PLL_0));
	rx_pr("RG_RX20PLL_1-0x4=0x%x\n", hdmirx_rd_amlphy(A9_RG_RX20PLL_1));
	rx_pr("PHY Register:\n");
	rx_pr("dchd_eq-0x14=0x%x\n", hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_EQ));
	rx_pr("dchd_cdr-0x10=0x%x\n", hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_CDR));
	rx_pr("dcha_dfe-0xc=0x%x\n", hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHA_DFE));
	rx_pr("dcha_afe-0x8=0x%x\n", hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHA_AFE));
	rx_pr("misc2-0x1c=0x%x\n", hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHA_MISC2));
	rx_pr("misc1-0x18=0x%x\n", hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHA_MISC1));
}

/*
 * rx phy v2 debug
 */
int count_one_bits_a9(u32 value)
{
	int count = 0;

	for (; value != 0; value >>= 1) {
		if (value & 1)
			count++;
	}
	return count;
}

void get_val_a9(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
}

void get_flag_val_a9(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
}

void comb_val_a9(void (*p)(char *, unsigned int, int),
					char *type, unsigned int val_0, unsigned int val_1,
					unsigned int val_2, int len)
{
	char out[32], v0_buf[16], v1_buf[16], v2_buf[16];
	int pos = 0;

	p(v0_buf, val_0, len);
	p(v1_buf, val_1, len);
	p(v2_buf, val_2, len);
	pos += snprintf(out + pos, 32 - pos, "%s[", type);
	pos += snprintf(out + pos, 32 - pos, " %s,", v0_buf);
	pos += snprintf(out + pos, 32 - pos, " %s,", v1_buf);
	pos += snprintf(out + pos, 32 - pos, " %s]", v2_buf);
	rx_pr("%s\n", out);
}

void dump_aml_phy_sts_a9(void)
{
	u8 port = rx_info.main_port;
	u32 data32;
	u32 terminal;
	u32 ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1;
	u32 ch0_eq_err, ch1_eq_err, ch2_eq_err;
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0;
	u32 dfe0_tap1, dfe1_tap1, dfe2_tap1, dfe3_tap1;
	u32 dfe0_tap2, dfe1_tap2, dfe2_tap2, dfe3_tap2;
	u32 dfe0_tap3, dfe1_tap3, dfe2_tap3, dfe3_tap3;
	u32 dfe0_tap4, dfe1_tap4, dfe2_tap4, dfe3_tap4;

	u32 cdr0_lock, cdr1_lock, cdr2_lock;
	u32 cdr0_int, cdr1_int, cdr2_int;
	u32 cdr0_code, cdr1_code, cdr2_code;

	bool pll_lock;
	bool squelch;

	u32 sli0_ofst0, sli1_ofst0, sli2_ofst0;
	u32 sli0_ofst1, sli1_ofst1, sli2_ofst1;
	u32 sli0_ofst2, sli1_ofst2, sli2_ofst2;

	/* rterm */
	terminal = (hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, A9_RTERM_CNTL));

	/* eq_boost1 status */
	/* mux_eye_en */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	/* mux_block_sel */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	ch0_eq_boost1 = data32 & 0x1f;
	ch0_eq_err = (data32 >> 5) & 0x3;
	ch1_eq_boost1 = (data32 >> 8) & 0x1f;
	ch1_eq_err = (data32 >> 13) & 0x3;
	ch2_eq_boost1 = (data32 >> 16) & 0x1f;
	ch2_eq_err = (data32 >> 21) & 0x3;

	/* dfe tap0 sts */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap0 = graytodecimal_a9(data32 & 0x7f);
	dfe1_tap0 = graytodecimal_a9((data32 >> 8) & 0x7f);
	dfe2_tap0 = graytodecimal_a9((data32 >> 16) & 0x7f);
	dfe3_tap0 = graytodecimal_a9((data32 >> 24) & 0x7f);
	/* dfe tap1 sts */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap1 = graytodecimal_a9(data32 & 0x3f);
	dfe1_tap1 = graytodecimal_a9((data32 >> 8) & 0x3f);
	dfe2_tap1 = graytodecimal_a9((data32 >> 16) & 0x3f);
	dfe3_tap1 = graytodecimal_a9((data32 >> 24) & 0x3f);
	/* dfe tap2 sts */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x2);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap2 = data32 & 0x1f;
	dfe1_tap2 = (data32 >> 8) & 0x1f;
	dfe2_tap2 = (data32 >> 16) & 0x1f;
	dfe3_tap2 = (data32 >> 24) & 0x1f;
	/* dfe tap3 sts */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x3);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap3 = graytodecimal_a9(data32 & 0xf);
	dfe1_tap3 = graytodecimal_a9((data32 >> 8) & 0xf);
	dfe2_tap3 = graytodecimal_a9((data32 >> 16) & 0xf);
	dfe3_tap3 = graytodecimal_a9((data32 >> 24) & 0xf);
	/* dfe tap4 sts */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x4);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	dfe0_tap4 = graytodecimal_a9(data32 & 0xf);
	dfe1_tap4 = graytodecimal_a9((data32 >> 8) & 0xf);
	dfe2_tap4 = graytodecimal_a9((data32 >> 16) & 0xf);
	dfe3_tap4 = graytodecimal_a9((data32 >> 24) & 0xf);

	/* CDR status */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x22);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_MUX_CDR_DBG_SEL, 0x0);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	cdr0_int = data32 & 0x7f;
	cdr1_int = (data32 >> 8) & 0x7f;
	cdr2_int = (data32 >> 16) & 0x7f;

	/* pll lock */
	pll_lock = hdmirx_rd_amlphy(A9_RG_RX20PLL_0) >> 31;

	/* squelch */
	squelch = hdmirx_rd_top(TOP_MISC_STAT0, port) & 0x1;

	/* slicer offset status */
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	sli0_ofst0 = data32 & 0x1f;
	sli1_ofst0 = (data32 >> 8) & 0x1f;
	sli2_ofst0 = (data32 >> 16) & 0x1f;

	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	sli0_ofst1 = data32 & 0x1f;
	sli1_ofst1 = (data32 >> 8) & 0x1f;
	sli2_ofst1 = (data32 >> 16) & 0x1f;

	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_EHM_DBG_SEL, 0x0);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_EQ, A9_STATUS_MUX_SEL, 0x1);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_DFE_OFST_DBG_SEL, 0x2);
	hdmirx_wr_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_MUX_CDR_DBG_SEL, 0x1);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHD_STAT);
	sli0_ofst2 = data32 & 0x1f;
	sli1_ofst2 = (data32 >> 8) & 0x1f;
	sli2_ofst2 = (data32 >> 16) & 0x1f;

	rx_pr("\nhdmirx phy status:\n");
	rx_pr("pll_lock=%d, squelch=%d, terminal=%d\n", pll_lock, squelch, terminal);
	rx_pr("eq_boost1=[%d,%d,%d]\n",
	      ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1);
	rx_pr("eq_err=[%d,%d,%d]\n",
	      ch0_eq_err, ch1_eq_err, ch2_eq_err);

	comb_val_a9(get_val_a9, "dfe_tap0", dfe0_tap0, dfe1_tap0, dfe2_tap0, 7);
	comb_val_a9(get_val_a9, "dfe_tap1", dfe0_tap1, dfe1_tap1, dfe2_tap1, 6);
	comb_val_a9(get_val_a9, "dfe_tap2", dfe0_tap2, dfe1_tap2, dfe2_tap2, 5);
	comb_val_a9(get_val_a9, "dfe_tap3", dfe0_tap3, dfe1_tap3, dfe2_tap3, 4);
	comb_val_a9(get_val_a9, "dfe_tap4", dfe0_tap4, dfe1_tap4, dfe2_tap4, 4);

	comb_val_a9(get_val_a9, "slicer_ofst0", sli0_ofst0, sli1_ofst0, sli2_ofst0, 5);
	comb_val_a9(get_val_a9, "slicer_ofst1", sli0_ofst1, sli1_ofst1, sli2_ofst1, 5);
	comb_val_a9(get_val_a9, "slicer_ofst2", sli0_ofst2, sli1_ofst2, sli2_ofst2, 5);

	rx_pr("cdr_code=[%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code);
	rx_pr("cdr_lock=[%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock);
	comb_val_a9(get_val_a9, "cdr_int", cdr0_int, cdr1_int, cdr2_int, 7);
}

bool aml_get_tmds_valid_a9(void)
{
	u32 tmdsclk_valid;
	u32 sqofclk;
	u32 tmds_align;
	u32 ret;
	u8 port = rx_info.main_port;

	/* digital tmds valid depends on PLL lock from analog phy. */
	/* it is not necessary and T7 has not it */
	/* tmds_valid = hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01; */
	sqofclk = 1;
	tmdsclk_valid = is_tmds_clk_stable(port);
	/* modified in T7, 0x2b bit'0 tmds_align status */
	tmds_align = hdmirx_rd_top(TOP_TMDS_ALIGN_STAT, port) & 0x01;
	if (sqofclk && tmdsclk_valid && tmds_align) {
		ret = 1;
	} else {
		if (log_level & VIDEO_LOG) {
			rx_pr("sqo:%x,tmdsclk_valid:%x,align:%x\n",
			      sqofclk, tmdsclk_valid, tmds_align);
			rx_pr("cable clk0:%d\n", rx[port].clk.cable_clk);
			//rx_pr("cable clk1:%d\n", rx_get_clock(TOP_HDMI_CABLECLK, port));
		}
		ret = 0;
	}
	return ret;
}

void hdmirx_rd_check_top_a9(u32 addr, u32 exp_data, u32 mask)
{
	u32 rd_data;

	rd_data = hdmirx_rd_top(addr, rx_info.main_port);
	rx_pr("check_top=0x%x\n", rd_data);
	if ((rd_data | mask) != (exp_data | mask))
		rx_pr("top reg 0x%x,rd_data=0x%x, exp_data=0x%x mask=0x%x\n",
		addr, rd_data, exp_data, mask);
}

int aml_phy_short_bist_a9(void)
{
	u32 data32;
	u8 port = rx_info.main_port;
	int ret = 0;
	int cnt = 0;

	//initialize pll/phy registers
	hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL0, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL1, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_DFE, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, 0x0);
	//set registers to default
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x30211050);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x04005053);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, 0x11873000);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0xfb320000);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_DFE, 0x040bfc89);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, 0x01811666);

	do {
		hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL0, 0x010109f4);
		hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL1, 0x12301865);
		hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL0, 0x110109f4);
		usleep_range(100, 120);
		hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL0, 0x510109f4);
		usleep_range(100, 120);
		hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL0, 0x711109f4);
		usleep_range(100, 120);
		cnt++;
		if (cnt > 5000)
			return 0;
	} while (!hdmirx_rd_bits_amlphy(A9_HDMIRX20PLL_CTRL0, _BIT(31)));

	//phy config
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x30233050);
	usleep_range(100, 110);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0xff320000);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0xff320070);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0xfb320070);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0xff320070);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, 0x11873008);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x04005053);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x04007053);
	usleep_range(100, 110);

	data32 = 0;
	data32 |= (1 << 4);//[4]valid_always
	data32 |= (7 << 0);//[3:0]decoup_thresh
	hdmirx_wr_top(TOP_CHAN_SWITCH_1, data32, port);

	data32 = 0;
	data32 |= (2 << 28);//[29:28]source_2
	data32 |= (1 << 26);//[27:26]source_1
	data32 |= (0 << 24);//[25:24]source_0
	data32 |= (0 << 20);//[22:20]skew_2
	data32 |= (0 << 16);//[18:16]skew_1
	data32 |= (0 << 12);//[14:12]skew_0
	data32 |= (0 << 10);//[10]bitswap_2
	data32 |= (0 << 9);//[9]bitswap_1
	data32 |= (0 << 8);//[8]bitswap_0
	data32 |= (0 << 6);//[6]polarity_2
	data32 |= (0 << 5);//[5]polarity_1
	data32 |= (0 << 4);//[4]polarity_0
	data32 |= (0 << 3);//[3]charswap_2
	data32 |= (0 << 2);//[2]charswap_1
	data32 |= (0 << 1);//[1]charswap_0
	data32 |= (0 << 0);//[0]enable
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);
	data32 |= (1 << 0);// [0]        enable
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);

	// Ccnfigure PRBS generator
	data32 = 0;
	data32 |= (0 << 16);//[16]inj_prbs_err
	data32 |= (0 << 10);//[14:10]pttn_max
	data32 |= (0 << 9);//[9]pttn_gen_enable
	data32 |= (0 << 8);//[8]bist_loopback
	data32 |= (3 << 5);//[7:5]wrp_threshold
	data32 |= (3 << 2);//[4:2]prbs_mode:3=prbs15
	data32 |= (0 << 0);//[0]prbs_gen_enable
	hdmirx_wr_top(TOP_PRBS_GEN, data32, port);

	// Ccnfigure PRBS analyzer: PRBS15
	data32 = 0;
	data32 |= (3 << 28);//[30:28] prbs_mode:3=prbs15
	data32 |= (0 << 25);//[25]prbs_ana_resync_ch3
	data32 |= (0 << 24);//[24]prbs_ana_enable_ch3
	data32 |= (3 << 20);//[22:20]prbs_mode:3=prbs15
	data32 |= (0 << 17);//[17]prbs_ana_resync_ch2
	data32 |= (0 << 16);//[16]prbs_ana_enable_ch2
	data32 |= (3 << 12);//[14:12]prbs_mode3=prbs15
	data32 |= (0 << 9);//[9]prbs_ana_resync_ch1
	data32 |= (0 << 8);//[8]prbs_ana_enable_ch1
	data32 |= (3 << 4);//[6:4]prbs_mode:3=prbs15
	data32 |= (0 << 1);//[1]prbs_ana_resync_ch0
	data32 |= (0 << 0);//[0]prbs_ana_enable_ch0
	hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);
	data32 |= (1 << 24);// [24]prbs_ana_enable_ch3
	data32 |= (1 << 16);// [16]prbs_ana_enable_ch2
	data32 |= (1 << 8);// [8]prbs_ana_enable_ch1
	data32 |= (1 << 0);// [0]prbs_ana_enable_ch0
	hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);

	// Start PRBS
	rx_pr("Start PRBS\n");
	data32 = 0;
	data32 |= (0 << 16);//[16]inj_prbs_err
	data32 |= (0 << 10);//[14:10]pttn_max
	data32 |= (0 << 9);//[9]pttn_gen_enable
	data32 |= (0 << 8);//[8]bist_loopback
	data32 |= (3 << 5);//[7:5]wrp_threshold
	data32 |= (3 << 2);//[4:2] prbs_mode:3=prbs15
	data32 |= (1 << 0);//[0] prbs_gen_enable
	hdmirx_wr_top(TOP_PRBS_GEN, data32, port);
	// wait 5ms
	usleep_range(5000, 5100);

	data32 = 0;
	data32 |= (1 << 7);
	data32 |= (0 << 6);
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	//hdmirx_rd_check_top_t6d(TOP_PRBS_ANA_STAT, data32, 0);
	if ((hdmirx_rd_top(TOP_PRBS_ANA_STAT, port) & 0x15) == 0x15) {
		ret = 1;
		rx_pr("bist pass\n");
	} else {
		ret = 0;
		rx_pr("bist fail\n");
		hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH0, 0, 0);
		hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH1, 0, 0);
		hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH2, 0, 0);
	}
	return ret;
}

void aml_phy_exbist_a9(u8 port, u8 ch)
{
	u32 data32, tmp;
	int rst_pll_cnt = 0;

	sm_pause = 1;
	//initialize pll/phy registers
	//initialize pll/phy registers
	hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL0, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL1, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_DFE, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, 0x0);
	//set registers to default
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x30211050);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x04005013);
	data32 = 0xf0873000;
	data32 |= ((1 << port) << 24);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, data32);//port_sel
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0xfb320000);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_DFE, 0x040bf885);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, 0x01811777);
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0xff320070);//enable channel

	do {
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x01290850);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_1, 0x12301865);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x11290850);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x51290850);
		usleep_range(10, 20);
		hdmirx_wr_amlphy(A9_RG_RX20PLL_0, 0x71290850);
		if (rst_pll_cnt++ > pll_rst_max) {
			if (log_level & VIDEO_LOG)
				rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG) {
			rx_pr("sq=%d,pll_lock=%d",
			      hdmirx_rd_top(TOP_MISC_STAT0, port) & 0x1,
			      is_pll_lock_a9());
		}
	} while (!is_tmds_clk_stable(port) && is_clk_stable(port) && !is_pll_lock_a9());

	//cdr eq dfe enable
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x04007013);//enable cdr
	usleep_range(100, 110);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x04007053);//enable cdr fr
	usleep_range(10, 20);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x30213050);//enable eq
	usleep_range(2000, 2010);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x30213350);//hold eq
	usleep_range(100, 110);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x30233350);//enable dfe
	usleep_range(1000, 1010);

	//dump phy status
	dump_aml_phy_sts_a9();

	data32 = 0;
	data32 |= (1 << 4);//[4]valid_always
	data32 |= (7 << 0);//[3:0]decoup_thresh
	hdmirx_wr_top(TOP_CHAN_SWITCH_1, data32, port);

	data32 = 0;
	data32 |= (2 << 28);//[29:28]source_2
	data32 |= (1 << 26);//[27:26]source_1
	data32 |= (0 << 24);//[25:24]source_0
	data32 |= (0 << 20);//[22:20]skew_2
	data32 |= (0 << 16);//[18:16]skew_1
	data32 |= (0 << 12);//[14:12]skew_0
	data32 |= (0 << 10);//[10]bitswap_2
	data32 |= (0 << 9);//[9]bitswap_1
	data32 |= (0 << 8);//[8]bitswap_0
	data32 |= (0 << 6);//[6]polarity_2
	data32 |= (0 << 5);//[5]polarity_1
	data32 |= (0 << 4);//[4]polarity_0
	data32 |= (0 << 3);//[3]charswap_2
	data32 |= (0 << 2);//[2]charswap_1
	data32 |= (0 << 1);//[1]charswap_0
	data32 |= (0 << 0);//[0]enable
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);
	data32 |= (1 << 0);// [0]        enable
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);

	// Ccnfigure PRBS generator
	data32 = 0;
	data32 |= (0 << 16);//[16]inj_prbs_err
	data32 |= (0 << 10);//[14:10]pttn_max
	data32 |= (0 << 9);//[9]pttn_gen_enable
	data32 |= (0 << 8);//[8]bist_loopback
	data32 |= (3 << 5);//[7:5]wrp_threshold
	data32 |= (3 << 2);//[4:2]prbs_mode:3=prbs15
	data32 |= (0 << 0);//[0]prbs_gen_enable
	hdmirx_wr_top(TOP_PRBS_GEN, data32, port);

	// Ccnfigure PRBS analyzer: PRBS15
	data32 = 0;
	data32 |= (3 << 28);//[30:28] prbs_mode:3=prbs15
	data32 |= (0 << 25);//[25]prbs_ana_resync_ch3
	data32 |= (0 << 24);//[24]prbs_ana_enable_ch3
	data32 |= (3 << 20);//[22:20]prbs_mode:3=prbs15
	data32 |= (0 << 17);//[17]prbs_ana_resync_ch2
	data32 |= (0 << 16);//[16]prbs_ana_enable_ch2
	data32 |= (3 << 12);//[14:12]prbs_mode3=prbs15
	data32 |= (0 << 9);//[9]prbs_ana_resync_ch1
	data32 |= (0 << 8);//[8]prbs_ana_enable_ch1
	data32 |= (3 << 4);//[6:4]prbs_mode:3=prbs15
	data32 |= (0 << 1);//[1]prbs_ana_resync_ch0
	data32 |= (0 << 0);//[0]prbs_ana_enable_ch0
	hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);
	data32 |= (1 << 24);// [24]prbs_ana_enable_ch3
	data32 |= (1 << 16);// [16]prbs_ana_enable_ch2
	data32 |= (1 << 8);// [8]prbs_ana_enable_ch1
	data32 |= (1 << 0);// [0]prbs_ana_enable_ch0
	hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);

	// Start PRBS
	rx_pr("Start PRBS\n");
	data32 = 0;
	data32 |= (0 << 16);//[16]inj_prbs_err
	data32 |= (0 << 10);//[14:10]pttn_max
	data32 |= (0 << 9);//[9]pttn_gen_enable
	data32 |= (0 << 8);//[8]bist_loopback
	data32 |= (3 << 5);//[7:5]wrp_threshold
	data32 |= (3 << 2);//[4:2] prbs_mode:3=prbs15
	data32 |= (1 << 0);//[0] prbs_gen_enable
	hdmirx_wr_top(TOP_PRBS_GEN, data32, port);
	// wait 5ms
	usleep_range(5000, 5100);

	data32 = 0;
	data32 |= (1 << 7);
	data32 |= (0 << 6);
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	//hdmirx_rd_check_top_a9(TOP_PRBS_ANA_STAT, data32, 0);
	tmp = hdmirx_rd_top(TOP_PRBS_ANA_STAT, port);
	if (ch == E_CH0) {
		if ((tmp & 0x1) == 0x1) {
			rx_pr("ch0 pass\n");
		} else {
			rx_pr("ch0 exbist fail\n");
			hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH0, 0, 0);
		}
	}
	if (ch == E_CH1) {
		if ((tmp & 0x4) == 0x4) {
			rx_pr("ch1 pass\n");
		} else {
			rx_pr("ch1 exbist fail\n");
			hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH1, 0, 0);
		}
	}
	if (ch == E_CH2) {
		if ((tmp & 0x10) == 0x10) {
			rx_pr("ch2 pass\n");
		} else {
			rx_pr("ch2 exbist fail\n");
			hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH2, 0, 0);
		}
	}
	if (ch == E_ALL_CH) {
		if ((tmp & 0x15) == 0x15) {
			rx_pr("exbist pass\n");
		} else {
			rx_pr("exbist fail\n");
			hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH0, 0, 0);
			hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH1, 0, 0);
			hdmirx_rd_check_top_a9(TOP_PRBS_ANA_BER_CH2, 0, 0);		}
	}
}

void aml_phy_power_off_a9(void)
{
	hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL0, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PLL_CTRL1, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_AFE, 0x00800000);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_DFE, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_CDR, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHD_EQ, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC1, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX_ARC_CNTL, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX_PHY_PROD_TEST0, 0x0);
	hdmirx_wr_amlphy(A9_HDMIRX_PHY_PROD_TEST1, 0x0);
}

bool rx_is_power_off_a9(void)
{
	return hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHA_MISC1) == 0;
}

void aml_phy_switch_port_a9(void)
{
	u32 data32;
	u8 port = rx_info.main_port;

	/* reset and select data port */
	rx_i2c_mux_cfg(port);
	data32 = hdmirx_rd_amlphy(A9_HDMIRX20PHY_DCHA_MISC2);
	data32 &= (~(0xf << 24));
	data32 |= ((1 << port) << 24);
	hdmirx_wr_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, data32);
	hdmirx_wr_bits_top(TOP_PORT_SEL, MSK(4, 0), (1 << port), port);
	hdmirx_wr_top_common(HDMIRX_TOP_FSW_CNTL, 0x101);
	hdmirx_wr_top_common(HDMIRX_TOP_FSW_CLK_CNTL, 1);
}

void dump_vsi_reg_a9(u8 port)
{
	u8 data8, i;

	rx_pr("vsi data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(VSIRX_TYPE_DP3_IVCRX + i, port);
		rx_pr("%d-[%x]\n", i, data8);
	}
	rx_pr("hf-vsi data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(HF_VSIRX_TYPE_DP3_IVCRX + i, port);
		rx_pr("%d-[%x]\n", i, data8);
	}
	rx_pr("aif-vsi data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(AUDRX_TYPE_DP2_IVCRX + i, port);
		rx_pr("%d-[%x]\n", i, data8);
	}
	rx_pr("unrec data:\n");
	for (i = 0; i <= 30; i++) {
		data8 = hdmirx_rd_cor(RX_UNREC_BYTE1_DP2_IVCRX + i, port);
		rx_pr("%d-[%x]\n", i, data8);
	}
}

unsigned int rx_sec_hdcp_cfg_a9(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(HDMI_RX_HDCP_CFG, 0, 0, 0, 0, 0, 0, 0, &res);

	return (unsigned int)((res.a0) & 0xffffffff);
}

/*
 * 0 SPDIF
 * 1  I2S
 * 2  TDM
 */
void rx_set_aud_output_a9(u32 param)
{
	u8 port = rx_info.main_port;

	if (param == 2) {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x0f, port);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0xff, port);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x90, port); //TDM
	} else if (param == 1) {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x00, port);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0x10, port);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80, port); //I2S
	} else {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x00, port);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0x10, port);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80, port); //SPDIF
	}
}

void rx_sw_reset_a9(int level)
{
	u8 port = rx_info.main_port;

	/* deep color fifo */
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 1, port);
	udelay(1);
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 0, port);
}

void hdcp_init_a9(void)
{
	u8 port = rx_info.main_port;
	u8 data8;
	//key config and crc check
	//rx_sec_hdcp_cfg_a9();
	//hdcp config

	//======================================
	// HDCP 2.X Config ---- RX
	//======================================
	hdmirx_wr_cor(RX_HPD_C_CTRL_AON_IVCRX, 0x1, port);//HPD
	hdmirx_wr_cor(SCDCS_100MS_IN_1MS_CNT_SCDC_IVCRX, 0x1, port);
	//todo: enable hdcp22 according hdcp burning
	hdmirx_wr_cor(RX_HDCP2x_CTRL_PWD_IVCRX, 0x01, port);//ri_hdcp2x_en
	//hdmirx_wr_cor(RX_INTR13_MASK_PWD_IVCRX, 0x02, port);// irq
	hdmirx_wr_cor(PWD_SW_CLMP_AUE_OIF_PWD_IVCRX, 0x0, port);

	data8 = 0;
	data8 |= (hdmirx_repeat_support() && rx[port].hdcp.repeat) << 1;
	hdmirx_wr_cor(CP2PAX_CTRL_0_HDCP2X_IVCRX, data8, port);
	//depth
	hdmirx_wr_cor(CP2PAX_RPT_DEPTH_HDCP2X_IVCRX, 0, port);
	//dev cnt
	hdmirx_wr_cor(CP2PAX_RPT_DEVCNT_HDCP2X_IVCRX, 0, port);
	//
	data8 = 0;
	data8 |= 0 << 0; //hdcp1dev
	data8 |= 0 << 1; //hdcp1dev
	data8 |= 0 << 2; //max_casc
	data8 |= 0 << 3; //max_devs
	hdmirx_wr_cor(CP2PAX_RPT_DETAIL_HDCP2X_IVCRX, data8, port);

	hdmirx_wr_cor(CP2PAX_RX_CTRL_0_HDCP2X_IVCRX, 0x83, port);
	hdmirx_wr_cor(CP2PAX_RX_CTRL_0_HDCP2X_IVCRX, 0x80, port);

	//======================================
	// HDCP 1.X Config ---- RX
	//======================================
	hdmirx_wr_cor(RX_SYS_SWTCHC_AON_IVCRX, 0x86, port);//SYS_SWTCHC,Enable HDCP DDC,SCDC DDC

	//----clear ksv fifo rdy --------
	data8  =  0;
	data8 |= (1 << 3);//bit[  3] reg_hdmi_clr_en
	data8 |= (7 << 0);//bit[2:0] reg_fifordy_clr_en
	hdmirx_wr_cor(RX_RPT_RDY_CTRL_PWD_IVCRX, data8, port);//register address: 0x1010 (0x0f)

	//----BCAPS config-----
	data8 = 0;
	data8 |= (0 << 4);//bit[4] reg_fast I2C transfers speed.
	data8 |= (0 << 5);//bit[5] reg_fifo_rdy
	data8 |= ((hdmirx_repeat_support() &&
		rx[port].hdcp.repeat) << 6);//bit[6] reg_repeater
	data8 |= (1 << 7);//bit[7] reg_hdmi_capable  HDMI capable
	hdmirx_wr_cor(RX_BCAPS_SET_HDCP1X_IVCRX, data8, port);//register address: 0x169e (0x80)

	//for (data8 = 0; data8 < 10; data8++) //ksv list number
		//hdmirx_wr_cor(RX_KSV_FIFO_HDCP1X_IVCRX, ksvlist[data8], port);

	//----Bstatus1 config-----
	data8 =  0;
	// data8 |= (2 << 0); //bit[6:0] reg_dev_cnt
	data8 |= (0 << 7);//bit[  7] reg_dve_exceed
	hdmirx_wr_cor(RX_SHD_BSTATUS1_HDCP1X_IVCRX, data8, port);//register address: 0x169f (0x00)

		//----Bstatus2 config-----
	data8 =  0;
	// data8 |= (2 << 0);//bit[2:0] reg_depth
	data8 |= (0 << 3);//bit[  3] reg_casc_exceed
	hdmirx_wr_cor(RX_SHD_BSTATUS2_HDCP1X_IVCRX, data8, port);//register address: 0x169f (0x00)

	//----Rx Sha length in bytes----
	hdmirx_wr_cor(RX_SHA_length1_HDCP1X_IVCRX, 0x0a, port);//[7:0] 10=2ksv*5byte
	hdmirx_wr_cor(RX_SHA_length2_HDCP1X_IVCRX, 0x00, port);//[9:8]

	//----Rx Sha repeater KSV fifo start addr----
	hdmirx_wr_cor(RX_KSV_SHA_start1_HDCP1X_IVCRX, 0x00, port);//[7:0]
	hdmirx_wr_cor(RX_KSV_SHA_start2_HDCP1X_IVCRX, 0x00, port);//[9:8]
	//hdmirx_wr_cor(CP2PAX_INTR0_MASK_HDCP2X_IVCRX, 0x3, port); irq
	//hdmirx_wr_cor(RX_HDCP2x_CTRL_PWD_IVCRX, 0x1, port); //same as L3309
	//hdmirx_wr_cor(CP2PA_TP1_HDCP2X_IVCRX, 0x9e, port);
	//hdmirx_wr_cor(CP2PA_TP3_HDCP2X_IVCRX, 0x32, port);
	//hdmirx_wr_cor(CP2PA_TP5_HDCP2X_IVCRX, 0x32, port);
	//hdmirx_wr_cor(CP2PAX_GP_IN1_HDCP2X_IVCRX, 0x2, port);
	//hdmirx_wr_cor(CP2PAX_GP_CTL_HDCP2X_IVCRX, 0xdb, port);
	hdmirx_wr_cor(RX_PWD_SRST2_PWD_IVCRX, 0x8, port);
	hdmirx_wr_cor(RX_PWD_SRST2_PWD_IVCRX, 0x2, port);
}

void clk_init_cor_a9(void)
{
	u32 data32;
	u8 port = rx_info.main_port;

	/* Turn on clk_hdmirx_pclk, also = sysclk */
	wr_reg_clk_ctl(CLKCTRL_SYS_CLK_EN0_REG2,
		       rd_reg_clk_ctl(CLKCTRL_SYS_CLK_EN0_REG2) | (1 << 9));

	data32	= 0;
	data32 |= (0 << 25);// [26:25] clk_sel for cts_hdmirx_2m_clk: 0=cts_oscin_clk
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_2m_clk
	data32 |= (11 << 16);// [22:16] clk_div for cts_hdmirx_2m_clk: 24/12=2M
	data32 |= (3 << 9);// [10: 9] clk_sel for cts_hdmirx_5m_clk: 3=fclk_div5
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_5m_clk
	data32 |= (79 << 0);// [ 6: 0] clk_div for cts_hdmirx_5m_clk: fclk_dvi5/80=400/80=5M
	wr_reg_clk_ctl(RX_CLK_CTRL, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_2m_clk
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_5m_clk
	wr_reg_clk_ctl(RX_CLK_CTRL, data32);

	data32  = 0;
	data32 |= (3 << 25);// [26:25] clk_sel for cts_hdmirx_hdcp2x_eclk: 3=fclk_div5
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_hdcp2x_eclk
	data32 |= (15 << 16);// [22:16] clk_div for cts_hdmirx_hdcp2x_eclk:
	//fclk_dvi5/16=400/16=25M
	data32 |= (3 << 9);// [10: 9] clk_sel for cts_hdmirx_cfg_clk: 3=fclk_div5
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_cfg_clk
	data32 |= (7 << 0);// [ 6: 0] clk_div for cts_hdmirx_cfg_clk: fclk_dvi5/8=400/8=50M
	wr_reg_clk_ctl(RX_CLK_CTRL1, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_hdcp2x_eclk
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_cfg_clk
	wr_reg_clk_ctl(RX_CLK_CTRL1, data32);

	data32  = 0;
	data32 |= (1 << 25);// [26:25] clk_sel for cts_hdmirx_acr_ref_clk: 1=fclk_div4
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_acr_ref_clk
	data32 |= (0 << 16);// [22:16] clk_div for cts_hdmirx_acr_ref_clk://fclk_div4/1=500M
	data32 |= (0 << 9);// [10: 9] clk_sel for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 0);// [ 6: 0] clk_div for cts_hdmirx_aud_pll_clk
	wr_reg_clk_ctl(RX_CLK_CTRL2, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_acr_ref_clk
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
	wr_reg_clk_ctl(RX_CLK_CTRL2, data32);

	data32  = 0;
	data32 |= (0 << 9);// [10: 9] clk_sel for cts_hdmirx_meter_clk: 0=cts_oscin_clk
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_meter_clk
	data32 |= (0 << 0);// [ 6: 0] clk_div for cts_hdmirx_meter_clk: 24M
	wr_reg_clk_ctl(RX_CLK_CTRL3, data32);
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_meter_clk
	wr_reg_clk_ctl(RX_CLK_CTRL3, data32);

	data32  = 0;
	data32 |= (0 << 31);// [31]	  free_clk_en
	data32 |= (0 << 15);// [15]	  hbr_spdif_en
	data32 |= (0 << 8);// [8]	  tmds_ch2_clk_inv
	data32 |= (0 << 7);// [7]	  tmds_ch1_clk_inv
	data32 |= (0 << 6);// [6]	  tmds_ch0_clk_inv
	data32 |= (0 << 5);// [5]	  pll4x_cfg
	data32 |= (0 << 4);// [4]	  force_pll4x
	data32 |= (0 << 3);// [3]	  phy_clk_inv
	hdmirx_wr_top(TOP_CLK_CNTL, data32, port);
}

void rx_dig_clk_en_a9(bool en)
{
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL, CLK_2M_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL, CLK_5M_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL1, HDCP2X_ECLK_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL1, CFG_CLK_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL3, METER_CLK_EN, en);
}

void rx_aud_pll_ctl_a9(bool en, u8 port)
{
	int tmp = 0;

	if (en) {
		/* analog pll */
		tmp = rd_reg_ana_ctl(ANACTRL_VDAC_CTRL0_A9);
		tmp |= (1 << 7);
		wr_reg_ana_ctl(ANACTRL_VDAC_CTRL0_A9, tmp);
		tmp = rd_reg_ana_ctl(ANACTRL_VDAC_CTRL0_A9);
		tmp |= (1 << 5);
		wr_reg_ana_ctl(ANACTRL_VDAC_CTRL0_A9, tmp);
		wr_bits_reg_ana_ctl(ANACTRL_VDAC_CTRL0_A9, _BIT(5), 0);
		wr_reg_ana_ctl(ANACTRL_VDAC_CTRL1_A9, 0);
		tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
		tmp |= (1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
		wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
		/* AUD_CLK=N/CTS*TMDS_CLK */
		wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL_A9, 0x20000000);
		wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL_A9, 0x60008000);
		wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL2_A9, 0x80b);
		wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL2_A9, 0x89b);
		if (rx[port].phy.pll_bw == PLL_BW_4)
			wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL2_A9, 0x90);
		/* cntl3 2:0 000=1*cts 001=2*cts 010=4*cts 011=8*cts */
		wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL3_A9,
			rx[port].phy.aud_div);
		if (log_level & AUDIO_LOG)
			rx_pr("aud div=%d\n",
				rd_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL3_A9));
		wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL_A9, 0x40004000);
		if (log_level & AUDIO_LOG)
			/* t3 audio pll lock bit: top reg acr_cntl_stat bit'31 */
			rx_pr("audio pll lock:0x%x\n",
				  (hdmirx_rd_top_common(TOP_ACR_CNTL_STAT) >> 31));
		rx_audio_pll_sw_update(port);
		//hdmirx_audio_fifo_rst(port);
	} else {
		/* disable pll, into reset mode */
		hdmirx_audio_disabled(port);
		wr_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL_A9, 0x0);
		tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
		tmp &= ~(1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
		wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
	}
}

void rx_i2c_mux_cfg_a9(u8 port)
{
	switch (port) {
	case E_PORT0:
		hdmirx_wr_top_common(TOP_PORT_SEL, 0x00084211);
		break;
	case E_PORT1:
		hdmirx_wr_top_common(TOP_PORT_SEL, 0x00084112);
		break;
	case E_PORT2:
		hdmirx_wr_top_common(TOP_PORT_SEL, 0x00081214);
		break;
	case E_PORT3:
		hdmirx_wr_top_common(TOP_PORT_SEL, 0x00014218);
		break;
	default:
		break;
	}
}

void rx_dump_pll_param_a9(void)
{
	u32 data32;
	u8 port = rx_info.main_port;
	int m_n_val = 0;
	int ctstemp = 0;
	int ctsa = 0;
	int cts_rate = 0;
	int n_rate = 0;
	u8 clk_rate = (hdmirx_rd_cor(SCDCS_TMDS_CONFIG_SCDC_IVCRX, port) >> 1) & 1;
	u8 aud_dds_ext_clkdiv2_en = (rd_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL_A9) >> 13) & 1;
	u8 aud_dds_tmds_flag = rd_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL3_A9) & 7;
	u32 aud_dds_dac_cntrl = rd_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL2_A9)  & 0xfff;
	u32 aud_dds_digit_reve = (rd_reg_ana_ctl(ANACTRL_AUD_PLL_CNTL3_A9) >> 16) & 0xffff;
	static struct aud_info_s aud_info_a;
	//u32 val0, val1;

	rx_get_aud_info(&aud_info_a, port);

	/* phy pll */
	rx_pr("cable clock = %d\n", rx[port].clk.cable_clk);
	rx_pr("tmds clock = %d\n", rx[port].clk.tmds_clk);
	rx_pr("clk rate = %s\n", clk_rate ? "40" : "10");
	if (!clk_rate) {
		if (rx[port].clk.tmds_clk >= 25.175 * MHz &&
			rx[port].clk.tmds_clk <= 33.75 * MHz)
			m_n_val = 160;
		else if (rx[port].clk.tmds_clk >= 37.7625 * MHz &&
			rx[port].clk.tmds_clk <= 74.25 * MHz)
			m_n_val = 80;
		else if (rx[port].clk.tmds_clk >= 74.5 * MHz &&
			rx[port].clk.tmds_clk <= 148.5 * MHz)
			m_n_val = 40;
		else if (rx[port].clk.tmds_clk >= 149 * MHz &&
			rx[port].clk.tmds_clk <= 297 * MHz)
			m_n_val = 20;
		else if (rx[port].clk.tmds_clk >= 300 * MHz &&
			rx[port].clk.tmds_clk <= 360 * MHz)
			m_n_val = 10;
	} else {
		m_n_val = 40;
	}
	rx_pr("M/N = %d\n", m_n_val);
	rx_pr("VCO=%d\n", rx[port].clk.cable_clk / MHz * m_n_val);
	data32 = hdmirx_rd_amlphy(A9_RG_RX20PLL_0);
	rx_pr("src clk=%s\n", ((data32 >> 19) & 1) ? "tmdsclk" : "osci");
	rx_pr("ref_div=%d\n", (data32 >> 11) & 0x1f);
	rx_pr("fb_div=%d\n", data32 & 0x1ff);
	rx_pr("en=%d\n", (data32 >> 19) & 1);
	rx_pr("od_1_5=%s\n", ((data32 >> 20) & 0x1) ? "div5" : "div1");
	if (((data32 >> 21) & 0x3) == 0)
		rx_pr("od_1_2_4_8=1\n");
	else if (((data32 >> 21) & 0x3) == 1)
		rx_pr("od_1_2_4_8=2\n");
	else if (((data32 >> 21) & 0x3) == 2)
		rx_pr("od_1_2_4_8=4\n");
	else if (((data32 >> 21) & 0x3) == 3)
		rx_pr("od_1_2_4_8=8\n");

	/* audio pll */
	rx_pr("aud_dds_hdmirx_n=%d\n", aud_info_a.n);
	rx_pr("aud_dds_hdmirx_cts=%d\n", aud_info_a.cts);
	rx_pr("acr clk=%d\n", aud_info_a.arc);
	rx_pr("aud_dds_digit_reve = 0x%x\n", aud_dds_digit_reve);
	rx_pr("aud_dds_dac_cntrl = 0x%x\n", aud_dds_dac_cntrl);
	rx_pr("aud_dds_tmds_flag = %d\n", aud_dds_tmds_flag);
	rx_pr("aud_dds_ext_clkdiv2_en = %d\n", aud_dds_ext_clkdiv2_en);
	if (aud_dds_ext_clkdiv2_en)
		n_rate = 2;
	else
		n_rate = 1;
	rx_pr("N_rate=%d, Na=%d\n", n_rate, aud_info_a.n * n_rate);
	if (aud_dds_tmds_flag == 0)
		ctstemp = aud_info_a.cts;
	else if (aud_dds_tmds_flag == 1)
		ctstemp = aud_info_a.cts * 2;
	else if (aud_dds_tmds_flag == 2)
		ctstemp = aud_info_a.cts * 4;
	else if (aud_dds_tmds_flag == 3)
		ctstemp = aud_info_a.cts * 8;

	aud_dds_dac_cntrl = (aud_dds_dac_cntrl >> 9) & 7;
	if (aud_dds_dac_cntrl == 0)
		cts_rate = 1;
	else if (aud_dds_dac_cntrl == 1)
		cts_rate = 40;
	else if (aud_dds_dac_cntrl == 2)
		cts_rate = 4;
	else if (aud_dds_dac_cntrl == 3)
		cts_rate = 8;
	else if (aud_dds_dac_cntrl == 4)
		cts_rate = 5;
	else if (aud_dds_dac_cntrl == 5)
		cts_rate = 10;
	else if (aud_dds_dac_cntrl == 6)
		cts_rate = 16;
	else if (aud_dds_dac_cntrl == 7)
		cts_rate = 20;
	ctsa = ctstemp * cts_rate;
	rx_pr("cts_rate=%d, CTSa=%d\n", cts_rate, ctsa);

	/* phy rate */
	rx_pr("cdr_os_rate=0x%x\n",
		hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_OS_RATE));
	rx_pr("cdr_ph_div=0x%x\n",
		hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_CDR_PH_DIV));
	rx_pr("pi_band_sel=0x%x\n",
		hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_PI_BAND_SEL));
	rx_pr("pi_div_sel=0x%x\n",
		hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHA_DFE, A9_PI_DIV_SEL));
	rx_pr("tmds_div_sel=0x%x\n",
		hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHD_CDR, A9_TMDS_DIV_SEL));
	rx_pr("pll_clk_sel=0x%x\n",
		hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, A9_PLL_CLK_SEL));
	rx_pr("cable_clk_sel=0x%x\n",
		hdmirx_rd_bits_amlphy(A9_HDMIRX20PHY_DCHA_MISC2, A9_CABLE_CLK_SEL));
}

