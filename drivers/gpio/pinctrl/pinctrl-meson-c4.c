// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <dt-bindings/gpio/meson-c4-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"

static const struct pinctrl_pin_desc meson_c4_periphs_pins[] = {
	/* GPIOD */
	MESON_PIN(GPIOD_0),
	MESON_PIN(GPIOD_1),
	MESON_PIN(GPIOD_2),
	MESON_PIN(GPIOD_3),
	MESON_PIN(GPIOD_4),
	MESON_PIN(GPIOD_5),
	MESON_PIN(GPIOD_6),
	/* GPIOE */
	MESON_PIN(GPIOE_0),
	MESON_PIN(GPIOE_1),
	MESON_PIN(GPIOE_2),
	MESON_PIN(GPIOE_3),
	MESON_PIN(GPIOE_4),
	/* GPIOB */
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
	/* GPIOC */
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
	MESON_PIN(GPIOC_6),
	/* GPIOX */
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
	/* GPIOM */
	MESON_PIN(GPIOM_0),
	MESON_PIN(GPIOM_1),
	MESON_PIN(GPIOM_2),
	MESON_PIN(GPIOM_3),
	MESON_PIN(GPIOM_4),
	MESON_PIN(GPIOM_5),
	MESON_PIN(GPIOM_6),
	MESON_PIN(GPIOM_7),
	MESON_PIN(GPIOM_8),
	MESON_PIN(GPIOM_9),
	/* GPIOA */
	MESON_PIN(GPIOA_0),
	MESON_PIN(GPIOA_1)
};

static const struct pinctrl_pin_desc meson_c4_aobus_pins[] = {
	/* GPIOAO */
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
	MESON_PIN(GPIOAO_13)
};

static const struct pinctrl_pin_desc meson_c4_testn_pins[] = {
	MESON_PIN(GPIO_TEST_N)
};

/* Bank[D]:Function[1] */
static const unsigned int pwm_g_pins[]			= { GPIOD_0 };
static const unsigned int pwm_h_pins[]			= { GPIOD_1 };
static const unsigned int eth_led_act_pins[]		= { GPIOD_2 };
static const unsigned int eth_led_link_pins[]		= { GPIOD_3 };
static const unsigned int pwm_d_pins[]			= { GPIOD_4 };
static const unsigned int pwm_f_pins[]			= { GPIOD_5 };
static const unsigned int pwm_k_pins[]			= { GPIOD_6 };

/* Bank[D]:Function[2] */
static const unsigned int uart_a_tx_pins[]		= { GPIOD_0 };
static const unsigned int uart_a_rx_pins[]		= { GPIOD_1 };
static const unsigned int spi_b_miso_pins[]		= { GPIOD_2 };
static const unsigned int spi_b_mosi_pins[]		= { GPIOD_3 };
static const unsigned int spi_b_sclk_pins[]		= { GPIOD_4 };
static const unsigned int spi_b_ss1_pins[]		= { GPIOD_5 };
static const unsigned int spi_b_ss0_pins[]		= { GPIOD_6 };

/* Bank[D]:Function[3] */
static const unsigned int i2cm_a_scl_pins[]		= { GPIOD_0 };
static const unsigned int i2cm_a_sda_pins[]		= { GPIOD_1 };
static const unsigned int i2cm_b_scl_pins[]		= { GPIOD_2 };
static const unsigned int i2cm_b_sda_pins[]		= { GPIOD_3 };
static const unsigned int pio2_pins[]			= { GPIOD_4 };
static const unsigned int pdm_dclk_pins[]		= { GPIOD_5 };
static const unsigned int pdm_din0_pins[]		= { GPIOD_6 };

/* Bank[D]:Function[4] */
static const unsigned int pio0_pins[]			= { GPIOD_0 };
static const unsigned int pio1_pins[]			= { GPIOD_1 };
static const unsigned int pwm_i_pins[]			= { GPIOD_2 };
static const unsigned int pwm_j_pins[]			= { GPIOD_3 };
static const unsigned int i2cm_d_sda_pins[]		= { GPIOD_4 };
static const unsigned int i2cm_d_scl_pins[]		= { GPIOD_5 };

/* Bank[D]:Function[5] */
static const unsigned int i2cs_a_scl_pins[]		= { GPIOD_0 };
static const unsigned int i2cs_a_sda_pins[]		= { GPIOD_1 };
static const unsigned int tdm_fs0_pins[]		= { GPIOD_2 };
static const unsigned int tdm_sclk0_pins[]		= { GPIOD_3 };
static const unsigned int mclk0_pins[]			= { GPIOD_4 };
static const unsigned int tdm_d1_pins[]			= { GPIOD_5 };
static const unsigned int tdm_d0_pins[]			= { GPIOD_6 };

/* Bank[D]:Function[6] */

/* Bank[D]:Function[7] */
static const unsigned int gen_clk_pins[]		= { GPIOD_1 };
static const unsigned int jtag_a_clk_pins[]		= { GPIOD_2 };
static const unsigned int jtag_a_tms_pins[]		= { GPIOD_3 };
static const unsigned int jtag_a_tdi_pins[]		= { GPIOD_4 };
static const unsigned int jtag_a_tdo_pins[]		= { GPIOD_5 };

/* Bank[E]:Function[1] */
static const unsigned int pwm_a_pins[]			= { GPIOE_0 };
static const unsigned int pwm_b_pins[]			= { GPIOE_1 };
static const unsigned int i2cm_c_sda_pins[]		= { GPIOE_2 };
static const unsigned int i2cm_c_scl_pins[]		= { GPIOE_3 };
static const unsigned int gen_clk_e_pins[]		= { GPIOE_4 };

/* Bank[E]:Function[2] */
static const unsigned int i2cm_a_scl_e_pins[]		= { GPIOE_0 };
static const unsigned int i2cm_a_sda_e_pins[]		= { GPIOE_1 };
static const unsigned int clk_32k_in_pins[]		= { GPIOE_4 };

/* Bank[E]:Function[3] */
static const unsigned int clk12_24_pins[]		= { GPIOE_4 };

/* Bank[E]:Function[4] */

/* Bank[E]:Function[5] */
static const unsigned int pio3_pins[]			= { GPIOE_0 };

/* Bank[E]:Function[6] */

/* Bank[E]:Function[7] */
static const unsigned int gen_clk_e0_pins[]		= { GPIOE_0 };
static const unsigned int pio4_pins[]			= { GPIOE_1 };
static const unsigned int pio5_pins[]			= { GPIOE_2 };
static const unsigned int pio6_pins[]			= { GPIOE_3 };
static const unsigned int pio7_pins[]			= { GPIOE_4 };

/* Bank[B]:Function[1] */
static const unsigned int emmc_d0_pins[]		= { GPIOB_0 };
static const unsigned int emmc_d1_pins[]		= { GPIOB_1 };
static const unsigned int emmc_d2_pins[]		= { GPIOB_2 };
static const unsigned int emmc_d3_pins[]		= { GPIOB_3 };
static const unsigned int emmc_d4_pins[]		= { GPIOB_4 };
static const unsigned int emmc_d5_pins[]		= { GPIOB_5 };
static const unsigned int emmc_d6_pins[]		= { GPIOB_6 };
static const unsigned int emmc_d7_pins[]		= { GPIOB_7 };
static const unsigned int emmc_clk_pins[]		= { GPIOB_8 };
static const unsigned int emmc_reset_pins[]		= { GPIOB_9 };
static const unsigned int emmc_cmd_pins[]		= { GPIOB_10 };
static const unsigned int emmc_ds_pins[]		= { GPIOB_11 };
static const unsigned int pio19_pins[]			= { GPIOB_12 };
static const unsigned int pio20_pins[]			= { GPIOB_13 };

/* Bank[B]:Function[2] */
static const unsigned int tdm_d3_pins[]			= { GPIOB_4 };
static const unsigned int tdm_d2_pins[]			= { GPIOB_5 };
static const unsigned int tdm_sclk1_pins[]		= { GPIOB_6 };
static const unsigned int tdm_fs1_pins[]		= { GPIOB_7 };
static const unsigned int mclk1_pins[]			= { GPIOB_8 };
static const unsigned int pio16_pins[]			= { GPIOB_9 };
static const unsigned int pio17_pins[]			= { GPIOB_10 };
static const unsigned int pio18_pins[]			= { GPIOB_11 };
static const unsigned int uart_c_tx_pins[]		= { GPIOB_12 };
static const unsigned int uart_c_rx_pins[]		= { GPIOB_13 };

