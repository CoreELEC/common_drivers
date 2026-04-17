// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/of.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/dma-buf.h>
#include <linux/pagemap.h>
#include <linux/amlogic/ion.h>
#include <linux/dma-heap.h>
#include <linux/dma-direction.h>
#include <linux/dma-direct.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <uapi/linux/dma-heap.h>

#include <linux/amlogic/media/avbc_wrapper_interface.h>
#include <linux/amlogic/media/dev_ion.h>
#include <linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h>
#include <linux/amlogic/media/registers/cpu_version.h>
#include <linux/amlogic/meson_uvm_allocator.h>
#include <linux/amlogic/meson_uvm_interface.h>

struct mua_device *mua_dev;
struct device *uvmdev;
static int enable_screencap;
module_param_named(enable_screencap, enable_screencap, int, 0664);
static int mua_debug_level = MUA_ERROR;
module_param(mua_debug_level, int, 0644);
static int mua_filldata_policy = MUA_ALL_WRAPPER;
module_param(mua_filldata_policy, int, 0644);
static int realloc_buf_count = 3;
module_param(realloc_buf_count, int, 0664);
static int enable_rec_buf_pool;
module_param_named(enable_rec_buf_pool, enable_rec_buf_pool, int, 0664);

#define MUA_PRINTK(level, fmt, arg...) \
	do {	\
		if (mua_debug_level & (level))     \
			pr_info("MUA: " fmt, ## arg); \
	} while (0)

#define UVM_DECODER_NODE_NUM 32
#define UVM_DECODER_PARA_NUM 7 // (sizeof(struct uvm_decoder_para) / sizeof(uint32_t))
static int uvm_decoder_para_num = UVM_DECODER_PARA_NUM * UVM_DECODER_NODE_NUM;
/*
 * The values in the array represent width and height respectively.
 * If the first value is 0, the second value represents size.
 * If both values are 0, it means that the node has been released.
 */
static int decoder_para[UVM_DECODER_PARA_NUM * UVM_DECODER_NODE_NUM] = { 0 };
module_param_array(decoder_para, uint, &uvm_decoder_para_num, 0644);

#define MAX_SIZE_8K (8192 * 4608)
#define MAX_SIZE_4K (4096 * 2304)
#define MAX_SIZE_2K (1920 * 1088)
#define IS_8K_SIZE(w, h)  (((w) * (h)) > MAX_SIZE_4K)
#define IS_4K_SIZE(w, h)  (((w) * (h)) > MAX_SIZE_2K)
#define MMU_COMPRESS_HEADER_SIZE_1080P  0x10000
#define MMU_COMPRESS_HEADER_SIZE_4K     0x48000
#define MMU_COMPRESS_HEADER_SIZE_8K     0x120000
static int mua_imm_alloc_count[MAX_PIPE_LINE] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static int insert_count[MAX_PIPE_LINE];
static int skip_realloc;
static bool video_exit;

static int get_header_size(int w, int h)
{
	w = ALIGN(w, 64);
	h = ALIGN(h, 64);

	if (IS_8K_SIZE(w, h))
		return MMU_COMPRESS_HEADER_SIZE_8K;

	if (IS_4K_SIZE(w, h))
		return MMU_COMPRESS_HEADER_SIZE_4K;

	return MMU_COMPRESS_HEADER_SIZE_1080P;
}

struct mua_device *meson_uvm_get_mua_dev(void)
{
	return mua_dev;
}
EXPORT_SYMBOL(meson_uvm_get_mua_dev);

static void mua_dummy_buffer_init(void)
{
	int i;

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		mua_dev->dummy_dmabuf[i] = NULL;
		mua_dev->dummy_dmabuf_w[i] = 0;
		mua_dev->dummy_dmabuf_h[i] = 0;
	}
}

static struct dma_buf *mua_dummy_buffer_get(u32 width, u32 height)
{
	int i;
	struct dma_buf *dmabuf = NULL;

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->dummy_dmabuf[i] &&
		    mua_dev->dummy_dmabuf_w[i] == width &&
		    mua_dev->dummy_dmabuf_h[i] == height) {
			dmabuf = mua_dev->dummy_dmabuf[i];
			kref_get(&mua_dev->dummy_dmabuf_ref[i]);
			break;
		}
	}
	return dmabuf;
}

static int mua_dummy_buffer_exit(struct dma_buf *dmabuf)
{
	int i, ret;

	ret = 0;
	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->dummy_dmabuf[i] &&
		    mua_dev->dummy_dmabuf[i] == dmabuf) {
			ret = 1;
			break;
		}
	}
	return ret;
}

static int mua_dummy_buffer_insert(struct dma_buf *dmabuf,
				   u32 width, u32 height)
{
	int i, ret;

	ret = 0;
	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (!mua_dev->dummy_dmabuf[i]) {
			mua_dev->dummy_dmabuf[i] = dmabuf;
			mua_dev->dummy_dmabuf_w[i] = width;
			mua_dev->dummy_dmabuf_h[i] = height;
			kref_init(&mua_dev->dummy_dmabuf_ref[i]);
			ret = 1;
			break;
		}
	}
	MUA_PRINTK(MUA_INFO, "dummy insert: dmabuf(%px). ret=%d\n", dmabuf, ret);
	return ret;
}

static void mua_dummy_buffer_del(struct kref *kref)
{
	int i;

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->dummy_dmabuf[i] &&
		    &mua_dev->dummy_dmabuf_ref[i] == kref) {
			dma_buf_put(mua_dev->dummy_dmabuf[i]);
			mua_dev->dummy_dmabuf[i] = NULL;
			mua_dev->dummy_dmabuf_w[i] = 0;
			mua_dev->dummy_dmabuf_h[i] = 0;
			break;
		}
	}
	MUA_PRINTK(MUA_INFO, "dummy del: index=%d\n", i);
}

static void mua_dummy_buffer_put(struct dma_buf *dmabuf)
{
	int i;

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->dummy_dmabuf[i] &&
		    mua_dev->dummy_dmabuf[i] == dmabuf) {
			kref_put(&mua_dev->dummy_dmabuf_ref[i],
				 mua_dummy_buffer_del);
			break;
		}
	}
	if (i < MAX_PIPE_LINE)
		MUA_PRINTK(MUA_INFO, "dummy put: dummy_dmabuf_ref[%d]=%u\n",
			i, kref_read(&mua_dev->dummy_dmabuf_ref[i]));
	else
		MUA_PRINTK(MUA_INFO, "dummy put: no match dmabuf\n");
}

static int mua_realloc_buffer_insert(struct dma_buf *idmabuf,
			u32 width, u32 height, u32 slot_id)
{
	int i;
	struct mua_realloc_buffer_list *new_node;

	new_node = kzalloc(sizeof(*new_node), GFP_KERNEL);
	if (!new_node)
		return -ENOMEM;

	new_node->dmabuf = idmabuf;
	new_node->fake_dmabuf = NULL;
	atomic_add(1, &new_node->dmabuf_ref);
	new_node->dmabuf_w = width;
	new_node->dmabuf_h = height;
	new_node->flag = false;
	new_node->timestamp = ktime_get();
	new_node->slot_id = slot_id;
	MUA_PRINTK(MUA_INFO, "insert buf: idmabuf(%px) WxH(%ux%u) slot_id(%u)\n",
		idmabuf, width, height, slot_id);

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->mua_rec_buf_list[i].slot_id == slot_id)
			break;
	}
	if (i == MAX_PIPE_LINE) {
		kfree(new_node);
		return -ENOMEM;
	}

	mutex_lock(&mua_dev->mua_rec_buf_lock[i]);
	list_add_tail(&new_node->dmabuf_list, &mua_dev->mua_rec_buf_list[i].dmabuf_list);
	mutex_unlock(&mua_dev->mua_rec_buf_lock[i]);
	insert_count[i]++;

	return 0;
}

