/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _DT_BINDINGS_CLOCK_AMLOGIC_A9_CLK_H
#define _DT_BINDINGS_CLOCK_AMLOGIC_A9_CLK_H

/* RTC */
#define CLKID_RTC_DUAL				0
#define CLKID_RTC_CLK				1

/* Smart Card */
#define CLKID_SC_PRE_MUX			0
#define CLKID_SC_PRE_DIV			1
#define CLKID_SC_PRE				2
#define CLKID_SC				3

/* Mali */
#define CLKID_MALI				0
#define CLKID_MALI_STACK			1

/* VAPB GE2D */
#define CLKID_VAPB_0_MUX			0
#define CLKID_VAPB_0_DIV			1
#define CLKID_VAPB_0				2
#define CLKID_VAPB_1_MUX			3
#define CLKID_VAPB_1_DIV			4
#define CLKID_VAPB_1				5
#define CLKID_VAPB				6
#define CLKID_GE2D				7

/* HDMI TX */
#define CLKID_HDMITX_SYS			0
#define CLKID_HDMITX_PRIF			1
#define CLKID_HDMITX_200M			2
#define CLKID_HDMITX_AUD			3

/* PWM */
#define CLKID_PWM_H				0
#define CLKID_PWM_I				1
#define CLKID_PWM_J				2
#define CLKID_PWM_K				3
#define CLKID_PWM_L				4
#define CLKID_PWM_M				5
#define CLKID_PWM_N				6

/* SPISG */
#define CLKID_SPISG_0				0
#define CLKID_SPISG_1				1
#define CLKID_SPISG_2				2

/* SD_EMMC */
#define CLKID_SD_EMMC_A				0
#define CLKID_SD_EMMC_B				1
#define CLKID_SD_EMMC_C				2

/* ETH */
#define CLKID_ETH_RMII_MUX			0
#define CLKID_ETH_RMII_DIV			1
#define CLKID_ETH_RMII				2
#define CLKID_ETH_125M				3

/* Temperature Sensor */
#define CLKID_TS_DIV				0
#define CLKID_TS				1

/* PCIE TL */
#define CLKID_PCIE_TL_0				0
#define CLKID_PCIE_TL_1				1

/* CVE VGE */
#define CLKID_CVE				0
#define CLKID_VGE				1

/* DPTX */
#define CLKID_DPTX_APB2				0
#define CLKID_DPTX_AUD				1

/* DSI MEAS */
#define CLKID_DSI_MEAS				0
#define CLKID_DSI_B_MEAS			1

/* HDMI RX */
#define CLKID_HDMIRX_5M				0
#define CLKID_HDMIRX_2M				1
#define CLKID_HDMIRX_CFG			2
#define CLKID_HDMIRX_HDCP2X			3
#define CLKID_HDMIRX_ACR_REF			4
#define CLKID_HDMIRX_METER			5

/* USB */
#define CLKID_USB_250M				0
#define CLKID_USB_48M_PRE			1

#define CLKID_USB_48M_DUAL			0
#define CLKID_USB_48M				1

