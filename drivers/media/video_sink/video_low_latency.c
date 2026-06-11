// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <uapi/linux/sched/types.h>

#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#include <linux/amlogic/media/video_sink/video_keeper.h>
#include "video_priv.h"
#include "video_reg.h"
#include <linux/amlogic/media/utils/vdec_reg.h>
#include <linux/amlogic/media/registers/register.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/utils/amports_config.h>
#include <linux/amlogic/media/vpu/vpu.h>
#ifdef AMLOGIC_MEDIA_DPSS
#include <linux/amlogic/media/dpss/dpss_frc.h>
#include <linux/amlogic/media/di/dpss_interface.h>
#endif
#include "videolog.h"
#include "video_reg.h"
#ifdef CONFIG_AM_VIDEO_LOG
#define AMLOG
#endif
#include <linux/amlogic/media/utils/amlog.h>
MODULE_AMLOG(LOG_LEVEL_ERROR, 0, LOG_DEFAULT_LEVEL_DESC, LOG_MASK_DESC);

#include <linux/amlogic/media/video_sink/vpp.h>
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
#include "../common/rdma/rdma.h"
#endif
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/codec_mm/configs.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>

#include "../common/vfm/vfm.h"
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_TVIN
#include "linux/amlogic/media/frame_provider/tvin/tvin_v4l2.h"
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
#include <linux/amlogic/media/amprime_sl/prime_sl.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_MSYNC
#include <uapi/amlogic/msync.h>
#endif
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/media/video_processor/video_pp_common.h>

#include <linux/math64.h>
#include "video_receiver.h"
#include "video_low_latency.h"
#include "video_common.h"

static u32 lowlatency_proc_drop;
static u32 lowlatency_err_drop;
static u32 lowlatency_proc_frame_cnt;
static u32 lowlatency_skip_frame_cnt;
static u32 lowlatency_proc_done;
static u32 lowlatency_overflow_cnt;
static u32 lowlatency_overrun_cnt;
u32 lowlatency_overrun_recovery_cnt;
static int lowlatency_max_proc_lines;
static u32 lowlatency_max_enter_lines;
static u32 lowlatency_max_exit_lines;
static u32 lowlatency_min_enter_lines = 0xffffffff;
static u32 lowlatency_min_exit_lines = 0xffffffff;
static u32 lowlatency_rdma_proc_drop;
static u32 lowlatency_rdma_err_drop;
static u32 lowlatency_rdma_err2_drop;

static u32 vsync_proc_done;
u32 frame_line_threshold = 10; /* n/100 */
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
static bool first_irq = true;
#endif
/* vout */
static const struct vinfo_s *vinfo;

/* low latency version
 * 0: low latency is disabled
 * 1: version1
 *    in the vc thread, the proc_lowlatency_frame function is called to achieve low latency,
 *    and it only supports single-stream video(VD1) scenarios.
 * 2: version2.
 *    in the thread triggered by the line-n interrupt,
 *    frame parameters are processed and registers are set.
 *    it is a global low-latency mode, supporting multi-stream video display.
 */
static unsigned int low_latency_ver;
static u32 lowlatency_enable;

u32 rdma_check_min_line, rdma_check_max_line;
u32 rdma_line_threshold = 7; /* n/1000 */
u32 rdma_add_delay_ms;
u32 only_vpp_wake;

static DEFINE_SPINLOCK(video_llm_lock);

void get_low_latency_info(u32 *enable, u32 *version)
{
	if (!enable || !version) {
		pr_info("%s, wrong params, %p %p\n", __func__, enable, version);
		return;
	}
	*enable = lowlatency_enable;
	*version = low_latency_ver;
}

void set_low_latency_info(u32 enable, u32 version)
{
	lowlatency_enable = enable;
	low_latency_ver = version;
}

int get_low_latency_version(void)
{
	return lowlatency_enable ? low_latency_ver : 0;
}

void get_low_latency_params(u32 *p0, u32 *p1, u32 *p2, u32 *p3, u32 *p4, u32 *p5)
{
	if (p0 && p1 && p2 && p3 && p4 && p5) {
		*p0 = frame_line_threshold;
		*p1 = rdma_line_threshold;
		*p2 = rdma_check_min_line;
		*p3 = rdma_check_max_line;
		*p4 = rdma_add_delay_ms;
		*p5 = only_vpp_wake;
	}
}

void set_low_latency_params(u32 p0, u32 p1, u32 p2, u32 p3, u32 p4, u32 p5)
{
	frame_line_threshold = p0;
	rdma_line_threshold = p1;
	rdma_check_min_line = p2;
	rdma_check_max_line = p3;
	rdma_add_delay_ms = p4;
	only_vpp_wake = p5;
}

u32 vsync_proc_drop;
ulong lowlatency_vsync_count;
bool overrun_flag;

static int lowlatency_vsync(u8 instance_id)
{
	s32 vout_type;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	struct vframe_s *vf = NULL;
#endif
	struct vframe_s *path0_new_frame = NULL;
	struct vframe_s *path1_new_frame = NULL;
	struct vframe_s *path2_new_frame = NULL;
	struct vframe_s *path3_new_frame = NULL;
	struct vframe_s *path4_new_frame = NULL;
	struct vframe_s *path5_new_frame = NULL;

	static s32 cur_vd1_path_id = VFM_PATH_INVALID;
	static s32 cur_vd2_path_id = VFM_PATH_INVALID;
	static s32 cur_vd3_path_id = VFM_PATH_INVALID;
	s32 vd1_path_id = glayer_info[0].display_path_id;
	s32 vd2_path_id = glayer_info[1].display_path_id;
	s32 vd3_path_id = glayer_info[2].display_path_id;
	struct vframe_s *new_frame = NULL;
	struct vframe_s *new_frame2 = NULL;
	struct vframe_s *new_frame3 = NULL;
	u32 cur_blackout;
	enum vframe_signal_fmt_e fmt;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int pq_process_debug[4];
#endif
	int axis[4];
	int crop[4];
	int crop_save[4];
	int source_type = 0;
	u32 next_afbc_request = atomic_read(&gafbc_request);
	struct path_id_s path_id;
	int i;
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	u16 line = glayer_info[0].layer_top;
	struct vpq_size_s vpq_size = {0};
#endif

	vinfo = get_current_vinfo();
#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	set_vsync_rdma_id(EX_VSYNC_RDMA);
#endif

	if (debug_flag & DEBUG_FLAG_VSYNC_DONONE)
		return IRQ_HANDLED;
	path_id.vd1_path_id = vd1_path_id;
	path_id.vd2_path_id = vd2_path_id;
	path_id.vd3_path_id = vd3_path_id;

#ifdef CONFIG_AMLOGIC_MEDIA_MSYNC
	msync_vsync_update();
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_on())
		amdv_update_backlight();
#endif

	if (cur_vd1_path_id == 0xff)
		cur_vd1_path_id = vd1_path_id;
	if (cur_vd2_path_id == 0xff)
		cur_vd2_path_id = vd2_path_id;
	if (cur_vd3_path_id == 0xff)
		cur_vd3_path_id = vd3_path_id;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	/* Just a workaround to enable RDMA without any register config.
	 * Because rdma is enabled after first rdma config.
	 * Previously, it will write register directly and
	 * maybe out of blanking in first irq.
	 */
	if (first_irq) {
		first_irq = false;
		goto RUN_FIRST_RDMA;
	}
