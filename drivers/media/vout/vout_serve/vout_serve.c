// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

 #define pr_fmt(fmt) "vout: " fmt

/* Linux Headers */
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/ctype.h>
#include <linux/of.h>
#include <linux/major.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/compat.h>

/* Amlogic Headers */
#include <linux/amlogic/media/vout/vout_notify.h>
#include <uapi/amlogic/vout_ioc.h>

/* Local Headers */
#include "vout_func.h"
#include "vout_reg.h"
#include "vout_null_server.h"

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
#include <linux/amlogic/pm.h>
static struct early_suspend early_suspend;
static int early_suspend_flag;
#endif

/* must be last include file */
#include <linux/amlogic/gki_module.h>

#define VOUT_CDEV_NAME  "display"
#define VOUT_CLASS_NAME "display"
#define MAX_NUMBER_PARA 10

#define VMODE_NAME_LEN_MAX    64
static struct class *vout_class;
static DEFINE_MUTEX(vout_serve_mutex);
static char vout_mode_uboot[VMODE_NAME_LEN_MAX] = "null";
static char vout_mode[VMODE_NAME_LEN_MAX] __nosavedata;
static char local_name[VMODE_NAME_LEN_MAX] = {0};
static char connector0_type[VMODE_NAME_LEN_MAX];
static u32 vout_init_vmode = VMODE_INIT_NULL;
static int uboot_display;
static unsigned int bist_mode;

static struct vout_cdev_s *vout_cdev;
static struct device *vout_dev;
static int vsync_irq;
static unsigned int vs_meas_en;

static bool disable_modesysfs;
static bool enable_debugmode;

int vout_debug_print;

static int vout_print_enable(char *str)
{
	if (strncmp("1", str, 1) == 0)
		vout_debug_print = 1;
	else
		vout_debug_print = 0;

	return 1;
}
__setup("vout_print=", vout_print_enable);

/* ********************************************************** */
static irqreturn_t vout_vsync_irq_handler(int irq, void *data)
{
	struct vinfo_s *vinfo = NULL;
	unsigned int fr;

	if (vs_meas_en == 0)
		return IRQ_HANDLED;

	vinfo = get_current_vinfo();
	if (!vinfo)
		return IRQ_HANDLED;
	switch (vinfo->mode) {
	case VMODE_HDMI:
	case VMODE_CVBS:
	case VMODE_LCD:
	case VMODE_DUMMY_ENCP:
	case VMODE_DUMMY_ENCI:
	case VMODE_DUMMY_ENCL:
		fr = vout_frame_rate_measure(1);
		vinfo->sync_duration_num = fr;
		vinfo->sync_duration_den = 1000;
		break;
	default:
		break;
	}

	return IRQ_HANDLED;
}

/* ********************************************************** */

char *get_vout_mode_internal(void)
{
	return vout_mode;
}
EXPORT_SYMBOL(get_vout_mode_internal);

char *get_vout_mode_uboot(void)
{
	return vout_mode_uboot;
}
EXPORT_SYMBOL(get_vout_mode_uboot);

char *get_uboot_connector0_type(void)
{
	return connector0_type;
}
EXPORT_SYMBOL(get_uboot_connector0_type);

int get_vout_mode_uboot_state(void)
{
	return uboot_display;
}
EXPORT_SYMBOL(get_vout_mode_uboot_state);

#define MAX_UEVENT_LEN 64

int vout_set_uevent(unsigned int vout_event, int val)
{
	char env[MAX_UEVENT_LEN];
	char *envp[2];
	int ret;

	if (vout_event != VOUT_EVENT_MODE_CHANGE)
		return 0;

	if (!vout_dev) {
		VOUTERR("no vout_dev\n");
		return -1;
	}

	memset(env, 0, sizeof(env));
	envp[0] = env;
	envp[1] = NULL;
	snprintf(env, MAX_UEVENT_LEN, "vout_setmode=%d", val);

	ret = kobject_uevent_env(&vout_dev->kobj, KOBJ_CHANGE, envp);

	return ret;
}
EXPORT_SYMBOL(vout_set_uevent);

int set_vout_mode_pre_process(enum vmode_e mode)
{
	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 1);
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	return 0;
}
EXPORT_SYMBOL(set_vout_mode_pre_process);

int set_vout_mode_post_process(enum vmode_e mode)
{
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);
	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 0);
	return 0;
}
EXPORT_SYMBOL(set_vout_mode_post_process);

int set_vout_mode_name(char *name)
{
	if (!name)
		return -EINVAL;

	memset(vout_mode, 0, sizeof(vout_mode));
	snprintf(vout_mode, VMODE_NAME_LEN_MAX, "%s", name);
	return 0;
}
EXPORT_SYMBOL(set_vout_mode_name);

