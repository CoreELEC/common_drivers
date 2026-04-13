// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#include "sed_hw.h"
#include "sed_filter.h"
#include <linux/amlogic/media/registers/cpu_version.h>

#define MEL_FILTER_COUNT 32

int sed_enable(struct regmap *audio_top_sed_map, int enable)
{
	if (IS_ERR_OR_NULL(audio_top_sed_map)) {
		pr_err("sed audio_top_sed regmap is null:%s\n", __func__);
		return -EINVAL;
	}
	regmap_update_bits(audio_top_sed_map, EE_AUDIO2_TOSED_CTRL0,
					   1 << 0,	enable << 0);
	return 0;
}

unsigned int sed_irq_status(struct regmap *audio_top_sed_map)
{
	unsigned int val;

	if (IS_ERR_OR_NULL(audio_top_sed_map)) {
		pr_info("audio_top_sed_map is null:%s\n", __func__);
		return -EINVAL;
	}

	regmap_read(audio_top_sed_map, SED_IRQ_STS, &val);
	/*wrtie will clear irq status*/
	regmap_write(audio_top_sed_map, SED_IRQ_STS, val);
	return val;
}

int sed_top_ctrl_init(struct regmap *audio_top_sed_map, int source)
{
	if (IS_ERR_OR_NULL(audio_top_sed_map)) {
		pr_info("sed regmap is null:%s\n", __func__);
		return -EINVAL;
	}
	regmap_update_bits(audio_top_sed_map, EE_AUDIO2_TOSED_CTRL0,
					   1 << 1, 1 << 1);

	/*pdm mic source */
	/*todo freeze tdm mic*/
	regmap_update_bits(audio_top_sed_map, EE_AUDIO2_TOSED_CTRL0,
							0x1f << 8, source << 8);

	regmap_write(audio_top_sed_map, EE_AUDIO2_TOSED_CH_EN, 0x1);
	return 0;
}

int pdm_dclk_coefficient(struct regmap *audio_top_sed_map, int bypss, int measure_clk)
{
	 //pdm dclk 0.768M, 30/0.768 =	39.0625  ->int 39
	//frac=round(  fraction(source_freq/tar_freq) * 256)
	// frac = 0.0625 * 256 = 16
	unsigned long long div_int = 39;
	unsigned long long div_frac = 16;
	unsigned long long rc = 1;
	unsigned long long int_div;
	unsigned long long sysclk = measure_clk;

	if (IS_ERR_OR_NULL(audio_top_sed_map)) {
		pr_info("sed regmap is null:%s\n", __func__);
		return -EINVAL;
	}
	if (bypss) {
		div_int = 1;
		div_frac = 0;
		rc = 0;
	} else {
		int_div = sysclk * 1000;
		int_div = div64_ul(int_div, 768000);
		div_int = div64_ul(sysclk, 768000);
		div_frac = int_div - (div_int * 1000);
		div_frac = div64_ul(div_frac * 256, 1000);
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_A9)
		rc = 0;
	regmap_update_bits(audio_top_sed_map, EE_AUDIO2_CLK_PDMIN_CTRL_0,
					0x1 << 28, rc  << 28);
	regmap_update_bits(audio_top_sed_map, EE_AUDIO2_CLK_PDMIN_CTRL2,
					0xff << 16, div_frac << 16);
	regmap_update_bits(audio_top_sed_map, EE_AUDIO2_CLK_PDMIN_CTRL2,
					0xffff << 0, div_int << 0);
	return 0;
}

int sed_clk_set(struct regmap *sed_map, int measure_clk)
{
	if (IS_ERR_OR_NULL(sed_map)) {
		pr_info("sed regmap is null:%s\n", __func__);
		return -EINVAL;
	}
	regmap_write(sed_map, SED_SYS_CFG_REG0, 0x1);
	regmap_write(sed_map, SED_SYS_CFG_REG1, 0x1);
	regmap_update_bits(sed_map, SED_CFG_REG1, 0xffff << 0, (measure_clk / 1000) << 0);

	return 0;
}

void sed_parameter_set(struct sed_cfg_t *cfg)
{
	cfg->preemp_coeff = 0x3E6;
	cfg->hop_length = 0x0;
	cfg->win_length = 0x0;
	cfg->pow_diff_dist = 0x1;
	cfg->pow_old_avglen = 0x1;
	cfg->pow_rampup_thr = 0x404A;
	cfg->pow_cur_thr = 0x20BB;
	cfg->pow_cur_thr_high = 0x4120;
	cfg->pow_freq_start = 0x0;
	cfg->pow_freq_end = 0x100;
	cfg->pow_discard_len = 0x0A;
	cfg->pow_traceback_len = 0x1E;
	cfg->pow_delay_len = 0xE1;
	cfg->log_input_min = 0x2180;
	cfg->log_zeropoint = 0x006A;
	cfg->log_scale_r = 0x407A;
	cfg->nn_stride_frame = 0x2;
	cfg->nn_post_frame = 0x5;
}

