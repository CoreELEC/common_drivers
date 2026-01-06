/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_VENC_H__
#define __AML_LCD_VENC_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

#define LCD_WAIT_VSYNC_TIMEOUT    50000

#define LCD_DUAL_PORT_L_R        0
#define LCD_DUAL_PORT_R_L        1
#define LCD_DUAL_PORT_O_E        2
#define LCD_DUAL_PORT_E_O        3

struct lcd_venc_op_s {
	int init_flag;
	void (*wait_vsync)(struct aml_lcd_drv_s *pdrv);
	unsigned int (*get_max_lcnt)(struct aml_lcd_drv_s *pdrv);
	void (*gamma_test_en)(struct aml_lcd_drv_s *pdrv, int flag);
	int (*venc_debug_test)(struct aml_lcd_drv_s *pdrv, unsigned int num);
	void (*venc_bist_change)(struct aml_lcd_drv_s *pdrv, unsigned int level_r,
				 unsigned int level_g, unsigned int level_b);
	void (*mute_set)(struct aml_lcd_drv_s *pdrv, unsigned char flag); //vsync latch
	void (*venc_set_timing)(struct aml_lcd_drv_s *pdrv);
	void (*venc_set)(struct aml_lcd_drv_s *pdrv);
	void (*venc_set_dummy)(struct aml_lcd_drv_s *pdrv);
	void (*venc_fr_change)(struct aml_lcd_drv_s *pdrv);
	void (*venc_enable)(struct aml_lcd_drv_s *pdrv, int flag);
	int (*get_venc_state)(struct aml_lcd_drv_s *pdrv);
	int (*get_venc_init_config)(struct aml_lcd_drv_s *pdrv);
	void (*venc_vrr_recovery)(struct aml_lcd_drv_s *pdrv);
	unsigned int (*get_encl_line_cnt)(struct aml_lcd_drv_s *pdrv);
	unsigned int (*get_encl_frm_cnt)(struct aml_lcd_drv_s *pdrv);
	void (*venc_set_htotal)(struct aml_lcd_drv_s *pdrv, unsigned int htotal);
	void (*venc_set_vtotal)(struct aml_lcd_drv_s *pdrv, unsigned int vtotal);
	int (*venc_reg_dump)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
	int (*window_attr_change)(struct aml_lcd_drv_s *pdrv);
	int (*venc_probe_cursor)(struct aml_lcd_drv_s *pdrv, struct lcd_cursor_attr_s *cursor_attr);
};

int lcd_venc_op_init_dft(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op);
int lcd_venc_op_init_t7(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op);
int lcd_venc_op_init_t3x(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op);
int lcd_venc_op_init_t6x(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op);
int lcd_venc_op_init_a9(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op);
int lcd_venc_op_init_vout_model(struct lcd_data_s *pdata, struct lcd_venc_op_s *venc_op);

#endif

