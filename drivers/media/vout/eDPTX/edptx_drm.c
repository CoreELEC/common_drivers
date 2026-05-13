// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/clk.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX_notify.h>
#include "edptx_common.h"
#include <linux/component.h>
#include <drm/amlogic/meson_drm_bind.h>

static int meson_eDP_bind(struct device *dev, struct device *master, void *data);
static void meson_eDP_unbind(struct device *dev, struct device *master, void *data);

struct drm_eDP_wrapper_s {
	struct meson_panel_dev drm_eDPTX_instance;
	struct edptx_drv_s *edp_drv;
	int drm_type;
	int drm_id;
};

#define to_drm_edptx_wrapper(x)	container_of(x, struct drm_eDP_wrapper_s, drm_eDPTX_instance)

static struct drm_eDP_wrapper_s drm_eDP_wrappers[eDPTX_MAX_DRV];

static int edptx_drm_add_modes(struct meson_panel_dev *dptx_drm_dev,
				struct drm_display_mode *pmode, u8 vmd_idx)
{
	struct drm_eDP_wrapper_s *wrapper =
		(struct drm_eDP_wrapper_s *)to_drm_edptx_wrapper(dptx_drm_dev);
	struct edptx_drv_s *edp = wrapper->edp_drv;
	struct dptx_detail_timing_s *raw_timing;
	struct dptx_vmode_s *vmd_p;
	u64 pclk;

	if (vmd_idx == 0xff) {
		vmd_p = &DPTX_SafeMode_640x480_vmode;
		raw_timing = &DPTX_SafeMode_640x480_timing;
	} else {
		vmd_p = &edp->vmode_mgr.vmodes[vmd_idx];
		raw_timing = &edp->panel_data.timing[vmd_p->base_dtd_idx];
	}

	pmode->type        = DRM_MODE_TYPE_DRIVER;

	pmode->hdisplay    = raw_timing->h_act;
	pmode->hsync_start = raw_timing->h_act + raw_timing->h_fp;
	pmode->hsync_end   = raw_timing->h_act + raw_timing->h_fp + raw_timing->h_pw;
	pmode->htotal      = raw_timing->h_period;
	pmode->flags      |= raw_timing->ctrl & BIT(1) ?
				DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;
	pmode->vdisplay    = raw_timing->v_act;
	pmode->vsync_start = raw_timing->v_act + raw_timing->v_fp;
	pmode->vsync_end   = raw_timing->v_act + raw_timing->v_fp + raw_timing->v_pw;
	pmode->vtotal      = raw_timing->v_period;
	pmode->flags      |= raw_timing->ctrl & BIT(2) ?
				DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;
	pmode->width_mm    = raw_timing->h_size;
	pmode->height_mm   = raw_timing->v_size;

	switch (vmd_p->fr_adv) {
	case 0: //fr int
		pclk = (vmd_p->fr100_int + 50) / 100;
		pclk = pclk * raw_timing->h_period * raw_timing->v_period;
		break;
	case 1: //fr frac
		pclk = vmd_p->fr100_int;
		pclk *= 10;
		pclk = pclk * raw_timing->h_period * raw_timing->v_period;
		pclk = dptx_div_around(pclk, 1001);
		break;
	case 2: //raw fr
	default:
		pclk = raw_timing->pclk;
		break;
	}
	pmode->clock = dptx_div_around(pclk, 1000);
	//edp->act_timing.pclk = pclk;
	pclk = dptx_div_around(pclk * 1000, edp->act_timing.v_period);
	pclk = dptx_div_around(pclk, edp->act_timing.h_period);

	//edp->act_timing.cfmt = edp_cfmt_tb[edp->vmode_mgr.vmode_cfmt_sel].cfmt_id;

	//str_add_vmode ??
	//sprintf(&pmode->name, "%ux%up%uhz")
	__str_add_vmode(edp, pmode->name, vmd_p, dptx_div_around(pclk, 1000));

	eDP_PR(edp, "%s: %s, clock=%dkHz, htotal=%d, vtotal=%d",
		__func__, pmode->name, pmode->clock, pmode->htotal, pmode->vtotal);
	return 0;
}

static int get_edptx_modes(struct meson_panel_dev *dptx_drm_dev,
		struct drm_display_mode **modes, int *num)
{
	struct drm_eDP_wrapper_s *wrapper = to_drm_edptx_wrapper(dptx_drm_dev);
	struct edptx_drv_s *edp = wrapper->edp_drv;
	u8 mode_idx = 0;

