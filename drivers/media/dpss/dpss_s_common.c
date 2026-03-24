// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "sys_def.h"		//ary add for sim
//#include "define.h"
#include <linux/clk.h>
#include <linux/clk-provider.h>
#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/semaphore.h>
#include <linux/kfifo.h>
#include <linux/sched/clock.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>

#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/dma-map-ops.h>
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/tee.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>
#include <linux/dma-mapping.h>	//for rdma
#include <linux/amlogic/media/registers/cpu_version.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#endif				/* RUN_ON_ARM */

#include "dpss_input_q.h"
#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_func.h"
#include "dpss_hw.h"
#include "dpss_s_frc.h"
#include "dpss_rdma_frc.h"
#include "hw/fgrain.h"

#include "drv/d_dd.h"
#include "drv/d_hdr.h"
#include "drv/d_pq.h"

u32 frc_secure_handle;
// extern u32 dpss_mc_rls_cnt;
u32 resolution_switch_count;

static unsigned int dpss_is_vinfo_chg(struct dpss_vinfo_s *v1,	/* new */
				      struct dpss_vinfo_s *v2, bool prt_en);
static void out_put_vf(struct dpss_ch_s *pch, unsigned int idx, bool output_last);
static bool is_high_fps_src(struct vframe_s *vfm_f);

unsigned int dpss_wr_sel = C_BIT0;
module_param_named(dpss_wr_sel, dpss_wr_sel, uint, 0664);

bool dpss_tst_4k;	//
module_param_named(dpss_tst_4k, dpss_tst_4k, bool, 0664);

unsigned int dpss_o_fmt = EDPSS_DP_MODE_422_10BIT_2PACK;
module_param_named(dpss_o_fmt, dpss_o_fmt, uint, 0664);

unsigned int dpss_pps_count;
module_param_named(dpss_pps_count, dpss_pps_count, uint, 0664);

//bit 0 for ch 0;
//bi 1 for ch 1;
unsigned int dpss_dbg_bypss;
module_param_named(dpss_dbg_bypss, dpss_dbg_bypss, uint, 0664);
bool dpss_dbg_secure; //dpss_dbg_sc
module_param_named(dpss_dbg_secure, dpss_dbg_secure, bool, 0664);
unsigned int dpss_afrc_y = 40;
module_param_named(dpss_afrc_y, dpss_afrc_y, uint, 0664);
unsigned int dpss_afrc_c = 40;
module_param_named(dpss_afrc_c, dpss_afrc_c, uint, 0664);

unsigned int dpss_afrc_head = 1;
module_param_named(dpss_afrc_head, dpss_afrc_head, uint, 0664);
bool dpss_lc_buf;
module_param_named(dpss_lc_buf, dpss_lc_buf, bool, 0664);

unsigned int dpss_hw_pause;
module_param_named(dpss_hw_pause, dpss_hw_pause, uint, 0664);

unsigned int pass_cnt; //crc

unsigned int dpss_light_chg = 0x1;
module_param_named(dpss_light_chg, dpss_light_chg, uint, 0664);

//bit[15:0] src0 en: bit 15
//bit [31:16] src1 en : bit 31
unsigned int dpss_mem_support; //
module_param_named(dpss_mem_support, dpss_mem_support, uint, 0664);

unsigned int dpss_cfg_buf_num;
module_param_named(dpss_cfg_buf_num, dpss_cfg_buf_num, uint, 0664);

unsigned char dpss_cfg_num_in(unsigned int ch)
{
	if (ch)
		return (unsigned char)((dpss_cfg_buf_num >> 16) & 0xf);

	return (unsigned char)(dpss_cfg_buf_num & 0xf);
}

unsigned char dpss_cfg_num_out(unsigned int ch)
{
	unsigned int tmp;

	if (ch) {
		tmp = dpss_cfg_buf_num >> 16;
		return (unsigned char)((tmp >> 4) & 0xf);
	}
	return (unsigned char)((dpss_cfg_buf_num >> 4) & 0xf);
}

unsigned char dpss_cfg_num_pq(unsigned int ch)
{
	unsigned int tmp;

	if (ch) {
		tmp = dpss_cfg_buf_num >> 16;
		return (unsigned char)((tmp >> 8) & 0xf);
	}
	return (unsigned char)((dpss_cfg_buf_num >> 8) & 0xf);
}

/********************************
 *timer:
 *******************************/

u64 dpss_cur_to_msecs(void)
{				/*cur_to_msecs */
#ifdef RUN_ON_ARM
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_MSEC);
#else
	u64 cur = cur_to_msecs();
#endif
	return cur;
}

u64 dpss_cur_to_usecs(void)
{				/*cur_to_usecs */
#ifdef RUN_ON_ARM
	u64 cur = sched_clock();

	do_div(cur, NSEC_PER_USEC);
#else
	u64 cur = cur_to_usecs();
#endif
	return cur;
}

bool dpss_trig_bypass_nr;
module_param_named(dpss_trig_bypass_nr, dpss_trig_bypass_nr, bool, 0664);

//new feature
bool dpss_is_bypass_nr(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	if (dpss_trig_bypass_nr) {
		dbg_i2("trig bypass nr\n");
		return true;
	}
	if (is_meson_t6w_cpu() && is_high_fps_src(vfm))
		return true;

	return false;
}

/* common */
bool dpss_vfm_2_subvf(struct dpss_sub_vf_s *vfms, struct vframe_s *vfm)
{
	if (!vfms || !vfm)
		return false;

	vfms->index_disp = vfm->index_disp;
	vfms->type = vfm->type;
	vfms->canvas0Addr = vfm->canvas0Addr;
	vfms->canvas1Addr = vfm->canvas1Addr;
	vfms->compHeadAddr = vfm->compHeadAddr;
	vfms->compBodyAddr = vfm->compBodyAddr;
	vfms->plane_num = vfm->plane_num;
	vfms->sig_fmt = vfm->sig_fmt;
	memcpy(&vfms->canvas0_config[0], &vfm->canvas0_config[0],
	       sizeof(vfm->canvas0_config[0]) * 2);

	vfms->width = vfm->width;
	vfms->height = vfm->height;
	if (VFM_IS_COMP_MODE(vfm->type)) {
		vfms->width = vfm->compWidth;
		vfms->height = vfm->compHeight;
		vfms->cvs_h = roundup(vfms->width, 64);
	} else {
		vfms->cvs_h = vfms->canvas0_config[0].width;
		if (vfms->canvas0_config[0].width & 0x3f)
			DBG_WARN("cvs h=%d\n", vfms->canvas0_config[0].width);
	}
	vfms->bitdepth = vfm->bitdepth;
	vfms->bitdepth_dw = vfm->bitdepth_dw;
	vfms->decontour_pre = vfm->decontour_pre;
	vfms->flag = vfm->flag;
	vfms->source_type = vfm->source_type;
	vfms->video_angle = vfm->video_angle;
	vfms->signal_type = vfm->signal_type;
	vfms->sig_fmt = vfm->sig_fmt;
	vfms->fmt = vfm->src_fmt.fmt;
	vfms->sei_magic_code = vfm->src_fmt.sei_magic_code;
	vfms->duration = vfm->duration;
	vfms->fgs_valid = vfm->fgs_valid;
	vfms->fgs_table_adr = vfm->fgs_table_adr;
//      vfms->dpss_id           = vfm->dpss_id;
	/*****************************************/
	if (vfms->width > 3840 || vfms->height > 2160)
		vfms->is_not_support = true;
	else if (vfms->width > 1920 || vfms->height > 1088)
		vfms->is_4k = true;
	else if (VFM_IS_I_SRC(vfm->type))
		vfms->is_i = true;
	if (VFM_IS_VDIN_SRC(vfms->source_type))
		vfms->is_vdin = true;
	vfms->duration_overflow = dpss_is_bypass_nr(NULL, vfm);
	dbg_h1
	    ("cvs:sub_frm canvas0_cfg_size<%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
	     vfms->canvas0_config[0].width,
		 vfms->canvas0_config[0].height,
	     vfms->width,
		 vfms->height,
		 vfms->canvas0_config[0].phy_addr,
	     vfms->canvas0_config[1].phy_addr);
	dbg_h1
	    ("cvs:input_vf canvas0_cfg_size<%d,%d> <%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
	     vfm->canvas0_config[0].width,
		 vfm->canvas0_config[0].height,
	     vfm->width,
		 vfm->height,
		 vfm->compWidth,
		 vfm->compHeight,
	     vfm->canvas0_config[0].phy_addr,
		 vfm->canvas0_config[1].phy_addr);
	return true;
}

bool dpss_vfm_2_subvf_frc(struct dpss_sub_vf_s *vfms, struct vframe_s *vfm)
{
	if (!vfms || !vfm)
		return false;

	vfms->index_disp = vfm->index_disp;
	vfms->type = vfm->type;
	vfms->type_ext = vfm->type_ext;
	vfms->canvas0Addr = vfm->canvas0Addr;
	vfms->canvas1Addr = vfm->canvas1Addr;
	vfms->compHeadAddr = vfm->compHeadAddr;
	vfms->compBodyAddr = vfm->compBodyAddr;
	vfms->plane_num = vfm->plane_num;
	vfms->sig_fmt = vfm->sig_fmt;
	memcpy(&vfms->canvas0_config[0], &vfm->canvas0_config[0],
	       sizeof(vfm->canvas0_config[0]) * 2);

	vfms->width = vfm->width;
	vfms->height = vfm->height;
	if (VFM_IS_COMP_MODE(vfm->type)) {
		vfms->width = vfm->compWidth;
		vfms->height = vfm->compHeight;
		vfms->cvs_h = roundup(vfms->width, 64);
	} else {
		vfms->cvs_h = vfms->canvas0_config[0].width;
		if (vfms->canvas0_config[0].width & 0x3f)
			DBG_WARN("cvs h=%d\n", vfms->canvas0_config[0].width);
	}
	vfms->bitdepth = vfm->bitdepth;
	vfms->bitdepth_dw = vfm->bitdepth_dw;
	vfms->decontour_pre = vfm->decontour_pre;
	vfms->flag = vfm->flag;
	vfms->source_type = vfm->source_type;
	vfms->video_angle = vfm->video_angle;
	vfms->signal_type = vfm->signal_type;
	vfms->sig_fmt = vfm->sig_fmt;
	vfms->fmt = vfm->src_fmt.fmt;
	vfms->sei_magic_code = vfm->src_fmt.sei_magic_code;
	vfms->duration = vfm->duration;
//      vfms->dpss_id           = vfm->dpss_id;
	/*****************************************/
	if (vfms->width > 3840 || vfms->height > 2160)
		vfms->is_not_support = true;
	else if (vfms->width > 1920 || vfms->height > 1088)
		vfms->is_4k = true;
	else if (VFM_IS_I_SRC(vfm->type))
		vfms->is_i = true;
	if (VFM_IS_VDIN_SRC(vfms->source_type))
		vfms->is_vdin = true;
	vfms->duration_overflow = dpss_is_bypass_nr(NULL, vfm);
/****vdin afrc to dpss_frc with afrc format **********************/
	memcpy(&vfms->afrc_info, &vfm->afrc_info, sizeof(vfm->afrc_info));
	dbg_f1
	    ("cvs:sub_frm canvas0_cfg_size<%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
	     vfms->canvas0_config[0].width,
		 vfms->canvas0_config[0].height,
	     vfms->width,
		 vfms->height,
		 vfms->canvas0_config[0].phy_addr,
	     vfms->canvas0_config[1].phy_addr);
	dbg_f1
	    ("cvs:input_vf canvas0_cfg_size<%d,%d> <%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
	     vfm->canvas0_config[0].width,
		 vfm->canvas0_config[0].height,
	     vfm->width,
		 vfm->height,
		 vfm->compWidth,
		 vfm->compHeight,
	     vfm->canvas0_config[0].phy_addr,
		 vfm->canvas0_config[1].phy_addr);
	dbg_f1("frc_path_afrc:<%lx,%lx> <%lx,%lx> <0x%x,0x%x>\n",
		(ulong)vfms->afrc_info.luma_head_addr,
		(ulong)vfms->afrc_info.luma_body_addr,
		(ulong)vfms->afrc_info.chrm_head_addr,
		(ulong)vfms->afrc_info.chrm_body_addr,
		vfms->afrc_info.mmu_baddr0,
		vfms->afrc_info.mmu_baddr1);
	return true;
}

bool vfm_from_subvf(struct vframe_s *vfm, struct dpss_sub_vf_s *vfms)
{
	if (!vfms || !vfm)
		return false;

	vfm->type = vfms->type;
	vfm->canvas0Addr = vfms->canvas0Addr;
	vfm->canvas1Addr = vfms->canvas1Addr;
	vfm->compHeadAddr = vfms->compHeadAddr;
	vfm->compBodyAddr = vfms->compBodyAddr;
	vfm->plane_num = vfms->plane_num;
	memcpy(&vfm->canvas0_config[0], &vfms->canvas0_config[0],
	       sizeof(vfm->canvas0_config[0]) * 2);

	vfm->width = vfms->width;
	vfm->height = vfms->height;

	if (VFM_IS_COMP_MODE(vfms->type)) {
		vfm->compWidth = vfms->width;
		vfm->compHeight = vfms->height;
	}

	vfm->bitdepth = vfms->bitdepth;
	vfms->bitdepth_dw = vfm->bitdepth_dw;
	vfm->decontour_pre = vfms->decontour_pre;
	vfm->flag = vfms->flag;
	vfm->source_type = vfms->source_type;
	vfm->video_angle = vfms->video_angle;
	vfm->signal_type = vfms->signal_type;
	vfm->sig_fmt = vfms->sig_fmt;
	vfm->src_fmt.fmt = vfms->fmt;
	vfm->src_fmt.sei_magic_code = vfms->sei_magic_code;
	vfm->duration = vfms->duration;
	return true;
}

//subvf_2_vinfo
static void sub_vf_2_vinfo(struct dpss_vinfo_s *vt, struct dpss_sub_vf_s *vfms)
{
	if (!vt || !vfms) {
		DBG_ERR("%s:no input\n", __func__);
		return;
	}
	vt->vtype = vfms->type;
	vt->src_type = vfms->source_type;
	vt->trans_fmt = vfms->trans_fmt;

	vt->h = vfms->width;
	vt->v = vfms->height;
	vt->duration_overflow = vfms->duration_overflow;
	vt->bitdepth = vfms->bitdepth;
	if (vfms->fgs_valid && vfms->fgs_table_adr)
		vt->fg = true;
	else
		vt->fg = false;
	memcpy(&vt->canvas0_config[0], &vfms->canvas0_config[0], sizeof(vt->canvas0_config[0]) * 2);
}

/****************************************
 * compare current vframe info with last
 * return
 *	0. no change
 *	1. video format change
 *	2. scan mode change?
 ****************************************/
unsigned int dpss_sub_vf_check(struct dpss_ch_s *pch,
			       struct dpss_sub_vf_s *vfms)
{
	struct dpss_vinfo_s *vinf_l;
	struct dpss_vinfo_s *vinf_c;
	unsigned int ret;

	vinf_l = &pch->d->vf_st_lst;
	vinf_c = &pch->d->vf_st_crr;

	memcpy(vinf_l, vinf_c, sizeof(*vinf_l));
	sub_vf_2_vinfo(vinf_c, vfms);
	ret = dpss_is_vinfo_chg(vinf_c, vinf_l, true);

	if (VFM_IS_I_SRC(vfms->type))
		pch->d->is_i = 1;
	else
		pch->d->is_i = 0;
	//nr_dpe_pps_para(pch, vinf_c->h, vinf_c->v);

	dbg_i0("%s:is_pps ch=%d =%d,=%d\n", __func__,
		pch->c.ch, pch->c.prm_top.is_pps, pch->c.prm_top.is_di2pps);

	if (ret) {
		DBG_INF("%s:ch[%d]:0x%x:%d x %d:%d:%d > 0x%x:%d x %d:%d:%d.\n",
		  "input chg",
		  pch->c.ch,
		  vinf_l->vtype,
		  vinf_l->h,
		  vinf_l->v,
		  vinf_l->src_type,
		  vinf_l->bitdepth,
		  vinf_c->vtype,
		  vinf_c->h,
		  vinf_c->v,
		  vinf_c->src_type,
		  vinf_c->bitdepth);

		if (vinf_l->fg != vinf_c->fg)
			DBG_INF("\tchg fg:%d\n", vinf_c->fg);
	}
	return ret;
}

/****************************************
 * compare current vframe info with last
 * return
 *	0. no change
 *	1. video format change
 *	2. scan mode change?
 ****************************************/
static unsigned int dpss_is_vinfo_chg(struct dpss_vinfo_s *v1,	/* new */
				      struct dpss_vinfo_s *v2, bool prt_en)
{
	unsigned int ret = 0;

	if (v1->src_type != v2->src_type ||
	    !D_COM_M(DPSS_VFM_T_MASK_CHANGE, v1->vtype, v2->vtype) ||
	    v1->v != v2->v ||
	    v1->h != v2->h ||
	    v1->trans_fmt != v2->trans_fmt ||
	    v1->bitdepth != v2->bitdepth ||
	    v1->canvas0_config[0].width != v2->canvas0_config[0].width ||
	    v1->canvas0_config[0].height != v2->canvas0_config[0].height) {
		/* video format changed */
		ret = 1;
	} else if (!D_COM_M(VIDTYPE_VIU_FIELD, v1->vtype, v2->vtype)) {
		/* just scan mode changed */
		ret = 2;
	} else if (v1->duration_overflow != v2->duration_overflow) {
		/* */
		ret = 3;
	} else if (v1->fg != v2->fg) {
		ret = 4;
	}
	return ret;
}

void dpss_vfm_cp(struct vframe_s *vfm_t, struct vframe_s *vfm_f)
{
	//back
	u32 dpss_flg;
//      u32 dpss_id; //when have dpss, set this val from top
	void *dpss_data;
//      void *caller_data;

	dpss_flg = vfm_t->dpss_flg;
//      dpss_id = vfm_t->dpss_id;
	dpss_data = vfm_t->dpss_data;
//      caller_data = vfm_t->caller_data;

	memcpy(vfm_t, vfm_f, sizeof(*vfm_t));
	memcpy(&vfm_t->canvas0_config[0], &vfm_f->canvas0_config[0],
	       sizeof(vfm_t->canvas0_config[0]) << 1);
	vfm_t->dpss_flg = dpss_flg;
//      vfm_t->dpss_id = dpss_id;
	vfm_t->dpss_data = dpss_data;
//      vfm_t->caller_data = caller_data;

	dbg_h1("vf_frc:%p cvs:sub_frm canvas0_cfg_size<%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
	     (void *)vfm_t, vfm_t->canvas0_config[0].width,
	     vfm_t->canvas0_config[0].height, vfm_t->width, vfm_t->height,
	     vfm_t->canvas0_config[0].phy_addr,
	     vfm_t->canvas0_config[1].phy_addr);
	dbg_h1("vf_in:%p cvs:input_vf canvas0_cfg_size<%d,%d> <%d,%d> <%d,%d> addr<0x%lx,0x%lx>\n",
	     (void *)vfm_f, vfm_f->canvas0_config[0].width,
	     vfm_f->canvas0_config[0].height, vfm_f->width, vfm_f->height,
	     vfm_f->compWidth, vfm_f->compHeight,
	     vfm_f->canvas0_config[0].phy_addr,
	     vfm_f->canvas0_config[1].phy_addr);
}

static bool is_high_fps_src(struct vframe_s *vfm_f)
{
	if (!vfm_f)
		return false;

	u32 vfm_width = vfm_f->compWidth > vfm_f->width ? vfm_f->compWidth : vfm_f->width;
	u32 vfm_height = vfm_f->compHeight > vfm_f->height ? vfm_f->compHeight : vfm_f->height;

	if (vfm_f->duration < 800 && (vfm_width >= 1920 || vfm_height >= 1080)) {
		dbg_h1("%s:vf->duration:%d.", __func__, vfm_f->duration);
		return true;
	}

	return false;
}

/* common function **********************************/
bool dpss_s_prob(struct dpss_dev_s *dpss_pdev)
{
	struct dpss_ch_s *pch = dpss_get_ch(0);
	/**/
	itf_vfm_path_init();
	dpss_buf_infor(pch);
	dpss_buf_infor_4k(pch);
	dpss_pch_init();
	dpss_pre_buf_prob();

	return 0;
}

void dpss_cfg_top_dts(struct dpss_dev_s *dpss_pdev)
{
}

void dpss_mem_prob(struct dpss_dev_s *dpss_pdev)
{
}

//
void dpss_s_remove(struct dpss_dev_s *dpss_pdev)
{
	itf_vfm_path_exit();
}

void dpss_mem_remove(struct dpss_dev_s *dpss_pdev)
{
}

void dpss_exit(void)
{
}

void dpss_s_shutdown(struct dpss_dev_s *dpss_pdev)
{
}

void dpss_s_suspend(struct dpss_dev_s *dpss_pdev)
{
	unsigned int ch = 0;
	struct dpss_ch_s *pch;

	for (ch = 0; ch < DPSS_CHANNEL_NUB; ch++) {
		pch = dpss_get_ch(ch);
		if (pch->c.reg_s1) {
			DBG_WARN("dpss ins no destroy ch:%d\n", ch);
			return;
		}
	}
#ifdef FUNC_EN_PQ
	pq_update_suspend_flag(1);
#endif
	if (dpss_pdev->clk_status) {
		DBG_INF("dpss suspend start close clk\n");
		clk_set_rate(dpss_pdev->vpu_clk_dpe, 0);
		clk_disable_unprepare(dpss_pdev->vpu_clk_dae);
		dpss_pdev->clk_status &= ~(1 << 0);
		clk_set_rate(dpss_pdev->vpu_clk_dae, 0);
		clk_disable_unprepare(dpss_pdev->vpu_clk_dpe);
		dpss_pdev->clk_status &= ~(1 << 1);
	}
	dbg_s1("%s ok\n", __func__);
}

void dpss_s_resume(struct dpss_dev_s *dpss_pdev)
{
	if (!dpss_pdev->clk_status) {
		clk_prepare_enable(dpss_pdev->vpu_clk_dae);
		clk_set_rate(dpss_pdev->vpu_clk_dae, 50000000);
		dpss_pdev->clk_status |= (1 << 0);

		clk_prepare_enable(dpss_pdev->vpu_clk_dpe);
		clk_set_rate(dpss_pdev->vpu_clk_dpe, 50000000);
		dpss_pdev->clk_status |= (1 << 1);
	}

	dpss_irq_set_affinity_hint(dpss_pdev);

	dbg_s1("cur dpe clk :0x%lx\n", clk_get_rate(dpss_pdev->vpu_clk_dpe));
	dbg_s1("cur dae clk :0x%lx\n", clk_get_rate(dpss_pdev->vpu_clk_dae));
	dbg_s1("%s ok\n", __func__);
}

void dpss_s_freeze(struct dpss_dev_s *dpss_pdev)
{
	unsigned int ch = 0;
	struct dpss_ch_s *pch;

	for (ch = 0; ch < DPSS_CHANNEL_NUB; ch++) {
		pch = dpss_get_ch(ch);
		if (pch->c.reg_s1) {
			DBG_WARN("dpss ins no destroy ch:%d\n", ch);
			return;
		}
	}
#ifdef FUNC_EN_PQ
	pq_update_suspend_flag(1);
#endif
	dbg_s1("dpss freeze start close clk\n");
	if (dpss_pdev->clk_status) {
		clk_set_rate(dpss_pdev->vpu_clk_dpe, 0);
		clk_disable_unprepare(dpss_pdev->vpu_clk_dae);
		dpss_pdev->clk_status &= ~(1 << 0);
		clk_set_rate(dpss_pdev->vpu_clk_dae, 0);
		clk_disable_unprepare(dpss_pdev->vpu_clk_dpe);
		dpss_pdev->clk_status &= ~(1 << 1);
	}
	dbg_s1("%s ok\n", __func__);
}

void dpss_s_restore(struct dpss_dev_s *dpss_pdev)
{
	if (!dpss_pdev->clk_status) {
		clk_prepare_enable(dpss_pdev->vpu_clk_dae);
		clk_set_rate(dpss_pdev->vpu_clk_dae, 50000000);
		dpss_pdev->clk_status |= (1 << 0);

		clk_prepare_enable(dpss_pdev->vpu_clk_dpe);
		clk_set_rate(dpss_pdev->vpu_clk_dpe, 50000000);
		dpss_pdev->clk_status |= (1 << 1);
	}

	dpss_irq_set_affinity_hint(dpss_pdev);

	dbg_s1("cur dpe clk :0x%lx\n", clk_get_rate(dpss_pdev->vpu_clk_dpe));
	dbg_s1("cur dae clk :0x%lx\n", clk_get_rate(dpss_pdev->vpu_clk_dae));
	dbg_s1("%s ok\n", __func__);
}

void dpss_irq_set_affinity_hint(struct dpss_dev_s *dpss_pdev)
{
	u32 i;

	if (!dpss_pdev)
		return;

	for (i = 0; i < DPSS_IRQ_ID_NUB; i++) {
		if (i == 0)
			irq_set_affinity_hint(dpss_pdev->irq_q[i], cpumask_of(3));
		else
			irq_set_affinity_hint(dpss_pdev->irq_q[i], cpumask_of(2));
	}
}

void dpss_s_thaw(struct dpss_dev_s *dpss_pdev)
{
}

#ifdef C_TST_ON_T6D
irqreturn_t dpss_pre_irq(int irq, void *dev_instance)
{
	return IRQ_HANDLED;
}

irqreturn_t dpss_post_irq(int irq, void *dev_instance)
{
	return IRQ_HANDLED;
}

irqreturn_t dpss_aisr_irq(int irq, void *dev_instance)
{
	return IRQ_HANDLED;
}
#endif				/* C_TST_ON_T6D */

#ifdef FUNC_EN_IRQ

