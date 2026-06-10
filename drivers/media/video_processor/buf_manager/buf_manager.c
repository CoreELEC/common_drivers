// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/file.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/sched/clock.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/vmalloc.h>
#include <linux/sync_file.h>
#include <linux/amlogic/media/dmabuf_heaps/amlogic_dmabuf_heap.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/meson_uvm_core.h>
#include <linux/amlogic/media/video_processor/di_proc_buf_mgr.h>
#include <linux/amlogic/media/video_sink/v4lvideo_ext.h>
#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/amlogic/cpu_version.h>
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
#include <linux/amlogic/media/video_processor/dpss_process_ext.h>
#endif
#ifdef CONFIG_AMLOGIC_DI_PROCESS
#include <linux/amlogic/media/video_processor/di_process_ext.h>
#endif
#define WAIT_DISP_Q_TIMEOUT 100

static u32 list_q_wait = 15;
static u32 print_flag;
static u32 di_proc_enable;

#define PRINT_ERROR		0X0
#define PRINT_OTHER		0X0001
#define PRINT_MORE		0X0080

static DEFINE_MUTEX(buf_mgr_mutex);

static int buf_mgr_print(struct dp_buf_mgr_t *mgr, int debug_flag, const char *fmt, ...)
{
	if ((print_flag & debug_flag) ||
		debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "buf_mgr:[%d:%d]", mgr->dec_type, mgr->instance_id);
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

int get_di_proc_enable(void)
{
	return di_proc_enable;
}
EXPORT_SYMBOL(get_di_proc_enable);

int set_di_proc_enable(int enable)
{
	mutex_lock(&buf_mgr_mutex);
	di_proc_enable = enable;
	mutex_unlock(&buf_mgr_mutex);

	return 0;
}
EXPORT_SYMBOL(set_di_proc_enable);

int get_buf_mgr_print_flag(void)
{
	return print_flag;
}
EXPORT_SYMBOL(get_buf_mgr_print_flag);

int set_buf_mgr_print_flag(int flag)
{
	mutex_lock(&buf_mgr_mutex);
	print_flag = flag;
	mutex_unlock(&buf_mgr_mutex);

	return 0;
}
EXPORT_SYMBOL(set_buf_mgr_print_flag);

int get_di_backend_need_mem(int width, int height, int source_type)
{
	int ret = 0;
	int cpu_type = get_cpu_type();

	if (cpu_type == MESON_CPU_MAJOR_ID_T6W || cpu_type == MESON_CPU_MAJOR_ID_T6X)
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
		ret = get_dpss_process_need_mem(width, height, source_type);
#else
		ret = 0;
#endif
	else
#ifdef CONFIG_AMLOGIC_DI_PROCESS
		ret = get_di_process_need_mem(width, height, source_type);
#else
		ret = 0;
#endif

	return ret;
}
EXPORT_SYMBOL(get_di_backend_need_mem);

static struct file_private_data *dp_get_v4lvideo_private(struct dp_buf_mgr_t *buf_mgr,
							  struct file *file_vf)
{
	struct file_private_data *file_private_data = NULL;
	struct uvm_hook_mod *uhmod;

	if (!file_vf || !buf_mgr) {
		pr_info("%s: NULL param.\n", __func__);
		return NULL;
	}

	if (is_v4lvideo_buf_file(file_vf)) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: v4lvideo buf.\n", __func__);
		file_private_data = (struct file_private_data *)(file_vf->private_data);
	} else {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: dma buf.\n", __func__);
		uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
			VF_PROCESS_V4LVIDEO);
		if (uhmod) {
			file_private_data = uhmod->arg;
			uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
				VF_PROCESS_V4LVIDEO);
		}
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER,
			"%s: private_data=%px\n",
			__func__,
			file_private_data);

	return file_private_data;
}

static struct dma_buf *get_uvmdma_from_yuvfile(struct dp_buf_mgr_t *buf_mgr,
						struct file *yuvfile)
{
	struct dma_buf *uvm_dma = NULL;
	struct dma_buf *yuv_dma = NULL;
	struct codec_mm_heap_buffer *buffer = NULL;

	if (!buf_mgr || !yuvfile) {
		pr_err("%s: NULL param.\n", __func__);
		return NULL;
	}

	yuv_dma = (struct dma_buf *)yuvfile->private_data;
	if (yuv_dma) {
		buffer = (struct codec_mm_heap_buffer *)yuv_dma->priv;
		if (buffer)
			uvm_dma = (struct dma_buf *)buffer->priv;
		else
			buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: buffer is NULL.\n", __func__);
	} else {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: yuv_dma is NULL.\n", __func__);
	}

	return uvm_dma;
}

static struct file *get_uvmfile_from_yuvfile(struct dp_buf_mgr_t *buf_mgr,
						struct file *yuvfile)
{
	struct dma_buf *uvm_dma = NULL;
	struct file *uvm_file = NULL;

	if (!buf_mgr || !yuvfile) {
		pr_err("%s: NULL param.\n", __func__);
		return NULL;
	}

	uvm_dma = get_uvmdma_from_yuvfile(buf_mgr, yuvfile);
	if (uvm_dma)
		uvm_file = (struct file *)uvm_dma->file;
	else
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: uvm_dma is NULL.\n", __func__);

	return uvm_file;
}

static struct vframe_s *get_vf_from_file(struct dp_buf_mgr_t *buf_mgr,
					 struct file *file_vf)
{
	struct vframe_s *vf = NULL;
	bool is_dec_vf = false;
	bool is_v4l_vf = false;
	struct file_private_data *file_private_data = NULL;

	if (IS_ERR_OR_NULL(buf_mgr) || IS_ERR_OR_NULL(file_vf)) {
		pr_info("%s: NULL param.\n", __func__);
		return NULL;
	}

	is_dec_vf = is_valid_mod_type(file_vf->private_data, VF_SRC_DECODER);
	is_v4l_vf = is_valid_mod_type(file_vf->private_data, VF_PROCESS_V4LVIDEO);