static struct dma_buf *mua_realloc_buffer_get(struct dma_buf *idmabuf,
			u32 width, u32 height, u32 slot_id)
{
	int i;
	struct dma_buf *dmabuf = NULL;
	struct mua_realloc_buffer_list *entry, *tmp, *oldest;
	ktime_t oldest_ts = KTIME_MAX;

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->mua_rec_buf_list[i].slot_id == slot_id)
			break;
	}
	if (i == MAX_PIPE_LINE)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&mua_dev->mua_rec_buf_lock[i]);
	list_for_each_entry_safe(entry, tmp,
			&mua_dev->mua_rec_buf_list[i].dmabuf_list, dmabuf_list) {
		if (!entry->flag && ktime_compare(entry->timestamp, oldest_ts) < 0) {
			oldest_ts = entry->timestamp;
			oldest = entry;
		}
	}

	list_for_each_entry_safe(entry, tmp,
			&mua_dev->mua_rec_buf_list[i].dmabuf_list, dmabuf_list) {
		if (entry->flag && entry->dmabuf == idmabuf &&
		    entry->dmabuf_w == width && entry->dmabuf_h == height) {
			dmabuf = entry->dmabuf;
			skip_realloc = 1;
			MUA_PRINTK(MUA_INFO, "get buf: skip_realloc(%d)\n", skip_realloc);
			break;
		}
		if (entry == oldest && entry->dmabuf_w == width && entry->dmabuf_h == height) {
			entry->flag = true;
			dmabuf = entry->dmabuf;
			MUA_PRINTK(MUA_INFO, "get buf: dmabuf(%px) idmabuf(%px) flag(%d)\n",
				dmabuf, idmabuf, entry->flag);
			break;
		}
	}
	mutex_unlock(&mua_dev->mua_rec_buf_lock[i]);

	return dmabuf;
}

static void mua_realloc_buffer_bind(struct dma_buf *idmabuf,
			struct dma_buf *fake_dmabuf, u32 width, u32 height, u32 slot_id)
{
	int i;
	struct uvm_buf_obj *obj = NULL;
	struct mua_buffer *buffer = NULL;
	struct mua_realloc_buffer_list *entry, *tmp;

	obj = dmabuf_get_uvm_buf_obj(fake_dmabuf);
	if (IS_ERR_OR_NULL(obj))
		return;
	buffer = container_of(obj, struct mua_buffer, base);
	if (!buffer)
		return;

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->mua_rec_buf_list[i].slot_id == slot_id)
			break;
	}
	if (i == MAX_PIPE_LINE)
		return;

	mutex_lock(&mua_dev->mua_rec_buf_lock[i]);
	list_for_each_entry_safe(entry, tmp,
			&mua_dev->mua_rec_buf_list[i].dmabuf_list, dmabuf_list) {
		if (entry->dmabuf == buffer->idmabuf[1]) {
			atomic_sub(1, &entry->dmabuf_ref);
			MUA_PRINTK(MUA_INFO, "pre_idmabuf(%px) fake_dmabuf(%px) dmabuf_ref(%u)\n",
				entry->dmabuf, fake_dmabuf, atomic_read(&entry->dmabuf_ref));
			break;
		}
	}

	list_for_each_entry_safe(entry, tmp,
			&mua_dev->mua_rec_buf_list[i].dmabuf_list, dmabuf_list) {
		if (entry->dmabuf == idmabuf &&
		    entry->dmabuf_w == width && entry->dmabuf_h == height) {
			entry->fake_dmabuf = fake_dmabuf;
			atomic_add(1, &entry->dmabuf_ref);
			MUA_PRINTK(MUA_INFO, "bind idmabuf(%px) fake_dmabuf(%px) dmabuf_ref(%u)\n",
				idmabuf, fake_dmabuf, atomic_read(&entry->dmabuf_ref));
			break;
		}
	}
	mutex_unlock(&mua_dev->mua_rec_buf_lock[i]);
}

static void mua_realloc_buffer_put(struct dma_buf *idmabuf, u32 slot_id)
{
	int i;
	struct mua_realloc_buffer_list *entry, *tmp;

	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->mua_rec_buf_list[i].slot_id == slot_id)
			break;
	}
	if (i == MAX_PIPE_LINE)
		return;

	mua_imm_alloc_count[i]--;
	mutex_lock(&mua_dev->mua_rec_buf_lock[i]);
	list_for_each_entry_safe(entry, tmp,
			&mua_dev->mua_rec_buf_list[i].dmabuf_list, dmabuf_list) {
		if (entry->dmabuf == idmabuf) {
			atomic_sub(1, &entry->dmabuf_ref);
			if (atomic_read(&entry->dmabuf_ref) == 1) {
				MUA_PRINTK(MUA_INFO, "put dmabuf(%px) count: imm(%d) insert(%d)\n",
					entry->dmabuf, mua_imm_alloc_count[i], insert_count[i]);
				dma_buf_put(entry->dmabuf);
				list_del(&entry->dmabuf_list);
				kfree(entry);
				insert_count[i]--;
				video_exit = true;
			}
			break;
		}
	}

	if (insert_count[i] != 0 && mua_imm_alloc_count[i] == 0) {
		list_for_each_entry_safe(entry, tmp,
				&mua_dev->mua_rec_buf_list[i].dmabuf_list, dmabuf_list) {
			MUA_PRINTK(MUA_INFO, "put dmabuf(%px)\n", entry->dmabuf);
			dma_buf_put(entry->dmabuf);
			list_del(&entry->dmabuf_list);
			kfree(entry);
			insert_count[i]--;
		}
	}
	if (list_empty(&mua_dev->mua_rec_buf_list[i].dmabuf_list)) {
		mua_dev->mua_rec_buf_list[i].slot_id = -1;
		mua_imm_alloc_count[i] = 1;
		insert_count[i] = 0;
	}
	mutex_unlock(&mua_dev->mua_rec_buf_lock[i]);
}

static bool mua_is_valid_dmabuf(int fd)
{
	struct dma_buf *dmabuf = NULL;

	dmabuf = dma_buf_get(fd);
	if (IS_ERR_OR_NULL(dmabuf))
		return false;

	if (!dmabuf_is_uvm(dmabuf)) {
		dma_buf_put(dmabuf);
		return false;
	}

	dma_buf_put(dmabuf);
	return true;
}

static struct vframe_s *mua_dmabuf_get_vframe(struct dma_buf *dmabuf)
{
	struct vframe_s *vf = NULL;
	bool is_dec_vf = false;

	is_dec_vf = is_valid_mod_type(dmabuf, VF_SRC_DECODER);
	if (is_dec_vf) {
		vf = dmabuf_get_vframe(dmabuf);
		if (vf)
			MUA_PRINTK(MUA_INFO, "vf_type=0x%x, vf_flag=0x%x\n", vf->type, vf->flag);
		dmabuf_put_vframe(dmabuf);
	}

	return vf;
}

static void mua_handle_free(struct uvm_buf_obj *obj)
{
	struct mua_buffer *buffer;
	struct dma_buf *idmabuf[2];
	int buf_cnt = 0;

	buffer = container_of(obj, struct mua_buffer, base);
	if (!buffer)
		return;

	idmabuf[0] = buffer->idmabuf[0];
	idmabuf[1] = buffer->idmabuf[1];
	MUA_PRINTK(MUA_INFO, "mua handle free idmabuf[0]=%px idmabuf[1]=%px\n",
		idmabuf[0], idmabuf[1]);

	if (enable_rec_buf_pool) {
		while (buf_cnt < 2 && idmabuf[0]) {
			MUA_PRINTK(MUA_INFO, "free idmabuf[%d]\n", buf_cnt);
			if (buf_cnt == 1)
				mua_realloc_buffer_put(idmabuf[buf_cnt], buffer->slot_id);
			else
				dma_buf_put(idmabuf[buf_cnt]);
			buf_cnt++;
		}
	} else {
		while (buf_cnt < 2 && idmabuf[buf_cnt]) {
			MUA_PRINTK(MUA_INFO, "free idmabuf[%d]\n", buf_cnt);
			if (buf_cnt == 1 && mua_dummy_buffer_exit(idmabuf[buf_cnt]))
				mua_dummy_buffer_put(idmabuf[buf_cnt]);
			else
				dma_buf_put(idmabuf[buf_cnt]);
			buf_cnt++;
		}
	}

	kfree(buffer);
}

size_t mua_calc_real_dmabuf_size(unsigned int align, unsigned int byte_stride,
			   unsigned int height)
{
	size_t size = 0;
	int align_height = ALIGN(height, align);

	size = (size_t)byte_stride * (size_t)align_height * 3 / 2;
	MUA_PRINTK(MUA_INFO,
		"calc size: align=%d byte_stride=%d height=%d align_height=%d size:%zu\n",
		align, byte_stride, height, align_height, size);
	return size;
}

