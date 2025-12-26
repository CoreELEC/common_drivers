// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2022 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <dt-bindings/gpio/meson-c5-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"

static const struct pinctrl_pin_desc meson_c5_ao_pins[] = {
	MESON_PIN(GPIOF_0),
	MESON_PIN(GPIOF_1),
	MESON_PIN(GPIOF_2),
	MESON_PIN(GPIOF_3),
	MESON_PIN(GPIOF_4),
	MESON_PIN(GPIOF_5),
	MESON_PIN(GPIOF_6),
	MESON_PIN(GPIOP_0),
	MESON_PIN(GPIOP_1),
	MESON_PIN(GPIOP_2),
	MESON_PIN(GPIOP_3),
	MESON_PIN(GPIOP_4),
	MESON_PIN(GPIOP_5),
	MESON_PIN(GPIOP_6),
	MESON_PIN(GPIOP_7),
	MESON_PIN(GPIOP_8),
	MESON_PIN(GPIOP_9),
	MESON_PIN(GPIOP_10),
	MESON_PIN(GPIOP_11),
	MESON_PIN(GPIOP_12),
	MESON_PIN(GPIOP_13),
	MESON_PIN(GPIOAO_0),
	MESON_PIN(GPIOAO_1),
	MESON_PIN(GPIOAO_2),
	MESON_PIN(GPIOAO_3),
	MESON_PIN(GPIOAO_4),
	MESON_PIN(GPIOAO_5),
	MESON_PIN(GPIOAO_6),
	MESON_PIN(GPIOAO_7),
	MESON_PIN(GPIOAO_8),
	MESON_PIN(GPIOAO_9),
	MESON_PIN(GPIOAO_10),
	MESON_PIN(GPIOAO_11),
	MESON_PIN(GPIOAO_12),
	MESON_PIN(GPIOAO_13),
	MESON_PIN(GPIOAO_14),
	MESON_PIN(GPIOAO_15),
	MESON_PIN(GPIOAO_16),
	MESON_PIN(GPIOAO_17),
	MESON_PIN(GPIOAO_18),
	MESON_PIN(GPIOAO_19),
	MESON_PIN(GPIOAO_20),
	MESON_PIN(GPIOAO_21),
	MESON_PIN(GPIOAO_22),
	MESON_PIN(GPIOAO_23),
	MESON_PIN(GPIOAO_24),
	MESON_PIN(GPIOAO_25),
	MESON_PIN(GPIO_TEST_N)
};

/* BANK F func1 */
static const unsigned  int uart_ao_b_tx_f_pins[]	= { GPIOF_0 };
static const unsigned  int uart_ao_b_rx_f_pins[]	= { GPIOF_1 };
static const unsigned  int pwm_ao_a_hiz_pins[]		= { GPIOF_2 };
static const unsigned  int pwm_ao_b_hiz_pins[]		= { GPIOF_3 };
static const unsigned  int pwm_k_hiz_pins[]		= { GPIOF_4 };
static const unsigned  int pcie_preset_f_pins[]		= { GPIOF_5 };
static const unsigned  int pcieck_req_f_pins[]		= { GPIOF_6 };

/* BANK F func2 */
static const unsigned  int pwm_ao_a_f_pins[]		= { GPIOF_2 };
static const unsigned  int pwm_ao_b_f_pins[]		= { GPIOF_3 };
static const unsigned  int pwm_k_f_pins[]		= { GPIOF_4 };
static const unsigned  int pwm_l_f_pins[]		= { GPIOF_5 };
static const unsigned  int pwm_m_f_pins[]		= { GPIOF_6 };

/* BANK F func3 */
static const unsigned  int i2c1_ao_sck_f_pins[]		= { GPIOF_2 };
static const unsigned  int i2c1_ao_sda_f_pins[]		= { GPIOF_3 };
static const unsigned  int i3c0_ao_sck_pins[]		= { GPIOF_4 };
static const unsigned  int i3c0_ao_sda_pins[]		= { GPIOF_5 };
static const unsigned  int mclk12_24_pins[]		= { GPIOF_6 };

/* BANK F func4 */
static const unsigned  int spi0_ao_sck_f_pins[]		= { GPIOF_2 };
static const unsigned  int spi0_ao_mosi_f_pins[]	= { GPIOF_3 };
static const unsigned  int spi0_ao_miso_f_pins[]	= { GPIOF_4 };
static const unsigned  int spi0_ao_ss0_f_pins[]		= { GPIOF_5 };
static const unsigned  int clk_32k_ao_in_pins[]		= { GPIOF_6 };

/* BANK F func5 */
static const unsigned  int uart_ao_a_tx_f_pins[]	= { GPIOF_2 };
static const unsigned  int uart_ao_a_rx_f_pins[]	= { GPIOF_3 };
static const unsigned  int can_a_tx_pins[]		= { GPIOF_4 };
static const unsigned  int can_a_rx_pins[]		= { GPIOF_5 };

/* BANK F func6 */
static const unsigned  int tdm_d3_f_pins[]		= { GPIOF_2 };
static const unsigned  int tdm_d2_f_pins[]		= { GPIOF_3 };
static const unsigned  int tdm_sclk1_f_pins[]		= { GPIOF_4 };
static const unsigned  int tdm_fs1_f_pins[]		= { GPIOF_5 };
static const unsigned  int mclk1_f_pins[]		= { GPIOF_6 };

/* BANK F func7 */
static const unsigned  int ir_ao_out_pins[]		= { GPIOF_2 };
static const unsigned  int ir_ao_in_pins[]		= { GPIOF_3 };
static const unsigned  int gen_clk_f_pins[]		= { GPIOF_6 };

/* BANK P func1 */
static const unsigned  int psram_clkn_ao_pins[]		= { GPIOP_0  };
static const unsigned  int psram_clkp_ao_pins[]		= { GPIOP_1  };
static const unsigned  int psram_ce_n_ao_pins[]		= { GPIOP_2  };
static const unsigned  int psram_rst_n_ao_pins[]	= { GPIOP_3  };
static const unsigned  int psram_adq0_ao_pins[]		= { GPIOP_4  };
static const unsigned  int psram_adq1_ao_pins[]		= { GPIOP_5  };
static const unsigned  int psram_adq2_ao_pins[]		= { GPIOP_6  };
static const unsigned  int psram_adq3_ao_pins[]		= { GPIOP_7  };
static const unsigned  int psram_adq4_ao_pins[]		= { GPIOP_8  };
static const unsigned  int psram_adq5_ao_pins[]		= { GPIOP_9  };
static const unsigned  int psram_adq6_ao_pins[]		= { GPIOP_10 };
static const unsigned  int psram_adq7_ao_pins[]		= { GPIOP_11 };
static const unsigned  int psram_dqs_dm_ao_pins[]	= { GPIOP_12 };
static const unsigned  int clk12_24_p_pins[]		= { GPIOP_13 };

/* BANK P func2 */
static const unsigned  int eth_a_mdio_pins[]		= { GPIOP_0 };
static const unsigned  int eth_a_mdc_pins[]		= { GPIOP_1 };
static const unsigned  int eth_a_rgmii_rx_clk_pins[]	= { GPIOP_2 };
static const unsigned  int eth_a_rx_dv_pins[]		= { GPIOP_3 };
static const unsigned  int eth_a_rxd0_pins[]		= { GPIOP_4 };
static const unsigned  int eth_a_rxd1_pins[]		= { GPIOP_5 };
static const unsigned  int eth_a_rxd2_rgmii_pins[]	= { GPIOP_6 };
static const unsigned  int eth_a_rxd3_rgmii_pins[]	= { GPIOP_7 };
static const unsigned  int eth_a_rgmii_tx_clk_pins[]	= { GPIOP_8 };
static const unsigned  int eth_a_txen_pins[]		= { GPIOP_9 };
static const unsigned  int eth_a_txd0_pins[]		= { GPIOP_10 };
static const unsigned  int eth_a_txd1_pins[]		= { GPIOP_11 };
static const unsigned  int eth_a_txd2_rgmii_pins[]	= { GPIOP_12 };
static const unsigned  int eth_a_txd3_rgmii_pins[]	= { GPIOP_13 };

/* BANK P func3 */
static const unsigned  int i2c2_sck_p_pins[]		= { GPIOP_0 };
static const unsigned  int i2c2_sda_p_pins[]		= { GPIOP_1 };
static const unsigned  int i2c3_sck_p_pins[]		= { GPIOP_2 };
static const unsigned  int i2c3_sda_p_pins[]		= { GPIOP_3 };
static const unsigned  int i2c4_sck_p_pins[]		= { GPIOP_4 };
static const unsigned  int i2c4_sda_p_pins[]		= { GPIOP_5 };
static const unsigned  int i2c5_sck_p_pins[]		= { GPIOP_6 };
static const unsigned  int i2c5_sda_p_pins[]		= { GPIOP_7 };
static const unsigned  int i2c6_sck_p_pins[]		= { GPIOP_8 };
static const unsigned  int i2c6_sda_p_pins[]		= { GPIOP_9 };

/* BANK P func4 */
static const unsigned  int pwm_k_p_pins[]		= { GPIOP_0 };
static const unsigned  int pwm_l_p_pins[]		= { GPIOP_1 };
static const unsigned  int pwm_m_p_pins[]		= { GPIOP_2 };
static const unsigned  int pwm_n_p_pins[]		= { GPIOP_3 };
static const unsigned  int pwm_o_p_pins[]		= { GPIOP_4 };
static const unsigned  int pwm_p_p_pins[]		= { GPIOP_5 };
static const unsigned  int pwm_q_p_pins[]		= { GPIOP_6 };
static const unsigned  int pwm_r_p_pins[]		= { GPIOP_7 };
static const unsigned  int pwm_s_p_pins[]		= { GPIOP_8 };
static const unsigned  int pwm_t_p_pins[]		= { GPIOP_9 };
static const unsigned  int pwm_ao_a_p_pins[]		= { GPIOP_12 };
static const unsigned  int pwm_ao_b_p_pins[]		= { GPIOP_13 };

/* BANK P func5 */
static const unsigned  int spi0_ao_sck_p_pins[]		= { GPIOP_0 };
static const unsigned  int spi0_ao_mosi_p_pins[]	= { GPIOP_1 };
static const unsigned  int spi0_ao_miso_p_pins[]	= { GPIOP_2 };
static const unsigned  int spi0_ao_ss0_p_pins[]		= { GPIOP_3 };
static const unsigned  int spi1_sck_p_pins[]		= { GPIOP_4 };
static const unsigned  int spi1_mosi_p_pins[]		= { GPIOP_5 };
static const unsigned  int spi1_miso_p_pins[]		= { GPIOP_6 };
static const unsigned  int spi1_ss0_f_pins[]		= { GPIOP_7 };
static const unsigned  int uart_f_tx_p_pins[]		= { GPIOP_8 };
static const unsigned  int uart_f_rx_p_pins[]		= { GPIOP_9  };
static const unsigned  int uart_e_tx_p_pins[]		= { GPIOP_10 };
static const unsigned  int uart_e_rx_p_pins[]		= { GPIOP_11 };
static const unsigned  int uart_e_cts_p_pins[]		= { GPIOP_12 };
static const unsigned  int uart_e_rts_p_pins[]		= { GPIOP_13 };

/* BANK P func6 */
static const unsigned  int uart_ao_a_tx_p_pins[]	= { GPIOP_0 };
static const unsigned  int uart_ao_a_rx_p_pins[]	= { GPIOP_1 };
static const unsigned  int uart_ao_a_cts_p_pins[]	= { GPIOP_2 };
static const unsigned  int uart_ao_a_rts_p_pins[]	= { GPIOP_3 };
static const unsigned  int pdm_ao_din3_p_pins[]		= { GPIOP_9  };
static const unsigned  int pdm_ao_din2_p_pins[]		= { GPIOP_10 };
static const unsigned  int pdm_ao_din1_p_pins[]		= { GPIOP_11 };
static const unsigned  int pdm_ao_din0_p_pins[]		= { GPIOP_12 };
static const unsigned  int pdm_ao_dclk_p_pins[]		= { GPIOP_13 };

/* BANK P func7 */
static const unsigned  int gen_clk_p_pins[]		= { GPIOP_0 };

/* BANK AO func1 */
static const unsigned  int i2c0_ao_sck_pins[]		= { GPIOAO_0 };
static const unsigned  int i2c0_ao_sda_pins[]		= { GPIOAO_1 };
static const unsigned  int spi0_ao_sck_ao_pins[]	= { GPIOAO_6 };
static const unsigned  int spi0_ao_mosi_ao_pins[]	= { GPIOAO_7 };
static const unsigned  int spi0_ao_miso_ao_pins[]	= { GPIOAO_8 };
static const unsigned  int spi0_ao_ss0_ao_pins[]	= { GPIOAO_9 };
static const unsigned  int mic_mute_en_ao_pins[]	= { GPIOAO_10 };
static const unsigned  int mic_mute_key_ao_pins[]	= { GPIOAO_11 };
static const unsigned  int pwr_up_ao_pins[]		= { GPIOAO_15 };
static const unsigned  int str_en_ao_pins[]		= { GPIOAO_16 };
static const unsigned  int rtc32k_out_ao_pins[]		= { GPIOAO_19 };
static const unsigned  int clk_32k_in_ao_pins[]		= { GPIOAO_20 };
static const unsigned  int i2c2_sck_ao_pins[]		= { GPIOAO_21 };
static const unsigned  int i2c2_sda_ao_pins[]		= { GPIOAO_22 };

