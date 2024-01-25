// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <drm/drm_modeset_helper.h>
#include <drm/drmP.h>
#include <drm/drm_edid.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_connector.h>
#include <drm/drm_modeset_lock.h>

#include <linux/component.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#include <linux/miscdevice.h>

#ifdef CONFIG_DEBUG_FS
#include <linux/debugfs.h>
#include <drm/drm_debugfs.h>
#endif

#include <drm/amlogic/meson_connector_dev.h>
#include <vout/vout_serve/vout_func.h>
#include <enhancement/amvecm/amcsc.h>
#include <drm/display/drm_hdcp_helper.h>

#include "meson_hdmi.h"
#include "meson_vpu.h"
#include "meson_crtc.h"

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif

#define HDMITX_ATTR_LEN_MAX	16
#define HDMITX_MAX_BPC	12

/* confirm whether the current mode is integer mode or frac mode */
static bool meson_hdmitx_is_alter_mode(struct drm_display_mode *mode);

/*for hw limitation, limit to 1080p/720p for recovery ui.*/
static bool hdmitx_set_smaller_pref = true;

/*TODO:will remove later.*/
static struct drm_display_mode dummy_mode = {
	.name = "dummy_l",
	.type = DRM_MODE_TYPE_USERDEF,
	.status = MODE_OK,
	.clock = 148500,
	.hdisplay = 1920,
	.hsync_start = 1930,
	.hsync_end = 1970,
	.htotal = 2200,
	.hskew = 0,
	.vdisplay = 1080,
	.vsync_start = 1090,
	.vsync_end = 1100,
	.vtotal = 1125,
	.vscan = 0,
	.flags =  DRM_MODE_FLAG_NHSYNC | DRM_MODE_FLAG_NVSYNC,
};

struct hdmitx_color_attr dv_color_attr_list[] = {
	{HDMI_COLORSPACE_YUV444, 8}, //"444,8bit"
	{HDMI_COLORSPACE_RESERVED6, COLORDEPTH_RESERVED}
};

struct hdmitx_color_attr dv_ll_color_attr_list[] = {
	{HDMI_COLORSPACE_YUV422, 12}, //"422,12bit"
	{HDMI_COLORSPACE_RESERVED6, COLORDEPTH_RESERVED}
};

/* this is prior selected list of
 * 4k2k50hz, 4k2k60hz smpte50hz, smpte60hz
 */
struct hdmitx_color_attr color_attr_list[] = {
	{HDMI_COLORSPACE_YUV420, 10}, //"420,10bit"
	{HDMI_COLORSPACE_YUV422, 12}, //"422,12bit"
	{HDMI_COLORSPACE_YUV420, 8}, //"420,8bit"
	{HDMI_COLORSPACE_YUV444, 8}, //"444,8bit"
	{HDMI_COLORSPACE_RGB, 8}, //"rgb,8bit"
	{HDMI_COLORSPACE_RESERVED6, COLORDEPTH_RESERVED}
};

/* this is prior selected list of other display mode */
struct hdmitx_color_attr other_color_attr_list[] = {
	{HDMI_COLORSPACE_YUV444, 10}, //"444,10bit"
	{HDMI_COLORSPACE_YUV422, 12}, //"422,12bit"
	{HDMI_COLORSPACE_RGB, 10}, //"rgb,10bit"
	{HDMI_COLORSPACE_YUV444, 8}, //"444,8bit"
	{HDMI_COLORSPACE_RGB, 8}, //"rgb,8bit"
	{HDMI_COLORSPACE_RESERVED6, COLORDEPTH_RESERVED}
};

#define MODE_4K2K24HZ                   "2160p24hz"
#define MODE_4K2K25HZ                   "2160p25hz"
#define MODE_4K2K30HZ                   "2160p30hz"
#define MODE_4K2K50HZ                   "2160p50hz"
#define MODE_4K2K60HZ                   "2160p60hz"
#define MODE_4K2KSMPTE                  "smpte24hz"
#define MODE_4K2KSMPTE30HZ              "smpte30hz"
#define MODE_4K2KSMPTE50HZ              "smpte50hz"
#define MODE_4K2KSMPTE60HZ              "smpte60hz"

struct hdmitx_common *meson_get_hdmitx_common(struct drm_connector *connector)
{
	struct am_hdmi_tx *am_hdmi = NULL;

	am_hdmi = connector_to_am_hdmi(connector);
	if (!am_hdmi || !(am_hdmi->hdmitx_dev))
		return NULL;

	return to_hdmitx_common(am_hdmi->hdmitx_dev);
}

static enum hdmi_color_depth bitdepth_to_colordepth(int bitdepth)
{
	enum hdmi_color_depth color_depth;

	switch (bitdepth) {
	case 8:
		color_depth = COLORDEPTH_24B;
		break;
	case 10:
		color_depth = COLORDEPTH_30B;
		break;
	case 12:
		color_depth = COLORDEPTH_36B;
		break;
	case 16:
		color_depth = COLORDEPTH_48B;
		break;
	default:
		color_depth = COLORDEPTH_24B;
		break;
	}

	return color_depth;
}

static int colordepth_to_bitdepth(enum hdmi_color_depth color_depth)
{
	int bitdepth;

	switch (color_depth) {
	case COLORDEPTH_24B:
		bitdepth = 8;
		break;
	case COLORDEPTH_30B:
		bitdepth = 10;
		break;
	case COLORDEPTH_36B:
		bitdepth = 12;
		break;
	case COLORDEPTH_48B:
		bitdepth = 16;
		break;
	default:
		bitdepth = 8;
		break;
	}

	return bitdepth;
}

static struct hdmitx_color_attr *meson_hdmitx_get_candidate_attr_list
	(struct am_hdmi_tx *am_hdmi, struct am_meson_crtc_state *crtc_state)
{
	const char *outputmode = crtc_state->base.adjusted_mode.name;
	struct hdmitx_color_attr *attr_list = NULL;
	enum hdmi_hdr_status hdr_status = SDR;
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);

	if (am_hdmi->recovery_mode)
		hdr_status = hdmitx_common_get_hdr_status(tx_comm);

	/* filter some color value options, aimed at some modes. */
	if (crtc_state->crtc_eotf_type ==
			HDMI_EOTF_MESON_DOLBYVISION ||
			hdr_status == DOLBYVISION_STD) {
		attr_list = dv_color_attr_list;
	} else if (crtc_state->crtc_eotf_type ==
			HDMI_EOTF_MESON_DOLBYVISION_LL ||
			hdr_status == DOLBYVISION_LOWLATENCY) {
		attr_list = dv_ll_color_attr_list;
	} else if (!strcmp(outputmode, MODE_4K2K60HZ) ||
	    !strcmp(outputmode, MODE_4K2K50HZ) ||
	    !strcmp(outputmode, MODE_4K2KSMPTE60HZ) ||
	    !strcmp(outputmode, MODE_4K2KSMPTE50HZ)) {
		attr_list = color_attr_list;
	} else {
		attr_list = other_color_attr_list;
	}

	return attr_list;
}

static bool meson_hdmitx_test_color_attr(struct am_hdmi_tx *am_hdmi,
	struct am_meson_crtc_state *crtc_state,
	struct hdmitx_color_attr *test_attr, u64 sequence_id)
{
	struct hdmitx_common_state comm_state;
	char *outputmode = crtc_state->base.adjusted_mode.name;
	struct hdmitx_color_attr *attr_list = NULL;
	struct hdmitx_common *common = to_hdmitx_common(am_hdmi->hdmitx_dev);

	if (test_attr->colorformat == HDMI_COLORSPACE_RESERVED6)
		return false;

	attr_list = meson_hdmitx_get_candidate_attr_list(am_hdmi, crtc_state);

	do {
		if (attr_list->colorformat == HDMI_COLORSPACE_RESERVED6)
			break;

		if (attr_list->colorformat == test_attr->colorformat &&
			attr_list->bitdepth == test_attr->bitdepth) {
			memset(&comm_state, 0, sizeof(comm_state));
			comm_state.state_sequence_id = sequence_id;
			if (!hdmitx_common_validate_mode_locked(common, &comm_state, outputmode,
					attr_list->colorformat,
					bitdepth_to_colordepth(attr_list->bitdepth),
					false)) {
				DRM_DEBUG_KMS("success [%d]+[%d]\n",
					attr_list->colorformat,
					attr_list->bitdepth);
				break;
			}
		}
	} while (attr_list++);

	if (attr_list->colorformat == HDMI_COLORSPACE_RESERVED6)
		return false;
	else
		return true;
}

static int meson_hdmitx_decide_color_attr
	(struct am_hdmi_tx *am_hdmi, struct am_meson_crtc_state *crtc_state,
	struct hdmitx_color_attr *attr, u64 sequence_id)
{
	struct hdmitx_common_state comm_state;
	char *outputmode = crtc_state->base.adjusted_mode.name;
	struct hdmitx_color_attr *attr_list = NULL;
	struct hdmitx_common *common = to_hdmitx_common(am_hdmi->hdmitx_dev);

	if (!outputmode) {
		DRM_ERROR("current mode empty.\n");
		return -EINVAL;
	}

	attr_list = meson_hdmitx_get_candidate_attr_list(am_hdmi, crtc_state);

	do {
		if (attr_list->colorformat == HDMI_COLORSPACE_RESERVED6)
			break;
		memset(&comm_state, 0, sizeof(comm_state));
		comm_state.state_sequence_id = sequence_id;

		if (!hdmitx_common_validate_mode_locked(common, &comm_state, outputmode,
				attr_list->colorformat,
				bitdepth_to_colordepth(attr_list->bitdepth), false)) {
			attr->colorformat = attr_list->colorformat;
			attr->bitdepth = attr_list->bitdepth;
			DRM_DEBUG_KMS("get fmt attr [%d]+[%d]\n",
				attr->colorformat,
				attr->bitdepth);
			break;
		}
	} while (attr_list++);
	if (attr_list->colorformat == HDMI_COLORSPACE_RESERVED6) {
		DRM_DEBUG_KMS("no attr found, reset to 444,8bit.\n");
		attr->colorformat = HDMI_COLORSPACE_RGB;
		attr->bitdepth = 8;
	}

	DRM_DEBUG_KMS("[%s,eotf:%d]=>attr[%d,%d]\n",
		outputmode, crtc_state->crtc_eotf_type,
		attr->colorformat, attr->bitdepth);

	return 0;
}

/*
 * Calculate the alternate clock for the CEA mode
 * (60Hz vs. 59.94Hz etc.)
 */
static unsigned int
cea_mode_alternate_clock(const struct drm_display_mode *cea_mode)
{
	unsigned int clock = cea_mode->clock;

	if (drm_mode_vrefresh(cea_mode) % 6 != 0)
		return clock;

	/*
	 * edid_cea_modes contains the 59.94Hz
	 * variant for 240 and 480 line modes,
	 * and the 60Hz variant otherwise.
	 */
	clock = DIV_ROUND_CLOSEST(clock * 1000, 1001);

	return clock;
}

static void meson_hdmitx_add_alter_mode(struct drm_connector *connector,
			 struct drm_display_mode *mode, int vic)
{
	struct drm_display_mode *newmode;
	unsigned int clock1, clock2;
	struct drm_device *dev = connector->dev;
	int vrefresh;

	clock1 = mode->clock;
	clock2 = cea_mode_alternate_clock(mode);

	if (clock1 == clock2)
		return;

	newmode = drm_mode_duplicate(dev, mode);
	if (!newmode)
		return;

	newmode->clock = clock2;
	vrefresh = drm_mode_vrefresh(newmode);
	if (vrefresh < 1) {
		drm_mode_destroy(dev, newmode);
		return;
	}

	if (mode->vdisplay == 2160 && mode->hdisplay == 4096) {
		snprintf(newmode->name, DRM_DISPLAY_MODE_LEN, "smpte%dhz",
			vrefresh - 1);
	} else {
		char *resolution_end = strrchr(newmode->name, 'p') ?: strrchr(newmode->name, 'i');

		if (resolution_end) {
			size_t remaining = DRM_DISPLAY_MODE_LEN -
				(resolution_end - newmode->name) - 1;

			snprintf(resolution_end + 1, remaining, "%dhz", vrefresh - 1);
		}
	}

	drm_mode_probed_add(connector, newmode);
}

