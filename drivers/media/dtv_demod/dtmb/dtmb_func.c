// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2021 Amlogic, Inc. All rights reserved.
 */

#include "addr_dtmb_front_bit.h"
#include "demod_func.h"
#include "demod_dbg.h"
#include "dtmb_func.h"

bool demod_enable_performance = true;
int demod_sync_count = 60;
int demod_sync_delay_time = 8;
int demod_timeout = 120;

void dtmb_set_fe_config_modify(unsigned int modify)
{
	union DTMB_SYNC_FE_CONFIG_BITS fe_config;

	fe_config.d32 = dtmb_read_reg(DTMB_SYNC_FE_CONFIG);
	fe_config.b.fe_modify = modify;

	dtmb_write_reg(DTMB_SYNC_FE_CONFIG, fe_config.d32);
	PR_DTMB("set fe_config 0x%x,0x%x\n", modify, fe_config.d32);
}

/* formula: fs(MHz)
 *  2*7.56*2^23/fs/256
 *	24MHz: 0x50a3
 *	25MHz: 0x4d6a
 */
void dtmb_clk_set(unsigned int adc_clk)
{
	unsigned int fe_modify = 0x4d6a;

	if (adc_clk)
		fe_modify = 3963617280UL / (adc_clk << 3);

	dtmb_set_fe_config_modify(fe_modify);
}

static void dtmb_24m_coeff(void)
{
	dtmb_write_reg(DTMB_FRONT_COEF_SET19, 0xebd2530d);
	dtmb_write_reg(DTMB_FRONT_COEF_SET18, 0x04dad364);
	dtmb_write_reg(DTMB_FRONT_COEF_SET17, 0x181e0508);
	dtmb_write_reg(DTMB_FRONT_COEF_SET16, 0x080a031a);
	dtmb_write_reg(DTMB_FRONT_COEF_SET15, 0x0217161f);
	dtmb_write_reg(DTMB_FRONT_COEF_SET14, 0x000c0c);
	dtmb_write_reg(DTMB_FRONT_COEF_SET13, 0x0e3f3334);
	dtmb_write_reg(DTMB_FRONT_COEF_SET12, 0x2e330310);
	dtmb_write_reg(DTMB_FRONT_COEF_SET11, 0x08160f3c);
	dtmb_write_reg(DTMB_FRONT_COEF_SET10, 0x352731);
	dtmb_write_reg(DTMB_FRONT_COEF_SET9, 0x70101f11);
	dtmb_write_reg(DTMB_FRONT_COEF_SET8, 0x2d126b5c);
	dtmb_write_reg(DTMB_FRONT_COEF_SET7, 0x1f);
	dtmb_write_reg(DTMB_FRONT_COEF_SET6, 0xd4c8ef);
	dtmb_write_reg(DTMB_FRONT_COEF_SET5, 0x04e013);
	dtmb_write_reg(DTMB_FRONT_COEF_SET4, 0x46);
	dtmb_write_reg(DTMB_FRONT_COEF_SET3, 0x3883ee);
	dtmb_write_reg(DTMB_FRONT_COEF_SET2, 0x37e);
	dtmb_write_reg(DTMB_FRONT_COEF_SET1, 0x123013);
	dtmb_write_reg(DTMB_FRONT_ACF_BYPASS,
		((dtmb_read_reg(DTMB_FRONT_ACF_BYPASS) & ~0xffffff)
		| 0x29922b));
}

static void dtmb_25m_coeff(void)
{
	dtmb_write_reg(DTMB_FRONT_COEF_SET19, 0x242fde12);
	dtmb_write_reg(DTMB_FRONT_COEF_SET18, 0x451dce);
	dtmb_write_reg(DTMB_FRONT_COEF_SET17, 0x051f1a1b);
	dtmb_write_reg(DTMB_FRONT_COEF_SET16, 0x181c0307);
	dtmb_write_reg(DTMB_FRONT_COEF_SET15, 0x0809031b);
	dtmb_write_reg(DTMB_FRONT_COEF_SET14, 0x15161f);
	dtmb_write_reg(DTMB_FRONT_COEF_SET13, 0x060e0a3e);
	dtmb_write_reg(DTMB_FRONT_COEF_SET12, 0x06363038);
	dtmb_write_reg(DTMB_FRONT_COEF_SET11, 0x2d3e0f12);
	dtmb_write_reg(DTMB_FRONT_COEF_SET10, 0x133c2b);
	dtmb_write_reg(DTMB_FRONT_COEF_SET9, 0x5f700c1b);
	dtmb_write_reg(DTMB_FRONT_COEF_SET8, 0x23270b6a);
	dtmb_write_reg(DTMB_FRONT_COEF_SET7, 0x7e);
	dtmb_write_reg(DTMB_FRONT_COEF_SET6, 0xf3cbd4);
	dtmb_write_reg(DTMB_FRONT_COEF_SET5, 0x04f031);
	dtmb_write_reg(DTMB_FRONT_COEF_SET4, 0x29);
	dtmb_write_reg(DTMB_FRONT_COEF_SET3, 0x37f3cc);
	dtmb_write_reg(DTMB_FRONT_COEF_SET2, 0x396);
	dtmb_write_reg(DTMB_FRONT_COEF_SET1, 0x131036);
	dtmb_write_reg(DTMB_FRONT_ACF_BYPASS,
		((dtmb_read_reg(DTMB_FRONT_ACF_BYPASS) & ~0xffffff)
		| 0x274217));
}