	if (is_dec_vf) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: decoder vf.\n", __func__);
		vf = dmabuf_get_vframe((struct dma_buf *)(file_vf->private_data));
		dmabuf_put_vframe((struct dma_buf *)(file_vf->private_data));
	} else if (is_v4l_vf) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: v4lvideo vf.\n", __func__);
		file_private_data = dp_get_v4lvideo_private(buf_mgr, file_vf);
		if (!file_private_data)
			buf_mgr_print(buf_mgr, PRINT_ERROR, "invalid fd: uvm is v4lvideo!!\n");
		else
			vf = &file_private_data->vf;
	} else {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "unknown vf type!!\n");
	}

	if (vf)
		buf_mgr_print(buf_mgr, PRINT_OTHER,
			"vframe_type = 0x%x, vframe_flag = 0x%x.\n", vf->type, vf->flag);
	else
		buf_mgr_print(buf_mgr, PRINT_OTHER, "vf is NULL.\n");

	return vf;
}

static void print_all_in_ref_list(struct dp_buf_mgr_t *buf_mgr)
{
	int i;

	if (!buf_mgr) {
		pr_err("%s: NULL param.\n", __func__);
		return;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER, "print ref list in\n");
	for (i = 0; i < VF_LIST_COUNT; i++) {
		if (atomic_read(&buf_mgr->ref_list[i].on_use)) {
			buf_mgr_print(buf_mgr, PRINT_OTHER,
				"ref_list[%2d]:%d; %d/%d; %d, %px, %d, %d; %d, %px, %px.\n",
				i,
				buf_mgr->ref_list[i].frame_index,
				buf_mgr->ref_list[i].ref_count,
				buf_mgr->ref_list[i].ref_number,
				buf_mgr->ref_list[i].ref_other_number,
				buf_mgr->ref_list[i].vf_p,
				buf_mgr->ref_list[i].index,
				buf_mgr->ref_list[i].buf_mgr_reset_id,
				buf_mgr->ref_list[i].flag,
				buf_mgr->ref_list[i].file,
				buf_mgr->ref_list[i].file_ext);
		}
	}
	buf_mgr_print(buf_mgr, PRINT_OTHER, "print ref list out\n");
}

void pop_from_ref_list(struct dp_buf_mgr_t *buf_mgr, struct vf_ref_t *vf_ref)
{
	if (!buf_mgr || !vf_ref) {
		pr_err("%s: NULL param.\n", __func__);
		return;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER,
			"%s frame_index=%d\n",
			__func__,
			vf_ref->frame_index);
	vf_ref->index = -1;
	vf_ref->frame_index = -1;
	vf_ref->vf_p = NULL;
	memset(&vf_ref->vf, 0, sizeof(struct vframe_s));
	vf_ref->ref_count = 0;
	vf_ref->ref_number = 0;
	vf_ref->file = NULL;
	vf_ref->file_ext = NULL;
	vf_ref->di_processed = false;
	//vf_ref->recycle_buffer_cb = NULL;
	vf_ref->buf_mgr_reset_id = 0;
	vf_ref->qbuf_id = 0;
	vf_ref->flag = 0;
	atomic_set(&vf_ref->on_use, false);
}

/**
 * @brief  vf_ref_count_dec  vf ref count -1
 *
 * @param[in]  vf	 vf
 *
 * @return	   0 for  success, or fail type if < 0
 */
int vf_ref_count_dec(struct dp_buf_mgr_t *buf_mgr, struct vf_ref_t *vf_ref)
{
	struct vframe_s *vf = NULL;

	if (!buf_mgr || !vf_ref) {
		pr_err("%s: NULL param.\n", __func__);
		return -1;
	}

	vf = &vf_ref->vf;
	if (!vf) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: vf is NULL.\n", __func__);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER,
		"%s: frame_index=%d, count=%d, file:%px, file_ext:%px\n",
		__func__,
		vf->frame_index,
		vf_ref->ref_count,
		vf_ref->file,
		vf_ref->file_ext);

	mutex_lock(&buf_mgr->ref_count_mutex);
	if (vf_ref->ref_count > 1) {
		vf_ref->ref_count--;
		mutex_unlock(&buf_mgr->ref_count_mutex);
	} else if (vf_ref->ref_count == 1) {
		vf_ref->ref_count = 0;
		mutex_unlock(&buf_mgr->ref_count_mutex);

		if (vf_ref->qbuf_id == buf_mgr->reset_id ||
			buf_mgr->dec_type == DEC_TYPE_VDEC_CORE_I) {
			buf_mgr_print(buf_mgr, PRINT_OTHER, "recycle:frame_index=%d\n",
				vf->frame_index);
			buf_mgr->recycle_buffer_cb(buf_mgr->caller_data, vf_ref->file,
				buf_mgr->instance_id);
		} else {
			buf_mgr_print(buf_mgr, PRINT_OTHER,
				"not recycle:frame_index=%d, qbuf_id: %d, reset_id: %d\n",
				vf->frame_index,
				vf_ref->qbuf_id,
				buf_mgr->reset_id);
		}
		pop_from_ref_list(buf_mgr, vf_ref);
	} else {
		mutex_unlock(&buf_mgr->ref_count_mutex);
		buf_mgr_print(buf_mgr, PRINT_OTHER, "ref_count is 0 !!!\n");
	}

	return 0;
}

/**
 * @brief  push_to_ref_list  put vf to disp list
 *
 * @param[in]  vf	 vf
 *
 * @return	   0 for  success, or fail type if < 0
 */
