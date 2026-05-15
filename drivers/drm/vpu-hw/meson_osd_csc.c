// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "meson_vpu_pipeline.h"
#include "meson_vpu_reg.h"
#include "meson_osd_csc.h"
#include "meson_crtc.h"

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
	MESON_DRM_BLOCK("%s hw init.\n", vblk->name);
}

static void csc_register_init(struct meson_vpu_block *vblk,
				struct meson_vpu_block_state *state)
{
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
