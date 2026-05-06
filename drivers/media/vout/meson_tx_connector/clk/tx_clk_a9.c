// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/printk.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/amlogic/media/vout/meson_tx_connector/clk/meson_tx_clk.h>

#include "meson_tx_clk_reg.h"
#include "meson_tx_clk_internal.h"

/* unit: kHz */
#define PIXEL_PLL_VCO_MIN  1400000
#define PIXEL_PLL_VCO_MAX  2800000
#define XTAL_BASE_CLK      24000

/*
 * set path and divider between analog pll output(hdmitx_clk_out2
 * or pll_pix_clk) and CRT_VIDEO
 */
static void tx_vid_pll_pix_div_set(struct meson_tx_clk *tx_clk, u8 pll_idx, u8 div)
{
	u32 pll_ctrl0_reg = ANACTRL_TCON_PLL2_CNTL0;
	u32 pll_od1 = 0;
	u32 pll_od2 = 0;

	if (!tx_clk)
		return;
	if (div > 128) {
		pr_err("meson_tx_clk pixel pll div(%d) not supported\n", div);
		return;
	}

	/* Note: there are two pixel pll,
	 * ANACTRL_TCON_PLL2_CNTLx(pixel_pll0) is same as ANACTRL_TCON_PLL3_CNTLx(pixel_pll1)
	 */
	if (pll_idx > 1) {
		pr_err("%d exceed a9 pixel pll index(0/1)\n", pll_idx);
		return;
	} else if (pll_idx == 1) {
		pll_ctrl0_reg = ANACTRL_TCON_PLL3_CNTL0;
	} else {
		pll_ctrl0_reg = ANACTRL_TCON_PLL2_CNTL0;
	}

	/* pixel_pll_ctrl0[21:19]: pll_od1 */
	if ((div % 16) == 0) {
		/* div16 */
		pll_od1 = 4;
		div /= 16;
	} else if ((div % 8) == 0) {
		/* div8 */
		pll_od1 = 3;
		div /= 8;
	} else if ((div % 4) == 0) {
		/* div4 */
		pll_od1 = 2;
		div /= 4;
	} else if ((div % 2) == 0) {
		/* div2 */
		pll_od1 = 1;
		div /= 2;
	} else {
		pll_od1 = 0;
		div = 1;
	}

	/* pixel_pll_ctrl0[23:22]: pll_od2 */
	if ((div % 8) == 0) {
		/* div8 */
		pll_od2 = 3;
		div /= 8;
	} else if ((div % 4) == 0) {
		/* div4 */
		pll_od2 = 2;
		div /= 4;
	} else if ((div % 2) == 0) {
		/* div2 */
		pll_od2 = 1;
		div /= 2;
	} else {
		pll_od2 = 0;
		div = 1;
	}

	tx_clk_set_reg_bits(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl0_reg, pll_od1, 19, 3);
	tx_clk_set_reg_bits(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl0_reg, pll_od2, 22, 2);
}

static int wait_for_pll_lock(struct meson_tx_clk *tx_clk, u32 st_reg, u8 lock_bit, u32 loop_cnt)
{
	unsigned int st = 0;
	u32 cnt = loop_cnt;

	if (!tx_clk || lock_bit >= 32)
		return -1;

	while (cnt > 0) {
		usleep_range(50, 60);
		st = (tx_clk_read_reg(tx_clk->reg_io_base[ANACTRL_REG], st_reg) >> lock_bit) & 0x1;
		if (st == 0x1)
			break;
		cnt--;
	}
	if (cnt == 0)
		return -1;

	if (cnt < loop_cnt)
		pr_info("warning: pll[0x%x] lock retry %d times\n", st_reg, loop_cnt - cnt);

	return 0;
}

/*
 * clk_freq: VCO frequency, for example: 1485M set_pix_pll0(clk_freq = 148500)
 * pll_idx 0/1 for pixel_pll0/1
 */
