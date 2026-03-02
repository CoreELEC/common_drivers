// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

/* Standard Linux Headers */
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/extcon.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/sched/rt.h>
#include <uapi/linux/sched/types.h>

#include <linux/amlogic/media/codec_mm/codec_mm.h>
#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
#include <linux/amlogic/pixel_probe.h>
#endif
#include <linux/amlogic/aml_ddr_tool.h>

/* Local Headers */
#include "../tvin_format_table.h"
#include "vdin_drv.h"
#include "vdin_ctl.h"
#include "vdin_regs.h"
#include "vdin_afbce.h"
#include "vdin_canvas.h"
#include <asm/div64.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/vout/vout_notify.h>
/*2018-07-18 add debugfs*/
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include "vdin_hdr.h"
#include "vdin_mem_scatter.h"
#include "vdin_hw.h"

#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif

extern unsigned int vdin_game_mode_dly;
static const char vdin_group_name[] = "vdin";

const char *vdin_get_dst_fmt_str(enum vdin_format_convert_e format_convert)
{
	switch (format_convert) {
	case VDIN_FORMAT_CONVERT_YUV_YUV422:
	case VDIN_FORMAT_CONVERT_RGB_YUV422:
	case VDIN_FORMAT_CONVERT_GBR_YUV422:
	case VDIN_FORMAT_CONVERT_BRG_YUV422:
		return "yuv422";
	case VDIN_FORMAT_CONVERT_YUV_YUV444:
	case VDIN_FORMAT_CONVERT_RGB_YUV444:
		return "yuv444";
	case VDIN_FORMAT_CONVERT_YUV_RGB:
	case VDIN_FORMAT_CONVERT_RGB_RGB:
		return "rgb";
	case VDIN_FORMAT_CONVERT_YUV_GBR:
		return "gbr";
	case VDIN_FORMAT_CONVERT_YUV_BRG:
		return "brg";
	case VDIN_FORMAT_CONVERT_YUV_NV12:
	case VDIN_FORMAT_CONVERT_RGB_NV12:
		return "nv12";
	case VDIN_FORMAT_CONVERT_YUV_NV21:
	case VDIN_FORMAT_CONVERT_RGB_NV21:
		return "nv21";
	default:
		return "null";
	}
}

void vdin_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strncat(delim1, delim2, 1);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

enum tvin_port_e vdin_parse_port(char *parm)
{
	enum tvin_port_e port;
	/*parse the port*/
	if (!strcmp(parm, "bt656"))
		port = TVIN_PORT_CAMERA;
	else if (!strcmp(parm, "viu_in"))
		port = TVIN_PORT_VIU1;
	else if (!strcmp(parm, "video"))
		port = TVIN_PORT_VIU1_VIDEO;
	else if (!strcmp(parm, "viu_wb0_vpp") ||
		!strcmp(parm, "vpp"))
		port = TVIN_PORT_VIU1_WB0_VPP;
	else if (!strcmp(parm, "viu_wb0_vd1") ||
		!strcmp(parm, "vd1"))
		port = TVIN_PORT_VIU1_WB0_VD1;
	else if (!strcmp(parm, "viu_wb0_vd2") ||
		!strcmp(parm, "vd2"))
		port = TVIN_PORT_VIU1_WB0_VD2;
	else if (!strcmp(parm, "viu_wb0_osd1") ||
		!strcmp(parm, "osd1"))
		port = TVIN_PORT_VIU1_WB0_OSD1;
	else if (!strcmp(parm, "viu_wb0_osd2") ||
		!strcmp(parm, "osd2"))
		port = TVIN_PORT_VIU1_WB0_OSD2;
	else if (!strcmp(parm, "viu_wb0_post_blend") ||
		!strcmp(parm, "post_blend"))
		port = TVIN_PORT_VIU1_WB0_POST_BLEND;
	else if (!strcmp(parm, "viu_wb1_vpp"))
		port = TVIN_PORT_VIU1_WB1_VPP;
	else if (!strcmp(parm, "viu_wb1_vd1"))
		port = TVIN_PORT_VIU1_WB1_VD1;
	else if (!strcmp(parm, "viu_wb1_vd2"))
		port = TVIN_PORT_VIU1_WB1_VD2;
	else if (!strcmp(parm, "viu_wb1_osd1"))
		port = TVIN_PORT_VIU1_WB1_OSD1;
	else if (!strcmp(parm, "viu_wb0_osd2"))
		port = TVIN_PORT_VIU1_WB1_OSD2;
	else if (!strcmp(parm, "viu_wb1_post_blend"))
		port = TVIN_PORT_VIU1_WB1_POST_BLEND;
	else if (!strcmp(parm, "viu_wb1_vdin_bist"))
		port = TVIN_PORT_VIU1_WB1_VDIN_BIST;
	else if (!strcmp(parm, "viu_in2"))
		port = TVIN_PORT_VIU2;
	else if (!strcmp(parm, "viu2_encl"))
		port = TVIN_PORT_VIU2_ENCL;
	else if (!strcmp(parm, "viu2_enci"))
		port = TVIN_PORT_VIU2_ENCI;
	else if (!strcmp(parm, "viu2_encp"))
		port = TVIN_PORT_VIU2_ENCP;
	else if (!strcmp(parm, "viu2_vd1"))
		port = TVIN_PORT_VIU2_VD1;
	else if (!strcmp(parm, "viu2_osd1"))
		port = TVIN_PORT_VIU2_OSD1;
	else if (!strcmp(parm, "viu2_vpp"))
		port = TVIN_PORT_VIU2_VPP;
	else if (!strcmp(parm, "viu3_vd1"))
		port = TVIN_PORT_VIU3_VD1;
	else if (!strcmp(parm, "viu3_osd1"))
		port = TVIN_PORT_VIU3_OSD1;
	else if (!strcmp(parm, "viu3_vpp"))
		port = TVIN_PORT_VIU3_VPP;
	else if (!strcmp(parm, "venc0"))
		port = TVIN_PORT_VENC0;
	else if (!strcmp(parm, "venc1"))
		port = TVIN_PORT_VENC1;
	else if (!strcmp(parm, "venc2"))
		port = TVIN_PORT_VENC2;
	else if (!strcmp(parm, "isp"))
		port = TVIN_PORT_ISP;
	else
		port = TVIN_PORT_NULL;

	return port;
}

void vdin_get_input_size(struct vdin_dev_s *devp, struct vdin_parm_s *parm)
{
	if (devp->hw_core != VDIN_HW_CORE_LITE)
		return;

	const struct vinfo_s *vinfo;

	vinfo = get_current_vinfo();
	parm->frame_rate = vinfo->std_duration;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int cur_axis[4];
	struct video_input_info video_input_parms;

	memset(&cur_axis, 0, sizeof(cur_axis));
	memset(&video_input_parms, 0, sizeof(struct video_input_info));

	if (vdin_get_video_ready_state(parm->port)) {
		if (parm->port == TVIN_PORT_VIU1_WB0_VD1) {
			get_vdx_real_axis(0, cur_axis);
			parm->h_active = cur_axis[2] - cur_axis[0] + 1;
			parm->v_active = cur_axis[3] - cur_axis[1] + 1;
		} else if (parm->port == TVIN_PORT_VIU2_VD1) {
			get_vdx_real_axis(1, cur_axis);
			parm->h_active = cur_axis[2] - cur_axis[0] + 1;
			parm->v_active = cur_axis[3] - cur_axis[1] + 1;
		} else if (parm->port == TVIN_PORT_VIU1_VIDEO) {
			get_video_input_info(&video_input_parms);
			parm->h_active = video_input_parms.width;
			parm->v_active = video_input_parms.height;
		}
	} else {
#endif
		parm->h_active = vinfo->width;
		parm->v_active = vinfo->height;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	}
#endif
}

ssize_t sig_det_show(struct device *dev,
		     struct device_attribute *attr,
		     char *buf)
{
	int callmaster_status = 0;

	return sprintf(buf, "%d\n", callmaster_status);
}

ssize_t sig_det_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t len)
{
	enum tvin_port_e port = TVIN_PORT_NULL;
	struct tvin_frontend_s *frontend = NULL;
	int callmaster_status = 0;
	long val = 0;

	if (!buf)
		return len;
	/* port = simple_strtol(buf, NULL, 10); */
	if (kstrtol(buf, 10, &val) < 0)
		return -EINVAL;

	port = val;

	frontend = tvin_get_frontend(port, 0);
	if (frontend && frontend->dec_ops &&
	    frontend->dec_ops->callmaster_det) {
		/*call the frontend det function*/
		callmaster_status = frontend->dec_ops->callmaster_det(port,
								      frontend);
		/* pr_info("%d\n",callmaster_status); */
	}

	return len;
}

static DEVICE_ATTR_RW(sig_det);

ssize_t attr_show(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	return len;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static ssize_t dump_vdin_mif_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int ret = 0;
	struct device *dev = kobj_to_dev(kobj);
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct dump_info_s *info;
	u32 sizeimage;
	size_t to_copy = 0;
	unsigned int span = 0;
	u64 map_addr;
	loff_t line_id;
	u32 line_offset;
	bool is_data_end;
	size_t remaining_bytes;

	if (!devp) {
		ret = -1;
		goto error;
	}

	info = &devp->debug.dump_info_mif;

	if (!info->vir_addr && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			ret = -2;
			goto error;
		}

		if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC)
			info->phy_addr = devp->vf_mem_start[0];
		else
			info->phy_addr = devp->mem_start;

		info->highmem_flag = PageHighMem(phys_to_page(info->phy_addr));

		if (vdin_is_convert_to_nv21(devp->format_convert))
			info->height = (devp->canvas_h * 3) / 2;
		else
			info->height = devp->canvas_h;

		info->width = devp->canvas_w;

		info->sizeimage = info->width * info->height;

		if (info->highmem_flag == 0) {
			if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC)
				info->vir_addr = codec_mm_phys_to_virt(info->phy_addr);
			else
				info->vir_addr = phys_to_virt(info->phy_addr);
		}
	}

	sizeimage = info->sizeimage;
	if (off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}

	if (info->highmem_flag) {
		remaining_bytes = min_t(size_t, count, sizeimage - off);
		info->map_size = devp->canvas_w;
		span = devp->canvas_w;
		line_id = div_u64(off, info->map_size);
		div_u64_rem(off, info->map_size, &line_offset);

		while (remaining_bytes) {
			map_addr = info->phy_addr + line_id * info->map_size;
			info->vir_addr = vdin_vmap(map_addr, info->map_size);
			if (!info->vir_addr) {
				mutex_unlock(&info->mutex);
				ret = -3;
				goto error;

			}
			vdin_dma_flush(devp, info->vir_addr, info->map_size, DMA_FROM_DEVICE);
			size_t bytes_this_line = info->map_size - line_offset;

			if (bytes_this_line > remaining_bytes)
				bytes_this_line = remaining_bytes;
			if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
				pr_debug("copy %zu bytes to line id=%lld, map=%llu, offset=%d, off=%lld\n",
						bytes_this_line, line_id,
						info->map_size, line_offset, off);

			memcpy(buf, info->vir_addr + line_offset, bytes_this_line);
			buf += bytes_this_line;
			to_copy += bytes_this_line;
			remaining_bytes -= bytes_this_line;

			vdin_unmap_phyaddr(info->vir_addr);

			if (line_offset + bytes_this_line >= info->map_size) {
				line_id++;
				line_offset = 0;
			} else {
				line_offset += bytes_this_line;
			}
		}
	} else {
		if (!info->vir_addr) {
			mutex_unlock(&info->mutex);
			ret = -4;
			goto error;

		}
		to_copy = min_t(size_t, count, info->sizeimage - off);
		memcpy(buf, info->vir_addr + off, to_copy);
	}

	is_data_end = (off + to_copy) >= info->sizeimage;

	if (is_data_end) {
		pr_info("dump vdin%d mif ok %dx%d(%d) %s %d bit\n",
			devp->index, devp->curr_wr_vfe->vf.width,
			devp->curr_wr_vfe->vf.height, devp->canvas_w,
			vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth_dw);
		if (info->highmem_flag)
			vdin_unmap_phyaddr(info->vir_addr);
		info->vir_addr = NULL;
		mutex_unlock(&info->mutex);
	}

	return to_copy;

error:
	pr_err("dump vdin mif error: %d\n", ret);
	return ret;
}

static ssize_t dump_vdin_sct_body_show(struct kobject *kobj,
			char *buf, loff_t off, size_t count)
{
	int ret = 0;
	struct device *dev = kobj_to_dev(kobj);
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct dump_info_s *info;
	u32 sizeimage;
	size_t to_copy = 0;
	bool is_data_end;
	struct codec_mm_scatter *sc;
	struct vdin_mmu_box *box = NULL;
	size_t sct_page_idx = 0;
	size_t sct_page_offset = 0;

	if (!devp || devp->mem_type != VDIN_MEM_TYPE_SCT) {
		ret = -1;
		goto error;
	}

	info = &devp->debug.dump_info_sct_body;

	if (!info->vir_addr && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			ret = -2;
			goto error;
		}

		box = devp->msct_top.box;
		if (!box) {
			mutex_unlock(&info->mutex);
			ret = -3;
			goto error;
		}

		info->sc = vdin_mmu_box_get_sc_from_idx(box, 0);
		if (!info->sc) {
			mutex_unlock(&info->mutex);
			ret = -4;
			goto error;
		}
		info->sizeimage = devp->msct_top.mmu_4k_number * PAGE_SIZE;
		info->sc_page_cnt = info->sc->page_cnt;
	}

	sizeimage = info->sizeimage;
	sc = info->sc;

	if (off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}

	if (!info->sc) {
		mutex_unlock(&info->mutex);
		ret = -5;
		goto error;
	}

	sct_page_idx = off / PAGE_SIZE;
	sct_page_offset = off % PAGE_SIZE;

	if (sc && sct_page_idx < info->sc_page_cnt) {
		info->phy_addr = vdin_sct_get_page_addr(sc, sct_page_idx);
		info->highmem_flag = PageHighMem(phys_to_page(info->phy_addr));

		if (info->highmem_flag) {
			info->vir_addr = vdin_vmap(info->phy_addr, PAGE_SIZE);
		} else {
			if ((devp->cma_config_flag & 0xfff) == 0x101)
				info->vir_addr = codec_mm_phys_to_virt(info->phy_addr);
			else
				info->vir_addr = phys_to_virt(info->phy_addr);
		}
		if (!info->vir_addr) {
			mutex_unlock(&info->mutex);
			ret = -6;
			goto error;
		}

		vdin_dma_flush(devp, info->vir_addr, PAGE_SIZE, DMA_FROM_DEVICE);

		to_copy = min_t(size_t, count, PAGE_SIZE - sct_page_offset);
		memcpy(buf, info->vir_addr + sct_page_offset, to_copy);

		if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
			pr_info("sct page id:%zu (%u %d), paddr:0x%lx, mem:%d",
				sct_page_idx, info->sc_page_cnt, devp->msct_top.mmu_4k_number,
				(unsigned long)info->phy_addr, info->highmem_flag);

		if (info->highmem_flag)
			vdin_unmap_phyaddr(info->vir_addr);
	}

	if (sc && sct_page_idx >= info->sc_page_cnt &&
		sct_page_idx < devp->msct_top.mmu_4k_number) {
		if (!info->vir_addr) {
			mutex_unlock(&info->mutex);
			ret = -7;
			goto error;
		}

		to_copy = min_t(size_t, count, PAGE_SIZE - sct_page_offset);
		memset(buf, 0, to_copy);

		if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
			pr_info("sct 4k page id:%zu (%u %d), paddr:0x%lx, mem:%d",
				sct_page_idx, info->sc_page_cnt,  devp->msct_top.mmu_4k_number,
				(unsigned long)info->phy_addr, info->highmem_flag);
	}

	is_data_end = (off + to_copy) >= info->sizeimage;

	if (is_data_end) {
		pr_info("dump vdin%d sct body ok %dx%d %s %d bit addr:0x0 sct:%d\n",
			devp->index, devp->h_active, devp->v_active,
			vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth,
			devp->mem_type);
		info->vir_addr = NULL;
		mutex_unlock(&info->mutex);
	}

	return to_copy;
error:
	pr_err("dump vdin sct body error: %d\n", ret);
	return ret;
}

static ssize_t dump_vdin_afbc_head_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int ret = 0;
	struct device *dev = kobj_to_dev(kobj);
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct dump_info_s *info;
	u32 sizeimage;
	u8 *vir_addr;
	size_t to_copy;

	if (!devp || !devp->double_wr) {
		ret = -1;
		goto error;
	}

	info = &devp->debug.dump_info_afbc_head;

	if (!info->vir_addr && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			ret = -2;
			goto error;
		}

		info->phy_addr = devp->afbce_info->fm_head_paddr[0];
		info->sizeimage = devp->afbce_info->frame_head_size;

		info->highmem_flag =
				PageHighMem(phys_to_page(devp->afbce_info->fm_body_paddr[0]));

		if (info->highmem_flag == 0) {
			if (devp->cma_config_flag & MEM_ALLOC_FROM_CODEC)
				info->vir_addr = codec_mm_phys_to_virt(info->phy_addr);
			else
				info->vir_addr = phys_to_virt(info->phy_addr);
		} else {
			info->vir_addr = vdin_vmap(info->phy_addr, info->sizeimage);
		}

		if (!info->vir_addr) {
			mutex_unlock(&info->mutex);
			ret = -3;
			goto error;

		}
	}

	sizeimage = info->sizeimage;
	vir_addr = info->vir_addr;

	if (off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}

	if (!vir_addr) {
		mutex_unlock(&info->mutex);
		ret = -4;
		goto error;
	}

	vdin_dma_flush(devp, vir_addr, sizeimage, DMA_FROM_DEVICE);
	to_copy = min_t(size_t, count, sizeimage - off);
	memcpy(buf, vir_addr + off, to_copy);

	if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
		pr_debug("%s() copy %zu bytes to off:%lld\n", __func__, to_copy, off);

	if ((off + to_copy) >= sizeimage) {
		pr_info("dump vdin%d afbc head ok\n", devp->index);
		if (info->highmem_flag)
			vdin_unmap_phyaddr(vir_addr);
		info->vir_addr = NULL;
		mutex_unlock(&info->mutex);
	}

	return to_copy;
error:
	pr_err("dump vdin afbc head error: %d\n", ret);
	return ret;
}

static ssize_t dump_vdin_afbc_table_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int ret = 0;
	struct device *dev = kobj_to_dev(kobj);
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct dump_info_s *info;
	u32 sizeimage;
	u8 *vir_addr;
	size_t to_copy;

	if (!devp || !devp->double_wr) {
		ret = -1;
		goto error;
	}

	info = &devp->debug.dump_info_afbc_table;

	if (!info->vir_addr && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			ret = -2;
			goto error;
		}

		info->phy_addr = devp->afbce_info->fm_table_paddr[0];
		info->sizeimage = devp->afbce_info->frame_table_size;

		info->highmem_flag =
				PageHighMem(phys_to_page(devp->afbce_info->fm_body_paddr[0]));

		if (info->highmem_flag == 0) {
			if ((devp->cma_config_flag & 0xfff) == 0x101)
				info->vir_addr = codec_mm_phys_to_virt(info->phy_addr);
			else
				info->vir_addr = phys_to_virt(info->phy_addr);
		} else {
			info->vir_addr = vdin_vmap(info->phy_addr, info->sizeimage);
		}

		if (!info->vir_addr) {
			mutex_unlock(&info->mutex);
			ret = -3;
			goto error;

		}
	}

	sizeimage = info->sizeimage;
	vir_addr = info->vir_addr;

	if (off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}

	if (!vir_addr) {
		mutex_unlock(&info->mutex);
		ret = -4;
		goto error;
	}

	vdin_dma_flush(devp, vir_addr, sizeimage, DMA_FROM_DEVICE);
	to_copy = min_t(size_t, count, sizeimage - off);
	memcpy(buf, vir_addr + off, to_copy);

	if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
		pr_debug("%s() copy %zu bytes to off:%lld\n", __func__, to_copy, off);

	if ((off + to_copy) >= sizeimage) {
		pr_info("dump vdin%d afbc table ok\n", devp->index);
		if (info->highmem_flag)
			vdin_unmap_phyaddr(vir_addr);
		info->vir_addr = NULL;
		mutex_unlock(&info->mutex);
	}

	return to_copy;
error:
	pr_err("dump vdin afbc table error: %d\n", ret);
	return ret;
}

static ssize_t dump_vdin_afbc_body_show(struct file *filp, struct kobject *kobj,
			 struct bin_attribute *attr, char *buf, loff_t off,
			 size_t count)
{
	int ret = 0;
	struct device *dev = kobj_to_dev(kobj);
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct dump_info_s *info;
	u32 sizeimage;
	u8 *vir_addr;
	size_t to_copy = 0;
	u64 map_addr;
	unsigned int span = SZ_1M;
	loff_t block_idx = off / span;
	loff_t block_offset = off % span;
	bool is_block_end;
	bool is_data_end;

	if (!devp || !devp->double_wr) {
		ret = -1;
		goto error;
	}

	info = &devp->debug.dump_info_afbc_body;

	if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
		info->sizeimage = devp->msct_top.mmu_4k_number * PAGE_SIZE;
		to_copy = dump_vdin_sct_body_show(kobj, buf, off, count);
		return to_copy;
	}

	if (!info->vir_addr && off == 0) {
		ret = mutex_lock_interruptible(&info->mutex);
		if (ret < 0) {
			ret = -2;
			goto error;
		}

		info->phy_addr = devp->afbce_info->fm_body_paddr[0];
		info->sizeimage = devp->afbce_info->frame_body_size;
		info->highmem_flag = PageHighMem(phys_to_page(info->phy_addr));
		if (info->highmem_flag == 0) {
			if ((devp->cma_config_flag & 0xfff) == 0x101)
				info->vir_addr = codec_mm_phys_to_virt(info->phy_addr);
			else
				info->vir_addr = phys_to_virt(info->phy_addr);
		} else {
			info->remain_span = devp->afbce_info->frame_body_size % PAGE_ALIGN(span);

			if (info->remain_span)
				info->remain_map_size =
					devp->afbce_info->frame_body_size - info->remain_span;

			if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
				pr_debug("high mem area, body_size:%d - remain:%d = remain_size:%d\n",
					devp->afbce_info->frame_body_size,
					info->remain_span, info->remain_map_size);

			info->map_addr = info->phy_addr;
			info->map_size = span;
			info->vir_addr = vdin_vmap(info->map_addr, info->map_size);
			if (!info->vir_addr) {
				mutex_unlock(&info->mutex);
				ret = -3;
				goto error;
			}
			vdin_dma_flush(devp, info->vir_addr, info->map_size, DMA_FROM_DEVICE);
		}
	}

	sizeimage = info->sizeimage;
	if (off >= sizeimage) {
		mutex_unlock(&info->mutex);
		return 0;
	}

	vir_addr = info->vir_addr;
	if (!vir_addr) {
		mutex_unlock(&info->mutex);
		ret = -4;
		goto error;
	}

	if (info->highmem_flag) {
		map_addr = info->phy_addr + (block_idx * span);

		if (info->remain_span &&
			(map_addr >= info->phy_addr + info->remain_map_size))
			info->map_size = info->remain_span;
		else
			info->map_size = span;

		if (map_addr != info->map_addr) {
			info->map_addr = map_addr;
			info->vir_addr = vdin_vmap(info->map_addr, info->map_size);
			if (!info->vir_addr) {
				mutex_unlock(&info->mutex);
				ret = -5;
				goto error;
			}
			vdin_dma_flush(devp, info->vir_addr, info->map_size, DMA_FROM_DEVICE);
			if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
				pr_debug("Remapping: block_idx=%lld, map_size=%llu\n",
					block_idx, info->map_size);
		}

		to_copy = min_t(size_t, count, info->map_size);
		memcpy(buf, info->vir_addr + block_offset, to_copy);

		is_block_end = (block_offset + to_copy) >= span;
		if (devp->debug.vdin_dbg_en & DBG_VDIN_DUMP)
			pr_debug("Freeing mapping: block_idx=%lld, is_block_end=%d\n",
				block_idx, is_block_end);
		if (is_block_end)
			vdin_unmap_phyaddr(info->vir_addr);
	} else {
		to_copy = min_t(size_t, count, info->sizeimage - off);
		memcpy(buf, info->vir_addr + off, to_copy);
	}

	is_data_end = (off + to_copy) >= info->sizeimage;

	if (is_data_end) {
		pr_info("dump vdin%d afbc body ok %dx%d %s %d bit afbc:0x%x 0x%lx\n",
			devp->index, devp->h_active, devp->v_active,
			vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth,
			devp->afbce_flag, devp->afbce_info->fm_body_paddr[0]);
		if (info->highmem_flag)
			vdin_unmap_phyaddr(info->vir_addr);
		info->vir_addr = NULL;
		mutex_unlock(&info->mutex);
	}

	return to_copy;
error:
	pr_err("dump vdin afbc body error: %d\n", ret);
	return ret;
}

static struct bin_attribute vdin_attr[] = {
	{
		.attr.name = "dump_mif",
		.attr.mode = 0664,
		.private = NULL,
		.read = dump_vdin_mif_show,
		.write = NULL,
	},
	{
		.attr.name = "dump_afbc_head",
		.attr.mode = 0664,
		.private = NULL,
		.read = dump_vdin_afbc_head_show,
		.write = NULL,
	},
	{
		.attr.name = "dump_afbc_table",
		.attr.mode = 0664,
		.private = NULL,
		.read = dump_vdin_afbc_table_show,
		.write = NULL,
	},
	{
		.attr.name = "dump_afbc_body",
		.attr.mode = 0664,
		.private = NULL,
		.read = dump_vdin_afbc_body_show,
		.write = NULL,
	},
};

static struct bin_attribute *vdin_bin_attrs[] = {
	&vdin_attr[0],
	&vdin_attr[1],
	&vdin_attr[2],
	&vdin_attr[3],
	NULL,
};

static const struct attribute_group vdin_attr_group[VDIN_MAX_INSTANCE] = {
	{
		.name = vdin_group_name,
		.bin_attrs = vdin_bin_attrs,
	},
};