/* BANK AO func2 */
static const unsigned  int pdm_ao_din3_ao_pins[]	= { GPIOAO_1 };
static const unsigned  int pdm_ao_din2_ao_pins[]	= { GPIOAO_2 };
static const unsigned  int pdm_ao_din1_ao_pins[]	= { GPIOAO_3 };
static const unsigned  int pdm_ao_din0_ao_pins[]	= { GPIOAO_4 };
static const unsigned  int pdm_ao_dclk_ao_pins[]	= { GPIOAO_5 };
static const unsigned  int i2c1_ao_sck_ao6_pins[]	= { GPIOAO_6 };
static const unsigned  int i2c1_ao_sda_ao7_pins[]	= { GPIOAO_7 };
static const unsigned  int i2c1_ao_sck_ao15_pins[]	= { GPIOAO_15 };
static const unsigned  int i2c1_ao_sda_ao16_pins[]	= { GPIOAO_16 };
static const unsigned  int wd_rsto_ao_pins[]		= { GPIOAO_17 };
static const unsigned  int pmic_sleep_ao_pins[]		= { GPIOAO_18 };

/* BANK AO func3 */
static const unsigned  int uart_ao_a_cts_ao_pins[]	= { GPIOAO_6 };
static const unsigned  int uart_ao_a_rts_ao_pins[]	= { GPIOAO_7 };
static const unsigned  int uart_ao_a_tx_ao_pins[]	= { GPIOAO_8 };
static const unsigned  int uart_ao_a_rx_ao_pins[]	= { GPIOAO_9 };
static const unsigned  int clk12_24_ao_pins[]		= { GPIOAO_19 };

/* BANK AO func4 */
static const unsigned  int pwm_m_ao_pins[]		= { GPIOAO_4 };
static const unsigned  int pwm_n_ao_pins[]		= { GPIOAO_5 };
static const unsigned  int pwm_o_ao_pins[]		= { GPIOAO_6 };
static const unsigned  int pwm_p_ao_pins[]		= { GPIOAO_7 };
static const unsigned  int pwm_q_ao_pins[]		= { GPIOAO_8 };
static const unsigned  int pwm_r_ao_pins[]		= { GPIOAO_9 };
static const unsigned  int pwm_ao_b_ao10_pins[]		= { GPIOAO_10 };
static const unsigned  int pwm_s_ao_pins[]		= { GPIOAO_11 };

/* BANK AO func5 */
static const unsigned  int pwm_ao_b_ao7_pins[]		= { GPIOAO_7 };

/* BANK AO func6 */
static const unsigned  int pio0_pins[]			= { GPIOAO_0 };
static const unsigned  int pio1_pins[]			= { GPIOAO_1 };
static const unsigned  int pio2_pins[]			= { GPIOAO_2 };
static const unsigned  int pio3_pins[]			= { GPIOAO_3 };
static const unsigned  int pio4_pins[]			= { GPIOAO_4 };
static const unsigned  int pio5_pins[]			= { GPIOAO_5 };
static const unsigned  int pio6_pins[]			= { GPIOAO_6 };
static const unsigned  int pio7_pins[]			= { GPIOAO_7 };
static const unsigned  int pio8_pins[]			= { GPIOAO_8 };
static const unsigned  int pio9_pins[]			= { GPIOAO_9 };
static const unsigned  int pio10_pins[]			= { GPIOAO_10 };
static const unsigned  int pio11_pins[]			= { GPIOAO_11 };
static const unsigned  int pio12_pins[]			= { GPIOAO_12 };
static const unsigned  int pio13_pins[]			= { GPIOAO_13 };
static const unsigned  int pio14_pins[]			= { GPIOAO_14 };
static const unsigned  int pio15_pins[]			= { GPIOAO_15 };
static const unsigned  int pio16_pins[]			= { GPIOAO_16 };
static const unsigned  int pio17_pins[]			= { GPIOAO_17 };
static const unsigned  int pio18_pins[]			= { GPIOAO_18 };
static const unsigned  int pio19_pins[]			= { GPIOAO_19 };
static const unsigned  int pio20_pins[]			= { GPIOAO_20 };
static const unsigned  int pio21_pins[]			= { GPIOAO_21 };
static const unsigned  int pio22_pins[]			= { GPIOAO_22 };
static const unsigned  int pio23_pins[]			= { GPIOAO_23 };
static const unsigned  int pio24_pins[]			= { GPIOAO_24 };
static const unsigned  int pio25_pins[]			= { GPIOAO_25 };

/* BANK AO func7 */
static const unsigned  int gen_clk_ao_pins[]		= { GPIOAO_19 };

/* TESTN */
static const unsigned  int pwm_ao_a_testn_pins[]	= { GPIO_TEST_N };

static struct meson_pmx_group meson_c5_ao_groups[] __initdata = {
	GPIO_GROUP(GPIOF_0),
	GPIO_GROUP(GPIOF_1),
	GPIO_GROUP(GPIOF_2),
	GPIO_GROUP(GPIOF_3),
	GPIO_GROUP(GPIOF_4),
	GPIO_GROUP(GPIOF_5),
	GPIO_GROUP(GPIOF_6),
	GPIO_GROUP(GPIOP_0),
	GPIO_GROUP(GPIOP_1),
	GPIO_GROUP(GPIOP_2),
	GPIO_GROUP(GPIOP_3),
	GPIO_GROUP(GPIOP_4),
	GPIO_GROUP(GPIOP_5),
	GPIO_GROUP(GPIOP_6),
	GPIO_GROUP(GPIOP_7),
	GPIO_GROUP(GPIOP_8),
	GPIO_GROUP(GPIOP_9),
	GPIO_GROUP(GPIOP_10),
	GPIO_GROUP(GPIOP_11),
	GPIO_GROUP(GPIOP_12),
	GPIO_GROUP(GPIOP_13),
	GPIO_GROUP(GPIOAO_0),
	GPIO_GROUP(GPIOAO_1),
	GPIO_GROUP(GPIOAO_2),
	GPIO_GROUP(GPIOAO_3),
	GPIO_GROUP(GPIOAO_4),
	GPIO_GROUP(GPIOAO_5),
	GPIO_GROUP(GPIOAO_6),
	GPIO_GROUP(GPIOAO_7),
	GPIO_GROUP(GPIOAO_8),
	GPIO_GROUP(GPIOAO_9),
	GPIO_GROUP(GPIOAO_10),
	GPIO_GROUP(GPIOAO_11),
	GPIO_GROUP(GPIOAO_12),
	GPIO_GROUP(GPIOAO_13),
	GPIO_GROUP(GPIOAO_14),
	GPIO_GROUP(GPIOAO_15),
	GPIO_GROUP(GPIOAO_16),
	GPIO_GROUP(GPIOAO_17),
	GPIO_GROUP(GPIOAO_18),
	GPIO_GROUP(GPIOAO_19),
	GPIO_GROUP(GPIOAO_20),
	GPIO_GROUP(GPIOAO_21),
	GPIO_GROUP(GPIOAO_22),
	GPIO_GROUP(GPIOAO_23),
	GPIO_GROUP(GPIOAO_24),
	GPIO_GROUP(GPIOAO_25),

	/* BANK F func1 */
	GROUP(uart_ao_b_tx_f,		1),
	GROUP(uart_ao_b_rx_f,		1),
	GROUP(pwm_ao_a_hiz,		1),
	GROUP(pwm_ao_b_hiz,		1),
	GROUP(pwm_k_hiz,		1),
	GROUP(pcie_preset_f,		1),
	GROUP(pcieck_req_f,		1),

	/* BANK F func2 */
	GROUP(pwm_ao_a_f,		2),
	GROUP(pwm_ao_b_f,		2),
	GROUP(pwm_k_f,			2),
	GROUP(pwm_l_f,			2),
	GROUP(pwm_m_f,			2),

	/* BANK F func3 */
	GROUP(i2c1_ao_sck_f,		3),
	GROUP(i2c1_ao_sda_f,		3),
	GROUP(i3c0_ao_sck,		3),
	GROUP(i3c0_ao_sda,		3),
	GROUP(mclk12_24,		3),

	/* BANK F func4 */
	GROUP(spi0_ao_sck_f,		4),
	GROUP(spi0_ao_mosi_f,		4),
	GROUP(spi0_ao_miso_f,		4),
	GROUP(spi0_ao_ss0_f,		4),
	GROUP(clk_32k_ao_in,		4),

	/* BANK F func5 */
	GROUP(uart_ao_a_tx_f,		5),
	GROUP(uart_ao_a_rx_f,		5),
	GROUP(can_a_tx,			5),
	GROUP(can_a_rx,			5),

	/* BANK F func6 */
	GROUP(tdm_d3_f,			6),
	GROUP(tdm_d2_f,			6),
	GROUP(tdm_sclk1_f,		6),
	GROUP(tdm_fs1_f,		6),
	GROUP(mclk1_f,			6),

	/* BANK F func7 */
	GROUP(ir_ao_out,		7),
	GROUP(ir_ao_in,			7),
	GROUP(gen_clk_f,		7),

	/* BANK P func1 */
	GROUP(psram_clkn_ao,		1),
	GROUP(psram_clkp_ao,		1),
	GROUP(psram_ce_n_ao,		1),
	GROUP(psram_rst_n_ao,		1),
	GROUP(psram_adq0_ao,		1),
	GROUP(psram_adq1_ao,		1),
	GROUP(psram_adq2_ao,		1),
	GROUP(psram_adq3_ao,		1),
	GROUP(psram_adq4_ao,		1),
	GROUP(psram_adq5_ao,		1),
	GROUP(psram_adq6_ao,		1),
	GROUP(psram_adq7_ao,		1),
	GROUP(psram_dqs_dm_ao,		1),
	GROUP(clk12_24_p,		1),

	/* BANK P func2 */
	GROUP(eth_a_mdio,		2),
	GROUP(eth_a_mdc,		2),
	GROUP(eth_a_rgmii_rx_clk,	2),
	GROUP(eth_a_rx_dv,		2),
	GROUP(eth_a_rxd0,		2),
	GROUP(eth_a_rxd1,		2),
	GROUP(eth_a_rxd2_rgmii,		2),
	GROUP(eth_a_rxd3_rgmii,		2),
	GROUP(eth_a_rgmii_tx_clk,	2),
	GROUP(eth_a_txen,		2),
	GROUP(eth_a_txd0,		2),
	GROUP(eth_a_txd1,		2),
	GROUP(eth_a_txd2_rgmii,		2),
	GROUP(eth_a_txd3_rgmii,		2),

	/* BANK P func3 */
	GROUP(i2c2_sck_p,		3),
	GROUP(i2c2_sda_p,		3),
	GROUP(i2c3_sck_p,		3),
	GROUP(i2c3_sda_p,		3),
	GROUP(i2c4_sck_p,		3),
	GROUP(i2c4_sda_p,		3),
	GROUP(i2c5_sck_p,		3),
	GROUP(i2c5_sda_p,		3),
	GROUP(i2c6_sck_p,		3),
	GROUP(i2c6_sda_p,		3),

	/* BANK P func4 */
	GROUP(pwm_k_p,			4),
	GROUP(pwm_l_p,			4),
	GROUP(pwm_m_p,			4),
	GROUP(pwm_n_p,			4),
	GROUP(pwm_o_p,			4),
	GROUP(pwm_p_p,			4),
	GROUP(pwm_q_p,			4),
	GROUP(pwm_r_p,			4),
	GROUP(pwm_s_p,			4),
	GROUP(pwm_t_p,			4),
	GROUP(pwm_ao_a_p,		4),
	GROUP(pwm_ao_b_p,		4),

	/* BANK P func5 */
	GROUP(spi0_ao_sck_p,		5),
	GROUP(spi0_ao_mosi_p,		5),
	GROUP(spi0_ao_miso_p,		5),
	GROUP(spi0_ao_ss0_p,		5),
	GROUP(spi1_sck_p,		5),
	GROUP(spi1_mosi_p,		5),
	GROUP(spi1_miso_p,		5),
	GROUP(spi1_ss0_f,		5),
	GROUP(uart_f_tx_p,		5),
	GROUP(uart_f_rx_p,		5),
	GROUP(uart_e_tx_p,		5),
	GROUP(uart_e_rx_p,		5),
	GROUP(uart_e_cts_p,		5),
	GROUP(uart_e_rts_p,		5),

	/* BANK P func6 */
	GROUP(uart_ao_a_tx_p,		6),
	GROUP(uart_ao_a_rx_p,		6),
	GROUP(uart_ao_a_cts_p,		6),
	GROUP(uart_ao_a_rts_p,		6),
	GROUP(pdm_ao_din3_p,		6),
	GROUP(pdm_ao_din2_p,		6),
	GROUP(pdm_ao_din1_p,		6),
	GROUP(pdm_ao_din0_p,		6),
	GROUP(pdm_ao_dclk_p,		6),

	/* BANK P func7 */
	GROUP(gen_clk_p,		7),

