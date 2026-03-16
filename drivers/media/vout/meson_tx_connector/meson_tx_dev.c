// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_dev.h>
#include <linux/amlogic/media/vout/meson_tx_connector/meson_tx_hw_opcode.h>

#include "meson_tx_task_mgr.h"
#include "meson_tx_event_mgr.h"
#include "meson_tx_internal.h"

#define DRIVER_NAME "meson_tx"

void meson_tx_get_init_state(struct meson_tx_dev *tx_dev,
			   struct meson_tx_state *state)
{
	struct meson_tx_format_para *para = NULL;
	struct meson_tx_log *tx_log = meson_get_tx_log(tx_dev);

	if (!tx_dev || !state) {
		TX_ERROR(tx_log, "[%s]: invalid input param\n", __func__);
		return;
	}
	para = &tx_dev->sw_fmt_para;
	memcpy(&state->para, para, sizeof(*para));
}
EXPORT_SYMBOL(meson_tx_get_init_state);

void meson_tx_fire_drm_hpd_cb_unlocked(struct meson_tx_dev *base)
{
	if (base && base->drm_hpd_cb.callback)
		base->drm_hpd_cb.callback(base->drm_hpd_cb.data);
}

int meson_tx_register_hpd_cb(struct meson_tx_dev *base, struct connector_hpd_cb *hpd_cb)
{
	struct meson_tx_log *tx_log = meson_get_tx_log(base);

	if (!base || !hpd_cb)
		return -1;

	base->drm_hpd_cb.callback = hpd_cb->callback;
	base->drm_hpd_cb.data = hpd_cb->data;
	TX_INFO(tx_log, "%s\n", __func__);

	return 0;
}
EXPORT_SYMBOL(meson_tx_register_hpd_cb);

int meson_tx_get_hpd_state(struct meson_tx_dev *base)
{
	struct meson_tx_log *tx_log = meson_get_tx_log(base);

	if (!base) {
		TX_ERROR(tx_log, "[%s]: invalid input param\n", __func__);
		return 0;
	}
	return base->hpd_state;
}
EXPORT_SYMBOL(meson_tx_get_hpd_state);

/* get hpd plugin sequence id */
u64 meson_tx_get_hpd_hw_sequence_id(struct meson_tx_dev *base)
{
	u64 tmp_sequence_id;
	struct meson_tx_log *tx_log = meson_get_tx_log(base);

	if (!base || !base->tx_hw_base) {
		TX_ERROR(tx_log, "[%s]: invalid input param\n", __func__);
		return 0;
	}
	mutex_lock(&base->set_mode_mutex);
	tmp_sequence_id = base->hw_sequence_id;
	mutex_unlock(&base->set_mode_mutex);

	return tmp_sequence_id;
}
EXPORT_SYMBOL(meson_tx_get_hpd_hw_sequence_id);

bool meson_tx_get_used_state(struct meson_tx_dev *base)
{
	struct meson_tx_log *tx_log = meson_get_tx_log(base);

	if (!base) {
		TX_ERROR(tx_log, "[%s]: invalid input param\n", __func__);
		return false;
	}
	return base->already_used_state;
}
EXPORT_SYMBOL(meson_tx_get_used_state);

struct meson_tx_log *meson_get_tx_log(struct meson_tx_dev *tx_dev)
{
	return &tx_dev->tx_log;
}

/* for bootup/resume */
void meson_tx_plugin_unlocked(struct meson_tx_dev *tx_dev)
{
	struct meson_tx_log *tx_log = meson_get_tx_log(tx_dev);

	if (!tx_dev)
		return;
	/*
	 * only happen in such case:
	 * hpd high when suspend->plugout->plugin->late resume, the
	 * last plugin/resume flow sequence is unknown, will do
	 * plugin handler only once
	 */
	if (tx_dev->hpd_state) {
		TX_INFO(tx_log, "warning: continuous plugin, should not happen!\n");
		return;
	}

	tx_dev->hw_sequence_id = get_jiffies_64();
	TX_INFO(tx_log, "hpd_high, sequence id: %lld\n", tx_dev->hw_sequence_id);
	meson_tx_tracer_write_event(tx_dev->tx_tracer, TX_HPD_PLUGIN);
	/* SW: status update */
	tx_dev->already_used_state = 1;
	tx_dev->hpd_state = true;

	if (tx_dev->ops->hpd_plugin)
		tx_dev->ops->hpd_plugin(tx_dev);
	meson_tx_notify_hpd_status(tx_dev, false);
}

