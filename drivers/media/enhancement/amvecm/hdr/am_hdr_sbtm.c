// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#include <linux/types.h>
#include <linux/module.h>
#include <linux/amlogic/media/amvecm/hdr10_tmo_alg.h>

#include "am_hdr_sbtm.h"
#include "gamut_convert.h"
#include "../set_hdr2_v0.h"
#include "../s5_set_hdr2_v0.h"
#include "am_hdr10_tmo_fw.h"

uint pr_sbtm_en;

#define pr_sbtm_dbg(lvl, fmt, args...)\
	do {\
		if (pr_sbtm_en & (lvl))\
			pr_info("SBTM: " fmt, ##args);\
	} while (0)

#define NORM 50000
static u32 std_bt2020_prmy[3][2] = {
	{0.17 * NORM + 0.5, 0.797 * NORM + 0.5},	/* G */
	{0.131 * NORM + 0.5, 0.046 * NORM + 0.5},	/* B */
	{0.708 * NORM + 0.5, 0.292 * NORM + 0.5},	/* R */
};

static u32 std_bt2020_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

static u32 std_p3_primaries[3][2] = {
	{0.265 * NORM + 0.5, 0.69 * NORM + 0.5},	/* G */
	{0.15 * NORM + 0.5, 0.06 * NORM + 0.5},		/* B */
	{0.68 * NORM + 0.5, 0.32 * NORM + 0.5},		/* R */
};

static u32 std_p3_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

static u32 std_bt709_prmy[3][2] = {
	{0.30 * NORM + 0.5, 0.60 * NORM + 0.5},	/* G */
	{0.15 * NORM + 0.5, 0.06 * NORM + 0.5},	/* B */
	{0.64 * NORM + 0.5, 0.33 * NORM + 0.5},	/* R */
};

static u32 std_bt709_white_point[2] = {
	0.3127 * NORM + 0.5, 0.3290 * NORM + 0.5
};

int ncl_709_2020_sbtm[9] = {1285, 674, 89, 142, 1883, 23, 34, 180, 1834};

#define LUT_SHIFT 520
static u16 linear_e_to_o_sbtm[504] = {
	100, 101, 102, 103, 104, 105, 106, 107, 108, 109,
	110, 111, 112, 113, 114, 115, 117, 118, 119, 120,
	121, 122, 123, 125, 126, 127, 128, 130, 131, 132,
	133, 135, 136, 137, 139, 140, 141, 143, 144, 145,
	147, 148, 150, 151, 152, 154, 155, 157, 158, 160,
	161, 163, 165, 166, 168, 169, 171, 172, 174, 176,
	177, 179, 181, 183, 184, 186, 188, 190, 191, 193,
	195, 197, 199, 201, 202, 204, 206, 208, 210, 212,
	214, 216, 218, 220, 222, 224, 226, 229, 231, 233,
	235, 237, 239, 242, 244, 246, 249, 251, 253, 256,
	258, 260, 263, 265, 268, 270, 273, 275, 278, 280,
	283, 286, 288, 291, 294, 296, 299, 302, 305, 307,
	310, 313, 316, 319, 322, 325, 328, 331, 334, 337,
	340, 343, 346, 350, 353, 356, 359, 363, 366, 369,
	373, 376, 380, 383, 387, 390, 394, 397, 401, 405,
	408, 412, 416, 420, 424, 427, 431, 435, 439, 443,
	447, 451, 456, 460, 464, 468, 472, 477, 481, 485,
	490, 494, 499, 503, 508, 513, 517, 522, 527, 532,
	536, 541, 546, 551, 556, 561, 566, 571, 577, 582,
	587, 592, 598, 603, 609, 614, 620, 625, 631, 637,
	643, 648, 654, 660, 666, 672, 678, 684, 691, 697,
	703, 710, 716, 722, 729, 736, 742, 749, 756, 763,
	769, 776, 783, 791, 798, 805, 812, 819, 827, 834,
	842, 849, 857, 865, 873, 881, 888, 896, 905, 913,
	921, 929, 938, 946, 955, 963, 972, 981, 990, 998,
	1007, 1017, 1026, 1035, 1044, 1054, 1063, 1073, 1082, 1092,
	1102, 1112, 1122, 1132, 1142, 1152, 1163, 1173, 1184, 1195,
	1205, 1216, 1227, 1238, 1249, 1261, 1272, 1283, 1295, 1307,
	1318, 1330, 1342, 1354, 1366, 1379, 1391, 1404, 1416, 1429,
	1442, 1455, 1468, 1481, 1495, 1508, 1522, 1535, 1549, 1563,
	1577, 1591, 1606, 1620, 1635, 1649, 1664, 1679, 1694, 1710,
	1725, 1741, 1756, 1772, 1788, 1804, 1820, 1837, 1853, 1870,
	1887, 1904, 1921, 1938, 1956, 1974, 1991, 2009, 2027, 2046,
	2064, 2083, 2102, 2121, 2140, 2159, 2178, 2198, 2218, 2238,
	2258, 2279, 2299, 2320, 2341, 2362, 2383, 2405, 2427, 2448,
	2471, 2493, 2515, 2538, 2561, 2584, 2608, 2631, 2655, 2679,
	2703, 2728, 2753, 2777, 2803, 2828, 2854, 2879, 2905, 2932,
	2958, 2985, 3012, 3040, 3067, 3095, 3123, 3151, 3180, 3209,
	3238, 3267, 3297, 3327, 3357, 3388, 3418, 3449, 3481, 3512,
	3544, 3577, 3609, 3642, 3675, 3708, 3742, 3776, 3811, 3845,
	3880, 3916, 3951, 3987, 4024, 4061, 4098, 4135, 4173, 4211,
	4249, 4288, 4327, 4367, 4407, 4447, 4487, 4528, 4570, 4612,
	4654, 4696, 4739, 4783, 4827, 4871, 4915, 4961, 5006, 5052,
	5098, 5145, 5192, 5240, 5288, 5337, 5386, 5435, 5485, 5535,
	5586, 5638, 5690, 5742, 5795, 5848, 5902, 5956, 6011, 6067,
	6123, 6179, 6236, 6294, 6352, 6410, 6470, 6529, 6590, 6651,
	6712, 6774, 6837, 6900, 6964, 7028, 7094, 7159, 7226, 7293,
	7360, 7429, 7498, 7567, 7637, 7708, 7780, 7852, 7925, 7999,
	8074, 8149, 8225, 8301, 8379, 8457, 8536, 8615, 8696, 8777,
	8859, 8941, 9025, 9109, 9195, 9281, 9367, 9455, 9544, 9633,
	9723, 9815, 9907, 10000
};

static u32 oo_lut[HDR2_OOTF_LUT_SIZE] = {0};
static u32 cgain_lut[HDR2_CGAIN_LUT_SIZE] = {0};

/* 3847->1000  knee = 600 */
int oo_y_lut_sbtm[HDR2_OOTF_LUT_SIZE] = {
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096, 4096,
	3951, 3565, 3261, 3007, 2800, 2625, 2471, 2341, 2228, 2035, 1882, 1756,
	1653, 1564, 1488, 1422, 1366, 1269, 1194, 1130, 1079, 1034, 997, 963,
	935, 909, 887, 867, 849, 832, 818, 804, 791, 780, 769, 759,
	750, 742, 734, 726, 719
};

int oo_y_lut_sbtm_osd[HDR2_OOTF_LUT_SIZE] = {
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128
};

static int num_oo_y_lut_dbg = HDR2_OOTF_LUT_SIZE;
int oo_y_lut_dbg[HDR2_OOTF_LUT_SIZE] = {
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410, 410,
	410, 410, 410, 410, 410
};
module_param_array(oo_y_lut_dbg, int, &num_oo_y_lut_dbg, 0664);
MODULE_PARM_DESC(oo_y_lut_dbg, "\n oo_y_lut_dbg\n");

static int num_oo_y_lut_sdr_hdr_def = HDR2_OOTF_LUT_SIZE;
int oo_y_lut_sdr_hdr_def[HDR2_OOTF_LUT_SIZE] = {
	68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68, 68,
	68, 68, 68, 68, 68, 68, 68, 68, 70, 76, 80, 83,
	86, 89, 91, 93, 94, 95, 100, 106, 111, 118, 123, 126,
	129, 139, 147, 152, 156, 167, 175, 181, 185, 188, 191, 193,
	195, 196, 198, 199, 200, 202, 203, 204, 205, 206, 206, 207,
	208, 208, 209, 210, 210, 210, 211, 211, 211, 212, 212, 212,
	213, 213, 213, 213, 213, 213, 214, 214, 214, 214, 214, 214,
	214, 214, 214, 214, 214, 214, 215, 215, 215, 215, 215, 215,
	215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
	215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215, 215,
	215, 217, 218, 220, 221, 223, 225, 226, 229, 232, 235, 238,
	240, 243, 245, 248, 250, 252, 255, 258, 260, 262, 266, 270,
	274, 280, 292, 325, 409
};
module_param_array(oo_y_lut_sdr_hdr_def, int, &num_oo_y_lut_sdr_hdr_def, 0664);
MODULE_PARM_DESC(oo_y_lut_sdr_hdr_def, "\n oo_y_lut_sdr_hdr_def\n");

int cgain_sbtm[HDR2_CGAIN_LUT_SIZE] = {
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023
};

int cgain_sbtm_osd[HDR2_CGAIN_LUT_SIZE] = {
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023, 1023,
	1023, 1023
};

static unsigned int eo_lut_28_sbtm[143] = {
	0, 1, 4, 7, 12, 17, 24, 31, 39, 49, 59, 71, 83, 97,
	111, 127, 144, 320, 578, 931, 1392, 1977, 2701, 3582,
	4638, 5891, 7362, 9076, 11058, 13337, 15942, 18907,
	22266, 26058, 30324, 35106, 40454, 46418, 53052, 60415,
	68572, 77590, 87542, 98508, 110570, 123821, 138356, 154282,
	171708, 190757, 211556, 234243, 258967, 285886, 315170, 347002,
	381578, 419105, 459810, 503932, 551729, 603476, 659470, 720026,
	785484, 856207, 932584, 1015032, 1103997, 1199956, 1303424,
	1414947, 1535113, 1664552, 1803938, 1953994, 2115494, 2289267,
	2476202, 2677251, 2893435, 3125848, 3375662, 3644136, 3932616,
	4242547, 4575481, 4933078, 5317122, 5729526, 6172341, 6647771,
	7158178, 7706100, 8294261, 8925586, 9603216, 10330526, 11111139,
	11948953, 12848153, 13813241, 14849055, 15960799, 17154072, 18434897,
	19809755, 21285624, 22870015, 24571018, 26397348, 28358395, 30464279,
	32725910, 35155053, 37764399, 40567642, 43579559, 46816107, 50294518,
	54033406, 58052888, 62374710, 67022388, 72021358, 77399146, 83185550,
	89412833, 96115952, 103332784, 111104394, 119475320, 128493881, 138212527,
	148688209, 159982795, 172163524, 185303500, 199482248, 214786307,
	231309901, 249155666, 268435456
};

int init_sbtm_pbpct[5] = { 0 };
int init_sbtm_pbnits[5] = { 0 };
int sbtm_adj_y_arr[12] = { 4096 };

struct aml_tmo_hdr_hdr_reg_sw tmo_hdr_hdr_reg = {
	.tmo_en = 1,
	.tmo_highlight = 524,
	.tmo_sc_hist_th = 682,
	.tmo_oo_init_lut = eo_lut_28_sbtm,
	.sbtm_pbpct = init_sbtm_pbpct,
	.sbtm_pbnits = init_sbtm_pbnits,
	.sbtm_numpb = 0,
	.sbtm_frmpblimit = 0,
	.signal_limit = 1,
	.sbtm_pst_adj_en = 1,
	.sbtm_pst_adj_thr = 500,
	.sbtm_adj_point_y_arr = sbtm_adj_y_arr,

	/*param for alg func*/
	.hdr_ogain_shift = 12,
	.hdr_hist_sel = 0
};

static struct sbtm_info sbtmdb_reg = {
	.sbtm_support = 0,
	.max_sbtm_ver = 0,
	.grdm_support = 2,
	.drdm_ind = 1,
	.hgig_cat_drdm_sel = 0,
	.use_hgig_drdm = 0,
	.maxrgb = 0,
	.gamut = 0,
	.red_x = 0x8a48,
	.red_y = 0x3908,
	.green_x = 0x2134,
	.green_y = 0x9baa,
	.blue_x = 0x1996,
	.blue_y = 0x8fc,
	.white_x = 0x3d13,
	.white_y = 0x4042,
	.min_bright_10 = 0,
	.peak_bright_100 = 0x94,
	.p0_exp = 0,
	.p0_mant = 0x10,
	.peak_bright_p0 = 0x9f,
	.p1_exp = 0,
	.p1_mant = 0x08,
	.peak_bright_p1 = 0xa7,
	.p2_exp = 1,
	.p2_mant = 0x20,
	.peak_bright_p2 = 0xb8,
	.p3_exp = 1,
	.p3_mant = 0x08,
	.peak_bright_p3 = 0xc0,
};

struct sbtmdb_tgt_parm sbtmdb_reg_tgt = {
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}},
	{0, 0, 0, 0, 0}, {0, 0, 0, 0, 0},
	0, 1000, 0};

