// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux Headers */
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/extcon.h>
#include <linux/workqueue.h>

/* Amlogic Headers */
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vrr/vrr.h>

/* Local Headers */
#include "../tvin_frontend.h"
#include "../tvin_format_table.h"

#include "vdin_sm.h"
#include "vdin_ctl.h"
#include "vdin_drv.h"
#include "../tvafe/tvafe_regs.h"

#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AVDETECT
#include "tvafe_avin_detect.h"
#endif

static struct tvin_sm_s sm_dev[VDIN_MAX_DEVS];

int sm_print_nosig;
static int sm_print_notsup;
static int sm_print_fmt_nosig;
static int sm_print_fmt_chg;
static int sm_atv_prestable_fmt;
static int sm_print_prestable;

/* new add in gxtvbb@20160613,reason:
 * ensure vdin fmt can update when fmt is changed in menu
 */
static int atv_stable_fmt_check_enable;

static int hdmi_prestable_out_cnt = 20;
static int manual_unstable_out_cnt = 20;
bool manual_flag;
bool atv_std_flag;

//#ifdef DEBUG_SUPPORT
static int signal_status = TVIN_SIG_STATUS_NULL;
module_param(signal_status, int, 0664);
MODULE_PARM_DESC(signal_status, "signal_status");

enum tvin_color_fmt_range_e
	tvin_get_force_fmt_range(enum tvin_color_fmt_e color_fmt)
{
	u32 fmt_range = TVIN_YUV_FULL;

	if (color_fmt == TVIN_YUV444 ||
	    color_fmt == TVIN_YUV422 ||
	    color_fmt == TVIN_YUV420) {
		if (color_range_force == COLOR_RANGE_FULL)
			fmt_range = TVIN_YUV_FULL;
		else if (color_range_force == COLOR_RANGE_LIMIT)
			fmt_range = TVIN_YUV_LIMIT;
		else
			fmt_range = TVIN_YUV_FULL;
	} else if (color_fmt == TVIN_RGB444) {
		if (color_range_force == COLOR_RANGE_FULL)
			fmt_range = TVIN_RGB_FULL;
		else if (color_range_force == COLOR_RANGE_LIMIT)
			fmt_range = TVIN_RGB_LIMIT;
		else
			fmt_range = TVIN_RGB_FULL;
	}
	return fmt_range;
}

void vdin_update_prop(struct vdin_dev_s *devp)
{
	/*devp->pre_prop.fps = devp->prop.fps;*/
	devp->vrr_data.cur_vrr_status = get_cur_vrr_status(devp);
	devp->vrr_data.pre_vrr_status = devp->vrr_data.cur_vrr_status;
	devp->dv.dv_flag = devp->prop.dolby_vision;
	devp->dv.low_latency = devp->prop.low_latency;
	/*devp->pre_prop.latency.allm_mode = devp->prop.latency.allm_mode;*/
	/*devp->pre_prop.aspect_ratio = devp->prop.aspect_ratio;*/
	devp->parm.info.aspect_ratio = devp->prop.aspect_ratio;
	devp->misc_info.afd_info = (devp->prop.pic_aspect_ratio << 4) |
				devp->prop.active_ratio;
	vdin_get_base_fr(devp);
	memcpy(&devp->pre_prop, &devp->prop,
	       sizeof(struct tvin_sig_property_s));
}

static inline void vdin_update_parm(struct vdin_dev_s *devp)
{
	/* 3D interlaced signals are not supported default to 2D */
	if (devp->fmt_info_p &&
	    devp->fmt_info_p->scan_mode == TVIN_SCAN_MODE_INTERLACED)
		devp->parm.info.trans_fmt = TVIN_TFMT_2D;
	else
		devp->parm.info.trans_fmt = devp->prop.trans_fmt;

	devp->parm.info.is_dvi = devp->prop.dvi_info;
	devp->parm.info.cfmt =
		devp->prop.color_format;
}

/*
 * check hdmirx color format
 */
