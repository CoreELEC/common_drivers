// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/dma-fence.h>
#include <linux/uaccess.h>
#include <linux/sync_file.h>
#include <linux/file.h>
#include <linux/types.h>

#include <drm/drmP.h>
#include <drm/drm_atomic.h>
#include <drm/drm_mode.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_property.h>
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
#define KERNEL_ATRACE_TAG KERNEL_ATRACE_TAG_DRM
#include <trace/events/meson_atrace.h>
#endif

#include "meson_drv.h"
#include "meson_async_atomic.h"
#include "meson_plane.h"
#include "meson_crtc.h"

struct drm_property *meson_mode_obj_find_prop_id(struct drm_mode_object *obj,
					       uint32_t prop_id)
{
	int i;

	for (i = 0; i < obj->properties->count; i++)
		if (obj->properties->properties[i]->base.id == prop_id)
			return obj->properties->properties[i];

	return NULL;
}

static bool meson_property_change_valid_get(struct drm_property *property,
				   u64 value, struct drm_mode_object **ref)
{
	int i;

	if (property->flags & DRM_MODE_PROP_IMMUTABLE)
		return false;

	*ref = NULL;

	if (drm_property_type_is(property, DRM_MODE_PROP_RANGE)) {
		if (value < property->values[0] || value > property->values[1])
			return false;
		return true;
	} else if (drm_property_type_is(property, DRM_MODE_PROP_SIGNED_RANGE)) {
		s64 svalue = U642I64(value);

		if (svalue < U642I64(property->values[0]) ||
				svalue > U642I64(property->values[1]))
			return false;
		return true;
	} else if (drm_property_type_is(property, DRM_MODE_PROP_BITMASK)) {
		u64 valid_mask = 0;

		for (i = 0; i < property->num_values; i++)
			valid_mask |= (1ULL << property->values[i]);
		return !(value & ~valid_mask);
	} else if (drm_property_type_is(property, DRM_MODE_PROP_BLOB)) {
		struct drm_property_blob *blob;

		if (value == 0)
			return true;

		blob = drm_property_lookup_blob(property->dev, value);
		if (blob) {
			*ref = &blob->base;
			return true;
		} else {
			return false;
		}
	} else if (drm_property_type_is(property, DRM_MODE_PROP_OBJECT)) {
		/* a zero value for an object property translates to null: */
		if (value == 0)
			return true;

		*ref = drm_mode_object_find(property->dev, NULL, value,
					      property->values[0]);
		if (*ref)
			return true;
		else
			return false;
	}

	for (i = 0; i < property->num_values; i++)
		if (property->values[i] == value)
			return true;
	return false;
}

static void meson_property_change_valid_put(struct drm_property *property,
		struct drm_mode_object *ref)
{
	if (!ref)
		return;

	if (drm_property_type_is(property, DRM_MODE_PROP_OBJECT))
		drm_mode_object_put(ref);
	else if (drm_property_type_is(property, DRM_MODE_PROP_BLOB))
		drm_property_blob_put(obj_to_blob(ref));
}

bool is_am_osd_async_commit(struct drm_atomic_state *atomic_state)
{
	struct drm_plane *plane = NULL;
	struct drm_plane_state *new_plane_state = NULL;
	int i;

	if (!atomic_state->async_update)
		return false;

	for_each_new_plane_in_state(atomic_state, plane, new_plane_state, i) {
		if (!plane || strncmp(plane->name, "osd", 3))
			return false;
	}

	return true;
}

static void meson_video_bypass_vblank_wait(struct drm_atomic_state *state)
{
	struct am_meson_crtc_state *meson_crtc_state;
	struct drm_crtc_state *crtc_state;
	struct drm_crtc *crtc;
	int i;
	struct drm_plane *plane = NULL;
	struct drm_plane_state *new_plane_state = NULL;

	for_each_new_plane_in_state(state, plane,  new_plane_state, i) {
		if (!plane || strncmp(plane->name, "vid", 3))
			return;
	}

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		meson_crtc_state = to_am_meson_crtc_state(crtc_state);
		meson_crtc_state->nonblock_by_vblank = true;
		DRM_DEBUG("bypass vblank wait %d\n",
			  state->async_update);
	}
}

