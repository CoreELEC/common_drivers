// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"
#include "meson_vpu_util.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_postblend.h"

static u32 osd_vpp_misc_mask = 0x33330;
static u32 osd_vpp1_bld_ctrl;
static u32 osd_vpp1_bld_ctrl_mask = 0x30;
static u32 osd_vpp2_bld_ctrl;
static u32 osd_vpp2_bld_ctrl_mask = 0x30;
/* indicates whether vpp1&vpp2 has been notified or not */
static u32 osd_vpp_bld_ctrl_update_mask = 0x80000000;

static struct postblend_reg_s postblend_reg = {
	VPP_OSD1_BLD_H_SCOPE,
	VPP_OSD1_BLD_V_SCOPE,
	VPP_OSD2_BLD_H_SCOPE,
	VPP_OSD2_BLD_V_SCOPE,
	VD1_BLEND_SRC_CTRL,
	VD2_BLEND_SRC_CTRL,
	OSD1_BLEND_SRC_CTRL,
	OSD2_BLEND_SRC_CTRL,
	VPP_OSD1_IN_SIZE,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct postblend1_reg_s postblend1_reg[3] = {
	{},
	{
		VPP1_BLD_DIN0_HSCOPE,
		VPP1_BLD_DIN0_VSCOPE,
		VPP1_BLD_DIN1_HSCOPE,
		VPP1_BLD_DIN1_VSCOPE,
		VPP1_BLD_DIN2_HSCOPE,
		VPP1_BLD_DIN2_VSCOPE,
		VPP1_BLD_CTRL,
		VPP1_BLD_OUT_SIZE,
		VPP1_BLEND_BLEND_DUMMY_DATA,
		VPP1_BLEND_DUMMY_ALPHA,
	},
	{
		VPP2_BLD_DIN0_HSCOPE,
		VPP2_BLD_DIN0_VSCOPE,
		VPP2_BLD_DIN1_HSCOPE,
		VPP2_BLD_DIN1_VSCOPE,
		VPP2_BLD_DIN2_HSCOPE,
		VPP2_BLD_DIN2_VSCOPE,
		VPP2_BLD_CTRL,
		VPP2_BLD_OUT_SIZE,
		VPP2_BLEND_BLEND_DUMMY_DATA,
		VPP2_BLEND_DUMMY_ALPHA,
	},
};

static struct postblend_reg_s s5_postblend_reg = {
	VPP_OSD1_BLD_H_SCOPE_S5,
	VPP_OSD1_BLD_V_SCOPE_S5,
	VPP_OSD2_BLD_H_SCOPE_S5,
	VPP_OSD2_BLD_V_SCOPE_S5,
	VD1_BLEND_SRC_CTRL_S5,
	VD2_BLEND_SRC_CTRL_S5,
	OSD1_BLEND_SRC_CTRL_S5,
	OSD2_BLEND_SRC_CTRL_S5,
	VPP_OSD1_IN_SIZE,
};
#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void fix_vpu_clk2_default_regs(struct meson_vpu_block *vblk,
				struct rdma_reg_ops *reg_ops, int crtc_index, u32 *crtcmask_osd);
#endif

static void postblend_osd2_def_conf(struct meson_vpu_block *vblk);

/*vpp post&post blend for osd1 premult flag config as 0 default*/
static void osd1_blend_premult_set(struct meson_vpu_block *vblk,
				   struct rdma_reg_ops *reg_ops,
				   struct postblend_reg_s *reg)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, 0, 4, 1);
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, 0, 16, 1);
}

/*vpp osd1 blend sel*/
static void osd1_blend_switch_set(struct meson_vpu_block *vblk,
				  struct rdma_reg_ops *reg_ops,
				  struct postblend_reg_s *reg,
				  enum vpp_blend_e blend_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd2 blend sel*/
static void osd2_blend_switch_set(struct meson_vpu_block *vblk,
				  struct rdma_reg_ops *reg_ops,
				  struct postblend_reg_s *reg,
				  enum vpp_blend_e blend_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd2_blend_src_ctrl, blend_sel, 20, 1);
}

