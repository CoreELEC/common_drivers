// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_osd_csc.h"
#include "meson_crtc.h"

static struct csc_reg_s csc_reg_a9[HW_OSD_MATRIX_NUM] = {
	{
		VPP_WRAP_OSD1_MATRIX_COEF00_01,
		VPP_WRAP_OSD1_MATRIX_COEF02_10,
		VPP_WRAP_OSD1_MATRIX_COEF11_12,
		VPP_WRAP_OSD1_MATRIX_COEF20_21,
		VPP_WRAP_OSD1_MATRIX_COEF22,
		VPP_WRAP_OSD1_MATRIX_OFFSET0_1,
		VPP_WRAP_OSD1_MATRIX_OFFSET2,
		VPP_WRAP_OSD1_MATRIX_EN_CTRL,
	},
	{
		VPP_WRAP_OSD2_MATRIX_COEF00_01,
		VPP_WRAP_OSD2_MATRIX_COEF02_10,
		VPP_WRAP_OSD2_MATRIX_COEF11_12,
		VPP_WRAP_OSD2_MATRIX_COEF20_21,
		VPP_WRAP_OSD2_MATRIX_COEF22,
		VPP_WRAP_OSD2_MATRIX_OFFSET0_1,
		VPP_WRAP_OSD2_MATRIX_OFFSET2,
		VPP_WRAP_OSD2_MATRIX_EN_CTRL,
	},
	{
		VIU_OSD3_MATRIX_COEF00_01,
		VIU_OSD3_MATRIX_COEF02_10,
		VIU_OSD3_MATRIX_COEF11_12,
		VIU_OSD3_MATRIX_COEF20_21,
		VIU_OSD3_MATRIX_COEF22,
		VIU_OSD3_MATRIX_OFFSET0_1,
		VIU_OSD3_MATRIX_OFFSET2,
		VIU_OSD3_MATRIX_EN_CTRL,
	}
};

static int csc_check_state(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state,
				 struct meson_vpu_sub_pipeline_state *mvps)
{
	return 0;
}

static void csc_set_state(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state,
			   struct meson_vpu_sub_pipeline_state *mvsps)
{
	if (vblk->ops->init_register)
		vblk->ops->init_register(vblk, state);

	MESON_DRM_BLOCK("%s set_state called.\n", vblk->name);
}

static void csc_hw_enable(struct meson_vpu_block *vblk, struct meson_vpu_block_state *state)
{
}

static void csc_hw_disable(struct meson_vpu_block *vblk,
		struct meson_vpu_sub_pipeline *sub_pipeline)
{
}

static void csc_dump_register(struct drm_printer *p, struct meson_vpu_block *vblk)
{
}

static void csc_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_csc *csc = to_csc_block(vblk);

	csc->reg = &csc_reg_a9[0];
	MESON_DRM_BLOCK("%s hw init.\n", vblk->name);
}

static void csc_register_init(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state)
{
	struct meson_vpu_csc *csc = to_csc_block(vblk);
	struct rdma_reg_ops *reg_ops = state->sub->reg_ops;
	struct csc_reg_s *csc_reg = csc->reg;
	int crtc_index = vblk->index;
	struct am_meson_crtc *amc;
	u32 *crtcmask_osd = NULL;
	int i = 0;

	if (vblk->init_done)
		return;

	amc = vblk->pipeline->priv->crtcs[crtc_index];
	crtcmask_osd = amc->priv->of_conf.crtcmask_osd;

	/*
	 * If there is a VPP on the path, configure the csc
	 * rgb2yuv function before the VPP
	 */
	/* The first path always has a configuration option.*/
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_coef00_01, 0x00e60252);
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_coef02_10, 0x00341f83);
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_coef11_12, 0xfebd01c0);
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_coef20_21, 0x01c01e64);
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_coef22, 0x00001fdc);
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_offset0_1, 0x00400200);
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_offset2, 0x00400200);
	reg_ops->rdma_write_reg(csc_reg[0].vpp_wrap_osd_matrix_en_ctrl, 1);

	/* The second path selects its configuration based on the assigned OSD */
	for (i = 0; i < HW_OSD_MATRIX_NUM; i++) {
		if (crtcmask_osd[i] == VPP1) {
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_coef00_01,
									0x00e60252);
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_coef02_10,
									0x00341f83);
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_coef11_12,
									0xfebd01c0);
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_coef20_21,
									0x01c01e64);
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_coef22,
									0x00001fdc);
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_offset0_1,
									0x00400200);
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_offset2,
									0x00400200);
			reg_ops->rdma_write_reg(csc_reg[i].vpp_wrap_osd_matrix_en_ctrl, 1);
		}
	}

	vblk->init_done = 1;
	MESON_DRM_BLOCK("%s register_init called.\n", vblk->name);
}

static void csc_hw_fini(struct meson_vpu_block *vblk)
{
}

struct meson_vpu_block_ops csc_ops = {
	.check_state = csc_check_state,
	.update_state = csc_set_state,
	.dump_register = csc_dump_register,
	.enable = csc_hw_enable,
	.disable = csc_hw_disable,
	.init = csc_hw_init,
	.init_register = csc_register_init,
	.fini = csc_hw_fini,
};
