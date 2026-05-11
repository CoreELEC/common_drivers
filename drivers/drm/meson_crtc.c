// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/sync_file.h>

#include "meson_crtc.h"
#include "meson_vpu_pipeline.h"
#include <linux/init.h>
#include <linux/amlogic/gki_module.h>

#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <vout/vout_serve/vout_func.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include <enhancement/amvecm/amcsc.h>
#include "meson_drm_rdma.h"
#include "meson_async_atomic.h"
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_DRM
#include <trace/events/meson_atrace.h>
#endif

#define EOTF_RESERVED 23

#define OSD_DUMP_PATH		"/tmp/osd_dump/"

int gamma_ctl = 1;
int meson_gamma_ctl = -1;

static int drm_get_dv_support_info(void)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	return get_dv_support_info();
#else
	return 0;
#endif
}

static int gamma_boot_ctl(char *str)
{
	if (strncmp("0", str, 1) == 0)
		gamma_ctl = 0;
	else
		gamma_ctl = 1;

	return 1;
}

__setup("gamma=", gamma_boot_ctl);

static void meson_crtc_destroy_state(struct drm_crtc *crtc,
	struct drm_crtc_state *state)
{
	struct am_meson_crtc_state *meson_crtc_state;

	meson_crtc_state = to_am_meson_crtc_state(state);

	am_meson_drm_put_logo_fb(crtc->dev, crtc->index, meson_crtc_state->uboot_mode_init);
	__drm_atomic_helper_crtc_destroy_state(&meson_crtc_state->base);
	kfree(meson_crtc_state);
}

static struct drm_crtc_state *meson_crtc_duplicate_state(struct drm_crtc *crtc)
{
	struct am_meson_crtc_state *new_state, *cur_state;

	cur_state = to_am_meson_crtc_state(crtc->state);

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	__drm_atomic_helper_crtc_duplicate_state(crtc, &new_state->base);

	new_state->crtc_hdr_process_policy =
		cur_state->crtc_hdr_process_policy;
	new_state->crtc_eotf_type = cur_state->crtc_eotf_type;
	new_state->crtc_dv_enable = cur_state->crtc_dv_enable;
	new_state->crtc_hdr_enable = cur_state->crtc_hdr_enable;
	new_state->dv_mode = cur_state->dv_mode;
	new_state->preset_vmode = VMODE_INVALID;
	new_state->vmode = cur_state->vmode & VMODE_MODE_BIT_MASK;
	new_state->prev_vrefresh = cur_state->prev_vrefresh;
	new_state->prev_height = cur_state->prev_height;
	new_state->hdr_conversion_ctrl = cur_state->hdr_conversion_ctrl;
	new_state->attr_changed = false;
	new_state->brr_update = false;
	new_state->brr = cur_state->brr;
	new_state->seamless = false;
	new_state->vrr_type = cur_state->vrr_type;
	new_state->game_rate = cur_state->game_rate;
	strncpy(new_state->brr_mode, cur_state->brr_mode, DRM_DISPLAY_MODE_LEN);
	new_state->crtc_bgcolor_flag = cur_state->crtc_bgcolor_flag;
	new_state->crtc_bgcolor = cur_state->crtc_bgcolor;
	new_state->outfence_ctrl = cur_state->outfence_ctrl;
	new_state->viu_mux = VIU_MUX_INVALID;
	/*reset dynamic info.*/

	new_state->uboot_mode_init = 0;

	return &new_state->base;
}

static void meson_crtc_init_hdr_preference
	(struct am_meson_crtc_state *crtc_state)
{
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
	crtc_state->crtc_hdr_process_policy = get_hdr_policy();
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
	crtc_state->crtc_dv_enable = is_amdv_enable();
#else
	crtc_state->crtc_dv_enable = false;
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
	crtc_state->crtc_hdr_enable = true;
#else
	crtc_state->crtc_hdr_enable = false;
#endif
}

static void meson_crtc_reset(struct drm_crtc *crtc)
{
	struct am_meson_crtc_state *meson_crtc_state;

	if (crtc->state) {
		meson_crtc_destroy_state(crtc, crtc->state);
		crtc->state = NULL;
	}

	meson_crtc_state = kzalloc(sizeof(*meson_crtc_state), GFP_KERNEL);
	if (!meson_crtc_state)
		return;

	crtc->state = &meson_crtc_state->base;
	crtc->state->crtc = crtc;

	meson_crtc_init_hdr_preference(meson_crtc_state);
}

static int get_osd_pixelformat(void)
{
	int ret = 0;

	ret = BIT(RGBA_8888) | BIT(RGBX_8888) | BIT(RGB_888) | BIT(RGB_565) | BIT(BGRA_8888);
	return ret;
}

static int get_video_pixelformat(void)
{
	int ret = 0;

	ret = BIT(YCBCR_422_SP) | BIT(YCBCR_422_I);
	return ret;
}

static const char *meson_crtc_fence_get_driver_name(struct dma_fence *fence)
{
	return "meson";
}

static const char *meson_crtc_fence_get_timeline_name(struct dma_fence *fence)
{
	return "present_fence";
}

static const struct dma_fence_ops meson_crtc_fence_ops = {
	.get_driver_name = meson_crtc_fence_get_driver_name,
	.get_timeline_name = meson_crtc_fence_get_timeline_name,
};

struct dma_fence *meson_crtc_create_fence(spinlock_t *lock,
		struct drm_crtc *crtc)
{
	struct dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence)
		return NULL;

	dma_fence_init(fence, &meson_crtc_fence_ops, lock,
		       crtc->fence_context, ++crtc->fence_seqno);

	return fence;
}

