/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __HDMITX_HW_PLATFORM_H
#define __HDMITX_HW_PLATFORM_H

#include <linux/version.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/mm.h>
#include <linux/major.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/cdev.h>
#include <linux/io.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/miscdevice.h>

#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vpu/vpu.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include <drm/amlogic/meson_connector_dev.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_hw_common.h>
#include <linux/amlogic/media/vout/hdmi_tx_repeater.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_types.h>
#ifdef CONFIG_AMLOGIC_DSC
#include <linux/amlogic/media/vout/dsc.h>
#endif
#include "hdmi_tx_connector/hdmitx_log.h"
#include "hdmitx_core_reg.h"
#include "hdmitx_ddc.h"
#include "hdmitx_platform_reg.h"

struct hdmitx21_hw {
	struct hdmitx_hw_common *base;
	u32 enc_idx;
	struct hdmitx_infoframe *infoframe;
	/*
	 * For use with s7 and later chips, default 0
	 * 1: new clk config, encp/pixel clk is directly configured by the pll simulation part.
	 * through [ 49]hdmi_vx1_pix_clk to encp/pixel clk
	 * CLKCTRL_VID_CLK0_CTRL clk source should select vid_pix_clk.
	 */
	u8 clk_analog_path;
	int gate_bit_mask; /* for ctrl phy/pll/clk */
};

struct hdmitx21_dev {
	struct hdmitx_common tx_comm;
	struct hdmitx_hw_common hw_comm;

	struct hdmitx21_hw tx21_hw;
	/* dedicated for intr */
	struct workqueue_struct *hdmi_intr_wq;

	u8 manual_frl_rate; /* for manual setting */

	/* ignore fifo intr5 if hdmitx output disabled */
	bool ignore_fifo_intr5;
#ifdef CONFIG_AMLOGIC_DSC
	/* pps data and clk info from dsc module */
	struct dsc_offer_tx_data dsc_data;
#endif
	/* 0: RGB444  1: Y444  2: Y422  3: Y420 */
	/* 4: 24bit  5: 30bit  6: 36bit  7: 48bit */
	/* if equals to 1, means current video & audio output are blank */

	/*
	 * debug only, should be positive value. if it is N, then vysnc_handler
	 * will handle N frames, then it will be 0, and vysnc_handler is pending
	 * value 1 is only for single steps, and -1 will work as normally.
	 */
	int vrr_dbg_vframe;

	int dfm_type; /* for dfm debug */
	/*
	 * for choose VPU_HDMI_if function
	 * 1: yuv2rgb (default)
	 * 2: rgb2yuv
	 */
	int csc_type;
	/* for dsc debug */
	int emp_no;
	int emp_verbose;
};

enum map_addr_idx_e {
	VPUCTRL_REG_IDX,
	HDMITX_COR_REG_IDX,
	HDMITX_TOP_REG_IDX,
	SYSCTRL_REG_IDX,
	PWRCTRL_REG_IDX,
	ANACTRL_REG_IDX,
	RESETCTRL_REG_IDX,
	CLKCTRL_REG_IDX,
	PADCTRL_REG_IDX,
	REG_IDX_END
};

struct reg_map {
	u32 phy_addr;
	u32 size;
	void __iomem *p;
};

struct reg_s {
	u32 reg;
	u32 val;
};

#define VID_PLL_DIV_1 0
#define VID_PLL_DIV_2      1
#define VID_PLL_DIV_3      2
#define VID_PLL_DIV_3p5    3
#define VID_PLL_DIV_3p75   4
#define VID_PLL_DIV_4      5
#define VID_PLL_DIV_5      6
#define VID_PLL_DIV_6      7
#define VID_PLL_DIV_6p25   8
#define VID_PLL_DIV_7      9
#define VID_PLL_DIV_7p5    10
#define VID_PLL_DIV_12     11
#define VID_PLL_DIV_14     12
#define VID_PLL_DIV_15     13
#define VID_PLL_DIV_2p5    14
#define VID_PLL_DIV_3p25   15
#define VID_PLL_DIV_10     16

#define GROUP_MAX	10
struct hw_enc_clk_val_group {
	enum hdmi_vic group[GROUP_MAX];
	u32 hpll_clk_out; /* Unit: kHz */
	u32 od1;
	u32 od2; /* HDMI_CLK_TODIG */
	u32 od3;
	u32 vid_pll_div;
	u32 vid_clk_div;
	u32 enc_div;
	u32 fe_div;
	u32 pnx_div;
	u32 pixel_div;
};