/**
 * Most of the implementation is the same as drm_atomic_get_crtc_state,
 * but this function removes the call to drm_modeset_lock.
 * It was added for use by meson_drm_atomic_get_plane_state.
 */
static struct drm_crtc_state *
meson_drm_atomic_get_crtc_state(struct drm_atomic_state *state,
			  struct drm_crtc *crtc)
{
	int ret;
	int index = drm_crtc_index(crtc);
	struct drm_crtc_state *crtc_state;

	WARN_ON(!state->acquire_ctx);

	crtc_state = drm_atomic_get_existing_crtc_state(state, crtc);
	if (crtc_state)
		return crtc_state;

	if (!state->async_update || is_am_osd_async_commit(state)) {
		ret = drm_modeset_lock(&crtc->mutex, state->acquire_ctx);
		if (ret)
			return ERR_PTR(ret);
	}

	crtc_state = crtc->funcs->atomic_duplicate_state(crtc);
	if (!crtc_state)
		return ERR_PTR(-ENOMEM);

	state->crtcs[index].state = crtc_state;
	state->crtcs[index].old_state = crtc->state;
	state->crtcs[index].new_state = crtc_state;
	state->crtcs[index].ptr = crtc;
	crtc_state->state = state;

	drm_dbg_atomic(state->dev, "Added [CRTC:%d:%s] %p state to %p\n",
		       crtc->base.id, crtc->name, crtc_state, state);

	return crtc_state;
}

/**
 * This function shares most of its implementation with drm_atomic_get_plane_state.
 * This function is added to support calls to meson_drm_atomic_get_crtc_state.
 */
static struct drm_plane_state *
meson_drm_atomic_get_plane_state(struct drm_atomic_state *state,
			  struct drm_plane *plane)
{
	int ret, index = drm_plane_index(plane);
	struct drm_plane_state *plane_state;

	WARN_ON(!state->acquire_ctx);

	/* the legacy pointers should never be set */
	WARN_ON(plane->fb);
	WARN_ON(plane->old_fb);
	WARN_ON(plane->crtc);

	plane_state = drm_atomic_get_existing_plane_state(state, plane);
	if (plane_state)
		return plane_state;

	ret = drm_modeset_lock(&plane->mutex, state->acquire_ctx);
	if (ret)
		return ERR_PTR(ret);

	plane_state = plane->funcs->atomic_duplicate_state(plane);
	if (!plane_state)
		return ERR_PTR(-ENOMEM);

	state->planes[index].state = plane_state;
	state->planes[index].ptr = plane;
	state->planes[index].old_state = plane->state;
	state->planes[index].new_state = plane_state;
	plane_state->state = state;

	drm_dbg_atomic(plane->dev, "Added [PLANE:%d:%s] %p state to %p\n",
		       plane->base.id, plane->name, plane_state, state);

	if (plane_state->crtc) {
		struct drm_crtc_state *crtc_state;

		crtc_state = meson_drm_atomic_get_crtc_state(state,
						       plane_state->crtc);
		if (IS_ERR(crtc_state))
			return ERR_CAST(crtc_state);
	}

	return plane_state;
}

static int atomic_set_prop(struct drm_atomic_state *state,
		struct drm_mode_object *obj, struct drm_file *file_priv,
		struct drm_property *prop, u64 prop_value)
{
	struct drm_mode_object *ref;
	int ret;

	if (!meson_property_change_valid_get(prop, prop_value, &ref))
		return -EINVAL;