int meson_crtc_creat_present_fence_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	struct drm_meson_present_fence *arg = data;
	struct am_meson_crtc_present_fence *pre_fence;
	struct am_meson_crtc *amcrtc;
	struct drm_crtc *crtc;
	struct dma_fence *fence;
	struct sync_file *sync_file;
	int fd, ret;

	if (arg->crtc_idx >= dev->num_crtcs)
		return -EINVAL;

	crtc = drm_crtc_from_index(dev, arg->crtc_idx);
	if (!crtc)
		return -EINVAL;

	amcrtc = to_am_meson_crtc(crtc);
	pre_fence = &amcrtc->present_fence;

	if (pre_fence->fence) {
		MESON_DRM_FENCE("fence already created, return!");
		return -EEXIST;
	}

	fence = meson_crtc_create_fence(&pre_fence->lock, crtc);
	if (!fence)
		return -ENOMEM;

	fd = get_unused_fd_flags(O_CLOEXEC);
	if (fd < 0) {
		ret = fd;
		goto err_put_fence;
	}

	sync_file = sync_file_create(fence);
	//dma_fence_put(fence);
	if (!sync_file) {
		ret = -ENOMEM;
		goto err_put_fd;
	}

	fd_install(fd, sync_file->file);
	arg->fd = fd;
	pre_fence->fd = fd;
	pre_fence->fence = fence;
	pre_fence->sync_file = sync_file;
	MESON_DRM_FENCE("fd=%d, fence=%px\n",
		pre_fence->fd, pre_fence->fence);

	return 0;

err_put_fd:
	put_unused_fd(fd);
err_put_fence:
	dma_fence_put(fence);
	return ret;
}

static int meson_crtc_atomic_get_property(struct drm_crtc *crtc,
	const struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t *val)
{
	struct am_meson_crtc_state *crtc_state =
		to_am_meson_crtc_state(state);
	struct am_meson_crtc *meson_crtc = to_am_meson_crtc(crtc);
	int ret = 0;

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	crtc_state->hdr_conversion_ctrl = get_hdr_cur_output();
#endif

	if (property == meson_crtc->hdr_policy) {
		*val = crtc_state->crtc_hdr_process_policy;
		return 0;
	} else if (property == meson_crtc->dv_enable_property) {
		*val = crtc_state->crtc_dv_enable;
		return 0;
	} else if (property == meson_crtc->bgcolor_property) {
		*val = crtc_state->crtc_bgcolor;
		return 0;
	} else if (property == meson_crtc->dv_mode_property) {
		*val = crtc_state->dv_mode;
		return 0;
	} else if (property == meson_crtc->osd_pixelformat_property) {
		*val = get_osd_pixelformat();
		return 0;
	} else if (property == meson_crtc->video_pixelformat_property) {
		*val = get_video_pixelformat();
		return 0;
	} else if (property == meson_crtc->hdr_conversion_ctrl_property) {
		*val = crtc_state->hdr_conversion_ctrl;
		return 0;
	} else if (property == meson_crtc->hdr_conversion_cap_property) {
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
		*val = get_hdr_conversion_cap();
#endif
		return 0;
	} else if (property == meson_crtc->force_output) {
		*val = crtc_state->force_output_type;
		return 0;
	} else if (property == meson_crtc->brr_update_property) {
		*val = crtc_state->brr_update;
		return 0;
	} else if (property == meson_crtc->drm_policy_property) {
		*val = meson_crtc->priv->of_conf.drm_policy_mask;
		return 0;
	} else if (property == meson_crtc->nonblock_by_vblank_property) {
		*val = crtc_state->nonblock_by_vblank;
		return 0;
	} else if (property == meson_crtc->dv_support_info) {
		*val = drm_get_dv_support_info();
		return 0;
	} else if (property == meson_crtc->vrr_type_property) {
		*val = crtc_state->vrr_type;
		return 0;
	} else if (property == meson_crtc->game_rate_property) {
		*val = crtc_state->game_rate;
		return 0;
	} else if (property == meson_crtc->vpu_clk_property) {
		*val = vpu_clk_get();
		return 0;
	} else if (property == meson_crtc->outfence_ctrl_property) {
		*val = crtc_state->outfence_ctrl;
		return 0;
	}

	return ret;
}

static int meson_crtc_atomic_set_property(struct drm_crtc *crtc,
	struct drm_crtc_state *state,
	struct drm_property *property,
	uint64_t val)
{
	struct am_meson_crtc_state *crtc_state =
		to_am_meson_crtc_state(state);
	struct am_meson_crtc *meson_crtc = to_am_meson_crtc(crtc);
	int ret = 0;

	if (property == meson_crtc->hdr_policy) {
		crtc_state->crtc_hdr_process_policy = val;
		return 0;
	} else if (property == meson_crtc->dv_enable_property) {
		crtc_state->crtc_dv_enable = val;
		return 0;
	} else if (property == meson_crtc->bgcolor_property) {
		crtc_state->crtc_bgcolor = val;
		crtc_state->crtc_bgcolor_flag = true;
		return 0;
	} else if (property == meson_crtc->dv_mode_property) {
		crtc_state->dv_mode = val;
		return 0;
	} else if (property == meson_crtc->hdr_conversion_ctrl_property) {
		crtc_state->hdr_conversion_ctrl = val;
		return 0;
	} else if (property == meson_crtc->force_output) {
		crtc_state->force_output_type = val;
		return 0;
	} else if (property == meson_crtc->brr_update_property) {
		crtc_state->brr_update = val;
		return 0;
	} else if (property == meson_crtc->nonblock_by_vblank_property) {
		crtc_state->nonblock_by_vblank = val;
		return 0;
	} else if (property == meson_crtc->vrr_type_property) {
		crtc_state->vrr_type = val;
		return 0;
	} else if (property == meson_crtc->game_rate_property) {
		crtc_state->game_rate = val;
		return 0;
	} else if (property == meson_crtc->outfence_ctrl_property) {
		crtc_state->outfence_ctrl = val;
		return 0;
	}

	return ret;
}

