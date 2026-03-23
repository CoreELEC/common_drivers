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
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/clk-provider.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/suspend.h>

#include <linux/input.h>
#include <linux/amlogic/pm.h>
#include <linux/pm_wakeirq.h>
#include <linux/pm_wakeup.h>

#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/firmware.h>

#include "ddr_mngr.h"
#include "sed.h"
#include "sed_hw.h"
#include "pdm_hw.h"
#include "pdm.h"

#define DRV_NAME "snd_sed"

struct sed {
	struct sed_cfg_t cfg;
	struct device *dev;
	struct regmap *audio_top_sed_map;
	struct regmap *sed_map;

	struct clk *hifi_pll;
	struct clk *rc_pll;
	struct clk *sed_clk;
	struct clk *sed_clk_pll;
	struct clk *pdm_sysclk_srcpll;
	struct clk *pdm_dclk_srcpll;
	struct clk *pdm_sysclk;
	struct clk *pdm_dclk;
	struct toddr *tddr;
	int irq_wakeup;
	void *mic_src;
	unsigned int sample_bit;
	unsigned int channel;
	unsigned int asla_active;
	unsigned int rate;
	struct input_dev *input_device;
	enum sed_level level;
	enum sed_source mic_source;
	enum sed_suspend_type suspend_type;
	int sed_enable;
	int pdm_sample_cnt;
	enum sed_mic_rate mic_rate;
	int sed_clk_en;
	int sed_load_bin;
};

#define SED_BUFFER_BYTES (512 * 1024 * 2)
static const struct snd_pcm_hardware aml_sed_hardware = {
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
	.buffer_bytes_max = SED_BUFFER_BYTES,

	.rate_min = 8000,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 32,
};

#define SED_LOAD_MAX_SIZE (64 * 1024)

static int load_sed_firmware(struct sed *sed_p)
{
	const struct firmware *fw;
	int ret;
	int reg_size;
	const unsigned int	*buf;

	ret = request_firmware(&fw, "sed.bin", sed_p->dev);
	if (ret) {
		dev_err(sed_p->dev, "request_firmware failed !\n");
		return -EINVAL;
	}
	if (fw->size > SED_LOAD_MAX_SIZE)
		reg_size = SED_LOAD_MAX_SIZE / 4;
	else
		reg_size = fw->size / 4;
	buf = (const unsigned int *)fw->data;
	sed_load_bin(sed_p->sed_map, buf, reg_size);
	release_firmware(fw);
	sed_p->sed_load_bin = 1;
	return 0;
}

static int sed_pdm_cfg(struct sed *p_sed)
{
	unsigned int osr, rate = 16000;
	struct pdm_info info;
	struct aml_pdm *pdm = (struct aml_pdm *)p_sed->mic_src;
	int gain_index = 0, lpf_filter_mode = 4, hpf_filter_mode = 6;

	if (!pdm) {
		dev_err(p_sed->dev, "pdm handle is null!\n");
		return -EINVAL;
	}
	gain_index = pdm->pdm_gain_index;
	info.bitdepth = p_sed->sample_bit;
	info.channels = p_sed->channel;
	info.lane_masks = pdm->lane_mask_in;
	/*768k, 16k suspend mode*/
	if (p_sed->suspend_type == SUSPEND_MODE)
		info.dclk_idx = 2;
	else
		info.dclk_idx = pdm->dclk_idx;
	info.bypass = 0;
	// todo sample cnt 768k
	pr_info("%s:lane_mask_in:%d\n", __func__, pdm->lane_mask_in);
	info.sample_count = p_sed->pdm_sample_cnt;
	info.mute = 0;

	if (pdm) {
		info.mute = pdm->pdm_mute;
		lpf_filter_mode = pdm->lpf_filter_mode;
		hpf_filter_mode = pdm->hpf_filter_mode;
	}
	aml_pdm_ctrl(&info, 0);
	/* filter for pdm */
	if (p_sed->mic_rate == MIC_16K)
		rate = 16000;
	else if (p_sed->mic_rate == MIC_48K)
		rate = 48000;
	else if (p_sed->mic_rate == MIC_96K)
		rate = 96000;
	p_sed->rate = rate;
	osr = pdm_get_ors(info.dclk_idx, rate);

	pr_info("%s:%d\n", __func__, osr);
	aml_pdm_filter_ctrl(gain_index, osr, lpf_filter_mode, hpf_filter_mode, 0);
	return 0;
}

