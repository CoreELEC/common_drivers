// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include "hdmitx_log.h"

const struct hdmi_timing *hdmitx_mode_match_timing_name(const char *name);

/*!!Only one instance supported.*/
static struct hdmitx_common *global_tx_common;
static struct hdmitx_hw_common *global_tx_hw;
static const struct dv_info dv_dummy;

void hdmi_physical_size_to_vinfo(struct hdmitx_common *tx_comm)
{
	u32 width, height;
	struct vinfo_s *info = &tx_comm->hdmitx_vinfo;

	if (info->mode == VMODE_HDMI) {
		width = tx_comm->rxcap.physical_width;
		height = tx_comm->rxcap.physical_height;
		if (width == 0 || height == 0) {
			info->screen_real_width = info->aspect_ratio_num;
			info->screen_real_height = info->aspect_ratio_den;
		} else {
			info->screen_real_width = width;
			info->screen_real_height = height;
		}
		HDMITX_INFO("update physical size: %d %d\n",
			info->screen_real_width, info->screen_real_height);
	}
}

void set_dummy_dv_info(struct vout_device_s *vdev)
{
	vdev->dv_info = &dv_dummy;
};

void hdrinfo_to_vinfo(struct hdr_info *hdrinfo, struct hdmitx_common *tx_comm)
{
	memcpy(hdrinfo, &tx_comm->rxcap.hdr_info, sizeof(struct hdr_info));
	hdrinfo->colorimetry_support = tx_comm->rxcap.colorimetry_data;
}

void rxlatency_to_vinfo(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *info = &tx_comm->hdmitx_vinfo;

	info->rx_latency.vLatency = tx_comm->rxcap.vLatency;
	info->rx_latency.aLatency = tx_comm->rxcap.aLatency;
	info->rx_latency.i_vLatency = tx_comm->rxcap.i_vLatency;
	info->rx_latency.i_aLatency = tx_comm->rxcap.i_aLatency;
}

void edidinfo_attach_to_vinfo(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *info = &tx_comm->hdmitx_vinfo;
	struct vout_device_s *vdev = tx_comm->vdev;

	hdrinfo_to_vinfo(&info->hdr_info, tx_comm);
	rxlatency_to_vinfo(tx_comm);
	vdev->dv_info = &tx_comm->rxcap.dv_info;
	hdmi_physical_size_to_vinfo(tx_comm);
	memcpy(info->hdmichecksum, tx_comm->rxcap.hdmichecksum, 10);
}

void edidinfo_detach_to_vinfo(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *info = &tx_comm->hdmitx_vinfo;
	struct vout_device_s *vdev = tx_comm->vdev;

	memset(&info->hdr_info, 0, sizeof(info->hdr_info));
	memset(&info->rx_latency, 0, sizeof(info->rx_latency));
	vdev->dv_info = &dv_dummy;

	info->screen_real_width = 0;
	info->screen_real_height = 0;
}

static int calc_vinfo_from_hdmi_timing(const struct hdmi_timing *timing, struct vinfo_s *tx_vinfo)
{
	/* manually assign hdmitx_vinfo from timing */
	tx_vinfo->name = timing->sname ? timing->sname : timing->name;
	tx_vinfo->mode = VMODE_HDMI;
	tx_vinfo->frac = 0; /* TODO */
	if (timing->pixel_repetition_factor)
		tx_vinfo->width = timing->h_active >> 1;
	else
		tx_vinfo->width = timing->h_active;
	tx_vinfo->height = timing->v_active;
	tx_vinfo->field_height = timing->pi_mode ?
		timing->v_active : timing->v_active / 2;
	tx_vinfo->aspect_ratio_num = timing->h_pict;
	tx_vinfo->aspect_ratio_den = timing->v_pict;
	if (timing->v_freq % 1000 == 0) {
		tx_vinfo->sync_duration_num = timing->v_freq / 1000;
		tx_vinfo->sync_duration_den = 1;
	} else {
		tx_vinfo->sync_duration_num = timing->v_freq;
		tx_vinfo->sync_duration_den = 1000;
	}
	tx_vinfo->video_clk = timing->pixel_freq;
	tx_vinfo->htotal = timing->h_total;
	tx_vinfo->vtotal = timing->v_total;
	tx_vinfo->fr_adj_type = VOUT_FR_ADJ_HDMI;
	tx_vinfo->viu_color_fmt = COLOR_FMT_YUV444;
	tx_vinfo->viu_mux = timing->pi_mode ? VIU_MUX_ENCP : VIU_MUX_ENCI;
	/* 1080i use the ENCP, not ENCI */
	if (timing->name && strstr(timing->name, "1080i"))
		tx_vinfo->viu_mux = VIU_MUX_ENCP;
	tx_vinfo->viu_mux |= global_tx_common->enc_idx << 4;

	return 0;
}