	switch (obj->type) {
	case DRM_MODE_OBJECT_PLANE: {
		struct drm_plane *plane = obj_to_plane(obj);
		struct drm_plane_state *plane_state;

		plane_state = meson_drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			ret = PTR_ERR(plane_state);
			break;
		}

		ret = meson_async_atomic_plane_set_property(plane,
				plane_state, file_priv, prop, prop_value);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	meson_property_change_valid_put(prop, ref);
	return ret;
}

struct drm_pending_vblank_event *meson_create_vblank_event
	(struct drm_crtc *crtc, uint64_t user_data)
{
	struct drm_pending_vblank_event *e = NULL;

	e = kzalloc(sizeof(*e), GFP_KERNEL);
	if (!e)
		return NULL;

	e->event.base.type = DRM_EVENT_FLIP_COMPLETE;
	e->event.base.length = sizeof(e->event);
	e->event.vbl.crtc_id = crtc->base.id;
	e->event.vbl.user_data = user_data;

	return e;
}

static int meson_page_flip_prepare(struct drm_device *dev,
	struct drm_atomic_state *state,
	struct drm_mode_atomic *arg, struct drm_file *file_priv)
{
	struct drm_crtc *crtc;
	struct drm_pending_vblank_event *e;
	struct drm_crtc_state *crtc_state;
	int i, c = 0, ret;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (arg->flags & DRM_MODE_PAGE_FLIP_EVENT) {
			e = meson_create_vblank_event(crtc, arg->user_data);
			DRM_DEBUG_VBL("create event, e=%p, user_data=%llx\n", e, arg->user_data);
			if (!e) {
				DRM_ERROR("meson_create_vblank_event fail.\n");
				return -ENOMEM;
			}

			crtc_state->event = e;
			if (!file_priv) {
				DRM_INFO("file_priv is null\n");
				continue;
			}

			ret = drm_event_reserve_init(dev, file_priv, &e->base,
							 &e->event.base);
			if (ret) {
				DRM_ERROR("drm_event_reserve_init fail. len=%d, ret=%d!\n",
					e->event.base.length, ret);
				kfree(e);
				crtc_state->event = NULL;
				return ret;
			}
		}
		c++;
	}

	if (c == 0 && (arg->flags & DRM_MODE_PAGE_FLIP_EVENT)) {
		DRM_ERROR("para invalid.\n");
		return -EINVAL;
	}

	return 0;
}

static void meson_page_flip_signaling(struct drm_device *dev,
			       struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int i;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		struct drm_pending_vblank_event *event = crtc_state->event;
		/*
		 * Free the allocated event. drm_atomic_helper_setup_commit
		 * can allocate an event too, so only free it if it's ours
		 * to prevent a double free in drm_atomic_state_clear.
		 */
		if (event && (event->base.fence || event->base.file_priv)) {
			drm_event_cancel_free(dev, &event->base);
			crtc_state->event = NULL;
		}
	}
}

/**
 * same as drm_atomic_helper_check_plane_damage
 */
static void meson_atomic_helper_check_plane_damage(struct drm_atomic_state *state,
					  struct drm_plane_state *plane_state)
{
	struct drm_crtc_state *crtc_state;

	if (plane_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state,
							   plane_state->crtc);

		if (WARN_ON(!crtc_state))
			return;

		if (drm_atomic_crtc_needs_modeset(crtc_state)) {
			drm_property_blob_put(plane_state->fb_damage_clips);
			plane_state->fb_damage_clips = NULL;
		}
	}
}

/**
 * This function is same with drm_atomic_helper_plane_changed
 * drm_atomic_helper_plane_changed is static function
 */
static void
meson_atomic_helper_plane_changed(struct drm_atomic_state *state,
				struct drm_plane_state *old_plane_state,
				struct drm_plane_state *plane_state,
				struct drm_plane *plane)
{
	struct drm_crtc_state *crtc_state;

	if (old_plane_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state,
							   old_plane_state->crtc);

		if (WARN_ON(!crtc_state))
			return;

		crtc_state->planes_changed = true;
	}

	if (plane_state->crtc) {
		crtc_state = drm_atomic_get_new_crtc_state(state, plane_state->crtc);

		if (WARN_ON(!crtc_state))
			return;

		crtc_state->planes_changed = true;
	}
}

/**
 * drm_atomic_helper_check includes:
 * - drm_atomic_helper_check_modeset
 * - drm_atomic_helper_check_planes
 * - drm_atomic_helper_async_check
 *
 * In async_commit, drm_atomic_helper_check_modeset is skipped.
 * Instead, check_crtcs is called after async_check.
 */
