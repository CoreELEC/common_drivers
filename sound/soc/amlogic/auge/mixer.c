// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/pinctrl/consumer.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/clk-provider.h>
#include <linux/regulator/consumer.h>
#include <linux/clk-provider.h>

#include <linux/amlogic/pm.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/jiffies.h>

#include "mixer_hw.h"

#define DRV_NAME "mixer"

struct mixer_chipinfo {
	int id;
};

struct mixer {
	int id;
	struct mixer_chipinfo *chipinfo;
	struct device *dev;
	struct aml_audio_controller *actrl;
	struct regmap *mixer_reg_map;
	struct regmap *coef_reg_map;
	struct frddr *fddr;
	unsigned int mixer_vol;
	int mixer_output_src;
	int mixer_lane_status[4];
	unsigned int unstable;
};

static struct snd_pcm_substream *mixer_substream;
#define SPDIF_BUFFER_BYTES (512 * 1024 * 2)
static const struct snd_pcm_hardware aml_mixer_hardware = {
	.info =
		SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER | SNDRV_PCM_INFO_PAUSE,
	.formats =
		SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S24_LE |
		SNDRV_PCM_FMTBIT_S32_LE,

	.period_bytes_min = 64,
	.period_bytes_max = 128 * 1024,
	.periods_min = 2,
	.periods_max = 1024,
	.buffer_bytes_max = SPDIF_BUFFER_BYTES,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 32,
};

static int aml_dai_mixer_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static void aml_dai_mixer_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
}

static int aml_get_mixer_frddr_type(int bitwidth)
{
	unsigned int frddr_type = 0;

	switch (bitwidth) {
	case 8:
		frddr_type = 0;
		break;
	case 16:
		frddr_type = 2;
		break;
	case 24:
	case 32:
		frddr_type = 4;
		break;
	default:
		pr_err("invalid bit_depth: %d\n", bitwidth);
		break;
	}

	return frddr_type;
}

static int aml_dai_mixer_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mixer *mixer_p = snd_soc_dai_get_drvdata(cpu_dai);
	unsigned int bit_depth = 0;
	unsigned int fifo_id = 0;
	struct frddr *fddr = NULL;
	unsigned int fddr_type = 0;

	if (!mixer_p) {
		pr_err("%s:mixer null\n", __func__);
		return -ENXIO;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		fddr = mixer_p->fddr;
		if (!fddr) {
			dev_err(mixer_p->dev, "%s:fddr not find\n", __func__);
			return -EINVAL;
		}

		bit_depth = snd_pcm_format_width(runtime->format);
		fifo_id = fddr->fifo_id;
		fddr_type = aml_get_mixer_frddr_type(bit_depth);
		mic_mixer_ch_set(runtime->channels);
		mic_mixer_format_set(bit_depth, fddr_type, runtime->channels);
		mixer_fddr_rate(fddr, 1);
		mic_mixer_source(fifo_id);
		aml_frddr_select_dst(fddr, mixer_p->mixer_output_src);
	}
	return 0;
}

static int aml_dai_mixer_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *cpu_dai)
{
	struct mixer *mixer_p = snd_soc_dai_get_drvdata(cpu_dai);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		mixer_clip_top_en(true);
		aml_frddr_enable(mixer_p->fddr, true);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		aml_frddr_enable(mixer_p->fddr, false);
		mixer_clip_top_en(false);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int aml_dai_mixer_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params,
				struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int aml_dai_mixer_hw_free(struct snd_pcm_substream *substream,
				struct snd_soc_dai *cpu_dai)
{
	return 0;
}

static int aml_dai_set_mixer_sysclk(struct snd_soc_dai *cpu_dai,
				int clk_id, unsigned int freq, int dir)
{
	return 0;
}

static irqreturn_t aml_mixer_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static int aml_mixer_open(struct snd_soc_component *component,
			  struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = snd_soc_rtd_to_cpu(rtd, 0)->dev;
	struct mixer *mixer_p = (struct mixer *)
		snd_soc_dai_get_drvdata(snd_soc_rtd_to_cpu(rtd, 0));
	int ret = 0;

	snd_soc_set_runtime_hwparams(substream, &aml_mixer_hardware);
	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, SPDIF_BUFFER_BYTES / 2, SPDIF_BUFFER_BYTES);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		mixer_p->fddr = aml_audio_register_frddr(dev,
			aml_mixer_ddr_isr, substream, false);
		if (!mixer_p->fddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim from ddr\n");
			goto err_ddr;
		}
	}
	aml_frddr_mixer_set(mixer_p->fddr, 1);
	aml_mixer_mic_set(mixer_p->fddr, 1);
	runtime->private_data = mixer_p;
	mixer_substream = substream;
	return 0;
