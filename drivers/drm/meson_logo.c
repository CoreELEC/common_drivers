// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/of_graph.h>
#include <linux/dma-map-ops.h>
#include <linux/of_device.h>
#include <linux/of.h>
#include <linux/of_reserved_mem.h>
#include <uapi/linux/sched/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/notifier.h>
#include <linux/clk.h>
#include <linux/cma.h>
#include <linux/of_address.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_flip_work.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_rect.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_fb_dma_helper.h>

#ifdef CONFIG_AMLOGIC_DRM_USE_ION
#include "meson_gem.h"
#include "meson_fb.h"
#endif
#include "meson_vpu.h"
#include "meson_vpu_pipeline.h"
#include "meson_crtc.h"
#include "meson_logo.h"
#include "meson_hdmi.h"
#include "meson_plane.h"

#ifdef CONFIG_AMLOGIC_MEDIA_FB
#include <linux/amlogic/media/osd/osd_logo.h>
#endif
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/gki_module.h>
#include <linux/amlogic/aml_free_reserved.h>

#ifdef CONFIG_CMA
struct cma *cma_logo;
#endif

static char *strmode;
struct am_meson_logo logo[MESON_MAX_CRTC];
#define NUM_CONNECTORS_LOGO	1
static struct platform_device *gp_dev;
static unsigned long gem_mem_start, gem_mem_size;
static struct resource osd_mem_res;
bool is_cma;

#ifndef CONFIG_AMLOGIC_MEDIA_FB
static u32 drm_logo_bpp = 16;
static u32 drm_logo_width = 1920;
static u32 drm_logo_height = 1080;

static int drm_logo_bpp_setup(char *str)
{
	int ret;

	ret = kstrtoint(str, 0, &drm_logo_bpp);
	if (ret)
		return ret;
	return 1;
}
__setup("display_bpp=", drm_logo_bpp_setup);

static u32 drm_logo_get_display_bpp(void)
{
	return drm_logo_bpp;
}

static int drm_logo_width_setup(char *str)
{
	int ret;

	ret = kstrtoint(str, 0, &drm_logo_width);
	if (ret)
		return ret;
	return 1;
}
__setup("fb_width=", drm_logo_width_setup);

static u32 drm_logo_get_fb_width(void)
{
	return drm_logo_width;
}

static int drm_logo_height_setup(char *str)
{
	int ret;

	ret = kstrtoint(str, 0, &drm_logo_height);
	if (ret)
		return ret;
	return 1;
}
__setup("fb_height=", drm_logo_height_setup);

static u32 drm_logo_get_fb_height(void)
{
	return drm_logo_height;
}

#define OSD_INVALID_INFO 0xffffffff
#define OSD_FIRST_GROUP_START 1
#define OSD_SECOND_GROUP_START 4
#define OSD_END 7

static inline  int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static struct osd_info_s osd_info = {
	.index = 0,
	.osd_reverse = 0,
};

static struct para_osd_info_s para_osd_info[OSD_END + 2] = {
	/* head */
	{
		"head", OSD_INVALID_INFO,
		OSD_END + 1, 1,
		0, OSD_END + 1
	},
	/* dev */
	{
		"osd0",	DEV_OSD0,
		OSD_FIRST_GROUP_START - 1, OSD_FIRST_GROUP_START + 1,
		OSD_FIRST_GROUP_START, OSD_SECOND_GROUP_START - 1
	},
	{
		"osd1",	DEV_OSD1,
		OSD_FIRST_GROUP_START, OSD_FIRST_GROUP_START + 2,
		OSD_FIRST_GROUP_START, OSD_SECOND_GROUP_START - 1
	},
	{
		"all", DEV_ALL,
		OSD_FIRST_GROUP_START + 1, OSD_FIRST_GROUP_START + 3,
		OSD_FIRST_GROUP_START, OSD_SECOND_GROUP_START - 1
	},
	/* reverse_mode */
	{
		"false", REVERSE_FALSE,
		OSD_SECOND_GROUP_START - 1, OSD_SECOND_GROUP_START + 1,
		OSD_SECOND_GROUP_START, OSD_END
	},
	{
		"true", REVERSE_TRUE,
		OSD_SECOND_GROUP_START, OSD_SECOND_GROUP_START + 2,
		OSD_SECOND_GROUP_START, OSD_END
	},
	{
		"x_rev", REVERSE_X,
		OSD_SECOND_GROUP_START + 1, OSD_SECOND_GROUP_START + 3,
		OSD_SECOND_GROUP_START, OSD_END
	},
	{
		"y_rev", REVERSE_Y,
		OSD_SECOND_GROUP_START + 2, OSD_SECOND_GROUP_START + 4,
		OSD_SECOND_GROUP_START, OSD_END
	},
	{
		"tail", OSD_INVALID_INFO, OSD_END,
		0, 0,
		OSD_END + 1
	},
};

