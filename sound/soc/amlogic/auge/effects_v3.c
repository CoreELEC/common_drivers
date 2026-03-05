// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <sound/soc.h>
#include <sound/tlv.h>
#include <linux/clk-provider.h>
#include "effects_common.h"
#include "effects_v2.h"
#include "effects_v3.h"
#include "effects_hw_v3.h"
#include "effects_hw_v3_coeff.h"
#include "ddr_mngr.h"
#include "regs.h"
#include "iomap.h"

#define AED_EQ_COEFF_TOTAL_ELEM  (EQ_V3_BAND * (AED_FILTER_PARAM_SIZE + 1) + AED_EQ_FILTER_SIZE_CH)
#define AED_UINT_BYTE            (sizeof(unsigned int))
static DEFINE_MUTEX(aed_eq_coeff_lock);

void aed_v3_set_filter_data(void)
{
	unsigned int *p;
	int i, addr;

	/* set default filter param*/
	p = &AED_DC_CUT_COEFF[0];
	aed_v3_set_ram_coeff(AED_DC_CUT_RAM_ADD, AED_DC_FILTER_PARAM_SIZE, p);
	p = &AED_EQ_COEFF[0];
	aed_v3_set_ram_coeff(AED_EQ_FILTER_RAM_ADD, AED_EQ_FILTER_SIZE, p);
	p = &AED_3D_SURROUND_COEFF[0];
	aed_v3_set_ram_coeff(AED_3D_SURROUND_RAM_ADD, AED_3D_SURROUND_SIZE, p);
	for (i = 0; i < 4; i++) {
		p = &AED_CROSSOVER_COEFF[i * AED_CROSSOVER_FILTER_PARAM_SIZE];
		if (i == 0)
			addr = AED_CROSSOVER_FILTER_RAM_ADD +
				AED_CROSSOVER_FILTER_PARAM_SIZE;
		else if (i == 1)
			addr = AED_CROSSOVER_FILTER_RAM_ADD +
				2 * AED_CROSSOVER_FILTER_PARAM_SIZE;
		else if (i == 2)
			addr = AED_CROSSOVER_FILTER_RAM_ADD;
		else if (i == 3)
			addr = AED_CROSSOVER_FILTER_RAM_ADD +
				3 * AED_CROSSOVER_FILTER_PARAM_SIZE;

		aed_v3_set_ram_coeff(addr, AED_CROSSOVER_FILTER_PARAM_SIZE, p);
	}
	p = &AED_POST_EQ_COEFF[0];
	aed_v3_set_ram_coeff(AED_POST_EQ_FILTER_RAM_ADD, AED_POST_EQ_FILTER_SIZE, p);
}

void aed_v3_set_drc_data(void)
{
	unsigned int *p;

	/*set FDRC default value*/
	p = &AED_FULLBAND_DRC_COEFF[0];
	aed_v3_set_ram_coeff(AED_FULLBAND_FILTER_RAM_ADD, AED_FULLBAND_DRC_SIZE_V3, p);
	/*set MDRC default value*/
	p = &AED_MULTIBAND_DRC_COEFF[0];
	aed_v3_set_ram_coeff(AED_MULTIBAND_FILTER_RAM_ADD, AED_MULTIBAND_DRC_SIZE_V3, p);
}

static int mixer_get_level_meter_coeff(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = 0;

	eqdrc_write(AEQ_COEF_ADDR, reg);
	value = eqdrc_read(AEQ_COEF_DATA);
	ucontrol->value.integer.value[0] = value;

	return 0;
}

static int mixer_set_level_meter_coeff(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	unsigned int value = ucontrol->value.integer.value[0];

	eqdrc_write(AEQ_COEF_ADDR, reg);
	eqdrc_write(AEQ_COEF_DATA, value);
	eqdrc_write(AEQ_COEF_DATA, 0x0);

	return 0;
}

static int mixer_get_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &AED_EQ_COEFF[0];
	int i, j = 0;

	if (p_effect->aed_get_ram_mask & V3_EQ_FILTER_RAM_READ)
		aed_v3_get_ram_coeff(AED_EQ_FILTER_RAM_ADD, AED_EQ_FILTER_SIZE_CH, p);

	for (i = 0; i < (AED_EQ_FILTER_SIZE_CH - 1); i++) {
		j = i + 1;
		if (j % 6 == 0)
			p++;
		else
			*value++ = cpu_to_be32(*p++);
	}

	return 0;
}