/*vpp osd1 preblend mux sel*/
static void vpp_osd1_preblend_mux_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct postblend_reg_s *reg,
				      enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd2 preblend mux sel*/
static void vpp_osd2_preblend_mux_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct postblend_reg_s *reg,
				      enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd2_blend_src_ctrl, src_sel, 0, 4);
}

/*vpp osd1 postblend mux sel*/
static void vpp_osd1_postblend_mux_set(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops,
				       struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 8, 4);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void vpp_osd1_postblend_5mux_set(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops,
					   struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd1_blend_src_ctrl, src_sel, 0, 4);
}
#endif

/*vpp osd2 postblend mux sel*/
static void vpp_osd2_postblend_mux_set(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops,
				       struct postblend_reg_s *reg,
				       enum vpp_blend_src_e src_sel)
{
	reg_ops->rdma_write_reg_bits(reg->osd2_blend_src_ctrl, src_sel, 8, 4);
}

/*vpp osd1 blend scope set*/
static void vpp_osd1_blend_scope_set(struct meson_vpu_block *vblk,
				     struct rdma_reg_ops *reg_ops,
				     struct postblend_reg_s *reg,
				     struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg(reg->vpp_osd1_bld_h_scope,
				(scope.h_start << 16) | scope.h_end);
	reg_ops->rdma_write_reg(reg->vpp_osd1_bld_v_scope,
				(scope.v_start << 16) | scope.v_end);
}

static int drm_postblend_notify_amvideo(void)
{
	u32 para[7];

	para[0] = meson_drm_read_reg(VPP_MISC);
	para[1] = osd_vpp_misc_mask;
	/* osd_vpp1_bld_ctrl */
	para[2] = osd_vpp1_bld_ctrl;
	para[3] = osd_vpp1_bld_ctrl_mask;
	/* osd_vpp2_bld_ctrl */
	para[4] = osd_vpp2_bld_ctrl;
	para[5] = osd_vpp2_bld_ctrl_mask;
	para[6] = osd_vpp_bld_ctrl_update_mask;

#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
	amvideo_notifier_call_chain(AMVIDEO_UPDATE_OSD_MODE,
					    (void *)&para[0]);
#endif
	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void vpp1_osd1_blend_scope_set(struct meson_vpu_block *vblk,
				      struct rdma_reg_ops *reg_ops,
				      struct postblend1_reg_s *reg,
				      struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg(reg->vpp_bld_din1_hscope,
				(scope.h_start << 16) | scope.h_end);
	reg_ops->rdma_write_reg(reg->vpp_bld_din1_vscope,
				(scope.v_start << 16) | scope.v_end);
}

static void osd_set_vpp_path_default(struct meson_vpu_block *vblk,
				     struct rdma_reg_ops *reg_ops,
				     u32 osd_index, u32 vpp_index)
{
	/* osd_index is vpp mux input */
	/* default setting osd2 route to vpp0 vsync */
	if (osd_index == 3)
		reg_ops->rdma_write_reg_bits(PATH_START_SEL, vpp_index, 24, 2);
	/* default setting osd3 route to vpp0 vsync */
	if (osd_index == 4)
		reg_ops->rdma_write_reg_bits(PATH_START_SEL, vpp_index, 28, 2);
}
#endif

static void vpp_chk_crc(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct am_meson_crtc *amcrtc)
{
	if (amcrtc->force_crc_chk ||
	    (amcrtc->vpp_crc_enable && cpu_after_eq(MESON_CPU_MAJOR_ID_SM1))) {
		reg_ops->rdma_write_reg(VPP_CRC_CHK, 1);
		amcrtc->force_crc_chk--;
	}
}

static int postblend_check_state(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state,
				 struct meson_vpu_pipeline_state *mvps)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	u32 video_zorder, osd_zorder, top_flag, bottom_flag, i, j;

	if (state->checked)
		return 0;

	state->checked = true;
	for (i = 0; i < MESON_MAX_VIDEO &&
	     mvps->video_plane_info[i].enable; i++) {
		video_zorder = mvps->video_plane_info[i].zorder;
		top_flag = 0;
		bottom_flag = 0;
		for (j = 0; j < MESON_MAX_OSDS &&
		     mvps->plane_info[j].enable; j++) {
			osd_zorder = mvps->plane_info[j].zorder;
			if (video_zorder > osd_zorder)
				top_flag++;
			else
				bottom_flag++;
		}
		if (top_flag && bottom_flag) {
			DRM_DEBUG("unsupported zorder\n");
			return -1;
		} else if (top_flag) {
			set_video_zorder(video_zorder +
					 VPP_POST_BLEND_REF_ZORDER, i);
			DRM_DEBUG("video on the top\n");
		} else if (bottom_flag) {
			set_video_zorder(video_zorder, i);
			DRM_DEBUG("video on the bottom\n");
		}
	}

	DRM_DEBUG("%s check_state called.\n", postblend->base.name);
	return 0;
}