	/* BANK AO func1 */
	GROUP(i2c0_ao_sck,		1),
	GROUP(i2c0_ao_sda,		1),
	GROUP(spi0_ao_sck_ao,		1),
	GROUP(spi0_ao_mosi_ao,		1),
	GROUP(spi0_ao_miso_ao,		1),
	GROUP(spi0_ao_ss0_ao,		1),
	GROUP(mic_mute_en_ao,		1),
	GROUP(mic_mute_key_ao,		1),
	GROUP(pwr_up_ao,		1),
	GROUP(str_en_ao,		1),
	GROUP(rtc32k_out_ao,		1),
	GROUP(clk_32k_in_ao,		1),
	GROUP(i2c2_sck_ao,		1),
	GROUP(i2c2_sda_ao,		1),

	/* BANK AO func2 */
	GROUP(pdm_ao_din3_ao,		2),
	GROUP(pdm_ao_din2_ao,		2),
	GROUP(pdm_ao_din1_ao,		2),
	GROUP(pdm_ao_din0_ao,		2),
	GROUP(pdm_ao_dclk_ao,		2),
	GROUP(i2c1_ao_sck_ao6,		2),
	GROUP(i2c1_ao_sda_ao7,		2),
	GROUP(i2c1_ao_sck_ao15,		2),
	GROUP(i2c1_ao_sda_ao16,		2),
	GROUP(wd_rsto_ao,		2),
	GROUP(pmic_sleep_ao,		2),

	/* BANK AO func3 */
	GROUP(uart_ao_a_cts_ao,		3),
	GROUP(uart_ao_a_rts_ao,		3),
	GROUP(uart_ao_a_tx_ao,		3),
	GROUP(uart_ao_a_rx_ao,		3),
	GROUP(clk12_24_ao,		3),

	/* BANK AO func4 */
	GROUP(pwm_m_ao,			4),
	GROUP(pwm_n_ao,			4),
	GROUP(pwm_o_ao,			4),
	GROUP(pwm_p_ao,			4),
	GROUP(pwm_q_ao,			4),
	GROUP(pwm_r_ao,			4),
	GROUP(pwm_ao_b_ao10,		4),
	GROUP(pwm_s_ao,			4),

	/* BANK AO func5 */
	GROUP(pwm_ao_b_ao7,		5),

	/* BANK AO func6 */
	GROUP(pio0,			6),
	GROUP(pio1,			6),
	GROUP(pio2,			6),
	GROUP(pio3,			6),
	GROUP(pio4,			6),
	GROUP(pio5,			6),
	GROUP(pio6,			6),
	GROUP(pio7,			6),
	GROUP(pio8,			6),
	GROUP(pio9,			6),
	GROUP(pio10,			6),
	GROUP(pio11,			6),
	GROUP(pio12,			6),
	GROUP(pio13,			6),
	GROUP(pio14,			6),
	GROUP(pio15,			6),
	GROUP(pio16,			6),
	GROUP(pio17,			6),
	GROUP(pio18,			6),
	GROUP(pio19,			6),
	GROUP(pio20,			6),
	GROUP(pio21,			6),
	GROUP(pio22,			6),
	GROUP(pio23,			6),
	GROUP(pio24,			6),
	GROUP(pio25,			6),

	/* BANK AO func7 */
	GROUP(gen_clk_ao,		7),

	/*TESTN */
	GROUP(pwm_ao_a_testn,		1)
};

static const char * const gpio_ao_groups[] = {
	"GPIOF_0", "GPIOF_1", "GPIOF_2", "GPIOF_3", "GPIOF_4",
	"GPIOF_5", "GPIOF_6", "GPIOP_0", "GPIOP_1", "GPIOP_2",
	"GPIOP_3", "GPIOP_4", "GPIOP_5", "GPIOP_6", "GPIOP_7",
	"GPIOP_8", "GPIOP_9", "GPIOP_10", "GPIOP_11", "GPIOP_12",
	"GPIOP_13", "GPIOAO_0", "GPIOAO_1", "GPIOAO_2", "GPIOAO_3",
	"GPIOAO_4", "GPIOAO_5", "GPIOAO_6", "GPIOAO_7", "GPIOAO_8",
	"GPIOAO_9", "GPIOAO_10", "GPIOAO_11", "GPIOAO_12", "GPIOAO_13", "GPIOAO_14",
	"GPIOAO_15", "GPIOAO_16", "GPIOAO_17", "GPIOAO_18", "GPIOAO_19", "GPIOAO_20",
	"GPIOAO_21", "GPIOAO_22", "GPIOAO_23", "GPIOAO_24", "GPIOAO_25",

	"GPIO_TEST_N"
};

static const char * const pwm_ao_a_groups[] = {
	"pwm_ao_a_testn", "pwm_ao_a_f", "pwm_ao_a_p"
};

static const char * const pwm_ao_b_groups[] = {
	"pwm_ao_b_f", "pwm_ao_b_p", "pwm_ao_b_ao10", "pwm_ao_b_ao7"
};

static const char * const pwm_ao_k_groups[] = {
	"pwm_k_f", "pwm_k_p"
};

static const char * const pwm_ao_l_groups[] = {
	"pwm_l_f", "pwm_l_p",
};

static const char * const pwm_ao_m_groups[] = {
	"pwm_m_f", "pwm_m_p", "pwm_m_ao"
};

static const char * const pwm_ao_n_groups[] = {
	"pwm_n_p", "pwm_n_ao"
};

static const char * const pwm_ao_o_groups[] = {
	"pwm_o_p", "pwm_o_ao"
};

static const char * const pwm_ao_p_groups[] = {
	"pwm_p_p", "pwm_p_ao"
};

static const char * const pwm_ao_q_groups[] = {
	"pwm_q_p", "pwm_q_ao"
};

static const char * const pwm_ao_r_groups[] = {
	"pwm_r_p", "pwm_r_ao"
};

static const char * const pwm_ao_s_groups[] = {
	"pwm_s_p", "pwm_s_ao",
};

static const char * const pwm_ao_t_groups[] = {
	"pwm_t_p",
};

static const char * const pwm_ao_a_hiz_groups[] = {
	"pwm_ao_a_hiz",
};

static const char * const pwm_ao_b_hiz_groups[] = {
	"pwm_ao_b_hiz",
};

static const char * const pwm_ao_k_hiz_groups[] = {
	"pwm_k_hiz",
};

static const char * const i2c0_ao_groups[] = {
	"i2c0_ao_sck", "i2c0_ao_sda"
};

static const char * const i2c1_ao_groups[] = {
	"i2c1_ao_sck_f", "i2c1_ao_sda_f",
	"i2c1_ao_sck_ao6", "i2c1_ao_sda_ao7",
	"i2c1_ao_sck_ao15", "i2c1_ao_sda_ao16"
};

static const char * const i2c2_ao_groups[] = {
	"i2c2_sck_p", "i2c2_sda_p",
	"i2c2_sck_ao", "i2c2_sda_ao"
};

static const char * const i2c3_ao_groups[] = {
	"i2c3_sck_p", "i2c3_sda_p"
};

static const char * const i2c4_ao_groups[] = {
	"i2c4_sck_p", "i2c4_sda_p"
};

static const char * const i2c5_ao_groups[] = {
	"i2c5_sck_p", "i2c5_sda_p"
};

static const char * const i2c_ao_groups[] = {
	"i2c_sck_p", "i2c_sda_p"
};

static const char * const i3c0_ao_groups[] = {
	"i3c0_ao_sck", "i3c0_ao_sda"
};

static const char * const mclk12_24_groups[] = {
	"mclk12_24"
};

static const char * const spi0_ao_groups[] = {
	"spi0_ao_sck_f", "spi0_ao_mosi_f",
	"spi0_ao_miso_f", "spi0_ao_ss0_f",

	"spi0_ao_sck_p", "spi0_ao_mosi_p",
	"spi0_ao_miso_p", "spi0_ao_ss0_p",

	"spi0_ao_sck_ao", "spi0_ao_mosi_ao",
	"spi0_ao_miso_ao", "spi0_ao_ss0_ao"
};

static const char * const spi1_ao_groups[] = {
	"spi1_sck_p", "spi1_mosi_p",
	"spi1_miso_p", "spi1_ss0_f"
};

static const char * const clk_32k_ao_in_groups[] = {
	"clk_32k_ao_in"
};

static const char * const uart_ao_a_groups[] = {
	"uart_ao_a_tx_f", "uart_ao_a_rx_f",
	"uart_ao_a_tx_p", "uart_ao_a_rx_p",
	"uart_ao_a_cts_p", "uart_ao_a_rts_p",

	"uart_ao_a_cts_ao", "uart_ao_a_rts_ao",
	"uart_ao_a_tx_ao", "uart_ao_a_rx_ao"
};

static const char * const uart_ao_b_groups[] = {
	"uart_ao_b_tx_f", "uart_ao_b_rx_f"
};

static const char * const uart_ao_f_groups[] = {
	"uart_f_tx_p", "uart_f_rx_p"
};

static const char * const uart_ao_e_groups[] = {
	"uart_e_tx_p", "uart_e_rx_p",
	"uart_f_tx_p", "uart_f_rx_p"
};

static const char * const pcie_ao_groups[] = {
	"pcie_preset_f", "pcieck_req_f"
};

static const char * const can_a_groups[] = {
	"can_a_tx", "can_a_rx"
};

static const char * const tdm_ao_groups[] = {
	"tdm_d3_f", "tdm_d2_f", "tdm_sclk1_f", "tdm_fs1_f"
};

static const char * const mclk1_ao_groups[] = {
	"mclk1_f"
};

static const char * const ir_ao_out_groups[] = {
	"ir_ao_out"
};

static const char * const ir_ao_in_groups[] = {
	"ir_ao_in"
};

static const char * const gen_clk_ao_groups[] = {
	"gen_clk_f", "gen_clk_p", "gen_clk_ao"
};

static const char * const psram_ao_groups[] = {
	"psram_clkn_ao", "psram_clkp_ao",
	"psram_ce_n_ao", "psram_rst_n_ao",
	"psram_adq0_ao", "psram_adq1_ao",
	"psram_adq2_ao", "psram_adq3_ao",
	"psram_adq4_ao", "psram_adq5_ao",
	"psram_adq6_ao", "psram_adq7_ao",
	"psram_dqs_dm_ao"
};

static const char * const eth_groups[] = {
	"eth_a_mdio", "eth_a_mdc", "eth_a_rgmii_rx_clk", "eth_a_rx_dv",
	"eth_a_rxd0", "eth_a_rxd1", "eth_a_rxd2_rgmii",
	"eth_a_rxd3_rgmii", "eth_a_rgmii_tx_clk", "eth_a_txen",
	"eth_a_txd0", "eth_a_txd1", "eth_a_txd2_rgmii",
	"eth_a_txd3_rgmii"
};

static const char * const pdm_ao_groups[] = {
	"pdm_ao_din3_p", "pdm_ao_din2_p",
	"pdm_ao_din1_p", "pdm_ao_din0_p",
	"pdm_ao_dclk_p",

	"pdm_ao_din3_ao", "pdm_ao_din2_ao",
	"pdm_ao_din1_ao", "pdm_ao_din0_ao",
	"pdm_ao_dclk_ao"
};

static const char * const mic_mute_ao_groups[] = {
	"mic_mute_en_ao", "mic_mute_key_ao"
};

static const char * const pwr_up_ao_groups[] = {
	"pwr_up_ao",
};

static const char * const rtc32k_out_ao_groups[] = {
	"rtc32k_out_ao",
};

static const char * const clk_32k_in_ao_groups[] = {
	"clk_32k_in_ao",
};

static const char * const wd_rsto_ao_groups[] = {
	"wd_rsto_ao"
};

static const char * const pmic_sleep_ao_groups[] = {
	"pmic_sleep_ao"
};

static const char * const clk12_24_ao_groups[] = {
	"clk12_24_ao", "clk12_24_p"
};

static const char * const pio_ao_groups[] = {
	"pio0", "pio1", "pio2", "pio3", "pio4",
	"pio5", "pio6", "pio7", "pio8", "pio9",
	"pio10", "pio11", "pio12", "pio13", "pio14",
	"pio15", "pio16", "pio17", "pio18", "pio19",
	"pio20", "pio21", "pio22", "pio23",
	"pio24", "pio25"
};

static const char * const pwm_ao_testn_groups[] = {
	"pwm_ao_a_testn"
};