static enum tvin_sg_chg_flg vdin_hdmirx_fmt_chg_detect(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	enum tvin_port_e port = TVIN_PORT_NULL;
	enum tvin_color_fmt_e cur_color_fmt, pre_color_fmt;
	/*enum tvin_color_fmt_e cur_dest_color_fmt, pre_dest_color_fmt;*/
	struct tvin_sig_property_s *prop, *pre_prop;
	unsigned int vdin_hdr_flag, pre_vdin_hdr_flag;
	unsigned int vdin_hdr10p_flag, pre_vdin_hdr10p_flag;
	unsigned int vdin_fmt_range, pre_vdin_fmt_range;
	unsigned int cur_dv_flag, pre_dv_flag;
	unsigned int temp;
	enum tvin_sg_chg_flg signal_chg = TVIN_SIG_CHG_NONE;

	if (!devp)
		return signal_chg;

	if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return signal_chg;
	}

	prop = &devp->prop;
	pre_prop = &devp->pre_prop;
	sm_ops = devp->frontend->sm_ops;
	port = devp->parm.port;

	if (!IS_HDMI_SRC(port) &&
		!IS_TVAFE_SRC(port))
		return signal_chg;

	if (sm_ops->get_sig_property) {
		/*if (!(devp->flags & VDIN_FLAG_ISR_EN))*/
		/*	sm_ops->get_sig_property(devp->frontend, prop);*/

		cur_color_fmt = prop->color_format;
		pre_color_fmt = pre_prop->color_format;
		/*cur_dest_color_fmt = prop->dest_cfmt;*/
		/*pre_dest_color_fmt = pre_prop->dest_cfmt;*/

		vdin_hdr_flag = prop->vdin_hdr_flag;
		pre_vdin_hdr_flag = pre_prop->vdin_hdr_flag;
		if (vdin_hdr_flag != pre_vdin_hdr_flag ||
			prop->hdr_info.hdr_data.eotf != pre_prop->hdr_info.hdr_data.eotf) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				prop->hdr_info.hdr_check_cnt++;
			if (prop->hdr_info.hdr_check_cnt >=
			    devp->dts_config.vdin_hdr_chg_cnt) {
				prop->hdr_info.hdr_check_cnt = 0;
				signal_chg |= vdin_hdr_flag ?
					TVIN_SIG_CHG_SDR2HDR :
					TVIN_SIG_CHG_HDR2SDR;
				if (signal_chg &&
				    (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_2))
					pr_info("%s hdr chg 0x%x:(0x%x->0x%x), eotf:%d->%d\n",
						__func__,
						signal_chg, pre_vdin_hdr_flag,
						vdin_hdr_flag,
						devp->pre_prop.hdr_info.hdr_data.eotf,
						devp->prop.hdr_info.hdr_data.eotf);
				pre_prop->vdin_hdr_flag = prop->vdin_hdr_flag;
				pre_prop->hdr_info.hdr_data.eotf = prop->hdr_info.hdr_data.eotf;
			}
		} else {
			prop->hdr_info.hdr_check_cnt = 0;
		}

		vdin_hdr10p_flag = prop->hdr10p_info.hdr10p_on;
		pre_vdin_hdr10p_flag = pre_prop->hdr10p_info.hdr10p_on;
		if (vdin_hdr10p_flag != pre_vdin_hdr10p_flag) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				prop->hdr10p_info.hdr10p_check_cnt++;
			if (prop->hdr10p_info.hdr10p_check_cnt >=
			    devp->dts_config.vdin_hdr_chg_cnt) {
				prop->hdr10p_info.hdr10p_check_cnt = 0;
				signal_chg |= vdin_hdr10p_flag ?
					TVIN_SIG_CHG_SDR2HDR :
					TVIN_SIG_CHG_HDR2SDR;
				if (signal_chg &&
				    (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_1))
					pr_info("%s hdr10p chg 0x%x:(0x%x->0x%x)\n",
						__func__,
						signal_chg, pre_vdin_hdr10p_flag,
						vdin_hdr10p_flag);
				pre_prop->hdr10p_info.hdr10p_on = prop->hdr10p_info.hdr10p_on;
			}
		} else {
			prop->hdr10p_info.hdr10p_check_cnt = 0;
		}

		cur_dv_flag = prop->dolby_vision;
		pre_dv_flag = devp->dv.dv_flag;
		if (cur_dv_flag != pre_dv_flag) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				devp->dv.chg_cnt++;
			if (devp->dv.chg_cnt > devp->dts_config.vdin_dv_chg_cnt) {
				devp->dv.chg_cnt = 0;
				signal_chg |= cur_dv_flag ? TVIN_SIG_CHG_NO2DV :
						TVIN_SIG_CHG_DV2NO;
				if (signal_chg &&
				    (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_1))
					pr_info("%s dv chg0x%x:(0x%x->0x%x)\n",
						__func__,
						signal_chg, pre_dv_flag,
						cur_dv_flag);
				pre_prop->dolby_vision = prop->dolby_vision;
				devp->dv.dv_flag = prop->dolby_vision;
			}
		}

		if (prop->low_latency != devp->dv.low_latency &&
		    devp->dv.dv_flag) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				devp->dv.chg_cnt++;
			if (devp->dv.chg_cnt > devp->dts_config.vdin_dv_chg_cnt) {
				devp->dv.chg_cnt = 0;
				signal_chg |= TVIN_SIG_DV_CHG;
				if (signal_chg &&
				    (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_1))
					pr_info("%s LL chg0x%x:(0x%x->0x%x)\n",
						__func__,
						signal_chg, devp->dv.low_latency,
						prop->low_latency);
				pre_prop->low_latency = prop->low_latency;
				devp->dv.low_latency = prop->low_latency;
			}
		}

		if (devp->prop.latency.allm_mode !=
			devp->pre_prop.latency.allm_mode) {
			temp = devp->prop.fps > VDIN_HIGH_FRAME_RATE_TH ?
				devp->prop.fps / 60 + devp->dts_config.vdin_dv_chg_cnt :
				devp->dts_config.vdin_dv_chg_cnt;
			if (devp->dv.allm_chg_cnt > temp) {
				devp->dv.allm_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				temp = devp->pre_prop.latency.allm_mode;
				if (signal_chg)
					pr_info("%s allm chg:(0x%x->0x%x)\n",
						__func__,
						temp,
						devp->prop.latency.allm_mode);
				if (sm_ops->hdmi_is_xbox_dev) {
					if (sm_ops->hdmi_is_xbox_dev(devp->frontend,
						devp->port_type) && !devp->dv.dv_flag &&
						!devp->pre_prop.latency.allm_mode)
						devp->chg_drop_frame_cnt =
							devp->dts_config.vdin_re_cfg_drop_cnt;
				}
				devp->pre_prop.latency.allm_mode =
					devp->prop.latency.allm_mode;
			}
		}

		if (devp->pre_prop.latency.fmm_flag != devp->prop.latency.fmm_flag) {
			if (devp->dv.allm_chg_cnt > devp->dts_config.vdin_dv_chg_cnt) {
				devp->dv.allm_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				if (signal_chg)
					pr_info("%s filmmaker chg:(0x%x->0x%x)\n",
						__func__,
						devp->pre_prop.latency.fmm_flag,
						devp->prop.latency.fmm_flag);
				devp->pre_prop.latency.fmm_flag = devp->prop.latency.fmm_flag;
			}
		}

		if (devp->pre_prop.filmmaker.fmm_data.content_subtype !=
		    devp->prop.filmmaker.fmm_data.content_subtype) {
			signal_chg |= TVIN_SIG_CHG_FMM_SUBTYPE;
			if (signal_chg)
				pr_info("%s filmmaker content_subtype:(0x%x->0x%x)\n",
					__func__,
					devp->pre_prop.filmmaker.fmm_data.content_subtype,
					devp->prop.filmmaker.fmm_data.content_subtype);
			devp->pre_prop.filmmaker.fmm_data.content_subtype =
				devp->prop.filmmaker.fmm_data.content_subtype;
		}

		if (devp->pre_prop.imax_flag != devp->prop.imax_flag) {
			if (devp->dv.allm_chg_cnt > devp->dts_config.vdin_dv_chg_cnt) {
				devp->dv.allm_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				if (signal_chg)
					pr_info("%s imax chg:(0x%x->0x%x)\n",
						__func__,
						devp->pre_prop.imax_flag,
						devp->prop.imax_flag);
				devp->pre_prop.imax_flag = devp->prop.imax_flag;
			}
		}

		if (devp->pre_prop.latency.it_content !=
		    devp->prop.latency.it_content) {
			if (devp->dv.allm1_chg_cnt > devp->dts_config.vdin_dv_chg_cnt) {
				devp->dv.allm1_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				temp = devp->pre_prop.latency.it_content;
				if (signal_chg)
					pr_info("%s it_content chg:(0x%x->0x%x)\n",
						__func__,
						temp,
						devp->prop.latency.it_content);
				devp->pre_prop.latency.it_content =
					devp->prop.latency.it_content;
			}
		}

		if (devp->pre_prop.latency.cn_type !=
		    devp->prop.latency.cn_type) {
			if (devp->dv.allm1_chg_cnt > devp->dts_config.vdin_dv_chg_cnt) {
				devp->dv.allm1_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_DV_ALLM;
				temp = devp->pre_prop.latency.cn_type;
				if (signal_chg)
					pr_info("%s cn_type chg:(0x%x->0x%x)\n",
						__func__,
						temp,
						devp->prop.latency.cn_type);
				devp->pre_prop.latency.cn_type =
					devp->prop.latency.cn_type;
			}
		}

		if (devp->pre_prop.fps != devp->prop.fps &&
		    IS_HDMI_SRC(devp->parm.port) && !vdin_is_vrr_state(devp) &&
		    !(devp->prop.vtem_data.qms_en || devp->prop.qms_plus_flag)) {
			if (devp->sg_chg_fps_cnt > 8) {
				devp->sg_chg_fps_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_VS_FRQ;
				pr_info("%s fps chg:(%d->%d)\n", __func__,
					devp->pre_prop.fps, devp->prop.fps);
				devp->pre_prop.fps = devp->prop.fps;
				//devp->parm.info.fps = devp->prop.fps;
				vdin_get_base_fr(devp);
			}
		} else {
			devp->sg_chg_fps_cnt = 0;
		}
		if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_2)
			pr_info("%s(): PAR:%d->%d, AR:%d->%d, ACR:%d->%d, cnt:%d\n",
				__func__,
				devp->pre_prop.pic_aspect_ratio, devp->prop.pic_aspect_ratio,
				devp->pre_prop.aspect_ratio, devp->prop.aspect_ratio,
				devp->pre_prop.active_ratio, devp->prop.active_ratio,
				devp->sg_chg_afd_cnt);

		if (devp->pre_prop.aspect_ratio != devp->prop.aspect_ratio ||
			devp->pre_prop.pic_aspect_ratio != devp->prop.pic_aspect_ratio ||
			devp->pre_prop.active_ratio != devp->prop.active_ratio) {
			if (devp->sg_chg_afd_cnt > 1 ||
				(IS_TVAFE_SRC(port))) {
				signal_chg |= TVIN_SIG_CHG_AFD;
				pr_info("%s(): PAR:%d->%d, AR:%d->%d, ACR:%d->%d\n",
					__func__,
					devp->pre_prop.pic_aspect_ratio,
					devp->prop.pic_aspect_ratio,
					devp->pre_prop.aspect_ratio, devp->prop.aspect_ratio,
					devp->pre_prop.active_ratio, devp->prop.active_ratio);
				devp->pre_prop.aspect_ratio = devp->prop.aspect_ratio;
				devp->pre_prop.pic_aspect_ratio = devp->prop.pic_aspect_ratio;
				devp->pre_prop.active_ratio = devp->prop.active_ratio;
				devp->parm.info.aspect_ratio = prop->aspect_ratio;
				devp->misc_info.afd_info = (devp->prop.pic_aspect_ratio << 4) |
							devp->prop.active_ratio;
			}
		} else {
			devp->sg_chg_afd_cnt = 0;
		}

		if (vdin_is_vrr_state_chg(devp) ||
		    vdin_check_freesync_state_chg(devp)) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED))
				devp->vrr_data.vrr_chg_cnt++;
			if (devp->vrr_data.vrr_chg_cnt > devp->dts_config.vdin_vrr_chg_cnt ||
			    devp->vrr_data.freesync_chg_cnt > devp->dts_config.vdin_vrr_chg_cnt) {
				devp->vrr_data.vrr_chg_cnt = 0;
				devp->vrr_data.freesync_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_VRR;
				vdin_frame_lock_check(devp, 1);
				if (signal_chg && (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_1))
					pr_info("%s vrr_chg:(%d->%d) spd:([5]%d->%d) [0](%d->%d)\n",
						__func__,
						devp->vrr_data.vdin_vrr_en_flag,
						devp->prop.vtem_data.vrr_en,
						devp->pre_prop.spd_data.data[5],
						devp->prop.spd_data.data[5],
						devp->pre_prop.spd_data.data[0],
						devp->prop.spd_data.data[0]);
			}
		} else {
			devp->vrr_data.vrr_chg_cnt = 0;
			devp->vrr_data.freesync_chg_cnt = 0;
		}

		if (vdin_is_qms_state_chg(devp)) {
			devp->vrr_data.qms_chg_cnt++;
			if (devp->vrr_data.qms_chg_cnt >= devp->dts_config.vdin_qms_chg_cnt) {
				devp->vrr_data.qms_chg_cnt = 0;
				signal_chg |= TVIN_SIG_CHG_QMS;
			}
		} else {
			devp->vrr_data.qms_chg_cnt = 0;
		}

		if (vdin_is_qms_plus_state_chg(devp)) {
			devp->vrr_data.qms_plus_chg_cnt++;
			if (devp->vrr_data.qms_plus_chg_cnt >= devp->dts_config.vdin_qms_chg_cnt) {
				devp->vrr_data.qms_plus_chg_cnt = 0;
				pr_info("%s qms+ chg:%d->%d\n", __func__,
					devp->pre_prop.qms_plus_flag,
					devp->prop.qms_plus_flag);
				if (devp->prop.qms_plus_flag)
					signal_chg |= TVIN_SIG_CHG_QMS;
				devp->pre_prop.qms_plus_flag = devp->prop.qms_plus_flag;
			}
		} else {
			devp->vrr_data.qms_plus_chg_cnt = 0;
		}

		if (devp->pre_prop.hdcp_sts != devp->prop.hdcp_sts) {
			signal_chg |= TVIN_SIG_CHG_HDCP_STS;
			if (signal_chg)
				pr_info("%s hdcp sts chg:(0x%x->0x%x)\n",
					__func__,
					devp->pre_prop.hdcp_sts,
					devp->prop.hdcp_sts);
			devp->pre_prop.hdcp_sts = devp->prop.hdcp_sts;
		}

		if (color_range_force)
			prop->color_fmt_range =
			tvin_get_force_fmt_range(pre_prop->color_format);
		vdin_fmt_range = prop->color_fmt_range;
		pre_vdin_fmt_range = pre_prop->color_fmt_range;

		if (devp->flags & VDIN_FLAG_DEC_STARTED &&
		    (cur_color_fmt != pre_color_fmt ||
		     vdin_fmt_range != pre_vdin_fmt_range)) {
			if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_1 && devp->csc_cfg == 0)
				pr_info("[smr.%d] fmt(%d->%d), fmt_range(%d->%d), csc_cfg:0x%x\n",
					devp->index,
					pre_color_fmt, cur_color_fmt,
					pre_vdin_fmt_range, vdin_fmt_range,
					devp->csc_cfg);

			if (cur_color_fmt != pre_color_fmt) {
				if (!devp->game_mode) {
					vdin_vf_skip_all_disp(devp->vfp);
					devp->chg_drop_frame_cnt =
						devp->dts_config.vdin_re_cfg_drop_cnt;
				}
				vdin_get_format_convert(devp);
			}
			devp->csc_cfg = 1;
		}
	}

	return signal_chg;
}