void update_vinfo_from_formatpara(struct hdmitx_common *tx_comm)
{
	struct vinfo_s *vinfo = &tx_comm->hdmitx_vinfo;
	struct hdmi_format_para *fmtpara = &tx_comm->fmt_para;

	/*update vinfo for out device.*/
	calc_vinfo_from_hdmi_timing(&fmtpara->timing, vinfo);
	vinfo->info_3d = NON_3D;

	if (tx_comm->flag_3dfp) {
		vinfo->height =
			vinfo->height * 2 + fmtpara->timing.v_blank;
		vinfo->field_height =
			vinfo->field_height * 2 + fmtpara->timing.v_blank;
		vinfo->vtotal *= 2;
		vinfo->info_3d = FP_3D;
	} else if (tx_comm->flag_3dtb)
		vinfo->info_3d = TB_3D;
	else if (tx_comm->flag_3dss)
		vinfo->info_3d = SS_3D;

	/*dynamic info, always need set.*/
	vinfo->cs = fmtpara->cs;
	vinfo->cd = fmtpara->cd;
}

static void reset_vinfo(struct vinfo_s *tx_vinfo)
{
	tx_vinfo->name = "invalid";
	tx_vinfo->mode = VMODE_MAX;
}

static int hdmitx_common_pre_enable_mode(struct hdmitx_common *tx_comm,
					 struct hdmi_format_para *para)
{
	if (tx_comm->ready)
		HDMITX_ERROR("Should run disable_mode before enable new mode.\n");

	if (tx_comm->suspend_flag) {
		HDMITX_ERROR("current hpd_state/suspend (%d,%d), exit %s\n",
			__func__, tx_comm->hpd_state, tx_comm->suspend_flag);
		return -1;
	}

	/*TODO: keep for hw module to read formatpara, remove later.*/
	memcpy(&tx_comm->fmt_para, para, sizeof(struct hdmi_format_para));
	/* update fmt_attr: userspace still need this.*/
	hdmitx_format_para_rebuild_fmtattr_str(&tx_comm->fmt_para, tx_comm->fmt_attr,
					       sizeof(tx_comm->fmt_attr));

	if (tx_comm->ctrl_ops->pre_enable_mode)
		tx_comm->ctrl_ops->pre_enable_mode(tx_comm, para);

	return 0;
}

static int hdmitx_common_enable_mode(struct hdmitx_common *tx_comm,
				     struct hdmi_format_para *para)
{
	tx_comm->ctrl_ops->enable_mode(tx_comm, para);
	return 0;
}

static int hdmitx_common_post_enable_mode(struct hdmitx_common *tx_comm,
					  struct hdmi_format_para *para)
{
	if (tx_comm->ctrl_ops->post_enable_mode)
		tx_comm->ctrl_ops->post_enable_mode(tx_comm, para);

	if (tx_comm->cedst_policy) {
		cancel_delayed_work(&tx_comm->work_cedst);
		queue_delayed_work(tx_comm->cedst_wq, &tx_comm->work_cedst, 0);
	}

	edidinfo_attach_to_vinfo(tx_comm);
	update_vinfo_from_formatpara(tx_comm);

	tx_comm->ready = 1;
	return 0;
}

int hdmitx_common_do_mode_setting(struct hdmitx_common *tx_comm,
				  struct hdmitx_common_state *new_state,
				  struct hdmitx_common_state *old_state)
{
	int ret = 0;
	struct hdmi_format_para *new_para;

	new_para = &new_state->para;

	if (0) {
		if (new_state->mode & VMODE_INIT_BIT_MASK) {
			tx_comm->ctrl_ops->init_uboot_mode(new_state->mode);
			return 0;
		}
	}

