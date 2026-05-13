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
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/clk.h>
#include <linux/sched/clock.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <linux/amlogic/media/vout/eDPTX/eDPTX_notify.h>
#include <drm/amlogic/meson_connector_dev.h>
#include "./edptx_common.h"
#include <linux/sched/clock.h>

/* **************************************************
 * vout server api
 * **************************************************
 */
static struct vinfo_s *dptx_get_current_info(void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return NULL;
	return &edp->vinfo;
}

static int dptx_check_same_vmodeattr(char *mode, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	eDP_DBG(edp, "%s: %s", __func__, mode);

	return 1;
}

static int dptx_vmode_is_supported(enum vmode_e mode, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	eDP_DBG(edp, "%s: 0x%x", __func__, mode);

	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_eDP)
		return true;
	return false;
}

static int dptx_set_current_vmode(enum vmode_e mode, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	//unsigned long long local_time[2];
	int ret = 0;

	if (!edp)
		return -1;

	if ((mode & VMODE_MODE_BIT_MASK) != VMODE_eDP) {
		eDP_ERR(edp, "unsupport mode 0x%x", mode & VMODE_MODE_BIT_MASK);
		return -1;
	}

	// lcd_vrr_dev_register(pdrv);
	if (mode & VMODE_INIT_BIT_MASK) { //first boot: set clktree
		//lcd_clk_gate_switch(pdrv, 1);
		//bypass
		edp->status |= DPTX_STA_DISP_ON;
	} else if (edp->status & DPTX_STA_LINK_ON) { //mode change by vout
		if (edp->next_vmd_idx == 0xff) {
			eDP_ERR(edp, "next vmode to 640x480p60hz");
			edptx_user_set_vmode(edp, edp->next_vmd_idx);
		} else if (edp->next_vmd_idx == edp->vmode_mgr.vmode_sel_idx) {
			eDP_ERR(edp, "next vmode same as current");
		} else {
			edptx_user_set_vmode(edp, edp->next_vmd_idx);
		}
	} else if (!(edp->status & DPTX_STA_LINK_ON)) {
		//mode change by drm (disable + mode_change)
		if ((edp->status & DPTX_STA_DRV_READY) == 0)
			edptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)edp);
		edptx_notifier_call_chain(DPTX_PRIORITY_HPD_CHECK, (void *)edp);
		// aml_lcd_notifier_call_chain(LCD_EVENT_ENABLE, (void *)pdrv);
		// pdrv->status |= LCD_EVENT_ENABLE;
	}

	return ret;
}

static int dptx_vout_disable(enum vmode_e cur_vmod, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return -1;

	//lcd_vrr_dev_unregister(pdrv);

	edptx_notifier_call_chain(DPTX_EVENT_DISABLE, (void *)edp);

	return 0;
}

static int dptx_vout_set_state(int index, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return -1;

	eDP_PR(edp, "%s: viu:[%d -> %d]", __func__, edp->viu_sel, index);
	edp->viu_sel = index;

	return 0;
}

static int dptx_vout_clr_state(int index, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return -1;

	eDP_PR(edp, "%s: viu=%d, clear_viu=%d", __func__, edp->viu_sel, index);
	if (edp->viu_sel == index)
		edp->viu_sel = 0;

	return 0;
}

static int dptx_vout_get_state(void *data)
{
	//struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	//return edp->vout_state;
	return 0;
}

static int dptx_vout_get_disp_cap(char *buf, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;
	u8 idx;
	u32 fr100, str_n = 0;
	struct dptx_vmode_s *vmode_p;

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		vmode_p = &edp->vmode_mgr.vmodes[idx];
		if (!(vmode_p->flag & VMODE_FLAG_VALID))
			continue;
		if (!(vmode_p->cfmt_support))
			continue;
		if (vmode_p->fr_adv == 1) { //5994 case
			fr100 = vmode_p->fr100_int * 1000;
			fr100 = fr100 / 1001;
		} else {
			fr100 = vmode_p->fr100_int;
		}

		str_n += __str_add_vmode(edp, buf + str_n, vmode_p, fr100 / 100);
		str_n += sprintf(buf + str_n, "\n");
	}
	return str_n;
}

static enum vmode_e dptx_validate_vmode(char *mode, unsigned int frac, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp || !mode)
		return VMODE_MAX;

	eDP_PR(edp, "%s: %s (curr_mode=%s)", __func__, mode, edp->vinfo.name);

	if (edptx_vmode_str_check(edp, mode))
		return VMODE_MAX;

	return VMODE_eDP;
}

static int dptx_set_vframe_rate_hint(int duration, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	eDP_PR(edp, "%s: %d", __func__, duration);

	return 0;
}