bool vdin_is_cut_win_changed(struct vdin_dev_s *devp)
{
	if (devp->prop.pre_vs != devp->prop.vs || devp->prop.pre_ve != devp->prop.ve ||
	    devp->prop.pre_hs != devp->prop.hs || devp->prop.pre_he != devp->prop.he) {
		if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_4)
			pr_info("vdin%d pre->cur,vs(%d->%d),ve(%d->%d),hs(%d->%d),he(%d->%d)\n",
				devp->index, devp->prop.pre_vs, devp->prop.vs,
				devp->prop.pre_ve, devp->prop.ve,
				devp->prop.pre_hs, devp->prop.hs,
				devp->prop.pre_he, devp->prop.he);
		return true;
	}
	return false;
}

int vdin_hdmirx_fps_abn_check(struct vdin_dev_s *devp, unsigned int fps)
{
	if (!fps)
		return true;
	if (devp->prop.frl_flag &&
	    ((devp->fmt_info_p->h_active >= 3840 &&
	    devp->fmt_info_p->v_active >= 2160 && fps > 180) ||
	    (devp->fmt_info_p->h_active >= 1920 &&
	    devp->fmt_info_p->v_active >= 1080 && fps > 720)))
		return true;
	return false;
}

/* check auto de to adjust vdin cut window */
void vdin_auto_de_handler(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_sig_property_s *prop;

	if (!devp) {
		return;
	} else if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return;
	}
	if (devp->auto_cut_window_en == 0)
		return;
	prop = &devp->prop;
	sm_ops = devp->frontend->sm_ops;

	if ((devp->flags & VDIN_FLAG_DEC_STARTED) && IS_TVAFE_SRC(devp->parm.port) &&
	    sm_ops->get_sig_property && !devp->cut_window_cfg) {
		sm_ops->get_sig_property(devp->frontend, prop, devp->port_type);

		if (devp->debug.cutwin.en) {
			devp->prop.hs = devp->debug.cutwin.hs;
			devp->prop.he = devp->debug.cutwin.he;
			devp->prop.vs = devp->debug.cutwin.vs;
			devp->prop.ve = devp->debug.cutwin.ve;
		}
		if (vdin_is_cut_win_changed(devp))
			devp->cut_window_cfg = 1;
	}
}

