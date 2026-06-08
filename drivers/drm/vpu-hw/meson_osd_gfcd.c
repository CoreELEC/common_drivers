// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_osd_afbc.h"
#include "meson_osd_gfcd.h"

static struct gfcd_reg_s gfcd_reg[2] = {
	{
		GFCD_MIF_MODE_SEL,
		GFCD_MIF_HEAD_BADDR,
		GFCD_MIF_BODY_BADDR,
		GFCD_FRM_BGN,
		GFCD_FRM_END,
		GFCD_TOP_CTRL,
		GFCD_AFRC_CTRL,
		GFCD_AFBC_CTRL,
		GFCD_FRM_SIZE,
		GFCD_MIF_HEAD_CTRL0,
		GFCD_MIF_BODY_CTRL,
		GFCD_RO_SOLID_G0,
		VPP_OSD_HDR_DIV_ALPHA,
	},
	{
		GFCD_MIF_MODE_SEL_S1,
		GFCD_MIF_HEAD_BADDR_S1,
		GFCD_MIF_BODY_BADDR_S1,
		GFCD_FRM_BGN_S1,
		GFCD_FRM_END_S1,
		GFCD_TOP_CTRL_S1,
		GFCD_AFRC_CTRL_S1,
		GFCD_AFBC_CTRL_S1,
		GFCD_FRM_SIZE_S1,
		GFCD_MIF_HEAD_CTRL0_S1,
		GFCD_MIF_BODY_CTRL_S1,
		GFCD_RO_SOLID_G0_S1,
		VPP_OSD_HDR_DIV_ALPHA,
	},
};

static struct gfcd_reg_s a9_gfcd_reg[3] = {
	{
		GFCD_MIF_MODE_SEL_A9,
		GFCD_MIF_HEAD_BADDR_A9,
		GFCD_MIF_BODY_BADDR_A9,
		GFCD_FRM_BGN_A9,
		GFCD_FRM_END_A9,
		GFCD_TOP_CTRL_A9,
		GFCD_AFRC_CTRL_A9,
		GFCD_AFBC_CTRL_A9,
		GFCD_FRM_SIZE_A9,
		GFCD_MIF_HEAD_CTRL0_A9,
		GFCD_MIF_BODY_CTRL_A9,
		GFCD_RO_SOLID_G0_A9,
		VPP_OSD1_HDR_DIV_ALPHA,
		GFCD_GLB_ALPHA_A9,
	},
	{
		GFCD_MIF_MODE_SEL_S1_A9,
		GFCD_MIF_HEAD_BADDR_S1_A9,
		GFCD_MIF_BODY_BADDR_S1_A9,
		GFCD_FRM_BGN_S1_A9,
		GFCD_FRM_END_S1_A9,
		GFCD_TOP_CTRL_S1_A9,
		GFCD_AFRC_CTRL_S1_A9,
		GFCD_AFBC_CTRL_S1_A9,
		GFCD_FRM_SIZE_S1_A9,
		GFCD_MIF_HEAD_CTRL0_S1_A9,
		GFCD_MIF_BODY_CTRL_S1_A9,
		GFCD_RO_SOLID_G1_A9,
		VPP_OSD2_HDR_DIV_ALPHA,
		GFCD_GLB_ALPHA_S1_A9,
	},
	{
		GFCD_MIF_MODE_SEL_S2_A9,
		GFCD_MIF_HEAD_BADDR_S2_A9,
		GFCD_MIF_BODY_BADDR_S2_A9,
		GFCD_FRM_BGN_S2_A9,
		GFCD_FRM_END_S2_A9,
		GFCD_TOP_CTRL_S2_A9,
		GFCD_AFRC_CTRL_S2_A9,
		GFCD_AFBC_CTRL_S2_A9,
		GFCD_FRM_SIZE_S2_A9,
		GFCD_MIF_HEAD_CTRL0_S2_A9,
		GFCD_MIF_BODY_CTRL_S2_A9,
		GFCD_RO_SOLID_G2_A9,
		VPP_OSD3_HDR_DIV_ALPHA,
		GFCD_GLB_ALPHA_S2_A9,
	},
};

