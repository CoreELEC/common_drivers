/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __SED_H__
#define __SED_H__

#include <linux/input.h>
#include <linux/types.h>

struct sed_cfg_t {
	u32 downsample_rate;
	u32 preemp_coeff;
	u32 hop_length;
	u32 win_length;
	u32 pow_diff_dist;
	u32 pow_old_avglen;
	u32 pow_rampup_thr;
	u32 pow_cur_thr;
	u32 pow_cur_thr_high;
	u32 pow_freq_start;
	u32 pow_freq_end;
	u32 pow_discard_len;
	u32 pow_traceback_len;
	u32 pow_delay_len;
	u32 log_input_min;
	u32 log_zeropoint;
	u32 log_scale_r;
	u32 nn_stride_frame;
	u32 nn_post_frame;
};

enum sed_level {
	LEVEL_USER,
	LEVEL_KERNEL,
};

enum sed_source {
	PDM_MIC,
	TDM_MIC,
};

enum sed_suspend_type {
	FREEZE_MODE,
	/*cpu suspend*/
	SUSPEND_MODE,
	/*screen on/off envent*/
	EARLY_SUSPEND_MODE,
};

enum sed_mic_rate {
	MIC_16K,
	MIC_48K,
	MIC_96K,
};
#endif