/* for hpd irq */
void meson_tx_plugin_handler(void *para)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)para;

	mutex_lock(&tx_dev->set_mode_mutex);
	meson_tx_plugin_unlocked(tx_dev);
	mutex_unlock(&tx_dev->set_mode_mutex);
	/* notify to drm dptx */
	meson_tx_fire_drm_hpd_cb_unlocked(tx_dev);
}

/* for bootup/resume */
void meson_tx_plugout_unlocked(struct meson_tx_dev *tx_dev)
{
	struct meson_tx_log *tx_log = meson_get_tx_log(tx_dev);

	if (!tx_dev)
		return;
	if (!tx_dev->hpd_state) {
		TX_INFO(tx_log, "continuous plugout handler, send TX_LINK_UNSTABLE uevent\n");
		meson_tx_event_mgr_send_uevent(tx_dev->event_mgr,
				TX_LINK_UNSTABLE, 1, false);
		return;
	}

	TX_INFO(tx_log, "hpd_low\n");
	meson_tx_tracer_write_event(tx_dev->tx_tracer, TX_HPD_PLUGOUT);
	meson_tx_clear_rx_cap(&tx_dev->rxcap);
	memset(tx_dev->edid_buf, 0, sizeof(tx_dev->edid_buf));
	tx_dev->hpd_state = false;
	tx_dev->hw_sequence_id = 0;
	if (tx_dev->ops->hpd_plugout)
		tx_dev->ops->hpd_plugout(tx_dev);

	meson_tx_notify_hpd_status(tx_dev, false);
}

/* for hpd irq */
void meson_tx_plugout_handler(void *para)
{
	struct meson_tx_dev *tx_dev = (struct meson_tx_dev *)para;

	mutex_lock(&tx_dev->set_mode_mutex);
	meson_tx_plugout_unlocked(tx_dev);
	mutex_unlock(&tx_dev->set_mode_mutex);

	/* notify to drm dptx */
	meson_tx_fire_drm_hpd_cb_unlocked(tx_dev);
}

static struct tx_task_info tx_task_infos[] = {
	{
		.name = "plugin",
		.fn = meson_tx_plugin_handler,
		.type = HPD_PLUGIN,
		.queue_type = TASK_QUEUE_HPD,
		.flag = TASK_FLAG_DELAY_WORK,
		.queue_name = DRIVER_NAME,
		.queue_flag = WQ_HIGHPRI | __WQ_LEGACY | WQ_MEM_RECLAIM,
	},
	{
		.name = "plugout",
		.fn = meson_tx_plugout_handler,
		.type = HPD_PLUGOUT,
		.queue_type = TASK_QUEUE_HPD,
		.flag = TASK_FLAG_DELAY_WORK,
		.queue_name = DRIVER_NAME,
		.queue_flag = WQ_HIGHPRI | __WQ_LEGACY | WQ_MEM_RECLAIM,
	},
};

static int meson_tx_task_init(struct meson_tx_dev *tx_dev, struct meson_tx_hw *tx_hw)
{
	int i, ret = 0;

	if (!tx_dev || !tx_hw) {
		pr_err("%s NULL tx_dev or tx_hw pointer\n", __func__);
		return -EINVAL;
	}
	tx_dev->task_mgr = tx_task_mgr_init();
	if (!tx_dev->task_mgr)
		return -ENOMEM;

	for (i = 0; i < ARRAY_SIZE(tx_task_infos); i++) {
		ret = tx_task_mgr_setup_task(tx_dev->task_mgr, &tx_task_infos[i], tx_dev);
		if (ret)
			pr_err("%s setup task[%s] failed\n",
				__func__, tx_task_infos[i].name);
	}

	/* init hw task ops */
	tx_hw->event_ops = kzalloc(sizeof(*tx_hw->event_ops), GFP_KERNEL);
	if (!tx_hw->event_ops)
		return -ENOMEM;
	tx_hw->event_ops->cancel_event = tx_task_mgr_cancel_task;
	tx_hw->event_ops->queue_event = tx_task_mgr_queue_task;
	tx_hw->event_ops->data = tx_dev->task_mgr;
	return 0;
}