static void meson_hdmitx_convert_timing_para(int vic,
					     struct drm_display_mode *mode,
					     bool edid_vic)
{
	const struct hdmi_timing *timing;
	char *strp = NULL;
	/*
	 * From edid_cea_modes_1 in drm_edid.c:
	 * 6 - 720(1440)x480i@60Hz 4:3
	 * 7 - 720(1440)x480i@60Hz 16:9
	 * 8 - 720(1440)x240@60Hz 4:3
	 * 9 - 720(1440)x240@60Hz 16:9
	 * 21 - 720(1440)x576i@50Hz 4:3
	 * 22 - 720(1440)x576i@50Hz 16:9
	 * 23 - 720(1440)x288@50Hz 4:3
	 * 24 - 720(1440)x288@50Hz 16:9
	 * 44 - 720(1440)x576i@100Hz 4:3
	 * 45 - 720(1440)x576i@100Hz 16:9
	 * 50 - 720(1440)x480i@120Hz 4:3
	 * 51 - 720(1440)x480i@120Hz 16:9
	 * 54 - 720(1440)x576i@200Hz 4:3
	 * 55 - 720(1440)x576i@200Hz 16:9
	 * 58 - 720(1440)x480i@240Hz 4:3
	 * 59 - 720(1440)x480i@240Hz 16:9
	 */
	static int vic_dbl_clk[] = {6, 7, 8, 9, 21, 22, 23, 24, 44, 45, 50, 51, 54, 55, 58, 59};
	int i;

	timing = hdmitx_mode_vic_to_hdmi_timing(vic);
	if (!timing) {
		DRM_ERROR("Get timing by vic [%d] failed.\n", vic);
		return;
	}
	if (timing->sname) {
		memcpy(mode->name, timing->sname,
		       (strlen(timing->sname) < DRM_DISPLAY_MODE_LEN) ?
		       strlen(timing->sname) : DRM_DISPLAY_MODE_LEN);
	} else if (timing->name) {
		memcpy(mode->name, timing->name,
		       (strlen(timing->name) < DRM_DISPLAY_MODE_LEN) ?
		       strlen(timing->name) : DRM_DISPLAY_MODE_LEN);
	} else {
		DRM_ERROR("get vic %d without name\n", vic);
		return;
	}

	mode->name[DRM_DISPLAY_MODE_LEN - 1] = '\0';
	/* remove _4x3 suffix, in case misunderstand */
	strp = strstr(mode->name, "_4x3");
	if (strp)
		*strp = '\0';

	mode->type = DRM_MODE_TYPE_DRIVER;
	mode->clock = timing->pixel_freq;

	mode->hdisplay = timing->h_active;
	mode->hsync_start = timing->h_active + timing->h_front;
	mode->hsync_end = timing->h_active + timing->h_front + timing->h_sync;

	mode->htotal = timing->h_total;
	/* for 480i/576i, horizontal timing is repeated */
	if (timing->pixel_repetition_factor) {
		mode->hdisplay >>= 1;
		mode->hsync_start >>= 1;
		mode->hsync_end >>= 1;
		mode->htotal >>= 1;
		mode->clock >>= 1;
	}

	/*use hskew to distinguish whether it's qms mode or edid mode*/
	if (edid_vic)
		mode->hskew = 1;
	else
		mode->hskew = 0;

	mode->flags |= timing->h_pol ?
		DRM_MODE_FLAG_PHSYNC : DRM_MODE_FLAG_NHSYNC;

	mode->vdisplay = timing->v_active;
	mode->vsync_start = timing->v_active + timing->v_front;
	mode->vsync_end = timing->v_active + timing->v_front + timing->v_sync;
	mode->vtotal = timing->v_total;
	mode->vscan = 0;
	mode->flags |= timing->v_pol ?
		DRM_MODE_FLAG_PVSYNC : DRM_MODE_FLAG_NVSYNC;

	if (!timing->pi_mode)
		mode->flags |= DRM_MODE_FLAG_INTERLACE;

	for (i = 0; i < ARRAY_SIZE(vic_dbl_clk); i++)
		if (vic_dbl_clk[i] == vic) {
			mode->flags |= DRM_MODE_FLAG_DBLCLK;
			DRM_DEBUG("found vic %d with DBLCK\n", vic);
		}
}

static int meson_hdmitx_mode_probed_add(int count, int *vics,
	struct drm_connector *connector, bool edid_vic)
{
	struct drm_display_mode *mode, *pref_mode = NULL;
	struct am_hdmi_tx *am_hdmitx = connector_to_am_hdmi(connector);
	bool pref_flag;
	struct meson_drm *priv;
	struct meson_of_conf *conf;
	int i;

	if (!am_hdmitx) {
		DRM_ERROR("am_hdmitx is NULL!\n");
		return -EINVAL;
	}

	priv = am_hdmitx->base.drm_priv;
	conf = &priv->of_conf;

	for (i = 0; i < count; i++) {
		mode = drm_mode_create(connector->dev);
		if (!mode) {
			DRM_ERROR("drm mode create failed.\n");
			continue;
		}

		meson_hdmitx_convert_timing_para(vics[i], mode, edid_vic);

		/*for recovery ui*/
		if (edid_vic && hdmitx_set_smaller_pref) {
			/*
			 * select 1080P mode with hightest refresh rate first,
			 * if not find then select 720p mode as pref mode
			 */
			if (conf->pref_mode && (!strcmp(conf->pref_mode, "2160p"))) {
				pref_flag = (!(mode->flags & DRM_MODE_FLAG_INTERLACE)) &&
				((mode->hdisplay == 3840 && mode->vdisplay == 2160) ||
				(mode->hdisplay == 1920 && mode->vdisplay == 1080) ||
				(mode->hdisplay == 1280 && mode->vdisplay == 720));
				DRM_DEBUG_KMS("mode_name is %s, pref_flag = %d\n",
					conf->pref_mode, pref_flag);
			} else {
				pref_flag = (!(mode->flags & DRM_MODE_FLAG_INTERLACE)) &&
				((mode->hdisplay == 1920 && mode->vdisplay == 1080) ||
				(mode->hdisplay == 1280 && mode->vdisplay == 720));
				DRM_DEBUG_KMS("mode_name is %s, pref_flag = %d\n",
				conf->pref_mode, pref_flag);
			}
			if (pref_flag) {
				if (!pref_mode)
					pref_mode = mode;
				else if (pref_mode->hdisplay < mode->hdisplay)
					pref_mode = mode;
				else if (pref_mode->hdisplay == mode->hdisplay &&
					(drm_mode_vrefresh(pref_mode) < drm_mode_vrefresh(mode)))
					pref_mode = mode;
			}
		}

		drm_mode_probed_add(connector, mode);
		meson_hdmitx_add_alter_mode(connector, mode, vics[i]);

		DRM_DEBUG("add mode [%s] [%d]\n", mode->name, mode->hskew);
	}

	if (edid_vic && pref_mode)
		pref_mode->type |= DRM_MODE_TYPE_PREFERRED;

	return 0;
}

int meson_hdmitx_get_modes(struct drm_connector *connector)
{
	u32 vrr_cap = 0;
	struct edid *edid;
	int *vics;
	int count = 0, count_qms = 0, i = 0, j = 0;
	struct drm_display_mode *mode;
	struct am_hdmi_tx *am_hdmitx;
	struct hdmitx_common *tx_comm;
	struct hdmitx_vrr_mode_group *groups;
	struct hdmitx_vrr_mode_group *group;
	unsigned char *raw_edid = NULL;
	int *vrr_list;
	int *tmp;
	int num_group = 0;
	u64 sequence_id = 0;

	if (!connector) {
		DRM_ERROR("connector is NULL!\n");
		return count;
	}
	am_hdmitx = connector_to_am_hdmi(connector);
	if (!am_hdmitx) {
		DRM_ERROR("amhdmitx is NULL!\n");
		return count;
	}

	tx_comm = to_hdmitx_common(am_hdmitx->hdmitx_dev);

	raw_edid = hdmitx_get_raw_edid(tx_comm);
	if (raw_edid) {
		edid = (struct edid *)raw_edid;
		drm_connector_update_edid_property(connector, edid);
	}

	sequence_id = am_hdmitx->sequence_id;

	am_hdmitx->sequence_id = hdmitx_get_hpd_hw_sequence_id(tx_comm);
	/*
	 * After set mode, hwc will update the connector.
	 * In order to prevent the edid from being parsed every time,
	 * the sequence_id judgment is added, and the edid is only parsed
	 * when the hot_plug time occurs.
	 */
	if (sequence_id != am_hdmitx->sequence_id)
		hdmitx_edid_process(tx_comm, false, true);

	vrr_list = kcalloc(MAX_VRR_GROUP_VIC_NUM, sizeof(int),  GFP_KERNEL);
	tmp = kcalloc(MAX_VRR_GROUP_VIC_NUM, sizeof(int),  GFP_KERNEL);
	groups = kcalloc(MAX_VRR_MODE_GROUP, sizeof(*groups), GFP_KERNEL);

	if (!groups || !tmp || !vrr_list) {
		DRM_ERROR("alloc fail\n");
		goto end;
	}

	num_group = hdmitx_common_get_vrr_mode_group(tx_comm, groups, MAX_VRR_MODE_GROUP);

	/* get vrr capability */
	vrr_cap = hdmitx_common_get_vrr_cap(tx_comm);
	DRM_DEBUG_KMS("support vrr_cap[%d]\n", vrr_cap);
	drm_connector_set_vrr_capable_property(connector, vrr_cap);
	/*add modes from hdmitx instead of edid*/
	count = hdmitx_common_get_vic_list(tx_comm, &vics);

	if (vrr_cap && count) {
		int src = 0, dst = 0;

		for (i = 0; i < num_group; i++) {
			group = &groups[i];
			for (j = 0; j < ARRAY_SIZE(group->qms_vic_lists); j++) {
				tmp[count_qms] = group->qms_vic_lists[j];
				DRM_DEBUG_KMS("_%d__%d__%zd\n", __LINE__, tmp[count_qms],
				ARRAY_SIZE(group->qms_vic_lists));
				count_qms++;
			}
		}

		/*remove duplicate vics array variables*/
		while (src  < count_qms) {
			bool exist = false;

			for (i = 0; i < count; i++) {
				if (tmp[src] == vics[i]) {
					src++;
					exist = true;
					break;
				}
			}

			if (!exist)
				vrr_list[dst++] = tmp[src++];
		}

		count_qms = dst;
	}

	if (count) {
		meson_hdmitx_mode_probed_add(count, vics, connector, true);
		kfree(vics);
	}

	if (count_qms)
		meson_hdmitx_mode_probed_add(count_qms, vrr_list, connector, false);

	/*TODO:add dummy mode temp.*/
	if (am_hdmitx->base.drm_priv->dummyl_from_hdmitx) {
		mode = drm_mode_duplicate(connector->dev, &dummy_mode);
		if (!mode) {
			DRM_INFO("[%s:%d]dup dummy mode failed.\n", __func__,
				 __LINE__);
		} else {
			drm_mode_probed_add(connector, mode);
			count++;
		}
	}

	connector->display_info.monitor_range.max_vfreq = am_hdmitx->max_vfreq;
	connector->display_info.monitor_range.min_vfreq = am_hdmitx->min_vfreq;

end:
	kfree(vrr_list);
	kfree(tmp);
	kfree(groups);

	return count + count_qms;
}

/*   drm_display_mode	     :		 hdmi_format_para
 *		hdisp     : h_active
 *		hsync_start(hss)    : h_active + h_front
 *		hsync_end(hse) : h_active + h_front + h_sync
 *		htotal : h_total
 */
enum drm_mode_status meson_hdmitx_check_mode(struct drm_connector *connector,
					   struct drm_display_mode *mode)
{
	return MODE_OK;
}

static struct drm_encoder *meson_hdmitx_best_encoder
	(struct drm_connector *connector)
{
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);

	return &am_hdmi->encoder;
}

static enum drm_connector_status am_hdmitx_connector_detect
	(struct drm_connector *connector, bool force)
{
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	int hpdstat = hdmitx_get_hpd_state(tx_comm);

	DRM_DEBUG_KMS("am_hdmi_connector_detect [%d]\n", hpdstat);
	return hpdstat == 1 ?
		connector_status_connected : connector_status_disconnected;
}

static int _get_hdr_info(const struct hdr_info *hdr)
{
	int hdr_cap_value = 0;
	const struct hdr10_plus_info *hdr10p = &hdr->hdr10plus_info;

	/*
	 * hdr_cap_value:
	 *   bit0 SDR
	 *   bit1 HDR
	 *   bit2 SMPTE2084
	 *   bit3 HLG
	 *   bit4 HDR10PLUS
	 */
	hdr_cap_value = hdr->hdr_support;
	/* hdr10plus_supported */
	if (hdr10p->ieeeoui == HDR10_PLUS_IEEE_OUI &&
		hdr10p->application_version != 0xFF)
		hdr_cap_value |= BIT(4);

	return hdr_cap_value;
}

static int get_hdr_info(struct am_hdmi_tx *am_hdmi, struct hdmitx_common *tx_comm)
{
	const struct hdr_info *hdr = NULL;

	hdmitx_set_hdr_priority(tx_comm,
			tx_comm->hdr_priority,
			&am_hdmi->hdr_info, &am_hdmi->dv_info);

	hdr = &am_hdmi->hdr_info;

	return _get_hdr_info(hdr);
}

static int get_hdr_info_rx(struct hdmitx_common *tx_comm)
{
	const struct hdr_info *hdr = hdmitx_common_get_hdr_info_rx(tx_comm);

	return _get_hdr_info(hdr);
}

static int __get_dv_info(const struct dv_info *dv)
{
	int dv_flag = 0;

	/*The Rx don't support DolbyVision*/
	if (dv->ieeeoui != DV_IEEE_OUI || dv->block_flag != CORRECT)
		return 0;

	/*DolbyVision RX support list:*/

	if (dv->ver == 0) {
		/*VSVDB Version:  bit0,1*/
		dv_flag |= 0 << 0;
		/*2160p%shz: 1*/
		dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
		/*Support mode:*/
		/*DV_RGB_444_8BIT*/
		dv_flag |= 1 << 3;
		/*DV_YCbCr_422_12BIT*/
		dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
	}

	if (dv->ver == 1) {
		/*VSVDB Version: %d-byte*/
		dv_flag |= 1 << 0;

		if (dv->length == 0xB) {
			/*2160p%shz: 1*/
			dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
			/*Support mode:*/
			/*DV_RGB_444_8BIT*/
			dv_flag |= 1 << 3;
			/*DV_YCbCr_422_12BIT*/
			dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
			/*LL_YCbCr_422_12BIT*/
			if (dv->low_latency == 0x01)
				dv_flag |= 1 << 5;
		}

		if (dv->length == 0xE) {
			/*2160p%shz: 1*/
			dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
			/*Support mode:*/
			/*DV_RGB_444_8BIT*/
			dv_flag |= 1 << 3;
			/*DV_YCbCr_422_12BIT*/
			dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
		}
	}

	if (dv->ver == 2) {
		/*VSVDB Version:*/
		dv_flag |= 2 << 0;

		/*2160p%shz: 1*/
		dv_flag |= (dv->sup_2160p60hz ? 1 : 0) << 2;
		/*Support mode:*/

		if (dv->Interface != 0x00 && dv->Interface != 0x01) {
			/*DV_RGB_444_8BIT*/
			dv_flag |= 1 << 3;
			/*DV_YCbCr_422_12BIT*/
			if (dv->sup_yuv422_12bit)
				dv_flag |= (dv->sup_yuv422_12bit ? 1 : 0) << 4;
		}

		/*LL_YCbCr_422_12BIT*/
		dv_flag |= 1 << 5;

		if (dv->Interface == 0x01 || dv->Interface == 0x03) {
			if (dv->sup_10b_12b_444 == 0x1) {
				/*LL_RGB_444_10BITT*/
				dv_flag |= 1 << 6;
			}
			if (dv->sup_10b_12b_444 == 0x2) {
				/*LL_RGB_444_12BIT*/
				dv_flag |= 1 << 7;
			}
		}

		dv_flag |= (dv->parity ? 1 : 0) << 8;
	}

	return dv_flag;
}