	mutex_lock(&tx_comm->hdmimode_mutex);
	ret = hdmitx_common_pre_enable_mode(tx_comm, new_para);
	if (ret < 0) {
		HDMITX_ERROR("pre mode enable fail\n");
		goto fail;
	}

	if (new_state->mode & VMODE_INIT_BIT_MASK) {
		HDMITX_INFO("skip real mode setting for uboot init\n");
	} else {
		ret = hdmitx_common_enable_mode(tx_comm, new_para);
		if (ret < 0) {
			HDMITX_ERROR("mode enable fail\n");
			goto fail;
		}
	}

	ret = hdmitx_common_post_enable_mode(tx_comm, new_para);
	if (ret < 0) {
		HDMITX_ERROR("post mode enable fail\n");
		goto fail;
	}

fail:
	mutex_unlock(&tx_comm->hdmimode_mutex);
	return ret;
}
EXPORT_SYMBOL(hdmitx_common_do_mode_setting);

void hdmitx_common_output_disable(struct hdmitx_common *tx_comm,
	bool phy_dis, bool hdcp_reset, bool pkt_clear, bool edid_clear)
{
	struct hdmitx_hw_common *tx_hw_base = tx_comm->tx_hw;

	/* step1: HW: disable hdmitx phy, SW: clear status */
	if (phy_dis) {
		tx_comm->ready = 0;
		hdmitx_hw_cntl_misc(tx_hw_base, MISC_TMDS_PHY_OP, TMDS_PHY_DISABLE);
	}
	/* disable frl/dsc/vrr */
	if (tx_comm->ctrl_ops->disable_21_work)
		tx_comm->ctrl_ops->disable_21_work();

	/* step2: clear edid vinfo */
	if (edid_clear)
		hdmitx_common_edid_clear(tx_comm);

	/* step3: HW: clear packets */
	if (pkt_clear)
		tx_comm->ctrl_ops->clear_pkt(tx_hw_base);

	/* step4: reset hdcp */
	if (hdcp_reset)
		tx_comm->ctrl_ops->reset_hdcp(tx_comm);

	/* step5: SW: cancel ced work */
	if (tx_comm->cedst_en)
		cancel_delayed_work(&tx_comm->work_cedst);
}

int hdmitx_common_disable_mode(struct hdmitx_common *tx_comm,
			       struct hdmitx_common_state *new_state)
{
	struct hdmi_format_para *para;

	HDMITX_DEBUG("%s to disable ready state\n", __func__);
	/* TODO: clear pkt */
	hdmitx_common_output_disable(tx_comm,
		true, true, false, false);

	hdmitx_format_para_reset(&tx_comm->fmt_para);
	reset_vinfo(&tx_comm->hdmitx_vinfo);

	if (new_state)
		para = &new_state->para;
	else
		para = NULL;

	if (tx_comm->ctrl_ops->disable_mode)
		tx_comm->ctrl_ops->disable_mode(tx_comm, para);

	return 0;
}
EXPORT_SYMBOL(hdmitx_common_disable_mode);

#ifdef CONFIG_AMLOGIC_VOUT_SERVE
struct vinfo_s *hdmitx_get_current_vinfo(void *data)
{
	return &global_tx_common->hdmitx_vinfo;
}

static int hdmitx_set_current_vmode(enum vmode_e mode, void *data)
{
	if (!(mode & VMODE_INIT_BIT_MASK))
		HDMITX_INFO("warning, echo /sys/class/display/mode is disabled\n");
	else
		HDMITX_INFO("already display in uboot\n");

	return 0;
}

static enum vmode_e hdmitx_validate_vmode(char *mode, unsigned int frac, void *data)
{
	struct vinfo_s *vinfo = &global_tx_common->hdmitx_vinfo;
	const struct hdmi_timing *timing = 0;

	/* vout validate vmode only used to confirm the mode is
	 * supported by this server. And dont check with edid,
	 * maybe we dont have edid when this function called.
	 */
	timing = hdmitx_mode_match_timing_name(mode);
	if (hdmitx_common_validate_vic(global_tx_common, timing->vic) == 0) {
		/*should save mode name to vinfo, will be used in set_vmode*/
		calc_vinfo_from_hdmi_timing(timing, vinfo);
		vinfo->vout_device = global_tx_common->vdev;
		return VMODE_HDMI;
	}

	HDMITX_ERROR("%s validate %s fail\n", __func__, mode);
	return VMODE_MAX;
}