int push_to_ref_list(struct dp_buf_mgr_t *buf_mgr, struct vframe_s *vf, struct file *file)
{
	/*set vf to vf_ref list, and set ref_count
	 *I: THE_FIRST_FRAME set ref_count 1; THE_SECOND_FRAME 2; THE_NORMAL_FRAME 3
	 *P: THE_FIRST_FRAME set ref_count 1;THE_NORMAL_FRAME 2
	 */
	int i, j;
	struct vf_ref_t *vf_ref = NULL;
	struct vf_ref_t *ref_list_tmp1 = NULL;
	struct vf_ref_t *ref_list_tmp2 = NULL;

	if (!buf_mgr || !vf || !file) {
		pr_info("%s: NULL param.\n", __func__);
		return -1;
	}

	j = 0;
	while (1) {
		for (i = 0; i < VF_LIST_COUNT; i++) {
			if (!atomic_read(&buf_mgr->ref_list[i].on_use))
				break;
		}
		if (i == VF_LIST_COUNT) {
			print_all_in_ref_list(buf_mgr);
			j++;
			if (j > WAIT_DISP_Q_TIMEOUT) {
				buf_mgr_print(buf_mgr, PRINT_ERROR,
					"ref_list is full, wait timeout!\n");
				return -1;
			}
			usleep_range(1000 * list_q_wait, 1000 * (list_q_wait + 1));
			buf_mgr_print(buf_mgr, PRINT_ERROR,
				"ref_list is full, need wait =%d\n",
				j);
			continue;
		} else {
			break;
		}
	}
	vf_ref = &buf_mgr->ref_list[i];
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: vf_ref is NULL.\n", __func__);
		return -1;
	}
	atomic_set(&vf_ref->on_use, true);
	vf_ref->vf_p = vf;
	vf_ref->vf = *vf;
	vf_ref->frame_index = vf->frame_index;

	ref_list_tmp1 = buf_mgr->ref_list_1;
	ref_list_tmp2 = buf_mgr->ref_list_2;

	if (vf->type & VIDTYPE_INTERLACE) {
		if (buf_mgr->receive_count == 0) {
			vf_ref->ref_count = 3;
			vf_ref->ref_other_number = 1;
		} else if (buf_mgr->receive_count == 1) {
			vf_ref->ref_count = 3;
			vf_ref->ref_other_number = 2;
		} else {
			vf_ref->ref_count = 3;
			vf_ref->ref_other_number = 3;
		}

		if (ref_list_tmp1 &&
			ref_list_tmp1->vf_p &&
			!(ref_list_tmp1->vf_p->type & VIDTYPE_INTERLACE)) {
			vf_ref->ref_other_number = 2;
			buf_mgr_print(buf_mgr, PRINT_OTHER, "P->I\n");
		}

		if ((ref_list_tmp1 && ref_list_tmp1->vf_p &&
			ref_list_tmp1->vf_p->type & VIDTYPE_INTERLACE) &&
			(ref_list_tmp2 && ref_list_tmp2->vf_p &&
			!(ref_list_tmp2->vf_p->type & VIDTYPE_INTERLACE))) {
			vf_ref->ref_other_number = 2;
			buf_mgr_print(buf_mgr, PRINT_OTHER, "P->I->I\n");
		}
	} else {
		if (buf_mgr->receive_count == 0) {
			vf_ref->ref_count = 2;
			vf_ref->ref_other_number = 1;
		} else if (buf_mgr->receive_count == 1) {
			vf_ref->ref_count = 2;
			vf_ref->ref_other_number = 2;
		} else {
			vf_ref->ref_count = 2;
			vf_ref->ref_other_number = 2;
		}

		if (ref_list_tmp1 && ref_list_tmp1->vf_p &&
			ref_list_tmp1->vf_p->type & VIDTYPE_INTERLACE) {
			vf_ref_count_dec(buf_mgr, ref_list_tmp1);
			buf_mgr_print(buf_mgr, PRINT_OTHER, "I->P: ref_list_1 count need -1\n");

			if (ref_list_tmp2 && ref_list_tmp2->vf_p &&
				ref_list_tmp2->vf_p->type & VIDTYPE_INTERLACE) {
				vf_ref_count_dec(buf_mgr, ref_list_tmp2);
				buf_mgr_print(buf_mgr, PRINT_OTHER, "I->P: ref_list_2 count -1\n");
			}
		}
	}
	vf_ref->index = buf_mgr->receive_count++;
	vf_ref->ref_number = vf_ref->ref_count;
	vf_ref->file = file;
	if (buf_mgr->dec_type == DEC_TYPE_VDEC_CORE_I)
		vf_ref->file_ext = get_uvmfile_from_yuvfile(buf_mgr, file);
	else
		vf_ref->file_ext = NULL;
	vf_ref->di_processed = false;
	vf_ref->buf_mgr_reset_id = buf_mgr->reset_id;
	vf_ref->flag = 0;
	buf_mgr->ref_list_2 = ref_list_tmp1;
	buf_mgr->ref_list_1 = vf_ref;
	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s, i =%d, ref_count=%d, other=%d\n",
		__func__, i, vf_ref->ref_count, vf_ref->ref_other_number);

	return 0;
}

struct vf_ref_t *get_ref_from_list(struct dp_buf_mgr_t *buf_mgr, struct vframe_s *vf)
{
	int i;
	struct vf_ref_t *vf_ref = NULL;

	if (!buf_mgr || !vf) {
		pr_err("%s: NULL param.\n", __func__);
		return NULL;
	}

	for (i = 0; i < VF_LIST_COUNT; i++) {
		if (buf_mgr->ref_list[i].vf_p == vf &&
			buf_mgr->ref_list[i].frame_index == vf->frame_index)
			break;
	}
	if (i == VF_LIST_COUNT) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s get ref fail\n", __func__);
		print_all_in_ref_list(buf_mgr);
		return NULL;
	}

	if (!atomic_read(&buf_mgr->ref_list[i].on_use)) {
		buf_mgr_print(buf_mgr, PRINT_OTHER,
			"%s get ref fail: on_use is false\n",
			__func__);
		print_all_in_ref_list(buf_mgr);
		return NULL;
	}

	vf_ref = &buf_mgr->ref_list[i];
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: vf_ref is NULL.\n", __func__);
		return NULL;
	}

	if (vf->frame_index != vf_ref->frame_index) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s err: %d %d\n",
			__func__, vf->frame_index, vf_ref->frame_index);
		print_all_in_ref_list(buf_mgr);
	}

	return vf_ref;
}

struct vf_ref_t *get_ref1_from_list(struct dp_buf_mgr_t *buf_mgr, struct vf_ref_t *vf_ref)
{
	int i;
	struct vf_ref_t *vf_ref1 = NULL;

