// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <drm/drm_blend.h>

#ifdef CONFIG_AMLOGIC_MEDIA_CANVAS
#include <linux/amlogic/media/canvas/canvas.h>
#include <linux/amlogic/media/canvas/canvas_mgr.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT
#include <linux/amlogic/media/amvecm/amvecm.h>
#endif
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
#include <linux/amlogic/media/vpu_secure/vpu_secure.h>
#endif

#include "meson_vpu_pipeline.h"
#include "meson_crtc.h"
#include "meson_vpu_reg.h"
#include "meson_vpu_util.h"

u32 frame_seq[MESON_MAX_OSDS];

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
u32 original_swap_t3x[HW_OSD_MIF_NUM];
u32 original_osd1_fifo_ctrl_stat_t3x[HW_OSD_MIF_NUM];
#endif

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
static u32 osd_secure_input_index[] = {OSD1_INPUT_SECURE,
	OSD2_INPUT_SECURE, OSD3_INPUT_SECURE, OSD4_INPUT_SECURE};
#endif

static unsigned int osd_canvas[MESON_MAX_OSDS][MAX_CANVAS_NUM];
static u32 canvas_index[MESON_MAX_OSDS] = {-1, -1, -1, -1};

static struct osd_mif_reg_s osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT,
		VIU_OSD1_CTRL_STAT2,
		VIU_OSD1_COLOR_ADDR,
		VIU_OSD1_COLOR,
		VIU_OSD1_TCOLOR_AG0,
		VIU_OSD1_TCOLOR_AG1,
		VIU_OSD1_TCOLOR_AG2,
		VIU_OSD1_TCOLOR_AG3,
		VIU_OSD1_BLK0_CFG_W0,
		VIU_OSD1_BLK0_CFG_W1,
		VIU_OSD1_BLK0_CFG_W2,
		VIU_OSD1_BLK0_CFG_W3,
		VIU_OSD1_BLK0_CFG_W4,
		VIU_OSD1_BLK1_CFG_W4,
		VIU_OSD1_BLK2_CFG_W4,
		VIU_OSD1_FIFO_CTRL_STAT,
		VIU_OSD1_TEST_RDDATA,
		VIU_OSD1_PROT_CTRL,
		VIU_OSD1_MALI_UNPACK_CTRL,
		VIU_OSD1_DIMM_CTRL,
		VIU_OSD1_NORMAL_SWAP,
	},
	{
		VIU_OSD2_CTRL_STAT,
		VIU_OSD2_CTRL_STAT2,
		VIU_OSD2_COLOR_ADDR,
		VIU_OSD2_COLOR,
		VIU_OSD2_TCOLOR_AG0,
		VIU_OSD2_TCOLOR_AG1,
		VIU_OSD2_TCOLOR_AG2,
		VIU_OSD2_TCOLOR_AG3,
		VIU_OSD2_BLK0_CFG_W0,
		VIU_OSD2_BLK0_CFG_W1,
		VIU_OSD2_BLK0_CFG_W2,
		VIU_OSD2_BLK0_CFG_W3,
		VIU_OSD2_BLK0_CFG_W4,
		VIU_OSD2_BLK1_CFG_W4,
		VIU_OSD2_BLK2_CFG_W4,
		VIU_OSD2_FIFO_CTRL_STAT,
		VIU_OSD2_TEST_RDDATA,
		VIU_OSD2_PROT_CTRL,
		VIU_OSD2_MALI_UNPACK_CTRL,
		VIU_OSD2_DIMM_CTRL,
		VIU_OSD2_NORMAL_SWAP,
	},
	{
		VIU_OSD3_CTRL_STAT,
		VIU_OSD3_CTRL_STAT2,
		VIU_OSD3_COLOR_ADDR,
		VIU_OSD3_COLOR,
		VIU_OSD3_TCOLOR_AG0,
		VIU_OSD3_TCOLOR_AG1,
		VIU_OSD3_TCOLOR_AG2,
		VIU_OSD3_TCOLOR_AG3,
		VIU_OSD3_BLK0_CFG_W0,
		VIU_OSD3_BLK0_CFG_W1,
		VIU_OSD3_BLK0_CFG_W2,
		VIU_OSD3_BLK0_CFG_W3,
		VIU_OSD3_BLK0_CFG_W4,
		VIU_OSD3_BLK1_CFG_W4,
		VIU_OSD3_BLK2_CFG_W4,
		VIU_OSD3_FIFO_CTRL_STAT,
		VIU_OSD3_TEST_RDDATA,
		VIU_OSD3_PROT_CTRL,
		VIU_OSD3_MALI_UNPACK_CTRL,
		VIU_OSD3_DIMM_CTRL,
		VIU_OSD3_NORMAL_SWAP,
	},
	{
		VIU_OSD4_CTRL_STAT,
		VIU_OSD4_CTRL_STAT2,
		VIU_OSD4_COLOR_ADDR,
		VIU_OSD4_COLOR,
		VIU_OSD4_TCOLOR_AG0,
		VIU_OSD4_TCOLOR_AG1,
		VIU_OSD4_TCOLOR_AG2,
		VIU_OSD4_TCOLOR_AG3,
		VIU_OSD4_BLK0_CFG_W0,
		VIU_OSD4_BLK0_CFG_W1,
		VIU_OSD4_BLK0_CFG_W2,
		VIU_OSD4_BLK0_CFG_W3,
		VIU_OSD4_BLK0_CFG_W4,
		VIU_OSD4_BLK1_CFG_W4,
		VIU_OSD4_BLK2_CFG_W4,
		VIU_OSD4_FIFO_CTRL_STAT,
		VIU_OSD4_TEST_RDDATA,
		VIU_OSD4_PROT_CTRL,
		VIU_OSD4_MALI_UNPACK_CTRL,
		VIU_OSD4_DIMM_CTRL,
		VIU_OSD4_NORMAL_SWAP,
	}
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static struct osd_mif_reg_s g12b_osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT,
		VIU_OSD1_CTRL_STAT2,
		VIU_OSD1_COLOR_ADDR,
		VIU_OSD1_COLOR,
		VIU_OSD1_TCOLOR_AG0,
		VIU_OSD1_TCOLOR_AG1,
		VIU_OSD1_TCOLOR_AG2,
		VIU_OSD1_TCOLOR_AG3,
		VIU_OSD1_BLK0_CFG_W0,
		VIU_OSD1_BLK0_CFG_W1,
		VIU_OSD1_BLK0_CFG_W2,
		VIU_OSD1_BLK0_CFG_W3,
		VIU_OSD1_BLK0_CFG_W4,
		VIU_OSD1_BLK1_CFG_W4,
		VIU_OSD1_BLK2_CFG_W4,
		VIU_OSD1_FIFO_CTRL_STAT,
		VIU_OSD1_TEST_RDDATA,
		VIU_OSD1_PROT_CTRL,
		VIU_OSD1_MALI_UNPACK_CTRL,
		VIU_OSD1_DIMM_CTRL,
	},
	{
		VIU_OSD2_CTRL_STAT,
		VIU_OSD2_CTRL_STAT2,
		VIU_OSD2_COLOR_ADDR,
		VIU_OSD2_COLOR,
		VIU_OSD2_TCOLOR_AG0,
		VIU_OSD2_TCOLOR_AG1,
		VIU_OSD2_TCOLOR_AG2,
		VIU_OSD2_TCOLOR_AG3,
		VIU_OSD2_BLK0_CFG_W0,
		VIU_OSD2_BLK0_CFG_W1,
		VIU_OSD2_BLK0_CFG_W2,
		VIU_OSD2_BLK0_CFG_W3,
		VIU_OSD2_BLK0_CFG_W4,
		VIU_OSD2_BLK1_CFG_W4,
		VIU_OSD2_BLK2_CFG_W4,
		VIU_OSD2_FIFO_CTRL_STAT,
		VIU_OSD2_TEST_RDDATA,
		VIU_OSD2_PROT_CTRL,
		VIU_OSD2_MALI_UNPACK_CTRL,
		VIU_OSD2_DIMM_CTRL,
	},
	{
		VIU_OSD3_CTRL_STAT,
		VIU_OSD3_CTRL_STAT2,
		VIU_OSD3_COLOR_ADDR,
		VIU_OSD3_COLOR,
		VIU_OSD3_TCOLOR_AG0,
		VIU_OSD3_TCOLOR_AG1,
		VIU_OSD3_TCOLOR_AG2,
		VIU_OSD3_TCOLOR_AG3,
		VIU_OSD3_BLK0_CFG_W0,
		VIU_OSD3_BLK0_CFG_W1,
		VIU_OSD3_BLK0_CFG_W2,
		VIU_OSD3_BLK0_CFG_W3,
		VIU_OSD3_BLK0_CFG_W4,
		VIU_OSD3_BLK1_CFG_W4,
		VIU_OSD3_BLK2_CFG_W4,
		VIU_OSD3_FIFO_CTRL_STAT,
		VIU_OSD3_TEST_RDDATA,
		VIU_OSD3_PROT_CTRL,
		VIU_OSD3_MALI_UNPACK_CTRL,
		VIU_OSD3_DIMM_CTRL,
	},
	{
		VIU2_OSD1_CTRL_STAT,
		VIU2_OSD1_CTRL_STAT2,
		VIU2_OSD1_COLOR_ADDR,
		VIU2_OSD1_COLOR,
		VIU2_OSD1_TCOLOR_AG0,
		VIU2_OSD1_TCOLOR_AG1,
		VIU2_OSD1_TCOLOR_AG2,
		VIU2_OSD1_TCOLOR_AG3,
		VIU2_OSD1_BLK0_CFG_W0,
		VIU2_OSD1_BLK0_CFG_W1,
		VIU2_OSD1_BLK0_CFG_W2,
		VIU2_OSD1_BLK0_CFG_W3,
		VIU2_OSD1_BLK0_CFG_W4,
		VIU2_OSD1_BLK1_CFG_W4,
		VIU2_OSD1_BLK2_CFG_W4,
		VIU2_OSD1_FIFO_CTRL_STAT,
		VIU2_OSD1_TEST_RDDATA,
		VIU2_OSD1_PROT_CTRL,
		VIU2_OSD1_MALI_UNPACK_CTRL,
		VIU2_OSD1_DIMM_CTRL,
	}
};

static struct osd_mif_reg_s s5_osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT_S5,
		VIU_OSD1_CTRL_STAT2_S5,
		VIU_OSD1_COLOR_ADDR_S5,
		VIU_OSD1_COLOR_S5,
		VIU_OSD1_TCOLOR_AG0_S5,
		VIU_OSD1_TCOLOR_AG1_S5,
		VIU_OSD1_TCOLOR_AG2_S5,
		VIU_OSD1_TCOLOR_AG3_S5,
		VIU_OSD1_BLK0_CFG_W0_S5,
		VIU_OSD1_BLK0_CFG_W1_S5,
		VIU_OSD1_BLK0_CFG_W2_S5,
		VIU_OSD1_BLK0_CFG_W3_S5,
		VIU_OSD1_BLK0_CFG_W4_S5,
		VIU_OSD1_BLK1_CFG_W4_S5,
		VIU_OSD1_BLK2_CFG_W4_S5,
		VIU_OSD1_FIFO_CTRL_STAT_S5,
		VIU_OSD1_TEST_RDDATA_S5,
		VIU_OSD1_PROT_CTRL_S5,
		VIU_OSD1_MALI_UNPACK_CTRL_S5,
		VIU_OSD1_DIMM_CTRL_S5,
		VIU_OSD1_NORMAL_SWAP_S5,
	},
	{
		VIU_OSD2_CTRL_STAT_S5,
		VIU_OSD2_CTRL_STAT2_S5,
		VIU_OSD2_COLOR_ADDR_S5,
		VIU_OSD2_COLOR_S5,
		VIU_OSD2_TCOLOR_AG0_S5,
		VIU_OSD2_TCOLOR_AG1_S5,
		VIU_OSD2_TCOLOR_AG2_S5,
		VIU_OSD2_TCOLOR_AG3_S5,
		VIU_OSD2_BLK0_CFG_W0_S5,
		VIU_OSD2_BLK0_CFG_W1_S5,
		VIU_OSD2_BLK0_CFG_W2_S5,
		VIU_OSD2_BLK0_CFG_W3_S5,
		VIU_OSD2_BLK0_CFG_W4_S5,
		VIU_OSD2_BLK1_CFG_W4_S5,
		VIU_OSD2_BLK2_CFG_W4_S5,
		VIU_OSD2_FIFO_CTRL_STAT_S5,
		VIU_OSD2_TEST_RDDATA_S5,
		VIU_OSD2_PROT_CTRL_S5,
		VIU_OSD2_MALI_UNPACK_CTRL_S5,
		VIU_OSD2_DIMM_CTRL_S5,
		VIU_OSD2_NORMAL_SWAP_S5,
	},
	{
		VIU_OSD3_CTRL_STAT_S5,
		VIU_OSD3_CTRL_STAT2_S5,
		VIU_OSD3_COLOR_ADDR_S5,
		VIU_OSD3_COLOR_S5,
		VIU_OSD3_TCOLOR_AG0_S5,
		VIU_OSD3_TCOLOR_AG1_S5,
		VIU_OSD3_TCOLOR_AG2_S5,
		VIU_OSD3_TCOLOR_AG3_S5,
		VIU_OSD3_BLK0_CFG_W0_S5,
		VIU_OSD3_BLK0_CFG_W1_S5,
		VIU_OSD3_BLK0_CFG_W2_S5,
		VIU_OSD3_BLK0_CFG_W3_S5,
		VIU_OSD3_BLK0_CFG_W4_S5,
		VIU_OSD3_BLK1_CFG_W4_S5,
		VIU_OSD3_BLK2_CFG_W4_S5,
		VIU_OSD3_FIFO_CTRL_STAT_S5,
		VIU_OSD3_TEST_RDDATA_S5,
		VIU_OSD3_PROT_CTRL_S5,
		VIU_OSD3_MALI_UNPACK_CTRL_S5,
		VIU_OSD3_DIMM_CTRL_S5,
		VIU_OSD3_NORMAL_SWAP_S5,
	},
	{
		VIU_OSD4_CTRL_STAT_S5,
		VIU_OSD4_CTRL_STAT2_S5,
		VIU_OSD4_COLOR_ADDR_S5,
		VIU_OSD4_COLOR_S5,
		VIU_OSD4_TCOLOR_AG0_S5,
		VIU_OSD4_TCOLOR_AG1_S5,
		VIU_OSD4_TCOLOR_AG2_S5,
		VIU_OSD4_TCOLOR_AG3_S5,
		VIU_OSD4_BLK0_CFG_W0_S5,
		VIU_OSD4_BLK0_CFG_W1_S5,
		VIU_OSD4_BLK0_CFG_W2_S5,
		VIU_OSD4_BLK0_CFG_W3_S5,
		VIU_OSD4_BLK0_CFG_W4_S5,
		VIU_OSD4_BLK1_CFG_W4_S5,
		VIU_OSD4_BLK2_CFG_W4_S5,
		VIU_OSD4_FIFO_CTRL_STAT_S5,
		VIU_OSD4_TEST_RDDATA_S5,
		VIU_OSD4_PROT_CTRL_S5,
		VIU_OSD4_MALI_UNPACK_CTRL_S5,
		VIU_OSD4_DIMM_CTRL_S5,
		VIU_OSD4_NORMAL_SWAP_S5
	},
};

