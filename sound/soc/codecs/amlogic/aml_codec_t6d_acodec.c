// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/regmap.h>
#include <linux/amlogic/iomap.h>
#include <linux/amlogic/media/sound/auge_utils.h>
#include <linux/of_device.h>
#include <linux/of_address.h>

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
#include "../../../soc/amlogic/auge/iomap.h"
#include "../../../soc/amlogic/auge/regs.h"
#include "aml_codec_t6d_acodec.h"

struct am_acodec_chipinfo {
	int id;
	bool is_bclk_cap_inv;
	bool is_bclk_o_inv;
	bool is_lrclk_inv;
	bool is_codec_sw_cap_inv; //default true
	int mclk_sel;
	bool separate_toacodec_en;
	int dac_num;
	int lineout_num;
};

struct am_acodec_priv {
	struct snd_soc_component *component;
	struct snd_pcm_hw_params *params;
	struct regmap *regmap;
	struct regmap *to_acodec_regmap;
	struct regmap *to_acodec_adc_regmap;
	struct work_struct work;
	struct am_acodec_chipinfo *chipinfo;
	struct clk *acodec_clk;
	int tdmout_index;
	int dat0_ch_sel;
	int dat1_ch_sel;

	int tdmin_index;
	int adc_output_sel;
	int dac1_input_sel;
	int dac2_input_sel;
	struct reset_control *rst;
	int diff_output;
	int diff_input;
	int dac_extra_gain;
	int dac_output_invert;
	int lane_offset;
	int adc_pga_gain;
	/* store user setting */
	u32 user_setting[ACODEC_REG_NUM];
	int chip_version;
	/*the headphone mode capacitance*/
	int charger_current_cap;
	/*LP_RP_EN */
	int headphone_pin;
	int lineout_pin;
	/*headphone and lineout output*/
	int output_type;
	int input_sel;
	int headphone_mute;
	int suspend_ref;
	int resume_ref;
	int early_suspend;
	int headphone_depop;
	struct work_struct mute_work;
	int headphone_power_on;
};

enum meson_acodec_version {
	MESON_ACODEC_ID_VERSION_T6D,
	MESON_ACODEC_ID_VERSION_T6W,
	MESON_ACODEC_ID_VERSION_T6X,
	MESON_ACODEC_ID_VERSION_C4,
};

enum input_pin_sel {
	LP_RP = 0x0, //0x5
	LP_RN,       //0x9
	LN_RP,       //0x6
	LN_RN,       //0xa
	LR_PN        //0xf
};

enum output_pin_sel {
	LP_RP_EN = 0x0,
	LP_RN_EN,
	LN_RP_EN,
	LN_RN_EN,
	LR_PN_EN
};

enum output_type_sel {
	DUAL_OUTPUT = 0x0,
	HEADPHONE_SINGLE,
	LINEOUT_SINGLE,
	DIFF_LINEOUT
};

enum charge_cap_level {
	CAP_1_4UF	 = 0x0,
	CAP_4_8UF	 = 0x1,
	CAP_8_30UF	 = 0x2,
	CAP_30_50UF  = 0x4,
	CAP_50_70UF  = 0x8,
	CAP_70_100UF = 0xf
};

static const struct reg_default t6d_acodec_init_list[] = {
	{ACODEC_0, 0x3400bff0},
	{ACODEC_1, 0x46460b0b},
	{ACODEC_2, 0xf8f87000},
	/*default LP/RP(headphone)	 LN/RN(lineout->spk)*/
	/*dual output*/
	{ACODEC_3, 0x00034422},
	{ACODEC_4, 0x00020000},
	{ACODEC_5, 0xffff0033},
	{ACODEC_6, 0x0},
	{ACODEC_7, 0x0},
	{ACODEC_8, 0x14000000},
	{ACODEC_ALC_0, 0x80001000},
	{ACODEC_ALC_1, 0x7d03e8},
	{ACODEC_ALC_2, 0x68b973},
	{ACODEC_ALC_3, 0x44e161},
	{ACODEC_ALC_4, 0x15165},
	{ACODEC_ALC_5, 0x6bb2d6},
	{ACODEC_ALC_6, 0x8020d7},
	{ACODEC_ALC_7, 0x4ae9b},
	{ACODEC_ALC_8, 0x1f412c},
	{ACODEC_ALC_9, 0xa33e83e8},
	{ACODEC_ALC_10, 0x800000},
};

static struct am_acodec_chipinfo acodec_cinfo_v3 = {
	.id = 0,
	.is_bclk_cap_inv = true,
	.is_bclk_o_inv = false,
	.is_lrclk_inv = false,

	.separate_toacodec_en = true,
	.dac_num = 2,
	.lineout_num = 4,
};

static void aml_am_acodec_dac_extra_gain_set(struct snd_soc_component *component,
		int dac_index, int value)
{
	u32 reg_addr = 0;
	u32 val = 0;

	if (dac_index == 0) {
		reg_addr = ACODEC_1;
		val = snd_soc_component_read(component, reg_addr);

		if (value == 0) {
			val &= ~(0x1 << REG_DAC_GAIN_SEL_1);
			val &= ~(0x1 << REG_DAC_GAIN_SEL_0);
		} else if (value == 1) {
			val &= ~(0x1 << REG_DAC_GAIN_SEL_1);
			val |= (0x1 << REG_DAC_GAIN_SEL_0);
		} else if (value == 2) {
			val |= (0x1 << REG_DAC_GAIN_SEL_1);
			val &= ~(0x1 << REG_DAC_GAIN_SEL_0);
		} else if (value == 3) {
			val |= (0x1 << REG_DAC_GAIN_SEL_1);
			val |= (0x1 << REG_DAC_GAIN_SEL_0);
		}
		snd_soc_component_write(component, reg_addr, val);
	} else if (dac_index == 1) {
		reg_addr = ACODEC_7;
		val = snd_soc_component_read(component, reg_addr);

		if (value == 0) {
			val &= ~(0x1 << REG_DAC2_GAIN_SEL_1);
			val &= ~(0x1 << REG_DAC2_GAIN_SEL_0);
		} else if (value == 1) {
			val &= ~(0x1 << REG_DAC2_GAIN_SEL_1);
			val |= (0x1 << REG_DAC2_GAIN_SEL_0);
		} else if (value == 2) {
			val |= (0x1 << REG_DAC2_GAIN_SEL_1);
			val &= ~(0x1 << REG_DAC2_GAIN_SEL_0);
		} else if (value == 3) {
			val |= (0x1 << REG_DAC2_GAIN_SEL_1);
			val |= (0x1 << REG_DAC2_GAIN_SEL_0);
		}
		snd_soc_component_write(component, reg_addr, val);
	}
}

static void output_path_set(struct snd_soc_component *component,
		int version, enum output_pin_sel pin, int enable)
{
	switch (pin) {
	case LP_RP_EN:
		snd_soc_component_update_bits(component, ACODEC_3, 0X7 << 4,
						enable << LOLP_SEL_DACL);
		snd_soc_component_update_bits(component, ACODEC_3, 0X7 << 0,
						enable << LORP_SEL_DACR);
		break;
	case LP_RN_EN:
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 4,
						enable << LOLP_SEL_DACL);
		snd_soc_component_update_bits(component, ACODEC_3, 0X7 << 8,
						enable << LORN_SEL_DACR);
		break;
	case LN_RP_EN:

		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 12,
						enable << LOLN_SEL_DACL);
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 0,
						enable << LORP_SEL_DACR);
		break;
	case LN_RN_EN:
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 12,
						enable << LOLN_SEL_DACL);
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 8,
						enable << LORN_SEL_DACR);
		break;
	case LR_PN_EN:
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 0,
						enable << LORP_SEL_DACR);
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 4,
						enable << LOLP_SEL_DACL);
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 12,
						enable << LOLN_SEL_DACL_INV);
		snd_soc_component_update_bits(component, ACODEC_3, 0x7 << 8,
						enable << LORN_SEL_DACR_INV);
		break;
	default:
		break;
	}
}

