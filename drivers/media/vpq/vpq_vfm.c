// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#define DEBUG
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include "vpq_vfm.h"
#include "vpq_printk.h"
#include "vpq_table_logic.h"
#include "vpq_drv.h"

static vpq_vfm_source_type_e cur_src_type;
static vpq_vfm_source_type_e pre_src_type;
static vpq_vfm_trans_fmt_e cur_trans_fmt;
static vpq_vfm_source_mode_e cur_src_mode;
static enum vpq_vfm_color_primary_e cur_color_primaries;
static enum vpq_vfm_scan_mode_e cur_scan_mode;

static unsigned int cur_fps;

static unsigned int cur_sig[VFRAME_SOURCE_TYPE_HWC + 1][SIG_MAX];
static unsigned int pre_sig[VFRAME_SOURCE_TYPE_HWC + 1][SIG_MAX];

#define SIGNAL_TRANSFER_CHARACTERISTIC(type) ((type >> 8) & 0xff)
#define SIGNAL_COLOR_PRIMARIES(type) ((type >> 16) & 0xff)
#define SIGNAL_IS_AMDV(type) ((type >> 30) & 0x01)

void vpq_vfm_init(void)
{
	//vpq_pr_info("start\n");
	memset(pre_sig, 0, sizeof(pre_sig));
	memset(cur_sig, 0, sizeof(cur_sig));
}

static void _vpq_vfm_update_source_type(enum vframe_source_type_e src_type)
{
	cur_src_type = src_type;
}

static void _vpq_vfm_update_signal_format(enum tvin_sig_fmt_e sig_fmt)
{
	cur_sig[cur_src_type][SIG_FMT] = sig_fmt;
}

static void _vpq_vfm_update_trans_format(enum tvin_trans_fmt trans_fmt)
{
	cur_trans_fmt = trans_fmt;
}

//static void _vpq_vfm_update_source_port(enum tvin_port_e src_port)
//{
//	cur_sig[cur_src_type][SIG_PORT] = src_port;
//}

static void _vpq_vfm_update_source_mode(enum vframe_source_mode_e src_mode)
{
	cur_src_mode = src_mode;
}

static void _vpq_vfm_update_height_width(unsigned int type,
	unsigned int compHeight, unsigned int compWidth,
	unsigned int height, unsigned int width)
{
	if (compHeight == 0 || compWidth == 0) {
		cur_sig[cur_src_type][SIG_HEIGHT] = 0;
		cur_sig[cur_src_type][SIG_WIDTH] = 0;
		return;
	}

	cur_sig[cur_src_type][SIG_HEIGHT] = (type & VIDTYPE_COMPRESS) ? compHeight : height;
	cur_sig[cur_src_type][SIG_WIDTH] = (type & VIDTYPE_COMPRESS) ? compWidth : width;
	//vpq_pr_dbg(lev_vfm, "height/width:%d, %d\n",
		//cur_sig[cur_src_type][SIG_HEIGHT], cur_sig[cur_src_type][SIG_WIDTH]);
}

static void _vpq_vfm_update_color_primaries(unsigned int signal_type)
{
	/* bit 23-16: color_primaries
	 * "unknown", "bt709", "undef", "bt601", "bt470m", "bt470bg",
	 * "smpte170m", "smpte240m", "film", "bt2020"
	 */

	unsigned int cp = SIGNAL_COLOR_PRIMARIES(signal_type);

	if (cp == 0x01)
		cur_color_primaries = VPQ_COLOR_PRIM_BT709;
	else if (cp == 0x03)
		cur_color_primaries = VPQ_COLOR_PRIM_BT601;
	else if (cp == 0x09)
		cur_color_primaries = VPQ_COLOR_PRIM_BT2020;
	else
		cur_color_primaries = VPQ_COLOR_PRIM_NULL;

	//vpq_pr_dbg(lev_vfm, "cur_color_primaries:%d\n", cur_color_primaries);
}