/* Bank[B]:Function[3] */
static const unsigned int spinf_mo_d0_pins[]		= { GPIOB_0 };
static const unsigned int spinf_mi_d1_pins[]		= { GPIOB_1 };
static const unsigned int spinf_wp_d2_pins[]		= { GPIOB_2 };
static const unsigned int spinf_hold_d3_pins[]		= { GPIOB_3 };
static const unsigned int pio12_pins[]			= { GPIOB_4 };
static const unsigned int pio13_pins[]			= { GPIOB_5 };
static const unsigned int pio14_pins[]			= { GPIOB_6 };
static const unsigned int pio15_pins[]			= { GPIOB_7 };
static const unsigned int uart_e_tx_pins[]		= { GPIOB_8 };
static const unsigned int uart_e_rx_pins[]		= { GPIOB_9 };
static const unsigned int spinf_clk_pins[]		= { GPIOB_10 };
static const unsigned int uart_e_cts_pins[]		= { GPIOB_11 };
static const unsigned int uart_e_rts_pins[]		= { GPIOB_12 };
static const unsigned int spinf_cs0_pins[]		= { GPIOB_13 };

/* Bank[B]:Function[4] */
static const unsigned int pwm_g_b_pins[]		= { GPIOB_4 };
static const unsigned int pwm_h_b_pins[]		= { GPIOB_5 };
static const unsigned int pwm_c_pins[]			= { GPIOB_6 };
static const unsigned int pwm_f_b_pins[]		= { GPIOB_7 };
static const unsigned int pwm_k_b_pins[]		= { GPIOB_8 };
static const unsigned int pwm_l_pins[]			= { GPIOB_9 };
static const unsigned int pwm_m_pins[]			= { GPIOB_11 };
static const unsigned int pwm_n_pins[]			= { GPIOB_12 };
static const unsigned int pwm_m_b_pins[]		= { GPIOB_13 };

/* Bank[B]:Function[5] */
static const unsigned int spi_a_mosi_pins[]		= { GPIOB_4 };
static const unsigned int spi_a_miso_pins[]		= { GPIOB_5 };
static const unsigned int spi_a_sclk_pins[]		= { GPIOB_6 };
static const unsigned int spi_a_ss0_pins[]		= { GPIOB_7 };
static const unsigned int spi_a_ss1_pins[]		= { GPIOB_8 };
static const unsigned int spi_a_ss2_pins[]		= { GPIOB_9 };
static const unsigned int i2cm_b_scl_b_pins[]		= { GPIOB_11 };
static const unsigned int i2cm_b_sda_b_pins[]		= { GPIOB_12 };
static const unsigned int i2cm_b_scl_b13_pins[]		= { GPIOB_13 };

/* Bank[B]:Function[6] */
static const unsigned int uart_a_rx_b_pins[]		= { GPIOB_4 };
static const unsigned int uart_a_tx_b_pins[]		= { GPIOB_5 };
static const unsigned int uart_c_tx_b_pins[]		= { GPIOB_6 };
static const unsigned int uart_c_rx_b_pins[]		= { GPIOB_7 };
static const unsigned int i2cs_a_scl_b_pins[]		= { GPIOB_8 };
static const unsigned int i2cs_a_sda_b_pins[]		= { GPIOB_9 };
static const unsigned int pdm_dclk_b_pins[]		= { GPIOB_11 };
static const unsigned int pdm_din0_b_pins[]		= { GPIOB_12 };
static const unsigned int pdm_dclk_b13_pins[]		= { GPIOB_13 };

/* Bank[B]:Function[7] */
static const unsigned int pio8_pins[]			= { GPIOB_0 };
static const unsigned int pio9_pins[]			= { GPIOB_1 };
static const unsigned int pio10_pins[]			= { GPIOB_2 };
static const unsigned int pio11_pins[]			= { GPIOB_3 };
static const unsigned int gen_clk_b_pins[]		= { GPIOB_5 };
static const unsigned int i2cm_d_scl_b_pins[]		= { GPIOB_8 };
static const unsigned int i2cm_d_sda_b_pins[]		= { GPIOB_9 };

/* Bank[C]:Function[1] */
static const unsigned int sdcard_d0_pins[]		= { GPIOC_0 };
static const unsigned int sdcard_d1_pins[]		= { GPIOC_1 };
static const unsigned int sdcard_d2_pins[]		= { GPIOC_2 };
static const unsigned int sdcard_d3_pins[]		= { GPIOC_3 };
static const unsigned int sdcard_clk_pins[]		= { GPIOC_4 };
static const unsigned int sdcard_cmd_pins[]		= { GPIOC_5 };
static const unsigned int sdcard_det_pins[]		= { GPIOC_6 };

/* Bank[C]:Function[2] */
static const unsigned int jtag_b_tdo_pins[]		= { GPIOC_0 };
static const unsigned int jtag_b_tdi_pins[]		= { GPIOC_1 };
static const unsigned int uart_b_rx_pins[]		= { GPIOC_2 };
static const unsigned int uart_b_tx_pins[]		= { GPIOC_3 };
static const unsigned int jtag_b_clk_pins[]		= { GPIOC_4 };
static const unsigned int jtag_b_tms_pins[]		= { GPIOC_5 };
static const unsigned int gen_clk_c_pins[]		= { GPIOC_6 };

/* Bank[C]:Function[3] */
static const unsigned int tdm_d3_c_pins[]		= { GPIOC_0 };
static const unsigned int tdm_d2_c_pins[]		= { GPIOC_1 };
static const unsigned int mclk1_c_pins[]		= { GPIOC_2 };
static const unsigned int tdm_sclk1_c_pins[]		= { GPIOC_3 };
static const unsigned int tdm_fs1_c_pins[]		= { GPIOC_4 };
static const unsigned int pdm_dclk_c_pins[]		= { GPIOC_5 };
static const unsigned int pdm_din0_c_pins[]		= { GPIOC_6 };

/* Bank[C]:Function[4] */
static const unsigned int spi_a_mosi_c_pins[]		= { GPIOC_0 };
static const unsigned int spi_a_miso_c_pins[]		= { GPIOC_1 };
static const unsigned int spi_a_sclk_c_pins[]		= { GPIOC_2 };
static const unsigned int spi_a_ss0_c_pins[]		= { GPIOC_3 };
static const unsigned int spi_a_ss1_c_pins[]		= { GPIOC_4 };
static const unsigned int pio21_pins[]			= { GPIOC_5 };
static const unsigned int pio22_pins[]			= { GPIOC_6 };

/* Bank[C]:Function[5] */
static const unsigned int pwm_g_c_pins[]		= { GPIOC_0 };
static const unsigned int pwm_h_c_pins[]		= { GPIOC_1 };
static const unsigned int pwm_i_c_pins[]		= { GPIOC_2 };
static const unsigned int pwm_j_c_pins[]		= { GPIOC_3 };
static const unsigned int pwm_k_c_pins[]		= { GPIOC_4 };
static const unsigned int pwm_l_c_pins[]		= { GPIOC_5 };
static const unsigned int pwm_m_c_pins[]		= { GPIOC_6 };

/* Bank[C]:Function[6] */
static const unsigned int uart_a_rx_c_pins[]		= { GPIOC_0 };
static const unsigned int uart_a_tx_c_pins[]		= { GPIOC_1 };
static const unsigned int uart_c_rx_c_pins[]		= { GPIOC_2 };
static const unsigned int uart_c_tx_c_pins[]		= { GPIOC_3 };
static const unsigned int i2cm_d_scl_c_pins[]		= { GPIOC_4 };
static const unsigned int i2cm_d_sda_c_pins[]		= { GPIOC_5 };
static const unsigned int clk12_24_c_pins[]		= { GPIOC_6 };

/* Bank[C]:Function[7] */
static const unsigned int gen_clk_c0_pins[]		= { GPIOC_0 };

/* Bank[X]:Function[1] */
static const unsigned int sdio_d0_pins[]		= { GPIOX_0 };
static const unsigned int sdio_d1_pins[]		= { GPIOX_1 };
static const unsigned int sdio_d2_pins[]		= { GPIOX_2 };
static const unsigned int sdio_d3_pins[]		= { GPIOX_3 };
static const unsigned int sdio_clk_pins[]		= { GPIOX_4 };
static const unsigned int sdio_cmd_pins[]		= { GPIOX_5 };
static const unsigned int pwm_m_x_pins[]		= { GPIOX_6 };
static const unsigned int pwm_j_x_pins[]		= { GPIOX_7 };
static const unsigned int pwm_k_x_pins[]		= { GPIOX_8 };
static const unsigned int pwm_e_pins[]			= { GPIOX_9 };