struct vtem_sbtm_st sbtmem_reg = {
	.sbtm_ver = 0,
	.sbtm_mode = 0,
	.sbtm_type = 3,
	.grdm_min = 0,
	.grdm_lum = 0,
	.frmpblimitint = 0x0,
};

/*sbtm params*/
uint sbtm_max_display;  /* max_display for debug*/
uint sbtmdb_auto_update = 1;/* sbtmdb update use edid info */
uint sbtm_sdr_hdr_scheme = SBTM_PROC_DTMO;  /* sbtm sdr2hdr scheme*/
uint sbtm_hdr_hdr_scheme = SBTM_PROC_DTMO;  /* sbtm hdr2hdr scheme*/

unsigned int user_sbtm_flag = 1;
unsigned int user_sbtm_get(void)
{
	return user_sbtm_flag;
}

void user_sbtm_set(unsigned int enable)
{
	user_sbtm_flag = enable;
}

uint sbtm_en;/* sbtm support 0:disable  1:grdm  2:drdm  3:grdm and drdm */
module_param(sbtm_en, uint, 0664);
MODULE_PARM_DESC(sbtm_en, "\n sbtm_en\n");

unsigned int sbtm_mode_support_get(void)
{
	return sbtm_en;
}

void sbtm_mode_support_update(struct sbtm_info *sbtmdb_info)
{
	sbtm_en = SBTM_MODE_OFF;

	if (sbtmdb_info->sbtm_support) {
		if (sbtmdb_info->grdm_support && sbtmdb_info->drdm_ind)
			sbtm_en = SBTM_MODE_ALL;
		else if (sbtmdb_info->grdm_support)
			sbtm_en = SBTM_MODE_GRDM;
		else if (sbtmdb_info->drdm_ind)
			sbtm_en = SBTM_MODE_DRDM;
		else
			sbtm_en = SBTM_MODE_OFF;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_en  update to %d\n", sbtm_en);
	}
}

uint cur_sbtm_mode[VD_PATH_MAX];
uint sbtm_mode = SBTM_MODE_OFF;

unsigned int sbtm_mode_get(void)
{
	return sbtm_mode;
}

void sbtm_mode_set(uint mode)
{
	if (sbtm_en == SBTM_MODE_ALL ||
		sbtm_en == mode ||
		mode == SBTM_MODE_OFF) {
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "set sbtm mode from %d to %d\n", sbtm_mode, mode);
		sbtm_mode = mode;
	} else {
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "set sbtm mode fail, sbtm support %d but set %d\n",
			sbtm_en, mode);
	}
}

void sbtm_mode_update(void)
{
	if (user_sbtm_flag) {
		if (sbtm_en == SBTM_MODE_ALL || sbtm_en == SBTM_MODE_DRDM)
			sbtm_mode = SBTM_MODE_DRDM;
		else if (sbtm_en == SBTM_MODE_GRDM)
			sbtm_mode = SBTM_MODE_GRDM;
		else
			sbtm_mode = SBTM_MODE_OFF;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm mode update to %d\n", sbtm_mode);
	} else {
		sbtm_mode = SBTM_MODE_OFF;
	}
}

unsigned int alpha_change;
unsigned int tmo_debug_sbtm;
unsigned int sbtmdb_change[VD_PATH_MAX];

unsigned int sbtmdb_change_get(enum vd_path_e vd_path)
{
	return sbtmdb_change[vd_path];
}

void sbtmdb_change_set(enum vd_path_e vd_path, unsigned int change)
{
	sbtmdb_change[vd_path] = change;
}

struct aml_tmo_hdr_hdr_reg_sw *tmo_hdr_hdr_reg_get(void)
{
	return &tmo_hdr_hdr_reg;
}

enum sbtm_hdr_proc_scheme sbtm_sdr_hdr_scheme_get(void)
{
	return sbtm_sdr_hdr_scheme;
}

void sbtm_sdr_hdr_scheme_set(enum sbtm_hdr_proc_scheme scheme)
{
	sbtm_sdr_hdr_scheme = scheme;
}

enum sbtm_hdr_proc_scheme sbtm_hdr_hdr_scheme_get(void)
{
	return sbtm_hdr_hdr_scheme;
}

void sbtm_hdr_hdr_scheme_set(enum sbtm_hdr_proc_scheme scheme)
{
	sbtm_hdr_hdr_scheme = scheme;
}

bool is_sbtm_tmo_dynamic(enum hdr_type_e source_format)
{
	bool ret = false;

	if ((source_format == SDR_SOURCE &&
		sbtm_sdr_hdr_scheme_get() == SBTM_PROC_DTMO) ||
		source_format == HDR10_SOURCE){
		ret = true;
	}

	return ret;
}

static int _get_maxl_e(int maxl)
{
	int i;
	int maxl_e = 0;

	if (maxl > 10000) {
		pr_sbtm_dbg(PR_SBTM_EN_WARNINGS, "maxl over 10000. invalid maxl\n");
		maxl_e = 1023;
		return maxl_e;
	}

	for (i = 0; i < 504; i++) {
		if (linear_e_to_o_sbtm[i] >= maxl) {
			maxl_e = i + LUT_SHIFT;
			break;
		}
	}

	/* pr_sbtm_dbg(PR_SBTM_EN_INFO, "maxl = %d, maxl_e = %d\n", maxl, maxl_e);*/

	return maxl_e;
}

static s64 _pqeotf(s64 e, unsigned int *eo_lut)
{
	int x = 0;
	int e_mid = 0;
	s64 o;

	e_mid = e >> 6; //12bit

	if (e_mid == 4096) {
		o = eo_lut[142];
	} else if (e_mid > 64) {
		x = (e_mid >> 5) + 14; //to find e in which interval
		if (x > 141)
			x = 141;
		e_mid = (e_mid >> 5) << 11 ;  //18bit
		o = eo_lut[x] + ((((e - e_mid) * (eo_lut[x + 1] - eo_lut[x]) >> 5) + 32) >> 6);
	} else {
		x = e_mid >> 2; //to find e in which interval
		e_mid = (e_mid >> 2) << 8;
		o = eo_lut[x] + ((((e - e_mid) * (eo_lut[x + 1] - eo_lut[x]) >> 2) + 32) >> 6);
	}
	return o;
}

static s64 _pqeo_multiply(unsigned int e)
{
	s64 o;
	s64 o_out;

	o = _pqeotf(e << 6, eo_lut_28_sbtm) >> 16;
	o_out = o * 10000 * 1000 >> 12;

	return o_out;
}

void sbtm_dest_primary_update(void)
{
	int i, j;
	u8 gamut_type;
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();
	struct sbtm_info *sbtmdb = sbtm_sbtmdb_reg_get();
	unsigned int sbtm_mode = sbtm_mode_get();

	if (sbtm_mode == SBTM_MODE_GRDM) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				tgt_parm->dest_primary[i][j] =
					std_bt2020_prmy[(i + 2) % 3][j];
				tgt_parm->dest_primary[3][j] =
					std_bt2020_white_point[j];
			}
	} else if (sbtm_mode == SBTM_MODE_DRDM) {
		gamut_type = sbtmdb->gamut;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "gamut type = %d\n", sbtmdb->gamut);
		switch (gamut_type) {
		case SBTM_DRDM_GAMUT_SPECIFIED:
			tgt_parm->dest_primary[0][0] = sbtmdb->red_x;
			tgt_parm->dest_primary[0][1] = sbtmdb->red_y;
			tgt_parm->dest_primary[1][0] = sbtmdb->green_x;
			tgt_parm->dest_primary[1][1] = sbtmdb->green_y;
			tgt_parm->dest_primary[2][0] = sbtmdb->blue_x;
			tgt_parm->dest_primary[2][1] = sbtmdb->blue_y;
			tgt_parm->dest_primary[3][0] = sbtmdb->white_x;
			tgt_parm->dest_primary[3][1] = sbtmdb->white_y;
			break;
		case SBTM_DRDM_GAMUT_709:
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					tgt_parm->dest_primary[i][j] =
						std_bt709_prmy[(i + 2) % 3][j];
					tgt_parm->dest_primary[3][j] =
						std_bt709_white_point[j];
				}
			break;
		case SBTM_DRDM_GAMUT_P3:
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					tgt_parm->dest_primary[i][j] =
						std_p3_primaries[(i + 2) % 3][j];
					tgt_parm->dest_primary[3][j] =
						std_p3_white_point[j];
				}
			break;
		case SBTM_DRDM_GAMUT_2020:
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					tgt_parm->dest_primary[i][j] =
						std_bt2020_prmy[(i + 2) % 3][j];
					tgt_parm->dest_primary[3][j] =
						std_bt2020_white_point[j];
				}
			break;
		default:
			break;
		}
	}
}