static int get_dv_info(struct am_hdmi_tx *am_hdmi, struct hdmitx_common *tx_comm)
{
	const struct dv_info *dv = NULL;

	hdmitx_set_hdr_priority(tx_comm,
			tx_comm->hdr_priority,
			&am_hdmi->hdr_info, &am_hdmi->dv_info);

	dv = &am_hdmi->dv_info;

	return __get_dv_info(dv);
}

static int get_dv_info_rx(struct hdmitx_common *tx_comm)
{
	const struct dv_info *dv = hdmitx_common_get_dv_info_rx(tx_comm);

	return __get_dv_info(dv);
}

static int hdcp_rx_ver(struct am_hdmi_tx *am_hdmi)
{
	/* Detect RX support HDCP14
	 * Here, must assume RX support HDCP14, otherwise affect 1A-03
	 */

	if (am_hdmi->hdcp_rx_type == 0x3)
		return 36;
	else
		return 14;

	return 0;
}

/*like hdmitx hdcp_mode node*/
static int get_hdcp_mode(struct am_hdmi_tx *am_hdmi)
{
	int hdcp_mode_flag = 0;

	if (am_hdmi->hdcp_mode) {
		hdcp_mode_flag |= am_hdmi->hdcp_mode;
		if (am_hdmi->hdcp_state == HDCP_STATE_SUCCESS)
			hdcp_mode_flag |= 1 << 3;
		else if (am_hdmi->hdcp_state == HDCP_STATE_FAIL)
			hdcp_mode_flag |= 0 << 3;
	} else {
		return 0;
	}

	return hdcp_mode_flag;
}

static int get_sink_type(struct hdmitx_common *tx_comm)
{
	int sink_type_flag = 0;

	if (!tx_comm->hpd_state) {
		/* none */
		sink_type_flag = BIT(0);
		return sink_type_flag;
	}

	if (tx_comm->rxcap.vsdb_phy_addr.b)
		/* repeater */
		sink_type_flag = BIT(1);
	else
		/* sink */
		sink_type_flag = BIT(2);

	return sink_type_flag;
}

void get_metadata(struct hdmitx_common *tx_comm,
			struct meson_hdr_static_metadata *mHdrMetaDataValue)
{
	const struct hdr_info hdrinfo = tx_comm->rxcap.hdr_info;

	mHdrMetaDataValue->lumi_max = hdrinfo.lumi_max;
	mHdrMetaDataValue->lumi_min = hdrinfo.lumi_min;
	mHdrMetaDataValue->lumi_avg = hdrinfo.lumi_avg;
}

static bool get_allm_cap(struct hdmitx_common *tx_comm)
{
	bool allm_cap_flag = 0;

	if (tx_comm->rxcap.allm)
		allm_cap_flag = 1;
	else
		allm_cap_flag = 0;

	return allm_cap_flag;
}

static int get_dc_cap(struct hdmitx_common *tx_comm)
{
	struct rx_cap *prxcap = &tx_comm->rxcap;
	const struct dv_info *dv =  &prxcap->dv_info;
	int i, dc_cap_mask = 0;

	/* DVI case, only rgb,8bit */
	if (prxcap->ieeeoui != HDMI_IEEE_OUI) {
		/* rgb,8bit */
		dc_cap_mask |= BIT(COLOR_RGB_8BIT);
		return dc_cap_mask;
	}

	if (prxcap->dc_36bit_420)
		/* 420,12bit */
		dc_cap_mask |= BIT(COLOR_YCBCR420_12BIT);
	if (prxcap->dc_30bit_420)
		/* 420,10bit */
		dc_cap_mask |= BIT(COLOR_YCBCR420_10BIT);

	for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
		if (prxcap->y420_vic[i]) {
			/* 420,8bit */
			dc_cap_mask |= BIT(COLOR_YCBCR420_8BIT);
			break;
		}
	}

	if (prxcap->native_Mode & CAP_BIT5_YCBCR_444_MASK) {
		if (prxcap->dc_y444) {
			if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
				/* 444,12bit */
				dc_cap_mask |= BIT(COLOR_YCBCR444_12BIT);
			if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1) {
				/* 444,10bit */
				dc_cap_mask |= BIT(COLOR_YCBCR444_10BIT);
			}
		}
		/* 444,8bit */
		dc_cap_mask |= BIT(COLOR_YCBCR444_8BIT);
	}
	/* y422, not check dc */
	if (prxcap->native_Mode & CAP_BIT4_YCBCR_422_MASK)
		/* 422,12bit */
		dc_cap_mask |= BIT(COLOR_YCBCR422_12BIT);

	if (prxcap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
		/* rgb,12bit */
		dc_cap_mask |= BIT(COLOR_RGB_12BIT);
	if (prxcap->dc_30bit || dv->sup_10b_12b_444 == 0x1)
		/* rgb,10bit */
		dc_cap_mask |= BIT(COLOR_RGB_10BIT);
	/* rgb,8bit */
	dc_cap_mask |= BIT(COLOR_RGB_8BIT);
	return dc_cap_mask;
}

static int am_hdmitx_connector_atomic_set_property
	(struct drm_connector *connector,
	struct drm_connector_state *state,
	struct drm_property *property, uint64_t val)
{
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(state);
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct hdmitx_color_attr *attr = &hdmitx_state->color_attr_para;
	struct hdmitx_common *tx_comm = meson_get_hdmitx_common(connector);

	DRM_DEBUG_KMS("\n");
	if (property == am_hdmi->update_attr_prop) {
		hdmitx_state->update = true;
		return 0;
	} else if (property == am_hdmi->color_space_prop) {
		attr->colorformat = val;
		return 0;
	} else if (property == am_hdmi->color_depth_prop) {
		attr->bitdepth = val;
		hdmitx_state->color_force = true;
		return 0;
	} else if (property == am_hdmi->avmute_prop) {
		hdmitx_state->avmute = val;
		return 0;
	} else if (property == am_hdmi->allm_prop) {
		hdmitx_state->allm_mode = val;
		return 0;
	} else if (property == am_hdmi->hdr_priority_prop) {
		hdmitx_state->hdr_priority = val;
		return 0;
	} else if (property == am_hdmi->ready_prop) {
		hdmitx_state->ready = val;
		return 0;
	} else if (property == am_hdmi->frac_rate_policy_prop) {
		DRM_DEBUG("frac rate property was not used\n");
		return 0;
	} else if (property == am_hdmi->scan_info_prop) {
		hdmitx_state->scan_info = val;
		hdmitx_common_set_scan_info(tx_comm, hdmitx_state->scan_info);
		return 0;
	}

	return -EINVAL;
}

static int am_hdmitx_connector_atomic_get_property
	(struct drm_connector *connector,
	const struct drm_connector_state *state,
	struct drm_property *property, uint64_t *val)
{
	struct hdmitx_common *tx_comm = meson_get_hdmitx_common(connector);
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(state);
	struct hdmitx_color_attr *attr = &hdmitx_state->color_attr_para;
	struct meson_hdr_static_metadata mHdrMetaDataValue;
	struct drm_property_blob *new_metadata, *old_metadata;
	bool replaced;

	if (property == am_hdmi->update_attr_prop) {
		*val = 0;
		return 0;
	} else if (property == am_hdmi->color_space_prop) {
		*val = attr->colorformat;
		return 0;
	} else if (property == am_hdmi->color_depth_prop) {
		*val = attr->bitdepth;
		return 0;
	} else if (property == am_hdmi->avmute_prop) {
		*val = hdmitx_state->avmute;
		return 0;
	} else if (property == am_hdmi->hdmi_hdr_status_prop) {
		*val = hdmitx_common_get_hdr_status(tx_comm);
		return 0;
	} else if (property == am_hdmi->hdr_cap_prop) {
		*val = get_hdr_info(am_hdmi, tx_comm);
		return 0;
	} else if (property == am_hdmi->hdr_cap_rx_prop) {
		*val = get_hdr_info_rx(tx_comm);
		return 0;
	} else if (property == am_hdmi->dv_cap_prop) {
		*val = get_dv_info(am_hdmi, tx_comm);
		return 0;
	} else if (property == am_hdmi->dv_cap_rx_prop) {
		*val = get_dv_info_rx(tx_comm);
		return 0;
	} else if (property == am_hdmi->hdcp_ver_prop) {
		*val = hdcp_rx_ver(am_hdmi);
		return 0;
	} else if (property == am_hdmi->hdcp_mode_prop) {
		*val = get_hdcp_mode(am_hdmi);
		return 0;
	} else if (property == am_hdmi->hdcp_topo_prop) {
		*val = drm_hdmitx_common_get_dw_hdcp_topo_info(tx_comm);
		return 0;
	} else if (property == am_hdmi->contenttype_cap_prop) {
		*val = hdmitx_common_get_content_types(tx_comm);
		return 0;
	} else if (property == am_hdmi->allm_prop) {
		*val = hdmitx_state->allm_mode;
		return 0;
	} else if (property == am_hdmi->hdr_priority_prop) {
		*val = hdmitx_state->hdr_priority;
		return 0;
	} else if (property == am_hdmi->ready_prop) {
		*val = hdmitx_common_get_ready_state(tx_comm);
		return 0;
	} else if (property == am_hdmi->type_prop) {
		*val = am_hdmi->hdmi_type;
		return 0;
	} else if (property == am_hdmi->edid_valid_prop) {
		*val = hdmitx_common_get_edid_valid_state(tx_comm);
		return 0;
	} else if (property == am_hdmi->hdcp_user_prop) {
		*val = hdmitx_common_get_hdcp_user_state(tx_comm);
		return 0;
	} else if (property == am_hdmi->frac_rate_policy_prop) {
		*val = tx_comm->fmt_para.frac_mode;
		return 0;
	} else if (property == am_hdmi->hdmi_used_prop) {
		*val = hdmitx_common_get_hdmi_used_state(tx_comm);
		return 0;
	} else if (property == am_hdmi->sink_type_prop) {
		*val = get_sink_type(tx_comm);
		return 0;
	} else if (property == am_hdmi->static_meta_prop) {
		get_metadata(tx_comm, &mHdrMetaDataValue);
		old_metadata = hdmitx_state->metadata;
		new_metadata = drm_property_create_blob(connector->dev,
			sizeof(mHdrMetaDataValue), &mHdrMetaDataValue);
		if (IS_ERR(new_metadata)) {
			DRM_ERROR("create metadata blob fail.\n");
			return 0;
		}
		replaced = drm_property_replace_blob(&hdmitx_state->metadata,
			new_metadata);
		if (replaced && old_metadata)
			drm_property_blob_put(old_metadata);

		if (!replaced && new_metadata)
			drm_property_blob_put(new_metadata);

		*val = (hdmitx_state->metadata) ? hdmitx_state->metadata->base.id : 0;
		return 0;
	} else if (property == am_hdmi->allm_cap_prop) {
		*val = get_allm_cap(tx_comm);
		return 0;
	} else if (property == am_hdmi->dc_cap_prop) {
		*val = get_dc_cap(tx_comm);
		return 0;
	} else if (property == am_hdmi->scan_info_prop) {
		*val = hdmitx_common_get_scan_info(tx_comm);
		return 0;
	} else if (property == am_hdmi->vrr_capable_type_prop) {
		*val = hdmitx_common_get_vrr_cap(tx_comm);
		return 0;
	} else if (property == am_hdmi->sink_device_type_prop) {
		*val = hdmitx_common_get_sink_device_type(tx_comm);
		return 0;
	} else {
		DRM_ERROR("[CONNECTOR:%d:%s] unknown property [PROP:%d:%s]\n",
				connector->base.id, connector->name,
				property->base.id, property->name);
	return -EINVAL;
	}
}

static void am_hdmitx_connector_destroy(struct drm_connector *connector)
{
	drm_connector_unregister(connector);
	drm_connector_cleanup(connector);
}

int meson_hdmitx_atomic_check(struct drm_connector *connector,
	struct drm_atomic_state *state)
{
	struct am_hdmitx_connector_state *new_hdmitx_state, *old_hdmitx_state;
	struct drm_crtc_state *new_crtc_state = NULL;
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	unsigned int hdmitx_content_type = hdmitx_common_get_content_types(tx_comm);
	struct am_meson_crtc_state *meson_crtc_state;

	if (!state) {
		DRM_ERROR("state is NULL.\n");
		return -EINVAL;
	}

	old_hdmitx_state = to_am_hdmitx_connector_state
		(drm_atomic_get_old_connector_state(state, connector));
	new_hdmitx_state = to_am_hdmitx_connector_state
		(drm_atomic_get_new_connector_state(state, connector));

	if (!new_hdmitx_state || !old_hdmitx_state) {
		DRM_ERROR("hdmitx_state is NULL.\n");
		return -EINVAL;
	}

	/*check content type.*/
	if (((1 << new_hdmitx_state->base.content_type) &
		hdmitx_content_type) == 0) {
		DRM_ERROR("check content type[%d-%u] fail\n",
			new_hdmitx_state->base.content_type,
			hdmitx_content_type);
		return -EINVAL;
	}

	if (new_hdmitx_state->base.crtc)
		new_crtc_state = drm_atomic_get_new_crtc_state(state,
			new_hdmitx_state->base.crtc);
	else
		return 0;

	meson_crtc_state = to_am_meson_crtc_state(new_crtc_state);

	if (state->allow_modeset && new_crtc_state) {
		if (!am_hdmi->hdmitx_on && !am_hdmi->android_path) {
			new_crtc_state->connectors_changed = true;
			DRM_ERROR("hdmitx_on changed, force modeset.\n");
		}

		DRM_DEBUG_KMS("update[%d],color_force[%d],cs[%d %d],cd[%d %d],frac_rate[%d %d]\n",
			new_hdmitx_state->update, new_hdmitx_state->color_force,
			old_hdmitx_state->color_attr_para.colorformat,
			new_hdmitx_state->color_attr_para.colorformat,
			old_hdmitx_state->color_attr_para.bitdepth,
			new_hdmitx_state->color_attr_para.bitdepth,
			old_hdmitx_state->hcs.para.frac_mode,
			new_hdmitx_state->hcs.para.frac_mode);
		/*force set mode.*/
		if (new_hdmitx_state->update)
			new_crtc_state->connectors_changed = true;

		if (new_hdmitx_state->color_force) {
			new_crtc_state->mode_changed = true;
			if (new_hdmitx_state->color_attr_para.colorformat !=
				old_hdmitx_state->color_attr_para.colorformat ||
				new_hdmitx_state->color_attr_para.bitdepth !=
				old_hdmitx_state->color_attr_para.bitdepth) {
				meson_crtc_state->attr_changed = true;
			}
		}

		if (new_hdmitx_state->hcs.para.frac_mode !=
				old_hdmitx_state->hcs.para.frac_mode) {
			new_crtc_state->mode_changed = true;
		}
	}