#endif

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void vdin_dump_write_sct_mem_user(struct vdin_dev_s *devp,
									unsigned int buf_num,
									void *output_buffer,
									size_t buffer_size)
{
	int i;
	int highmem_flag = 0;
	phy_addr_type phy_addr;
	struct codec_mm_scatter *sc;
	void *buf_body = NULL;
	struct vdin_mmu_box *box = NULL;
	size_t offset = 0;
	int ret = 0;

	box = devp->msct_top.box;
	if (!box) {
		ret = -1;
		goto error;
	}

	mutex_lock(&box->mutex);
	sc = vdin_mmu_box_get_sc_from_idx(box, buf_num);
	if (!sc) {
		mutex_unlock(&box->mutex);
		ret = -2;
		goto error;
	}

	for (i = 0; i < sc->page_cnt; i++) {
		phy_addr = vdin_sct_get_page_addr(sc, i);
		if (!phy_addr) {
			pr_info("[%d] phy_addr == 0\n", i);
			continue;
		}

		highmem_flag = PageHighMem(phys_to_page(phy_addr));
		if (highmem_flag == 0) {
			if ((devp->cma_config_flag & 0xfff) == 0x101)
				buf_body = codec_mm_phys_to_virt(phy_addr);
			else if ((devp->cma_config_flag & 0xfff) == 0)
				buf_body = phys_to_virt(phy_addr);

			pr_debug("[%d] Low phy_addr=0x%lx,buf_body=0x%lx,page:0x%lx\n", i,
					 phy_addr, (unsigned long)buf_body, PAGE_SIZE);

			if (offset + PAGE_SIZE > buffer_size) {
				pr_err("Buffer overflow at index %d\n", i);
				break;
			}
			if (!buf_body) {
				mutex_unlock(&box->mutex);
				ret = -3;
				goto error;
			}
			vdin_dma_flush(devp, buf_body, PAGE_SIZE, DMA_FROM_DEVICE);
			memcpy(output_buffer + offset, buf_body, PAGE_SIZE);
			offset += PAGE_SIZE;
		} else {
			buf_body = vdin_vmap(phy_addr, PAGE_SIZE);
			if (!buf_body) {
				mutex_unlock(&box->mutex);
				ret = -4;
				goto error;
			}

			pr_debug("[%d] High phy_addr=0x%lx,buf_body=0x%lx,page:0x%lx\n", i,
					 phy_addr, (unsigned long)buf_body, PAGE_SIZE);

			if (offset + PAGE_SIZE > buffer_size) {
				pr_err("Buffer overflow at index %d\n", i);
				vdin_unmap_phyaddr(buf_body);
				break;
			}

			vdin_dma_flush(devp, buf_body, PAGE_SIZE, DMA_FROM_DEVICE);
			memcpy(output_buffer + offset, buf_body, PAGE_SIZE);
			offset += PAGE_SIZE;
			vdin_unmap_phyaddr(buf_body);
		}
	}

	pr_info("[%d] page_cnt=%d,mmu_4k_number:%d\n", i,
			sc->page_cnt, devp->msct_top.mmu_4k_number);

	if (highmem_flag == 0) {
		for (i = sc->page_cnt; i < devp->msct_top.mmu_4k_number; i++) {
			if (offset + PAGE_SIZE > buffer_size) {
				break;
			}
			if (!buf_body) {
				mutex_unlock(&box->mutex);
				ret = -7;
				goto error;
			}
			pr_debug("[%d] Low phy_addr=0x%lx,buf_body=0x%lx,page:0x%lx\n", i,
					 phy_addr, (unsigned long)buf_body, PAGE_SIZE);
			memcpy(output_buffer + offset, buf_body, PAGE_SIZE);
			offset += PAGE_SIZE;
		}
	}

	mutex_unlock(&box->mutex);
	return;

error:
	pr_info("user dump sct mem error: %d\n", ret);
	return;
}

void vdin_dump_one_afbce_mem_user(void *output_head_ptr, void *output_table_ptr,
void *output_body_ptr, struct vdin_dev_s *devp, unsigned int buf_num)
{
	#define K_PATH_BUFF_LENGTH	128
	loff_t pos = 0;
	void *buf_head = NULL;
	void *buf_table = NULL;
	void *buf_body = NULL;
	unsigned long high_addr;
	unsigned long phys;
	int highmem_flag = 0;
	unsigned int span = 0;
	unsigned int remain = 0;
	unsigned int count = 0;
	unsigned long j = 0;
	void *virt_buf = NULL;
	int ret = 0;

	if (buf_num >= devp->canvas_max_num) {
		ret = -1;
		goto error;
	}

	if ((devp->cma_config_flag & 0x1) &&
	    devp->cma_mem_alloc == 0) {
		ret = -2;
		goto error;
	}

	highmem_flag = PageHighMem(phys_to_page(devp->afbce_info->fm_body_paddr[0]));

	if (highmem_flag == 0) {
		/*low mem area*/
		if ((devp->cma_config_flag & 0xfff) == 0x101) {
			buf_head =
			codec_mm_phys_to_virt(devp->afbce_info->fm_head_paddr[buf_num]);
			buf_table =
			codec_mm_phys_to_virt(devp->afbce_info->fm_table_paddr[buf_num]);
			buf_body =
			codec_mm_phys_to_virt(devp->afbce_info->fm_body_paddr[buf_num]);
		} else if (devp->cma_config_flag == 0) {
			buf_head =
				phys_to_virt(devp->afbce_info->fm_head_paddr[buf_num]);
			buf_table =
				phys_to_virt(devp->afbce_info->fm_table_paddr[buf_num]);
			buf_body =
				phys_to_virt(devp->afbce_info->fm_body_paddr[buf_num]);
		}
	} else {
		/*high mem area*/
		buf_head = vdin_vmap(devp->afbce_info->fm_head_paddr[buf_num],
				     devp->afbce_info->frame_head_size);

		buf_table = vdin_vmap(devp->afbce_info->fm_table_paddr[buf_num],
				      devp->afbce_info->frame_table_size);
	}
	/*write header bin*/
	if (!buf_head) {
		ret = -3;
		goto error;
	}

	pr_debug("highmem=%d head_paddr=0x%lx,table_paddr=0x%lx,body_paddr=0x%lx\n",
		highmem_flag, devp->afbce_info->fm_head_paddr[buf_num],
		(devp->afbce_info->fm_table_paddr[buf_num]),
		devp->afbce_info->fm_body_paddr[buf_num]);

	vdin_dma_flush(devp, buf_head,
		       devp->afbce_info->frame_head_size,
		       DMA_FROM_DEVICE);
	memcpy(output_head_ptr, buf_head, devp->afbce_info->frame_head_size);
	//kernel_write(filp, buf_head, devp->afbce_info->frame_head_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(buf_head);

	if (!buf_table) {
		ret = -4;
		goto error;
	}
	/*write table bin*/
	pos = 0;
	vdin_dma_flush(devp, buf_table,
		       devp->afbce_info->frame_table_size,
		       DMA_FROM_DEVICE);
	memcpy(output_table_ptr, buf_table, devp->afbce_info->frame_table_size);
	//kernel_write(filp, buf_table, devp->afbce_info->frame_table_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(buf_table);

	/*write body bin*/
	pos = 0;
	//strcpy(buff, path);
	if (highmem_flag == 0) {
		if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
			vdin_dump_write_sct_mem_user(devp, buf_num, output_body_ptr,
				devp->afbce_info->frame_body_size * 2);
		} else {
			//kernel_write(filp, buf_body,
				//devp->afbce_info->frame_body_size, &pos);
			if (buf_body)
				memcpy(output_body_ptr, buf_body,
				devp->afbce_info->frame_body_size);
		}
	} else {
		span = SZ_1M;
		count = devp->afbce_info->frame_body_size / PAGE_ALIGN(span);
		remain = devp->afbce_info->frame_body_size % PAGE_ALIGN(span);
		phys = devp->afbce_info->fm_body_paddr[buf_num];

		for (j = 0; j < count; j++) {
			high_addr = phys + j * span;
			virt_buf = vdin_vmap(high_addr, span);
			if (!virt_buf) {
				ret = -5;
				goto error;
			}

			vdin_dma_flush(devp, virt_buf, span, DMA_FROM_DEVICE);
			memcpy(output_body_ptr + j * span, virt_buf, span);
			vdin_unmap_phyaddr(virt_buf);
		}

		if (remain) {
			span = devp->afbce_info->frame_body_size - remain;
			high_addr = phys + span;
			virt_buf = vdin_vmap(high_addr, remain);
			if (!virt_buf) {
				ret = -6;
				goto error;
			}

			vdin_dma_flush(devp, virt_buf, remain, DMA_FROM_DEVICE);
			memcpy(output_body_ptr + span, virt_buf, remain);
			vdin_unmap_phyaddr(virt_buf);
		}
	}
	pr_info("user dump vdin%d afbc buffer ok %dx%d %s %d bit afbc:0x%x 0x%lx %d sct:%d\n",
		devp->index, devp->h_active, devp->v_active,
		vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth,
		devp->afbce_flag, devp->afbce_info->fm_body_paddr[0],
		devp->afbce_info->frame_body_size, devp->mem_type);
	return;
error:
	pr_info("user dump afbc buffer error: %d\n", ret);
	return;
}

int vdin_dump_one_buf_mem_user(void *output_buf, struct vdin_dev_s *devp,
									unsigned int buf_num)
{
	void *buf = NULL;
	unsigned int i, offset = 0;
	unsigned long j;
	unsigned int span = 0, count = 0;
	int highmem_flag;
	unsigned long high_addr;
	unsigned long phys;
	char *output_ptr = (char *)output_buf;
	size_t written_bytes = 0;
	int ret = 0;

	if (devp->mem_protected) {
		ret = -1;
		goto error;
	}

	if ((devp->cma_config_flag & 0x1) &&
		devp->cma_mem_alloc == 0) {
		ret = -2;
		goto error;
	}

	if (buf_num >= devp->canvas_max_num) {
		ret = -3;
		goto error;
	}

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vf_mem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));

	if (vdin_is_convert_to_nv21(devp->format_convert))
		count = (devp->canvas_h * 3) / 2;
	else
		count = devp->canvas_h;

	if (highmem_flag == 0) {
		pr_debug("low mem area: one line size (%d, active:%d),vf_mem_start[%d]:%lx\n",
				devp->canvas_max_stride, devp->canvas_active_w, buf_num,
				devp->vf_mem_start[buf_num]);
		if (devp->cma_config_flag & 0x1)
			buf = codec_mm_phys_to_virt(devp->vf_mem_start[buf_num]);
		else
			buf = phys_to_virt(devp->vf_mem_start[buf_num]);
		/*only write active data*/
		for (i = 0; i < count; i++) {
			vdin_dma_flush(devp, buf, devp->canvas_w,
						   DMA_FROM_DEVICE);
			memcpy(output_ptr, buf, devp->canvas_active_w);
			output_ptr += devp->canvas_active_w;
			written_bytes += devp->canvas_active_w;
			offset = devp->canvas_active_w % 8;
			if (offset && written_bytes) {
				output_ptr -= offset;
				memcpy(output_ptr,
					buf + devp->canvas_active_w + 8 - 2 * offset, offset);
				output_ptr += offset;
			}
			buf += devp->canvas_w;
		}
		pr_debug("write buffer %2d of %2u  to output buffer.offset=%d\n",
				buf_num, devp->canvas_max_num, offset);
	} else {
		pr_debug("high mem area: one line size (%d, active:%d)\n",
				devp->canvas_max_stride, devp->canvas_active_w);
		span = devp->canvas_active_w;
		phys = devp->vf_mem_start[buf_num];

		for (j = 0; j < count; j++) {
			high_addr = phys + j * devp->canvas_w;
			buf = vdin_vmap(high_addr, span);
			if (!buf) {
				pr_info("vdin_vmap error\n");
				return written_bytes;
			}

			vdin_dma_flush(devp, buf, span, DMA_FROM_DEVICE);
			memcpy(output_ptr, buf, span);
			output_ptr += span;
			written_bytes += span;
			vdin_unmap_phyaddr(buf);
		}
		pr_debug("high-mem write buffer %2d of %2u to output buffer.\n",
				buf_num, devp->canvas_max_num);
	}

	pr_info("user dump vdin%d mif ok %dx%d(%d) %s %d bit\n",
		devp->index, devp->curr_wr_vfe->vf.width,
		devp->curr_wr_vfe->vf.height, devp->canvas_w,
		vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth_dw);

	return written_bytes;

error:
	pr_info("user dump mif buffer error: %d\n", ret);
	return ret;
}
#endif

#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void vdin_dump_write_sct_mem(struct vdin_dev_s *devp,
				    unsigned int buf_num, struct file *filp)
{
	int i;
	int highmem_flag = 0;
	phy_addr_type phy_addr;
	struct codec_mm_scatter *sc;
	loff_t pos = 0;
	void *buf_body = NULL;
	struct vdin_mmu_box *box = NULL;
	int ret = 0;

	box = devp->msct_top.box;
	if (!box) {
		ret = -1;
		goto error;
	}

	mutex_lock(&box->mutex);
	sc = vdin_mmu_box_get_sc_from_idx(box, buf_num);
	if (!sc) {
		ret = -2;
		goto error;
	}
	for (i = 0; i < sc->page_cnt; i++) {//devp->msct_top.mmu_4k_number
		phy_addr = vdin_sct_get_page_addr(sc, i);
		if (!phy_addr) {
			pr_info("[%d] phy_addr == 0\n", i);
			continue;
		}
		highmem_flag = PageHighMem(phys_to_page(phy_addr));
		if (highmem_flag == 0) {
			if ((devp->cma_config_flag & 0xfff) == 0x101)
				buf_body = codec_mm_phys_to_virt(phy_addr);
			else if ((devp->cma_config_flag & 0xfff) == 0)
				buf_body = phys_to_virt(phy_addr);
			pr_debug("[%d] Low phy_addr=0x%lx,buf_body=0x%lx,page:0x%lx\n", i,
				phy_addr, (unsigned long)buf_body, PAGE_SIZE);
			vdin_dma_flush(devp, buf_body, PAGE_SIZE,
				       DMA_FROM_DEVICE);
			kernel_write(filp, buf_body, PAGE_SIZE, &pos);
		} else {
			buf_body = vdin_vmap(phy_addr, PAGE_SIZE);
			if (!buf_body) {
				ret = -3;
				goto error;
			}
			pr_debug("[%d] High phy_addr=0x%lx,buf_body=0x%lx,page:0x%lx\n", i,
				phy_addr, (unsigned long)buf_body, PAGE_SIZE);
			vdin_dma_flush(devp, buf_body, PAGE_SIZE, DMA_FROM_DEVICE);
			kernel_write(filp, buf_body, PAGE_SIZE, &pos);
			vdin_unmap_phyaddr(buf_body);
		}
	}
	pr_info("[%d] page_cnt=%d,mmu_4k_number:%d\n", i,
		sc->page_cnt, devp->msct_top.mmu_4k_number);
	if (highmem_flag == 0) {
		for (i = sc->page_cnt; i < devp->msct_top.mmu_4k_number; i++) {
			pr_debug("[%d] Low phy_addr=0x%lx,buf_body=0x%lx,page:0x%lx\n", i,
				phy_addr, (unsigned long)buf_body, PAGE_SIZE);
			kernel_write(filp, buf_body, PAGE_SIZE, &pos);
		}
	}

	mutex_unlock(&box->mutex);
	return;
error:
	pr_info("kernel dump sct buffer error: %d\n", ret);
	return;
}
#endif
#endif
static void vdin_dump_more_mem(char *path, struct vdin_dev_s *devp,
				unsigned int dump_num, int sel_start)
{
#if IS_ENABLED(CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA)
	struct file *filp = NULL;
	loff_t pos = 0;
	loff_t mem_size = 0;
	void *buf = NULL;
	unsigned int i;
	int k;
	unsigned int count = 0;
	unsigned long pre_put_frames;
	unsigned int *rec_dum_frame = NULL;
	unsigned long phys;
	unsigned long high_addr;
	ulong vf_phy_addr;
	int highmem_flag;
	unsigned int dump_frame_count = dump_num;
	int ret = 0;

	if (devp->mem_protected) {
		ret = -1;
		goto error;
	}

	if (devp->cma_mem_alloc == 0) {
		ret = -2;
		goto error;
	}

	filp = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		ret = -3;
		goto error;
	}

	rec_dum_frame = kzalloc(sizeof(unsigned int) * dump_frame_count, GFP_KERNEL);
	if (!rec_dum_frame) {
		filp_close(filp, NULL);
		ret = -4;
		goto error;
	}

	if (vdin_is_convert_to_nv21(devp->format_convert))
		count = (devp->canvas_h * 3) / 2;
	else
		count = devp->canvas_h;
	mem_size = (loff_t)devp->canvas_active_w * count;

	if (sel_start) {
		while (!(devp->flags & VDIN_FLAG_DEC_STARTED) ||
			devp->frame_cnt > 1)
			usleep_range(5000, 6000);
	}

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vf_mem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));

	pre_put_frames = devp->put_frame_cnt;
	if (highmem_flag == 0) {
		/*low mem area*/
		for (k = 0; k < dump_frame_count; k++) {
			if (pre_put_frames == devp->put_frame_cnt ||
			    !devp->put_frame_cnt) {
				k--;
				usleep_range(5, 8);
				continue;
			}
			pos = mem_size * k;
			pre_put_frames = devp->put_frame_cnt;
			rec_dum_frame[k] = pre_put_frames;
			if (devp->baddr_en)
				vf_phy_addr =
					devp->vfp->last_last_vfe->vf.canvas0_config[0].phy_addr;
			else
				vf_phy_addr =
				canvas_get_addr(devp->vfp->last_last_vfe->vf.canvas0Addr & 0xff);
			if ((devp->cma_config_flag & 0xfff) == 0x1)
				buf = codec_mm_phys_to_virt(vf_phy_addr);
			else if ((devp->cma_config_flag & 0xfff) == 0x101)
				buf = codec_mm_phys_to_virt(vf_phy_addr);
			else if ((devp->cma_config_flag & 0xfff) == 0x100)
				buf = phys_to_virt(vf_phy_addr);
			else
				buf = phys_to_virt(vf_phy_addr);

			/*only write active data*/
			for (i = 0; i < count; i++) {
				vdin_dma_flush(devp, buf, devp->canvas_w, DMA_FROM_DEVICE);
				kernel_write(filp, buf, devp->canvas_active_w, &pos);
				buf += devp->canvas_w;
			}
		}
	} else {
		/*high mem area*/
		for (k = 0; k < dump_frame_count; k++) {
			if (pre_put_frames == devp->put_frame_cnt ||
			    !devp->put_frame_cnt) {
				k--;
				usleep_range(5, 8);
				continue;
			}
			pos = mem_size * k;
			pre_put_frames = devp->put_frame_cnt;
			rec_dum_frame[k] = pre_put_frames;
			if (devp->baddr_en)
				phys = devp->vfp->last_last_vfe->vf.canvas0_config[0].phy_addr;
			else
				phys =
				canvas_get_addr(devp->vfp->last_last_vfe->vf.canvas0Addr & 0xff);
			for (i = 0; i < count; i++) {
				high_addr = phys + i * devp->canvas_w;
				buf = vdin_vmap(high_addr, devp->canvas_active_w);
				if (!buf) {
					filp_close(filp, NULL);
					kfree(rec_dum_frame);
					ret = -5;
					goto error;
				}

				vdin_dma_flush(devp, buf, devp->canvas_active_w, DMA_FROM_DEVICE);
				kernel_write(filp, buf, devp->canvas_active_w, &pos);
				vdin_unmap_phyaddr(buf);
			}
		}
	}

	if ((rec_dum_frame[dump_frame_count - 1] - rec_dum_frame[0] + 1) == dump_num)
		pr_info("kernel dump %u buffer start:%u end:%u continuous ok\n", dump_num,
			rec_dum_frame[0], rec_dum_frame[dump_frame_count - 1]);

	filp_close(filp, NULL);
	kfree(rec_dum_frame);
	rec_dum_frame = NULL;
	return;
error:
	pr_info("kernel dump more buffer error: %d\n", ret);
	return;
#endif
}

static void vdin_dump_one_buf_mem(char *path, struct vdin_dev_s *devp,
				  unsigned int buf_num)
{
#if IS_ENABLED(CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA)
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	unsigned int i, j, offset = 0;
	unsigned int span = 0, count = 0;
	int highmem_flag;
	unsigned long high_addr;
	unsigned long phys;
	int ret = 0;

	if (devp->mem_protected) {
		ret = -1;
		goto error;
	}

	if ((devp->cma_config_flag & 0x1) &&
	    devp->cma_mem_alloc == 0) {
		ret = -2;
		goto error;
	}
	pr_info("cma_config_flag:0x%x\n", devp->cma_config_flag);
	filp = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		ret = -3;
		goto error;
	}

	if (buf_num >= devp->canvas_max_num) {
		filp_close(filp, NULL);
		ret = -4;
		goto error;
	}

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vf_mem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));

	if (vdin_is_convert_to_422(devp->format_convert) &&
		(devp->dtdata->hw_ver == VDIN_HW_T6W || devp->dtdata->hw_ver == VDIN_HW_T6X) &&
		devp->yuv422_2plane_en)
		count = devp->canvas_h * 2;
	else if (vdin_is_convert_to_nv21(devp->format_convert))
		count = (devp->canvas_h * 3) / 2;
	else
		count = devp->canvas_h;

	if (highmem_flag == 0) {
		pr_debug("low mem area: one line size (%d, active:%d),vf_mem_start[%d]:%lx\n",
			devp->canvas_w, devp->canvas_active_w, buf_num,
			devp->vf_mem_start[buf_num]);
		if (devp->cma_config_flag & 0x1)
			buf = codec_mm_phys_to_virt(devp->vf_mem_start[buf_num]);
		else
			buf = phys_to_virt(devp->vf_mem_start[buf_num]);
		/*only write active data*/
		for (i = 0; i < count; i++) {
			vdin_dma_flush(devp, buf, devp->canvas_w,
				       DMA_FROM_DEVICE);
			kernel_write(filp, buf, devp->canvas_active_w, &pos);
			offset = devp->canvas_active_w % 8;
			if (offset && pos) {
				pos = pos - offset;
				kernel_write(filp, buf + devp->canvas_active_w + 8 - 2 * offset,
					offset, &pos);
			}
			buf += devp->canvas_w;
		}
		/*kernel_write(filp, buf, devp->canvas_max_size, &pos);*/
		pr_debug("write buffer %2d of %2u  to %s.offset=%d\n",
			buf_num, devp->canvas_max_num, path, offset);
	} else {
		pr_debug("high mem area: one line size (%d, active:%d)\n",
			devp->canvas_w, devp->canvas_active_w);
		span = devp->canvas_active_w;
		phys = devp->vf_mem_start[buf_num];

		for (j = 0; j < count; j++) {
			high_addr = phys + j * devp->canvas_w;
			buf = vdin_vmap(high_addr, span);
			if (!buf) {
				filp_close(filp, NULL);
				pr_info("vdin_vmap error\n");
				return;
			}

			vdin_dma_flush(devp, buf, span, DMA_FROM_DEVICE);
			kernel_write(filp, buf, span, &pos);
			offset = devp->canvas_active_w % 8;
			if (offset && pos) {
				pos = pos - offset;
				kernel_write(filp, buf + devp->canvas_active_w + 8 - 2 * offset,
					offset, &pos);
			}
			vdin_unmap_phyaddr(buf);
		}
		pr_debug("high-mem write buffer %2d of %2u to %s.\n",
			buf_num, devp->canvas_max_num, path);
	}
	pr_info("kernel dump one vdin%d mif ok %dx%d(%d) %s %d bit\n",
			devp->index, devp->curr_wr_vfe->vf.width,
			devp->curr_wr_vfe->vf.height, devp->canvas_w,
			vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth_dw);
	filp_close(filp, NULL);
	return;
error:
	pr_info("kernel dump one buffer error: %d\n", ret);
	return;
#endif
}

static void vdin_dump_mem(char *path, struct vdin_dev_s *devp)
{
#if IS_ENABLED(CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA)
	struct file *filp = NULL;
	loff_t pos = 0;
	loff_t mem_size = 0;
	unsigned int i = 0, j = 0;
	unsigned int span = 0;
	unsigned int count = 0;
	unsigned long high_addr;
	unsigned long phys;
	int highmem_flag;
	void *buf = NULL;
	void *vfbuf[VDIN_CANVAS_MAX_CNT];
	int ret = 0;

	if (devp->mem_protected) {
		ret = -1;
		goto error;
	}

	if (devp->cma_mem_alloc == 0) {
		ret = -2;
		goto error;
	}

	filp = filp_open(path, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		ret = -3;
		goto error;
	}

	if (vdin_is_convert_to_nv21(devp->format_convert))
		count = (devp->canvas_h * 3) / 2;
	else
		count = devp->canvas_h;

	mem_size = (loff_t)devp->canvas_active_w * count;

	for (i = 0; i < VDIN_CANVAS_MAX_CNT; i++)
		vfbuf[i] = NULL;

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vf_mem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));

	if (highmem_flag == 0) {
		/*low mem area*/
		pr_debug("low mem area: one line size (%d, active:%d)\n",
			devp->canvas_w, devp->canvas_active_w);
		for (i = 0; i < devp->canvas_max_num; i++) {
			pos = mem_size * i;
			if ((devp->cma_config_flag & 0xfff) == 0x1)
				buf = codec_mm_phys_to_virt(devp->vf_mem_start[i]);
			else if ((devp->cma_config_flag & 0xfff) == 0x101)
				vfbuf[i] = codec_mm_phys_to_virt(devp->vf_mem_start[i]);
			else if ((devp->cma_config_flag & 0xfff) == 0x100)
				vfbuf[i] = phys_to_virt(devp->vf_mem_start[i]);
			else
				buf = phys_to_virt(devp->vf_mem_start[i]);

			/*only write active data*/
			for (j = 0; j < count; j++) {
				vdin_dma_flush(devp, buf, devp->canvas_w,
					       DMA_FROM_DEVICE);
				if (devp->cma_config_flag & 0x100) {
					kernel_write(filp, vfbuf[i],
						  devp->canvas_active_w, &pos);
					vfbuf[i] += devp->canvas_w;
				} else {
					kernel_write(filp, buf,
						  devp->canvas_active_w, &pos);
					buf += devp->canvas_w;
				}
			}
			pr_debug("write buffer %2d of %2u to %s.\n",
				i, devp->canvas_max_num, path);
		}
	} else {
		/*high mem area*/
		pr_debug("high mem area: one line size (%d, active:%d)\n",
			devp->canvas_w, devp->canvas_active_w);
		span = devp->canvas_active_w;

		for (i = 0; i < devp->canvas_max_num; i++) {
			pos = mem_size * i;
			phys = devp->vf_mem_start[i];
			for (j = 0; j < count; j++) {
				high_addr = phys + j * devp->canvas_w;
				buf = vdin_vmap(high_addr, span);
				if (!buf) {
					filp_close(filp, NULL);
					ret = -4;
					goto error;
				}

				vdin_dma_flush(devp, buf, span,
					       DMA_FROM_DEVICE);
				kernel_write(filp, buf, span, &pos);
				vdin_unmap_phyaddr(buf);
			}
			pr_debug("high-mem write buffer %2d of %2u to %s.\n",
				i, devp->canvas_max_num, path);
		}
	}
	pr_info("kernel dump vdin%d mif ok %dx%d(%d) %s %d bit\n",
			devp->index, devp->curr_wr_vfe->vf.width,
			devp->curr_wr_vfe->vf.height, devp->canvas_w,
			vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth_dw);
	filp_close(filp, NULL);
	return;
error:
	pr_info("kernel dump more buffer error: %d\n", ret);
	return;
#endif
}