static int ge2d_uvm_fill_pattern(struct mua_buffer *buffer, struct dma_buf *dmabuf)
{
	u32 src_w_stride, src_h_stride, dst_w_stride, dst_h_stride;
	struct canvas_config_s src_canvas_config[2], dst_canvas_config[2];
	struct uvm_ge2d_info ge2d_info = { 0 };
	struct vframe_s *vf = NULL;
	int ret = -1;
	size_t buf_size = 0;

	vf = mua_dmabuf_get_vframe(dmabuf);
	if (!vf)
		return ret;

	src_w_stride = ALIGN(vf->width, 64);
	src_h_stride = ALIGN(vf->height, 64);

	dst_w_stride = buffer->byte_stride;
	dst_h_stride = ALIGN(buffer->height, buffer->align);

	src_canvas_config[0].phy_addr = buffer->paddr;
	src_canvas_config[0].width = src_w_stride;
	src_canvas_config[0].height = src_h_stride;
	src_canvas_config[0].block_mode = 0;
	src_canvas_config[0].endian = 0;
	src_canvas_config[0].bit_depth = 0;

	src_canvas_config[1].phy_addr = buffer->paddr + src_w_stride * src_h_stride;
	src_canvas_config[1].width = src_w_stride;
	src_canvas_config[1].height = src_h_stride;
	src_canvas_config[1].block_mode = 0;
	src_canvas_config[1].endian = 0;
	src_canvas_config[1].bit_depth = 0;

	buf_size = mua_calc_real_dmabuf_size(buffer->align,
			buffer->byte_stride, buffer->height) * 2 / 3;
	MUA_PRINTK(MUA_INFO, "ge2d fill buf_size=%zu\n", buf_size);

	memset(phys_to_virt(buffer->realloc_paddr), 0x15, buf_size);
	memset(phys_to_virt(buffer->realloc_paddr) + buf_size, 0x80, buf_size / 2);
	dma_sync_single_for_device(uvmdev, phys_to_dma(uvmdev, buffer->realloc_paddr),
				   buf_size * 3 / 2, DMA_BIDIRECTIONAL);

	dst_canvas_config[0] = src_canvas_config[0];
	dst_canvas_config[0].width = dst_w_stride;
	dst_canvas_config[0].height = dst_h_stride;
	dst_canvas_config[0].phy_addr = buffer->realloc_paddr;

	dst_canvas_config[1] = src_canvas_config[1];
	dst_canvas_config[1].width = dst_w_stride;
	dst_canvas_config[1].height = dst_h_stride;
	dst_canvas_config[1].phy_addr = buffer->realloc_paddr + dst_w_stride * dst_h_stride;

	ge2d_info.src_canvasAddr = -1;
	ge2d_info.src_yuv_width = vf->width;
	ge2d_info.src_yuv_height = vf->height;
	ge2d_info.dst_vf = vf;
	ge2d_info.dst_yuv_width = vf->compWidth - vf->src_crop.right;
	ge2d_info.dst_yuv_height = vf->compHeight - vf->src_crop.bottom;
	ge2d_info.src_canvas_config[0] = src_canvas_config[0];
	ge2d_info.src_canvas_config[1] = src_canvas_config[1];
	ge2d_info.dst_canvas_config[0] = dst_canvas_config[0];
	ge2d_info.dst_canvas_config[1] = dst_canvas_config[1];

	MUA_PRINTK(MUA_INFO,
		"ge2d copy: compWxcompH(%ux%u), top:%d, left:%d, bottom:%d, right:%d, size:%zu\n",
		vf->compWidth, vf->compHeight,
		vf->src_crop.top, vf->src_crop.left,
		vf->src_crop.bottom, vf->src_crop.right, buffer->idmabuf[1]->size);
	MUA_PRINTK(MUA_INFO,
		"src_addr(0x%lx, 0x%lx), dst_addr(0x%lx, 0x%lx), src_stride(%ux%u), dst_stride(%ux%u)\n",
		src_canvas_config[0].phy_addr, src_canvas_config[1].phy_addr,
		dst_canvas_config[0].phy_addr, dst_canvas_config[1].phy_addr,
		src_w_stride, src_h_stride, dst_w_stride, dst_h_stride);
	if (!mua_dev->ge2d) {
		int mode = (vf->type == AML_PIX_FMT_NV21) ?
				UVM_GE2D_MODE_CONVERT_NV21 : UVM_GE2D_MODE_CONVERT_NV12;
		mode |= UVM_GE2D_MODE_CONVERT_LE;
		ret = AMLOGIC_UVM_ge2d_init(&mua_dev->ge2d, mode);
		if (ret < 0) {
			MUA_PRINTK(MUA_ERROR, "uvm_ge2d_init fail\n");
			return ret;
		}
	}
	ret = AMLOGIC_UVM_ge2d_copy_data(mua_dev->ge2d, &ge2d_info);
	if (ret < 0)
		MUA_PRINTK(MUA_ERROR, "aml_ge2d_copy() fail\n");

	if (mua_dev->ge2d) {
		AMLOGIC_UVM_ge2d_destroy(mua_dev->ge2d);
		mua_dev->ge2d = NULL;
	}

	return ret;
}

static int avbcd_uvm_fill_pattern(struct mua_buffer *buffer, struct dma_buf *dmabuf)
{
	struct vframe_s *vf = NULL;
	struct avbc_input *in;
	struct avbc_output *out;
	int ret = -1;
	size_t buf_size = 0;

	vf = mua_dmabuf_get_vframe(dmabuf);
	if (!vf)
		return ret;

	if (!(vf->type & VIDTYPE_COMPRESS)) {
		MUA_PRINTK(MUA_ERROR, "go to GE2D copy for non-compress\n");
		return ret;
	}

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;
	out = kzalloc(sizeof(*out), GFP_KERNEL);
	if (!out) {
		kfree(in);
		return -ENOMEM;
	}

	if ((vf->bitdepth & BITDEPTH_YMASK) == BITDEPTH_Y10)
		in->img.bitdep = 10;
	else
		in->img.bitdep = 8;
	in->img.data = vf->compHeadAddr;
	in->img.size = get_header_size(vf->compWidth, vf->compHeight);
	in->img.rect.x = 0;
	in->img.rect.y = 0;
	in->img.rect.width = vf->compWidth;
	in->img.rect.height = vf->compHeight;
	if (is_src_crop_valid(vf->src_crop)) {
		in->img.crop.top = vf->src_crop.top;
		in->img.crop.left = vf->src_crop.left;
		in->img.crop.bottom = vf->src_crop.bottom;
		in->img.crop.right = vf->src_crop.right;
	}
	MUA_PRINTK(MUA_INFO,
		"wrapper copy: addr=0x%llx size=%u width=%u height=%u bitdepth=%u\n",
		in->img.data, in->img.size,
		in->img.rect.width, in->img.rect.height, in->img.bitdep);
	MUA_PRINTK(MUA_INFO, "crop info: top=%u left=%u bottom=%u right=%u\n",
		in->img.crop.top, in->img.crop.left, in->img.crop.bottom, in->img.crop.right);

	buf_size = mua_calc_real_dmabuf_size(buffer->align,
			buffer->byte_stride, buffer->height) * 2 / 3;
	MUA_PRINTK(MUA_INFO, "wrapper fill buf_size=%zu\n", buf_size);

	memset(phys_to_virt(buffer->realloc_paddr), 0x15, buf_size);
	memset(phys_to_virt(buffer->realloc_paddr) + buf_size, 0x80, buf_size / 2);
	dma_sync_single_for_device(uvmdev, phys_to_dma(uvmdev, buffer->realloc_paddr),
				   buf_size * 3 / 2, DMA_BIDIRECTIONAL);

	out->img.bitdep = in->img.bitdep;
	if (buffer->byte_stride == buffer->width && in->img.bitdep == 10) {
		MUA_PRINTK(MUA_DBG, "memory not enough, convert 10bit to 8bit\n");
		out->img.bitdep = 8;
	}
	out->img.format = (out->img.bitdep == 10) ? AML_PIX_FMT_P010 :
				((vf->type & VIDTYPE_VIU_NV12) ?
					AML_PIX_FMT_NV12 : AML_PIX_FMT_NV21);
	out->img.mtype = AVBC_MEM_PHYADDR;
	out->img.data = buffer->realloc_paddr;
	out->img.size = buffer->idmabuf[1]->size;
	out->img.rect.x = 0;
	out->img.rect.y = 0;
	if (out->img.bitdep == 10)
		out->img.rect.width = buffer->byte_stride / 2;
	else
		out->img.rect.width = buffer->byte_stride;
	out->img.rect.height = ALIGN(buffer->height, buffer->align);
	MUA_PRINTK(MUA_INFO, "addr=0x%llx size=%u width=%u height=%u bitdepth=%u\n",
		out->img.data, out->img.size,
		out->img.rect.width, out->img.rect.height, out->img.bitdep);

	ret = AMLOGIC_AVBC_WRAPPER_vframe_decoder(out, in, AVBC_FLAG_IO_BLOCKING);
	if (ret < 0)
		MUA_PRINTK(MUA_ERROR, "aml_avbc_decode() fail\n");

	kfree(in);
	kfree(out);
	return ret;
}