static void acodec_dac_extra_gain_set(struct snd_soc_component *component, int version)
{
	struct am_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);

	aml_am_acodec_dac_extra_gain_set(component, 0, aml_acodec->dac_extra_gain);
}

static void acodec_adc_pga_gain_set(struct snd_soc_component *component, int version)
{
	struct am_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);

	snd_soc_component_update_bits(component, ACODEC_1, 0x1f << 8 | 0x1f,
		aml_acodec->adc_pga_gain << 8 | aml_acodec->adc_pga_gain);

}

static void output_pin_enable(struct snd_soc_component *component,
	enum output_pin_sel pin, int enable)
{
	unsigned int mask = 0, val = 0;

	switch (pin) {
	case LP_RP_EN:
		mask = 1 << LOLP_EN | 1 << LORP_EN;
		val = enable << LOLP_EN | enable << LORP_EN;
		break;
	case LP_RN_EN:
		mask = 1 << LOLP_EN | 1 << LORN_EN;
		val = enable << LOLP_EN | enable << LORN_EN;
		break;
	case LN_RP_EN:
		mask = 1 << LOLN_EN | 1 << LORP_EN;
		val = enable << LOLN_EN | enable << LORP_EN;
		break;
	case LN_RN_EN:
		mask = 1 << LOLN_EN | 1 << LORN_EN;
		val = enable << LOLN_EN | enable << LORN_EN;
		break;
	case LR_PN_EN:
		mask = 0xf;
		val = enable ? 0xf : 0;
		break;
	default:
		mask = 0xf;
		val = enable ? 0xf : 0;
		break;
	}
	snd_soc_component_update_bits(component, ACODEC_0, mask,
							val);
}

#ifdef AUDIO_DEBUG_POP
static void drop_pop_mode_0(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);
	snd_soc_component_update_bits(component, ACODEC_3, 1 << DISCHARGE_CURRENT,
							1 << DISCHARGE_CURRENT);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << EN_CHARGE,
							1 << EN_CHARGE);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << EN_C_ONCE,
							0 << EN_C_ONCE);
	snd_soc_component_update_bits(component, ACODEC_8, 0xf << CHARGE_CURRENT_CAP,
			aml_acodec->charger_current_cap << CHARGE_CURRENT_CAP);
	msleep(500);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << TIME_OUT,
							1 << TIME_OUT);
}
#endif

static void drop_pop_mode_1(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);
	enum charge_cap_level level = aml_acodec->charger_current_cap;

	snd_soc_component_update_bits(component, ACODEC_3, 1 << DISCHARGE_CURRENT,
							0 << DISCHARGE_CURRENT);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << EN_CHARGE,
							1 << EN_CHARGE);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W)
		snd_soc_component_update_bits(component, ACODEC_8, 1 << EN_C_ONCE,
							0 << EN_C_ONCE);
	else
		snd_soc_component_update_bits(component, ACODEC_8, 1 << EN_C_ONCE,
							1 << EN_C_ONCE);
	snd_soc_component_update_bits(component, ACODEC_8, 0xf << CHARGE_CURRENT_CAP,
						level << CHARGE_CURRENT_CAP);
}

/* t0:acodec0 [15]: 0, [14]:1 ,acodec3 [22] :1
 * t1(400ms): acodec0 [1:0]:3  unmute
 * t2(410ms): acodec0 [15]:1
 * t3(700ms): acodec0 [14]:0, acodec3 [22] :0
 */

/* t0:acodec0 [15]: 0, [14]:1 ,acodec3 [22] :1
 * t1(400ms): acodec0 [1:0]:0  mute
 * t2(410ms): acodec0 [15]:1
 * t3(700ms): acodec0 [14]:0, acodec3 [22] :0
 */

static void aml_headphone_mute(struct snd_soc_component *component, int mute)
{
	unsigned int mask, val;

	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W) {
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 11,
						0x0 << 11);
	snd_soc_component_update_bits(component, ACODEC_3, 1 << 23,
						0x1 << 23);
	mask = BIT(VMID_GEN_FAST) | BIT(VMID_GEN_EN);
	val = BIT(VMID_GEN_FAST) | 0 << VMID_GEN_EN;
	snd_soc_component_update_bits(component, ACODEC_0, mask, val);
	snd_soc_component_update_bits(component, ACODEC_3, BIT(VMID_SOURCE),
						BIT(VMID_SOURCE));
	msleep(500);
	snd_soc_component_update_bits(component, ACODEC_0, 0x3 << LORP_EN,
						(mute ? 0 : 3) << LORP_EN);
	msleep(20);
	snd_soc_component_update_bits(component, ACODEC_0, BIT(VMID_GEN_EN),
						BIT(VMID_GEN_EN));
	msleep(300);
	snd_soc_component_update_bits(component, ACODEC_0, BIT(VMID_GEN_FAST),
						0 << VMID_GEN_FAST);
	snd_soc_component_update_bits(component, ACODEC_3, BIT(VMID_SOURCE),
						0x0 << VMID_SOURCE);
	} else {
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 11,
						0x0 << 11);
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 10,
						0x1 << 10);
		mask = BIT(VMID_GEN_FAST) | BIT(VMID_GEN_EN);
		val = BIT(VMID_GEN_FAST) | 0 << VMID_GEN_EN;
		snd_soc_component_update_bits(component, ACODEC_0, mask, val);
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 19,
						1 << 19);
		msleep(500);
		snd_soc_component_update_bits(component, ACODEC_0, 0x3 << LORP_EN,
						(mute ? 0 : 3) << LORP_EN);
		msleep(20);
		snd_soc_component_update_bits(component, ACODEC_0, BIT(VMID_GEN_EN),
						BIT(VMID_GEN_EN));
		msleep(300);
		snd_soc_component_update_bits(component, ACODEC_0, BIT(VMID_GEN_FAST),
						0 << VMID_GEN_FAST);
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 19,
						0x0 << 19);
	}
}

static void aml_headphone_mute_work(struct work_struct *acodec)
{
	struct am_acodec_priv *aml_acodec;

	aml_acodec = container_of(acodec, struct am_acodec_priv, mute_work);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		if (aml_acodec->headphone_power_on == 1) {
			aml_headphone_mute(aml_acodec->component, 0);
			aml_acodec->headphone_power_on = 0;
		}
	} else {
		aml_headphone_mute(aml_acodec->component, aml_acodec->headphone_mute);
	}
}

static void headphone_mute(struct snd_soc_component *component, bool enable)
{
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);

#ifdef HEADPHONE_MODE_DEBUG
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		if (work_pending(&aml_acodec->mute_work))
			flush_work(&aml_acodec->mute_work);
		else
			schedule_work(&aml_acodec->mute_work);
	}