static void dump_other_mem(struct vdin_dev_s *devp, char *path,
			   unsigned int start, unsigned int offset)
{
#ifdef CONFIG_AMLOGIC_ENABLE_MEDIA_FILE
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf = NULL;
	int ret = 0;

	if (devp->mem_protected) {
		ret = -1;
		goto error;
	}

	filp = filp_open(path, O_RDWR | O_CREAT, 0666);

	if (IS_ERR_OR_NULL(filp)) {
		ret = -2;
		goto error;
	}
	buf = phys_to_virt(start);
	vfs_write(filp, buf, offset, &pos);
	pr_info("kernel dump mif from 0x%x to 0x%x to %s.\n",
		start, start + offset, path);
	vfs_fsync(filp, 0);
	filp_close(filp, NULL);
	return;
error:
	pr_info("kernel dump mif from 0x%x to 0x%x error:%d\n",
		start, start + offset, ret);
	return;
#endif
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void vdin_dump_one_afbce_mem(char *path, struct vdin_dev_s *devp,
				    unsigned int buf_num)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	#define K_PATH_BUFF_LENGTH	128
	struct file *filp = NULL;
	loff_t pos = 0;
	void *buf_head = NULL;
	void *buf_table = NULL;
	void *buf_body = NULL;
	unsigned long high_addr;
	unsigned long phys;
	int highmem_flag = 0;
	unsigned int span = 0;
	unsigned int remain = 0;
	unsigned int count = 0;
	unsigned long j = 0;
	void *virt_buf = NULL;
	unsigned char buff[K_PATH_BUFF_LENGTH];
	int ret = 0;

	if (buf_num >= devp->canvas_max_num) {
		ret = -1;
		goto error;
	}

	if ((devp->cma_config_flag & 0x1) &&
	    devp->cma_mem_alloc == 0) {
		ret = -2;
		goto error;
	}

	highmem_flag = PageHighMem(phys_to_page(devp->afbce_info->fm_body_paddr[0]));

	if (highmem_flag == 0) {
		/*low mem area*/
		pr_info("low mem area\n");
		if ((devp->cma_config_flag & 0xfff) == 0x101) {
			buf_head =
			codec_mm_phys_to_virt(devp->afbce_info->fm_head_paddr[buf_num]);
			buf_table =
			codec_mm_phys_to_virt(devp->afbce_info->fm_table_paddr[buf_num]);
			buf_body =
			codec_mm_phys_to_virt(devp->afbce_info->fm_body_paddr[buf_num]);

			pr_info(".head_paddr=0x%lx,table_paddr=0x%lx,body_paddr=0x%lx\n",
				devp->afbce_info->fm_head_paddr[buf_num],
				(devp->afbce_info->fm_table_paddr[buf_num]),
				devp->afbce_info->fm_body_paddr[buf_num]);
		} else if (devp->cma_config_flag == 0) {
			buf_head =
				phys_to_virt(devp->afbce_info->fm_head_paddr[buf_num]);
			buf_table =
				phys_to_virt(devp->afbce_info->fm_table_paddr[buf_num]);
			buf_body =
				phys_to_virt(devp->afbce_info->fm_body_paddr[buf_num]);

			pr_info("head_paddr=0x%lx,table_paddr=0x%lx,body_paddr=0x%lx\n",
				devp->afbce_info->fm_head_paddr[buf_num],
				(devp->afbce_info->fm_table_paddr[buf_num]),
				devp->afbce_info->fm_body_paddr[buf_num]);
		}
	} else {
		/*high mem area*/
		pr_info("high mem area\n");
		buf_head = vdin_vmap(devp->afbce_info->fm_head_paddr[buf_num],
				     devp->afbce_info->frame_head_size);

		buf_table = vdin_vmap(devp->afbce_info->fm_table_paddr[buf_num],
				      devp->afbce_info->frame_table_size);

		pr_info(".head_paddr=0x%lx,table_paddr=0x%lx,body_paddr=0x%lx\n",
			devp->afbce_info->fm_head_paddr[buf_num],
			(devp->afbce_info->fm_table_paddr[buf_num]),
			devp->afbce_info->fm_body_paddr[buf_num]);
	}
	/*write header bin*/

	if (strlen(path) < K_PATH_BUFF_LENGTH) {
		//strcpy(buff, path);
		snprintf(buff, sizeof(buff), "%s/img_%03d", path, buf_num);
	} else {
		ret = -3;
		goto error;
	}
	strcat(buff, "header.bin");
	filp = filp_open(buff, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		ret = -4;
		goto error;
	}

	vdin_dma_flush(devp, buf_head,
		       devp->afbce_info->frame_head_size,
		       DMA_FROM_DEVICE);
	kernel_write(filp, buf_head, devp->afbce_info->frame_head_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(buf_head);
	pr_debug("write buffer %2d of %2u head to %s.\n",
		buf_num, devp->canvas_max_num, buff);
	filp_close(filp, NULL);

	/*write table bin*/
	pos = 0;
	//strcpy(buff, path);
	snprintf(buff, sizeof(buff), "%s/img_%03d", path, buf_num);
	strcat(buff, "table.bin");
	filp = filp_open(buff, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		ret = -6;
		goto error;
	}
	vdin_dma_flush(devp, buf_table,
		       devp->afbce_info->frame_table_size,
		       DMA_FROM_DEVICE);
	kernel_write(filp, buf_table, devp->afbce_info->frame_table_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(buf_table);
	pr_debug("write buffer %2d of %2u table to %s.\n",
		buf_num, devp->canvas_max_num, buff);
	filp_close(filp, NULL);

	/*write body bin*/
	pos = 0;
	//strcpy(buff, path);
	snprintf(buff, sizeof(buff), "%s/img_%03d", path, buf_num);
	strcat(buff, "body.bin");
	filp = filp_open(buff, O_RDWR | O_CREAT, 0666);
	if (IS_ERR_OR_NULL(filp)) {
		ret = -7;
		goto error;
	}
	if (highmem_flag == 0) {
		if (devp->mem_type == VDIN_MEM_TYPE_SCT) {
			vdin_dump_write_sct_mem(devp, buf_num, filp);
		} else {
			kernel_write(filp, buf_body,
				devp->afbce_info->frame_body_size, &pos);
		}
	} else {
		span = SZ_1M;
		count = devp->afbce_info->frame_body_size / PAGE_ALIGN(span);
		remain = devp->afbce_info->frame_body_size % PAGE_ALIGN(span);
		phys = devp->afbce_info->fm_body_paddr[buf_num];

		for (j = 0; j < count; j++) {
			high_addr = phys + j * span;
			virt_buf = vdin_vmap(high_addr, span);
			if (!virt_buf) {
				filp_close(filp, NULL);
				ret = -8;
				goto error;
			}

			vdin_dma_flush(devp, virt_buf, span, DMA_FROM_DEVICE);
			kernel_write(filp, virt_buf, span, &pos);
			vdin_unmap_phyaddr(virt_buf);
		}

		if (remain) {
			span = devp->afbce_info->frame_body_size - remain;
			high_addr = phys + span;
			virt_buf = vdin_vmap(high_addr, remain);
			if (!virt_buf) {
				filp_close(filp, NULL);
				ret = -9;
				goto error;
			}

			vdin_dma_flush(devp, virt_buf, remain, DMA_FROM_DEVICE);
			kernel_write(filp, virt_buf, remain, &pos);
			vdin_unmap_phyaddr(virt_buf);
		}
	}
	pr_debug("write buffer %2d of %2u body to %s.\n",
		buf_num, devp->canvas_max_num, buff);
	filp_close(filp, NULL);

	pr_info("kernel dump vdin%d afbc buffer ok %dx%d %s %d bit en:0x%x 0x%lx sct:%d\n",
		devp->index, devp->h_active, devp->v_active,
		vdin_get_dst_fmt_str(devp->format_convert), devp->source_bitdepth,
		devp->afbce_flag, devp->afbce_info->fm_body_paddr[0], devp->mem_type);
	return;
error:
	pr_info("kernel dump afbc buffer error: %d\n", ret);
	return;
#endif
}

static void vdin_dv_debug(struct vdin_dev_s *devp)
{
	struct vframe_s *dv_vf;
	struct vf_entry *vfe;
	struct provider_aux_req_s req;

	vfe = devp->curr_wr_vfe;
	if (!vfe) {
		pr_info("current wr vfe is null\n");
		return;
	}
	dv_vf = &vfe->vf;
	req.vf = dv_vf;
	req.bot_flag = 0;
	req.aux_buf = NULL;
	req.aux_size = 0;
	req.dv_enhance_exist = 0;
	vf_notify_provider_by_name("vdin0",
				   VFRAME_EVENT_RECEIVER_GET_AUX_DATA,
				   (void *)&req);
}

/*config vdin output channel order*/
static void vdin_channel_order_config(unsigned int offset,
				      unsigned int vdin_data_bus_0,
				      unsigned int vdin_data_bus_1,
				      unsigned int vdin_data_bus_2)
{
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_0,
		COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_1,
		COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
	wr_bits(offset, VDIN_COM_CTRL0, vdin_data_bus_2,
		COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
}

static void vdin_channel_order_status(unsigned int offset)
{
	unsigned int c0, c1, c2;

	c0 = rd_bits(offset, VDIN_COM_CTRL0,
		     COMP0_OUT_SWT_BIT, COMP0_OUT_SWT_WID);
	c1 = rd_bits(offset, VDIN_COM_CTRL0,
		     COMP1_OUT_SWT_BIT, COMP1_OUT_SWT_WID);
	c2 = rd_bits(offset, VDIN_COM_CTRL0,
		     COMP2_OUT_SWT_BIT, COMP2_OUT_SWT_WID);
	pr_info("vdin(%x) current channel order is: %d %d %d\n",
		offset, c0, c1, c2);
}

void vdin_dump_vs_info(struct vdin_dev_s *devp)
{
	unsigned int cnt = devp->unreliable_vs_cnt;

	if (devp->unreliable_vs_cnt > 10)
		cnt = 10;

	if (devp->unreliable_vs_cnt > 0) {
		for (devp->unreliable_vs_idx = 0; devp->unreliable_vs_idx < cnt;
		     devp->unreliable_vs_idx++)
			pr_info("unreliable_vs_time %d [%d / %d]\n",
				devp->unreliable_vs_time[devp->unreliable_vs_idx],
				devp->unreliable_vs_idx, devp->unreliable_vs_cnt);
	}
}
#endif

const char *vdin_trans_matrix_str(enum vdin_matrix_csc_e csc_idx)
{
	switch (csc_idx) {
	case VDIN_MATRIX_XXX_YUV601_BLACK:
		return "VDIN_MATRIX_XXX_YUV601_BLACK";
	case VDIN_MATRIX_RGB_YUV601:
		return "VDIN_MATRIX_RGB_YUV601";
	case VDIN_MATRIX_GBR_YUV601:
		return "VDIN_MATRIX_GBR_YUV601";
	case VDIN_MATRIX_BRG_YUV601:
		return "VDIN_MATRIX_BRG_YUV601";
	case VDIN_MATRIX_YUV601_RGB:
		return "VDIN_MATRIX_YUV601_RGB";
	case VDIN_MATRIX_YUV601_GBR:
		return "VDIN_MATRIX_YUV601_GBR";
	case VDIN_MATRIX_YUV601_BRG:
		return "VDIN_MATRIX_YUV601_BRG";
	case VDIN_MATRIX_RGB_YUV601F:
		return "VDIN_MATRIX_RGB_YUV601F";
	case VDIN_MATRIX_YUV601F_RGB:
		return "VDIN_MATRIX_YUV601F_RGB";
	case VDIN_MATRIX_RGBS_YUV601:
		return "VDIN_MATRIX_RGBS_YUV601";
	case VDIN_MATRIX_YUV601_RGBS:
		return "VDIN_MATRIX_YUV601_RGBS";
	case VDIN_MATRIX_RGBS_YUV601F:
		return "VDIN_MATRIX_RGBS_YUV601F";
	case VDIN_MATRIX_YUV601F_RGBS:
		return "VDIN_MATRIX_YUV601F_RGBS";
	case VDIN_MATRIX_YUV601F_YUV601:
		return "VDIN_MATRIX_YUV601F_YUV601";
	case VDIN_MATRIX_YUV601_YUV601F:
		return "VDIN_MATRIX_YUV601_YUV601F";
	case VDIN_MATRIX_RGB_YUV709:
		return "VDIN_MATRIX_RGB_YUV709";
	case VDIN_MATRIX_YUV709_RGB:
		return "VDIN_MATRIX_YUV709_RGB";
	case VDIN_MATRIX_YUV709_GBR:
		return "VDIN_MATRIX_YUV709_GBR";
	case VDIN_MATRIX_YUV709_BRG:
		return "VDIN_MATRIX_YUV709_BRG";
	case VDIN_MATRIX_RGB_YUV709F:
		return "VDIN_MATRIX_RGB_YUV709F";
	case VDIN_MATRIX_YUV709F_RGB:
		return "VDIN_MATRIX_YUV709F_RGB";
	case VDIN_MATRIX_RGBS_YUV709:
		return "VDIN_MATRIX_RGBS_YUV709";
	case VDIN_MATRIX_YUV709_RGBS:
		return "VDIN_MATRIX_YUV709_RGBS";
	case VDIN_MATRIX_RGBS_YUV709F:
		return "VDIN_MATRIX_RGBS_YUV709F";
	case VDIN_MATRIX_YUV709F_RGBS:
		return "VDIN_MATRIX_YUV709F_RGBS";
	case VDIN_MATRIX_YUV709F_YUV709:
		return "VDIN_MATRIX_YUV709F_YUV709";
	case VDIN_MATRIX_YUV709_YUV709F:
		return "VDIN_MATRIX_YUV709_YUV709F";
	case VDIN_MATRIX_YUV601_YUV709:
		return "VDIN_MATRIX_YUV601_YUV709";
	case VDIN_MATRIX_YUV709_YUV601:
		return "VDIN_MATRIX_YUV709_YUV601";
	case VDIN_MATRIX_YUV601_YUV709F:
		return "VDIN_MATRIX_YUV601_YUV709F";
	case VDIN_MATRIX_YUV709F_YUV601:
		return "VDIN_MATRIX_YUV709F_YUV601";
	case VDIN_MATRIX_YUV601F_YUV709:
		return "VDIN_MATRIX_YUV601F_YUV709";
	case VDIN_MATRIX_YUV709_YUV601F:
		return "VDIN_MATRIX_YUV709_YUV601F";
	case VDIN_MATRIX_YUV601F_YUV709F:
		return "VDIN_MATRIX_YUV601F_YUV709F";
	case VDIN_MATRIX_YUV709F_YUV601F:
		return "VDIN_MATRIX_YUV709F_YUV601F";
	case VDIN_MATRIX_RGBS_RGB:
		return "VDIN_MATRIX_RGBS_RGB";
	case VDIN_MATRIX_RGB_RGBS:
		return "VDIN_MATRIX_RGB_RGBS";
	case VDIN_MATRIX_RGB2020_YUV2020:
		return "VDIN_MATRIX_RGB2020_YUV2020";
	case VDIN_MATRIX_YUV2020F_YUV2020:
		return "VDIN_MATRIX_YUV2020F_YUV2020";
	default:
		return "VDIN_MATRIX_NULL";
	}
};

const char *vdin_trans_irq_flag_to_str(enum vdin_irq_flg_e flag)
{
	switch (flag) {
	case VDIN_IRQ_FLG_NO_END:
		return "VDIN_IRQ_FLG_NO_FRONT_END";
	case VDIN_IRQ_FLG_IRQ_STOP:
		return "VDIN_IRQ_FLG_IRQ_STOP";
	case VDIN_IRQ_FLG_FAKE_IRQ:
		return "VDIN_IRQ_FLG_FAKE_IRQ";
	case VDIN_IRQ_FLG_DROP_FRAME:
		return "VDIN_IRQ_FLG_DROP_FRAME";
	case VDIN_IRQ_FLG_DV_CHK_SUM_ERR:
		return "VDIN_IRQ_FLG_DV_CHK_SUM_ERR";
	case VDIN_IRQ_FLG_CYCLE_CHK:
		return "VDIN_IRQ_FLG_CYCLE_CHK";
	case VDIN_IRQ_FLG_SIG_NOT_STABLE:
		return "VDIN_IRQ_FLG_SIG_NOT_STABLE";
	case VDIN_IRQ_FLG_FMT_TRANS_CHG:
		return "VDIN_IRQ_FLG_FMT_TRANS_CHG";
	case VDIN_IRQ_FLG_CSC_CHG:
		return "VDIN_IRQ_FLG_CSC_CHG";
	case VDIN_IRQ_FLG_BUFF_SKIP:
		return "VDIN_IRQ_FLG_BUFF_SKIP";
	case VDIN_IRQ_FLG_IGNORE_FRAME:
		return "VDIN_IRQ_FLG_IGNORE_FRAME";
	case VDIN_IRQ_FLG_SKIP_FRAME:
		return "VDIN_IRQ_FLG_SKIP_FRAME";
	case VDIN_IRQ_FLG_GM_DV_CHK_SUM_ERR:
		return "VDIN_IRQ_FLG_GM_DV_CHK_SUM_ERR";
	case VDIN_IRQ_FLG_NO_WR_FE:
		return "VDIN_IRQ_FLG_NO_WR_FE";
	case VDIN_IRQ_FLG_NO_NEXT_FE:
		return "VDIN_IRQ_FLG_NO_NEXT_FE";
	default:
		return "VDIN_IRQ_FLAG_NULL";
	}
}

static void vdin_dump_state(struct vdin_dev_s *devp)
{
	unsigned int i;
	struct vframe_s *vf = &devp->curr_wr_vfe->vf;
	struct tvin_parm_s *cur_parm = &devp->parm;
	struct vf_pool *vfp = devp->vfp;
	unsigned int vframe_size;
//	unsigned int offset = devp->addr_offset;

	if (devp->vf_mem_size_small)
		vframe_size = devp->vf_mem_size_small;
	else
		vframe_size = devp->vf_mem_size;

	pr_info("flags=0x%x,flags_isr=0x%x\n", devp->flags, devp->flags_isr);
	pr_info("h_active = %d, v_active = %d\n",
		devp->h_active, devp->v_active);
	pr_info("canvas_w = %d,%d, canvas_h = %d\n",
		devp->canvas_w, devp->canvas_max_stride, devp->canvas_h);
	pr_info("canvas_align_w = %d, canvas_active_w = %d\n",
		devp->canvas_align_w, devp->canvas_active_w);
	pr_info("double write cfg:0x%x, cur:%d,10bit sup: %d\n", devp->double_wr_cfg,
		devp->double_wr,
		devp->double_wr_10bit_sup);
	pr_info("secure_video:%d, mem protected:%d, support_secure:%d\n",
		devp->secure_video, devp->mem_protected, devp->support_secure);
	if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x100))
		pr_info("mem_start = 0x%lx, mem_size = 0x%x\n",
			devp->mem_start, devp->mem_size);
	else
		for (i = 0; i < devp->canvas_max_num; i++)
			pr_info("buf[%d]mem_start = 0x%lx, mem_size = 0x%x\n",
				i, devp->vf_mem_start[i], vframe_size);
	pr_info("parm.port	= %s(0x%x)\n",
		tvin_port_str(devp->parm.port),
		devp->parm.port);
	pr_info("signal format	= %s(0x%x)\n",
		tvin_sig_fmt_str(devp->parm.info.fmt),
		devp->parm.info.fmt);
	pr_info("prop.trans_fmt	= %s(%d)\n",
		tvin_trans_fmt_str(devp->prop.trans_fmt),
		devp->prop.trans_fmt);
	pr_info("prop.color_format= %s(%d) raw_fmt= %s(%d)\n",
		tvin_color_fmt_str(devp->prop.color_format),
		devp->prop.color_format, tvin_color_fmt_str(devp->prop.raw_color_fmt),
		devp->prop.raw_color_fmt);
	pr_info("prop.dest_cfmt	= %s(%d)\n",
		tvin_color_fmt_str(devp->prop.dest_cfmt),
		devp->prop.dest_cfmt);
	pr_info("prop.color_fmt_range	= (%s)%d\n",
		tvin_trans_color_range_str(devp->prop.color_fmt_range),
		devp->prop.color_fmt_range);
	pr_info("prop.cnt = %d\n", devp->prop.cnt);
	pr_info("format_convert	= %s(%d)\n",
		vdin_fmt_convert_str(devp->format_convert),
		devp->format_convert);
	pr_info("vdin csc_idx	= %s(%d)\n",
		vdin_trans_matrix_str(devp->csc_idx),
		devp->csc_idx);
	pr_info("input_colorimetry = 0x%x\n", devp->parm.info.input_colorimetry);
	pr_info("signal_type	= 0x%x\n", devp->parm.info.signal_type);
	pr_info("qms plus = 0x%x\n", devp->prop.qms_plus_flag);
	pr_info("color_range_force = %d\n", color_range_force);
	pr_info("aspect_ratio = %s(%d)\n decimation_ratio/dvi	= %u / %u\n",
		tvin_aspect_ratio_str(devp->prop.aspect_ratio),
		devp->prop.aspect_ratio,
		devp->prop.decimation_ratio, devp->prop.dvi_info);
	pr_info("pic_aspect_ratio = %s(%d)\n",
		tvin_aspect_ratio_str(devp->prop.pic_aspect_ratio),
		devp->prop.pic_aspect_ratio);
	pr_info("afd active-video = (%d)\n",
		devp->prop.active_ratio);
	pr_info("[pre->cur]:hs(%d->%d),he(%d->%d),vs(%d->%d),ve(%d->%d)\n",
		devp->prop.pre_hs, devp->prop.hs,
		devp->prop.pre_he, devp->prop.he,
		devp->prop.pre_vs, devp->prop.vs,
		devp->prop.pre_ve, devp->prop.ve);
	pr_info("scaling4w:%d,scaling4h:%u\n", devp->prop.scaling4w, devp->prop.scaling4h);
	pr_info("report: h_active %d, v_active:%d, v_total:%d\n",
		vdin_get_active_h(devp), vdin_get_active_v(devp),
		vdin_get_total_v(devp));
	pr_info("frontend_fps:%d\n", devp->prop.fps);
	pr_info("frontend_colordepth:%d\n", devp->prop.colordepth);
	pr_info("source_bitdepth:%d %d\n", devp->source_bitdepth, devp->source_bitdepth_dw);
	pr_info("color_depth_config:0x%x\n", devp->color_depth_config);
	pr_info("matrix_pattern_mode:0x%x\n", devp->matrix_pattern_mode);
	pr_info("hdcp_sts:0x%x\n", devp->prop.hdcp_sts);
	pr_info("macrovision_sts:0x%x\n", devp->prop.macrovision_sts);
	pr_info("full_pack:%d\n", devp->full_pack);
	pr_info("force_malloc_yuv_422_to_444:%d\n", devp->force_malloc_yuv_422_to_444);
	pr_info("color_depth_support:0x%x\n", devp->color_depth_support);
	pr_info("cma_flag:0x%x\n", devp->cma_config_flag);
	pr_info("auto_cut_window_en:%d\n", devp->auto_cut_window_en);
	pr_info("cut_window_cfg:%d\n", devp->cut_window_cfg);
	pr_info("cma_mem_alloc:%d\n", devp->cma_mem_alloc);
	pr_info("cma_mem_size:0x%x\n", devp->cma_mem_size);
	pr_info("cma_mem_mode:%d\n", devp->cma_mem_mode);
	pr_info("frame_buff_num:%d, vf_mem_max_cnt:%d\n",
		devp->frame_buff_num, devp->vf_mem_max_cnt);
	pr_info("force_yuv444_malloc:%d\n", devp->force_yuv444_malloc);
	pr_info("hdr_Flag =0x%x\n", devp->prop.vdin_hdr_flag);
	pr_info("hdr10p_Flag =0x%x\n", devp->prop.hdr10p_info.hdr10p_on);
	vdin_check_hdmi_hdr(devp);
	pr_info("cycle:%d, msr_clk_val:%d\n", devp->cycle, devp->msr_clk_val);

	vdin_dump_vf_state(devp->vfp);
	if (vf) {
		pr_info("current vframe index(%u):\n", vf->index);
		pr_info("\t buf(w%u, h%u),type(0x%x 0x%x 0x%x),flag(0x%x), flag_ext(0x%x), duration(%d),",
		vf->width, vf->height, vf->type, vf->type_ext, vf->type_dw, vf->flag,
		vf->flag_ext, vf->duration);
		pr_info("\t ratio_control(0x%x), signal_type:0x%x, ext_signal_type:0x%x\n",
			vf->ratio_control, vf->signal_type, vf->ext_signal_type);
		pr_info("\t bitdepth(0x%x), bitdepth_dw(0x%x)\n", vf->bitdepth, vf->bitdepth_dw);
		pr_info("\t trans fmt %u, left_start_x %u,",
			vf->trans_fmt, vf->left_eye.start_x);
		pr_info("\t right_start_x %u, width_x %u\n",
			vf->right_eye.start_x, vf->left_eye.width);
		pr_info("\t left_start_y %u, right_start_y %u, height_y %u\n",
			vf->left_eye.start_y, vf->right_eye.start_y,
			vf->left_eye.height);
		pr_info("vf compwidth:%u,compheight:%u\n", vf->compWidth,
			vf->compHeight);
		pr_info("CRC: 0x%x\n", vf->crc);
	}
	if (vfp) {
		pr_info("skip_vf_num:%d\n", vfp->skip_vf_num);
		pr_info("**************disp_mode**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			pr_info("[%d]:%-5d", i, vfp->disp_mode[i]);
		pr_info("\n**************disp_index**************\n");
		for (i = 0; i < VFRAME_DISP_MAX_NUM; i++)
			pr_info("[%d]:%-5d", i, vfp->disp_index[i]);
	}
	pr_info("\n current parameters:\n");
	pr_info("\t frontend of vdin index :  %d, 3d flag : 0x%x\n",
		cur_parm->index,  cur_parm->flag);
	pr_info("\t reserved 0x%x, devp->flags:0x%x\n",
		cur_parm->reserved, devp->flags);
	pr_info("max buffer num %u, msr_clk_val:%d.\n",
		devp->canvas_max_num, devp->msr_clk_val);
	pr_info("canvas buffer size %u, rdma_enable: %d.\n",
		devp->canvas_max_size, devp->rdma_enable);
	pr_info("range(%d),csc_cfg:0x%x,urgent_en:%d\n",
		devp->prop.color_fmt_range,
		devp->csc_cfg, devp->dts_config.urgent_en);
	pr_info("black_bar_enable: %d, hist_bar_enable: %d, use_frame_rate: %d\n ",
		devp->black_bar_enable,
		devp->hist_bar_enable, devp->use_frame_rate);
	pr_info("vdin_rest_flag: %d, irq_cnt: %d, rdma_irq_cnt: %d\n",
		devp->vdin_reset_flag, devp->irq_cnt, devp->rdma_irq_cnt);
	pr_info("vdin_irq_flag:%d %s\n", devp->vdin_irq_flag,
		vdin_trans_irq_flag_to_str(devp->vdin_irq_flag));
	pr_info("vpu crash irq cnt: %d\n", devp->vpu_crash_cnt);
	pr_info("vdin_drop_cnt: %d frame_cnt:%d ignore_frames:%d\n",
		devp->vdin_drop_cnt, devp->frame_cnt, devp->ignore_frames);
	pr_info("game_mode cfg :  0x%x 0x%x\n", game_mode_cfg, game_mode);
	pr_info("game_mode cur:  0x%x bypass_game_mode:%#x\n",
		devp->game_mode, devp->debug.bypass_game_mode);
	pr_info("vrr_mode:  0x%x,vdin_vrr_en_flag=%d\n", devp->vrr_data.vrr_mode,
		devp->vrr_data.vdin_vrr_en_flag);
	pr_info("vrr_en:  pre=%d,cur:%d,base_fr:%d\n",
		devp->pre_prop.vtem_data.vrr_en, devp->prop.vtem_data.vrr_en,
		devp->prop.vtem_data.base_framerate);
	pr_info("vdin_vrr_flag:  pre=%d,cur:%d\n", devp->pre_prop.vdin_vrr_flag,
		devp->prop.vdin_vrr_flag);
	pr_info("vdin_pc_mode:%d vdin_cur_mode:%d bypass_pc_mode:%d\n", vdin_pc_mode,
		devp->vdin_pc_mode, devp->debug.bypass_pc_mode);
	pr_info("vdin_game_frc:%d bypass_game_frc:%d\n",
		devp->vdin_game_frc, devp->debug.bypass_game_frc);

	pr_info("afbce_flag: %#x %#x\n", devp->dts_config.afbce_flag_cfg, devp->afbce_flag);
	pr_info("afbce_mode: %d, afbce_valid: %d\n", devp->afbce_mode,
		devp->afbce_valid);
	pr_info("write vframe en: %d, pre: %d\n", devp->vframe_wr_en,
		devp->vframe_wr_en_pre);
	if (devp->afbce_mode == 1 || devp->double_wr) {
		for (i = 0; i < devp->vf_mem_max_cnt; i++) {
			pr_info("head(%d) addr:0x%lx, size:0x%x;y:0x%x\n",
				i, devp->afbce_info->fm_head_paddr[i],
				devp->afbce_info->frame_head_size,
				devp->afbce_info->frame_head_size_y);
		}

		for (i = 0; i < devp->vf_mem_max_cnt; i++) {
			pr_info("table(%d) addr:0x%lx, size:0x%x;y:0x%x\n",
				i, devp->afbce_info->fm_table_paddr[i],
				devp->afbce_info->frame_table_size,
				devp->afbce_info->frame_table_size_y);
		}

		for (i = 0; i < devp->vf_mem_max_cnt; i++) {
			pr_info("body(%d) addr:0x%lx, size:0x%x;y:0x%x\n",
				i, devp->afbce_info->fm_body_paddr[i],
				devp->afbce_info->frame_body_size,
				devp->afbce_info->frame_body_size_y);
		}
	}
	pr_info("dolby_input : %d,unique:%d\n", devp->dv.dolby_input,
		devp->prop.dv_unique_drm_flag);
	pr_info("sbtm mode: %d", devp->prop.sbtm_data.sbtm_mode);
	/*if (devp->cma_config_en != 1 || !(devp->cma_config_flag & 0x100))*/
	/*	pr_info("dolby_mem_start = %ld, dolby_mem_size = %d\n",*/
	/*		(devp->mem_start +*/
	/*		devp->mem_size - devp->canvas_max_num * dolby_size_byte),*/
	/*		dolby_size_byte);*/
	/*else*/
	/*	for (i = 0; i < devp->canvas_max_num; i++)*/
	/*		pr_info("dolby_mem_start[%d] = %ld, dolby_mem_size = %d\n",*/
	/*			i, (devp->vf_mem_start[i] + devp->vf_mem_size -*/
	/*			dolby_size_byte), dolby_size_byte);*/

	for (i = 0; i < devp->canvas_max_num; i++) {
		pr_info("dv_mem(%d):0x%x;mem_handle:%p\n", devp->vfp->dv_buf_size[i],
			devp->vfp->dv_buf_mem[i], devp->vf_codec_mem[i]);
	}

	pr_info("dvEn:%d,dv_flag:%d;dv_config:%d,dolby_ver:%d,low_latency:(%d,%d,%d) allm:%d\n",
		is_amdv_enable(),
		devp->dv.dv_flag, devp->dv.dv_config, devp->prop.dolby_vision,
		devp->dv.low_latency, devp->prop.low_latency,
		devp->vfp->low_latency, devp->pre_prop.latency.allm_mode);
	pr_info("fmm_flag:%d, fmm_vsif_flag:%d imax_flag:%d\n",
		devp->prop.filmmaker.fmm_flag, devp->prop.filmmaker.fmm_vsif_flag,
		devp->prop.imax_flag);
	pr_info("dv emp size:%d crc_flag:%d\n", devp->prop.emp_data.size,
		devp->dv.dv_crc_check);
	pr_info("dv_is_not_std:%d\n", devp->dv_is_not_std);
	pr_info("size of struct vdin_dev_s: %d\n", devp->vdin_dev_ssize);
	pr_info("devp->dv.dv_vsif:(%d,%d,%d,%d,%d,%d,%d,%d);\n",
		devp->dv.dv_vsif.dolby_vision_signal,
		devp->dv.dv_vsif.backlt_ctrl_MD_present,
		devp->dv.dv_vsif.auxiliary_MD_present,
		devp->dv.dv_vsif.eff_tmax_PQ_hi,
		devp->dv.dv_vsif.eff_tmax_PQ_low,
		devp->dv.dv_vsif.auxiliary_runmode,
		devp->dv.dv_vsif.auxiliary_runversion,
		devp->dv.dv_vsif.auxiliary_debug0);
	pr_info("devp->prop.dv_vsif:(%d,%d,%d,%d,%d,%d,%d,%d)\n",
		devp->prop.dv_vsif.dolby_vision_signal,
		devp->prop.dv_vsif.backlt_ctrl_MD_present,
		devp->prop.dv_vsif.auxiliary_MD_present,
		devp->prop.dv_vsif.eff_tmax_PQ_hi,
		devp->prop.dv_vsif.eff_tmax_PQ_low,
		devp->prop.dv_vsif.auxiliary_runmode,
		devp->prop.dv_vsif.auxiliary_runversion,
		devp->prop.dv_vsif.auxiliary_debug0);
	pr_info("devp->vfp->dv_vsif:(%d,%d,%d,%d,%d,%d,%d,%d)\n",
		devp->vfp->dv_vsif.dolby_vision_signal,
		devp->vfp->dv_vsif.backlt_ctrl_MD_present,
		devp->vfp->dv_vsif.auxiliary_MD_present,
		devp->vfp->dv_vsif.eff_tmax_PQ_hi,
		devp->vfp->dv_vsif.eff_tmax_PQ_low,
		devp->vfp->dv_vsif.auxiliary_runmode,
		devp->vfp->dv_vsif.auxiliary_runversion,
		devp->vfp->dv_vsif.auxiliary_debug0);
	pr_info("rdma handle : %d\n", devp->rdma_handle);
	pr_info("hv reverse enabled: %d\n", devp->hv_reverse_en);
	pr_info("dbg_dump_frames: %d,dbg_stop_dec_delay:%d\n",
		devp->dbg_dump_frames, devp->dbg_stop_dec_delay);
	pr_info("fs_open_cnt: %d\n", devp->fs_open_cnt);
	pr_info("tuner_id: %d\n", devp->tuner_id);
	pr_info("vdin_function_sel: 0x%x\n", devp->vdin_function_sel);
	pr_info("Vdin driver version :  %s\n", VDIN_VER);
	pr_info("Vdin driver version_V1 :  %s\n", VDIN_VER_V1);
	/*vdin_dump_vs_info(devp);*/
}

/*same as vdin_dump_state*/
static int seq_file_vdin_state_show(struct seq_file *seq, void *v)
{
	return 0;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void vdin_dump_count(struct vdin_dev_s *devp)
{
	pr_info("vdin%d check:%d, afbce[%d %d], mif[%d %d]\n",
		devp->index, devp->stats.write_done_check,
		devp->stats.afbce_normal_cnt, devp->stats.afbce_abnormal_cnt,
		devp->stats.wmif_normal_cnt, devp->stats.wmif_abnormal_cnt);
	pr_info("irq:%d(%d), vpu_irq:%d(%d), wr_done:%d(%d), vdin2_wr_done:%d(%d)\n",
		devp->irq, devp->irq_cnt, devp->vpu_crash_irq, devp->vpu_crash_cnt,
		devp->wr_done_irq, devp->stats.wr_done_irq_cnt,
		devp->vdin2_meta_wr_done_irq, devp->stats.meta_wr_done_irq_cnt);
	pr_info("put_frame:%d, frame:%d, ignore:%d\n",
		devp->put_frame_cnt, devp->frame_cnt, devp->ignore_frames);
	pr_info("frame_drop:%d, drop_cnt:%d, drop_num:%d\n",
		devp->frame_drop_num, devp->vdin_drop_cnt, devp->vdin_drop_num);
	pr_info("rdma_manual_cnt:%d, rdma_undone_cnt:%d\n",
		devp->stats.rdma_manual_cnt, devp->rdma_undone_cnt);
	pr_info("dbg_fr_ctl:%d, drop_ctl_cnt:%d\n", devp->dbg_fr_ctl, devp->vdin_drop_ctl_cnt);

	vdin_dump_vs_info(devp);
}

static void vdin_dump_dts_config(struct vdin_dev_s *devp)
{
	pr_info("vdin%d chk_wr_done_en: %d\n", devp->index, devp->dts_config.chk_write_done_en);
	pr_info("urgent_en: %d\n", devp->dts_config.urgent_en);
	pr_info("v4l_en:%d\n", devp->dts_config.v4l_en);
	pr_info("keystone_sel:%d\n", devp->dts_config.keystone_sel);
	pr_info("max_buf_num:%d, min_buf_num:%d\n",
		devp->dts_config.max_buf_num, devp->dts_config.min_buf_num);
	pr_info("cm_enable:%d\n", devp->dts_config.cm_enable);
	pr_info("vdin_det_idle_wait:%d\n", devp->dts_config.vdin_det_idle_wait);
	pr_info("vs_en:%d, sm_en:%d\n",
		devp->dts_config.vdin_get_prop_in_vs_en, devp->dts_config.vdin_get_prop_in_sm_en);
	pr_info("canvas_config_mode:%d\n", devp->dts_config.canvas_config_mode);
	pr_info("viu_hw_irq:%d\n", devp->dts_config.viu_hw_irq);
	pr_info("game_mode switch_frames:%d %d\n",
		devp->dts_config.game_mode_switch_frames,
		devp->dts_config.game_mode_phlock_switch_frames);
	pr_info("vdin_vrr_switch_cnt:%d\n", devp->dts_config.vdin_vrr_switch_cnt);
	pr_info("sct_cache_size:%d\n", devp->dts_config.sct_cache_size);
	pr_info("re_config:%d re_cfg_drop:%d\n",
		devp->dts_config.vdin_re_config, devp->dts_config.vdin_re_cfg_drop_cnt);
	pr_info("back_nosig_max_cnt:%d\n", devp->dts_config.back_nosig_max_cnt);
	pr_info("atv unstable_cnt in:%d, out:%d %d\n",
		devp->dts_config.atv_unstable_in_cnt, devp->dts_config.atv_unstable_out_cnt,
		devp->dts_config.atv_stable_out_cnt);
	pr_info("hdmi unstable_cnt:%d stable cnt:%d\n", devp->dts_config.hdmi_unstable_out_cnt,
		devp->dts_config.hdmi_stable_out_cnt);
	pr_info("atv stable cnt out:%d check:%d\n",
		devp->dts_config.atv_stable_out_cnt, devp->dts_config.atv_stable_fmt_check_cnt);
	pr_info("atv prestable cnt out:%d\n", devp->dts_config.atv_prestable_out_cnt);
	pr_info("other stable cnt out:%d\n", devp->dts_config.other_stable_out_cnt);
	pr_info("other unstable cnt in:%d out:%d\n",
		devp->dts_config.other_unstable_in_cnt, devp->dts_config.other_unstable_out_cnt);
	pr_info("nosig_cnt:%d, nosig2_cnt:%d\n",
		devp->dts_config.nosig_in_cnt, devp->dts_config.nosig2_unstable_cnt);
	pr_info("vdin_chg_cnt dv:%d, hdr:%d, vrr:%d, qms:%d\n",
		devp->dts_config.vdin_dv_chg_cnt, devp->dts_config.vdin_hdr_chg_cnt,
		devp->dts_config.vdin_vrr_chg_cnt, devp->dts_config.vdin_qms_chg_cnt);
}

static void vdin_dump_debug_config(struct vdin_dev_s *devp)
{
	pr_info("vdin%d dbg_en:0x%x, monitor:0x%x ctl:0x%x, sm:0x%x t:%d\n",
		devp->index, devp->debug.vdin_dbg_en, devp->debug.vdin_isr_monitor,
		devp->debug.vdin_ctl_dbg, devp->debug.sm_debug_enable,
		devp->debug.vdin_time_en);
	pr_info("sct_print_ctl:%#x\n", devp->debug.sct_print_ctl);
	pr_info("cutwin:%d %d %d %d\n", devp->debug.cutwin.hs, devp->debug.cutwin.he,
		devp->debug.cutwin.vs, devp->debug.cutwin.ve);
	pr_info("vdin_recycle_num:%d\n", devp->debug.vdin_recycle_num);
	pr_info("dbg_print_cntl:%d\n", devp->debug.dbg_print_cntl);
	pr_info("dbg_pattern:%d\n", devp->debug.dbg_pattern);
	pr_info("dbg_sct_ctl:%d\n", devp->debug.dbg_sct_ctl);
	pr_info("dbg_de_interlanced_ctl:%d\n", devp->debug.dbg_de_interlanced_ctl);
	pr_info("scaler:%dx%d, dest_fmt:%d\n",
		devp->debug.scaling4h, devp->debug.scaling4w, devp->debug.dest_cfmt);
	pr_info("vdin1_line_buff:%d\n", devp->debug.vdin1_line_buff);
	pr_info("manual_change_csc:%d\n", devp->debug.manual_change_csc);
	pr_info("change_get_drm:%d\n", devp->debug.change_get_drm);
	pr_info("dbg_sel_mat:%d\n", devp->debug.dbg_sel_mat);
	pr_info("vdin1_set_hdr_bypass:%d\n", devp->debug.vdin1_set_hdr_bypass);
	pr_info("dbg_force_shrink_en:%d\n", devp->debug.dbg_force_shrink_en);
	pr_info("bypass_tunnel:%d\n", devp->bypass_tunnel);
	pr_info("pause_dec mif:%d afbc:%d\n",
		devp->debug.pause_mif_dec, devp->debug.pause_afbce_dec);
	pr_info("bypass_filter_vsync:%d\n", devp->debug.bypass_filter_vsync);
	pr_info("sar: %dx%d\n", devp->debug.sar_width, devp->debug.sar_height);
	pr_info("ratio_control:%d\n", devp->debug.ratio_control);
	pr_info("dbg_rw_reg_en:%d\n", devp->debug.dbg_rw_reg_en);
	pr_info("rgb_info_enable:%d\n", devp->debug.rgb_info_enable);
	pr_info("invert_top_bot:%d\n", devp->debug.invert_top_bot);
	pr_info("dv_dbg_log:%#x, dv_dbg_log_du=%#x, dv_mask=%#x\n",
		devp->debug.dv_dbg_log, devp->debug.dv_dbg_log_du, devp->dts_config.dv_mask);
	pr_info("vdin_delay_num:%d\n", devp->debug.vdin_delay_num);
	pr_info("vdin_frame_work_mode:%d\n", devp->debug.vdin_frame_work_mode);
	pr_info("sleep_time:%d\n", devp->debug.sleep_time);
}

static void vdin_dump_histgram(struct vdin_dev_s *devp)
{
	uint i;

	for (i = 0; i < 64; i++) {
		pr_info("dump hist [%d]0x%-8x\t", i, devp->parm.histgram[i]);
		if ((i + 1) % 8 == 0)
			pr_info("\n");
	}
}

/*type: 1:nv21 2:yuv422 3:yuv444*/
static void vdin_write_afbce_mem(struct vdin_dev_s *devp, char *type,
				 char *path)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	char md_path_head[100], md_path_body[100];
	unsigned int i;
	unsigned long j;
	int highmem_flag = 0;
	unsigned int size = 0;
	unsigned int span = 0;
	unsigned int remain = 0;
	unsigned int count = 0;
	unsigned long high_addr;
	unsigned long phys;
	struct file *filp = NULL;
	loff_t pos = 0;
	void *body_dts = NULL;
	void *virt_buf = NULL;
	long val;
	void *head_dts = NULL;

	if (kstrtol(type, 10, &val) < 0)
		return;
	if (!path)
		return;

	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}

	sprintf(md_path_head, "%s_1header.bin", path);
	sprintf(md_path_body, "%s_1body.bin", path);

	i = devp->curr_wr_vfe->af_num;
	devp->curr_wr_vfe->vf.type = VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_FIELD | VIDTYPE_COMPRESS | VIDTYPE_SCATTER;
	switch (val) {
	case 1:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_NV21;
		break;
	case 2:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_422;
		break;
	case 3:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_444;
		break;
	default:
		devp->curr_wr_vfe->vf.type |= VIDTYPE_VIU_422;
		break;
	}

	devp->curr_wr_vfe->vf.compHeadAddr = devp->afbce_info->fm_head_paddr[i];
	devp->curr_wr_vfe->vf.compBodyAddr = devp->afbce_info->fm_body_paddr[i];

	highmem_flag = PageHighMem(phys_to_page(devp->afbce_info->fm_body_paddr[0]));
	if (highmem_flag == 0) {
		head_dts =
			codec_mm_phys_to_virt(devp->afbce_info->fm_head_paddr[i]);
	} else {
		head_dts = vdin_vmap(devp->afbce_info->fm_head_paddr[i],
				     devp->afbce_info->frame_head_size);
	}

	pr_info("highmem=%d head bin file path = %s\n", highmem_flag, md_path_head);
	filp = filp_open(md_path_head, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", md_path_head);
		return;
	}

	size = kernel_read(filp, head_dts,
			devp->afbce_info->frame_head_size, &pos);
	if (highmem_flag)
		vdin_unmap_phyaddr(head_dts);

	filp_close(filp, NULL);
	pos = 0;
	pr_info("body bin file path = %s\n", md_path_body);
	filp = filp_open(md_path_body, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", md_path_body);
		return;
	}

	if (highmem_flag == 0) {
		body_dts =
		codec_mm_phys_to_virt(devp->afbce_info->fm_body_paddr[i]);

		size = kernel_read(filp, body_dts,
				devp->afbce_info->frame_body_size, &pos);
	} else {
		span = SZ_1M;
		count = devp->afbce_info->frame_body_size / PAGE_ALIGN(span);
		remain = devp->afbce_info->frame_body_size % PAGE_ALIGN(span);
		phys = devp->afbce_info->fm_body_paddr[i];

		for (j = 0; j < count; j++) {
			high_addr = phys + j * span;
			virt_buf = vdin_vmap(high_addr, span);
			if (!virt_buf) {
				pr_info("vdin_vmap error\n");
				return;
			}
			kernel_read(filp, virt_buf, span, &pos);
			vdin_unmap_phyaddr(virt_buf);
		}
	}

	filp_close(filp, NULL);

	provider_vf_put(devp->curr_wr_vfe, devp->vfp);
	devp->curr_wr_vfe = NULL;
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_VFRAME_READY,
			   NULL);
#endif
}

