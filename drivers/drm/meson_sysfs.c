// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/seq_file.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drmP.h>
#include <drm/drm_modeset_lock.h>
#include <linux/kernel.h>

#include "meson_sysfs.h"
#include "meson_crtc.h"
#include "meson_plane.h"
#include "meson_hdmi.h"
#include "meson_vpu_pipeline.h"
#include "meson_gem.h"

static const char vpu_group_name[] = "vpu";
static const char module_param_group_name[] = "module_param";
static const char osd0_group_name[] = "osd0";
static const char osd1_group_name[] = "osd1";
static const char osd2_group_name[] = "osd2";
static const char osd3_group_name[] = "osd3";
static const char osd4_group_name[] = "osd4";
int osd_index[MESON_MAX_OSDS] = {0, 1, 2, 3, 4};
static const char video0_group_name[] = "video0";
static const char video1_group_name[] = "video1";
static const char video2_group_name[] = "video2";
int video_index[MESON_MAX_VIDEOS] = {0, 1, 2};
static const char crtc0_group_name[] = "crtc0";
static const char crtc1_group_name[] = "crtc1";
static const char crtc2_group_name[] = "crtc2";
int crtc_index[MESON_MAX_POSTBLEND] = {0, 1, 2};
u32 pages;
u32 *overwrite_reg;
u32 *overwrite_val;
u8 *overwrite_crtc_idx;
int overwrite_enable;
int reg_num;

#define AM_BIN_ATTR_MOD_PARAM_W_DEF(_name) \
static ssize_t _name##_store(struct file *filp, struct kobject *kobj,	\
			 struct bin_attribute *attr, char *buf, loff_t off,	\
			 size_t count)	\
{\
	struct device *dev = kobj_to_dev(kobj);	\
	struct drm_minor *minor = dev_get_drvdata(dev);	\
	struct meson_drm *priv;	 \
	int val;	\
\
	if (!minor || !minor->dev)	\
		return -EINVAL;	\
\
	priv = minor->dev->dev_private;	\
\
	if (kstrtouint(buf, 10, &val) < 0)	\
		return -EINVAL;	\
\
	 am_drm_param._name = val;	\
\
	return count;	\
}

#define AM_BIN_ATTR_MOD_PARAM_R_DEF(_name) \
static ssize_t _name##_show(struct file *filp, struct kobject *kobj,	\
			 struct bin_attribute *attr, char *buf, loff_t off,	\
			 size_t count)	\
{\
	struct device *dev = kobj_to_dev(kobj);	\
	struct drm_minor *minor = dev_get_drvdata(dev);	\
	struct meson_drm *priv;	\
	int pos = 0;	\
\
	if (!minor || !minor->dev)	\
		return -EINVAL;	\
	if (off > 0)	\
		return 0;	\
\
	priv = minor->dev->dev_private;	\
\
	pos += snprintf(buf + pos, PAGE_SIZE - pos,\
	__stringify(_name) ": %d\n", am_drm_param._name);	\
\
	return pos;\
}

#define _AM_BIN_ATTR(_name, _mode, _show, _store)		\
	{\
		.attr = { .name = __stringify(_name), .mode = _mode },\
		.read	= _show,	\
		.write = _store,	\
	},

#define AM_BIN_ATTR_MOD_PARAM_RW(_name)	\
		_AM_BIN_ATTR(_name, 0644, _name##_show, _name##_store)

//EXPORT_SYMBOL_GPL(vpu_group_name);

static void am_meson_drm_unmap_phyaddr(u8 *vaddr)
{
	void *addr = (void *)(PAGE_MASK & (ulong)vaddr);

	DRM_DEBUG("unmap va(%p)\n", addr);
	vunmap(addr);
}

static void parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

int init_overwrite(struct device *dev)
{
	if (!overwrite_reg)
		overwrite_reg = devm_kcalloc(dev, sizeof(u32),
				MESON_MAX_OVERWRITE_REG, GFP_KERNEL);

	if (!overwrite_val)
		overwrite_val = devm_kcalloc(dev, sizeof(u32),
				MESON_MAX_OVERWRITE_REG, GFP_KERNEL);

	if (!overwrite_crtc_idx)
		overwrite_crtc_idx = devm_kcalloc(dev, sizeof(u8),
				MESON_MAX_OVERWRITE_REG, GFP_KERNEL);

	return 0;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	int i, pos = 0;

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo rv reg > debug to read the register\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo wv reg val > debug to overwrite the register\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo wv reg val crtc_idx > debug to overwrite the reg on crtc\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo wvb reg val start lens > debug to overwrite the specific bits in register\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo ow 1 > debug to enable overwrite register\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"\noverwrote status: %s\n", overwrite_enable ? "on" : "off");

	if (overwrite_enable) {
		init_overwrite(dev);

		for (i = 0; i < reg_num; i++)
			pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"reg[0x%04x]=0x%08x,crtc_idx:%d\n", overwrite_reg[i], overwrite_val[i],
				overwrite_crtc_idx[i]);
	}

	return pos;
}

static ssize_t debug_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t n)
{
	char dst_buf[64];
	long val;
	int i;
	unsigned int reg_addr, reg_val, crtc_idx = 0, tmp_val, read_val, start, len;
	char *bufp, *parm[8] = {NULL};
	int lens = strlen(buf);

	if (lens > sizeof(dst_buf) - 1)
		return -EINVAL;

	memcpy(dst_buf, buf, lens);

	dst_buf[lens] = '\0';
	bufp = dst_buf;
	parse_param(bufp, (char **)&parm);

	if (strcmp(parm[0], "rv"))
		init_overwrite(dev);

	if (!strcmp(parm[0], "rv")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;

		reg_addr = val;
		DRM_INFO("reg[0x%04x]=0x%08x\n", reg_addr, meson_drm_read_reg(reg_addr));
	} else if (!strcmp(parm[0], "wv")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		reg_addr = val;

		if (kstrtoul(parm[2], 16, &val) < 0)
			return -EINVAL;

		reg_val = val;

		/*
		 * optional crtc index parameter
		 * default value: 0
		 */
		if (parm[3]) {
			if (kstrtoul(parm[3], 16, &val) < 0)
				return -EINVAL;

			crtc_idx = val;
			if (crtc_idx >= MESON_MAX_CRTC) {
				DRM_INFO("Invalid crtc index! Use default 0\n");
				crtc_idx = 0;
			}
		}

		for (i = 0; i < reg_num; i++) {
			if (overwrite_reg[i] == reg_addr) {
				overwrite_val[i] = reg_val;
				overwrite_crtc_idx[i] = crtc_idx;
				return lens;
			}
		}

		if (i == reg_num) {
			overwrite_reg[i] = reg_addr;
			overwrite_val[i] = reg_val;
			overwrite_crtc_idx[i] = crtc_idx;
			reg_num++;
		}
	} else if (!strcmp(parm[0], "ow")) {
		if (parm[1] && !strcmp(parm[1], "1")) {
			overwrite_enable = 1;
		} else if (parm[1] && !strcmp(parm[1], "0")) {
			overwrite_enable = 0;
			for (i = 0; i < reg_num; i++) {
				overwrite_val[i] = 0;
				overwrite_val[i] = 0;
				overwrite_crtc_idx[i] = 0;
			}
			reg_num = 0;
		}
	} else if (!strcmp(parm[0], "wvb")) {
		if (kstrtoul(parm[1], 16, &val) < 0)
			return -EINVAL;
		reg_addr = val;

		if (kstrtouint(parm[2], 10, &tmp_val) < 0)
			return -EINVAL;
		if (kstrtouint(parm[3], 10, &start) < 0)
			return -EINVAL;
		if (kstrtouint(parm[4], 10, &len) < 0)
			return -EINVAL;

		read_val = meson_drm_read_reg(reg_addr);
		reg_val = (read_val & ~(((1L << (len)) - 1) << (start))) |
				((unsigned int)(tmp_val) << (start));

		for (i = 0; i < reg_num; i++) {
			if (overwrite_reg[i] == reg_addr) {
				overwrite_val[i] = reg_val;
				return lens;
			}
		}

		if (i == reg_num) {
			overwrite_reg[i] = reg_addr;
			overwrite_val[i] = reg_val;
			reg_num++;
		}
	}

	return n;
}

static DEVICE_ATTR_RW(debug);

static struct attribute *vpu_attrs[] = {
	&dev_attr_debug.attr,
	NULL,
};

static const struct attribute_group vpu_attr_group = {
	.name	= vpu_group_name,
	.attrs	= vpu_attrs,
};

