// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "./edptx_clk_ctrl.h"
#include "../edptx_common.h"
#include "../edptx_reg_op.h"

static unsigned int tcon_div[][4] = {
	/* vx1pll_div214h, tcon_bypass_en, vx1pll_clk1x_selh */
	{0, 0, 1, 0x80},  /* div1 */
	{0, 0, 0, 0x80},  /* div2 */
	{1, 0, 0, 0x80},  /* div4 */
	{0, 0, 0, 0x0f},  /* div8 */
	{1, 0, 0, 0x0f},  /* div16 */
};

static void edptx_set_link_pll_a9(struct edptx_drv_s *edp, u8 port)
{
	struct edptx_clk_cfg_s *link_cconf = &edp->link_clk;
	u32 pll_ctrl0, pll_ctrl2, pll_ctrl3;
	u32 tcon_div_sel;
	u32 pll_ctrl0_reg, pll_ctrl1_reg, pll_ctrl2_reg, pll_ctrl3_reg, pll_ctrl4_reg,
		 pll_stts_reg;
	int cnt = 0;

	eDP_P_DBG(edp, port, "%s m=%u",  __func__, link_cconf->pll_m);

	tcon_div_sel = link_cconf->pll_tcon_div_sel;
	pll_ctrl0 = 0x13e00 | (link_cconf->pll_m << 0);
	pll_ctrl2 = link_cconf->pll_frac |
		(1 << 20) | /*pll sdm_en*/
		(tcon_div[tcon_div_sel][0] << 24) | /*div2_4_sel*/
		(tcon_div[tcon_div_sel][2] << 25) | /*clk1x_sel*/
		(tcon_div[tcon_div_sel][1] << 31);
	pll_ctrl3 =
		(0x3 << 10) |
		// (link_cconf->pll_od_sel[0] << 10) |
		(link_cconf->pll_od_sel[1] << 12) |
		(link_cconf->pll_od_sel[2] << 14) |
		(1 << 8) | /*tcon_od_en*/
		(tcon_div[tcon_div_sel][3]); /*tcon_bypass_en*/

	if (edp->idx == 0) {
		pll_ctrl0_reg = ANACTRL_TCON_PLL0_CNTL0_A9;
		pll_ctrl1_reg = ANACTRL_TCON_PLL0_CNTL1_A9;
		pll_ctrl2_reg = ANACTRL_TCON_PLL0_CNTL2_A9;
		pll_ctrl3_reg = ANACTRL_TCON_PLL0_CNTL3_A9;
		pll_ctrl4_reg = ANACTRL_TCON_PLL0_CNTL4_A9;
		pll_stts_reg  = ANACTRL_TCON_PLL0_STS_A9;
	} else {
		pll_ctrl0_reg = ANACTRL_TCON_PLL1_CNTL0_A9;
		pll_ctrl1_reg = ANACTRL_TCON_PLL1_CNTL1_A9;
		pll_ctrl2_reg = ANACTRL_TCON_PLL1_CNTL2_A9;
		pll_ctrl3_reg = ANACTRL_TCON_PLL1_CNTL3_A9;
		pll_ctrl4_reg = ANACTRL_TCON_PLL1_CNTL4_A9;
		pll_stts_reg  = ANACTRL_TCON_PLL1_STS_A9;
	}

set_pll_retry_t7:
	edp_ana_write(edp, pll_ctrl0_reg, pll_ctrl0);
	edp_ana_write(edp, pll_ctrl1_reg, 0x94401545);
	edp_ana_write(edp, pll_ctrl2_reg, pll_ctrl2);
	edp_ana_write(edp, pll_ctrl3_reg, pll_ctrl3);
	edp_ana_setb(edp, pll_ctrl0_reg, 1, 28, 1);
	edptx_delay_us(10);
	edp_ana_setb(edp, pll_ctrl0_reg, 1, 30, 1);
	edptx_delay_us(10);
	edp_ana_setb(edp, pll_ctrl0_reg, 1, 29, 1);
	if (edptx_pll_wait_lock(edp, pll_stts_reg, 31)) {
		edptx_delay_us(100);
		// edp_ana_setb(edp, pll_ctrl2_reg, 1, 5, 1);
	} else {
		if (cnt++ < PLL_RETRY_MAX)
			goto set_pll_retry_t7;
		eDP_ERR(edp, "pll lock failed");
	}
		/* set load to 0 */
	edp_ana_setb(edp, ANACTRL_TCON_PLL_VLOCK_A9, 0, 4, 1);
	/* select ANACTRL_TCON_PLL_VLOCK[4] as load */
	edp_ana_setb(edp, ANACTRL_TCON_PLL_VLOCK_A9, 0, 3, 1);
	/* enable load en*/
	edp_ana_setb(edp, ANACTRL_TCON_PLL0_CNTL0_A9, 1, 14, 1);
}