static void meson_tx_task_release(struct meson_tx_dev *tx_dev)
{
	struct tx_task_manager *tx_task = (struct tx_task_manager *)tx_dev->task_mgr;

	kfree(tx_dev->tx_hw_base->event_ops);
	tx_dev->tx_hw_base->event_ops = NULL;

	tx_task_mgr_release(tx_task);
}

int meson_tx_sysfs_create(struct meson_tx_dev *tx_dev)
{
	tx_dev->dev = tx_dev->conn_dev.conn->kdev;
	// do common sysfs create
	if (tx_dev->ops->create_sysfs)
		tx_dev->ops->create_sysfs(tx_dev);

	meson_tx_event_mgr_set_uevent_dev(tx_dev->event_mgr, tx_dev->dev, dev_name(tx_dev->dev));
	return 0;
}
EXPORT_SYMBOL(meson_tx_sysfs_create);

void meson_tx_sysfs_remove(struct meson_tx_dev *tx_dev)
{
	// do common sysfs remove
	if (tx_dev->ops->remove_sysfs)
		tx_dev->ops->remove_sysfs(tx_dev);
}
EXPORT_SYMBOL(meson_tx_sysfs_remove);

void meson_tx_dev_init(struct meson_tx_dev *tx_dev, struct meson_tx_hw *tx_hw,
		       struct meson_tx_helper_ops *ops)
{
	tx_dev->ops = ops;
	tx_dev->tx_hw_base = tx_hw;
	meson_tx_log_init(&tx_dev->tx_log, dev_name(tx_dev->pdev));
	meson_tx_task_init(tx_dev, tx_dev->tx_hw_base);
	tx_dev->event_mgr = meson_tx_event_mgr_create(tx_dev);
	meson_tx_event_mgr_suspend(tx_dev->event_mgr, false);
	tx_dev->tx_tracer = meson_tx_tracer_create(tx_dev->event_mgr);
}

void meson_tx_dev_release(struct meson_tx_dev *tx_dev)
{
	meson_tx_task_release(tx_dev);
}

unsigned char *meson_tx_get_raw_edid(struct meson_tx_dev *tx_dev)
{
	if (!tx_dev) {
		pr_err("[%s]: invalid input param\n", __func__);
		return NULL;
	}
	if (tx_dev->rxcap.edid_parsing)
		return tx_dev->edid_buf;
	else
		return NULL;
}
EXPORT_SYMBOL(meson_tx_get_raw_edid);

/* only check vic in edid */
bool meson_tx_edid_validate_vic(struct rx_cap *rxcap, u32 vic)
{
	int i = 0;
	bool edid_matched = false;

	if (!rxcap)
		return false;

	if (vic < HDMITX_VESA_OFFSET) {
		/* check cea cap */
		for (i = 0 ; i < rxcap->VIC_count; i++) {
			if (rxcap->VIC[i] == vic) {
				edid_matched = true;
				break;
			}
		}
	} else {
		enum hdmi_vic *vesa_t = &rxcap->vesa_timing[0];
		/* check vesa mode. */
		for (i = 0; i < VESA_MAX_TIMING && vesa_t[i]; i++) {
			if (vic == vesa_t[i]) {
				edid_matched = true;
				break;
			}
		}
	}

	return edid_matched;
}

/* check vic support y420 and supported by EDID */
bool meson_tx_edid_check_y420_support(struct rx_cap *prxcap, enum hdmi_vic vic)
{
	unsigned int i = 0;
	bool ret = false;

	if (!prxcap)
		return false;

	if (meson_tx_validate_y420_vic(vic)) {
		for (i = 0; i < Y420_VIC_MAX_NUM; i++) {
			if (prxcap->y420_vic[i]) {
				if (prxcap->y420_vic[i] == vic) {
					ret = true;
					break;
				}
			} else {
				ret = false;
				break;
			}
		}
	}

	return ret;
}

