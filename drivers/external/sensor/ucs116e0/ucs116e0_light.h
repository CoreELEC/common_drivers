/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _UCS116E0_H_
#define _UCS116E0_H_

//=============================================================================
// 7-bits slave address
//=============================================================================
#define UCS116E0_SLAVE_ADDR              0x38

//=============================================================================
// System registers
//=============================================================================
#define UCS116E0_SYSM_CTRL               0x00  /* bit0=EN_ALS, bit1=EN_ONE_SHOT, bit5=EN_BOR_N, bit7=SWRST */
#define UCS116E0_INT_CTRL                0x01  /* bit0=EN_AINT, bit1=EN_CINT, bit4=ALS_CLR_SYNC */
#define UCS116E0_INT_FLAG                0x02  /* bit0=INT_ALS, bit1=INT_CLEAR, bit6=DATA_FLAG, bit7=INT_POR */
#define UCS116E0_WAIT_TIME               0x03  /* TWAIT = 6.5 + 10*(WTIME+1) ms */

//=============================================================================
// ALS control registers
//=============================================================================
#define UCS116E0_SUB_GAIN                0x04  /* bits[7:4]=CLEAR_SUB_GAIN, bits[3:0]=ALS_SUB_GAIN (x1~x128) */
#define UCS116E0_INTE_TIME               0x05  /* 1 unit=2ms, TINTE=(INTE_TIME+1)*2ms */
#define UCS116E0_ALS_CLR_GAIN            0x06  /* bits[6:4]=CLEAR_GAIN, bits[2:0]=ALS_GAIN (x1~x16) */
#define UCS116E0_PERSISTENCE             0x07  /* bits[3:0]=PRS_ALS */

//=============================================================================
// ALS interrupt threshold registers
//=============================================================================
#define UCS116E0_ALS_THRES_LL            0x08
#define UCS116E0_ALS_THRES_LH            0x09
#define UCS116E0_ALS_THRES_HL            0x0A
#define UCS116E0_ALS_THRES_HH            0x0B

//=============================================================================
// CLEAR interrupt threshold registers
//=============================================================================
#define UCS116E0_CLEAR_THRES_LL          0x0C
#define UCS116E0_CLEAR_THRES_LH          0x0D
#define UCS116E0_CLEAR_THRES_HL          0x0E
#define UCS116E0_CLEAR_THRES_HH          0x0F

//=============================================================================
// Compensation and sync registers
//=============================================================================
#define UCS116E0_ALS_COEF                0x13  /* ALS_DATA = ALS_RAW - (coef/128)*DARK_DATA, default=0x80 */
#define UCS116E0_ERROR_FLAG              0x17  /* bit6=DATA_READY, bit2=SAT_CLEAR, bit1=SAT_ALS, bit0=SAT_DARK */
#define UCS116E0_COMP_COEF               0x2C
#define UCS116E0_ALS_WIN_LOSS            0x2D  /* ALS window loss compensation, default=0x40 */
#define UCS116E0_CLEAR_WIN_LOSS          0x2E  /* CLEAR window loss compensation, default=0x40 */

//=============================================================================
// Data output registers (23-bit, must read L->M->H in order)
//=============================================================================
#define UCS116E0_DARK_DATA_L             0x1C
#define UCS116E0_DARK_DATA_M             0x1D
#define UCS116E0_DARK_DATA_H             0x1E
#define UCS116E0_ALS_DATA_L              0x20
#define UCS116E0_ALS_DATA_M              0x21
#define UCS116E0_ALS_DATA_H              0x22
#define UCS116E0_CLEAR_DATA_L            0x24
#define UCS116E0_CLEAR_DATA_M            0x25
#define UCS116E0_CLEAR_DATA_H            0x26

//=============================================================================
// Product ID registers
//=============================================================================
#define UCS116E0_PROD_ID_L               0xBC
#define UCS116E0_PROD_ID_H               0xBD
#define UCS116E0_PROD_ID_VAL             0x1111  /* PROD_ID_H=0x11, PROD_ID_L=0x11 */

//=============================================================================
// Init register values (from official UCapteur vendor pseudo code)
//=============================================================================
#define UCS116E0_INIT_SUB_GAIN           0x07  /* ALS_SUB_GAIN=x128, CLEAR_SUB_GAIN=x1 */
#define UCS116E0_INIT_INTE_TIME          0x3F  /* 64T * 2ms = 128ms integration time */
#define UCS116E0_INIT_ALS_CLR_GAIN       0x04  /* ALS_GAIN=x16, CLEAR_GAIN=x1 */

/*
 * Platform lux calibration (tune per module):
 * lux = als_comp * LUX_COEF_NUM * 64 * 128 /
 *       (LUX_COEF_DEN * pga_gain * sub_gain * inte_ms * win_loss)
 */
#define UCS116E0_LUX_COEF_NUM            1000
#define UCS116E0_LUX_COEF_DEN            1000
#define UCS116E0_INTE_TIME_UNIT_MS       2

#endif /* _UCS116E0_H_ */