// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "dptx_common.h"

u16 dptx_fr_int[]  = {144, 120, 100, 96, 75, 60, 50, 48, 30, 25, 24};
//u16 dptx_fr_frac[] = {120, 96, 60, 30, 24};

void dptx_vmode_apply_to_act_timing(struct dptx_drv_s *dptx, struct dptx_vmode_s *vmd)
{
	struct dptx_detail_timing_s *vmd_dtd;
	unsigned long long pclk;

	if (vmd->base_dtd_idx >= DPTX_DRV_TIMING_MAX) // overflow or safemode
		vmd_dtd = &DPTX_SafeMode_640x480_timing;
	else
		vmd_dtd = &dptx->panel_data.timing[vmd->base_dtd_idx];

	dptx->act_timing.h_size   = vmd_dtd->h_size;
	dptx->act_timing.v_size   = vmd_dtd->v_size;

	dptx->act_timing.h_period = vmd_dtd->h_period;
	dptx->act_timing.h_act    = vmd_dtd->h_act;
	dptx->act_timing.h_blank  = vmd_dtd->h_blank;
	dptx->act_timing.h_bp     = vmd_dtd->h_bp;
	dptx->act_timing.h_pw     = vmd_dtd->h_pw;
	dptx->act_timing.h_fp     = vmd_dtd->h_fp;

	dptx->act_timing.v_period = vmd_dtd->v_period;
	dptx->act_timing.v_act    = vmd_dtd->v_act;
	dptx->act_timing.v_blank  = vmd_dtd->v_blank;
	dptx->act_timing.v_fp     = vmd_dtd->v_fp;
	dptx->act_timing.v_pw     = vmd_dtd->v_pw;
	dptx->act_timing.v_bp     = vmd_dtd->v_bp;

	//dptx->act_timing.h_border = vmd_dtd->h_border;
	//dptx->act_timing.v_border = vmd_dtd->v_border;

	switch (vmd->fr_adv) {
	case 0:
		pclk = (vmd->fr100_int + 50) / 100;
		dptx->act_timing.sync_duration_num = pclk;
		dptx->act_timing.sync_duration_den = 1;
		dptx->act_timing.fr1000 = pclk * 1000;
		pclk = pclk * vmd_dtd->h_period * vmd_dtd->v_period;
		dptx->act_timing.pclk = pclk;
		break;
	case 1:
		pclk = vmd->fr100_int;
		pclk *= 10U;
		dptx->act_timing.sync_duration_num = pclk;
		dptx->act_timing.sync_duration_den = 1001;
		dptx->act_timing.fr1000 = dptx_div_around(pclk * 1000, 1001);
		pclk = pclk * vmd_dtd->h_period * vmd_dtd->v_period;
		pclk = dptx_div_around(pclk, 1001);
		dptx->act_timing.pclk = pclk;
		break;
	case 0xff:
	default:
		pclk = vmd_dtd->pclk;
		dptx->act_timing.pclk = pclk;
		pclk = dptx_div_around(pclk * 1000, dptx->act_timing.v_period);
		dptx->act_timing.fr1000 = dptx_div_around(pclk, dptx->act_timing.h_period);
		dptx->act_timing.sync_duration_num = dptx->act_timing.fr1000;
		dptx->act_timing.sync_duration_den = 1000;
		break;
	}

	if (dptx->vmode_mgr.vmode_cfmt_sel >= DPTX_COLOR_TYPE_MAX)
		return;

	__str_add_vmode(dptx, dptx->act_timing.name, vmd, dptx->act_timing.fr1000 / 1000);
	dptx->act_timing.cfmt = edp_cfmt_tb[dptx->vmode_mgr.vmode_cfmt_sel].cfmt_id;

	DPTX_PR(dptx, "apply timing[%u]: %ux%u@%u.%03uHz %s pclk=%u.%uMHz",
		vmd->base_dtd_idx, dptx->act_timing.h_act, dptx->act_timing.v_act,
		dptx->act_timing.fr1000 / 1000, dptx->act_timing.fr1000 % 1000,
		edp_cfmt_tb[dptx->act_timing.cfmt].name,
		dptx->act_timing.pclk / 1000000, (dptx->act_timing.pclk / 1000) % 1000);
}