static int mixer_set_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_FILTER_PARAM_SIZE + 1] = {0};
	char tmp_string[AED_FILTER_PARAM_BYTE + 1] = {0};
	unsigned int *p_data = tmp_data;
	char *p_string = tmp_string;
	unsigned int *p = NULL;
	size_t base_offset = 0;
	size_t copy_byte_len = AED_FILTER_PARAM_SIZE * AED_UINT_BYTE;
	bool stack_dirty = false;
	int num, band_id;

	int version = check_aed_version();
	char *val = (char *)ucontrol->value.bytes.data;

	if (version < 0) {
		pr_debug("Failed to obtain version: %d\n", version);
		return -EINVAL;
	}

	if (!kcontrol || !ucontrol || !val) {
		pr_debug("Invalid input pointer: kcontrol=%p, ucontrol=%p, val=%p\n",
				kcontrol, ucontrol, val);
		return -EINVAL;
	}

	if (!virt_addr_valid(val)) {
		pr_debug("Invalid kernel address: %p\n", val);
		return -EFAULT;
	}
	memcpy(p_string, val, AED_FILTER_PARAM_BYTE);
	stack_dirty = true;

	num = str2int(p_string, p_data, AED_FILTER_PARAM_BYTE);
	if (num < 0 || num != (AED_FILTER_PARAM_SIZE + 1)) {
		pr_debug("Invalid param num: %d (expected: %d)\n", num, AED_FILTER_PARAM_SIZE + 1);
		goto  err_clean;
	}

	band_id = tmp_data[0];
	if (band_id < 0 || band_id >= EQ_V3_BAND) {
		pr_debug("Invalid band_id: %d (max: %d)\n", band_id, EQ_V3_BAND - 1);
		goto  err_clean;
	}
	if (band_id > 13 && version == VERSION5)
		goto  err_clean;

	p_data = &tmp_data[1];
	base_offset = band_id * (AED_FILTER_PARAM_SIZE + 1);
	if (base_offset + AED_FILTER_PARAM_SIZE >= AED_EQ_COEFF_TOTAL_ELEM) {
		pr_debug("base_offset %zu out of range, total elem: %d\n",
				base_offset, AED_EQ_COEFF_TOTAL_ELEM);
		goto err_clean;
	}
	if (base_offset + AED_EQ_FILTER_SIZE_CH + AED_FILTER_PARAM_SIZE >=
							AED_EQ_COEFF_TOTAL_ELEM) {
		pr_debug("offset with FILTER_SIZE_CH out of range: %zu\n",
				base_offset + AED_EQ_FILTER_SIZE_CH);
		goto err_clean;
	}

	mutex_lock(&aed_eq_coeff_lock);

	/* base coeff */
	p = &AED_EQ_COEFF[base_offset];
	memcpy(p, p_data, copy_byte_len);

	/* FILTER_SIZE_CH coeff */
	p = &AED_EQ_COEFF[base_offset + AED_EQ_FILTER_SIZE_CH];
	memcpy(p, p_data, copy_byte_len);

	p = &AED_EQ_COEFF[base_offset];
	if (p) {
		aed_v3_set_ram_coeff(AED_EQ_FILTER_RAM_ADD + base_offset,
								AED_FILTER_PARAM_SIZE + 1, p);
	}

	p = &AED_EQ_COEFF[base_offset + AED_EQ_FILTER_SIZE_CH];
	if (p) {
		aed_v3_set_ram_coeff(AED_EQ_FILTER_RAM_ADD + AED_EQ_FILTER_SIZE_CH + base_offset,
						AED_FILTER_PARAM_SIZE + 1, p);
	}

	mutex_unlock(&aed_eq_coeff_lock);
	stack_dirty = false;
	pr_debug("EQ params set success: band_id=%d, version=%d\n", band_id, version);
	return 0;
err_clean:
	if (stack_dirty) {
		memset(tmp_data, 0, sizeof(tmp_data));
		memset(tmp_string, 0, sizeof(tmp_string));
	}
	return -EINVAL;
}