static int gfcd_format_is_rgbx(u32 pixel_format)
{
	int is_rgbx = 0;

	switch (pixel_format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
		is_rgbx = 1;
		break;
	default:
		break;
	}

	return is_rgbx;
}

static int gfcd_check_state(struct meson_vpu_block *vblk,
				 struct meson_vpu_block_state *state,
				 struct meson_vpu_sub_pipeline_state *mvps)
{
	return 0;
}

static void gfcd_set_surfaces(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state,
				struct meson_vpu_block_state *old_state,
				struct meson_vpu_sub_pipeline_state *mvsps)
{
	struct meson_vpu_gfcd *gfcd;
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd_layer_info *plane_info;
	struct rdma_reg_ops *reg_ops;
	struct gfcd_reg_s *reg;
	const struct meson_drm_format_info *info;
	u64 header_addr;
	u32 val;
	bool reverse_x, reverse_y, is_afrc, gfcd_en = 0;
	u8 tile_mode, blk_mode = 0, alpha_replace = 0;
	int i;
	u16 global_alpha = 256; /*range 0~256*/

	gfcd = to_gfcd_block(vblk);
	pipeline = gfcd->base.pipeline;
	reg_ops = state->sub->reg_ops;

	for (i = gfcd->start_surface; i <= gfcd->end_surface; i++) {
		plane_info = &mvsps->plane_info[i];
		reg = &gfcd->reg[i];
		gfcd_en = (plane_info->process_unit == GFCD_AFBC ||
			plane_info->process_unit == GFCD_AFRC);
		if (plane_info->enable && gfcd_en) {
			//check gfcd decoder status
			val = meson_drm_read_reg(reg->gfcd_ro_solid_g0);
			if (val & 0xf0000000) {
				DRM_ERROR("gfbc error happened 0x%x osd%d\n", val,
					plane_info->plane_index);
				gfcd->gfbc_err_cnt++;
			}
			reverse_x = (plane_info->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
			reverse_y = (plane_info->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;

			header_addr = plane_info->phy_addr;
			is_afrc = (plane_info->process_unit == GFCD_AFRC) ? 1 : 0;

			info = __meson_drm_gfcd_format_info(plane_info->pixel_format);
			if (info) {
				alpha_replace = info->alpha_replace;
				if (is_afrc)
					blk_mode = info->gfcd_hw_blkmode_afrc;
				else
					blk_mode = info->gfcd_hw_blkmode_afbc;
			}

			tile_mode = !!(plane_info->afbc_inter_format & TILED_HEADER_EN);
			/*reverse config*/
			reg_ops->rdma_write_reg_bits(reg->gfcd_mif_mode_sel, reverse_x, 5, 1);
			reg_ops->rdma_write_reg_bits(reg->gfcd_mif_mode_sel, reverse_y, 4, 1);

			/* set header addr */
			reg_ops->rdma_write_reg(reg->gfcd_mif_head_baddr, header_addr >> 4);

			/* set body addr for afrc*/
			if (is_afrc)
				reg_ops->rdma_write_reg(reg->gfcd_mif_body_baddr, header_addr >> 4);

			// TOP
			reg_ops->rdma_write_reg(reg->gfcd_frm_bgn,
						plane_info->src_y << 16 | plane_info->src_x);
			reg_ops->rdma_write_reg(reg->gfcd_frm_end,
				(plane_info->src_y + plane_info->src_h - 1) << 16 |
				(plane_info->src_x + plane_info->src_w - 1));

			/*0:afbc 1:afrc*/
			reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, is_afrc, 0, 1);

			reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 1, 4, 1);
			reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 0, 16, 2);

			if (alpha_replace) {
				reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 1, 30, 1);
				reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 0x3ff, 20, 10);
			} else {
				reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 0, 30, 1);
			}

			if (is_afrc) {
				/*0:config from reg 1:config from header*/
				reg_ops->rdma_write_reg_bits(reg->gfcd_afrc_ctrl, 0, 0, 1);

				/*0:16byte 1:24byte 2:32byte*/
				reg_ops->rdma_write_reg_bits(reg->gfcd_afrc_ctrl,
					plane_info->afrc_cu_bits, 8, 2);

				reg_ops->rdma_write_reg_bits(reg->gfcd_afrc_ctrl, 1, 16, 3);
				reg_ops->rdma_write_reg_bits(reg->gfcd_afrc_ctrl, blk_mode, 20, 5);
			} else {
				/*4'd0: RGBA8888 4'd1: RGBA1010102 4'd2: RGB888 4'd3: RGBA10101010*/
				reg_ops->rdma_write_reg_bits(reg->gfcd_afbc_ctrl,
						blk_mode, 0, 4);
				/*yuv_trans android should be enable*/
				reg_ops->rdma_write_reg_bits(reg->gfcd_afbc_ctrl, 1, 4, 1);
			}
			/*2: even skip(line 0 begin)*/
			if (plane_info->gfcd_vskip)
				reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 1, 16, 2);
			else
				reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 0, 16, 2);

			if (plane_info->gfcd_hskip)
				reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 1, 18, 2);
			else
				reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 0, 18, 2);

			reg_ops->rdma_write_reg_bits(reg->gfcd_frm_size, plane_info->fb_h, 0, 13);
			reg_ops->rdma_write_reg_bits(reg->gfcd_frm_size, plane_info->fb_w, 16, 13);

			if (!is_afrc)
				reg_ops->rdma_write_reg_bits(reg->gfcd_mif_head_ctrl0,
							     tile_mode, 4, 2);
			else
				reg_ops->rdma_write_reg_bits(reg->gfcd_mif_body_ctrl,
							     tile_mode, 0, 2);

			/*a9 support*/
			if (gfcd->support_global_alpha) {
				/*drm alaph 16bit, amlogic alpha 8bit*/
				global_alpha = plane_info->global_alpha >> 8;
				if (global_alpha == 0xff)
					global_alpha = 0x100;
				reg_ops->rdma_write_reg_bits(reg->gfcd_glb_alpha,
							     global_alpha, 0, 9);
			}
			MESON_DRM_BLOCK("is_afrc=%d, afrc_cu_bits=%d blk_mode=%d tile_mode=%d.\n",
					is_afrc, plane_info->afrc_cu_bits, blk_mode, tile_mode);
		} else {
			reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 0, 4, 1);
		}
		MESON_DRM_BLOCK("osd%d, enable=%d, gfcd_en=%d, gfcd vskip is %d, hskip is %d.\n",
				plane_info->plane_index, plane_info->enable, gfcd_en,
				plane_info->gfcd_vskip, plane_info->gfcd_hskip);
	}
}

