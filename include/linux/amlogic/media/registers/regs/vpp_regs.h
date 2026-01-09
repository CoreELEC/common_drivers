/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef VPP_REGS_HEADER_
#define VPP_REGS_HEADER_

#define VPP_DUMMY_DATA                    0x1d00
#define VPP_LINE_IN_LENGTH                0x1d01
#define VPP_PIC_IN_HEIGHT                 0x1d02
#define VPP_SCALE_COEF_IDX                0x1d03
#define VPP_SCALE_COEF                    0x1d04
#define VPP_VSC_REGION12_STARTP           0x1d05
#define VPP_VSC_REGION34_STARTP           0x1d06
#define VPP_VSC_REGION4_ENDP              0x1d07
#define VPP_VSC_START_PHASE_STEP          0x1d08
#define VPP_VSC_REGION0_PHASE_SLOPE       0x1d09
#define VPP_VSC_REGION1_PHASE_SLOPE       0x1d0a
#define VPP_VSC_REGION3_PHASE_SLOPE       0x1d0b
#define VPP_VSC_REGION4_PHASE_SLOPE       0x1d0c
#define VPP_VSC_PHASE_CTRL                0x1d0d
#define VPP_VSC_INI_PHASE                 0x1d0e
#define VPP_HSC_REGION12_STARTP           0x1d10
#define VPP_HSC_REGION34_STARTP           0x1d11
#define VPP_HSC_REGION4_ENDP              0x1d12
#define VPP_HSC_START_PHASE_STEP          0x1d13
#define VPP_HSC_REGION0_PHASE_SLOPE       0x1d14
#define VPP_HSC_REGION1_PHASE_SLOPE       0x1d15
#define VPP_HSC_REGION3_PHASE_SLOPE       0x1d16
#define VPP_HSC_REGION4_PHASE_SLOPE       0x1d17
#define VPP_HSC_PHASE_CTRL                0x1d18
#define VPP_SC_MISC                       0x1d19
#define VPP_PREBLEND_VD1_H_START_END      0x1d1a
#define VPP_PREBLEND_VD1_V_START_END      0x1d1b
#define VPP_POSTBLEND_VD1_H_START_END     0x1d1c
#define VPP_POSTBLEND_VD1_V_START_END     0x1d1d
#define VPP_BLEND_VD2_H_START_END         0x1d1e
#define VPP_BLEND_VD2_V_START_END         0x1d1f
#define VPP_PREBLEND_H_SIZE               0x1d20
#define VPP_POSTBLEND_H_SIZE              0x1d21
#define VPP_HOLD_LINES                    0x1d22
#define VPP_BLEND_ONECOLOR_CTRL           0x1d23
#define VPP_PREBLEND_CURRENT_XY           0x1d24
#define VPP_POSTBLEND_CURRENT_XY          0x1d25
#define VPP_MISC                          0x1d26
#define VPP_OFIFO_SIZE                    0x1d27
#define VPP_FIFO_STATUS                   0x1d28
#define VPP_SMOKE_CTRL                    0x1d29
#define VPP_SMOKE1_VAL                    0x1d2a
#define VPP_SMOKE2_VAL                    0x1d2b
#define VPP_SMOKE3_VAL                    0x1d2c
#define VPP_SMOKE1_H_START_END            0x1d2d
#define VPP_SMOKE1_V_START_END            0x1d2e
#define VPP_SMOKE2_H_START_END            0x1d2f
#define VPP_SMOKE2_V_START_END            0x1d30
#define VPP_SMOKE3_H_START_END            0x1d31
#define VPP_SMOKE3_V_START_END            0x1d32
#define VPP_SCO_FIFO_CTRL                 0x1d33
#define VPP_HSC_PHASE_CTRL1               0x1d34
#define VPP_HSC_INI_PAT_CTRL              0x1d35
#define VPP_SC_GCLK_CTRL_T7               0x1d36
#define VPP_PREHSC_COEF                   0x1d37
#define VPP_PREHSC_CTRL                   0x1d38
#define VPP_PRE_SCALE_CTRL                0x1d38
#define VPP_PREVSC_COEF                   0x1d39
#define VPP_PREHSC_COEF1                  0x1d3a

#define VPP_VADJ_CTRL                     0x1d40
#define VPP_VADJ1_Y                       0x1d41
#define VPP_VADJ1_MA_MB                   0x1d42
#define VPP_VADJ1_MC_MD                   0x1d43
#define VPP_VADJ2_Y                       0x1d44
#define VPP_VADJ2_MA_MB                   0x1d45
#define VPP_VADJ2_MC_MD                   0x1d46
#define VPP_HSHARP_CTRL                   0x1d50
#define VPP_HSHARP_LUMA_THRESH01          0x1d51
#define VPP_HSHARP_LUMA_THRESH23          0x1d52
#define VPP_HSHARP_CHROMA_THRESH01        0x1d53
#define VPP_HSHARP_CHROMA_THRESH23        0x1d54
#define VPP_HSHARP_LUMA_GAIN              0x1d55
#define VPP_HSHARP_CHROMA_GAIN            0x1d56
#define VPP_MATRIX_PROBE_COLOR            0x1d5c
#define VPP_MATRIX_HL_COLOR               0x1d5d
#define VPP_MATRIX_PROBE_POS              0x1d5e
#define VPP_MATRIX_CTRL                   0x1d5f
#define VPP_MATRIX_COEF00_01              0x1d60
#define VPP_MATRIX_COEF02_10              0x1d61
#define VPP_MATRIX_COEF11_12              0x1d62
#define VPP_MATRIX_COEF20_21              0x1d63
#define VPP_MATRIX_COEF22                 0x1d64
#define VPP_MATRIX_OFFSET0_1              0x1d65
#define VPP_MATRIX_OFFSET2                0x1d66
#define VPP_MATRIX_PRE_OFFSET0_1          0x1d67
#define VPP_MATRIX_PRE_OFFSET2            0x1d68
#define VPP_DUMMY_DATA1                   0x1d69
#define VPP_GAINOFF_CTRL0                 0x1d6a
#define VPP_GAINOFF_CTRL1                 0x1d6b
#define VPP_GAINOFF_CTRL2                 0x1d6c
#define VPP_GAINOFF_CTRL3                 0x1d6d
#define VPP_GAINOFF_CTRL4                 0x1d6e
#define VPP_CHROMA_ADDR_PORT              0x1d70
#define VPP_CHROMA_DATA_PORT              0x1d71
#define VPP_GCLK_CTRL0                    0x1d72
#define VPP_GCLK_CTRL1                    0x1d73
#define VPP_SC_GCLK_CTRL                  0x1d74
#define VPP_MISC1                         0x1d76
#define VPP_SRSCL_GCLK_CTRL               0x1d77
#define VPP_BLACKEXT_CTRL                 0x1d80
#define VPP_DNLP_CTRL_00                  0x1d81
#define VPP_DNLP_CTRL_01                  0x1d82
#define VPP_DNLP_CTRL_02                  0x1d83
#define VPP_DNLP_CTRL_03                  0x1d84
#define VPP_DNLP_CTRL_04                  0x1d85
#define VPP_DNLP_CTRL_05                  0x1d86
#define VPP_DNLP_CTRL_06                  0x1d87
#define VPP_DNLP_CTRL_07                  0x1d88
#define VPP_DNLP_CTRL_08                  0x1d89
#define VPP_DNLP_CTRL_09                  0x1d8a
#define VPP_DNLP_CTRL_10                  0x1d8b
#define VPP_DNLP_CTRL_11                  0x1d8c
#define VPP_DNLP_CTRL_12                  0x1d8d
#define VPP_DNLP_CTRL_13                  0x1d8e
#define VPP_DNLP_CTRL_14                  0x1d8f
#define VPP_DNLP_CTRL_15                  0x1d90
#define VPP_SRSHARP0_CTRL                 0x1d91
#define VPP_SRSHARP1_CTRL                 0x1d92
#define VPP_PEAKING_NLP_1                 0x1d93
/* gxm has no super-core */
#define VPP_AMDV_CTRL                    0x1d93
#define VPP_PEAKING_NLP_2                 0x1d94
#define VPP_PEAKING_NLP_3                 0x1d95
#define VPP_PEAKING_NLP_4                 0x1d96
#define VPP_PEAKING_NLP_5                 0x1d97
#define VPP_SHARP_LIMIT                   0x1d98
#define VPP_VLTI_CTRL                     0x1d99
#define VPP_HLTI_CTRL                     0x1d9a
#define VPP_CTI_CTRL                      0x1d9b
#define VPP_BLUE_STRETCH_1                0x1d9c
#define VPP_BLUE_STRETCH_2                0x1d9d
#define VPP_BLUE_STRETCH_3                0x1d9e
#define VPP_CCORING_CTRL                  0x1da0
#define VPP_VE_ENABLE_CTRL                0x1da1
#define VPP_VE_DEMO_LEFT_TOP_SCREEN_WIDTH 0x1da2
#define VPP_VE_DEMO_CENTER_BAR            0x1da3
#define VPP_VE_H_V_SIZE                   0x1da4
#define VPP_PSR_H_V_SIZE                  0x1da5
#define VPP_OUT_H_V_SIZE                  0x1da5
#define VPP_IN_H_V_SIZE                   0x1da6
#define VPP_VDO_MEAS_CTRL                 0x1da8
#define VPP_VDO_MEAS_VS_COUNT_HI          0x1da9
#define VPP_VDO_MEAS_VS_COUNT_LO          0x1daa
#define VPP_INPUT_CTRL                    0x1dab
#define VPP_CTI_CTRL2                     0x1dac
#define VPP_PEAKING_SAT_THD1              0x1dad
#define VPP_PEAKING_SAT_THD2              0x1dae
#define VPP_PEAKING_SAT_THD3              0x1daf
#define VPP_PEAKING_SAT_THD4              0x1db0
#define VPP_PEAKING_SAT_THD5              0x1db1
#define VPP_PEAKING_SAT_THD6              0x1db2
#define VPP_PEAKING_SAT_THD7              0x1db3
#define VPP_PEAKING_SAT_THD8              0x1db4
#define VPP_PEAKING_SAT_THD9              0x1db5
#define VPP_PEAKING_GAIN_ADD1             0x1db6
#define VPP_PEAKING_GAIN_ADD2             0x1db7
#define VPP_PEAKING_DNLP                  0x1db8
#define VPP_SHARP_DEMO_WIN_CTRL1          0x1db9
#define VPP_SHARP_DEMO_WIN_CTRL2          0x1dba
#define VPP_FRONT_HLTI_CTRL               0x1dbb
#define VPP_FRONT_CTI_CTRL                0x1dbc
#define VPP_FRONT_CTI_CTRL2               0x1dbd
#define VPP_OSD_VSC_PHASE_STEP            0x1dc0
#define VPP_OSD_VSC_INI_PHASE             0x1dc1
#define VPP_OSD_VSC_CTRL0                 0x1dc2
#define VPP_OSD_HSC_PHASE_STEP            0x1dc3
#define VPP_OSD_HSC_INI_PHASE             0x1dc4
#define VPP_OSD_HSC_CTRL0                 0x1dc5
#define VPP_OSD_HSC_INI_PAT_CTRL          0x1dc6
#define VPP_OSD_SC_DUMMY_DATA             0x1dc7
#define VPP_OSD_SC_CTRL0                  0x1dc8
#define VPP_OSD_SCI_WH_M1                 0x1dc9
#define VPP_OSD_SCO_H_START_END           0x1dca
#define VPP_OSD_SCO_V_START_END           0x1dcb
#define VPP_OSD_SCALE_COEF_IDX            0x1dcc
#define VPP_OSD_SCALE_COEF                0x1dcd
#define VPP_INT_LINE_NUM                  0x1dce

#define VPP_CLIP_MISC0                    0x1dd9
#define VPP_CLIP_MISC1                    0x1dda

/* vpu vout clip for c5 */
#define VPU_VOUT_CLIP_MISC0               0x0074
#define VPU_VOUT_CLIP_MISC1               0x0075
#define VPU_VOUT_VD1_CLIP_MISC0           0x0076
#define VPU_VOUT_VD1_CLIP_MISC1           0x0077

#define VPP_VD1_CLIP_MISC0                0x1de1
#define VPP_VD1_CLIP_MISC1                0x1de2
#define VPP_VD2_CLIP_MISC0                0x1de3
#define VPP_VD2_CLIP_MISC1                0x1de4
#define VPP_VD3_CLIP_MISC0                0x1de5
#define VPP_VD3_CLIP_MISC1                0x1de6

#define VPP2_MISC                         0x1e26
#define VPP2_OFIFO_SIZE                   0x1e27
#define VPP2_INT_LINE_NUM                 0x1e20
#define VPP2_OFIFO_URG_CTRL               0x1e21

#define SRSHARP0_SHARP_HVSIZE             0x3200
#define SRSHARP0_SHARP_HVBLANK_NUM        0x3201
#define SRSHARP0_SHARP_PK_NR_ENABLE       0x3227
#define SRSHARP0_SHARP_DNLP_EN            0x3245
#define SRSHARP0_SHARP_SR2_CTRL           0x3257
#define SRSHARP0_SHARP_SR2_CTRL2          0x3364
#define SRSHARP1_SHARP_HVSIZE             0x3280
#define SRSHARP1_SHARP_HVBLANK_NUM        0x3281
#define SRSHARP1_SHARP_PK_NR_ENABLE       0x32a7
#define SRSHARP1_SHARP_DNLP_EN            0x32c5
#define SRSHARP1_SHARP_SR2_CTRL           0x32d7
#define SRSHARP1_DEMO_MODE_WINDOW_CTRL0   0x3400
#define SRSHARP1_NN_POST_TOP              0x3402
#define SRSHARP1_SHARP_SR2_CTRL2          0x33e4

#define VPP_POST_MATRIX_SAT               0x32c1

/* g12a vd2 pps */
/* for sc2 */
#define VD2_PREHSC_COEF                   0x3937
#define VD2_PREHSC_CTRL                   0x3938
/* for t5d, s4 */
#define VD2_PREHSC_COEF_T5D               0x395d
#define VD2_PRE_SCALE_CTRL                0x395e
#define VD2_PREVSC_COEF                   0x395f
#define VD2_PREVSC_COEF1                  0x3942

#define VD2_SCALE_COEF_IDX                0x3943
#define VD2_SCALE_COEF                    0x3944
#define VD2_VSC_REGION12_STARTP           0x3945
#define VD2_VSC_REGION34_STARTP           0x3946
#define VD2_VSC_REGION4_ENDP              0x3947
#define VD2_VSC_START_PHASE_STEP          0x3948
#define VD2_VSC_REGION0_PHASE_SLOPE       0x3949
#define VD2_VSC_REGION1_PHASE_SLOPE       0x394a
#define VD2_VSC_REGION3_PHASE_SLOPE       0x394b
#define VD2_VSC_REGION4_PHASE_SLOPE       0x394c
#define VD2_VSC_PHASE_CTRL                0x394d
#define VD2_VSC_INI_PHASE                 0x394e
#define VD2_HSC_REGION12_STARTP           0x394f
#define VD2_HSC_REGION34_STARTP           0x3950
#define VD2_HSC_REGION4_ENDP              0x3951
#define VD2_HSC_START_PHASE_STEP          0x3952
#define VD2_HSC_REGION0_PHASE_SLOPE       0x3953
#define VD2_HSC_REGION1_PHASE_SLOPE       0x3954
#define VD2_HSC_REGION3_PHASE_SLOPE       0x3955
#define VD2_HSC_REGION4_PHASE_SLOPE       0x3956
#define VD2_HSC_PHASE_CTRL                0x3957
#define VD2_SC_MISC                       0x3958
#define VD2_SCO_FIFO_CTRL                 0x3959
#define VD2_HSC_PHASE_CTRL1               0x395a
#define VD2_HSC_INI_PAT_CTRL              0x395b
#define VD2_SC_GCLK_CTRL                  0x395c
#define VPP_VD2_HDR_IN_SIZE               0x1df0

/* t7 vd2 pps */
#define VD2_SCALE_COEF_IDX_T7                         0x3933
#define VD2_SCALE_COEF_T7                             0x3934
#define VD2_VSC_REGION12_STARTP_T7                    0x3935
#define VD2_VSC_REGION34_STARTP_T7                    0x3936
#define VD2_VSC_REGION4_ENDP_T7                       0x3937
#define VD2_VSC_START_PHASE_STEP_T7                   0x3938
#define VD2_VSC_REGION0_PHASE_SLOPE_T7                0x3939
#define VD2_VSC_REGION1_PHASE_SLOPE_T7                0x393a
#define VD2_VSC_REGION3_PHASE_SLOPE_T7                0x393b
#define VD2_VSC_REGION4_PHASE_SLOPE_T7                0x393c
#define VD2_VSC_PHASE_CTRL_T7                         0x393d
#define VD2_VSC_INI_PHASE_T7                          0x393e
#define VD2_HSC_REGION12_STARTP_T7                    0x393f
#define VD2_HSC_REGION34_STARTP_T7                    0x3940
#define VD2_HSC_REGION4_ENDP_T7                       0x3941
#define VD2_HSC_START_PHASE_STEP_T7                   0x3942
#define VD2_HSC_REGION0_PHASE_SLOPE_T7                0x3943
#define VD2_HSC_REGION1_PHASE_SLOPE_T7                0x3944
#define VD2_HSC_REGION3_PHASE_SLOPE_T7                0x3945
#define VD2_HSC_REGION4_PHASE_SLOPE_T7                0x3946
#define VD2_HSC_PHASE_CTRL_T7                         0x3947
#define VD2_SC_MISC_T7                                0x3948
#define VD2_SCO_FIFO_CTRL_T7                          0x3949
#define VD2_HSC_PHASE_CTRL1_T7                        0x394a
#define VD2_HSC_INI_PAT_CTRL_T7                       0x394b
#define VD2_SC_GCLK_CTRL_T7                           0x394c
#define VD2_PREHSC_COEF_T7                            0x394d
#define VD2_PRE_SCALE_CTRL_T7                         0x394e
#define VD2_PREVSC_COEF_T7                            0x394f
#define VD2_PREHSC_COEF1_T7                           0x3950