static int meson_drm_trigger_atomic_commit(struct drm_minor *minor)
{
	struct drm_modeset_acquire_ctx ctx;
	int err;
	struct drm_atomic_state *state = ERR_PTR(-EINVAL);

	DRM_MODESET_LOCK_ALL_BEGIN(minor->dev, ctx, 0, err);
	state = drm_atomic_helper_duplicate_state(minor->dev, &ctx);
	if (IS_ERR(state))
		return -EFAULT;
	err = drm_atomic_helper_commit_duplicated_state(state, &ctx);
	DRM_MODESET_LOCK_ALL_END(minor->dev, ctx, err);
	if (IS_ERR(state))
		return -EFAULT;
	drm_atomic_state_put(state);

	return err;
}

static ssize_t osd_pixel_blend_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	struct am_osd_plane *amp;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;
	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	amp = priv->osd_planes[*(int *)attr->private];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"cat pixel_blend to show blend mask\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo <value> > pixel_blend to set blend mask\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"pixel blend mask: 0x%x\n", amp->pixel_blend_debug);

	return pos;
}

static ssize_t osd_pixel_blend_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int idx = *(int *)attr->private;
	struct meson_drm *priv;
	struct am_osd_plane *amp;

	if (!minor || !minor->dev)
		return -EINVAL;
	priv = minor->dev->dev_private;
	amp = priv->osd_planes[idx];

	if (kstrtou16(buf, 16, &amp->pixel_blend_debug) < 0)
		return -EINVAL;

	DRM_INFO("Set pixel blend mask to %s\n", buf);

	return count;
}

static ssize_t osd_reverse_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	struct am_osd_plane *amp;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;
	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	amp = priv->osd_planes[*(int *)attr->private];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1/2/3 > osd_reverse :reverse the osd xy/x/y\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0 > osd_reverse to un_reverse the osd plane\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"osd_reverse: %d\n", amp->osd_reverse);

	return pos;
}

static ssize_t osd_reverse_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int idx = *(int *)attr->private;
	struct meson_drm *priv;
	struct am_osd_plane *amp;

	if (!minor || !minor->dev)
		return -EINVAL;
	priv = minor->dev->dev_private;
	amp = priv->osd_planes[idx];

	if (sysfs_streq(buf, "0")) {
		amp->osd_reverse = 0;
		DRM_INFO("disable the osd reverse\n");
	} else if (sysfs_streq(buf, "1")) {
		amp->osd_reverse = DRM_MODE_REFLECT_MASK;
		DRM_INFO("enable the osd reverse\n");
	} else if (sysfs_streq(buf, "2")) {
		amp->osd_reverse = DRM_MODE_REFLECT_X;
		DRM_INFO("enable the osd reverse_x\n");
	} else if (sysfs_streq(buf, "3")) {
		amp->osd_reverse = DRM_MODE_REFLECT_Y;
		DRM_INFO("enable the osd reverse_y\n");
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t osd_blend_bypass_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	struct am_osd_plane *amp;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;
	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	amp = priv->osd_planes[*(int *)attr->private];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1/0 > osd_blend_bypass :enable/disable\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"osd_blend_bypass: %d\n", amp->osd_blend_bypass);

	return pos;
}

static ssize_t osd_blend_bypass_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	struct am_osd_plane *amp;

	if (!minor || !minor->dev)
		return -EINVAL;

	priv = minor->dev->dev_private;
	amp = priv->osd_planes[*(int *)attr->private];

	if (sysfs_streq(buf, "1")) {
		amp->osd_blend_bypass = 1;
		DRM_INFO("enable the osd blend bypass\n");
	} else if (sysfs_streq(buf, "0")) {
		amp->osd_blend_bypass = 0;
		DRM_INFO("disable the osd blend bypass\n");
	}

	return count;
}

static ssize_t osd_read_port_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	struct am_osd_plane *amp;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;
	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	amp = priv->osd_planes[*(int *)attr->private];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1 > enable read port setting\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0 > disable read port setting\n");
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"\nstatus: %d\n", (amp->osd_read_ports == 1) ? 1 : 0);

	return pos;
}

static ssize_t osd_read_port_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	struct am_osd_plane *amp;
	long val;

	if (!minor || !minor->dev)
		return -EINVAL;

	priv = minor->dev->dev_private;
	amp = priv->osd_planes[*(int *)attr->private];

	if (kstrtoul(buf, 16, &val) < 0)
		return -EINVAL;

	val = val >= 1 ? 1 : 0;
	amp->osd_read_ports = val;

	return count;
}

static ssize_t osd_fbdump_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	struct am_osd_plane *amp;
	bool bflg;
	int crtc_index;
	u32 fb_size;
	u32 index;
	u32 num_pages;
	void *vir_addr;
	u64 phy_addr;
	struct meson_vpu_osd_layer_info *info;
	struct meson_vpu_sub_pipeline_state *mvps;

	if (!minor || !minor->dev)
		return -EINVAL;

	index = *(int *)attr->private;
	priv = minor->dev->dev_private;
	crtc_index = priv->pipeline->osd_crtc_index[index];
	if (crtc_index == -1) {
		DRM_INFO("osd is disabled\n");
		return 0;
	}

	amp = priv->osd_planes[index];
	mvps = priv_to_sub_pipeline_state(priv->pipeline->subs[crtc_index]->obj.state);
	info = &mvps->plane_info[index];
	if (!info->enable) {
		DRM_INFO("osd is disabled\n");
		return 0;
	}

	phy_addr = info->phy_addr;
	fb_size = info->fb_size;
	bflg = false;
	if (pages == 0 && off < fb_size) {
		vir_addr = am_meson_drm_vmap(phy_addr, fb_size, &bflg);
		amp->bflg = bflg;
		amp->vir_addr = vir_addr;
		amp->dump_size = fb_size;
	}
	if (!amp->vir_addr) {
		DRM_INFO("vmap failed, vir_addr is null\n");
		return -EINVAL;
	}
	num_pages = PAGE_ALIGN(amp->dump_size) / PAGE_SIZE;
	pages++;

	if (pages <= num_pages && off < amp->dump_size) {
		memcpy(buf, amp->vir_addr + off, count);
		if (pages == num_pages && amp->bflg)
			am_meson_drm_unmap_phyaddr(amp->vir_addr);
		return count;
	}

	if (off >= amp->dump_size)
		pages = 0;

	return 0;
}

static ssize_t osd_fbdump_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	return count;
}

static ssize_t osd_blank_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int osd_index = *(int *)attr->private;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1 > enable osd-%d blank\n", *(int *)attr->private);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0 > disable osd-%d blank\n", *(int *)attr->private);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"osd-%d blank status is %d\n", *(int *)attr->private,
		priv->osd_planes[osd_index]->osd_permanent_blank);

	return pos;
}

static ssize_t osd_blank_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int osd_index = *(int *)attr->private;
	struct meson_drm *priv;
	int ret;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (buf[0] != '0' && buf[0] != '1')
		return -EINVAL;

	priv = minor->dev->dev_private;

	if (buf[0] == '1') {
		priv->osd_planes[osd_index]->osd_permanent_blank = 1;
		DRM_INFO("osd-%d enable blank\n", osd_index);
	} else if (buf[0] == '0') {
		priv->osd_planes[osd_index]->osd_permanent_blank = 0;
		DRM_INFO("osd-%d disable blank\n", osd_index);
	} else {
		return -EINVAL;
	}

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

	return count;
}

static ssize_t osd_resize_src_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int osd_index = *(int *)attr->private;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo src_x src_y src_w src_h > resize_source to enable osd-%d resize src\n",
		osd_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo -1 -1 -1 -1 > resize_source to disable osd-%d resize src\n", osd_index);

	return pos;
}