static struct meson_pmx_func meson_c5_ao_functions[] __initdata = {
	FUNCTION(gpio_ao),
	FUNCTION(pwm_ao_a),
	FUNCTION(pwm_ao_b),
	FUNCTION(pwm_ao_k),
	FUNCTION(pwm_ao_l),
	FUNCTION(pwm_ao_m),
	FUNCTION(pwm_ao_n),
	FUNCTION(pwm_ao_o),
	FUNCTION(pwm_ao_p),
	FUNCTION(pwm_ao_q),
	FUNCTION(pwm_ao_r),
	FUNCTION(pwm_ao_s),
	FUNCTION(pwm_ao_t),
	FUNCTION(pwm_ao_a_hiz),
	FUNCTION(pwm_ao_b_hiz),
	FUNCTION(pwm_ao_k_hiz),
	FUNCTION(i2c0_ao),
	FUNCTION(i2c1_ao),
	FUNCTION(i2c2_ao),
	FUNCTION(i2c3_ao),
	FUNCTION(i2c4_ao),
	FUNCTION(i2c5_ao),
	FUNCTION(i2c_ao),
	FUNCTION(i3c0_ao),
	FUNCTION(mclk12_24),
	FUNCTION(spi0_ao),
	FUNCTION(spi1_ao),
	FUNCTION(clk_32k_ao_in),
	FUNCTION(uart_ao_a),
	FUNCTION(uart_ao_b),
	FUNCTION(uart_ao_f),
	FUNCTION(uart_ao_e),
	FUNCTION(pcie_ao),
	FUNCTION(can_a),
	FUNCTION(tdm_ao),
	FUNCTION(mclk1_ao),
	FUNCTION(ir_ao_out),
	FUNCTION(ir_ao_in),
	FUNCTION(gen_clk_ao),
	FUNCTION(psram_ao),
	FUNCTION(clk12_24_ao),
	FUNCTION(eth),
	FUNCTION(pdm_ao),
	FUNCTION(mic_mute_ao),
	FUNCTION(pwr_up_ao),
	FUNCTION(rtc32k_out_ao),
	FUNCTION(clk_32k_in_ao),
	FUNCTION(wd_rsto_ao),
	FUNCTION(pmic_sleep_ao),
	FUNCTION(pio_ao),
	FUNCTION(pwm_ao_testn)
};

static const struct pinctrl_pin_desc meson_c5_periphs_pins[] = {
	MESON_PIN(GPIOB_0),
	MESON_PIN(GPIOB_1),
	MESON_PIN(GPIOB_2),
	MESON_PIN(GPIOB_3),
	MESON_PIN(GPIOB_4),
	MESON_PIN(GPIOB_5),
	MESON_PIN(GPIOB_6),
	MESON_PIN(GPIOB_7),
	MESON_PIN(GPIOB_8),
	MESON_PIN(GPIOB_9),
	MESON_PIN(GPIOB_10),
	MESON_PIN(GPIOB_11),
	MESON_PIN(GPIOB_12),
	MESON_PIN(GPIOB_13),
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
	MESON_PIN(GPIOC_6),
	MESON_PIN(GPIOC_7),
	MESON_PIN(GPIOD_0),
	MESON_PIN(GPIOD_1),
	MESON_PIN(GPIOD_2),
	MESON_PIN(GPIOD_3),
	MESON_PIN(GPIOD_4),
	MESON_PIN(GPIOD_5),
	MESON_PIN(GPIOD_6),
	MESON_PIN(GPIOD_7),
	MESON_PIN(GPIOD_8),
	MESON_PIN(GPIOD_9),
	MESON_PIN(GPIOD_10),
	MESON_PIN(GPIOD_11),
	MESON_PIN(GPIOE_0),
	MESON_PIN(GPIOE_1),
	MESON_PIN(GPIOE_2),
	MESON_PIN(GPIOE_3),
	MESON_PIN(GPIOE_4),
	MESON_PIN(GPIOE_5),
	MESON_PIN(GPIOE_6),
	MESON_PIN(GPIOE_7),
	MESON_PIN(GPIOX_0),
	MESON_PIN(GPIOX_1),
	MESON_PIN(GPIOX_2),
	MESON_PIN(GPIOX_3),
	MESON_PIN(GPIOX_4),
	MESON_PIN(GPIOX_5),
	MESON_PIN(GPIOX_6),
	MESON_PIN(GPIOX_7),
	MESON_PIN(GPIOX_8),
	MESON_PIN(GPIOX_9),
	MESON_PIN(GPIOX_10),
	MESON_PIN(GPIOX_11),
	MESON_PIN(GPIOX_12),
	MESON_PIN(GPIOX_13),
	MESON_PIN(GPIOA_0),
	MESON_PIN(GPIOA_1),
	MESON_PIN(GPIOA_2),
	MESON_PIN(GPIOA_3),
	MESON_PIN(GPIOA_4),
	MESON_PIN(GPIOA_5),
	MESON_PIN(GPIOA_6),
	MESON_PIN(GPIOADC_0),
	MESON_PIN(GPIOADC_1),
	MESON_PIN(GPIOADC_2),
	MESON_PIN(GPIOADC_3),
	MESON_PIN(GPIOADC_4),
	MESON_PIN(GPIOADC_5),
	MESON_PIN(GPIOADC_6),
	MESON_PIN(GPIOADC_7),
	MESON_PIN(GPIODSI_0),
	MESON_PIN(GPIODSI_1),
	MESON_PIN(GPIODSI_2),
	MESON_PIN(GPIODSI_3),
	MESON_PIN(GPIODSI_4),
	MESON_PIN(GPIODSI_5),
	MESON_PIN(GPIODSI_6),
	MESON_PIN(GPIODSI_7),
	MESON_PIN(GPIODSI_8),
	MESON_PIN(GPIODSI_9),
	MESON_PIN(GPIOCSI_0),
	MESON_PIN(GPIOCSI_1),
	MESON_PIN(GPIOCSI_2),
	MESON_PIN(GPIOCSI_3),
	MESON_PIN(GPIOCSI_4),
	MESON_PIN(GPIOCSI_5),
	MESON_PIN(GPIOCSI_6),
	MESON_PIN(GPIOCSI_7),
	MESON_PIN(GPIOCSI_8),
	MESON_PIN(GPIOCSI_9),
	MESON_PIN(GPIOCSI_10),
	MESON_PIN(GPIOCSI_11),
	MESON_PIN(GPIOCSI_12),
	MESON_PIN(GPIOCSI_13),
	MESON_PIN(GPIOCSI_14),
	MESON_PIN(GPIOCSI_15),
	MESON_PIN(GPIOCSI_16),
	MESON_PIN(GPIOCSI_17),
	MESON_PIN(GPIOCSI_18),
	MESON_PIN(GPIOCSI_19),
	MESON_PIN(GPIOCSI_20),
	MESON_PIN(GPIOCSI_21),
	MESON_PIN(GPIOCSI_22),
	MESON_PIN(GPIOCSI_23),
	MESON_PIN(GPIOMCLK_0),
	MESON_PIN(GPIOMCLK_1),
	MESON_PIN(GPIOMCLK_2),
	MESON_PIN(GPIOMCLK_3),
};

/* Bank B func1 */
static const unsigned  int emmc_d0_pins[]		= { GPIOB_0 };
static const unsigned  int emmc_d1_pins[]		= { GPIOB_1 };
static const unsigned  int emmc_d2_pins[]		= { GPIOB_2 };
static const unsigned  int emmc_d3_pins[]		= { GPIOB_3 };
static const unsigned  int emmc_d4_pins[]		= { GPIOB_4 };
static const unsigned  int emmc_d5_pins[]		= { GPIOB_5 };
static const unsigned  int emmc_d6_pins[]		= { GPIOB_6 };
static const unsigned  int emmc_d7_pins[]		= { GPIOB_7 };
static const unsigned  int emmc_clk_pins[]		= { GPIOB_8 };
static const unsigned  int emmc_cmd_pins[]		= { GPIOB_10 };
static const unsigned  int emmc_ds_pins[]		= { GPIOB_11 };
static const unsigned  int clk12_24_b_pins[]		= { GPIOB_12 };

/* Bank B func2 */
static const unsigned  int spi1_ss0_b_pins[]		= { GPIOB_4 };
static const unsigned  int spi1_sclk_b_pins[]		= { GPIOB_5 };
static const unsigned  int spi1_mosi_b_pins[]		= { GPIOB_6 };
static const unsigned  int spi1_miso_b_pins[]		= { GPIOB_7 };
static const unsigned  int spi1_ss1_b_pins[]		= { GPIOB_8 };
static const unsigned  int spi1_ss2_b_pins[]		= { GPIOB_9 };

/* Bank B func3 */
static const unsigned  int spinf_mo_d0_pins[]		= { GPIOB_0 };
static const unsigned  int spinf_mi_d1_pins[]		= { GPIOB_1 };
static const unsigned  int spinf_wp_d2_pins[]		= { GPIOB_2 };
static const unsigned  int spinf_hold_d3_pins[]		= { GPIOB_3 };
static const unsigned  int spinf_d4_pins[]		= { GPIOB_4 };
static const unsigned  int spinf_d5_pins[]		= { GPIOB_5 };
static const unsigned  int spinf_d6_pins[]		= { GPIOB_6 };
static const unsigned  int spinf_d7_pins[]		= { GPIOB_7 };
static const unsigned  int spinf_clk_pins[]		= { GPIOB_10 };
static const unsigned  int spinf_cs0_pins[]		= { GPIOB_13 };

/* Bank B func4 */
static const unsigned  int pdm_din3_b_pins[]		= { GPIOB_0 };
static const unsigned  int pdm_din2_b_pins[]		= { GPIOB_1 };
static const unsigned  int pdm_din1_b_pins[]		= { GPIOB_2 };
static const unsigned  int pdm_din0_b_pins[]		= { GPIOB_3 };
static const unsigned  int pdm_clk_b_pins[]		= { GPIOB_4 };
static const unsigned  int i2c6_sck_b_pins[]		= { GPIOB_11 };
static const unsigned  int i2c6_sda_b_pins[]		= { GPIOB_12 };

/* Bank B func5 */
static const unsigned  int pwm_o_b_pins[]		= { GPIOB_4 };
static const unsigned  int pwm_p_b_pins[]		= { GPIOB_5 };
static const unsigned  int pwm_q_b_pins[]		= { GPIOB_6 };
static const unsigned  int pwm_r_b_pins[]		= { GPIOB_7 };
static const unsigned  int pwm_s_b_pins[]		= { GPIOB_8 };
static const unsigned  int pwm_t_b_pins[]		= { GPIOB_9 };

/* Bank B func6 */
static const unsigned  int uart_c_cts_b_pins[]		= { GPIOB_4 };
static const unsigned  int uart_c_rts_b_pins[]		= { GPIOB_5 };
static const unsigned  int uart_c_tx_b_pins[]		= { GPIOB_6 };
static const unsigned  int uart_c_rx_b_pins[]		= { GPIOB_7 };

/* Bank B func7 */
static const unsigned  int i2c5_sck_b_pins[]		= { GPIOB_5 };
static const unsigned  int i2c5_sda_b_pins[]		= { GPIOB_6 };

/* Bank C func1 */
static const unsigned  int sdcard_d0_pins[]		= { GPIOC_0 };
static const unsigned  int sdcard_d1_pins[]		= { GPIOC_1 };
static const unsigned  int sdcard_d2_pins[]		= { GPIOC_2 };
static const unsigned  int sdcard_d3_pins[]		= { GPIOC_3 };
static const unsigned  int sdcard_clk_pins[]		= { GPIOC_4 };
static const unsigned  int sdcard_cmd_pins[]		= { GPIOC_5 };
static const unsigned  int pcieck_reqn_c_pins[]		= { GPIOC_7 };

/* Bank C func2 */
static const unsigned  int jtag_b_tdo_pins[]		= { GPIOC_0 };
static const unsigned  int jtag_b_tdi_pins[]		= { GPIOC_1 };
static const unsigned  int uart_b_rx_pins[]		= { GPIOC_2 };
static const unsigned  int uart_b_tx_pins[]		= { GPIOC_3 };
static const unsigned  int jtag_b_clk_pins[]		= { GPIOC_4 };
static const unsigned  int jtag_b_tms_pins[]		= { GPIOC_5 };

/* Bank C func3 */
static const unsigned  int tdm_d3_c_pins[]		= { GPIOC_0 };
static const unsigned  int tdm_d2_c_pins[]		= { GPIOC_1 };
static const unsigned  int mclk1_c_pins[]		= { GPIOC_2 };
static const unsigned  int tdm_sclk1_c_pins[]		= { GPIOC_3 };
static const unsigned  int tdm_fs1_c_pins[]		= { GPIOC_4 };
static const unsigned  int tdm_d4_c_pins[]		= { GPIOC_5 };
static const unsigned  int tdm_d5_c_pins[]		= { GPIOC_6 };

/* Bank C func4 */
static const unsigned  int spi1_mosi_c_pins[]		= { GPIOC_0 };
static const unsigned  int spi1_miso_c_pins[]		= { GPIOC_1 };
static const unsigned  int spi1_sclk_c_pins[]		= { GPIOC_2 };
static const unsigned  int spi1_ss0_c_pins[]		= { GPIOC_3 };
static const unsigned  int spi1_ss1_c_pins[]		= { GPIOC_4 };
static const unsigned  int spi1_ss2_c_pins[]		= { GPIOC_5 };
static const unsigned  int clk12_24_c_pins[]		= { GPIOC_6 };

/* Bank C func5 */
static const unsigned  int pwm_s_c_pins[]		= { GPIOC_0 };
static const unsigned  int pwm_t_c_pins[]		= { GPIOC_1 };
static const unsigned  int pwm_m_c_pins[]		= { GPIOC_2 };
static const unsigned  int pwm_n_c_pins[]		= { GPIOC_3 };
static const unsigned  int pwm_o_c_pins[]		= { GPIOC_4 };
static const unsigned  int pwm_p_c_pins[]		= { GPIOC_5 };
static const unsigned  int pwm_q_c_pins[]		= { GPIOC_6 };