/* t7 vd3 pps */
#define VD3_SCALE_COEF_IDX                         0x5903
#define VD3_SCALE_COEF                             0x5904
#define VD3_VSC_REGION12_STARTP                    0x5905
#define VD3_VSC_REGION34_STARTP                    0x5906
#define VD3_VSC_REGION4_ENDP                       0x5907
#define VD3_VSC_START_PHASE_STEP                   0x5908
#define VD3_VSC_REGION0_PHASE_SLOPE                0x5909
#define VD3_VSC_REGION1_PHASE_SLOPE                0x590a
#define VD3_VSC_REGION3_PHASE_SLOPE                0x590b
#define VD3_VSC_REGION4_PHASE_SLOPE                0x590c
#define VD3_VSC_PHASE_CTRL                         0x590d
#define VD3_VSC_INI_PHASE                          0x590e
#define VD3_HSC_REGION12_STARTP                    0x590f
#define VD3_HSC_REGION34_STARTP                    0x5910
#define VD3_HSC_REGION4_ENDP                       0x5911
#define VD3_HSC_START_PHASE_STEP                   0x5912
#define VD3_HSC_REGION0_PHASE_SLOPE                0x5913
#define VD3_HSC_REGION1_PHASE_SLOPE                0x5914
#define VD3_HSC_REGION3_PHASE_SLOPE                0x5915
#define VD3_HSC_REGION4_PHASE_SLOPE                0x5916
#define VD3_HSC_PHASE_CTRL                         0x5917
#define VD3_SC_MISC                                0x5918
#define VD3_SCO_FIFO_CTRL                          0x5919
#define VD3_HSC_PHASE_CTRL1                        0x591a
#define VD3_HSC_INI_PAT_CTRL                       0x591b
#define VD3_SC_GCLK_CTRL                           0x591c
#define VD3_PREHSC_COEF                            0x591d
#define VD3_PRE_SCALE_CTRL                         0x591e
#define VD3_PREVSC_COEF                            0x591f
#define VD3_PREHSC_COEF1                           0x5920

/* t3 aisr pps */
#define SCHN_SCALE_COEF_IDX                        0x2e70
#define SCHN_SCALE_COEF                            0x2e71
#define SCHN_VSC_REGION12_STARTP                   0x2e72
#define SCHN_VSC_REGION34_STARTP                   0x2e73
#define SCHN_VSC_REGION4_ENDP                      0x2e74
#define SCHN_VSC_START_PHASE_STEP                  0x2e75
#define SCHN_VSC_REGION0_PHASE_SLOPE               0x2e76
#define SCHN_VSC_REGION1_PHASE_SLOPE               0x2e77
#define SCHN_VSC_REGION3_PHASE_SLOPE               0x2e78
#define SCHN_VSC_REGION4_PHASE_SLOPE               0x2e79
#define SCHN_VSC_PHASE_CTRL                        0x2e7a
#define SCHN_VSC_INI_PHASE                         0x2e7b
#define SCHN_HSC_REGION12_STARTP                   0x2e7c
#define SCHN_HSC_REGION34_STARTP                   0x2e7d
#define SCHN_HSC_REGION4_ENDP                      0x2e7e
#define SCHN_HSC_START_PHASE_STEP                  0x2e7f
#define SCHN_HSC_REGION0_PHASE_SLOPE               0x2e80
#define SCHN_HSC_REGION1_PHASE_SLOPE               0x2e81
#define SCHN_HSC_REGION3_PHASE_SLOPE               0x2e82
#define SCHN_HSC_REGION4_PHASE_SLOPE               0x2e83
#define SCHN_HSC_PHASE_CTRL                        0x2e84
#define SCHN_SC_MISC                               0x2e85
#define SCHN_SCO_FIFO_CTRL                         0x2e86
#define SCHN_HSC_PHASE_CTRL1                       0x2e87
#define SCHN_HSC_INI_PAT_CTRL                      0x2e88
#define SCHN_SC_GCLK_CTRL                          0x2e89
#define SCHN_PREHSC_COEF                           0x2e8a
#define SCHN_PREVSC_COEF                           0x2e8b
#define SCHN_PRE_SCALE_CTRL                        0x2e8c
#define SCHN_PREHSC_COEF1                          0x2e8d

#define VD1_BLEND_SRC_CTRL                0x1dfb
#define VD2_BLEND_SRC_CTRL                0x1dfc
#define OSD1_BLEND_SRC_CTRL               0x1dfd
#define OSD2_BLEND_SRC_CTRL               0x1dfe

#define VPP_BLEND_VD3_H_START_END                  0x1deb
#define VPP_BLEND_VD3_V_START_END                  0x1dec
#define VD3_BLEND_SRC_CTRL                         0x1def

#define VPP_POST_BLEND_BLEND_DUMMY_DATA   0x3968
#define VPP_POST_BLEND_DUMMY_ALPHA        0x3969

/* after g12b */
#define SRSHARP0_SHARP_SYNC_CTRL          0x3eb0
#define SRSHARP1_SHARP_SYNC_CTRL          0x3fb0
/* after tm2revb */
#define TM2REVB_SRSHARP0_SHARP_SYNC_CTRL 0x50b0
#define TM2REVB_SRSHARP1_SHARP_SYNC_CTRL 0x52b0
#define VPU_RDARB_MODE_L2C1               0x279d
#define VPU_WRARB_MODE_L2C1               0x27a2
#define VPP_XVYCC_MISC   0x1dcf
#define VPP_XVYCC_MISC0  0x1ddf

/* vpp crc */
#define VPP_RO_CRCSUM           0x1db2
#define VPP_CRC_CHK             0x1db3

/*viu2  vpp crc */
#define VPP2_CRC_CHK            0x1eb0
#define VPP2_RO_CRCSUM          0x1eb1

/* vpp1 crc A9 new add */
#define VPP1_TOP_CTRL           0x6480
#define VPP1_CRC_RO             0x6483

/* a9 vpp1 ofifo */
#define VPP1_LFIFO_CTRL                            0x6482

/* vpp crc for c5 */
#define VPU_VOUT_CRC_CHK        0x0073
#define VPU_VOUT_RO_CRCSUM      0x0079

/* pip alpha gen */
#define VD1_PIP_ALPH_CTRL                          0x5880
#define VD1_PIP_ALPH_SCP_H_0                       0x5881
#define VD1_PIP_ALPH_SCP_H_1                       0x5882
#define VD1_PIP_ALPH_SCP_H_2                       0x5883
#define VD1_PIP_ALPH_SCP_H_3                       0x5884
#define VD1_PIP_ALPH_SCP_H_4                       0x5885
#define VD1_PIP_ALPH_SCP_H_5                       0x5886
#define VD1_PIP_ALPH_SCP_H_6                       0x5887
#define VD1_PIP_ALPH_SCP_H_7                       0x5888
#define VD1_PIP_ALPH_SCP_H_8                       0x5889
#define VD1_PIP_ALPH_SCP_H_9                       0x588a
#define VD1_PIP_ALPH_SCP_H_10                      0x588b
#define VD1_PIP_ALPH_SCP_H_11                      0x588c
#define VD1_PIP_ALPH_SCP_H_12                      0x588d
#define VD1_PIP_ALPH_SCP_H_13                      0x588e
#define VD1_PIP_ALPH_SCP_H_14                      0x588f
#define VD1_PIP_ALPH_SCP_H_15                      0x5890
#define VD1_PIP_ALPH_SCP_V_0                       0x5891
#define VD1_PIP_ALPH_SCP_V_1                       0x5892
#define VD1_PIP_ALPH_SCP_V_2                       0x5893
#define VD1_PIP_ALPH_SCP_V_3                       0x5894
#define VD1_PIP_ALPH_SCP_V_4                       0x5895
#define VD1_PIP_ALPH_SCP_V_5                       0x5896
#define VD1_PIP_ALPH_SCP_V_6                       0x5897
#define VD1_PIP_ALPH_SCP_V_7                       0x5898
#define VD1_PIP_ALPH_SCP_V_8                       0x5899
#define VD1_PIP_ALPH_SCP_V_9                       0x589a
#define VD1_PIP_ALPH_SCP_V_10                      0x589b
#define VD1_PIP_ALPH_SCP_V_11                      0x589c
#define VD1_PIP_ALPH_SCP_V_12                      0x589d
#define VD1_PIP_ALPH_SCP_V_13                      0x589e
#define VD1_PIP_ALPH_SCP_V_14                      0x589f
#define VD1_PIP_ALPH_SCP_V_15                      0x58a0

#define VD2_PIP_ALPH_CTRL                          0x58b0
#define VD2_PIP_ALPH_SCP_H_0                       0x58b1
#define VD2_PIP_ALPH_SCP_H_1                       0x58b2
#define VD2_PIP_ALPH_SCP_H_2                       0x58b3
#define VD2_PIP_ALPH_SCP_H_3                       0x58b4
#define VD2_PIP_ALPH_SCP_H_4                       0x58b5
#define VD2_PIP_ALPH_SCP_H_5                       0x58b6
#define VD2_PIP_ALPH_SCP_H_6                       0x58b7
#define VD2_PIP_ALPH_SCP_H_7                       0x58b8
#define VD2_PIP_ALPH_SCP_H_8                       0x58b9
#define VD2_PIP_ALPH_SCP_H_9                       0x58ba
#define VD2_PIP_ALPH_SCP_H_10                      0x58bb
#define VD2_PIP_ALPH_SCP_H_11                      0x58bc
#define VD2_PIP_ALPH_SCP_H_12                      0x58bd
#define VD2_PIP_ALPH_SCP_H_13                      0x58be
#define VD2_PIP_ALPH_SCP_H_14                      0x58bf
#define VD2_PIP_ALPH_SCP_H_15                      0x58c0
#define VD2_PIP_ALPH_SCP_V_0                       0x58c1
#define VD2_PIP_ALPH_SCP_V_1                       0x58c2
#define VD2_PIP_ALPH_SCP_V_2                       0x58c3
#define VD2_PIP_ALPH_SCP_V_3                       0x58c4
#define VD2_PIP_ALPH_SCP_V_4                       0x58c5
#define VD2_PIP_ALPH_SCP_V_5                       0x58c6
#define VD2_PIP_ALPH_SCP_V_6                       0x58c7
#define VD2_PIP_ALPH_SCP_V_7                       0x58c8
#define VD2_PIP_ALPH_SCP_V_8                       0x58c9
#define VD2_PIP_ALPH_SCP_V_9                       0x58ca
#define VD2_PIP_ALPH_SCP_V_10                      0x58cb
#define VD2_PIP_ALPH_SCP_V_11                      0x58cc
#define VD2_PIP_ALPH_SCP_V_12                      0x58cd
#define VD2_PIP_ALPH_SCP_V_13                      0x58ce
#define VD2_PIP_ALPH_SCP_V_14                      0x58cf
#define VD2_PIP_ALPH_SCP_V_15                      0x58d0

#define VD3_PIP_ALPH_CTRL                          0x5850
#define VD3_PIP_ALPH_SCP_H_0                       0x5851
#define VD3_PIP_ALPH_SCP_H_1                       0x5852
#define VD3_PIP_ALPH_SCP_H_2                       0x5853
#define VD3_PIP_ALPH_SCP_H_3                       0x5854
#define VD3_PIP_ALPH_SCP_H_4                       0x5855
#define VD3_PIP_ALPH_SCP_H_5                       0x5856
#define VD3_PIP_ALPH_SCP_H_6                       0x5857
#define VD3_PIP_ALPH_SCP_H_7                       0x5858
#define VD3_PIP_ALPH_SCP_H_8                       0x5859
#define VD3_PIP_ALPH_SCP_H_9                       0x585a
#define VD3_PIP_ALPH_SCP_H_10                      0x585b
#define VD3_PIP_ALPH_SCP_H_11                      0x585c
#define VD3_PIP_ALPH_SCP_H_12                      0x585d
#define VD3_PIP_ALPH_SCP_H_13                      0x585e
#define VD3_PIP_ALPH_SCP_H_14                      0x585f
#define VD3_PIP_ALPH_SCP_H_15                      0x5860
#define VD3_PIP_ALPH_SCP_V_0                       0x5861
#define VD3_PIP_ALPH_SCP_V_1                       0x5862
#define VD3_PIP_ALPH_SCP_V_2                       0x5863
#define VD3_PIP_ALPH_SCP_V_3                       0x5864
#define VD3_PIP_ALPH_SCP_V_4                       0x5865
#define VD3_PIP_ALPH_SCP_V_5                       0x5866
#define VD3_PIP_ALPH_SCP_V_6                       0x5867
#define VD3_PIP_ALPH_SCP_V_7                       0x5868
#define VD3_PIP_ALPH_SCP_V_8                       0x5869
#define VD3_PIP_ALPH_SCP_V_9                       0x586a
#define VD3_PIP_ALPH_SCP_V_10                      0x586b
#define VD3_PIP_ALPH_SCP_V_11                      0x586c
#define VD3_PIP_ALPH_SCP_V_12                      0x586d
#define VD3_PIP_ALPH_SCP_V_13                      0x586e
#define VD3_PIP_ALPH_SCP_V_14                      0x586f
#define VD3_PIP_ALPH_SCP_V_15                      0x5870

#define G12_FGRAIN_CTRL                  0x4800
#define G12_FGRAIN_WIN_H                 0x4801
#define G12_FGRAIN_WIN_V                 0x4802
#define G12_VD2_FGRAIN_CTRL              0x4810
#define G12_VD2_FGRAIN_WIN_H             0x4811
#define G12_VD2_FGRAIN_WIN_V             0x4812

#define SC2_FGRAIN_CTRL                  0x4870
#define SC2_FGRAIN_WIN_H                 0x4871
#define SC2_FGRAIN_WIN_V                 0x4872
#define SC2_VD2_FGRAIN_CTRL              0x48f0
#define SC2_VD2_FGRAIN_WIN_H             0x48f1
#define SC2_VD2_FGRAIN_WIN_V             0x48f2
#define VD3_FGRAIN_CTRL                  0x4970
#define VD3_FGRAIN_WIN_H                 0x4971
#define VD3_FGRAIN_WIN_V                 0x4972

#define MALI_AFBCD_TOP_CTRL                        0x1a0f
#define VD1_HDR_IN_SIZE                            0x1a57
#define VD2_HDR_IN_SIZE                            0x1a58
#define VD3_HDR_IN_SIZE                            0x1a59
#define VPP_MISC2                                  0x1d7a
#define MALI_AFBCD1_TOP_CTRL                       0x1a55
#define MALI_AFBCD2_TOP_CTRL                       0x1a56
#define VD_PATH_MISC_CTRL                          0x1a7a
#define T7_VD2_PPS_DUMMY_DATA                      0x1a81
#define VD3_PPS_DUMMY_DATA                         0x1a82
#define VPP_VD1_DSC_CTRL                           0x1a83
#define VPP_VD2_DSC_CTRL                           0x1a84
#define VPP_VD3_DSC_CTRL                           0x1a85
#define PATH_START_SEL                             0x1a8a

//T6W added
#define VPU_HDR2_SIZE_IN                           0x4514

#define VIU_FRM_CTRL                               0x1a51
#define VIU1_FRM_CTRL                              0x1a8d
#define VIU2_FRM_CTRL                              0x1a8e

#define VPU_VENC_ERROR                             0x1cea
#define VPU_VENC_CTRL                              0x1cef
#define VPU1_VENC_CTRL                             0x22ef
#define VPU2_VENC_CTRL                             0x24ef

#define VPU_ENC_ERROR                             0x270c
//a9 added
#define VPU1_VENC_ERROR                            0x22ea


#define VPP1_BLD_CTRL                              0x5985
#define VPP1_BLD_OUT_SIZE                          0x5986
#define VPP1_BLD_DIN0_HSCOPE                       0x5987
#define VPP1_BLD_DIN0_VSCOPE                       0x5988
#define VPP1_BLD_DIN1_HSCOPE                       0x5989
#define VPP1_BLD_DIN1_VSCOPE                       0x598a
#define VPP1_BLD_DIN2_HSCOPE                       0x598b
#define VPP1_BLD_DIN2_VSCOPE                       0x598c
#define VPP1_BLEND_CTRL                            0x59a8
#define VPP1_BLEND_BLEND_DUMMY_DATA                0x59a9
#define VPP1_BLEND_DUMMY_ALPHA                     0x59aa

#define VPP2_BLD_CTRL                              0x59c5
#define VPP2_BLD_OUT_SIZE                          0x59c6
#define VPP2_BLD_DIN0_HSCOPE                       0x59c7
#define VPP2_BLD_DIN0_VSCOPE                       0x59c8
#define VPP2_BLD_DIN1_HSCOPE                       0x59c9
#define VPP2_BLD_DIN1_VSCOPE                       0x59ca
#define VPP2_BLD_DIN2_HSCOPE                       0x59cb
#define VPP2_BLD_DIN2_VSCOPE                       0x59cc
#define VPP2_BLEND_CTRL                            0x59e8
#define VPP2_BLEND_BLEND_DUMMY_DATA                0x59e9
#define VPP2_BLEND_DUMMY_ALPHA                     0x59ea