static int set_vout_mode_type(char *name, int type_val)
{
	enum vmode_e mode;
	unsigned int frac;
	int ret = 0;

	vout_trim_string(name);
	VOUTPR("vmode set to %s, type_val=%d\n", name, type_val);

	if ((vout_check_same_vmodeattr(name) &&
	    (strcmp(name, vout_mode) == 0))) {
		VOUTPR("don't set the same mode as current, exit\n");
		return -1;
	}

	memset(local_name, 0, sizeof(local_name));
	snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", name);
	frac = vout_parse_vout_name(local_name);
	if (vout_debug_print) {
		VOUTPR("%s: local_name=%s, frac=%d\n",
			__func__, local_name, frac);
	}

	mode = validate_vmode(local_name, type_val, frac);
	if (mode == VMODE_MAX) {
		VOUTERR("no matched vout mode, exit\n");
		return -1;
	}

	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 1);

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	ret = set_current_vmode(mode);
	if (ret) {
		VOUTERR("new mode %s set error\n", name);
	} else {
		memset(vout_mode, 0, sizeof(vout_mode));
		snprintf(vout_mode, VMODE_NAME_LEN_MAX, "%s", name);
		VOUTPR("new mode %s set ok\n", name);
	}
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 0);

	return ret;
}

int set_vout_mode(char *name)
{
	enum vmode_e mode;
	unsigned int frac;
	int type_val = 0;
	int ret = 0;

	vout_trim_string(name);
	VOUTPR("vmode set to %s, type_val=%d\n", name, type_val);

	if ((vout_check_same_vmodeattr(name) &&
	    (strcmp(name, vout_mode) == 0))) {
		VOUTPR("don't set the same mode as current, exit\n");
		return -1;
	}

	memset(local_name, 0, sizeof(local_name));
	snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", name);
	frac = vout_parse_vout_name(local_name);
	if (vout_debug_print) {
		VOUTPR("%s: local_name=%s, frac=%d\n",
			__func__, local_name, frac);
	}

	mode = validate_vmode(local_name, type_val, frac);
	if (mode == VMODE_MAX) {
		VOUTERR("no matched vout mode, exit\n");
		return -1;
	}

	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 1);

	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE_PRE, &mode);
	ret = set_current_vmode(mode);
	if (ret) {
		VOUTERR("new mode %s set error\n", name);
	} else {
		memset(vout_mode, 0, sizeof(vout_mode));
		snprintf(vout_mode, VMODE_NAME_LEN_MAX, "%s", name);
		VOUTPR("new mode %s set ok\n", name);
	}
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	vout_set_uevent(VOUT_EVENT_MODE_CHANGE, 0);

	return ret;
}
EXPORT_SYMBOL(set_vout_mode);

int get_vout_mode_cap(char *buf)
{
	if (!get_vout_disp_cap(buf))
		sprintf(buf, "null\n");

	return 0;
}
EXPORT_SYMBOL(get_vout_mode_cap);

static int set_vout_init_mode(void)
{
	enum vmode_e vmode;
	unsigned int frac;
	int ret = 0;
	char *connector_type = NULL;
	int type_val = 0;

	connector_type = get_uboot_connector0_type();

	if (connector_type)
		type_val = convert_connector_type_to_val(connector_type);

	VOUTPR("%s, type_val = %d\n", __func__, type_val);

	memset(local_name, 0, sizeof(local_name));
	snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", vout_mode_uboot);
	frac = vout_parse_vout_name(local_name);

	vout_init_vmode = validate_vmode(local_name, type_val, frac);
	if (vout_init_vmode >= VMODE_MAX) {
		VOUTERR("no matched vout_init mode %s, force to invalid\n",
			vout_mode_uboot);
		nulldisp_index = 1;
		vout_init_vmode = nulldisp_vinfo[nulldisp_index].mode;
		snprintf(local_name, VMODE_NAME_LEN_MAX, "%s",
			 nulldisp_vinfo[nulldisp_index].name);
	} else { /* recover vout_mode_uboot */
		snprintf(local_name, VMODE_NAME_LEN_MAX, "%s", vout_mode_uboot);
	}

	if (uboot_display)
		vmode = vout_init_vmode | VMODE_INIT_BIT_MASK;
	else
		vmode = vout_init_vmode;

	ret = set_current_vmode(vmode);
	if (ret) {
		VOUTERR("init mode %s set error\n", local_name);
	} else {
		memset(vout_mode, 0, sizeof(vout_mode));
		snprintf(vout_mode, VMODE_NAME_LEN_MAX, "%s", local_name);
		VOUTDBG("init mode %s set ok\n", local_name);
	}
	vout_notifier_call_chain(VOUT_EVENT_SYS_INIT, &vmode);

	return ret;
}

/* ************************************************************* */
/* vout sysfs                                                    */
/* ************************************************************* */
static ssize_t vout_mode_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int ret = 0;

	ret = snprintf(buf, VMODE_NAME_LEN_MAX, "%s\n", vout_mode);

	return ret;
}

