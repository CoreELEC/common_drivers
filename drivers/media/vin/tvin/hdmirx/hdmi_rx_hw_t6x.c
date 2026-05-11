// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
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
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/media/video_sink/video.h>

/* Local Include */
#include "hdmi_rx_repeater.h"
#include "hdmi_rx_drv.h"
#include "hdmi_rx_hw.h"
#include "hdmi_rx_wrapper.h"
#include "hdmi_rx_hw_t6x.h"
#include "hdmi_rx_frl.h"

int bist_lvl;
int bist_mode = 3;
int l_bist_en;
int frl_rate_id = FRL_12G_4LANE;
u32 top_irq_mask_t6x[IRQ_TYPE_CNT] = {
	_BIT(0),	//IRQ_AON_CTL = 0,
	0,		//IRQ_EDID_SLT,
	_BIT(1),	//IRQ_PWD_CTL,
	_BIT(2),	//IRQ_PHY,
	0,		//IRQ_5V_RISE0,
	0,		//IRQ_5V_RISE1,
	0,		//IRQ_5V_RISE2,
	0,		//IRQ_5V_RISE3,
	0,		//IRQ_5V_FALL0,
	0,		//IRQ_5V_FALL1,
	0,		//IRQ_5V_FALL2,
	0,		//IRQ_5V_FALL3,
	_BIT(6),	//IRQ_FMT_CHG,
	_BIT(7),	//IRQ_COL_DEP,
	0,		//IRQ_TMDS_STB,
	_BIT(15),	//IRQ_HDCP_ST_RISE,
	_BIT(16),	//IRQ_HDCP_ST_FALL,
	_BIT(17),	//IRQ_HDCP_EN_RISE,
	_BIT(18),	//IRQ_HDCP_EN_FALL,
	0,		//IRQ_EDID_AD0,
	0,		//IRQ_EDID_AD1,
	0,		//IRQ_EDID_AD2,
	0,		//IRQ_EDID_AD3,
	0,		//IRQ_EDID_CFT0,
	0,		//IRQ_EDID_CFT1,
	0,		//IRQ_EDID_CFT2,
	0,		//IRQ_EDID_CFT3,
	0,		//IRQ_HDCP_SKP,
	0,		//IRQ_HDCP_RND_ERR,
	_BIT(10),	//IRQ_CAB_STB,
	0,		//IRQ_TMDS_ALG,
	_BIT(21),	//IRQ_EMP_DONE,
	_BIT(22),	//IRQ_LAST_EMP,
	_BIT(23),	//IRQ_DE_RISE,
	_BIT(19),	//IRQ_SQOF_RISE,
	_BIT(20),	//IRQ_SQOF_FALL,
	_BIT(24),	//IRQ_AUD_CHG,
	_BIT(25),	//IRQ_CDR_STB,
	/* T6X */
	_BIT(3),	//IRQ_T6X_5V_RISE,
	_BIT(4),	//IRQ_T6X_5V_FALL,
	_BIT(5),	//IRQ_T6X_EDID_AD,
	_BIT(8),	//IRQ_T6X_20_STB,
	_BIT(9),	//IRQ_T6X_21_STB,
	_BIT(10),	//IRQ_PHY_STB,
	_BIT(11),	//IRQ_PXL_STB,
	_BIT(13),	//IRQ_1618_STB,
	_BIT(25),	//IRQ_PLL_CHG0,
	_BIT(26),	//IRQ_PLL_CHG1,
	_BIT(27),	//IRQ_VS_RISE,
	_BIT(28),	//IRQ_VALID_M_RISE,
	_BIT(29),	//IRQ_VALID_M_FALL,
	_BIT(30)	//IRQ_T6X_EDID_CFT,
};

/* for T6X 2.0 */
static const u32 phy_misc_t6x_20[][2] = {
		/*  0x18	0x1c	*/
	{	 /* 24~35M */
		0xff6000c0, 0x11c73003,
	},
	{	 /* 37~75M */
		0xff6000c0, 0x11c73003,
	},
	{	 /* 75~115M */
		0xff600080, 0x11c73002,
	},
	{	 /* 115~150M */
		0xff600080, 0x11c73002,
	},
	{	 /* 150~340M */
		0xff600040, 0x11c73001,
	},
	{	 /* 340~525M */
		0xff600000, 0x11c73000,
	},
	{	 /* 525~600M */
		0xff600000, 0x11c73000,
	},
};

static const u32 phy_dcha_t6x_20[][2] = {
		 /* 0x08	 0x0c*/
		/* some bits default close,reopen when pll stable */
	{	 /* 24~45M */
		0x00f77ccc, 0x40100c59,
	},
	{	 /* 35~75M */
		0x00f77666, 0x40100c59,
	},
	{	 /* 75~115M */
		0x00f77666, 0x40100459,
	},
	{	 /* 115~150M */
		0x00f77666, 0x40100459,
	},
	{	 /* 150~340M */
		0x00f77666, 0x40100459,
	},
	{	 /* 340~525M */
		0x00f73666, 0x7ff00459,
	},
	{	 /* 525~600M */
		0x02821666, 0x7ff00459,
	},
};

static const u32 phy_dchd_t6x_20[][2] = {
		/*  0x10	 0x14 */
		/* some bits default close,reopen when pll stable */
		/* 0x10:12,13,14,15;0x14:12,13,16,17 */
	{	 /* 24~35M */
		0x04000586, 0x30880060,
	},
	{	 /* 35~75M */
		0x04000095, 0x30880060,
	},
	{	 /* 75~115M */
		0x04000095, 0x30880065,
	},
	{	 /* 115~150M */
		0x04000095, 0x30880069,
	},
	{	 /* 140~340M */
		0x04080095, 0x30e80469,
	},
	{	 /* 340~525M */
		0x04080093, 0x30e00469,
	},
	{	 /* 525~600M */
		0x04080093, 0x30e00469,
	},
};

u32 t6x_rlevel[] = {8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7};

/*Decimal to Gray code */
unsigned int decimaltogray_t6x(unsigned int x)
{
	return x ^ (x >> 1);
}

/* Gray code to Decimal */
unsigned int graytodecimal_t6x(unsigned int x)
{
	unsigned int y = x;

	while (x >>= 1)
		y ^= x;
	return y;
}

bool is_pll_lock_t6x(u8 port)
{
	return ((hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, port) >> 31) & 0x1);
}

void t6x_480p_pll_cfg_20(u8 port)
{
	/* the times of pll = 80 for debug */
//	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305000);
//	usleep_range(10, 20);
//	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01481236);
//	usleep_range(10, 20);
//	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305001);
//	usleep_range(10, 20);
//	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305003);
//	usleep_range(10, 20);
//	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01401236);
//	usleep_range(10, 20);
//	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305007);
//	usleep_range(10, 20);
//	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x45305007);
//	usleep_range(10, 20);
	/*the times of pll = 160 */
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0530a000, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x014812a6, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0530a001, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0530a003, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x414012a6, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0530a007, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x4530a007, port);
	usleep_range(10, 20);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x408;
}

void t6x_720p_pll_cfg_20(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305000, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x014812a6, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305001, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305003, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x614012a6, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05305007, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x45305007, port);
	usleep_range(10, 20);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x8;
}

void t6x_1080p_pll_cfg_20(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302800, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x014812a6, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302801, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302803, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x414012a6, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302807, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x45302807, port);
	usleep_range(10, 20);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x8;
}

void t6x_4k30_pll_cfg_20(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302810, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01481236, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302811, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302813, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x21401236, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x05302817, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x45302817, port);
	usleep_range(10, 20);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x8;
}

void t6x_4k60_pll_cfg_20(u32 clkrate, u8 port)
{
	u8 div_4 = 0;

	/* clkrate 1:10 case */
	if (!clkrate)
		div_4 = 1;
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x05302800 | (div_4 << 5)), port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01481236, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x05302801 | (div_4 << 5)), port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x45302803 | (div_4 << 5)), port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01401236, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x05302807 | (div_4 << 5)), port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x45302807 | (div_4 << 5)), port);
	usleep_range(10, 20);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x8;
}

void aml_pll_bw_cfg_t6x_20(u8 port)
{
	u32 idx = rx[port].phy.phy_bw;
	u32 cableclk = rx[port].clk.cable_clk / KHz;
	int pll_rst_cnt = 0;
	u32 clk_rate;

	clk_rate = rx_get_scdc_clkrate_sts(port);
	idx = aml_phy_pll_band(rx[port].clk.cable_clk, clk_rate, port);
	if (!is_clk_stable(port) || !cableclk)
		return;
	if (log_level & PHY_LOG)
		rx_pr("pll bw: %d\n", idx);
	if (rx_info.aml_phy.osc_mode && idx == PHY_BW_5) {
		/* sel osc as pll clock */
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, T6X_20_PLL_CLK_SEL, 1, port);
		/* t6x: select tmds_clk from tclk or tmds_ch_clk */
		/* cdr = tmds_ch_ck,  vco =tclk */
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, T6X_20_VCO_TMDS_EN, 0, port);
	}
	switch (idx) {
	case PLL_BW_0:
		t6x_480p_pll_cfg_20(port);
		break;
	case PLL_BW_1:
		t6x_720p_pll_cfg_20(port);
		break;
	case PLL_BW_2:
		t6x_1080p_pll_cfg_20(port);
		break;
	case PLL_BW_3:
		t6x_4k30_pll_cfg_20(port);
		break;
	case PLL_BW_4:
		t6x_4k60_pll_cfg_20(clk_rate, port);
		break;
	}
	/* do 5 times when clk not stable within a interrupt */
	do {
		if (idx == PLL_BW_0)
			t6x_480p_pll_cfg_20(port);
		if (idx == PLL_BW_1)
			t6x_720p_pll_cfg_20(port);
		if (idx == PLL_BW_2)
			t6x_1080p_pll_cfg_20(port);
		if (idx == PLL_BW_3)
			t6x_4k30_pll_cfg_20(port);
		if (idx == PLL_BW_4)
			t6x_4k60_pll_cfg_20(clk_rate, port);
		if (log_level & PHY_LOG)
			rx_pr("PLL0=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_RG_RX20PLL_0, port));
		if (pll_rst_cnt++ > pll_rst_max) {
			if (log_level & VIDEO_LOG)
				rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG) {
			rx_pr("sq=%d,pll_lock=%d",
			      hdmirx_rd_top(TOP_MISC_STAT0_T6X, port) & 0x1,
			      is_pll_lock_t6x(port));
		}
		usleep_range(50, 100);
	} while (!is_tmds_clk_stable(port) && is_clk_stable(port) && !aml_phy_pll_lock(0));
	if (log_level & PHY_LOG)
		rx_pr("pll done\n");
	/* t6x debug */
	/* manual VGA mode for debug,hyper gain=1 */
	if (rx_info.aml_phy.vga_gain <= 0xfff) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, MSK(12, 0),
				      (decimaltogray_t6x(rx_info.aml_phy.vga_gain & 0x7) |
				      (decimaltogray_t6x(rx_info.aml_phy.vga_gain
				      >> 4 & 0x7) << 4) |
				      (decimaltogray_t6x(rx_info.aml_phy.vga_gain
				      >> 8 & 0x7) << 8)),
				      port);
	}
	/*tap2 byp*/
	if (rx_info.aml_phy.tap2_byp && rx[port].phy.phy_bw >= PHY_BW_3)
		/* dfe_tap_en [28:20]*/
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, _BIT(22), 0, port);
}

int get_tap2_t6x_20(int val)
{
	if ((val >> 4) == 0)
		return val;
	else
		return (0 - (val & 0xf));
}

//for further using,not used now,keep it
bool is_dfe_sts_ok_t6x(u8 port)
{
	u32 data32;
	u32 dfe0_tap2, dfe1_tap2, dfe2_tap2;
	u32 dfe0_tap3, dfe1_tap3, dfe2_tap3;
	u32 dfe0_tap4, dfe1_tap4, dfe2_tap4;
	u32 dfe0_tap5, dfe1_tap5, dfe2_tap5;
	u32 dfe0_tap6, dfe1_tap6, dfe2_tap6;
	u32 dfe0_tap7, dfe1_tap7, dfe2_tap7;
	u32 dfe0_tap8, dfe1_tap8, dfe2_tap8;
	bool ret = true;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x2, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap2 = data32 & 0xf;
	dfe1_tap2 = (data32 >> 8) & 0xf;
	dfe2_tap2 = (data32 >> 16) & 0xf;
	if (dfe0_tap2 >= 8 ||
	    dfe1_tap2 >= 8 ||
	    dfe2_tap2 >= 8)
		ret = false;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap3 = data32 & 0x7;
	dfe1_tap3 = (data32 >> 8) & 0x7;
	dfe2_tap3 = (data32 >> 16) & 0x7;
	if (dfe0_tap3 >= 6 ||
	    dfe1_tap3 >= 6 ||
	    dfe2_tap3 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x4, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap4 = data32 & 0x7;
	dfe1_tap4 = (data32 >> 8) & 0x7;
	dfe2_tap4 = (data32 >> 16) & 0x7;
	if (dfe0_tap4 >= 6 ||
	    dfe1_tap4 >= 6 ||
	    dfe2_tap4 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x5, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap5 = data32 & 0x7;
	dfe1_tap5 = (data32 >> 8) & 0x7;
	dfe2_tap5 = (data32 >> 16) & 0x7;
	if (dfe0_tap5 >= 6 ||
	    dfe1_tap5 >= 6 ||
	    dfe2_tap5 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x6, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap6 = data32 & 0x7;
	dfe1_tap6 = (data32 >> 8) & 0x7;
	dfe2_tap6 = (data32 >> 16) & 0x7;
	if (dfe0_tap6 >= 6 ||
	    dfe1_tap6 >= 6 ||
	    dfe2_tap6 >= 6)
		ret = false;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x7, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap7 = data32 & 0x7;
	dfe0_tap8 = (data32 >> 4) & 0x7;
	dfe1_tap7 = (data32 >> 8) & 0x7;
	dfe1_tap8 = (data32 >> 12) & 0x7;
	dfe2_tap7 = (data32 >> 16) & 0x7;
	dfe2_tap8 = (data32 >> 20) & 0x7;
	if (dfe0_tap7 >= 6 ||
	    dfe1_tap7 >= 6 ||
	    dfe2_tap7 >= 6 ||
	    dfe0_tap8 >= 6 ||
	    dfe1_tap8 >= 6 ||
	    dfe2_tap8 >= 6)
		ret = false;

	return ret;
}

//for further using,not used now,keep it
/* long cable detection for <3G need to be change */
void aml_phy_long_cable_det_t6x(u8 port)
{
	int tap2_0, tap2_1, tap2_2;
	int tap2_max = 0;
	u32 data32 = 0;

	if (rx[port].phy.phy_bw > PHY_BW_3)
		return;
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x2, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	tap2_0 = get_tap2_t6x_20(data32 & 0x1f);
	tap2_1 = get_tap2_t6x_20(((data32 >> 8) & 0x1f));
	tap2_2 = get_tap2_t6x_20(((data32 >> 16) & 0x1f));
	if (rx[port].phy.phy_bw == PHY_BW_2) {
		/*disable DFE*/
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_RSTB, 0, port);
		tap2_max = 6;
	} else if (rx[port].phy.phy_bw == PHY_BW_3) {
		tap2_max = 10;
	}
	if ((tap2_0 + tap2_1 + tap2_2) >= tap2_max) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_BYP_EQ, 0x12, port);
		usleep_range(10, 20);
		rx_pr("long cable\n");
	}
}

/* aml_hyper_gain_tuning */
void aml_hyper_gain_tuning_t6x_20(u8 port)
{
	u32 data32;
	u32 tap0, tap1, tap2;
	u32 hyper_gain_0, hyper_gain_1, hyper_gain_2;
	int eq_boost0, eq_boost1, eq_boost2;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	eq_boost0 = data32 & 0x1f;
	eq_boost1 = (data32 >> 8)  & 0x1f;
	eq_boost2 = (data32 >> 16)	& 0x1f;
	/* use HYPER_GAIN calibration instead of vga */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);

	tap0 = data32 & 0xff;
	tap1 = (data32 >> 8) & 0xff;
	tap2 = (data32 >> 16) & 0xff;
	if ((rx_info.aml_phy.eq_en && eq_boost0 < 3) || tap0 < 0x12) {
		hyper_gain_0 = 1;
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE,
					  T6X_20_LEQ_HYPER_GAIN_CH0,
					  hyper_gain_0,
					  port);
		if (log_level & PHY_LOG)
			rx_pr("ch0 hyper gain trigger\n");
	}
	if ((rx_info.aml_phy.eq_en && eq_boost1 < 3) || tap1 < 0x12) {
		hyper_gain_1 = 1;
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE,
					  T6X_20_LEQ_HYPER_GAIN_CH1,
					  hyper_gain_1,
					  port);
		if (log_level & PHY_LOG)
			rx_pr("ch1 hyper gain trigger\n");
	}
	if ((rx_info.aml_phy.eq_en && eq_boost2 < 3) || tap2 < 0x12) {
		hyper_gain_2 = 1;
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE,
					  T6X_20_LEQ_HYPER_GAIN_CH2,
					  hyper_gain_2,
					  port);
		if (log_level & PHY_LOG)
			rx_pr("ch2 hyper gain trigger\n");
	}
}

int max_offset_t6x_20(int a, int b, int c)
{
	if (a >= b && a >= c)
		return 0;
	if (b >= a && b >= c)
		return 1;
	if (c >= a && c >= b)
		return 2;
	return -1;
}

void aml_eq_retry_t6x_20(u8 port)
{
	u32 data32 = 0;
	u32 eq_boost0, eq_boost1, eq_boost2;

	if (rx[port].phy.phy_bw >= PHY_BW_3) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			T6X_20_EHM_DBG_SEL, 0x0, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
		T6X_20_STATUS_MUX_SEL, 0x3, port);
		usleep_range(100, 110);
		data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
		eq_boost0 = data32 & 0x1f;
		eq_boost1 = (data32 >> 8)  & 0x1f;
		eq_boost2 = (data32 >> 16)	& 0x1f;
		if (eq_boost0 == 0 || eq_boost0 == 31 ||
		    eq_boost1 == 0 || eq_boost1 == 31 ||
		    eq_boost2 == 0 || eq_boost2 == 31) {
			rx_pr("eq_retry:%d-%d-%d\n", eq_boost0, eq_boost1, eq_boost2);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
				T6X_20_EQ_EN, 1, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
				T6X_20_EQ_RSTB, 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
				T6X_20_CDR_RSTB, 0x1, port);
			usleep_range(100, 110);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
				T6X_20_EQ_RSTB, 0x1, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
				T6X_20_CDR_RSTB, 0x1, port);
			usleep_range(10000, 10100);
			if (rx_info.aml_phy.eq_hold)
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_EN,
				0, port);
		}
	}
}

void aml_dfe_en_t6x_20(u8 port)
{
	if (rx_info.aml_phy.dfe_en) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_EN, 1, port);
		//if (rx_info.aml_phy.eq_hold)
			//hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_MODE, 1);
		if (rx_info.aml_phy.eq_retry)
			aml_eq_retry_t6x_20(port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_RSTB, 0, port);
		usleep_range(10, 20);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
				      T6X_20_DFE_RSTB, 1, port);
		usleep_range(200, 220);
		if (rx_info.aml_phy.dfe_hold)
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
					      T6X_20_DFE_HOLD_EN, 1, port);
		if (log_level & PHY_LOG)
			rx_pr("dfe\n");
	}
}

/* phy offset calibration based on different chip and board */
void aml_phy_offset_cal_t6x_20(u8 port)
{
	u32 data32;
	bool rx_sense;

	/* PHY */
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, 0x70080050, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, 0x04008013, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, 0x40102459, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, 0x02821666, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, 0x11c73220, port);
	usleep_range(10, 20);
	data32 = 0xff600100;
	if (rx_info.aml_phy.rterm_flag) {
		data32 = ((data32 & (~((0xf << 12) | 0x1))) |
			(rx_info.aml_phy.rterm_val << 12) | rx_info.aml_phy.rterm_flag << 4);
	}
	rx_sense = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, _BIT(23), port);
	data32 |= (rx_sense << 23);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, data32, port);
	usleep_range(10, 20);

	/* PLL */
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0500f800, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01481236, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0500f801, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0500f803, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01401236, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0500f807, port);
	usleep_range(10, 20);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x4500f807, port);
	usleep_range(100, 200);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, _BIT(26), 1, port);
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, MSK(2, 12), 0X3, port);
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, T6X_20_SLICER_OFSTCAL_START, 1, port);
	usleep_range(10000, 11000);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_OFSET_CAL_START, 1, port);
	usleep_range(200, 210);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_OFSET_CAL_START, 0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, T6X_20_SLICER_OFSTCAL_START, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x0, port);
	if (log_level & PHY_LOG)
		rx_pr("ofst cal\n");
}