err_ddr:
	snd_pcm_lib_free_pages(substream);
	return ret;
}

static int aml_mixer_close(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mixer *mixer_p = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK)
		aml_audio_unregister_frddr(mixer_p->dev, substream);

	runtime->private_data = NULL;
	mixer_substream = NULL;
	snd_pcm_lib_free_pages(substream);
	return 0;
}

static int aml_mixer_ioctl(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream,
			   unsigned int cmd, void *arg)
{
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int aml_mixer_hw_params(struct snd_soc_component *component,
				   struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int aml_mixer_hw_free(struct snd_soc_component *component,
				 struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int aml_mixer_prepare(struct snd_soc_component *component,
				 struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mixer *mixer_p = runtime->private_data;
	unsigned int start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	if (!mixer_p) {
		pr_err("%s:mixer null\n", __func__);
		return -ENXIO;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		struct frddr *fr = mixer_p->fddr;

		start_addr = runtime->dma_addr;
		end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
		period   = frames_to_bytes(runtime, runtime->period_size);
		int_addr = period / FIFO_BURST;
		aml_frddr_reset(mixer_p->fddr);
		mic_mixer_fifo_reset();
		/*
		 * Contrast minimum of period and fifo depth,
		 * and set the value as half.
		 */
		threshold = min(period, fr->chipinfo->fifo_depth);
		threshold /= 2;
		/* Use all the fifo */
		aml_frddr_set_fifos(fr, fr->chipinfo->fifo_depth, threshold);

		aml_frddr_set_buf(fr, start_addr, end_addr);
		aml_frddr_set_intrpt(fr, int_addr);
	}
	return 0;
}

static snd_pcm_uframes_t aml_mixer_pointer(struct snd_soc_component *component,
					   struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mixer *mixer_p  = runtime->private_data;
	unsigned int addr, start_addr;
	snd_pcm_uframes_t frames = 0;

	start_addr = runtime->dma_addr;
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		addr = aml_frddr_get_position(mixer_p->fddr);
		frames = bytes_to_frames(runtime, addr - start_addr);
		if (frames > runtime->buffer_size)
			frames = 0;
	}
	return frames;
}

static int aml_mixer_mmap(struct snd_soc_component *component,
			  struct snd_pcm_substream *substream,
			  struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static int mixer_vol_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	ucontrol->value.enumerated.item[0] = mixer_p->mixer_vol;
	return 0;
}

static int mixer_vol_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	unsigned int value = ucontrol->value.enumerated.item[0];
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	if (value > 0x7fffffff) {
		pr_err("vol not support %d\n",
				value);
		return 0;
	}
	mixer_p->mixer_vol = value;
	mixer_mic_coef_set(mixer_p->coef_reg_map, value);
	return 0;
}

static int mixer_get_lane_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	ucontrol->value.enumerated.item[0] = mixer_p->mixer_lane_status[reg];

	return 0;
}

static int mixer_set_lane_enum(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	unsigned int value = ucontrol->value.enumerated.item[0];
	struct soc_mixer_control *mc =
			(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	mixer_lane_source(mixer_p->coef_reg_map, reg, value);
	mixer_p->mixer_lane_status[reg] = value;

	return 0;
}

static int mixer_stable_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	ucontrol->value.enumerated.item[0] = mixer_p->unstable;
	return 0;
}