/* Bank[X]:Function[2] */
static const unsigned int spi_a_mosi_x_pins[]		= { GPIOX_0 };
static const unsigned int spi_a_miso_x_pins[]		= { GPIOX_1 };
static const unsigned int spi_a_sclk_x_pins[]		= { GPIOX_2 };
static const unsigned int spi_a_ss0_x_pins[]		= { GPIOX_3 };
static const unsigned int spi_a_ss1_x_pins[]		= { GPIOX_4 };
static const unsigned int spi_a_ss2_x_pins[]		= { GPIOX_5 };
static const unsigned int gen_clk_x_pins[]		= { GPIOX_9 };

/* Bank[X]:Function[3] */
static const unsigned int tdm_d1_x_pins[]		= { GPIOX_0 };
static const unsigned int tdm_d0_x_pins[]		= { GPIOX_1 };
static const unsigned int mclk0_x_pins[]		= { GPIOX_2 };
static const unsigned int tdm_sclk0_x_pins[]		= { GPIOX_3 };
static const unsigned int tdm_fs0_x_pins[]		= { GPIOX_4 };
static const unsigned int pdm_dclk_x_pins[]		= { GPIOX_5 };
static const unsigned int pio23_pins[]			= { GPIOX_6 };

/* Bank[X]:Function[4] */

/* Bank[X]:Function[5] */
static const unsigned int pwm_g_x_pins[]		= { GPIOX_0 };
static const unsigned int pwm_h_x_pins[]		= { GPIOX_1 };
static const unsigned int pwm_i_x_pins[]		= { GPIOX_2 };
static const unsigned int pwm_j_x3_pins[]		= { GPIOX_3 };
static const unsigned int pwm_k_x4_pins[]		= { GPIOX_4 };
static const unsigned int pwm_l_x_pins[]		= { GPIOX_5 };
static const unsigned int spi_b_ss1_x_pins[]		= { GPIOX_6 };
static const unsigned int spi_b_mosi_x_pins[]		= { GPIOX_7 };
static const unsigned int spi_b_miso_x_pins[]		= { GPIOX_8 };
static const unsigned int spi_b_sclk_x_pins[]		= { GPIOX_9 };

/* Bank[X]:Function[6] */
static const unsigned int uart_a_rx_x_pins[]		= { GPIOX_0 };
static const unsigned int uart_a_tx_x_pins[]		= { GPIOX_1 };
static const unsigned int uart_c_rx_x_pins[]		= { GPIOX_2 };
static const unsigned int uart_c_tx_x_pins[]		= { GPIOX_3 };
static const unsigned int i2cm_d_scl_x_pins[]		= { GPIOX_4 };
static const unsigned int i2cm_d_sda_x_pins[]		= { GPIOX_5 };

/* Bank[X]:Function[7] */
static const unsigned int test_clk_1m_pins[]		= { GPIOX_6 };

/* Bank[M]:Function[1] */
static const unsigned int spi_a_mosi_m_pins[]		= { GPIOM_0 };
static const unsigned int spi_a_miso_m_pins[]		= { GPIOM_1 };
static const unsigned int spi_a_sclk_m_pins[]		= { GPIOM_2 };
static const unsigned int spi_a_ss0_m_pins[]		= { GPIOM_3 };
static const unsigned int pwm_k_m_pins[]		= { GPIOM_4 };
static const unsigned int clk12_24_m_pins[]		= { GPIOM_5 };

/* Bank[M]:Function[2] */
static const unsigned int pdm_din0_m_pins[]		= { GPIOM_0 };
static const unsigned int pdm_dclk_m_pins[]		= { GPIOM_1 };
static const unsigned int pwm_i_m_pins[]		= { GPIOM_2 };
static const unsigned int pwm_j_m_pins[]		= { GPIOM_3 };
static const unsigned int uart_f_tx_pins[]		= { GPIOM_4 };
static const unsigned int uart_f_rx_pins[]		= { GPIOM_5 };
static const unsigned int i2cm_b_scl_m_pins[]		= { GPIOM_6 };
static const unsigned int i2cm_b_sda_m_pins[]		= { GPIOM_7 };
static const unsigned int uart_c_tx_m_pins[]		= { GPIOM_8 };
static const unsigned int uart_c_rx_m_pins[]		= { GPIOM_9 };

/* Bank[M]:Function[3] */
static const unsigned int uart_a_rx_m_pins[]		= { GPIOM_0 };
static const unsigned int uart_a_tx_m_pins[]		= { GPIOM_1 };
static const unsigned int uart_c_tx_m2_pins[]		= { GPIOM_2 };
static const unsigned int uart_c_rx_m3_pins[]		= { GPIOM_3 };
static const unsigned int i2cm_d_scl_m_pins[]		= { GPIOM_4 };
static const unsigned int i2cm_d_sda_m_pins[]		= { GPIOM_5 };

/* Bank[M]:Function[4] */
static const unsigned int pio24_pins[]			= { GPIOM_3 };
static const unsigned int pio25_pins[]			= { GPIOM_4 };
static const unsigned int pio26_pins[]			= { GPIOM_5 };

/* Bank[M]:Function[6] */
static const unsigned int i2cm_b_scl_m0_pins[]		= { GPIOM_0 };
static const unsigned int i2cm_b_sda_m1_pins[]		= { GPIOM_1 };
static const unsigned int gen_clk_m_pins[]		= { GPIOM_2 };

/* Bank[M]:Function[7] */
static const unsigned int gen_clk_m6_pins[]		= { GPIOM_6 };

/* Bank[A]:Function[1] */
static const unsigned int uart_b_tx_a_pins[]		= { GPIOA_0 };
static const unsigned int uart_b_rx_a_pins[]		= { GPIOA_1 };

/* Bank[A]:Function[2] */
static const unsigned int pwm_m_a_pins[]		= { GPIOA_0 };
static const unsigned int pwm_j_a_pins[]		= { GPIOA_1 };

/* Bank[A]:Function[4] */
static const unsigned int pio27_pins[]			= { GPIOA_0 };
static const unsigned int pio28_pins[]			= { GPIOA_1 };

/* Bank[A]:Function[6] */

/* Bank[AO]:Function[1] */
static const unsigned int pwm_c_ao0_pins[]		= { GPIOAO_0 };
static const unsigned int pwm_l_ao_pins[]		= { GPIOAO_1 };
static const unsigned int i2cm_b_sda_ao2_pins[]		= { GPIOAO_2 };
static const unsigned int i2cm_b_scl_ao3_pins[]		= { GPIOAO_3 };
static const unsigned int pwm_j_ao_pins[]		= { GPIOAO_4 };
static const unsigned int pwm_k_ao_pins[]		= { GPIOAO_5 };
static const unsigned int pwm_e_ao_pins[]		= { GPIOAO_6 };
static const unsigned int pwm_k_ao_ao_pins[]		= { GPIOAO_7 };
static const unsigned int uart_e_tx_ao10_pins[]		= { GPIOAO_10 };
static const unsigned int uart_e_rx_ao11_pins[]		= { GPIOAO_11 };
static const unsigned int uart_e_cts_ao12_pins[]	= { GPIOAO_12 };
static const unsigned int uart_e_rts_ao13_pins[]	= { GPIOAO_13 };

/* Bank[AO]:Function[2] */
static const unsigned int uart_e_tx_ao0_pins[]		= { GPIOAO_0 };
static const unsigned int uart_e_rx_ao1_pins[]		= { GPIOAO_1 };
static const unsigned int uart_f_tx_ao2_pins[]		= { GPIOAO_2 };
static const unsigned int uart_f_rx_ao3_pins[]		= { GPIOAO_3 };
static const unsigned int spi_b_miso_ao10_pins[]	= { GPIOAO_10 };
static const unsigned int spi_b_sclk_ao11_pins[]	= { GPIOAO_11 };
static const unsigned int spi_b_mosi_ao12_pins[]	= { GPIOAO_12 };
static const unsigned int spi_b_ss0_ao13_pins[]		= { GPIOAO_13 };