static struct osd_mif_reg_s s6_osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT,
		VIU_OSD1_CTRL_STAT2,
		VIU_OSD1_COLOR_ADDR,
		VIU_OSD1_COLOR,
		VIU_OSD1_TCOLOR_AG0,
		VIU_OSD1_TCOLOR_AG1,
		VIU_OSD1_TCOLOR_AG2,
		VIU_OSD1_TCOLOR_AG3,
		VIU_OSD1_BLK0_CFG_W0,
		VIU_OSD1_BLK0_CFG_W1,
		VIU_OSD1_BLK0_CFG_W2,
		VIU_OSD1_BLK0_CFG_W3,
		VIU_OSD1_BLK0_CFG_W4,
		VIU_OSD1_BLK1_CFG_W4,
		VIU_OSD1_BLK2_CFG_W4,
		VIU_OSD1_FIFO_CTRL_STAT,
		VIU_OSD1_TEST_RDDATA,
		VIU_OSD1_PROT_CTRL,
		VIU_OSD1_MALI_UNPACK_CTRL,
		VIU_OSD1_DIMM_CTRL,
	},
	{
		VIU_OSD2_CTRL_STAT,
		VIU_OSD2_CTRL_STAT2,
		VIU_OSD2_COLOR_ADDR,
		VIU_OSD2_COLOR,
		VIU_OSD2_TCOLOR_AG0,
		VIU_OSD2_TCOLOR_AG1,
		VIU_OSD2_TCOLOR_AG2,
		VIU_OSD2_TCOLOR_AG3,
		VIU_OSD2_BLK0_CFG_W0,
		VIU_OSD2_BLK0_CFG_W1,
		VIU_OSD2_BLK0_CFG_W2,
		VIU_OSD2_BLK0_CFG_W3,
		VIU_OSD2_BLK0_CFG_W4,
		VIU_OSD2_BLK1_CFG_W4,
		VIU_OSD2_BLK2_CFG_W4,
		VIU_OSD2_FIFO_CTRL_STAT,
		VIU_OSD2_TEST_RDDATA,
		VIU_OSD2_PROT_CTRL,
		VIU_OSD2_MALI_UNPACK_CTRL,
		VIU_OSD2_DIMM_CTRL,
	},
	{
		VIU2_OSD1_CTRL_STAT,
		VIU2_OSD1_CTRL_STAT2,
		VIU2_OSD1_COLOR_ADDR,
		VIU2_OSD1_COLOR,
		VIU2_OSD1_TCOLOR_AG0,
		VIU2_OSD1_TCOLOR_AG1,
		VIU2_OSD1_TCOLOR_AG2,
		VIU2_OSD1_TCOLOR_AG3,
		VIU2_OSD1_BLK0_CFG_W0,
		VIU2_OSD1_BLK0_CFG_W1,
		VIU2_OSD1_BLK0_CFG_W2,
		VIU2_OSD1_BLK0_CFG_W3,
		VIU2_OSD1_BLK0_CFG_W4,
		VIU2_OSD1_BLK1_CFG_W4,
		VIU2_OSD1_BLK2_CFG_W4,
		VIU2_OSD1_FIFO_CTRL_STAT,
		VIU2_OSD1_TEST_RDDATA,
		VIU2_OSD1_PROT_CTRL,
		VIU2_OSD1_MALI_UNPACK_CTRL,
		VIU2_OSD1_DIMM_CTRL,
	},
	{}
};

/*
 * add VIU_OSD_NORMAL_SWAP register compare with g12a/g12b
 */
static struct osd_mif_reg_s t6d_osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT,
		VIU_OSD1_CTRL_STAT2,
		VIU_OSD1_COLOR_ADDR,
		VIU_OSD1_COLOR,
		VIU_OSD1_TCOLOR_AG0,
		VIU_OSD1_TCOLOR_AG1,
		VIU_OSD1_TCOLOR_AG2,
		VIU_OSD1_TCOLOR_AG3,
		VIU_OSD1_BLK0_CFG_W0,
		VIU_OSD1_BLK0_CFG_W1,
		VIU_OSD1_BLK0_CFG_W2,
		VIU_OSD1_BLK0_CFG_W3,
		VIU_OSD1_BLK0_CFG_W4,
		VIU_OSD1_BLK1_CFG_W4,
		VIU_OSD1_BLK2_CFG_W4,
		VIU_OSD1_FIFO_CTRL_STAT,
		VIU_OSD1_TEST_RDDATA,
		VIU_OSD1_PROT_CTRL,
		VIU_OSD1_MALI_UNPACK_CTRL,
		VIU_OSD1_DIMM_CTRL,
		VIU_OSD1_NORMAL_SWAP,
	},
	{
		VIU_OSD2_CTRL_STAT,
		VIU_OSD2_CTRL_STAT2,
		VIU_OSD2_COLOR_ADDR,
		VIU_OSD2_COLOR,
		VIU_OSD2_TCOLOR_AG0,
		VIU_OSD2_TCOLOR_AG1,
		VIU_OSD2_TCOLOR_AG2,
		VIU_OSD2_TCOLOR_AG3,
		VIU_OSD2_BLK0_CFG_W0,
		VIU_OSD2_BLK0_CFG_W1,
		VIU_OSD2_BLK0_CFG_W2,
		VIU_OSD2_BLK0_CFG_W3,
		VIU_OSD2_BLK0_CFG_W4,
		VIU_OSD2_BLK1_CFG_W4,
		VIU_OSD2_BLK2_CFG_W4,
		VIU_OSD2_FIFO_CTRL_STAT,
		VIU_OSD2_TEST_RDDATA,
		VIU_OSD2_PROT_CTRL,
		VIU_OSD2_MALI_UNPACK_CTRL,
		VIU_OSD2_DIMM_CTRL,
		VIU_OSD2_NORMAL_SWAP,
	},
	{
		VIU_OSD3_CTRL_STAT,
		VIU_OSD3_CTRL_STAT2,
		VIU_OSD3_COLOR_ADDR,
		VIU_OSD3_COLOR,
		VIU_OSD3_TCOLOR_AG0,
		VIU_OSD3_TCOLOR_AG1,
		VIU_OSD3_TCOLOR_AG2,
		VIU_OSD3_TCOLOR_AG3,
		VIU_OSD3_BLK0_CFG_W0,
		VIU_OSD3_BLK0_CFG_W1,
		VIU_OSD3_BLK0_CFG_W2,
		VIU_OSD3_BLK0_CFG_W3,
		VIU_OSD3_BLK0_CFG_W4,
		VIU_OSD3_BLK1_CFG_W4,
		VIU_OSD3_BLK2_CFG_W4,
		VIU_OSD3_FIFO_CTRL_STAT,
		VIU_OSD3_TEST_RDDATA,
		VIU_OSD3_PROT_CTRL,
		VIU_OSD3_MALI_UNPACK_CTRL,
		VIU_OSD3_DIMM_CTRL,
		VIU_OSD3_NORMAL_SWAP,
	},
};

static struct osd_mif_reg_s a9_osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VIU_OSD1_CTRL_STAT,
		VIU_OSD1_CTRL_STAT2,
		VIU_OSD1_COLOR_ADDR,
		VIU_OSD1_COLOR,
		VIU_OSD1_TCOLOR_AG0,
		VIU_OSD1_TCOLOR_AG1,
		VIU_OSD1_TCOLOR_AG2,
		VIU_OSD1_TCOLOR_AG3,
		VIU_OSD1_BLK0_CFG_W0,
		VIU_OSD1_BLK0_CFG_W1,
		VIU_OSD1_BLK0_CFG_W2,
		VIU_OSD1_BLK0_CFG_W3,
		VIU_OSD1_BLK0_CFG_W4,
		VIU_OSD1_BLK1_CFG_W4,
		VIU_OSD1_BLK2_CFG_W4,
		VIU_OSD1_FIFO_CTRL_STAT,
		VIU_OSD1_TEST_RDDATA,
		VIU_OSD1_PROT_CTRL,
		VIU_OSD1_MALI_UNPACK_CTRL,
		VIU_OSD1_DIMM_CTRL,
		VIU_OSD1_NORMAL_SWAP,// new add
		VIU_OSD1_MIF_CTRL,//  new add
		VIU_OSD1_MIF_CTRL1,//  new add
	},
	{
		VIU_OSD2_CTRL_STAT,
		VIU_OSD2_CTRL_STAT2,
		VIU_OSD2_COLOR_ADDR,
		VIU_OSD2_COLOR,
		VIU_OSD2_TCOLOR_AG0,
		VIU_OSD2_TCOLOR_AG1,
		VIU_OSD2_TCOLOR_AG2,
		VIU_OSD2_TCOLOR_AG3,
		VIU_OSD2_BLK0_CFG_W0,
		VIU_OSD2_BLK0_CFG_W1,
		VIU_OSD2_BLK0_CFG_W2,
		VIU_OSD2_BLK0_CFG_W3,
		VIU_OSD2_BLK0_CFG_W4,
		VIU_OSD2_BLK1_CFG_W4,
		VIU_OSD2_BLK2_CFG_W4,
		VIU_OSD2_FIFO_CTRL_STAT,
		VIU_OSD2_TEST_RDDATA,
		VIU_OSD2_PROT_CTRL,
		VIU_OSD2_MALI_UNPACK_CTRL,
		VIU_OSD2_DIMM_CTRL,
		VIU_OSD2_NORMAL_SWAP,
		VIU_OSD2_MIF_CTRL,
		VIU_OSD2_MIF_CTRL1,
	},
	{
		VIU_OSD3_CTRL_STAT,
		VIU_OSD3_CTRL_STAT2,
		VIU_OSD3_COLOR_ADDR,
		VIU_OSD3_COLOR,
		VIU_OSD3_TCOLOR_AG0,
		VIU_OSD3_TCOLOR_AG1,
		VIU_OSD3_TCOLOR_AG2,
		VIU_OSD3_TCOLOR_AG3,
		VIU_OSD3_BLK0_CFG_W0,
		VIU_OSD3_BLK0_CFG_W1,
		VIU_OSD3_BLK0_CFG_W2,
		VIU_OSD3_BLK0_CFG_W3,
		VIU_OSD3_BLK0_CFG_W4,
		VIU_OSD3_BLK1_CFG_W4,
		VIU_OSD3_BLK2_CFG_W4,
		VIU_OSD3_FIFO_CTRL_STAT,
		VIU_OSD3_TEST_RDDATA,
		VIU_OSD3_PROT_CTRL,
		VIU_OSD3_MALI_UNPACK_CTRL,
		VIU_OSD3_DIMM_CTRL,
		VIU_OSD3_NORMAL_SWAP,
		VIU_OSD3_MIF_CTRL,
		VIU_OSD3_MIF_CTRL1,
	},
	{
		VIU_OSD4_CTRL_STAT,
		VIU_OSD4_CTRL_STAT2,
		VIU_OSD4_COLOR_ADDR,
		VIU_OSD4_COLOR,
		VIU_OSD4_TCOLOR_AG0,
		VIU_OSD4_TCOLOR_AG1,
		VIU_OSD4_TCOLOR_AG2,
		VIU_OSD4_TCOLOR_AG3,
		VIU_OSD4_BLK0_CFG_W0,
		VIU_OSD4_BLK0_CFG_W1,
		VIU_OSD4_BLK0_CFG_W2,
		VIU_OSD4_BLK0_CFG_W3,
		VIU_OSD4_BLK0_CFG_W4,
		VIU_OSD4_BLK1_CFG_W4,
		VIU_OSD4_BLK2_CFG_W4,
		VIU_OSD4_FIFO_CTRL_STAT,
		VIU_OSD4_TEST_RDDATA,
		VIU_OSD4_PROT_CTRL,
		VIU_OSD4_MALI_UNPACK_CTRL,
		VIU_OSD4_DIMM_CTRL,
		VIU_OSD4_NORMAL_SWAP,
		VIU_OSD4_MIF_CTRL,
		VIU_OSD4_MIF_CTRL1,
	},
	{
		VIU_OSD5_CTRL_STAT,
		VIU_OSD5_CTRL_STAT2,
		VIU_OSD5_COLOR_ADDR,
		VIU_OSD5_COLOR,
		VIU_OSD5_TCOLOR_AG0,
		VIU_OSD5_TCOLOR_AG1,
		VIU_OSD5_TCOLOR_AG2,
		VIU_OSD5_TCOLOR_AG3,
		VIU_OSD5_BLK0_CFG_W0,
		VIU_OSD5_BLK0_CFG_W1,
		VIU_OSD5_BLK0_CFG_W2,
		VIU_OSD5_BLK0_CFG_W3,
		VIU_OSD5_BLK0_CFG_W4,
		VIU_OSD5_BLK1_CFG_W4,
		VIU_OSD5_BLK2_CFG_W4,
		VIU_OSD5_FIFO_CTRL_STAT,
		VIU_OSD5_TEST_RDDATA,
		VIU_OSD5_PROT_CTRL,
		VIU_OSD5_MALI_UNPACK_CTRL,
		VIU_OSD5_DIMM_CTRL,
		VIU_OSD5_NORMAL_SWAP,
		VIU_OSD5_MIF_CTRL,
		VIU_OSD5_MIF_CTRL1,
	},
};

