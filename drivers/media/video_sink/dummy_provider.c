// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

/* Linux Headers */
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/amlogic/major.h>
#include <linux/platform_device.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/file.h>
#include <linux/dma-direction.h>
#include <linux/dma-buf.h>
#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif
#include <linux/of.h>
#include <linux/dma-mapping.h>
#include <linux/dma-map-ops.h>
#include <linux/cma.h>
#include <linux/of_reserved_mem.h>
#include <linux/io.h>

/* Amlogic Headers */
#include <linux/amlogic/ion.h>
#include <linux/amlogic/media/dev_ion.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>
#include <linux/amlogic/media/vfm/vfm_ext.h>
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#ifdef CONFIG_AMLOGIC_VOUT
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_VECM
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_VIDEO
#include <linux/amlogic/media/video_sink/video.h>
#endif
#include <linux/amlogic/tee.h>

/* Local Headers */
#include "video_priv.h"
#include "dummy_provider.h"

static struct video_provider_device_s video_provider_device;

static struct vframe_provider_s vp_vfm_prov;
static struct vp_pool_s vp_pool[VP_VFM_POOL_SIZE];
static s32 fill_ptr, get_ptr, put_ptr;
static int vp_canvas_table[VP_VFM_POOL_SIZE];

static struct vframe_provider_s vp_vfm_prov_pip;
static struct vp_pool_s vp_pool_pip[VP_VFM_POOL_SIZE];
static s32 fill_ptr_pip, get_ptr_pip, put_ptr_pip;
static int vp_canvas_table_pip[VP_VFM_POOL_SIZE];

static DEFINE_SPINLOCK(lock);
static struct vp_cma_info cma_info;
unsigned int dummy_video_log_level;
static unsigned int video_mosaic_en;

static int num = 4;
static int axis_0[4] = {0, 0, 1919, 1079};
module_param_array(axis_0, int, &num, 0664);
MODULE_PARM_DESC(axis_0, "\n axis_0\n");

static int axis_1[4] = {0, 1080, 1919, 2159};
module_param_array(axis_1, int, &num, 0664);
MODULE_PARM_DESC(axis_1, "\n axis_1\n");

static int axis_2[4] = {1920, 0, 3839, 1079};
module_param_array(axis_2, int, &num, 0664);
MODULE_PARM_DESC(axis_2, "\n axis_2\n");

static int axis_3[4] = {1920, 1080, 3839, 2159};
module_param_array(axis_3, int, &num, 0664);
MODULE_PARM_DESC(axis_3, "\n axis_3\n");

#define INCPTR(p) ptr_wrap_inc(&(p))
#define AFBC_ALLOC_SIZE 0x800000 /* 8MB for afbc test */

/* Used for the 2x2 mosaic mode */
static void *g_fg_vaddr;
static dma_addr_t g_fg_paddr;

static int parse_para(const char *para, int para_num, int *result)
{
	char *token = NULL;
	char *params, *params_base;
	int *out = result;
	int len = 0, count = 0;
	int res = 0;
	int ret = 0;

	if (!para)
		return 0;

	params = kstrdup(para, GFP_KERNEL);
	params_base = params;
	token = params;
	if (token) {
		len = strlen(token);
		do {
			token = strsep(&params, " ");
			if (!token)
				break;
			while (token &&
			       (isspace(*token) ||
				!isgraph(*token)) && len) {
				token++;
				len--;
			}
			if (len == 0)
				break;
			ret = kstrtoint(token, 0, &res);
			if (ret < 0)
				break;
			len = strlen(token);
			*out++ = res;
			count++;
		} while ((count < para_num) && (len > 0));
	}

	kfree(params_base);
	return count;
}

static inline void ptr_wrap_inc(u32 *ptr)
{
	u32 i = *ptr;

	i++;
	if (i >= VP_VFM_POOL_SIZE)
		i = 0;
	*ptr = i;
}

static inline u32 index2canvas(u32 index)
{
	return vp_canvas_table[index];
}

static inline u32 index2canvas_pip(u32 index)
{
	return vp_canvas_table_pip[index];
}

static int has_unused_pool(int patch_id)
{
	int i;
	struct vp_pool_s *pool = vp_pool;

	if (patch_id == 1)
		pool = vp_pool_pip;
	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		if (pool[i].used == 0)
			return i;
	}
	return -1;
}

static ssize_t log_level_show(const struct class *cla,
			      const struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", dummy_video_log_level);
}

static ssize_t log_level_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	if (ret) {
		vp_err("kstrtoint err\n");
		return -EINVAL;
	}

	vp_info("log_level: %d->%d\n", dummy_video_log_level, res);
	dummy_video_log_level = res;

	return count;
}

static ssize_t mosaic_en_show(const struct class *cla,
			      const struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 40, "%d\n", video_mosaic_en);
}

static ssize_t mosaic_en_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int res = 0;
	int ret = 0;

	ret = kstrtoint(buf, 0, &res);
	if (ret) {
		vp_err("kstrtoint err\n");
		return -EINVAL;
	}

	vp_info("video_mosaic_en: %d->%d\n", video_mosaic_en, res);
	video_mosaic_en = res;

	return count;
}

static ssize_t mosaic_axis_show(const struct class *cla,
			      const struct class_attribute *attr,
			      char *buf)
{
	return snprintf(buf, 80, "mosaic axis: %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		axis_0[0], axis_0[1], axis_0[2], axis_0[3],
		axis_1[0], axis_1[1], axis_1[2], axis_1[3],
		axis_2[0], axis_2[1], axis_2[2], axis_2[3],
		axis_3[0], axis_3[1], axis_3[2], axis_3[3]);
}

static ssize_t mosaic_axis_store(const struct class *cla,
			       const struct class_attribute *attr,
			       const char *buf, size_t count)
{
	int parsed[5];

	if (likely(parse_para(buf, 5, parsed) == 5)) {
		if (parsed[0] == 0) {
			axis_0[0] = parsed[1];
			axis_0[1] = parsed[2];
			axis_0[2] = parsed[3];
			axis_0[3] = parsed[4];
		} else if (parsed[0] == 1) {
			axis_1[0] = parsed[1];
			axis_1[1] = parsed[2];
			axis_1[2] = parsed[3];
			axis_1[3] = parsed[4];

		} else if (parsed[0] == 2) {
			axis_2[0] = parsed[1];
			axis_2[1] = parsed[2];
			axis_2[2] = parsed[3];
			axis_2[3] = parsed[4];
		} else if (parsed[0] == 3) {
			axis_3[0] = parsed[1];
			axis_3[1] = parsed[2];
			axis_3[2] = parsed[3];
			axis_3[3] = parsed[4];
		}
	}
	return count;
}

static CLASS_ATTR_RW(log_level);
static CLASS_ATTR_RW(mosaic_en);
static CLASS_ATTR_RW(mosaic_axis);

