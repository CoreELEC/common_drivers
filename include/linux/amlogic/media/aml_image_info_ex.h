/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef _AML_IMAGE_INFO_EX_H_
#define _AML_IMAGE_INFO_EX_H_

#include <linux/types.h>
#include <linux/amlogic/media/aml_image_info.h>

//aml_image_info_ex.h
/*
 * memory_type_t - Memory type classification.
 *
 * @MEM_TYPE_VIRTADDR	: Virtual address memory.
 * @MEM_TYPE_PHYADDR	: Physical address memory.
 * @MEM_TYPE_DMABUF	: DMA buffer memory.
 * @MEM_TYPE_FD		: Handle for DMA buffer memory.
 * @MEM_TYPE_MAX	: Sentinel value.
 */
enum memory_type_t {
	MEM_TYPE_VIRTADDR,
	MEM_TYPE_PHYADDR,
	MEM_TYPE_DMABUF,
	MEM_TYPE_DMABUF_FD,
	MEM_TYPE_MAX
};

/*
 * struct image_plane_s - Image plane descriptor.
 *
 * @data		: Store virtual or physical address.
 * @stride		: Line stride in bytes (distance between rows).
 * @size		: Plane data size in bytes.
 * @pixel_stride: Pixel stride for formats with padding (e.g., RGBA).
 */
struct image_plane_s {
	u64		data;
	u32		stride;
	u32		size;
	u32		pixel_stride;
};

/*
 * struct image_info_ex_s - Comprehensive image metadata descriptor.
 *
 * @width	: Image width in pixels.
 * @height	: Image height in pixels.
 * @format	: Image pixel format (RGB, YUV, etc.).
 * @bit_depth	: Bit depth per component (8, 10, 12, 16).
 * @alignment	: Memory alignment requirement in bytes. 0~15bit, for width.16~31bit, for height.
 * @secure	: Secure memory flag.
 * @memory_type : Memory type classification.
 * @uncached	: 1 if uncached memory, 0 if cached.
 * @total_size	: Total buffer size in bytes.
 * @plane_count : Number of valid planes (1 for RGB, 2-3 for YUV).
 * @planes	: Array of plane descriptors (up to 4 planes).
 * @roi		: Region of interest within the image.
 * @crop	: Border cropping parameters.
 * @timestamp	: Image acquisition timestamp (optional).
 * @sequence	: Frame sequence number (optional).
 * @flags	: Processing flags and attributes.
 *
 * Usage notes:
 * - For packed formats: use plane_count=1, planes[0] contains all data.
 * - For planar formats: each plane has separate data pointer and stride.
 * - @roi defines the active processing area, @crop defines border removal.
 * - @stride must be >= width * bytes_per_pixel (consider padding).
 */
struct image_info_ex_s {
	/* Basic attributes */
	u32				width;
	u32				height;
	u32				format;
	u32				bit_depth;
	u32				alignment;
	u32				secure;
	/* Memory information */
	u32				memory_type;
	u32				uncached;
	u32				total_size;
	/* Plane information */
	u32				plane_count;
	struct image_plane_s planes[4];
	/* Processing regions */
	struct image_rect_s roi;
	struct image_crop_s crop;
	/* Metadata */
	u32				timestamp;
	u32				sequence;
	u32				flags;
};

#endif /* _AML_IMAGE_INFO_EX_H_ */