irqreturn_t dpss_irq_pre_vs(int irq, void *dev_instance)
{
	irq_pre_vs();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_buf_rls1(int irq, void *dev_instance)
{
	dbg_irq0("irq:%s:\n", "rls1");
	hw_vdi_rls_int();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_buf_rls0(int irq, void *dev_instance)
{
	dbg_irq0("irq:%s:\n", "rls0");
	hw_rls0_irq();

	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_inp(int irq, void *dev_instance)
{
	dbg_irq1("irq:%s:\n", "inp");
	irq_inp();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_dae2(int irq, void *dev_instance)
{
	dbg_irq0("irq:%s:\n", "dae2");
	irq_dae2();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_dae1(int irq, void *dev_instance)	//
{
	dbg_irq0("irq:%s:\n", "dae1");
	irq_dae1();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_dae0(int irq, void *dev_instance)
{
	dbg_irq1("irq:%s:\n", "dae0");
	irq_dae0();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_dpe2(int irq, void *dev_instance)
{
	dbg_irq0("irq:%s:\n", "dpe2");
	irq_dpe2();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_dpe1(int irq, void *dev_instance)	//
{
	dbg_irq0("irq:%s:\n", "dpe1");
	irq_dpe1();
	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_dpe0(int irq, void *dev_instance)
{
	dbg_w2("irq:%s:\n", "dpe0");

	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_rdma(int irq, void *dev_instance)
{
	dbg_w2("irq:%s:\n", "rdma");
#ifdef USE_FRC_PRE_VS_RDMA
	irq_rdma();
#else
	dpss_rdma_isr();
#endif

	return IRQ_HANDLED;
}

irqreturn_t dpss_irq_dbg(int irq, void *dev_instance)
{
	unsigned int dbg;

	dbg = rd(DPSS_APB_STAT_INFO);
	DBG_WARN("irq:%s:0x%x, 0x%x\n", "dbg", dbg, dbg & 0xffff);

	return IRQ_HANDLED;
}
#endif

void dpss_tsk_s_polling(void)
{
}

void dpss_tsk_m_polling(void)
{
	unsigned int ch;
	struct dpss_ch_s *pch;

	for (ch = 0; ch < DPSS_CHANNEL_NUB; ch++) {
		pch = dpss_get_ch(ch);
		if (pch->c.reg_s2 && !atomic_read(&pch->c.trig_unreg)) {
			/* normal work */
			dpss_s2_work(pch);
		} else if (pch->c.reg_s2 && atomic_read(&pch->c.trig_unreg)) {
			if (!pch->c.unreg_s1) {
				dpss_s_unreg_step1(pch);
				pch->c.unreg_s1 = true;
			}
			if (dpss_s_unreg_wait_hw_stop(pch)) {
				frc_unreg_clear_state(ch);
				dpss_s_unreg_step2(pch);
				pch->c.unreg_s1 = 0;
				pch->c.reg_s2 = 0;
				/* destroy function wait this signal */
				atomic_set(&pch->c.trig_unreg, 0);
			}
			//wait....
		} else if (pch->c.reg_s1 && atomic_read(&pch->c.trig_reg)) {
			//mutex_lock(&pch->c.ch_lock); //lock
			if (pch->c.reg_s1 &&
			    !pch->c.reg_s2 && atomic_read(&pch->c.trig_reg)) {
				dpss_s_reg(pch);	//mutex lock
				pch->c.reg_s2 = 1;
				atomic_set(&pch->c.trig_reg, 0);
				frc_reg_update_state(ch);
			}
			//mutex_unlock(&pch->c.ch_lock);        //unlock
		}
		//do nothing...
	}

#ifdef DBG_TEST_CREATE
	dbg_crt_do_polling();
#endif
}

unsigned int g_dpss_crr_ch;	//tmp
struct dpss_ch_s *dpss_get_crr_ch(void)
{
	struct dpss_ch_s *pch = NULL;

	if (g_dpss_crr_ch < DPSS_CHANNEL_MAX)
		pch = dpss_get_ch(g_dpss_crr_ch);
	return pch;
}

bool g_dpss_dbg_sent_fst_vpp;	//tmp

//==================================================================================
//lk:

//==================================================================================
#ifdef REF_CODE
static unsigned int dpss_sub_support_nr(struct dpss_ch_s *pch,
					struct vframe_s *vfm)
{
	bool support = false;

	if (!(pch->c.work_mode_cfg & D_W_B(NR)))
		return 0;

	//to-do ....

	if (support)
		return D_W_B(NR);

	return 0;
}

#endif				/*REF_CODE */

static unsigned int dpss_sub_support_null(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	if (!pch->c.ch && dpss_dbg_bypss & C_BIT0)
		return D_W_B(BP);

	if (pch->c.ch && dpss_dbg_bypss & C_BIT1)
		return D_W_B(BP);
	if (pch->d->bypass_rs)
		return D_W_B(BP);
	if (pch->d->di_front) {
		if (!VFM_IS_I_SRC(vfm->type) ||
		   (vfm->type & VIDTYPE_V4L_EOS))
			return D_W_B(BP);
	}
	return 0;
}

static unsigned int dpss_sub_support_nr(struct dpss_ch_s *pch,
					struct vframe_s *vfm)
{
	bool support = false;

	if (pch->c.parm.dps_work_mode == DPSS_WORK_MODE_FRC)
		return 0;

	if (!(pch->c.work_mode_cfg & D_W_B(NR))) {
		dbg_i0("%s:ch[%d]:0x%x, 0x%x\n", __func__, pch->c.ch,
			pch->c.work_mode_cfg, D_W_B(NR));
		return D_W_B(BP);
	}

	if (vfm->height > 2160 || vfm->width > 3840 ||
		vfm->height < 10 || vfm->width < 64) {	//tmp
		support = false;
		dbg_i0("%s:ch[%d]:<%d,%d>\n", __func__, pch->c.ch, vfm->height, vfm->width);
	} else {
		support = true;
	}

	if (support)
		return D_W_B(NR);

	return D_W_B(BP);
}

static unsigned int dpss_sub_support_di(struct dpss_ch_s *pch,
					struct vframe_s *vfm)
{
	bool support = false;

	if (pch->c.ch == 0) //only for ch1, tmp
		return 0;
	if (!(pch->c.work_mode_cfg & D_W_B(DI)))
		return 0;
	if (VFM_IS_I_SRC(vfm->type) &&
	    vfm->width <= 1920 && vfm->height <= 1080)
		support = true;

	if (support)
		return D_W_B(DI);
	return 0;
}

static unsigned int dpss_sub_support_dct(struct dpss_ch_s *pch,
					 struct vframe_s *vfm)
{
	//marked for coverity
	//bool support = false;

	if (!(pch->c.work_mode_cfg & D_W_B(DCT)))
		return 0;
	//to-do
	//if (support)
		//return D_W_B(DCT);
	return 0;
}

static unsigned int dpss_sub_support_levc(struct dpss_ch_s *pch,
					  struct vframe_s *vfm)
{
	//marked for coverity
	//bool support = false;

	if (!(pch->c.work_mode_cfg & D_W_B(LEVC)))
		return 0;
	//to-do
	//if (support)
		//return D_W_B(LEVC);
	return 0;
}

static unsigned int dpss_sub_support_frc(struct dpss_ch_s *pch,
					 struct vframe_s *vfm)
{
	bool support = false;

	if (!(pch->c.work_mode_cfg & D_W_B(FRC)))
		return 0;
	if (pch->c.parm.dps_work_mode != DPSS_WORK_MODE_FRC)
		return 0;
	//to-do
	if (vfm->height > 2160 || vfm->width > 3840 ||
		vfm->height < 10 || vfm->width < 64) {	//tmp
		support = false;
		dbg_i0("%s:<%d,%d>\n", __func__, vfm->height, vfm->width);
	} else {
		support = true;
	}
	if (support)
		return D_W_B(FRC);
	return 0;
}

typedef unsigned int (*FUNC_M_SUPPORT)(struct dpss_ch_s *pch,
				       struct vframe_s *vfm);

//must keep order with EDPSS_W_MODE_B:
FUNC_M_SUPPORT module_support_tab[] = {
	dpss_sub_support_null,
	dpss_sub_support_nr,
	dpss_sub_support_di,
	dpss_sub_support_dct,
	dpss_sub_support_hdr,
	dpss_sub_support_ddd,
	dpss_sub_support_frc,
	dpss_sub_support_levc,
};

static unsigned int dpss_sub_chg_null(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	return 0;
}

unsigned int dpss_sub_chg_nr(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	unsigned int chg = 0;
	struct dpss_nr_i_s *nr_i;
	bool tmp;

	if (pch->c.parm.dps_work_mode != DPSS_WORK_MODE_FRC) {
		nr_i = lk_get_nr_i(vfm);
		if (!nr_i) {
			DBG_ERR("%s nr_i is null (vfm :%p)\n", __func__, vfm);
			return 0;
		}
		dpss_vfm_2_subvf(&nr_i->sub_vf_in, vfm);
		chg = dpss_sub_vf_check(pch, &nr_i->sub_vf_in);
		if (!chg) {
			if ((vfm->dpss_flg & DPSS_FLG_NEW_TRIG) ||
			    (dpss_dbg_bypss & C_BIT8)) {
				chg = 10;
				DBG_INF("%s:ch[%d]:new trig\n",
					"input chg", pch->c.ch);
			}
		}

		if (pch->c.ch) {
			if (chg)
				return D_W_B(NR);
			return 0;
		}
		//----for dv cp
		if (pch->d->new_w_mode &&
		    ((pch->d->new_w_mode & DPSS_DD_CP_MODE) !=
		    (pch->c.parm.dps_work_mode & DPSS_DD_CP_MODE))) {
			if (pch->d->new_w_mode & DPSS_DD_CP_MODE) {
				nr_bypass_test |= C_BIT4; //tmp
				pch->c.parm.dps_work_mode |= DPSS_DD_CP_MODE;
			} else {
				nr_bypass_test &= ~C_BIT4;
				pch->c.parm.dps_work_mode &= (~DPSS_DD_CP_MODE);
			}
			chg = 0x10;
			DBG_INF("%s:0x%x:%d\n",
				__func__,
				pch->c.parm.dps_work_mode,
				nr_bypass_test);
		} else if (pch->d->new_w_mode) {
			dbg_i1("%s:new_w_mode=0x%x\n", __func__, pch->d->new_w_mode);
		} else {
			tmp = nr_bypass_test & C_BIT4;
			if (!tmp &&
			    (pch->c.parm.dps_work_mode & DPSS_DD_CP_MODE))
				nr_bypass_test |= C_BIT4;
			else if (tmp &&
			    !(pch->c.parm.dps_work_mode & DPSS_DD_CP_MODE))
				nr_bypass_test &= ~C_BIT4;
			if (tmp != (nr_bypass_test & C_BIT4)) {
				dbg_i0("%s:dv cp2:%d -> %d\n",
					__func__, tmp, nr_bypass_test);
			}
		}
	}
	if (chg)
		return D_W_B(NR);
	return 0;
}

static unsigned int dpss_sub_chg_di(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	//marked for coverity
	//unsigned int chg = 0;

	//if (chg)
		//return D_W_B(DI);
	return 0;
}

static unsigned int dpss_sub_chg_dct(struct dpss_ch_s *pch,
				     struct vframe_s *vfm)
{
	//marked for coverity
	//unsigned int chg = 0;

	//if (chg)
		//return D_W_B(DCT);
	return 0;
}

static unsigned int dpss_sub_chg_levc(struct dpss_ch_s *pch,
				      struct vframe_s *vfm)
{
	//marked for coverity
	//unsigned int chg = 0;

	//if (chg)
		//return D_W_B(LEVC);
	return 0;
}

unsigned int dpss_sub_chg_frc(struct dpss_ch_s *pch,
				     struct vframe_s *vfm)
{
	unsigned int chg = 0;
	struct dpss_frc_i_s *frc_i;

	if (pch->c.parm.dps_work_mode == DPSS_WORK_MODE_FRC) {
		frc_i = lk_get_frc_i(vfm);
		if (!frc_i) {
			DBG_ERR("%s frc_i is null (vfm :%p)\n", __func__, vfm);
			return 0;
		}
		dpss_vfm_2_subvf_frc(&frc_i->sub_vf_in, vfm);
		chg = dpss_sub_vf_check(pch, &frc_i->sub_vf_in);
	}
	if (chg)
		return D_W_B(FRC);

	return 0;
}

//must keep order with EDPSS_W_MODE_B:
FUNC_M_SUPPORT module_chg_tab[] = {
	dpss_sub_chg_null,
	dpss_sub_chg_nr,
	dpss_sub_chg_di,
	dpss_sub_chg_dct,
	dpss_sub_chg_hdr,
	dpss_sub_chg_ddd,
	dpss_sub_chg_frc,
	dpss_sub_chg_levc,
};

//count w mode
//count chg or not
static unsigned int dpss_work_mode_count(struct dpss_ch_s *pch,
					 struct vframe_s *vfm)
{
	int i;
	unsigned int cnt;
	unsigned int mode_t = 0, mode;

	cnt = EDPSS_W_MODE_B_LEVC + 1;

	for (i = 0; i < cnt; i++) {
		mode = module_support_tab[i] (pch, vfm);
		//dbg_i2("%s:%d,0x%x\n", __func__, i, mode);
		mode_t |= mode;
	}
	if (mode_t)
		dbg_i2("mode_t=0x%x\n", mode_t);
	return mode_t;
}

static unsigned int dpss_m_chg_count(struct dpss_ch_s *pch,
				     struct vframe_s *vfm)
{
	int i;
	unsigned int cnt;
	unsigned int mode_t = 0, mode;

	cnt = EDPSS_W_MODE_B_LEVC + 1;

	for (i = 0; i < cnt; i++) {
		mode = module_chg_tab[i] (pch, vfm);
		//dbg_i2("%s:%d,0x%x\n", __func__, i, mode);
		mode_t |= mode;
	}
	if (mode_t)
		dbg_i2("%s:0x%x\n", __func__, mode_t);
	return mode_t;
}

static void dpss_work_mode_cfg_count(struct dpss_ch_s *pch)
{
	unsigned int mode, mode_c = 0;

	mode = pch->c.parm.dps_work_mode;

	if (pch->c.parm.dps_work_mode & DPSS_WORK_MODE_BY_DPSS) {
		if (pch->c.ch == 1)
			pch->c.work_mode_cfg = D_W_B(DI);
		else
			pch->c.work_mode_cfg = DPSS_W_MODE_T_FULL;
	} else {
		if (mode & DPSS_WORK_MODE_NR)
			mode_c = mode_c | D_W_B(NR);
		if (mode & DPSS_WORK_MODE_DDD)
			mode_c = mode_c | D_W_B(DDD);
		if (mode & DPSS_WORK_MODE_LC_EVC)
			mode_c = mode_c | D_W_B(LEVC);
		if (mode & DPSS_WORK_MODE_DCT)
			mode_c = mode_c | D_W_B(DCT);
		if (mode & DPSS_WORK_MODE_FRC)
			mode_c = mode_c | D_W_B(FRC);
		if (mode & DPSS_WORK_MODE_HDR)
			mode_c = mode_c | D_W_B(HDR);
		if (mode & DPSS_WORK_MODE_DI)
			mode_c = mode_c | D_W_B(DI);

		pch->c.work_mode_cfg = mode_c;
	}

	dbg_ins0("ch[%d]:work mode cfg:0x%x\n", pch->c.ch, pch->c.work_mode_cfg);
}

#define DPSS_NR_NO_FREE		(0xff)

static unsigned int dpss_h_b_buf_get_free(struct dpss_ch_s *pch)
{
	unsigned int idx = DPSS_NR_NO_FREE;
	//to-do
	//0 ~ 6?
	unsigned int free_nub;
	int diff;
	struct dpss_h_nr_mng_s *mng;

	//check
	diff = pch->c.in_cnt - pch->c.out_cnt;
	if (diff >= pch->c.in_b_nub) {
		DBG_ERR("%s:%d,%d,%d,0x%x,0x%x\n", __func__, diff, pch->c.in_cnt,
			pch->c.out_cnt, rd(DPSS_FBUF_PROC_STATUS),
			rd(DPSS_FRC_PROC_STATUS));
		return DPSS_NR_NO_FREE;
	} else if (diff >= pch->c.in_b_nub - 1) {
		DBG_WARN("%s:%d,%d,%d,0x%x,0x%x\n", __func__, diff, pch->c.in_cnt,
			pch->c.out_cnt, rd(DPSS_FBUF_PROC_STATUS),
			rd(DPSS_FRC_PROC_STATUS));
	}
	free_nub = pch->c.in_b_nub - diff;
	if (free_nub == 0) {
		dbg_ins2("%s:ch[%d]:1:%d %d %d\n", __func__, pch->c.ch, diff, pch->c.in_cnt,
			 pch->c.out_cnt);
		return DPSS_NR_NO_FREE;
	}
	idx = pch->c.in_cnt % pch->c.in_b_nub;
	mng = &pch->d->h_nr_mng[idx];
	if (!mng->active) {
		dbg_ins2("%s:ch[%d]:2:not active:%d\n", __func__, pch->c.ch, idx);
		return DPSS_NR_NO_FREE;
	}
	if (mng->used) {
		DBG_ERR("%s:b_buf:%d, is used?\n", __func__, idx);
		return DPSS_NR_NO_FREE;
	}
	return idx;
}

//nub need work by hw
unsigned int dpss_h_b_buf_get_work(struct dpss_ch_s *pch)
{
	//return (g_dpss_in_cnt - g_dpss_out_cnt);
	int i = 0;
	struct dpss_h_nr_mng_s *mng;
	unsigned int cnt = 0;

	for (i = 0; i < pch->c.in_b_nub; i++) {
		mng = &pch->d->h_nr_mng[i];
		if (mng->active && mng->used)
			cnt++;
	}
	dbg_i2("%s:work nub:%d\n", __func__, cnt);
	return cnt;
}

static void dpss_h_b_buf_get(struct dpss_ch_s *pch, unsigned char idx,
			     struct vframe_s *vfm)
{
	struct dpss_h_nr_mng_s *mng;

	mng = &pch->d->h_nr_mng[idx];
	if (!mng->active || mng->r_b_idx == DPSS_NR_NO_FREE || mng->used)
		DBG_WARN("%s:%d,0x%x,%d\n", __func__, mng->active, mng->r_b_idx,
			 mng->used);
	mng->used = true;

	mng->vf = vfm;
	dbg_ins2("%s:ch[%d]:%d:%p:%p\n", "h_b:get", pch->c.ch, idx, vfm, vfm->dpss_data);
}

static struct vframe_s *dpss_h_b_buf_put(struct dpss_ch_s *pch,
					 unsigned char idx)
{
	struct dpss_h_nr_mng_s *mng;
	struct vframe_s *vfm;

	mng = &pch->d->h_nr_mng[idx];
	if (!mng->active || mng->r_b_idx == DPSS_NR_NO_FREE || !mng->used) {
		DBG_WARN("%s:%d,0x%x,%d\n", __func__, mng->active, mng->r_b_idx,
			 mng->used);
		return NULL;
	}
	mng->used = false;
	mng->vf_idx = DPSS_NR_NO_FREE;
	vfm = mng->vf;
	mng->vf = NULL;
	dbg_ins2("%s:ch[%d]:%d:%p:%p\n", "h_b:put", pch->c.ch, idx, vfm, vfm->dpss_data);
	return vfm;
}

static unsigned char dpss_h_buf_get_b_idx(struct dpss_ch_s *pch,
					  unsigned char idx)
{
	struct dpss_h_nr_mng_s *mng;

	mng = &pch->d->h_nr_mng[idx];
	return mng->r_b_idx;
}

void dpss_h_b_buf_add(struct dpss_ch_s *pch, unsigned char m_idx,
		      unsigned char buf_idx)
{
	struct dpss_h_nr_mng_s *mng;

	mng = &pch->d->h_nr_mng[m_idx];
	mng->r_b_idx = buf_idx;
	mng->active = true;
	mng->used = false;
}

unsigned char dpss_h_b_buf_del(struct dpss_ch_s *pch, unsigned char m_idx)
{
	struct dpss_h_nr_mng_s *mng;

	mng = &pch->d->h_nr_mng[m_idx];
	mng->r_b_idx = DPSS_NR_NO_FREE;
	mng->active = false;
	return 0;
}

//depend on buffer has alloc
void dpss_h_b_int(struct dpss_ch_s *pch)	//tmp
{
	int i;
	struct dpss_h_nr_mng_s *mng;

	for (i = 0; i < DPSS_VFM_IN_NUB; i++) {
		mng = &pch->d->h_nr_mng[i];
		memset(mng, 0, sizeof(*mng));
		if (i < pch->c.in_b_nub)
			dpss_h_b_buf_add(pch, i, i);
		else
			dpss_h_b_buf_del(pch, i);
	}
}

//===========================================================
void dpss_reg_val(struct dpss_ch_s *pch)
{
	int i;
	struct dpss_dd_s *dd;

	unsigned char cfg_num_in = 0, cfg_num_o = 0, cfg_num_pq = 0;

	dd = dpss_get_dd();
	if (!pch || !dd) {
		DBG_WARN("%s:no pch or dd\n", __func__);
		return;
	}
	//secure:
	if (pch->c.etype) {
		if (pch->c.parm.di_parm.output_format & DPSS_OUTPUT_TVP)
			pch->d->is_secure = true;
		else
			pch->d->is_secure = false;
	} else { //vframe path to-do
		DBG_WARN("secure for vfm path:to-do\n");
	}
	if (dpss_dbg_secure)
		pch->d->is_secure = true;
	dbg_i0("is_secure:%d\n", pch->d->is_secure);

	cfg_num_in = dpss_cfg_num_in(pch->c.ch);
	cfg_num_o = dpss_cfg_num_out(pch->c.ch);

	if (pch->c.parm.dps_work_mode != DPSS_WORK_MODE_FRC) {
		pch->c.in_b_nub = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
		pch->c.o_b_nub = DPSS_HW_LOOP_IN_OUT_BUF_NUB;

		if (cfg_num_in && cfg_num_in < DPSS_HW_LOOP_IN_OUT_BUF_NUB)
			pch->c.in_b_nub = cfg_num_in;
		if (cfg_num_o && cfg_num_o < DPSS_HW_LOOP_IN_OUT_BUF_NUB)
			pch->c.o_b_nub = cfg_num_o;
	} else {
		pch->c.in_b_nub = 16;
		pch->c.o_b_nub = 16;
	}
	dbg_i0("in_b_nub=%d o = %d\n", pch->c.in_b_nub, pch->c.o_b_nub);

	pch->c.cnt_in = 0;
	pch->c.cnt_proce = 0;
	pch->c.cnt_dpe_rd_cp = 0;
	pch->c.rdma_handle_dae = -1;
	pch->c.rdma_handle_dpe[0] = -1;
	pch->c.rdma_handle_dpe[1] = -1;
	pch->c.in_cnt = 0;
	pch->c.out_cnt = 0;
	pch->c.in_total_cnt = 0;
	pch->c.out_total_cnt = 0;
	pch->c.disp_cnt = 0;
	pch->c.last_chg = false;

	pch->d->idx_hd = 1;
	for (i = 0; i < DPSS_VFM_IN_NUB; i++)
		pch->d->h_dae_vf_idx[i] = 0xff;

	dpss_work_mode_cfg_count(pch);

	for (i = 0; i < DPSS_HW_LOOP_IN_OUT_BUF_NUB; i++) {
		pch->c.in_state[i] = 0;
		pch->c.o_state[i] = 0;
	}
	g_dpss_crr_ch = pch->c.ch;
	g_dpss_dbg_sent_fst_vpp = false;

	if (pch->c.ch == 0)
		resolution_switch_count = 0;

	pch->d->afrc_bs_y = dd->afrc_bs_y;
	pch->d->afrc_bs_c = dd->afrc_bs_c;
	dbg_i2("afrc:bs <%d> <%d>\n", pch->d->afrc_bs_y, pch->d->afrc_bs_c);
//------
	pch->c.en_dct = true;
	pch->c.en_tfbc = true;
	pch->c.en_dw = true;
	pch->c.en_frc = true;
	//num for mem
//	pch->c.num_dpe_o = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
	if (dpss_nr_debug)
		pch->c.num_lc = DPSS_HW_LOOP_IN_OUT_BUF_NUB; //
	else
		pch->c.num_lc = 4;
	pch->c.num_aepe = nr_aepe_buf_num_dbg;
	pch->c.num_nr_wrpt = 3;
	pch->c.num_pq_buf = pch->c.o_b_nub;
	cfg_num_pq = dpss_cfg_num_pq(pch->c.ch);

	if (cfg_num_pq && cfg_num_pq <= DPSS_HW_LOOP_IN_OUT_BUF_NUB) {
		pch->c.num_pq_buf = cfg_num_pq;
		dbg_i0("num_pq_buf = %d\n", pch->c.num_pq_buf);
	}

	if (pch->d->di_front) {
		pch->c.en_dw = false;
		pch->c.en_frc = false;
		pch->c.support_i = true;
		pch->c.support_4k = false;
		pch->c.mem_support = 0;//C_BIT2;
	} else if (!pch->c.ch) {
		if (!(pch->c.parm.dps_work_mode & DPSS_WORK_MODE_FRC))
			pch->c.en_frc = false;
		if (pch->c.etype) {
			if (pch->c.parm.di_parm.is_interlace) {
				pch->c.support_i = true;
				pch->c.support_4k = false;
			} else {
				pch->c.support_i = false;
				pch->c.support_4k = true;
			}
		} else {
			//vfm path: tmp:
			pch->c.support_4k = true;
			pch->c.support_i = true;
		}
		if (pch->c.support_4k)
			pch->c.mem_support = C_BIT2 | C_BIT5;
		else
			pch->c.mem_support = C_BIT2;
		if (dpss_mem_support & C_BIT15)
			pch->c.mem_support = (unsigned char)(dpss_mem_support & 0xff);
	} else {
		pch->c.en_dw = false;
		pch->c.en_frc = false;
		pch->c.support_4k = false;
		pch->c.support_i = true;
		pch->c.mem_support = C_BIT2;
		if (dpss_mem_support & C_BIT31)
			pch->c.mem_support =
			(unsigned char)((dpss_mem_support >> 16) & 0xff);
	}
	if (!pch->c.support_i && !dpss_lc_buf && !dpss_nr_debug)
		pch->c.num_lc = 0;

	dbg_m0("%s:mem_support:0x%x\n", __func__, pch->c.mem_support);
//------

	if (pch->c.ch == 0) {
		dpss_get_hw()->ch_act |= C_BIT0;
		dpss_get_hw()->st_idle |= C_BIT0;
		dpss_get_hw()->need_hold &= ~C_BIT0;
	} else {
		dpss_get_hw()->ch_act |= C_BIT1;
		dpss_get_hw()->st_idle |= C_BIT1;
		dpss_get_hw()->need_hold &= ~C_BIT1;
	}

	dbg_i0("%s:ch[%d]\n", __func__, pch->c.ch);
}

bool dpss_is_h_first_ch(struct dpss_ch_s *pch)
{
	bool ret;

	if (pch->c.ch == 0 && (dpss_get_hw()->src_act & C_BIT1))
		ret = false;
	else  if (pch->c.ch == 1 && (dpss_get_hw()->src_act & C_BIT0))
		ret = false;
	else
		ret = true;

	dbg_i0("%s:ch[%d], src_act = 0x%x, %d\n", __func__,
		pch->c.ch, dpss_get_hw()->src_act, ret);
	return ret;
}

void dpss_trig_mem_alloc_di(struct dpss_ch_s *pch)
{
}

void dpss_trig_mem_release_di(struct dpss_ch_s *pch)
{
}

void dpss_s_reg(struct dpss_ch_s *pch)
{
	if (!pch) {
		DBG_ERR("%s: NULL param.\n", __func__);
		return;
	}

	if (!pch->d) {
		DBG_ERR("%s:ch[%d]\n", __func__, pch->c.ch);
		return;
	}
	dbg_i2("%s:ch[%d]\n", __func__, pch->c.ch);
	dpss_reg_val(pch);
	if (pch->c.parm.di_parm.buffer_mode == DPSS_BUFFER_MODE_ALLOC_FAST)
		dpss_trig_mem_alloc_di(pch);

	dpss_rdma_pre_tab_reg(pch);
	dpss_buf_reg(pch);	//tmp
	dpss_ram_tab_init(pch);
	dpss_h_b_int(pch);
	dpss_tsk_m_wake(20);
	dpss_check_st(pch, 0x01);
	dbg_i2("%s:ch[%d] end\n", __func__, pch->c.ch);
}

/* unreg_step 1: trig internal unreg flow. */
void dpss_s_unreg_step1(struct dpss_ch_s *pch)
{
	dbg_i0("%s:ch[%d]\n", __func__, pch->c.ch);
	nr_unreg_hw(pch);
	frc_unreg_hw(pch->c.ch);
}

bool dpss_s_unreg_wait_hw_stop(struct dpss_ch_s *pch)
{
	return true;
}

void dpss_s_print_inf(unsigned int ch)	//debug only
{
	struct dpss_ch_s *pch;

	pch = dpss_get_ch(ch);
	if (!pch)
		return;

	dbg_i1("ch[%d]:cnt_pre:get[%d](%d), put[%d]\n", pch->c.ch,
	       pch->d->cnt_pre_get, pch->d->cnt_in, pch->d->cnt_pre_put);
	dbg_i1("cnt_pst:get[%d], put[%d]\n",
	       pch->d->cnt_pst_get, pch->d->cnt_pst_put);
	dbg_i1("cnt_parser:%d, hw:%d\n", pch->d->cnt_parser_in,
	       pch->d->cnt_h_in);
}

void dpss_s_unreg_step2(struct dpss_ch_s *pch)
{
	dbg_i0("%s:ch[%d]\n", __func__, pch->c.ch);

	dpss_rdma_pre_tab_unreg(pch);
	dpss_trig_mem_release_di(pch);
	dpss_buf_unreg(pch);
	if (!pch->c.ch) { // 2026-02-05
#ifdef FUNC_EN_DD
		dpss_dd_disable();
#endif
#ifdef FUNC_EN_HDR
		w_reg_bit(VPU_VBE_TOP_HDR_CTRL, 3, 0, 2);
		dpss_hdr_disable();
#endif
	}
	dpss_s_print_inf(pch->c.ch);
#ifdef _HIS_CODE_	//ary
	//dpss-patch for reg
	vfree(pch->d);
	pch->d = NULL;
	dbg_i0("\t:clr d\n");
#endif
}

//work when reg_s2
void dpss_s2_work(struct dpss_ch_s *pch)
{
	if (!pch)
		return;
	if (pch->c.parm.dps_work_mode != DPSS_WORK_MODE_FRC) {
		if (!pch->d->di_front)
			dpss_s2_recycle_back(pch);
		//itf_polling_in_o(pch);
		dpss_s2_parser_input_new(pch);
		dpss_h_parser_nr(pch);
		if (pch->d->di_front)
			dpss_s2_parser_rd_nr_only(pch);
		else
			dpss_s2_parser_rd_new(pch);
		itf_polling_in_o(pch);
	} else { //  dps_work_mode == DPSS_WORK_MODE_FRC
		dpss_s2_recycle_back_frc(pch);
		itf_polling_in_o(pch);
		dpss_s2_parser_input_frc(pch);
		dpss_h_parser_frc(pch);
		dpss_s2_parser_rd_frc(pch);
	}
}

unsigned int dpss_pause_in;	// 0 : run; 1: pause; 2: step;
module_param_named(dpss_pause_in, dpss_pause_in, uint, 0664);

void dpss_s2_parser_input_new(struct dpss_ch_s *pch)
{
	struct vframe_s *vfm = NULL, *vfm_in;
	unsigned int chg;
	unsigned int w_mode;
	struct dpss_nr_i_s *nr_i;
	unsigned int nr_free_id;
	unsigned char idx_front = 0;
	struct vframe_s *vfm_front = NULL; //for di_front
	int ret_fifo;
#ifdef FUNC_EN_PQ
	struct di_parm_s *de_devp;
#endif

	if (pch->c.parm.dps_work_mode == DPSS_WORK_MODE_FRC)
		return;

	if (dpss_pause_in == 1)
		return;

	if (dpss_pause_in == 2)	//step
		dpss_pause_in = 1;

	//check nr free
	nr_free_id = dpss_h_b_buf_get_free(pch);
	if (nr_free_id == DPSS_NR_NO_FREE) {
		dbg_i2("ch[%d]:wait nr free\n", pch->c.ch);
		pch->d->cnt_wait_nr_free++;	//dbg only
		return;
	}
	pch->d->cnt_wait_nr_free = 0;	//dbg only

	//
	if (pch->c.last_chg) {	//set by nr
		dbg_i2("ch[%d]:wait last chg\n", pch->c.ch);
		return;
	}
	if ((dpss_get_hw()->need_hold & C_BIT0 && pch->c.ch) ||
	    (dpss_get_hw()->need_hold & C_BIT1 && !pch->c.ch)) {
		dbg_i2("ch[%d]:wait hold:0x%x\n", pch->c.ch, dpss_get_hw()->need_hold);
		return;
	}
	//========================================================
	if (pch->d->di_front &&
	    kfifo_is_empty(&pch->d->q_ch[EDPSS_Q_CH_FRONT_O].f))
		return;

	//peek:
	vfm = dpss_q_peek_vfm(pch, EDPSS_Q_CH_IN);
	if (!vfm || !vfm->dpss_data) {
		//DBG_ERR("%s:1:%p\n", __func__, vfm);
		return;
	}

	nr_i = lk_get_nr_i(vfm);
	if (!nr_i) {
		DBG_ERR("%s:2:%p\n", __func__, vfm);
		return;
	}
	//========================================================
	if (pch->d->di_front) {
		ret_fifo = kfifo_get(&pch->d->q_ch[EDPSS_Q_CH_FRONT_O].f, &idx_front);
		if (ret_fifo != 1 || idx_front >= DPSS_CFG_NUM) {
			DBG_ERR("%s:front o %d %d\n", __func__,
				ret_fifo, idx_front);
			return;
		}
		vfm_front = pch->d->cfg_vfm[idx_front];
		pch->d->cfg_vfm[idx_front] = NULL;
		ret_fifo = kfifo_put(&pch->d->q_ch[EDPSS_Q_CH_FRONT_IDLE].f, idx_front);
		if (ret_fifo != 1) {
			DBG_ERR("%s:front idle %d %d\n", __func__,
				ret_fifo, idx_front);
			return;
		}
		nr_i->front_vfm = vfm_front;
	}
	vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_IN);

	vfm_in = nr_i->in_vfm;
	//copy and back
	dpss_vfm_cp(vfm, vfm_in);
	vfm->dpss_flg = vfm_in->dpss_flg & DPSS_FLG_NEW_TRIG; //keep input
	w_mode = dpss_work_mode_count(pch, vfm);

	chg = dpss_m_chg_count(pch, vfm);

	if (w_mode & D_W_B(DDD))
		pch->d->is_dd = true;
	else
		pch->d->is_dd = false;
	if ((w_mode & C_BIT0) &&
	    pch->d->state_bypass) {
		//send back to-do
		if (pch->d->di_front)
			dpss_di_front_bypass(pch, nr_i);
		else
			dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IN_RECYCLE], vfm); //to-do
		return;
	}

	if (w_mode & C_BIT0) {
		nr_i->chg = C_BIT31 | chg;
		nr_i->work_mode_v = w_mode;
		nr_i->sub_vf_in.is_chg = true; //tmp
		pch->c.last_chg = true;
		nr_i->sub_vf_in.is_bypass = true;
		dbg_i0("%s:w_mode = %d\n", __func__, w_mode);
	} else if (pch->c.work_mode_curr != w_mode || chg) {
		if (pch->d->state_bypass)
			pch->d->state_bypass = false;
		nr_i->chg = C_BIT31 | chg;
		nr_i->work_mode_v = w_mode;
		nr_i->sub_vf_in.is_chg = true;	//tmp
		pch->c.last_chg = true;
	} else {
		nr_i->chg = 0;
	}

	if (pch->c.last_chg) {
		if (!pch->c.ch)
			dpss_get_hw()->need_hold |= C_BIT0;
		else
			dpss_get_hw()->need_hold |= C_BIT1;
	}
	vfm->width = nr_i->sub_vf_in.width;
	vfm->height = nr_i->sub_vf_in.height;

	nr_i->info_out.idx_m = nr_i->info_in.idx_m;
	dbg_i2("%s:ch[%d]:m_idx = %d\n", __func__, pch->c.ch, nr_i->info_out.idx_m);
	itf_clear_caller_data(pch, vfm);

	//change vfm;
	dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_NR_DOING], vfm);
	pch->d->cnt_parser_in++;
	dbg_i2("%s:ch[%d]:cnt_parser_in=%d\n", __func__, pch->c.ch, pch->d->cnt_parser_in);
#ifdef FUNC_EN_PQ
	de_devp = dpss_pq_data(pch->c.ch);
	if (vfm->sig_fmt == TVIN_SIG_FMT_CVBS_SECAM)
		de_devp->v_sig_fmt = 1;//
	else if (_IS_VDIN_SRC(vfm->source_type))
		de_devp->v_sig_fmt = 2;
	else
		de_devp->v_sig_fmt = 0;
	de_devp->v_type = vfm->type;
	de_devp->reserved1 = dpss_sfmt(vfm);
	de_devp->reserved2 = vfm->phase;
	dbg_i2("cnt_parser pq=%d,0x%x,0x%x,0x%x\n", pch->d->cnt_parser_in,
		vfm->type, de_devp->reserved1,
		vfm->phase);
#endif
}

void dpss_s2_parser_input_frc(struct dpss_ch_s *pch)
{
	struct vframe_s *vfm = NULL, *vfm_in;
	unsigned int chg;
	unsigned int w_mode;
	struct dpss_frc_i_s *frc_i;
	unsigned int frc_free_id;

	if (pch->c.parm.dps_work_mode != DPSS_WORK_MODE_FRC)
		return;

	if (dpss_pause_in == 1)
		return;

	if (dpss_pause_in == 2)	//step
		dpss_pause_in = 1;

	// USE_FRC_ONLY_CASE
	frc_free_id = dpss_h_b_buf_get_free(pch);
	if (frc_free_id == DPSS_NR_NO_FREE) {
		dbg_i2("wait frc free\n");
		pch->d->cnt_wait_frc_free++; //dbg only
		return;
	}
	pch->d->cnt_wait_frc_free = 0; //dbg only
	if (pch->c.last_chg) {	//set by nr
		dbg_i2("wait last chg\n");
		return;
	}
	vfm = dpss_q_peek_vfm(pch, EDPSS_Q_CH_IN);
	if (!vfm || !vfm->dpss_data)
		return;

	frc_i = lk_get_frc_i(vfm);
	if (!frc_i) {
		DBG_ERR("%s:2:%p\n", __func__, vfm);
		return;
	}
	vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_IN);
	vfm_in = frc_i->in_vfm;
	dpss_vfm_cp(vfm, vfm_in);
	w_mode = dpss_work_mode_count(pch, vfm);
	chg = dpss_m_chg_count(pch, vfm);  // need check only frc
	if (pch->c.work_mode_curr != w_mode ||
		chg) {
		frc_i->chg = C_BIT31 | chg;
		frc_i->work_mode_v = w_mode;
		frc_i->sub_vf_in.is_chg = true; //tmp
		pch->c.last_chg = true;
	} else {
		frc_i->chg = 0;
	}
	if (pch->d->frc_ds_scale_en == 0) {
		vfm->width = frc_i->sub_vf_in.width;
		vfm->height = frc_i->sub_vf_in.height;
	}
	dbg_i2("chg: vfm->(%d,%d)\n", vfm->width, vfm->height);
	frc_i->info_out.idx_m = frc_i->info_in.idx_m;
	dbg_i2("%s:m_idx = %d\n", __func__, frc_i->info_out.idx_m);
	itf_clear_caller_data(pch, vfm);
	dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_FRC_DOING], vfm);
	pch->d->cnt_parser_in++;
	dbg_i2("vdin:%d  mc:%d\n", pch->d->cnt_pre_get, dpss_mc_rls_cnt);
}

/***************************************************
 * dpss_h_parser_nr
 *	get vfm from doing;
 *	when chg:
 *		clear all old buffer
 *		create new display path
 *
 ***************************************************/

void dpss_h_parser_nr(struct dpss_ch_s *pch)
{
	struct dpss_nr_i_s *nr_i;

//      unsigned int chg;
	struct vframe_s *vfm = NULL;

	unsigned int nr_free_id;
	unsigned int len;
	unsigned int work_nub;
	unsigned int input_q_free_count;
	unsigned int dae_idle_buf_nub;
	unsigned int timeout_ms = 1000;
	ktime_t start, now;
	s64 elapsed_ms;
	bool is_vd1_link;
	bool light_chg = false;
	//=============================================
	//peek:
	vfm = dpss_q_peek_vfm(pch, EDPSS_Q_CH_NR_DOING);
	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_NR_DOING].f);

	if (!vfm || !vfm->dpss_data)
		return;

	//check if chg:

	pch->q.wait_hw_finish = false;
	pch->q.wait_sw_finish = false;
	if (!pch->d->di_front) {
		input_q_free_count = dpss_input_q_free_count(&pch->q);
		if (input_q_free_count == 0) {
			dbg_i2("%s:ch[%d] input_q is full!\n", __func__, pch->c.ch);
			return;
		}
	}
	nr_free_id = dpss_h_b_buf_get_free(pch);
	if (nr_free_id == DPSS_NR_NO_FREE) {
		dbg_i2("%s:ch[%d] wait input vfm\n", __func__, pch->c.ch);
		return;
	}

	if (pch->d->nr_cp_mode) {
		dae_idle_buf_nub = pch->c.in_cnt - pch->c.out_cnt;

		if (dae_idle_buf_nub > DAE_BUF_NUM_LEFT) {
			DBG_WARN("%s:dae buf no enough\n", __func__);
			return;
		}
	}

	nr_i = lk_get_nr_i(vfm);
	if (!nr_i) {
		DBG_ERR("%s:illegal:vfm = %p\n", __func__, nr_i);
		return;
	}
	if (nr_i->chg) {
		if ((dpss_get_hw()->st_idle & dpss_get_hw()->ch_act) !=
		    dpss_get_hw()->ch_act) {
			dbg_i2("%s:ch[%d]wait idle:0x%x, 0x%x\n", __func__,
				pch->c.ch,
				dpss_get_hw()->st_idle,
				dpss_get_hw()->ch_act);
			pch->q.wait_hw_finish = true;
			return;
		}
		if (pch->c.in_cnt != pch->c.disp_cnt && !pch->d->di_front) {
			dbg_i2("%s:ch[%d]in_cnt:%d, disp_cnt:%d\n", __func__,
				pch->c.ch,
				pch->c.in_cnt,
				pch->c.disp_cnt);
			pch->q.wait_sw_finish = true;
			return;
		}
		work_nub = dpss_h_b_buf_get_work(pch);
		if (work_nub) {
			if (!pch->c.ch)
				dpss_get_hw()->st_idle &= ~C_BIT0;
			else
				dpss_get_hw()->st_idle &= ~C_BIT1;
			dbg_i1("%s:ch[%d] wait hw finish\n", __func__, pch->c.ch);
			return;
		}
		if (!pch->c.ch)
			dpss_get_hw()->st_idle |= C_BIT0;
		else
			dpss_get_hw()->st_idle |= C_BIT1;
		dpss_hw_disable_one_play(pch);//tmp here ??
	}
	//=============================================
	//out
	vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_NR_DOING);

	nr_i = lk_get_nr_i(vfm);
	if (!nr_i) {
		DBG_ERR("%s:2:%p\n", __func__, vfm->dpss_data);
		return;
	}
	if (nr_i->chg && nr_i->sub_vf_in.is_bypass) {
		//send back vframe:
		if (pch->d->di_front)
			dpss_di_front_bypass(pch, nr_i);
		else
			dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IN_RECYCLE], vfm);

		if (!pch->d->di_front)
			dpss_input_q_init(&pch->q);
		pch->c.in_cnt = 0;
		pch->c.out_cnt = 0;
		pch->c.disp_cnt = 0;
		pch->c.last_chg = false;
		pch->c.work_mode_curr = 0;
		if (!pch->c.ch)
			dpss_get_hw()->need_hold &= ~C_BIT0;
		else
			dpss_get_hw()->need_hold &= ~C_BIT1;
		pch->d->state_bypass = true;
		DBG_INF("bypass\n");
		return;
	}

	if (nr_i->chg) {
		is_vd1_link = is_vd1_link_state();
		if ((dpss_light_chg & C_BIT0) &&
		     pch->d->nr_reset &&
		     !VFM_IS_I_SRC(vfm->type) &&
			 !VFM_IS_HDMI_SRC(vfm->source_type) &&
			 !is_vd1_link)
			light_chg = true;

		if (pch->c.ch == 0 && pch->d->frc_link &&
			resolution_switch_count && !light_chg) {
			start = ktime_get();
			dpss_enable_mc_link(0);
			while (1) {
				if (is_vd1_link_state())
					break;
				now = ktime_get();
				elapsed_ms = ktime_ms_delta(now, start);
				if (elapsed_ms > timeout_ms) {
					DBG_ERR("timeout: mc exit link more than %d ms\n",
						timeout_ms);
					break;
				}
				usleep_range(2000, 3000);
			}
		}
		if (pch->c.ch == 0 && !light_chg)
			resolution_switch_count++;

		if (!light_chg) {
			dpss_h_b_int(pch);
			nr_only_int(pch, &nr_i->sub_vf_in, nr_i->in_vfm);
			if (!pch->d->di_front)
				dpss_input_q_init(&pch->q);
			pch->c.in_cnt = 0;
			pch->c.out_cnt = 0;
			pch->c.disp_cnt = 0;
		} else {
			nr_only_int(pch, &nr_i->sub_vf_in, nr_i->in_vfm);
		}

		pch->c.last_chg = false;	//use this to start doing que input
		pch->c.work_mode_curr = nr_i->work_mode_v;
		if (!pch->c.ch)
			dpss_get_hw()->need_hold &= ~C_BIT0;
		else
			dpss_get_hw()->need_hold &= ~C_BIT1;
		//chg_flg = true;
		//nr_i->chg = 0; //ary need check
		dbg_i0("%s:ch[%d]restart\n", __func__, pch->c.ch);
	}
	nr_free_id = dpss_h_b_buf_get_free(pch);
	if (nr_free_id == DPSS_NR_NO_FREE) {
		dbg_ins2("wait input vfm\n");
		return;
	}

	if (!pch->d->di_front)
		dpss_input_q_in(&pch->q, vfm);
	if (!pch->c.ch)
		dpss_get_hw()->st_idle &= ~C_BIT0;
	else
		dpss_get_hw()->st_idle &= ~C_BIT1;
	dbg_i2("%s:ch[%d] in:%d\n", __func__, pch->c.ch, pch->d->cnt_h_in);
	//=====================================
	dpss_h_b_buf_get(pch, nr_free_id, vfm);
	if (pch->c.ch == 0 && pch->d->frc_link)
		dpss_h_update_frc(vfm);
	if (pch->d->di_front)
		hw_cfg_addr_dio(pch, pch->c.in_cnt, nr_i->front_vfm);
	nr_only_input_buf(pch, nr_i);
	pch->d->cnt_h_in++;
}

