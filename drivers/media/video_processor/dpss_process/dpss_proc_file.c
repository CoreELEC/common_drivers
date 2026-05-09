// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/video_processor/di_proc_buf_mgr.h>
#include <linux/amlogic/media/codec_mm/codec_mm_keeper.h>
#include <linux/amlogic/media/video_processor/video_pipe_adapter.h>
#include "dpss_process.h"
#include "dpss_proc_file.h"

static int di_proc_file_print(int debug_flag, const char *fmt, ...)
{
	if ((dpss_buf_mgr_print_flag & debug_flag) ||
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

static void di_proc_free_fd_private(void *arg)
{
	struct vframe_s *vf;
	u32 flag;
	struct file_private_data *data = (struct file_private_data *)arg;

	if (!data) {
		di_proc_file_print(PRINT_ERROR, "%s: arg is NULL\n", __func__);
	} else {
		di_proc_file_print(PRINT_OTHER, "%s: is_keep =%d\n", __func__, data->is_keep);
		if (data->is_keep) {
			vf = data->vf_p;
			flag = data->flag;
			di_proc_file_print(PRINT_OTHER, "%s: flag=0x%x\n", __func__, flag);
			if (flag & V4LVIDEO_FLAG_DI_V3) {
				//if (vf && (!(vf->di_flag & DI_FLAG_DI_PVPPLINK))) {
				di_proc_file_print(PRINT_OTHER,
					"%s: release dpss v3 buf\n",
					__func__);
					/*release keep buf*/
					if (data->keep_id > 0) {
						video_keeper_free_mem(data->keep_id, 0);
						data->keep_id = -1;
						di_proc_file_print(PRINT_OTHER,
							"%s: release mem_handle buf", __func__);
					}

					if (data->keep_head_id > 0) {
						video_keeper_free_mem(data->keep_head_id, 0);
						data->keep_head_id = -1;
						di_proc_file_print(PRINT_OTHER,
							"%s:release keep head buf", __func__);
					}

					if (data->keep_id_1 > 0) {
						video_keeper_free_mem(data->keep_id_1, 0);
						data->keep_id_1 = -1;
						di_proc_file_print(PRINT_OTHER,
							"%s:release keep table buf", __func__);
					}

					if (data->keep_dw_id > 0) {
						video_keeper_free_mem(data->keep_dw_id, 0);
						data->keep_dw_id = -1;
						di_proc_file_print(PRINT_OTHER,
							"%s: release keep dw buf", __func__);
					}
					dpss_total_fill_count_increase();
				//}
			}  else if (flag & V4LVIDEO_FLAG_DI_BYPASS) {
				/*di bypass, need put dec file*/
				vf = (struct vframe_s *)(data->vf_p);
				di_proc_file_print(PRINT_OTHER,
					"%s: di bypass, vf=%px, file=%px\n",
					__func__,
					vf,
					data->file);
				if (vf && data->file)
					dpss_put_file_ext(0, data->file);
				else
					di_proc_file_print(PRINT_ERROR,
						"%s: di bypass, but dec vf/file is null.\n",
						__func__);
			} else {
				di_proc_file_print(PRINT_ERROR, "%s: is not v3 buf\n", __func__);
			}
		}

		if (data->p_ud_param)
			vfree(data->p_ud_param);

		data->p_ud_param = NULL;
		memset(data, 0, sizeof(struct file_private_data));
		vfree(arg);
	}
}

struct file_private_data *dpss_proc_get_file_private_data(struct file *file_vf,
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
		di_proc_file_print(PRINT_ERROR, "%s: Invalid dmabuf.\n", __func__);
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

	file_private_data->p_ud_param = vmalloc(VF_UD_MAX_SIZE);
	if (!file_private_data->p_ud_param) {
		di_proc_file_print(PRINT_ERROR, "%s: alloc UD buf failed.\n", __func__);
		vfree(file_private_data);
		return NULL;
	}

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