static int
meson_atomic_helper_check_crtcs(struct drm_device *dev,
			       struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_crtc_state;
	int i, ret = 0;

	for_each_new_crtc_in_state(state, crtc, new_crtc_state, i) {
		const struct drm_crtc_helper_funcs *funcs;

		funcs = crtc->helper_private;

		if (!funcs || !funcs->atomic_check)
			continue;

		ret = funcs->atomic_check(crtc, state);
		if (ret) {
			drm_dbg_atomic(crtc->dev,
				       "[CRTC:%d:%s] atomic driver check failed\n",
				       crtc->base.id, crtc->name);
			return ret;
		}
	}

	return ret;
}

/**
 * The drm_atomic_helper_check_planes function performs both plane
 * and CRTC checks. In this implementation, the checks are split into
 * separate functions: meson_atomic_helper_check_planes and
 * meson_atomic_helper_check_crtcs。
 */
static int
meson_atomic_helper_check_planes(struct drm_device *dev,
			       struct drm_atomic_state *state)
{
	struct drm_plane *plane;
	struct drm_plane_state *new_plane_state, *old_plane_state;
	int i, ret = 0;

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		const struct drm_plane_helper_funcs *funcs;

		WARN_ON(!drm_modeset_is_locked(&plane->mutex));

		funcs = plane->helper_private;

		if (!old_plane_state->crtc && !new_plane_state->crtc) {
			drm_dbg_atomic(plane->dev, "[PLANE:%d:%s] already disabled\n",
					plane->base.id, plane->name);
			return -EINVAL;
		}

		meson_atomic_helper_plane_changed(state, old_plane_state, new_plane_state, plane);

		meson_atomic_helper_check_plane_damage(state, new_plane_state);

		if (!funcs || !funcs->atomic_check)
			continue;

		ret = funcs->atomic_check(plane, state);
		if (ret) {
			DRM_ERROR("[PLANE:%d:%s] atomic driver check planes failed, ret=%d!\n",
				plane->base.id, plane->name, ret);
			return ret;
		}
	}

	return ret;
}

/**
 * Same as drm_atomic_helper_async_check for now,
 * but will be refined/tuned later.
 */
static int meson_atomic_helper_async_check(struct drm_device *dev,
				   struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_plane *plane = NULL;
	struct drm_plane_state *old_plane_state = NULL;
	struct drm_plane_state *new_plane_state = NULL;
	const struct drm_plane_helper_funcs *funcs;
	int i, ret, n_planes = 0;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (drm_atomic_crtc_needs_modeset(crtc_state))
			return -EINVAL;
	}

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i)
		n_planes++;

	/* FIXME: we support only single plane updates for now */
	if (n_planes != 1) {
		drm_dbg_atomic(dev,
			       "only single plane async updates are supported\n");
		return -EINVAL;
	}

	if (!new_plane_state->crtc ||
	    old_plane_state->crtc != new_plane_state->crtc) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] async update cannot change CRTC\n",
			       plane->base.id, plane->name);
		return -EINVAL;
	}

	funcs = plane->helper_private;
	if (!funcs->atomic_async_update) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] driver does not support async updates\n",
			       plane->base.id, plane->name);
		return -EINVAL;
	}

	if (new_plane_state->fence) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] missing fence for async update\n",
			       plane->base.id, plane->name);
		return -EINVAL;
	}

	/*
	 * Don't do an async update if there is an outstanding commit modifying
	 * the plane.  This prevents our async update's changes from getting
	 * overridden by a previous synchronous update's state.
	 */
	if (old_plane_state->commit &&
	    !try_wait_for_completion(&old_plane_state->commit->hw_done)) {
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] inflight previous commit preventing async commit\n",
			       plane->base.id, plane->name);
		return -EBUSY;
	}

	ret = funcs->atomic_async_check(plane, state);
	if (ret != 0)
		drm_dbg_atomic(dev,
			       "[PLANE:%d:%s] driver async check failed\n",
			       plane->base.id, plane->name);
	return ret;
}