static struct attribute *video_provider_class_attrs[] = {
	&class_attr_log_level.attr,
	&class_attr_mosaic_en.attr,
	&class_attr_mosaic_axis.attr,
	NULL
};
ATTRIBUTE_GROUPS(video_provider_class);

static struct class video_provider_class = {
	.name = VIDEO_PROVIDER_NAME,
	.class_groups = video_provider_class_groups,
};

static struct vframe_s *vp_vfm_peek(void *op_arg)
{
	vp_dbg2("%s (%d).\n", __func__, get_ptr);
	if (get_ptr == fill_ptr)
		return NULL;
	return &vp_pool[get_ptr].vfm;
}

static struct vframe_s *vp_vfm_peek_pip(void *op_arg)
{
	vp_dbg2("%s (%d).\n", __func__, get_ptr_pip);
	if (get_ptr_pip == fill_ptr_pip)
		return NULL;
	return &vp_pool_pip[get_ptr_pip].vfm;
}

static struct vframe_s *vp_vfm_get(void *op_arg)
{
	struct vframe_s *vf;

	vp_dbg2("%s (%d).\n", __func__, get_ptr);

	if (get_ptr == fill_ptr)
		return NULL;
	vf = &vp_pool[get_ptr].vfm;
	INCPTR(get_ptr);

	return vf;
}

static struct vframe_s *vp_vfm_get_pip(void *op_arg)
{
	struct vframe_s *vf;

	vp_dbg2("%s (%d).\n", __func__, get_ptr_pip);

	if (get_ptr_pip == fill_ptr_pip)
		return NULL;
	vf = &vp_pool_pip[get_ptr_pip].vfm;
	INCPTR(get_ptr_pip);

	return vf;
}

static void vp_vfm_put(struct vframe_s *vf, void *op_arg)
{
	int i;
	int canvas_addr;

	vp_dbg2("%s %p.\n", __func__, vf);

	if (!vf)
		return;
	INCPTR(put_ptr);

	if (put_ptr == fill_ptr) {
		vp_info("buffer%d is being in use, skip\n", fill_ptr);
		return;
	}

	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		canvas_addr = index2canvas(i);
		if (vf->canvas0Addr == (canvas_addr & 0xff)) {
			vp_pool[i].used = 0;
			vp_dbg("******recycle buffer index : %d ******\n", i);
		}
	}
}

static void vp_vfm_put_pip(struct vframe_s *vf, void *op_arg)
{
	int i;
	int canvas_addr;

	vp_dbg2("%s %p.\n", __func__, vf);

	if (!vf)
		return;
	INCPTR(put_ptr_pip);

	if (put_ptr_pip == fill_ptr_pip) {
		vp_info("pip buffer%d is being in use, skip\n", fill_ptr_pip);
		return;
	}

	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		canvas_addr = index2canvas_pip(i);
		if (vf->canvas0Addr == (canvas_addr & 0xff)) {
			vp_pool_pip[i].used = 0;
			vp_dbg("******recycle buffer index : %d ******\n", i);
		}
	}
}

static int vp_event_cb(int type, void *data, void *private_data)
{
	vp_dbg2("%s type(0x%x).\n", __func__, type);

	return 0;
}

static int vp_event_cb_pip(int type, void *data, void *private_data)
{
	vp_dbg2("%s type(0x%x).\n", __func__, type);

	return 0;
}

static int vp_vfm_states(struct vframe_states *states, void *op_arg)
{
	int i;
	unsigned long flags;

	vp_dbg2("%s.\n", __func__);

	if (!states) {
		vp_err("vframe_states is NULL");
		return -EINVAL;
	}

	spin_lock_irqsave(&lock, flags);
	states->vf_pool_size = VP_VFM_POOL_SIZE;
	i = fill_ptr - get_ptr;
	if (i < 0)
		i += VP_VFM_POOL_SIZE;
	states->buf_avail_num = i;
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

static int vp_vfm_states_pip(struct vframe_states *states, void *op_arg)
{
	int i;
	unsigned long flags;

	if (!states) {
		vp_err("vframe_states is NULL");
		return -EINVAL;
	}

	spin_lock_irqsave(&lock, flags);
	states->vf_pool_size = VP_VFM_POOL_SIZE;
	i = fill_ptr_pip - get_ptr_pip;
	if (i < 0)
		i += VP_VFM_POOL_SIZE;
	states->buf_avail_num = i;
	spin_unlock_irqrestore(&lock, flags);
	return 0;
}

static const struct vframe_operations_s vp_vfm_ops = {
	.peek = vp_vfm_peek,
	.get = vp_vfm_get,
	.put = vp_vfm_put,
	.event_cb = vp_event_cb,
	.vf_states = vp_vfm_states,
};

static const struct vframe_operations_s vp_vfm_ops_pip = {
	.peek = vp_vfm_peek_pip,
	.get = vp_vfm_get_pip,
	.put = vp_vfm_put_pip,
	.event_cb = vp_event_cb_pip,
	.vf_states = vp_vfm_states_pip,
};

static void video_provider_release_path(void)
{
	vf_unreg_provider(&vp_vfm_prov);
	vfm_map_remove(VP_VFPATH_ID);
}

static void video_provider_release_path_pip(void)
{
	vf_unreg_provider(&vp_vfm_prov_pip);
	vfm_map_remove(VP_VFPATH_ID_PIP);
}

static int video_provider_creat_path(void)
{
	int ret = -1;
	char path_id[] = VP_VFPATH_ID;
	char path_chain[] = VP_VFPATH_CHAIN;

	if (vfm_map_add(path_id, path_chain) < 0) {
		vp_err("video_provider map creation failed\n");
		return -ENOMEM;
	}

	vf_provider_init(&vp_vfm_prov, VIDEO_PROVIDER_NAME,
		&vp_vfm_ops, NULL);
	ret = vf_reg_provider(&vp_vfm_prov);
	if (ret < 0)
		vp_info("vfm path is already created\n");
	ret = vf_notify_receiver(VIDEO_PROVIDER_NAME,
					VFRAME_EVENT_PROVIDER_START, NULL);
	if (ret < 0) {
		vp_err("notify receiver error\n");
		video_provider_release_path();
	}

	return ret;
}

static int video_provider_creat_path_pip(void)
{
	int ret = -1;
	char path_id[] = VP_VFPATH_ID_PIP;
	char path_chain[] = VP_VFPATH_CHAIN_PIP;

	if (vfm_map_add(path_id, path_chain) < 0) {
		vp_err("video_provider map creation failed\n");
		return -ENOMEM;
	}

	vf_provider_init(&vp_vfm_prov_pip, VIDEO_PROVIDER_NAME_PIP,
		&vp_vfm_ops_pip, NULL);
	ret = vf_reg_provider(&vp_vfm_prov_pip);
	if (ret < 0)
		vp_info("pip vfm path is already created\n");
	ret = vf_notify_receiver(VIDEO_PROVIDER_NAME_PIP,
					VFRAME_EVENT_PROVIDER_START, NULL);
	if (ret < 0) {
		vp_err("pip notify receiver error\n");
		video_provider_release_path_pip();
	}

	return ret;
}

static int canvas_table_alloc(void)
{
	int i;

	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		if (vp_canvas_table[i])
			break;
	}

	/* alloc 2 * VP_VFM_POOL_SIZE for multi planes */
	if (i == VP_VFM_POOL_SIZE) {
		u32 canvas_table[VP_VFM_POOL_SIZE * 2];

		if (canvas_pool_alloc_canvas_table("video_provider",
						canvas_table,
						VP_VFM_POOL_SIZE * 2,
						CANVAS_MAP_TYPE_1)) {
			pr_err("%s allocate canvas error.\n", __func__);
			return -ENOMEM;
		}
		for (i = 0; i < VP_VFM_POOL_SIZE; i++)
			vp_canvas_table[i] = (canvas_table[2 * i] |
						(canvas_table[2 * i + 1] << 8));
	} else {
		vp_info("canvas_table is already alloced");
	}

	return 0;
}