void meson_uvm_fill_pattern(struct mua_buffer *buffer, struct dma_buf *dmabuf, void *vaddr)
{
	struct v4l_data_t val_data;
	size_t buf_size = 0;

	buf_size = mua_calc_real_dmabuf_size(buffer->align,
			buffer->byte_stride, buffer->height) * 2 / 3;

	MUA_PRINTK(MUA_INFO, "soft decode buf_size=%zu\n", buf_size);
	memset(vaddr, 0x15, buf_size);
	memset(vaddr + buf_size, 0x80, buf_size / 2);

	memset(&val_data, 0, sizeof(val_data));
	val_data.dst_addr = vaddr;
	val_data.byte_stride = buffer->byte_stride;
	val_data.width = buffer->width;
	val_data.height = ALIGN(buffer->height, buffer->align);
	val_data.size = buffer->idmabuf[1]->size;
	val_data.phy_addr[0] = buffer->realloc_paddr;
	MUA_PRINTK(MUA_INFO,
		"v4l copy: width=%d height=%d byte_stride=%d align=%d size=%zu\n",
		buffer->width, buffer->height,
		buffer->byte_stride, buffer->align, buffer->idmabuf[1]->size);

	AMLOGIC_V4LVIDEO_data_copy(&val_data, dmabuf, buffer->align);
}

static void mua_screencap_filldata(struct mua_buffer *buffer, struct dma_buf *dmabuf,
			void *vaddr, bool skip_fill_buf)
{
	size_t buf_size = 0;
	int ret = 0;

	switch (mua_filldata_policy) {
	case MUA_ALL_WRAPPER:
		MUA_PRINTK(MUA_INFO, "all avbcd wrapper fill\n");
		ret = avbcd_uvm_fill_pattern(buffer, dmabuf);
		break;
	case MUA_WRAPPER_GE2D:
		if (get_cpu_type() == MESON_CPU_MAJOR_ID_S5 ||
		    get_cpu_type() == MESON_CPU_MAJOR_ID_S6 ||
		    get_cpu_type() == MESON_CPU_MAJOR_ID_T3X) {
			MUA_PRINTK(MUA_INFO, "vicp fill\n");
			ret = avbcd_uvm_fill_pattern(buffer, dmabuf);
		} else {
			if (skip_fill_buf) {
				struct vframe_s *vf = mua_dmabuf_get_vframe(dmabuf);

				if (vf && vf->compWidth >= 3840 && vf->compHeight >= 2160) {
					MUA_PRINTK(MUA_INFO, "avbcd fill\n");
					ret = avbcd_uvm_fill_pattern(buffer, dmabuf);
				} else {
					MUA_PRINTK(MUA_INFO, "ge2d fill\n");
					ret = ge2d_uvm_fill_pattern(buffer, dmabuf);
				}
			} else {
				if (avbcd_uvm_fill_pattern(buffer, dmabuf)) {
					MUA_PRINTK(MUA_INFO, "afbc soft decode fill\n");
					meson_uvm_fill_pattern(buffer, dmabuf, vaddr);
				}
			}
		}
		break;
	case MUA_SRC_DEFAULT:
		if (skip_fill_buf) {
			buf_size = mua_calc_real_dmabuf_size(buffer->align,
					buffer->byte_stride, buffer->height) * 2 / 3;

			MUA_PRINTK(MUA_INFO, "default fill dummy data buf_size=%zu\n",
				buf_size);
			memset(vaddr, 0x15, buf_size);
			memset(vaddr + buf_size, 0x80, buf_size / 2);
		} else {
			if (avbcd_uvm_fill_pattern(buffer, dmabuf)) {
				MUA_PRINTK(MUA_INFO, "default afbc soft decode fill\n");
				meson_uvm_fill_pattern(buffer, dmabuf, vaddr);
			}
		}
		break;
	case MUA_FORCE_FILL_DUMMY:
		buf_size = mua_calc_real_dmabuf_size(buffer->align,
				buffer->byte_stride, buffer->height) * 2 / 3;

		MUA_PRINTK(MUA_INFO, "force fill dummy data buf_size=%zu\n",
			buf_size);
		memset(vaddr, 0x15, buf_size);
		memset(vaddr + buf_size, 0x80, buf_size / 2);
		break;
	case MUA_FORCE_AFBC_SOFT_DECODE:
		MUA_PRINTK(MUA_INFO, "force afbc soft decode fill\n");
		meson_uvm_fill_pattern(buffer, dmabuf, vaddr);
		break;
	case MUA_FORCE_GE2D_COPY:
		MUA_PRINTK(MUA_INFO, "force ge2d copy fill\n");
		ret = ge2d_uvm_fill_pattern(buffer, dmabuf);
		break;
	default:
		MUA_PRINTK(MUA_ERROR, "mua_filldata_policy is error\n");
	}

	if (ret < 0)
		MUA_PRINTK(MUA_ERROR, "screencap filldata fail\n");
	return;
}

static void realloc_buffer_pool_alloc_worker(struct work_struct *work)
{
	struct realloc_buffer_work_data *data = NULL;
	struct dma_buf *realloc_idmabuf = NULL;
	struct dma_heap *realloc_heap = NULL;
	int ret = 0;

	data = container_of(work, struct realloc_buffer_work_data, work);
	if (!data)
		return;
	realloc_heap = dma_heap_find(CODECMM_HEAP_NAME);
	if (!realloc_heap)
		return;

	realloc_idmabuf = dma_heap_buffer_alloc(realloc_heap,
		data->size, O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR_OR_NULL(realloc_idmabuf))
		return;
	MUA_PRINTK(MUA_INFO, "realloc_idmabuf(%px) alloc success\n", realloc_idmabuf);

	ret = mua_realloc_buffer_insert(realloc_idmabuf,
		data->width, data->height, data->slot_id);
	if (ret)
		MUA_PRINTK(MUA_ERROR, "insert rec_buf fail\n");
	return;
}

static int submit_realloc_buffer_pool_task(size_t size, u32 width, u32 height, u32 slot_id)
{
	struct realloc_buffer_work_data *work_data;

	work_data = kzalloc(sizeof(*work_data), GFP_KERNEL);
	if (!work_data)
		return -ENOMEM;

	work_data->size = size;
	work_data->width = width;
	work_data->height = height;
	work_data->slot_id = slot_id;

	INIT_WORK(&work_data->work, realloc_buffer_pool_alloc_worker);

	if (!queue_work(mua_dev->workq, &work_data->work)) {
		MUA_PRINTK(MUA_ERROR, "queue_work fail\n");
		kfree(work_data);
		return -EINVAL;
	}
	return 0;
}

static int mua_process_gpu_realloc(struct dma_buf *dmabuf,
				   struct uvm_buf_obj *obj, int scalar)
{
	int i, j, num_pages;
	struct dma_buf *idmabuf = NULL;
	struct uvm_alloc_info info;
	struct mua_buffer *buffer;
	struct page *page;
	struct page **tmp;
	struct page **page_array;
	pgprot_t pgprot;
	void *vaddr;
	bool skip_fill_buf = false;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;
	size_t pre_size = 0;
	size_t new_size = 0;
	struct dma_heap *heap;
	struct dma_buf_attachment *attachment = NULL;
	struct timeval start, end;
	unsigned long time_use = 0;

	do_gettimeofday(&start);
	buffer = container_of(obj, struct mua_buffer, base);
	if (!buffer)
		return -EINVAL;
	MUA_PRINTK(MUA_INFO, "mua realloc: dmabuf(%px) buf_scalar=%d WxH(%dx%d) slot_id=%d\n",
		dmabuf, scalar, buffer->width, buffer->height, buffer->slot_id);

	memset(&info, 0, sizeof(info));
	MUA_PRINTK(MUA_INFO, "current->tgid=%d mua_dev->pid=%d buffer->commit_display=%d\n",
		current->tgid, mua_dev->pid, buffer->commit_display);

