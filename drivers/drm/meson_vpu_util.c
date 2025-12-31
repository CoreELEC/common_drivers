// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/ktime.h>
#include "meson_vpu_util.h"
#include "vpu-hw/meson_vpu_reg.h"
#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#include <linux/amlogic/media/rdma/rdma_mgr.h>
#include <common/rdma/rdma.h>
#endif
#include "vpu-hw/meson_osd_afbc.h"
#include "meson_vpu_pipeline.h"
#include "meson_print.h"
#include "meson_drm_rdma.h"

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
#define RDMA_COUNT 3
static int drm_vsync_rdma_handle[RDMA_COUNT] = {-1, -1, -1};

static void meson_vpu_vsync_rdma_irq(void *arg)
{
	struct meson_vpu_sub_pipeline *sub_pipeline = arg;

	if (drm_vsync_rdma_handle[sub_pipeline->index] == -1)
		return;

	vpu_pipeline_check_finish_reg(sub_pipeline->index, sub_pipeline->pipeline->priv);
	vpu_pipeline_detect_reset(sub_pipeline);
	vpu_pipeline_detect_status(sub_pipeline);
}

static struct rdma_op_s meson_vpu_vsync_rdma_op[VPP_MAX];
void meson_vpu_reg_handle_register(void *arg)
{
	struct meson_vpu_sub_pipeline *sub_pipeline = arg;
	u32 vpp_index = sub_pipeline->index;

	if (drm_vsync_rdma_handle[vpp_index] == -1) {
		meson_vpu_vsync_rdma_op[vpp_index].irq_cb = meson_vpu_vsync_rdma_irq;
		meson_vpu_vsync_rdma_op[vpp_index].arg = sub_pipeline;
		drm_vsync_rdma_handle[vpp_index] =
			rdma_register(&meson_vpu_vsync_rdma_op[vpp_index],
			NULL, MESON_VPU_RDMA_TABLE_SIZE);
		meson_drm_rdma_init(sub_pipeline->pipeline->priv, vpp_index,
			drm_vsync_rdma_handle[vpp_index]);
	}

	DRM_DEBUG("vpp%d-%d\n", vpp_index, drm_vsync_rdma_handle[vpp_index]);
}

static u32 rdma_trigger_inputs[] = {
	RDMA_TRIGGER_VSYNC_INPUT,
	RDMA_TRIGGER_VPP1_VSYNC_INPUT,
	RDMA_TRIGGER_VPP2_VSYNC_INPUT,
};

/*suggestion: call this after atomic done*/
int meson_vpu_reg_vsync_config(u32 vpp_index)
{
	if (vpp_index >= MESON_MAX_CRTC) {
		DRM_ERROR("Invalid vpp index!");
		return -1;
	} else if (drm_vsync_rdma_handle[vpp_index] == -1) {
		DRM_ERROR("RDMA is not registered!");
		return -1;
	}
	MESON_DRM_REG("%d\n", vpp_index);

	return rdma_config(drm_vsync_rdma_handle[vpp_index],
		rdma_trigger_inputs[vpp_index]);
}
#endif

static u32 meson_vpu_read_reg(u32 addr)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	MESON_DRM_REG("0x%x\n", addr);
	if (rdma_tbl[0].flag)
		ret = meson_drm_read_rdma_table_reg(0, drm_vsync_rdma_handle[0], addr);
	else
		ret = rdma_read_reg(drm_vsync_rdma_handle[0], addr);
	return ret;
#else
	return aml_read_vcbus(addr);
#endif
}

static int meson_vpu_write_reg(u32 addr, u32 val)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	MESON_DRM_REG("0x%x, 0x%x\n", addr, val);
	if (rdma_tbl[0].flag)
		ret = meson_drm_write_rdma_table_reg(0, drm_vsync_rdma_handle[0], addr, val);
	else
		ret = rdma_write_reg(drm_vsync_rdma_handle[0], addr, val);
	return ret;
#else
	aml_write_vcbus(addr, val);
	return 0;
#endif
}

static int meson_vpu_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	MESON_DRM_REG("0x%x, 0x%x, %d, %d\n", addr, val, start, len);
	if (rdma_tbl[0].flag)
		ret = meson_drm_write_rdma_table_reg_bits(0, drm_vsync_rdma_handle[0],
			addr, val, start, len);
	else
		ret = rdma_write_reg_bits(drm_vsync_rdma_handle[0], addr, val, start, len);
	return ret;
#else
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
#endif
}

static u32 meson_vpu_dummy_read_reg(u32 addr)
{
	MESON_DRM_REG("%s, 0x%x, dummy_read\n", __func__, addr);
	return 0;
}

static int meson_vpu_dummy_write_reg(u32 addr, u32 val)
{
	MESON_DRM_REG("0x%x, 0x%x dummy_write\n", addr, val);
	return 0;
}

static int meson_vpu_dummy_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	MESON_DRM_REG("0x%x, 0x%x, %d, %d dummy_write\n", addr, val, start, len);
	return 0;
}

static u32 meson_vpu1_read_reg(u32 addr)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	MESON_DRM_REG("0x%x\n", addr);
	if (rdma_tbl[1].flag)
		ret = meson_drm_read_rdma_table_reg(1, drm_vsync_rdma_handle[1], addr);
	else
		ret = rdma_read_reg(drm_vsync_rdma_handle[1], addr);
	return ret;
#else
	return aml_read_vcbus(addr);
#endif
}