static int mixer_get_post_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &AED_POST_EQ_COEFF[0];
	int i, j = 0;

	if (p_effect->aed_get_ram_mask & V3_POST_EQ_FILTER_RAM_READ)
		aed_v3_get_ram_coeff(AED_POST_EQ_FILTER_RAM_ADD, AED_POST_EQ_FILTER_SIZE_CH, p);

	for (i = 0; i < (AED_POST_EQ_FILTER_SIZE_CH - 1); i++) {
		j = i + 1;
		if (j % 6 == 0)
			p++;
		else
			*value++ = cpu_to_be32(*p++);
	}

	return 0;
}

static int mixer_set_post_EQ_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_FILTER_PARAM_SIZE + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[AED_FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &AED_POST_EQ_COEFF[0];
	int num, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, AED_FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, AED_FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (AED_FILTER_PARAM_SIZE + 1) || band_id >= AED_POST_EQ_BAND) {
		pr_err("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &AED_POST_EQ_COEFF[band_id * (AED_FILTER_PARAM_SIZE + 1)];
	memcpy(p, p_data, AED_FILTER_PARAM_SIZE * UNSIGNED_INT_SIZE);
	memcpy(p + AED_POST_EQ_FILTER_SIZE_CH, p_data, AED_FILTER_PARAM_SIZE * UNSIGNED_INT_SIZE);

	p = &AED_POST_EQ_COEFF[band_id * (AED_FILTER_PARAM_SIZE + 1)];
	aed_v3_set_ram_coeff((AED_POST_EQ_FILTER_RAM_ADD +
		band_id * (AED_FILTER_PARAM_SIZE + 1)),
		AED_FILTER_PARAM_SIZE + 1, p);

	p = &AED_POST_EQ_COEFF[band_id * (AED_FILTER_PARAM_SIZE + 1) + AED_POST_EQ_FILTER_SIZE_CH];
	aed_v3_set_ram_coeff((AED_POST_EQ_FILTER_RAM_ADD +
		AED_POST_EQ_FILTER_SIZE_CH + band_id * (AED_FILTER_PARAM_SIZE + 1)),
		AED_FILTER_PARAM_SIZE + 1, p);

	return 0;
}

static int mixer_get_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &AED_CROSSOVER_COEFF[0];
	int i, j = 0;

	if (p_effect->aed_get_ram_mask & V3_CROSSOVER_FILTER_RAM_READ) {
		aed_v3_get_ram_coeff((AED_CROSSOVER_FILTER_RAM_ADD +
		AED_CROSSOVER_FILTER_PARAM_SIZE),
		AED_CROSSOVER_FILTER_PARAM_SIZE, p);

		p = &AED_CROSSOVER_COEFF[AED_CROSSOVER_FILTER_PARAM_SIZE];
		aed_v3_get_ram_coeff((AED_CROSSOVER_FILTER_RAM_ADD +
		2 * AED_CROSSOVER_FILTER_PARAM_SIZE),
		AED_CROSSOVER_FILTER_PARAM_SIZE, p);

		p = &AED_CROSSOVER_COEFF[2 * AED_CROSSOVER_FILTER_PARAM_SIZE];
		aed_v3_get_ram_coeff(AED_CROSSOVER_FILTER_RAM_ADD,
		AED_CROSSOVER_FILTER_PARAM_SIZE, p);

		p = &AED_CROSSOVER_COEFF[3 * AED_CROSSOVER_FILTER_PARAM_SIZE];
		aed_v3_get_ram_coeff((AED_CROSSOVER_FILTER_RAM_ADD +
		3 * AED_CROSSOVER_FILTER_PARAM_SIZE),
		AED_CROSSOVER_FILTER_PARAM_SIZE, p);
	}

	p = &AED_CROSSOVER_COEFF[0];
	for (i = 0; i < (AED_CROSSOVER_FILTER_SIZE - 1); i++) {
		j = i + 1;
		if (j % 6 == 0)
			p++;
		else
			*value++ = cpu_to_be32(*p++);
	}

	return 0;
}