/* check if timing is supported in RX_Cap(EDID/displayID)
 * note should use meson_tx_match_detail_timing() to get complete timing firstly
 */
bool meson_tx_edid_validate_timing(struct tx_timing *timing, struct rx_cap *rx_cap)
{
	u32 idx = 0;
	struct display_id_cap *disp_cap = NULL;
	int array_idx = 0;
	u8 remainder = 0;
	struct tx_timing *tmp_timing = NULL;
	bool ret = false;

	if (!rx_cap || !timing)
		return false;

	/* match timing to get full timing param including vic,
	 * if VIC is not NULL, it means that it's a timing
	 * declared in pure EDID;
	 * step1: check if VIC supported by pure EDID
	 */
	if (timing->vic)
		ret = meson_tx_edid_validate_vic(rx_cap, timing->vic);
	if (ret)
		return true;

	/* step2: continue check if timing is supported by pure displayID */
	disp_cap = &rx_cap->disp_id_cap;
	if (disp_cap->version == 0)
		return ret;
	for (idx = 0; idx < disp_cap->timing_count; idx++) {
		array_idx = idx / BLK_TIMING_CNT;
		remainder = idx % BLK_TIMING_CNT;
		tmp_timing = &disp_cap->timing[array_idx][remainder];
		if (meson_tx_mode_compare(timing, tmp_timing))
			break;
	}
	if (idx < disp_cap->timing_count)
		ret = true;
	return ret;
}
EXPORT_SYMBOL(meson_tx_edid_validate_timing);

/* check if cs/cd under specific timing is supported in RX_Cap(EDID/displayID)
 * note should use meson_tx_match_detail_timing() to get complete timing firstly
 */
bool meson_tx_edid_validate_color(struct tx_timing *timing,
	enum hdmi_colorspace cs, enum hdmi_color_depth cd, struct rx_cap *rx_cap)
{
	struct display_id_cap *disp_cap = NULL;
	bool ret = false;
	const struct dv_info *dv;

	if (!rx_cap || !timing)
		return false;

	dv = &rx_cap->dv_info;

	/* step1: check color space/depth + vic is within pure EDID cap */
	if (timing->vic) {
		if (cs == HDMI_COLORSPACE_YUV444) {
			enum hdmi_color_depth rx_y444_max_dc = COLORDEPTH_24B;
			/* Rx may not support Y444 */
			if (!(rx_cap->native_Mode & CAP_BIT5_YCBCR_444_MASK))
				return true;
			if (rx_cap->dc_y444 && (rx_cap->dc_30bit ||
				dv->sup_10b_12b_444 == 0x1))
				rx_y444_max_dc = COLORDEPTH_30B;
			else if (rx_cap->dc_y444 && (rx_cap->dc_36bit ||
				dv->sup_10b_12b_444 == 0x2))
				rx_y444_max_dc = COLORDEPTH_36B;

			if (cd <= rx_y444_max_dc)
				return true;
		} else if (cs == HDMI_COLORSPACE_YUV422) {
			/* Rx may not support Y422 */
			if (rx_cap->native_Mode & CAP_BIT4_YCBCR_422_MASK)
				return true;
		} else if (cs == HDMI_COLORSPACE_RGB) {
			enum hdmi_color_depth rx_rgb_max_dc = COLORDEPTH_24B;
			/* Always assume RX supports RGB444 */
			if (rx_cap->dc_30bit || dv->sup_10b_12b_444 == 0x1)
				rx_rgb_max_dc = COLORDEPTH_30B;
			else if (rx_cap->dc_36bit || dv->sup_10b_12b_444 == 0x2)
				rx_rgb_max_dc = COLORDEPTH_36B;

			if (cd <= rx_rgb_max_dc)
				return true;
		} else if (cs == HDMI_COLORSPACE_YUV420) {
			if (!meson_tx_edid_check_y420_support(rx_cap, timing->vic))
				ret = false;
			else if (!rx_cap->dc_30bit_420 && cd == COLORDEPTH_30B)
				ret = false;
			else if (!rx_cap->dc_36bit_420 && cd == COLORDEPTH_36B)
				ret = false;
			else
				ret = true;
			if (ret)
				return ret;
		}
	}