static int pdm_clk_enable(struct sed *p_sed)
{
	int ret = 0;

	/* enable clock */
	ret = clk_prepare_enable(p_sed->pdm_sysclk_srcpll);
	if (ret) {
		pr_err("Can't enable pcm pdm_sysclk_srcpll clock: %d\n", ret);
		goto err;
	}

	ret = clk_prepare_enable(p_sed->pdm_dclk_srcpll);
	if (ret) {
		pr_err("Can't enable pcm pdm_dclk_srcpll clock: %d\n", ret);
		goto err;
	}
	ret = clk_prepare_enable(p_sed->pdm_sysclk);
	if (ret) {
		pr_err("Can't enable pcm pdm_sysclk clock: %d\n", ret);
		goto err;
	}
	ret = clk_prepare_enable(p_sed->pdm_dclk);
	if (ret) {
		pr_err("Can't enable pcm pdm_sysclk clock: %d\n", ret);
		goto err;
	}

	return 0;
err:
	pr_err("failed enable pdm clock\n");
	return -EINVAL;
}

/*rc clk 30M*/
#define SED_CLK_PLL 30000000

static int pdm_set_clk(struct sed *p_sed)
{
	unsigned int ret;
	unsigned int rate, pdm_sysclk, pdm_dclk, by_pass;

	if (p_sed->suspend_type == SUSPEND_MODE) {
		rate = 30000000;
		pdm_sysclk = 30000000;
		pdm_dclk = 30000000;
		p_sed->pdm_sysclk_srcpll = p_sed->rc_pll;
		p_sed->pdm_dclk_srcpll = p_sed->rc_pll;
		by_pass = 0;
	} else if (p_sed->suspend_type == FREEZE_MODE) {
		pdm_sysclk = 133333351;
		pdm_dclk = 3072000;
		rate = 491520000;
		by_pass = 1;
	} else {
		rate = 30000000;
		pdm_sysclk = 30000000;
		pdm_dclk = 30000000;
		p_sed->pdm_sysclk_srcpll = p_sed->rc_pll;
		p_sed->pdm_dclk_srcpll = p_sed->rc_pll;
		by_pass = 0;
	}
	clk_set_rate(p_sed->pdm_sysclk_srcpll, rate);
	clk_set_rate(p_sed->pdm_dclk_srcpll, rate);

	ret = clk_set_parent(p_sed->pdm_sysclk, p_sed->pdm_sysclk_srcpll);
	if (ret) {
		dev_err(p_sed->dev,
			"Can't set clk_pdm_sysclk parent clock\n");
		return ret;
	}

	ret = clk_set_parent(p_sed->pdm_dclk, p_sed->pdm_dclk_srcpll);
	if (ret) {
		dev_err(p_sed->dev,
			"Can't set clk_pdm_dclk parent clock\n");
		return ret;
	}

	clk_set_rate(p_sed->pdm_sysclk, pdm_sysclk);
	clk_set_rate(p_sed->pdm_dclk, pdm_dclk);

	pdm_dclk_coefficient(p_sed->audio_top_sed_map, by_pass);
	pdm_clk_enable(p_sed);

	return 0;
}

static int sed_clk_config(struct sed *p_sed)
{
	int ret = 0;

	clk_set_rate(p_sed->sed_clk_pll, SED_CLK_PLL);
	ret = clk_set_parent(p_sed->sed_clk, p_sed->sed_clk_pll);
	if (ret) {
		pr_err("Can't se sed clk parent: %d\n", ret);
		return ret;
	}
	clk_set_rate(p_sed->sed_clk, SED_CLK_PLL);
	ret = clk_prepare_enable(p_sed->sed_clk);
	if (ret) {
		pr_err("Can't enable sed clk: %d\n", ret);
		return ret;
	}
	sed_clk_set(p_sed->sed_map);
	return ret;
}