static void postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	int crtc_index;
	struct am_meson_crtc *amc;
	struct meson_vpu_pipeline_state *mvps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];

	DRM_DEBUG("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	scope.h_start = 0;
	scope.h_end = mvps->scaler_param[0].output_width - 1;
	scope.v_start = 0;
	scope.v_end = mvps->scaler_param[0].output_height - 1;

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_config(OSD_MODULE, mvps->sec_src, crtc_index);
#endif

	if (crtc_index == 0) {
		vpp_osd1_blend_scope_set(vblk, reg_ops, reg, scope);

		if (amc->blank_enable) {
			vpp_osd1_postblend_mux_set(vblk, reg_ops,
						   postblend->reg, VPP_NULL);
		} else {
			/*dout switch config*/
			osd1_blend_switch_set(vblk, reg_ops, postblend->reg,
					      VPP_POSTBLEND);
			/*vpp input config*/
			vpp_osd1_preblend_mux_set(vblk, reg_ops,
						  postblend->reg, VPP_NULL);

			vpp_osd1_postblend_mux_set(vblk, reg_ops,
							   postblend->reg,
							   VPP_OSD1);
		}

		vpp_chk_crc(vblk, reg_ops, amc);
		osd1_blend_premult_set(vblk, reg_ops, reg);
	}

	DRM_DEBUG("scope h/v start/end [%d,%d,%d,%d].\n",
		  scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void t7_postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	int i, crtc_index;
	struct am_meson_crtc *amc;
	struct meson_vpu_pipeline_state *mvps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;
	u32 *crtcmask_osd;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];
	crtcmask_osd = amc->priv->of_conf.crtcmask_osd;

	DRM_DEBUG("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	scope.h_start = 0;
	scope.h_end = mvps->scaler_param[0].output_width - 1;
	scope.v_start = 0;
	scope.v_end = mvps->scaler_param[0].output_height - 1;

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_config(OSD_MODULE, mvps->sec_src, crtc_index);
#endif

	if (!vblk->init_done) {
		if (crtc_index == 0)
			postblend_osd2_def_conf(vblk);

		if (!postblend->postblend_path_mask)
			fix_vpu_clk2_default_regs(vblk, reg_ops, crtc_index,
						  crtcmask_osd);

		vblk->init_done = 1;
	}

	if (crtc_index == 0) {
		vpp_osd1_blend_scope_set(vblk, reg_ops, reg, scope);

		if (amc->blank_enable) {
			vpp_osd1_postblend_mux_set(vblk, reg_ops,
						   postblend->reg, VPP_NULL);
		} else {
			/*dout switch config*/
			osd1_blend_switch_set(vblk, reg_ops, postblend->reg,
					      VPP_POSTBLEND);
			/*vpp input config*/
			vpp_osd1_preblend_mux_set(vblk, reg_ops,
						  postblend->reg, VPP_NULL);

			vpp_osd1_postblend_mux_set(vblk, reg_ops,
						   postblend->reg,
						   VPP_OSD2);
		}

		for (i = 0; i < MESON_MAX_OSDS; i++) {
			if (mvps->plane_info[i].enable &&
			    mvps->plane_info[i].crtc_index == crtc_index) {
				osd_set_vpp_path_default(vblk, reg_ops, i + 1,
							 crtc_index);
			}
		}

		vpp_chk_crc(vblk, reg_ops, amc);
		osd1_blend_premult_set(vblk, reg_ops, reg);
	} else {
		/* 1:vd1-din0, 2:osd1-din1 */

		u32 val, vppx_bld;
		u32 bld_src2_sel = 2;
		u32 scaler_index = 2;
		struct postblend1_reg_s *reg1 = postblend->reg1;

		for (i = 0; i < MESON_MAX_OSDS; i++) {
			if (mvps->plane_info[i].enable &&
			    mvps->plane_info[i].crtc_index == crtc_index) {
				bld_src2_sel = i;
				scaler_index = i;
				break;
			}
		}

		scope.h_start = mvps->plane_info[scaler_index].dst_x;
		scope.h_end = scope.h_start + mvps->scaler_param[scaler_index].output_width - 1;
		scope.v_start = mvps->plane_info[scaler_index].dst_y;
		scope.v_end = scope.v_start + mvps->scaler_param[scaler_index].output_height - 1;

		vpp1_osd1_blend_scope_set(vblk, reg_ops, reg1, scope);

		vppx_bld = reg_ops->rdma_read_reg(reg1->vpp_bld_ctrl);
		if (amc->blank_enable)
			val = (vppx_bld & ~0xf0) | 1 << 31;
		else
			val = vppx_bld | 2 << 4 | 1 << 31;

		if (crtc_index == 1)
			osd_vpp1_bld_ctrl = val;
		else if (crtc_index == 2)
			osd_vpp2_bld_ctrl = val;
		else
			DRM_DEBUG("invalid crtc index\n");

		drm_postblend_notify_amvideo();

		if (bld_src2_sel == 2) {
			if (postblend->postblend_path_mask) {
				reg_ops->rdma_write_reg_bits(PATH_START_SEL, crtc_index, 24, 2);
				reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 1, 2, 1);
				reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 4, 1);
			} else {
				reg_ops->rdma_write_reg(VPP_OSD3_SCALE_CTRL, 0x7);
			}
		} else if (bld_src2_sel == 3) {
			reg_ops->rdma_write_reg(VPP_OSD4_SCALE_CTRL, 0x7);
		} else {
			DRM_DEBUG("invalid src2_sel %d\n", bld_src2_sel);
		}
	}

	DRM_DEBUG("scope h/v start/end [%d,%d,%d,%d].\n",
		  scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

static void s5_postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	int crtc_index, vpp_osd1_mux;
	struct am_meson_crtc *amc;
	struct am_meson_crtc_state *meson_crtc_state;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];
	meson_crtc_state = to_am_meson_crtc_state(amc->base.state);

	DRM_DEBUG("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	mvsps = &mvps->sub_states[0];

	scope.h_start = 0;
	scope.v_start = 0;
	if (mvsps->more_4k) {
		scope.h_end = mvsps->blend_dout_hsize[0] * 2 - 1;
		scope.v_end = mvsps->blend_dout_vsize[0] * 2 - 1;
	} else {
		scope.h_end = mvsps->blend_dout_hsize[0] - 1;
		scope.v_end = mvsps->blend_dout_vsize[0] - 1;
	}

	vpp_osd1_blend_scope_set(vblk, reg_ops, reg, scope);

	if (amc->blank_enable) {
		vpp_osd1_postblend_5mux_set(vblk, reg_ops, reg, VPP_NULL);
	} else {
		vpp_osd1_mux = VPP_5MUX_OSD1;
		vpp_osd1_postblend_5mux_set(vblk, reg_ops, reg, vpp_osd1_mux);
	}

	osd1_blend_premult_set(vblk, reg_ops, reg);
	reg_ops->rdma_write_reg_bits(VPP_POSTBLND_CTRL_S5, 1, 8, 1);

	DRM_DEBUG("scope h/v start/end [%d,%d,%d,%d].\n",
		  scope.h_start, scope.h_end, scope.v_start, scope.v_end);
}