/* Bank[AO]:Function[3] */
static const unsigned int i2cm_a_scl_ao0_pins[]		= { GPIOAO_0 };
static const unsigned int i2cm_a_sda_ao1_pins[]		= { GPIOAO_1 };
static const unsigned int pwm_m_ao_pins[]		= { GPIOAO_2 };
static const unsigned int pwm_n_ao3_pins[]		= { GPIOAO_3 };
static const unsigned int spi_a_mosi_ao4_pins[]		= { GPIOAO_4 };
static const unsigned int spi_a_miso_ao5_pins[]		= { GPIOAO_5 };
static const unsigned int spi_a_sclk_ao6_pins[]		= { GPIOAO_6 };
static const unsigned int spi_a_ss0_ao7_pins[]		= { GPIOAO_7 };
static const unsigned int pwm_b_ao_pins[]		= { GPIOAO_8 };
static const unsigned int pwm_c_ao9_pins[]		= { GPIOAO_9 };
static const unsigned int pdm_din0_ao12_pins[]		= { GPIOAO_12 };
static const unsigned int pdm_dclk_ao13_pins[]		= { GPIOAO_13 };

/* Bank[AO]:Function[4] */
static const unsigned int rtc32k_out_ao_pins[]		= { GPIOAO_3 };

/* Bank[AO]:Function[5] */
static const unsigned int pwr_up_ao_pins[]		= { GPIOAO_0 };
static const unsigned int str_en_ao_pins[]		= { GPIOAO_1 };
static const unsigned int clk_32k_in_ao_pins[]		= { GPIOAO_3 };
static const unsigned int i2cm_c_scl_ao6_pins[]		= { GPIOAO_6 };
static const unsigned int i2cm_c_sda_ao7_pins[]		= { GPIOAO_7 };
static const unsigned int pwm_n_ao10_pins[]		= { GPIOAO_10 };
static const unsigned int pwm_g_ao_pins[]		= { GPIOAO_11 };
static const unsigned int pwm_h_ao_pins[]		= { GPIOAO_12 };
static const unsigned int pwm_i_ao_pins[]		= { GPIOAO_13 };

/* Bank[AO]:Function[6] */
static const unsigned int pdm_dclk_ao4_pins[]		= { GPIOAO_4 };
static const unsigned int pdm_din0_ao5_pins[]		= { GPIOAO_5 };
static const unsigned int i2cm_b_sda_ao10_pins[]	= { GPIOAO_10 };
static const unsigned int i2cm_b_scl_ao11_pins[]	= { GPIOAO_11 };
static const unsigned int uart_d_tx_pins[]		= { GPIOAO_12 };
static const unsigned int uart_d_rx_pins[]		= { GPIOAO_13 };

/* Bank[AO]:Function[7] */
static const unsigned int gen_clk_ao1_pins[]		= { GPIOAO_1 };
static const unsigned int gen_clk_ao10_pins[]		= { GPIOAO_10 };