static struct osd_mif_reg_s c5_osd_mif_reg[HW_OSD_MIF_NUM] = {
	{
		VOUT_OSD1_CTRL_STAT,
		VOUT_OSD1_CTRL_STAT2,
		VOUT_OSD1_COLOR_ADDR,
		VOUT_OSD1_COLOR,
		VOUT_OSD1_TCOLOR_AG0,
		VOUT_OSD1_TCOLOR_AG1,
		VOUT_OSD1_TCOLOR_AG2,
		VOUT_OSD1_TCOLOR_AG3,
		VOUT_OSD1_BLK0_CFG_W0,
		VOUT_OSD1_BLK0_CFG_W1,
		VOUT_OSD1_BLK0_CFG_W2,
		VOUT_OSD1_BLK0_CFG_W3,
		VOUT_OSD1_BLK0_CFG_W4,
		VOUT_OSD1_BLK1_CFG_W4,
		VOUT_OSD1_BLK2_CFG_W4,
		VOUT_OSD1_FIFO_CTRL_STAT,
		VOUT_OSD1_TEST_RDDATA,
		VOUT_OSD1_PROT_CTRL,
		VOUT_OSD1_MALI_UNPACK_CTRL,
		VOUT_OSD1_DIMM_CTRL,
		VOUT_OSD1_NORMAL_SWAP,
	},
	{},
	{},
	{}
};

#endif

/*
 * Internal function to query information for a given format. See
 * meson_drm_format_info() for the public API.
 */
static const struct meson_drm_format_info base_formats[] = {
	{ .format = DRM_FORMAT_XRGB8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ARGB8888,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_XBGR8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ABGR8888,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_RGBX8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_RGBA8888,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_BGRX8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_BGRA8888,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_ARGB8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ARGB8888,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ABGR8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ABGR8888,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGBA8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_RGBA8888,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_BGRA8888,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_BGRA8888,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGBA1010102,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_RGBA1010102,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ARGB2101010,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ARGB2101010,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ABGR2101010,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ABGR2101010,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_BGRA1010102,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_BGRA1010102,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGB888,
		.hw_blkmode = BLOCK_MODE_24BIT,
		.hw_colormat = COLOR_MATRIX_RGB888,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_BGR888,
		.hw_blkmode = BLOCK_MODE_24BIT,
		.hw_colormat = COLOR_MATRIX_BGR888,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGB565,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_565,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_INVALID },
};

static const struct meson_drm_format_info *formats_of_g12[] = {
	base_formats,
	NULL,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static const struct meson_drm_format_info t3x_formats[] = {
	{ .format = DRM_FORMAT_ABGR4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_4444,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ARGB4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_4444,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_BGRA4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_4444_T3X,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGBA4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_4444_T3X,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGB565,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_565_T3X,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_INVALID },
};

static const struct meson_drm_format_info t6d_formats[] = {
	{ .format = DRM_FORMAT_RGBX1010102,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_RGBA1010102,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_XBGR2101010,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ABGR2101010,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_ARGB2101010,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_ARGB2101010_T6D,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_BGRA1010102,
		.hw_blkmode = BLOCK_MODE_32BIT,
		.hw_colormat = COLOR_MATRIX_BGRA1010102_T6D,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGB565,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_565_T6D,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGBA4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_RGBA4444,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ARGB4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_ARGB4444,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGBX4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_RGBA4444,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_XRGB4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_ARGB4444,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_RGBA5551,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_RGBA5551,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ARGB1555,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_ARGB1555,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGBX5551,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_RGBA5551,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_XRGB1555,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_ARGB1555,
		.alpha_replace = 1 },
	{ .format = DRM_FORMAT_C8,
		.hw_blkmode = BLOCK_MODE_8BIT_PER_PIXEL,
		.hw_colormat = 0,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_INVALID },
};

static const struct meson_drm_format_info *formats_of_t3x[] = {
	base_formats,
	t3x_formats,
	NULL,
};

static const struct meson_drm_format_info *formats_of_t6d[] = {
	base_formats,
	t6d_formats,
	NULL,
};
#endif

static const struct meson_drm_format_info s1a_formats[] = {
	{ .format = DRM_FORMAT_RGBA4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_RGBA4444,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ARGB4444,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_ARGB4444,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGBA5551,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_RGBA5551,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_ARGB1555,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_ARGB1555,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_RGB565,
		.hw_blkmode = BLOCK_MODE_16BIT,
		.hw_colormat = COLOR_MATRIX_565_T3X,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_C8,
		.hw_blkmode = BLOCK_MODE_8BIT_PER_PIXEL,
		.hw_colormat = 0,
		.alpha_replace = 0 },
	{ .format = DRM_FORMAT_INVALID },
};

static const struct meson_drm_format_info *formats_of_s1a[] = {
	base_formats,
	s1a_formats,
	NULL,
};

const struct meson_drm_format_info *__meson_drm_format_info(struct meson_vpu_block *vblk,
						u32 format)
{
	unsigned int i, j;
	const struct meson_drm_format_info *info_array, *target_info;
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	const struct meson_drm_format_info **infos = osd->infos;

	if (!infos) {
		WARN_ONCE(1, "formats was not initialized\n");
		infos = formats_of_g12;
	}

	target_info = NULL;

	for (i = 0; infos[i]; i++) {
		info_array = infos[i];
		for (j = 0; info_array[j].format != DRM_FORMAT_INVALID; j++) {
			if (info_array[j].format == format)
				target_info = &info_array[j];
		}
	}

	return target_info;
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
const struct meson_drm_format_info *__meson_drm_afbc_format_info(u32 format)
{
	static const struct meson_drm_format_info formats[] = {
		{ .format = DRM_FORMAT_XRGB8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_XBGR8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_RGBX8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_BGRX8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_ARGB8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_ABGR8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGBA8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_BGRA8888,
			.hw_blkmode = BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGB888,
			.hw_blkmode = BLOCK_MODE_RGB888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGB565,
			.hw_blkmode = BLOCK_MODE_RGB565,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_ABGR2101010,
			.hw_blkmode = BLOCK_MODE_RGBA1010102,
			.alpha_replace = 0 },
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}
#endif

const struct meson_drm_format_info *__meson_drm_gfcd_format_info(u32 format)
{
	static const struct meson_drm_format_info formats[] = {
		{ .format = DRM_FORMAT_RGBA8888,
			.gfcd_hw_blkmode_afbc = GFCD_AFBC_BLOCK_MODE_RGBA8888,
			.gfcd_hw_blkmode_afrc = GFCD_AFRC_BLOCK_MODE_RGBA8888,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_ABGR2101010,
			.gfcd_hw_blkmode_afbc = GFCD_AFBC_BLOCK_MODE_RGBA1010102,
			.gfcd_hw_blkmode_afrc = BLOCK_MODE_RESERVED,
			.alpha_replace = 0 },
		{ .format = DRM_FORMAT_RGB888,
			.gfcd_hw_blkmode_afbc = GFCD_AFBC_BLOCK_MODE_RGB888,
			.gfcd_hw_blkmode_afrc = BLOCK_MODE_RESERVED,
			.alpha_replace = 1 },
		{ .format = DRM_FORMAT_ABGR10101010,
			.gfcd_hw_blkmode_afbc = GFCD_AFBC_BLOCK_MODE_RGBA10101010,
			.gfcd_hw_blkmode_afrc = GFCD_AFRC_BLOCK_MODE_RGBA10101010,
			.alpha_replace = 0 },
	};

	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (formats[i].format == format)
			return &formats[i];
	}

	return NULL;
}

/**
 * meson_drm_format_info - query information for a given format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * The caller should only pass a supported pixel format to this function.
 * Unsupported pixel formats will generate a warning in the kernel log.
 *
 * Returns:
 * The instance of struct meson_drm_format_info that describes the
 * pixel format, or NULL if the format is unsupported.
 */
const struct meson_drm_format_info *meson_drm_format_info(struct meson_vpu_block *vblk, u32 format,
							  bool afbc_en)
{
	const struct meson_drm_format_info *info;

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT_C1A
	if (afbc_en)
		info = __meson_drm_afbc_format_info(format);
	else
#endif
		info = __meson_drm_format_info(vblk, format);
	WARN_ON(!info);
	return info;
}

/**
 * meson_drm_format_hw_blkmode - get the hw_blkmode for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The hw_blkmode match the specified pixel format.
 */
static u8 meson_drm_format_hw_blkmode(struct meson_vpu_block *vblk, u32 format, bool afbc_en)
{
	const struct meson_drm_format_info *info;

	info = meson_drm_format_info(vblk, format, afbc_en);
	return info ? info->hw_blkmode : 0;
}

/**
 * meson_drm_format_hw_colormat - get the hw_colormat for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The hw_colormat match the specified pixel format.
 */
static u8 meson_drm_format_hw_colormat(struct meson_vpu_block *vblk, u32 format, bool afbc_en)
{
	const struct meson_drm_format_info *info;

	info = meson_drm_format_info(vblk, format, afbc_en);
	return info ? info->hw_colormat : 0;
}

/**
 * meson_drm_format_alpha_replace - get the alpha replace for format
 * @format: pixel format (DRM_FORMAT_*)
 *
 * Returns:
 * The alpha_replace match the specified pixel format.
 */
static u8 meson_drm_format_alpha_replace(struct meson_vpu_block *vblk, u32 format, bool afbc_en)
{
	const struct meson_drm_format_info *info;

	info = meson_drm_format_info(vblk, format, afbc_en);
	return info ? info->alpha_replace : 0;
}

static u32 osd_plane_update_canvas(struct meson_vpu_osd *osd,
	struct osd_mif_reg_s *reg, u32 flush_reg)
{
	u32 val, idx, canvas_idx = 0;

	if (!osd) {
		DRM_ERROR("osd is null!\n");
		return -1;
	}

	idx = osd->base.index;
	val = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w0);
	val = (val >> 16) & 0xff;
	if (canvas_index[idx] == -1 || (canvas_index[idx] == val && flush_reg)) {
		MESON_DRM_BLOCK("update canvas_index[%d]:0x%x-0x%x, flush_reg:%d\n",
			idx, canvas_index[idx], val, flush_reg);

		if (!kfifo_get(&osd->canvas_q, &canvas_idx)) {
			DRM_ERROR("canvas_q is NULL!");
			return -EINVAL;
		}

		if (!kfifo_put(&osd->canvas_q, canvas_idx)) {
			DRM_ERROR("canvas_q is full!");
			return -EINVAL;
		}
	}

	if (!kfifo_peek(&osd->canvas_q, &canvas_idx))
		DRM_ERROR("canvas_q peek fail!");

	return canvas_idx;
}

/*osd hold line config*/
void ods_hold_line_config(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, int hold_line)
{
	u32 data = 0, value = 0;

	data = reg_ops->rdma_read_reg(reg->viu_osd_fifo_ctrl_stat);
	value = (data >> 5) & 0x1f;
	if (value != hold_line)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_fifo_ctrl_stat,
					     hold_line & 0x1f, 5, 5);
}

