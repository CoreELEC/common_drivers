// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/printk.h>
#include <linux/delay.h>

#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_common.h>
#include <linux/amlogic/media/vout/hdmitx_common/hdmitx_edid.h>
#include <linux/amlogic/media/vout/hdmi_tx_repeater.h>

#include "hdmitx_log.h"
#include "hdmitx_check_valid.h"
#include "hdmitx_vout.h"
#include "hdmitx_module.h"
#include "hdmitx_hdr.h"
#include "hdmitx/tx21/hdmitx_packet.h"
#include "hdmitx_compliance.h"

const struct hdmi_timing *hdmitx_mode_match_timing_name(const char *name);

static int hdmitx_common_pre_enable_mode(struct hdmitx_common *tx_comm,
					 struct hdmi_format_para *para)
{
	mutex_lock(&tx_comm->valid_mutex);
	if (tx_comm->ready)
		HDMITX_ERROR("Should run disable_mode before enable new mode.\n");

	if (tx_comm->hpd_state == 0 || tx_comm->suspend_flag) {
		HDMITX_WARNING("%s current hpd_state/suspend (%d,%d), skip phy setting\n",
			__func__, tx_comm->hpd_state, tx_comm->suspend_flag);
		hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_KMS_SKIP);
		tx_comm->skip_phy_setting = true;
	}

	/*TODO: keep for hw module to read format_para, remove later.*/
	memcpy(&tx_comm->fmt_para, para, sizeof(struct hdmi_format_para));

	/*check if vic supported by rx*/
	if (!hdmitx_edid_validate_mode(&tx_comm->rxcap, tx_comm->fmt_para.vic)) {
		HDMITX_ERROR("edid invalid vic-%d return error\n", tx_comm->fmt_para.vic);
		mutex_unlock(&tx_comm->valid_mutex);
		return -EINVAL;
	}

	if (hdmitx_common_validate_vic(tx_comm, tx_comm->fmt_para.vic)) {
		HDMITX_ERROR("validate vic-%d return error\n", tx_comm->fmt_para.vic);
		mutex_unlock(&tx_comm->valid_mutex);
		return -EINVAL;
	}

	if (hdmitx_common_validate_format_para(tx_comm, &tx_comm->fmt_para)) {
		HDMITX_ERROR("format para check fail.\n");
		mutex_unlock(&tx_comm->valid_mutex);
		return -EINVAL;
	}

	hdmitx_hw_cntl(tx_comm->tx_hw, MODE_FLOW_PRE_ENABLE_MODE, NULL, NULL);

	mutex_unlock(&tx_comm->valid_mutex);
	return 0;
}

static int hdmitx_common_enable_mode(struct hdmitx_common *tx_comm,
				     struct hdmi_format_para *para)
{
	return hdmitx_hw_cntl(tx_comm->tx_hw, MODE_FLOW_ENABLE_MODE, NULL, NULL);
}

static int hdmitx_common_post_enable_mode(struct hdmitx_common *tx_comm,
					  struct hdmi_format_para *para)
{
	hdmitx_hw_cntl(tx_comm->tx_hw, MODE_FLOW_POST_ENABLE_MODE, NULL, NULL);

	/*
	 * attach vinfo, if hdr_cap and dv_cap change, the HDR/DV module will
	 * call the packet sending function, need to set ready flag to 1 first
	 */
	tx_comm->ready = 1;
	hdmitx_update_vinfo(tx_comm);
	/* cedst_en defaults to 0 */
	if (para->tmds_clk && tx_comm->cedst_en) {
		tx_comm->ced_check_count = 0;
		cancel_delayed_work(&tx_comm->work_cedst);
		queue_delayed_work(tx_comm->cedst_wq, &tx_comm->work_cedst,
				msecs_to_jiffies(2000));
	} else if (tx_comm->cedst_policy == 0) {
		tx_comm->cedst_en = 0;
		cancel_delayed_work(&tx_comm->work_cedst);
	}

	/*
	 * need set audio mode again when set video mode, so send the event
	 * to audio hal to reset alsa and set audio mode
	 */
	hdmitx_event_mgr_send_uevent(tx_comm->event_mgr,
		HDMITX_VMODE_AUDIO_EVENT, true, true);

	return 0;
}