u32 min_ch_t6x_20(u32 a, u32 b, u32 c)
{
	if (a <= b && a <= c)
		return 0;
	if (b <= a && b <= c)
		return 1;
	if (c <= a && c <= b)
		return 2;
	return 3;
}

/* hardware eye monitor */
u32 aml_eq_eye_monitor_t6x_20(u8 port)
{
	u32 data32;
	u32 positive_eye_height0, positive_eye_height1, positive_eye_height2;

	usleep_range(50, 100);
	/* hold dfe tap1~tap8 */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			      T6X_20_DFE_HOLD_EN, 1, port);
	usleep_range(10, 20);
	/* disable hw scan mode */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			      T6X_20_EHM_HW_SCAN_EN, 0, port);
	usleep_range(10, 20);

	/* enable hw scan mode */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			      T6X_20_EHM_HW_SCAN_EN, 1, port);

	/* wait for scan done */
	usleep_range(rx_info.aml_phy.eye_delay, rx_info.aml_phy.eye_delay + 100);

	/* Check status */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			      T6X_20_EHM_DBG_SEL, 1, port);
	/* positive eye height  */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			      T6X_20_EHM_DBG_SEL, 1, port);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	positive_eye_height0 = data32 & 0xff;
	positive_eye_height1 = (data32 >> 8) & 0xff;
	positive_eye_height2 = (data32 >> 16) & 0xff;
	/* exit eye monitor scan mode */
	/* disable hw scan mode */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			      T6X_20_EHM_HW_SCAN_EN, 0, port);
	/* disable eye monitor */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			     T6X_20_EHM_DBG_SEL, 0, port);
	//hdmirx_wr_bits_amlphy_t6x(T7_HHI_RX_PHY_DCHA_CNTL2,
			      //T7_EYE_MONITOR_EN1, 0);
	usleep_range(10, 20);
	/* release dfe */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			      T6X_20_DFE_HOLD_EN, 0, port);
	positive_eye_height0 = positive_eye_height0 & 0x3f;
	positive_eye_height1 = positive_eye_height1 & 0x3f;
	positive_eye_height2 = positive_eye_height2 & 0x3f;
	rx_pr("eye height:[%d, %d, %d]\n",
		positive_eye_height0, positive_eye_height1, positive_eye_height2);
	return min_ch_t6x_20(positive_eye_height0, positive_eye_height1, positive_eye_height2);
}

void aml_eq_eye_monitor_t6x_21(u8 port)
{
	u32 data32;
	u32 eye_height0, eye_height1, eye_height2, eye_height3;
	u32 eom_done0, eom_done1, eom_done2, eom_done3;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_HOLD, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, _BIT(31), 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_EYE_EN_HW_SCAN, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x1, port);
	usleep_range(5000, 5500);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	eye_height0 = data32 & 0x3f;
	eye_height1 = (data32 >> 8) & 0x3f;
	eye_height2 = (data32 >> 16) & 0x3f;
	eye_height3 = (data32 >> 24) & 0x3f;
	eom_done0 = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, _BIT(6), port);
	eom_done1 = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, _BIT(14), port);
	eom_done2 = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, _BIT(22), port);
	eom_done3 = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, _BIT(30), port);
	rx_pr("eom_ber < 4e-7\n");
	rx_pr("eom_done=[%d, %d, %d, %d]\n", eom_done0, eom_done1, eom_done2, eom_done3);
	rx_pr("eom_eh = [%d, %d, %d, %d]\n", eye_height0, eye_height1, eye_height2, eye_height3);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_EYE_EN_HW_SCAN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, _BIT(31), 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_HOLD, 0x0, port);
}

void aml_eq_eye_monitor_t6x(u8 port)
{
	if (port <= E_PORT1)
		aml_eq_eye_monitor_t6x_20(port);
	else
		aml_eq_eye_monitor_t6x_21(port);
}

void get_eq_val_t6x_20(u8 port)
{
	u32 data32 = 0;
	u32 eq_boost0, eq_boost1, eq_boost2;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	eq_boost0 = data32 & 0x1f;
	eq_boost1 = (data32 >> 8)  & 0x1f;
	eq_boost2 = (data32 >> 16)      & 0x1f;
	if (log_level & PHY_LOG)
		rx_pr("eq:%d-%d-%d\n", eq_boost0, eq_boost1, eq_boost2);
}

/* check eq_boost1 & tap0 status */
bool is_eq1_tap0_err_t6x_20(u8 port)
{
	u32 data32 = 0;
	u32 eq0, eq1, eq2;
	u32 tap0, tap1, tap2;
	u32 eq_avr, tap0_avr;
	bool ret = false;

	if (rx[port].phy.phy_bw < PHY_BW_5)
		return ret;
	/* get eq_boost1 val */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	eq0 = data32 & 0x1f;
	eq1 = (data32 >> 8)  & 0x1f;
	eq2 = (data32 >> 16)      & 0x1f;
	eq_avr =  (eq0 + eq1 + eq2) / 3;
	if (log_level & EQ_LOG)
		rx_pr("eq0=0x%x, eq1=0x%x, eq2=0x%x avr=0x%x\n",
			eq0, eq1, eq2, eq_avr);

	/* get tap0 val */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x3, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	tap0 = data32 & 0xff;
	tap1 = (data32 >> 8) & 0xff;
	tap2 = (data32 >> 16) & 0xff;
	tap0_avr = (tap0 + tap1 + tap2) / 3;
	if (log_level & EQ_LOG)
		rx_pr("tap0=0x%x, tap1=0x%x, tap2=0x%x avr=0x%x\n",
			tap0, tap1, tap2, tap0_avr);
	if (eq_avr >= 21 && tap0_avr >= 40)
		ret = true;

	return ret;
}

void aml_agc_flow_t6x_20(u8 port)
{
	int i;
	u32 data32 = 0;
	u32 tap0, tap1, tap2;
	int flags = 0x7;

	for (i = 7; i > 0; i--) {
		if (flags & 0x1)
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, MSK(3, 0),
									decimaltogray_t6x(i),
									port);
		if (flags & 0x2)
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, MSK(3, 4),
									decimaltogray_t6x(i),
									port);
		if (flags & 0x4)
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, MSK(3, 8),
									decimaltogray_t6x(i),
									port);
		usleep_range(50, 60);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			T6X_20_EHM_DBG_SEL, 0x0, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			T6X_20_STATUS_MUX_SEL, 0x3, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			T6X_20_DFE_OFST_DBG_SEL, 0x0, port);
		data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
		tap0 = data32 & 0xff;
		tap1 = (data32 >> 8) & 0xff;
		tap2 = (data32 >> 16) & 0xff;
		if (tap0 <= rx_info.aml_phy.tapx_value)
			flags &= 0x6;
		if (tap1 <= rx_info.aml_phy.tapx_value)
			flags &= 0x5;
		if (tap2 <= rx_info.aml_phy.tapx_value)
			flags &= 0x3;
		if (!flags)
			break;
	}
	rx_pr("agc done\n");
}

u32 eq_eye_height_t6x_20(u32 wst_ch, u8 port)
{
	u32 data32;
	u32 positive_eye_height;

	/* hold dfe tap1~tap8 */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
					  T6X_20_DFE_HOLD_EN, 1, port);
	usleep_range(10, 20);
	/* disable hw scan mode */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
					  T6X_20_EHM_HW_SCAN_EN, 0, port);
	usleep_range(10, 20);
	/* enable hw scan mode */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			  T6X_20_EHM_HW_SCAN_EN, 1, port);

	/* wait for scan done */
	usleep_range(rx_info.aml_phy.eye_delay, rx_info.aml_phy.eye_delay + 100);
	/* positive eye height	*/
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			  T6X_20_EHM_DBG_SEL, 1, port);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	positive_eye_height = data32 >> (8 * wst_ch) & 0xff;
	/* exit eye monitor scan mode */
	/* disable hw scan mode */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			  T6X_20_EHM_HW_SCAN_EN, 0, port);
	/* disable eye monitor */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			 T6X_20_EHM_DBG_SEL, 0, port);
	//hdmirx_wr_bits_amlphy_t6x(T7_HHI_RX_PHY_DCHA_CNTL2,
			  //T7_EYE_MONITOR_EN1, 0);
	usleep_range(10, 20);
	/* release dfe */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
					  T6X_20_DFE_HOLD_EN, 0, port);
	positive_eye_height = positive_eye_height & 0x3f;
	return positive_eye_height;
}

void dump_cdr_info_t6x_20(u8 port)
{
	u32 cdr0_lock, cdr1_lock, cdr2_lock;
	u32 cdr0_int, cdr1_int, cdr2_int;
	u32 cdr0_code, cdr1_code, cdr2_code;
	u32 data32;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x22, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_MUX_CDR_DBG_SEL, 0x0, port);
	usleep_range(10, 20);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(10, 20);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	cdr0_int = data32 & 0x7f;
	cdr1_int = (data32 >> 8) & 0x7f;
	cdr2_int = (data32 >> 16) & 0x7f;
	rx_pr("cdr_code=[%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code);
	rx_pr("cdr_lock=[%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock);
	comb_val_t6x(get_val_t6x, "cdr_int", cdr0_int, cdr1_int, cdr2_int, 7);
}

void cdr_retry_t6x_20(u8 port)
{
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, _BIT(6), 0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_CDR_RSTB, 0x0, port);
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_CDR_RSTB, 0x1, port);
	usleep_range(100, 200);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_RSTB, 0x1, port);
	usleep_range(100, 200);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_RSTB, 0x1, port);
	usleep_range(500, 600);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, _BIT(6), 1, port);
	usleep_range(100, 200);
	if (log_level & PHY_LOG)
		dump_cdr_info_t6x_20(port);
}

void dfe_tap0_pol_polling_t6x_20(u32 pos_min_eh, u32 pos_avg_eh, u32 wst_ch, u8 port)
{
	u32 int_eye_height_sum = 0;
	u32 int_eye_height[20];
	u32 int_avg_eye_height;
	u32 int_min_eye_height = 63;
	u32 neg_eye_height_sum = 0;
	u32 neg_eye_height[20];
	u32 neg_avg_eye_height;
	u32 neg_min_eye_height = 63;
	int i, j, k;

	/*select inter leave*/
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(2, 20), 0x0, port);
	usleep_range(100, 200);
	for (i = 0; i < 20; i++)
		int_eye_height[i] = eq_eye_height_t6x_20(wst_ch, port);
	quick_sort2_t6x_20(int_eye_height, 0, 19);
	int_min_eye_height = int_eye_height[1];
	for (j = 1; j < 6; j++)
		int_eye_height_sum += int_eye_height[j];
	int_avg_eye_height = int_eye_height_sum;
	if (log_level & PHY_LOG) {
		rx_pr("int_min_eye_height = %d\n", int_min_eye_height);
		rx_pr("int_avg_eye_height = %d / 5\n", int_avg_eye_height);
	}
	if (int_min_eye_height > pos_min_eh ||
		(int_min_eye_height == pos_min_eh && int_avg_eye_height > pos_avg_eh)) {
		rx_pr("select int eq\n");
		return;
	}
	/*select negative*/
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(2, 20), 0x3, port);
	usleep_range(100, 200);
	for (k = 0; k < 20; k++)
		neg_eye_height[k] = eq_eye_height_t6x_20(wst_ch, port);
	quick_sort2_t6x_20(neg_eye_height, 0, 19);
	neg_min_eye_height = neg_eye_height[1];
	for (j = 1; j < 6; j++)
		neg_eye_height_sum += neg_eye_height[j];
	neg_avg_eye_height = neg_eye_height_sum;
	if (log_level & PHY_LOG) {
		rx_pr("neg_min_eye_height = %d\n", neg_min_eye_height);
		rx_pr("neg_avg_eye_height = %d / 5\n", neg_avg_eye_height);
	}
	if (neg_min_eye_height > pos_min_eh ||
		(neg_min_eye_height == pos_min_eh && neg_avg_eye_height > pos_avg_eh)) {
		rx_pr("select neg eq\n");
		return;
	}
	/*select positive*/
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(2, 20), 0x2, port);
	usleep_range(100, 200);
	rx_pr("select pos eq\n");
}

void swap_num_t6x_20(int *a, int *b)
{
	int temp = *a;
	*a = *b;
	*b = temp;
}

int *num_divide_three_t6x_20(int arr[], int l, int r)
{
	int less = l - 1;
	int more = r;
	int *p = kmalloc(sizeof(int) * 2, GFP_KERNEL);

	while (l < more) {
		if (arr[l] < arr[r])
			swap_num_t6x_20(&arr[++less], &arr[l++]);
		else if (arr[l] > arr[r])
			swap_num_t6x_20(&arr[l], &arr[--more]);
		else
			l++;
	}
	swap_num_t6x_20(&arr[more], &arr[r]);
	p[0] = less + 1;
	p[1] = more;
	return p;
}

void quick_sort2_t6x_20(int arr[], int l, int r)
{
	int len = r + 1;
	int i, j;

	for (i = 0; i < len - 1; i++) {
		for (j = 0; j < len - 1 - i; j++) {
			if (arr[j] > arr[j + 1])
				swap_num_t6x_20(&arr[j], &arr[j + 1]);
		}
	}
}

void aml_enhance_dfe_old_t6x_20(u8 port)
{
	u32 wst_ch;
	int i, j;
	u32 pos_eye_height_sum = 0;
	u32 pos_min_eye_height = 63;
	u32 pos_eye_height[20];
	u32 pos_avg_eye_height;

	wst_ch = aml_eq_eye_monitor_t6x_20(port);
	for (i = 0; i < 20; i++)
		pos_eye_height[i] = eq_eye_height_t6x_20(wst_ch, port);
	quick_sort2_t6x_20(pos_eye_height, 0, 19);
	pos_min_eye_height = pos_eye_height[1];
	for (j = 1; j < 6; j++)
		pos_eye_height_sum += pos_eye_height[j];
	pos_avg_eye_height = pos_eye_height_sum;
	if (log_level & PHY_LOG) {
		rx_pr("pos_min_eye_height = %d\n", pos_min_eye_height);
		rx_pr("pos_avg_eye_height = %d / 5\n", pos_avg_eye_height);
	}
	if (pos_avg_eye_height < rx_info.aml_phy.eye_height * 5)
		dfe_tap0_pol_polling_t6x_20(pos_min_eye_height, pos_avg_eye_height, wst_ch, port);
}

void aml_enhance_dfe_new_t6x_20(u8 port)
{
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(2, 20), 0x0, port);
}

void eq_max_offset_t6x_20(int eq_boost0, int eq_boost1, int eq_boost2, u8 port)
{
	int offset_eq0, offset_eq1, offset_eq2;
	int ch = -1;
	int eq_initial = 0;

	offset_eq0 = abs(2 * eq_boost0 - eq_boost1 - eq_boost2);
	offset_eq1 = abs(2 * eq_boost1 - eq_boost0 - eq_boost2);
	offset_eq2 = abs(2 * eq_boost2 - eq_boost0 - eq_boost1);
	ch = max_offset_t6x_20(offset_eq0, offset_eq1, offset_eq2);
	if (ch == 0)
		eq_initial = (eq_boost1 + eq_boost2) / 2;
	if (ch == 1)
		eq_initial = (eq_boost0 + eq_boost2) / 2;
	if (ch == 2)
		eq_initial = (eq_boost0 + eq_boost1) / 2;
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_RSTB, 0x0, port);
	if (rx_info.aml_phy.eq_level & 0x2) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(5, 0), eq_initial, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, _BIT(5), 0x1, port);
	}
	if (rx_info.aml_phy.eq_level & 0x4) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(5, 0), eq_initial, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, _BIT(5), 0x0, port);
	}
	if (rx_info.aml_phy.eq_level & 0x8) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(5, 0), 15, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, _BIT(5), 0x1, port);
	}
	if (rx_info.aml_phy.eq_level & 0x10) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(5, 0), 15, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, _BIT(5), 0x0, port);
	}
	if (rx_info.aml_phy.eq_level & 0x20) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, MSK(5, 0), 15, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, _BIT(13), 0, port);
		usleep_range(10, 20);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, _BIT(13), 1, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, _BIT(13), 0, port);
		usleep_range(100, 110);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, _BIT(13), 1, port);
	}
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_RSTB, 0x1, port);
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, _BIT(5), 0x0, port);
	usleep_range(100, 110);
}

void aml_enhance_eq_t6x_20(u8 port)
{
	int eq_boost0, eq_boost1, eq_boost2;
	int data32;
	int offset_eq0, offset_eq1, offset_eq2;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	eq_boost0 = data32 & 0x1f;
	eq_boost1 = (data32 >> 8)  & 0x1f;
	eq_boost2 = (data32 >> 16)	& 0x1f;
	eq_boost0 = (eq_boost0 >= 23) ? 23 : eq_boost0;
	eq_boost1 = (eq_boost1 >= 23) ? 23 : eq_boost1;
	eq_boost2 = (eq_boost2 >= 23) ? 23 : eq_boost2;
	offset_eq0 = abs(2 * eq_boost0 - eq_boost1 - eq_boost2);
	offset_eq1 = abs(2 * eq_boost1 - eq_boost0 - eq_boost2);
	offset_eq2 = abs(2 * eq_boost2 - eq_boost0 - eq_boost1);
	if (offset_eq0 > 15 || offset_eq1 > 15 || offset_eq2 > 15) {
		eq_max_offset_t6x_20(eq_boost0, eq_boost1, eq_boost2, port);
	/* read eq value after eq retry */
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
		T6X_20_EHM_DBG_SEL, 0x0, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			T6X_20_STATUS_MUX_SEL, 0x3, port);
		usleep_range(100, 110);
		data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
		eq_boost0 = data32 & 0x1f;
		eq_boost1 = (data32 >> 8)  & 0x1f;
		eq_boost2 = (data32 >> 16)	& 0x1f;
		if (log_level & PHY_LOG)
			rx_pr("after enhance eq:%d-%d-%d\n", eq_boost0, eq_boost1, eq_boost2);
	} else {
		if (log_level & PHY_LOG)
			rx_pr("no enhance eq\n");
	}
}