	return 0;
}

struct drm_connector_state *meson_hdmitx_atomic_duplicate_state
	(struct drm_connector *connector)
{
	struct am_hdmitx_connector_state *new_state;
	struct am_hdmitx_connector_state *cur_state =
		to_am_hdmitx_connector_state(connector->state);
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return NULL;

	__drm_atomic_helper_connector_duplicate_state(connector,
		&new_state->base);

	new_state->update = false;
	new_state->color_force = false;
	new_state->color_attr_para.colorformat = cur_state->color_attr_para.colorformat;
	new_state->color_attr_para.bitdepth = cur_state->color_attr_para.bitdepth;
	new_state->hdr_priority = cur_state->hdr_priority;
	new_state->pref_hdr_policy = cur_state->pref_hdr_policy;
	new_state->allm_mode = cur_state->allm_mode;
	cur_state->hcs.state_sequence_id = am_hdmi->sequence_id;
	new_state->metadata = cur_state->metadata;
	memcpy(&new_state->hcs, &cur_state->hcs, sizeof(struct hdmitx_common_state));

	return &new_state->base;
}

void meson_hdmitx_atomic_destroy_state(struct drm_connector *connector,
	 struct drm_connector_state *state)
{
	struct am_hdmitx_connector_state *hdmitx_state;

	hdmitx_state = to_am_hdmitx_connector_state(state);
	__drm_atomic_helper_connector_destroy_state(&hdmitx_state->base);
	kfree(hdmitx_state);
}

/*similar to drm_atomic_helper_connector_reset*/
void meson_hdmitx_reset(struct drm_connector *connector)
{
	struct am_hdmitx_connector_state *hdmitx_state;
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);

	hdmitx_state = kzalloc(sizeof(*hdmitx_state), GFP_KERNEL);
	if (!hdmitx_state)
		return;

	if (connector->state)
		__drm_atomic_helper_connector_destroy_state(connector->state);
	kfree(connector->state);

	__drm_atomic_helper_connector_reset(connector, &hdmitx_state->base);

	hdmitx_state->base.hdcp_content_type = am_hdmi->hdcp_request_content_type;
	hdmitx_state->base.content_protection = am_hdmi->hdcp_request_content_protection;

	hdmitx_state->pref_hdr_policy = MESON_PREF_DV;
	hdmitx_state->color_attr_para.colorformat = HDMI_COLORSPACE_RGB;
	hdmitx_state->color_attr_para.bitdepth = 8;

	/*drm api need update state, so need delay attach when create state.*/
	if (!connector->max_bpc_property)
		drm_connector_attach_max_bpc_property
				(connector, 8, HDMITX_MAX_BPC);
}

void meson_hdmitx_atomic_print_state(struct drm_printer *p,
	const struct drm_connector_state *state)
{
	int i, num_group = 0;
	struct hdmitx_vrr_mode_group *group;
	struct hdmitx_vrr_mode_group *groups;
	struct drm_connector *connector = state->connector;
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(state);

	groups = kcalloc(MAX_VRR_MODE_GROUP, sizeof(*groups), GFP_KERNEL);
	if (groups)
		num_group = hdmitx_common_get_vrr_mode_group(tx_comm, groups,
							      MAX_VRR_MODE_GROUP);
	if (!num_group)
		DRM_ERROR("get vrr error or not support qms\n");

	drm_printf(p, "\tdrm hdmitx state:\n");
	drm_printf(p, "\t\t android_path:[%d]\n", am_hdmi->android_path);
	drm_printf(p, "\t\t VRR_CAP:[%u]\n", hdmitx_common_get_vrr_cap(tx_comm));
	drm_printf(p, "\t\t hdr_policy[%d]\n", hdmitx_state->pref_hdr_policy);
	drm_printf(p, "\t\t hdcp_state:[%d]\n", am_hdmi->hdcp_state);
	drm_printf(p, "\t\t update:[%d]\n", hdmitx_state->update);
	drm_printf(p, "\t\t cs: [%d], cd: [%d], hdr_priority: [%d]\n",
		   hdmitx_state->color_attr_para.colorformat,
		   hdmitx_state->color_attr_para.bitdepth,
		   hdmitx_state->hdr_priority);
	drm_printf(p, "\t\t avmute:[%d]\n", hdmitx_state->avmute);
	drm_printf(p, "\t\t hdmi_hdr_status:[%d]\n",
		   hdmitx_common_get_hdr_status(tx_comm));
	drm_printf(p, "\t\t hdr_cap:[%d]\n", get_hdr_info(am_hdmi, tx_comm));
	drm_printf(p, "\t\t hdr_cap_rx:[%d]\n", get_hdr_info_rx(tx_comm));
	drm_printf(p, "\t\t dv_cap:[%d]\n", get_dv_info(am_hdmi, tx_comm));
	drm_printf(p, "\t\t dv_cap_rx:[%d]\n", get_dv_info_rx(tx_comm));
	drm_printf(p, "\t\t hdcp_ver:[%d]\n", hdcp_rx_ver(am_hdmi));
	drm_printf(p, "\t\t hdcp_mode:[%d]\n", get_hdcp_mode(am_hdmi));
	drm_printf(p, "\t\t hdcp_topo:[%d]\n", drm_hdmitx_common_get_dw_hdcp_topo_info(tx_comm));
	drm_printf(p, "\t\t contenttype_cap:[%d]\n",
		   hdmitx_common_get_content_types(tx_comm));
	drm_printf(p, "\t\t allm_mode:[%d]\n", hdmitx_state->allm_mode);
	drm_printf(p, "\t\t hdr_priority:[%d]\n", hdmitx_state->hdr_priority);
	drm_printf(p, "\t\t ready:[%d]\n", hdmitx_common_get_ready_state(tx_comm));
	drm_printf(p, "\t\t hdmi_type:[%d]\n", am_hdmi->hdmi_type);
	drm_printf(p, "\t\t edid_valid:[%d]\n", hdmitx_common_get_edid_valid_state(tx_comm));
	drm_printf(p, "\t\t hdmi_user:[%d]\n", hdmitx_common_get_hdcp_user_state(tx_comm));
	drm_printf(p, "\t\t hdmi_used:[%d]\n", hdmitx_common_get_hdmi_used_state(tx_comm));
	drm_printf(p, "\t\t sink_type:[%d]\n", get_sink_type(tx_comm));
	drm_printf(p, "\t\t allm_cap:[%d]\n", get_allm_cap(tx_comm));
	drm_printf(p, "\t\t dc_cap:[%d]\n", get_dc_cap(tx_comm));
	drm_printf(p, "\t\t scan_info:[%d]\n", hdmitx_common_get_scan_info(tx_comm));
	drm_printf(p, "\t\t sink_device_type:[%d]\n", hdmitx_common_get_sink_device_type(tx_comm));

	drm_printf(p, "\t\t drm to hdmitx timing state:\n");
	drm_printf(p, "\t\t\t vic:[%d], cs:[%d], cd:[%d], name:[%s]\n",
		   hdmitx_state->hcs.para.vic, hdmitx_state->hcs.para.cs,
		   hdmitx_state->hcs.para.cd, hdmitx_state->hcs.para.name);
	drm_printf(p, "\t\t\t frac_rate_policy:[%d]\n", hdmitx_state->hcs.para.frac_mode);
	drm_printf(p, "\t\t qms vrr info:\n");
	for (i = 0; i < num_group; i++) {
		if (groups) {
			group = &groups[i];
			drm_printf(p, "\t\t\t %u,%u,%u-%u,%u\n", group->width, group->height,
				group->vrr_min, group->vrr_max, group->brr_vic);
		}
	}
	drm_printf(p, "\t\t game vrr info:\n");
	for (i = 0; i < num_group; i++) {
		if (groups) {
			group = &groups[i];
			drm_printf(p, "\t\t\t %u,%u,%u-%u,%u\n", group->width, group->height,
				group->game_vrr_min, group->game_vrr_max, group->game_brr_vic);
		}
	}

	kfree(groups);
}

static bool meson_hdmitx_is_hdcp_running(struct am_hdmi_tx *am_hdmi)
{
	if (am_hdmi->hdcp_state == HDCP_STATE_DISCONNECT ||
		am_hdmi->hdcp_state == HDCP_STATE_STOP)
		return false;

	if (am_hdmi->hdcp_mode == HDCP_NULL)
		DRM_ERROR("hdcp mode should NOT null for state [%d]\n",
			am_hdmi->hdcp_state);

	return true;
}

static void meson_hdmitx_hdcp_work_func(struct work_struct *work)
{
	struct am_hdmi_tx *am_hdmi = container_of(work, struct am_hdmi_tx, hdcp_update_work);
	struct drm_connector *connector = &am_hdmi->base.connector;
	struct drm_modeset_lock *mode_lock = &connector->dev->mode_config.connection_mutex;

	drm_modeset_lock(mode_lock, NULL);
	drm_hdcp_update_content_protection(connector, am_hdmi->content_protection);
	drm_modeset_unlock(mode_lock);
}

static void meson_hdmitx_set_hdcp_result(struct am_hdmi_tx *am_hdmi, int result)
{
	struct drm_connector *connector = &am_hdmi->base.connector;

	if (result == HDCP_AUTH_OK) {
		am_hdmi->hdcp_state = HDCP_STATE_SUCCESS;
		am_hdmi->content_protection = DRM_MODE_CONTENT_PROTECTION_ENABLED;
		schedule_work(&am_hdmi->hdcp_update_work);
		DRM_DEBUG_KMS("hdcp [%d] set result ok.\n", am_hdmi->hdcp_mode);
	} else if (result == HDCP_AUTH_FAIL) {
		am_hdmi->hdcp_state = HDCP_STATE_FAIL;
		/*no event needed when fail.*/
		DRM_ERROR("hdcp [%d] set result fail.\n", am_hdmi->hdcp_mode);
	} else if (result == HDCP_AUTH_UNKNOWN) {
		/*reset property value to DESIRED.*/
		if (connector->state &&
			connector->state->content_protection ==
			DRM_MODE_CONTENT_PROTECTION_ENABLED) {
			am_hdmi->content_protection = DRM_MODE_CONTENT_PROTECTION_DESIRED;
			schedule_work(&am_hdmi->hdcp_update_work);
		}
	}
}

static void meson_hdmitx_start_hdcp(struct am_hdmi_tx *am_hdmi, int hdcp_mode)
{
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);

	if (hdcp_mode == HDCP_NULL)
		return;

	am_hdmi->hdcp_mode = hdcp_mode;
	am_hdmi->hdcp_state = HDCP_STATE_START;
	drm_hdmitx_common_hdcp_enable(tx_comm, hdcp_mode);
	DRM_DEBUG_KMS("start hdcp [%d-%d]...\n",
		am_hdmi->hdcp_request_content_type, am_hdmi->hdcp_mode);
}

static void meson_hdmitx_stop_hdcp(struct am_hdmi_tx *am_hdmi)
{
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);

	if (meson_hdmitx_is_hdcp_running(am_hdmi)) {
		drm_hdmitx_common_hdcp_disable(tx_comm);
		am_hdmi->hdcp_state = HDCP_STATE_STOP;
		am_hdmi->hdcp_mode = HDCP_NULL;
		meson_hdmitx_set_hdcp_result(am_hdmi, HDCP_AUTH_UNKNOWN);
	}
}

static void meson_hdmitx_disconnect_hdcp(struct am_hdmi_tx *am_hdmi)
{
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);

	if (meson_hdmitx_is_hdcp_running(am_hdmi)) {
		drm_hdmitx_common_hdcp_disconnect(tx_comm);
		am_hdmi->hdcp_state = HDCP_STATE_DISCONNECT;
		am_hdmi->hdcp_mode = HDCP_NULL;
		meson_hdmitx_set_hdcp_result(am_hdmi, HDCP_AUTH_UNKNOWN);
	}
}