void sbtm_grdm_peaklum_update(void)
{
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();
	struct sbtm_info *sbtmdb = sbtm_sbtmdb_reg_get();

	if (sbtmdb->grdm_support == SBTM_GRDM_SDR)
		tgt_parm->frmpblimit = 300;
	else if (sbtmdb->grdm_support == SBTM_GRDM_HDR)
		tgt_parm->frmpblimit = 1000;
	else
		tgt_parm->frmpblimit = 300;

	if (sbtm_max_display)
		tgt_parm->frmpblimit = sbtm_max_display;
	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "tgt_maxl = %d\n", tgt_parm->frmpblimit);
}

u32 sbtm_cal_pbpct(u32 p_exp, u32 p_mant)
{
	u32 pbpct;
	int tmp_exp_idx;
	u32 divisor_exp = 0;
	u64 tmp_p_mant;

	tmp_exp_idx = p_exp;
	tmp_p_mant = p_mant;

	while (tmp_exp_idx)
		divisor_exp = 10 * tmp_exp_idx--;
	if (divisor_exp == 0)
		divisor_exp = 1;

	/*pct * 10000*/
	pbpct = div64_u64(div64_u64(tmp_p_mant * 10000, 64) + (divisor_exp >> 1), divisor_exp);

	return pbpct;
}

void sbtm_pb_pct_nits_update(void)
{
	u8 use_hgig;
	u8 hgig_sel;
	int i;
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();
	struct sbtm_info *sbtmdb = sbtm_sbtmdb_reg_get();
	struct aml_tmo_hdr_hdr_reg_sw *sbtm_hdr_tmo_reg = tmo_hdr_hdr_reg_get();

	use_hgig = sbtmdb->use_hgig_drdm;
	hgig_sel = sbtmdb->hgig_cat_drdm_sel;

	if (use_hgig) {
		switch (hgig_sel) {
		case SBTM_DRDM_HGIG_SEL_0:
			tgt_parm->numpb = 4;
			tgt_parm->pbpct[0] = sbtm_cal_pbpct(1, 0x08);
			tgt_parm->pbnits[0] = div64_s64(_pqeo_multiply(0xc0 << 4), 1000);
			tgt_parm->pbpct[1] = sbtm_cal_pbpct(1, 0x20);
			tgt_parm->pbnits[1] = div64_s64(_pqeo_multiply(0xb8 << 4), 1000);
			tgt_parm->pbpct[2] = sbtm_cal_pbpct(0, 0x08);
			tgt_parm->pbnits[2] = div64_s64(_pqeo_multiply(0xa7 << 4), 1000);
			tgt_parm->pbpct[3] = sbtm_cal_pbpct(0, 0x10);
			tgt_parm->pbnits[3] = div64_s64(_pqeo_multiply(0x9f << 4), 1000);
			tgt_parm->pbpct[4] = 10000;
			tgt_parm->pbnits[4] = div64_s64(_pqeo_multiply(0x94 << 4), 1000);
			tgt_parm->minnits = 0;
			break;
		case SBTM_DRDM_HGIG_SEL_1:
			tgt_parm->numpb = 0;
			tgt_parm->pbpct[0] = 10000;
			tgt_parm->pbnits[0] = 600;
			for (i = 1; i < 5; i++) {
				tgt_parm->pbpct[i] = 0;
				tgt_parm->pbnits[i] = 0;
			}
			tgt_parm->minnits = 100; // 0.1 nit * 1000
			break;
		case SBTM_DRDM_HGIG_SEL_2:
			tgt_parm->numpb = 1;
			tgt_parm->pbpct[0] = 1000;
			tgt_parm->pbnits[0] = 1000;
			tgt_parm->pbpct[1] = 10000;
			tgt_parm->pbnits[1] = 600;
			for (i = 2; i < 5; i++) {
				tgt_parm->pbpct[i] = 0;
				tgt_parm->pbnits[i] = 0;
			}
			tgt_parm->minnits = 100; // 0.1 nit * 1000
			break;
		case SBTM_DRDM_HGIG_SEL_3:
			tgt_parm->numpb = 1;
			tgt_parm->pbpct[0] = 1000;
			tgt_parm->pbnits[0] = 4000;
			tgt_parm->pbpct[1] = 10000;
			tgt_parm->pbnits[1] = 600;
			for (i = 2; i < 5; i++) {
				tgt_parm->pbpct[i] = 0;
				tgt_parm->pbnits[i] = 0;
			}
			tgt_parm->minnits = 100; // 0.1 nit * 1000
			break;
		case SBTM_DRDM_HGIG_SEL_4:
			tgt_parm->numpb = 1;
			tgt_parm->pbpct[0] = 1000;
			tgt_parm->pbnits[0] = 10000;
			tgt_parm->pbpct[1] = 10000;
			tgt_parm->pbnits[1] = 600;
			for (i = 2; i < 5; i++) {
				tgt_parm->pbpct[i] = 0;
				tgt_parm->pbnits[i] = 0;
			}
			tgt_parm->minnits = 0;
			break;
		}
	} else {
		for (i = 0; i < 5; i++) {
			tgt_parm->pbpct[i] = 0;
			tgt_parm->pbnits[i] = 0;
		}
		if (sbtmdb->p0_exp == 0 &&
			sbtmdb->p0_mant == 0 &&
			sbtmdb->peak_bright_p0 == 0) {
			tgt_parm->numpb = 0;
			tgt_parm->pbpct[0] = 10000;
			tgt_parm->pbnits[0] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_100 << 4), 1000);
		} else if (sbtmdb->p1_exp == 0 &&
			sbtmdb->p1_mant == 0 &&
			sbtmdb->peak_bright_p1 == 0) {
			tgt_parm->numpb = 1;
			tgt_parm->pbpct[0] =
				sbtm_cal_pbpct(sbtmdb->p0_exp, sbtmdb->p0_mant);
			tgt_parm->pbnits[0] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p0 << 4), 1000);
			tgt_parm->pbpct[1] = 10000;
			tgt_parm->pbnits[1] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_100 << 4), 1000);
		} else if (sbtmdb->p2_exp == 0 &&
			sbtmdb->p2_mant == 0 &&
			sbtmdb->peak_bright_p2 == 0) {
			tgt_parm->numpb = 2;
			tgt_parm->pbpct[0] =
				sbtm_cal_pbpct(sbtmdb->p1_exp, sbtmdb->p1_mant);
			tgt_parm->pbnits[0] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p1 << 4), 1000);
			tgt_parm->pbpct[1] =
				sbtm_cal_pbpct(sbtmdb->p0_exp, sbtmdb->p0_mant);
			tgt_parm->pbnits[1] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p0 << 4), 1000);
			tgt_parm->pbpct[2] = 10000;
			tgt_parm->pbnits[2] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_100 << 4), 1000);
		} else if (sbtmdb->p3_exp == 0 &&
			sbtmdb->p3_mant == 0 &&
			sbtmdb->peak_bright_p3 == 0) {
			tgt_parm->numpb = 3;
			tgt_parm->pbpct[0] =
				sbtm_cal_pbpct(sbtmdb->p2_exp, sbtmdb->p2_mant);
			tgt_parm->pbnits[0] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p2 << 4), 1000);
			tgt_parm->pbpct[1] =
				sbtm_cal_pbpct(sbtmdb->p1_exp, sbtmdb->p1_mant);
			tgt_parm->pbnits[1] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p1 << 4), 1000);
			tgt_parm->pbpct[2] =
				sbtm_cal_pbpct(sbtmdb->p0_exp, sbtmdb->p0_mant);
			tgt_parm->pbnits[2] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p0 << 4), 1000);
			tgt_parm->pbpct[3] = 10000;
			tgt_parm->pbnits[3] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_100 << 4), 1000);
		} else {
			tgt_parm->numpb = 4;
			tgt_parm->pbpct[0] =
				sbtm_cal_pbpct(sbtmdb->p3_exp, sbtmdb->p3_mant);
			tgt_parm->pbnits[0] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p3 << 4), 1000);
			tgt_parm->pbpct[1] =
				sbtm_cal_pbpct(sbtmdb->p2_exp, sbtmdb->p2_mant);
			tgt_parm->pbnits[1] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p2 << 4), 1000);
			tgt_parm->pbpct[2] =
				sbtm_cal_pbpct(sbtmdb->p1_exp, sbtmdb->p1_mant);
			tgt_parm->pbnits[2] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p1 << 4), 1000);
			tgt_parm->pbpct[3] =
				sbtm_cal_pbpct(sbtmdb->p0_exp, sbtmdb->p0_mant);
			tgt_parm->pbnits[3] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_p0 << 4), 1000);
			tgt_parm->pbpct[4] = 10000;
			tgt_parm->pbnits[4] =
				div64_s64(_pqeo_multiply(sbtmdb->peak_bright_100 << 4), 1000);
		}
		tgt_parm->minnits = _pqeo_multiply(sbtmdb->min_bright_10);
	}

	sbtm_hdr_tmo_reg->sbtm_numpb = tgt_parm->numpb;
	for (i = 0; i <= tgt_parm->numpb; i++) {
		sbtm_hdr_tmo_reg->sbtm_pbpct[i] = tgt_parm->pbpct[i];
		sbtm_hdr_tmo_reg->sbtm_pbnits[i] = tgt_parm->pbnits[i];
	}
	//pr_sbtm_dbg(PR_SBTM_EN_INFO, "numpb = %d\n",tgt_parm->numpb);
}

