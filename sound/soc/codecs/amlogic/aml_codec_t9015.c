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
#include <linux/of_address.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <linux/regmap.h>
#include <linux/of_device.h>
#include <linux/reset.h>

#include <linux/amlogic/media/sound/auge_utils.h>

#include "aml_codec_t9015.h"

struct t9015_acodec_chipinfo {
	bool separate_toacodec_en;
	int data_sel_shift;
};

struct aml_T9015_audio_priv {
	struct snd_soc_component *component;
	struct snd_pcm_hw_params *params;
	struct regmap *to_acodec_regmap;

	/* tocodec ctrl supports in and out data */
	bool tocodec_inout;
	/* attach which tdm when play */
	int tdmout_index;
	/* channel map */
	int ch0_sel;
	int ch1_sel;
	struct t9015_acodec_chipinfo *chipinfo;
	struct reset_control *rst;
	int dac_reset;
};

static struct t9015_acodec_chipinfo aml_acodec_cinfo = {
	.separate_toacodec_en = false,
	.data_sel_shift = DATA_SEL_SHIFT_VERSION0,
};

static struct t9015_acodec_chipinfo sc2_acodec_cinfo = {
	.separate_toacodec_en = true,
	.data_sel_shift = DATA_SEL_SHIFT_VERSION0,
};

static struct t9015_acodec_chipinfo s4_acodec_cinfo = {
	.separate_toacodec_en = true,
	.data_sel_shift = DATA_SEL_SHIFT_VERSION1,
};

static const struct reg_default t9015_init_list[] = {
	{AUDIO_CONFIG_BLOCK_ENABLE, 0x0000B03F},
	{ADC_VOL_CTR_PGA_IN_CONFIG, 0x00000000},
	{DAC_VOL_CTR_DAC_SOFT_MUTE, 0xFEFE0000},
	{LINE_OUT_CONFIG, 0x00001111},
	{POWER_CONFIG, 0x00010000},
};

static int aml_T9015_audio_reg_init(struct snd_soc_component *component)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(t9015_init_list); i++)
		snd_soc_component_write(component,
					t9015_init_list[i].reg,
					t9015_init_list[i].def);

	return 0;
}

static int aml_dac_gain_get_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	u32 reg_addr = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_component_read(component, reg_addr);
	u32 val1 = (val & (0x1 << DAC_GAIN_SEL_L))
		>> DAC_GAIN_SEL_L;
	u32 val2 = (val & (0x1 << DAC_GAIN_SEL_H))
		>> (DAC_GAIN_SEL_H);

	val = val1 | (val2 << 1);

	ucontrol->value.enumerated.item[0] = val;

	return 0;
}

static int aml_dac_gain_set_enum(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	u32 addr = ADC_VOL_CTR_PGA_IN_CONFIG;
	u32 val = snd_soc_component_read(component, addr);

	if (ucontrol->value.enumerated.item[0] == 0) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
	} else if (ucontrol->value.enumerated.item[0] == 1) {
		val &= ~(0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 2) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val &= ~(0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	} else if (ucontrol->value.enumerated.item[0] == 3) {
		val |= (0x1 << DAC_GAIN_SEL_H);
		val |= (0x1 << DAC_GAIN_SEL_L);
		pr_info("It has risk of distortion!\n");
	}

	snd_soc_component_write(component, addr, val);
	return 0;
}

static const DECLARE_TLV_DB_SCALE(dac_vol_tlv, -95250, 375, 1);

static const char *const dac_gain_texts[] = {
	"0dB", "6dB", "12dB", "18dB" };

static const struct soc_enum dac_gain_enum =
SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(dac_gain_texts),
		dac_gain_texts);

static int Dac_reset_get(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	ucontrol->value.integer.value[0] = T9015_audio->dac_reset;
	return 0;
}