int hdmitx_common_do_mode_setting(struct hdmitx_common *tx_comm,
				  struct hdmitx_common_state *new_state,
				  struct hdmitx_common_state *old_state)
{
	int ret = 0;
	struct hdmi_format_para *new_para;

	new_para = &new_state->para;

	if (new_state->mode & VMODE_INIT_BIT_MASK) {
		HDMITX_INFO("skip real mode setting for uboot init and update vinfo\n");
		hdmitx_update_vinfo_duration(tx_comm);
		/* note that for bootup, hdmitx_common_post_enable_mode()
		 * action will be done in hdmitx_set_current_vmode()
		 * when vout probe, it's earlier than drm to
		 * call hdmitx_common_do_mode_setting(), and
		 * thus VPP/DV won't miss dv/hdr cap in vinfo
		 */
		return ret;
	}

	mutex_lock(&tx_comm->hdmimode_mutex);
	if (new_state->state_sequence_id != tx_comm->tx_hw->hw_sequence_id) {
		HDMITX_ERROR("state_sequence_id failed %lld\n", new_state->state_sequence_id);
		tx_comm->skip_phy_setting = true;
	}
	ret = hdmitx_common_pre_enable_mode(tx_comm, new_para);
	if (ret < 0) {
		HDMITX_ERROR("pre mode enable fail\n");
		tx_comm->skip_phy_setting = true;
	}

	ret = hdmitx_common_enable_mode(tx_comm, new_para);
	if (ret < 0) {
		if (!tx_comm->skip_phy_setting) {
			HDMITX_ERROR("mode enable fail\n");
			hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_KMS_ERROR);
		}
		tx_comm->skip_phy_setting = false;
		goto fail;
	}

	ret = hdmitx_common_post_enable_mode(tx_comm, new_para);
	if (ret < 0) {
		HDMITX_ERROR("post mode enable fail\n");
		hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_KMS_ERROR);
		goto fail;
	}

	hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_KMS_ENABLE_OUTPUT);

fail:
	mutex_unlock(&tx_comm->hdmimode_mutex);
	return ret;
}
EXPORT_SYMBOL(hdmitx_common_do_mode_setting);

/* sync with hdmitx_common_do_mode_setting() */
static int hdmitx_common_do_mode_setting_test(struct hdmitx_common *tx_comm,
				  enum hdmi_vic vic, char *attr_str)
{
	int ret = 0;
	struct hdmi_format_para new_para;

	if (!tx_comm || !attr_str)
		return -1;

	mutex_lock(&tx_comm->hdmimode_mutex);
	memset(&new_para, 0, sizeof(new_para));

	hdmitx_parse_color_attr(attr_str, &new_para.cs, &new_para.cd, &new_para.cr);
	ret = hdmitx_common_build_format_para(tx_comm, &new_para,
					      vic, 0,
					      new_para.cs, new_para.cd,
					      HDMI_QUANTIZATION_RANGE_FULL);
	if (ret < 0) {
		HDMITX_ERROR("%s format para build fail\n", __func__);
		goto fail;
	}
	ret = hdmitx_common_pre_enable_mode(tx_comm, &new_para);
	if (ret < 0) {
		HDMITX_ERROR("pre mode enable fail\n");
		goto fail;
	}
	ret = hdmitx_common_enable_mode(tx_comm, &new_para);
	if (ret < 0) {
		HDMITX_ERROR("mode enable fail\n");
		goto fail;
	}
	ret = hdmitx_common_post_enable_mode(tx_comm, &new_para);
	if (ret < 0) {
		HDMITX_ERROR("post mode enable fail\n");
		goto fail;
	}

fail:
	mutex_unlock(&tx_comm->hdmimode_mutex);
	return ret;
}