static void vdin_write_mem(struct vdin_dev_s *devp, char *type,
			   char *path, char *md_path)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	unsigned int size = 0, val_type = 0;
	struct file *filp = NULL,  *md_flip = NULL;
	loff_t pos = 0;
	void *dts = NULL;
	long val;
	int index, j, span;
	int highmem_flag, count;
	unsigned long addr;
	unsigned long high_addr;
	struct vf_pool *p = devp->vfp;

	/* vtype = simple_strtol(type, NULL, 10); */

	if (kstrtol(type, 10, &val) < 0)
		return;

	val_type = val;
	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}

	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}
	devp->curr_wr_vfe->vf.type = VIDTYPE_VIU_SINGLE_PLANE |
			VIDTYPE_VIU_FIELD | VIDTYPE_VIU_422;
	if (val_type == 1) {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_INTERLACE_TOP;

	} else if (val_type == 3) {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_INTERLACE_BOTTOM;
	} else {
		devp->curr_wr_vfe->vf.type |= VIDTYPE_PROGRESSIVE;
	}
	if (val_type == 6) {
		devp->curr_wr_vfe->vf.bitdepth = BITDEPTH_Y10;
		devp->curr_wr_vfe->vf.bitdepth |= FULL_PACK_422_MODE;
		devp->curr_wr_vfe->vf.signal_type = 0x91000;
		devp->curr_wr_vfe->vf.source_type = VFRAME_SOURCE_TYPE_HDMI;
	}
	if (val_type == 7 || val_type == 8) {
		devp->curr_wr_vfe->vf.type = 0x7000;
		devp->curr_wr_vfe->vf.bitdepth = 0;
		devp->curr_wr_vfe->vf.signal_type = 0x10100;
		devp->curr_wr_vfe->vf.source_type = VFRAME_SOURCE_TYPE_HDMI;
	}
	addr = canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr);
	/* dts = ioremap(canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr), */
	/* real_size); */

	if (devp->cma_config_flag & 0x100)
		highmem_flag = PageHighMem(phys_to_page(devp->vf_mem_start[0]));
	else
		highmem_flag = PageHighMem(phys_to_page(devp->mem_start));

	if (highmem_flag == 0) {
		dts = phys_to_virt(addr);
		for (j = 0; j < devp->canvas_h; j++) {
			kernel_read(filp, dts + (devp->canvas_w * j),
				 devp->canvas_active_w, &pos);
		}
		iounmap(dts);
		filp_close(filp, NULL);
	} else {
		count = devp->canvas_h;
		span = devp->canvas_active_w;
		for (j = 0; j < count; j++) {
			high_addr = addr + j * devp->canvas_w;
			dts = vdin_vmap(high_addr, span);
			if (!dts) {
				pr_info("vdin_vmap error\n");
				return;
			}
			kernel_read(filp, dts, span, &pos);
			vdin_dma_flush(devp, dts, span, DMA_TO_DEVICE);
			vdin_unmap_phyaddr(dts);
		}
		filp_close(filp, NULL);
	}

	if (val_type == 8) {
		md_flip = filp_open(md_path, O_RDONLY, 0);
		if (IS_ERR_OR_NULL(md_flip)) {
			pr_info("read %s error or md_flip = NULL.\n", md_path);
			return;
		}
		index = devp->curr_wr_vfe->vf.index & 0xff;
		if (index != 0xff && index >= 0 && index < p->size) {
			u8 *c = devp->vfp->dv_buf_vmem[index];

			pos = 0;
			size = (unsigned int)
			kernel_read(md_flip, devp->vfp->dv_buf_vmem[index],
				 4096, &pos);
			p->dv_buf_size[index] = size;
			devp->vfp->dv_buf[index] = &c[0];
		}
		filp_close(md_flip, NULL);
	}

	provider_vf_put(devp->curr_wr_vfe, devp->vfp);
	devp->curr_wr_vfe = NULL;
	vf_notify_receiver(devp->name, VFRAME_EVENT_PROVIDER_VFRAME_READY,
			   NULL);
	pr_info("path = %s, md_path=%d, highmem=%d, type=%d\n",
		path, md_path, highmem_flag, val_type);
#endif
}

static void vdin_write_cont_mem(struct vdin_dev_s *devp, char *type,
				char *path, char *num)
{
#ifdef CONFIG_AMLOGIC_ENABLE_VIDEO_PIPELINE_DUMP_DATA
	unsigned int size = 0, i = 0;
	struct file *filp = NULL;
	loff_t pos = 0;
	void *dts = NULL;
	long field_num;
	unsigned long addr;

	/* vtype = simple_strtol(type, NULL, 10); */

	if (kstrtol(num, 10, &field_num) < 0)
		return;

	if (!devp->curr_wr_vfe) {
		devp->curr_wr_vfe = provider_vf_get(devp->vfp);
		if (!devp->curr_wr_vfe) {
			pr_info("no buffer to write.\n");
			return;
		}
	}
	pr_info("bin file path = %s type = %s\n", path, type);
	filp = filp_open(path, O_RDONLY, 0);
	if (IS_ERR_OR_NULL(filp)) {
		pr_info("read %s error or filp is NULL.\n", path);
		return;
	}
	for (i = 0; i < field_num; i++) {
		if (!devp->curr_wr_vfe) {
			devp->curr_wr_vfe = provider_vf_get(devp->vfp);
			if (!devp->curr_wr_vfe) {
				pr_info("no buffer to write.\n");
				return;
			}
		}
		if (!strcmp("top", type)) {
			if (i % 2 == 0) {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_TOP;
			} else {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_BOTTOM;
			}
		} else if (!strcmp("bot", type)) {
			if (i % 2 == 1) {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_TOP;
			} else {
				devp->curr_wr_vfe->vf.type |=
					VIDTYPE_INTERLACE_BOTTOM;
			}
		} else {
			devp->curr_wr_vfe->vf.type |= VIDTYPE_PROGRESSIVE;
		}
		addr = canvas_get_addr(devp->curr_wr_vfe->vf.canvas0Addr);
		/* real_size); */
		dts = phys_to_virt(addr);
		size = kernel_read(filp, dts, devp->canvas_max_size, &pos);
		if (size < devp->canvas_max_size) {
			pr_info("%s read %u < %u error.\n",
				__func__, size, devp->canvas_max_size);
			return;
		}
		provider_vf_put(devp->curr_wr_vfe, devp->vfp);
		devp->curr_wr_vfe = NULL;
		vf_notify_receiver(devp->name,
			VFRAME_EVENT_PROVIDER_VFRAME_READY, NULL);
		iounmap(dts);
	}
	filp_close(filp, NULL);
#endif
}

static void dump_dolby_metadata(struct vdin_dev_s *devp)
{
	unsigned int i, j;
	char *p;

	for (i = 0; i < devp->canvas_max_num; i++) {
		pr_info("*****dolby_metadata(%d)[0x%x](%d byte):*****\n",
			i, devp->vfp->dv_buf_mem[i], dolby_size_byte);
		for (j = 0; j < dolby_size_byte; j++) {
			if ((j % 16) == 0)
				pr_info("\n");
			p = devp->vfp->dv_buf_vmem[i];
			pr_info("0x%02x\t", *(p + j));
			/*pr_info("0x%02x\t", *(devp->vfp->dv_buf_ori[i] + j));*/

		}
	}
}

static void dump_dolby_buf_clean(struct vdin_dev_s *devp)
{
	/*unsigned int i;*/

	/*for (i = 0; i < devp->canvas_max_num; i++)*/
	/*	memset(devp->vfp->dv_buf_ori[i], 0, dolby_size_byte);*/
}

#ifdef CONFIG_AML_LOCAL_DIMMING

static void vdin_dump_histgram_ldim(struct vdin_dev_s *devp,
	unsigned int h_num, unsigned int v_num)
{
	uint i, j;
	unsigned int local_ldim_max[100] = {0};

	/*memcpy(&local_ldim_max[0], &vdin_ldim_max_global[0],
	 *100*sizeof(unsigned int));
	 */
	for (i = 0; i < h_num; i++) {
		for (j = 0; j < v_num; j++) {
			pr_info("dump hist local dimming (%d,%d,%d)\t",
				local_ldim_max[j + i * 10] & 0x3ff,
				(local_ldim_max[j + i * 10] >> 10) & 0x3ff,
				(local_ldim_max[j + i * 10] >> 20) & 0x3ff);
		}
		pr_info("\n");
	}
}
#endif