void aml_eq_cfg_t6x_20(u8 port)
{
	u32 idx = rx[port].phy.phy_bw;
	u32 cdr0_int, cdr1_int, cdr2_int;
	u32 data32;
	int i = 0;
	/* do not need to run eq if no sqo_clk or pll not lock */
	if (!is_clk_stable(port))
		return;
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_CDR_RSTB, 1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_CDR_EN, 1, port);
	usleep_range(200, 210);
	if (idx >= PHY_BW_3)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EN_BYP_EQ, 0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_EN, 1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_RSTB, 1, port);
	usleep_range(200, 210);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_EN, 1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_RSTB, 1, port);
	if (rx_info.aml_phy.cdr_fr_en) {
		udelay(rx_info.aml_phy.cdr_fr_en);
		/*cdr fr en*/
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, _BIT(6), 1, port);
	}
	usleep_range(10000, 10100);
	get_eq_val_t6x_20(port);
	/* enable dfe for all frequency */
	if (rx[port].phy.phy_bw >= PHY_BW_3)
		aml_dfe_en_t6x_20(port);
	if (is_eq1_tap0_err_t6x_20(port)) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, T6X_20_LEQ_BUF_GAIN, 0x0, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, T6X_20_LEQ_POLE, 0x0, port);
		if (log_level & EQ_LOG)
			rx_pr("eq1 & tap0 err, tune eq setting\n");
	}
	/*enable HYPER_GAIN calibration for 6G to fix 2.0 cts HF2-1 issue*/
	if (rx[port].phy.phy_bw >= PHY_BW_2 && rx_info.aml_phy.agc_enable)
		aml_agc_flow_t6x_20(port);
	//eq <= 3 will trigger the new hyper gain function
	// if an inappropriate eq value,it happened to
	//trigger hyper gain when eq <= 3,there is no chance to convergence to
	// a proper eq value any more.auto eq failed.
	//the only way to exit this wrong state is back to phy_init entrance.
	//enhance_eq insert before hyper gain function is try to get a proper/
	//right value.
	//this enhance_eq should auto convergence,never to be forced in this
	//stage,or hyper gain fail.
	//can do enhance_eq one more time after enhance_dfe finished.
	if (rx[port].phy.phy_bw >= PHY_BW_2 && rx_info.aml_phy.enhance_eq)
		aml_enhance_eq_t6x_20(port);
	if (rx[port].phy.phy_bw == PHY_BW_2 || rx[port].phy.phy_bw == PHY_BW_1 ||
		rx_info.aml_phy.hyper_gain_en)
		aml_hyper_gain_tuning_t6x_20(port);
	/* manual EQ mode for debug */
	if (rx_info.aml_phy.eq_stg1 < 0x1f) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
				      T6X_20_BYP_EQ, rx_info.aml_phy.eq_stg1 & 0x1f, port);
		/* eq adaptive:0-adaptive 1-manual */
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EN_BYP_EQ, 1, port);
	}
	usleep_range(200, 210);
	/*tmds valid det*/
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_CDR_LKDET_EN, 1, port);
	if (log_level & PHY_LOG)
		dump_cdr_info_t6x_20(port);
	for (i = 0; i < rx_info.aml_phy.cdr_retry_max && rx_info.aml_phy.cdr_retry_en; i++) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			T6X_20_EHM_DBG_SEL, 0x0, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ,
			T6X_20_STATUS_MUX_SEL, 0x22, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			T6X_20_MUX_CDR_DBG_SEL, 0x0, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR,
			T6X_20_MUX_CDR_DBG_SEL, 0x1, port);
		usleep_range(10, 20);
		data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
		cdr0_int = data32 & 0x7f;
		cdr1_int = (data32 >> 8) & 0x7f;
		cdr2_int = (data32 >> 16) & 0x7f;
		if (cdr0_int || cdr1_int || cdr2_int)
			cdr_retry_t6x_20(port);
		else
			break;
	}
	if (log_level & PHY_LOG)
		rx_pr("cdr retry times:%d!!!\n", i);
	if (i == rx_info.aml_phy.cdr_retry_max && rx_info.aml_phy.cdr_fr_en_auto) {
		if ((cdr0_int == 0 && cdr1_int == 0) ||
			(cdr0_int == 0 && cdr2_int == 0) ||
			(cdr1_int == 0 && cdr2_int == 0)) {
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, _BIT(6), 0, port);
			if (log_level & PHY_LOG)
				rx_pr("cdr_fr_en force 0!!!\n");
		}
	}
	if (rx[port].phy.phy_bw >= PHY_BW_5 && rx_info.aml_phy.enhance_dfe_en_old)
		aml_enhance_dfe_old_t6x_20(port);
	if (rx[port].phy.phy_bw >= PHY_BW_5 && rx_info.aml_phy.enhance_dfe_en_new)
		aml_enhance_dfe_new_t6x_20(port);
	if (rx[port].phy.phy_bw >= PHY_BW_2 && rx_info.aml_phy.enhance_eq)
		aml_enhance_eq_t6x_20(port);
	if (log_level & PHY_LOG) {
		rx_pr("%s,%s,%s\n", rx_info.aml_phy.enhance_dfe_en_new ? "new dfe" : "old dfe",
		rx_info.aml_phy.enhance_eq ? "eq en" : "no eq",
		rx_info.aml_phy.eq_en ? "eq trigger" : "eq no trigger");
		rx_pr("2.0 PHY Register:\n");
		rx_pr("dchd_eq-0x14=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, port));
		rx_pr("dchd_cdr-0x10=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, port));
		rx_pr("dcha_dfe-0xc=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, port));
		rx_pr("dcha_afe-0x8=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, port));
		rx_pr("misc2-0x1c=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, port));
		rx_pr("misc1-0x18=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, port));
		rx_pr("phy end\n");
	}
}

void aml_phy_get_trim_val_t6x(void)
{
	if (rx_info.aml_phy.rterm_dbg_lvl)
		rx_info.aml_phy.rterm_dts_lvl =
			rx_info.aml_phy.rterm_dbg_lvl;
	if (rx_info.aml_phy.rterm_dts_lvl > 15)
		rx_info.aml_phy.rterm_dts_lvl = 15;
	rx_info.aml_phy.rterm_val = t6x_rlevel[rx_info.aml_phy.rterm_dts_lvl];
}

void aml_phy_cfg_t6x_20(u8 port)
{
	u32 idx = rx[port].phy.phy_bw;
	u32 data32;
	bool rx_sense;

	if (rx_info.aml_phy.pre_int) {
		if (log_level & PHY_LOG)
			rx_pr("\nphy reg bw: %d\n port = %d\n", idx, port);
		if (rx_info.aml_phy.ofst_en)
			aml_phy_offset_cal_t6x_20(port);
		data32 = phy_dcha_t6x_20[idx][0];
		if (rx_info.aml_phy.phy_debug_en && rx_info.aml_phy.afe_value)
			data32 = rx_info.aml_phy.afe_value;
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, data32, port);
		usleep_range(5, 10);
		data32 = phy_dcha_t6x_20[idx][1];
		if (rx_info.aml_phy.phy_debug_en && rx_info.aml_phy.dfe_value)
			data32 = rx_info.aml_phy.dfe_value;
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, data32, port);
		usleep_range(5, 10);
		data32 = phy_dchd_t6x_20[idx][0];
		if (rx_info.aml_phy.phy_debug_en && rx_info.aml_phy.cdr_value)
			data32 = rx_info.aml_phy.cdr_value;
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, data32, port);
		usleep_range(5, 10);
		data32 = phy_dchd_t6x_20[idx][1];
		if (rx_info.aml_phy.phy_debug_en && rx_info.aml_phy.eq_value)
			data32 = rx_info.aml_phy.eq_value;
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, data32, port);
		usleep_range(5, 10);
		data32 = phy_misc_t6x_20[idx][0];
		if (rx_info.aml_phy.phy_debug_en && rx_info.aml_phy.misc1_value)
			data32 = rx_info.aml_phy.misc1_value;
		if (rx_info.aml_phy.rterm_flag) {
			data32 = ((data32 & (~((0xf << 12) | 0x1))) |
				(rx_info.aml_phy.rterm_val << 12) |
				rx_info.aml_phy.rterm_flag << 4);
		}
		rx_sense = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, _BIT(23), port);
		data32 |= (rx_sense << 23);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, data32, port);
		usleep_range(5, 10);
		data32 = phy_misc_t6x_20[idx][1];
		if (rx_info.aml_phy.phy_debug_en && rx_info.aml_phy.misc2_value)
			data32 = rx_info.aml_phy.misc2_value;
		/* port switch */
		data32 &= (~(0xf << 28));
		data32 |= (0xf << 28);
		data32 &= (~(0xf << 24));
		data32 |= ((1 << port) << 24);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, data32, port);
		usleep_range(5, 10);
		if (!rx_info.aml_phy.pre_int_en)
			rx_info.aml_phy.pre_int = 0;
	}
	if (rx_info.aml_phy.sqrst_en) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, T6X_20_SQ_RSTN, 0, port);
		usleep_range(5, 10);
		/*sq_rst*/
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, T6X_20_SQ_RSTN, 1, port);
	}
}

/*
 * HDMIRX 2.1 PHY
 */
static const u32 phy_misc_t6x_21[][3] = {
		/* misc0	misc1		misc2 */
		/* 0x114	0x118		0x11c */
	{	 /* 24~35M */
		0x1bf777f, 0x10f7000f, 0x00005f00,
	},
	{	 /* 37~75M */
		0x1bf777f, 0x10f7000f, 0x00005f00,
	},
	{	 /* 75~150M */
		0x1bf777f, 0x10f7000f, 0x00005f00,
	},
	{	 /* 150~340M */
		0x1bf777f, 0x10f7000f, 0x00005a00,
	},
	{	 /* 340~525M */
		0x1bf777f, 0x10f7000f, 0x00005a00,
	},
	{	 /* 525~600M */
		0x1bf777f, 0x10f7000f, 0x00005a00,
	},
	{	 /* FRL 3G */
		0x01bcffff, 0x30f7000f, 0x00001a00,
	},
	{	 /* FRL 6G3L */
		0x01bcffff, 0x30f7000f, 0x00001a00,
	},
	{	 /* FRL 6G4L */
		0x01bcffff, 0x30f7000f, 0x00001a00,
	},
	{	 /* FRL 8G */
		0x01bcffff, 0x30f7000f, 0x00001a00,
	},
	{	 /* FRL 10G */
		0x01bcffff, 0x30f7000f, 0x00001a00,
	},
	{	 /* FRL 12G */
		0x01bcffff, 0x30f7000f, 0x00001a00,
	},
};

static const u32 phy_dcha_t6x_21[][4] = {
		/* dcha_afe		dcha_dfe	dcha_pi		dcha_ctrl*/
		/* 0x120		0x124		0x128		0x12c*/
	{	 /* 24~45M */
		0x03708888, 0x04ff1a05, 0x41070238, 0x03f06555,
	},
	{	 /* 35~75M */
		0x03708888, 0x04ff1a05, 0x41070138, 0x03f06555,
	},
	{	 /* 75~150M */
		0x0370ffff, 0x04ff1a05, 0x41070038, 0x03f06555,
	},
	{	 /* 150~340M */
		0x0370ffff, 0x04ff1a05, 0x41030028, 0x03f06555,
	},
	{	 /* 340~525M */
		0x2320ffff, 0x04ff1a05, 0x41000018, 0x03f06555,
	},
	{	 /* 525~600M */
		0x2320ffff, 0x04ff1a05, 0x41000018, 0x03f06555,
	},
	{	 /* FRL 3G */
		0x0370ffff, 0x04ff1a05, 0x41030028, 0x03f06555,
	},
	{	 /* FRL 6G3L */
		0x2320ffff, 0x04ff1a05, 0x41001018, 0x03f06555,
	},
	{	 /* FRL 6G4L */
		0x2320ffff, 0x04ff1a05, 0x41001018, 0x03f06555,
	},
	{	 /* FRL 8G */
		0x2340ffff, 0x04ff1a05, 0x41102008, 0x03f06555,
	},
	{	 /* FRL 10G */
		0x1340ffff, 0x04ff1a05, 0x41102008, 0x03f06555,
	},
	{	 /* FRL 12G */
		0x1340ffff, 0x04ff1a05, 0x41102008, 0x03f06555,
	},
};

static const u32 phy_dchd_t6x_21[][2] = {
		/* dchd_cdr	 dchd_eq */
		/*  0x130	 0x134 */
	{	 /* 24~35M */
		0x06407e83, 0x300b3060,
	},
	{	 /* 35~75M */
		0x06407d83, 0x300b3060,
	},
	{	 /* 75~150M */
		0x06407c83, 0x30033069,
	},
	{	 /* 140~340M */
		0x06407882, 0x30033049,
	},
	{	 /* 340~525M */
		0x06407482, 0x3003304f,
	},
	{	 /* 525~600M */
		0x06407482, 0x3003304f,
	},
	{	 /* FRL 3G */
		0x06407882, 0x3003304f,
	},
	{	 /* FRL 6G3L */
		0x06407482, 0x3003304f,
	},
	{	 /* FRL 6G4L */
		0x06407482, 0x3003304f,
	},
	{	 /* FRL 8G */
		0x06407082, 0x3003304f,
	},
	{	 /* FRL 10G */
		0x06407082, 0x3003304f,
	},
	{	 /* FRL12G */
		0x06407082, 0x3003304f,
	},
};

int dts_debug_flag_t6x_21;
int rlevel_t6x_21;
int rterm_trim_val_t6x_21;
int rterm_trim_flag_t6x_21;

void aml_phy_get_trim_val_t6x_21(u8 port)
{
	if (rx_info.aml_phy_21.rterm_dbg_lvl)
		rx_info.aml_phy_21.rterm_dts_lvl =
			rx_info.aml_phy_21.rterm_dbg_lvl;
	if (rx_info.aml_phy_21.rterm_dts_lvl > 15)
		rx_info.aml_phy_21.rterm_dts_lvl = 15;
	rx_info.aml_phy_21.rterm_val = t6x_rlevel[rx_info.aml_phy_21.rterm_dts_lvl];
}

void aml_phy_offset_cal_t6x_21(int port)
{
	u32 data32;
	/* rterm not enabled */
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x013cfff0, port);
	/* squelch not enabled*/
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, 0x3077000f, port);
	data32 = 0x00005a00;
	if (rx_info.aml_phy_21.rterm_flag) {
		data32 = ((data32 & (~(0xf | (0x1 << 31)))) |
			(rx_info.aml_phy_21.rterm_val) | rx_info.aml_phy_21.rterm_flag << 31);
	}
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC2,  data32, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,  0x2320ffff, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE,  0x05ff1a05, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,  0x51102008, port);//zw
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL,  0x03f06555, port);//zw
	/* cdr lkdet reset = 0*/
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR,  0x064010c2, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ,  0x30011052, port);//zw
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, DCH_RSTN, 0xf, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, SQ_RSTN, 0x1, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa00, port);//zw
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x01445384, port);//zw
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa01, port);//zw
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa03, port);//zw
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa07, port);//zw
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4520fa07, port);//zw
	usleep_range(20, 30);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, DFE_TAP_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_FR_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_LKDET_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_OFST_CAL_START, 0x0, port);//zw
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, DFE_OFST_CAL_START, 0x1, port);//zw
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, OFST_CAL_STAGE, 0x0, port);//zw
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, OFST_CAL_START, 0x1, port);
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_RSTB, 0x1, port);
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, OFST_CAL_STAGE, 0x1, port);//zw
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, OFST_CAL_STAGE, 0x2, port);//zw
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, OFST_CAL_STAGE, 0x3, port);//zw
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, OFST_CAL_START, 0x0, port);//zw
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, DFE_OFST_CAL_START, 0x0, port);//zw
	usleep_range(100, 110);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_OFST_CAL_START, 0x1, port);//zw
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_RSTB, 0x0, port);//zw
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_RSTB, 0x1, port);//zw
	usleep_range(1000, 1100);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_OFST_CAL_START, 0x0, port);//zw
	usleep_range(100, 110);
	/* turn off pll */
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x0, port);
	if (log_level & PHY_LOG)
		rx_pr("h21 ofst cal\n");
}

void rx_21_frl_phy_cfg_t6x(u8 port)
{
	u32 data32 = 0;
	u32 idx = rx[port].phy.phy_bw;
	bool rx_sense;

	if (rx_info.aml_phy_21.pre_int_21[port]) {
		if (rx_info.aml_phy_21.ofst_en)
			aml_phy_offset_cal_t6x_21(port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, DFE_TAP_EN, 0x1ff, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, DFE_H1_PD, 0x0, port);

		data32 = phy_misc_t6x_21[idx][0];
		rx_sense = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(22), port);
		data32 |= (rx_sense << 22);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, data32, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, phy_misc_t6x_21[idx][1], port);
		aml_phy_get_trim_val_t6x_21(port);
		data32 = phy_misc_t6x_21[idx][2];
		if (rx_info.aml_phy_21.rterm_flag) {
			data32 = ((data32 & (~(0xf | (0x1 << 31)))) |
				(rx_info.aml_phy_21.rterm_val) |
				rx_info.aml_phy_21.rterm_flag << 31);
		}
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, data32, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,  phy_dcha_t6x_21[idx][0], port);
		rx_pr("RX21PHY_DCHA_AFE-0x120=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, port));
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE,  phy_dcha_t6x_21[idx][1], port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,  phy_dcha_t6x_21[idx][2], port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL,  phy_dcha_t6x_21[idx][3], port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR,  phy_dchd_t6x_21[idx][0], port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ,  phy_dchd_t6x_21[idx][1], port);
		//hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(22), 0x1, port);
		if (!rx_info.aml_phy_21.pre_int_en)
			rx_info.aml_phy_21.pre_int_21[port] = 0;
	}
	//rterm_en(dcha_misc0[22]) = 0x1
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_FR_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_LKDET_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_MODE, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_HOLD, 0x0, port);
	if (log_level & PHY_LOG)
		rx_pr("rx_21_phy_cfg\n");
}

void t6x_480p_pll_cfg_21(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a000, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014812a6, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a001, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a003, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x414012a6, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a007, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4530a007, port);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x408;
}

void t6x_720p_pll_cfg_21(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a010, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014812a6, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a011, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a013, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x214012a6, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0530a017, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4530a017, port);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x408;
}

void t6x_1080p_pll_cfg_21(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302800, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014452a6, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302801, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302803, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x45302807, port);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x8;
}

void t6x_4k30_pll_cfg_21(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302810, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x01481236, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302811, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302813, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x21401236, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302817, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x45302817, port);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x8;
}

void t6x_4k60_pll_cfg_21(u32 clkrate, u8 port)
{
	u8 div_4 = 0;

	if (!clkrate)
		div_4 = 1;
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, (0x05302800 | (div_4 << 5)), port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x01481236, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, (0x05302801 | (div_4 << 5)), port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x05302803 | (div_4 << 5), port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x01401236, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, (0x05302807 | (div_4 << 5)), port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, (0x45302807 | (div_4 << 5)), port);
	usleep_range(10, 20);
	rx[port].phy.aud_div = 0;
	rx[port].phy.aud_div_1 = 0x8;
}

void aml_pll_bw_cfg_t6x_21(int f_rate, u8 port)
{
	u32 data32;
	int pll_rst_cnt = 0;
	u32 idx = rx[port].phy.pll_bw;
	u32 clk_rate = 0;

	if (f_rate)
		idx = f_rate + 5;
	if (rx_info.aml_phy_21.phy_bwth) {
		switch (f_rate) {
		case FRL_6G_4LANE:
		case FRL_8G_4LANE:
		case FRL_10G_4LANE:
		case FRL_12G_4LANE:
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(7), 0x1, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(11), 0x1, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(15), 0x1, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, CCH_EN, 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, SQ_GATED, 0x1, port);
			break;
		case FRL_3G_3LANE:
		case FRL_6G_3LANE:
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(7), 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(11), 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(15), 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, CCH_EN, 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, SQ_GATED, 0x1, port);
			break;
		case FRL_OFF:
		default:
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(7), 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(11), 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, _BIT(15), 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, CCH_EN, 0x3, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, SQ_GATED, 0x0, port);
			break;
		}
		data32 = phy_dcha_t6x_21[idx][0];
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, data32, port);
		data32 = phy_dcha_t6x_21[idx][2];
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, data32, port);
		data32 = phy_dchd_t6x_21[idx][0];
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_OS_RATE,
			(data32 >> 8) & 0x3, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_PI_DIV,
			(data32 >> 10) & 0x3, port);
		data32 = phy_dchd_t6x_21[idx][1];
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_VAL1,
			data32 & 0x1f, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_EN,
			(data32 >> 5) & 0x1, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_TAPS_DISABLE,
			(data32 >> 19) & 0x1, port);
		data32 = phy_misc_t6x_21[idx][2];
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, LDO_VTUNE,
			(data32 >> 8) & 0xf, port);
		if (log_level & PHY_LOG)
			rx_pr("phy bwth-%d\n", idx);
	}
	//config pll
	clk_rate = rx_get_scdc_clkrate_sts(port);
	if (log_level & PHY_LOG)
		rx_pr("clk rate = %d\n", clk_rate);
	//rx_clkmsr_handler();
	idx = aml_phy_pll_band(rx[port].clk.cable_clk, clk_rate, port);
	if (log_level & PHY_LOG)
		rx_pr("idx = %d\n", idx);
	if (f_rate == FRL_OFF) {
		do {
			if (idx == PLL_BW_0)
				t6x_480p_pll_cfg_21(port);
			if (idx == PLL_BW_1)
				t6x_720p_pll_cfg_21(port);
			if (idx == PLL_BW_2)
				t6x_1080p_pll_cfg_21(port);
			if (idx == PLL_BW_3)
				t6x_4k30_pll_cfg_21(port);
			if (idx == PLL_BW_4)
				t6x_4k60_pll_cfg_21(clk_rate, port);
			if (log_level & PHY_LOG)
				rx_pr("PLL0=0x%x\n",
					hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, port));
			if (pll_rst_cnt++ > pll_rst_max) {
				if (log_level & PHY_LOG)
					rx_pr("pll rst error\n");
				break;
			}
			if (log_level & PHY_LOG) {
				rx_pr("sq=%d,pll_lock=%d",
					hdmirx_rd_top(TOP_MISC_STAT0_T6X, port) & 0x1,
					is_pll_lock_t6x(port));
			}
			usleep_range(50, 100);
		} while (!is_tmds_clk_stable(port) && is_clk_stable(port) &&
			!aml_phy_pll_lock(port));
	}
	//for debug
	if (rx_info.aml_phy_21.vga_gain <= 0xffff)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, VGA_GAIN,
			decimaltogray_t6x(rx_info.aml_phy_21.vga_gain), port);
	if (rx_info.aml_phy_21.eq_stg1 < 0x1f) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_VAL1,
			rx_info.aml_phy_21.eq_stg1, port);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_EN,
			0x1, port);
	}
	if (rx_info.aml_phy_21.eq_stg2 <= 0x7)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, BUF_BST,
			rx_info.aml_phy_21.eq_stg2, port);
	if (rx_info.aml_phy_21.eq_pole <= 0x7)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, LEQ_POLE,
			rx_info.aml_phy_21.eq_pole, port);
	if (rx_info.aml_phy_21.buf_gain <= 0x7)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, BUF_GAIN,
			rx_info.aml_phy_21.buf_gain, port);
	if (rx_info.aml_phy_21.cdr_ph_div <= 0x7)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_PH_DIV,
			rx_info.aml_phy_21.cdr_ph_div, port);
	if (rx_info.aml_phy_21.cdr_pi_ofst <= 0x3f)
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_PI_OFST,
			rx_info.aml_phy_21.cdr_pi_ofst, port);
}

