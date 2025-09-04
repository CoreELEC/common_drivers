// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <dt-bindings/gpio/meson-a9-gpio.h>
#include "pinctrl-meson.h"
#include "pinctrl-meson-axg-pmx.h"

/********************************** GPIO EE DOMAIN *******************************/
static const struct pinctrl_pin_desc meson_a9_periphs_pins[] = {
	/* GPIO_B */
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
	/* GPIO_X */
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
	MESON_PIN(GPIOX_14),
	MESON_PIN(GPIOX_15),
	MESON_PIN(GPIOX_16),
	MESON_PIN(GPIOX_17),
	/* GPIO_A */
	MESON_PIN(GPIOA_0),
	MESON_PIN(GPIOA_1),
	MESON_PIN(GPIOA_2),
	MESON_PIN(GPIOA_3),
	MESON_PIN(GPIOA_4),
	MESON_PIN(GPIOA_5),
	MESON_PIN(GPIOA_6),
	MESON_PIN(GPIOA_7),
	MESON_PIN(GPIOA_8),
	MESON_PIN(GPIOA_9),
	MESON_PIN(GPIOA_10),
	MESON_PIN(GPIOA_11),
	MESON_PIN(GPIOA_12),
	MESON_PIN(GPIOA_13),
	MESON_PIN(GPIOA_14),
	MESON_PIN(GPIOA_15),
	MESON_PIN(GPIOA_16),
	MESON_PIN(GPIOA_17),
	MESON_PIN(GPIOA_18),
	MESON_PIN(GPIOA_19),
	/* GPIOZ */
	MESON_PIN(GPIOZ_0),
	MESON_PIN(GPIOZ_1),
	MESON_PIN(GPIOZ_2),
	MESON_PIN(GPIOZ_3),
	MESON_PIN(GPIOZ_4),
	MESON_PIN(GPIOZ_5),
	MESON_PIN(GPIOZ_6),
	MESON_PIN(GPIOZ_7),
	MESON_PIN(GPIOZ_8),
	MESON_PIN(GPIOZ_9),
	MESON_PIN(GPIOZ_10),
	MESON_PIN(GPIOZ_11),
	MESON_PIN(GPIOZ_12),
	MESON_PIN(GPIOZ_13),
	MESON_PIN(GPIOZ_14),
	MESON_PIN(GPIOZ_15),
	/* GPIO_H */
	MESON_PIN(GPIOH_0),
	MESON_PIN(GPIOH_1),
	MESON_PIN(GPIOH_2),
	MESON_PIN(GPIOH_3),
	MESON_PIN(GPIOH_4),
	MESON_PIN(GPIOH_5),
	MESON_PIN(GPIOH_6),
	MESON_PIN(GPIOH_7),
	/* GPIOY */
	MESON_PIN(GPIOY_0),
	MESON_PIN(GPIOY_1),
	MESON_PIN(GPIOY_2),
	MESON_PIN(GPIOY_3),
	MESON_PIN(GPIOY_4),
	MESON_PIN(GPIOY_5),
	MESON_PIN(GPIOY_6),
	MESON_PIN(GPIOY_7),
	MESON_PIN(GPIOY_8),
	MESON_PIN(GPIOY_9),
	/* GPIOM */
	MESON_PIN(GPIOM_0),
	MESON_PIN(GPIOM_1),
	MESON_PIN(GPIOM_2),
	MESON_PIN(GPIOM_3),
	MESON_PIN(GPIOM_4),
	MESON_PIN(GPIOM_5),
	MESON_PIN(GPIOM_6),
	MESON_PIN(GPIOM_7),
	/* USB CC */
	MESON_PIN(GPIOCC_0),
	MESON_PIN(GPIOCC_1)
};

/* Bank[B]:Function[1] */
static const unsigned int emmc_d0_pins[]			= { GPIOB_0 };
static const unsigned int emmc_d1_pins[]			= { GPIOB_1 };
static const unsigned int emmc_d2_pins[]			= { GPIOB_2 };
static const unsigned int emmc_d3_pins[]			= { GPIOB_3 };
static const unsigned int emmc_d4_pins[]			= { GPIOB_4 };
static const unsigned int emmc_d5_pins[]			= { GPIOB_5 };
static const unsigned int emmc_d6_pins[]			= { GPIOB_6 };
static const unsigned int emmc_d7_pins[]			= { GPIOB_7 };
static const unsigned int emmc_clk_pins[]			= { GPIOB_8 };
static const unsigned int emmc_cmd_pins[]			= { GPIOB_10 };
static const unsigned int emmc_ds_pins[]			= { GPIOB_11 };

/* Bank[B]:Function[2] */
static const unsigned int nand_wen_clk_pins[]			= { GPIOB_8 };
static const unsigned int nand_ale_pins[]			= { GPIOB_9 };
static const unsigned int nand_ren_wr_pins[]			= { GPIOB_10 };
static const unsigned int nand_cle_pins[]			= { GPIOB_11 };
static const unsigned int nand_ce0_pins[]			= { GPIOB_12 };

/* Bank[B]:Function[3] */
static const unsigned int spinf_mo_d0_pins[]			= { GPIOB_0 };
static const unsigned int spinf_mi_d1_pins[]			= { GPIOB_1 };
static const unsigned int spinf_wp_d2_pins[]			= { GPIOB_2 };
static const unsigned int spinf_hold_d3_pins[]			= { GPIOB_3 };
static const unsigned int spinf_d4_pins[]			= { GPIOB_4 };
static const unsigned int spinf_d5_pins[]			= { GPIOB_5 };
static const unsigned int spinf_d6_pins[]			= { GPIOB_6 };
static const unsigned int spinf_d7_pins[]			= { GPIOB_7 };
static const unsigned int spinf_clk_pins[]			= { GPIOB_10 };
static const unsigned int spinf_cs0_pins[]			= { GPIOB_12 };

/* Bank[X]:Function[1] */
static const unsigned int sdio_d0_pins[]			= { GPIOX_0 };
static const unsigned int sdio_d1_pins[]			= { GPIOX_1 };
static const unsigned int sdio_d2_pins[]			= { GPIOX_2 };
static const unsigned int sdio_d3_pins[]			= { GPIOX_3 };
static const unsigned int sdio_clk_pins[]			= { GPIOX_4 };
static const unsigned int sdio_cmd_pins[]			= { GPIOX_5 };
static const unsigned int tdm_d1_pins[]				= { GPIOX_7 };
static const unsigned int tdm_d0_pins[]				= { GPIOX_8 };
static const unsigned int tdm_fs0_pins[]			= { GPIOX_9 };
static const unsigned int tdm_sclk0_pins[]			= { GPIOX_10 };
static const unsigned int uart_a_tx_pins[]			= { GPIOX_11 };
static const unsigned int uart_a_rx_pins[]			= { GPIOX_12 };
static const unsigned int uart_a_cts_pins[]			= { GPIOX_13 };
static const unsigned int uart_a_rts_pins[]			= { GPIOX_14 };
static const unsigned int pwm_k_pins[]				= { GPIOX_15 };
static const unsigned int pwm_h_pins[]				= { GPIOX_17 };

/* Bank[X]:Function[2] */
static const unsigned int pdm_din0_pins[]			= { GPIOX_0 };
static const unsigned int pdm_din1_pins[]			= { GPIOX_1 };
static const unsigned int pdm_din2_pins[]			= { GPIOX_2 };
static const unsigned int pdm_din3_pins[]			= { GPIOX_3 };
static const unsigned int pdm_dclk_pins[]			= { GPIOX_4 };
static const unsigned int pwm_c_pins[]				= { GPIOX_5 };
static const unsigned int pwm_f_pins[]				= { GPIOX_6 };

/* Bank[X]:Function[3] */
static const unsigned int tsin_b_din0_pins[]			= { GPIOX_0 };
static const unsigned int tsin_b_sop_pins[]			= { GPIOX_1 };
static const unsigned int tsin_b_valid_pins[]			= { GPIOX_2 };
static const unsigned int tsin_b_clk_pins[]			= { GPIOX_3 };

/* Bank[X]:Function[4] */
static const unsigned int i2c_i_sda_pins[]			= { GPIOX_0 };
static const unsigned int i2c_i_scl_pins[]			= { GPIOX_1 };
static const unsigned int i2c_h_sda_pins[]			= { GPIOX_2 };
static const unsigned int i2c_h_scl_pins[]			= { GPIOX_3 };

/* Bank[X]:Function[5] */
static const unsigned int tdm_d6_pins[]				= { GPIOX_0 };
static const unsigned int tdm_d7_pins[]				= { GPIOX_1 };
static const unsigned int tdm_fs1_pins[]			= { GPIOX_2 };
static const unsigned int tdm_sclk1_pins[]			= { GPIOX_3 };
static const unsigned int mclk_1_pins[]				= { GPIOX_4 };
static const unsigned int tdm_d4_pins[]				= { GPIOX_5 };
static const unsigned int tdm_d5_pins[]				= { GPIOX_6 };

/* Bank[X]:Function[6] */
static const unsigned int iso7816_clk_pins[]			= { GPIOX_0 };
static const unsigned int iso7816_data_pins[]			= { GPIOX_1 };
static const unsigned int tdm_d8_pins[]				= { GPIOX_2 };
static const unsigned int tdm_d9_pins[]				= { GPIOX_3 };
static const unsigned int pio0_pins[]				= { GPIOX_4 };
static const unsigned int pio1_pins[]				= { GPIOX_5 };
static const unsigned int pio2_pins[]				= { GPIOX_6 };
static const unsigned int pio3_pins[]				= { GPIOX_7 };
static const unsigned int pio4_pins[]				= { GPIOX_8 };
static const unsigned int pio5_pins[]				= { GPIOX_9 };
static const unsigned int pio6_pins[]				= { GPIOX_10 };
static const unsigned int pio7_pins[]				= { GPIOX_11 };
static const unsigned int pio8_pins[]				= { GPIOX_12 };
static const unsigned int pio9_pins[]				= { GPIOX_13 };
static const unsigned int pio10_pins[]				= { GPIOX_14 };
static const unsigned int pio11_pins[]				= { GPIOX_15 };
static const unsigned int pio12_pins[]				= { GPIOX_16 };
static const unsigned int pio13_pins[]				= { GPIOX_17 };

/* Bank[X]:Function[7] */
static const unsigned int debug_i0_pins[]			= { GPIOX_0 };
static const unsigned int debug_i1_pins[]			= { GPIOX_1 };
static const unsigned int debug_i2_pins[]			= { GPIOX_2 };
static const unsigned int debug_i3_pins[]			= { GPIOX_3 };
static const unsigned int debug_i4_pins[]			= { GPIOX_4 };
static const unsigned int debug_i5_pins[]			= { GPIOX_5 };
static const unsigned int debug_i6_pins[]			= { GPIOX_6 };
static const unsigned int debug_i7_pins[]			= { GPIOX_7 };
static const unsigned int debug_i8_pins[]			= { GPIOX_8 };
static const unsigned int debug_i9_pins[]			= { GPIOX_9 };
static const unsigned int debug_i10_pins[]			= { GPIOX_10 };
static const unsigned int debug_i11_pins[]			= { GPIOX_11 };
static const unsigned int debug_i12_pins[]			= { GPIOX_12 };
static const unsigned int debug_i13_pins[]			= { GPIOX_13 };
static const unsigned int debug_i14_pins[]			= { GPIOX_14 };
static const unsigned int debug_i15_pins[]			= { GPIOX_15 };
static const unsigned int debug_i16_pins[]			= { GPIOX_16 };
static const unsigned int debug_i17_pins[]			= { GPIOX_17 };

/* Bank[A]:Function[1] */
static const unsigned int mclk_1_a_pins[]			= { GPIOA_0 };
static const unsigned int tdm_sclk1_a_pins[]			= { GPIOA_1 };
static const unsigned int tdm_fs1_a_pins[]			= { GPIOA_2 };
static const unsigned int tdm_d2_pins[]				= { GPIOA_3 };
static const unsigned int tdm_d3_pins[]				= { GPIOA_4 };
static const unsigned int tdm_d4_a_pins[]			= { GPIOA_5 };
static const unsigned int tdm_d5_a_pins[]			= { GPIOA_6 };
static const unsigned int tdm_d6_a_pins[]			= { GPIOA_7 };
static const unsigned int tdm_d7_a_pins[]			= { GPIOA_8 };
static const unsigned int tdm_d8_a_pins[]			= { GPIOA_9 };
static const unsigned int tdm_d9_a_pins[]			= { GPIOA_10 };
static const unsigned int tdm_d10_pins[]			= { GPIOA_11 };
static const unsigned int tdm_d11_pins[]			= { GPIOA_12 };
static const unsigned int tdm_d12_pins[]			= { GPIOA_13 };
static const unsigned int tdm_d13_pins[]			= { GPIOA_14 };
static const unsigned int tdm_d14_pins[]			= { GPIOA_15 };
static const unsigned int mclk_2_pins[]				= { GPIOA_16 };
static const unsigned int tdm_sclk2_pins[]			= { GPIOA_17 };
static const unsigned int tdm_fs2_pins[]			= { GPIOA_18 };
static const unsigned int tdm_d15_pins[]			= { GPIOA_19 };

/* Bank[A]:Function[2] */
static const unsigned int rt_gpio_19_pins[]			= { GPIOA_0 };
static const unsigned int rt_gpio_20_pins[]			= { GPIOA_1 };
static const unsigned int rt_gpio_21_pins[]			= { GPIOA_2 };
static const unsigned int rt_gpio_22_pins[]			= { GPIOA_3 };
static const unsigned int rt_gpio_23_pins[]			= { GPIOA_4 };
static const unsigned int rt_gpio_24_pins[]			= { GPIOA_5 };
static const unsigned int rt_gpio_25_pins[]			= { GPIOA_6 };
static const unsigned int iso7816_clk_a_pins[]			= { GPIOA_7 };
static const unsigned int iso7816_data_a_pins[]			= { GPIOA_8 };
static const unsigned int pcieck_a_reqn_pins[]			= { GPIOA_10 };
static const unsigned int pcieck_b_reqn_pins[]			= { GPIOA_11 };
static const unsigned int rt_gpio_26_pins[]			= { GPIOA_12 };
static const unsigned int rt_gpio_27_pins[]			= { GPIOA_13 };
static const unsigned int rt_gpio_28_pins[]			= { GPIOA_14 };
static const unsigned int rt_gpio_29_pins[]			= { GPIOA_15 };
static const unsigned int iso7816_clk_a16_pins[]		= { GPIOA_16 };
static const unsigned int iso7816_data_a17_pins[]		= { GPIOA_17 };
static const unsigned int spdif_in_pins[]			= { GPIOA_18 };

/* Bank[A]:Function[3] */
static const unsigned int i2c_g_sda_pins[]			= { GPIOA_3 };
static const unsigned int i2c_g_scl_pins[]			= { GPIOA_4 };
static const unsigned int i2c_e_sda_pins[]			= { GPIOA_5 };
static const unsigned int i2c_e_scl_pins[]			= { GPIOA_6 };
static const unsigned int spi_c_mosi_pins[]			= { GPIOA_8 };
static const unsigned int spi_c_miso_pins[]			= { GPIOA_9 };
static const unsigned int spi_c_sclk_pins[]			= { GPIOA_10 };
static const unsigned int spi_c_ss0_pins[]			= { GPIOA_11 };
static const unsigned int i2c_i_sda_a_pins[]			= { GPIOA_12 };
static const unsigned int i2c_i_scl_a_pins[]			= { GPIOA_13 };
static const unsigned int i2c_d_sda_pins[]			= { GPIOA_14 };
static const unsigned int i2c_d_scl_pins[]			= { GPIOA_15 };
static const unsigned int spdif_out_pins[]			= { GPIOA_16 };
static const unsigned int can_a_tx_pins[]			= { GPIOA_18 };
static const unsigned int can_a_rx_pins[]			= { GPIOA_19 };