static void _vpq_vfm_update_transfer_characteristic(unsigned int signal_type)
{
	/* bit 15-8: transfer_characteristic
	 * "unknown", "bt709", "undef", "bt601", "bt470m", "bt470bg",
	 * "smpte170m", "smpte240m", "linear", "log100", "log316",
	 * "iec61966-2-4", "bt1361e", "iec61966-2-1", "bt2020-10",
	 * "bt2020-12", "smpte-st-2084", "smpte-st-428"
	 */

	enum vpq_vfm_hdr_type_e hdr_type = VPQ_VFM_HDR_TYPE_NONE;
	unsigned int tc = SIGNAL_TRANSFER_CHARACTERISTIC(signal_type);

	if ((tc == 0x0e || tc == 0x12) && cur_color_primaries == VPQ_COLOR_PRIM_BT2020)
		hdr_type = VPQ_VFM_HDR_TYPE_HLG;
	else if (tc == 0x30 && cur_color_primaries == VPQ_COLOR_PRIM_BT2020)
		hdr_type = VPQ_VFM_HDR_TYPE_HDR10PLUS;
	else if (tc == 0x10)
		hdr_type = VPQ_VFM_HDR_TYPE_HDR10;
	else
		hdr_type = VPQ_VFM_HDR_TYPE_SDR;

	//vpq_pr_dbg(lev_vfm, "hdr_type:%d\n", hdr_type);
	cur_sig[cur_src_type][SIG_HDR] = hdr_type;
}

static void _vpq_vfm_update_is_amdv(unsigned int signal_type, struct vframe_s *pvf)
{
	/* 1) vdin path; signal_type; bit 30; 0:not dv 1:dv
	 * 2) decoder + vdin path; is_amdv_frame(); 0:no dv 1:dv std 2:dv ll
	 */

	unsigned int is_amdv = 0;

	if (cur_src_type == VFRAME_SOURCE_TYPE_HDMI)
		is_amdv = SIGNAL_IS_AMDV(signal_type);
	else if (cur_src_type == VFRAME_SOURCE_TYPE_OTHERS)
		is_amdv = is_amdv_frame(pvf);
	else
		is_amdv = 0; // no amdv source

	//vpq_pr_dbg(lev_vfm, "is_amdv:%d\n", is_amdv);
	cur_sig[cur_src_type][SIG_AMDV] = is_amdv;
}

