// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include "d_define.h"
#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>

//--------------------------------------
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
//--------------------------------------
#endif

#include "../dpss/dpss_base.h"
#include "../dpss/dpss_s.h"
#include "../dpss/dpss_sys.h"
#include "../dpss/dpss_func.h"

unsigned int dpss_enable_dd_rdma;/*auto mode,not enable rdma by default*/
module_param_named(dpss_enable_dd_rdma, dpss_enable_dd_rdma, uint, 0664);

unsigned int dpss_sub_support_ddd(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	bool support = false;

	if (!(pch->c.work_mode_cfg & D_W_B(DDD)))
		return 0;

#ifdef FUNC_EN_DD
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_dpss_path() && is_amdv_enable() && is_amdv_frame(vfm) &&
		get_amdv_force_mode() != AMDV_OUTPUT_MODE_BYPASS)
		support = true;
#endif
#endif

	if (support)
		return D_W_B(DDD);
	return 0;
}

unsigned int dpss_sub_chg_ddd(struct dpss_ch_s *pch, struct vframe_s *vfm)
{
	unsigned int chg = 0;

#ifdef FUNC_EN_DD
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	bool support = false;
	static bool last_support;

	if (pch->c.ch)
		return 0;
	if (vfm) {
		dbg_h2("%s, dpss path %d, dv frame %d\n",
		__func__, is_amdv_dpss_path(), is_amdv_frame(vfm));
		if (is_amdv_dpss_path() && is_amdv_enable() && is_amdv_frame(vfm) &&
			get_amdv_force_mode() != AMDV_OUTPUT_MODE_BYPASS)
			support = true;

		if (support != last_support) {
			chg = true;
			dbg_h2("dpss dd support change %d=>%d\n", last_support, support);
		}
		last_support = support;
	}
	if (get_pic_mode() != get_target_pic_mode()) {
		chg = true;
		dbg_h2("dpss dd pic mode change %d=>%d\n", get_pic_mode(), get_target_pic_mode());
	}
#endif
#endif
	if (chg)
		return D_W_B(DDD);
	return 0;
}

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
void dpss_dd_init(struct vframe_s *vfm, struct dpss_info_s *dpss_info) //
{
#ifdef FUNC_EN_DD
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION

	struct dpss_info_s dpss_inf;

	dbg_h2("dpss dd init\n");

	if (is_amdv_dpss_path() && is_amdv_enable() && is_amdv_frame(vfm) &&
		get_amdv_force_mode() != AMDV_OUTPUT_MODE_BYPASS) {
		if (dpss_info) {
			dpss_inf.pad_mode = dpss_info->pad_mode;
			dpss_inf.slice_num = dpss_info->slice_num;
			dpss_inf.frm_hsize_sel = dpss_info->frm_hsize_sel;
			dpss_inf.tbc_mode = dpss_info->tbc_mode;
			dpss_inf.vds_4k1k_en = dpss_info->vds_4k1k_en;
			dpss_inf.direct_mode = dpss_info->direct_mode;
			dpss_inf.dct_ahead_dv_mode = dpss_info->dct_ahead_dv_mode;
			dpss_inf.real_h = dpss_info->real_h;
			dpss_inf.real_v = dpss_info->real_v;
		} else {
			dpss_inf.pad_mode = 2;
			dpss_inf.slice_num = 2;
			dpss_inf.frm_hsize_sel = 0;
			dpss_inf.tbc_mode = 0;
			dpss_inf.vds_4k1k_en = 0;
			dpss_inf.direct_mode = false;
			dpss_inf.dct_ahead_dv_mode = 0;
			dpss_inf.real_h = 3840;
			dpss_inf.real_v = 2160;
		}
		dbg_h0("r_h=%d,r_v=%d\n", dpss_inf.real_h, dpss_inf.real_v);
		update_target_pic_mode();
		amdv_wait_metadata(vfm, 0);/*update_top1_onoff*/
		init_prm_dolby(&dpss_inf);
		if (!get_top1_onoff())
			force_top1_done();
	}
#endif
#endif /* FUNC_EN_DD */
}

void dpss_dd_update_mode(enum dolby_work_mode mode)
{
	update_dd_mode(mode);
}

void dpss_dd_update_info(struct dpss_info_s *dpss_info)
{
	update_dd_dpss_info(dpss_info);
}

void dpss_dd_update_info_by_cnt(unsigned int cnt_n, struct PRM_DPSS_TOP *prm_top,
		struct PRM_DPSS_DPE *prm_dpe)
{
	struct dpss_info_s dpss_info;
	bool direct_mode = false;
	enum DPSS_WORK_MODE	dpe_nr_mode = prm_top->dpe_nr_mode;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;