static ssize_t osd_resize_src_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int osd_index = *(int *)attr->private;
	struct meson_drm *priv;
	unsigned int src_x, src_y, src_w, src_h;
	char dst_buf[64];
	char *bufp, *parm[8] = {NULL};
	int ret;
	int lens = strlen(buf);

	if (!minor || !minor->dev)
		return -EINVAL;

	if (lens > sizeof(dst_buf) - 1)
		return -EINVAL;

	memcpy(dst_buf, buf, lens);

	dst_buf[lens] = '\0';
	bufp = dst_buf;
	parse_param(bufp, (char **)&parm);

	priv = minor->dev->dev_private;

	if (!(parm[0] && parm[1] && parm[2] && parm[3])) {
		DRM_INFO("%s, parameters is incorrect.\n", __func__);
		return -EINVAL;
	}
	if (kstrtoint(parm[0], 10, &src_x) < 0)
		return -EINVAL;
	if (kstrtoint(parm[1], 10, &src_y) < 0)
		return -EINVAL;
	if (kstrtoint(parm[2], 10, &src_w) < 0)
		return -EINVAL;
	if (kstrtoint(parm[3], 10, &src_h) < 0)
		return -EINVAL;

	priv->osd_planes[osd_index]->adjust_src.x1 = src_x;
	priv->osd_planes[osd_index]->adjust_src.y1 = src_y;
	priv->osd_planes[osd_index]->adjust_src.x2 = src_w;
	priv->osd_planes[osd_index]->adjust_src.y2 = src_h;

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

	return count;
}

static ssize_t osd_zorder_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int crtc_index;
	u32 index;
	int pos = 0;
	struct meson_vpu_osd_layer_info *info;
	struct meson_vpu_sub_pipeline_state *mvps;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	index = *(int *)attr->private;
	priv = minor->dev->dev_private;
	crtc_index = priv->pipeline->osd_crtc_index[index];
	if (crtc_index == -1) {
		DRM_INFO("osd is disabled\n");
		return 0;
	}

	mvps = priv_to_sub_pipeline_state(priv->pipeline->subs[crtc_index]->obj.state);
	info = &mvps->plane_info[index];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"osd-%d zorder is %d\n", *(int *)attr->private, info->zorder);

	return pos;
}

static ssize_t osd_zorder_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int osd_index = *(int *)attr->private;
	struct meson_drm *priv;
	int new_zorder;
	char dst_buf[64];
	char *bufp, *parm[8] = {NULL};
	int ret;
	int lens = strlen(buf);

	if (!minor || !minor->dev)
		return -EINVAL;

	if (lens > sizeof(dst_buf) - 1)
		return -EINVAL;

	memcpy(dst_buf, buf, lens);

	dst_buf[lens] = '\0';
	bufp = dst_buf;
	parse_param(bufp, (char **)&parm);

	priv = minor->dev->dev_private;

	if (!parm[0]) {
		DRM_INFO("%s, parameters is incorrect.\n", __func__);
		return -EINVAL;
	}
	if (kstrtouint(parm[0], 10, &new_zorder) < 0)
		return -EINVAL;

	priv->osd_planes[osd_index]->zorder = new_zorder;

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

	return count;
}

static ssize_t osd_gfcd_hskip_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int osd_index = *(int *)attr->private;
	int pos = 0;
	struct meson_vpu_osd_layer_info *info;
	struct meson_vpu_sub_pipeline_state *mvps;
	int crtc_index;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	crtc_index = priv->pipeline->osd_crtc_index[osd_index];
	if (crtc_index == -1) {
		DRM_INFO("osd is disabled\n");
		return 0;
	}

	mvps = priv_to_sub_pipeline_state(priv->pipeline->subs[crtc_index]->obj.state);
	info = &mvps->plane_info[osd_index];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1 > gfcd_hskip to enable osd-%d gfcd hskip\n",
		osd_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0 > gfcd_hskip to disable osd-%d gfcd hskip\n", osd_index);
	if (info->process_unit == GFCD_AFBC || info->process_unit == GFCD_AFRC)
		pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"osd-%d gfcd enabled\n", osd_index);
	else
		pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"osd-%d gfcd disabled\n", osd_index);

	return pos;
}

static ssize_t osd_gfcd_hskip_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int osd_index = *(int *)attr->private;
	struct meson_drm *priv;
	struct meson_vpu_osd_layer_info *info;
	struct meson_vpu_sub_pipeline_state *mvps;
	int crtc_index;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (buf[0] != '0' && buf[0] != '1')
		return -EINVAL;

	priv = minor->dev->dev_private;
	crtc_index = priv->pipeline->osd_crtc_index[osd_index];
	if (crtc_index == -1) {
		DRM_INFO("osd-%d: crtc is disconnected, cannot set gfcd skip\n",
			osd_index);
		return -ENODEV;
	}

	mvps = priv_to_sub_pipeline_state(priv->pipeline->subs[crtc_index]->obj.state);
	info = &mvps->plane_info[osd_index];

	if (buf[0] == '1') {
		info->gfcd_hskip = 1;
		DRM_INFO("osd-%d enable gfcd hskip\n", osd_index);
	} else if (buf[0] == '0') {
		info->gfcd_hskip = 0;
		DRM_INFO("osd-%d disable gfcd hskip\n", osd_index);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t osd_gfcd_vskip_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int osd_index = *(int *)attr->private;
	int pos = 0;
	struct meson_vpu_osd_layer_info *info;
	struct meson_vpu_sub_pipeline_state *mvps;
	int crtc_index;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	crtc_index = priv->pipeline->osd_crtc_index[osd_index];
	if (crtc_index == -1) {
		DRM_INFO("osd is disabled\n");
		return 0;
	}

	mvps = priv_to_sub_pipeline_state(priv->pipeline->subs[crtc_index]->obj.state);
	info = &mvps->plane_info[osd_index];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1 > gfcd_vskip to enable osd-%d gfcd vskip\n",
		osd_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0 > gfcd_vskip to disable osd-%d gfcd vskip\n", osd_index);
	if (info->process_unit == GFCD_AFBC || info->process_unit == GFCD_AFRC)
		pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"osd-%d gfcd enabled\n", osd_index);
	else
		pos += snprintf(buf + pos, PAGE_SIZE - pos,
			"osd-%d gfcd disabled\n", osd_index);

	return pos;
}

static ssize_t osd_gfcd_vskip_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int osd_index = *(int *)attr->private;
	struct meson_drm *priv;
	struct meson_vpu_osd_layer_info *info;
	struct meson_vpu_sub_pipeline_state *mvps;
	int crtc_index;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (buf[0] != '0' && buf[0] != '1')
		return -EINVAL;

	priv = minor->dev->dev_private;
	crtc_index = priv->pipeline->osd_crtc_index[osd_index];
	if (crtc_index == -1) {
		DRM_INFO("osd-%d: crtc is disconnected, cannot set gfcd skip\n",
			osd_index);
		return -ENODEV;
	}

	mvps = priv_to_sub_pipeline_state(priv->pipeline->subs[crtc_index]->obj.state);
	info = &mvps->plane_info[osd_index];

	if (buf[0] == '1') {
		info->gfcd_vskip = 1;
		DRM_INFO("osd-%d enable gfcd vskip\n", osd_index);
	} else if (buf[0] == '0') {
		info->gfcd_vskip = 0;
		DRM_INFO("osd-%d disable gfcd vskip\n", osd_index);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t osd_resize_dst_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int osd_index = *(int *)attr->private;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo dst_x dst_y dst_w dst_h > resize_destination to enable osd-%d resize dst\n",
		osd_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo -1 -1 -1 -1 > resize_destination to disable osd-%d resize dst\n", osd_index);

	return pos;
}

static ssize_t osd_resize_dst_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int osd_index = *(int *)attr->private;
	struct meson_drm *priv;
	unsigned int dst_x, dst_y, dst_w, dst_h;
	char dst_buf[64];
	char *bufp, *parm[8] = {NULL};
	int ret;
	int lens = strlen(buf);

	if (!minor || !minor->dev)
		return -EINVAL;

	if (lens > sizeof(dst_buf) - 1)
		return -EINVAL;

	memcpy(dst_buf, buf, lens);

	dst_buf[lens] = '\0';
	bufp = dst_buf;
	parse_param(bufp, (char **)&parm);

	priv = minor->dev->dev_private;

	if (!(parm[0] && parm[1] && parm[2] && parm[3])) {
		DRM_INFO("%s, parameters is incorrect.\n", __func__);
		return -EINVAL;
	}
	if (kstrtoint(parm[0], 10, &dst_x) < 0)
		return -EINVAL;
	if (kstrtoint(parm[1], 10, &dst_y) < 0)
		return -EINVAL;
	if (kstrtoint(parm[2], 10, &dst_w) < 0)
		return -EINVAL;
	if (kstrtoint(parm[3], 10, &dst_h) < 0)
		return -EINVAL;

	priv->osd_planes[osd_index]->adjust_dst.x1 = dst_x;
	priv->osd_planes[osd_index]->adjust_dst.y1 = dst_y;
	priv->osd_planes[osd_index]->adjust_dst.x2 = dst_w;
	priv->osd_planes[osd_index]->adjust_dst.y2 = dst_h;

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("ailed, ret = %d\n", ret);

	return count;
}