int set_disp_mode_debug(struct hdmitx_common *tx_comm, const char *mode, char *test_fmt_attr)
{
	int ret = 0;
	enum hdmi_vic vic;
	const struct hdmi_timing *timing = NULL;

	if (!tx_comm || !mode)
		return -1;

	if (!strncmp(mode, "off", strlen("off")) ||
		!strncmp(mode, "null", strlen("null")) ||
		!strncmp(mode, "invalid", strlen("invalid"))) {
		hdmitx_common_disable_mode(tx_comm, NULL);
		HDMITX_INFO("%s: disable hdmi mode\n", __func__);
		return 0;
	}
	/* function for debug, only get vic and check if ip can support, skip rx cap check. */
	timing = hdmitx_mode_match_timing_name(mode);
	if (!timing || timing->vic == HDMI_0_UNKNOWN) {
		HDMITX_ERROR("unknown mode %s\n", mode);
		return -EINVAL;
	}

	vic = timing->vic;
	/* force set mode for test purpose, not check HW support or not */
	/* if (hdmitx_common_validate_vic(tx_comm, timing->vic) != 0) { */
	/* HDMITX_ERROR("ip cannot support mode %s. %d\n", mode, timing->vic); */
	/* return -EINVAL; */
	/* } */
	ret = hdmitx_common_do_mode_setting_test(tx_comm,
					  vic, test_fmt_attr);
	return ret;
}

void hdmitx_common_output_disable(struct hdmitx_common *tx_comm,
	bool phy_dis, bool hdcp_reset, bool pkt_clear, bool edid_clear)
{
	struct hdmitx_hw_common *tx_hw_base = tx_comm->tx_hw;
	u32 arg;

	/* step1 detach vinfo
	 * detach vinfo is common operate for plugout and suspend and switch resolution.
	 * After setting the mode, you need to set ready to 1 first, and then attach
	 * vinfo to notify HDR/DV to send the package to ensure that pkt can be sent
	 * normally. In disable mode, first detach vinfo, notify HDR/DV to no longer
	 * send packets, and then set ready to 0
	 */
	hdmitx_reset_vinfo(&tx_comm->hdmitx_vinfo);

	/* step2: HW: disable hdmitx phy, SW: clear status */
	if (phy_dis) {
		tx_comm->ready = 0;
		arg = TMDS_PHY_DISABLE;
		hdmitx_hw_cntl(tx_hw_base, PLATFORM_PHY_OP, (void *)&arg, NULL);
		hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_KMS_DISABLE_OUTPUT);
	}

	/* disable frl/dsc/vrr */
	hdmitx_hw_cntl(tx_comm->tx_hw, MODE_FLOW_DISABLE_21_WORK, NULL, NULL);

	/* step3: clear edid */
	if (edid_clear)
		hdmitx_common_edid_clear(tx_comm);

	/* step4: HW: clear packets */
	if (pkt_clear)
		hdmitx_clear_all_infoframe_pkt(tx_comm);

	/* step5: reset hdcp */
	if (hdcp_reset)
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_DISABLE, NULL, NULL);

	/* step6: SW: cancel ced work in sync to make sure it won't queued again */
	if (tx_comm->cedst_en)
		cancel_delayed_work_sync(&tx_comm->work_cedst);
}

int hdmitx_common_disable_mode(struct hdmitx_common *tx_comm,
			       struct hdmitx_common_state *new_state)
{
	struct hdmi_format_para *para;
	u32 arg = 0;

	HDMITX_DEBUG("%s to disable ready state\n", __func__);
	mutex_lock(&tx_comm->hdmimode_mutex);
	hdmitx_common_output_disable(tx_comm,
		true, true, true, false);

	hdmitx_format_para_reset(&tx_comm->fmt_para);

	if (new_state)
		para = &new_state->para;
	else
		para = NULL;
	/* unregister when disable current mode */
	hdmitx_hw_cntl(tx_comm->tx_hw, VRR_REGISTER, (void *)&arg, NULL);
	memset(&tx_comm->ced_cnt, 0, sizeof(tx_comm->ced_cnt));
	memset(&tx_comm->ch_locked_st, 0, sizeof(tx_comm->ch_locked_st));

	if (tx_comm->debug_param.disable_delay_ms > 0) {
		msleep_interruptible(tx_comm->debug_param.disable_delay_ms);
		HDMITX_INFO("%s disable output and delay %d ms\n", __func__,
			tx_comm->debug_param.disable_delay_ms);
	} else if (hdmitx_find_vendor_disable_delay(tx_comm->edid_buf)) {
		msleep_interruptible(500);
		HDMITX_INFO("%s disable output and delay 500 ms\n", __func__);
	}
	mutex_unlock(&tx_comm->hdmimode_mutex);

	return 0;
}