void rx_21_frl_pll_cfg_t6x(int f_rate, u8 port)
{
	if (log_level & FRL_LOG)
		rx_pr("port-%d f_rate=%d\n", port, f_rate);

	/* some changes compared with t3x: no pll_ctrl2/3/4, settings changed */
	if (f_rate == FRL_3G_3LANE) {
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa00, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014400a4 |
			(rx_info.aml_phy_21.pll_bw_21 << 8), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa01, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa03, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4520fa07, port);
	} else if (f_rate == FRL_6G_3LANE || f_rate == FRL_6G_4LANE) {
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa00, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014400a4 |
			(rx_info.aml_phy_21.pll_bw_21 << 8), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa01, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa03, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4520fa07, port);
	} else if (f_rate == FRL_8G_4LANE) {
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x052aa600, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014400a4 |
			(rx_info.aml_phy_21.pll_bw_21 << 8), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x052aa601, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x052aa603, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x452aa607, port);
	} else if (f_rate == FRL_10G_4LANE) {
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0522d000, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014400a4 |
			(rx_info.aml_phy_21.pll_bw_21 << 8), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0522d001, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0522d003, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4522d007, port);
	} else if (f_rate == FRL_12G_4LANE) {
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa00, port);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014400a4 |
			(rx_info.aml_phy_21.pll_bw_21 << 8), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa01, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa03, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4520fa07, port);
	}
	usleep_range(100, 110);
}

void rx_21_eq_get_val_t6x(u32 *ch0, u32 *ch1, u32 *ch2, u32 *ch3, u8 port)
{
	u32 data32;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	*ch0 = data32 & 0x1f;
	*ch1 = (data32 >> 8) & 0x1f;
	*ch2 = (data32 >> 16) & 0x1f;
	*ch3 = (data32 >> 24) & 0x1f;
}

void rx_21_eq_retry_t6x(u8 port)
{
	u32 eq_boost0 = 0;
	u32 eq_boost1 = 0;
	u32 eq_boost2 = 0;
	u32 eq_boost3 = 0;
	int idx = rx[port].phy.pll_bw;

	if (rx[port].var.frl_rate)
		idx = rx[port].var.frl_rate + 5;
	if (idx >= 3) {
		if (rx_info.aml_phy_21.eq_hold)
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_MODE,
			rx_info.aml_phy_21.eq_hold, port);
		rx_21_eq_get_val_t6x(&eq_boost0, &eq_boost1, &eq_boost2, &eq_boost3, port);
		rx_pr("eq:0x%x-0x%x-0x%x-0x%x\n", eq_boost0, eq_boost1,
			eq_boost2, eq_boost3);
		if  (rx_info.aml_phy_21.eq_retry) {
			if (eq_boost0 <= 2 || eq_boost0 == 31 ||
				eq_boost1 <= 2 || eq_boost1 == 31 ||
				eq_boost2 <= 2 || eq_boost2 == 31 ||
				eq_boost3 <= 2 || eq_boost3 == 31) {
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ,
					EQ_MODE, 0x0, port);
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ,
					EQ_RSTB, 0x0, port);
				usleep_range(10, 20);
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ,
					EQ_RSTB, 0x1, port);
				mdelay(10);
				if (rx_info.aml_phy_21.eq_hold)
					hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_MODE,
						rx_info.aml_phy_21.eq_hold, port);
				rx_21_eq_get_val_t6x(&eq_boost0, &eq_boost1,
					&eq_boost2, &eq_boost3, port);
				rx_pr("eq_retry:0x%x-0x%x-0x%x-0x%x\n", eq_boost0, eq_boost1,
					eq_boost2, eq_boost3);
			}
		}
	}
}

void rx_21_dfe_en_t6x(u8 port)
{
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_RSTB, 0x0, port);
	usleep_range(10, 20);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_RSTB, 0x1, port);
	usleep_range(100, 110);
	if (rx_info.aml_phy_21.dfe_hold) {
		usleep_range(1000, 1100);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_HOLD, 0x1, port);
	}
	if (log_level & PHY_LOG)
		rx_pr("dfe en\n");
}

static int convert_signed_value(u32 raw_val, u8 sign_bit_idx)
{
	u32 sign_mask = 1 << sign_bit_idx;
	u32 value_mask = sign_mask - 1;

	if (raw_val & sign_mask)
		return -(raw_val & value_mask);
	else
		return raw_val & value_mask;
}

void rx_21_eq_adjust(u8 port)
{
	u32 data32;
	u32 ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1, ch3_eq_boost1;
	int dfe0_tap2, dfe1_tap2, dfe2_tap2, dfe3_tap2;
	int dfe0_tap3, dfe1_tap3, dfe2_tap3, dfe3_tap3;
	int dfe0_tap4, dfe1_tap4, dfe2_tap4, dfe3_tap4;
	u32 ch_flag = 0;
	int normal_ch_count = 0;
	int eq_boost1_sum = 0;
	int eq_boost1_avg = 0;

	/* eq_boost1 status */
	/* mux_eye_en */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	/* mux_block_sel */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	ch0_eq_boost1 = data32 & 0x1f;
	ch1_eq_boost1 = (data32 >> 8) & 0x1f;
	ch2_eq_boost1 = (data32 >> 16) & 0x1f;
	ch3_eq_boost1 = (data32 >> 24) & 0x1f;

	/* dfe tap2 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x2, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap2 = convert_signed_value(data32 & 0x1f, 4);
	dfe1_tap2 = convert_signed_value((data32 >> 8) & 0x1f, 4);
	dfe2_tap2 = convert_signed_value((data32 >> 16) & 0x1f, 4);
	dfe3_tap2 = convert_signed_value((data32 >> 24) & 0x1f, 4);
	/* dfe tap3 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap3 = convert_signed_value(data32 & 0xf, 3);
	dfe1_tap3 = convert_signed_value((data32 >> 8) & 0xf, 3);
	dfe2_tap3 = convert_signed_value((data32 >> 16) & 0xf, 3);
	dfe3_tap3 = convert_signed_value((data32 >> 24) & 0xf, 3);
	/* dfe tap4 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x4, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap4 = convert_signed_value(data32 & 0xf, 3);
	dfe1_tap4 = convert_signed_value((data32 >> 8) & 0xf, 3);
	dfe2_tap4 = convert_signed_value((data32 >> 16) & 0xf, 3);
	dfe3_tap4 = convert_signed_value((data32 >> 24) & 0xf, 3);

	if (dfe0_tap2 > 0 && dfe0_tap2 + dfe0_tap3 + dfe0_tap4 >= 10 && ch0_eq_boost1 <= 10)
		ch_flag |= _BIT(0);
	if (dfe1_tap2 > 0 && dfe1_tap2 + dfe1_tap3 + dfe1_tap4 >= 10 && ch1_eq_boost1 <= 10)
		ch_flag |= _BIT(1);
	if (dfe2_tap2 > 0 && dfe2_tap2 + dfe2_tap3 + dfe2_tap4 >= 10 && ch2_eq_boost1 <= 10)
		ch_flag |= _BIT(2);
	if (dfe3_tap2 > 0 && dfe3_tap2 + dfe3_tap3 + dfe3_tap4 >= 10 && ch3_eq_boost1 <= 10)
		ch_flag |= _BIT(3);
	if (!(ch_flag & _BIT(0))) {
		eq_boost1_sum += ch0_eq_boost1;
		normal_ch_count++;
	}
	if (!(ch_flag & _BIT(1))) {
		eq_boost1_sum += ch1_eq_boost1;
		normal_ch_count++;
	}
	if (!(ch_flag & _BIT(2))) {
		eq_boost1_sum += ch2_eq_boost1;
		normal_ch_count++;
	}
	if (!(ch_flag & _BIT(3))) {
		eq_boost1_sum += ch3_eq_boost1;
		normal_ch_count++;
	}
	if (normal_ch_count > 0)
		eq_boost1_avg = eq_boost1_sum / normal_ch_count + 1;
	if (log_level & PHY_LOG) {
		rx_pr("dfe_tap2 %d %d %d %d\n", dfe0_tap2, dfe1_tap2, dfe2_tap2, dfe3_tap2);
		rx_pr("dfe_tap3 %d %d %d %d\n", dfe0_tap3, dfe1_tap3, dfe2_tap3, dfe3_tap3);
		rx_pr("dfe_tap4 %d %d %d %d\n", dfe0_tap4, dfe1_tap4, dfe2_tap4, dfe3_tap4);
		rx_pr("eq_boost1=[%d,%d,%d,%d]\n",
			ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1, ch3_eq_boost1);
		rx_pr("ch flag: 0x%x\n", ch_flag);
		rx_pr("normal_ch_count: 0x%x eq_boost1_avg =%d\n",
			normal_ch_count, eq_boost1_avg);
	}
	if (ch_flag) {
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_VAL1,
			eq_boost1_avg, port);
		usleep_range(10, 20);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_BYP_EN, 0x1, port);
	}
}

void rx_21_eq_cfg_t6x(int f_rate, u8 port)
{
	u32 cdr0_int, cdr1_int, cdr2_int, cdr3_int, data32;
	//if pll unlock
		//return;
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_FR_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_LKDET_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_MODE, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_RSTB, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_HOLD, 0x0, port);
	//hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, DFE_RSTB, 0x1, port);
	//hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_MODE, 0x1, port);
	usleep_range(10, 15);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_RSTB, 0x1, port);
	//hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_LKDET_EN, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, EQ_RSTB, 0x1, port);
	if (rx_info.aml_phy_21.cdr_fr_en) {
		usleep_range(rx_info.aml_phy_21.cdr_fr_en, rx_info.aml_phy_21.cdr_fr_en + 10);
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_FR_EN, 0x1, port);
	}
	usleep_range(10000, 10100);
	rx_21_eq_retry_t6x(port);
	if (rx_info.aml_phy_21.dfe_en)
		rx_21_dfe_en_t6x(port);
	if (rx[port].var.frl_rate == FRL_10G_4LANE &&
		rx_info.aml_phy_21.eq_adp_chk_en)
		rx_21_eq_adjust(port);
	if (rx_info.aml_phy_21.vga_tune &&
		(rx[port].var.frl_rate == FRL_12G_4LANE ||
		rx[port].var.frl_rate == FRL_10G_4LANE ||
		rx[port].var.frl_rate == FRL_8G_4LANE))
		hdmirx_vga_gain_tuning_t6x(port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, CDR_LKDET_EN, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x2, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	//hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MSK(4, 0), cdr_bw, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	cdr0_int = data32 & 0x7f;
	cdr1_int = (data32 >> 8) & 0x7f;
	cdr2_int = (data32 >> 16) & 0x7f;
	cdr3_int = (data32 >> 24) & 0x7f;
	if (log_level & FRL_LOG)
		rx_pr("cdr int=0x%x-0x%x-0x%x-0x%x\n", cdr0_int, cdr1_int, cdr2_int, cdr3_int);
}

void rx_21_dump_fpll_0_t6x(void)
{
	rx_pr("PLL0_CTRL0=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0));
	rx_pr("PLL0_CTRL1=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL1));
	rx_pr("PLL0_CTRL2=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL2));
	rx_pr("PLL0_CTRL3=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL3));
}

void rx_21_dump_fpll_1_t6x(void)
{
	rx_pr("PLL1_CTRL0=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0));
	rx_pr("PLL1_CTRL1=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL1));
	rx_pr("PLL1_CTRL2=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL2));
	rx_pr("PLL1_CTRL3=0x%x\n", rd_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL3));
}

void hdmirx_fpll_recovery_t6x(u8 port)
{
	if (port == E_PORT2) {
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0, 0x21280035);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0, 0x30280035);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL1, 0x83afa82a);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL2, 0x00040020);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL3, 0x0b09a001);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0, 0x10280035);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL3, 0x0b09a201);
	} else {
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0, 0x21280035);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0, 0x30280035);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL1, 0x83afa82a);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL2, 0x00040020);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL3, 0x0b09a001);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0, 0x10280035);
		wr_reg_ana_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL3, 0x0b09a201);
	}
}

 /* aml_hyper_gain_tuning */
void aml_hyper_gain_tuning_t6x_21(u8 port)
{
	u32 data32;
	u32 tap0, tap1, tap2, tap3;
	u32 hyper_gain_0, hyper_gain_1, hyper_gain_2, hyper_gain_3;

	/* use HYPER_GAIN calibration instead of vga */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);

	tap0 = data32 & 0x7f;
	tap1 = (data32 >> 8) & 0x7f;
	tap2 = (data32 >> 16) & 0x7f;
	tap3 = (data32 >> 24) & 0x7f;
	if (tap0 < 0x12) {
		hyper_gain_0 = 1;
		rx_pr("ch0 amp is too small\n");
	} else {
		hyper_gain_0 = 0;
	}
	if (tap1 < 0x12) {
		hyper_gain_1 = 1;
		rx_pr("ch1 amp is too small\n");
	} else {
		hyper_gain_1 = 0;
	}
	if (tap2 < 0x12) {
		hyper_gain_2 = 1;
		rx_pr("ch2 amp is too small\n");
	} else {
		hyper_gain_2 = 0;
	}
	if (tap3 < 0x12) {
		hyper_gain_3 = 1;
		rx_pr("ch3 amp is too small\n");
	} else {
		hyper_gain_3 = 0;
	}
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, HYPER_GAIN,
	hyper_gain_0 | (hyper_gain_1 << 1) |
	(hyper_gain_2 << 2) | (hyper_gain_3 << 3), port);
}

 /* For t6x */
void aml_phy_init_t6x_20(u8 port)
{
	if (rx[port].state == FSM_WAIT_CLK_STABLE && !rx[port].cableclk_stb_flg) {
		aml_phy_cfg_t6x_20(port);
		return;
	}
	aml_phy_cfg_t6x_20(port);
	usleep_range(10, 20);
	aml_pll_bw_cfg_t6x_20(port);
	usleep_range(10, 20);
	aml_eq_cfg_t6x_20(port);
}

void aml_phy_init_t6x_21(u8 port)
{
	rx_21_frl_phy_cfg_t6x(port);
	aml_pll_bw_cfg_t6x_21(rx[port].var.frl_rate, port);
	if (rx[port].state <= FSM_FRL_FLT_READY || rx[port].state == FSM_WAIT_CLK_STABLE ||
		(!rx[port].var.frl_rate && rx[port].clk.cable_clk < TMDS_CLK_MIN * KHz))
		return;
	rx_21_frl_pll_cfg_t6x(rx[port].var.frl_rate, port);
	rx_21_eq_cfg_t6x(rx[port].var.frl_rate, port);
}

void aml_phy_init_t6x(u8 port)
{
	if (port <= E_PORT1)
		aml_phy_init_t6x_20(port);
	else
		aml_phy_init_t6x_21(port);
}