/* Bank[A]:Function[4] */
static const unsigned int iso7816_clk_a0_pins[]			= { GPIOA_0 };
static const unsigned int iso7816_data_a1_pins[]		= { GPIOA_1 };
static const unsigned int tsin_b_sop_a_pins[]			= { GPIOA_4 };
static const unsigned int tsin_b_valid_a_pins[]			= { GPIOA_5 };
static const unsigned int tsin_b_din0_a_pins[]			= { GPIOA_6 };
static const unsigned int tsin_b_clk_a_pins[]			= { GPIOA_7 };
static const unsigned int tsin_b_fail_pins[]			= { GPIOA_8 };
static const unsigned int tsin_b_din1_pins[]			= { GPIOA_9 };
static const unsigned int tsin_b_din2_pins[]			= { GPIOA_10 };
static const unsigned int tsin_b_din3_pins[]			= { GPIOA_11 };
static const unsigned int tsin_b_din4_pins[]			= { GPIOA_12 };
static const unsigned int tsin_b_din5_pins[]			= { GPIOA_13 };
static const unsigned int tsin_b_din6_pins[]			= { GPIOA_16 };
static const unsigned int tsin_b_din7_pins[]			= { GPIOA_17 };
static const unsigned int i3c_a_sda_pins[]			= { GPIOA_18 };
static const unsigned int i3c_a_scl_pins[]			= { GPIOA_19 };

/* Bank[A]:Function[5] */
static const unsigned int tsin_a_din0_pins[]			= { GPIOA_0 };
static const unsigned int tsin_a_sop_pins[]			= { GPIOA_1 };
static const unsigned int tsin_a_valid_pins[]			= { GPIOA_2 };
static const unsigned int tsin_a_clk_pins[]			= { GPIOA_3 };
static const unsigned int tsin_b_sop_a4_pins[]			= { GPIOA_4 };
static const unsigned int tsin_b_valid_a5_pins[]		= { GPIOA_5 };
static const unsigned int tsin_b_din0_a6_pins[]			= { GPIOA_6 };
static const unsigned int tsin_b_clk_a7_pins[]			= { GPIOA_7 };
static const unsigned int pcie_a_preset_pins[]			= { GPIOA_8 };
static const unsigned int pcie_b_preset_pins[]			= { GPIOA_9 };
static const unsigned int i3c_b_sda_d1_pins[]			= { GPIOA_13 };
static const unsigned int i3c_b_sda_d0_pins[]			= { GPIOA_14 };
static const unsigned int i3c_b_scl_pins[]			= { GPIOA_15 };
static const unsigned int i3c_b_sda_d2_pins[]			= { GPIOA_16 };
static const unsigned int i3c_b_sda_d3_pins[]			= { GPIOA_17 };

/* Bank[A]:Function[6] */
static const unsigned int pio14_pins[]				= { GPIOA_0 };
static const unsigned int pio15_pins[]				= { GPIOA_1 };
static const unsigned int pio16_pins[]				= { GPIOA_2 };
static const unsigned int pio17_pins[]				= { GPIOA_3 };
static const unsigned int pio18_pins[]				= { GPIOA_4 };
static const unsigned int pio19_pins[]				= { GPIOA_5 };
static const unsigned int pio20_pins[]				= { GPIOA_6 };
static const unsigned int pio21_pins[]				= { GPIOA_7 };
static const unsigned int pio22_pins[]				= { GPIOA_8 };
static const unsigned int pio23_pins[]				= { GPIOA_9 };
static const unsigned int eth_mdio_pins[]			= { GPIOA_10 };
static const unsigned int eth_mdc_pins[]			= { GPIOA_11 };
static const unsigned int pio24_pins[]				= { GPIOA_12 };
static const unsigned int pio25_pins[]				= { GPIOA_13 };
static const unsigned int pio26_pins[]				= { GPIOA_14 };
static const unsigned int pio27_pins[]				= { GPIOA_15 };
static const unsigned int pio28_pins[]				= { GPIOA_16 };
static const unsigned int pio29_pins[]				= { GPIOA_17 };
static const unsigned int pio30_pins[]				= { GPIOA_18 };

/* Bank[A]:Function[7] */
static const unsigned int debug_i18_pins[]			= { GPIOA_0 };
static const unsigned int debug_i19_pins[]			= { GPIOA_1 };

/* Bank[Z]:Function[1] */
static const unsigned int eth_mdio_z_pins[]			= { GPIOZ_0 };
static const unsigned int eth_mdc_z_pins[]			= { GPIOZ_1 };
static const unsigned int eth_rgmii_rx_clk_pins[]		= { GPIOZ_2 };
static const unsigned int eth_rx_dv_pins[]			= { GPIOZ_3 };
static const unsigned int eth_rxd0_pins[]			= { GPIOZ_4 };
static const unsigned int eth_rxd1_pins[]			= { GPIOZ_5 };
static const unsigned int eth_rxd2_rgmii_pins[]			= { GPIOZ_6 };
static const unsigned int eth_rxd3_rgmii_pins[]			= { GPIOZ_7 };
static const unsigned int eth_rgmii_tx_clk_pins[]		= { GPIOZ_8 };
static const unsigned int eth_txen_pins[]			= { GPIOZ_9 };
static const unsigned int eth_txd0_pins[]			= { GPIOZ_10 };
static const unsigned int eth_txd1_pins[]			= { GPIOZ_11 };
static const unsigned int eth_txd2_rgmii_pins[]			= { GPIOZ_12 };
static const unsigned int eth_txd3_rgmii_pins[]			= { GPIOZ_13 };
static const unsigned int i2c_c_sda_pins[]			= { GPIOZ_14 };
static const unsigned int i2c_c_scl_pins[]			= { GPIOZ_15 };

/* Bank[Z]:Function[2] */
static const unsigned int iso7816_clk_z_pins[]			= { GPIOZ_0 };
static const unsigned int iso7816_data_z_pins[]			= { GPIOZ_1 };
static const unsigned int tsin_b_valid_z_pins[]			= { GPIOZ_2 };
static const unsigned int tsin_b_sop_z_pins[]			= { GPIOZ_3 };
static const unsigned int tsin_b_din0_z_pins[]			= { GPIOZ_4 };
static const unsigned int tsin_b_clk_z_pins[]			= { GPIOZ_5 };
static const unsigned int tsin_b_fail_z_pins[]			= { GPIOZ_6 };
static const unsigned int tsin_b_din1_z_pins[]			= { GPIOZ_7 };
static const unsigned int tsin_b_din2_z_pins[]			= { GPIOZ_8 };
static const unsigned int tsin_b_din3_z_pins[]			= { GPIOZ_9 };
static const unsigned int tsin_b_din4_z_pins[]			= { GPIOZ_10 };
static const unsigned int tsin_b_din5_z_pins[]			= { GPIOZ_11 };
static const unsigned int tsin_b_din6_z_pins[]			= { GPIOZ_12 };
static const unsigned int tsin_b_din7_z_pins[]			= { GPIOZ_13 };

/* Bank[Z]:Function[3] */
static const unsigned int sdcard_d0_pins[]			= { GPIOZ_0 };
static const unsigned int sdcard_d1_pins[]			= { GPIOZ_1 };
static const unsigned int sdcard_d2_pins[]			= { GPIOZ_2 };
static const unsigned int sdcard_d3_pins[]			= { GPIOZ_3 };
static const unsigned int sdcard_clk_pins[]			= { GPIOZ_4 };
static const unsigned int sdcard_cmd_pins[]			= { GPIOZ_5 };
static const unsigned int tsin_a_valid_z_pins[]			= { GPIOZ_6 };
static const unsigned int tsin_a_sop_z_pins[]			= { GPIOZ_7 };
static const unsigned int tsin_a_din0_z_pins[]			= { GPIOZ_8 };
static const unsigned int tsin_a_clk_z_pins[]			= { GPIOZ_9 };
static const unsigned int iso7816_clk_z10_pins[]		= { GPIOZ_10 };
static const unsigned int iso7816_data_z11_pins[]		= { GPIOZ_11 };

/* Bank[Z]:Function[4] */
static const unsigned int spi_e_mosi_pins[]			= { GPIOZ_0 };
static const unsigned int spi_e_miso_pins[]			= { GPIOZ_1 };
static const unsigned int spi_e_sclk_pins[]			= { GPIOZ_2 };
static const unsigned int spi_e_ss0_pins[]			= { GPIOZ_3 };
static const unsigned int spi_f_mosi_pins[]			= { GPIOZ_4 };
static const unsigned int spi_f_miso_pins[]			= { GPIOZ_5 };
static const unsigned int spi_f_sclk_pins[]			= { GPIOZ_6 };
static const unsigned int spi_f_ss0_pins[]			= { GPIOZ_7 };
static const unsigned int spi_b_ss2_pins[]			= { GPIOZ_8 };
static const unsigned int spi_b_ss1_pins[]			= { GPIOZ_9 };
static const unsigned int spi_b_mosi_pins[]			= { GPIOZ_10 };
static const unsigned int spi_b_miso_pins[]			= { GPIOZ_11 };
static const unsigned int spi_b_sclk_pins[]			= { GPIOZ_12 };
static const unsigned int spi_b_ss0_pins[]			= { GPIOZ_13 };

/* Bank[Z]:Function[5] */
static const unsigned int tdm_sclk1_z_pins[]			= { GPIOZ_0 };
static const unsigned int tdm_fs1_z_pins[]			= { GPIOZ_1 };
static const unsigned int tdm_d2_z_pins[]			= { GPIOZ_2 };
static const unsigned int tdm_d3_z_pins[]			= { GPIOZ_3 };
static const unsigned int tdm_d4_z_pins[]			= { GPIOZ_4 };
static const unsigned int tdm_d5_z_pins[]			= { GPIOZ_5 };
static const unsigned int tdm_d6_z_pins[]			= { GPIOZ_6 };
static const unsigned int tdm_d7_z_pins[]			= { GPIOZ_7 };
static const unsigned int tdm_d8_z_pins[]			= { GPIOZ_8 };
static const unsigned int tdm_d9_z_pins[]			= { GPIOZ_9 };
static const unsigned int tdm_d10_z_pins[]			= { GPIOZ_10 };
static const unsigned int mclk_2_z_pins[]			= { GPIOZ_11 };
static const unsigned int tdm_sclk2_z_pins[]			= { GPIOZ_12 };
static const unsigned int tdm_fs2_z_pins[]			= { GPIOZ_13 };

/* Bank[Z]:Function[6] */
static const unsigned int i2c_e_sda_z_pins[]			= { GPIOZ_0 };
static const unsigned int i2c_e_scl_z_pins[]			= { GPIOZ_1 };
static const unsigned int i2c_f_sda_pins[]			= { GPIOZ_2 };
static const unsigned int i2c_f_scl_pins[]			= { GPIOZ_3 };
static const unsigned int i2c_g_sda_z_pins[]			= { GPIOZ_4 };
static const unsigned int i2c_g_scl_z_pins[]			= { GPIOZ_5 };
static const unsigned int i2c_h_sda_z_pins[]			= { GPIOZ_6 };
static const unsigned int i2c_h_scl_z_pins[]			= { GPIOZ_7 };
static const unsigned int pio31_pins[]				= { GPIOZ_8 };
static const unsigned int debug_o18_pins[]			= { GPIOZ_12 };
static const unsigned int debug_o19_pins[]			= { GPIOZ_13 };

/* Bank[Z]:Function[7] */
static const unsigned int uart_d_cts_pins[]			= { GPIOZ_0 };
static const unsigned int uart_d_rts_pins[]			= { GPIOZ_1 };
static const unsigned int uart_d_tx_pins[]			= { GPIOZ_2 };
static const unsigned int uart_d_rx_pins[]			= { GPIOZ_3 };
static const unsigned int uart_e_tx_pins[]			= { GPIOZ_6 };
static const unsigned int uart_e_rx_pins[]			= { GPIOZ_7 };
static const unsigned int uart_e_cts_pins[]			= { GPIOZ_8 };
static const unsigned int uart_e_rts_pins[]			= { GPIOZ_9 };
static const unsigned int tst_out18_pins[]			= { GPIOZ_12 };
static const unsigned int tst_out19_pins[]			= { GPIOZ_13 };

/* Bank[H]:Function[1] */
static const unsigned int hdmirx_sda_pins[]			= { GPIOH_0 };
static const unsigned int hdmirx_scl_pins[]			= { GPIOH_1 };
static const unsigned int hdmirx_hpd_pins[]			= { GPIOH_2 };
static const unsigned int hdmirx_5vdet_pins[]			= { GPIOH_3 };
static const unsigned int hdmitx_sda_pins[]			= { GPIOH_4 };
static const unsigned int hdmitx_scl_pins[]			= { GPIOH_5 };
static const unsigned int hdmitx_hpd_in_pins[]			= { GPIOH_6 };

/* Bank[H]:Function[2] */
static const unsigned int uart_c_tx_pins[]			= { GPIOH_0 };
static const unsigned int uart_c_rx_pins[]			= { GPIOH_1 };
static const unsigned int i2c_f_sda_h_pins[]			= { GPIOH_4 };
static const unsigned int i2c_f_scl_h_pins[]			= { GPIOH_5 };

/* Bank[H]:Function[3] */
static const unsigned int can_b_tx_pins[]			= { GPIOH_0 };
static const unsigned int can_b_rx_pins[]			= { GPIOH_1 };
static const unsigned int pwm_m_pins[]				= { GPIOH_4 };
static const unsigned int pwm_vs_pins[]				= { GPIOH_5 };
static const unsigned int vx1_htpdn_pins[]			= { GPIOH_6 };
static const unsigned int vx1_lockn_pins[]			= { GPIOH_7 };

/* Bank[H]:Function[4] */
static const unsigned int dp_hpd_pins[]				= { GPIOH_5 };
static const unsigned int edp_hpd_pins[]			= { GPIOH_6 };

/* Bank[H]:Function[5] */
static const unsigned int pcieck_a_reqn_h_pins[]		= { GPIOH_4 };
static const unsigned int pcieck_b_reqn_h_pins[]		= { GPIOH_5 };

/* Bank[Y]:Function[1] */
static const unsigned int pwm_n_pins[]				= { GPIOY_0 };
static const unsigned int pwm_i_pins[]				= { GPIOY_1 };
static const unsigned int pwm_k_y_pins[]			= { GPIOY_2 };
static const unsigned int pwm_l_pins[]				= { GPIOY_3 };
static const unsigned int pwm_m_y_pins[]			= { GPIOY_4 };
static const unsigned int pwm_j_pins[]				= { GPIOY_5 };
static const unsigned int pwm_vs_y_pins[]			= { GPIOY_6 };
static const unsigned int vx1_lockn_y_pins[]			= { GPIOY_7 };
static const unsigned int vx1_htpdn_y_pins[]			= { GPIOY_9 };

