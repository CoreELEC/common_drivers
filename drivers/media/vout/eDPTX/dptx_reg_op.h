/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_REG_H__
#define __DPTX_REG_H__

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>

#ifndef EDPTX0_A_BASE
#define EDPTX0_A_BASE          EDPTX0_BASE
#endif

#ifndef EDPTX0_B_BASE
#define EDPTX0_B_BASE          0
#endif

#ifndef EDPTX1_A_BASE
#define EDPTX1_A_BASE          EDPTX1_BASE
#endif

#ifndef EDPTX1_B_BASE
#define EDPTX1_B_BASE          0
#endif
/* register offset address define */
/* base & offset */

//#define LCD_REG_OFFSET(reg)                   (((reg) << 2))
//#define LCD_REG_OFFSET_BYTE(reg)              ((reg))

/* PERIPHS: 0xc8834400 */
#define PREG_PAD_GPIO1_EN_N                        0x0f
#define PREG_PAD_GPIO1_O                           0x10
#define PREG_PAD_GPIO1_I                           0x11
#define PREG_PAD_GPIO2_EN_N                        0x12
#define PREG_PAD_GPIO2_O                           0x13
#define PREG_PAD_GPIO2_I                           0x14
#define PREG_PAD_GPIO3_EN_N                        0x15
#define PREG_PAD_GPIO3_O                           0x16
#define PREG_PAD_GPIO3_I                           0x17
#define PREG_PAD_GPIO4_EN_N                        0x18
#define PREG_PAD_GPIO4_O                           0x19
#define PREG_PAD_GPIO4_I                           0x1a
#define PREG_PAD_GPIO5_EN_N                        0x1b
#define PREG_PAD_GPIO5_O                           0x1c
#define PREG_PAD_GPIO5_I                           0x1d

#define PERIPHS_PIN_MUX_0                          0x2c
#define PERIPHS_PIN_MUX_1                          0x2d
#define PERIPHS_PIN_MUX_2                          0x2e
#define PERIPHS_PIN_MUX_3                          0x2f
#define PERIPHS_PIN_MUX_4                          0x30
#define PERIPHS_PIN_MUX_5                          0x31
#define PERIPHS_PIN_MUX_6                          0x32
#define PERIPHS_PIN_MUX_7                          0x33
#define PERIPHS_PIN_MUX_8                          0x34
#define PERIPHS_PIN_MUX_9                          0x35
#define PERIPHS_PIN_MUX_10                         0x36
#define PERIPHS_PIN_MUX_11                         0x37
#define PERIPHS_PIN_MUX_12                         0x38

#define PERIPHS_PIN_MUX_0_TL1                      0x0b0
#define PERIPHS_PIN_MUX_1_TL1                      0x0b1
#define PERIPHS_PIN_MUX_2_TL1                      0x0b2
#define PERIPHS_PIN_MUX_3_TL1                      0x0b3
#define PERIPHS_PIN_MUX_4_TL1                      0x0b4
#define PERIPHS_PIN_MUX_5_TL1                      0x0b5
#define PERIPHS_PIN_MUX_6_TL1                      0x0b6
#define PERIPHS_PIN_MUX_7_TL1                      0x0b7
#define PERIPHS_PIN_MUX_8_TL1                      0x0b8
#define PERIPHS_PIN_MUX_9_TL1                      0x0b9
#define PERIPHS_PIN_MUX_A_TL1                      0x0ba
#define PERIPHS_PIN_MUX_B_TL1                      0x0bb
#define PERIPHS_PIN_MUX_C_TL1                      0x0bc
#define PERIPHS_PIN_MUX_D_TL1                      0x0bd
#define PERIPHS_PIN_MUX_E_TL1                      0x0be
#define PERIPHS_PIN_MUX_F_TL1                      0x0bf

#define PADCTRL_PIN_MUX_REG0                       0x0000
#define PADCTRL_PIN_MUX_REG1                       0x0001
#define PADCTRL_PIN_MUX_REG2                       0x0002
#define PADCTRL_PIN_MUX_REG3                       0x0003
#define PADCTRL_PIN_MUX_REG4                       0x0004
#define PADCTRL_PIN_MUX_REG5                       0x0005
#define PADCTRL_PIN_MUX_REG6                       0x0006
#define PADCTRL_PIN_MUX_REG7                       0x0007
#define PADCTRL_PIN_MUX_REG8                       0x0008
#define PADCTRL_PIN_MUX_REG9                       0x0009
#define PADCTRL_PIN_MUX_REGA                       0x000a
#define PADCTRL_PIN_MUX_REGB                       0x000b
#define PADCTRL_PIN_MUX_REGC                       0x000c
#define PADCTRL_PIN_MUX_REGD                       0x000d
#define PADCTRL_PIN_MUX_REGE                       0x000e
#define PADCTRL_PIN_MUX_REGF                       0x000f
#define PADCTRL_PIN_MUX_REGG                       0x0010
#define PADCTRL_PIN_MUX_REGH                       0x0011
#define PADCTRL_PIN_MUX_REGI                       0x0012
#define PADCTRL_PIN_MUX_REGJ                       0x0013
#define PADCTRL_PIN_MUX_REGK                       0x0014
#define PADCTRL_PIN_MUX_REGL                       0x0015
#define PADCTRL_PIN_MUX_REGM                       0x0016
#define PADCTRL_PIN_MUX_REGN                       0x0017
#define PADCTRL_PIN_MUX_REGO                       0x0018

/* HIU:  HHI_CBUS_BASE = 0x10 */
#define HHI_VIID_PLL_CNTL4                         0x46
#define HHI_VIID_PLL_CNTL                          0x47
#define HHI_VIID_PLL_CNTL2                         0x48
#define HHI_VIID_PLL_CNTL3                         0x49
#define HHI_VIID_CLK_DIV                           0x4a
      #define DAC0_CLK_SEL           28
      #define DAC1_CLK_SEL           24
      #define DAC2_CLK_SEL           20
      #define VCLK2_XD_RST           17
      #define VCLK2_XD_EN            16
      #define ENCL_CLK_SEL           12
      #define VCLK2_XD                0
#define HHI_VIID_CLK_CNTL                          0x4b
      #define VCLK2_EN               19
      #define VCLK2_CLK_IN_SEL       16
      #define VCLK2_SOFT_RST         15
      #define VCLK2_DIV12_EN          4
      #define VCLK2_DIV6_EN           3
      #define VCLK2_DIV4_EN           2
      #define VCLK2_DIV2_EN           1
      #define VCLK2_DIV1_EN           0
#define HHI_VIID_DIVIDER_CNTL                      0x4c
      #define DIV_CLK_IN_EN          16
      #define DIV_CLK_SEL            15
      #define DIV_POST_TCNT          12
      #define DIV_LVDS_CLK_EN        11
      #define DIV_LVDS_DIV2          10
      #define DIV_POST_SEL            8
      #define DIV_POST_SOFT_RST       7
      #define DIV_PRE_SEL             4
      #define DIV_PRE_SOFT_RST        3
      #define DIV_POST_RST            1
      #define DIV_PRE_RST             0
#define HHI_VID_CLK_DIV                            0x59
      #define ENCI_CLK_SEL           28
      #define ENCP_CLK_SEL           24
      #define ENCT_CLK_SEL           20
      #define VCLK_XD_RST            17
      #define VCLK_XD_EN             16
      #define ENCL_CLK_SEL           12
      #define VCLK_XD1                8
      #define VCLK_XD0                0
#define HHI_VID_CLK_CNTL                           0x5f
#define HHI_VID_CLK_CNTL2                          0x65
#define HHI_VID_CLK_CNTL2_T5W                      0xa4
      #define HDMI_TX_PIXEL_GATE_VCLK  5
      #define VDAC_GATE_VCLK           4
      #define ENCL_GATE_VCLK           3
      #define ENCP_GATE_VCLK           2
      #define ENCT_GATE_VCLK           1
      #define ENCI_GATE_VCLK           0
