/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __C4_H
#define __C4_H

int c4_clk_init(void);
void c4_clk_exit(void);
int c4_aoclk_init(void);
void c4_aoclk_exit(void);

#define ANACTRL_FIXPLL_CTRL3                 (0x0013  << 2)
#define ANACTRL_GP0PLL_CTRL0                 (0x0020  << 2)
#define ANACTRL_GP0PLL_CTRL1                 (0x0021  << 2)
#define ANACTRL_GP0PLL_CTRL2                 (0x0022  << 2)
#define ANACTRL_GP0PLL_CTRL3                 (0x0023  << 2)
#define ANACTRL_HIFIPLL_CTRL0                (0x0040  << 2)
#define ANACTRL_HIFIPLL_CTRL1                (0x0041  << 2)
#define ANACTRL_HIFIPLL_CTRL2                (0x0042  << 2)
#define ANACTRL_HIFIPLL_CTRL3                (0x0043  << 2)
#define ANACTRL_MPLL_CTRL0                   (0x0060  << 2)
#define ANACTRL_MPLL_CTRL1                   (0x0061  << 2)
#define ANACTRL_MPLL_CTRL2                   (0x0062  << 2)
#define ANACTRL_MPLL_CTRL3                   (0x0063  << 2)
#define ANACTRL_MPLL_CTRL4                   (0x0064  << 2)

#define CLKCTRL_RTC_BY_OSCIN_CTRL0           (0x0002  << 2)
#define CLKCTRL_RTC_BY_OSCIN_CTRL1           (0x0003  << 2)
#define CLKCTRL_RTC_CTRL                     (0x0004  << 2)
#define CLKCTRL_SYS_CLK_EN0_REG0             (0x0011  << 2)
#define CLKCTRL_SYS_CLK_EN0_REG1             (0x0012  << 2)
#define CLKCTRL_SYS_CLK_EN0_REG2             (0x0013  << 2)
#define CLKCTRL_CLK12_24_CTRL                (0x002a  << 2)
#define CLKCTRL_AXI_CLK_EN0                  (0x002b  << 2)
#define CLKCTRL_GE2DCLK_CTRL                 (0x0043  << 2)
#define CLKCTRL_ISP0_CLK_CTRL                (0x0044  << 2)
#define CLKCTRL_DEWARPA_CLK_CTRL             (0x0045  << 2)
#define CLKCTRL_CVECLK_CTRL                  (0x0047  << 2)
#define CLKCTRL_VDEC_CLK_CTRL                (0x0050  << 2)
#define CLKCTRL_VDEC3_CLK_CTRL               (0x0052  << 2)
#define CLKCTRL_TS_CLK_CTRL                  (0x0056  << 2)
#define CLKCTRL_ETH_CLK_CTRL                 (0x0059  << 2)
#define CLKCTRL_SD_EMMC_CLK_CTRL0            (0x005a  << 2)
#define CLKCTRL_SD_EMMC_CLK_CTRL1            (0x005b  << 2)
#define CLKCTRL_SPISG_CLK_CTRL               (0x005d  << 2)
#define CLKCTRL_GEN_CLK_CTRL                 (0x005e  << 2)
#define CLKCTRL_SAR_CLK_CTRL0                (0x005f  << 2)
#define CLKCTRL_PWM_CLK_CD_CTRL              (0x0061  << 2)
#define CLKCTRL_PWM_CLK_EF_CTRL              (0x0062  << 2)
#define CLKCTRL_PWM_CLK_GH_CTRL              (0x0063  << 2)
#define CLKCTRL_PWM_CLK_IJ_CTRL              (0x0064  << 2)
#define CLKCTRL_PWM_CLK_KL_CTRL              (0x0065  << 2)
#define CLKCTRL_PWM_CLK_MN_CTRL              (0x0066  << 2)
#define CLKCTRL_TAHOE_CLK_CTRL               (0x0067  << 2)
#define CLKCTRL_NNA_CLK_CTRL                 (0x0088  << 2)
#define CLKCTRL_AMFC_CLK_CTRL                (0x0089  << 2)

#define AO_CLKCTRL_OSCIN_CTRL                (0x0000  << 2)
#define AO_CLKCTRL_RTC_SYS_CLK               (0x0001  << 2)
#define AO_CLKCTRL_PWM_CLK_JK_CTRL           (0x0006  << 2)
#define AO_CLKCTRL_ROUGH_32K_CTRL            (0x0007  << 2)

#endif  /* __C4_H */