static int canvas_table_alloc_pip(void)
{
	int i;

	for (i = 0; i < VP_VFM_POOL_SIZE; i++) {
		if (vp_canvas_table_pip[i])
			break;
	}

	/* alloc 2 * VP_VFM_POOL_SIZE for multi planes */
	if (i == VP_VFM_POOL_SIZE) {
		u32 canvas_table[VP_VFM_POOL_SIZE * 2];

		if (canvas_pool_alloc_canvas_table("video_provider_pip",
						canvas_table,
						VP_VFM_POOL_SIZE * 2,
						CANVAS_MAP_TYPE_1)) {
			pr_err("pip %s allocate canvas error.\n", __func__);
			return -ENOMEM;
		}
		for (i = 0; i < VP_VFM_POOL_SIZE; i++)
			vp_canvas_table_pip[i] = (canvas_table[2 * i] |
						(canvas_table[2 * i + 1] << 8));
	} else {
		vp_info("pip canvas_table is already alloced");
	}

	return 0;
}

static void canvas_table_release(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vp_canvas_table); i++) {
		if (vp_canvas_table[i]) {
			if (vp_canvas_table[i] & 0xff)
				canvas_pool_map_free_canvas
					(vp_canvas_table[i] & 0xff);
			if ((vp_canvas_table[i] >> 8) & 0xff)
				canvas_pool_map_free_canvas
					((vp_canvas_table[i] >> 8) & 0xff);
			if ((vp_canvas_table[i] >> 16) & 0xff)
				canvas_pool_map_free_canvas
					((vp_canvas_table[i] >> 16) & 0xff);
		}
		vp_canvas_table[i] = 0;
	}
}

static void canvas_table_release_pip(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(vp_canvas_table_pip); i++) {
		if (vp_canvas_table_pip[i]) {
			if (vp_canvas_table_pip[i] & 0xff)
				canvas_pool_map_free_canvas
					(vp_canvas_table_pip[i] & 0xff);
			if ((vp_canvas_table_pip[i] >> 8) & 0xff)
				canvas_pool_map_free_canvas
					((vp_canvas_table_pip[i] >> 8) & 0xff);
			if ((vp_canvas_table_pip[i] >> 16) & 0xff)
				canvas_pool_map_free_canvas
					((vp_canvas_table_pip[i] >> 16) & 0xff);
		}
		vp_canvas_table_pip[i] = 0;
	}
}

#define FGRAIN_TBL_SIZE  (498 * 16)
#define FGRAIN_TBL_MOSAIC_SIZE  (FGRAIN_TBL_SIZE * 2)

static struct video_composer_private vc_private;
struct vframe_s mosaic_vf[4];

#define ORDER 4  // 2^4 = 16 pages = 64KB aligned, for tee_protect_mem_by_type()