/* Bank[Y]:Function[2] */
static const unsigned int pcieck_a_reqn_y_pins[]		= { GPIOY_0 };
static const unsigned int pcieck_b_reqn_y_pins[]		= { GPIOY_1 };
static const unsigned int pcie_a_preset_y_pins[]		= { GPIOY_2 };
static const unsigned int pcie_b_preset_y_pins[]		= { GPIOY_3 };
static const unsigned int i2c_e_sda_y_pins[]			= { GPIOY_5 };
static const unsigned int i2c_e_scl_y_pins[]			= { GPIOY_6 };
static const unsigned int dp_hpd_y_pins[]			= { GPIOY_8 };
static const unsigned int edp_hpd_y_pins[]			= { GPIOY_9 };

/* Bank[Y]:Function[3] */
static const unsigned int pdm_din0_y_pins[]			= { GPIOY_0 };
static const unsigned int pdm_dclk_y_pins[]			= { GPIOY_1 };
static const unsigned int pdm_din1_y_pins[]			= { GPIOY_2 };
static const unsigned int pdm_din2_y_pins[]			= { GPIOY_3 };
static const unsigned int pdm_din3_y_pins[]			= { GPIOY_4 };
static const unsigned int i2c_i_sda_y_pins[]			= { GPIOY_8 };
static const unsigned int i2c_i_scl_y_pins[]			= { GPIOY_9 };

/* Bank[Y]:Function[4] */
static const unsigned int mclk_1_y_pins[]			= { GPIOY_0 };
static const unsigned int tdm_sclk1_y_pins[]			= { GPIOY_1 };
static const unsigned int tdm_fs1_y_pins[]			= { GPIOY_2 };
static const unsigned int tdm_d2_y_pins[]			= { GPIOY_3 };
static const unsigned int tdm_d3_y_pins[]			= { GPIOY_4 };
static const unsigned int tdm_d4_y_pins[]			= { GPIOY_5 };
static const unsigned int tdm_d5_y_pins[]			= { GPIOY_6 };
static const unsigned int tdm_d6_y_pins[]			= { GPIOY_7 };
static const unsigned int tdm_d7_y_pins[]			= { GPIOY_8 };
static const unsigned int tdm_d8_y_pins[]			= { GPIOY_9 };

/* Bank[Y]:Function[5] */
static const unsigned int rt_gpio_30_pins[]			= { GPIOY_0 };
static const unsigned int rt_gpio_31_pins[]			= { GPIOY_1 };
static const unsigned int rt_gpio_32_pins[]			= { GPIOY_2 };
static const unsigned int spdif_in_y_pins[]			= { GPIOY_3 };
static const unsigned int spdif_out_y_pins[]			= { GPIOY_4 };
static const unsigned int rt_gpio_33_pins[]			= { GPIOY_5 };
static const unsigned int rt_gpio_34_pins[]			= { GPIOY_6 };
static const unsigned int rt_gpio_35_pins[]			= { GPIOY_7 };
static const unsigned int rt_gpio_36_pins[]			= { GPIOY_8 };
static const unsigned int rt_gpio_37_pins[]			= { GPIOY_9 };

/* Bank[Y]:Function[6] */
static const unsigned int debug_o0_pins[]			= { GPIOY_0 };
static const unsigned int debug_o1_pins[]			= { GPIOY_1 };
static const unsigned int debug_o2_pins[]			= { GPIOY_2 };
static const unsigned int debug_o3_pins[]			= { GPIOY_3 };
static const unsigned int debug_o4_pins[]			= { GPIOY_4 };
static const unsigned int debug_o5_pins[]			= { GPIOY_5 };
static const unsigned int debug_o6_pins[]			= { GPIOY_6 };
static const unsigned int debug_o7_pins[]			= { GPIOY_7 };
static const unsigned int debug_o8_pins[]			= { GPIOY_8 };
static const unsigned int debug_o9_pins[]			= { GPIOY_9 };

/* Bank[Y]:Function[7] */
static const unsigned int tst_out0_pins[]			= { GPIOY_0 };
static const unsigned int tst_out1_pins[]			= { GPIOY_1 };
static const unsigned int tst_out2_pins[]			= { GPIOY_2 };
static const unsigned int tst_out3_pins[]			= { GPIOY_3 };
static const unsigned int tst_out4_pins[]			= { GPIOY_4 };
static const unsigned int tst_out5_pins[]			= { GPIOY_5 };
static const unsigned int tst_out6_pins[]			= { GPIOY_6 };
static const unsigned int tst_out7_pins[]			= { GPIOY_7 };
static const unsigned int tst_out8_pins[]			= { GPIOY_8 };
static const unsigned int tst_out9_pins[]			= { GPIOY_9 };

/* Bank[M]:Function[1] */
static const unsigned int i2c_g_sda_m_pins[]			= { GPIOM_0 };
static const unsigned int i2c_g_scl_m_pins[]			= { GPIOM_1 };
static const unsigned int i2c_e_sda_m_pins[]			= { GPIOM_2 };
static const unsigned int i2c_e_scl_m_pins[]			= { GPIOM_3 };
static const unsigned int i2c_f_sda_m_pins[]			= { GPIOM_4 };
static const unsigned int i2c_f_scl_m_pins[]			= { GPIOM_5 };
static const unsigned int i2c_h_sda_m_pins[]			= { GPIOM_6 };
static const unsigned int i2c_h_scl_m_pins[]			= { GPIOM_7 };

/* Bank[M]:Function[2] */
static const unsigned int uart_d_cts_m_pins[]			= { GPIOM_0 };
static const unsigned int uart_d_rts_m_pins[]			= { GPIOM_1 };
static const unsigned int uart_d_tx_m_pins[]			= { GPIOM_2 };
static const unsigned int uart_d_rx_m_pins[]			= { GPIOM_3 };
static const unsigned int uart_f_tx_pins[]			= { GPIOM_4 };
static const unsigned int uart_f_rx_pins[]			= { GPIOM_5 };
static const unsigned int uart_f_cts_pins[]			= { GPIOM_6 };
static const unsigned int uart_f_rts_pins[]			= { GPIOM_7 };

/* Bank[M]:Function[3] */
static const unsigned int pdm_din0_m_pins[]			= { GPIOM_0 };
static const unsigned int pdm_dclk_m_pins[]			= { GPIOM_1 };
static const unsigned int pdm_din1_m_pins[]			= { GPIOM_2 };
static const unsigned int pdm_din2_m_pins[]			= { GPIOM_3 };
static const unsigned int pdm_din3_m_pins[]			= { GPIOM_4 };
static const unsigned int pcieck_b_reqn_m_pins[]		= { GPIOM_5 };
static const unsigned int dp_hpd_m_pins[]			= { GPIOM_6 };
static const unsigned int edp_hpd_m_pins[]			= { GPIOM_7 };

/* Bank[M]:Function[4] */
static const unsigned int tdm_d9_m_pins[]			= { GPIOM_0 };
static const unsigned int tdm_d10_m_pins[]			= { GPIOM_1 };
static const unsigned int tdm_d11_m_pins[]			= { GPIOM_2 };
static const unsigned int mclk_2_m_pins[]			= { GPIOM_3 };
static const unsigned int tdm_sclk2_m_pins[]			= { GPIOM_4 };
static const unsigned int tdm_fs2_m_pins[]			= { GPIOM_5 };
static const unsigned int tdm_d14_m_pins[]			= { GPIOM_6 };
static const unsigned int tdm_d15_m_pins[]			= { GPIOM_7 };

/* Bank[M]:Function[5] */
static const unsigned int spi_c_mosi_m_pins[]			= { GPIOM_0 };
static const unsigned int spi_c_miso_m_pins[]			= { GPIOM_1 };
static const unsigned int spi_c_sclk_m_pins[]			= { GPIOM_2 };
static const unsigned int spi_c_ss0_m_pins[]			= { GPIOM_3 };
static const unsigned int pwm_j_m_pins[]			= { GPIOM_4 };
static const unsigned int pwm_vs_m_pins[]			= { GPIOM_5 };
static const unsigned int vx1_lockn_m_pins[]			= { GPIOM_6 };
static const unsigned int vx1_htpdn_m_pins[]			= { GPIOM_7 };

/* Bank[M]:Function[6] */
static const unsigned int debug_o10_pins[]			= { GPIOM_0 };
static const unsigned int debug_o11_pins[]			= { GPIOM_1 };
static const unsigned int debug_o12_pins[]			= { GPIOM_2 };
static const unsigned int debug_o13_pins[]			= { GPIOM_3 };
static const unsigned int debug_o14_pins[]			= { GPIOM_4 };
static const unsigned int debug_o15_pins[]			= { GPIOM_5 };
static const unsigned int debug_o16_pins[]			= { GPIOM_6 };
static const unsigned int debug_o17_pins[]			= { GPIOM_7 };

/* Bank[M]:Function[7] */
static const unsigned int tst_out10_pins[]			= { GPIOM_0 };
static const unsigned int tst_out11_pins[]			= { GPIOM_1 };
static const unsigned int tst_out12_pins[]			= { GPIOM_2 };
static const unsigned int tst_out13_pins[]			= { GPIOM_3 };
static const unsigned int tst_out14_pins[]			= { GPIOM_4 };
static const unsigned int tst_out15_pins[]			= { GPIOM_5 };
static const unsigned int tst_out16_pins[]			= { GPIOM_6 };
static const unsigned int tst_out17_pins[]			= { GPIOM_7 };

/* Bank[CC]:Function[1] */
static const unsigned int i2c_f_sda_cc_pins[]			= { GPIOCC_0 };
static const unsigned int i2c_f_scl_cc_pins[]			= { GPIOCC_1 };