static void edptx_set_phy_dig_div(struct edptx_drv_s *edp, u8 port_to_pll)
{
	if (edp->idx) //edp1 use usb-dp phy
		return;

	eDP_DBG(edp, "%s(%u)", __func__, port_to_pll);

	//edp_ana_write(edp, ANACTRL_LVDS_TX_PHY_CNTL0_A9,
	//	(0 << 0) | //phy0_lvds_vx1
	//	(1 << 1) | //phy0_eDP
	//	(0 << 2) | //phy0_dsi
	//	(0 << 3) | //phy1_lvds_vx1
	//	(0 << 4) | //phy1_eDP
	//	(0 << 5) | //phy1_dsi
	//	(0 << 6) | //phy1_pll_sel
	//	(0 << 7)); //phy1_p2s_sel

	//                                         phy0_lvds_vx1 | phy0_eDP | phy0_dsi
	edp_ana_setb(edp, ANACTRL_LVDS_TX_PHY_CNTL0_A9, (0 << 0) | (1 << 1) | (0 << 2), 0, 3);

	//edp_ana_write(edp, ANACTRL_LVDS_TX_PHY_CNTL1_A9,
	//	(1 << 0) | //phy0_fifo_en
	//	(1 << 1) | //phy0_wr_en
	//	(0 << 2) | //phy1_fifo_en
	//	(0 << 3) | //phy1_wr_en
	//	(0x0 << 4) | //test_low
	//	(0x0 << 6) | //test_high
	//	(0x0 << 8) | //test_prbs
	//	(0 << 9)); //scan_reg

	//                                          phy0_fifo_en | phy0_wr_en
	edp_ana_setb(edp, ANACTRL_LVDS_TX_PHY_CNTL1_A9, (1 << 0) | (1 << 1), 0, 2);

	edp_ana_setb(edp, ANACTRL_LVDS_TX_PHY_CNTL2_A9, 0x155, 0, 10);
}

static void dptx_set_vclk_crt_by_path(struct edptx_drv_s *edp, u8 path)
{
	unsigned int reg_vid_clk_div, reg_vid_clk_ctrl;
	unsigned int encl_clk_sel_bit, encl_clk_sel;
	unsigned int encl_gate_bit;

	eDP_DBG(edp, "%s(%u)", __func__, path);

	switch (edp->idx) {
	case 1:
		reg_vid_clk_div = CLKCTRL_VIID_CLK_DIV_A9;
		reg_vid_clk_ctrl = CLKCTRL_VIID_CLK_CTRL_A9;
		encl_clk_sel_bit = 8;
		encl_clk_sel = 8;
		encl_gate_bit = 11;
		break;
	case 0:
	default:
		reg_vid_clk_div = CLKCTRL_VID_CLK_DIV_A9;
		reg_vid_clk_ctrl = CLKCTRL_VID_CLK_CTRL_A9;
		encl_clk_sel_bit = 12;
		encl_clk_sel = 0;
		encl_gate_bit = 10;
		break;
	}

	edp_clk_setb(reg_vid_clk_ctrl, 0, VCLK2_EN, 1);
	edptx_delay_us(2);

	// lcd_set_vid_pll_div_a9(pdrv);

	/* setup the XD divider value */
	edp_clk_setb(reg_vid_clk_div, (edp->vid_clk.xd - 1), VCLK2_XD, 8);
	edptx_delay_us(5);

	/* select dsi_pll_clk */
	edp_clk_setb(reg_vid_clk_ctrl, edp->vid_clk.vclk_sel, VCLK2_CLK_IN_SEL, 3);

	/* [15:12] encl_clk_sel, select vclk2_div1 */
	edp_clk_setb(CLKCTRL_VIID_CLK_DIV_A9, encl_clk_sel, encl_clk_sel_bit, 4);
	/* release vclk2_div_reset and enable vclk2_div */
	edp_clk_setb(reg_vid_clk_div, 1, VCLK2_XD_EN, 2);
	edptx_delay_us(5);
	edp_clk_setb(reg_vid_clk_ctrl, 1, VCLK2_DIV1_EN, 1);
	edp_clk_setb(reg_vid_clk_ctrl, 1, VCLK2_SOFT_RST, 1);
	edp_clk_setb(reg_vid_clk_ctrl, 0, VCLK2_SOFT_RST, 1);
	edptx_delay_us(5);

	/* enable CTS_ENCL clk gate */
	edp_clk_setb(CLKCTRL_VID_CLK_CTRL2_A9, 1, encl_gate_bit, 1);

	edp_clk_setb(reg_vid_clk_ctrl, 1, VCLK2_EN, 1);

	edptx_delay_us(2);
}

