// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/version.h>
#include <linux/string.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/amlogic/media/vout/backlight/aml_ldim.h>
#include <linux/amlogic/media/vout/backlight/ldim_fw.h>
#include "ldim_drv.h"

struct ldim_ext_hist_s ext_hist = {
	.glb_apl = 0,
	.glb_hist_cnt = 0,
	.glb_hist_sum = 0,
	.glb_hist = NULL,
};

static struct ldim_stts_s ldim_stts = {
	.global_hist = NULL,
	.seg_hist = NULL,
	.ext_hist = &ext_hist,
};

static struct ldim_fw_config_s ldim_fw_conf = {
	.seg_row = 1,
	.seg_col = 1,
	.hsize = 3840,
	.vsize = 2160,
	.func_en = 0,
	.remap_en = 0,
	.boundary_x = NULL,
	.boundary_y = NULL,
};

static struct ldim_fw_param_s ldim_fw_param = {
	.para_ver = 1,
	.para_size = sizeof(struct ldim_fw_param_s),
	.pq_header = 0,
	.pq_len = 0,
	.pq = NULL,

	.fw_sel = 0,
	.res_update = 0,
	.litgain = LD_DATA_MAX,

	.conf = &ldim_fw_conf,
	.rmem = NULL,
	.stts = &ldim_stts,
	.ext_boost = NULL,
	.pparam = NULL,
};

static struct ldim_fw_s ldim_fw = {
	/* header */
	.para_ver = FW_PARA_VER,
	.para_size = sizeof(struct ldim_fw_s),
	.alg_ver = "not installed",
	.valid = 0,
	.flag = 0,

	.fw_ctrl = FW_CTRL_LD_SEL,/*bit4:ld_sel*/
	.fw_state = 0,

	.param = &ldim_fw_param,
	.bl_matrix = NULL,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
	.fw_init = NULL,
	.fw_info_update = NULL,
	.fw_pq_set = NULL,
	.fw_profile_set = NULL,
	.fw_rmem_duty_get = NULL,
	.fw_rmem_duty_set = NULL,
	.fw_debug_show = NULL,
	.fw_debug_store = NULL,
	.fw_cmd = NULL,
};

struct ldim_cus_fw_param_s ldim_cus_fw_param = {
	.para_ver = FW_PARA_VER,
	.para_size = sizeof(struct ldim_cus_fw_param_s),

	.param_len = 0,
	.param = NULL,
	.cus = NULL,
};

static struct ldim_fw_custom_s ldim_fw_cus = {
	.valid = 0,
	.seg_col = 1,
	.seg_row = 1,
	.global_hist_bin_num = 64,

	.fw_print_frequent = 200,
	.fw_print_lv = 0,

	.fw_param = &ldim_cus_fw_param,
	.bl_matrix = NULL,

	.fw_alg_frm = NULL,
	.fw_alg_para_print = NULL,
};

struct ldim_fw_s *aml_ldim_get_fw(void)
{
	return &ldim_fw;
}
EXPORT_SYMBOL(aml_ldim_get_fw);

struct ldim_fw_custom_s *aml_ldim_get_fw_cus(void)
{
	return &ldim_fw_cus;
}
EXPORT_SYMBOL(aml_ldim_get_fw_cus);