static void t3x_postblend_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state)
{
	int crtc_index, vpp_osd1_mux;
	struct am_meson_crtc *amc;
	struct am_meson_crtc_state *meson_crtc_state;
	struct meson_vpu_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline_state *mvsps;

	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct osd_scope_s scope = {0, 1919, 0, 1079};
	struct meson_vpu_pipeline *pipeline = postblend->base.pipeline;
	struct postblend_reg_s *reg = postblend->reg;
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;

	crtc_index = vblk->index;
	amc = vblk->pipeline->priv->crtcs[crtc_index];
	meson_crtc_state = to_am_meson_crtc_state(amc->base.state);

	DRM_DEBUG("%s set_state called.\n", postblend->base.name);
	mvps = priv_to_pipeline_state(pipeline->obj.state);
	mvsps = &mvps->sub_states[0];
	if (crtc_index == 0) {
		scope.h_start = 0;
		scope.v_start = 0;
		if (mvsps->more_4k) {
			scope.h_end = mvsps->blend_dout_hsize[0] * 2 - 1;
			scope.v_end = mvsps->blend_dout_vsize[0] * 2 - 1;
		} else {
			scope.h_end = mvsps->blend_dout_hsize[0] - 1;
			scope.v_end = mvsps->blend_dout_vsize[0] - 1;
		}

		vpp_osd1_blend_scope_set(vblk, reg_ops, reg, scope);

		if (amc->blank_enable) {
			vpp_osd1_postblend_5mux_set(vblk, reg_ops, reg, VPP_NULL);
		} else {
			vpp_osd1_mux = VPP_5MUX_OSD1;
			vpp_osd1_postblend_5mux_set(vblk, reg_ops, reg, vpp_osd1_mux);
		}

		osd1_blend_premult_set(vblk, reg_ops, reg);
		reg_ops->rdma_write_reg_bits(VPP_POSTBLND_CTRL_S5, 1, 8, 1);

		DRM_DEBUG("scope h/v start/end [%d,%d,%d,%d].\n",
			scope.h_start, scope.h_end, scope.v_start, scope.v_end);
	}
	if (crtc_index == 1) {
		/* 1:vd1-din0, 2:osd1-din1*/
		scope.h_start = mvps->plane_info[2].dst_x;
		scope.h_end = scope.h_start + mvps->scaler_param[2].output_width - 1;
		scope.v_start = mvps->plane_info[2].dst_y;
		scope.v_end = scope.v_start + mvps->scaler_param[2].output_height - 1;
		reg_ops->rdma_write_reg_bits(VIU_MODE_CTRL, 1, 1, 1);
		reg_ops->rdma_write_reg(VPP1_OSD3_BLD_H_SCOPE,
					(scope.h_start << 16) | scope.h_end);
		reg_ops->rdma_write_reg(VPP1_OSD3_BLD_V_SCOPE,
					(scope.v_start << 16) | scope.v_end);
		reg_ops->rdma_write_reg(VPP1_BLD_CTRL_T3X,
		(reg_ops->rdma_read_reg(VPP1_BLD_CTRL_T3X) & (3 << 29)) |
		1 << 31 | 2 << 4 | 1 << 29);
		//osd3 link vsync2
		reg_ops->rdma_write_reg_bits(VIU_OSD3_MISC, 1, 0, 1);
		reg_ops->rdma_write_reg_bits(OSD_PROC_1MUX3_SEL, 0, 4, 2);
		reg_ops->rdma_write_reg_bits(OSD_SYS_5MUX4_SEL, 5, 8, 4);
	}
}