static void vdin_write_back_reg(struct vdin_dev_s *devp)
{
	pr_info("VPU_VIU_VENC_MUX_CTRL(0x%x):0x%x\n", 0x271a, R_VCBUS(0x271a));
	if (devp->dtdata->hw_ver == VDIN_HW_T6X) {
		pr_info("VPU_VIU2VDIN0_HALF_CTRL(0x%x):0x%x\n", 0x272d, R_VCBUS(0x272d));
		pr_info("VPU_VIU2VDIN0_BUF_SIZE(0x%x):0x%x\n", 0x2737, R_VCBUS(0x2737));
		pr_info("VPP_WRBAK_HOLD_CTRL(0x%x):0x%x\n", 0x1df4, R_VCBUS(0x1df4));
	}
	pr_info("VPU_VIU2VDIN_HDN_CTRL(0x%x):0x%x\n", 0x2780, R_VCBUS(0x2780));
	pr_info("VPU_VDIN_MISC_CTRL(0x%x):0x%x\n", 0x2782, R_VCBUS(0x2782));
	pr_info("VPU_VIU_VDIN_IF_MUX_CTRL(0x%x):0x%x\n", 0x2783, R_VCBUS(0x2783));
	pr_info("VIU_MISC_CTRL1(0x%x):0x%x\n", 0x1a07, R_VCBUS(0x1a07));
	pr_info("WR_BACK_MISC_CTRL(0x%x):0x%x\n", 0x1a0d, R_VCBUS(0x1a0d));
	pr_info("VIU_FRM_CTRL(0x%x):0x%x\n", 0x1a51, R_VCBUS(0x1a51));
	pr_info("VIU1_FRM_CTRL(0x%x):0x%x\n", 0x1a8d, R_VCBUS(0x1a8d));
	pr_info("VIU2_FRM_CTRL(0x%x):0x%x\n", 0x1a8e, R_VCBUS(0x1a8e));
	pr_info("VPP_VE_H_V_SIZE(0x%x):0x%x\n", 0x1da4, R_VCBUS(0x1da4));
	pr_info("VPP_OUT_H_V_SIZE(0x%x):0x%x\n", 0x1da5, R_VCBUS(0x1da5));
	pr_info("VPP_WR_BAK_CTRL(0x%x):0x%x\n", 0x1df9, R_VCBUS(0x1df9));
	if (R_VCBUS_BIT(0x2783, 0, 6) == 0x20)
		pr_info("VPP1_WR_BAK_CTRL(0x%x):0x%x\n", 0x5981, R_VCBUS(0x5981));
	else if (R_VCBUS_BIT(0x2783, 0, 6) == 0x40)
		pr_info("VPP2_WR_BAK_CTRL(0x%x):0x%x\n", 0x59c1, R_VCBUS(0x59c1));
	else
		pr_info("if has VPP1/VPP2 check VPP1/VPP2_WR_BAK_CTRL(0x5981 0x59c1)\n");
	pr_info("VDIN_COM_CTRL0:(0x%x):0x%x\n",
		(VDIN_COM_CTRL0 + devp->addr_offset),
		rd(devp->addr_offset, VDIN_COM_CTRL0));
	pr_info("VDIN_ASFIFO_CTRL3:(0x%x):0x%x\n",
		(VDIN_ASFIFO_CTRL3 + devp->addr_offset),
		rd(devp->addr_offset, VDIN_ASFIFO_CTRL3));
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
void vdin_dump_regs_t3x(struct vdin_dev_s *devp, u32 size)
{
	unsigned int reg;
	unsigned int offset = devp->addr_offset;

	pr_info("t3x:vdin%d TOP regs start----\n", devp->index);
	for (reg = VDIN_REG_TOP_START_T3X; reg <= VDIN_REG_TOP_END_T3X; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));
	pr_info("vdin%d TOP regs end----\n\n", devp->index);

	pr_info("vdin%d regs bank0 start----\n", devp->index);
	for (reg = VDIN_REG_BANK0_START_T3X; reg <= VDIN_REG_BANK0_END_T3X; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	pr_info("vdin%d regs bank0 end----\n\n", devp->index);

	pr_info("vdin%d regs bank1 start----\n", devp->index);
	for (reg = VDIN_REG_BANK1_START_T3X; reg <= VDIN_REG_BANK1_END_T3X; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	pr_info("vdin%d regs bank1 end----\n\n", devp->index);

	pr_info("t3x:vdin%d preproc regs start----\n", devp->index);
	for (reg = VPU_VDIN_HDMI0_CTRL0; reg <= VPU_VDIN_HDMI1_TUNNEL; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));
	pr_info("vdin%d TOP preproc end----\n\n", devp->index);

	pr_info("vdin%d regs loopback start----\n", devp->index);
	reg = VIU_WR_BAK_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));
	reg = VPP_POST_HOLD_CTRL_T3X;
	pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));
	reg = VPU_VIU2VDIN_HDN_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));
	reg = VPU_VIU_VDIN_IF_MUX_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));
	reg = VPU_VIU2VDIN0_BUF_SIZE_T3X;
	pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));
	//reg = S5_VPP_POST_HOLD_CTRL;
	//pr_info("0x%04x = 0x%08x\n", (reg + 0), rd(0, reg));

//	for (reg = 0x2700; reg <= 0x2800; reg++)
//	pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
//	reg = VPP1_WR_BAK_CTRL;
//	pr_info("VPP1,0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
//	reg = VPP2_WR_BAK_CTRL;
//	pr_info("VPP2,0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
//	pr_info("vdin%d regs loopback end----\n\n", devp->index);
}

