// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/vmalloc.h>
#include "di_proc_file.h"
#include <linux/amlogic/media/video_processor/di_proc_buf_mgr.h>
#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
#include <linux/amlogic/media/di/di_interface.h>
#endif
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>

#define IS_DI_PSTLINK(di_flag) ((di_flag) & DI_FLAG_DI_PSTVPPLINK)

static int di_proc_file_print(int debug_flag, const char *fmt, ...)
{
	if ((dp_buf_mgr_print_flag & debug_flag) ||
	    debug_flag == PRINT_ERROR) {
		unsigned char buf[256];
		int len = 0;
		va_list args;

		va_start(args, fmt);
		len = sprintf(buf, "dp_file:");
		vsnprintf(buf + len, 256 - len, fmt, args);
		pr_info("%s", buf);
		va_end(args);
	}
	return 0;
}

static void di_proc_vf_free(struct file_private_data *file_private_data)
{
	struct vframe_s *vf;
	u32 flag;
	struct di_buffer *buf;

	if (!file_private_data) {
		di_proc_file_print(PRINT_OTHER, "%s: NULL param.\n", __func__);
		return;
	}

	vf = &file_private_data->vf;
	flag = file_private_data->flag;
	di_proc_file_print(PRINT_OTHER, "vf_free: type=0x%x, flag=0x%x\n", vf->type, flag);
	if (flag & V4LVIDEO_FLAG_DI_V3) {
		/*di vf has dec vf, need put dec file*/
		if (vf->flag & VFRAME_FLAG_DOUBLE_FRAM) {
			vf = vf->vf_ext;
			di_proc_file_print(PRINT_ERROR,
				"free: di has dec vf, vf=%px, file=%px.\n",
				vf, file_private_data->file);
			if (vf && file_private_data->file)
				dp_put_file_ext(0, file_private_data->file);
			else
				di_proc_file_print(PRINT_ERROR,
					"free: di has dec vf, but dec vf/file is null.\n");
		}
		if (vf && vf->di_flag && IS_DI_PSTLINK(vf->di_flag) &&
			file_private_data->keep_id > 0) {
			/*postlink need free keeped local buf*/
			codec_mm_keeper_unmask_keeper(file_private_data->keep_id, 0);
			di_proc_file_print(PRINT_OTHER,
				"free keep: keep_id=%d\n", file_private_data->keep_id);
		} else  if (vf && (!(vf->di_flag & DI_FLAG_DI_PVPPLINK))) {
			buf = (struct di_buffer *)file_private_data->private2;
			if (!buf) {
				di_proc_file_print(PRINT_OTHER, "%s: buf is NULL.\n", __func__);
				return;
			}
			di_proc_file_print(PRINT_OTHER, "vf_free: release di v3 buf\n");
		#ifdef CONFIG_AMLOGIC_MEDIA_DEINTERLACE
			di_release_keep_buf(buf);
		#endif
			total_fill_count++;
		}
	}  else if (flag & V4LVIDEO_FLAG_DI_BYPASS) {
		/*di bypass, need put dec file*/
		vf = (struct vframe_s *)(file_private_data->vf_p);
		di_proc_file_print(PRINT_OTHER,
			"vf_free: di bypass, vf=%px, file=%px\n",
			vf, file_private_data->file);
		if (vf && file_private_data->file)
			dp_put_file_ext(0, file_private_data->file);
		else
			di_proc_file_print(PRINT_ERROR,
				"free: di bypass, but dec vf/file is null.\n");
	} else {
		di_proc_file_print(PRINT_ERROR, "vf_free: is not v3 buf\n");
	}
}

static void di_proc_private_data_release(struct file_private_data *data)
{
	if (!data)
		return;

	di_proc_file_print(PRINT_OTHER, "%s: is_keep =%d\n", __func__, data->is_keep);
	if (data->is_keep)
		di_proc_vf_free(data);

	memset(data, 0, sizeof(struct file_private_data));
}

static void di_proc_free_fd_private(void *arg)
{
	if (arg) {
		di_proc_file_print(PRINT_OTHER, "free_fd_private\n");
		di_proc_private_data_release(arg);
		vfree((u8 *)arg);
	} else {
		di_proc_file_print(PRINT_ERROR, "free: arg is NULL\n");
	}
}

struct file_private_data *di_proc_get_file_private_data(struct file *file_vf,
							 bool alloc_if_null)
{
	struct file_private_data *file_private_data;
	struct uvm_hook_mod *uhmod;
	struct uvm_hook_mod_info info;
	struct dma_buf *dmabuf;
	int ret;

	if (!file_vf) {
		di_proc_file_print(PRINT_ERROR, "get_file_private_data fail\n");
		return NULL;
	}

	dmabuf = (struct dma_buf *)file_vf->private_data;
	if (IS_ERR_OR_NULL(dmabuf)) {
		di_proc_file_print(PRINT_ERROR, "%s: private_data is null\n", __func__);
		return NULL;
	}

	if (!dmabuf_is_uvm(dmabuf)) {
		di_proc_file_print(PRINT_ERROR, "%s: dmabuf is not uvm\n", __func__);
		return NULL;
	}

	uhmod = uvm_get_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
	if (uhmod && uhmod->arg) {
		file_private_data = uhmod->arg;
		uvm_put_hook_mod((struct dma_buf *)(file_vf->private_data),
				 VF_PROCESS_V4LVIDEO);
		return file_private_data;
	} else if (!alloc_if_null) {
		return NULL;
	}

	file_private_data = vmalloc(sizeof(*file_private_data));
	if (!file_private_data)
		return NULL;
	memset(file_private_data, 0, sizeof(struct file_private_data));

	memset(&info, 0, sizeof(struct uvm_hook_mod_info));
	di_proc_file_print(PRINT_OTHER, "uvm attch\n");
	info.type = VF_PROCESS_V4LVIDEO;
	info.arg = file_private_data;
	info.free = di_proc_free_fd_private;
	info.acquire_fence = NULL;
	ret = uvm_attach_hook_mod((struct dma_buf *)(file_vf->private_data),
				   &info);
	return file_private_data;
}