u32 sbtm_cal_frmpblimit(s32 pbright, s32 *pbpct, s32 *pbnits, u8 numpb)
{
	s32 pbidx;
	s64 pctdiff, pbdiff, pbrange;
	s64 frmpblilit = 0;
	u32 frmpblilitint = 0;

	if (pbright < pbpct[0] || numpb == 0) {
		frmpblilit = pbnits[0];
	} else {
		for (pbidx = 1; pbidx <= numpb; pbidx++)
			if (pbright <= pbpct[pbidx]) {
				pctdiff = pbpct[pbidx] - pbpct[pbidx - 1];
				pbdiff = pbright - pbpct[pbidx - 1];
				pbrange = div64_s64(pbdiff << 10, pctdiff);
				frmpblilit = pbnits[pbidx - 1] +
					(pbrange * (pbnits[pbidx] - pbnits[pbidx - 1]) >> 10);
				break;
			}
	}
	frmpblilitint = frmpblilit;
	return frmpblilitint;
}

u32 sbtm_cal_pbright(u32 *hist, int peak_bright_100)
{
	u32 pbright = 10000;
	int pb_th;
	int i;
	int pb_pos = 0;
	u64 el_ofst = 0;
	u64 sum_light = 0;
	u64 exceedlight = 0;
	int peak_100 = peak_bright_100 << 2; //10bit e val

	if (!hist) {
		pr_sbtm_dbg(PR_SBTM_EN_WARNINGS,
			"%s: HDR hist is NULL, pbright is pct 100.\n", __func__);
		return pbright;
	}

	pb_th = peak_100; //10bit e val
	if (tmo_debug_sbtm)
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "%s: pb_th = %d\n", __func__, pb_th);
	if (!sbtm_use_max_rgb() && get_cur_signal_type_limited()) {
		// sel 1 means y hist at e domain
		pb_th = pb_th * (940 - 64) / 1023 + 64; // 10bit e val limited
		pb_th = (pb_th > 1023) ? 1023 : pb_th;
	}

	for (i = 127; i >= 0; i--) {
		sum_light += hist[i];
		if (i * 8 > pb_th) {
			exceedlight += hist[i];
			pb_pos = i;
		}
	}
	el_ofst = pb_pos > 0 ? (((u64)hist[pb_pos - 1] * (pb_pos * 8 - pb_th)) >> 3) : 0;
	pbright = (u32)div64_u64((exceedlight + el_ofst) * 10000 + (sum_light >> 1), sum_light);
	if (tmo_debug_sbtm)
		pr_sbtm_dbg(PR_SBTM_EN_INFO,
			"%s:peak_100=%d,pb_th=%d,pbright=%u,sum_light=%lld,exceedlight=%lld,el_ofst=%lld,pb_pos=%d\n",
			__func__, peak_100, pb_th, pbright,
			sum_light, exceedlight, el_ofst, pb_pos);

	return pbright;
}

u32 sbtm_drdm_frmpblimit_cal(void)
{
	u32 frmlimit;
	u32 pbright;
	s32 *pbpct;
	s32 *pbnits;
	u8 numpb;
	int peak_bright_100;
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();
	struct sbtm_info *sbtmdb = sbtm_sbtmdb_reg_get();

	peak_bright_100 = sbtmdb->peak_bright_100;
	pbpct = tgt_parm->pbpct;
	pbnits = tgt_parm->pbnits;
	numpb = tgt_parm->numpb;
	pbright = sbtm_cal_pbright(hdr_hist[15], peak_bright_100);
	frmlimit = sbtm_cal_frmpblimit(pbright, pbpct, pbnits, numpb);

	if (sbtm_max_display)
		frmlimit = sbtm_max_display;

	pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "frmlimit=%d\n", frmlimit);

	return frmlimit;
}

void sbtm_drdm_frmpblimit_update(void)
{
	u32 frmpblimit;
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();
	enum sbtm_hdr_proc_scheme hdr_scheme = sbtm_hdr_hdr_scheme_get();
	struct aml_tmo_hdr_hdr_reg_sw *sbtm_hdr_tmo_reg = tmo_hdr_hdr_reg_get();

	if (hdr_scheme == SBTM_PROC_DTMO)
		frmpblimit = sbtm_hdr_tmo_reg->sbtm_frmpblimit;
	else
		frmpblimit = sbtm_drdm_frmpblimit_cal();
	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "hdr_scheme=%d,frmpblimit=%d\n", hdr_scheme, frmpblimit);

	tgt_parm->frmpblimit = frmpblimit;
}

void sbtm_tgt_param_update(void)
{
	sbtm_dest_primary_update();
	sbtm_pb_pct_nits_update();
}

void sbtm_sbtmdb_print(struct sbtm_info *sbtmdb_info)
{
	if (!sbtmdb_info) {
		pr_sbtm_dbg(PR_SBTM_EN_ERRORS, "sbtmdb_info is NULL\n");
		return;
	}

	pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_support = %d\n", sbtmdb_info->sbtm_support);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "max_sbtm_ver = %d\n", sbtmdb_info->max_sbtm_ver);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "grdm_support = %d\n", sbtmdb_info->grdm_support);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "drdm_ind = %d\n", sbtmdb_info->drdm_ind);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "hgig_cat_drdm_sel = %d\n", sbtmdb_info->hgig_cat_drdm_sel);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "use_hgig_drdm = %d\n", sbtmdb_info->use_hgig_drdm);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "maxrgb = %d\n", sbtmdb_info->maxrgb);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "gamut = %d\n", sbtmdb_info->gamut);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "red_x = %d\n", sbtmdb_info->red_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "red_y = %d\n", sbtmdb_info->red_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "green_x = %d\n", sbtmdb_info->green_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "green_y = %d\n", sbtmdb_info->green_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "blue_x = %d\n", sbtmdb_info->blue_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "blue_y = %d\n", sbtmdb_info->blue_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "white_x = %d\n", sbtmdb_info->white_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "white_y = %d\n", sbtmdb_info->white_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "min_bright_10 = %d\n", sbtmdb_info->min_bright_10);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_100 = %d\n", sbtmdb_info->peak_bright_100);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p0_exp = %d\n", sbtmdb_info->p0_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p0_mant = %d\n", sbtmdb_info->p0_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p0 = %d\n", sbtmdb_info->peak_bright_p0);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p1_exp = %d\n", sbtmdb_info->p1_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p1_mant = %d\n", sbtmdb_info->p1_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p1 = %d\n", sbtmdb_info->peak_bright_p1);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p2_exp = %d\n", sbtmdb_info->p2_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p2_mant = %d\n", sbtmdb_info->p2_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p2 = %d\n", sbtmdb_info->peak_bright_p2);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p3_exp = %d\n", sbtmdb_info->p3_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p3_mant = %d\n", sbtmdb_info->p3_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p3 = %d\n", sbtmdb_info->peak_bright_p3);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "drdm_ind = %d\n", sbtmdb_info->drdm_ind);
}

void sbtm_sbtmdb_reg_set(struct sbtm_info *sbtmdb_info)
{
	if (!sbtmdb_info) {
		pr_sbtm_dbg(PR_SBTM_EN_WARNINGS, "sbtmdb_info is NULL\n");
		return;
	}
	if (pr_sbtm_en & PR_SBTM_EN_DEBUG)
		sbtm_sbtmdb_print(sbtmdb_info);

	if (sbtmdb_reg.sbtm_support != sbtmdb_info->sbtm_support) {
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_support(%d,%d)\n",
			sbtmdb_reg.sbtm_support, sbtmdb_info->sbtm_support);
		sbtm_mode_support_update(sbtmdb_info);
		//sbtm_mode_update();
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);

		if (sbtmdb_info->sbtm_support == 0) {
			pr_sbtm_dbg(PR_SBTM_EN_WARNINGS, "sbtm_support is 0\n");
			sbtmdb_reg.sbtm_support = 0;
			return;
		}
	}

	if (sbtmdb_reg.sbtm_support != sbtmdb_info->sbtm_support ||
		sbtmdb_reg.max_sbtm_ver != sbtmdb_info->max_sbtm_ver ||
		sbtmdb_reg.grdm_support != sbtmdb_info->grdm_support ||
		sbtmdb_reg.drdm_ind != sbtmdb_info->drdm_ind ||
		sbtmdb_reg.hgig_cat_drdm_sel != sbtmdb_info->hgig_cat_drdm_sel ||
		sbtmdb_reg.use_hgig_drdm != sbtmdb_info->use_hgig_drdm ||
		sbtmdb_reg.maxrgb != sbtmdb_info->maxrgb ||
		sbtmdb_reg.gamut != sbtmdb_info->gamut ||
		sbtmdb_reg.red_x != sbtmdb_info->red_x ||
		sbtmdb_reg.red_y != sbtmdb_info->red_y ||
		sbtmdb_reg.green_x != sbtmdb_info->green_x ||
		sbtmdb_reg.green_y != sbtmdb_info->green_y ||
		sbtmdb_reg.blue_x != sbtmdb_info->blue_x ||
		sbtmdb_reg.blue_y != sbtmdb_info->blue_y ||
		sbtmdb_reg.white_x != sbtmdb_info->white_x ||
		sbtmdb_reg.white_y != sbtmdb_info->white_y ||
		sbtmdb_reg.min_bright_10 != sbtmdb_info->min_bright_10 ||
		sbtmdb_reg.peak_bright_100 != sbtmdb_info->peak_bright_100 ||
		sbtmdb_reg.p0_exp != sbtmdb_info->p0_exp ||
		sbtmdb_reg.p0_mant != sbtmdb_info->p0_mant ||
		sbtmdb_reg.peak_bright_p0 != sbtmdb_info->peak_bright_p0 ||
		sbtmdb_reg.p1_exp != sbtmdb_info->p1_exp ||
		sbtmdb_reg.p1_mant != sbtmdb_info->p1_mant ||
		sbtmdb_reg.peak_bright_p1 != sbtmdb_info->peak_bright_p1 ||
		sbtmdb_reg.p2_exp != sbtmdb_info->p2_exp ||
		sbtmdb_reg.p2_mant != sbtmdb_info->p2_mant ||
		sbtmdb_reg.peak_bright_p2 != sbtmdb_info->peak_bright_p2 ||
		sbtmdb_reg.p3_exp != sbtmdb_info->p3_exp ||
		sbtmdb_reg.p3_mant != sbtmdb_info->p3_mant ||
		sbtmdb_reg.peak_bright_p3 != sbtmdb_info->peak_bright_p3) {
		sbtm_sbtmdb_print(sbtmdb_info);
		sbtmdb_reg.sbtm_support = sbtmdb_info->sbtm_support;
		sbtmdb_reg.max_sbtm_ver = sbtmdb_info->max_sbtm_ver;
		sbtmdb_reg.grdm_support = sbtmdb_info->grdm_support;
		sbtmdb_reg.drdm_ind = sbtmdb_info->drdm_ind;
		sbtmdb_reg.hgig_cat_drdm_sel = sbtmdb_info->hgig_cat_drdm_sel;
		sbtmdb_reg.use_hgig_drdm = sbtmdb_info->use_hgig_drdm;
		sbtmdb_reg.maxrgb = sbtmdb_info->maxrgb;
		sbtmdb_reg.gamut = sbtmdb_info->gamut;
		sbtmdb_reg.red_x = sbtmdb_info->red_x;
		sbtmdb_reg.red_y = sbtmdb_info->red_y;
		sbtmdb_reg.green_x = sbtmdb_info->green_x;
		sbtmdb_reg.green_y = sbtmdb_info->green_y;
		sbtmdb_reg.blue_x = sbtmdb_info->blue_x;
		sbtmdb_reg.blue_y = sbtmdb_info->blue_y;
		sbtmdb_reg.white_x = sbtmdb_info->white_x;
		sbtmdb_reg.white_y = sbtmdb_info->white_y;
		sbtmdb_reg.min_bright_10 = sbtmdb_info->min_bright_10;
		sbtmdb_reg.peak_bright_100 = sbtmdb_info->peak_bright_100;
		sbtmdb_reg.p0_exp = sbtmdb_info->p0_exp;
		sbtmdb_reg.p0_mant = sbtmdb_info->p0_mant;
		sbtmdb_reg.peak_bright_p0 = sbtmdb_info->peak_bright_p0;
		sbtmdb_reg.p1_exp = sbtmdb_info->p1_exp;
		sbtmdb_reg.p1_mant = sbtmdb_info->p1_mant;
		sbtmdb_reg.peak_bright_p1 = sbtmdb_info->peak_bright_p1;
		sbtmdb_reg.p2_exp = sbtmdb_info->p2_exp;
		sbtmdb_reg.p2_mant = sbtmdb_info->p2_mant;
		sbtmdb_reg.peak_bright_p2 = sbtmdb_info->peak_bright_p2;
		sbtmdb_reg.p3_exp = sbtmdb_info->p3_exp;
		sbtmdb_reg.p3_mant = sbtmdb_info->p3_mant;
		sbtmdb_reg.peak_bright_p3 = sbtmdb_info->peak_bright_p3;

		sbtm_tgt_param_update();
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	}
}