static ssize_t osd_dimm_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int crtc_index;
	u32 index;
	int pos = 0;
	struct meson_vpu_osd_layer_info *info;
	struct meson_vpu_sub_pipeline_state *mvps;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	index = *(int *)attr->private;
	priv = minor->dev->dev_private;
	crtc_index = priv->pipeline->osd_crtc_index[index];
	if (crtc_index == -1) {
		DRM_INFO("osd is disabled\n");
		return 0;
	}

	mvps = priv_to_sub_pipeline_state(priv->pipeline->subs[crtc_index]->obj.state);
	info = &mvps->plane_info[index];

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0x7ff00000 > osd_dimm to set osd-%d red dimm\n",
		*(int *)attr->private);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0x400ffc00 > osd_dimm to set osd-%d green dimm\n",
		*(int *)attr->private);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0x400003ff > osd_dimm to set osd-%d blue dimm\n",
		*(int *)attr->private);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0x0 > osd_dimm to clear osd-%d dimm\n",
		*(int *)attr->private);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"osd-%d current dimm flag %d value 0x%x\n",
		*(int *)attr->private, info->osd_dimm.dimm_ctrl, info->osd_dimm.dimm_value);

	return pos;
}

static ssize_t osd_dimm_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int osd_index = *(int *)attr->private;
	struct meson_drm *priv;
	long osd_dimm;
	int ret;

	DRM_INFO("dimm IN!\n");

	if (!minor || !minor->dev)
		return -EINVAL;

	if (!buf[0]) {
		DRM_ERROR("parameters is incorrect.\n");
		return -EINVAL;
	}
	if (kstrtoul(buf, 16, &osd_dimm) < 0) {
		DRM_ERROR("osd-%d: invalid value\n", osd_index);
		return -EINVAL;
	}

	priv = minor->dev->dev_private;
	if (!priv) {
		DRM_ERROR("priv is null\n");
		return -EINVAL;
	}
	priv->osd_planes[osd_index]->osd_dimm.dimm_value = osd_dimm;
	priv->osd_planes[osd_index]->osd_dimm.dimm_ctrl = 2;

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

	return count;
}

static ssize_t state_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_print_iterator iter;
	struct drm_printer p;
	ssize_t ret;

	iter.data = buf;
	iter.start = off;
	iter.remain = count;

	p = drm_coredump_printer(&iter);
	drm_state_dump(minor->dev, &p);
	ret = count - iter.remain;
	return ret;
}

static ssize_t reg_dump_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_print_iterator iter;
	struct drm_printer p;
	struct meson_drm *priv;
	struct am_meson_crtc *amc;
	struct meson_vpu_pipeline *mvp1;
	struct meson_vpu_block *mvb;
	int i;
	ssize_t ret;

	iter.data = buf;
	iter.start = off;
	iter.remain = count;

	p = drm_coredump_printer(&iter);

	if (!minor || !minor->dev)
		return -EINVAL;

	priv = minor->dev->dev_private;
	amc = priv->crtcs[0];
	mvp1 = amc->pipeline;

	for (i = 0; i < MESON_MAX_BLOCKS; i++) {
		mvb = mvp1->mvbs[i];
		if (!mvb)
			continue;

		drm_printf(&p, "*************%s*************\n", mvb->name);
		if (mvb->ops && mvb->ops->dump_register)
			mvb->ops->dump_register(&p, mvb);
	}
	ret = count - iter.remain;

	return ret;
}

static ssize_t crtc_blank_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_crtc *crtc;
	struct am_meson_crtc *amc;
	int crtc_index = *(int *)attr->private;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;

	crtc = drm_crtc_from_index(minor->dev, crtc_index);
	if (!crtc)
		return -EINVAL;

	amc = to_am_meson_crtc(crtc);

	if (off > 0)
		return 0;

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1 > crtc_blank enable crtc-%d blank\n", crtc_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0 > crtc_blank disable crtc-%d blank\n", crtc_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"crtc-%d blank status is %d\n", crtc_index,
		amc->blank_enable);

	return pos;
}

static ssize_t crtc_blank_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_crtc *crtc;
	struct am_meson_crtc *amc;
	int crtc_index = *(int *)attr->private;
	int ret;

	if (!minor || !minor->dev)
		return -EINVAL;

	crtc = drm_crtc_from_index(minor->dev, crtc_index);
	if (!crtc)
		return -EINVAL;

	amc = to_am_meson_crtc(crtc);

	if (buf[0] != '0' && buf[0] != '1')
		return -EINVAL;

	if (buf[0] == '1') {
		amc->blank_enable = 1;
		DRM_INFO("crtc-%d enable blank\n", crtc_index);
	} else if (buf[0] == '0') {
		amc->blank_enable = 0;
		DRM_INFO("crtc-%d disable blank\n", crtc_index);
	} else {
		return -EINVAL;
	}

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

	return count;
}

struct drm_atomic_state *
meson_drm_duplicate_state(struct drm_device *dev, struct drm_modeset_acquire_ctx *ctx,
			  struct drm_crtc *dst_crtc, struct drm_connector *dst_conn,
			  const struct drm_display_mode *mode)
{
	struct drm_atomic_state *state;
	struct drm_connector *conn;
	struct drm_connector_state *conn_state;
	struct drm_connector_list_iter conn_iter;
	struct drm_plane *plane;
	struct drm_plane_state *plane_state;
	struct drm_crtc *crtc;
	struct drm_crtc_state *crtc_state;
	int hdisplay, vdisplay;
	int ret, err = 0;

	state = drm_atomic_state_alloc(dev);
	if (!state)
		return ERR_PTR(-ENOMEM);

	state->acquire_ctx = ctx;
	state->duplicated = true;

	drm_for_each_crtc(crtc, dev) {
		if (crtc != dst_crtc)
			continue;

		crtc_state = drm_atomic_get_crtc_state(state, crtc);
		if (IS_ERR(crtc_state)) {
			DRM_ERROR("%d, get_crtc_state error\n", __LINE__);
			err = PTR_ERR(crtc_state);
			goto free;
		}

		ret = drm_atomic_set_mode_for_crtc(crtc_state, mode);
		if (ret != 0) {
			DRM_ERROR("%d, set_mode_for_crtc error\n", __LINE__);
			goto free;
		}
		crtc_state->active = true;
	}

	drm_mode_get_hv_timing(mode, &hdisplay, &vdisplay);

	drm_for_each_plane(plane, dev) {
		plane_state = drm_atomic_get_plane_state(state, plane);
		if (IS_ERR(plane_state)) {
			err = PTR_ERR(plane_state);
			DRM_ERROR("%d, get_plane_state error\n", __LINE__);
			goto free;
		}
		plane_state->crtc_x = 0;
		plane_state->crtc_y = 0;
		plane_state->crtc_w = hdisplay;
		plane_state->crtc_h = vdisplay;
	}

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		if (conn != dst_conn)
			continue;

		conn_state = drm_atomic_get_connector_state(state, conn);
		if (IS_ERR(conn_state)) {
			err = PTR_ERR(conn_state);
			drm_connector_list_iter_end(&conn_iter);
			DRM_ERROR("%d, get_conn_state error\n", __LINE__);
			goto free;
		}
		ret = drm_atomic_set_crtc_for_connector(conn_state, dst_crtc);
		if (ret != 0) {
			DRM_ERROR("%d, set_crtc_for_connector error\n", __LINE__);
			goto free;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	/* clear the acquire context so that it isn't accidentally reused */
	state->acquire_ctx = NULL;

free:
	if (err < 0) {
		drm_atomic_state_put(state);
		state = ERR_PTR(err);
	}

	return state;
}

static ssize_t crtc_mode_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int found, num_modes, ret = 0;
	char mode_name[DRM_DISPLAY_MODE_LEN];
	struct drm_display_mode *mode = NULL;
	struct drm_connector *connector;
	struct drm_modeset_acquire_ctx *ctx;
	struct drm_crtc *crtc;
	struct am_meson_crtc *am_crtc;
	struct device *dev_ = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev_);
	struct drm_device *dev = minor->dev;
	struct meson_drm *private = dev->dev_private;
	int crtc_index = *(int *)attr->private;
	struct drm_atomic_state *state;

	crtc = &private->crtcs[crtc_index]->base;
	am_crtc = to_am_meson_crtc(crtc);

	memset(mode_name, 0, DRM_DISPLAY_MODE_LEN);
	memcpy(mode_name, buf, (strlen(buf) < DRM_DISPLAY_MODE_LEN) ?
	       strlen(buf) - 1 : DRM_DISPLAY_MODE_LEN - 1);

	DRM_INFO("drm set mode to %s\n", mode_name);
	/*init all connector and found matched uboot mode.*/
	found = 0;
	list_for_each_entry(connector, &dev->mode_config.connector_list, head) {
		drm_modeset_lock_all(dev);
		if (drm_modeset_is_locked(&dev->mode_config.connection_mutex))
			drm_modeset_unlock(&dev->mode_config.connection_mutex);
		num_modes = connector->funcs->fill_modes(connector,
							 dev->mode_config.max_width,
							 dev->mode_config.max_height);
		drm_modeset_unlock_all(dev);

		if (num_modes) {
			list_for_each_entry(mode, &connector->modes, head) {
				if (!strcmp(mode->name, mode_name)) {
					found = 1;
					break;
				}
			}
			if (found)
				break;
		}

		DRM_DEBUG("Connector[%d] status[%d], %d\n",
			connector->connector_type, connector->status, found);
	}

	if (!found) {
		DRM_INFO("No Found mode[%s]\n", mode_name);
		if (!strcmp("null", mode_name))
			drm_atomic_helper_shutdown(dev);

		return count;
	}

	DRM_INFO("Found Connector[%d] mode[%s]\n", connector->connector_type,
				mode->name);
	drm_modeset_lock_all(dev);
	ctx = dev->mode_config.acquire_ctx;
	state = meson_drm_duplicate_state(dev, ctx, crtc, connector, mode);
	ret = drm_atomic_helper_commit_duplicated_state(state, ctx);
	drm_modeset_unlock_all(dev);

	return count;
}