#define HHI_VID_DIVIDER_CNTL                       0x66
#define HHI_VID_PLL_CLK_DIV                        0x68
#define HHI_EDP_APB_CLK_CNTL                       0x7b
#define HHI_EDP_APB_CLK_CNTL_M8M2                  0x82
#define HHI_EDP_TX_PHY_CNTL0                       0x9c
#define HHI_EDP_TX_PHY_CNTL1                       0x9d
/*  T7  */
#define CLKCTRL_VID_CLK0_CTRL                      0x0030
#define CLKCTRL_VID_CLK0_CTRL2                     0x0031
#define CLKCTRL_VID_CLK0_DIV                       0x0032
#define CLKCTRL_VIID_CLK0_DIV                      0x0033
#define CLKCTRL_VIID_CLK0_CTRL                     0x0034
#define CLKCTRL_VID_CLK1_CTRL                      0x0073
#define CLKCTRL_VID_CLK1_CTRL2                     0x0074
#define CLKCTRL_VID_CLK1_DIV                       0x0075
#define CLKCTRL_VIID_CLK1_DIV                      0x0076
#define CLKCTRL_VIID_CLK1_CTRL                     0x0077
#define CLKCTRL_VID_CLK2_CTRL                      0x0078
#define CLKCTRL_VID_CLK2_CTRL2                     0x0079
#define CLKCTRL_VID_CLK2_DIV                       0x007a
#define CLKCTRL_VIID_CLK2_DIV                      0x007b
#define CLKCTRL_VIID_CLK2_CTRL                     0x007c
#define CLKCTRL_MIPIDSI_PHY_CLK_CTRL               0x0041
#define CLKCTRL_MIPI_DSI_MEAS_CLK_CTRL             0x0080
#define CLKCTRL_HDMI_VID_PLL_CLK_DIV               0x0081
/* T3 */
#define CLKCTRL_TCON_CLK_CNTL                      0x0087

/* T5W */
#define HHI_VIID_CLK0_DIV                          0x0a0
#define HHI_VIID_CLK0_CTRL                         0x0a1
#define HHI_VID_CLK0_CTRL2                         0x0a4
#define HHI_LVDS_TX_PHY_CNTL2                      0x09c
#define HHI_LVDS_TX_PHY_CNTL3                      0x09d
/* g12A */
#define HHI_HDMI_PLL_CNTL0                         0xc8
#define HHI_HDMI_PLL_CNTL1                         0xc9
#define HHI_HDMI_PLL_CNTL2                         0xca
#define HHI_HDMI_PLL_CNTL3                         0xcb
#define HHI_HDMI_PLL_CNTL4                         0xcc
#define HHI_HDMI_PLL_CNTL5                         0xcd
#define HHI_HDMI_PLL_CNTL6                         0xce
/* TL1 */
#define HHI_TCON_PLL_CNTL0                         0x020
#define HHI_TCON_PLL_CNTL1                         0x021
#define HHI_TCON_PLL_CNTL2                         0x022
#define HHI_TCON_PLL_CNTL3                         0x023
#define HHI_TCON_PLL_STS						   0x024
#define HHI_TCON_PLL_CNTL4                         0x0df
#define HHI_TCON_PLL_CNTL5						   0x0e0
#define HHI_TCON_PLL_CNTL6                         0x0e1

#define HHI_DSI_LVDS_EDP_CNTL0                     0xd1
#define HHI_DSI_LVDS_EDP_CNTL1                     0xd2
#define HHI_DIF_CSI_PHY_CNTL0                      0xd8
#define HHI_DIF_CSI_PHY_CNTL1                      0xd9
#define HHI_DIF_CSI_PHY_CNTL2                      0xda
#define HHI_DIF_CSI_PHY_CNTL3                      0xdb
#define HHI_DIF_CSI_PHY_CNTL5                      0xdd
#define HHI_LVDS_TX_PHY_CNTL0                      0x9a
#define HHI_LVDS_TX_PHY_CNTL1                      0x9b
#define HHI_VID2_PLL_CNTL                          0xe0
#define HHI_VID2_PLL_CNTL2                         0xe1
#define HHI_VID2_PLL_CNTL3                         0xe2
#define HHI_VID2_PLL_CNTL4                         0xe3
#define HHI_VID2_PLL_CNTL5                         0xe4
#define HHI_VID2_PLL_CNTL6                         0xe5
#define HHI_VID_LOCK_CLK_CNTL                      0xf2

#define HHI_DIF_CSI_PHY_CNTL10                     0x8e
#define HHI_DIF_CSI_PHY_CNTL11                     0x8f
#define HHI_DIF_CSI_PHY_CNTL12                     0x90
#define HHI_DIF_CSI_PHY_CNTL13                     0x91
#define HHI_DIF_CSI_PHY_CNTL14                     0x92
#define HHI_DIF_CSI_PHY_CNTL15                     0x93
#define HHI_DIF_CSI_PHY_CNTL16                     0xde
#define HHI_DIF_CSI_PHY_CNTL4                      0xe9
#define HHI_DIF_CSI_PHY_CNTL6                      0xea
#define HHI_DIF_CSI_PHY_CNTL7                      0xeb
#define HHI_DIF_CSI_PHY_CNTL8                      0xec
#define HHI_DIF_CSI_PHY_CNTL9                      0xed

/* AXG use PLL   0xff63c000 */
#define HHI_GP0_PLL_CNTL_AXG                       0x10
#define HHI_GP0_PLL_CNTL2_AXG                      0x11
#define HHI_GP0_PLL_CNTL3_AXG                      0x12
#define HHI_GP0_PLL_CNTL4_AXG                      0x13
#define HHI_GP0_PLL_CNTL5_AXG                      0x14
#define HHI_GP0_PLL_CNTL1_AXG                      0x16

/* G12A use PLL   0xff63c000 */
#define HHI_GP0_PLL_CNTL0                          0x10
#define HHI_GP0_PLL_CNTL1                          0x11
#define HHI_GP0_PLL_CNTL2                          0x12
#define HHI_GP0_PLL_CNTL3                          0x13
#define HHI_GP0_PLL_CNTL4                          0x14
#define HHI_GP0_PLL_CNTL5                          0x15
#define HHI_GP0_PLL_CNTL6                          0x16

#define HHI_MIPIDSI_PHY_CLK_CNTL                   0x95
#define HHI_VDIN_MEAS_CLK_CNTL                     0x094

#define HHI_MIPI_CNTL0                             0x00
#define HHI_MIPI_CNTL1                             0x01
#define HHI_MIPI_CNTL2                             0x02

#define HHI_DIF_TCON_CNTL0                         0x3c
#define HHI_DIF_TCON_CNTL1                         0x3d
#define HHI_DIF_TCON_CNTL2                         0x3e
#define HHI_TCON_CLK_CNTL                          0x9c

#define ANACTRL_DIF_PHY_CNTL1                      0x00c8
#define ANACTRL_DIF_PHY_CNTL2                      0x00c9
#define ANACTRL_DIF_PHY_CNTL3                      0x00ca
#define ANACTRL_DIF_PHY_CNTL4                      0x00cb
#define ANACTRL_DIF_PHY_CNTL5                      0x00cc
#define ANACTRL_DIF_PHY_CNTL6                      0x00cd
#define ANACTRL_DIF_PHY_CNTL7                      0x00ce
#define ANACTRL_DIF_PHY_CNTL8                      0x00cf
#define ANACTRL_DIF_PHY_CNTL9                      0x00d0
#define ANACTRL_DIF_PHY_CNTL10                     0x00d1
#define ANACTRL_DIF_PHY_CNTL11                     0x00d2
#define ANACTRL_DIF_PHY_CNTL12                     0x00d3
#define ANACTRL_DIF_PHY_CNTL13                     0x00d4
#define ANACTRL_DIF_PHY_CNTL14                     0x00d5
#define ANACTRL_DIF_PHY_CNTL15                     0x00d6
#define ANACTRL_DIF_PHY_CNTL16                     0x00d7
#define ANACTRL_DIF_PHY_CNTL17                     0x00d8
#define ANACTRL_DIF_PHY_CNTL18                     0x00d9
#define ANACTRL_DIF_PHY_CNTL19                     0x00da
#define ANACTRL_DIF_PHY_CNTL20                     0x00db
#define ANACTRL_DIF_PHY_CNTL21                     0x00dc
#define ANACTRL_TCON_PLL0_CNTL0                    0x00e0
#define ANACTRL_TCON_PLL0_CNTL1                    0x00e1
#define ANACTRL_TCON_PLL0_CNTL2                    0x00e2
#define ANACTRL_TCON_PLL0_CNTL3                    0x00e3
#define ANACTRL_TCON_PLL0_CNTL4                    0x00e4
#define ANACTRL_TCON_PLL1_CNTL0                    0x00e5
#define ANACTRL_TCON_PLL1_CNTL1                    0x00e6
#define ANACTRL_TCON_PLL1_CNTL2                    0x00e7
#define ANACTRL_TCON_PLL1_CNTL3                    0x00e8
#define ANACTRL_TCON_PLL1_CNTL4                    0x00e9
#define ANACTRL_TCON_PLL2_CNTL0                    0x00ea
#define ANACTRL_TCON_PLL2_CNTL1                    0x00eb
#define ANACTRL_TCON_PLL2_CNTL2                    0x00ec
#define ANACTRL_TCON_PLL2_CNTL3                    0x00ed
#define ANACTRL_TCON_PLL2_CNTL4                    0x00ee
#define ANACTRL_TCON_PLL0_STS                      0x00ef
#define ANACTRL_TCON_PLL1_STS                      0x00f0
#define ANACTRL_TCON_PLL2_STS                      0x00f1