static inline bool vdin_is_need_send_event(struct vdin_dev_s *devp,
					struct tvin_info_s *info)
{
	if (!(devp->flags & VDIN_FLAG_SNOW_FLAG) &&
	    ((devp->flags & VDIN_FLAG_DEC_STARTED &&
	      info->status == TVIN_SIG_STATUS_UNSTABLE &&
	      !(devp->vdin_stable_cnt % VDIN_SEND_EVENT_INTERVAL)) ||
	      (!(devp->flags & VDIN_FLAG_DEC_STARTED) &&
	      info->status == TVIN_SIG_STATUS_NOSIG &&
	      atv_std_flag && (devp->vdin_nosig_cnt >= 20)) ||
	     (!(devp->flags & VDIN_FLAG_DEC_STARTED) &&
	      info->status == TVIN_SIG_STATUS_STABLE &&
	      devp->vdin_stable_cnt >= VDIN_STABLED_CNT &&
	      !(devp->vdin_stable_cnt % VDIN_SEND_EVENT_INTERVAL) &&
	      info->fmt != TVIN_SIG_FMT_NULL)))
		return true;
	else
		return false;
}

void tvin_smr_init_counter(int index)
{
	sm_dev[index].state_cnt          = 0;
	sm_dev[index].exit_nosig_cnt     = 0;
	sm_dev[index].back_nosig_cnt     = 0;
	sm_dev[index].back_stable_cnt    = 0;
	sm_dev[index].exit_prestable_cnt = 0;
}

u32 tvin_hdmirx_signal_type_check(struct vdin_dev_s *devp, enum tvin_sm_status_e state)
{
	unsigned int signal_type = 0;
	enum tvin_sg_chg_flg signal_chg = TVIN_SIG_CHG_NONE;
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_sig_property_s *prop;
	unsigned int i;
	u32 val = 0;

	if (state < TVIN_SM_STATUS_PRESTABLE)
		return TVIN_SIG_CHG_NONE;

	/* need always polling the signal property, if isr enable,
	 * it be called in isr
	 */
	prop = &devp->prop;
	if (!(devp->flags & VDIN_FLAG_ISR_EN) && devp->frontend) {
		sm_ops = devp->frontend->sm_ops;
		if (sm_ops && sm_ops->get_sig_property) {
			if (IS_TVAFE_SRC(devp->parm.port) ||
			    devp->dts_config.vdin_get_prop_in_sm_en)
				sm_ops->get_sig_property(devp->frontend,
					&devp->prop, devp->port_type);
			/*devp->dv.dv_flag = devp->prop.dolby_vision;*/
		}
	}

	memcpy(&devp->dv.dv_vsif,
	       &prop->dv_vsif, sizeof(struct tvin_dv_vsif_s));

	if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_7) {
		pr_info("[sm.%d]pkttype:%#x,version:%#x,length:%#x,checksum:%#x\n",
			devp->index,
			devp->prop.spd_data.pkttype, devp->prop.spd_data.version,
			devp->prop.spd_data.length, devp->prop.spd_data.checksum);
		for (i = 0; i < sizeof(devp->prop.spd_data.data); i += 4)
			pr_info("data[%d ~ %d]=%#x %#x %#x %#x\n", i, i + 3,
				devp->prop.spd_data.data[i], devp->prop.spd_data.data[i + 1],
				devp->prop.spd_data.data[i + 2], devp->prop.spd_data.data[i + 3]);
		pr_info("vrr_en:%d,const:%#x,qms:%#x,fva:%#x,v_front:%d,fr:%d,vic:%d\n",
			devp->prop.vtem_data.vrr_en, devp->prop.vtem_data.m_const,
			devp->prop.vtem_data.qms_en, devp->prop.vtem_data.fva_factor_m1,
			devp->prop.vtem_data.base_vfront, devp->prop.vtem_data.base_framerate,
			devp->prop.hw_vic);
	}

	if (devp->prop.dolby_vision)
		signal_type |= (1 << 30);
	else
		signal_type &= ~(1 << 30);
	/* check dv end */

	/* check HDR/HLG begin */
	if (prop->hdr_info.hdr_state == HDR_STATE_GET) {
		devp->prop.vdin_hdr_flag = true;
		if (prop->hdr_info.hdr_data.eotf ==
		    EOTF_SMPTE_ST_2048 ||
		    prop->hdr_info.hdr_data.eotf == EOTF_HDR) {
			signal_type |= (1 << 29);
			signal_type &= ~(1 << 25);/* 0:limit */
			signal_type = ((9 << 16) |
				(signal_type & (~0xFF0000)));
			signal_type = ((16 << 8) |
				(signal_type & (~0xFF00)));
			signal_type = ((9 << 0) |
				(signal_type & (~0xFF)));
		} else if (devp->prop.hdr_info.hdr_data.eotf ==
			   EOTF_HLG) {
			signal_type |= (1 << 29);
			signal_type &= ~(1 << 25);/* 0:limit */
			signal_type = ((9 << 16) |
				(signal_type & (~0xFF0000)));
			signal_type = ((14 << 8) |
				(signal_type & (~0xFF00)));
			signal_type = ((9 << 0) |
				(signal_type & (~0xFF)));
		} else {
			devp->prop.vdin_hdr_flag = false;
			signal_type &= ~(1 << 29);
			signal_type &= ~(1 << 25);
			val = vdin_matrix_range_chk(devp);
			signal_type |= (val << 25);
			/* default is bt709,if change need sync */
			signal_type = ((1 << 16) |
				(signal_type & (~0xFF0000)));
			signal_type = ((1 << 8) |
				(signal_type & (~0xFF00)));
		}
	} else if (prop->hdr_info.hdr_state == HDR_STATE_NULL) {
		devp->prop.vdin_hdr_flag = false;
		signal_type &= ~(1 << 29);
		signal_type &= ~(1 << 25);
		val = vdin_matrix_range_chk(devp);
		signal_type |= (val << 25);
		/* default is bt709,if change need sync */
		signal_type = ((1 << 16) | (signal_type & (~0xFF0000)));
		signal_type = ((1 << 8) | (signal_type & (~0xFF00)));
	}
	/* check HDR/HLG end */

	/* check HDR 10+ begin */
	if (prop->hdr10p_info.hdr10p_on) {
		devp->prop.vdin_hdr_flag = true;

		signal_type |= (1 << 29);/* present_flag */
		signal_type &= ~(1 << 25);/* 0:limited */

		/* color_primaries */
		signal_type = ((9 << 16) | (signal_type & (~0xFF0000)));

		/*transfer_characteristic*/
		signal_type = ((0x30 << 8) | (signal_type & (~0xFF00)));

		/* matrix_coefficient */
		signal_type = ((9 << 0) | (signal_type & (~0xFF)));
	}

	if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_4)
		pr_info("[sm.%d]dv:%d, hdr state:%d %d,eotf:%d flag:%#x,vrr state:%d type:%#x\n",
			devp->index,
			devp->prop.dolby_vision, devp->prop.hdr_info.hdr_state,
			devp->prop.dv_unique_drm_flag,
			devp->prop.hdr_info.hdr_data.eotf, devp->prop.vdin_hdr_flag,
			devp->prop.vdin_vrr_flag, signal_type);

	//if (devp->prop.vdin_hdr_flag &&
	    //devp->parm.info.signal_type != signal_type) {
		//signal_chg |= TVIN_SIG_CHG_SDR2HDR;
	//}
	/* check HDR 10+ end */

	/* check vrr begin */
	if (devp->prop.vdin_vrr_flag)
		signal_type |= (1 << 31);
	else
		signal_type &= ~(1 << 31);
	/* check vrr end */

	devp->parm.info.dolby_vision = devp->prop.dolby_vision;
	if (devp->prop.dolby_vision)
		devp->parm.info.low_latency = devp->prop.low_latency;
	else
		devp->parm.info.low_latency = 0;

	devp->parm.info.signal_type = signal_type;
	/* hdmirx avi colorimetry bit[11:8] + ext_colorimetry bit[15:12] */
	devp->parm.info.input_colorimetry =
		vdin_get_rx_avi_colorimetry(devp, devp->parm.info.input_colorimetry);

	return signal_chg;
}