/* Bank C func6 */
static const unsigned  int uart_c_tx_c_pins[]		= { GPIOC_0 };
static const unsigned  int uart_c_rx_c_pins[]		= { GPIOC_1 };
static const unsigned  int uart_d_tx_c_pins[]		= { GPIOC_2 };
static const unsigned  int uart_d_rx_c_pins[]		= { GPIOC_3 };
static const unsigned  int i2c5_sck_c_pins[]		= { GPIOC_5 };
static const unsigned  int i2c5_sda_c_pins[]		= { GPIOC_6 };

/* Bank C func7 */
static const unsigned  int gen_clk_c_pins[]		= { GPIOC_6 };

/* Bank D func1 */
static const unsigned  int pwm_t_d_pins[]		= { GPIOD_2 };
static const unsigned  int pwm_k_d_pins[]		= { GPIOD_3 };
static const unsigned  int pwm_l_d_pins[]		= { GPIOD_4 };
static const unsigned  int pwm_m_d_pins[]		= { GPIOD_5 };
static const unsigned  int pwm_n_d_pins[]		= { GPIOD_6 };
static const unsigned  int pwm_o_d_pins[]		= { GPIOD_7 };
static const unsigned  int pwm_p_d_pins[]		= { GPIOD_8 };
static const unsigned  int pwm_q_d_pins[]		= { GPIOD_9 };
static const unsigned  int pwm_r_d_pins[]		= { GPIOD_10 };
static const unsigned  int pwm_s_d_pins[]		= { GPIOD_11 };

/* Bank D func2 */
static const unsigned  int spi2_mosi_d_pins[]		= { GPIOD_0 };
static const unsigned  int spi2_miso_d_pins[]		= { GPIOD_1 };
static const unsigned  int spi2_sclk_d_pins[]		= { GPIOD_2 };
static const unsigned  int spi2_ss0_d_pins[]		= { GPIOD_3 };
static const unsigned  int spi1_miso_d_pins[]		= { GPIOD_4 };
static const unsigned  int spi1_mosi_d_pins[]		= { GPIOD_5 };
static const unsigned  int spi1_sclk_d_pins[]		= { GPIOD_6 };
static const unsigned  int spi1_ss1_d_pins[]		= { GPIOD_7 };
static const unsigned  int can_b_tx_pins[]		= { GPIOD_8 };
static const unsigned  int can_b_rx_pins[]		= { GPIOD_9 };
static const unsigned  int pcie_preset_d_pins[]		= { GPIOD_10 };
static const unsigned  int pcieck_reqn_d_pins[]		= { GPIOD_11 };

/* Bank D func3 */
static const unsigned  int tdm_d3_d_pins[]		= { GPIOD_0 };
static const unsigned  int tdm_d2_d_pins[]		= { GPIOD_1 };
static const unsigned  int mclk0_pins[]			= { GPIOD_2 };
static const unsigned  int tdm_sclk0_pins[]		= { GPIOD_3 };
static const unsigned  int tdm_fs0_pins[]		= { GPIOD_4 };
static const unsigned  int tdm_d1_d_pins[]		= { GPIOD_5 };
static const unsigned  int i2c3_sck_d_pins[]		= { GPIOD_8 };
static const unsigned  int i2c3_sda_d_pins[]		= { GPIOD_9 };
static const unsigned  int i3c1_sck_d_pins[]		= { GPIOD_10 };
static const unsigned  int i3c1_sda_d_pins[]		= { GPIOD_11 };

/* Bank D func5 */
static const unsigned  int uart_c_tx_d_pins[]		= { GPIOD_0 };
static const unsigned  int uart_c_rx_d_pins[]		= { GPIOD_1 };
static const unsigned  int uart_d_tx_d_pins[]		= { GPIOD_2 };
static const unsigned  int uart_d_rx_d_pins[]		= { GPIOD_3 };
static const unsigned  int uart_e_tx_d_pins[]		= { GPIOD_4 };
static const unsigned  int uart_e_rx_d_pins[]		= { GPIOD_5 };
static const unsigned  int uart_f_tx_d_pins[]		= { GPIOD_6 };
static const unsigned  int uart_f_rx_d_pins[]		= { GPIOD_7 };

/* Bank D func6 */
static const unsigned  int jtag_a_clk_pins[]		= { GPIOD_2 };
static const unsigned  int jtag_a_tms_pins[]		= { GPIOD_3 };
static const unsigned  int jtag_a_tdi_pins[]		= { GPIOD_4 };
static const unsigned  int jtag_a_tdo_pins[]		= { GPIOD_5 };

/* Bank D func7 */
static const unsigned  int gen_clk_d0_pins[]		= { GPIOD_0 };
static const unsigned  int clk12_24_d_pins[]		= { GPIOD_8 };
static const unsigned  int gen_clk_d9_pins[]		= { GPIOD_9 };

/* Bank E func1 */
static const unsigned  int pwm_c_e_pins[]		= { GPIOE_0 };
static const unsigned  int pwm_d_e_pins[]		= { GPIOE_1 };
static const unsigned  int pwm_e_e_pins[]		= { GPIOE_2 };
static const unsigned  int pwm_f_e_pins[]		= { GPIOE_3 };
static const unsigned  int pwm_g_e_pins[]		= { GPIOE_4 };
static const unsigned  int pwm_h_e_pins[]		= { GPIOE_5 };
static const unsigned  int pwm_i_e_pins[]		= { GPIOE_6 };
static const unsigned  int pwm_j_e_pins[]		= { GPIOE_7 };

/* Bank E func6 */
static const unsigned  int pio26_pins[]			= { GPIOE_0 };
static const unsigned  int pio27_pins[]			= { GPIOE_1 };
static const unsigned  int pio28_pins[]			= { GPIOE_2 };
static const unsigned  int pio29_pins[]			= { GPIOE_3 };
static const unsigned  int pio30_pins[]			= { GPIOE_4 };
static const unsigned  int pio31_pins[]			= { GPIOE_5 };

/* Bank E func7 */
static const unsigned  int gen_clk_e_pins[]		= { GPIOE_3 };
static const unsigned  int clk12_24_e_pins[]		= { GPIOE_5 };

/* Bank X func1 */
static const unsigned  int sdio_d0_pins[]		= { GPIOX_0 };
static const unsigned  int sdio_d1_pins[]		= { GPIOX_1 };
static const unsigned  int sdio_d2_pins[]		= { GPIOX_2 };
static const unsigned  int sdio_d3_pins[]		= { GPIOX_3 };
static const unsigned  int sdio_clk_pins[]		= { GPIOX_4 };
static const unsigned  int sdio_cmd_pins[]		= { GPIOX_5 };
static const unsigned  int uart_e_tx_x_pins[]		= { GPIOX_6 };
static const unsigned  int uart_e_rx_x_pins[]		= { GPIOX_7 };
static const unsigned  int uart_e_cts_x_pins[]		= { GPIOX_8 };
static const unsigned  int uart_e_rts_x_pins[]		= { GPIOX_9  };
static const unsigned  int tdm_d1_x_pins[]		= { GPIOX_10 };
static const unsigned  int tdm_d0_x_pins[]		= { GPIOX_11 };
static const unsigned  int tdm_fs0_x_pins[]		= { GPIOX_12 };
static const unsigned  int tdm_sclk0_x_pins[]		= { GPIOX_13 };

/* Bank X func2 */
static const unsigned  int spi3_sclk_pins[]		= { GPIOX_0 };
static const unsigned  int spi3_mosi_pins[]		= { GPIOX_1 };
static const unsigned  int spi3_miso_pins[]		= { GPIOX_2 };
static const unsigned  int spi3_ss0_pins[]		= { GPIOX_3 };
static const unsigned  int spi3_ss1_pins[]		= { GPIOX_4 };
static const unsigned  int clk12_24_x_pins[]		= { GPIOX_5 };
static const unsigned  int spi2_sclk_x_pins[]		= { GPIOX_6 };
static const unsigned  int spi2_mosi_x_pins[]		= { GPIOX_7 };
static const unsigned  int spi2_miso_x_pins[]		= { GPIOX_8 };
static const unsigned  int spi2_ss0_x_pins[]		= { GPIOX_9 };
static const unsigned  int spi1_ss0_x_pins[]		= { GPIOX_10 };
static const unsigned  int spi1_sclk_x_pins[]		= { GPIOX_11 };
static const unsigned  int spi1_mosi_x_pins[]		= { GPIOX_12 };
static const unsigned  int spi1_miso_x_pins[]		= { GPIOX_13 };

/* Bank X func3 */
static const unsigned  int pwm_n_x_pins[]		= { GPIOX_0 };
static const unsigned  int pwm_o_x_pins[]		= { GPIOX_1 };
static const unsigned  int pwm_p_x_pins[]		= { GPIOX_2 };
static const unsigned  int pwm_q_x_pins[]		= { GPIOX_3 };
static const unsigned  int pwm_r_x_pins[]		= { GPIOX_4 };
static const unsigned  int pwm_s_x_pins[]		= { GPIOX_5 };
static const unsigned  int pcie_preset_x_pins[]		= { GPIOX_6 };
static const unsigned  int pcieck_req_x_pins[]		= { GPIOX_7 };
static const unsigned  int pwm_t_x_pins[]		= { GPIOX_8 };
static const unsigned  int pwm_k_x_pins[]		= { GPIOX_9 };
static const unsigned  int pwm_l_x_pins[]		= { GPIOX_10 };
static const unsigned  int pwm_m_x_pins[]		= { GPIOX_11 };

/* Bank X func4 */
static const unsigned  int i2c2_sck_x_pins[]		= { GPIOX_0 };
static const unsigned  int i2c2_sda_x_pins[]		= { GPIOX_1 };
static const unsigned  int i2c6_sck_x2_pins[]		= { GPIOX_2 };
static const unsigned  int i2c6_sda_x3_pins[]		= { GPIOX_3 };
static const unsigned  int i2c4_sck_x_pins[]		= { GPIOX_4 };
static const unsigned  int i2c4_sda_x_pins[]		= { GPIOX_5 };
static const unsigned  int i2c5_sck_x_pins[]		= { GPIOX_6 };
static const unsigned  int i2c5_sda_x_pins[]		= { GPIOX_7 };
static const unsigned  int i2c6_sck_x8_pins[]		= { GPIOX_8 };
static const unsigned  int i2c6_sda_x9_pins[]		= { GPIOX_9 };
static const unsigned  int i2c3_sck_x_pins[]		= { GPIOX_10 };
static const unsigned  int i2c3_sda_x_pins[]		= { GPIOX_11 };
static const unsigned  int i3c1_sck_x_pins[]		= { GPIOX_12 };
static const unsigned  int i3c1_sda_x_pins[]		= { GPIOX_13 };

/* Bank X func5 */
static const unsigned  int uart_d_rx_x_pins[]		= { GPIOX_0 };
static const unsigned  int uart_d_tx_x_pins[]		= { GPIOX_1 };
static const unsigned  int uart_d_cts_x_pins[]		= { GPIOX_2 };
static const unsigned  int uart_d_rts_x_pins[]		= { GPIOX_3 };
static const unsigned  int uart_f_cts_x_pins[]		= { GPIOX_4 };
static const unsigned  int uart_f_rts_x_pins[]		= { GPIOX_5 };
static const unsigned  int uart_f_tx_x_pins[]		= { GPIOX_6 };
static const unsigned  int uart_f_rx_x_pins[]		= { GPIOX_7 };

/* Bank X func7 */
static const unsigned  int gen_clk_x_pins[]		= { GPIOX_13 };

/* Bank A func0 */
static const unsigned  int tdm_d3_a_pins[]		= { GPIOA_0 };
static const unsigned  int tdm_d2_a_pins[]		= { GPIOA_1 };
static const unsigned  int mclk1_a_pins[]		= { GPIOA_2 };
static const unsigned  int tdm_sclk1_a_pins[]		= { GPIOA_3 };
static const unsigned  int tdm_fs1_a_pins[]		= { GPIOA_4 };
static const unsigned  int tdm_d4_a_pins[]		= { GPIOA_5 };
static const unsigned  int tdm_d5_a_pins[]		= { GPIOA_6 };

/* Bank A func1 */
static const unsigned  int pdm_din3_a_pins[]		= { GPIOA_0 };
static const unsigned  int pdm_din2_a_pins[]		= { GPIOA_1 };
static const unsigned  int pdm_din1_a_pins[]		= { GPIOA_2 };
static const unsigned  int pdm_din0_a_pins[]		= { GPIOA_3 };
static const unsigned  int pdm_dclk_a_pins[]		= { GPIOA_4 };
static const unsigned  int i2c3_sck_a_pins[]		= { GPIOA_5 };
static const unsigned  int i2c3_sda_a_pins[]		= { GPIOA_6 };

/* Bank A func7 */
static const unsigned  int gen_clk_a_pins[]		= { GPIOA_6 };

/* Bank ADC func1 */