static void meson_crtc_atomic_print_state(struct drm_printer *p,
		const struct drm_crtc_state *state)
{
	struct am_meson_crtc_state *cstate =
			container_of(state, struct am_meson_crtc_state, base);
	struct am_meson_crtc *meson_crtc = to_am_meson_crtc(cstate->base.crtc);
	struct meson_drm *priv = meson_crtc->priv;
	struct meson_vpu_pipeline *pipeline = priv->pipeline;
	struct meson_vpu_sub_pipeline_state *mvps;
	struct meson_vpu_sub_pipeline *subs;
	struct drm_private_state *obj_state;
	int i;

	subs = priv->pipeline->subs[meson_crtc->crtc_index];
	if (!subs) {
		DRM_ERROR("null sub pipeline!\n");
		return;
	}

	obj_state = subs->obj.state;
	if (!obj_state) {
		DRM_ERROR("null pipeline obj state!\n");
		return;
	}

	mvps = container_of(obj_state, struct meson_vpu_sub_pipeline_state, obj);
	if (!mvps) {
		DRM_INFO("%s mvps is NULL!\n", __func__);
		return;
	}

	drm_printf(p, "\t\tvrr_enabled=%u\n", state->vrr_enabled);
	drm_printf(p, "\t\tbrr_mode=%s\n", cstate->brr_mode);
	drm_printf(p, "\t\tbrr=%u\n", cstate->brr);
	drm_printf(p, "\t\tbrr_update=%u\n", cstate->brr_update);
	drm_printf(p, "\t\tvrr_type=%u\n", cstate->vrr_type);
	drm_printf(p, "\t\tgame_rate=%u\n", cstate->game_rate);
	drm_printf(p, "\t\tseamless=%d\n", cstate->seamless);
	drm_printf(p, "\t\tforce_output=%d\n", cstate->force_output_type);
	drm_printf(p, "\t\tuboot_mode_init=%u\n", cstate->uboot_mode_init);
	drm_printf(p, "\t\toutfence_ctrl=%u\n", cstate->outfence_ctrl);
	drm_printf(p, "\t\tcrtc_hdr_policy:[%u,%u]\n",
		cstate->crtc_hdr_process_policy,
		cstate->crtc_eotf_type);

	drm_printf(p, "\t\tdv-hdr core state:[%d,%d]\n",
		cstate->crtc_dv_enable,
		cstate->crtc_hdr_enable);
	drm_printf(p, "\t\tvmode=%u, preset_vmode:%u\n", cstate->vmode, cstate->preset_vmode);

	drm_printf(p, "\tmeson vpu pipeline state:\n");
	drm_printf(p, "\t\tenable_blocks=%llu\n",
		mvps->enable_blocks);
	drm_printf(p, "\t\tnum_plane=%u\n", mvps->num_plane);
	drm_printf(p, "\t\tnum_plane_video=%u\n", mvps->num_plane_video);
	drm_printf(p, "\t\tglobal_afbc=%u\n", mvps->global_afbc);
	drm_printf(p, "\t\tdrm_policy_mask=%llu\n", priv->of_conf.drm_policy_mask);
	drm_printf(p, "\t\tdv_support_info=%d\n", drm_get_dv_support_info());
	drm_printf(p, "\t\tvpu_blocks_type=%d\n", pipeline->vpu_blocks_type);
	for (i = 0; i < MESON_MAX_OSDS; i++)
		drm_printf(p, "\t\tosd%d_capability=%llx\n", i, pipeline->osd_capability[i]);

	if (priv->vpu_data && priv->vpu_data->has_gfcd) {
		drm_printf(p, "\t\tgfcd_enable=%u\n",
			priv->of_conf.gfcd_enable);
		drm_printf(p, "\t\tgfcd_mask=%u\n",
			priv->of_conf.gfcd_mask);
		drm_printf(p, "\t\tosdblend_input_width_offset=%u\n",
			mvps->osdblend_input_width_offset);
		for (i = 0; i < MAX_DIN_NUM; i++)
			drm_printf(p, "\t\tosd_scope_width_offset[%u]=%u\n",
				i, mvps->osd_scope_width_offset[i]);
		for (i = 0; i < MESON_MAX_SCALERS; i++)
			drm_printf(p, "\t\tscaler_width_offset[%u]=in:%u out:%u\n", i,
				mvps->scaler_param[i].input_width_offset,
				mvps->scaler_param[i].output_width_offset);
	}
}

static const char * const pipe_crc_sources[] = {"vpp1", "NULL"};

static const char *const *meson_crtc_get_crc_sources(struct drm_crtc *crtc,
						     size_t *count)
{
	*count = ARRAY_SIZE(pipe_crc_sources);
	return pipe_crc_sources;
}

static int meson_crc_parse_source(const char *source, bool *enabled)
{
	int i;
	int count = ARRAY_SIZE(pipe_crc_sources);

	if (!source) {
		*enabled = false;
		return 0;
	}

	for (i = 0; i < count; i++) {
		if (!strcmp(pipe_crc_sources[i], source))
			break;
	}

	if (i >= count) {
		*enabled = false;
		return -EINVAL;
	} else if (!strcmp(pipe_crc_sources[i], "NULL")) {
		*enabled  = false;
	} else {
		*enabled = true;
	}

	return 0;
}

static int meson_crtc_set_crc_source(struct drm_crtc *crtc, const char *source)
{
	bool enabled = false;
	int ret = 0;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);

	ret = meson_crc_parse_source(source, &enabled);
	amcrtc->vpp_crc_enable = enabled;

	return ret;
}

static int meson_crtc_verify_crc_source(struct drm_crtc *crtc,
					const char *source, size_t *values_cnt)
{
	bool enabled;

	if (meson_crc_parse_source(source, &enabled) < 0) {
		DRM_ERROR("unknown source %s\n", source);
		return -EINVAL;
	}

	*values_cnt = 1;

	return 0;
}

static int meson_crtc_enable_vblank(struct drm_crtc *crtc)
{
	return 0;
}

static void meson_crtc_disable_vblank(struct drm_crtc *crtc)
{
}

#ifdef CONFIG_DEBUG_FS
static int meson_crtc_late_register(struct drm_crtc *crtc)
{
	meson_crtc_debugfs_late_init(crtc);
	return 0;
}
#else
static int meson_crtc_late_register(struct drm_crtc *crtc)
{
	return 0;
}
#endif

static void meson_crtc_early_unregister(struct drm_crtc *crtc)
{
}

static struct drm_crtc_funcs am_meson_crtc_funcs = {
	.atomic_destroy_state	= meson_crtc_destroy_state,
	.atomic_duplicate_state = meson_crtc_duplicate_state,
	.destroy		= drm_crtc_cleanup,
	.page_flip		= drm_atomic_helper_page_flip,
	.reset			= meson_crtc_reset,
	.set_config		= drm_atomic_helper_set_config,
	.atomic_get_property = meson_crtc_atomic_get_property,
	.atomic_set_property = meson_crtc_atomic_set_property,
	.atomic_print_state = meson_crtc_atomic_print_state,
	.get_crc_sources	= meson_crtc_get_crc_sources,
	.set_crc_source		= meson_crtc_set_crc_source,
	.verify_crc_source	= meson_crtc_verify_crc_source,
	.enable_vblank = meson_crtc_enable_vblank,
	.disable_vblank = meson_crtc_disable_vblank,
	.late_register = meson_crtc_late_register,
	.early_unregister = meson_crtc_early_unregister,
};