void dump_reg_phy_t6x_20(u8 port)
{
	rx_pr("2.0 PHY Register:\n");
	rx_pr("dchd_eq-0x14=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, port));
	rx_pr("dchd_cdr-0x10=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, port));
	rx_pr("dcha_dfe-0xc=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, port));
	rx_pr("dcha_afe-0x8=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, port));
	rx_pr("misc2-0x1c=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, port));
	rx_pr("misc1-0x18=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, port));
	rx_pr("pll0-0x0=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, port));
	rx_pr("pll1-0x4=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PLL_CTRL1, port));
}

void dump_reg_phy_t6x_21(u8 port)
{
	rx_pr("2.1 PHY Register:\n");
	rx_pr("RX21PLL_CTRL0-0x100=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, port));
	rx_pr("RX21PLL_CTRL1-0x104=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, port));
	rx_pr("RX21PHY_MISC0-0x114=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, port));
	rx_pr("RX21PHY_MISC1-0x118=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, port));
	rx_pr("RX21PHY_MISC2-0x11c=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, port));
	rx_pr("RX21PHY_DCHA_AFE-0x120=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, port));
	rx_pr("RX21PHY_DCHA_DFE-0x124=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, port));
	rx_pr("RX21PHY_DCHA_PI-0x128=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, port));
	rx_pr("RX21PHY_DCHA_CTRL-0x12c=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, port));
	rx_pr("RX21PHY_DCHD_CDR-0x130=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, port));
	rx_pr("RX21PHY_DCHD_EQ-0x134=0x%x\n",
		hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, port));
}

void dump_reg_phy_t6x(u8 port)
{
	if (port <= E_PORT1) {
		dump_reg_phy_t6x_20(port);
	} else {
		dump_reg_phy_t6x_21(port);
		rx_21_dump_fpll(port);
	}
}

/*
 * rx phy v2 debug
 */
int count_one_bits_t6x(u32 value)
{
	int count = 0;

	for (; value != 0; value >>= 1) {
		if (value & 1)
			count++;
	}
	return count;
}

void get_val_t6x(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
}

void get_flag_val_t6x(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
}

void comb_val_t6x(void (*p)(char *, unsigned int, int),
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

void dump_aml_phy_sts_t6x_20(u8 port)
{
	u32 data32;
	u32 terminal;
	u32 ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1;
	u32 ch0_eq_err, ch1_eq_err, ch2_eq_err;
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0;
	u32 dfe0_tap1, dfe1_tap1, dfe2_tap1, dfe3_tap1;
	u32 dfe0_tap2, dfe1_tap2, dfe2_tap2, dfe3_tap2;
	u32 dfe0_tap3, dfe1_tap3, dfe2_tap3, dfe3_tap3;
	u32 dfe0_tap4, dfe1_tap4, dfe2_tap4, dfe3_tap4;
	u32 dfe0_tap5, dfe1_tap5, dfe2_tap5, dfe3_tap5;
	u32 dfe0_tap6, dfe1_tap6, dfe2_tap6, dfe3_tap6;
	u32 dfe0_tap7, dfe1_tap7, dfe2_tap7, dfe3_tap7;
	u32 dfe0_tap8, dfe1_tap8, dfe2_tap8, dfe3_tap8;

	u32 cdr0_lock, cdr1_lock, cdr2_lock;
	u32 cdr0_int, cdr1_int, cdr2_int;
	u32 cdr0_code, cdr1_code, cdr2_code;

	bool pll_lock;
	bool squelch;

	u32 sli0_ofst0, sli1_ofst0, sli2_ofst0;
	u32 sli0_ofst1, sli1_ofst1, sli2_ofst1;
	u32 sli0_ofst2, sli1_ofst2, sli2_ofst2;

	/* rterm */
	terminal = (hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, RTERM_VAL_T6X_20, port));

	/* eq_boost1 status */
	/* mux_eye_en */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	/* mux_block_sel */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	ch0_eq_boost1 = data32 & 0x1f;
	ch0_eq_err = (data32 >> 5) & 0x3;
	ch1_eq_boost1 = (data32 >> 8) & 0x1f;
	ch1_eq_err = (data32 >> 13) & 0x3;
	ch2_eq_boost1 = (data32 >> 16) & 0x1f;
	ch2_eq_err = (data32 >> 21) & 0x3;

	/* dfe tap0 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap0 = data32 & 0x7f;
	dfe1_tap0 = (data32 >> 8) & 0x7f;
	dfe2_tap0 = (data32 >> 16) & 0x7f;
	dfe3_tap0 = (data32 >> 24) & 0x7f;
	/* dfe tap1 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap1 = data32 & 0x3f;
	dfe1_tap1 = (data32 >> 8) & 0x3f;
	dfe2_tap1 = (data32 >> 16) & 0x3f;
	dfe3_tap1 = (data32 >> 24) & 0x3f;
	/* dfe tap2 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x2, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap2 = data32 & 0x1f;
	dfe1_tap2 = (data32 >> 8) & 0x1f;
	dfe2_tap2 = (data32 >> 16) & 0x1f;
	dfe3_tap2 = (data32 >> 24) & 0x1f;
	/* dfe tap3 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap3 = data32 & 0xf;
	dfe1_tap3 = (data32 >> 8) & 0xf;
	dfe2_tap3 = (data32 >> 16) & 0xf;
	dfe3_tap3 = (data32 >> 24) & 0xf;
	/* dfe tap4 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x4, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap4 = data32 & 0xf;
	dfe1_tap4 = (data32 >> 8) & 0xf;
	dfe2_tap4 = (data32 >> 16) & 0xf;
	dfe3_tap4 = (data32 >> 24) & 0xf;
	/* dfe tap5 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x5, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap5 = data32 & 0xf;
	dfe1_tap5 = (data32 >> 8) & 0xf;
	dfe2_tap5 = (data32 >> 16) & 0xf;
	dfe3_tap5 = (data32 >> 24) & 0xf;
	/* dfe tap6 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x6, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap6 = data32 & 0xf;
	dfe1_tap6 = (data32 >> 8) & 0xf;
	dfe2_tap6 = (data32 >> 16) & 0xf;
	dfe3_tap6 = (data32 >> 24) & 0xf;
	/* dfe tap7/8 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x7, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap7 = data32 & 0xf;
	dfe1_tap7 = (data32 >> 8) & 0xf;
	dfe2_tap7 = (data32 >> 16) & 0xf;
	dfe3_tap7 = (data32 >> 24) & 0xf;
	dfe0_tap8 = (data32 >> 4) & 0xf;
	dfe1_tap8 = (data32 >> 12) & 0xf;
	dfe2_tap8 = (data32 >> 20) & 0xf;
	dfe3_tap8 = (data32 >> 24) & 0xf;

	/* CDR status */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x22, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_MUX_CDR_DBG_SEL, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	cdr0_int = data32 & 0x7f;
	cdr1_int = (data32 >> 8) & 0x7f;
	cdr2_int = (data32 >> 16) & 0x7f;

	/* pll lock */
	pll_lock = hdmirx_rd_amlphy_t6x(T6X_RG_RX20PLL_0, port) >> 31;

	/* squelch */
	squelch = hdmirx_rd_top(TOP_MISC_STAT0, port) & 0x1;

	/* slicer offset status */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	sli0_ofst0 = data32 & 0x1f;
	sli1_ofst0 = (data32 >> 8) & 0x1f;
	sli2_ofst0 = (data32 >> 16) & 0x1f;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	sli0_ofst1 = data32 & 0x1f;
	sli1_ofst1 = (data32 >> 8) & 0x1f;
	sli2_ofst1 = (data32 >> 16) & 0x1f;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x2, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	sli0_ofst2 = data32 & 0x1f;
	sli1_ofst2 = (data32 >> 8) & 0x1f;
	sli2_ofst2 = (data32 >> 16) & 0x1f;

	rx_pr("\nhdmirx phy status:\n");
	rx_pr("pll_lock=%d, squelch=%d, terminal=%d\n", pll_lock, squelch, terminal);
	rx_pr("eq_boost1=[%d,%d,%d]\n",
	      ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1);
	rx_pr("eq_err=[%d,%d,%d]\n",
	      ch0_eq_err, ch1_eq_err, ch2_eq_err);

	comb_val_t6x(get_val_t6x, "dfe_tap0", dfe0_tap0, dfe1_tap0, dfe2_tap0, 7);
	comb_val_t6x(get_val_t6x, "dfe_tap1", dfe0_tap1, dfe1_tap1, dfe2_tap1, 6);
	comb_val_t6x(get_val_t6x, "dfe_tap2", dfe0_tap2, dfe1_tap2, dfe2_tap2, 5);
	comb_val_t6x(get_val_t6x, "dfe_tap3", dfe0_tap3, dfe1_tap3, dfe2_tap3, 4);
	comb_val_t6x(get_val_t6x, "dfe_tap4", dfe0_tap4, dfe1_tap4, dfe2_tap4, 4);
	comb_val_t6x(get_val_t6x, "dfe_tap5", dfe0_tap5, dfe1_tap5, dfe2_tap5, 4);
	comb_val_t6x(get_val_t6x, "dfe_tap6", dfe0_tap6, dfe1_tap6, dfe2_tap6, 4);
	comb_val_t6x(get_val_t6x, "dfe_tap7", dfe0_tap7, dfe1_tap7, dfe2_tap7, 4);
	comb_val_t6x(get_val_t6x, "dfe_tap8", dfe0_tap8, dfe1_tap8, dfe2_tap8, 4);

	comb_val_t6x(get_val_t6x, "slicer_ofst0", sli0_ofst0, sli1_ofst0, sli2_ofst0, 5);
	comb_val_t6x(get_val_t6x, "slicer_ofst1", sli0_ofst1, sli1_ofst1, sli2_ofst1, 5);
	comb_val_t6x(get_val_t6x, "slicer_ofst2", sli0_ofst2, sli1_ofst2, sli2_ofst2, 5);

	rx_pr("cdr_code=[%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code);
	rx_pr("cdr_lock=[%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock);
	comb_val_t6x(get_val_t6x, "cdr_int", cdr0_int, cdr1_int, cdr2_int, 7);
}

void get_val_21_t6x(char *temp, unsigned int val, int len)
{
	if ((val >> (len - 1)) == 0)
		sprintf(temp, "+%d", val & (~(1 << (len - 1))));
	else
		sprintf(temp, "-%d", val & (~(1 << (len - 1))));
}

void comb_val_21_t6x(char *type, unsigned int val_0, unsigned int val_1,
		 unsigned int val_2, unsigned int val_3, int len)
{
	char out[32], v0_buf[16], v1_buf[16], v2_buf[16], v3_buf[16];
	int pos = 0;

	get_val_21_t6x(v0_buf, val_0, len);
	get_val_21_t6x(v1_buf, val_1, len);
	get_val_21_t6x(v2_buf, val_2, len);
	get_val_21_t6x(v3_buf, val_3, len);
	pos += snprintf(out + pos, 32 - pos, "%s[", type);
	pos += snprintf(out + pos, 32 - pos, " %s,", v0_buf);
	pos += snprintf(out + pos, 32 - pos, " %s,", v1_buf);
	pos += snprintf(out + pos, 32 - pos, " %s,", v2_buf);
	pos += snprintf(out + pos, 32 - pos, " %s]", v3_buf);
	rx_pr("%s\n", out);
}

void dump_aml_phy_sts_t6x_21(u8 port)
{
	u32 data32;
	u32 vga_ch0, vga_ch1, vga_ch2, vga_ch3;
	u32 ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1, ch3_eq_boost1;
	u32 ch0_eq_err, ch1_eq_err, ch2_eq_err, ch3_eq_err;
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0;
	u32 dfe0_tap1, dfe1_tap1, dfe2_tap1, dfe3_tap1;
	u32 dfe0_tap2, dfe1_tap2, dfe2_tap2, dfe3_tap2;
	u32 dfe0_tap3, dfe1_tap3, dfe2_tap3, dfe3_tap3;
	u32 dfe0_tap4, dfe1_tap4, dfe2_tap4, dfe3_tap4;

	u32 cdr0_lock, cdr1_lock, cdr2_lock, cdr3_lock;
	u32 cdr0_int, cdr1_int, cdr2_int, cdr3_int;
	u32 cdr0_code, cdr1_code, cdr2_code, cdr3_code;
	bool pll_lock, lan0_lock, lan1_lock, lan2_lock, lan3_lock;
	bool squelch;

	u32 sli1_ofst3, sli0_ofst3, sli1_ofst2, sli0_ofst2;
	u32 sli3_ofst3, sli2_ofst3, sli3_ofst2, sli2_ofst2;
	u32 sli5_ofst3, sli4_ofst3, sli5_ofst2, sli4_ofst2;
	u32 sli7_ofst3, sli6_ofst3, sli7_ofst2, sli6_ofst2;
	u32 sli9_ofst3, sli8_ofst3, sli9_ofst2, sli8_ofst2;

	u32 sli1_ofst1, sli0_ofst1, sli1_ofst0, sli0_ofst0;
	u32 sli3_ofst1, sli2_ofst1, sli3_ofst0, sli2_ofst0;
	u32 sli5_ofst1, sli4_ofst1, sli5_ofst0, sli4_ofst0;
	u32 sli7_ofst1, sli6_ofst1, sli7_ofst0, sli6_ofst0;
	u32 sli9_ofst1, sli8_ofst1, sli9_ofst0, sli8_ofst0;

	u32 afe_ofst0, afe_ofst1, afe_ofst2, afe_ofst3;
	u8 sign_sli_ofst3, sign_sli_ofst2, sign_sli_ofst1, sign_sli_ofst0;
	u8 sign_sli_ofst3_h, sign_sli_ofst2_h, sign_sli_ofst1_h, sign_sli_ofst0_h;
	u8 sign_afe_ofst3, sign_afe_ofst2, sign_afe_ofst1, sign_afe_ofst0;
	/* eq_boost1 status */
	/* mux_eye_en */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	/* mux_block_sel */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	ch0_eq_boost1 = data32 & 0x1f;
	ch0_eq_err = (data32 >> 5) & 0x3;
	ch1_eq_boost1 = (data32 >> 8) & 0x1f;
	ch1_eq_err = (data32 >> 13) & 0x3;
	ch2_eq_boost1 = (data32 >> 16) & 0x1f;
	ch2_eq_err = (data32 >> 21) & 0x3;
	ch3_eq_boost1 = (data32 >> 24) & 0x1f;
	ch3_eq_err = (data32 >> 29) & 0x3;

	vga_ch0 = graytodecimal_t6x(hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
		MSK(4, 0), port));
	vga_ch1 = graytodecimal_t6x(hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
		MSK(4, 4), port));
	vga_ch2 = graytodecimal_t6x(hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
		MSK(4, 8), port));
	vga_ch3 = graytodecimal_t6x(hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
		MSK(4, 12), port));
	/* dfe tap0 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap0 = data32 & 0x7f;
	dfe1_tap0 = (data32 >> 8) & 0x7f;
	dfe2_tap0 = (data32 >> 16) & 0x7f;
	dfe3_tap0 = (data32 >> 24) & 0x7f;
	/* dfe tap1 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap1 = data32 & 0x3f;
	dfe1_tap1 = (data32 >> 8) & 0x3f;
	dfe2_tap1 = (data32 >> 16) & 0x3f;
	dfe3_tap1 = (data32 >> 24) & 0x3f;
	/* dfe tap2 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x2, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap2 = data32 & 0x1f;
	dfe1_tap2 = (data32 >> 8) & 0x1f;
	dfe2_tap2 = (data32 >> 16) & 0x1f;
	dfe3_tap2 = (data32 >> 24) & 0x1f;
	/* dfe tap3 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x3, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap3 = data32 & 0xf;
	dfe1_tap3 = (data32 >> 8) & 0xf;
	dfe2_tap3 = (data32 >> 16) & 0xf;
	dfe3_tap3 = (data32 >> 24) & 0xf;
	/* dfe tap4 sts */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x4, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap4 = data32 & 0xf;
	dfe1_tap4 = (data32 >> 8) & 0xf;
	dfe2_tap4 = (data32 >> 16) & 0xf;
	dfe3_tap4 = (data32 >> 24) & 0xf;

	/* CDR status */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x2, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	cdr0_code = data32 & 0x7f;
	cdr0_lock = (data32 >> 7) & 0x1;
	cdr1_code = (data32 >> 8) & 0x7f;
	cdr1_lock = (data32 >> 15) & 0x1;
	cdr2_code = (data32 >> 16) & 0x7f;
	cdr2_lock = (data32 >> 23) & 0x1;
	cdr3_code = (data32 >> 24) & 0x7f;
	cdr3_lock = (data32 >> 31) & 0x1;
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	cdr0_int = data32 & 0x7f;
	cdr1_int = (data32 >> 8) & 0x7f;
	cdr2_int = (data32 >> 16) & 0x7f;
	cdr3_int = (data32 >> 24) & 0x7f;

	/* pll lock */
	pll_lock = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, port) >> 31;

	/* squelch */
	squelch = hdmirx_rd_top(TOP_MISC_STAT0, port) & 0x1;

	/* slicer offset status */
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	sli0_ofst0 = data32 & 0xf;
	sli1_ofst0 = (data32 >> 4) & 0xf;
	sli0_ofst1 = (data32 >> 8) & 0xf;
	sli1_ofst1 = (data32 >> 12) & 0xf;
	sli0_ofst2 = (data32 >> 16) & 0xf;
	sli1_ofst2 = (data32 >> 20) & 0xf;
	sli0_ofst3 = (data32 >> 24) & 0xf;
	sli1_ofst3 = (data32 >> 28) & 0xf;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	sli2_ofst0 = data32 & 0xf;
	sli3_ofst0 = (data32 >> 4) & 0xf;
	sli2_ofst1 = (data32 >> 8) & 0xf;
	sli3_ofst1 = (data32 >> 12) & 0xf;
	sli2_ofst2 = (data32 >> 16) & 0xf;
	sli3_ofst2 = (data32 >> 20) & 0xf;
	sli2_ofst3 = (data32 >> 24) & 0xf;
	sli3_ofst3 = (data32 >> 28) & 0xf;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x2, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	sli4_ofst0 = data32 & 0xf;
	sli5_ofst0 = (data32 >> 4) & 0xf;
	sli4_ofst1 = (data32 >> 8) & 0xf;
	sli5_ofst1 = (data32 >> 12) & 0xf;
	sli4_ofst2 = (data32 >> 16) & 0xf;
	sli5_ofst2 = (data32 >> 20) & 0xf;
	sli4_ofst3 = (data32 >> 24) & 0xf;
	sli5_ofst3 = (data32 >> 28) & 0xf;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x3, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	sli6_ofst0 = data32 & 0xf;
	sli7_ofst0 = (data32 >> 4) & 0xf;
	sli6_ofst1 = (data32 >> 8) & 0xf;
	sli7_ofst1 = (data32 >> 12) & 0xf;
	sli6_ofst2 = (data32 >> 16) & 0xf;
	sli7_ofst2 = (data32 >> 20) & 0xf;
	sli6_ofst3 = (data32 >> 24) & 0xf;
	sli7_ofst3 = (data32 >> 28) & 0xf;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x4, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	sli8_ofst0 = data32 & 0xf;
	sli9_ofst0 = (data32 >> 4) & 0xf;
	sli8_ofst1 = (data32 >> 8) & 0xf;
	sli9_ofst1 = (data32 >> 12) & 0xf;
	sli8_ofst2 = (data32 >> 16) & 0xf;
	sli9_ofst2 = (data32 >> 20) & 0xf;
	sli8_ofst3 = (data32 >> 24) & 0xf;
	sli9_ofst3 = (data32 >> 28) & 0xf;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x5, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	afe_ofst0 = data32 & 0xf;
	afe_ofst1 = (data32 >> 8) & 0xf;
	afe_ofst2 = (data32 >> 16) & 0xf;
	afe_ofst3 = (data32 >> 24) & 0xf;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x6, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	sign_sli_ofst0 = data32 & 0xff;
	sign_sli_ofst1 = (data32 >> 8) & 0xff;
	sign_sli_ofst2 = (data32 >> 16) & 0xff;
	sign_sli_ofst3 = (data32 >> 24) & 0xff;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x7, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_CDR_DBG_SEL, 0x1, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	sign_sli_ofst0_h = data32 & 0x3;
	sign_afe_ofst0 = (data32 >> 2) & 0x1;
	sign_sli_ofst1_h = (data32 >> 8) & 0x3;
	sign_afe_ofst1 = (data32 >> 10) & 0x1;
	sign_sli_ofst2_h = (data32 >> 16) & 0x3;
	sign_afe_ofst2 = (data32 >> 18) & 0x1;
	sign_sli_ofst3_h = (data32 >> 24) & 0x3;
	sign_afe_ofst3 = (data32 >> 26) & 0x1;

	sli0_ofst0 = (sign_sli_ofst0 & 0x1)  ? -sli0_ofst0 : sli0_ofst0;
	sli1_ofst0 = ((sign_sli_ofst0 >> 1) & 0x1) ? -sli1_ofst0 : sli1_ofst0;
	sli2_ofst0 = ((sign_sli_ofst0 >> 2) & 0x1) ? -sli2_ofst0 : sli2_ofst0;
	sli3_ofst0 = ((sign_sli_ofst0 >> 3) & 0x1) ? -sli3_ofst0 : sli3_ofst0;
	sli4_ofst0 = ((sign_sli_ofst0 >> 4) & 0x1) ? -sli4_ofst0 : sli4_ofst0;
	sli5_ofst0 = ((sign_sli_ofst0 >> 5) & 0x1) ? -sli5_ofst0 : sli5_ofst0;
	sli6_ofst0 = ((sign_sli_ofst0 >> 6) & 0x1) ? -sli6_ofst0 : sli6_ofst0;
	sli7_ofst0 = ((sign_sli_ofst0 >> 7) & 0x1) ? -sli7_ofst0 : sli7_ofst0;
	sli8_ofst0 = (sign_sli_ofst0_h & 0x1) ? -sli8_ofst0 : sli8_ofst0;
	sli9_ofst0 = ((sign_sli_ofst0 >> 1) & 0x1) ? -sli9_ofst0 : sli9_ofst0;
	afe_ofst0 = sign_afe_ofst0 ? -afe_ofst0 : afe_ofst0;

	sli0_ofst1 = (sign_sli_ofst1 & 0x1)  ? -sli0_ofst1 : sli0_ofst1;
	sli1_ofst1 = ((sign_sli_ofst1 >> 1) & 0x1) ? -sli1_ofst1 : sli1_ofst1;
	sli2_ofst1 = ((sign_sli_ofst1 >> 2) & 0x1) ? -sli2_ofst1 : sli2_ofst1;
	sli3_ofst1 = ((sign_sli_ofst1 >> 3) & 0x1) ? -sli3_ofst1 : sli3_ofst1;
	sli4_ofst1 = ((sign_sli_ofst1 >> 4) & 0x1) ? -sli4_ofst1 : sli4_ofst1;
	sli5_ofst1 = ((sign_sli_ofst1 >> 5) & 0x1) ? -sli5_ofst1 : sli5_ofst1;
	sli6_ofst1 = ((sign_sli_ofst1 >> 6) & 0x1) ? -sli6_ofst1 : sli6_ofst1;
	sli7_ofst1 = ((sign_sli_ofst1 >> 7) & 0x1) ? -sli7_ofst1 : sli7_ofst1;
	sli8_ofst1 = (sign_sli_ofst1_h & 0x1) ? -sli8_ofst1 : sli8_ofst1;
	sli9_ofst1 = ((sign_sli_ofst1 >> 1) & 0x1) ? -sli9_ofst1 : sli9_ofst1;
	afe_ofst1 = sign_afe_ofst1 ? -afe_ofst1 : afe_ofst1;

	sli0_ofst2 = (sign_sli_ofst2 & 0x1)  ? -sli0_ofst2 : sli0_ofst2;
	sli1_ofst2 = ((sign_sli_ofst2 >> 1) & 0x1) ? -sli1_ofst2 : sli1_ofst2;
	sli2_ofst2 = ((sign_sli_ofst2 >> 2) & 0x1) ? -sli2_ofst2 : sli2_ofst2;
	sli3_ofst2 = ((sign_sli_ofst2 >> 3) & 0x1) ? -sli3_ofst2 : sli3_ofst2;
	sli4_ofst2 = ((sign_sli_ofst2 >> 4) & 0x1) ? -sli4_ofst2 : sli4_ofst2;
	sli5_ofst2 = ((sign_sli_ofst2 >> 5) & 0x1) ? -sli5_ofst2 : sli5_ofst2;
	sli6_ofst2 = ((sign_sli_ofst2 >> 6) & 0x1) ? -sli6_ofst2 : sli6_ofst2;
	sli7_ofst2 = ((sign_sli_ofst2 >> 7) & 0x1) ? -sli7_ofst2 : sli7_ofst2;
	sli8_ofst2 = (sign_sli_ofst2_h & 0x1) ? -sli8_ofst2 : sli8_ofst2;
	sli9_ofst2 = ((sign_sli_ofst2 >> 1) & 0x1) ? -sli9_ofst2 : sli9_ofst2;
	afe_ofst2 = sign_afe_ofst2 ? -afe_ofst2 : afe_ofst2;

	sli0_ofst3 = (sign_sli_ofst3 & 0x1)  ? -sli0_ofst3 : sli0_ofst3;
	sli1_ofst3 = ((sign_sli_ofst3 >> 1) & 0x1) ? -sli1_ofst3 : sli1_ofst3;
	sli2_ofst3 = ((sign_sli_ofst3 >> 2) & 0x1) ? -sli2_ofst3 : sli2_ofst3;
	sli3_ofst3 = ((sign_sli_ofst3 >> 3) & 0x1) ? -sli3_ofst3 : sli3_ofst3;
	sli4_ofst3 = ((sign_sli_ofst3 >> 4) & 0x1) ? -sli4_ofst3 : sli4_ofst3;
	sli5_ofst3 = ((sign_sli_ofst3 >> 5) & 0x1) ? -sli5_ofst3 : sli5_ofst3;
	sli6_ofst3 = ((sign_sli_ofst3 >> 6) & 0x1) ? -sli6_ofst3 : sli6_ofst3;
	sli7_ofst3 = ((sign_sli_ofst3 >> 7) & 0x1) ? -sli7_ofst3 : sli7_ofst3;
	sli8_ofst3 = (sign_sli_ofst3_h & 0x1) ? -sli8_ofst3 : sli8_ofst3;
	sli9_ofst3 = ((sign_sli_ofst3 >> 1) & 0x1) ? -sli9_ofst3 : sli9_ofst3;
	afe_ofst3 = sign_afe_ofst3 ? -afe_ofst3 : afe_ofst3;

	data32 = hdmirx_rd_top(TOP_LANE01_ERRCNT, port);
	lan0_lock = (data32 >> 15) & 1;
	lan1_lock = (data32 >> 31) & 1;
	data32 = hdmirx_rd_top(TOP_LANE23_ERRCNT, port);
	lan2_lock = (data32 >> 15) & 1;
	lan3_lock = (data32 >> 31) & 1;
	rx_pr("\nhdmirx phy status:\n");
	if (rx[port].var.frl_rate == FRL_OFF)
		;//rx_pr("data_rate=tmds-%d\n", tmds_clk_msr());
	else if (rx[port].var.frl_rate == FRL_3G_3LANE)
		rx_pr("data_rate=FRL_3G_3LANE\n");
	else if (rx[port].var.frl_rate == FRL_6G_3LANE)
		rx_pr("data_rate=FRL_6G_3LANE\n");
	else if (rx[port].var.frl_rate == FRL_6G_4LANE)
		rx_pr("data_rate=FRL_6G_4LANE\n");
	else if (rx[port].var.frl_rate == FRL_8G_4LANE)
		rx_pr("data_rate=FRL_8G_4LANE\n");
	else if (rx[port].var.frl_rate == FRL_10G_4LANE)
		rx_pr("data_rate=FRL_10G_4LANE\n");
	else if (rx[port].var.frl_rate == FRL_12G_4LANE)
		rx_pr("data_rate=FRL_12G_4LANE\n");
	rx_pr("pll_lock=%d, squelch=%d\n", pll_lock, squelch);

	rx_pr("vga_gain =[%d,%d,%d,%d]\n",
	      vga_ch0, vga_ch1, vga_ch2, vga_ch3);
	rx_pr("eq_boost1=[%d,%d,%d,%d]\n",
	      ch0_eq_boost1, ch1_eq_boost1, ch2_eq_boost1, ch3_eq_boost1);
	rx_pr("eq_err=[%d,%d,%d,%d]\n",
	      ch0_eq_err, ch1_eq_err, ch2_eq_err, ch3_eq_err);

	comb_val_21_t6x("dfe_tap0", dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0, 7);
	comb_val_21_t6x("dfe_tap1", dfe0_tap1, dfe1_tap1, dfe2_tap1, dfe3_tap1, 6);
	comb_val_21_t6x("dfe_tap2", dfe0_tap2, dfe1_tap2, dfe2_tap2, dfe3_tap2, 5);
	comb_val_21_t6x("dfe_tap3", dfe0_tap3, dfe1_tap3, dfe2_tap3, dfe3_tap3, 4);
	comb_val_21_t6x("dfe_tap4", dfe0_tap4, dfe1_tap4, dfe2_tap4, dfe3_tap4, 4);

	rx_pr("slicer0_ofst: %d, %d, %d, %d", sli0_ofst0, sli0_ofst1, sli0_ofst2, sli0_ofst3);
	rx_pr("slicer1_ofst: %d, %d, %d, %d", sli1_ofst0, sli1_ofst1, sli1_ofst2, sli1_ofst3);
	rx_pr("slicer2_ofst: %d, %d, %d, %d", sli2_ofst0, sli2_ofst1, sli2_ofst2, sli2_ofst3);
	rx_pr("slicer3_ofst: %d, %d, %d, %d", sli3_ofst0, sli3_ofst1, sli3_ofst2, sli3_ofst3);
	rx_pr("slicer4_ofst: %d, %d, %d, %d", sli4_ofst0, sli4_ofst1, sli4_ofst2, sli4_ofst3);
	rx_pr("slicer5_ofst: %d, %d, %d, %d", sli5_ofst0, sli5_ofst1, sli5_ofst2, sli5_ofst3);
	rx_pr("slicer6_ofst: %d, %d, %d, %d", sli6_ofst0, sli6_ofst1, sli6_ofst2, sli6_ofst3);
	rx_pr("slicer7_ofst: %d, %d, %d, %d", sli7_ofst0, sli7_ofst1, sli7_ofst2, sli7_ofst3);
	rx_pr("slicer8_ofst: %d, %d, %d, %d", sli8_ofst0, sli8_ofst1, sli8_ofst2, sli8_ofst3);
	rx_pr("slicer9_ofst: %d, %d, %d, %d", sli9_ofst0, sli9_ofst1, sli9_ofst2, sli9_ofst3);
	rx_pr("afe_ofst: %d, %d, %d, %d", afe_ofst0, afe_ofst1, afe_ofst2, afe_ofst3);

	rx_pr("cdr_code=[%d,%d,%d,%d]\n", cdr0_code, cdr1_code, cdr2_code, cdr3_code);
	rx_pr("cdr_lock=[%d,%d,%d,%d]\n", cdr0_lock, cdr1_lock, cdr2_lock, cdr3_lock);
	comb_val_21_t6x("cdr_int", cdr0_int, cdr1_int, cdr2_int, cdr3_int, 7);
}

void dump_aml_phy_sts_t6x(u8 port)
{
	if (port <= E_PORT1)
		dump_aml_phy_sts_t6x_20(port);
	else
		dump_aml_phy_sts_t6x_21(port);
}

bool aml_get_tmds_valid_t6x_20(u8 port)
{
	u32 tmdsclk_valid;
	u32 sqofclk;
	u32 tmds_align;
	u32 ret;
	/* digital tmds valid depends on PLL lock from analog phy. */
	/* it is not necessary and T7 has not it */
	/* tmds_valid = hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01; */
	sqofclk = hdmirx_rd_top(TOP_MISC_STAT0_T6X, port) & 0x1;
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

bool aml_get_tmds_valid_t6x_21(u8 port)
{
	u32 tmdsclk_valid;
	u32 sqofclk;
	u32 tmds_align;
	u32 ret;

	/* frl_debug todo */
	if (rx[port].var.frl_rate)
		return true;
	/* digital tmds valid depends on PLL lock from analog phy. */
	/* it is not necessary and T7 has not it */
	/* tmds_valid = hdmirx_rd_dwc(DWC_HDMI_PLL_LCK_STS) & 0x01; */
	sqofclk = hdmirx_rd_top(TOP_MISC_STAT0_T6X, port) & 0x1;
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

bool aml_get_tmds_valid_t6x(u8 port)
{
	if (port <= E_PORT1)
		return aml_get_tmds_valid_t6x_20(port);
	else
		return aml_get_tmds_valid_t6x_21(port);
}

int debug_port;
void hdmirx_rd_check_top_t6x(u32 addr, u32 exp_data, u32 mask, u8 port)
{
	u32 rd_data;

	rd_data = hdmirx_rd_top(addr, port);
	rx_pr("check_top=0x%x\n", rd_data);
	if ((rd_data | mask) != (exp_data | mask))
		rx_pr("top reg 0x%x,rd_data=0x%x, exp_data=0x%x mask=0x%x\n",
		addr, rd_data, exp_data, mask);
}

int aml_phy_short_bist_t6x_20(void)
{
	u32 data32;
	int rx_port;
	int ch0_lock = 0;
	int ch1_lock = 0;
	int ch2_lock = 0;
	int lock_sts = 0;
	int ret = 0;
	u8 port = debug_port;

	//power off pll
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL1, 0, port);
	//end

	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, 0x03100100, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0x0500f800, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL1, 0x01481236, port);
	usleep_range(2000, 2100);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0x0500f801, port);
	usleep_range(2000, 2100);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0x0500f803, port);
	usleep_range(2000, 2100);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL1, 0x01401236, port);
	usleep_range(2000, 2100);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0x0500f807, port);
	usleep_range(2000, 2100);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0x4500f807, port);
	usleep_range(2000, 2100);
	if (l_bist_en)
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, 0x30003469, port);
	else
		hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, 0x30000050, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, 0x04007053, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, 0x7ff00459, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, 0x02821666, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, 0x11c73228 |
		(l_bist_en << 2), port);
	usleep_range(2000, 2100);
	for (rx_port = 0; rx_port <= 2; rx_port = rx_port + 1) {
		rx_pr("hdmirx_bist_test -- channel %d\n", rx_port);
		// Program PHY to select which channel's clock to be the source of tmds_clk
		if (rx_port == 0)
			// TODO: add PHY reg here to select channel0 clock source!!!
			hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, 0xfff00100 &
				(~(l_bist_en << 23)), port);
		else if (rx_port == 1)
			// TODO: add PHY reg here to select channel1 clock source!!!
			hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, 0xfff00101 &
				(~(l_bist_en << 23)), port);
		else// port == 2
			// TODO: add PHY reg here to select channel2 clock source!!!
			hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, 0xfff00102 &
				(~(l_bist_en << 23)), port);

		// Reset
		// [8]  ~bist_rst_n = 1
		// [7]  ~chan_rst_n = 1
		hdmirx_wr_top(TOP_SW_RESET, 0xffffffff, port);
		// [8]  ~bist_rst_n = 0
		// [7]  ~chan_rst_n = 0
		hdmirx_wr_top(TOP_SW_RESET, 0xfffffe7f, port);

		// Configure channel switch

		data32 = 0;
		data32 |= (1 << 4); // [    4]  valid_always
		data32 |= (7 << 0); // [ 3: 0]  decoup_thresh
		hdmirx_wr_top(TOP_CHAN_SWITCH_1, data32, port);

		data32 = 0;
		data32 |= (2 << 28); // [29:28]    source_2
		data32 |= (1 << 26); // [27:26]    source_1
		data32 |= (0 << 24); // [25:24]    source_0
		data32 |= (0 << 20); // [22:20]    skew_2
		data32 |= (0 << 16); // [18:16]    skew_1
		data32 |= (0 << 12); // [14:12]    skew_0
		data32 |= (0 << 10); // [10]       bitswap_2
		data32 |= (0 << 9); // [9]        bitswap_1
		data32 |= (0 << 8); // [8]        bitswap_0
		data32 |= (0 << 6); // [6]        polarity_2
		data32 |= (0 << 5); // [5]        polarity_1
		data32 |= (0 << 4); // [4]        polarity_0
		data32 |= (0 << 3); // [3]        charswap_2
		data32 |= (0 << 2); // [2]        charswap_1
		data32 |= (0 << 1); // [1]        charswap_0
		data32 |= (0 << 0); // [0]        enable
		hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);
		data32 |= (1 << 0); // [0]        enable
		hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);

		// Ccnfigure PRBS generator
		data32 = 0;
		data32 |= (0 << 16);//[16]inj_prbs_err
		data32 |= (0 << 10);//[14:10]pttn_max
		data32 |= (0 << 9);//[9]pttn_gen_enable
		data32 |= (0 << 8);//[8]bist_loopback
		data32 |= (3 << 5);//[7:5]wrp_threshold
		data32 |= bist_mode;//[4:2]prbs_mode
		data32 |= (0 << 0);//[0]prbs_gen_enable
		hdmirx_wr_top(TOP_PRBS_GEN, data32, port);

		// Ccnfigure PRBS analyzer: PRBS15
		data32 = 0;
		data32 |= (bist_mode << 28);//[30:28]prbs_mode
		data32 |= (0 << 25);// [25]prbs_ana_resync_ch3
		data32 |= (0 << 24);// [24]prbs_ana_enable_ch3
		data32 |= (bist_mode << 20);//[22:20]prbs_mode
		data32 |= (1 << 17);//[17]prbs_ana_resync_ch2
		data32 |= (0 << 16);//[16]prbs_ana_enable_ch2
		data32 |= (bist_mode << 12);//[14:12]prbs_mode
		data32 |= (1 << 9);//[9]prbs_ana_resync_ch1
		data32 |= (0 << 8);//[8]prbs_ana_enable_ch1
		data32 |= (bist_mode << 4);//[6:4]prbs_mode
		data32 |= (1 << 1);//[1]prbs_ana_resync_ch0
		data32 |= (0 << 0);//[0]prbs_ana_enable_ch0
		hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);
		data32 |= (1 << 24);//[24]prbs_ana_enable_ch3
		data32 |= (1 << 16);//[16]prbs_ana_enable_ch2
		data32 |= (1 << 8);//[8]prbs_ana_enable_ch1
		data32 |= (1 << 0);//[0]prbs_ana_enable_ch0
		hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);

		// Start PRBS
		rx_pr("Start PRBS\n");
		data32 = 0;
		data32 |= (0 << 16);//[16]inj_prbs_err
		data32 |= (0 << 10);//[14:10]pttn_max
		data32 |= (0 << 9);//[9]pttn_gen_enable
		data32 |= (0 << 8);//[8]bist_loopback
		data32 |= (3 << 5);//[7:5]wrp_threshold
		data32 |= (bist_mode << 2);//[4:2]prbs_mode
		data32 |= (1 << 0);//[0]prbs_gen_enable
		hdmirx_wr_top(TOP_PRBS_GEN, data32, port);
		// wait 5ms
		usleep_range(5000, 5100);
		hdmirx_wr_bits_top(TOP_PRBS_ANA_0, _BIT(1), 0,  port);
		hdmirx_wr_bits_top(TOP_PRBS_ANA_0, _BIT(9), 0,  port);
		hdmirx_wr_bits_top(TOP_PRBS_ANA_0, _BIT(17), 0,  port);
		usleep_range(5000, 5100);
		rx_pr("prbs_ana=0x%x\n", hdmirx_rd_top(TOP_PRBS_ANA_0, port));
		/* Check BIST analyzer BER counters */
		if (rx_port == 0)
			rx_pr("BER_CH0 = %x\n",
			      hdmirx_rd_top(TOP_PRBS_ANA_BER_CH0, port));
		else if (rx_port == 1)
			rx_pr("BER_CH1 = %x\n",
			      hdmirx_rd_top(TOP_PRBS_ANA_BER_CH1, port));
		else if (rx_port == 2)
			rx_pr("BER_CH2 = %x\n",
			      hdmirx_rd_top(TOP_PRBS_ANA_BER_CH2, port));

		/* check BIST analyzer result */
		lock_sts = hdmirx_rd_top(TOP_PRBS_ANA_STAT, port) & 0x3f;
		rx_pr("ch%dsts=0x%x\n", rx_port, lock_sts);
		if (rx_port == 0) {
			ch0_lock = lock_sts & 3;
			if (ch0_lock == 1)
				rx_pr("ch0 PASS\n");
			else
				rx_pr("ch0 NG\n");
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH0, 0, 0xff, debug_port);
		}
		if (rx_port == 1) {
			ch1_lock = (lock_sts >> 2) & 3;
			if (ch1_lock == 1)
				rx_pr("ch1 PASS\n");
			else
				rx_pr("ch1 NG\n");
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH1, 0, 0xff, debug_port);
		}
		if (rx_port == 2) {
			ch2_lock = (lock_sts >> 4) & 3;
			if (ch2_lock == 1)
				rx_pr("ch2 PASS\n");
			else
				rx_pr("ch2 NG\n");
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH2, 0, 0xff, debug_port);
		}
		usleep_range(1000, 1100);
		if (bist_lvl == 1) {
			// Stop PRBS analyzer
			hdmirx_wr_top(TOP_PRBS_ANA_0, 0, port);
			// Stop PRBS generator
			hdmirx_wr_top(TOP_PRBS_GEN, 0, port);
			// Disable channel
			hdmirx_wr_top(TOP_CHAN_SWITCH_0, 0, port);
		}
	}
	lock_sts = ch0_lock | (ch1_lock << 2) | (ch2_lock << 4);
	if (lock_sts == 0x15) {/* lock_sts == b'010101' is PASS*/
		ret = 1;
		rx_pr("bist_test PASS\n");
	} else {
		rx_pr("bist_test FAIL\n");
	}
	rx_pr("short bist done\n");
	if (bist_lvl == 1)
		hdmirx_wr_top(TOP_CHAN_SWITCH_0, 0x24000001, port);
	return ret;
}