#endif

static void postblend_hw_enable(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	DRM_DEBUG("%s enable called.\n", postblend->base.name);
}

static void postblend_hw_disable(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state)
{
	u32 vppx_bld;
	int crtc_index = vblk->index;
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;
	struct postblend1_reg_s *reg1 = postblend->reg1;

	if (vblk->index == 0)
		vpp_osd1_postblend_mux_set(vblk, state->sub->reg_ops, postblend->reg, VPP_NULL);
	else if (vblk->index == 1 || vblk->index == 2) {
		vppx_bld = reg_ops->rdma_read_reg(reg1->vpp_bld_ctrl);
		vppx_bld = vppx_bld & 0xffffff0f;
		if (crtc_index == 1)
			osd_vpp1_bld_ctrl = vppx_bld;
		else if (crtc_index == 2)
			osd_vpp2_bld_ctrl = vppx_bld;
		else
			DRM_DEBUG("invalid crtc index\n");

		drm_postblend_notify_amvideo();
	}

	DRM_DEBUG("%s disable called.\n", postblend->base.name);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void s5_postblend_hw_disable(struct meson_vpu_block *vblk,
				    struct meson_vpu_block_state *state)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	vpp_osd1_postblend_5mux_set(vblk, state->sub->reg_ops, postblend->reg, VPP_NULL);
	DRM_DEBUG("%s disable called.\n", postblend->base.name);
}
#endif