void hdmitx_common_late_resume(struct hdmitx_common *tx_comm)
{
	struct hdmitx_hw_common *tx_hw_base = tx_comm->tx_hw;
	u32 arg = HDMITX_LATE_RESUME;

	tx_hw_base->debug_hpd_lock = 0;

	/* step1: SW: status update */
	tx_comm->suspend_flag = false;

	/* step2: HW: reset HW */
	hdmitx_hw_cntl(tx_hw_base, CORE_MISC_SUSPEND_RESUME_CNTL, (void *)&arg, NULL);

	/* step3: SW: post uevent to system */
	hdmitx_common_notify_hpd_status(tx_comm, true);
	hdmitx_event_mgr_send_uevent(tx_comm->event_mgr,
		HDMITX_HDCPPWR_EVENT, HDMI_WAKEUP, false);
}

/* plugin process: action should be done in lock
 * 1.whole edid is parsed in hdmi side for bootup
 * while only phy_addr/audio_cap is parsed in hdmi
 * side for non-bootup case
 * 2.audio mode is set in bootup case(to be moved)
 */
void hdmitx_process_plugin(struct hdmitx_common *tx_comm, bool boot_flag, bool set_audio)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return;
	}
	/* trace event */
	hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_HPD_PLUGIN);

	tx_comm->tx_hw->hw_sequence_id = get_jiffies_64();
	HDMITX_INFO("plugin sequence id: %lld\n", tx_comm->tx_hw->hw_sequence_id);

	/* step1: SW: start rxsense check */
	if (tx_comm->rxsense_policy) {
		cancel_delayed_work(&tx_comm->work_rxsense);
		queue_delayed_work(tx_comm->rxsense_wq, &tx_comm->work_rxsense, 0);
	}

	/* step2: SW: read/parse EDID */
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
	if (tx_comm->hdcptx_comm.hdcp_mode != 0) {
		HDMITX_INFO("hdcp: %d should not be enabled before signal ready\n",
			tx_comm->hdcptx_comm.hdcp_mode);
		hdmitx_hw_cntl(tx_comm->tx_hw, HDCP_DISABLE, NULL, NULL);
	}
	/* read EDID */
	hdmitx_common_get_edid(tx_comm);
	/* parse EDID */
	hdmitx_edid_process(tx_comm, boot_flag, false);

	/* step3: SW: status update */
	tx_comm->hpd_state = 1;
	tx_comm->already_used = 1;
	/* SW: special for hdcp repeater */
	if (tx_comm->hdcptx_comm.hdcp_rpt_en)
		rx_set_repeater_support(1);
	tx_comm->last_hpd_handle_done_stat = HDMI_TX_HPD_PLUGIN;

	/* step4: SW: notify client modules and update uevent state */
	hdmitx_common_notify_hpd_status(tx_comm, false);
}

/* plugin handle for hpd irq */
void hdmitx_hpd_plugin_irq_handler(struct work_struct *work)
{
	struct hdmitx_common *tx_comm = container_of((struct delayed_work *)work,
		struct hdmitx_common, work_hpd_plugin);

	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return;
	}
	mutex_lock(&tx_comm->hdmimode_mutex);
	/*
	 * this may happen when just queue plugin work,
	 * but plugout event happen at this time. no need
	 * to continue plugin work.
	 */
	if (hdmitx_hw_cntl(tx_comm->tx_hw, PLATFORM_GET_HPD_GPI_ST, NULL, NULL) == 0) {
		HDMITX_INFO("plug out event come when plugin handle, abort handle\n");
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	/*
	 * only happen in such case:
	 * hpd high when suspend->plugout->plugin->late resume, the
	 * last plugin/resume flow sequence is unknown, will do
	 * plugin handler only once
	 */
	if (tx_comm->last_hpd_handle_done_stat == HDMI_TX_HPD_PLUGIN) {
		HDMITX_INFO("warning: continuous plugin, should not happen!\n");
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	HDMITX_INFO(SYS "hpd_high\n");
	hdmitx_process_plugin(tx_comm, false, false);

	mutex_unlock(&tx_comm->hdmimode_mutex);

	/* notify to drm hdmi */
	hdmitx_fire_drm_hpd_cb_unlocked(tx_comm);
}

/* common process for plugout flow, which should be done in lock */
void hdmitx_process_plugout(struct hdmitx_common *tx_comm)
{
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return;
	}

	HDMITX_INFO(SYS "hpd_low\n");
	/* trace event */
	hdmitx_tracer_write_event(tx_comm->tx_tracer, HDMITX_HPD_PLUGOUT);

	tx_comm->tx_hw->hw_sequence_id = 0;
	HDMITX_INFO("sequence id: %lld\n", tx_comm->tx_hw->hw_sequence_id);

	/* step1: disable output */
	hdmitx_common_output_disable(tx_comm, true, true, true, tx_comm->forced_edid ? 0 : 1);
	/* private process for plugout */
	hdmitx_hw_cntl(tx_comm->tx_hw, MODE_FLOW_PRIVATE_PLUGOUT_PROCESS, NULL, NULL);

	/* step2: SW: status update */
	tx_comm->hpd_state = 0;
	tx_comm->last_hpd_handle_done_stat = HDMI_TX_HPD_PLUGOUT;

	/* step3: SW: notify event to user space and other modules */
	hdmitx_common_notify_hpd_status(tx_comm, false);
}