	dpss_info.slice_num = prm_top->dpe_slc_num;
	dpss_info.frm_hsize_sel = prm_top->frm_hsize_sel;
	dpss_info.tbc_mode = prm_top->sw_tbc_mode;
	unsigned int cnt;
	bool dct_ahead_dv_mode = prm_top->dct_ahead_dv_mode;

	cnt = prm_top->s_cnt;
//	prm_top->s_cnt++;

	dbg_h2("%s:%d\n", __func__, cnt);
	if (prm_top->mode_drct || prm_top->mode_drct2)
		direct_mode = true;
	if (direct_mode)
		dct_ahead_dv_mode = 0;
	if (!cnt) {
		dpss_info.pad_mode = 0;
		if (prm_dpe->dcntr_en && !prm_top->dct_ahead_dv_mode)
			dpss_info.pad_mode += 8;
		if (prm_dpe->aa_pad) {
			if (pps->pps_en)
				dpss_info.pad_mode += prm_dpe->aa_pad;
			else
				dpss_info.pad_mode += 8;
		}
		if (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE)
			dpss_info.pad_mode = 0;

		dpss_info.direct_mode = direct_mode;
		dpss_info.dct_ahead_dv_mode = dct_ahead_dv_mode;

		dbg_h2("%s:cnt=%d,nr=%d,dct=%d,pps=%d,aa=%d,dolby_mode=%d,pad=%d,direct=%d %d\n",
			__func__,
			cnt,
			prm_top->dpe_nr_mode,
			prm_dpe->dcntr_en,
			pps->pps_en,
			prm_dpe->aa_pad,
			prm_top->dolby_mode,
			dpss_info.pad_mode,
			direct_mode,
			dpss_info.dct_ahead_dv_mode);
		update_dd_dpss_info(&dpss_info);

		prm_top->mode_drct_lst = direct_mode;
	} else if ((cnt == 1) ||
		   (direct_mode != prm_top->mode_drct_lst)) {
		if (direct_mode) {
			dpe_nr_mode = DPE_NR_BYPS;
			dct_ahead_dv_mode = 0;
		}
		if (dpe_nr_mode == DPE_NR_BYPS) {
			dpss_info.pad_mode = 0;
			if (prm_dpe->dcntr_en)
				dpss_info.pad_mode = 8;
			if (prm_dpe->aa_pad) {
				if (pps->pps_en)
					dpss_info.pad_mode += prm_dpe->aa_pad;
				else
					dpss_info.pad_mode += 8;
			}
		} else {
			dpss_info.pad_mode = 64;
			if (prm_dpe->dcntr_en && !prm_top->dct_ahead_dv_mode)
				dpss_info.pad_mode += 8;
		}
		if (prm_top->dolby_mode == DOLBY_DPSS_PRL_MODE)
			dpss_info.pad_mode = 0;

		dpss_info.direct_mode = direct_mode;
		dpss_info.dct_ahead_dv_mode = dct_ahead_dv_mode;
		dbg_h2("%s:cnt=%d,nr%d(%d),dct=%d,pps=%d,aa=%d,dolby_mode=%d,pad=%d,direct=%d %d\n",
			__func__,
			cnt,
			prm_top->dpe_nr_mode,
			dpe_nr_mode,
			prm_dpe->dcntr_en,
			pps->pps_en,
			prm_dpe->aa_pad,
			prm_top->dolby_mode,
			dpss_info.pad_mode,
			direct_mode,
			dpss_info.dct_ahead_dv_mode);

		dbg_h2("\tcnt2=%d\n", cnt_n);
		update_dd_dpss_info(&dpss_info);
		prm_top->mode_drct_lst = direct_mode;
	}
}
#endif

void dpss_dd_cfg_inp(struct vframe_s *vfm)//dae
{
	//input  setting
#ifdef FUNC_EN_DD
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	s32 ret;
	u32 ori_w;
	u32 ori_h;

	dbg_h2("dpss dd cfg inp\n");
	if (is_amdv_dpss_path() && is_amdv_enable() && vfm && is_amdv_frame(vfm)) {
		ret = amdv_wait_metadata(vfm, 0);/*check wait on and update_top1_onoff*/
		//if (ret == 4)
			//todo
		//vfm->src_fmt.pr_done = false;//init
		//vfm->src_fmt.py_level = PY_NO_LEVEL;
		//vfm->src_fmt.downsamplers = 2;
		//vfm->src_fmt.py_id = 0;
		ori_w = (vfm->type & VIDTYPE_COMPRESS) ?
			vfm->compWidth : vfm->width;
		ori_h = (vfm->type & VIDTYPE_COMPRESS) ?
			vfm->compHeight : vfm->height;
		dbg_h2("%s:type %x, ori size %d %d\n", __func__, vfm->type, ori_w, ori_h);
		if (get_top1_onoff()) {/* top1 + top2*/
			//top1 parser + top1 cp
			amdv_parse_metadata_hw5_top1(vfm);
			//top1 reg set
			amdolby_vision_process_hw5(vfm, NULL,
				ori_w << 16 | ori_h, 1);

			dpss_v_rdma_config_a();
		} else {
			force_top1_done();
		}
	}
#endif
#endif /* FUNC_EN_DD */
}