static int Dac_reset_set(struct snd_kcontrol *kcontrol,
			  struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	int value = ucontrol->value.integer.value[0];
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	T9015_audio->dac_reset = value;
	if (value) {
		snd_soc_component_update_bits(component, AUDIO_CONFIG_BLOCK_ENABLE, 0x3 << 4,
						0 << 4);
		snd_soc_component_update_bits(component, AUDIO_CONFIG_BLOCK_ENABLE, 0x3 << 4,
						0x3 << 4);
	}
	return 0;
}

static const struct snd_kcontrol_new T9015_audio_snd_controls[] = {
	/*DAC Digital Volume control */
	SOC_DOUBLE_TLV("DAC Digital Playback Volume",
		       DAC_VOL_CTR_DAC_SOFT_MUTE,
		       DACL_VC, DACR_VC,
		       0xff, 0, dac_vol_tlv),

	/*DAC extra Digital Gain control */
	SOC_ENUM_EXT("DAC Extra Digital Gain",
		     dac_gain_enum,
		     aml_dac_gain_get_enum,
		     aml_dac_gain_set_enum),

	SOC_SINGLE_BOOL_EXT("DAC Reset", 0,
			    Dac_reset_get,
			    Dac_reset_set),

};

/*line out Left Positive mux */
static const char * const T9015_out_lp_txt[] = {
	"None", "LOLP_SEL_DACL", "LOLP_SEL_DACL_INV"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_lp_enum, LINE_OUT_CONFIG,
			    LOLP_SEL_DACL, T9015_out_lp_txt);

static const struct snd_kcontrol_new line_out_lp_mux =
SOC_DAPM_ENUM("ROUTE_LP_OUT", T9015_out_lp_enum);

/*line out Left Negative mux */
static const char * const T9015_out_ln_txt[] = {
	"None", "LOLN_SEL_DACL_INV", "LOLN_SEL_DACL"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_ln_enum, LINE_OUT_CONFIG,
			    LOLN_SEL_DACL_INV, T9015_out_ln_txt);

static const struct snd_kcontrol_new line_out_ln_mux =
SOC_DAPM_ENUM("ROUTE_LN_OUT", T9015_out_ln_enum);

/*line out Right Positive mux */
static const char * const T9015_out_rp_txt[] = {
	"None", "LORP_SEL_DACR", "LORP_SEL_DACR_INV"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_rp_enum, LINE_OUT_CONFIG,
			    LORP_SEL_DACR, T9015_out_rp_txt);

static const struct snd_kcontrol_new line_out_rp_mux =
SOC_DAPM_ENUM("ROUTE_RP_OUT", T9015_out_rp_enum);

/*line out Right Negative mux */
static const char * const T9015_out_rn_txt[] = {
	"None", "LORN_SEL_DACR_INV", "LORN_SEL_DACR"
};

static SOC_ENUM_SINGLE_DECL(T9015_out_rn_enum, LINE_OUT_CONFIG,
			    LORN_SEL_DACR_INV, T9015_out_rn_txt);

static const struct snd_kcontrol_new line_out_rn_mux =
SOC_DAPM_ENUM("ROUTE_RN_OUT", T9015_out_rn_enum);