static int dptx_get_vframe_rate_hint(void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return 0;

	return 0;
}

static void dptx_vout_debug_test(unsigned int num, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return;

	dptx_debug_test(edp, num);
}

static void edptx_vout_set_bl_brightness(int brightness, void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return;
}

static int dptx_vout_get_bl_brightness(void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return 0;

	if ((edp->status & DPTX_STA_DISP_ON) == 0)
		return 1;

	return 1;
}

static int dptx_early_suspend(void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return -1;

	if ((edp->status & DPTX_STA_PROBE_DONE) == 0)
		return 0;

	edptx_notifier_call_chain(DPTX_EVENT_DISP_OFF, (void *)edp);
	eDP_PR(edp, "%s finished (0x%x)", __func__, edp->status);

	return 0;
}

static int dptx_late_resume(void *data)
{
	struct edptx_drv_s *edp = (struct edptx_drv_s *)data;

	if (!edp)
		return -1;

	if ((edp->status & DPTX_STA_PROBE_DONE) == 0)
		return 0;

	if ((edp->status & DPTX_STA_DRV_READY) == 0)
		edptx_notifier_call_chain(DPTX_EVENT_TX_READY, (void *)edp);
		// edptx_notifier_call_chain(DPTX_EVENT_PREPARE, (void *)edp);

	edptx_notifier_call_chain(DPTX_EVENT_HPD_CHECK, edp);
	if (edp->status & DPTX_STA_HPD_HIGH)
		edptx_notifier_call_chain(DPTX_EVENT_PLUG_IN, edp);
	else
		edptx_notifier_call_chain(DPTX_EVENT_HPD_RESTORE, edp);

	eDP_PR(edp, "%s finished (0x%x)", __func__, edp->status);

	return 0;
}

//bind 3 vout op
void edptx_vout_server_init(struct edptx_drv_s *edp)
{
	char *curr_vout_connector;//, *curr_vout_mode;
	char edp_connector[8];

	edp->viu_sel = 0;

	snprintf(edp_connector, 7, "EDP-%c", 'A' + edp->idx);

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	curr_vout_connector = get_uboot_connector0_type();
	// curr_vout_mode = get_vout_mode_uboot();
	if (!strcmp(edp_connector, curr_vout_connector))
		edp->viu_sel = 1;
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	curr_vout_connector = get_uboot_connector1_type();
	// curr_vout_mode = get_vout2_mode_uboot();
	if (!strcmp(edp_connector, curr_vout_connector))
		edp->viu_sel = 2;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	curr_vout_connector = get_uboot_connector2_type();
	// curr_vout_mode = get_vout3_mode_uboot();
	if (!strcmp(edp_connector, curr_vout_connector))
		edp->viu_sel = 3;
#endif

	edp->vout_server.connector_type = DRM_MODE_CONNECTOR_MESON_EDP_A + edp->idx;

	edp->vout_server.op.get_vinfo = dptx_get_current_info;
	edp->vout_server.op.set_vmode = dptx_set_current_vmode;
	edp->vout_server.op.validate_vmode = dptx_validate_vmode;
	edp->vout_server.op.check_same_vmodeattr = dptx_check_same_vmodeattr;
	edp->vout_server.op.vmode_is_supported = dptx_vmode_is_supported;
	edp->vout_server.op.disable = dptx_vout_disable;
	edp->vout_server.op.set_state = dptx_vout_set_state;
	edp->vout_server.op.clr_state = dptx_vout_clr_state;
	edp->vout_server.op.get_state = dptx_vout_get_state;
	edp->vout_server.op.get_disp_cap = dptx_vout_get_disp_cap;
	edp->vout_server.op.set_vframe_rate_hint = dptx_set_vframe_rate_hint;
	edp->vout_server.op.get_vframe_rate_hint = dptx_get_vframe_rate_hint;
	edp->vout_server.op.set_bist = dptx_vout_debug_test;
	edp->vout_server.op.set_bl_brightness = edptx_vout_set_bl_brightness;
	edp->vout_server.op.get_bl_brightness = dptx_vout_get_bl_brightness;
	edp->vout_server.op.vout_suspend = dptx_early_suspend;
	edp->vout_server.op.vout_resume = dptx_late_resume;
	edp->vout_server.data = (void *)edp;
	edp->vout_server.name = kzalloc(32, GFP_KERNEL);
	if (edp->vout_server.name)
		snprintf(edp->vout_server.name, 30, "edptx_%c_vout_server", '0' + edp->idx);

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	eDP_PR(edp, "regist: %s", edp->vout_server.name);
	vout_register_server(&edp->vout_server);
#endif
}

void edptx_vout_server_deinit(struct edptx_drv_s *edp)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_unregister_server(&edp->vout_server);
#endif
}