static int meson_hdmitx_get_hdcp_request(struct am_hdmi_tx *am_hdmi,
	int request_type_mask)
{
	int type;
	unsigned int hdcp_rx_type = am_hdmi->hdcp_rx_type;
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	unsigned int hdcp_tx_type = drm_hdmitx_common_get_tx_hdcp_cap(tx_comm);

	/*
	 * for bootup case, some project not have tee notify to load hdcp key,
	 * need to get rx_cap by hdmitx api.
	 * for hotplug case, rx_cap is updated in hpd callback.
	 */
	if (hdcp_rx_type == 0)
		hdcp_rx_type = drm_hdmitx_common_get_rx_hdcp_cap(tx_comm);

	DRM_INFO("%s usr_type: %d, hdcp cap: %d,%d\n",
			__func__, request_type_mask,
			hdcp_tx_type, hdcp_rx_type);

	switch (hdcp_tx_type & 0x3) {
	case 0x3:
		if ((hdcp_rx_type & 0x2) && (request_type_mask & 0x2))
			type = HDCP_MODE22;
		else if ((hdcp_rx_type & 0x1) && (request_type_mask & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	case 0x2:
		if ((hdcp_rx_type & 0x2) && (request_type_mask & 0x2))
			type = HDCP_MODE22;
		else
			type = HDCP_NULL;
		break;
	case 0x1:
		if ((hdcp_rx_type & 0x1) && (request_type_mask & 0x1))
			type = HDCP_MODE14;
		else
			type = HDCP_NULL;
		break;
	default:
		type = HDCP_NULL;
		DRM_INFO("[%s]: TX no hdcp key\n", __func__);
		break;
	}
	return type;
}

void meson_hdmitx_update_hdcp(struct am_hdmi_tx *am_hdmi)
{
	int hdcp_request_mode = HDCP_NULL;
	int hdcp_request_mask = HDCP_NULL;

	DRM_DEBUG_KMS("\n");

	/*Undesired, disable hdcp.*/
	if (am_hdmi->hdcp_request_content_protection == DRM_MODE_CONTENT_PROTECTION_UNDESIRED) {
		meson_hdmitx_stop_hdcp(am_hdmi);
		return;
	}

	if (am_hdmi->hdcp_request_content_type == DRM_MODE_HDCP_CONTENT_TYPE0)
		hdcp_request_mask = HDCP_MODE14 | HDCP_MODE22;
	else
		hdcp_request_mask = HDCP_MODE22;

	hdcp_request_mode = meson_hdmitx_get_hdcp_request(am_hdmi, hdcp_request_mask);

	/*mode is same, try to re-use last state*/
	if (hdcp_request_mode == am_hdmi->hdcp_mode) {
		switch (am_hdmi->hdcp_state) {
		case HDCP_STATE_START:
			DRM_INFO("waiting hdcp result.\n");
			return;
		case HDCP_STATE_SUCCESS:
			meson_hdmitx_set_hdcp_result(am_hdmi, HDCP_AUTH_OK);
			return;
		case HDCP_STATE_FAIL:
		default:
			DRM_ERROR("meet stopped hdcp stat\n");
			break;
		};
	}

	meson_hdmitx_stop_hdcp(am_hdmi);

	if (hdcp_request_mode != HDCP_NULL)
		meson_hdmitx_start_hdcp(am_hdmi, hdcp_request_mode);
	else
		DRM_ERROR("No valid hdcp mode exit, maybe hdcp havenot init.\n");
}

void meson_hdmitx_update(struct drm_connector_state *new_state,
	struct drm_connector_state *old_state)
{
	int mute_op = OFF_AVMUTE;
	struct drm_connector *connector = new_state->connector;
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct hdmitx_common *tx_comm = meson_get_hdmitx_common(connector);
	struct am_hdmitx_connector_state *old_hdmitx_state =
		to_am_hdmitx_connector_state(old_state);
	struct am_hdmitx_connector_state *new_hdmitx_state =
		to_am_hdmitx_connector_state(new_state);
	struct drm_crtc_state *old_crtc_state;
	struct drm_crtc_state *new_crtc_state;
	struct am_meson_crtc_state *old_am_crtc_state;
	struct am_meson_crtc_state *meson_crtc_state;
	struct drm_atomic_state *old_atomic_state = old_state->state;
	struct drm_crtc *crtc = new_state->crtc;

	if (crtc) {
		old_crtc_state = drm_atomic_get_old_crtc_state(old_atomic_state, crtc);
		old_am_crtc_state = to_am_meson_crtc_state(old_crtc_state);
		new_crtc_state = drm_atomic_get_new_crtc_state(old_atomic_state, crtc);
		meson_crtc_state = to_am_meson_crtc_state(new_crtc_state);

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
		if (meson_crtc_state->crtc_hdr_process_policy !=
			old_am_crtc_state->crtc_hdr_process_policy) {
			set_hdr_policy(meson_crtc_state->crtc_hdr_process_policy);
		}
#endif

		if (meson_crtc_state->force_output_type !=
			old_am_crtc_state->force_output_type) {
			set_force_output(meson_crtc_state->force_output_type);
		}
	}

	if (new_state->content_type != old_state->content_type)
		hdmitx_common_set_contenttype(tx_comm, new_state->content_type);

	mute_op = new_hdmitx_state->avmute ? SET_AVMUTE : CLR_AVMUTE;

	if (new_hdmitx_state->avmute != old_hdmitx_state->avmute)
		hdmitx_common_avmute_locked(tx_comm, mute_op, AVMUTE_PATH_DRM);

	if (new_hdmitx_state->allm_mode != old_hdmitx_state->allm_mode)
		hdmitx_common_set_allm_mode(tx_comm,
			new_hdmitx_state->allm_mode);

	if (am_hdmi->android_path)
		return;

	/*Linux only implement*/
	if (old_state->hdcp_content_type
			!= new_state->hdcp_content_type ||
		(old_state->content_protection !=
			new_state->content_protection &&
		new_state->content_protection !=
			DRM_MODE_CONTENT_PROTECTION_ENABLED)) {
		/*check hdcp property update*/
		am_hdmi->hdcp_request_content_type =
			new_state->hdcp_content_type;
		am_hdmi->hdcp_request_content_protection =
			new_state->content_protection;

		if (new_state->crtc && !drm_atomic_crtc_needs_modeset(new_state->crtc->state))
			meson_hdmitx_update_hdcp(am_hdmi);
	}
}

static void meson_hdmitx_hdcp_notify(void *data, int type, int result)
{
	struct am_hdmi_tx *am_hdmi = (struct am_hdmi_tx *)data;
	struct drm_connector *connector = &am_hdmi->base.connector;
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	struct drm_modeset_lock *mode_lock =
		&connector->dev->mode_config.connection_mutex;

	drm_modeset_lock(mode_lock, NULL);

	if (type == HDCP_KEY_UPDATE && result == HDCP_AUTH_UNKNOWN) {
		DRM_INFO("HDCP statue changed, need re-run hdcp\n");
		if (hdmitx_get_hpd_state(tx_comm))
			am_hdmi->hdcp_rx_type = drm_hdmitx_common_get_rx_hdcp_cap(tx_comm);
		if (!am_hdmi->hdmitx_on)
			goto end;
		meson_hdmitx_update_hdcp(am_hdmi);
		goto end;
	}

	if (!am_hdmi->hdmitx_on)
		goto end;

	if (type != am_hdmi->hdcp_mode) {
		DRM_DEBUG_KMS("notify type is mismatch[%d]-[%d]\n",
			type, am_hdmi->hdcp_mode);
		goto end;
	}

	if (result == HDCP_AUTH_OK) {
		meson_hdmitx_set_hdcp_result(am_hdmi, HDCP_AUTH_OK);
	} else if (result == HDCP_AUTH_FAIL) {
		if (type == HDCP_MODE14) {
			meson_hdmitx_set_hdcp_result(am_hdmi, HDCP_AUTH_FAIL);
		} else if (type == HDCP_MODE22) {
			if (am_hdmi->hdcp_request_content_type == DRM_MODE_HDCP_CONTENT_TYPE0) {
				DRM_INFO("ContentType0 hdcp 22 -> hdcp14.\n");
				meson_hdmitx_stop_hdcp(am_hdmi);
				meson_hdmitx_start_hdcp(am_hdmi, HDCP_MODE14);
			} else {
				meson_hdmitx_set_hdcp_result(am_hdmi, HDCP_AUTH_FAIL);
			}
		}
	} else {
		DRM_ERROR("HDCP report unknown result [%d-%d]\n", type, result);
	}
end:
	drm_modeset_unlock(mode_lock);
	return;
}

static const struct drm_connector_helper_funcs am_hdmi_connector_helper_funcs = {
	.get_modes = meson_hdmitx_get_modes,
	.mode_valid = meson_hdmitx_check_mode,
	.atomic_check	= meson_hdmitx_atomic_check,
	.best_encoder = meson_hdmitx_best_encoder,
};

static const struct drm_connector_funcs am_hdmi_connector_funcs = {
	.detect			= am_hdmitx_connector_detect,
	.fill_modes		= drm_helper_probe_single_connector_modes,
	.atomic_set_property	= am_hdmitx_connector_atomic_set_property,
	.atomic_get_property	= am_hdmitx_connector_atomic_get_property,
	.destroy		= am_hdmitx_connector_destroy,
	.reset			= meson_hdmitx_reset,
	.atomic_duplicate_state	= meson_hdmitx_atomic_duplicate_state,
	.atomic_destroy_state	= meson_hdmitx_atomic_destroy_state,
	.atomic_print_state = meson_hdmitx_atomic_print_state,
};

static int meson_hdmitx_update_dv_eotf(struct am_hdmi_tx *am_hdmi, struct drm_display_mode *mode,
	u8 *crtc_eotf_type)
{
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	const struct dv_info *dvcap = NULL;
	u8 dv_types = 0;
	u8 eotf_type = *crtc_eotf_type;

	hdmitx_set_hdr_priority(tx_comm, tx_comm->hdr_priority,
			&am_hdmi->hdr_info, &am_hdmi->dv_info);
	dvcap = &am_hdmi->dv_info;

	if (dvcap->ieeeoui != DV_IEEE_OUI || dvcap->block_flag != CORRECT)
		return -ENODEV;

	DRM_DEBUG_KMS("Mode %s,%d\n", mode->name, mode->flags);
	if (mode->flags & DRM_MODE_FLAG_INTERLACE)
		return -EINVAL;

	if (strstr(mode->name, "2160p60hz") ||
	    strstr(mode->name, "2160p50hz")) {
		if (!dvcap->sup_2160p60hz)
			return -ERANGE;
	}

	/*refer to sink_dv_support()*/
	if (dvcap->ver == 2) {
		if (dvcap->Interface <= 1)
			dv_types = 2; /* LL only */
		else
			dv_types = 3; /* STD & LL */
	} else if (dvcap->low_latency) {
		dv_types = 3; /* STD & LL */
	} else {
		dv_types = 1; /* STD only */
	}

	/*When pref mode not supported, update to other mode.*/
	if (eotf_type == HDMI_EOTF_MESON_DOLBYVISION_LL &&
		!(dv_types & 2)) {
		eotf_type = HDMI_EOTF_MESON_DOLBYVISION;
	}
	if (eotf_type == HDMI_EOTF_MESON_DOLBYVISION &&
		!(dv_types & 1)) {
		eotf_type = HDMI_EOTF_MESON_DOLBYVISION_LL;
	}

	if (*crtc_eotf_type != eotf_type) {
		DRM_INFO("[%s] change eotf [%u]->[%u]\n",
			__func__, *crtc_eotf_type, eotf_type);
		*crtc_eotf_type = eotf_type;
	}

	return 0;
}

/*check hdr10&hlg cap*/
static int meson_hdmitx_update_hdr_eotf(struct am_hdmi_tx *am_hdmi, struct drm_display_mode *mode,
	u8 *crtc_eotf_type)
{
	/*refer to sink_hdr_support()*/
	const u8 hdr10_bit = BIT(2);
	const u8 hlg_bit = BIT(3);
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	const struct hdr_info *hdrcap = NULL;

	hdmitx_set_hdr_priority(tx_comm, tx_comm->hdr_priority,
			&am_hdmi->hdr_info, &am_hdmi->dv_info);
	hdrcap = &am_hdmi->hdr_info;

	/*hdr core can support all the hdr types.*/
	if ((hdrcap->hdr_support & hdr10_bit) ||
		(hdrcap->hdr_support & hlg_bit) ||
		(hdrcap->hdr10plus_info.ieeeoui == HDR10_PLUS_IEEE_OUI &&
		hdrcap->hdr10plus_info.application_version == 1)) {
		return 0;
	}

	return -ENODEV;
}

static int meson_hdmitx_decide_eotf_type
	(struct am_hdmi_tx *am_hdmi, struct am_meson_crtc_state *meson_crtc_state,
	struct am_hdmitx_connector_state *hdmitx_state)
{
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	u8 crtc_eotf_type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	int ret = 0;
	struct drm_display_mode *mode = &meson_crtc_state->base.adjusted_mode;

	/*If the HDMI cable is not plugin before starting the board,pref_hdr_policy
	 *may not be available in am_meson_update_output_state() function
	 */
	hdmitx_state->pref_hdr_policy = tx_comm->hdr_priority;

	/* TODO:hdr priority handled by hdmitx, and dont support dynamic set.
	 * Currently checking is to confirm crtc_eotf_type == dv/dv_ll mode,
	 * we need special setting for dv.
	 */
	if (hdmitx_state->pref_hdr_policy == MESON_PREF_DV) {
		if (meson_crtc_state->dv_mode)
			crtc_eotf_type = HDMI_EOTF_MESON_DOLBYVISION_LL;
		else
			crtc_eotf_type = HDMI_EOTF_MESON_DOLBYVISION;
	} else if (hdmitx_state->pref_hdr_policy == MESON_PREF_HDR) {
		crtc_eotf_type = HDMI_EOTF_SMPTE_ST2084;
	} else {
		crtc_eotf_type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
	}

	DRM_DEBUG_KMS("default eotf [%u]\n", crtc_eotf_type);

	if (crtc_eotf_type == HDMI_EOTF_MESON_DOLBYVISION ||
	    crtc_eotf_type == HDMI_EOTF_MESON_DOLBYVISION_LL) {
		/*check if dv core valid*/
		if (meson_crtc_state->crtc_dv_enable)
			ret = 0;
		else
			ret = -ENODEV;

		/*check dv cap & mode */
		if (ret == 0)
			ret = meson_hdmitx_update_dv_eotf(am_hdmi, mode,
				&crtc_eotf_type);
		if (ret < 0) {
			crtc_eotf_type = HDMI_EOTF_SMPTE_ST2084;/*try hdr10*/
			DRM_DEBUG_KMS("hdmitx dv eotf check fail [%d]\n", ret);
		}

		DRM_DEBUG_KMS("dv check dv eotf finish => [%u]\n",
		crtc_eotf_type);
	}

	if (crtc_eotf_type == HDMI_EOTF_SMPTE_ST2084) {
		/*check if hdr core valid*/
		if (meson_crtc_state->crtc_hdr_enable)
			ret = 0;
		else
			ret = -ENODEV;

		if (ret == 0)
			ret = meson_hdmitx_update_hdr_eotf(am_hdmi, mode,
				&crtc_eotf_type);
		if (ret < 0) {
			crtc_eotf_type = HDMI_EOTF_TRADITIONAL_GAMMA_SDR;
			DRM_INFO("hdmitx hdr eotf check fail [%d]\n", ret);
		}

		DRM_DEBUG_KMS("HDR10 check eotf => [%u]\n",
			crtc_eotf_type);
	}

	meson_crtc_state->crtc_eotf_type = crtc_eotf_type;

	DRM_DEBUG_KMS("[%u->%u]\n",
		hdmitx_state->pref_hdr_policy,
		meson_crtc_state->crtc_eotf_type);

	return 0;
}

static void meson_hdmitx_cal_brr(struct am_hdmi_tx *am_hdmi,
				 struct am_meson_crtc_state *crtc_state,
				 struct drm_display_mode *adj_mode)
{
	int i, vic, brr = 60;
	int num_group;
	struct hdmitx_vrr_mode_group *group;
	char mode_name[DRM_DISPLAY_MODE_LEN];
	struct hdmitx_vrr_mode_group *groups;
	const struct hdmi_timing *timing;
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);

	groups = kcalloc(MAX_VRR_MODE_GROUP, sizeof(*groups), GFP_KERNEL);
	if (!groups) {
		DRM_ERROR("alloc fail\n");
		return;
	}

	num_group = hdmitx_common_get_vrr_mode_group(tx_comm, groups, MAX_VRR_MODE_GROUP);

	vic = HDMI_0_UNKNOWN;

	for (i = 0; i < num_group; i++) {
		group = &groups[i];
		if (group->width == adj_mode->hdisplay &&
		    group->height == adj_mode->vdisplay) {
			if (crtc_state->vrr_type == DRM_VRR_QMS &&
			    group->vrr_max / VRR_DIV >= brr) {
				brr = group->vrr_max / VRR_DIV;
				vic = group->brr_vic;
				am_hdmi->min_vfreq = group->vrr_min / VRR_DIV;
				am_hdmi->max_vfreq = group->vrr_max / VRR_DIV;
			}

			if (crtc_state->vrr_type == DRM_VRR_GAME &&
			    group->game_vrr_max / VRR_DIV >= brr) {
				brr = group->game_vrr_max / VRR_DIV;
				vic = group->game_brr_vic;
				am_hdmi->min_vfreq = group->game_vrr_min / VRR_DIV;
				am_hdmi->max_vfreq = group->game_vrr_max / VRR_DIV;
			}
		}
	}

	timing = hdmitx_mode_vic_to_hdmi_timing(vic);
	if (!timing) {
		DRM_ERROR("Get timing by vic [%d] failed.\n", vic);
		return;
	}

	if (timing->sname) {
		memcpy(mode_name, timing->sname,
			   (strlen(timing->sname) < DRM_DISPLAY_MODE_LEN) ?
			   strlen(timing->sname) : DRM_DISPLAY_MODE_LEN);
	} else {
		memcpy(mode_name, timing->name,
			   (strlen(timing->name) < DRM_DISPLAY_MODE_LEN) ?
			   strlen(timing->name) : DRM_DISPLAY_MODE_LEN);
	}

	mode_name[DRM_DISPLAY_MODE_LEN - 1] = '\0';

	if (am_hdmi->max_vfreq < drm_mode_vrefresh(adj_mode)) {
		memset(crtc_state->brr_mode, 0, DRM_DISPLAY_MODE_LEN);
		crtc_state->valid_brr = 0;
	} else {
		strncpy(crtc_state->brr_mode, mode_name,
			DRM_DISPLAY_MODE_LEN);
		crtc_state->brr_mode[DRM_DISPLAY_MODE_LEN - 1] = '\0';
		crtc_state->valid_brr = 1;
	}

	DRM_DEBUG_KMS("%d, %d, %s, %d\n",
		vic, brr, crtc_state->brr_mode, crtc_state->valid_brr);
	crtc_state->brr = brr;
	kfree(groups);
}

/*Calculate preset_mode and eotf_type before enable crtc&encoder.*/
void meson_hdmitx_encoder_atomic_mode_set(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(conn_state);
	struct hdmitx_color_attr *attr = &hdmitx_state->color_attr_para;
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;
	struct am_hdmi_tx *am_hdmi = encoder_to_am_hdmi(encoder);
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(crtc_state->crtc);
	char *modename = adj_mode->name;
	struct meson_connector_dev *hdmitx_dev = am_hdmi->hdmitx_dev;

	DRM_DEBUG_KMS("[%d]: enter\n", __LINE__);

	update_curr_vout_server(amcrtc->vout_index, hdmitx_dev->vout_serv);
	meson_crtc_state->viu_mux = vout_func_get_viu_mux(amcrtc->vout_index,
		hdmitx_dev->vout_serv, modename);

	if (am_hdmi->android_path)
		return;

	DRM_INFO("%s enter:attr[%d-%d]\n", __func__,
		attr->colorformat, attr->bitdepth);
	meson_hdmitx_decide_eotf_type(am_hdmi, meson_crtc_state, hdmitx_state);
}

static
int meson_encoder_vrr_change(struct drm_encoder *encoder,
			     struct drm_atomic_state *state)
{
	struct drm_connector *connector;
	struct drm_connector_state *conn_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *new_state, *old_state;
	struct drm_display_mode *new_mode, *old_mode;
	struct am_meson_crtc_state *meson_crtc_state, *old_crtc_state;
	char *brr_name;

	connector = drm_atomic_get_new_connector_for_encoder(state, encoder);
	if (!connector)
		return 0;

	conn_state = drm_atomic_get_new_connector_state(state, connector);
	if (!conn_state)
		return 0;

	crtc = conn_state->crtc;
	new_state = drm_atomic_get_new_crtc_state(state, crtc);
	old_state = drm_atomic_get_old_crtc_state(state, crtc);

	if (!new_state || !old_state) {
		DRM_INFO("%s crtc state is NULL!\n", __func__);
		return 0;
	}
	new_mode = &new_state->adjusted_mode;
	old_mode = &old_state->adjusted_mode;
	meson_crtc_state = to_am_meson_crtc_state(new_state);
	old_crtc_state = to_am_meson_crtc_state(old_state);

	if (new_mode->hdisplay != old_mode->hdisplay ||
		new_mode->vdisplay != old_mode->vdisplay ||
		meson_crtc_state->attr_changed ||
		meson_crtc_state->brr_update ||
		(old_mode->flags & DRM_MODE_FLAG_INTERLACE) ||
		(new_mode->flags & DRM_MODE_FLAG_INTERLACE)) {
		DRM_DEBUG_KMS("%s non-seamless\n", __func__);
		return meson_crtc_state->seamless;
	}

	if (new_state->vrr_enabled) {
		if (old_state->vrr_enabled) {
			/*qms->qms*/
			meson_crtc_state->seamless = true;
		} else {
			/*allm -> qms, new vrr_enable 1, brr_update 0*/
			brr_name = meson_crtc_state->brr_mode;
			if (!strcmp(old_mode->name, brr_name))
				meson_crtc_state->seamless = true;
			else
				meson_crtc_state->seamless = false;
		}
	} else {
		if (old_state->vrr_enabled) {
			/*qms->allm*/
			brr_name = old_crtc_state->brr_mode;
			if (!strcmp(old_mode->name, brr_name) &&
				!strcmp(new_mode->name, brr_name))
				meson_crtc_state->seamless = true;
			else
				meson_crtc_state->seamless = false;
		} else {
			/*none qms-> none qms*/
			meson_crtc_state->seamless = false;
		}
	}

	DRM_DEBUG_KMS("seamless is %d\n", meson_crtc_state->seamless);
	return meson_crtc_state->seamless;
}

void meson_hdmitx_encoder_atomic_enable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_hdmi_tx *am_hdmi = encoder_to_am_hdmi(encoder);
	struct drm_connector *conn = &am_hdmi->base.connector;
	struct vrr_setting_info vrr_info;
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(encoder->crtc->state);
	struct drm_connector_state *conn_state =
		drm_atomic_get_new_connector_state(state, conn);
	struct drm_connector_state *old_conn_state =
		drm_atomic_get_old_connector_state(state, conn);
	struct am_hdmitx_connector_state *meson_conn_state =
		to_am_hdmitx_connector_state(conn_state);
	struct am_hdmitx_connector_state *old_meson_conn_state =
		to_am_hdmitx_connector_state(old_conn_state);
	struct drm_display_mode *mode = &encoder->crtc->state->adjusted_mode;
	int dst_vrefresh, mode_vrefresh = drm_mode_vrefresh(mode);
	struct am_meson_crtc *amcrtc = to_am_meson_crtc(encoder->crtc);
	enum vmode_e vmode = meson_crtc_state->vmode;
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	bool is_alter;

	DRM_DEBUG_KMS("[%d]\n", __LINE__);

	if ((vmode & VMODE_MODE_BIT_MASK) != VMODE_HDMI) {
		DRM_INFO("[%s] skip vmode[%d]\n", __func__, vmode);
		return;
	}

	is_alter = meson_hdmitx_is_alter_mode(mode);
	if (is_alter) {
		vrr_info.frac_mode = true;
	} else {
		vrr_info.frac_mode = false;
	}

	if (meson_crtc_state->uboot_mode_init == 1 &&
		meson_conn_state->update != 1)
		vmode |= VMODE_INIT_BIT_MASK;

	meson_conn_state->hcs.mode = vmode;
	if (meson_crtc_state->seamless) {
		if (meson_crtc_state->vrr_type == DRM_VRR_QMS) {
			dst_vrefresh = meson_crtc_state->base.vrr_enabled ? mode_vrefresh : 0;
			dst_vrefresh *= 100;
			vrr_info.type = T_VRR_QMS;
		} else if (meson_crtc_state->vrr_type == DRM_VRR_GAME) {
			dst_vrefresh = meson_crtc_state->game_rate;
			vrr_info.type = T_VRR_GAME;
		} else {
			dst_vrefresh = 0;
			vrr_info.type = T_VRR_NONE;
		}

		DRM_INFO("%s, set frame rate: %d\n", __func__, dst_vrefresh);
		hdmitx_common_set_vframe_rate_hint(tx_comm, &meson_conn_state->hcs,
				dst_vrefresh, &vrr_info);
		return;
	}

	if (!meson_crtc_state->seamless) {
		hdmitx_set_hdr_priority(tx_comm,
					meson_conn_state->hdr_priority,
					&am_hdmi->hdr_info,
					&am_hdmi->dv_info);

		meson_vout_notify_mode_change(amcrtc->vout_index,
					      vmode, EVENT_MODE_SET_START);
		hdmitx_common_do_mode_setting(tx_comm,
					      &meson_conn_state->hcs,
					      &old_meson_conn_state->hcs);
		meson_vout_notify_mode_change(amcrtc->vout_index,
					      vmode, EVENT_MODE_SET_FINISH);
		meson_vout_update_mode_name(amcrtc->vout_index, mode->name, "hdmitx");
	}

	am_hdmi->hdmitx_on = 1;

	if (!am_hdmi->android_path) {
		hdmitx_common_avmute_locked(tx_comm, CLR_AVMUTE, AVMUTE_PATH_DRM);
		meson_hdmitx_update_hdcp(am_hdmi);
	}

	if (meson_crtc_state->base.vrr_enabled) {
		if (meson_crtc_state->vrr_type == DRM_VRR_QMS) {
			dst_vrefresh = mode_vrefresh * 100;
			vrr_info.type = T_VRR_QMS;
		} else if (meson_crtc_state->vrr_type == DRM_VRR_GAME) {
			dst_vrefresh = meson_crtc_state->game_rate;
			vrr_info.type = T_VRR_GAME;
		} else {
			dst_vrefresh = 0;
			vrr_info.type = T_VRR_NONE;
		}

		hdmitx_common_set_vframe_rate_hint(tx_comm, &meson_conn_state->hcs,
				dst_vrefresh, &vrr_info);
		DRM_INFO("%s, vrr set rate hint, %d\n", __func__,
			 dst_vrefresh);
	} else {
		vrr_info.type = T_VRR_NONE;
		hdmitx_common_set_vframe_rate_hint(tx_comm, &meson_conn_state->hcs, 0, &vrr_info);
		DRM_INFO("%s, disable vrr\n", __func__);
	}
}

