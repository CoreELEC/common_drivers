// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include<linux/delay.h>
#include "mixer_hw.h"
#include "earc.h"

static BLOCKING_NOTIFIER_HEAD(mixer_notifier_list);

int mixer_register_client(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&mixer_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(mixer_register_client);

int mixer_notifier_audioclient(unsigned long val, void *v)
{
	return blocking_notifier_call_chain(&mixer_notifier_list, val, v);
}
EXPORT_SYMBOL_GPL(mixer_notifier_audioclient);

static struct regmap *mixer_reg_map;
int set_mixer_regmap(struct regmap *mixer_reg)
{
	mixer_reg_map = mixer_reg;
	return 0;
}

int mixer_format_set(int channel, int sample_bit, int fddr_type, int mixer_lane)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0xff << 16, (channel - 1) << 16);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x7 << 20, fddr_type << 20);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0xf << 16, channel << 16);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x7 << 24, mixer_lane << 24);
	background_mixer_ch_set(mixer_lane);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1f << 24, 0x1 << 24);

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1f << 11, (sample_bit - 1) << 11);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x7 << 8, fddr_type << 8);

	if (channel == 2)
		mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0xff << 0, 0x1 << 0);
	else if (channel == 8)
		mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0xff << 0, 0xf << 0);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0x1 << 8, 0x1 << 8);

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 30, 0 << 30);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_format_set);

int mixer_fifo_reset(void)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 29, 0x1 << 29);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 29, 0x0 << 29);

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0x1 << 28, 0x0 << 28);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0x1 << 29, 0x0 << 29);

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0x1 << 28, 0x1 << 28);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0x1 << 29, 0x1 << 29);

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1 << 31, 0x1 << 31);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1 << 31, 0x0 << 31);

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1 << 30, 0x0 << 30);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1 << 29, 0x0 << 29);

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1 << 30, 0x1 << 30);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL2, 0x1 << 29, 0x1 << 29);

	return 0;
}
EXPORT_SYMBOL_GPL(mixer_fifo_reset);

int mixer_source_set(int fifo_id)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x7 << 0, fifo_id << 0);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_source_set);

static inline unsigned int
	calc_frddr_address(unsigned int reg, unsigned int base)
{
	return base + reg - EE_AUDIO_FRDDR_A_CTRL0;
}

int mixer_eq_enable(struct frddr *fr, int enable)
{
	unsigned int reg_base = fr->reg_base;
	unsigned int reg;

	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	reg = calc_frddr_address(EE_AUDIO_FRDDR_A_CTRL2, reg_base);
	aml_audiobus_update_bits(fr->actrl,
				reg, 0x1 << 3, 0 << 3);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0x1 << 27, 0x1 << 27);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0x7 << 24, 0x6 << 24);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 3, 0 << 3);
	eqdrc_update_bits(AED_TOP_CTL0, 0x1 << 14, enable << 14);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL4, 0x1 << 3, enable << 3);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_eq_enable);

int mixer_demux_sel(enum frddr_dest dst, int enable)
{
	unsigned int offset, reg, index = 0;

	index = dst;
	if (dst <= TDMOUT_C || dst == TDMOUT_D) {
		if (dst == TDMOUT_D)
			index = 3;
		if (index == 3) {
			reg = EE_AUDIO_TDMOUT_D_CTRL1;
		} else {
			offset = EE_AUDIO_TDMOUT_B_CTRL1
						- EE_AUDIO_TDMOUT_A_CTRL1;
			reg = EE_AUDIO_TDMOUT_A_CTRL1 + offset * index;
		}
		audiobus_update_bits(reg, 0x1 << 1, enable << 1);
	} else if (dst >= SPDIFOUT_A && dst <= SPDIFOUT_B) {
		if (dst == SPDIFOUT_A)
			index = 0;
		else if (dst == SPDIFOUT_B)
			index = 1;
		offset = EE_AUDIO_SPDIFOUT_B_CTRL1 - EE_AUDIO_SPDIFOUT_CTRL1;
		reg = EE_AUDIO_SPDIFOUT_CTRL1 + offset * index;
		audiobus_update_bits(reg, 0x1 << 1, enable << 1);
	} else if (dst == EARCTX_DMAC) {
		earc_mixer_enable(enable);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_demux_sel);

int mic_mixer_source(int fifo_id)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}

	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x7 << 4, fifo_id << 4);
	return 0;
}
EXPORT_SYMBOL_GPL(mic_mixer_source);

int mic_mixer_fifo_reset(void)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1 << 31, 0x1 << 31);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1 << 31, 0x0 << 31);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1 << 30, 0x0 << 30);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1 << 29, 0x0 << 29);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1 << 30, 0x1 << 30);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1 << 29, 0x1 << 29);
	return 0;
}
EXPORT_SYMBOL_GPL(mic_mixer_fifo_reset);

int mic_mixer_format_set(int sample_bit, int fddr_type, int channel)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1f << 24, 0x1 << 24);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1f << 11, (sample_bit - 1) << 11);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x7 << 8, fddr_type << 8);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL3, 0x1f << 24, (channel - 1) << 24);
	return 0;
}
EXPORT_SYMBOL_GPL(mic_mixer_format_set);

