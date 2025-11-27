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

/* DI */
#define CLKID_VPU_CLKB_TMP_MUX			0
#define CLKID_VPU_CLKB_TMP_DIV			1
#define CLKID_VPU_CLKB_TMP			2
#define CLKID_VPU_CLKB_DIV			3
#define CLKID_VPU_CLKB				4

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
#define CLKID_SYS_CAN_0				19
#define CLKID_SYS_CAN_1				20
#define CLKID_SYS_SD_EMMC_A			21
#define CLKID_SYS_SD_EMMC_B			22
#define CLKID_SYS_SD_EMMC_C			23
#define CLKID_SYS_SC				24
#define CLKID_SYS_ACODEC			25
#define CLKID_SYS_MIPI_ISP			26
#define CLKID_SYS_MSR_CLK			27
#define CLKID_SYS_AUDIO				28
#define CLKID_SYS_MIPI_DSI_B			29
#define CLKID_SYS_MIPI_DSI1_PHY			30
#define CLKID_SYS_ETH				31
#define CLKID_SYS_ETH_MAC_1G			32
#define CLKID_SYS_UART_A			33
#define CLKID_SYS_UART_F			34
#define CLKID_SYS_TS_A55			35
#define CLKID_SYS_ETH_AXI_1G			36
#define CLKID_SYS_TS_DOS			37
#define CLKID_SYS_USB3DRD_B			38
#define CLKID_SYS_TS_CORE			39
#define CLKID_SYS_TS_PLL			40
#define CLKID_SYS_CDI_DIG_CLKIN			41
#define CLKID_SYS_CVE				42
#define CLKID_SYS_GE2D				43
#define CLKID_SYS_SPISG_0			44
#define CLKID_SYS_USB3DRD_1			45
#define CLKID_SYS_USB_U2H			46
#define CLKID_SYS_PCIE_MAC_A			47
#define CLKID_SYS_USB3DRD_A			48
#define CLKID_SYS_USB2DRD			49
#define CLKID_SYS_PCIE_PHY			50
#define CLKID_SYS_PCIE_MAC_B			51
#define CLKID_SYS_PERIPH			52
#define CLKID_SYS_PIO				53
#define CLKID_SYS_I3C				54
#define CLKID_SYS_I2C_M_E			55
#define CLKID_SYS_I2C_M_F			56
#define CLKID_SYS_HDMITX_APB			57
#define CLKID_SYS_I2C_M_I			58
#define CLKID_SYS_I2C_M_G			59
#define CLKID_SYS_I2C_M_H			60
#define CLKID_SYS_HDMI20_AES			61
#define CLKID_SYS_MMC_APB			62
#define CLKID_SYS_CSI2_HOST			63
#define CLKID_SYS_CSI2_ADAPT			64
#define CLKID_SYS_CPU_APB			65
#define CLKID_SYS_DSPA				66
#define CLKID_SYS_PP_DMA			67
#define CLKID_SYS_PP_WARPPER			68
#define CLKID_SYS_VPU_INTR			69
#define CLKID_SYS_CSI2_PHY0			70
#define CLKID_SYS_SAR_ADC			71
#define CLKID_SYS_GIC				72
#define CLKID_SYS_PWM_J				73
#define CLKID_SYS_PWM_I				74
#define CLKID_SYS_PWM_H				75
#define CLKID_SYS_PWM_N				76
#define CLKID_SYS_PWM_M				77
#define CLKID_SYS_PWM_L				78
#define CLKID_SYS_PWM_K				79

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

/* AO APB */
#define CLKID_AO_SYS_I3C			0
#define CLKID_AO_SYS_RTC_REG			1
#define CLKID_AO_SYS_CLK_TREE			2
#define CLKID_AO_SYS_RST_CTRL			3
#define CLKID_AO_SYS_PAD			4
#define CLKID_AO_SYS_RTC_DIG			5
#define CLKID_AO_SYS_IRQ			6
#define CLKID_AO_SYS_PWR_CTRL			7
#define CLKID_AO_SYS_PWM_A			8
#define CLKID_AO_SYS_PWM_B			9
#define CLKID_AO_SYS_PWM_C			10
#define CLKID_AO_SYS_PWM_D			11
#define CLKID_AO_SYS_PWM_E			12
#define CLKID_AO_SYS_PWM_F			13
#define CLKID_AO_SYS_PWM_G			14
#define CLKID_AO_SYS_I2C_M_A			15
#define CLKID_AO_SYS_I2C_M_B			16
#define CLKID_AO_SYS_I2C_M_C			17
#define CLKID_AO_SYS_I2C_M_D			18
#define CLKID_AO_SYS_SED			19
#define CLKID_AO_SYS_IR_CTRL			20
#define CLKID_AO_SYS_UART_B			21
#define CLKID_AO_SYS_UART_C			22
#define CLKID_AO_SYS_UART_D			23
#define CLKID_AO_SYS_UART_E			24
#define CLKID_AO_SYS_SPISG_0			25
#define CLKID_AO_SYS_RTC_SEC			26
#define CLKID_AO_SYS_CEC			27
#define CLKID_AO_SYS_AOCPU			28
#define CLKID_AO_SYS_SRAM			29
#define CLKID_AO_SYS_SPISG_1			30
#define CLKID_AO_SYS_SPISG_2			31

/* AO RTC */
#define CLKID_AO_RTC_DUAL			0
#define CLKID_AO_RTC_CLK			1

#endif  /* _DT_BINDINGS_CLOCK_AMLOGIC_A9_CLK_H */