void dtmb_all_reset(struct aml_dtvdemod *demod)
{
	int temp_data = 0;
	unsigned int reg_val;

	if (demod_chip_eq(DTVDEMOD_HW_TXL)) {
		/*fix bug 139044: DTMB lost sync*/
		/*dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x1f);*/
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x22);
		/*modified bu xiaotong*/
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x1a027719);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x101a7);
		/*21bit set ddr access urgent*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331a31);
		/*detect 64qam 420 595 problems*/
		dtmb_write_reg(DTMB_FRONT_19_CONFIG, 0x300);
		dtmb_write_reg(DTMB_FRONT_4d_CONFIG, 0x12ffbe0);
		/*fix fsm b bug*/
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x5680000);
		/*fix agc problem,skip warm_up status*/
		dtmb_write_reg(DTMB_FRONT_46_CONFIG, 0x1a000f0f);
		dtmb_write_reg(DTMB_FRONT_ST_FREQ, 0xf2400000);
		dtmb_clk_set(ADC_CLK_25M);
	} else if (demod_chip_eq(DTVDEMOD_HW_TXHD)) {
		/* dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x1f); */
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x1e);

		/*demod can't sync when freq offset >1.5KHz for air signal*/
		dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x6aaaaa);
		dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x13196596);

		/*modified bu xiaotong*/
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x1a027719);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x101a7);
		/*21bit set ddr access urgent*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331a31);
		/*detect 64qam 420 595 problems*/
		dtmb_write_reg(DTMB_FRONT_19_CONFIG, 0x300);
		dtmb_write_reg(DTMB_FRONT_4d_CONFIG, 0x12ffbe0);
		/*fix fsm b bug*/
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x5680000);
		/*fix agc problem,skip warm_up status*/
		dtmb_write_reg(DTMB_FRONT_46_CONFIG, 0x1a000f0f);
		dtmb_write_reg(DTMB_FRONT_ST_FREQ, 0xf2400000);
		dtmb_clk_set(ADC_CLK_24M);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x1b027719);
	} else if (demod_chip_after_eq(DTVDEMOD_HW_TL1)) {
		if (demod_get_adc_clk(demod) == ADC_CLK_24M) {
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x6aaaaa);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x13196596);
			dtmb_write_reg(0x5b, 0x50a30a25);
			dtmb_24m_coeff();
		} else if (demod_get_adc_clk(demod) == ADC_CLK_25M) {
			dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x62c1a5);
			dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x131a747d);
			dtmb_write_reg(0x5b, 0x4d6a0a25);
			dtmb_25m_coeff();
		}

		/*for timeshift mosaic issue
		 *bit 30:ts_sync_sel,0=ts_sync,1=searched ts_sync
		 */
		dtmb_write_reg(DTMB_FRONT_4e_CONFIG, 0x656cf604);

		/*delay fec lock & make fec lost faster
		 *to prevent eq is confused by signal
		 */
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x5580000);
		/*reduce fec lost timeout*/
		dtmb_write_reg(DTMB_FRONT_19_CONFIG, 0x30);

		reg_val = dtmb_read_reg(DTMB_TOP_CTRL_TPS);
		/* for Task 19:Switch mode and modulation parameters test
		 * spectrum: 0=normal, 1=inverted
		 */
		if (demod->demod_status.spectrum == 0)
			reg_val |= 0x4;
		else if (demod->demod_status.spectrum == 1)
			reg_val &= ~0x4;
		dtmb_write_reg(DTMB_TOP_CTRL_TPS, reg_val);
	} else {
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG1, 0x10127);
		dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG6, 0x943228cc);
		dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG7, 0xc09aa8cd);
		dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, 0x0);
		dtmb_write_reg(DTMB_CHE_EQ_CONFIG, 0x9dc59);
		/*0x2 is auto,0x406 is  invert spectrum*/
		if (demod->demod_status.spectrum == 0)
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x406);
		else if (demod->demod_status.spectrum == 1)
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x402);
		else
			dtmb_write_reg(DTMB_TOP_CTRL_TPS, 0x2);

		PR_DTMB("spectrum %d\n", demod->demod_status.spectrum);
		dtmb_write_reg(DTMB_TOP_CTRL_FEC, 0x41444400);
		dtmb_write_reg(DTMB_TOP_CTRL_INTLV_TIME, 0x180300);
		dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x662ca0);
		dtmb_write_reg(DTMB_FRONT_AFIFO_ADC, 0x29);
		dtmb_write_reg(DTMB_FRONT_DC_HOLD, 0xa1066);
		/*cci para*/
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG3, 0x80201f6);
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG2, 0x3f20080);
		dtmb_write_reg(DTMB_CHE_TPS_CONFIG, 0xc00000);
		dtmb_write_reg(DTMB_TOP_CTRL_AGC, 0x3);
		dtmb_write_reg(DTMB_TOP_CTRL_TS_SFO_CFO, 0x20403006);
		dtmb_write_reg(DTMB_FRONT_AGC_CONFIG2, 0x7200a16);
		dtmb_write_reg(DTMB_FRONT_DEBUG_CFG, 0x1e00000);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0x7fffff);
		/*close ts3 timing loop*/
		dtmb_write_reg(DTMB_TOP_CTRL_DAGC_CCI, 0x305);
		/*dektec card issue,close f case snr drop*/
		dtmb_write_reg(DTMB_CHE_MC_SC_TIMING_POWTHR, 0xc06100a);
		if (demod_enable_performance) {
			dtmb_write_reg(DTMB_CHE_IBDFE_CONFIG1, 0x4040002);
			temp_data = dtmb_read_reg(DTMB_CHE_FD_TD_COEFF);
		    temp_data = (temp_data & ~0x3fff) | (0x241f & 0x3fff);
		    temp_data = temp_data | (1 << 21);
			/*Set freeze_mode and reset coeff*/
		    dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, temp_data);
		    temp_data = temp_data & ~(1 << 21);
			/*Set freeze_mode and reset coeff*/
		    dtmb_write_reg(DTMB_CHE_FD_TD_COEFF, temp_data);
		}
	}

	/*for non-standard signal,
	 *ignore calculating amplitude tps(transport parameter signalling)
	 */
	dtmb_write_reg(DTMB_CHE_IBDFE_CONF0,
		dtmb_read_reg(DTMB_CHE_IBDFE_CONF0) & 0xfffff0ff);

	/*increase cfo track val to avoid dtmb playing is not smooth
	 *when temperature is over 80 degree Celsius
	 */
	dtmb_write_reg(DTMB_SYNC_TRACK_CFO_MAX,
		(dtmb_read_reg(DTMB_SYNC_TRACK_CFO_MAX) & ~0xff) | 0x1f);
}

