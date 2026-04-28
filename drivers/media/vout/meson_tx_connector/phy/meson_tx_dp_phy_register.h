/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __MESON_TX_DP_PHY_REGISTER_H__
#define __MESON_TX_DP_PHY_REGISTER_H__

/*
 * [30:29]  u3p2pll0_lock_lkw
 * [28]     u3p2pll0_lock_f
 * [27]     u3p2pll0_lock_en
 * [26]     25m_rstn
 * [25:17]  u3p2pll0_int_num
 * [16]     u3p2pll0_free_run_en
 * [15:14]  u3p2pll0_free_run
 * [13:12]  u3p2pll0_dt
 * [11:9]   u3p2pll0_cp_iea
 * [8:5]    u3p2pll0_cp_icp
 * [3]      u3p2pll0_clk_output_select
 * [2]      u3p2pll0_bias_en
 * [1:0]    u3p2pll0_bias_cp_adj
 */
#define DP_RG_USBCDP_PLL0_CNTL0                   (0x00)

/*
 * [27]u3p2pll0 Rstn
 * [26:24]  u3p2pll0_reve
 * [23:21]  u3p2pll0_ref_clk_od
 * [20:19]  u3p2pll0_25m_adj
 * [18:13]  u3p2pll0_output_od2
 * [12:7]   u3p2pll0_output_od
 * [6:5]    u3p2pll0_output_gate
 * [4:2]    u3p2pll0_lpf_res_adj
 * [1:0]    u3p2pll0_lock_long
 */
#define DP_RG_USBCDP_PLL0_CNTL1                   (0x04)

/*
 * [31:29]  u3p2pll1_ib_adj
 * [28:26]  u3p2pll1_fref_sel
 * [25:7]   u3p2pll1_frac
 * [6]      u3p2pll1_fb_div2_enb
 * [5]      u3p2pll1_dpll_load
 * [4]      u3p2pll1_div_0p5_en
 * [3]      u3p2pll1_clk_test_en
 * [2]      u3p2pll1_bias_en
 * [1:0]    u3p2pll1_bias_cp_adj
 */
#define DP_RG_USBCDP_PLL1_0_CNTL0                 (0x08)

/*
 * [30:29]  u3p2pll1_ref_adj
 * [28:27]  u3p2pll1_pfd_dead_time
 * [26:25]  u3p2pll1_os_ssc
 * [24:22]  u3p2pll1_lpf_r2
 * [21]     u3p2pll1_lpf_fr_en
 * [20]     u3p2pll1_lpf_fr_adj
 * [19]     u3p2pll1_lock_start
 * [18:17]  u3p2pll1_lock_long
 * [16]     u3p2pll1_lock_f
 * [15]     u3p2pll1_load_en
 * [14:13]  u3p2pll1_lkwide_adj
 * [12:4]   u3p2pll1_inte_num
 * [3:0]    u3p2pll1_icp_adj
 */
#define DP_RG_USBCDP_PLL1_0_CNTL1                 (0x0c)

/*
 * [29]     u3p2pll1_ssg_en
 * [28:25]  u3p2pll1_ssc_str_m
 * [24:21]  u3p2pll1_ssc_dep_sel
 * [20:19]  u3p2pll1_ss_mode
 * [18:17]  u3p2pll1_shift_v
 * [16]     u3p2pll1_shift_en
 * [15]     u3p2pll1_sdm_en
 * [14]     u3p2pll1_rstn_lock
 * [13]     u3p2pll1_rstn
 * [12:5]   u3p2pll1_reve
 * [4:3]    u3p2pll1_ref_sel
 * [2:0]    u3p2pll1_ref_clk_od
 */
#define DP_RG_USBCDP_PLL1_0_CNTL2                 (0x10)

/*
 * same definition as DP_RG_USBCDP_PLL1_0_* registers
 */
#define DP_RG_USBCDP_PLL1_1_CNTL0                 (0x14)
#define DP_RG_USBCDP_PLL1_1_CNTL1                 (0x18)
#define DP_RG_USBCDP_PLL1_1_CNTL2                 (0x1c)

/* SOC debug only, not for SW */
#define DP_PLL_CFG0                               (0x20)
#define DP_REG_DCHA_AFE                           (0x24)
#define DP_REG_DCHA_DFE                           (0x28)
#define DP_REG_DCHD_CDR                           (0x2c)
#define DP_REG_DCHD_EQ                            (0x30)
#define DP_REG_ENBG                               (0x34)
#define DP_REG_MISC                               (0x38)