void short_bist_check_t6x(void)
{
	int ch0_lock = 0;
	int ch1_lock = 0;
	int ch2_lock = 0;
	int lock_sts = 0;
	u8 port = debug_port;

	/* check BIST analyzer result */
	lock_sts = hdmirx_rd_top(TOP_PRBS_ANA_STAT, port) & 0x3f;
	ch0_lock = lock_sts & 3;
	if (ch0_lock == 1)
		rx_pr("ch0 PASS\n");
	else
		rx_pr("ch0 NG\n");
	hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH0, 0, 0xff, port);
	ch1_lock = (lock_sts >> 2) & 3;
	if (ch1_lock == 1)
		rx_pr("ch1 PASS\n");
	else
		rx_pr("ch1 NG\n");
	hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH1, 0, 0xff, port);
	ch2_lock = (lock_sts >> 4) & 3;
	if (ch2_lock == 1)
		rx_pr("ch2 PASS\n");
	else
		rx_pr("ch2 NG\n");
	hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH2, 0, 0xff, port);
	usleep_range(1000, 1100);
	// Stop PRBS analyzer
	hdmirx_wr_top(TOP_PRBS_ANA_0, 0, port);
	// Stop PRBS generator
	hdmirx_wr_top(TOP_PRBS_GEN, 0, port);
	// Disable channel
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, 0, port);
	lock_sts = ch0_lock | (ch1_lock << 2) | (ch2_lock << 4);
	if (lock_sts == 0x15)/* lock_sts == b'010101' is PASS*/
		rx_pr("bist_test PASS\n");
	else
		rx_pr("bist_test FAIL\n");
}

void aml_phy_short_bist_t6x_21(void)
{
	//todo
}

void aml_phy_short_bist_t6x(u8 port)
{
	if (port <= E_PORT1)
		aml_phy_short_bist_t6x_20();
	else
		aml_phy_short_bist_t6x_21();
}

bool rx_check_tap0_t6x_20(u8 port)
{
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0;
	u32 data32;
	bool ret = true;

	if (rx[port].state != FSM_SIG_READY) {
		rx_pr("sig unready\n");
		return false;
	}
	hdmirx_wr_bits_amlphy_t3x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_EHM_DBG_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t3x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_STATUS_MUX_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t3x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_DFE_OFST_DBG_SEL, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t3x(T6X_HDMIRX20PHY_DCHD_STAT, port);
	dfe0_tap0 = data32 & 0x7f;
	dfe1_tap0 = (data32 >> 8) & 0x7f;
	dfe2_tap0 = (data32 >> 16) & 0x7f;
	dfe3_tap0 = (data32 >> 24) & 0x7f;

	if (dfe0_tap0 < 25 || dfe1_tap0 < 25 || dfe2_tap0 < 25) {
		ret = false;
		rx_pr("tap0 error\n");
	}
	return ret;
}

bool rx_check_tap0_t6x_21(u8 port)
{
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0;
	u32 data32;
	bool ret = true;

	if (rx[port].state != FSM_SIG_READY) {
		rx_pr("sig unready\n");
		return false;
	}
	hdmirx_wr_bits_amlphy_t3x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t3x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t3x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t3x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap0 = data32 & 0x7f;
	dfe1_tap0 = (data32 >> 8) & 0x7f;
	dfe2_tap0 = (data32 >> 16) & 0x7f;
	dfe3_tap0 = (data32 >> 24) & 0x7f;

	if (dfe0_tap0 < 15 || dfe1_tap0 < 15 || dfe2_tap0 < 15) {
		ret = false;
		rx_pr("port%d tap0 error\n", port);
	}
	return ret;
}

bool rx_check_tap0_t6x(void)
{
	if (rx_info.main_port <= E_PORT1)
		return rx_check_tap0_t6x_20(rx_info.main_port);
	else
		return rx_check_tap0_t6x_21(rx_info.main_port);
}

void aml_phy_exbist_t6x_20(u8 port, u8 ch)
{
	u32 data32, tmp;
	int rst_pll_cnt = 0;

	sm_pause = 1;
	//initialize pll/phy registers
	//initialize pll/phy registers
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL1, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, 0x0, port);
	//set registers to default
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, 0x30e00469, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, 0x04080093, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, 0x11c73000, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, 0xffe00000, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, 0x7ff00459, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, 0x02821666, port);
	usleep_range(10, 20);

	do {
		hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x05302800), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01481236, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x05302801), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x45302803), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_1, 0x01401236, port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x05302807), port);
		usleep_range(10, 20);
		hdmirx_wr_amlphy_t6x(T6X_RG_RX20PLL_0, (0x45302807), port);
		if (rst_pll_cnt++ > pll_rst_max) {
			if (log_level & VIDEO_LOG)
				rx_pr("pll rst error\n");
			break;
		}
		if (log_level & VIDEO_LOG) {
			rx_pr("sq=%d,pll_lock=%d",
			      hdmirx_rd_top(TOP_MISC_STAT0, port) & 0x1,
			      is_pll_lock_t6x(port));
		}
	} while (!is_tmds_clk_stable(port) && is_clk_stable(port) && !is_pll_lock_t6x(port));

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_CDR_RSTB, 1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_CDR_EN, 1, port);
	usleep_range(200, 210);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EN_BYP_EQ, 0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_EN, 1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_EQ_RSTB, 1, port);
	usleep_range(200, 210);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_EN, 1, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, T6X_20_DFE_RSTB, 1, port);
	usleep_range(1000, 1010);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_CDR_FR_EN, 1, port);
	usleep_range(1000, 1010);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, T6X_20_CDR_LKDET_EN, 1, port);

	//dump phy status
	dump_aml_phy_sts_t6x(port);

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
	//hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_STAT, data32, 0);
	tmp = hdmirx_rd_top(TOP_PRBS_ANA_STAT, port);
	if (ch == E_CH0) {
		if ((tmp & 0x1) == 0x1) {
			rx_pr("ch0 pass\n");
		} else {
			rx_pr("ch0 exbist fail\n");
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH0, 0, 0, port);
		}
	}
	if (ch == E_CH1) {
		if ((tmp & 0x4) == 0x4) {
			rx_pr("ch1 pass\n");
		} else {
			rx_pr("ch1 exbist fail\n");
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH1, 0, 0, port);
		}
	}
	if (ch == E_CH2) {
		if ((tmp & 0x10) == 0x10) {
			rx_pr("ch2 pass\n");
		} else {
			rx_pr("ch2 exbist fail\n");
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH2, 0, 0, port);
		}
	}
	if (ch == E_ALL_CH) {
		if ((tmp & 0x15) == 0x15) {
			rx_pr("exbist pass\n");
		} else {
			rx_pr("exbist fail\n");
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH0, 0, 0, port);
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH1, 0, 0, port);
			hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_BER_CH2, 0, 0, port);		}
	}
}