void dtmb_initial(struct aml_dtvdemod *demod)
{
	/* dtmb_write_reg(0x049, memstart);		//only for init */
	dtmb_register_reset();

	if (demod_chip_eq(DTVDEMOD_HW_T3)) {
		clear_ddr_bus_data(demod);
		//dtmb_write_reg(0x47, 0x133220);
		dtmb_write_reg_bits(0x47, 0x0, 22, 1);
		dtmb_write_reg_bits(0x47, 0x0, 23, 1);
	}
	dtmb_all_reset(demod);

	if (demod_chip_eq(DTVDEMOD_HW_T3)) {
		//dtmb_write_reg(0x7, 0x4ffffff);
		dtmb_write_reg_bits(0x7, 0x1, 26, 1);
		dtmb_write_reg_bits(0x7, 0x0, 25, 1);
		dtmb_write_reg_bits(0x7, 0x0, 24, 1);
		dtmb_write_reg_bits(0x7, 0x1, 23, 1);
		dtmb_write_reg(0x19, 0xfffff);
	}
}

int check_dtmb_fec_lock(void)
{
	int fec_lock, snr, status;
	/*fec_lock = (dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1;*/
	fec_lock = dtmb_reg_r_fec_lock();
	/*snr = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) & 0x3fff;*/
	snr = dtmb_reg_r_che_snr();
	if (fec_lock && snr > 4)
		status = 1;
	else
		status = 0;

	return status;
}

int check_dtmb_mobile_det(void)
{
	int mobile_det = 0;

	mobile_det = (dtmb_read_reg(DTMB_TOP_CTRL_SYS_OFDM_CNT) >> 8) & 0x7ffff;

	return mobile_det;
}

int dtmb_information(struct seq_file *seq)
{
	int tps, snr, fec_lock, fec_bch_add, fec_ldpc_unc_acc, fec_ldpc_it_avg, che_snr;
	unsigned int buf[3];

	tps = dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT);

	che_snr = dtmb_reg_r_che_snr();
	snr = che_snr;
	snr = dtmb_convert_snr(snr);
	/*	if (che_snr >= 8192) */
	/*		che_snr = che_snr - 16384;*/
	/*	snr = che_snr / 32;*/
	/* snr = 10*log10(snr)-6; */
	/* fec_lock = (dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR) >> 14) & 0x1; */
	fec_lock = dtmb_reg_r_fec_lock();
	fec_bch_add = dtmb_reg_r_bch();
	fec_ldpc_unc_acc = dtmb_read_reg(DTMB_TOP_FEC_LDPC_UNC_ACC);
	fec_ldpc_it_avg = dtmb_read_reg(DTMB_TOP_FEC_LDPC_IT_AVG);
	dtmb_read_agc(DTMB_D9_ALL, &buf[0]);

	if (seq) {
		seq_printf(seq, "[FSM]: %x %x %x %x\n",
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0),
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE1),
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE2),
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE3));

		seq_printf(seq, "[AGC]: agc_power %d,agc_if_gain %d,agc_rf_gain %d,",
			   (-((buf[2]) / 16)),	buf[0], buf[1]);

		seq_printf(seq, "dagc_power %3d,dagc_gain %3d mobi_det_power %d\n",
		      ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 0) & 0xff),
		     ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 8) & 0xfff),
		     (dtmb_read_reg(DTMB_TOP_CTRL_SYS_OFDM_CNT) >> 8) & 0x7ffff);
		seq_printf(seq, "[TPS] SC or MC %2d,f_r %2d qam_nr %2d ",
		     (dtmb_read_reg(DTMB_TOP_CHE_OBS_STATE1) >> 1) & 0x1,
		     (tps >> 22) & 0x1, (tps >> 21) & 0x1);
		seq_printf(seq, "intlv %2d,cr %2d constl %2d\n",
			(tps >> 20) & 0x1,
		     (tps >> 18) & 0x3, (tps >> 16) & 0x3);

		seq_printf(seq, "[dtmb] snr %d,fec_lock %d,fec_bch_add %d,",
		     snr, fec_lock, fec_bch_add);
		seq_printf(seq, "fec_ldpc_unc_acc %d ,fec_ldpc_it_avg %d\n\n",
		     fec_ldpc_unc_acc,
		     fec_ldpc_it_avg / 256);
	} else {
		PR_DTMB("[FSM] : %x %x %x %x\n",
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0),
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE1),
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE2),
		       dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE3));

		PR_DTMB("[AGC]: agc_power %d,agc_if_gain %d,agc_rf_gain %d,", (-((buf[2]) / 16)),
			buf[0], buf[1]);

		PR_DTMB("dagc_power %3d,dagc_gain %3d mobi_det_power %d\n",
		      ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 0) & 0xff),
		     ((dtmb_read_reg(DTMB_TOP_FRONT_DAGC) >> 8) & 0xfff),
		     (dtmb_read_reg(DTMB_TOP_CTRL_SYS_OFDM_CNT) >> 8) & 0x7ffff);
		PR_DTMB("[TPS] SC or MC %2d,f_r %2d qam_nr %2d ",
		     (dtmb_read_reg(DTMB_TOP_CHE_OBS_STATE1) >> 1) & 0x1,
		     (tps >> 22) & 0x1, (tps >> 21) & 0x1);
		PR_DTMB("intlv %2d,cr %2d constl %2d\n",
			(tps >> 20) & 0x1,
		     (tps >> 18) & 0x3, (tps >> 16) & 0x3);

		PR_DTMB("[dtmb] snr %d,fec_lock %d,fec_bch_add %d,",
		     snr, fec_lock, fec_bch_add);
		PR_DTMB("fec_ldpc_unc_acc %d ,fec_ldpc_it_avg %d\n\n",
		     fec_ldpc_unc_acc,
		     fec_ldpc_it_avg / 256);
	}

	return 0;
}

