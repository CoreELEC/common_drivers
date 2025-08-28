// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/delay.h>
#include <linux/amlogic/arm-smccc.h>

#include "hdmitx_hw_platform.h"
#include "hdmitx_hw_core.h"

/* Min 3GHz */
#define MIN_HTXPLL_VCO 3000000
/* Max 6GHz */
#define MAX_HTXPLL_VCO 6000000

#define WAIT_FOR_PLL_LOCKED(_reg) \
	do { \
		unsigned int st = 0; \
		int cnt = 10; \
		unsigned int reg = _reg; \
		while (cnt--) { \
			usleep_range(50, 60); \
			st = (((hd21_read_reg(reg) >> 31) & 0x1) == 1); \
			if (st) \
				break; \
			else { \
				/* reset hpll */ \
				hd21_set_reg_bits(reg, 1, 30, 1); \
				hd21_set_reg_bits(reg, 0, 30, 1); \
			} \
		} \
		if (cnt < 9) \
			HDMITX_INFO("pll[0x%x] reset %d times\n", reg, 9 - cnt);\
	} while (0)

static const char od_map[9] = {
	0, 0, 1, 0, 2, 0, 0, 0, 3,
};

void hdmitx_disable_a9_plls(struct hdmitx21_dev *hdev)
{
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0);
}

/* htx pll VCO output: (3G, 6G), for tmds */
static void set_a9_htxpll_clk_other(const u32 clk, const bool frl_en)
{
	u32 quotient;
	u32 remainder;

	if (clk < 3000000 || clk >= 6000000) {
		pr_err("%s[%d] clock should be 4~6G\n", __func__, __LINE__);
		return;
	}

	quotient = clk / 12000;
	remainder = clk - quotient * 12000;
	/* remainder range: 0 ~ 23999, 0x5dbf, 15bits */
	remainder *= 1 << 17;
	remainder /= 12000;
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x00017000 | (quotient << 0));
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL1, 0x9040137d);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL2, 0x04000000);
	hd21_write_reg(ANACTRL_HDMIPLL_CTRL3, 0x01160000 | remainder);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 28, 1);
	usleep_range(10, 20);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 18, 1);
	usleep_range(10, 20);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 30, 1);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 0, 18, 1);
	usleep_range(80, 90);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0, 1, 29, 1);
	usleep_range(80, 90);
	WAIT_FOR_PLL_LOCKED(ANACTRL_HDMIPLL_CTRL0);
}

void hdmitx_set_a9_htxpll_clk_out(const u32 clk, u32 div)
{
	u32 pll_od0 = 0;
	u32 pll_od00 = 0;
	u32 pll_od01 = 0;
	u32 pll_od2 = 0;
	u32 pll_od20 = 0;
	u32 pll_od21 = 0;
	u32 pll_od1 = 0;
	struct hdmitx21_dev *hdev = get_hdmitx21_device();
	enum hdmi_colorspace cs = HDMI_COLORSPACE_YUV444;
	enum hdmi_color_depth cd = COLORDEPTH_24B;
	struct hdmi_format_para *para = NULL;

	if (!hdev)
		return;

	para = &hdev->tx_comm.fmt_para;
	cs = para->cs;
	cd = para->cd;

	HDMITX_INFO("%s[%d] htxpll vco %d div %d\n", __func__, __LINE__, clk, div);

	if (clk <= 3000000 || clk > 6000000) {
		pr_err("%s[%d] %d out of htxpll range(3~6G]\n", __func__, __LINE__, clk);
		return;
	}

	set_a9_htxpll_clk_other(clk, hdev->frl_rate ? 1 : 0);

	/* pll_od00 */
	if ((div % 8) == 0) {
		/* div8 */
		pll_od00 = 3;
		div = div / 8;
	} else if ((div % 4) == 0) {
		/* div4 */
		pll_od00 = 2;
		div = div / 4;
	} else if ((div % 2) == 0) {
		/* div2 */
		pll_od00 = 1;
		div = div / 2;
	}

	/* pll_od01 */
	if ((div % 8) == 0) {
		pll_od01 = 3;
		div = div / 8;
	} else if ((div % 4) == 0) {
		pll_od01 = 2;
		div = div / 4;
	} else if ((div % 2) == 0) {
		pll_od01 = 1;
		div = div / 2;
	}

	/* pll_od0 */
	pll_od0 = (pll_od01 << 3) | pll_od00;

	/* pll_od20 for clk to phy */
	if ((div % 4) == 0) {
		pll_od20 = 2;
		div = div / 4;
	} else if ((div % 2) == 0) {
		pll_od20 = 1;
		div = div / 2;
	}

	/* pll_od21 for clk_out2 */
	if (cs == HDMI_COLORSPACE_YUV420)
		pll_od21 = pll_od20;
	else
		pll_od21 = pll_od20 + 1;

	pll_od2 = (pll_od20 << 3) | pll_od21;

	/* pll_od1 */
	if (cs != HDMI_COLORSPACE_YUV422) {
		if (cd == COLORDEPTH_24B)
			/* pll_div3 = 5; */
			pll_od1 = 0;
		else if (cd == COLORDEPTH_30B)
			/* pll_div3 = 6.25; */
			pll_od1 = 1;
		else if (cd == COLORDEPTH_36B)
			/* pll_div3 = 7.5; */
			pll_od1 = 2;
	}

	/* tx_spll_hdmi_clk_select */
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3_A9, 1, 19, 1);
	HDMITX_INFO("pll_od0 = %d, pll_od2 = %d, pll_od1 = %d\n",
		pll_od0, pll_od2, pll_od1);
	/* tx_spll_lock_by_pass_alo */
	if (hdev->tx21_hw.clk_analog_path)
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3_A9, pll_od1, 22, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3_A9, pll_od2, 24, 6);
	hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL0_A9, pll_od0, 20, 6);
}

