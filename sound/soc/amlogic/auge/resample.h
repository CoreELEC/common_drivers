/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __AML_AUDIO_RESAMPLE_H__
#define __AML_AUDIO_RESAMPLE_H__

#include "resample_hw.h"

int card_add_resample_kcontrols(struct snd_soc_card *card);

int resample_set(enum resample_idx id, enum samplerate_index index);

int get_resample_module_num(void);

int set_resample_source(enum resample_idx id, enum toddr_src src);

struct audioresample *get_audioresample(enum resample_idx id);

int get_resample_version_id(enum resample_idx id);

bool get_resample_enable(enum resample_idx id);

bool get_resample_enable_chnum_sync(enum resample_idx id);

#endif