static const struct snd_soc_dapm_widget T9015_audio_dapm_widgets[] = {
	/*Output */
	SND_SOC_DAPM_OUTPUT("Lineout left N"),
	SND_SOC_DAPM_OUTPUT("Lineout left P"),
	SND_SOC_DAPM_OUTPUT("Lineout right N"),
	SND_SOC_DAPM_OUTPUT("Lineout right P"),

	/*DAC playback stream */
	SND_SOC_DAPM_DAC("Left DAC", "HIFI Playback",
			 SND_SOC_NOPM,
			 DACL_EN, 0),
	SND_SOC_DAPM_DAC("Right DAC", "HIFI Playback",
			 SND_SOC_NOPM,
			 DACR_EN, 0),

	/*DRV output */
	SND_SOC_DAPM_OUT_DRV("LOLP_OUT_EN", SND_SOC_NOPM,
			     VMID_GEN_EN, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LOLN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORP_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("LORN_OUT_EN", SND_SOC_NOPM,
			     0, 0, NULL, 0),

	/*MUX output source select */
	SND_SOC_DAPM_MUX("Lineout left P switch", SND_SOC_NOPM,
			 0, 0, &line_out_lp_mux),
	SND_SOC_DAPM_MUX("Lineout left N switch", SND_SOC_NOPM,
			 0, 0, &line_out_ln_mux),
	SND_SOC_DAPM_MUX("Lineout right P switch", SND_SOC_NOPM,
			 0, 0, &line_out_rp_mux),
	SND_SOC_DAPM_MUX("Lineout right N switch", SND_SOC_NOPM,
			 0, 0, &line_out_rn_mux),
};

static const struct snd_soc_dapm_route T9015_audio_dapm_routes[] = {
	/*Output path*/
	{"Lineout left P switch", "LOLP_SEL_DACL", "Left DAC"},
	{"Lineout left P switch", "LOLP_SEL_DACL_INV", "Left DAC"},

	{"Lineout left N switch", "LOLN_SEL_DACL_INV", "Left DAC"},
	{"Lineout left N switch", "LOLN_SEL_DACL", "Left DAC"},

	{"Lineout right P switch", "LORP_SEL_DACR", "Right DAC"},
	{"Lineout right P switch", "LORP_SEL_DACR_INV", "Right DAC"},

	{"Lineout right N switch", "LORN_SEL_DACR_INV", "Right DAC"},
	{"Lineout right N switch", "LORN_SEL_DACR", "Right DAC"},

	{"LOLN_OUT_EN", NULL, "Lineout left N switch"},
	{"LOLP_OUT_EN", NULL, "Lineout left P switch"},
	{"LORN_OUT_EN", NULL, "Lineout right N switch"},
	{"LORP_OUT_EN", NULL, "Lineout right P switch"},

	{"Lineout left N", NULL, "LOLN_OUT_EN"},
	{"Lineout left P", NULL, "LOLP_OUT_EN"},
	{"Lineout right N", NULL, "LORN_OUT_EN"},
	{"Lineout right P", NULL, "LORP_OUT_EN"},
};

static int aml_T9015_set_dai_fmt(struct snd_soc_dai *dai, unsigned int fmt)
{
	struct snd_soc_component *component = dai->component;
	u32 val = snd_soc_component_read(component,
					 AUDIO_CONFIG_BLOCK_ENABLE);

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

	snd_soc_component_write(component, AUDIO_CONFIG_BLOCK_ENABLE, val);

	return 0;
}

static int aml_T9015_hw_params(struct snd_pcm_substream *substream,
			       struct snd_pcm_hw_params *params,
			       struct snd_soc_dai *dai)
{
	struct snd_soc_component *component = dai->component;
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	T9015_audio->params = params;

	return 0;
}

static int aml_T9015_audio_set_bias_level(struct snd_soc_component *component,
					  enum snd_soc_bias_level level)
{
	pr_debug("%s\n", __func__);

	switch (level) {
	case SND_SOC_BIAS_ON:
		break;

	case SND_SOC_BIAS_PREPARE:
		break;

	case SND_SOC_BIAS_STANDBY:
		if (component->dapm.bias_level == SND_SOC_BIAS_OFF)
			snd_soc_component_cache_sync(component);
		break;

	case SND_SOC_BIAS_OFF:
		snd_soc_component_write(component,
					AUDIO_CONFIG_BLOCK_ENABLE, 0xB000);
		usleep_range(10000, 10050);
		snd_soc_component_write(component,
					AUDIO_CONFIG_BLOCK_ENABLE, 0xC000);
		msleep(200);
		snd_soc_component_write(component,
					AUDIO_CONFIG_BLOCK_ENABLE, 0);
		break;

	default:
		break;
	}
	component->dapm.bias_level = level;

	return 0;
}

static int aml_T9015_audio_reset(struct snd_soc_component *component)
{
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	if (T9015_audio && !IS_ERR(T9015_audio->rst))
		reset_control_reset(T9015_audio->rst);
	else
		pr_info("no call standard reset interface\n");

	return 0;
}

static int aml_T9015_audio_start_up(struct snd_soc_component *component)
{
	snd_soc_component_write(component, AUDIO_CONFIG_BLOCK_ENABLE, 0xc000);
	msleep(200);
	snd_soc_component_write(component, AUDIO_CONFIG_BLOCK_ENABLE, 0xF000);
	msleep(50);
	snd_soc_component_write(component, AUDIO_CONFIG_BLOCK_ENABLE, 0xB000);
	usleep_range(1000, 1050);
	return 0;
}

static int aml_T9015_codec_mute_stream(struct snd_soc_dai *dai,
				       int mute, int stream)
{
	struct snd_soc_component *component = dai->component;
	u32 reg;

	if (stream == SNDRV_PCM_STREAM_PLAYBACK) {
		reg = snd_soc_component_read(component,
					     DAC_VOL_CTR_DAC_SOFT_MUTE);
		if (mute)
			reg |= 0x1 << DAC_SOFT_MUTE;
		else
			reg &= ~(0x1 << DAC_SOFT_MUTE);

		snd_soc_component_write(component,
					DAC_VOL_CTR_DAC_SOFT_MUTE, reg);
	}
	return 0;
}

static void toacodec_ctrl(struct aml_T9015_audio_priv *priv)
{
	int id = priv->tdmout_index;

	// TODO: check skew for g12a
	regmap_write(priv->to_acodec_regmap, 0,
		       1 << 31 |
		       ((id << 2)) << 12 | /* data 0*/
		       id << 8 | /* lrclk */
		       1 << 7 |         /* Bclk_cap_inv*/
		       id << 4 | /* bclk */
		       id /* mclk */
	);
}

static void toacodec_ctrl_ext(struct aml_T9015_audio_priv *priv)
{
	int tdmout_id = priv->tdmout_index;
	int ch0_sel = priv->ch0_sel;
	int ch1_sel = priv->ch1_sel;
	bool separate = priv->chipinfo->separate_toacodec_en;
	int data_sel_shift = priv->chipinfo->data_sel_shift;

	regmap_write(priv->to_acodec_regmap, 0,
		tdmout_id << 12          /* lrclk */
		| 1 << 9                   /* Bclk_cap_inv*/
		| tdmout_id << 4           /* bclk */
		| tdmout_id << 0           /* mclk */
		);

	if (data_sel_shift == DATA_SEL_SHIFT_VERSION0)
		regmap_update_bits(priv->to_acodec_regmap, 0,
			0xf << 20 | 0xf << 16,
			((tdmout_id << 2) + ch1_sel) << 20 | ((tdmout_id << 2) + ch0_sel) << 16);
	else
		regmap_update_bits(priv->to_acodec_regmap, 0,
			0x1f << 22 | 0x1f << 16,
			((tdmout_id << 3) + ch1_sel) << 22 | ((tdmout_id << 3) + ch0_sel) << 16);

	/* if toacodec_en is separated, need do:
	 * step1: enable/disable mclk
	 * step2: enable/disable bclk
	 * step3: enable/disable dat
	 */
	if (separate) {
		regmap_update_bits(priv->to_acodec_regmap, 0,
				     0x1 << 29,
				     0x1 << 29);
		regmap_update_bits(priv->to_acodec_regmap, 0,
				     0x1 << 30,
				     0x1 << 30);
	}

	regmap_update_bits(priv->to_acodec_regmap, 0,
			0x1 << 31, 0x1 << 31);
}

static int aml_T9015_audio_probe(struct snd_soc_component *component)
{
	struct snd_soc_dapm_context *dapm =
		snd_soc_component_get_dapm(component);
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	if (!T9015_audio) {
		pr_info("T9015_audio is null!\n");
		return -ENODEV;
	}
	dapm->idle_bias_off = 0;
	T9015_audio->rst = devm_reset_control_get(component->dev, "acodec");

	/*reset audio codec register*/
	aml_T9015_audio_reset(component);
	aml_T9015_audio_start_up(component);
	aml_T9015_audio_reg_init(component);

	if (T9015_audio->tocodec_inout)
		toacodec_ctrl_ext(T9015_audio);
	else
		toacodec_ctrl(T9015_audio);

	component->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	T9015_audio->component = component;

	return 0;
}

static void aml_T9015_audio_remove(struct snd_soc_component *component)
{
	pr_info("%s!\n", __func__);

	aml_T9015_audio_set_bias_level(component, SND_SOC_BIAS_OFF);
}

static int aml_T9015_audio_suspend(struct snd_soc_component *component)
{
	pr_info("%s!\n", __func__);

	aml_T9015_audio_set_bias_level(component, SND_SOC_BIAS_OFF);

	return 0;
}

static int aml_T9015_audio_resume(struct snd_soc_component *component)
{
	struct aml_T9015_audio_priv *T9015_audio =
		snd_soc_component_get_drvdata(component);

	aml_T9015_audio_reset(component);
	aml_T9015_audio_start_up(component);
	aml_T9015_audio_reg_init(component);

	if (T9015_audio->tocodec_inout)
		toacodec_ctrl_ext(T9015_audio);
	else
		toacodec_ctrl(T9015_audio);

	component->dapm.bias_level = SND_SOC_BIAS_STANDBY;
	aml_T9015_audio_set_bias_level(component, SND_SOC_BIAS_STANDBY);

	return 0;
}

#define T9015_AUDIO_STEREO_RATES SNDRV_PCM_RATE_8000_96000
#define T9015_AUDIO_FORMATS (SNDRV_PCM_FMTBIT_S16_LE \
			     | SNDRV_PCM_FMTBIT_S20_3LE | SNDRV_PCM_FMTBIT_S24_LE \
			     | SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_dai_ops T9015_audio_aif_dai_ops = {
	.hw_params = aml_T9015_hw_params,
	.set_fmt = aml_T9015_set_dai_fmt,
	.mute_stream = aml_T9015_codec_mute_stream,
};

struct snd_soc_dai_driver aml_T9015_audio_dai[] = {
	{
		.name = "T9015-audio-hifi",
		.id = 0,
		.playback = {
			.stream_name  = "HIFI Playback",
			.channels_min = 2,
			.channels_max = 16,
			.rates        = T9015_AUDIO_STEREO_RATES,
			.formats      = T9015_AUDIO_FORMATS,
		},
		.capture = {
			.stream_name  = "HIFI Capture",
			.channels_min = 2,
			.channels_max = 8,
			.rates        = T9015_AUDIO_STEREO_RATES,
			.formats      = T9015_AUDIO_FORMATS,
		},
		.ops = &T9015_audio_aif_dai_ops,
	},
};

static const struct snd_soc_component_driver soc_codec_dev_aml_T9015_audio = {
	.probe            = aml_T9015_audio_probe,
	.remove           = aml_T9015_audio_remove,
	.suspend          = aml_T9015_audio_suspend,
	.resume           = aml_T9015_audio_resume,
	.set_bias_level   = aml_T9015_audio_set_bias_level,

	.controls         = T9015_audio_snd_controls,
	.num_controls     = ARRAY_SIZE(T9015_audio_snd_controls),
	.dapm_widgets     = T9015_audio_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(T9015_audio_dapm_widgets),
	.dapm_routes      = T9015_audio_dapm_routes,
	.num_dapm_routes  = ARRAY_SIZE(T9015_audio_dapm_routes),
};

static const struct regmap_config t9015_codec_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0x14,
	.reg_defaults = t9015_init_list,
	.num_reg_defaults = ARRAY_SIZE(t9015_init_list),
	.cache_type = REGCACHE_RBTREE,
};

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