static void sed_clk_enable(struct sed *sed_p)
{
	if (!sed_p->sed_clk_en) {
		sed_clk_config(sed_p);
		pdm_set_clk(sed_p);
		sed_p->sed_clk_en = 1;
	}
}

static int aml_dai_sed_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sed *sed_p = (struct sed *)
		snd_soc_dai_get_drvdata(snd_soc_rtd_to_cpu(rtd, 0));

	sed_clk_enable(sed_p);
	return 0;
}

static void sed_clk_disable(struct sed *sed_p)
{
	if (sed_p->sed_clk_en) {
		clk_disable_unprepare(sed_p->pdm_dclk);
		clk_disable_unprepare(sed_p->pdm_sysclk);
		clk_disable_unprepare(sed_p->pdm_sysclk_srcpll);
		clk_disable_unprepare(sed_p->pdm_dclk_srcpll);
		clk_disable_unprepare(sed_p->sed_clk);
		sed_p->sed_clk_en = 0;
	}
}

static void aml_dai_sed_shutdown(struct snd_pcm_substream *substream,
				struct snd_soc_dai *cpu_dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct sed *sed_p = (struct sed *)
		snd_soc_dai_get_drvdata(snd_soc_rtd_to_cpu(rtd, 0));

	sed_clk_disable(sed_p);
}

static void sed_init(struct sed *p_sed, int enable)
{
	pdm_fifo_reset(0);
	sed_pdm_cfg(p_sed);
	sed_top_ctrl_init(p_sed->audio_top_sed_map);
	sed_parameter_set(&p_sed->cfg);
	sed_mel_filter_set(&p_sed->cfg, p_sed->sed_map, p_sed->rate);
	sed_chain_set(p_sed->sed_map);
	pdm_enable(enable, 0);
}

static int sed_enable_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = sed_p->sed_enable;
	return 0;
}

static int sed_enable_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > 1) {
		pr_info("invalid value:%d\n", value);
		return 0;
	}
	sed_p->sed_enable = value;
	if (sed_p->sed_enable) {
		sed_clk_enable(sed_p);
		load_sed_firmware(sed_p);
		sed_init(sed_p, 1);
		sed_enable(sed_p->audio_top_sed_map, 1);
	} else {
		sed_init(sed_p, 0);
		sed_enable(sed_p->audio_top_sed_map, 0);
		sed_clk_disable(sed_p);
	}
	return 0;
}

static int sed_pdm_sample_count_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = sed_p->pdm_sample_cnt;
	return 0;
}

static int sed_pdm_sample_count_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);
	unsigned int value = ucontrol->value.enumerated.item[0];

	sed_p->pdm_sample_cnt = value;

	return 0;
}

static int sed_pdm_sample_train_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = sed_p->pdm_sample_cnt;
	return 0;
}

static int sed_pdm_sample_train_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (sed_p->sed_enable)
		sed_p->pdm_sample_cnt = pdm_auto_train_algorithm(0, value);

	return 0;
}

static const char *const sed_rate_texts[] = {
	"16000",
	"48000",
	"96000",

};

static const struct soc_enum sed_rate_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(sed_rate_texts),
			sed_rate_texts);

static int sed_mic_rate_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = sed_p->mic_rate;
	return 0;
}

static int sed_mic_rate_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > 2)
		dev_err(sed_p->dev, "%s: Invalid value %d\n", __func__, value);
	sed_p->mic_rate = value;

	return 0;
}

static const char *const sed_suspend_type_texts[] = {
	"FREEZE_MODE",
	"SUSPEND_MODE",
	"EARLY_SUSPEND_MODE",
};

static const struct soc_enum sed_suspend_type_enum =
	SOC_ENUM_SINGLE(SND_SOC_NOPM, 0, ARRAY_SIZE(sed_suspend_type_texts),
			sed_suspend_type_texts);

static int sed_mic_suspend_type_get(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);

	ucontrol->value.enumerated.item[0] = sed_p->suspend_type;
	return 0;
}