void dptx_act_timing_apply(struct dptx_drv_s *dptx)
{
	dptx_clk_set_vid_clk(dptx, dptx->act_timing.pclk);

	dptx_act_timing_to_venc_config(dptx);
	dptx_set_venc(dptx);
}

/* @param:
 * print_flag: 0~254: X-th supported, 0xff: all valid
 */
u32 dptx_print_vmode(struct dptx_drv_s *dptx, char *c_buf, u8 print_flag)
{
	u8 idx, i, _pos;
	u32 h_a, v_a, fr100, str_n = 0;
	char pr_buf[200];

	struct dptx_vmode_s *vmode_p;

	if (print_flag == 0xff)
		DPTX_PR(dptx, "VMODE Table:");

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		memset(pr_buf, 0, 200 * sizeof(char));
		vmode_p = &dptx->vmode_mgr.vmodes[idx];
		if (!(vmode_p->flag & VMODE_FLAG_VALID))
			continue;
		if (!(print_flag == 0xff || print_flag == idx))
			continue;

		if (vmode_p->fr_adv == 1) { //5994 case
			fr100 = vmode_p->fr100_int * 1000;
			fr100 = fr100 / 1001;
		} else {
			fr100 = vmode_p->fr100_int;
		}
		// h_a   = dptx->edid_info.dtd_timing[vmode_p->base_dtd_idx].h_act;
		// v_a   = dptx->edid_info.dtd_timing[vmode_p->base_dtd_idx].v_act;

		if (vmode_p->base_dtd_idx >= DPTX_DRV_TIMING_MAX) {// overflow or safemode
			h_a = 640;
			v_a = 480;
		} else {
			h_a   = dptx->panel_data.timing[vmode_p->base_dtd_idx].h_act;
			v_a   = dptx->panel_data.timing[vmode_p->base_dtd_idx].v_act;
		}

		_pos = snprintf(pr_buf, 199, " %s %2d: %4d * %4d @%3u.%-2uhz : ",
			idx == dptx->vmode_mgr.vmode_sel_idx ? "*" : "-",
			idx, h_a, v_a, fr100 / 100, fr100 % 100);
		for (i = 0; i < DPTX_COLOR_TYPE_MAX; i++) {
			if ((1 << edp_cfmt_tb[i].cfmt_id) & vmode_p->cfmt_support) {
				_pos += snprintf(pr_buf + _pos, 199 - _pos,
					"%s ", edp_cfmt_tb[i].name);
			}
		}
		//sprintf(pr_buf + _pos - 1, ") %s %s\n",
		//	(vmode_p->flag & VMODE_FLAG_PERFERED) ? "preferred " : "",
		//	(vmode_p->flag & VMODE_FLAG_FR_RANGE) ? "range" : "");
		sprintf(pr_buf + _pos, "\n");

		if (c_buf)
			str_n += snprintf(c_buf + str_n, 4095 - str_n, pr_buf);
		else
			pr_info("%s", pr_buf);
	}
	return str_n;
}

