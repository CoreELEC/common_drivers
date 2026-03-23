/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __SED_REG_H__
#define __SED_REG_H__

#define SED_VERSION_ADDR                           (0x0000  << 2)
#define SED_SYS_CFG_REG0                           (0x0004  << 2)
#define SED_SYS_CFG_REG1                           (0x0005  << 2)
#define SED_CMD_CHAIN                              (0x0010  << 2)
#define SED_IRQ_EN                                 (0x0011  << 2)
#define SED_IRQ_STS                                (0x0012  << 2)
#define SED_TIMELOG                                (0x0013  << 2)
#define SED_CMD_CFG0                               (0x0040  << 2)
#define SED_CMD_CFG1                               (0x0041  << 2)
#define SED_CMD_CFG2                               (0x0042  << 2)
#define SED_CMD_CFG3                               (0x0043  << 2)
#define SED_DSM_COEF0                              (0x0080  << 2)
#define SED_DSM_COEF1                              (0x0081  << 2)
#define SED_DSM_COEF2                              (0x0082  << 2)
#define SED_DSM_COEF3                              (0x0083  << 2)
#define SED_DSM_COEF4                              (0x0084  << 2)
#define SED_DSM_COEF5                              (0x0085  << 2)
#define SED_DSM_COEF6                              (0x0086  << 2)
#define SED_DSM_COEF7                              (0x0087  << 2)
#define SED_DSM_COEF8                              (0x0088  << 2)
#define SED_DSM_COEF9                              (0x0089  << 2)
#define SED_DSM_COEFA                              (0x008a  << 2)
#define SED_DSM_COEFB                              (0x008b  << 2)
#define SED_DSM_COEFC                              (0x008c  << 2)
#define SED_CFG_REG0                               (0x0090  << 2)
#define SED_CFG_REG1                               (0x0091  << 2)
#define SED_POW_CTRL0                              (0x0092  << 2)
#define SED_POW_CTRL1                              (0x0093  << 2)
#define SED_POW_CTRL2                              (0x0094  << 2)
#define SED_LOG_CTRL0                              (0x0095  << 2)
#define SED_LOG_CTRL1                              (0x0096  << 2)
#define SED_NN_CTRL0                               (0x0097  << 2)
#define SED_MEL_COEF_BASE_ADR                      (0x1200  << 2)

#define EE_AUDIO2_CLK_PDMIN_CTRL_0                  (0x0016  << 2)
#define EE_AUDIO2_CLK_PDMIN_CTRL2                  (0x0019  << 2)
#define EE_AUDIO2_TOSED_CTRL0                      (0x0020  << 2)
#define EE_AUDIO2_TOSED_CTRL1                      (0x0021  << 2)
#define EE_AUDIO2_TOSED_CH_EN                      (0x0022  << 2)

#define MEL_FILTER_REG 0x4800

#define SED_LOAD_MEMORY                            0x10000

#endif