static int tx_clk_set_tcon_pll(struct meson_tx_clk *tx_clk, u8 pll_idx,  u32 clk_feq)
{
	u32 pll_m = 0;
	u32 pll_frac = 0;
	u32 tmp = 0;
	u32 pll_ctrl0_reg, pll_ctrl1_reg, pll_ctrl2_reg, pll_sts_reg;

	/* for pll frac part */
	u32 remainder = 0;
	u32 rem_1 = 0;
	u32 rem_2 = 0;

	u32 clk_base = XTAL_BASE_CLK;

	if (!tx_clk)
		return -1;
	if (clk_feq < PIXEL_PLL_VCO_MIN || clk_feq > PIXEL_PLL_VCO_MAX) {
		pr_err("meson_tx_clk/a9: pixel clk_feq %d is out of VCO range (%d ~ %d)\n",
			clk_feq, PIXEL_PLL_VCO_MIN, PIXEL_PLL_VCO_MAX);
		return -1;
	}

	/* Note: there are two pixel pll,
	 * ANACTRL_TCON_PLL2_CNTLx(pixel_pll0) is same as ANACTRL_TCON_PLL3_CNTLx(pixel_pll1)
	 */
	if (pll_idx > 1) {
		pr_err("%d exceed a9 pixel pll index(0/1)\n", pll_idx);
		return -1;
	} else if (pll_idx == 1) {
		pll_ctrl0_reg = ANACTRL_TCON_PLL3_CNTL0;
		pll_ctrl1_reg = ANACTRL_TCON_PLL3_CNTL1;
		pll_ctrl2_reg = ANACTRL_TCON_PLL3_CNTL2;
		pll_sts_reg = ANACTRL_TCON_PLL3_STS;
	} else {
		pll_ctrl0_reg = ANACTRL_TCON_PLL2_CNTL0;
		pll_ctrl1_reg = ANACTRL_TCON_PLL2_CNTL1;
		pll_ctrl2_reg = ANACTRL_TCON_PLL2_CNTL2;
		pll_sts_reg = ANACTRL_TCON_PLL2_STS;
	}

	clk_base = XTAL_BASE_CLK / 2;
	if (clk_feq % clk_base == 0) {
		pll_m = clk_feq / clk_base;
		pll_frac = 0;
	} else {
		pll_m = clk_feq / clk_base;
		remainder = clk_feq - pll_m * clk_base;
		/* remainder range: 0 ~ 99999, 0x1869f, 17bits */
		/* convert remainder to 0 ~ 2^17 */
		if (remainder) {
			rem_1 = remainder / 16;
			rem_2 = remainder - rem_1 * 16;
			rem_1 *= 1 << 17;
			rem_1 /= 750;
			rem_2 *= 1 << 13;
			rem_2 /= 750;
			pll_frac = rem_1 + rem_2;
		}
	}

	/* bit27: div0.5 enable, bit16: N, bit[8:0]: M */
	tmp = (0x1 << 27) |
		(0x1 << 26) |
		(0x1 << 25) |
		(0x0 << 22) |
		(0x0 << 19) |
		(0x0 << 16) |
		(0x1 << 15) |
		(0x1 << 13) |
		(0x0 << 9) |
		(pll_m << 0);
	tx_clk_write_reg(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl0_reg, tmp);

	tmp = (0x1 << 28) |
		(0x0 << 26) |
		(0x5 << 22) |
		(0x1 << 19) |
		(pll_frac << 0);
	tx_clk_write_reg(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl1_reg, tmp);

	tmp = (0x1 << 31) |
		(0x1 << 30) |
		(0x1 << 28) |
		(0x0 << 27) |
		(0x1 << 25) |
		(0x0 << 24) |
		(0x0 << 23) |
		(0x0 << 20) |
		(0x0 << 18) |
		(0x0 << 17) |
		(0x0 << 13) |
		(0x4 << 10) |
		(0x2 << 8) |
		(0x0 << 6) |
		(0x1 << 5) |
		(0x1 << 4) |
		(0 << 0);
	tx_clk_write_reg(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl2_reg, tmp);

	usleep_range(8, 12);
	/* [28] pll enable */
	tx_clk_set_reg_bits(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl0_reg, 1, 28, 1);
	usleep_range(8, 12);
	/* [29] pll reset_n for sdm_div release reset */
	tx_clk_set_reg_bits(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl0_reg, 1, 29, 1);
	usleep_range(45, 55);
	/* [30] pll lock reset_n */
	tx_clk_set_reg_bits(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl0_reg, 1, 30, 1);
	usleep_range(75, 85);

	/* bit31 lock bit, retry lock loop cnt: 10 */
	if (wait_for_pll_lock(tx_clk, pll_sts_reg, 31, 10) < 0)
		pr_err("meson_tx_connector: video pixel_pll%d unlock!\n", pll_idx);
	return 0;
}