static ssize_t vout_mode_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	char mode[VMODE_NAME_LEN_MAX];
	char *buf_orig, *parm[8] = {NULL};
	long type_val = 0;
	int ret;

	if (!buf)
		return count;

	buf_orig = kstrdup(buf, GFP_KERNEL);

	vout_parse_param(buf_orig, (char **)&parm);

	mutex_lock(&vout_serve_mutex);
	if (!disable_modesysfs || enable_debugmode) {
		snprintf(mode, VMODE_NAME_LEN_MAX, "%s", parm[0]);

		if (parm[1])
			ret = kstrtoul(parm[1], 10, &type_val);

		VOUTPR("%s, type_val = %ld\n", __func__, type_val);
		set_vout_mode_type(mode, type_val);
	} else {
		VOUTPR("enable display/debug to set voutmode.\n");
	}
	mutex_unlock(&vout_serve_mutex);
	return count;
}

static ssize_t vout_fr_policy_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int policy;
	int ret = 0;

	policy = get_vframe_rate_policy();
	ret = sprintf(buf, "%d\n", policy);

	return ret;
}

static ssize_t vout_fr_policy_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int policy;
	int ret = 0;

	mutex_lock(&vout_serve_mutex);
	ret = kstrtoint(buf, 10, &policy);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		mutex_unlock(&vout_serve_mutex);
		return -EINVAL;
	}
	ret = set_vframe_rate_policy(policy);
	if (ret)
		pr_info("%s: %d failed\n", __func__, policy);
	mutex_unlock(&vout_serve_mutex);

	return count;
}

static ssize_t vout_fr_hint_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int fr_hint;
	int ret = 0;

	fr_hint = get_vframe_rate_hint();
	ret = sprintf(buf, "%d\n", fr_hint);

	return ret;
}

static ssize_t vout_fr_hint_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int fr_hint;
	int ret = 0;

	mutex_lock(&vout_serve_mutex);
	ret = kstrtoint(buf, 10, &fr_hint);
	if (ret) {
		mutex_unlock(&vout_serve_mutex);
		return -EINVAL;
	}
	set_vframe_rate_hint(fr_hint);
	mutex_unlock(&vout_serve_mutex);

	return count;
}

static ssize_t vout_fr_range_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	const struct vinfo_s *info = NULL;

	info = get_current_vinfo();
	if (!info)
		return sprintf(buf, "current vinfo is null\n");

	return sprintf(buf, "%d %d\n", info->vfreq_min, info->vfreq_max);
}

static ssize_t vout_frame_rate_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	unsigned int fr;
	int ret = 0;

	fr = vout_frame_rate_measure(1);
	ret = sprintf(buf, "%d.%03d\n", (fr / 1000), (fr % 1000));

	return ret;
}

static ssize_t vout_frame_rate_high_res_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	unsigned int fr;
	int ret = 0;

	fr = vout_frame_rate_msr_high_res(1);
	ret = sprintf(buf, "%d.%06d\n", (fr / 1000000), (fr % 1000000));

	return ret;
}

static ssize_t vout_bist_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int ret = 0;

	ret = sprintf(buf, "%d\n", bist_mode);

	return ret;
}

static ssize_t vout_bist_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret = 0;

	mutex_lock(&vout_serve_mutex);

	ret = kstrtouint(buf, 10, &bist_mode);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		mutex_unlock(&vout_serve_mutex);
		return -EINVAL;
	}
	set_vout_bist(bist_mode);

	mutex_unlock(&vout_serve_mutex);

	return count;
}

static ssize_t vout_bl_brightness_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int brightness;
	int ret = 0;

	mutex_lock(&vout_serve_mutex);
	brightness = get_vout_bl_brightness();
	ret = sprintf(buf, "%d\n", brightness);
	mutex_unlock(&vout_serve_mutex);

	return ret;
}

static ssize_t vout_bl_brightness_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int brightness;
	int ret = 0;

	mutex_lock(&vout_serve_mutex);

	ret = kstrtoint(buf, 10, &brightness);
	if (ret) {
		pr_info("%s: invalid data\n", __func__);
		mutex_unlock(&vout_serve_mutex);
		return -EINVAL;
	}
	set_vout_bl_brightness(brightness);

	mutex_unlock(&vout_serve_mutex);

	return count;
}