	struct drm_display_mode *nmodes;
	unsigned int i = 0, mode_cnt = 0, valid_mode_cnt = 0;

	if (!edp)
		return -EFAULT;
	//mode_cnt = edp->vmode_mgr.vmode_cnt;

	for (i = 0; i < DPTX_DRV_VMODE_MAX; i++) {
		if (edp->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID) {
			mode_cnt++;
			if (edp->vmode_mgr.vmodes[i].cfmt_support)
				valid_mode_cnt++;
		}
	}

	eDP_PR(edp, "%s: %u/%u", __func__, valid_mode_cnt, mode_cnt);

	nmodes = kcalloc(valid_mode_cnt + 1, sizeof(struct drm_display_mode), GFP_KERNEL);
	if (!nmodes) {
		*num = 0;
		return -ENOMEM;
	}

	for (i = 0; i < DPTX_DRV_VMODE_MAX; i++) {
		if (edp->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID &&
		    edp->vmode_mgr.vmodes[i].cfmt_support) {
			edptx_drm_add_modes(dptx_drm_dev, nmodes + mode_idx, i);
			mode_idx++;
		}
	}

	edptx_drm_add_modes(dptx_drm_dev, nmodes + mode_idx, 0xff);

	*num = valid_mode_cnt + 1;
	*modes = nmodes;

	return 0;
}

u8 edptx_drm_timing_find(struct edptx_drv_s *edp, struct drm_display_mode *pmode)
{
	u8 idx;
	u32 fr100;
	struct dptx_vmode_s *vmode_p;
	struct dptx_detail_timing_s *v_tm;
	u32 pclk, htotal, vtotal, h_active, v_active;

	if (!pmode)
		return 1;

	pclk = pmode->clock;
	htotal = pmode->htotal;
	vtotal = pmode->vtotal;
	h_active = pmode->hdisplay;
	v_active = pmode->vdisplay;
	eDP_PR(edp, "drm set %u[%u] * %u[%u] %u kHz",
		h_active, htotal, v_active, vtotal, pclk);

	pclk = pclk * 1000;
	pclk = dptx_div_around(pclk, vtotal);
	pclk = pclk * 100;
	pclk = dptx_div_around(pclk, htotal);
	// pclk is (frame_rate * 100) now

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		vmode_p = &edp->vmode_mgr.vmodes[idx];
		if (!(vmode_p->flag & VMODE_FLAG_VALID))
			continue;

		v_tm = &edp->panel_data.timing[vmode_p->base_dtd_idx];

		if (v_tm->h_act != h_active || v_tm->v_act != v_active ||
				v_tm->h_period != htotal || v_tm->v_period != vtotal)
			continue;

		if (vmode_p->fr_adv == 1) { //5994 case
			fr100 = vmode_p->fr100_int * 1000;
			fr100 = fr100 / 1001;
		} else {
			fr100 = vmode_p->fr100_int;
		}

		if (dptx_diff(pclk, fr100) <= 50) {
			eDP_PR(edp, "%s: match %s, %dx%d@%u.%02uhz", __func__,
				v_tm->name, v_tm->h_act, v_tm->v_act, fr100 / 100, fr100 % 100);
			edp->next_vmd_idx = idx;
			return 0;
		}
	}
	if (h_active == 640 && v_active == 480 && dptx_diff(pclk, 6000) <= 50) { // 640x480p
		edp->next_vmd_idx = 0xff;
		return 0;
	}

	edp->next_vmd_idx = 0xff;
	return 1;
}

static int edptx_set_apply_vmode(struct edptx_drv_s *edp,  enum vmode_e mode)
{
	int ret = 0;

	if ((mode & VMODE_MODE_BIT_MASK) != VMODE_eDP) {
		eDP_ERR(edp, "unsupport mode 0x%x", mode & VMODE_MODE_BIT_MASK);
		return -1;
	}

	// lcd_vrr_dev_register(pdrv);
	if (mode & VMODE_INIT_BIT_MASK) { //first boot: set clktree
		//lcd_clk_gate_switch(pdrv, 1);
		edp->status |= DPTX_STA_DISP_ON;
	} else if (edp->status & DPTX_STA_LINK_ON) { //mode change
		if (edp->next_vmd_idx == 0xff) {
			eDP_ERR(edp, "next vmode to 640x480p60hz");
			edptx_user_set_vmode(edp, edp->next_vmd_idx);
		} else if (edp->next_vmd_idx == edp->vmode_mgr.vmode_sel_idx) {
			eDP_ERR(edp, "next vmode same as current");
		} else {
			edptx_user_set_vmode(edp, edp->next_vmd_idx);
		}
	} else {
		//mode change by drm (disable + mode_change)
		if ((edp->status & DPTX_STA_DRV_READY) == 0)
			edptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)edp);
		edptx_notifier_call_chain(DPTX_PRIORITY_HPD_CHECK, (void *)edp);
		// aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
		// pdrv->status |= LCD_EVENT_ENABLE;
	}

	return ret;
}