static int meson_vpu1_write_reg(u32 addr, u32 val)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	MESON_DRM_REG("0x%x, 0x%x\n", addr, val);
	if (rdma_tbl[1].flag)
		ret = meson_drm_write_rdma_table_reg(1, drm_vsync_rdma_handle[1], addr, val);
	else
		ret = rdma_write_reg(drm_vsync_rdma_handle[1], addr, val);
	return ret;
#else
	aml_write_vcbus(addr, val);
	return 0;
#endif
}

static int meson_vpu1_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	MESON_DRM_REG("0x%x, 0x%x, %d, %d\n", addr, val, start, len);
	if (rdma_tbl[1].flag)
		ret = meson_drm_write_rdma_table_reg_bits(1, drm_vsync_rdma_handle[1],
			addr, val, start, len);
	else
		ret = rdma_write_reg_bits(drm_vsync_rdma_handle[1], addr, val, start, len);
	return ret;
#else
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
#endif
}

static u32 meson_vpu1_read_reg_non_rdma(u32 addr)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

	return aml_read_vcbus(addr);
}

static int meson_vpu1_write_reg_non_rdma(u32 addr, u32 val)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

	aml_write_vcbus(addr, val);
	return 0;
}

static int meson_vpu1_write_reg_bits_non_rdma(u32 addr, u32 val, u32 start, u32 len)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
}

static u32 meson_vpu2_read_reg(u32 addr)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	u32 ret;

	MESON_DRM_REG("0x%x\n", addr);
	if (rdma_tbl[2].flag)
		ret = meson_drm_read_rdma_table_reg(2, drm_vsync_rdma_handle[2], addr);
	else
		ret = rdma_read_reg(drm_vsync_rdma_handle[2], addr);
	return ret;
#else
	return aml_read_vcbus(addr);
#endif
}

static int meson_vpu2_write_reg(u32 addr, u32 val)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	if (rdma_tbl[2].flag)
		ret = meson_drm_write_rdma_table_reg(2, drm_vsync_rdma_handle[2], addr, val);
	else
		ret = rdma_write_reg(drm_vsync_rdma_handle[2], addr, val);
	return ret;
#else
	aml_write_vcbus(addr, val);
	return 0;
#endif
}

static int meson_vpu2_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

#ifdef CONFIG_AMLOGIC_MEDIA_RDMA
	int ret;

	MESON_DRM_REG("0x%x, 0x%x, %d, %d\n", addr, val, start, len);
	if (rdma_tbl[2].flag)
		ret = meson_drm_write_rdma_table_reg_bits(2, drm_vsync_rdma_handle[2],
			addr, val, start, len);
	else
		ret = rdma_write_reg_bits(drm_vsync_rdma_handle[2], addr, val, start, len);
	return ret;
#else
	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
	return 0;
#endif
}

struct rdma_reg_ops common_reg_ops[3] = {
	{
		.rdma_read_reg = meson_vpu_read_reg,
		.rdma_write_reg = meson_vpu_write_reg,
		.rdma_write_reg_bits = meson_vpu_write_reg_bits,
		.dummy_read_reg = meson_vpu_dummy_read_reg,
		.dummy_write_reg = meson_vpu_dummy_write_reg,
		.dummy_write_reg_bits = meson_vpu_dummy_write_reg_bits,
	},
	{
		.rdma_read_reg = meson_vpu1_read_reg,
		.rdma_write_reg = meson_vpu1_write_reg,
		.rdma_write_reg_bits = meson_vpu1_write_reg_bits,

	},
	{
		.rdma_read_reg = meson_vpu2_read_reg,
		.rdma_write_reg = meson_vpu2_write_reg,
		.rdma_write_reg_bits = meson_vpu2_write_reg_bits,
	},
};

struct rdma_reg_ops g12b_reg_ops[2] = {
	{
		.rdma_read_reg = meson_vpu_read_reg,
		.rdma_write_reg = meson_vpu_write_reg,
		.rdma_write_reg_bits = meson_vpu_write_reg_bits,
	},
	{
		.rdma_read_reg = meson_vpu1_read_reg_non_rdma,
		.rdma_write_reg = meson_vpu1_write_reg_non_rdma,
		.rdma_write_reg_bits = meson_vpu1_write_reg_bits_non_rdma,
	},
};

struct rdma_reg_ops c5_reg_ops[1] = {
	{
		.rdma_read_reg = meson_vpu1_read_reg_non_rdma,
		.rdma_write_reg = meson_vpu1_write_reg_non_rdma,
		.rdma_write_reg_bits = meson_vpu1_write_reg_bits_non_rdma,
	},
};

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
void *osd_secure_op[VPP_TOP_MAX] = {
		meson_vpu_write_reg_bits,
		meson_vpu1_write_reg_bits,
		meson_vpu2_write_reg_bits,
};
#endif

/** reg direct access without rdma **/
u32 meson_drm_read_reg(u32 addr)
{
	u32 val;

	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return -EINVAL;
	}

	val = aml_read_vcbus(addr);

	return val;
}

void meson_drm_write_reg(u32 addr, u32 val)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return;
	}

	aml_write_vcbus(addr, val);
}

void meson_drm_write_reg_bits(u32 addr, u32 val, u32 start, u32 len)
{
	if (!addr) {
		MESON_DRM_REG("addr is 0, skip!\n");
		return;
	}

	aml_vcbus_update_bits(addr, ((1 << len) - 1) << start, val << start);
}