void dpss_h_parser_frc(struct dpss_ch_s *pch)
{
	struct dpss_frc_i_s *frc_i;

//      unsigned int chg;
	struct vframe_s *vfm = NULL;
	bool need_restart = false;
	unsigned int frc_free_id;
	unsigned int len;
	unsigned int work_nub;

	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (!pchip_st)
		return;
	if (!src0_data_trigger() && !pchip_st->state_st.is_first_frame) {
		DBG_WARN("%s:src0 no rd\n", __func__);
		return;
	}
	//=============================================
	//peek:
	vfm = dpss_q_peek_vfm(pch, EDPSS_Q_CH_FRC_DOING);
	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_FRC_DOING].f);

	if (!vfm || !vfm->dpss_data)
		return;
	//check if chg:
	dbg_i2("%s:vf_frc_peek: %p\n", __func__, (void *)vfm);
	frc_free_id = dpss_h_b_buf_get_free(pch);
	if (!need_restart && frc_free_id == DPSS_NR_NO_FREE) {
		dbg_ins2("wait input vfm\n");
		return;
	}

	frc_i = lk_get_frc_i(vfm);
	if (!frc_i) {
		DBG_ERR("%s:illegal:vfm = %p\n", __func__, (void *)frc_i);
		return;
	}
	if (frc_i->chg) {
		work_nub = dpss_h_b_buf_get_work(pch);
		if (work_nub) {
			dbg_i2("%s:wait hw finish\n", __func__);
			return;
		}
	}
	//=============================================
	//out
	vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_FRC_DOING);
	dbg_i2("%s:vf_frc_get: %p\n", __func__, (void *)vfm);
	frc_i = lk_get_frc_i(vfm);
	if (!frc_i) {
		DBG_ERR("%s:2: %p\n", __func__, (void *)vfm->dpss_data);
		return;
	}

	if (frc_i->chg) {
		dpss_h_b_int(pch);
		// frc_only_int(pch, &frc_i->sub_vf_in);
		frc_only_int(pch, &frc_i->sub_vf_in, frc_i->in_vfm);
		need_restart = true;
		pch->c.out_cnt = 0;	//tmp;
		pch->c.in_cnt = 0;	//tmp;
		pch->c.disp_cnt = 0;	//tmp
		pch->c.last_chg = false;	//use this to start doing que input
		pch->c.work_mode_curr = frc_i->work_mode_v;
		frc_i->chg = 0;
		pchip_st->state_st.is_first_frame = 0;
		dbg_ins0("%s:restart\n", __func__);
	}
	frc_free_id = dpss_h_b_buf_get_free(pch);
	if (frc_free_id == DPSS_NR_NO_FREE) {
		dbg_ins2("wait input vfm\n");
		return;
	}
	dbg_i2("%s:ch[%d] in:%d\n", __func__, pch->c.ch, pch->d->cnt_h_in);
	//=====================================
	dpss_h_b_buf_get(pch, frc_free_id, vfm);
	dpss_h_update_frc(vfm);
	frc_only_input_buf(pch, frc_i);
	pch->d->cnt_h_in++;
}

//============================================================
void dpss_dbg_peek(unsigned int ch)
{
	struct vframe_s *vfm = NULL, *vfm_in;
	struct dpss_nr_i_s *nr_i;
	struct dpss_ch_s *pch;

	pch = dpss_get_ch(ch);
	DBG_INF("%s:ch[%d]\n", __func__, ch);

	vfm = dpss_q_peek_vfm(pch, EDPSS_Q_CH_IN);
	if (vfm) {
		nr_i = lk_get_nr_i(vfm);
		if (!nr_i) {
			DBG_WARN("send err1\n");
			return;
		}

		vfm_in = nr_i->in_vfm;
		pch->d->vfm_dis = vfm_in;
		pch->d->vfm_dis_en = true;
		if (pch->c.etype == 0) {	//vframe path
			itf_vfm_path_ready_notify(pch);
		}

		DBG_INF("send finish\n");
		return;

	} else {
		DBG_INF("%s:no vfm\n", __func__);
	}
}

//dbg only:
unsigned int dpss_dbg_o_type;
module_param_named(dpss_dbg_o_type, dpss_dbg_o_type, uint, 0664);
unsigned int dpss_dbg_o_w;
module_param_named(dpss_dbg_o_w, dpss_dbg_o_w, uint, 0664);
unsigned int dpss_dbg_o_h;
module_param_named(dpss_dbg_o_h, dpss_dbg_o_h, uint, 0664);
unsigned int dpss_dbg_o_cvs_w;
module_param_named(dpss_dbg_o_cvs_w, dpss_dbg_o_cvs_w, uint, 0664);
unsigned int dpss_dbg_o_cvs_h;
module_param_named(dpss_dbg_o_cvs_h, dpss_dbg_o_cvs_h, uint, 0664);

unsigned int dpss_dbg_o_bitd;
module_param_named(dpss_dbg_o_bitd, dpss_dbg_o_bitd, uint, 0664);

unsigned int dpss_o_42210;
module_param_named(dpss_o_42210, dpss_o_42210, uint, 0664);

unsigned int dpss_o_dw;
module_param_named(dpss_o_dw, dpss_o_dw, uint, 0664);

unsigned int dpss_o_42212 = EDPSS_DP_MODE_422_10BIT_2PACK;
module_param_named(dpss_o_42212, dpss_o_42212, uint, 0664);

unsigned int dpss_o_42210_4K = EDPSS_DP_MODE_422_10BIT_2PACK;
module_param_named(dpss_o_42210_4K, dpss_o_42210_4K, uint, 0664);

unsigned int dpss_frc_delay_count = 2; // 3
module_param_named(dpss_frc_delay_count, dpss_frc_delay_count, uint, 0664);

void dpss_di_front_bypass(struct dpss_ch_s *pch, struct dpss_nr_i_s *nr_i)
{
	struct vframe_s *vfm_in, *vfm_front, *vfm;

	if (!pch || !nr_i ||
	    !nr_i->in_vfm ||
	    !nr_i->front_vfm ||
	    !nr_i->lk_grd_parent) {
		DBG_ERR("%s:no input\n", __func__);
		return;
	}
	vfm_in = nr_i->in_vfm;
	vfm_front = nr_i->front_vfm;
	vfm = (struct vframe_s *)nr_i->lk_grd_parent;
	vfm_in->dpss_flg = DPSS_FLG_BYPASS | DPSS_FLG_FRONT;
	vfm_front->dpss_flg = DPSS_FLG_BYPASS | DPSS_FLG_FRONT;
	if (vfm_in->type & VIDTYPE_V4L_EOS)
		vfm_front->type |= VIDTYPE_V4L_EOS;

	dbg_ins1("%s:fill_out:%p, %d\n", __func__, vfm_front, pch->d->cnt_pst_get);
	pch->d->cnt_pst_get++;
	pch->c.parm.ops.fill_output_done(pch->c.parm.ops.arg, vfm_front);

	dpss_in_rck_in(pch, vfm_in);
	nr_i->in_vfm = NULL;
	nr_i->front_vfm = NULL;

	memset(&nr_i->sub_vf_in, 0, sizeof(nr_i->sub_vf_in));

	dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IDLE_NR], vfm);
}

/* only for di_front */
void dpss_s2_parser_rd_nr_only(struct dpss_ch_s *pch)
{
	unsigned int idx;
	struct PRM_DPSS_TOP *prm_top;// = prm_dpss_top;
	unsigned int dpe_done_diff;
	bool tbc_mode;
	unsigned int count;
	unsigned int in_cnt, out_cnt;
//	unsigned char b_idx;
	struct vframe_s *vfm = NULL, *vfm_in = NULL, *vfm_fr = NULL;
	struct dpss_nr_i_s *nr_i;
	struct canvas_config_s *cvs;

	if (!pch->d || !pch->d->init)
		return;
	if (!pch->d->di_front) {
		DBG_ERR("%s:not di_front?\n", __func__);
		return;
	}

	prm_top = &pch->c.prm_top;

	nr_h_wait_mode(pch);	//tmp here

	if (pch->c.in_cnt < pch->c.out_cnt) {
		DBG_WARN("%s:overflow:%u, %u\n", __func__,
			pch->c.in_cnt, pch->c.out_cnt);
		return;
	}
	if (pch->c.in_cnt == pch->c.out_cnt)
		return;

	in_cnt = pch->c.in_cnt;
	out_cnt = pch->c.out_cnt;
	if (prm_top->src_mode == 0)
		count = rd(DPSS_FBUF_DPE_FRM_IDX);
	else
		count = rd(DPSS_FBUF_DPE_FRM_IDX + 0x300);

	if (out_cnt >= count)
		return;

	dpe_done_diff = count - pch->c.out_cnt;
	//---------------------------------------------
	idx = out_cnt % pch->c.o_b_nub;

	vfm = dpss_h_b_buf_put(pch, idx);//just for balance

	if (!vfm || !vfm->dpss_data) {
		DBG_WARN("%s:ch[%d]:no vfm?\n", __func__, pch->c.ch);
		//return;
	} else {
		nr_i = lk_get_nr_i(vfm);
		if (!nr_i || !nr_i->in_vfm) {
			DBG_ERR("%s: nr_i is null or no in_vfm?\n", __func__);
			return;
		}
		vfm_fr = nr_i->front_vfm;
		if (vfm_fr) {
			vfm_fr->dpss_flg = 0;
			vfm_fr->dpss_flg = DPSS_FLG_FRONT;
			vfm_fr->plane_num = 2;
			vfm_fr->bitdepth = 0;
			vfm_fr->type_dw = 0;
			vfm_fr->type &= ~DPSS_VFMT_MASK_ALL;
			vfm_fr->type |= VIDTYPE_VIU_FIELD;
			if (prm_top->out_mode == OUT_YUV420_NV12)
				vfm_fr->type |= VIDTYPE_VIU_NV12;
			else
				vfm_fr->type |= VIDTYPE_VIU_NV21;

			vfm_fr->bitdepth |= (BITDEPTH_Y8 |
				BITDEPTH_U8	|
				BITDEPTH_V8);
			vfm_fr->canvas0Addr = (u32)-1;
			cvs = &vfm_fr->canvas0_config[0];
			cvs->bit_depth = 8;
			cvs->width = nr_i->sub_vf_in.cvs_h;
			cvs->height = nr_i->sub_vf_in.height;
			cvs->block_mode = 0;
			cvs->endian = 0;
			cvs = &vfm_fr->canvas0_config[1];
			cvs->bit_depth = 8;
			cvs->width = nr_i->sub_vf_in.cvs_h;
			cvs->height = nr_i->sub_vf_in.height;
			cvs->height = cvs->height >> 1;
			cvs->block_mode = 0;
			cvs->endian = 0;

			nr_i->front_vfm = NULL;
			vfm_in = nr_i->in_vfm;

			dpss_in_rck_in(pch, vfm_in);
			nr_i->in_vfm = NULL;

			memset(&nr_i->sub_vf_in, 0, sizeof(nr_i->sub_vf_in));
			//to-do: need mv sub infor in one;
			dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IDLE_NR], vfm);
		}
	}
	//---------------------------------------------
	//
	if (vfm_fr) {
		dbg_ins1("%s:fill_out:%p, %d\n", __func__, vfm_fr, pch->d->cnt_pst_get);
		pch->d->cnt_pst_get++;
		pch->c.parm.ops.fill_output_done(pch->c.parm.ops.arg, vfm_fr);
	} else {
		DBG_ERR("%s:no vfm ?\n", __func__);
	}
	//---------------------------------------------
	// release buffer:-----------------------------
	if (prm_top->sw_tbc_mode)
		tbc_mode = true;
	else
		tbc_mode = false;
	hw_release_buf(prm_top->src_mode, idx, tbc_mode); //idx need check
	//---------------------------------------------
	dbg_ct1("%s:ch[%d] r in_cnt =%d out_cnt=%d hw = %d\n",
		__func__,
		pch->c.ch, pch->c.in_cnt, pch->c.out_cnt, count);
	pch->c.out_cnt++;

	if (pch->c.in_cnt == pch->c.out_cnt) {
		dbg_i2("%s:hw finish\n", __func__);
		if (!pch->c.ch)
			dpss_get_hw()->st_idle |= C_BIT0;
		else
			dpss_get_hw()->st_idle |= C_BIT1;
	}
}
//pre-mem:
/************************
 * [3:0] sml
 ************************/
unsigned int dpss_mem_flg;
module_param_named(dpss_mem_flg, dpss_mem_flg, uint, 0664);