static ssize_t vout_vinfo_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	const struct vinfo_s *info = NULL;
	ssize_t len = 0;
	unsigned int i, j, fr;
	unsigned char val;

	info = get_current_vinfo();
	if (!info)
		return sprintf(buf, "current vinfo is null\n");

	fr = vout_frame_rate_measure(1);
	len = sprintf(buf, "current vinfo:\n"
		       "    name:                  %s\n"
		       "    connector_type:        %d\n"
		       "    mode:                  %d\n"
		       "    frac:                  %d\n"
		       "    width:                 %d\n"
		       "    height:                %d\n"
		       "    field_height:          %d\n"
		       "    aspect_ratio_num:      %d\n"
		       "    aspect_ratio_den:      %d\n"
		       "    screen_real_width:     %d\n"
		       "    screen_real_height:    %d\n"
		       "    sync_duration_num:     %d\n"
		       "    sync_duration_den:     %d\n"
		       "    brr_duration:          %d\n"
		       "    (meas_frame_rate:      %d.%03d)\n"
		       "    std_duration:          %d\n"
		       "    vfreq_max:             %d\n"
		       "    vfreq_min:             %d\n"
		       "    htotal:                %d\n"
		       "    vtotal:                %d\n"
		       "    video_clk:             %d\n"
		       "    fr_adj_type:           %d\n"
		       "    viu_color_fmt:         %d\n"
		       "    viu_mux:               0x%x\n"
		       "    cur_enc_ppc:           %d\n"
		       "    ufr_mode:              %d\n"
		       "    asf_mode:              %d\n"
		       "    3d_info:               %d\n"
		       "    cs:                    %d\n"
		       "    cd:                    %d\n"
		       "    vpp_post_out_color_fmt:%d\n\n"
		       "    vpp_post_out_colorimetry:%d\n"
		       "    vpp_post_out_color_range:%d\n\n",
		       info->name, info->connector_type, info->mode, info->frac,
		       info->width, info->height, info->field_height,
		       info->aspect_ratio_num, info->aspect_ratio_den,
		       info->screen_real_width, info->screen_real_height,
		       info->sync_duration_num, info->sync_duration_den,
		       info->brr_duration,
		       (fr / 1000), (fr % 1000), info->std_duration,
		       info->vfreq_max, info->vfreq_min,
		       info->htotal, info->vtotal, info->video_clk,
		       info->fr_adj_type, info->viu_color_fmt,
		       info->viu_mux, info->cur_enc_ppc, info->ufr_mode,
		       info->asf_mode, info->info_3d,
		       info->cs, info->cd, info->vpp_post_out_color_fmt,
		       info->vpp_post_out_colorimetry, info->vpp_post_out_range);
	len += sprintf(buf + len, "master_display_info:\n"
		       "    present_flag          %d\n"
		       "    features              0x%x\n"
		       "    primaries             0x%x, 0x%x\n"
		       "                          0x%x, 0x%x\n"
		       "                          0x%x, 0x%x\n"
		       "    white_point           0x%x, 0x%x\n"
		       "    luminance             %d, %d\n\n",
		       info->master_display_info.present_flag,
		       info->master_display_info.features,
		       info->master_display_info.primaries[0][0],
		       info->master_display_info.primaries[0][1],
		       info->master_display_info.primaries[1][0],
		       info->master_display_info.primaries[1][1],
		       info->master_display_info.primaries[2][0],
		       info->master_display_info.primaries[2][1],
		       info->master_display_info.white_point[0],
		       info->master_display_info.white_point[1],
		       info->master_display_info.luminance[0],
		       info->master_display_info.luminance[1]);

	len += sprintf(buf + len, "dv_info:\n"
		       "    ieeeoui                0x%06x\n"
		       "    ver                    %d\n"
		       "    length                 %d\n"
		       "    sup_422_12bit_sink_led %d\n"
		       "    sup_2160p60hz          %d\n"
		       "    sup_1080p120hz         %d\n"
		       "    sup_global_dimming     %d\n"
		       "    dv_emp_cap             %d\n"
		       "    low_latency            %d\n\n",
		       info->dv_info.ieeeoui,
		       info->dv_info.ver,
		       info->dv_info.length,
		       info->dv_info.sup_yuv422_12bit,
		       info->dv_info.sup_2160p60hz,
		       info->dv_info.sup_1080p120hz,
		       info->dv_info.sup_global_dimming,
		       info->dv_info.dv_emp_cap,
		       info->dv_info.low_latency);

	len += sprintf(buf + len, "hdr_static_info:\n"
		       "    hdr_support           %d\n"
		       "      hlg_support         %d\n"
		       "      hdr10_support       %d\n"
		       "      hdr_support         %d\n"
		       "      sdr_support         %d\n"
		       "    lumi_max              %d\n"
		       "    lumi_avg              %d\n"
		       "    lumi_min              %d\n",
		       info->hdr_info.hdr_support,
		       (info->hdr_info.hdr_support & 0x8) >> 3,
		       (info->hdr_info.hdr_support & 0x4) >> 2,
		       (info->hdr_info.hdr_support & 0x2) >> 1,
		       info->hdr_info.hdr_support & 0x1,
		       info->hdr_info.lumi_max,
		       info->hdr_info.lumi_avg,
		       info->hdr_info.lumi_min);
	len += sprintf(buf + len, "hdr_dynamic_info:");
	for (i = 0; i < 4; i++) {
		len += sprintf(buf + len,
			       "\n    metadata_version:  %x\n"
			       "    support_flags:     %x\n",
			       info->hdr_info.dynamic_info[i].type,
			       info->hdr_info.dynamic_info[i].support_flags);
		len += sprintf(buf + len, "    optional_fields:  ");
		for (j = 0; j < info->hdr_info.dynamic_info[i].of_len; j++) {
			val = info->hdr_info.dynamic_info[i].optional_fields[j];
			len += sprintf(buf + len, " %x", val);
		}
	}
	len += sprintf(buf + len, "\n");
	len += sprintf(buf + len, "hdr10+:\n");
	len += sprintf(buf + len, "    ieeeoui: %x\n",
		       info->hdr_info.hdr10plus_info.ieeeoui);
	len += sprintf(buf + len, "    application_version: %x\n",
		       info->hdr_info.hdr10plus_info.application_version);

	return len;
}