static int hdmitx_vmode_is_supported(enum vmode_e mode, void *data)
{
	if ((mode & VMODE_MODE_BIT_MASK) == VMODE_HDMI)
		return true;
	else
		return false;
}

static int hdmitx_module_disable(enum vmode_e cur_vmod, void *data)
{
	hdmitx_common_disable_mode(global_tx_common, NULL);
	return 0;
}

static int hdmitx_vout_state;
static int hdmitx_vout_set_state(int index, void *data)
{
	hdmitx_vout_state |= (1 << index);
	return 0;
}

static int hdmitx_vout_clr_state(int index, void *data)
{
	hdmitx_vout_state &= ~(1 << index);
	return 0;
}

static int hdmitx_vout_get_state(void *data)
{
	return hdmitx_vout_state;
}

/* if cs/cd/frac_rate is changed, then return 0 */
static int hdmitx_check_same_vmodeattr(char *name, void *data)
{
	HDMITX_ERROR("not support anymore\n");
	return 0;
}

static int hdmitx_vout_get_disp_cap(char *buf, void *data)
{
	int pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE, "check disp_cap sysfs node in hdmitx.\n");
	return pos;
}

static void hdmitx_set_bist(u32 num, void *data)
{
	HDMITX_ERROR("Not Support: try debug sysfs node in amhdmitx\n");
}

static int hdmitx_vout_set_vframe_rate_hint(int duration, void *data)
{
	HDMITX_ERROR("not support %s\n", __func__);
	return 0;
}

static int hdmitx_vout_get_vframe_rate_hint(void *data)
{
	HDMITX_ERROR("not support %S\n", __func__);
	return 0;
}

static struct vout_server_s hdmitx_vout_server = {
	.name = "hdmitx_vout_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.check_same_vmodeattr = hdmitx_check_same_vmodeattr,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.get_disp_cap = hdmitx_vout_get_disp_cap,
		.set_vframe_rate_hint = hdmitx_vout_set_vframe_rate_hint,
		.get_vframe_rate_hint = hdmitx_vout_get_vframe_rate_hint,
		.set_bist = hdmitx_set_bist,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
	.data = NULL,
};
#else
static struct vinfo_s *hdmitx_get_current_vinfo(void *data)
{
	return NULL;
}
#endif

#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
static struct vout_server_s hdmitx_vout2_server = {
	.name = "hdmitx_vout2_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.check_same_vmodeattr = hdmitx_check_same_vmodeattr,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.get_disp_cap = hdmitx_vout_get_disp_cap,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = hdmitx_set_bist,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
	.data = NULL,
};
#endif

#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
static struct vout_server_s hdmitx_vout3_server = {
	.name = "hdmitx_vout3_server",
	.op = {
		.get_vinfo = hdmitx_get_current_vinfo,
		.set_vmode = hdmitx_set_current_vmode,
		.validate_vmode = hdmitx_validate_vmode,
		.check_same_vmodeattr = hdmitx_check_same_vmodeattr,
		.vmode_is_supported = hdmitx_vmode_is_supported,
		.disable = hdmitx_module_disable,
		.set_state = hdmitx_vout_set_state,
		.clr_state = hdmitx_vout_clr_state,
		.get_state = hdmitx_vout_get_state,
		.get_disp_cap = hdmitx_vout_get_disp_cap,
		.set_vframe_rate_hint = NULL,
		.get_vframe_rate_hint = NULL,
		.set_bist = hdmitx_set_bist,
#ifdef CONFIG_PM
		.vout_suspend = NULL,
		.vout_resume = NULL,
#endif
	},
	.data = NULL,
};
#endif