static int aml_T9015_audio_codec_probe(struct platform_device *pdev)
{
	struct aml_T9015_audio_priv *T9015_audio;
	int ret = 0;
	struct t9015_acodec_chipinfo *p_chipinfo;
	struct regmap *regmap;

	dev_dbg(&pdev->dev, "%s\n", __func__);

	T9015_audio = devm_kzalloc(&pdev->dev,
				   sizeof(struct aml_T9015_audio_priv),
				   GFP_KERNEL);
	if (!T9015_audio)
		return -ENOMEM;

	p_chipinfo = (struct t9015_acodec_chipinfo *)
		of_device_get_match_data(&pdev->dev);
	if (!p_chipinfo) {
		dev_warn_once(&pdev->dev,
			      "no update t9015_acodec_chipinfo\n");
		return -EINVAL;
	}

	T9015_audio->chipinfo = p_chipinfo;

	platform_set_drvdata(pdev, T9015_audio);

	T9015_audio->to_acodec_regmap = regmap_resource(&pdev->dev, "to_acodec");
	if (IS_ERR(T9015_audio->to_acodec_regmap)) {
		dev_err(&pdev->dev, "miss to_acodec regmap\n");
		return PTR_ERR(T9015_audio->to_acodec_regmap);
	}

	regmap = regmap_resource(&pdev->dev, "acodec");
	if (IS_ERR(regmap)) {
		dev_err(&pdev->dev, "miss acodec regmap\n");
		return PTR_ERR(regmap);
	}

	T9015_audio->tocodec_inout = of_property_read_bool(pdev->dev.of_node,
							   "tocodec_inout");

	of_property_read_u32(pdev->dev.of_node,
			     "tdmout_index",
			     &T9015_audio->tdmout_index);

	of_property_read_u32(pdev->dev.of_node,
			     "ch0_sel",
			     &T9015_audio->ch0_sel);

	of_property_read_u32(pdev->dev.of_node,
			     "ch1_sel",
			     &T9015_audio->ch1_sel);

	dev_dbg(&pdev->dev, "T9015 acodec tdmout index:%d\n", T9015_audio->tdmout_index);

	ret = devm_snd_soc_register_component(&pdev->dev,
					      &soc_codec_dev_aml_T9015_audio,
					      &aml_T9015_audio_dai[0], 1);

	return ret;
}