static ssize_t vout_cap_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	int ret;

	ret = get_vout_disp_cap(buf);
	if (!ret)
		return sprintf(buf, "null\n");

	return ret;
}

static ssize_t vout_connector_type_cap_show(const struct class *class,
			     const struct class_attribute *attr, char *buf)
{
	int count = 0, i = 0;
	ssize_t len = 0;
	struct vout_connector_type_s *type_s;

	type_s = kcalloc(MAX_CONNECTOR_TYPE_NUM, sizeof(*type_s), GFP_KERNEL);
	if (!type_s)
		return -ENOMEM;

	count = vout_func_get_connector_type_cap(type_s);

	if (!count) {
		kfree(type_s);
		return sprintf(buf, "current type_s is null\n");
	}

	for (i = 0; i < count; i++)
		len += sprintf(buf + len, "%s     %d\n", type_s[i].name, type_s[i].connector_type);

	kfree(type_s);
	return len;
}

static ssize_t vout_debug_mode_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", enable_debugmode);
}

static ssize_t vout_debug_mode_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int debug;

	ret = kstrtoint(buf, 10, &debug);
	if (ret)
		return -EINVAL;

	if (debug > 0)
		enable_debugmode = 1;
	else
		enable_debugmode = 0;

	return count;
}

static ssize_t vout_debug_vs_meas_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", vs_meas_en);
}

static ssize_t vout_debug_vs_meas_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int temp;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		return -EINVAL;

	if (temp > 0)
		vs_meas_en = 1;
	else
		vs_meas_en = 0;

	return count;
}

static ssize_t vout_debug_print_show(const struct class *class,
			const struct class_attribute *attr,
			char *buf)
{
	return sprintf(buf, "%d\n", vout_debug_print);
}

static ssize_t vout_debug_print_store(const struct class *class,
			const struct class_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	int temp;

	ret = kstrtoint(buf, 10, &temp);
	if (ret)
		return -EINVAL;

	if (temp > 0)
		vout_debug_print = 1;
	else
		vout_debug_print = 0;

	return count;
}

static struct class_attribute vout_class_attrs[] = {
	__ATTR(mode,       0644, vout_mode_show, vout_mode_store),
	__ATTR(fr_policy,  0644, vout_fr_policy_show, vout_fr_policy_store),
	__ATTR(fr_hint,    0644, vout_fr_hint_show, vout_fr_hint_store),
	__ATTR(fr_range,   0644, vout_fr_range_show, NULL),
	__ATTR(frame_rate, 0644, vout_frame_rate_show, NULL),
	__ATTR(fr_high_res, 0644, vout_frame_rate_high_res_show, NULL),
	__ATTR(bist,       0644, vout_bist_show, vout_bist_store),
	__ATTR(bl_brightness, 0644, vout_bl_brightness_show, vout_bl_brightness_store),
	__ATTR(vinfo,      0444, vout_vinfo_show, NULL),
	__ATTR(cap,        0644, vout_cap_show, NULL),
	__ATTR(connector_type_cap,        0644, vout_connector_type_cap_show, NULL),
	__ATTR(debug,      0644, vout_debug_mode_show, vout_debug_mode_store),
	__ATTR(vs_meas,    0644, vout_debug_vs_meas_show, vout_debug_vs_meas_store),
	__ATTR(print,      0644, vout_debug_print_show, vout_debug_print_store),
};

static int vout_attr_create(void)
{
	int i;
	int ret = 0;

	/* create vout class */
	vout_class = class_create(VOUT_CLASS_NAME);
	if (IS_ERR(vout_class)) {
		VOUTERR("create vout class fail\n");
		return -1;
	}

	/* create vout class attr files */
	for (i = 0; i < ARRAY_SIZE(vout_class_attrs); i++) {
		if (class_create_file(vout_class, &vout_class_attrs[i])) {
			VOUTERR("create vout attribute %s fail\n",
				vout_class_attrs[i].attr.name);
		}
	}

	/*VOUTPR("create vout attribute OK\n");*/

	return ret;
}

static int vout_attr_remove(void)
{
	int i;

	if (!vout_class)
		return 0;

	for (i = 0; i < ARRAY_SIZE(vout_class_attrs); i++)
		class_remove_file(vout_class, &vout_class_attrs[i]);

	class_destroy(vout_class);
	vout_class = NULL;

	return 0;
}

/* ************************************************************* */
/* vout ioctl                                                    */
/* ************************************************************* */
static int vout_io_open(struct inode *inode, struct file *file)
{
	struct vout_cdev_s *vcdev;

	if (vout_debug_print)
		VOUTPR("vout io__open\n");
	vcdev = container_of(inode->i_cdev, struct vout_cdev_s, cdev);
	file->private_data = vcdev;
	return 0;
}

static int vout_io_release(struct inode *inode, struct file *file)
{
	if (vout_debug_print)
		VOUTPR("vout io_release\n");
	file->private_data = NULL;
	return 0;
}