#endif
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6D)
		output_pin_enable(component, aml_acodec->headphone_pin, enable);
	else
		output_path_set(component, aml_acodec->chip_version,
				aml_acodec->headphone_pin, enable);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		if (!enable) {
			snd_soc_component_update_bits(component, ACODEC_8, 1 << 23,
				0 << 23);
		} else {
			if (aml_acodec->output_type == DUAL_OUTPUT)
				snd_soc_component_update_bits(component, ACODEC_8, 1 << 23,
					1 << 23);
		}
	}
}

static void headphone_mode_sel(struct snd_soc_component *component,
	enum output_pin_sel pin)
{
	switch (pin) {
	case LP_RP_EN:
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPRP_MODE_EN,
							1 << HDPRP_MODE_EN);
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPLP_MODE_EN,
							1 << HDPLP_MODE_EN);
		break;
	case LP_RN_EN:
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPLP_MODE_EN,
							1 << HDPLP_MODE_EN);
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPRN_MODE_EN,
							1 << HDPRN_MODE_EN);
		break;
	case LN_RP_EN:
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPLN_MODE_EN,
							1 << HDPLN_MODE_EN);
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPRP_MODE_EN,
							1 << HDPRP_MODE_EN);
		break;
	case LN_RN_EN:
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPRN_MODE_EN,
							1 << HDPRN_MODE_EN);
		snd_soc_component_update_bits(component, ACODEC_8, 1 << HDPLN_MODE_EN,
							1 << HDPLN_MODE_EN);
		break;
	case LR_PN_EN:
		snd_soc_component_update_bits(component, ACODEC_8, 0x3 << 30,
			0 << 30);
		snd_soc_component_update_bits(component, ACODEC_8, 0x3 << 12,
			0 << 12);
		break;
	default:
		break;
	}
}

static void single_mode_headphone_set(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);

	output_path_set(component, aml_acodec->chip_version, aml_acodec->headphone_pin, 1);
	headphone_mode_sel(component, aml_acodec->headphone_pin);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6D) {
		snd_soc_component_update_bits(component, ACODEC_3, 1 << HEADPHONE_MODE,
							1 << HEADPHONE_MODE);
	snd_soc_component_update_bits(component, ACODEC_3, 0x3 << DRIVER_MODE_SEL,
							0x00 << DRIVER_MODE_SEL);
	} else {
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 16,
							1 << 16);
		snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 23,
						0x0 << 23);
	}
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6D)
		drop_pop_mode_1(component);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X)
		aml_headphone_mute(component, 0);
	output_pin_enable(component, aml_acodec->lineout_pin, 0);
	output_pin_enable(component, aml_acodec->headphone_pin, 1);
}

static void single_mode_lineout_set(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);

	output_path_set(component, aml_acodec->chip_version, aml_acodec->lineout_pin, 1);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6D) {
		snd_soc_component_update_bits(component, ACODEC_3, 1 << HEADPHONE_MODE,
					0 << HEADPHONE_MODE);
		snd_soc_component_update_bits(component, ACODEC_3, 0x3 << DRIVER_MODE_SEL,
						0x00 << DRIVER_MODE_SEL);
	} else {
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 16,
						1 << 16);
		snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 23,
						0x1 << 23);
	}
	output_pin_enable(component, aml_acodec->headphone_pin, 0);
	output_pin_enable(component, aml_acodec->lineout_pin, 1);
}

static void dual_output_single_mode_set(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);
	enum output_pin_sel headphone = aml_acodec->headphone_pin;
	enum output_pin_sel lineout = aml_acodec->lineout_pin;

	output_path_set(component, aml_acodec->chip_version, headphone, 1);
	output_path_set(component, aml_acodec->chip_version, lineout, 1);

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6D) {
		snd_soc_component_update_bits(component, ACODEC_3, 1 << HEADPHONE_MODE,
						1 << HEADPHONE_MODE);
		snd_soc_component_update_bits(component, ACODEC_3, 0x3 << DRIVER_MODE_SEL,
						0x3 << DRIVER_MODE_SEL);
	} else {
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 16,
			1 << 16);
		snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 23,
		0x1 << 23);
	}
	headphone_mode_sel(component, aml_acodec->headphone_pin);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6D)
		drop_pop_mode_1(component);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X)
		aml_headphone_mute(component, 0);
	output_pin_enable(component, aml_acodec->lineout_pin, 1);
	output_pin_enable(component, aml_acodec->headphone_pin, 1);
}

static void diff_mode_lineout_set(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);

	output_pin_enable(component, LR_PN_EN, 1);
	output_path_set(component, aml_acodec->chip_version, LR_PN_EN, 1);

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6D) {
		snd_soc_component_update_bits(component, ACODEC_3, 1 << HEADPHONE_MODE,
					1 << HEADPHONE_MODE);
		snd_soc_component_update_bits(component, ACODEC_3, 0x3 << DRIVER_MODE_SEL,
							0x3 << DRIVER_MODE_SEL);
	} else {
		snd_soc_component_update_bits(component, ACODEC_8, 1 << 16,
						1 << 16);
		snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 23,
					0x1 << 23);
	}
	/*lineout diff mode*/
	headphone_mode_sel(component, LR_PN_EN);
}

static int input_pin_set(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
			snd_soc_component_get_drvdata(component);

	switch (aml_acodec->input_sel) {
	case LP_RN:
		snd_soc_component_update_bits(component, ACODEC_8, 0xf << 0,
			0x9 << 0);
		break;
	case LN_RP:
		snd_soc_component_update_bits(component, ACODEC_8, 0xf << 0,
			0x6 << 0);
		break;
	case LN_RN:
		snd_soc_component_update_bits(component, ACODEC_8, 0xf << 0,
			0xa << 0);
		break;
	case LR_PN:
		snd_soc_component_write(component, ACODEC_1, 0x50500505);
		snd_soc_component_update_bits(component, ACODEC_8, 0xf << 0,
			0xf << 0);
		break;
	default:
		snd_soc_component_update_bits(component, ACODEC_8, 0xf << 0,
			0xa << 0);
		break;
	}
	return 0;
}

static int output_mode_set(struct snd_soc_component *component, int acodec_chip_version)
{
	struct am_acodec_priv *aml_acodec =
				snd_soc_component_get_drvdata(component);

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_C4) {
		snd_soc_component_update_bits(component, ACODEC_0, 0xf << 0, 0xf);
		snd_soc_component_write(component, ACODEC_8, 0x0000);
		if (aml_acodec->output_type == DIFF_LINEOUT) {
			snd_soc_component_write(component, ACODEC_3, 0x0042);
			snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 8,
				0x1 << 8);
		} else if (aml_acodec->output_type == LINEOUT_SINGLE) {
			snd_soc_component_write(component, ACODEC_3, 0x0022);
			snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 8,
					0x1 << 0);
		}
		snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 4,
			0x1 << 4);
		snd_soc_component_update_bits(component, ACODEC_8, 0x1 << 6,
			0x1 << 6);
		snd_soc_component_write(component, ACODEC_1, 0x50501111);
		snd_soc_component_write(component, ACODEC_4, 0x10000);
		input_pin_set(component);
		return 0;
	}
	if (aml_acodec->output_type == DUAL_OUTPUT)
		dual_output_single_mode_set(component);
	else if (aml_acodec->output_type == HEADPHONE_SINGLE)
		single_mode_headphone_set(component);
	else if (aml_acodec->output_type == LINEOUT_SINGLE)
		single_mode_lineout_set(component);
	else if (aml_acodec->output_type == DIFF_LINEOUT)
		diff_mode_lineout_set(component);
	return 0;
}

