/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#ifndef __DVBT_FUNC_H__
#define __DVBT_FUNC_H__

enum channel_bw_e {
	CHAN_BW_1M7 = 1,
	CHAN_BW_5M = 5,
	CHAN_BW_6M = 6,
	CHAN_BW_7M = 7,
	CHAN_BW_8M = 8,
	CHAN_BW_10M = 9,
	CHAN_BW_UNKNOWN = 10
};

enum plp_type_e {
	PLPCOMMON = 0,
	PLPDATA1 = 1,
	PLPDATA2 = 2,
	PLPTYPERESERVED = 3,
	PLPTYPEDATA = 4,
	PLPTYPEUNKNOWN = 5
};

enum ldpc_ite_md {
	QPSK_1_2 = 0x0,
	QPSK_3_5 = 0x1,
	QPSK_2_3 = 0x2,
	QPSK_3_4 = 0x3,
	QPSK_4_5 = 0x4,
	QPSK_5_6 = 0x5,
	QAM16_1_2 = 0x8,
	QAM16_3_5 = 0x9,
	QAM16_2_3 = 0xa,
	QAM16_3_4 = 0xb,
	QAM16_4_5 = 0xc,
	QAM16_5_6 = 0xd,
	QAM64_1_2 = 0x10,
	QAM64_3_5 = 0x11,
	QAM64_2_3 = 0x12,
	QAM64_3_4 = 0x13,
	QAM64_4_5 = 0x14,
	QAM64_5_6 = 0x15,
	QAM256_1_2 = 0x18,
	QAM256_3_5 = 0x19,
	QAM256_2_3 = 0x1a,
	QAM256_3_4 = 0x1b,
	QAM256_4_5 = 0x1c,
	QAM256_5_6 = 0x1d
};

#define MAX_PLP_NUM	256

#define DVBT2_ICCM_BASE		0xa00000
#define DVBT2_DCCM_BASE		0xb00000

/* I2CRPT */
#define	R368TER_I2CRPT		0x0001

/* TOPCTRL */
#define	R368TER_TOPCTRL		0x0002

/* STDBY0 */
#define	R368TER_STDBY0		0x0004

/* STDBY1 */
#define	R368TER_STDBY1		0x0005

/* RESET0 */
#define	R368TER_RESET0		0x0006

/* RESET1 */
#define	R368TER_RESET1		0x0007

/* LOWPOW0 */
#define	R368TER_LOWPOW0		0x0008

/* LOWPOW1 */
#define	R368TER_LOWPOW1		0x0009

/* LOWPOW2 */
#define	R368TER_LOWPOW2		0x000A

/* PAD_CFG0 */
#define	R368TER_PAD_CFG0		0x000D

/* AUX_CLK */
#define	R368TER_AUX_CLK		0x0014

/* DVBX_CHOICE */
#define	R368TER_DVBX_CHOICE		0x0019

/* CLK_XP70_CFG */
#define	R368TER_CLK_XP70_CFG		0x001A

/* TEST_CFG0 */
#define	R368TER_TEST_CFG0		0x001B

/* TEST_CFG1 */
#define	R368TER_TEST_CFG1		0x001C

/* XP70_CONF1 */
#define	R368TER_XP70_CONF1		0x0041

/* AD12_CONF */
#define R368TER_AD12_CONF		0x003C

/* SERIAL_XP70_DBG0 */
#define	R368TER_SERIAL_XP70_DBG0		0x004D

/* SERIAL_XP70_DBG1 */
#define	R368TER_SERIAL_XP70_DBG1		0x004E

/* ANA_CTRL */
#define	R368TER_ANA_CTRL		0x0050

/* ANADIG_CTRL */
#define	R368TER_ANADIG_CTRL		0x005C

/* PLLODF */
#define	R368TER_PLLODF		0x0060

/* PLLNDIV */
#define	R368TER_PLLNDIV		0x0061

/* PLLIDF */
#define	R368TER_PLLIDF		0x0062

/* DUAL_AD12 */
#define	R368TER_DUAL_AD12		0x0063

/* PAD_COMP_CTRL */
#define	R368TER_PAD_COMP_CTRL		0x0064