void meson_hdmitx_encoder_atomic_disable(struct drm_encoder *encoder,
	struct drm_atomic_state *state)
{
	struct am_hdmi_tx *am_hdmi = encoder_to_am_hdmi(encoder);
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	struct hdmitx_hw_common *hw_comm = tx_comm->tx_hw;
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(encoder->crtc->state);

	if (meson_crtc_state->seamless)
		return;

	DRM_INFO("[%s]\n", __func__);

	if (am_hdmi->android_path ||
		meson_crtc_state->uboot_mode_init == 1)
		return;

	am_hdmi->hdmitx_on = 0;
	hdmitx_common_avmute_locked(tx_comm, SET_AVMUTE, AVMUTE_PATH_DRM);
	msleep(100);
	/*
	 * there's about 300ms delay after hdmitx encoder disable and
	 * before hdmitx_module_disable(disable phy), need to disable
	 * hdmitx phy asap for TV better detection
	 */
	hdmitx_hw_set_phy(hw_comm, 0);
	meson_hdmitx_stop_hdcp(am_hdmi);
	msleep(100);
}

/*
 * The flow matching mode + attr was originally in atomic_mode_set,
 * now put this flow into encoder_atomic_check.
 */
static int meson_hdmitx_encoder_autoselect_attr(struct drm_encoder *encoder,
	struct drm_crtc_state *crtc_state,
	struct drm_connector_state *conn_state)
{
	enum hdmi_vic vic;
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(conn_state);
	struct hdmitx_color_attr *attr = &hdmitx_state->color_attr_para;
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;
	struct am_hdmi_tx *am_hdmi = encoder_to_am_hdmi(encoder);
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	bool update_attr = false;
	char *modename = adj_mode->name;
	int ret = 0;
	u64 sequence_id = hdmitx_state->hcs.state_sequence_id;

	vic = hdmitx_common_parse_vic_in_edid(tx_comm, modename);
	if (vic == HDMI_0_UNKNOWN) {
		DRM_ERROR("invalid vic for %s\n", modename);
		return -EINVAL;
	}

	meson_hdmitx_decide_eotf_type(am_hdmi, meson_crtc_state, hdmitx_state);

	if (hdmitx_state->color_force) {
		if (!hdmitx_common_validate_mode_locked(tx_comm, &hdmitx_state->hcs,
							modename, attr->colorformat,
				bitdepth_to_colordepth(attr->bitdepth), 1)) {
			DRM_DEBUG_KMS("color property setting successfully\n");
		} else {
			hdmitx_state->color_force = false;
			DRM_DEBUG_KMS("color property setting failed\n");
		}
	}

