/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _UCS11681_H_
#define _UCS11681_H_

// ALS Coefficient
#define COEF                             1.0

// 7-bits slave address
#define UCS11681_SLAVE_ADDR              0x38
// System registers
#define UCS11681_SYSM_CTRL               0x00
#define UCS11681_INT_FLAG                0x02
#define UCS11681_WAIT_TIME               0x03
// ALS registers
#define UCS11681_ALS_GAIN                 0x04
#define UCS11681_ALS_TIME                 0x05
#define UCS11681_ALS_DATA_L               0x1E
#define UCS11681_ALS_DATA_H               0x1F

#define UCS11681_PROD_ID_L                0xBC
#define UCS11681_PROD_ID_H                0xBD
#define UCS11681_PROD_ID_VAL              0x1011

// To reduce probe time consume
//#define ADD_UC11681_INPUT_DEVICE
#define CONFIG_REDUCE_TIME_CONSUME

#endif