static void edptx_drm_set_mode_timing(struct meson_panel_dev *panel,
				struct drm_display_mode *mode, enum vmode_e vmode)
{
	struct drm_eDP_wrapper_s *wrapper = to_drm_edptx_wrapper(panel);
	struct edptx_drv_s *edp;
	// struct lcd_vmode_list_s *temp_list = pdrv->vmode_mgr.vmode_list_header;
	u8 ret;

	if (!wrapper || !wrapper->edp_drv)
		return;

	edp = wrapper->edp_drv;

	if (!mode || ((vmode & 0xf) != VMODE_eDP)) {
		eDP_PR(edp, "null mode received", __func__);
		return;
	}

	ret = edptx_drm_timing_find(edp, mode);
	if (ret) {
		eDP_PR(edp, "%s: not matched", __func__);
		return;
	}
	edptx_set_apply_vmode(edp, vmode);
}

static int meson_eDP_bind(struct device *dev, struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct edptx_drv_s *edp = (struct edptx_drv_s *)dev_get_drvdata(dev);
	//int connector_type = 0;

	/*init drm instance*/
	drm_eDP_wrappers[edp->idx].edp_drv = edp;
	drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.base.ver = MESON_DRM_CONNECTOR_V10;
	// drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.add_modes = edptx_drm_add_modes;
	drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.get_modes = get_edptx_modes;
	drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.get_modes_vrr_range = NULL;
	drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.set_mode_timing = edptx_drm_set_mode_timing;

	drm_eDP_wrappers[edp->idx].drm_type = DRM_MODE_CONNECTOR_MESON_EDP_A + edp->idx;

	/*bind instance to drm*/
	if (bound_data->connector_component_bind) {
		drm_eDP_wrappers[edp->idx].drm_id =
			bound_data->connector_component_bind(bound_data->drm,
				drm_eDP_wrappers[edp->idx].drm_type,
				&drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.base);
		eDP_PR(edp, "%s: connector_type: 0x%x, drm_id: %d", __func__,
			drm_eDP_wrappers[edp->idx].drm_type,
			drm_eDP_wrappers[edp->idx].drm_id);
	} else {
		eDP_ERR(edp, "no bind func from drm");
	}

	drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.base.vout_serv = &edp->vout_server;
	// drm_panel_init(edp->drm_panel, )

	return 0;
}

static void meson_eDP_unbind(struct device *dev, struct device *master, void *data)
{
	struct meson_drm_bound_data *bound_data = data;
	struct edptx_drv_s *edp = (struct edptx_drv_s *)dev_get_drvdata(dev);

	if (bound_data->connector_component_unbind) {
		bound_data->connector_component_unbind(bound_data->drm,
			drm_eDP_wrappers[edp->idx].drm_type,
			&drm_eDP_wrappers[edp->idx].drm_eDPTX_instance.base);
		eDP_PR(edp, "%s: connector_type: 0x%x, drm_id: %d", __func__,
			drm_eDP_wrappers[edp->idx].drm_type,
			drm_eDP_wrappers[edp->idx].drm_id);
	} else {
		eDP_ERR(edp, "no unbind func from drm");
	}

	drm_eDP_wrappers[edp->idx].drm_id = 0;
}

static const struct component_ops meson_eDPTX_bind_ops = {
	.bind	= meson_eDP_bind,
	.unbind	= meson_eDP_unbind,
};

int dptx_drm_add(struct device *dev)
{
	/*bind to drm*/
	component_add(dev, &meson_eDPTX_bind_ops);

	return 0;
}

void dptx_drm_remove(struct device *dev)
{
	component_del(dev, &meson_eDPTX_bind_ops);
}