static bool am_meson_crtc_mode_fixup(struct drm_crtc *crtc,
				     const struct drm_display_mode *mode,
				struct drm_display_mode *adj_mode)
{
	/* TODO: drm_calc_timestamping_constants() do framedur_ns /= 2,
	 * reset crtc info same as logical size, so we can get correct
	 * framedur_ns.
	 */
	drm_mode_set_crtcinfo(adj_mode, 0);

	return true;
}

static void am_meson_crtc_atomic_enable(struct drm_crtc *crtc,
					struct drm_atomic_state *old_atomic_state)
{
	char *name, *brr_name;
	enum vmode_e mode;
	struct drm_display_mode *adjusted_mode = &crtc->state->adjusted_mode;
	int adjusted_vrefresh;
	struct drm_crtc_state *old_crtc_state;
	struct drm_display_mode *old_mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct meson_drm *priv = amcrtc->priv;
	struct am_meson_crtc_state *meson_crtc_state =
					to_am_meson_crtc_state(crtc->state);
	struct meson_vpu_sub_pipeline_state *mvsps =
		priv_to_sub_pipeline_state(pipeline->subs[amcrtc->crtc_index]->obj.state);
	struct am_meson_crtc_state *old_am_crtc_state;
	int hdrpolicy = 0;
	struct drm_connector_state *new_conn_state;
	struct meson_connector *mesonconn = NULL;
	struct drm_connector *connector;
	int i;

	for_each_new_connector_in_state(old_atomic_state, connector, new_conn_state, i) {
		if (new_conn_state->crtc == crtc) {
			connector = new_conn_state->connector;
			mesonconn = connector_to_meson_connector(connector);
			break;
		}
	}

	DRM_DEBUG_KMS("[%d]:in\n", amcrtc->crtc_index);
	old_crtc_state = drm_atomic_get_old_crtc_state(old_atomic_state, crtc);
	if (!old_crtc_state) {
		DRM_INFO("%s crtc state is NULL!\n", __func__);
		return;
	}

	old_am_crtc_state = to_am_meson_crtc_state(old_crtc_state);
	old_mode = &old_crtc_state->adjusted_mode;

	if (!adjusted_mode) {
		DRM_ERROR("meson_crtc_enable NULL mode failed.\n");
		return;
	}
	DRM_INFO("%s-[%d] in: new[%s], old[%s], vmode[%d-%d], uboot[%d], compat[%d], vrr[%d]\n",
		__func__, amcrtc->crtc_index, adjusted_mode->name, old_mode->name,
		meson_crtc_state->vmode, meson_crtc_state->preset_vmode,
		meson_crtc_state->uboot_mode_init, priv->compat_mode,
		crtc->state->vrr_enabled);
	adjusted_vrefresh = drm_mode_vrefresh(adjusted_mode);

	if (!priv->compat_mode) {
		/* update follow source/follow sink to hdr/dv core.
		 * drm didnot send hdmitx pkt, we just set policy to hdr core.
		 */
		if (meson_crtc_state->crtc_hdr_process_policy
				== MESON_HDR_POLICY_FOLLOW_SOURCE ||
			meson_crtc_state->crtc_hdr_process_policy
				== MESON_HDR_POLICY_FOLLOW_SINK) {
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
			/*enable/disable dv*/
			if (meson_crtc_state->crtc_dv_enable) {
				if (meson_crtc_state->crtc_eotf_type
						== HDMI_EOTF_MESON_DOLBYVISION_LL) {
					set_amdv_ll_policy(1);
				} else {
					set_amdv_ll_policy(0);
				}
				set_amdv_enable(true);
			}
		#endif

			hdrpolicy = (meson_crtc_state->crtc_hdr_process_policy
				== MESON_HDR_POLICY_FOLLOW_SINK) ? 0 : 1;
		#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
			set_hdr_policy(hdrpolicy);
		#endif
		}
		/*force eotf by property*/
		//set_eotf_by_property(meson_crtc_state);
		if (meson_crtc_state->force_output_type !=
			old_am_crtc_state->force_output_type) {
			set_force_output(meson_crtc_state->force_output_type);
		}
	}

	/*Turn on the settings function later
	 *if (priv->compat_mode) {
	 *	set_hdr_output(meson_crtc_state->hdr_conversion_ctrl);
	 *	DRM_INFO("[%s-%d] setting hdr output\n", __func__,
	 *			 meson_crtc_state->hdr_conversion_ctrl);
	 *}
	 */

	/*update mode*/
	name = adjusted_mode->name;
	if (crtc->state->vrr_enabled) {
		DRM_INFO("%s-[%d], adjust raw %s to brr %s\n",
			__func__, amcrtc->crtc_index,
			 name, meson_crtc_state->brr_mode);
		brr_name = meson_crtc_state->brr_mode;
		if (meson_crtc_state->valid_brr)
			name = brr_name;
	}

	if (meson_crtc_state->preset_vmode == VMODE_INVALID) {
		if (!mesonconn) {
			DRM_ERROR("not match crtc and mesonconn is NULL\n");
			return;
		}
		mode = vout_func_validate_vmode(amcrtc->vout_index, name,
			mesonconn->connector_type,  meson_crtc_state->frac);
		DRM_DEBUG("connector_type = %d, mode = %s, vmode =%d\n",
			mesonconn->connector_type, name, mode);
		if (mode == VMODE_MAX) {
			DRM_ERROR("crtc [%d]: no matched vout mode\n", amcrtc->crtc_index);
			return;
		}
	} else {
		mode = meson_crtc_state->preset_vmode;
		DRM_DEBUG_KMS("crtc [%d]: preset_vmode %d.\n", amcrtc->crtc_index, mode);
	}

	DRM_INFO("%s-[%d]: enable mode %s final vmode %d\n",
		__func__, amcrtc->crtc_index, name, mode);

	if (meson_crtc_state->uboot_mode_init)
		mode |= VMODE_INIT_BIT_MASK;

