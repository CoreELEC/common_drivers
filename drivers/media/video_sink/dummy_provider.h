/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef VIDEO_PROVIDER_HEADER_HH
#define VIDEO_PROVIDER_HEADER_HH

extern unsigned int dummy_video_log_level;

#define VP_VFM_POOL_SIZE 3
#define VP_LEVEL_DEBUG   1
#define VP_LEVEL_DEBUG2  2

#define VIDEO_PROVIDER_NAME "dummy_video"
#define VP_VFPATH_ID    "dummy_video"
#define VP_VFPATH_CHAIN "dummy_video amvideo"

#define VIDEO_PROVIDER_NAME_PIP "dummy_videopip"
#define VP_VFPATH_ID_PIP    "dummy_videopip"
#define VP_VFPATH_CHAIN_PIP "dummy_videopip videopip"

#undef pr_fmt
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#define DUMMY_PROVIDER_DEBUG 0

#if DUMMY_PROVIDER_DEBUG
#define vp_info(fmt, ...) \
	pr_info(fmt, ##__VA_ARGS__)

#define vp_err(fmt, ...) \
	pr_err(fmt, ##__VA_ARGS__)

#define vp_dbg(fmt, ...) \
	do { \
		if (dummy_video_log_level >= VP_LEVEL_DEBUG) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#define vp_dbg2(fmt, ...) \
	do { \
		if (dummy_video_log_level >= VP_LEVEL_DEBUG2) { \
			pr_info(fmt, ##__VA_ARGS__); \
		} \
	} while (0)
#else
#define vp_info(fmt, ...) \
	do {} while (0)

#define vp_err(fmt, ...) \
	do {} while (0)

#define vp_dbg(fmt, ...) \
	do {} while (0)

#define vp_dbg2(fmt, ...) \
	do {} while (0)
#endif
enum {
	VP_FMT_NV21,
	VP_FMT_NV12,
	VP_FMT_RGB888,
	VP_FMT_YUV444_PACKED,
	VP_FMT_AFBC,
	VP_FMT_AFRC,
	VP_FMT_YUV444_3PLANE,
};

enum {
	VP_MEM_ION,
	VP_MEM_DMABUF,
	VP_MEM_DRIVER_CMA,
};

enum {
	VP_BIG_ENDIAN,
	VP_LITTLE_ENDIAN
};

struct video_provider_device_s {
	char name[20];
	int major;
	struct class *cla;
	struct device *dev;
	const struct vinfo_s *vinfo;
	struct platform_device *pdev;
};

struct vp_frame_s {
	int shared_fd;
	int mem_type;
	int fg_mem_type;
	int fg_data_shared_fd;
	int width;  /* frame width */
	int height; /* frame height */
	int format;
	int endian;
	int offset; /* for afbc data, offset = body start addr - head start addr */
	int afbc_format;
	int afrc_format;
	int bit_depth;
	int luma_only; /* only use the luma data from YUV */
	int secure;
	int fg_test;
	int fg_secure;
};

struct vp_lcevc_frame_s {
	struct vp_frame_s residual;
	struct vp_frame_s base;
};

struct vp_pool_s {
	struct vframe_s vfm;
	int used;
};

struct vp_cma_info {
	struct page *alloc_page;
	int alloc_len;
};

#define VIDEO_PROVIDER_IOC_MAGIC  'V'
#define VIDEO_PROVIDER_IOCTL_RENDER   _IOW(VIDEO_PROVIDER_IOC_MAGIC, 0x00, \
						struct vp_frame_s)
#define VIDEO_PROVIDER_IOCTL_POST     _IO(VIDEO_PROVIDER_IOC_MAGIC, 0x01)

#define VIDEO_PROVIDER_IOCTL_RENDER_PIP   _IOW(VIDEO_PROVIDER_IOC_MAGIC, 0x02, \
						struct vp_frame_s)
#define VIDEO_PROVIDER_IOCTL_POST_PIP     _IO(VIDEO_PROVIDER_IOC_MAGIC, 0x03)

#define VIDEO_PROVIDER_IOCTL_RENDER_LCEVC   _IOW(VIDEO_PROVIDER_IOC_MAGIC, 0x04, \
						struct vp_lcevc_frame_s)

#endif