static u8 dtd_add_vmode_list(struct dptx_drv_s *dptx, u8 dtd_idx, u32 fr_int, u8 fr_frac)
{
	u8 idx, i;
	u64 temp_fr;
	u32 pixel_cnt;
	struct dptx_vmode_s *tmp_vmode;
	struct dptx_detail_timing_s *tmp_dtd;

	if (dtd_idx >= DPTX_DRV_TIMING_MAX)
		return 0;

	fr_int = 100 * fr_int;

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		if (dptx->vmode_mgr.vmodes[idx].flag == 0)
			break;

		tmp_vmode = &dptx->vmode_mgr.vmodes[idx];
		if (tmp_vmode->base_dtd_idx >= DPTX_DRV_TIMING_MAX) // overflow or safemode
			continue;

		tmp_dtd = &dptx->panel_data.timing[tmp_vmode->base_dtd_idx];
		temp_fr = fr_int ? fr_int :
			((dptx->panel_data.timing[dtd_idx].fr1000 + 5) / 10);

		if (dtd_idx == tmp_vmode->base_dtd_idx ||
		    (tmp_dtd->h_act    == dptx->panel_data.timing[dtd_idx].h_act &&
		     tmp_dtd->v_act    == dptx->panel_data.timing[dtd_idx].v_act &&
		     tmp_dtd->h_period == dptx->panel_data.timing[dtd_idx].h_period &&
		     tmp_dtd->v_period == dptx->panel_data.timing[dtd_idx].v_period)) {
			if ((fr_frac == tmp_vmode->fr_adv || tmp_vmode->fr_adv == 0xff) &&
			    (dptx_diff(temp_fr, tmp_vmode->fr100_int) < 50))
				return 0;
		}
	}
	if (idx == DPTX_DRV_VMODE_MAX)
		return 0;

	tmp_dtd = &dptx->panel_data.timing[dtd_idx];
	pixel_cnt = tmp_dtd->h_period; pixel_cnt *= tmp_dtd->v_period;

	dptx->vmode_mgr.vmodes[idx].base_dtd_idx = dtd_idx;
	if (fr_int) { //assigned frame_rate
		dptx->vmode_mgr.vmodes[idx].fr100_int = fr_int;
		dptx->vmode_mgr.vmodes[idx].fr_adv    = fr_frac;
		dptx->vmode_mgr.vmodes[idx].flag      = VMODE_FLAG_VALID | VMODE_FLAG_FR_RANGE;
	} else { //native frame_rate from dtd
		temp_fr = tmp_dtd->pclk;
		temp_fr = dptx_div_around(temp_fr * 1000, pixel_cnt);
		dptx->vmode_mgr.vmodes[idx].fr100_int = dptx_div_around(temp_fr, 10);
		dptx->vmode_mgr.vmodes[idx].fr_adv    = 0xff;
		dptx->vmode_mgr.vmodes[idx].flag       = VMODE_FLAG_VALID;
	}

	temp_fr = pixel_cnt;
	temp_fr = dptx_div(temp_fr * dptx->vmode_mgr.vmodes[idx].fr100_int, 100);

	if (dptx->panel_data.timing[dtd_idx].cfmt) { //panel config timing
		if (dptx_vid_band_width_check(dptx, temp_fr,
			edp_cfmt_tb[dptx->panel_data.timing[dtd_idx].cfmt].bpp)) {
			dptx->vmode_mgr.vmodes[idx].cfmt_support |=
				(1 << edp_cfmt_tb[dptx->panel_data.timing[dtd_idx].cfmt].cfmt_id);
		}
	} else {
		for (i = 0; edp_cfmt_tb[i].cfmt_id != DPTX_CFMT_invalid; i++) {
			if (!((1 << edp_cfmt_tb[i].cfmt_id) & dptx->sink.edid.cfmt_support))
				continue;
			if (dptx_vid_band_width_check(dptx, temp_fr, edp_cfmt_tb[i].bpp)) {
				dptx->vmode_mgr.vmodes[idx].cfmt_support |=
					(1 << edp_cfmt_tb[i].cfmt_id);
			}
		}
	}

	if (dptx_print_level >= LOG_V) {
		temp_fr = fr_frac ? (fr_int * 1000) / 1001 : dptx->vmode_mgr.vmodes[idx].fr100_int;
		pixel_cnt = temp_fr;
		pr_info(" - add[%2u]: %4u * %4u @%3u.%-2uHz (h_blank:%3u, v_blank:%3u)\n",
			idx, tmp_dtd->h_act, tmp_dtd->v_act, pixel_cnt / 100, pixel_cnt % 100,
			tmp_dtd->h_blank, tmp_dtd->v_blank);
	}
	return 1;
}