/*osd input size config*/
void osd_input_size_config(struct meson_vpu_block *vblk,
			   struct rdma_reg_ops *reg_ops,
			   struct osd_mif_reg_s *reg, struct osd_scope_s scope)
{
	reg_ops->rdma_write_reg(reg->viu_osd_blk0_cfg_w1,
			    (scope.h_end << 16) | /*x_end pixels[13bits]*/
		scope.h_start/*x_start pixels[13bits]*/);
	reg_ops->rdma_write_reg(reg->viu_osd_blk0_cfg_w2,
			    (scope.v_end << 16) | /*y_end pixels[13bits]*/
		scope.v_start/*y_start pixels[13bits]*/);
}

/*osd canvas config*/
void osd_canvas_config(struct meson_vpu_block *vblk,
		       struct rdma_reg_ops *reg_ops,
		       struct osd_mif_reg_s *reg, u32 canvas_index)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0,
				     canvas_index, 16, 8);
}

static void osd_rpt_y_config(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_mif_reg_s *reg)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0,
				     0, 14, 0);
}

/*osd mali afbc src en
 * 1: read data from mali afbcd;0: read data from DDR directly
 */
void osd_mali_src_en(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_mif_reg_s *reg, u8 osd_index, bool flag)
{
	struct meson_vpu_osd *osd;

	osd = to_osd_block(vblk);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 30, 1);
	if (!osd->is_viu2_osd)
		reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL,
					 flag, (osd_index + 4), 1);
}

void osd_mali_src_en_linear(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_mif_reg_s *reg, u8 osd_index, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 30, 1);

	reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL, (osd_index + 1),
				     (osd_index * 4 + 16), 4);
}

/*osd axi mux
 *1:axi input data to gfcd;0:axi input data to osd mali
 */
void osd_gfcd_axi_input_en(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_mif_reg_s *reg, u8 osd_index, bool flag)
{
	reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL,
				     flag, (osd_index + 14), 1);
}

/*osd dout mux
 *1:osd dout from gfcd; 0:osd dout from osd
 */
void osd_gfcd_dout_en(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_mif_reg_s *reg, u8 osd_index, bool flag)
{
	reg_ops->rdma_write_reg_bits(OSD_PATH_MISC_CTRL,
				     flag, (osd_index + 10), 1);
}

/*
 * osd axi and out config for a9
 * bit26: axi select, bit27: output sel
 */
void a9_osd_in_out_cfg(struct meson_vpu_block *vblk,
		     struct rdma_reg_ops *reg_ops,
		     struct osd_mif_reg_s *reg, u8 osd_index, bool flag)
{
		int val = flag ? 3 : 0;

		switch (osd_index) {
		case MESON_OSD1:
			reg_ops->rdma_write_reg_bits(VIU_OSD1_PATH_CTRL, val, 26, 2);
			break;
		case MESON_OSD2:
			reg_ops->rdma_write_reg_bits(VIU_OSD2_PATH_CTRL, val, 26, 2);
			break;
		case MESON_OSD3:
			reg_ops->rdma_write_reg_bits(VIU_OSD3_PATH_CTRL, val, 26, 2);
			break;
		case MESON_OSD4:
			reg_ops->rdma_write_reg_bits(VIU_OSD4_PATH_CTRL, val, 26, 2);
			break;
		case MESON_OSD5:
			reg_ops->rdma_write_reg_bits(VIU_OSD5_PATH_CTRL, val, 26, 2);
			break;
		default:
			DRM_DEBUG("invalid osd index\n");
			break;
		}
}

/*osd endian mode
 * 1: little endian;0: big endian[for mali afbc input]
 */
void osd_endian_mode(struct meson_vpu_block *vblk, struct rdma_reg_ops *reg_ops,
		     struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 15, 1);
}

/*osd mif enable*/
void osd_block_enable(struct meson_vpu_block *vblk,
		      struct rdma_reg_ops *reg_ops,
		      struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat, flag, 0, 1);
}

void osd_block_mif_enable(struct meson_vpu_block *vblk,
		      struct rdma_reg_ops *reg_ops,
		      struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_mif_ctrl, flag, 27, 1);
}

/*osd mem mode
 * 0: canvas_addr;1:linear_addr[for mali-afbc-mode]
 */
void osd_mem_mode(struct meson_vpu_block *vblk, struct rdma_reg_ops *reg_ops,
		  struct osd_mif_reg_s *reg, bool mode)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat, mode, 2, 1);
}

/*osd alpha_div en
 *if input is premult,alpha_div=1,else alpha_div=0
 */
void osd_global_alpha_set(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, u16 val)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat, val, 12, 9);
}

/*osd alpha_div en
 *if input is premult,alpha_div=1,else alpha_div=0
 */
void osd_premult_enable(struct meson_vpu_block *vblk,
			struct rdma_reg_ops *reg_ops,
			struct osd_mif_reg_s *reg, int flag)
{
	/*afbc*/
	reg_ops->rdma_write_reg_bits(reg->viu_osd_mali_unpack_ctrl,
				     flag, 28, 1);
	/*mif*/
	//meson_vpu_write_reg_bits(reg->viu_osd_ctrl_stat, flag, 1, 1);
}

/*osd x reverse en
 *reverse read in X direction
 */
void osd_reverse_x_enable(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 28, 1);
}

/*osd y reverse en
 *reverse read in Y direction
 */
void osd_reverse_y_enable(struct meson_vpu_block *vblk,
			  struct rdma_reg_ops *reg_ops,
			  struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, flag, 29, 1);
}

/*osd mali unpack en
 * 1: osd will unpack mali_afbc_src;0:osd will unpack normal src
 */
void osd_mali_unpack_enable(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_mif_reg_s *reg, bool flag)
{
	reg_ops->rdma_write_reg_bits(reg->viu_osd_mali_unpack_ctrl, flag, 31, 1);
}

static void osd_color_config(struct meson_vpu_block *vblk,
			     struct rdma_reg_ops *reg_ops,
			     struct osd_mif_reg_s *reg,
			     u32 pixel_format, u32 pixel_blend, bool afbc_en)
{
	u8 blk_mode, color, alpha_replace, reg_pix_bits_mode;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (osd->format_swap) {
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
		/*
		 * color_expand_mode is 1 for 16-bit pixel format, expand each color component
		 * to 8bits by padding right-most bits with left-most bits
		 */
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 1, 0, 1);
		blk_mode = meson_drm_format_hw_blkmode(vblk, pixel_format, afbc_en);
		color = meson_drm_format_hw_colormat(vblk, pixel_format, afbc_en);
		alpha_replace = (pixel_blend == DRM_MODE_BLEND_PIXEL_NONE) ||
		meson_drm_format_alpha_replace(vblk, pixel_format, afbc_en);
		/* t3x pixel format workaround */
		reg_ops->rdma_write_reg(reg->viu_osd_normal_swap, original_swap_t3x[vblk->index]);
		reg_ops->rdma_write_reg(reg->viu_osd_fifo_ctrl_stat,
						original_osd1_fifo_ctrl_stat_t3x[vblk->index]);
		if (pixel_format == DRM_FORMAT_ARGB4444) {
			reg_ops->rdma_write_reg(reg->viu_osd_normal_swap, 0x1230);
		} else if (pixel_format == DRM_FORMAT_BGRA4444) {
			reg_ops->rdma_write_reg_bits(reg->viu_osd_fifo_ctrl_stat, 1, 30, 1);
		} else if (pixel_format == DRM_FORMAT_RGBA4444) {
			reg_ops->rdma_write_reg(reg->viu_osd_normal_swap, 0x1230);
			reg_ops->rdma_write_reg_bits(reg->viu_osd_fifo_ctrl_stat, 1, 30, 1);
		}
#endif
	} else {
		blk_mode = meson_drm_format_hw_blkmode(vblk, pixel_format, afbc_en);
		color = meson_drm_format_hw_colormat(vblk, pixel_format, afbc_en);
		alpha_replace = (pixel_blend == DRM_MODE_BLEND_PIXEL_NONE) ||
			meson_drm_format_alpha_replace(vblk, pixel_format, afbc_en);
	}

	if (osd->format_swap_AR30_BA30) {
		if (pixel_format == DRM_FORMAT_ARGB2101010 ||
			pixel_format == DRM_FORMAT_BGRA1010102 ||
			pixel_format == DRM_FORMAT_XRGB2101010 ||
			pixel_format == DRM_FORMAT_BGRX1010102)
			reg_ops->rdma_write_reg(reg->viu_osd_normal_swap, 0x1230);
		else
			reg_ops->rdma_write_reg(reg->viu_osd_normal_swap, 0x3210);
	}

	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0,
					blk_mode, 8, 4);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0,
					color, 2, 4);

	if (osd->no_afbc_ctrl) {
		switch (blk_mode) {
		case BLOCK_MODE_16BIT:
			reg_pix_bits_mode = 2;
			break;
		case BLOCK_MODE_24BIT:
			reg_pix_bits_mode = 9;
			break;
		case BLOCK_MODE_32BIT:
			reg_pix_bits_mode = 3;
			break;
		default:
			reg_pix_bits_mode = 3;
			break;
		}

		reg_ops->rdma_write_reg_bits(reg->viu_osd_mif_ctrl, reg_pix_bits_mode, 9, 4);
	}

	if (alpha_replace)
		/*replace alpha    : bit 14 enable, 6~13 alpha val.*/
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0x1ff, 6, 9);
	else
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0x0, 6, 9);
}

static void osd_afbc_config(struct meson_vpu_block *vblk,
			    struct rdma_reg_ops *reg_ops,
			    struct osd_mif_reg_s *reg,
			    u8 osd_index, bool afbc_en)
{
	struct meson_vpu_osd *osd;

	osd = to_osd_block(vblk);

	if (!afbc_en)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0, 1, 1);
	else
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 1, 1, 1);

	if (!osd->no_afbc_ctrl)
		osd_mali_unpack_enable(vblk, reg_ops, reg, afbc_en);
	osd_mali_src_en(vblk, reg_ops, reg, osd_index, afbc_en);
	osd_endian_mode(vblk, reg_ops, reg, !afbc_en);
	osd_mem_mode(vblk, reg_ops, reg, afbc_en);
}

static void osd_afbc_config_linear(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osd_mif_reg_s *reg,
			       u8 osd_index, u32 pixel_format, bool afbc_en)
{
	struct meson_vpu_osd *osd;

	osd = to_osd_block(vblk);

	if (!afbc_en)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0, 1, 1);
	else
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 1, 1, 1);

	if (!osd->no_afbc_ctrl)
		osd_mali_unpack_enable(vblk, reg_ops, reg, afbc_en);
	if (pixel_format == DRM_FORMAT_RGBA1010102 ||
	    pixel_format == DRM_FORMAT_ARGB2101010 ||
	    pixel_format == DRM_FORMAT_ABGR2101010 ||
	    pixel_format == DRM_FORMAT_BGRA1010102)
		osd_endian_mode(vblk, reg_ops, reg, afbc_en);
	else
		osd_endian_mode(vblk, reg_ops, reg, !afbc_en);

	osd_mem_mode(vblk, reg_ops, reg, 1);
}

static void osd_set_dimm_ctrl(struct meson_vpu_block *vblk,
			       struct rdma_reg_ops *reg_ops,
			       struct osd_mif_reg_s *reg,
			       u32 val)
{
	u32 dimm_rgb = 0;

	if (val) { /* set dimm */
		dimm_rgb = (val & 0x3ff00000);  /* get [29:20] bit for red channel */
		dimm_rgb |= (val & 0x000ffc00); /* get [19:10] bit for green channel */
		dimm_rgb |= (val & 0x000003ff); /* get [9:0] bit for blue channel */

		reg_ops->rdma_write_reg(reg->viu_osd_dimm_ctrl, 0x40000000 | dimm_rgb);

		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0x1, 14, 1);
		reg_ops->rdma_write_reg_bits(reg->viu_osd_ctrl_stat2, 0xff, 6, 8);
	} else { /* clear dimm */
		reg_ops->rdma_write_reg(reg->viu_osd_dimm_ctrl, val);
	}
}