void reset_tvin_smr(unsigned int index)
{
	sm_dev[index].sig_status = TVIN_SIG_STATUS_NULL;
}

void tvin_sig_chg_event_process(struct vdin_dev_s *devp, u32 chg)
{
	/*struct tvin_sm_s *sm_p;*/
	bool re_cfg = 0;

	if (chg & TVIN_SIG_CHG_STS) {
		devp->event_info.event_sts = TVIN_SIG_CHG_STS;
	} else {
		if (chg & TVIN_SIG_DV_CHG) {
			devp->event_info.event_sts = (chg & TVIN_SIG_DV_CHG);
			if (devp->dts_config.vdin_re_config & RE_CONFIG_DV_EN)
				re_cfg = true;
		} else if (chg & TVIN_SIG_HDR_CHG) {
			devp->event_info.event_sts = (chg & TVIN_SIG_HDR_CHG);
			if (devp->dts_config.vdin_re_config & RE_CONFIG_HDR_EN)
				re_cfg = true;
		} else if (chg & TVIN_SIG_CHG_COLOR_FMT) {
			devp->event_info.event_sts = TVIN_SIG_CHG_COLOR_FMT;
		} else if (chg & TVIN_SIG_CHG_RANGE) {
			devp->event_info.event_sts = TVIN_SIG_CHG_RANGE;
		} else if (chg & TVIN_SIG_CHG_BIT) {
			devp->event_info.event_sts = TVIN_SIG_CHG_BIT;
		} else if (chg & TVIN_SIG_CHG_VS_FRQ) {
			devp->event_info.event_sts = TVIN_SIG_CHG_VS_FRQ;
		} else if (chg & TVIN_SIG_CHG_DV_ALLM) {
			devp->event_info.event_sts = TVIN_SIG_CHG_DV_ALLM;
			if (devp->dts_config.vdin_re_config & RE_CONFIG_ALLM_EN)
				re_cfg = true;
		} else if (chg & TVIN_SIG_CHG_AFD) {
			devp->event_info.event_sts = TVIN_SIG_CHG_AFD;
		} else if (chg & TVIN_SIG_CHG_VRR) {
			devp->event_info.event_sts = TVIN_SIG_CHG_VRR;
			memcpy(&devp->pre_prop.spd_data, &devp->prop.spd_data,
				sizeof(devp->prop.spd_data));
			devp->vrr_data.cur_vrr_status = get_cur_vrr_status(devp);
			devp->vrr_data.pre_vrr_status = devp->vrr_data.cur_vrr_status;
			devp->pre_prop.vtem_data.vrr_en =
				devp->prop.vtem_data.vrr_en;
			devp->vrr_data.vdin_vrr_en_flag =
				devp->prop.vtem_data.vrr_en;
		} else if (chg & TVIN_SIG_CHG_QMS) {
			devp->event_info.event_sts = TVIN_SIG_CHG_QMS;
			memcpy(&devp->pre_prop.vtem_data, &devp->prop.vtem_data,
				sizeof(devp->prop.vtem_data));
			if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_1) {
				pr_info("qms_chg:(%d->%d) const:%d,fr:%d,vf:%d,base:%d\n",
					devp->pre_prop.vtem_data.qms_en,
					devp->prop.vtem_data.qms_en,
					devp->prop.vtem_data.m_const,
					devp->prop.vtem_data.next_tfr,
					devp->prop.vtem_data.base_vfront,
					devp->prop.vtem_data.base_framerate);
			}
		} else {
			return;
		}
	}

	if (re_cfg) {
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			devp->parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
			sm_dev[devp->index].state = TVIN_SM_STATUS_UNSTABLE;
			devp->frame_drop_num = devp->dts_config.vdin_re_cfg_drop_cnt;
			devp->re_cfg = true;
		}
		if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_1)
			pr_info("vdin reconfig, flags = 0x%x;%#x\n", devp->flags, chg);
	}
	devp->pre_event_info.event_sts = devp->event_info.event_sts;
	vdin_send_event(devp, devp->event_info.event_sts);
	wake_up(&devp->queue);
}