void dpss_s2_parser_rd_new(struct dpss_ch_s *pch)
{
	unsigned int idx;
	int ret = 0;
	struct PRM_DPSS_TOP *prm_top;// = prm_dpss_top;
	unsigned int dpe_done_diff;
	bool input_enabled; // = prm_top->frc_en;
	bool tbc_mode;
	int count;
	bool need_output_last_i = false;

	// struct dpss_dd_s *dd;
	// dd = dpss_get_dd();
	//	if (g_dpss_tst_case == TST_CASE_IDX_0002)
	if (!pch->d || !pch->d->init)
		return;
	prm_top = &pch->c.prm_top;
	input_enabled = prm_top->frc_en;

	nr_h_wait_mode(pch);	//tmp here
	if (pch->c.parm.dps_work_mode == DPSS_WORK_MODE_FRC) {
		if (pch->d && pch->d->frc_link)
			return;
	}

	//rd_cnt = hw_check_rd_nub(prm_top->src_mode);
	//if (!rd_cnt) {
	//	dbg_i2("%s:rd_cnt is 0\n", __func__);
	//	return;
	//}

	if (pch->c.in_cnt < pch->c.out_cnt) {
		DBG_WARN("%s:overflow:0x%x, 0x%x\n", __func__, pch->c.in_cnt,
			 pch->c.out_cnt);
		//to-do
		return;
	}

	if (input_enabled && !prm_top->is_i) {
		/*output all vf that input done && is non-key frame*/
		count = dpss_input_q_out_1(&pch->q);
		while (count > 0) {
			out_put_vf(pch, pch->q.output_idx_last, false);
			count--;
		}
	}

	if (prm_top->src_mode == 0)
		count = rd(DPSS_FBUF_DPE_FRM_IDX);
	else
		count = rd(DPSS_FBUF_DPE_FRM_IDX + 0x300);
	dpe_done_diff = count - pch->q.dpe_done_count;
	dbg_ct0("%s:ch[%d] input_q: dpe_done_count=%d, diff=%d, in_cnt =%d out_cnt=%d,disp:%d,%d\n",
		__func__,
		pch->c.ch,
		pch->q.dpe_done_count,
		dpe_done_diff,
		pch->c.in_cnt,
		pch->c.out_cnt,
		pch->c.disp_cnt,
		prm_top->src_mode);

	if (pch->c.ch == 0 && pch->d->frc_link && count == dpss_frc_delay_count &&
		resolution_switch_count > 1) {
		dbg_ct0("%s: now need link frm.\n", __func__);
		dpss_enable_mc_link(1);
	}

	if (prm_top->is_i && pch->q.wait_sw_finish) {
		dbg_ct0("%s: I wait_sw_finish\n", __func__);
		if (pch->c.in_cnt - pch->c.disp_cnt == 1)
			need_output_last_i = true;
		dbg_ct0("%s: in=%d, out=%dneed_output_last_i = %d\n",
			__func__,
			pch->c.in_cnt,
			pch->c.out_cnt,
			need_output_last_i);
	}

	//if (prm_top->is_i) {
	//	if (dpe_done_diff > 0 && !need_output_last_i)
	//		dpe_done_diff--;
	//}

	if (prm_top->is_i && need_output_last_i) {
		out_put_vf(pch, 0, true);
		return;
	}

	if (dpe_done_diff == 0)
		return;

output_vf:
	/*only process one dpe done frame*/
	dpe_done_diff--;

	if (prm_top->is_i) {
		if (need_output_last_i) {
			idx = pch->q.output_idx_last;
		} else {
			if (prm_top->frc_en && pch->c.ch == 0) {
				ret = dpss_input_get_buf_index(&pch->q);
				if (ret >= 0)
					idx = ret;
				else
					return;
			} else {
				idx = pch->q.dpe_done_count % pch->c.o_b_nub;
			}
		}
	} else {
		idx = pch->q.dpe_done_count % pch->c.o_b_nub;
	}

	if (idx > pch->c.o_b_nub - 1) {
		DBG_ERR("%s: invalid buf index(%d).\n", __func__, idx);
		return;
	}

	// release buffer:
	if (!prm_top->frc_en && !need_output_last_i) {
		if (prm_top->sw_tbc_mode)
			tbc_mode = true;
		else
			tbc_mode = false;
		hw_release_buf(prm_top->src_mode, idx, tbc_mode); //check to-do
	}

	pch->q.dpe_done_count++;

	if (input_enabled && !prm_top->is_i) {
		/*output all vf that input done && is non-key frame*/
		count = dpss_input_q_out_1(&pch->q);
		while (count > 0) {
			out_put_vf(pch, pch->q.output_idx_last, false);
			count--;
		}
		/*output vf that dpe done && is key frame*/
		dpss_input_q_out_head_key(&pch->q);
	} else {
		/*output head for no frc case*/
		dpss_input_q_out_head(&pch->q);
	}

	out_put_vf(pch, idx, false);
	pch->q.output_idx_last = idx;
	dbg_ct0("%s:ch[%d] input_q: in_cnt=%d, out_cnt=%d, dpe_done_cnt=%d, buf_in_dpss=%d\n",
		__func__,
		pch->c.ch,
		pch->c.in_cnt,
		pch->c.out_cnt,
		pch->q.dpe_done_count,
		pch->c.in_cnt - pch->c.out_cnt);

	if (dpe_done_diff > 0)
		goto output_vf;
}

void out_put_vf(struct dpss_ch_s *pch, unsigned int idx, bool output_last)
{
	struct vframe_s *vfm, *vfm_in, *vfm_tmp;
	struct canvas_config_s *cvs;
	struct dpss_nr_i_s *nr_i;
	unsigned char b_idx, bb_idx, hd_idx;
	struct PRM_DPSS_TOP *prm_top = &pch->c.prm_top;//&prm_dpss_top;
	unsigned int canvas_align_width = 64;
	unsigned int vf_idx;
	struct dpss_blk_s *blk;
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;

	if (output_last) {
		dbg_h1("%s:output_last: in=%d, out=%d\n", __func__, pch->c.in_cnt, pch->c.out_cnt);
		pch->c.vfm_last->type_ext |= VIDTYPE_EXT_DPSS_SKIP;
		dpss_dis_vfm(pch, pch->c.vfm_last);
		return;
	}

	vf_idx = pch->c.out_cnt % pch->c.in_b_nub;

	if (dpss_slt_mode)
		if (pch->c.out_cnt == 0)
			pass_cnt = 0;

	vfm = dpss_h_b_buf_put(pch, vf_idx);

	bb_idx = dpss_h_buf_get_b_idx(pch, idx);
	dbg_h1("%s:out_cnt=%d,idx = %d:%d, vf_idx=%d\n",
		__func__,
		pch->c.out_cnt,
		idx,
		bb_idx,
		vf_idx);
	pch->d->idx_done = bb_idx;
	b_idx = (bb_idx + pch->d->idx_start) % pch->c.o_b_nub;

	hd_idx = bb_idx;
	if (pch->d->idx_hd) {
		hd_idx = bb_idx + pch->c.o_b_nub;
	}
	if (!vfm || !vfm->dpss_data) {
		DBG_ERR("%s:no vfm?\n", __func__);
		return;
	}
	nr_i = lk_get_nr_i(vfm);
	if (!nr_i || !nr_i->in_vfm) {
		DBG_ERR("%s: nr_i is null or no in_vfm?\n", __func__);
		return;
	}
	vfm_in = nr_i->in_vfm;
	vfm->dpss_flg |= (vfm_in->dpss_flg &
		(DPSS_FLG_DDD |
		DPSS_FLG_HDR | DPSS_FLG_HDR_CHG)); //for dd/hdr
	dpss_in_rck_in(pch, vfm_in);
	nr_i->in_vfm = NULL;

	if (dpss_slt_mode) {
		dbg_h0("DPSS CRC <CUR 0x%x NR 0x%x DI 0x%x> index:%d\n", rd(VPSS_VBE_CRC0_RO),
			rd(VPSS_VBE_CRC1_RO), rd(VPSS_VBE_CRC2_RO), vfm->frame_index);

		if (rd(VPSS_VBE_CRC0_RO) == 0x65e9168f && rd(VPSS_VBE_CRC2_RO) == 0xb795ab15)
			pass_cnt++;
		if (rd(VPSS_VBE_CRC0_RO) == 0x924f7a86 && rd(VPSS_VBE_CRC2_RO) == 0x88bd6a67)
			pass_cnt++;
		if (rd(VPSS_VBE_CRC0_RO) == 0xe4423e5b && rd(VPSS_VBE_CRC2_RO) == 0xc86b4615)
			pass_cnt++;
		if (rd(VPSS_VBE_CRC0_RO) == 0xe723fa9f && rd(VPSS_VBE_CRC2_RO) == 0x78827618)
			pass_cnt++;
		if (rd(VPSS_VBE_CRC0_RO) == 0x7ff805db && rd(VPSS_VBE_CRC2_RO) == 0x6e3c5ce9)
			pass_cnt++;
		if (rd(VPSS_VBE_CRC0_RO) == 0x8f5431fa && rd(VPSS_VBE_CRC2_RO) == 0xbc17c1c1)
			pass_cnt++;
		if (rd(VPSS_VBE_CRC0_RO) == 0xa1be8642 && rd(VPSS_VBE_CRC2_RO) == 0xb846af4)
			pass_cnt++;
		if (rd(VPSS_VBE_CRC0_RO) == 0x30d1e73f && rd(VPSS_VBE_CRC2_RO) == 0x39cd6e84)
			pass_cnt++;

		if (pch->c.out_cnt == 9) {
			if (pass_cnt == 8)
				DBG_INF("DPSS SLT PASS\n");
			else
				DBG_INF("DPSS SLT failed\n");
			pass_cnt = 0;
		}
	}

	if (prm_top->is_i && pch->c.out_cnt > 0) {
		vfm_tmp = vfm;
		vfm = pch->c.vfm_last;
	}

	vfm->plane_num = 2;
	vfm->bitdepth = 0;
	vfm->type_dw = 0;
	//fix nv12:
	vfm->type &= ~DPSS_VFMT_MASK_ALL;
	vfm->type |= VIDTYPE_VIU_NV12 | VIDTYPE_VIU_FIELD;

	if (prm_top->out_mode == OUT_YUV420_NV12 ||
		prm_top->out_mode == OUT_YUV420_NV21) {
		vfm->bitdepth = 0;
		if (prm_top->out_mode == OUT_YUV420_NV12)
			vfm->type = VIDTYPE_VIU_NV12 | VIDTYPE_VIU_FIELD;
		else
			vfm->type = VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y8	|
					BITDEPTH_U8	|
					BITDEPTH_V8);
		vfm->plane_num = 2;
		dbg_h1("\t type:0x%x, plan:%d, bitdepth = 0x%x\n", vfm->type,
			vfm->plane_num, vfm->bitdepth);
	}

	if (dpss_o_42210 == 1 || prm_top->out_mode == OUT_YUV422_1_PLANE) {
		vfm->type &= ~(VIDTYPE_VIU_NV12	|
			       VIDTYPE_VIU_444	|
			       VIDTYPE_VIU_NV21	|
			       VIDTYPE_VIU_422	|
			       VIDTYPE_VIU_SINGLE_PLANE	|
			       VIDTYPE_COMPRESS	|
			       VIDTYPE_PRE_INTERLACE |
				VIDTYPE_TYPEMASK |
				VIDTYPE_INTERLACE_FIRST);
		vfm->type |=	VIDTYPE_VIU_422 |
				VIDTYPE_PROGRESSIVE |
				VIDTYPE_VIU_SINGLE_PLANE |
				VIDTYPE_VIU_FIELD;
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |= (BITDEPTH_Y10 |
				  BITDEPTH_U10 |
				  BITDEPTH_V10 | FULL_PACK_422_MODE);
		vfm->plane_num = 1;
	}

	if (vfm->fgs_valid && prm_top->fg_en) {
		vfm->fgs_valid = false;
		dbg_h0("dpss doing fg\n");
	}

	if (dpss_o_42210 == 2 || prm_top->out_mode == OUT_YUV422_2_PLANE) {
		vfm->type &= ~(VIDTYPE_VIU_NV12 |
			       VIDTYPE_VIU_444	|
			       VIDTYPE_VIU_NV21 |
			       VIDTYPE_VIU_422	|
			       VIDTYPE_VIU_SINGLE_PLANE |
			       VIDTYPE_COMPRESS |
			       VIDTYPE_PRE_INTERLACE |
				VIDTYPE_TYPEMASK |
				VIDTYPE_INTERLACE_FIRST);
		vfm->type |=	VIDTYPE_VIU_422 |
				VIDTYPE_PROGRESSIVE |
				VIDTYPE_VIU_SINGLE_PLANE |
				VIDTYPE_VIU_FIELD;
		vfm->bitdepth &= ~(BITDEPTH_MASK);
		vfm->bitdepth &= ~(FULL_PACK_422_MODE);
		vfm->bitdepth |=	(BITDEPTH_Y10	|
					BITDEPTH_U10	|
					BITDEPTH_V10	|
					FULL_PACK_422_MODE);
		vfm->plane_num = 2;
		vfm->type_ext |= VIDTYPE_EXT_422_10_2P_DPSS;
	}

	if (b_idx > pch->c.o_b_nub) {
		DBG_ERR("%s:idx is overflow:%d,%d\n", __func__, b_idx,
			pch->c.o_b_nub);
		b_idx = 0;
	}
	if (pch->d->frc_link) {
		vfm->type_ext |= VIDTYPE_EXT_FRC_LINK;
		nr_i->nr_buf_idx = b_idx;
	}
	if (dpss_dbg_o_w) {
		DBG_INF("force o w=%d -> %d\n", vfm->width, dpss_dbg_o_w);
		vfm->width = dpss_dbg_o_w;
	}
	if (dpss_dbg_o_h) {
		DBG_INF("force o h=%d -> %d\n", vfm->height, dpss_dbg_o_h);
		vfm->height = dpss_dbg_o_h;
	}

	vfm->original_width = vfm->width;
	vfm->original_height = vfm->height;

	if (pch->c.prm_top.is_pps && dpss_dpe_nr_frm_cnt > dpss_pps_count) {
		dbg_h2("%s: open pps, set vf.%d,%d,%d,%d,%d,%d\n", __func__,
			vfm->original_width, vfm->original_height,
			vfm->width, vfm->height, vfm->dpss_pps_dsy, vfm->dpss_pps_dsx);
		if (pps_out_w) {
			vfm->width = pps_out_w;
			vfm->height = pps_out_h;
		} else {
			vfm->width = prm_dpe->dpe_nr_size.pps_hsize;
			vfm->height = prm_dpe->dpe_nr_size.pps_vsize;
		}
		vfm->dpss_pps_dsx = pch->c.prm_top.pps_dsx;
		vfm->dpss_pps_dsy = pch->c.prm_top.pps_dsy;
	} else {
		vfm->dpss_pps_dsx = 0;
		vfm->dpss_pps_dsy = 0;
	}
	dbg_i0("%s:is_pps,ch=%d, =%d\n", __func__, pch->c.ch, pch->c.prm_top.is_pps);

	if (prm_top->vds_4k1k_en || dpss_force_nr_debug || dpss_nr_debug == 2) {
		vfm->height = vfm->height >> 1;
		vfm->canvas0_config[0].height = vfm->canvas0_config[0].height >> 1;
	}

	dbg_h2("%s: output_h:%d, output_canvas_h:%d, afrc_mode is:%d\n",
		__func__,
		vfm->height,
		vfm->canvas0_config[0].height,
		pch->c.o_afbc);

	if ((dpss_en_afbc & C_BIT0) || (pch->c.o_afbc & C_BIT0)) {
		vfm->type =
		    VIDTYPE_COMPRESS | VIDTYPE_SCATTER | VIDTYPE_VIU_NV12;
		vfm->compWidth = vfm->width;
		vfm->compHeight = vfm->height;
		vfm->compBodyAddr = pch->c.addr_nr[b_idx];
		vfm->compHeadAddr = pch->c.addr_afbc_info[hd_idx];
		if (vfm->type_ext & VIDTYPE_EXT_AFRC_COMPRESS)
			vfm->type_ext &= ~VIDTYPE_EXT_AFRC_COMPRESS;
		dbg_i2("ch[%d]:afbce:o:vfm:type=0x%x:0x%lx\n", pch->c.ch,
				vfm->type, vfm->compHeadAddr);

		//void *mem_handle_1; //table
		//void *mem_head_handle; //head
		//void *mem_handle; //body
		//void *mem_dw_handle //dw
		//mem_handle
		//tab
		blk = &pch->c.blk_r_afbc_tab;
		vfm->mem_handle_1 = blk->c.b.blk_m.mem_handle;
		blk = &pch->c.blk_r_afbc_hd;
		if (pch->d->idx_hd)
			blk = &pch->c.blk_r_afbc_hd_b;
		vfm->mem_head_handle = blk->c.b.blk_m.mem_handle;
		blk = &pch->c.blk_r_nr[b_idx];
		vfm->mem_handle = blk->c.b.blk_m.mem_handle;
		blk = &pch->c.blk_r_dw[b_idx];
		vfm->mem_dw_handle = blk->c.b.blk_m.mem_handle;
	} else if ((dpss_en_afbc & C_BIT1) || (pch->c.o_afbc & C_BIT1)) {
		vfm->type = VIDTYPE_COMPRESS | VIDTYPE_SCATTER | VIDTYPE_VIU_422;
		vfm->type_ext |= VIDTYPE_EXT_AFRC_COMPRESS;
		vfm->compWidth = vfm->width;
		vfm->compHeight = vfm->height;
		vfm->compBodyAddr = pch->c.addr_nr[b_idx];
		vfm->compHeadAddr = pch->c.addr_afbc_info[hd_idx];

		vfm->afrc_info.luma_head_addr = pch->c.addr_afbc_info[hd_idx];
		vfm->afrc_info.luma_body_addr = pch->c.addr_nr[b_idx];
		vfm->afrc_info.chrm_head_addr = pch->c.addr_afbc_info_c[hd_idx];
		vfm->afrc_info.chrm_body_addr = pch->c.addr_nr_uv[b_idx];
		vfm->afrc_info.luma_header_en = dpss_afrc_head;
		vfm->afrc_info.chrm_header_en = dpss_afrc_head;
		vfm->afrc_info.luma_dict_en = prm_top->comp_setting.afrc_dict_mode_y;
		vfm->afrc_info.chrm_dict_en = prm_top->comp_setting.afrc_dict_mode_c;
		vfm->afrc_info.luma_comp_target = pch->d->afrc_bs_y;
		vfm->afrc_info.chrm_comp_target = pch->d->afrc_bs_c;
		dbg_i2("ch[%d]:afrc:o:type:0x%x,0x%x,0x%x:\n", pch->c.ch, vfm->type,
			vfm->type_ext, vfm->afrc_info.luma_header_en);

		if (prm_top->out_mode == OUT_YUV422_1_PLANE) {
			vfm->canvas0Addr = (u32)-1;
			vfm->width = vfm->width >> prm_top->dpe_dw_dsx;
			vfm->height = vfm->height >> prm_top->dpe_dw_dsy;
			vfm->bitdepth_dw = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
			vfm->bitdepth_dw |= FULL_PACK_422_MODE;
			vfm->type_dw |= (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_422);

			cvs = &vfm->canvas0_config[0];
			cvs->bit_depth = 10;
			cvs->block_mode = 0;
			cvs->endian = 0;
			cvs->phy_addr = pch->c.addr_dw[b_idx];
			cvs->width = (vfm->width * 5) / 2;
			cvs->width = roundup(cvs->width, canvas_align_width);
			cvs->height = vfm->height;
			dbg_i2("%s:wh=%d %d, xy=%d %d\n", __func__, vfm->width,
				vfm->height, prm_top->dpe_dw_dsx, prm_top->dpe_dw_dsy);
		} else if (prm_top->out_mode == OUT_YUV422_2_PLANE) {
			vfm->canvas0Addr = (u32)-1;
			vfm->width = vfm->width >> prm_top->dpe_dw_dsx;
			vfm->height = vfm->height >> prm_top->dpe_dw_dsy;
			vfm->bitdepth_dw = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
			vfm->bitdepth_dw |= FULL_PACK_422_MODE;
			vfm->type_dw |= (VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD | VIDTYPE_VIU_422);

			cvs = &vfm->canvas0_config[0];
			cvs->bit_depth = 10;
			cvs->block_mode = 0;
			cvs->endian = 0;
			cvs->phy_addr = pch->c.addr_dw[b_idx];
			cvs->width = (vfm->width * 5) / 4;
			cvs->width = roundup(cvs->width, canvas_align_width);
			cvs->height = vfm->height;

			cvs = &vfm->canvas0_config[1];
			cvs->bit_depth = 10;
			cvs->block_mode = 0;
			cvs->endian = 0;
			cvs->phy_addr = pch->c.addr_dwuv[b_idx];
			cvs->width = (vfm->width * 5) / 4;
			cvs->width = roundup(cvs->width, canvas_align_width);
			cvs->height = vfm->height;
			dbg_i2("%s:wh=%d %d, xy=%d %d\n", __func__, vfm->width,
				vfm->height, prm_top->dpe_dw_dsx, prm_top->dpe_dw_dsy);
		}
		//mem_handle
		//table
		blk = &pch->c.blk_r_afbc_tab;
		vfm->mem_handle_1 = blk->c.b.blk_m.mem_handle;
		blk = &pch->c.blk_r_afbc_hd;
		if (pch->d->idx_hd)
			blk = &pch->c.blk_r_afbc_hd_b;
		vfm->mem_head_handle = blk->c.b.blk_m.mem_handle;
		blk = &pch->c.blk_r_nr[b_idx];
		vfm->mem_handle = blk->c.b.blk_m.mem_handle;
		blk = &pch->c.blk_r_dw[b_idx];
		vfm->mem_dw_handle = blk->c.b.blk_m.mem_handle;
	} else if (!dpss_en_afbc || !pch->c.o_afbc) {
		if (vfm->type_ext & VIDTYPE_EXT_AFRC_COMPRESS)
			vfm->type_ext &= ~VIDTYPE_EXT_AFRC_COMPRESS;
		dbg_i2("ch[%d]:afbce:o:vfm:type_ext=0x%x\n", pch->c.ch, vfm->type_ext);
	}

	if (!dpss_en_afbc && !pch->c.o_afbc) {
		vfm->mem_handle_1 = NULL;
		vfm->mem_head_handle = NULL;
		vfm->mem_dw_handle = NULL;
		if (pch->c.prm_top.is_di2pps &&
			dpss_dpe_nr_frm_cnt > dpss_pps_count) {
			blk = &pch->c.blk_r_di2pps[b_idx];
			vfm->mem_handle = blk->c.b.blk_m.mem_handle;
		} else {
			blk = &pch->c.blk_r_nr[b_idx];
			vfm->mem_handle = blk->c.b.blk_m.mem_handle;
		}
	}
	dbg_m2("ch[%d]:%d:hd:\n", pch->c.ch, b_idx);
	dbg_m2("\t%p %p %p %p\n", vfm->mem_handle_1,
		vfm->mem_handle, vfm->mem_head_handle, vfm->mem_dw_handle);
	if (vfm->type_ext & VIDTYPE_EXT_LCEVC) {
		//clear:
		vfm->type_ext &= ~VIDTYPE_EXT_LCEVC;
		vfm->type_ext &= ~VIDTYPE_EXT_LUMA_ONLY;
		vfm->enhance_vf = NULL;
		dbg_i2("%s:clear type_ext\n", __func__);
	}
	if (prm_top->out_mode == OUT_YUV420_NV12 ||
		prm_top->out_mode == OUT_YUV420_NV21) {
		vfm->canvas0Addr = (u32)-1;
		cvs = &vfm->canvas0_config[0];
		cvs->bit_depth = 8;
		cvs->phy_addr = pch->c.addr_nr[b_idx];
		if (!dpss_en_afbc && !pch->c.o_afbc &&
		    (vfm->type & VIDTYPE_VIU_NV12 ||
		     vfm->type & VIDTYPE_VIU_NV21) &&
		     vfm->bitdepth == 0)
			cvs->width = nr_i->sub_vf_in.cvs_h;
		//else to-do
		dbg_i2("\t:Y phy_addr:0x%lx, w = 0x%x, h = 0x%x\n", cvs->phy_addr,
			cvs->width, cvs->height);
		cvs->height = nr_i->sub_vf_in.height;
		cvs->block_mode = 0;
		cvs->endian = 0;
		cvs = &vfm->canvas0_config[1];
		cvs->bit_depth = 8;
		cvs->phy_addr = pch->c.addr_nr_uv[b_idx];
		cvs->width = vfm->canvas0_config[0].width;
		cvs->height = vfm->height / 2;
		cvs->block_mode = 0;
		cvs->endian = 0;
		dbg_i2("\t:uvphy_addr:0x%lx, w = 0x%x, h = 0x%x\n", cvs->phy_addr,
			cvs->width, cvs->height);
	} else if (prm_top->out_mode == OUT_YUV422_1_PLANE) {
		vfm->canvas0Addr = (u32)-1;
		cvs = &vfm->canvas0_config[0];
		cvs->bit_depth = 10;
		cvs->phy_addr = pch->c.addr_nr[b_idx];
		cvs->width = (vfm->width * 5) / 2;
		cvs->width = roundup(cvs->width, canvas_align_width);
		cvs->height = vfm->height;
		cvs->block_mode = 0;
		cvs->endian = 0;
		if (dpss_o_dw == 1) {
			cvs->phy_addr = pch->c.addr_dw[b_idx];
			if (vfm->type & VIDTYPE_COMPRESS) {
				vfm->width = vfm->compWidth >> prm_top->dpe_dw_dsx;
				vfm->height = vfm->compHeight >> prm_top->dpe_dw_dsy;
			} else {
				vfm->width = vfm->width >> prm_top->dpe_dw_dsx;
				vfm->height = vfm->height >> prm_top->dpe_dw_dsy;
			}
			cvs->width = (vfm->width * 5) / 2;
			cvs->width = roundup(cvs->width, canvas_align_width);
			cvs->height = vfm->height;
			vfm->bitdepth_dw = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
			vfm->bitdepth_dw |= FULL_PACK_422_MODE;
		}

		dbg_i2("\t:Y 422 phy_addr:0x%lx, w = 0x%x, h = 0x%x\n", cvs->phy_addr,
			cvs->width, cvs->height);
	} else if (!(vfm->type & VIDTYPE_COMPRESS) &&
		(prm_top->out_mode == OUT_YUV422_2_PLANE || dpss_o_42210 == 2)) {
		vfm->canvas0Addr = (u32)-1;
		cvs = &vfm->canvas0_config[0];
		cvs->bit_depth = 10;
		cvs->phy_addr = pch->c.addr_nr[b_idx];
		if (pch->c.prm_top.is_di2pps && dpss_dpe_nr_frm_cnt > dpss_pps_count) {
			cvs->phy_addr = pch->c.addr_di2pps[b_idx];
			dbg_pps0("di2pps out yaddr[%d]: 0x%lx\n", b_idx, cvs->phy_addr);
		} else if (pch->c.prm_top.is_pps && dpss_dpe_nr_frm_cnt > dpss_pps_count) {
			cvs->phy_addr = pch->c.addr_lc[b_idx];
			dbg_pps0("p yad[%d]: 0x%lx,0x%lx\n", b_idx,
				cvs->phy_addr, pch->c.addr_nr[b_idx]);
		} else {
			dbg_pps0("yad[%d]: 0x%lx,0x%lx\n", b_idx,
				cvs->phy_addr, pch->c.addr_nr[b_idx]);
		}
		cvs->width = (vfm->width * 5) / 4;
		cvs->width = roundup(cvs->width, canvas_align_width);
		cvs->height = vfm->height;

		cvs->block_mode = 0;
		cvs->endian = 0;
		vfm->canvas0Addr = (u32)-1;
		cvs = &vfm->canvas0_config[1];
		cvs->bit_depth = 10;
		cvs->phy_addr = pch->c.addr_nr_uv[b_idx];
		if (pch->c.prm_top.is_di2pps && dpss_dpe_nr_frm_cnt > dpss_pps_count) {
			cvs->phy_addr = pch->c.addr_di2pps_uv[b_idx];
			dbg_pps0("di2pps out uvaddr[%d]: 0x%lx\n", b_idx, cvs->phy_addr);
		} else if (pch->c.prm_top.is_pps && dpss_dpe_nr_frm_cnt > dpss_pps_count) {
			cvs->phy_addr = pch->c.addr_lc_uv[b_idx];
			dbg_pps0("is_pps pps out uvaddr[%d]: 0x%lx\n", b_idx, cvs->phy_addr);
		} else {
			dbg_pps0("uvaddr[%d]: 0x%lx\n", b_idx, cvs->phy_addr);
		}
		dbg_pps0("%s:is_pps ch=%d,=%d,=%d\n", __func__, pch->c.ch,
			pch->c.prm_top.is_pps, pch->c.prm_top.is_di2pps);

		cvs->width = (vfm->width * 5) / 4;
		cvs->width = roundup(cvs->width, canvas_align_width);
		cvs->height = vfm->height;
		cvs->block_mode = 0;
		cvs->endian = 0;
		dbg_i2("\t:Y 422-2 phy_addr:0x%lx, w = 0x%x, h = 0x%x\n", cvs->phy_addr,
			cvs->width, cvs->height);
	}
	if (dpss_dbg_o_type)
		vfm->type = dpss_dbg_o_type;
	if (dpss_dbg_o_bitd)
		vfm->bitdepth = dpss_dbg_o_bitd;
	if (dpss_dbg_o_cvs_w) {
		cvs = &vfm->canvas0_config[0];
		dbg_h2("force cvs w:%d -> %d ]n", cvs->width, dpss_dbg_o_cvs_w);
		cvs->width = dpss_dbg_o_cvs_w;
		cvs = &vfm->canvas0_config[1];
		cvs->width = dpss_dbg_o_cvs_w;
		//vfm->width = dpss_dbg_o_w;
	}
	if (dpss_dbg_o_cvs_h) {
		cvs = &vfm->canvas0_config[0];
		dbg_h2("force cvs w:%d -> %d ]n", cvs->height, dpss_dbg_o_cvs_h);
		cvs->height = dpss_dbg_o_cvs_h;
		cvs = &vfm->canvas0_config[1];
		cvs->height = dpss_dbg_o_cvs_h / 2;
		//vfm->height = dpss_dbg_o_h;
	}
	vfm->nr_buf_id = idx;
	dbg_h1("out ch%d,buf_index:%d,vfm index:0x%x,type:0x%x,flag:0x%x,in_cnt:0x%x,out:0x%x\n",
			pch->c.ch,
			vfm->nr_buf_id,
			vfm->frame_index,
			vfm->type,
			vfm->flag,
			pch->c.in_cnt,
			pch->c.out_cnt);
	pch->c.out_cnt++;
	pch->c.out_total_cnt++;
	if (pch->c.in_cnt == pch->c.out_cnt) { //tmp here
		dbg_i2("%s:hw finish\n", __func__);
		if (!pch->c.ch)
			dpss_get_hw()->st_idle |= C_BIT0;
		else
			dpss_get_hw()->st_idle |= C_BIT1;
	}

	if (prm_top->is_i) {
		if (pch->c.out_cnt == 1) {
			dbg_h2("first di out, return");
			pch->c.vfm_last = vfm;
			return;
		}
		pch->c.vfm_last = vfm_tmp;
	}

	dpss_dis_vfm(pch, vfm);
}