void vdin_dump_regs_s5(struct vdin_dev_s *devp, u32 size)
{
	unsigned int reg;
	unsigned int offset = devp->addr_offset;

	pr_info("vdin%d regs bank0 start----\n", devp->index);
	for (reg = VDIN_REG_BANK0_START; reg <= VDIN_REG_BANK0_END; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	pr_info("vdin%d regs bank0 end----\n\n", devp->index);

	pr_info("vdin%d regs bank1 start----\n", devp->index);
	for (reg = VDIN_REG_BANK1_START; reg <= VDIN_REG_BANK1_END; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	pr_info("vdin%d regs bank1 end----\n\n", devp->index);

	pr_info("vdin%d regs loopback start----\n", devp->index);
	reg = VIU_WR_BAK_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	reg = VPU_VIU2VDIN_HDN_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	reg = VPU_VIU_VDIN_IF_MUX_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	reg = S5_VPP_POST_HOLD_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
//	for (reg = 0x2700; reg <= 0x2800; reg++)
//	pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
//	reg = VPP1_WR_BAK_CTRL;
//	pr_info("VPP1,0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
//	reg = VPP2_WR_BAK_CTRL;
//	pr_info("VPP2,0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
//	pr_info("vdin%d regs loopback end----\n\n", devp->index);
}
#endif

static void vdin_dump_regs(struct vdin_dev_s *devp, u32 size)
{
	unsigned int reg;
	unsigned int offset = devp->addr_offset;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	if (is_meson_s5_cpu()) {
		vdin_dump_regs_s5(devp, size);
		return;
	} else if (is_meson_t3x_cpu()) {
		vdin_dump_regs_t3x(devp, size);
		return;
	}
#endif
	if (size) {
		pr_info("vdin dump reg value:%d\n", size);
		if (size > 256)
			size = 256;
		for (reg = VDIN_SCALE_COEF_IDX;
		     reg <= VDIN_SCALE_COEF_IDX + size; reg++)
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		return;
	}

	pr_info("vdin%d regs start----\n", devp->index);
	for (reg = VDIN_SCALE_COEF_IDX; reg <= VDIN_COM_STATUS3; reg++)
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
	pr_info("vdin%d regs end----\n\n", devp->index);

#ifndef CONFIG_AMLOGIC_REMOVE_OLD
	if (is_meson_tm2_cpu()) {
		pr_info("vdin%d HDR2 regs start----\n", devp->index);
		for (reg = VDIN_HDR2_CTRL;
		     reg <= VDIN_HDR2_MATRIXO_EN_CTRL; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("vdin%d HDR2 regs end----\n\n", devp->index);

		pr_info("vdin descrambler scramble----start\n");
		for (reg = VDIN_DSC_CTRL; reg <= VDIN_DSC_TUNNEL_SEL; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("vdin descrambler scramble----end\n\n");

		pr_info("vdin%d DV regs start----\n", devp->index);
		for (reg = VDIN_DOLBY_DSC_CTRL0;
		     reg <= VDIN_DOLBY_DSC_STATUS2; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}

		pr_info("0x%04x = 0x%08x\n",
			(VDIN_DOLBY_DSC_STATUS3 + offset),
			rd(offset, VDIN_DOLBY_DSC_STATUS3));
		pr_info("vdin%d DV regs end----\n", devp->index);
	}
#endif
	if (devp->dtdata->hw_ver == VDIN_HW_T6X) {
		if (!devp->index) {
			pr_info("t6x aa ds regs begin\n");
			for (reg = VDIN0_AA_DS_CTRL + VDIN0_WRMIF_REG_OFFSET_T6X;
			     reg <= VDIN0_AA_DS_RO_DBG + VDIN0_WRMIF_REG_OFFSET_T6X; reg++) {
				pr_info("0x%04x = 0x%08x\n",
					(reg + offset), rd(offset, reg));
			}
			pr_info("t6x aa ds regs end\n");
		}
		pr_info("t6x wrmif regs begin\n");
		for (reg = VDIN0_WRMIF_FRM_EN_CTRL + VDIN0_WRMIF_REG_OFFSET_T6X;
		     reg <= VDIN0_WRMIF_MMU_CTRL + VDIN0_WRMIF_REG_OFFSET_T6X; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("t6x wrmif regs end\n");
		pr_info("t6x hist regs begin\n");
		pr_info("0x%04x = 0x%08x\n", 0x1292, R_VCBUS(VDIN_HIST_H_START_END_T6X));
		pr_info("0x%04x = 0x%08x\n", 0x1293, R_VCBUS(VDIN_HIST_V_START_END_T6X));
		pr_info("0x%04x = 0x%08x\n", 0x1297, R_VCBUS(VDIN_HIST_SPL_VAL_T6X));
		pr_info("t6x hist regs end\n");
	} else if (devp->dtdata->hw_ver == VDIN_HW_T6W) {
		if (!devp->index) {
			pr_info("t6w aa ds regs begin\n");
			for (reg = VDIN0_AA_DS_CTRL;
			     reg <= VDIN0_AA_DS_RO_DBG; reg++) {
				pr_info("0x%04x = 0x%08x\n",
					(reg + offset), rd(offset, reg));
			}
			pr_info("t6w aa ds regs end\n");
		}
		pr_info("t6w wrmif regs begin\n");
		for (reg = VDIN0_WRMIF_FRM_EN_CTRL;
		     reg <= VDIN0_WRMIF_MMU_CTRL; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("t6w wrmif regs end\n");
	} else if (devp->dtdata->hw_ver == VDIN_HW_T6D) {
		for (reg = VDIN_WRMIF_CTRL0;
		     reg <= VDIN_WRMIF_LUMA_BADDR; reg++) {
			pr_info("0x%04x = 0x%08x (t6d cfg_wrmif)\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("0x%04x = 0x%08x (t6d cfg_wrmif)\n",
			(VDIN_WRMIF_FRM_EN_CTRL + offset), rd(offset, VDIN_WRMIF_FRM_EN_CTRL));
		for (reg = VDIN_WRMIF_LUMA_CTRL1;
		     reg <= VDIN_WRMIF_RGBA_CTRL; reg++) {
			pr_info("0x%04x = 0x%08x (t6d cfg_wrmif)\n",
				(reg + offset), rd(offset, reg));
		}
		for (reg = VDIN_WRMIF_LUMA_X;
		     reg <= VDIN_WRMIF_LUMA_Y; reg++) {
			pr_info("0x%04x = 0x%08x (t6d cfg_wrmif)\n",
				(reg + offset), rd(offset, reg));
		}
		for (reg = VDIN_WRMIF_CHRM_BADDR;
		     reg <= VDIN_WRMIF_CHRM_CTRL1; reg++) {
			pr_info("0x%04x = 0x%08x (t6d cfg_wrmif)\n",
				(reg + offset), rd(offset, reg));
		}
	} else if (devp->dtdata->hw_ver >= VDIN_HW_T7) {
		for (reg = VDIN_WR_BADDR_LUMA;
		     reg <= VDIN_WR_STRIDE_CHROMA; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
	}

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_TM2)) {
		pr_info("vdin%d h/v shark regs start----\n", devp->index);
		reg = VDIN_VSHRK_SIZE_M1;
		pr_info("0x%04x = 0x%08x\n", (reg + offset), rd(offset, reg));
		for (reg = VDIN_HSK_CTRL; reg <= HSK_COEF_15; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg + offset), rd(offset, reg));
		}
		pr_info("vdin%d h/v sk regs end----\n\n", devp->index);

		reg = VDIN_TOP_DOUBLE_CTRL;
		pr_info("0x%04x = 0x%08x\n\n", (reg), R_VCBUS(reg));

		for (reg = VDIN2_WR_CTRL; reg <= VDIN_TOP_MEAS_RO_PIXB; reg++) {
			pr_info("0x%04x = 0x%08x\n",
				(reg), R_VCBUS(reg));
		}
	}

	if (devp->afbce_flag & VDIN_AFBCE_EN) {
		pr_info("vdin%d afbce regs start----\n", devp->index);
		if (devp->dtdata->hw_ver == VDIN_HW_T6W ||
			devp->dtdata->hw_ver == VDIN_HW_T6X) {
			for (reg = VFCE_CHNL0_ENABLE; reg <= VFCE_TOP1_STA_0;
				reg++) {
				pr_info("0x%04x = 0x%08x\n", (reg), R_VCBUS(reg));
			}
		} else {
			for (reg = AFBCE_ENABLE; reg <= AFBCE_MMU_RMIF_RO_STAT;
				reg++) {
				pr_info("0x%04x = 0x%08x\n", (reg), R_VCBUS(reg));
			}
		}
		pr_info("vdin%d afbce regs end----\n\n", devp->index);
	}
	if (devp->dtdata->hw_ver == VDIN_HW_T6W ||
		devp->dtdata->hw_ver == VDIN_HW_T6X) {
		pr_info("t6w ip pattern regs begin\n");
		for (reg = VDIN0_IP_PAT_CTRL; reg <= VDIN0_IP_PAT_GCLK_CTRL;
			reg++) {
			pr_info("0x%04x = 0x%08x\n", (reg), R_VCBUS(reg));
		}
		pr_info("t6w ip pattern regs end\n");
	}

	reg = VDIN_MISC_CTRL;
	pr_info("0x%04x = 0x%08x\n", (reg), R_VCBUS(reg));
	if (devp->index) {
		pr_info("write back reg ---\n");
		vdin_write_back_reg(devp);
	}
}

void vdin_test_front_end(void)
{
	struct tvin_frontend_s *fe = tvin_get_frontend(TVIN_PORT_HDMI0,
		VDIN_FRONTEND_IDX);

	if (fe && fe->sm_ops && fe->sm_ops->vdin_set_property)
		fe->sm_ops->vdin_set_property(fe);
}

int vdin_afbce_compression_ratio_monitor(struct vdin_dev_s *devp, struct vf_entry *vfe)
{
	unsigned int mmu_num, mif_size, src_size1, dst_frame_size, ratio1 = 0;
	unsigned int ratio2 = 0, src_size2;

	mmu_num = rd(devp->addr_offset, AFBCE_MMU_NUM);
	mif_size = rd_bits(devp->addr_offset, AFBCE_MIF_SIZE, 0, 16);
	dst_frame_size = mmu_num * mif_size;

	if (devp->prop.color_format == TVIN_RGB444 ||
		devp->prop.color_format == TVIN_YUV444)
		src_size1 = devp->h_active * devp->v_active * 3 * devp->prop.colordepth / 8;
	else if (devp->prop.color_format == TVIN_NV12 ||
			 devp->prop.color_format == TVIN_NV21)
		src_size1 =
			devp->h_active * devp->v_active * 3 / 2 * devp->prop.colordepth / 8;
	else
		src_size1 = devp->h_active * devp->v_active * 2 * devp->prop.colordepth / 8;

	if (src_size1)
		ratio1 = dst_frame_size * 100 / src_size1;

	if (vdin_is_convert_to_444(devp->format_convert))
		src_size2 = devp->h_active * devp->v_active * 3 * devp->source_bitdepth / 8;
	else if (vdin_is_convert_to_nv21(devp->format_convert))
		src_size2 =
			devp->h_active * devp->v_active * 3 / 2 * devp->source_bitdepth / 8;
	else
		src_size2 = devp->h_active * devp->v_active * 2 * devp->source_bitdepth / 8;

	if (src_size2)
		ratio2 = dst_frame_size * 100 / src_size2;

	if (devp->debug.vdin_isr_monitor & VDIN_ISR_MONITOR_AFBCE)
		pr_info("ratio_src=%d%%,ratio_afbc_in=%d%%,dst:%d(%ux%u),src1=%d,src2=%d;\n",
			ratio1, ratio2, dst_frame_size, mmu_num, mif_size, src_size1, src_size2);

	if (devp->dbg_afbce_monitor > 1 &&
		ratio2 >= devp->dbg_afbce_monitor &&
		devp->kthread) {
		devp->vfe_tmp = vfe;
		devp->dbg_afbce_monitor = 1;
		complete_all(&devp->thd_completion);
	}

	return 0;
}

static int vdin_task(void *data)
{
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	struct vdin_dev_s *devp = (struct vdin_dev_s *)(data);

	if (!devp) {
		pr_info("devp == NULL\n");
		return -1;
	}
	sched_setscheduler_nocheck(current, SCHED_FIFO, &param);
	allow_signal(SIGTERM);

	while (1) {
		wait_for_completion(&devp->thd_completion);
		if (kthread_should_stop() || devp->quit_flag) {
			pr_info("task: quit\n");
			break;
		}
		vdin_pause_dec(devp, 0);
		if (devp->vfe_tmp) {
			vdin_dump_one_afbce_mem("/mnt",  devp, devp->vfe_tmp->vf.index);
			devp->vfe_tmp = NULL;
		}
		vdin_resume_dec(devp, 0);
		pr_info("%s %d,wake up\n", __func__, __LINE__);
		usleep_range(9000, 10000);
	}

	while (!kthread_should_stop()) {
		/* may not call stop, wait..
		 * it is killed by SIGTERM,eixt on down_interruptible
		 * if not call stop,this thread may on do_exit and
		 * kthread_stop may not work good;
		 */
		/* msleep(10); */
		usleep_range(9000, 10000);
	}
	return 0;
}

int vdin_kthread_start(struct vdin_dev_s *devp)
{
	devp->quit_flag = false;
	devp->kthread = kthread_run(vdin_task, devp, "vdin_kthread");

	if (!IS_ERR_OR_NULL(devp->kthread))
		pr_info("vdin%u,kthread start ok", devp->index);
	else
		devp->kthread = NULL;

	return 0;
}

int vdin_kthread_stop(struct vdin_dev_s *devp)
{
	if (!IS_ERR_OR_NULL(devp->kthread)) {
		devp->quit_flag = true;
		complete_all(&devp->thd_completion);
		kthread_stop(devp->kthread);
		//devp->quit_flag = false;
		devp->kthread = NULL;
		pr_info("vdin%u,kthread stop", devp->index);
	}
	return 0;
}

static void vdin_read_rw_reg(struct vdin_dev_s *devp)
{
	int i;

	for (i = 0; i < DBG_REG_LENGTH; i++) {
		if (devp->debug.dbg_reg_addr[i])
			pr_info("read reg:[%#x]:%#x\n", devp->debug.dbg_reg_addr[i],
				rd(0, devp->debug.dbg_reg_addr[i]));
		else
			break;
	}
}

static void vdin_write_rw_reg(struct vdin_dev_s *devp)
{
	int i;

	for (i = 0; i < DBG_REG_LENGTH; i++) {
		if (devp->debug.dbg_reg_addr[i])
			vdin_wr(devp, devp->debug.dbg_reg_addr[i], devp->debug.dbg_reg_val[i]);
		else
			break;
	}
}

static void vdin_write_bit_rw_reg(struct vdin_dev_s *devp)
{
	int i;

	for (i = 0; (i * 4 + 3) < DBG_REG_LENGTH; i++) {
		if (devp->debug.dbg_reg_bit[i * 4])
			vdin_wr_bits(devp, devp->debug.dbg_reg_bit[i * 4],
				devp->debug.dbg_reg_bit[i * 4 + 1],
				devp->debug.dbg_reg_bit[i * 4 + 2],
				devp->debug.dbg_reg_bit[i * 4 + 3]);
		else
			break;
	}
}

/*
 * update_site:
 *	0: vdin_start_dec
 *	1: vdin_isr
 */
void vdin_dbg_access_reg(struct vdin_dev_s *devp, unsigned int update_site)
{
	if (!devp->debug.dbg_rw_reg_en)
		return;

	if (update_site) {
		if (devp->debug.dbg_rw_reg_en & DBG_REG_ISR_SET_BIT)
			vdin_write_bit_rw_reg(devp);

		if (devp->debug.dbg_rw_reg_en & DBG_REG_ISR_WRITE) {
			vdin_write_rw_reg(devp);
		} else if (devp->debug.dbg_rw_reg_en & DBG_REG_ISR_READ) {
			vdin_read_rw_reg(devp);
		} else if (devp->debug.dbg_rw_reg_en & DBG_REG_ISR_READ_WRITE) {
			vdin_write_rw_reg(devp);
			vdin_read_rw_reg(devp);
		}
	} else {
		if (devp->debug.dbg_rw_reg_en & DBG_REG_START_SET_BIT)
			vdin_write_bit_rw_reg(devp);

		if (devp->debug.dbg_rw_reg_en & DBG_REG_START_WRITE) {
			vdin_write_rw_reg(devp);
		} else if (devp->debug.dbg_rw_reg_en & DBG_REG_START_READ) {
			vdin_read_rw_reg(devp);
		} else if (devp->debug.dbg_rw_reg_en & DBG_REG_START_READ_WRITE) {
			vdin_write_rw_reg(devp);
			vdin_read_rw_reg(devp);
		}
	}

	if (devp->debug.dbg_rw_reg_en & DBG_REG_TOP_FUNCTION) {
		wr_bits(0x200, VDIN0_WRMIF_CTRL, 0,
			WR_REQ_EN_BIT, WR_REQ_EN_WID);
		wr(0x200, VDIN0_DW_IN_SIZE, 0x7800870);
		wr(0x200, VDIN0_DW_OUT_SIZE, 0x7800870);
		wr(0x200, VDIN0_WRMIF_H_START_END, 0x77f);
		wr_bits(0x200, VDIN0_CORE_CTRL, 1, 10, 1);
		wr_bits(0x200, VDIN0_CORE_CTRL, 0, 6, 1);
		wr(0x200, VDIN0_WRMIF_STRIDE_LUMA, (devp->canvas_max_stride / 2) >> 4);
		wr_bits(0, T3X_VDIN_TOP_CTRL, 1, 25, 1);//reg_vdin0to1_en
		wr_bits(0x200, VDIN0_WRMIF_CTRL, 1,
			WR_REQ_EN_BIT, WR_REQ_EN_WID);
		devp->debug.dbg_rw_reg_en &= DBG_REG_CONVERT_SYNC;
		devp->debug.dbg_reg_addr[0]  = 0;
		devp->debug.dbg_reg_val[0]   = 0;
		wr_bits(0, VDIN1_SYNC_CONVERT_SYNC_CTRL0, 1, 31, 1);
	} else if (devp->debug.dbg_rw_reg_en & DBG_REG_CONVERT_SYNC) {
		wr_bits(0, VDIN1_SYNC_CONVERT_SYNC_CTRL0, 1, 31, 1);
	}

	return;
}
#endif

void vdin_memset_dbg(char *parm[], struct vdin_dev_s *devp)
{
	unsigned int temp = 0, i = 0;

	if (parm[2] && kstrtouint(parm[2], 10, &temp) == 0) {
		devp->debug.flag = (enum vdin_memset_dbg_flag)temp;
		switch (devp->debug.flag) {
		case YUV422_8B_FULL:
			for (i = 0; i < 4; i++) {
				if (kstrtouint(parm[i + 3], 16, &temp) == 0) {
					devp->debug.yuv422_8f[i] = temp;
					pr_info("debug.yuv422_8f[%d] = 0x%02X\n",
						i, devp->debug.yuv422_8f[i]);
				}
			}
			break;
		case YUV422_8B_LIMIT:
			for (i = 0; i < 4; i++) {
				if (kstrtouint(parm[i + 3], 16, &temp) == 0) {
					devp->debug.yuv422_8l[i] = temp;
					pr_info("debug.yuv422_8l[%d] = 0x%02X\n",
						i, devp->debug.yuv422_8l[i]);
				}
			}
			break;
		case YUV422_10B_FULL:
			for (i = 0; i < 5; i++) {
				if (kstrtouint(parm[i + 3], 16, &temp) == 0) {
					devp->debug.yuv422_10f[i] = temp;
					pr_info("debug.yuv422_10f[%d] = 0x%02X\n",
						i, devp->debug.yuv422_10f[i]);
				}
			}
			break;
		case YUV422_10B_LIMIT:
			for (i = 0; i < 5; i++) {
				if (kstrtouint(parm[i + 3], 16, &temp) == 0) {
					devp->debug.yuv422_10l[i] = temp;
					pr_info("debug.yuv422_10l[%d] = 0x%02X\n",
						i, devp->debug.yuv422_10l[i]);
				}
			}
			break;
		case RGB_8B_FULL:
			for (i = 0; i < 3; i++) {
				if (kstrtouint(parm[i + 3], 16, &temp) == 0) {
					devp->debug.rgb_8f[i] = temp;
					pr_info("devp->debug.rgb_8f[%d] = 0x%02X\n",
						i, devp->debug.rgb_8f[i]);
				}
			}
			break;
		case RGB_8B_LIMIT:
			for (i = 0; i < 3; i++) {
				if (kstrtouint(parm[i + 3], 16, &temp) == 0) {
					devp->debug.rgb_8l[i] = temp;
					pr_info("devp->debug.rgb_8l[%d] = 0x%02X\n",
						i, devp->debug.rgb_8l[i]);
				}
			}
			break;
		}
	}
}

/*
 * 1.show the current frame rate
 * echo fps >/sys/class/vdin/vdinx/attr
 * 2.dump the data from vdin memory
 * echo capture dir >/sys/class/vdin/vdinx/attr
 * 3.start the vdin hardware
 * echo tvstart/v4l2start port fmt_id/resolution(width height frame_rate) >dir
 * 4.freeze the vdin buffer
 * echo freeze/unfreeze >/sys/class/vdin/vdinx/attr
 * 5.enable vdin0-nr path or vdin0-mem
 * echo output2nr >/sys/class/vdin/vdin0/attr
 * echo output2mem >/sys/class/vdin/vdin0/attr
 * 6.modify for vdin fmt & color fmt conversion
 * echo conversion w h cfmt >/sys/class/vdin/vdin0/attr
 */
static ssize_t attr_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t len)
{
	unsigned int fps = 0;
	char *buf_orig, *parm[47] = {NULL};
	struct vdin_dev_s *devp;
	long val = 0;
	unsigned int offset;
	int err = 0;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	ulong flags = 0;
	struct vdin_hist_s vdin1_hist_temp;
	unsigned int i = 0;
	char ret = 0;
	unsigned int time_start = 0, time_end = 0, time_delta;
	unsigned int temp = 0, addr = 0, val_tmp = 0;
	unsigned int mode = 0, flag = 0;

#endif

	if (!buf) {
		err = -1;
		goto error;
	}
	buf_orig = kstrdup(buf, GFP_KERNEL);
	devp = dev_get_drvdata(dev);
	if (!devp->vfp) {
		err = -2;
		goto error;
	}
	vdin_parse_param(buf_orig, (char **)&parm);
	offset = devp->addr_offset;
	if (!strncmp(parm[0], "fps", 3)) {
		if (devp->cycle)
			fps = (devp->msr_clk_val +
			       (devp->cycle >> 3)) / devp->cycle;
		pr_info("%u Hz (hdmirx %u Hz)\n", fps, devp->parm.info.fps);
	} else if (!strcmp(parm[0], "capture")) {
		if (parm[3]) {
			unsigned int start = 0, offset = 0;

			if (kstrtol(parm[2], 16, &val) == 0)
				start = val;
			if (kstrtol(parm[3], 16, &val) == 0)
				offset = val;
			dump_other_mem(devp, parm[1], start, offset);
		} else if (parm[2]) {
			unsigned int buf_num = 0;

			if (kstrtol(parm[2], 10, &val) == 0)
				buf_num = val;
			vdin_dump_one_buf_mem(parm[1], devp, buf_num);
		} else if (parm[1]) {
			vdin_dump_mem(parm[1], devp);
		}
	} else if (!strcmp(parm[0], "dump_picture")) {
		if (parm[1]) {
			unsigned int dump_num = 0;

			if (kstrtol(parm[2], 10, &val) == 0)
				dump_num = val;
			vdin_dump_more_mem(parm[1], devp, dump_num, 0);
		}
	}
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	else if (!strcmp(parm[0], "vdin_recycle_num")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->debug.vdin_recycle_num = val;
		}
	} else if (!strcmp(parm[0], "dump_start_pic")) {
		if (parm[1]) {
			unsigned int dump_num = 0;

			if (kstrtol(parm[2], 10, &val) == 0)
				dump_num = val;
			vdin_dump_more_mem(parm[1], devp, dump_num, 1);
		}
	} else if (!strcmp(parm[0], "cma_config_flag")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->cma_config_flag = val;
		}
	} else if (!strcmp(parm[0], "irq_en")) {
		if (!parm[1]) {
			pr_err("miss parameters .\n");
		} else if (kstrtol(parm[1], 10, &val) == 0) {
			if (val)
				enable_irq(devp->irq);
			else
				disable_irq(devp->irq);
		}
	} else if  (!strcmp(parm[0], "request_irq")) {
		snprintf(devp->irq_name, sizeof(devp->irq_name),
			 "vdin%d-irq", devp->index);
		if (!(devp->flags & VDIN_FLAG_ISR_REQ)) {
			ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
					  devp->irq_name, (void *)devp);
			disable_irq(devp->irq);
			devp->flags |= VDIN_FLAG_ISR_REQ;
		}
	} else if  (!strcmp(parm[0], "free_irq")) {
		free_irq(devp->irq, (void *)devp);
	} else if (!strcmp(parm[0], "tvstart")) {
		unsigned int port = 0, fmt = 0;

		if (!parm[2]) {
			kfree(buf_orig);
			return len;
		}
		if (kstrtol(parm[1], 16, &val) == 0)
			port = val;
		switch (port) {
		case 0:/* HDMI0 */
			port = TVIN_PORT_HDMI0;
			break;
		case 1:/* HDMI1 */
			port = TVIN_PORT_HDMI1;
			break;
		case 2:/* HDMI2 */
			port = TVIN_PORT_HDMI2;
			break;
		case 3:/* HDMI3 */
			port = TVIN_PORT_HDMI3;
			break;
		case 5:/* CVBS0 */
			port = TVIN_PORT_CVBS0;
			break;
		case 6:/* CVBS1 */
			port = TVIN_PORT_CVBS1;
			break;
		case 8:/* CVBS2 */
			port = TVIN_PORT_CVBS2;
			break;
		case 9:/* CVBS3 as atv demod to cvd */
			port = TVIN_PORT_CVBS3;
			break;
		default:
			port = TVIN_PORT_CVBS0;
			break;
		}
		if (kstrtol(parm[2], 16, &val) == 0)
			fmt = val;

		/* devp->flags |= VDIN_FLAG_FS_OPENED; */
		/* request irq */
		/*snprintf(devp->irq_name, sizeof(devp->irq_name),*/
		/*	 "vdin%d-irq", devp->index);*/
		/*pr_info("vdin work in normal mode\n");*/
		/*ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,*/
		/*		devp->irq_name, (void *)devp);*/

		/*if (vdin_dbg_en)*/
		/*	pr_info("%s vdin.%d request_irq\n", __func__,*/
		/*		devp->index);*/

		/*disable irq until vdin is configured completely*/
		/*disable_irq_nosync(devp->irq);*/

		/*init queue*/
		init_waitqueue_head(&devp->queue);
		/* remove the hardware limit to vertical [0-max]*/
		/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000fff); */
		/* pr_info("open device %s ok\n", dev_name(devp->dev)); */
		/* vdin_open_fe(port, 0, devp); */
		devp->parm.port = port;
		devp->parm.info.fmt = fmt;
		devp->fmt_info_p  = (struct tvin_format_s *)
				tvin_get_fmt_info(fmt);

		devp->unstable_flag = false;
		ret = vdin_open_fe(devp->parm.port, 0, devp);
		if (ret) {
			err = -4;
			goto error;
		}
		devp->flags |= VDIN_FLAG_DEC_OPENED;

		time_start = jiffies_to_msecs(jiffies);
start_chk:
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			err = -5;
			goto error;
		}
		time_end = jiffies_to_msecs(jiffies);
		time_delta = time_end - time_start;
		if ((devp->parm.info.status != TVIN_SIG_STATUS_STABLE ||
		     devp->parm.info.fmt == TVIN_SIG_FMT_NULL) &&
		    time_delta <= 5000)
			goto start_chk;

		if (devp->parm.info.fmt != TVIN_SIG_FMT_NULL)
			fmt = devp->parm.info.fmt; /* = parm.info.fmt; */
		devp->fmt_info_p  =
			(struct tvin_format_s *)tvin_get_fmt_info(fmt);
		/* devp->fmt_info_p  =
		 * tvin_get_fmt_info(devp->parm.info.fmt);
		 */
		if (!devp->fmt_info_p) {
			err = -6;
			goto error;
		}
		if (!(devp->flags & VDIN_FLAG_ISR_REQ)) {
			ret = request_irq(devp->irq, vdin_isr, IRQF_SHARED,
					  devp->irq_name, (void *)devp);
			disable_irq(devp->irq);
			/* for t7 meta data */
			if (devp->dtdata->hw_ver == VDIN_HW_T7) {
				ret = request_irq(devp->vdin2_meta_wr_done_irq,
						  vdin_wrmif2_dv_meta_wr_done_isr,
						  IRQF_SHARED,
						  devp->vdin2_meta_wr_done_irq_name,
						  (void *)devp);
				disable_irq(devp->vdin2_meta_wr_done_irq);
			}
			devp->flags |= VDIN_FLAG_ISR_REQ;
		}
		vdin_start_dec(devp);
		if (!(devp->flags & VDIN_FLAG_ISR_EN)) {
			/*enable irq */
			enable_irq(devp->irq);

			/* for t7 dv meta data */
			if (devp->vdin2_meta_wr_done_irq > 0)
				enable_irq(devp->vdin2_meta_wr_done_irq);

			devp->flags |= VDIN_FLAG_ISR_EN;
		}

		devp->flags |= VDIN_FLAG_DEC_STARTED;
		pr_info("tvstart port %s, decode started ok\n",
			tvin_port_str(devp->parm.port));
	} else if (!strcmp(parm[0], "start_dec")) {
		temp = devp->parm.info.fmt;
		pr_info("cur timing info:0x%x %s\n", temp,
			tvin_sig_fmt_str(devp->parm.info.fmt));
		devp->fmt_info_p =
			(struct tvin_format_s *)tvin_get_fmt_info(temp);

		vdin_start_dec(devp);
		/*enable irq */
		enable_irq(devp->irq);
		devp->flags |= VDIN_FLAG_DEC_STARTED;
	} else if (!strcmp(parm[0], "tv_stop")) {
		vdin_stop_dec(devp);
		vdin_close_fe(devp);
		/* devp->flags &= (~VDIN_FLAG_FS_OPENED); */
		devp->flags &= (~VDIN_FLAG_DEC_STARTED);
		/* free irq, free when close device */
		/* free_irq(devp->irq, (void *)devp); */

		/* reset the hardware limit to vertical [0-1079]  */
		/* WRITE_VCBUS_REG(VPP_PREBLEND_VD1_V_START_END, 0x00000437); */
	} else if (!strcmp(parm[0], "force_port")) {
		devp->debug.port = vdin_parse_port(parm[1]);
	} else if (!strcmp(parm[0], "v4l2stop")) {
		stop_tvin_service(devp->index);
		devp->flags &= (~VDIN_FLAG_V4L2_DEBUG);
	} else if (!strcmp(parm[0], "v4l2start")) {
		struct vdin_parm_s param;
		memset(&param, 0, sizeof(struct vdin_parm_s));

		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else {
			/*parse the port*/
			param.port = vdin_parse_port(parm[1]);
		}

		if (!parm[2] && !parm[3] && !parm[4] &&
			devp->hw_core == VDIN_HW_CORE_LITE) {
			vdin_get_input_size(devp, &param);
		} else {
			/*parse the resolution*/
			if (kstrtol(parm[2], 10, &val) == 0)
				param.h_active = val;
			if (kstrtol(parm[3], 10, &val) == 0)
				param.v_active = val;
			if (kstrtol(parm[4], 10, &val) == 0)
				param.frame_rate = val;
		}

		if (!parm[5])
			param.cfmt = TVIN_YUV422;
		else if (kstrtol(parm[5], 10, &val) == 0)
			param.cfmt = val;

		if (!parm[6])
			param.dfmt = TVIN_NV21;
		else if (kstrtol(parm[6], 10, &val) == 0)
			param.dfmt = val;

		if (!parm[7])
			param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE;
		else if (kstrtol(parm[7], 10, &val) == 0)
			param.scan_mode = val;

		if ((devp->flags & VDIN_FLAG_MANUAL_CONVERSION) &&
			devp->debug.scaling4w && devp->debug.scaling4h) {
			param.dest_h_active = devp->debug.scaling4w;
			param.dest_v_active = devp->debug.scaling4h;
			param.dfmt = devp->debug.dest_cfmt;
		} else {
			param.dest_h_active = param.h_active;
			param.dest_v_active = param.v_active;
		}
		param.fmt = TVIN_SIG_FMT_MAX;
		devp->flags |= VDIN_FLAG_V4L2_DEBUG;
		param.reserved |= PARAM_STATE_HISTGRAM;
		/* param.scan_mode = TVIN_SCAN_MODE_PROGRESSIVE; */
		/*start the vdin hardware*/

		start_tvin_service(devp->index, &param);
	} else if (!strcmp(parm[0], "disable_sm")) {
		del_timer_sync(&devp->timer);
	} else if (!strcmp(parm[0], "freeze")) {
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			err = -EINVAL;
			goto error;
		} else if (devp->fmt_info_p->scan_mode ==
			TVIN_SCAN_MODE_PROGRESSIVE) {
			vdin_vf_freeze(devp->vfp, 1);
		} else {
			vdin_vf_freeze(devp->vfp, 2);
		}
	} else if (!strcmp(parm[0], "unfreeze")) {
		if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
			err = -EINVAL;
			goto error;
		} else {
			vdin_vf_unfreeze(devp->vfp);
		}
	} else if (!strcmp(parm[0], "conversion")) {
		if (parm[1] && parm[2] && parm[3]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				devp->debug.scaling4w = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				devp->debug.scaling4h = val;
			if (kstrtoul(parm[3], 10, &val) == 0)
				devp->debug.dest_cfmt = val;
			devp->flags |= VDIN_FLAG_MANUAL_CONVERSION;
			devp->debug.conversion = true;
		} else {
			devp->flags &= (~VDIN_FLAG_MANUAL_CONVERSION);
			devp->debug.conversion = false;
		}
	} else if (!strcmp(parm[0], "counter")) {
		vdin_dump_count(devp);
	} else if (!strcmp(parm[0], "histgram")) {
		vdin_dump_histgram(devp);
#ifdef CONFIG_AML_LOCAL_DIMMING
	} else if (!strcmp(parm[0], "histgram_ldim")) {
		unsigned int hnum, vnum;

		if (parm[1] && parm[2]) {
			hnum = kstrtoul(parm[1], 10, (unsigned long *)&hnum);
			vnum = kstrtoul(parm[2], 10, (unsigned long *)&vnum);
		} else {
			hnum = 8;
			vnum = 2;
		}
		vdin_dump_histgram_ldim(devp, hnum, vnum);
#endif
	} else if (!strcmp(parm[0], "aspect_ratio_sar")) {
		if (parm[1] && parm[2] && parm[3]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				devp->debug.sar_width = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				devp->debug.sar_height = val;
			if (kstrtoul(parm[3], 10, &val) == 0)
				devp->debug.ratio_control = val;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "force_recycle")) {
		devp->flags |= VDIN_FLAG_FORCE_RECYCLE;
	} else if (!strcmp(parm[0], "read_pic_afbce")) {
		if (parm[1] && parm[2]) {
			vdin_write_afbce_mem(devp, parm[1], parm[2]);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "read_pic")) {
		if (parm[1] && parm[2]) {
			vdin_write_mem(devp, parm[1], parm[2], parm[3]);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "read_bin")) {
		if (parm[1] && parm[2] && parm[3]) {
			vdin_write_cont_mem(devp, parm[1], parm[2], parm[3]);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dump_reg")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			vdin_dump_regs(devp, temp);
		else
			vdin_dump_regs(devp, 0);
	} else if (!strcmp(parm[0], "mpeg2vdin")) {
		if (parm[1] && parm[2]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				devp->h_active = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				devp->v_active = val;
			vdin_set_mpegin(devp);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "hdr")) {
		int i;
		struct vframe_master_display_colour_s *prop;

		prop = &devp->curr_wr_vfe->vf.prop.master_display_colour;

		for (i = 0; i < 6; i++)
			pr_info("primaries %d: %#x\n", i,
				*(((u32 *)(prop->primaries)) + i));

		pr_info("flag:%d, white x:%#x, y:%#x, lum max:%#x, min:%#x\n",
			prop->present_flag,
			*((u32 *)(prop->white_point)),
			*((u32 *)(prop->white_point) + 1),
			*((u32 *)(prop->luminance)),
			*(((u32 *)(prop->luminance)) + 1));

	} else if (!strcmp(parm[0], "snow_on")) {
		unsigned int fmt;

		fmt = TVIN_SIG_FMT_CVBS_NTSC_M;
		devp->flags |= VDIN_FLAG_SNOW_FLAG;
		devp->flags |= VDIN_FLAG_SM_DISABLE;
		if (devp->flags & VDIN_FLAG_DEC_STARTED) {
			err = -EINVAL;
			goto error;
		} else {
			devp->parm.info.fmt = fmt;
			devp->fmt_info_p  =
				(struct tvin_format_s *)tvin_get_fmt_info(fmt);
			vdin_start_dec(devp);
			devp->flags |= VDIN_FLAG_DEC_STARTED;
		}
		devp->flags &= (~VDIN_FLAG_SM_DISABLE);
		/*tvafe_snow_config(1);*/
		/*tvafe_snow_config_clamp(1);*/
	} else if (!strcmp(parm[0], "snow_off")) {
		devp->flags &= (~VDIN_FLAG_SNOW_FLAG);
		devp->flags |= VDIN_FLAG_SM_DISABLE;
		/*tvafe_snow_config(0);*/
		/*tvafe_snow_config_clamp(0);*/
		/* if fmt change, need restart dec vdin */
		if (devp->parm.info.fmt != TVIN_SIG_FMT_CVBS_NTSC_M &&
		    devp->parm.info.fmt != TVIN_SIG_FMT_NULL) {
			if (!(devp->flags & VDIN_FLAG_DEC_STARTED)) {
				err = -EINVAL;
				goto error;
			} else {
				devp->flags |= VDIN_FLAG_DEC_STOP_ISR;
				vdin_stop_dec(devp);
				devp->flags &= ~VDIN_FLAG_DEC_STOP_ISR;
				devp->flags &= (~VDIN_FLAG_DEC_STARTED);
			}
			if (devp->flags & VDIN_FLAG_DEC_STARTED) {
				err = -4;
				goto error;
			} else {
				devp->fmt_info_p  =
				(struct tvin_format_s *)
				tvin_get_fmt_info(devp->parm.info.fmt);
				vdin_start_dec(devp);
				devp->flags |= VDIN_FLAG_DEC_STARTED;
			}
		}
		devp->flags &= (~VDIN_FLAG_SM_DISABLE);
	} else if (!strcmp(parm[0], "vf_reg")) {
		if ((devp->flags & VDIN_FLAG_DEC_REGISTERED)
			== VDIN_FLAG_DEC_REGISTERED) {
			err = -EINVAL;
			goto error;
		} else {
			devp->flags |= VDIN_FLAG_DEC_REGISTERED;
			vdin_vf_reg(devp);
		}
	} else if (!strcmp(parm[0], "vf_unreg")) {
		if ((devp->flags & VDIN_FLAG_DEC_REGISTERED)
			!= VDIN_FLAG_DEC_REGISTERED) {
			err = -EINVAL;
			goto error;
		} else {
			devp->flags &= (~VDIN_FLAG_DEC_REGISTERED);
			vdin_vf_unreg(devp);
		}
	} else if (!strcmp(parm[0], "pause_dec")) {
		vdin_pause_dec(devp, 0);
	} else if (!strcmp(parm[0], "resume_dec")) {
		vdin_resume_dec(devp, 0);
	} else if (!strcmp(parm[0], "pause_mif_dec")) {
		vdin_pause_dec(devp, 1);
	} else if (!strcmp(parm[0], "resume_mif_dec")) {
		vdin_resume_dec(devp, 1);
	} else if (!strcmp(parm[0], "pause_afbce_dec")) {
		vdin_pause_dec(devp, 2);
	} else if (!strcmp(parm[0], "resume_afbce_dec")) {
		vdin_resume_dec(devp, 2);
	} else if (!strcmp(parm[0], "color_depth")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			if (val == 0)
				devp->color_depth_config = COLOR_DEEPS_AUTO;
			else
				devp->color_depth_config =
					val | COLOR_DEEPS_MANUAL;
		}
	} else if (!strcmp(parm[0], "color_depth_support")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->color_depth_support = val;
		}
	} else if (!strcmp(parm[0], "force_stop_frame_num")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->dbg_force_stop_frame_num = val;
		}
	} else if (!strcmp(parm[0], "force_disp_skip_num")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->force_disp_skip_num = val;
		}
	} else if (!strcmp(parm[0], "force_one_buffer")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->dbg_force_one_buffer = val;
		}
	} else if (!strcmp(parm[0], "no_wr_check")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->dts_config.chk_write_done_en = val;
		}
	} else if (!strcmp(parm[0], "skip_isr")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->vdin_isr_drop = val;
		}
	} else if (!strcmp(parm[0], "fr_ctl")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->dbg_fr_ctl = val;
		}
	} else if (!strcmp(parm[0], "full_pack")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->full_pack = val;
		}
	} else if (!strcmp(parm[0], "flags_isr")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->flags_isr = val;
		}
	} else if (!strcmp(parm[0], "flags")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->flags = val;
		}
	} else if (!strcmp(parm[0], "force_malloc_yuv_422_to_444")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			if (val)
				devp->force_malloc_yuv_422_to_444 = 1;
			else
				devp->force_malloc_yuv_422_to_444 = 0;
		}
	} else if (!strcmp(parm[0], "auto_cut_window_en")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->auto_cut_window_en = val;
		}
	} else if (!strcmp(parm[0], "dolby_config")) {
		vdin_dolby_config(devp);
	} else if (!strcmp(parm[0], "metadata")) {
		dump_dolby_metadata(devp);
	} else if (!strcmp(parm[0], "clean_dv")) {
		dump_dolby_buf_clean(devp);
	} else if (!strcmp(parm[0], "dv_debug")) {
		vdin_dv_debug(devp);
	} else if (!strcmp(parm[0], "channel_order_config")) {
		unsigned int c0, c1, c2;

		if (!parm[3]) {
			err = -EINVAL;
			goto error;
		} else {
			c0 = 0;
			c1 = 0;
			c2 = 0;
			if (kstrtoul(parm[1], 10, &val) == 0)
				c0 = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				c1 = val;
			if (kstrtoul(parm[3], 10, &val) == 0)
				c2 = val;
			vdin_channel_order_config(devp->addr_offset,
						  c0, c1, c2);
		}
	} else if (!strcmp(parm[0], "channel_order_status")) {
		vdin_channel_order_status(devp->addr_offset);
	} else if (!strcmp(parm[0], "open_port")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 16, &val) == 0) {
			devp->parm.index = 0;
			devp->parm.port  = val;
			devp->unstable_flag = false;
			ret = vdin_open_fe(devp->parm.port,
					   devp->parm.index, devp);
			if (ret) {
				err = -4;
				goto error;
			}
		}
	} else if (!strcmp(parm[0], "close_port")) {
		if (!(devp->flags & VDIN_FLAG_DEC_OPENED)) {
			err = -EINVAL;
			goto error;
		} else {
			vdin_close_fe(devp);
		}
	} else if (!strcmp(parm[0], "prehsc_en")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->pre_h_scale_en = val;
		}
	} else if (!strcmp(parm[0], "vshrk_en")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->v_shrink_en = val;
		}
	} else if (!strcmp(parm[0], "cma_mem_mode")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->cma_mem_mode = val;
		}
	} else if (!strcmp(parm[0], "black_bar_enable")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->black_bar_enable = val;
		}
	} else if (!strcmp(parm[0], "hist_bar_enable")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->hist_bar_enable = val;
		}
	} else if (!strcmp(parm[0], "use_frame_rate")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->use_frame_rate = val;
		}
	} else if (!strcmp(parm[0], "dolby_input")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->dv.dolby_input = val;
		}
	} else if (!strcmp(parm[0], "rdma_enable")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->rdma_enable = val;
		}
	} else if (!strcmp(parm[0], "rdma_wr_reg")) {
		if (!parm[1] || !parm[2]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtouint(parm[1], 0, &addr) == 0 &&
			kstrtouint(parm[2], 0, &val_tmp) == 0) {
			rdma_write_reg(devp->rdma_handle, addr, val);
		}
	} else if (!strcmp(parm[0], "urgent_en")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->dts_config.urgent_en = val;
		}
	} else if (!strcmp(parm[0], "dynamic_crop")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->dts_config.dynamic_crop_en = val;
		}
	} else if (!strcmp(parm[0], "skip_vf_num")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if ((kstrtoul(parm[1], 10, &val) == 0) && (devp->vfp)) {
			devp->vfp->skip_vf_num = val;
			if (val == 0)
				memset(devp->vfp->disp_mode, 0,
				       (sizeof(enum vframe_disp_mode_e) *
					VFRAME_DISP_MAX_NUM));
		}
	} else if (!strcmp(parm[0], "dump_afbce")) {
		if (parm[2]) {
			unsigned int buf_num = 0;

			if (kstrtol(parm[2], 10, &val) == 0)
				buf_num = val;
			vdin_dump_one_afbce_mem(parm[1], devp, buf_num);
		} else if (parm[1]) {
			vdin_dump_one_afbce_mem(parm[1], devp, 0);
		}
	} else if (!strcmp(parm[0], "dbg_dump_frames")) {
		if (parm[1]) {
			if (kstrtol(parm[1], 10, &val) == 0)
				devp->dbg_dump_frames = val;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "no_swap_en")) {
		if (parm[1]) {
			if (kstrtol(parm[1], 0, &val) == 0)
				devp->dbg_no_swap_en = val;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dbg_stop_dec_delay")) {
		if (parm[1]) {
			if (kstrtol(parm[1], 10, &val) == 0)
				devp->dbg_stop_dec_delay = val;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "skip_frame_debug")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &skip_frame_debug) == 0)
				pr_info("set skip_frame_debug: %d\n",
					skip_frame_debug);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "max_ignore_cnt")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &max_ignore_frame_cnt) == 0)
				pr_info("set max_ignore_frame_cnt: %d\n",
					max_ignore_frame_cnt);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "afbce_flag")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 16, &temp) == 0) {
				devp->dts_config.afbce_flag_cfg = temp;
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "afbce_mode")) {
		if (parm[2]) {
			if ((kstrtouint(parm[1], 10, &mode) == 0) &&
			    (kstrtouint(parm[2], 10, &flag) == 0)) {
				vdin0_afbce_debug_force = flag;
				if (devp->afbce_flag & VDIN_AFBCE_EN)
					devp->afbce_mode = mode;
				else
					devp->afbce_mode = 0;
				pr_info("set vdin_afbce_mode: %d force:%d\n",
					devp->afbce_mode, vdin0_afbce_debug_force);
			}
		} else if (parm[1]) {
			if (kstrtouint(parm[1], 10, &mode) == 0) {
				if (devp->afbce_flag & VDIN_AFBCE_EN)
					devp->afbce_mode = mode;
				else
					devp->afbce_mode = 0;
				pr_info("set vdin_afbce_mode: %d\n",
					devp->afbce_mode);
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdi6_afifo_overflow")) {
		pr_info("vdi6_afifo_overflow: %d\n",
			vdin_check_vdi6_afifo_overflow(devp->addr_offset));
	} else if (!strcmp(parm[0], "vdi6_afifo_clear")) {
		vdin_clear_vdi6_afifo_overflow_flg(devp->addr_offset);
	} else if (!strcmp(parm[0], "force_disp_mode")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 0, &devp->debug.force_disp_mode)
				== 0)
				pr_info("force_disp_mode: %#x\n",
					devp->debug.force_disp_mode);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_mtx")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &temp) == 0  &&
			    kstrtouint(parm[2], 10, &mode) == 0) {
				if (temp == VDIN_SEL_MATRIX0)
					vdin_change_matrix0(offset, mode);
				else if (temp == VDIN_SEL_MATRIX1)
					vdin_change_matrix1(offset, mode);
				else if (temp == VDIN_SEL_MATRIX_HDR)
					vdin_change_matrix_hdr(offset, mode);
			}
		}
	} else if (!strcmp(parm[0], "wr_frame_en")) {
		if (parm[1]) {
			if (kstrtouint(parm[1], 10, &devp->vframe_wr_en) == 0)
				pr_info("vdin.%d vframe_wr_en %d\n",
					devp->index, devp->vframe_wr_en);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dv_mask")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0))
			devp->dts_config.dv_mask = temp;
	} else if (!strcmp(parm[0], "get_hist")) {
		pr_info("sum:0x%lx, width:%d, height:%d ave:0x%x\n",
			vdin1_hist.sum,
			vdin1_hist.width, vdin1_hist.height,
			vdin1_hist.ave);
	} else if (!strcmp(parm[0], "vf_crc")) {
		u32 ro_crc;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		if (is_meson_s5_cpu())
			ro_crc = rd(devp->addr_offset, VDIN_TOP_CRC1_OUT);
		else if (is_meson_t3x_cpu())
			ro_crc = rd(devp->addr_offset, VDIN0_PP_CRC_OUT);
		else
#endif
			ro_crc = rd(devp->addr_offset, VDIN_RO_CRC);
		if (devp->vfp && devp->vfp->last_last_vfe)
			pr_info("ro crc:0x%x, vf crc:0x%x\n",
				ro_crc, devp->vfp->last_last_vfe->vf.crc);
	} else if (!strcmp(parm[0], "game_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp) {
				devp->debug.bypass_game_mode = true;
				if (temp <= 0xb) {
					vdin_force_game_mode = temp;
					vdin_game_mode_chg(devp, game_mode, 1);
					game_mode = 1;
				} else {
					vdin_force_game_mode = 0;
					vdin_game_mode_chg(devp, game_mode, 0);
					game_mode = 0;
				}
			} else {
				vdin_game_mode_chg(devp, game_mode, 0);
				devp->debug.bypass_game_mode = false;
				vdin_force_game_mode = 0;
				game_mode = 0;
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "game_mode_chg")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			vdin_game_mode_chg(devp, game_mode, temp);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dyn_fmt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			spin_lock_irqsave(&devp->isr_lock, flags);
			devp->debug.force_fmt = temp;
			vdin_dyn_fmt(devp);
			spin_unlock_irqrestore(&devp->isr_lock, flags);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "auto_pc_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp) {
				vdin_pc_mode = 1;
				devp->vdin_pc_mode = vdin_pc_mode;
				//devp->debug.bypass_pc_mode = true;
				devp->flags |= VDIN_FLAG_DYN_RECONFIG;
				devp->game_mode_dly = vdin_game_mode_dly;
			} else {
				vdin_pc_mode = 0;
				devp->vdin_pc_mode = vdin_pc_mode;
				//devp->debug.bypass_pc_mode = false;
				devp->flags |= VDIN_FLAG_DYN_RECONFIG;
				devp->game_mode_dly = vdin_game_mode_dly;
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "pc_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp) {
				devp->debug.bypass_pc_mode = true;
				if (temp == 1)
					vdin_pc_mode = 1;
				else
					vdin_pc_mode = 0;
			} else {
				devp->debug.bypass_pc_mode = false;
				vdin_pc_mode = 0;
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "game_frc")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp) {
				devp->debug.bypass_game_frc = false;
				devp->vdin_game_frc = 1;

				if (devp->flags & VDIN_FLAG_DEC_STARTED &&
				   (devp->vdin_function_sel & VDIN_SELF_STOP_START)) {
					//_video_set_disable(1);
					devp->self_stop_start = 1;
					vdin_self_stop_dec(devp);
					vdin_self_start_dec(devp);
					devp->self_stop_start = 0;
				}
			} else {
				devp->debug.bypass_game_frc = true;
				devp->vdin_game_frc = 0;
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "invalid_input_en")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp)
				devp->debug.invalid_input_en = true;
			else
				devp->debug.invalid_input_en = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "bypass_secure_check")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp)
				devp->debug.bypass_secure_check = true;
			else
				devp->debug.bypass_secure_check = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "skip_samba_csc")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp)
				devp->debug.skip_samba_csc = true;
			else
				devp->debug.skip_samba_csc = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "v4l2_buff_area")) {
		/*
		 * 0: codec_mm_cma area
		 * 1: CMA reserved area
		 * 2: vdin1_cma area
		 */
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->debug.v4l2_buff_area = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vrr_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			devp->vrr_mode = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "matrix_pattern")) {
		/*
		 * 0:off 1:enable
		 */
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->debug.matrix_pattern_mode = temp;
			vdin_set_matrix_color(devp);
			/* pr_info("matrix_pattern_mode:%d\n", devp->matrix_pattern_mode); */
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "bist_set")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			val = temp;
			vdin_set_bist_pattern(devp, val);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "ip_patgen")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.ip_pat_en = temp;
			val = temp;
			vdin_set_ip_pattern(devp, val);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin1_hist_on_off")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			/*
			 * 0:off 1:enable
			 */
			if (temp) {
				if (devp->hw_core == VDIN_HW_CORE_LITE) {
					viuin_select_loopback_path();
					vdin1_hw_hist_on_off(devp, TRUE);
					devp->flags |= VDIN_FLAG_HIST_STARTED;
					vdin_ioctl_get_hist(devp, &vdin1_hist_temp);
				} else {
					err = -4;
					goto error;
				}
			} else {
				viuin_clear_loopback_path();
				vdin1_hw_hist_on_off(devp, FALSE);
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin1_hist_get")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			vdin_ioctl_get_hist(devp, &vdin1_hist_temp);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "pause_num")) {
		/*
		 * 0:off 1:enable
		 */
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->pause_num = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "hv_reverse_en")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->hv_reverse_en = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "double_write")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->double_wr_cfg = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "secure_mem")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp) {
				devp->secure_en = 1;
				devp->cma_config_flag = 0x1;
			} else {
				devp->secure_en = 0;
				devp->cma_config_flag = 0x101;
			}
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin1_bypass_hdr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp)
				devp->debug.vdin1_set_hdr_bypass = true;
			else
				devp->debug.vdin1_set_hdr_bypass = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "rv")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			pr_info("addr:0x%x val:0x%x\n", temp, R_VCBUS(temp));
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "wr")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0) &&
			(kstrtouint(parm[2], 16, &val_tmp) == 0)) {
			W_VCBUS(temp, val_tmp);
			pr_info("write addr:%#x val:%#x(%#x)\n", temp, val_tmp, R_VCBUS(temp));
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "wr_bit")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &addr) == 0) &&
			(kstrtouint(parm[2], 16, &temp) == 0) &&
			(kstrtouint(parm[3], 16, &time_start) == 0) &&
			(kstrtouint(parm[4], 16, &time_end) == 0)) {
			wr_bits(0, addr, temp, time_start, time_end);
			pr_info("write addr:%#x val:%#x(%#x) start:%#x end:%#x\n",
				temp, val_tmp, rd_bits(0, addr, time_start, time_end),
				time_start, time_end);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "rw_reg")) {
		memset(devp->debug.dbg_reg_addr, 0,
			(sizeof(devp->debug.dbg_reg_addr[0]) * DBG_REG_LENGTH));
		memset(devp->debug.dbg_reg_val, 0,
			(sizeof(devp->debug.dbg_reg_val[0]) * DBG_REG_LENGTH));
		if (parm[1] && parm[2]) {
			if (kstrtouint(parm[1], 0, &temp) == 0) {
				if (temp)
					devp->debug.dbg_rw_reg_en |= temp;
				else
					devp->debug.dbg_rw_reg_en = 0;
			}

			if (devp->debug.dbg_rw_reg_en & DBG_REG_START_WRITE ||
			    devp->debug.dbg_rw_reg_en & DBG_REG_ISR_WRITE ||
			    devp->debug.dbg_rw_reg_en & DBG_REG_START_READ_WRITE ||
			    devp->debug.dbg_rw_reg_en & DBG_REG_ISR_READ_WRITE) {
				for (i = 0; i < DBG_REG_LENGTH && parm[i * 2 + 2] &&
				     parm[i * 2 + 3]; i++) {
					if (kstrtouint(parm[i * 2 + 2], 0, &temp) == 0 &&
					    kstrtouint(parm[i * 2 + 3], 0, &val_tmp) == 0) {
						devp->debug.dbg_reg_addr[i] = temp;
						devp->debug.dbg_reg_val[i] = val_tmp;
					} else {
						break;
					}
				}
			} else if (devp->debug.dbg_rw_reg_en & DBG_REG_START_READ ||
				devp->debug.dbg_rw_reg_en & DBG_REG_ISR_READ) {
				for (i = 0; i < DBG_REG_LENGTH && parm[i + 2]; i++) {
					if (kstrtouint(parm[i + 2], 0, &temp) == 0)
						devp->debug.dbg_reg_addr[i] = temp;
					else
						break;
				}
			}
		} else {
			err = -EINVAL;
			goto error;
		}
		pr_info("vdin%d,reg_en:%#x,reg_num:%d\n",
			devp->index, devp->debug.dbg_rw_reg_en, i);
		for (i = 0; i < DBG_REG_LENGTH && devp->debug.dbg_reg_addr[i]; i++)
			pr_info("reg:[%#x]:%#x(%#x)\n", devp->debug.dbg_reg_addr[i],
				R_VCBUS(devp->debug.dbg_reg_addr[i]),
				devp->debug.dbg_reg_val[i]);
	} else if (!strcmp(parm[0], "rw_bit")) {
		memset(devp->debug.dbg_reg_bit, 0,
			(sizeof(devp->debug.dbg_reg_addr[0]) * DBG_REG_LENGTH));
		if (parm[1] && parm[2]) {
			if (kstrtouint(parm[1], 0, &temp) == 0) {
				if (temp)
					devp->debug.dbg_rw_reg_en |= temp;
				else
					devp->debug.dbg_rw_reg_en = 0;
			}

			if (devp->debug.dbg_rw_reg_en & DBG_REG_START_SET_BIT ||
			    devp->debug.dbg_rw_reg_en & DBG_REG_ISR_SET_BIT) {
				for (i = 0; (i * 4 + 3) < DBG_REG_LENGTH && parm[i * 4 + 2] &&
				     parm[i * 4 + 3] && parm[i * 4 + 4] && parm[i * 4 + 5]; i++) {
					if (kstrtouint(parm[i * 4 + 2], 0, &temp) == 0 &&
					    kstrtouint(parm[i * 4 + 3], 0, &val_tmp) == 0 &&
					    kstrtouint(parm[i * 4 + 4], 0, &time_start) == 0 &&
					    kstrtouint(parm[i * 4 + 5], 0, &time_end) == 0) {
						devp->debug.dbg_reg_bit[i * 4] = temp;
						devp->debug.dbg_reg_bit[i * 4 + 1] = val_tmp;
						devp->debug.dbg_reg_bit[i * 4 + 2] = time_start;
						devp->debug.dbg_reg_bit[i * 4 + 3] = time_end;
					} else {
						break;
					}
				}
			}
		} else {
			devp->debug.dbg_rw_reg_en = 0;
			err = -EINVAL;
			goto error;
		}
		pr_info("vdin%d,reg_en:%d,reg_num:%d\n", devp->index, devp->debug.dbg_rw_reg_en, i);
		for (i = 0; (i * 4 + 3) < DBG_REG_LENGTH && devp->debug.dbg_reg_bit[i * 4]; i++)
			pr_info("reg:[%#x]:val:%#x(%#x) start:%d len:%d\n",
				devp->debug.dbg_reg_bit[i * 4],
				devp->debug.dbg_reg_bit[i * 4 + 1],
				R_VCBUS(devp->debug.dbg_reg_bit[i * 4]),
				devp->debug.dbg_reg_bit[i * 4 + 2],
				devp->debug.dbg_reg_bit[i * 4 + 3]);
	} else if (!strcmp(parm[0], "self_restart")) {
		vdin_self_stop_dec(devp);
		vdin_self_start_dec(devp);
	} else if (!strcmp(parm[0], "vdin_function_sel")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			devp->vdin_function_sel = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "rgb_info_enable")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->debug.rgb_info_enable = true;
			else
				devp->debug.rgb_info_enable = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "delay_line_num")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->delay_line_num = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "enable_reset")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->enable_reset = true;
			else
				devp->enable_reset = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vsync_reset_mask")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->vsync_reset_mask = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dv_dbg_log")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dv_dbg_log = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dv_dbg_log_du")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dv_dbg_log_du = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_ctl_dbg")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.vdin_ctl_dbg = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_isr_monitor")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.vdin_isr_monitor = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_get_prop_in_fe_en")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp)
				devp->vdin_get_prop_in_fe_en = true;
			else
				devp->vdin_get_prop_in_fe_en = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "work_mode_simple")) {
		if (parm[1] && (kstrtouint(parm[1], 16, &temp) == 0)) {
			if (temp)
				devp->work_mode_simple = true;
			else
				devp->work_mode_simple = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dv_work_dolby")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dv_work_dolby = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "sct_print_ctl")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.sct_print_ctl = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_dbg_en")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.vdin_dbg_en = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_delay_num")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.vdin_delay_num = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_time_en")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->debug.vdin_time_en = true;
			else
				devp->debug.vdin_time_en = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_frame_work_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.vdin_frame_work_mode = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "sleep_time")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.sleep_time = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "sm_debug_enable")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.sm_debug_enable = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dmc_ctrl")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			vdin_dmc_ctrl(devp, !!temp);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "manual_change_csc")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.manual_change_csc = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "afbce_monitor")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dbg_afbce_monitor = temp;
			if (!devp->kthread && devp->dbg_afbce_monitor > 1)
				vdin_kthread_start(devp);
			else if (devp->kthread && !devp->dbg_afbce_monitor)
				vdin_kthread_stop(devp);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "debug_config")) {
		vdin_dump_debug_config(devp);
	} else if (!strcmp(parm[0], "change_get_drm")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->debug.change_get_drm = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "bypass_update_prop")) {
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0)) {
			devp->debug.bypass_update_prop = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	}  else if (!strcmp(parm[0], "force_shrink")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_force_shrink_en = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "select_matrix")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_sel_mat = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_dbg_cntl")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_print_cntl = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dbg_de_interlanced_ctl")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_de_interlanced_ctl = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_drop_num")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->vdin_drop_num = temp;
			pr_info("vdin_drop_num:%d\n", devp->vdin_drop_num);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_isr_drop_num")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->vdin_isr_drop_num = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_pcs_reset_threshold")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->vdin_input_data_threshold = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "report_size_abnormal_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->report_size_abnormal_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "invert_top_bot")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->debug.invert_top_bot = true;
			else
				devp->debug.invert_top_bot = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "color_range_force")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				color_range_force = temp;
			else
				color_range_force = 0;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "bypass_tunnel")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->debug.force_bypass_tunnel = true;
			else
				devp->debug.force_bypass_tunnel = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "yuv422_2plane")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp == 0)
				devp->yuv422_2plane_en = 0;
			else
				devp->yuv422_2plane_en =
					temp | YUV422_2PLANE_MANUAL;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "force_afbce_422_12bit")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.force_afbce_422_12bit = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "invert_secure")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->debug.invert_secure = true;
			else
				devp->debug.invert_secure = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "bypass_filter_vsync")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.bypass_filter_vsync = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "set_unstable_sig")) {
		devp->parm.info.status = TVIN_SIG_STATUS_UNSTABLE;
		devp->frame_drop_num = 8;
	} else if (!strcmp(parm[0], "cr_lossy")) {
		if (parm[1] && parm[2] && parm[3] && parm[4]) {
			if (kstrtouint(parm[1], 0, &temp) == 0)
				devp->cr_lossy_param.lossy_mode = temp;
			if (kstrtouint(parm[2], 0, &temp) == 0)
				devp->cr_lossy_param.quant_diff_root_leave = temp;
			if (kstrtouint(parm[3], 0, &temp) == 0)
				devp->cr_lossy_param.cr_lossy_target_byte = temp;
			if (kstrtouint(parm[4], 0, &temp) == 0)
				devp->cr_lossy_param.mmu_en = temp;
			pr_info("vdin%d,lossy_mode:%d,root_leave:%d;%d %d\n", devp->index,
				devp->cr_lossy_param.lossy_mode,
				devp->cr_lossy_param.quant_diff_root_leave,
				devp->cr_lossy_param.cr_lossy_target_byte,
				devp->cr_lossy_param.mmu_en);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_dmc_notifier")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &devp->debug.dbg_device_id) == 0)) {
			vdin_reg_dmc_notifier(devp->index);
		} else {
			vdin_unreg_dmc_notifier(devp->index);
		}
	} else if (!strcmp(parm[0], "dbg_dv_hw5")) {
		if (parm[1] && parm[2] && parm[3] && parm[4]) {
			if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
				devp->debug.dbg_dv_hw5 = temp;
				devp->dv_hw5.hw5_ctl = temp;
			}
			if (parm[2] && (kstrtouint(parm[2], 0, &temp) == 0))
				devp->dv_hw5.dw_out_w = temp;
			if (parm[3] && (kstrtouint(parm[3], 0, &temp) == 0))
				devp->dv_hw5.dw_out_h = temp;
			if (parm[4] && (kstrtouint(parm[4], 0, &temp) == 0))
				devp->dv_hw5.dw_dfmt = temp;
			pr_info("vdin%d:dv_hw5:%#x;%d,%d,%d\n",
				devp->index,
				devp->dv_hw5.hw5_ctl,
				devp->dv_hw5.dw_out_w, devp->dv_hw5.dw_out_h,
				devp->dv_hw5.dw_dfmt);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "hconv_mode")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 10, &val) == 0) {
			devp->debug.hconv_mode = val;
		}
	} else if (!strcmp(parm[0], "set_buf_num")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->frame_buff_num = val;
			devp->frame_buff_num_bak = val;
		}
	} else if (!strcmp(parm[0], "vdin_mem_memset")) {
		devp->debug.vdin_memset_dbg_en = 1;
		if (parm[1] && (kstrtouint(parm[1], 10, &temp) == 0))
			devp->debug.vdin_memset_en = temp;
		if (parm[2])
			vdin_memset_dbg(parm, devp);
	} else if (!strcmp(parm[0], "vdin_manual_req_mem_size")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.vdin_manual_req_mem_size = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "force_convert")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_force_convert = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "force_type")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_force_type = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "reg_addr")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.reg_addr = temp;
			pr_info("vdin%d,reg_addr = %#x\n",
				devp->index, devp->debug.reg_addr);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "bypass_game_dyn_fmt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->debug.bypass_game_dyn_fmt = true;
			else
				devp->debug.bypass_game_dyn_fmt = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "sct_remain_size")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.sct_remain_size = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_mut_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_mut_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "force_pause_en")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->debug.force_pause_en = true;
			else
				devp->debug.force_pause_en = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "dbg_force_duration")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_force_duration = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "game_frc_mute_time")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->game_frc_mute_time = temp; //ms
		} else {
			err = -EINVAL;
			goto error;
		}
	}