static void dptx_clk_cfg_print_t7(struct edptx_drv_s *edp)
{
}

/****************** eDP 1PLL model T7 **********************/
/* PLL_VCO / tcon_div --> eDP_PHY_clk                      */
/*              '--edp_div0&1 / PLL_CLK_DIV == VID_PLL_CLK */
/* VID_PLL_CLK    -->  .                         */
/* GP0(no)        -->  |                         */
/* HiFi(no)       -->  |                         */
/* MP1(no)        -->  |                         */
/* fclk_div3(667M)-->  --> / enc_xd == ENCL_clk  */
/* fclk_div4(500M)-->  |                         */
/* fclk_div5(400M)-->  |                         */
/* fclk_div7(286M)-->  `                         */
static void dptx_link_clk_config_a9(struct edptx_drv_s *edp, u8 port, u8 dptx_link_rate)
{
	struct edptx_clk_cfg_s *link_cconf = &edp->link_clk;

	switch (dptx_link_rate) {
	case eDP_LINK_RATE_HBR2:
		link_cconf->pll_m    = 450;
		link_cconf->pll_fvco = 5400000000ULL;
		link_cconf->pll_fout = 5400000000ULL;
		link_cconf->pll_tcon_div_sel = 0;
		break;
	case eDP_LINK_RATE_HBR:
		link_cconf->pll_m    = 450;
		link_cconf->pll_fvco = 5400000000ULL;
		link_cconf->pll_fout = 2700000000ULL;
		link_cconf->pll_tcon_div_sel = 1;
		break;
	case eDP_LINK_RATE_RBR:
	default:
		link_cconf->pll_m    = 270;
		link_cconf->pll_fvco = 3240000000ULL;
		link_cconf->pll_fout = 1620000000ULL;
		link_cconf->pll_tcon_div_sel = 1;
		break;
	}
	link_cconf->pll_n    = 1;
	link_cconf->pll_frac = 0;
	link_cconf->pll_frac_half_shift = 0;
	link_cconf->pll_clk_div_sel = CLK_DIV_SEL_1;

	eDP_P_PR(edp, port, "PLL_M=%u, out=%llu", link_cconf->pll_m, link_cconf->pll_fout);
}

static void dptx_vid_clk_config_a9(struct edptx_drv_s *edp, u32 pixel_clk)
{
	struct edptx_pll_data_s *vid_data = (struct edptx_pll_data_s *)edp->vid_clk.pll_data;

	unsigned long long pll_vco, pll_out;
	u32 enc_xd;
	u8 od0, od1;
	u8 od0_table[5] = {1, 2, 4, 8, 16};
	u8 od1_table[4] = {1, 2, 4, 8};

	if (!vid_data)
		return;

	for (enc_xd = 1; enc_xd < vid_data->xd_max; enc_xd++) {
		pll_out = enc_xd;
		pll_out = pll_out * pixel_clk;

		if (pll_out > vid_data->pll_out_range[1] ||
			pll_out < vid_data->pll_out_range[0])
			continue;

		for (od0 = 0; od0 < 5; od0++) {
			for (od1 = 0; od1 < 4; od1++) {
				pll_vco = pll_out;
				pll_vco = pll_vco * od0_table[od0] * od1_table[od1];
				if (pll_vco < vid_data->pll_vco_range[0] ||
				    pll_vco > vid_data->pll_vco_range[1]) {
					continue;
				}
				edp->vid_clk.pll_clk_div_sel = 1;
				edp->vid_clk.xd = enc_xd;
				edp->vid_clk.vclk_sel = 1; //0:hdmi 1:pll2 2:pll0 3:pll3 4:div3
				edp->vid_clk.clk_src = 0; //LINK_CLK
				edp->vid_clk.fin = pll_vco;
				edp->vid_clk.fout = pixel_clk;
				edp->vid_clk.pll_od_sel[0] = od0;
				edp->vid_clk.pll_od_sel[1] = od1;
				edp->vid_clk.pll_fout = pll_out;

				edptx_check_vco(&edp->vid_clk, pll_vco);
				goto edptx_vid_clk_done;
			}
		}
	}
	eDP_PR(edp, "%s [%u] failed", __func__, pixel_clk);
	return;

edptx_vid_clk_done:
	eDP_PR(edp, "vco=%lluHz pll_out:%lluHz xd[%hu]->fout=%lluhz",
		pll_vco, edp->vid_clk.pll_fout, edp->vid_clk.xd, edp->vid_clk.fout);
}

static void dptx_link_clk_set_a9(struct edptx_drv_s *edp, u8 port)
{
	edptx_set_link_pll_a9(edp, port);
	edptx_set_phy_dig_div(edp, 0);
}