void sbtm_sbtmdb_set(struct vinfo_s *vinfo)
{
	struct sbtm_info *sbtmdb = NULL;

	sbtmdb = &vinfo->hdr_info.sbtm_info;
	if (!sbtmdb) {
		pr_sbtm_dbg(PR_SBTM_EN_WARNINGS, "sbtmdb_info is NULL\n");
		return;
	}

	if (sbtmdb_auto_update)
		sbtm_sbtmdb_reg_set(sbtmdb);
}

struct sbtm_info *sbtm_sbtmdb_reg_get(void)
{
	return &sbtmdb_reg;
}

struct sbtmdb_tgt_parm *sbtm_sbtmdb_tgt_get(void)
{
	return &sbtmdb_reg_tgt;
}

bool sbtm_use_max_rgb(void)
{
	bool ret = false;

	if (sbtm_mode_get() == SBTM_MODE_DRDM && sbtmdb_reg.maxrgb) {
		ret = true;
		pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "max_rgb is %d\n", ret);
	}

	return ret;
}

bool sbtm_min_limited(void)
{
	bool ret = true;
	struct sbtm_info *sbtmdb = sbtm_sbtmdb_reg_get();
	unsigned int sbtm_mode = sbtm_mode_get();

	if ((sbtm_mode == SBTM_MODE_GRDM && sbtmdb->grdm_support == 1) ||
		(sbtm_mode == SBTM_MODE_DRDM && sbtmdb->drdm_ind &&
		sbtmdb->use_hgig_drdm == 1 && sbtmdb->hgig_cat_drdm_sel == 4) ||
		(sbtm_mode == SBTM_MODE_DRDM && sbtmdb->drdm_ind &&
		sbtmdb->use_hgig_drdm == 0 && sbtmdb->min_bright_10 < 0xff))
		ret = false;

	return ret;
}

/* -1: invalid osd index
 *  0: osd is disabled
 *  1: osd is enabled
 */
int (*get_osd_enable)(u32 index);

u8 sbtm_content_type_update(void)
{
	u8 sbtmtype;
	int osd_status = 0;

	if (get_cpu_type() == MESON_CPU_MAJOR_ID_T3 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_T5W) {
		if (get_osd_enable)
			osd_status = get_osd_enable(SBTM_OSD1) ||
				get_osd_enable(SBTM_OSD2) ||
				get_osd_enable(SBTM_OSD3);
		else
			osd_status = 0;

	} else if (get_cpu_type() == MESON_CPU_MAJOR_ID_T7 ||
		get_cpu_type() == MESON_CPU_MAJOR_ID_S5) {
		if (get_osd_enable)
			osd_status = get_osd_enable(SBTM_OSD1) ||
				get_osd_enable(SBTM_OSD3);
		else
			osd_status = 0;
	} else {
		if (get_osd_enable)
			osd_status = get_osd_enable(SBTM_OSD1);
		else
			osd_status = 0;
	}

	if (osd_status == 1)
		sbtmtype = SBTM_TYPE_COMPOSITED;
	else
		sbtmtype = SBTM_TYPE_VIDEO;

	return sbtmtype;
}

int register_osd_status_cb(int (*get_osd_enable_status)(u32 index))
{
	pr_info("%s: SBTM register osd enable status func\n", __func__);
	get_osd_enable = get_osd_enable_status;
	return 0;
}
EXPORT_SYMBOL(register_osd_status_cb);

void unregister_osd_status_cb(void)
{
	pr_info("SBTM unregister osd enable status func\n");
	get_osd_enable = NULL;
}
EXPORT_SYMBOL(unregister_osd_status_cb);

void sbtm_send_sbtmem_pkt(struct vinfo_s *vinfo, int clear)
{
	struct vtem_sbtm_st *sbtmem = &sbtmem_reg;
	struct sbtm_info *sbtmdb = sbtm_sbtmdb_reg_get();
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();

	if (!sbtm_mode_support_get() && !clear)
		return;

	if (clear) {
		memset(sbtmem, 0, sizeof(struct vtem_sbtm_st));
		if (vinfo && vinfo->vout_device &&
			vinfo->vout_device->fresh_tx_sbtm_pkt)
			vinfo->vout_device->fresh_tx_sbtm_pkt(vinfo->vout_device->tx_instance,
			NULL);
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "fresh_tx_sbtm_pkt, NULL\n");
		return;
	}

	sbtmem->sbtm_mode = sbtm_mode_get();
	if (sbtmem->sbtm_mode == SBTM_MODE_GRDM) {
		sbtmem->grdm_min = sbtmdb->grdm_support;
		sbtmem->grdm_lum = sbtmdb->grdm_support;
	} else {
		sbtmem->grdm_min = 0;
		sbtmem->grdm_lum = 0;
	}

	if (sbtmem->sbtm_mode == SBTM_MODE_DRDM)
		sbtmem->frmpblimitint = tgt_parm->frmpblimit;
	else
		sbtmem->frmpblimitint = 0;

	sbtmem->sbtm_type = sbtm_content_type_update();

	if (vinfo && vinfo->vout_device &&
		vinfo->vout_device->fresh_tx_sbtm_pkt)
		vinfo->vout_device->fresh_tx_sbtm_pkt(vinfo->vout_device->tx_instance, sbtmem);

	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "fresh_tx_sbtm_pkt\n");
}
EXPORT_SYMBOL(sbtm_send_sbtmem_pkt);

int scene_change_detect(int *hdr_hist, u32 sc_th)
{
	int i;
	u32 hist_diff;
	u32 diff;
	u32 sum = 0;
	int sumshft;
	int norm14;
	int ret; /*ret=1: scn change; ret=0: blend*/
	static int pre_hist[128] = {0};
	u32 sc_hist_th = sc_th;

	if (!hdr_hist) {
		if (tmo_debug_sbtm)
			pr_info("hdr hist null.\n");
		return 0;
	}

	hist_diff = 0;
	for (i = 0; i < 128; i++) {
		sum += hdr_hist[i];
		diff = (hdr_hist[i] > pre_hist[i]) ? (hdr_hist[i] - pre_hist[i]) :
			(pre_hist[i] - hdr_hist[i]);
		hist_diff += diff;

		pre_hist[i] = hdr_hist[i];
	}

	if (sum == 0) {
		if (tmo_debug_sbtm)
			pr_info("sum = 0, no hist\n");
		return 0;
	}

	sumshft =
		(sum >= (1 << 24)) ? 8 :
		(sum >= (1 << 22)) ? 6 :
		(sum >= (1 << 20)) ? 4 :
		(sum >= (1 << 18)) ? 2 :
		(sum >= (1 << 16)) ? 0 :
		(sum >= (1 << 14)) ? -2 :
		(sum >= (1 << 12)) ? -4 :
		(sum >= (1 << 10)) ? -6 :
		(sum >= (1 << 8)) ? -8 :
		(sum >= (1 << 6)) ? -10 :
		(sum >= (1 << 4)) ? -12 : -16;

	if (sumshft >= 0)
		norm14 = (1 << 30) / (sum >> sumshft);
	else if (sumshft > -16)
		norm14 = (1 << (30 + sumshft)) / sum;
	else
		norm14 = 1 << 14;

	if (sumshft >= 0)
		hist_diff = ((hist_diff >> sumshft) * norm14 + (1 << 13)) >> 14;
	else
		hist_diff = (((hist_diff << (-sumshft)) * norm14) + (1 << 13)) >> 14;

	/*normalize to 10bit*/
	hist_diff >>= 6;
	if (hist_diff > sc_hist_th)
		ret = 1;
	else
		ret = 0;

	if (tmo_debug_sbtm)
		pr_info("sum=0x%x, hist_diff=%d, sc_hist_th=%d, ret=%d\n",
			sum, hist_diff, sc_hist_th, ret);

	memcpy(pre_hist, hdr_hist, sizeof(int) * 128);

	return ret;
}

void tmo_alg_sdr2hdr_static(uint lum_tgr, int *oolut)
{
	int i = 0;
	int maxl = 10000;
	u32 lum = lum_tgr;

	/* liner gain */
	for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
		oolut[i] = (OO_COEF * lum + (maxl >> 1)) / maxl;
	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "lum = %d, oo_lut[10] = %d\n", lum, oolut[10]);
}