void hdmitx_vout_init(struct hdmitx_common *tx_comm, struct hdmitx_hw_common *tx_hw)
{
	global_tx_common = tx_comm;
	global_tx_hw = tx_hw;
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_register_server(&hdmitx_vout_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_register_server(&hdmitx_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vout3_register_server(&hdmitx_vout3_server);
#endif
}

void hdmitx_vout_uninit(void)
{
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
	vout_unregister_server(&hdmitx_vout_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	vout2_unregister_server(&hdmitx_vout2_server);
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	vout3_unregister_server(&hdmitx_vout3_server);
#endif
}

/* common work for plugin/resume, witch is done in lock */
void hdmitx_plugin_common_work(struct hdmitx_common *tx_comm)
{
	/* trace event */
	hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_HPD_PLUGIN);

	/* SW: start rxsense check */
	if (tx_comm->rxsense_policy) {
		cancel_delayed_work(&tx_comm->work_rxsense);
		queue_delayed_work(tx_comm->rxsense_wq, &tx_comm->work_rxsense, 0);
	}

	/* SW/HW: read/parse EDID */
	/* there may be such case:
	 * hpd rising & hpd level high (0.6S > HZ/2)-->
	 * plugin handler-->hpd falling & hpd level low(0.05S)-->
	 * continue plugin handler, EDID read normal,
	 * post plugin uevent-->
	 * plugout handler(may be filtered and skipped):
	 * stop hdcp/clear edid, post plugout uevent-->
	 * system plugin handle: set hdmi mode/hdcp auth-->
	 * system plugout handle: set non-hdmi mode(but hdcp is still running)-->
	 * hpd rising & keep level high-->plugin handler, EDID read abnormal
	 * as hdcp auth is running and may access DDC when reading EDID.
	 * so need to disable hdcp auth before EDID reading
	 */
	if (tx_comm->hdcp_mode != 0) {
		HDMITX_INFO("hdcp: %d should not be enabled before signal ready\n",
			tx_comm->hdcp_mode);
		tx_comm->ctrl_ops->reset_hdcp(tx_comm);
	}

	/*read edid*/
	hdmitx_common_get_edid(tx_comm);

	/* SW: update flags */
	if (tx_comm->cedst_policy == 1)
		tx_comm->cedst_en = !!tx_comm->rxcap.scdc_present;

	tx_comm->hpd_state = 1;
	tx_comm->already_used = 1;

	/* SW: special for hdcp repeater */
	if (tx_comm->tx_hw->hdcp_repeater_en)
		rx_set_repeater_support(1);

	tx_comm->last_hpd_handle_done_stat = HDMI_TX_HPD_PLUGIN;
}

void hdmitx_common_late_resume(struct hdmitx_common *tx_comm)
{
	struct hdmitx_hw_common *tx_hw_base = tx_comm->tx_hw;

	tx_hw_base->debug_hpd_lock = 0;

	/* TODO: special for RDK */
	/* for RDK userspace, after receive plug change uevent,
	 * it will check connector state before enable encoder.
	 * so should not change hpd_state other than in plug handler
	 */
	/* if (tx_comm->hdcp_ctl_lvl != 0x1) */
		/* tx_comm->hpd_state = hpd_state; */

	/* step1: SW: force HPD/EDID update, as there may be
	 * no hpd event during suspend/resume stage but edid
	 * have been cleared when suspend
	 */
	tx_comm->hpd_state = !!(hdmitx_hw_cntl_misc(tx_hw_base, MISC_HPD_GPI_ST, 0));
	if (tx_comm->hpd_state)
		tx_comm->already_used = 1;

	HDMITX_INFO("hdmitx hpd state: %d\n", tx_comm->hpd_state);

	if (tx_comm->hpd_state) {
		/* if there's hpd plugin event during early suspend,
		 * then hdmitx_plugin_common_work() have been done,
		 * no need to call again except edid abnormal
		 */
		if (tx_comm->rxcap.edid_parsing == 0)
			hdmitx_plugin_common_work(tx_comm);
	} else {
		/* Note: if plugout event and resume come together
		 * here clear edid info, as later will post
		 * plugout uevent and system may check edid/cap
		 * and find it not in clear state.
		 */
		hdmitx_common_edid_clear(tx_comm);
		/* other work will be done in plugout handler */
	}

	/* step2: SW: status update */
	tx_comm->suspend_flag = false;
	hdmitx_hw_cntl_misc(tx_hw_base, MISC_SUSFLAG, 0);

	/* step3: HW: reset HW */
	hdmitx_hw_cntl(tx_hw_base, HDMITX_EARLY_SUSPEND_RESUME_CNTL,
		HDMITX_LATE_RESUME);

	/* step4: SW: post uevent to system */
	hdmitx_common_notify_hpd_status(tx_comm, true);
	hdmitx_event_mgr_send_uevent(tx_comm->event_mgr,
		HDMITX_HDCPPWR_EVENT, HDMI_WAKEUP, false);
}