/*
 * VCO for video clk, and phy clk which is coherent with video clk.
 *
 * 1.connector_type: differ pll source for different connector type
 * 2.vco_clk: VCO frequency before divide to connector phy/digital
 * 3.clk_to_crt_video is clk for digital video, such as pixel/venc clk
 * and may be for tmds_clk in special case.
 * clk_to_crt_video is divided from vco_clk
 */
static int tx_pixel_pll_set_a9(struct meson_tx_clk *tx_clk,
	enum tx_clk_type clk_type, u8 pll_idx, u32 vco_clk, u32 clk_to_crt_video)
{
	u8 div = 1;
	u32 vco_clk_tmp = vco_clk;

	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	pr_info("clk_type: %d, pixel_pll_idx: %d, vco_clk: %d, crt_video_clk: %d\n",
		clk_type, pll_idx, vco_clk, clk_to_crt_video);

	div = DIV_ROUND_CLOSEST_ULL(vco_clk_tmp, clk_to_crt_video);

	/* 1. pixel PLL OD: pixel_pll_ctrl0[21:19] for(1/2/4/8/16) and [23:22] for (1/2/4/8),
	 * the maximum PLL OD is 16 * 8 = 128;
	 * 2. video pixel clk range:
	 * PIXEL_PLL_VCO_MIN / 128 ~ PIXEL_PLL_VCO_MAX / 1, 11.937MHz ~ 2.8GHz
	 */
	do {
		if (vco_clk_tmp >= PIXEL_PLL_VCO_MIN && vco_clk_tmp <= PIXEL_PLL_VCO_MAX)
			break;
		div *= 2;
		vco_clk_tmp *= 2;
	} while (div <= 128);

	if (div > 128) {
		pr_err("divider(%d) for pixel pll OD abnormal!\n", div);
		return -1;
	}
	if (clk_type == CLK_DPTX) {
		tx_clk_set_tcon_pll(tx_clk, pll_idx, vco_clk_tmp);
		tx_vid_pll_pix_div_set(tx_clk, pll_idx, div);
	} else {
		pr_err("only for dptx PLL!\n");
	}
	return 0;
}

static int tx_pixel_pll_disable_a9(struct meson_tx_clk *tx_clk, enum tx_clk_type clk_type)
{
	struct meson_tx_clk_data *clk_cfg = NULL;
	u32 pll_ctrl0_reg = ANACTRL_TCON_PLL2_CNTL0;
	u32 pll_idx = 0;

	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	clk_cfg = &tx_clk->tx_clk_cfg;
	/* assume video_pll_idx = venc index, that's to say
	 * venc0(V1 crt divider) use pixel_pll0,
	 * venc1(V2 crt divider) use pixel_pll1
	 */
	pll_idx = clk_cfg->clk_div_path & BIT(0);

	if (pll_idx > 1) {
		pr_err("%d exceed a9 pixel pll index(0/1)\n", pll_idx);
		return -1;
	} else if (pll_idx == 1) {
		pll_ctrl0_reg = ANACTRL_TCON_PLL3_CNTL0;
	} else {
		pll_ctrl0_reg = ANACTRL_TCON_PLL2_CNTL0;
	}

	/* disable pll & set reset pll */
	tx_clk_set_reg_bits(tx_clk->reg_io_base[ANACTRL_REG], pll_ctrl0_reg, 0, 28, 3);
	return 0;
}

/* clk_type: differ phy clk source by connector type
 *
 * if phy link clk is not coherent with video clk(such
 * as FRL link or dp link), phy clk is divided from PLL independent
 * from pixel pll;
 * if pixel clk and phy clk are coherent, phy clk is divided
 * from pixel pll which is set by pixel_pll_set;
 */

static int tx_phy_clk_set_a9(struct meson_tx_clk *tx_clk, enum tx_clk_type clk_type,
			     u32 clk_todig_phy)
{
	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}
	pr_info("%s TODO\n", __func__);
	return 0;
}

static int tx_phy_clk_disable_a9(struct meson_tx_clk *tx_clk, enum tx_clk_type clk_type)
{
	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	pr_info("%s TODO\n", __func__);
	return 0;
}