void tmo_alg_sdr2hdr_dynamic(int *oolut, int *cgain, int hist_flag)
{
	struct aml_tmo_reg_sw *pre_tmo_reg;
	u32 oo_lut1[HDR2_OOTF_LUT_SIZE];

	pre_tmo_reg = tmo_fw_param_get();
	if (pre_tmo_reg->pre_hdr10_tmo_alg)
		hdr10_tmo_gen(oo_lut, cgain_lut, oo_lut1, HDR_MODE_SDR2HDR10_SR_MODE, hist_flag);
	else
		pr_sbtm_dbg(PR_SBTM_EN_WARNINGS, "pre_hdr10_tmo_alg is NULL\n");

	memcpy(oolut, oo_lut, sizeof(int) * HDR2_OOTF_LUT_SIZE);
	memcpy(cgain, cgain_lut, sizeof(int) * HDR2_CGAIN_LUT_SIZE);
}

void sbtm_tmo_sdr2hdr_proc(int *oolut, int *cgain, int hist_flag)
{
	int i = 0;
	u32 tgt_lum;
	enum sbtm_hdr_proc_scheme scheme = sbtm_sdr_hdr_scheme_get();
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();

	if (hist_flag) {
		tgt_lum = sbtm_drdm_frmpblimit_cal();
		tgt_parm->frmpblimit = tgt_lum;
		pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "cal tgt_lum=%d\n", tgt_lum);
	} else {
		tgt_lum = tgt_parm->frmpblimit;
		pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "get tgt_lum=%d\n", tgt_lum);
	}
	switch (scheme) {
	case SBTM_PROC_LINER:
		/* scheme 0 liner */
		tmo_alg_sdr2hdr_static(tgt_lum, oolut);
		break;
	case SBTM_PROC_DTMO:
		/* scheme 1 dynamic */
		tmo_alg_sdr2hdr_dynamic(oolut, cgain, hist_flag);
		break;
	case SBTM_PROC_SPECIFIC:
		/* scheme 2  Y adjust*/
		for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
			oolut[i] = oo_y_lut_sdr_hdr_def[i];
		break;
	case SBTM_PROC_DEBUG:
		/* scheme 3 force lut*/
		for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
			oolut[i] = oo_y_lut_dbg[i];
		break;
	default:
		for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
			oolut[i] = oo_y_lut_sdr_hdr_def[i];
		break;
	}

	if (scheme != SBTM_PROC_DTMO)
		for (i = 0; i < HDR2_CGAIN_LUT_SIZE; i++)
			cgain[i] = 1023;
	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "scheme=%d. tgt_lum=%u\n", scheme, tgt_lum);
}

void tmo_alg_hdr2hdr_static(struct vinfo_s *vinfo, u32 lum_tgt, int *oolut)
{
	int i = 0;
	u32 metadate_maxl = 1000;

	struct master_display_info_s *p = NULL;

	if (vinfo->master_display_info.present_flag) {
		p = &vinfo->master_display_info;
		metadate_maxl = p->luminance[1];
	}

	for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
		oo_lut[i] = (OO_COEF * lum_tgt + (metadate_maxl >> 1)) / metadate_maxl;
	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "hdr2hdr_tmo convert OK.lum_tgt=%d\n", lum_tgt);
	memcpy(oolut, oo_lut, sizeof(int) * HDR2_OOTF_LUT_SIZE);
}

void tmo_alg_hdr2hdr_dynamic(int *oolut, int *cgain, int hist_flag)
{
	struct aml_tmo_reg_sw *pre_tmo_reg;
	u32 oo_lut1[HDR2_OOTF_LUT_SIZE];

	pre_tmo_reg = tmo_fw_param_get();
	if (pre_tmo_reg->pre_hdr10_tmo_alg)
		hdr10_tmo_gen(oo_lut, cgain_lut, oo_lut1, HDR_MODE_HDR102HDR10_SR_MODE, hist_flag);
	else
		pr_sbtm_dbg(PR_SBTM_EN_WARNINGS, "pre_hdr10_tmo_alg is NULL\n");

	memcpy(oolut, oo_lut, sizeof(int) * HDR2_OOTF_LUT_SIZE);
	memcpy(cgain, cgain_lut, sizeof(int) * HDR2_CGAIN_LUT_SIZE);
}

void alg_hdr2hdr_dynamic_liner(struct aml_hdr_prm_s *hdr_prm_st,
			struct aml_tmo_hdr_hdr_reg_sw *hdr_reg_tmo,
			int *hist, u32 tgt_lum)
{
	s64 pix_num = 0;
	s64 display_e;
	s64 display_o;
	s64 display_o_l;
	s64 high_light_num = 1020;
	s64 high_light_num_o;
	s64 high_light_num_o_l;
	int i;
	s64 hist_sum = 0;

	for (i = 0; i < 128; i++)
		hist_sum += hist[i];

	for (i = 127; i >= 0; i--) {
		pix_num = pix_num + hist[i];
		if (pix_num < (hist_sum * hdr_reg_tmo->tmo_highlight >> 17)) {
			 /*0.4% percent*/
			high_light_num = high_light_num - 8;
		}
	}
	if (high_light_num < 8)
		high_light_num = 8;
	if (tmo_debug_sbtm)
		pr_info("high_light_num = %lld, hist_sum = %lld, highlight_th = %d\n",
			high_light_num, hist_sum, hdr_reg_tmo->tmo_highlight);

	if (!sbtm_use_max_rgb() && get_cur_signal_type_limited()) {
		// case hdr_hist_select == 1  need limited->full
		high_light_num = div64_s64((high_light_num - 64) * 1023, (940 - 64));
		high_light_num = (high_light_num > 1023) ? 1023 : high_light_num;
	}
	if (tmo_debug_sbtm)
		pr_info("high_light_num = %lld, max_rgb = %d, signal_limited=%d\n",
			high_light_num, sbtm_use_max_rgb(), get_cur_signal_type_limited());

	display_o_l = tgt_lum;
	display_e = _get_maxl_e(tgt_lum);
	display_o = div64_s64((display_o_l << 28) + (10000 >> 1), 10000) >> 16; //28bit>> 16bit
	if (high_light_num < display_e)
		high_light_num = display_e; /* do nothing source below target lum */
	high_light_num_o = _pqeotf((high_light_num << 8), eo_lut_28_sbtm) >> 16;
	high_light_num_o_l = high_light_num_o * 10000 >> 12;
	if (tmo_debug_sbtm) {
		pr_info("high_light_num = %lld, high_light_num_o = %lld, high_light_num_o_l = %lld\n",
			high_light_num, high_light_num_o >> 2, high_light_num_o_l);
		pr_info("display_e = %lld, display_o = %lld, display_o_l = %lld\n",
			display_e, display_o >> 2, display_o_l);
	}

	/* linear gain*/
	for (i = 1; i < HDR2_OOTF_LUT_SIZE; i++) {
		hdr_prm_st->hdr_ogain_lut[i] =
			div64_s64(OO_COEF * display_o, high_light_num_o);
	}
	hdr_prm_st->hdr_ogain_lut[0] = hdr_prm_st->hdr_ogain_lut[1];
}

void tmo_alg_hdr2hdr_dynamic_liner(struct aml_tmo_hdr_hdr_reg_sw *hdr_reg_tmo,
	u32 *oo_gain)
{
	int i;
	struct aml_hdr_prm_s *gain_tbl_ptr;
	static int first_frame = 1;
	static unsigned int blend_gain[HDR2_OOTF_LUT_SIZE];
	int alpha = 250;
	int sc_flag = 0;
	u32 tgt_lum;
	u32 sc_th;
	int *hist;
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();

	gain_tbl_ptr =
		kzalloc(sizeof(struct aml_hdr_prm_s), GFP_KERNEL);

	if (!gain_tbl_ptr)
		return;

	if (chip_type_id == chip_s5)
		hist = &s5_hdr_hist[15][0];
	else
		hist = &hdr_hist[15][0];

	if (tmo_debug_sbtm) {
		pr_info("%s --------hdr hist[128]---------\n", __func__);
		for (i = 0; i < 8; i++)
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			hist[i * 16 + 0], hist[i * 16 + 1], hist[i * 16 + 2],
			hist[i * 16 + 3], hist[i * 16 + 4], hist[i * 16 + 5],
			hist[i * 16 + 6], hist[i * 16 + 7], hist[i * 16 + 8],
			hist[i * 16 + 9], hist[i * 16 + 10], hist[i * 16 + 11],
			hist[i * 16 + 12], hist[i * 16 + 13],
			hist[i * 16 + 14], hist[i * 16 + 15]);
	}

	tgt_lum = tgt_parm->frmpblimit;
	alg_hdr2hdr_dynamic_liner(gain_tbl_ptr, hdr_reg_tmo, hist, tgt_lum);

	if (first_frame)
		memcpy(blend_gain, gain_tbl_ptr->hdr_ogain_lut, sizeof(int) * 149);

	if (tmo_debug_sbtm)
		pr_info("alpha = %d\n", alpha);
	sc_th = hdr_reg_tmo->tmo_sc_hist_th;
	sc_flag = scene_change_detect(hist, sc_th);
	if (sc_flag || alpha_change) {
		alpha = 0;
		alpha_change  = 0;
	}
	if (tmo_debug_sbtm)
		pr_info("sc_flag=%d, alpha_change=%d, alpha=%d\n", sc_flag, alpha_change, alpha);

	for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++) {
		blend_gain[i] = (blend_gain[i] * alpha +
			((1 << 8) - alpha) * gain_tbl_ptr->hdr_ogain_lut[i]) >> 8;
	}
	memcpy(oo_gain, blend_gain, sizeof(int) * 149);

	if (tmo_debug_sbtm) {
		pr_info("%s -----------ogain lut[149]----------\n", __func__);
		for (i = 0; i < 14; i++)
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 0],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 1],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 2],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 3],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 4],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 5],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 6],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 7],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 8],
			gain_tbl_ptr->hdr_ogain_lut[i * 10 + 9]);
		pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			gain_tbl_ptr->hdr_ogain_lut[140], gain_tbl_ptr->hdr_ogain_lut[141],
			gain_tbl_ptr->hdr_ogain_lut[142], gain_tbl_ptr->hdr_ogain_lut[143],
			gain_tbl_ptr->hdr_ogain_lut[144], gain_tbl_ptr->hdr_ogain_lut[145],
			gain_tbl_ptr->hdr_ogain_lut[146], gain_tbl_ptr->hdr_ogain_lut[147],
			gain_tbl_ptr->hdr_ogain_lut[148]);

		pr_info("%s -----------blend ogain lut[149]----------\n", __func__);
		for (i = 0; i < 14; i++)
			pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d,\n",
			blend_gain[i * 10 + 0], blend_gain[i * 10 + 1],
			blend_gain[i * 10 + 2], blend_gain[i * 10 + 3],
			blend_gain[i * 10 + 4], blend_gain[i * 10 + 5],
			blend_gain[i * 10 + 6], blend_gain[i * 10 + 7],
			blend_gain[i * 10 + 8], blend_gain[i * 10 + 9]);
		pr_info("%d, %d, %d, %d, %d, %d, %d, %d, %d\n",
			blend_gain[140], blend_gain[141], blend_gain[142],
			blend_gain[143], blend_gain[144], blend_gain[145],
			blend_gain[146], blend_gain[147], blend_gain[148]);
		tmo_debug_sbtm--;
	}

	first_frame = 0;
	kfree(gain_tbl_ptr);
}