int dtmb_check_cci(void)
{
	int cci_det = 0;

	cci_det =
	((dtmb_read_reg(DTMB_TOP_SYNC_CCI_NF2_POSITION) >> 22)
		& 0x3);

	if (cci_det > 0) {
		PR_DTMB("find cci\n");
		dtmb_write_reg(DTMB_CHE_CCIDET_CONFIG, 0x20210290);
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG3, 0x20081f6);
		dtmb_write_reg(DTMB_CHE_M_CCI_THR_CONFIG2, 0x3f08020);
	}

	return cci_det;
}

int dtmb_bch_check(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;
	unsigned int value_before = 0;
	int fec_bch_add = 0, last_fec_bch_add = 0, i = 0, strength = 0;
	char *info1 = "fec lock,but bch add,need reset,wait not to reset";
	char *info2 = "fec lock,but bch add,need reset,now is lock";
	int strength_limit = THRD_TUNER_STRENGTH_DTMB;

	if (tuner_find_by_name(fe, "atbm253"))
		strength_limit = THRD_TUNER_STRENGTH_DTMB + 5;

	strength = tuner_get_ch_power(fe);
	fec_bch_add = dtmb_reg_r_bch();
	last_fec_bch_add = fec_bch_add;

	while ((fec_bch_add - last_fec_bch_add) < 50 && i < 20) {
		last_fec_bch_add = fec_bch_add;

		usleep_range(500, 600);//msleep(10);

		fec_bch_add = dtmb_reg_r_bch();
		++i;
	}

	PR_DTMB("i %d fec_bch_add %d last_fec_bch_add %d strength %d\n",
			i, fec_bch_add, last_fec_bch_add, strength);

	if ((fec_bch_add - last_fec_bch_add) >= 50) {
		PR_DTMB("%s\n", info1);

		if (demod_chip_eq(DTVDEMOD_HW_T3) ||
			demod_chip_eq(DTVDEMOD_HW_T5M)) {
			value_before = dtmb_read_reg(0x7);
			PR_INFO("dtmb set ddr\n");
			dtmb_write_reg(0x7, 0x6ffffd);
			//dtmb_write_reg(0x47, 0xed33221);
			dtmb_write_reg_bits(0x47, 0x1, 22, 1);
			dtmb_write_reg_bits(0x47, 0x1, 23, 1);

			msleep(20);
		}

		sw_rst.b.ctrl_sw_rst = 1;
		sw_rst.b.ctrl_sw_rst_noreg = 1;
		dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);

		if (demod_chip_eq(DTVDEMOD_HW_T3) ||
			demod_chip_eq(DTVDEMOD_HW_T5M)) {
			clear_ddr_bus_data(demod);
			dtmb_write_reg(0x7, value_before);
			dtmb_write_reg_bits(0x47, 0x0, 22, 1);
			dtmb_write_reg_bits(0x47, 0x0, 23, 1);
		}

		sw_rst.b.ctrl_sw_rst = 0;
		sw_rst.b.ctrl_sw_rst_noreg = 0;
		dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);

		for (i = 0; i < 60; i++) {
			PR_DTMB("fsm:0x%x\n", dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0));
			msleep(50);
			if (check_dtmb_fec_lock() == 1) {
				PR_DTMB("%s\n", info2);
				return 0;
			}
			if (i % 2 == 0) {
				strength = tuner_get_ch_power(fe);
				if (strength < strength_limit) {
					/*weak signal,return*/
					PR_DTMB("%s strength=%d, return\n",
						 __func__, strength);
					return 0;
				}
			}
		}
	}

	return 0;
}

void dtmb_bch_check_new(struct dvb_frontend *fe, bool reset)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	int fec_bch = 0, strength = 0;
	static int last_fec_bch;
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;
	unsigned int val = 0;
	int strength_limit = THRD_TUNER_STRENGTH_DTMB;

	if (tuner_find_by_name(fe, "atbm253"))
		strength_limit = THRD_TUNER_STRENGTH_DTMB + 5;

	if (reset) {
		last_fec_bch = -1;
		return;
	}

	strength = tuner_get_ch_power(fe);
	fec_bch = dtmb_reg_r_bch();
	PR_DTMB("fec_bch %d, last_fec_bch %d strength %d\n",
			fec_bch, last_fec_bch, strength);
	if ((last_fec_bch != -1 && (fec_bch - last_fec_bch) > 50)) {
		PR_DTMB("fec lock, but bch add, need reset\n");
		if (demod_chip_eq(DTVDEMOD_HW_T3) ||
			demod_chip_eq(DTVDEMOD_HW_T5M)) {
			val = dtmb_read_reg(0x7);
			PR_INFO("dtmb set ddr\n");
			dtmb_write_reg(0x7, 0x6ffffd);
			//dtmb_write_reg(0x47, 0xed33221);
			dtmb_write_reg_bits(0x47, 0x1, 22, 1);
			dtmb_write_reg_bits(0x47, 0x1, 23, 1);
			msleep(20);
		}

		sw_rst.b.ctrl_sw_rst = 1;
		sw_rst.b.ctrl_sw_rst_noreg = 1;
		dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);

		if (demod_chip_eq(DTVDEMOD_HW_T3) ||
			demod_chip_eq(DTVDEMOD_HW_T5M)) {
			clear_ddr_bus_data(demod);
			dtmb_write_reg(0x7, val);
			dtmb_write_reg_bits(0x47, 0x0, 22, 1);
			dtmb_write_reg_bits(0x47, 0x0, 23, 1);
		}

		sw_rst.b.ctrl_sw_rst = 0;
		sw_rst.b.ctrl_sw_rst_noreg = 0;
		dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
	}
	last_fec_bch = fec_bch;
}