static struct meson_pmx_group meson_c4_periphs_groups[] __initdata = {
	/* GPIOD */
	GPIO_GROUP(GPIOD_0),
	GPIO_GROUP(GPIOD_1),
	GPIO_GROUP(GPIOD_2),
	GPIO_GROUP(GPIOD_3),
	GPIO_GROUP(GPIOD_4),
	GPIO_GROUP(GPIOD_5),
	GPIO_GROUP(GPIOD_6),
	/* GPIOE */
	GPIO_GROUP(GPIOE_0),
	GPIO_GROUP(GPIOE_1),
	GPIO_GROUP(GPIOE_2),
	GPIO_GROUP(GPIOE_3),
	GPIO_GROUP(GPIOE_4),
	/* GPIOB */
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
	/* GPIOC */
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
	GPIO_GROUP(GPIOC_6),
	/* GPIOX */
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
	/* GPIOM */
	GPIO_GROUP(GPIOM_0),
	GPIO_GROUP(GPIOM_1),
	GPIO_GROUP(GPIOM_2),
	GPIO_GROUP(GPIOM_3),
	GPIO_GROUP(GPIOM_4),
	GPIO_GROUP(GPIOM_5),
	GPIO_GROUP(GPIOM_6),
	GPIO_GROUP(GPIOM_7),
	GPIO_GROUP(GPIOM_8),
	GPIO_GROUP(GPIOM_9),
	/* GPIOA */
	GPIO_GROUP(GPIOA_0),
	GPIO_GROUP(GPIOA_1),
	/* Bank[D]:Function[1] */
	GROUP(pwm_g,			1),
	GROUP(pwm_h,			1),
	GROUP(eth_led_act,		1),
	GROUP(eth_led_link,		1),
	GROUP(pwm_d,			1),
	GROUP(pwm_f,			1),
	GROUP(pwm_k,			1),
	/* Bank[D]:Function[2] */
	GROUP(uart_a_tx,		2),
	GROUP(uart_a_rx,		2),
	GROUP(spi_b_miso,		2),
	GROUP(spi_b_mosi,		2),
	GROUP(spi_b_sclk,		2),
	GROUP(spi_b_ss1,		2),
	GROUP(spi_b_ss0,		2),
	/* Bank[D]:Function[3] */
	GROUP(i2cm_a_scl,		3),
	GROUP(i2cm_a_sda,		3),
	GROUP(i2cm_b_scl,		3),
	GROUP(i2cm_b_sda,		3),
	GROUP(pio2,			3),
	GROUP(pdm_dclk,			3),
	GROUP(pdm_din0,			3),
	/* Bank[D]:Function[4] */
	GROUP(pio0,			4),
	GROUP(pio1,			4),
	GROUP(pwm_i,			4),
	GROUP(pwm_j,			4),
	GROUP(i2cm_d_sda,		4),
	GROUP(i2cm_d_scl,		4),
	/* Bank[D]:Function[5] */
	GROUP(i2cs_a_scl,		5),
	GROUP(i2cs_a_sda,		5),
	GROUP(tdm_fs0,			5),
	GROUP(tdm_sclk0,		5),
	GROUP(mclk0,			5),
	GROUP(tdm_d1,			5),
	GROUP(tdm_d0,			5),
	/* Bank[D]:Function[6] */
	/* Bank[D]:Function[7] */
	GROUP(gen_clk,			7),
	GROUP(jtag_a_clk,		7),
	GROUP(jtag_a_tms,		7),
	GROUP(jtag_a_tdi,		7),
	GROUP(jtag_a_tdo,		7),
	/* Bank[E]:Function[1] */
	GROUP(pwm_a,			1),
	GROUP(pwm_b,			1),
	GROUP(i2cm_c_sda,		1),
	GROUP(i2cm_c_scl,		1),
	GROUP(gen_clk_e,		1),
	/* Bank[E]:Function[2] */
	GROUP(i2cm_a_scl_e,		2),
	GROUP(i2cm_a_sda_e,		2),
	GROUP(clk_32k_in,		2),
	/* Bank[E]:Function[3] */
	GROUP(clk12_24,			3),
	/* Bank[E]:Function[4] */
	/* Bank[E]:Function[5] */
	GROUP(pio3,			5),
	/* Bank[E]:Function[6] */
	/* Bank[E]:Function[7] */
	GROUP(gen_clk_e0,		7),
	GROUP(pio4,			7),
	GROUP(pio5,			7),
	GROUP(pio6,			7),
	GROUP(pio7,			7),
	/* Bank[B]:Function[1] */
	GROUP(emmc_d0,			1),
	GROUP(emmc_d1,			1),
	GROUP(emmc_d2,			1),
	GROUP(emmc_d3,			1),
	GROUP(emmc_d4,			1),
	GROUP(emmc_d5,			1),
	GROUP(emmc_d6,			1),
	GROUP(emmc_d7,			1),
	GROUP(emmc_clk,			1),
	GROUP(emmc_reset,		1),
	GROUP(emmc_cmd,			1),
	GROUP(emmc_ds,			1),
	GROUP(pio19,			1),
	GROUP(pio20,			1),
	/* Bank[B]:Function[2] */
	GROUP(tdm_d3,			2),
	GROUP(tdm_d2,			2),
	GROUP(tdm_sclk1,		2),
	GROUP(tdm_fs1,			2),
	GROUP(mclk1,			2),
	GROUP(pio16,			2),
	GROUP(pio17,			2),
	GROUP(pio18,			2),
	GROUP(uart_c_tx,		2),
	GROUP(uart_c_rx,		2),
	/* Bank[B]:Function[3] */
	GROUP(spinf_mo_d0,		3),
	GROUP(spinf_mi_d1,		3),
	GROUP(spinf_wp_d2,		3),
	GROUP(spinf_hold_d3,		3),
	GROUP(pio12,			3),
	GROUP(pio13,			3),
	GROUP(pio14,			3),
	GROUP(pio15,			3),
	GROUP(uart_e_tx,		3),
	GROUP(uart_e_rx,		3),
	GROUP(spinf_clk,		3),
	GROUP(uart_e_cts,		3),
	GROUP(uart_e_rts,		3),
	GROUP(spinf_cs0,		3),
	/* Bank[B]:Function[4] */
	GROUP(pwm_g_b,			4),
	GROUP(pwm_h_b,			4),
	GROUP(pwm_c,			4),
	GROUP(pwm_f_b,			4),
	GROUP(pwm_k_b,			4),
	GROUP(pwm_l,			4),
	GROUP(pwm_m,			4),
	GROUP(pwm_n,			4),
	GROUP(pwm_m_b,			4),
	/* Bank[B]:Function[5] */
	GROUP(spi_a_mosi,		5),
	GROUP(spi_a_miso,		5),
	GROUP(spi_a_sclk,		5),
	GROUP(spi_a_ss0,		5),
	GROUP(spi_a_ss1,		5),
	GROUP(spi_a_ss2,		5),
	GROUP(i2cm_b_scl_b,		5),
	GROUP(i2cm_b_sda_b,		5),
	GROUP(i2cm_b_scl_b13,		5),
	/* Bank[B]:Function[6] */
	GROUP(uart_a_rx_b,		6),
	GROUP(uart_a_tx_b,		6),
	GROUP(uart_c_tx_b,		6),
	GROUP(uart_c_rx_b,		6),
	GROUP(i2cs_a_scl_b,		6),
	GROUP(i2cs_a_sda_b,		6),
	GROUP(pdm_dclk_b,		6),
	GROUP(pdm_din0_b,		6),
	GROUP(pdm_dclk_b13,		6),
	/* Bank[B]:Function[7] */
	GROUP(pio8,			7),
	GROUP(pio9,			7),
	GROUP(pio10,			7),
	GROUP(pio11,			7),
	GROUP(gen_clk_b,		7),
	GROUP(i2cm_d_scl_b,		7),
	GROUP(i2cm_d_sda_b,		7),
	/* Bank[C]:Function[1] */
	GROUP(sdcard_d0,		1),
	GROUP(sdcard_d1,		1),
	GROUP(sdcard_d2,		1),
	GROUP(sdcard_d3,		1),
	GROUP(sdcard_clk,		1),
	GROUP(sdcard_cmd,		1),
	GROUP(sdcard_det,		1),
	/* Bank[C]:Function[2] */
	GROUP(jtag_b_tdo,		2),
	GROUP(jtag_b_tdi,		2),
	GROUP(uart_b_rx,		2),
	GROUP(uart_b_tx,		2),
	GROUP(jtag_b_clk,		2),
	GROUP(jtag_b_tms,		2),
	GROUP(gen_clk_c,		2),
	/* Bank[C]:Function[3] */
	GROUP(tdm_d3_c,			3),
	GROUP(tdm_d2_c,			3),
	GROUP(mclk1_c,			3),
	GROUP(tdm_sclk1_c,		3),
	GROUP(tdm_fs1_c,		3),
	GROUP(pdm_dclk_c,		3),
	GROUP(pdm_din0_c,		3),
	/* Bank[C]:Function[4] */
	GROUP(spi_a_mosi_c,		4),
	GROUP(spi_a_miso_c,		4),
	GROUP(spi_a_sclk_c,		4),
	GROUP(spi_a_ss0_c,		4),
	GROUP(spi_a_ss1_c,		4),
	GROUP(pio21,			4),
	GROUP(pio22,			4),
	/* Bank[C]:Function[5] */
	GROUP(pwm_g_c,			5),
	GROUP(pwm_h_c,			5),
	GROUP(pwm_i_c,			5),
	GROUP(pwm_j_c,			5),
	GROUP(pwm_k_c,			5),
	GROUP(pwm_l_c,			5),
	GROUP(pwm_m_c,			5),
	/* Bank[C]:Function[6] */
	GROUP(uart_a_rx_c,		6),
	GROUP(uart_a_tx_c,		6),
	GROUP(uart_c_rx_c,		6),
	GROUP(uart_c_tx_c,		6),
	GROUP(i2cm_d_scl_c,		6),
	GROUP(i2cm_d_sda_c,		6),
	GROUP(clk12_24_c,		6),
	/* Bank[C]:Function[7] */
	GROUP(gen_clk_c0,		7),
	/* Bank[X]:Function[1] */
	GROUP(sdio_d0,			1),
	GROUP(sdio_d1,			1),
	GROUP(sdio_d2,			1),
	GROUP(sdio_d3,			1),
	GROUP(sdio_clk,			1),
	GROUP(sdio_cmd,			1),
	GROUP(pwm_m_x,			1),
	GROUP(pwm_j_x,			1),
	GROUP(pwm_k_x,			1),
	GROUP(pwm_e,			1),
	/* Bank[X]:Function[2] */
	GROUP(spi_a_mosi_x,		2),
	GROUP(spi_a_miso_x,		2),
	GROUP(spi_a_sclk_x,		2),
	GROUP(spi_a_ss0_x,		2),
	GROUP(spi_a_ss1_x,		2),
	GROUP(spi_a_ss2_x,		2),
	GROUP(gen_clk_x,		2),
	/* Bank[X]:Function[3] */
	GROUP(tdm_d1_x,			3),
	GROUP(tdm_d0_x,			3),
	GROUP(mclk0_x,			3),
	GROUP(tdm_sclk0_x,		3),
	GROUP(tdm_fs0_x,		3),
	GROUP(pdm_dclk_x,		3),
	GROUP(pio23,			3),
	/* Bank[X]:Function[4] */
	/* Bank[X]:Function[5] */
	GROUP(pwm_g_x,			5),
	GROUP(pwm_h_x,			5),
	GROUP(pwm_i_x,			5),
	GROUP(pwm_j_x3,			5),
	GROUP(pwm_k_x4,			5),
	GROUP(pwm_l_x,			5),
	GROUP(spi_b_ss1_x,		5),
	GROUP(spi_b_mosi_x,		5),
	GROUP(spi_b_miso_x,		5),
	GROUP(spi_b_sclk_x,		5),
	/* Bank[X]:Function[6] */
	GROUP(uart_a_rx_x,		6),
	GROUP(uart_a_tx_x,		6),
	GROUP(uart_c_rx_x,		6),
	GROUP(uart_c_tx_x,		6),
	GROUP(i2cm_d_scl_x,		6),
	GROUP(i2cm_d_sda_x,		6),
	/* Bank[X]:Function[7] */
	GROUP(test_clk_1m,		7),
	/* Bank[M]:Function[1] */
	GROUP(spi_a_mosi_m,		1),
	GROUP(spi_a_miso_m,		1),
	GROUP(spi_a_sclk_m,		1),
	GROUP(spi_a_ss0_m,		1),
	GROUP(pwm_k_m,			1),
	GROUP(clk12_24_m,		1),
	/* Bank[M]:Function[2] */
	GROUP(pdm_din0_m,		2),
	GROUP(pdm_dclk_m,		2),
	GROUP(pwm_i_m,			2),
	GROUP(pwm_j_m,			2),
	GROUP(uart_f_tx,		2),
	GROUP(uart_f_rx,		2),
	GROUP(i2cm_b_scl_m,		2),
	GROUP(i2cm_b_sda_m,		2),
	GROUP(uart_c_tx_m,		2),
	GROUP(uart_c_rx_m,		2),
	/* Bank[M]:Function[3] */
	GROUP(uart_a_rx_m,		3),
	GROUP(uart_a_tx_m,		3),
	GROUP(uart_c_tx_m2,		3),
	GROUP(uart_c_rx_m3,		3),
	GROUP(i2cm_d_scl_m,		3),
	GROUP(i2cm_d_sda_m,		3),
	/* Bank[M]:Function[4] */
	GROUP(pio24,			4),
	GROUP(pio25,			4),
	GROUP(pio26,			4),
	/* Bank[M]:Function[6] */
	GROUP(i2cm_b_scl_m0,		6),
	GROUP(i2cm_b_sda_m1,		6),
	GROUP(gen_clk_m,		6),
	/* Bank[M]:Function[7] */
	GROUP(gen_clk_m6,		7),
	/* Bank[A]:Function[1] */
	GROUP(uart_b_tx_a,		1),
	GROUP(uart_b_rx_a,		1),
	/* Bank[A]:Function[2] */
	GROUP(pwm_m_a,			2),
	GROUP(pwm_j_a,			2),
	/* Bank[A]:Function[4] */
	GROUP(pio27,			4),
	GROUP(pio28,			4),
	/* Bank[A]:Function[6] */
};