#define A9_VPP1_BLD_CTRL                              0x6485
#define A9_VPP1_BLD_OUT_SIZE                          0x6486
#define A9_VPP1_BLD_DIN0_HSCOPE                       0x6487
#define A9_VPP1_BLD_DIN0_VSCOPE                       0x6488
#define A9_VPP1_BLD_DIN1_HSCOPE                       0x6489
#define A9_VPP1_BLD_DIN1_VSCOPE                       0x648a
#define A9_VPP1_BLD_DIN2_HSCOPE                       0x648b
#define A9_VPP1_BLD_DIN2_VSCOPE                       0x648c
#define A9_VPP1_BLEND_CTRL                            0x64a8
#define A9_VPP1_BLEND_BLEND_DUMMY_DATA                0x64a9
#define A9_VPP1_BLEND_DUMMY_ALPHA                     0x64aa

#define A9_VPP2_BLD_CTRL                              0x64c5
#define A9_VPP2_BLD_OUT_SIZE                          0x64c6
#define A9_VPP2_BLD_DIN0_HSCOPE                       0x64c7
#define A9_VPP2_BLD_DIN0_VSCOPE                       0x64c8
#define A9_VPP2_BLD_DIN1_HSCOPE                       0x64c9
#define A9_VPP2_BLD_DIN1_VSCOPE                       0x64ca
#define A9_VPP2_BLD_DIN2_HSCOPE                       0x64cb
#define A9_VPP2_BLD_DIN2_VSCOPE                       0x64cc
#define A9_VPP2_BLEND_CTRL                            0x64e8
#define A9_VPP2_BLEND_BLEND_DUMMY_DATA                0x64e9
#define A9_VPP2_BLEND_DUMMY_ALPHA                     0x64ea

/* aisr */
#define AISR_RESHAP_CTRL0                          0x2e40
#define AISR_RESHAP_CTRL1                          0x2e41
#define AISR_RESHAP_CTRL2                          0x2e42
#define AISR_RESHAP_SCOPE_X                        0x2e43
#define AISR_RESHAP_SCOPE_Y                        0x2e44
#define AISR_RESHAP_BADDR00                        0x2e45
#define AISR_RESHAP_BADDR01                        0x2e46
#define AISR_RESHAP_BADDR02                        0x2e47
#define AISR_RESHAP_BADDR03                        0x2e48
#define AISR_RESHAP_BADDR10                        0x2e49
#define AISR_RESHAP_BADDR11                        0x2e4a
#define AISR_RESHAP_BADDR12                        0x2e4b
#define AISR_RESHAP_BADDR13                        0x2e4c
#define AISR_RESHAP_BADDR20                        0x2e4d
#define AISR_RESHAP_BADDR21                        0x2e4e
#define AISR_RESHAP_BADDR22                        0x2e4f
#define AISR_RESHAP_BADDR23                        0x2e50
#define AISR_RESHAP_BADDR30                        0x2e51
#define AISR_RESHAP_BADDR31                        0x2e52
#define AISR_RESHAP_BADDR32                        0x2e53
#define AISR_RESHAP_BADDR33                        0x2e54
#define AISR_RESHAP_MISC                           0x2e55
#define AISR_POST_CTRL                             0x2e5a
#define AISR_POST_SIZE                             0x2e5b

#define DEMO_MODE_WINDO_CTRL0                      0x5380
#define DEMO_MODE_WINDO_CTRL1                      0x5381

#define VPP_SR0_IN_SIZE                            0x1d97
#define VPP_SR1_IN_SIZE                            0x1d98

#define VIU_VD1_PATH_CTRL                          0x1a73
#define VIU_VD2_PATH_CTRL                          0x1a74
#define VIU_VD3_PATH_CTRL                          0x1a75
#define VIU_OSD1_PATH_CTRL                         0x1a76
#define VIU_OSD2_PATH_CTRL                         0x1a77
#define VIU_OSD3_PATH_CTRL                         0x1a78
#define VIU_OSD4_PATH_CTRL                         0x1a79

#define OSD1_HDR2_CLK_GATE                         0x38a1
#define OSD2_HDR2_CLK_GATE                         0x5b51
#define OSD2_HDR2_CLK_GATE_T5M                     0x5b01
#define VD1_HDR2_CLK_GATE                          0x3801
#define VD1_HDR2_CLK_GATE_T6D                      0x6201
#define VD2_HDR2_CLK_GATE                          0x3851
#define VD2_HDR2_CLK_GATE_T6D                      0x6281
#define VD3_HDR2_CLK_GATE                          0x5931
#define DOLBY_HDR2_CLK_GATE                        0x4c01

#define VPU_AXI_CACHE                              0x2733

/* noc_vpu */
#define NOC_VPU_QOS_R_OFFSET_FRC0      0x100
#define NOC_VPU_QOS_W_OFFSET_FRC0      0x300
#define NOC_VPU_QOS_R_OFFSET_FRC1      0x500
#define NOC_VPU_QOS_W_OFFSET_FRC1      0x700
#define NOC_VPU_QOS_R_OFFSET_FRC2      0x900
#define NOC_VPU_QOS_R_OFFSET_VPU0      0xb00
#define NOC_VPU_QOS_W_OFFSET_VPU0      0xd00
#define NOC_VPU_QOS_R_OFFSET_VPU1      0x1000
#define NOC_VPU_QOS_W_OFFSET_VPU1      0x1200
#define NOC_VPU_QOS_R_OFFSET_VPU2      0x1400