	if (!enable_screencap && current->tgid == mua_dev->pid &&
	    buffer->commit_display) {
		MUA_PRINTK(MUA_DBG, "gpu_realloc: screen cap should not access the uvm buffer\n");
		skip_fill_buf = true;
	}

	heap = dma_heap_find(CODECMM_HEAP_NAME);
	if (!heap)
		return -ENOMEM;

	if (!enable_rec_buf_pool) {
		if (buffer->idmabuf[1])
			pre_size = buffer->idmabuf[1]->size;
		else if (buffer->idmabuf[0])
			pre_size = buffer->idmabuf[0]->size;
		else
			pre_size = dmabuf->size;
	}

	if (buffer->ion_flags & MUA_SIZE_SKIP)
		new_size = buffer->origin_size;
	else
		new_size = mua_calc_real_dmabuf_size(buffer->align,
				buffer->byte_stride, buffer->height);
	/* dmabuf->size will do page align when alloc by ion or dmaheap,
	 * so we need align it before check with pre_size
	 */
	new_size = PAGE_ALIGN(new_size);
	MUA_PRINTK(MUA_INFO, "buffer(%px)->size=%zu realloc new_size=%zu\n",
		buffer, buffer->size, new_size);

	if (enable_rec_buf_pool) {
		int n, ret;

		for (n = 0; n < MAX_PIPE_LINE; n++) {
			if (mua_dev->mua_rec_buf_list[n].slot_id == buffer->slot_id)
				break;
		}

		if (n < MAX_PIPE_LINE) {
			if (insert_count[n] <= realloc_buf_count) {
				ret = submit_realloc_buffer_pool_task(new_size,
					 buffer->width, buffer->height, buffer->slot_id);
				if (ret) {
					MUA_PRINTK(MUA_ERROR, "work queue submit fail\n");
					return -ENOMEM;
				}
			}

			idmabuf = mua_realloc_buffer_get(buffer->idmabuf[1],
					buffer->width, buffer->height, buffer->slot_id);
			MUA_PRINTK(MUA_INFO, "idmabuf(%px) don't do realloc\n", idmabuf);
			if (skip_realloc) {
				skip_realloc = 0;
				return 0;
			}

			if (!idmabuf && insert_count[n] <= realloc_buf_count) {
				flush_workqueue(mua_dev->workq);
				idmabuf = mua_realloc_buffer_get(buffer->idmabuf[1],
						buffer->width, buffer->height, buffer->slot_id);
				MUA_PRINTK(MUA_INFO, "workqueue idmabuf(%px) don't do realloc\n",
					idmabuf);
				if (skip_realloc) {
					skip_realloc = 0;
					return 0;
				}
			} else if (!idmabuf && insert_count[n] > realloc_buf_count &&
			    insert_count[n] <= mua_imm_alloc_count[n]) {
				MUA_PRINTK(MUA_INFO, "idmabuf is NULL/all used\n");
				idmabuf = dma_heap_buffer_alloc(heap, new_size,
					O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
				if (IS_ERR_OR_NULL(idmabuf))
					return -ENOMEM;
				MUA_PRINTK(MUA_INFO, "idmabuf(%px) alloc success\n", idmabuf);

				ret = mua_realloc_buffer_insert(idmabuf,
						buffer->width, buffer->height, buffer->slot_id);
				if (ret) {
					MUA_PRINTK(MUA_ERROR, "insert rec_buf fail\n");
					return -ENOMEM;
				}
			} else if (!idmabuf && insert_count[n] > mua_imm_alloc_count[n]) {
				MUA_PRINTK(MUA_ERROR, "realloc buffer reached up limit\n");
				return -ENOMEM;
			}

			mua_realloc_buffer_bind(idmabuf, dmabuf,
					buffer->width, buffer->height, buffer->slot_id);

			attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
			if (!attachment)
				return -ENOMEM;

			src_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
			if (!src_sgt) {
				dma_buf_detach(idmabuf, attachment);
				return -ENOMEM;
			}

			MUA_PRINTK(MUA_INFO, "src_sgt(%px). nents=%u, length=%u\n",
				src_sgt, src_sgt->nents, src_sgt->sgl->length);
			page = sg_page(src_sgt->sgl);
			buffer->realloc_paddr = PFN_PHYS(page_to_pfn(page));
			buffer->idmabuf[1] = idmabuf;
			buffer->sg_table = src_sgt;

			info.sgt = src_sgt;
			dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
		} else {
			MUA_PRINTK(MUA_ERROR, "idmabuf is NULL\n");
			idmabuf = dma_heap_buffer_alloc(heap, new_size,
				O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
			if (IS_ERR_OR_NULL(idmabuf))
				return -ENOMEM;
			MUA_PRINTK(MUA_INFO, "idmabuf(%px) alloc success\n", idmabuf);

			attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
			if (!attachment)
				return -ENOMEM;

			src_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
			if (!src_sgt) {
				dma_buf_detach(idmabuf, attachment);
				return -ENOMEM;
			}

			MUA_PRINTK(MUA_INFO, "src_sgt(%px). nents=%u, length=%u\n",
				src_sgt, src_sgt->nents, src_sgt->sgl->length);
			page = sg_page(src_sgt->sgl);
			buffer->realloc_paddr = PFN_PHYS(page_to_pfn(page));
			buffer->idmabuf[1] = idmabuf;
			buffer->sg_table = src_sgt;

			info.sgt = src_sgt;
			dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
		}
	} else {
		if (new_size < pre_size)
			new_size = pre_size;

		if (pre_size != new_size && buffer->idmabuf[1]) {
			dma_buf_put(buffer->idmabuf[1]);
			buffer->idmabuf[1] = NULL;
		}

		if (skip_fill_buf && !buffer->idmabuf[1]) {
			idmabuf = mua_dummy_buffer_get(buffer->width, buffer->height);
			buffer->idmabuf[1] = idmabuf;
		}

		if (!buffer->idmabuf[1]) {
			idmabuf = dma_heap_buffer_alloc(heap, new_size,
				O_RDWR, DMA_HEAP_VALID_HEAP_FLAGS);
			if (IS_ERR_OR_NULL(idmabuf))
				return -ENOMEM;
			MUA_PRINTK(MUA_INFO, "idmabuf(%px) alloc success\n", idmabuf);

			attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
			if (!attachment)
				return -ENOMEM;

			src_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
			if (!src_sgt) {
				dma_buf_detach(idmabuf, attachment);
				return -ENOMEM;
			}

			MUA_PRINTK(MUA_INFO, "src_sgt(%px). nents=%u, length=%u\n",
				src_sgt, src_sgt->nents, src_sgt->sgl->length);
			page = sg_page(src_sgt->sgl);
			buffer->realloc_paddr = PFN_PHYS(page_to_pfn(page));
			buffer->idmabuf[1] = idmabuf;
			buffer->sg_table = src_sgt;

			info.sgt = src_sgt;
			dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
			if (skip_fill_buf)
				mua_dummy_buffer_insert(idmabuf, buffer->width,
							buffer->height);
		} else {
			idmabuf = buffer->idmabuf[1];
			MUA_PRINTK(MUA_INFO, "idmabuf(%px) don't do realloc\n", idmabuf);
			attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
			if (!attachment)
				return -ENOMEM;

			src_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
			if (!src_sgt) {
				dma_buf_detach(idmabuf, attachment);
				return -ENOMEM;
			}

			MUA_PRINTK(MUA_INFO, "src_sgt(%px). nents=%u, length=%u\n",
				src_sgt, src_sgt->nents, src_sgt->sgl->length);
			page = sg_page(src_sgt->sgl);
			buffer->realloc_paddr = PFN_PHYS(page_to_pfn(page));
			buffer->sg_table = src_sgt;

			info.sgt = src_sgt;
			dmabuf_bind_uvm_delay_alloc(dmabuf, &info);
		}
	}

	//start to do vmap
	if (!buffer->sg_table)
		return -ENODEV;

	src_sgt = buffer->sg_table;
	num_pages = PAGE_ALIGN(new_size) / PAGE_SIZE;
	tmp = vmalloc(sizeof(struct page *) * num_pages);
	if (!tmp) {
		if (src_sgt && attachment) {
			dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(idmabuf, attachment);
		}
		return -ENOMEM;
	}
	page_array = tmp;
	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(src_sgt->sgl, sg, src_sgt->nents, i) {
		int npages_this_entry =
			PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(page_array, num_pages, VM_MAP, pgprot);
	if (!vaddr) {
		vfree(page_array);
		if (src_sgt && attachment) {
			dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(idmabuf, attachment);
		}
		return -ENOMEM;
	}
	vfree(page_array);

	//start to filldata
	mua_screencap_filldata(buffer, dmabuf, vaddr, skip_fill_buf);

	vunmap(vaddr);
	if (src_sgt && attachment) {
		dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(idmabuf, attachment);
	}
	do_gettimeofday(&end);
	time_use = (end.tv_sec - start.tv_sec) * 1000 +
				(end.tv_usec - start.tv_usec) / 1000;
	MUA_PRINTK(MUA_INFO, "uvm realloc use time: %ldms\n", time_use);

	return 0;
}

static int mua_process_delay_alloc(struct dma_buf *dmabuf,
				   struct uvm_buf_obj *obj)
{
	int i, j, num_pages;
	struct dma_buf *idmabuf;
	struct uvm_alloc_info info;
	struct mua_buffer *buffer;
	struct page *page;
	struct page **tmp;
	struct page **page_array;
	pgprot_t pgprot;
	void *vaddr;
	struct sg_table *src_sgt = NULL;
	struct scatterlist *sg = NULL;
	char *name = CODECMM_HEAP_NAME;
	struct dma_heap *heap;
	struct dma_buf_attachment *attachment = NULL;

	buffer = container_of(obj, struct mua_buffer, base);
	if (!buffer)
		return -EINVAL;

	memset(&info, 0, sizeof(info));

	if (!enable_screencap && current->tgid == mua_dev->pid &&
	    buffer->commit_display) {
		MUA_PRINTK(MUA_ERROR, "delay_alloc: skip delay alloc\n");
		return -ENODEV;
	}

	if (buffer->ion_flags & MUA_USAGE_PROTECTED)
		name = CODECMM_SECURE_HEAP_NAME;
	else if (buffer->ion_flags & ION_FLAG_CACHED)
		name = CODECMM_CACHED_HEAP_NAME;

	heap = dma_heap_find(name);
	if (!heap)
		return -ENOMEM;

	idmabuf = dma_heap_buffer_alloc(heap, dmabuf->size, O_RDWR,
		DMA_HEAP_VALID_HEAP_FLAGS);
	if (IS_ERR_OR_NULL(idmabuf))
		return -ENOMEM;

	attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
	if (!attachment)
		return -ENOMEM;

	src_sgt = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
	if (!src_sgt) {
		dma_buf_detach(idmabuf, attachment);
		return -ENOMEM;
	}

	page = sg_page(src_sgt->sgl);
	buffer->realloc_paddr = PFN_PHYS(page_to_pfn(page));
	buffer->sg_table = src_sgt;
	buffer->idmabuf[0] = idmabuf;
	info.sgt = src_sgt;
	dmabuf_bind_uvm_delay_alloc(dmabuf, &info);

	//start to do vmap
	if (!buffer->sg_table)
		return -ENODEV;

	src_sgt = buffer->sg_table;
	num_pages = PAGE_ALIGN(buffer->size) / PAGE_SIZE;
	tmp = vmalloc(sizeof(struct page *) * num_pages);
	if (!tmp) {
		if (src_sgt && attachment) {
			dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(idmabuf, attachment);
		}
		return -ENOMEM;
	}
	page_array = tmp;
	pgprot = pgprot_writecombine(PAGE_KERNEL);

	for_each_sg(src_sgt->sgl, sg, src_sgt->nents, i) {
		int npages_this_entry =
			PAGE_ALIGN(sg->length) / PAGE_SIZE;
		struct page *page = sg_page(sg);

		for (j = 0; j < npages_this_entry; j++)
			*(tmp++) = page++;
	}

	vaddr = vmap(page_array, num_pages, VM_MAP, pgprot);
	if (!vaddr) {
		vfree(page_array);
		if (src_sgt && attachment) {
			dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
			dma_buf_detach(idmabuf, attachment);
		}
		return -ENOMEM;
	}
	vfree(page_array);

	//start to filldata
	meson_uvm_fill_pattern(buffer, dmabuf, vaddr);

	vunmap(vaddr);
	if (src_sgt && attachment) {
		dma_buf_unmap_attachment(attachment, src_sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(idmabuf, attachment);
	}

	return 0;
}

static int mua_handle_alloc(struct dma_buf *dmabuf, struct uvm_alloc_data *data, int alloc_buf_size)
{
	int i;
	bool same_slot_id = false;
	struct mua_buffer *buffer;
	struct uvm_alloc_info info;
	struct dma_buf *idmabuf;
	char *name = CODECMM_HEAP_NAME;
	struct dma_heap *heap;
	struct dma_buf_attachment *attachment = NULL;
	struct sg_table *sg = NULL;
	struct page *page;

	memset(&info, 0, sizeof(info));
	buffer = kzalloc(sizeof(*buffer), GFP_KERNEL);
	if (!buffer)
		return -ENOMEM;
	buffer->size = alloc_buf_size;
	buffer->dev = mua_dev;
	buffer->byte_stride = data->byte_stride;
	buffer->width = data->width;
	buffer->height = data->height;
	buffer->ion_flags = data->flags;
	buffer->align = data->align;
	buffer->slot_id = data->slot_id;
	for (i = 0; i < MAX_PIPE_LINE; i++) {
		if (mua_dev->mua_rec_buf_list[i].slot_id == buffer->slot_id) {
			same_slot_id = true;
			mua_imm_alloc_count[i]++;
			break;
		}
	}
	if (!same_slot_id) {
		for (i = 0; i < MAX_PIPE_LINE; i++) {
			if (mua_dev->mua_rec_buf_list[i].slot_id == -1) {
				mua_dev->mua_rec_buf_list[i].slot_id = buffer->slot_id;
				break;
			}
		}
	}
	if (same_slot_id && video_exit) {
		same_slot_id = false;
		video_exit = false;
	}

	if (data->flags & MUA_SIZE_SKIP)
		buffer->origin_size = data->size;

	if (data->flags & MUA_IMM_ALLOC) {
		if (data->flags & MUA_USAGE_PROTECTED)
			name = CODECMM_SECURE_HEAP_NAME;
		else if (data->flags & MUA_BUFFER_CACHED)
			name = CODECMM_CACHED_HEAP_NAME;

		heap = dma_heap_find(name);
		if (!heap)
			goto free_buffer;

		idmabuf = dma_heap_buffer_alloc(heap, alloc_buf_size, O_RDWR,
			DMA_HEAP_VALID_HEAP_FLAGS);
		if (IS_ERR_OR_NULL(idmabuf))
			goto free_buffer;
		MUA_PRINTK(MUA_INFO, "idmabuf(%px) alloc success. heap name: %s, flags=%u\n",
			idmabuf, name, data->flags);

		if (enable_rec_buf_pool) {
			int ret;

			if (!same_slot_id) {
				ret = submit_realloc_buffer_pool_task(data->size,
					 buffer->width, buffer->height, buffer->slot_id);
				if (ret) {
					MUA_PRINTK(MUA_ERROR, "work queue submit fail\n");
					goto free_buffer;
				}
			}
		}

		buffer->idmabuf[0] = idmabuf;
		buffer->idmabuf[1] = NULL;

#if IS_ENABLED(CONFIG_AMLOGIC_HEAP_CODEC_MM)
		if (strcmp(CODECMM_HEAP_NAME, name))
			((struct codec_mm_heap_buffer *)(idmabuf->priv))->uvm_dma_buf = dmabuf;
#endif

		attachment = dma_buf_attach(idmabuf, dma_heap_get_dev(heap));
		if (!attachment)
			goto free_buffer;

		sg = dma_buf_map_attachment(attachment, DMA_BIDIRECTIONAL);
		if (!sg) {
			dma_buf_detach(idmabuf, attachment);
			goto free_buffer;
		}

		page = sg_page(sg->sgl);
		buffer->paddr = PFN_PHYS(page_to_pfn(page));

		info.sgt = sg;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.size = alloc_buf_size;
		info.scalar = data->scalar;
		info.gpu_realloc = mua_process_gpu_realloc;
		info.free = mua_handle_free;
		MUA_PRINTK(MUA_INFO, "UVM FLAGS is MUA_IMM_ALLOC, obj(%px) sgt(%px) dmabuf(%px)\n",
			info.obj, info.sgt, idmabuf);
	} else if (data->flags & MUA_DELAY_ALLOC) {
		info.size = data->size;
		info.obj = &buffer->base;
		info.flags = data->flags;
		info.delay_alloc = mua_process_delay_alloc;
		info.free = mua_handle_free;
		MUA_PRINTK(MUA_INFO, "UVM FLAGS is MUA_DELAY_ALLOC, obj(%px)\n", info.obj);
	} else {
		MUA_PRINTK(MUA_ERROR, "unsupported MUA FLAGS\n");
		goto free_buffer;
	}

	dmabuf_bind_uvm_alloc(dmabuf, &info);
	if (info.sgt) {
		dma_buf_unmap_attachment(attachment, info.sgt, DMA_BIDIRECTIONAL);
		dma_buf_detach(idmabuf, attachment);
	}
	return 0;

free_buffer:
	kfree(buffer);
	return -ENOMEM;
}

static int mua_set_commit_display(struct dma_buf *dmabuf, int commit_display)
{
	struct uvm_buf_obj *obj;
	struct mua_buffer *buffer;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	obj = dmabuf_get_uvm_buf_obj(dmabuf);
	if (IS_ERR_OR_NULL(obj))
		return -EINVAL;

	buffer = container_of(obj, struct mua_buffer, base);
	if (!buffer)
		return -EINVAL;
	buffer->commit_display = commit_display;

	return 0;
}

static int mua_set_mesondisplay_data(struct dma_buf *dmabuf, int tunnel_id)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;
	if (IS_ERR_OR_NULL(handle))
		return -EINVAL;

	handle->commit_tunnel_id = tunnel_id;
	MUA_PRINTK(MUA_INFO, "set mesondisplay tunnel_id = %d\n", handle->commit_tunnel_id);
	return 0;
}

static int mua_get_mesondisplay_data(struct dma_buf *dmabuf, int *tunnel_id)
{
	struct uvm_handle *handle;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	handle = dmabuf->priv;
	if (IS_ERR_OR_NULL(handle))
		return -EINVAL;

	*tunnel_id = handle->commit_tunnel_id;
	MUA_PRINTK(MUA_INFO, "get mesondisplay tunnel_id = %d\n", *tunnel_id);
	return 0;
}

static int mua_get_meta_data(struct dma_buf *dmabuf, int fd, ulong arg)
{
	struct vframe_s *vf;
	struct uvm_meta_data meta;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	vf = dmabuf_get_vframe(dmabuf);
	if (IS_ERR_OR_NULL(vf))
		goto put_vframe;

	/* check source type. */
	if (!vf->meta_data_size ||
		vf->meta_data_size > META_DATA_SIZE) {
		MUA_PRINTK(MUA_DBG, "meta data size: %d is invalid\n", vf->meta_data_size);
		goto put_vframe;
	}

	meta.fd = fd;
	meta.type = vf->type;
	meta.size = vf->meta_data_size;
	if (!vf->meta_data_buf) {
		MUA_PRINTK(MUA_ERROR, "vf->meta_data_buf is NULL\n");
		goto put_vframe;
	}
	memcpy(meta.data, vf->meta_data_buf, vf->meta_data_size);
	dmabuf_put_vframe(dmabuf);

	if (copy_to_user((void __user *)arg, &meta, sizeof(meta)))
		return -EFAULT;

	return 0;

put_vframe:
	dmabuf_put_vframe(dmabuf);
	return -EINVAL;
}

static int mua_attach(struct dma_buf *dmabuf, int fd, int type, char *buf)
{
	int ret = 0;

	ret = AMLOGIC_ATTACH_uvm_info(dmabuf, fd, type, buf);
	if (ret) {
		MUA_PRINTK(MUA_ERROR, "attach hook_mod_info failed\n");
		return -EINVAL;
	}
	MUA_PRINTK(MUA_INFO, "mua attach: type=%d dmabuf(%px)\n", type, dmabuf);

	return ret;
}

static int mua_detach(struct dma_buf *dmabuf, int type)
{
	int ret = 0;
	int index = 0;

	if (IS_ERR_OR_NULL(dmabuf) || !dmabuf_is_uvm(dmabuf))
		return -EINVAL;

	MUA_PRINTK(MUA_INFO, "mua detach: dmabuf(%px)\n", dmabuf);

	for (index = VF_SRC_DECODER; index < PROCESS_INVALID; index++) {
		if (type & BIT(index)) {
			index |= BIT(PROCESS_HWC);
			ret &= uvm_detach_hook_mod(dmabuf, index);
		}
	}

	return ret;
}

static int mua_set_decoder_para(struct uvm_decoder_para *para)
{
	int ret = 0;
	struct uvm_decoder_para *node;

	if (para->slot_id >= UVM_DECODER_NODE_NUM) {
		MUA_PRINTK(MUA_ERROR, "slot_id=%u is invalid\n", para->slot_id);
		return -EINVAL;
	}

	node = (struct uvm_decoder_para *)decoder_para + para->slot_id;
	*node = *para;

	MUA_PRINTK(MUA_INFO,
		"mua set para: slot_id(%u) w*h(%u*%u) align_w*h(%u*%u) size(%u) drt_map(%u)\n",
		para->slot_id, para->width, para->height,
		para->w_align, para->h_align, para->size, para->direct_map);
	return ret;
}

static int mua_get_decoder_para(struct uvm_decoder_para *para)
{
	int ret = 0;
	struct uvm_decoder_para *node;

	if (para->slot_id >= UVM_DECODER_NODE_NUM) {
		MUA_PRINTK(MUA_ERROR, "slot_id=%u is invalid\n", para->slot_id);
		return -EINVAL;
	}

	node = (struct uvm_decoder_para *)decoder_para + para->slot_id;
	*para = *node;

	MUA_PRINTK(MUA_INFO,
		"mua get para: slot_id(%u) w*h(%u*%u) align_w*h(%u*%u) size(%u) drt_map(%u)\n",
		para->slot_id, para->width, para->height,
		para->w_align, para->h_align, para->size, para->direct_map);
	return ret;
}

static long mua_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mua_device *md;
	union uvm_ioctl_arg data;
	struct dma_buf *dmabuf = NULL;
	int pid;
	int ret = 0;
	int fd = 0;
	size_t usage = 0;
	int alloc_buf_size = 0;
	unsigned int align_size = 0;
	unsigned int sync_size = 0;

	md = file->private_data;

	if (_IOC_SIZE(cmd) > sizeof(data))
		return -EINVAL;

	if (copy_from_user(&data, (void __user *)arg, _IOC_SIZE(cmd)))
		return -EFAULT;

	switch (cmd) {
	case UVM_IOC_ATTACH:
		fd = data.hook_data.shared_fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		MUA_PRINTK(MUA_INFO, "mua_ioctl_attach: shared_fd=%d, mode_type=%d\n",
			fd, data.hook_data.mode_type);
		dmabuf = dma_buf_get(fd);
		ret = mua_attach(dmabuf, fd,
				 data.hook_data.mode_type,
				 data.hook_data.data_buf);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "mua_attach fail\n");
			return -EINVAL;
		}

		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_DETACH:
		fd = data.hook_data.shared_fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		MUA_PRINTK(MUA_INFO, "mua_ioctl_detach: shared_fd=%d, mode_type=%d\n",
			fd, data.hook_data.mode_type);
		dmabuf = dma_buf_get(fd);
		ret = mua_detach(dmabuf, data.hook_data.mode_type);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "mua_detach fail\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_GET_INFO:
		fd = data.hook_data.shared_fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		dmabuf = dma_buf_get(fd);
		ret = meson_uvm_getinfo(dmabuf,
					data.hook_data.mode_type,
					data.hook_data.data_buf);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_getinfo fail\n");
			return -EINVAL;
		}

		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_SET_INFO:
		fd = data.hook_data.shared_fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		dmabuf = dma_buf_get(fd);
		ret = meson_uvm_setinfo(dmabuf,
					data.hook_data.mode_type,
					data.hook_data.data_buf);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_setinfo fail\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_ALLOC:
		MUA_PRINTK(MUA_INFO, "mua_ioctl_alloc: original buf size:%d width:%d height:%d\n",
			data.alloc_data.size, data.alloc_data.width, data.alloc_data.height);