static int am_acodec_reg_init(struct snd_soc_component *component)
{
	int i;
	struct am_acodec_priv *aml_acodec =
			snd_soc_component_get_drvdata(component);
	int acodec_chip_version = 0;

	if (!aml_acodec) {
		pr_err("%s, Get am_acodec_priv fail\n", __func__);
		return 0;
	}

	if (aml_acodec) {
		for (i = 0;
			i < ARRAY_SIZE(t6d_acodec_init_list); i++)
			snd_soc_component_write
			(component,
			t6d_acodec_init_list[i].reg,
			t6d_acodec_init_list[i].def);
	}
	acodec_chip_version = aml_acodec->chip_version;
	/*t6x acodec config master mode*/
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		snd_soc_component_write(component, ACODEC_4, 0x00010000);
		snd_soc_component_update_bits(component, ACODEC_2, 0x1 << 0, 1 << 0);
	}
	output_mode_set(component, acodec_chip_version);
	acodec_adc_pga_gain_set(component, acodec_chip_version);
	acodec_dac_extra_gain_set(component, acodec_chip_version);
	if (aml_acodec->dac_output_invert == 1)
		snd_soc_component_update_bits(component, ACODEC_0, 0x3 << 20, 0 << 20);
	return 0;
}

static int aml_dac_gain_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);

	u32 reg_addr = ACODEC_1;
	u32 val = snd_soc_component_read(component, reg_addr);
	u32 val1 = (val & (0x1 <<  REG_DAC_GAIN_SEL_0))
					>> REG_DAC_GAIN_SEL_0;
	u32 val2 = (val & (0x1 <<  REG_DAC_GAIN_SEL_1))
					>> (REG_DAC_GAIN_SEL_1);
	val = val1 | (val2 << 1);

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_dac_gain_set_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int value = ucontrol->value.enumerated.item[0];

	aml_am_acodec_dac_extra_gain_set(component, 0, value);

	return 0;
}

static int aml_dac_output_mode_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = aml_acodec->output_type;
	return 0;
}

static int aml_dac_output_mode_set_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int value = ucontrol->value.enumerated.item[0];
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);

	aml_acodec->output_type = value;
	output_mode_set(component, aml_acodec->chip_version);
	return 0;
}

static int aml_dac2_gain_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);

	u32 reg_addr = ACODEC_7;
	u32 val = snd_soc_component_read(component, reg_addr);
	u32 val1 = (val & (0x1 <<  REG_DAC2_GAIN_SEL_0))
					>> REG_DAC_GAIN_SEL_0;
	u32 val2 = (val & (0x1 <<  REG_DAC2_GAIN_SEL_1))
					>> (REG_DAC2_GAIN_SEL_1);
	val = val1 | (val2 << 1);

	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_dac2_gain_set_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int value = ucontrol->value.enumerated.item[0];

	aml_am_acodec_dac_extra_gain_set(component, 1, value);

	return 0;
}

static int aml_DAC_source_sel_get_enum
	(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);
	u32 val = 0;

	regmap_read(aml_acodec->to_acodec_regmap, 0, &val);
	if (aml_acodec->lane_offset == 4) {
		val = (val >> 16) & 0xf;
		val -= (aml_acodec->tdmout_index << 2);
	} else {
		val = (val >> 16) & 0x1f;
		val -= (aml_acodec->tdmout_index << 3);
	}

	if (val > 3) {
		pr_info("Warning: tdmout_index = %d, val = 0x%x\n", aml_acodec->tdmout_index, val);
		val = 0;
	}
	ucontrol->value.enumerated.item[0] = val;
	return 0;
}

static int aml_DAC_source_sel_set_enum
		(struct snd_kcontrol *kcontrol,
		struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_soc_kcontrol_component(kcontrol);
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);
	u32 val = ucontrol->value.enumerated.item[0];

	if (val > 3) {
		pr_info("Warning: tdmout_index = %d, val = 0x%x\n", aml_acodec->tdmout_index, val);
		return 0;
	}

	if (aml_acodec->lane_offset == 4) {
		val += (aml_acodec->tdmout_index << 2);
		regmap_update_bits(aml_acodec->to_acodec_regmap, 0,
					(0xf << 16), (val << 16));

	} else {
		val += (aml_acodec->tdmout_index << 3);
		regmap_update_bits(aml_acodec->to_acodec_regmap, 0,
					(0x1f << 16), (val << 16));
	}

	return 0;
}

static const DECLARE_TLV_DB_SCALE(pga_in_tlv, -1200, 250, 1);
static const DECLARE_TLV_DB_SCALE(adc_vol_tlv, -29625, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);
static const DECLARE_TLV_DB_SCALE(dac2_vol_tlv, -95250, 375, 1);

static const char *const dac_gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };
static const char *const dac2_gain_texts[] = { "0dB", "6dB", "12dB", "18dB" };
static const char *const DAC_Src_texts[] = {"Lane0", "Lane1", "Lane2", "Lane3"};

static const char *const DAC_output_mode_texts[] = {"Dual Single", "Headphone Single",
							"Lineout Single", "Diff Lineout"};

static const struct soc_enum dac_gain_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0,
			ARRAY_SIZE(dac_gain_texts),
			dac_gain_texts);

static const struct soc_enum dac_output_mode_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0,
			ARRAY_SIZE(DAC_output_mode_texts),
			DAC_output_mode_texts);

static const struct soc_enum dac2_gain_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0,
			ARRAY_SIZE(dac2_gain_texts),
			dac2_gain_texts);
static const struct soc_enum DAC_source_sel_enum =
	SOC_ENUM_SINGLE
			(SND_SOC_NOPM, 0, ARRAY_SIZE(DAC_Src_texts),
			DAC_Src_texts);

static int Headphone_mute_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = aml_acodec->headphone_mute;
	return 0;
}

static int Headphone_mute_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int value = ucontrol->value.integer.value[0];

	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);

	aml_acodec->headphone_mute = value;

	headphone_mute(component, value ? 0 : 1);
	return 0;
}

static const struct snd_kcontrol_new am_acodec_snd_controls[] = {
	/*ALC  Switch*/
	SOC_SINGLE("ALC Switch", ACODEC_ALC_0, REG_ALC_EN, 1, 0),

	/*ALC PGA_IN Gain Index*/
	SOC_SINGLE_TLV("ALC PGA Gain", ACODEC_ALC_4,
			REG_PGA_GAIN_INDEX, 0x1f, 0, pga_in_tlv),

	/*PGA_IN Gain */
	SOC_DOUBLE_TLV
			("PGA IN Gain", ACODEC_1,
			PGAL_IN_GAIN, PGAR_IN_GAIN,
			0x1f, 0, pga_in_tlv),

	/*ADC Digital Volume control */
	SOC_DOUBLE_TLV
			("ADC Digital Capture Volume", ACODEC_1,
			ADCL_VC, ADCR_VC,
			0x7f, 0, adc_vol_tlv),

	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV
			("DAC Digital Playback Volume",
			ACODEC_2,
			DACL_VC, DACR_VC,
			0xff, 0, dac_vol_tlv),

	/*DAC extra Digital Gain control */
	SOC_ENUM_EXT
			("DAC Extra Digital Gain",
			dac_gain_enum,
			aml_dac_gain_get_enum,
			aml_dac_gain_set_enum),

	SOC_ENUM_EXT
			("DAC output mode select",
			dac_output_mode_enum,
			aml_dac_output_mode_get_enum,
			aml_dac_output_mode_set_enum),

	SOC_ENUM_EXT("DAC SOURCE SELECT",
			DAC_source_sel_enum,
			aml_DAC_source_sel_get_enum,
			aml_DAC_source_sel_set_enum),

	SOC_SINGLE_BOOL_EXT("Headphone mute", 0,
			    Headphone_mute_get,
			    Headphone_mute_set),

};