/* set osd, video two port */
void osd_set_two_ports(u32 set, struct rdma_reg_ops *reg_ops)
{
	static u32 data32[2];

	if (cpu_after_eq(MESON_CPU_MAJOR_ID_T7)) {
		if (is_meson_t3_cpu()) {
			/* set osd, video two port */
			if (set == 1) {
				/*mali afbcd,dolby0, osd1-4 etc->VPU0*/
				/*aisr reshape, vd1, vd2, tcon p1 read->VPU2*/
				reg_ops->rdma_write_reg(VPP_RDARB_MODE, 0x10c00000);
				reg_ops->rdma_write_reg(VPU_RDARB_MODE_L2C1, 0x920000);
			} else if (set == 0) {
				reg_ops->rdma_write_reg(VPP_RDARB_MODE, 0xaa0000);
				reg_ops->rdma_write_reg(VPU_RDARB_MODE_L2C1, 0x900000);
			}
		}
		return;
	}

	/* set osd, video two port */
	if (!data32[0] && !data32[1]) {
		data32[0] = reg_ops->rdma_read_reg(VPP_RDARB_MODE);
		data32[1] = reg_ops->rdma_read_reg(VPU_RDARB_MODE_L2C1);
	}

	if (set == 1) {
		reg_ops->rdma_write_reg_bits(VPP_RDARB_MODE, 2, 20, 8);
		reg_ops->rdma_write_reg_bits(VPU_RDARB_MODE_L2C1, 2, 16, 8);
	} else if (set == 0) {
		reg_ops->rdma_write_reg(VPP_RDARB_MODE, data32[0]);
		reg_ops->rdma_write_reg(VPU_RDARB_MODE_L2C1, data32[1]);
	}
}

static void osd_scan_mode_config(struct meson_vpu_block *vblk,
				 struct rdma_reg_ops *reg_ops,
				 struct osd_mif_reg_s *reg, int scan_mode)
{
	if (scan_mode)
		reg_ops->rdma_write_reg_bits(reg->viu_osd_blk0_cfg_w0, 0, 1, 1);
}

static void meson_drm_osd_canvas_alloc(unsigned int *canvas_array, int size)
{
	if (canvas_pool_alloc_canvas_table("osd_drm",
					   canvas_array, size,
					   CANVAS_MAP_TYPE_1)) {
		DRM_INFO("allocate drm osd canvas error.\n");
	}
}

static void meson_drm_osd_canvas_free(u8 index)
{
	canvas_pool_free_canvas_table(osd_canvas[index], MAX_CANVAS_NUM);
}

static void osd_linear_addr_config(struct meson_vpu_block *vblk,
				   struct rdma_reg_ops *reg_ops,
				   struct osd_mif_reg_s *reg, u64 phy_addr,
				   u32 byte_stride)
{
	reg_ops->rdma_write_reg(reg->viu_osd_blk1_cfg_w4, phy_addr >> 4);
	reg_ops->rdma_write_reg_bits(reg->viu_osd_blk2_cfg_w4,
				byte_stride, 0, 12);
}

static bool osd_check_config(struct meson_vpu_osd_state *mvos,
	struct meson_vpu_osd_state *old_mvos)
{
	if (!old_mvos || old_mvos->src_x != mvos->src_x ||
		old_mvos->src_y != mvos->src_y ||
		old_mvos->src_w != mvos->src_w ||
		old_mvos->src_h != mvos->src_h ||
		old_mvos->phy_addr != mvos->phy_addr ||
		old_mvos->pixel_format != mvos->pixel_format ||
		old_mvos->global_alpha != mvos->global_alpha ||
		old_mvos->crtc_index != mvos->crtc_index ||
		old_mvos->rotation != mvos->rotation ||
		old_mvos->afbc_en != mvos->afbc_en) {
		return true;
	} else {
		return false;
	}
}

static int osd_check_state(struct meson_vpu_block *vblk,
			   struct meson_vpu_block_state *state,
		struct meson_vpu_sub_pipeline_state *mvps)
{
	struct meson_vpu_osd_layer_info *plane_info;
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	struct meson_vpu_osd_state *mvos = to_osd_state(state);

	if (state->checked)
		return 0;

	state->checked = true;

	if (!mvos || mvos->plane_index >= MESON_MAX_OSDS) {
		DRM_INFO("mvos is NULL!\n");
		return -1;
	}
	MESON_DRM_BLOCK("%s - %d check_state called.\n", osd->base.name, vblk->index);
	plane_info = &mvps->plane_info[vblk->index];
	mvos->src_x = plane_info->src_x;
	mvos->src_y = plane_info->src_y;
	mvos->src_w = plane_info->src_w;
	mvos->src_h = plane_info->src_h;
	mvos->dst_x = plane_info->dst_x;
	mvos->dst_y = plane_info->dst_y;
	mvos->dst_w = plane_info->dst_w;
	mvos->dst_h = plane_info->dst_h;
	mvos->fb_w = plane_info->fb_w;
	mvos->fb_h = plane_info->fb_h;
	mvos->byte_stride = plane_info->byte_stride;
	mvos->phy_addr = plane_info->phy_addr;
	mvos->pixel_format = plane_info->pixel_format;
	mvos->fb_size = plane_info->fb_size;
	mvos->pixel_blend = plane_info->pixel_blend;
	mvos->rotation = plane_info->rotation;
	mvos->afbc_en = plane_info->afbc_en;
	mvos->blend_bypass = plane_info->blend_bypass;
	mvos->plane_index = plane_info->plane_index;
	mvos->global_alpha = plane_info->global_alpha;
	mvos->crtc_index = plane_info->crtc_index;
	mvos->read_ports = plane_info->read_ports;
	mvos->sec_en = plane_info->sec_en;
	mvos->palette_arry = plane_info->palette_arry;
	mvos->process_unit = plane_info->process_unit;
	mvos->osd_dimm.dimm_value = plane_info->osd_dimm.dimm_value;
	mvos->osd_dimm.dimm_ctrl = plane_info->osd_dimm.dimm_ctrl;

	return 0;
}

/* the return stride unit is 128bit(16bytes) */
static u32 line_stride_calc(u32 drm_format,
		u32 hsize,
		u32 stride_align_32bytes)
{
	u32 line_stride = 0;
	u32 line_stride_32bytes;
	u32 line_stride_64bytes;

	switch (drm_format) {
	/* 8-bit */
	case DRM_FORMAT_R8:
	case DRM_FORMAT_C8:
	case DRM_FORMAT_RGB332:
	case DRM_FORMAT_BGR233:
		line_stride = ((hsize << 3) + 127) >> 7;
		break;

		/* 16-bit */
	case DRM_FORMAT_R16:
	case DRM_FORMAT_RG88:
	case DRM_FORMAT_GR88:
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_XBGR4444:
	case DRM_FORMAT_RGBX4444:
	case DRM_FORMAT_BGRX4444:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_RGBA4444:
	case DRM_FORMAT_BGRA4444:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
	case DRM_FORMAT_RGBX5551:
	case DRM_FORMAT_BGRX5551:
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_ABGR1555:
	case DRM_FORMAT_RGBA5551:
	case DRM_FORMAT_BGRA5551:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
		line_stride = ((hsize << 4) + 127) >> 7;
		break;

		/* 24-bit */
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
		line_stride = ((hsize << 4) + (hsize << 3) + 127) >> 7;
		break;
		/* 32-bit */
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_XRGB2101010:
	case DRM_FORMAT_XBGR2101010:
	case DRM_FORMAT_RGBX1010102:
	case DRM_FORMAT_BGRX1010102:
	case DRM_FORMAT_ARGB2101010:
	case DRM_FORMAT_ABGR2101010:
	case DRM_FORMAT_RGBA1010102:
	case DRM_FORMAT_BGRA1010102:
		line_stride = ((hsize << 5) + 127) >> 7;
		break;
	}

	line_stride_32bytes = ((line_stride + 1) >> 1) << 1;
	line_stride_64bytes = ((line_stride + 3) >> 2) << 2;

	/* need wr ddr is 32bytes aligned */
	if (stride_align_32bytes)
		line_stride = line_stride_32bytes;
	else
		line_stride = line_stride_64bytes;

	return line_stride;
}

static void osd_set_palette(struct osd_mif_reg_s *reg, struct rdma_reg_ops *reg_ops, u32 *palette)
{
	int regno;

	for (regno = 0; regno < 256; regno++) {
		reg_ops->rdma_write_reg(reg->viu_osd_color_addr, regno);
		reg_ops->rdma_write_reg(reg->viu_osd_color, palette[regno]);
	}
}

static int osd_format_is_rgbx(u32 pixel_format)
{
	int is_rgbx = 0;

	switch (pixel_format) {
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGBX8888:
	case DRM_FORMAT_BGRX8888:
		is_rgbx = 1;
		break;
	default:
		break;
	}

	return is_rgbx;
}

static void osd_set_state(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state,
			  struct meson_vpu_block_state *old_state,
			  struct meson_vpu_sub_pipeline_state *mvsps)
{
	struct meson_vpu_osd *osd;
	struct meson_vpu_osd_state *mvos, *old_mvos = NULL;
	struct meson_vpu_pipeline *pipe;
	struct meson_vpu_state_check *status;
	struct meson_drm *priv;
	struct rdma_reg_ops *reg_ops;
	int crtc_index;
	u32 pixel_format, canvas_idx = 0, src_h, byte_stride, flush_reg, hold_line;
	struct osd_scope_s scope_src = {0, 1919, 0, 1079};
	struct osd_mif_reg_s *reg;
	bool alpha_div_en = 0, reverse_x, reverse_y, afbc_en;
	enum osd_process_unit process_unit;
	u64 phy_addr;
	u16 global_alpha = 256; /*range 0~256*/

	if (!vblk || !state) {
		MESON_DRM_BLOCK("set_state break for NULL.\n");
		return;
	}

	mvos = to_osd_state(state);
	osd = to_osd_block(vblk);
	crtc_index = mvos->crtc_index;
	reg_ops = state->sub->reg_ops;
	pipe = vblk->pipeline;
	priv = pipe->priv;
	reg = osd->reg;
	if (!reg) {
		MESON_DRM_BLOCK("set_state break for NULL OSD mixer reg.\n");
		return;
	}

	if (old_state)
		old_mvos = to_osd_state(old_state);

	if (vblk->ops->init_register)
		vblk->ops->init_register(vblk, state);

	if (priv->vpu_data && priv->vpu_data->has_gfcd && crtc_index == VPP0) {
		process_unit = mvos->process_unit;
		if (process_unit == GFCD_AFBC || process_unit == GFCD_AFRC) {
			MESON_DRM_BLOCK("set gfcd %d.\n", process_unit);
			osd_gfcd_axi_input_en(vblk, reg_ops, reg, vblk->index, 1);
			osd_gfcd_dout_en(vblk, reg_ops, reg, vblk->index, 1);
			if (osd->mali_src_en_switch)
				osd_mali_src_en(vblk, reg_ops, reg, vblk->index, 1);
			return;
		}

		MESON_DRM_BLOCK("set mif or afbc.\n");
		osd_gfcd_axi_input_en(vblk, reg_ops, reg, vblk->index, 0);
		osd_gfcd_dout_en(vblk, reg_ops, reg, vblk->index, 0);
	}

	if (mvos->crtc_index == 1)
		/*for multi-vpp, the vpp1 default is 4*/
		hold_line = osd->viu2_hold_line;
	else
		hold_line = am_drm_param.osd_hold_line;

	afbc_en = mvos->afbc_en ? 1 : 0;
	flush_reg = osd_check_config(mvos, old_mvos);
	MESON_DRM_BLOCK("flush_reg-%d index-%d\n", flush_reg, vblk->index);
	status = &pipe->subs[crtc_index]->status[vblk->index];
	status->phy_addr = mvos->phy_addr;
	status->viu_osd_blk1_cfg_w4 = reg->viu_osd_blk1_cfg_w4;
	status->afbc_en = afbc_en;
	status->index = vblk->index;
	status->mif_acc_mode = osd->mif_acc_mode;
	status->update = 1;
	MESON_DRM_STATE("osd%d (\"%s\" pid=%d]) state:0x%p\n"
		"mvps:0x%p phy:0x%llx src[%d %d %d %d] dst[%d %d %d %d]\n",
		vblk->index, current->comm, task_pid_nr(current),
		old_state ? old_state->obj.state : state->obj.state, mvsps, mvos->phy_addr,
		mvos->src_x, mvos->src_y, mvos->src_w, mvos->src_h,
		mvos->dst_x, mvos->dst_y, mvos->dst_w, mvos->dst_h);
	if (!flush_reg &&
		meson_drm_read_reg(reg->viu_osd_tcolor_ag3) == frame_seq[vblk->index] &&
		osd->mif_acc_mode != CANVAS_MODE) {
		/*after v7 chips, always linear addr*/
		if (osd->mif_acc_mode == LINEAR_MIF)
			osd_mem_mode(vblk, reg_ops, reg, 1);

		ods_hold_line_config(vblk, reg_ops, reg, hold_line);

		MESON_DRM_BLOCK("%s-%d not need to flush mif register.\n",
			osd->base.name, vblk->index);
		return;
	}