/*T3*/
#define ANACTRL_LVDS_TX_PHY_CNTL0                  0x00f4
#define ANACTRL_LVDS_TX_PHY_CNTL1                  0x00f5
#define ANACTRL_LVDS_TX_PHY_CNTL2                  0x00f6
#define ANACTRL_LVDS_TX_PHY_CNTL3                  0x00f7
#define ANACTRL_VID_PLL_CLK_DIV                    0x00f8

/*C3*/
#define ANACTRL_GP0PLL_CTRL0                       0x0020
#define ANACTRL_GP0PLL_CTRL1                       0x0021
#define ANACTRL_GP0PLL_CTRL2                       0x0022
#define ANACTRL_GP0PLL_CTRL3                       0x0023
#define ANACTRL_GP0PLL_CTRL4                       0x0024
#define ANACTRL_GP0PLL_CTRL5                       0x0025
#define ANACTRL_GP0PLL_CTRL6                       0x0026
#define ANACTRL_GP0PLL_STS                         0x0027
#define ANACTRL_MIPIDSI_CTRL0                      0x00a0
#define ANACTRL_MIPIDSI_CTRL1                      0x00a1
#define ANACTRL_MIPIDSI_CTRL2                      0x00a2
#define ANACTRL_MIPIDSI_STS                        0x00a3

#define CLKCTRL_VOUTENC_CLK_CTRL                   0x0046

#define COMBO_DPHY_CNTL0                           0x0000
#define COMBO_DPHY_CNTL1                           0x0001
#define COMBO_DPHY_VID_PLL0_DIV                    0x0002
#define COMBO_DPHY_VID_PLL1_DIV                    0x0003
#define COMBO_DPHY_VID_PLL2_DIV                    0x0004
#define COMBO_DPHY_EDP_PIXEL_CLK_DIV               0x0005
#define COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0          0x0006
#define COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0          0x0007
#define COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL0          0x0008
#define COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL1          0x0009
#define COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL1          0x000a
#define COMBO_DPHY_EDP_LVDS_TX_PHY2_CNTL1          0x000b
#define COMBO_DPHY_RO_EDP_LVDS_TX_PHY0_CNTL1       0x0010
#define COMBO_DPHY_RO_EDP_LVDS_TX_PHY1_CNTL1       0x0011
#define COMBO_DPHY_RO_EDP_LVDS_TX_PHY2_CNTL1       0x0012

/*  Global control:  RESET_CBUS_BASE = 0x11 */
#define VERSION_CTRL                               0x1100
#define RESET0_REGISTER                            0x1101
#define RESET1_REGISTER                            0x1102
#define RESET2_REGISTER                            0x1103
#define RESET3_REGISTER                            0x1104
#define RESET4_REGISTER                            0x1105
#define RESET5_REGISTER                            0x1106
#define RESET6_REGISTER                            0x1107
#define RESET7_REGISTER                            0x1108
#define RESET0_MASK                                0x1110
#define RESET1_MASK                                0x1111
#define RESET2_MASK                                0x1112
#define RESET3_MASK                                0x1113
#define RESET4_MASK                                0x1114
#define RESET5_MASK                                0x1115
#define RESET6_MASK                                0x1116
#define CRT_MASK                                   0x1117
#define RESET7_MASK                                0x1118

/* t5 */
#define RESET0_MASK_T5                             0x0010
#define RESET1_MASK_T5                             0x0011
#define RESET2_MASK_T5                             0x0012
#define RESET3_MASK_T5                             0x0013
#define RESET4_MASK_T5                             0x0014
#define RESET5_MASK_T5                             0x0015
#define RESET6_MASK_T5                             0x0016
#define RESET7_MASK_T5                             0x0017
#define RESET0_LEVEL_T5                            0x0020
#define RESET1_LEVEL_T5                            0x0021
#define RESET2_LEVEL_T5                            0x0022
#define RESET3_LEVEL_T5                            0x0023
#define RESET4_LEVEL_T5                            0x0024
#define RESET5_LEVEL_T5                            0x0025
#define RESET6_LEVEL_T5                            0x0026
#define RESET7_LEVEL_T5                            0x0027

#define RESETCTRL_RESET0                           0x0000
#define RESETCTRL_RESET1                           0x0001
#define RESETCTRL_RESET2                           0x0002
#define RESETCTRL_RESET3                           0x0003
#define RESETCTRL_RESET4                           0x0004
#define RESETCTRL_RESET5                           0x0005
#define RESETCTRL_RESET6                           0x0006
#define RESETCTRL_RESET0_LEVEL                     0x0010
#define RESETCTRL_RESET1_LEVEL                     0x0011
#define RESETCTRL_RESET2_LEVEL                     0x0012
#define RESETCTRL_RESET3_LEVEL                     0x0013
#define RESETCTRL_RESET4_LEVEL                     0x0014
#define RESETCTRL_RESET5_LEVEL                     0x0015
#define RESETCTRL_RESET6_LEVEL                     0x0016
#define RESETCTRL_RESET0_MASK                      0x0020
#define RESETCTRL_RESET1_MASK                      0x0021
#define RESETCTRL_RESET2_MASK                      0x0022
#define RESETCTRL_RESET3_MASK                      0x0023
#define RESETCTRL_RESET4_MASK                      0x0024
#define RESETCTRL_RESET5_MASK                      0x0025
#define RESETCTRL_RESET6_MASK                      0x0026

/* ********************************
 * TCON:  VCBUS_BASE = 0x14
 */
/* TCON_L register */
#define L_GAMMA_CNTL_PORT                          0x1400
#define L_GAMMA_DATA_PORT                          0x1401
#define L_GAMMA_ADDR_PORT                          0x1402
#define L_GAMMA_VCOM_HSWITCH_ADDR                  0x1403
#define L_RGB_BASE_ADDR                            0x1405
#define L_RGB_COEFF_ADDR                           0x1406
#define L_POL_CNTL_ADDR                            0x1407
#define L_DITH_CNTL_ADDR                           0x1408
#define L_GAMMA_PROBE_CTRL                         0x1409

#define LCD_GAMMA_CNTL_PORT0                       0x14b4