void sbtm_tmo_hdr2hdr_proc(struct vinfo_s *vinfo, int *oolut, int *cgain)
{
	int i = 0;
	u32 tgt_lum;
	struct aml_tmo_hdr_hdr_reg_sw *sbtm_hdr_tmo_reg = tmo_hdr_hdr_reg_get();
	enum sbtm_hdr_proc_scheme scheme = sbtm_hdr_hdr_scheme_get();
	struct sbtmdb_tgt_parm *tgt_parm = sbtm_sbtmdb_tgt_get();

	tgt_lum = tgt_parm->frmpblimit;
	switch (scheme) {
	case SBTM_PROC_LINER:
		/* scheme 0 static */
		tmo_alg_hdr2hdr_static(vinfo, tgt_lum, oolut);
		break;
	case SBTM_PROC_DTMO:
		/* scheme 1 dynamic */
		tmo_alg_hdr2hdr_dynamic(oolut, cgain, 1);
		break;
	case SBTM_PROC_SPECIFIC:
		/* scheme 2  */
		tmo_alg_hdr2hdr_dynamic_liner(sbtm_hdr_tmo_reg, oolut);
		break;
	case SBTM_PROC_DEBUG:
		/* scheme 3 force lut*/
		for (i = 0; i < HDR2_OOTF_LUT_SIZE; i++)
			oolut[i] = oo_y_lut_dbg[i];
		break;
	default:
		tmo_alg_hdr2hdr_dynamic_liner(sbtm_hdr_tmo_reg, oolut);
		break;
	}
	if (scheme != SBTM_PROC_DTMO)
		for (i = 0; i < HDR2_CGAIN_LUT_SIZE; i++)
			cgain[i] = 1023;
	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "scheme=%d. tgt_lum=%u\n", scheme, tgt_lum);
}

int sbtm_grdm_tmo_convert_proc(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct vinfo_s *vinfo)
{
	int ret = 0;
	int hist_flag = 1;

	switch (sbtm_proc_sel) {
	case SBTM_OSD_PROC:
		hist_flag = 0;
		sbtm_tmo_sdr2hdr_proc(oo_y_lut_sbtm_osd, cgain_sbtm_osd, hist_flag);
		break;
	case SBTM_SDR_HDR_PROC:
		sbtm_grdm_peaklum_update();
		sbtm_tmo_sdr2hdr_proc(oo_y_lut_sbtm, cgain_sbtm, hist_flag);
		break;
	case SBTM_HDR_HDR_PROC:
		sbtm_grdm_peaklum_update();
		sbtm_tmo_hdr2hdr_proc(vinfo, oo_y_lut_sbtm, cgain_sbtm);
		//sbtm_tmo_sdr2hdr_proc(oo_y_lut_sbtm_osd, cgain_sbtm, 0);
		break;
	default:
		break;
	}
	return ret;
}

int sbtm_drdm_tmo_convert_proc(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct vinfo_s *vinfo)
{
	int ret = 0;
	int hist_flag = 1;

	switch (sbtm_proc_sel) {
	case SBTM_OSD_PROC:
		hist_flag = 0;
		sbtm_tmo_sdr2hdr_proc(oo_y_lut_sbtm_osd, cgain_sbtm_osd, hist_flag);
		break;
	case SBTM_SDR_HDR_PROC:
		sbtm_drdm_frmpblimit_update();
		sbtm_tmo_sdr2hdr_proc(oo_y_lut_sbtm, cgain_sbtm, hist_flag);
		break;
	case SBTM_HDR_HDR_PROC:
		if (sbtm_hdr_hdr_scheme_get() != SBTM_PROC_DTMO)
			sbtm_drdm_frmpblimit_update();
		sbtm_tmo_hdr2hdr_proc(vinfo, oo_y_lut_sbtm, cgain_sbtm);
		//sbtm_tmo_alg_sdr2hdr(sbtmdb_reg_tgt.frmpblimit, oo_y_lut_sbtm_osd, 0);
		if (sbtm_hdr_hdr_scheme_get() == SBTM_PROC_DTMO)
			sbtm_drdm_frmpblimit_update();
		break;
	default:
		break;
	}
	return ret;
}

int sbtm_tmo_dynamic_process(struct vinfo_s *vinfo, enum sbtm_hdr_proc_sel sbtm_proc_sel)
{
	if (sbtm_mode == SBTM_MODE_DRDM)
		sbtm_drdm_tmo_convert_proc(sbtm_proc_sel, vinfo);
	else
		sbtm_grdm_tmo_convert_proc(sbtm_proc_sel, vinfo);
	return 0;
}

int sbtm_gamut_convert_proc(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  struct matrix_s *mtx,
			  int mtx_depth)
{
	int i, j, ret;
	s64 src_prmy[4][2];
	s64 dest_prmy[4][2];

	/*default 11bit*/
	if (mtx_depth == 0)
		mtx_depth = 11;

	if (sbtm_proc_sel == SBTM_OSD_PROC ||
		sbtm_proc_sel == SBTM_SDR_HDR_PROC) {
		for (i = 0; i < 3; i++)
			for (j = 0; j < 2; j++) {
				src_prmy[i][j] = std_bt709_prmy[(i + 2) % 3][j];
				src_prmy[3][j] = std_bt709_white_point[j];
			}
	} else if (sbtm_proc_sel == SBTM_HDR_HDR_PROC) {
		if (get_primary_policy() == PRIMARIES_AUTO) {
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					src_prmy[i][j] = std_p3_primaries[(i + 2) % 3][j];
					src_prmy[3][j] = std_p3_white_point[j];
				}
		} else {
			for (i = 0; i < 3; i++)
				for (j = 0; j < 2; j++) {
					src_prmy[i][j] = std_bt2020_prmy[(i + 2) % 3][j];
					src_prmy[3][j] = std_bt2020_white_point[j];
				}
		}
	}

	for (i = 0; i < 4; i++)
		for (j = 0; j < 2; j++)
			dest_prmy[i][j] = sbtmdb_reg_tgt.dest_primary[i][j];

	ret = gamut_convert(&src_prmy[0][0], &dest_prmy[0][0], mtx, mtx_depth);

	return ret;
}

int sbtm_convert_process(enum sbtm_hdr_proc_sel sbtm_proc_sel,
			  enum hdr_type_e src_format,
			  struct vinfo_s *vinfo,
			  struct matrix_s *mtx,
			  int mtx_depth)
{
	if (sbtm_mode != SBTM_MODE_OFF)
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_proc_sel=%x,sbtm_mode=%d\n",
			sbtm_proc_sel, sbtm_mode);

	sbtm_gamut_convert_proc(sbtm_proc_sel, mtx, mtx_depth);

	if (sbtm_mode == SBTM_MODE_DRDM) {
		if (sbtmdb_auto_update) {
			sbtm_sdr_hdr_scheme_set(SBTM_PROC_DTMO);
			sbtm_hdr_hdr_scheme_set(SBTM_PROC_DTMO);
		}
		if (is_sbtm_tmo_dynamic(src_format))
			pr_sbtm_dbg(PR_SBTM_EN_INFO, "DRDM  tmo_dynamic.\n");
		else
			sbtm_drdm_tmo_convert_proc(sbtm_proc_sel, vinfo);
	} else if (sbtm_mode == SBTM_MODE_GRDM) {
		if (sbtmdb_auto_update) {
			sbtm_sdr_hdr_scheme_set(SBTM_PROC_DTMO);
			sbtm_hdr_hdr_scheme_set(SBTM_PROC_DTMO);
		}
		if (is_sbtm_tmo_dynamic(src_format))
			pr_sbtm_dbg(PR_SBTM_EN_INFO, "GRDM  tmo_dynamic.\n");
		else
			sbtm_grdm_tmo_convert_proc(sbtm_proc_sel, vinfo);
	} else {
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_mode  off or invalid .\n");
	}

	return 0;
}

bool is_sbtm_on(void)
{
	bool ret = false;

	if (sbtm_en && sbtm_mode)
		ret = true;
	//pr_sbtm_dbg(PR_SBTM_EN_DEBUG, "sbtm_en=%d, sbtm_mode=%d\n", sbtm_en, sbtm_mode);
	return ret;
}
EXPORT_SYMBOL(is_sbtm_on);

void sbtm_tmo_hdr_hdr_param_show(void)
{
	struct aml_tmo_hdr_hdr_reg_sw *hdr_hdr_reg = tmo_hdr_hdr_reg_get();
	int i = 0;

	pr_info("tmo_en = %d\n", hdr_hdr_reg->tmo_en);
	pr_info("tmo_highlight = %d\n", hdr_hdr_reg->tmo_highlight);
	pr_info("tmo_sc_hist_th = %d\n", hdr_hdr_reg->tmo_sc_hist_th);
	pr_info("sbtm_pbpct = %d  %d  %d  %d  %d\n",
		hdr_hdr_reg->sbtm_pbpct[0], hdr_hdr_reg->sbtm_pbpct[1],
		hdr_hdr_reg->sbtm_pbpct[2], hdr_hdr_reg->sbtm_pbpct[3],
		hdr_hdr_reg->sbtm_pbpct[4]);
	pr_info("sbtm_pbnits = %d  %d  %d  %d  %d\n",
		hdr_hdr_reg->sbtm_pbnits[0], hdr_hdr_reg->sbtm_pbnits[1],
		hdr_hdr_reg->sbtm_pbnits[2], hdr_hdr_reg->sbtm_pbnits[3],
		hdr_hdr_reg->sbtm_pbnits[4]);
	pr_info("sbtm_numpb = %d\n", hdr_hdr_reg->sbtm_numpb);
	pr_info("sbtm_frmpblimit = %d\n", hdr_hdr_reg->sbtm_frmpblimit);
	pr_info("hdr_hist_sel = %d\n", hdr_hdr_reg->hdr_hist_sel);
	pr_info("signal_limit = %d\n", hdr_hdr_reg->signal_limit);
	pr_info("sbtm_pst_adj_en = %d\n", hdr_hdr_reg->sbtm_pst_adj_en);
	pr_info("sbtm_pst_adj_thr = %d\n", hdr_hdr_reg->sbtm_pst_adj_thr);
	for (i = 0; i < 12; i++)
		pr_info("sbtm_adj_point_y_arr[%d] = %d\n",
			i, hdr_hdr_reg->sbtm_adj_point_y_arr[i]);
}