	/* step2: check color space/depth + timing is within pure displayID cap */
	disp_cap = &rx_cap->disp_id_cap;
	if (disp_cap->version == 0)
		return ret;
	/* TO CONFIRM: need to check color capability declared in EDID */
	if (cs == HDMI_COLORSPACE_RGB) {
		if (cd == COLORDEPTH_18B &&
			disp_cap->display_interface.color_depth.rgb_color_depth.bpc6)
			ret = true;
		else if (cd == COLORDEPTH_24B &&
			disp_cap->display_interface.color_depth.rgb_color_depth.bpc8)
			ret = true;
		else if (cd == COLORDEPTH_30B &&
			disp_cap->display_interface.color_depth.rgb_color_depth.bpc10)
			ret = true;
		else if (cd == COLORDEPTH_36B &&
			disp_cap->display_interface.color_depth.rgb_color_depth.bpc12)
			ret = true;
		else if (cd == COLORDEPTH_42B &&
			disp_cap->display_interface.color_depth.rgb_color_depth.bpc14)
			ret = true;
		else if (cd == COLORDEPTH_48B &&
			disp_cap->display_interface.color_depth.rgb_color_depth.bpc16)
			ret = true;
	} else if (cs == HDMI_COLORSPACE_YUV444) {
		if (cd == COLORDEPTH_18B &&
			disp_cap->display_interface.color_depth.yuv444_color_depth.bpc6)
			ret = true;
		else if (cd == COLORDEPTH_24B &&
			disp_cap->display_interface.color_depth.yuv444_color_depth.bpc8)
			ret = true;
		else if (cd == COLORDEPTH_30B &&
			disp_cap->display_interface.color_depth.yuv444_color_depth.bpc10)
			ret = true;
		else if (cd == COLORDEPTH_36B &&
			disp_cap->display_interface.color_depth.yuv444_color_depth.bpc12)
			ret = true;
		else if (cd == COLORDEPTH_42B &&
			disp_cap->display_interface.color_depth.yuv444_color_depth.bpc14)
			ret = true;
		else if (cd == COLORDEPTH_48B &&
			disp_cap->display_interface.color_depth.yuv444_color_depth.bpc16)
			ret = true;
	} else if (cs == HDMI_COLORSPACE_YUV422) {
		if (cd == COLORDEPTH_24B &&
			disp_cap->display_interface.color_depth.yuv422_color_depth.bpc8)
			ret = true;
		else if (cd == COLORDEPTH_30B &&
			disp_cap->display_interface.color_depth.yuv422_color_depth.bpc10)
			ret = true;
		else if (cd == COLORDEPTH_36B &&
			disp_cap->display_interface.color_depth.yuv422_color_depth.bpc12)
			ret = true;
		else if (cd == COLORDEPTH_42B &&
			disp_cap->display_interface.color_depth.yuv422_color_depth.bpc14)
			ret = true;
		else if (cd == COLORDEPTH_48B &&
			disp_cap->display_interface.color_depth.yuv422_color_depth.bpc16)
			ret = true;
	} else if (cs == HDMI_COLORSPACE_YUV420) {
		if ((timing->flags & DISPLAY_ID_TYPE_VII_YUV420) == 0)
			return false;
		if (cd == COLORDEPTH_24B &&
			disp_cap->display_interface.color_depth.yuv420_color_depth.bpc8)
			ret = true;
		else if (cd == COLORDEPTH_30B &&
			disp_cap->display_interface.color_depth.yuv420_color_depth.bpc10)
			ret = true;
		else if (cd == COLORDEPTH_36B &&
			disp_cap->display_interface.color_depth.yuv420_color_depth.bpc12)
			ret = true;
		else if (cd == COLORDEPTH_42B &&
			disp_cap->display_interface.color_depth.yuv420_color_depth.bpc14)
			ret = true;
		else if (cd == COLORDEPTH_48B &&
			disp_cap->display_interface.color_depth.yuv420_color_depth.bpc16)
			ret = true;
	}
	return ret;
}

static struct parse_cd parse_cd_[] = {
	{COLORDEPTH_24B, "8bit",},
	{COLORDEPTH_30B, "10bit"},
	{COLORDEPTH_36B, "12bit"},
	{COLORDEPTH_48B, "16bit"},
};