static void _vpq_vfm_update_signal_scan_mode(unsigned int type_original)
{
	if ((type_original & VIDTYPE_TYPEMASK) == VIDTYPE_PROGRESSIVE) {
		cur_scan_mode = VPQ_VFM_SCAN_MODE_PROGRESSIVE;
	} else if ((type_original & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_TOP ||
			   (type_original & VIDTYPE_TYPEMASK) == VIDTYPE_INTERLACE_BOTTOM) {
		cur_scan_mode = VPQ_VFM_SCAN_MODE_INTERLACED;
	} else {
		cur_scan_mode = VPQ_VFM_SCAN_MODE_NULL;
	}

	//vpq_pr_dbg(lev_vfm, "cur_scan_mode:0x%x\n", cur_scan_mode);
}

static void _vpq_vfm_update_fps(unsigned int duration)
{
	/* duration:
	 * 800(120fps) 801(119.88fps) 960(100fps) 1600(60fps) 1920(50fps)
	 * 3200(30fps) 3203(29.97) 3840(25fps) 4000(24fps) 4004(23.976fps)
	 */

	if (duration == 800 || duration == 801)
		cur_fps = 120;
	else if (duration == 960)
		cur_fps = 100;
	else if (duration == 1600)
		cur_fps = 60;
	else if (duration == 1920)
		cur_fps = 50;
	else if (duration == 3200 || duration == 3203)
		cur_fps = 30;
	else if (duration == 3840)
		cur_fps = 25;
	else if (duration == 4000 || duration == 4004)
		cur_fps = 24;

	//vpq_pr_dbg(lev_vfm, "cur_fps:%d\n", cur_fps);
}

//static void _vpq_vfm_update_latency_and_vrr(struct tvin_latency_s latency,
//	enum vdin_vrr_mode_e cur_vrr_status)
//{
//	if (cur_src_type != VFRAME_SOURCE_TYPE_HDMI)
//		return;

//	if ((latency.allm_mode & ALLM_MODE_HDMI_AND_AMDV) ||
//		(latency.it_content && latency.cn_type == GAME) ||
//		(cur_vrr_status != VDIN_VRR_OFF && cur_vrr_status != VDIN_VRR_LATENCY))
//		cur_sig[VFRAME_SOURCE_TYPE_HDMI][SIG_GAME] = 1;
//	else
//		cur_sig[VFRAME_SOURCE_TYPE_HDMI][SIG_GAME] = 0;

//	if (latency.it_content && latency.cn_type != GAME)
//		cur_sig[VFRAME_SOURCE_TYPE_HDMI][SIG_PC] = 1;
//	else
//		cur_sig[VFRAME_SOURCE_TYPE_HDMI][SIG_PC] = 0;
//}

static void print_signal_info(const char *string, int scr_type, int sig_info[])
{
	vpq_pr_dbg(lev_vfm, "%s src:%d, port:0x%x, fmt:0x%x, w-h:%d %d, hdr:%d, amdv:%d, game:%d, pc:%d\n",
		string, scr_type,
		sig_info[SIG_PORT], sig_info[SIG_FMT],  sig_info[SIG_WIDTH], sig_info[SIG_HEIGHT],
		sig_info[SIG_HDR],  sig_info[SIG_AMDV], sig_info[SIG_GAME],  sig_info[SIG_PC]);
}

static void printf_signal_info(void)
{
	print_signal_info("current", cur_src_type, cur_sig[cur_src_type]);
	print_signal_info("previous", pre_src_type, pre_sig[cur_src_type]);
}

static void _vpq_vfm_update_pq_effect(void)
{
	if (cur_src_type == VFRAME_SOURCE_TYPE_HDMI) {
		/* switch src to hdmi case
		 * switch sig fmt case
		 * switch port case
		 * switch hdr type case
		 * switch dv flag case
		 * switch game/pc case
		 */
		if (cur_src_type != pre_src_type ||
			cur_sig[cur_src_type][SIG_FMT]  != pre_sig[cur_src_type][SIG_FMT]  ||
			cur_sig[cur_src_type][SIG_PORT] != pre_sig[cur_src_type][SIG_PORT] ||
			cur_sig[cur_src_type][SIG_HDR]  != pre_sig[cur_src_type][SIG_HDR]  ||
			cur_sig[cur_src_type][SIG_AMDV] != pre_sig[cur_src_type][SIG_AMDV] ||
			cur_sig[cur_src_type][SIG_GAME] != pre_sig[cur_src_type][SIG_GAME] ||
			cur_sig[cur_src_type][SIG_PC]   != pre_sig[cur_src_type][SIG_PC]) {
			printf_signal_info();

			if (cur_sig[cur_src_type][SIG_HEIGHT] != 0 &&
				cur_sig[cur_src_type][SIG_WIDTH]  != 0) {
				vpq_pr_info("trigger hdmi pq update and event\n");
				vpq_set_pq_effect();
				vpq_vfm_send_event(VPQ_VFM_EVENT_SIG_INFO_CHANGE);
			}
		}
	} else if (cur_src_type == VFRAME_SOURCE_TYPE_CVBS) {
		/* switch src to av case
		 * switch sig fmt case
		 */
		if (pre_src_type != VFRAME_SOURCE_TYPE_CVBS ||
			cur_sig[cur_src_type][SIG_FMT] != pre_sig[cur_src_type][SIG_FMT]) {
			printf_signal_info();
			vpq_set_pq_effect();
		}
	} else if (cur_src_type == VFRAME_SOURCE_TYPE_OTHERS) {
		/* switch src to mpeg case
		 * switch sig resolution case
		 * switch hdr type case
		 * switch dv flag case
		 */
		if (cur_src_type != pre_src_type ||
			cur_sig[cur_src_type][SIG_HEIGHT] != pre_sig[cur_src_type][SIG_HEIGHT] ||
			cur_sig[cur_src_type][SIG_WIDTH]  != pre_sig[cur_src_type][SIG_WIDTH]  ||
			cur_sig[cur_src_type][SIG_HDR]    != pre_sig[cur_src_type][SIG_HDR]    ||
			cur_sig[cur_src_type][SIG_AMDV]   != pre_sig[cur_src_type][SIG_AMDV]) {
			printf_signal_info();

			if (cur_sig[cur_src_type][SIG_HEIGHT] != 0 &&
				cur_sig[cur_src_type][SIG_WIDTH]  != 0) {
				vpq_pr_info("trigger mpeg pq update and event\n");
				vpq_set_pq_effect();
				vpq_vfm_send_event(VPQ_VFM_EVENT_SIG_INFO_CHANGE);
			}
		}
	}

	pre_src_type = cur_src_type;
	pre_sig[cur_src_type][SIG_FMT]    = cur_sig[cur_src_type][SIG_FMT];
	pre_sig[cur_src_type][SIG_PORT]   = cur_sig[cur_src_type][SIG_PORT];
	pre_sig[cur_src_type][SIG_HEIGHT] = cur_sig[cur_src_type][SIG_HEIGHT];
	pre_sig[cur_src_type][SIG_WIDTH]  = cur_sig[cur_src_type][SIG_WIDTH];
	pre_sig[cur_src_type][SIG_HDR]    = cur_sig[cur_src_type][SIG_HDR];
	pre_sig[cur_src_type][SIG_AMDV]   = cur_sig[cur_src_type][SIG_AMDV];
	pre_sig[cur_src_type][SIG_GAME]   = cur_sig[cur_src_type][SIG_GAME];
	pre_sig[cur_src_type][SIG_PC]     = cur_sig[cur_src_type][SIG_PC];
}

void vpq_vfm_process(struct vframe_s *pvf)
{
	if (!pvf) {
		vpq_pr_dbg(lev_vfm, "pvf is null\n");
		return;
	}

	// update local signal information
	_vpq_vfm_update_source_type(pvf->source_type);
	_vpq_vfm_update_signal_format(pvf->sig_fmt);
	_vpq_vfm_update_trans_format(pvf->trans_fmt);
	//_vpq_vfm_update_source_port(pvf->port);
	_vpq_vfm_update_source_mode(pvf->source_mode);
	_vpq_vfm_update_color_primaries(pvf->signal_type);
	_vpq_vfm_update_transfer_characteristic(pvf->signal_type);
	_vpq_vfm_update_is_amdv(pvf->signal_type, pvf);
	_vpq_vfm_update_signal_scan_mode(pvf->type_original);
	_vpq_vfm_update_height_width(pvf->type, pvf->compHeight, pvf->compWidth,
		pvf->height, pvf->width);
	_vpq_vfm_update_fps(pvf->duration);
	//_vpq_vfm_update_latency_and_vrr(pvf->latency, pvf->cur_vrr_status);

	//vpq_pr_dbg(lev_vfm,
	//	"source_type:%d,   port:0x%x,        sig_fmt:0x%x,\n"
	//	"trans_fmt:%d,     source_mode:%d,   type:%d,\n"
	//	"compHeight:%d,    compWidth:%d,     height:%d,\n"
	//	"width:%d,         signal_type:0x%x, type_original:%d,\n"
	//	"duration:%d,      allm_mode:%d,     cn_type:%d,\n"
	//	"fmm_flag:%d,      it_content:%d,    cur_vrr_status:%d,\n",
	//	pvf->source_type,      pvf->sig_fmt,            pvf->port,
	//	pvf->trans_fmt,        pvf->source_mode,        pvf->type,
	//	pvf->compHeight,       pvf->compWidth,          pvf->height,
	//	pvf->width,            pvf->signal_type,        pvf->type_original,
	//	pvf->duration,         pvf->latency.allm_mode,  pvf->latency.cn_type,
	//	pvf->latency.fmm_flag, pvf->latency.it_content, pvf->cur_vrr_status);

	// trigger pq update when vf changed
	_vpq_vfm_update_pq_effect();
}
EXPORT_SYMBOL(vpq_vfm_process);

void vpq_vfm_video_enable(int enable)
{
	vpq_pr_info("%d\n", enable);
	if (enable == 0) {
		pre_src_type = VFRAME_SOURCE_TYPE_HWC;
		memset(pre_sig, 0, sizeof(pre_sig));
		memset(cur_sig, 0, sizeof(cur_sig));
	}
}
EXPORT_SYMBOL(vpq_vfm_video_enable);

/* private api for other files to get current signal information */
vpq_vfm_source_type_e vpq_vfm_get_source_type(void)
{
	return cur_src_type;
}

vpq_vfm_sig_fmt_e vpq_vfm_get_signal_format(void)
{
	return cur_sig[cur_src_type][SIG_FMT];
}

vpq_vfm_trans_fmt_e vpq_vfm_get_trans_format(void)
{
	return cur_trans_fmt;
}

vpq_vfm_port_e vpq_vfm_get_source_port(void)
{
	return cur_sig[cur_src_type][SIG_PORT];
}

vpq_vfm_source_mode_e vpq_vfm_get_source_mode(void)
{
	return cur_src_mode;
}

enum vpq_vfm_color_primary_e vpq_vfm_get_color_primaries(void)
{
	return cur_color_primaries;
}

enum vpq_vfm_hdr_type_e vpq_vfm_get_hdr_type(void)
{
	return cur_sig[cur_src_type][SIG_HDR];

	/* Note:
	 * or whether can directly use vframe.h below api to replace cur_hdr_type?
	 * enum vframe_signal_fmt_e signal_fmt = get_vframe_src_fmt(pvf);
	 */
}

void vpq_vfm_get_is_amdv(unsigned int *is_amdv)
{
	*is_amdv = cur_sig[cur_src_type][SIG_AMDV];
}

void vpq_vfm_get_is_game(unsigned int *is_game)
{
	*is_game = cur_sig[VFRAME_SOURCE_TYPE_HDMI][SIG_GAME];
}

void vpq_vfm_get_is_pc(unsigned int *is_pc)
{
	*is_pc = cur_sig[VFRAME_SOURCE_TYPE_HDMI][SIG_PC];
}

void vpq_vfm_get_height_width(unsigned int *height, unsigned int *width)
{
	*height = cur_sig[cur_src_type][SIG_HEIGHT];
	*width = cur_sig[cur_src_type][SIG_WIDTH];
}

enum vpq_vfm_scan_mode_e vpq_vfm_get_signal_scan_mode(void)
{
	return cur_scan_mode;
}

void vpq_frm_get_fps(unsigned int *fps)
{
	*fps = cur_fps;
}

void vpq_vfm_send_event(enum vpq_vfm_event_info_e event_info)
{
	vpq_pr_dbg(lev_vfm, "start event_info:%d\n", event_info);

	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	vpq_devp->event_info = (unsigned int)event_info;

	schedule_delayed_work(&vpq_devp->event_dwork, 0);

	wake_up(&vpq_devp->queue);
}

enum vpq_vfm_event_info_e vpq_vfm_get_event_info(void)
{
	struct vpq_dev_s *vpq_devp = get_vpq_dev();

	vpq_pr_dbg(lev_vfm, "vpq_devp->event_info:%d\n", vpq_devp->event_info);

	return (enum vpq_vfm_event_info_e)vpq_devp->event_info;
}