	if (!buf_mgr) {
		pr_err("%s: NULL param.\n", __func__);
		return NULL;
	}

	for (i = 0; i < VF_LIST_COUNT; i++) {
		if ((buf_mgr->ref_list[i].index + 1 == vf_ref->index) &&
			buf_mgr->ref_list[i].buf_mgr_reset_id == vf_ref->buf_mgr_reset_id)
			break;
	}
	if (i == VF_LIST_COUNT) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "get ref1 fail, not find ref, frame_index=%d\n",
			vf_ref->frame_index);
		print_all_in_ref_list(buf_mgr);
		return NULL;
	}

	if (!atomic_read(&buf_mgr->ref_list[i].on_use)) {
		buf_mgr_print(buf_mgr, PRINT_OTHER,
			"get ref1 fail: i=%d, index=%d, vf_ref->frame_index=%d, frame_index=%d\n",
			i, buf_mgr->ref_list[i].index,
			vf_ref->frame_index, vf_ref->vf.frame_index);
		print_all_in_ref_list(buf_mgr);
		return NULL;
	}

	vf_ref1 = &buf_mgr->ref_list[i];

	return vf_ref1;
}

static struct vf_ref_t *get_ref_from_yuvfile(struct dp_buf_mgr_t *buf_mgr,
					struct file *file, struct file *file_ext)
{
	int i = 0;
	struct vf_ref_t *vf_ref = NULL;

	if (!buf_mgr || !file || !file_ext) {
		pr_err("%s: NULL param.\n", __func__);
		return NULL;
	}

	for (i = 0; i < VF_LIST_COUNT; i++) {
		if (buf_mgr->ref_list[i].file == file &&
			buf_mgr->ref_list[i].file_ext == file_ext)
			break;
	}
	if (i == VF_LIST_COUNT) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s get ref fail\n", __func__);
		print_all_in_ref_list(buf_mgr);
		return NULL;
	}

	if (!atomic_read(&buf_mgr->ref_list[i].on_use)) {
		buf_mgr_print(buf_mgr, PRINT_ERROR,
			"%s get ref fail: on_use is false\n",
			__func__);
		print_all_in_ref_list(buf_mgr);
		return NULL;
	}

	vf_ref = &buf_mgr->ref_list[i];
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s failed.\n", __func__);
		return NULL;
	}

	return vf_ref;
}

static int process_ref_in_list(struct dp_buf_mgr_t *buf_mgr, struct vf_ref_t *vf_ref)
{
	struct vf_ref_t *vf_ref1 = NULL;
	struct vf_ref_t *vf_ref2 = NULL;

	if (!buf_mgr || !vf_ref) {
		pr_err("%s: NULL param.\n", __func__);
		return -1;
	}

	if (vf_ref->qbuf_id > 0) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: second:%d %d\n",
			__func__, vf_ref->qbuf_id, buf_mgr->reset_id);
		vf_ref->qbuf_id = buf_mgr->reset_id;
		return 0;
	}
	vf_ref->qbuf_id = buf_mgr->reset_id;

	buf_mgr_print(buf_mgr, PRINT_OTHER,
		"%s: di_processed=%d\n",
		__func__,
		vf_ref->di_processed);
	/*drop frame, need N, N-1, N-2 ref-1*/
	if (!vf_ref->di_processed) {
		if (vf_ref->ref_other_number == 2) {
			vf_ref1 = get_ref1_from_list(buf_mgr, vf_ref);
			if (!vf_ref1) {
				buf_mgr_print(buf_mgr, PRINT_ERROR, "%s:0ref1 dec fail\n",
					__func__);
				goto exit;
			}
			vf_ref_count_dec(buf_mgr, vf_ref1);
		} else if  (vf_ref->ref_other_number == 3) {
			vf_ref1 = get_ref1_from_list(buf_mgr, vf_ref);
			if (!vf_ref1) {
				buf_mgr_print(buf_mgr, PRINT_ERROR, "%s:1ref1 dec fail\n",
					__func__);
				goto exit;
			}

			vf_ref2 = get_ref1_from_list(buf_mgr, vf_ref1);
			if (!vf_ref2) {
				buf_mgr_print(buf_mgr, PRINT_ERROR, "%s:ref2 dec fail\n",
					__func__);
				vf_ref_count_dec(buf_mgr, vf_ref1);
				goto exit;
			}
			vf_ref_count_dec(buf_mgr, vf_ref2);
			vf_ref_count_dec(buf_mgr, vf_ref1);
		}
	}

exit:
	if (vf_ref->di_processed && !(vf_ref->flag & VF_REF_FLAG_DIPROCESS_CHECKIN)) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: ref need dec self.\n", __func__);
		vf_ref->flag |= VF_REF_FLAG_DECREASE_SELF;
	} else {
		vf_ref_count_dec(buf_mgr, vf_ref);
	}

	return 0;
}

void buf_mgr_get(struct dp_buf_mgr_t *buf_mgr)
{
	if (!buf_mgr) {
		pr_err("di buf_mgr get fail\n");
		return;
	}

	mutex_lock(&buf_mgr_mutex);
	buf_mgr->get_count++;
	buf_mgr_print(buf_mgr, PRINT_MORE, "%s: get_count=%d\n", __func__, buf_mgr->get_count);
	mutex_unlock(&buf_mgr_mutex);
}

void buf_mgr_put(struct dp_buf_mgr_t *buf_mgr)
{
	if (!buf_mgr) {
		pr_err("di buf_mgr put fail\n");
		return;
	}

	mutex_lock(&buf_mgr_mutex);
	buf_mgr->get_count--;
	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: get_count=%d\n", __func__, buf_mgr->get_count);
	if (buf_mgr->get_count == 0) {
		mutex_unlock(&buf_mgr_mutex);
		buf_mgr_print(buf_mgr, PRINT_ERROR, "di buf_mgr free\n");
		vfree(buf_mgr);
		return;
	}
	mutex_unlock(&buf_mgr_mutex);
}