static struct meson_pmx_group meson_c4_aobus_groups[] __initdata = {
	/* GPIOAO */
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
	/* Bank[AO]:Function[1] */
	GROUP(pwm_c_ao0,		1),
	GROUP(pwm_l_ao,			1),
	GROUP(i2cm_b_sda_ao2,		1),
	GROUP(i2cm_b_scl_ao3,		1),
	GROUP(pwm_j_ao,			1),
	GROUP(pwm_k_ao,			1),
	GROUP(pwm_e_ao,			1),
	GROUP(pwm_k_ao_ao,		1),
	GROUP(uart_e_tx_ao10,		1),
	GROUP(uart_e_rx_ao11,		1),
	GROUP(uart_e_cts_ao12,		1),
	GROUP(uart_e_rts_ao13,		1),
	/* Bank[AO]:Function[2] */
	GROUP(uart_e_tx_ao0,		2),
	GROUP(uart_e_rx_ao1,		2),
	GROUP(uart_f_tx_ao2,		2),
	GROUP(uart_f_rx_ao3,		2),
	GROUP(spi_b_miso_ao10,		2),
	GROUP(spi_b_sclk_ao11,		2),
	GROUP(spi_b_mosi_ao12,		2),
	GROUP(spi_b_ss0_ao13,		2),
	/* Bank[AO]:Function[3] */
	GROUP(i2cm_a_scl_ao0,		3),
	GROUP(i2cm_a_sda_ao1,		3),
	GROUP(pwm_m_ao,			3),
	GROUP(pwm_n_ao3,		3),
	GROUP(spi_a_mosi_ao4,		3),
	GROUP(spi_a_miso_ao5,		3),
	GROUP(spi_a_sclk_ao6,		3),
	GROUP(spi_a_ss0_ao7,		3),
	GROUP(pwm_b_ao,			3),
	GROUP(pwm_c_ao9,		3),
	GROUP(pdm_din0_ao12,		3),
	GROUP(pdm_dclk_ao13,		3),
	/* Bank[AO]:Function[4] */
	GROUP(rtc32k_out_ao,		4),
	/* Bank[AO]:Function[5] */
	GROUP(pwr_up_ao,		5),
	GROUP(str_en_ao,		5),
	GROUP(clk_32k_in_ao,		5),
	GROUP(i2cm_c_scl_ao6,		5),
	GROUP(i2cm_c_sda_ao7,		5),
	GROUP(pwm_n_ao10,		5),
	GROUP(pwm_g_ao,			5),
	GROUP(pwm_h_ao,			5),
	GROUP(pwm_i_ao,			5),
	/* Bank[AO]:Function[6] */
	GROUP(pdm_dclk_ao4,		6),
	GROUP(pdm_din0_ao5,		6),
	GROUP(i2cm_b_sda_ao10,		6),
	GROUP(i2cm_b_scl_ao11,		6),
	GROUP(uart_d_tx,		6),
	GROUP(uart_d_rx,		6),
	/* Bank[AO]:Function[7] */
	GROUP(gen_clk_ao1,		7),
	GROUP(gen_clk_ao10,		7)
};

static struct meson_pmx_group meson_c4_testn_groups[] __initdata = {
	GPIO_GROUP(GPIO_TEST_N)
};

static const char * const gpio_periphs_groups[] = {
	"GPIOA_0", "GPIOA_1",  "GPIOB_0",  "GPIOB_1",  "GPIOB_2",
	"GPIOB_3", "GPIOB_4",  "GPIOB_5",  "GPIOB_6",  "GPIOB_7",
	"GPIOB_8", "GPIOB_9",  "GPIOC_0",  "GPIOC_1",  "GPIOC_2",
	"GPIOC_3", "GPIOC_4",  "GPIOC_5",  "GPIOC_6",  "GPIOD_0",
	"GPIOD_1", "GPIOD_2",  "GPIOD_3",  "GPIOD_4",  "GPIOD_5",
	"GPIOD_6", "GPIOE_0",  "GPIOE_1",  "GPIOE_2",  "GPIOE_3",
	"GPIOE_4", "GPIOM_0",  "GPIOM_1",  "GPIOM_2",  "GPIOM_3",
	"GPIOM_4", "GPIOM_5",  "GPIOM_6",  "GPIOM_7",  "GPIOM_8",
	"GPIOM_9", "GPIOX_0",  "GPIOX_1",  "GPIOX_2",  "GPIOX_3",
	"GPIOX_4", "GPIOX_5",  "GPIOX_6",  "GPIOX_7",  "GPIOX_8",
	"GPIOX_9", "GPIOB_10", "GPIOB_11", "GPIOB_12", "GPIOB_13"
};

static const char * const clk12_24_groups[] = {
	"clk12_24", "clk12_24_c", "clk12_24_m"
};

static const char * const clk_32k_in_groups[] = {
	"clk_32k_in"
};

static const char * const emmc_groups[] = {
	"emmc_d0", "emmc_d1",  "emmc_d2",  "emmc_d3",
	"emmc_d4", "emmc_d5",  "emmc_d6",  "emmc_d7",
	"emmc_ds", "emmc_clk", "emmc_cmd", "emmc_reset"
};

static const char * const eth_groups[] = {
	"eth_led_act", "eth_led_link"
};

static const char * const gen_clk_groups[] = {
	"gen_clk",    "gen_clk_b", "gen_clk_c",  "gen_clk_e",
	"gen_clk_m",  "gen_clk_x", "gen_clk_c0", "gen_clk_e0",
	"gen_clk_m6"
};

static const char * const i2c0_groups[] = {
	"i2cm_a_scl", "i2cm_a_sda", "i2cm_a_scl_e", "i2cm_a_sda_e"
};

static const char * const i2c1_groups[] = {
	"i2cm_b_scl",     "i2cm_b_sda",   "i2cm_b_scl_b",  "i2cm_b_scl_m",
	"i2cm_b_sda_b",   "i2cm_b_sda_m", "i2cm_b_scl_m0", "i2cm_b_sda_m1",
	"i2cm_b_scl_b13"
};

static const char * const i2c2_groups[] = {
	"i2cm_c_scl", "i2cm_c_sda"
};

static const char * const i2c3_groups[] = {
	"i2cm_d_scl",   "i2cm_d_sda",   "i2cm_d_scl_b", "i2cm_d_scl_c",
	"i2cm_d_scl_m", "i2cm_d_scl_x", "i2cm_d_sda_b", "i2cm_d_sda_c",
	"i2cm_d_sda_m", "i2cm_d_sda_x"
};

static const char * const i2c_slave_groups[] = {
	"i2cs_a_scl", "i2cs_a_sda", "i2cs_a_scl_b", "i2cs_a_sda_b"
};

static const char * const jtag_a_groups[] = {
	"jtag_a_clk", "jtag_a_tdi", "jtag_a_tdo", "jtag_a_tms"
};

static const char * const jtag_b_groups[] = {
	"jtag_b_clk", "jtag_b_tdi", "jtag_b_tdo", "jtag_b_tms"
};

static const char * const mclk_groups[] = {
	"mclk0", "mclk1", "mclk0_x", "mclk1_c"
};

static const char * const pdm_groups[] = {
	"pdm_dclk",   "pdm_din0",     "pdm_dclk_b", "pdm_dclk_c",
	"pdm_dclk_m", "pdm_dclk_x",   "pdm_din0_b", "pdm_din0_c",
	"pdm_din0_m", "pdm_dclk_b13"
};

static const char * const pio_groups[] = {
	"pio0",  "pio1",  "pio2",  "pio3",  "pio4",  "pio5",
	"pio6",  "pio7",  "pio8",  "pio9",  "pio10", "pio11",
	"pio12", "pio13", "pio14", "pio15", "pio16", "pio17",
	"pio18", "pio19", "pio20", "pio21", "pio22", "pio23",
	"pio24", "pio25", "pio26", "pio27", "pio28"
};

static const char * const pwm_a_groups[] = {
	"pwm_a"
};

static const char * const pwm_b_groups[] = {
	"pwm_b"
};

static const char * const pwm_c_groups[] = {
	"pwm_c"
};

static const char * const pwm_d_groups[] = {
	"pwm_d"
};

static const char * const pwm_e_groups[] = {
	"pwm_e"
};