	if (meson_crtc_state->seamless) {
		drm_crtc_vblank_on(crtc);
		return;
	}

	vout_func_set_state(amcrtc->vout_index, mode);
	vout_func_update_viu(amcrtc->vout_index, meson_crtc_state->viu_mux);

	meson_crtc_state->vmode = mode;
	pipeline->subs[amcrtc->crtc_index]->vmode = mode;

	memcpy(&pipeline->subs[amcrtc->crtc_index]->mode, adjusted_mode,
	       sizeof(struct drm_display_mode));

	mvsps->vsync_enabled = 1;
	drm_crtc_vblank_on(crtc);

	DRM_DEBUG_KMS("[%d]: out\n", amcrtc->crtc_index);
}

static void am_meson_crtc_atomic_disable(struct drm_crtc *crtc,
						struct drm_atomic_state *old_atomic_state)
{
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct drm_crtc_state *old_crtc_state;
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc->state);
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct meson_vpu_sub_pipeline_state *mvsps =
		priv_to_sub_pipeline_state(pipeline->subs[amcrtc->crtc_index]->obj.state);
	enum vmode_e mode;
	int connector_type = 0;

	DRM_INFO("%s-[%d]:in\n", __func__, amcrtc->crtc_index);
	old_crtc_state = drm_atomic_get_old_crtc_state(old_atomic_state, crtc);
	if (!old_crtc_state) {
		DRM_INFO("%s crtc state is NULL!\n", __func__);
		return;
	}

	drm_crtc_vblank_off(crtc);

	if (crtc->state->event && !crtc->state->active) {
		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, crtc->state->event);
		spin_unlock_irq(&crtc->dev->event_lock);
		crtc->state->event = NULL;
	}

	if (meson_crtc_state->seamless) {
		DRM_INFO("skip set vmode to null\n");
		return;
	}

#ifdef CONFIG_AMLOGIC_LCD
	/*0=tv, 1=tablet, 2=invalid*/
	if ((meson_crtc_state->vmode & VMODE_MASK) == VMODE_LCD &&
		get_vout_lcd_mode(amcrtc->vout_index) == 0) {
		DRM_INFO("%s[%d], lcd skip setting null vmode\n",
				 __func__, meson_crtc_state->vmode);
		return;
	}
#endif
	mvsps->vsync_enabled = 0;
	meson_crtc_state->vmode = VMODE_INVALID;
	/* disable output by config null
	 * Todo: replace or delete it if have new method
	 */
	mode = vout_func_validate_vmode(amcrtc->vout_index, "null", connector_type, 0);
	if (mode == VMODE_MAX) {
		DRM_ERROR("no matched vout mode\n");
		return;
	}
	vout_func_set_state(amcrtc->vout_index, mode);
	DRM_DEBUG_KMS("out\n");
}

static int meson_crtc_atomic_check(struct drm_crtc *crtc,
	struct drm_atomic_state *atomic_state)
{
	int ret;
	struct meson_vpu_sub_pipeline_state *mvsps;
	struct drm_display_mode *mode;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct am_meson_crtc_state *cur_state =
		to_am_meson_crtc_state(crtc->state);
	struct meson_drm *priv = amcrtc->priv;
	struct drm_crtc_state *crtc_state;
	struct am_meson_crtc_state *new_state;
	ret = 0;

	crtc_state = drm_atomic_get_new_crtc_state(atomic_state, crtc);
	if (!crtc_state) {
		DRM_INFO("%s crtc state is NULL!\n", __func__);
		return -EINVAL;
	}

	mvsps = meson_vpu_pipeline_get_state(amcrtc->pipeline->subs[crtc->index],
			crtc_state->state);
	if (PTR_ERR(mvsps) == -EDEADLK) {
		DRM_DEBUG_ATOMIC("deadlock!\n");
		return -EDEADLK;
	}

	mode = &crtc_state->mode;
	if (mode->hdisplay > 4096 || mode->vdisplay > 2160)
		mvsps->more_4k = 1;
	else
		mvsps->more_4k = 0;

	mvsps->more_60 = 0;
	mvsps->pm_state = priv->pm_state;
	if (priv->vpu_data->slice_mode == 1) {
		if (drm_mode_vrefresh(mode) > 60 || priv->of_conf.force_slice)
			mvsps->more_60 = 1;
	}

	new_state = to_am_meson_crtc_state(crtc_state);
	DRM_DEBUG("modeset[%d]force_hint[%d]dv enable[%d %d]dv mode[%d %d]brr[%d]\n",
		atomic_state->allow_modeset, am_drm_param.crtc_force_hint,
		cur_state->crtc_dv_enable, new_state->crtc_dv_enable,
		cur_state->dv_mode, new_state->dv_mode, new_state->brr_update);
	/*apply parameters need modeset.*/
	if (atomic_state->allow_modeset) {
		/*apply state value not set from property.*/
		DRM_DEBUG_KMS("force modeset.\n");
		if (am_drm_param.crtc_force_hint > 0) {
			crtc_state->mode_changed = true;
			am_drm_param.crtc_force_hint = 0;
		}

		if (cur_state->crtc_dv_enable != new_state->crtc_dv_enable)
			crtc_state->mode_changed = true;

		if (cur_state->dv_mode != new_state->dv_mode)
			crtc_state->mode_changed = true;

		if (cur_state->hdr_conversion_ctrl != new_state->hdr_conversion_ctrl)
			crtc_state->mode_changed = true;

		if (new_state->brr_update)
			crtc_state->mode_changed = true;
	}

	if (!crtc_state->active)
		mvsps->vsync_enabled = 0;

	/*check plane-update*/
	if (!atomic_state->async_update || is_am_osd_async_commit(atomic_state))
		ret = vpu_pipeline_check(amcrtc->pipeline->subs[crtc->index], atomic_state);

	return ret;
}

static void am_meson_crtc_atomic_begin(struct drm_crtc *crtc,
	struct drm_atomic_state *old_atomic_state)
{
}

static void meson_drm_crtc_arm_vblank_event(struct drm_crtc *crtc,
			       struct drm_pending_vblank_event *e, bool outfence_ctrl)
{
	struct drm_device *dev = crtc->dev;
	unsigned int pipe = drm_crtc_index(crtc);
	int add;

	assert_spin_locked(&dev->event_lock);