void free_di_mgr_data(void *arg)
{
	struct uvm_di_mgr_t *uvm_di_mgr = (struct uvm_di_mgr_t *)arg;

	if (uvm_di_mgr) {
		if (!uvm_di_mgr->buf_mgr) {
			pr_err("%s: MULL param.\n", __func__);
			return;
		}

		buf_mgr_print(uvm_di_mgr->buf_mgr, PRINT_OTHER, "%s: arg=%px, file=%px\n",
			__func__, arg, uvm_di_mgr->file);
		buf_mgr_free_checkin(uvm_di_mgr->buf_mgr, uvm_di_mgr->file);
		if (!IS_ERR_OR_NULL(uvm_di_mgr->uhmod_v4lvideo) &&
			!IS_ERR_OR_NULL(uvm_di_mgr->file)) {
			buf_mgr_print(uvm_di_mgr->buf_mgr, PRINT_OTHER, "put v4lvideo data.\n");
			uvm_put_hook_mod((struct dma_buf *)(uvm_di_mgr->file->private_data),
				VF_PROCESS_V4LVIDEO);
		}

		if (!IS_ERR_OR_NULL(uvm_di_mgr->uhmod_dec) && !IS_ERR_OR_NULL(uvm_di_mgr->file)) {
			buf_mgr_print(uvm_di_mgr->buf_mgr, PRINT_OTHER, "put decoder data.\n");
			uvm_put_hook_mod((struct dma_buf *)(uvm_di_mgr->file->private_data),
				VF_SRC_DECODER);
		}

		buf_mgr_put(uvm_di_mgr->buf_mgr);

		kfree((u8 *)arg);
	} else {
		pr_err("%s: arg is NULL\n", __func__);
	}
}

int di_proc_buf_mgr_getinfo(void *arg, char *buf)
{
	return 0;
}

int di_proc_buf_mgr_setinfo(void *arg, char *buf)
{
	return 0;
}

/**
 * @brief  vf_to_file  set all vf to file
 *
 * @param[in]  file    file
 *
 * @return	   0 for  success, or fail type if < 0
 */
int buf_mgr_to_file(struct dp_buf_mgr_t *buf_mgr, struct file *file)
{
	struct dma_buf *dmabuf = NULL;
	struct uvm_hook_mod *uhmod = NULL;
	struct uvm_di_mgr_t *uvm_di_mgr = NULL;
	struct uvm_hook_mod_info info;
	bool attached = false;
	int ret;

	if (!buf_mgr || !file) {
		pr_err("%s: NULL param.\n", __func__);
		return -1;
	}

	dmabuf = (struct dma_buf *)(file->private_data);
	if (!dmabuf) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "dmabuf is NULL\n");
		return -1;
	}

	uhmod = uvm_get_hook_mod(dmabuf, PROCESS_DI_MGR);
	if (IS_ERR_OR_NULL(uhmod)) {
		uvm_di_mgr = kmalloc(sizeof(*uvm_di_mgr), GFP_KERNEL);
		buf_mgr_print(buf_mgr, PRINT_OTHER, "attach:PROCESS_DI_MGR\n");
		if (!uvm_di_mgr) {
			dma_buf_put(dmabuf);
			return -ENOMEM;
		}
	} else {
		uvm_put_hook_mod(dmabuf, PROCESS_DI_MGR);
		attached = true;
		uvm_di_mgr = uhmod->arg;
		if (!uvm_di_mgr) {
			buf_mgr_print(buf_mgr, PRINT_ERROR,
				"attach:uvm_di_mgr is null, dmabuf=%p\n", dmabuf);
			dma_buf_put(dmabuf);
			return -EINVAL;
		}
		buf_mgr_print(buf_mgr, PRINT_OTHER, "uvm_di_mgr=%px\n", uvm_di_mgr);
	}

	if (attached) {
		if (uvm_di_mgr->buf_mgr != buf_mgr) {
			buf_mgr_put(uvm_di_mgr->buf_mgr);
			buf_mgr_get(buf_mgr);
			uvm_di_mgr->buf_mgr = buf_mgr;
			buf_mgr_print(buf_mgr, PRINT_OTHER, "%s:buf_mgr changed\n", __func__);
		}
		return 0;
	}

	memset(uvm_di_mgr, 0, sizeof(*uvm_di_mgr));
	uvm_di_mgr->buf_mgr = buf_mgr;
	uvm_di_mgr->file = file;
	uvm_di_mgr->uhmod_v4lvideo = uvm_get_hook_mod(dmabuf, VF_PROCESS_V4LVIDEO);
	uvm_di_mgr->uhmod_dec = uvm_get_hook_mod(dmabuf, VF_SRC_DECODER);
	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s:uvm_di_mgr=%px, file=%px\n",
		__func__, uvm_di_mgr, file);

	buf_mgr_get(buf_mgr);

	info.type = PROCESS_DI_MGR;
	info.arg = uvm_di_mgr;
	info.free = free_di_mgr_data;
	info.acquire_fence = NULL;
	info.getinfo = di_proc_buf_mgr_getinfo;
	info.setinfo = di_proc_buf_mgr_setinfo;

	ret = uvm_attach_hook_mod(dmabuf, &info);
	if (ret < 0) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "fail attach PROCESS_DI_MGR\n");
		return -EINVAL;
	}
	return 0;
}

struct uvm_di_mgr_t *get_uvm_di_mgr(struct file *file_vf)
{
	struct uvm_di_mgr_t *uvm_di_mgr;
	struct uvm_hook_mod *uhmod;

	if (!file_vf) {
		pr_err("%s file is NULL\n", __func__);
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data), PROCESS_DI_MGR);
	if (!uhmod) {
		pr_err("%s uhmod is NULL\n", __func__);
		return NULL;
	}

	if (IS_ERR_VALUE(uhmod) || !uhmod->arg) {
		pr_err("%s file_private_data is NULL\n", __func__);
		return NULL;
	}
	uvm_di_mgr = uhmod->arg;
	uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data), PROCESS_DI_MGR);

	return uvm_di_mgr;
}