#endif
	else if (!strcmp(parm[0], "state")) {
		vdin_dump_state(devp);
	} else {
		pr_info("unknown command:%s\n", parm[0]);
	}

	kfree(buf_orig);
	return len;
error:
	pr_err("vdin set param error: %d\n", err);
	return len;
}

static DEVICE_ATTR_WO(attr);
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT

#ifdef VF_LOG_EN
static ssize_t vf_log_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct vf_log_s *log = &devp->vfp->log;

	len += sprintf(buf + len, "%d of %d\n", log->log_cur, VF_LOG_LEN);
	return len;
}

static ssize_t vf_log_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	if (!strncmp(buf, "start", 5)) {
		vf_log_init(devp->vfp);
	} else if (!strncmp(buf, "print", 5)) {
		vf_log_print(devp->vfp);
	} else {
		pr_info("unknown command :  %s\n"
			"Usage:\n"
			"a. show log message:\n"
			"echo print > / sys/class/vdin/vdin0/vf_log\n"
			"b. restart log message:\n"
			"echo start > / sys/class/vdin/vdin0/vf_log\n"
			"c. show log records\n"
			"cat > / sys/class/vdin/vdin0/vf_log\n", buf);
	}
	return count;
}

/*
 * 1. show log length.
 * cat /sys/class/vdin/vdin0/vf_log
 * cat /sys/class/vdin/vdin1/vf_log
 * 2. clear log buffer and start log.
 * echo start > /sys/class/vdin/vdin0/vf_log
 * echo start > /sys/class/vdin/vdin1/vf_log
 * 3. print log
 * echo print > /sys/class/vdin/vdin0/vf_log
 * echo print > /sys/class/vdin/vdin1/vf_log
 */
static DEVICE_ATTR_RW(vf_log);

#endif /* VF_LOG_EN */
static ssize_t dts_config_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	vdin_dump_dts_config(devp);

	return len;
}

static ssize_t dts_config_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	struct vdin_dev_s *devp;
	unsigned int temp = 0;
	int err = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	devp = dev_get_drvdata(dev);
	vdin_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "max_buf_num")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.max_buf_num = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "min_buf_num")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.min_buf_num = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "cm_enable")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->dts_config.cm_enable = true;
			else
				devp->dts_config.cm_enable = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_det_idle_wait")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_det_idle_wait = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_get_prop_in_vs_en")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->dts_config.vdin_get_prop_in_vs_en = true;
			else
				devp->dts_config.vdin_get_prop_in_vs_en = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_get_prop_in_sm_en")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			if (temp)
				devp->dts_config.vdin_get_prop_in_sm_en = true;
			else
				devp->dts_config.vdin_get_prop_in_sm_en = false;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "canvas_config_mode")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.canvas_config_mode = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "viu_hw_irq")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.viu_hw_irq = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "game_mode_switch_frames")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.game_mode_switch_frames = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "game_mode_phlock_switch_frames")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.game_mode_phlock_switch_frames = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_vrr_switch_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_vrr_switch_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "sct_cache_size")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.sct_cache_size = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_re_config")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_re_config = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_re_cfg_drop_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_re_cfg_drop_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "back_nosig_max_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.back_nosig_max_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "atv_unstable_in_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.atv_unstable_in_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "atv_unstable_out_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.atv_unstable_out_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "hdmi_unstable_out_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.hdmi_unstable_out_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "hdmi_stable_out_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.hdmi_stable_out_cnt  = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "atv_stable_out_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.atv_stable_out_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "atv_stable_fmt_check_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.atv_stable_fmt_check_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "atv_prestable_out_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0))  {
			devp->dts_config.atv_prestable_out_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "other_stable_out_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.other_stable_out_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "other_unstable_out_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.other_unstable_out_cnt  = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "other_unstable_in_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.other_unstable_in_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "nosig_in_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.nosig_in_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "nosig2_unstable_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.nosig2_unstable_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_dv_chg_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_dv_chg_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_hdr_chg_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_hdr_chg_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_vrr_chg_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_vrr_chg_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "vdin_qms_chg_cnt")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->dts_config.vdin_qms_chg_cnt = temp;
		} else {
			err = -EINVAL;
			goto error;
		}
	}

	kfree(buf_orig);
	return count;
error:
	pr_err("vdin dts config error: %d\n", err);
	return count;
}

/*
 * 1. show log length.
 * cat /sys/class/vdin/vdin0/vf_log
 * cat /sys/class/vdin/vdin1/vf_log
 * 2. clear log buffer and start log.
 * echo start > /sys/class/vdin/vdin0/vf_log
 * echo start > /sys/class/vdin/vdin1/vf_log
 * 3. print log
 * echo print > /sys/class/vdin/vdin0/vf_log
 * echo print > /sys/class/vdin/vdin1/vf_log
 */
static DEVICE_ATTR_RW(dts_config);

#ifdef ISR_LOG_EN
static ssize_t isr_log_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	u32 len = 0;
	struct vdin_dev_s *devp;

	devp = dev_get_drvdata(dev);
		len += sprintf(buf + len, "%d of %d\n",
			       devp->vfp->isr_log.log_cur, ISR_LOG_LEN);
	return len;
}

/*
 * 1. show isr log length.
 * cat /sys/class/vdin/vdin0/vf_log
 * 2. clear isr log buffer and start log.
 * echo start > /sys/class/vdin/vdinx/isr_log
 * 3. print isr log
 * echo print > /sys/class/vdin/vdinx/isr_log
 */
static ssize_t isr_log_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct vdin_dev_s *devp;

	devp = dev_get_drvdata(dev);
	if (!strncmp(buf, "start", 5))
		isr_log_init(devp->vfp);
	else if (!strncmp(buf, "print", 5))
		isr_log_print(devp->vfp);
	return count;
}

static DEVICE_ATTR_RW(isr_log);
#endif

static ssize_t crop_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct tvin_cutwin_s *crop = &devp->debug.cutwin;

	len += sprintf(buf + len,
			"cutwin:%d, hs:%u, he:%u, vs:%u, ve:%u\n",
			crop->en, crop->hs, crop->he, crop->vs, crop->ve);
	return len;
}

static ssize_t crop_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t count)
{
	char *parm[5] = {NULL}, *buf_orig;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	struct tvin_cutwin_s *crop = &devp->debug.cutwin;
	long val = 0;
	ssize_t ret_ext = count;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	vdin_parse_param(buf_orig, parm);

	if (!parm[4]) {
		ret_ext = -EINVAL;
		pr_info("miss param!! 5 args\n");
	} else {
		if (kstrtol(parm[0], 10, &val) == 0)
			devp->debug.cutwin.en = val;
		if (kstrtol(parm[1], 10, &val) == 0)
			crop->hs = val;
		if (kstrtol(parm[2], 10, &val) == 0)
			crop->he = val;
		if (kstrtol(parm[3], 10, &val) == 0)
			crop->vs = val;
		if (kstrtol(parm[4], 10, &val) == 0)
			crop->ve = val;
	}

	kfree(buf_orig);

	return ret_ext;
}

static DEVICE_ATTR_RW(crop);

static ssize_t color_info_show(struct device *dev,
			   struct device_attribute *attr,
			   char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	unsigned int y, u, v;
	unsigned int r, g, b;

	vdin_prob_get_yuv(devp->addr_offset, &y, &u, &v);
	vdin_prob_get_rgb(devp->addr_offset, &r, &g, &b);

	len += sprintf(buf + len,
		       "values_rgb_yuv-->%d,%d,%d,%d,%d,%d\n",
		       r, g, b, y, u, v);

	return len;
}

static ssize_t color_info_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = {NULL};
	struct vdin_dev_s *devp;
	long val = 0;
	int err = 0;

	if (!buf)
		return count;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	devp = dev_get_drvdata(dev);
	vdin_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "prob_xy")) {
		unsigned int x = 0, y = 0;

#ifdef CONFIG_AMLOGIC_PIXEL_PROBE
		vdin_probe_enable();
#endif
		if (parm[1] && parm[2]) {
			if (kstrtoul(parm[1], 10, &val) == 0)
				x = val;
			if (kstrtoul(parm[2], 10, &val) == 0)
				y = val;
			vdin_prob_set_xy(devp->addr_offset, x, y, devp);
		} else {
			err = -EINVAL;
			goto error;
		}
	} else if (!strcmp(parm[0], "prob_pre_post")) {
		unsigned int x = 0;

		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else {
			if (kstrtoul(parm[1], 10, &val) == 0)
				x = val;
			pr_info("matrix post sel: %d\n", x);
			vdin_prob_set_before_or_after_mat(devp->addr_offset, x, devp);
		}
	} else if (!strcmp(parm[0], "prob_mat_sel")) {
		unsigned int x = 0;

		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else {
			if (kstrtoul(parm[1], 10, &val) == 0)
				x = val;
			pr_info("matrix sel : %d\n", x);
			vdin_prob_matrix_sel(devp->addr_offset, x, devp);
		}
	} else if (!strcmp(parm[0], "prob_yuv")) {
		unsigned int y, u, v;

		vdin_prob_get_yuv(devp->addr_offset, &y, &u, &v);
		pr_info("yuv_info-->y:%x, u:%x, v:%x\n", y, u, v);
	} else if (!strcmp(parm[0], "prob_rgb")) {
		unsigned int r, g, b;

		vdin_prob_get_rgb(devp->addr_offset, &r, &g, &b);
		pr_info("rgb_info-->r:%x, g:%x, b:%x\n", r, g, b);
	}

	kfree(buf_orig);
	return count;