	add = outfence_ctrl ? VBLANK_STEP_2 : VBLANK_STEP_1;

	e->pipe = pipe;
	e->sequence = drm_crtc_accurate_vblank_count(crtc) + add;
	list_add_tail(&e->base.link, &dev->vblank_event_list);
}

static void am_meson_crtc_atomic_flush(struct drm_crtc *crtc,
	struct drm_atomic_state *old_atomic_state)
{
	struct drm_color_ctm *ctm;
	struct drm_color_lut *lut;
	struct meson_vpu_sub_pipeline *sub_pipe;
	unsigned long flags;
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc);
	struct drm_crtc_state *old_crtc_state;
	struct meson_drm *priv = amcrtc->priv;
	struct meson_vpu_pipeline *pipeline = amcrtc->pipeline;
	struct am_meson_crtc_state *old_am_crtc_state;
	struct am_meson_crtc_state *meson_crtc_state;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	int gamma_lut_size = 0;
#endif

	int crtc_index = amcrtc->crtc_index;

	old_crtc_state = drm_atomic_get_old_crtc_state(old_atomic_state, crtc);
	if (!old_crtc_state) {
		DRM_INFO("%s state is NULL!\n", __func__);
		return;
	}
	old_am_crtc_state = to_am_meson_crtc_state(old_crtc_state);

	sub_pipe = pipeline->subs[crtc_index];
	meson_crtc_state = to_am_meson_crtc_state(crtc->state);
	if (crtc->state->color_mgmt_changed) {
		DRM_INFO("%s color_mgmt_changed!\n", __func__);
		if (crtc->state->ctm) {
			DRM_INFO("%s color_mgmt_changed 1!\n", __func__);
			ctm = (struct drm_color_ctm *)
				crtc->state->ctm->data;
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			am_meson_ctm_set(0, ctm);
			#endif
		} else {
			DRM_DEBUG_KMS("Disable CTM!\n");
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			am_meson_ctm_disable();
			#endif
		}
	}
	if (crtc->state->gamma_lut != priv->gamma_lut_blob && gamma_ctl) {
		DRM_DEBUG_KMS("GAMMA LUT blob changed!\n");
		drm_property_blob_put(priv->gamma_lut_blob);
		priv->gamma_lut_blob = NULL;
		if (crtc->state->gamma_lut) {
			DRM_INFO("%s Set GAMMA\n", __func__);
			priv->gamma_lut_blob =
				drm_property_blob_get(crtc->state->gamma_lut);
			lut = (struct drm_color_lut *)
				crtc->state->gamma_lut->data;
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			gamma_lut_size = amvecm_drm_get_gamma_size(0);
			amvecm_drm_gamma_set(0, lut, gamma_lut_size);
			#endif
		} else {
			DRM_DEBUG_KMS("Disable GAMMA!\n");
			#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
			amvecm_drm_gamma_disable(0);
			#endif
		}
	}

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
	ATRACE_BEGIN("crtc_flush");
#endif
	meson_commit_reenter_inc(pipeline->priv, crtc_index, NONBLOCK_MODE);
	if (!meson_crtc_state->uboot_mode_init) {
		/*
		 * C5 does not have a scanout pos register, so there is
		 * no need to determine when a register is written.
		 */
		if (amcrtc->pipeline->priv->vpu_data->enc_method != ENC_METHOD_NULL)
			vpu_pipeline_prepare_update(amcrtc->pipeline,
				crtc->mode.vdisplay, drm_mode_vrefresh(&crtc->mode), crtc_index);
		ATRACE_BEGIN("crtc_update");
		vpu_osd_pipeline_update(sub_pipe, old_atomic_state);
		spin_lock_irqsave(&crtc->dev->event_lock, flags);
		vpu_pipeline_finish_update(pipeline, crtc_index);
		spin_unlock_irqrestore(&crtc->dev->event_lock, flags);
		ATRACE_END();
	}
	meson_commit_reenter_dec(pipeline->priv, crtc_index, NONBLOCK_MODE);
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
	ATRACE_END();
#endif

	spin_lock_irqsave(&crtc->dev->event_lock, flags);
	if (crtc->state->event) {
		if (drm_crtc_vblank_get(crtc) == 0)
			meson_drm_crtc_arm_vblank_event(crtc, crtc->state->event,
			meson_crtc_state->outfence_ctrl);
		else
			amcrtc->event = crtc->state->event;

		crtc->state->event = NULL;
	}
	spin_unlock_irqrestore(&crtc->dev->event_lock, flags);

#ifdef CONFIG_AMLOGIC_VIDEO_DISPLAY
	if (meson_crtc_state->crtc_bgcolor !=
		old_am_crtc_state->crtc_bgcolor) {
		video_dummy_data_set(meson_crtc_state->crtc_bgcolor,
			meson_crtc_state->crtc_bgcolor_flag);
	}
#endif
}

bool am_meson_crtc_get_scanout_position(struct drm_crtc *crtc,
				 bool in_vblank_irq, int *vpos, int *hpos,
				 ktime_t *stime, ktime_t *etime,
				 const struct drm_display_mode *mode)
{
	int ret = 0;
	struct am_meson_crtc *am_crtc = to_am_meson_crtc(crtc);

	ret = am_crtc->get_scanout_position(am_crtc,
		in_vblank_irq, vpos, hpos, stime, etime, mode);

	return (ret == 0) ? true : false;
}

static const struct drm_crtc_helper_funcs am_crtc_helper_funcs = {
	.atomic_check	= meson_crtc_atomic_check,
	.mode_fixup		= am_meson_crtc_mode_fixup,
	.atomic_begin	= am_meson_crtc_atomic_begin,
	.atomic_flush	= am_meson_crtc_atomic_flush,
	.atomic_enable	= am_meson_crtc_atomic_enable,
	.atomic_disable	= am_meson_crtc_atomic_disable,
	.get_scanout_position = am_meson_crtc_get_scanout_position,
};