/* SIGD_FREQ0 */
#define	R368TER_SIGD_FREQ0		0x0080

/* SIGD_FREQ1 */
#define	R368TER_SIGD_FREQ1		0x0081

/* SIGD_FREQ2 */
#define	R368TER_SIGD_FREQ2		0x0082

/* SIGD_FREQ3 */
#define	R368TER_SIGD_FREQ3		0x0083

/* SIGD0 */
#define	R368TER_SIGD0		0x0084

/* SIGD1 */
#define	R368TER_SIGD1		0x0085

/* SIGD2 */
#define	R368TER_SIGD2		0x0086

/* SIGD3 */
#define	R368TER_SIGD3		0x0087

/* GPIO_AGC_IF */
#define R368TER_GPIO_AGC_IF		0x0098

/* GPIO_AGC_RF */
#define R368TER_GPIO_AGC_RF		0x0099

/* GPIO_ANT_CTRL */
#define R368TER_GPIO_ANT_CTRL		0x009A

/* GPIO_SDAT */
#define R368TER_GPIO_SDAT		0x009B

/* GPIO_SCLT */
#define R368TER_GPIO_SCLT		0x009C

/* GPIO_AUX_CLK */
#define R368TER_GPIO_AUX_CLK		0x009D

/* GPIO_TS0_CLK_OUT */
#define R368TER_GPIO_TS0_CLK_OUT		0x00C5

/* GPIO_TS0_D_NOT_P */
#define R368TER_GPIO_TS0_D_NOT_P		0x00C6

/* GPIO_TS0_ERROR */
#define R368TER_GPIO_TS0_ERROR		0x00C7

/* GPIO_TS0_STR_OUT */
#define R368TER_GPIO_TS0_STR_OUT		0x00C8

/* P1_TSDLYSET2 */
#define R368TER_P1_TSDLYSET2		0x0592

/* P1_TSDLYSET1 */
#define R368TER_P1_TSDLYSET1		0x0593

/* P1_TSDLYSET0 */
#define R368TER_P1_TSDLYSET0		0x0594

/* P1_SFDLYSETH */
#define R368TER_P1_SFDLYSETH		0x05D0

/* P1_SFDLYSET1 */
#define R368TER_P1_SFDLYSET1		0x05D1

/* P1_SFDLYSET0 */
#define R368TER_P1_SFDLYSET0		0x05D2

/* RAMCFGM */
#define R368TER_RAMCFGM		0x0634

/* RAMCFGL */
#define R368TER_RAMCFGL		0x0635

/* MB4 */
#define	R368TER_MB4		0x00D0

/* MB5 */
#define	R368TER_MB5		0x00D1

/* MB6 */
#define	R368TER_MB6		0x00D2

/* MB7 */
#define	R368TER_MB7		0x00D3

/* TEST_CONF1 */
#define	R368TER_TEST_CONF1		0x00E6

/* P1_TSMINSPEED */
#define	R368TER_P1_TSMINSPEED		0x054B

/* P1_TSMAXSPEED */
#define	R368TER_P1_TSMAXSPEED		0x054C

/* P1_TSCFGH */
#define	R368TER_P1_TSCFGH		0x0572

/* P1_SYMBCFG2 */
#define	R368TER_P1_SYMBCFG2		0x05E8

/* AGC_DBW0 */
#define	R368TER_AGC_DBW0		0x0821

/* AGC_DBW1 */
#define	R368TER_AGC_DBW1		0x0822

/* TOP_AGC_CONF1 */
#define	R368TER_TOP_AGC_CONF1		0x0824

/* AGC_FREEZE_CONF0 */
#define	R368TER_AGC_FREEZE_CONF0		0x082C

/* TOP_AGC_CONF5 */
#define	R368TER_TOP_AGC_CONF5		0x0830

/* TSTTS4 */
#define	R368TER_TSTTS4		0x0F5F

/* IQFE_AGC_CONF0 */
#define	R368TER_IQFE_AGC_CONF0		0x1560

/* IQFE_AGC_CONF1 */
#define	R368TER_IQFE_AGC_CONF1		0x1561

/* IQFE_AGC_CONF2 */
#define	R368TER_IQFE_AGC_CONF2		0x1562