/* APB */
#define CLKID_SYS_AM_AXI			0
#define CLKID_SYS_DOS				1
#define CLKID_SYS_TAHOE				2
#define CLKID_SYS_MIPI_DSI			3
#define CLKID_SYS_ETH_PHY			4
#define CLKID_SYS_AMFC				5
#define CLKID_SYS_MALI				6
#define CLKID_SYS_NNA				7
#define CLKID_SYS_ETH_AXI			8
#define CLKID_SYS_DP_APB			9
#define CLKID_SYS_EDPTX_APB			10
#define CLKID_SYS_U3HSG				11
#define CLKID_SYS_PP				12
#define CLKID_SYS_AUCPU				13
#define CLKID_SYS_GLB				14
#define CLKID_SYS_COMBO_DPHY_APB		15
#define CLKID_SYS_HDMIRX_APB			16
#define CLKID_SYS_HDMIRX_PCLK			17
#define CLKID_SYS_MIPI_DSI_PHY			18
#define CLKID_SYS_SD_EMMC_A			19
#define CLKID_SYS_SD_EMMC_B			20
#define CLKID_SYS_SD_EMMC_C			21
#define CLKID_SYS_SC				22
#define CLKID_SYS_ACODEC			23
#define CLKID_SYS_MIPI_ISP			24
#define CLKID_SYS_MSR_CLK			25
#define CLKID_SYS_AUDIO				26
#define CLKID_SYS_MIPI_DSI_B			27
#define CLKID_SYS_MIPI_DSI1_PHY			28
#define CLKID_SYS_ETH				29
#define CLKID_SYS_UART_A			30
#define CLKID_SYS_UART_F			31
#define CLKID_SYS_TS_A55			32
#define CLKID_SYS_USB3DRD_B			33
#define CLKID_SYS_TS_CORE			34
#define CLKID_SYS_TS_PLL			35
#define CLKID_SYS_CDI_DIG_CLKIN			36
#define CLKID_SYS_CVE				37
#define CLKID_SYS_GE2D				38
#define CLKID_SYS_SPISG_0			39
#define CLKID_SYS_USB3DRD_1			40
#define CLKID_SYS_USB_U2H			41
#define CLKID_SYS_PCIE_MAC_A			42
#define CLKID_SYS_USB3DRD_A			43
#define CLKID_SYS_USB2DRD			44
#define CLKID_SYS_PCIE_PHY			45
#define CLKID_SYS_PCIE_MAC_B			46
#define CLKID_SYS_I2C_M_E			47
#define CLKID_SYS_I2C_M_F			48
#define CLKID_SYS_HDMITX_APB			49
#define CLKID_SYS_I2C_M_I			50
#define CLKID_SYS_I2C_M_G			51
#define CLKID_SYS_I2C_M_H			52
#define CLKID_SYS_HDMI20_AES			53
#define CLKID_SYS_MMC_APB			54
#define CLKID_SYS_CSI2_HOST			55
#define CLKID_SYS_CSI2_ADAPT			56
#define CLKID_SYS_CPU_APB			57
#define CLKID_SYS_DSPA				58
#define CLKID_SYS_PERIPH_TOP			59
#define CLKID_SYS_PERIPH_TOP_APB		60
#define CLKID_SYS_VPU_INTR			61
#define CLKID_SYS_CSI2_PHY0			62
#define CLKID_SYS_SAR_ADC			63
#define CLKID_SYS_PWM_J				64
#define CLKID_SYS_GIC				65
#define CLKID_SYS_PWM_I				66
#define CLKID_SYS_PWM_H				67
#define CLKID_SYS_PWM_N				68
#define CLKID_SYS_PWM_M				69
#define CLKID_SYS_PWM_L				70
#define CLKID_SYS_PWM_K				71

/* 12M/24M/25M */
#define CLKID_24M_IN				0
#define CLKID_12M_24M				1
#define CLKID_25M_DIV				2
#define CLKID_25M				3

/* GEN OUT */
#define CLKID_GEN_SEL				0
#define CLKID_GEN_DIV				1
#define CLKID_GEN				2

/* CAN */
#define CLKID_CAN_PE_0				0
#define CLKID_CAN_PE_1				1
#define CLKID_CAN_FILTER_0			2
#define CLKID_CAN_FILTER_1			3

/* MCLK */
#define CLKID_MCLK_0_PRE_DIV			0
#define CLKID_MCLK_0_SEL			1
#define CLKID_MCLK_0_DIV			2
#define CLKID_MCLK_0				3
#define CLKID_MCLK_1_PRE_DIV			4
#define CLKID_MCLK_1_SEL			5
#define CLKID_MCLK_1_DIV			6
#define CLKID_MCLK_1				7

/* MCLK1 */
#define CLKID_MCLK1_0_PRE_DIV			0
#define CLKID_MCLK1_0_SEL			1
#define CLKID_MCLK1_0_DIV			2
#define CLKID_MCLK1_0				3
#define CLKID_MCLK1_1_PRE_DIV			4
#define CLKID_MCLK1_1_SEL			5
#define CLKID_MCLK1_1_DIV			6
#define CLKID_MCLK1_1				7

/* AO CECB */
#define CLKID_AO_CECB_DUAL			0
#define CLKID_AO_CECB_CLK			1

/* AO PWM */
#define CLKID_AO_PWM_A				0
#define CLKID_AO_PWM_B				1
#define CLKID_AO_PWM_C				2
#define CLKID_AO_PWM_D				3
#define CLKID_AO_PWM_E				4
#define CLKID_AO_PWM_F				5
#define CLKID_AO_PWM_G				6

#endif  /* _DT_BINDINGS_CLOCK_AMLOGIC_A9_CLK_H */