/* plugout handle for hpd irq */
void hdmitx_hpd_plugout_irq_handler(struct work_struct *work)
{
	struct hdmitx_common *tx_comm = container_of((struct delayed_work *)work,
		struct hdmitx_common, work_hpd_plugout);
	if (!tx_comm) {
		HDMITX_ERROR("%s NULL tx_comm pointer\n", __func__);
		return;
	}

	mutex_lock(&tx_comm->hdmimode_mutex);
	if (tx_comm->last_hpd_handle_done_stat == HDMI_TX_HPD_PLUGOUT) {
		HDMITX_INFO("continuous plugout handler, send HDMITX_LINK_UNSTABLE uevent\n");
		hdmitx_set_uevent(tx_comm, HDMITX_LINK_UNSTABLE, 1);
		mutex_unlock(&tx_comm->hdmimode_mutex);
		return;
	}
	hdmitx_process_plugout(tx_comm);
	mutex_unlock(&tx_comm->hdmimode_mutex);

	/* notify to drm hdmi */
	hdmitx_fire_drm_hpd_cb_unlocked(tx_comm);
}

static bool hdmitx_check_bootup_output_ready(struct hdmitx_common *tx_comm)
{
	bool ready = false;

	if (!tx_comm || !tx_comm->tx_hw)
		return false;

	if (hdmitx_hw_cntl(tx_comm->tx_hw, CORE_MISC_GET_OUTPUT_ST, NULL, NULL))
		ready = true;
	if (ready) {
		if (tx_comm->tx_hw->chip_data->chip_type == MESON_CPU_ID_S5) {
			if (!hdmitx_hw_cntl(tx_comm->tx_hw, FRL_GET_RX_READY_ST, NULL, NULL))
				ready = false;
		}
	}

	return ready;
}

/* similar as hdmitx_common_post_enable_mode() for bootup
 * except for resend div40 if output ready. it should be done in lock
 */
void hdmitx_bootup_post_process(struct hdmitx_common *tx_comm)
{
	int frl_rate = FRL_NONE;
	u32 arg = 1;

	tx_comm->ready = hdmitx_check_bootup_output_ready(tx_comm);

	if (tx_comm->ready) {
		if (tx_comm->tx_hw->chip_data->chip_type == MESON_CPU_ID_S5)
			frl_rate = hdmitx_hw_cntl(tx_comm->tx_hw, FRL_GET_MODE, NULL, NULL);
		/* if current mode is TMDS/nonFRL, then resend_div40 */
		if (frl_rate == FRL_NONE) {
			if (tx_comm->fmt_para.tmds_clk_div40)
				hdmitx_hw_cntl(tx_comm->tx_hw, DDC_SCDC_DIV40_SCRAMB,
					(void *)&arg, NULL);
		}
		/* bootup need parse frac mode and update vinfo */
		if (tx_comm->fmt_para.frac_mode)
			hdmitx_mode_update_timing(&tx_comm->fmt_para.timing,
						  tx_comm->fmt_para.frac_mode);

		/* step1: write all zeros to the CUVA EMP hardware buffer */
		hdmitx_cuva_dhdr_init(tx_comm);
		/*
		 * During the kernel startup process, the HDR/DV module will use
		 * vinfo information, it needs to attach vinfo after the EDID is
		 * parsed and before the HDR/DV module is enabled.
		 * so do as hdmitx_common_post_enable_mode()
		 */
		hdmitx_update_vinfo(tx_comm);
		if (tx_comm->cedst_en)
			queue_delayed_work(tx_comm->cedst_wq, &tx_comm->work_cedst, 0);
	}
}