void aml_phy_exbist_t6x_21(u8 port, u8 ch)
{
	u32 data32, tmp;

	rx_pr("port21 frl exbist:frlrate=%d,ch%d\n", frl_rate_id, ch);
	//phy cfg
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01fcf0f0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, 0x30f7000f, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, 0x00005a00, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, 0x2320ffff, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, 0x041f0205, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, 0x01102008, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, 0x03f06555, port);

	//pll cfg
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa00, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x014451a4, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa01, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa03, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa07, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4520fa07, port);
	usleep_range(10, 20);
	if ((hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, port) >> 31) & 1)
		rx_pr("pll lock\n");
	else
		rx_pr("pll unlock\n");

	//phy cdr&eq&dfe adp
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01fcf0ff, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, 0x00001082, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, 0x3001104f, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, 0x00007082, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, 0x3003304f, port);
	usleep_range(1000, 1100);
	//sw reset
	hdmirx_wr_top(TOP_SW_RESET, 0xfffffe7f, port);
	usleep_range(5, 10);
	hdmirx_wr_top(TOP_SW_RESET, 0, port);
	usleep_range(1000, 1100);
	data32 = 0;
	data32 |= (1 << 14);//tmds_clk_sel:1-tmds21 0-tmds20
	data32 |= ((ch * 4) << 10);
	hdmirx_wr_top(TOP_CLK_CNTL, data32, port);
	//digital prbs 15 check
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

	hdmirx_wr_top(TOP_CHAN_SWITCH_1, 0x3000007, port);
	data32 |= (1 << 0);//[0]enable
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, 0x24000001, port);
	hdmirx_wr_top(TOP_PRBS_GEN, 0x6d, port);////5:prbs7, d:prbs15
	hdmirx_wr_top(TOP_PRBS_ANA_0, 0x30303030, port);
	hdmirx_wr_top(TOP_PRBS_ANA_0, 0x31313131, port);
	usleep_range(1000, 1100);
	data32 = 0;
	data32 |= (1 << 7);
	data32 |= (0 << 6);
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	//hdmirx_rd_check_top_t6x(TOP_PRBS_ANA_STAT, data32, 0);
	tmp = hdmirx_rd_top(TOP_PRBS_ANA_STAT, port);
	if (ch == E_CH0) {
		if ((tmp & 0x1) == 0x1) {
			rx_pr("ch0 pass\n");
		} else {
			rx_pr("ch0 exbist fail\n");
			dump_aml_phy_sts_t6x_21(port);
			dump_reg_phy_t6x_21(port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH0, 0, 0, port);
		}
	}
	if (ch == E_CH1) {
		if ((tmp & 0x4) == 0x4) {
			rx_pr("ch1 pass\n");
		} else {
			rx_pr("ch1 exbist fail\n");
			dump_aml_phy_sts_t6x_21(port);
			dump_reg_phy_t6x_21(port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH1, 0, 0, port);
		}
	}
	if (ch == E_CH2) {
		if ((tmp & 0x10) == 0x10) {
			rx_pr("ch2 pass\n");
		} else {
			rx_pr("ch2 exbist fail\n");
			dump_aml_phy_sts_t6x_21(port);
			dump_reg_phy_t6x_21(port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH2, 0, 0, port);
		}
	}
	if (ch == E_CH3) {
		if ((tmp & 0x40) == 0x40) {
			rx_pr("ch3 pass\n");
		} else {
			rx_pr("ch3 exbist fail\n");
			dump_aml_phy_sts_t6x_21(port);
			dump_reg_phy_t6x_21(port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH3, 0, 0, port);
		}
	}
	if (ch == E_ALL_CH) {
		if ((tmp & 0x55) == 0x55) {
			rx_pr("exbist pass\n");
		} else {
			rx_pr("exbist fail\n");
			dump_aml_phy_sts_t6x_21(port);
			dump_reg_phy_t6x_21(port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH0, 0, 0, port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH1, 0, 0, port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH2, 0, 0, port);
			hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH3, 0, 0, port);
		}
	}
}

/* check ber error */
void aml_phy_exbist_ber_check_t6x_21(u8 port)
{
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH0, 0, 0, port);
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH1, 0, 0, port);
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH2, 0, 0, port);
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH3, 0, 0, port);
}

/* sw_reset_bist */
void aml_phy_exbist_ber_rst_t6x_21(u8 port)
{
	hdmirx_wr_top(TOP_SW_RESET, 0x100, port);
	hdmirx_wr_top(TOP_SW_RESET, 0x0, port);
}

int aml_phy_get_iq_skew_val_t6x(u32 val_0, u32 val_1)
{
	int val = val_0 - val_1;

	rx_pr("val=%d\n", val);
	if (val)
		return (val - 32);
	else
		return (val + 128 - 32);
}

/* IQ skew monitor */
void aml_phy_iq_skew_monitor_t6x(void)
{
}

void rx_set_term_value_t6x_20(unsigned char port, bool value)
{
	u32 data32;

	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, port);
	if (value) {
		data32 |= (1 << 23);
	} else {
		/* rst cdr to clr tmds_valid */
		//data32 &= ~(MSK(3, 7));
		data32 &= ~(1 << 23);
	}
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, data32, port);
}

void rx_set_term_value_t6x_21(unsigned char port, bool value)
{
	u32 data32;

	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, port);
	if (value) {
		data32 |= (1 << 22);
	} else {
		/* rst cdr to clr tmds_valid */
		//data32 &= ~(MSK(3, 7));
		data32 &= ~(1 << 22);
	}
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, data32, port);
}

void rx_set_term_value_t6x(unsigned char port, bool value)
{
	if (port <= E_PORT1) {
		rx_set_term_value_t6x_20(port, value);
	} else if (port <= E_PORT3) {
		rx_set_term_value_t6x_21(port, value);
	} else {
		rx_set_term_value_t6x_20(E_PORT0, value);
		rx_set_term_value_t6x_20(E_PORT1, value);
		rx_set_term_value_t6x_21(E_PORT2, value);
		rx_set_term_value_t6x_21(E_PORT3, value);
	}
}

void aml_phy_power_off_t6x_20(u8 port)
{
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL0, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PLL_CTRL1, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_AFE, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_DFE, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_CDR, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_EQ, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHD_STAT, 0x0, port);
	//hdmirx_wr_amlphy_t6x(T6X_HDMIRX_EARCTX_CNTL0, 0x0, E_PORT0);
	//hdmirx_wr_amlphy_t6x(T6X_HDMIRX_EARCTX_CNTL1, 0x0, E_PORT0);
	//hdmirx_wr_amlphy_t6x(T6X_HDMIRX_ARC_CNTL, 0x0, E_PORT0);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX_PHY_PROD_TEST0, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX_PHY_PROD_TEST1, 0x0, port);
}

void aml_phy_power_off_t6x_21(u8 port)
{
	/* power off phy and pll */
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL3, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL4, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, 0x0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, 0x0, port);
	/* poweroff port C FPLL */
	if (port == E_PORT2) {
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL0, 0x0);
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL1, 0x0);
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL2, 0x0);
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL0_CTRL3, 0x0);
	}

	if (port == E_PORT3) {
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL0, 0x0);
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL1, 0x0);
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL2, 0x0);
		wr_reg_clk_ctl(T6X_ANACTRL_HDMI_PLL1_CTRL3, 0x0);
	}
}

void aml_phy_power_off_t6x(u8 port)
{
	if (port <= E_PORT1) {
		aml_phy_power_off_t6x_20(port);
	} else if (port <= E_PORT3) {
		aml_phy_power_off_t6x_21(port);
	} else {
		aml_phy_power_off_t6x_20(port);
		aml_phy_power_off_t6x_21(port);
	}
}

void aml_phy_offset_cal_t6x(void)
{
	aml_phy_offset_cal_t6x_20(E_PORT0);
	usleep_range(10, 20);
	aml_phy_offset_cal_t6x_20(E_PORT1);
	usleep_range(10, 20);
	aml_phy_offset_cal_t6x_21(E_PORT2);
	usleep_range(10, 20);
	aml_phy_offset_cal_t6x_21(E_PORT3);
}

void aml_phy_switch_port_t6x(u8 port)
{
	u32 data32;

	/* reset and select data port */
	//data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, port);
	//data32 &= (~(0xf << 24));
	//data32 |= ((1 << port) << 24);
	//hdmirx_wr_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC2, data32, port);
	//hdmirx_wr_bits_top_common(TOP_PORT_SEL, MSK(4, 0), (1 << port));
	data32 = 0;
	data32 |= (1 << (rx_info.main_port + 8));//[11:8]
	data32 |= (1 << rx_info.main_port);//[3:0]
	hdmirx_wr_top_common(HDMIRX_TOP_FSW_CNTL, data32);

	hdmirx_wr_top_common(HDMIRX_TOP_FSW_CLK_CNTL, 1);

	hdmirx_wr_top_common(HDMIRX_TOP_SW_RESET_COMMON, 0x19a);//todo
	hdmirx_wr_top_common(HDMIRX_TOP_SW_RESET_COMMON, 0);
}

unsigned int rx_sec_hdcp_cfg_t6x(void)
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
void rx_set_aud_output_t6x(u32 param, u8 port)
{
	if (param == 2) {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x0f, port);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0xff, port);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x90, port); //TDM
	} else if (param == 1) {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x00, port);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0x10, port);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80, port); //I2S
		/* bit'15 1-spdif, 0- hbr */
		hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(15), 0, port);
		hdmirx_wr_bits_top_common_1(TOP_ACR_CNTL2_T6X, _BIT(5), port);
	} else {
		hdmirx_wr_cor(RX_TDM_CTRL1_AUD_IVCRX, 0x00, port);
		hdmirx_wr_cor(RX_TDM_CTRL2_AUD_IVCRX, 0x10, port);
		hdmirx_wr_cor(AAC_MCLK_SEL_AUD_IVCRX, 0x80, port); //SPDIF
		//hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(15), 1);
		hdmirx_wr_bits_top(TOP_CLK_CNTL, _BIT(4), 0, port);
	}
}

void rx_sw_reset_t6x(int level, u8 port)
{
	/* deep color fifo */
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 1, port);
	udelay(1);
	hdmirx_wr_bits_cor(RX_PWD_SRST_PWD_IVCRX, _BIT(4), 0, port);
	//TODO..
}

void hdcp_init_t6x(u8 port)
{
	u8 data8;
	//key config and crc check
	//rx_sec_hdcp_cfg_t6x();
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
	data8 |= (hdmirx_repeat_support() &&
		rx[port].hdcp.repeat) << 1;
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

//void reset_pcs(void)
//{
	//hdmirx_wr_top(TOP_SW_RESET, 0x80);
	//hdmirx_wr_top(TOP_SW_RESET, 0);
//}

void rx_t6x_prbs(void)
{
	u32 data32;
	u8 port = E_PORT2;

	rx_pr("prbs start\n");
	//Prbs/Pattern config
	//Release Bist Reset
	// [8] ~bist rst n = 0// [7]~chan rst n = 0
	hdmirx_wr_top(TOP_SW_RESET, 0xfffffe7f, port);
	//wait for PHY initial hdmirx delay us(50);
	rx_pr("prbs-1\n");
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL3, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL4, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, 0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, 0, port);
	rx_pr("prbs-2\n");
	//phy reg set default value
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, 0x0370ffff, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, 0x07f06555, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, 0x15ff1a05, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, 0x51001010, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, 0x040074b3, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, 0x3013304f, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01fcffff, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, 0x20f70f0f, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, 0x00005500, port);
	rx_pr("prbs-3\n");
	//program pll reg
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x45001800, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x01401202, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2, 0x000c7d01, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL3, 0xf0002dd3, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL4, 0x55813041, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2, 0x000c7d07, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2, 0x000c7d01, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x45001801, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x45001803, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2, 0x000c7d07, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x45001807, port);
	//toggle phy dch_rstn and tmds clk_en
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01fcfff0, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, 0x07f06555, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01fcffff, port);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, 0x07f06555, port);
	rx_pr("prbs-4\n");
	//select tmds or frl mode
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01fcffff, port);
	mdelay(1000);
	rx_pr("MISC_STAT0 = 0x%x\n", hdmirx_rd_top(TOP_MISC_STAT0_T6X, port));
	dump_state(RX_DUMP_PHY, port);
	mdelay(1000);
	dump_state(RX_DUMP_PHY, port);
	mdelay(1000);
	dump_state(RX_DUMP_PHY, port);
	mdelay(1000);
	dump_state(RX_DUMP_PHY, port);

	hdmirx_poll_top(TOP_MISC_STAT0_T6X, 0X5 << 16, ~(0X5 << 16), port);
	rx_pr("prbs phy end\n");

	//config channel switch
	data32  = 0;
	data32 |= (3 << 24); /* source_2 */
	data32 |= (0 << 4); /* [  4]  valid_always*/
	data32 |= (7 << 0); /* [3:0]  decoup_thresh*/
	hdmirx_wr_top(TOP_CHAN_SWITCH_1_T6X, data32, port);

	data32  = 0;
	data32 |= (2 << 28); /* [29:28]      source_2 */
	data32 |= (1 << 26); /* [27:26]      source_1 */
	data32 |= (0); /* [25:24]      source_0 */
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);
	data32 |= (1 << 0);// [0] enable
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);

	if (1) {
		rx_pr("prbs-5\n");
		//config prbs_gen
		data32  = 0;
		data32 |= (0 << 16);// in_bist_err
		data32 |= (3 << 5);// decouple_thresh_prbs
		data32 |= (1 << 2);//prbs_mode. 1=p7;2=p11,3=p15,5=p20,6=p23,7=p31
		data32 |= (1 << 0);//prbs_en
		hdmirx_wr_top(TOP_PRBS_GEN, data32, port);

		//config prbs_ana
		data32  = 0;
		data32 |= (1 << 28);
		data32 |= (0 << 25);
		data32 |= (0 << 24);
		data32 |= (1 << 20);
		data32 |= (0 << 17);
		data32 |= (0 << 16);
		data32 |= (1 << 12);
		data32 |= (0 << 9);
		data32 |= (0 << 8);
		data32 |= (1 << 4);
		data32 |= (0 << 1);
		data32 |= (0 << 0);
		hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);
		data32 |= (1 << 24);
		data32 |= (1 << 16);
		data32 |= (1 << 8);
		data32 |= (1 << 0);
		hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);
		rx_pr("prbs-6\n");
		mdelay(1);

		//check prbs
		data32 = 0;
		data32 |= (0 << 7);
		data32 |= (1 << 6);
		data32 |= (0 << 5);
		data32 |= (1 << 4);
		data32 |= (0 << 3);
		data32 |= (1 << 2);
		data32 |= (0 << 1);
		data32 |= (1 << 0);
		hdmirx_rd_check_top(TOP_PRBS_ANA_STAT, data32, 0, port);

		//check err cnt
		hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH0, 0, 0, port);
		hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH1, 0, 0, port);
		hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH2, 0, 0, port);
		hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH3, 0, 0, port);
		rx_pr("prbs end\n");
	} else {
		data32 = 0;
		data32 |= (0x133 << 20);
		data32 |= (0x122 << 10);
		data32 |= (0x111 << 0);
		hdmirx_wr_top_common(TOP_SHFT_PTTN_0, data32);

		data32 = 0;
		data32 |= (0x266 << 20);
		data32 |= (0x255 << 10);
		data32 |= (0x244 << 0);
		hdmirx_wr_top_common(TOP_SHFT_PTTN_1, data32);

		data32 = 0;
		data32 |= (0x399 << 20);
		data32 |= (0x388 << 10);
		data32 |= (0x377 << 0);
		hdmirx_wr_top_common(TOP_SHFT_PTTN_2, data32);

		data32 = 0;
		data32 |= (0x0cc << 20);
		data32 |= (0x0bb << 10);
		data32 |= (0x0aa << 0);
		hdmirx_wr_top_common(TOP_SHFT_PTTN_3, data32);

		//config prbs_gen
		data32  = 0;
		data32 |= (4 << 10);// in_bist_err
		data32 |= (3 << 5);// decouple_thresh_prbs
		data32 |= (1 << 9);//prbs_mode. 1=p7;2=p11,3=p15,5=p20,6=p23,7=p31
		hdmirx_wr_top(TOP_PRBS_GEN, data32, port);

		//config pattern ana
		data32  = 0;
		data32 |= (0xffff << 4);
		data32 |= (0 << 3);
		data32 |= (0 << 1);
		data32 |= (1 << 0);
		hdmirx_wr_top(TOP_SHFT_ANA_CNTL, data32, port);

		//config pattern ana
		data32  = 0;
		data32 |= (0 << 4);
		data32 |= (0 << 1);
		data32 |= (1 << 0);
		hdmirx_rd_check_top(TOP_SHFT_ANA_STAT, data32, 0, port);
	}
}

void rx_long_bist_t6x(void)
{
	u8 port = E_PORT2;
	u32 data32 = 0;

	//Release Bist Reset
	// [8] ~bist rst n = 0// [7]~chan rst n = 0
	hdmirx_wr_top(TOP_SW_RESET, 0xfffffe7f, port);

	//1. phy ldo en
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01bcfff0, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC1, 0x30f700ff, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC2, 0x00005a00, port);
	usleep_range(20, 30);
	//2. pll frl 12g                            ,
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0500fa00, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL1, 0x01445384, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa01, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa03, port);
	usleep_range(20, 30);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x0520fa07, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, 0x4500fa07, port);
	usleep_range(20, 30);
	rx_pr("ctrl0=0x%x\n", hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL0, port));

	//3. phy cdr reset
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, 0x2300ffff, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_DFE, 0x205, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, 0x01102038, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_CTRL, 0x03f06555, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, 0x04001c82, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, 0x30011066, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, 0x01bcffff, port);
	usleep_range(20, 30);
	hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, 0x04007c82, port);

	hdmirx_poll_top(TOP_MISC_STAT0_T6X, 0X5 << 16, ~(0X5 << 16), port);
	rx_pr("long bist phy end\n");

	//config channel switch
	data32  = 0;
	data32 |= (3 << 24); /* source_2 */
	data32 |= (0 << 4); /* [  4]  valid_always*/
	data32 |= (7 << 0); /* [3:0]  decoup_thresh*/
	hdmirx_wr_top(TOP_CHAN_SWITCH_1_T6X, data32, port);

	data32  = 0;
	data32 |= (2 << 28); /* [29:28]      source_2 */
	data32 |= (1 << 26); /* [27:26]      source_1 */
	data32 |= 0; /* [25:24]      source_0 */
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);
	data32 |= (1 << 0);// [0] enable
	hdmirx_wr_top(TOP_CHAN_SWITCH_0, data32, port);

	//config prbs_gen
	data32  = 0;
	data32 |= (0 << 16);// in_bist_err
	data32 |= (3 << 5);// decouple_thresh_prbs
	data32 |= (1 << 2);//prbs_mode. 1=p7;2=p11,3=p15,5=p20,6=p23,7=p31
	data32 |= (1 << 0);//prbs_en
	hdmirx_wr_top(TOP_PRBS_GEN, data32, port);

	//config prbs_ana
	data32  = 0;
	data32 |= (1 << 28);
	data32 |= (0 << 25);
	data32 |= (0 << 24);
	data32 |= (1 << 20);
	data32 |= (0 << 17);
	data32 |= (0 << 16);
	data32 |= (1 << 12);
	data32 |= (0 << 9);
	data32 |= (0 << 8);
	data32 |= (1 << 4);
	data32 |= (0 << 1);
	data32 |= (0 << 0);
	hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);
	data32 |= (1 << 24);
	data32 |= (1 << 16);
	data32 |= (1 << 8);
	data32 |= (1 << 0);
	hdmirx_wr_top(TOP_PRBS_ANA_0, data32, port);
	mdelay(1);

	//check prbs
	data32 = 0;
	data32 |= (0 << 7);
	data32 |= (1 << 6);
	data32 |= (0 << 5);
	data32 |= (1 << 4);
	data32 |= (0 << 3);
	data32 |= (1 << 2);
	data32 |= (0 << 1);
	data32 |= (1 << 0);
	hdmirx_rd_check_top(TOP_PRBS_ANA_STAT, data32, 0, port);

	//check err cnt
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH0, 0, 0, port);
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH1, 0, 0, port);
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH2, 0, 0, port);
	hdmirx_rd_check_top(TOP_PRBS_ANA_BER_CH3, 0, 0, port);
	rx_pr("long bist end\n");
}