static int mixer_set_crossover_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_FILTER_PARAM_SIZE + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[AED_FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &AED_CROSSOVER_COEFF[0];
	int num, band_id, addr;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, AED_FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, AED_FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != AED_FILTER_PARAM_SIZE + 1 ||
			band_id >= AED_CROSSOVER_FILTER_BAND) {
		pr_err("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &AED_CROSSOVER_COEFF[band_id * AED_CROSSOVER_FILTER_PARAM_SIZE];
	memcpy(p, p_data, AED_FILTER_PARAM_SIZE * UNSIGNED_INT_SIZE);

	p = &AED_CROSSOVER_COEFF[band_id * AED_CROSSOVER_FILTER_PARAM_SIZE];
	if (band_id == 0) {
		addr = AED_CROSSOVER_FILTER_RAM_ADD + AED_CROSSOVER_FILTER_PARAM_SIZE;
		aed_v3_set_ram_coeff(addr, AED_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 1) {
		addr = AED_CROSSOVER_FILTER_RAM_ADD + 2 * AED_CROSSOVER_FILTER_PARAM_SIZE;
		aed_v3_set_ram_coeff(addr, AED_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 2) {
		addr = AED_CROSSOVER_FILTER_RAM_ADD;
		aed_v3_set_ram_coeff(addr, AED_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 3) {
		addr = AED_CROSSOVER_FILTER_RAM_ADD + 3 * AED_CROSSOVER_FILTER_PARAM_SIZE;
		aed_v3_set_ram_coeff(addr, AED_FILTER_PARAM_SIZE + 1, p);
	}

	return 0;
}

static int mixer_get_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &AED_MULTIBAND_DRC_COEFF[0];
	int i, j = 0;

	if (p_effect->aed_get_ram_mask & V3_MULTIBAND_FILTER_RAM_READ)
		aed_v3_get_ram_coeff(AED_MULTIBAND_FILTER_RAM_ADD, AED_MULTIBAND_DRC_SIZE_V3, p);

	p = &AED_MULTIBAND_DRC_COEFF[0];
	for (i = 0; i < AED_MULTIBAND_DRC_SIZE_V3; i++) {
		j = i + 1;
		if (j % 12 == 0)
			p++;
		else
			*value++ = cpu_to_be32(*p++);
	}

	return 0;
}

static int mixer_set_multiband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_SINGLE_BAND_DRC_SIZE_V3 + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[AED_MULTIBAND_DRC_PARAM_BYTE_V3];
	char *p_string = &tmp_string[0];
	unsigned int *p = &AED_MULTIBAND_DRC_COEFF[0];
	int num, band_id;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, AED_MULTIBAND_DRC_PARAM_BYTE_V3);

	num = str2int(p_string, p_data, AED_MULTIBAND_DRC_PARAM_BYTE_V3);
	band_id = tmp_data[0];
	if (num != (AED_SINGLE_BAND_DRC_SIZE_V3 + 1) ||
			band_id >= AED_MULTIBAND_DRC_BANDS_V3) {
		pr_err("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &AED_MULTIBAND_DRC_COEFF[band_id * AED_SINGLE_BAND_DRC_SIZE_V3];
	memcpy(p, p_data, AED_SINGLE_BAND_DRC_SIZE_V3 * UNSIGNED_INT_SIZE);

	aed_v3_set_ram_coeff((AED_MULTIBAND_FILTER_RAM_ADD +
		band_id * AED_SINGLE_BAND_DRC_SIZE_V3),
		AED_SINGLE_BAND_DRC_SIZE_V3, p);

	return 0;
}

static int mixer_get_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &AED_FULLBAND_DRC_COEFF[0];
	int i;

	if (p_effect->aed_get_ram_mask & V3_FULLBAND_FILTER_RAM_READ)
		aed_v3_get_ram_coeff(AED_FULLBAND_FILTER_RAM_ADD, AED_FULLBAND_DRC_SIZE_V3, p);

	for (i = 0; i < AED_FULLBAND_DRC_SIZE_V3; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_fullband_DRC_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_FULLBAND_DRC_SIZE_V3 + 1] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[AED_FULLBAND_DRC_BYTES_V3];
	char *p_string = &tmp_string[0];
	unsigned int *p = &AED_FULLBAND_DRC_COEFF[0];
	int num, group;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, AED_FULLBAND_DRC_BYTES_V3);

	num = str2int(p_string, p_data, AED_FULLBAND_DRC_BYTES_V3);
	group = tmp_data[0];
	if (num != (AED_FULLBAND_DRC_SIZE_V3 + 1) ||
			group >= AED_FULLBAND_DRC_GROUP_SIZE_V3) {
		pr_err("Error: parma_num = %d, group = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &AED_FULLBAND_DRC_COEFF[0];
	memcpy(p, p_data, AED_FULLBAND_DRC_SIZE_V3 * UNSIGNED_INT_SIZE);

	p = &AED_FULLBAND_DRC_COEFF[0];
	aed_v3_set_ram_coeff(AED_FULLBAND_FILTER_RAM_ADD, AED_FULLBAND_DRC_SIZE_V3, p);

	return 0;
}

static int mixer_get_3D_Surround_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);
	unsigned int *value = (unsigned int *)ucontrol->value.bytes.data;
	unsigned int *p = &AED_3D_SURROUND_COEFF[0];
	int i;

	if (p_effect->aed_get_ram_mask & V3_3D_SURROUND_RAM_READ)
		aed_v3_get_ram_coeff(AED_3D_SURROUND_RAM_ADD, AED_3D_SURROUND_SIZE, p);

	for (i = 0; i < AED_3D_SURROUND_SIZE; i++)
		*value++ = cpu_to_be32(*p++);

	return 0;
}

static int mixer_set_3D_Surround_params(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	unsigned int tmp_data[AED_FILTER_PARAM_SIZE + 2] = {0};
	unsigned int *p_data = &tmp_data[0];
	char tmp_string[AED_3D_FILTER_PARAM_BYTE];
	char *p_string = &tmp_string[0];
	unsigned int *p = &AED_3D_SURROUND_COEFF[0];
	int num, band_id, addr;
	char *val = (char *)ucontrol->value.bytes.data;

	if (!val)
		return -ENOMEM;
	memcpy(p_string, val, AED_3D_FILTER_PARAM_BYTE);

	num = str2int(p_string, p_data, AED_3D_FILTER_PARAM_BYTE);
	band_id = tmp_data[0];
	if (num != (AED_FILTER_PARAM_SIZE + 2) || band_id >= AED_3D_SURROUND_BAND) {
		pr_err("Error: parma_num = %d, band_id = %d\n",
			num, tmp_data[0]);
		return 0;
	}

	p_data = &tmp_data[1];
	p = &AED_3D_SURROUND_COEFF[band_id * (AED_FILTER_PARAM_SIZE + 1)];
	memcpy(p, p_data, (AED_FILTER_PARAM_SIZE + 1) * UNSIGNED_INT_SIZE);

	p = &AED_3D_SURROUND_COEFF[band_id * (AED_FILTER_PARAM_SIZE + 1)];
	if (band_id == 0) {
		addr = AED_3D_SURROUND_RAM_ADD;
		aed_v3_set_ram_coeff(addr, AED_FILTER_PARAM_SIZE + 1, p);
	} else if (band_id == 1) {
		addr = AED_3D_SURROUND_RAM_ADD + AED_FILTER_PARAM_SIZE + 1;
		aed_v3_set_ram_coeff(addr, AED_FILTER_PARAM_SIZE + 1, p);
	}

	return 0;
}

static int v3_aed_get_ram_mask(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);

	ucontrol->value.integer.value[0] = p_effect->aed_get_ram_mask;
	return 0;
}