enum viu_Type {
	VIU_ENCL = 0, VIU_ENCI, VIU_ENCP, VIU_ENCT,
};

struct enc_clk_val {
	enum vmode_e mode;
	u32 hpll_clk_out;
	u32 hpll_hdmi_od;
	u32 hpll_lvds_od;
	u32 viu_path;
	enum viu_Type viu_type;
	u32 vid_pll_div;
	u32 clk_final_div;
	u32 hdmi_tx_pixel_div;
	u32 encp_div;
	u32 enci_div;
	u32 enct_div;
	u32 encl_div;
	u32 vdac0_div;
	u32 vdac1_div;
	u32 unused; /* prevent compile error */
};

struct hdmitx21_hw *get_hdmitx21_hw_instance(void);
struct hdmitx21_dev *get_hdmitx21_device(void);

#define to_hdmitx21_hw(x)	container_of(x, struct hdmitx21_hw, base)

int likely_frac_rate_mode(const char *m);

/* 1. reg map */
extern struct reg_map reg21_maps[REG_IDX_END];
u32 TO21_PHY_ADDR(u32 addr);
void __iomem *TO21_PMAP_ADDR(u32 addr);

/* 2. platform reg operation */
u32 hd21_read_reg(u32 addr);
void hd21_write_reg(u32 addr, u32 val);
void hd21_set_reg_bits(u32 addr, u32 value,	u32 offset, u32 len);

/* 3. core reg operation */
/* sequence read registers */
void hdmitx21_seq_rd_reg(u16 offset, u8 *buf, u16 cnt);
void hdmitx21_fifo_read(u16 offset, u8 *buf, u16 cnt);
u32 hdmitx21_rd_reg(u32 addr);
void hdmitx21_wr_reg(u32 addr, u32 val);
void hdmitx21_poll_reg(u32 addr, u8 exp_data, u8 mask, ulong timeout);
/* set the value in length from offset */
void hdmitx21_set_reg_bits(u32 addr, u32 value, u32 offset, u32 len);
/* set the bits value */
void hdmitx21_set_bit(u32 addr, u32 bit_val, bool st);
void hdmitx21_set_reset_reg_bit(u32 addr, u32 bit_nr);
void hdmitx21_n_reset_reg_bit(u32 addr, u32 bit_nr);

/* 4. venc/vpu */
void hdmitx_set_tv_encp_new(struct hdmitx21_dev *hdev, u32 enc_index,
	enum hdmi_vic vic, u32 enable);
void hdmitx_set_tv_enci_new(struct hdmitx21_dev *hdev, u32 enc_index,
	enum hdmi_vic vic, u32 enable);
void hdmitx21_venc_en(bool en, bool pi_mode);
void hdmitx21_dither_config(struct hdmitx21_dev *hdev);
void hdmitx21_color_convert(struct hdmitx21_dev *hdev, u32 output_color_format);
void hdmitx21_csc_update_avi_infoframe(struct hdmitx21_dev *hdev, u32 output_color_format);

/* 5. hdmi phy */
void hdmitx21_phy_band_gap_en_t7(void);
void hdmitx21_phy_band_gap_en_s5(void);
void hdmitx21_phy_band_gap_en_s7(void);
void hdmitx21_phy_band_gap_en_s7d(void);

void hdmitx_set_phy_by_mode_t7(u32 mode);
void hdmitx_set_phy_by_mode_s5(u32 mode);
void hdmitx_set_s5_phypara(enum frl_rate_enum frl_rate, u32 tmds_clk);
void hdmitx_set_phy_by_mode_s1a(u32 mode);
void hdmitx_set_phy_by_mode_s7(u32 mode);
void hdmitx_set_phy_by_mode_s7d(u32 mode);
void hdmitx_set_phy_by_mode_s6(u32 mode);
void hdmitx_set_phy_by_mode_a9(u32 mode);

void hdmitx_s5_phy_pre_init(struct hdmitx21_dev *hdev);
void hdmitx_s7_phy_pre_init(struct hdmitx21_dev *hdev);
void hdmitx_s7d_phy_pre_init(struct hdmitx21_dev *hdev);

/* 6. platform clk */
/* 6.1 basic clk */
void hdmitx21_set_default_clk(void);
void hdmitx21_set_clk(struct hdmitx21_dev *hdev);
void hdmitx21_set_cts_sys_clk(struct hdmitx21_dev *hdev);
void hdmitx21_set_top_pclk(struct hdmitx21_dev *hdev);
void hdmitx21_set_hdcp_pclk(struct hdmitx21_dev *hdev);
void hdmitx21_set_cts_hdcp22_clk(struct hdmitx21_dev *hdev);
bool hdmitx21_is_basic_clk_en(struct hdmitx21_dev *hdev);