static const struct snd_kcontrol_new am_acodec_snd_dac2_controls[] = {
	/*DAC 2 Digital Volume control */
	SOC_DOUBLE_TLV
			("DAC2 Digital Playback Volume",
			ACODEC_5,
			DAC2L_VC, DAC2R_VC,
			0xff, 0, dac2_vol_tlv),

	/*DAC 2 extra Digital Gain control */
	SOC_ENUM_EXT
			("DAC2 Extra Digital Gain",
			dac2_gain_enum,
			aml_dac2_gain_get_enum,
			aml_dac2_gain_set_enum),
};

static int am_acodec_dai_set_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u32 val = snd_soc_component_read(component, ACODEC_0);

	pr_debug("%s, format:%x, codec = %p\n", __func__, fmt, component);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		val |= (0x1 << I2S_MODE);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		val &= ~(0x1 << I2S_MODE);
		break;
	default:
		return -EINVAL;
	}

	snd_soc_component_write(component, ACODEC_0, val);

	return 0;
}

static int am_acodec_dai_set_sysclk
	(struct snd_soc_dai *dai, int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static int am_acodec_dai_hw_params
	(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct am_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);

	aml_acodec->params = params;
	return 0;
}

static int am_acodec_dai_set_bias_level
	(struct snd_soc_component *component,
	enum snd_soc_bias_level level)
{
	pr_info("%s set bias level %d\n", __func__, level);
	switch (level) {
	case SND_SOC_BIAS_ON:
		break;
	case SND_SOC_BIAS_PREPARE:
		break;
	case SND_SOC_BIAS_STANDBY:
		snd_soc_component_cache_sync(component);
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_component_update_bits(component, ACODEC_2, 1 << DAC_SOFT_MUTE,
							1 << DAC_SOFT_MUTE);
		snd_soc_component_update_bits(component, ACODEC_6, 1 << DAC2_SOFT_MUTE,
							1 << DAC2_SOFT_MUTE);
		break;

	default:
		break;
	}
	component->dapm.bias_level = level;

	return 0;
}

static int am_acodec_dai_prepare
	(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	return 0;
}

static int am_acodec_reset(struct snd_soc_component *component)
{
	struct am_acodec_priv *am_acodec = snd_soc_component_get_drvdata(component);

	if (am_acodec && !IS_ERR(am_acodec->rst)) {
		pr_debug("call standard reset interface\n");
		reset_control_reset(am_acodec->rst);
		usleep_range(950, 1000);
	} else {
		pr_info("no call standard reset interface\n");
	}
	return 0;
}

static int am_acodec_start_up(struct snd_soc_component *component)
{
	snd_soc_component_write(component, ACODEC_0, 0xF000);
	msleep(200);
	snd_soc_component_write(component, ACODEC_0, 0xB000);

	return 0;
}

static int am_acodec_set_toacodec(struct am_acodec_priv *aml_acodec);
static int aml_tocodec_adc_control_init(struct am_acodec_priv *aml_acodec);

static void am_acodec_release_fast_mode_work_func(struct work_struct *p_work)
{
	struct am_acodec_priv *aml_acodec;
	struct snd_soc_component *component;

	aml_acodec = container_of(p_work, struct am_acodec_priv, work);
	if (!aml_acodec) {
		pr_err("%s, Get am_acodec_priv fail\n", __func__);
		return;
	}

	component = aml_acodec->component;
	if (!component) {
		pr_err("%s, Get snd_soc_codec fail\n", __func__);
		return;
	}
	am_acodec_set_toacodec(aml_acodec);
	aml_tocodec_adc_control_init(aml_acodec);
	/*
	 * reset audio codec register
	 * after init, need do reset again.
	 * only reset before init acodec, there is the
	 * output phase difference of left and right channels.
	 */

	am_acodec_reset(component);
	am_acodec_start_up(component);
	am_acodec_reg_init(component);
	aml_acodec->component = component;
	am_acodec_dai_set_bias_level(component, SND_SOC_BIAS_STANDBY);
}

static int am_acodec_dai_mute_stream
		(struct snd_soc_dai *dai, int mute,
		int stream)
{
	struct snd_soc_component *component = dai->component;

	pr_debug("%s, mute:%d\n", __func__, mute);

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		if (mute) {
			/* DAC 1 */
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE,
						1 << DAC_SOFT_MUTE);
			/* DAC 2 */
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE,
						1 << DAC2_SOFT_MUTE);
		} else {
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE, 0);
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE, 0);
		}
	}

	return 0;
}

static int am_acodec_dai_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (cmd) {
		case SNDRV_PCM_TRIGGER_START:
		case SNDRV_PCM_TRIGGER_RESUME:
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
			pr_debug("%s(), start\n", __func__);
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE, 0);
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE, 0);
			break;
		case SNDRV_PCM_TRIGGER_STOP:
		case SNDRV_PCM_TRIGGER_SUSPEND:
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
			pr_debug("%s(), stop\n", __func__);
			/* DAC 1 */
			snd_soc_component_update_bits(component, ACODEC_2,
						1 << DAC_SOFT_MUTE,
						1 << DAC_SOFT_MUTE);
			/* DAC 2 */
			snd_soc_component_update_bits(component, ACODEC_6,
						1 << DAC2_SOFT_MUTE,
						1 << DAC2_SOFT_MUTE);
			break;
		}
	}
	return 0;
}

struct snd_soc_dai_ops am_acodec_dai_ops = {
	.hw_params = am_acodec_dai_hw_params,
	.prepare = am_acodec_dai_prepare,
	.set_fmt = am_acodec_dai_set_fmt,
	.set_sysclk = am_acodec_dai_set_sysclk,
	.mute_stream = am_acodec_dai_mute_stream,
	.trigger = am_acodec_dai_trigger,
};

static int am_acodec_probe(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);
	int ret = 0;

	if (!aml_acodec) {
		pr_err("Failed to get am acodec priv\n");
		return -EINVAL;
	}
	aml_acodec->component = component;
	aml_acodec->rst = devm_reset_control_get(component->dev, "acodec");
	INIT_WORK(&aml_acodec->work, am_acodec_release_fast_mode_work_func);
	schedule_work(&aml_acodec->work);

	if (aml_acodec->chipinfo->dac_num == 4) {
		ret = snd_soc_add_component_controls(component,
						am_acodec_snd_dac2_controls,
						ARRAY_SIZE(am_acodec_snd_dac2_controls));
	}

	if (ret < 0) {
		dev_err(component->dev, "%s: could not add kcontrol for component (err=%d)\n",
			 __func__, ret);
		return ret;
	}
	return 0;
}