static int v3_aed_set_ram_mask(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect  = snd_kcontrol_chip(kcontrol);

	p_effect->aed_get_ram_mask = ucontrol->value.integer.value[0];

	return 0;
}

static const DECLARE_TLV_DB_SCALE(master_vol_tlv, -12276, 12, 1);
static const DECLARE_TLV_DB_SCALE(lr_vol_tlv, -12750, 50, 1);

static const char *const eq_src_texts[] = {"Lane0", "Lane1", "Lane2", "Lane3"};

static const struct soc_enum eq_source_sel_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0, ARRAY_SIZE(eq_src_texts),
			eq_src_texts);

static int aml_eq_lane_source_sel_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect = snd_kcontrol_chip(kcontrol);

	ucontrol->value.enumerated.item[0] = p_effect->eq_lane_sel;
	return 0;
}

static int aml_eq_lane_source_sel_set_enum
		(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct audioeffect *p_effect = snd_kcontrol_chip(kcontrol);
	int version;

	int val = ucontrol->value.enumerated.item[0];

	if (val < 0 || val > 4) {
		pr_info("Warning: eq_lane_sel val = 0x%x not support\n", val);
		return 0;
	}
	p_effect->eq_lane_sel = val;
	p_effect->lane_mask = 1U << p_effect->eq_lane_sel;

	version = check_aed_version();
	if (version < 0)
		pr_err("Failed to obtain version: %d\n", version);

	/*just only for v3 and check the version*/
	if (version > VERSION4)
		aed_set_lane_and_channels_v3(p_effect->lane_mask, p_effect->ch_mask);

	return 0;
}

