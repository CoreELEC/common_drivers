// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include "effects_hw_v3.h"
#include "regs.h"
#include "iomap.h"
#include "tdm_hw.h"
#include "spdif_hw.h"

void aed_v3_set_ram_coeff(int add, int len, unsigned int *params)
{
	int i;
	unsigned int *p = params;

	eqdrc_write(AEQ_COEF_ADDR, add);
	for (i = 0; i < len; i++, p++)
		eqdrc_write(AEQ_COEF_DATA, *p);
}

void aed_v3_get_ram_coeff(int add, int len, unsigned int *params)
{
	int i;
	unsigned int *p = params;

	eqdrc_write(AEQ_COEF_ADDR, add);
	for (i = 0; i < len; i++, p++)
		*p = eqdrc_read(AEQ_COEF_DATA);
}

void aed_v3_set_mixer_params(void)
{
	eqdrc_write(AEQ_MIX0_LL, 0x00800000);
	eqdrc_write(AEQ_MIX0_RL, 0x0);
	eqdrc_write(AEQ_MIX0_LR, 0x0);
	eqdrc_write(AEQ_MIX0_RR, 0x00800000);
	eqdrc_write(AED_CLIP_THD_V3, AED_CLIP_THD_MAX);
}

void aed_v3_eq_taps(unsigned int eq1_taps)
{
	if (eq1_taps > EQ_V3_BAND) {
		pr_err("Error EQ1_Tap = %d\n", eq1_taps);
		return;
	}
	eqdrc_update_bits(AED_STATUS_CTRL, 0x1f << 8, eq1_taps << 8);
}

void aed_status_set(void)
{
	eqdrc_update_bits(AED_STATUS_CTRL,
						0x7 << 20 | 0x3 << 24 | 0x7 << 17,
						0x7 << 20 | 0x3 << 24 | 0x7 << 17);
}

void aed_v3_set_volume(unsigned int master_vol,
			unsigned int lch_vol, unsigned int rch_vol)
{
	eqdrc_write(AED_EQ_VOLUME_VAL,
			(master_vol << 20) |	/* master volume: 0dB */
			(lch_vol << 10) |	/* channel 2 volume: 0dB */
			(rch_vol << 0)		/* channel 1 volume: 0dB */
	);
	eqdrc_write(AED_EQ_VOLUME_STEP_CNT,
	/*volume step: 0x03:0.5dB, 0x02:0.5dB, 0x01:0.25dB, 0x00:0.125dB*/
			(0x0 << 30) |
	/*volume master step: 0x03:0.5dB, 0x02:0.5dB, 0x01:0.25dB, 0x00:0.125dB*/
			(0x0 << 28) |
	/*volume slew cnt:40ms from -120dB~0dB*/
			(0x2 << 12) |
	/*volume master slew cnt:40ms from -120dB~0dB*/
			(0x2 << 0)
	);
	/* unmute */
	eqdrc_write(AED_MUTE_CTRL, 0x0);
}

/* need soft reset after ram set && get, to make ram data effective */
void aed_soft_reset(void)
{
	eqdrc_write(AEQ_SOFT_REST, 0xf);
	eqdrc_write(AEQ_SOFT_REST, 0);
}