static int mixer_stable_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	unsigned int value = ucontrol->value.enumerated.item[0];
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	if (value > 0) {
		pr_err("mixer stable not support %d\n",
				value);
		return 0;
	}
	mixer_p->unstable = value;
	return 0;
}

static const char * const mixer_output_source_text[] = {
	"tdmout_b", "tdmout_b", "tdmout_c", "tdmout_d"
};

static const struct soc_enum mixer_output_source_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(mixer_output_source_text),
		mixer_output_source_text);

static int mixer_src_enum_get(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	ucontrol->value.enumerated.item[0] = mixer_p->mixer_output_src;
	return 0;
}

static int mixer_src_enum_put(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	unsigned int value = ucontrol->value.enumerated.item[0];
	struct mixer *mixer_p;

	if (!component) {
		pr_err("%s: component is null\n", __func__);
		return 0;
	}
	mixer_p  = snd_soc_component_get_drvdata(component);
	if (!mixer_p)
		return 0;

	if (value > 3)
		pr_info("not support mixer source:%d\n", value);
	mixer_p->mixer_output_src = value;
	if (component->active)
		snd_pcm_stop_xrun(mixer_substream);
	return 0;
}

static const struct snd_kcontrol_new snd_mixer_controls[] = {
	SOC_SINGLE_EXT("mixer vol",
			0, 0, 0x7fffffff, 0,
			mixer_vol_get, mixer_vol_set),
	SOC_SINGLE_EXT("Main mixer lane0 Source Select",
				0, 0, 3, 0,
				mixer_get_lane_enum,
				mixer_set_lane_enum),
	SOC_SINGLE_EXT("Main mixer lane1 Source Select",
				1, 0, 3, 0,
				mixer_get_lane_enum,
				mixer_set_lane_enum),
	SOC_SINGLE_EXT("Main mixer lane2 Source Select",
				2, 0, 3, 0,
				mixer_get_lane_enum,
				mixer_set_lane_enum),

	SOC_SINGLE_EXT("Main mixer lane3 Source Select",
				3, 0, 3, 0,
				mixer_get_lane_enum,
				mixer_set_lane_enum),
	SOC_SINGLE_EXT("Mixer Xrun",
			0, 0, 1, 0,
			mixer_stable_get, mixer_stable_set),
	SOC_ENUM_EXT("Mixer source select",
				mixer_output_source_enum,
				mixer_src_enum_get,
				mixer_src_enum_put),

};

static const struct snd_soc_component_driver aml_mixer_component[] = {
	{
		.name		= "MIXER",
		.controls		= snd_mixer_controls,
		.num_controls	= ARRAY_SIZE(snd_mixer_controls),
		.open		  = aml_mixer_open,
		.close		  = aml_mixer_close,
		.ioctl		  = aml_mixer_ioctl,
		.hw_params	  = aml_mixer_hw_params,
		.hw_free	  = aml_mixer_hw_free,
		.prepare	  = aml_mixer_prepare,
		.pointer	  = aml_mixer_pointer,
		.mmap		  = aml_mixer_mmap,
	},
};

static const struct snd_soc_dai_ops aml_mixer_ops = {
	.startup = aml_dai_mixer_startup,
	.shutdown = aml_dai_mixer_shutdown,
	.prepare = aml_dai_mixer_prepare,
	.trigger = aml_dai_mixer_trigger,
	.hw_params = aml_dai_mixer_hw_params,
	.hw_free   = aml_dai_mixer_hw_free,
	.set_sysclk = aml_dai_set_mixer_sysclk,
};

#define AML_DAI_MIXER_RATES	(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_MIXER_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE |\
				 SNDRV_PCM_FMTBIT_S24_LE |\
				 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aml_mixer_dai[] = {
	{
		.name = "MIXER",
		.id = 1,
		.playback = {
			  .channels_min = 1,
			  .channels_max = 8,
			  .rates = AML_DAI_MIXER_RATES,
			  .formats = AML_DAI_MIXER_FORMATS,
		},
		.ops = &aml_mixer_ops,
	},
};

