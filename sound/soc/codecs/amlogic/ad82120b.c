// SPDX-License-Identifier: GPL-2.0
/*
 * ad82120b.c  --  ad82120b ALSA SoC Audio driver
 *
 * Copyright 1998 Elite Semiconductor Memory Technology
 *
 * Author: ESMT Audio/Power Product BU Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/device.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/initval.h>
#include <linux/regmap.h>
#include <linux/version.h>

#include "ad82120b.h"

// after enable,you can use "tinymix" or "amixer" cmd to change eq mode.
// If you do not need to change eq mode, you can ignore it
//#define	AD82120B_CHANGE_EQ_MODE_EN

#ifdef AD82120B_CHANGE_EQ_MODE_EN
	// eq file, add Parameter file in "codec\ad82120b_eq", and then include the header file.
	#include "ad82120b_eq/ad82120b_eq_mode_1.h"
	#include "ad82120b_eq/ad82120b_eq_mode_2.h"

	// the value range of eq mode, modify them according to your request.
	#define AD82120B_EQ_MODE_MIN 1
	#define AD82120B_EQ_MODE_MAX 2
#endif

/* Define how often to check (and clear) the fault status register (in ms) */
#define AD82120B_FAULT_CHECK_INTERVAL		500

// you can read out the register and ram data, and check them.
//#define AD82120B_REG_RAM_CHECK
#define AD82120B_VOLUME_MAX  (230)
#define AD82120B_VOLUME_MIN  (0)
#define AD82120B_PARAM_COUNT 1220

enum ad82120b_type {
	AD82120B,
};

static char string[AD82120B_PARAM_COUNT];
static const char * const ad82120b_supply_names[] = {
	"dvdd",		/* Digital power supply. Connect to 3.3-V supply. */
	"pvdd",		/* Class-D amp and analog power supply (connected). */
};

#define AD82120B_NUM_SUPPLIES	ARRAY_SIZE(ad82120b_supply_names)

static int m_reg_tab[AD82120B_REGISTER_COUNT][2] = {
{0x00, 0x00}, //##State_Control_1
{0x01, 0x82}, //##State_Control_2
{0x02, 0x00}, //##State_Control_3
{0x03, 0x40}, //##Master_volume_control
{0x04, 0x18}, //##Channel_1_volume_control
{0x05, 0x18}, //##Channel_2_volume_control
{0x06, 0x18}, //##Channel_3_volume_control
{0x07, 0x18}, //##Channel_4_volume_control
{0x08, 0x18}, //##Channel_5_volume_control
{0x09, 0x18}, //##Channel_6_volume_control
{0x0A, 0x40}, //##DTC_Setting
{0x0B, 0x00}, //##Reserve
{0x0C, 0x90}, //##State_Control_4
{0x0D, 0x80}, //##Channel_1_configuration_registers
{0x0E, 0x80}, //##Channel_2_configuration_registers
{0x0F, 0x80}, //##Channel_3_configuration_registers
{0x10, 0x80}, //##Channel_4_configuration_registers
{0x11, 0x80}, //##Channel_5_configuration_registers
{0x12, 0x80}, //##Channel_6_configuration_registers
{0x13, 0x80}, //##Channel_7_configuration_registers
{0x14, 0x80}, //##Channel_8_configuration_registers
{0x15, 0x00}, //##Reserve
{0x16, 0x00}, //##Reserve
{0x17, 0x00}, //##Reserve
{0x18, 0x00}, //##Reserve
{0x19, 0x80}, //##Reserve
{0x1A, 0x28}, //##State_Control_5
{0x1B, 0x80}, //##State_Control_6
{0x1C, 0x20}, //##State_Control_7
{0x1D, 0x00}, //##Coefficient_RAM_Base_Address
{0x1E, 0x00}, //##First_4-bits_of_coefficients_A1
{0x1F, 0x00}, //##Second_8-bits_of_coefficients_A1
{0x20, 0x00}, //##Third_8-bits_of_coefficients_A1
{0x21, 0x00}, //##Fourth-bits_of_coefficients_A1
{0x22, 0x00}, //##First_4-bits_of_coefficients_A2
{0x23, 0x00}, //##Second_8-bits_of_coefficients_A2
{0x24, 0x00}, //##Third_8-bits_of_coefficients_A2
{0x25, 0x00}, //##Fourth_8-bits_of_coefficients_A2
{0x26, 0x00}, //##First_4-bits_of_coefficients_B1
{0x27, 0x00}, //##Second_8-bits_of_coefficients_B1
{0x28, 0x00}, //##Third_8-bits_of_coefficients_B1
{0x29, 0x00}, //##Fourth_8-bits_of_coefficients_B1
{0x2A, 0x00}, //##First_4-bits_of_coefficients_B2
{0x2B, 0x00}, //##Second_8-bits_of_coefficients_B2
{0x2C, 0x00}, //##Third_8-bits_of_coefficients_B2
{0x2D, 0x00}, //##Fourth-bits_of_coefficients_B2
{0x2E, 0x00}, //##First_4-bits_of_coefficients_A0
{0x2F, 0x80}, //##Second_8-bits_of_coefficients_A0
{0x30, 0x00}, //##Third_8-bits_of_coefficients_A0
{0x31, 0x00}, //##Fourth_8-bits_of_coefficients_A0
{0x32, 0x00}, //##Coefficient_RAM_R/W_control
{0x33, 0x02}, //##State_Control_8
{0x34, 0x00}, //##Reserve
{0x35, 0x00}, //##Volume_Fine_tune
{0x36, 0x00}, //##Volume_Fine_tune
{0x37, 0xA0}, //##Device_ID_register
{0x38, 0x00}, //##Level_Meter_Clear
{0x39, 0x00}, //##Power_Meter_Clear
{0x3A, 0x00}, //##First_8bits_of_C1_Level_Meter
{0x3B, 0x00}, //##Second_8bits_of_C1_Level_Meter
{0x3C, 0x00}, //##Third_8bits_of_C1_Level_Meter
{0x3D, 0x00}, //##Fourth_8bits_of_C1_Level_Meter
{0x3E, 0x00}, //##First_8bits_of_C2_Level_Meter
{0x3F, 0x00}, //##Second_8bits_of_C2_Level_Meter
{0x40, 0x00}, //##Third_8bits_of_C2_Level_Meter
{0x41, 0x00}, //##Fourth_8bits_of_C2_Level_Meter
{0x42, 0x00}, //##CHK_stat
{0x43, 0x00}, //##First_4_bits_of_BEQ_CHK_val
{0x44, 0x00}, //##Second_8_bits_of_BEQ_CHK_val
{0x45, 0x00}, //##Third_4_bits_of_BEQ_CHK_val
{0x46, 0x00}, //##Bottom_4_bits_of_BEQ_CHK_val
{0x47, 0x00}, //##First_4_bits_of_BEQ_CHK_result
{0x48, 0x00}, //##Second_4_bits_of_BEQ_CHK_result
{0x49, 0x00}, //##Third_4_bits_of_BEQ_CHK_result
{0x4A, 0x00}, //##Bottom_4_bits_of_BEQ_CHK_result
{0x4B, 0x10}, //##digital_DC_control
{0x4C, 0x20}, //##digital_DC_judge_threshold
{0x4D, 0xFF}, //##ERROR_register
{0x4E, 0xFF}, //##ERROR_latch_register
{0x4F, 0x00}, //##ERROR_clear_register
{0x50, 0x01}, //##ERROR_register_2
{0x51, 0x01}, //##ERROR_latch_register_2
{0x52, 0x00}, //##ERROR_clear_register_2
{0x53, 0x00}, //##Reserve
{0x54, 0x00}, //##Reserve
{0x55, 0x20}, //##clock_FS_detection
{0x56, 0x10}, //##CLK_DET
{0x57, 0x00}, //##TDM_word_width_selection
{0x58, 0x00}, //##TDM_offset
{0x59, 0x06}, //##I2S_Data_output_selection_register
{0x5A, 0x08}, //##PWM_frequency_selection
{0x5B, 0x00}, //##Mono_Key_High_Byte
{0x5C, 0x00}, //##Mono_Key_Low_Byte
{0x5D, 0x07}, //##Hi-res_Item
{0x5E, 0x00}, //##analog_gain
{0x5F, 0x00}, //##AGC_Control
{0x60, 0x00}, //##Channel1_EQ_bypass_1
{0x61, 0x00}, //##Channel1_EQ_bypass_2
{0x62, 0x00}, //##Channel1_EQ_bypass_3
{0x63, 0x00}, //##Channel2_EQ_bypass_1
{0x64, 0x00}, //##Channel2_EQ_bypass_2
{0x65, 0x00}, //##Channel2_EQ_bypass_3
{0x66, 0x00}, //##ATTACK_HOLD_SET
{0x67, 0x00}, //##Reserve
{0x68, 0x00}, //##Reserve
{0x69, 0x00}, //##Reserve
{0x6A, 0x00}, //##Reserve
{0x6B, 0x00}, //##Reserve
{0x6C, 0x02}, //##FS_and_PMF_read_out
{0x6D, 0x03}, //##OC_Selection
{0x6E, 0x00}, //##digital_DC_control_testmode
{0x6F, 0x32}, //##test_mode_register0
{0x70, 0x07}, //##noise_gate_analog_gain
{0x71, 0x41}, //##Testmode_register1
{0x72, 0x38}, //##Testmode_register2
{0x73, 0x18}, //##dither_signal_setting
{0x74, 0x0D}, //##Error_Delay
{0x75, 0x05}, //##First_4bits_of_MBIST_User_Program_Even
{0x76, 0x55}, //##Second_8bits_of_MBIST_User_Program_Even
{0x77, 0x55}, //##Third_8bits_of_MBIST_User_Program_Even
{0x78, 0x55}, //##Fourth_8bits_of_MBIST_User_Program_Even
{0x79, 0x55}, //##Fifth_8bits_of_MBIST_User_Program_Even
{0x7A, 0x05}, //##First_8bits_of_MBIST_User_Program_Odd
{0x7B, 0x55}, //##Second_8bits_of_MBIST_User_Program_Odd
{0x7C, 0x55}, //##Third_8bits_of_MBIST_User_Program_Odd
{0x7D, 0x55}, //##Fourth_8bits_of_MBIST_User_Program_Odd
{0x7E, 0x55}, //##Fifth_8bits_of_MBIST_User_Program_Odd
{0x7F, 0x50}, //##IP_shutdown_register
{0x80, 0x00}, //##Protection_Register_Set
{0x81, 0x00}, //##MBIST_Status
{0x82, 0x00}, //##PWM_output_control
{0x83, 0x00}, //##Test_Mode_Control
{0x84, 0x00}, //##RAM1_test_register_address
{0x85, 0x00}, //##First_4bits_of_RAM1_data
{0x86, 0x00}, //##Second_8bits_of_RAM1_Data
{0x87, 0x00}, //##Third_8bits_of_RAM1_data
{0x88, 0x00}, //##Fourth_8bits_of_RAM1_Data
{0x89, 0x00}, //##Fifth_8bits_of_RAM1_Data
{0x8A, 0x00}, //##RAM1_test_r/w_control
{0x8B, 0x00}, //##RAM2_test_register_address
{0x8C, 0x00}, //##First_4bits_of_RAM2_data
{0x8D, 0x00}, //##Second_8bits_of_RAM2_Data
{0x8E, 0x00}, //##Third_8bits_of_RAM2_data
{0x8F, 0x00}, //##Fourth_8bits_of_RAM2_Data
{0x90, 0x00}, //##Fifth_8bits_of_RAM2_Data
{0x91, 0x00}, //##RAM2_test_r/w_control
{0x92, 0x00}, //##BURN_EN1
{0x93, 0x00}, //##BURN_EN2
{0x94, 0x00}, //##OSC_TRIM_CTRL
{0x95, 0x80}, //##PLL_CTRL
{0x96, 0x11}, //##PLL_CTRL2
{0x97, 0x06}, //##CLKDET_read_out1
{0x98, 0x11}, //##CLKDET_read_out2
{0x99, 0x02}, //##CLKDET_read_out3
{0x9A, 0x56}, //##CLKDET_read_out4
{0x9B, 0x90}, //##CLKDET_read_out5
{0x9C, 0x3F}, //##CLKDET_read_out6
{0x9D, 0x00}, //##CLKDET_read_out7
{0x9E, 0x02}, //##CLKDET_read_out8
{0x9F, 0x02}, //##RECOUNT_CTRL
{0xA0, 0xE4}, //##RECOUNT_STABLE_CTRL
{0xA1, 0x04}, //##Auto_clock_detect_FS96k_window
{0xA2, 0x04}, //##Auto_clock_detect_FS48k_window
{0xA3, 0x04}, //##Auto_clock_detect_FS16k_window
{0xA4, 0x04}, //##Auto_clock_detect_FS8k_window
{0xA5, 0x80}, //##SYNC_LR
};