/* vpu Qos */
#define VPU_RDARB_UGT_L2C1             0x27c2
/* VPU_WRARB_UGT_L2C1 0x27c3 bit[1:0]vdin urgent, bit[9:8]di urgent */
#define VPU_WRARB_UGT_L2C1             0x27c3
/* vpu set 2R1W ,VPU_INTF_CTRL 0x270a bit22 =1 */
#define VPU_INTF_CTRL                  0x270a
//===========================================================================
// -----------------------------------------------
// REG_BASE:  DPSS_DPE_INTF_TOP_VCBUS_BASE = 0x6d
// -----------------------------------------------
//===========================================================================
//
// Reading file:  ././vpu_axird_intf_top_reg.h
//
// synopsys translate_off
// synopsys translate_on
#define VPU_AXIRD_MISC_CTRL                        0x6d00
//Bit 31:16         reserved
//Bit 15:10        reg_nrdv_rst_num              // unsigned ,   RW,  default = 4
//Bit  9:4         reg_vd1_dv_rst_num            // unsigned ,   RW,  default = 4
//Bit  3:1         reg_nr_vfcd_slc_num           // unsigned ,   RW,  default = 4
//Bit  0           reg_vfcdxn_path_force         // unsigned ,   RW,  default = 0
#define VPU_AXIRD_TOP_CTRL                         0x6d01
//Bit 31:16       reg_gclk_ctrl                 // unsigned ,   RW,  default = 0
//Bit 15:0        reg_secure_dat                // unsigned ,   RW,  default = 0
#define VPU_AXIRD_PATH_CTRL                        0x6d02
//Bit 31:10       reserved
//Bit 9:8         reg_dv_path_mode              // unsigned ,   RW,  default = 0,0:off,1:nr_dv,2:vd1_dv
//Bit 7:3         reserved
//Bit 2           reg_di_vpp_link               // unsigned ,   RW,  default = 0
//Bit 1           reg_nr_vpp_link               // unsigned ,   RW,  default = 0
//Bit 0           reg_vd1_vpp_link              // unsigned ,   RW,  default = 1
#define VPU_AXIRD_FG_CTRL                          0x6d03
//Bit 31:18      reserved
//Bit 17:14       reg_fg_ldma_trig_mux          // unsigned ,   RW,  default = 0
//Bit 13:12       pls_fg_ldma_trig              // unsigned ,   W1T,  default = 0
//Bit 11: 8       reg_fg_src_idx_1              // unsigned ,   RW,  default = 4'hf
//Bit  7: 5       reserved
//Bit     4       reg_irq_flg_sel               // unsigned ,   RW,  default = 0
//Bit  3: 0       reg_fg_src_idx_0              // unsigned ,   RW,  default = 4'hf
#define VPU_AXIRD_IRQ_EN0                          0x6d04
//Bit 31: 0       reg_irq_en0                   // unsigned ,   RW,  default = 0
#define VPU_AXIRD_IRQ_CLR                          0x6d05
//Bit 31: 0       pls_irq_clr0                  // unsigned ,   W1T,  default = 0
#define VPU_AXIRD_IRQ_EN1                          0x6d06
//Bit 31:16       pls_irq_clr1                  // unsigned ,   W1T,  default = 0
//Bit 15: 0       reg_irq_en1                   // unsigned ,   RW,  default = 0
#define VPU_AXIRD_NR_HOLD_CTRL                     0x6d07
//Bit 31:29       reserved
//Bit 28          reg_nr_pre_hold_en
//Bit 27:22       reg_nr_pre_hold_num
//Bit 21:16       reg_nr_pre_pass_num
//Bit 15:13       reserved
//Bit 12          reg_nr_cur_hold_en
//Bit 11:6        reg_nr_cur_hold_num
//Bit 5:0         reg_nr_cur_pass_num
#define VPU_AXIRD_LINE_CRTL0                       0x6d0a
//Bit 31:28       reg_line_sync_en               // unsigned ,   RW,  default = 0
//Bit 27:24       reg_rd_dly_en                  // unsigned ,   RW,  default = 0
//Bit 23:16       reg_line_clr_reg               // unsigned ,   RW,  default = 0
//Bit 15:8        reg_line_end_ds                // unsigned ,   RW,  default = 0
//Bit 7:4         reg_rd_idx_num                 // unsigned ,   RW,  default =15
//Bit 3           reg_vd1_sync_sel               // unsigned ,   RW,  default = 0
//Bit 2:1         reserved
//Bit 0           pls_rd_idx_int_en              // unsigned ,   W1T, default = 0
#define VPU_AXIRD_LINE_CRTL1                       0x6d0b
//Bit 31:0        reg_line_ini0                   // unsigned ,   RW,  default =0
#define VPU_AXIRD_LINE_CRTL2                       0x6d0c
//Bit 31:8       reserved
//Bit 7:0        reg_rd_idx_int                  // unsigned ,   RW,  default =0
#define VPU_AXIRD_LINE_CRTL3                       0x6d0d
//Bit 31:0        reg_rd_dly_line0                // unsigned ,   RW,  default =0
#define VPU_AXIRD_LINE_CRTL4                       0x6d0e
//Bit 31:0        reg_rd_dly_line1                // unsigned ,   RW,  default =0
#define VPU_AXIRD_LINE_CRTL5                       0x6d0f
//Bit 31:0        reg_line_ini1                  // unsigned ,   RW,  default =0
#define VPU_AXIRD_OUT_REMAP                        0x6d10
//Bit 31:0        reg_vfcdxn_path_sel           // unsigned ,   RW,  default = 32'h64ff3210
#define VPU_AXIRD_BC_REMAP0                        0x6d11
//Bit 31:28       reg_bc_core_idx7              // unsigned ,   RW,  default = 15
//Bit 27:24       reg_bc_core_idx6              // unsigned ,   RW,  default = 5
//Bit 23:20       reg_bc_core_idx5              // unsigned ,   RW,  default = 15
//Bit 19:16       reg_bc_core_idx4              // unsigned ,   RW,  default = 15
//Bit 15:12       reg_bc_core_idx3              // unsigned ,   RW,  default = 4
//Bit 11: 8       reg_bc_core_idx2              // unsigned ,   RW,  default = 6
//Bit  7: 4       reg_bc_core_idx1              // unsigned ,   RW,  default = 1
//Bit  3: 0       reg_bc_core_idx0              // unsigned ,   RW,  default = 0
#define VPU_AXIRD_BC_REMAP1                        0x6d12
//Bit 31: 4       reserved
//Bit  3: 0       reg_bc_core_idx8              // unsigned ,   RW,  default = 15
#define VPU_AXIRD_RC_REMAP0                        0x6d13
//Bit 31:24       reserved
//Bit 23:20       reg_rc_core_idx5              // unsigned ,   RW,  default = 15
//Bit 19:16       reg_rc_core_idx4              // unsigned ,   RW,  default = 5
//Bit 15:12       reg_rc_core_idx3              // unsigned ,   RW,  default = 4
//Bit 11: 8       reg_rc_core_idx2              // unsigned ,   RW,  default = 6
//Bit  7: 4       reg_rc_core_idx1              // unsigned ,   RW,  default = 1
//Bit  3: 0       reg_rc_core_idx0              // unsigned ,   RW,  default = 0
#define VPU_AXIRD_IRQ_RO_0                         0x6d17
//Bit 31: 0       ro_irq_status0                // unsigned ,   RW,  default = 0
#define VPU_AXIRD_IRQ_RO_1                         0x6d18
//Bit 31:16       reserved
//Bit 15: 0       ro_irq_status1                // unsigned ,   RW,  default = 0
#define VPU_AXIRD_SECURE_EN                        0x6d19
//Bit  31: 13     reserved
//Bit  12         reg_dv_para_mode             // unsigned ,   RW,  default = 0
//Bit  11: 9      reserved
//Bit  8          reg_lcevc_en                  // unsigned ,   RW,  default = 0
//Bit  7          reserved
//Bit  6          reg_mmu_secure_en             // unsigned ,   RW,  default = 0
//Bit  5: 4       reg_mcx2_secure_en            // unsigned ,   RW,  default = 0
//Bit  3: 0       reg_nrx4_secure_en            // unsigned ,   RW,  default = 0
#define VPU_AXIRD_VFCDXN_RST                       0x6d1a
//Bit  31:20     reg_intf_sw_rst               // unsigned ,   RW,  default = 0
//Bit  19:18     reserved
//Bit  17:16     reg_fgrain_sw_rst             // unsigned ,   RW,  default = 0
//Bit  15:12     reserved
//Bit  11:0      reg_vfcdxn_sw_rst             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_TUNNEL_CTRL0                     0x6d1b
//Bit  31:0      reg_tunnel_ctrl0             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_TUNNEL_CTRL1                     0x6d1c
//Bit  31:0      reg_tunnel_ctrl1             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_LLM_RO                           0x6d1d
//Bit  31:0      ro_llm_idx                   // unsigned ,   RO,  default = 0
#define VPU_AXIRD_PROB_SIZE                        0x6d20
//Bit  31:29     reserved
//Bit  28:16     reg_prob_hsize             // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_prob_vsize             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_PROB_POS                         0x6d21
//Bit  31:29     reserved
//Bit  28:16     reg_prob_xpos             // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_prob_ypos             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_PROB_CTRL                        0x6d22
//Bit  31:10  reserved
//Bit  9:4    reg_prob_sel             // unsigned ,   RW,  default = 0
//Bit  3:2    reserved
//Bit  1      reg_prob_clr             // unsigned ,   RW,  default = 0
//Bit  0      reg_prob_en              // unsigned ,   RW,  default = 0
#define VPU_AXIRD_PROB_DAT                         0x6d25
//Bit  31:0      ro_prob_dat0             // unsigned ,   RO
#define VPU_AXIRD_CRC_CTRL                         0x6d28
//Bit  31:20     reserved
//Bit  19:16     reg_crc_sel                   // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_crc_frm_vsize             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_CRC_HSIZE0                       0x6d29
//Bit  31:29     reserved
//Bit  28:16     reg_crc_slc_hsize1             // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_crc_slc_hsize0             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_CRC_HSIZE1                       0x6d2a
//Bit  31:29     reserved
//Bit  28:16     reg_crc_slc_hsize3             // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_crc_slc_hsize2             // unsigned ,   RW,  default = 0
#define VPU_AXIRD_CRC_SLC0_POS                     0x6d2b
//Bit  31:29     reserved
//Bit  28:16     reg_crc_slc0_hbgn            // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_crc_slc0_hend            // unsigned ,   RW,  default = 0
#define VPU_AXIRD_CRC_SLC1_POS                     0x6d2c
//Bit  31:29     reserved
//Bit  28:16     reg_crc_slc1_hbgn            // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_crc_slc1_hend            // unsigned ,   RW,  default = 0
#define VPU_AXIRD_CRC_SLC2_POS                     0x6d2d
//Bit  31:29     reserved
//Bit  28:16     reg_crc_slc2_hbgn            // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_crc_slc2_hend            // unsigned ,   RW,  default = 0
#define VPU_AXIRD_CRC_SLC3_POS                     0x6d2e
//Bit  31:29     reserved
//Bit  28:16     reg_crc_slc3_hbgn            // unsigned ,   RW,  default = 0
//Bit  15:13     reserved
//Bit  12:0      reg_crc_slc3_hend            // unsigned ,   RW,  default = 0
#define VPU_AXIRD_CRC_DAT                          0x6d2f
//Bit  31:0      ro_crc_result             // unsigned ,   RO
#define VPU_AXIRD_LLM_DAT0                         0x6d38
//Bit  31:0      ro_line_sync_0             // unsigned ,   RO
#define VPU_AXIRD_LLM_DAT1                         0x6d39
//Bit  31:0      ro_line_sync_1             // unsigned ,   RO
#define VPU_AXIRD_LLM_DAT2                         0x6d3a
//Bit  31:0      ro_line_sync_2             // unsigned ,   RO
#define VPU_AXIRD_LLM_DAT3                         0x6d3b
//Bit  31:0      ro_line_sync_3             // unsigned ,   RO
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ././vpu_axird_intf_top_reg.h
//===========================================================================
// -----------------------------------------------
// REG_BASE:  DPSS_DPE_VFCD0_VCBUS_BASE = 0x65
// -----------------------------------------------
//===========================================================================
//
// Reading file:  ././ip_inc/mif/viu_pix_rdmif_reg.h
//
// synopsys translate_off
// synopsys translate_on
#define RMIF_TOP_CTRL                              0x6500
//Bit 31    reg_luma_only     // unsigned , RW ,default = 0
//Bit 30    reserved
//Bit 29    reg_last_line_end // unsigned , RW ,default = 1
//Bit 28    reg_last_line_mode// unsigned , RW ,default = 0
//Bit 27:15 reg_luma_last_line_cnt // unsigned , RW ,default = 0
//Bit 14    reg_line_hs_mode   // unsigned , RW ,default = 0
//Bit 13    reg_hz_avg        // unsigned , RW ,default = 0
//Bit 12    reg_rgba_en       // unsigned , RW ,default = 0
//Bit 11    reg_uv_swap       // unsigned , RW ,default = 0
//Bit 10    reg_acc_mode      // unsigned , RW ,default = 1
//Bit 9:8   reg_bits_mode     // unsigned , RW, default = 0 , 0:8 1:10in10 2:10in12 3:10in16
//Bit 7:6   reg_fmt_mode      // unsigned , RW, default = 0 , 0:444 1:422 2:420
//Bit 5:4   reg_plane_mode    // unsigned , RW, default = 0 , 0:1plane 1:2plane 2: 3plane
//Bit 3     reg_swap_64bit    // unsigned , RW, default = 0, 64bits of 128bit swap enable
//Bit 2     reg_little_endian // unsigned , RW, default = 1, 1: little endian 0:big endian enable
//Bit 1     reg_y_rev         // unsigned , RW, default = 0, vertical reverse enable
//Bit 0     reg_x_rev         // unsigned , RW, default = 0, horizontal reverse enable
#define MIF0_RMIF_CTRL1                            0x6501
//Bit 31    reserved
//Bit 30    reg_field_sel          // unsigned , RW, default = 0
//Bit 29    reg_field_rev          // unsigned , RW, default = 0
//Bit 28:27 reg_mode_sel           // unsigned , RW ,default = 0
//Bit 26:25 reg_latch_sel          // unsigned , RW, default = 3
//Bit 24    reg_cav_rst            // unsigned , RW, default = 0
//Bit 23    reg_arb_rst            // unsigned , RW, default = 0
//Bit 22    reg_mif0_32b_align     // unsigned , RW, default = 0, 1: stride32aligned
//Bit 21:20 reg_mif0_block_mode    // unsigned , RW, default = 0, 0: linear 1:32x32burst2  2:64x32 burst4
//Bit 19    reg_mif0_en            // unsigned , RW, default = 0,
//Bit 18:17 reg_mif0_sync_sel      // unsigned , RW, default = 0, axi canvas id sync with frm rst
//Bit 16:9  reg_mif0_canvas_id     // unsigned , RW, default = 0, axi canvas id num
//Bit 8:6   reg_mif0_cmd_intr_len  // unsigned , RW, default = 1, interrupt send cmd when how many series axi cmd,// 0=12 1=16 2=24 3=32 4=40 5=48 6=56 7=64
//Bit 5:3   reg_mif0_cmd_req_size  // unsigned , RW, default = 1, how many room fifo have, then axi send series req, 0=16 1=32 2=24 3=64
//Bit 2:0   reg_mif0_burst_len     // unsigned , RW, default = 2,
#define MIF0_RMIF_CTRL2                            0x6502
//Bit 31:30 reg_mif0_sw_rst        // unsigned , RW, default = 0,
//Bit 29:28 reserved
//Bit 27:26 reg_mif0_gclk_ctrl1    // unsigned , RW, default = 0,
//Bit 25:24 reg_f0_field_mode_y    // unsigned , RW, default = 0,
//Bit 23:22 reg_f0_field_mode_c    // unsigned , RW, default = 0,
//Bit 21:20 reg_mif0_int_clr       // unsigned , RW, default = 0
//Bit 19:18 reg_mif0_gclk_ctrl0    // unsigned , RW, default = 0,
//Bit 17    reserved
//Bit 16:0  reg_mif0_urgent_ctrl   // unsigned , RW, default = 0, urgent control reg ://  16  reg_ugt_init  :  urgent initial value//  15  reg_ugt_en    :  urgent enable//  14  reg_ugt_type  :  1= wrmif 0=rdmif// 7:4  reg_ugt_top_th:  urgent top threshold// 3:0  reg_ugt_bot_th:  urgent bottom threshold
#define MIF0_RMIF_CTRL3                            0x6503
//Bit 31    reserved
//Bit 30    reg_mif0_hold_en       // unsigned , RW, default = 0
//Bit 29:24 reg_mif0_pass_num      // unsigned , RW, default = 1
//Bit 23:18 reg_mif0_hold_num      // unsigned , RW, default = 0
//Bit 17    reserved
//Bit 16:13 reg_luma_vstep0       // unsigned , RW, default = 1
//Bit 12:0  reg_mif0_f0_stride     // unsigned , RW, default = 4096,
#define MIF0_RMIF_CTRL4                            0x6504
//Bit 31:0  reg_mif0_f0_baddr      // unsigned , RW, default = 0,
#define MIF0_RMIF_CTRL5                            0x6505
//Bit 31:30  reg_mif0_lath_vld_sel     //unsigned, RW, default=0, latch use 0:frm_start 1:frm_end 2:1'b1 3: 1'b0
//Bit 29:28  reg_mif0_lath_out_sel     //unsigned, RW, default=1, [0]: 0:use latch 1:use reg,  [1]: 0:baddr use baddr_sel 1:baddr use reg
//Bit 27:20  reg_luma_psel_loop        //unsigned, RW, default=0,
//Bit 19     reserved
//Bit 18:17  reg_f1_field_mode_y    // unsigned , RW, default = 0,
//Bit 16:15  reg_f1_field_mode_c    // unsigned , RW, default = 0,
//Bit 14:7   reg_mif0_f1_canvas_id    //unsigned ,RW, default=0
//Bit 6:3    reg_mif0_pix_bits_mode    //unsigned, RW, default=1, 0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b
//Bit 2:0    reg_mif0_out_pack_mode    //unsigned, RW, default=0, 0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x
#define MIF1_RMIF_CTRL1                            0x6506
//Bit 31    reserved
//Bit 30:27 reg_chrm_vstep1        // unsigned , RW, default = 1
//Bit 26:23 reg_chrm_vstep0        // unsigned , RW, default = 1
//Bit 22    reg_mif1_32b_align     // unsigned , RW, default = 0, 1: stride32aligned
//Bit 21:20 reg_mif1_block_mode    // unsigned , RW, default = 0, 0: linear 1:32x32burst2  2:64x32 burst4
//Bit 19    reg_mif1_en            // unsigned , RW, default = 0,
//Bit 18:17 reg_mif1_sync_sel      // unsigned , RW, default = 0, axi canvas id sync with frm rst
//Bit 16:9  reg_mif1_canvas_id     // unsigned , RW, default = 0, axi canvas id num
//Bit 8:6   reg_mif1_cmd_intr_len  // unsigned , RW, default = 1, interrupt send cmd when how many series axi cmd,// 0=12 1=16 2=24 3=32 4=40 5=48 6=56 7=64
//Bit 5:3   reg_mif1_cmd_req_size  // unsigned , RW, default = 1, how many room fifo have, then axi send series req, 0=16 1=32 2=24 3=64
//Bit 2:0   reg_mif1_burst_len     // unsigned , RW, default = 2,
#define MIF1_RMIF_CTRL2                            0x6507
//Bit 31:30 reg_mif1_sw_rst        // unsigned , RW, default = 0,
//Bit 29:28 reserved
//Bit 27:26 reg_mif1_gclk_ctrl1    // unsigned , RW, default = 0,
//Bit 25:22 reg_luma_vstep1        // unsigned , RW, default = 1
//Bit 21:20 reg_mif1_int_clr       // unsigned , RW, default = 0
//Bit 19:18 reg_mif1_gclk_ctrl0    // unsigned , RW, default = 0,
//Bit 17    reserved
//Bit 16:0  reg_mif1_urgent_ctrl   // unsigned , RW, default = 0, urgent control reg ://  16  reg_ugt_init  :  urgent initial value//  15  reg_ugt_en    :  urgent enable//  14  reg_ugt_type  :  1= wrmif 0=rdmif// 7:4  reg_ugt_top_th:  urgent top threshold// 3:0  reg_ugt_bot_th:  urgent bottom threshold
#define MIF1_RMIF_CTRL3                            0x6508
//Bit 31    reserved
//Bit 30    reg_mif1_hold_en       // unsigned , RW, default = 0
//Bit 29:24 reg_mif1_pass_num      // unsigned , RW, default = 1
//Bit 23:18 reg_mif1_hold_num      // unsigned , RW, default = 0
//Bit 17:13 reserved
//Bit 12:0  reg_mif1_f0_stride        // unsigned , RW, default = 4096,
#define MIF1_RMIF_CTRL4                            0x6509
//Bit 31:0  reg_mif1_f0_baddr        // unsigned , RW, default = 0,
#define MIF1_RMIF_CTRL5                            0x650a
//Bit 31:30  reg_mif1_lath_vld_sel     //unsigned, RW, default=0, latch use 0:frm_start 1:frm_end 2:1'b1 3: 1'b0
//Bit 29:28  reg_mif1_lath_out_sel     //unsigned, RW, default=1, [0]: 0:use latch 1:use reg,  [1]: 0:baddr use baddr_sel 1:baddr use reg
//Bit 27:20  reg_mif1_f1_canvas_id    //unsigned ,RW, default=0
//Bit 19:7   reg_chrm_last_line_cnt  // unsigned , RW, default = 0,
//Bit 6:3    reg_mif1_pix_bits_mode    //unsigned, RW, default=1, 0:4b 1:8b 2:16b 3:32b 4:64b 5:128b 6:12b 7:10b 8:14b
//Bit 2:0    reg_mif1_out_pack_mode    //unsigned, RW, default=0, 0:1x 1:2x 2:4x 3:8x 4:16x 5:32x 6:64x
#define MIF2_RMIF_CTRL                             0x650b
//Bit 31:23 reserved
//Bit 22    reg_mif2_32b_align     // unsigned , RW, default = 0, 1: stride32aligned
//Bit 21:20 reg_mif2_block_mode    // unsigned , RW, default = 0, 0: linear 1:32x32burst2  2:64x32 burst4
//Bit 19    reg_mif2_en            // unsigned , RW, default = 0,
//Bit 18:17 reg_mif2_sync_sel      // unsigned , RW, default = 0, axi canvas id sync with frm rst
//Bit 16:9  reg_mif2_canvas_id     // unsigned , RW, default = 0, axi canvas id num
//Bit 8:6   reg_mif2_cmd_intr_len  // unsigned , RW, default = 1, interrupt send cmd when how many series axi cmd,// 0=12 1=16 2=24 3=32 4=40 5=48 6=56 7=64
//Bit 5:3   reg_mif2_cmd_req_size  // unsigned , RW, default = 1, how many room fifo have, then axi send series req, 0=16 1=32 2=24 3=64
//Bit 2:0   reg_mif2_burst_len     // unsigned , RW, default = 2,
#define MIF2_RMIF_BADDR                            0x650c
//Bit 31:0  reg_mif2_f0_baddr        // unsigned , RW, default = 0,
#define MIF2_RMIF_STRIDE                           0x650d
//Bit 31:13 reserved
//Bit 12:0  reg_mif2_f0_stride        // unsigned , RW, default = 4096,
#define LUMA_RMIF_SCOPE_X                          0x650e
//Bit 31:29 reserved
//Bit 28:16 reg_f0_luma_x_end         // unsigned , RW, default = 4095, the canvas hor end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_f0_luma_x_start       // unsigned , RW, default = 0, the canvas hor start pixel position
#define LUMA_RMIF_SCOPE_Y                          0x650f
//Bit 31:29 reserved
//Bit 28:16 reg_f0_luma_y_end         // unsigned , RW, default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_f0_luma_y_start       // unsigned , RW, default = 0, the canvas ver start pixel position
#define LUMA_SLC1_XSIZE                            0x6510
//Bit 31:29   reserved
//Bit 28:16   reg_luma_slc1_x_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_luma_slc1_x_start    //unsigned, RW, default=0
#define LUMA_SLC1_YSIZE                            0x6511
//Bit 31:29   reserved
//Bit 28:16   reg_luma_slc1_y_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_luma_slc1_y_start    //unsigned, RW, default=0
#define LUMA_SLC2_XSIZE                            0x6512
//Bit 31:29   reserved
//Bit 28:16   reg_luma_slc2_x_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_luma_slc2_x_start    //unsigned, RW, default=0
#define LUMA_SLC2_YSIZE                            0x6513
//Bit 31:29   reserved
//Bit 28:16   reg_luma_slc2_y_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_luma_slc2_y_start    //unsigned, RW, default=0
#define LUMA_SLC3_XSIZE                            0x6514
//Bit 31:29   reserved
//Bit 28:16   reg_luma_slc3_x_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_luma_slc3_x_start    //unsigned, RW, default=0
#define LUMA_SLC3_YSIZE                            0x6515
//Bit 31:29   reserved
//Bit 28:16   reg_luma_slc3_y_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_luma_slc3_y_start    //unsigned, RW, default=0
#define CHRM_RMIF_SCOPE_X                          0x6516
//Bit 31:29 reserved
//Bit 28:16 reg_f0_chrm_x_end         // unsigned , RW, default = 4095, the canvas hor end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_f0_chrm_x_start       // unsigned , RW, default = 0, the canvas hor start pixel position
#define CHRM_RMIF_SCOPE_Y                          0x6517
//Bit 31:29 reserved
//Bit 28:16 reg_f0_chrm_y_end         // unsigned , RW, default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_f0_chrm_y_start       // unsigned , RW, default = 0, the canvas ver start pixel position
#define CHRM_SLC1_XSIZE                            0x6518
//Bit 31:29   reserved
//Bit 28:16   reg_chrm_slc1_x_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_chrm_slc1_x_start    //unsigned, RW, default=0
#define CHRM_SLC1_YSIZE                            0x6519
//Bit 31:29   reserved
//Bit 28:16   reg_chrm_slc1_y_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_chrm_slc1_y_start    //unsigned, RW, default=0
#define CHRM_SLC2_XSIZE                            0x651a
//Bit 31:29   reserved
//Bit 28:16   reg_chrm_slc2_x_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_chrm_slc2_x_start    //unsigned, RW, default=0
#define CHRM_SLC2_YSIZE                            0x651b
//Bit 31:29   reserved
//Bit 28:16   reg_chrm_slc2_y_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_chrm_slc2_y_start    //unsigned, RW, default=0
#define CHRM_SLC3_XSIZE                            0x651c
//Bit 31:29   reserved
//Bit 28:16   reg_chrm_slc3_x_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_chrm_slc3_x_start    //unsigned, RW, default=0
#define CHRM_SLC3_YSIZE                            0x651d
//Bit 31:29   reserved
//Bit 28:16   reg_chrm_slc3_y_end      //unsigned, RW, default=0
//Bit 15:13   reserved
//Bit 12:0    reg_chrm_slc3_y_start    //unsigned, RW, default=0
#define RMIF_RO_STAT                               0x651e
//Bit 31    reserved
//Bit 30:0  ro_status        // unsigned , RO, default = 0 ,
#define RMIF_DUMMY_PIXEL                           0x6522
//Bit 31:0     reg_dummy_pixel_val                   //unsigned, RW, default = 8421376
#define RMIF_RPT_LOOP                              0x6523
//Bit 31:24     reg_f1_chrm_rpt_loop                     //unsigned, RW, default =0
//Bit 23:16     reg_f1_luma_rpt_loop                     //unsigned, RW, default =0
//Bit 15:8      reg_f0_chrm_rpt_loop                     //unsigned, RW, default =0
//Bit  7:0      reg_f0_luma_rpt_loop                     //unsigned, RW, default =0
#define RMIF_F0_LUMA_RPT_PAT                       0x6524
//Bit 31:0     reg_f0_luma_rpt_pat                      //unsigned, RW, default = 0
#define RMIF_F0_CHRM_RPT_PAT                       0x6525
//Bit 31:0     reg_f0_chrm_rpt_pat                    //unsigned, RW, default = 0
#define RMIF_F1_LUMA_RPT_PAT                       0x6526
//Bit 31:0     reg_f1_luma_rpt_pat                      //unsigned, RW, default = 0
#define RMIF_F1_CHRM_RPT_PAT                       0x6527
//Bit 31:0     reg_f1_chrm_rpt_pat                    //unsigned, RW, default = 0
#define RMIF_F1_LUMA_YSIZE                         0x6528
//Bit 31:29 reserved
//Bit 28:16 reg_f1_luma_y_end         // unsigned , RW, default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_f1_luma_y_start       // unsigned , RW, default = 0, the canvas ver start pixel position
#define RMIF_F1_CHRM_YSIZE                         0x6529
//Bit 31:29 reserved
//Bit 28:16 reg_f1_chrm_y_end         // unsigned , RW, default = 0, the canvas ver end pixel position
//Bit 15:13 reserved
//Bit 12: 0 reg_f1_chrm_y_start       // unsigned , RW, default = 0, the canvas ver start pixel position
#define RMIF_F1_MIF0_BADDR                         0x652a
//Bit 31:0  reg_mif0_f1_baddr        // unsigned , RW, default = 0,
#define RMIF_F1_MIF1_BADDR                         0x652b
//Bit 31:0  reg_mif1_f1_baddr        // unsigned , RW, default = 0,
#define RMIF_F1_MIF2_BADDR                         0x652c
//Bit 31:0  reg_mif2_f1_baddr        // unsigned , RW, default = 0,
#define RMIF_F1_STRIDE0                            0x652d
//Bit 31 reserved
//Bit 30:29 reg_chrm_psel_mode        // unsigned , RW, default = 0,
//Bit 28:16 reg_mif1_f1_stride        // unsigned , RW, default = 0,
//Bit 15 reserved
//Bit 14:13 reg_luma_psel_mode        // unsigned , RW, default = 0,
//Bit 12: 0 reg_mif0_f1_stride       // unsigned , RW, default = 0,
#define RMIF_F1_STRIDE1                            0x652e
//Bit 31:16 reg_luma_psel_pat             // unsigned , RW, default = 0,
//Bit 15:13 reserved
//Bit 12: 0 reg_mif2_f1_stride       // unsigned , RW, default = 0,
#define RMIF_FIFO_SIZE                             0x652f
//Bit 31:29 reserved
//Bit 28:16 reg_chrm_fifo_size         // unsigned , RW, default = 32,
//Bit 15:13 reserved
//Bit 12: 0 reg_luma_fifo_size         // unsigned , RW, default = 64,
#define RMIF_CHRM_PSEL                             0x6530
//Bit 31:16 reg_chrm_psel_pat          // unsigned , RW, default = 0,
//Bit 15:8  reg_mif2_f1_canvas_id      // unsigned , RW ,default = 0,
//Bit 7: 0  reg_chrm_psel_loop         // unsigned , RW, default = 0,
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ././ip_inc/mif/viu_pix_rdmif_reg.h
//
//
// Reading file:  ././ip_inc/vfcd/vfcd_dec_regs.h
//
#define VFCD_AFBC_ENABLE                           0x6540
//Bit   31:31     reserved
//Bit   30:29     reg_head_comb_en       //unsigned, RW, default = 1, 0:dis 1:combx2 2:combx4 3:combx8
//Bit   28:23     reg_gclk_ctrl_core     //unsigned, RW, default = 0,
//Bit   22:21     reg_addr_link_en       //unsigned, RW, default = 1, 0:dis 1:link4  2:link8  3:link16
//Bit   20        reg_fmt444_comb        //unsigned, RW, default = 0, 0: 444 8bit uncomb
//Bit   19        reg_dos_uncomp_mode    //unsigned, RW, default = 0
//Bit   18:14     reserved
//Bit   13:12     reg_ddr_blk_size       //unsigned, RW, default = 1
//Bit   11:9      reg_cmd_blk_size       //unsigned, RW, default = 3
//Bit   8         reg_dec_enable         //unsigned, RW, default = 0
//Bit   7:1       reserved
//Bit   0         reg_latch_mux          //unsigned, RW, default = 0 ,0:frm_en_latch size+addr 1:no_lath
#define VFCD_AFBC_MODE                             0x6541
//Bit   31        reg_core_afrc_mode    //unsigned, RW, default = 0 , 0 : afbc 1 : afrc;
//Bit   30        reg_afbc_emit_mode    //unsigned, RW, default = 1 , 0 : core_out 16DW 1 : core_out 64 DW
//Bit   29        reg_ddr_sz_mode       //unsigned, RW, default = 0 , 0: fixed block ddr size 1 : unfixed block ddr size;
//Bit   28        reg_blk_mem_mode      //unsigned, RW, default = 0 , 0: fixed 16x128 size; 1 : fixed 12x128 size
//Bit   27:26     reg_rev_mode          //unsigned, RW, default = 0 , reverse mode
//Bit   25:24     reg_mif_urgent        //unsigned, RW, default = 3 , info mif and data mif urgent
//Bit   23        reg_afbc_bndry_en     //unsigned, RW, default = 0 , afbc_bndry_mode for _afbce
//Bit   22:17     reserved
//Bit   16:14     reg_burst_len         //unsigned, RW, default = 2, 0: burst1 1:burst2 2:burst4 3:burst8 4:burst16
//Bit   13:8      reserved
//Bit   7:6       reg_vert_skip_y       //unsigned, RW, default = 0 , luma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   5:4       reg_horz_skip_y       //unsigned, RW, default = 0 , luma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   3:2       reg_vert_skip_uv      //unsigned, RW, default = 0 , chroma vert skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
//Bit   1:0       reg_horz_skip_uv      //unsigned, RW, default = 0 , chroma horz skip mode : 00-y0y1, 01-y0, 10-y1, 11-(y0+y1)/2
#define VFCD_CONV_BUF_LENS                         0x6542
//Bit   31:28    reserved
//Bit   27:16    reg_tfmt_ram_ofst1      //usigned, RW,  default =512 , t2s_unit = 32 pixel
//Bit   15:12    reserved
//Bit   11: 0    reg_tfmt_ram_ofst0      //usigned, RW,  default =256 , t2s_unit = 32 pixel
#define VFCD_AFBC_DEC_DEF_COLOR                    0x6543
//Bit   31:28    reserved
//Bit   27:16    reg_def_color_u        //usigned, RW,  default = 1023, afbc dec u default setting value
//Bit   15:12    reserved
//Bit   11: 0    reg_def_color_v        //usigned, RW,  default = 1023, afbc dec v default setting value
#define VFCD_AFBC_CONV_CTRL                        0x6544
//Bit   31:28    reserved
//Bit   27:16    reg_def_color_y         //usigned, RW,  default = 1023, afbc dec y default setting value
//Bit   15:14    reserved
//Bit   13:12    reg_fmt_mode            //usigned, RW,  default = 2, 0:yuv444 1:yuv422 2:yuv420
//Bit   11: 0    reg_conv_lbuf_len       //usigned, RW,  default = 128, unit=16 pixel need to set = 2^n
#define VFCD_AFBC_LBUF_DEPTH                       0x6545
//Bit   31:28    reserved
//Bit   27:16    reg_tfmt_ram_ofst2      //usigned, RW,  default = 512; // unit= 32 pixel y_delay_buf for afbc_420_in_fmtup_,yc_sync
//Bit   15:12    reserved
//Bit   11:0     reg_mif_lbuf_depth      //usigned, RW,  default = 128;
#define VFCD_AFBC_HEAD_BADDR                       0x6546
//Bit   31:0    reg_mif_info_baddr      //usigned, RW,  default = 32'h0;
#define VFCD_AFBC_BODY_BADDR                       0x6547
//Bit   31:0    reg_mif_data_baddr      //usigned, RW,  default = 32'h00010000;
#define VFCD_AFRC_CHRM_HEAD_BADDR                  0x6548
//Bit   31:0    reg_afrc1_info_baddr      //usigned, RW,  default = 32'h0;
#define VFCD_AFRC_CHRM_BODY_BADDR                  0x6549
//Bit   31:0    reg_afrc1_data_baddr      //usigned, RW,  default = 32'h00010000;
#define VFCD_FIELD_SKIP_CTRL                       0x654a
//Bit  31:18    reserved
//Bit  17:16    reg_lens_sft_bits     //usigned,  RW,  default = 2'h0
//Bit  15:8     reserved
//Bit   7:6     reg_vskip_mode_1      //usigned,  RW,  default = 2'h1
//Bit   5:4     reg_vskip_mode_0      //usigned,  RW,  default = 2'h1
//Bit   3       pls_fst_frm           //unsigned, W1T, default = 1'h0  pulse
//Bit   2       reg_field_ini         //usigned,  RW,  default = 1'h0
//Bit   1       reg_field_sel         //usigned,  RW,  default = 1'h0
//Bit   0       reg_field_skip_enable //usigned,  RW,  default = 1'h0
#define VFCD_URGENT_CTRL                           0x654b
//Bit  31:0     reg_urgent_ctrl       //usigned,  RW,  default = 32'h0
//======================================================================
//0x52-0x58
////=====================================================================
#define VFCD_AFBC_IQUANT_ENABLE                    0x6552
//Bit 31:12        reserved
//Bit  11          reg_iquant_expand_en_1    //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  10          reg_iquant_expand_en_0    //unsigned,      RW, default = 0  enable for quantization value expansion
//Bit  9: 8        reg_bcleav_ofst           //signed ,       RW, default = 0  bcleave ofset to get lower range, especially under lossy, for v1/v2, x=0 is equivalent, value = -1;
//Bit  7: 5        reserved
//Bit  4           reg_iquant_enable_1       // unsigned ,    RW, default = 0  enable for quant to get some lossy
//Bit  3: 1        reserved
//Bit  0           reg_iquant_enable_0       // unsigned ,    RW, default = 0  enable for quant to get some lossy
#define VFCD_AFBC_IQUANT_LUT_1                     0x6553
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_0_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_0_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_0_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_0_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define VFCD_AFBC_IQUANT_LUT_2                     0x6554
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_0_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_0_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_0_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_0_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define VFCD_AFBC_IQUANT_LUT_3                     0x6555
//Bit 31           reserved
//Bit 30:28        reg_iquant_yclut_1_11     // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 27           reserved
//Bit 26:24        reg_iquant_yclut_1_10     // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 23           reserved
//Bit 22:20        reg_iquant_yclut_1_9      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 19           reserved
//Bit 18:16        reg_iquant_yclut_1_8      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_7      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_6      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_5      // unsigned ,    RW, default = 5  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_4      // unsigned ,    RW, default = 4  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define VFCD_AFBC_IQUANT_LUT_4                     0x6556
//Bit 31:16        reserved
//Bit 15           reserved
//Bit 14:12        reg_iquant_yclut_1_3      // unsigned ,    RW, default = 3  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit 11           reserved
//Bit 10: 8        reg_iquant_yclut_1_2      // unsigned ,    RW, default = 2  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  7           reserved
//Bit  6: 4        reg_iquant_yclut_1_1      // unsigned ,    RW, default = 1  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
//Bit  3           reserved
//Bit  2: 0        reg_iquant_yclut_1_0      // unsigned ,    RW, default = 0  quantization lut for mintree leavs, iquant=2^lut(bc_leav_q+1)
#define VFCD_AFBC_BURST_CTRL                       0x6557
//Bit 31:5     reserved
//Bit 4        reg_ofset_burst4_en              //unsigned ,    RW  default = 0
//Bit 3        reg_burst_length_add_en          //unsigned ,    RW  default = 0
//Bit 2:0      reg_burst_length_add_value       //unsigned ,    RW  default = 2
#define VFCD_AFBC_LOSS_CTRL                        0x6558
//Bit 31:5     reserved
//Bit 4        reg_fix_cr_en                    //unsigned ,    RW  default = 0
//Bit 3:0      reg_quant_diff_root_leave        //unsigned ,    RW  default = 2
#define VFCD_MIF1_URGENT                           0x6560
//Bit 31:28 reserved
//Bit 27:26 reg_bayer_plane_sel   // unsigned, RW, default = 0 0:raw_input_from_y 1:raw_input_from_c
//Bit 25    reg_afrc_raw_en       // unsigned, RW, default = 0
//Bit 24    reg_yc_size_link      // unsigned, RW, default = 1
//Bit 23:17 reserved
//Bit 16:0  reg_afrc1_urgent              //unsigned, RW, default=0
#define VFCD_MIF1_DIMENSION                        0x6561
//Bit 31:23 reserved
//Bit 22:20 reg_afrc1_cmd_size            //unsigned, RW, default=0
//Bit 19:18 reserved
//Bit 17:16 reg_afrc1_ddr_size            //unsigned, RW, default=0
//Bit 15:14 reserved
//Bit 13:12 reg_afrc1_burst_len           //unsigned, RW, default=0
//Bit 11: 0 reserved
#define VFCD_AFRC0_CTRL                            0x6562
//Bit 31:30 reserved
//Bit 29:28 reg_afrc0_partial_decode           //unsigned, RW, default=3
//Bit 27:25 reserved
//Bit 24:20 reg_afrc0_pix_bits                 //unsigned, RW, default=8
//Bit 19:16 reg_afrc0_pix_type                 //unsigned, RW, default=1
//Bit 15:12 reserved
//Bit 11:8  reg_afrc0_cu_bits                  //unsigned, RW, default=0
//Bit 7:5   reserved
//Bit   4   reg_afrc0_header_en                //unsigned, RW, default=0
//Bit 3:1   reserved
//Bit   0   reg_afrc0_mode                     //unsigned, RW, default=0
#define VFCD_AFRC0_PARAM                           0x6563
//Bit 31:9  reserved
//Bit 8:8   reg_afrc_core_bypass               //unsigned, RW, default=0
//Bit 7:0   reg_afrc0_codec_params             //unsigned, RW, default=0
#define VFCD_AFRC1_CTRL                            0x6564
//Bit 31:30 reserved
//Bit 29:28 reg_afrc1_partial_decode           //unsigned, RW, default=3
//Bit 27:25 reserved
//Bit 24:20 reg_afrc1_pix_bits                 //unsigned, RW, default=8
//Bit 19:16 reg_afrc1_pix_type                 //unsigned, RW, default=1
//Bit 15:12 reserved
//Bit 11:8  reg_afrc1_cu_bits                  //unsigned, RW, default=0
//Bit 7:5   reserved
//Bit   4   reg_afrc1_header_en                //unsigned, RW, default=0
//Bit 3:1   reserved
//Bit   0   reg_afrc1_mode                     //unsigned, RW, default=0
#define VFCD_AFRC1_PARAM                           0x6565
//Bit 31:21 reg_head_ram_ofst                  //unsigned ,RW,default = 128
//Bit 20:8  reg_t2s_ram_ofst                   //unsigned ,RW,default = 0
//Bit 7:0   reg_afrc1_codec_params             //unsigned, RW,default = 0
#define VFCD_AFRC_CTRL0                            0x6566
//Bit  31:27       reserved
//Bit  26:25       reg_afrc0_bayer_mode       // unsigned ,    RW, default = 0
//Bit  24          reg_afrc0_pixel_is_diff_chn// unsigned ,    RW, default = 0
//Bit  23:20       reg_afrc0_max_ac_depth     // unsigned ,    RW, default = 9  max ac depth
//Bit  19:17       reserved
//Bit  16          reg_afrc0_last_extend_en   // unsigned ,    RW, default = 0  last extend en
//Bit  15:13       reserved
//Bit  12          reg_afrc0_raw_mode_en      // unsigned ,    RW, default = 1  component raw mode en
//Bit  11:9        reserved
//Bit  8           reg_afrc0_header_mode      // unsigned ,    RW, default = 0  header mode en; must 0
//Bit  7:5         reserved
//Bit  4           reg_afrc0_dict_en          // unsigned ,    RW, default = 0  dictionary mode en
//Bit  3:0         reg_afrc0_core_fmt         // unsigned ,    RW, default = 5  0:R16X4;1:RG;2:RGB;3:RGBA;4:YUV;5:Y_UV
#define VFCD_AFRC_CTRL1                            0x6567
//Bit  31:27       reserved
//Bit  26:25       reg_afrc1_bayer_mode       // unsigned ,    RW, default = 0 //0:mono 1:bayer2x2 2 :bayer4x4
//Bit  24          reg_afrc1_pixel_is_diff_chn// unsigned ,    RW, default = 0 // yuv_uv plane : 0 ; raw_rg_plane : 1 only c_chan need
//Bit  23:20       reg_afrc1_max_ac_depth     // unsigned ,    RW, default = 9  max ac depth
//Bit  19:17       reserved
//Bit  16          reg_afrc1_last_extend_en   // unsigned ,    RW, default = 0  last extend en
//Bit  15:13       reserved
//Bit  12          reg_afrc1_raw_mode_en      // unsigned ,    RW, default = 1  component raw mode en
//Bit  11:9        reserved
//Bit  8           reg_afrc1_header_mode      // unsigned ,    RW, default = 0  header mode en; must 0
//Bit  7:5         reserved
//Bit  4           reg_afrc1_dict_en          // unsigned ,    RW, default = 0  dictionary mode en
//Bit  3:0         reg_afrc1_core_fmt         // unsigned ,    RW, default = 5  0:R16X4;1:RG;2:RGB;3:RGBA;4:YUV;5:Y_UV
#define VFCD_FRM_PIC_SIZE                          0x656a
//Bit  31:29       reserved
//Bit  28:16       reg_frm_hsize_in        //unsigned ,RW,default = 320
//Bit  15:13       reserved
//Bit  12:0        reg_frm_vsize_in        //unsigned ,RW,default = 240
#define VFCD_LUMA_PIC_XPOS                         0x656b
//Bit  31:29       reserved
//Bit  28:16       reg_luma_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_pic_xend       //unsigned ,RW,default = 319
#define VFCD_LUMA_PIC_YPOS                         0x656c
//Bit  31:29       reserved
//Bit  28:16       reg_luma_pic_ybgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_pic_yend       //unsigned ,RW,default = 239
#define VFCD_CHRM_PIC_XPOS                         0x656d
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_pic_xend       //unsigned ,RW,default = 319
#define VFCD_CHRM_PIC_YPOS                         0x656e
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_pic_ybgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_pic_yend       //unsigned ,RW,default = 239
#define VFCD_SLC1_LUMA_PIC_XPOS                    0x656f
//Bit  31:29       reserved
//Bit  28:16       reg_luma_slc1_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_slc1_pic_xend       //unsigned ,RW,default = 319
#define VFCD_SLC1_CHRM_PIC_XPOS                    0x6570
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_slc1_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_slc1_pic_xend       //unsigned ,RW,default = 319
#define VFCD_SLC2_LUMA_PIC_XPOS                    0x6571
//Bit  31:29       reserved
//Bit  28:16       reg_luma_slc2_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_slc2_pic_xend       //unsigned ,RW,default = 319
#define VFCD_SLC2_CHRM_PIC_XPOS                    0x6572
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_slc2_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_slc2_pic_xend       //unsigned ,RW,default = 319
#define VFCD_SLC3_LUMA_PIC_XPOS                    0x6573
//Bit  31:29       reserved
//Bit  28:16       reg_luma_slc3_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_slc3_pic_xend       //unsigned ,RW,default = 319
#define VFCD_SLC3_CHRM_PIC_XPOS                    0x6574
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_slc3_pic_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_slc3_pic_xend       //unsigned ,RW,default = 319
#define VFCD_TOP_CTRL0                             0x6580
//Bit  31:29       reg_soft_rst           //unsigned, RW, default = 4
//Bit  28:27       reg_mode_mux           //unsigned, RW, default = 0 [0]:addr_reg_mode [1] size_reg_mode ,=1,use_inner_reg =0 use_port_input
//Bit  26:24       reg_slc_num            //unsigned, RW , default = 1
//Bit  23:18       reg_afbc_gclk_ctrl     //unsigned, RW , default = 0
//Bit  17:16       reg_afbc_vd_sel        //unsigned, RW , default = 2, 1:afbc_dec 2:nor_rdmif 3:afrc
//Bit  15:4        reg_hold_line_num      //unsigned, RW , default = 4
//Bit   3:2        reg_frm_start_sel      //unsigned, RW , default = 0  0:frm_start_in ; 1: holdline_cac_frm_start 2: reg_frm_start
//Bit   1          pls_frm_end_stat       //unsigned, W1T, default = 0  frame end status
//Bit   0          pls_frm_start          //unsigned, W1T, default = 0  pulse
#define VFCD_TOP_IRQ_CTRL                          0x6581
//Bit 31:17       reserved
//Bit 16          reg_irq_flg_sel        //unsigned, RW, default = 0
//Bit 15:8        pls_irq_clr            //unsigned, W1T, default = 0
//Bit 7:0         reg_irq_en             //unsigned, RW, default = 0
#define VFCD_TOP_CTRL2                             0x6582
//Bit 31          reg_vfcd_en            //unsigned, RW, default = 0
//Bit 30          reg_yc_ctrl_sep        //unsigned, RW, default = 0  //1:for yc_frm_rst/start_sep,only baddr change
//Bit 29:20       reg_body1_ram_ofst     //unsigned, RW, default = 96 //for rdmif 3plane 2k:96 4k:192
//Bit 19:10       reg_body0_ram_ofst     //unsigned, RW, default = 64 //for rdmif 2plane,or afrc _second_plane ram_ofst 2k:64 4k:128
//Bit  9:4        reg_compbits_yuv       //unsigned, RW, default = 42 , //[1:0]:y,[3:2]:u,[5:4]:v,component bitwidth,00-8bit 01-9bit 10-10bit 11-12bit
//Bit  3:1        reg_fg_src_idx         //unsigned, RW, default = 0
//Bit  0          reg_fg_en              //unsigned, RW, default = 0
#define VFCD_TOP_CTRL3                             0x6583
//Bit  31:0       reg_top_ctrl_misc       //unsigned ,RW,default = 0
#define VFCD_AFBC_STAT                             0x6585
//Bit   31:0    ro_dec_info              //usigned, RO, ro_cmd2rd_cnt,ro_abort,frm_end
#define VFCD_MMU_BADDR0                            0x6586
//Bit   31:0    reg_mmu_baddr0           //usigned, RW,default =0 ;
#define VFCD_MMU_BADDR1                            0x6587
//Bit   31:0    reg_mmu_baddr1           //usigned, RW,default =0 ;
#define VFCD_MMU_BADDR2                            0x6588
//Bit   31:0    reg_mmu_baddr2           //usigned, RW,default =0 ;
#define VFCD_MMU_CTRL                              0x6589
//Bit   31:7   reserved
//Bit   6:2    reg_mmu_pre_num           //usigned, RW,default =4 ;
//Bit   1      reg_mmu_pg_mode           //usigned, RW,default =0 ;
//Bit   0      reg_mmu_mode_en           //usigned, RW,default =0 ;
#define VFCD_DBG_CORE_0                            0x658b
//Bit   31:0    ro_core_info             //usigned, RO, comb_phs,cnt, core_phs,shift_phs
#define VFCD_DBG_INFO_0                            0x658c
//Bit   31:0    ro_dec_info_0            //usigned, RO, ro_axi_abort,commif_abort,frm_end
#define VFCD_DBG_INFO_1                            0x658d
//Bit   31:0    ro_dec_info_1            //usigned, RO, t2s_abort,ro_cmd2rd_cnt,t2s_en,cbuf_room_level_x2
#define VFCD_DBG_INFO_2                            0x658e
//Bit   31:0    ro_dec_info_2            //usigned, RO, mif_abort,ro_cmd2rd_cnt,dbuf_room_level  y_plane
#define VFCD_DBG_INFO_3                            0x658f
//Bit   31:0    ro_dec_info_3            //usigned, RO, mif_abort,ro_cmd2rd_cnt,dbuf_room_level   c_plane
#define VFCD_POST_LUMA_SLC1_WIN_H                  0x65e0
//Bit  31:29       reserved
//Bit  28:16       reg_luma_slc1_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_slc1_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_CHRM_SLC1_WIN_H                  0x65e1
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_slc1_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_slc1_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_LUMA_SLC2_WIN_H                  0x65e2
//Bit  31:29       reserved
//Bit  28:16       reg_luma_slc2_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_slc2_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_CHRM_SLC2_WIN_H                  0x65e3
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_slc2_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_slc2_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_LUMA_SLC3_WIN_H                  0x65e4
//Bit  31:29       reserved
//Bit  28:16       reg_luma_slc3_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_slc3_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_CHRM_SLC3_WIN_H                  0x65e5
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_slc3_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_slc3_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_VFMT_SLC1_SIZE                   0x65e6
//Bit  31:29       reserved
//Bit  28:16       reg_slc1_chfmt_w       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_slc1_cvfmt_w       //unsigned ,RW,default = 160
#define VFCD_POST_VFMT_SLC2_SIZE                   0x65e7
//Bit  31:29       reserved
//Bit  28:16       reg_slc2_chfmt_w       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_slc2_cvfmt_w       //unsigned ,RW,default = 160
#define VFCD_POST_VFMT_SLC3_SIZE                   0x65e8
//Bit  31:29       reserved
//Bit  28:16       reg_slc3_chfmt_w       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_slc3_cvfmt_w       //unsigned ,RW,default = 160
#define VFCD_POST_LUMA_SIZE                        0x65ea
//Bit  31:29       reserved
//Bit  28:16       reg_luma_post_win_hsize       //unsigned ,RW,default = 320
//Bit  15:13       reserved
//Bit  12:0        reg_luma_post_win_vsize       //unsigned ,RW,default = 240
#define VFCD_POST_LUMA_SLC1_SIZE                   0x65eb
//Bit  31:29       reserved
//Bit  28:16       reg_luma_post_slc1_hsize       //unsigned ,RW,default = 320
//Bit  15:0        reserved
#define VFCD_POST_LUMA_SLC2_SIZE                   0x65ec
//Bit  31:29       reserved
//Bit  28:16       reg_luma_post_slc2_hsize       //unsigned ,RW,default = 320
//Bit  15:0        reserved
#define VFCD_POST_LUMA_SLC3_SIZE                   0x65ed
//Bit  31:29       reserved
//Bit  28:16       reg_luma_post_slc3_hsize       //unsigned ,RW,default = 320
//Bit  15:0        reserved
#define VFCD_AFBC_VD_CFMT_CTRL                     0x65f0
//Bit 31    reg_cfmt_gclk_bit_dis      //unsigned, RW, default = 0 ; //  it true, disable clock, otherwise enable clock
//Bit 30    reg_cfmt_soft_rst_bit      //unsigned, RW, default = 0 ; //  soft rst bit
//Bit 29    reserved
//Bit 28    reg_chfmt_rpt_pix          //unsigned, RW, default = 0 ; //  if true, horizontal formatter use repeating to generate pixel, otherwise use bilinear interpolation
//Bit 27:24 reg_chfmt_ini_phase        //unsigned, RW, default = 0 ; //  horizontal formatter initial phase
//Bit 23    reg_chfmt_rpt_p0_en        //unsigned, RW, default = 0 ; //  horizontal formatter repeat pixel 0 enable
//Bit 22:21 reg_chfmt_yc_ratio         //unsigned, RW, default = 0 ; //  horizontal Y/C ratio, 00: 1:1, 01: 2:1, 10: 4:1
//Bit 20    reg_chfmt_en               //unsigned, RW, default = 0 ; //  horizontal formatter enable
//Bit 19    reg_cvfmt_phase0_always_en //unsigned, RW, default = 0 ; //if true, always use phase0 while vertical formater, meaning always //repeat data, no interpolation
//Bit 18    reg_cvfmt_rpt_last_dis     //unsigned, RW, default = 0 ; //if true, disable vertical formatter chroma repeat last line
//Bit 17    reg_cvfmt_phase0_nrpt_en   //unsigned, RW, default = 0 ; //vertical formatter don't need repeat line on phase0, 1: enable, 0: disable
//Bit 16    reg_cvfmt_rpt_line0_en     //unsigned, RW, default = 0 ; //vertical formatter repeat line 0 enable
//Bit 15:12 reg_cvfmt_skip_line_num    //unsigned, RW, default = 0 ; //vertical formatter skip line num at the beginning
//Bit 11:8  reg_cvfmt_ini_phase        //unsigned, RW, default = 0 ; //vertical formatter initial phase
//Bit 7:1   reg_cvfmt_phase_step       //unsigned, RW, default = 0 ; //vertical formatter phase step (3.4)
//Bit 0     reg_cvfmt_en               //unsigned, RW, default = 0 ; //vertical formatter enable
#define VFCD_AFBC_VD_CFMT_W                        0x65f1
//Bit 31:29 reserved
//Bit 28:16 reg_chfmt_w                //unsigned, RW, default = 0 ;horizontal formatter width
//Bit 15:13 reserved
//Bit 12:0  reg_cvfmt_w                //unsigned, RW, default = 0 ;vertical formatter width
#define VFCD_AFBC_VD_CFMT_H                        0x65f2
//Bit 31:20     reserved
//Bit 19        reg_fmt_endian_sel     //unsigned, RW, default = 0, 0:fmt_in_act_bits 1:fmt_in_path_data_width
//Bit 18        reg_fmt_size_sw_mode   //unsigned, RW, default = 0, 0:hw mode 1:sw mode for format size
//Bit 17:16     reg_win_bypass_en      //unsigned, RW, default = 0
//Bit 15:14     reserved
//Bit 13        reg_uv_swap            //unsigned, RW, default = 0
//Bit 12:0      reg_cfmt_h             //unsigned, RW, default = 142  ; //vertical formatter height
#define VFCD_POST_LUMA_WIN_H                       0x65f3
//Bit  31:29       reserved
//Bit  28:16       reg_luma_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_LUMA_WIN_V                       0x65f4
//Bit  31:29       reserved
//Bit  28:16       reg_luma_post_ybgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_luma_post_yend       //unsigned ,RW,default = 239
#define VFCD_POST_CHRM_WIN_H                       0x65f5
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_post_xbgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_post_xend       //unsigned ,RW,default = 319
#define VFCD_POST_CHRM_WIN_V                       0x65f6
//Bit  31:29       reserved
//Bit  28:16       reg_chrm_post_ybgn       //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_chrm_post_yend       //unsigned ,RW,default = 239
#define VFCD_LUMA_PAD_SIZE                         0x65f7
//Bit  31          reserved
//Bit  30          reg_pad_mode            //unsigned ,RW,default = 0
//Bit  29          reg_pad_en              //unsigned ,RW,default = 0
//Bit  28:16       reg_pad_luma_vsize      //unsigned ,RW,default = 240
//Bit  15:13       reserved
//Bit  12:0        reg_pad_luma_hsize      //unsigned ,RW,default = 320
#define VFCD_LUMA_PAD_OFST                         0x65f8
//Bit  31:29       reserved
//Bit  28:16       reg_pad_luma_vofst      //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_pad_luma_hofst      //unsigned ,RW,default = 0
#define VFCD_CHRM_PAD_SIZE                         0x65f9
//Bit  31:29       reserved
//Bit  28:16       reg_pad_chrm_vsize      //unsigned ,RW,default = 240
//Bit  15:13       reserved
//Bit  12:0        reg_pad_chrm_hsize      //unsigned ,RW,default = 320
#define VFCD_CHRM_PAD_OFST                         0x65fa
//Bit  31:29       reserved
//Bit  28:16       reg_pad_chrm_vofst      //unsigned ,RW,default = 0
//Bit  15:13       reserved
//Bit  12:0        reg_pad_chrm_hofst      //unsigned ,RW,default = 0
#define VFCD_PAD_DUMY_DATA                         0x65fb
//Bit  31:28       reserved
//Bit  27:16       reg_pad_luma_data       //unsigned ,RW,default = 128
//Bit  15:12       reserved
//Bit  11:0        reg_pad_chrm_data       //unsigned ,RW,default = 128
#define VFCD_POST_RO_0                             0x65fd
//Bit  31:0      ro_dbug_0  //unsigned ,RO , post_win_cut _hcnt/vcnt  act_end
#define VFCD_POST_RO_1                             0x65ff
//Bit  31:0      ro_dbug_1  //unsigned ,RO , post_win_cut _hcnt/vcnt act_end
//
// Closing file:  ././ip_inc/vfcd/vfcd_dec_regs.h
//
//
#define T6W_FGRAIN_CTRL                                0x6c00
//Bit 31:30      reserved
//Bit 29         reg_use_input_size         //unsigned  ,RW, default = 1,1:use input size, 0:use reg size, for slice mode
//Bit 28:26      reg_slice_size_num         //unsigned  ,RW, default = 1
//Bit 25:24      reg_sync_ctrl              //unsigned  ,RW, default = 0
//Bit 23         reserved
//Bit 22         reg_dma_st_clr             //unsigned  ,RW, default = 0,clear DMA error status
//Bit 21         reg_hold4dma_scale         //unsigned  ,RW, default = 0,wait DMA scale data ready before accept input data
//Bit 20         reg_hold4dma_tbl           //unsigned  ,RW, default = 0,wait DMA grain table data ready before accept input data
//Bit 19         reg_cin_uv_swap            //unsigned , RW, default = 0,swap uv input data order.
//Bit 18         reg_cin_rev                //unsigned , RW, default = 0,reverse c order.
//Bit 17         reg_yin_rev                //unsigned , RW, default = 0,reverse y order
//Bit 16         reg_fgrain_ext_imode       //unsigned , RW, default = 1,1:extern data when input data is 8bit and number of it is not 4x
//Bit 15         reg_use_par_apply_fgrain   //unsigned , RW, default = 0,1:use fgrain from dma
//Bit 14         reg_fgrain_last_ln_mode    //unsigned , RW, default = 0,1:last line mode, only used in rdmif mode
//Bit 13         reg_fgrain_use_sat4bp      //unsigned , RW, default = 0,only for debug.
//Bit 12         reg_apply_c_mode           //unsigned , RW, default = 1,chroma scaling from luma
//Bit 11         reg_fgrain_tbl_sign_mode   //unsigned , RW, default = 1,0: fgrain table data is unsigned
//Bit 10         reg_fgrain_tbl_ext_mode    //unsigned , RW, default = 1,1: fgrain table data number is not 4x when widtn is 8bit
//Bit  9: 8      reg_fmt_mode               //unsigned , RW, default = 2, 0:444; 1:422; 2:420; 3:reserved
//Bit  7: 6      reg_comp_bits              //unsigned , RW, default = 1, 0:8bits; 1:10bits, else 12 bits
//Bit  5: 4      reg_rev_mode               //unsigned , RW, default = 0, 0:h_rev; 1:v_rev;
//Bit  3         reserved
//Bit  2         reg_block_mode             //unsigned , RW, default = 1
//Bit  1         reg_fgrain_loc_en          //unsigned , RW, default = 0, used by sync to enable fgrain
//Bit  0         reg_fgrain_glb_en          //unsigned , RW, default = 0
#define T6W_FGRAIN_WIN_H0                              0x6c01
//Bit 31:16     reg_win_end_h0      //unsigned ,RW, default = 3812
//Bit 15: 0     reg_win_bgn_h0      //unsigned ,RW, default = 0
#define T6W_FGRAIN_WIN_H1                              0x6c02
//Bit 31:16     reg_win_end_h1      //unsigned ,RW, default = 3812
//Bit 15: 0     reg_win_bgn_h1      //unsigned ,RW, default = 0
#define T6W_FGRAIN_WIN_H2                              0x6c03
//Bit 31:16     reg_win_end_h2      //unsigned ,RW, default = 3812
//Bit 15: 0     reg_win_bgn_h2      //unsigned ,RW, default = 0
#define T6W_FGRAIN_WIN_H3                              0x6c04
//Bit 31:16     reg_win_end_h3      //unsigned ,RW, default = 3812
//Bit 15: 0     reg_win_bgn_h3      //unsigned ,RW, default = 0
#define T6W_FGRAIN_WIN_V                               0x6c05
//Bit  31:16     reg_win_end_v      //unsigned , RW, default = 2156
//Bit  15: 0     reg_win_bgn_v      //unsigned , RW, default = 0
// Reading file:  ././vpu_axird_intf_arb_reg.h
//
#define VPU_AXIRD_INTF_ARB_MODE                    0x6c20
#define VPU_AXIRD_INTF_ARB_REQEN_SLV               0x6c21
#define VPU_AXIRD_INTF_ARB_WEIGH0_SLV              0x6c22
#define VPU_AXIRD_INTF_ARB_WEIGH1_SLV              0x6c23
#define VPU_AXIRD_INTF_ARB_UGT                     0x6c24
#define VPU_AXIRD_INTF_ARB_LIMT0                   0x6c25
#define VPU_AXIRD_INTF_ARB_STATUS                  0x6c26
#define VPU_AXIRD_INTF_ARB_DBG_CTRL                0x6c27
#define VPU_AXIRD_INTF_ARB_PROT                    0x6c28
#define VPU_AXIRD_INTF_ARB_PROT_STAT               0x6c29
//
// Closing file:  ././vpu_axird_intf_arb_reg.h
//
//axird_arb0 8'h20~8'h3f
//axird_arb1 8'h40~8'h5f
//
// Reading file:  ././ip_inc/vpu_mmu/inc_reg/vpu_mmu_reg.h
//
// synopsys translate_off
// synopsys translate_on
#define VPU_MMU_RDMIF_CTRL                         0x6c60
//Bit 31:26       reserved
//Bit 25:24       reg_gclk_ctrl                   // unsigned ,   RW,  default = 0  2'h3:always open 2'h0:gating clock
//Bit 23:17       reserved
//Bit 16          reg_soft_rst                    // unsigned ,   RW,  default = 0  active high
//Bit 15: 9       reserved
//Bit  8          pls_sw_rst                      // unsigned ,   W1T, default = 0
//Bit  7: 2       reserved
//Bit  1: 0       reg_burst_mode                  // unsigned ,   RW,  default = 2, 0: burst1 1:burst2 2:burst4
#define VPU_MMU_RDMIF_SIZE                         0x6c61
//Bit 31:30       reserved
//Bit 29:28       reg_ddr_blk_size                // unsigned ,   RW,  default = 3
//Bit 27:25       reg_cmd_blk_size                // unsigned ,   RW,  default = 3
//Bit 24:17       reserved
//Bit 16: 0       reg_urgent                      // unsigned ,   RW,  default = 0
#define VPU_MMU_RDMIF_TMOT                         0x6c62
//Bit 31:24       reserved
//Bit 23: 8       reg_time_out_num                // unsigned ,   RW,  default = 1024
//Bit  7: 2       reserved
//Bit  1          pls_time_out_clr                // unsigned ,   W1T, default = 0  clear debug status
//Bit  0          reg_time_out_en                 // unsigned ,   RW,  default = 0
#define VPU_MMU_RDMIF_STA0                         0x6c63
//Bit 31: 0       ro_status0                      // unsigned ,   RO   default = 0
#define VPU_MMU_RDMIF_STA1                         0x6c64
//Bit 31: 0       ro_status1                      // unsigned ,   RO   default = 0
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ././ip_inc/vpu_mmu/inc_reg/vpu_mmu_reg.h
//
//`include "./ip_inc/fgrain/fgrain_regs.h"          //8'ha0-8'hdf src2_fg
//
// Reading file:  ././ip_inc/lcevc/lcevc_pps_regs.h
//
// synopsys translate_off
// synopsys translate_on
#define LCEVC_PPS_CTRL                             0x6cc0
//Bit 31:21        reserved
//Bit 20           reg_aa_ds_ue_clr             // unsigned,   RW, default = 0  clr undone status
//Bit 19:18        reserved
//Bit 17:16        reg_gclk_ctrl                // unsigned,   RW, default = 0  gate clk ctrl
//Bit 15:12        reg_aa_ds_dsx_ofst           // signed ,    RW, default = 0  horizontal pixel offset for the input pixel to downsample filter
//Bit 11:8         reg_aa_ds_dsy_ofst           // signed ,    RW, default = 0  vertical pixel offset for the input pixel to downsample filter
//Bit 7 :6         reg_aa_ds_h_step             // unsigned,   RW, default = 1  downscale mode of x direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
//Bit 5 :4         reg_aa_ds_v_step             // unsigned,   RW, default = 1  downscale mode of y direction for me input data; 0: no downscale; 1:1/2 downscale; 2:1/4 downscale
//Bit 3 :2         reserved
//Bit 1            reg_aa_ds_hdsc_en            // unsigned,   RW, default = 1  downscale of x direction enable
//Bit 0            reg_aa_ds_vdsc_en            // unsigned,   RW, default = 1  downscale of y direction enable
#define LCEVC_PPS_COEF_0                           0x6cc1
//Bit 31:16        reserved
//Bit 15: 8        reg_aa_ds_dsx_coef_0         // signed ,    RW, default = 24  coef of AA filter for horizontal downsampling of blended data, normalized to 128 as 1
//Bit  7: 0        reg_aa_ds_dsy_coef_0         // signed ,    RW, default = 24  coef of AA filter for vertical downsampling of blended data, normalized to 128 as 1
#define LCEVC_PPS_COEF_1                           0x6cc2
//Bit 31:16        reserved
//Bit 15: 8        reg_aa_ds_dsx_coef_1         // signed ,    RW, default = 20  coef of AA filter for horizontal downsampling of blended data, normalized to 128 as 1
//Bit  7: 0        reg_aa_ds_dsy_coef_1         // signed ,    RW, default = 20  coef of AA filter for vertical downsampling of blended data, normalized to 128 as 1
#define LCEVC_PPS_COEF_2                           0x6cc3
//Bit 31:16        reserved
//Bit 15: 8        reg_aa_ds_dsx_coef_2         // signed ,    RW, default = 16  coef of AA filter for horizontal downsampling of blended data, normalized to 128 as 1
//Bit  7: 0        reg_aa_ds_dsy_coef_2         // signed ,    RW, default = 16  coef of AA filter for vertical downsampling of blended data, normalized to 128 as 1
#define LCEVC_PPS_COEF_3                           0x6cc4
//Bit 31:16        reserved
//Bit 15: 8        reg_aa_ds_dsx_coef_3         // signed ,    RW, default = 16  coef of AA filter for horizontal downsampling of blended data, normalized to 128 as 1
//Bit  7: 0        reg_aa_ds_dsy_coef_3         // signed ,    RW, default = 16  coef of AA filter for vertical downsampling of blended data, normalized to 128 as 1
#define LCEVC_PPS_BB_SCP_H                         0x6cc5
//Bit 31:16        reg_aa_ds_bb_hscp0           // unsigned ,  RW, default = 0     ds lft posi, dft0
//Bit 15: 0        reg_aa_ds_bb_hscp1           // unsigned ,  RW, default = 1919  ds rit posi, dft0
#define LCEVC_PPS_BB_SCP_V                         0x6cc6
//Bit 31:16        reg_aa_ds_bb_vscp0           // unsigned ,  RW, default = 0     ds top posi, dft xsize-1
//Bit 15: 0        reg_aa_ds_bb_vscp1           // unsigned ,  RW, default = 2159  ds bot posi, dft ysize-1
#define LCEVC_PPS_RO_DBG                           0x6cc7
//Bit 31:2         reserved
//Bit 1            ro_undone_err1                // unsigned ,  RO, default = 0
//Bit 0            ro_undone_err0                // unsigned ,  RO, default = 0
#define LCEVC_PPS_C42C44_MODE                      0x6cc8
//Bit 31: 8        reserved
//Bit  7           reserved
//Bit  6           reg_422to444_en           // unsigned ,    RW, default = 1
//Bit  5: 4        reg_422to444_mode         // unsigned ,    RW, default = 3
//Bit  3           reserved
//Bit  2           reg_444to422_en           // unsigned ,    RW, default = 1
//Bit  1: 0        reg_444to422_mode         // unsigned ,    RW, default = 0
#define LCEVC_PPS_HSC_START_PHASE_STEP             0x6cc9
//Bit 31:28        reserved
//Bit 27:24        reg_hsc_integer_part        // unsigned ,   RW, default = 1 integer	part	of	step
//Bit 23: 0        reg_hsc_fraction_part       // unsigned ,   RW, default = 0 fraction	part	of	step
#define LCEVC_PPS_VSC_START_PHASE_STEP             0x6cca
//Bit 31:30        reserved
//Bit 29           reg_post_sc_mux_sel         // unsigned ,   RW, default = 1 sel hscaler first
//Bit 28           reg_vf_sep_coef_en          // unsigned ,   RW, default = 0
//Bit 27:24        reg_vsc_integer_part        // unsigned ,   RW, default = 1 integer	part	of	step
//Bit 23: 0        reg_vsc_fraction_part       // unsigned ,   RW, default = 0 fraction	part	of	step
#define LCEVC_PPS_VSC_INIT                         0x6ccb
//Bit 31:29        reserved
//Bit 28:24        reg_vsc_ini_integer         // signed ,     RW, default = 0
//Bit 23: 0        reg_vsc_ini_phase           // unsigned ,   RW, default = 0
#define LCEVC_PPS_COMP                             0x6ccc
//Bit 31:28       reg_hold_num                 // unsigned ,   RW, default = 0
//Bit 27          reg_act_phs_mask             // unsigned ,   RW, default = 0
//Bit 26          reg_hold_phs_mask            // unsigned ,   RW, default = 0
//Bit 25          reg_frm_en_sel               // unsigned ,   RW, default = 0
//Bit 24:23       reg_hds_sel                  // unsigned ,   RW, default = 0
//Bit 22:21       reg_vds_sel                  // unsigned ,   RW, default = 0
//Bit 20          reg_hvds_sel                 // unsigned ,   RW, default = 0
//Bit 19:18       reserved
//Bit 17:14       reg_vsc_nor_rs_bits          // unsigned ,   RW, default = 7
//Bit 13:10       reg_hsc_nor_rs_bits          // unsigned ,   RW, default = 7
//Bit  9:8        reserved
//Bit  7:4        reg_hds_tap_num              // unsigned ,   RW, default = 4
//Bit  3:0        reg_vds_tap_num              // unsigned ,   RW, default = 4
#define LCEVC_PPS_HSC_PHASE_CTRL_0                 0x6ccd
//Bit 31:29       reserved
//Bit 28:24       reg_hsc_ini_integer0         // signed ,     RW, default = 0
//Bit 23:0        reg_hsc_ini_phase0           // unsigned ,   RW, default = 0
#define LCEVC_PPS_HSC_PHASE_CTRL_1                 0x6cce
//Bit 31:29       reserved
//Bit 28:24       reg_hsc_ini_integer1         // signed ,     RW, default = 0
//Bit 23:0        reg_hsc_ini_phase1           // unsigned ,   RW, default = 0
#define LCEVC_PPS_HSC_PHASE_CTRL_2                 0x6ccf
//Bit 31:29       reserved
//Bit 28:24       reg_hsc_ini_integer2         // signed ,     RW, default = 0
//Bit 23:0        reg_hsc_ini_phase2           // unsigned ,   RW, default = 0
#define LCEVC_PPS_HSC_PHASE_CTRL_3                 0x6cd0
//Bit 31:29       reserved
//Bit 28:24       reg_hsc_ini_integer3         // signed ,     RW, default = 0
//Bit 23:0        reg_hsc_ini_phase3           // unsigned ,   RW, default = 0
#define LCEVC_PPS_OUT_HSIZE_1                      0x6cd1
//Bit 31:16        reg_out_hsize_3           // unsigned ,    RW, default = 3840
//Bit 15:0         reg_out_hsize_2           // unsigned ,    RW, default = 3840
#define LCEVC_PPS_OUT_HSIZE_0                      0x6cd2
//Bit 31:16        reg_out_hsize_1           // unsigned ,    RW, default = 3840
//Bit 15:0         reg_out_hsize_0           // unsigned ,    RW, default = 3840
#define LCEVC_PPS_OUT_VSIZE_0                      0x6cd3
//Bit 31:16        reserved
//Bit 15:0         reg_out_vsize             // unsigned ,    RW, default = 2160
#define LCEVC_PPS_COEF_IDX                         0x6cd4
//Bit 31:13        reserved
//Bit 12:10        reg_ctype                 //unsigned, RW, default = 0
//Bit 9            reg_coef_s11_mode         //unsigned, RW, default = 0
//Bit 8            reg_idx_inc               //unsigned, RW, default = 0
//Bit 7            reg_apb_rd_coef_en        //unsigned, RW, default = 0
//Bit 6:0          reg_index                 //unsigned, RW, default = 0
#define LCEVC_PPS_COEF                             0x6cd5
//Bit 31:16        reg_coef_data_1           //unsigned, RW, default = 0
//Bit 15:0         reg_coef_data_0           //unsigned, RW, default = 0
#define LCEVC_PPS_HDS_ADDR                         0x6cde
//Bit 31:30        reserved
//Bit 29:24        reg_hds_coef_addr3        //usigned, RW, default = 0
//Bit 23:22        reserved
//Bit 21:16        reg_hds_coef_addr2        //usigned, RW, default = 32
//Bit 15:14        reserved
//Bit 13:8         reg_hds_coef_addr1        //usigned, RW, default = 16
//Bit 7:6          reserved
//Bit 5:0          reg_hds_coef_addr0        //usigned, RW, default = 0
#define LCEVC_PPS_VDS_ADDR                         0x6cdf
//Bit 31:30        reserved
//Bit 29:24        reg_vds_coef_addr3        //usigned, RW, default = 0
//Bit 23:22        reserved
//Bit 21:16        reg_vds_coef_addr2        //usigned, RW, default = 32
//Bit 15:14        reserved
//Bit 13:8         reg_vds_coef_addr1        //usigned, RW, default = 16
//Bit 7:6          reserved
//Bit 5:0          reg_vds_coef_addr0        //usigned, RW, default = 0
#define LCEVC_PPS_HDS_COEF0                        0x6ce0
//Bit 31:21        reg_hds_coef03            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef02            //signed, RW, default = 0
#define LCEVC_PPS_HDS_COEF1                        0x6ce1
//Bit 31:21        reg_hds_coef01            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef00            //signed, RW, default = 0
#define LCEVC_PPS_HDS_COEF2                        0x6ce2
//Bit 31:21        reg_hds_coef13            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef12            //signed, RW, default = 0
#define LCEVC_PPS_HDS_COEF3                        0x6ce3
//Bit 31:21        reg_hds_coef11            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef10            //signed, RW, default = 0
#define LCEVC_PPS_HDS_COEF4                        0x6ce4
//Bit 31:21        reg_hds_coef23            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef22            //signed, RW, default = 0
#define LCEVC_PPS_HDS_COEF5                        0x6ce5
//Bit 31:21        reg_hds_coef21            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef20            //signed, RW, default = 0
#define LCEVC_PPS_HDS_COEF6                        0x6ce6
//Bit 31:21        reg_hds_coef33            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef32            //signed, RW, default = 0
#define LCEVC_PPS_HDS_COEF7                        0x6ce7
//Bit 31:21        reg_hds_coef31            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_hds_coef30            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF0                        0x6ce8
//Bit 31:21        reg_vds_coef03            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef02            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF1                        0x6ce9
//Bit 31:21        reg_vds_coef01            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef00            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF2                        0x6cea
//Bit 31:21        reg_vds_coef13            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef12            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF3                        0x6ceb
//Bit 31:21        reg_vds_coef11            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef10            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF4                        0x6cec
//Bit 31:21        reg_vds_coef23            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef22            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF5                        0x6ced
//Bit 31:21        reg_vds_coef21            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef20            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF6                        0x6cee
//Bit 31:21        reg_vds_coef33            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef32            //signed, RW, default = 0
#define LCEVC_PPS_VDS_COEF7                        0x6cef
//Bit 31:21        reg_vds_coef31            //signed, RW, default = 0
//Bit 20:11        reserved
//Bit 10:0         reg_vds_coef30            //signed, RW, default = 0
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ././ip_inc/lcevc/lcevc_pps_regs.h
//
//
// Reading file:  ././ip_inc/lcevc/lcevc_misc_regs.h
//
// synopsys translate_off
// synopsys translate_on
#define LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_0      0x6ca0
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_din_hbgn_0           // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_din_hend_0           // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_1      0x6ca1
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_din_hbgn_1           // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_din_hend_1           // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_2      0x6ca2
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_din_hbgn_2           // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_din_hend_2           // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_WIN_SLC_DIN_BEGIN_END_3      0x6ca3
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_din_hbgn_3           // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_din_hend_3           // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_0     0x6ca4
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_dout_hbgn_0          // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_dout_hend_0          // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_1     0x6ca5
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_dout_hbgn_1          // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_dout_hend_1          // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_2     0x6ca6
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_dout_hbgn_2          // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_dout_hend_2          // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_WIN_SLC_DOUT_BEGIN_END_3     0x6ca7
//Bit 31:29        reserved
//Bit 28:16        reg_lcevc_pps_slc_dout_hbgn_3          // unsigned,   RW, default = 0
//Bit 15:13        reserved
//Bit 12:0         reg_lcevc_pps_slc_dout_hend_3          // unsigned,   RW, default = 0
//`define   LCEVC_TOP_BLD_WIN_SLC_DIN_BEGIN_END_0   8'ha8
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_din_hbgn_0           // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_din_hend_0           // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_BLD_WIN_SLC_DIN_BEGIN_END_1   8'ha9
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_din_hbgn_1           // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_din_hend_1           // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_BLD_WIN_SLC_DIN_BEGIN_END_2   8'haa
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_din_hbgn_2           // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_din_hend_2           // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_BLD_WIN_SLC_DIN_BEGIN_END_3   8'hab
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_din_hbgn_3           // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_din_hend_3           // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_BLD_WIN_SLC_DOUT_BEGIN_END_0  8'hac
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_dout_hbgn_0          // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_dout_hend_0          // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_BLD_WIN_SLC_DOUT_BEGIN_END_1  8'had
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_dout_hbgn_1          // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_dout_hend_1          // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_BLD_WIN_SLC_DOUT_BEGIN_END_2  8'hae
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_dout_hbgn_2          // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_dout_hend_2          // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_BLD_WIN_SLC_DOUT_BEGIN_END_3  8'haf
////Bit 31:29        reserved
////Bit 28:16        reg_lcevc_bld_slc_dout_hbgn_3          // unsigned,   RW, default = 0
////Bit 15:13        reserved
////Bit 12:0         reg_lcevc_bld_slc_dout_hend_3          // unsigned,   RW, default = 0
//`define   LCEVC_BLEND_MISC_CTRL                   8'hb0
////Bit 31:28  reserved
////Bit 27:20  hold_lines                     //unsigned ,RW, default = 4
////Bit 19:2   reserved
////Bit 1 :0   gclk_ctrl                      //unsigned ,RW, default = 0
//
//`define   LCEVC_BLEND_DUMMY_DATA                  8'hb1
////Bit 31:24  reserved
////Bit 23:16  blend0_dummy_data_y            //unsigned ,RW, default = 8'h00
////Bit 15:8   blend0_dummy_data_cb           //unsigned ,RW, default = 8'h80
////Bit 7 :0   blend0_dummy_data_cr           //unsigned ,RW, default = 8'h80
//
//`define   LCEVC_BLEND_DUMMY_ALPHA		8'hb2
////Bit 31:9   reserved
////Bit 8 :0   blend0_dummy_alpha             //unsigned ,RW, default = 9'h0
//
//`define   LCEVC_BLEND_RO_CURRENT_XY               8'hb3
////Bit 31:0   ro_blend_current_xy            //unsigned ,RO, default = 32'h0
//
//`define   LCEVC_BLEND_OSD_FLAG_CTRL               8'hb4
////Bit 31:23  reg_osd_flag_thd               //unsigned ,RW, default = 9'h80
////Bit 22:16  reserved
////Bit 15:8   reg_osd_flag_premult_mask      //unsigned ,RW, default = 8'h01
////Bit 7 :5   reserved
////Bit 4 :0   reg_default_osd_flag           //unsigned ,RW, default = 5'h18
//`define   LCEVC_TOP_BLD_HSIZE_BGN_END_0           8'hb5
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_bld_hsize_bgn_0     //unsigned ,RW, default = 13'h0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_bld_hsize_end_0     //unsigned ,RW, default = 13'h3c0
//
//`define   LCEVC_TOP_BLD_HSIZE_BGN_END_1           8'hb6
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_bld_hsize_bgn_1     //unsigned ,RW, default = 13'h3c0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_bld_hsize_end_1     //unsigned ,RW, default = 13'h780
//
//`define   LCEVC_TOP_BLD_HSIZE_BGN_END_2           8'hb7
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_bld_hsize_bgn_2     //unsigned ,RW, default = 13'h780
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_bld_hsize_end_2     //unsigned ,RW, default = 13'hb40
//
//`define   LCEVC_TOP_BLD_HSIZE_BGN_END_3           8'hb8
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_bld_hsize_bgn_3     //unsigned ,RW, default = 13'hb40
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_bld_hsize_end_3     //unsigned ,RW, default = 13'hf00
//`define   LCEVC_TOP_BLD_SIZE                      8'hf0
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_bld_vsize          //unsigned ,RW, default = 13'h438
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_bld_hsize          //unsigned ,RW, default = 13'h780
#define LCEVC_TOP_MISC_CTRL                        0x6cf1
//Bit 31:20  reserved
//Bit 19:12  reg_lcevc_gclk_ctrl              //unsigned ,RW, default = 0
//Bit    11  reserved
//Bit    10  reg_lcevc_bld_en                 //unsigned ,RW, default = 0
//Bit    9   reg_lcevc_din1_premult           //unsigned ,RW, default = 0
//Bit    8   reg_lcevc_din0_premult           //unsigned ,RW, default = 0
//Bit    7   reserved
//Bit  6:4   reg_lcevc_slc_num                //unsigned ,RW, default = 4
//Bit  3:2   reg_lcevc_din1_src_sel           //unsigned ,RW, default = 2
//Bit  1:0   reg_lcevc_din0_src_sel           //unsigned ,RW, default = 1
//`define   LCEVC_TOP_VD1_HSCOPE_0		              8'hf2
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din0_hstart_0              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din0_hend_0                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD1_HSCOPE_1		              8'hf3
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din0_hstart_1              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din0_hend_1                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD1_HSCOPE_2		              8'hf4
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din0_hstart_2              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din0_hend_2                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD1_HSCOPE_3		              8'hf5
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din0_hstart_3              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din0_hend_3                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD1_VSCOPE_0		              8'hf6
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din0_vstart_0              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din0_vend_0                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD2_HSCOPE_0				8'hf7
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din1_hstart_0              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din1_hend_0                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD2_HSCOPE_1				8'hf8
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din1_hstart_1              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din1_hend_1                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD2_HSCOPE_2				8'hf9
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din1_hstart_2              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din1_hend_2                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD2_HSCOPE_3				8'hfa
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din1_hstart_3              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din1_hend_3                // unsigned,   RW, default = 0
//
//`define   LCEVC_TOP_VD2_VSCOPE_0		              8'hfb
////Bit 31:29  reserved
////Bit 28:16  reg_lcevc_din1_vstart_0              // unsigned,   RW, default = 0
////Bit 15:13  reserved
////Bit 12:0   reg_lcevc_din1_hstart_0              // unsigned,   RW, default = 0
#define LCEVC_TOP_BLD_ALPHA                        0x6cfc
//Bit 31:21  reserved
//Bit 20:12  reg_lcevc_din1_alpha                 // unsigned,   RW, default = 0
//Bit 11:9   reserved
//Bit  8:0   reg_lcevc_din0_alpha                 // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_IN_HSIZE0                    0x6cfd
//Bit 31:29  reserved
//Bit 28:16  reg_lcevc_pps_in_hsize_0             // unsigned,   RW, default = 0
//Bit 15:13  reserved
//Bit 12:0   reg_lcevc_pps_in_hsize_1             // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_IN_HSIZE1                    0x6cfe
//Bit 31:29  reserved
//Bit 28:16  reg_lcevc_pps_in_hsize_2             // unsigned,   RW, default = 0
//Bit 15:13  reserved
//Bit 12:0   reg_lcevc_pps_in_hsize_3             // unsigned,   RW, default = 0
#define LCEVC_TOP_PPS_VSIZE0                       0x6cff
//Bit 31:29  reserved
//Bit 28:16  reg_lcevc_pps_in_vsize_0             // unsigned,   RW, default = 0
//Bit 15:13  reserved
//Bit 12:0   reg_lcevc_pps_out_hsize_0            // unsigned,   RW, default = 0
// synopsys translate_off
// synopsys translate_on
//
// Closing file:  ././ip_inc/lcevc/lcevc_misc_regs.h
//
/* add t6x vd1/vd2 scaler regs */
#define VD1_SCALE_COEF_IDX                         0x6100
#define VD1_SCALE_COEF                             0x6101
#define VD1_VSC_REGION12_STARTP                    0x6102
#define VD1_VSC_REGION34_STARTP                    0x6103
#define VD1_VSC_REGION4_ENDP                       0x6104
#define VD1_VSC_START_PHASE_STEP                   0x6105
#define VD1_VSC_REGION0_PHASE_SLOPE                0x6106
#define VD1_VSC_REGION1_PHASE_SLOPE                0x6107
#define VD1_VSC_REGION3_PHASE_SLOPE                0x6108
#define VD1_VSC_REGION4_PHASE_SLOPE                0x6109
#define VD1_VSC_PHASE_CTRL                         0x610a
#define VD1_VSC_INI_PHASE                          0x610b
#define VD1_HSC_REGION12_STARTP                    0x610c
#define VD1_HSC_REGION34_STARTP                    0x610d
#define VD1_HSC_REGION4_ENDP                       0x610e
#define VD1_HSC_START_PHASE_STEP                   0x610f
#define VD1_HSC_REGION0_PHASE_SLOPE                0x6110
#define VD1_HSC_REGION1_PHASE_SLOPE                0x6111
#define VD1_HSC_REGION3_PHASE_SLOPE                0x6112
#define VD1_HSC_REGION4_PHASE_SLOPE                0x6113
#define VD1_HSC_PHASE_CTRL                         0x6114
#define VD1_SC_MISC                                0x6115
#define VD1_SCO_FIFO_CTRL                          0x6116
#define VD1_HSC_PHASE_CTRL1                        0x6117
#define VD1_HSC_INI_PAT_CTRL                       0x6118
#define VD1_SC_GCLK_CTRL                           0x6119
#define VD1_PRE_HSCALE_COEF_0                      0x611a
#define VD1_PRE_VSCALE_COEF_1                      0x611b
#define VD1_PRE_SCALE_FLT_NUM                      0x611c
#define VD1_PRE_HSCALE_COEF_1                      0x611d
#define VD1_1_SCALE_COEF_IDX                       0x6120
#define VD1_1_SCALE_COEF                           0x6121
#define VD1_1_VSC_REGION12_STARTP                  0x6122
#define VD1_1_VSC_REGION34_STARTP                  0x6123
#define VD1_1_VSC_REGION4_ENDP                     0x6124
#define VD1_1_VSC_START_PHASE_STEP                 0x6125
#define VD1_1_VSC_REGION0_PHASE_SLOPE              0x6126
#define VD1_1_VSC_REGION1_PHASE_SLOPE              0x6127
#define VD1_1_VSC_REGION3_PHASE_SLOPE              0x6128
#define VD1_1_VSC_REGION4_PHASE_SLOPE              0x6129
#define VD1_1_VSC_PHASE_CTRL                       0x612a
#define VD1_1_VSC_INI_PHASE                        0x612b
#define VD1_1_HSC_REGION12_STARTP                  0x612c
#define VD1_1_HSC_REGION34_STARTP                  0x612d
#define VD1_1_HSC_REGION4_ENDP                     0x612e
#define VD1_1_HSC_START_PHASE_STEP                 0x612f
#define VD1_1_HSC_REGION0_PHASE_SLOPE              0x6130
#define VD1_1_HSC_REGION1_PHASE_SLOPE              0x6131
#define VD1_1_HSC_REGION3_PHASE_SLOPE              0x6132
#define VD1_1_HSC_REGION4_PHASE_SLOPE              0x6133
#define VD1_1_HSC_PHASE_CTRL                       0x6134
#define VD1_1_SC_MISC                              0x6135
#define VD1_1_SCO_FIFO_CTRL                        0x6136
#define VD1_1_HSC_PHASE_CTRL1                      0x6137
#define VD1_1_HSC_INI_PAT_CTRL                     0x6138
#define VD1_1_SC_GCLK_CTRL                         0x6139
#define VD1_1_PRE_HSCALE_COEF_0                    0x613a
#define VD1_1_PRE_VSCALE_COEF_1                    0x613b
#define VD1_1_PRE_SCALE_FLT_NUM                    0x613c
#define VD1_1_PRE_HSCALE_COEF_1                    0x613d
#define T6X_VD2_SCALE_COEF_IDX                     0x6140
#define T6X_VD2_SCALE_COEF                         0x6141
#define T6X_VD2_VSC_REGION12_STARTP                0x6142
#define T6X_VD2_VSC_REGION34_STARTP                0x6143
#define T6X_VD2_VSC_REGION4_ENDP                   0x6144
#define T6X_VD2_VSC_START_PHASE_STEP               0x6145
#define T6X_VD2_VSC_REGION0_PHASE_SLOPE            0x6146
#define T6X_VD2_VSC_REGION1_PHASE_SLOPE            0x6147
#define T6X_VD2_VSC_REGION3_PHASE_SLOPE            0x6148
#define T6X_VD2_VSC_REGION4_PHASE_SLOPE            0x6149
#define T6X_VD2_VSC_PHASE_CTRL                     0x614a
#define T6X_VD2_VSC_INI_PHASE                      0x614b
#define T6X_VD2_HSC_REGION12_STARTP                0x614c
#define T6X_VD2_HSC_REGION34_STARTP                0x614d
#define T6X_VD2_HSC_REGION4_ENDP                   0x614e
#define T6X_VD2_HSC_START_PHASE_STEP               0x614f
#define T6X_VD2_HSC_REGION0_PHASE_SLOPE            0x6150
#define T6X_VD2_HSC_REGION1_PHASE_SLOPE            0x6151
#define T6X_VD2_HSC_REGION3_PHASE_SLOPE            0x6152
#define T6X_VD2_HSC_REGION4_PHASE_SLOPE            0x6153
#define T6X_VD2_HSC_PHASE_CTRL                     0x6154
#define T6X_VD2_SC_MISC                            0x6155
#define T6X_VD2_SCO_FIFO_CTRL                      0x6156
#define T6X_VD2_HSC_PHASE_CTRL1                    0x6157
#define T6X_VD2_HSC_INI_PAT_CTRL                   0x6158
#define T6X_VD2_SC_GCLK_CTRL                       0x6159
#define T6X_VD2_PRE_HSCALE_COEF_0                  0x615a
#define T6X_VD2_PRE_VSCALE_COEF_1                  0x615b
#define T6X_VD2_PRE_SCALE_FLT_NUM                  0x615c
#define T6X_VD2_PRE_HSCALE_COEF_1                  0x615d
#define VD2_1_SCALE_COEF_IDX                       0x6160
#define VD2_1_SCALE_COEF                           0x6161
#define VD2_1_VSC_REGION12_STARTP                  0x6162
#define VD2_1_VSC_REGION34_STARTP                  0x6163
#define VD2_1_VSC_REGION4_ENDP                     0x6164
#define VD2_1_VSC_START_PHASE_STEP                 0x6165
#define VD2_1_VSC_REGION0_PHASE_SLOPE              0x6166
#define VD2_1_VSC_REGION1_PHASE_SLOPE              0x6167
#define VD2_1_VSC_REGION3_PHASE_SLOPE              0x6168
#define VD2_1_VSC_REGION4_PHASE_SLOPE              0x6169
#define VD2_1_VSC_PHASE_CTRL                       0x616a
#define VD2_1_VSC_INI_PHASE                        0x616b
#define VD2_1_HSC_REGION12_STARTP                  0x616c
#define VD2_1_HSC_REGION34_STARTP                  0x616d
#define VD2_1_HSC_REGION4_ENDP                     0x616e
#define VD2_1_HSC_START_PHASE_STEP                 0x616f
#define VD2_1_HSC_REGION0_PHASE_SLOPE              0x6170
#define VD2_1_HSC_REGION1_PHASE_SLOPE              0x6171
#define VD2_1_HSC_REGION3_PHASE_SLOPE              0x6172
#define VD2_1_HSC_REGION4_PHASE_SLOPE              0x6173
#define VD2_1_HSC_PHASE_CTRL                       0x6174
#define VD2_1_SC_MISC                              0x6175
#define VD2_1_SCO_FIFO_CTRL                        0x6176
#define VD2_1_HSC_PHASE_CTRL1                      0x6177
#define VD2_1_HSC_INI_PAT_CTRL                     0x6178
#define VD2_1_SC_GCLK_CTRL                         0x6179
#define VD2_1_PRE_HSCALE_COEF_0                    0x617a
#define VD2_1_PRE_VSCALE_COEF_1                    0x617b
#define VD2_1_PRE_SCALE_FLT_NUM                    0x617c
#define VD2_1_PRE_HSCALE_COEF_1                    0x617d