/*
 * tvin state machine routine
 *
 */
void tvin_smr(struct vdin_dev_s *devp)
{
	struct tvin_state_machine_ops_s *sm_ops;
	struct tvin_info_s *info;
	enum tvin_port_e port = TVIN_PORT_NULL;
	unsigned int unstable_in;
	struct tvin_sm_s *sm_p;
	struct tvin_frontend_s *fe;
	struct tvin_sig_property_s *prop, *pre_prop;
	enum tvin_sg_chg_flg signal_chg = TVIN_SIG_CHG_NONE;
	unsigned int cnt, tmp;

	if (!devp) {
		return;
	} else if (!devp->frontend) {
		sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
		return;
	}

	if (devp->flags & VDIN_FLAG_SM_DISABLE ||
	    devp->flags & VDIN_FLAG_SUSPEND)
		return;

	if (!(devp->flags & VDIN_FLAG_DEC_OPENED))
		return;

	sm_p = &sm_dev[devp->index];
	fe = devp->frontend;
	sm_ops = devp->frontend->sm_ops;
	info = &devp->parm.info;
	port = devp->parm.port;
	prop = &devp->prop;
	pre_prop = &devp->pre_prop;

	signal_chg = tvin_hdmirx_signal_type_check(devp, sm_p->state);

	switch (sm_p->state) {
	case TVIN_SM_STATUS_NOSIG:
		++sm_p->state_cnt;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
		if (IS_TVAFE_ATV_SRC(port) &&
		    (devp->flags & VDIN_FLAG_SNOW_FLAG))
			tvafe_snow_config_clamp(1);
#endif
		if (sm_ops->nosig(devp->frontend, devp->port_type)) {
			if (IS_TVAFE_ATV_SRC(port) && atv_std_flag)
				devp->vdin_nosig_cnt++;
			sm_p->exit_nosig_cnt = 0;
			if (sm_p->state_cnt >= devp->dts_config.nosig_in_cnt) {
				sm_p->state_cnt = devp->dts_config.nosig_in_cnt;
				info->status = TVIN_SIG_STATUS_NOSIG;
				if (!(IS_TVAFE_ATV_SRC(port) &&
				      (devp->flags & VDIN_FLAG_SNOW_FLAG)))
					info->fmt = TVIN_SIG_FMT_NULL;
				if (devp->debug.sm_debug_enable && !sm_print_nosig) {
					pr_info("[smr.%d] no signal\n",
						devp->index);
					sm_print_nosig = 1;
				}
			}
		} else {
			if (IS_TVAFE_SRC(port))
				devp->dts_config.nosig2_unstable_cnt = 2;
			++sm_p->exit_nosig_cnt;
			if (sm_p->exit_nosig_cnt >= devp->dts_config.nosig2_unstable_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (devp->debug.sm_debug_enable) {
					pr_info("[smr.%d] no signal --> unstable\n",
						devp->index);
					pr_info("unstable_cnt:0x%x\n",
						devp->dts_config.nosig2_unstable_cnt);
				}
				if (IS_HDMI_SRC(port))
					devp->dts_config.nosig2_unstable_cnt = 5;

				sm_print_nosig  = 0;
				devp->vdin_nosig_cnt = 0;
			}
		}
		break;
	case TVIN_SM_STATUS_UNSTABLE:
		++sm_p->state_cnt;
		if (sm_ops->nosig(devp->frontend, devp->port_type)) {
			sm_p->back_stable_cnt = 0;
			++sm_p->back_nosig_cnt;
			if (sm_p->back_nosig_cnt >= sm_p->back_nosig_max_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_NOSIG;
				info->status = TVIN_SIG_STATUS_NOSIG;
				if (!(IS_TVAFE_ATV_SRC(port) &&
				      (devp->flags & VDIN_FLAG_SNOW_FLAG)))
					info->fmt = TVIN_SIG_FMT_NULL;
				if (devp->debug.sm_debug_enable)
					pr_info("[smr.%d] unstable --> no signal\n",
						devp->index);
				sm_print_nosig  = 0;
				if (devp->debug.unplug_mute_en)
					vdin_mute_vpp(devp, true);
			}
		} else {
			sm_p->back_nosig_cnt = 0;
			if (sm_ops->fmt_changed(devp->frontend, devp->port_type)) {
				sm_p->back_stable_cnt = 0;
				if (IS_TVAFE_ATV_SRC(port) &&
				    devp->unstable_flag &&
				    (devp->flags & VDIN_FLAG_SNOW_FLAG))
					/* UNSTABLE_ATV_MAX_CNT; */
					unstable_in = sm_p->atv_unstable_in_cnt;
				else
					unstable_in = devp->dts_config.other_unstable_in_cnt;
				if (sm_p->state_cnt >= unstable_in) {
					sm_p->state_cnt  = unstable_in;
					info->status = TVIN_SIG_STATUS_UNSTABLE;
					/*info->fmt = TVIN_SIG_FMT_NULL;*/
					sm_print_nosig  = 0;
				}
			} else {
				++sm_p->back_stable_cnt;
				if (IS_TVAFE_ATV_SRC(port) &&
				    (devp->flags & VDIN_FLAG_SNOW_FLAG))
					unstable_in = sm_p->atv_unstable_out_cnt;
				else if (IS_TVAFE_ATV_SRC(port) && manual_flag)
					unstable_in = manual_unstable_out_cnt;
				else if (IS_HDMI_SRC(port))
					unstable_in = sm_p->hdmi_unstable_out_cnt;
				else
					unstable_in = devp->dts_config.other_unstable_out_cnt;

				cnt = sizeof(struct tvin_sig_property_s);
				 /* must wait enough time for cvd signal lock */
				if (sm_p->back_stable_cnt >= unstable_in) {
					sm_p->back_stable_cnt = 0;
					sm_p->state_cnt = 0;
					if (sm_ops->get_fmt &&
					    sm_ops->get_sig_property) {
						info->fmt =
							sm_ops->get_fmt(fe, devp->port_type);
						/*sm_ops->get_sig_property(fe,*/
						/*prop);*/
						info->cfmt = prop->color_format;
						memcpy(pre_prop, prop, cnt);
					}
					if (info->fmt == TVIN_SIG_FMT_NULL) {
						sm_p->state = TVIN_SM_STATUS_UNSTABLE;
						info->status = TVIN_SIG_STATUS_UNSTABLE;
						break;
					}
					if (sm_ops->fmt_config)
						sm_ops->fmt_config(fe);

					tvin_smr_init_counter(devp->index);
					sm_p->state = TVIN_SM_STATUS_PRESTABLE;
					sm_p->exit_prestable_cnt = 0;
					sm_atv_prestable_fmt = info->fmt;

					if (devp->debug.sm_debug_enable) {
						pr_info("[smr.%d]unstable-->prestable",
							devp->index);
						pr_info("and format is %d(%s)\n",
							info->fmt,
						tvin_sig_fmt_str(info->fmt));
					}

					sm_print_nosig  = 0;
					sm_print_fmt_nosig = 0;
					sm_print_fmt_chg = 0;
					sm_print_prestable = 0;

				} else {
					info->status = TVIN_SIG_STATUS_UNSTABLE;

					if (devp->debug.sm_debug_enable &&
					    !sm_print_notsup)
						sm_print_notsup = 1;
				}
			}
		}
		break;
	case TVIN_SM_STATUS_PRESTABLE: {
		bool nosig = false, fmt_changed = false;
		unsigned int prestable_out_cnt = 0;

		devp->unstable_flag = true;
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN_AFE
		if (IS_TVAFE_ATV_SRC(port) &&
		    (devp->flags & VDIN_FLAG_SNOW_FLAG))
			tvafe_snow_config_clamp(0);
#endif
		if (sm_ops->nosig(devp->frontend, devp->port_type)) {
			nosig = true;
			if (devp->debug.sm_debug_enable && !(sm_print_prestable & 0x1)) {
				pr_info("[smr.%d] warning: no signal\n",
					devp->index);
				sm_print_prestable |= 1;
			}
		}

		if (sm_ops->fmt_changed(devp->frontend, devp->port_type)) {
			fmt_changed = true;
			if (devp->debug.sm_debug_enable && !(sm_print_prestable & 0x2)) {
				pr_info("[smr.%d] warning: format changed\n",
					devp->index);
				sm_print_prestable |= (1 << 1);
			}
		}

		if (nosig || fmt_changed) {
			++sm_p->state_cnt;
			if (IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG))
				prestable_out_cnt = devp->dts_config.atv_prestable_out_cnt;
			else
				prestable_out_cnt = devp->dts_config.other_stable_out_cnt;
			if (sm_p->state_cnt >= prestable_out_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				if (devp->debug.sm_debug_enable)
					pr_info("[smr.%d] prestable --> unstable\n",
						devp->index);
				sm_print_nosig  = 0;
				sm_print_notsup = 0;
				sm_print_prestable = 0;
				break;
			}
		} else {
			sm_p->state_cnt = 0;

			if (IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG)) {
				++sm_p->exit_prestable_cnt;
				if (sm_p->exit_prestable_cnt <
				    devp->dts_config.atv_prestable_out_cnt)
					break;
				else
					sm_p->exit_prestable_cnt = 0;
			}
			devp->fmt_info_p = (struct tvin_format_s *)tvin_get_fmt_info(info->fmt);
			/* spd_data maybe zeros randomly in freesync
			 * wait hdmi_prestable_out_cnt times for
			 * spd_data ready
			 */
			if (IS_HDMI_SRC(port)) {
				++sm_p->exit_prestable_cnt;
				if (sm_p->exit_prestable_cnt <=
					hdmi_prestable_out_cnt) {
					tmp = vdin_get_base_fr(devp);
					if (vdin_hdmirx_fps_abn_check(devp, tmp))
						break;
				}
			}

			if (IS_TVAFE_SRC(port) && sm_ops->get_sig_property) {
				sm_ops->get_sig_property(devp->frontend, prop, devp->port_type);
				devp->parm.info.fps = prop->fps;
			}

			if (IS_HDMI_SRC(port)) {
				/* for tvstart, do not judge VDIN_FLAG_DEC_STARTED */
				if (/*!(devp->flags & VDIN_FLAG_DEC_STARTED) &&*/
				    !mutex_is_locked(&devp->fe_lock) && info->fmt) {
					sm_p->state = TVIN_SM_STATUS_STABLE;
					info->status = TVIN_SIG_STATUS_STABLE;
					vdin_update_prop(devp);
					vdin_update_parm(devp);
					/* vrr case */
					devp->vrr_data.vdin_vrr_en_flag =
						devp->pre_prop.vtem_data.vrr_en;
					/* sometime alloc mem too long signal detected again */
					if (!mutex_is_locked(&devp->fe_lock))
						devp->csc_cfg = 0;

					if (devp->debug.sm_debug_enable)
						pr_info("[smr.%d] %ums prestable --> stable(%#x)\n",
							devp->index, jiffies_to_msecs(jiffies),
							info->fmt);
					sm_print_nosig  = 0;
					sm_print_notsup = 0;
					sm_print_prestable = 0;
				} else {
					sm_p->state = TVIN_SM_STATUS_UNSTABLE;
					if (info->fmt)
						info->status = TVIN_SIG_STATUS_UNSTABLE;
					else
						info->status = TVIN_SIG_STATUS_NOSIG;
					devp->frame_drop_num =
						devp->dts_config.vdin_re_cfg_drop_cnt;
				}
			} else {
				sm_p->state = TVIN_SM_STATUS_STABLE;
				info->status = TVIN_SIG_STATUS_STABLE;
				vdin_update_prop(devp);
				vdin_update_parm(devp);
				/* vrr case */
				devp->vrr_data.vdin_vrr_en_flag =
					devp->pre_prop.vtem_data.vrr_en;
				/* sometime alloc mem too long signal detected again */
				if (!mutex_is_locked(&devp->fe_lock)) {
					devp->csc_cfg = 0;
					devp->cut_window_cfg = 0;
				}
				if (devp->debug.sm_debug_enable)
					pr_info("[smr.%d] %ums prestable --> stable(%#x)\n",
						devp->index, jiffies_to_msecs(jiffies), info->fmt);
				sm_print_nosig  = 0;
				sm_print_notsup = 0;
				sm_print_prestable = 0;
				if (devp->debug.unplug_mute_en)
					vdin_mute_vpp(devp, false);
			}
		}
		break;
	}
	case TVIN_SM_STATUS_STABLE: {
		bool nosig = false, fmt_changed = false;
		unsigned int stable_out_cnt = 0;
		unsigned int stable_fmt = 0;

		devp->unstable_flag = true;
		if (sm_ops->nosig(devp->frontend, devp->port_type)) {
			nosig = true;
			if (devp->debug.sm_debug_enable && !sm_print_fmt_nosig) {
				pr_info("[smr.%d] warning: no signal\n",
					devp->index);
				sm_print_fmt_nosig = 1;
			}
		}

		if (sm_ops->fmt_changed(devp->frontend, devp->port_type)) {
			fmt_changed = true;
			if (devp->debug.sm_debug_enable && !sm_print_fmt_chg) {
				pr_info("[smr.%d] warning: format changed\n",
					devp->index);
				sm_print_fmt_chg = 1;
			}
		}
		/* dynamic adjust cut window */
		vdin_auto_de_handler(devp);

		if (nosig || fmt_changed /* || !pll_lock */) {
			++sm_p->state_cnt;
			if (IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG))
				stable_out_cnt = sm_p->atv_stable_out_cnt;
			else if (IS_HDMI_SRC(port))
				stable_out_cnt = devp->dts_config.hdmi_stable_out_cnt;
			else
				stable_out_cnt = devp->dts_config.other_stable_out_cnt;
			/*add for atv snow*/
			if (sm_p->state_cnt >= devp->dts_config.atv_stable_fmt_check_cnt &&
			    IS_TVAFE_ATV_SRC(port) &&
			    (devp->flags & VDIN_FLAG_SNOW_FLAG))
				atv_stable_fmt_check_enable = 1;
			if (sm_p->state_cnt >= stable_out_cnt) {
				tvin_smr_init_counter(devp->index);
				sm_p->state = TVIN_SM_STATUS_UNSTABLE;
				info->status = TVIN_SIG_STATUS_UNSTABLE;
				if (devp->debug.sm_debug_enable)
					pr_info("[smr.%d] stable --> unstable\n",
						devp->index);
				sm_print_nosig  = 0;
				sm_print_notsup = 0;
				sm_print_fmt_nosig = 0;
				sm_print_fmt_chg = 0;
				sm_print_prestable = 0;
				atv_stable_fmt_check_enable = 0;
				devp->vdin_stable_cnt = 0;
			}
		} else {
			/*add for atv snow*/
			if (IS_TVAFE_ATV_SRC(port) &&
			    atv_stable_fmt_check_enable &&
				(devp->flags & VDIN_FLAG_SNOW_FLAG) &&
				(sm_ops->get_fmt && sm_ops->get_sig_property)) {
				sm_p->state_cnt = 0;
				stable_fmt = sm_ops->get_fmt(fe, devp->port_type);
				if (sm_atv_prestable_fmt != stable_fmt &&
				    stable_fmt != TVIN_SIG_FMT_NULL) {
					/*cvbs in*/
					sm_ops->get_sig_property(fe, prop, devp->port_type);
					memcpy(pre_prop, prop,
					       sizeof(struct
						      tvin_sig_property_s));
					devp->parm.info.trans_fmt =
						prop->trans_fmt;
					devp->parm.info.is_dvi =
						prop->dvi_info;
					devp->parm.info.fps =
						prop->fps;
					info->fmt = stable_fmt;
					atv_stable_fmt_check_enable = 0;
					if (devp->debug.sm_debug_enable)
						pr_info("[smr.%d] stable fmt changed:0x%x-->0x%x\n",
							devp->index,
							sm_atv_prestable_fmt,
							stable_fmt);
					sm_atv_prestable_fmt = stable_fmt;
				}
			}
			devp->vdin_stable_cnt++;
			sm_p->state_cnt = 0;
			signal_chg |= vdin_hdmirx_fmt_chg_detect(devp);
			if (signal_chg)
				tvin_sig_chg_event_process(devp, signal_chg);
		}
		/* check unreliable vsync interrupt */
		if (devp->unreliable_vs_cnt != devp->unreliable_vs_cnt_pre) {
			devp->unreliable_vs_cnt_pre = devp->unreliable_vs_cnt;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			if (devp->debug.sm_debug_enable & 0x2)
				vdin_dump_vs_info(devp);
#endif
		}
		break;
	}
	case TVIN_SM_STATUS_NULL:
	default:
		sm_p->state = TVIN_SM_STATUS_NOSIG;
		break;
	}

	if (devp->debug.sm_debug_enable & VDIN_SM_LOG_L_5)
		pr_info("[smr.%d]status:%x %x %x\n", devp->index,
			sm_p->state, info->status, sm_p->sig_status);
	/* signal status changed during start_dec need to restart vdin */
	if (devp->is_status_locked && sm_p->sig_status != info->status)
		devp->is_need_re_cfg = true;
	if (devp->is_need_re_cfg) {
		devp->parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
		sm_dev[devp->index].state = TVIN_SM_STATUS_UNSTABLE;
	}

	if (devp->flags & VDIN_FLAG_DEC_OPENED) {
		if ((sm_p->sig_status != info->status ||
		     vdin_is_need_send_event(devp, info)) &&
		    !mutex_is_locked(&devp->fe_lock)) {
			devp->is_need_re_cfg = false;
			sm_p->sig_status = info->status;
			devp->event_info.event_sts = TVIN_SIG_CHG_STS;
			devp->pre_event_info.event_sts =
				devp->event_info.event_sts;
			vdin_send_event(devp, devp->event_info.event_sts);
			devp->vdin_nosig_cnt = 0;
			atv_std_flag = 0;
			wake_up(&devp->queue);
		}
	}

	if (sm_p->sig_status != signal_status &&
		sm_p->state == TVIN_SM_STATUS_UNSTABLE)
		vrr_check_dec(0);

	signal_status = sm_p->sig_status;
}