static struct parse_cs parse_cs_[] = {
	{HDMI_COLORSPACE_RGB, "rgb",},
	{HDMI_COLORSPACE_YUV422, "422",},
	{HDMI_COLORSPACE_YUV444, "444",},
	{HDMI_COLORSPACE_YUV420, "420",},
};

static struct parse_cr parse_cr_[] = {
	{HDMI_QUANTIZATION_RANGE_LIMITED, "limit",},
	{HDMI_QUANTIZATION_RANGE_FULL, "full",},
};

void meson_tx_parse_color_attr(char const *attr_str,
	enum hdmi_colorspace *cs, enum hdmi_color_depth *cd,
	enum hdmi_quantization_range *cr)
{
	int i;

	/* parse color depth */
	for (i = 0; i < sizeof(parse_cd_) / sizeof(struct parse_cd); i++) {
		if (strstr(attr_str, parse_cd_[i].name)) {
			*cd = parse_cd_[i].cd;
			break;
		}
	}
	/* set default value */
	if (i == sizeof(parse_cd_) / sizeof(struct parse_cd))
		*cd = COLORDEPTH_24B;

	/* parse color space */
	for (i = 0; i < sizeof(parse_cs_) / sizeof(struct parse_cs); i++) {
		if (strstr(attr_str, parse_cs_[i].name)) {
			*cs = parse_cs_[i].cs;
			break;
		}
	}
	/* set default value */
	if (i == sizeof(parse_cs_) / sizeof(struct parse_cs))
		*cs = HDMI_COLORSPACE_YUV444;

	/* parse color range */
	for (i = 0; i < sizeof(parse_cr_) / sizeof(struct parse_cr); i++) {
		if (strstr(attr_str, parse_cr_[i].name)) {
			*cr = parse_cr_[i].cr;
			break;
		}
	}
	/* set default value */
	if (i == sizeof(parse_cr_) / sizeof(struct parse_cr))
		*cr = HDMI_QUANTIZATION_RANGE_FULL;
}
EXPORT_SYMBOL(meson_tx_parse_color_attr);

void meson_tx_format_para_rst(struct meson_tx_format_para *para)
{
	if (!para)
		return;
	memset(para, 0, sizeof(struct meson_tx_format_para));
	memcpy(para->name, "invalid", sizeof("invalid"));
	para->cs = HDMI_COLORSPACE_RESERVED4;
	para->cd = COLORDEPTH_RESERVED;
	para->cr = HDMI_QUANTIZATION_RANGE_RESERVED;
}

/* init SW and HW format_para with cs/cd/timing... */
int meson_tx_format_para_init(struct meson_tx_dev *tx_dev, struct tx_timing *timing,
	u32 frac_mode, enum hdmi_colorspace cs,
	enum hdmi_color_depth cd, enum hdmi_quantization_range cr,
	struct meson_tx_format_para *para)
{
	int ret = 0;
	const struct tx_timing *match_timing = NULL;

	if (!para || !timing || !tx_dev)
		return -EINVAL;

	meson_tx_format_para_rst(para);

	/* try to match complete timing parameters firstly */
	if (!timing->vic)
		match_timing = meson_tx_match_detail_timing(timing);

	if (match_timing)
		para->timing = *match_timing;
	else
		para->timing = *timing;

	para->cs = cs;
	para->cd = cd;
	para->cr = cr;
	/* check fraction mode, and update pixel_freq */
	ret = meson_tx_mode_update_timing(&para->timing, frac_mode);
	if (ret < 0)
		para->frac_mode = 0;
	else
		para->frac_mode = frac_mode;
	if (!para->timing.v_freq)
		para->timing.v_freq = meson_tx_get_timing_vrefresh(&para->timing);
	if (!para->timing.h_freq)
		para->timing.h_freq =
			DIV_ROUND_CLOSEST_ULL(mul_u32_u32(para->timing.pixel_freq, 1000),
				para->timing.h_total);