static struct meson_pmx_group meson_a9_periphs_groups[] __initdata = {
	/* GPIO_B */
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
	/* GPIO_X */
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
	GPIO_GROUP(GPIOX_14),
	GPIO_GROUP(GPIOX_15),
	GPIO_GROUP(GPIOX_16),
	GPIO_GROUP(GPIOX_17),
	/* GPIO_A */
	GPIO_GROUP(GPIOA_0),
	GPIO_GROUP(GPIOA_1),
	GPIO_GROUP(GPIOA_2),
	GPIO_GROUP(GPIOA_3),
	GPIO_GROUP(GPIOA_4),
	GPIO_GROUP(GPIOA_5),
	GPIO_GROUP(GPIOA_6),
	GPIO_GROUP(GPIOA_7),
	GPIO_GROUP(GPIOA_8),
	GPIO_GROUP(GPIOA_9),
	GPIO_GROUP(GPIOA_10),
	GPIO_GROUP(GPIOA_11),
	GPIO_GROUP(GPIOA_12),
	GPIO_GROUP(GPIOA_13),
	GPIO_GROUP(GPIOA_14),
	GPIO_GROUP(GPIOA_15),
	GPIO_GROUP(GPIOA_16),
	GPIO_GROUP(GPIOA_17),
	GPIO_GROUP(GPIOA_18),
	GPIO_GROUP(GPIOA_19),
	/* GPIOZ */
	GPIO_GROUP(GPIOZ_0),
	GPIO_GROUP(GPIOZ_1),
	GPIO_GROUP(GPIOZ_2),
	GPIO_GROUP(GPIOZ_3),
	GPIO_GROUP(GPIOZ_4),
	GPIO_GROUP(GPIOZ_5),
	GPIO_GROUP(GPIOZ_6),
	GPIO_GROUP(GPIOZ_7),
	GPIO_GROUP(GPIOZ_8),
	GPIO_GROUP(GPIOZ_9),
	GPIO_GROUP(GPIOZ_10),
	GPIO_GROUP(GPIOZ_11),
	GPIO_GROUP(GPIOZ_12),
	GPIO_GROUP(GPIOZ_13),
	GPIO_GROUP(GPIOZ_14),
	GPIO_GROUP(GPIOZ_15),
	/* GPIO_H */
	GPIO_GROUP(GPIOH_0),
	GPIO_GROUP(GPIOH_1),
	GPIO_GROUP(GPIOH_2),
	GPIO_GROUP(GPIOH_3),
	GPIO_GROUP(GPIOH_4),
	GPIO_GROUP(GPIOH_5),
	GPIO_GROUP(GPIOH_6),
	GPIO_GROUP(GPIOH_7),
	/* GPIOY */
	GPIO_GROUP(GPIOY_0),
	GPIO_GROUP(GPIOY_1),
	GPIO_GROUP(GPIOY_2),
	GPIO_GROUP(GPIOY_3),
	GPIO_GROUP(GPIOY_4),
	GPIO_GROUP(GPIOY_5),
	GPIO_GROUP(GPIOY_6),
	GPIO_GROUP(GPIOY_7),
	GPIO_GROUP(GPIOY_8),
	GPIO_GROUP(GPIOY_9),
	/* GPIOM */
	GPIO_GROUP(GPIOM_0),
	GPIO_GROUP(GPIOM_1),
	GPIO_GROUP(GPIOM_2),
	GPIO_GROUP(GPIOM_3),
	GPIO_GROUP(GPIOM_4),
	GPIO_GROUP(GPIOM_5),
	GPIO_GROUP(GPIOM_6),
	GPIO_GROUP(GPIOM_7),
	/* USB CC */
	GPIO_GROUP(GPIOCC_0),
	GPIO_GROUP(GPIOCC_1),
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
	GROUP(emmc_cmd,			1),
	GROUP(emmc_ds,			1),
	/* Bank[B]:Function[2] */
	GROUP(nand_wen_clk,		2),
	GROUP(nand_ale,			2),
	GROUP(nand_ren_wr,		2),
	GROUP(nand_cle,			2),
	GROUP(nand_ce0,			2),
	/* Bank[B]:Function[3] */
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
	/* Bank[X]:Function[1] */
	GROUP(sdio_d0,			1),
	GROUP(sdio_d1,			1),
	GROUP(sdio_d2,			1),
	GROUP(sdio_d3,			1),
	GROUP(sdio_clk,			1),
	GROUP(sdio_cmd,			1),
	GROUP(tdm_d1,			1),
	GROUP(tdm_d0,			1),
	GROUP(tdm_fs0,			1),
	GROUP(tdm_sclk0,		1),
	GROUP(uart_a_tx,		1),
	GROUP(uart_a_rx,		1),
	GROUP(uart_a_cts,		1),
	GROUP(uart_a_rts,		1),
	GROUP(pwm_k,			1),
	GROUP(pwm_h,			1),
	/* Bank[X]:Function[2] */
	GROUP(pdm_din0,			2),
	GROUP(pdm_din1,			2),
	GROUP(pdm_din2,			2),
	GROUP(pdm_din3,			2),
	GROUP(pdm_dclk,			2),
	GROUP(pwm_c,			2),
	GROUP(pwm_f,			2),
	/* Bank[X]:Function[3] */
	GROUP(tsin_b_din0,		3),
	GROUP(tsin_b_sop,		3),
	GROUP(tsin_b_valid,		3),
	GROUP(tsin_b_clk,		3),
	/* Bank[X]:Function[4] */
	GROUP(i2c_i_sda,		4),
	GROUP(i2c_i_scl,		4),
	GROUP(i2c_h_sda,		4),
	GROUP(i2c_h_scl,		4),
	/* Bank[X]:Function[5] */
	GROUP(tdm_d6,			5),
	GROUP(tdm_d7,			5),
	GROUP(tdm_fs1,			5),
	GROUP(tdm_sclk1,		5),
	GROUP(mclk_1,			5),
	GROUP(tdm_d4,			5),
	GROUP(tdm_d5,			5),
	/* Bank[X]:Function[6] */
	GROUP(iso7816_clk,		6),
	GROUP(iso7816_data,		6),
	GROUP(tdm_d8,			6),
	GROUP(tdm_d9,			6),
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
	/* Bank[X]:Function[7] */
	GROUP(debug_i0,			7),
	GROUP(debug_i1,			7),
	GROUP(debug_i2,			7),
	GROUP(debug_i3,			7),
	GROUP(debug_i4,			7),
	GROUP(debug_i5,			7),
	GROUP(debug_i6,			7),
	GROUP(debug_i7,			7),
	GROUP(debug_i8,			7),
	GROUP(debug_i9,			7),
	GROUP(debug_i10,		7),
	GROUP(debug_i11,		7),
	GROUP(debug_i12,		7),
	GROUP(debug_i13,		7),
	GROUP(debug_i14,		7),
	GROUP(debug_i15,		7),
	GROUP(debug_i16,		7),
	GROUP(debug_i17,		7),
	/* Bank[A]:Function[1] */
	GROUP(mclk_1_a,			1),
	GROUP(tdm_sclk1_a,		1),
	GROUP(tdm_fs1_a,		1),
	GROUP(tdm_d2,			1),
	GROUP(tdm_d3,			1),
	GROUP(tdm_d4_a,			1),
	GROUP(tdm_d5_a,			1),
	GROUP(tdm_d6_a,			1),
	GROUP(tdm_d7_a,			1),
	GROUP(tdm_d8_a,			1),
	GROUP(tdm_d9_a,			1),
	GROUP(tdm_d10,			1),
	GROUP(tdm_d11,			1),
	GROUP(tdm_d12,			1),
	GROUP(tdm_d13,			1),
	GROUP(tdm_d14,			1),
	GROUP(mclk_2,			1),
	GROUP(tdm_sclk2,		1),
	GROUP(tdm_fs2,			1),
	GROUP(tdm_d15,			1),
	/* Bank[A]:Function[2] */
	GROUP(rt_gpio_19,		2),
	GROUP(rt_gpio_20,		2),
	GROUP(rt_gpio_21,		2),
	GROUP(rt_gpio_22,		2),
	GROUP(rt_gpio_23,		2),
	GROUP(rt_gpio_24,		2),
	GROUP(rt_gpio_25,		2),
	GROUP(iso7816_clk_a,		2),
	GROUP(iso7816_data_a,		2),
	GROUP(pcieck_a_reqn,		2),
	GROUP(pcieck_b_reqn,		2),
	GROUP(rt_gpio_26,		2),
	GROUP(rt_gpio_27,		2),
	GROUP(rt_gpio_28,		2),
	GROUP(rt_gpio_29,		2),
	GROUP(iso7816_clk_a16,		2),
	GROUP(iso7816_data_a17,		2),
	GROUP(spdif_in,			2),
	/* Bank[A]:Function[3] */
	GROUP(i2c_g_sda,		3),
	GROUP(i2c_g_scl,		3),
	GROUP(i2c_e_sda,		3),
	GROUP(i2c_e_scl,		3),
	GROUP(spi_c_mosi,		3),
	GROUP(spi_c_miso,		3),
	GROUP(spi_c_sclk,		3),
	GROUP(spi_c_ss0,		3),
	GROUP(i2c_i_sda_a,		3),
	GROUP(i2c_i_scl_a,		3),
	GROUP(i2c_d_sda,		3),
	GROUP(i2c_d_scl,		3),
	GROUP(spdif_out,		3),
	GROUP(can_a_tx,			3),
	GROUP(can_a_rx,			3),
	/* Bank[A]:Function[4] */
	GROUP(iso7816_clk_a0,		4),
	GROUP(iso7816_data_a1,		4),
	GROUP(tsin_b_sop_a,		4),
	GROUP(tsin_b_valid_a,		4),
	GROUP(tsin_b_din0_a,		4),
	GROUP(tsin_b_clk_a,		4),
	GROUP(tsin_b_fail,		4),
	GROUP(tsin_b_din1,		4),
	GROUP(tsin_b_din2,		4),
	GROUP(tsin_b_din3,		4),
	GROUP(tsin_b_din4,		4),
	GROUP(tsin_b_din5,		4),
	GROUP(tsin_b_din6,		4),
	GROUP(tsin_b_din7,		4),
	GROUP(i3c_a_sda,		4),
	GROUP(i3c_a_scl,		4),
	/* Bank[A]:Function[5] */
	GROUP(tsin_a_din0,		5),
	GROUP(tsin_a_sop,		5),
	GROUP(tsin_a_valid,		5),
	GROUP(tsin_a_clk,		5),
	GROUP(tsin_b_sop_a4,		5),
	GROUP(tsin_b_valid_a5,		5),
	GROUP(tsin_b_din0_a6,		5),
	GROUP(tsin_b_clk_a7,		5),
	GROUP(pcie_a_preset,		5),
	GROUP(pcie_b_preset,		5),
	GROUP(i3c_b_sda_d1,		5),
	GROUP(i3c_b_sda_d0,		5),
	GROUP(i3c_b_scl,		5),
	GROUP(i3c_b_sda_d2,		5),
	GROUP(i3c_b_sda_d3,		5),
	/* Bank[A]:Function[6] */
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
	GROUP(eth_mdio,			6),
	GROUP(eth_mdc,			6),
	GROUP(pio24,			6),
	GROUP(pio25,			6),
	GROUP(pio26,			6),
	GROUP(pio27,			6),
	GROUP(pio28,			6),
	GROUP(pio29,			6),
	GROUP(pio30,			6),
	/* Bank[A]:Function[7] */
	GROUP(debug_i18,		7),
	GROUP(debug_i19,		7),
	/* Bank[Z]:Function[1] */
	GROUP(eth_mdio_z,		1),
	GROUP(eth_mdc_z,		1),
	GROUP(eth_rgmii_rx_clk,		1),
	GROUP(eth_rx_dv,		1),
	GROUP(eth_rxd0,			1),
	GROUP(eth_rxd1,			1),
	GROUP(eth_rxd2_rgmii,		1),
	GROUP(eth_rxd3_rgmii,		1),
	GROUP(eth_rgmii_tx_clk,		1),
	GROUP(eth_txen,			1),
	GROUP(eth_txd0,			1),
	GROUP(eth_txd1,			1),
	GROUP(eth_txd2_rgmii,		1),
	GROUP(eth_txd3_rgmii,		1),
	GROUP(i2c_c_sda,		1),
	GROUP(i2c_c_scl,		1),
	/* Bank[Z]:Function[2] */
	GROUP(iso7816_clk_z,		2),
	GROUP(iso7816_data_z,		2),
	GROUP(tsin_b_valid_z,		2),
	GROUP(tsin_b_sop_z,		2),
	GROUP(tsin_b_din0_z,		2),
	GROUP(tsin_b_clk_z,		2),
	GROUP(tsin_b_fail_z,		2),
	GROUP(tsin_b_din1_z,		2),
	GROUP(tsin_b_din2_z,		2),
	GROUP(tsin_b_din3_z,		2),
	GROUP(tsin_b_din4_z,		2),
	GROUP(tsin_b_din5_z,		2),
	GROUP(tsin_b_din6_z,		2),
	GROUP(tsin_b_din7_z,		2),
	/* Bank[Z]:Function[3] */
	GROUP(sdcard_d0,		3),
	GROUP(sdcard_d1,		3),
	GROUP(sdcard_d2,		3),
	GROUP(sdcard_d3,		3),
	GROUP(sdcard_clk,		3),
	GROUP(sdcard_cmd,		3),
	GROUP(tsin_a_valid_z,		3),
	GROUP(tsin_a_sop_z,		3),
	GROUP(tsin_a_din0_z,		3),
	GROUP(tsin_a_clk_z,		3),
	GROUP(iso7816_clk_z10,		3),
	GROUP(iso7816_data_z11,		3),
	/* Bank[Z]:Function[4] */
	GROUP(spi_e_mosi,		4),
	GROUP(spi_e_miso,		4),
	GROUP(spi_e_sclk,		4),
	GROUP(spi_e_ss0,		4),
	GROUP(spi_f_mosi,		4),
	GROUP(spi_f_miso,		4),
	GROUP(spi_f_sclk,		4),
	GROUP(spi_f_ss0,		4),
	GROUP(spi_b_ss2,		4),
	GROUP(spi_b_ss1,		4),
	GROUP(spi_b_mosi,		4),
	GROUP(spi_b_miso,		4),
	GROUP(spi_b_sclk,		4),
	GROUP(spi_b_ss0,		4),
	/* Bank[Z]:Function[5] */
	GROUP(tdm_sclk1_z,		5),
	GROUP(tdm_fs1_z,		5),
	GROUP(tdm_d2_z,			5),
	GROUP(tdm_d3_z,			5),
	GROUP(tdm_d4_z,			5),
	GROUP(tdm_d5_z,			5),
	GROUP(tdm_d6_z,			5),
	GROUP(tdm_d7_z,			5),
	GROUP(tdm_d8_z,			5),
	GROUP(tdm_d9_z,			5),
	GROUP(tdm_d10_z,		5),
	GROUP(mclk_2_z,			5),
	GROUP(tdm_sclk2_z,		5),
	GROUP(tdm_fs2_z,		5),
	/* Bank[Z]:Function[6] */
	GROUP(i2c_e_sda_z,		6),
	GROUP(i2c_e_scl_z,		6),
	GROUP(i2c_f_sda,		6),
	GROUP(i2c_f_scl,		6),
	GROUP(i2c_g_sda_z,		6),
	GROUP(i2c_g_scl_z,		6),
	GROUP(i2c_h_sda_z,		6),
	GROUP(i2c_h_scl_z,		6),
	GROUP(pio31,			6),
	GROUP(debug_o18,		6),
	GROUP(debug_o19,		6),
	/* Bank[Z]:Function[7] */
	GROUP(uart_d_cts,		7),
	GROUP(uart_d_rts,		7),
	GROUP(uart_d_tx,		7),
	GROUP(uart_d_rx,		7),
	GROUP(uart_e_tx,		7),
	GROUP(uart_e_rx,		7),
	GROUP(uart_e_cts,		7),
	GROUP(uart_e_rts,		7),
	GROUP(tst_out18,		7),
	GROUP(tst_out19,		7),
	/* Bank[H]:Function[1] */
	GROUP(hdmirx_sda,		1),
	GROUP(hdmirx_scl,		1),
	GROUP(hdmirx_hpd,		1),
	GROUP(hdmirx_5vdet,		1),
	GROUP(hdmitx_sda,		1),
	GROUP(hdmitx_scl,		1),
	GROUP(hdmitx_hpd_in,		1),
	/* Bank[H]:Function[2] */
	GROUP(uart_c_tx,		2),
	GROUP(uart_c_rx,		2),
	GROUP(i2c_f_sda_h,		2),
	GROUP(i2c_f_scl_h,		2),
	/* Bank[H]:Function[3] */
	GROUP(can_b_tx,			3),
	GROUP(can_b_rx,			3),
	GROUP(pwm_m,			3),
	GROUP(pwm_vs,			3),
	GROUP(vx1_htpdn,		3),
	GROUP(vx1_lockn,		3),
	/* Bank[H]:Function[4] */
	GROUP(dp_hpd,			4),
	GROUP(edp_hpd,			4),
	/* Bank[H]:Function[5] */
	GROUP(pcieck_a_reqn_h,		5),
	GROUP(pcieck_b_reqn_h,		5),
	/* Bank[Y]:Function[1] */
	GROUP(pwm_n,			1),
	GROUP(pwm_i,			1),
	GROUP(pwm_k_y,			1),
	GROUP(pwm_l,			1),
	GROUP(pwm_m_y,			1),
	GROUP(pwm_j,			1),
	GROUP(pwm_vs_y,			1),
	GROUP(vx1_lockn_y,		1),
	GROUP(vx1_htpdn_y,		1),
	/* Bank[Y]:Function[2] */
	GROUP(pcieck_a_reqn_y,		2),
	GROUP(pcieck_b_reqn_y,		2),
	GROUP(pcie_a_preset_y,		2),
	GROUP(pcie_b_preset_y,		2),
	GROUP(i2c_e_sda_y,		2),
	GROUP(i2c_e_scl_y,		2),
	GROUP(dp_hpd_y,			2),
	GROUP(edp_hpd_y,		2),
	/* Bank[Y]:Function[3] */
	GROUP(pdm_din0_y,		3),
	GROUP(pdm_dclk_y,		3),
	GROUP(pdm_din1_y,		3),
	GROUP(pdm_din2_y,		3),
	GROUP(pdm_din3_y,		3),
	GROUP(i2c_i_sda_y,		3),
	GROUP(i2c_i_scl_y,		3),
	/* Bank[Y]:Function[4] */
	GROUP(mclk_1_y,			4),
	GROUP(tdm_sclk1_y,		4),
	GROUP(tdm_fs1_y,		4),
	GROUP(tdm_d2_y,			4),
	GROUP(tdm_d3_y,			4),
	GROUP(tdm_d4_y,			4),
	GROUP(tdm_d5_y,			4),
	GROUP(tdm_d6_y,			4),
	GROUP(tdm_d7_y,			4),
	GROUP(tdm_d8_y,			4),
	/* Bank[Y]:Function[5] */
	GROUP(rt_gpio_30,		5),
	GROUP(rt_gpio_31,		5),
	GROUP(rt_gpio_32,		5),
	GROUP(spdif_in_y,		5),
	GROUP(spdif_out_y,		5),
	GROUP(rt_gpio_33,		5),
	GROUP(rt_gpio_34,		5),
	GROUP(rt_gpio_35,		5),
	GROUP(rt_gpio_36,		5),
	GROUP(rt_gpio_37,		5),
	/* Bank[Y]:Function[6] */
	GROUP(debug_o0,			6),
	GROUP(debug_o1,			6),
	GROUP(debug_o2,			6),
	GROUP(debug_o3,			6),
	GROUP(debug_o4,			6),
	GROUP(debug_o5,			6),
	GROUP(debug_o6,			6),
	GROUP(debug_o7,			6),
	GROUP(debug_o8,			6),
	GROUP(debug_o9,			6),
	/* Bank[Y]:Function[7] */
	GROUP(tst_out0,			7),
	GROUP(tst_out1,			7),
	GROUP(tst_out2,			7),
	GROUP(tst_out3,			7),
	GROUP(tst_out4,			7),
	GROUP(tst_out5,			7),
	GROUP(tst_out6,			7),
	GROUP(tst_out7,			7),
	GROUP(tst_out8,			7),
	GROUP(tst_out9,			7),
	/* Bank[M]:Function[1] */
	GROUP(i2c_g_sda_m,		1),
	GROUP(i2c_g_scl_m,		1),
	GROUP(i2c_e_sda_m,		1),
	GROUP(i2c_e_scl_m,		1),
	GROUP(i2c_f_sda_m,		1),
	GROUP(i2c_f_scl_m,		1),
	GROUP(i2c_h_sda_m,		1),
	GROUP(i2c_h_scl_m,		1),
	/* Bank[M]:Function[2] */
	GROUP(uart_d_cts_m,		2),
	GROUP(uart_d_rts_m,		2),
	GROUP(uart_d_tx_m,		2),
	GROUP(uart_d_rx_m,		2),
	GROUP(uart_f_tx,		2),
	GROUP(uart_f_rx,		2),
	GROUP(uart_f_cts,		2),
	GROUP(uart_f_rts,		2),
	/* Bank[M]:Function[3] */
	GROUP(pdm_din0_m,		3),
	GROUP(pdm_dclk_m,		3),
	GROUP(pdm_din1_m,		3),
	GROUP(pdm_din2_m,		3),
	GROUP(pdm_din3_m,		3),
	GROUP(pcieck_b_reqn_m,		3),
	GROUP(dp_hpd_m,			3),
	GROUP(edp_hpd_m,		3),
	/* Bank[M]:Function[4] */
	GROUP(tdm_d9_m,			4),
	GROUP(tdm_d10_m,		4),
	GROUP(tdm_d11_m,		4),
	GROUP(mclk_2_m,			4),
	GROUP(tdm_sclk2_m,		4),
	GROUP(tdm_fs2_m,		4),
	GROUP(tdm_d14_m,		4),
	GROUP(tdm_d15_m,		4),
	/* Bank[M]:Function[5] */
	GROUP(spi_c_mosi_m,		5),
	GROUP(spi_c_miso_m,		5),
	GROUP(spi_c_sclk_m,		5),
	GROUP(spi_c_ss0_m,		5),
	GROUP(pwm_j_m,			5),
	GROUP(pwm_vs_m,			5),
	GROUP(vx1_lockn_m,		5),
	GROUP(vx1_htpdn_m,		5),
	/* Bank[M]:Function[6] */
	GROUP(debug_o10,		6),
	GROUP(debug_o11,		6),
	GROUP(debug_o12,		6),
	GROUP(debug_o13,		6),
	GROUP(debug_o14,		6),
	GROUP(debug_o15,		6),
	GROUP(debug_o16,		6),
	GROUP(debug_o17,		6),
	/* Bank[M]:Function[7] */
	GROUP(tst_out10,		7),
	GROUP(tst_out11,		7),
	GROUP(tst_out12,		7),
	GROUP(tst_out13,		7),
	GROUP(tst_out14,		7),
	GROUP(tst_out15,		7),
	GROUP(tst_out16,		7),
	GROUP(tst_out17,		7),
	/* Bank[CC]:Function[1] */
	GROUP(i2c_f_sda_cc,		1),
	GROUP(i2c_f_scl_cc,		1)
};