static inline int install_osd_reverse_info(struct osd_info_s *init_osd_info,
					   char *para)
{
	u32 i = 0;
	static u32 tail = OSD_END + 1;
	u32 first = para_osd_info[0].next_idx;

	for (i = first; i < tail; i = para_osd_info[i].next_idx) {
		if (strcmp(para_osd_info[i].name, para) == 0) {
			u32 group_start = para_osd_info[i].cur_group_start;
			u32 group_end = para_osd_info[i].cur_group_end;
			u32	prev = para_osd_info[group_start].prev_idx;
			u32  next = para_osd_info[group_end].next_idx;

			switch (para_osd_info[i].cur_group_start) {
			case OSD_FIRST_GROUP_START:
				init_osd_info->index = para_osd_info[i].info;
				break;
			case OSD_SECOND_GROUP_START:
				init_osd_info->osd_reverse =
					para_osd_info[i].info;
				break;
			}
			para_osd_info[prev].next_idx = next;
			para_osd_info[next].prev_idx = prev;
			return 0;
		}
	}
	return 0;
}

static int drm_logo_reverse_setup(char *str)
{
	char	*ptr = str;
	char	sep[2];
	char	*option;
	int count = 2;
	char find = 0;
	struct osd_info_s *init_osd_info;

	if (!str)
		return -EINVAL;

	init_osd_info = &osd_info;
	memset(init_osd_info, 0, sizeof(struct osd_info_s));
	do {
		if (!isalpha(*ptr) && !isdigit(*ptr)) {
			find = 1;
			break;
		}
	} while (*++ptr != '\0');
	if (!find)
		return -EINVAL;
	sep[0] = *ptr;
	sep[1] = '\0';
	while ((count--) && (option = strsep(&str, sep))) {
		str2lower(option);
		install_osd_reverse_info(init_osd_info, option);
	}
	return 1;
}
__setup("osd_reverse=", drm_logo_reverse_setup);

void drm_logo_get_osd_reverse(u32 *index, u32 *reverse_type)
{
	*index = osd_info.index;
	*reverse_type = osd_info.osd_reverse;
}

#endif

struct para_pair_s {
	char *name;
	int value;
};

#ifdef CONFIG_64BIT
static void free_reserved_highmem(unsigned long start, unsigned long end)
{
}
#else
static void free_reserved_highmem(unsigned long start, unsigned long end)
{
	for (; start < end; ) {
		free_highmem_page(phys_to_page(start));
	start += PAGE_SIZE;
	}
}
#endif

static void free_reserved_mem(unsigned long start, unsigned long size)
{
	unsigned long end = PAGE_ALIGN(start + size);
	struct page *page, *epage;

	pr_info("%s %d logo start_addr=%lx, end=%lx\n", __func__, __LINE__, start, end);
	page = phys_to_page(start);
	if (PageHighMem(page)) {
		free_reserved_highmem(start, end);
	} else {
		epage = phys_to_page(end);
		if (!PageHighMem(epage)) {
			aml_free_reserved_area(__va(start),
					   __va(end), 0, "fb-memory");
		} else {
			/* reserved area cross zone */
			struct zone *zone;
			unsigned long bound;

			zone  = page_zone(page);
			bound = zone_end_pfn(zone);
			aml_free_reserved_area(__va(start),
					   __va(bound << PAGE_SHIFT),
					   0, "fb-memory");
			zone  = page_zone(epage);
			bound = zone->zone_start_pfn;
			free_reserved_highmem(bound << PAGE_SHIFT, end);
		}
	}
}