void dpss_dd_cfg_dpe(struct vframe_s *vfm)
{
/*wait mode*/
#ifdef FUNC_EN_DD
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	s32 ret;
	u32 ori_w;
	u32 ori_h;

	//if (dpss_dv_en & C_BIT16) {
		if (is_amdv_dpss_path() && is_amdv_enable() && vfm &&
			is_amdv_frame(vfm)/* && get_top1_onoff()*/) {
			ori_w = (vfm->type & VIDTYPE_COMPRESS) ?
				vfm->compWidth : vfm->width;
			ori_h = (vfm->type & VIDTYPE_COMPRESS) ?
				vfm->compHeight : vfm->height;
			dbg_h2("%s:type %x, ori size %d %d\n", __func__, vfm->type, ori_w, ori_h);
			/*top2 parser+cp*/
			ret = amdv_update_metadata(vfm, 0, false);
			if (ret == 0)
				amdv_set_toggle_flag(1);
			/*top2 reg set*/
			amdolby_vision_process_hw5(NULL, vfm,
					ori_w << 16 | ori_h, 1);
			dpss_v_rdma_config_b();
			vfm->dpss_flg |= DPSS_FLG_DDD;
		}
	//}
#endif
#endif /* FUNC_EN_DD */
}

void dpss_dd_pause(void)
{			//for multi dpss, 2nd ch need disable dd
#ifdef FUNC_EN_DD

#endif /* FUNC_EN_DD */
}

void dpss_dd_disable(void)
{
#ifdef FUNC_EN_DD
	dbg_h2("dpss dd disable\n");
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	if (is_amdv_dpss_path() && is_amdv_enable())
		amdolby_vision_process_hw5(NULL, NULL, 0, 0);
#endif
#endif /* FUNC_EN_DD */
}

void dpss_dd_dae_update(struct vframe_s *vfm)
{
#ifdef FUNC_EN_DD
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	s32 ret;
	u32 ori_w;
	u32 ori_h;

	dbg_h2("dpss dd dae update(%px)\n", vfm);
	if (is_amdv_dpss_path() && is_amdv_enable() && vfm && is_amdv_frame(vfm)) {
		ret = amdv_wait_metadata(vfm, 0);/*check wait on and update_top1_onoff*/
		//if (ret == 4)
			//todo
		//vfm->src_fmt.pr_done = false;//init
		//vfm->src_fmt.py_level = PY_NO_LEVEL;
		//vfm->src_fmt.downsamplers = 2;
		//vfm->src_fmt.py_id = 0;
		ori_w = (vfm->type & VIDTYPE_COMPRESS) ?
			vfm->compWidth : vfm->width;
		ori_h = (vfm->type & VIDTYPE_COMPRESS) ?
			vfm->compHeight : vfm->height;
		dbg_h2("%s:type %x, ori size %d %d\n", __func__, vfm->type, ori_w, ori_h);
		if (get_top1_onoff()) {/* top1 + top2*/
			//top1 parser + top1 cp
			amdv_parse_metadata_hw5_top1(vfm);
			//top1 reg set
			amdolby_vision_process_hw5(vfm, NULL,
				ori_w << 16 | ori_h, 1);
			if (dpss_enable_dd_rdma)
				dpss_v_rdma_config_a();
		} else {
			force_top1_done();
		}
	}
#endif

#endif /* FUNC_EN_DD */
}

void dpss_dd_dpe_update(struct vframe_s *vfm)
{
#ifdef FUNC_EN_DD
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	s32 ret;
	u32 ori_w;
	u32 ori_h;

	dbg_h2("dpss dd dpe update(%px)\n", vfm);
	if (is_amdv_dpss_path() && is_amdv_enable() && vfm &&
		is_amdv_frame(vfm)) {
		ori_w = (vfm->type & VIDTYPE_COMPRESS) ?
			vfm->compWidth : vfm->width;
		ori_h = (vfm->type & VIDTYPE_COMPRESS) ?
			vfm->compHeight : vfm->height;
		dbg_h2("%s:type %x, ori size %d %d\n", __func__, vfm->type, ori_w, ori_h);
		/*top2 parser+cp*/
		ret = amdv_update_metadata(vfm, 0, false);
		if (ret == 0)
			amdv_set_toggle_flag(1);
		/*top2 reg set*/
		amdolby_vision_process_hw5(NULL, vfm,
				ori_w << 16 | ori_h, 1);
		if (dpss_enable_dd_rdma)
			dpss_v_rdma_config_b();

		//if have dv set flg:
		vfm->dpss_flg |= DPSS_FLG_DDD;
	}
#endif
#endif /* FUNC_EN_DD */
}