void audio_setting_for_aud21_t6x(int frl_rate, u8 port)
{
	if (rx[port].var.frl_rate == FRL_RATE_3G_3LANES) {
		wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1, 0x8);
		//aud div
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,
		MSK(2, 12), 0x1, port);
		//Na
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
		_BIT(13), 0);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2,
		_BIT(19), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1,
		_BIT(9), 1);
	} else if (rx[port].var.frl_rate == FRL_RATE_6G_3LANES) {
		wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1, 0x8);
		//aud div
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,
		MSK(2, 12), 0x2, port);
		//Na
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
		_BIT(13), 0);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2,
		_BIT(19), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1,
		_BIT(9), 0);
	} else if (rx[port].var.frl_rate == FRL_RATE_6G_4LANES) {
		wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1, 0x8);
		//aud div
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,
		MSK(2, 12), 0x2, port);
		//Na
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
		_BIT(13), 0);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2,
		_BIT(19), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1,
		_BIT(9), 0);
	} else if (rx[port].var.frl_rate == FRL_RATE_8G_4LANES) {
		wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1, 0x8);
		//aud div
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,
		MSK(2, 12), 0x2, port);
		//Na
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
		_BIT(13), 0);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2,
		_BIT(19), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1,
		_BIT(9), 0);
	} else if (rx[port].var.frl_rate == FRL_RATE_10G_4LANES) {
		wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1, 0x8);
		//aud div
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,
		MSK(2, 12), 0x3, port);
		//Na
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
		_BIT(13), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2,
		_BIT(19), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1,
		_BIT(9), 0);
	} else {
		wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1, 0x8);
		//aud div
		hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,
		MSK(2, 12), 0x3, port);
		//Na
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
		_BIT(13), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2,
		_BIT(19), 1);
		//ctsa
		hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1,
		_BIT(9), 0);
	}
}

void dump_aud21_param_t6x(u8 port)
{
	u32 data0, data1, data2, data3, data32;
	u32 ctsa2 = 0;
	int n;
	u32 clk_test_after_mux, cts;

	//update todo
	hdmirx_wr_bits_top_common_1(TOP_ACR_CNTL_STAT, _BIT(11), 1);

	data0 = rd_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0);
	data1 = rd_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1);
	data2 = rd_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2);
	data3 = rd_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL3);
	n = hdmirx_rd_top_common_1(TOP_ACR_N_STST);
	cts = hdmirx_rd_top_common_1(TOP_ACR_CTS_STST);
	clk_test_after_mux = meson_clk_measure_with_precision(143, 32);
	rx_pr("N=%d, CTS=%d\n", n, cts);
	rx_pr("clk1618=%d\n", meson_clk_measure_with_precision(9, 32));
	rx_pr("pll_clk_test=%d\n", meson_clk_measure_with_precision(14, 32));
	rx_pr("clk_audpll=%d\n", meson_clk_measure_with_precision(148, 32));
	rx_pr("clk_test_after_mux=%d\n", clk_test_after_mux);
	rx_pr("aud21_pll_clk1=%d\n", meson_clk_measure_with_precision(147, 32));
	rx_pr("aud_pll(N/CTS)=%d\n", clk_test_after_mux * n / cts);
	rx_pr("Na=0x%x\n", (((data0 >> 13) & 0x1) == 1) ? 2 : 1);
	if ((data2 & 0x7) == 0)
		rx_pr("CTSa = 1\n");
	else if ((data2 & 0x7) == 1)
		rx_pr("CTS_a = 2\n");
	else if ((data2 & 0x7) == 2)
		rx_pr("CTS_a = 4\n");
	else if ((data2 & 0x7) == 3)
		rx_pr("CTS_a = 8\n");
	ctsa2 = ((data2 >> 19) << 3) | ((data1 >> 9) & 0x7);
	switch (ctsa2) {
	case 0:
		rx_pr("ctsa_2=1\n");
		break;
	case 1:
		rx_pr("ctsa_2=40\n");
		break;
	case 2:
		rx_pr("ctsa_2=4\n");
		break;
	case 3:
		rx_pr("ctsa_2=8\n");
		break;
	case 4:
		rx_pr("ctsa_2=5\n");
		break;
	case 5:
		rx_pr("ctsa_2=10\n");
		break;
	case 6:
		rx_pr("ctsa_2=16\n");
		break;
	case 7:
		rx_pr("ctsa_2=20\n");
		break;
	case 8:
		rx_pr("ctsa_2=2.25\n");
		break;
	case 9:
		rx_pr("ctsa_2=4.5\n");
		break;
	case 10:
		rx_pr("ctsa_2=9\n");
		break;
	default:
		break;
	}

	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI, port);
	switch ((data32 >> 12) & 0x3) {
	case 0:
		rx_pr("clk_audpll_div=2\n");
		break;
	case 1:
		rx_pr("clk_audpll_div=4\n");
		break;
	case 2:
		rx_pr("clk_audpll_div=8\n");
		break;
	case 3:
		rx_pr("clk_audpll_div=16\n");
		break;
	}
	data32 = (hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2, port) >> 30) & 0x1;
	if (data32 == 0)
		rx_pr("select clk_test\n");
	else
		rx_pr("select clk_audpll21\n");
	rx_pr("ana 4x = 0x%x\n", hdmirx_rd_top_common_1(TOP_ACR_CNTL2_T6X));
}

void clk_init_cor_t6x(void)
{
	u32 data32;
	u8 port = rx_info.main_port;

	rx_pr("\n clk_init\n");
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
	data32 |= (0 << 25);// [26:25] clk_sel for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 16);// [22:16] clk_div for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 9);// [10: 9] clk_sel for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
	data32 |= (0 << 0);// [ 6: 0] clk_div for cts_hdmirx_aud_pll_clk
	wr_reg_clk_ctl(RX_CLK_CTRL2, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_aud_pll_clk_1
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
	wr_reg_clk_ctl(RX_CLK_CTRL2, data32);

	data32  = 0;
	data32 |= (0 << 25);// [26:25] clk_sel for cts_hdmirx_meter_clk
	data32 |= (0 << 24);// [   24] clk_en for cts_hdmirx_meter_clk
	data32 |= (0 << 16);// [22:16] clk_div for cts_hdmirx_meter_clk
	data32 |= (1 << 9);// [10: 9] clk_sel for cts_hdmirx_acr_ref_clk: 1=fclk_div4
	data32 |= (0 << 8);// [    8] clk_en for cts_hdmirx_acr_ref_clk
	data32 |= (0 << 0);// [ 6: 0] clk_div for cts_hdmirx_acr_ref_clk: 24M
	wr_reg_clk_ctl(RX_CLK_CTRL3, data32);
	data32 |= (1 << 24);// [   24] clk_en for cts_hdmirx_meter_clk
	data32 |= (1 << 8);// [    8] clk_en for cts_hdmirx_acr_ref_clk
	wr_reg_clk_ctl(RX_CLK_CTRL3, data32);

	/* new added for t6x emp */
	data32  = 0;
	data32 |= (2 << 9);     // [10: 9] clk_sel for cts_hdmirx_axi_clk: 2=fclk_div3
	data32 |= (0 << 8);     // [    8] clk_en for cts_hdmirx_axi_clk
	data32 |= (0 << 0);     // [ 6: 0] clk_div for cts_hdmirx_axi_clk: fclk_div3/1=666/1=666M
	wr_reg_clk_ctl(RX_CLK_CTRL4, data32);
	data32 |= (1 << 8);     // [    8] clk_en for cts_hdmirx_axi_clk
	wr_reg_clk_ctl(RX_CLK_CTRL4, data32);

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

void rx_dig_clk_en_t6x(bool en)
{
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL, CLK_2M_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL, CLK_5M_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL1, HDCP2X_ECLK_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL1, CFG_CLK_EN, en);
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL3, METER_CLK_EN, en);
	/* added for t6x emp */
	hdmirx_wr_bits_clk_ctl(RX_CLK_CTRL4, AXI_CLK_EN, en);
}

void hdmirx_vga_gain_tuning_t6x(u8 port)
{
	//T6X_HDMIRX21PHY_DCHA_AFE
	u32 data32;
	u32 dfe0_tap0, dfe1_tap0, dfe2_tap0, dfe3_tap0;
	u32 tap0_0_def, tap0_1_def, tap0_2_def, tap0_3_def;
	u32 tap0_0_dec, tap0_1_dec, tap0_2_dec, tap0_3_dec;
	bool tap0_0_done, tap0_1_done, tap0_2_done, tap0_3_done;
	static int tap0_0_cnt, tap0_1_cnt, tap0_2_cnt, tap0_3_cnt;
	//int i;
	unsigned long timeout = jiffies + HZ / 100;
	//bool b_time_out;

	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_EYE_EN, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ, MUX_BLOCK_SEL, 0x0, port);
	hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR, MUX_DFE_OFST_EYE, 0x0, port);
	usleep_range(100, 110);
	data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
	dfe0_tap0 = data32 & 0x7f;
	dfe1_tap0 = (data32 >> 8) & 0x7f;
	dfe2_tap0 = (data32 >> 16) & 0x7f;
	dfe3_tap0 = (data32 >> 24) & 0x7f;
	tap0_0_done = 0;
	tap0_1_done = 0;
	tap0_2_done = 0;
	tap0_3_done = 0;
	while (time_before(jiffies, timeout)) {
		if (!tap0_0_done || !tap0_1_done || !tap0_2_done || !tap0_3_done) {
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR,
				MUX_EYE_EN, 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_EQ,
				MUX_BLOCK_SEL, 0x0, port);
			hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHD_CDR,
				MUX_DFE_OFST_EYE, 0x0, port);
			usleep_range(100, 110);
			data32 = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_DCH_STAT, port);
			dfe0_tap0 = data32 & 0x7f;
			dfe1_tap0 = (data32 >> 8) & 0x7f;
			dfe2_tap0 = (data32 >> 16) & 0x7f;
			dfe3_tap0 = (data32 >> 24) & 0x7f;

			tap0_0_def = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
				MSK(4, 0), port);
			if (dfe0_tap0 < vga_tuning_min && !tap0_0_done) {
				tap0_0_dec = graytodecimal_t6x(tap0_0_def);
				tap0_0_dec += 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
					MSK(4, 0), decimaltogray_t6x(tap0_0_dec), port);
				if (tap0_0_dec == 15)
					tap0_0_done = 1;
			} else if (dfe0_tap0 > vga_tuning_max && !tap0_0_done) {
				tap0_0_dec = graytodecimal_t6x(tap0_0_def);
				tap0_0_dec -= 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
					MSK(4, 0), decimaltogray_t6x(tap0_0_dec), port);
				dfe0_tap0 = data32 & 0x7f;
				if (tap0_0_dec == 0)
					tap0_0_done = 1;
			} else {
				tap0_0_cnt++;
				if (tap0_0_cnt == 3) {
					tap0_0_done = 1;
					tap0_0_cnt = 0;
				}
			}
			tap0_1_def = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
			MSK(4, 4), port);
			if (dfe1_tap0 < vga_tuning_min && !tap0_1_done) {
				tap0_1_dec = graytodecimal_t6x(tap0_1_def);
				tap0_1_dec += 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, MSK(4, 4),
				    decimaltogray_t6x(tap0_1_dec), port);
				if (tap0_1_dec == 15)
					tap0_1_done = 1;
			} else if (dfe1_tap0 > vga_tuning_max && !tap0_1_done) {
				tap0_1_dec = graytodecimal_t6x(tap0_1_def);
				tap0_1_dec -= 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, MSK(4, 4),
				    decimaltogray_t6x(tap0_1_dec), port);
				if (tap0_1_dec == 0)
					tap0_1_done = 1;
			} else {
				tap0_1_cnt++;
				if (tap0_1_cnt == 3) {
					tap0_1_done = 1;
					tap0_1_cnt = 0;
				}
			}
			tap0_2_def = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
			MSK(4, 8), port);
			if (dfe2_tap0 < vga_tuning_min && !tap0_2_done) {
				tap0_2_dec = graytodecimal_t6x(tap0_2_def);
				tap0_2_dec += 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, MSK(4, 8),
				    decimaltogray_t6x(tap0_2_dec), port);
				if (tap0_2_dec == 15)
					tap0_2_done = 1;
			} else if ((dfe2_tap0 > vga_tuning_max) && !tap0_2_done) {
				tap0_2_dec = graytodecimal_t6x(tap0_2_def);
				tap0_2_dec -= 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
					MSK(4, 8), decimaltogray_t6x(tap0_2_dec), port);
				if (tap0_2_dec == 0)
					tap0_2_done = 1;
			} else {
				tap0_2_cnt++;
				if (tap0_2_cnt == 3) {
					tap0_2_done = 1;
					tap0_2_cnt = 0;
				}
			}
			tap0_3_def = hdmirx_rd_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE,
			MSK(4, 12), port);
			if (dfe3_tap0 < vga_tuning_min && !tap0_3_done) {
				tap0_3_dec = graytodecimal_t6x(tap0_3_def);
				tap0_3_dec += 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, MSK(4, 12),
					decimaltogray_t6x(tap0_3_dec), port);
				if (tap0_3_dec == 15)
					tap0_3_done = 1;
			} else if ((dfe3_tap0 > vga_tuning_max) && !tap0_3_done) {
				tap0_3_dec = graytodecimal_t6x(tap0_3_def);
				tap0_3_dec -= 1;
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_AFE, MSK(4, 12),
				    decimaltogray_t6x(tap0_3_dec), port);
				dfe3_tap0 = (data32 >> 24) & 0x7f;
				if (tap0_3_dec == 0)
					tap0_3_done = 1;
			} else {
				tap0_3_cnt++;
				if (tap0_3_cnt == 3) {
					tap0_3_done = 1;
					tap0_3_cnt = 0;
				}
			}
		} else {
			if (log_level & FRL_LOG) {
				rx_pr("tuning dfe0_tap0 = 0x%x\n", dfe0_tap0);
				rx_pr("tuning dfe1_tap0 = 0x%x\n", dfe1_tap0);
				rx_pr("tuning dfe2_tap0 = 0x%x\n", dfe2_tap0);
				rx_pr("tuning dfe3_tap0 = 0x%x\n", dfe3_tap0);
			}
			break;
		}
	}
	if (!tap0_0_done || !tap0_1_done || !tap0_2_done || !tap0_3_done) {
		if (log_level & FRL_LOG) {
			rx_pr("vga gain timeout\n");
			rx_pr("tuning dfe0_tap0 = 0x%x\n", dfe0_tap0);
			rx_pr("tuning dfe1_tap0 = 0x%x\n", dfe1_tap0);
			rx_pr("tuning dfe2_tap0 = 0x%x\n", dfe2_tap0);
			rx_pr("tuning dfe3_tap0 = 0x%x\n", dfe3_tap0);
		}
	}
	tap0_0_cnt = 0;
	tap0_1_cnt = 0;
	tap0_2_cnt = 0;
	tap0_3_cnt = 0;
	if (log_level & FRL_LOG)
		rx_pr("vga tuning done\n");
}

bool rx_is_power_off_t6x(u8 port)
{
	if (port <= E_PORT1)
		return hdmirx_rd_amlphy_t6x(T6X_HDMIRX20PHY_DCHA_MISC1, port) == 0;
	else
		return hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PHY_MISC0, port) == 0;
}

void rx_mute_t6x(bool en, u8 port_type)
{
	if (rx_info.chip_id != CHIP_ID_T6X)
		return;
	if (en) {
		if (port_type == TVIN_PORT_MAIN) {
			rx_mute_dual_video_rdma(E_RX_MUTE, E_RX_NA);
			if (tvin_get_game_mode_status(port_type))
				rx_mute_dual_video_vcbus(E_RX_MUTE, E_RX_NA);
			if (log_level & VIDEO_LOG)
				rx_pr("main port mute\n");
		} else if (port_type == TVIN_PORT_SUB) {
			rx_mute_dual_video_rdma(E_RX_NA, E_RX_MUTE);
			rx_mute_dual_video_vcbus(E_RX_NA, E_RX_MUTE);
			if (log_level & VIDEO_LOG)
				rx_pr("sub port mute\n");
		} else {
			rx_mute_dual_video_rdma(E_RX_NA, E_RX_NA);
			if (tvin_get_game_mode_status(port_type))
				rx_mute_dual_video_vcbus(E_RX_MUTE, E_RX_NA);
			rx_mute_dual_video_vcbus(E_RX_NA, E_RX_NA);
		}
	} else {
		if (port_type == TVIN_PORT_MAIN) {
			rx_mute_dual_video_rdma(E_RX_UNMUTE, E_RX_NA);
			rx_mute_dual_video_vcbus(E_RX_UNMUTE, E_RX_NA);
			if (log_level & VIDEO_LOG)
				rx_pr("main port unmute\n");
		} else if (port_type == TVIN_PORT_SUB) {
			rx_mute_dual_video_rdma(E_RX_NA, E_RX_UNMUTE);
			rx_mute_dual_video_vcbus(E_RX_NA, E_RX_UNMUTE);
			if (log_level & VIDEO_LOG)
				rx_pr("sub port unmute\n");
		} else {
			rx_mute_dual_video_rdma(E_RX_NA, E_RX_NA);
			rx_mute_dual_video_vcbus(E_RX_NA, E_RX_NA);
		}
	}
}

void rx_aud_pll_ctl_t6x(bool en, u8 port)
{
	int tmp = 0;

	if (rx_is_pip_on() && port == rx_info.sub_port) {
		rx_pr("%s sub_port mute\n", __func__);
		return;
	}

	if (en) {
		if (port == rx_info.main_port && vpcore1_select) {//to do
			/* switch to core1 no sound */
			tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
			tmp |= (1 << 24);
			tmp &= ~(1 << 25);
			wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
			wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0, 0x40001540);
			/* 0:tmds_clk 1:ref_clk 2:mpll_clk */
			wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL1,
			rx[port].phy.aud_div_1);
			wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL3,
				rx[port].phy.aud_div);
			if (rx[port].var.frl_rate) {
				audio_setting_for_aud21_t6x(rx[port].var.frl_rate,
				port);
			} else {
				hdmirx_wr_bits_amlphy_t6x(T6X_HDMIRX21PHY_DCHA_PI,
					MSK(2, 12), 0x0, port);
				hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
					_BIT(13), 0);
				hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL2,
					_BIT(19), 0);
			}
			//wr_reg_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0, 0x6000d540);
			hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
				_BIT(14), 1);
			hdmirx_wr_bits_clk_ctl(T6X_CLKCTRL_AUD21_PLL_CTRL0,
				_BIT(29), 1);
			if (rx[port].var.frl_rate) {
				tmp = hdmirx_rd_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2,
					port);
				hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2,
					(tmp | (1 << 30)), port);
			} else {
				hdmirx_wr_amlphy_t6x(T6X_HDMIRX21PLL_CTRL2,
					0, port);
			}
			rx_audio_pll_sw_update(port);
			//hdmirx_audio_fifo_rst(port);
		} else if (!vpcore1_select) {
			tmp = hdmirx_rd_top_common(HDMIRX_TOP_FSW_CNTL);
			tmp |= _BIT(8 + port * 2);
			hdmirx_wr_top_common(HDMIRX_TOP_FSW_CNTL, tmp);
			tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
			tmp |= (1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
			wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
			/* AUD_CLK=N/CTS*TMDS_CLK */
			wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL, 0x40001540);
			/* use mpll */
			tmp = 0;
			tmp |= 2 << 2; /* 0:tmds_clk 1:ref_clk 2:mpll_clk */
			wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL2, tmp);
			/* cntl3 2:0 000=1*cts 001=2*cts 010=4*cts 011=8*cts */
			wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL3,
				rx[port].phy.aud_div);
			if (log_level & AUDIO_LOG)
				rx_pr("aud div=%d\n",
					rd_reg_ana_ctl(ANACTL_AUD_PLL_CNTL3));
			wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL, 0x60001540);
			tmp = hdmirx_rd_top_common(TOP_ACR_CNTL_STAT) >> 31;
			if (log_level & AUDIO_LOG)
				rx_pr("audio pll lock:0x%x\n", tmp);
			rx_audio_pll_sw_update(port);
			//hdmirx_audio_fifo_rst(port);
		}
	} else {
		/* disable pll, into reset mode */
		hdmirx_audio_disabled(port);
		wr_reg_ana_ctl(ANACTL_AUD_PLL_CNTL, 0x0);
		tmp = rd_reg_clk_ctl(RX_CLK_CTRL2);
		tmp &= ~(1 << 8);// [    8] clk_en for cts_hdmirx_aud_pll_clk
		tmp &= ~(1 << 24);
		wr_reg_clk_ctl(RX_CLK_CTRL2, tmp);
	}
}