	if (!hdmitx_state->color_force) {
		if (attr->colorformat != HDMI_COLORSPACE_RESERVED6) {
			if (meson_hdmitx_test_color_attr(am_hdmi, meson_crtc_state,
				attr, sequence_id)) {
				update_attr = false;
			} else {
				update_attr = true;
				DRM_DEBUG_KMS("force attr fail[%d-%d]\n",
					attr->colorformat, attr->bitdepth);
			}
		} else {
			update_attr = true;
		}

		if (update_attr) {
			meson_hdmitx_decide_color_attr(am_hdmi, meson_crtc_state,
				attr, sequence_id);
			hdmitx_state->update = true;
		}
	}

	ret = hdmitx_common_build_format_para(tx_comm, &hdmitx_state->hcs.para,
					      vic, tx_comm->fmt_para.frac_mode,
					      attr->colorformat,
					      bitdepth_to_colordepth(attr->bitdepth),
					      HDMI_QUANTIZATION_RANGE_FULL);
	if (ret < 0)
		DRM_ERROR("format para build fail\n");

	DRM_DEBUG_KMS("driver autoselect attr:[%s][%d][%d]\n",
		hdmitx_state->hcs.para.name, hdmitx_state->hcs.para.cs, hdmitx_state->hcs.para.cd);

	return ret;
}

static bool meson_hdmitx_is_alter_mode(struct drm_display_mode *mode)
{
	u8 vic;
	struct drm_display_mode vic_mode = {0};

	mode->hskew = 0;
	vic = drm_match_cea_mode(mode);
	if (vic) {
		meson_hdmitx_convert_timing_para(vic, &vic_mode, false);
		DRM_DEBUG_KMS("vic-%d, name-%s, %s\n", vic, mode->name, vic_mode.name);

		if (drm_mode_vrefresh(mode) % 6 == 0 &&
		    cea_mode_alternate_clock(&vic_mode) == mode->clock) {
			strncpy(mode->name, vic_mode.name, DRM_DISPLAY_MODE_LEN);

			DRM_DEBUG_KMS("match ok, vic-%d, name-%s, %s\n",
						vic, mode->name, vic_mode.name);
			return true;
		}
		return false;
	}

	DRM_ERROR("Invalid Modeline " DRM_MODE_FMT "\n", DRM_MODE_ARG(mode));
	return false;
}

static int meson_hdmitx_encoder_atomic_check(struct drm_encoder *encoder,
					     struct drm_crtc_state *crtc_state,
					     struct drm_connector_state *conn_state)
{
	struct am_hdmi_tx *am_hdmi = encoder_to_am_hdmi(encoder);
	struct am_meson_crtc_state *meson_crtc_state =
		to_am_meson_crtc_state(crtc_state);
	struct am_hdmitx_connector_state *hdmitx_state =
		to_am_hdmitx_connector_state(conn_state);
	struct hdmitx_color_attr *attr = &hdmitx_state->color_attr_para;
	struct drm_display_mode *adj_mode = &crtc_state->adjusted_mode;
	char *modename = adj_mode->name;
	struct hdmitx_common *common = to_hdmitx_common(am_hdmi->hdmitx_dev);
	u64 sequence_id = hdmitx_state->hcs.state_sequence_id;
	int ret = 0;
	bool is_alter;
	u32 brr = 0, qms_en = 0;

	/* do not atomic check if hpd is low*/
	if (strstr(modename, "dummy") || !hdmitx_get_hpd_state(common))
		return 0;

	is_alter = meson_hdmitx_is_alter_mode(adj_mode);
	if (is_alter) {
		hdmitx_state->hcs.para.frac_mode = true;
		meson_crtc_state->frac = 1;
	} else {
		hdmitx_state->hcs.para.frac_mode = false;
		meson_crtc_state->frac = 0;
	}

	if (meson_crtc_state->uboot_mode_init == 1) {
		DRM_INFO("%s[%d]\n", __func__, __LINE__);
		hdmitx_get_qms_init_state(common, &brr, &qms_en);
		if (brr && qms_en)
			crtc_state->vrr_enabled = true;
	}

	if (crtc_state->vrr_enabled &&
		!(adj_mode->flags & DRM_MODE_FLAG_INTERLACE)) {
		meson_hdmitx_cal_brr(am_hdmi, meson_crtc_state, adj_mode);
		modename = meson_crtc_state->brr_mode;
	}

	meson_encoder_vrr_change(encoder, conn_state->state);

	DRM_DEBUG_KMS("[%d]: enter\n", __LINE__);

	if (meson_crtc_state->uboot_mode_init == 1) {
		common->fmt_para.frac_mode = hdmitx_state->hcs.para.frac_mode;
		DRM_INFO("%s[%d] uboot get: %d\n", __func__, __LINE__, common->fmt_para.frac_mode);
		hdmitx_get_init_state(common, &hdmitx_state->hcs);
		attr->colorformat = hdmitx_state->hcs.para.cs;
		attr->bitdepth = colordepth_to_bitdepth(hdmitx_state->hcs.para.cd);
		hdmitx_state->hdr_priority = hdmitx_state->hcs.hdr_priority;
	}

	/*The recovery mode not have composer to set attr*/
	if (!meson_crtc_state->uboot_mode_init && am_hdmi->recovery_mode)
		meson_hdmitx_decide_color_attr(am_hdmi, meson_crtc_state,
						 attr, sequence_id);

	ret = hdmitx_common_validate_mode_locked(common, &hdmitx_state->hcs,
						 modename, attr->colorformat,
						 bitdepth_to_colordepth(attr->bitdepth),
						 meson_crtc_state->valid_brr);
	/*
	 * ret == 0
	 * mode and attr are supported, don't need to match mode and attr
	 * ret != 0
	 * RDK without mode policy, Android or Yocto mode policy selects an wrong mode + attr,
	 * need to match mode and attr
	 */
	if (ret) {
		ret = meson_hdmitx_encoder_autoselect_attr(encoder, crtc_state, conn_state);
	}

	if (!ret)
		meson_crtc_state->preset_vmode = VMODE_HDMI;
	else
		DRM_ERROR("validate fail %d\n", ret);

	DRM_DEBUG_KMS("vic:%d, cs:%d, cd:%d frac:%d ret:%d\n", hdmitx_state->hcs.para.vic,
		 hdmitx_state->hcs.para.cs, hdmitx_state->hcs.para.cd,
		 hdmitx_state->hcs.para.frac_mode, ret);

	return ret;
}

static const struct drm_encoder_helper_funcs meson_hdmitx_encoder_helper_funcs = {
	.atomic_mode_set	= meson_hdmitx_encoder_atomic_mode_set,
	.atomic_enable		= meson_hdmitx_encoder_atomic_enable,
	.atomic_disable		= meson_hdmitx_encoder_atomic_disable,
	.atomic_check		= meson_hdmitx_encoder_atomic_check,
};

static const struct drm_encoder_funcs meson_hdmitx_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

static const struct of_device_id am_meson_hdmi_dt_ids[] = {
	{ .compatible = "amlogic, drm-amhdmitx", },
	{},
};

MODULE_DEVICE_TABLE(of, am_meson_hdmi_dt_ids);

struct meson_range_prop_info {
	u64 init_val;
	u64 min;
	u64 max;
	u32 flags;
	char *prop_name;
};

static struct meson_range_prop_info range_props[] = {
	{0, 0, 1, 0, "hdcp_user"},
	{0, 0, 1, 0, "hdcp_topo"},
	{0, 0, 36, 0, "hdcp_ver"},
	{0, 0, 36, 0, "hdcp_mode"},
	{0, 0, 64, 0, "hdr_cap"},
	{0, 0, 64, 0, "hdr_cap_rx"},
	{0, 0, 512, 0, "dv_cap"},
	{0, 0, 512, 0, "dv_cap_rx"},
	{0, 0,  1 << COLOR_MAX_ATTR, 0, "dc_cap"},
	{0, 0, 1023, 0, "contenttype_cap"},
	{0, 0, 1, 0, "allm_cap"},
	{0, 0, 3, 0, "allm"},
	{0, 0, 1, 0, "UPDATE"},
	{0, 0, 1, 0, "MESON_DRM_HDMITX_PROP_AVMUTE"},
	{0, 0, 1, 0, "ready"},
	{0, 0, 1, 0, "FRAC_RATE_POLICY"},
	{0, 0, 1, 0, "hdmi_used"},
	{0, 0, 8, 0, "sink_type"},
	{0, 0, 1, 0, "edid_valid"},
	{0, 0, 3, 0, "vrr_capable_type"},
	{0, 0, 1, 0, "sink_device_type"},
};

static void meson_hdmitx_create_range_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	int i;
	struct meson_range_prop_info *info;
	struct drm_property *prop;
	struct drm_property **props = &am_hdmi->hdcp_user_prop;

	for (i = 0; i < ARRAY_SIZE(range_props); i++, props++) {
		info = &range_props[i];
		prop = drm_property_create_range(drm_dev, info->flags, info->prop_name,
						 info->min, info->max);
		if (prop) {
			*(props) = prop;
			drm_object_attach_property(&am_hdmi->base.connector.base, prop,
						   info->init_val);
		} else {
			DRM_ERROR("Failed to %s property\n", info->prop_name);
		}
	}
}

static const struct drm_prop_enum_list hdmi_hdr_status_enum_list[] = {
	{ HDR10PLUS_VSIF, "HDR10Plus-VSIF" },
	{ DOLBYVISION_STD, "DolbyVision-Std" },
	{ DOLBYVISION_LOWLATENCY, "DolbyVision-Lowlatency" },
	{ HDR10_GAMMA_ST2084, "HDR10-GAMMA_ST2084" },
	{ HDR10_OTHERS, "HDR10-others" },
	{ HDR10_GAMMA_HLG, "HDR10-GAMMA_HLG" },
	{ SDR, "SDR" }
};

static void meson_hdmitx_init_hdmi_hdr_status_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0, "hdmi_hdr_status",
					hdmi_hdr_status_enum_list,
					ARRAY_SIZE(hdmi_hdr_status_enum_list));
	if (prop) {
		am_hdmi->hdmi_hdr_status_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to hdmi_hdr_status property\n");
	}
}

/* Optional colorspace properties. */
static const struct drm_prop_enum_list hdmi_color_space_enum_list[] = {
	{ HDMI_COLORSPACE_RGB, "RGB" },
	{ HDMI_COLORSPACE_YUV422, "422" },
	{ HDMI_COLORSPACE_YUV444, "444" },
	{ HDMI_COLORSPACE_YUV420, "420" },
	{ HDMI_COLORSPACE_RESERVED6, "HDMI_COLORSPACE_RESERVED6" }
};

static void meson_hdmitx_init_colorspace_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	int colorformat, bitdepth;
	struct hdmitx_common *common = to_hdmitx_common(am_hdmi->hdmitx_dev);

	hdmitx_get_attr(common, &colorformat, &bitdepth);

	prop = drm_property_create_enum(drm_dev, 0, "color_space",
					hdmi_color_space_enum_list,
					ARRAY_SIZE(hdmi_color_space_enum_list));
	if (prop) {
		am_hdmi->color_space_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop,
		colorformat);
	} else {
		DRM_ERROR("Failed to color_space property\n");
	}
}

static void meson_hdmitx_init_colordepth_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	int colorformat, bitdepth;
	struct hdmitx_common *common = to_hdmitx_common(am_hdmi->hdmitx_dev);

	hdmitx_get_attr(common, &colorformat, &bitdepth);

	prop = drm_property_create_range(drm_dev, 0,
			"color_depth", 0, 16);

	if (prop) {
		am_hdmi->color_depth_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop,
		colordepth_to_bitdepth(bitdepth));
	} else {
		DRM_ERROR("Failed to color_depth property\n");
	}
}

static const struct drm_prop_enum_list hdmi_scan_info_enum_list[] = {
	{ HDMI_SCAN_MODE_NONE, "no data" },
	{ HDMI_SCAN_MODE_OVERSCAN, "overscan" },
	{ HDMI_SCAN_MODE_UNDERSCAN, "underscan" },
	{ HDMI_SCAN_MODE_RESERVED, "future" },
};

static void meson_hdmitx_init_scan_info_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create_enum(drm_dev, 0,
			"scan_info", hdmi_scan_info_enum_list,
			ARRAY_SIZE(hdmi_scan_info_enum_list));

	if (prop) {
		am_hdmi->scan_info_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to scan_info property\n");
	}
}

static void meson_hdmitx_init_hdr_priority_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;
	u32 hdr_priority;
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);

	hdmitx_get_hdr_priority(tx_comm, &hdr_priority);
	prop = drm_property_create_range(drm_dev, 0,
			"hdr_priority", 0, UINT_MAX);

	if (prop) {
		am_hdmi->hdr_priority_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, hdr_priority);
	} else {
		DRM_ERROR("Failed to allm property\n");
	}
}

static void meson_hdmitx_init_static_meta_property(struct drm_device *drm_dev,
						  struct am_hdmi_tx *am_hdmi)
{
	struct drm_property *prop;

	prop = drm_property_create(drm_dev, DRM_MODE_PROP_BLOB, "HDR_STATIC_META", 0);
	if (prop) {
		am_hdmi->static_meta_prop = prop;
		drm_object_attach_property(&am_hdmi->base.connector.base, prop, 0);
	} else {
		DRM_ERROR("Failed to create hdr static metadata property\n");
	}
}

static void meson_hdmitx_hpd_cb(void *data)
{
	struct am_hdmi_tx *am_hdmi = (struct am_hdmi_tx *)data;
	struct drm_connector *connector = &am_hdmi->base.connector;
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);
	struct drm_modeset_lock *mode_lock =
		&connector->dev->mode_config.connection_mutex;
#ifdef CONFIG_CEC_NOTIFIER
	struct edid *pedid;
	unsigned char *raw_edid = NULL;
#endif

	DRM_INFO("drm hdmitx hpd notify\n");
	if (!hdmitx_get_hpd_state(tx_comm) && !am_hdmi->android_path) {
		drm_modeset_lock(mode_lock, NULL);
		meson_hdmitx_disconnect_hdcp(am_hdmi);
		am_hdmi->hdmitx_on = 0;
		drm_modeset_unlock(mode_lock);
	}

	/*get hdcp ver property immediately after plugin in case hdcp14
	 *authentication snow screen issue
	 */
	if (hdmitx_get_hpd_state(tx_comm))
		am_hdmi->hdcp_rx_type = drm_hdmitx_common_get_rx_hdcp_cap(tx_comm);