error:
	pr_err("vdin color info error: %d\n", err);
	kfree(buf_orig);
	return count;
}

static DEVICE_ATTR_RW(color_info);

static ssize_t cm2_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	int len = 0;
	struct vdin_dev_s *devp;
	unsigned int addr_port = VDIN_CHROMA_ADDR_PORT;
	unsigned int data_port = VDIN_CHROMA_DATA_PORT;

	devp = dev_get_drvdata(dev);
	if (devp->addr_offset != 0) {
		addr_port = VDIN_CHROMA_ADDR_PORT + devp->addr_offset;
		data_port = VDIN_CHROMA_DATA_PORT + devp->addr_offset;
	}

	len += sprintf(buf + len, "addr_port[0x%x] data_port[0x%x]\n",
		       addr_port, data_port);
	len += sprintf(buf + len, "Usage:");
	len += sprintf(buf + len,
		       "echo wm addr data0 data1 data2 data3 data4 >");
	len += sprintf(buf + len, "/sys/class/vdin/vdin0/cm2\n");
	len += sprintf(buf + len,
		       "echo rm addr > / sys/class/vdin/vdin0/cm2\n");
	len += sprintf(buf + len,
		       "echo wm addr data0 data1 data2 data3 data4 >");
	len += sprintf(buf + len, "/sys/class/vdin/vdin1/cm2\n");
	len += sprintf(buf + len,
		       "echo rm addr > / sys/class/vdin/vdin1/cm2\n");
	return len;
}

static ssize_t cm2_store(struct device *dev,
			 struct device_attribute *attr,
			 const char *buffer, size_t count)
{
	struct vdin_dev_s *devp;
	int n = 0;
	char *buf_orig, *ps, *token;
	char *parm[7];
	u32 addr = 0;
	int data[5] = {0};
	unsigned int addr_port = VDIN_CHROMA_ADDR_PORT;
	unsigned int data_port = VDIN_CHROMA_DATA_PORT;
	long val;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	int err = 0;

	strncat(delim1, delim2, 1);

	devp = dev_get_drvdata(dev);
	if (devp->addr_offset != 0) {
		addr_port = VDIN_CHROMA_ADDR_PORT + devp->addr_offset;
		data_port = VDIN_CHROMA_DATA_PORT + devp->addr_offset;
	}
	buf_orig = kstrdup(buffer, GFP_KERNEL);
	ps = buf_orig;
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
	if (n == 0) {
		err = -1;
		goto error;
	}
	if ((parm[0][0] == 'w') && parm[0][1] == 'm') {
		if (n != 7) {
			err = -1;
			goto error;
		}
		/* addr = simple_strtol(parm[1], NULL, 16); */
		if (kstrtol(parm[1], 16, &val) < 0) {
			kfree(buf_orig);
			return -EINVAL;
		}
		addr = val;
		addr = addr - addr % 8;
		if (kstrtol(parm[2], 16, &val) == 0)
			data[0] = val;
		if (kstrtol(parm[3], 16, &val) == 0)
			data[1] = val;
		if (kstrtol(parm[4], 16, &val) == 0)
			data[2] = val;
		if (kstrtol(parm[5], 16, &val) == 0)
			data[3] = val;
		if (kstrtol(parm[6], 16, &val) == 0)
			data[4] = val;
		aml_write_vcbus(addr_port, addr);
		aml_write_vcbus(data_port, data[0]);
		aml_write_vcbus(addr_port, addr + 1);
		aml_write_vcbus(data_port, data[1]);
		aml_write_vcbus(addr_port, addr + 2);
		aml_write_vcbus(data_port, data[2]);
		aml_write_vcbus(addr_port, addr + 3);
		aml_write_vcbus(data_port, data[3]);
		aml_write_vcbus(addr_port, addr + 4);
		aml_write_vcbus(data_port, data[4]);

		pr_info("wm[0x%x]  0x0\n", addr);
	} else if ((parm[0][0] == 'r') && parm[0][1] == 'm') {
		if (n != 2) {
			err = -EINVAL;
			goto error;
		}
		if (kstrtol(parm[1], 16, &val) == 0)
			addr = val;
		addr = addr - addr % 8;
		aml_write_vcbus(addr_port, addr);
		data[0] = aml_read_vcbus(data_port);
		data[0] = aml_read_vcbus(data_port);
		data[0] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 1);
		data[1] = aml_read_vcbus(data_port);
		data[1] = aml_read_vcbus(data_port);
		data[1] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 2);
		data[2] = aml_read_vcbus(data_port);
		data[2] = aml_read_vcbus(data_port);
		data[2] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 3);
		data[3] = aml_read_vcbus(data_port);
		data[3] = aml_read_vcbus(data_port);
		data[3] = aml_read_vcbus(data_port);
		aml_write_vcbus(addr_port, addr + 4);
		data[4] = aml_read_vcbus(data_port);
		data[4] = aml_read_vcbus(data_port);
		data[4] = aml_read_vcbus(data_port);

		pr_info("rm:[0x%x] : data[0x%x][0x%x][0x%x][0x%x][0x%x]\n",
			addr, data[0], data[1], data[2], data[3], data[4]);
	} else if (!strcmp(parm[0], "enable")) {
		wr_bits(devp->addr_offset, VDIN_CM_BRI_CON_CTRL, 1,
			CM_TOP_EN_BIT, CM_TOP_EN_WID);
	} else if (!strcmp(parm[0], "disable")) {
		wr_bits(devp->addr_offset, VDIN_CM_BRI_CON_CTRL, 0,
			CM_TOP_EN_BIT, CM_TOP_EN_WID);
	} else {
		err = -4;
		goto error;
	}

	kfree(buf_orig);
	return count;

error:
	pr_err("vdin cm2 error: %d\n", err);
	kfree(buf_orig);
	return count;
}

static DEVICE_ATTR_RW(cm2);

static ssize_t snow_flag_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	int snow_flag = 0;
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	snow_flag = (devp->flags & VDIN_FLAG_SNOW_FLAG) >> 14;
	return snprintf(buf, sizeof(unsigned int), "%d\n", snow_flag);
}

static DEVICE_ATTR_RO(snow_flag);

static inline unsigned int vdin_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long val = num;

	do_div(val, den);

	return (unsigned int)val;
}

//for vrr get input fate
static ssize_t input_rate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u64 tmp_msr_clk_val;
	unsigned int tmp;
	static unsigned int input_rate = 60000;//60.000hz
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	if (devp->parm.info.status == TVIN_SIG_STATUS_STABLE) {
		if (devp->irq_cnt > 2 && devp->cycle != 0) {
			tmp_msr_clk_val = (u64)devp->msr_clk_val * 1000;
			tmp = vdin_do_div(tmp_msr_clk_val, devp->cycle);

			if (tmp > (devp->parm.info.fps + 1) * 1000) {
				if (devp->debug.vdin_dbg_en)
					pr_info("invalid input_rate:%d.%03d,fps:%d\n",
						(tmp / 1000), (tmp % 1000), devp->parm.info.fps);
			} else {
				input_rate = tmp;
			}
		} else {
			if (devp->prop.vtem_data.qms_en)
				input_rate = devp->prop.vtem_data.next_tfr;
			else
				input_rate = devp->parm.info.fps * 1000;
		}
	} else {
		input_rate = 60000;//unstable default 60hz
	}
	return sprintf(buf, "%d.%03d\n",
		 (input_rate / 1000), (input_rate % 1000));
}

static DEVICE_ATTR_RO(input_rate);

ssize_t slt_test_store(struct device *dev,
		      struct device_attribute *attr,
		      const char *buf, size_t len)
{
	char *buf_orig, *parm[48] = {NULL};
	struct vdin_dev_s *devp = dev_get_drvdata(dev);
	long val = 0;
	int err = 0;

	if (!buf)
		return len;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	vdin_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "set_crc")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->debug.slt_test.vf_ori_crc = val;
			pr_info("vdin%d:vf_ori_crc = 0x%x\n\n", devp->index,
				devp->debug.slt_test.vf_ori_crc);
		}
	} else if (!strcmp(parm[0], "enable")) {
		if (!parm[1]) {
			err = -EINVAL;
			goto error;
		} else if (kstrtoul(parm[1], 0, &val) == 0) {
			devp->debug.slt_test.en = !!val;
			pr_info("vdin%d:en:%d,vf_ori_crc = 0x%x\n\n", devp->index,
				devp->debug.slt_test.en, devp->debug.slt_test.vf_ori_crc);
		}
	} else if (!strcmp(parm[0], "state")) {
		pr_info("vdin%d:en:%d,vf_ori_crc = 0x%x,chk:%d,cnt:%d\n", devp->index,
			devp->debug.slt_test.en, devp->debug.slt_test.vf_ori_crc,
			devp->debug.slt_test.vf_check_result, devp->debug.slt_test.vf_pass_cnt);
	}

	kfree(buf_orig);

	return len;
error:
	pr_err("vdin slt test error: %d\n", err);
	kfree(buf_orig);
	return len;
}

static ssize_t slt_test_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct vdin_dev_s *devp = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		devp->debug.slt_test.vf_check_result ? "pass" : "fail");
}
static DEVICE_ATTR_RW(slt_test);

static void vdin_dump_sct_state(struct vdin_dev_s *devp)
{
	int i, sum = 0;
	struct vf_entry *vfe = NULL;
	struct codec_mm_scatter *sc;
	struct vdin_mmu_box *box = NULL;

	pr_info("type:%d pool size:%d, WL:%d, WM:%d, RL:%d, RM:%d\n",
		devp->mem_type, devp->vfp->size, devp->vfp->wr_list_size,
		devp->vfp->wr_mode_size, devp->vfp->rd_list_size,
		devp->vfp->rd_mode_size);

	pr_info("irq:%d, frm:%d, que:%d, run:%d, af_num:%d\n",
		devp->irq_cnt, devp->frame_cnt,
		devp->msct_top.que_work_cnt,
		devp->msct_top.worker_run_cnt,
		devp->af_num);

	/* initialize provider write list */
	for (i = 0; i < devp->vfp->size; i++) {
		vfe = vf_get_master(devp->vfp, i);
		if (!vfe) {
			pr_err("%s null vfe\n", __func__);
			break;
		}
		sum += devp->msct_top.sct_stat[i].cur_page_cnt;
		pr_info("[%d](%p),id:%d,sct_stat:%d,status:%d,cur_page_cnt:%d,addr:%#lx\n",
			i, vfe->vf.mem_handle, vfe->vf.index, vfe->sct_stat, vfe->status,
			devp->msct_top.sct_stat[i].cur_page_cnt,
			devp->afbce_info->fm_body_paddr[i]);
	}
	pr_info("max_buf_mum:%d,mmu number:%d,total pages vf:%d,size:%ld KB\n",
		devp->msct_top.max_buf_num, devp->msct_top.mmu_4k_number,
		sum, (sum * PAGE_SIZE) / 1024);

	box = devp->msct_top.box;
	if (!box) {
		pr_err("%s can't get scatter\n", __func__);
		return;
	}
	sum = 0;
	for (i = 0; i < box->max_sc_num; i++) {
		sc = box->sc_list[i];
		if (!sc) {
			pr_err("sc[%d],null\n", i);
			continue;
		}
		sum += sc->page_used;
		pr_info("sc[%d],max_cnt:%d,cnt:%d,tail:%d,used:%d\n",
			i, sc->page_max_cnt, sc->page_cnt,
			sc->page_tail, sc->page_used);
	}
	pr_info("total pages codec_mm_scatter:%d,size:%ld KB\n", sum, (sum * PAGE_SIZE) / 1024);
}

ssize_t sct_attr_show(struct device *dev,
		  struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len,
		       "echo state > /sys/class/vdin/vdin0/sct_attr\n");
	return len;
}

static ssize_t sct_attr_store(struct device *dev,
			  struct device_attribute *attr,
			  const char *buf, size_t len)
{
	char *buf_orig, *parm[47] = {NULL};
	struct vdin_dev_s *devp;
	unsigned int offset, temp = 0;

	if (!buf)
		return len;
	buf_orig = kstrdup(buf, GFP_KERNEL);
	devp = dev_get_drvdata(dev);
	vdin_parse_param(buf_orig, (char **)&parm);
	offset = devp->addr_offset;

	if (!strncmp(parm[0], "state", 3)) {
		vdin_dump_sct_state(devp);
	} else if (!strcmp(parm[0], "dbg_sct_ctl")) {
		if (parm[1] && (kstrtouint(parm[1], 0, &temp) == 0)) {
			devp->debug.dbg_sct_ctl = temp;
		}
	} else {
		pr_info("unknown command:%s\n", parm[0]);
	}

	kfree(buf_orig);
	return len;
}

static DEVICE_ATTR_RW(sct_attr);
#endif

int vdin_create_debug_files(struct device *dev)
{
	int ret = 0;
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	unsigned int i = 0;
#endif

	ret |= device_create_file(dev, &dev_attr_sig_det);
	ret |= device_create_file(dev, &dev_attr_attr);
	/* create sysfs attribute files */
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	#ifdef VF_LOG_EN
	ret |= device_create_file(dev, &dev_attr_vf_log);
	#endif
	#ifdef ISR_LOG_EN
	ret |= device_create_file(dev, &dev_attr_isr_log);
	#endif
	ret |= device_create_file(dev, &dev_attr_color_info);
	ret |= device_create_file(dev, &dev_attr_sct_attr);
	ret |= device_create_file(dev, &dev_attr_cm2);
	/*ret = device_create_file(dev, &dev_attr_debug_for_isp);*/
	ret |= device_create_file(dev, &dev_attr_crop);
	ret |= device_create_file(dev, &dev_attr_snow_flag);
	ret |= device_create_file(dev, &dev_attr_input_rate);
	ret |= device_create_file(dev, &dev_attr_slt_test);
	ret |= device_create_file(dev, &dev_attr_dts_config);
	for (i = 0; i < VDIN_MAX_INSTANCE; i++)
		ret |= sysfs_create_group(&dev->kobj, &vdin_attr_group[i]);
#endif
	return ret;
}

void vdin_remove_debug_files(struct device *dev)
{
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	#ifdef VF_LOG_EN
	device_remove_file(dev, &dev_attr_vf_log);
	#endif
	#ifdef ISR_LOG_EN
	device_remove_file(dev, &dev_attr_isr_log);
	#endif

	device_remove_file(dev, &dev_attr_sct_attr);
	device_remove_file(dev, &dev_attr_cm2);
	/*device_remove_file(dev, &dev_attr_debug_for_isp);*/
	device_remove_file(dev, &dev_attr_crop);
	device_remove_file(dev, &dev_attr_snow_flag);
	device_remove_file(dev, &dev_attr_input_rate);
	device_remove_file(dev, &dev_attr_slt_test);
	device_remove_file(dev, &dev_attr_color_info);
	device_remove_file(dev, &dev_attr_dts_config);
#endif
	device_remove_file(dev, &dev_attr_attr);
	device_remove_file(dev, &dev_attr_sig_det);
}

#ifdef DEBUG_SUPPORT
static int memp = MEMP_DCDR_WITHOUT_3D;
static char *memp_str(int profile)
{
	switch (profile) {
	case MEMP_VDIN_WITHOUT_3D:
		return "vdin without 3d";
	case MEMP_VDIN_WITH_3D:
		return "vdin with 3d";
	case MEMP_DCDR_WITHOUT_3D:
		return "decoder without 3d";
	case MEMP_DCDR_WITH_3D:
		return "decoder with 3d";
	case MEMP_ATV_WITHOUT_3D:
		return "atv without 3d";
	case MEMP_ATV_WITH_3D:
		return "atv with 3d";
	default:
		return "unknown";
	}
}

/*
 * cat /sys/class/vdin/memp
 */
static ssize_t memp_show(const struct class *class,
			 const struct class_attribute *attr, char *buf)
{
	int len = 0;

	len += sprintf(buf + len, "%d %s\n", memp, memp_str(memp));
	return len;
}

/*
 * echo 0|1|2|3|4|5 > /sys/class/vdin/memp
 */
static void memp_set(int type)
{
	switch (type) {
	case MEMP_VDIN_WITHOUT_3D:
	case MEMP_VDIN_WITH_3D:
		aml_write_vcbus(VPU_VDIN_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VDISP_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VPUARB2_ASYNC_HOLD_CTRL, 0x80408040);
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
				aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1 << 12));
		 /* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
				aml_read_vcbus(VPU_VD2_MMC_CTRL) |
				(1 << 12));
		  /* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
				aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
				aml_read_vcbus(VPU_DI_INP_MMC_CTRL) &
				(~(1 << 12)));
		 /* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
				aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) &
				(~(1 << 12)));
		memp = type;
		break;
	case MEMP_DCDR_WITHOUT_3D:
	case MEMP_DCDR_WITH_3D:
		aml_write_vcbus(VPU_VDIN_ASYNC_HOLD_CTRL, 0x80408040);
		 /* arb0 */
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
				aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1 << 12));
		/* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
				aml_read_vcbus(VPU_VD2_MMC_CTRL) | (1 << 12));
		/* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
				aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) |
				(1 << 12));
		  /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
				aml_read_vcbus(VPU_DI_INP_MMC_CTRL) &
				(~(1 << 12)));
		/* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) |
				(1 << 12));
		/* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
				aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) &
				(~(1 << 12)));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) &
				(~(1 << 12)));
		memp = type;
		break;
	case MEMP_ATV_WITHOUT_3D:
	case MEMP_ATV_WITH_3D:
		/* arb0 */
		aml_write_vcbus(VPU_VD1_MMC_CTRL,
				aml_read_vcbus(VPU_VD1_MMC_CTRL) | (1 << 12));
		/* arb0 */
		aml_write_vcbus(VPU_VD2_MMC_CTRL,
				aml_read_vcbus(VPU_VD2_MMC_CTRL) | (1 << 12));
		 /* arb0 */
		aml_write_vcbus(VPU_DI_IF1_MMC_CTRL,
				aml_read_vcbus(VPU_DI_IF1_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MEM_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MEM_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_INP_MMC_CTRL,
				aml_read_vcbus(VPU_DI_INP_MMC_CTRL) &
				(~(1 << 12)));
		/* arb0 */
		aml_write_vcbus(VPU_DI_MTNRD_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNRD_MMC_CTRL) |
				(1 << 12));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_CHAN2_MMC_CTRL,
				aml_read_vcbus(VPU_DI_CHAN2_MMC_CTRL) &
				(~(1 << 12)));
		 /* arb1 */
		aml_write_vcbus(VPU_DI_MTNWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_MTNWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_NRWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_NRWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb1 */
		aml_write_vcbus(VPU_DI_DIWR_MMC_CTRL,
				aml_read_vcbus(VPU_DI_DIWR_MMC_CTRL) &
				(~(1 << 12)));
		/* arb2 */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
				aml_read_vcbus(VPU_TVD3D_MMC_CTRL) |
				(1 << 14));
		/* urgent */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
				aml_read_vcbus(VPU_TVD3D_MMC_CTRL) |
				(1 << 15));
		/* arb2 */
		aml_write_vcbus(VPU_TVDVBI_MMC_CTRL,
				aml_read_vcbus(VPU_TVDVBI_MMC_CTRL) |
				(1 << 14));
		 /* urgent */
		aml_write_vcbus(VPU_TVD3D_MMC_CTRL,
				aml_read_vcbus(VPU_TVD3D_MMC_CTRL) |
				(1 << 15));
		memp = type;
		break;
	default:
		/* @todo */
		break;
	}
}

static ssize_t memp_store(const struct class *class,
			  const struct class_attribute *attr,
			  const char *buf, size_t count)
{
	/* int type = simple_strtol(buf, NULL, 10); */
	long type;

	if (kstrtol(buf, 10, &type) == 0)
		memp_set(type);
	else
		return -EINVAL;

	return count;
}

static CLASS_ATTR_RW(memp);

int vdin_create_class_files(struct class *vdin_class)
{
	int ret = 0;

	ret = class_create_file(vdin_class, &class_attr_memp);
	return ret;
}

void vdin_remove_class_files(struct class *vdin_class)
{
	class_remove_file(vdin_class, &class_attr_memp);
}

#endif

/*2018-07-18 add debugfs*/
#define DEFINE_SHOW_VDIN(__name) \
static int __name ## _open(struct inode *inode, struct file *file)	\
{ \
	return single_open(file, __name ## _show, inode->i_private);	\
} \
									\
static const struct file_operations __name ## _fops = {			\
	.owner = THIS_MODULE,		\
	.open = __name ## _open,	\
	.read = seq_read,		\
	.llseek = seq_lseek,		\
	.release = single_release,	\
}

DEFINE_SHOW_VDIN(seq_file_vdin_state);

struct vdin_debugfs_files_t {
	const char *name;
	const umode_t mode;
	const struct file_operations *fops;
};

static struct vdin_debugfs_files_t vdin_debugfs_files[] = {
	{"state", S_IFREG | 0644, &seq_file_vdin_state_fops},

};

void vdin_debugfs_init(struct vdin_dev_s *devp)
{
	int i;
	struct dentry *ent;
	unsigned int nub;
	int err = 0;

	nub = devp->index;

	if (nub > 0)
		return;

	if (devp->dbg_root) {
		err = -2;
		goto error;
	}
	devp->dbg_root = debugfs_create_dir("vdin0", NULL);

	if (!devp->dbg_root) {
		err = -EINVAL;
		goto error;
	}

	for (i = 0; i < ARRAY_SIZE(vdin_debugfs_files); i++) {
		ent = debugfs_create_file(vdin_debugfs_files[i].name,
					  vdin_debugfs_files[i].mode,
					  devp->dbg_root, NULL,
					  vdin_debugfs_files[i].fops);
		if (!ent) {
			err = -4;
			goto cleanup;
		}
	}
	return;
cleanup:
	debugfs_remove(devp->dbg_root);
error:
	pr_err("vdin debugfs error:%d\n", err);
}

void vdin_debugfs_exit(struct vdin_dev_s *devp)
{
	unsigned int nub;

	nub = devp->index;
	if (nub > 0) {
		pr_info("%s only support debug vdin0 %d\n", __func__, nub);
		return;
	}

	debugfs_remove(devp->dbg_root);
}

void vdin_dump_frames(struct vdin_dev_s *devp)
{
	char file_path[32];
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	int  i;
#endif

	if (devp->dbg_dump_frames) {
		/* todo:check path available */
		snprintf(file_path, sizeof(file_path), "%s", "/mnt/img");
		pr_info("vdin dump frame path:%s\n", file_path);
		if (devp->afbce_valid == 1) {
			vdin_pause_dec(devp, 0);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
			for (i = 0; i < devp->canvas_max_num; i++)
				vdin_dump_one_afbce_mem(file_path, devp, i);
#endif

			vdin_resume_dec(devp, 0);
		} else {
			/* mif */
		}
	}
}

#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
#define DEV0_NAME "VPU0"
static char vdin_id;
static unsigned long addr, size;

static unsigned int vdin_idx;
enum vdin_dmc_device_id_e {
	VDIN_DMC_DEVICE_META = 0,
	VDIN_DMC_DEVICE_AFBCE,
	VDIN_DMC_DEVICE_WRMIF,
	VDIN_DMC_DEVICE_MAX,
};

static int vdin_dmc_meta(struct vdin_dev_s *devp)
{
	devp->dv.meta_data_raw_p_buffer0 = addr;
	vdin_wrmif2_addr_update(devp);

	return 0;
}

static int vdin_dmc_afbce(struct vdin_dev_s *devp)
{
	if (devp->afbce_info && devp->afbce_valid) {
		devp->afbce_info->fm_body_paddr[0] = addr;
		vdin_afbce_maptable_init(devp);
	}

	return 0;
}

static int vdin_dmc_wrmif(struct vdin_dev_s *devp)
{
	struct vf_entry *vfe;

	vfe = provider_vf_peek(devp->vfp);
	if (!vfe) {
		pr_info("receiver_vf_peek failed\n");
		return 0;
	}
	vfe->vf.canvas0_config[0].phy_addr = addr;

	return 0;
}

//notifier handle func
static int vdin_dmc_dev_access_notify(struct notifier_block *nb, unsigned long id, void *data)
{
	struct dmc_dev_access_data *tmp = (struct dmc_dev_access_data *)data;
	struct vdin_dev_s *devp = NULL;
	unsigned int device_id = 0; /* refer to xx_vpu_arb assignment.xlsx */

	if (vdin_id == id) {
		addr = tmp->addr;
		size = tmp->size;
		devp = vdin_get_dev(vdin_idx);//todo
		device_id = devp->debug.dbg_device_id;
		pr_info("vdin%ld dmc access addr:%lx,size:%lx, device_id:%d\n",
			id, addr, size, device_id);
		if (device_id == VDIN_DMC_DEVICE_META)
			vdin_dmc_meta(devp);
		else if (device_id == VDIN_DMC_DEVICE_AFBCE)
			vdin_dmc_afbce(devp);
		else if (device_id == VDIN_DMC_DEVICE_WRMIF)
			vdin_dmc_wrmif(devp);
	}

	return 0;
}

static struct notifier_block vdin_dmc_dev_access_nb = {
	.notifier_call = vdin_dmc_dev_access_notify,
};
#endif

//register notifier
void vdin_reg_dmc_notifier(unsigned int index)
{
#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
	vdin_idx = index;
	if (is_meson_s5_cpu() || is_meson_t3_cpu() || is_meson_t7_cpu())
		vdin_id = register_dmc_dev_access_notifier("VPU0",
			&vdin_dmc_dev_access_nb);
	else if (is_meson_t5w_cpu())
		vdin_id = register_dmc_dev_access_notifier("VPU WRITE0",
			&vdin_dmc_dev_access_nb);
	else
		vdin_id = register_dmc_dev_access_notifier("VPU WRITE1",
			&vdin_dmc_dev_access_nb);
#endif
}

//unregister notifier
void vdin_unreg_dmc_notifier(unsigned int index)
{
#if IS_ENABLED(CONFIG_AMLOGIC_DMC_DEV_ACCESS)
	if (is_meson_s5_cpu() || is_meson_t3_cpu() || is_meson_t7_cpu())
		vdin_id = unregister_dmc_dev_access_notifier("VPU0",
			&vdin_dmc_dev_access_nb);
	else if (is_meson_t5w_cpu())
		vdin_id = unregister_dmc_dev_access_notifier("VPU WRITE0",
			&vdin_dmc_dev_access_nb);
	else
		vdin_id = unregister_dmc_dev_access_notifier("VPU WRITE1",
			&vdin_dmc_dev_access_nb);
#endif
}

/*------------------------------------------*/