static void am_acodec_remove(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec =
		snd_soc_component_get_drvdata(component);

	cancel_work_sync(&aml_acodec->work);
	am_acodec_dai_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static int am_acodec_power_on(struct am_acodec_priv *aml_acodec)
{
	int i = 0;

	if (!aml_acodec || !aml_acodec->component) {
		pr_err("Failed to get am acodec priv\n");
		return -EINVAL;
	}
	am_acodec_reset(aml_acodec->component);
	am_acodec_start_up(aml_acodec->component);
	/*don't restore power reg*/
	if (aml_acodec) {
		for (i = 1; i < ARRAY_SIZE(t6d_acodec_init_list); i++)
			snd_soc_component_write(aml_acodec->component,
				t6d_acodec_init_list[i].reg, aml_acodec->user_setting[i]);
	}
	/*don't restore power reg*/
	snd_soc_component_write(aml_acodec->component, ACODEC_0, 0x34003ffc);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		aml_acodec->headphone_power_on = 1;
		if (work_pending(&aml_acodec->mute_work))
			flush_work(&aml_acodec->mute_work);
		else
			schedule_work(&aml_acodec->mute_work);
	}
	return 0;
}

static int am_acodec_power_off(struct am_acodec_priv *aml_acodec)
{
	unsigned int mask, val;

	if (!aml_acodec || !aml_acodec->component) {
		pr_err("Failed to get am acodec priv\n");
		return -EINVAL;
	}
	snd_soc_component_update_bits(aml_acodec->component, ACODEC_8, 1 << 11,
						0x0 << 11);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W)
		snd_soc_component_update_bits(aml_acodec->component, ACODEC_3, 1 << 23,
			0x1 << 23);
	else
		snd_soc_component_update_bits(aml_acodec->component, ACODEC_8, 1 << 10,
			0x1 << 10);
	mask = BIT(VMID_GEN_FAST) | BIT(VMID_GEN_EN);
	val = BIT(VMID_GEN_FAST) | 0 << VMID_GEN_EN;
	snd_soc_component_update_bits(aml_acodec->component, ACODEC_0, mask, val);
	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W)
		snd_soc_component_update_bits(aml_acodec->component, ACODEC_3, BIT(VMID_SOURCE),
					BIT(VMID_SOURCE));
	else
		snd_soc_component_update_bits(aml_acodec->component, ACODEC_8, 1 << 19,
						1 << 19);
	msleep(400);
	snd_soc_component_update_bits(aml_acodec->component, ACODEC_0, 0x3 << LORP_EN,
						0 << LORP_EN);
	snd_soc_component_update_bits(aml_acodec->component, ACODEC_0, 0xffff << 0, 0);
	snd_soc_component_update_bits(aml_acodec->component, ACODEC_8, 1 << 26, 0 << 26);
	snd_soc_component_update_bits(aml_acodec->component, ACODEC_8, 1 << 28, 0 << 28);
	return 0;
}

static int am_acodec_suspend(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);
	int i = 0;

	if (!aml_acodec) {
		pr_err("Failed to get am acodec priv\n");
		return -EINVAL;
	}

	if (aml_acodec && aml_acodec->suspend_ref > 0) {
		aml_acodec->suspend_ref = 0;
		return 0;
	}
	if (aml_acodec) {
		for (i = 0; i < ARRAY_SIZE(t6d_acodec_init_list); i++)
			aml_acodec->user_setting[i] = snd_soc_component_read(component,
				t6d_acodec_init_list[i].reg);
	}

	if ((aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) &&
		aml_acodec->headphone_depop) {
		am_acodec_power_off(aml_acodec);
		return 0;
	}
	snd_soc_component_update_bits(component, ACODEC_0, 0xffff << 0, 0);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << 26, 0 << 26);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << 28, 0 << 28);
	pr_info("%s suspend!\n", __func__);
	return 0;
}

static int am_acodec_resume(struct snd_soc_component *component)
{
	struct am_acodec_priv *aml_acodec = snd_soc_component_get_drvdata(component);
	int i = 0;

	if (!aml_acodec) {
		pr_err("Failed to get am acodec priv\n");
		return -EINVAL;
	}
	/*for str case, early suspend first execute */
	if (aml_acodec && !aml_acodec->resume_ref)
		aml_acodec->resume_ref = 1;

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		am_acodec_power_on(aml_acodec);
		return 0;
	}
	am_acodec_reset(component);
	am_acodec_start_up(component);
	am_acodec_reg_init(component);
	if (aml_acodec) {
		for (i = 0; i < ARRAY_SIZE(t6d_acodec_init_list); i++)
			snd_soc_component_write(component,
				t6d_acodec_init_list[i].reg, aml_acodec->user_setting[i]);
	}

	pr_info("%s resume!\n", __func__);
	return 0;
}

static const struct snd_soc_component_driver soc_codec_dev_am_acodec = {
	.probe = am_acodec_probe,
	.remove = am_acodec_remove,
	.suspend = am_acodec_suspend,
	.resume = am_acodec_resume,
//	.set_bias_level = am_acodec_dai_set_bias_level,
	.controls = am_acodec_snd_controls,
	.num_controls = ARRAY_SIZE(am_acodec_snd_controls),
//	.dapm_widgets = am_acodec_dapm_widgets,
//	.num_dapm_widgets = ARRAY_SIZE(am_acodec_dapm_widgets),
//	.dapm_routes = am_acodec_dapm_routes,
//	.num_dapm_routes = ARRAY_SIZE(am_acodec_dapm_routes),
};

static const struct regmap_config am_acodec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x68,
	.reg_defaults = t6d_acodec_init_list,
	.num_reg_defaults = ARRAY_SIZE(t6d_acodec_init_list),
	.cache_type = REGCACHE_RBTREE,
};

#define am_ACODEC_RATES		SNDRV_PCM_RATE_8000_96000
#define am_ACODEC_FORMATS		(SNDRV_PCM_FMTBIT_S16_LE \
			| SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			| SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_driver aml_am_acodec_dai = {
	.name = "acodec-hifi",
	.id = 0,
	.playback = {
		  .stream_name = "Playback",
		  .channels_min = 2,
		  .channels_max = 8,
		  .rates = am_ACODEC_RATES,
		  .formats = am_ACODEC_FORMATS,
		  },
	.capture = {
		 .stream_name = "Capture",
		 .channels_min = 2,
		 .channels_max = 8,
		 .rates = am_ACODEC_RATES,
		 .formats = am_ACODEC_FORMATS,
		 },
	.ops = &am_acodec_dai_ops,
};

static int aml_tocodec_adc_control_init(struct am_acodec_priv *aml_acodec)
{
	unsigned int lrclk_sel = 0, bclk_sel = 0, mclk_sel = 0;
	unsigned int mask = 0, val = 0;

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_C4) {
		/*only ph0 is right*/
		bclk_sel = 8 + aml_acodec->tdmin_index;
		lrclk_sel = 8 + aml_acodec->tdmin_index;
		mclk_sel  = aml_acodec->tdmin_index;
		mask = 0xf << 12 | 0xf << 4 | 0x7 << 0;
		val = bclk_sel << 12 | lrclk_sel << 4 | mclk_sel << 0;

		regmap_update_bits(aml_acodec->to_acodec_adc_regmap, 0,
							mask,  val);
		/*clk enable flow*/
		regmap_update_bits(aml_acodec->to_acodec_adc_regmap, 0,
							BIT(29), BIT(29));
		regmap_update_bits(aml_acodec->to_acodec_adc_regmap, 0,
							BIT(30), BIT(30));
		regmap_update_bits(aml_acodec->to_acodec_adc_regmap, 0,
							BIT(31), BIT(31));
	}
	return 0;
}