static int meson_atomic_helper_multi_async_check(struct drm_device *dev,
				   struct drm_atomic_state *state)
{
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	struct drm_plane *plane = NULL;
	struct drm_plane_state *old_plane_state = NULL;
	struct drm_plane_state *new_plane_state = NULL;
	const struct drm_plane_helper_funcs *funcs;
	int i, ret = 0;

	for_each_new_crtc_in_state(state, crtc, crtc_state, i) {
		if (drm_atomic_crtc_needs_modeset(crtc_state))
			return -EINVAL;
	}

	for_each_oldnew_plane_in_state(state, plane, old_plane_state, new_plane_state, i) {
		if (!new_plane_state->crtc ||
			old_plane_state->crtc != new_plane_state->crtc) {
			drm_dbg_atomic(dev,
					"[PLANE:%d:%s] async update cannot change CRTC\n",
					plane->base.id, plane->name);
			return -EINVAL;
		}

		funcs = plane->helper_private;
		if (!funcs->atomic_async_update) {
			drm_dbg_atomic(dev,
					"[PLANE:%d:%s] driver does not support async updates\n",
					plane->base.id, plane->name);
			return -EINVAL;
		}

		if (new_plane_state->fence) {
			drm_dbg_atomic(dev,
					"[PLANE:%d:%s] missing fence for async update\n",
					plane->base.id, plane->name);
			return -EINVAL;
		}

		/*
		 * Don't do an async update if there is an outstanding commit modifying
		 * the plane.  This prevents our async update's changes from getting
		 * overridden by a previous synchronous update's state.
		 */

		if (old_plane_state->commit &&
			!try_wait_for_completion(&old_plane_state->commit->hw_done)) {
			drm_dbg_atomic(dev,
					"[PLANE:%d:%s] inflight previous commit preventing async commit\n",
					plane->base.id, plane->name);
			return -EBUSY;
		}

		ret = funcs->atomic_async_check(plane, state);
		if (ret != 0)
			drm_dbg_atomic(dev,
					"[PLANE:%d:%s] driver async check failed\n",
					plane->base.id, plane->name);
	}

	return ret;
}