/*
 * Function: tx_crt_video_clk_set_a9
 *
 * set venc/fe/pixel clk path/divider
 * clk_to_crt_video: input clk to crt_video
 * vclk_sel 0: V1, 1:V2. assume V1 connect to VENC0 and V2 connect to VENC1
 * crt_video_clk_out: output clk from crt_video
 *
 * there have 2 parallel set clock generator: V1 and V2,
 * TO confirm: V1 connect to VENC0, and V2 connect to VENC1
 * by default and can't be changed
 *
 * vc_id(0) refers to the primary display device, vc_id(1)
 * refers to the secondary display device, the display device
 * may be one of dptx/hdmitx/edp/dsi/vbo/lvds
 *
 * vc_id0: pixel0/1 pll-->V1_VENC0/V2_VENC1-->HDMI_IF_unit0
 * vc_id1: pixel0/1 pll-->V1_VENC0/V2_VENC1-->HDMI_IF_unit1
 *
 * Note: need to use different VENC between connectors
 * hdmitx/DP/eDP/dsi/vbo, this is decided by drm encoder.
 */
static int tx_crt_video_clk_set_a9(struct meson_tx_clk *tx_clk,
	u32 clk_to_crt_video, u8 vclk_sel, u32 crt_video_clk_out)
{
	/* 0:vid_pll_clk_pre; 1:pixel0_pll_clkout; 2:vid_pll0_clk;
	 * 3:pixel1_pll_clkout;
	 * the clk rate from clk_src equals to clk_to_crt_video
	 */
	u8 clk_src = 0;
	u8 clk_div = 1;

	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}
	if (crt_video_clk_out == 0) {
		pr_err("%s crt_video_clk_out invalid\n", __func__);
		return -EINVAL;
	}

	clk_div = clk_to_crt_video / crt_video_clk_out;

	if (vclk_sel == 0) {
		/* step1: set crt_video_clk source and divider */
		/* clk source: V1 always use pixel0_pll as source by default */
		clk_src = 1;
		/* path/div set for V1 */
		/* bit[19]: disable clk_div0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL, 0, 19, 1);
		/* bit[18:16]: cntl_clk_in_sel */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL, clk_src, 16, 3);
		/* bit[7:0]: cntl_xd0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_DIV, clk_div - 1, 0, 8);
		/* bit[19]: enable clk_div0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL, 1, 19, 1);
		/* bit[0]: enable v1 div1 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL, 1, 0, 1);
	} else if (vclk_sel == 1) {
		/* step1: set crt_video_clk source and divider */
		/* clk source: V2 always use pixel1_pll as source by default */
		clk_src = 3;
		/* path/div set for V2 */
		/* bit[19]: disable clk_div0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_CTRL, 0, 19, 1);
		/* bit[18:16]: cntl_clk_in_sel */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_CTRL, clk_src, 16, 3);
		/* bit[7:0]: cntl_xd0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_DIV, clk_div - 1, 0, 8);
		/* bit[19]: enable clk_div0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_CTRL, 1, 19, 1);
		/* v2 div1 enable */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_CTRL, 1, 0, 1);
	} else {
		pr_err("%s invalid vclk_sel(%d)\n", __func__, vclk_sel);
		return -EINVAL;
	}
	return 0;
}

/* vclk_sel 0: V1, 1:V2 */
static int tx_crt_video_clk_disable_a9(struct meson_tx_clk *tx_clk, u8 vclk_sel)
{
	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	if (vclk_sel == 0) {
		/* path/div set for V1 */
		/* bit[19]: disable clk_div0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL, 0, 19, 1);
		/* bit[0]: v1 div1 disable */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL, 0, 0, 1);
	} else if (vclk_sel == 1) {
		/* path/div set for V2 */
		/* bit[19]: disable clk_div0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_CTRL, 0, 19, 1);
		/* bit[0]: v2 div1 disable */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_CTRL, 0, 0, 1);
	} else {
		pr_err("%s invalid vclk_sel(%d)\n", __func__, vclk_sel);
		return -EINVAL;
	}
	return 0;
}

/*
 * crt_video_clk_out: clk set crt_video_clk_set_a9()
 * vclk_sel: 0: V1, 1:V2. assume V1 connect to VENC0 and V2 connect to VENC1
 */