void dpss_s2_parser_rd_frc(struct dpss_ch_s *pch)
{
	unsigned int idx;
	struct vframe_s *vfm;
	// struct canvas_config_s *cvs;
	struct dpss_frc_i_s *frc_i;
	unsigned char b_idx;
	unsigned int rd_cnt;

//      if (g_dpss_tst_case == TST_CASE_IDX_0002)
	if (!pch->d || !pch->d->init)
		return;

	//nr_h_wait_mode(pch);//tmp here
	rd_cnt = 1;//frc_ocnt_status;
	if (!rd_cnt) {
		dbg_i2("%s:rd_cnt is 0\n", __func__);
		return;
	}

	if (pch->c.in_cnt < pch->c.out_cnt) {
		DBG_WARN("%s:overflow:0x%x, 0x%x\n", __func__, pch->c.in_cnt,
			 pch->c.out_cnt);
		//to-do
		return;
	}
	idx = pch->c.out_cnt % pch->c.o_b_nub;

	vfm = dpss_h_b_buf_put(pch, idx);
	b_idx = dpss_h_buf_get_b_idx(pch, idx);
	dbg_h1("%s:g_dpss_out_cnt=%d,idx = %d:%d\n", __func__,
			pch->c.out_cnt, idx, b_idx);

	if (!vfm || !vfm->dpss_data) {
		DBG_ERR("%s:no vfm?\n", __func__);
		return;
	}
	frc_i = lk_get_frc_i(vfm);

	// vfm->plane_num = 1;

	if (pch->d->frc_link)
		vfm->type_ext |= VIDTYPE_EXT_FRC_LINK;

	if (pch->d->is_afbcd) {
		//vfm->type = VIDTYPE_VIU_NV21 | VIDTYPE_VIU_FIELD;
		//vfm->bitdepth = 0;
	}
	if (b_idx > pch->c.o_b_nub) {
		DBG_ERR("%s:idx is overflow:%d,%d\n", __func__, b_idx,
			pch->c.o_b_nub);
		b_idx = 0;
	}
	if (dpss_dbg_o_w) {
		dbg_f2("force o w=%d -> %d\n", vfm->width, dpss_dbg_o_w);
		vfm->width = dpss_dbg_o_w;
	}
	if (dpss_dbg_o_h) {
		dbg_f2("force o h=%d -> %d\n", vfm->height, dpss_dbg_o_h);
		vfm->height = dpss_dbg_o_h;
	}
	dbg_f2("frm_comp_size(%d,%d), size(%d,%d)\n",
		vfm->compWidth, vfm->compHeight,
		vfm->width, vfm->height);
	if (pch->d->mc_skip_mode == 3) {
		if (pch->d->is_afbcd) {
			vfm->compWidth = vfm->compWidth >> 1;
			vfm->compHeight = vfm->compHeight >> 1;
		} else {
			vfm->width = vfm->width >> 1;
			vfm->height = vfm->height >> 1;
		}
	} else if (pch->d->mc_skip_mode == 1) {
		if (pch->d->is_afbcd) {
			// vfm->compWidth = vfm->compWidth;
			vfm->compHeight = vfm->compHeight >> 1;
		} else {
			// vfm->width = vfm->width;
			vfm->height = vfm->height >> 1;
		}
	} else if (pch->d->mc_skip_mode == 2) {
		if (pch->d->is_afbcd) {
			vfm->compWidth = vfm->compWidth >> 1;
			// vfm->compHeight = vfm->compHeight;
		} else {
			vfm->width = vfm->width >> 1;
			// vfm->height = vfm->height;
		}
	}
	dbg_f2("mc_skip_mode frm_comp_size(%d,%d), size(%d,%d)\n",
		vfm->compWidth, vfm->compHeight,
		vfm->width, vfm->height);

	vfm->canvas0Addr = (u32)-1;
//      cvs = &vfm->canvas0_config[0];
//      cvs->bit_depth = 8;
//      cvs->phy_addr = pch->c.addr_nr[b_idx];
//      cvs->width = frc_i->sub_vf_in.width;
//      cvs->height = frc_i->sub_vf_in.height;
//      cvs->block_mode = 0;
//      cvs->endian = 0;
//      cvs = &vfm->canvas0_config[1];
//      cvs->bit_depth = 8;
//      cvs->phy_addr = pch->c.addr_nr_uv[b_idx];
//      cvs->width = frc_i->sub_vf_in.width;
//      cvs->height =frc_i->sub_vf_in.height / 2;
//      cvs->block_mode = 0;
//      cvs->endian = 0;

		//vfm->width = dpss_dbg_o_w;
		//vfm->height = dpss_dbg_o_h;
//		release buffer;
//		hw_release_buf(prm_top->src_mode); //check
//	if (prm_top->sw_tbc_mode)
//		tbc_mode = true;
//	else
//		tbc_mode = false;
//	hw_release_buf(prm_top->src_mode, idx, tbc_mode); //check to-do

	pch->c.out_cnt++;
	dpss_dis_vfm(pch, vfm);
}

void dpss_s2_recycle_back(struct dpss_ch_s *pch)
{
	struct vframe_s *vfm;
	struct vframe_s *vfm_in;

	struct dpss_nr_i_s *nr_i;
	int i;
	unsigned int len;
	unsigned char type;

//-----------------------------------------------------------
	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_IN_RECYCLE].f);

	if (len) {
		for (i = 0; i < len; i++) {
			vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_IN_RECYCLE);

			if (!vfm || !vfm->dpss_data) {
				DBG_ERR("%s:%d no vfm\n", __func__, i);
				break;
			}
			nr_i = lk_get_nr_i(vfm);

			type = lk_owner_get(vfm);

			if (type == DPSS_VF_OWNER_NR) {
				vfm_in = nr_i->in_vfm;
				if (!vfm_in) {
					DBG_ERR("%s:no vfm_in\n", __func__);
				} else {
					vfm_in->dpss_flg |= DPSS_FLG_BYPASS;
					dpss_in_rck_in(pch, vfm_in);
				}
				nr_i->in_vfm = NULL;
				vfm->dpss_flg = 0;
				memset(&nr_i->sub_vf_in, 0, sizeof(nr_i->sub_vf_in));
				//to-do: need mv sub infor in one;
				dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IDLE_NR], vfm);
			} else {
				DBG_WARN("%s:recycle\n", __func__);
			}
		}
	}
//-----------------------------------------------------------
	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_BACK].f);
	if (!len)
		return;

	//back to nr or frc:
	for (i = 0; i < len; i++) {
		vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_BACK);
		nr_i = lk_get_nr_i(vfm);
		type = lk_owner_get(vfm);
		if (type == DPSS_VF_OWNER_NR) {
			vfm->dpss_flg = 0;
			memset(&nr_i->sub_vf_in, 0, sizeof(nr_i->sub_vf_in));
			dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IDLE_NR], vfm);
		} else if (type == DPSS_VF_OWNER_OTHER) {
			//bypass mode:
			//to-do, need put vfm back to provider?
		}
	}
}

void dpss_s2_recycle_back_frc(struct dpss_ch_s *pch)
{
	struct vframe_s *vfm;
	struct frc_chip_st *pchip_st;

	//struct dpss_nr_i_s *nr_i;
	struct dpss_frc_i_s *frc_i;
	int i;
	unsigned int len;
	unsigned char type;
	u32 rls_idx;
	u32 rls_buf_is_empty;

	len = kfifo_len(&pch->d->q_ch[EDPSS_Q_CH_BACK].f);
	if (!len)
		return;
	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	if (dpss_dae_frm_cnt_src0 <= 2)
		return;

	dbg_h2("%s vpp return num:%d\n", __func__, len);
	for (i = 0; i < len; i++) {
		if (!vpu_dequeue(&inp_bufQ, &rls_idx, &rls_buf_is_empty)) {
			if (i == 0)
				dbg_h2("%s is empty (%d)\n", __func__, i);
			return;
		}
		vfm = dpss_q_out_vfm(pch, EDPSS_Q_CH_BACK);
		frc_i = lk_get_frc_i(vfm);
		if (frc_i->inp_idx != rls_idx)
			dbg_h2("%s buffer id don't match(%d, %d)\n", __func__,
					frc_i->inp_idx, rls_idx);
		else
			dbg_h2("%s rls buffer id (%d,%d)\n", __func__,
					frc_i->inp_idx, rls_idx);
		type = lk_owner_get(vfm);
		if (type ==  DPSS_VF_OWNER_FRC)
			if (frc_i->in_vfm) //recycle input vfm:
				//recycle input vfm:
				dpss_q_in_vfm(&pch->d->q_ch[EDPSS_Q_CH_IN_RECYCLE], vfm);
	}
}

/********************************************************/
//buf:small mif:
void dpss_buf_s_count_rdma(struct dpss_buf_rdma_s *info)
{
	//tmp:
	if (!info)
		return;
	info->size_total = 0x1000;
	info->size_one = DPSS_RDMA_RAM_ONE_SIZE;
	info->nub_max = info->size_total / info->size_one;

	dbg_a0("%s:t=0x%x, o= 0x%x, nub=%d\n", __func__,
	       info->size_total, info->size_one, info->nub_max);
}

#define m_cnt(h, v, bit, off, a_b)  \
	(((((((h) * (bit)) + (off)) >> (a_b)) << (a_b)) >> 3) * (v))

/********************************************************/
//buf:small mif:
void dpss_buf_s_count_hd(bool hd, struct dpss_buf_sml_s *sml_i, struct dpss_ch_s *pch)
{
	unsigned int h = 3840, v = 2160;
	unsigned int m_size = 0;
	unsigned int canvas_width = 3840, canvas_align_width = 32;
	unsigned int logo_h_size = 960, logo_v_size = 540, logo_v_size_i = 576;
	unsigned int me_blk_h_size = 240, me_blk_v_size = 135, me_blk_v_size_i = 144;
	unsigned int nr_addr_align = 16; //PAGE_SIZE;	//
	unsigned char num_aepe;
//	unsigned char num_dpe_o;
	unsigned char num_dblk;
	unsigned char num_nr_wrpt;
	unsigned char num_dpe_ro;
	unsigned char num_nr_me_ro;
	struct frc_chip_st *pchip_st = NULL;

#ifdef _HIS_CODE_
	bool is_p = false;

	if (pch->c.parm.di_parm.is_interlace)
		is_p = false;
	else
		is_p = true;
	dbg_m2("%s is_p = %d\n", __func__, is_p);
#endif
	pchip_st = dpss_get_frc_st();
	/* tmp */
//	sml_i->en_dct = true;
//	sml_i->en_dw = true;
//	sml_i->en_grad = true;
//	sml_i->en_tfbc = true;
//	sml_i->en_frc = true;

//	sml_i->en_afbc = true;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	canvas_align_width = 64;
	if (hd) {
		h = 960;
		v = 544;	//540?
		canvas_width = h;
	}

	logo_h_size = roundup(logo_h_size, canvas_align_width);
	me_blk_h_size = roundup(me_blk_h_size, canvas_align_width);

	canvas_align_width = 4 * 1024;
	// SRC/NR DW		960	540	1	422	10
	// mix			960	540	1	null	8
	// mv			240	135	1	null	64
	// logo(filt/scc)	960	540	1	null	1
	// melogo		240	135	1	null	1
	// mtn			960	540	1	null	4
	// blk info(nr di)	240	135	1	null	32
	// dmsq		960	540	1	null	8
	// dcntr map		960	540	1	null	8
	// tfbc		960	540	1	null	8
	// dcntr grid		81	46	1	null	384
	// gradh		3840	1	1	null	32
	// gradv		1	2160	1	null	32

	dbg_m1("%s:canvas_width %d\n", __func__, canvas_width);
	//size_dw fmt = 422 10 bit h * v
	m_size = roundup(h * 10 * 2 / 8, 128) * v;
	//sml_i->size_dw = roundup(m_size, canvas_align_width);
	sml_i->size_dw = roundup(m_size, PAGE_SIZE); //09-16
	//------------------------
	//size_mix  h * v * 1 * 8 / 8

	sml_i->size_mix = m_cnt(960, 544, 8, 511, 9);
	//------------------------
	//size_mv  240 * 135 * 1 * 64 / 8

	sml_i->size_mv =  m_cnt(240, 136, 64, 511, 9);
	sml_i->size_alp =  m_cnt(960, 544, 4, 511, 9);
	//------------------------
	//size_logo  h * v * 1 * 1 / 8
	m_size = (960 * 1 + 511) / 512 * 512 * v / 8;//h * v / 8;
	sml_i->size_logo = roundup(m_size, canvas_align_width);
	//------------------------
	//size_melogo  240 * 135 * 1  / 8
	m_size = (960 * 1 + 511) / 512 * 512 * v / 8;//240 * 135 / 8;
	sml_i->size_melogo = roundup(m_size, canvas_align_width);

	sml_i->size_mtn =  m_cnt(960, 544, 8, 511, 9);
	sml_i->size_blk_info =  m_cnt(240, 136, 24, 511, 9);
	sml_i->size_dmsq =  m_cnt(1920, 1088, 4, 511, 9);

	sml_i->size_tfbc =  m_cnt(128, 96, 8, 511, 9);
	sml_i->size_grad_h =  m_cnt(3840, 1, 32, 511, 9);
	sml_i->size_grad_v =  m_cnt(4, 2160, 32, 511, 9);
	sml_i->size_dct_grid =  m_cnt(648, 46, 128, 511, 9);
	sml_i->size_dct_y =  m_cnt(960, 544, 8, 511, 9);
	sml_i->size_dct_c = sml_i->size_dct_y << 1; // x 2
	sml_i->size_ro1 =  m_cnt(580, 1, 32, 511, 9);
	sml_i->size_ro2 =  sml_i->size_ro1;

	// frc size -----------------------------------------------------------
	m_size = (logo_h_size * 8 + 511) / 512 * 512 / 8 * 540; // 8000;//8 bit @dongfei
	sml_i->size_frc_inp_mbuf0 = roundup(m_size, canvas_align_width);
//      dbg_m1("\tsize_frc_inp_mbuf0:0x%x, 0x%x\n", m_size, sml_i->size_frc_inp_mbuf0);
//
//      m_size = (me_h_size *16 + 511)/8 * me_v_size; // 8000;
	sml_i->size_frc_inp_mbuf1 = roundup(m_size, canvas_align_width);
//      dbg_m1("\tsize_frc_inp_mbuf1:0x%x, 0x%x\n", m_size, sml_i->size_frc_inp_mbuf1);

	m_size = (logo_h_size * 8 + 511) / 8 * logo_v_size;//;8000
	sml_i->size_frc_me_mbuf = roundup(m_size, canvas_align_width);

	m_size = (me_blk_h_size * 64 + 511) / 8 * me_blk_v_size;	// 8000
	sml_i->size_frc_me_nc_uni_mv = roundup(m_size, canvas_align_width);

	m_size = (me_blk_h_size * 64 + 511) / 8 * me_blk_v_size;	// 8000
	sml_i->size_frc_me_cn_uni_mv = roundup(m_size, canvas_align_width);

	m_size = (me_blk_h_size * 64 + 511) / 8 * me_blk_v_size;	// 8000
	sml_i->size_frc_me_pc_phs_mv = roundup(m_size, canvas_align_width);

	m_size = (logo_h_size * 6 + 511) / 8 * logo_v_size;	// 8000
	sml_i->size_frc_logo_iir = roundup(m_size, canvas_align_width);

	m_size = (logo_h_size * 5 + 511) / 8 * logo_v_size;	// 8000
	sml_i->size_frc_logo_ssc = roundup(m_size, canvas_align_width);

	m_size = (me_blk_h_size * 16 + 511) / 8 * me_blk_v_size;	// 8000
	sml_i->size_frc_blk_logo = roundup(m_size, canvas_align_width);

	m_size = (logo_h_size * 1 + 511) / 8 * logo_v_size;	// 8000
	if (pch->c.support_i &&  pchip_st &&
		pchip_st->chip == ID_T6X)
		m_size = (logo_h_size * 1 + 511) / 8 * logo_v_size_i;
	sml_i->size_frc_pix_logo = roundup(m_size, canvas_align_width);

	m_size = (me_blk_h_size * 64 + 511) / 8 * me_blk_v_size;	// 8000
	sml_i->size_frc_vp_mc_mv = roundup(m_size, canvas_align_width);

	m_size = (me_blk_h_size * 1 + 511) / 8 * me_blk_v_size;	// 8000
	if (pch->c.support_i &&  pchip_st &&
		pchip_st->chip == ID_T6X)
		m_size = (me_blk_h_size * 1 + 511) / 8 * me_blk_v_size_i;
	sml_i->size_frc_vp_mc_logo = roundup(m_size, canvas_align_width);

	m_size = 0x10000;	// 0x100
	sml_i->size_frc_mer0 = roundup(m_size, canvas_align_width);
	dbg_m1("\tsize_frc_mer0:0x%x, 0x%x\n", m_size, sml_i->size_frc_mer0);
	// frc size -----------------------------------------------------------
	//------------------------DPSS_SML_NUB  (8)

	num_aepe     = pch->c.num_aepe;
	//num_dpe_o    = sml_i->num_dpe_o;
	num_nr_wrpt  = pch->c.num_nr_wrpt;
	num_dblk     = pch->c.num_pq_buf;
	num_dpe_ro   = pch->c.num_pq_buf;
	num_nr_me_ro = pch->c.num_pq_buf;

	sml_i->size_ts_mix =
	    roundup((sml_i->size_mix) * num_nr_wrpt, nr_addr_align);
	sml_i->size_ts_mv =
	    roundup(sml_i->size_mv * num_aepe, nr_addr_align); //num
	if (!pch->c.support_i)
		sml_i->size_ts_alp = 0;
	else
		sml_i->size_ts_alp =
		roundup(sml_i->size_alp * num_aepe, nr_addr_align); //num
	sml_i->size_ts_logo =
	    roundup(sml_i->size_logo * DPSS_SML_NUB, PAGE_SIZE);
	sml_i->size_ts_melogo =
	    roundup(sml_i->size_melogo * DPSS_SML_NUB, PAGE_SIZE);
	sml_i->size_ts_mtn =
	    roundup(sml_i->size_mtn * num_aepe, nr_addr_align);
	sml_i->size_ts_blk_info =
	    roundup(sml_i->size_blk_info * num_aepe, nr_addr_align);
	sml_i->size_ts_dmsq =
	    roundup(sml_i->size_dmsq * DPSS_DMS_NUB, nr_addr_align);
	sml_i->size_ts_dct_grid =
	    roundup(sml_i->size_dct_grid * num_aepe, nr_addr_align);
	sml_i->size_ts_dct_y =
	    roundup(sml_i->size_dct_y * num_aepe, nr_addr_align);
	sml_i->size_ts_dct_c =
	    roundup(sml_i->size_dct_c * num_aepe, nr_addr_align);
	sml_i->size_ts_tfbc =
	    roundup(sml_i->size_tfbc * num_aepe, nr_addr_align);
	sml_i->size_ts_grad_h =
	    roundup(sml_i->size_grad_h * num_dblk, nr_addr_align);
	sml_i->size_ts_grad_v =
	    roundup(sml_i->size_grad_v * num_dblk, nr_addr_align);
	sml_i->size_ts_ro1 =
	    roundup(sml_i->size_ro1 * num_nr_me_ro, nr_addr_align);
	sml_i->size_ts_ro2 =
	    roundup(sml_i->size_ro2 * num_dpe_ro, nr_addr_align);
#ifdef _HIS_CODE_
	sml_i->size_ts_dw =
	    roundup(sml_i->size_dw * DPSS_HW_LOOP_IN_OUT_BUF_NUB,
		    PAGE_SIZE);
#endif
	sml_i->size_ts_frc_inp_mbuf0 =
	    roundup(sml_i->size_frc_inp_mbuf0, PAGE_SIZE);
	sml_i->size_ts_frc_inp_mbuf1 =
	    roundup(sml_i->size_frc_inp_mbuf1, PAGE_SIZE);
	sml_i->size_ts_frc_me_mbuf =
	    roundup(sml_i->size_frc_me_mbuf * DPSS_SML_NUB, PAGE_SIZE);
	sml_i->size_ts_frc_me_nc_uni_mv =
	    roundup(sml_i->size_frc_me_nc_uni_mv * 4, PAGE_SIZE);
	sml_i->size_ts_frc_me_cn_uni_mv =
	    roundup(sml_i->size_frc_me_cn_uni_mv * 4, PAGE_SIZE);
	sml_i->size_ts_frc_me_pc_phs_mv =
	    roundup(sml_i->size_frc_me_pc_phs_mv * 4, PAGE_SIZE);
	sml_i->size_ts_frc_logo_iir =
	    roundup(sml_i->size_frc_logo_iir * 2, PAGE_SIZE);
	sml_i->size_ts_frc_logo_ssc =
	    roundup(sml_i->size_frc_logo_ssc * 2, PAGE_SIZE);
	sml_i->size_ts_frc_blk_logo =
	    roundup(sml_i->size_frc_blk_logo * DPSS_SML_NUB, PAGE_SIZE);
	sml_i->size_ts_frc_pix_logo =
	    roundup(sml_i->size_frc_pix_logo * DPSS_SML_NUB, PAGE_SIZE);
	sml_i->size_ts_frc_vp_mc_mv =
	    roundup(sml_i->size_frc_vp_mc_mv * DPSS_SML_NUB, PAGE_SIZE);
	sml_i->size_ts_frc_vp_mc_logo =
	    roundup(sml_i->size_frc_vp_mc_logo * DPSS_SML_NUB, PAGE_SIZE);
	sml_i->size_ts_frc_mero =
	    roundup(sml_i->size_frc_mer0 * 8, PAGE_SIZE);

	dbg_m1("\tsize_ts_frc_me_mbuf:0x%8x\n", sml_i->size_ts_frc_me_mbuf);
	dbg_m1("\tsize_ts_ro1:0x%8x\n", sml_i->size_ts_ro1);
	dbg_m1("\tsize_ts_frc_me_nc_uni_mv:0x%8x\n",
	       sml_i->size_ts_frc_me_nc_uni_mv);
	dbg_m1("\tsize_ts_frc_me_cn_uni_mv:0x%8x\n",
	       sml_i->size_ts_frc_me_cn_uni_mv);
	dbg_m1("\tsize_ts_frc_me_pc_phs_mv:0x%8x\n",
	       sml_i->size_ts_frc_me_pc_phs_mv);
	dbg_m1("\tsize_ts_frc_logo_iir:0x%8x\n", sml_i->size_ts_frc_logo_iir);
	dbg_m1("\tsize_ts_frc_logo_ssc:0x%8x\n", sml_i->size_ts_frc_logo_ssc);
	dbg_m1("\tsize_ts_frc_blk_logo:0x%8x\n", sml_i->size_ts_frc_blk_logo);
	dbg_m1("\tsize_ts_frc_pix_logo:0x%8x\n", sml_i->size_ts_frc_pix_logo);
	dbg_m1("\tsize_ts_frc_vp_mc_mv:0x%8x\n", sml_i->size_ts_frc_vp_mc_mv);
	dbg_m1("\tsize_ts_frc_vp_mc_logo:0x%8x\n",
	       sml_i->size_ts_frc_vp_mc_logo);
	dbg_m1("\tsize_ts_frc_mero:0x%8x\n", sml_i->size_ts_frc_mero);

	sml_i->size_t_nr_s = sml_i->size_ts_mix +
	    sml_i->size_ts_mv +
	    sml_i->size_ts_alp +
	    sml_i->size_ts_mtn +
	    sml_i->size_ts_blk_info +
			sml_i->size_ts_ro1	+
			sml_i->size_ts_ro2	+
			sml_i->size_ts_dmsq;
	sml_i->size_t_frc = sml_i->size_ts_frc_inp_mbuf0 +
		sml_i->size_ts_frc_inp_mbuf1 + sml_i->size_ts_frc_me_mbuf +
	    sml_i->size_ts_frc_me_nc_uni_mv +
	    sml_i->size_ts_frc_me_cn_uni_mv +
	    sml_i->size_ts_frc_me_pc_phs_mv +
	    sml_i->size_ts_frc_logo_iir +
	    sml_i->size_ts_frc_logo_ssc +
	    sml_i->size_ts_frc_blk_logo +
	    sml_i->size_ts_frc_pix_logo +
	    sml_i->size_ts_frc_vp_mc_mv +
	    sml_i->size_ts_frc_vp_mc_logo + sml_i->size_ts_frc_mero;

	if (pch->c.en_dct) {
		sml_i->size_t_nr_s = sml_i->size_t_nr_s + sml_i->size_ts_dct_y +
			sml_i->size_ts_dct_c + sml_i->size_ts_dct_grid;
	} else {
		sml_i->size_ts_dct_grid = 0;
		sml_i->size_ts_dct_y = 0;
		sml_i->size_ts_dct_c = 0;
	}
	if (pch->c.en_tfbc)
		sml_i->size_t_nr_s = sml_i->size_t_nr_s + sml_i->size_ts_tfbc;
	else
		sml_i->size_ts_tfbc = 0;

	sml_i->size_t_nr_s = sml_i->size_t_nr_s +
	    sml_i->size_ts_grad_h + sml_i->size_ts_grad_v;

	if (!pch->c.en_dw)
		sml_i->size_ts_dw = 0;

	sml_i->size_t_nr_s = roundup(sml_i->size_t_nr_s, PAGE_SIZE);

	if (pch->c.en_frc) {
		sml_i->size_t_nr_frc_s = sml_i->size_t_nr_s + sml_i->size_t_frc;
	} else {
		sml_i->size_t_frc = 0;
		sml_i->size_t_nr_frc_s = sml_i->size_t_nr_s;
	}

	sml_i->size_t_dct = sml_i->size_ts_dct_grid +
		sml_i->size_ts_dct_y + sml_i->size_ts_dct_c;
	sml_i->size_t_grad = sml_i->size_ts_grad_h + sml_i->size_ts_grad_v;
	dbg_m1("\tsize_t_frc:0x%x\n", sml_i->size_t_frc);
	dbg_m1("\tsize_s_nr_only:0x%x, 0x%x\n", m_size, sml_i->size_t_nr_s);
	dbg_m1("\tsize_s_nr_frc:0x%x\n", sml_i->size_t_nr_frc_s);
	//------------------------
	//to-do sml_i->size_s_frc_only
	//to-do sml_i->size_s_nr_frc
}

/*copy from count_mm_p */
static int dpss_buf_nr_count(struct dpss_mm_p_cnt_in_s *i_cfg,
			     struct dpss_mm_p_cnt_out_s *o_cfg)
{
	unsigned int tmpa, tmpb;
	unsigned int height;
	unsigned int width;
	unsigned int canvas_height;

	unsigned int nr_width;
	unsigned int canvas_align_width = 64;

	if (!i_cfg || !o_cfg) {
		DBG_ERR("%s:no input\n", __func__);
		return -1;
	}
	height = i_cfg->h;
	width = i_cfg->w;
	canvas_height = roundup(height, 32);
	nr_width = width;
	o_cfg->mode = i_cfg->mode;
	/**********************************************/
	/* count buf info */
	/**********************************************/
	if (i_cfg->mode == EDPSS_DP_MODE_422_10BIT_PACK ||
			i_cfg->mode == EDPSS_DP_MODE_422_10BIT_2PACK)
		nr_width = (width * 5) / 4;
	else if (i_cfg->mode == EDPSS_DP_MODE_422_12BIT_1PACK ||
			i_cfg->mode == EDPSS_DP_MODE_422_12BIT_2PACK)
		nr_width = (width * 6) / 2;
	else if (i_cfg->mode == EDPSS_DP_MODE_422_10BIT)
		nr_width = (width * 3) / 2;
	else if (i_cfg->mode == EDPSS_DP_MODE_420_10BIT)
		nr_width = (width * 3 * 10) / (16 * 2);
	else
		nr_width = width;

	/* p */
	o_cfg->dbuf_hsize = roundup(width, canvas_align_width);	//??

	if (i_cfg->mode == EDPSS_DP_MODE_NV21_8BIT) {
		nr_width = roundup(nr_width, canvas_align_width);
		tmpa = (nr_width * canvas_height) >> 1;	/*uv */
		tmpb = roundup(tmpa, PAGE_SIZE);
		tmpa = roundup(nr_width * canvas_height, PAGE_SIZE);

		o_cfg->off_uv = tmpa;
		o_cfg->size_total = tmpa + tmpb;
		o_cfg->size_page = o_cfg->size_total >> PAGE_SHIFT;
		o_cfg->cvs_w = nr_width;
		o_cfg->cvs_h = canvas_height;
	} else if (i_cfg->mode == EDPSS_DP_MODE_422_10BIT_2PACK ||
		i_cfg->mode == EDPSS_DP_MODE_422_12BIT_2PACK) {
		nr_width = roundup(nr_width, canvas_align_width);
		tmpa = (nr_width * canvas_height);/*uv*/
		tmpb = roundup(tmpa, PAGE_SIZE);
		tmpa = roundup(nr_width * canvas_height, PAGE_SIZE);
		o_cfg->off_uv		= tmpa;
		o_cfg->size_total	= tmpa + tmpb;
		o_cfg->size_page = o_cfg->size_total >> PAGE_SHIFT;
		o_cfg->cvs_w	= nr_width;
		o_cfg->cvs_h	= canvas_height;
	} else {
		/* 422, 420 10bit */
		tmpa = roundup(nr_width * canvas_height * 2, PAGE_SIZE);
		o_cfg->size_total = tmpa;
		o_cfg->size_page = o_cfg->size_total >> PAGE_SHIFT;
		o_cfg->cvs_w = nr_width << 1;
		o_cfg->cvs_h = canvas_height;
		o_cfg->off_uv = roundup(nr_width * canvas_height * 10 / 8, PAGE_SIZE);
		//o_cfg->size_total >> 1;//tmp
	}