static const char * const gpio_periphs_groups[] = {
	"GPIOA_0",  "GPIOA_1",  "GPIOA_2",  "GPIOA_3",  "GPIOA_4",
	"GPIOA_5",  "GPIOA_6",  "GPIOA_7",  "GPIOA_8",  "GPIOA_9",
	"GPIOB_0",  "GPIOB_1",  "GPIOB_2",  "GPIOB_3",  "GPIOB_4",
	"GPIOB_5",  "GPIOB_6",  "GPIOB_7",  "GPIOB_8",  "GPIOB_9",
	"GPIOH_0",  "GPIOH_1",  "GPIOH_2",  "GPIOH_3",  "GPIOH_4",
	"GPIOH_5",  "GPIOH_6",  "GPIOH_7",  "GPIOM_0",  "GPIOM_1",
	"GPIOM_2",  "GPIOM_3",  "GPIOM_4",  "GPIOM_5",  "GPIOM_6",
	"GPIOM_7",  "GPIOX_0",  "GPIOX_1",  "GPIOX_2",  "GPIOX_3",
	"GPIOX_4",  "GPIOX_5",  "GPIOX_6",  "GPIOX_7",  "GPIOX_8",
	"GPIOX_9",  "GPIOY_0",  "GPIOY_1",  "GPIOY_2",  "GPIOY_3",
	"GPIOY_4",  "GPIOY_5",  "GPIOY_6",  "GPIOY_7",  "GPIOY_8",
	"GPIOY_9",  "GPIOZ_0",  "GPIOZ_1",  "GPIOZ_2",  "GPIOZ_3",
	"GPIOZ_4",  "GPIOZ_5",  "GPIOZ_6",  "GPIOZ_7",  "GPIOZ_8",
	"GPIOZ_9",  "GPIOA_10", "GPIOA_11", "GPIOA_12", "GPIOA_13",
	"GPIOA_14", "GPIOA_15", "GPIOA_16", "GPIOA_17", "GPIOA_18",
	"GPIOA_19", "GPIOB_10", "GPIOB_11", "GPIOB_12", "GPIOB_13",
	"GPIOX_10", "GPIOX_11", "GPIOX_12", "GPIOX_13", "GPIOX_14",
	"GPIOX_15", "GPIOX_16", "GPIOX_17", "GPIOZ_10", "GPIOZ_11",
	"GPIOZ_12", "GPIOZ_13", "GPIOZ_14", "GPIOZ_15", "GPIOCC_0",
	"GPIOCC_1"
};

static const char * const can_a_groups[] = {
	"can_a_rx", "can_a_tx"
};

static const char * const can_b_groups[] = {
	"can_b_rx", "can_b_tx"
};

static const char * const debug_groups[] = {
	"debug_i0",  "debug_i1",  "debug_i2",  "debug_i3",  "debug_i4",
	"debug_i5",  "debug_i6",  "debug_i7",  "debug_i8",  "debug_i9",
	"debug_o0",  "debug_o1",  "debug_o2",  "debug_o3",  "debug_o4",
	"debug_o5",  "debug_o6",  "debug_o7",  "debug_o8",  "debug_o9",
	"debug_i10", "debug_i11", "debug_i12", "debug_i13", "debug_i14",
	"debug_i15", "debug_i16", "debug_i17", "debug_i18", "debug_i19",
	"debug_o10", "debug_o11", "debug_o12", "debug_o13", "debug_o14",
	"debug_o15", "debug_o16", "debug_o17", "debug_o18", "debug_o19"
};

static const char * const dp_hpd_groups[] = {
	"dp_hpd",    "edp_hpd", "dp_hpd_m", "dp_hpd_y", "edp_hpd_m",
	"edp_hpd_y"
};

static const char * const emmc_groups[] = {
	"emmc_d0",  "emmc_d1", "emmc_d2", "emmc_d3", "emmc_d4",
	"emmc_d5",  "emmc_d6", "emmc_d7", "emmc_ds", "emmc_clk",
	"emmc_cmd"
};

static const char * const eth_groups[] = {
	"eth_mdc",          "eth_mdio",       "eth_rxd0",
	"eth_rxd1",         "eth_txd0",       "eth_txd1",
	"eth_txen",         "eth_mdc_z",      "eth_rx_dv",
	"eth_mdio_z",       "eth_rxd2_rgmii", "eth_rxd3_rgmii",
	"eth_txd2_rgmii",   "eth_txd3_rgmii", "eth_rgmii_rx_clk",
	"eth_rgmii_tx_clk"
};

static const char * const hdmirx_groups[] = {
	"hdmirx_hpd", "hdmirx_scl", "hdmirx_sda", "hdmirx_5vdet"
};

static const char * const hdmitx_groups[] = {
	"hdmitx_scl", "hdmitx_sda", "hdmitx_hpd_in"
};

static const char * const i2c_c_groups[] = {
	"i2c_c_scl", "i2c_c_sda"
};

static const char * const i2c_d_groups[] = {
	"i2c_d_scl", "i2c_d_sda"
};

static const char * const i2c_e_groups[] = {
	"i2c_e_scl",   "i2c_e_sda",   "i2c_e_scl_m", "i2c_e_scl_y",
	"i2c_e_scl_z", "i2c_e_sda_m", "i2c_e_sda_y", "i2c_e_sda_z"
};

static const char * const i2c_f_groups[] = {
	"i2c_f_scl",   "i2c_f_sda",   "i2c_f_scl_h", "i2c_f_scl_m",
	"i2c_f_sda_h", "i2c_f_sda_m", "i2c_f_sda_cc", "i2c_f_scl_cc"
};

static const char * const i2c_g_groups[] = {
	"i2c_g_scl",   "i2c_g_sda",   "i2c_g_scl_m", "i2c_g_scl_z",
	"i2c_g_sda_m", "i2c_g_sda_z"
};

static const char * const i2c_h_groups[] = {
	"i2c_h_scl",   "i2c_h_sda",   "i2c_h_scl_m", "i2c_h_scl_z",
	"i2c_h_sda_m", "i2c_h_sda_z"
};

static const char * const i2c_i_groups[] = {
	"i2c_i_scl",   "i2c_i_sda",   "i2c_i_scl_a", "i2c_i_scl_y",
	"i2c_i_sda_a", "i2c_i_sda_y"
};

static const char * const i3c_a_groups[] = {
	"i3c_a_scl", "i3c_a_sda"
};

static const char * const i3c_b_groups[] = {
	"i3c_b_scl",    "i3c_b_sda_d0", "i3c_b_sda_d1", "i3c_b_sda_d2",
	"i3c_b_sda_d3"
};

static const char * const iso7816_groups[] = {
	"iso7816_clk",     "iso7816_data",     "iso7816_clk_a",
	"iso7816_clk_z",   "iso7816_clk_a0",   "iso7816_data_a",
	"iso7816_data_z",  "iso7816_clk_a16",  "iso7816_clk_z10",
	"iso7816_data_a1", "iso7816_data_a17", "iso7816_data_z11"
};

static const char * const mclk_groups[] = {
	"mclk_1",   "mclk_2", "mclk_1_a", "mclk_1_y", "mclk_2_m",
	"mclk_2_z"
};

static const char * const nand_groups[] = {
	"nand_ale",     "nand_ce0", "nand_cle", "nand_ren_wr",
	"nand_wen_clk"
};

static const char * const pcie_groups[] = {
	"pcie_a_preset",   "pcie_b_preset",   "pcieck_a_reqn",
	"pcieck_b_reqn",   "pcie_a_preset_y", "pcie_b_preset_y",
	"pcieck_a_reqn_h", "pcieck_a_reqn_y", "pcieck_b_reqn_h",
	"pcieck_b_reqn_m", "pcieck_b_reqn_y"
};

static const char * const pdm_groups[] = {
	"pdm_dclk",   "pdm_din0",   "pdm_din1",   "pdm_din2",
	"pdm_din3",   "pdm_dclk_m", "pdm_dclk_y", "pdm_din0_m",
	"pdm_din0_y", "pdm_din1_m", "pdm_din1_y", "pdm_din2_m",
	"pdm_din2_y", "pdm_din3_m", "pdm_din3_y"
};

static const char * const pio_groups[] = {
	"pio0",  "pio1",  "pio2",  "pio3",  "pio4",  "pio5",
	"pio6",  "pio7",  "pio8",  "pio9",  "pio10", "pio11",
	"pio12", "pio13", "pio14", "pio15", "pio16", "pio17",
	"pio18", "pio19", "pio20", "pio21", "pio22", "pio23",
	"pio24", "pio25", "pio26", "pio27", "pio28", "pio29",
	"pio30", "pio31"
};

static const char * const pwm_c_groups[] = {
	"pwm_c"
};

static const char * const pwm_f_groups[] = {
	"pwm_f"
};

static const char * const pwm_h_groups[] = {
	"pwm_h"
};

static const char * const pwm_i_groups[] = {
	"pwm_i"
};

static const char * const pwm_j_groups[] = {
	"pwm_j", "pwm_j_m"
};

static const char * const pwm_k_groups[] = {
	"pwm_k", "pwm_k_y"
};

static const char * const pwm_l_groups[] = {
	"pwm_l"
};

static const char * const pwm_m_groups[] = {
	"pwm_m", "pwm_m_y"
};

static const char * const pwm_n_groups[] = {
	"pwm_n"
};

static const char * const pwm_vs_groups[] = {
	"pwm_vs", "pwm_vs_m", "pwm_vs_y"
};

static const char * const rt_gpio_groups[] = {
	"rt_gpio_19", "rt_gpio_20", "rt_gpio_21", "rt_gpio_22",
	"rt_gpio_23", "rt_gpio_24", "rt_gpio_25", "rt_gpio_26",
	"rt_gpio_27", "rt_gpio_28", "rt_gpio_29", "rt_gpio_30",
	"rt_gpio_31", "rt_gpio_32", "rt_gpio_33", "rt_gpio_34",
	"rt_gpio_35", "rt_gpio_36", "rt_gpio_37"
};

static const char * const sdcard_groups[] = {
	"sdcard_d0",  "sdcard_d1",  "sdcard_d2", "sdcard_d3",
	"sdcard_clk", "sdcard_cmd"
};

static const char * const sdio_groups[] = {
	"sdio_d0",  "sdio_d1", "sdio_d2", "sdio_d3", "sdio_clk",
	"sdio_cmd"
};

static const char * const spdif_groups[] = {
	"spdif_in", "spdif_out", "spdif_in_y", "spdif_out_y"
};

static const char * const spi_b_groups[] = {
	"spi_b_ss0",  "spi_b_ss1",  "spi_b_ss2", "spi_b_miso",
	"spi_b_mosi", "spi_b_sclk"
};

static const char * const spi_c_groups[] = {
	"spi_c_ss0",   "spi_c_miso",   "spi_c_mosi",   "spi_c_sclk",
	"spi_c_ss0_m", "spi_c_miso_m", "spi_c_mosi_m", "spi_c_sclk_m"
};

static const char * const spi_e_groups[] = {
	"spi_e_ss0", "spi_e_miso", "spi_e_mosi", "spi_e_sclk"
};

static const char * const spi_f_groups[] = {
	"spi_f_ss0", "spi_f_miso", "spi_f_mosi", "spi_f_sclk"
};

static const char * const spinf_groups[] = {
	"spinf_d4",    "spinf_d5",      "spinf_d6",    "spinf_d7",
	"spinf_clk",   "spinf_cs0",     "spinf_mi_d1", "spinf_mo_d0",
	"spinf_wp_d2", "spinf_hold_d3"
};

static const char * const tdm_groups[] = {
	"tdm_d0",      "tdm_d1",      "tdm_d2",      "tdm_d3",
	"tdm_d4",      "tdm_d5",      "tdm_d6",      "tdm_d7",
	"tdm_d8",      "tdm_d9",      "tdm_d10",     "tdm_d11",
	"tdm_d12",     "tdm_d13",     "tdm_d14",     "tdm_d15",
	"tdm_fs0",     "tdm_fs1",     "tdm_fs2",     "tdm_d2_y",
	"tdm_d2_z",    "tdm_d3_y",    "tdm_d3_z",    "tdm_d4_a",
	"tdm_d4_y",    "tdm_d4_z",    "tdm_d5_a",    "tdm_d5_y",
	"tdm_d5_z",    "tdm_d6_a",    "tdm_d6_y",    "tdm_d6_z",
	"tdm_d7_a",    "tdm_d7_y",    "tdm_d7_z",    "tdm_d8_a",
	"tdm_d8_y",    "tdm_d8_z",    "tdm_d9_a",    "tdm_d9_m",
	"tdm_d9_z",    "tdm_d10_m",   "tdm_d10_z",   "tdm_d11_m",
	"tdm_d14_m",   "tdm_d15_m",   "tdm_fs1_a",   "tdm_fs1_y",
	"tdm_fs1_z",   "tdm_fs2_m",   "tdm_fs2_z",   "tdm_sclk0",
	"tdm_sclk1",   "tdm_sclk2",   "tdm_sclk1_a", "tdm_sclk1_y",
	"tdm_sclk1_z", "tdm_sclk2_m", "tdm_sclk2_z"
};