/* 6.2 sys clk */
void hdmitx21_sys_reset_t7(void);
void hdmitx21_sys_reset_s5(void);
void hdmitx21_sys_reset_s6(void);
void hdmitx21_sys_reset_s7(void);
void hdmitx21_sys_reset_s7d(void);
void hdmitx21_sys_reset_a9(void);

/* 6.3 special clk */
void hdmitx_set_s5_tmds_clk_div(struct hdmitx21_dev *hdev);
void hdmitx_s5_phy_keep_clk_todig(bool en);
/* enc/pixel clk, not used */
void hdmitx21_disable_clk(struct hdmitx21_dev *hdev);

/* 6.4 clk divider */
void hdmitx_set_clkdiv(struct hdmitx21_dev *hdev);
void hdmitx_s1a_reset_div_clk(struct hdmitx21_dev *hdev);
void hdmitx21_s5_clk_div_rst(u32 clk_idx);

/* 7. platform PLL */
/* 7.1 disable plls */
void hdmitx_disable_s5_plls(struct hdmitx21_dev *hdev);
void hdmitx_disable_s7_plls(struct hdmitx21_dev *hdev);
void hdmitx_disable_s7d_plls(struct hdmitx21_dev *hdev);
void hdmitx_disable_s6_plls(struct hdmitx21_dev *hdev);
void hdmitx_disable_a9_plls(struct hdmitx21_dev *hdev);

/* 7.2 fpll */
void hdmitx_set_frl_hpll_od(enum frl_rate_enum rate);
void hdmitx_set_fpll(struct hdmitx21_dev *hdev);
void hdmitx_set_s5_fpll(u32 clk, u32 div, u32 pixel_od);
/* 7.3 gpll */
void hdmitx_set_s5_gp2pll(u32 clk, u32 div);
void hdmitx_set_gp2pll(struct hdmitx21_dev *hdev);
void hdmitx_mux_gp2pll(struct hdmitx21_dev *hdev);

/* 7.4 hpll */
void hdmitx_set_t7_hpll_clk_out(u32 frac_rate, u32 clk);
void hdmitx_set_s5_htxpll_clk_out(u32 clk, u32 div);
void hdmitx_set_s1a_hpll_clk_out(u32 frac_rate, u32 clk);
void hdmitx_set_s7_htxpll_clk_out(u32 clk, u32 div);
void hdmitx_set_s7d_htxpll_clk_out(u32 clk, u32 div);
void hdmitx_set_s6_htxpll_clk_out(u32 clk, u32 div);
void hdmitx_set_a9_htxpll_clk_out(u32 clk, u32 div);
void hdmitx_set_hpll_od0_t7(u32 div);
void hdmitx_set_hpll_od1_t7(u32 div);
void hdmitx_set_hpll_od2_t7(u32 div);
void hdmitx_set_hpll_od3_t7(u32 div);

/* 7.5 hpll sspll */
void hdmitx_set_hpll_sspll_t7(enum hdmi_vic vic);
void hdmitx_set_hpll_sspll_s5(enum hdmi_vic vic);
void hdmitx_set_hpll_sspll_s7(enum hdmi_vic vic);
void hdmitx_set_hpll_sspll_s7d(enum hdmi_vic vic);
void hdmitx_set_hpll_sspll_s6(enum hdmi_vic vic);
void hdmitx_set_hpll_sspll_a9(enum hdmi_vic vic);

/* 7.6 clk gate for suspend power */
void hdmitx_t7_clock_gate_ctrl(bool en);
void hdmitx_s5_clock_gate_ctrl(struct hdmitx21_dev *hdev, bool en);
void hdmitx_s1a_clock_gate_ctrl(bool en);
void hdmitx_s7_clock_gate_ctrl(struct hdmitx21_dev *hdev, bool en);
void hdmitx_s7d_clock_gate_ctrl(struct hdmitx21_dev *hdev, bool en);
void hdmitx_s6_clock_gate_ctrl(struct hdmitx21_dev *hdev, bool en);
void hdmitx_a9_clock_gate_ctrl(struct hdmitx21_dev *hdev, bool en);
void hdmitx21_clks_gate_ctrl(bool en);

/* 8. hpd */
int hdmitx21_hpd_hw_op(enum hpd_op cmd);
void hdmitx_hpd_irq_top_half_process(struct hdmitx21_dev *hdev, bool hpd);
#endif
