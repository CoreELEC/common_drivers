/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_AMLOGIC_MESON_C4_RESET_H
#define _DT_BINDINGS_AMLOGIC_MESON_C4_RESET_H

/* RESET0 */
/*					0-3*/
#define RESET_USB			4
/*					5-6*/
#define RESET_USB2PHY_APB		7
#define RESET_U2PHY20			8
/*					9*/
#define RESET_U2DRD			10
/*					11-20*/
#define RESET_GE2D			21
#define RESET_DWAP			22
/*					23-31*/

/* RESET1 */
#define RESET_AUDIO			32
#define RESET_AUDIO_SED			33
/*					34-36*/
#define RESET_DOS_CAPB3			37
#define RESET_DOS			38
/*					39-42*/
#define RESET_AMFC			43
#define RESET_AMFC_APB			44
#define RESET_NNA_APB			45
#define RESET_NNA			46
#define RESET_ETH_APB			47
#define RESET_ETH_AXI			48
#define RESET_ETH			49
#define RESET_ISP			50
/*					51-63*/

/* RESET2 */
/*					64-66*/
#define RESET_TS_PLL			67
/*					68-72*/
#define RESET_SPICC_0			73
/*					74-79*/
#define RESET_MSR_CLK			80
/*					81*/
#define RESET_SAR_ADC			82
/*					83-87*/
#define RESET_ACODEC			88
/*					89-90*/
#define RESET_WATCHDOG			91
/*					92-95*/

/* RESET3 */
/*					96-109*/
#define RESET_CVE			110
#define RESET_CVE_ARB			111
/*					112-127*/

/* RESET4 */
#define RESET_PWM_A			128
#define RESET_PWM_B			129
#define RESET_PWM_C			130
#define RESET_PWM_D			131
#define RESET_PWM_E			132
#define RESET_PWM_F			133
#define RESET_PWM_G			134
#define RESET_PWM_H			135
#define RESET_PWM_I			136
#define RESET_PWM_J			137
#define RESET_PWM_K			138
#define RESET_PWM_L			139
#define RESET_PWM_N			140
#define RESET_PWM_M			141
#define RESET_RTC			142
#define RESET_UART_A			143
#define RESET_UART_B			144
#define RESET_UART_C			145
#define RESET_UART_D			146
#define RESET_UART_E			147
#define RESET_UART_F			148
#define RESET_I2C_S_A			149
#define RESET_I2C_M_A			150
#define RESET_I2C_M_B			151
#define RESET_I2C_M_C			152
#define RESET_I2C_M_D			153
#define RESET_SD_EMMC_A			154
#define RESET_SD_EMMC_B			155
#define RESET_SD_EMMC_C			156
#define RESET_TAHOE			157
#define RESET_TAHOE_PAD			158
#define RESET_TAHOE_APB			159

/* RESET5 */
/*					160-172*/
#define RESET_BRG_NIC_NNA		173
#define RESET_BRG_MUX_NIC_MAIN		174
#define RESET_BRG_AO_NIC_ALL		175
/*					176-182*/
#define RESET_BRG_NIC_AMFC		183
#define RESET_BRG_NIC_VAPB		184
#define RESET_BRG_NIC_SDIO_B		185
#define RESET_BRG_NIC_SDIO_A		186
#define RESET_BRG_NIC_EMMC		187
#define RESET_BRG_NIC_DSU		188
#define RESET_BRG_NIC_CLK81		189
#define RESET_BRG_NIC_MAIN		190
#define RESET_BRG_NIC_ALL		191

#endif