/*
 * SOC debug only, not for SW
 * dp_idle_rx_cfg0
 *     [31]    dp_upcrx_vcm_en
 *     [30]    dp_upcrx_rterm_en
 *     [29]    dp_upcrx_pi_clk_en
 *     [28]    dp_upcrx_ofst_start_dig
 *     [27]    dp_upcrx_ofst_start_ana
 *     [26]    dp_upcrx_ofst_rstb
 *     [25]    dp_upcrx_mux_rstb
 *     [24]    dp_upcrx_lpbk_eq_en
 *     [23]    dp_upcrx_lpbk_des_en
 *     [22]    dp_upcrx_lfps_lpf_bypass
 *     [21]    dp_upcrx_lfps_en
 *     [20]    dp_upcrx_ldo_en
 *     [19]    dp_upcrx_igen_en
 *     [18]    dp_upcrx_eyem_en
 *     [17]    dp_upcrx_eq_rstb
 *     [16:15] dp_upcrx_eq_mode
 *     [14]    dp_upcrx_eq_dcoc_en
 *     [13:9]  dp_upcrx_eq_byp_val
 *     [8]     dp_upcrx_eq_byp_en
 *     [7]     dp_upcrx_dfe_rstb
 *     [6]     dp_upcrx_dfe_hold
 *     [5]     dp_upcrx_cdr_lkdet_en
 *     [4]     dp_upcrx_dch_rstn
 *     [3]     dp_upcrx_dch_en
 *     [2]     dp_upcrx_cdr_rstb
 *     [1]     dp_upcrx_cdr_hold
 *     [0]     dp_upcrx_cdr_fr_en
 */
#define DP_IDLE_RX_CFG0                           (0x3c)

/* SOC debug only, not for SW */
#define DP_IDLE_RX_CFG1                           (0x40)

/*
 * SOC debug only, not for SW
 * [31:16] u3dp_idle_cfg1
 * [15:0]  u3dp_idle_cfg2
 */
#define U3DP_IDLE_CFG0                            (0x44)

/*
 * SOC debug only, not for SW
 * [14]   u3dp_idle_lfps_mask
 * [13]   u3dp_idle_rxdeten
 * [12]   u3dp_idle_idle_en
 * [11]   u3dp_idle_lfps_sel
 * [10:0] u3dp_idle_efuse
 */
#define U3DP_IDLE_CFG1                            (0x48)

/*
 * usbcdptx_lane0_cfg2/upctx_cfg2:
 *     [22] upctx_boost_en: TX boost bias enable
 *     [21] upctx_sel_drv_post: FFE post-EQ de-emphasis level ctrl 0=3.5dB 1=6dB
 *     [20] upctx_low_amp_en: TX output swing amplitude contrl
 *     [19] upctx_sel_pdr_post: Enable De-emph, 0 for no de-emph, 1 for de-emph
 *     [18] upctx_preldo_en:ldo for clk path enable
 *     [17] upctx_fast_ldo_en: pdr_ldo vref voltage fast start up
 *     [16] upctx_ibas_en: TX internal isource enable
 * usbcdptx_lane0_cfg1/upctx_cfg1:
 *     [15] upctx_div_en: TX clk DIV enable: 1 for 2.5G/2.7G/1.62G, 0 for 5G/3.125G/5.4G
 *     [14] upctx_div4_en: TX clk DIV 4enable: 1 for 1.25G, 0 for 3.125G
 *     [13] upctx_sel_10bit_mode: 20bit/10bit mode sel, 1:10 bit mode, 0:20bit mode
 *     [12] upctx_clk_en: clk enable, clock reset signal 1=work 0=reset need reset
 *     [11] upctx_p2s_en: p2s enable, p2s reset signal 1=work 0=reset need reset
 *     [10] upctx_ffe_en: FFE enable 1=enable 0=disable need reset
 *     [9] upctx_loopback_en: for internal loop back enable signal
 *     [8] upctx_rxdet_ref_sel: Detec comparator ref voltage sel 0=vdd*5/6 1=vdd*8/9
 *     [7:6] upctx_idle_voltage_sel<1:0>: Detection common voltage output
 *                                        00:425mV, 01:450mV, 10:475mV, 11:500mV
 *     [5] upctx_sel_dc_mode: TX dc output enable
 *     [4] upctx_post_vg_ctrl: De-emph varition gain ctrl,
 *                             0: no varition gain,1: varition gain with impedance trim
 *     [3:2] upctx_dcout_main: TX dc mode driver ctrl:
 *                             00:output High Z, 01: pull up;10:pull down, 11:output common voltage
 *     [1:0] upctx_dcout_post: TX dc mode driver ctrl:
 *                             00:output High Z, 01: pull up;10:pull down, 11:output common voltage
 */