void am_meson_free_logo_memory(void)
{
	if (is_cma) {
		phys_addr_t logo_addr = page_to_phys(logo[VPP0].logo_page);

		if (logo[VPP0].size > 0 && logo[VPP0].alloc_flag) {
#ifdef CONFIG_CMA
			DRM_INFO("%s, free cma memory: addr:0x%pa,size:0x%x\n",
				 __func__, &logo_addr, logo[VPP0].size);

			cma_release(cma_logo, logo[VPP0].logo_page, logo[VPP0].size >> PAGE_SHIFT);
#endif
		}
	} else {
		free_reserved_mem(logo[VPP0].start, logo[VPP0].size);
		DRM_INFO("%s, free none_cma memory: addr:0x%pa,size:0x%x\n",
				 __func__, &logo[VPP0].start, logo[VPP0].size);
	}

	logo[VPP0].alloc_flag = 0;
	logo[VPP0].is_std = 0;
	logo[VPP0].plane_has_fb = 0;
}

void am_meson_drm_put_logo_fb(struct drm_device *dev,
	int index, int uboot_mode_init)
{
	struct meson_vpu_sub_pipeline *sub_pipe;
	struct meson_drm *private;

	private = dev->dev_private;
	sub_pipe = private->pipeline->subs[index];
	if (sub_pipe->logo_fb && !uboot_mode_init) {
		DRM_INFO("%s, logo_fb[id:%d,ref:%d]\n", __func__,
			sub_pipe->logo_fb->base.base.id,
			kref_read(&sub_pipe->logo_fb->base.base.refcount));
		drm_framebuffer_put(&sub_pipe->logo_fb->base);
		sub_pipe->logo_fb = NULL;
	}
}

static int am_meson_logo_info_update(struct meson_drm *priv)
{
	if (is_cma)
		logo[VPP0].start = page_to_phys(logo[VPP0].logo_page);

	logo[VPP0].alloc_flag = 1;
	/*config 1080p logo as default*/
	if (!logo[VPP0].width || !logo[VPP0].height) {
		logo[VPP0].width = 1920;
		logo[VPP0].height = 1080;
	}
	if (!logo[VPP0].bpp)
		logo[VPP0].bpp = 16;
	if (!logo[VPP0].outputmode_t) {
		strcpy(logo[VPP0].outputmode, "1080p60hz");
	} else {
		strncpy(logo[VPP0].outputmode, logo[VPP0].outputmode_t, VMODE_NAME_LEN_MAX);
		logo[VPP0].outputmode[VMODE_NAME_LEN_MAX - 1] = '\0';
	}
	priv->logo = &logo[VPP0];

	return 0;
}

static int am_meson_logo_init_fb(struct drm_device *dev,
		struct drm_framebuffer *fb, int idx)
{
	struct am_meson_fb *meson_fb;
	struct am_meson_logo *slogo = &logo[idx];
	struct meson_drm *priv = dev->dev_private;

	memcpy(slogo, &logo[VPP0], sizeof(struct am_meson_logo));
	if (idx == VPP0) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		strcpy(slogo->outputmode, get_vout_mode_uboot());
#endif
	} else if (idx == VPP1) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		strcpy(slogo->outputmode, get_vout2_mode_uboot());
#endif
	} else if (idx == VPP2) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		strcpy(slogo->outputmode, get_vout3_mode_uboot());
#endif
	}

	slogo->logo_page = logo[VPP0].logo_page;
	slogo->vaddr = logo[VPP0].vaddr;
	slogo->start = logo[VPP0].start;
	slogo->panel_index = priv->primary_plane_index[idx];
	slogo->vpp_index = idx;

	DRM_INFO("logo%d w=%d,h=%d,start_addr=0x%pa,size=%d\n",
			 idx, slogo->width, slogo->height, &slogo->start, slogo->size);
	DRM_DEBUG("bpp=%d,alloc_flag=%d, osd_reverse=%d\n",
		 slogo->bpp, slogo->alloc_flag, slogo->osd_reverse);
	DRM_DEBUG("outputmode=%s\n", slogo->outputmode);

	meson_fb = to_am_meson_fb(fb);
	meson_fb->logo = slogo;

	if (!strcmp("null", slogo->outputmode) ||
		!strcmp("dummy_l", slogo->outputmode)) {
		DRM_DEBUG("NULL MODE or DUMMY MODE, nothing to do.");
		return -EINVAL;
	}

	return 0;
}

/*copy from update_output_state,
 *TODO:sync with update_output_state
 */