		MUA_PRINTK(MUA_INFO, "scaled_buf_size:%d align=%d flags=%d\n",
			data.alloc_data.scaled_buf_size,
			data.alloc_data.align, data.alloc_data.flags);

		data.alloc_data.size = PAGE_ALIGN(data.alloc_data.size);
		data.alloc_data.scaled_buf_size =
				PAGE_ALIGN(data.alloc_data.scaled_buf_size);
		if (data.alloc_data.scalar > 1 ||
		    (data.alloc_data.flags & MUA_SIZE_SKIP))
			alloc_buf_size = data.alloc_data.scaled_buf_size;
		else
			alloc_buf_size = data.alloc_data.size;
		MUA_PRINTK(MUA_INFO, "buf_scalar=%d scaled_buf_size=%d\n",
			data.alloc_data.scalar, data.alloc_data.scaled_buf_size);

		if (data.alloc_data.flags & MUA_SIZE_SKIP) {
			align_size = data.alloc_data.size;
		} else {
			/* fake size shouldn't be used */
			align_size = mua_calc_real_dmabuf_size(data.alloc_data.align,
					data.alloc_data.byte_stride,
					data.alloc_data.height);
			/* dmabuf->size will do page align when alloc by ion or dmaheap,
			 * so we need align it before check with pre_size
			 */
			align_size = PAGE_ALIGN(align_size);
		}