static ssize_t crtc_mode_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int pos = 0;
	struct drm_crtc *crtc;
	struct am_meson_crtc *am_crtc;
	struct drm_display_mode *mode;
	struct device *dev_ = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev_);
	struct drm_device *dev = minor->dev;
	struct meson_drm *private = dev->dev_private;
	int crtc_index = *(int *)attr->private;

	crtc = &private->crtcs[crtc_index]->base;
	am_crtc = to_am_meson_crtc(crtc);
	mode = &crtc->state->adjusted_mode;

	if (off > 0)
		return 0;

	pos += snprintf(buf + pos, PAGE_SIZE - pos, "%s\n", mode->name);
	return pos;
}

static ssize_t hdmitx_attr_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	char attr_str[16];
	int cs, cd;
	struct drm_crtc *crtc;
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;
	struct am_hdmitx_connector_state *am_conn_state;
	struct device *dev_ = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev_);
	struct drm_device *dev = minor->dev;
	struct meson_drm *private = dev->dev_private;
	int crtc_index = *(int *)attr->private;
	bool found = false;

	crtc = &private->crtcs[crtc_index]->base;
	memset(attr_str, 0, sizeof(attr_str));
	memcpy(attr_str, buf, (strlen(buf) < sizeof(attr_str)) ?
	       strlen(buf) - 1 : sizeof(attr_str) - 1);

	if (strstr(attr_str, "420"))
		cs = HDMI_COLORSPACE_YUV420;
	else if (strstr(attr_str, "422"))
		cs = HDMI_COLORSPACE_YUV422;
	else if (strstr(attr_str, "444"))
		cs = HDMI_COLORSPACE_YUV444;
	else if (strstr(attr_str, "rgb"))
		cs = HDMI_COLORSPACE_RGB;
	else
		cs = HDMI_COLORSPACE_YUV444;

	/*parse colorspace success*/
	if (strstr(attr_str, "12bit"))
		cd = 12;
	else if (strstr(attr_str, "10bit"))
		cd = 10;
	else if (strstr(attr_str, "8bit"))
		cd = 8;
	else
		cd = 8;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		if (conn->connector_type != DRM_MODE_CONNECTOR_HDMIA)
			continue;

		if (!conn->state)
			continue;

		if (conn->state->crtc == crtc) {
			found = true;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (found) {
		am_conn_state = to_am_hdmitx_connector_state(conn->state);
		am_conn_state->color_attr_para.colorformat = cs;
		am_conn_state->color_attr_para.bitdepth = cd;
		DRM_INFO("%s set cs-%d, cd-%d\n", __func__, cs, cd);
	} else {
		DRM_INFO("%s not found hdmi state\n", __func__);
	}

	return count;
}

static ssize_t hdmitx_attr_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int pos = 0;
	const char *colorspace;
	int cs, cd;
	struct drm_crtc *crtc;
	struct drm_connector *conn;
	struct drm_connector_list_iter conn_iter;
	struct am_hdmitx_connector_state *am_conn_state;
	struct device *dev_ = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev_);
	struct drm_device *dev = minor->dev;
	struct meson_drm *private = dev->dev_private;
	int crtc_index = *(int *)attr->private;
	bool found = false;

	crtc = &private->crtcs[crtc_index]->base;
	if (off > 0)
		return 0;

	drm_connector_list_iter_begin(dev, &conn_iter);
	drm_for_each_connector_iter(conn, &conn_iter) {
		if (conn->connector_type != DRM_MODE_CONNECTOR_HDMIA)
			continue;

		if (!conn->state)
			continue;

		if (conn->state->crtc == crtc) {
			found = true;
			break;
		}
	}
	drm_connector_list_iter_end(&conn_iter);

	if (!found) {
		DRM_INFO("no hdmi connector or not connected!\n");
		return pos;
	}

	am_conn_state = to_am_hdmitx_connector_state(conn->state);
	cs = am_conn_state->color_attr_para.colorformat;
	cd = am_conn_state->color_attr_para.bitdepth;

	switch (cs) {
	case HDMI_COLORSPACE_YUV420:
		colorspace = "420";
		break;
	case HDMI_COLORSPACE_YUV422:
		colorspace = "422";
		break;
	case HDMI_COLORSPACE_YUV444:
		colorspace = "444";
		break;
	case HDMI_COLORSPACE_RGB:
		colorspace = "rgb";
		break;
	default:
		colorspace = "rgb";
		DRM_ERROR("Unknown colospace value %d\n", cs);
		break;
	};

	pos += snprintf(buf + pos, PAGE_SIZE - pos, "%s,%dbit\n", colorspace, cd);

	return pos;
}

static ssize_t crtc_rdma_table_switch_show(struct file *filp,
			 struct kobject *kobj, struct bin_attribute *attr, char *buf,
			 loff_t off, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_crtc *crtc;
	int crtc_index = *(int *)attr->private;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;

	crtc = drm_crtc_from_index(minor->dev, crtc_index);
	if (!crtc)
		return -EINVAL;

	if (off > 0)
		return 0;

	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 1 > rdma_table_switch enable crtc-%d drm rdma table\n", crtc_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo 0 > rdma_table_switch disable crtc-%d drm rdma table\n", crtc_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"crtc-%d drm rdma table status is %d\n", crtc_index,
		rdma_tbl[crtc_index].flag);

	return pos;
}

static ssize_t crtc_rdma_table_switch_store(struct file *filp,
			 struct kobject *kobj, struct bin_attribute *attr, char *buf,
			 loff_t off, size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct drm_crtc *crtc;
	struct am_meson_crtc *amcrtc;
	int crtc_index = *(int *)attr->private;

	if (!minor || !minor->dev)
		return -EINVAL;

	crtc = drm_crtc_from_index(minor->dev, crtc_index);
	if (!crtc)
		return -EINVAL;

	amcrtc = to_am_meson_crtc(crtc);

	if (buf[0] != '0' && buf[0] != '1')
		return -EINVAL;

	if (buf[0] == '1') {
		amcrtc->rdma_table_enable = true;
		DRM_INFO("crtc-%d enable drm rdma table\n", crtc_index);
	} else if (buf[0] == '0') {
		amcrtc->rdma_table_enable = false;
		DRM_INFO("crtc-%d disable drm rdma table\n", crtc_index);
	} else {
		return -EINVAL;
	}

	return count;
}

static ssize_t video_resize_src_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int video_index = *(int *)attr->private;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo src_x src_y src_w src_h > resize_source to enable video-%d resize src\n",
		video_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo -1 -1 -1 -1 > resize_source to disable video-%d resize src\n", video_index);

	return pos;
}