static int am_acodec_set_toacodec(struct am_acodec_priv *aml_acodec)
{
	int dat0_sel, dat1_sel, lrclk_sel, bclk_sel, mclk_sel;
	unsigned int update_bits_msk = 0x0, update_bits = 0x0;

	update_bits_msk = 0xFF7777;
	if (aml_acodec->chipinfo->is_bclk_cap_inv)
		update_bits |= (0x1 << 9);
	if (aml_acodec->chipinfo->is_bclk_o_inv)
		update_bits |= (0x1 << 8);
	if (aml_acodec->chipinfo->is_lrclk_inv)
		update_bits |= (0x1 << 10);

	if (aml_acodec->lane_offset == 4) {
		dat0_sel = (aml_acodec->tdmout_index << 2) + aml_acodec->dat0_ch_sel;
		dat0_sel = dat0_sel << 16;
		dat1_sel = (aml_acodec->tdmout_index << 2) + aml_acodec->dat1_ch_sel;
		dat1_sel = dat1_sel << 20;
	} else {
		dat0_sel = (aml_acodec->tdmout_index << 3) + aml_acodec->dat0_ch_sel;
		dat0_sel = dat0_sel << 16;
		dat1_sel = (aml_acodec->tdmout_index << 3) + aml_acodec->dat1_ch_sel;
		dat1_sel = dat1_sel << 22;
	}

	lrclk_sel = (aml_acodec->tdmout_index) << 12;
	bclk_sel = (aml_acodec->tdmout_index) << 4;

	//mclk_sel = aml_acodec->chipinfo->mclk_sel;
	mclk_sel = aml_acodec->tdmin_index;

	update_bits |= dat0_sel | dat1_sel | lrclk_sel | bclk_sel | mclk_sel;
	regmap_update_bits(aml_acodec->to_acodec_regmap, 0,
		update_bits_msk, update_bits);

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		/*only ph0 is right*/
		bclk_sel = 8 + aml_acodec->tdmout_index;
		lrclk_sel = 8 + aml_acodec->tdmout_index;
		mclk_sel  = aml_acodec->tdmout_index;

		/* mst_b sclk ctrl1[5:0] :23, tocodec[9:8] :0x1
		 * mst_b sclk ctrl1[5:0] :22, tocodec[9:8] :0x2
		 * mst_b sclk ctrl1 is 0x23
		 */
		update_bits_msk = 0x1 << 8 | 0x1 << 9 | 0xf << 12 | 0xf << 4 | 0x7 << 0;
		update_bits = 0x1 << 8 | 0x0 << 9 | bclk_sel << 12 | lrclk_sel << 4 | mclk_sel << 0;
		regmap_update_bits(aml_acodec->to_acodec_regmap, 0, update_bits_msk,  update_bits);
	}

	/* if toacodec_en is separated, need do:
	 * step1: enable/disable mclk
	 * step2: enable/disable bclk
	 * step3: enable/disable dat
	 */
	if (aml_acodec->chipinfo->separate_toacodec_en) {
		regmap_update_bits(aml_acodec->to_acodec_regmap, 0,
			BIT(29), BIT(29));
		regmap_update_bits(aml_acodec->to_acodec_regmap, 0,
			BIT(30), BIT(30));
	}
	regmap_update_bits(aml_acodec->to_acodec_regmap, 0,
		BIT(31), BIT(31));
	return 0;
}

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void aml_acodec_early_suspend(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	if (pdev) {
		struct am_acodec_priv *aml_acodec = platform_get_drvdata(pdev);

		if (aml_acodec && aml_acodec->component) {
			if (!aml_acodec->suspend_ref)
				am_acodec_suspend(aml_acodec->component);
			aml_acodec->suspend_ref = 1;
		}
	}
}

static void aml_acodec_early_resume(struct early_suspend *h)
{
	struct platform_device *pdev = h->param;

	if (pdev) {
		struct am_acodec_priv *aml_acodec = platform_get_drvdata(pdev);

		if (aml_acodec && aml_acodec->component) {
			if (!aml_acodec->resume_ref)
				am_acodec_resume(aml_acodec->component);
			aml_acodec->resume_ref = 0;
			aml_acodec->suspend_ref = 0;
		}
	}
}

static struct early_suspend acodec_early_suspend_handler = {
	.suspend = aml_acodec_early_suspend,
	.resume  = aml_acodec_early_resume,
};
#endif

static struct regmap *regmap_resource(struct device *dev, char *name)
{
	struct resource res;
	void __iomem *base;
	struct device_node *node = dev->of_node;
	static struct regmap_config aud_regmap_config = {
		.reg_bits = 32,
		.val_bits = 32,
		.reg_stride = 4,
		.cache_type = REGCACHE_RBTREE,
	};
	int i;

	i = of_property_match_string(node, "reg-names", name);
	if (of_address_to_resource(node, i, &res))
		return ERR_PTR(-ENOENT);

	base = devm_ioremap_resource(dev, &res);
	if (IS_ERR(base))
		return ERR_CAST(base);

	pr_info("%s, %s, start:%#x, size:%#x\n",
		__func__, name, (u32)res.start, (u32)resource_size(&res));

	aud_regmap_config.max_register = resource_size(&res) - 4;
	aud_regmap_config.name =
		devm_kasprintf(dev, GFP_KERNEL, "%s-%s", node->name, name);
	if (!aud_regmap_config.name)
		return ERR_PTR(-ENOMEM);

	return devm_regmap_init_mmio(dev, base, &aud_regmap_config);
}