static void dptx_set_video_pll_a9(struct edptx_drv_s *edp)
{
	unsigned int pll_ctrl0, pll_ctrl1;
	u8 cnt = 0;

dptx_set_video_pll_a9_retry:
	pll_ctrl0 = 0x600a000 |
		(1 << 27) | //0.5en
		((edp->vid_clk.pll_od_sel[0] & 0x7) << 19) |
		((edp->vid_clk.pll_od_sel[1] & 0x3) << 22) |
		(edp->vid_clk.pll_m & 0x1ff);
	pll_ctrl1 = 0x11480000 | (edp->vid_clk.pll_frac & 0x1ffff);
	edp_ana_write(edp, ANACTRL_TCON_PLL2_CNTL0_A9, pll_ctrl0);
	edp_ana_write(edp, ANACTRL_TCON_PLL2_CNTL1_A9, pll_ctrl1);
	edp_ana_write(edp, ANACTRL_TCON_PLL2_CNTL2_A9, 0x120c1230);
	edp_ana_setb(edp, ANACTRL_TCON_PLL2_CNTL0_A9, 1, 28, 1);
	edptx_delay_us(20);
	edp_ana_setb(edp, ANACTRL_TCON_PLL2_CNTL0_A9, 1, 29, 1);
	edptx_delay_us(20);
	edp_ana_setb(edp, ANACTRL_TCON_PLL2_CNTL0_A9, 1, 30, 1);
	if (edptx_pll_wait_lock(edp, ANACTRL_TCON_PLL2_STS_A9, 31)) {
		edptx_delay_us(100);
	} else {
		if (cnt++ < PLL_RETRY_MAX)
			goto dptx_set_video_pll_a9_retry;
		eDP_ERR(edp, "pll lock failed");
	}
}

static void dptx_vid_clk_set_a9(struct edptx_drv_s *edp)
{
	dptx_set_video_pll_a9(edp);
	dptx_set_vclk_crt_by_path(edp, 0);
}

static void dptx_clk_ssc_switch_a9(struct edptx_drv_s *edp, u8 port, u8 status)
{
}

static struct edptx_pll_data_s edptx_link_clk_pll0_data = {
	.pll_od_fb = 0,
	.fin_base = 24000000,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_vco_range = {3000000000ULL, 6000000000ULL},
	.pll_out_range = {187500000,     3100000000ULL},
	.pll_div_in_fmax = 3100000000ULL,
	.pll_div_out_fmax = 1500000000,
	.od_cnt = 3,
	.div_sel_max = CLK_DIV_SEL_MAX,

	.xd_out_fmax = 750000000,
	.xd_max = 256,

	.ss_level_max = 60,
	.ss_freq_max = 6,
	.ss_mode_max = 2,
	.ss_dep_base = 500, //ppm
	.ss_dep_sel_max = 12,
	.ss_str_m_max = 10,
	.pll_0_5_div_en = 1,
};

static struct edptx_pll_data_s edptx_video_clk_pll2_data = {
	.pll_od_fb = 0,
	.fin_base = 24000000,
	.pll_frac_range = (1 << 17),
	.pll_frac_sign_bit = 18,
	.pll_vco_range = {1400000000ULL, 2800000000ULL},
	.pll_out_range = {  18750000ULL, 2800000000ULL},
	.pll_div_in_fmax = 2800000000ULL,
	.pll_div_out_fmax = 1500000000U,
	.od_cnt = 2,
	.div_sel_max = CLK_DIV_SEL_MAX,

	.xd_out_fmax = 750000000,
	.xd_max = 256,
	.pll_0_5_div_en = 1,
};

struct edptx_clk_op_s dptx_clk_op_a9 = {
	.clk_config_print = dptx_clk_cfg_print_t7,
	.clktree_set      = NULL,
	.link_clk_config  = dptx_link_clk_config_a9,
	.link_clk_set     = dptx_link_clk_set_a9,
	.vid_clk_config   = dptx_vid_clk_config_a9,
	.vid_clk_set      = dptx_vid_clk_set_a9,
	.clk_ssc_switch   = dptx_clk_ssc_switch_a9,
	// void (*prbs)(struct edptx_drv_s *edp);
};

struct edptx_clk_op_s *edptx_clk_op_init_a9(struct edptx_drv_s *edp)
{
	if (edp->idx == 0) {
		edp->link_clk.pll_data = &edptx_link_clk_pll0_data;
		edp->vid_clk.pll_data = &edptx_video_clk_pll2_data;
	} else {
		edp->vid_clk.pll_data = &edptx_link_clk_pll0_data;
		edp->link_clk.pll_data = &edptx_video_clk_pll2_data;
	}

	return &dptx_clk_op_a9;
}