static ssize_t video_resize_src_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int video_index = *(int *)attr->private;
	struct meson_drm *priv;
	unsigned int src_x, src_y, src_w, src_h;
	char dst_buf[64];
	char *bufp, *parm[8] = {NULL};
	int ret;
	int lens = strlen(buf);

	if (!minor || !minor->dev)
		return -EINVAL;

	if (lens > sizeof(dst_buf) - 1)
		return -EINVAL;

	memcpy(dst_buf, buf, lens);

	dst_buf[lens] = '\0';
	bufp = dst_buf;
	parse_param(bufp, (char **)&parm);

	priv = minor->dev->dev_private;

	if (!(parm[0] && parm[1] && parm[2] && parm[3])) {
		DRM_INFO("%s, parameters is incorrect.\n", __func__);
		return -EINVAL;
	}
	if (kstrtoint(parm[0], 10, &src_x) < 0)
		return -EINVAL;
	if (kstrtoint(parm[1], 10, &src_y) < 0)
		return -EINVAL;
	if (kstrtoint(parm[2], 10, &src_w) < 0)
		return -EINVAL;
	if (kstrtoint(parm[3], 10, &src_h) < 0)
		return -EINVAL;

	priv->video_planes[video_index]->adjust_src.x1 = src_x;
	priv->video_planes[video_index]->adjust_src.y1 = src_y;
	priv->video_planes[video_index]->adjust_src.x2 = src_w;
	priv->video_planes[video_index]->adjust_src.y2 = src_h;

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

	return count;
}

static ssize_t video_resize_dst_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	struct meson_drm *priv;
	int video_index = *(int *)attr->private;
	int pos = 0;

	if (!minor || !minor->dev)
		return -EINVAL;

	if (off > 0)
		return 0;

	priv = minor->dev->dev_private;
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo dst_x dst_y dst_w dst_h > resize_destination to enable video-%d resize dst\n",
		video_index);
	pos += snprintf(buf + pos, PAGE_SIZE - pos,
		"echo -1 -1 -1 -1 > resize_destination to disable video-%d resize dst\n",
		video_index);

	return pos;
}

static ssize_t video_resize_dst_store(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	struct device *dev = kobj_to_dev(kobj);
	struct drm_minor *minor = dev_get_drvdata(dev);
	int video_index = *(int *)attr->private;
	struct meson_drm *priv;
	unsigned int dst_x, dst_y, dst_w, dst_h;
	char dst_buf[64];
	char *bufp, *parm[8] = {NULL};
	int ret;
	int lens = strlen(buf);

	if (!minor || !minor->dev)
		return -EINVAL;

	if (lens > sizeof(dst_buf) - 1)
		return -EINVAL;

	memcpy(dst_buf, buf, lens);

	dst_buf[lens] = '\0';
	bufp = dst_buf;
	parse_param(bufp, (char **)&parm);

	priv = minor->dev->dev_private;

	if (!(parm[0] && parm[1] && parm[2] && parm[3])) {
		DRM_INFO("%s, parameters is incorrect.\n", __func__);
		return -EINVAL;
	}
	if (kstrtoint(parm[0], 10, &dst_x) < 0)
		return -EINVAL;
	if (kstrtoint(parm[1], 10, &dst_y) < 0)
		return -EINVAL;
	if (kstrtoint(parm[2], 10, &dst_w) < 0)
		return -EINVAL;
	if (kstrtoint(parm[3], 10, &dst_h) < 0)
		return -EINVAL;

	priv->video_planes[video_index]->adjust_dst.x1 = dst_x;
	priv->video_planes[video_index]->adjust_dst.y1 = dst_y;
	priv->video_planes[video_index]->adjust_dst.x2 = dst_w;
	priv->video_planes[video_index]->adjust_dst.y2 = dst_h;

	ret = meson_drm_trigger_atomic_commit(minor);
	if (ret)
		DRM_ERROR("failed, ret = %d\n", ret);

	return count;
}

static struct bin_attribute osd0_attr[] = {
	{
		.attr.name = "osd_reverse",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_reverse_show,
		.write = osd_reverse_store,
	},
	{
		.attr.name = "osd_blend_bypass",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_blend_bypass_show,
		.write = osd_blend_bypass_store,
	},
	{
		.attr.name = "osd_read_port",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_read_port_show,
		.write = osd_read_port_store,
	},
	{
		.attr.name = "fbdump",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_fbdump_show,
		.write = osd_fbdump_store,
		.size = 36864000,
	},
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_blank_show,
		.write = osd_blank_store,
	},
	{
		.attr.name = "pixel_blend",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_pixel_blend_show,
		.write = osd_pixel_blend_store,
	},
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_resize_src_show,
		.write = osd_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_resize_dst_show,
		.write = osd_resize_dst_store,
	},
	{
		.attr.name = "gfcd_hskip",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_gfcd_hskip_show,
		.write = osd_gfcd_hskip_store,
	},
	{
		.attr.name = "gfcd_vskip",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_gfcd_vskip_show,
		.write = osd_gfcd_vskip_store,
	},
	{
		.attr.name = "osd_zorder",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_zorder_show,
		.write = osd_zorder_store,
	},
	{
		.attr.name = "osd_dimm",
		.attr.mode = 0664,
		.private = &osd_index[0],
		.read = osd_dimm_show,
		.write = osd_dimm_store,
	},
};

static struct bin_attribute *osd0_bin_attrs[] = {
	&osd0_attr[0],
	&osd0_attr[1],
	&osd0_attr[2],
	&osd0_attr[3],
	&osd0_attr[4],
	&osd0_attr[5],
	&osd0_attr[6],
	&osd0_attr[7],
	&osd0_attr[8],
	&osd0_attr[9],
	&osd0_attr[10],
	&osd0_attr[11],
	NULL,
};

static struct bin_attribute osd1_attr[] = {
	{
		.attr.name = "osd_reverse",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_reverse_show,
		.write = osd_reverse_store,
	},
	{
		.attr.name = "osd_blend_bypass",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_blend_bypass_show,
		.write = osd_blend_bypass_store,
	},
	{
		.attr.name = "osd_read_port",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_read_port_show,
		.write = osd_read_port_store,
	},
	{
		.attr.name = "fbdump",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_fbdump_show,
		.write = osd_fbdump_store,
		.size = 36864000,
	},
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_blank_show,
		.write = osd_blank_store,
	},
	{
		.attr.name = "pixel_blend",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_pixel_blend_show,
		.write = osd_pixel_blend_store,
	},
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_resize_src_show,
		.write = osd_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_resize_dst_show,
		.write = osd_resize_dst_store,
	},
	{
		.attr.name = "gfcd_hskip",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_gfcd_hskip_show,
		.write = osd_gfcd_hskip_store,
	},
	{
		.attr.name = "gfcd_vskip",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_gfcd_vskip_show,
		.write = osd_gfcd_vskip_store,
	},
	{
		.attr.name = "osd_zorder",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_zorder_show,
		.write = osd_zorder_store,
	},
	{
		.attr.name = "osd_dimm",
		.attr.mode = 0664,
		.private = &osd_index[1],
		.read = osd_dimm_show,
		.write = osd_dimm_store,
	},
};

static struct bin_attribute *osd1_bin_attrs[] = {
	&osd1_attr[0],
	&osd1_attr[1],
	&osd1_attr[2],
	&osd1_attr[3],
	&osd1_attr[4],
	&osd1_attr[5],
	&osd1_attr[6],
	&osd1_attr[7],
	&osd1_attr[8],
	&osd1_attr[9],
	&osd1_attr[10],
	&osd1_attr[11],
	NULL,
};

static struct bin_attribute osd2_attr[] = {
	{
		.attr.name = "osd_reverse",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_reverse_show,
		.write = osd_reverse_store,
	},
	{
		.attr.name = "osd_blend_bypass",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_blend_bypass_show,
		.write = osd_blend_bypass_store,
	},
	{
		.attr.name = "osd_read_port",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_read_port_show,
		.write = osd_read_port_store,
	},
	{
		.attr.name = "fbdump",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_fbdump_show,
		.write = osd_fbdump_store,
		.size = 36864000,
	},
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_blank_show,
		.write = osd_blank_store,
	},
	{
		.attr.name = "pixel_blend",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_pixel_blend_show,
		.write = osd_pixel_blend_store,
	},
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_resize_src_show,
		.write = osd_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_resize_dst_show,
		.write = osd_resize_dst_store,
	},
	{
		.attr.name = "gfcd_hskip",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_gfcd_hskip_show,
		.write = osd_gfcd_hskip_store,
	},
	{
		.attr.name = "gfcd_vskip",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_gfcd_vskip_show,
		.write = osd_gfcd_vskip_store,
	},
	{
		.attr.name = "osd_zorder",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_zorder_show,
		.write = osd_zorder_store,
	},
	{
		.attr.name = "osd_dimm",
		.attr.mode = 0664,
		.private = &osd_index[2],
		.read = osd_dimm_show,
		.write = osd_dimm_store,
	},
};