static int am_meson_update_output_state(struct drm_atomic_state *state,
					struct drm_mode_set *set)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	struct drm_connector *connector;
	struct drm_connector_state *new_conn_state;
	int ret, i;

	/* First disable all connectors on the target crtc. */
	ret = drm_atomic_add_affected_connectors(state, set->crtc);
	if (ret)
		return ret;

	for_each_new_connector_in_state(state, connector, new_conn_state, i) {
		if (new_conn_state->crtc == set->crtc) {
			ret = drm_atomic_set_crtc_for_connector(new_conn_state,
								NULL);
			if (ret)
				return ret;

			/* Make sure legacy setCrtc always re-trains */
			new_conn_state->link_status = DRM_LINK_STATUS_GOOD;
		}
	}

	/* Then set all connectors from set->connectors on the target crtc */
	for (i = 0; i < set->num_connectors; i++) {
		new_conn_state =
			drm_atomic_get_connector_state(state,
						       set->connectors[i]);
		if (IS_ERR(new_conn_state))
			return PTR_ERR(new_conn_state);

		ret = drm_atomic_set_crtc_for_connector(new_conn_state,
							set->crtc);
		if (ret)
			return ret;
	}

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		/* Don't update ->enable for the CRTC in the set_config request,
		 * since a mismatch would indicate a bug in the upper layers.
		 * The actual modeset code later on will catch any
		 * inconsistencies here.
		 */
		if (crtc == set->crtc)
			continue;

		if (!new_crtc_state->connector_mask) {
			ret = drm_atomic_set_mode_prop_for_crtc(new_crtc_state,
								NULL);
			if (ret < 0)
				return ret;

			new_crtc_state->active = false;
		}
	}

	return 0;
}

static int _am_meson_occupy_plane_config(struct drm_atomic_state *state,
					struct drm_mode_set *set)
{
	struct drm_crtc *crtc = set->crtc;
	struct meson_drm *private = crtc->dev->dev_private;
	struct am_osd_plane *osd_plane;
	struct drm_plane_state *plane_state;
	int i, hdisplay, vdisplay, ret;

	for (i = 0; i < MESON_MAX_OSD; i++) {
		osd_plane = private->osd_planes[i];
		if (!osd_plane || osd_plane->osd_occupied)
			break;
	}

	if (!osd_plane || !osd_plane->osd_occupied)
		return 0;

	plane_state = drm_atomic_get_plane_state(state, &osd_plane->base);
	if (IS_ERR(plane_state))
		return PTR_ERR(plane_state);

	ret = drm_atomic_set_crtc_for_plane(plane_state, crtc);
	if (ret != 0)
		return ret;

	drm_mode_get_hv_timing(set->mode, &hdisplay, &vdisplay);
	drm_atomic_set_fb_for_plane(plane_state, set->fb);
	plane_state->crtc_x = 0;
	plane_state->crtc_y = 0;
	plane_state->crtc_w = hdisplay;
	plane_state->crtc_h = vdisplay;
	plane_state->src_x = 0;
	plane_state->src_y = 0;
	plane_state->src_w = 1280/*set->fb->width*/ << 16;
	plane_state->src_h = 720/*set->fb->height*/ << 16;
	plane_state->alpha = 1;
	plane_state->zpos = 128;

	return 0;
}

static int am_meson_drm_set_config(struct drm_mode_set *set,
			      struct drm_atomic_state *state, int idx)
{
	struct drm_crtc_state *crtc_state;
	struct drm_plane_state *plane_state;
	struct drm_crtc *crtc = set->crtc;
	struct meson_drm *private = crtc->dev->dev_private;
	struct am_meson_crtc_state *meson_crtc_state;
	struct am_osd_plane *osd_plane;
	struct am_meson_fb *meson_fb;
	int hdisplay, vdisplay, ret;
	unsigned int zpos = OSD_PLANE_BEGIN_ZORDER;

	crtc_state = drm_atomic_get_crtc_state(state, crtc);
	if (IS_ERR(crtc_state))
		return PTR_ERR(crtc_state);