int sed_mel_filter_set(struct sed_cfg_t *cfg, struct regmap *sed_map, int rate)
{
	u32 b_win_gap;
	u32 wr_data;
	unsigned int reg, val;
	const u32 *down_sample = resample_16k;

	if (IS_ERR_OR_NULL(sed_map)) {
		pr_info("sed regmap is null:%s\n", __func__);
		return -EINVAL;
	}
	b_win_gap = cfg->hop_length == 1 ? 19 : 9;
	wr_data = cfg->downsample_rate << 16 | cfg->preemp_coeff << 1 |
				cfg->hop_length << 18 | cfg->win_length << 19 |
				b_win_gap << 20 | 0x1 << 0;

	regmap_write(sed_map, SED_CFG_REG0, wr_data);

	if (rate == 48000)
		down_sample = resample_48k;
	else if (rate == 16000)
		down_sample = resample_16k;

	for (int i = 0; i < 12; i++) {
		reg = SED_DSM_COEF0 + (i * 4);
		val = (down_sample[(2 * i) + 1] << 16) | (down_sample[2 * i] << 0);
		regmap_write(sed_map, reg, val);
	}
	regmap_write(sed_map, SED_DSM_COEFC, down_sample[24] << 0);

	for (int i = 0; i < MEL_FILTER_COUNT; i++) {
		regmap_write(sed_map, MEL_FILTER_REG + i * 8,
					sed_filter_table[4 * i] |
					sed_filter_table[4 * i + 1] << 16);
		regmap_write(sed_map, MEL_FILTER_REG + i * 8 + 4,
					sed_filter_table[4 * i + 2] |
					sed_filter_table[4 * i + 3] << 16);
	}
	// set pow_discard_len=0 to speed up simulation generate pow_flag
	cfg->pow_discard_len = 0x0;
	cfg->pow_delay_len = 0x0;
	cfg->pow_traceback_len = 0x80;
	regmap_write(sed_map, SED_NN_CTRL0,
		(cfg->pow_traceback_len << 0) |
		(cfg->pow_discard_len << 16) | (cfg->pow_delay_len << 8));

	//config sed_pow_ctrl0: pow_old_avglen,pow_diff_dist,pow_rampup_thr
	wr_data = cfg->pow_rampup_thr << 0 |
	(4 + 4 * cfg->pow_diff_dist) << 16 | cfg->pow_old_avglen << 24;
	regmap_write(sed_map, SED_POW_CTRL0, wr_data);

	//config sed_pow_ctrl1: pow_cur_thr_high, pow_cur_thr
	//set pow_cur_thr_high=0 to speed up simulation generate pow_flag
	//cfg->pow_cur_thr_high = 0x;
	wr_data = cfg->pow_cur_thr << 0 | cfg->pow_cur_thr_high << 16;
	regmap_write(sed_map, SED_POW_CTRL1, wr_data);

	//config sed_pow_ctrl2: pow_end_addr, pow_start_addr
	wr_data = cfg->pow_freq_start << 0 | cfg->pow_freq_end << 16;
	regmap_write(sed_map, SED_POW_CTRL2, wr_data);

	//config sed_log_ctrl0: log_input_min
	wr_data = cfg->log_input_min <<  0;
	regmap_write(sed_map, SED_LOG_CTRL0, wr_data);

	//config sed_log_ctrl1: log_scale, log_zeropoint
	wr_data = (cfg->log_zeropoint << 0) | (cfg->log_scale_r << 16);
	regmap_write(sed_map, SED_LOG_CTRL1, wr_data);
	regmap_write(sed_map, SED_IRQ_EN, 0x3fffffff);
	return 0;
}

int sed_chain_set(struct regmap *sed_map)
{
	u32 chain_addr;
	u32 run_num;
	u32 run_skip;
	u32 run_imm;
	u32 pending;

	chain_addr = 0;
	run_num = 5;
	run_skip = 2;
	run_imm = 0;
	pending = 0;
	regmap_write(sed_map, SED_CMD_CHAIN,
	chain_addr << 0 | run_num << 12 |
	run_skip << 20 | run_imm << 28 | pending << 31);
	return 0;
}

int sed_load_bin(struct regmap *sed_map, const unsigned int *p_data, int len)
{
	unsigned int reg;
	const unsigned int *p = p_data;

	if (IS_ERR_OR_NULL(sed_map)) {
		pr_info("sed regmap is null:%s\n", __func__);
		return -EINVAL;
	}
	for (int i = 0; i < len; i ++, p++) {
		reg = SED_LOAD_MEMORY + i * 4;
		regmap_write(sed_map, reg, *p);
	}
	return 0;
}