#define MIXER_0 0

struct mixer_chipinfo t6d_mixer_chipinfo = {
	.id = MIXER_0,
};

const struct of_device_id aml_mixer_device_id[] = {
	{
		.compatible = "amlogic, t6d-mixer",
		.data		= &t6d_mixer_chipinfo,
	},
	{},
};

static int mixer_notify_callback(struct notifier_block *block,
				    unsigned long cmd, void *para)
{
	if (mixer_substream) {
		struct snd_soc_pcm_runtime *rtd = mixer_substream->private_data;
		struct mixer *mixer_p = (struct mixer *)
			snd_soc_dai_get_drvdata(snd_soc_rtd_to_cpu(rtd, 0));
		if (cmd == BACKGROUND_RESET_CMD) {
			mixer_p->unstable = 1;
			snd_pcm_stop_xrun(mixer_substream);
			pr_debug("notify mic mixer xun\n");
		}
	}
	return 0;
}

static struct notifier_block mixer_notifier_audio = {
	.notifier_call	= mixer_notify_callback,
};

static int aml_mixer_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct mixer_chipinfo *chip_info;
	struct mixer *mixer_p;
	struct device *dev = &pdev->dev;
	struct device_node *node_prt = NULL;
	struct platform_device *pdev_parent;
	struct aml_audio_controller *actrl = NULL;
	int ret = 0;

	mixer_p = devm_kzalloc(dev, sizeof(struct mixer), GFP_KERNEL);
	if (!mixer_p)
		return -ENOMEM;
	mixer_p->dev = dev;
	dev_set_drvdata(dev, mixer_p);
	chip_info = (struct mixer_chipinfo *)
		of_device_get_match_data(dev);

	mixer_p->id = chip_info->id;
	/* get audio controller */
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	pdev_parent = of_find_device_by_node(node_prt);
	of_node_put(node_prt);
	actrl = (struct aml_audio_controller *)
				platform_get_drvdata(pdev_parent);
	mixer_p->actrl = actrl;

	ret = of_property_read_s32(node, "mic_output", &mixer_p->mixer_output_src);
	if (ret < 0)
		mixer_p->mixer_output_src = TDMOUT_B;

	mixer_p->mixer_reg_map = regmap_resource(&pdev->dev, "mixer_reg");
	if (IS_ERR_OR_NULL(mixer_p->mixer_reg_map)) {
		dev_err(dev, "Can't mixer regmap!!\n");
		return -ENXIO;
	}
	set_mixer_regmap(mixer_p->mixer_reg_map);
	mixer_p->coef_reg_map = regmap_resource(&pdev->dev, "coef_reg");
	if (IS_ERR_OR_NULL(mixer_p->coef_reg_map)) {
		dev_err(dev, "Can't coef_reg_map regmap!!\n");
		return -ENXIO;
	}
	ret = devm_snd_soc_register_component(dev,
			&aml_mixer_component[mixer_p->id],
			&aml_mixer_dai[mixer_p->id], 1);
	if (ret) {
		dev_err(dev, "devm_snd_soc_register_component failed\n");
		return ret;
	}
	mixer_register_client(&mixer_notifier_audio);
	mixer_p->mixer_vol = 0x800000;
	mixer_coef_set(mixer_p->coef_reg_map, mixer_p->mixer_vol);

	return ret;
}

struct platform_driver aml_mixer_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = aml_mixer_device_id,
	},
	.probe = aml_mixer_platform_probe,
};

int __init mixer_init(void)
{
	return platform_driver_register(&aml_mixer_driver);
}

void __exit mixer_exit(void)
{
	platform_driver_unregister(&aml_mixer_driver);
}

#ifndef MODULE
module_init(mixer_init);
module_exit(mixer_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic S/PDIF ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, aml_mixer_device_id);
#endif