	meson_fb = to_am_meson_fb(set->fb);
	meson_crtc_state = to_am_meson_crtc_state(crtc_state);
	/*logo init without waiting for VBlank.*/
	meson_crtc_state->nonblock_by_vblank = true;
	if (meson_fb->logo->vpp_index == VPP0) {
#ifdef CONFIG_AMLOGIC_VOUT_SERVE
		meson_crtc_state->uboot_mode_init = get_vout_mode_uboot_state();
#endif
	} else if (meson_fb->logo->vpp_index == VPP1) {
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
		meson_crtc_state->uboot_mode_init = get_vout2_mode_uboot_state();
#endif
	} else if (meson_fb->logo->vpp_index == VPP2) {
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
		meson_crtc_state->uboot_mode_init = get_vout3_mode_uboot_state();
#endif
	}
	DRM_DEBUG("uboot_mode_init=%d\n", meson_crtc_state->uboot_mode_init);

	osd_plane = private->osd_planes[idx];
	if (!osd_plane)
		return -EINVAL;

	plane_state = drm_atomic_get_plane_state(state, &osd_plane->base);
	if (IS_ERR(plane_state))
		return PTR_ERR(plane_state);

	if (!set->mode) {
		WARN_ON(set->fb);
		WARN_ON(set->num_connectors);

		ret = drm_atomic_set_mode_for_crtc(crtc_state, NULL);
		if (ret != 0)
			return ret;

		crtc_state->active = false;

		ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
		if (ret != 0)
			return ret;

		drm_atomic_set_fb_for_plane(plane_state, NULL);

		goto commit;
	}

	WARN_ON(!set->fb);
	WARN_ON(!set->num_connectors);

	ret = drm_atomic_set_mode_for_crtc(crtc_state, set->mode);
	if (ret != 0)
		return ret;

	crtc_state->active = true;

	ret = drm_atomic_set_crtc_for_plane(plane_state, crtc);
	if (ret != 0)
		return ret;

	drm_mode_get_hv_timing(set->mode, &hdisplay, &vdisplay);
	drm_atomic_set_fb_for_plane(plane_state, set->fb);
	plane_state->crtc_x = 0;
	plane_state->crtc_y = 0;
	plane_state->crtc_w = hdisplay;
	plane_state->crtc_h = vdisplay;
	plane_state->src_x = set->x << 16;
	plane_state->src_y = set->y << 16;
	plane_state->zpos = zpos + osd_plane->plane_index;

	switch (meson_fb->logo->osd_reverse) {
	case 1:
		plane_state->rotation = DRM_MODE_REFLECT_MASK;
		break;
	case 2:
		plane_state->rotation = DRM_MODE_REFLECT_X;
		break;
	case 3:
		plane_state->rotation = DRM_MODE_REFLECT_Y;
		break;
	default:
		plane_state->rotation = DRM_MODE_ROTATE_0;
		break;
	}

	if (drm_rotation_90_or_270(plane_state->rotation)) {
		if (private->ui_config.ui_h)
			plane_state->src_w = private->ui_config.ui_h << 16;
		else
			plane_state->src_w = set->fb->height << 16;
		if (private->ui_config.ui_w)
			plane_state->src_h = private->ui_config.ui_w << 16;
		else
			plane_state->src_h = set->fb->width << 16;
	} else {
		if (private->ui_config.ui_w)
			plane_state->src_w = private->ui_config.ui_w << 16;
		else
			plane_state->src_w = set->fb->width << 16;
		if (private->ui_config.ui_h)
			plane_state->src_h = private->ui_config.ui_h << 16;
		else
			plane_state->src_h = set->fb->height << 16;
	}

	if (meson_fb->logo->vpp_index == VPP0)
		_am_meson_occupy_plane_config(state, set);

commit:
	ret = am_meson_update_output_state(state, set);
	if (ret)
		return ret;

	return 0;
}

static void am_meson_load_logo(struct drm_device *dev,
	struct drm_framebuffer *fb, struct drm_atomic_state *state,
	struct meson_vpu_sub_pipeline *sub_pipe)
{
	struct drm_display_mode *mode;
	struct drm_connector **connector_set;
	struct drm_connector *connector;
	struct meson_drm *private = dev->dev_private;
	struct am_meson_fb *meson_fb;
	char *connector_type = NULL;
	u32 found, num_modes;
	int ret = 0;
	int idx = sub_pipe->index;

	DRM_DEBUG("idx[%d]\n", idx);

	if (!logo[VPP0].alloc_flag) {
		DRM_INFO("%s: logo memory is not alloc\n", __func__);
		return;
	}