		sync_size = alloc_buf_size;

		if (!((data.alloc_data.flags & BIT(UVM_SKIP_REALLOC)) ||
		    (data.alloc_data.flags & BIT(UVM_SECURE_ALLOC))) &&
		    align_size > alloc_buf_size) {
			sync_size = align_size;
		}
		MUA_PRINTK(MUA_INFO, "align_size=%d sync_size=%d alloc_buf_size=%d\n",
			align_size, sync_size, alloc_buf_size);

		dmabuf = uvm_alloc_dmabuf(sync_size,
					  data.alloc_data.align,
					  data.alloc_data.flags);
		if (IS_ERR_OR_NULL(dmabuf)) {
			MUA_PRINTK(MUA_ERROR, "uvm_alloc_dmabuf fail\n");
			return -ENOMEM;
		}

		ret = mua_handle_alloc(dmabuf, &data.alloc_data, alloc_buf_size);
		if (ret < 0) {
			MUA_PRINTK(MUA_ERROR, "mua_handle_alloc fail\n");
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}

		fd = dma_buf_fd(dmabuf, O_CLOEXEC);
		if (fd < 0) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}

		data.alloc_data.fd = fd;
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_SET_PID:
		pid = data.pid_data.pid;
		if (pid < 0)
			return -ENOMEM;

		md->pid = pid;
		break;
	case UVM_IOC_SET_FD:
		fd = data.fd_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		dmabuf = dma_buf_get(fd);
		ret = mua_set_commit_display(dmabuf, data.fd_data.data);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "mua_set_commit_display fail\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_SET_USAGE:
		fd = data.usage_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		dmabuf = dma_buf_get(fd);
		usage = data.usage_data.uvm_data_usage;
		ret = meson_uvm_set_usage(dmabuf, usage);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_set_usage fail\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_GET_USAGE:
		fd = data.usage_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		dmabuf = dma_buf_get(fd);
		ret = meson_uvm_get_usage(dmabuf, &usage);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "meson_uvm_get_usage fail\n");
			return -EINVAL;
		}

		data.usage_data.uvm_data_usage = usage;
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_GET_METADATA:
		fd = data.meta_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		dmabuf = dma_buf_get(fd);
		ret = mua_get_meta_data(dmabuf, fd, arg);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "get meta data fail\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_GET_VIDEO_INFO:
		fd = data.fd_info.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;

		ret = AMLOGIC_GET_UVM_video_info(fd, &data.fd_info);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "get video info fail\n");
			return ret;
		}
		MUA_PRINTK(MUA_DBG,
			"get video info fd:%d, type:%x, timestamp:%lld, buffer size:%ux%u, priority:%u\n",
			fd, data.fd_info.type, data.fd_info.timestamp,
			data.fd_info.buf_width, data.fd_info.buf_height, data.fd_info.priority);

		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_SET_DECODER_PARA:
		ret = mua_set_decoder_para(&data.decode_para);
		break;
	case UVM_IOC_GET_DECODER_PARA:
		ret = mua_get_decoder_para(&data.decode_para);
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	case UVM_IOC_SET_MESONDISPLAY_DATA:
		fd = data.mesondisplay_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;
		dmabuf = dma_buf_get(fd);
		ret = mua_set_mesondisplay_data(dmabuf, data.mesondisplay_data.tunnel_id);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "mesondisplay data set fail\n");
			return -EINVAL;
		}
		break;
	case UVM_IOC_GET_MESONDISPLAY_DATA:
		fd = data.mesondisplay_data.fd;
		if (!mua_is_valid_dmabuf(fd))
			return -EINVAL;
		dmabuf = dma_buf_get(fd);
		ret = mua_get_mesondisplay_data(dmabuf, &data.mesondisplay_data.tunnel_id);
		dma_buf_put(dmabuf);
		if (ret < 0) {
			MUA_PRINTK(MUA_INFO, "mesondisplay data get fail\n");
			return -EINVAL;
		}
		if (copy_to_user((void __user *)arg, &data, _IOC_SIZE(cmd)))
			return -EFAULT;
		break;
	default:
		return -ENOTTY;
	}

	return ret;
}