static const char * const tsin_a_groups[] = {
	"tsin_a_clk",   "tsin_a_sop",   "tsin_a_din0",   "tsin_a_clk_z",
	"tsin_a_sop_z", "tsin_a_valid", "tsin_a_din0_z", "tsin_a_valid_z"
};

static const char * const tsin_b_groups[] = {
	"tsin_b_clk",     "tsin_b_sop",      "tsin_b_din0",
	"tsin_b_din1",    "tsin_b_din2",     "tsin_b_din3",
	"tsin_b_din4",    "tsin_b_din5",     "tsin_b_din6",
	"tsin_b_din7",    "tsin_b_fail",     "tsin_b_clk_a",
	"tsin_b_clk_z",   "tsin_b_sop_a",    "tsin_b_sop_z",
	"tsin_b_valid",   "tsin_b_clk_a7",   "tsin_b_din0_a",
	"tsin_b_din0_z",  "tsin_b_din1_z",   "tsin_b_din2_z",
	"tsin_b_din3_z",  "tsin_b_din4_z",   "tsin_b_din5_z",
	"tsin_b_din6_z",  "tsin_b_din7_z",   "tsin_b_fail_z",
	"tsin_b_sop_a4",  "tsin_b_din0_a6",  "tsin_b_valid_a",
	"tsin_b_valid_z", "tsin_b_valid_a5"
};

static const char * const tst_groups[] = {
	"tst_out0",  "tst_out1",  "tst_out2",  "tst_out3",  "tst_out4",
	"tst_out5",  "tst_out6",  "tst_out7",  "tst_out8",  "tst_out9",
	"tst_out10", "tst_out11", "tst_out12", "tst_out13", "tst_out14",
	"tst_out15", "tst_out16", "tst_out17", "tst_out18", "tst_out19"
};

static const char * const uart_a_groups[] = {
	"uart_a_rx", "uart_a_tx", "uart_a_cts", "uart_a_rts"
};

static const char * const uart_c_groups[] = {
	"uart_c_rx", "uart_c_tx"
};

static const char * const uart_d_groups[] = {
	"uart_d_rx",   "uart_d_tx",   "uart_d_cts",   "uart_d_rts",
	"uart_d_rx_m", "uart_d_tx_m", "uart_d_cts_m", "uart_d_rts_m"
};

static const char * const uart_e_groups[] = {
	"uart_e_rx", "uart_e_tx", "uart_e_cts", "uart_e_rts"
};

static const char * const uart_f_groups[] = {
	"uart_f_rx", "uart_f_tx", "uart_f_cts", "uart_f_rts"
};

static const char * const vx1_groups[] = {
	"vx1_htpdn",   "vx1_lockn",   "vx1_htpdn_m", "vx1_htpdn_y",
	"vx1_lockn_m", "vx1_lockn_y"
};

static const char * const usb_cc_groups[] = {
	"usb_cc0", "usb_cc1"
};

static struct meson_pmx_func meson_a9_periphs_functions[] __initdata = {
	FUNCTION(gpio_periphs),
	FUNCTION(can_a),
	FUNCTION(can_b),
	FUNCTION(debug),
	FUNCTION(dp_hpd),
	FUNCTION(emmc),
	FUNCTION(eth),
	FUNCTION(hdmirx),
	FUNCTION(hdmitx),
	FUNCTION(i2c_c),
	FUNCTION(i2c_d),
	FUNCTION(i2c_e),
	FUNCTION(i2c_f),
	FUNCTION(i2c_g),
	FUNCTION(i2c_h),
	FUNCTION(i2c_i),
	FUNCTION(i3c_a),
	FUNCTION(i3c_b),
	FUNCTION(iso7816),
	FUNCTION(mclk),
	FUNCTION(nand),
	FUNCTION(pcie),
	FUNCTION(pdm),
	FUNCTION(pio),
	FUNCTION(pwm_c),
	FUNCTION(pwm_f),
	FUNCTION(pwm_h),
	FUNCTION(pwm_i),
	FUNCTION(pwm_j),
	FUNCTION(pwm_k),
	FUNCTION(pwm_l),
	FUNCTION(pwm_m),
	FUNCTION(pwm_n),
	FUNCTION(pwm_vs),
	FUNCTION(rt_gpio),
	FUNCTION(sdcard),
	FUNCTION(sdio),
	FUNCTION(spdif),
	FUNCTION(spi_b),
	FUNCTION(spi_c),
	FUNCTION(spi_e),
	FUNCTION(spi_f),
	FUNCTION(spinf),
	FUNCTION(tdm),
	FUNCTION(tsin_a),
	FUNCTION(tsin_b),
	FUNCTION(tst),
	FUNCTION(uart_a),
	FUNCTION(uart_c),
	FUNCTION(uart_d),
	FUNCTION(uart_e),
	FUNCTION(uart_f),
	FUNCTION(vx1),
	FUNCTION(usb_cc)
};

static struct meson_bank meson_a9_periphs_banks[] = {
	/*    name   first   last   irq   pullen   pull   dir   out   in   ds */
	BANK_DS("B",    GPIOB_0,  GPIOB_13,   0,  13,
		0x063,  0, 0x064,  0, 0x062,  0, 0x061,  0, 0x060,  0, 0x067,  0),
	BANK_DS("X",    GPIOX_0,  GPIOX_17,  30,  47,
		0x013,  0, 0x014,  0, 0x012,  0, 0x011,  0, 0x010,  0, 0x017,  0),
	BANK_DS("A",    GPIOA_0,  GPIOA_19,  64,  83,
		0x073,  0, 0x074,  0, 0x072,  0, 0x071,  0, 0x070,  0, 0x077,  0),
	BANK_DS("Z",    GPIOZ_0,  GPIOZ_15,  48,  63,
		0x003,  0, 0x004,  0, 0x002,  0, 0x001,  0, 0x000,  0, 0x007,  0),
	BANK_DS("H",    GPIOH_0,   GPIOH_7,  22,  29,
		0x023,  0, 0x024,  0, 0x022,  0, 0x021,  0, 0x020,  0, 0x027,  0),
	BANK_DS("Y",    GPIOY_0,   GPIOY_9,  86,  95,
		0x083,  0, 0x084,  0, 0x082,  0, 0x081,  0, 0x080,  0, 0x087,  0),
	BANK_DS("M",    GPIOM_0,   GPIOM_7,  14,  21,
		0x03b,  0, 0x03c,  0, 0x03a,  0, 0x039,  0, 0x038,  0, 0x03f,  0),
	BANK_DS("CC",   GPIOCC_0, GPIOCC_1,  84,  85,
		0x090,  8, 0x090, 16, 0x092,  0, 0x091,  0, 0x090,  0, 0x090,  24)
};

static struct meson_pmx_bank meson_a9_periphs_pmx_banks[] = {
	/*       name      first        last         reg   offset */
	BANK_PMX("B",      GPIOB_0,    GPIOB_13,     0x000,  0),
	BANK_PMX("X",      GPIOX_0,    GPIOX_17,     0x003,  0),
	BANK_PMX("A",      GPIOA_0,    GPIOA_19,     0x010,  0),
	BANK_PMX("Z",      GPIOZ_0,    GPIOZ_15,     0x006,  0),
	BANK_PMX("H",      GPIOH_0,     GPIOH_7,     0x00b,  0),
	BANK_PMX("Y",      GPIOY_0,     GPIOY_9,     0x00c,  0),
	BANK_PMX("M",      GPIOM_0,     GPIOM_7,     0x008,  0),
	BANK_PMX("CC",     GPIOCC_0,   GPIOCC_1,     0x005, 24)
};

static struct meson_axg_pmx_data meson_a9_periphs_pmx_banks_data = {
	.pmx_banks	= meson_a9_periphs_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_a9_periphs_pmx_banks),
};