	if (para->timing.sname)
		memcpy(para->name, para->timing.sname, sizeof(para->name));
	else if (para->timing.name)
		memcpy(para->name, para->timing.name, sizeof(para->name));
	else
		sprintf(para->name, "%dx%d%s%dhz", timing->h_active, timing->v_active,
			timing->pi_mode ? "p" : "i", para->timing.v_freq / 1000);

	/* build hw fmt_para */
	if (tx_dev->ops->build_hw_fmt_para)
		return tx_dev->ops->build_hw_fmt_para(tx_dev, para);

	return 0;
}
EXPORT_SYMBOL(meson_tx_format_para_init);

/* drm api for validate mode */
int meson_tx_validate_mode(struct meson_tx_dev *tx_dev, struct meson_tx_state *new_state)
{
	int ret = 0;
	struct meson_tx_format_para *new_sw_para;
	struct meson_tx_log *tx_log = meson_get_tx_log(tx_dev);

	if (!tx_dev || !new_state) {
		TX_ERROR(tx_log, "[%s]: invalid input param\n", __func__);
		return -EINVAL;
	}
	new_sw_para = &new_state->para;

	if (new_state->sequence_id != tx_dev->hw_sequence_id) {
		TX_ERROR(tx_log, "%s: sequence_id failed: %lld\n",
							__func__, new_state->sequence_id);
		ret = -EINVAL;
		goto out;
	}

	mutex_lock(&tx_dev->valid_mode_mutex);
	ret = tx_dev->ops->validate_fmt_para(tx_dev, new_state);
	if (ret)
		TX_DEBUG(tx_log, "validate format para [%s,cs:%d,cd:%d] return error %d\n",
			     new_sw_para->name, new_sw_para->cs, new_sw_para->cd, ret);
out:
	mutex_unlock(&tx_dev->valid_mode_mutex);
	return ret;
}
EXPORT_SYMBOL(meson_tx_validate_mode);

int meson_tx_build_clk_param(struct meson_tx_dev *tx_dev,
	struct meson_tx_format_para *para, u32 enc_idx, u32 hdmi_if_idx)
{
	struct meson_tx_log *tx_log = meson_get_tx_log(tx_dev);

	if (!tx_dev || !para) {
		TX_ERROR(tx_log, "[%s]: invalid input param\n", __func__);
		return -EINVAL;
	}
	para->vid_clk_path = hdmi_if_idx << 4 | enc_idx;
	meson_tx_hw_cntl(tx_dev->tx_hw_base, PLATFORM_VID_CLK_PARAM_BUILD,
		para, &tx_dev->tx_hw_base->tx_clk->tx_clk_cfg);
	return 0;
}
EXPORT_SYMBOL(meson_tx_build_clk_param);

int meson_tx_notify_hpd_status(struct meson_tx_dev *tx_dev, bool force_uevent)
{
	if (!tx_dev)
		return -EINVAL;
	if (!tx_dev->suspend_flag) {
		/* notify to userspace by uevent */
		meson_tx_event_mgr_send_uevent(tx_dev->event_mgr,
				TX_HPD_EVENT, tx_dev->hpd_state, force_uevent);
		meson_tx_event_mgr_send_uevent(tx_dev->event_mgr,
				TX_AUDIO_EVENT, tx_dev->hpd_state, force_uevent);
	} else {
		/*
		 * under early suspend, only update uevent state, not
		 * post to system, in case 1.old android system will
		 * set hdmi mode, 2.audio server and audio_hal will
		 * start run, increase power consumption
		 */
		meson_tx_event_mgr_set_uevent_state(tx_dev->event_mgr,
				TX_HPD_EVENT, tx_dev->hpd_state);
		meson_tx_event_mgr_set_uevent_state(tx_dev->event_mgr,
				TX_AUDIO_EVENT, tx_dev->hpd_state);
	}
	/*
	 * always notify to other driver module: such as CEC/RX
	 * CEC/RX side will decide to update HPD/EDID or
	 * not by product type
	 */
	if (tx_dev->hpd_state)
		meson_tx_event_mgr_notify(tx_dev->event_mgr, TX_PLUG, tx_dev->edid_buf);
	else
		meson_tx_event_mgr_notify(tx_dev->event_mgr, TX_UNPLUG, NULL);

	return 0;
}