static const char * const pwm_f_groups[] = {
	"pwm_f", "pwm_f_b"
};

static const char * const pwm_g_groups[] = {
	"pwm_g", "pwm_g_b", "pwm_g_c", "pwm_g_x"
};

static const char * const pwm_h_groups[] = {
	"pwm_h", "pwm_h_b", "pwm_h_c", "pwm_h_x"
};

static const char * const pwm_i_groups[] = {
	"pwm_i", "pwm_i_c", "pwm_i_m", "pwm_i_x"
};

static const char * const pwm_j_groups[] = {
	"pwm_j",    "pwm_j_a", "pwm_j_c", "pwm_j_m", "pwm_j_x",
	"pwm_j_x3"
};

static const char * const pwm_k_groups[] = {
	"pwm_k",    "pwm_k_b", "pwm_k_c", "pwm_k_m", "pwm_k_x",
	"pwm_k_x4"
};

static const char * const pwm_l_groups[] = {
	"pwm_l", "pwm_l_c", "pwm_l_x"
};

static const char * const pwm_m_groups[] = {
	"pwm_m", "pwm_m_a", "pwm_m_b", "pwm_m_c", "pwm_m_x"
};

static const char * const pwm_n_groups[] = {
	"pwm_n"
};

static const char * const sdcard_groups[] = {
	"sdcard_d0",  "sdcard_d1",  "sdcard_d2",  "sdcard_d3",
	"sdcard_clk", "sdcard_cmd", "sdcard_det"
};

static const char * const sdio_groups[] = {
	"sdio_d0",  "sdio_d1", "sdio_d2", "sdio_d3", "sdio_clk",
	"sdio_cmd"
};

static const char * const spi_a_groups[] = {
	"spi_a_ss0",    "spi_a_ss1",    "spi_a_ss2",    "spi_a_miso",
	"spi_a_mosi",   "spi_a_sclk",   "spi_a_ss0_c",  "spi_a_ss0_m",
	"spi_a_ss0_x",  "spi_a_ss1_c",  "spi_a_ss1_x",  "spi_a_ss2_x",
	"spi_a_miso_c", "spi_a_miso_m", "spi_a_miso_x", "spi_a_mosi_c",
	"spi_a_mosi_m", "spi_a_mosi_x", "spi_a_sclk_c", "spi_a_sclk_m",
	"spi_a_sclk_x"
};

static const char * const spi_b_groups[] = {
	"spi_b_ss0",    "spi_b_ss1",   "spi_b_miso",   "spi_b_mosi",
	"spi_b_sclk",   "spi_b_ss1_x", "spi_b_miso_x", "spi_b_mosi_x",
	"spi_b_sclk_x"
};

static const char * const spinf_groups[] = {
	"spinf_clk",   "spinf_cs0",     "spinf_mi_d1", "spinf_mo_d0",
	"spinf_wp_d2", "spinf_hold_d3"
};

static const char * const tdm_groups[] = {
	"tdm_d0",    "tdm_d1",    "tdm_d2",      "tdm_d3",
	"tdm_fs0",   "tdm_fs1",   "tdm_d0_x",    "tdm_d1_x",
	"tdm_d2_c",  "tdm_d3_c",  "tdm_fs0_x",   "tdm_fs1_c",
	"tdm_sclk0", "tdm_sclk1", "tdm_sclk0_x", "tdm_sclk1_c"
};

static const char * const uart_a_groups[] = {
	"uart_a_rx",   "uart_a_tx",   "uart_a_rx_b", "uart_a_rx_c",
	"uart_a_rx_m", "uart_a_rx_x", "uart_a_tx_b", "uart_a_tx_c",
	"uart_a_tx_m", "uart_a_tx_x"
};

static const char * const uart_b_groups[] = {
	"uart_b_rx", "uart_b_tx", "uart_b_rx_a", "uart_b_tx_a"
};

static const char * const uart_c_groups[] = {
	"uart_c_rx",   "uart_c_tx",   "uart_c_rx_b",  "uart_c_rx_c",
	"uart_c_rx_m", "uart_c_rx_x", "uart_c_tx_b",  "uart_c_tx_c",
	"uart_c_tx_m", "uart_c_tx_x", "uart_c_rx_m3", "uart_c_tx_m2"
};

static const char * const uart_e_groups[] = {
	"uart_e_rx", "uart_e_tx", "uart_e_cts", "uart_e_rts"
};

static const char * const uart_f_groups[] = {
	"uart_f_rx", "uart_f_tx"
};

static const char * const test_clk_groups[] = {
	"test_clk_1m"
};

static const char * const gpio_aobus_groups[] = {
	"GPIOAO_0",  "GPIOAO_1",  "GPIOAO_2",  "GPIOAO_3",  "GPIOAO_4",
	"GPIOAO_5",  "GPIOAO_6",  "GPIOAO_7",  "GPIOAO_8",  "GPIOAO_9",
	"GPIOAO_10", "GPIOAO_11", "GPIOAO_12", "GPIOAO_13"
};

static const char * const clk_32k_in_ao_groups[] = {
	"clk_32k_in_ao"
};

static const char * const gen_clk_ao_groups[] = {
	"gen_clk_ao1", "gen_clk_ao10"
};

static const char * const i2c0_ao_groups[] = {
	"i2cm_a_scl_ao0", "i2cm_a_sda_ao1"
};

static const char * const i2c1_ao_groups[] = {
	"i2cm_b_scl_ao3", "i2cm_b_sda_ao2", "i2cm_b_scl_ao11", "i2cm_b_sda_ao10"
};

static const char * const i2c2_ao_groups[] = {
	"i2cm_c_scl_ao6", "i2cm_c_sda_ao7"
};

static const char * const pdm_ao_groups[] = {
	"pdm_dclk_ao13", "pdm_din0_ao12", "pdm_dclk_ao4", "pdm_din0_ao5"
};

static const char * const pwm_b_ao_groups[] = {
	"pwm_b_ao"
};

static const char * const pwm_c_ao_groups[] = {
	"pwm_c_ao0", "pwm_c_ao9"
};

static const char * const pwm_e_ao_groups[] = {
	"pwm_e_ao"
};

static const char * const pwm_g_ao_groups[] = {
	"pwm_g_ao"
};

static const char * const pwm_h_ao_groups[] = {
	"pwm_h_ao"
};

static const char * const pwm_i_ao_groups[] = {
	"pwm_i_ao"
};

static const char * const pwm_j_ao_groups[] = {
	"pwm_j_ao"
};

static const char * const pwm_k_ao_groups[] = {
	"pwm_k_ao", "pwm_k_ao_ao"
};

static const char * const pwm_l_ao_groups[] = {
	"pwm_l_ao"
};

static const char * const pwm_m_ao_groups[] = {
	"pwm_m_ao"
};

static const char * const pwm_n_ao_groups[] = {
	"pwm_n_ao10", "pwm_n_ao3"
};

static const char * const pwr_up_ao_groups[] = {
	"pwr_up_ao"
};

static const char * const rtc32k_out_ao_groups[] = {
	"rtc32k_out_ao"
};

static const char * const spi_a_ao_groups[] = {
	"spi_a_ss0_ao7", "spi_a_miso_ao5", "spi_a_mosi_ao4", "spi_a_sclk_ao6"
};

static const char * const spi_b_ao_groups[] = {
	"spi_b_ss0_ao13", "spi_b_miso_ao10", "spi_b_mosi_ao12",
	"spi_b_sclk_ao11"
};

static const char * const str_en_ao_groups[] = {
	"str_en_ao"
};

static const char * const uart_d_ao_groups[] = {
	"uart_d_rx", "uart_d_tx"
};

static const char * const uart_e_ao_groups[] = {
	"uart_e_rx_ao11",    "uart_e_tx_ao10",    "uart_e_cts_ao12",
	"uart_e_rts_ao13",   "uart_e_rx_ao1", "uart_e_tx_ao0"
};

static const char * const uart_f_ao_groups[] = {
	"uart_f_rx_ao3", "uart_f_tx_ao2"
};

static const char * const gpio_testn_groups[] = {
	"GPIO_TEST_N"
};

