/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _LCD_TX_CONNECTOR_
#define _LCD_TX_CONNECTOR_

#include <linux/amlogic/media/vout/lcd/lcd_vout.h>

/* MIPI-DPI */
#ifdef CONFIG_AMLOGIC_LCD_DPI
void lcd_rgb_control_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_bt_control_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
#endif

/* lvds */
#ifdef CONFIG_AMLOGIC_LCD_LVDS
void lcd_lvds_enable(struct aml_lcd_drv_s *pdrv);
void lcd_lvds_disable(struct aml_lcd_drv_s *pdrv);
#endif

#ifdef CONFIG_AMLOGIC_LCD_VBYONE
/* vbyone */
void lcd_vbyone_enable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_disable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_wait_timing_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_cdr_training_hold(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_vbyone_wait_hpd(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_power_on_wait_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_wait_stable(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_interrupt_enable(struct aml_lcd_drv_s *pdrv, int flag);
int lcd_vbyone_interrupt_up(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_interrupt_down(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_cdr(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_lock(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_debug_reset(struct aml_lcd_drv_s *pdrv);
void lcd_vbyone_probe(struct aml_lcd_drv_s *pdrv);
int lcd_vbyone_vsync_handler(struct aml_lcd_drv_s *pdrv);
#endif

#ifdef CONFIG_AMLOGIC_LCD_TCON
/* tcon  */
#include "./tcon/lcd_tcon_swpdf.h"

unsigned int lcd_tcon_reg_read(struct aml_lcd_drv_s *pdrv, unsigned int addr);
void lcd_tcon_reg_write(struct aml_lcd_drv_s *pdrv,
			unsigned int addr, unsigned int val);
int lcd_tcon_probe(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_remove(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_tcon_table_read(unsigned int addr);
unsigned int lcd_tcon_table_write(unsigned int addr, unsigned int val);
int lcd_tcon_core_update(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_core_reg_get(struct aml_lcd_drv_s *pdrv,
			  unsigned char *buf, unsigned int size);
int lcd_tcon_top_init(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_enable(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_reload(struct aml_lcd_drv_s *pdrv);
int lcd_tcon_reload_pre(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_disable(struct aml_lcd_drv_s *pdrv);
void lcd_tcon_dbg_check(struct aml_lcd_drv_s *pdrv, struct lcd_detail_timing_s *ptiming);
void lcd_tcon_vsync_isr(struct aml_lcd_drv_s *pdrv);

void lcd_tcon_fw_base_timing_update(struct aml_lcd_drv_s *pdrv);

/* tcon debug */
int lcd_tcon_info_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
ssize_t lcd_tcon_debug_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_status_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_reg_debug_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_reg_debug_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_fw_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_fw_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_pdf_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_pdf_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_rdma_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_rdma_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
ssize_t lcd_tcon_info_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_demura_dbg_show(struct device *dev, struct device_attribute *attr, char *buf);
ssize_t lcd_tcon_demura_dbg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count);
long lcd_tcon_ioctl_handler(struct aml_lcd_drv_s *pdrv, int mcd_nr, unsigned long arg);
unsigned int lcd_tcon_get_vfp_tail(struct aml_lcd_drv_s *pdrv);
#endif

#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
/* MIPI-DSI */
/* @lcd_common.c */
void lcd_mipi_dsi_init_table_load_json(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
					struct json_parse_s *jsp, struct json_s *parent);
void lcd_mipi_dsi_init_table_load_dts(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
					struct device_node *child);
void lcd_mipi_dsi_init_table_free(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_dsi_tx_ctrl(struct aml_lcd_drv_s *pdrv, unsigned char en);
unsigned long long lcd_dsi_get_min_bitrate(struct aml_lcd_drv_s *pdrv);
/* @lcd_debug.c */
void lcd_dsi_info_print(struct lcd_config_s *pconf);
void lcd_dsi_post_config_load(struct aml_lcd_drv_s *pdrv);
void lcd_dsi_if_bind(struct aml_lcd_drv_s *pdrv);
void lcd_dsi_set_operation_mode(struct aml_lcd_drv_s *pdrv, u8 op_mode);
void lcd_dsi_dphy_test(struct aml_lcd_drv_s *pdrv, u8 test_item);
void lcd_dsi_write_cmd(struct aml_lcd_drv_s *pdrv, u8 *payload);
unsigned char lcd_dsi_read(struct aml_lcd_drv_s *pdrv, u8 *payload, u8 *rd_data, u8 rd_byte_len);
/* @lcd_addons/dsi_check_panel.c */
int mipi_dsi_check_state(struct aml_lcd_drv_s *pdrv, u8 reg, u8 cnt);
int lcd_dsi_interrupt_ctrl(struct aml_lcd_drv_s *pdrv, u8 en);
#endif

#endif