	if (am_meson_logo_init_fb(dev, fb, idx)) {
		DRM_DEBUG("vout%d logo is disabled!\n", idx + 1);
		meson_fb = to_am_meson_fb(fb);
		sub_pipe->logo_fb = meson_fb;
		return;
	}

	switch (idx) {
	case 0:
		connector_type = get_uboot_connector0_type();
		DRM_DEBUG("[%d]: connector_type %s\n", idx, connector_type);
		break;
#ifdef CONFIG_AMLOGIC_VOUT2_SERVE
	case 1:
		connector_type = get_uboot_connector1_type();
		DRM_DEBUG("[%d]: connector_type %s\n", idx, connector_type);
		break;
#endif
#ifdef CONFIG_AMLOGIC_VOUT3_SERVE
	case 2:
		connector_type = get_uboot_connector2_type();
		DRM_DEBUG("[%d]: connector_type %s\n", idx, connector_type);
		break;
#endif
	default:
		DRM_DEBUG("connector_type not found\n");
		break;
	}

	if (connector_type) {
		if (strlen(connector_type) == 0 || strstr(connector_type, "NULL"))
			connector_type = NULL;
	}

	meson_fb = to_am_meson_fb(fb);
	/*init all connector and found matched uboot mode.*/
	found = 0;

	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		/*connector->name is same as connector_type char*/
		if (connector_type && strcmp(connector->name, connector_type)) {
			DRM_DEBUG("[%d]: %s != %s, jump\n", idx,
				connector->name, connector_type);
			continue;
		}

		if (drm_modeset_is_locked(&dev->mode_config.connection_mutex))
			drm_modeset_unlock(&dev->mode_config.connection_mutex);
		num_modes = connector->funcs->fill_modes(connector,
							 dev->mode_config.max_width,
							 dev->mode_config.max_height);
		if (!drm_modeset_is_locked(&dev->mode_config.connection_mutex))
			ret = drm_modeset_lock(&dev->mode_config.connection_mutex,
				state->acquire_ctx);

		if (ret)
			DRM_ERROR("enter, drm_modeset_lock ret %d.\n", ret);

		if (num_modes) {
			list_for_each_entry(mode, &connector->modes, head) {
				if (!strcmp(mode->name, meson_fb->logo->outputmode)) {
					found = 1;
					break;
				}
			}
			if (found)
				break;
		}

		DRM_DEBUG("Connector[%d] status[%d]\n",
			connector->connector_type, connector->status);
	}

	if (found) {
		DRM_INFO("Found Connector[%d] mode[%s]\n",
			connector->connector_type, mode->name);
		if (!strcmp("null", mode->name)) {
			DRM_INFO("NULL MODE, nothing to do.");
			sub_pipe->logo_fb = meson_fb;
			return;
		}
	} else {
		connector = NULL;
		mode = NULL;
		sub_pipe->logo_fb = meson_fb;
		return;
	}

	DRM_DEBUG("mode flag %x\n", mode->flags);

	private->logo_set[idx].num_connectors = NUM_CONNECTORS_LOGO;

	connector_set = kmalloc_array(private->logo_set[idx].num_connectors,
			sizeof(struct drm_connector *), GFP_KERNEL);
	if (!connector_set)
		return;

	connector_set[0] = connector;
	private->logo_set[idx].connectors = connector_set;

	private->logo_set[idx].crtc = &private->crtcs[idx]->base;
	private->logo_set[idx].x = 0;
	private->logo_set[idx].y = 0;
	private->logo_set[idx].mode = mode;
	private->logo_set[idx].crtc->mode = *mode;
	private->logo_set[idx].fb = fb;

	if (am_meson_drm_set_config(&private->logo_set[idx],
		state, meson_fb->logo->panel_index))
		DRM_INFO("[%s]am_meson_drm_set_config fail\n", __func__);

	kfree(connector_set);
}

static int parse_reserve_mem_resource(struct device_node *np,
				       struct resource *res)
{
	int ret;

	ret = of_address_to_resource(np, 0, res);
	of_node_put(np);

	return ret;
}