/* Bank DSI func1 */
static const unsigned  int i2c2_sck_dsi_pins[]		= { GPIODSI_0 };
static const unsigned  int i2c2_sda_dsi_pins[]		= { GPIODSI_1 };
static const unsigned  int i2c3_sck_dsi_pins[]		= { GPIODSI_2 };
static const unsigned  int i2c3_sda_dsi_pins[]		= { GPIODSI_3 };
static const unsigned  int i2c4_sck_dsi_pins[]		= { GPIODSI_4 };
static const unsigned  int i2c4_sda_dsi_pins[]		= { GPIODSI_5 };
static const unsigned  int i2c5_sck_dsi_pins[]		= { GPIODSI_6 };
static const unsigned  int i2c5_sda_dsi_pins[]		= { GPIODSI_7 };
static const unsigned  int i2c6_sck_dsi_pins[]		= { GPIODSI_8 };
static const unsigned  int i2c6_sda_dsi_pins[]		= { GPIODSI_9 };

/* Bank DSI func2 */
static const unsigned  int pwm_k_dsi_pins[]		= { GPIODSI_0 };
static const unsigned  int pwm_l_dsi_pins[]		= { GPIODSI_1 };
static const unsigned  int pwm_m_dsi_pins[]		= { GPIODSI_2 };
static const unsigned  int pwm_n_dsi_pins[]		= { GPIODSI_3 };
static const unsigned  int pwm_o_dsi_pins[]		= { GPIODSI_4 };
static const unsigned  int pwm_p_dsi_pins[]		= { GPIODSI_5 };
static const unsigned  int pwm_q_dsi_pins[]		= { GPIODSI_6 };
static const unsigned  int pwm_r_dsi_pins[]		= { GPIODSI_7 };
static const unsigned  int pwm_s_dsi_pins[]		= { GPIODSI_8 };
static const unsigned  int pwm_t_dsi_pins[]		= { GPIODSI_9 };

/* Bank DSI func4 */
static const unsigned  int pdm_din3_dsi_pins[]		= { GPIODSI_0 };
static const unsigned  int pdm_din2_dsi_pins[]		= { GPIODSI_1 };
static const unsigned  int pdm_din1_dsi_pins[]		= { GPIODSI_2 };
static const unsigned  int pdm_din0_dsi_pins[]		= { GPIODSI_3 };
static const unsigned  int pdm_dclk_dsi_pins[]		= { GPIODSI_4 };

static struct meson_pmx_group meson_c5_periphs_groups[] __initdata = {
	GPIO_GROUP(GPIOB_0),
	GPIO_GROUP(GPIOB_1),
	GPIO_GROUP(GPIOB_2),
	GPIO_GROUP(GPIOB_3),
	GPIO_GROUP(GPIOB_4),
	GPIO_GROUP(GPIOB_5),
	GPIO_GROUP(GPIOB_6),
	GPIO_GROUP(GPIOB_7),
	GPIO_GROUP(GPIOB_8),
	GPIO_GROUP(GPIOB_9),
	GPIO_GROUP(GPIOB_10),
	GPIO_GROUP(GPIOB_11),
	GPIO_GROUP(GPIOB_12),
	GPIO_GROUP(GPIOB_13),
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
	GPIO_GROUP(GPIOC_6),
	GPIO_GROUP(GPIOC_7),
	GPIO_GROUP(GPIOD_0),
	GPIO_GROUP(GPIOD_1),
	GPIO_GROUP(GPIOD_2),
	GPIO_GROUP(GPIOD_3),
	GPIO_GROUP(GPIOD_4),
	GPIO_GROUP(GPIOD_5),
	GPIO_GROUP(GPIOD_6),
	GPIO_GROUP(GPIOD_7),
	GPIO_GROUP(GPIOD_8),
	GPIO_GROUP(GPIOD_9),
	GPIO_GROUP(GPIOD_10),
	GPIO_GROUP(GPIOD_11),
	GPIO_GROUP(GPIOE_0),
	GPIO_GROUP(GPIOE_1),
	GPIO_GROUP(GPIOE_2),
	GPIO_GROUP(GPIOE_3),
	GPIO_GROUP(GPIOE_4),
	GPIO_GROUP(GPIOE_5),
	GPIO_GROUP(GPIOE_6),
	GPIO_GROUP(GPIOE_7),
	GPIO_GROUP(GPIOX_0),
	GPIO_GROUP(GPIOX_1),
	GPIO_GROUP(GPIOX_2),
	GPIO_GROUP(GPIOX_3),
	GPIO_GROUP(GPIOX_4),
	GPIO_GROUP(GPIOX_5),
	GPIO_GROUP(GPIOX_6),
	GPIO_GROUP(GPIOX_7),
	GPIO_GROUP(GPIOX_8),
	GPIO_GROUP(GPIOX_9),
	GPIO_GROUP(GPIOX_10),
	GPIO_GROUP(GPIOX_11),
	GPIO_GROUP(GPIOX_12),
	GPIO_GROUP(GPIOX_13),
	GPIO_GROUP(GPIOA_0),
	GPIO_GROUP(GPIOA_1),
	GPIO_GROUP(GPIOA_2),
	GPIO_GROUP(GPIOA_3),
	GPIO_GROUP(GPIOA_4),
	GPIO_GROUP(GPIOA_5),
	GPIO_GROUP(GPIOA_6),
	GPIO_GROUP(GPIOADC_0),
	GPIO_GROUP(GPIOADC_1),
	GPIO_GROUP(GPIOADC_2),
	GPIO_GROUP(GPIOADC_3),
	GPIO_GROUP(GPIOADC_4),
	GPIO_GROUP(GPIOADC_5),
	GPIO_GROUP(GPIOADC_6),
	GPIO_GROUP(GPIOADC_7),
	GPIO_GROUP(GPIODSI_0),
	GPIO_GROUP(GPIODSI_1),
	GPIO_GROUP(GPIODSI_2),
	GPIO_GROUP(GPIODSI_3),
	GPIO_GROUP(GPIODSI_4),
	GPIO_GROUP(GPIODSI_5),
	GPIO_GROUP(GPIODSI_6),
	GPIO_GROUP(GPIODSI_7),
	GPIO_GROUP(GPIODSI_8),
	GPIO_GROUP(GPIODSI_9),
	GPIO_GROUP(GPIOCSI_0),
	GPIO_GROUP(GPIOCSI_1),
	GPIO_GROUP(GPIOCSI_2),
	GPIO_GROUP(GPIOCSI_3),
	GPIO_GROUP(GPIOCSI_4),
	GPIO_GROUP(GPIOCSI_5),
	GPIO_GROUP(GPIOCSI_6),
	GPIO_GROUP(GPIOCSI_7),
	GPIO_GROUP(GPIOCSI_8),
	GPIO_GROUP(GPIOCSI_9),
	GPIO_GROUP(GPIOCSI_10),
	GPIO_GROUP(GPIOCSI_11),
	GPIO_GROUP(GPIOCSI_12),
	GPIO_GROUP(GPIOCSI_13),
	GPIO_GROUP(GPIOCSI_14),
	GPIO_GROUP(GPIOCSI_15),
	GPIO_GROUP(GPIOCSI_16),
	GPIO_GROUP(GPIOCSI_17),
	GPIO_GROUP(GPIOCSI_18),
	GPIO_GROUP(GPIOCSI_19),
	GPIO_GROUP(GPIOCSI_20),
	GPIO_GROUP(GPIOCSI_21),
	GPIO_GROUP(GPIOCSI_22),
	GPIO_GROUP(GPIOCSI_23),
	GPIO_GROUP(GPIOMCLK_0),
	GPIO_GROUP(GPIOMCLK_1),
	GPIO_GROUP(GPIOMCLK_2),
	GPIO_GROUP(GPIOMCLK_3),

	/* Bank B func1 */
	GROUP(emmc_d0,			1),
	GROUP(emmc_d1,			1),
	GROUP(emmc_d2,			1),
	GROUP(emmc_d3,			1),
	GROUP(emmc_d4,			1),
	GROUP(emmc_d5,			1),
	GROUP(emmc_d6,			1),
	GROUP(emmc_d7,			1),
	GROUP(emmc_clk,			1),
	GROUP(emmc_cmd,			1),
	GROUP(emmc_ds,			1),
	GROUP(clk12_24_b,		1),

	/* Bank B func2 */
	GROUP(spi1_ss0_b,		2),
	GROUP(spi1_sclk_b,		2),
	GROUP(spi1_mosi_b,		2),
	GROUP(spi1_miso_b,		2),
	GROUP(spi1_ss1_b,		2),
	GROUP(spi1_ss2_b,		2),

	/* Bank B func3 */
	GROUP(spinf_mo_d0,		3),
	GROUP(spinf_mi_d1,		3),
	GROUP(spinf_wp_d2,		3),
	GROUP(spinf_hold_d3,		3),
	GROUP(spinf_d4,			3),
	GROUP(spinf_d5,			3),
	GROUP(spinf_d6,			3),
	GROUP(spinf_d7,			3),
	GROUP(spinf_clk,		3),
	GROUP(spinf_cs0,		3),

	/* Bank B func4 */
	GROUP(pdm_din3_b,		4),
	GROUP(pdm_din2_b,		4),
	GROUP(pdm_din1_b,		4),
	GROUP(pdm_din0_b,		4),
	GROUP(pdm_clk_b,		4),
	GROUP(i2c6_sck_b,		4),
	GROUP(i2c6_sda_b,		4),

	/* Bank B func5 */
	GROUP(pwm_o_b,			5),
	GROUP(pwm_p_b,			5),
	GROUP(pwm_q_b,			5),
	GROUP(pwm_r_b,			5),
	GROUP(pwm_s_b,			5),
	GROUP(pwm_t_b,			5),

	/* Bank B func6 */
	GROUP(uart_c_cts_b,		6),
	GROUP(uart_c_rts_b,		6),
	GROUP(uart_c_tx_b,		6),
	GROUP(uart_c_rx_b,		6),

	/* Bank B func7 */
	GROUP(i2c5_sck_b,		7),
	GROUP(i2c5_sda_b,		7),

	/* Bank C func1 */
	GROUP(sdcard_d0,		1),
	GROUP(sdcard_d1,		1),
	GROUP(sdcard_d2,		1),
	GROUP(sdcard_d3,		1),
	GROUP(sdcard_clk,		1),
	GROUP(sdcard_cmd,		1),
	GROUP(pcieck_reqn_c,		1),

	/* Bank C func2 */
	GROUP(jtag_b_tdo,		2),
	GROUP(jtag_b_tdi,		2),
	GROUP(uart_b_rx,		2),
	GROUP(uart_b_tx,		2),
	GROUP(jtag_b_clk,		2),
	GROUP(jtag_b_tms,		2),

	/* Bank C func3 */
	GROUP(tdm_d3_c,			3),
	GROUP(tdm_d2_c,			3),
	GROUP(mclk1_c,			3),
	GROUP(tdm_sclk1_c,		3),
	GROUP(tdm_fs1_c,		3),
	GROUP(tdm_d4_c,			3),
	GROUP(tdm_d5_c,			3),

	/* Bank C func4 */
	GROUP(spi1_mosi_c,		4),
	GROUP(spi1_miso_c,		4),
	GROUP(spi1_sclk_c,		4),
	GROUP(spi1_ss0_c,		4),
	GROUP(spi1_ss1_c,		4),
	GROUP(spi1_ss2_c,		4),
	GROUP(clk12_24_c,		4),

	/* Bank C func5 */
	GROUP(pwm_s_c,			5),
	GROUP(pwm_t_c,			5),
	GROUP(pwm_m_c,			5),
	GROUP(pwm_n_c,			5),
	GROUP(pwm_o_c,			5),
	GROUP(pwm_p_c,			5),
	GROUP(pwm_q_c,			5),

	/* Bank C func6 */
	GROUP(uart_c_tx_c,		6),
	GROUP(uart_c_rx_c,		6),
	GROUP(uart_d_tx_c,		6),
	GROUP(uart_d_rx_c,		6),
	GROUP(i2c5_sck_c,		6),
	GROUP(i2c5_sda_c,		6),

	/* Bank C func7 */
	GROUP(gen_clk_c,		7),

	/* Bank D func1 */
	GROUP(pwm_t_d,			1),
	GROUP(pwm_k_d,			1),
	GROUP(pwm_l_d,			1),
	GROUP(pwm_m_d,			1),
	GROUP(pwm_n_d,			1),
	GROUP(pwm_o_d,			1),
	GROUP(pwm_p_d,			1),
	GROUP(pwm_q_d,			1),
	GROUP(pwm_r_d,			1),
	GROUP(pwm_s_d,			1),

	/* Bank D func2 */
	GROUP(spi2_mosi_d,		2),
	GROUP(spi2_miso_d,		2),
	GROUP(spi2_sclk_d,		2),
	GROUP(spi2_ss0_d,		2),
	GROUP(spi1_miso_d,		2),
	GROUP(spi1_mosi_d,		2),
	GROUP(spi1_sclk_d,		2),
	GROUP(spi1_ss1_d,		2),
	GROUP(can_b_tx,			2),
	GROUP(can_b_rx,			2),
	GROUP(pcie_preset_d,		2),
	GROUP(pcieck_reqn_d,		2),