static int meson_crtc_get_scanout_position(struct am_meson_crtc *crtc,
	bool in_vblank_irq, int *vpos, int *hpos,
	ktime_t *stime, ktime_t *etime,
	const struct drm_display_mode *mode)
{
	int ret = 0;
	/*adjust mode crtc_vtotal is same as logical vtotal*/
	int real_vtotal = 0;

	if (stime)
		*stime = ktime_get();

	ret = vpu_pipeline_read_scanout_pos(crtc->pipeline, vpos, hpos, crtc->crtc_index);

	if (mode->flags & DRM_MODE_FLAG_INTERLACE) {
		/* for interlace mode, enc 0 ~ vtotal*2
		 * include two interlace image.
		 */
		real_vtotal = mode->crtc_vtotal >> 1;
		if (*vpos >= real_vtotal)
			*vpos -= real_vtotal;
	}

	if (etime)
		*etime = ktime_get();

	return ret;
}

static const struct drm_prop_enum_list force_output_list[] = {
	{ UNKNOWN_FMT, "UNKNOWN_FMT" },
	{ BT709, "BT709" },
	{ BT709_HDR, "BT709_HDR" },
	{ BT2020, "BT2020" },
	{ BT2020_PQ, "BT2020_PQ" },
	{ BT2020_PQ_DYNAMIC, "BT2020_PQ_DYNAMIC" },
	{ BT2020_HLG, "BT2020_HLG" },
	{ BT2100_IPT, "BT2100_IPT" },
	{ BT2020YUV_BT2020RGB_CUVA, "BT2020YUV_BT2020RGB_CUVA" },
	{ BT_BYPASS, "BT_BYPASS" }
};

static void meson_crtc_init_force_output_property(struct drm_device *drm_dev,
						   struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "force_output",
					force_output_list,
					ARRAY_SIZE(force_output_list));
	if (prop) {
		amcrtc->force_output = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, UNKNOWN_FMT);
	} else {
		DRM_ERROR("Failed to force_output property\n");
	}
}

static const struct drm_prop_enum_list hdr_policy_enum_list[] = {
	{ MESON_HDR_POLICY_FOLLOW_SINK, "HDR_POLICY_FOLLOW_SINK" },
	{ MESON_HDR_POLICY_FOLLOW_SOURCE, "HDR_POLICY_FOLLOW_SOURCE" },
	{ MESON_HDR_POLICY_FOLLOW_FORCE_MODE, "HDR_POLICY_FOLLOW_FORCE_MODE" },
};

static void meson_crtc_init_hdr_policy_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "meson.crtc.hdr_policy",
			hdr_policy_enum_list, ARRAY_SIZE(hdr_policy_enum_list));
	if (prop) {
		amcrtc->hdr_policy = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, MESON_HDR_POLICY_FOLLOW_SINK);
	} else {
		DRM_ERROR("Failed to UPDATE property\n");
	}
}

static void meson_crtc_init_dv_enable_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "dv_enable");
	if (prop) {
		amcrtc->dv_enable_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to dv_enable property\n");
	}
}

static void meson_crtc_init_outfence_ctrl_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "outfence_ctrl");
	if (prop) {
		amcrtc->outfence_ctrl_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to outfence_ctrl property\n");
	}
}

static void meson_crtc_init_brr_update_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "brr_update");
	if (prop) {
		amcrtc->brr_update_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to brr_update property\n");
	}
}

static void meson_crtc_init_dv_mode_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "dv_mode");
	if (prop) {
		amcrtc->dv_mode_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to dv_mode property\n");
	}
}

static void meson_crtc_add_bgcolor_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "BACKGROUND_COLOR",
					0, GENMASK_ULL(63, 0));
	if (prop) {
		amcrtc->bgcolor_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to background color property\n");
	}
}

static void meson_crtc_init_video_pixelformat_property(struct drm_device *drm_dev,
						struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "video_pixelformat",
					0, 65535);
	if (prop) {
		amcrtc->video_pixelformat_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to video pixelformatcolor property\n");
	}
}

static void meson_crtc_init_vpu_clk_property(struct drm_device *drm_dev,
						struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "vpu_clk",
					0, 65535);
	if (prop) {
		amcrtc->vpu_clk_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to vpu_clk property\n");
	}
}

static void meson_crtc_init_osd_pixelformat_property(struct drm_device *drm_dev,
						struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "osd_pixelformat",
					0, 65535);
	if (prop) {
		amcrtc->osd_pixelformat_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to osd pixelformatcolor property\n");
	}
}

static void meson_crtc_init_hdr_conversion_cap_property(struct drm_device *drm_dev,
						struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "hdr_conversion_cap",
					0, 4294967295ull);
	if (prop) {
		amcrtc->hdr_conversion_cap_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdr conversion cap property\n");
	}
}

static void meson_crtc_init_hdr_conversion_ctrl_property(struct drm_device *drm_dev,
						struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "hdr_conversion_ctrl",
					0, 36);
	if (prop) {
		amcrtc->hdr_conversion_ctrl_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdr conversion ctrl property\n");
	}
}

static void meson_crtc_init_drm_policy_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "drm_policy_mask",
					0, GENMASK_ULL(MAX_POLICY_ID - 1, 0));
	if (prop) {
		amcrtc->drm_policy_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to drm_policy property\n");
	}
}

static const u32 am_crtc_max_out_size_list[] = {
	1080 << 16 | 1920,
	2160 << 16 | 3840,
};

static void meson_crtc_add_max_out_property(struct drm_device *drm_dev,
					    struct am_meson_crtc *amcrtc)
{
	int ret;
	struct drm_property *prop;
	u32 max_out_size_indices[MESON_MAX_CRTC];
	u32 max_out_size_idx;
	u32 max_out_size;
	struct meson_vpu_pipeline *ppl = amcrtc->pipeline;

	ret = of_property_read_u32_array(drm_dev->dev->of_node,
			"crtc_max_out_size", max_out_size_indices, ppl->num_postblend);

	if (ret) {
		/* use 0 to indicate platforms without the property */
		max_out_size = 0;
	} else {
		max_out_size_idx = max_out_size_indices[amcrtc->crtc_index];
		max_out_size = am_crtc_max_out_size_list[max_out_size_idx];
	}
	prop = drm_property_create_range(drm_dev, DRM_MODE_PROP_IMMUTABLE,
					"crtc_max_out_size", 0, UINT_MAX);
	if (prop) {
		amcrtc->crtc_max_out_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, max_out_size);
	} else {
		DRM_ERROR("Failed to create crtc_max_out_size property\n");
	}
}