/* read only */
#define L_GAMMA_PROBE_COLOR_L                      0x140a
#define L_GAMMA_PROBE_COLOR_H                      0x140b
#define L_GAMMA_PROBE_HL_COLOR                     0x140c
#define L_GAMMA_PROBE_POS_X                        0x140d
#define L_GAMMA_PROBE_POS_Y                        0x140e
#define L_STH1_HS_ADDR                             0x1410
#define L_STH1_HE_ADDR                             0x1411
#define L_STH1_VS_ADDR                             0x1412
#define L_STH1_VE_ADDR                             0x1413
#define L_STH2_HS_ADDR                             0x1414
#define L_STH2_HE_ADDR                             0x1415
#define L_STH2_VS_ADDR                             0x1416
#define L_STH2_VE_ADDR                             0x1417
#define L_OEH_HS_ADDR                              0x1418
#define L_OEH_HE_ADDR                              0x1419
#define L_OEH_VS_ADDR                              0x141a
#define L_OEH_VE_ADDR                              0x141b
#define L_VCOM_HSWITCH_ADDR                        0x141c
#define L_VCOM_VS_ADDR                             0x141d
#define L_VCOM_VE_ADDR                             0x141e
#define L_CPV1_HS_ADDR                             0x141f
#define L_CPV1_HE_ADDR                             0x1420
#define L_CPV1_VS_ADDR                             0x1421
#define L_CPV1_VE_ADDR                             0x1422
#define L_CPV2_HS_ADDR                             0x1423
#define L_CPV2_HE_ADDR                             0x1424
#define L_CPV2_VS_ADDR                             0x1425
#define L_CPV2_VE_ADDR                             0x1426
#define L_STV1_HS_ADDR                             0x1427
#define L_STV1_HE_ADDR                             0x1428
#define L_STV1_VS_ADDR                             0x1429
#define L_STV1_VE_ADDR                             0x142a
#define L_STV2_HS_ADDR                             0x142b
#define L_STV2_HE_ADDR                             0x142c
#define L_STV2_VS_ADDR                             0x142d
#define L_STV2_VE_ADDR                             0x142e
#define L_OEV1_HS_ADDR                             0x142f
#define L_OEV1_HE_ADDR                             0x1430
#define L_OEV1_VS_ADDR                             0x1431
#define L_OEV1_VE_ADDR                             0x1432
#define L_OEV2_HS_ADDR                             0x1433
#define L_OEV2_HE_ADDR                             0x1434
#define L_OEV2_VS_ADDR                             0x1435
#define L_OEV2_VE_ADDR                             0x1436
#define L_OEV3_HS_ADDR                             0x1437
#define L_OEV3_HE_ADDR                             0x1438
#define L_OEV3_VS_ADDR                             0x1439
#define L_OEV3_VE_ADDR                             0x143a
#define L_LCD_PWR_ADDR                             0x143b
#define L_LCD_PWM0_LO_ADDR                         0x143c
#define L_LCD_PWM0_HI_ADDR                         0x143d
#define L_LCD_PWM1_LO_ADDR                         0x143e
#define L_LCD_PWM1_HI_ADDR                         0x143f
#define L_INV_CNT_ADDR                             0x1440
#define L_TCON_MISC_SEL_ADDR                       0x1441
#define L_DUAL_PORT_CNTL_ADDR                      0x1442
#define MLVDS_CLK_CTL1_HI                          0x1443
#define MLVDS_CLK_CTL1_LO                          0x1444
/* [31:30] enable mlvds clocks
 * [24]    mlvds_clk_half_delay       24 //Bit 0
 * [23:0]  mlvds_clk_pattern           0 //Bit 23:0
 */
#define L_TCON_DOUBLE_CTL                          0x1449
#define L_TCON_PATTERN_HI                          0x144a
#define L_TCON_PATTERN_LO                          0x144b
#define LDIM_BL_ADDR_PORT                          0x144e
#define LDIM_BL_DATA_PORT                          0x144f
#define L_DE_HS_ADDR                               0x1451
#define L_DE_HE_ADDR                               0x1452
#define L_DE_VS_ADDR                               0x1453
#define L_DE_VE_ADDR                               0x1454
#define L_HSYNC_HS_ADDR                            0x1455
#define L_HSYNC_HE_ADDR                            0x1456
#define L_HSYNC_VS_ADDR                            0x1457
#define L_HSYNC_VE_ADDR                            0x1458
#define L_VSYNC_HS_ADDR                            0x1459
#define L_VSYNC_HE_ADDR                            0x145a
#define L_VSYNC_VS_ADDR                            0x145b
#define L_VSYNC_VE_ADDR                            0x145c
/* bit 8 -- vfifo_mcu_enable
 * bit 7 -- halt_vs_de
 * bit 6 -- R8G8B8_format
 * bit 5 -- R6G6B6_format (round to 6 bits)
 * bit 4 -- R5G6B5_format
 * bit 3 -- dac_dith_sel
 * bit 2 -- lcd_mcu_enable_de     -- ReadOnly
 * bit 1 -- lcd_mcu_enable_vsync  -- ReadOnly
 * bit 0 -- lcd_mcu_enable
 */
#define L_LCD_MCU_CTL                              0x145d

/* ************************************
 *  TCON register
 */
#define GAMMA_CNTL_PORT                            0x1480
   #define  GAMMA_VCOM_POL    7
   #define  GAMMA_RVS_OUT     6
   /* Read Only */
   #define  ADR_RDY           5
   /* Read Only */
   #define  WR_RDY            4
   /* Read Only */
   #define  RD_RDY            3
   #define  GAMMA_TR          2
   #define  GAMMA_SET         1
   #define  GAMMA_EN          0
#define GAMMA_DATA_PORT                            0x1481
#define GAMMA_ADDR_PORT                            0x1482
   #define  H_RD              12
   #define  H_AUTO_INC        11
   #define  H_SEL_R           10
   #define  H_SEL_G           9
   #define  H_SEL_B           8
   /* 7:0 */
   #define  HADR_MSB          7
   #define  HADR              0
#define GAMMA_VCOM_HSWITCH_ADDR                    0x1483
#define RGB_BASE_ADDR                              0x1485
#define RGB_COEFF_ADDR                             0x1486
#define POL_CNTL_ADDR                              0x1487
   /* FOR DCLK OUTPUT */
   #define   DCLK_SEL             14
   /* FOR RGB format DVI output */
   #define   TCON_VSYNC_SEL_DVI   11
   /* FOR RGB format DVI output */
   #define   TCON_HSYNC_SEL_DVI   10
   /* FOR RGB format DVI output */
   #define   TCON_DE_SEL_DVI      9
   #define   CPH3_POL         8
   #define   CPH2_POL         7
   #define   CPH1_POL         6
   #define   TCON_DE_SEL      5
   #define   TCON_VS_SEL      4
   #define   TCON_HS_SEL      3
   #define   DE_POL           2
   #define   VS_POL           1
   #define   HS_POL           0
#define DITH_CNTL_ADDR                             0x1488
   #define  DITH10_EN         10
   #define  DITH8_EN          9
   #define  DITH_MD           8
   /* 7:4 */
   #define  DITH10_CNTL_MSB   7
   #define  DITH10_CNTL       4
   /* 3:0 */
   #define  DITH8_CNTL_MSB    3
   #define  DITH8_CNTL        0
/* Bit 1 highlight_en
 * Bit 0 probe_en
 */
#define GAMMA_PROBE_CTRL                           0x1489
/* read only
 * Bit [15:0]  probe_color[15:0]
 */
#define GAMMA_PROBE_COLOR_L                        0x148a
/* Read only
 * Bit 15: if true valid probed color
 * Bit [13:0]  probe_color[29:16]
 */
#define GAMMA_PROBE_COLOR_H                        0x148b
/* bit 15:0, 5:6:5 color */
#define GAMMA_PROBE_HL_COLOR                       0x148c
/* 12:0 pos_x */
#define GAMMA_PROBE_POS_X                          0x148d
/* 12:0 pos_y */
#define GAMMA_PROBE_POS_Y                          0x148e
#define STH1_HS_ADDR                               0x1490
#define STH1_HE_ADDR                               0x1491
#define STH1_VS_ADDR                               0x1492
#define STH1_VE_ADDR                               0x1493
#define STH2_HS_ADDR                               0x1494
#define STH2_HE_ADDR                               0x1495
#define STH2_VS_ADDR                               0x1496
#define STH2_VE_ADDR                               0x1497
#define OEH_HS_ADDR                                0x1498
#define OEH_HE_ADDR                                0x1499
#define OEH_VS_ADDR                                0x149a
#define OEH_VE_ADDR                                0x149b
#define VCOM_HSWITCH_ADDR                          0x149c
#define VCOM_VS_ADDR                               0x149d
#define VCOM_VE_ADDR                               0x149e
#define CPV1_HS_ADDR                               0x149f
#define CPV1_HE_ADDR                               0x14a0
#define CPV1_VS_ADDR                               0x14a1
#define CPV1_VE_ADDR                               0x14a2
#define CPV2_HS_ADDR                               0x14a3
#define CPV2_HE_ADDR                               0x14a4
#define CPV2_VS_ADDR                               0x14a5
#define CPV2_VE_ADDR                               0x14a6
#define STV1_HS_ADDR                               0x14a7
#define STV1_HE_ADDR                               0x14a8
#define STV1_VS_ADDR                               0x14a9
#define STV1_VE_ADDR                               0x14aa
#define STV2_HS_ADDR                               0x14ab
#define STV2_HE_ADDR                               0x14ac
#define STV2_VS_ADDR                               0x14ad
#define STV2_VE_ADDR                               0x14ae
#define OEV1_HS_ADDR                               0x14af
#define OEV1_HE_ADDR                               0x14b0
#define OEV1_VS_ADDR                               0x14b1
#define OEV1_VE_ADDR                               0x14b2
#define OEV2_HS_ADDR                               0x14b3
#define OEV2_HE_ADDR                               0x14b4
#define OEV2_VS_ADDR                               0x14b5
#define OEV2_VE_ADDR                               0x14b6
#define OEV3_HS_ADDR                               0x14b7
#define OEV3_HE_ADDR                               0x14b8
#define OEV3_VS_ADDR                               0x14b9
#define OEV3_VE_ADDR                               0x14ba
#define LCD_PWR_ADDR                               0x14bb
   #define      LCD_VDD        5
   #define      LCD_VBL        4
   #define      LCD_GPI_MSB    3
   #define      LCD_GPIO       0
