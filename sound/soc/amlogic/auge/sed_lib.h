/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __SED_LIB_H__
#define __SED_LIB_H__

int aml_snd_pcm_lib_xfer(struct snd_pcm_substream *substream, void *data,
		snd_pcm_uframes_t size);

static inline snd_pcm_sframes_t
aml_snd_pcm_kernel_read(struct snd_pcm_substream *substream,
			void *buf, snd_pcm_uframes_t frames)
{
	return aml_snd_pcm_lib_xfer(substream, buf, frames);
}
#endif