int dtmb_constell_check(void)
{
	int constell;

	constell = dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT) >> 16 & 0x3;
	if (constell == 0)/*4qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x333221);
	else if (constell == 1)/*16qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x332821);
	else if (constell == 2)/*32qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331e21);
	else if (constell == 3)/*64qam*/
		dtmb_write_reg(DTMB_FRONT_47_CONFIG, 0x331a31);

	return 0;
}

int dtmb_check_fsm(void)
{
	int tmp, fsm_status, i, has_signal;

	tmp = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
	fsm_status =  tmp & 0xffffffff;
	has_signal = 0;
	PR_DTMB("fsm_status %x\n", fsm_status);
	for (i = 0 ; i < 8 ; i++) {
		if (((fsm_status >> (i * 4)) & 0xf) > 3) {
			/*has signal*/
		/*	PR_DTMB("has signal\n");*/
			has_signal = 1;
		}
	}

	return has_signal;
}

int patch_ts3(int delay1_us, int delay2_us)
{
	if (((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0) & 0xf) == 0x7) & 1) {
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x300f);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x310f);
		msleep(delay1_us);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0xffdfff);
		dtmb_write_reg(DTMB_TOP_CTRL_ENABLE, 0xffffff);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3110);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3010);
		dtmb_write_reg(DTMB_TOP_CTRL_FSM, 0x3000);
		return 1;
	} else {
		return 0;
	}
}

int read_cfo_all(void)
{
	int icfo_all, fcfo_all;

	icfo_all = dtmb_read_reg(DTMB_TOP_CTRL_ICFO_ALL) & 0xfffff;
	fcfo_all = dtmb_read_reg(DTMB_TOP_CTRL_FCFO_ALL) & 0x3fff;
	if (icfo_all > (1 << 19))
		icfo_all = icfo_all - (1 << 20);
	if (fcfo_all > (1 << 13))
		fcfo_all = fcfo_all - (1 << 14);

	return (int)(icfo_all * 4 + fcfo_all);
}

int dtmb_v3_soft_sync(int cfo_init)
{
/*	int cfo_all;*/
/*	int cfo_setting;*/

	if (cfo_init == 0)
		cfo_init = patch_ts3(11, 0);

	return cfo_init;
}

int dtmb_check_status_gxtv(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	int local_state;
	int time_cnt;/* cci_det, src_config;*/
	int cfo_init, count;

	dtmb_information(NULL);
	time_cnt = 0;
	local_state = 0;
	cfo_init = 0;
	if (check_dtmb_fec_lock() != 1) {
		dtmb_register_reset();
		dtmb_all_reset(demod);
		count = 15;
		while ((count) &&
		((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0) & 0xf) < 0x6)) {
			msleep(20);
			count--;
		}

		count = demod_sync_count;
		while ((count) && (cfo_init == 0)) {
			cfo_init = dtmb_v3_soft_sync(cfo_init);

			msleep(demod_sync_delay_time);
			count--;
		}
		if (cfo_init == 0 &&
			((dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0) & 0xf) <= 7)) {
			PR_DTMB("over 400ms,status %x, need reset\n",
				(dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0) & 0xf));
			return 0;
		}
		while ((time_cnt < 10) && (check_dtmb_fec_lock() != 1)) {
			msleep(demod_timeout);
			time_cnt++;
			local_state = AMLOGIC_DTMB_STEP3;
			dtmb_information(NULL);
			dtmb_check_cci();
			if (time_cnt > 8)
				PR_DTMB("local_state %d\n", local_state);
		}
		if (time_cnt >= 10 && (check_dtmb_fec_lock() != 1)) {
			local_state = AMLOGIC_DTMB_STEP4;
			time_cnt = 0;
			PR_DTMB("all reset,timeout %d\n", demod_timeout);
		}
	} else {
		dtmb_check_cci();
		dtmb_bch_check(fe);
	}

	if (check_dtmb_fec_lock() == 1)
		dtmb_write_reg(DTMB_TOP_CTRL_LOOP, 0xf);

	return 0;
}

int dtmb_check_status_txl(struct dvb_frontend *fe)
{
	struct aml_dtvdemod *demod = (struct aml_dtvdemod *)fe->demodulator_priv;
	int time_cnt;

	time_cnt = 0;
	dtmb_information(NULL);
	if (check_dtmb_fec_lock() != 1) {
		while ((time_cnt < 10) && (check_dtmb_fec_lock() != 1)) {
			msleep(demod_timeout);
			time_cnt++;
			dtmb_information(NULL);
			if (((dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT)
				>> 21) & 0x1) == 0x1) {
				PR_DTMB("4qam-nr,need set spectrum\n");
				if (demod->demod_status.spectrum == 1) {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010406);
				} else if (demod->demod_status.spectrum == 0) {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010402);
				} else {
					dtmb_write_reg
					(DTMB_TOP_CTRL_TPS, 0x1010002);
				}
			}
			if (time_cnt > 8)
				PR_DTMB("time_cnt %d\n", time_cnt);
		}
		if (time_cnt >= 10 && (check_dtmb_fec_lock() != 1)) {
			time_cnt = 0;
			dtmb_register_reset();
			dtmb_all_reset(demod);
			if (demod->demod_status.spectrum == 0)
				demod->demod_status.spectrum = 1;
			else
				demod->demod_status.spectrum = 0;
			PR_DTMB("all reset,timeout %d\n", demod_timeout);
		}
	} else {
		dtmb_bch_check(fe);
		dtmb_constell_check();
	}
	return 0;
}