static long vout_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __user *argp;
	int mcd_nr;
	struct vinfo_s *vinfo = NULL;
	struct vinfo_base_s baseinfo;
	struct optical_base_s optical_info;
	struct venc_base_s venc_info;
	int i, j, temp;

	mcd_nr = _IOC_NR(cmd);
	if (vout_debug_print) {
		VOUTPR("%s: cmd: 0x%x, cmd_dir = 0x%x, cmd_nr = 0x%x\n",
			__func__, cmd, _IOC_DIR(cmd), mcd_nr);
	}

	vinfo = get_current_vinfo();
	if (!vinfo) {
		if (vout_debug_print)
			VOUTERR("%s: vinfo is null\n", __func__);
		return -EFAULT;
	}
	if (vinfo->mode == VMODE_INIT_NULL) {
		if (vout_debug_print)
			VOUTERR("%s: VMODE_INIT_NULL\n", __func__);
		return -EFAULT;
	}

	argp = (void __user *)arg;
	switch (mcd_nr) {
	case VOUT_IOC_NR_GET_VINFO:
		if (vout_debug_print)
			VOUTPR("%s: VOUT_IOC_NR_GET_VINFO\n", __func__);
		baseinfo.mode = vinfo->mode;
		baseinfo.width = vinfo->width;
		baseinfo.height = vinfo->height;
		baseinfo.field_height = vinfo->field_height;
		baseinfo.aspect_ratio_num = vinfo->aspect_ratio_num;
		baseinfo.aspect_ratio_den = vinfo->aspect_ratio_den;
		baseinfo.sync_duration_num = vinfo->sync_duration_num;
		baseinfo.sync_duration_den = vinfo->sync_duration_den;
		baseinfo.screen_real_width = vinfo->screen_real_width;
		baseinfo.screen_real_height = vinfo->screen_real_height;
		baseinfo.video_clk = vinfo->video_clk;
		baseinfo.viu_color_fmt = vinfo->viu_color_fmt;
		if (copy_to_user(argp, &baseinfo, sizeof(struct vinfo_base_s)))
			ret = -EFAULT;
		break;
	case VOUT_IOC_NR_GET_OPTICAL_INFO:
		if (vout_debug_print)
			VOUTPR("%s: VOUT_IOC_NR_GET_OPTICAL_INFO\n", __func__);
		memset(&optical_info, 0, sizeof(struct optical_base_s));
		for (i = 0; i < 3; i++) {
			for (j = 0; j < 2; j++) {
				optical_info.primaries[i][j] =
					vinfo->master_display_info.primaries[i][j];
			}
		}
		for (i = 0; i < 2; i++) {
			optical_info.white_point[i] =
				vinfo->master_display_info.white_point[i];
		}
		optical_info.lumi_max = vinfo->hdr_info.lumi_max;
		optical_info.lumi_min = vinfo->hdr_info.lumi_min;
		optical_info.lumi_avg = vinfo->hdr_info.lumi_avg;
		optical_info.lumi_peak = vinfo->hdr_info.lumi_peak;
		optical_info.ldim_support = vinfo->hdr_info.ldim_support;
		if (copy_to_user(argp, &optical_info, sizeof(struct optical_base_s)))
			ret = -EFAULT;
		break;
	case VOUT_IOC_NR_GET_VENC_INFO:
		if (vout_debug_print)
			VOUTPR("%s: VOUT_IOC_NR_GET_VENC_INFO\n", __func__);
		memset(&venc_info, 0, sizeof(struct venc_base_s));
		venc_info.venc_index = (vinfo->viu_mux >> 4) & 0xf;
		venc_info.venc_sel = vinfo->viu_mux & 0xf;
		if (copy_to_user(argp, &venc_info, sizeof(struct venc_base_s)))
			ret = -EFAULT;
		break;
	case VOUT_IOC_NR_GET_BL_BRIGHTNESS:
		if (vout_debug_print)
			VOUTPR("%s: VOUT_IOC_NR_GET_BL_BRIGHTNESS\n", __func__);

		temp = get_vout_bl_brightness();
		if (copy_to_user(argp, &temp, sizeof(int)))
			ret = -EFAULT;
		break;
	case VOUT_IOC_NR_SET_BL_BRIGHTNESS:
		if (vout_debug_print)
			VOUTPR("%s: VOUT_IOC_NR_SET_BL_BRIGHTNESS\n", __func__);
		if (copy_from_user(&temp, argp, sizeof(int))) {
			ret = -EFAULT;
			break;
		}
		set_vout_bl_brightness(temp);
		break;
	default:
		VOUTERR("%s: invalid mcd_nr 0x%x\n", __func__, mcd_nr);
		ret = -EINVAL;
		break;
	}

	return ret;
}

#ifdef CONFIG_COMPAT
static long vout_compat_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned long ret;

	arg = (unsigned long)compat_ptr(arg);
	ret = vout_ioctl(file, cmd, arg);
	return ret;
}
#endif

static const struct file_operations vout_fops = {
	.owner          = THIS_MODULE,
	.open           = vout_io_open,
	.release        = vout_io_release,
	.unlocked_ioctl = vout_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = vout_compat_ioctl,
#endif
};