#define LCD_PWM0_LO_ADDR                           0x14bc
#define LCD_PWM0_HI_ADDR                           0x14bd
#define LCD_PWM1_LO_ADDR                           0x14be
#define LCD_PWM1_HI_ADDR                           0x14bf
#define INV_CNT_ADDR                               0x14c0
   #define     INV_EN          4
   #define     INV_CNT_MSB     3
   #define     INV_CNT         0
#define TCON_MISC_SEL_ADDR                         0x14c1
   #define     STH2_SEL        12
   #define     STH1_SEL        11
   #define     OEH_SEL         10
   #define     VCOM_SEL         9
   #define     DB_LINE_SW       8
   #define     CPV2_SEL         7
   #define     CPV1_SEL         6
   #define     STV2_SEL         5
   #define     STV1_SEL         4
   #define     OEV_UNITE        3
   #define     OEV3_SEL         2
   #define     OEV2_SEL         1
   #define     OEV1_SEL         0
#define DUAL_PORT_CNTL_ADDR                        0x14c2
   #define     OUTPUT_YUV       15
   /* 14:12 */
   #define     DUAL_IDF         12
   /* 11:9 */
   #define     DUAL_ISF         9
   #define     LCD_ANALOG_SEL_CPH3   8
   #define     LCD_ANALOG_3PHI_CLK_SEL   7
   #define     LCD_LVDS_SEL54   6
   #define     LCD_LVDS_SEL27   5
   #define     LCD_TTL_SEL      4
   #define     DUAL_LVDC_EN     3
   #define     PORT_SWP         2
   #define     RGB_SWP          1
   #define     BIT_SWP          0

/* New from M3 :
 * Bit 15:12 -- Enable OFFSET Double Generate(TCON7-TCON4)
 * Bit 11:0 -- de_hs(old tcon) second offset_hs (new tcon)
 */
#define DE_HS_ADDR                                 0x14d1
/* New from M3 :
 * Bit 15:12 -- Enable OFFSET Double Generate(TCON3-TCON0)
 */
#define DE_HE_ADDR                                 0x14d2
#define DE_VS_ADDR                                 0x14d3
#define DE_VE_ADDR                                 0x14d4
#define HSYNC_HS_ADDR                              0x14d5
#define HSYNC_HE_ADDR                              0x14d6
#define HSYNC_VS_ADDR                              0x14d7
#define HSYNC_VE_ADDR                              0x14d8
#define VSYNC_HS_ADDR                              0x14d9
#define VSYNC_HE_ADDR                              0x14da
#define VSYNC_VS_ADDR                              0x14db
#define VSYNC_VE_ADDR                              0x14dc
/* bit 8 -- vfifo_mcu_enable
 * bit 7 -- halt_vs_de
 * bit 6 -- R8G8B8_format
 * bit 5 -- R6G6B6_format (round to 6 bits)
 * bit 4 -- R5G6B5_format
 * bit 3 -- dac_dith_sel
 * bit 2 -- lcd_mcu_enable_de     -- ReadOnly
 * bit 1 -- lcd_mcu_enable_vsync  -- ReadOnly
 * bit 0 -- lcd_mcu_enable
 */
#define LCD_MCU_CTL                                0x14dd
/* ReadOnly
 *   R5G6B5 when R5G6B5_format
 *   G8R8   when R8G8B8_format
 *   G5R10  Other
 */
#define LCD_MCU_DATA_0                             0x14de
/* ReadOnly
 *   G8B8   when R8G8B8_format
 *   G5B10  Other
 */
#define LCD_MCU_DATA_1                             0x14df
#define LVDS_CH_SWAP0                              0x14e1
#define LVDS_CH_SWAP1                              0x14e2
#define LVDS_CH_SWAP2                              0x14e3

#define DE_HS_ADDR_T7                              0x19d1
//New from M3 :
//Bit 15:12 -- Enable OFFSET Double Generate(TCON3-TCON0)
#define DE_HE_ADDR_T7                               0x19d2
#define DE_VS_ADDR_T7                              0x19d3
#define DE_VE_ADDR_T7                              0x19d4
#define HSYNC_HS_ADDR_T7                           0x19d5
#define HSYNC_HE_ADDR_T7                           0x19d6
#define HSYNC_VS_ADDR_T7                           0x19d7
#define HSYNC_VE_ADDR_T7                           0x19d8
#define VSYNC_HS_ADDR_T7                           0x19d9
#define VSYNC_HE_ADDR_T7                           0x19da
#define VSYNC_VS_ADDR_T7                           0x19db
#define VSYNC_VE_ADDR_T7                           0x19dc

#define LVDS_SER_EN_T7                             0x19f0
#define LVDS_PACK_CNTL_ADDR_T7                     0x19d0
#define LVDS_GEN_CNTL_T7                           0x19e0
#define P2P_CH_SWAP0_T7                            0x195e
#define P2P_CH_SWAP1_T7                            0x195f
#define P2P_BIT_REV_T7                             0x1950
/* **************************************************************************
 * Vbyone registers  (Note: no MinLVDS in G9tv, share the register)
 */