void dtmb_no_signal_check_v3(struct aml_dtvdemod *demod)
{
	if (((dtmb_read_reg(DTMB_TOP_CTRL_CHE_WORKCNT)
		>> 21) & 0x1) == 0x1) {
		PR_DTMB("4qam-nr,need set spectrum\n");
		if (demod->demod_status.spectrum == 1) {
			dtmb_write_reg
			(DTMB_TOP_CTRL_TPS, 0x1010406);
		} else if (demod->demod_status.spectrum == 0) {
			dtmb_write_reg
			(DTMB_TOP_CTRL_TPS, 0x1010402);
		} else {
			dtmb_write_reg
			(DTMB_TOP_CTRL_TPS, 0x1010002);
		}
	}
}

void dtmb_no_signal_check_finishi_v3(struct aml_dtvdemod *demod)
{
	dtmb_register_reset();
	dtmb_all_reset(demod);
	if (demod->demod_status.spectrum == 0)
		demod->demod_status.spectrum = 1;
	else
		demod->demod_status.spectrum = 0;
}

void dtmb_reset(void)
{
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;

	sw_rst.b.ctrl_sw_rst = 1;
	sw_rst.b.ctrl_sw_rst_noreg = 1;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
	sw_rst.b.ctrl_sw_rst = 0;
	sw_rst.b.ctrl_sw_rst_noreg = 0;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
}

void dtmb_register_reset(void)
{
	union DTMB_TOP_CTRL_SW_RST_BITS sw_rst;

	sw_rst.b.ctrl_sw_rst = 1;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
	sw_rst.b.ctrl_sw_rst = 0;
	dtmb_write_reg(DTMB_TOP_CTRL_SW_RST, sw_rst.d32);
}

int dtmb_set_ch(struct aml_dtvdemod *demod,
		struct aml_demod_dtmb *demod_dtmb)
{
	int ret = 0;
	u8 demod_mode = 0;
	u8 ifreq = 0, agc_mode = 0;
	u32 ch_freq = 0;

	ifreq = demod_dtmb->ifreq;
	agc_mode = demod_dtmb->agc_mode;
	ch_freq = demod_dtmb->ch_freq;
	demod_mode = demod_dtmb->dat0;
	demod->demod_status.ch_mode = demod_dtmb->mode;	/* TODO */
	demod->demod_status.agc_mode = agc_mode;
	demod->demod_status.ch_freq = ch_freq;

	dtmb_initial(demod);

	//use top frontend
	if (demod_chip_after_eq(DTVDEMOD_HW_T6W)) {
		dtmb_write_reg(0xa, 0x00000011); //bypass local frontend

		front_write_reg(0x36, 0x0);//fsm config
		front_write_reg(0x37, 0x0);

		front_write_reg(0x20, (front_read_reg(0x20) | (1 << 17)));
		front_write_reg(0x20, (front_read_reg(0x20) | (1 << 18)));
		//app_apb_write_reg(0xe20,0x6011a);
		front_write_reg(0x21, 0x00010122);
		front_write_reg(0x22, 0x07200a16);
		front_write_reg(0x23, 0x42190190);
		front_write_reg(0x26, 0x1a000f0f);
		front_write_reg(0x28, 0x20003030);

		front_write_reg(0x2c, 0x8c042214);
		front_write_reg(0x2d, 0x00007011);
		front_write_reg(0x27, 0x03555555); //ddc
		front_write_reg(0x2e, 0x80400000);
		front_write_reg(0x2f, 0x00000004);
		front_write_reg(0x39, 0x40001000); //ddr  dvbc-wu

		if (demod->demod_status.ch_bw == 6000) { //6M BW
			front_write_reg(0x2a, 0x4204101a);
			//app_apb_write_reg(0xe2b, 0x301b6db7); //src
			front_write_reg(0x2b, 0x3021dcc8); //src---24m-6m
			front_write_reg(0x40, 0x061e81bc);

			front_write_reg(0x41, 0x1450a9);
			front_write_reg(0x42, 0x187b7);
			front_write_reg(0x43, 0x7977b0);
			front_write_reg(0x44, 0x7e901f);
			front_write_reg(0x45, 0x3c036);
			front_write_reg(0x46, 0x177f1);
			front_write_reg(0x47, 0x7d97d8);
			front_write_reg(0x48, 0x7ea006);
			front_write_reg(0x49, 0x1b020);
			front_write_reg(0x4a, 0x14000);
			front_write_reg(0x4b, 0x7ee7e7);
			front_write_reg(0x4c, 0x7ed7fd);
			front_write_reg(0x4d, 0xc014);
			front_write_reg(0x4e, 0x11006);
			front_write_reg(0x4f, 0x7f87f0);
			front_write_reg(0x50, 0x7f17f9);
			front_write_reg(0x51, 0x400d);
			front_write_reg(0x52, 0xe008);
			front_write_reg(0x53, 0x7fe7f7);
			front_write_reg(0x54, 0x7f47f8);
			front_write_reg(0x55, 0x7);
			front_write_reg(0x56, 0xa008);
			front_write_reg(0x57, 0x27fb);
			front_write_reg(0x58, 0x7f87f9);
			front_write_reg(0x59, 0x7fd003);
			front_write_reg(0x5a, 0x6006);
			front_write_reg(0x5b, 0x37ff);
			front_write_reg(0x5c, 0x7fc7fb);
			front_write_reg(0x5d, 0x7fc000);
			front_write_reg(0x5e, 0x3004);
			front_write_reg(0x5f, 0x4002);
			front_write_reg(0x60, 0x7ff7fe);
			front_write_reg(0x61, 0x7fe);
		} else { //8M BW
			front_write_reg(0x2a, 0x4404101a);
			//app_apb_write_reg(0xe2b, 0x301b6db7); //src
			front_write_reg(0x2b, 0x30196596); //src---24m-8m
			front_write_reg(0x40, 0x0629922b);

			front_write_reg(0x41, 0x123013);
			front_write_reg(0x42, 0x77e788);
			front_write_reg(0x43, 0x7ee046);
			front_write_reg(0x44, 0x4e013);
			front_write_reg(0x45, 0x7d47c8);
			front_write_reg(0x46, 0x7ef01f);
			front_write_reg(0x47, 0x2d012);
			front_write_reg(0x48, 0x7eb7dc);
			front_write_reg(0x49, 0x7f0010);
			front_write_reg(0x4a, 0x1f011);
			front_write_reg(0x4b, 0x7f57e7);
			front_write_reg(0x4c, 0x7f1008);
			front_write_reg(0x4d, 0x1600f);
			front_write_reg(0x4e, 0x7fc7ee);
			front_write_reg(0x4f, 0x7f3003);
			front_write_reg(0x50, 0x1000e);
			front_write_reg(0x51, 0x7ff7f3);
			front_write_reg(0x52, 0x7f4000);
			front_write_reg(0x53, 0xc00c);
			front_write_reg(0x54, 0x27f7);
			front_write_reg(0x55, 0x7f67ff);
			front_write_reg(0x56, 0x800a);
			front_write_reg(0x57, 0x37fa);
			front_write_reg(0x58, 0x7f87fe);
			front_write_reg(0x59, 0x5008);
			front_write_reg(0x5a, 0x47fd);
			front_write_reg(0x5b, 0x7fa7fd);
			front_write_reg(0x5c, 0x3006);
			front_write_reg(0x5d, 0x47fe);
			front_write_reg(0x5e, 0x7fb7fd);
			front_write_reg(0x5f, 0x2005);
			front_write_reg(0x60, 0x3000);
			front_write_reg(0x61, 0x7fd);
		}
	}

