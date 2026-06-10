// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include "edptx_common.h"

u16 dptx_fr_int[]  = {144, 120, 100, 96, 75, 60, 50, 48, 30, 25, 24};
//u16 dptx_fr_frac[] = {120, 96, 60, 30, 24};

void edptx_vmode_apply_to_act_timing(struct edptx_drv_s *edp, struct dptx_vmode_s *vmd)
{
	struct edptx_detail_timing_s *vmd_dtd;
	unsigned long long pclk;

	if (vmd->base_dtd_idx >= DPTX_DRV_TIMING_MAX) // overflow or safemode
		vmd_dtd = &DPTX_SafeMode_640x480_timing;
	else
		vmd_dtd = &edp->panel_data.timing[vmd->base_dtd_idx];

	edp->act_timing.h_size   = vmd_dtd->h_size;
	edp->act_timing.v_size   = vmd_dtd->v_size;

	edp->act_timing.h_period = vmd_dtd->h_period;
	edp->act_timing.h_act    = vmd_dtd->h_act;
	edp->act_timing.h_blank  = vmd_dtd->h_blank;
	edp->act_timing.h_bp     = vmd_dtd->h_bp;
	edp->act_timing.h_pw     = vmd_dtd->h_pw;
	edp->act_timing.h_fp     = vmd_dtd->h_fp;

	edp->act_timing.v_period = vmd_dtd->v_period;
	edp->act_timing.v_act    = vmd_dtd->v_act;
	edp->act_timing.v_blank  = vmd_dtd->v_blank;
	edp->act_timing.v_fp     = vmd_dtd->v_fp;
	edp->act_timing.v_pw     = vmd_dtd->v_pw;
	edp->act_timing.v_bp     = vmd_dtd->v_bp;

	//edp->act_timing.h_border = vmd_dtd->h_border;
	//edp->act_timing.v_border = vmd_dtd->v_border;

	switch (vmd->fr_adv) {
	case 0:
		pclk = (vmd->fr100_int + 50) / 100;
		edp->act_timing.sync_duration_num = pclk;
		edp->act_timing.sync_duration_den = 1;
		edp->act_timing.fr1000 = pclk * 1000;
		pclk = pclk * vmd_dtd->h_period * vmd_dtd->v_period;
		edp->act_timing.pclk = pclk;
		break;
	case 1:
		pclk = vmd->fr100_int;
		pclk *= 10U;
		edp->act_timing.sync_duration_num = pclk;
		edp->act_timing.sync_duration_den = 1001;
		edp->act_timing.fr1000 = dptx_div_around(pclk * 1000, 1001);
		pclk = pclk * vmd_dtd->h_period * vmd_dtd->v_period;
		pclk = dptx_div_around(pclk, 1001);
		edp->act_timing.pclk = pclk;
		break;
	case 0xff:
	default:
		pclk = vmd_dtd->pclk;
		edp->act_timing.pclk = pclk;
		pclk = dptx_div_around(pclk * 1000, edp->act_timing.v_period);
		edp->act_timing.fr1000 = dptx_div_around(pclk, edp->act_timing.h_period);
		edp->act_timing.sync_duration_num = edp->act_timing.fr1000;
		edp->act_timing.sync_duration_den = 1000;
		break;
	}

	if (edp->vmode_mgr.vmode_cfmt_sel >= DPTX_COLOR_TYPE_MAX)
		return;

	__str_add_vmode(edp, edp->act_timing.name, vmd, edp->act_timing.fr1000 / 1000);
	edp->act_timing.cfmt = edp_cfmt_tb[edp->vmode_mgr.vmode_cfmt_sel].cfmt_id;

	eDP_PR(edp, "apply timing[%u]: %ux%u@%u.%03uHz %s pclk=%u.%uMHz",
		vmd->base_dtd_idx, edp->act_timing.h_act, edp->act_timing.v_act,
		edp->act_timing.fr1000 / 1000, edp->act_timing.fr1000 % 1000,
		edp_cfmt_tb[edp->act_timing.cfmt].name,
		edp->act_timing.pclk / 1000000, (edp->act_timing.pclk / 1000) % 1000);
}

