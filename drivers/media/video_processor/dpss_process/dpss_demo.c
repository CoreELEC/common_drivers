// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/kfifo.h>
#include <linux/delay.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/utils/amlog.h>
#include <linux/amlogic/media/ge2d/ge2d_func.h>
#include <linux/amlogic/media/di/dpss_interface.h>

#define DPSSPR_SIZE 16
#define WAIT_READY_Q_TIMEOUT 100
#define MAX_RECEIVE_WAIT_TIME 15  /*15ms*/

struct dpss_out_t {
	int index;
	bool on_use;
	struct vframe_s vf;
};

struct dpss_demo_dev {
	bool inited;
	struct dpss_init_parm init_parm;
	struct dpss_out_t dpss_out[DPSSPR_SIZE];
};

static struct dpss_demo_dev dev;
static unsigned int dpss_bypass = 1;

MODULE_PARM_DESC(dpss_bypass, "\n dpss_bypass\n");
module_param(dpss_bypass, uint, 0664);

static struct vframe_s *vfm_last_p;
static struct vframe_s vfm_last;

int get_dpss_out_free_index(void)
{
	int i = 0;
	int j = 0;

	while (1) {
		for (i = 0; i < DPSSPR_SIZE; i++) {
			if (!dev.dpss_out[i].on_use)
				break;
		}
		if (i == DPSSPR_SIZE) {
			j++;
			if (j > WAIT_READY_Q_TIMEOUT) {
				pr_err("dpss_out_buf is full, wait timeout!\n");
				return -1;
			}
			usleep_range(1000 * MAX_RECEIVE_WAIT_TIME,
				     1000 * (MAX_RECEIVE_WAIT_TIME + 1));
			pr_err("dpss_out_buf is full!!! need wait =%d\n", j);
			continue;
		} else {
			break;
		}
	}
	return i;
}

int get_dpss_out_index(struct vframe_s *vfm)
{
	int i = 0;

	for (i = 0; i < DPSSPR_SIZE; i++) {
		if (&dev.dpss_out[i].vf == vfm)
			break;
	}
	if (i == DPSSPR_SIZE)
		pr_err("not find vf in dpss_out list\n");

	return i;
}

int dpss_create_instance(struct dpss_init_parm *parm)
{
	int ret = 0;

	memset(&dev, 0, sizeof(struct dpss_demo_dev));
	dev.inited = true;
	dev.init_parm = *parm;

	vfm_last_p = NULL;

	return ret;
}

int dpss_destroy_instance(int index)
{
	int ret = 0;

	dev.inited = false;
	return ret;
}

enum DPSS_ERRORTYPE dpss_empty_input_buffer(int index, struct vframe_s *vfm)
{
	int ret = 0;
	int i;
	struct pp_info_t *pp_info;

	vfm->dpss_flg = 0;
	if (dpss_bypass) {
		pp_info = (struct pp_info_t *)vfm->pp_info;
		vfm->dpss_flg |= DPSS_FLG_BYPASS;
		dev.init_parm.ops.empty_input_done(dev.init_parm.ops.arg, vfm);
	} else {
		if (vfm->type & VIDTYPE_INTERLACE) {
			if (!vfm_last_p) {
				vfm_last_p = vfm;
				vfm_last = *vfm;
			} else {
				i = get_dpss_out_free_index();
				dev.dpss_out[i].on_use = true;
				dev.dpss_out[i].vf = vfm_last;
				dev.init_parm.ops.fill_output_done(dev.init_parm.ops.arg,
					&dev.dpss_out[i].vf);
				dev.init_parm.ops.empty_input_done(dev.init_parm.ops.arg,
					vfm_last_p);
				vfm_last_p = vfm;
				vfm_last = *vfm;
			}
		} else {
			i = get_dpss_out_free_index();
			dev.dpss_out[i].on_use = true;
			dev.dpss_out[i].vf = *vfm;
			dev.init_parm.ops.fill_output_done(dev.init_parm.ops.arg,
				&dev.dpss_out[i].vf);
			dev.init_parm.ops.empty_input_done(dev.init_parm.ops.arg, vfm);
		}
	}

	return ret;
}

enum DPSS_ERRORTYPE dpss_fill_output_buffer(int index, struct vframe_s *vfm)
{
	int ret = 0;
	int i;

	i = get_dpss_out_index(vfm);
	dev.dpss_out[i].on_use = false;

	return ret;
}

int dpss_release_keep_buf(struct vframe_s *vfm)
{
	int ret = 0;
	int i;

	i = get_dpss_out_index(vfm);
	dev.dpss_out[i].on_use = false;

	return ret;
}

int dpss_get_vf_info(struct vframe_s *vf, struct dpss_out_vf_info *dpss_out_info)
{
	struct pp_info_t *pp_info;

	pp_info = (struct pp_info_t *)vf->pp_info;

	dpss_out_info->idx_m = pp_info->main_index;
	dpss_out_info->idx_s = 0;
	return 0;
}