static int mua_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct mua_device *md = container_of(miscdev, struct mua_device, dev);

	file->private_data = md;

	return 0;
}

static int mua_release(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations mua_fops = {
	.owner = THIS_MODULE,
	.open = mua_open,
	.release = mua_release,
	.unlocked_ioctl = mua_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = mua_ioctl,
#endif
};

static int mua_probe(struct platform_device *pdev)
{
	int i, ret;
	u32 enable_scp, enable_recbuf_pool;

	ret = of_property_read_u32(pdev->dev.of_node, "enable_screencap", &enable_scp);
	if (ret)
		enable_scp = 0;
	enable_screencap = enable_scp;
	ret = of_property_read_u32(pdev->dev.of_node, "enable_rec_buf_pool", &enable_recbuf_pool);
	if (ret)
		enable_recbuf_pool = 0;
	enable_rec_buf_pool = enable_recbuf_pool;
	uvmdev = &pdev->dev;

	mua_dev = kzalloc(sizeof(*mua_dev), GFP_KERNEL);
	if (!mua_dev)
		return -ENOMEM;

	mua_dev->dev.minor = MISC_DYNAMIC_MINOR;
	mua_dev->dev.name = "uvm";
	mua_dev->dev.fops = &mua_fops;
	mutex_init(&mua_dev->buffer_lock);
	mua_dummy_buffer_init();
	mua_dev->workq = alloc_ordered_workqueue("realloc_buffer_workq", 0);
	if (!mua_dev->workq) {
		MUA_PRINTK(MUA_ERROR, "fail to init rec_buf workqueue\n");
		kfree(mua_dev);
		return -ENOMEM;
	}
	for (i = 0; i < MAX_PIPE_LINE; i++) {
		mua_dev->mua_rec_buf_list[i].dmabuf = NULL;
		mua_dev->mua_rec_buf_list[i].fake_dmabuf = NULL;
		mua_dev->mua_rec_buf_list[i].dmabuf_w = 0;
		mua_dev->mua_rec_buf_list[i].dmabuf_h = 0;
		mua_dev->mua_rec_buf_list[i].slot_id = -1;
		INIT_LIST_HEAD(&mua_dev->mua_rec_buf_list[i].dmabuf_list);
		mutex_init(&mua_dev->mua_rec_buf_lock[i]);
	}

	return misc_register(&mua_dev->dev);
}

static void mua_remove(struct platform_device *pdev)
{
	misc_deregister(&mua_dev->dev);
	if (mua_dev->workq) {
		flush_workqueue(mua_dev->workq);
		destroy_workqueue(mua_dev->workq);
		mua_dev->workq = NULL;
	}
}

static const struct of_device_id mua_match[] = {
	{.compatible = "amlogic, meson_uvm",},
	{},
};

static struct platform_driver mua_driver = {
	.driver = {
		.name = "meson_uvm_allocator",
		.owner = THIS_MODULE,
		.of_match_table = mua_match,
	},
	.probe = mua_probe,
	.remove = mua_remove,
};

static int __init mua_init(void)
{
	return platform_driver_register(&mua_driver);
}

static void __exit mua_exit(void)
{
	platform_driver_unregister(&mua_driver);
}

module_init(mua_init);
module_exit(mua_exit);
MODULE_IMPORT_NS(DMA_BUF);
MODULE_LICENSE("GPL v2");
