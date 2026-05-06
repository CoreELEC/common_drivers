/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMDV_EXT_H_
#define _AMDV_EXT_H_

#ifndef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/amvecm/amvecm.h>

#define AMDV_OUTPUT_MODE_IPT			0
#define AMDV_OUTPUT_MODE_IPT_TUNNEL		1
#define AMDV_OUTPUT_MODE_HDR10			2
#define AMDV_OUTPUT_MODE_SDR10			3
#define AMDV_OUTPUT_MODE_SDR8			4
#define AMDV_OUTPUT_MODE_BYPASS			5

#define MAX_ZERO_COUNT  40

enum OSD_INDEX {
	OSD1_INDEX = 0,
	OSD2_INDEX = 1,
	OSD3_INDEX = 2,
	OSD4_INDEX = 3,
	OSD_MAX_INDEX = 4
};

static inline bool is_amdv_enable(void) { return false; }
static inline bool is_amdv_dpss_path(void) { return false; }
static inline bool is_amdv_on(void) { return false; }
static inline bool is_amdv_stb_mode(void) { return false; }
static inline bool for_amdv_certification(void) { return false; }
static inline void dv_vf_light_reg_provider(void) {}
static inline void dv_vf_light_unreg_provider(void) {}
static inline void amdv_update_backlight(void) {}
static inline int is_amdv_frame(struct vframe_s *vf) { return false; }
static inline void amdv_set_toggle_flag(int flag) {}
static inline int get_dv_support_info(void) { return false; }
static inline bool support_multi_core1(void) { return false; }
static inline bool is_hdmi_ll_as_hdr10(void) { return false; }
static inline int get_amdv_mode(void) { return -1; }
static inline void set_amdv_mode(int mode) {}
static inline int get_amdv_src_format(enum vd_path_e vd_path, struct vframe_s *vf) { return 0; }
static inline int get_amdv_ll_policy(void) { return 0; }
static inline void set_amdv_ll_policy(u32 policy) {}
static inline void set_amdv_enable(bool enable) {}
static inline void set_amdv_policy(int policy) {}
static inline int amdv_notifier_call_chain(unsigned long val, void *v) { return 0; }
static inline int register_osd_func(int (*get_osd_enable_status)(u32 index)) { return 0; }

#endif
#endif