static struct bin_attribute *osd2_bin_attrs[] = {
	&osd2_attr[0],
	&osd2_attr[1],
	&osd2_attr[2],
	&osd2_attr[3],
	&osd2_attr[4],
	&osd2_attr[5],
	&osd2_attr[6],
	&osd2_attr[7],
	&osd2_attr[8],
	&osd2_attr[9],
	&osd2_attr[10],
	&osd2_attr[11],
	NULL,
};

static struct bin_attribute osd3_attr[] = {
	{
		.attr.name = "osd_reverse",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_reverse_show,
		.write = osd_reverse_store,
	},
	{
		.attr.name = "osd_blend_bypass",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_blend_bypass_show,
		.write = osd_blend_bypass_store,
	},
	{
		.attr.name = "osd_read_port",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_read_port_show,
		.write = osd_read_port_store,
	},
	{
		.attr.name = "fbdump",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_fbdump_show,
		.write = osd_fbdump_store,
		.size = 36864000,
	},
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_blank_show,
		.write = osd_blank_store,
	},
	{
		.attr.name = "pixel_blend",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_pixel_blend_show,
		.write = osd_pixel_blend_store,
	},
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_resize_src_show,
		.write = osd_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_resize_dst_show,
		.write = osd_resize_dst_store,
	},
	{
		.attr.name = "gfcd_hskip",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_gfcd_hskip_show,
		.write = osd_gfcd_hskip_store,
	},
	{
		.attr.name = "gfcd_vskip",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_gfcd_vskip_show,
		.write = osd_gfcd_vskip_store,
	},
	{
		.attr.name = "osd_zorder",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_zorder_show,
		.write = osd_zorder_store,
	},
	{
		.attr.name = "osd_dimm",
		.attr.mode = 0664,
		.private = &osd_index[3],
		.read = osd_dimm_show,
		.write = osd_dimm_store,
	},
};

static struct bin_attribute *osd3_bin_attrs[] = {
	&osd3_attr[0],
	&osd3_attr[1],
	&osd3_attr[2],
	&osd3_attr[3],
	&osd3_attr[4],
	&osd3_attr[5],
	&osd3_attr[6],
	&osd3_attr[7],
	&osd3_attr[8],
	&osd3_attr[9],
	&osd3_attr[10],
	&osd3_attr[11],
	NULL,
};

static struct bin_attribute osd4_attr[] = {
	{
		.attr.name = "osd_reverse",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_reverse_show,
		.write = osd_reverse_store,
	},
	{
		.attr.name = "osd_blend_bypass",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_blend_bypass_show,
		.write = osd_blend_bypass_store,
	},
	{
		.attr.name = "osd_read_port",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_read_port_show,
		.write = osd_read_port_store,
	},
	{
		.attr.name = "fbdump",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_fbdump_show,
		.write = osd_fbdump_store,
		.size = 36864000,
	},
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_blank_show,
		.write = osd_blank_store,
	},
	{
		.attr.name = "pixel_blend",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_pixel_blend_show,
		.write = osd_pixel_blend_store,
	},
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_resize_src_show,
		.write = osd_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_resize_dst_show,
		.write = osd_resize_dst_store,
	},
	{
		.attr.name = "gfcd_hskip",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_gfcd_hskip_show,
		.write = osd_gfcd_hskip_store,
	},
	{
		.attr.name = "gfcd_vskip",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_gfcd_vskip_show,
		.write = osd_gfcd_vskip_store,
	},
	{
		.attr.name = "osd_zorder",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_zorder_show,
		.write = osd_zorder_store,
	},
	{
		.attr.name = "osd_dimm",
		.attr.mode = 0664,
		.private = &osd_index[4],
		.read = osd_dimm_show,
		.write = osd_dimm_store,
	},
};

static struct bin_attribute *osd4_bin_attrs[] = {
	&osd4_attr[0],
	&osd4_attr[1],
	&osd4_attr[2],
	&osd4_attr[3],
	&osd4_attr[4],
	&osd4_attr[5],
	&osd4_attr[6],
	&osd4_attr[7],
	&osd4_attr[8],
	&osd4_attr[9],
	&osd4_attr[10],
	&osd4_attr[11],
	NULL,
};

static struct bin_attribute video0_attr[] = {
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &video_index[0],
		.read = video_resize_src_show,
		.write = video_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &video_index[0],
		.read = video_resize_dst_show,
		.write = video_resize_dst_store,
	},
};

static struct bin_attribute *video0_bin_attrs[] = {
	&video0_attr[0],
	&video0_attr[1],
	NULL,
};

static struct bin_attribute video1_attr[] = {
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &video_index[1],
		.read = video_resize_src_show,
		.write = video_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &video_index[1],
		.read = video_resize_dst_show,
		.write = video_resize_dst_store,
	},
};

static struct bin_attribute *video1_bin_attrs[] = {
	&video1_attr[0],
	&video1_attr[1],
	NULL,
};

static struct bin_attribute video2_attr[] = {
	{
		.attr.name = "resize_source",
		.attr.mode = 0664,
		.private = &video_index[2],
		.read = video_resize_src_show,
		.write = video_resize_src_store,
	},
	{
		.attr.name = "resize_destination",
		.attr.mode = 0664,
		.private = &video_index[2],
		.read = video_resize_dst_show,
		.write = video_resize_dst_store,
	},
};

static struct bin_attribute *video2_bin_attrs[] = {
	&video2_attr[0],
	&video2_attr[1],
	NULL,
};

static const struct attribute_group osd_attr_group[MESON_MAX_OSDS] = {
	{
		.name = osd0_group_name,
		.bin_attrs = osd0_bin_attrs,
	},
	{
		.name = osd1_group_name,
		.bin_attrs = osd1_bin_attrs,
	},
	{
		.name = osd2_group_name,
		.bin_attrs = osd2_bin_attrs,
	},
	{
		.name = osd3_group_name,
		.bin_attrs = osd3_bin_attrs,
	},
	{
		.name = osd4_group_name,
		.bin_attrs = osd4_bin_attrs,
	},
};

AM_BIN_ATTR_MOD_PARAM_R_DEF(crtc_force_hint)
AM_BIN_ATTR_MOD_PARAM_W_DEF(crtc_force_hint)
AM_BIN_ATTR_MOD_PARAM_R_DEF(flush_time)
AM_BIN_ATTR_MOD_PARAM_W_DEF(flush_time)
AM_BIN_ATTR_MOD_PARAM_R_DEF(osd_slice_mode)
AM_BIN_ATTR_MOD_PARAM_W_DEF(osd_slice_mode)
AM_BIN_ATTR_MOD_PARAM_R_DEF(afbc_order_conf)
AM_BIN_ATTR_MOD_PARAM_W_DEF(afbc_order_conf)
AM_BIN_ATTR_MOD_PARAM_R_DEF(osdscaler_force_update)
AM_BIN_ATTR_MOD_PARAM_W_DEF(osdscaler_force_update)
AM_BIN_ATTR_MOD_PARAM_R_DEF(osdscaler_v_filter_mode)
AM_BIN_ATTR_MOD_PARAM_W_DEF(osdscaler_v_filter_mode)
AM_BIN_ATTR_MOD_PARAM_R_DEF(osdscaler_h_filter_mode)
AM_BIN_ATTR_MOD_PARAM_W_DEF(osdscaler_h_filter_mode)
AM_BIN_ATTR_MOD_PARAM_R_DEF(osd_hold_line)
AM_BIN_ATTR_MOD_PARAM_W_DEF(osd_hold_line)
AM_BIN_ATTR_MOD_PARAM_R_DEF(force_mali_afbc_mode)
AM_BIN_ATTR_MOD_PARAM_W_DEF(force_mali_afbc_mode)
AM_BIN_ATTR_MOD_PARAM_R_DEF(async_commit_ioctl_threshold)
AM_BIN_ATTR_MOD_PARAM_W_DEF(async_commit_ioctl_threshold)
AM_BIN_ATTR_MOD_PARAM_R_DEF(fbdev_pan_display_threshold)
AM_BIN_ATTR_MOD_PARAM_W_DEF(fbdev_pan_display_threshold)
AM_BIN_ATTR_MOD_PARAM_R_DEF(osdscaler_safety_factor)
AM_BIN_ATTR_MOD_PARAM_W_DEF(osdscaler_safety_factor)