static struct meson_pmx_func meson_c4_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(clk12_24),
	FUNCTION(clk_32k_in),
	FUNCTION(emmc),
	FUNCTION(eth),
	FUNCTION(gen_clk),
	FUNCTION(i2c0),
	FUNCTION(i2c1),
	FUNCTION(i2c2),
	FUNCTION(i2c3),
	FUNCTION(i2c_slave),
	FUNCTION(jtag_a),
	FUNCTION(jtag_b),
	FUNCTION(mclk),
	FUNCTION(pdm),
	FUNCTION(pio),
	FUNCTION(pwm_a),
	FUNCTION(pwm_b),
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
	FUNCTION(sdcard),
	FUNCTION(sdio),
	FUNCTION(spi_a),
	FUNCTION(spi_b),
	FUNCTION(spinf),
	FUNCTION(tdm),
	FUNCTION(uart_a),
	FUNCTION(uart_b),
	FUNCTION(uart_c),
	FUNCTION(uart_e),
	FUNCTION(uart_f),
	FUNCTION(test_clk)
};

static struct meson_pmx_func meson_c4_aobus_functions[] __initdata = {
	FUNCTION(gpio_aobus),
	FUNCTION(clk_32k_in_ao),
	FUNCTION(gen_clk_ao),
	FUNCTION(i2c0_ao),
	FUNCTION(i2c1_ao),
	FUNCTION(i2c2_ao),
	FUNCTION(pdm_ao),
	FUNCTION(pwm_b_ao),
	FUNCTION(pwm_c_ao),
	FUNCTION(pwm_e_ao),
	FUNCTION(pwm_g_ao),
	FUNCTION(pwm_h_ao),
	FUNCTION(pwm_i_ao),
	FUNCTION(pwm_j_ao),
	FUNCTION(pwm_k_ao),
	FUNCTION(pwm_l_ao),
	FUNCTION(pwm_m_ao),
	FUNCTION(pwm_n_ao),
	FUNCTION(pwr_up_ao),
	FUNCTION(rtc32k_out_ao),
	FUNCTION(spi_a_ao),
	FUNCTION(spi_b_ao),
	FUNCTION(str_en_ao),
	FUNCTION(uart_d_ao),
	FUNCTION(uart_e_ao),
	FUNCTION(uart_f_ao)
};

static struct meson_pmx_func meson_c4_testn_functions[] __initdata = {
	FUNCTION(gpio_testn)
};

static struct meson_bank meson_c4_periphs_banks[] = {
	/*    name   first   last   irq   pullen   pull   dir   out   in   ds */
	BANK_DS("D",    GPIOD_0,   GPIOD_6,  26,  32,
		0x033,  0, 0x034,  0, 0x032,  0, 0x031,  0, 0x030,  0, 0x037,  0),
	BANK_DS("E",    GPIOE_0,   GPIOE_4,  21,  25,
		0x043,  0, 0x044,  0, 0x042,  0, 0x041,  0, 0x040,  0, 0x047,  0),
	BANK_DS("B",    GPIOB_0,  GPIOB_13,   0,  13,
		0x063,  0, 0x064,  0, 0x062,  0, 0x061,  0, 0x060,  0, 0x067,  0),
	BANK_DS("C",    GPIOC_0,   GPIOC_6,  14,  20,
		0x053,  0, 0x054,  0, 0x052,  0, 0x051,  0, 0x050,  0, 0x057,  0),
	BANK_DS("X",    GPIOX_0,   GPIOX_9,  33,  42,
		0x013,  0, 0x014,  0, 0x012,  0, 0x011,  0, 0x010,  0, 0x017,  0),
	BANK_DS("M",    GPIOM_0,   GPIOM_9,  43,  52,
		0x003,  0, 0x004,  0, 0x002,  0, 0x001,  0, 0x000,  0, 0x007,  0),
	BANK_DS("A",    GPIOA_0,   GPIOA_1,  53,  54,
		0x073,  0, 0x074,  0, 0x072,  0, 0x071,  0, 0x070,  0, 0x077,  0)
};

static struct meson_pmx_bank meson_c4_periphs_pmx_banks[] = {
	/*       name      first        last         reg   offset */
	BANK_PMX("D",      GPIOD_0,     GPIOD_6,     0x005,  0),
	BANK_PMX("E",      GPIOE_0,     GPIOE_4,     0x009,  0),
	BANK_PMX("B",      GPIOB_0,    GPIOB_13,     0x000,  0),
	BANK_PMX("C",      GPIOC_0,     GPIOC_6,     0x002,  0),
	BANK_PMX("X",      GPIOX_0,     GPIOX_9,     0x003,  0),
	BANK_PMX("M",      GPIOM_0,     GPIOM_9,     0x007,  0),
	BANK_PMX("A",      GPIOA_0,     GPIOA_1,     0x006,  0)
};

static struct meson_axg_pmx_data meson_c4_periphs_pmx_banks_data = {
	.pmx_banks	= meson_c4_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_c4_periphs_pmx_banks),
};

static struct meson_pinctrl_data meson_c4_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_c4_periphs_pins,
	.groups		= meson_c4_periphs_groups,
	.funcs		= meson_c4_periphs_functions,
	.banks		= meson_c4_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_c4_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_c4_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_c4_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_c4_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_c4_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static struct meson_bank meson_c4_aobus_banks[] = {
	/*    name   first   last   irq   pullen   pull   dir   out   in   ds */
	BANK_DS("AO",  GPIOAO_0, GPIOAO_13,   0,  13,
		0x003,  0, 0x004,  0, 0x002,  0, 0x001,  0, 0x000,  0, 0x007,  0)
};

static struct meson_pmx_bank meson_c4_aobus_pmx_banks[] = {
	/*       name      first        last         reg   offset */
	BANK_PMX("AO",    GPIOAO_0,   GPIOAO_13,     0x000,  0)
};

static struct meson_axg_pmx_data meson_c4_aobus_pmx_banks_data = {
	.pmx_banks	= meson_c4_aobus_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_c4_aobus_pmx_banks),
};

static struct meson_pinctrl_data meson_c4_aobus_pinctrl_data __refdata = {
	.name		= "aobus-banks",
	.pins		= meson_c4_aobus_pins,
	.groups		= meson_c4_aobus_groups,
	.funcs		= meson_c4_aobus_functions,
	.banks		= meson_c4_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_c4_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_c4_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_c4_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_c4_aobus_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_c4_aobus_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static struct meson_bank meson_c4_testn_banks[] = {
	/*    name   first   last   irq   pullen   pull   dir   out   in   ds */
	BANK_DS("TEST_N",  GPIO_TEST_N, GPIO_TEST_N,   14,  14,
		0x003,  0, 0x004,  0, 0x002,  0, 0x001,  0, 0x000,  0, 0x007,  0)
};

static struct meson_pmx_bank meson_c4_testn_pmx_banks[] = {
	/*       name      first        last         reg   offset */
	BANK_PMX("TEST_N",    GPIO_TEST_N,   GPIO_TEST_N,     0x000,  0)
};

static struct meson_axg_pmx_data meson_c4_testn_pmx_banks_data = {
	.pmx_banks	= meson_c4_testn_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_c4_testn_pmx_banks),
};

static struct meson_pinctrl_data meson_c4_testn_pinctrl_data __refdata = {
	.name		= "testn-banks",
	.pins		= meson_c4_testn_pins,
	.groups		= meson_c4_testn_groups,
	.funcs		= meson_c4_testn_functions,
	.banks		= meson_c4_testn_banks,
	.num_pins	= ARRAY_SIZE(meson_c4_testn_pins),
	.num_groups	= ARRAY_SIZE(meson_c4_testn_groups),
	.num_funcs	= ARRAY_SIZE(meson_c4_testn_functions),
	.num_banks	= ARRAY_SIZE(meson_c4_testn_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_c4_testn_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static const struct of_device_id meson_c4_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-c4-periphs-pinctrl",
		.data = &meson_c4_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-c4-aobus-pinctrl",
		.data = &meson_c4_aobus_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-c4-testn-pinctrl",
		.data = &meson_c4_testn_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_c4_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name = "meson-c4-pinctrl",
	},
};

#ifndef MODULE
static int __init c4_pmx_init(void)
{
	meson_c4_pinctrl_driver.driver.of_match_table =
			meson_c4_pinctrl_dt_match;
	return platform_driver_register(&meson_c4_pinctrl_driver);
}
arch_initcall(c4_pmx_init);
#else
int __init meson_c4_pinctrl_init(void)
{
	meson_c4_pinctrl_driver.driver.of_match_table =
			meson_c4_pinctrl_dt_match;
	return platform_driver_register(&meson_c4_pinctrl_driver);
}
module_init(meson_c4_pinctrl_init);
#endif

MODULE_LICENSE("GPL v2");