void hdmitx_set_phy_by_mode_a9(u32 mode)
{
	struct arm_smccc_res res;
	/* this will get from efuse */
	u8 rterm = 0;

	switch (mode) {
	/* 5.94/4.5/3.7Gbps */
	case HDMI_PHYPARA_6G:
	case HDMI_PHYPARA_4p5G:
	case HDMI_PHYPARA_3p7G:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x8003cafb);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x2555);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x4ef00b);
		break;
	/* 2.97Gbps */
	case HDMI_PHYPARA_3G:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x800380dd);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x2555);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x4ef00b);
		break;
	/* 1.485Gbps, and below */
	case HDMI_PHYPARA_270M:
	case HDMI_PHYPARA_DEF:
	default:
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x82038088);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x2555);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x4ef00b);
		break;
	}

	/* write Rterm */
	arm_smccc_smc(HDCPTX_IOOPR, HDMITX_GET_RTERM, 0, 0, 0, 0, 0, 0, &res);
	rterm = (u8)((res.a0) & 0xff);
	/* default value when efuse invalid, 0xff indicate efuse invalid */
	if (rterm != 0xff) {
		HDMITX_INFO("%s[%d] rterm = %d\n", __func__, __LINE__, rterm);
		hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL0, rterm, 28, 4);
	} else {
		HDMITX_INFO("efuse invalid, use default value\n");
	}

	/* The bit with resetn is configured later than other bits. */
	usleep_range(100, 110);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 3, 10, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 1, 4, 1);
	/* finally config bit[30:28] */
	usleep_range(1000, 1010);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 3, 28, 2);
	hd21_set_reg_bits(ANACTRL_HDMIPHY_CTRL3, 1, 30, 1);
}

void hdmitx21_sys_reset_a9(void)
{
	/* Refer to system-Registers.docx */
	/* hdmi_tx */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 29);
	/* hdmitxphy */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 22);
	/* vid_pll_div */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 19);
	/* hdmitx_apb */
	hd21_write_reg(RESETCTRL_RESET0, 1 << 16);
}

void hdmitx_set_hpll_sspll_a9(enum hdmi_vic vic)
{
	switch (vic) {
	case HDMI_16_1920x1080p60_16x9:
	case HDMI_31_1920x1080p50_16x9:
	case HDMI_4_1280x720p60_16x9:
	case HDMI_19_1280x720p50_16x9:
	case HDMI_5_1920x1080i60_16x9:
	case HDMI_20_1920x1080i50_16x9:
		/* enable ssc, need update electric to 0x0100 */
		/* enable ssc */
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL3_A9, 3, 20, 2);
		/* set ssc 1000 ppm */
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL2_A9, 2, 16, 4);
		/* SS strength multiplier */
		hd21_set_reg_bits(ANACTRL_HDMIPLL_CTRL2_A9, 1, 4, 1);
		break;
	default:
		break;
	}
}

/*
 * CLKCTRL_HTX_CLK_CTRL0 bit8 gate for cts_hdmitx_prif_clk
 * it's necessary for register access of controller
 * CLKCTRL_HTX_CLK_CTRL0 bit24 gate for cts_hdmitx_200m_clk
 * it's necessary for i2c clk
 * CLKCTRL_HDMI_CLK_CTRL bit8 gate for cts_hdmitx_sys_clk
 * it's necessary for register access of hdmitx top
 */
void hdmitx_a9_clock_gate_ctrl(struct hdmitx21_dev *hdev, bool en)
{
	int gate_bit_mask = hdev->tx21_hw.gate_bit_mask;

	HDMITX_INFO("gate_bit_mask = 0x%x, enable: %d\n", gate_bit_mask, en);
	if (gate_bit_mask & BIT(1))
		hd21_set_reg_bits(CLKCTRL_VID_PLL_CLK0_DIV, en, 19, 1);
	if (gate_bit_mask & BIT(2))
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, en, 4, 1);
	if (gate_bit_mask & BIT(3))
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, en, 20, 1);
	if (gate_bit_mask & BIT(4))
		hd21_set_reg_bits(CLKCTRL_ENC_HDMI_CLK_CTRL, en, 12, 1);
	if (gate_bit_mask & BIT(5))
		hd21_set_reg_bits(CLKCTRL_VID_CLK0_CTRL2, en, 3, 1);
	if (gate_bit_mask & BIT(6))
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL1, en, 8, 1);
	if (gate_bit_mask & BIT(7))
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL0, en, 24, 1);
	if (gate_bit_mask & BIT(8))
		hd21_set_reg_bits(CLKCTRL_HTX_CLK_CTRL0, en, 8, 1);
	if (gate_bit_mask & BIT(9))
		hd21_set_reg_bits(CLKCTRL_HDMI_CLK_CTRL, en, 8, 1);

	/* this will enable during the mode setting */
	if (gate_bit_mask & BIT(10)) {
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL0, 0x0);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL5, 0x0);
	}
	/* power off need */
	if (gate_bit_mask & BIT(11)) {
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0x704efc1b);
		hd21_write_reg(ANACTRL_HDMIPHY_CTRL3, 0xc1b);
	}
	usleep_range(1, 10);
	/* this will enable during the pll setting */
	if (gate_bit_mask & BIT(12))
		hd21_write_reg(ANACTRL_HDMIPLL_CTRL0, 0x0);
}

