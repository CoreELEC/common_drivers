/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_BL_DRV_H__
#define __AML_BL_DRV_H__
#include <linux/amlogic/media/vout/lcd/aml_bl.h>

static inline unsigned int bl_do_div(unsigned long long num, unsigned int den)
{
	unsigned long long val = num;

	do_div(val, den);

	return (unsigned int)val;
}

char *bl_method_type_to_str(int type);
int bl_config_load(struct aml_bl_drv_s *bdrv, struct platform_device *pdev);

int bl_config_load_post(struct aml_bl_drv_s *bdrv);
int lcd_bl_check_config_load(struct aml_bl_drv_s *drv);

int aml_bl_index_get(int drv_index);
void bl_gpio_probe(struct aml_bl_drv_s *bdrv, int index);
int bl_pwm_init_config_probe(struct bl_data_s *bdata);
enum bl_pwm_port_e bl_pwm_str_to_num(const char *str);
char *bl_pwm_num_to_str(unsigned int num);
int bl_str_to_pwm_method(const char *str, int def_val);
void bl_pwm_ctrl(struct bl_pwm_config_s *bl_pwm, int status);
void bl_pwm_set_duty(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm);
void bl_pwm_set_level(struct aml_bl_drv_s *bdrv, struct bl_pwm_config_s *bl_pwm, int level);
void bl_pwm_config_init(struct bl_pwm_config_s *bl_pwm);
void bl_pwm_curve_init(struct aml_bl_drv_s *bdrv);
int bl_pwm_channel_register(struct device *dev, phandle pwm_phandle,
			    struct bl_pwm_config_s *bl_pwm);

ssize_t bl_pwm_vs_reg_dump(char *buf, ssize_t len, size_t size);

int bl_bri_level_input_scale(struct aml_bl_drv_s *bdrv, int level);
int bl_bri_level_output_scale(struct aml_bl_drv_s *bdrv, int level);
unsigned int bl_pwm_duty_input_scale(struct aml_bl_drv_s *bdrv, unsigned int duty);
unsigned int bl_pwm_duty_output_scale(struct aml_bl_drv_s *bdrv, unsigned int duty);

void bl_pwm_ctrl_status_set(struct aml_bl_drv_s *bdrv, int status);
void bl_power_on(struct aml_bl_drv_s *bdrv);
void bl_power_off(struct aml_bl_drv_s *bdrv);

void aml_bl_set_level(struct aml_bl_drv_s *bdrv, int level);
int bl_gd_level_map(struct aml_bl_drv_s *bdrv, int gd_level);
int bl_gd_sel_func(struct aml_bl_drv_s *bdrv, unsigned int sel);
void bl_gd_diming_func(struct aml_bl_drv_s *bdrv, int level);

int bl_debug_file_creat(struct aml_bl_drv_s *bdrv);
int bl_debug_file_remove(struct aml_bl_drv_s *bdrv);

#endif
