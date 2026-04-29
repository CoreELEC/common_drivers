/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/amlogic/media/aml_image_info_ex.h>

#ifndef _DHP_INTERFACE_H_
#define _DHP_INTERFACE_H_

/*
 * DHP processing mode.
 *
 * @DHP_FLAG_IO_BLOCKING	: Block waiting for avbcd processing to complete.
 * @DHP_FLAG_IO_NON_BLOCKING	: Submit avbcd task without blocking.
 * @DHP_FLAG_IO_FENCE		: Fence signal to caller.
 * @DHP_FLAG_FROM_USER		: Task from userspace.
 * @DHP_FLAG_FROM_KERNEL	: Task from kernel space.
 * @DHP_FLAG_AVBCD		: Task type is avbcd process.
 * @DHP_FLAG_YUV_SCALE		: Task type is yuv scalar process.
 */
enum dhp_type_e {
	/* Block flag */
	DHP_FLAG_IO_BLOCKING = BIT(0),
	DHP_FLAG_IO_NON_BLOCKING = BIT(1),
	DHP_FLAG_IO_FENCE = BIT(2),

	/* Task from */
	DHP_FLAG_FROM_USER = BIT(3),
	DHP_FLAG_FROM_KERNEL = BIT(4),

	/* Process flag */
	DHP_FLAG_AVBCD = BIT(5),
	DHP_FLAG_YUV_SCALE = BIT(6),
};

/*
 * enum dhp_memory_type_e - DHP output buffer type.
 *
 * @DHP_MEM_VIRTADDR	: Output buffer type is virtual address.
 * @DHP_MEM_PHYADDR	: Output buffer type is physical address.
 * @DHP_MEM_DMABUF	: Output buffer type is dma buffer.
 */
enum dhp_memory_type_e {
	DHP_MEM_VIRTADDR,
	DHP_MEM_PHYADDR,
	DHP_MEM_DMABUF,
	DHP_MEM_MAX
};

typedef void (*dhp_done)(void *out);

/*
 * struct dhp_input - Input parameters for DHP.
 *
 * @img     : Input image information.
 */
struct dhp_input {
	struct image_info_ex_s	img;
};

/*
 * struct dhp_output - Output parameters for DHP.
 *
 * @img       : Output image information.
 * @done_func : Callback for non-blocking operations.
 */
struct dhp_output {
	struct image_info_ex_s	img;
	dhp_done		done_func;
};

int AMLOGIC_DHP_PROCESS(struct dhp_output *out,
						struct dhp_input *in,
						u32 flag);

typedef int (*AMLOGIC_DHP_PROCESS_fun_t)(struct dhp_output *,
						struct dhp_input *,
						u32);

int register_amlogic_dhp_fun(AMLOGIC_DHP_PROCESS_fun_t fn);
int unregister_amlogic_dhp_fun(void);
#endif
