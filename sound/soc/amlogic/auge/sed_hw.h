/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __SED_HW_H__
#define __SED_HW_H__

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/soc.h>
#include <sound/pcm_params.h>
#include <linux/amlogic/pm.h>
#include <linux/amlogic/clk_measure.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/notifier.h>
#include "ddr_mngr.h"
#include "audio_io.h"
#include "../common/iomapres.h"
#include "regs.h"
#include "iomap.h"

#include "sed.h"
#include "sed_reg.h"

int sed_enable(struct regmap *audio_top_sed_map, int enable);
int sed_top_ctrl_init(struct regmap *audio_top_sed_map);
int pdm_dclk_coefficient(struct regmap *audio_top_sed_map, int bypss);
int sed_clk_set(struct regmap *sed_map);
void sed_parameter_set(struct sed_cfg_t *cfg);
int sed_mel_filter_set(struct sed_cfg_t *cfg, struct regmap *sed_map, int rate);
int sed_chain_set(struct regmap *sed_map);
unsigned int sed_irq_status(struct regmap *audio_top_sed_map);
int sed_load_bin(struct regmap *sed_map, const unsigned int *p_data, int len);

#endif