void edptx_act_timing_apply(struct edptx_drv_s *edp)
{
	dptx_clk_set_vid_clk(edp, edp->act_timing.pclk);

	edptx_act_timing_to_venc_config(edp);
	dptx_set_venc(edp);
}

/* @param:
 * print_flag: 0~254: X-th supported, 0xff: all valid
 */
u32 edptx_print_vmode(struct edptx_drv_s *edp, char *c_buf, u8 print_flag)
{
	u8 idx, i, _pos;
	u32 h_a, v_a, fr100, str_n = 0;
	char pr_buf[200];

	struct dptx_vmode_s *vmode_p;

	if (print_flag == 0xff)
		eDP_PR(edp, "VMODE Table:");

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		memset(pr_buf, 0, 200 * sizeof(char));
		vmode_p = &edp->vmode_mgr.vmodes[idx];
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
		// h_a   = edp->edid_info.dtd_timing[vmode_p->base_dtd_idx].h_act;
		// v_a   = edp->edid_info.dtd_timing[vmode_p->base_dtd_idx].v_act;

		if (vmode_p->base_dtd_idx >= DPTX_DRV_TIMING_MAX) {// overflow or safemode
			h_a = 640;
			v_a = 480;
		} else {
			h_a   = edp->panel_data.timing[vmode_p->base_dtd_idx].h_act;
			v_a   = edp->panel_data.timing[vmode_p->base_dtd_idx].v_act;
		}

		_pos = snprintf(pr_buf, 199, " %s %2d: %4d * %4d @%3u.%-2uhz : ",
			idx == edp->vmode_mgr.vmode_sel_idx ? "*" : "-",
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

static u8 dtd_add_vmode_list(struct edptx_drv_s *edp, u8 dtd_idx, u32 fr_int, u8 fr_frac)
{
	u8 idx, i;
	u64 temp_fr;
	u32 pixel_cnt;
	struct dptx_vmode_s *tmp_vmode;
	struct edptx_detail_timing_s *tmp_dtd;

	if (dtd_idx >= DPTX_DRV_TIMING_MAX)
		return 0;

	fr_int = 100 * fr_int;

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		if (edp->vmode_mgr.vmodes[idx].flag == 0)
			break;

		tmp_vmode = &edp->vmode_mgr.vmodes[idx];
		if (tmp_vmode->base_dtd_idx >= DPTX_DRV_TIMING_MAX) // overflow or safemode
			continue;

		tmp_dtd = &edp->panel_data.timing[tmp_vmode->base_dtd_idx];
		temp_fr = fr_int ? fr_int :
			((edp->panel_data.timing[dtd_idx].fr1000 + 5) / 10);

		if (dtd_idx == tmp_vmode->base_dtd_idx ||
		    (tmp_dtd->h_act    == edp->panel_data.timing[dtd_idx].h_act &&
		     tmp_dtd->v_act    == edp->panel_data.timing[dtd_idx].v_act &&
		     tmp_dtd->h_period == edp->panel_data.timing[dtd_idx].h_period &&
		     tmp_dtd->v_period == edp->panel_data.timing[dtd_idx].v_period)) {
			if ((fr_frac == tmp_vmode->fr_adv || tmp_vmode->fr_adv == 0xff) &&
			    (dptx_diff(temp_fr, tmp_vmode->fr100_int) < 50))
				return 0;
		}
	}
	if (idx == DPTX_DRV_VMODE_MAX)
		return 0;

	tmp_dtd = &edp->panel_data.timing[dtd_idx];
	pixel_cnt = tmp_dtd->h_period; pixel_cnt *= tmp_dtd->v_period;

	edp->vmode_mgr.vmodes[idx].base_dtd_idx = dtd_idx;
	if (fr_int) { //assigned frame_rate
		edp->vmode_mgr.vmodes[idx].fr100_int = fr_int;
		edp->vmode_mgr.vmodes[idx].fr_adv    = fr_frac;
		edp->vmode_mgr.vmodes[idx].flag      = VMODE_FLAG_VALID | VMODE_FLAG_FR_RANGE;
	} else { //native frame_rate from dtd
		temp_fr = tmp_dtd->pclk;
		temp_fr = dptx_div_around(temp_fr * 1000, pixel_cnt);
		edp->vmode_mgr.vmodes[idx].fr100_int = dptx_div_around(temp_fr, 10);
		edp->vmode_mgr.vmodes[idx].fr_adv    = 0xff;
		edp->vmode_mgr.vmodes[idx].flag       = VMODE_FLAG_VALID;
	}

	temp_fr = pixel_cnt;
	temp_fr = dptx_div(temp_fr * edp->vmode_mgr.vmodes[idx].fr100_int, 100);

	if (edp->panel_data.timing[dtd_idx].cfmt) { //panel config timing
		if (dptx_vid_band_width_check(edp, temp_fr,
			edp_cfmt_tb[edp->panel_data.timing[dtd_idx].cfmt].bpp)) {
			edp->vmode_mgr.vmodes[idx].cfmt_support |=
				(1 << edp_cfmt_tb[edp->panel_data.timing[dtd_idx].cfmt].cfmt_id);
		}
	} else {
		for (i = 0; edp_cfmt_tb[i].cfmt_id != DPTX_CFMT_invalid; i++) {
			if (!((1 << edp_cfmt_tb[i].cfmt_id) & edp->sink.edid.cfmt_support))
				continue;
			if (dptx_vid_band_width_check(edp, temp_fr, edp_cfmt_tb[i].bpp)) {
				edp->vmode_mgr.vmodes[idx].cfmt_support |=
					(1 << edp_cfmt_tb[i].cfmt_id);
			}
		}
	}

	if (edptx_print_level >= LOG_V) {
		temp_fr = fr_frac ? (fr_int * 1000) / 1001 : edp->vmode_mgr.vmodes[idx].fr100_int;
		pixel_cnt = temp_fr;
		pr_info(" - add[%u]: %4u*%4u@%3u.%02uHz (h_b:%3u, v_b:%3u)\n",
			idx, tmp_dtd->h_act, tmp_dtd->v_act, pixel_cnt / 100, pixel_cnt % 100,
			tmp_dtd->h_blank, tmp_dtd->v_blank);
	}
	return 1;
}

static void safemode_add_vmode_list(struct edptx_drv_s *edp)
{
	u8 idx;

	for (idx = 0; idx < DPTX_DRV_VMODE_MAX; idx++) {
		if (edp->vmode_mgr.vmodes[idx].flag == 0)
			break;
	}
	if (idx == DPTX_DRV_VMODE_MAX)
		return;

	edp->vmode_mgr.vmodes[idx].fr100_int    = 6000;
	edp->vmode_mgr.vmodes[idx].fr_adv       = 0xff;
	edp->vmode_mgr.vmodes[idx].base_dtd_idx = 0xff;
	edp->vmode_mgr.vmodes[idx].flag         = VMODE_FLAG_VALID;
	edp->vmode_mgr.vmodes[idx].cfmt_support = 1 << DPTX_CFMT_RGB_6bit;
}

static void dptx_vmodes_reorder(struct edptx_drv_s *edp)
{
	unsigned char x, y;
	u16 dtd0_v, dtd0_h, dtd1_v, dtd1_h, s_size;
	u32 dtd0_fr, dtd1_fr;
	struct dptx_vmode_s vmd_t;
	struct dptx_vmode_s *vmode_p, *vmode_p1;

	s_size = sizeof(struct dptx_vmode_s);

	for (x = 0; x < DPTX_DRV_VMODE_MAX; x++) {
		for (y = 0; y + 1 < DPTX_DRV_VMODE_MAX - x; y++) {
			vmode_p  = &edp->vmode_mgr.vmodes[y];
			vmode_p1 = &edp->vmode_mgr.vmodes[y + 1];

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

			dtd0_v =  edp->panel_data.timing[vmode_p->base_dtd_idx].v_act;
			dtd0_h =  edp->panel_data.timing[vmode_p->base_dtd_idx].h_act;
			dtd1_v =  edp->panel_data.timing[vmode_p1->base_dtd_idx].v_act;
			dtd1_h =  edp->panel_data.timing[vmode_p1->base_dtd_idx].h_act;
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

void edptx_vmode_manage(struct edptx_drv_s *edp)
{
	struct edptx_detail_timing_s *temp_dt_p;
	//unsigned int act_pix_cnt, total_pix_cnt, dtd_idx, fr100;
	unsigned int act_pix_cnt, total_pix_cnt, fr100;
	unsigned char i, j;
	unsigned int dtd_pix_idx[DPTX_DRV_TIMING_MAX][3];

	memset(&edp->vmode_mgr.vmodes[0], 0, 64 * sizeof(struct dptx_vmode_s));

	for (i = 0; i < DPTX_DRV_TIMING_MAX; i++) {
		if (i < edp->panel_data.timing_cnt) {
			temp_dt_p = &edp->panel_data.timing[i];
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
			if (edp->panel_data.timing[i].ctrl & CTRL_DUPLICATED_TIMING ||
			    edp->panel_data.timing[j].ctrl & CTRL_DUPLICATED_TIMING)
				continue;
			if (dtd_pix_idx[i][0] != dtd_pix_idx[j][0]) //same act
				continue;

			if (edp->sink.edid.range.vfreq[1]) { //fr range
				if (dtd_pix_idx[i][1] >= dtd_pix_idx[j][1]) {
					if (dtd_pix_idx[i][2] >
							edp->sink.edid.range.vfreq[1] ||
					    dtd_pix_idx[i][2] <
							edp->sink.edid.range.vfreq[0])
						continue;
					edp->panel_data.timing[i].ctrl |= CTRL_DUPLICATED_TIMING;
				} else {
					if (dtd_pix_idx[j][2] >
							edp->sink.edid.range.vfreq[1] ||
					    dtd_pix_idx[j][2] <
							edp->sink.edid.range.vfreq[0])
						continue;
					edp->panel_data.timing[j].ctrl |= CTRL_DUPLICATED_TIMING;
				}
			} else {
				if (dtd_pix_idx[i][2] == dtd_pix_idx[j][2]) { // same fr
					if (dtd_pix_idx[i][1] >= dtd_pix_idx[j][1])
						edp->panel_data.timing[i].ctrl |=
							CTRL_DUPLICATED_TIMING;
					else
						edp->panel_data.timing[j].ctrl |=
							CTRL_DUPLICATED_TIMING;
					continue;
				}
			}
		}
	}

	for (i = 0; i < DPTX_DRV_TIMING_MAX; i++) {
		if (dtd_pix_idx[i][0] == 0)
			continue;

		if (edp->panel_data.timing[i].ctrl & CTRL_DUPLICATED_TIMING)
			continue;

		dtd_add_vmode_list(edp, i, 0, 0);

		if (!edp->sink.edid.range.vfreq[1])
			continue;

		if (edp->panel_data.timing[i].fr1000 / 1000 >
						edp->sink.edid.range.vfreq[1] ||
		    edp->panel_data.timing[i].fr1000 / 1000 <
						edp->sink.edid.range.vfreq[0])
			continue;

		for (j = 0; j < ARRAY_SIZE(dptx_fr_int); j++) {
			if (dptx_fr_int[j] <= edp->sink.edid.range.vfreq[1] &&
			    dptx_fr_int[j] >= edp->sink.edid.range.vfreq[0]) {
				dtd_add_vmode_list(edp, i, dptx_fr_int[j], 0);
			}
		}
	}

	// eDP and all mode bw not enough
	if (edp->sink.link[0].cap.DPCD_reg_func & BIT(1) && !dptx_get_optimum_vmode(edp)) {
		for (i = DPTX_DRV_VMODE_MAX - 2; i > 0; i--) {
			if (edp->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID) {
				memcpy(&edp->vmode_mgr.vmodes[i + 1],
					&edp->vmode_mgr.vmodes[i], sizeof(struct dptx_vmode_s));
				edp->vmode_mgr.vmodes[i + 1].fr100_int = 4800;
				edp->vmode_mgr.vmodes[i + 1].fr_adv = 0;
				edp->vmode_mgr.vmodes[i + 1].cfmt_support =
					1 << DPTX_CFMT_RGB_6bit;
				eDP_PR(edp, "add force 48hz");
				break;
			}
		}
	}

	if (0)
		safemode_add_vmode_list(edp);

	dptx_vmodes_reorder(edp);

	if (edptx_print_level >= LOG_V)
		edptx_print_vmode(edp, NULL, 0xff);
}

static u8 cfmt_priority[] = {
	DPTX_CFMT_RGB_10bit, DPTX_CFMT_RGB_8bit, DPTX_CFMT_RGB_6bit, DPTX_CFMT_YCbCr420_8bit,
};

struct dptx_vmode_s *dptx_get_vmode(struct edptx_drv_s *edp, u8 th)
{
	u8 j;

	if (th >= DPTX_DRV_VMODE_MAX)
		return &DPTX_SafeMode_640x480_vmode;

	if (edp->vmode_mgr.vmodes[th].cfmt_support ||
	    edp->vmode_mgr.vmodes[th].flag & VMODE_FLAG_VALID) {
		for (j = 0; j < ARRAY_SIZE(cfmt_priority); j++) {
			if ((1 << cfmt_priority[j]) & edp->vmode_mgr.vmodes[th].cfmt_support) {
				edp->vmode_mgr.vmode_cfmt_sel = cfmt_priority[j];
				break;
			}
		}
		edp->vmode_mgr.vmode_sel_idx = th;
		return &edp->vmode_mgr.vmodes[th];
	}
	return NULL;
}

struct dptx_vmode_s *dptx_get_optimum_vmode(struct edptx_drv_s *edp)
{
	u8 i, j;

	if (edp->setting.user_color_format != 0xff) {
		for (i = 0; i < DPTX_DRV_VMODE_MAX; i++) {
			if (edp->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID &&
			    ((1 << edp->setting.user_color_format) &
			     edp->vmode_mgr.vmodes[i].cfmt_support)) {
				edp->vmode_mgr.vmode_cfmt_sel = edp->setting.user_color_format;
				edp->vmode_mgr.vmode_sel_idx = i;
				return &edp->vmode_mgr.vmodes[i];
			}
		}
	}

	for (i = 0; i < DPTX_DRV_VMODE_MAX; i++) {
		if (edp->vmode_mgr.vmodes[i].flag & VMODE_FLAG_VALID &&
		    edp->vmode_mgr.vmodes[i].cfmt_support) {
			for (j = 0; j < ARRAY_SIZE(cfmt_priority); j++) {
				if ((1 << cfmt_priority[j]) &
				    edp->vmode_mgr.vmodes[i].cfmt_support) {
					edp->vmode_mgr.vmode_cfmt_sel = cfmt_priority[j];
					break;
				}
			}
			edp->vmode_mgr.vmode_sel_idx = i;
			return &edp->vmode_mgr.vmodes[i];
		}
	}

	eDP_ERR(edp, "%s no vmode available", __func__);
	return NULL;
}