static int tx_venc_clk_set_a9(struct meson_tx_clk *tx_clk,
	u32 crt_video_clk_out, u32 clk_div_path, u32 venc_clk)
{
	u8 venc_clk_div = 1;
	u8 venc_clk_div_offset = 0;
	/* 0: V1, 1:V2. assume V1 connect to VENC0 and V2 connect to VENC1 */
	u8 vclk_sel = clk_div_path & BIT(0);

	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	if (venc_clk == 0) {
		pr_err("%s venc_clk invalid\n", __func__);
		return -EINVAL;
	}

	venc_clk_div = crt_video_clk_out / venc_clk;
	switch (venc_clk_div) {
	case 2:
		/* extra div 2 */
		venc_clk_div_offset = 1;
		break;
	case 1:
		/* no extra div */
		venc_clk_div_offset = 0;
		break;
	default:
		pr_err("%s crt_video_clk_out(%d)/venc_clk(%d) ratio invalid\n",
			__func__, crt_video_clk_out, venc_clk);
		return -EINVAL;
	}

	if (vclk_sel == 0) {
		/* V1 enc0 sel div1 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_DIV, venc_clk_div_offset, 12, 4);
		/* bit[10] enable enc0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, 1, 10, 1);
	} else if (vclk_sel == 1) {
		/* V2 enc1 sel div1 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VIID_CLK_DIV, 8 + venc_clk_div_offset, 8, 4);
		/* [11] enable enc1 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, 1, 11, 1);
	}

	return 0;
}

/*
 * crt_video_clk_out: clk set crt_video_clk_set_a9()
 */
static int tx_pixel_clk_set_a9(struct meson_tx_clk *tx_clk,
	u32 crt_video_clk_out, u32 clk_div_path, u32 pixel_clk)
{
	/* 0: V1, 1:V2. assume V1 connect to VENC0 and V2 connect to VENC1 */
	u8 vclk_sel = clk_div_path & BIT(0);
	/* 0: HDMI_IF0, 1:HDMI_IF1 */
	u8 hdmi_if_idx = (clk_div_path >> 4) & 0x1;
	u8 pixel_clk_div = 1;
	u8 pixel_clk_div_offset = 0;

	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	if (pixel_clk == 0) {
		pr_err("%s pixel clk invalid\n", __func__);
		return -EINVAL;
	}

	pixel_clk_div = crt_video_clk_out / pixel_clk;
	switch (pixel_clk_div) {
	case 2:
		/* extra div 2 */
		pixel_clk_div_offset = 1;
		break;
	case 1:
		/* no extra div */
		pixel_clk_div_offset = 0;
		break;
	default:
		pr_err("%s crt_video_clk_out(%d)/pixel_clk(%d) ratio invalid\n",
			__func__, crt_video_clk_out, pixel_clk);
		return -EINVAL;
	}

	/* crt_video_clk to hdmi_if */
	if (hdmi_if_idx == 0) {
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG], CLKCTRL_HDMI_CLK_CTRL,
			vclk_sel == 0 ? 0 + pixel_clk_div_offset : 8 + pixel_clk_div_offset, 16, 4);
		/* fe0 sel div0 always */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_HDMI_CLK_CTRL,
			vclk_sel == 0 ? 0 : 8, 20, 4);
		/* bit[5] enable pix0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, 1, 5, 1);
		/* [9] enable fe0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, 1, 9, 1);
	} else if (hdmi_if_idx == 1) {
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG], CLKCTRL_HDMI_CLK_CTRL,
			vclk_sel == 0 ? 0 + pixel_clk_div_offset : 8 + pixel_clk_div_offset, 24, 4);
		/* fe0 sel div0 always */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_HDMI_CLK_CTRL,
			vclk_sel == 0 ? 0 : 8, 28, 4);
		/* bit[5] enable pix0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, 1, 12, 1);
		/* [9] enable fe0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, 1, 13, 1);
	}

	return 0;
}

/* assume venc_idx = clk_divider_idx */
static int tx_venc_clk_gate_a9(struct meson_tx_clk *tx_clk, u8 venc_idx, bool en)
{
	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	if (venc_idx == 0)
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, en, 10, 1);
	else if (venc_idx == 1)
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, en, 11, 1);
	else
		pr_err("%s invalid venc_idx: %d\n", __func__, venc_idx);

	return 0;
}