static int aml_am_acodec_probe(struct platform_device *pdev)
{
	struct am_acodec_priv *aml_acodec;
	struct am_acodec_chipinfo *p_chipinfo;
	struct device_node *np;
	int ret = 0;

	np = pdev->dev.of_node;
	aml_acodec = devm_kzalloc
			(&pdev->dev,
			sizeof(struct am_acodec_priv),
			GFP_KERNEL);
	if (!aml_acodec)
		return -ENOMEM;
	/* match data */
	p_chipinfo = (struct am_acodec_chipinfo *)
		of_device_get_match_data(&pdev->dev);
	if (!p_chipinfo)
		dev_warn_once(&pdev->dev, "check whether to update am_acodec_chipinfo\n");

	aml_acodec->chipinfo = p_chipinfo;
	aml_acodec->chip_version = MESON_ACODEC_ID_VERSION_T6D;
	/* Don't change the regmaps order, it will affect the components regmap */
	aml_acodec->to_acodec_regmap = regmap_resource(&pdev->dev, "to_acodec");
	if (IS_ERR(aml_acodec->to_acodec_regmap)) {
		dev_err(&pdev->dev, "miss to_acodec regmap\n");
		return PTR_ERR(aml_acodec->to_acodec_regmap);
	}
	if (of_device_is_compatible(pdev->dev.of_node, "amlogic, t6w_acodec")) {
		aml_acodec->chip_version = MESON_ACODEC_ID_VERSION_T6W;
		aml_acodec->to_acodec_adc_regmap = regmap_resource(&pdev->dev, "to_acodec_adc");
		if (IS_ERR(aml_acodec->to_acodec_adc_regmap)) {
			dev_err(&pdev->dev, "miss to_acodec adc regmap\n");
			return PTR_ERR(aml_acodec->to_acodec_adc_regmap);
		}
	}
	if (of_device_is_compatible(pdev->dev.of_node, "amlogic, t6x_acodec")) {
		aml_acodec->chip_version = MESON_ACODEC_ID_VERSION_T6X;
		aml_acodec->to_acodec_adc_regmap = regmap_resource(&pdev->dev, "to_acodec_adc");
		if (IS_ERR(aml_acodec->to_acodec_adc_regmap)) {
			dev_err(&pdev->dev, "miss to_acodec adc regmap\n");
			return PTR_ERR(aml_acodec->to_acodec_adc_regmap);
		}
	}
	if (of_device_is_compatible(pdev->dev.of_node, "amlogic, c4_acodec")) {
		aml_acodec->chip_version = MESON_ACODEC_ID_VERSION_C4;
		aml_acodec->to_acodec_adc_regmap = regmap_resource(&pdev->dev, "to_acodec_adc");
		if (IS_ERR(aml_acodec->to_acodec_adc_regmap)) {
			dev_err(&pdev->dev, "miss to_acodec adc regmap\n");
			return PTR_ERR(aml_acodec->to_acodec_adc_regmap);
		}
	}

	/*soc component use the last regmap*/
	aml_acodec->regmap = regmap_resource(&pdev->dev, "acodec");
	if (IS_ERR(aml_acodec->regmap)) {
		dev_err(&pdev->dev, "miss acodec regmap\n");
		return PTR_ERR(aml_acodec->regmap);
	}

	aml_acodec->acodec_clk = devm_clk_get(&pdev->dev, "acodec_clk");
	if (!IS_ERR(aml_acodec->acodec_clk))
		clk_prepare_enable(aml_acodec->acodec_clk);
	else
		dev_info(&pdev->dev, "Can't retrieve acodec clock\n");

	of_property_read_u32
			(pdev->dev.of_node,
			"tdmout_index",
			&aml_acodec->tdmout_index);

	of_property_read_u32(pdev->dev.of_node,
			"dat0_ch_sel", &aml_acodec->dat0_ch_sel);

	of_property_read_u32
			(pdev->dev.of_node,
			"dat1_ch_sel",
			&aml_acodec->dat1_ch_sel);

	of_property_read_u32
			(pdev->dev.of_node,
			"tdmin_index",
			&aml_acodec->tdmin_index);

	of_property_read_u32
			(pdev->dev.of_node,
			"diff_output",
			&aml_acodec->diff_output);

	of_property_read_u32
			(pdev->dev.of_node,
			"diff_input",
			&aml_acodec->diff_input);

	of_property_read_u32
			(pdev->dev.of_node,
			"dac_extra_gain",
			&aml_acodec->dac_extra_gain);

	of_property_read_u32
			(pdev->dev.of_node,
			"dac_output_invert",
			&aml_acodec->dac_output_invert);

	of_property_read_u32
			(pdev->dev.of_node,
			"lane_offset",
			&aml_acodec->lane_offset);
	ret = of_property_read_u32
			(pdev->dev.of_node,
			"adc_pga_gain",
			&aml_acodec->adc_pga_gain);
	if (ret < 0)
		aml_acodec->adc_pga_gain = 11;
	ret = of_property_read_u32(pdev->dev.of_node,
			"charger_current_cap", &aml_acodec->charger_current_cap);
	if (ret < 0)
		aml_acodec->charger_current_cap = CAP_8_30UF;
	ret = of_property_read_u32(pdev->dev.of_node, "headphone_pin", &aml_acodec->headphone_pin);
	if (ret < 0)
		aml_acodec->headphone_pin = LP_RP_EN;

	ret = of_property_read_u32(pdev->dev.of_node, "lineout_pin", &aml_acodec->lineout_pin);
	if (ret < 0)
		aml_acodec->lineout_pin = LN_RN_EN;

	ret = of_property_read_u32(pdev->dev.of_node, "output_type", &aml_acodec->output_type);
	if (ret < 0)
		aml_acodec->output_type = DIFF_LINEOUT;

	ret = of_property_read_u32(pdev->dev.of_node, "early_suspend", &aml_acodec->early_suspend);
	if (ret < 0)
		aml_acodec->early_suspend = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "headphone_depop",
					&aml_acodec->headphone_depop);
	if (ret < 0)
		aml_acodec->headphone_depop = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "input_type",
					&aml_acodec->input_sel);
	if (ret < 0)
		aml_acodec->input_sel = 4;

	platform_set_drvdata(pdev, aml_acodec);

	INIT_WORK(&aml_acodec->mute_work, aml_headphone_mute_work);

	ret = devm_snd_soc_register_component
			(&pdev->dev,
			&soc_codec_dev_am_acodec,
			&aml_am_acodec_dai, 1);
	if (ret)
		pr_info("%s call snd_soc_register_codec error\n", __func__);
	else
		pr_debug("%s over\n", __func__);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	if (aml_acodec->early_suspend > 0) {
		acodec_early_suspend_handler.param = pdev;
		register_early_suspend(&acodec_early_suspend_handler);
	}
#endif

	return ret;
}

static void aml_am_acodec_remove(struct platform_device *pdev)
{
	struct am_acodec_priv *aml_acodec;

	aml_acodec = platform_get_drvdata(pdev);

	if (!IS_ERR(aml_acodec->acodec_clk))
		clk_disable_unprepare(aml_acodec->acodec_clk);

	if (work_pending(&aml_acodec->mute_work))
		cancel_work_sync(&aml_acodec->mute_work);

	snd_soc_unregister_component(&pdev->dev);
}

static void aml_am_acodec_shutdown(struct platform_device *pdev)
{
	struct am_acodec_priv *aml_acodec;
	struct snd_soc_component *component;

	aml_acodec = platform_get_drvdata(pdev);
	component = aml_acodec->component;

	if (!IS_ERR(aml_acodec->acodec_clk))
		clk_disable_unprepare(aml_acodec->acodec_clk);

	if (work_pending(&aml_acodec->mute_work))
		flush_work(&aml_acodec->mute_work);
	if (!component)
		return;

	if (aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6W ||
		aml_acodec->chip_version == MESON_ACODEC_ID_VERSION_T6X) {
		am_acodec_power_off(aml_acodec);
		return;
	}

	snd_soc_component_update_bits(component, ACODEC_0, 0xffff << 0, 0);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << 26, 0 << 26);
	snd_soc_component_update_bits(component, ACODEC_8, 1 << 28, 0 << 28);
}

static const struct of_device_id aml_am_acodec_dt_match[] = {
	{
		.compatible = "amlogic, t6d_acodec",
		.data = &acodec_cinfo_v3,
	}, {
		.compatible = "amlogic, t6w_acodec",
		.data = &acodec_cinfo_v3,
	}, {
		.compatible = "amlogic, t6x_acodec",
		.data = &acodec_cinfo_v3,
	}, {
		.compatible = "amlogic, c4_acodec",
		.data = &acodec_cinfo_v3,
	},
	{},
};

static struct platform_driver aml_am_acodec_platform_driver = {
	.driver = {
		   .name = "am_acodec",
		   .owner = THIS_MODULE,
		   .of_match_table = aml_am_acodec_dt_match,
		   },
	.probe = aml_am_acodec_probe,
	.remove = aml_am_acodec_remove,
	.shutdown = aml_am_acodec_shutdown,
};

static int __init aml_am_acodec_modinit(void)
{
	int ret = 0;

	ret = platform_driver_register(&aml_am_acodec_platform_driver);
	if (ret != 0)
		pr_err("register am acodec fail: %d\n", ret);

	return ret;
}

module_init(aml_am_acodec_modinit);

static void __exit aml_am_acodec_modexit(void)
{
	platform_driver_unregister(&aml_am_acodec_platform_driver);
}

module_exit(aml_am_acodec_modexit);

MODULE_DESCRIPTION("ASoC AML am audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