static struct meson_pinctrl_data meson_a9_periphs_pinctrl_data __refdata = {
	.name		= "periphs-banks",
	.pins		= meson_a9_periphs_pins,
	.groups		= meson_a9_periphs_groups,
	.funcs		= meson_a9_periphs_functions,
	.banks		= meson_a9_periphs_banks,
	.num_pins	= ARRAY_SIZE(meson_a9_periphs_pins),
	.num_groups	= ARRAY_SIZE(meson_a9_periphs_groups),
	.num_funcs	= ARRAY_SIZE(meson_a9_periphs_functions),
	.num_banks	= ARRAY_SIZE(meson_a9_periphs_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_a9_periphs_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

/********************************** GPIO AO DOMAIN *******************************/
static const struct pinctrl_pin_desc meson_a9_aobus_pins[] = {
	/* GPIOD (AO domain) */
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
	MESON_PIN(GPIOD_12),
	MESON_PIN(GPIOD_13),
	MESON_PIN(GPIOD_14),
	MESON_PIN(GPIOD_15),
	MESON_PIN(GPIOD_16),
	/* GPIO_AO (AO domain) */
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
	/* GPIO_C (AO domain) */
	MESON_PIN(GPIOC_0),
	MESON_PIN(GPIOC_1),
	MESON_PIN(GPIOC_2),
	MESON_PIN(GPIOC_3),
	MESON_PIN(GPIOC_4),
	MESON_PIN(GPIOC_5),
	MESON_PIN(GPIOC_6)
};

/* Bank[D]:Function[1] */
static const unsigned int uart_b_tx_ao_pins[]		= { GPIOD_0 };
static const unsigned int uart_b_rx_ao_pins[]		= { GPIOD_1 };
static const unsigned int i2c_a_scl_ao_pins[]		= { GPIOD_2 };
static const unsigned int i2c_a_sda_ao_pins[]		= { GPIOD_3 };
static const unsigned int ir_out_ao_pins[]		= { GPIOD_4 };
static const unsigned int ir_in_ao_pins[]		= { GPIOD_5 };
static const unsigned int i2c_a_scl_ao_d_pins[]		= { GPIOD_6 };
static const unsigned int i2c_a_sda_ao_d_pins[]		= { GPIOD_7 };
static const unsigned int pwm_a_hiz_ao_pins[]		= { GPIOD_10 };
static const unsigned int spdif_in_ao_pins[]		= { GPIOD_11 };
static const unsigned int spdif_out_ao_pins[]		= { GPIOD_12 };
static const unsigned int i2c_c_scl_ao_pins[]		= { GPIOD_13 };
static const unsigned int i2c_c_sda_ao_pins[]		= { GPIOD_14 };
static const unsigned int i2c_d_scl_ao_pins[]		= { GPIOD_15 };
static const unsigned int i2c_d_sda_ao_pins[]		= { GPIOD_16 };

/* Bank[D]:Function[2] */
static const unsigned int spi_a_sclk_ao_pins[]		= { GPIOD_2 };
static const unsigned int spi_a_mosi_ao_pins[]		= { GPIOD_3 };
static const unsigned int spi_a_miso_ao_pins[]		= { GPIOD_4 };
static const unsigned int spi_a_ss0_ao_pins[]		= { GPIOD_5 };
static const unsigned int spi_a_ss1_ao_pins[]		= { GPIOD_6 };
static const unsigned int spi_a_ss2_ao_pins[]		= { GPIOD_7 };
static const unsigned int jtag_a_clk_ao_pins[]		= { GPIOD_8 };
static const unsigned int jtag_a_tms_ao_pins[]		= { GPIOD_9 };
static const unsigned int jtag_a_tdi_ao_pins[]		= { GPIOD_10 };
static const unsigned int jtag_a_tdo_ao_pins[]		= { GPIOD_11 };
static const unsigned int gen_clk_out_ao_pins[]		= { GPIOD_12 };
static const unsigned int spdif_out_ao_d_pins[]		= { GPIOD_13 };
static const unsigned int cec_ao_pins[]			= { GPIOD_14 };
static const unsigned int pmic_sleep_ao_pins[]		= { GPIOD_15 };

/* Bank[D]:Function[3] */
static const unsigned int i3c_a_scl_ao_pins[]		= { GPIOD_2 };
static const unsigned int i3c_a_sda_ao_pins[]		= { GPIOD_3 };
static const unsigned int mic_mute_en_ao_pins[]		= { GPIOD_6 };
static const unsigned int mic_mute_key_ao_pins[]	= { GPIOD_7 };
static const unsigned int ir_out_ao_d_pins[]		= { GPIOD_8 };
static const unsigned int ir_in_ao_d_pins[]		= { GPIOD_9 };
static const unsigned int pwm_a_ao_pins[]		= { GPIOD_10 };
static const unsigned int pwm_g_ao_pins[]		= { GPIOD_11 };
static const unsigned int spdif_in_ao_d_pins[]		= { GPIOD_13 };
static const unsigned int str_en_ao_pins[]		= { GPIOD_15 };

/* Bank[D]:Function[4] */
static const unsigned int pwm_c_hiz_ao_pins[]		= { GPIOD_10 };
static const unsigned int pwm_g_hiz_ao_pins[]		= { GPIOD_11 };

/* Bank[D]:Function[5] */
static const unsigned int uart_e_tx_ao_pins[]		= { GPIOD_8 };
static const unsigned int uart_e_rx_ao_pins[]		= { GPIOD_9 };
static const unsigned int uart_e_cts_ao_pins[]		= { GPIOD_10 };
static const unsigned int uart_e_rts_ao_pins[]		= { GPIOD_11 };

/* Bank[D]:Function[6] */
static const unsigned int rt_gpio_0_ao_pins[]		= { GPIOD_2 };
static const unsigned int rt_gpio_1_ao_pins[]		= { GPIOD_4 };
static const unsigned int rt_gpio_2_ao_pins[]		= { GPIOD_5 };
static const unsigned int rt_gpio_3_ao_pins[]		= { GPIOD_6 };
static const unsigned int rt_gpio_4_ao_pins[]		= { GPIOD_10 };
static const unsigned int rt_gpio_5_ao_pins[]		= { GPIOD_11 };
static const unsigned int rt_gpio_6_ao_pins[]		= { GPIOD_13 };
static const unsigned int rt_gpio_7_ao_pins[]		= { GPIOD_14 };

/* Bank[AO]:Function[1] */
static const unsigned int pwm_a_ao_ao_pins[]		= { GPIOAO_0 };
static const unsigned int pwm_b_ao_pins[]		= { GPIOAO_1 };
static const unsigned int pwm_c_ao_pins[]		= { GPIOAO_2 };
static const unsigned int pwm_d_ao_pins[]		= { GPIOAO_3 };
static const unsigned int pwm_e_ao_pins[]		= { GPIOAO_4 };
static const unsigned int pwm_f_ao_pins[]		= { GPIOAO_5 };
static const unsigned int pdm_din0_ao_pins[]		= { GPIOAO_6 };
static const unsigned int pdm_din1_ao_pins[]		= { GPIOAO_7 };
static const unsigned int pdm_din2_ao_pins[]		= { GPIOAO_8 };
static const unsigned int pdm_din3_ao_pins[]		= { GPIOAO_9 };
static const unsigned int pdm_dclk_ao_pins[]		= { GPIOAO_10 };
static const unsigned int i2c_d_scl_ao_ao_pins[]	= { GPIOAO_11 };
static const unsigned int i2c_d_sda_ao_ao_pins[]	= { GPIOAO_12 };

/* Bank[AO]:Function[2] */
static const unsigned int i2c_a_scl_ao_ao_pins[]	= { GPIOAO_0 };
static const unsigned int i2c_a_sda_ao_ao_pins[]	= { GPIOAO_1 };
static const unsigned int clk25m_ao_pins[]		= { GPIOAO_2 };
static const unsigned int clk12m_24m_ao_pins[]		= { GPIOAO_3 };
static const unsigned int i2c_b_scl_ao_pins[]		= { GPIOAO_4 };
static const unsigned int i2c_b_sda_ao_pins[]		= { GPIOAO_5 };
static const unsigned int i2c_c_scl_ao_ao_pins[]	= { GPIOAO_6 };
static const unsigned int i2c_c_sda_ao_ao_pins[]	= { GPIOAO_7 };
static const unsigned int i2c_d_scl_ao_ao8_pins[]	= { GPIOAO_8 };
static const unsigned int i2c_d_sda_ao_ao9_pins[]	= { GPIOAO_9 };
static const unsigned int pwm_a_ao_ao11_pins[]		= { GPIOAO_11 };

/* Bank[AO]:Function[3] */
static const unsigned int spi_a_sclk_ao_ao_pins[]	= { GPIOAO_0 };
static const unsigned int spi_a_mosi_ao_ao_pins[]	= { GPIOAO_1 };
static const unsigned int spi_a_miso_ao_ao_pins[]	= { GPIOAO_2 };
static const unsigned int spi_a_ss0_ao_ao_pins[]	= { GPIOAO_3 };
static const unsigned int spi_b_mosi_ao_pins[]		= { GPIOAO_4 };
static const unsigned int spi_b_miso_ao_pins[]		= { GPIOAO_5 };
static const unsigned int spi_b_sclk_ao_pins[]		= { GPIOAO_6 };
static const unsigned int spi_b_ss0_ao_pins[]		= { GPIOAO_7 };
static const unsigned int spi_b_ss1_ao_pins[]		= { GPIOAO_8 };
static const unsigned int spi_b_ss2_ao_pins[]		= { GPIOAO_9 };

/* Bank[AO]:Function[4] */
static const unsigned int pmic_sleep_ao_ao_pins[]	= { GPIOAO_0 };
static const unsigned int str_en_ao_ao_pins[]		= { GPIOAO_1 };
static const unsigned int uart_c_cts_ao_pins[]		= { GPIOAO_2 };
static const unsigned int uart_c_rts_ao_pins[]		= { GPIOAO_3 };
static const unsigned int uart_c_tx_ao_pins[]		= { GPIOAO_4 };
static const unsigned int uart_c_rx_ao_pins[]		= { GPIOAO_5 };
static const unsigned int pwm_g_ao_ao_pins[]		= { GPIOAO_6 };

/* Bank[AO]:Function[5] */
static const unsigned int clk_32k_out_ao_pins[]		= { GPIOAO_2 };

/* Bank[AO]:Function[6] */
static const unsigned int rt_gpio_8_ao_pins[]		= { GPIOAO_0 };
static const unsigned int rt_gpio_9_ao_pins[]		= { GPIOAO_1 };
static const unsigned int rt_gpio_10_ao_pins[]		= { GPIOAO_2 };
static const unsigned int rt_gpio_11_ao_pins[]		= { GPIOAO_3 };
static const unsigned int rt_gpio_12_ao_pins[]		= { GPIOAO_4 };
static const unsigned int rt_gpio_13_ao_pins[]		= { GPIOAO_5 };
static const unsigned int rt_gpio_14_ao_pins[]		= { GPIOAO_11 };
static const unsigned int rt_gpio_15_ao_pins[]		= { GPIOAO_12 };

/* Bank[C]:Function[1] */
static const unsigned int sdcard_d0_ao_pins[]		= { GPIOC_0 };
static const unsigned int sdcard_d1_ao_pins[]		= { GPIOC_1 };
static const unsigned int sdcard_d2_ao_pins[]		= { GPIOC_2 };
static const unsigned int sdcard_d3_ao_pins[]		= { GPIOC_3 };
static const unsigned int sdcard_clk_ao_pins[]		= { GPIOC_4 };
static const unsigned int sdcard_cmd_ao_pins[]		= { GPIOC_5 };
static const unsigned int gen_clk_out_ao_c_pins[]	= { GPIOC_6 };

/* Bank[C]:Function[2] */
static const unsigned int jtag_b_tdo_ao_pins[]		= { GPIOC_0 };
static const unsigned int jtag_b_tdi_ao_pins[]		= { GPIOC_1 };
static const unsigned int jtag_b_clk_ao_pins[]		= { GPIOC_4 };
static const unsigned int jtag_b_tms_ao_pins[]		= { GPIOC_5 };
static const unsigned int pcieck_a_reqn_ao_pins[]	= { GPIOC_6 };

/* Bank[C]:Function[3] */
static const unsigned int spi_d_mosi_ao_pins[]		= { GPIOC_0 };
static const unsigned int spi_d_miso_ao_pins[]		= { GPIOC_1 };
static const unsigned int spi_d_sclk_ao_pins[]		= { GPIOC_2 };
static const unsigned int spi_d_ss0_ao_pins[]		= { GPIOC_3 };
static const unsigned int spi_d_ss1_ao_pins[]		= { GPIOC_4 };
static const unsigned int spi_d_ss2_ao_pins[]		= { GPIOC_5 };

/* Bank[C]:Function[4] */
static const unsigned int i2c_c_scl_ao_c_pins[]		= { GPIOC_0 };
static const unsigned int i2c_c_sda_ao_c_pins[]		= { GPIOC_1 };
static const unsigned int i2c_b_scl_ao_c_pins[]		= { GPIOC_2 };
static const unsigned int i2c_b_sda_ao_c_pins[]		= { GPIOC_3 };
static const unsigned int i2c_a_scl_ao_c_pins[]		= { GPIOC_4 };
static const unsigned int i2c_a_sda_ao_c_pins[]		= { GPIOC_5 };

/* Bank[C]:Function[5] */
static const unsigned int pdm_din0_ao_c_pins[]		= { GPIOC_0 };
static const unsigned int pdm_din1_ao_c_pins[]		= { GPIOC_1 };
static const unsigned int pdm_din2_ao_c_pins[]		= { GPIOC_2 };
static const unsigned int pdm_din3_ao_c_pins[]		= { GPIOC_3 };
static const unsigned int pdm_dclk_ao_c_pins[]		= { GPIOC_4 };

/* Bank[C]:Function[6] */
static const unsigned int uart_d_cts_ao_pins[]		= { GPIOC_0 };
static const unsigned int uart_d_rts_ao_pins[]		= { GPIOC_1 };
static const unsigned int rt_gpio_16_ao_pins[]		= { GPIOC_2 };
static const unsigned int rt_gpio_17_ao_pins[]		= { GPIOC_3 };
static const unsigned int rt_gpio_18_ao_pins[]		= { GPIOC_4 };
static const unsigned int uart_d_rx_ao_pins[]		= { GPIOC_5 };
static const unsigned int uart_d_tx_ao_pins[]		= { GPIOC_6 };

/* Bank[C]:Function[7] */
static const unsigned int i3c_a_scl_ao_c_pins[]		= { GPIOC_0 };
static const unsigned int i3c_a_sda_ao_c_pins[]		= { GPIOC_1 };

static struct meson_pmx_group meson_a9_aobus_groups[] __initdata = {
	/* GPIOD (AO domain) */
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
	GPIO_GROUP(GPIOD_12),
	GPIO_GROUP(GPIOD_13),
	GPIO_GROUP(GPIOD_14),
	GPIO_GROUP(GPIOD_15),
	GPIO_GROUP(GPIOD_16),
	/* GPIO_AO (AO domain) */
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
	/* GPIO_C (AO domain) */
	GPIO_GROUP(GPIOC_0),
	GPIO_GROUP(GPIOC_1),
	GPIO_GROUP(GPIOC_2),
	GPIO_GROUP(GPIOC_3),
	GPIO_GROUP(GPIOC_4),
	GPIO_GROUP(GPIOC_5),
	GPIO_GROUP(GPIOC_6),
	/* Bank[D]:Function[1] */
	GROUP(uart_b_tx_ao,		1),
	GROUP(uart_b_rx_ao,		1),
	GROUP(i2c_a_scl_ao,		1),
	GROUP(i2c_a_sda_ao,		1),
	GROUP(ir_out_ao,		1),
	GROUP(ir_in_ao,			1),
	GROUP(i2c_a_scl_ao_d,		1),
	GROUP(i2c_a_sda_ao_d,		1),
	GROUP(pwm_a_hiz_ao,		1),
	GROUP(spdif_in_ao,		1),
	GROUP(spdif_out_ao,		1),
	GROUP(i2c_c_scl_ao,		1),
	GROUP(i2c_c_sda_ao,		1),
	GROUP(i2c_d_scl_ao,		1),
	GROUP(i2c_d_sda_ao,		1),
	/* Bank[D]:Function[2] */
	GROUP(spi_a_sclk_ao,		2),
	GROUP(spi_a_mosi_ao,		2),
	GROUP(spi_a_miso_ao,		2),
	GROUP(spi_a_ss0_ao,		2),
	GROUP(spi_a_ss1_ao,		2),
	GROUP(spi_a_ss2_ao,		2),
	GROUP(jtag_a_clk_ao,		2),
	GROUP(jtag_a_tms_ao,		2),
	GROUP(jtag_a_tdi_ao,		2),
	GROUP(jtag_a_tdo_ao,		2),
	GROUP(gen_clk_out_ao,		2),
	GROUP(spdif_out_ao_d,		2),
	GROUP(cec_ao,			2),
	GROUP(pmic_sleep_ao,		2),
	/* Bank[D]:Function[3] */
	GROUP(i3c_a_scl_ao,		3),
	GROUP(i3c_a_sda_ao,		3),
	GROUP(mic_mute_en_ao,		3),
	GROUP(mic_mute_key_ao,		3),
	GROUP(ir_out_ao_d,		3),
	GROUP(ir_in_ao_d,		3),
	GROUP(pwm_a_ao,			3),
	GROUP(pwm_g_ao,			3),
	GROUP(spdif_in_ao_d,		3),
	GROUP(str_en_ao,		3),
	/* Bank[D]:Function[4] */
	GROUP(pwm_c_hiz_ao,		4),
	GROUP(pwm_g_hiz_ao,		4),
	/* Bank[D]:Function[5] */
	GROUP(uart_e_tx_ao,		5),
	GROUP(uart_e_rx_ao,		5),
	GROUP(uart_e_cts_ao,		5),
	GROUP(uart_e_rts_ao,		5),
	/* Bank[D]:Function[6] */
	GROUP(rt_gpio_0_ao,		6),
	GROUP(rt_gpio_1_ao,		6),
	GROUP(rt_gpio_2_ao,		6),
	GROUP(rt_gpio_3_ao,		6),
	GROUP(rt_gpio_4_ao,		6),
	GROUP(rt_gpio_5_ao,		6),
	GROUP(rt_gpio_6_ao,		6),
	GROUP(rt_gpio_7_ao,		6),
	/* Bank[AO]:Function[1] */
	GROUP(pwm_a_ao_ao,		1),
	GROUP(pwm_b_ao,			1),
	GROUP(pwm_c_ao,			1),
	GROUP(pwm_d_ao,			1),
	GROUP(pwm_e_ao,			1),
	GROUP(pwm_f_ao,			1),
	GROUP(pdm_din0_ao,		1),
	GROUP(pdm_din1_ao,		1),
	GROUP(pdm_din2_ao,		1),
	GROUP(pdm_din3_ao,		1),
	GROUP(pdm_dclk_ao,		1),
	GROUP(i2c_d_scl_ao_ao,		1),
	GROUP(i2c_d_sda_ao_ao,		1),
	/* Bank[AO]:Function[2] */
	GROUP(i2c_a_scl_ao_ao,		2),
	GROUP(i2c_a_sda_ao_ao,		2),
	GROUP(clk25m_ao,		2),
	GROUP(clk12m_24m_ao,		2),
	GROUP(i2c_b_scl_ao,		2),
	GROUP(i2c_b_sda_ao,		2),
	GROUP(i2c_c_scl_ao_ao,		2),
	GROUP(i2c_c_sda_ao_ao,		2),
	GROUP(i2c_d_scl_ao_ao8,		2),
	GROUP(i2c_d_sda_ao_ao9,		2),
	GROUP(pwm_a_ao_ao11,		2),
	/* Bank[AO]:Function[3] */
	GROUP(spi_a_sclk_ao_ao,		3),
	GROUP(spi_a_mosi_ao_ao,		3),
	GROUP(spi_a_miso_ao_ao,		3),
	GROUP(spi_a_ss0_ao_ao,		3),
	GROUP(spi_b_mosi_ao,		3),
	GROUP(spi_b_miso_ao,		3),
	GROUP(spi_b_sclk_ao,		3),
	GROUP(spi_b_ss0_ao,		3),
	GROUP(spi_b_ss1_ao,		3),
	GROUP(spi_b_ss2_ao,		3),
	/* Bank[AO]:Function[4] */
	GROUP(pmic_sleep_ao_ao,		4),
	GROUP(str_en_ao_ao,		4),
	GROUP(uart_c_cts_ao,		4),
	GROUP(uart_c_rts_ao,		4),
	GROUP(uart_c_tx_ao,		4),
	GROUP(uart_c_rx_ao,		4),
	GROUP(pwm_g_ao_ao,		4),
	/* Bank[AO]:Function[5] */
	GROUP(clk_32k_out_ao,		5),
	/* Bank[AO]:Function[6] */
	GROUP(rt_gpio_8_ao,		6),
	GROUP(rt_gpio_9_ao,		6),
	GROUP(rt_gpio_10_ao,		6),
	GROUP(rt_gpio_11_ao,		6),
	GROUP(rt_gpio_12_ao,		6),
	GROUP(rt_gpio_13_ao,		6),
	GROUP(rt_gpio_14_ao,		6),
	GROUP(rt_gpio_15_ao,		6),
	/* Bank[C]:Function[1] */
	GROUP(sdcard_d0_ao,		1),
	GROUP(sdcard_d1_ao,		1),
	GROUP(sdcard_d2_ao,		1),
	GROUP(sdcard_d3_ao,		1),
	GROUP(sdcard_clk_ao,		1),
	GROUP(sdcard_cmd_ao,		1),
	GROUP(gen_clk_out_ao_c,		1),
	/* Bank[C]:Function[2] */
	GROUP(jtag_b_tdo_ao,		2),
	GROUP(jtag_b_tdi_ao,		2),
	GROUP(jtag_b_clk_ao,		2),
	GROUP(jtag_b_tms_ao,		2),
	GROUP(pcieck_a_reqn_ao,		2),
	/* Bank[C]:Function[3] */
	GROUP(spi_d_mosi_ao,		3),
	GROUP(spi_d_miso_ao,		3),
	GROUP(spi_d_sclk_ao,		3),
	GROUP(spi_d_ss0_ao,		3),
	GROUP(spi_d_ss1_ao,		3),
	GROUP(spi_d_ss2_ao,		3),
	/* Bank[C]:Function[4] */
	GROUP(i2c_c_scl_ao_c,		4),
	GROUP(i2c_c_sda_ao_c,		4),
	GROUP(i2c_b_scl_ao_c,		4),
	GROUP(i2c_b_sda_ao_c,		4),
	GROUP(i2c_a_scl_ao_c,		4),
	GROUP(i2c_a_sda_ao_c,		4),
	/* Bank[C]:Function[5] */
	GROUP(pdm_din0_ao_c,		5),
	GROUP(pdm_din1_ao_c,		5),
	GROUP(pdm_din2_ao_c,		5),
	GROUP(pdm_din3_ao_c,		5),
	GROUP(pdm_dclk_ao_c,		5),
	/* Bank[C]:Function[6] */
	GROUP(uart_d_cts_ao,		6),
	GROUP(uart_d_rts_ao,		6),
	GROUP(rt_gpio_16_ao,		6),
	GROUP(rt_gpio_17_ao,		6),
	GROUP(rt_gpio_18_ao,		6),
	GROUP(uart_d_rx_ao,		6),
	GROUP(uart_d_tx_ao,		6),
	/* Bank[C]:Function[7] */
	GROUP(i3c_a_scl_ao_c,		7),
	GROUP(i3c_a_sda_ao_c,		7)
};

static const char * const gpio_aobus_groups[] = {
	"GPIOC_0",   "GPIOC_1",   "GPIOC_2",  "GPIOC_3",  "GPIOC_4",
	"GPIOC_5",   "GPIOC_6",   "GPIOD_0",  "GPIOD_1",  "GPIOD_2",
	"GPIOD_3",   "GPIOD_4",   "GPIOD_5",  "GPIOD_6",  "GPIOD_7",
	"GPIOD_8",   "GPIOD_9",   "GPIOAO_0", "GPIOAO_1", "GPIOAO_2",
	"GPIOAO_3",  "GPIOAO_4",  "GPIOAO_5", "GPIOAO_6", "GPIOAO_7",
	"GPIOAO_8",  "GPIOAO_9",  "GPIOD_10", "GPIOD_11", "GPIOD_12",
	"GPIOD_13",  "GPIOD_14",  "GPIOD_15", "GPIOD_16", "GPIOAO_10",
	"GPIOAO_11", "GPIOAO_12"
};

static const char * const cec_ao_groups[] = {
	"cec_ao"
};

static const char * const clk_ao_groups[] = {
	"clk25m_ao",        "clk12m_24m_ao", "clk_32k_out_ao", "gen_clk_out_ao",
	"gen_clk_out_ao_c"
};

static const char * const i2c_a_ao_groups[] = {
	"i2c_a_scl_ao",   "i2c_a_sda_ao",   "i2c_a_scl_ao_c",  "i2c_a_scl_ao_d",
	"i2c_a_sda_ao_c", "i2c_a_sda_ao_d", "i2c_a_scl_ao_ao", "i2c_a_sda_ao_ao"
};

static const char * const i2c_b_ao_groups[] = {
	"i2c_b_scl_ao", "i2c_b_sda_ao", "i2c_b_scl_ao_c", "i2c_b_sda_ao_c"
};

static const char * const i2c_c_ao_groups[] = {
	"i2c_c_scl_ao",    "i2c_c_sda_ao",    "i2c_c_scl_ao_c", "i2c_c_sda_ao_c",
	"i2c_c_scl_ao_ao", "i2c_c_sda_ao_ao"
};

static const char * const i2c_d_ao_groups[] = {
	"i2c_d_scl_ao",     "i2c_d_sda_ao",     "i2c_d_scl_ao_ao", "i2c_d_sda_ao_ao",
	"i2c_d_scl_ao_ao8", "i2c_d_sda_ao_ao9"
};

static const char * const i3c_a_ao_groups[] = {
	"i3c_a_scl_ao", "i3c_a_sda_ao", "i3c_a_scl_ao_c", "i3c_a_sda_ao_c"
};

static const char * const ir_in_ao_groups[] = {
	"ir_in_ao", "ir_in_ao_d"
};

static const char * const ir_out_ao_groups[] = {
	"ir_out_ao", "ir_out_ao_d"
};

static const char * const jtag_a_ao_groups[] = {
	"jtag_a_clk_ao", "jtag_a_tdi_ao", "jtag_a_tdo_ao", "jtag_a_tms_ao"
};

static const char * const jtag_b_ao_groups[] = {
	"jtag_b_clk_ao", "jtag_b_tdi_ao", "jtag_b_tdo_ao", "jtag_b_tms_ao"
};

static const char * const mic_mute_ao_groups[] = {
	"mic_mute_en_ao", "mic_mute_key_ao"
};

static const char * const pcie_ao_groups[] = {
	"pcieck_a_reqn_ao"
};

static const char * const pdm_ao_groups[] = {
	"pdm_dclk_ao",   "pdm_din0_ao",   "pdm_din1_ao",   "pdm_din2_ao",
	"pdm_din3_ao",   "pdm_dclk_ao_c", "pdm_din0_ao_c", "pdm_din1_ao_c",
	"pdm_din2_ao_c", "pdm_din3_ao_c"
};

static const char * const pmic_ao_groups[] = {
	"pmic_sleep_ao", "pmic_sleep_ao_ao"
};

static const char * const pwm_a_ao_groups[] = {
	"pwm_a_ao", "pwm_a_ao_ao", "pwm_a_hiz_ao", "pwm_a_ao_ao11"
};

static const char * const pwm_b_ao_groups[] = {
	"pwm_b_ao"
};

static const char * const pwm_c_ao_groups[] = {
	"pwm_c_ao", "pwm_c_hiz_ao"
};

static const char * const pwm_d_ao_groups[] = {
	"pwm_d_ao"
};

static const char * const pwm_e_ao_groups[] = {
	"pwm_e_ao"
};

static const char * const pwm_f_ao_groups[] = {
	"pwm_f_ao"
};

static const char * const pwm_g_ao_groups[] = {
	"pwm_g_ao", "pwm_g_ao_ao", "pwm_g_hiz_ao"
};

static const char * const rt_gpio_ao_groups[] = {
	"rt_gpio_0_ao",  "rt_gpio_1_ao",  "rt_gpio_2_ao",  "rt_gpio_3_ao",
	"rt_gpio_4_ao",  "rt_gpio_5_ao",  "rt_gpio_6_ao",  "rt_gpio_7_ao",
	"rt_gpio_8_ao",  "rt_gpio_9_ao",  "rt_gpio_10_ao", "rt_gpio_11_ao",
	"rt_gpio_12_ao", "rt_gpio_13_ao", "rt_gpio_14_ao", "rt_gpio_15_ao",
	"rt_gpio_16_ao", "rt_gpio_17_ao", "rt_gpio_18_ao"
};

static const char * const sdcard_ao_groups[] = {
	"sdcard_d0_ao",  "sdcard_d1_ao",  "sdcard_d2_ao", "sdcard_d3_ao",
	"sdcard_clk_ao", "sdcard_cmd_ao"
};

static const char * const spdif_ao_groups[] = {
	"spdif_in_ao", "spdif_out_ao", "spdif_in_ao_d", "spdif_out_ao_d"
};

static const char * const spi_a_ao_groups[] = {
	"spi_a_ss0_ao",     "spi_a_ss1_ao",     "spi_a_ss2_ao",    "spi_a_miso_ao",
	"spi_a_mosi_ao",    "spi_a_sclk_ao",    "spi_a_ss0_ao_ao", "spi_a_miso_ao_ao",
	"spi_a_mosi_ao_ao", "spi_a_sclk_ao_ao"
};

static const char * const spi_b_ao_groups[] = {
	"spi_b_ss0_ao",  "spi_b_ss1_ao",  "spi_b_ss2_ao", "spi_b_miso_ao",
	"spi_b_mosi_ao", "spi_b_sclk_ao"
};

static const char * const spi_d_ao_groups[] = {
	"spi_d_ss0_ao",  "spi_d_ss1_ao",  "spi_d_ss2_ao", "spi_d_miso_ao",
	"spi_d_mosi_ao", "spi_d_sclk_ao"
};

static const char * const str_en_ao_groups[] = {
	"str_en_ao", "str_en_ao_ao"
};

static const char * const uart_b_ao_groups[] = {
	"uart_b_rx_ao", "uart_b_tx_ao"
};

static const char * const uart_c_ao_groups[] = {
	"uart_c_rx_ao", "uart_c_tx_ao", "uart_c_cts_ao", "uart_c_rts_ao"
};

static const char * const uart_d_ao_groups[] = {
	"uart_d_rx_ao", "uart_d_tx_ao", "uart_d_cts_ao", "uart_d_rts_ao"
};

static const char * const uart_e_ao_groups[] = {
	"uart_e_rx_ao", "uart_e_tx_ao", "uart_e_cts_ao", "uart_e_rts_ao"
};

static struct meson_pmx_func meson_a9_aobus_functions[] __initdata = {
	FUNCTION(gpio_aobus),
	FUNCTION(cec_ao),
	FUNCTION(clk_ao),
	FUNCTION(i2c_a_ao),
	FUNCTION(i2c_b_ao),
	FUNCTION(i2c_c_ao),
	FUNCTION(i2c_d_ao),
	FUNCTION(i3c_a_ao),
	FUNCTION(ir_in_ao),
	FUNCTION(ir_out_ao),
	FUNCTION(jtag_a_ao),
	FUNCTION(jtag_b_ao),
	FUNCTION(mic_mute_ao),
	FUNCTION(pcie_ao),
	FUNCTION(pdm_ao),
	FUNCTION(pmic_ao),
	FUNCTION(pwm_a_ao),
	FUNCTION(pwm_b_ao),
	FUNCTION(pwm_c_ao),
	FUNCTION(pwm_d_ao),
	FUNCTION(pwm_e_ao),
	FUNCTION(pwm_f_ao),
	FUNCTION(pwm_g_ao),
	FUNCTION(rt_gpio_ao),
	FUNCTION(sdcard_ao),
	FUNCTION(spdif_ao),
	FUNCTION(spi_a_ao),
	FUNCTION(spi_b_ao),
	FUNCTION(spi_d_ao),
	FUNCTION(str_en_ao),
	FUNCTION(uart_b_ao),
	FUNCTION(uart_c_ao),
	FUNCTION(uart_d_ao),
	FUNCTION(uart_e_ao)
};

static struct meson_bank meson_a9_aobus_banks[] = {
	/*    name   first   last   irq   pullen   pull   dir   out   in   ds */
	BANK_DS("D",    GPIOD_0,  GPIOD_16,  13,  29,
		0x013,  0, 0x014,  0, 0x012,  0, 0x011,  0, 0x010,  0, 0x017,  0),
	BANK_DS("AO",  GPIOAO_0, GPIOAO_12,   0,  12,
		0x003,  0, 0x004,  0, 0x002,  0, 0x001,  0, 0x000,  0, 0x007,  0),
	BANK_DS("C",    GPIOC_0,   GPIOC_6,  30,  36,
		0x00b,  0, 0x00c,  0, 0x00a,  0, 0x009,  0, 0x008,  0, 0x00f,  0)
};

static struct meson_pmx_bank meson_a9_aobus_pmx_banks[] = {
	/*       name      first        last         reg   offset */
	BANK_PMX("D",      GPIOD_0,    GPIOD_16,     0x002,  0),
	BANK_PMX("AO",    GPIOAO_0,   GPIOAO_12,     0x000,  0),
	BANK_PMX("C",      GPIOC_0,     GPIOC_6,     0x004,  4)
};

static struct meson_axg_pmx_data meson_a9_aobus_pmx_banks_data = {
	.pmx_banks	= meson_a9_aobus_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_a9_aobus_pmx_banks),
};

static struct meson_pinctrl_data meson_a9_aobus_pinctrl_data __refdata = {
	.name		= "aobus-banks",
	.pins		= meson_a9_aobus_pins,
	.groups		= meson_a9_aobus_groups,
	.funcs		= meson_a9_aobus_functions,
	.banks		= meson_a9_aobus_banks,
	.num_pins	= ARRAY_SIZE(meson_a9_aobus_pins),
	.num_groups	= ARRAY_SIZE(meson_a9_aobus_groups),
	.num_funcs	= ARRAY_SIZE(meson_a9_aobus_functions),
	.num_banks	= ARRAY_SIZE(meson_a9_aobus_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_a9_aobus_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

static const struct pinctrl_pin_desc meson_a9_test_pins[] = {
	/* TEST_N (AO domain) */
	MESON_PIN(GPIO_TEST_N)
};

static struct meson_pmx_group meson_a9_test_groups[] __initdata = {
	/* TEST_N (AO domain) */
	GPIO_GROUP(GPIO_TEST_N)
};

static const char * const gpio_test_groups[] = {
	"GPIO_TEST_N"
};

static struct meson_pmx_func meson_a9_test_functions[] __initdata = {
	FUNCTION(gpio_test),
};

static struct meson_bank meson_a9_test_banks[] = {
	/*    name   first   last   irq   pullen   pull   dir   out   in   ds */
	BANK_DS("TEST_N", GPIO_TEST_N,    GPIO_TEST_N,   37, 37,
		0x003,  0,  0x004,  0,  0x002, 0,  0x001,  0, 0x000, 0, 0x007, 0)
};

static struct meson_pmx_bank meson_a9_test_pmx_banks[] = {
	/*       name      first        last         reg   offset */
	BANK_PMX("TEST_N", GPIO_TEST_N, GPIO_TEST_N, 0x000,  0),
};

static struct meson_axg_pmx_data meson_a9_test_pmx_banks_data = {
	.pmx_banks	= meson_a9_test_pmx_banks,
	.num_pmx_banks	= ARRAY_SIZE(meson_a9_test_pmx_banks),
};

static struct meson_pinctrl_data meson_a9_test_pinctrl_data __refdata = {
	.name		= "test-banks",
	.pins		= meson_a9_test_pins,
	.groups		= meson_a9_test_groups,
	.funcs		= meson_a9_test_functions,
	.banks		= meson_a9_test_banks,
	.num_pins	= ARRAY_SIZE(meson_a9_test_pins),
	.num_groups	= ARRAY_SIZE(meson_a9_test_groups),
	.num_funcs	= ARRAY_SIZE(meson_a9_test_functions),
	.num_banks	= ARRAY_SIZE(meson_a9_test_banks),
	.pmx_ops	= &meson_axg_pmx_ops,
	.pmx_data	= &meson_a9_test_pmx_banks_data,
	.parse_dt	= &meson_a1_parse_dt_extra,
};

/************************************** END **************************************/

static const struct of_device_id meson_a9_pinctrl_dt_match[] = {
	{
		.compatible = "amlogic,meson-a9-periphs-pinctrl",
		.data = &meson_a9_periphs_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-a9-aobus-pinctrl",
		.data = &meson_a9_aobus_pinctrl_data,
	},
	{
		.compatible = "amlogic,meson-a9-test-pinctrl",
		.data = &meson_a9_test_pinctrl_data,
	},
	{ },
};

static struct platform_driver meson_a9_pinctrl_driver = {
	.probe  = meson_pinctrl_probe,
	.driver = {
		.name = "meson-a9-pinctrl",
	},
};

#ifndef MODULE
static int __init a9_pmx_init(void)
{
	meson_a9_pinctrl_driver.driver.of_match_table =
			meson_a9_pinctrl_dt_match;
	return platform_driver_register(&meson_a9_pinctrl_driver);
}
arch_initcall(a9_pmx_init);
#else
int __init meson_a9_pinctrl_init(void)
{
	meson_a9_pinctrl_driver.driver.of_match_table =
			meson_a9_pinctrl_dt_match;
	return platform_driver_register(&meson_a9_pinctrl_driver);
}
module_init(meson_a9_pinctrl_init);
#endif

MODULE_LICENSE("GPL v2");