	dbg_m1("%s: mode:%d, size(total:0x%x, page:0x%x), cvs:<%d, %d>, off_uv=0x%x, nr<%d, %d>.\n",
		__func__,
		i_cfg->mode,
		o_cfg->size_total,
		o_cfg->size_page,
		o_cfg->cvs_w,
		o_cfg->cvs_h,
		o_cfg->off_uv,
		nr_width,
		canvas_height);
	return 0;
}

void dpss_buf_infor(struct dpss_ch_s *pch)
{
	struct dpss_mm_p_cnt_in_s *i_cfg, in_cfg;
	struct dpss_dd_s *dd;

	dd = dpss_get_dd();

	dd->afrc_bs_y = dpss_afrc_y; //tmp here
	dd->afrc_bs_c = dpss_afrc_c; //tmp here
	dpss_buf_s_count_rdma(&dd->rdma_info);
	//before dpss_buf_s_count_hd
	dpss_afbc_info_cnt(1920, 1088, &dd->hd_afbc_info);

	//to-do :mem is 422, fmt is 420 8bit
	afrc_info_cnt(1920, 1088,
		dd->afrc_bs_y, dd->afrc_bs_c, 1, &dd->hd_afrc_info);
	dpss_buf_s_count_hd(true, &dd->hd_sml_info, pch);

	i_cfg = &in_cfg;
	memset(i_cfg, 0, sizeof(*i_cfg));
	if (dpss_nr_debug == 1 || dpss_force_nr_debug) {
		i_cfg->h = 2160;// for di debug pattern,todo temp
		i_cfg->w = 3840;
	} else if (dpss_nr_debug == 2) {
		i_cfg->h = 2160;// for di debug pattern,todo temp
		i_cfg->w = 3840;
	} else {
		i_cfg->h = 1088;
		i_cfg->w = 1920;
	}
	if (pps_out_w) {
		i_cfg->h = pps_out_h;
		i_cfg->w = pps_out_w;
	}
	dbg_i0("%s:is_pps ch=%d,=%d,=%d\n", __func__,
		pch->c.ch, pch->c.prm_top.is_pps, pch->c.prm_top.is_di2pps);
	i_cfg->mode = dpss_o_42212;
	dpss_buf_nr_count(i_cfg, &dd->hd_nr_info);
	memcpy(&dd->hd_lc_info, &dd->hd_nr_info, sizeof(dd->hd_lc_info));
	if (!dpss_nr_debug && !dpss_force_nr_debug) {
		dd->hd_lc_info.cvs_h = dd->hd_lc_info.cvs_h / 2;
		dd->hd_lc_info.off_uv =  dd->hd_lc_info.off_uv / 2;
		dd->hd_lc_info.size_total = dd->hd_lc_info.size_total / 2;
	}
	dd->hd_lc_info.size_page = dd->hd_lc_info.size_total >> PAGE_SHIFT;
}

void dpss_buf_infor_4k(struct dpss_ch_s *pch)
{
	struct dpss_mm_p_cnt_in_s *i_cfg, in_cfg;
	struct dpss_dd_s *dd;
	unsigned int w = 3840, h = 2160;
//	struct dpss_mm_p_cnt_out_s tmp_info;

	dd = dpss_get_dd();

	dpss_buf_s_count_rdma(&dd->rdma_info);
	//before dpss_buf_s_count_hd
	dpss_afbc_info_cnt(w, h, &dd->fd_afbc_info);

	afrc_info_cnt(w, h,
			dd->afrc_bs_y, dd->afrc_bs_c, 1, &dd->fd_afrc_info);

	dpss_buf_s_count_hd(true, &dd->uhd_sml_info, pch);

	i_cfg = &in_cfg;
	memset(i_cfg, 0, sizeof(*i_cfg));
	i_cfg->h = h;
	i_cfg->w = w;
	i_cfg->mode = dpss_o_42210_4K;
	dpss_buf_nr_count(i_cfg, &dd->fd_nr_info);
}

void dpss_pch_init(void)
{
	int i;
	struct dpss_ch_s *pch;

	for (i = 0; i < DPSS_CHANNEL_MAX; i++) {
		pch = dpss_get_ch(i);
		mutex_init(&pch->c.ch_lock);
		pch->c.check_st1 = DPSS_CHECK_ST1;

		//07-10 need check
		if (!i) {
			prm_dpss_top = &pch->c.prm_top;
			prm_dpss_dae = &pch->c.prm_dae;
			prm_dpss_dpe = &pch->c.prm_dpe;
		} else if (i == 1) {
			prm_dpss_top2 = &pch->c.prm_top;
			prm_dpss_dae2 = &pch->c.prm_dae;
			prm_dpss_dpe2 = &pch->c.prm_dpe;
		}

		dbg_s2("%s:ch[%d]\n", __func__, i);
	}
}

void dpss_check_st(struct dpss_ch_s *pch, unsigned int pos)
{
	if (pch->c.check_st1 != DPSS_CHECK_ST1) {
		DBG_ERR("%s:ch[%d],pos[%d], sta:0x%x\n",
			__func__, pch->c.ch, pos, pch->c.check_st1);
	}
}

#define TVP_MEM_PAGES	0xffff

static bool _mm_cma_alloc(struct device *dev, size_t count, struct dpss_mm_s *o)
{
#ifdef RUN_ON_ARM
	o->ppage = dma_alloc_from_contiguous(dev, count, 0, 0);
	if (o->ppage) {
		o->addr = page_to_phys(o->ppage);
		return true;
	}
	DBG_ERR("%s: failed\n", __func__);

	return false;
#else
	DBG_INF("%s:null\n", __func__);
	return true;
#endif
}

static bool _mm_codec_alloc(const char *owner, size_t count,
			    unsigned int cma_mode, struct dpss_mm_s *o, bool tvp_flg)
{
#ifdef RUN_ON_ARM
	int flags = 0;
	bool istvp = false;

	if (tvp_flg) {
		istvp = true;
		flags |= CODEC_MM_FLAGS_TVP;
	} else {
		flags |= CODEC_MM_FLAGS_RESERVED | CODEC_MM_FLAGS_CMA;
	}

	if (!(cma_mode & C_BIT0) && !istvp)
		flags = CODEC_MM_FLAGS_CMA_FIRST;
	if (cma_mode & C_BIT1)
		flags |= CODEC_MM_FLAGS_FOR_TRY_PREALLOC;
	o->addr = codec_mm_alloc_for_dma(owner, count, 0, flags);

	if (o->addr == 0) {
		DBG_ERR("%s: failed\n", __func__);
		return false;
	}
	dbg_m1("%s:cma_mode=0x%x, flags=0x%x\n", __func__, cma_mode, flags);
	if (istvp)
		o->ppage = (struct page *)TVP_MEM_PAGES;
	else
		o->ppage = codec_mm_phys_to_virt(o->addr);
#endif				//
	return true;
}

static bool _mm_codec_alloc_hd(const char *owner, size_t count,
			    unsigned int cma_mode, struct dpss_mm_s *o, bool tvp_flg)
{
#ifdef RUN_ON_ARM
	int flags = 0;
	bool istvp = false;
	struct codec_mm_s *mm;

	if (tvp_flg) {
		istvp = true;
		flags |= CODEC_MM_FLAGS_TVP;
	} else {
		flags |= CODEC_MM_FLAGS_RESERVED | CODEC_MM_FLAGS_CMA;
	}

	if (!(cma_mode & C_BIT0) && !istvp)
		flags = CODEC_MM_FLAGS_CMA_FIRST;
	if (cma_mode & C_BIT1)
		flags |= CODEC_MM_FLAGS_FOR_TRY_PREALLOC;

	mm = codec_mm_alloc(owner, count << PAGE_SHIFT, 0, flags, -1);
	if (!mm) {
		DBG_ERR("%s: failed\n", __func__);
		return false;
	}
	dbg_m1("%s:hd:%p flags=0x%x\n", __func__, mm, flags);

	o->addr = mm->phy_addr;
	if (o->addr == 0) {
		PR_ERR("%s: failed\n", __func__);
		return false;
	}
	o->mem_handle = (void *)mm;
	if (istvp)
		o->ppage = (struct page *)TVP_MEM_PAGES;
	else
		o->ppage = codec_mm_phys_to_virt(o->addr);
#endif				//
	return true;
}

//static
bool dpss_mm_alloc_api2(struct dpss_mem_a_s *in_para, struct dpss_mm_s *o)
{
	bool ret = false;
	struct dpss_dev_s *de_devp = dpss_get_devp();
	u64 timer_st, timer_end, diff;	//debug only
	struct dpss_cfg_blki_s *blk_i;

	if (!in_para || !in_para->inf || !o) {
		DBG_ERR("%s:no input\n", __func__);
		return false;
	}

	blk_i = in_para->inf;
	timer_st = dpss_cur_to_msecs();	//dbg
#ifdef CONFIG_CMA
//#if 1 //to-do
	if (blk_i->mem_from == DPSS_MEM_FROM_CODEC) {	//
		ret = _mm_codec_alloc(DEVICE_NAME,
				     blk_i->page_size, in_para->cma_flg, o, blk_i->tvp);
	} else if (blk_i->mem_from == DPSS_MEM_FROM_CODEC_HD) {
		ret = _mm_codec_alloc_hd(DEVICE_NAME,
				     blk_i->page_size, in_para->cma_flg, o, blk_i->tvp);
	} else if (blk_i->mem_from == DPSS_MEM_FROM_CMA_DPSS) {	//
		ret = _mm_cma_alloc(&de_devp->pdev->dev, blk_i->page_size, o);
	} else if (blk_i->mem_from == DPSS_MEM_FROM_CMA_C) {	//
		ret = _mm_cma_alloc(NULL, blk_i->page_size, o);
	} else {
		DBG_ERR("%s:not finish function:%d:\n",
			__func__, blk_i->mem_from);
	}

/*#else
 *
 *	alloc from codec:
 *	ret = _mm_codec_alloc(DEVICE_NAME,
 *			blk_i->page_size,
 *			4,
 *			o,
 *			blk_i->tvp);
 *
 *	#endif
 */

#endif				/* CONFIG_CMA */

	timer_end = dpss_cur_to_msecs();	//dbg
#ifdef RUN_ON_PC
	o->addr = in_para->ower_id * 0x10000000;
	o->flg = 0;
	ret = true;
#endif

	diff = timer_end - timer_st;
	if (!ret) {
		DBG_ERR("%s:<%d,%d,0x%x>:%s,%s\n",
			__func__, blk_i->mem_from, blk_i->tvp, blk_i->page_size,
			in_para->owner, in_para->note);
	}
	dbg_m1("%s:%ums:<%d,%d>:<0x%lx,0x%x> addr_shif:0x%x:%s,%s\n",
	       __func__, (unsigned int)diff,
	       blk_i->mem_from, blk_i->tvp,
	       o->addr, blk_i->mem_size,
		(unsigned int)(o->addr >> in_para->shift_bits),
		in_para->owner, in_para->note);

	return ret;
}

//static
bool dpss_mm_release_api2(struct dpss_mem_r_s *in_para)
{
	bool ret = false;
	struct dpss_dev_s *de_devp = dpss_get_devp();
	struct dpss_blk_m_s *blk;
	struct codec_mm_s *mm;

#ifdef CONFIG_CMA
	if (!in_para || !in_para->blk) {
		DBG_ERR("%s:no input\n", __func__);
		return false;
	}
	dbg_m1("%s:ret:%d enter\n", __func__, ret);
	blk = in_para->blk;
	if (!blk->inf) {
		DBG_ERR("%s: blk no inf\n", __func__);
		return false;
	}
	if (blk->inf->mem_from == DPSS_MEM_FROM_CODEC) {
		codec_mm_free_for_dma(DEVICE_NAME, blk->mem_start);
		ret = true;
	} else if (blk->inf->mem_from == DPSS_MEM_FROM_CODEC_HD) {
		mm = (struct codec_mm_s *)blk->mem_handle;
		dbg_m1("release codec hd:%p\n", mm);
		codec_mm_release(mm, DEVICE_NAME);
		ret = true;
	} else if (blk->inf->mem_from == DPSS_MEM_FROM_CMA_DPSS) {
		ret = dma_release_from_contiguous(&de_devp->pdev->dev,
						  blk->pages,
						  blk->inf->page_size);
	} else if (blk->inf->mem_from == DPSS_MEM_FROM_CMA_C) {
		ret = dma_release_from_contiguous(NULL,
						  blk->pages,
						  blk->inf->page_size);
	}
	dbg_m1("%s:r:<%d,%d>,<0x%lx 0x%x>:%s\n",
	       __func__,
	       blk->inf->mem_from,
	       blk->inf->tvp,
	       blk->mem_start, blk->inf->mem_size, in_para->note);
#endif				/* CONFIG_CMA */
	return ret;
}

#define RDMA_ALLOC_MODE_DMA	(1)

static void dpss_buf_alloc(struct dpss_ch_s *pch)
{
//      struct dpss_blk_s *blk_sml;
//      struct dpss_blk_s *blk_nr;
//      struct dpss_buf_nr_s *buf_nr;
	struct dpss_buf_sml_s *sml_info;
	struct dpss_buf_rdma_s *rdma_info;
//      struct dpss_mem_a_s *in_para;
//      struct dpss_mm_s *o;
	struct dpss_blk_s *blk;
	struct dpss_blk_s *blk_local;
	struct dpss_cfg_blki_s *blk_i;
	struct dpss_cfg_blki_s *blk_l;
	struct dpss_mm_s oret;
	struct dpss_mem_a_s a_para;
	bool aret, flg_a = false;
	struct dpss_dd_s *dd;
	unsigned int i;
	unsigned long b_inpm0, b_inpm1;
	unsigned long b_addr =
	    0, b_mix, b_mv, b_alp, b_mtn, b_blk_info, b_dmsq;
	unsigned long b_dct_graid, b_tfbc, b_grad_h, b_grad_v;
	unsigned long b_dct_y, b_dct_c, b_afbc;
	unsigned long b_ro1, b_ro2;
	unsigned long b_mem, b_memv_n, b_memv_c, b_memv_p, b_iir_logo;
	unsigned long b_scc_logo, b_blk_logo, b_iplogo, b_mevp, b_melogo,
	    b_mero;
	unsigned long b_tmp;
	unsigned int size_tmp, size_buf;
	struct dpss_mm_p_cnt_out_s *nr_info;
	struct afbcd_buf_inf_s *afbc_info;
	struct afrc_buf_inf_s *afrc_info;
	bool a_afbc_hd = false;
	bool a_afbc_hd_b = false; //0916
	bool a_afbc_tab = false;
	bool a_sml = false;
	bool en_secure = false;
	bool is_4k = true;
	bool is_i = true;
//	unsigned int max_1080;
//	unsigned int max_4k;
	bool need_afrc = false;
	unsigned int pps_uv_size;

	unsigned int mem_support; // bit 0: afbc; bit 1: afrc; bit 2: mif;

#ifdef RDMA_ALLOC_MODE_DMA
	dma_addr_t dma_handle;
//--------------rdma--------------
	struct dpss_dev_s *de_devp = dpss_get_devp();
#endif
	struct frc_chip_st *pchip_st = NULL;

	pchip_st = dpss_get_frc_st();
	dd = dpss_get_dd();
	if (!pch || !dd) {
		DBG_WARN("%s:no pch or dd\n", __func__);
		return;
	}

	if (pch->d->di_front) {
		is_4k = false;
		is_i = true;
		need_afrc = false;
	} else if (pch->c.etype) {
		if (pch->c.parm.di_parm.width > 1 || pch->c.parm.di_parm.height > 1)
			is_4k = true;
		else
			is_4k = false;
		if (pch->c.parm.di_parm.is_interlace) {
			is_i = true;
		} else {
			need_afrc = true;
			is_i = false;
		}
	}
	if (is_i)
		pch->d->is_i = 1;
	else
		pch->d->is_i = 0;
	nr_dpe_pps_para(pch, pch->c.parm.di_parm.width,
		pch->c.parm.di_parm.height);

	if (dpss_nr_debug == 1 || dpss_en_afbc_force & C_BIT2)
		need_afrc = false;

	if (pch->d->is_secure) //
		en_secure = true;
	dbg_i0("%s:en_secure=%d, need_afrc=%d\n", __func__, en_secure, need_afrc);

	if (dpss_tst_4k || is_4k || need_afrc)
		sml_info = &dd->uhd_sml_info;
	else
		sml_info = &dd->hd_sml_info;
	if (dpss_tst_4k || is_4k || need_afrc) {
		afbc_info = &dd->fd_afbc_info;
		afrc_info = &dd->fd_afrc_info;
	} else {
		afbc_info = &dd->hd_afbc_info;
		afrc_info = &dd->hd_afrc_info;
	}
	if ((dpss_en_afbc & C_BIT0) || (pch->c.o_afbc & C_BIT0)) {
		sml_info->size_afbc = afbc_info->size_info;
		sml_info->size_ts_afbc =	//size_info -> table
				sml_info->size_afbc * pch->c.o_b_nub;
		sml_info->size_ts_afbc_tab =
				afbc_info->size_tab * pch->c.o_b_nub;
		sml_info->size_ts_afrc_tab_c = 0;
	} else if ((dpss_en_afbc & C_BIT1) || (pch->c.o_afbc & C_BIT1) || need_afrc) {
		sml_info->size_afrc_y = afrc_info->size_hearder_y;
		sml_info->size_afrc_c = afrc_info->size_hearder_c;
		sml_info->size_afbc = sml_info->size_afrc_y + sml_info->size_afrc_c;
		sml_info->size_ts_afbc =	//size_info -> table
				sml_info->size_afbc * pch->c.o_b_nub;
		sml_info->size_ts_afbc_tab =
				afrc_info->size_tab_y * pch->c.o_b_nub;
		sml_info->size_ts_afrc_tab_c =
				afrc_info->size_tab_c * pch->c.o_b_nub;
	} else {
		sml_info->size_afbc = 0;
		sml_info->size_ts_afbc = 0;
		sml_info->size_ts_afbc_tab = 0;
		sml_info->size_ts_afrc_tab_c = 0;
	}
	dbg_m0("size_afbc:0x%x\n", sml_info->size_afbc);
	dbg_m0("size_hearder_y:0x%x\n", afrc_info->size_hearder_y);
//afbc header ---------------------------------------------------
	flg_a = false;
	if ((dpss_en_afbc & 0xff) || (pch->c.o_afbc & 0xff) || need_afrc) {
		blk_i = &pch->c.blki_afbc_hd;
		memset(blk_i, 0, sizeof(*blk_i));
		blk_i->mem_size = sml_info->size_ts_afbc; // set 1
		blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
		blk_i->tvp = en_secure ? 1 : 0;
		blk_i->mem_from = DPSS_MEM_FROM_CODEC_HD;
		blk_i->cnt_cfg = NULL;

		blk = &pch->c.blk_r_afbc_hd;
		memset(&oret, 0, sizeof(oret));
		memset(&a_para, 0, sizeof(a_para));
		a_para.inf = blk_i;
		a_para.owner = "afbc_hd";
		a_para.note	= "afbc_hd buffer";
		a_para.ower_id = 10;
		aret = dpss_mm_alloc_api2(&a_para, &oret);
		if (aret) {
			blk->c.blk_typ = 0; //tmp
			blk->c.b.blk_m.inf = blk_i;
			blk->c.b.blk_m.flg_alloc = true;
			blk->c.b.blk_m.mem_start = oret.addr;
			blk->c.b.blk_m.pages	= oret.ppage;
			blk->c.b.blk_m.mem_handle = oret.mem_handle;
			blk->c.st_id = 0; //EBLK_ST_ALLOC;
			dbg_m1("alloc:afbc_hd:%s\n", a_para.owner);
			a_afbc_hd = true;
			pch->c.addr_afbc_hd_base = blk->c.b.blk_m.mem_start;
		} else {
			pch->d->mem_err |= C_BIT0;
			DBG_WARN("alloc failed afbc hd\n");
		}
		//~~~ set b:
		blk_i = &pch->c.blki_afbc_hd_b;
		memset(blk_i, 0, sizeof(*blk_i));
		blk_i->mem_size = sml_info->size_ts_afbc; //set b
		blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
		blk_i->tvp = en_secure ? 1 : 0;
		blk_i->mem_from = DPSS_MEM_FROM_CODEC_HD;
		blk_i->cnt_cfg = NULL;

		blk = &pch->c.blk_r_afbc_hd_b;
		memset(&oret, 0, sizeof(oret));
		memset(&a_para, 0, sizeof(a_para));
		a_para.inf = blk_i;
		a_para.owner = "afbc_hd_b";
		a_para.note	= "afbc_hd buffer";
		a_para.ower_id = 10;
		aret = dpss_mm_alloc_api2(&a_para, &oret);
		if (aret) {
			blk->c.blk_typ = 0; //tmp
			blk->c.b.blk_m.inf = blk_i;
			blk->c.b.blk_m.flg_alloc = true;
			blk->c.b.blk_m.mem_start = oret.addr;
			blk->c.b.blk_m.pages	= oret.ppage;
			blk->c.b.blk_m.mem_handle = oret.mem_handle;
			blk->c.st_id = 0; //EBLK_ST_ALLOC;

			dbg_m1("alloc:afbc_hd b:%s\n", a_para.owner);
			a_afbc_hd_b = true;
			pch->c.addr_afbc_hd_b_base = blk->c.b.blk_m.mem_start;
		} else {
			pch->d->mem_err |= C_BIT0;
			DBG_WARN("alloc failed afbc hd b\n");
		}
	}

	if (a_afbc_hd && a_afbc_hd_b) {
		if ((dpss_en_afbc & C_BIT0) || (pch->c.o_afbc & C_BIT0)) {
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_info[i] =
					pch->c.addr_afbc_hd_base +
					i * sml_info->size_afbc;
				pch->c.addr_afbc_info[i + pch->c.o_b_nub] =
					pch->c.addr_afbc_hd_b_base +
					i * sml_info->size_afbc;
				dbg_m2("afbce:infor :addr:%d:0x%lx: addr:%d:0x%lx:\n",
					i, pch->c.addr_afbc_info[i],
					i + pch->c.o_b_nub,
					pch->c.addr_afbc_info[i + pch->c.o_b_nub]);
			}
		} else if ((dpss_en_afbc & C_BIT1) || (pch->c.o_afbc & C_BIT1) ||
			need_afrc) { //afrc:
			b_tmp = pch->c.addr_afbc_hd_base;
			//check:to-do
			//info y:
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_info[i] =
					b_tmp + i * afrc_info->size_hearder_y;
				dbg_m2("afrc:infor:y:addr:%d:0x%lx:\n",
					i, pch->c.addr_afbc_info[i]);
			}
			//info c:
			b_tmp = pch->c.addr_afbc_hd_base +
				pch->c.o_b_nub *
				afrc_info->size_hearder_y;
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_info_c[i] =
					b_tmp + i * afrc_info->size_hearder_c;
				dbg_m2("afrc:infor:c:addr:%d:0x%lx:\n",
					i, pch->c.addr_afbc_info_c[i]);
			}
			//~~~~ b
			//info y:
			b_tmp = pch->c.addr_afbc_hd_b_base;
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_info[i + pch->c.o_b_nub] =
					b_tmp + i * afrc_info->size_hearder_y;
				dbg_m2("afrc:infor:y:addr:%d:0x%lx:\n",
					i + pch->c.o_b_nub,
					pch->c.addr_afbc_info[i + pch->c.o_b_nub]);
			}

			//info c:
			b_tmp = pch->c.addr_afbc_hd_b_base +
				pch->c.o_b_nub *
				afrc_info->size_hearder_y;
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_info_c[i + pch->c.o_b_nub] =
					b_tmp + i * afrc_info->size_hearder_c;
				dbg_m2("afrc:infor:c:addr:%d:0x%lx:\n",
					i + pch->c.o_b_nub,
					pch->c.addr_afbc_info_c[i + pch->c.o_b_nub]);
			}
			dbg_m2("afrc c end addr:%d:0x%lx\n", i,
				(b_tmp + i * afrc_info->size_hearder_c));
		}
	}
//afbc hd end----------------------------
//afbc tab ---------------------------------------------------
	flg_a = false;
	if ((dpss_en_afbc & 0xff) || (pch->c.o_afbc & 0xff) || need_afrc) {
		blk_i = &pch->c.blki_afbc_tab;
		memset(blk_i, 0, sizeof(*blk_i));
		blk_i->mem_size = sml_info->size_ts_afbc_tab +
				sml_info->size_ts_afrc_tab_c;
		blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
		blk_i->tvp = 0;
		blk_i->mem_from = DPSS_MEM_FROM_CODEC_HD;
		blk_i->cnt_cfg = NULL;

		blk = &pch->c.blk_r_afbc_tab;
		memset(&oret, 0, sizeof(oret));
		memset(&a_para, 0, sizeof(a_para));
		a_para.inf = blk_i;
		a_para.owner = "afbc_tab";
		a_para.note	= "afbc_tab buffer";
		a_para.ower_id = 10;
		aret = dpss_mm_alloc_api2(&a_para, &oret);
		if (aret) {
			blk->c.blk_typ = 0; //tmp
			blk->c.b.blk_m.inf = blk_i;
			blk->c.b.blk_m.flg_alloc = true;
			blk->c.b.blk_m.mem_start = oret.addr;
			blk->c.b.blk_m.pages	= oret.ppage;
			blk->c.b.blk_m.mem_handle = oret.mem_handle;
			blk->c.st_id = 0; //EBLK_ST_ALLOC;

			dbg_m1("alloc:afbc_tab:%s\n", a_para.owner);
			a_afbc_tab = true;
			pch->c.addr_afbc_tab_base = blk->c.b.blk_m.mem_start;
		} else {
			pch->d->mem_err |= C_BIT6;
			DBG_WARN("alloc failed afbc tab\n");
		}
	}
	if (a_afbc_tab) {
		b_afbc = pch->c.addr_afbc_tab_base;
		if ((dpss_en_afbc & C_BIT0) || (pch->c.o_afbc & C_BIT0)) {
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_tab[i] = b_afbc +
					i * afbc_info->size_tab;
				pch->c.addr_afbc_tab_c[i] = pch->c.addr_afbc_tab[i];
				dbg_m2("afbce:tab :addr:%d:0x%lx:\n",
					i, pch->c.addr_afbc_tab[i]);
			}
		} else if ((dpss_en_afbc & C_BIT1) || (pch->c.o_afbc & C_BIT1) ||
			need_afrc) { //afrc:
			b_tmp = b_afbc;
			//check:to-do
			//info y:
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_tab[i] = b_tmp +
					i * afrc_info->size_tab_y;
				dbg_m2("afrc:tab:y:addr:%d:0x%lx:\n",
					i, pch->c.addr_afbc_tab[i]);
			}
			//info c:
			b_tmp = b_afbc + sml_info->size_ts_afbc_tab;
			for (i = 0; i < pch->c.o_b_nub; i++) {
				pch->c.addr_afbc_tab_c[i] = b_tmp +
					i * afrc_info->size_tab_c;

				dbg_m2("afrc:tab:c:addr:%d:0x%lx:\n",
					i, pch->c.addr_afbc_tab_c[i]);
			}
			dbg_m2("afrc c end addr:%d:0x%lx\n", i,
				(b_tmp + i * afrc_info->size_tab_c));
		}
	}
//afbc tab end----------------------------

	//sml buf:
	blk_i = &pch->c.blki_sml;
	memset(blk_i, 0, sizeof(*blk_i));
	blk_i->mem_size = sml_info->size_t_nr_frc_s;	//sml_info->size_t_nr_s;
//	blk_i->mem_size += sml_info->size_ts_afbc;
#ifdef _HIS_CODE_
	blk_i->mem_size = blk_i->mem_size +
		sml_info->size_ts_afbc_tab +
		sml_info->size_ts_afrc_tab_c;