static void postblend_dump_register(struct meson_vpu_block *vblk,
				    struct seq_file *seq)
{
	u32 value;
	struct meson_vpu_postblend *postblend;
	struct postblend_reg_s *reg;

	postblend = to_postblend_block(vblk);
	reg = postblend->reg;

	value = meson_drm_read_reg(reg->osd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->osd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "OSD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd1_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD1_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vd2_blend_src_ctrl);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VD2_BLEND_SRC_CTRL:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_in_size);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_IN_SIZE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd1_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD1_BLD_V_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_h_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_H_SCOPE:", value);

	value = meson_drm_read_reg(reg->vpp_osd2_bld_v_scope);
	seq_printf(seq, "%-35s\t\t0x%08X\n", "VPP_OSD2_BLD_V_SCOPE:", value);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
/*based on crtc index select corresponding rdma handle write method*/
static void fix_vpu_clk2_default_regs(struct meson_vpu_block *vblk,
				 struct rdma_reg_ops *reg_ops, int crtc_index, u32 *crtcmask_osd)
{
	int i;

	if (crtc_index == 0) {
		/* default: osd byp dolby */
		reg_ops->rdma_write_reg_bits(VPP_VD1_DSC_CTRL, 0x1, 4, 1);
		reg_ops->rdma_write_reg_bits(VPP_VD2_DSC_CTRL, 0x1, 4, 1);
		reg_ops->rdma_write_reg_bits(VPP_VD3_DSC_CTRL, 0x1, 4, 1);
		reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x1, 14, 1);
		reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x1, 19, 1);
		/* default: osd 12bit path */
		reg_ops->rdma_write_reg_bits(VPP_VD1_DSC_CTRL, 0x0, 5, 1);
		reg_ops->rdma_write_reg_bits(VPP_VD2_DSC_CTRL, 0x0, 5, 1);
		reg_ops->rdma_write_reg_bits(VPP_VD3_DSC_CTRL, 0x0, 5, 1);
		reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x0, 15, 1);
		reg_ops->rdma_write_reg_bits(MALI_AFBCD_TOP_CTRL, 0x0, 20, 1);
		/* OSD3  uses VPP1*/
		osd_set_vpp_path_default(vblk, reg_ops, 3, 1);
		/* OSD4  uses VPP2*/
		osd_set_vpp_path_default(vblk, reg_ops, 4, 2);
	}

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (crtcmask_osd[i] == crtc_index) {
			if (i == 0) {
				reg_ops->rdma_write_reg_bits(VPP_OSD1_SCALE_CTRL, 0x2, 0, 3);
			} else if (i == 1) {
				reg_ops->rdma_write_reg_bits(VPP_OSD2_SCALE_CTRL, 0x3, 0, 3);
			} else if (i == 2) {
				reg_ops->rdma_write_reg_bits(VPP_OSD3_SCALE_CTRL, 0x7, 0, 3);
				reg_ops->rdma_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x1, 19, 1);
				reg_ops->rdma_write_reg_bits(MALI_AFBCD1_TOP_CTRL, 0x0, 20, 1);
			} else if (i == 3) {
				reg_ops->rdma_write_reg_bits(VPP_OSD4_SCALE_CTRL, 0x3, 0, 3);
				reg_ops->rdma_write_reg_bits(MALI_AFBCD2_TOP_CTRL, 0x1, 19, 1);
				reg_ops->rdma_write_reg_bits(MALI_AFBCD2_TOP_CTRL, 0x0, 20, 1);
			}
		}
	}
}