int hdmitx_set_display(struct hdmitx_common *tx_comm, enum hdmi_vic video_code)
{
	struct hdmitx_hw_common *hw_comm = tx_comm->tx_hw;
	struct hdmi_format_para *para = &tx_comm->fmt_para;
	int ret = -1;
	unsigned char buffer[31] = {0x87, 0x1, 26};
	u32 arg;

	HDMITX_DEBUG_VIDEO("%s set VIC = %d\n", __func__, video_code);

	if (video_code >= HDMITX_VESA_OFFSET) {
		para->cs = HDMI_COLORSPACE_RGB;
		para->cd = COLORDEPTH_24B;
		HDMITX_INFO("VESA only support RGB format\n");
	}

	if (hw_comm->set_disp_mode(hw_comm, para) >= 0) {
		hdmitx_hw_cntl(hw_comm, AUX_PKT_AVI_CONSTRUCT, NULL, NULL);
		/* HDMI CT 7-33 DVI Sink, no HDMI VSDB nor any
		 * other VSDB, No GB or DI expected
		 * TMDS_MODE[hdmi_config]
		 * 0: DVI Mode	   1: HDMI Mode
		 */
		/*
		 * HDMI Identifier = HDMI_IEEE_OUI 0x000c03
		 * If not, treated as a DVI Device
		 */
		if (tx_comm->rxcap.ieeeoui != HDMI_IEEE_OUI) {
			HDMITX_INFO("Sink is DVI device\n");
			arg = DVI_MODE;
			hdmitx_hw_cntl(hw_comm,
				CORE_MISC_SET_HDMI_DVI_MODE, (void *)&arg, NULL);
		} else {
			HDMITX_INFO("Sink is HDMI device\n");
			arg = HDMI_MODE;
			hdmitx_hw_cntl(hw_comm,
				CORE_MISC_SET_HDMI_DVI_MODE, (void *)&arg, NULL);
		}

		if (video_code == HDMI_95_3840x2160p30_16x9 ||
			video_code == HDMI_94_3840x2160p25_16x9 ||
			video_code == HDMI_93_3840x2160p24_16x9 ||
			video_code == HDMI_98_4096x2160p24_256x135) {
			hdmitx_common_setup_vsif_packet(tx_comm, VT_HDMI14_4K, 1, NULL);
		} else if ((!para->flag_3dfp) && (!para->flag_3dtb) &&
				(!para->flag_3dss)) {
			/* For non-4kx2k mode setting */
			hdmitx_common_setup_vsif_packet(tx_comm, VT_HDMI14_4K, 0, NULL);
		}

		/* if TV support traditional SDR, then enable hdr.sdr packet by default */
		if (tx_comm->rxcap.hdr_info.hdr_support & 0x1) {
			/*
			 * only for hdr.sdr packet send after mode setting,
			 * for sync purpose, should not use work queue,
			 * no uevent/trace for such case
			 */
			HDMITX_INFO("hdr: %s: hdr.sdr pkt sent\n", __func__);
			tx_comm->colormetry = 0;
			arg = CLR_AVI_BT2020;
			hdmitx_hw_cntl(hw_comm, AUX_PKT_CONF_AVI_BT2020, (void *)&arg, NULL);
			hdmitx_hw_cntl(hw_comm, AUX_PKT_SET_DRM, buffer, NULL);
		}
		if (tx_comm->allm_mode) {
			hdmitx_common_setup_vsif_packet(tx_comm, VT_ALLM, 1, NULL);
			arg = SET_CT_OFF;
			hdmitx_hw_cntl(hw_comm, AUX_PKT_CONF_AVI_CT, (void *)&arg, NULL);
		} else {
			arg = tx_comm->ct_mode;
			hdmitx_hw_cntl(hw_comm, AUX_PKT_CONF_AVI_CT, (void *)&arg, NULL);
		}
		hdmitx_hw_cntl(hw_comm, AUX_PKT_SET_SPD_INFO, NULL, NULL);
		ret = 0;
	} else {
		if (tx_comm->skip_phy_setting)
			HDMITX_INFO("%s skip phy setting\n", __func__);
		else
			HDMITX_ERROR("%s set video mode fail\n", __func__);
	}

	return ret;
}