static void s7d_gfcd_set_state(struct meson_vpu_block *vblk,
			       struct meson_vpu_block_state *state,
			       struct meson_vpu_block_state *old_state,
			       struct meson_vpu_sub_pipeline_state *mvsps)
{
	struct meson_vpu_gfcd *gfcd;
	struct meson_vpu_osd_layer_info *plane_info;
	struct rdma_reg_ops *reg_ops;
	struct gfcd_reg_s *reg;

	bool alpha_div_en = 0, gfcd_en = 0;
	int i;

	gfcd = to_gfcd_block(vblk);
	reg_ops = state->sub->reg_ops;

	gfcd_set_surfaces(vblk, state, old_state, mvsps);

	for (i = gfcd->start_surface; i <= gfcd->end_surface; i++) {
		plane_info = &mvsps->plane_info[i];
		reg = &gfcd->reg[i];
		gfcd_en = (plane_info->process_unit == GFCD_AFBC ||
			plane_info->process_unit == GFCD_AFRC);
		if (plane_info->enable && gfcd_en) {
			/*div alpha*/
			if (plane_info->pixel_blend == DRM_MODE_BLEND_PREMULTI &&
				!gfcd_format_is_rgbx(plane_info->pixel_format))
				alpha_div_en = 1;

			reg_ops->rdma_write_reg_bits(reg->vpp_osd_hdr_div_alpha, alpha_div_en,
					i * 5, 1);
			/*8 bit alpha, set 0 9bit alpha, set 3 default*/
			reg_ops->rdma_write_reg_bits(reg->vpp_osd_hdr_div_alpha, 3, i * 5 + 3, 2);
		} else {
			reg_ops->rdma_write_reg_bits(reg->vpp_osd_hdr_div_alpha, 0, i * 5, 1);
		}
	}