static void independ_path_default_regs(struct meson_vpu_block *vblk,
				       struct rdma_reg_ops *reg_ops)
{
	/* default: osd1_bld_din_sel -- do not osd_data_byp osd_blend */
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 4, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x0, 4, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x0, 4, 1);

	/* default: osd1_sc_path_sel -- before osd_blend or after hdr */
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 0, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x1, 0, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 0, 1);

	/* default: osd byp dolby */
	reg_ops->rdma_write_reg_bits(VIU_VD1_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_VD2_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x1, 16, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x1, 16, 1);

	/* default: osd 12bit path */
	reg_ops->rdma_write_reg_bits(VIU_VD1_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_VD2_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, 0x0, 17, 1);
	reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, 0x0, 17, 1);

	/* OSD3  uses VPP0*/
	osd_set_vpp_path_default(vblk, reg_ops, 3, 0);
	/* OSD4  uses VPP0*/
	osd_set_vpp_path_default(vblk, reg_ops, 4, 0);
}
#endif

static void postblend_osd2_def_conf(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	osd2_blend_switch_set(vblk, vblk->pipeline->subs[0].reg_ops,
			      postblend->reg, VPP_POSTBLEND);
	vpp_osd2_preblend_mux_set(vblk, vblk->pipeline->subs[0].reg_ops,
				  postblend->reg, VPP_NULL);
	vpp_osd2_postblend_mux_set(vblk, vblk->pipeline->subs[0].reg_ops,
				   postblend->reg, VPP_NULL);
}

static void postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;
	postblend_osd2_def_conf(vblk);
	DRM_DEBUG("%s hw_init called.\n", postblend->base.name);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void t7_postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;
	postblend->reg1 = &postblend1_reg[vblk->index];

	DRM_DEBUG("%s hw_init called.\n", postblend->base.name);
}

static void t3_postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);

	postblend->reg = &postblend_reg;

	independ_path_default_regs(vblk, vblk->pipeline->subs[0].reg_ops);
	/*t3 t5w t5m paht crtl flag*/
	postblend->postblend_path_mask = true;
	DRM_DEBUG("%s hw_init called.\n", postblend->base.name);
}

static void s5_postblend_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_postblend *postblend = to_postblend_block(vblk);
	struct rdma_reg_ops *reg_ops = vblk->pipeline->subs[0].reg_ops;

	postblend->reg = &s5_postblend_reg;

	/* default: osd byp dolby */
	/*core2a core2c dv init in uboot*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 0, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 2, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 4, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x1, 6, 1);*/

	/* default: osd 12bit path */
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 1, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 3, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 5, 1);*/
	/*reg_ops->rdma_write_reg_bits(OSD_DOLBY_BYPASS_EN, 0x0, 7, 1);*/

	reg_ops->rdma_write_reg_bits(VPP_INTF_OSD3_CTRL, 0, 1, 1);
	reg_ops->rdma_write_reg(VPP_MISC_T3X, 0);
}
#endif

struct meson_vpu_block_ops postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = postblend_hw_init,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct meson_vpu_block_ops t7_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = t7_postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = t7_postblend_hw_init,
};

struct meson_vpu_block_ops t3_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = t7_postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = t3_postblend_hw_init,
};

struct meson_vpu_block_ops s5_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = s5_postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = s5_postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = s5_postblend_hw_init,
};

struct meson_vpu_block_ops t3x_postblend_ops = {
	.check_state = postblend_check_state,
	.update_state = t3x_postblend_set_state,
	.enable = postblend_hw_enable,
	.disable = s5_postblend_hw_disable,
	.dump_register = postblend_dump_register,
	.init = s5_postblend_hw_init,
};

#endif