/**
 * @brief  buf_mgr_creat  creat buf mgr instance
 *
 * @param[in]  dec_type    dec_type
 * @param[in]  id	 decoder instance id, need always ++ from system boot
 * @param[in]  caller_data	 decoder instance dev
 *
 * @return	   struct dp_buf_mgr_t * for  success, or NULL for fail
 */
struct dp_buf_mgr_t *buf_mgr_creat(int dec_type, int id, void *caller_data,
	void (*recycle_buffer_cb)(void *caller_data, struct file *file, int instance_id))
{
	struct dp_buf_mgr_t *buf_mgr = NULL;

	buf_mgr = vmalloc(sizeof(*buf_mgr));
	if (!buf_mgr)
		return NULL;

	memset(buf_mgr, 0, sizeof(*buf_mgr));
	buf_mgr->dec_type = dec_type;
	buf_mgr->instance_id = id;
	buf_mgr->caller_data = caller_data;
	buf_mgr->recycle_buffer_cb = recycle_buffer_cb;
	buf_mgr->reset_id = 1;
	buf_mgr_print(buf_mgr, PRINT_OTHER, "buf mgr creat\n");
	buf_mgr_get(buf_mgr);
	mutex_init(&buf_mgr->ref_count_mutex);
	mutex_init(&buf_mgr->file_mutex);
	return buf_mgr;
}
EXPORT_SYMBOL(buf_mgr_creat);

/**
 * @brief  buf_mgr_release	release buf mgr instance
 *
 * @param[in]  dp_buf_mgr_t *	 dp_buf_mgr_t
 *
 * @return	   0 for  success, or fail type if < 0
 */
int buf_mgr_release(struct dp_buf_mgr_t *buf_mgr)
{
	if (!buf_mgr) {
		pr_err("%s buf_mgr is null\n", __func__);
		return -1;
	}

	buf_mgr->reset_id++;
	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s func is called\n", __func__);
	buf_mgr_put(buf_mgr);
	return 0;
}
EXPORT_SYMBOL(buf_mgr_release);

/**
 * @brief  buf_mgr_reset  reset buf mgr instance for seek
 *
 * @param[in]  dp_buf_mgr_t *	 dp_buf_mgr_t
 *
 * @return	   0 for  success, or fail type if < 0
 */
int buf_mgr_reset(struct dp_buf_mgr_t *buf_mgr)
{
	if (!buf_mgr) {
		pr_err("%s buf_mgr is null\n", __func__);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s func is called\n", __func__);

	mutex_lock(&buf_mgr->file_mutex);
	buf_mgr->reset_id++;
	mutex_unlock(&buf_mgr->file_mutex);

	buf_mgr->receive_count = 0;

	if (buf_mgr->ref_list_1) {
		if (buf_mgr->ref_list_1->ref_number == 2) {
			vf_ref_count_dec(buf_mgr, buf_mgr->ref_list_1);
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
			buf_mgr_set_eos(print_flag, buf_mgr->ref_list_1);
			get_eos_from_dp(1);
#endif
		} else if (buf_mgr->ref_list_1->ref_number == 3) {
#ifdef CONFIG_AMLOGIC_DPSS_PROCESS
			buf_mgr_set_eos(print_flag, buf_mgr->ref_list_1);
			get_eos_from_dp(1);
#endif
			vf_ref_count_dec(buf_mgr, buf_mgr->ref_list_1);
			vf_ref_count_dec(buf_mgr, buf_mgr->ref_list_1);
		}
	}

	if (buf_mgr->ref_list_2) {
		if (buf_mgr->ref_list_2->ref_number == 3)
			vf_ref_count_dec(buf_mgr, buf_mgr->ref_list_2);
	}

	buf_mgr->ref_list_1 = NULL;
	buf_mgr->ref_list_2 = NULL;
	return 0;
}
EXPORT_SYMBOL(buf_mgr_reset);

/**
 * @brief  buf_mgr_dq_checkin  dq buffer from decoder need checkin vf info
 *
 * @param[in]  dp_buf_mgr_t *buf_manager
 * @param[in]  file    file
 *
 * @return	   0 for  success, or fail type if < 0
 */
int buf_mgr_dq_checkin(struct dp_buf_mgr_t *buf_mgr, struct file *file)
{
	/*1: set vf to vf_ref list, 2: set ref_count; 3: set callback
	 *I: THE_FIRST_FRAME set ref_count 1; THE_SECOND_FRAME 2; THE_NORMAL_FRAME 3
	 *P: THE_FIRST_FRAME set ref_count 1;THE_NORMAL_FRAME 2
	 */
	struct vframe_s *vf;
	struct file *uvm_file = NULL;

	if (!buf_mgr) {
		pr_err("%s: NULL param.\n", __func__);
		return -1;
	}

	if (!di_proc_enable)
		return -1;

	if (buf_mgr->dec_type == DEC_TYPE_VDEC_CORE_I) {
		uvm_file = get_uvmfile_from_yuvfile(buf_mgr, file);
		vf = get_vf_from_file(buf_mgr, uvm_file);
	} else {
		vf = get_vf_from_file(buf_mgr, file);
	}
	if (!vf) {
		buf_mgr_print(buf_mgr, PRINT_ERROR,
			"%s: not find vf, buf_mgr=%px, file=%px\n", __func__, buf_mgr, file);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER,
		"%s: frame_index=%d, %px, %px\n", __func__, vf->frame_index, buf_mgr, file);

	if (vf->type & VIDTYPE_V4L_EOS) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: eos\n", __func__);
		return -1;
	}

	push_to_ref_list(buf_mgr, vf, file);
	if (buf_mgr->dec_type == DEC_TYPE_VDEC_CORE_I)
		buf_mgr_to_file(buf_mgr, uvm_file);
	else
		buf_mgr_to_file(buf_mgr, file);

	return 0;
}
EXPORT_SYMBOL(buf_mgr_dq_checkin);

/**
 * @brief  buf_mgr_q_checkin  q buffer from hal need checkin vf info
 *
 * @param[in]  file    file
 *
 * @return	   0 for  success, or fail type if < 0
 */