static void safemode_add_vmode_list(struct dptx_drv_s *dptx)
{
	u8 idx;

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		if (dptx->vmode_mgr.vmodes[idx].flag == 0)
			break;
	}
	if (idx == DPTX_DRV_VMODE_MAX)
		return;

	dptx->vmode_mgr.vmodes[idx].fr100_int    = 6000;
	dptx->vmode_mgr.vmodes[idx].fr_adv       = 0xff;
	dptx->vmode_mgr.vmodes[idx].base_dtd_idx = 0xff;
	dptx->vmode_mgr.vmodes[idx].flag         = VMODE_FLAG_VALID;
	dptx->vmode_mgr.vmodes[idx].cfmt_support = 1 << DPTX_CFMT_RGB_6bit;
}

static void dptx_vmodes_reorder(struct dptx_drv_s *dptx)
{
	unsigned char x, y;
	u16 dtd0_v, dtd0_h, dtd1_v, dtd1_h, s_size;
	u32 dtd0_fr, dtd1_fr;
	struct dptx_vmode_s vmd_t;
	struct dptx_vmode_s *vmode_p, *vmode_p1;

	s_size = sizeof(struct dptx_vmode_s);

	for (x = 0; x < DPTX_DRV_VMODE_MAX; x++) {
		for (y = 0; y + 1 < DPTX_DRV_VMODE_MAX - x; y++) {
			vmode_p  = &dptx->vmode_mgr.vmodes[y];
			vmode_p1 = &dptx->vmode_mgr.vmodes[y + 1];

			if (!((vmode_p->flag & VMODE_FLAG_VALID) &&
			      (vmode_p1->flag & VMODE_FLAG_VALID)))
				continue;

			if (vmode_p->base_dtd_idx >= DPTX_DRV_TIMING_MAX ||
			    vmode_p1->base_dtd_idx >= DPTX_DRV_TIMING_MAX) {// overflow or safemode
				//memcpy(&vmd_t, vmode_p, s_size);
				//memcpy(vmode_p, vmode_p1, s_size);
				//memcpy(vmode_p1, &vmd_t, s_size);
				continue;
			}

			dtd0_v =  dptx->panel_data.timing[vmode_p->base_dtd_idx].v_act;
			dtd0_h =  dptx->panel_data.timing[vmode_p->base_dtd_idx].h_act;
			dtd1_v =  dptx->panel_data.timing[vmode_p1->base_dtd_idx].v_act;
			dtd1_h =  dptx->panel_data.timing[vmode_p1->base_dtd_idx].h_act;
			dtd0_fr = vmode_p->fr_adv == 1 ? (vmode_p->fr100_int * 10 / 1001) :
							((vmode_p->fr100_int + 50) / 100);
			dtd1_fr = vmode_p1->fr_adv == 1 ? (vmode_p1->fr100_int * 10 / 1001) :
							((vmode_p1->fr100_int + 50) / 100);

			if (dtd0_v < dtd1_v ||
			    (dtd0_v == dtd1_v && dtd0_h < dtd1_h) ||
			    (dtd0_v == dtd1_v && dtd0_h == dtd1_h && dtd0_fr < dtd1_fr)) {
				memcpy(&vmd_t, vmode_p, s_size);
				memcpy(vmode_p, vmode_p1, s_size);
				memcpy(vmode_p1, &vmd_t, s_size);
			}
		}
	}
}