static int vout_fops_create(void)
{
	int ret = 0;

	vout_cdev = kmalloc(sizeof(*vout_cdev), GFP_KERNEL);
	if (!vout_cdev) {
		VOUTERR("failed to allocate vout_cdev\n");
		return -1;
	}

	ret = alloc_chrdev_region(&vout_cdev->devno, 0, 1, VOUT_CDEV_NAME);
	if (ret < 0) {
		VOUTERR("failed to alloc vout devno\n");
		goto vout_fops_err1;
	}

	cdev_init(&vout_cdev->cdev, &vout_fops);
	vout_cdev->cdev.owner = THIS_MODULE;
	ret = cdev_add(&vout_cdev->cdev, vout_cdev->devno, 1);
	if (ret) {
		VOUTERR("failed to add vout cdev\n");
		goto vout_fops_err2;
	}

	vout_cdev->dev = device_create(vout_class, NULL, vout_cdev->devno,
				       NULL, VOUT_CDEV_NAME);
	if (IS_ERR(vout_cdev->dev)) {
		ret = PTR_ERR(vout_cdev->dev);
		VOUTERR("failed to create vout device: %d\n", ret);
		goto vout_fops_err3;
	}

	/*VOUTPR("%s OK\n", __func__);*/
	return 0;

vout_fops_err3:
	cdev_del(&vout_cdev->cdev);
vout_fops_err2:
	unregister_chrdev_region(vout_cdev->devno, 1);
vout_fops_err1:
	kfree(vout_cdev);
	vout_cdev = NULL;
	return -1;
}

static void vout_fops_remove(void)
{
	cdev_del(&vout_cdev->cdev);
	unregister_chrdev_region(vout_cdev->devno, 1);
	kfree(vout_cdev);
	vout_cdev = NULL;
}

/* ************************************************************* */

#ifdef CONFIG_PM
static int aml_vout_suspend(struct platform_device *pdev, pm_message_t state)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_suspend();
	return 0;
}

static int aml_vout_resume(struct platform_device *pdev)
{
#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND

	if (early_suspend_flag)
		return 0;

#endif
	vout_resume();
	return 0;
}
#endif

#ifdef CONFIG_HIBERNATION
static int aml_vout_freeze(struct device *dev)
{
	return 0;
}

static int aml_vout_thaw(struct device *dev)
{
	return 0;
}

static int aml_vout_restore(struct device *dev)
{
	enum vmode_e mode;

	mode = get_current_vmode();
	vout_notifier_call_chain(VOUT_EVENT_MODE_CHANGE, &mode);

	return 0;
}

static int aml_vout_pm_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout_suspend(pdev, PMSG_SUSPEND);
}

static int aml_vout_pm_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);

	return aml_vout_resume(pdev);
}
#endif

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
static void aml_vout_early_suspend(struct early_suspend *h)
{
	if (early_suspend_flag)
		return;

	vout_suspend();
	early_suspend_flag = 1;
}

static void aml_vout_late_resume(struct early_suspend *h)
{
	if (!early_suspend_flag)
		return;

	early_suspend_flag = 0;
	vout_resume();
}
#endif

static void aml_vout_get_dt_info(struct platform_device *pdev)
{
	unsigned int para[2];
	int ret;

	ret = of_property_read_u32(pdev->dev.of_node, "fr_policy", &para[0]);
	if (!ret) {
		ret = set_vframe_rate_policy(para[0]);
		if (ret)
			VOUTERR("init fr_policy %d failed\n", para[0]);
		else
			VOUTDBG("fr_policy:%d\n", para[0]);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "vs_meas", &para[0]);
	if (!ret) {
		vs_meas_en = para[0];
		VOUTPR("get vs_meas:%d\n", vs_meas_en);
	}

	vsync_irq = platform_get_irq_byname(pdev, "vsync");
	if (vsync_irq < 0) {
		VOUTPR("%s: can't get vsync irq\n", __func__);
	} else {
		if (request_irq(vsync_irq, vout_vsync_irq_handler, IRQF_SHARED,
				"vout_vsync", (void *)&vsync_irq)) {
			VOUTERR("%s: can't request vout_vsync\n", __func__);
		}
	}
}

/*****************************************************************
 **
 **	vout driver interface
 **
 ******************************************************************/
static int aml_vout_probe(struct platform_device *pdev)
{
	int ret = -1;

	vout_dev = &pdev->dev;

	aml_vout_get_dt_info(pdev);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	early_suspend.suspend = aml_vout_early_suspend;
	early_suspend.resume = aml_vout_late_resume;
	register_early_suspend(&early_suspend);
#endif

	vout_class = NULL;
	ret = vout_attr_create();
	ret = vout_fops_create();

	vout_register_server(&nulldisp_vout_server);
	set_vout_init_mode();

	VOUTDBG("%s OK\n", __func__);
	return ret;
}

static void aml_vout_remove(struct platform_device *pdev)
{
	if (vsync_irq > 0)
		free_irq(vsync_irq, (void *)&vsync_irq);

#ifdef CONFIG_AMLOGIC_LEGACY_EARLY_SUSPEND
	unregister_early_suspend(&early_suspend);
#endif

	vout_attr_remove();
	vout_fops_remove();
	vout_unregister_server(&nulldisp_vout_server);
}