#endif
	blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
	blk_i->tvp = 0;
	blk_i->mem_from = DPSS_MEM_FROM_CODEC;
	blk_i->cnt_cfg = NULL;

	//small buffer:----------------------------
	blk = &pch->c.blk_r_sml_nr;
	memset(&oret, 0, sizeof(oret));
	memset(&a_para, 0, sizeof(a_para));
	a_para.inf = blk_i;
	a_para.owner = "samll";
	a_para.note = "tmp";
	a_para.ower_id = 1;
	a_para.shift_bits = 4;

	a_para.cma_flg = C_BIT1; //pre-alloc
	if (dpss_mem_flg & C_BIT1) {
		a_para.cma_flg &= (~C_BIT1);
		dbg_m0("sml:cma_flg= 0x%x\n", a_para.cma_flg);
	}
	aret = dpss_mm_alloc_api2(&a_para, &oret);
	if (aret) {
		blk->c.blk_typ = 0;	//tmp
		blk->c.b.blk_m.inf = blk_i;
		blk->c.b.blk_m.flg_alloc = true;
		blk->c.b.blk_m.mem_start = oret.addr;
		blk->c.b.blk_m.pages = oret.ppage;
		blk->c.st_id = 0;	//EBLK_ST_ALLOC;
		dbg_m1("alloc:sml: %s\n", a_para.owner);
		flg_a = true;
		pch->c.blk_sml_nr = blk;
		pch->c.sml_info = sml_info;
		pch->c.addr_sml_base = blk->c.b.blk_m.mem_start;
		a_sml = true;
	} else {
		pch->d->mem_err |= C_BIT1;
		DBG_WARN("alloc failed sml\n");
		//blk_idle_put(d_dd, blk);
	}
	dbg_m1("addr_sml_base = 0x%lx\n", pch->c.addr_sml_base);

	if (dpss_slt_mode) {
		pch->c.vaddr_ro5 = codec_mm_vmap(blk->c.b.blk_m.mem_start, blk_i->mem_size);
		DBG_INF("clear sml buf\n");
		memset(pch->c.vaddr_ro5, 0, (size_t)blk_i->mem_size);
	}

	if (a_sml) {
		//addr:
		b_addr = pch->c.addr_sml_base;
		if (pch->c.en_frc) {
			b_inpm0 = b_addr + 0;
			b_inpm1 = b_inpm0 + sml_info->size_ts_frc_inp_mbuf0;
			b_mem =  b_inpm1 + sml_info->size_ts_frc_inp_mbuf1;
			b_memv_n = b_mem + sml_info->size_ts_frc_me_mbuf;
			b_memv_c = b_memv_n + sml_info->size_ts_frc_me_nc_uni_mv;
			b_memv_p = b_memv_c + sml_info->size_ts_frc_me_cn_uni_mv;
			b_iir_logo = b_memv_p + sml_info->size_ts_frc_me_pc_phs_mv;
			b_scc_logo = b_iir_logo + sml_info->size_ts_frc_logo_iir;
			b_blk_logo = b_scc_logo + sml_info->size_ts_frc_logo_ssc;
			b_iplogo = b_blk_logo + sml_info->size_ts_frc_blk_logo;
			b_mevp = b_iplogo + sml_info->size_ts_frc_pix_logo;
			b_melogo = b_mevp + sml_info->size_ts_frc_vp_mc_mv;
			b_mero = b_melogo + sml_info->size_ts_frc_vp_mc_logo;
			b_mix = b_mero + sml_info->size_ts_frc_mero;
		} else {
			b_inpm0 = b_addr + 0;
			b_inpm1 = b_inpm0;
			b_mem =  b_inpm0;
			b_memv_n = b_inpm0;
			b_memv_c = b_inpm0;
			b_memv_p = b_inpm0;
			b_iir_logo = b_inpm0;
			b_scc_logo = b_inpm0;
			b_blk_logo = b_inpm0;
			b_iplogo = b_inpm0;
			b_mevp = b_inpm0;
			b_melogo = b_inpm0;
			b_mero = b_inpm0;

			b_mix = b_addr; //begin for nr
		}

		b_mv = b_mix + sml_info->size_ts_mix;
		b_alp = b_mv + sml_info->size_ts_mv;
		b_mtn = b_alp + sml_info->size_ts_alp;
		b_blk_info = b_mtn + sml_info->size_ts_mtn;
		b_dmsq = b_blk_info + sml_info->size_ts_blk_info;
		b_ro1 = b_dmsq + sml_info->size_ts_dmsq;
		b_ro2 = b_ro1 + sml_info->size_ts_ro1;
		b_dct_graid = b_ro2 + sml_info->size_ts_ro2;
		b_dct_y = b_dct_graid + sml_info->size_ts_dct_grid;
		b_dct_c = b_dct_y + sml_info->size_ts_dct_y;
#ifdef _HIS_CODE_
		b_afbc = b_dct_c + sml_info->size_ts_dct_c;
		b_tfbc = b_afbc + sml_info->size_ts_afbc_tab +
			sml_info->size_ts_afrc_tab_c;
#else
		b_tfbc = b_dct_c + sml_info->size_ts_dct_c;
#endif
		b_grad_h = b_tfbc + sml_info->size_ts_tfbc;
		b_grad_v = b_grad_h + sml_info->size_ts_grad_h;

		for (i = 0; i < DPSS_SML_NUB; i++) {
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_MIX] =
			    b_mix + i * sml_info->size_mix;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_MV] =
			    b_mv + i * sml_info->size_mv;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_MEALP] =
				b_alp + i * sml_info->size_alp;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_MTN] =
			    b_mtn + i * sml_info->size_mtn;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_BLK_INFO] =
			    b_blk_info + i * sml_info->size_blk_info;
			//pch->c.addr_sml[i][DPPS_SML_BUF_IDX_DMSQ] =
			//    b_dmsq + i * sml_info->size_dmsq;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_RO1] =
			    b_ro1 + i * sml_info->size_ro1;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_RO2] =
				b_ro2 + i * sml_info->size_ro2;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_DCT_GRAID] =
			    b_dct_graid + i * sml_info->size_dct_grid;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_DCT_Y] =
			    b_dct_y + i * sml_info->size_dct_y;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_DCT_C] =
			    b_dct_c + i * sml_info->size_dct_c;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_TFBC] =
			    b_tfbc + i * sml_info->size_tfbc;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_GRAD_H] =
			    b_grad_h + i * sml_info->size_grad_h;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_GRAD_V] =
			    b_grad_v + i * sml_info->size_grad_v;

			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_INP_MBUF0] =
			    b_inpm0 + i * sml_info->size_frc_inp_mbuf0;
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_INP_MBUF1] =
			    b_inpm1 + i * sml_info->size_frc_inp_mbuf1;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_ME_NC_UNI_MV] =
			    b_memv_n + i * sml_info->size_frc_me_nc_uni_mv;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_BLK_LOGO] =
			    b_blk_logo + i * sml_info->size_frc_blk_logo;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_PIX_LOGO] =
			    b_iplogo + i * sml_info->size_frc_pix_logo;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_VP_MC_MV] =
			    b_mevp + i * sml_info->size_frc_vp_mc_mv;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_VP_MC_LOGO] =
			    b_melogo + i * sml_info->size_frc_vp_mc_logo;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_FRC_MERO] =
			    b_mero + i * sml_info->size_frc_mer0;
		}
		for (i = 0; i < DPSS_DMS_NUB; i++) {
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_DMSQ] =
			    b_dmsq + i * sml_info->size_dmsq;
		}

//--------------------------------------------------------
		for (i = 0; i < DPSS_SML_NUB; i++) {
			pch->c.addr_sml[i][DPPS_SML_BUF_IDX_ME_MBUF] =
			    b_mem + i * sml_info->size_frc_me_mbuf;
		}
		for (i = 0; i < DPSS_SML_NUB; i++) {
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_ME_CN_UNI_MV] =
			    b_memv_c + i * sml_info->size_frc_me_cn_uni_mv;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_LOGO_IIR_BUF] =
			    b_iir_logo + i * sml_info->size_frc_logo_iir;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_LOGO_SSC_BUF] =
			    b_scc_logo + i * sml_info->size_frc_logo_ssc;
			pch->c.addr_sml[i][DPSS_SML_BUF_IDX_ME_PC_PHS_MV] =
			    b_memv_p + i * sml_info->size_frc_me_pc_phs_mv;
		}
		// pch->c.addr_sml[i][DPSS_SML_BUF_IDX_ME_PC_PHS_MV] =
		//              b_memv_p + i * sml_info->size_frc_me_pc_phs_mv;
		//debug only:
		//for (i = 0; i < DPPS_SML_BUF_IDX_NUB; i++) {
		//	dbg_m1("sml: md: 0x%lx, 0x%x\n",
		//	       pch->c.addr_sml[0][i],
		//	       (unsigned int)(pch->c.addr_sml[0][i] >> 4));
		//}

		/*map v addr for pq ko*/
		pch->c.vaddr_ro1 = codec_mm_vmap(b_ro1, sml_info->size_ts_ro1);
		pch->c.vaddr_ro2 = codec_mm_vmap(b_ro2, sml_info->size_ts_ro2);
		pch->c.vaddr_ro3 = codec_mm_vmap(b_grad_h,
			sml_info->size_ts_grad_h);
		pch->c.vaddr_ro4 = codec_mm_vmap(b_grad_v,
			sml_info->size_ts_grad_v);
		if (!pch->c.vaddr_ro1 || !pch->c.vaddr_ro2 ||
			!pch->c.vaddr_ro3 || !pch->c.vaddr_ro4)
			DBG_ERR("map ro vaddr fail\n");

		dbg_m1("vaddr_ro1=%px, vaddr_ro2=%px,\n", pch->c.vaddr_ro1,
			pch->c.vaddr_ro2);
		dbg_m1("vaddr_ro3=%px, vaddr_ro4=%px,%lx,%lx\n", pch->c.vaddr_ro3,
			pch->c.vaddr_ro4, b_grad_h, b_grad_v);
	}
	//dw buffer:----------------------------
	if (pch->c.en_dw) {
		blk_i = &pch->c.blki_dw;
		memset(blk_i, 0, sizeof(*blk_i));
		blk_i->mem_size = sml_info->size_dw; //sml_info->size_ts_dw;
		blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
		blk_i->tvp = en_secure ? 1 : 0;
		blk_i->mem_from = DPSS_MEM_FROM_CODEC_HD;
		blk_i->cnt_cfg = NULL;

		for (i = 0; i < pch->c.o_b_nub; i++) {
			blk = &pch->c.blk_r_dw[i];
			memset(&oret, 0, sizeof(oret));
			memset(&a_para, 0, sizeof(a_para));
			a_para.inf = blk_i; //need check
			a_para.owner = "dw";
			a_para.note = "dw buffer";
			a_para.shift_bits = 9;
			a_para.ower_id = 2;
			aret = dpss_mm_alloc_api2(&a_para, &oret);
			if (aret) {
				blk->c.blk_typ = 0;
				blk->c.b.blk_m.inf = blk_i;
				blk->c.b.blk_m.flg_alloc = true;
				blk->c.b.blk_m.mem_start = oret.addr;
				blk->c.b.blk_m.pages = oret.ppage;
				blk->c.b.blk_m.mem_handle = oret.mem_handle;
				blk->c.st_id = 0;	//EBLK_ST_ALLOC;
				dbg_m1("alloc:dw: %s\n", a_para.owner);
				flg_a = true;
				pch->c.blk_dw[i] = blk;
				pch->c.addr_dw[i] = blk->c.b.blk_m.mem_start;
				pch->c.addr_dwuv[i] =
				    pch->c.addr_dw[i] + sml_info->size_dw / 2;
				pch->c.alloc_cnt_blk_dw++;
			} else {
				pch->d->mem_err |= C_BIT2;
				DBG_WARN("alloc failed dw\n");
				pch->c.blk_dw[i] = NULL;
				pch->c.addr_dw[i] = 0;
				pch->c.addr_dwuv[i] = 0;
				blk->c.b.blk_m.flg_alloc = false;
				blk->c.b.blk_m.inf = NULL;
			}
		}
	}
	//nr buf:
	blk_i = &pch->c.blki_nr;
	memset(blk_i, 0, sizeof(*blk_i));
	if (dpss_tst_4k || is_4k || need_afrc)
		nr_info = &dd->fd_nr_info;
	else
		nr_info = &dd->hd_nr_info;

	blk_i->mem_size = nr_info->size_total;
	blk_i->page_size = nr_info->size_page;
	blk_i->tvp = en_secure ? 1 : 0;
	blk_i->mem_from = DPSS_MEM_FROM_CODEC_HD;
	blk_i->cnt_cfg = NULL;
	if (pchip_st && pchip_st->chip == ID_T6X && !dpss_nr_debug) {
		blk_l = &pch->c.blki_lc;
		memset(blk_l, 0, sizeof(*blk_l));
		blk_l->mem_size = dd->hd_lc_info.size_total;
		blk_l->page_size = dd->hd_lc_info.size_page;
		blk_l->tvp = en_secure ? 1 : 0;
		blk_l->mem_from = DPSS_MEM_FROM_CODEC;
		blk_l->cnt_cfg = NULL;
	}
	size_buf = 0;
	mem_support = (pch->c.mem_support & 0xf0) >> 4;
	if (mem_support) { //4k
		size_buf = 0;
		if (mem_support & C_BIT0) {//afbce
			size_tmp = dd->fd_afbc_info.body_buffer_size;
			size_buf = max(size_tmp, size_buf);
			dbg_m1("4k:afbce:0x%x\n", size_buf);
		}
		if (mem_support & C_BIT1) {//afrc
			size_tmp = dd->fd_afrc_info.size_body;
			size_buf = max(size_tmp, size_buf);
			dbg_m1("4k:afrc:0x%x\n", size_buf);
		}
		if (mem_support & C_BIT2) {//mif
			size_tmp = dd->fd_nr_info.size_total;
			size_buf = max(size_tmp, size_buf);
			dbg_m1("4k:mif:0x%x\n", size_buf);
		}
	}
	mem_support = pch->c.mem_support & 0xf;
	if (mem_support) {
		if (mem_support & C_BIT0) {//afbce
			size_tmp = dd->hd_afbc_info.body_buffer_size;
			size_buf = max(size_tmp, size_buf);
			dbg_m1("1080p:afbce:0x%x\n", size_buf);
		}
		if (mem_support & C_BIT1) {//afrc
			size_tmp = dd->hd_afrc_info.size_body;
			size_buf = max(size_tmp, size_buf);
			dbg_m1("1080p:arc:0x%x\n", size_buf);
		}
		if (mem_support & C_BIT2) {//mif
			size_tmp = dd->hd_nr_info.size_total;
			size_buf = max(size_tmp, size_buf);
			dbg_m1("1080p:mif:0x%x\n", size_buf);
		}
	}
	dbg_m0("ch[%d] nr:0x%x, mem_support=0x%x\n",
		pch->c.ch, size_buf, pch->c.mem_support);
	if (!size_buf && !pch->d->di_front) {
		DBG_ERR("%s:size is 0? 0x%x\n", __func__,
			(unsigned int)pch->c.mem_support);
		size_buf = dd->hd_nr_info.size_total;
	}
	blk_i->mem_size = size_buf;
	blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
#ifdef _HIS_CODE_
	if ((dpss_en_afbc & 0xff) || (pch->c.o_afbc & 0xff) || need_afrc) {
		size_buf = nr_info->size_total;
		if (afbc_info->body_buffer_size > nr_info->size_total) {
			dbg_m1("afbce:alloc:buf size: 0x%x <- 0x%x\n",
				afbc_info->body_buffer_size,
				nr_info->size_total);
			size_buf = afbc_info->body_buffer_size;
		}

		size_tmp = size_buf;
		blk_i->mem_size = size_tmp;
		blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;

		dbg_m0("afbce:buf size: 0x%x <- 0x%x, 0x%x <- 0x%x\n",
			blk_i->mem_size,
			nr_info->size_total,
			blk_i->page_size,
			nr_info->size_page);
	}

	max_1080 = dd->hd_lc_info.size_total;
	if (dpss_en_afbc_force == 0)
		max_4k = afrc_info->size_body;
	else
		max_4k = nr_info->size_total;

	size_tmp = max(max_1080, max_4k);
	blk_i->mem_size = size_tmp;
	blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
	dbg_m1("%s:m_1080:%u,m_4k:%u,b_buf:%u,s_body:%u,s_total:%u,s_tmp:%u\n",
		__func__,
		max_1080,
		max_4k,
		afbc_info->body_buffer_size,
		afrc_info->size_body,
		nr_info->size_total,
		size_tmp);
#endif
#ifdef _HIS_CODE_
	if (((dpss_en_afbc & C_BIT1) || (pch->c.o_afbc & C_BIT1)) &&
		dpss_afrc_head) {
		size_buf = nr_info->size_total;
		if (nr_info->size_total > afrc_info->size_body) {
			dbg_m1("afrce:alloc:buf size: 0x%x <- 0x%x\n",
				afbc_info->body_buffer_size,
				nr_info->size_total);
			size_buf = afrc_info->size_body;
		}
		size_tmp = size_buf;
		blk_i->mem_size = size_tmp;
		blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;

		dbg_m1("afrce:buf size: 0x%x <- 0x%x, 0x%x <- 0x%x\n",
			blk_i->mem_size,
			nr_info->size_total,
			blk_i->page_size,
			nr_info->size_page);
	}
#endif

	if ((dpss_en_afbc & C_BIT1) || (pch->c.o_afbc & C_BIT1))
		dbg_m0("Workmode is : AFRC, Compression ratio : %u%%, %u%%,is header: %u\n",
			(dpss_afrc_y * 100) / 80,
			(dpss_afrc_c * 100) / 80,
			dpss_afrc_head);
	else if (dpss_en_afbc & C_BIT0 || (pch->c.o_afbc & C_BIT0))
		dbg_m0("Workmode is : AFBC\n");
	else
		dbg_m0("Workmode is : YUV\n");
//-------------------------
	if (!pch->d->di_front) {
		for (i = 0; i < pch->c.o_b_nub; i++) {
			blk = &pch->c.blk_r_nr[i];
			if (pchip_st && pchip_st->chip == ID_T6X &&
				!dpss_nr_debug)
				blk_local = &pch->c.blk_r_lc[i];
			memset(&oret, 0, sizeof(oret));
			memset(&a_para, 0, sizeof(a_para));
			a_para.inf = blk_i;
			a_para.owner = "nr";
			a_para.note = "tmp";
			a_para.shift_bits = 9;
			a_para.ower_id = 3 + i;

			a_para.cma_flg = C_BIT1; //pre-alloc
			if (dpss_mem_flg & 0xf0) {
				a_para.cma_flg &= (~C_BIT1);
				dbg_m0("nr:cma_flg= 0x%x\n", a_para.cma_flg);
			}
			aret = dpss_mm_alloc_api2(&a_para, &oret);
			if (aret) {
				blk->c.blk_typ = 0;	//tmp
				blk->c.b.blk_m.inf = blk_i;
				blk->c.b.blk_m.flg_alloc = true;
				blk->c.b.blk_m.mem_start = oret.addr;
				blk->c.b.blk_m.pages = oret.ppage;
				blk->c.b.blk_m.mem_handle = oret.mem_handle;
				blk->c.st_id = 0;	//EBLK_ST_ALLOC;
				pch->c.alloc_cnt_blk_nr++;
				dbg_m1("alloc:nr %s:%d\n", a_para.owner, i);
				flg_a = true;
				pch->c.blk_nr[i] = blk;

				pch->c.addr_nr[i] = blk->c.b.blk_m.mem_start;
				dbg_m1("nr addr:0x%lx\n", pch->c.addr_nr[i]);
				pch->c.addr_nr_uv[i] = pch->c.addr_nr[i] +
					(blk_i->mem_size / 2);
				if ((dpss_en_afbc & C_BIT1) ||
				    (pch->c.o_afbc & C_BIT1) || need_afrc)
					pch->c.addr_nr_uv[i] = pch->c.addr_nr[i] +
						afrc_info->size_body_y;

				dbg_m1("uv addr=0x%lx\n", pch->c.addr_nr_uv[i]);
				if (pchip_st && pchip_st->chip == ID_T6X &&
					!dpss_nr_debug) {
					blk_local->c.blk_typ = 0;	//tmp
					blk_local->c.b.blk_m.inf = blk_i;
					blk_local->c.b.blk_m.flg_alloc = true;
					blk_local->c.b.blk_m.mem_start =
						pch->c.addr_nr[i] + dd->hd_nr_info.off_uv;
					blk_local->c.b.blk_m.pages = oret.ppage;
					blk_local->c.st_id = 0;	//EBLK_ST_ALLOC;
					pch->c.alloc_cnt_blk_lc++;
					dbg_m1("alloc:lc %s:%d\n", a_para.owner, i);
					flg_a = true;
					pch->c.blk_lc[i] = blk_local;
					pch->c.addr_lc[i] = blk_local->c.b.blk_m.mem_start;
					pch->c.addr_lc_uv[i] =
						pch->c.addr_nr_uv[i] + dd->hd_nr_info.off_uv;
					dbg_m1("y addr=0x%lx,uv addr=0x%lx\n", pch->c.addr_lc[i],
							pch->c.addr_lc_uv[i]);
				}
				//cnt tab:
				if (((dpss_en_afbc & C_BIT0) ||
				     (pch->c.o_afbc & C_BIT0)) &&
				     a_afbc_hd && a_afbc_tab) {
					pch->c.crc_y[i] =
						dpss_afbc_int_tab(pch->c.addr_afbc_tab[i],
						pch->c.addr_nr[i],
						afbc_info->size_tab,
						size_buf);
				} else if (((dpss_en_afbc & C_BIT1) ||
					(pch->c.o_afbc & C_BIT1) || need_afrc) &&
					a_afbc_hd  && a_afbc_tab) {
					//y:
					pch->c.crc_y[i] =
					dpss_afbc_int_tab(pch->c.addr_afbc_tab[i],
							pch->c.addr_nr[i],
							afrc_info->size_tab_y,
							afrc_info->size_body_y);
					//c:
					pch->c.crc_c[i] =
					dpss_afbc_int_tab(pch->c.addr_afbc_tab_c[i],
							pch->c.addr_nr_uv[i],
							afrc_info->size_tab_c,
							afrc_info->size_body_c);
				} else if (((dpss_en_afbc & 0xff)	||
					   (pch->c.o_afbc & 0xff)) &&
					   (!a_afbc_hd || !a_afbc_tab)) {
					DBG_ERR("no hd ?:0x%x %d %d\n",
						dpss_en_afbc, a_afbc_hd, a_afbc_tab);
				}
			} else {
				pch->d->mem_err |= C_BIT3;
				DBG_WARN("alloc failed nr:%i\n", i);
				//blk_idle_put(d_dd, blk);
				pch->c.addr_nr[i] = 0;
			}
		}
	}

	//lc buf:
	if (pch->c.support_i || dpss_nr_debug) { //to-do
		blk_i = &pch->c.blki_lc;
		memset(blk_i, 0, sizeof(*blk_i));
		blk_i->mem_size = dd->hd_lc_info.size_total;
		blk_i->page_size = dd->hd_lc_info.size_page;
		blk_i->tvp = en_secure ? 1 : 0;
		blk_i->mem_from = DPSS_MEM_FROM_CODEC;
		blk_i->cnt_cfg = NULL;

		for (i = 0; i < pch->c.num_lc; i++) {
			blk = &pch->c.blk_r_lc[i];
			memset(&oret, 0, sizeof(oret));
			memset(&a_para, 0, sizeof(a_para));
			a_para.inf = blk_i;
			a_para.owner = "lc";
			a_para.note = "tmp";
			a_para.shift_bits = 4;
			a_para.ower_id = 10 + i;
			aret = dpss_mm_alloc_api2(&a_para, &oret);
			if (aret) {
				blk->c.blk_typ = 0;	//tmp
				blk->c.b.blk_m.inf = blk_i;
				blk->c.b.blk_m.flg_alloc = true;
				blk->c.b.blk_m.mem_start = oret.addr;
				blk->c.b.blk_m.pages = oret.ppage;
				blk->c.st_id = 0;	//EBLK_ST_ALLOC;
				pch->c.alloc_cnt_blk_lc++;
				dbg_m1("alloc:lc %s:%d\n", a_para.owner, i);
				flg_a = true;
				pch->c.blk_lc[i] = blk;
				pch->c.addr_lc[i] = blk->c.b.blk_m.mem_start;
				pch->c.addr_lc_uv[i] =
					pch->c.addr_lc[i] + dd->hd_lc_info.off_uv;
				dbg_m1("uv addr=0x%lx\n", pch->c.addr_lc_uv[i]);
			} else {
				pch->d->mem_err |= C_BIT4;
				DBG_WARN("alloc failed di:%i\n", i);
				//blk_idle_put(d_dd, blk);
				pch->c.addr_lc[i] = 0;
			}
		}
	}

//di2pps buf
	if ((pch->c.support_i &&  pchip_st &&
		pchip_st->chip == ID_T6X) && dpss_pps_check != C_BIT1 &&
		!dpss_nr_debug && !dpss_force_nr_debug) {
		blk_i = &pch->c.blki_di2pps;
		memset(blk_i, 0, sizeof(*blk_i));
		blk_i->mem_size = dd->hd_lc_info.size_total * 2;
		blk_i->page_size = dd->hd_lc_info.size_page * 2;
		blk_i->tvp = en_secure ? 1 : 0;
		blk_i->mem_from = DPSS_MEM_FROM_CODEC_HD;
		blk_i->cnt_cfg = NULL;
		pps_uv_size = dd->hd_lc_info.off_uv * 2;

		for (i = 0; i < pch->c.o_b_nub; i++) {
			blk = &pch->c.blk_r_di2pps[i];
			memset(&oret, 0, sizeof(oret));
			memset(&a_para, 0, sizeof(a_para));
			a_para.inf = blk_i;
			a_para.owner = "di2pps";
			a_para.note = "tmp";
			a_para.shift_bits = 4;
			a_para.ower_id = 20 + i;
			aret = dpss_mm_alloc_api2(&a_para, &oret);
			if (aret) {
				blk->c.blk_typ = 0;	//tmp
				blk->c.b.blk_m.inf = blk_i;
				blk->c.b.blk_m.flg_alloc = true;
				blk->c.b.blk_m.mem_start = oret.addr;
				blk->c.b.blk_m.pages = oret.ppage;
				blk->c.b.blk_m.mem_handle = oret.mem_handle;
				blk->c.st_id = 0;	//EBLK_ST_ALLOC;
				pch->c.alloc_cnt_blk_di2pps++;
				dbg_m1("alloc: %s:%d, %p\n", a_para.owner,
					i, blk->c.b.blk_m.mem_handle);
				flg_a = true;
				pch->c.blk_di2pps[i] = blk;
				pch->c.addr_di2pps[i] = blk->c.b.blk_m.mem_start;
				pch->c.addr_di2pps_uv[i] =
					pch->c.addr_di2pps[i] + pps_uv_size;
				dbg_m1("y addr = 0x%lx,uv addr=0x%lx,size = 0x%x\n",
						pch->c.addr_di2pps[i],
						pch->c.addr_di2pps_uv[i],
						pps_uv_size);
			} else {
				pch->d->mem_err |= C_BIT5;
				DBG_WARN("alloc failed di2pps:%i\n", i);
				pch->c.addr_di2pps[i] = 0;
				pch->c.addr_di2pps_uv[i] = 0;
			}
		}
	}
	//===================================
	//rdma:
	rdma_info = &dd->rdma_info;

#ifdef RDMA_ALLOC_MODE_DMA
	pch->c.addr_rdma_v = dma_alloc_coherent(&de_devp->pdev->dev,
						rdma_info->size_total,
						&dma_handle, GFP_KERNEL);
	pch->c.addr_rdma_base = (ulong)(dma_handle);

	//set info:
	blk_i = &pch->c.blki_rdma;
	memset(blk_i, 0, sizeof(*blk_i));
	blk_i->mem_size = rdma_info->size_total;	//sml_info->size_t_nr_s;
	blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
	blk_i->tvp = 0;
	blk_i->mem_from = DPSS_MEM_FROM_DMA;
	blk_i->cnt_cfg = NULL;
	blk = &pch->c.blk_r_rdma;
	blk->c.b.blk_m.inf = blk_i;
	blk->c.b.blk_m.flg_alloc = true;
	pch->c.blk_rdma = blk;
	pch->c.rdma_info = rdma_info;
#else
	// buf:
	blk_i = &pch->c.blki_rdma;
	memset(blk_i, 0, sizeof(*blk_i));
	blk_i->mem_size = rdma_info->size_total;	//sml_info->size_t_nr_s;
	blk_i->page_size = blk_i->mem_size >> PAGE_SHIFT;
	blk_i->tvp = 0;
	blk_i->mem_from = DPSS_MEM_FROM_CODEC;
	blk_i->cnt_cfg = NULL;

	// buffer:----------------------------
	blk = &pch->c.blk_r_rdma;
	memset(&oret, 0, sizeof(oret));
	memset(&a_para, 0, sizeof(a_para));
	a_para.inf = blk_i;
	a_para.owner = "rdma";
	a_para.note = "tmp";
	a_para.shift_bits = 0;
	aret = dpss_mm_alloc_api2(&a_para, &oret);
	if (aret) {
		blk->c.blk_typ = 0;	//tmp
		blk->c.b.blk_m.inf = blk_i;
		blk->c.b.blk_m.flg_alloc = true;
		blk->c.b.blk_m.mem_start = oret.addr;
		blk->c.b.blk_m.pages = oret.ppage;
		blk->c.st_id = 0;	//EBLK_ST_ALLOC;
		dbg_m1("alloc:rdma: %s\n", a_para.owner);
		flg_a = true;
		pch->c.blk_rdma = blk;
		pch->c.rdma_info = rdma_info;
		pch->c.addr_rdma_base = blk->c.b.blk_m.mem_start;
		a_sml = true;
	} else {
		pch->d->mem_err |= C_BIT5;
		DBG_WARN("alloc failed rdma\n");
	}
#endif
	dbg_h1("addr_rdma_base = 0x%lx\n", pch->c.addr_rdma_base);
	if (!pch->d->mem_err) {
		pch->c.buf_status = DPSS_BUF_STATE_READY;
	} else {
		pch->c.buf_status = DPSS_BUF_STATE_FAIL;
		pch->d->bypass_rs |= C_BIT0;
		DBG_WARN("%s:bypass_rs:0x%x\n", __func__, pch->d->bypass_rs);
	}

	//===================================
}

