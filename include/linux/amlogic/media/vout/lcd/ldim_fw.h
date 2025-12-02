/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */
#ifndef _INC_AML_LDIM_ALG_H_
#define _INC_AML_LDIM_ALG_H_

#include <linux/uaccess.h>

#define	LDC_T7 0x00
#define	LDC_T3 0x01
#define	LDC_T3X 0x02
#define	LDC_T6W 0x03
#define	LDC_T6X 0x04
#define	LDC_MAX_CHIP 0xff

enum ldc_dbg_type_e {
	LDC_DBG_ATTR		= 0x01,
	LDC_DBG_MEM			= 0x02,
	LDC_DBG_REG			= 0x03,
	LDC_DBG_DBGREG		= 0x04,
	LDC_DBG_DEBUG		= 0x05,
	LDC_DBG_PARA		= 0x06,
};

struct ldim_seg_hist_s {
	unsigned int weight_avg;
	unsigned int weight_avg_95;
	unsigned int max_index;
	unsigned int min_index;
};

struct ldim_ext_hist_s {
	unsigned int *glb_hist;
	unsigned int glb_hist_sum;
	unsigned int glb_hist_cnt;
	unsigned int glb_apl;
};

struct ldim_stts_s {
	unsigned int *global_hist;
	unsigned int global_hist_sum;
	unsigned int global_hist_cnt;
	unsigned int global_apl;
	struct ldim_seg_hist_s *seg_hist;
	struct ldim_ext_hist_s *ext_hist;
};

struct ldim_rmem_s {
	unsigned char flag; //0:none, 1:ldc_cma, 2:sys_cma_pool, 3:kmalloc

	void *rsv_mem_vaddr;
	phys_addr_t rsv_mem_paddr;
	unsigned int rsv_mem_size;
};

struct ldim_fw_config_s {
	unsigned char seg_row;
	unsigned char seg_col;
	unsigned short hsize;
	unsigned short vsize;
	unsigned char func_en;
	unsigned char remap_en;
	unsigned int *boundary_x;
	unsigned int *boundary_y;
};

struct ldim_boost_s {
	unsigned int en;
	unsigned int mode;
	unsigned int i_l100;
	unsigned int i_l32;
	unsigned int i_l100_val;
	unsigned int i_l32_val;
	unsigned int kp_l100;
	unsigned int kp_l32;
	unsigned int kp_cur;
	unsigned int i_cur;
	unsigned int apl;
	unsigned int pre_apl;
	int *iset;
	int *val;
	int *parm;
};

struct ldim_fw_param_s {
	unsigned int para_ver;
	unsigned int para_size;
	unsigned int pq_header;
	unsigned int pq_len;
	char *pq;

	unsigned char fw_sel;
	unsigned char res_update;
	unsigned char vsync_flag;
	unsigned char level;
	unsigned int litgain;

	struct ldim_fw_config_s *conf;
	struct ldim_rmem_s *rmem;
	struct ldim_stts_s *stts;
	struct ldim_boost_s *ext_boost;
	int **pparam;
};

/* fw_ctrl description
 * bit31: pqbypass
 * bit30: get hist
 * bit29: bypass alg
 * bit28: bypass remap bl
 * bit27: resume
 * bit9:  fw_print_en
 * bit8:  ld sel
 */
#define FW_CTRL_PQBYPASS		BIT(31)
#define FW_CTRL_GET_HIST		BIT(30)
#define FW_CTRL_BYPASS_ALG		BIT(29)
#define FW_CTRL_BYPASS_REMAP_BL	BIT(28)
#define FW_CTRL_RESUME			BIT(27)
#define FW_CTRL_RGB				BIT(26)
#define FW_CTRL_GET_GLB_HIST	BIT(10)
#define FW_CTRL_FW_PRINT_EN		BIT(9)
#define FW_CTRL_LD_SEL			BIT(8)

/* fw_cmd cmd description */
#define CMD_PWROFF 0x80000000

struct ldim_fw_s {
	/* header */
	unsigned int para_ver;
	unsigned int para_size;
	char alg_ver[20];
	unsigned char valid;
	unsigned char flag;
	unsigned char chip_type;

	unsigned int fw_ctrl;
	unsigned int fw_state;

	struct ldim_fw_param_s *param;
	unsigned int *bl_matrix;

	void (*fw_alg_frm)(struct ldim_fw_s *fw);
	void (*fw_alg_para_print)(struct ldim_fw_s *fw);
	void (*fw_init)(struct ldim_fw_s *fw);
	void (*fw_info_update)(struct ldim_fw_s *fw);
	void (*fw_pq_set)(char *pq);
	void (*fw_profile_set)(unsigned char *buf, unsigned int len);
	void (*fw_rmem_duty_get)(struct ldim_fw_s *fw);
	void (*fw_rmem_duty_set)(unsigned int *bl_matrix);
	ssize_t (*fw_debug_show)(struct ldim_fw_s *fw,
		enum ldc_dbg_type_e type, char *buf);
	ssize_t (*fw_debug_store)(struct ldim_fw_s *fw,
		enum ldc_dbg_type_e type, char *buf, size_t len);
	int (*fw_cmd)(struct ldim_fw_s *fw, int cmd, void *data);
};

struct ldim_cus_fw_param_s {
	unsigned int para_ver;
	unsigned int para_size;

	int param_len;
	int *param;
	int *cus;
};

struct ldim_fw_custom_s {
	/* header */
	unsigned char valid;/*if ld firmware is ready*/

	unsigned char seg_col;
	unsigned char seg_row;
	unsigned int global_hist_bin_num;
	unsigned char comp_en;//use cus_sw duty for compensation, default 0
	unsigned char pq_update;//when modify pqdata, set pq_update = 1;
	char *pqdata;

	unsigned int fw_print_frequent;/*debug print frequent, count of frames*/
	unsigned int fw_print_lv;/*debug print level, range at 200 - 300*/

	unsigned int fw_ctrl;
	unsigned int fw_state;
	struct ldim_cus_fw_param_s *fw_param; /*custom fw parameters*/

	unsigned int *bl_matrix;/*backlight matrix output*/

	/*function for backlight matrix algorithm*/
	void (*fw_alg_frm)(struct ldim_fw_custom_s *fw_cus);
	void (*fw_alg_para_print)(struct ldim_fw_custom_s *fw_cus);
};

/* if struct ldim_fw_s changed, FW_PARA_VER must be update */
/*20221118 version 1*/
/*20230915 version 2 modify interface for set pq*/
/*20230915 version 3 add in_param, out_param*/
/*20250305 version 4 add boost*/
/*20250305 version 5 add ext_hist, mot...*/
#define FW_PARA_VER    5

struct ldim_fw_s *aml_ldim_get_fw(void);
struct ldim_fw_custom_s *aml_ldim_get_fw_cus(void);

#endif