	pixel_format = mvos->pixel_format;
	if (mvos->pixel_blend == DRM_MODE_BLEND_PREMULTI &&
		!osd_format_is_rgbx(pixel_format))
		alpha_div_en = 1;

	/*drm alaph 16bit, amlogic alpha 8bit*/
	global_alpha = mvos->global_alpha >> 8;
	if (global_alpha == 0xff)
		global_alpha = 0x100;

	src_h = mvos->src_h + mvos->src_y;
	byte_stride = mvos->byte_stride;
	if (osd->mif_acc_mode == LINEAR_MIF)
		byte_stride = line_stride_calc(mvos->pixel_format,
						mvos->fb_w, 0);

	phy_addr = mvos->phy_addr;
	scope_src.h_start = mvos->src_x;
	scope_src.h_end = mvos->src_x + mvos->src_w - 1;
	scope_src.v_start = mvos->src_y;
	scope_src.v_end = mvos->src_y + mvos->src_h - 1;
	if (mvsps->more_60) {
		if (vblk->index == OSD1_SLICE0)
			scope_src.h_end = mvos->src_x +
				mvsps->scaler_param[vblk->index].input_width - 1;
		if (vblk->index == OSD3_SLICE1)
			scope_src.h_start = scope_src.h_end -
				mvsps->scaler_param[vblk->index].input_width + 1;
	}

	reverse_x = (mvos->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
	reverse_y = (mvos->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;
	osd_reverse_x_enable(vblk, reg_ops, reg, reverse_x);
	osd_reverse_y_enable(vblk, reg_ops, reg, reverse_y);
	if (osd->mif_acc_mode == LINEAR_MIF) {
		osd_linear_addr_config(vblk, reg_ops, reg, phy_addr, byte_stride);
		MESON_DRM_BLOCK("byte stride=0x%x,phy_addr=0x%pa\n",
			  byte_stride, &phy_addr);
	} else {
		canvas_idx = osd_plane_update_canvas(osd, reg, flush_reg);
		canvas_config(canvas_idx, phy_addr, byte_stride, src_h,
				CANVAS_ADDR_NOWRAP, CANVAS_BLKMODE_LINEAR);
		osd_canvas_config(vblk, reg_ops, reg, canvas_idx);
		MESON_DRM_BLOCK("canvas_idx=0x%x,phy_addr=0x%pa\n",
			  canvas_idx, &phy_addr);
	}

	status->canvas_index = canvas_idx;
	osd_rpt_y_config(vblk, reg_ops, reg);
	osd_input_size_config(vblk, reg_ops, reg, scope_src);
	osd_color_config(vblk, reg_ops, reg, pixel_format, mvos->pixel_blend, afbc_en);

	if (osd->mif_acc_mode == LINEAR_MIF) {
		osd_afbc_config_linear(vblk, reg_ops, reg, vblk->index, pixel_format, afbc_en);
		if (osd->mali_src_en_switch)
			osd_mali_src_en(vblk, reg_ops, reg, vblk->index, afbc_en);
		else
			osd_mali_src_en_linear(vblk, reg_ops, reg, vblk->index, afbc_en);
	} else {
		osd_afbc_config(vblk, reg_ops, reg, vblk->index, afbc_en);
	}

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	if (mvos->sec_en)
		mvsps->sec_src |= osd_secure_input_index[vblk->index];
#endif

	osd_premult_enable(vblk, reg_ops, reg, alpha_div_en);
	if (!osd->gfcd_global_alpha_policy)
		osd_global_alpha_set(vblk, reg_ops, reg, global_alpha);

	if (mvos->osd_dimm.dimm_ctrl)
		osd_set_dimm_ctrl(vblk, reg_ops, reg, mvos->osd_dimm.dimm_value);

	ods_hold_line_config(vblk, reg_ops, reg, hold_line);
	osd_set_two_ports(mvos->read_ports, reg_ops);
	if (mvos->palette_arry)
		osd_set_palette(reg, reg_ops, mvos->palette_arry);

	frame_seq[vblk->index]++;
	reg_ops->rdma_write_reg(reg->viu_osd_tcolor_ag3, frame_seq[vblk->index]);
	MESON_DRM_BLOCK("plane_index=%d,HW-OSD=%d,dimm=[0x%x,%d]\n",
		  mvos->plane_index, vblk->index,
		  mvos->osd_dimm.dimm_value, mvos->osd_dimm.dimm_ctrl);
	MESON_DRM_BLOCK("scope h/v start/end:[%d/%d/%d/%d]\n",
		  scope_src.h_start, scope_src.h_end,
		scope_src.v_start, scope_src.v_end);
	MESON_DRM_BLOCK("%s set_state done.\n", osd->base.name);
}

/*
 * compare the below soc's osd mif, a9 has below changelist
 * 1 remove repeate_y, interlace config
 * 2 OSD_PATH_MISC_CTRL was not used anymore
 * 3 mali afbc related register was not used anymore
 */

static void a9_osd_set_state(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state,
			  struct meson_vpu_block_state *old_state,
			  struct meson_vpu_sub_pipeline_state *mvsps)
{
	struct meson_vpu_osd *osd;
	struct meson_vpu_osd_state *mvos, *old_mvos = NULL;
	struct meson_vpu_pipeline *pipe;
	struct meson_vpu_state_check *status;
	struct meson_drm *priv;
	struct rdma_reg_ops *reg_ops;
	int crtc_index;
	u32 pixel_format, src_h, byte_stride, flush_reg, hold_line;
	struct osd_scope_s scope_src = {0, 1919, 0, 1079};
	struct osd_mif_reg_s *reg;
	bool alpha_div_en = 0, reverse_x, reverse_y, afbc_en;
	u64 phy_addr;
	enum osd_process_unit process_unit;
	u16 global_alpha = 256; /*range 0~256*/

	if (!vblk || !state) {
		MESON_DRM_BLOCK("set_state break for NULL.\n");
		return;
	}

	mvos = to_osd_state(state);
	osd = to_osd_block(vblk);
	crtc_index = mvos->crtc_index;
	reg_ops = state->sub->reg_ops;
	pipe = vblk->pipeline;
	priv = pipe->priv;
	reg = osd->reg;
	if (!reg) {
		MESON_DRM_BLOCK("set_state break for NULL OSD mixer reg.\n");
		return;
	}

	if (old_state)
		old_mvos = to_osd_state(old_state);

	if (vblk->ops->init_register)
		vblk->ops->init_register(vblk, state);

	if (priv->vpu_data && priv->vpu_data->has_gfcd) {
		process_unit = mvos->process_unit;
		if (process_unit == GFCD_AFBC || process_unit == GFCD_AFRC) {
			MESON_DRM_BLOCK("set gfcd %d.\n", process_unit);
			a9_osd_in_out_cfg(vblk, reg_ops, reg, vblk->index, 1);
			return;
		}

		MESON_DRM_BLOCK("set mif or afbc.\n");
		/* use mif*/
		a9_osd_in_out_cfg(vblk, reg_ops, reg, vblk->index, 0);
	}

	if (mvos->crtc_index == 1)
		/*for multi-vpp, the vpp1 default is 4*/
		hold_line = osd->viu2_hold_line;
	else
		hold_line = am_drm_param.osd_hold_line;

	afbc_en = mvos->afbc_en ? 1 : 0;
	flush_reg = osd_check_config(mvos, old_mvos);
	MESON_DRM_BLOCK("flush_reg-%d index-%d\n", flush_reg, vblk->index);
	status = &pipe->subs[crtc_index]->status[vblk->index];
	status->phy_addr = mvos->phy_addr;
	status->viu_osd_blk1_cfg_w4 = reg->viu_osd_blk1_cfg_w4;
	status->afbc_en = afbc_en;
	status->index = vblk->index;
	status->mif_acc_mode = osd->mif_acc_mode;
	status->update = 1;
	MESON_DRM_STATE("osd%d (\"%s\" pid=%d]) state:0x%p\n"
		"mvps:0x%p phy:0x%llx src[%d %d %d %d] dst[%d %d %d %d]\n",
		vblk->index, current->comm, task_pid_nr(current),
		old_state ? old_state->obj.state : state->obj.state, mvsps, mvos->phy_addr,
		mvos->src_x, mvos->src_y, mvos->src_w, mvos->src_h,
		mvos->dst_x, mvos->dst_y, mvos->dst_w, mvos->dst_h);
	if (!flush_reg &&
		meson_drm_read_reg(reg->viu_osd_tcolor_ag3) == frame_seq[vblk->index] &&
		osd->mif_acc_mode != CANVAS_MODE) {
		/*after v7 chips, always linear addr*/
		if (osd->mif_acc_mode == LINEAR_MIF)
			osd_mem_mode(vblk, reg_ops, reg, 1);

		ods_hold_line_config(vblk, reg_ops, reg, hold_line);

		MESON_DRM_BLOCK("%s-%d not need to flush mif register.\n",
			osd->base.name, vblk->index);
		return;
	}

	pixel_format = mvos->pixel_format;
	if (mvos->pixel_blend == DRM_MODE_BLEND_PREMULTI &&
		!osd_format_is_rgbx(pixel_format))
		alpha_div_en = 1;

	/*drm alaph 16bit, amlogic alpha 8bit*/
	global_alpha = mvos->global_alpha >> 8;
	if (global_alpha == 0xff)
		global_alpha = 0x100;

	src_h = mvos->src_h + mvos->src_y;
	byte_stride = line_stride_calc(mvos->pixel_format,
						mvos->fb_w, 0);

	phy_addr = mvos->phy_addr;
	scope_src.h_start = mvos->src_x;
	scope_src.h_end = mvos->src_x + mvos->src_w - 1;
	scope_src.v_start = mvos->src_y;
	scope_src.v_end = mvos->src_y + mvos->src_h - 1;

	reverse_x = (mvos->rotation & DRM_MODE_REFLECT_X) ? 1 : 0;
	reverse_y = (mvos->rotation & DRM_MODE_REFLECT_Y) ? 1 : 0;
	osd_reverse_x_enable(vblk, reg_ops, reg, reverse_x);
	osd_reverse_y_enable(vblk, reg_ops, reg, reverse_y);
	osd_linear_addr_config(vblk, reg_ops, reg, phy_addr, byte_stride);
	MESON_DRM_BLOCK("byte stride=0x%x,phy_addr=0x%pa\n",
			  byte_stride, &phy_addr);

	osd_input_size_config(vblk, reg_ops, reg, scope_src);
	osd_color_config(vblk, reg_ops, reg, pixel_format, mvos->pixel_blend, afbc_en);
	osd_afbc_config_linear(vblk, reg_ops, reg, vblk->index, pixel_format, afbc_en);

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	if (mvos->sec_en)
		mvsps->sec_src |= osd_secure_input_index[vblk->index];
#endif

	osd_premult_enable(vblk, reg_ops, reg, alpha_div_en);
	if (!osd->gfcd_global_alpha_policy)
		osd_global_alpha_set(vblk, reg_ops, reg, global_alpha);

	osd_set_dimm_ctrl(vblk, reg_ops, reg, 0);
	ods_hold_line_config(vblk, reg_ops, reg, hold_line);
	osd_set_two_ports(mvos->read_ports, reg_ops);
	if (mvos->palette_arry)
		osd_set_palette(reg, reg_ops, mvos->palette_arry);

	frame_seq[vblk->index]++;
	reg_ops->rdma_write_reg(reg->viu_osd_tcolor_ag3, frame_seq[vblk->index]);
	MESON_DRM_BLOCK("plane_index=%d,HW-OSD=%d\n",
		  mvos->plane_index, vblk->index);
	MESON_DRM_BLOCK("scope h/v start/end:[%d/%d/%d/%d]\n",
		  scope_src.h_start, scope_src.h_end,
		scope_src.v_start, scope_src.v_end);
	MESON_DRM_BLOCK("%s set_state done.\n", osd->base.name);
}

static void osd_hw_enable(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	struct osd_mif_reg_s *reg = osd->reg;

	if (!vblk) {
		MESON_DRM_BLOCK("enable break for NULL.\n");
		return;
	}
	osd_block_enable(vblk, state->sub->reg_ops, reg, 1);
	MESON_DRM_BLOCK("%s enable done.\n", osd->base.name);
}

static void a9_osd_hw_enable(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	struct osd_mif_reg_s *reg = osd->reg;

	if (!vblk) {
		MESON_DRM_BLOCK("enable break for NULL.\n");
		return;
	}
	osd_block_enable(vblk, state->sub->reg_ops, reg, 1);
	osd_block_mif_enable(vblk, state->sub->reg_ops, reg, 1);
	MESON_DRM_BLOCK("%s enable done.\n", osd->base.name);
}

static void osd_hw_disable(struct meson_vpu_block *vblk,
			   struct meson_vpu_sub_pipeline *sub_pipeline)
{
	struct meson_vpu_osd *osd;
	struct osd_mif_reg_s *reg;
	u8 version;
	enum drm_pm_state pm_state;
	struct meson_vpu_sub_pipeline_state *mvsps =
		priv_to_sub_pipeline_state(sub_pipeline->obj.state);

	if (!vblk) {
		MESON_DRM_BLOCK("disable break for NULL.\n");
		return;
	}

	osd = to_osd_block(vblk);
	reg = osd->reg;
	version = vblk->pipeline->osd_version;
	pm_state = mvsps->pm_state;

	/*G12B should always enable,avoid afbc decoder error*/
	if ((version != OSD_V2 && version != OSD_V3) || pm_state == DRM_PM_SUSPEND)
		osd_block_enable(vblk, sub_pipeline->reg_ops, reg, 0);
	MESON_DRM_BLOCK("%s disable done.\n", osd->base.name);
}

static void a9_osd_hw_disable(struct meson_vpu_block *vblk,
			   struct meson_vpu_sub_pipeline *sub_pipeline)
{
	struct meson_vpu_osd *osd;
	struct osd_mif_reg_s *reg;
	u8 version;
	enum drm_pm_state pm_state;
	struct meson_vpu_sub_pipeline_state *mvsps =
		priv_to_sub_pipeline_state(sub_pipeline->obj.state);

	if (!vblk) {
		MESON_DRM_BLOCK("disable break for NULL.\n");
		return;
	}

	osd = to_osd_block(vblk);
	reg = osd->reg;
	version = vblk->pipeline->osd_version;
	pm_state = mvsps->pm_state;

	/*G12B should always enable,avoid afbc decoder error*/
	if ((version != OSD_V2 && version != OSD_V3) || pm_state == DRM_PM_SUSPEND)
		osd_block_enable(vblk, sub_pipeline->reg_ops, reg, 0);
	osd_block_mif_enable(vblk, sub_pipeline->reg_ops, reg, 0);
	MESON_DRM_BLOCK("%s disable done.\n", osd->base.name);
}

static void osd_dump_register(struct drm_printer *p, struct meson_vpu_block *vblk)
{
	int osd_index;
	u32 value, reg_addr;
	char buff[8];
	struct meson_vpu_osd *osd;
	struct osd_mif_reg_s *reg;

	osd_index = vblk->index;
	osd = to_osd_block(vblk);
	reg = osd->reg;

	snprintf(buff, 8, "OSD%d", osd_index + 1);

	reg_addr = reg->viu_osd_fifo_ctrl_stat;
	value = meson_drm_read_reg(reg->viu_osd_fifo_ctrl_stat);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"FIFO_CTRL_STAT", reg_addr, value);

	reg_addr = reg->viu_osd_ctrl_stat;
	value = meson_drm_read_reg(reg->viu_osd_ctrl_stat);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"CTRL_STAT", reg_addr, value);