static const struct snd_kcontrol_new snd_effect_v3_controls[] = {
	SOC_SINGLE_EXT_TLV("AED master volume",
			AED_EQ_VOLUME_VAL, 20, 0x3FF, 1,
			mixer_aed_read, mixer_aed_write,
			master_vol_tlv),

	SOC_SINGLE_EXT_TLV("AED Lch volume",
			AED_EQ_VOLUME_VAL, 0, 0x3FF, 1,
			mixer_aed_read, mixer_aed_write,
			master_vol_tlv),

	SOC_SINGLE_EXT_TLV("AED Rch volume",
			AED_EQ_VOLUME_VAL, 10, 0x3FF, 1,
			mixer_aed_read, mixer_aed_write,
			master_vol_tlv),

	SOC_SINGLE_EXT("AED Mute",
			AED_MUTE_CTRL, 2, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Level Meter enable",
			AED_STATUS_CTRL, 6, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED DC cut enable",
			AED_STATUS_CTRL, 0, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED 3D Surround enable",
			AED_STATUS_CTRL, 3, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED EQ enable",
			AED_STATUS_CTRL, 2, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Post EQ enable",
			AED_STATUS_CTRL, 1, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Multi-band DRC enable",
			AED_STATUS_CTRL, 7, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Full-band DRC enable",
			AED_STATUS_CTRL, 23, 0x1, 0,
			mixer_aed_read, mixer_aed_write),

	SND_SOC_BYTES_EXT("AED EQ Parameters",
			(AED_EQ_FILTER_SIZE_CH * 4),
			mixer_get_EQ_params,
			mixer_set_EQ_params),

	SND_SOC_BYTES_EXT("AED Post EQ Parameters",
			(AED_POST_EQ_FILTER_SIZE_CH * 4),
			mixer_get_post_EQ_params,
			mixer_set_post_EQ_params),

	SND_SOC_BYTES_EXT("AED Crossover Filter Parameters",
			(AED_CROSSOVER_FILTER_SIZE * 4),
			mixer_get_crossover_params,
			mixer_set_crossover_params),

	SND_SOC_BYTES_EXT("AED Multi-band DRC Parameters",
			(AED_MULTIBAND_DRC_SIZE_V3 * 4),
			mixer_get_multiband_DRC_params,
			mixer_set_multiband_DRC_params),

	SND_SOC_BYTES_EXT("AED Full-band DRC Parameters",
			AED_FULLBAND_DRC_BYTES_V3,
			mixer_get_fullband_DRC_params,
			mixer_set_fullband_DRC_params),

	SND_SOC_BYTES_EXT("AED 3D Surround Parameters",
			AED_3D_FILTER_PARAM_BYTE,
			mixer_get_3D_Surround_params,
			mixer_set_3D_Surround_params),

	SOC_SINGLE_EXT("AED Clip THD",
			AED_CLIP_THD_V3, 0, AED_CLIP_THD_MAX, 0,
			mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain LL",
		       AEQ_MIX0_LL, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain RL",
		       AEQ_MIX0_RL, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain LR",
		       AEQ_MIX0_LR, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Mixer Gain RR",
		       AEQ_MIX0_RR, 0, 0x3ffffff, 0,
		       mixer_aed_read, mixer_aed_write),

	SOC_SINGLE_EXT("AED Level Meter RMS Parameters",
			AED_LEVEL_METER_RAM_ADD, 0, 0xfffffff, 0,
			mixer_get_level_meter_coeff, mixer_set_level_meter_coeff),

	SND_SOC_BYTES_EXT("AED RAM Read Debug Mask",
			1,
			v3_aed_get_ram_mask,
			v3_aed_set_ram_mask),
	SOC_ENUM_EXT("AED Source Select",
			eq_source_sel_enum,
			aml_eq_lane_source_sel_get_enum,
			aml_eq_lane_source_sel_set_enum),
};

int add_effect_v3_kcontrols(struct snd_soc_card *card)
{
	unsigned int idx;
	int err;

	struct audioeffect *p_effect = get_audioeffects();

	for (idx = 0; idx < ARRAY_SIZE(snd_effect_v3_controls); idx++) {
		err = snd_ctl_add(card->snd_card,
				snd_ctl_new1(&snd_effect_v3_controls[idx],
					p_effect));
		if (err < 0)
			return err;
	}

	return 0;
}