static void meson_crtc_init_nonblock_by_vblank_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_bool(drm_dev, 0, "nonblock_by_vblank");
	if (prop) {
		amcrtc->nonblock_by_vblank_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to nonblock by vblank property\n");
	}
}

static void meson_crtc_init_dv_support_info_property(struct drm_device *drm_dev,
						    struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "dv_support_info", 0, 15);
	if (prop) {
		amcrtc->dv_support_info = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to dv_support_info property\n");
	}
}

static const struct drm_prop_enum_list drm_vrr_type_enum_list[] = {
	{ DRM_VRR_QMS, "QMS-VRR"}, /* QMS VRR */
	{ DRM_VRR_GAME, "GAME-VRR"}, /* GAME VRR */
};

static void meson_crtc_init_vrr_type_property(struct drm_device *drm_dev,
					      struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "VRR_TYPE",
					drm_vrr_type_enum_list, ARRAY_SIZE(drm_vrr_type_enum_list));
	if (prop) {
		amcrtc->vrr_type_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to init vrr_type property\n");
	}
}

static void meson_crtc_init_game_rate_property(struct drm_device *drm_dev,
						  struct am_meson_crtc *amcrtc)
{
	struct drm_property *prop;

	prop = drm_property_create_range(drm_dev, 0, "GAME_RATE",
					0, USHRT_MAX);
	if (prop) {
		amcrtc->game_rate_property = prop;
		drm_object_attach_property(&amcrtc->base.base, prop, 0);
	} else {
		DRM_ERROR("Failed to init game_rate property\n");
	}
}

struct am_meson_crtc *meson_crtc_bind(struct meson_drm *priv, int idx)
{
	struct am_meson_crtc *amcrtc;
	struct drm_crtc *crtc;
	struct meson_vpu_pipeline *pipeline = priv->pipeline;
	struct meson_vpu_sub_pipeline *sub_pipeline;
	struct drm_plane *primary_plane;
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	int gamma_lut_size = 0;
#endif
	int ret, plane_index;
	char crtc_name[64];

	DRM_DEBUG_KMS("[%d]\n", __LINE__);

	amcrtc = devm_kzalloc(priv->dev, sizeof(*amcrtc), GFP_KERNEL);
	if (!amcrtc)
		return NULL;

	amcrtc->priv = priv;
	amcrtc->dev = priv->dev;
	amcrtc->drm_dev = priv->drm;
	amcrtc->crtc_index = idx;
	amcrtc->vout_index = idx + 1;/*vout index start from 1.*/
	amcrtc->pipeline = pipeline;
	strcpy(amcrtc->osddump_path, OSD_DUMP_PATH);

	crtc = &amcrtc->base;
	plane_index = priv->primary_plane_index[idx];
	primary_plane = &priv->osd_planes[plane_index]->base;
	sub_pipeline = pipeline->subs[idx];

	snprintf(crtc_name, 64, "%s-%d", "VPP", amcrtc->crtc_index);

	if (!priv->remove_get_vblank_timestamp)
		am_meson_crtc_funcs.get_vblank_timestamp =
			drm_crtc_vblank_helper_get_vblank_timestamp;

	ret = drm_crtc_init_with_planes(priv->drm, crtc,
					primary_plane, priv->cursor_plane,
					&am_meson_crtc_funcs, crtc_name);
	if (ret) {
		dev_err(amcrtc->drm_dev->dev, "Failed to init CRTC\n");
		return NULL;
	}

	drm_crtc_helper_add(crtc, &am_crtc_helper_funcs);
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	meson_vpu_reg_handle_register(sub_pipeline);
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
	ret = of_property_read_u32(priv->dev->of_node, "gamma_ctl", &meson_gamma_ctl);
	if (!gamma_ctl || !meson_gamma_ctl || priv->pxp_mode) {
		DRM_INFO("skip gamma init\n");
	} else {
		amvecm_drm_init(0);
		gamma_lut_size = amvecm_drm_get_gamma_size(0);
		drm_mode_crtc_set_gamma_size(crtc, gamma_lut_size);
		drm_crtc_enable_color_mgmt(crtc, 0, true, gamma_lut_size);
	}
#endif

	amcrtc->get_scanout_position = meson_crtc_get_scanout_position;
	amcrtc->force_crc_chk = 8;
	amcrtc->rdma_table_enable = rdma_tbl[idx].flag;
	atomic_set(&amcrtc->commit_num, 0);
	mutex_init(&amcrtc->commit_mutex);
	spin_lock_init(&amcrtc->present_fence.lock);
	crtc->fence_context = dma_fence_context_alloc(1);
	crtc->fence_seqno = 0;
	meson_crtc_init_hdr_policy_property(priv->drm, amcrtc);
	meson_crtc_init_dv_enable_property(priv->drm, amcrtc);
	meson_crtc_init_brr_update_property(priv->drm, amcrtc);
	meson_crtc_init_dv_mode_property(priv->drm, amcrtc);
	meson_crtc_add_bgcolor_property(priv->drm, amcrtc);
	meson_crtc_init_osd_pixelformat_property(priv->drm, amcrtc);
	meson_crtc_init_video_pixelformat_property(priv->drm, amcrtc);
	meson_crtc_init_vpu_clk_property(priv->drm, amcrtc);
	meson_crtc_init_hdr_conversion_cap_property(priv->drm, amcrtc);
	meson_crtc_init_hdr_conversion_ctrl_property(priv->drm, amcrtc);
	meson_crtc_init_force_output_property(priv->drm, amcrtc);
	meson_crtc_init_drm_policy_property(priv->drm, amcrtc);
	meson_crtc_init_nonblock_by_vblank_property(priv->drm, amcrtc);
	meson_crtc_init_dv_support_info_property(priv->drm, amcrtc);
	meson_crtc_init_vrr_type_property(priv->drm, amcrtc);
	meson_crtc_init_game_rate_property(priv->drm, amcrtc);
	meson_crtc_add_max_out_property(priv->drm, amcrtc);
	meson_crtc_init_vrr_type_property(priv->drm, amcrtc);
	meson_crtc_init_game_rate_property(priv->drm, amcrtc);
	meson_crtc_init_outfence_ctrl_property(priv->drm, amcrtc);

	priv->crtcs[priv->num_crtcs++] = amcrtc;

	return amcrtc;
}