	reg_addr = reg->viu_osd_ctrl_stat2;
	value = meson_drm_read_reg(reg->viu_osd_ctrl_stat2);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"CTRL_STAT2", reg_addr, value);

	reg_addr = reg->viu_osd_blk0_cfg_w0;
	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w0);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n",  buff,
		"BLK0_CFG_W0", reg_addr, value);

	reg_addr = reg->viu_osd_blk0_cfg_w1;
	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w1);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"BLK0_CFG_W1", reg_addr, value);

	reg_addr = reg->viu_osd_blk0_cfg_w2;
	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w2);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"BLK0_CFG_W2", reg_addr, value);

	reg_addr = reg->viu_osd_blk0_cfg_w3;
	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w3);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"BLK0_CFG_W3", reg_addr, value);

	reg_addr = reg->viu_osd_blk0_cfg_w4;
	value = meson_drm_read_reg(reg->viu_osd_blk0_cfg_w4);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"BLK0_CFG_W4", reg_addr, value);

	reg_addr = reg->viu_osd_blk1_cfg_w4;
	value = meson_drm_read_reg(reg->viu_osd_blk1_cfg_w4);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"BLK1_CFG_W4", reg_addr, value);

	reg_addr = reg->viu_osd_blk2_cfg_w4;
	value = meson_drm_read_reg(reg->viu_osd_blk2_cfg_w4);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"BLK2_CFG_W4", reg_addr, value);

	reg_addr = reg->viu_osd_prot_ctrl;
	value = meson_drm_read_reg(reg->viu_osd_prot_ctrl);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"PROT_CTRL", reg_addr, value);

	reg_addr = reg->viu_osd_mali_unpack_ctrl;
	value = meson_drm_read_reg(reg->viu_osd_mali_unpack_ctrl);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"MALI_UNPACK_CTRL", reg_addr, value);

	reg_addr = reg->viu_osd_dimm_ctrl;
	value = meson_drm_read_reg(reg->viu_osd_dimm_ctrl);
	drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
		"DIMM_CTRL", reg_addr, value);

	if (osd->no_afbc_ctrl) {
		reg_addr = reg->viu_osd_normal_swap;
		value = meson_drm_read_reg(reg->viu_osd_normal_swap);
		drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
			"NORMAL_SWAP", reg_addr, value);

		reg_addr = reg->viu_osd_mif_ctrl;
		value = meson_drm_read_reg(reg->viu_osd_mif_ctrl);
		drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
			"MIF_CTRL", reg_addr, value);

		reg_addr = reg->viu_osd_mif_ctrl1;
		value = meson_drm_read_reg(reg->viu_osd_mif_ctrl1);
		drm_printf(p, "%s_%-35s\taddr: 0x%04X\tvalue: 0x%08X\n", buff,
			"MIF_CTRL1", reg_addr, value);
	}
}

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
static void osd_secure_cb(u32 arg)
{
	// TODO
}
#endif

static void osd_register_init(struct meson_vpu_block *vblk,
	struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd;
	struct rdma_reg_ops *reg_ops;
	struct osd_mif_reg_s *reg;
	/*Need config follow crtc index.*/
	u8 holdline = VIU1_DEFAULT_HOLD_LINE;
	/*the fifo_size of the chips below is 512:
	 *g12a g12b sm1 sc2 s4 s1a s7 s7d s6 t6w gxlx4 t6d t6x
	 *t3x s5 t5m t5 t5w t3 t7 txhd2 tm2 tx1 txlx tl1
	 *the fifo_size of the chips below is 256, but unused:
	 *gxl gxm
	 */
	u32 fifo_size = 512;
	u32 fifo_val = fifo_size / 8;
	u32 dn_th = (fifo_size / 4) >> 6;
	u32 up_th = (fifo_size * 3 / 4) >> 6;

	if (vblk->init_done)
		return;

	reg_ops = state->sub->reg_ops;
	osd = to_osd_block(vblk);
	reg = osd->reg;
	if (!reg) {
		MESON_DRM_BLOCK("set_state break for NULL OSD mixer reg.\n");
		return;
	}

	/*
	 * force turning off deprecated mif interlace config
	 */
	osd_scan_mode_config(vblk, reg_ops, reg, 1);

	reg_ops->rdma_write_reg(reg->viu_osd_fifo_ctrl_stat,
			    (1 << 31) | /*BURST_LEN_SEL[2]*/
			    (0 << 30) | /*no swap*/
			    (0 << 29) | /*div swap*/
			    (2 << 24) | /*Fifo_lim 5bits*/
			    (2 << 22) | /*Fifo_ctrl 2bits*/
			    (fifo_val << 12) | /*FIFO_DEPATH_VAL 7bits*/
			    (1 << 10) | /*BURST_LEN_SEL[1:0]*/
			    (holdline << 5) | /*hold fifo lines 5bits*/
			    (0 << 4) | /*CLEAR_ERR*/
			    (0 << 3) | /*fifo_sync_rst*/
			    (0 << 1) | /*ENDIAN:no conversion*/
			    (1 << 0)/*urgent enable*/);
	reg_ops->rdma_write_reg(reg->viu_osd_ctrl_stat,
			    (0 << 31) | /*osd_cfg_sync_en*/
			    (0 << 30) | /*Enable free_clk*/
			    (0x100 << 12) | /*global alpha*/
			    (0 << 11) | /*TEST_RD_EN*/
			    (0 << 2) | /*osd_mem_mode 0:canvas_addr*/
			    (0 << 1) | /*premult_en*/
			    (0 << 0)/*OSD_BLK_ENABLE*/);
	reg_ops->rdma_write_reg(reg->viu_osd_tcolor_ag3, 0);
	reg_ops->rdma_write_reg(reg->viu_osd_prot_ctrl,
		(fifo_size << 0) | /*13bits*/
		(1 << 31) | /*auto_urgent_en*/
		(0 << 30) | /*urgent_wr*/
		(dn_th << 16) | /*4bits*/
		(up_th << 20)/*4bits*/);
	reg_ops->rdma_write_reg(reg->viu_osd_dimm_ctrl, 0);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/*The fifo_crtl bits need to be configured with a maximum value of 0x2, otherwise it
	 *will affect bandwidth. The original values should to be obtained after mif init
	 */
	if (is_meson_t3x_cpu()) {
		original_swap_t3x[vblk->index] = reg_ops->rdma_read_reg(reg->viu_osd_normal_swap);
		original_osd1_fifo_ctrl_stat_t3x[vblk->index] =
			reg_ops->rdma_read_reg(reg->viu_osd_fifo_ctrl_stat);
	}
#endif

	vblk->init_done = 1;
	MESON_DRM_BLOCK("register_init vblk:%d.\n", vblk->index);
}

static void a9_osd_register_init(struct meson_vpu_block *vblk,
	struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd;
	struct rdma_reg_ops *reg_ops;
	struct osd_mif_reg_s *reg;
	/*Need config follow crtc index.*/
	u8 holdline = VIU1_DEFAULT_HOLD_LINE;
	//u8 fifo_val = 0x20;

	if (vblk->init_done)
		return;

	reg_ops = state->sub->reg_ops;
	osd = to_osd_block(vblk);
	reg = osd->reg;
	if (!reg) {
		MESON_DRM_BLOCK("set_state break for NULL OSD mixer reg.\n");
		return;
	}

	reg_ops->rdma_write_reg(reg->viu_osd_fifo_ctrl_stat,
			    (0 << 30) | /*no swap*/
			    (0 << 29) | /*div swap*/
			    (2 << 22) | /*Fifo_ctrl 2bits*/
			    (holdline << 5) | /*hold fifo lines 5bits*/
			    (0 << 3) | /*fifo_sync_rst*/
			    (0 << 1) | /*ENDIAN:no conversion*/
			    (1 << 0)/*urgent enable*/);
	reg_ops->rdma_write_reg(reg->viu_osd_ctrl_stat,
			    (0 << 31) | /*osd_cfg_sync_en*/
			    (0 << 30) | /*Enable free_clk*/
			    (0x100 << 12) | /*global alpha*/
			    (0 << 11) | /*TEST_RD_EN*/
			    (0 << 2) | /*osd_mem_mode 0:canvas_addr*/
			    (0 << 1) | /*premult_en*/
			    (0 << 0)/*OSD_BLK_ENABLE*/);
	reg_ops->rdma_write_reg(reg->viu_osd_tcolor_ag3, 0);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
	/*The fifo_crtl bits need to be configured with a maximum value of 0x2, otherwise it
	 *will affect bandwidth. The original values should to be obtained after mif init
	 */
	if (is_meson_t3x_cpu()) {
		original_swap_t3x[vblk->index] = reg_ops->rdma_read_reg(reg->viu_osd_normal_swap);
		original_osd1_fifo_ctrl_stat_t3x[vblk->index] =
			reg_ops->rdma_read_reg(reg->viu_osd_fifo_ctrl_stat);
	}
#endif

	vblk->init_done = 1;
	MESON_DRM_BLOCK("register_init vblk:%d.\n", vblk->index);
}