static int m_ram1_tab[][5] = {
{0x00, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ1
{0x01, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ1
{0x02, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ1
{0x03, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ1
{0x04, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ1
{0x05, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ2
{0x06, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ2
{0x07, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ2
{0x08, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ2
{0x09, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ2
{0x0A, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ3
{0x0B, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ3
{0x0C, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ3
{0x0D, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ3
{0x0E, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ3
{0x0F, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ4
{0x10, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ4
{0x11, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ4
{0x12, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ4
{0x13, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ4
{0x14, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ5
{0x15, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ5
{0x16, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ5
{0x17, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ5
{0x18, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ5
{0x19, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ6
{0x1A, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ6
{0x1B, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ6
{0x1C, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ6
{0x1D, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ6
{0x1E, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ7
{0x1F, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ7
{0x20, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ7
{0x21, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ7
{0x22, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ7
{0x23, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ8
{0x24, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ8
{0x25, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ8
{0x26, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ8
{0x27, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ8
{0x28, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ9
{0x29, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ9
{0x2A, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ9
{0x2B, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ9
{0x2C, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ9
{0x2D, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ10
{0x2E, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ10
{0x2F, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ10
{0x30, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ10
{0x31, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ10
{0x32, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ11
{0x33, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ11
{0x34, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ11
{0x35, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ11
{0x36, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ11
{0x37, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ12
{0x38, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ12
{0x39, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ12
{0x3A, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ12
{0x3B, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ12
{0x3C, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ13
{0x3D, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ13
{0x3E, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ13
{0x3F, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ13
{0x40, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ13
{0x41, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ14
{0x42, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ14
{0x43, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ14
{0x44, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ14
{0x45, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ14
{0x46, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ15
{0x47, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ15
{0x48, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ15
{0x49, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ15
{0x4A, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ15
{0x4B, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ16
{0x4C, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ16
{0x4D, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ16
{0x4E, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ16
{0x4F, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ16
{0x50, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ17
{0x51, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ17
{0x52, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ17
{0x53, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ17
{0x54, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ17
{0x55, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ18
{0x56, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ18
{0x57, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ18
{0x58, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ18
{0x59, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ18
{0x5A, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ19
{0x5B, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ19
{0x5C, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ19
{0x5D, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ19
{0x5E, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ19
{0x5F, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ20
{0x60, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ20
{0x61, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ20
{0x62, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ20
{0x63, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ20
{0x64, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ21
{0x65, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ21
{0x66, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ21
{0x67, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ21
{0x68, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ21
{0x69, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ22
{0x6A, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ22
{0x6B, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ22
{0x6C, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ22
{0x6D, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ22
{0x6E, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A1_EQ23
{0x6F, 0x00, 0x00, 0x00, 0x00},//##Channel_1_A2_EQ23
{0x70, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B1_EQ23
{0x71, 0x00, 0x00, 0x00, 0x00},//##Channel_1_B2_EQ23
{0x72, 0x02, 0x00, 0x00, 0x00},//##Channel_1_A0_EQ23
{0x73, 0x07, 0xFF, 0xFF, 0xFF},//##Channel_1_Mixer1
{0x74, 0x00, 0x00, 0x00, 0x00},//##Channel_1_Mixer2
{0x75, 0x00, 0x7E, 0x88, 0xE0},//##Channel_1_Prescale
{0x76, 0x02, 0x00, 0x00, 0x00},//##Channel_1_Postscale
{0x77, 0x02, 0x00, 0x00, 0x00},//##CH1.2_Power_Clipping
{0x78, 0x00, 0x00, 0x01, 0xA0},//####Noise_Gate_Attack_Level
{0x79, 0x00, 0x00, 0x05, 0x30},//##Noise_Gate_Release_Level
{0x7A, 0x00, 0x01, 0x00, 0x00},//##DRC1_Energy_Coefficient
{0x7B, 0x00, 0x01, 0x00, 0x00},//##DRC2_Energy_Coefficient
{0x7C, 0x00, 0x01, 0x00, 0x00},//##DRC3_Energy_Coefficient
{0x7D, 0x00, 0x01, 0x00, 0x00},//##DRC4_Energy_Coefficient
{0x7E, 0x00, 0x00, 0x00, 0x00},//##DRC1_Power_Meter
{0x7F, 0x00, 0x00, 0x00, 0x00},//##DRC3_Power_Meter
{0x80, 0x00, 0x00, 0x00, 0x00},//##DRC5_Power_Meter
{0x81, 0x00, 0x00, 0x00, 0x00},//##DRC7_Power_Meter
{0x82, 0x02, 0x00, 0x00, 0x00},//##Channel_1_DRC_GAIN1
{0x83, 0x02, 0x00, 0x00, 0x00},//##Channel_1_DRC_GAIN2
{0x84, 0x02, 0x00, 0x00, 0x00},//##Channel_1_DRC_GAIN3
{0x85, 0x0E, 0x01, 0xC0, 0x70},//##DRC1_FF_threshold
{0x86, 0x02, 0x00, 0x00, 0x00},//##DRC1_FF_slope
{0x87, 0x00, 0x00, 0x40, 0x00},//##DRC1_FF_aa
{0x88, 0x00, 0x00, 0x40, 0x00},//##DRC1_FF_da
{0x89, 0x0E, 0x01, 0xC0, 0x70},//##DRC2_FF_threshold
{0x8A, 0x02, 0x00, 0x00, 0x00},//##DRC2_FF_slope
{0x8B, 0x00, 0x00, 0x40, 0x00},//##DRC2_FF_aa
{0x8C, 0x00, 0x00, 0x40, 0x00},//##DRC2_FF_da
{0x8D, 0x0E, 0x01, 0xC0, 0x70},//##DRC3_FF_threshold
{0x8E, 0x02, 0x00, 0x00, 0x00},//##DRC3_FF_slope
{0x8F, 0x00, 0x00, 0x40, 0x00},//##DRC3_FF_aa
{0x90, 0x00, 0x00, 0x40, 0x00},//##DRC3_FF_da
{0x91, 0x0E, 0x01, 0xC0, 0x70},//##DRC4_FF_threshold
{0x92, 0x02, 0x00, 0x00, 0x00},//##DRC4_FF_slope
{0x93, 0x00, 0x00, 0x40, 0x00},//##DRC4_FF_aa
{0x94, 0x00, 0x00, 0x40, 0x00},//##DRC4_FF_da
{0x95, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x96, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x97, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x98, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x99, 0x00, 0x80, 0x00, 0x00},//##I2SO_LCH_GAIN
{0x9A, 0x02, 0x00, 0x00, 0x00},//##SRS_Gain
{0x9B, 0x01, 0xD7, 0xE6, 0xE0},//##Compensate_A0
{0x9C, 0x00, 0x2E, 0x77, 0x40},//##Compensate_A1
{0x9D, 0x0F, 0xF9, 0xA1, 0xE0},//##Compensate_B1
{0x9E, 0x01, 0x91, 0x2B, 0xE0},//##QT_AT
{0x9F, 0x01, 0x1B, 0x5A, 0xB9},//##QT_R
{0xA0, 0x00, 0x5F, 0xCA, 0xCA},//##QT_Region_T
{0xA1, 0x00, 0x00, 0x00, 0x00},//##QT_Region_B
};

static int m_ram2_tab[][5] = {
{0x00, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ1
{0x01, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ1
{0x02, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ1
{0x03, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ1
{0x04, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ1
{0x05, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ2
{0x06, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ2
{0x07, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ2
{0x08, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ2
{0x09, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ2
{0x0A, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ3
{0x0B, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ3
{0x0C, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ3
{0x0D, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ3
{0x0E, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ3
{0x0F, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ4
{0x10, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ4
{0x11, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ4
{0x12, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ4
{0x13, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ4
{0x14, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ5
{0x15, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ5
{0x16, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ5
{0x17, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ5
{0x18, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ5
{0x19, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ6
{0x1A, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ6
{0x1B, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ6
{0x1C, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ6
{0x1D, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ6
{0x1E, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ7
{0x1F, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ7
{0x20, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ7
{0x21, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ7
{0x22, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ7
{0x23, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ8
{0x24, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ8
{0x25, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ8
{0x26, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ8
{0x27, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ8
{0x28, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ9
{0x29, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ9
{0x2A, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ9
{0x2B, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ9
{0x2C, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ9
{0x2D, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ10
{0x2E, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ10
{0x2F, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ10
{0x30, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ10
{0x31, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ10
{0x32, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ11
{0x33, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ11
{0x34, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ11
{0x35, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ11
{0x36, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ11
{0x37, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ12
{0x38, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ12
{0x39, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ12
{0x3A, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ12
{0x3B, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ12
{0x3C, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ13
{0x3D, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ13
{0x3E, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ13
{0x3F, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ13
{0x40, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ13
{0x41, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ14
{0x42, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ14
{0x43, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ14
{0x44, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ14
{0x45, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ14
{0x46, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ15
{0x47, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ15
{0x48, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ15
{0x49, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ15
{0x4A, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ15
{0x4B, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ16
{0x4C, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ16
{0x4D, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ16
{0x4E, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ16
{0x4F, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ16
{0x50, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ17
{0x51, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ17
{0x52, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ17
{0x53, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ17
{0x54, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ17
{0x55, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ18
{0x56, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ18
{0x57, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ18
{0x58, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ18
{0x59, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ18
{0x5A, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ19
{0x5B, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ19
{0x5C, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ19
{0x5D, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ19
{0x5E, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ19
{0x5F, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ20
{0x60, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ20
{0x61, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ20
{0x62, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ20
{0x63, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ20
{0x64, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ21
{0x65, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ21
{0x66, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ21
{0x67, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ21
{0x68, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ21
{0x69, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ22
{0x6A, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ22
{0x6B, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ22
{0x6C, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ22
{0x6D, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ22
{0x6E, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A1_EQ23
{0x6F, 0x00, 0x00, 0x00, 0x00},//##Channel_2_A2_EQ23
{0x70, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B1_EQ23
{0x71, 0x00, 0x00, 0x00, 0x00},//##Channel_2_B2_EQ23
{0x72, 0x02, 0x00, 0x00, 0x00},//##Channel_2_A0_EQ23
{0x73, 0x00, 0x00, 0x00, 0x00},//##Channel_2_Mixer1
{0x74, 0x07, 0xFF, 0xFF, 0xFF},//##Channel_2_Mixer2
{0x75, 0x00, 0x80, 0x00, 0x00},//##Channel_2_Prescale
{0x76, 0x02, 0x00, 0x00, 0x00},//##Channel_2_Postscale
{0x77, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x78, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x79, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x7A, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x7B, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x7C, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x7D, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x7E, 0x00, 0x00, 0x00, 0x00},//##DRC2_Power_Meter
{0x7F, 0x00, 0x00, 0x00, 0x00},//##DRC4_Power_Meter
{0x80, 0x00, 0x00, 0x00, 0x00},//##DRC6_Power_Meter
{0x81, 0x00, 0x00, 0x00, 0x00},//##DRC8_Power_Meter
{0x82, 0x02, 0x00, 0x00, 0x00},//##Channel_2_DRC_GAIN1
{0x83, 0x02, 0x00, 0x00, 0x00},//##Channel_2_DRC_GAIN2
{0x84, 0x02, 0x00, 0x00, 0x00},//##Channel_2_DRC_GAIN3
{0x85, 0x00, 0x02, 0x00, 0x00},//##DPEQ_Energy_Coefficient
{0x86, 0x0D, 0x2D, 0x26, 0x00},//##DPEQ_UP_TH
{0x87, 0x0B, 0x83, 0xF1, 0x10},//##DPEQ_Low_TH
{0x88, 0x00, 0x26, 0x87, 0xF0},//##DBE_1_div_(Upper-Lower)
{0x89, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x8A, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x8B, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x8C, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x8D, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x8E, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x8F, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x90, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x91, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x92, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x93, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x94, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x95, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x96, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x97, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x98, 0x00, 0x00, 0x00, 0x00},//##Reserve
{0x99, 0x00, 0x80, 0x00, 0x00},//##I2SO_RCH_GAI
{0x9A, 0x00, 0x5A, 0x9D, 0xF0},//##AGC_Attach_threshold
{0x9B, 0x00, 0x47, 0xFA, 0xD0},//##AGC_Release_threshold
{0x9C, 0x00, 0x00, 0x08, 0x64},//##AGC_AR_RR
{0x9D, 0x00, 0x00, 0x08, 0x64},//##AGC_AT_RT
{0x9E, 0x00, 0x10, 0x00, 0x00},//##AGC ALPHA
};

struct ad82120b_data {
	int pd_gpio;
	int CHIP_SYNC;
	int CHIP_SYNC_lock;
	int ASR_DET;
	int ASR_DET_lock;
	struct snd_soc_component *component;
	struct regmap *regmap;
	struct i2c_client *ad82120b_client;
	enum ad82120b_type devtype;
	struct regulator_bulk_data supplies[AD82120B_NUM_SUPPLIES];
	struct delayed_work fault_check_work;
	unsigned int last_fault;
#ifdef AD82120B_CHANGE_EQ_MODE_EN
	unsigned int eq_mode;
	unsigned char (*m_ram_tab)[5];
#endif
	int vol;
	int ch_mode;
	int mute;
	char *m_reg_tab;
	char *m_ram1_tab;
	char *m_ram2_tab;
	bool eq_enable;
	bool drc_enable;
	bool reg_setting;
	struct device *dev;
	struct gpio_desc *reset_pin_desc;
	int no_device;
};

static int ad82120b_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	unsigned int rate = params_rate(params);
	unsigned int ssz_ds;
	int ret;

	pr_info("%s rate in amp is (%d)\n", __func__, rate);
	switch (rate) {
	case 44100:
	case 48000:
		ssz_ds = 0;
		break;
	case 88200:
	case 96000:
		ssz_ds = AD82120B_SSZ_DS;
		break;
	default:
		dev_err(component->dev, "unsupported sample rate: %u\n", rate);
		return -EINVAL;
	}

	pr_info("%s setting ad82120b hw params.\n", __func__);
	ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL2_REG,
				  AD82120B_SSZ_DS, ssz_ds);

	if (ret < 0) {
		dev_err(component->dev, "error setting sample rate: %d\n", ret);
		return ret;
	}

	return 0;
}

static int ad82120b_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u8 serial_format;
	int ret;

	if ((fmt & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		dev_vdbg(component->dev, "DAI Format master is not found\n");
		return -EINVAL;
	}

	switch (fmt & (SND_SOC_DAIFMT_FORMAT_MASK |
		       SND_SOC_DAIFMT_INV_MASK)) {
	case (SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF):
		/* 1st data bit occur one BCLK cycle after the frame sync */
		serial_format = AD82120B_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_A | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Note that although the AD82120B does not have a dedicated DSP
		 * mode it doesn't care about the LRCLK duty cycle during TDM
		 * operation. Therefore we can use the device's I2S mode with
		 * its delaying of the 1st data bit to receive DSP_A formatted
		 * data. See device datasheet for additional details.
		 */
		serial_format = AD82120B_SAIF_I2S;
		break;
	case (SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF):
		/*
		 * Similar to DSP_A, we can use the fact that the AD82120B does
		 * not care about the LRCLK duty cycle during TDM to receive
		 * DSP_B formatted data in LEFTJ mode (no delaying of the 1st
		 * data bit).
		 */
		serial_format = AD82120B_SAIF_LEFTJ;
		break;
	case (SND_SOC_DAIFMT_LEFT_J | SND_SOC_DAIFMT_NB_NF):
		/* No delay after the frame sync */
		serial_format = AD82120B_SAIF_LEFTJ;
		break;
	default:
		dev_vdbg(component->dev, "DAI Format is not found\n");
		return -EINVAL;
	}

	pr_info("%s setting ad82120b dai format.\n", __func__);
	ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL1_REG,
				  AD82120B_SAIF_FORMAT_MASK,
				  serial_format);
	if (ret < 0) {
		dev_err(component->dev, "error setting SAIF format: %d\n", ret);
		return ret;
	}

	return 0;
}

#ifdef AD82120B_CHANGE_EQ_MODE_EN
static int ad82120b_change_eq_mode(struct snd_soc_component *component, int channel)
{
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	int eq_seg = 0;
	int i = 0;
	int cmd = 0;

	for (i = 0; i < 20; i++) {
		// ram addr
		regmap_write(ad82120b->regmap, 0x1d, ad82120b->m_ram_tab[eq_seg][0]);

		// write A1
		regmap_write(ad82120b->regmap, 0x1e, ad82120b->m_ram_tab[eq_seg][1]);
		regmap_write(ad82120b->regmap, 0x1f, ad82120b->m_ram_tab[eq_seg][2]);
		regmap_write(ad82120b->regmap, 0x20, ad82120b->m_ram_tab[eq_seg][3]);
		regmap_write(ad82120b->regmap, 0x21, ad82120b->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write A2
		regmap_write(ad82120b->regmap, 0x22, ad82120b->m_ram_tab[eq_seg][1]);
		regmap_write(ad82120b->regmap, 0x23, ad82120b->m_ram_tab[eq_seg][2]);
		regmap_write(ad82120b->regmap, 0x24, ad82120b->m_ram_tab[eq_seg][3]);
		regmap_write(ad82120b->regmap, 0x25, ad82120b->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write B1
		regmap_write(ad82120b->regmap, 0x26, ad82120b->m_ram_tab[eq_seg][1]);
		regmap_write(ad82120b->regmap, 0x27, ad82120b->m_ram_tab[eq_seg][2]);
		regmap_write(ad82120b->regmap, 0x28, ad82120b->m_ram_tab[eq_seg][3]);
		regmap_write(ad82120b->regmap, 0x29, ad82120b->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write B2
		regmap_write(ad82120b->regmap, 0x2a, ad82120b->m_ram_tab[eq_seg][1]);
		regmap_write(ad82120b->regmap, 0x2b, ad82120b->m_ram_tab[eq_seg][2]);
		regmap_write(ad82120b->regmap, 0x2c, ad82120b->m_ram_tab[eq_seg][3]);
		regmap_write(ad82120b->regmap, 0x2d, ad82120b->m_ram_tab[eq_seg][4]);

		eq_seg += 1;
		// write A0
		regmap_write(ad82120b->regmap, 0x2e, ad82120b->m_ram_tab[eq_seg][1]);
		regmap_write(ad82120b->regmap, 0x2f, ad82120b->m_ram_tab[eq_seg][2]);
		regmap_write(ad82120b->regmap, 0x30, ad82120b->m_ram_tab[eq_seg][3]);
		regmap_write(ad82120b->regmap, 0x31, ad82120b->m_ram_tab[eq_seg][4]);

		if (channel == 1)
			cmd = 0x02;
		else if (channel == 2)
			cmd = 0x42;

		regmap_write(ad82120b->regmap, 0x32, cmd);
		eq_seg += 1;

		if (eq_seg > 0x72)
			break;
	}

	for (eq_seg = 0x73; eq_seg < 0x9E; eq_seg++) {
		if (eq_seg >= 0x7E && eq_seg <= 0x81)
			continue;

		regmap_write(ad82120b->regmap, CFADDR, ad82120b->m_ram_tab[eq_seg][0]);
		regmap_write(ad82120b->regmap, A1CF1, ad82120b->m_ram_tab[eq_seg][1]);
		regmap_write(ad82120b->regmap, A1CF2, ad82120b->m_ram_tab[eq_seg][2]);
		regmap_write(ad82120b->regmap, A1CF3, ad82120b->m_ram_tab[eq_seg][3]);
		regmap_write(ad82120b->regmap, A1CF4, ad82120b->m_ram_tab[eq_seg][4]);

		if (channel == 1)
			cmd = 0x01;
		else if (channel == 2)
			cmd = 0x41;
		else
			cmd = 0;

		regmap_write(ad82120b->regmap, CFUD, cmd);
	}

	return 0;
}

static int ad82120b_eq_mode_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type   = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access = (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count  = 1;

	uinfo->value.integer.min  = AD82120B_EQ_MODE_MIN;
	uinfo->value.integer.max  = AD82120B_EQ_MODE_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int ad82120b_eq_mode_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = ad82120b->eq_mode;

	return 0;
}

static int ad82120b_eq_mode_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	int id_reg = 0xff;

	if (ucontrol->value.integer.value[0] > AD82120B_EQ_MODE_MAX ||
		ucontrol->value.integer.value[0] < AD82120B_EQ_MODE_MIN) {
		dev_err(component->dev, "error mode value setting, please check!\n");
		return -1;
	}

	regmap_read(ad82120b->regmap, AD82120B_DEVICE_ID_REG, &id_reg);
	if ((id_reg & 0xf0) != AD82120B_DEVICE_ID) {	// amp i2c have not ack ,i2c error
		dev_err(component->dev, "error device id 0x%02x, please check!\n", id_reg);
		return -1;
	}

	ad82120b->eq_mode = ucontrol->value.integer.value[0];

	pr_info("%s change ad82120b eq mode = %d\n", __func__, ad82120b->eq_mode);

	if (ad82120b->eq_mode == 1) {
		ad82120b->m_ram_tab = eq_mode_1_ram1_tab;
		ad82120b_change_eq_mode(component, 1);
		#ifdef CONFIG_SND_SOC_AD82120B_2CHANNEL
			ad82120b->m_ram_tab = eq_mode_1_ram2_tab;
			ad82120b_change_eq_mode(component, 2);
		#endif
	}
	if (ad82120b->eq_mode == 2) {
		ad82120b->m_ram_tab = eq_mode_2_ram1_tab;
		ad82120b_change_eq_mode(component, 1);
		#ifdef CONFIG_SND_SOC_AD82120B_2CHANNEL
			ad82120b->m_ram_tab = eq_mode_2_ram2_tab;
			ad82120b_change_eq_mode(component, 2);
		#endif
	}

	// add your other eq mode here
	// ...

	return 0;
}

static const struct snd_kcontrol_new ad82120b_eq_mode_control[] = {
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
		.name  = "AD82120B EQ Mode",  // Just fake the name
		.info  = ad82120b_eq_mode_info,
		.get   = ad82120b_eq_mode_get,
		.put   = ad82120b_eq_mode_put,
	},
};
#endif

#define AD82120B_ERROR_FLAG (AD82120B_OCE | AD82120B_DCDE | AD82120B_OTE |\
			 AD82120B_UVE | AD82120B_BSUVE | AD82120B_OVPE)

static int ad82120b_power_on(struct device *dev, bool enable)
{
	struct ad82120b_data *ad82120b =  dev_get_drvdata(dev);

	if (IS_ERR(ad82120b->reset_pin_desc)) {
		dev_err(dev, "failed to request reset pin gpio\n");
		return -1;
	}

	// pull high amp PD pin
	if (enable)
		gpiod_direction_output(ad82120b->reset_pin_desc, GPIOF_OUT_INIT_HIGH);
	else
		gpiod_direction_output(ad82120b->reset_pin_desc, GPIOF_OUT_INIT_LOW);
	dev_vdbg(dev, "ad82120b pd pin status = %d\n",
		gpiod_get_value(ad82120b->reset_pin_desc));
	return 0;
}

static void ad82120b_fault_check_work(struct work_struct *work)
{
	struct ad82120b_data *ad82120b = container_of(work, struct ad82120b_data,
			fault_check_work.work);
	struct device *dev = ad82120b->component->dev;
	unsigned int curr_fault;
	int ret;

	// if CHIP SYNC is enable at initialization, (enable-> delay 200us -> disable)
	// it again Whenever i2s clk on, both BCLK and LRCK are required to be
	// simultaneously and stably supplied to the AMP.
	if (ad82120b->CHIP_SYNC == 1 && ad82120b->CHIP_SYNC_lock == 0) {
		/* Set CHIP SYNC to enable */
		ret = snd_soc_component_update_bits(ad82120b->component, AD82120B_STATE_CTRL4_REG,
				  AD82120B_CHIP_SYNC_EN, AD82120B_CHIP_SYNC_EN);

		usleep_range(1 * 1000, 2 * 1000);

		/* Set CHIP SYNC to disable */
		ret = snd_soc_component_update_bits(ad82120b->component, AD82120B_STATE_CTRL4_REG,
				  AD82120B_CHIP_SYNC_EN, 0);

		ad82120b->CHIP_SYNC_lock = 1;
	}

	// Force Auto sample rate detection function to turn off during initialization,
	// and now turn it on(I2S clock is out).
	if (ad82120b->ASR_DET != 0 && ad82120b->ASR_DET_lock == 0) {
		ret = regmap_write(ad82120b->regmap, AD82120B_CLK_DET_CTRL, ad82120b->ASR_DET);
		if (ret < 0)
			dev_err(dev, "failed to write AD82120B_CLK_DET_CTRL register: %d\n", ret);
		ad82120b->ASR_DET_lock = 1;
	}

	// amp fault check
	ret = regmap_read(ad82120b->regmap, AD82120B_FAULT_REG, &curr_fault);
	if (ret < 0) {
		dev_err(dev, "failed to read FAULT register: %d\n", ret);
		goto out;
	}

	/* Check/handle all errors except SAIF clock errors */
	curr_fault &= AD82120B_ERROR_FLAG;
	/*
	 * Only flag errors once for a given occurrence. This is needed as
	 * the AD82120B will take time clearing the fault condition internally
	 * during which we don't want to bombard the system with the same
	 * error message over and over.
	 */
	if (!(curr_fault & AD82120B_OCE) && (ad82120b->last_fault & AD82120B_OCE))
		dev_crit(dev, "experienced an over current hardware fault\n");

	if (!(curr_fault & AD82120B_DCDE) && (ad82120b->last_fault & AD82120B_DCDE))
		dev_crit(dev, "experienced a DCD detection fault\n");

	if (!(curr_fault & AD82120B_OTE) && (ad82120b->last_fault & AD82120B_OTE))
		dev_crit(dev, "experienced an over temperature fault\n");

	if (!(curr_fault & AD82120B_UVE) && (ad82120b->last_fault & AD82120B_UVE))
		dev_crit(dev, "experienced an UV fault\n");

	if (!(curr_fault & AD82120B_BSUVE) && (ad82120b->last_fault & AD82120B_BSUVE))
		dev_crit(dev, "experienced a BSUV fault\n");

	if (!(curr_fault & AD82120B_OVPE) && (ad82120b->last_fault & AD82120B_OVPE))
		dev_crit(dev, "experienced an OVP fault\n");
	/* Store current fault value so we can detect any changes next time */
	ad82120b->last_fault = curr_fault;

	if (curr_fault == AD82120B_ERROR_FLAG)
		goto out;

	/*
	 * Periodically toggle SDZ (shutdown bit) H->L->H to clear any latching
	 * faults as long as a fault condition persists. Always going through
	 * the full sequence no matter the first return value to minimizes
	 * chances for the device to end up in shutdown mode.
	 */
	dev_crit(dev, "toggle pd pin H->L->H to clear latching faults\n");

	ad82120b_power_on(dev, false);		// pull low amp PD pin
	msleep(20);
	ad82120b_power_on(dev, true);		// pull high amp PD pin

out:
	/* Schedule the next fault check at the specified interval */
	schedule_delayed_work(&ad82120b->fault_check_work,
			      msecs_to_jiffies(AD82120B_FAULT_CHECK_INTERVAL));
}

static int ad82120b_reg_ram_init(struct snd_soc_component *component)
{
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	struct device *dev = ad82120b->dev;
	int ret;
	int i;
	int reg_data;

	pr_info("ad82120b i2c address = %p,  %s!\n", component, __func__);

	ad82120b_power_on(dev, true);;			// pull high amp PD pin

	msleep(45);

	// software reset amp
	ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL5_REG,
			  AD82120B_SW_RESET, 0);
	if (ret < 0)
		goto error_snd_soc_component_update_reg;

	usleep_range(5 * 1000, 6 * 1000);

	ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL5_REG,
			  AD82120B_SW_RESET, AD82120B_SW_RESET);
	if (ret < 0)
		goto error_snd_soc_component_update_reg;

	msleep(20);			// wait 20ms

	/* Set device to mute */
	ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL3_REG,
				  AD82120B_MUTE, AD82120B_MUTE);
	if (ret < 0)
		goto error_snd_soc_component_update_reg;

	// Write register table
	for (i = 0; i < AD82120B_REGISTER_COUNT; i++) {
		reg_data = m_reg_tab[i][1];

		if (m_reg_tab[i][0] == 0x02)
			continue;

		if (ad82120b->ch_mode) {
			// set mono
			if (m_reg_tab[i][0] == 0x1A) {
				reg_data = 0x68;
				reg_data |= 0x40;			// mono_en = 1
			}
			if (m_reg_tab[i][0] == 0x5B)
				reg_data = 0x30;
			if (m_reg_tab[i][0] == 0x5C)		// mono_key = 0x3006
				reg_data = 0x06;
			if (m_reg_tab[i][0] == 0x98)		// mono_key = 0x3006
				reg_data = 0x12;
			if (m_reg_tab[i][0] == 0x9A)		// mono_key = 0x3006
				reg_data = 0x64;
			// set mono end
		} else {
			// set stereo
			if (m_reg_tab[i][0] == 0x1A)
				reg_data &= (~0x40);		// mono_en = 0
			if (m_reg_tab[i][0] == 0x5B)
				reg_data = 0x00;
			if (m_reg_tab[i][0] == 0x5C)		// mono_key = 0x0000
				reg_data = 0x00;
			// set stereo end
		}
		// check if CHIP SYNC enable ?
		if (m_reg_tab[i][0] == AD82120B_STATE_CTRL4_REG) {
			if ((reg_data & AD82120B_CHIP_SYNC_EN) == AD82120B_CHIP_SYNC_EN) {
				ad82120b->CHIP_SYNC = 1;
				pr_info("%s ad82120b chip sync enable!\n", __func__);
			} else {
				ad82120b->CHIP_SYNC = 0;
			}
		}

		// check if Auto sample rate detection enable ?
		ad82120b->ASR_DET_lock = 0;
		if (m_reg_tab[i][0] == AD82120B_CLK_DET_CTRL) {
			if ((reg_data & AD82120B_ASR_DET) == AD82120B_ASR_DET) {
				ad82120b->ASR_DET = reg_data;
				pr_info("%s ad82120b Auto sample rate detection enable!\n",
						__func__);

				// Force this feature to turn off during initialization,
				// and turn it on after the I2S clock is out.
				reg_data = 0x10;
			} else {
				ad82120b->ASR_DET = 0;
			}
		}

		ret = regmap_write(ad82120b->regmap, m_reg_tab[i][0], reg_data);
		if (ret < 0)
			goto error_snd_soc_component_update_reg;
	}

	// Write ram1
	for (i = 0; i < AD82120B_RAM1_TABLE_COUNT; i++) {
		if (ad82120b->ch_mode) {
			// set mono
			if (m_ram1_tab[i][0] == 0x73) {
				m_ram1_tab[i][1] = 0x04;
				m_ram1_tab[i][2] = 0x00;
				m_ram1_tab[i][3] = 0x00;
				m_ram1_tab[i][4] = 0x00;
			}
			if (m_ram1_tab[i][0] == 0x74) {
				m_ram1_tab[i][1] = 0x04;
				m_ram1_tab[i][2] = 0x00;
				m_ram1_tab[i][3] = 0x00;
				m_ram1_tab[i][4] = 0x00;
			}
			// set mono end
		}
		regmap_write(ad82120b->regmap, CFADDR, m_ram1_tab[i][0]);
		regmap_write(ad82120b->regmap, A1CF1, m_ram1_tab[i][1]);
		regmap_write(ad82120b->regmap, A1CF2, m_ram1_tab[i][2]);
		regmap_write(ad82120b->regmap, A1CF3, m_ram1_tab[i][3]);
		regmap_write(ad82120b->regmap, A1CF4, m_ram1_tab[i][4]);
		regmap_write(ad82120b->regmap, CFUD, 0x01);
	}
	// Write ram2
	for (i = 0; i < AD82120B_RAM2_TABLE_COUNT; i++) {
		if (ad82120b->ch_mode) {
			// set mono
			if (m_ram2_tab[i][0] == 0x73) {
				m_ram2_tab[i][1] = 0x04;
				m_ram2_tab[i][2] = 0x00;
				m_ram2_tab[i][3] = 0x00;
				m_ram2_tab[i][4] = 0x00;
			}
			if (m_ram2_tab[i][0] == 0x74) {
				m_ram2_tab[i][1] = 0x04;
				m_ram2_tab[i][2] = 0x00;
				m_ram2_tab[i][3] = 0x00;
				m_ram2_tab[i][4] = 0x00;
			}
			// set mono end
		}
		regmap_write(ad82120b->regmap, CFADDR, m_ram2_tab[i][0]);
		regmap_write(ad82120b->regmap, A1CF1, m_ram2_tab[i][1]);
		regmap_write(ad82120b->regmap, A1CF2, m_ram2_tab[i][2]);
		regmap_write(ad82120b->regmap, A1CF3, m_ram2_tab[i][3]);
		regmap_write(ad82120b->regmap, A1CF4, m_ram2_tab[i][4]);
		regmap_write(ad82120b->regmap, CFUD, 0x41);
	}
	ad82120b->no_device = 0;
	return 0;

error_snd_soc_component_update_reg:
	dev_err(component->dev, "error init device registers: %d\n", ret);

	return ret;
}

static int ad82120b_codec_probe(struct snd_soc_component *component)
{
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	//unsigned int device_id, expected_device_id;
	int ret;
#ifdef AD82120B_REG_RAM_CHECK
	int i;
	int reg_data;
	int ram_h8_data;
	int ram_m8_data;
	int ram_l8_data;
	int ram_ll8_data;
#endif
	ad82120b->component = component;
	ad82120b->mute = 0;

	pr_info("ad82120b i2c address = %p,  %s!\n", component, __func__);
	ret = regulator_bulk_enable(ARRAY_SIZE(ad82120b->supplies),
				    ad82120b->supplies);
	if (ret != 0) {
		dev_err(component->dev, "failed to enable supplies: %d\n", ret);
		return ret;
	}

	usleep_range(10 * 1000, 11 * 1000);

	ret = ad82120b_reg_ram_init(component);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	usleep_range(2 * 1000, 3 * 1000);

	// if CHIP SYNC is enable, need disable it before unmute
	if (ad82120b->CHIP_SYNC == 1) {
		/* Set CHIP SYNC to disable */
		ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL4_REG,
				  AD82120B_CHIP_SYNC_EN, 0);

		ad82120b->CHIP_SYNC_lock = 0;
	}

	/* Set device to unmute */
	ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL3_REG,
				  AD82120B_MUTE, 0);
	if (ret < 0)
		goto error_snd_soc_component_update_bits;

	INIT_DELAYED_WORK(&ad82120b->fault_check_work, ad82120b_fault_check_work);

#ifdef AD82120B_CHANGE_EQ_MODE_EN
	ret = snd_soc_add_component_controls(component, ad82120b_eq_mode_control, 1);
	if (ret != 0)
		dev_err(component->dev, "Failed to register ad82120b_eq_mode_control (%d)\n", ret);
#endif

#ifdef AD82120B_REG_RAM_CHECK

	msleep(1000);

	for (i = 0; i < AD82120B_REGISTER_COUNT; i++) {
		regmap_read(ad82120b->regmap, m_reg_tab[i][0], &reg_data);
		//printk("read ad82120b register {addr, data} = {0x%02x, 0x%02x}\n",
		//m_reg_tab[i][0], reg_data);
	}

	for (i = 0; i < AD82120B_RAM1_TABLE_COUNT; i++) {
		regmap_write(ad82120b->regmap, CFADDR, m_ram1_tab[i][0]);	// write ram addr
		regmap_write(ad82120b->regmap, CFUD, 0x04);	 // write read a single ram data cmd

		regmap_read(ad82120b->regmap, A1CF1, &ram_h8_data);
		regmap_read(ad82120b->regmap, A1CF2, &ram_m8_data);
		regmap_read(ad82120b->regmap, A1CF3, &ram_l8_data);
		regmap_read(ad82120b->regmap, A1CF4, &ram_ll8_data);
	}
#endif

	return 0;

error_snd_soc_component_update_bits:
	dev_err(component->dev, "error configuring device registers: %d\n", ret);

//probe_fail:
	//regulator_bulk_disable(ARRAY_SIZE(ad82120b->supplies),
			       //ad82120b->supplies);
	return ret;
}

static void ad82120b_codec_remove(struct snd_soc_component *component)
{
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	int ret;

	cancel_delayed_work_sync(&ad82120b->fault_check_work);
	ad82120b->CHIP_SYNC_lock = 0;
	ad82120b->ASR_DET_lock = 0;

	ret = regulator_bulk_disable(ARRAY_SIZE(ad82120b->supplies),
				     ad82120b->supplies);
	if (ret < 0)
		dev_err(component->dev, "failed to disable supplies: %d\n", ret);
};

static int ad82120b_dac_event(struct snd_soc_dapm_widget *w,
			     struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_component *component = snd_soc_dapm_to_component(w->dapm);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	int ret;

	if (ad82120b->no_device) {
		dev_err(component->dev, "no device connect for ad82120b\n");
		return 0;
	}

	if (event & SND_SOC_DAPM_POST_PMU) {
		pr_info("%s ad82120b dac event post PMU.\n", __func__);
		/*
		 * Observe codec shutdown-to-active time. The datasheet only
		 * lists a nominal value however just use-it as-is without
		 * additional padding to minimize the delay introduced in
		 * starting to play audio (actually there is other setup done
		 * by the ASoC framework that will provide additional delays,
		 * so we should always be safe).
		 */
		if (!ad82120b->mute) {
			ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL3_REG,
						  AD82120B_MUTE, 0);
			if (ret < 0)
				dev_err(component->dev, "failed to write MUTE register: %d\n", ret);
		}
		/* Turn on AD82120B periodic fault checking/handling */
		ad82120b->last_fault = 0xFF;
		schedule_delayed_work(&ad82120b->fault_check_work,
				msecs_to_jiffies(AD82120B_FAULT_CHECK_INTERVAL));
	} else if (event & SND_SOC_DAPM_PRE_PMD) {
		pr_info("%s ad82120b dac event pre PMD.\n", __func__);
		/* Disable AD82120B periodic fault checking/handling */
		cancel_delayed_work_sync(&ad82120b->fault_check_work);
		// clear amp sync lock
		ad82120b->CHIP_SYNC_lock = 0;
		// disable ASR_DET function and clear lock before I2S clk stop
		if (ad82120b->ASR_DET != 0)
			regmap_write(ad82120b->regmap, AD82120B_CLK_DET_CTRL, 0x10);
		ad82120b->ASR_DET_lock = 0;

		/* Place AD82120B in shutdown mode to minimize current draw */
		ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL3_REG,
					  AD82120B_MUTE, AD82120B_MUTE);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);
	}

	return 0;
}

static int ad82120b_mute(struct snd_soc_component *component, int mute)
{
	int ret;

	if (mute) {
		//mute master volume
		ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL3_REG,
			AD82120B_MUTE, AD82120B_MUTE);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);
	} else {
		//unmute
		ret = snd_soc_component_update_bits(component, AD82120B_STATE_CTRL3_REG,
			AD82120B_MUTE, 0);
		if (ret < 0)
			dev_err(component->dev, "failed to write MUTE register: %d\n", ret);
	}

	return 0;
}

static inline int get_volume_index(int vol)
{
	int index;

	index = vol;

	if (index < AD82120B_VOLUME_MIN)
		index = AD82120B_VOLUME_MIN;

	if (index > AD82120B_VOLUME_MAX)
		index = AD82120B_VOLUME_MAX;

	return index;
}

static void ad82120b_set_volume(struct snd_soc_component *component, int vol)
{
	unsigned int index;
	u32 volume_hex;
	u8 byte;

	index = get_volume_index(vol);
	volume_hex = ad82120b_volume[index];

	byte = (volume_hex & 0xFF);

	snd_soc_component_write(component, AD82120B_VOLUME_CTRL_REG, byte);
}

#ifdef CONFIG_PM
static int ad82120b_suspend(struct snd_soc_component *component)
{
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	struct device *dev = ad82120b->dev;
	int ret;

	pr_info("%s ad82120b suspend.\n", __func__);

	//regcache_cache_only(ad82120b->regmap, true);
	//regcache_mark_dirty(ad82120b->regmap);

	ret = regulator_bulk_disable(ARRAY_SIZE(ad82120b->supplies),
				     ad82120b->supplies);
	if (ret < 0)
		dev_err(component->dev, "failed to disable supplies: %d\n", ret);

	ad82120b_power_on(dev, false);

	return ret;
}

static int ad82120b_resume(struct snd_soc_component *component)
{
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	struct device *dev = ad82120b->dev;
	int ret, i, j;
	char *p = ad82120b->m_reg_tab;

	pr_info("%s ad82120b resume.\n", __func__);

	ad82120b_power_on(dev, true);
	ret = regulator_bulk_enable(ARRAY_SIZE(ad82120b->supplies),
				    ad82120b->supplies);
	if (ret < 0) {
		dev_err(component->dev, "failed to enable supplies: %d\n", ret);
		return ret;
	}

	//regcache_cache_only(ad82120b->regmap, false);

	//ret = regcache_sync(ad82120b->regmap);
	//if (ret < 0) {
	//	dev_err(component->dev, "failed to sync regcache: %d\n", ret);
	//	return ret;
	//}

	msleep(20);

	if (ad82120b->eq_enable) {
		memset(string, 0, AD82120B_PARAM_COUNT);
		memcpy(string, ad82120b->m_ram1_tab, AD82120B_RAM1_TABLE_COUNT * 5);

		for (i = 0; i < AD82120B_RAM1_TABLE_COUNT; i++) {
			for (j = 0; j < 5; j++)
				m_ram1_tab[i][j] = string[i * 5 + j];
		}
	}

	if (ad82120b->drc_enable) {
		memset(string, 0, AD82120B_PARAM_COUNT);
		memcpy(string, ad82120b->m_ram2_tab, AD82120B_RAM2_TABLE_COUNT * 5);

		for (i = 0; i < AD82120B_RAM2_TABLE_COUNT; i++) {
			for (j = 0; j < 5; j++)
				m_ram2_tab[i][j] = string[i * 5 + j];
		}
	}
	ret = ad82120b_reg_ram_init(component);
	if (ret < 0)
		goto resume_error_snd_soc_component_update_bits;
	ad82120b_set_volume(component, ad82120b->vol);
	if (ad82120b->reg_setting) {
		for (i = 0; i < AD82120B_REGISTER_COUNT; i++) {
			snd_soc_component_write(component, *p, *(p + 1));
			p += 2;
		}
	}
	ad82120b_mute(component, ad82120b->mute);

	return 0;

resume_error_snd_soc_component_update_bits:
	dev_err(component->dev, "resume error configuring device registers: %d\n", ret);

	return ret;
}
#else
#define ad82120b_suspend NULL
#define ad82120b_resume NULL
#endif

static bool ad82120b_is_volatile_reg(struct device *dev, unsigned int reg)
{
#ifdef	AD82120B_REG_RAM_CHECK
	if (reg <= AD82120B_MAX_REG)
		return true;
	else
		return false;
#else
	switch (reg) {
	case AD82120B_FAULT_REG:
	case AD82120B_STATE_CTRL1_REG:
	case AD82120B_STATE_CTRL2_REG:
	case AD82120B_STATE_CTRL3_REG:
	case AD82120B_STATE_CTRL5_REG:
	case AD82120B_DEVICE_ID_REG:
		return true;
	default:
		return true;
	}
#endif
}

static const struct regmap_config ad82120b_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = AD82120B_MAX_REG,

	//.reg_defaults = m_reg_tab,

	.cache_type = REGCACHE_RBTREE,
	.volatile_reg = ad82120b_is_volatile_reg,
};

static int  ad82120b_trigger(struct snd_pcm_substream *substream, int cmd,
			       struct snd_soc_dai *codec_dai)
{
	struct ad82120b_data *ad82120b = snd_soc_dai_get_drvdata(codec_dai);
	struct snd_soc_component *component = ad82120b->component;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s(), start\n", __func__);
			if (!ad82120b->mute)
				ad82120b_mute(component, 0);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_debug("%s(), stop\n", __func__);
			if (!ad82120b->mute)
				ad82120b_mute(component, 1);
			break;
		}
	}
	return 0;
}

static int ad82120b_mute_info(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;
	uinfo->value.integer.step = 1;

	return 0;
}

static int ad82120b_mute_locked_put(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ad82120b->mute = ucontrol->value.integer.value[0];
	ad82120b_mute(component, ad82120b->mute);

	return 0;
}

static int ad82120b_mute_locked_get(struct snd_kcontrol *kcontrol,
				    struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = ad82120b->mute;

	return 0;
}

static int ad82120b_vol_info(struct snd_kcontrol *kcontrol,
			     struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->access =
	    (SNDRV_CTL_ELEM_ACCESS_TLV_READ | SNDRV_CTL_ELEM_ACCESS_READWRITE);
	uinfo->count = 1;

	uinfo->value.integer.min = AD82120B_VOLUME_MIN;
	uinfo->value.integer.max = AD82120B_VOLUME_MAX;
	uinfo->value.integer.step = 1;

	return 0;
}

static int ad82120b_vol_locked_get(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = ad82120b->vol;

	return 0;
}

static int ad82120b_vol_locked_put(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ad82120b->vol = ucontrol->value.integer.value[0];
	ad82120b_set_volume(component, ad82120b->vol);

	return 0;
}

static int ad82120b_set_ram1_param(struct snd_kcontrol *kcontrol,
				  const unsigned int __user *bytes,
				  unsigned int size)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	char tmp_string[AD82120B_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	unsigned int i, j, res;
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);

	res = copy_from_user(p_string, val, AD82120B_PARAM_COUNT);
	if (res)
		return -EFAULT;

	memcpy(ad82120b->m_ram1_tab, p_string, AD82120B_RAM1_TABLE_COUNT * 5);

	for (i = 0; i < AD82120B_RAM1_TABLE_COUNT; i++) {
		for (j = 0; j < 5; j++)
			m_ram1_tab[i][j] = tmp_string[i * 5 + j];
	}

	for (i = 0; i < AD82120B_RAM1_TABLE_COUNT; i++) {
		regmap_write(ad82120b->regmap, CFADDR, m_ram1_tab[i][0]);
		regmap_write(ad82120b->regmap, A1CF1, m_ram1_tab[i][1]);
		regmap_write(ad82120b->regmap, A1CF2, m_ram1_tab[i][2]);
		regmap_write(ad82120b->regmap, A1CF3, m_ram1_tab[i][3]);
		regmap_write(ad82120b->regmap, A1CF4, m_ram1_tab[i][4]);
		regmap_write(ad82120b->regmap, CFUD, 0x01);
	}

	return 0;
}

static int ad82120b_get_ram1_param(struct snd_kcontrol *kcontrol,
			    unsigned int __user *bytes,
			    unsigned int size)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);
	char tmp_string[AD82120B_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	int i, j, res = 0;

	for (i = 0; i < AD82120B_RAM1_TABLE_COUNT; i++) {
		regmap_write(ad82120b->regmap, CFADDR, m_ram1_tab[i][0]);
		regmap_write(ad82120b->regmap, CFUD, 0x04);

		regmap_read(ad82120b->regmap, A1CF1, &m_ram1_tab[i][1]);
		regmap_read(ad82120b->regmap, A1CF2, &m_ram1_tab[i][2]);
		regmap_read(ad82120b->regmap, A1CF3, &m_ram1_tab[i][3]);
		regmap_read(ad82120b->regmap, A1CF4, &m_ram1_tab[i][4]);
	}

	for (i = 0; i < AD82120B_RAM1_TABLE_COUNT; i++) {
		for (j = 0; j < 5; j++)
			tmp_string[i * 5 + j] = m_ram1_tab[i][j];
	}

	res = copy_to_user(val, p_string, AD82120B_RAM1_TABLE_COUNT * 5);
	if (res)
		return -EFAULT;

	return 0;
}

static int ad82120b_set_ram2_param(struct snd_kcontrol *kcontrol,
				  const unsigned int __user *bytes,
				  unsigned int size)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	char tmp_string[AD82120B_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	unsigned int i, j, res;
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);

	res = copy_from_user(p_string, val, AD82120B_PARAM_COUNT);
	if (res)
		return -EFAULT;

	memcpy(ad82120b->m_ram2_tab, p_string, AD82120B_RAM2_TABLE_COUNT * 5);

	for (i = 0; i < AD82120B_RAM2_TABLE_COUNT; i++) {
		for (j = 0; j < 5; j++)
			m_ram2_tab[i][j] = tmp_string[i * 5 + j];
	}

	for (i = 0; i < AD82120B_RAM2_TABLE_COUNT; i++) {
		regmap_write(ad82120b->regmap, CFADDR, m_ram2_tab[i][0]);
		regmap_write(ad82120b->regmap, A1CF1, m_ram2_tab[i][1]);
		regmap_write(ad82120b->regmap, A1CF2, m_ram2_tab[i][2]);
		regmap_write(ad82120b->regmap, A1CF3, m_ram2_tab[i][3]);
		regmap_write(ad82120b->regmap, A1CF4, m_ram2_tab[i][4]);
		regmap_write(ad82120b->regmap, CFUD, 0x41);
	}

	return 0;
}

static int ad82120b_get_ram2_param(struct snd_kcontrol *kcontrol,
			    unsigned int __user *bytes,
			    unsigned int size)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);
	char tmp_string[AD82120B_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	int i, j, res = 0;

	for (i = 0; i < AD82120B_RAM2_TABLE_COUNT; i++) {
		regmap_write(ad82120b->regmap, CFADDR, m_ram2_tab[i][0]);
		regmap_write(ad82120b->regmap, CFUD, 0x44);

		regmap_read(ad82120b->regmap, A1CF1, &m_ram2_tab[i][1]);
		regmap_read(ad82120b->regmap, A1CF2, &m_ram2_tab[i][2]);
		regmap_read(ad82120b->regmap, A1CF3, &m_ram2_tab[i][3]);
		regmap_read(ad82120b->regmap, A1CF4, &m_ram2_tab[i][4]);
	}

	for (i = 0; i < AD82120B_RAM2_TABLE_COUNT; i++) {
		for (j = 0; j < 5; j++)
			tmp_string[i * 5 + j] = m_ram2_tab[i][j];
	}

	res = copy_to_user(val, p_string, AD82120B_RAM2_TABLE_COUNT * 5);
	if (res)
		return -EFAULT;

	return 0;
}

static int ad82120b_set_register_value(struct snd_kcontrol *kcontrol,
				  const unsigned int __user *bytes,
				  unsigned int size)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	char tmp_string[AD82120B_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	char *p = ad82120b->m_reg_tab;
	unsigned int i = 0, res;
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);

	res = copy_from_user(p_string, val, AD82120B_PARAM_COUNT);
	if (res)
		return -EFAULT;

	memcpy(p, p_string, AD82120B_REGISTER_COUNT * 2);

	for (i = 0; i < AD82120B_REGISTER_COUNT; i++) {
		snd_soc_component_write(component, *p, *(p + 1));
		p += 2;
	}

	return 0;
}

static int ad82120b_get_register_value(struct snd_kcontrol *kcontrol,
				unsigned int __user *bytes,
				unsigned int size)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	struct snd_ctl_tlv *tlv;
	char *val = (char *)bytes + sizeof(*tlv);
	char tmp_string[AD82120B_PARAM_COUNT];
	char *p_string = &tmp_string[0];
	char *p = ad82120b->m_reg_tab;
	int i, res = 0;
	unsigned int curr_fault;

	for (i = 0; i < AD82120B_REGISTER_COUNT; i++) {
		regmap_read(ad82120b->regmap, m_reg_tab[i][0], &curr_fault);
		*p = m_reg_tab[i][0];
		*(p + 1) = curr_fault;
		p += 2;
	}

	p = ad82120b->m_reg_tab;
	for (i = 0; i < AD82120B_REGISTER_COUNT * 2; i++) {
		tmp_string[i] = *p;
		p++;
	}
	res = copy_to_user(val, p_string, AD82120B_REGISTER_COUNT * 2);
	if (res)
		return -EFAULT;

	return 0;
}

static int ad82120b_set_reg_setting_enable(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ad82120b->reg_setting = ucontrol->value.integer.value[0];

	return 0;
}

static int ad82120b_get_reg_setting_enable(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	bool enable = (bool)ad82120b->reg_setting & 0x1;

	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static int ad82120b_set_EQ_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ad82120b->eq_enable = ucontrol->value.integer.value[0];

	return 0;
}

static int ad82120b_get_EQ_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	bool enable = (bool)ad82120b->eq_enable & 0x1;

	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static int ad82120b_set_DRC_enum(struct snd_kcontrol *kcontrol,
				   struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);

	ad82120b->drc_enable = ucontrol->value.integer.value[0];

	return 0;
}

static int ad82120b_get_DRC_enum(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct ad82120b_data *ad82120b = snd_soc_component_get_drvdata(component);
	bool enable = (bool)ad82120b->drc_enable & 0x1;

	ucontrol->value.integer.value[0] = enable;

	return 0;
}

static const struct snd_kcontrol_new ad82120b_snd_controls[] = {
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Master Volume",
	 .info = ad82120b_vol_info,
	 .get = ad82120b_vol_locked_get,
	 .put = ad82120b_vol_locked_put,
	 },
	{
	 .iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	 .name = "Master Mute",
	 .info = ad82120b_mute_info,
	 .get = ad82120b_mute_locked_get,
	 .put = ad82120b_mute_locked_put,
	},
	SOC_SINGLE_BOOL_EXT("Register setting enable", 0,
		   ad82120b_get_reg_setting_enable, ad82120b_set_reg_setting_enable),
	SOC_SINGLE_BOOL_EXT("Set EQ Enable", 0,
			   ad82120b_get_EQ_enum, ad82120b_set_EQ_enum),
	SOC_SINGLE_BOOL_EXT("Set DRC Enable", 0,
			   ad82120b_get_DRC_enum, ad82120b_set_DRC_enum),
	SND_SOC_BYTES_TLV("Register value", AD82120B_PARAM_COUNT,
			   ad82120b_get_register_value, ad82120b_set_register_value),
	SND_SOC_BYTES_TLV("EQ table", AD82120B_PARAM_COUNT,
			   ad82120b_get_ram1_param, ad82120b_set_ram1_param),
	SND_SOC_BYTES_TLV("DRC table", AD82120B_PARAM_COUNT,
		   ad82120b_get_ram2_param, ad82120b_set_ram2_param),
};

static const struct snd_soc_dapm_widget ad82120b_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("DAC IN", "Playback", 0, SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_DAC_E("DAC", NULL, SND_SOC_NOPM, 0, 0, ad82120b_dac_event,
			   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_OUTPUT("OUT")
};

static const struct snd_soc_dapm_route ad82120b_audio_map[] = {
	{ "DAC", NULL, "DAC IN" },
	{ "OUT", NULL, "DAC" },
};

static const struct snd_soc_component_driver soc_component_dev_ad82120b = {
	.probe			= ad82120b_codec_probe,
	.remove			= ad82120b_codec_remove,
	.suspend		= ad82120b_suspend,
	.resume			= ad82120b_resume,
	.controls		= ad82120b_snd_controls,
	.num_controls		= ARRAY_SIZE(ad82120b_snd_controls),
	.dapm_widgets		= ad82120b_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(ad82120b_dapm_widgets),
	.dapm_routes		= ad82120b_audio_map,
	.num_dapm_routes	= ARRAY_SIZE(ad82120b_audio_map),
	.idle_bias_on		= 1,
	.use_pmdown_time	= 1,
	.endianness		= 1,
};

/* PCM rates supported by the AD82120B driver */
#define AD82120B_RATES	(SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 |\
			 SNDRV_PCM_RATE_88200 | SNDRV_PCM_RATE_96000)

/* Formats supported by AD82120B driver */
#define AD82120B_FORMATS (SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_LE |\
			 SNDRV_PCM_FMTBIT_S24_LE | SNDRV_PCM_FMTBIT_S32_LE)

static const struct snd_soc_dai_ops ad82120b_speaker_dai_ops = {
	.hw_params	= ad82120b_hw_params,
	.set_fmt	= ad82120b_set_dai_fmt,
	.trigger = ad82120b_trigger,
};

/*
 * AD82120B DAI structure
 *
 * Note that were are advertising .playback.channels_max = 2 despite this being
 * a mono amplifier. The reason for that is that some serial ports such as ESMT's
 * McASP module have a minimum number of channels (2) that they can output.
 * Advertising more channels than we have will allow us to interface with such
 * a serial port without really any negative side effects as the AD82120B will
 * simply ignore any extra channel(s) asides from the one channel that is
 * configured to be played back.
 */
static struct snd_soc_dai_driver ad82120b_dai[] = {
	{
		.name = "ad82120b",
		.playback = {
			.stream_name = "Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = AD82120B_RATES,
			.formats = AD82120B_FORMATS,
		},
		.ops = &ad82120b_speaker_dai_ops,
	},
};

static int ad82120b_parse_dt(struct ad82120b_data *ad82120b, struct device *dev)
{
	int ch_mode = 0;
	struct device_node *np = dev->of_node;

	ad82120b->no_device = 0;

	of_property_read_u32(np, "channel_mode", &ch_mode);
	ad82120b->ch_mode = ch_mode;
	dev_info(dev, "ch mode %d\n", ch_mode);

	ad82120b->reset_pin_desc = gpiod_get(ad82120b->dev,
					"reset_pin", GPIOF_OUT_INIT_LOW);

	if (IS_ERR(ad82120b->reset_pin_desc))
		dev_err(dev, "ad82120b reset_pin err\n");

	return 0;
}

static const struct i2c_device_id ad82120b_id[] = {
	{ "ad82120b", AD82120B },
	{ }
};
MODULE_DEVICE_TABLE(i2c, ad82120b_id);

#define MAX_TOTAL_PROBE 10
static int ad82120b_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct ad82120b_data *data;
	const struct regmap_config *regmap_config;
	int ret;
	int i;
	u32 dummy;
	const struct i2c_device_id *id = i2c_match_id(ad82120b_id, client);

	data = devm_kzalloc(dev, sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->ad82120b_client = client;
	data->devtype = id->driver_data;

	switch (id->driver_data) {
	case AD82120B:
		regmap_config = &ad82120b_regmap_config;
		break;
	default:
		dev_err(dev, "unexpected private driver data\n");
		return -EINVAL;
	}
	data->regmap = devm_regmap_init_i2c(client, regmap_config);
	if (IS_ERR(data->regmap)) {
		ret = PTR_ERR(data->regmap);
		dev_err(dev, "failed to allocate register map: %d\n", ret);
		return ret;
	}

	for (i = 0; i < ARRAY_SIZE(data->supplies); i++)
		data->supplies[i].supply = ad82120b_supply_names[i];

	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(data->supplies),
				      data->supplies);
	if (ret != 0) {
		dev_err(dev, "failed to request supplies: %d\n", ret);
		return ret;
	}

	dev_set_drvdata(dev, data);
	data->dev  = dev;
	ad82120b_parse_dt(data, &client->dev);
	ad82120b_power_on(dev, true);

	for (i = 0; i < MAX_TOTAL_PROBE; i++) {
		ret = regmap_read(data->regmap, 0x00, &dummy);
		if (ret) {
			if (i >= (MAX_TOTAL_PROBE - 1)) {
				dev_err(dev, "ad82120b pd pin status = %d, ret = %d\n",
						gpiod_get_value(data->reset_pin_desc), ret);
				return -EPROBE_DEFER;
			}
			usleep_range(5000, 6000);
		} else {
			pr_info("ad82120b device read okay\n");
			break;
		}
	}

	ret = devm_snd_soc_register_component(&client->dev,
				     &soc_component_dev_ad82120b,
				     ad82120b_dai, ARRAY_SIZE(ad82120b_dai));
	if (ret < 0) {
		dev_err(dev, "failed to register component: %d\n", ret);
		return ret;
	}

	data->m_reg_tab =
		devm_kzalloc(&client->dev,
			     sizeof(char) * AD82120B_PARAM_COUNT,
			     GFP_KERNEL);
	if (!data->m_reg_tab)
		return -ENOMEM;

	data->m_ram1_tab =
			devm_kzalloc(&client->dev,
					 sizeof(char) * AD82120B_PARAM_COUNT,
					 GFP_KERNEL);
	if (!data->m_ram1_tab)
		return -ENOMEM;

	data->m_ram2_tab =
			devm_kzalloc(&client->dev,
					 sizeof(char) * AD82120B_PARAM_COUNT,
					 GFP_KERNEL);
	if (!data->m_ram2_tab)
		return -ENOMEM;

	return 0;
}

static void ad82120b_shutdown(struct i2c_client *client)
{
	struct ad82120b_data *ad82120b = i2c_get_clientdata(client);
	int ret;

	if (!ad82120b)
		return;

	if (!IS_ERR(ad82120b->reset_pin_desc)) {
		ret = ad82120b_power_on(&client->dev, false);
		pr_debug("%s, ret:%d ad82120b pd pin status = %d\n",
			__func__, ret, gpiod_get_value(ad82120b->reset_pin_desc));
		ad82120b->no_device = 1;
		msleep(20);
		gpiod_put(ad82120b->reset_pin_desc);
		ad82120b->reset_pin_desc = NULL;
	}
	return;

}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id ad82120b_of_match[] = {
	{ .compatible = "ESMT,ad82120b", },
	{ },
};
MODULE_DEVICE_TABLE(of, ad82120b_of_match);
#endif

static struct i2c_driver ad82120b_i2c_driver = {
	.driver = {
		.name = "ad82120b",
		.of_match_table = of_match_ptr(ad82120b_of_match),
	},
	.shutdown = ad82120b_shutdown,
	.probe = ad82120b_probe,
	.id_table = ad82120b_id,
};

module_i2c_driver(ad82120b_i2c_driver);

MODULE_AUTHOR("ESMT BU2");
MODULE_DESCRIPTION("AD82120B Audio amplifier driver");
MODULE_LICENSE("GPL");