static int sed_mic_suspend_type_set(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p  = snd_soc_component_get_drvdata(component);
	unsigned int value = ucontrol->value.enumerated.item[0];

	if (value > 2)
		dev_err(sed_p->dev, "%s: Invalid value %d\n", __func__, value);
	sed_p->suspend_type = value;

	return 0;
}

static int sed_channel_get_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p = dev_get_drvdata(component->dev);

	if (!sed_p)
		return 0;

	ucontrol->value.enumerated.item[0] = sed_p->channel;

	return 0;
}

static int sed_channel_set_enum(struct snd_kcontrol *kcontrol,
	struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_component *component = snd_kcontrol_chip(kcontrol);
	struct sed *sed_p = dev_get_drvdata(component->dev);

	if (!sed_p)
		return 0;

	sed_p->channel = ucontrol->value.enumerated.item[0];

	return 0;
}

static const struct snd_kcontrol_new snd_sed_controls[] = {
	SOC_SINGLE_EXT("Sed Enable Test",
			0, 0, 1, 0,
			sed_enable_get, sed_enable_set),

	SOC_SINGLE_EXT("Sed Pdm Sample cnt",
			0, 0, 100, 0,
			sed_pdm_sample_count_get, sed_pdm_sample_count_set),

	SOC_SINGLE_EXT("Sed Pdm Sample Train",
			0, 0, 60, 0,
			sed_pdm_sample_train_get, sed_pdm_sample_train_set),

	SOC_ENUM_EXT("Sed Mic Rate",
			 sed_rate_enum,
			 sed_mic_rate_get,
			 sed_mic_rate_set),

	SOC_SINGLE_EXT("Sed Pdm Channel",
			0, 0, 8, 0,
			sed_channel_get_enum, sed_channel_set_enum),

	SOC_ENUM_EXT("Sed Mic Suspend Type",
			 sed_suspend_type_enum,
			 sed_mic_suspend_type_get,
			 sed_mic_suspend_type_set),

};

static irqreturn_t aml_sed_ddr_isr(int irq, void *devid)
{
	struct snd_pcm_substream *substream =
		(struct snd_pcm_substream *)devid;

	if (!snd_pcm_running(substream))
		return IRQ_HANDLED;

	snd_pcm_period_elapsed(substream);

	return IRQ_HANDLED;
}

static int aml_sed_open(struct snd_soc_component *component,
			  struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct device *dev = snd_soc_rtd_to_cpu(rtd, 0)->dev;
	struct sed *sed_p = (struct sed *)
		snd_soc_dai_get_drvdata(snd_soc_rtd_to_cpu(rtd, 0));
	int ret = 0;

	snd_soc_set_runtime_hwparams(substream, &aml_sed_hardware);
	snd_pcm_lib_preallocate_pages(substream, SNDRV_DMA_TYPE_DEV,
		dev, SED_BUFFER_BYTES / 2, SED_BUFFER_BYTES);
	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE) {
		sed_p->tddr = aml_audio_register_toddr(dev,
			aml_sed_ddr_isr, substream, 0);
		if (!sed_p->tddr) {
			ret = -ENXIO;
			dev_err(dev, "failed to claim to ddr\n");
			goto err_ddr;
		}
	}
	/*user read, freeze mode*/
	rtd->dai_link->ignore_suspend = 1;
	if (sed_p->suspend_type == FREEZE_MODE)
		current->flags |= PF_NOFREEZE;
	runtime->private_data = sed_p;
	return 0;
err_ddr:
	snd_pcm_lib_free_pages(substream);
	return ret;
}

static int aml_sed_close(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sed *sed_p = runtime->private_data;

	if (substream->stream == SNDRV_PCM_STREAM_CAPTURE)
		aml_audio_unregister_toddr(sed_p->dev,
			substream, 0);
	runtime->private_data = NULL;
	snd_pcm_lib_free_pages(substream);
	return 0;
}

static int aml_sed_ioctl(struct snd_soc_component *component,
			   struct snd_pcm_substream *substream,
			   unsigned int cmd, void *arg)
{
	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

static int aml_sed_hw_params(struct snd_soc_component *component,
				   struct snd_pcm_substream *substream,
				   struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream,
					params_buffer_bytes(hw_params));
}