	if (demod->demod_status.ch_bw == 6000) { // 6M BW
		dtmb_write_reg(DTMB_FRONT_DDC_BYPASS, 0x006aaaab);  //(0x25)

		dtmb_write_reg(DTMB_FRONT_ACF_BYPASS, 0x1e81bc);    //(0x2a)
		dtmb_write_reg(DTMB_FRONT_COEF_SET1, 0x1450a9);     //(0x2b)
		dtmb_write_reg(DTMB_FRONT_COEF_SET2, 0x18);         //(0x2c)
		dtmb_write_reg(DTMB_FRONT_COEF_SET3, 0x3b7379);     //(0x2d)
		dtmb_write_reg(DTMB_FRONT_COEF_SET4, 0x1b0);        //(0x2e)
		dtmb_write_reg(DTMB_FRONT_COEF_SET5, 0x1e901f);     //(0x2f)
		dtmb_write_reg(DTMB_FRONT_COEF_SET6, 0x3c3617);     //(0x30)
		dtmb_write_reg(DTMB_FRONT_COEF_SET7, 0x71);         //(0x31)
		dtmb_write_reg(DTMB_FRONT_COEF_SET8, 0x59586a06);   //(0x32)
		dtmb_write_reg(DTMB_FRONT_COEF_SET9, 0x1b201400);   //(0x33)
		dtmb_write_reg(DTMB_FRONT_COEF_SET10, 0x2e272d);    //(0x34)
		dtmb_write_reg(DTMB_FRONT_COEF_SET11, 0x3d0c1411);  //(0x35)
		dtmb_write_reg(DTMB_FRONT_COEF_SET12, 0x06383031); //(0x36)
		dtmb_write_reg(DTMB_FRONT_COEF_SET13, 0x39040d0e);  //(0x37)
		dtmb_write_reg(DTMB_FRONT_COEF_SET14, 0x81e17);     //(0x38)
		dtmb_write_reg(DTMB_FRONT_COEF_SET15, 0x14180007);  //(0x39)
		dtmb_write_reg(DTMB_FRONT_COEF_SET16, 0xa08021b);   //(0x3a)
		dtmb_write_reg(DTMB_FRONT_COEF_SET17, 0x18191d03);  //(0x3b)
		dtmb_write_reg(DTMB_FRONT_COEF_SET18, 0x663fcbc);   //(0x3c)
		dtmb_write_reg(DTMB_FRONT_COEF_SET19, 0x03442fee);  //(0x3d)
		dtmb_write_reg(DTMB_FRONT_SRC_CONFIG1, 0x1321dcc8); //(0x3e)

		dtmb_write_reg(0x57, 0x0275025c); //(0x57)
		dtmb_write_reg(0x58, 0x022f008c); //(0x58)
		dtmb_write_reg(0x59, 0x0f2e00e2); //(0x59)
		dtmb_write_reg(0x5a, 0x07935023); //(0x5a)
		dtmb_write_reg(0x5b, 0x3c7b0a25); //(0x5b)
		dtmb_write_reg(0x5c, 0x10ee0a32); //(0x5c)
		dtmb_write_reg(0x5e, 0x042103b0); //(0x5e)
		dtmb_write_reg(0x5f, 0x07ed038a); //(0x5f)
		dtmb_write_reg(0x60, 0x07570347); //(0x60)
	}