static void aml_vout_shutdown(struct platform_device *pdev)
{
	VOUTPR("%s\n", __func__);
	vout_shutdown();
}

static const struct of_device_id aml_vout_dt_match[] = {
	{ .compatible = "amlogic, vout",},
	{ },
};

#ifdef CONFIG_HIBERNATION
const struct dev_pm_ops vout_pm = {
	.freeze		= aml_vout_freeze,
	.thaw		= aml_vout_thaw,
	.restore	= aml_vout_restore,
	.suspend	= aml_vout_pm_suspend,
	.resume		= aml_vout_pm_resume,
};
#endif

static struct platform_driver vout_driver = {
	.probe     = aml_vout_probe,
	.remove    = aml_vout_remove,
	.shutdown   = aml_vout_shutdown,
#ifdef CONFIG_PM
	.suspend   = aml_vout_suspend,
	.resume    = aml_vout_resume,
#endif
	.driver = {
		.name = "vout",
		.of_match_table = aml_vout_dt_match,
#ifdef CONFIG_HIBERNATION
		.pm = &vout_pm,
#endif
	},
};

int __init vout_init_module(void)
{
	return platform_driver_register(&vout_driver);
}

__exit void vout_exit_module(void)
{
	platform_driver_unregister(&vout_driver);
}

static int str2lower(char *str)
{
	while (*str != '\0') {
		*str = tolower(*str);
		str++;
	}
	return 0;
}

static void vout_init_mode_parse(char *str)
{
	/* detect vout mode */
	if (strlen(str) <= 1) {
		VOUTERR("%s: %s\n", __func__, str);
		return;
	}

	/* detect uboot display */
	if (strncmp(str, "en", 2) == 0) { /* enable */
		uboot_display = 1;
		VOUTPR("%s: %d\n", str, uboot_display);
		return;
	}
	if (strncmp(str, "dis", 3) == 0) { /* disable */
		uboot_display = 0;
		VOUTPR("%s: %d\n", str, uboot_display);
		return;
	}
	if (strncmp(str, "frac", 4) == 0) { /* frac */
		if ((strlen(vout_mode_uboot) + strlen(str))
		    < VMODE_NAME_LEN_MAX)
			strcat(vout_mode_uboot, str);
		else
			VOUTERR("%s: str len out of support\n", __func__);
		VOUTPR("%s\n", str);
		return;
	}
	if (strncmp(str, "frac", 4) == 0) { /* frac */
		if ((strlen(vout_mode_uboot) + strlen(str))
		    < VMODE_NAME_LEN_MAX)
			strcat(vout_mode_uboot, str);
		else
			VOUTERR("%s: str len out of support\n", __func__);
		VOUTPR("%s\n", str);
		return;
	}

	/*
	 * just save the vmode_name,
	 * convert to vmode when vout sever registered
	 */
	snprintf(vout_mode_uboot, VMODE_NAME_LEN_MAX, "%s", str);
	vout_trim_string(vout_mode_uboot);
	VOUTPR("%s\n", vout_mode_uboot);
}

int get_vout_init_mode(char *str)
{
	char *ptr = str;
	char sep[2];
	char *option;
	int count = 3;
	char find = 0;

	if (!str)
		return -EINVAL;

	do {
		if (!isalpha(*ptr) && !isdigit(*ptr) &&
		    (*ptr != '_') && (*ptr != '-')) {
			find = 1;
			break;
		}
	} while (*++ptr != '\0');
	if (!find)
		return -EINVAL;

	sep[0] = *ptr;
	sep[1] = '\0';
	while ((count--) && (option = strsep(&str, sep))) {
		/* VOUTPR("%s\n", option); */
		str2lower(option);
		vout_init_mode_parse(option);
	}

	return 1;
}
__setup("vout=", get_vout_init_mode);

static int get_connector0_type(char *str)
{
	if (str)
		sprintf(connector0_type, "%s", str);

	VOUTPR("connector0_type: %s\n", connector0_type);
	return 1;
}

__setup("connector0_type=", get_connector0_type);

static int get_connector_type_to_compat(char *str)
{
	char *ret = NULL;

	if (!str)
		return 0;
	if (connector0_type[0]) {
		VOUTPR("bypass for connector0_type(%s) already set\n", connector0_type);
		return 0;
	}

	snprintf(connector0_type, VMODE_NAME_LEN_MAX - 1, "%s", str);

	ret = strstr(connector0_type, "_");
	if (ret)
		connector0_type[ret - connector0_type] = '-';

	VOUTPR("connector_type(compact): %s\n", connector0_type);
	return 1;
}

__setup("connector_type=", get_connector_type_to_compat);

/*TODO: drm to disable display/mode sysfs set.*/
void disable_vout_mode_set_sysfs(void)
{
	disable_modesysfs = true;
}
EXPORT_SYMBOL(disable_vout_mode_set_sysfs);

//MODULE_AUTHOR("Platform-BJ <platform.bj@amlogic.com>");
//MODULE_DESCRIPTION("VOUT Server Module");
//MODULE_LICENSE("GPL");