void am_meson_logo_cma_alloc(struct device *dev, int logo_init)
{
	int ret;
#ifdef CONFIG_CMA
	struct reserved_mem *rmem = NULL;
	struct device_node *np, *mem_node;
#endif
	struct meson_drm *private = dev_get_drvdata(dev);

	np = gp_dev->dev.of_node;
	mem_node = of_parse_phandle(np, "memory-region", 0);

	ret = of_reserved_mem_device_init(&gp_dev->dev);
	if (ret != 0) {
		DRM_ERROR("failed to init reserved memory\n");
	} else {
#ifdef CONFIG_CMA
		rmem = of_reserved_mem_lookup(mem_node);
		of_node_put(mem_node);
		if (rmem) {
			logo[VPP0].size = rmem->size;
			DRM_DEBUG("of read %s reservememsize=0x%x, base %pa\n",
				rmem->name, logo[VPP0].size, &rmem->base);
		}

		cma_logo = dev_get_cma_area(&gp_dev->dev);

		if (cma_logo) {
			if (logo[VPP0].size > 0) {
				logo[VPP0].logo_page = cma_alloc(cma_logo,
						ALIGN(logo[VPP0].size, PAGE_SIZE) >> PAGE_SHIFT,
						0, GFP_KERNEL);

				if (!logo[VPP0].logo_page)
					DRM_ERROR("allocate buffer failed\n");
				else if (logo_init)
					am_meson_logo_info_update(private);

				DRM_INFO(" cma_alloc from %s start page %px-%px size %x\n",
					cma_get_name(cma_logo),
					logo[VPP0].logo_page,
					(void *)logo[VPP0].start,
					logo[VPP0].size);
			}
		}
#endif
		if (gem_mem_start && logo_init) {
			dma_declare_coherent_memory(dev,
							gem_mem_start,
							gem_mem_start,
							gem_mem_size);
			DRM_INFO("meson drm mem_start = 0x%x, size = 0x%x\n",
				(u32)gem_mem_start, (u32)gem_mem_size);
		}
	}
}

void am_meson_logo_cma_mem_reset_zero(struct am_meson_logo *logo)
{
	phys_addr_t logo_addr;
	void *vir_addr;
	bool bflg = false;

	if (logo->logo_page) {
		logo_addr = page_to_phys(logo->logo_page);
		vir_addr = am_meson_drm_vmap(logo_addr,
			logo->size, &bflg);
		memset(vir_addr, 0, logo->size);
		logo->alloc_flag = 1;
	} else {
		DRM_ERROR("reallocate logo cma buffer failed\n");
	}
}