	/* Bank D func3 */
	GROUP(tdm_d3_d,			3),
	GROUP(tdm_d2_d,			3),
	GROUP(mclk0,			3),
	GROUP(tdm_sclk0,		3),
	GROUP(tdm_fs0,			3),
	GROUP(tdm_d1_d,			3),
	GROUP(i2c3_sck_d,		3),
	GROUP(i2c3_sda_d,		3),
	GROUP(i3c1_sck_d,		3),
	GROUP(i3c1_sda_d,		3),

	/* Bank D func5 */
	GROUP(uart_c_tx_d,		5),
	GROUP(uart_c_rx_d,		5),
	GROUP(uart_d_tx_d,		5),
	GROUP(uart_d_rx_d,		5),
	GROUP(uart_e_tx_d,		5),
	GROUP(uart_e_rx_d,		5),
	GROUP(uart_f_tx_d,		5),
	GROUP(uart_f_rx_d,		5),

	/* Bank D func6 */
	GROUP(jtag_a_clk,		6),
	GROUP(jtag_a_tms,		6),
	GROUP(jtag_a_tdi,		6),
	GROUP(jtag_a_tdo,		6),

	/* Bank D func7 */
	GROUP(gen_clk_d0,		7),
	GROUP(clk12_24_d,		7),
	GROUP(gen_clk_d9,		7),

	/* Bank E func1 */
	GROUP(pwm_c_e,			1),
	GROUP(pwm_d_e,			1),
	GROUP(pwm_e_e,			1),
	GROUP(pwm_f_e,			1),
	GROUP(pwm_g_e,			1),
	GROUP(pwm_h_e,			1),
	GROUP(pwm_i_e,			1),
	GROUP(pwm_j_e,			1),

	/* Bank E func6 */
	GROUP(pio26,			6),
	GROUP(pio27,			6),
	GROUP(pio28,			6),
	GROUP(pio29,			6),
	GROUP(pio30,			6),
	GROUP(pio31,			6),

	/* Bank E func7 */
	GROUP(gen_clk_e,		7),
	GROUP(clk12_24_e,		7),

	/* Bank X func1 */
	GROUP(sdio_d0,			1),
	GROUP(sdio_d1,			1),
	GROUP(sdio_d2,			1),
	GROUP(sdio_d3,			1),
	GROUP(sdio_clk,			1),
	GROUP(sdio_cmd,			1),
	GROUP(uart_e_tx_x,		1),
	GROUP(uart_e_rx_x,		1),
	GROUP(uart_e_cts_x,		1),
	GROUP(uart_e_rts_x,		1),
	GROUP(tdm_d1_x,			1),
	GROUP(tdm_d0_x,			1),
	GROUP(tdm_fs0_x,		1),
	GROUP(tdm_sclk0_x,		1),

	/* Bank X func2 */
	GROUP(spi3_sclk,		2),
	GROUP(spi3_mosi,		2),
	GROUP(spi3_miso,		2),
	GROUP(spi3_ss0,			2),
	GROUP(spi3_ss1,			2),
	GROUP(clk12_24_x,		2),
	GROUP(spi2_sclk_x,		2),
	GROUP(spi2_mosi_x,		2),
	GROUP(spi2_miso_x,		2),
	GROUP(spi2_ss0_x,		2),
	GROUP(spi1_ss0_x,		2),
	GROUP(spi1_sclk_x,		2),
	GROUP(spi1_mosi_x,		2),
	GROUP(spi1_miso_x,		2),

	/* Bank X func3 */
	GROUP(pwm_n_x,			3),
	GROUP(pwm_o_x,			3),
	GROUP(pwm_p_x,			3),
	GROUP(pwm_q_x,			3),
	GROUP(pwm_r_x,			3),
	GROUP(pwm_s_x,			3),
	GROUP(pcie_preset_x,		3),
	GROUP(pcieck_req_x,		3),
	GROUP(pwm_t_x,			3),
	GROUP(pwm_k_x,			3),
	GROUP(pwm_l_x,			3),
	GROUP(pwm_m_x,			3),

	/* Bank X func4 */
	GROUP(i2c2_sck_x,		4),
	GROUP(i2c2_sda_x,		4),
	GROUP(i2c6_sck_x2,		4),
	GROUP(i2c6_sda_x3,		4),
	GROUP(i2c4_sck_x,		4),
	GROUP(i2c4_sda_x,		4),
	GROUP(i2c5_sck_x,		4),
	GROUP(i2c5_sda_x,		4),
	GROUP(i2c6_sck_x8,		4),
	GROUP(i2c6_sda_x9,		4),
	GROUP(i2c3_sck_x,		4),
	GROUP(i2c3_sda_x,		4),
	GROUP(i3c1_sck_x,		4),
	GROUP(i3c1_sda_x,		4),

	/* Bank X func5 */
	GROUP(uart_d_rx_x,		5),
	GROUP(uart_d_tx_x,		5),
	GROUP(uart_d_cts_x,		5),
	GROUP(uart_d_rts_x,		5),
	GROUP(uart_f_cts_x,		5),
	GROUP(uart_f_rts_x,		5),
	GROUP(uart_f_tx_x,		5),
	GROUP(uart_f_rx_x,		5),

	/* Bank X func7 */
	GROUP(gen_clk_x,		7),

	/* Bank A func1 */
	GROUP(tdm_d3_a,			1),
	GROUP(tdm_d2_a,			1),
	GROUP(mclk1_a,			1),
	GROUP(tdm_sclk1_a,		1),
	GROUP(tdm_fs1_a,		1),
	GROUP(tdm_d4_a,			1),
	GROUP(tdm_d5_a,			1),

	/* Bank A func2 */
	GROUP(pdm_din3_a,		2),
	GROUP(pdm_din2_a,		2),
	GROUP(pdm_din1_a,		2),
	GROUP(pdm_din0_a,		2),
	GROUP(pdm_dclk_a,		2),
	GROUP(i2c3_sck_a,		2),
	GROUP(i2c3_sda_a,		2),

	/* Bank A func7 */
	GROUP(gen_clk_a,		7),

	/* Bank ADC func1 */

	/* Bank DSI func1 */
	GROUP(i2c2_sck_dsi,		1),
	GROUP(i2c2_sda_dsi,		1),
	GROUP(i2c3_sck_dsi,		1),
	GROUP(i2c3_sda_dsi,		1),
	GROUP(i2c4_sck_dsi,		1),
	GROUP(i2c4_sda_dsi,		1),
	GROUP(i2c5_sck_dsi,		1),
	GROUP(i2c5_sda_dsi,		1),
	GROUP(i2c6_sck_dsi,		1),
	GROUP(i2c6_sda_dsi,		1),

	/* Bank DSI func2 */
	GROUP(pwm_k_dsi,		2),
	GROUP(pwm_l_dsi,		2),
	GROUP(pwm_m_dsi,		2),
	GROUP(pwm_n_dsi,		2),
	GROUP(pwm_o_dsi,		2),
	GROUP(pwm_p_dsi,		2),
	GROUP(pwm_q_dsi,		2),
	GROUP(pwm_r_dsi,		2),
	GROUP(pwm_s_dsi,		2),
	GROUP(pwm_t_dsi,		2),

	/* Bank DSI func4 */
	GROUP(pdm_din3_dsi,		4),
	GROUP(pdm_din2_dsi,		4),
	GROUP(pdm_din1_dsi,		4),
	GROUP(pdm_din0_dsi,		4),
	GROUP(pdm_dclk_dsi,		4)
};

static const char * const gpio_periphs_groups[] = {
	"GPIOB_0", "GPIOB_1", "GPIOB_2", "GPIOB_3", "GPIOB_4", "GPIOB_5",
	"GPIOB_6", "GPIOB_7", "GPIOB_8", "GPIOB_9",
	"GPIOB_10", "GPIOB_11", "GPIOB_12", "GPIOB_13",

	"GPIOC_0", "GPIOC_1", "GPIOC_2", "GPIOC_3", "GPIOC_4",
	"GPIOC_5", "GPIOC_6", "GPIOC_7",

	"GPIOD_0", "GPIOD_1", "GPIOD_2", "GPIOD_3", "GPIOD_4",
	"GPIOD_5", "GPIOD_6", "GPIOD_7", "GPIOD_8", "GPIOD_9", "GPIOD_10",
	"GPIOD_11",

	"GPIOE_0", "GPIOE_1", "GPIOE_2", "GPIOE_3", "GPIOE_4",
	"GPIOE_5", "GPIOE_6", "GPIOE_7",

	"GPIOX_0", "GPIOX_1", "GPIOX_2", "GPIOX_3", "GPIOX_4",
	"GPIOX_5", "GPIOX_6", "GPIOX_7", "GPIOX_8", "GPIOX_9", "GPIOX_10",
	"GPIOX_11", "GPIOX_12", "GPIOX_13",

	"GPIOA_0", "GPIOA_1", "GPIOA_2", "GPIOA_3", "GPIOA_4",
	"GPIOA_5", "GPIOA_6",

	"GPIOADC_0", "GPIOADC_1", "GPIOADC_2", "GPIOADC_3",
	"GPIOADC_4", "GPIOADC_5", "GPIOADC_6", "GPIOADC_7",

	"GPIODSI_0", "GPIODSI_1", "GPIODSI_2", "GPIODSI_3", "GPIODSI_4",
	"GPIODSI_5", "GPIODSI_6", "GPIODSI_7", "GPIODSI_8", "GPIODSI_9",

	"GPIOCSI_0", "GPIOCSI_1", "GPIOCSI_2", "GPIOCSI_3", "GPIOCSI_4",
	"GPIOCSI_5", "GPIOCSI_6", "GPIOCSI_7", "GPIOCSI_8", "GPIOCSI_9",
	"GPIOCSI_10", "GPIOCSI_11", "GPIOCSI_12", "GPIOCSI_13",
	"GPIOCSI_14", "GPIOCSI_15", "GPIOCSI_16", "GPIOCSI_17", "GPIOCSI_18",
	"GPIOCSI_19", "GPIOCSI_20", "GPIOCSI_21", "GPIOCSI_22",
	"GPIOCSI_23",

	"GPIOMCLK_0", "GPIOMCLK_1", "GPIOMCLK_2", "GPIOMCLK_3",
};

static const char * const emmc_groups[] = {
	"emmc_d0", "emmc_d1", "emmc_d2", "emmc_d3", "emmc_d4",
	"emmc_d5", "emmc_d6", "emmc_d7", "emmc_clk",
	"emmc_cmd", "emmc_ds"
};

static const char * const clk12_24_groups[] = {
	"clk12_24_b", "clk12_24_c", "clk12_24_d", "clk12_24_e",
	"clk12_24_x"
};

static const char * const spi1_groups[] = {
	"spi1_ss0_b", "spi1_sclk_b", "spi1_mosi_b", "spi1_miso_b",
	"spi1_ss1_b", "spi1_ss2_b",

	"spi1_mosi_c", "spi1_miso_c", "spi1_sclk_c", "spi1_ss0_c",
	"spi1_ss1_c", "spi1_ss2_c",

	"spi2_mosi_d", "spi2_miso_d", "spi2_sclk_d", "spi2_ss0_d",

	"spi1_ss0_x", "spi1_sclk_x", "spi1_mosi_x", "spi1_miso_x"

};

static const char * const spi2_groups[] = {
	"spi2_mosi_d", "spi2_miso_d", "spi2_sclk_d", "spi2_ss0_d",
	"spi2_sclk_x", "spi2_mosi_x", "spi2_miso_x", "spi2_ss0_x"
};

static const char * const spi3_groups[] = {
	"spi3_sclk", "spi3_mosi", "spi3_miso", "spi3_ss0",
	"spi3_ss1"
};

static const char * const spinf_groups[] = {
	"spinf_mo_d0", "spinf_mi_d1", "spinf_wp_d2", "spinf_hold_d3",
	"spinf_d4", "spinf_d5", "spinf_d6", "spinf_d7",
	"spinf_clk", "spinf_cs0"
};

static const char * const pdm_groups[] = {
	"pdm_din3_b", "pdm_din2_b", "pdm_din1_b", "pdm_din0_b",
	"pdm_clk_b",

	"pdm_din3_a", "pdm_din2_a", "pdm_din1_a", "pdm_din0_a",
	"pdm_clk_a",

	"pdm_din3_dsi", "pdm_din2_dsi", "pdm_din1_dsi", "pdm_din0_dsi",
	"pdm_clk_dsi"

};

static const char * const i2c2_groups[] = {
	"i2c2_sck_x", "i2c2_sda_x",
	"i2c2_sck_dsi", "i2c2_sda_dsi"
};

static const char * const i2c3_groups[] = {
	"i2c3_sck_d", "i2c3_sda_d",
	"i2c3_sck_x", "i2c3_sda_x",
	"i2c3_sck_a", "i2c3_sda_a",
	"i2c3_sck_dsi", "i2c3_sda_dsi"
};

static const char * const i2c4_groups[] = {
	"i2c4_sck_x", "i2c4_sda_x",
	"i2c4_sck_dsi", "i2c4_sda_dsi"
};

static const char * const i2c5_groups[] = {
	"i2c5_sck_b", "i2c5_sda_b",
	"i2c5_sck_c", "i2c5_sda_c",
	"i2c5_sck_x", "i2c5_sda_x",
	"i2c5_sck_dsi", "i2c5_sda_dsi"
};

