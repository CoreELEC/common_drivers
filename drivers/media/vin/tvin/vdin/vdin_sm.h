/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __TVIN_STATE_MACHINE_H
#define __TVIN_STATE_MACHINE_H

#include "vdin_drv.h"

enum tvin_sm_status_e {
/* processing status from init to the finding of the 1st confirmed status */
	TVIN_SM_STATUS_NULL = 0,
	/* no signal - physically no signal */
	TVIN_SM_STATUS_NOSIG,
	/* unstable - physically bad signal */
	TVIN_SM_STATUS_UNSTABLE,
	 /* not supported - physically good signal & not supported */
	TVIN_SM_STATUS_NOTSUP,

	TVIN_SM_STATUS_PRESTABLE,
	 /* stable - physically good signal & supported */
	TVIN_SM_STATUS_STABLE,
};

#define RE_CONFIG_DV_EN		0x01
#define RE_CONFIG_HDR_EN	0x02
#define RE_CONFIG_ALLM_EN	0x04

#define VDIN_STABLED_CNT		50 /* no start_dec stable_cnt */
#define VDIN_SEND_EVENT_INTERVAL	50 /* Experience value can be adjusted */

#define VDIN_HIGH_FRAME_RATE_TH		100

enum vdin_sm_log_level {
	VDIN_SM_LOG_L_1 = 0x01,
	VDIN_SM_LOG_L_2 = 0x02,
	VDIN_SM_LOG_L_3 = 0x04,
	VDIN_SM_LOG_L_4 = 0x08,
	VDIN_SM_LOG_L_5 = 0x10,
	VDIN_SM_LOG_L_6 = 0x20,
	VDIN_SM_LOG_L_7 = 0x40, /* vtem and spd */
};

struct tvin_sm_s {
	enum tvin_sig_status_e sig_status;
	enum tvin_sm_status_e state;
	unsigned int state_cnt; /* STATE_NOSIG, STATE_UNSTABLE */
	unsigned int exit_nosig_cnt; /* STATE_NOSIG */
	unsigned int back_nosig_cnt; /* STATE_UNSTABLE */
	unsigned int back_stable_cnt; /* STATE_UNSTABLE */
	unsigned int exit_prestable_cnt; /* STATE_PRESTABLE */
	/* thresholds of state switched */
	int back_nosig_max_cnt;
	int atv_unstable_in_cnt;
	int atv_unstable_out_cnt;
	int atv_stable_out_cnt;
	int hdmi_unstable_out_cnt;
};

extern bool manual_flag;
extern bool atv_std_flag;
extern unsigned int vdin_get_prop_in_sm_en;

void tvin_smr(struct vdin_dev_s *pdev);
void tvin_smr_init(struct vdin_dev_s *devp);
void reset_tvin_smr(unsigned int index);

enum tvin_sm_status_e tvin_get_sm_status(int index);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void vdin_dump_vs_info(struct vdin_dev_s *devp);
#endif
void vdin_send_event(struct vdin_dev_s *devp, enum tvin_sg_chg_flg sts);
void vdin_update_prop(struct vdin_dev_s *devp);
bool vdin_is_cut_win_changed(struct vdin_dev_s *devp);
int vdin_hdmirx_fps_abn_check(struct vdin_dev_s *devp, unsigned int fps);
#endif

