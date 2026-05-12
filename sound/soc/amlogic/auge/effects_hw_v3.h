/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __EFFECTS_HW_V3_H__
#define __EFFECTS_HW_V3_H__
#include <linux/types.h>
#include "ddr_mngr.h"

#define EQ_V3_BAND        (16)
#define AED_CLIP_THD_MAX 0x7FFFFF

void aed_v3_set_volume(unsigned int master_volume,
		    unsigned int lch_vol, unsigned int rch_vol);
void aed_v3_set_mixer_params(void);
void aed_v3_eq_taps(unsigned int eq1_taps);
void aed_status_set(void);
void aed_soft_reset(void);
void aed_v3_set_ram_coeff(int add, int len, unsigned int *params);
void aed_v3_get_ram_coeff(int add, int len, unsigned int *params);

#endif