void dptx_vmode_manage(struct dptx_drv_s *dptx)
{
	struct dptx_detail_timing_s *temp_dt_p;
	//unsigned int act_pix_cnt, total_pix_cnt, dtd_idx, fr100;
	unsigned int act_pix_cnt, total_pix_cnt, fr100;
	unsigned char i, j;
	unsigned int dtd_pix_idx[DPTX_DRV_TIMING_MAX][3];

	memset(&dptx->vmode_mgr.vmodes[0], 0, 64 * sizeof(struct dptx_vmode_s));

	for (i = 0; i < DPTX_DRV_TIMING_MAX; i++) {
		if (i < dptx->panel_data.timing_cnt) {
			temp_dt_p = &dptx->panel_data.timing[i];
			act_pix_cnt = temp_dt_p->h_act * temp_dt_p->v_act;
			total_pix_cnt = temp_dt_p->h_period * temp_dt_p->v_period;
			fr100 = dptx_div_around(temp_dt_p->pclk, total_pix_cnt);
			dtd_pix_idx[i][0] = act_pix_cnt;
			dtd_pix_idx[i][1] = total_pix_cnt;
			dtd_pix_idx[i][2] = fr100;
		} else {
			dtd_pix_idx[i][0] = 0;
			dtd_pix_idx[i][1] = 0;
			dtd_pix_idx[i][2] = 0;
		}
	}

	for (i = 0; i < DPTX_DRV_TIMING_MAX; i++) {
		for (j = 0; j < DPTX_DRV_TIMING_MAX; j++) {
			if (i == j)
				continue;

			if (dtd_pix_idx[i][0] == 0 || dtd_pix_idx[j][0] == 0)
				continue;
			if (dptx->panel_data.timing[i].ctrl & CTRL_DUPLICATED_TIMING ||
			    dptx->panel_data.timing[j].ctrl & CTRL_DUPLICATED_TIMING)
				continue;
			if (dtd_pix_idx[i][0] != dtd_pix_idx[j][0]) //same act
				continue;

			if (dptx->sink.edid.range.vfreq[1]) { //fr range
				if (dtd_pix_idx[i][1] >= dtd_pix_idx[j][1]) {
					if (dtd_pix_idx[i][2] >
							dptx->sink.edid.range.vfreq[1] ||
					    dtd_pix_idx[i][2] <
							dptx->sink.edid.range.vfreq[0])
						continue;
					dptx->panel_data.timing[i].ctrl |= CTRL_DUPLICATED_TIMING;
				} else {
					if (dtd_pix_idx[j][2] >
							dptx->sink.edid.range.vfreq[1] ||
					    dtd_pix_idx[j][2] <
							dptx->sink.edid.range.vfreq[0])
						continue;
					dptx->panel_data.timing[j].ctrl |= CTRL_DUPLICATED_TIMING;
				}
			} else {
				if (dtd_pix_idx[i][2] == dtd_pix_idx[j][2]) { // same fr
					if (dtd_pix_idx[i][1] >= dtd_pix_idx[j][1])
						dptx->panel_data.timing[i].ctrl |=
							CTRL_DUPLICATED_TIMING;
					else
						dptx->panel_data.timing[j].ctrl |=
							CTRL_DUPLICATED_TIMING;
					continue;
				}
			}
		}
	}

	for (i = 0; i < DPTX_DRV_TIMING_MAX; i++) {
		if (dtd_pix_idx[i][0] == 0)
			continue;

		if (dptx->panel_data.timing[i].ctrl & CTRL_DUPLICATED_TIMING)
			continue;

		dtd_add_vmode_list(dptx, i, 0, 0);

		if (!dptx->sink.edid.range.vfreq[1])
			continue;

		if (dptx->panel_data.timing[i].fr1000 / 1000 >
						dptx->sink.edid.range.vfreq[1] ||
		    dptx->panel_data.timing[i].fr1000 / 1000 <
						dptx->sink.edid.range.vfreq[0])
			continue;

		for (j = 0; j < ARRAY_SIZE(dptx_fr_int); j++) {
			if (dptx_fr_int[j] <= dptx->sink.edid.range.vfreq[1] &&
			    dptx_fr_int[j] >= dptx->sink.edid.range.vfreq[0]) {
				dtd_add_vmode_list(dptx, i, dptx_fr_int[j], 0);
			}
		}
	}

	// eDP and all mode bw not enough
	if (dptx->sink.link[0].cap.DPCD_reg_func & BIT(1) && !dptx_get_optimum_vmode(dptx)) {
		for (i = DPTX_DRV_VMODE_MAX - 2; i > 0; i--) {
			if (dptx->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID) {
				memcpy(&dptx->vmode_mgr.vmodes[i + 1],
					&dptx->vmode_mgr.vmodes[i], sizeof(struct dptx_vmode_s));
				dptx->vmode_mgr.vmodes[i + 1].fr100_int = 4800;
				dptx->vmode_mgr.vmodes[i + 1].fr_adv = 0;
				dptx->vmode_mgr.vmodes[i + 1].cfmt_support =
					1 << DPTX_CFMT_RGB_6bit;
				DPTX_PR(dptx, "add force 48hz");
				break;
			}
		}
	}

	if (0)
		safemode_add_vmode_list(dptx);

	dptx_vmodes_reorder(dptx);

	if (dptx_print_level >= LOG_V)
		dptx_print_vmode(dptx, NULL, 0xff);
}