/* IQFE_AGC_CONF3 */
#define	R368TER_IQFE_AGC_CONF3		0x1563

/* IQFE_AGC_CONF4 */
#define	R368TER_IQFE_AGC_CONF4		0x1564

/* AGC_TARGETI0 */
#define	R368TER_AGC_TARGETI0		0x1568

/* AGC_TARGETI1 */
#define	R368TER_AGC_TARGETI1		0x1569

/* AGC_TARGETQ0 */
#define	R368TER_AGC_TARGETQ0		0x156A

/* AGC_TARGETQ1 */
#define	R368TER_AGC_TARGETQ1		0x156B

/* AGC_CONF5 */
#define	R368TER_AGC_CONF5		0x156C

/* AGC_CONF6 */
#define	R368TER_AGC_CONF6		0x156D

/* LOCK_DET1_0 */
#define R368TER_LOCK_DET1_0		0x1570

/* LOCK_DET1_1 */
#define R368TER_LOCK_DET1_1		0x1571

/* LOCK_DET2_0 */
#define R368TER_LOCK_DET2_0		0x1572

/* LOCK_DET2_1 */
#define R368TER_LOCK_DET2_1		0x1573

/* LOCK_DET3_0 */
#define R368TER_LOCK_DET3_0		0x1574

/* LOCK_DET3_1 */
#define R368TER_LOCK_DET3_1		0x1575

/* LOCK_DET4_0 */
#define R368TER_LOCK_DET4_0		0x1576

/* LOCK_DET4_1 */
#define R368TER_LOCK_DET4_1		0x1577

/* LOCK_N */
#define	R368TER_LOCK_N		0x1578

/* FEPATH_CONF0 */
#define R368TER_FEPATH_CONF0		0x1594

/* INC_CONF0 */
#define R368TER_INC_CONF0		0x15A0

/* INC_CONF1 */
#define R368TER_INC_CONF1		0x15A1

/* INC_CONF2 */
#define R368TER_INC_CONF2		0x15A2

/* INC_CONF3 */
#define R368TER_INC_CONF3		0x15A3

/* INC_CONF4 */
#define R368TER_INC_CONF4		0x15A4

/* INC_BRSTCNT0 */
#define R368TER_INC_BRSTCNT0		0x15A5

/* INC_BRSTCNT1 */
#define R368TER_INC_BRSTCNT1		0x15A6

/* DCCOMP */
#define R368TER_DCCOMP		0x15A8

/* SRC_CONF1 */
#define	R368TER_SRC_CONF1		0x15B3

/* P1AGC_TARG */
#define	R368TER_P1AGC_TARG		0x15B9

/* CAS_CONF1 */
#define	R368TER_CAS_CONF1		0x15D1

/* CAS_CCSMU */
#define	R368TER_CAS_CCSMU		0x15D3

/* CAS_CCDCCI */
#define	R368TER_CAS_CCDCCI		0x15D8

/* CAS_CCDNOCCI */
#define	R368TER_CAS_CCDNOCCI		0x15D9

/* FFT_FACTOR_2K_S2 */
#define	R368TER_FFT_FACTOR_2K_S2		0x2815

/* FFT_FACTOR_8K_S2 */
#define	R368TER_FFT_FACTOR_8K_S2		0x281D

/* P1_CTRL */
#define	R368TER_P1_CTRL		0x2830

/* P1_DIV */
#define	R368TER_P1_DIV		0x2833

/* P1_CFG */
#define	R368TER_P1_CFG		0x283C

/* TFO_GAIN */
#define R368TER_TFO_GAIN		0x2895

/* TFO_GAIN_CONV */
#define R368TER_TFO_GAIN_CONV		0x2896

/* CFO_GAIN */
#define R368TER_CFO_GAIN		0x2897

/* CFO_GAIN_CONV */
#define R368TER_CFO_GAIN_CONV		0x2898

/* TFO_COEFF0 */
#define R368TER_TFO_COEFF0		0x2899

/* TFO_COEFF1 */
#define R368TER_TFO_COEFF1		0x289A

/* TFO_COEFF2 */
#define R368TER_TFO_COEFF2		0x289B