#define USBCDPTX_LANE0_CFG0                       (0x4c)

/*
 * 0x048 u3dp_idle_cfg1
 *       [14] u3dp_idle_lfps_mask/upctx_lfps_mask：LFPS input signal
 *       [13] u3dp_idle_rxdeten/upctx_rxdeten：Receiver Detection start signal
 *       [12] u3dp_idle_idle_en/upctx_idle_en：TX idle state enable, 1=enable 0=disable
 *       [11] u3dp_idle_lfps_sel/upctx_lfps_sel：LFPS work mode enable, 1 for LFPS
 *       [10:0] u3dp_idle_efuse/upctx_efuse<10:0>
 *              <10:8>upctx_pdio_main: TX Driver pull down impedance ctrl, need FT trim
 *              <7:5>upctx_puio_main: TX Driver pull up impedance ctrl, need FT trim
 *              <4:0>upctx_boost_ctrl: TX boost current control.
 *                                     [2:0]:1.6mA~2.3mA, [3]:double，[4]:half
 */
#define USBCDPTX_LANE0_CFG1                       (0x50)

/*
 * same definition, refer to USBCDPTX_LANE0_CFG0/USBCDPTX_LANE0_CFG1
 */
#define USBCDPTX_LANE1_CFG0                       (0x54)
#define USBCDPTX_LANE1_CFG1                       (0x58)
#define USBCDPTX_LANE2_CFG0                       (0x5c)
#define USBCDPTX_LANE2_CFG1                       (0x60)
#define USBCDPTX_LANE3_CFG0                       (0x64)
#define USBCDPTX_LANE3_CFG1                       (0x68)

/*
 * 0x06c usbcdp_pll_reg0
 *       [3] RO   dp_phy_rdy
 *       [2] RO   usbcdp_pll1_1_pll_lock_flag
 *       [1] RO   usbcdp_pll1_0_pll_lock_flag
 *       [0] RO   usbcdp_pll0_pll_done
 */
#define USBCDP_PLL_REG0                           (0x6c)

/*
 * [15] soft_reset: 1=reset
 * [12] data mux: 1=prbs_data, 0=dptx data
 * [7:6] clk mux: 0=lane0_clk, 1=lane1_clk, 2=lane2_clk, 3=lane3_clk
 * [4:2] prbs mode: 1=PRBS7, 2=PRBS11, 3=PRBS15, 5=PRBS20, 6=PRBS23, 7=PRBS31
 * [1] inj_bist_err: 1=inject error
 * [0] prbs_en: 1=prbs_enable
 */
#define DP_PRBS_TEST_CFG0                         (0x74)

/*
 * [14:12] ana1 mode: 1=PRBS7, 2=PRBS11, 3=PRBS15, 5=PRBS20, 6=PRBS23, 7=PRBS31
 * [9] Restart prbs ana1
 * [8] enable prbs ana1
 * [6:4] ana0 mode: 1=PRBS7, 2=PRBS11, 3=PRBS15, 5=PRBS20, 6=PRBS23, 7=PRBS31
 * [1] Restart prbs ana0
 * [0] enable prbs ana0
 */
#define DP_PRBS_TEST_CFG1                         (0x78)

/*
 * [31:16]      bypass_lane0_cfg2
 * [15:0]       bypass_lane0_cfg1
 */
#define BYPASS_LANE0_CFG0                         (0x80)

/*
 * SOC debug only, not for SW
 * [31:15]   bypass_lane0_reserved0
 * [14]      bypass_lane0_lfps_mask
 * [13]      bypass_lane0_rxdeten
 * [12]      bypass_lane0_idle_en
 * [11]      bypass_lane0_lfps_sel
 * [10:0]    bypass_lane0_efuse
 */
#define BYPASS_LANE0_CFG1                         (0x84)
/*
 * SOC debug only, not for SW
 * same definition, refer to BYPASS_LANE0_CFG0/BYPASS_LANE0_CFG1
 */