/* for fe_clk and pixel_clk gate */
static int tx_pixel_clk_gate_a9(struct meson_tx_clk *tx_clk, u8 hdmi_if_idx, bool en)
{
	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	if (hdmi_if_idx == 0) {
		/* bit[5] enable pix0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, en, 5, 1);
		/* bit[9] enable fe0 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, en, 9, 1);
	} else if (hdmi_if_idx == 1) {
		/* bit[12] enable pix1 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, en, 12, 1);
		/* bit[13] enable fe1 */
		tx_clk_set_reg_bits(tx_clk->reg_io_base[CLKCTRL_REG],
			CLKCTRL_VID_CLK_CTRL2, en, 13, 1);
	} else {
		pr_err("%s invalid hdmi_if_idx: %d\n", __func__, hdmi_if_idx);
	}

	return 0;
}

int tx_bulk_clk_set_a9(struct meson_tx_clk *tx_clk, u32 clk_mask)
{
	u32 crt_video_clk_out;
	struct meson_tx_clk_data *clk_cfg = NULL;
	u8 venc_idx = 0;

	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	clk_cfg = &tx_clk->tx_clk_cfg;
	crt_video_clk_out = clk_cfg->venc_clk;
	/* assume video_pll_idx = venc index, that's to say
	 * venc0(V1 crt divider) use pixel_pll0,
	 * venc1(V2 crt divider) use pixel_pll1
	 */
	venc_idx = clk_cfg->clk_div_path & BIT(0);

	if (clk_mask & PIXEL_PLL_MASK)
		tx_pixel_pll_set_a9(tx_clk, clk_cfg->clk_type, venc_idx,
			clk_cfg->pixel_vco_clk, clk_cfg->clk_to_crt_video);

	/* crt_video_clk: set clk source for venc and pixel clk,
	 * here set the crt_video divider to output clk at venc clk rate
	 */
	if (clk_mask & (PIXEL_CLK_MASK | VENC_CLK_MASK))
		tx_crt_video_clk_set_a9(tx_clk, clk_cfg->clk_to_crt_video,
			clk_cfg->clk_div_path & BIT(0), crt_video_clk_out);

	/* clk div1 for VENC */
	if (clk_mask & VENC_CLK_MASK)
		tx_venc_clk_set_a9(tx_clk, crt_video_clk_out,
			clk_cfg->clk_div_path, clk_cfg->venc_clk);

	if (clk_mask & PIXEL_CLK_MASK)
		tx_pixel_clk_set_a9(tx_clk, crt_video_clk_out,
			clk_cfg->clk_div_path, clk_cfg->pixel_clk);

	if (clk_mask & PHY_CLK_MASK)
		tx_phy_clk_set_a9(tx_clk, clk_cfg->clk_type, clk_cfg->phy_clk);

	return 0;
}

int tx_bulk_clk_disable_a9(struct meson_tx_clk *tx_clk, u32 clk_mask)
{
	struct meson_tx_clk_data *clk_cfg = NULL;

	if (!tx_clk) {
		pr_err("%s NULL tx_clk\n", __func__);
		return -EINVAL;
	}

	clk_cfg = &tx_clk->tx_clk_cfg;
	if (clk_mask & PIXEL_PLL_MASK)
		tx_pixel_pll_disable_a9(tx_clk, clk_cfg->clk_type);

	/*
	 * crt_video_clk: clk source for venc and pixel clk,
	 * disable crt_video only if both pixel clk and venc clk are disabled
	 */
	if ((clk_mask & (PIXEL_CLK_MASK | VENC_CLK_MASK)) == (PIXEL_CLK_MASK | VENC_CLK_MASK))
		tx_crt_video_clk_disable_a9(tx_clk, clk_cfg->clk_div_path & BIT(0));

	if (clk_mask & VENC_CLK_MASK)
		tx_venc_clk_gate_a9(tx_clk, clk_cfg->clk_div_path & BIT(0), false);

	if (clk_mask & PIXEL_CLK_MASK)
		tx_pixel_clk_gate_a9(tx_clk, (clk_cfg->clk_div_path >> 4) & 0x1, false);

	if (clk_mask & PHY_CLK_MASK)
		tx_phy_clk_disable_a9(tx_clk, clk_cfg->clk_type);

	return 0;
}