#endif

	vout_type = detect_vout_type(vinfo);

	for (i = 0; i < cur_dev->max_vd_layers; i++) {
		glayer_info[0].need_no_compress =
			(next_afbc_request & (i + 1)) ? true : false;
		vd_layer[i].bypass_pps = bypass_pps == 1 ? true : false;
		vd_layer[i].global_debug = debug_flag;
		vd_layer[i].vout_type = vout_type;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	vsync_rdma_config_pre();
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	/* check video frame before VECM process */
	if (is_amdv_enable() && vf &&
		(vd1_path_id == VFM_PATH_AMVIDEO ||
		 vd1_path_id == VFM_PATH_DEF ||
		 vd1_path_id == VFM_PATH_AUTO))
		amdv_check_format(vf);

	if (cur_vd1_path_id != vd1_path_id) {
		char *provider_name = NULL;

		/* FIXME: add more receiver check */
		if (vd1_path_id == VFM_PATH_PIP) {
			provider_name = vf_get_provider_name("videopip");
			while (provider_name) {
				if (!vf_get_provider_name(provider_name))
					break;
				provider_name =
					vf_get_provider_name(provider_name);
			}
			if (provider_name)
				amdv_set_provider(provider_name, VD2_PATH);
		} else {
			amdv_set_provider("dvbldec", VD1_PATH);
		}
	}
#endif

	if (atomic_read(&video_unreg_flag))
		goto exit;

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
	if (is_vsync_rdma_enable() || cur_dev->latch_enable) {
		vd_layer[0].cur_canvas_id = vd_layer[0].next_canvas_id;
		vd_layer[1].cur_canvas_id = vd_layer[1].next_canvas_id;
		vd_layer[2].cur_canvas_id = vd_layer[2].next_canvas_id;
	} else {
		if (rdma_enable_pre)
			goto exit;

		vd_layer[0].cur_canvas_id = 0;
		vd_layer[0].next_canvas_id = 1;
		vd_layer[1].cur_canvas_id = 0;
		vd_layer[1].next_canvas_id = 1;
		vd_layer[2].cur_canvas_id = 0;
		vd_layer[2].next_canvas_id = 1;
	}
#endif
	if (!get_lowlatency_mode()) {
		if (gvideo_recv[0]) {
			gvideo_recv[0]->irq_mode = false;
			gvideo_recv[0]->func->early_proc(gvideo_recv[0],
							 over_field ? 1 : 0);
		}
		if (gvideo_recv[1]) {
			gvideo_recv[1]->irq_mode = false;
			gvideo_recv[1]->func->early_proc(gvideo_recv[1],
							 over_field ? 1 : 0);
		}
		if (gvideo_recv[2]) {
			gvideo_recv[2]->irq_mode = false;
			gvideo_recv[2]->func->early_proc(gvideo_recv[2],
							 over_field ? 1 : 0);
		}
	}

	/* video_render.0 toggle frame */
	if (gvideo_recv[0]) {
		u32 frame_cnt = 0;
		struct vframe_s *tmp_vf = NULL;

		do {
			tmp_vf =
				gvideo_recv[0]->func->dequeue_frame(gvideo_recv[0], &path_id);
			if (tmp_vf) {
				lowlatency_proc_frame_cnt++;
				if (path3_new_frame)
					frame_cnt++;
				path3_new_frame = tmp_vf;
			}
		} while (tmp_vf);

		lowlatency_skip_frame_cnt += frame_cnt;
		if (path3_new_frame &&
			tvin_vf_is_keeped(path3_new_frame)) {
			new_frame_count = 0;
		} else if (path3_new_frame) {
			new_frame_count = gvideo_recv[0]->frame_count;
			hdmi_in_delay_maxmin_new(path3_new_frame);
		} else if (gvideo_recv[0]->cur_buf) {
			if (tvin_vf_is_keeped(gvideo_recv[0]->cur_buf))
				new_frame_count = 0;
		}
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
		if (vd1_path_id == VFM_PATH_VIDEO_RENDER0 &&
			cur_frame_par[0]) {
			/*need call every vsync*/
			if (path3_new_frame)
				frame_lock_process(path3_new_frame,
					cur_frame_par[0], line);
			else if (vd_layer[0].dispbuf)
				frame_lock_process(vd_layer[0].dispbuf,
					cur_frame_par[0], line);
			else
				frame_lock_process(NULL, cur_frame_par[0], line);
		}

		if (vd1_path_id == gvideo_recv[0]->path_id) {
			amvecm_on_vs((gvideo_recv[0]->cur_buf !=
				 &gvideo_recv[0]->local_buf)
				? gvideo_recv[0]->cur_buf : NULL,
				path3_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				vpq_size,
				VD1_PATH,
				VPP_TOP0);
		}
		if (vd2_path_id == gvideo_recv[0]->path_id)
			amvecm_on_vs((gvideo_recv[0]->cur_buf !=
				 &gvideo_recv[0]->local_buf)
				? gvideo_recv[0]->cur_buf : NULL,
				path3_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				vpq_size,
				VD2_PATH,
				VPP_TOP0);
		if (vd3_path_id == gvideo_recv[0]->path_id)
			amvecm_on_vs((gvideo_recv[0]->cur_buf !=
				 &gvideo_recv[0]->local_buf)
				? gvideo_recv[0]->cur_buf : NULL,
				path3_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				vpq_size,
				VD3_PATH,
				VPP_TOP0);
#endif
	}

	/* video_render.1 toggle frame */
	if (gvideo_recv[1]) {
		struct vframe_s *tmp_vf = NULL;

		do {
			tmp_vf =
				gvideo_recv[1]->func->dequeue_frame(gvideo_recv[1],
					&path_id);
			if (tmp_vf)
				path4_new_frame = tmp_vf;
		} while (tmp_vf);

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
		if (vd1_path_id == gvideo_recv[1]->path_id) {
			amvecm_on_vs((gvideo_recv[1]->cur_buf !=
				 &gvideo_recv[1]->local_buf)
				? gvideo_recv[1]->cur_buf : NULL,
				path4_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				vpq_size,
				VD1_PATH,
				VPP_TOP0);
		}
		if (vd2_path_id == gvideo_recv[1]->path_id)
			amvecm_on_vs((gvideo_recv[1]->cur_buf !=
				 &gvideo_recv[1]->local_buf)
				? gvideo_recv[1]->cur_buf : NULL,
				path4_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				vpq_size,
				VD2_PATH,
				VPP_TOP0);
		if (vd3_path_id == gvideo_recv[1]->path_id)
			amvecm_on_vs((gvideo_recv[1]->cur_buf !=
				 &gvideo_recv[1]->local_buf)
				? gvideo_recv[1]->cur_buf : NULL,
				path4_new_frame,
				CSC_FLAG_CHECK_OUTPUT,
				vpq_size,
				VD3_PATH,
				VPP_TOP0);
#endif
	}

	/* video_render.2 toggle frame */
	if (gvideo_recv[2]) {
		struct vframe_s *tmp_vf = NULL;

		do {
			tmp_vf =
				gvideo_recv[2]->func->dequeue_frame(gvideo_recv[2],
					&path_id);
			if (tmp_vf)
				path5_new_frame = tmp_vf;
		} while (tmp_vf);

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	if (vd1_path_id == gvideo_recv[2]->path_id) {
		amvecm_on_vs((gvideo_recv[2]->cur_buf !=
			 &gvideo_recv[2]->local_buf)
			? gvideo_recv[2]->cur_buf : NULL,
			path5_new_frame,
			CSC_FLAG_CHECK_OUTPUT,
			vpq_size,
			VD1_PATH,
			VPP_TOP0);
	}
	if (vd2_path_id == gvideo_recv[2]->path_id)
		amvecm_on_vs((gvideo_recv[2]->cur_buf !=
			 &gvideo_recv[2]->local_buf)
			? gvideo_recv[2]->cur_buf : NULL,
			path5_new_frame,
			CSC_FLAG_CHECK_OUTPUT,
			vpq_size,
			VD2_PATH,
			VPP_TOP0);
	if (vd3_path_id == gvideo_recv[2]->path_id)
		amvecm_on_vs((gvideo_recv[2]->cur_buf !=
			 &gvideo_recv[2]->local_buf)
			? gvideo_recv[2]->cur_buf : NULL,
			path5_new_frame,
			CSC_FLAG_CHECK_OUTPUT,
			vpq_size,
			VD3_PATH,
			VPP_TOP0);
#endif
	}
	/* FIXME: if need enable for vd1 */
#ifdef CHECK_LATER
	if (!vd_layer[0].global_output) {
		cur_vd1_path_id = VFM_PATH_INVALID;
		vd1_path_id = VFM_PATH_INVALID;
	}
#endif

	if (!vd_layer[1].global_output) {
		cur_vd2_path_id = VFM_PATH_INVALID;
		vd2_path_id = VFM_PATH_INVALID;
	}
	if (!vd_layer[2].global_output) {
		cur_vd3_path_id = VFM_PATH_INVALID;
		vd3_path_id = VFM_PATH_INVALID;
	}

	if ((cur_vd1_path_id != vd1_path_id ||
	     cur_vd2_path_id != vd2_path_id ||
	     cur_vd3_path_id != vd3_path_id) &&
	    (debug_flag & DEBUG_FLAG_PRINT_PATH_SWITCH)) {
		pr_info("VID: === before path switch ===\n");
		pr_info("VID: \tcur_path_id: %d, %d, %d;\nVID: \tnew_path_id: %d, %d, %d;\nVID: \ttoggle:%p, %p, %p %p, %p, %p\nVID: \tcur:%p, %p, %p, %p, %p, %p;\n",
			cur_vd1_path_id, cur_vd2_path_id, cur_vd3_path_id,
			vd1_path_id, vd2_path_id, vd3_path_id,
			path0_new_frame, path1_new_frame,
			path2_new_frame, path3_new_frame,
			path4_new_frame, path4_new_frame,
			cur_dispbuf[0], cur_dispbuf[1], cur_dispbuf[2],
			gvideo_recv[0] ? gvideo_recv[0]->cur_buf : NULL,
			gvideo_recv[1] ? gvideo_recv[1]->cur_buf : NULL,
			gvideo_recv[2] ? gvideo_recv[2]->cur_buf : NULL);
		pr_info("VID: \tdispbuf:%p, %p, %p; \tvf_ext:%p, %p, %p;\nVID: \tlocal:%p, %p, %p, %p, %p, %p\n",
			vd_layer[0].dispbuf, vd_layer[1].dispbuf, vd_layer[2].dispbuf,
			vd_layer[0].vf_ext, vd_layer[1].vf_ext, vd_layer[2].vf_ext,
			&vf_local[0], &vf_local[1], &vf_local[2],
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL,
			gvideo_recv[2] ? &gvideo_recv[2]->local_buf : NULL);
		pr_info("VID: \tblackout:%d %d, %d force:%d;\n",
			blackout[0], blackout[1], blackout[2], force_blackout);
	}
	if (debug_flag & DEBUG_FLAG_PRINT_DISBUF_PER_VSYNC)
		pr_info("VID(%s): path id: %d, %d, %d; new_frame:%p, %p, %p, %p, %p, %p cur:%p, %p, %p, %p, %p, %p; vd dispbuf:%p, %p, %p; vf_ext:%p, %p, %p; local:%p, %p, %p, %p, %p, %p\n",
			__func__,
			vd1_path_id, vd2_path_id, vd2_path_id,
			path0_new_frame, path1_new_frame,
			path2_new_frame, path3_new_frame,
			path4_new_frame, path5_new_frame,
			cur_dispbuf[0], cur_dispbuf[1], cur_dispbuf[2],
			gvideo_recv[0] ? gvideo_recv[0]->cur_buf : NULL,
			gvideo_recv[1] ? gvideo_recv[1]->cur_buf : NULL,
			gvideo_recv[2] ? gvideo_recv[2]->cur_buf : NULL,
			vd_layer[0].dispbuf, vd_layer[1].dispbuf, vd_layer[2].dispbuf,
			vd_layer[0].vf_ext, vd_layer[1].vf_ext, vd_layer[2].vf_ext,
			&vf_local[0], &vf_local[1], &vf_local[2],
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL,
			gvideo_recv[2] ? &gvideo_recv[2]->local_buf : NULL);

	if (vd_layer[0].dispbuf_mapping == &cur_dispbuf[0] &&
	    (cur_dispbuf[0] == &vf_local[0] ||
	     !cur_dispbuf[0]) &&
	    vd_layer[0].dispbuf != cur_dispbuf[0])
		vd_layer[0].dispbuf = cur_dispbuf[0];

	if (vd_layer[0].dispbuf_mapping == &cur_dispbuf[1] &&
	    (cur_dispbuf[1] == &vf_local[1] ||
	     !cur_dispbuf[1]) &&
	    vd_layer[0].dispbuf != cur_dispbuf[1])
		vd_layer[0].dispbuf = cur_dispbuf[1];

	if (vd_layer[0].dispbuf_mapping == &cur_dispbuf[2] &&
	    (cur_dispbuf[2] == &vf_local[2] ||
	     !cur_dispbuf[2]) &&
	    vd_layer[0].dispbuf != cur_dispbuf[2])
		vd_layer[0].dispbuf = cur_dispbuf[2];

	if (gvideo_recv[0] &&
	    vd_layer[0].dispbuf_mapping == &gvideo_recv[0]->cur_buf &&
	    (gvideo_recv[0]->cur_buf == &gvideo_recv[0]->local_buf ||
	     !gvideo_recv[0]->cur_buf) &&
	    vd_layer[0].dispbuf != gvideo_recv[0]->cur_buf)
		vd_layer[0].dispbuf = gvideo_recv[0]->cur_buf;

	if (gvideo_recv[1] &&
	    vd_layer[0].dispbuf_mapping == &gvideo_recv[1]->cur_buf &&
	    (gvideo_recv[1]->cur_buf == &gvideo_recv[1]->local_buf ||
	     !gvideo_recv[1]->cur_buf) &&
	    vd_layer[0].dispbuf != gvideo_recv[1]->cur_buf)
		vd_layer[0].dispbuf = gvideo_recv[1]->cur_buf;

	if (gvideo_recv[2] &&
	    vd_layer[0].dispbuf_mapping == &gvideo_recv[2]->cur_buf &&
	    (gvideo_recv[2]->cur_buf == &gvideo_recv[2]->local_buf ||
	     !gvideo_recv[2]->cur_buf) &&
	    vd_layer[0].dispbuf != gvideo_recv[2]->cur_buf)
		vd_layer[0].dispbuf = gvideo_recv[2]->cur_buf;

	if (vd_layer[0].switch_vf &&
	    vd_layer[0].dispbuf &&
	    (vd_layer[0].dispbuf->vf_ext ||
	     vd_layer[0].dispbuf->uvm_vf)) {
		/* select uvm_vf first */
		if (vd_layer[0].dispbuf->uvm_vf)
			vd_layer[0].vf_ext =
				vd_layer[0].dispbuf->uvm_vf;
		else
			vd_layer[0].vf_ext =
				(struct vframe_s *)vd_layer[0].dispbuf->vf_ext;
	} else {
		vd_layer[0].vf_ext = NULL;
	}
	/* vd1 config */
	if (gvideo_recv[0] &&
	    gvideo_recv[0]->path_id == vd1_path_id) {
		/* video_render.0 display on VD1 */
		new_frame = path3_new_frame;
		if (!new_frame) {
			if (!gvideo_recv[0]->cur_buf) {
				/* video_render.0 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (gvideo_recv[0]->cur_buf ==
				&gvideo_recv[0]->local_buf) {
				/* video_render.0 keep frame */
				vd_layer[0].dispbuf = gvideo_recv[0]->cur_buf;
			} else if (vd_layer[0].dispbuf
				!= gvideo_recv[0]->cur_buf) {
				/* video_render.0 has frame in display */
				new_frame = gvideo_recv[0]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv[0]->cur_buf) {
			vd_layer[0].dispbuf_mapping = &gvideo_recv[0]->cur_buf;
			if ((gvideo_recv[0]->cur_buf->type_ext & VIDTYPE_EXT_LCEVC) &&
				gvideo_recv[0]->cur_buf->enhance_vf) {
				u32 src_width = 3840, src_height = 2160;
				struct vframe_s *vf = NULL;
				int coef0[4], coef1[4];

				video_lcevc.vd2_vd1_shared_vf = true;
				video_lcevc.preblend_en = true;
				video_lcevc.enhance_vf =
				gvideo_recv[0]->cur_buf->enhance_vf;
				vf = gvideo_recv[0]->cur_buf;
				if (vf->type & VIDTYPE_COMPRESS) {
					src_width = vf->compWidth;
					src_height = vf->compHeight;
				} else {
					src_width = vf->width;
					src_height = vf->height;
				}
				coef0[0] = (vf->scaler_coeff.k[0][0] - 127) / 128;
				coef0[1] = (vf->scaler_coeff.k[0][1] + 127) / 128;
				coef0[2] = (vf->scaler_coeff.k[0][2] + 127) / 128;
				coef0[3] = (vf->scaler_coeff.k[0][3] + 127) / 128;

				coef1[0] = (vf->scaler_coeff.k[1][0] + 127) / 128;
				coef1[1] = (vf->scaler_coeff.k[1][1] + 127) / 128;
				coef1[2] = (vf->scaler_coeff.k[1][2] + 127) / 128;
				coef1[3] = (vf->scaler_coeff.k[1][3] - 127) / 128;

				video_lcevc.vf_lcevc_coeff0 =
					(unsigned int)(coef0[0] & 0xff) << 24 |
					(unsigned int)(coef0[1] & 0xff) << 16 |
					(unsigned int)(coef0[2] & 0xff) << 8 |
					(unsigned int)(coef0[3] & 0xff);
				video_lcevc.vf_lcevc_coeff1 =
					(unsigned int)(coef1[0] & 0xff) << 24 |
					(unsigned int)(coef1[1] & 0xff) << 16 |
					(unsigned int)(coef1[2] & 0xff) << 8 |
					(unsigned int)(coef1[3] & 0xff);
				if (vf && (debug_common_flag & DEBUG_FLAG_COMMON_LCEVC))
					pr_info("%s,coef:[0]%d,%d,%d,%d, [1]%d,%d,%d,%d,coef0:%d,%d,%d,%d, coef1:%d,%d,%d,%d, trans:0x%x, 0x%x\n",
						__func__,
						vf->scaler_coeff.k[0][0],
						vf->scaler_coeff.k[0][1],
						vf->scaler_coeff.k[0][2],
						vf->scaler_coeff.k[0][3],
						vf->scaler_coeff.k[1][0],
						vf->scaler_coeff.k[1][1],
						vf->scaler_coeff.k[1][2],
						vf->scaler_coeff.k[1][3],
						coef0[0], coef0[1], coef0[2], coef0[3],
						coef1[0], coef1[1], coef1[2], coef1[3],
						video_lcevc.vf_lcevc_coeff0,
						video_lcevc.vf_lcevc_coeff1);
				if (debug_common_flag & DEBUG_FLAG_COMMON_LCEVC)
					pr_info("%s, vd2_vd1_shared_vf=%d,enhance_vf=0x%p, gvideo_recv[0]->cur_buf=0x%p, vd1 src_width=%d, src_height=%d\n",
						__func__,
						video_lcevc.vd2_vd1_shared_vf,
						gvideo_recv[0]->cur_buf->enhance_vf,
						gvideo_recv[0]->cur_buf,
						src_width, src_height);
				video_lcevc.vd1_src_width = src_width;
				video_lcevc.vd1_src_height = src_height;
				video_lcevc.vd1_type = vf->type;
				glayer_info[1].display_path_id =
					glayer_info[0].display_path_id;
				vd2_path_id = glayer_info[0].display_path_id;
				vd_layer[1].layer_alpha = video_lcevc.alpha;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
				amve_lc_evc_ctrl(lcevc_en, true);
#endif
				hscaler_8tap_enable[0] = false;
				hscaler_8tap_enable[1] = false;
			} else {
				switch_from_lcevc_to_nonlcevc(false);
				if (video_lcevc.lcevc_switch_normal) {
					vd2_path_id = glayer_info[1].display_path_id;
					if (debug_common_flag & DEBUG_FLAG_COMMON_LCEVC)
						pr_info("%s, line=%d, lcevc_switch_normal vd2_path_id=%d, vd_layer[1].global_output=%d\n",
							__func__, __LINE__,
							vd2_path_id, vd_layer[1].global_output);
				}
			}
		}
		cur_blackout = 1;
	} else if (gvideo_recv[1] &&
	    (gvideo_recv[1]->path_id == vd1_path_id)) {
		/* video_render.1 display on VD1 */
		new_frame = path4_new_frame;
		if (!new_frame) {
			if (!gvideo_recv[1]->cur_buf) {
				/* video_render.1 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (gvideo_recv[1]->cur_buf ==
				&gvideo_recv[1]->local_buf) {
				/* video_render.1 keep frame */
				vd_layer[0].dispbuf = gvideo_recv[1]->cur_buf;
			} else if (vd_layer[0].dispbuf
				!= gvideo_recv[1]->cur_buf) {
				/* video_render.1 has frame in display */
				new_frame = gvideo_recv[1]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv[1]->cur_buf)
			vd_layer[0].dispbuf_mapping = &gvideo_recv[1]->cur_buf;
		cur_blackout = 1;
	} else if (gvideo_recv[2] &&
	    (gvideo_recv[2]->path_id == vd1_path_id)) {
		/* video_render.2 display on VD1 */
		new_frame = path5_new_frame;
		if (!new_frame) {
			if (!gvideo_recv[2]->cur_buf) {
				/* video_render.2 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (gvideo_recv[2]->cur_buf ==
				&gvideo_recv[2]->local_buf) {
				/* video_render.2 keep frame */
				vd_layer[0].dispbuf = gvideo_recv[2]->cur_buf;
			} else if (vd_layer[0].dispbuf
				!= gvideo_recv[2]->cur_buf) {
				/* video_render.2 has frame in display */
				new_frame = gvideo_recv[2]->cur_buf;
			}
		}
		if (new_frame || gvideo_recv[2]->cur_buf)
			vd_layer[0].dispbuf_mapping = &gvideo_recv[2]->cur_buf;
		cur_blackout = 1;
	} else if (vd1_path_id == VFM_PATH_PIP2) {
		/* pip2 display on VD1 */
		new_frame = path2_new_frame;
		if (!new_frame) {
			if (!cur_dispbuf[2]) {
				/* pip2 no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (cur_dispbuf[2] == &vf_local[2]) {
				/* pip2 keep frame */
				vd_layer[0].dispbuf = cur_dispbuf[2];
			} else if (vd_layer[0].dispbuf
				!= cur_dispbuf[2]) {
				/* pip2 has frame in display */
				new_frame = cur_dispbuf[2];
			}
		}
		if (new_frame || cur_dispbuf[2])
			vd_layer[0].dispbuf_mapping = &cur_dispbuf[2];
		cur_blackout = blackout[2] | force_blackout;
	} else if (vd1_path_id == VFM_PATH_PIP) {
		/* pip display on VD1 */
		new_frame = path1_new_frame;
		if (!new_frame) {
			if (!cur_dispbuf[1]) {
				/* pip no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (cur_dispbuf[1] == &vf_local[1]) {
				/* pip keep frame */
				vd_layer[0].dispbuf = cur_dispbuf[1];
			} else if (vd_layer[0].dispbuf
				!= cur_dispbuf[1]) {
				/* pip has frame in display */
				new_frame = cur_dispbuf[1];
			}
		}
		if (new_frame || cur_dispbuf[1])
			vd_layer[0].dispbuf_mapping = &cur_dispbuf[1];
		cur_blackout = blackout[1] | force_blackout;
	} else if ((vd1_path_id != VFM_PATH_INVALID) &&
		   (vd1_path_id != VFM_PATH_AUTO)) {
		/* primary display on VD1 */
		new_frame = path0_new_frame;
		if (!new_frame) {
			if (!cur_dispbuf[0]) {
				/* primary no frame in display */
				if (cur_vd1_path_id != vd1_path_id)
					safe_switch_videolayer(0, false, true);
				vd_layer[0].dispbuf = NULL;
			} else if (cur_dispbuf[0] == &vf_local[0]) {
				/* primary keep frame */
				vd_layer[0].dispbuf = cur_dispbuf[0];
			} else if (vd_layer[0].dispbuf
				!= cur_dispbuf[0]) {
				/* primary has frame in display */
				new_frame = cur_dispbuf[0];
			}
		}
		if (new_frame || cur_dispbuf[0])
			vd_layer[0].dispbuf_mapping = &cur_dispbuf[0];
		cur_blackout = blackout[0] | force_blackout;
	} else if (vd1_path_id == VFM_PATH_AUTO) {
		new_frame = path0_new_frame;

		if (path3_new_frame &&
			(path3_new_frame->flag & VFRAME_FLAG_FAKE_FRAME)) {
			new_frame = path3_new_frame;
			pr_info("vsync: auto path2 get a fake\n");
		}

		if (!new_frame) {
			if (cur_dispbuf[0] == &vf_local[0])
				vd_layer[0].dispbuf = cur_dispbuf[0];
		}

		if (gvideo_recv[0]->cur_buf &&
			gvideo_recv[0]->cur_buf->flag & VFRAME_FLAG_FAKE_FRAME)
			vd_layer[0].dispbuf = gvideo_recv[0]->cur_buf;

		if (new_frame || cur_dispbuf[0])
			vd_layer[0].dispbuf_mapping = &cur_dispbuf[0];
		cur_blackout = blackout[0] | force_blackout;
	} else {
		cur_blackout = 1;
	}

	/* vout mode detection under new non-tunnel mode */
	if (vd_layer[0].dispbuf || vd_layer[1].dispbuf ||
	   vd_layer[2].dispbuf) {
		if (strcmp(old_vmode, new_vmode)) {
			vd_layer[0].property_changed = true;
			vd_layer[1].property_changed = true;
			vd_layer[2].property_changed = true;
			pr_info("detect vout mode change!!!!!!!!!!!!\n");
			strcpy(old_vmode, new_vmode);
		}
	}

	if (debug_flag & DEBUG_FLAG_PRINT_DISBUF_PER_VSYNC)
		pr_info("VID(%s): layer enable status: VD1:e:%d,e_save:%d,g:%d,d:%d,f:%s; VD2:e:%d,e_save:%d,g:%d,d:%d,f:%s; VD3:e:%d,e_save:%d,g:%d,d:%d,f:%s",
			__func__,
			vd_layer[0].enabled, vd_layer[0].enabled_status_saved,
			vd_layer[0].global_output, vd_layer[0].disable_video,
			vd_layer[0].force_disable ? "true" : "false",
			vd_layer[1].enabled, vd_layer[1].enabled_status_saved,
			vd_layer[1].global_output, vd_layer[1].disable_video,
			vd_layer[1].force_disable ? "true" : "false",
			vd_layer[2].enabled, vd_layer[2].enabled_status_saved,
			vd_layer[2].global_output, vd_layer[2].disable_video,
			vd_layer[2].force_disable ? "true" : "false");

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_enable() && vd_layer[0].global_output) {
		/* no new frame but path switched case, */
		if (new_frame && !is_local_vf(new_frame) &&
		    (!path0_new_frame || new_frame != path0_new_frame) &&
		    (!path1_new_frame || new_frame != path1_new_frame) &&
		    (!path2_new_frame || new_frame != path2_new_frame) &&
		    (!path3_new_frame || new_frame != path3_new_frame) &&
		    (!path4_new_frame || new_frame != path4_new_frame) &&
		    (!path5_new_frame || new_frame != path5_new_frame))
			amdv_update_src_format(new_frame, 1, VD1_PATH);
		else if (!new_frame &&
			 vd_layer[0].dispbuf &&
			 !is_local_vf(vd_layer[0].dispbuf))
			amdv_update_src_format(vd_layer[0].dispbuf, 0, VD1_PATH);
		/* pause and video off->on case */
	}
#endif

	if (!new_frame && vd_layer[0].dispbuf &&
	    is_local_vf(vd_layer[0].dispbuf)) {
		if (cur_blackout) {
			vd_layer[0].property_changed = false;
		} else if (!is_di_post_mode(vd_layer[0].dispbuf)) {
			if (vd_layer[0].switch_vf && vd_layer[0].vf_ext)
				vd_layer[0].vf_ext->canvas0Addr =
					get_layer_display_canvas(0);
			else
				vd_layer[0].dispbuf->canvas0Addr =
					get_layer_display_canvas(0);
		}
	}

	if (vd_layer[0].dispbuf &&
	    (vd_layer[0].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
		VFRAME_FLAG_VIDEO_DRM)) &&
	    !(vd_layer[0].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME) &&
	    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
		int mirror = 0;

		axis[0] = vd_layer[0].dispbuf->axis[0];
		axis[1] = vd_layer[0].dispbuf->axis[1];
		axis[2] = vd_layer[0].dispbuf->axis[2];
		axis[3] = vd_layer[0].dispbuf->axis[3];
		crop[0] = vd_layer[0].dispbuf->crop[0];
		crop[1] = vd_layer[0].dispbuf->crop[1];
		crop[2] = vd_layer[0].dispbuf->crop[2];
		crop[3] = vd_layer[0].dispbuf->crop[3];
		_set_video_window(&glayer_info[0], axis);
		source_type = vd_layer[0].dispbuf->source_type;
		if (is_crop_from_vf(vd_layer[0].dispbuf)) {
			_set_video_crop(&glayer_info[0], crop);
		} else {
			crop_save[0] = glayer_info[0].crop_top_save;
			crop_save[1] = glayer_info[0].crop_left_save;
			crop_save[2] = glayer_info[0].crop_bottom_save;
			crop_save[3] = glayer_info[0].crop_right_save;
			_set_video_crop(&glayer_info[0], crop_save);
		}
		if (vd_layer[0].dispbuf->flag & VFRAME_FLAG_MIRROR_H)
			mirror = H_MIRROR;
		if (vd_layer[0].dispbuf->flag & VFRAME_FLAG_MIRROR_V)
			mirror |= V_MIRROR;
		_set_video_mirror(&glayer_info[0], mirror);
		set_alpha_scpxn(&vd_layer[0], vd_layer[0].dispbuf->composer_info);
		glayer_info[0].zorder = vd_layer[0].dispbuf->zorder;
	} else {
		_set_video_mirror(&glayer_info[0], 0);
	}

	/* setting video display property in underflow mode */
	if (!new_frame &&
		vd_layer[0].dispbuf &&
		(vd_layer[0].property_changed ||
		 is_picmode_changed(0, vd_layer[0].dispbuf))) {
		primary_swap_frame(&vd_layer[0], vd_layer[0].dispbuf, __LINE__);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dvel_swap_frame(cur_dispbuf2);
#endif
	} else if (new_frame) {
		vframe_walk_delay = (int)div_u64(((jiffies_64 -
			new_frame->ready_jiffies64) * 1000), HZ);
		vframe_walk_delay += 1000 *
			vsync_pts_inc_scale / vsync_pts_inc_scale_base;
		vframe_walk_delay -= new_frame->duration / 96;
#ifdef CONFIG_AMLOGIC_MEDIA_FRC
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
		if (!(new_frame->flag & VFRAME_FLAG_GAME_MODE) &&
			(new_frame->dpss_flg & DPSS_FLG_OUT_DONE))
			vframe_walk_delay += dpss_frc_get_video_latency();
#else
		if (!(new_frame->flag & VFRAME_FLAG_GAME_MODE))
			vframe_walk_delay += frc_get_video_latency();
#endif
#endif
		primary_swap_frame(&vd_layer[0], new_frame, __LINE__);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		dvel_swap_frame(cur_dispbuf2);
#endif
	}
	if (vd_layer[0].switch_vd2_vf &&
		vd_layer[0].global_output &&
		vd_layer[0].disable_video != VIDEO_DISABLE_NORMAL)
		new_frame = video_lcevc.enhance_vf;

#if defined(CONFIG_AMLOGIC_MEDIA_FRC)
	frc_input_handle(vd_layer[0].dispbuf, cur_frame_par[0]);
#endif
	if (atomic_read(&axis_changed)) {
		video_prop_status |= VIDEO_PROP_CHANGE_AXIS;
		atomic_set(&axis_changed, 0);
	}

	if (vd1_path_id == VFM_PATH_AMVIDEO ||
	    vd1_path_id == VFM_PATH_DEF)
		vd_layer[0].keep_frame_id = 0;
	else if (vd1_path_id == VFM_PATH_PIP)
		vd_layer[0].keep_frame_id = 1;
	else if (vd1_path_id == VFM_PATH_PIP2)
		vd_layer[0].keep_frame_id = 2;
	else
		vd_layer[0].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	struct vpp_frame_par_s *frame_par = NULL;

	if (vd_layer[0].next_frame_par)
		frame_par = vd_layer[0].next_frame_par;
	else
		frame_par = vd_layer[0].cur_frame_par;

	if (frame_par) {
		vpq_size.cm_hsize = frame_par->cm_input_w;
		vpq_size.cm_vsize = frame_par->cm_input_h;
		vpq_size.sr1_hsc_en = frame_par->supsc1_hori_ratio;
		vpq_size.sr1_vsc_en = frame_par->supsc1_vert_ratio;
		vpq_size.sr1_in_hsize = frame_par->spsc1_w_in;
		vpq_size.sr1_in_vsize = frame_par->spsc1_h_in;
	}

	refresh_on_vs(new_frame, vd_layer[0].dispbuf, VPP_TOP0);
	amvecm_on_vs
		(!is_local_vf(vd_layer[0].dispbuf)
		? vd_layer[0].dispbuf : NULL,
		new_frame,
		new_frame ? CSC_FLAG_TOGGLE_FRAME : 0,
		vpq_size,
		VD1_PATH,
		VPP_TOP0);
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_PRIME_SL
	prime_sl_process(vd_layer[0].dispbuf);
#endif

	/* work around which dec/vdin don't call update src_fmt function */
	if (vd_layer[0].dispbuf && !is_local_vf(vd_layer[0].dispbuf)) {
		int new_src_fmt = -1;
		u32 src_map[] = {
			VFRAME_SIGNAL_FMT_INVALID,
			VFRAME_SIGNAL_FMT_HDR10,
			VFRAME_SIGNAL_FMT_HDR10PLUS,
			VFRAME_SIGNAL_FMT_DOVI,
			VFRAME_SIGNAL_FMT_HDR10PRIME,
			VFRAME_SIGNAL_FMT_HLG,
			VFRAME_SIGNAL_FMT_SDR,
			VFRAME_SIGNAL_FMT_MVC,
			VFRAME_SIGNAL_FMT_CUVA_HDR,
			VFRAME_SIGNAL_FMT_CUVA_HLG,
			VFRAME_SIGNAL_FMT_SDR_2020,
			VFRAME_SIGNAL_FMT_HDR10_709_SOURCE
		};
#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		if (is_amdv_on())
			new_src_fmt = get_amdv_src_format(VD1_PATH, vd_layer[0].dispbuf);
		else
#endif
			new_src_fmt =
				(int)get_cur_source_type(VD1_PATH, VPP_TOP0, vd_layer[0].dispbuf);
#endif
		if (new_src_fmt > 0 && new_src_fmt < MAX_SOURCE)
			fmt = (enum vframe_signal_fmt_e)src_map[new_src_fmt];
		else
			fmt = VFRAME_SIGNAL_FMT_INVALID;
		if (fmt != atomic_read(&cur_primary_src_fmt)) {
			/* atomic_set(&primary_src_fmt, fmt); */
			if (debug_flag & DEBUG_FLAG_TRACE_EVENT) {
				char *old_str = NULL, *new_str = NULL;
				enum vframe_signal_fmt_e old_fmt;

				old_fmt = (enum vframe_signal_fmt_e)
					atomic_read(&cur_primary_src_fmt);
				if (old_fmt != VFRAME_SIGNAL_FMT_INVALID)
					old_str = (char *)src_fmt_str[old_fmt];
				if (fmt != VFRAME_SIGNAL_FMT_INVALID)
					new_str = (char *)src_fmt_str[fmt];
				pr_info("VD1 src fmt changed: %s->%s. vf: %p, signal_type:0x%x\n",
					old_str ? old_str : "invalid",
					new_str ? new_str : "invalid",
					vd_layer[0].dispbuf,
					vd_layer[0].dispbuf->signal_type);
			}
			atomic_set(&cur_primary_src_fmt, fmt);
			atomic_set(&primary_src_fmt, fmt);
			video_prop_status |= VIDEO_PROP_CHANGE_FMT;
			update_primary_fmt_event();
		}
	}

	if (vd_layer[1].dispbuf_mapping == &cur_dispbuf[0] &&
	    (cur_dispbuf[0] == &vf_local[0] ||
	     !cur_dispbuf[0]) &&
	    vd_layer[1].dispbuf != cur_dispbuf[0])
		vd_layer[1].dispbuf = cur_dispbuf[0];

	if (vd_layer[1].dispbuf_mapping == &cur_dispbuf[1] &&
	    (cur_dispbuf[1] == &vf_local[1] ||
	     !cur_dispbuf[1]) &&
	    vd_layer[1].dispbuf != cur_dispbuf[1])
		vd_layer[1].dispbuf = cur_dispbuf[1];

	if (vd_layer[1].dispbuf_mapping == &cur_dispbuf[2] &&
	    (cur_dispbuf[2] == &vf_local[2] ||
	     !cur_dispbuf[2]) &&
	    vd_layer[1].dispbuf != cur_dispbuf[2])
		vd_layer[1].dispbuf = cur_dispbuf[2];

	if (gvideo_recv[0] &&
	    vd_layer[1].dispbuf_mapping == &gvideo_recv[0]->cur_buf &&
	    (gvideo_recv[0]->cur_buf == &gvideo_recv[0]->local_buf ||
	     !gvideo_recv[0]->cur_buf) &&
	    vd_layer[1].dispbuf != gvideo_recv[0]->cur_buf)
		vd_layer[1].dispbuf = gvideo_recv[0]->cur_buf;

	if (gvideo_recv[1] &&
	    vd_layer[1].dispbuf_mapping == &gvideo_recv[1]->cur_buf &&
	    (gvideo_recv[1]->cur_buf == &gvideo_recv[1]->local_buf ||
	     !gvideo_recv[1]->cur_buf) &&
	    vd_layer[1].dispbuf != gvideo_recv[1]->cur_buf)
		vd_layer[1].dispbuf = gvideo_recv[1]->cur_buf;

	if (gvideo_recv[2] &&
	    vd_layer[1].dispbuf_mapping == &gvideo_recv[2]->cur_buf &&
	    (gvideo_recv[2]->cur_buf == &gvideo_recv[2]->local_buf ||
	     !gvideo_recv[2]->cur_buf) &&
	    vd_layer[1].dispbuf != gvideo_recv[2]->cur_buf)
		vd_layer[1].dispbuf = gvideo_recv[2]->cur_buf;

	if (vd_layer[1].switch_vf &&
	    vd_layer[1].dispbuf &&
	    (vd_layer[1].dispbuf->vf_ext ||
	     vd_layer[1].dispbuf->uvm_vf)) {
		/* select uvm_vf first */
		if (vd_layer[1].dispbuf->uvm_vf)
			vd_layer[1].vf_ext =
				vd_layer[1].dispbuf->uvm_vf;
		else
			vd_layer[1].vf_ext =
				(struct vframe_s *)vd_layer[1].dispbuf->vf_ext;
	} else {
		vd_layer[1].vf_ext = NULL;
	}
	/* vd2 config */
	if (gvideo_recv[0] &&
	    gvideo_recv[0]->path_id == vd2_path_id) {
		/* video_render.0 display on VD2 */
		new_frame2 = path3_new_frame;
		//for vd2
		if (video_lcevc.vd2_vd1_shared_vf) {
			new_frame2 = video_lcevc.enhance_vf;
			if (new_frame2 && (debug_common_flag & DEBUG_FLAG_COMMON_LCEVC))
				pr_info("%s,new_frame=0x%p, type=0x%x, flag=0x%x, bitdepth=0x%x\n",
					__func__,
					new_frame2,
					new_frame2->type,
					new_frame2->flag,
					new_frame2->bitdepth);
		} else {
			if (!new_frame2) {
				if (!gvideo_recv[0]->cur_buf) {
					/* video_render.0 no frame in display */
					if (cur_vd2_path_id != vd2_path_id)
						safe_switch_videolayer(1, false, true);
					vd_layer[1].dispbuf = NULL;
				} else if (gvideo_recv[0]->cur_buf ==
					&gvideo_recv[0]->local_buf) {
					/* video_render.0 keep frame */
					vd_layer[1].dispbuf = gvideo_recv[0]->cur_buf;
				} else if (vd_layer[1].dispbuf
					!= gvideo_recv[0]->cur_buf) {
					/* video_render.0 has frame in display */
					new_frame2 = gvideo_recv[0]->cur_buf;
				}
			}
			if (new_frame2 || gvideo_recv[0]->cur_buf)
				vd_layer[1].dispbuf_mapping = &gvideo_recv[0]->cur_buf;
		}
		cur_blackout = 1;
	} else if (gvideo_recv[1] &&
	    (gvideo_recv[1]->path_id == vd2_path_id)) {
		/* video_render.1 display on VD2 */
		new_frame2 = path4_new_frame;
		if (!new_frame2) {
			if (!gvideo_recv[1]->cur_buf) {
				/* video_render.1 no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (gvideo_recv[1]->cur_buf ==
				&gvideo_recv[1]->local_buf) {
				/* video_render.1 keep frame */
				vd_layer[1].dispbuf = gvideo_recv[1]->cur_buf;
			} else if (vd_layer[1].dispbuf
				!= gvideo_recv[1]->cur_buf) {
				/* video_render.1 has frame in display */
				new_frame2 = gvideo_recv[1]->cur_buf;
			}
		}
		if (new_frame2 || gvideo_recv[1]->cur_buf)
			vd_layer[1].dispbuf_mapping = &gvideo_recv[1]->cur_buf;
		cur_blackout = 1;
	} else if (gvideo_recv[2] &&
	    (gvideo_recv[2]->path_id == vd2_path_id)) {
		/* video_render.2 display on VD2 */
		new_frame2 = path5_new_frame;
		if (!new_frame2) {
			if (!gvideo_recv[2]->cur_buf) {
				/* video_render.2 no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (gvideo_recv[2]->cur_buf ==
				&gvideo_recv[2]->local_buf) {
				/* video_render.2 keep frame */
				vd_layer[1].dispbuf = gvideo_recv[2]->cur_buf;
			} else if (vd_layer[1].dispbuf
				!= gvideo_recv[2]->cur_buf) {
				/* video_render.2 has frame in display */
				new_frame2 = gvideo_recv[2]->cur_buf;
			}
		}
		if (new_frame2 || gvideo_recv[2]->cur_buf)
			vd_layer[1].dispbuf_mapping = &gvideo_recv[2]->cur_buf;
		pr_info("new_frame2=%p\n", new_frame2);
		cur_blackout = 1;
	} else if (vd2_path_id == VFM_PATH_AMVIDEO) {
		/* primary display in VD2 */
		new_frame2 = path0_new_frame;
		if (!new_frame2) {
			if (!cur_dispbuf[0]) {
				/* primary no frame in display */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (cur_dispbuf[0] == &vf_local[0]) {
				/* primary keep frame */
				vd_layer[1].dispbuf = cur_dispbuf[0];
			} else if (vd_layer[1].dispbuf
				!= cur_dispbuf[0]) {
				new_frame2 = cur_dispbuf[0];
			}
		}
		if (new_frame2 || cur_dispbuf[0])
			vd_layer[1].dispbuf_mapping = &cur_dispbuf[0];
		cur_blackout = blackout[0] | force_blackout;
	} else if (vd2_path_id == VFM_PATH_PIP2) {
		/* pip display in VD3 */
		new_frame2 = path2_new_frame;
		if (!new_frame2) {
			if (!cur_dispbuf[2]) {
				/* pip no display frame */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (cur_dispbuf[2] == &vf_local[2]) {
				/* pip keep frame */
				vd_layer[1].dispbuf = cur_dispbuf[2];
			} else if (vd_layer[1].dispbuf
				!= cur_dispbuf[2]) {
				new_frame2 = cur_dispbuf[2];
			}
		}
		if (new_frame2 || cur_dispbuf[2])
			vd_layer[1].dispbuf_mapping = &cur_dispbuf[2];
		cur_blackout = blackout[2] | force_blackout;
	} else if (vd2_path_id != VFM_PATH_INVALID) {
		/* pip display in VD2 */
		new_frame2 = path1_new_frame;
		if (!new_frame2) {
			if (!cur_dispbuf[1]) {
				/* pip no display frame */
				if (cur_vd2_path_id != vd2_path_id)
					safe_switch_videolayer(1, false, true);
				vd_layer[1].dispbuf = NULL;
			} else if (cur_dispbuf[1] == &vf_local[1]) {
				/* pip keep frame */
				vd_layer[1].dispbuf = cur_dispbuf[1];
			} else if (vd_layer[1].dispbuf
				!= cur_dispbuf[1]) {
				new_frame2 = cur_dispbuf[1];
			}
		}
		if (new_frame2 || cur_dispbuf[1])
			vd_layer[1].dispbuf_mapping = &cur_dispbuf[1];
		cur_blackout = blackout[1] | force_blackout;
	} else {
		cur_blackout = 1;
	}

	if (!new_frame2 && vd_layer[1].dispbuf &&
	    is_local_vf(vd_layer[1].dispbuf)) {
		if (cur_blackout) {
			vd_layer[1].property_changed = false;
		} else if (vd_layer[1].dispbuf) {
			if (vd_layer[1].switch_vf && vd_layer[1].vf_ext)
				vd_layer[1].vf_ext->canvas0Addr =
					get_layer_display_canvas(1);
			else
				vd_layer[1].dispbuf->canvas0Addr =
					get_layer_display_canvas(1);
		}
	}

	if (vd_layer[1].dispbuf &&
	    (vd_layer[1].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
		VFRAME_FLAG_VIDEO_DRM)) &&
	    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
		int mirror = 0;

		axis[0] = vd_layer[1].dispbuf->axis[0];
		axis[1] = vd_layer[1].dispbuf->axis[1];
		axis[2] = vd_layer[1].dispbuf->axis[2];
		axis[3] = vd_layer[1].dispbuf->axis[3];
		crop[0] = vd_layer[1].dispbuf->crop[0];
		crop[1] = vd_layer[1].dispbuf->crop[1];
		crop[2] = vd_layer[1].dispbuf->crop[2];
		crop[3] = vd_layer[1].dispbuf->crop[3];
		if (video_lcevc.vd2_vd1_shared_vf) {
			/* vd2 is base frame, scaler up to vd1 source size */
			axis[0] = 0;
			axis[1] = 0;
			axis[2] = video_lcevc.vd1_src_width - 1;
			axis[3] = video_lcevc.vd1_src_height - 1;
			/* crop todo */
		}
		_set_video_window(&glayer_info[1], axis);
		source_type = vd_layer[1].dispbuf->source_type;
		if (is_crop_from_vf(vd_layer[1].dispbuf)) {
			_set_video_crop(&glayer_info[1], crop);
		} else {
			crop_save[0] = glayer_info[1].crop_top_save;
			crop_save[1] = glayer_info[1].crop_left_save;
			crop_save[2] = glayer_info[1].crop_bottom_save;
			crop_save[3] = glayer_info[1].crop_right_save;
			_set_video_crop(&glayer_info[1], crop_save);
		}
		if (vd_layer[1].dispbuf->flag & VFRAME_FLAG_MIRROR_H)
			mirror = H_MIRROR;
		if (vd_layer[1].dispbuf->flag & VFRAME_FLAG_MIRROR_V)
			mirror |= V_MIRROR;
		_set_video_mirror(&glayer_info[1], mirror);
		set_alpha_scpxn(&vd_layer[1], vd_layer[1].dispbuf->composer_info);
		glayer_info[1].zorder = vd_layer[1].dispbuf->zorder;
	} else {
		_set_video_mirror(&glayer_info[1], 0);
	}

	/* setting video display property in underflow mode */
	if (!new_frame2 &&
	    vd_layer[1].dispbuf &&
	    (vd_layer[1].property_changed ||
	     is_picmode_changed(1, vd_layer[1].dispbuf))) {
		pipx_swap_frame(&vd_layer[1], vd_layer[1].dispbuf, vinfo);
		need_disable_vd[1] = false;
	} else if (new_frame2) {
		pipx_swap_frame(&vd_layer[1], new_frame2, vinfo);
		need_disable_vd[1] = false;
	}

	if (vd2_path_id == VFM_PATH_PIP ||
	    vd2_path_id == VFM_PATH_DEF)
		vd_layer[1].keep_frame_id = 1;
	else if (vd2_path_id == VFM_PATH_PIP2)
		vd_layer[1].keep_frame_id = 2;
	else if (vd2_path_id == VFM_PATH_AMVIDEO)
		vd_layer[1].keep_frame_id = 0;
	else
		vd_layer[1].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	if (vd_layer[1].next_frame_par)
		frame_par = vd_layer[1].next_frame_par;
	else
		frame_par = vd_layer[1].cur_frame_par;

	memset(&vpq_size, 0, sizeof(struct vpq_size_s));
	if (frame_par) {
		vpq_size.cm_hsize = frame_par->cm_input_w;
		vpq_size.cm_vsize = frame_par->cm_input_h;
		vpq_size.sr1_hsc_en = frame_par->supsc1_hori_ratio;
		vpq_size.sr1_vsc_en = frame_par->supsc1_vert_ratio;
		vpq_size.sr1_in_hsize = frame_par->spsc1_w_in;
		vpq_size.sr1_in_vsize = frame_par->spsc1_h_in;
	}

	amvecm_on_vs
		(!is_local_vf(vd_layer[1].dispbuf)
		? vd_layer[1].dispbuf : NULL,
		new_frame2,
		new_frame2 ? CSC_FLAG_TOGGLE_FRAME : 0,
		vpq_size,
		VD2_PATH,
		VPP_TOP0);
#endif

	if (need_disable_vd[1]) {
		safe_switch_videolayer(1, false, true);
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
		/* reset dvel statue when disable vd2 */
		dvel_status = false;
#endif
	}

	if (cur_dev->max_vd_layers == 3) {
		if (vd_layer[2].dispbuf_mapping == &cur_dispbuf[0] &&
		    (cur_dispbuf[0] == &vf_local[0] ||
		     !cur_dispbuf[0]) &&
		    vd_layer[2].dispbuf != cur_dispbuf[0])
			vd_layer[2].dispbuf = cur_dispbuf[0];

		if (vd_layer[2].dispbuf_mapping == &cur_dispbuf[1] &&
		    (cur_dispbuf[1] == &vf_local[1] ||
		     !cur_dispbuf[1]) &&
		    vd_layer[2].dispbuf != cur_dispbuf[1])
			vd_layer[2].dispbuf = cur_dispbuf[1];

		if (vd_layer[2].dispbuf_mapping == &cur_dispbuf[2] &&
		    (cur_dispbuf[2] == &vf_local[2] ||
		     !cur_dispbuf[2]) &&
		    vd_layer[2].dispbuf != cur_dispbuf[2])
			vd_layer[2].dispbuf = cur_dispbuf[2];

		if (gvideo_recv[0] &&
		    vd_layer[2].dispbuf_mapping == &gvideo_recv[0]->cur_buf &&
		    (gvideo_recv[0]->cur_buf == &gvideo_recv[0]->local_buf ||
		     !gvideo_recv[0]->cur_buf) &&
		    vd_layer[2].dispbuf != gvideo_recv[0]->cur_buf)
			vd_layer[2].dispbuf = gvideo_recv[0]->cur_buf;

		if (gvideo_recv[1] &&
		    vd_layer[2].dispbuf_mapping == &gvideo_recv[1]->cur_buf &&
		    (gvideo_recv[1]->cur_buf == &gvideo_recv[1]->local_buf ||
		     !gvideo_recv[1]->cur_buf) &&
		    vd_layer[2].dispbuf != gvideo_recv[1]->cur_buf)
			vd_layer[2].dispbuf = gvideo_recv[1]->cur_buf;

		if (gvideo_recv[2] &&
		    vd_layer[2].dispbuf_mapping == &gvideo_recv[2]->cur_buf &&
		    (gvideo_recv[2]->cur_buf == &gvideo_recv[2]->local_buf ||
		     !gvideo_recv[2]->cur_buf) &&
		    vd_layer[2].dispbuf != gvideo_recv[2]->cur_buf)
			vd_layer[2].dispbuf = gvideo_recv[2]->cur_buf;

		if (vd_layer[2].switch_vf &&
		    vd_layer[2].dispbuf &&
		    (vd_layer[2].dispbuf->vf_ext ||
		     vd_layer[2].dispbuf->uvm_vf)) {
			/* select uvm_vf first */
			if (vd_layer[2].dispbuf->uvm_vf)
				vd_layer[2].vf_ext =
					vd_layer[2].dispbuf->uvm_vf;
			else
				vd_layer[2].vf_ext =
					(struct vframe_s *)vd_layer[2].dispbuf->vf_ext;
		} else {
			vd_layer[2].vf_ext = NULL;
		}
		/* vd3 config */
		if (gvideo_recv[0] &&
		    gvideo_recv[0]->path_id == vd3_path_id) {
			/* video_render.0 display on VD3 */
			new_frame3 = path3_new_frame;
			if (!new_frame3) {
				if (!gvideo_recv[0]->cur_buf) {
					/* video_render.0 no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (gvideo_recv[0]->cur_buf ==
					&gvideo_recv[0]->local_buf) {
					/* video_render.0 keep frame */
					vd_layer[2].dispbuf = gvideo_recv[0]->cur_buf;
				} else if (vd_layer[2].dispbuf
					!= gvideo_recv[0]->cur_buf) {
					/* video_render.0 has frame in display */
					new_frame3 = gvideo_recv[0]->cur_buf;
				}
			}
			if (new_frame3 || gvideo_recv[0]->cur_buf)
				vd_layer[2].dispbuf_mapping = &gvideo_recv[0]->cur_buf;
			cur_blackout = 1;
		} else if (gvideo_recv[1] &&
		    (gvideo_recv[1]->path_id == vd3_path_id)) {
			/* video_render.1 display on VD3 */
			new_frame3 = path4_new_frame;
			if (!new_frame3) {
				if (!gvideo_recv[1]->cur_buf) {
					/* video_render.1 no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (gvideo_recv[1]->cur_buf ==
					&gvideo_recv[1]->local_buf) {
					/* video_render.1 keep frame */
					vd_layer[2].dispbuf = gvideo_recv[1]->cur_buf;
				} else if (vd_layer[2].dispbuf
					!= gvideo_recv[1]->cur_buf) {
					/* video_render.1 has frame in display */
					new_frame3 = gvideo_recv[1]->cur_buf;
				}
			}
			if (new_frame3 || gvideo_recv[1]->cur_buf)
				vd_layer[2].dispbuf_mapping = &gvideo_recv[1]->cur_buf;
			cur_blackout = 1;
		} else if (gvideo_recv[2] &&
		    (gvideo_recv[2]->path_id == vd3_path_id)) {
			/* video_render.2 display on VD3 */
			new_frame3 = path5_new_frame;
			if (!new_frame3) {
				if (!gvideo_recv[2]->cur_buf) {
					/* video_render.2 no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (gvideo_recv[2]->cur_buf ==
					&gvideo_recv[2]->local_buf) {
					/* video_render.2 keep frame */
					vd_layer[2].dispbuf = gvideo_recv[2]->cur_buf;
				} else if (vd_layer[2].dispbuf
					!= gvideo_recv[2]->cur_buf) {
					/* video_render.2 has frame in display */
					new_frame3 = gvideo_recv[2]->cur_buf;
				}
			}
			if (new_frame3 || gvideo_recv[2]->cur_buf)
				vd_layer[2].dispbuf_mapping = &gvideo_recv[2]->cur_buf;
			cur_blackout = 1;
		} else if (vd3_path_id == VFM_PATH_AMVIDEO) {
			/* primary display in VD3 */
			new_frame3 = path0_new_frame;
			if (!new_frame3) {
				if (!cur_dispbuf[0]) {
					/* primary no frame in display */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (cur_dispbuf[0] == &vf_local[0]) {
					/* primary keep frame */
					vd_layer[2].dispbuf = cur_dispbuf[0];
				} else if (vd_layer[2].dispbuf
					!= cur_dispbuf[0]) {
					new_frame3 = cur_dispbuf[0];
				}
			}
			if (new_frame3 || cur_dispbuf[0])
				vd_layer[2].dispbuf_mapping = &cur_dispbuf[0];
			cur_blackout = blackout[0] | force_blackout;
		} else if (vd3_path_id == VFM_PATH_PIP) {
			/* pip2 display in VD3 */
			new_frame3 = path1_new_frame;
			if (!new_frame3) {
				if (!cur_dispbuf[1]) {
					/* pip no display frame */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (cur_dispbuf[1] == &vf_local[1]) {
					/* pip keep frame */
					vd_layer[2].dispbuf = cur_dispbuf[1];
				} else if (vd_layer[2].dispbuf
					!= cur_dispbuf[1]) {
					new_frame3 = cur_dispbuf[1];
				}
			}
			if (new_frame3 || cur_dispbuf[1])
				vd_layer[2].dispbuf_mapping = &cur_dispbuf[1];
			cur_blackout = blackout[1] | force_blackout;
		} else if (vd3_path_id != VFM_PATH_INVALID) {
			/* pip2 display in VD3 */
			new_frame3 = path2_new_frame;
			if (!new_frame3) {
				if (!cur_dispbuf[2]) {
					/* pip no display frame */
					if (cur_vd3_path_id != vd3_path_id)
						safe_switch_videolayer(2, false, true);
					vd_layer[2].dispbuf = NULL;
				} else if (cur_dispbuf[2] == &vf_local[2]) {
					/* pip keep frame */
					vd_layer[2].dispbuf = cur_dispbuf[2];
				} else if (vd_layer[2].dispbuf
					!= cur_dispbuf[2]) {
					new_frame3 = cur_dispbuf[2];
				}
			}
			if (new_frame3 || cur_dispbuf[2])
				vd_layer[2].dispbuf_mapping = &cur_dispbuf[2];
			cur_blackout = blackout[2] | force_blackout;
		} else {
			cur_blackout = 1;
		}

		if (!new_frame3 && vd_layer[2].dispbuf &&
		    is_local_vf(vd_layer[2].dispbuf)) {
			if (cur_blackout) {
				vd_layer[2].property_changed = false;
			} else if (vd_layer[2].dispbuf) {
				if (vd_layer[2].switch_vf && vd_layer[1].vf_ext)
					vd_layer[2].vf_ext->canvas0Addr =
						get_layer_display_canvas(2);
				else
					vd_layer[2].dispbuf->canvas0Addr =
						get_layer_display_canvas(2);
			}
		}

		if (vd_layer[2].dispbuf &&
		    (vd_layer[2].dispbuf->flag & (VFRAME_FLAG_VIDEO_COMPOSER |
			VFRAME_FLAG_VIDEO_DRM)) &&
		    !(debug_flag & DEBUG_FLAG_AXIS_NO_UPDATE)) {
			axis[0] = vd_layer[2].dispbuf->axis[0];
			axis[1] = vd_layer[2].dispbuf->axis[1];
			axis[2] = vd_layer[2].dispbuf->axis[2];
			axis[3] = vd_layer[2].dispbuf->axis[3];
			crop[0] = vd_layer[2].dispbuf->crop[0];
			crop[1] = vd_layer[2].dispbuf->crop[1];
			crop[2] = vd_layer[2].dispbuf->crop[2];
			crop[3] = vd_layer[2].dispbuf->crop[3];
			_set_video_window(&glayer_info[2], axis);
			source_type = vd_layer[2].dispbuf->source_type;
			if (is_crop_from_vf(vd_layer[2].dispbuf)) {
				_set_video_crop(&glayer_info[2], crop);
			} else {
				crop_save[0] = glayer_info[2].crop_top_save;
				crop_save[1] = glayer_info[2].crop_left_save;
				crop_save[2] = glayer_info[2].crop_bottom_save;
				crop_save[3] = glayer_info[2].crop_right_save;
				_set_video_crop(&glayer_info[2], crop_save);
			}
			set_alpha_scpxn(&vd_layer[2], vd_layer[2].dispbuf->composer_info);
			glayer_info[2].zorder = vd_layer[2].dispbuf->zorder;
		}

		/* setting video display property in underflow mode */
		if (!new_frame3 &&
		    vd_layer[2].dispbuf &&
		    (vd_layer[2].property_changed ||
		     is_picmode_changed(2, vd_layer[2].dispbuf))) {
			pipx_swap_frame(&vd_layer[2], vd_layer[2].dispbuf, vinfo);
			need_disable_vd[2] = false;
		} else if (new_frame3) {
			pipx_swap_frame(&vd_layer[2], new_frame3, vinfo);
			need_disable_vd[2] = false;
		}

		if (vd3_path_id == VFM_PATH_PIP2 ||
		    vd3_path_id == VFM_PATH_DEF)
			vd_layer[2].keep_frame_id = 2;
		else if (vd3_path_id == VFM_PATH_PIP)
			vd_layer[2].keep_frame_id = 1;
		else if (vd3_path_id == VFM_PATH_AMVIDEO)
			vd_layer[2].keep_frame_id = 0;
		else
			vd_layer[2].keep_frame_id = 0xff;

#if defined(CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM)
	if (vd_layer[2].next_frame_par)
		frame_par = vd_layer[2].next_frame_par;
	else
		frame_par = vd_layer[2].cur_frame_par;

	memset(&vpq_size, 0, sizeof(struct vpq_size_s));
	if (frame_par) {
		vpq_size.cm_hsize = frame_par->cm_input_w;
		vpq_size.cm_vsize = frame_par->cm_input_h;
		vpq_size.sr1_hsc_en = frame_par->supsc1_hori_ratio;
		vpq_size.sr1_vsc_en = frame_par->supsc1_vert_ratio;
		vpq_size.sr1_in_hsize = frame_par->spsc1_w_in;
		vpq_size.sr1_in_vsize = frame_par->spsc1_h_in;
	}

	amvecm_on_vs
		(!is_local_vf(vd_layer[2].dispbuf)
		? vd_layer[2].dispbuf : NULL,
		new_frame3,
		new_frame3 ? CSC_FLAG_TOGGLE_FRAME : 0,
		vpq_size,
		VD3_PATH,
		VPP_TOP0);
#endif
		if (need_disable_vd[2])
			safe_switch_videolayer(2, false, true);
	}

	/* filter setting management */
	for (i = 0; i < MAX_VD_LAYER; i++)
		vdx_render_frame(&vd_layer[i], vinfo);
	video_secure_set(VPP0);

	if (vd_layer[0].dispbuf &&
		(vd_layer[0].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME)) {
		if ((vd_layer[0].force_black &&
			!(debug_flag & DEBUG_FLAG_NO_CLIP_SETTING)) ||
			!vd_layer[0].force_black) {
			if (vd_layer[0].dispbuf->type & VIDTYPE_RGB_444) {
				/* RGB */
				vd_layer[0].clip_setting.clip_max =
					(0x0 << 20) | (0x0 << 10) | 0;
				vd_layer[0].clip_setting.clip_min =
					vd_layer[0].clip_setting.clip_max;
			} else {
				/* YUV */
				vd_layer[0].clip_setting.clip_max =
					(0x0 << 20) | (0x200 << 10) | 0x200;
				vd_layer[0].clip_setting.clip_min =
					vd_layer[0].clip_setting.clip_max;
			}
			vd_layer[0].clip_setting.clip_done = false;
		}
		if (!vd_layer[0].force_black) {
			pr_debug("vsync: vd1 force black\n");
			vd_layer[0].force_black = true;
		}
	} else if (vd_layer[0].force_black) {
		pr_debug("vsync: vd1 black to normal\n");
		vd_layer[0].clip_setting.clip_max =
			(0x3ff << 20) | (0x3ff << 10) | 0x3ff;
		vd_layer[0].clip_setting.clip_min = 0;
		vd_layer[0].clip_setting.clip_done = false;
		vd_layer[0].force_black = false;
	}

	if (vd_layer[1].dispbuf &&
	    (vd_layer[1].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME))
		safe_switch_videolayer(1, false, true);

	if (vd_layer[2].dispbuf &&
	    (vd_layer[2].dispbuf->flag & VFRAME_FLAG_FAKE_FRAME))
		safe_switch_videolayer(2, false, true);

	if ((cur_vd1_path_id != vd1_path_id ||
	     cur_vd2_path_id != vd2_path_id ||
	     cur_vd3_path_id != vd3_path_id) &&
	    (debug_flag & DEBUG_FLAG_PRINT_PATH_SWITCH)) {
		pr_info("VID: === After path switch ===\n");
		pr_info("VID: \tpath_id: %d, %d, %d;\nVID: \ttoggle:%p, %p, %p %p, %p, %p\nVID: \tcur:%p, %p, %p, %p, %p, %p;\n",
			vd1_path_id, vd2_path_id, vd3_path_id,
			path0_new_frame, path1_new_frame,
			path2_new_frame, path3_new_frame,
			path4_new_frame, path5_new_frame,
			cur_dispbuf[0], cur_dispbuf[1], cur_dispbuf[2],
			gvideo_recv[0] ? gvideo_recv[0]->cur_buf : NULL,
			gvideo_recv[1] ? gvideo_recv[1]->cur_buf : NULL,
			gvideo_recv[2] ? gvideo_recv[2]->cur_buf : NULL);
		pr_info("VID: \tdispbuf:%p, %p, %p; \tvf_ext:%p, %p, %p;\nVID: \tlocal:%p, %p, %p, %p, %p, %p\n",
			vd_layer[0].dispbuf, vd_layer[1].dispbuf, vd_layer[2].dispbuf,
			vd_layer[0].vf_ext, vd_layer[1].vf_ext, vd_layer[2].vf_ext,
			&vf_local[0], &vf_local[1], &vf_local[2],
			gvideo_recv[0] ? &gvideo_recv[0]->local_buf : NULL,
			gvideo_recv[1] ? &gvideo_recv[1]->local_buf : NULL,
			gvideo_recv[2] ? &gvideo_recv[2]->local_buf : NULL);
		pr_info("VID: \tblackout:%d %d, %d force:%d;\n",
			blackout[0], blackout[1], blackout[2], force_blackout);
	}

	if (vd_layer[0].dispbuf &&
	    (vd_layer[0].dispbuf->type & VIDTYPE_MVC))
		vd_layer[0].enable_3d_mode = mode_3d_mvc_enable;
	else if (process_3d_type)
		vd_layer[0].enable_3d_mode = mode_3d_enable;
	else
		vd_layer[0].enable_3d_mode = mode_3d_disable;

	/* all frames has been renderred, so reset new frame flag */
	vd_layer[0].new_frame = false;
	vd_layer[1].new_frame = false;
	vd_layer[2].new_frame = false;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (vd_layer[0].dispbuf) {
		pq_process_debug[0] = ai_pq_value;
		pq_process_debug[1] = ai_pq_disable;
		pq_process_debug[2] = ai_pq_debug;
		pq_process_debug[3] = ai_pq_policy;
#ifdef CONFIG_AMLOGIC_VDETECT
		vdetect_get_frame_nn_info(vd_layer[0].dispbuf);
#endif
		vf_pq_process(vd_layer[0].dispbuf, vpp_scenes, pq_process_debug,
						new_frame ? 1 : 0);
		if (ai_pq_debug > 0x10) {
			ai_pq_debug--;
			if (ai_pq_debug == 0x10)
				ai_pq_debug = 0;
		}
		memcpy(nn_scenes_value, vd_layer[0].dispbuf->nn_value,
			   sizeof(nn_scenes_value));
	}
#endif
exit:
	vd_clip_setting(VPP0, 0, &vd_layer[0].clip_setting, false);
	vd_clip_setting(VPP0, 1, &vd_layer[1].clip_setting, false);
	if (cur_dev->max_vd_layers == 3)
		vd_clip_setting(VPP0, 2, &vd_layer[2].clip_setting, false);

	vpp_blend_update(vinfo, VPP0);

	if (gvideo_recv[0])
		gvideo_recv[0]->func->late_proc(gvideo_recv[0]);
	if (gvideo_recv[1])
		gvideo_recv[1]->func->late_proc(gvideo_recv[1]);
	if (gvideo_recv[2])
		gvideo_recv[2]->func->late_proc(gvideo_recv[2]);

#ifdef CONFIG_AMLOGIC_MEDIA_VSYNC_RDMA
RUN_FIRST_RDMA:
	/* vsync_rdma_config(); */
	vsync_rdma_process();
#endif

	/* if prop_change not zero, event will be delayed to next vsync */
	if (video_prop_status &&
	    !atomic_read(&video_prop_wakeup)) {
		if (debug_flag & DEBUG_FLAG_TRACE_EVENT)
			pr_info("VD1 send event, changed status: 0x%x\n",
				video_prop_status);
		atomic_set(&video_prop_change, video_prop_status);
		video_prop_status = VIDEO_PROP_CHANGE_NONE;
		atomic_set(&video_prop_wakeup, 1);
		wake_up_interruptible(&amvideo_prop_change_wait);
	}
	if (video_info_change_status) {
		struct vd_info_s vd_info;

		if (debug_flag & DEBUG_FLAG_TRACE_EVENT)
			pr_info("VD1 send event to frc, changed status: 0x%x\n",
				video_info_change_status);
		vd_info.flags = video_info_change_status;
		vd_signal_notifier_call_chain(VIDEO_INFO_CHANGED,
					      &vd_info);
		video_info_change_status = VIDEO_INFO_CHANGE_NONE;
	}

	vpp_crc_result = vpp_crc_check(vpp_crc_en, VPP0);

	cur_vd1_path_id = vd1_path_id;
	cur_vd2_path_id = vd2_path_id;
	cur_vd3_path_id = vd3_path_id;
	return 0;
}

int proc_lowlatency_frame(u8 instance_id)
{
	u32 enc_line1, enc_line2, last_line;
	u32 min_line, max_line, start_line, vinfo_height;
	const struct vinfo_s *cur_vinfo;
	bool err_flag = false;
	static ulong last_vsync_count;

	if (get_low_latency_version() == 2) {
		if (debug_flag & DEBUG_FLAG_TRACE_EVENT)
			pr_info("%s should not be called in V2 low_latency mode\n",
				__func__);
	}
	if (legacy_vpp)
		return 0;

	if (atomic_read(&video_recv_cnt) > 1)
		return -1;

	cur_vinfo = get_current_vinfo();
	if (!cur_vinfo) {
		lowlatency_err_drop++;
		return -1;
	}
	if (cur_vinfo->field_height != cur_vinfo->height)
		vinfo_height = cur_vinfo->field_height;
	else
		vinfo_height = cur_vinfo->height;

	start_line = get_active_start_line();
	min_line = (vinfo_height * frame_line_threshold) / 100 + start_line;
	max_line = (vinfo_height * (100 - frame_line_threshold)) / 100 + start_line;
	enc_line1 = get_cur_enc_line();
	if (enc_line1 >= max_line || overrun_flag) {
		lowlatency_proc_drop++;
		return -2;
	}

	if (atomic_inc_return(&video_proc_lock) > 1) {
		lowlatency_proc_drop++;
		atomic_dec(&video_proc_lock);
		return -2;
	}

	if (lowlatency_vsync_count == last_vsync_count) {
		lowlatency_overflow_cnt++;
		atomic_dec(&video_proc_lock);
		return -2;
	}

	last_line = enc_line1;
	while (enc_line1 < min_line) {
		usleep_range(500, 600);
		enc_line1 = get_cur_enc_line();
		if (last_line == enc_line1) {
			/* active line no change */
			err_flag = true;
			break;
		}
		last_line = enc_line1;
	}
	if (err_flag) {
		lowlatency_err_drop++;
		atomic_dec(&video_proc_lock);
		return -1;
	}
	atomic_set(&video_inirq_flag, 1);
	enc_line1 = get_cur_enc_line();

	if (lowlatency_max_enter_lines < enc_line1)
		lowlatency_max_enter_lines = enc_line1;
	if (lowlatency_min_enter_lines > enc_line1)
		lowlatency_min_enter_lines = enc_line1;

	last_vsync_count = lowlatency_vsync_count;
	lowlatency_vsync(instance_id);
	lowlatency_proc_done++;
	enc_line2 = get_cur_enc_line();
	if (enc_line2 < enc_line1) {
		lowlatency_overrun_cnt++;
		overrun_flag = true;
	}
	if (lowlatency_min_exit_lines > enc_line2 && !overrun_flag)
		lowlatency_min_exit_lines = enc_line2;
	if (lowlatency_max_exit_lines < enc_line2)
		lowlatency_max_exit_lines = enc_line2;
	enc_line2 -= enc_line1;
	if (lowlatency_max_proc_lines < enc_line2)
		lowlatency_max_proc_lines = enc_line2;
	atomic_set(&video_inirq_flag, 0);
	atomic_dec(&video_proc_lock);
	return 0;
}
EXPORT_SYMBOL(proc_lowlatency_frame);

ssize_t lowlatency_states_show(const struct class *cla,
	const struct class_attribute *attr,
	char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		"low latency process done count: %d\n",
		lowlatency_proc_done);
	len += sprintf(buf + len,
		"low latency process frame count: %d\n",
		lowlatency_proc_frame_cnt);
	len += sprintf(buf + len,
		"low latency skip frame count: %d\n",
		lowlatency_skip_frame_cnt);
	len += sprintf(buf + len,
		"low latency overflow count: %d\n",
		lowlatency_overflow_cnt);
	len += sprintf(buf + len,
		"low latency process drop count: %d\n",
		lowlatency_proc_drop);
	len += sprintf(buf + len,
		"low latency process err count: %d\n",
		lowlatency_err_drop);
	len += sprintf(buf + len,
		"low latency process overrun count: %d\n",
		lowlatency_overrun_cnt);
	len += sprintf(buf + len,
		"low latency process overrun recovery count: %d\n",
		lowlatency_overrun_recovery_cnt);
	len += sprintf(buf + len,
		"low latency process max proc lines: %d\n",
		lowlatency_max_proc_lines);
	len += sprintf(buf + len,
		"low latency process max enter lines: %d\n",
		lowlatency_max_enter_lines);
	len += sprintf(buf + len,
		"low latency process max exit lines: %d\n",
		lowlatency_max_exit_lines);
	len += sprintf(buf + len,
		"low latency process min enter lines: %d\n",
		lowlatency_min_enter_lines);
	len += sprintf(buf + len,
		"low latency process min exit lines: %d\n",
		lowlatency_min_exit_lines);
	len += sprintf(buf + len,
		"vsync process done count: %d\n",
		vsync_proc_done);
	len += sprintf(buf + len,
		"vsync process drop count: %d\n",
		vsync_proc_drop);
	len += sprintf(buf + len,
		"video_receiver cnt: %d\n",
		atomic_read(&video_recv_cnt));
	len += sprintf(buf + len,
		"line threshold %d\n",
		frame_line_threshold);
	len += sprintf(buf + len,
		"rdma proc drop %llu\n",
		(unsigned long long)lowlatency_rdma_proc_drop);
	len += sprintf(buf + len,
		"rdma err drop %llu\n",
		(unsigned long long)lowlatency_rdma_err_drop);
	len += sprintf(buf + len,
		"rdma err2 drop %llu\n",
		(unsigned long long)lowlatency_rdma_err2_drop);

	return len;
}

ssize_t lowlatency_states_store(const struct class *cla,
	const struct class_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	u32 val = 0;

	ret = kstrtoint(buf, 0, &val);
	if (ret < 0)
		return -EINVAL;

	lowlatency_err_drop = 0;
	lowlatency_proc_done = 0;
	lowlatency_overrun_cnt = 0;
	lowlatency_overrun_recovery_cnt = 0;
	lowlatency_max_enter_lines = 0;
	lowlatency_max_proc_lines = 0;
	lowlatency_max_exit_lines = 0;
	lowlatency_min_enter_lines = 0xffffffff;
	lowlatency_min_exit_lines = 0xffffffff;
	lowlatency_proc_drop = 0;
	lowlatency_overflow_cnt = 0;
	lowlatency_proc_frame_cnt = 0;
	lowlatency_skip_frame_cnt = 0;
	vsync_proc_drop = 0;
	vsync_proc_done = 0;
	lowlatency_rdma_proc_drop = 0;
	lowlatency_rdma_err_drop = 0;
	lowlatency_rdma_err2_drop = 0;
	pr_info("Clear the lowlatency states information!\n");
	return count;
}

/* --- low latency v2 start ---*/
static int line_n_in;
static struct task_struct *video_thread;
static wait_queue_head_t frame_process_wq;
static bool video_thread_init_done;
static int wake_flag;

void vpp_lowlatency_wakeup(void)
{
	u32 enc_line;
	unsigned long lock_flags;

	if (only_vpp_wake && !wake_flag)
		return;

	if (get_low_latency_version() != 2 || !video_thread_init_done)
		return;

	spin_lock_irqsave(&video_llm_lock, lock_flags);
	if (!atomic_read(&video_llm_wake) || !atomic_read(&video_llm_done)) {
		spin_unlock_irqrestore(&video_llm_lock, lock_flags);
		return;
	}
	atomic_set(&video_llm_wake, 0);
	atomic_set(&video_llm_done, 0);
	spin_unlock_irqrestore(&video_llm_lock, lock_flags);

	line_n_in = 1;
	wake_up_interruptible(&frame_process_wq);
	enc_line = get_cur_enc_line();
	if (debug_flag & DEBUG_FLAG_LATENCY)
		pr_info("%s, wakeup enc_line:%d wake_flag:%d\n", __func__,
			enc_line, wake_flag);
}

static irqreturn_t line_n_isr(int irq, void *dev_id)
{
	wake_flag = 1;
	vpp_lowlatency_wakeup();
	wake_flag = 0;

	return IRQ_HANDLED;
}

int vpp_low_latency_check(void)
{
	u32 enc_line, min_line, max_line, start_line, vinfo_height;
	const struct vinfo_s *cur_vinfo;
	static ulong last_vsync_count;

	if (get_low_latency_version() != 2)
		return 0;

	cur_vinfo = get_current_vinfo();
	if (!cur_vinfo) {
		lowlatency_rdma_proc_drop++;
		return -1;
	}

	if (cur_vinfo->field_height != cur_vinfo->height)
		vinfo_height = cur_vinfo->field_height;
	else
		vinfo_height = cur_vinfo->height;

	// check line count and configure only within the threshold range.
	start_line = get_active_start_line();
	min_line = 0;
	max_line = (vinfo_height * (1000 - rdma_line_threshold)) / 1000 + 1 + start_line;
	if (rdma_check_min_line)
		min_line = rdma_check_min_line;
	if (rdma_check_max_line)
		max_line = rdma_check_max_line;

	enc_line = get_cur_enc_line();
	if (enc_line < min_line || enc_line > max_line) {
		if (debug_flag & DEBUG_FLAG_LATENCY)
			pr_info("%s enc_line:%d min_line:%d max_line:%d\n",
				__func__, enc_line, min_line, max_line);
		lowlatency_rdma_err_drop++;
		return -1;
	}

	// avoid repeated configuration within the same vsync.
	if (last_vsync_count == lowlatency_vsync_count) {
		lowlatency_rdma_err2_drop++;
		return -2;
	}
	last_vsync_count = lowlatency_vsync_count;

	return 0;
}

static int process_frame(void)
{
	int enc_line1, enc_line2;
	int max_line, start_line, vinfo_height;
	const struct vinfo_s *cur_vinfo;
	static ulong last_vsync_count;

	if (legacy_vpp)
		return 0;

	cur_vinfo = get_current_vinfo();
	if (!cur_vinfo) {
		lowlatency_err_drop++;
		return -1;
	}

	if (cur_vinfo->field_height != cur_vinfo->height)
		vinfo_height = cur_vinfo->field_height;
	else
		vinfo_height = cur_vinfo->height;

	if (rdma_add_delay_ms)
		mdelay(rdma_add_delay_ms);
	start_line = get_active_start_line();

	max_line = (vinfo_height * (100 - frame_line_threshold)) / 100 + start_line;
	enc_line1 = get_cur_enc_line();
	if (debug_flag & DEBUG_FLAG_LATENCY)
		pr_info("process frame start, line:%d start_line:%d max_line:%d\n",
			enc_line1, start_line, max_line);

	if (enc_line1 >= max_line || overrun_flag) {
		lowlatency_proc_drop++;
		return -2;
	}

	if (atomic_inc_return(&video_proc_lock) > 1) {
		lowlatency_proc_drop++;
		atomic_dec(&video_proc_lock);
		return -3;
	}

	if (lowlatency_vsync_count == last_vsync_count) {
		lowlatency_overflow_cnt++;
		atomic_dec(&video_proc_lock);
		return -4;
	}

	atomic_set(&video_inirq_flag, 1);
	enc_line1 = get_cur_enc_line();

	if (lowlatency_max_enter_lines < enc_line1)
		lowlatency_max_enter_lines = enc_line1;
	if (lowlatency_min_enter_lines > enc_line1)
		lowlatency_min_enter_lines = enc_line1;

	last_vsync_count = lowlatency_vsync_count;
	video_lowlatency_process(); // vsync_isr_in();
	lowlatency_proc_done++;
	enc_line2 = get_cur_enc_line();
	if (enc_line2 < enc_line1) {
		lowlatency_overrun_cnt++;
		overrun_flag = true;
	}
	if (lowlatency_min_exit_lines > enc_line2 && !overrun_flag)
		lowlatency_min_exit_lines = enc_line2;
	if (lowlatency_max_exit_lines < enc_line2)
		lowlatency_max_exit_lines = enc_line2;
	enc_line2 -= enc_line1;
	if (lowlatency_max_proc_lines < enc_line2)
		lowlatency_max_proc_lines = enc_line2;
	atomic_set(&video_inirq_flag, 0);
	atomic_dec(&video_proc_lock);
	if (debug_flag & DEBUG_FLAG_LATENCY)
		pr_info("process frame end, line:%d\n", enc_line2 + enc_line1);

	return 0;
}

static int threadfunc(void *data)
{
	int ret = -1;

	ktime_t start_time = 0, diff_time = 0;
	u32 time_cost_ms;
	u32 time_cost_us;

	while (1) {
		if (kthread_should_stop())
			break;
		ret = wait_event_interruptible(frame_process_wq, line_n_in);
		if (ret)
			pr_err("wait for the event failed, ret: %d\n", ret);

		if (line_n_in)
			line_n_in = 0;

		start_time = ktime_get();
		ret = process_frame();
		atomic_set(&video_llm_done, 1);
		diff_time = ktime_sub(ktime_get(), start_time);
		time_cost_ms = ktime_to_ms(diff_time);
		time_cost_us = ktime_to_us(diff_time);
		if (debug_flag & DEBUG_FLAG_PRINT_DISBUF_PER_VSYNC)
			pr_info("lowlatency time cost %dms %dus, ret:%d\n",
				time_cost_ms, time_cost_us, ret);
		if (ret && (debug_flag & DEBUG_FLAG_BASIC_INFO))
			pr_info("process_frame ret:%d\n", ret);
	}
	return 0;
}

void video_lowlatency_line_n_num(void)
{
	const struct vinfo_s *info = get_current_vinfo();

	if (!info || info->mode == VMODE_INVALID) {
		if (debug_flag & DEBUG_FLAG_BASIC_INFO)
			pr_info("%s invalid vinfo:%p vinfo->mode:%d\n",
				__func__, info, info ? info->mode : 0);
		return;
	}

	WRITE_VCBUS_REG(VPP_INT_LINE_NUM, info->field_height * 3 / 4);
	if (debug_flag & DEBUG_FLAG_BASIC_INFO)
		pr_info("%s info->field_height:%d VPP_INT_LINE_NUM:%d\n",
			__func__, info->height,
			READ_VCBUS_REG(VPP_INT_LINE_NUM));
}

void video_lowlatency_init(struct platform_device *pdev)
{
	int err, ret;
	int video_line_n_int = -ENXIO;
	struct sched_param param = {.sched_priority = 2};
	const void *prop;
	int low_latency_en = 0;

	prop = of_get_property(pdev->dev.of_node, "low_latency_en", NULL);
	if (prop)
		low_latency_en = of_read_ulong(prop, 1);

	if (!low_latency_en)
		return;
	/* set low latency enable:1 version:2 */
	set_low_latency_info(1, 2);

	/* line_n interrupt related */
	video_line_n_int = platform_get_irq_byname(pdev, "line_n");
	if (video_line_n_int < 0) {
		pr_info("VID: cannot get video_line_n irq resource\n");
		return;
	}
	pr_debug("VID: amvideom line_n irq: %d\n", video_line_n_int);
	video_lowlatency_line_n_num();

	if (video_line_n_int >= 0) {
		ret = request_irq(video_line_n_int, &line_n_isr,
			IRQF_SHARED, "line_n", (void *)"line_n");
		if (ret < 0) {
			pr_info("VID: request line_n irq fail, %d\n", ret);
			return;
		}
	}

	/* thread related */
	init_waitqueue_head(&frame_process_wq);

	video_thread = kthread_create(threadfunc, NULL, "video_monitor");
	if (IS_ERR(video_thread)) {
		err = PTR_ERR(video_thread);
		pr_err("VID: unable to start kernel thread, %d.", err);
		video_thread = NULL;
		return;
	}
	if (sched_setscheduler_nocheck(video_thread, SCHED_FIFO, &param)) {
		pr_err("VID: could not set realtime priority.\n");
		return;
	}
	wake_up_process(video_thread);
	video_thread_init_done = true;
}

void video_lowlatency_exit(void)
{
	if (video_thread) {
		video_thread_init_done = false;
		kthread_stop(video_thread);
		video_thread = NULL;
	}
}

/* --- low latency v2 end ---*/