/* CFO_COEFF0 */
#define R368TER_CFO_COEFF0		0x289C

/* CFO_COEFF1 */
#define R368TER_CFO_COEFF1		0x289D

/* CFO_COEFF2 */
#define R368TER_CFO_COEFF2		0x289E

/* DVBT_CTRL */
#define	R368TER_DVBT_CTRL		0x2900

/* CORREL_CTL */
#define R368TER_CORREL_CTL		0x2906

/* TPS_RCVD3 */
#define R368TER_TPS_RCVD3		0x2913

/* SCENARII_CFG */
#define	R368TER_SCENARII_CFG		0x2920

/* SYMBOL_TEMPO */
#define	R368TER_SYMBOL_TEMPO		0x2921

/* CHC_TI0 */
#define	R368TER_CHC_TI0		0x2A18

/* CHC_TRIG0 */
#define	R368TER_CHC_TRIG0		0x2A28

/* CHC_TRIG1 */
#define	R368TER_CHC_TRIG1		0x2A29

/* CHC_TRIG2 */
#define	R368TER_CHC_TRIG2		0x2A2A

/* CHC_TRIG3 */
#define	R368TER_CHC_TRIG3		0x2A2B

/* CHC_TRIG4 */
#define	R368TER_CHC_TRIG4		0x2A2C

/* CHC_TRIG5 */
#define	R368TER_CHC_TRIG5		0x2A2D

/* CHC_TRIG6 */
#define	R368TER_CHC_TRIG6		0x2A2E

/* CHC_TRIG7 */
#define R368TER_CHC_TRIG7		0x2A2F

/* CHC_TRIG8 */
#define R368TER_CHC_TRIG8		0x2A30

/* CHC_TRIG9 */
#define R368TER_CHC_TRIG9		0x2A31

/* CHC_SNR10 */
#define	R368TER_CHC_SNR10		0x2A5E

/* CHC_SNR11 */
#define	R368TER_CHC_SNR11		0x2A5F

/* DVBT_CONF */
#define R368TER_DVBT_CONF		0x3760

/* NSCALE_DVBT_0 */
#define	R368TER_NSCALE_DVBT_0		0x3761

/* NSCALE_DVBT_1 */
#define	R368TER_NSCALE_DVBT_1		0x3762

/* NSCALE_DVBT_2 */
#define	R368TER_NSCALE_DVBT_2		0x3763

/* NSCALE_DVBT_3 */
#define	R368TER_NSCALE_DVBT_3		0x3764

/* NSCALE_DVBT_4 */
#define	R368TER_NSCALE_DVBT_4		0x3765

/* NSCALE_DVBT_5 */
#define	R368TER_NSCALE_DVBT_5		0x3766

/* NSCALE_DVBT_6 */
#define	R368TER_NSCALE_DVBT_6		0x3767

/* IDM_RD_DVBT_1 */
#define	R368TER_IDM_RD_DVBT_1		0x3768

/* IDM_RD_DVBT_2 */
#define	R368TER_IDM_RD_DVBT_2		0x3769

#define TS_STATUS	0x581
#define DVBT_STATUS	0x2901
#define CHC_CIR_SNR0	0x2a08
#define CHC_CIR_SNR1	0x2a09

extern const unsigned int minimum_snr_x10[4][6];
void dvbt2_init(struct aml_dtvdemod *demod, struct dvb_frontend *fe);
unsigned int dtvdemod_calcul_get_field(unsigned int memory_base, unsigned int nb_bits_shift,
					unsigned int var_size);
void dtvdemod_get_plp(struct amldtvdemod_device_s *devp, struct dtv_property *tvp);
void dtvdemod_get_plp_dbg(void);
void dtvdemod_set_plpid(char id);
void dvbt_reg_initial(unsigned int bw, struct dvb_frontend *fe);
void dvbt2_reset(struct aml_dtvdemod *demod, struct dvb_frontend *fe);
void dvbt2_riscv_init(struct aml_dtvdemod *demod, struct dvb_frontend *fe);
void dvbt2_info(struct seq_file *seq);
void dvbt_info(struct aml_dtvdemod *demod, struct seq_file *seq);
#endif