int buf_mgr_q_checkin(struct dp_buf_mgr_t *buf_mgr, struct file *file)
{
	struct vframe_s *vf = NULL;
	struct vf_ref_t *vf_ref = NULL;

	if (!buf_mgr) {
		pr_err("%s: NULL param.\n", __func__);
		return -1;
	}

	if (!di_proc_enable)
		return -1;

	/*get main vf*/
	vf = get_vf_from_file(buf_mgr, file);
	if (!vf) {
		buf_mgr_print(buf_mgr, PRINT_OTHER,
			"%s: not find vf, buf_mgr=%px, file=%px\n",
			__func__,
			buf_mgr,
			file);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER,
		"%s: frame_index=%d, buf_mgr=%px, file=%px\n",
		__func__,
		vf->frame_index,
		buf_mgr,
		file);

	mutex_lock(&buf_mgr->file_mutex);
	vf_ref = get_ref_from_list(buf_mgr, vf);
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "q_checkin: ref dec fail\n");
		mutex_unlock(&buf_mgr->file_mutex);
		return -1;
	}

	process_ref_in_list(buf_mgr, vf_ref);
	mutex_unlock(&buf_mgr->file_mutex);

	return 0;
}
EXPORT_SYMBOL(buf_mgr_q_checkin);

/**
 * @brief  buf_mgr_q_checkin_dec  q buffer from hal need checkin vf info
 *
 * @param[in]  file    file
 * @param[in]  file_ext    used for H264 interlace uvm_dma
 *
 * @return     0 for  success, or fail type if < 0
 */
int buf_mgr_q_checkin_dec(struct dp_buf_mgr_t *buf_mgr, struct file *file,
	struct file *file_ext)
{
	struct vf_ref_t *vf_ref = NULL;
	int ret = 0;

	if (!buf_mgr || !file) {
		pr_err("%s: NULL param.\n", __func__);
		return -1;
	}

	if (!di_proc_enable)
		return -1;

	if (buf_mgr->dec_type == DEC_TYPE_VDEC_CORE_I) {
		mutex_lock(&buf_mgr->file_mutex);
		vf_ref = get_ref_from_yuvfile(buf_mgr, file, file_ext);
		if (!vf_ref) {
			buf_mgr_print(buf_mgr, PRINT_OTHER,
				"%s: ref dec fail, buf_mgr=%px, file=%px, file_ext=%px.\n",
				__func__,
				buf_mgr,
				file,
				file_ext);
			mutex_unlock(&buf_mgr->file_mutex);
			return -1;
		}

		buf_mgr_print(buf_mgr, PRINT_OTHER,
			"%s: frame_index=%d, buf_mgr=%px, file=%px, file_ext=%px.\n",
			__func__,
			vf_ref->vf.frame_index,
			buf_mgr,
			file,
			file_ext);
		process_ref_in_list(buf_mgr, vf_ref);
		mutex_unlock(&buf_mgr->file_mutex);
	} else {
		ret = buf_mgr_q_checkin(buf_mgr, file);
	}

	return ret;
}
EXPORT_SYMBOL(buf_mgr_q_checkin_dec);

int buf_mgr_free_checkin(struct dp_buf_mgr_t *buf_mgr, struct file *file)
{
	struct vframe_s *vf = NULL;
	struct vf_ref_t *vf_ref = NULL;

	if (!buf_mgr) {
		pr_err("%s: NULL param.\n", __func__);
		return -1;
	}

	/*get main vf*/
	vf = get_vf_from_file(buf_mgr, file);
	if (!vf) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: not find vf\n", __func__);
		return -1;
	}

	vf_ref = get_ref_from_list(buf_mgr, vf);
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: ref dec fail\n", __func__);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: frame_index=%d\n", __func__, vf->frame_index);

	mutex_lock(&buf_mgr->file_mutex);
	pop_from_ref_list(buf_mgr, vf_ref);
	mutex_unlock(&buf_mgr->file_mutex);

	return 0;
}

struct dp_buf_mgr_t *get_buf_mgr(struct file *file)
{
	struct uvm_di_mgr_t *uvm_di_mgr = NULL;

	if (!file) {
		pr_err("%s fail file is NULL\n", __func__);
		return NULL;
	}

	uvm_di_mgr = get_uvm_di_mgr(file);
	if (!uvm_di_mgr) {
		pr_err("%s: get uvm_di_mgr failed.\n", __func__);
		return NULL;
	}

	return uvm_di_mgr->buf_mgr;
}

/**
 * @brief  di_processed_checkin  when di process finished, need checkin
 *
 * @param[in]  struct dp_buf_mgr_t *buf_manager
 * @param[in]  file  file that di finish process
 *
 * @return	   0 for  success, or fail type if < 0
 */
int di_processed_checkin(struct file *file)
{
	struct vframe_s *vf = NULL;
	struct vf_ref_t *vf_ref = NULL;
	struct vf_ref_t *vf_ref1 = NULL;
	struct vf_ref_t *vf_ref2 = NULL;
	struct dp_buf_mgr_t *buf_mgr = NULL;
	int need_dec_two = false;

	buf_mgr = get_buf_mgr(file);
	if (!buf_mgr) {
		pr_err("%s get buf_mgr fail\n", __func__);
		return -1;
	}

	vf = get_vf_from_file(buf_mgr, file);
	if (!vf) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: not find vf\n", __func__);
		return -1;
	}

	mutex_lock(&buf_mgr->file_mutex);
	vf_ref = get_ref_from_list(buf_mgr, vf);
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: ref dec fail\n", __func__);
		mutex_unlock(&buf_mgr->file_mutex);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER,
		"%s: frame_index=%d, ref_other_number=%d.\n",
		__func__,
		vf->frame_index,
		vf_ref->ref_other_number);
	if (vf_ref->ref_other_number > 2)
		need_dec_two = true;

	if (vf_ref->ref_other_number > 1) {
		vf_ref1 = get_ref1_from_list(buf_mgr, vf_ref);
		if (!vf_ref1) {
			buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: ref1 dec fail.\n", __func__);
			mutex_unlock(&buf_mgr->file_mutex);
			return -1;
		}
		if (!need_dec_two)
			vf_ref_count_dec(buf_mgr, vf_ref1);
	}

	if (need_dec_two) {
		vf_ref2 = get_ref1_from_list(buf_mgr, vf_ref1);
		if (!vf_ref2) {
			buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: ref2 dec fail.\n", __func__);
			vf_ref_count_dec(buf_mgr, vf_ref1);
			mutex_unlock(&buf_mgr->file_mutex);
			return -1;
		}
		vf_ref_count_dec(buf_mgr, vf_ref2);
		vf_ref_count_dec(buf_mgr, vf_ref1);
	}

	if (vf_ref->di_processed && !(vf->flag & VFRAME_FLAG_DOUBLE_FRAM))
		buf_mgr_print(buf_mgr, PRINT_MORE, "processed: already processed!!!\n");
	vf_ref->di_processed = true;
	vf_ref->flag |= VF_REF_FLAG_DIPROCESS_CHECKIN;

	if (vf_ref->flag & VF_REF_FLAG_DECREASE_SELF) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: self ref decrease.\n", __func__);
		vf_ref_count_dec(buf_mgr, vf_ref);
	}

	/*if index = N processes, force release all frame that < N*/
	print_all_in_ref_list(buf_mgr);
	mutex_unlock(&buf_mgr->file_mutex);
	return 0;
}