#define VBO_CTRL_L                                 0x1460
#define VBO_CTRL_H                                 0x1461
#define VBO_SOFT_RST                               0x1462
#define VBO_LANES                                  0x1463
#define VBO_VIN_CTRL                               0x1464
#define VBO_ACT_VSIZE                              0x1465
#define VBO_REGION_00                              0x1466
#define VBO_REGION_01                              0x1467
#define VBO_REGION_02                              0x1468
#define VBO_REGION_03                              0x1469
#define VBO_VBK_CTRL_0                             0x146a
#define VBO_VBK_CTRL_1                             0x146b
#define VBO_HBK_CTRL                               0x146c
#define VBO_PXL_CTRL                               0x146d
#define VBO_LANE_SKEW_L                            0x146e
#define VBO_LANE_SKEW_H                            0x146f
#define VBO_GCLK_LANE_L                            0x1470
#define VBO_GCLK_LANE_H                            0x1471
#define VBO_GCLK_MAIN                              0x1472
#define VBO_STATUS_L                               0x1473
#define VBO_STATUS_H                               0x1474
#define VBO_LANE_OUTPUT                            0x1475
#define LCD_PORT_SWAP                              0x1476
#define VBO_TMCHK_THRD_L                           0x1478
#define VBO_TMCHK_THRD_H                           0x1479
#define VBO_FSM_HOLDER_L                           0x147a
#define VBO_FSM_HOLDER_H                           0x147b
#define VBO_INTR_STATE_CTRL                        0x147c
#define VBO_INTR_UNMASK                            0x147d
#define VBO_TMCHK_HSYNC_STATE_L                    0x147e
#define VBO_TMCHK_HSYNC_STATE_H                    0x147f
#define VBO_TMCHK_VSYNC_STATE_L                    0x14f4
#define VBO_TMCHK_VSYNC_STATE_H                    0x14f5
#define VBO_TMCHK_VDE_STATE_L                      0x14f6
#define VBO_TMCHK_VDE_STATE_H                      0x14f7
#define VBO_INTR_STATE                             0x14f8
#define VBO_INFILTER_CTRL                          0x14f9
#define VBO_INFILTER_TICK_PERIOD_L                 0x14f9
#define VBO_INSGN_CTRL                             0x14fa
#define VBO_INFILTER_TICK_PERIOD_H                 0x1477
/* T7 */
#define VBO_CTRL_L_T7                              0x1960
#define VBO_CTRL_H_T7                              0x1961
#define VBO_SOFT_RST_T7                            0x1962
#define VBO_LANES_T7                               0x1963
#define VBO_VIN_CTRL_T7                            0x1964
#define VBO_ACT_VSIZE_T7                           0x1965
#define VBO_REGION_00_T7                           0x1966
#define VBO_REGION_01_T7                           0x1967
#define VBO_REGION_02_T7                           0x1968
#define VBO_REGION_03_T7                           0x1969
#define VBO_VBK_CTRL_0_T7                          0x196a
#define VBO_VBK_CTRL_1_T7                          0x196b
#define VBO_HBK_CTRL_T7                            0x196c
#define VBO_PXL_CTRL_T7                            0x196d
#define VBO_LANE_SKEW_L_T7                         0x196e
#define VBO_LANE_SKEW_H_T7                         0x196f
#define VBO_GCLK_LANE_L_T7                         0x1970
#define VBO_GCLK_LANE_H_T7                         0x1971
#define VBO_GCLK_MAIN_T7                           0x1972
#define VBO_STATUS_L_T7                            0x1973
#define VBO_STATUS_H_T7                            0x1974
#define VBO_LANE_OUTPUT_T7                         0x1975
#define LCD_PORT_SWAP_T7                           0x1976
#define VBO_TMCHK_THRD_L_T7                        0x1978
#define VBO_TMCHK_THRD_H_T7                        0x1979
#define VBO_FSM_HOLDER_L_T7                        0x197a
#define VBO_FSM_HOLDER_H_T7                        0x197b
#define VBO_INTR_STATE_CTRL_T7                     0x197c
#define VBO_INTR_UNMASK_T7                         0x197d
#define VBO_TMCHK_HSYNC_STATE_L_T7                 0x197e
#define VBO_TMCHK_HSYNC_STATE_H_T7                 0x197f
#define VBO_TMCHK_VSYNC_STATE_L_T7                 0x19f4
#define VBO_TMCHK_VSYNC_STATE_H_T7                 0x19f5
#define VBO_TMCHK_VDE_STATE_L_T7                   0x19f6
#define VBO_TMCHK_VDE_STATE_H_T7                   0x19f7
#define VBO_INTR_STATE_T7                          0x19f8
#define VBO_INFILTER_CTRL_T7                       0x19f9
#define VBO_INSGN_CTRL_T7                          0x19fa
#define VBO_INFILTER_CTRL_H_T7                     0x1977

/* ********************************
 * Video Interface:  VENC_VCBUS_BASE = 0x1b
 */
#define VENC_INTCTRL                               0x1b6e

/* ********************************
 * ENCL:  VCBUS_BASE = 0x1c
 */
/* ENCL */
/* bit 15:8 -- vfifo2vd_vd_sel
 * bit 7 -- vfifo2vd_drop
 * bit 6:1 -- vfifo2vd_delay
 * bit 0 -- vfifo2vd_en
 */
#define ENCL_VFIFO2VD_CTL                          0x1c90
/* bit 12:0 -- vfifo2vd_pixel_start */
#define ENCL_VFIFO2VD_PIXEL_START                  0x1c91
/* bit 12:00 -- vfifo2vd_pixel_end */
#define ENCL_VFIFO2VD_PIXEL_END                    0x1c92
/* bit 10:0 -- vfifo2vd_line_top_start */
#define ENCL_VFIFO2VD_LINE_TOP_START               0x1c93
/* bit 10:00 -- vfifo2vd_line_top_end */
#define ENCL_VFIFO2VD_LINE_TOP_END                 0x1c94
/* bit 10:00 -- vfifo2vd_line_bot_start */
#define ENCL_VFIFO2VD_LINE_BOT_START               0x1c95
/* bit 10:00 -- vfifo2vd_line_bot_end */
#define ENCL_VFIFO2VD_LINE_BOT_END                 0x1c96
#define ENCL_VFIFO2VD_CTL2                         0x1c97
#define ENCL_TST_EN                                0x1c98
#define ENCL_TST_MDSEL                             0x1c99
#define ENCL_TST_Y                                 0x1c9a
#define ENCL_TST_CB                                0x1c9b
#define ENCL_TST_CR                                0x1c9c
#define ENCL_TST_CLRBAR_STRT                       0x1c9d
#define ENCL_TST_CLRBAR_WIDTH                      0x1c9e
#define ENCL_TST_VDCNT_STSET                       0x1c9f

/* ENCL registers */
#define ENCL_VIDEO_EN                              0x1ca0
#define ENCL_VIDEO_Y_SCL                           0x1ca1
#define ENCL_VIDEO_PB_SCL                          0x1ca2
#define ENCL_VIDEO_PR_SCL                          0x1ca3
#define ENCL_VIDEO_Y_OFFST                         0x1ca4
#define ENCL_VIDEO_PB_OFFST                        0x1ca5
#define ENCL_VIDEO_PR_OFFST                        0x1ca6
/* ----- Video mode */
#define ENCL_VIDEO_MODE                            0x1ca7
#define ENCL_VIDEO_MODE_ADV                        0x1ca8
/* --------------- Debug pins */
#define ENCL_DBG_PX_RST                            0x1ca9
#define ENCL_DBG_LN_RST                            0x1caa
#define ENCL_DBG_PX_INT                            0x1cab
#define ENCL_DBG_LN_INT                            0x1cac
/* ----------- Video Advanced setting */
#define ENCL_VIDEO_YFP1_HTIME                      0x1cad
#define ENCL_VIDEO_YFP2_HTIME                      0x1cae
#define ENCL_VIDEO_YC_DLY                          0x1caf
#define ENCL_VIDEO_MAX_PXCNT                       0x1cb0
#define ENCL_VIDEO_HAVON_END                       0x1cb1
#define ENCL_VIDEO_HAVON_BEGIN                     0x1cb2
#define ENCL_VIDEO_VAVON_ELINE                     0x1cb3
#define ENCL_VIDEO_VAVON_BLINE                     0x1cb4
#define ENCL_VIDEO_HSO_BEGIN                       0x1cb5
#define ENCL_VIDEO_HSO_END                         0x1cb6
#define ENCL_VIDEO_VSO_BEGIN                       0x1cb7
#define ENCL_VIDEO_VSO_END                         0x1cb8
#define ENCL_VIDEO_VSO_BLINE                       0x1cb9
#define ENCL_VIDEO_VSO_ELINE                       0x1cba
#define ENCL_VIDEO_MAX_LNCNT                       0x1cbb
#define ENCL_VIDEO_BLANKY_VAL                      0x1cbc
#define ENCL_VIDEO_BLANKPB_VAL                     0x1cbd
#define ENCL_VIDEO_BLANKPR_VAL                     0x1cbe
#define ENCL_VIDEO_HOFFST                          0x1cbf
#define ENCL_VIDEO_VOFFST                          0x1cc0
#define ENCL_VIDEO_RGB_CTRL                        0x1cc1
#define ENCL_VIDEO_FILT_CTRL                       0x1cc2
#define ENCL_VIDEO_OFLD_VPEQ_OFST                  0x1cc3
#define ENCL_VIDEO_OFLD_VOAV_OFST                  0x1cc4
#define ENCL_VIDEO_MATRIX_CB                       0x1cc5
#define ENCL_VIDEO_MATRIX_CR                       0x1cc6
#define ENCL_VIDEO_RGBIN_CTRL                      0x1cc7
#define ENCL_MAX_LINE_SWITCH_POINT                 0x1cc8
#define ENCL_DACSEL_0                              0x1cc9
#define ENCL_DACSEL_1                              0x1cca

#define ENCL_VIDEO_H_PRE_DE_END                    0x1ccf
#define ENCL_VIDEO_H_PRE_DE_BEGIN                  0x1cd0
#define ENCL_VIDEO_V_PRE_DE_ELINE                  0x1cd1
#define ENCL_VIDEO_V_PRE_DE_BLINE                  0x1cd2
#define ENCL_INBUF_CNTL0                           0x1cd3
#define ENCL_INBUF_CNTL1                           0x1cd4
#define ENCL_INBUF_CNT                             0x1cd5