#define BYPASS_LANE1_CFG0                         (0x88)
#define BYPASS_LANE1_CFG1                         (0x8c)
#define BYPASS_LANE2_CFG0                         (0x90)
#define BYPASS_LANE2_CFG1                         (0x94)
#define BYPASS_LANE3_CFG0                         (0x98)
#define BYPASS_LANE3_CFG1                         (0x9c)

/*
 * SOC debug only, not for SW
 * [31:0]      data_mux_cfg0
 */
#define DATA_MUX_CFG0                             (0xa0)

/*
 * SOC debug only, not for SW
 * [31:0]     data_mux_cfg1
 */
#define DATA_MUX_CFG1                             (0xa4)

/*
 * [31]     dpaux_dout_ro
 * [19:16]  dp_rg_aux_prod
 * [15:0]   dp_rg_aux_cntl
 * [14:12]  aux_vref_sel<2:0>  vref for vcm buffer select: x00 ~ x11: 0.35/0.8/1.0/1.2
 * [11:10]  aux_vcm_mode<1:0>
 *              rx vcm mode select <1>:  0: p-rterm, 1: n-rterm
 *                                 <0>: 0: vref/2, 1: vref
 * [9:7]    aux_tx_trim<2:0>    tx swing trim bits from efuse
 * [6:5]    aux_rxhys_sel<1:0>  00/01/10/10: hys = 0 / 0 / 50mV / 100mV
 * [4:3]    aux_rx_mode<1:0>    00: differential rx input, padp - padn
 *                              01: single ended rx input, padp - vcm
 *                              10: single ended rx input, vcm - padn
 *                              11: vcm - vcm
 * [2]      aux_en_txswap    1: tx data polarity reverse; 0: padp = din, padn = ~din
 * [1]      aux_en_rxswap    1: rx data polarity reverse; 0: dout = padp - padn
 * [0]      aux_en  enable aux
 */
#define DP_RG_AUX                                 (0xa8)

/*
 * [25:22]     stat_prbs_ana
 * [21:0]      stat_prbs_ana0_ber
 */
#define PRBS_REG_RO0                              (0xac)

/*
 * [21:0] stat_prbs_ana1_ber
 */
#define PRBS_REG_RO1                              (0xb0)

/*
 * [21:9] usbcdprx_lane1_state
 * [8]    usbcdprx_lane1_sqh_out
 * [7]    usbcdprx_lane1_lfps_out
 * [6]    usbcdprx_lane1_eyem_done
 * [5:1]  usbcdprx_lane1_dbg_eq
 * [0]    usbcdprx_lane1_cdrlock
 */
#define USBCDPRX_LANE1_RO0                        (0xb4)

/*
 * [21:9] usbcdprx_lane2_state
 * [8]    usbcdprx_lane2_sqh_out
 * [7]    usbcdprx_lane2_lfps_out
 * [6]    usbcdprx_lane2_eyem_done
 * [5:1]  usbcdprx_lane2_dbg_eq
 * [0]    usbcdprx_lane2_cdrlock
 */
#define USBCDPRX_LANE2_RO0                        (0xb8)

// Below register will control by USB driver
/*
 * ANACTRL_USBCDP_CTRL, 0xFE0086DC
 * [8:7] u3dp_sel
 *       2'b00 idle
 *       2'b01 usb3
 *       2'b10 dp 4lanes
 *       2'b11 usb_dp
 * [6]   flipped_en  0: normal plug  1: flip plug
 * [5:0] usbcdp_alt_mux
 * Reference configuration:
 * ---------------------------------------------------------------------------
 *     cases          | u3dp_sel | flipped_en | usbcdp_alt_mux | USBCDP_CTRL
 * ---------------------------------------------------------------------------
 *   nothing attached |     0    |     0      |   6'b000000    |    0x00
 *   usb only(flip=0) |     1    |     0      |   6'b100000    |    0xA0
 *   usb only(flip=1) |     1    |     1      |   6'b010000    |    0xD0
 *   usb+dp(flip=0)   |     3    |     0      |   6'b100000    |    0x1A0
 *   usb+dp(flip=1)   |     3    |     1      |   6'b010000    |    0x1D0
 *   dp only 4Lane    |     2    |     0      |   6'b111000    |    0x138
 *----------------------------------------------------------------------------
 */
#define ANACTRL_USBCDP_CTRL                       (0x01b7  << 2)

#endif