/**
 * @brief  di_get_ref_vf  get ref vf from bug_mgr
 *
 * @param[in]  file    file
 * @param[in]  vf_1  last frame n-1
 * @param[in]  vf_2  last frame n-2
 *
 * @return	   0 for  success, or fail type if < 0
 */
int di_get_ref_vf(struct file *file, struct vframe_s **vf_1, struct vframe_s **vf_2,
	struct file **file_1, struct file **file_2)
{
	struct vf_ref_t *vf_ref = NULL;
	struct dp_buf_mgr_t *buf_mgr = NULL;
	struct vframe_s *vf = NULL;
	struct vf_ref_t *vf_ref1 = NULL;
	struct vf_ref_t *vf_ref2 = NULL;
	bool is_vdec_core_i = false;

	*vf_1 = NULL;
	*vf_2 = NULL;
	*file_1 = NULL;
	*file_2 = NULL;

	buf_mgr = get_buf_mgr(file);
	if (!buf_mgr) {
		pr_err("%s get buf_mgr fail\n", __func__);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: file:%px.\n", __func__, file);

	if (buf_mgr->dec_type == DEC_TYPE_VDEC_CORE_I) {
		buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: new flow for di.\n", __func__);
		is_vdec_core_i = true;
	}

	vf = get_vf_from_file(buf_mgr, file);
	if (!vf) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: vf is null\n", __func__);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER, "%s: file:%px, frame_index=%d.\n",
		__func__,
		file,
		vf->frame_index);

	vf_ref = get_ref_from_list(buf_mgr, vf);
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: get vf_ref fail\n", __func__);
		return -1;
	}

	if (vf_ref->ref_number > 1 && vf_ref->buf_mgr_reset_id == buf_mgr->reset_id) {
		vf_ref1 = get_ref1_from_list(buf_mgr, vf_ref);
		if (!vf_ref1) {
			buf_mgr_print(buf_mgr, PRINT_ERROR, "%s: get vf_ref1 fail\n", __func__);
			return -1;
		}
		*vf_1 = &vf_ref1->vf;
		if (is_vdec_core_i)
			*file_1 = get_uvmfile_from_yuvfile(buf_mgr, vf_ref1->file);
		else
			*file_1 = vf_ref1->file;
	}

	if (vf_ref->ref_number > 2 && vf_ref->buf_mgr_reset_id == buf_mgr->reset_id) {
		vf_ref2 = get_ref1_from_list(buf_mgr, vf_ref1);
		if (!vf_ref2) {
			buf_mgr_print(buf_mgr, PRINT_ERROR, "processed: ref2 dec fail\n");
			return -1;
		}
		*vf_2 = &vf_ref2->vf;
		if (is_vdec_core_i)
			*file_2 = get_uvmfile_from_yuvfile(buf_mgr, vf_ref2->file);
		else
			*file_2 = vf_ref2->file;
	}
	return 0;
}

void buf_mgr_file_lock(struct uvm_di_mgr_t *uvm_di_mgr)
{
	struct dp_buf_mgr_t *buf_mgr = NULL;

	if (!uvm_di_mgr) {
		pr_err("%s NULL param.\n", __func__);
		return;
	}

	buf_mgr = uvm_di_mgr->buf_mgr;
	if (buf_mgr)
		mutex_lock(&buf_mgr->file_mutex);
}

void buf_mgr_file_unlock(struct uvm_di_mgr_t *uvm_di_mgr)
{
	struct dp_buf_mgr_t *buf_mgr = NULL;

	if (!uvm_di_mgr) {
		pr_err("%s NULL param.\n", __func__);
		return;
	}

	buf_mgr = uvm_di_mgr->buf_mgr;
	if (buf_mgr)
		mutex_unlock(&buf_mgr->file_mutex);
}

int update_di_process_state(struct file *file)
{
	struct dp_buf_mgr_t *buf_mgr = NULL;
	struct vframe_s *vf = NULL;
	struct vf_ref_t *vf_ref = NULL;

	buf_mgr = get_buf_mgr(file);
	if (!buf_mgr) {
		pr_err("%s get buf_mgr fail\n", __func__);
		return -1;
	}

	/*get main vf*/
	vf = get_vf_from_file(buf_mgr, file);
	if (!vf) {
		buf_mgr_print(buf_mgr, PRINT_OTHER,
			"%s: not find vf, buf_mgr=%px, file=%px\n", __func__, buf_mgr, file);
		return -1;
	}

	buf_mgr_print(buf_mgr, PRINT_OTHER,
		"%s: frame_index=%d, %px, %px\n", __func__, vf->frame_index, buf_mgr, file);

	vf_ref = get_ref_from_list(buf_mgr, vf);
	if (!vf_ref) {
		buf_mgr_print(buf_mgr, PRINT_ERROR, "get ref fail.\n");
		return -1;
	}

	vf_ref->di_processed = true;
	return 0;
}