int mixer_coef_set(struct regmap *coef_reg, unsigned int vol)
{
	if (IS_ERR_OR_NULL(coef_reg)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W) {
		mmio_write(coef_reg, AUDIO_MIXER_COEF0_A, vol);
		mmio_write(coef_reg, AUDIO_MIXER_COEF1_A, vol);
		mmio_write(coef_reg, AUDIO_MIXER_COEF2_A, vol);
		mmio_write(coef_reg, AUDIO_MIXER_COEF3_A, vol);
	}
	mmio_write(coef_reg, AUDIO_MIXER_COEF0_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF1_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF2_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF3_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF4_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF5_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF6_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF7_A, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF0_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF1_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF2_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF3_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF4_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF5_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF6_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF7_B, vol);

	return 0;
}
EXPORT_SYMBOL_GPL(mixer_coef_set);

int mixer_mic_coef_set(struct regmap *coef_reg, unsigned int vol)
{
	if (IS_ERR_OR_NULL(coef_reg)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W) {
		mmio_write(coef_reg, AUDIO_MIXER_COEF1_A, vol);
		mmio_write(coef_reg, AUDIO_MIXER_COEF3_A, vol);
		return 0;
	}
	mmio_write(coef_reg, AUDIO_MIXER_COEF0_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF1_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF2_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF3_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF4_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF5_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF6_B, vol);
	mmio_write(coef_reg, AUDIO_MIXER_COEF7_B, vol);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_mic_coef_set);

int mixer_en(int enable)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	pr_debug("%s:en:%d, mixer status:%x, aed status:%0x\n", __func__, enable,
		audiobus_read(EE_AUDIO_MIXER_STATUS1), eqdrc_read(AED_AED_TOP_ST));
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 31, enable << 31);

	if (enable)
		mixer_notifier_audioclient(BACKGROUND_RESET_CMD, NULL);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_en);

int mixer_clip_top_en(int enable)
{
	unsigned int status = 0, cnt = 0;
	unsigned int status_reg = 0;

	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W)
		status_reg = AUDIO_MIXER_STATUS1_T6D;
	else
		status_reg = AUDIO_MIXER_STATUS1;

	if (get_cpu_type() != MESON_CPU_MAJOR_ID_T6D) {
		if (enable) {
			while (1) {
				status = mmio_read(mixer_reg_map, status_reg);
				if (status & (1 << 28))
					break;
				usleep_range(20, 25);
				cnt++;
				if (cnt > 20) {
					pr_err("mic fddr no init done!\n");
					break;
				}
			}
		}

	}
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 30, enable << 30);
	return 0;
}
EXPORT_SYMBOL_GPL(mixer_clip_top_en);

int background_mixer_en(int enable)
{
	unsigned int status = 0, cnt = 0;
	unsigned int status_reg = 0;

	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W)
		status_reg = AUDIO_MIXER_STATUS1_T6D;
	else
		status_reg = AUDIO_MIXER_STATUS1;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D)
		return 0;
	if (enable) {
		while (1) {
			status = mmio_read(mixer_reg_map, status_reg);
			if (status & (1 << 27))
				break;
			usleep_range(20, 25);
			cnt++;
			if (cnt > 20) {
				pr_err("background fddr no init done!\n");
				break;
			}
		}
	}
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 9, 0 << 9);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL0, 0x1 << 23, enable << 23);
	return 0;
}
EXPORT_SYMBOL_GPL(background_mixer_en);

int mixer_lane_source(struct regmap *coef_reg, int lane_a, int lane_b)
{
	int lane_offset = 2 * lane_a;

	if (IS_ERR_OR_NULL(coef_reg) || IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W)
		return 0;
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL5, 0x3 << lane_offset,
		lane_b << lane_offset);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL5, 0x1 << lane_a, 1 << lane_a);
	return 0;
}

int mic_mixer_ch_set(int channel)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W)
		return 0;
	if (channel == 2)
		mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0xff << 8, 0x1 << 8);
	else if (channel == 8)
		mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL1, 0xff << 8, 0xf << 8);
	return 0;
}

int background_mixer_ch_set(int lane)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T6D ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T6W)
		return 0;
	/*clear lane source before set*/
	mmio_write(mixer_reg_map, AUDIO_MIXER_CTRL5, 0);
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL5, 0x1 << lane, 1 << lane);
	if (lane == 4)
		mmio_write(mixer_reg_map, AUDIO_MIXER_CTRL5, 0x00000e4f);
	return 0;
}

int mixer_samesource_ctrl_write(unsigned int mask, unsigned int value)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	mmio_update_bits(mixer_reg_map, AUDIO_MIXER_CTRL4, mask, value);
	return 0;
}

int mixer_samesource_ctrl_read(void)
{
	if (IS_ERR_OR_NULL(mixer_reg_map)) {
		pr_err("%s err\n", __func__);
		return 0;
	}
	return mmio_read(mixer_reg_map, AUDIO_MIXER_CTRL4);
}