static void aml_T9015_audio_codec_shutdown(struct platform_device *pdev)
{
	struct aml_T9015_audio_priv *aml_acodec;
	struct snd_soc_component *component;

	aml_acodec = platform_get_drvdata(pdev);
	component = aml_acodec->component;
	if (component)
		aml_T9015_audio_remove(component);
}

static const struct of_device_id aml_T9015_codec_dt_match[] = {
	{
		.compatible = "amlogic, aml_codec_T9015",
		.data = &aml_acodec_cinfo,
	},
	{
		.compatible = "amlogic, sc2_codec_T9015",
		.data = &sc2_acodec_cinfo,
	},
	{
		.compatible = "amlogic, s4_codec_T9015",
		.data = &s4_acodec_cinfo,
	},
	{}
};

static int aml_T9015_acodec_platform_restore(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_T9015_audio_priv *aml_acodec = platform_get_drvdata(pdev);
	struct snd_soc_component *component = aml_acodec->component;

	aml_T9015_audio_resume(component);
	pr_info("%s!\n", __func__);

	return 0;
}

static int aml_T9015_acodec_platform_freeze(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct aml_T9015_audio_priv *aml_acodec = platform_get_drvdata(pdev);
	struct snd_soc_component *component = aml_acodec->component;

	aml_T9015_audio_suspend(component);
	pr_info("%s!\n", __func__);

	return 0;
}

static const struct dev_pm_ops meson_T9015_pm_ops = {
	.restore = aml_T9015_acodec_platform_restore,
	.freeze = aml_T9015_acodec_platform_freeze,
};

static struct platform_driver aml_T9015_codec_platform_driver = {
	.driver  = {
		.name           = "aml_codec_T9015",
		.owner          = THIS_MODULE,
		.of_match_table = aml_T9015_codec_dt_match,
		.pm = &meson_T9015_pm_ops,
	},
	.probe    = aml_T9015_audio_codec_probe,
	.shutdown = aml_T9015_audio_codec_shutdown,
};

module_platform_driver(aml_T9015_codec_platform_driver);

MODULE_DESCRIPTION("ASoC AML T9015 audio codec driver");
MODULE_AUTHOR("AMLogic, Inc.");
MODULE_LICENSE("GPL");