static u8 cfmt_priority[] = {
	DPTX_CFMT_RGB_10bit, DPTX_CFMT_RGB_8bit, DPTX_CFMT_RGB_6bit, DPTX_CFMT_YCbCr420_8bit,
};

struct dptx_vmode_s *dptx_get_vmode(struct dptx_drv_s *dptx, u8 th)
{
	u8 j;

	if (th >= DPTX_DRV_VMODE_MAX)
		return &DPTX_SafeMode_640x480_vmode;

	if (dptx->vmode_mgr.vmodes[th].cfmt_support ||
	    dptx->vmode_mgr.vmodes[th].flag & VMODE_FLAG_VALID) {
		for (j = 0; j < ARRAY_SIZE(cfmt_priority); j++) {
			if ((1 << cfmt_priority[j]) & dptx->vmode_mgr.vmodes[th].cfmt_support) {
				dptx->vmode_mgr.vmode_cfmt_sel = cfmt_priority[j];
				break;
			}
		}
		dptx->vmode_mgr.vmode_sel_idx = th;
		return &dptx->vmode_mgr.vmodes[th];
	}
	return NULL;
}

struct dptx_vmode_s *dptx_get_optimum_vmode(struct dptx_drv_s *dptx)
{
	u8 i, j;

	if (dptx->setting.user_color_format != 0xff) {
		for (i = 0; i < DPTX_DRV_VMODE_MAX; i++) {
			if (dptx->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID &&
			    ((1 << dptx->setting.user_color_format) &
			     dptx->vmode_mgr.vmodes[i].cfmt_support)) {
				dptx->vmode_mgr.vmode_cfmt_sel = dptx->setting.user_color_format;
				dptx->vmode_mgr.vmode_sel_idx = i;
				return &dptx->vmode_mgr.vmodes[i];
			}
		}
	}

	for (i = 0; i < DPTX_DRV_VMODE_MAX; i++) {
		if (dptx->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID &&
		    dptx->vmode_mgr.vmodes[i].cfmt_support) {
			for (j = 0; j < ARRAY_SIZE(cfmt_priority); j++) {
				if ((1 << cfmt_priority[j]) &
				    dptx->vmode_mgr.vmodes[i].cfmt_support) {
					dptx->vmode_mgr.vmode_cfmt_sel = cfmt_priority[j];
					break;
				}
			}
			dptx->vmode_mgr.vmode_sel_idx = i;
			return &dptx->vmode_mgr.vmodes[i];
		}
	}

	DPTX_ERR(dptx, "%s no vmode available", __func__);
	return NULL;
}
