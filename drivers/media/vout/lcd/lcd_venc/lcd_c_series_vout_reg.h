/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_C_SERIES_REG_H__
#define __AML_LCD_C_SERIES_REG_H__

/* ********************************
 * C3 vout
 * ********************************
 */
#define VPU_VOUT_CORE_CTRL                         0x0100
#define VPU_VOUT_INT_CTRL                          0x0101
#define VPU_VOUT_DETH_CTRL                         0x0102
#define VPU_VOUT_DTH_DATA                          0x0103
#define VPU_VOUT_DTH_ADDR                          0x0104
#define VPU_VOUT_HS_POS                            0x0112
#define VPU_VOUT_VSLN_E_POS                        0x0113
#define VPU_VOUT_VSPX_E_POS                        0x0114
#define VPU_VOUT_VSLN_O_POS                        0x0115
#define VPU_VOUT_VSPX_O_POS                        0x0116
#define VPU_VOUT_DE_PX_EN                          0x0117
#define VPU_VOUT_DELN_E_POS                        0x0118
#define VPU_VOUT_DELN_O_POS                        0x0119
#define VPU_VOUT_MAX_SIZE                          0x011a
#define VPU_VOUT_FLD_BGN_LINE                      0x011b
#define VPU_VOUTO_HS_POS                           0x0120
#define VPU_VOUTO_VSLN_E_POS                       0x0121
#define VPU_VOUTO_VSPX_E_POS                       0x0122
#define VPU_VOUTO_VSLN_O_POS                       0x0123
#define VPU_VOUTO_VSPX_O_POS                       0x0124
#define VPU_VOUTO_DE_PX_EN                         0x0125
#define VPU_VOUTO_DELN_E_POS                       0x0126
#define VPU_VOUTO_DELN_O_POS                       0x0127
#define VPU_VOUTO_MAX_SIZE                         0x0128
#define VPU_VOUTO_FLD_BGN_LINE                     0x0129
#define VPU_VOUT_BT_CTRL                           0x0130
#define VPU_VOUT_BT_PLD_LINE                       0x0131
#define VPU_VOUT_BT_PLDIDT0                        0x0132
#define VPU_VOUT_BT_PLDIDT1                        0x0133
#define VPU_VOUT_BT_BLK_DATA                       0x0134
#define VPU_VOUT_BT_DAT_CLPY                       0x0135
#define VPU_VOUT_BT_DAT_CLPC                       0x0136
#define VPU_VOUT_RO_INT                            0x0140
#define VPU_VOUT_BLEND_CTRL                        0x0010
#define VPU_VOUT_BLEND_DUMDATA                     0x0011
#define VPU_VOUT_BLEND_SIZE                        0x0012
/* ********************************
 * C5 vout
 * ********************************
 */
#define VPU_VOUT_CORE_CTRL_C5                         0x0400
#define VPU_VOUT_INT_CTRL_C5                          0x0401
#define VPU_VOUT_DETH_CTRL_C5                         0x0402
#define VPU_VOUT_DTH_DATA_C5                          0x0403
#define VPU_VOUT_DTH_ADDR_C5                          0x0404
#define VPU_VOUT_HS_POS_C5                            0x0412
#define VPU_VOUT_VSLN_E_POS_C5                        0x0413
#define VPU_VOUT_VSPX_E_POS_C5                        0x0414
#define VPU_VOUT_VSLN_O_POS_C5                        0x0415
#define VPU_VOUT_VSPX_O_POS_C5                        0x0416
#define VPU_VOUT_DE_PX_EN_C5                          0x0417
#define VPU_VOUT_DELN_E_POS_C5                        0x0418
#define VPU_VOUT_DELN_O_POS_C5                        0x0419
#define VPU_VOUT_MAX_SIZE_C5                          0x041a
#define VPU_VOUT_FLD_BGN_LINE_C5                      0x041b
#define VPU_VOUT_BT_CTRL_C5                           0x0430
#define VPU_VOUT_BT_PLD_LINE_C5                       0x0431
#define VPU_VOUT_BT_PLDIDT0_C5                        0x0432
#define VPU_VOUT_BT_PLDIDT1_C5                        0x0433
#define VPU_VOUT_BT_BLK_DATA_C5                       0x0434
#define VPU_VOUT_BT_DAT_CLPY_C5                       0x0435
#define VPU_VOUT_BT_DAT_CLPC_C5                       0x0436
#define VPU_VOUT_LVDS_BIST_CTRL_C5                    0x0440
#define VPU_VOUT_LVDS_FIXED_VALE_C5                   0x0441
#define VPU_VOUT_LVDS_CNT1_C5                         0x0442
#define VPU_VOUT_LVDS_CLK_CFG_C5                      0x0443
#define VPU_VOUT_LVDS_CH_SWAP_C5                      0x0444
#define VPU_VOUT_LVDS_CH_SWAP_H16BIT_C5               0x0445
#define VPU_VOUT_LVDS_BLANK_DATA_C5                   0x0446
#define VPU_VOUT_LVDS_CH_SWAP0_C5                     0x0447
#define VPU_VOUT_LVDS_CH_SWAP1_C5                     0x0448
#define VPU_VOUT_BIST_CFG_YUV_C5                      0x0450
#define VPU_VOUT_CLRBAR_CFG_C5                        0x0451
#define VPU_VOUT_BIST_SEL_C5                          0x0452
#define VPU_VOUT_BIST_HIGH_C5                         0x0453
#define VPU_VOUT_GAMMA_CTRL_C5                        0x0458
#define VPU_VOUT_GAMMA_PROB0_C5                       0x0459
#define VPU_VOUT_GAMMA_PROB1_C5                       0x045a
#define VPU_VOUT_RO_GAMMA_PROB_C5                     0x045b
#define VPU_VOUT_RO_INT_C5                            0x0460
#define VPU_VOUT_LVDS_RO_COVGE_C5                     0x0461
#define VPU_VOUT_BLEND_CTRL_C5                        0x0040
#define VPU_VOUT_BLEND_DUMDATA_C5                     0x0041
#define VPU_VOUT_BLEND_SIZE_C5                        0x0042

#endif