static const char * const i2c6_groups[] = {
	"i2c6_sck_b", "i2c6_sda_b",
	"i2c6_sck_x2", "i2c6_sda_x3",
	"i2c6_sck_x8", "i2c6_sda_x9",
	"i2c6_sck_dsi", "i2c6_sda_dsi"
};

static const char * const i3c1_groups[] = {
	"i3c1_sda_d", "i3c1_sck_d",
	"i3c1_sda_x", "i3c1_sck_x"
};

static const char * const pwm_c_groups[] = {
	"pwm_c_e"
};

static const char * const pwm_d_groups[] = {
	"pwm_d_e"
};

static const char * const pwm_e_groups[] = {
	"pwm_e_e"
};

static const char * const pwm_f_groups[] = {
	"pwm_f_e"
};

static const char * const pwm_g_groups[] = {
	"pwm_g_e"
};

static const char * const pwm_h_groups[] = {
	"pwm_h_e"
};

static const char * const pwm_i_groups[] = {
	"pwm_i_e"
};

static const char * const pwm_j_groups[] = {
	"pwm_j_e"
};

static const char * const pwm_k_groups[] = {
	"pwm_k_d", "pwm_k_x", "pwm_k_dsi"
};

static const char * const pwm_l_groups[] = {
	"pwm_l_d", "pwm_l_x", "pwm_l_dsi"
};

static const char * const pwm_m_groups[] = {
	"pwm_m_c", "pwm_m_d", "pwm_m_x", "pwm_m_dsi"
};

static const char * const pwm_n_groups[] = {
	"pwm_n_c", "pwm_n_d", "pwm_n_x", "pwm_n_dsi"
};

static const char * const pwm_o_groups[] = {
	"pwm_o_b", "pwm_o_c", "pwm_o_d", "pwm_o_x", "pwm_o_dsi"
};

static const char * const pwm_p_groups[] = {
	"pwm_p_b", "pwm_p_c", "pwm_p_x", "pwm_p_dsi"
};

static const char * const pwm_q_groups[] = {
	"pwm_q_b", "pwm_q_c", "pwm_q_d", "pwm_q_x", "pwm_q_dsi"
};

static const char * const pwm_r_groups[] = {
	"pwm_r_b", "pwm_r_d", "pwm_r_x", "pwm_r_dsi"
};

static const char * const pwm_s_groups[] = {
	"pwm_s_b", "pwm_s_c", "pwm_s_d", "pwm_s_x", "pwm_s_dsi"
};

static const char * const pwm_t_groups[] = {
	"pwm_t_b", "pwm_t_c", "pwm_t_d", "pwm_t_x", "pwm_t_dsi"
};

static const char * const uart_b_groups[] = {
	"uart_b_tx", "uart_b_tx"
};

static const char * const uart_c_groups[] = {
	"uart_c_cts_b", "uart_c_rts_b", "uart_c_tx_b", "uart_c_rx_b",
	"uart_c_tx_c", "uart_c_rx_c", "uart_d_tx_c", "uart_d_rx_c",
	"uart_c_tx_d", "uart_c_tx_d"
};

static const char * const uart_d_groups[] = {
	"uart_d_tx_d", "uart_d_rx_d",
	"uart_d_tx_x", "uart_d_rx_x",
	"uart_d_cts_x", "uart_d_rts_x"
};

static const char * const uart_e_groups[] = {
	"uart_e_tx_d", "uart_e_rx_d",
	"uart_e_tx_x", "uart_e_rx_x", "uart_e_cts_x", "uart_e_rts_x"
};

static const char * const uart_f_groups[] = {
	"uart_f_tx_d", "uart_f_rx_d",
	"uart_f_tx_x", "uart_f_rx_x",
	"uart_f_cts_x", "uart_f_rts_x"
};

static const char * const jtag_a_groups[] = {
	"jtag_a_tdo", "jtag_a_tdi", "jtag_a_clk", "jtag_a_tms"
};

static const char * const jtag_b_groups[] = {
	"jtag_b_tdo", "jtag_b_tdi", "jtag_b_clk", "jtag_b_tms"
};

static const char * const tdm_groups[] = {
	"tdm_d3_c", "tdm_d2_c", "tdm_sclk1_c", "tdm_fs1_c",
	"tdm_d4_c", "tdm_d5_c",

	"tdm_d3_d", "tdm_d2_d", "tdm_sclk0", "tdm_fs0", "tdm_d1_d",
	"tdm_d1_x", "tdm_d0_x", "tdm_fs0_x", "tdm_sclk0_x",

	"tdm_d3_a", "tdm_d2_a", "tdm_sclk1_a", "tdm_fs1_a",
	"tdm_d4_a", "tdm_d5_a"

};

static const char * const mclk0_groups[] = {
	"mclk0",
};

static const char * const mclk1_groups[] = {
	"mclk1_c", "mclk1_a"
};

static const char * const gen_clk_groups[] = {
	"gen_clk_c", "gen_clk_d0", "gen_clk_d9", "gen_clk_e",
	"gen_clk_x", "gen_clk_a"
};

static const char * const can_b_groups[] = {
	"can_b_tx", "can_b_rx"
};

static const char * const pcie_groups[] = {
	"pcie_preset_d", "pcieck_reqn_d",
};

static const char * const pio_groups[] = {
	"pio26", "pio27", "pio28", "pio29",
	"pio30", "pio31"
};

static const char * const sdio_groups[] = {
	"sdio_d0", "sdio_d1", "sdio_d2", "sdio_d3",
	"sdio_clk", "sdio_cmd"
};

static struct meson_pmx_func meson_c5_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(emmc),
	FUNCTION(clk12_24),
	FUNCTION(spi1),
	FUNCTION(spi2),
	FUNCTION(spi3),
	FUNCTION(spinf),
	FUNCTION(pdm),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(i2c4),
	FUNCTION(i2c5),
	FUNCTION(i2c6),
	FUNCTION(i3c1),
	FUNCTION(pwm_c),
	FUNCTION(pwm_d),
	FUNCTION(pwm_e),
	FUNCTION(pwm_f),
	FUNCTION(pwm_g),
	FUNCTION(pwm_h),
	FUNCTION(pwm_i),
	FUNCTION(pwm_j),
	FUNCTION(pwm_k),
	FUNCTION(pwm_l),
	FUNCTION(pwm_m),
	FUNCTION(pwm_n),
	FUNCTION(pwm_o),
	FUNCTION(pwm_p),
	FUNCTION(pwm_q),
	FUNCTION(pwm_r),
	FUNCTION(pwm_s),
	FUNCTION(pwm_t),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_d),
	FUNCTION(uart_e),
	FUNCTION(uart_f),
	FUNCTION(jtag_a),
	FUNCTION(jtag_b),
	FUNCTION(tdm),
	FUNCTION(mclk0),
	FUNCTION(mclk1),
	FUNCTION(gen_clk),
	FUNCTION(can_b),
	FUNCTION(pcie),
	FUNCTION(pio),
	FUNCTION(sdio)
};

static struct meson_bank meson_c5_ao_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in*/
	BANK_DS("P", GPIOP_0,    GPIOP_13,  -1, -1,
		 0x3,  0,  0x4,  0,  0x2,  0,  0x1,  0,  0x0,  0, 0x7,  0),
	BANK_DS("F", GPIOF_0,    GPIOF_6,  -1, -1,
		 0xb,  0,  0xc,  0,  0xa,  0,  0x9,  0,  0x8,  0, 0xf,  0),
	BANK_DS("AO", GPIOAO_0,    GPIOAO_25,  -1, -1,
		 0x13,  0,  0x14,  0,  0x12,  0,  0x11,  0,  0x10,  0, 0x17,  0),
	BANK_DS("TESTN", GPIO_TEST_N,    GPIO_TEST_N,  -1, -1,
		 0x23,  0,  0x24,  0,  0x22,  0,  0x21,  0,  0x20,  0, 0x27,  0)
};

static struct meson_pmx_bank meson_c5_ao_pmx_banks[] = {
	BANK_PMX("P",       GPIOP_0,     GPIOP_13,     0x5,  0),
	BANK_PMX("F",       GPIOF_0,     GPIOF_6,      0x3,  0),
	BANK_PMX("AO",      GPIOAO_0,    GPIOAO_25,    0x0,  0),
	BANK_PMX("TESTN",   GPIO_TEST_N, GPIO_TEST_N,  0x3,  28)
};

static struct meson_bank meson_c5_periphs_banks[] = {
	/* name  first  last  irq  pullen  pull  dir  out  in ds*/
	BANK_DS("MCLK", GPIOMCLK_0, GPIOMCLK_3,  -1, -1,
		 0x3,   0,  0x4,    0,  0x2,   0,  0x1,   0,  0x0,  0, 0x7,  0),
	BANK_DS("X",    GPIOX_0,    GPIOX_13,  -1, -1,
		 0x13,  0,  0x14,   0,  0x12,  0,  0x11,  0,  0x10, 0, 0x17, 0),
	BANK_DS("ADC",  GPIOADC_0,  GPIOADC_7,  -1, -1,
		 0x23,  0,  0x24,   0,  0x22,  0,  0x21,  0,  0x20, 0, 0x27, 0),
	BANK_DS("E",    GPIOE_0,    GPIOE_7,  -1, -1,
		 0x2b,  0,  0x2c,   0,  0x2a,  0,  0x29,  0,  0x28, 0, 0x2f, 0),
	BANK_DS("D",    GPIOD_0,    GPIOD_11,  -1, -1,
		 0x33,  0,  0x34,   0,  0x32,  0,  0x31,  0,  0x30, 0, 0x37, 0),
	BANK_DS("A",    GPIOA_0,    GPIOA_6,  -1, -1,
		 0x3b,  0,  0x3c,   0,  0x3a,  0,  0x39,  0,  0x38, 0, 0x3f, 0),
	BANK_DS("DSI",  GPIODSI_0,  GPIODSI_9,  -1, -1,
		 0x43,  0,  0x44,   0,  0x42,  0,  0x41,  0,  0x40, 0, 0x47, 0),
	BANK_DS("C",    GPIOC_0,    GPIOC_7,  -1, -1,
		 0x53,  0,  0x54,   0,  0x52,  0,  0x51,  0,  0x50, 0, 0x57, 0),
	BANK_DS("B",    GPIOB_0,    GPIOB_13,  -1, -1,
		 0x63,  0,  0x64,   0,  0x62,  0,  0x61,  0,  0x60, 0, 0x67, 0),
};

static struct meson_pmx_bank meson_c5_periphs_pmx_banks[] = {
	/*name	             first	 lask        reg offset*/
	BANK_PMX("B",       GPIOB_0,     GPIOB_13,    0x0,  0),
	BANK_PMX("C",       GPIOC_0,     GPIOC_7,     0x6,  0),
	BANK_PMX("D",       GPIOD_0,     GPIOD_11,    0xb,  0),
	BANK_PMX("E",       GPIOE_0,     GPIOE_7,     0x8,  0),
	BANK_PMX("X",       GPIOX_0,     GPIOX_13,    0x3,  0),
	BANK_PMX("A",       GPIOA_0,     GPIOA_6,     0x5,  0),
	BANK_PMX("ADC",     GPIOADC_0,   GPIOADC_7,   0x7,  0),
	BANK_PMX("DSI",     GPIODSI_0,   GPIODSI_9,   0x10, 0),
	BANK_PMX("MCLK",    GPIOMCLK_0,  GPIOMCLK_3,  0xd,  0),
};

static struct meson_axg_pmx_data meson_c5_periphs_pmx_banks_data = {
	.pmx_banks	= meson_c5_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_c5_periphs_pmx_banks),
};

static struct meson_axg_pmx_data meson_c5_ao_pmx_banks_data = {
	.pmx_banks	= meson_c5_ao_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_c5_ao_pmx_banks),
};

static struct meson_pinctrl_data meson_c5_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_c5_periphs_pins,
	.groups		= meson_c5_periphs_groups,
	.funcs		= meson_c5_periphs_functions,
	.banks		= meson_c5_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_c5_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_c5_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_c5_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_c5_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_c5_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static struct meson_pinctrl_data meson_c5_ao_pinctrl_data __refdata = {
	.name		= "ao-banks",
	.pins		= meson_c5_ao_pins,
	.groups		= meson_c5_ao_groups,
	.funcs		= meson_c5_ao_functions,
	.banks		= meson_c5_ao_banks,
	.num_pins	= ARRAY_SIZE(meson_c5_ao_pins),
	.num_groups	= ARRAY_SIZE(meson_c5_ao_groups),
	.num_funcs	= ARRAY_SIZE(meson_c5_ao_functions),
	.num_banks	= ARRAY_SIZE(meson_c5_ao_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_c5_ao_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static const struct of_device_id meson_c5_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-c5-periphs-pinctrl",
		.data = &meson_c5_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-c5-ao-pinctrl",
		.data = &meson_c5_ao_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_c5_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name	= "meson-c5-pinctrl",
	},
};

static int __init meson_c5_pinctrl_init(void)
{
	meson_c5_pinctrl_driver.driver.of_match_table =
				meson_c5_pinctrl_dt_match;
	return platform_driver_register(&meson_c5_pinctrl_driver);
}
arch_initcall(meson_c5_pinctrl_init);

MODULE_LICENSE("GPL v2");