static int aml_sed_hw_free(struct snd_soc_component *component,
				 struct snd_pcm_substream *substream)
{
	snd_pcm_lib_free_pages(substream);

	return 0;
}

static int aml_sed_prepare(struct snd_soc_component *component, struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sed *p_sed = runtime->private_data;
	struct toddr *to = p_sed->tddr;
	dma_addr_t start_addr, end_addr, int_addr;
	unsigned int period, threshold;

	start_addr = runtime->dma_addr;
	end_addr = start_addr + runtime->dma_bytes - FIFO_BURST;
	period	 = frames_to_bytes(runtime, runtime->period_size);
	int_addr = period / FIFO_BURST;

	/*
	 * Contrast minimum of period and fifo depth,
	 * and set the value as half.
	 */
	threshold = min(period, p_sed->tddr->chipinfo->fifo_depth);
	threshold /= 2;
	aml_toddr_set_fifos(to, threshold);

	aml_toddr_set_buf(to, start_addr, end_addr);
	aml_toddr_set_intrpt(to, int_addr);

	return 0;
}

static snd_pcm_uframes_t aml_sed_pointer(struct snd_soc_component *component,
	struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct sed *p_sed = runtime->private_data;
	unsigned int addr = 0, start_addr = 0;

	start_addr = runtime->dma_addr;

	addr = aml_toddr_get_position(p_sed->tddr);

	return bytes_to_frames(runtime, addr - start_addr);
}

static int aml_sed_mmap(struct snd_soc_component *component,
			  struct snd_pcm_substream *substream,
			  struct vm_area_struct *vma)
{
	return snd_pcm_lib_default_mmap(substream, vma);
}

static int aml_dai_sed_prepare(struct snd_pcm_substream *substream,
	struct snd_soc_dai *cpu_dai)
{
	struct sed *p_sed = snd_soc_dai_get_drvdata(cpu_dai);
	struct snd_pcm_runtime *runtime = substream->runtime;
	unsigned int bitwidth;
	unsigned int toddr_type, lsb;
	struct toddr_fmt fmt;
	enum toddr_src mic_src = PDMIN;
	struct toddr *to;

	if (!p_sed)
		return -EINVAL;
	to = p_sed->tddr;
	/* set bclk */
	bitwidth = snd_pcm_format_width(runtime->format);
	lsb = 32 - bitwidth;

	switch (bitwidth) {
	case 16:
	case 32:
		toddr_type = 0;
		break;
	case 24:
		toddr_type = 4;
		break;
	default:
		pr_err("invalid bit_depth: %d\n", bitwidth);
		return -1;
	}

	/* to ddr pdmin */
	fmt.type = toddr_type;
	fmt.msb = 31;
	fmt.lsb = lsb;
	fmt.endian = 0;
	fmt.bit_depth = bitwidth;
	fmt.ch_num = runtime->channels;
	fmt.rate = runtime->rate;

	if (runtime->rate == 96000)
		p_sed->mic_rate = MIC_96K;
	else if (runtime->rate == 48000)
		p_sed->mic_rate = MIC_48K;
	else if (runtime->rate == 16000)
		p_sed->mic_rate = MIC_48K;

	p_sed->sample_bit = bitwidth;
	p_sed->channel = runtime->channels;

	if (p_sed->mic_source == PDM_MIC)
		mic_src = PDMIN;
	else if (p_sed->mic_source == TDM_MIC)
		mic_src = TDMIN_C;
	aml_toddr_select_src(to, mic_src);
	aml_toddr_set_format(to, &fmt);

	if (p_sed->mic_source == PDM_MIC) {
		pdm_fifo_reset(0);
		sed_pdm_cfg(p_sed);
	}
	sed_top_ctrl_init(p_sed->audio_top_sed_map);
	sed_parameter_set(&p_sed->cfg);
	sed_mel_filter_set(&p_sed->cfg, p_sed->sed_map, p_sed->rate);
	//sed_chain_set(p_sed->sed_map);

