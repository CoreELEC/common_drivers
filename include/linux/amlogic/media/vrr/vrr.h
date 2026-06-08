/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_VRR_H_
#define _AML_VRR_H_
#include <linux/amlogic/media/vfm/vframe.h>

enum vrr_input_src_e {
	VRR_INPUT_TVIN = 0,
	VRR_INPUT_GPU = 1,
	VRR_INPUT_DOS = 2,
	VRR_INPUT_MAX,
};

enum vrr_output_src_e {
	VRR_OUTPUT_ENCP = 0,
	VRR_OUTPUT_ENCL = 1,
	VRR_OUTPUT_MAX,
};

#define VRR_NAME_LEN_MAX    30
struct vrr_device_s {
	char name[VRR_NAME_LEN_MAX];
	enum vrr_output_src_e output_src;
	unsigned int enable;
	unsigned int vline;
	unsigned int vline_max;
	unsigned int vline_min;
	unsigned int vfreq_max;
	unsigned int vfreq_min;
	unsigned int vfreq_panel_max;
	unsigned int vfreq_panel_min;

	void *dev_data;
	unsigned int (*lfc_switch)(void *dev_data, int fps);
	int (*disable_cb)(void *dev_data);
	unsigned int v_active;
};

struct vrr_notifier_data_s {
	enum vrr_input_src_e input_src;
	unsigned int target_vfreq_num;
	unsigned int target_vfreq_den;
	unsigned int dev_vfreq_max;
	unsigned int dev_vfreq_min;
	unsigned int line_dly;

	unsigned int vrr_mode;
	unsigned int lfc_en;
	unsigned int vrr_policy; /* 0:fix fps as vlock, 1:dynamic fps */
	unsigned int vrr_priority; /* 1:vrr instead vlock do low latency */
	u32 qms_en;
	unsigned int vrr_panel_max;
	unsigned int vrr_panel_min;
	unsigned int rx_vfreq_max;
	unsigned int rx_vfreq_min;
	unsigned int vrr_enable_cnt;
};

/* **********************************
 * IOCTL define
 * **********************************
 */
#define VRR_IOC_TYPE           'V'
#define VRR_IOC_GET_CAP        0x0
#define VRR_IOC_ENABLE         0x1
#define VRR_IOC_DISABLE        0x2
#define VRR_IOC_GET_EN         0x3
#define VRR_IOC_GET_SUPPORT    0x4

#define VRR_IOC_CMD_GET_CAP        _IOR(VRR_IOC_TYPE, VRR_IOC_GET_CAP, unsigned int)
#define VRR_IOC_CMD_ENABLE         _IO(VRR_IOC_TYPE, VRR_IOC_ENABLE)
#define VRR_IOC_CMD_DISABLE        _IO(VRR_IOC_TYPE, VRR_IOC_DISABLE)
#define VRR_IOC_CMD_GET_EN         _IOR(VRR_IOC_TYPE, VRR_IOC_GET_EN, unsigned int)
#define VRR_IOC_CMD_GET_SUPPORT    _IOR(VRR_IOC_TYPE, VRR_IOC_GET_SUPPORT, unsigned int)

/* **********************************
 * NOTIFY define
 * **********************************
 */
/* original event */
#define FRAME_LOCK_EVENT_ON		BIT(0)
#define FRAME_LOCK_EVENT_OFF		BIT(1)
#define FRAME_LOCK_EVENT_VRR_ON_MODE	BIT(2)
#define FRAME_LOCK_EVENT_VRR_OFF_MODE	BIT(3)
#define VRR_EVENT_UPDATE		BIT(4)
#define VRR_EVENT_LFC_ON		BIT(5)
#define VRR_EVENT_LFC_OFF		BIT(6)
#define VRR_EVENT_GET_STATE		BIT(7)

/* **********************************
 * LINE define
 * **********************************
 */
/* check different type change display scale */
#define VRR_CROP			1
#define VRR_AXIS			2
#define VRR_DOT				3

/* ************************************************************* */
#ifdef CONFIG_AMLOGIC_MEDIA_VRR
int aml_vrr_state(void);
int aml_vrr_func_en(int flag);
int aml_vrr_flc_update(int flag, int fps);

int aml_vrr_register_device(struct vrr_device_s *vrr_dev, int index);
int aml_vrr_unregister_device(int index);
/* atomic notify */
int aml_vrr_atomic_notifier_register(struct notifier_block *nb);
int aml_vrr_atomic_notifier_unregister(struct notifier_block *nb);
int aml_vrr_atomic_notifier_call_chain(unsigned long event, void *v);
#else
static inline int aml_vrr_state(void)
{
	return 0;
}

static inline int aml_vrr_func_en(int flag)
{
	return 0;
}

static inline int aml_vrr_register_device(struct vrr_device_s *vrr_dev,
					  int index)
{
	return 0;
}

static inline int aml_vrr_unregister_device(int index)
{
	return 0;
}

static inline int aml_vrr_atomic_notifier_register(struct notifier_block *nb)
{
	return 0;
}

static inline int aml_vrr_atomic_notifier_unregister(struct notifier_block *nb)
{
	return 0;
}

static inline int aml_vrr_atomic_notifier_call_chain(unsigned long event,
						     void *v)
{
	return 0;
}
#endif
bool frame_lock_type_vrr_lock(void);
unsigned int vrr_check_frame_rate_min_hz(void);
void vrr_crop_update_delay_line(u32 line, u16 axis_y,
u16 dst_w, u16 dst_h, u16 type);
struct aml_vrr_drv_s *aml_vrr_drv_active_sel(void);
unsigned int vrr_instead_vlock(void);
bool vrr_small_window(void);
void vrr_check_dec(unsigned int state);
bool vrr_hdr_latency(void);
bool vrr_check_input_to_out(struct vframe_s *vf);
bool vrr_check_window_change(void);

#endif