void am_meson_logo_init(struct drm_device *dev)
{
	struct drm_mode_fb_cmd2 mode_cmd;
	struct drm_framebuffer *fb;
	struct meson_drm *private = dev->dev_private;
	struct meson_vpu_sub_pipeline *sub_pipe;
	struct platform_device *pdev = to_platform_device(private->dev);
	struct drm_atomic_state *state;
#ifdef CONFIG_CMA
	struct device_node *np, *mem_node;
#endif
	u32 reverse_type, osd_index;
	int i, ret;
	const char *compatible;
	const int *reusable;

	DRM_DEBUG("in[%d]\n", __LINE__);

	gp_dev = pdev;
	np = gp_dev->dev.of_node;
	mem_node = of_parse_phandle(np, "memory-region", 0);
	compatible = of_get_property(mem_node, "compatible", NULL);
	reusable = of_get_property(mem_node, "reusable", NULL);

	if (!strcmp(compatible, "shared-dma-pool") && reusable)
		is_cma = true;
	else
		is_cma = false;

	if (!mem_node || !of_device_is_available(mem_node)) {
		DRM_INFO("mem region is disabled, skip allocation!\n");
	}  else if (is_cma) {
		am_meson_logo_cma_alloc(dev->dev, 1);
	} else {
		ret = parse_reserve_mem_resource(mem_node, &osd_mem_res);

		if (ret != 0) {
			DRM_ERROR("failed to init none_cma memory\n");
		} else {
			logo[VPP0].size = resource_size(&osd_mem_res);
			logo[VPP0].start = osd_mem_res.start;
		}

		if (logo[VPP0].size == 0) {
			DRM_ERROR("logo size 0, error!\n");
		} else {
			logo[VPP0].vaddr = memremap(logo[VPP0].start, logo[VPP0].size,
					MEMREMAP_WB);

			if (!logo[VPP0].vaddr)
				DRM_ERROR("allocate buffer failed\n");
			else
				am_meson_logo_info_update(private);
		}

	}

#ifdef CONFIG_AMLOGIC_MEDIA_FB
	get_logo_osd_reverse(&osd_index, &reverse_type);
	logo[VPP0].osd_reverse = reverse_type;
	logo[VPP0].width = get_logo_fb_width();
	logo[VPP0].height = get_logo_fb_height();
	logo[VPP0].bpp = get_logo_display_bpp();
#else
	drm_logo_get_osd_reverse(&osd_index, &reverse_type);
	logo[VPP0].osd_reverse = reverse_type;
	logo[VPP0].width = drm_logo_get_fb_width();
	logo[VPP0].height = drm_logo_get_fb_height();
	logo[VPP0].bpp = drm_logo_get_display_bpp();
#endif
	if (!logo[VPP0].bpp)
		logo[VPP0].bpp = 16;

	if (logo[VPP0].bpp == 16)
		mode_cmd.pixel_format = DRM_FORMAT_RGB565;
	else if (logo[VPP0].bpp == 24)
		mode_cmd.pixel_format = DRM_FORMAT_RGB888;
	else
		mode_cmd.pixel_format = DRM_FORMAT_XRGB8888;
	logo[VPP0].is_cma = is_cma;

	mode_cmd.offsets[0] = 0;
	mode_cmd.width = logo[VPP0].width;
	mode_cmd.height = logo[VPP0].height;
	mode_cmd.modifier[0] = DRM_FORMAT_MOD_LINEAR;
	mode_cmd.pitches[0] = ALIGN(mode_cmd.width * logo[VPP0].bpp, 32) / 8;
	fb = am_meson_fb_alloc(dev, &mode_cmd, NULL);
	if (IS_ERR_OR_NULL(fb)) {
		DRM_ERROR("drm fb allocate failed\n");
		return;
	}

	if (strmode && !strcmp("4", strmode)) {
		DRM_INFO("current is strmode\n");
	} else {
		drm_modeset_lock_all(dev);
		state = drm_atomic_state_alloc(dev);
		if (!state) {
			drm_modeset_unlock_all(dev);
			drm_framebuffer_put(fb);
			DRM_ERROR("drm atomic state allocate failed\n");
			return;
		}
		state->acquire_ctx = dev->mode_config.acquire_ctx;

		for (i = 0; i < private->num_crtcs; i++) {
			sub_pipe = private->pipeline->subs[i];
			am_meson_load_logo(dev, fb, state, sub_pipe);
			if (sub_pipe->logo_fb) {
				drm_framebuffer_get(&sub_pipe->logo_fb->base);
				DRM_INFO("logo_fb[id:%d,ref:%d]\n",
					sub_pipe->logo_fb->base.base.id,
					kref_read(&sub_pipe->logo_fb->base.base.refcount));
			}
		}

		ret = drm_atomic_commit(state);
		if (ret != 0) {
			drm_atomic_state_put(state);
			DRM_ERROR("failed to commit state\n");
			drm_modeset_unlock_all(dev);
			drm_framebuffer_put(fb);
			return;
		}
		drm_atomic_state_put(state);
		drm_modeset_unlock_all(dev);
	}

	if (drm_framebuffer_read_refcount(fb) > 1)
		drm_framebuffer_put(fb);

	DRM_DEBUG("drm_fb[id:%d,ref:%d]\n", fb->base.id, kref_read(&fb->base.refcount));
	DRM_DEBUG("end[%d]\n", __LINE__);
}

static int gem_mem_device_init(struct reserved_mem *rmem, struct device *dev)
{
	s32 ret = 0;

	if (!rmem) {
		pr_info("Can't get reverse mem!\n");
		ret = -EFAULT;
		return ret;
	}
	gem_mem_start = rmem->base;
	gem_mem_size = rmem->size;
	DRM_INFO("init gem memory source addr:0x%x size:0x%x\n",
		(u32)gem_mem_start, (u32)gem_mem_size);

	return 0;
}

static const struct reserved_mem_ops rmem_gem_ops = {
	.device_init = gem_mem_device_init,
};

static int __init gem_mem_setup(struct reserved_mem *rmem)
{
	rmem->ops = &rmem_gem_ops;
	DRM_INFO("gem mem setup\n");
	return 0;
}

RESERVEDMEM_OF_DECLARE(gem, "amlogic, gem_memory", gem_mem_setup);