	/*todo tdm mic*/
	return 0;
}

static const struct snd_soc_component_driver aml_sed_component[] = {
	{
		.name		= "SED",
		.controls		= snd_sed_controls,
		.num_controls	= ARRAY_SIZE(snd_sed_controls),
		.open		  = aml_sed_open,
		.close		  = aml_sed_close,
		.ioctl		  = aml_sed_ioctl,
		.hw_params	  = aml_sed_hw_params,
		.hw_free	  = aml_sed_hw_free,
		.prepare	  = aml_sed_prepare,
		.pointer	  = aml_sed_pointer,
		.mmap		  = aml_sed_mmap,
	},
};

static int aml_dai_sed_trigger(struct snd_pcm_substream *substream, int cmd,
				   struct snd_soc_dai *cpu_dai)
{
	struct sed *sed_p = snd_soc_dai_get_drvdata(cpu_dai);
	bool toddr_stopped = false;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		pdm_fifo_reset(0);
		aml_toddr_enable(sed_p->tddr, 1);
		pdm_enable(true, 0);
		sed_p->sed_enable = 1;
		sed_enable(sed_p->audio_top_sed_map, true);

		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		pdm_enable(false, 0);
		toddr_stopped = aml_toddr_burst_finished(sed_p->tddr);
		if (toddr_stopped)
			aml_toddr_enable(sed_p->tddr, false);
		else
			pr_err("%s(), toddr may be stuck\n", __func__);
		sed_p->sed_enable = 0;
		sed_enable(sed_p->audio_top_sed_map, false);

		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dai_ops aml_sed_ops = {
	.startup = aml_dai_sed_startup,
	.shutdown = aml_dai_sed_shutdown,
	.prepare = aml_dai_sed_prepare,
	.trigger = aml_dai_sed_trigger,
};

#define AML_DAI_MIXER_RATES	(SNDRV_PCM_RATE_8000_192000)
#define AML_DAI_MIXER_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE |\
				 SNDRV_PCM_FMTBIT_S24_LE |\
				 SNDRV_PCM_FMTBIT_S32_LE)

static struct snd_soc_dai_driver aml_sed_dai[] = {
	{
		.name = "SED",
		.id = 1,
		.capture = {
			  .channels_min = 1,
			  .channels_max = 8,
			  .rates = AML_DAI_MIXER_RATES,
			  .formats = AML_DAI_MIXER_FORMATS,
		},
		.ops = &aml_sed_ops,
	},
};

static void sed_key_report(struct sed *p_sed)
{
	if (!p_sed)
		return;
	input_event(p_sed->input_device, EV_KEY, KEY_POWER, 1);
	input_sync(p_sed->input_device);
	input_event(p_sed->input_device, EV_KEY, KEY_POWER, 0);
	input_sync(p_sed->input_device);
}

static void sed_input_device_init(struct sed *pvad)
{
	pvad->input_device->name = "vad_keypad";
	pvad->input_device->phys = "vad_keypad/input3";
	pvad->input_device->dev.parent = pvad->dev;
	pvad->input_device->id.bustype = BUS_ISA;
	pvad->input_device->id.vendor  = 0x0001;
	pvad->input_device->id.product = 0x0001;
	pvad->input_device->id.version = 0x0100;
	pvad->input_device->evbit[0] = BIT_MASK(EV_KEY);
	pvad->input_device->keybit[BIT_WORD(BTN_0)] = BIT_MASK(BTN_0);
	set_bit(KEY_POWER, pvad->input_device->keybit);
}

static int sed_platform_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct device *dev = &pdev->dev;
	struct sed *p_sed = dev_get_drvdata(dev);

	dev_pm_set_wake_irq(dev, p_sed->irq_wakeup);

	if (!p_sed->sed_load_bin)
		pr_info("%s:no load bin!, need load\n", __func__);
	if (p_sed->mic_source == PDM_MIC) {
		if (!p_sed->asla_active) {
			sed_clk_enable(p_sed);
			sed_init(p_sed, 1);
			sed_enable(p_sed->audio_top_sed_map, 1);
			p_sed->sed_enable = 1;
		}
	}
	//dev_pm_set_wake_irq(dev, p_sed->irq_wakeup);
	return 0;
}

