/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_PHY_CONFIG_H__
#define __AML_LCD_PHY_CONFIG_H__
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

extern unsigned short lvds_lane_map_flag_6lane_map0[2][3];
// extern lvds_lane_map_flag_5lane = lvds_lane_map_flag_6lane_map1
extern unsigned short lvds_lane_map_flag_6lane_map1[2][3];
extern unsigned short lvds_lane_map_flag_8lane[2][3];

struct lcd_phy_ctrl_s {
	unsigned int lane_num;
	unsigned int lane_lock_total;
	unsigned int lane_lock[LCD_MAX_DRV];

	// pre device op, need assign device
	void (*phy_glb_param_dft_val)
		(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
	unsigned int (*phy_vswing_level_to_val)
		(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p, unsigned int level);
	unsigned int (*phy_preem_level_to_val)
		(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p, unsigned int level);
	unsigned int (*phy_amp_dft_val)
		(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
	unsigned char (*phy_lane_phase_sel_def)
		(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p, unsigned int lane);
	unsigned char (*phy_lane_pn_swap_dft)
		(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p, unsigned int lane);
	unsigned int (*phy_rterm_dft_val)
		(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);

	int (*phy_param_get)(struct aml_lcd_drv_s *pdrv,
			     struct phy_config_s *phy_cfg, struct phy_attr_s *phy);
	int (*phy_reg_dump)(struct aml_lcd_drv_s *pdrv, char *buf, int offset);

	void (*phy_reset)(struct aml_lcd_drv_s *pdrv);

	void (*phy_set_lvds)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_vx1)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_mlvds)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_p2p)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_mipi)(struct aml_lcd_drv_s *pdrv, int status);
	void (*phy_set_edp)(struct aml_lcd_drv_s *pdrv, int status);
};

struct lcd_phy_ctrl_s *lcd_phy_config_init_c3(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_g12(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t3_t5m(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t5d(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t5w(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t7(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_tm2(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t3x(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_txhd2(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_s6(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t6d(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t6w(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_t6x(struct lcd_data_s *pdata);
struct lcd_phy_ctrl_s *lcd_phy_config_init_a9(struct lcd_data_s *pdata);
unsigned int lcd_phy_vswing_level_to_value_dft(struct aml_lcd_drv_s *pdrv,
			struct aml_lcd_device_s *dev_p, unsigned int level);
unsigned int lcd_phy_preem_level_to_value_dft(struct aml_lcd_drv_s *pdrv,
			struct aml_lcd_device_s *dev_p, unsigned int level);
unsigned int lcd_phy_amp_dft(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_phy_glb_param_dft(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);

#endif