#define VPU_HDR2_TOP_CTRL                          0x4519
#define PLAYER_2X2_CTRL                            0x1aa7
#define VFCD_TOP_CTRL4                             0x6584
#define VPP_PREBLEND_VD1_1_H_START_END             0x1d2f
#define VPP_PREBLEND_VD1_1_V_START_END             0x1d30
#define VPP_BLEND_VD2_1_H_START_END                0x1d31
#define VPP_BLEND_VD2_1_V_START_END                0x1d32
#define VD1_1_HDR_IN_SIZE                          0x1aa5
#define VD2_1_HDR_IN_SIZE                          0x1aa6
#define VPU_AXIRD_FG_CTRL_1                        0x6d39
#define VPU_HDR2_FRM2_SIZE                         0x4518

#define VIU_VPP_VD_DIN0_CTRL                       0x1ae0
#define VIU_VPP_VD_DIN1_CTRL                       0x1ae1
#define VIU_VPP_VD_DIN2_CTRL                       0x1ae2
#define VIU_VPP_VD_DIN3_CTRL                       0x1ae3
#define VIU_VPP_VD_DIN4_CTRL                       0x1ae4

#define VPP1_VD1_BLEND_SRC_CTRL                    0x64af
#define VPP1_VD2_BLEND_SRC_CTRL                    0x64b0
#define VPP1_OSD1_BLEND_SRC_CTRL                   0x64b1
#define VPP1_OSD2_BLEND_SRC_CTRL                   0x64b2
#endif