	MESON_DRM_BLOCK("%s set_state called.\n", vblk->name);
}

static void a9_gfcd_set_state(struct meson_vpu_block *vblk,
			      struct meson_vpu_block_state *state,
			      struct meson_vpu_block_state *old_state,
			      struct meson_vpu_sub_pipeline_state *mvsps)
{
	struct meson_vpu_gfcd *gfcd;
	struct meson_vpu_osd_layer_info *plane_info;
	struct rdma_reg_ops *reg_ops;
	struct gfcd_reg_s *reg;

	bool alpha_div_en = 0, gfcd_en = 0;
	int i;

	gfcd = to_gfcd_block(vblk);
	reg_ops = state->sub->reg_ops;

	gfcd_set_surfaces(vblk, state, old_state, mvsps);

	for (i = gfcd->start_surface; i <= gfcd->end_surface; i++) {
		plane_info = &mvsps->plane_info[i];
		reg = &gfcd->reg[i];
		gfcd_en = (plane_info->process_unit == GFCD_AFBC ||
			plane_info->process_unit == GFCD_AFRC);
		if (plane_info->enable && gfcd_en) {
			/*div alpha*/
			if (plane_info->pixel_blend == DRM_MODE_BLEND_PREMULTI &&
				!gfcd_format_is_rgbx(plane_info->pixel_format))
				alpha_div_en = 1;

			reg_ops->rdma_write_reg_bits(reg->vpp_osd_hdr_div_alpha,
							     alpha_div_en, 0, 1);
		} else {
			reg_ops->rdma_write_reg_bits(reg->vpp_osd_hdr_div_alpha, 0, 0, 1);
		}
	}

	MESON_DRM_BLOCK("%s set_state called.\n", vblk->name);
}

static void gfcd_hw_enable(struct meson_vpu_block *vblk, struct meson_vpu_block_state *state)
{
}

static void gfcd_hw_disable(struct meson_vpu_block *vblk,
	struct meson_vpu_sub_pipeline *sub_pipeline)
{
	int i;
	struct gfcd_reg_s *reg;
	struct meson_vpu_gfcd *gfcd = to_gfcd_block(vblk);
	struct rdma_reg_ops *reg_ops = sub_pipeline->reg_ops;

	for (i = gfcd->start_surface; i <= gfcd->end_surface; i++) {
		reg = &gfcd->reg[i];
		reg_ops->rdma_write_reg_bits(reg->gfcd_top_ctrl, 0, 4, 1);
	}

	MESON_DRM_BLOCK("%s disable called.\n", gfcd->base.name);
}

static void gfcd_dump_register(struct drm_printer *p,
							struct meson_vpu_block *vblk)
{
	u32 value = 0, reg_addr;
	struct meson_vpu_gfcd *gfcd;
	struct gfcd_reg_s *reg;
	char buff[16];
	int i;

	gfcd = to_gfcd_block(vblk);