#define VPU_VENC_CTRL                              0x1cef
#define VPP_INT_LINE_NUM                           0x1dce
#define VPU_DISP_VIU0_CTRL                         0x2786
#define VPU_DISP_VIU1_CTRL                         0x2787
#define VPU_DISP_VIU2_CTRL                         0x2788
#define VPU_VENC_RGN_CTRL                          0x2789
#define VPU_VENC_RGN_RSIZE                         0x278a
#define VPU_DISP_WRAP_CTRL                         0x278b

#define LCD_RGB_BASE_ADDR                          0x14a5
#define LCD_RGB_COEFF_ADDR                         0x14a6
#define LCD_POL_CNTL_ADDR                          0x14a7
#define LCD_DITH_CNTL_ADDR                         0x14a8

#define P2P_CH_SWAP0                               0x4200
#define P2P_CH_SWAP1                               0x4201

/* ********************************
 * C3 vout
 * ********************************
 */
#define VPU_VOUT_CORE_CTRL                         0x0100
#define VPU_VOUT_INT_CTRL                          0x0101
#define VPU_VOUT_DETH_CTRL                         0x0102
#define VPU_VOUT_DTH_DATA                          0x0103
#define VPU_VOUT_DTH_ADDR                          0x0104
#define VPU_VOUT_HS_POS                            0x0112
#define VPU_VOUT_VSLN_E_POS                        0x0113
#define VPU_VOUT_VSPX_E_POS                        0x0114
#define VPU_VOUT_VSLN_O_POS                        0x0115
#define VPU_VOUT_VSPX_O_POS                        0x0116
#define VPU_VOUT_DE_PX_EN                          0x0117
#define VPU_VOUT_DELN_E_POS                        0x0118
#define VPU_VOUT_DELN_O_POS                        0x0119
#define VPU_VOUT_MAX_SIZE                          0x011a
#define VPU_VOUT_FLD_BGN_LINE                      0x011b
#define VPU_VOUTO_HS_POS                           0x0120
#define VPU_VOUTO_VSLN_E_POS                       0x0121
#define VPU_VOUTO_VSPX_E_POS                       0x0122
#define VPU_VOUTO_VSLN_O_POS                       0x0123
#define VPU_VOUTO_VSPX_O_POS                       0x0124
#define VPU_VOUTO_DE_PX_EN                         0x0125
#define VPU_VOUTO_DELN_E_POS                       0x0126
#define VPU_VOUTO_DELN_O_POS                       0x0127
#define VPU_VOUTO_MAX_SIZE                         0x0128
#define VPU_VOUTO_FLD_BGN_LINE                     0x0129
#define VPU_VOUT_BT_CTRL                           0x0130
#define VPU_VOUT_BT_PLD_LINE                       0x0131
#define VPU_VOUT_BT_PLDIDT0                        0x0132
#define VPU_VOUT_BT_PLDIDT1                        0x0133
#define VPU_VOUT_BT_BLK_DATA                       0x0134
#define VPU_VOUT_BT_DAT_CLPY                       0x0135
#define VPU_VOUT_BT_DAT_CLPC                       0x0136
#define VPU_VOUT_RO_INT                            0x0140

/* ********************************
 * Video post-processing:  VPP_VCBUS_BASE = 0x1d
 * Bit 31  vd1_bgosd_exchange_en for preblend
 * Bit 30  vd1_bgosd_exchange_en for postblend
 * bit 28   color management enable
 * Bit 27,  reserved
 * Bit 26:18, reserved
 * Bit 17, osd2 enable for preblend
 * Bit 16, osd1 enable for preblend
 * Bit 15, reserved
 * Bit 14, vd1 enable for preblend
 * Bit 13, osd2 enable for postblend
 * Bit 12, osd1 enable for postblend
 * Bit 11, reserved
 * Bit 10, vd1 enable for postblend
 * Bit 9,  if true, osd1 is alpha premultiplied
 * Bit 8,  if true, osd2 is alpha premultiplied
 * Bit 7,  postblend module enable
 * Bit 6,  preblend module enable
 * Bit 5,  if true, osd2 foreground compared with osd1 in preblend
 * Bit 4,  if true, osd2 foreground compared with osd1 in postblend
 * Bit 3,
 * Bit 2,  if true, disable resetting async fifo every vsync, otherwise every
 *           vsync the aync fifo will be reseted.
 * Bit 1,
 * Bit 0    if true, the output result of VPP is saturated
 */
#define VPP2_MISC                                  0x1926
/* Bit 31  vd1_bgosd_exchange_en for preblend
 * Bit 30  vd1_bgosd_exchange_en for postblend
 * Bit 28   color management enable
 * Bit 27,  if true, vd2 use viu2 output as the input, otherwise use normal
 *            vd2 from memory
 * Bit 26:18, vd2 alpha
 * Bit 17, osd2 enable for preblend
 * Bit 16, osd1 enable for preblend
 * Bit 15, vd2 enable for preblend
 * Bit 14, vd1 enable for preblend
 * Bit 13, osd2 enable for postblend
 * Bit 12, osd1 enable for postblend
 * Bit 11, vd2 enable for postblend
 * Bit 10, vd1 enable for postblend
 * Bit 9,  if true, osd1 is alpha premultiplied
 * Bit 8,  if true, osd2 is alpha premultiplied
 * Bit 7,  postblend module enable
 * Bit 6,  preblend module enable
 * Bit 5,  if true, osd2 foreground compared with osd1 in preblend
 * Bit 4,  if true, osd2 foreground compared with osd1 in postblend
 * Bit 3,
 * Bit 2,  if true, disable resetting async fifo every vsync, otherwise every
 *           vsync the aync fifo will be reseted.
 * Bit 1,
 * Bit 0	if true, the output result of VPP is saturated
 */
#define VPP_MISC                                   0x1d26

#define VPP2_POSTBLEND_H_SIZE                      0x1921
#define VPP_POSTBLEND_H_SIZE                       0x1d21
/* Bit 3	minus black level enable for vadj2
 * Bit 2	Video adjustment enable for vadj2
 * Bit 1	minus black level enable for vadj1
 * Bit 0	Video adjustment enable for vadj1
 */
#define VPP_VADJ_CTRL                              0x1d40
/* Bit 16:8  brightness, signed value
 * Bit 7:0	contrast, unsigned value,
 * contrast from  0 <= contrast <2
 */
#define VPP_VADJ1_Y                                0x1d41
/* cb' = cb*ma + cr*mb
 * cr' = cb*mc + cr*md
 * all are bit 9:0, signed value, -2 < ma/mb/mc/md < 2
 */
#define VPP_VADJ1_MA_MB                            0x1d42
#define VPP_VADJ1_MC_MD                            0x1d43
/* Bit 16:8  brightness, signed value
 * Bit 7:0   contrast, unsigned value,
 * contrast from  0 <= contrast <2
 */
#define VPP_VADJ2_Y                                0x1d44
/* cb' = cb*ma + cr*mb
 * cr' = cb*mc + cr*md
 * all are bit 9:0, signed value, -2 < ma/mb/mc/md < 2
 */
#define VPP_VADJ2_MA_MB                            0x1d45
#define VPP_VADJ2_MC_MD                            0x1d46

#define VPP_MATRIX_CTRL                            0x1d5f
/* Bit 28:16 coef00 */
/* Bit 12:0  coef01 */
#define VPP_MATRIX_COEF00_01                       0x1d60
/* Bit 28:16 coef02 */
/* Bit 12:0  coef10 */
#define VPP_MATRIX_COEF02_10                       0x1d61
/* Bit 28:16 coef11 */
/* Bit 12:0  coef12 */
#define VPP_MATRIX_COEF11_12                       0x1d62
/* Bit 28:16 coef20 */
/* Bit 12:0  coef21 */
#define VPP_MATRIX_COEF20_21                       0x1d63
#define VPP_MATRIX_COEF22                          0x1d64
/* Bit 26:16 offset0 */
/* Bit 10:0  offset1 */
#define VPP_MATRIX_OFFSET0_1                       0x1d65
/* Bit 10:0  offset2 */
#define VPP_MATRIX_OFFSET2                         0x1d66
/* Bit 26:16 pre_offset0 */
/* Bit 10:0  pre_offset1 */
#define VPP_MATRIX_PRE_OFFSET0_1                   0x1d67
/* Bit 10:0  pre_offset2 */
#define VPP_MATRIX_PRE_OFFSET2                     0x1d68

