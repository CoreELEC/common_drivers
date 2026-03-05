/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef CONFIG_AMLOGIC_ZAPPER_CUT
#ifndef _AM_HDR_SBTM_H
#define _AM_HDR_SBTM_H

#include <linux/amlogic/media/amvecm/hdr2_ext.h>
#include "../amcsc.h"

#define  PR_SBTM_EN_INFO 0x1
#define  PR_SBTM_EN_DEBUG 0x2
#define  PR_SBTM_EN_WARNINGS 0x4
#define  PR_SBTM_EN_ERRORS 0x8
#define OO_COEF 4096

enum sbtm_en_mode {
	SBTM_MODE_OFF = 0,
	SBTM_MODE_GRDM,
	SBTM_MODE_DRDM,
	SBTM_MODE_ALL,
	SBTM_MODE_MAX
};

enum sbtm_type {
	SBTM_TYPE_OFF = 0,
	SBTM_TYPE_COMPOSITED,
	SBTM_TYPE_GAME_PC,
	SBTM_TYPE_VIDEO,
	SBTM_TYPE_MAX
};

enum sbtm_drdm_gamut {
	SBTM_DRDM_GAMUT_SPECIFIED = 0,
	SBTM_DRDM_GAMUT_709,
	SBTM_DRDM_GAMUT_P3,
	SBTM_DRDM_GAMUT_2020,
	SBTM_DRDM_GAMUT_MAX
};

enum sbtm_drdm_hgig_sel {
	SBTM_DRDM_HGIG_SEL_0 = 0,
	SBTM_DRDM_HGIG_SEL_1,
	SBTM_DRDM_HGIG_SEL_2,
	SBTM_DRDM_HGIG_SEL_3,
	SBTM_DRDM_HGIG_SEL_4,
	SBTM_DRDM_HGIG_SEL_MAX
};

enum sbtm_grdm_support {
	SBTM_GRDM_OFF = 0,
	SBTM_GRDM_SDR,
	SBTM_GRDM_HDR,
	SBTM_GRDM_MAX
};

enum sbtm_osd_index_e {
	SBTM_OSD1 = 0,
	SBTM_OSD2,
	SBTM_OSD3,
	SBTM_OSD4,
	SBTM_OSD_MAX,
	SBTM_OSD_ERR
};

enum sbtm_hdr_proc_scheme {
	SBTM_PROC_LINER = 0,
	SBTM_PROC_DTMO,
	SBTM_PROC_SPECIFIC,
	SBTM_PROC_DEBUG,
};

enum sbtm_hdr_proc_sel {
	SBTM_OSD_PROC = BIT(0),
	SBTM_SDR_HDR_PROC = BIT(1),
	SBTM_HDR_HDR_PROC = BIT(2),
	SBTM_PROC_MAX = BIT(3),
};

struct sbtmdb_tgt_parm {
	u32 dest_primary[4][2];
	u32 pbpct[5];     /*pct * 10000*/
	u32 pbnits[5];    /*lum*/
	u8 numpb;
	u32 frmpblimit;
	u8 minnits;       /*12bit e val*/
};

struct sbtmem_s {
	u8 sbtm_mdoe;
	u8 sbtm_type;
};

extern uint sbtm_en;
extern uint sbtm_mode;
extern uint cur_sbtm_mode[VD_PATH_MAX];
extern int oo_y_lut_sbtm[HDR2_OOTF_LUT_SIZE];
extern int oo_y_lut_sbtm_osd[HDR2_OOTF_LUT_SIZE];
extern int cgain_sbtm[HDR2_CGAIN_LUT_SIZE];
extern int cgain_sbtm_osd[HDR2_CGAIN_LUT_SIZE];
extern uint sbtm_sdr_hdr_scheme;

unsigned int sbtm_mode_support_get(void);
unsigned int sbtm_mode_get(void);
void sbtm_mode_set(uint mode);
void sbtm_sbtmdb_set(struct vinfo_s *vinfo);
unsigned int sbtmdb_change_get(enum vd_path_e vd_path);
void sbtmdb_change_set(enum vd_path_e vd_path, unsigned int change);
struct sbtm_info *sbtm_sbtmdb_reg_get(void);
struct sbtmdb_tgt_parm *sbtm_sbtmdb_tgt_get(void);
bool sbtm_use_max_rgb(void);
bool sbtm_min_limited(void);

void sbtm_send_sbtmem_pkt(struct vinfo_s *vinfo, int clear);

int register_osd_status_cb(int (*get_osd_enable_status)(u32 index));
void unregister_osd_status_cb(void);

int sbtm_convert_process(enum sbtm_hdr_proc_sel sbtm_proc_sel,
	enum hdr_type_e src_format,
	struct vinfo_s *vinfo,
	struct matrix_s *mtx,
	int mtx_depth);
int sbtm_tmo_dynamic_process(struct vinfo_s *vinfo, enum sbtm_hdr_proc_sel sbtm_proc_sel);
int sbtm_tmo_hdr_hdr_param_dbg(char **parm);
int sbtm_sbtmdb_reg_dbg(char **parm);
void sbtm_tgt_param_update(void);
int sbtm_param_dbg(char **parm);
bool is_sbtm_on(void);
struct aml_tmo_hdr_hdr_reg_sw *tmo_hdr_hdr_reg_get(void);
enum sbtm_hdr_proc_scheme sbtm_sdr_hdr_scheme_get(void);
enum sbtm_hdr_proc_scheme sbtm_hdr_hdr_scheme_get(void);
bool is_sbtm_tmo_dynamic(enum hdr_type_e source_format);
unsigned int user_sbtm_get(void);
void user_sbtm_set(unsigned int enable);
void sbtm_mode_update(void);
#endif
#endif