	for (i = gfcd->start_surface; i <= gfcd->end_surface; i++) {
		reg = &gfcd->reg[i];
		snprintf(buff, 8, "OSD%d", i + 1);
		drm_printf(p, "gfbc error [%d]\n", gfcd->gfbc_err_cnt);
		reg_addr = reg->gfcd_mif_mode_sel;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_MIF_MODE_SEL", reg_addr, value);

		reg_addr = reg->gfcd_mif_head_baddr;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_MIF_HEAD_BADDR", reg_addr, value);

		reg_addr = reg->gfcd_mif_body_baddr;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_MIF_BODY_BADDR", reg_addr, value);

		reg_addr = reg->gfcd_frm_bgn;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_FRM_BGN", reg_addr, value);

		reg_addr = reg->gfcd_frm_end;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_FRM_END", reg_addr, value);

		reg_addr = reg->gfcd_top_ctrl;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_TOP_CTRL", reg_addr, value);

		reg_addr = reg->gfcd_afrc_ctrl;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_AFRC_CTRL", reg_addr, value);

		reg_addr = reg->gfcd_afbc_ctrl;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_AFBC_CTRL", reg_addr, value);

		reg_addr = reg->gfcd_frm_size;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_FRM_SIZE", reg_addr, value);

		reg_addr = reg->gfcd_mif_head_ctrl0;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_MIF_HEAD_CTRL0", reg_addr, value);

		reg_addr = reg->gfcd_mif_body_ctrl;
		value = meson_drm_read_reg(reg_addr);
		drm_printf(p, "%s_%-27s\t\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
				  "GFCD_MIF_BODY_CTRL", reg_addr, value);
	}

	reg_addr = OSD_PATH_MISC_CTRL;
	value = meson_drm_read_reg(reg_addr);
	drm_printf(p, "%-35s\t\taddr: 0x%04X\tvalue: 0x%08X\n",
			  "OSD_PATH_MISC_CTRL", reg_addr, value);
}

static void gfcd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_gfcd *gfcd = to_gfcd_block(vblk);

	gfcd->reg = &gfcd_reg[0];

	switch (vblk->index) {
	case GFCD_CORE1:
		gfcd->start_surface = 0;
		gfcd->end_surface = 1;
		break;
	default:
		gfcd->start_surface = 0;
		gfcd->end_surface = 0;
		break;
	};

	DRM_DEBUG("%s hw init.\n", vblk->name);
}

static void a9_gfcd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_gfcd *gfcd = to_gfcd_block(vblk);

	gfcd->reg = &a9_gfcd_reg[0];

	switch (vblk->index) {
	case GFCD_CORE1:
		gfcd->start_surface = 0;
		gfcd->end_surface = 1;
		break;
	case GFCD_CORE2:
		gfcd->start_surface = 2;
		gfcd->end_surface = 2;
		break;
	default:
		gfcd->start_surface = 0;
		gfcd->end_surface = 0;
		break;
	};

	gfcd->support_global_alpha = 1;

	DRM_DEBUG("%s hw init.\n", vblk->name);
}

static void gfcd_hw_fini(struct meson_vpu_block *vblk)
{
}

struct meson_vpu_block_ops gfcd_ops = {
	.check_state = gfcd_check_state,
	.update_state = s7d_gfcd_set_state,
	.enable = gfcd_hw_enable,
	.disable = gfcd_hw_disable,
	.dump_register = gfcd_dump_register,
	.init = gfcd_hw_init,
	.fini = gfcd_hw_fini,
};

struct meson_vpu_block_ops a9_gfcd_ops = {
	.check_state = gfcd_check_state,
	.update_state = a9_gfcd_set_state,
	.enable = gfcd_hw_enable,
	.disable = gfcd_hw_disable,
	.dump_register = gfcd_dump_register,
	.init = a9_gfcd_hw_init,
	.fini = gfcd_hw_fini,
};