static int video_provider_open(struct inode *inode, struct file *file)
{
	int ret = -1;

	if (video_mosaic_en) {
		g_fg_vaddr = (void *)__get_free_pages(GFP_KERNEL, ORDER);
		if (!g_fg_vaddr) {
			vp_err("%s, failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		g_fg_paddr = virt_to_phys(g_fg_vaddr);
		vp_dbg("%s, alloc paddr:0x%llx\n", __func__, (unsigned long long)g_fg_paddr);
	}
	_video_set_disable(VIDEO_DISABLE_FORNEXT);
	video_set_global_output(0, 1);
	_videopip_set_disable(1, VIDEO_DISABLE_FORNEXT);
	video_set_global_output(1, 1);

	ret = video_provider_creat_path();
	if (ret < 0)
		return -ENOMEM;

	ret = video_provider_creat_path_pip();
	if (ret < 0)
		return -ENOMEM;

	ret = canvas_table_alloc();
	if (ret < 0)
		return ret;

	ret = canvas_table_alloc_pip();
	if (ret < 0)
		return ret;

	fill_ptr = 0;
	get_ptr = 0;
	put_ptr = 0;

	fill_ptr_pip = 0;
	get_ptr_pip = 0;
	put_ptr_pip = 0;

	return 0;
}

static int vp_dma_buf_get_phys(int fd, unsigned long *addr)
{
	long ret = -1;
	struct dma_buf *dbuf = NULL;
	struct dma_buf_attachment *d_att = NULL;
	struct sg_table *sg = NULL;
	struct device *dev = &video_provider_device.pdev->dev;
	enum dma_data_direction dir = DMA_TO_DEVICE;
	struct page *page;

	if (fd < 0 || !dev) {
		vp_err("error input param or dev is null\n");
		return -EINVAL;
	}

	dbuf = dma_buf_get(fd);
	if (IS_ERR(dbuf)) {
		vp_err("failed to get dma buffer\n");
		return -EINVAL;
	}

	d_att = dma_buf_attach(dbuf, dev);
	if (IS_ERR(d_att)) {
		vp_err("failed to set dma attach\n");
		ret = -EINVAL;
		goto attach_err;
	}

	sg = dma_buf_map_attachment(d_att, dir);
	if (IS_ERR(sg)) {
		vp_err("failed to get dma sg\n");
		ret = -EINVAL;
		goto map_attach_err;
	} else {
		page = sg_page(sg->sgl);
		*addr = PFN_PHYS(page_to_pfn(page));
		ret = 0;
	}

	dma_buf_unmap_attachment(d_att, sg, dir);

map_attach_err:
	dma_buf_detach(dbuf, d_att);

attach_err:
	dma_buf_put(dbuf);

	return ret;
}

static int get_fram_phyaddr(struct vp_frame_s *frame_info, unsigned long *addr, int fg_test)
{
	int ret = -1;
	int share_fd;
	int mem_type;

	if (!frame_info || !addr) {
		vp_err("%s-%d frame_info is NULL\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (fg_test) {
		share_fd = frame_info->fg_data_shared_fd;
		mem_type = frame_info->fg_mem_type;
	} else {
		share_fd = frame_info->shared_fd;
		mem_type = frame_info->mem_type;
	}

	switch (mem_type) {
	case VP_MEM_ION:
#ifdef CONFIG_AMLOGIC_ION_DEV
		{
			size_t len = 0;

			ret = meson_ion_share_fd_to_phys(share_fd,
							 (phys_addr_t *)addr, &len);
			if (ret != 0)
				return ret;
			vp_dbg("ion frame addr 0x%lx, len %zu\n", *addr, len);
		}
#endif
		break;
	case VP_MEM_DMABUF:
		ret = vp_dma_buf_get_phys(share_fd, addr);
		if (ret != 0)
			return ret;
		vp_dbg("dma frame addr 0x%lx\n", *addr);
		break;
	default:
		vp_info("%s-%d mem type error\n", __func__, __LINE__);
		return -EINVAL;
	}

	return 0;
}

static int set_vfm_type(struct vp_frame_s *frame_info,
				struct vframe_s *vf, int *bpp)
{
	int _format = 0, _depth = 0;

	if (!frame_info || !vf || !bpp) {
		vp_info("%s-%d vf error\n", __func__, __LINE__);
		return -EINVAL;
	}
	switch (frame_info->format) {
	case VP_FMT_NV21:
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV21;
		vf->type_ext |= VIDTYPE_EXT_SWAP_UV;
		*bpp = 8;
		break;
	case VP_FMT_NV12:
		vf->type = VIDTYPE_PROGRESSIVE | VIDTYPE_VIU_FIELD |
				VIDTYPE_VIU_NV12;
		*bpp = 8;
		break;
	case VP_FMT_RGB888:
		*bpp = 24;
		vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE |
				VIDTYPE_VIU_FIELD;
		break;
	case VP_FMT_YUV444_PACKED:
		vf->type = VIDTYPE_VIU_444 | VIDTYPE_VIU_SINGLE_PLANE |
				VIDTYPE_VIU_FIELD;
		*bpp = 24;
		break;
	case VP_FMT_YUV444_3PLANE:
		vf->type = VIDTYPE_VIU_444 |
				VIDTYPE_VIU_FIELD;
		vf->plane_num = 3;
		*bpp = 8;
		break;
	case VP_FMT_AFBC:
		/* 0:YUV444 1:YUV422 2:YUV420 3:RGB */
		switch (frame_info->afbc_format) {
		case 0:
			_format = VIDTYPE_VIU_444;
			break;
		case 1:
			_format = VIDTYPE_VIU_422;
			break;
		case 2:
			_format = VIDTYPE_VIU_NV12;
			break;
		case 3:
			_format = VIDTYPE_RGB_444;
			break;
		default:
			vp_err("%s wrong afbc format:%d\n", __func__,
			       frame_info->afbc_format);
			break;
		}
		/* 0:8bit 1:10 2:12bit */
		switch (frame_info->bit_depth) {
		case 0:
			_depth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			break;
		case 1:
			_depth = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
			break;
		case 2:
			_depth = BITDEPTH_Y12 | BITDEPTH_U12 | BITDEPTH_V12;
			break;
		default:
			vp_err("%s wrong afbc bit depth:%d\n", __func__,
			       frame_info->bit_depth);
			break;
		}
		vf->type = VIDTYPE_COMPRESS | _format | VIDTYPE_SCATTER;
		vf->bitdepth = _depth;
		vf->source_type = VFRAME_SOURCE_TYPE_HDMI;
		break;
	case VP_FMT_AFRC:
		/* 0:YUV444 1:YUV422 2:YUV420 3:RGB */
		switch (frame_info->afrc_format) {
		case 0:
			_format = VIDTYPE_VIU_444;
			break;
		case 1:
			_format = VIDTYPE_VIU_422;
			break;
		case 2:
			_format = VIDTYPE_VIU_NV12;
			break;
		case 3:
			_format = VIDTYPE_RGB_444;
			break;
		default:
			vp_err("%s wrong afrc format:%d\n", __func__,
			       frame_info->afrc_format);
			break;
		}
		/* 0:8bit 1:10 2:12bit */
		switch (frame_info->bit_depth) {
		case 0:
			_depth = BITDEPTH_Y8 | BITDEPTH_U8 | BITDEPTH_V8;
			break;
		case 1:
			_depth = BITDEPTH_Y10 | BITDEPTH_U10 | BITDEPTH_V10;
			break;
		case 2:
			_depth = BITDEPTH_Y12 | BITDEPTH_U12 | BITDEPTH_V12;
			break;
		default:
			vp_err("%s wrong afrc bit depth:%d\n", __func__,
			       frame_info->bit_depth);
			break;
		}
		vf->type = VIDTYPE_COMPRESS | _format | VIDTYPE_SCATTER;
		vf->type_ext |= VIDTYPE_EXT_AFRC_COMPRESS;
		vf->bitdepth = _depth;
		vf->source_type = VFRAME_SOURCE_TYPE_HDMI;
		break;
	case VP_FMT_P010:
		vf->type = VIDTYPE_VIU_NV12; // VIDTYPE_VIU_NV12
		vf->canvas0_config[0].bit_depth = P010_MODE;
		vf->type_ext |= VIDTYPE_EXT_SWAP_UV;
		*bpp = 16;
		break;
	default:
		vp_info("%s-%d vf error\n", __func__, __LINE__);
		return -EINVAL;
	}
	if (frame_info->luma_only)
		vf->type_ext |= VIDTYPE_EXT_LUMA_ONLY;
	if (frame_info->secure)
		vf->flag |= VFRAME_FLAG_VIDEO_SECURE;
	switch (frame_info->endian) {
	case VP_BIG_ENDIAN:
		vf->flag &= ~VFRAME_FLAG_VIDEO_LINEAR;
		break;
	case VP_LITTLE_ENDIAN:
		vf->flag |= VFRAME_FLAG_VIDEO_LINEAR;
		break;
	default:
		vp_info("%s-%d vf error\n", __func__, __LINE__);
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_VOUT
	video_provider_device.vinfo = get_current_vinfo();

	/* indicate the vframe is a limited range frame */
	vf->signal_type =
		  (1 << 29)  /* video available */
		| (5 << 26)  /* unspecified */
		| (0 << 25)  /* limited */
		| (1 << 24); /* color available */
	if (video_provider_device.vinfo->width >= 1280 &&
		video_provider_device.vinfo->height >= 720) {
		/* >= 720p, use 709 */
		vf->signal_type |=
			(1 << 16) /* bt709 */
			| (1 << 8)  /* bt709 */
			| (1 << 0); /* bt709 */
	} else {
		/* < 720p, use 709 */
		vf->signal_type |=
			(3 << 16) /* bt601 */
			| (3 << 8)  /* bt601 */
			| (3 << 0); /* bt601 */
	}
#endif

	return 0;
}

static int set_vfm_info_from_frame(struct vp_frame_s *frame_info, int path_id)
{
	int ret = -1;
	int index;
	unsigned long addr = 0;
	unsigned long fg_data_addr = 0;
	unsigned long protect_addr = 0;
	unsigned long protect_size = 0;
	unsigned long protect_fg_addr = 0;
	unsigned long protect_fg_size = FGRAIN_TBL_SIZE;
#ifdef CONFIG_AMLOGIC_TEE
	u32 prot_ret;
	u32 secure_handle = 1;
#endif
	struct vframe_s *new_vf;
	int bpp;
	unsigned int canvas_width;
	int header_y_size, header_c_size, body_y_size, body_c_size;
	int table_y_size, table_c_size, target_byte = 32;
	int header_size, table_size, body_size;
	struct vp_pool_s *pool = vp_pool;
	s32 fill = fill_ptr;
	int *canvas_table = vp_canvas_table;

	if (path_id == 1) {
		pool = vp_pool_pip;
		fill = fill_ptr_pip;
		canvas_table = vp_canvas_table_pip;
	}
	index = has_unused_pool(path_id);
	if (index < 0) {
		vp_info("path_id:%d no buffer available, need post ASAP\n", path_id);
		return -ENOMEM;
	}
	memset(&pool[fill], 0, sizeof(struct vp_pool_s));

	if (frame_info->mem_type == VP_MEM_DRIVER_CMA) {
		if (!cma_info.alloc_page)
			return -EINVAL;
		addr = page_to_phys(cma_info.alloc_page);
	} else {
		ret = get_fram_phyaddr(frame_info, &addr, 0);
		if (ret < 0)
			return ret;
	}
	if (frame_info->secure)
		protect_addr = addr;

	if (frame_info->fg_test) {
		ret = get_fram_phyaddr(frame_info, &fg_data_addr, frame_info->fg_test);
		vp_info("get fg data phyaddr 0x%lx\n", fg_data_addr);
		if (ret < 0)
			return ret;
	}

	new_vf = &pool[fill].vfm;
	ret = set_vfm_type(frame_info, new_vf, &bpp);
	if (ret < 0)
		return ret;
	vp_dbg("vf type val=0x%x\n", new_vf->type);

	/* 256bit align, then calc bytes*/
	canvas_width = ((frame_info->width * bpp + 0xff) & ~0xff) / 8;
	vp_dbg("canvas_width val=%u\n", canvas_width);

	switch (frame_info->format) {
	case VP_FMT_NV21:
	case VP_FMT_NV12:
		canvas_config(canvas_table[fill] & 0xff,
				addr,
				canvas_width, frame_info->height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		addr += canvas_width * frame_info->height;
		canvas_config((canvas_table[fill] >> 8) & 0xff,
				addr,
				canvas_width, frame_info->height / 2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		new_vf->canvas0Addr = canvas_table[fill];
		if (frame_info->secure)
			protect_size = (frame_info->height * frame_info->width * 3) / 2;
		break;
	case VP_FMT_RGB888:
	case VP_FMT_YUV444_PACKED:

		if (frame_info->luma_only)
			canvas_config(canvas_table[fill] & 0xff,
					addr,
					frame_info->width, frame_info->height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		else
			canvas_config(canvas_table[fill] & 0xff,
					addr,
					canvas_width, frame_info->height,
					CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		new_vf->canvas0Addr = canvas_table[fill] & 0xff;
		if (frame_info->secure)
			protect_size = frame_info->height * frame_info->width * 3;
		break;
	case VP_FMT_AFBC:
		new_vf->compWidth    = frame_info->width;
		new_vf->compHeight   = frame_info->height;
		new_vf->compHeadAddr = addr;
		new_vf->compBodyAddr = addr + frame_info->offset;
		vp_dbg("afbc compHeadAddr:0x%lx compBodyAddr:0x%lx\n",
		       new_vf->compHeadAddr, new_vf->compBodyAddr);
		if (frame_info->secure)
			protect_size = AFBC_ALLOC_SIZE;
		break;
	case VP_FMT_AFRC:
		new_vf->compWidth    = frame_info->width;
		new_vf->compHeight   = frame_info->height;

		header_y_size = frame_info->width * (frame_info->height + 8) * 2;
		header_y_size /= (16 * 4);
		body_y_size = (frame_info->width * frame_info->height * target_byte);
		body_y_size /= (16 * 4);
		if (new_vf->type & VIDTYPE_VIU_444 ||
		    new_vf->type & VIDTYPE_RGB_444) {
			header_c_size = 2 * header_y_size;
			body_c_size = 2 * body_y_size;
		} else if (new_vf->type & VIDTYPE_VIU_422) {
			header_c_size = header_y_size;
			body_c_size = body_y_size;
		} else if (new_vf->type & VIDTYPE_VIU_NV12) {
			header_c_size = header_y_size / 2;
			body_c_size = body_y_size / 2;
		} else {
			vp_err("unsupported vf->type:0x%x\n", new_vf->type);
			return -EINVAL;
		}
		table_y_size = (body_y_size / PAGE_SIZE) * 4;
		table_c_size = (body_c_size / PAGE_SIZE) * 4;

		header_size = header_y_size + header_c_size;
		table_size = table_y_size + table_c_size;
		body_size = body_y_size + body_c_size;

		new_vf->afrc_info.luma_head_addr = addr;
		new_vf->afrc_info.luma_body_addr = addr + header_size + table_size;
		new_vf->afrc_info.luma_dict_en = 0;
		new_vf->afrc_info.luma_comp_target = 32;
		new_vf->afrc_info.luma_header_en = 1;

		new_vf->afrc_info.chrm_head_addr = addr + header_y_size;
		new_vf->afrc_info.chrm_body_addr = addr + header_size + table_size +
						body_y_size;
		new_vf->afrc_info.chrm_dict_en = 0;
		new_vf->afrc_info.chrm_comp_target = 32;
		new_vf->afrc_info.chrm_header_en = 1;

		new_vf->afrc_info.mmu_mode_en = 1;
		new_vf->afrc_info.mmu_page_mode = 0;
		new_vf->afrc_info.mmu_baddr0 = addr + header_size;
		new_vf->afrc_info.mmu_baddr1 = addr + header_size + table_y_size;
		if (frame_info->secure)
			protect_size = header_size + table_size + body_size;
		vp_dbg("afrc size info (header:%d %d)(table:%d %d)(body:%d %d)\n",
		       header_y_size, header_c_size,
		       table_y_size, table_c_size,
		       body_y_size, body_c_size);
		vp_dbg("afrc struct info (luma:0x%llx 0x%llx %d %d %d)(chroma:0x%llx 0x%llx %d %d %d)(table:%d %d 0x%x 0x%x)\n",
		       /* luma */
		       new_vf->afrc_info.luma_head_addr,
		       new_vf->afrc_info.luma_body_addr,
		       new_vf->afrc_info.luma_dict_en,
		       new_vf->afrc_info.luma_comp_target,
		       new_vf->afrc_info.luma_header_en,
		       /* chroma */
		       new_vf->afrc_info.chrm_head_addr,
		       new_vf->afrc_info.chrm_body_addr,
		       new_vf->afrc_info.chrm_dict_en,
		       new_vf->afrc_info.chrm_comp_target,
		       new_vf->afrc_info.chrm_header_en,
		       /* table(mmu) */
		       new_vf->afrc_info.mmu_mode_en,
		       new_vf->afrc_info.mmu_page_mode,
		       new_vf->afrc_info.mmu_baddr0,
		       new_vf->afrc_info.mmu_baddr1);
		break;
	case VP_FMT_YUV444_3PLANE:
		canvas_config(canvas_table[fill] & 0xff,
				addr,
				frame_info->width, frame_info->height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		addr += frame_info->width * frame_info->height;
		canvas_config((canvas_table[fill] >> 8) & 0xff,
				addr,
				frame_info->width, frame_info->height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		addr += frame_info->width * frame_info->height;
		canvas_config((canvas_table[fill] >> 16) & 0xff,
				addr,
				frame_info->width, frame_info->height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		new_vf->canvas0Addr = canvas_table[fill];
		if (frame_info->secure)
			protect_size = frame_info->height * frame_info->width * 3;
	case VP_FMT_P010:
		canvas_config(vp_canvas_table[fill_ptr] & 0xff,
				addr,
				canvas_width, frame_info->height,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		addr += canvas_width * frame_info->height;
		canvas_config((vp_canvas_table[fill_ptr] >> 8) & 0xff,
				addr,
				canvas_width, frame_info->height / 2,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		new_vf->canvas0Addr = vp_canvas_table[fill_ptr];
		break;
	default:
		vp_err("unsupported format to canvas_config\n");
		return -EINVAL;
	}
#ifdef CONFIG_AMLOGIC_TEE
	if (frame_info->secure) {
		/*protect size need 64k aligned*/
		protect_size = (((protect_size) + ((64 * 1024) - 1)) & ~((64 * 1024) - 1));
		prot_ret = tee_protect_mem_by_type(TEE_MEM_TYPE_STREAM_OUTPUT,
				protect_addr, protect_size, &secure_handle);
		if (prot_ret)
			vp_err("set memory secure error prot_ret = %x\n", prot_ret);
	}
#endif
	if (frame_info->fg_test) {
		new_vf->fgs_valid = true;
		new_vf->fgs_table_adr = fg_data_addr;
		protect_fg_addr = fg_data_addr;
		/* for the 2x2 mosaic mode, the lut_dma reads two fgrain tables each time.
		 * so the two fgrain tables need to be copied and combined together.
		 */
		if (video_mosaic_en) {
			void *src_ptr = NULL;
			char *dst_ptr = (char *)g_fg_vaddr;

			if (dst_ptr) {
				src_ptr = memremap(fg_data_addr,
						   FGRAIN_TBL_MOSAIC_SIZE, MEMREMAP_WB);
				if (src_ptr && dst_ptr) {
					memcpy(dst_ptr, src_ptr, FGRAIN_TBL_SIZE);
					memcpy(dst_ptr + FGRAIN_TBL_SIZE, src_ptr, FGRAIN_TBL_SIZE);
					memunmap(src_ptr);
					protect_fg_size = FGRAIN_TBL_MOSAIC_SIZE;
					new_vf->fgs_table_adr = g_fg_paddr;
					protect_fg_addr = g_fg_paddr;
				} else {
					vp_err("wrong param, 0x%llx %p %p\n",
					       (unsigned long long)fg_data_addr, src_ptr, dst_ptr);
				}
			}
		}
		/*protect size need 64k aligned*/
		protect_fg_size = (((protect_fg_size) + ((64 * 1024) - 1)) & ~((64 * 1024) - 1));
		vp_dbg("%s, secure addr:%lx size:%lx\n",
			__func__, protect_fg_addr, protect_fg_size);
#ifdef CONFIG_AMLOGIC_TEE
		if (frame_info->fg_secure) {
			prot_ret = tee_protect_mem_by_type(TEE_MEM_TYPE_STREAM_OUTPUT,
					protect_fg_addr, protect_fg_size, &secure_handle);
			if (prot_ret)
				vp_err("set memory secure error prot_ret = %x\n", prot_ret);
		}
#endif
	}
	new_vf->width  = frame_info->width;
	new_vf->height = frame_info->height;
	new_vf->index = fill;
	new_vf->duration_pulldown = 0;
	new_vf->pts = 0;
	new_vf->pts_us64 = 0;
	new_vf->ratio_control = 0;

	if (video_mosaic_en) {
		int i;

		new_vf->type_ext |= VIDTYPE_EXT_MOSAIC_22;
		memset(&vc_private, 0, sizeof(struct video_composer_private));
		new_vf->vc_private = &vc_private;
		for (i = 0; i < 4; i++) {
			memcpy(&mosaic_vf[i], new_vf, sizeof(struct vframe_s));

			if (i == 0)
				memcpy(&mosaic_vf[i].axis[0], axis_0, sizeof(int) * 4);
			else if (i == 1)
				memcpy(&mosaic_vf[i].axis[0], axis_1, sizeof(int) * 4);
			else if (i == 2)
				memcpy(&mosaic_vf[i].axis[0], axis_2, sizeof(int) * 4);
			else if (i == 3)
				memcpy(&mosaic_vf[i].axis[0], axis_3, sizeof(int) * 4);
			new_vf->vc_private->mosaic_vf[i] = &mosaic_vf[i];
			vp_info("%s %d (%d %d %d %d)\n", __func__, i,
				mosaic_vf[i].axis[0],
				mosaic_vf[i].axis[1],
				mosaic_vf[i].axis[2],
				mosaic_vf[i].axis[3]);
		}
	}

	return 0;
}

static int set_vfm_info_from_frame_lcevc(struct vp_lcevc_frame_s *lcevc_info)
{
	struct vframe_s *new_vf, *enhance_vf;
	int ret = -1;

	ret = set_vfm_info_from_frame(&lcevc_info->residual, 0);
	if (ret < 0)
		return ret;
	ret = set_vfm_info_from_frame(&lcevc_info->base, 1);
	if (ret < 0)
		return ret;

	new_vf = &vp_pool[fill_ptr].vfm;
	enhance_vf = &vp_pool_pip[fill_ptr_pip].vfm;

	new_vf->type_ext |= VIDTYPE_EXT_LCEVC;
	new_vf->enhance_vf = enhance_vf;
	new_vf->scaler_coeff.k[0][0] = -2900;
	new_vf->scaler_coeff.k[0][1] = 16384;
	new_vf->scaler_coeff.k[0][2] = 2900;
	new_vf->scaler_coeff.k[0][3] = 0;

	new_vf->scaler_coeff.k[1][0] = 0;
	new_vf->scaler_coeff.k[1][1] = 2900;
	new_vf->scaler_coeff.k[1][2] = 16384;
	new_vf->scaler_coeff.k[1][3] = -2900;

	return 0;
}

static void post_frame(void)
{
	INCPTR(fill_ptr);
	vf_notify_receiver(VIDEO_PROVIDER_NAME,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
}

static void post_frame_pip(void)
{
	INCPTR(fill_ptr_pip);
	vf_notify_receiver(VIDEO_PROVIDER_NAME_PIP,
				VFRAME_EVENT_PROVIDER_VFRAME_READY,
				NULL);
}

static long video_provider_ioctl(struct file *filp, unsigned int cmd,
						 unsigned long args)
{
	long ret = 0;
	void __user *argp = (void __user *)args;
	struct vp_frame_s frame_info;
	struct vp_frame_s *f1, *f2;
	struct vp_lcevc_frame_s lcevc_info;

	switch (cmd) {
	case VIDEO_PROVIDER_IOCTL_RENDER:
		if (!copy_from_user(&frame_info, argp, sizeof(frame_info))) {
			vp_dbg("    render main: canvas index 0x%x\n",
			       vp_canvas_table[fill_ptr]);
			vp_dbg("   frame format: %d\n", frame_info.format);
			vp_dbg("    frame width: %d\n", frame_info.width);
			vp_dbg("   frame height: %d\n", frame_info.height);
			vp_dbg(" frame mem_type: %d\n", frame_info.mem_type);
			vp_dbg("frame shared_fd: %d\n", frame_info.shared_fd);
			vp_dbg("   frame endian: %d\n", frame_info.endian);
			vp_dbg("         offset: 0x%x\n", frame_info.offset);
			vp_dbg("    afbc format: 0x%x\n", frame_info.afbc_format);
			vp_dbg("    afrc format: 0x%x\n", frame_info.afrc_format);
			vp_dbg("      bit_depth: 0x%x\n", frame_info.bit_depth);
			vp_dbg("      luma_only: 0x%x\n", frame_info.luma_only);
			vp_dbg("         secure: 0x%x\n", frame_info.secure);
			vp_dbg("        fg_test: 0x%x\n", frame_info.fg_test);
			vp_dbg("    fg_mem_type: 0x%x\n", frame_info.fg_mem_type);
			vp_dbg("      fg_secure: 0x%x\n", frame_info.fg_secure);
			ret = set_vfm_info_from_frame(&frame_info, 0);
		} else {
			ret = -EINVAL;
		}
		break;
	case VIDEO_PROVIDER_IOCTL_POST:
		vp_dbg("post: canvas index 0x%x\n", vp_canvas_table[get_ptr]);
		post_frame();
		break;
	case VIDEO_PROVIDER_IOCTL_RENDER_PIP:
		if (!copy_from_user(&frame_info, argp, sizeof(frame_info))) {
			vp_dbg("     render pip: canvas index 0x%x\n",
			       vp_canvas_table_pip[fill_ptr_pip]);
			vp_dbg("   frame format: %d\n", frame_info.format);
			vp_dbg("    frame width: %d\n", frame_info.width);
			vp_dbg("   frame height: %d\n", frame_info.height);
			vp_dbg(" frame mem_type: %d\n", frame_info.mem_type);
			vp_dbg("frame shared_fd: %d\n", frame_info.shared_fd);
			vp_dbg("   frame endian: %d\n", frame_info.endian);
			vp_dbg("         offset: 0x%x\n", frame_info.offset);
			vp_dbg("    afbc format: 0x%x\n", frame_info.afbc_format);
			vp_dbg("    afrc format: 0x%x\n", frame_info.afrc_format);
			vp_dbg("      bit_depth: 0x%x\n", frame_info.bit_depth);
			vp_dbg("      luma_only: 0x%x\n", frame_info.luma_only);
			vp_dbg("         secure: 0x%x\n", frame_info.secure);
			vp_dbg("        fg_test: 0x%x\n", frame_info.fg_test);
			vp_dbg("    fg_mem_type: 0x%x\n", frame_info.fg_mem_type);
			vp_dbg("      fg_secure: 0x%x\n", frame_info.fg_secure);
			ret = set_vfm_info_from_frame(&frame_info, 1);
		} else {
			ret = -EINVAL;
		}
		break;
	case VIDEO_PROVIDER_IOCTL_POST_PIP:
		vp_dbg("post pip: canvas index 0x%x\n", vp_canvas_table_pip[get_ptr_pip]);
		post_frame_pip();
		break;
	case VIDEO_PROVIDER_IOCTL_RENDER_LCEVC:
		if (!copy_from_user(&lcevc_info, argp, sizeof(lcevc_info))) {
			vp_dbg("   render lcevc: canvas index 0x%x 0x%x\n",
			       vp_canvas_table[fill_ptr], vp_canvas_table_pip[fill_ptr_pip]);
			f1 = &lcevc_info.residual;
			f2 = &lcevc_info.base;
			vp_dbg("   frame format: %d %d\n", f1->format, f2->format);
			vp_dbg("    frame width: %d %d\n", f1->width, f2->width);
			vp_dbg("   frame height: %d %d\n", f1->height, f2->height);
			vp_dbg(" frame mem_type: %d %d\n", f1->mem_type, f2->mem_type);
			vp_dbg("frame shared_fd: %d %d\n", f1->shared_fd, f2->shared_fd);
			vp_dbg("   frame endian: %d %d\n", f1->endian, f2->endian);
			vp_dbg("         offset: 0x%x 0x%x\n", f1->offset, f2->offset);
			vp_dbg("    afbc format: 0x%x 0x%x\n", f1->afbc_format, f2->afbc_format);
			vp_dbg("    afrc format: 0x%x 0x%x\n", f1->afrc_format, f2->afrc_format);
			vp_dbg("      bit_depth: 0x%x 0x%x\n", f1->bit_depth, f2->bit_depth);
			vp_dbg("      luma_only: 0x%x 0x%x\n", f1->luma_only, f2->luma_only);
			ret = set_vfm_info_from_frame_lcevc(&lcevc_info);
		} else {
			ret = -EINVAL;
		}
		break;
	default:
		vp_err("%s-%d, para err\n", __func__, __LINE__);
		ret = -EINVAL;
	}
	return ret;
}

static int memory_cma_alloc(int len)
{
	struct device *dev = &video_provider_device.pdev->dev;
	struct page *cma_pages = NULL;
	struct cma *cma_area = NULL;
	dma_addr_t cma_paddr = 0;

	if (cma_info.alloc_page || cma_info.alloc_len) {
		vp_err("%s, already allocated, len:%d\n",
		       __func__, cma_info.alloc_len);
		return -EINVAL;
	}
	if (len <= 0) {
		vp_err("failed to alloc, len:%d\n", len);
		return -EINVAL;
	}

	len = PAGE_ALIGN(len);
	/* change in kernel5.15 */
	if (dev && dev->cma_area)
		cma_area = dev->cma_area;
	else
		cma_area = dma_contiguous_default_area;

#if CONFIG_AMLOGIC_KERNEL_VERSION >= 14515
	cma_pages = cma_alloc(cma_area, len >> PAGE_SHIFT, 0, GFP_KERNEL);
#else
	cma_pages = cma_alloc(cma_area, len >> PAGE_SHIFT, 0, 0);
#endif
	if (!cma_pages) {
		vp_err("failed to alloc buff, len:%d\n", len);
		return -ENOMEM;
	}

	cma_info.alloc_page = cma_pages;
	cma_info.alloc_len = len;
	cma_paddr = page_to_phys(cma_info.alloc_page);
	vp_dbg("%s, paddr:%pad len:%d\n", __func__, &cma_paddr, len);
	dma_sync_single_for_device(dev, cma_paddr, len, DMA_TO_DEVICE);

	return 0;
}

static void memory_cma_release(void)
{
	struct device *dev = &video_provider_device.pdev->dev;
	struct page *cma_pages = cma_info.alloc_page;
	struct cma *cma_area = NULL;
	int len = cma_info.alloc_len;
	bool ret = false;

	if (!cma_pages || !len) {
		vp_err("%s failed, cma_pages:%p len:%d\n",
		       __func__, cma_pages, len);
		return;
	}

	if (dev && dev->cma_area)
		cma_area = dev->cma_area;
	else
		cma_area = dma_contiguous_default_area;
	ret = cma_release(cma_area, cma_pages, len >> PAGE_SHIFT);
	if (!ret) {
		vp_err("failed to release output buff\n");
		return;
	}

	cma_info.alloc_page = NULL;
	cma_info.alloc_len = 0;
}

static int video_provider_release(struct inode *inode, struct file *file)
{
	_video_set_disable(VIDEO_DISABLE_NORMAL);
	video_set_global_output(0, 0);
	_videopip_set_disable(1, VIDEO_DISABLE_NORMAL);
	video_set_global_output(1, 0);

	video_provider_release_path();
	video_provider_release_path_pip();
	canvas_table_release();
	canvas_table_release_pip();
	if (cma_info.alloc_page && cma_info.alloc_len)
		memory_cma_release();

	if (g_fg_vaddr)
		free_pages((unsigned long)g_fg_vaddr, ORDER);
	g_fg_vaddr = NULL;
	g_fg_paddr = 0;

	return 0;
}

#ifdef CONFIG_COMPAT
static long video_provider_compat_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long args)
{
	long ret = 0;

	ret = video_provider_ioctl(filp, cmd, (ulong)compat_ptr(args));
	return ret;
}
#endif

static int video_provider_mmap(struct file *file_p,
			  struct vm_area_struct *vma)
{
	int ret = -1;
	unsigned long buf_len = 0;
	dma_addr_t cma_paddr;

	buf_len = vma->vm_end - vma->vm_start;
	ret = memory_cma_alloc(buf_len);
	if (ret < 0)
		return ret;

	cma_paddr = page_to_phys(cma_info.alloc_page);

	ret = remap_pfn_range(vma, vma->vm_start,
			      cma_paddr >> PAGE_SHIFT,
			      buf_len, vma->vm_page_prot);
	if (ret != 0)
		vp_err("Failed to mmap buffer\n");

	return ret;
}

static const struct file_operations video_provider_fops = {
	.owner = THIS_MODULE,
	.open = video_provider_open,
	.unlocked_ioctl = video_provider_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = video_provider_compat_ioctl,
#endif
	.release = video_provider_release,
	.mmap = video_provider_mmap,
};

static int video_provider_probe(struct platform_device *pdev)
{
	int  ret = 0;

	strcpy(video_provider_device.name, VIDEO_PROVIDER_NAME);
	ret = register_chrdev(0, video_provider_device.name,
				&video_provider_fops);
	if (ret <= 0) {
		vp_err("register video provider device error\n");
		return  ret;
	}
	video_provider_device.major = ret;
	vp_info("video provider major:%d\n", ret);
	ret = class_register(&video_provider_class);
	if (ret < 0) {
		vp_err("error create video provider class\n");
		return ret;
	}
	video_provider_device.cla = &video_provider_class;
	video_provider_device.dev = device_create(video_provider_device.cla,
					NULL, MKDEV(video_provider_device.major,
					0), NULL, video_provider_device.name);
	if (IS_ERR(video_provider_device.dev)) {
		vp_err("create video provider device error\n");
		class_unregister(video_provider_device.cla);
		return -1;
	}
	video_provider_device.pdev = pdev;

	/* 8g memory support */
	pdev->dev.coherent_dma_mask = DMA_BIT_MASK(64);
	pdev->dev.dma_mask = &pdev->dev.coherent_dma_mask;

	ret = of_reserved_mem_device_init(&pdev->dev);
	if (ret != 0)
		vp_err("reserve_mem is not used\n");

	return ret;
}

static void video_provider_remove(struct platform_device *pdev)
{
	vp_info("%s\n", __func__);
	if (!video_provider_device.cla)
		return;

	if (video_provider_device.dev)
		device_destroy(video_provider_device.cla,
				MKDEV(video_provider_device.major, 0));
	class_unregister(video_provider_device.cla);
	unregister_chrdev(video_provider_device.major,
				video_provider_device.name);
}

static const struct of_device_id amlogic_video_provider_dt_match[] = {
	{
		.compatible = "amlogic, dummy_video_provider",
	},
	{},
};

static struct platform_driver video_provider_drv = {
	.probe = video_provider_probe,
	.remove = video_provider_remove,
	.driver = {
		.name = "video_provider",
		.owner = THIS_MODULE,
		.of_match_table = amlogic_video_provider_dt_match,
	}
};

//static int __init video_provider_init_module(void)
int __init video_provider_init_module(void)
{
	vp_info("%s\n", __func__);

	if (platform_driver_register(&video_provider_drv)) {
		pr_err("Failed to register video provider driver error\n");
		return -ENODEV;
	}

	return 0;
}

//static void __exit video_provider_remove_module(void)
void __exit video_provider_remove_module(void)
{
	platform_driver_unregister(&video_provider_drv);
	vp_info("video provider module removed.\n");
}

//module_init(video_provider_init_module);
//module_exit(video_provider_remove_module);

//MODULE_DESCRIPTION("Amlogic dummy video provider driver");
//MODULE_LICENSE("GPL");
