// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#include <linux/types.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include "ai_pq.h"
#include "../arch/vpp_regs.h"
#include "../dnlp_cal.h"
#include "../cm2_adj.h"
#include "../reg_helper.h"
#include "../amve.h"
#include "../arch/vpp_s7d_sr_regs.h"

unsigned int aipq_debug;
unsigned int aipq_smooth_dbg;
int aipq_bld_rs = 1;
int slower_coef = 1024;
unsigned int aipq_en =
	(1 << BLUE_SCENE) |
	(1 << GREEN_SCENE) |
	(1 << SKIN_TONE_SCENE) |
	(1 << PEAKING_SCENE) |
	(1 << SATURATION_SCENE) |
	(1 << DYNAMIC_CONTRAST_SCENE) |
	(1 << NOISE_SCENE);

#define pr_aipq_dbg(fmt, args...)\
	do {\
		if (aipq_debug)\
			pr_info("AIPQ: " fmt, ## args);\
	} while (0)\

#endif

#define DISABLE 0
#define ENABLE 1

struct single_scene_s detected_scenes[SCENE_MAX] = {
	{DISABLE, NULL},
	{DISABLE, NULL},
	{DISABLE, NULL},
	{DISABLE, NULL},
	{DISABLE, NULL},
	{DISABLE, NULL},
	{DISABLE, NULL},
};
EXPORT_SYMBOL(detected_scenes);

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
struct adap_param_setting_s adaptive_param;
struct adap_param_setting_s *adap_param;

int aipq_saturation_hue_get_base_val(void)
{
	return adap_param->satur_param.satur_hue_mab;
}

/*base value : from db
 *reg value : last frame setting
 *offset : target offset for the scene
 *bld_speed used for value smooth change
 *bld_offset: change value frame base
 */
int smooth_process(int base_val, int reg_val, int offset, int bld_rs)
{
	int bld_offset = 0;
	int temp_offset = 0;

		/*base = current reg setting*/
	if (reg_val == (base_val + offset)) {
		bld_offset = 0;
		return bld_offset;
	}

	if (!offset) {
		/*offset = 0, reset to base value*/
		temp_offset = (base_val > reg_val) ?
			 (base_val - reg_val) :
			 (reg_val - base_val);
		if (bld_rs == 0)
			bld_offset = 1;
		else
			bld_offset =
			(temp_offset + ((1 << bld_rs) - 1)) >> bld_rs;
	} else if (offset > 0) {
		if (bld_rs == 0 || bld_rs > offset)
			bld_offset = 1;
		else
			bld_offset = (offset + ((1 << bld_rs) - 1)) >> bld_rs;
	} else {
		if (bld_rs == 0 || bld_rs > (-offset))
			bld_offset = 1;
		else
			bld_offset = ((-offset) + ((1 << bld_rs) - 1)) >> bld_rs;
	}

	return bld_offset;
}

int aipq_get_bld_rs(void)
{
	if (chip_type_id != chip_t5m)
		aipq_bld_rs = 1;
	return aipq_bld_rs;
}

int aipq_get_smooth_slower_coef(void)
{
	if (chip_type_id != chip_t5m)
		slower_coef = 1 << SLOWER_BIT;
	return slower_coef;
}

int aipq_is_smooth_slower(int slower_val)
{
	if (slower_val >> SLOWER_BIT)
		return 0;
	else
		return 1;
}

int blue_scene_process(int offset, int enable)
{
	int bld_rs = 1;
	static int reg_val;
	int base_val = 0;
	int bld_offset;
	static int first_frame = 1;
	struct cm_color_md aipq_color_md;
	int force_flag;
	static int slower_num;

	if (!enable || !(aipq_en & (1 << BLUE_SCENE))) {
		aipq_color_md.color_type	 = cm_9_color;
		aipq_color_md.cm_9_color_md  = ecm2colormode_cyan;
		aipq_color_md.cm_14_color_md = cm_14_ecm2colormode_max;
		aipq_color_md.color_value	 = 0;

		cm2_sat(aipq_color_md, 0, 0);
		cm2_curve_update_sat(aipq_color_md);

		aipq_color_md.cm_9_color_md  = ecm2colormode_blue;
		cm2_sat(aipq_color_md, 0, 0);
		cm2_curve_update_sat(aipq_color_md);
		first_frame = 1;
		return 0;
	}

	if (first_frame == 1) {
		reg_val = base_val;
		slower_num = base_val;
		first_frame = 0;
	}

	bld_rs = aipq_get_bld_rs();
	bld_offset = smooth_process(base_val, reg_val, offset, bld_rs);
	if (bld_offset) {
		slower_num += aipq_get_smooth_slower_coef();
		if (aipq_is_smooth_slower(slower_num))
			bld_offset = 0;
	}

	if (reg_val != base_val + offset) {
		if (aipq_smooth_dbg)
			pr_info("%s, smooth, bld_ofst: %d, baseval: %d, regval: %d, offset: %d, bld_rs: %d, slower_num: %d\n",
				__func__, bld_offset, base_val, reg_val,
				offset, bld_rs, slower_num);
	}

	if (slower_num >= 1024)
		slower_num = 0;

	/*for s5, because size delay*/
	force_flag = cm_force_update_flag();

	if (bld_offset == 0 && force_flag == 0) {
		if (aipq_debug) {
			pr_aipq_dbg("%s, bld_ofst = 0, keep setting, baseval: %d, regval: %d, offset: %d, bld_rs %d\n",
				    __func__, base_val, reg_val,
				    offset, bld_rs);
			aipq_debug--;
		}
		return 0;
	}

	if (offset == 0)
		reg_val = (reg_val < base_val) ?
			((reg_val + bld_offset) >= base_val ?
			base_val : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < base_val ?
			base_val : (reg_val - bld_offset));
	else
		reg_val = (offset > 0) ?
			((reg_val + bld_offset) >= (base_val + offset) ?
			(base_val + offset) : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < (base_val + offset) ?
			(base_val + offset) : (reg_val - bld_offset));

	aipq_color_md.color_type	 = cm_9_color;
	aipq_color_md.cm_9_color_md  = ecm2colormode_cyan;
	aipq_color_md.cm_14_color_md = cm_14_ecm2colormode_max;
	aipq_color_md.color_value	 = reg_val;
	cm2_sat(aipq_color_md, reg_val, 0);
	cm2_curve_update_sat(aipq_color_md);

	aipq_color_md.cm_9_color_md  = ecm2colormode_blue;
	cm2_sat(aipq_color_md, reg_val, 0);
	cm2_curve_update_sat(aipq_color_md);

	if (aipq_debug) {
		pr_aipq_dbg("%s, baseval: %d, regval: %d, offset: %d,bld_ofst: %d, bld_rs: %d\n",
			    __func__, base_val, reg_val,
			    offset, bld_offset, bld_rs);
		aipq_debug--;
	}
	return 0;
}

int green_scene_process(int offset, int enable)
{
	int bld_rs = 1;
	static int reg_val;
	int base_val = 0;
	int bld_offset;
	static int first_frame = 1;
	struct cm_color_md aipq_color_md;
	int force_flag;
	static int slower_num;

	if (!enable || !(aipq_en & (1 << GREEN_SCENE))) {
		aipq_color_md.color_type	 = cm_9_color;
		aipq_color_md.cm_9_color_md  = ecm2colormode_green;
		aipq_color_md.cm_14_color_md = cm_14_ecm2colormode_max;
		aipq_color_md.color_value	 = 0;
		cm2_sat(aipq_color_md, 0, 0);
		cm2_curve_update_sat(aipq_color_md);
		first_frame = 1;
		return 0;
	}

	if (first_frame == 1) {
		reg_val = base_val;
		slower_num = base_val;
		first_frame = 0;
	}

	bld_rs = aipq_get_bld_rs();
	bld_offset = smooth_process(base_val, reg_val, offset, bld_rs);

	if (bld_offset) {
		slower_num += aipq_get_smooth_slower_coef();
		if (aipq_is_smooth_slower(slower_num))
			bld_offset = 0;
	}

	if (reg_val != base_val + offset) {
		if (aipq_smooth_dbg)
			pr_info("%s, smooth, bld_ofst: %d, baseval: %d, regval: %d, offset: %d, bld_rs: %d, slower_num: %d\n",
				__func__, bld_offset, base_val, reg_val,
				offset, bld_rs, slower_num);
	}

	if (slower_num >= 1024)
		slower_num = 0;

	/*for s5, because size delay*/
	force_flag = cm_force_update_flag();

	if (bld_offset == 0 && force_flag == 0) {
		if (aipq_debug) {
			pr_aipq_dbg("%s, bld_ofst = 0, keep setting, baseval: %d, regval: %d, offset: %d, bld_rs %d\n",
				    __func__, base_val, reg_val,
				    offset, bld_rs);
			aipq_debug--;
		}
		return 0;
	}

	if (offset == 0)
		reg_val = (reg_val < base_val) ?
			((reg_val + bld_offset) >= base_val ?
			base_val : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < base_val ?
			base_val : (reg_val - bld_offset));
	else
		reg_val = (offset > 0) ?
			((reg_val + bld_offset) >= (base_val + offset) ?
			(base_val + offset) : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < (base_val + offset) ?
			(base_val + offset) : (reg_val - bld_offset));

	aipq_color_md.color_type	 = cm_9_color;
	aipq_color_md.cm_9_color_md  = ecm2colormode_green;
	aipq_color_md.cm_14_color_md = cm_14_ecm2colormode_max;
	aipq_color_md.color_value	 = reg_val;
	cm2_sat(aipq_color_md, reg_val, 0);
	cm2_curve_update_sat(aipq_color_md);

	if (aipq_debug) {
		pr_aipq_dbg("%s, baseval: %d, regval: %d, offset: %d,bld_ofst: %d, bld_rs: %d\n",
			    __func__, base_val, reg_val,
			    offset, bld_offset, bld_rs);
		aipq_debug--;
	}

	return 0;
}

int peaking_scene_process(int offset, int enable)
{
	int bld_rs = 1;
	static int reg_val[4];
	int base_val[4];
	int bld_offset[4];
	static int first_frame = 1;
	int i;
	static int slower_num[4];

	/*sr0 hp,bp gain*/
	if (chip_type_id == chip_s6 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		base_val[0] = adap_param->peaking_param.sr0_final_pgains;
		base_val[1] = adap_param->peaking_param.sr0_final_ngains;
	} else {
		base_val[0] = adap_param->peaking_param.sr0_hp_final_gain;
		base_val[1] = adap_param->peaking_param.sr0_bp_final_gain;
	}

	/*sr1 hp,bp gain ,s6/s7d sr0 dgain,cgian*/
	if (chip_type_id == chip_s6 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9) {
		base_val[2] = adap_param->peaking_param.sr0_final_dgains;
		base_val[3] = adap_param->peaking_param.sr0_final_cgains;
	} else {
		base_val[2] = adap_param->peaking_param.sr1_hp_final_gain;
		base_val[3] = adap_param->peaking_param.sr1_bp_final_gain;
	}

	pr_aipq_dbg("%s, base_val[0]=%d, base_val[1]=%d, base_val[2]=%d, base_val[3]=%d\n",
		__func__, base_val[0], base_val[1], base_val[2], base_val[3]);

	adap_param->satur_param.offset = offset;

	if (!enable || !(aipq_en & (1 << PEAKING_SCENE))) {
		for (i = 0; i < 4; i++) {
			if (base_val[i] < 0)
				base_val[i] = 0;
			else if (base_val[i] > 255)
				base_val[i] = 255;
		}

		if (chip_type_id == chip_s6 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9)
			set_sharpness_gain(base_val[0] << 8 | base_val[1] |
				base_val[2] << 24 | base_val[3] << 16, 0);
		else
			set_sharpness_gain(base_val[0] << 8 | base_val[1],
				base_val[2] << 8 | base_val[3]);
		first_frame = 1;
		return 0;
	}

	if (first_frame == 1) {
		for (i = 0; i < 4; i++) {
			reg_val[i] = base_val[i];
			slower_num[i] = base_val[i];
		}
		first_frame = 0;
	}

	for (i = 0; i < 4; i++) {
		bld_rs = aipq_get_bld_rs();
		bld_offset[i] =
			smooth_process(base_val[i], reg_val[i], offset, bld_rs);
		if (bld_offset[i] == 0) {
			pr_aipq_dbg("%s, i = %d, bld_ofst = 0, keep setting, baseval: %d, regval: %d, offset: %d, bld_rs %d\n",
				    __func__, i, base_val[i], reg_val[i],
				    offset, bld_rs);
			continue;
		}

		if (bld_offset[i]) {
			slower_num[i] += aipq_get_smooth_slower_coef();
			if (aipq_is_smooth_slower(slower_num[i]))
				bld_offset[i] = 0;
		}

		if (reg_val[i] != base_val[i] + offset) {
			if (aipq_smooth_dbg)
				pr_info("%s, smooth, i = %d, bld_ofst: %d, baseval: %d, regval: %d, offset: %d, bld_rs: %d, slower_num: %d\n",
					__func__, i, bld_offset[i], base_val[i],
					reg_val[i], offset, bld_rs, slower_num[i]);
		}

		if (slower_num[i] >= 1024)
			slower_num[i] = 0;

		if (offset == 0)
			reg_val[i] = (reg_val[i] < base_val[i]) ?
			((reg_val[i] + bld_offset[i]) >= base_val[i] ?
			(base_val[i] + offset) : (reg_val[i] + bld_offset[i])) :
			((reg_val[i] - bld_offset[i]) < base_val[i] ?
			base_val[i] : (reg_val[i] - bld_offset[i]));
		else
			reg_val[i] = (offset > 0) ?
			((reg_val[i] + bld_offset[i]) >=
			(base_val[i] + offset) ?
			(base_val[i] + offset) :
			(reg_val[i] + bld_offset[i])) :
			((reg_val[i] - bld_offset[i]) < (base_val[i] + offset) ?
			(base_val[i] + offset) : (reg_val[i] - bld_offset[i]));

		if (aipq_debug) {
			pr_aipq_dbg("%s, i: %d, baseval: %d, regval: %d, offset: %d,bld_ofst: %d, bld_rs: %d\n",
				    __func__, i, base_val[i], reg_val[i],
				    offset, bld_offset[i], bld_rs);
			aipq_debug--;
		}
	}

	for (i = 0; i < 4; i++) {
		if (reg_val[i] < 0)
			reg_val[i] = 0;
		else if (reg_val[i] > 255)
			reg_val[i] = 255;
	}

	if (chip_type_id == chip_s6 ||
		chip_type_id == chip_s7d ||
		chip_type_id == chip_t6w ||
		chip_type_id == chip_t6x ||
		chip_type_id == chip_a9)
		set_sharpness_gain(reg_val[0] << 8 | reg_val[1] |
				reg_val[2] << 24 | reg_val[3] << 16, 0);
	else
		set_sharpness_gain(reg_val[0] << 8 | reg_val[1],
			reg_val[2] << 8 | reg_val[3]);

	return 0;
}

int contrast_scene_process(int offset, int enable)
{
	int bld_rs = 0;
	static int reg_val;
	int base_val;
	int bld_offset;
	static int first_frame = 1;

	base_val = adap_param->dnlp_param.dnlp_final_gain;
	adap_param->dnlp_param.offset = offset;

	if (!enable || !(aipq_en & (1 << DYNAMIC_CONTRAST_SCENE))) {
		ai_dnlp_param_update(base_val);
		first_frame = 1;
		return 0;
	}

	if (first_frame == 1) {
		reg_val = base_val;
		first_frame = 0;
	}

	bld_offset = smooth_process(base_val, reg_val, offset, bld_rs);

	if (reg_val != base_val + offset) {
		if (aipq_smooth_dbg)
			pr_info("%s, smooth, bld_ofst: %d, baseval: %d, regval: %d, offset: %d, bld_rs: %d\n",
				__func__, bld_offset, base_val, reg_val,
				offset, bld_rs);
	}

	if (bld_offset == 0) {
		if (aipq_debug) {
			pr_aipq_dbg("%s, bld_ofst = 0, keep setting, baseval: %d, regval: %d, offset: %d, bld_rs %d\n",
				    __func__, base_val, reg_val,
				    offset, bld_rs);
			aipq_debug--;
		}
		return 0;
	}

	if (offset == 0)
		reg_val = (reg_val < base_val) ?
			((reg_val + bld_offset) >= base_val ?
			base_val : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < base_val ?
			base_val : (reg_val - bld_offset));
	else
		reg_val = (offset > 0) ?
			((reg_val + bld_offset) >= (base_val + offset) ?
			(base_val + offset) : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < (base_val + offset) ?
			(base_val + offset) : (reg_val - bld_offset));

	if (aipq_debug) {
		pr_aipq_dbg("%s, baseval: %d, regval: %d, offset: %d,bld_ofst: %d, bld_rs: %d\n",
			    __func__, base_val, reg_val,
			    offset, bld_offset, bld_rs);
		aipq_debug--;
	}

	ai_dnlp_param_update(reg_val);

	return 0;
}

int skintone_scene_process(int offset, int enable)
{
	int bld_rs = 1;
	static int reg_val;
	int base_val = 0;
	int bld_offset;
	static int first_frame = 1;
	struct cm_color_md aipq_color_md;
	int force_flag;
	static int slower_num;

	if (!enable || !(aipq_en & (1 << SKIN_TONE_SCENE))) {
		aipq_color_md.color_type	 = cm_9_color;
		aipq_color_md.cm_9_color_md  = ecm2colormode_skin;
		aipq_color_md.cm_14_color_md = cm_14_ecm2colormode_max;
		aipq_color_md.color_value	 = 0;
		cm2_sat(aipq_color_md, 0, 0);
		cm2_curve_update_sat(aipq_color_md);
		first_frame = 1;
		return 0;
	}

	if (first_frame == 1) {
		reg_val = base_val;
		slower_num = base_val;
		first_frame = 0;
	}

	bld_rs = aipq_get_bld_rs();
	bld_offset = smooth_process(base_val, reg_val, offset, bld_rs);
	if (bld_offset) {
		slower_num += aipq_get_smooth_slower_coef();
		if (aipq_is_smooth_slower(slower_num))
			bld_offset = 0;
	}

	if (reg_val != base_val + offset) {
		if (aipq_smooth_dbg)
			pr_info("%s, smooth, bld_ofst: %d, baseval: %d, regval: %d, offset: %d, bld_rs: %d, slower_num: %d\n",
				__func__, bld_offset, base_val, reg_val,
				offset, bld_rs, slower_num);
	}

	if (slower_num >= 1024)
		slower_num = 0;

	/*for s5, because size delay*/
	force_flag = cm_force_update_flag();

	if (bld_offset == 0 && force_flag == 0) {
		if (aipq_debug) {
			pr_aipq_dbg("%s, bld_ofst = 0, keep setting, baseval: %d, regval: %d, offset: %d, bld_rs %d\n",
				    __func__, base_val, reg_val,
				    offset, bld_rs);
			aipq_debug--;
		}
		return 0;
	}

	if (offset == 0)
		reg_val = (reg_val < base_val) ?
			((reg_val + bld_offset) >= base_val ?
			base_val : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < base_val ?
			base_val : (reg_val - bld_offset));
	else
		reg_val = (offset > 0) ?
			((reg_val + bld_offset) >= (base_val + offset) ?
			(base_val + offset) : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < (base_val + offset) ?
			(base_val + offset) : (reg_val - bld_offset));

	aipq_color_md.color_type	 = cm_9_color;
	aipq_color_md.cm_9_color_md  = ecm2colormode_skin;
	aipq_color_md.cm_14_color_md = cm_14_ecm2colormode_max;
	aipq_color_md.color_value	 = reg_val;
	cm2_sat(aipq_color_md, reg_val, 0);
	cm2_curve_update_sat(aipq_color_md);

	if (aipq_debug) {
		pr_aipq_dbg("%s, baseval: %d, regval: %d, offset: %d,bld_ofst: %d, bld_rs: %d\n",
			    __func__, base_val, reg_val,
			    offset, bld_offset, bld_rs);
		aipq_debug--;
	}

	return 0;
}

int saturation_scene_process(int offset, int enable)
{
	int bld_rs = 1;
	static int reg_val;
	int base_val;
	int bld_offset;
	static int first_frame = 1;
	static int base_val_pre;
	static int slower_num;

	base_val = adap_param->satur_param.satur_hue_mab >> 16;
	adap_param->satur_param.offset = offset;

	if (!enable || !(aipq_en & (1 << SATURATION_SCENE))) {
		amvecm_saturation_hue_update(0);
		first_frame = 1;
		return 0;
	}

	if (first_frame == 1 || base_val_pre != base_val) {
		reg_val = base_val;
		slower_num = base_val;
		first_frame = 0;
	}

	base_val_pre = base_val;
	bld_rs = aipq_get_bld_rs();
	bld_offset = smooth_process(base_val, reg_val, offset, bld_rs);
	if (bld_offset) {
		slower_num += aipq_get_smooth_slower_coef();
		if (aipq_is_smooth_slower(slower_num))
			bld_offset = 0;
	}

	if (reg_val != base_val + offset) {
		if (aipq_smooth_dbg)
			pr_info("%s, smooth, bld_ofst: %d, baseval: %d, regval: %d, offset: %d, bld_rs: %d, slower_num: %d\n",
				__func__, bld_offset, base_val, reg_val,
				offset, bld_rs, slower_num);
	}

	if (slower_num >= 1024)
		slower_num = 0;

	if (bld_offset == 0) {
		if (aipq_debug) {
			pr_aipq_dbg("%s, bld_ofst = 0, keep setting, baseval: %d, regval: %d, offset: %d, bld_rs %d\n",
				    __func__, base_val, reg_val,
				    offset, bld_rs);
			aipq_debug--;
		}
		return 0;
	}

	if (offset == 0)
		reg_val = (reg_val < base_val) ?
			((reg_val + bld_offset) >= base_val ?
			base_val : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < base_val ?
			base_val : (reg_val - bld_offset));
	else
		reg_val = (offset > 0) ?
			((reg_val + bld_offset) >= (base_val + offset) ?
			(base_val + offset) : (reg_val + bld_offset)) :
			((reg_val - bld_offset) < (base_val + offset) ?
			(base_val + offset) : (reg_val - bld_offset));

	if (aipq_debug) {
		pr_aipq_dbg("%s, baseval: %d, regval: %d, offset: %d,bld_ofst: %d, bld_rs: %d\n",
			    __func__, base_val, reg_val,
			    offset, bld_offset, bld_rs);
		aipq_debug--;
	}

	amvecm_saturation_hue_update((reg_val - base_val) << 16);
	//amvecm_set_saturation_hue(reg_val << 16);
	return 0;
}

int noise_scene_process(int offset, int enable)
{
	return 0;
}

int adaptive_param_init(void)
{
	adap_param = &adaptive_param;
	/*dnlp parameters*/
	adap_param->dnlp_param.offset = 0;
	adap_param->dnlp_param.dnlp_final_gain = 8;
	/*saturation parameters*/
	adap_param->satur_param.offset = 0;
	adap_param->satur_param.satur_hue_mab = 0x1000000;
	/*peaking parameters*/
	adap_param->peaking_param.offset = 0;
	adap_param->peaking_param.sr0_hp_final_gain = 0;
	adap_param->peaking_param.sr0_bp_final_gain = 0;
	adap_param->peaking_param.sr0_final_pgains = 0;
	adap_param->peaking_param.sr0_final_ngains = 0;
	adap_param->peaking_param.sr0_final_dgains = 0;
	adap_param->peaking_param.sr0_final_cgains = 0;
	adap_param->peaking_param.sr1_hp_final_gain = 0;
	adap_param->peaking_param.sr1_bp_final_gain = 0;

	/*blue/green/skintone in cm,default def_sat_via_hs[3][32]*/
	return 0;
}

/*temporary solution for base param get from db*/
int aipq_base_dnlp_param(unsigned int final_gain)
{
	adap_param->dnlp_param.dnlp_final_gain = final_gain;
	return 0;
}

/*temporary solution for base param get from db*/
int aipq_base_peaking_param(unsigned int reg,
			    unsigned int mask,
			    unsigned int value)
{
	if (reg == offset_addr(SRSHARP0_PK_FINALGAIN_HP_BP) &&
	    (mask & 0xffff) == 0xffff) {
		adap_param->peaking_param.sr0_hp_final_gain =
			(value >> 8) & 0xff;
		adap_param->peaking_param.sr0_bp_final_gain =
			value & 0xff;
	}

	if (reg == offset_addr(SRSHARP1_PK_FINALGAIN_HP_BP) &&
	    (mask & 0xffff) == 0xffff) {
		adap_param->peaking_param.sr1_hp_final_gain =
			(value >> 8) & 0xff;
		adap_param->peaking_param.sr1_bp_final_gain =
			value & 0xff;
	}

	if (reg == offset_addr(VPP_PK_FINAL_GAIN) &&
		(mask & 0xffff) == 0xffff) {
		adap_param->peaking_param.sr0_final_dgains =
			(value >> 24) & 0xff;
		adap_param->peaking_param.sr0_final_cgains =
			(value >> 16) & 0xff;
		adap_param->peaking_param.sr0_final_pgains =
			(value >> 8) & 0xff;
		adap_param->peaking_param.sr0_final_ngains =
			value & 0xff;
	}

	pr_aipq_dbg("%s, sr0_pgains = %d, sr0_ngains = %d, dgains = %d, cgains = %d\n",
		__func__, adap_param->peaking_param.sr0_final_pgains,
		adap_param->peaking_param.sr0_final_ngains,
		adap_param->peaking_param.sr0_final_dgains,
		adap_param->peaking_param.sr0_final_cgains);

	return 0;
}

/*temporary solution for base param get from db*/
int aipq_base_satur_param(int value)
{
	adap_param->satur_param.satur_hue_mab = value;
	return 0;
}

int ai_detect_scene_init(void)
{
	enum detect_scene_e det_scs;

	for (det_scs = BLUE_SCENE; det_scs <= SCENE_MAX; det_scs++) {
		switch (det_scs) {
		case BLUE_SCENE:
			detected_scenes[det_scs].enable = ENABLE;
			detected_scenes[det_scs].func = blue_scene_process;
			break;
		case GREEN_SCENE:
			detected_scenes[det_scs].enable = ENABLE;
			detected_scenes[det_scs].func = green_scene_process;
			break;
		case SKIN_TONE_SCENE:
			detected_scenes[det_scs].enable = ENABLE;
			detected_scenes[det_scs].func = skintone_scene_process;
			break;
		case PEAKING_SCENE:
			detected_scenes[det_scs].enable = ENABLE;
			detected_scenes[det_scs].func = peaking_scene_process;
			break;
		case SATURATION_SCENE:
			detected_scenes[det_scs].enable = ENABLE;
			detected_scenes[det_scs].func =
				saturation_scene_process;
			break;
		case DYNAMIC_CONTRAST_SCENE:
			detected_scenes[det_scs].enable = ENABLE;
			detected_scenes[det_scs].func = contrast_scene_process;
			break;
		case NOISE_SCENE:
			detected_scenes[det_scs].enable = ENABLE;
			detected_scenes[det_scs].func = noise_scene_process;
			break;
		default:
			break;
		}
	}

	return 0;
}
#endif