/*modified from drm_mode_atomic_ioctl() */
int meson_async_atomic_ioctl(struct drm_device *dev,
			  void *data, struct drm_file *file_priv)
{
	struct drm_mode_atomic *arg = data;
	u32 __user *objs_ptr = (u32 __user *)(unsigned long)(arg->objs_ptr);
	u32 __user *count_props_ptr = (u32 __user *)(unsigned long)(arg->count_props_ptr);
	u32 __user *props_ptr = (u32 __user *)(unsigned long)(arg->props_ptr);
	u64 __user *prop_values_ptr = (u64 __user *)(unsigned long)(arg->prop_values_ptr);
	unsigned int copied_objs, copied_props;
	struct drm_atomic_state *state;
	struct drm_modeset_acquire_ctx ctx;
	struct drm_out_fence_state *fence_state;
	struct drm_mode_config *config = &dev->mode_config;
	int ret = 0;
	unsigned int i, j, num_fences;
	ktime_t start1, end1, start2, end2;
	s64 commit_time_ms = 0, total_time_ms;

#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
	ATRACE_BEGIN("async_atomic_ioctl");
#endif
	start1 = ktime_get();

	DRM_DEBUG_ATOMIC("IN, pid[%s %d]\n", current->comm, current->pid);

	drm_modeset_acquire_init(&ctx, 0);

	state = drm_atomic_state_alloc(dev);
	if (!state) {
		DRM_ERROR("state is NULL\n");
		return -ENOMEM;
	}

	state->allow_modeset = false;
	state->acquire_ctx = &ctx;

retry:
	copied_objs = 0;
	copied_props = 0;
	fence_state = NULL;
	num_fences = 0;

	if (arg->count_objs > 1 && !(arg->flags & MESON_MODE_MULTI_ATOMIC)) {
		DRM_ERROR("only accept plane object update(%d).\n", arg->count_objs);
		ret = -EINVAL;
		goto out;
	}

	for (i = 0; i < arg->count_objs; i++) {
		u32 obj_id, count_props;
		struct drm_mode_object *obj;

		if (get_user(obj_id, objs_ptr + copied_objs)) {
			DRM_ERROR("get_user obj_id fail\n");
			ret = -EFAULT;
			goto out;
		}

		obj = drm_mode_object_find(dev, file_priv, obj_id, DRM_MODE_OBJECT_ANY);
		if (!obj) {
			DRM_ERROR("obj is NULL\n");
			ret = -ENOENT;
			goto out;
		}

		if (obj->type != DRM_MODE_OBJECT_PLANE) {
			DRM_ERROR("only accept plane object update, can not set (%d)\n", obj->type);
			ret = -EINVAL;
			goto out;
		}

		if (!obj->properties) {
			DRM_ERROR("properties is NULL\n");
			drm_mode_object_put(obj);
			ret = -ENOENT;
			goto out;
		}

		if (get_user(count_props, count_props_ptr + copied_objs)) {
			DRM_ERROR("get_user count_props fail\n");
			drm_mode_object_put(obj);
			ret = -EFAULT;
			goto out;
		}

		copied_objs++;

		for (j = 0; j < count_props; j++) {
			u32 prop_id;
			u64 prop_value;
			struct drm_property *prop;

			if (get_user(prop_id, props_ptr + copied_props)) {
				DRM_ERROR("get_user prop_id fail\n");
				drm_mode_object_put(obj);
				ret = -EFAULT;
				goto out;
			}

			prop = meson_mode_obj_find_prop_id(obj, prop_id);
			if (!prop) {
				DRM_ERROR("prop is NULL\n");
				drm_mode_object_put(obj);
				ret = -ENOENT;
				goto out;
			}

			if (copy_from_user(&prop_value,
					   prop_values_ptr + copied_props,
					   sizeof(prop_value))) {
				DRM_ERROR("copy_from_user get prop_value fail\n");
				drm_mode_object_put(obj);
				ret = -EFAULT;
				goto out;
			}

			ret = atomic_set_prop(state, obj, file_priv, prop, prop_value);
			if (ret) {
				DRM_ERROR("set prop fail\n");
				drm_mode_object_put(obj);
				goto out;
			}

			copied_props++;
		}

		drm_mode_object_put(obj);
	}

	state->legacy_cursor_update = true;
	ret = meson_atomic_helper_check_planes(dev, state);
	if (ret)
		goto out;

	if (arg->flags & MESON_MODE_MULTI_ATOMIC)
		state->async_update = !meson_atomic_helper_multi_async_check(dev, state);
	else
		state->async_update = !meson_atomic_helper_async_check(dev, state);

	ret = meson_atomic_helper_check_crtcs(dev, state);
	if (ret)
		goto out;

	meson_video_bypass_vblank_wait(state);

	DRM_DEBUG_ATOMIC("%s, async_update-%d\n", __func__, state->async_update);

	ret = meson_page_flip_prepare(dev, state, arg, file_priv);
	if (ret)
		goto out;

	start2 = ktime_get();
	ret = config->funcs->atomic_commit(state->dev, state, false);
	end2 = ktime_get();
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

out:
	meson_page_flip_signaling(dev, state);
	if (ret == -EDEADLK) {
		drm_atomic_state_clear(state);
		drm_modeset_backoff(&ctx);
		goto retry;
	}

	drm_atomic_state_put(state);

	drm_modeset_drop_locks(&ctx);
	drm_modeset_acquire_fini(&ctx);

	end1 = ktime_get();
	total_time_ms = ktime_ms_delta(end1, start1);
#if IS_ENABLED(CONFIG_AMLOGIC_DEBUG_ATRACE)
	ATRACE_END();
	ATRACE_COUNTER("async_atomic_ioctl", total_time_ms);
#endif
	if (total_time_ms > am_drm_param.async_commit_ioctl_threshold)
		DRM_WARN("async commit cost (%lld %lld) ms, it is too long\n",
			commit_time_ms, total_time_ms);

	DRM_DEBUG_ATOMIC("OUT,  ret = %d\n", ret);
	return ret;
}