#ifdef CONFIG_CEC_NOTIFIER
	if (hdmitx_get_hpd_state(tx_comm)) {
		DRM_DEBUG_KMS("[%d]\n", __LINE__);
		raw_edid = hdmitx_get_raw_edid(tx_comm);
		if (raw_edid) {
			pedid = (struct edid *)raw_edid;
			cec_notifier_set_phys_addr_from_edid(am_hdmi->cec_notifier,
						     pedid);
		}
	} else {
		DRM_DEBUG_KMS("[%d]\n", __LINE__);
		cec_notifier_set_phys_addr(am_hdmi->cec_notifier,
					   CEC_PHYS_ADDR_INVALID);
	}
#endif
	drm_kms_helper_hotplug_event(am_hdmi->base.connector.dev);
}

int meson_hdmitx_dev_bind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	struct meson_drm *priv = drm->dev_private;
	struct am_hdmi_tx *am_hdmi;
	struct drm_connector *connector;
	struct drm_encoder *encoder;
	struct meson_connector *mesonconn;
	struct connector_hpd_cb hpd_cb;
	struct hdmitx_common *tx_comm;
	int ret;
	int connector_type = type;
	char *connector_name = NULL;
	int encoder_type = DRM_MODE_ENCODER_TMDS;
	struct drm_property *type_prop = NULL;
#ifdef CONFIG_CEC_NOTIFIER
	struct edid *pedid;
	unsigned char *raw_edid = NULL;
#endif
	struct connector_hdcp_cb hdcp_cb;
	int hdcp_ctl_lvl;

	DRM_DEBUG_KMS("[%d] type =%d\n", __LINE__, type);
	am_hdmi = devm_kzalloc(drm->dev, sizeof(*am_hdmi), GFP_KERNEL);
	if (!am_hdmi)
		return -ENOMEM;

	am_hdmi->hdmitx_dev = intf;
	tx_comm  = to_hdmitx_common(intf);
	hdcp_ctl_lvl = tx_comm->hdcptx_comm.hdcp_ctl_lvl;
	DRM_DEBUG_KMS("hdcp_ctl_lvl=%d\n", hdcp_ctl_lvl);

	if (hdcp_ctl_lvl == 0) {
		am_hdmi->android_path = true;
	} else if (drm_hdmitx_common_hdcp_init(tx_comm) == 0) {
		if (hdcp_ctl_lvl == 1) {
			/*TODO: for westeros start hdcp by driver, will move to userspace.*/
			am_hdmi->hdcp_request_content_type =
				DRM_MODE_HDCP_CONTENT_TYPE0;
			am_hdmi->hdcp_request_content_protection =
				DRM_MODE_CONTENT_PROTECTION_DESIRED;
		} else {
			am_hdmi->hdcp_request_content_type =
				DRM_MODE_HDCP_CONTENT_TYPE0;
			am_hdmi->hdcp_request_content_protection =
				DRM_MODE_CONTENT_PROTECTION_UNDESIRED;
		}
	} else {
		DRM_ERROR("no HDCP func registered.\n");
		am_hdmi->android_path = true;
	}

	/*TODO:update compat_mode for drm driver, remove later.*/
	priv->compat_mode = am_hdmi->android_path;

#ifdef CONFIG_CEC_NOTIFIER
	//am_hdmi_info.cec_notifier = cec_notifier_conn_register(dev, NULL, NULL);
	am_hdmi->cec_notifier = NULL;
#endif

	mesonconn = &am_hdmi->base;
	mesonconn->drm_priv = priv;
	mesonconn->update = meson_hdmitx_update;
	mesonconn->connector_type = type;
	encoder = &am_hdmi->encoder;
	connector = &am_hdmi->base.connector;
	intf->conn = connector;
	am_hdmi->hdmi_type = type;
	INIT_WORK(&am_hdmi->hdcp_update_work, meson_hdmitx_hdcp_work_func);

	switch (type) {
	case DRM_MODE_CONNECTOR_MESON_HDMIA_A:
		connector_type = DRM_MODE_CONNECTOR_HDMIA;
		connector_name = "HDMI-A-A";
		break;
	case DRM_MODE_CONNECTOR_MESON_HDMIA_B:
		connector_type = DRM_MODE_CONNECTOR_HDMIA;
		connector_name = "HDMI-A-B";
		break;
	case DRM_MODE_CONNECTOR_MESON_HDMIA_C:
		connector_type = DRM_MODE_CONNECTOR_HDMIA;
		connector_name = "HDMI-A-C";
		break;
	case DRM_MODE_CONNECTOR_MESON_HDMIB_A:
		connector_type = DRM_MODE_CONNECTOR_HDMIB;
		connector_name = "HDMI-B-A";
		break;
	case DRM_MODE_CONNECTOR_MESON_HDMIB_B:
		connector_type = DRM_MODE_CONNECTOR_HDMIB;
		connector_name = "HDMI-B-B";
		break;
	case DRM_MODE_CONNECTOR_MESON_HDMIB_C:
		connector_type = DRM_MODE_CONNECTOR_HDMIB;
		connector_name = "HDMI-B-C";
		break;
	default:
		connector_type = DRM_MODE_CONNECTOR_Unknown;
		encoder_type = DRM_MODE_ENCODER_NONE;
		break;
	};

	/* Connector */
	connector->polled = DRM_CONNECTOR_POLL_HPD;
	connector->ycbcr_420_allowed = true;
	connector->force = DRM_FORCE_ON;
	drm_connector_helper_add(connector,
				 &am_hdmi_connector_helper_funcs);

	ret = drm_connector_init(drm, connector, &am_hdmi_connector_funcs,
				 connector_type);
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi tx connector\n");
		return ret;
	}
	connector->interlace_allowed = 1;

	/*update name to amlogic name*/
	if (connector_name) {
		kfree(connector->name);
		connector->name = kasprintf(GFP_KERNEL, "%s", connector_name);
		if (!connector->name)
			DRM_ERROR("alloc name failed\n");
	}

	/* Encoder */
	encoder->possible_crtcs = priv->of_conf.crtc_masks[ENCODER_HDMI];
	drm_encoder_helper_add(encoder, &meson_hdmitx_encoder_helper_funcs);
	ret = drm_encoder_init(drm, encoder, &meson_hdmitx_encoder_funcs,
			       encoder_type, "am_hdmi_encoder");
	if (ret) {
		dev_err(priv->dev, "Failed to init hdmi encoder\n");
		return ret;
	}

	drm_connector_attach_encoder(connector, encoder);

	/*hpd irq moved to amhdmitx, register call back */
	hpd_cb.callback = meson_hdmitx_hpd_cb;
	hpd_cb.data = am_hdmi;
	hdmitx_register_hpd_cb(tx_comm, &hpd_cb);

	/*hdcp init, default is disable state*/
	if (!am_hdmi->android_path) {
		drm_connector_attach_content_protection_property(connector, true);
		hdcp_cb.hdcp_notify = meson_hdmitx_hdcp_notify;
		hdcp_cb.data = am_hdmi;
		drm_hdmitx_common_register_hdcp_notify(tx_comm, &hdcp_cb);
		am_hdmi->hdcp_mode = HDCP_NULL;
		am_hdmi->hdcp_state = HDCP_STATE_DISCONNECT;
	}

	am_hdmi->recovery_mode = priv->recovery_mode;

	drm_connector_attach_content_type_property(connector);
	drm_connector_attach_vrr_capable_property(connector);

	/*amlogic prop*/
	meson_hdmitx_init_colordepth_property(drm, am_hdmi);
	meson_hdmitx_init_colorspace_property(drm, am_hdmi);
	meson_hdmitx_init_hdmi_hdr_status_property(drm, am_hdmi);
	/*Getting hdr cap, don't similar to hdmitx sys hdr_cap node*/
	meson_hdmitx_init_static_meta_property(drm, am_hdmi);
	meson_hdmitx_init_hdr_priority_property(drm, am_hdmi);
	meson_hdmitx_create_range_property(drm, am_hdmi);
	meson_hdmitx_init_scan_info_property(drm, am_hdmi);

	/* notifier phy addr to cec when boot
	 * so that to not miss any hpd event
	 */
#ifdef CONFIG_CEC_NOTIFIER
	if (hdmitx_get_hpd_state(tx_comm)) {
		DRM_DEBUG_KMS("[%d]\n", __LINE__);
		raw_edid = hdmitx_get_raw_edid(tx_comm);
		if (raw_edid) {
			pedid = (struct edid *)raw_edid;
			cec_notifier_set_phys_addr_from_edid(am_hdmi->cec_notifier,
								 pedid);
		}
	} else {
		cec_notifier_set_phys_addr(am_hdmi->cec_notifier,
					   CEC_PHYS_ADDR_INVALID);
	}
#endif

	/*prop for userspace to acquire prop*/
	type_prop = drm_property_create_range(drm, DRM_MODE_PROP_IMMUTABLE,
		MESON_CONNECTOR_TYPE_PROP_NAME, 0, INT_MAX);
	if (type_prop) {
		am_hdmi->type_prop = type_prop;
		drm_object_attach_property(&am_hdmi->base.connector.base,
			type_prop, type);
	} else {
		DRM_ERROR("Failed to create property %s\n",
			MESON_CONNECTOR_TYPE_PROP_NAME);
	}

	DRM_DEBUG_KMS("out[%d]\n", __LINE__);
	return 0;
}

int meson_hdmitx_dev_unbind(struct drm_device *drm,
	int type, struct meson_connector_dev *intf)
{
	struct drm_connector *connector = intf->conn;
	struct am_hdmi_tx *am_hdmi = connector_to_am_hdmi(connector);
	struct hdmitx_common *tx_comm = to_hdmitx_common(am_hdmi->hdmitx_dev);

	drm_hdmitx_common_hdcp_exit(tx_comm);

#ifdef CONFIG_CEC_NOTIFIER
	if (am_hdmi->cec_notifier) {
		cec_notifier_set_phys_addr(am_hdmi->cec_notifier,
					   CEC_PHYS_ADDR_INVALID);
		//cec_notifier_put(am_hdmi_info.cec_notifier);
	}
#endif

	am_hdmi->base.connector.funcs->destroy(connector);
	am_hdmi->encoder.funcs->destroy(&am_hdmi->encoder);
	cancel_work_sync(&am_hdmi->hdcp_update_work);

	return 0;
}

int am_meson_mode_testattr_ioctl(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	struct drm_connector *connector;
	struct hdmitx_common *tx_comm;
	struct hdmitx_common_state *new_state;
	enum hdmi_colorspace cs;
	enum hdmi_color_depth cd;
	enum hdmi_quantization_range cr;
	struct drm_mode_test_attr *f = data;
	struct drm_mode_object *mo;

	mo = drm_mode_object_find(dev, file_priv, f->conn_id, DRM_MODE_OBJECT_CONNECTOR);
	if (!mo)
		return -EINVAL;

	connector = obj_to_connector(mo);
	tx_comm = meson_get_hdmitx_common(connector);
	hdmitx_parse_color_attr(f->attr, &cs, &cd, &cr);

	new_state = kzalloc(sizeof(*new_state), GFP_KERNEL);
	if (!new_state)
		return -ENOMEM;

	new_state->state_sequence_id = hdmitx_get_hpd_hw_sequence_id(tx_comm);

	if (!hdmitx_common_validate_mode_locked(tx_comm, new_state, f->modename, cs, cd, 1))
		f->valid = 1;
	else
		f->valid = 0;

	kfree(new_state);
	return 0;
}

int am_meson_hdmi_get_vrr_range(struct drm_device *dev,
			void *data, struct drm_file *file_priv)
{
	int i, num_group = 0;
	char *mode_name, *mode_qms_name;
	struct drm_connector *connector;
	struct drm_mode_object *mo;
	struct hdmitx_common *tx_comm;
	struct hdmitx_vrr_mode_group *hdmi_groups;
	struct drm_vrr_mode_groups *groups = data;

	mo = drm_mode_object_find(dev, file_priv, groups->conn_id, DRM_MODE_OBJECT_CONNECTOR);
	if (!mo)
		return 0;

	connector = obj_to_connector(mo);
	tx_comm = meson_get_hdmitx_common(connector);

	hdmi_groups = kcalloc(MAX_VRR_MODE_GROUP, sizeof(*hdmi_groups), GFP_KERNEL);
	if (!hdmi_groups)
		return 0;

	num_group = hdmitx_common_get_vrr_mode_group(tx_comm, hdmi_groups, MAX_VRR_MODE_GROUP);

	if (!num_group) {
		DRM_ERROR("get vrr error or not support qms\n");
		kfree(hdmi_groups);
		return 0;
	}

	for (i = 0; i < num_group; i++) {
		groups->groups[i].brr_vic = hdmi_groups[i].brr_vic;
		groups->groups[i].brr = hdmi_groups[i].brr;
		groups->groups[i].width = hdmi_groups[i].width;
		groups->groups[i].height = hdmi_groups[i].height;
		groups->groups[i].vrr_min = hdmi_groups[i].vrr_min / VRR_DIV;
		groups->groups[i].vrr_max = hdmi_groups[i].vrr_max / VRR_DIV;
		groups->groups[i].game_vrr_min = hdmi_groups[i].game_vrr_min;
		groups->groups[i].game_vrr_max = hdmi_groups[i].game_vrr_max;
		groups->groups[i].game_brr_vic = hdmi_groups[i].game_brr_vic;

		mode_name = groups->groups[i].modename;
		strncpy(mode_name, hdmi_groups[i].modename, DRM_DISPLAY_MODE_LEN);
		mode_name[DRM_DISPLAY_MODE_LEN - 1] = '\0';

		mode_qms_name = groups->groups[i].qms_modename;
		strncpy(mode_qms_name, hdmi_groups[i].qms_modename, DRM_DISPLAY_MODE_LEN);
		mode_qms_name[DRM_DISPLAY_MODE_LEN - 1] = '\0';
	}
	groups->num = num_group;

	kfree(hdmi_groups);
	return num_group;
}