/* ********************************
 * VPU:  VPU_VCBUS_BASE = 0x27
 * [31:11] Reserved.
 * [10: 8] cntl_viu_vdin_sel_data. Select VIU to VDIN data path,
 *   must clear it first before changing the path selection:
 *          3'b000=Disable VIU to VDIN path;
 *          3'b001=Enable VIU of ENC_I domain to VDIN;
 *          3'b010=Enable VIU of ENC_P domain to VDIN;
 *          3'b100=Enable VIU of ENC_T domain to VDIN;
 * [ 6: 4] cntl_viu_vdin_sel_clk. Select which clock to VDIN path,
 *   must clear it first before changing the clock:
 *          3'b000=Disable VIU to VDIN clock;
 *          3'b001=Select encI clock to VDIN;
 *          3'b010=Select encP clock to VDIN;
 *          3'b100=Select encT clock to VDIN;
 * [ 3: 2] cntl_viu2_sel_venc. Select which one of the encI/P/T
 *   that VIU2 connects to:
 *         0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 * [ 1: 0] cntl_viu1_sel_venc. Select which one of the encI/P/T
 * that VIU1 connects to:
 * 0=ENCL, 1=ENCI, 2=ENCP, 3=ENCT.
 */
#define VPU_VIU_VENC_MUX_CTRL                      0x271a
#define ENCL_INFO_READ                             0x271f
#define VPU_VENCP_STAT                             0x1cec

/* Bit  6 RW, gclk_mpeg_vpu_misc
 * Bit  5 RW, gclk_mpeg_venc_l_top
 * Bit  4 RW, gclk_mpeg_vencl_int
 * Bit  3 RW, gclk_mpeg_vencp_int
 * Bit  2 RW, gclk_mpeg_vi2_top
 * Bit  1 RW, gclk_mpeg_vi_top
 * Bit  0 RW, gclk_mpeg_venc_p_top
 */
#define VPU_CLK_GATE                               0x2723
#define VPU_MISC_CTRL                              0x2740

#define VPU_VENCL_DITH_CTRL                        0x27e0

#define VPU_VLOCK_CTRL                             0x3000
#define VPU_VLOCK_ADJ_EN_SYNC_CTRL                 0x301d
#define VPU_VLOCK_GCLK_EN                          0x301e

#define CLKCTRL_VID_CLK_CTRL_A9    0x0130
#define CLKCTRL_VID_CLK_CTRL2_A9   0x0131
#define CLKCTRL_VID_CLK_DIV_A9     0x0132
#define CLKCTRL_VIID_CLK_DIV_A9    0x0133
#define CLKCTRL_VIID_CLK_CTRL_A9   0x0134

#define ANACTRL_DIF_PHY_CNTL1_A9   0x0002
#define ANACTRL_DIF_PHY_CNTL2_A9   0x0003
#define ANACTRL_DIF_PHY_CNTL3_A9   0x0004
#define ANACTRL_DIF_PHY_CNTL4_A9   0x0005
#define ANACTRL_DIF_PHY_CNTL5_A9   0x0006
#define ANACTRL_DIF_PHY_CNTL6_A9   0x0007
#define ANACTRL_DIF_PHY_CNTL7_A9   0x0008
#define ANACTRL_DIF_PHY_CNTL8_A9   0x0009
#define ANACTRL_DIF_PHY_CNTL9_A9   0x000a

#define ANACTRL_TCON_PLL0_CNTL0_A9	0x0020
#define ANACTRL_TCON_PLL0_CNTL1_A9	0x0021
#define ANACTRL_TCON_PLL0_CNTL2_A9	0x0022
#define ANACTRL_TCON_PLL0_CNTL3_A9	0x0023
#define ANACTRL_TCON_PLL0_CNTL4_A9	0x0024
#define ANACTRL_TCON_PLL1_CNTL0_A9	0x0025
#define ANACTRL_TCON_PLL1_CNTL1_A9	0x0026
#define ANACTRL_TCON_PLL1_CNTL2_A9	0x0027
#define ANACTRL_TCON_PLL1_CNTL3_A9	0x0028
#define ANACTRL_TCON_PLL1_CNTL4_A9	0x0029
#define ANACTRL_TCON_PLL2_CNTL0_A9	0x002a
#define ANACTRL_TCON_PLL2_CNTL1_A9	0x002b
#define ANACTRL_TCON_PLL2_CNTL2_A9	0x002c
#define ANACTRL_TCON_PLL2_CNTL3_A9	0x002d
#define ANACTRL_TCON_PLL2_CNTL4_A9	0x002e
#define ANACTRL_TCON_PLL3_CNTL0_A9	0x0030
#define ANACTRL_TCON_PLL3_CNTL1_A9	0x0031
#define ANACTRL_TCON_PLL3_CNTL2_A9	0x0032
#define ANACTRL_TCON_PLL3_CNTL3_A9	0x0033

#define ANACTRL_TCON_PLL0_STS_A9	0x0035
#define ANACTRL_TCON_PLL1_STS_A9	0x0036
#define ANACTRL_TCON_PLL2_STS_A9	0x0037
#define ANACTRL_TCON_PLL3_STS_A9	0x0038
#define ANACTRL_TCON_PLL_VLOCK_A9	0x0039
#define ANACTRL_LVDS_TX_PHY_CNTL0_A9	0x003a
#define ANACTRL_LVDS_TX_PHY_CNTL1_A9	0x003b
#define ANACTRL_LVDS_TX_PHY_CNTL2_A9	0x003c
#define ANACTRL_LVDS_TX_PHY_CNTL3_A9	0x003d
#define ANACTRL_VID_PLL0_CLK_DIV_A9	0x003e
#define ANACTRL_VID_PLL1_CLK_DIV_A9	0x003f

#define CLKCTRL_HDMI_CLK_CTRL_A9	0x0121

/* ******************************** */

/* ***********************************************
 * register access api
 */

int dptx_ioremap(struct dptx_drv_s *dptx, struct platform_device *pdev);

u32 dptx_vcbus_read(u32 reg);
void dptx_vcbus_write(u32 reg, u32 value);
void dptx_vcbus_setb(u32 reg, u32 value, u32 start, u32 len);
u32 dptx_vcbus_getb(u32 reg, u32 start, u32 len);

u32 dptx_clk_read(u32 reg);
void dptx_clk_write(u32 reg, u32 value);
void dptx_clk_setb(u32 reg, u32 value, u32 start, u32 len);
u32 dptx_clk_getb(u32 reg, u32 start, u32 len);

u32 dptx_ana_read(u32 reg);
void dptx_ana_write(u32 reg, u32 value);
void dptx_ana_setb(u32 reg, u32 value, u32 start, u32 len);
u32 dptx_ana_getb(u32 reg, u32 start, u32 len);

u32 dptx_periphs_read(struct dptx_drv_s *dptx, u32 reg);
void dptx_periphs_write(struct dptx_drv_s *dptx, u32 reg, u32 value);

u32 edptx_reg_read(struct dptx_drv_s *dptx, u8 port, u32 reg);
void edptx_reg_write(struct dptx_drv_s *dptx, u8 port, u32 reg, u32 value);
void __dptx_reg_setb(struct dptx_drv_s *dptx, u8 port, u32 reg, u32 value, u32 start, u32 len);
u32 __dptx_reg_getb(struct dptx_drv_s *dptx, u8 port, u32 reg, u32 start, u32 len);

u32 dptx_combo_dphy_read(struct dptx_drv_s *dptx, u32 reg);
void dptx_combo_dphy_write(struct dptx_drv_s *dptx, u32 reg, u32 value);
void dptx_combo_dphy_setb(struct dptx_drv_s *dptx, u32 reg, u32 value, u32 start, u32 len);
u32 dptx_combo_dphy_getb(struct dptx_drv_s *dptx, u32 reg, u32 start, u32 len);

u32 dptx_reset_read(struct dptx_drv_s *dptx, u32 reg);
void dptx_reset_write(struct dptx_drv_s *dptx, u32 reg, u32 value);
void dptx_reset_setb(struct dptx_drv_s *dptx, u32 reg, u32 value, u32 start, u32 len);
u32 dptx_reset_getb(struct dptx_drv_s *dptx, u32 reg, u32 start, u32 len);
#endif

