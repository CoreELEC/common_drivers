// SPDX-License-Identifier: GPL-2.0
/*
 * sharebuffer driver
 *
 * Copyright (C) 2019 Amlogic, Inc. All rights reserved.
 *
 */

#include <sound/pcm.h>

#include <linux/amlogic/media/sound/aout_notify.h>

#include "sharebuffer.h"
#include "ddr_mngr.h"

#include "spdif_hw.h"

static int sharebuffer_spdifout_prepare(struct snd_pcm_substream *substream,
					struct frddr *fr, int spdif_id, int lane_i2s)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int bit_depth;
	struct iec958_chsts chsts;

	bit_depth = snd_pcm_format_width(runtime->format);

	spdifout_samesource_set(spdif_id,
				aml_frddr_get_fifo_id(fr),
				bit_depth,
				runtime->channels,
				true,
				lane_i2s);

	/* spdif to hdmitx */
	spdifout_to_hdmitx_ctrl(spdif_id);
	/* check and set channel status */
	spdif_get_channel_status_info(&chsts, runtime->rate);
	spdif_set_channel_status_info(&chsts, spdif_id);
	/* notify hdmitx audio */
	aout_notifier_call_chain(0x1, substream);

	return 0;
}

static int sharebuffer_spdifout_free(struct snd_pcm_substream *substream,
				     struct frddr *fr, int spdif_id)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	int bit_depth;

	bit_depth = snd_pcm_format_width(runtime->format);

	/* spdif b is always on */
	if (spdif_id != 1)
		spdifout_samesource_set(spdif_id,
					aml_frddr_get_fifo_id(fr),
					bit_depth,
					runtime->channels,
					false, 0);

	return 0;
}

void sharebuffer_enable(int sel, bool enable, bool reenable)
{
	if (sel < 0) {
		pr_err("Not support same source\n");
		return;
	} else if (sel < 3) {
		// TODO: same with tdm
	} else if (sel < 5) {
		/* same source with spdif a/b */
		spdifout_enable(sel - 3, enable, reenable);
	}
}

int sharebuffer_prepare(struct snd_pcm_substream *substream,
			void *pfrddr, int samesource_sel, int lane_i2s, int offset)
{
	struct frddr *fr = (struct frddr *)pfrddr;

	/* each module prepare, clocks and controls */
	if (samesource_sel < 0) {
		pr_err("Not support same source\n");
		return -EINVAL;
	} else if (samesource_sel < 3) {
		// TODO: same with tdm
	} else if (samesource_sel < 5) {
		/* same source with spdif a/b */
		sharebuffer_spdifout_prepare(substream,
					     fr, samesource_sel - 3, lane_i2s);
	}

	/* frddr, share buffer, src_sel1 */
	aml_frddr_select_dst_ss(fr, samesource_sel, 1, true);

	return 0;
}

int sharebuffer_free(struct snd_pcm_substream *substream,
		     void *pfrddr, int samesource_sel)
{
	struct frddr *fr = (struct frddr *)pfrddr;

	/* each module prepare, clocks and controls */
	if (samesource_sel < 0) {
		pr_err("Not support same source\n");
		return -EINVAL;
	} else if (samesource_sel < 3) {
		// TODO: same with tdm
	} else if (samesource_sel < 5) {
		/* same source with spdif a/b */
		sharebuffer_spdifout_free(substream, fr, samesource_sel - 3);
	}

	/* frddr, share buffer, src_sel1 */
	aml_frddr_select_dst_ss(fr, samesource_sel, 1, false);

	return 0;
}

int sharebuffer_trigger(int cmd, int samesource_sel, bool reenable)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		sharebuffer_enable(samesource_sel, true, reenable);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		sharebuffer_enable(samesource_sel, false, reenable);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

void sharebuffer_get_mclk_fs_ratio(int samesource_sel,
				   int *pll_mclk_ratio, int *mclk_fs_ratio)
{
	if (samesource_sel < 0) {
		pr_err("Not support same source\n");
	} else if (samesource_sel < 3) {
		// TODO: same with tdm
	} else if (samesource_sel < 5) {
		/* spdif a/b */
		*pll_mclk_ratio = 4;
		*mclk_fs_ratio = 128;
	}
}