	if (demod_chip_after_eq(DTVDEMOD_HW_T6W)) {
		front_write_reg(0x36, 0x3fffffff);//fsm config enable
		front_write_reg(0x37, 0x3fffffff);
	}
	return ret;
}

void dtmb_set_mem_st(unsigned int mem_start)
{
	dtmb_write_reg(DTMB_FRONT_MEM_ADDR, mem_start);
	PR_DTMB("[dtmb]mem start 0x%x, buf 0x%x\n",
			mem_start, dtmb_read_reg(DTMB_FRONT_MEM_ADDR));
}

int dtmb_read_agc(enum REG_DTMB_D9 type, unsigned int *buf)
{
	union DTMB_TOP_FRONT_AGC_BITS rval;

	rval.d32 = dtmb_read_reg(DTMB_TOP_FRONT_AGC);

	/*PR_DTMB("%s:type=%d,val=0x%x\n", __func__, type, rval.d32);*/
	/*PR_DTMB("if=0x%x,rf=0x%x,pw=0x%x", rval.b.front_agc_if_gain,*/
	/*		rval.b.front_agc_rf_gain,*/
	/*		rval.b.front_agc_power);*/
	switch (type) {
	case DTMB_D9_IF_GAIN:
		*buf = rval.b.front_agc_if_gain;
		break;
	case DTMB_D9_RF_GAIN:
		*buf = rval.b.front_agc_rf_gain;
		break;
	case DTMB_D9_POWER:
		*buf = rval.b.front_agc_power;
		break;
	case DTMB_D9_ALL:
		buf[0] = rval.b.front_agc_if_gain;
		buf[1] = rval.b.front_agc_rf_gain;
		buf[2] = rval.b.front_agc_power;
		break;
	}
	return 0;
}

unsigned int dtmb_reg_r_fec_lock(void)
{
	union DTMB_TOP_FEC_LOCK_SNR_BITS rval;
	unsigned int fec_lock;

	rval.d32 = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);

	if (demod_chip_eq(DTVDEMOD_HW_GXTVBB))
		fec_lock = rval.b_v2.fec_lock;
	else
		fec_lock = rval.b.fec_lock;

	return fec_lock;
}

unsigned int dtmb_reg_r_che_snr(void)
{
	union DTMB_TOP_FEC_LOCK_SNR_BITS rval;
	unsigned int che_snr;

	rval.d32 = dtmb_read_reg(DTMB_TOP_FEC_LOCK_SNR);

	if (demod_chip_eq(DTVDEMOD_HW_GXTVBB))
		che_snr = rval.b_v2.che_snr;
	else
		che_snr = rval.b.che_snr;

	return che_snr;
}

unsigned int dtmb_reg_r_bch(void)
{
	return dtmb_read_reg(DTMB_TOP_FEC_BCH_ACC);
}

/*move from dvb-core dvb_frontend.c dvb_frontend_swzigzag*/
/*1: timeout;2:have signal*/
unsigned int dtmb_detect_first(void)
{
	int has_signal = 0, i = 0;
	unsigned int dtmb_status;
	unsigned int timeout = 0;

	msleep(200);

	/*fsm status is 4,maybe analog signal*/
	dtmb_status = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
	PR_DTMB("fsm_status %x\n", dtmb_status);

	for (i = 0 ; i < 8 ; i++) {
		if (((dtmb_status >> (i * 4)) & 0xf) > 4) {
			/*has signal*/
		/*	dprintk("has signal\n");*/
			has_signal = 0x1;
		}
	}

	if (has_signal == 0x1) {
		/*fsm status is 6,digital signal*/
		/*fsm (1->4) 30ms,(4->5) 20ms,*/
		/*(5->6) 10ms,(6->7) 75ms,*/
		/*(7->8) 8ms,(8->9) 55ms, (9->a) 350ms*/
		msleep(500);
		dtmb_status = dtmb_read_reg(DTMB_TOP_CTRL_FSM_STATE0);
		PR_DTMB("fsm_status2 %x\n", dtmb_status);
		for (i = 0 ; i < 8 ; i++) {
			if (((dtmb_status >> (i * 4))
				& 0xf) > 6) {
				/*has signal*/
			/*	dprintk("has signal\n");*/
				has_signal = 0x3;
			}
		}
	}

	PR_DTMB("has_signal %d\n", has_signal);
	if (has_signal == 0 || has_signal == 0x1) {
		//timeout = 1;	/*FE_TIMEDOUT;*/
		timeout = 0;	/*FE_TIMEDOUT;*/
		PR_DTMB("timeout\n");
	} else {
		/*timeout = 2; *//*have signal*/
		PR_DTMB("have signal\n");
	}

	return timeout;
}

int dtmb_get_power_strength(int agc_gain)
{
	int strength = 0;
	int j = 0;
	static int calcE_R840[13] = {
		1010, 969, 890, 840, 800,
		760, 720, 680, 670, 660,
		510, 440, 368};

	for (j = 0; j < sizeof(calcE_R840) / sizeof(int); j++)
		if (agc_gain >= calcE_R840[j])
			break;

	if (agc_gain >= 440)
		strength = -90 + j * 3;
	else
		strength = -56;

	return strength;
}

int dtmb_convert_snr(int in_snr)
{
	int out_snr;
	int calce_snr[40] = {
		5, 6, 8, 10, 13,
		16, 20, 25, 32, 40,
		50, 63, 80, 100, 126,
		159, 200, 252, 318, 400,
		504, 634, 798, 1005, 1265,
		1592, 2005, 2524, 3177, 4000,
		5036, 6340, 7981, 10048, 12649,
		15924, 20047, 25238, 31773, 40000 };

	for (out_snr = 1; out_snr < 40; out_snr++)
		if (in_snr <= calce_snr[out_snr])
			break;

	return out_snr;
}