static void dpss_buf_release(struct dpss_ch_s *pch)
{
	struct dpss_mem_r_s r_para;
	u8 i;
	struct dpss_blk_s *blk;
#ifdef FUNC_EN_PQ //ary add mark
	struct di_parm_s *de_pqp;
	unsigned int cnt;
#endif
#ifdef RDMA_ALLOC_MODE_DMA
//      dma_addr_t dma_handle;
	struct dpss_dd_s *dd;

//--------------rdma--------------
	struct dpss_dev_s *de_devp = dpss_get_devp();
	struct dpss_buf_rdma_s *rdma_info;
#endif

	if (!pch) {
		DBG_WARN("%s:no pch\n", __func__);
		return;
	}
	//release afbc_hd=========================
	blk = &pch->c.blk_r_afbc_hd;
	if (!blk) {
		dbg_m0("%s:release:afbc_hd:no\n", __func__);
	} else if (blk->c.b.blk_m.flg_alloc) {
		memset(&r_para, 0, sizeof(r_para));
		r_para.blk = &blk->c.b.blk_m;//note: tmp;
		r_para.note = "release";
		dpss_mm_release_api2(&r_para);
		memset(&blk->c, 0, sizeof(blk->c));
	} else {
		dbg_m0("no afbc_hd need release\n");
	}
	blk = &pch->c.blk_r_afbc_hd_b;
	if (!blk) {
		DBG_WARN("%s:release:afbc_hd b:no\n", __func__);
	} else if (blk->c.b.blk_m.flg_alloc) {
		memset(&r_para, 0, sizeof(r_para));
		r_para.blk = &blk->c.b.blk_m;//note: tmp;
		r_para.note = "release";
		dpss_mm_release_api2(&r_para);
		memset(&blk->c, 0, sizeof(blk->c));
	} else {
		dbg_m0("no afbc_hd need release b\n");
	}
	//release afbc_tab=========================
	blk = &pch->c.blk_r_afbc_tab;
	if (!blk) {
		DBG_WARN("%s:release:afbc_tab:no\n", __func__);
	} else if (blk->c.b.blk_m.flg_alloc) {
		memset(&r_para, 0, sizeof(r_para));
		r_para.blk = &blk->c.b.blk_m;//note: tmp;
		r_para.note = "release";
		dpss_mm_release_api2(&r_para);
		memset(&blk->c, 0, sizeof(blk->c));
	} else {
		dbg_m0("no afbc_tab need release\n");
	}
	//release sml
#ifdef FUNC_EN_PQ //ary add mark
	cnt = 0; /* 200us x 5 = 1ms dpe fw 2ms*/
	while (!pq_can_exit(pch) && cnt < 10) {
		usleep_range(100, 200);
		cnt++;
	}
	if (pq_can_exit(pch)) {
		de_pqp = dpss_pq_data(pch->c.ch);
		de_pqp->dae_ro_buffer = NULL;
		de_pqp->dpe_ro_buffer = NULL;
		de_pqp->dblk_h_ro_buffer = NULL;
		de_pqp->dblk_v_ro_buffer = NULL;
		de_pqp->dae_ro_pd_buffer = NULL;
		if (de_pqp->me_ro_buffer) {
			if (de_pqp->me_ro_buffer->buf)
				memset(de_pqp->me_ro_buffer->buf,
					0, DPSS_RO_ME_SIZE * sizeof(u32));
			else
				DBG_WARN("ro:%d\n", pch->c.ch);
			de_pqp->me_ro_buffer = NULL;
		}
		if (de_pqp->input_ro_buffer) {
			if (de_pqp->input_ro_buffer->buf)
				memset(de_pqp->input_ro_buffer->buf,
					0, DPSS_RO_INPUT_SIZE * sizeof(u32));
			else
				DBG_WARN("ro2:%d\n", pch->c.ch);
			de_pqp->input_ro_buffer = NULL;
		}
		DBG_INF("%s:unreg finish:%d\n", __func__, cnt);
	}
#endif
	if (dpss_slt_mode)
		codec_mm_unmap_phyaddr(pch->c.vaddr_ro5);
	codec_mm_unmap_phyaddr(pch->c.vaddr_ro1);
	pch->c.vaddr_ro1 = NULL;
	codec_mm_unmap_phyaddr(pch->c.vaddr_ro2);
	pch->c.vaddr_ro2 = NULL;
	codec_mm_unmap_phyaddr(pch->c.vaddr_ro3);
	pch->c.vaddr_ro3 = NULL;
	codec_mm_unmap_phyaddr(pch->c.vaddr_ro4);
	pch->c.vaddr_ro4 = NULL;

	//blk = blk_o_get_locked(d_dd, j);
	blk = pch->c.blk_sml_nr;
	if (!blk) {
		DBG_WARN("%s:release:sml:no\n", __func__);
	} else {
		memset(&r_para, 0, sizeof(r_para));
		r_para.blk = &blk->c.b.blk_m;	//note: tmp;
		r_para.note = "release";
		dpss_mm_release_api2(&r_para);
		memset(&blk->c, 0, sizeof(blk->c));
		//blk_idle_put(d_dd, blk);
		pch->c.blk_sml_nr = NULL;
		//addr:
		memset(&pch->c.addr_sml[0][0], 0,
		       sizeof(unsigned long) * DPSS_SML_NUB *
		       DPPS_SML_BUF_IDX_NUB);
	}

	//release rdma =======================

	//blk = blk_o_get_locked(d_dd, j);
	blk = pch->c.blk_rdma;
	if (!blk) {
		DBG_WARN("%s:release:rdma:no\n", __func__);
	} else {
#ifdef RDMA_ALLOC_MODE_DMA
		dd = dpss_get_dd();
		rdma_info = &dd->rdma_info;
		dma_free_coherent(&de_devp->pdev->dev,
				  rdma_info->size_total,
				  pch->c.addr_rdma_v,
				  (dma_addr_t)pch->c.addr_rdma_base);
#else
		memset(&r_para, 0, sizeof(r_para));
		r_para.blk = &blk->c.b.blk_m;	//note: tmp;
		r_para.note = "release";
		dpss_mm_release_api2(&r_para);
#endif
		memset(&blk->c, 0, sizeof(blk->c));
		//blk_idle_put(d_dd, blk);
		pch->c.blk_rdma = NULL;
		//addr:
		memset(&pch->c.addr_rdma[0], 0,
		       sizeof(unsigned long) * DPSS_RDMA_RAM_NUB);
	}

	//release dw=========================
	for (i = 0; i < pch->c.o_b_nub; i++) {
		blk = pch->c.blk_dw[i];
		if (!blk) {
			if (pch->c.en_dw)
				DBG_WARN("%s:release:dw:no [%d]\n", __func__, i);
			continue;
		} else {
			memset(&r_para, 0, sizeof(r_para));
			r_para.blk = &blk->c.b.blk_m;	//note: tmp;
			r_para.note = "release";
			if (blk->c.b.blk_m.flg_alloc) {
				dpss_mm_release_api2(&r_para);
				pch->c.alloc_cnt_blk_dw--;
			} else {
				DBG_WARN("release dw %d not alloc\n", i);
			}
			memset(&blk->c, 0, sizeof(blk->c));

			pch->c.blk_dw[i] = NULL;
			//addr:
			pch->c.addr_dw[i] = 0;
			pch->c.addr_dwuv[i] = 0;
		}
	}
	//release nr buffer:
	for (i = 0; i < pch->c.o_b_nub; i++) {
		//blk = blk_o_get_locked(d_dd, j);
		blk = pch->c.blk_nr[i];
		if (!blk) {
			DBG_WARN("%s:release nr:no [%d]\n", __func__, i);
			continue;
		}
		memset(&r_para, 0, sizeof(r_para));
		r_para.blk = &blk->c.b.blk_m;	//note: tmp;
		r_para.note = "release";
		dpss_mm_release_api2(&r_para);
		memset(&blk->c, 0, sizeof(blk->c));
		//blk_idle_put(d_dd, blk);
		pch->c.alloc_cnt_blk_nr--;
		pch->c.blk_nr[i] = NULL;
	}

	//release lc buffer:
	for (i = 0; i < pch->c.num_lc; i++) {
		//blk = blk_o_get_locked(d_dd, j);
		blk = pch->c.blk_lc[i];
		if (!blk) {
			dbg_m0("%s:release nr:no [%d]\n", __func__, i);
			continue;
		}
		memset(&r_para, 0, sizeof(r_para));
		r_para.blk = &blk->c.b.blk_m;	//note: tmp;
		r_para.note = "release";
		dpss_mm_release_api2(&r_para);
		memset(&blk->c, 0, sizeof(blk->c));
		//blk_idle_put(d_dd, blk);
		pch->c.alloc_cnt_blk_lc--;
		pch->c.blk_lc[i] = NULL;
	}

//release di2pps buffer
	//if (pch->c.prm_top.is_di2pps) {
		for (i = 0; i < pch->c.o_b_nub; i++) {
			//blk = blk_o_get_locked(d_dd, j);
			blk = pch->c.blk_di2pps[i];
			if (!blk) {
				dbg_m1("%s:di2pps release nr:no [%d]\n", __func__, i);
				continue;
			}
			memset(&r_para, 0, sizeof(r_para));
			r_para.blk = &blk->c.b.blk_m;	//note: tmp;
			r_para.note = "release";
			dpss_mm_release_api2(&r_para);
			memset(&blk->c, 0, sizeof(blk->c));
			//blk_idle_put(d_dd, blk);
			pch->c.alloc_cnt_blk_di2pps--;
			pch->c.blk_di2pps[i] = NULL;
		}
	//}
	pch->c.buf_status = DPSS_BUF_STATE_UNREADY;
	dbg_m0("%s:finish:%d:%d:%d\n", __func__,
			pch->c.alloc_cnt_blk_nr,
			pch->c.alloc_cnt_blk_lc,
			pch->c.alloc_cnt_blk_di2pps);
}

void dpss_buf_reg(struct dpss_ch_s *pch)
{
	dpss_buf_infor(pch);
	if (!pch->d->di_front)
		dpss_buf_infor_4k(pch);//test

	dpss_buf_alloc(pch);
}

void dpss_buf_unreg(struct dpss_ch_s *pch)
{
	dpss_buf_release(pch);
	// 2026-02-05 film_grain_uint();
	nr_unreg_val(pch);
	memset(&pch->c.blki_sml, 0, sizeof(pch->c.blki_sml));
	memset(&pch->c.blki_dw, 0, sizeof(pch->c.blki_dw));
	memset(&pch->c.blki_nr, 0, sizeof(pch->c.blki_nr));

	memset(&pch->c.blk_r_sml_nr, 0, sizeof(pch->c.blk_r_sml_nr));
	memset(&pch->c.blk_r_dw, 0, sizeof(pch->c.blk_r_dw));
	memset(&pch->c.blk_r_nr[0], 0,
	       sizeof(pch->c.blk_r_nr[0]) * DPSS_HW_LOOP_IN_OUT_BUF_NUB);
	if (pch->c.ch == 0) {
		memset(&g_nr_pps_cfg, 0, sizeof(g_nr_pps_cfg));
		pch->c.prm_top.is_pps = 0;
		pch->c.prm_top.is_di2pps = 0;
		pch->c.prm_top.pps_dsx = 0;
		pch->c.prm_top.pps_dsy = 0;
		pch->c.prm_top.ch = 0;
	}
	dbg_i0("%s:is_pps ch=%d,=%d,=%d\n", __func__,
		pch->c.ch, pch->c.prm_top.is_pps, pch->c.prm_top.is_di2pps);
	//memset(&pch->c.buf_nr[0], 0, sizeof(pch->c.buf_nr[0]) * DPSS_DPS_NUB);
	dbg_m0("%s:ch[%d]:finish\n", __func__, pch->c.ch);
}

/********************************************************/
u8 *dpss_vmap(ulong addr, u32 size, bool *bflg)
{
	u8 *vaddr = NULL;
#ifdef RUN_ON_ARM
	ulong phys = addr;
	u32 offset = phys & ~PAGE_MASK;
	u32 npages = PAGE_ALIGN(size) / PAGE_SIZE;
	struct page **pages = NULL;
	pgprot_t pgprot;
	int i;

	if (!PageHighMem(phys_to_page(phys))) {
		*bflg = false;
		return phys_to_virt(phys);
	}
	if (offset)
		npages++;
	pages = vmalloc(sizeof(struct page *) * npages);
	if (!pages)
		return NULL;
	for (i = 0; i < npages; i++) {
		pages[i] = phys_to_page(phys);
		phys += PAGE_SIZE;
	}
	pgprot = pgprot_writecombine(PAGE_KERNEL);
	vaddr = vmap(pages, npages, VM_MAP, pgprot);
	if (!vaddr) {
		DBG_ERR("the phy(%lx) vmaped fail, size: %d\n",
			addr - offset, npages << PAGE_SHIFT);
		vfree(pages);
		return NULL;
	}
	vfree(pages);
	*bflg = true;
	return vaddr + offset;
#else
	vaddr = addr;
	bflg = false;
	return vaddr;
#endif
}

void dpss_unmap_phyaddr(u8 *vaddr)
{
#ifdef RUN_ON_ARM
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	vunmap(addr);

#endif
}

//dbg:
#ifndef FUNC_EN_CFGX
bool dpss_cfgx_get(unsigned int ch, enum EDPSS_CFGX_IDX idx)
{
	return false;
}

unsigned char dpss_cfgx_getc(unsigned int ch, enum EDPSS_CFGX_IDX idx)
{
	return 0;
}
#endif				/* FUNC_EN_CFGX */

static const unsigned int bit_tab[32] = {
	C_BIT0,
	C_BIT1,
	C_BIT2,
	C_BIT3,
	C_BIT4,
	C_BIT5,
	C_BIT6,
	C_BIT7,
	C_BIT8,
	C_BIT9,
	C_BIT10,
	C_BIT11,
	C_BIT12,
	C_BIT13,
	C_BIT14,
	C_BIT15,
	C_BIT16,
	C_BIT17,
	C_BIT18,
	C_BIT19,
	C_BIT20,
	C_BIT21,
	C_BIT22,
	C_BIT23,
	C_BIT24,
	C_BIT25,
	C_BIT26,
	C_BIT27,
	C_BIT28,
	C_BIT29,
	C_BIT30,
	C_BIT31,
};

void d_bset(unsigned int *p, unsigned int bitn)
{
	*p = *p | bit_tab[bitn];
}

void d_bclr(unsigned int *p, unsigned int bitn)
{
	*p = *p & (~bit_tab[bitn]);
}

bool d_bget(unsigned int *p, unsigned int bitn)
{
	return (*p & bit_tab[bitn]) ? true : false;
}

void dpss_frc_secure_proc(bool type, unsigned int addr, unsigned int size)
{
#ifdef RUN_ON_ARM
	int ret;

	dbg_m0("%s:type:%d addr:%#x size:%#x\n", __func__, type, addr, size);

	if (type == 0) {
		tee_unprotect_mem(frc_secure_handle);
		//wr();
	} else if (type == 1) {
		ret = tee_protect_mem(0x10, 0, addr, size, &frc_secure_handle);
		if (!ret)
			dbg_m0("set sec mem success\n");
		else
			dbg_m0("set sec mem\n");
		//wr();
	}
#endif				/* RUN_ON_ARM */
}

//#if 1

/*********************************************
 * size v2 2020-08-24
 ********************************************/

static unsigned int afbc_count_info_size(unsigned int w, unsigned int h,
					 unsigned int *blk_total,
					 struct afbcd_buf_inf_s *inf)
{
	unsigned int length = 0;

	unsigned int blk_num_total, blk_hsize, blk_vsize;
	unsigned int blk_hsize_align, blk_vsize_align;

	blk_hsize = ((w + 31) >> 5);	//hsize 32 ?? ??32
	blk_vsize = ((h + 3) >> 2);	//size  4  ?? ??4

	blk_hsize_align = ((blk_hsize + 1) >> 1) << 1;	//2??
	blk_vsize_align = ((blk_vsize + 15) >> 4) << 4;	//8??
	blk_num_total = blk_hsize_align * blk_vsize_align;

	length = blk_num_total * 4;
	length = PAGE_ALIGN(length);	//ary add 2020-08-26

	inf->blk_total = blk_num_total;

	*blk_total = blk_num_total;
	inf->size_info = length;

	inf->size_info = inf->size_info * 4;  //tmp:for afrc, inf size x 4:
	dbg_i0("%s:w=%d, h=%d:size=0x%x\n", "afbce:cnt info tmp x 4", w, h,
		inf->size_info);
	return length;
}

static unsigned int v2_afbc_count_buffer_size(unsigned int format,
					      unsigned int blk_nub_total,
					      struct afbcd_buf_inf_s *inf)
{
//      struct afbcd_ctr_s *pafd_ctr = di_get_afd_ctr();
	unsigned int sblk_num, src_bits, each_blk_bytes, body_buffer_size;
	unsigned int fmt_mode, compbits;

	fmt_mode = format & 0x0f;
	compbits = (format >> 4) & 0x0f;
	if (!fmt_mode)		/*4:2:0 */
		sblk_num = 12;
	else if (fmt_mode == 1)	/*4:2:2 */
		sblk_num = 16;
	else			/*4:4:4 */
		sblk_num = 24;

	if (compbits == 0)
		src_bits = 8;
	else if (compbits == 1)
		src_bits = 9;
	else
		src_bits = 10;

	each_blk_bytes = ((((sblk_num * 16 * src_bits + 7) >> 3) +
			   63) >> 6) << 6;
	body_buffer_size = ((blk_nub_total * each_blk_bytes) * 113 + 99) / 100;

	inf->body_buffer_size = PAGE_ALIGN(body_buffer_size);
	dbg_i0("%s:size=0x%x, 0x%x\n", "afbce:cnt buf", body_buffer_size,
		inf->body_buffer_size);
	return inf->body_buffer_size;
}

static unsigned int afbc_count_tab_size(unsigned int buf_size,
					struct afbcd_buf_inf_s *inf)
{
	unsigned int length = 0;

	length = PAGE_ALIGN(((buf_size + 0xfff) >> 12) * sizeof(unsigned int));

	inf->size_tab = length;
	dbg_i0("%s:size=0x%x\n", "afbce:cnt tab", inf->size_tab);
	return length;
}

static unsigned int afbc_int_tab(struct device *dev,
				 struct afb_tab_info_s *pcfg)
{
	unsigned int crc = 0;
#if RUN_ON_ARM
	bool flg = 0;
	unsigned int *p;
	unsigned int i, cnt, last;
	unsigned int body;

	unsigned long crc_tmp = 0;
	//struct div2_mm_s *mm = dim_mm_get(ch);
	//struct di_dev_s *de_devp = get_dim_de_devp();

	dma_sync_single_for_device(dev,
				   pcfg->tabadd, pcfg->size_tab, DMA_TO_DEVICE);

	p = (unsigned int *)dpss_vmap(pcfg->tabadd, pcfg->size_tab, &flg);
	if (!p) {
		DBG_ERR("%s:vmap:0x%lx\n", __func__, pcfg->tabadd);
		return 0;
	}

	dbg_m2("%s:p:%p\n", __func__, p);
	cnt = (pcfg->size_buf + 0xfff) >> 12;
	body = (unsigned int)(pcfg->bodyadd >> 12);
	for (i = 0; i < cnt; i++) {
		*(p + i) = body;
		if (i < 20)
			dbg_m2("w:%p:0x%x\n", (p + i), body);
		crc_tmp += body;
		body++;
	}

	crc = (unsigned int)crc_tmp;
	last = pcfg->size_tab - (cnt * sizeof(unsigned int));

	memset((p + cnt), 0, last);
	//memset((p + cnt), body, last);

	/*debug */
	dbg_h2("%s:tab:[0x%lx]: body[0x%lx];cnt[%d];last[%d]\n",
	       __func__, pcfg->tabadd, pcfg->bodyadd, cnt, last);

	dma_sync_single_for_device(dev,
				   pcfg->tabadd, pcfg->size_tab, DMA_TO_DEVICE);
	//PR_INF("%s:%d\n", __func__, flg);
	if (flg)
		dpss_unmap_phyaddr((u8 *)p);

	dbg_i0("%s:crc:0x%x\n", __func__, crc);
#endif
	return crc;
}

static unsigned int afbc_tab_cnt_crc(struct device *dev,
				     struct afb_tab_info_s *pcfg,
				     unsigned int check_mode,
				     unsigned int crc_old)
{
	bool flg = 0;
	unsigned int *p;
	int i, cnt;
	unsigned int body;
	unsigned int crc = 0;
	unsigned long crc_tmp = 0;
	unsigned int err_cnt = 0;
	unsigned int fist_err_pos = 0, fist_err = 0, first_right = 0;
	unsigned int fist_s_pos = 0, right_cnt = 0;

	//p = (unsigned int *)dim_vmap(pcfg->tabadd, pcfg->size_buf, &flg);
	dma_sync_single_for_cpu(dev,
				pcfg->tabadd, pcfg->size_tab, DMA_FROM_DEVICE);
	p = (unsigned int *)dpss_vmap(pcfg->tabadd, pcfg->size_tab, &flg);
	if (!p) {
		DBG_ERR("%s:vmap:0x%lx\n", __func__, pcfg->tabadd);
		return 0;
	}
	if (!pcfg->size_tab)
		return 0;
	cnt = (pcfg->size_buf + 0xfff) >> 12;
	body = (unsigned int)(pcfg->bodyadd >> 12);
	for (i = 0; i < cnt; i++) {
		if (check_mode == 1) {
			if (*(p + i) != body) {
				if (err_cnt == 0) {
					fist_err_pos = i;
					fist_err = *(p + i);
					first_right = body;
				}

				err_cnt++;
			} else {
				right_cnt++;
				if (right_cnt > 3)
					fist_s_pos = i;
			}
		}
		crc_tmp += *(p + i);

		body++;
	}
	crc = (unsigned int)crc_tmp;
	/*debug */
	if (crc_old != crc || err_cnt > 0) {
		DBG_ERR("%s:crc[0x%x], err[%d]\n", __func__, crc, err_cnt);
		DBG_ERR("pos[%d],er1[0x%x],[0x%x],rightpos[%d]\n",
			fist_err_pos, fist_err, first_right, fist_s_pos);
	}
	if (flg)
		dpss_unmap_phyaddr((u8 *)p);

	return crc;
}

#ifdef MOV			//tmp
bool dbg_checkcrc(struct di_buf_s *di_buf, unsigned int cnt)
{
	struct di_dev_s *devp = get_dim_de_devp();
	struct afb_tab_info_s pcfg;
	unsigned int crc;

	pcfg.bodyadd = di_buf->nr_adr;
	pcfg.tabadd = di_buf->afbct_adr;

	pcfg.size_buf = di_buf->nr_size;
	pcfg.size_tab = di_buf->tab_size;

	if (!pcfg.size_tab && !di_buf->afbc_crc)
		return true;
	else if (!pcfg.size_tab)
		return false;

	crc = afbc_tab_cnt_crc(&devp->pdev->dev, &pcfg, 1, di_buf->afbc_crc);

	if (crc != di_buf->afbc_crc) {
		PR_ERR("%s:cnt[%d],0x%x->0x%x\n",
		       __func__, cnt, crc, di_buf->afbc_crc);
		PR_INF("\t:bodyadd[0x%lx],tabadd[0x%lx]\n",
		       pcfg.bodyadd, pcfg.tabadd);
		PR_INF("\t:sizebuf[0x%x], sizetab[0x%x]\n",
		       pcfg.size_buf, pcfg.size_tab);

		return false;
	}
	return true;
}
#endif

struct afbc_op_s {
	unsigned int (*cnt_info_size)(unsigned int w, unsigned int h,
				      unsigned int *blk_total,
				      struct afbcd_buf_inf_s *inf);
	unsigned int (*cnt_tab_size)(unsigned int buf_size,
				     struct afbcd_buf_inf_s *inf);
	unsigned int (*cnt_buf_size)(unsigned int format,
				     unsigned int blk_nub_total,
				     struct afbcd_buf_inf_s *inf);
	unsigned int (*int_tab)(struct device *dev,
				struct afb_tab_info_s *pcfg);
	unsigned int (*tab_cnt_crc)(struct device *dev,
				    struct afb_tab_info_s *pcfg,
				    unsigned int check_mode,
				    unsigned int crc_old);
};

const struct afbc_op_s afbc_op = {
	.cnt_info_size = afbc_count_info_size,
	.cnt_tab_size = afbc_count_tab_size,
	.cnt_buf_size = v2_afbc_count_buffer_size,
	.int_tab = afbc_int_tab,
	.tab_cnt_crc = afbc_tab_cnt_crc,
};

void dpss_afbc_info_cnt(unsigned int width, unsigned int height,
			struct afbcd_buf_inf_s *inf)
{
	unsigned int afbc_info_size, afbc_buffer_size, afbc_tab_size;
	unsigned int blk_total;

	afbc_info_size = afbc_op.cnt_info_size(width, height, &blk_total, inf);
	afbc_buffer_size = afbc_op.cnt_buf_size(0x20, blk_total, inf);
	afbc_tab_size = afbc_op.cnt_tab_size(afbc_buffer_size, inf);
	//dbg_i0("%s afbc_buffer_size:0x%x\n", __func__, afbc_buffer_size);
}

unsigned int dpss_afbc_int_tab(unsigned long tab_addr, unsigned long buf_addr,
		       unsigned int size_tab, unsigned int size_buf)
{
	unsigned int crc;
	struct afb_tab_info_s cfg, *pcfg;

	struct dpss_dev_s *devp;

	devp = dpss_get_devp();

	pcfg = &cfg;
	pcfg->bodyadd = buf_addr;
	pcfg->tabadd = tab_addr;
	pcfg->size_buf = size_buf;
	pcfg->size_tab = size_tab;

	crc = afbc_op.int_tab(&devp->pdev->dev, pcfg);
	return crc;
}

void afrc_info_cnt(unsigned int w,
		   unsigned int h,
		   unsigned int tgt_b0,// 20 ~ 48, ag: 4
		   unsigned int tgt_b1,
		   unsigned int type_in, //0:yuv444, 1:yuv422, 2: yuv 420
		   struct afrc_buf_inf_s *inf)
{
	unsigned int size_hd_y, size_hd_c, size_tab_y, size_tab_c;
	unsigned int size_buf_y, size_buf_c;
	unsigned int tb0, tb1, type;
	unsigned int blk0, blk1;

	tb0 = tgt_b0;
	tb1 = tgt_b1;
	if (tgt_b0 < 20 || tgt_b0 > 48 || (tgt_b0 % 4)) {
		DBG_ERR("tgt_b0 is err:%d\n", tgt_b0);
		tb0 = 20;
	}
	if (tgt_b1 < 20 || tgt_b1 > 48 || (tgt_b1 % 4)) {
		DBG_ERR("tgt_b0 is err:%d\n", tgt_b1);
		tb1 = 20;
	}
	type = type_in;
	if (type_in > 2)
		type = 2;

	blk0 = 4096 / (4 * tb0);
	blk1 = 4096 / (4 * tb1);

	inf->w = w;
	inf->h = h;
	inf->type = type;
	inf->target_b0 = tb0;
	inf->target_b1 = tb1;

	size_hd_y = (w * (h + 8) * 2) / (16 * 4);
	size_buf_y = (w * h) / (16 * 4 * 4 * blk0) + 1;
	size_buf_c = (w * h) / (16 * 4 * 4 * blk1) + 1;

	size_buf_y = size_buf_y * 4096;
	size_buf_c = size_buf_c * 4096;
	if (type == 0) {
		size_hd_c = size_hd_y * 2;
		size_buf_c = size_buf_c * 2;
	} else if (type == 1) {
		size_hd_c = size_hd_y;
		//size_buf_c = size_buf_c;
	} else { //yuv 420
		size_hd_c = size_hd_y >> 1;
		size_buf_c = size_buf_c >> 1;
	}

	size_tab_y = ((size_buf_y + 1023) >> 12) << 2;
	size_tab_c = ((size_buf_c + 1023) >> 12) << 2;
	dbg_m0("afrc size y:hd,bd,tb<0x%x,0x%x,0x%x>\n",
		size_hd_y, size_buf_y, size_tab_y);

	inf->size_hearder_y = PAGE_ALIGN(size_hd_y);
	inf->size_hearder_c = PAGE_ALIGN(size_hd_c);
	inf->size_hd = size_hd_y + size_hd_c;
	inf->size_body_y = PAGE_ALIGN(size_buf_y);
	inf->size_body_c =  PAGE_ALIGN(size_buf_c);
	inf->size_body = size_buf_y + size_buf_c;

	inf->size_tab_y =  PAGE_ALIGN(size_tab_y);
	inf->size_tab_c =  PAGE_ALIGN(size_tab_c);
	inf->size_tab = size_tab_y + size_tab_c;

	dbg_m0("afrc size:%d:hd:<0x%x,0x%x,0x%x>\n", tb0,
		inf->size_hearder_y, inf->size_hearder_c, inf->size_hd);
	dbg_m0("\tsize:bd:<0x%x,0x%x,0x%x>\n",
		inf->size_body_y, inf->size_body_c, inf->size_body);
	dbg_m0("\tsize:tb:<0x%x,0x%x,0x%x>\n",
		inf->size_tab_y, inf->size_tab_c, inf->size_tab);
}

//#endif //