static struct bin_attribute module_param_attr[] = {
AM_BIN_ATTR_MOD_PARAM_RW(crtc_force_hint)
AM_BIN_ATTR_MOD_PARAM_RW(flush_time)
AM_BIN_ATTR_MOD_PARAM_RW(osd_slice_mode)
AM_BIN_ATTR_MOD_PARAM_RW(afbc_order_conf)
AM_BIN_ATTR_MOD_PARAM_RW(osdscaler_force_update)
AM_BIN_ATTR_MOD_PARAM_RW(osdscaler_v_filter_mode)
AM_BIN_ATTR_MOD_PARAM_RW(osdscaler_h_filter_mode)
AM_BIN_ATTR_MOD_PARAM_RW(osd_hold_line)
AM_BIN_ATTR_MOD_PARAM_RW(force_mali_afbc_mode)
AM_BIN_ATTR_MOD_PARAM_RW(async_commit_ioctl_threshold)
AM_BIN_ATTR_MOD_PARAM_RW(fbdev_pan_display_threshold)
AM_BIN_ATTR_MOD_PARAM_RW(osdscaler_safety_factor)
};

static struct attribute_group module_param_group = {
	.name = module_param_group_name,
};

static const struct attribute_group video_attr_group[MESON_MAX_VIDEOS] = {
	{
		.name = video0_group_name,
		.bin_attrs = video0_bin_attrs,
	},
	{
		.name = video1_group_name,
		.bin_attrs = video1_bin_attrs,
	},
	{
		.name = video2_group_name,
		.bin_attrs = video2_bin_attrs,
	},
};

static struct bin_attribute crtc0_attr[] = {
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &crtc_index[0],
		.read = crtc_blank_show,
		.write = crtc_blank_store,
	},
	{
		.attr.name = "mode",
		.attr.mode = 0664,
		.private = &crtc_index[0],
		.read = crtc_mode_show,
		.write = crtc_mode_store,
	},
	{
		.attr.name = "attr",
		.attr.mode = 0664,
		.private = &crtc_index[0],
		.read = hdmitx_attr_show,
		.write = hdmitx_attr_store,
	},
	{
		.attr.name = "rdma_table_switch",
		.attr.mode = 0664,
		.private = &crtc_index[0],
		.read = crtc_rdma_table_switch_show,
		.write = crtc_rdma_table_switch_store,
	},
};

static struct bin_attribute *crtc0_bin_attrs[] = {
	&crtc0_attr[0],
	&crtc0_attr[1],
	&crtc0_attr[2],
	&crtc0_attr[3],
	NULL,
};

static struct bin_attribute crtc1_attr[] = {
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &crtc_index[1],
		.read = crtc_blank_show,
		.write = crtc_blank_store,
	},
	{
		.attr.name = "mode",
		.attr.mode = 0664,
		.private = &crtc_index[1],
		.read = crtc_mode_show,
		.write = crtc_mode_store,
	},
	{
		.attr.name = "attr",
		.attr.mode = 0664,
		.private = &crtc_index[1],
		.read = hdmitx_attr_show,
		.write = hdmitx_attr_store,
	},
	{
		.attr.name = "rdma_table_switch",
		.attr.mode = 0664,
		.private = &crtc_index[1],
		.read = crtc_rdma_table_switch_show,
		.write = crtc_rdma_table_switch_store,
	},
};

static struct bin_attribute *crtc1_bin_attrs[] = {
	&crtc1_attr[0],
	&crtc1_attr[1],
	&crtc1_attr[2],
	&crtc1_attr[3],
	NULL,
};

static struct bin_attribute crtc2_attr[] = {
	{
		.attr.name = "blank",
		.attr.mode = 0664,
		.private = &crtc_index[2],
		.read = crtc_blank_show,
		.write = crtc_blank_store,
	},
	{
		.attr.name = "mode",
		.attr.mode = 0664,
		.private = &crtc_index[2],
		.read = crtc_mode_show,
		.write = crtc_mode_store,
	},
	{
		.attr.name = "attr",
		.attr.mode = 0664,
		.private = &crtc_index[2],
		.read = hdmitx_attr_show,
		.write = hdmitx_attr_store,
	},
	{
		.attr.name = "rdma_table_switch",
		.attr.mode = 0664,
		.private = &crtc_index[2],
		.read = crtc_rdma_table_switch_show,
		.write = crtc_rdma_table_switch_store,
	},
};

static struct bin_attribute *crtc2_bin_attrs[] = {
	&crtc2_attr[0],
	&crtc2_attr[1],
	&crtc2_attr[2],
	&crtc2_attr[3],
	NULL,
};

static const struct attribute_group crtc_attr_group[MESON_MAX_POSTBLEND] = {
	{
		.name = crtc0_group_name,
		.bin_attrs = crtc0_bin_attrs,
	},
	{
		.name = crtc1_group_name,
		.bin_attrs = crtc1_bin_attrs,
	},
	{
		.name = crtc2_group_name,
		.bin_attrs = crtc2_bin_attrs,
	},
};

static struct bin_attribute state_attr = {
	.attr.name = "state",
	.attr.mode = 0664,
	.read = state_show,
};

static struct bin_attribute reg_dump_attr = {
	.attr.name = "reg_dump",
	.attr.mode = 0664,
	.read = reg_dump_show,
};

int meson_drm_sysfs_register(struct drm_device *drm_dev)
{
	int rc, i;
	struct meson_drm *priv = drm_dev->dev_private;
	struct device *dev = drm_dev->primary->kdev;
	struct bin_attribute **module_param_bin_attrs;

	rc = sysfs_create_group(&dev->kobj, &vpu_attr_group);

	rc = sysfs_create_bin_file(&dev->kobj, &state_attr);
	rc = sysfs_create_bin_file(&dev->kobj, &reg_dump_attr);

	module_param_bin_attrs = devm_kcalloc(dev, sizeof(struct bin_attribute *),
			ARRAY_SIZE(module_param_attr) + 1, GFP_KERNEL); /* Zeroed */
	if (module_param_bin_attrs) {
		for (i = 0; i < ARRAY_SIZE(module_param_attr); i++)
			module_param_bin_attrs[i] = &module_param_attr[i];

		module_param_group.bin_attrs = module_param_bin_attrs;
		rc = sysfs_create_group(&dev->kobj, &module_param_group);
	} else {
		DRM_ERROR("Failed to calloc param bin attrs!\n");
	}

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!priv->pipeline->osds[i])
			continue;
		rc = sysfs_create_group(&dev->kobj, &osd_attr_group[i]);
	}
	for (i = 0; i < MESON_MAX_VIDEOS; i++) {
		if (!priv->pipeline->video[i])
			continue;
		rc = sysfs_create_group(&dev->kobj, &video_attr_group[i]);
	}

	for (i = 0; i < MESON_MAX_POSTBLEND; i++) {
		if (!priv->pipeline->postblends[i])
			continue;
		rc = sysfs_create_group(&dev->kobj, &crtc_attr_group[i]);
	}

	return rc;
}

void meson_drm_sysfs_unregister(struct drm_device *drm_dev)
{
	int i;
	struct meson_drm *priv = drm_dev->dev_private;
	struct device *dev = drm_dev->primary->kdev;

	sysfs_remove_group(&dev->kobj, &vpu_attr_group);
	sysfs_remove_bin_file(&dev->kobj, &state_attr);
	sysfs_remove_bin_file(&dev->kobj, &reg_dump_attr);

	if (module_param_group.bin_attrs) {
		sysfs_remove_group(&dev->kobj, &module_param_group);
		module_param_group.bin_attrs = NULL;
	}

	for (i = 0; i < MESON_MAX_OSDS; i++) {
		if (!priv->pipeline->osds[i])
			continue;
		sysfs_remove_group(&dev->kobj, &osd_attr_group[i]);
	}
	for (i = 0; i < MESON_MAX_VIDEOS; i++) {
		if (!priv->pipeline->video[i])
			continue;
		sysfs_remove_group(&dev->kobj, &video_attr_group[i]);
	}
	for (i = 0; i < MESON_MAX_POSTBLEND; i++) {
		if (!priv->pipeline->postblends[i])
			continue;
		sysfs_remove_group(&dev->kobj, &crtc_attr_group[i]);
	}
}