static void c5_osd_register_init(struct meson_vpu_block *vblk,
	struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd;
	struct rdma_reg_ops *reg_ops;
	struct osd_mif_reg_s *reg;
	/*Need config follow crtc index.*/
	u8 holdline = VIU1_DEFAULT_HOLD_LINE;
	/*the fifo_size of the chips below is 512:
	 *g12a g12b sm1 sc2 s4 s1a s7 s7d s6 t6w gxlx4 t6d t6x
	 *t3x s5 t5m t5 t5w t3 t7 txhd2 tm2 tx1 txlx tl1
	 *the fifo_size of the chips below is 256, but unused:
	 *gxl gxm
	 */
	u32 fifo_size = 512;
	u32 fifo_val = fifo_size / 8;
	u32 dn_th = (fifo_size / 4) >> 6;
	u32 up_th = (fifo_size * 3 / 4) >> 6;

	if (vblk->init_done)
		return;

	reg_ops = state->sub->reg_ops;
	osd = to_osd_block(vblk);
	reg = osd->reg;
	if (!reg) {
		MESON_DRM_BLOCK("set_state break for NULL OSD mixer reg.\n");
		return;
	}

	/*
	 * force turning off deprecated mif interlace config
	 */
	osd_scan_mode_config(vblk, reg_ops, reg, 1);

	reg_ops->rdma_write_reg(reg->viu_osd_fifo_ctrl_stat,
				(1 << 31) | /*BURST_LEN_SEL[2]*/
				(0 << 30) | /*no swap*/
				(0 << 29) | /*div swap*/
				(2 << 24) | /*Fifo_lim 5bits*/
				(2 << 22) | /*Fifo_ctrl 2bits*/
				(fifo_val << 12) | /*FIFO_DEPATH_VAL 7bits*/
				(1 << 10) | /*BURST_LEN_SEL[1:0]*/
				(holdline << 5) | /*hold fifo lines 5bits*/
				(0 << 4) | /*CLEAR_ERR*/
				(0 << 3) | /*fifo_sync_rst*/
				(0 << 1) | /*ENDIAN:no conversion*/
				(1 << 0)/*urgent enable*/);
	reg_ops->rdma_write_reg(reg->viu_osd_ctrl_stat,
				(1 << 31) | /*osd_cfg_sync_en*/
				(0 << 30) | /*Enable free_clk*/
				(0x100 << 12) | /*global alpha*/
				(0 << 11) | /*TEST_RD_EN*/
				(0 << 2) | /*osd_mem_mode 0:canvas_addr*/
				(0 << 1) | /*premult_en*/
				(0 << 0)/*OSD_BLK_ENABLE*/);
	reg_ops->rdma_write_reg(reg->viu_osd_tcolor_ag3, 0);
	reg_ops->rdma_write_reg(reg->viu_osd_prot_ctrl,
		(fifo_size << 0) | /*13bits*/
		(1 << 31) | /*auto_urgent_en*/
		(0 << 30) | /*urgent_wr*/
		(dn_th << 16) | /*4bits*/
		(up_th << 20)/*4bits*/);
	reg_ops->rdma_write_reg(reg->viu_osd_dimm_ctrl, 0);

	vblk->init_done = 1;
	MESON_DRM_BLOCK("register_init vblk:%d.\n", vblk->index);
}

static void osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	int i;

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	meson_drm_osd_canvas_alloc(osd_canvas[vblk->index], MAX_CANVAS_NUM);
	osd->reg = &osd_mif_reg[vblk->index];
	osd->mif_acc_mode = CANVAS_MODE;
	osd->viu2_hold_line = VIU2_DEFAULT_HOLD_LINE;
	osd->infos = formats_of_g12;
	INIT_KFIFO(osd->canvas_q);
	kfifo_reset(&osd->canvas_q);
	for (i = 0; i < MAX_CANVAS_NUM; i++)
		kfifo_put(&osd->canvas_q, osd_canvas[vblk->index][i]);

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
static void g12b_osd_detect_reset(struct meson_vpu_block *vblk,
			  struct meson_vpu_block_state *state)
{
	struct meson_vpu_osd *osd;
	struct meson_vpu_osd_state *mvos;
	struct osd_mif_reg_s *reg;
	u32 val;
	u32 reset_bit_pos;

	if (!vblk || !state) {
		MESON_DRM_BLOCK("detect_reset break for NULL.\n");
		return;
	}

	mvos = to_osd_state(state);
	osd = to_osd_block(vblk);
	reg = osd->reg;
	if (!reg) {
		MESON_DRM_BLOCK("detect_reset break for NULL OSD mixer reg.\n");
		return;
	}

	val = meson_drm_read_reg(reg->viu_osd_fifo_ctrl_stat);
	if (((val & OSD_FIFO_ST_BITMASK) >> 20) == 2) {
		DRM_WARN("FIFO request aborting (%x), reset OSD!\n", val);
		/* crtc0 */
		if (mvos->crtc_index == 0) {
			reset_bit_pos = 14 + vblk->index;
			meson_drm_write_reg_bits(VIU_SW_RESET, 1, reset_bit_pos, 1);
			meson_drm_write_reg_bits(VIU_SW_RESET, 0, reset_bit_pos, 1);
		/* crtc1 with VIU2 OSD1 */
		} else if (mvos->crtc_index == 1) {
			meson_drm_write_reg_bits(VIU2_SW_RESET, 1, 0, 1);
			meson_drm_write_reg_bits(VIU2_SW_RESET, 0, 0, 1);
		} else {
			DRM_WARN("crtc %d is not supported!\n", mvos->crtc_index);
		}
	}

	MESON_DRM_BLOCK("%s detect_reset done.\n", osd->base.name);
}

#define OSD_GLOBAL_ALPHA_DEF 0x100
static void g12b_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	u32 data32;
	int i;

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	meson_drm_osd_canvas_alloc(osd_canvas[vblk->index], MAX_CANVAS_NUM);

	osd->reg = &g12b_osd_mif_reg[vblk->index];
	osd->mif_acc_mode = CANVAS_MODE;
	osd->viu2_hold_line = VIU2_DEFAULT_HOLD_LINE;
	osd->infos = formats_of_g12;
	INIT_KFIFO(osd->canvas_q);
	kfifo_reset(&osd->canvas_q);
	for (i = 0; i < MAX_CANVAS_NUM; i++)
		kfifo_put(&osd->canvas_q, osd_canvas[vblk->index][i]);

	if (vblk->index == MESON_VIU2_OSD1) {
		DRM_INFO("%s hw_init for %s, index:%d.\n", __func__,
				osd->base.name, vblk->index);
		/* power and clock of viu2 */
		data32 = meson_drm_read_reg(VPU_CLK_GATE);
		data32 =  data32 | 0x30000;
		meson_drm_write_reg(VPU_CLK_GATE, data32);

		meson_drm_write_reg(VPP2_OFIFO_SIZE, 0x7ff00800);
		data32 = 0x1 << 0;
		data32 |= OSD_GLOBAL_ALPHA_DEF << 12;
		meson_drm_write_reg(osd->reg->viu_osd_ctrl_stat, data32);

		meson_drm_write_reg_bits(osd->reg->viu_osd_fifo_ctrl_stat,
			1, 31, 1);
		meson_drm_write_reg_bits(osd->reg->viu_osd_fifo_ctrl_stat,
			1, 10, 2);

		data32 = 0;
		data32 = meson_drm_read_reg(osd->reg->viu_osd_ctrl_stat);
		data32 |= 0x80000000;
		meson_drm_write_reg(osd->reg->viu_osd_ctrl_stat, data32);

		data32 = 1;
		data32 |= VIU2_DEFAULT_HOLD_LINE << 5;  /* hold_fifo_lines */

		data32 |= 1 << 10;
		data32 |= 1 << 31;
		/*
		 * bit 23:22, fifo_ctrl
		 * 00 : for 1 word in 1 burst
		 * 01 : for 2 words in 1 burst
		 * 10 : for 4 words in 1 burst
		 * 11 : reserved
		 */
		data32 |= 2 << 22;
		/* bit 28:24, fifo_lim */
		data32 |= 2 << 24;
		data32 |= (64
			& 0xfffffff) << 12;
		meson_drm_write_reg(osd->reg->viu_osd_fifo_ctrl_stat, data32);
		DRM_INFO("%s hw_init end for %s, index:%d.\n", __func__,
				osd->base.name, vblk->index);
	}

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void t7_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &osd_mif_reg[vblk->index];
	osd->mif_acc_mode = LINEAR_MIF;
	osd->viu2_hold_line = VIU2_DEFAULT_HOLD_LINE;
	osd->infos = formats_of_g12;

    /* osd secure function init */
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void t6d_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &t6d_osd_mif_reg[vblk->index];
	osd->mif_acc_mode = LINEAR_MIF;
	osd->viu2_hold_line = VIU2_DEFAULT_HOLD_LINE;
	osd->infos = formats_of_t6d;
	osd->format_swap_AR30_BA30 = 1;

	/* osd secure function init */
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void s5_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &s5_osd_mif_reg[vblk->index];
	osd->mif_acc_mode = LINEAR_MIF;
	// t3x has viu2, viu1 and viu2 has same hold line
	osd->viu2_hold_line = VIU1_DEFAULT_HOLD_LINE;
	osd->infos = formats_of_g12;

#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

/*
 * base on s5, but add more formats, support format swap
 */
static void t3x_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	s5_osd_hw_init(vblk);
	osd->format_swap = 1;
	osd->infos = formats_of_t3x;
}

/*
 * base on g12, use canvas, support more formats
 */
static void txhd2_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	osd_hw_init(vblk);
	osd->infos = formats_of_t3x;
}

/*
 * base on t7, no canvs, use linear addr, support more formats
 */
static void s7_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	t7_osd_hw_init(vblk);

	osd->infos = formats_of_s1a;
}

/*
 * base on s7, no canvs, use linear addr, has gfcd
 */
static void s7d_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &osd_mif_reg[vblk->index];
	osd->mif_acc_mode = LINEAR_MIF;
	osd->mali_src_en_switch = 1;
	osd->has_gfcd = true;
	osd->gfcd_global_alpha_policy =
		!!(pipeline->priv->of_conf.gfcd_mask & BIT(GFCD_GLOBAL_ALPHA));

	osd->infos = formats_of_s1a;

    /* osd secure function init */
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void s6_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &s6_osd_mif_reg[vblk->index];
	osd->mif_acc_mode = LINEAR_MIF;
	osd->mali_src_en_switch = 1;
	if (vblk->index == MESON_OSD3)
		osd->is_viu2_osd = 1;
	osd->has_gfcd = true;
	osd->gfcd_global_alpha_policy =
		!!(pipeline->priv->of_conf.gfcd_mask & BIT(GFCD_GLOBAL_ALPHA));

	osd->infos = formats_of_s1a;

    /* osd secure function init */
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void a9_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &a9_osd_mif_reg[vblk->index];
	osd->mif_acc_mode = LINEAR_MIF;
	osd->has_gfcd = true;
	osd->no_afbc_ctrl = true;
	osd->format_swap_AR30_BA30 = 1;
	osd->viu2_hold_line = VIU2_DEFAULT_HOLD_LINE;
	osd->infos = formats_of_t6d;

    /* osd secure function init */
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}

static void c5_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_pipeline *pipeline;
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_init break for NULL.\n");
		return;
	}

	osd->reg = &c5_osd_mif_reg[vblk->index];
	osd->mif_acc_mode = LINEAR_MIF;
	osd->viu2_hold_line = VIU2_DEFAULT_HOLD_LINE;
	osd->infos = formats_of_t6d;
	osd->format_swap_AR30_BA30 = 1;

    /* osd secure function init */
#ifdef CONFIG_AMLOGIC_MEDIA_SECURITY
	secure_register(OSD_MODULE, 0, osd_secure_op, osd_secure_cb);
#endif

	MESON_DRM_BLOCK("%s hw_init done.\n", osd->base.name);
}
#endif

/*
 * base on g12, use canvas, support more formats
 */
static void s1a_osd_hw_init(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);

	osd_hw_init(vblk);
	osd->infos = formats_of_s1a;
}

static void osd_hw_fini(struct meson_vpu_block *vblk)
{
	struct meson_vpu_osd *osd = to_osd_block(vblk);
	struct meson_vpu_pipeline *pipeline;

	if (!vblk || !osd) {
		MESON_DRM_BLOCK("hw_fini break for NULL.\n");
		return;
	}

	pipeline = osd->base.pipeline;
	if (!pipeline) {
		MESON_DRM_BLOCK("hw_fini break for NULL.\n");
		return;
	}

	if (osd->mif_acc_mode == CANVAS_MODE)
		meson_drm_osd_canvas_free(vblk->index);
}

struct meson_vpu_block_ops s1a_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = s1a_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct meson_vpu_block_ops g12b_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.detect_reset = g12b_osd_detect_reset,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = g12b_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops t7_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = t7_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops t6d_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = t6d_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops s5_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = s5_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops t3x_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = t3x_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops txhd2_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = txhd2_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops s7_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = s7_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops s7d_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = s7d_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops s6_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = s6_osd_hw_init,
	.init_register = osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops a9_osd_ops = {
	.check_state = osd_check_state,
	.update_state = a9_osd_set_state,
	.enable = a9_osd_hw_enable,
	.disable = a9_osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = a9_osd_hw_init,
	.init_register = a9_osd_register_init,
	.fini = osd_hw_fini,
};

struct meson_vpu_block_ops c5_osd_ops = {
	.check_state = osd_check_state,
	.update_state = osd_set_state,
	.enable = osd_hw_enable,
	.disable = osd_hw_disable,
	.dump_register = osd_dump_register,
	.init = c5_osd_hw_init,
	.init_register = c5_osd_register_init,
	.fini = osd_hw_fini,
};
#endif