int sbtm_tmo_hdr_hdr_param_dbg(char **parm)
{
	long val = 0;
	int idx = 0;
	struct aml_tmo_hdr_hdr_reg_sw *hdr_hdr_reg = tmo_hdr_hdr_reg_get();

	if (!strcmp(parm[1], "tmo_en"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		hdr_hdr_reg->tmo_en = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "tmo_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "highlight")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		hdr_hdr_reg->tmo_highlight = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "reg_highlight = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "sc_hist_th")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		hdr_hdr_reg->tmo_sc_hist_th = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sc_hist_th = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "sbtm_pst_adj_en")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		hdr_hdr_reg->sbtm_pst_adj_en = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_pst_adj_en = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "sbtm_pst_adj_thr")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		hdr_hdr_reg->sbtm_pst_adj_thr = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_pst_adj_thr = %d\n", (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[0], "sbtm_adj_point_y_arr")) {
		if (kstrtoul(parm[1], 10, &val) < 0)
			goto error;
		idx = (int)val;
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		if (idx > 12)
			goto error;
		hdr_hdr_reg->sbtm_adj_point_y_arr[idx] = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_adj_point_y_arr[%d] = %d\n", idx, (int)val);
		pr_info("\n");
	} else if (!strcmp(parm[1], "read_param")) {
		sbtm_tmo_hdr_hdr_param_show();
	}

	return 0;

error:
	return -1;
}

void sbtm_reg_show(void)
{
	int i;
	struct sbtm_info *sbtmdb_info = &sbtmdb_reg;
	struct sbtmdb_tgt_parm *sbtmdb_tgt = &sbtmdb_reg_tgt;
	struct vtem_sbtm_st *sbtmem_info = &sbtmem_reg;

	pr_sbtm_dbg(PR_SBTM_EN_INFO, "SBTMDB info.\n");
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_support = %d\n", sbtmdb_info->sbtm_support);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "max_sbtm_ver = %d\n", sbtmdb_info->max_sbtm_ver);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "grdm_support = %d\n", sbtmdb_info->grdm_support);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "drdm_ind = %d\n", sbtmdb_info->drdm_ind);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "hgig_cat_drdm_sel = %d\n", sbtmdb_info->hgig_cat_drdm_sel);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "use_hgig_drdm = %d\n", sbtmdb_info->use_hgig_drdm);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "maxrgb = %d\n", sbtmdb_info->maxrgb);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "gamut = %d\n", sbtmdb_info->gamut);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "red_x = %d\n", sbtmdb_info->red_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "red_y = %d\n", sbtmdb_info->red_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "green_x = %d\n", sbtmdb_info->green_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "green_y = %d\n", sbtmdb_info->green_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "blue_x = %d\n", sbtmdb_info->blue_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "blue_y = %d\n", sbtmdb_info->blue_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "white_x = %d\n", sbtmdb_info->white_x);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "white_y = %d\n", sbtmdb_info->white_y);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "min_bright_10 = %d\n", sbtmdb_info->min_bright_10);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_100 = %d\n", sbtmdb_info->peak_bright_100);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p0_exp = %d\n", sbtmdb_info->p0_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p0_mant = %d\n", sbtmdb_info->p0_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p0 = %d\n", sbtmdb_info->peak_bright_p0);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p1_exp = %d\n", sbtmdb_info->p1_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p1_mant = %d\n", sbtmdb_info->p1_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p1 = %d\n", sbtmdb_info->peak_bright_p1);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p2_exp = %d\n", sbtmdb_info->p2_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p2_mant = %d\n", sbtmdb_info->p2_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p2 = %d\n", sbtmdb_info->peak_bright_p2);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p3_exp = %d\n", sbtmdb_info->p3_exp);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "p3_mant = %d\n", sbtmdb_info->p3_mant);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p3 = %d\n", sbtmdb_info->peak_bright_p3);

	pr_sbtm_dbg(PR_SBTM_EN_INFO, "\nSBTMDB target info.\n");
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "target_paimary: %d, %d, %d, %d, %d, %d, %d, %d\n",
		sbtmdb_tgt->dest_primary[0][0], sbtmdb_tgt->dest_primary[0][1],
		sbtmdb_tgt->dest_primary[1][0], sbtmdb_tgt->dest_primary[1][1],
		sbtmdb_tgt->dest_primary[2][0], sbtmdb_tgt->dest_primary[2][1],
		sbtmdb_tgt->dest_primary[3][0], sbtmdb_tgt->dest_primary[3][1]);
	for (i = 0; i < 5; i++)
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "pbpct[%d] = %d, pbnits[%d] = %d\n",
			i, sbtmdb_tgt->pbpct[i], i, sbtmdb_tgt->pbnits[i]);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "numpb = %d\n", sbtmdb_tgt->numpb);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "frmpblimit = %d\n", sbtmdb_tgt->frmpblimit);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "minnits = %d\n", sbtmdb_tgt->minnits);

	pr_sbtm_dbg(PR_SBTM_EN_INFO, "\nSBTMEM info.\n");
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_ver = %d\n", sbtmem_info->sbtm_ver);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_mode = %d\n", sbtmem_info->sbtm_mode);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_type = %d\n", sbtmem_info->sbtm_type);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "grdm_min = %d\n", sbtmem_info->grdm_min);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "grdm_lum = %d\n", sbtmem_info->grdm_lum);
	pr_sbtm_dbg(PR_SBTM_EN_INFO, "frmpblimitint = %d\n", sbtmem_info->frmpblimitint);
}

int sbtm_sbtmdb_reg_dbg(char **parm)
{
	long val = 0;

	if (!strcmp(parm[1], "sbtm_support"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.sbtm_support = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_support = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "max_sbtm_ver")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.max_sbtm_ver = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "max_sbtm_ver = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "grdm_support")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		if (sbtmdb_reg.grdm_support != (int)val &&
			sbtm_hdr_hdr_scheme_get() == SBTM_PROC_SPECIFIC)
			alpha_change = 1;
		sbtmdb_reg.grdm_support = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "grdm_support = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "drdm_ind")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.drdm_ind = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "drdm_ind = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "hgig_cat_drdm_sel")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.hgig_cat_drdm_sel = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "hgig_cat_drdm_sel = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "use_hgig_drdm")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.use_hgig_drdm = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "use_hgig_drdm = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "maxrgb")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.maxrgb = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "maxrgb = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "gamut")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.gamut = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "gamut = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "red_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.red_x = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "red_x = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "red_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.red_y = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "red_y = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "green_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.green_x = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "green_x = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "green_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.green_y = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "green_y = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "blue_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.blue_x = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "blue_x = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "blue_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.blue_y = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "blue_y = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "white_x")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.white_x = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "white_x = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "white_y")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.white_y = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "white_y = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "min_bright_10")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.min_bright_10 = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "min_bright_10 = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "peak_bright_100")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_100 = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_100 = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p0_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p0_exp = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p0_exp = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p0_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p0_mant = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p0_mant = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "peak_bright_p0")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p0 = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p0 = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p1_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p1_exp = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p1_exp = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p1_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p1_mant = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p1_mant = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "peak_bright_p1")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p1 = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p1 = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p2_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p2_exp = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p2_exp = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p2_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p2_mant = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p2_mant = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "peak_bright_p2")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p2 = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p2 = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p3_exp")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p3_exp = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p3_exp = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "p3_mant")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.p3_mant = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "p3_mant = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "peak_bright_p3")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_reg.peak_bright_p3 = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "peak_bright_p3 = %d\n", (int)val);
		pr_info("\n");
		sbtmdb_change_set(VD1_PATH, 1);
		sbtmdb_change_set(VD2_PATH, 1);
	} else if (!strcmp(parm[1], "reg_dump")) {
		sbtm_reg_show();
	}

	return 0;

error:
	return -1;
}

int sbtm_param_dbg(char **parm)
{
	long val = 0;

	if (!strcmp(parm[1], "sbtm_max_display"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtm_max_display = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_max_display set to %d\n", (int)val);
	} else if (!strcmp(parm[1], "sbtm_sdr_hdr_scheme"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtm_sdr_hdr_scheme_set((enum sbtm_hdr_proc_scheme)val);
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_sdr_hdr_scheme set to %d\n", (int)val);
	} else if (!strcmp(parm[1], "sbtm_hdr_hdr_scheme"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtm_hdr_hdr_scheme_set((enum sbtm_hdr_proc_scheme)val);
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtm_hdr_hdr_scheme set to %d\n", (int)val);
	} else if (!strcmp(parm[1], "sbtmdb_auto_update"))  {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtmdb_auto_update = (int)val;
		pr_sbtm_dbg(PR_SBTM_EN_INFO, "sbtmdb_auto_update set to %d\n", (int)val);
	} else if (!strcmp(parm[1], "mode")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		sbtm_mode_set(val);
	} else if (!strcmp(parm[1], "en")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		user_sbtm_set(val);
	} else if (!strcmp(parm[1], "tmo_debug_sbtm")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		tmo_debug_sbtm = val;
	} else if (!strcmp(parm[1], "pr_sbtm_en")) {
		if (kstrtoul(parm[2], 10, &val) < 0)
			goto error;
		pr_sbtm_en = val;
	} else if (!strcmp(parm[1], "param_r")) {
		pr_info("sbtm_en = %d\n", sbtm_en);
		pr_info("sbtm_mode = %d\n", sbtm_mode);
		pr_info("sbtm_max_display = %d\n", sbtm_max_display);
		pr_info("sbtm_sdr_hdr_scheme = %d\n", sbtm_sdr_hdr_scheme);
		pr_info("sbtm_hdr_hdr_scheme = %d\n", sbtm_hdr_hdr_scheme);
		pr_info("sbtmdb_auto_update = %d\n", sbtmdb_auto_update);
		pr_info("user_sbtm_flag = %d\n", user_sbtm_flag);
		pr_info("tmo_debug_sbtm = %d\n", tmo_debug_sbtm);
		pr_info("pr_sbtm_en = %d\n", pr_sbtm_en);
	}
	return 0;

error:
	return -1;
}
#endif