/*
 * tvin state machine routine init
 *
 */

void tvin_smr_init(struct vdin_dev_s *devp)
{
	sm_dev[devp->index].sig_status = TVIN_SIG_STATUS_NULL;
	sm_dev[devp->index].state = TVIN_SM_STATUS_NULL;
	sm_dev[devp->index].atv_stable_out_cnt = devp->dts_config.atv_stable_out_cnt;
	sm_dev[devp->index].atv_unstable_in_cnt = devp->dts_config.atv_unstable_in_cnt;
	sm_dev[devp->index].back_nosig_max_cnt = devp->dts_config.back_nosig_max_cnt;
	sm_dev[devp->index].atv_unstable_out_cnt = devp->dts_config.atv_unstable_out_cnt;
	sm_dev[devp->index].hdmi_unstable_out_cnt = devp->dts_config.hdmi_unstable_out_cnt;
	tvin_smr_init_counter(devp->index);
}

enum tvin_sm_status_e tvin_get_sm_status(int index)
{
	return sm_dev[index].state;
}
EXPORT_SYMBOL(tvin_get_sm_status);

int tvin_get_av_status(void)
{
	if (tvin_get_sm_status(0) == TVIN_SM_STATUS_STABLE)
		return true;

	return false;
}
EXPORT_SYMBOL_GPL(tvin_get_av_status);

void vdin_send_event(struct vdin_dev_s *devp, enum tvin_sg_chg_flg sts)
{
	/*pr_info("%s :0x%x\n", __func__, sts);*/
	/*devp->extcon_event->state = sts;*/
	if (devp->dbg_v4l_no_vdin_event == 0)
		schedule_delayed_work(&devp->event_dwork, 0);
}