static int sed_platform_resume(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sed *p_sed = dev_get_drvdata(dev);

	dev_pm_clear_wake_irq(dev);
	if (get_resume_method() == VAD_WAKEUP)
		sed_key_report(p_sed);
	return 0;
}

static void sed_platform_shutdown(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct sed *p_sed = dev_get_drvdata(dev);

	dev_pm_set_wake_irq(dev, p_sed->irq_wakeup);
}

static irqreturn_t sed_wakeup_isr(int irq, void *data)
{
	struct sed *p_sed = (struct sed *)data;

	if (p_sed->sed_enable)
		pr_info("%s:%0x\n", __func__, sed_irq_status(p_sed->sed_map));
	pm_wakeup_hard_event(p_sed->dev);
	return IRQ_HANDLED;
}

static int sed_platform_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct sed *sed_p;
	struct device *dev = &pdev->dev;
	struct device_node *node_prt = NULL;
	struct device_node *np_src = NULL;
	struct platform_device *dev_src = NULL;
	int ret = 0;

	sed_p = devm_kzalloc(dev, sizeof(struct sed), GFP_KERNEL);
	if (!sed_p)
		return -ENOMEM;
	sed_p->dev = dev;
	dev_set_drvdata(dev, sed_p);
	node_prt = of_get_parent(node);
	if (!node_prt)
		return -ENXIO;

	np_src = of_parse_phandle(node, "mic-src", 0);
	if (np_src) {
		dev_src = of_find_device_by_node(np_src);
		of_node_put(np_src);
		sed_p->mic_src = platform_get_drvdata(dev_src);
		pr_info("%s, mic-src found\n", __func__);
	}

	sed_p->rc_pll = devm_clk_get(&pdev->dev, "rc_pll");
	if (IS_ERR(sed_p->rc_pll)) {
		dev_err(&pdev->dev,
			"Can't retrieve pll clock\n");
		ret = PTR_ERR(sed_p->rc_pll);
		goto err;
	}

	sed_p->pdm_sysclk_srcpll = devm_clk_get(&pdev->dev, "sysclk_srcpll");
	if (IS_ERR(sed_p->pdm_sysclk_srcpll)) {
		dev_err(&pdev->dev,
			"Can't retrieve pll clock\n");
		ret = PTR_ERR(sed_p->pdm_sysclk_srcpll);
		goto err;
	}

	sed_p->pdm_dclk_srcpll = devm_clk_get(&pdev->dev, "dclk_srcpll");
	if (IS_ERR(sed_p->pdm_dclk_srcpll)) {
		dev_err(&pdev->dev,
			"Can't retrieve data src clock\n");
		ret = PTR_ERR(sed_p->pdm_dclk_srcpll);
		goto err;
	}

	sed_p->pdm_sysclk = devm_clk_get(&pdev->dev, "pdm_sysclk");
	if (IS_ERR(sed_p->pdm_sysclk)) {
		dev_err(&pdev->dev,
			"Can't retrieve clk_pdm_sysclk clock\n");
		ret = PTR_ERR(sed_p->pdm_sysclk);
		goto err;
	}

	sed_p->pdm_dclk = devm_clk_get(&pdev->dev, "pdm_dclk");
	if (IS_ERR(sed_p->pdm_dclk)) {
		dev_err(&pdev->dev,
			"Can't retrieve clk_pdm_dclk clock\n");
		ret = PTR_ERR(sed_p->pdm_dclk);
		goto err;
	}

	sed_p->sed_clk_pll = devm_clk_get(&pdev->dev, "sed_clk_pll");
	if (IS_ERR(sed_p->sed_clk_pll)) {
		dev_err(&pdev->dev,
			"Can't retrieve sed_clk_pll clock\n");
		ret = PTR_ERR(sed_p->sed_clk_pll);
		goto err;
	}

	sed_p->sed_clk = devm_clk_get(&pdev->dev, "sed_clk");
	if (IS_ERR(sed_p->sed_clk)) {
		dev_err(&pdev->dev,
			"Can't retrieve sed_clk clock\n");
		ret = PTR_ERR(sed_p->sed_clk);
		goto err;
	}

	sed_p->sed_map = regmap_resource(&pdev->dev, "sed_reg");
	if (IS_ERR_OR_NULL(sed_p->sed_map)) {
		dev_err(dev, "Can't sed regmap!!\n");
		return -ENXIO;
	}
	sed_p->audio_top_sed_map = regmap_resource(&pdev->dev, "audio_top_reg");
	if (IS_ERR_OR_NULL(sed_p->audio_top_sed_map)) {
		dev_err(dev, "Can't aduio top sed regmap!!\n");
		return -ENXIO;
	}
	sed_p->irq_wakeup = platform_get_irq_byname(pdev, "irq_wakeup");
	if (sed_p->irq_wakeup < 0) {
		dev_err(dev, "Failed to get irq_wakeup:%d\n",
			sed_p->irq_wakeup);
		return -ENXIO;
	}
	ret = request_irq(sed_p->irq_wakeup,
			sed_wakeup_isr, IRQF_SHARED | IRQF_NO_SUSPEND, "vad_wakeup",
			sed_p);
	if (ret) {
		dev_err(sed_p->dev, "failed to claim irq_wakeup %u, ret: %d\n",
					sed_p->irq_wakeup, ret);
		return -ENXIO;
	}

	ret = of_property_read_u32(node, "train_sample_count",
			&sed_p->pdm_sample_cnt);
	if (ret < 0)
		sed_p->pdm_sample_cnt = 18;

	ret = of_property_read_u32(node, "sed_mic_source",
			&sed_p->mic_source);
	if (ret < 0)
		sed_p->mic_source = PDM_MIC;

	ret = of_property_read_u32(node, "sed_mic_rate",
			&sed_p->rate);
	if (ret < 0)
		sed_p->mic_rate = MIC_16K;

	ret = of_property_read_u32(node, "sed_mic_bit",
			&sed_p->sample_bit);
	if (ret < 0)
		sed_p->sample_bit = 16;

	ret = of_property_read_u32(node, "sed_mic_channel",
			&sed_p->channel);
	if (ret < 0)
		sed_p->channel = 2;

	ret = of_property_read_u32(node, "sed_suspend_type",
			&sed_p->channel);
	if (ret < 0)
		sed_p->suspend_type = SUSPEND_MODE;
	sed_clk_enable(sed_p);
	load_sed_firmware(sed_p);

	sed_p->input_device = input_allocate_device();
	if (!sed_p->input_device) {
		kfree(sed_p);
		return -ENOMEM;
	}
	sed_input_device_init(sed_p);
	if (input_register_device(sed_p->input_device)) {
		input_free_device(sed_p->input_device);
		kfree(sed_p);
		return -EINVAL;
	}

	ret = devm_snd_soc_register_component(dev,
			&aml_sed_component[0],
			&aml_sed_dai[0], 1);
	if (ret) {
		dev_err(dev, "devm_snd_soc_register_component failed\n");
		return ret;
	}
	return 0;
err:
	return ret;
}

static const struct of_device_id sed_device_id[] = {
	{
		.compatible = "amlogic, sed",
	},
	{},
};

struct platform_driver sed_driver = {
	.driver = {
		.name = DRV_NAME,
		.of_match_table = sed_device_id,
	},
	.probe	 = sed_platform_probe,
	.suspend = sed_platform_suspend,
	.resume  = sed_platform_resume,
	.shutdown = sed_platform_shutdown,
};

int __init sed_drv_init(void)
{
	return platform_driver_register(&sed_driver);
}

void __exit sed_drv_exit(void)
{
	platform_driver_unregister(&sed_driver);
}

#ifndef MODULE
module_init(sed_drv_init);
module_exit(sed_drv_exit);
MODULE_AUTHOR("Amlogic, Inc.");
MODULE_DESCRIPTION("Amlogic Voice Activity Detection ASoc driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("Platform:" DRV_NAME);
MODULE_DEVICE_TABLE(of, sed_device_id);
#endif

