/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef __AML_LCD_COMMON_H__
#define __AML_LCD_COMMON_H__
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/cpu_version.h>
#include <linux/amlogic/media/vout/panel_utils/lcd_resman.h>
// #include "lcd_reg.h"

/* 20250121: initial version*/
/* 20250123: update lcd bootargs transfer by lrm */
/* 20250221: optimize vbyone interrupt handler */
/* 20250304: support lcd_if early on with resume_type control */
/* 20250402: protect dummy state for lcd manual power on */
/* 20250602: lcd multi-device support */
/* 20250616: support sw vrr */
/* 20250902: update ufr clk_change flow */
/* 20250917: tcon support discontinued bin */
/* 20251028: support vrr vdf */
/* 20251113: support ip27 */
/* 20260131: merge tv &tablet */
/* 20260203: optimize t6 phy & dphy init flow */
/* 20260306: sw_vrr protect when vmode_switch */
/* 20260425: update encl dummy clk flow */
#define LCD_DRV_VERSION    "20260425"

#define CTYPE_MASK           0xf0
#define CTYPE_RGB            0x00
#define CTYPE_YUV422         0x10
#define CTYPE_YUV444         0x20
#define CTYPE_YUV420         0x30

#define CFMT_RGB565          0x05
#define CFMT_RGB_6bit        0x06
#define CFMT_RGB_8bit        0x08
#define CFMT_RGB_10bit       0x0a
#define CFMT_RGB_12bit       0x0c
#define CFMT_YCbCr422_8bit   0x18
#define CFMT_YCbCr422_10bit  0x1a
#define CFMT_YCbCr422_12bit  0x1c
#define CFMT_YCbCr444_8bit   0x28
#define CFMT_YCbCr444_10bit  0x2a
#define CFMT_YCbCr444_12bit  0x2c
#define CFMT_YCbCr420_8bit   0x38
#define CFMT_YCbCr420_10bit  0x3a
#define CFMT_YCbCr420_12bit  0x3c
#define CFMT_INVALID         0xff

struct color_fmt_info_s {
	unsigned int cfmt;
	unsigned char bits;
	char name[32];
};

extern struct mutex lcd_vout_mutex;
extern spinlock_t lcd_reg_spinlock;
extern int lcd_vout_serve_bypass;
extern struct mutex lcd_tcon_dbg_mutex;

struct num_str_s {
	int  num;
	char str[32];
};

unsigned int str_add_vmode(char *buf, unsigned char newline,
		unsigned short width, unsigned short height, unsigned short fr);
unsigned int str_parse_vmode(const char *str,
		unsigned short *width, unsigned short *height, unsigned short *fr);
/* lcd common */
int strnum_get_num(const char *str, struct num_str_s *arr, int size_arr, int dft);
void lcd_delay_us(int us);
void lcd_delay_ms(int ms);
void pxp_clk_div_gen(unsigned int pclk, unsigned int *xd, unsigned int *clk_sel);

unsigned char aml_lcd_i2c_bus_get_str(const char *str);
int lcd_type_str_to_type(const char *str);
char *lcd_type_type_to_str(int type);
unsigned char lcd_mode_str_to_mode(const char *str);
char *lcd_mode_mode_to_str(int mode);
void *lcd_alloc_dma_buffer(struct aml_lcd_drv_s *pdrv, unsigned int size, dma_addr_t *paddr);
// u8 *lcd_vmap(ulong addr, u32 size);
void lcd_unmap_phyaddr(u8 *vaddr);
int  lcd_debug_parse_param(char *buf_orig, char **parm, int max_parm);
void lcd_debug_info_print(char *print_buf);
int lcd_detail_timing_print(struct lcd_detail_timing_s *dt, char *buf, int offset, int max_len);
int lcd_phy_cfg_print(struct phy_config_s *cfg, char *buf, int offset, int max_len);
int lcd_phy_attr_print(struct phy_attr_s *phy, u32 lane_num, char *buf, int offset, int max_len);

void lcd_cpu_gpio_probe(struct aml_lcd_drv_s *pdrv, unsigned int index);
void lcd_cpu_gpio_set(struct aml_lcd_drv_s *pdrv, unsigned int index, int value);
unsigned int lcd_cpu_gpio_get(struct aml_lcd_drv_s *pdrv, unsigned int index);
void lcd_pinmux_set(struct aml_lcd_drv_s *pdrv, int status);

void lcd_act_timing_dbg_print(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_config_timing_check(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
				     struct lcd_detail_timing_s *ptiming);
int lcd_base_config_load_from_dts(struct aml_lcd_drv_s *pdrv);
void lcd_mlvds_phy_ckdi_config(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
int lcd_check_config_load(struct aml_lcd_drv_s *drv);
// int lcd_config_load(struct aml_lcd_drv_s *pdrv);

void lcd_config_load_probe(struct aml_lcd_drv_s *pdrv);
void lcd_config_load_remove(struct aml_lcd_drv_s *pdrv);
void lcd_config_load_print(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_device_config_pre_clean(struct aml_lcd_drv_s *pdrv);
void lcd_device_config_post_process(struct aml_lcd_drv_s *pdrv);
void lcd_optical_vinfo_update(struct aml_lcd_drv_s *pdrv);

#ifdef CONFIG_AMLOGIC_LCD_VBYONE
void lcd_vbyone_bit_rate_config(struct aml_lcd_drv_s *pdrv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
void lcd_mlvds_bit_rate_config(struct aml_lcd_drv_s *pdrv);
void lcd_p2p_bit_rate_config(struct aml_lcd_drv_s *pdrv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_LVDS
void lcd_lvds_bit_rate_config(struct aml_lcd_drv_s *pdrv);
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
void lcd_mipi_dsi_bit_rate_config(struct aml_lcd_drv_s *pdrv);
#endif
void lcd_clk_frame_rate_init(struct lcd_detail_timing_s *ptiming);
void lcd_default_to_basic_timing_init_config(struct aml_lcd_drv_s *pdrv,
				struct aml_lcd_device_s *dev_p);
void lcd_enc_timing_init_config(struct aml_lcd_drv_s *pdrv);
void lcd_base_to_act_timing_init_config(struct aml_lcd_drv_s *pdrv);
void lcd_enc_h_timing_change(struct aml_lcd_drv_s *pdrv);

int lcd_fr_is_fixed(struct aml_lcd_drv_s *pdrv);
int lcd_fr_is_frac(struct aml_lcd_drv_s *pdrv, unsigned int frame_rate);
int lcd_vmode_frac_is_support(struct aml_lcd_drv_s *pdrv, unsigned int frame_rate);
void lcd_frame_rate_change(struct aml_lcd_drv_s *pdrv);
void lcd_if_enable_retry(struct aml_lcd_drv_s *pdrv);
void lcd_vout_notify_mode_change_pre(struct aml_lcd_drv_s *pdrv);
void lcd_vout_notify_mode_change(struct aml_lcd_drv_s *pdrv);
void lcd_vinfo_update(struct aml_lcd_drv_s *pdrv);
void lcd_vrr_dev_update(struct aml_lcd_drv_s *pdrv);
void lcd_vrr_dev_register(struct aml_lcd_drv_s *pdrv);
void lcd_vrr_dev_unregister(struct aml_lcd_drv_s *pdrv);
void lcd_fr_lock_init(struct aml_lcd_drv_s *pdrv);
void lcd_fr_lock_deinit(struct aml_lcd_drv_s *pdrv);
void lcd_fr_lock(struct aml_lcd_drv_s *pdrv);
void lcd_sw_vrr_proc(struct aml_lcd_drv_s *pdrv);
void lcd_sw_vrr_disable_reset(struct aml_lcd_drv_s *pdrv);

void lcd_queue_work(struct work_struct *work);
inline void lcd_queue_delayed_work(struct delayed_work *delayed_work, int ms);

int lcd_cus_ctrl_load_from_ini(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
				void *inip, void *psec, unsigned char version);
int lcd_cus_ctrl_load_from_dts(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
				struct device_node *child);

struct lcd_detail_timing_s *lcd_timing_alloc(struct aml_lcd_drv_s *pdrv,
					struct aml_lcd_device_s *dev_p);
void lcd_timing_free_last(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_timing_free_all(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
struct phy_attr_s *lcd_phy_alloc(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_phy_free_last(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_phy_free_all(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
#ifdef CONFIG_AMLOGIC_LCD_TCON
void lcd_tcon_global_reset(struct aml_lcd_drv_s *pdrv);
#endif

/* lcd phy */
unsigned int lcd_phy_vswing_level_to_value(struct aml_lcd_drv_s *pdrv,
			struct aml_lcd_device_s *dev_p, unsigned int level);
unsigned int lcd_phy_preem_level_to_value(struct aml_lcd_drv_s *pdrv,
			struct aml_lcd_device_s *dev_p, unsigned int level);
unsigned int lcd_phy_support_lane_phase(struct aml_lcd_drv_s *pdrv);
int lcd_phy_param_preset(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
int lcd_phy_param_get(struct aml_lcd_drv_s *pdrv, struct phy_config_s *phy_cfg,
		      struct phy_attr_s *phy);

int lcd_phy_param_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_phy_analog_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
void lcd_phy_reset(struct aml_lcd_drv_s *pdrv);
void lcd_phy_set(struct aml_lcd_drv_s *pdrv, int status);
int lcd_phy_probe(struct aml_lcd_drv_s *pdrv);
int lcd_phy_config_init(struct lcd_data_s *pdata);

/* lcd dphy */
void lcd_lane_map_preset(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_lane_map_update(struct aml_lcd_drv_s *pdrv);
void lcd_lane_map_set(struct aml_lcd_drv_s *pdrv);
int lcd_lane_sel_get(struct aml_lcd_drv_s *pdrv, struct phy_config_s *phy_cfg);
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
void lcd_mipi_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
#endif
#ifdef CONFIG_AMLOGIC_LCD_LVDS
void lcd_lvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
void lcd_vbyone_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
void lcd_mlvds_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_p2p_dphy_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
#endif
void lcd_dphy_ctrl_set(struct aml_lcd_drv_s *pdrv, unsigned char on_off);
void lcd_dphy_set_data(struct aml_lcd_drv_s *pdrv, int data);
int lcd_dphy_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);

/* lcd debug */
int lcd_debug_info_len(int num);
int lcd_debug_probe(struct aml_lcd_drv_s *pdrv);
int lcd_debug_init_connector(struct aml_lcd_drv_s *pdrv);
int lcd_debug_remove_basic(struct aml_lcd_drv_s *pdrv);
int lcd_debug_remove_connector(struct aml_lcd_drv_s *pdrv);

/* lcd clk */
extern spinlock_t lcd_clk_lock;
int meson_clk_measure(unsigned int clk_mux);
void lcd_clk_frac_generate(struct aml_lcd_drv_s *pdrv);
void lcd_clk_generate_parameter(struct aml_lcd_drv_s *pdrv);
void lcd_ss_optimize_print(struct aml_lcd_drv_s *pdrv);

int lcd_get_ss(struct aml_lcd_drv_s *pdrv, char *buf);
int lcd_get_ss_num(struct aml_lcd_drv_s *pdrv,
	unsigned int *level, unsigned int *ppm, unsigned int *freq, unsigned int *mode);
int lcd_set_ss(struct aml_lcd_drv_s *pdrv, unsigned int level,
				unsigned int freq, unsigned int mode);
int lcd_encl_clk_msr(struct aml_lcd_drv_s *pdrv);
void lcd_clk_pll_reset(struct aml_lcd_drv_s *pdrv);
void lcd_update_clk_frac(struct aml_lcd_drv_s *pdrv);
void lcd_set_clk(struct aml_lcd_drv_s *pdrv);
void lcd_clk_set_dummy(struct aml_lcd_drv_s *pdrv);
int lcd_clk_dummy_support(struct aml_lcd_drv_s *pdrv);
void lcd_disable_clk(struct aml_lcd_drv_s *pdrv);
void lcd_clk_change(struct aml_lcd_drv_s *pdrv);
int lcd_mlvds_clk_phase_set(struct aml_lcd_drv_s *pdrv);
void lcd_clk_gate_switch(struct aml_lcd_drv_s *pdrv, int status);
int lcd_clk_clkmsr_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_config_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);
int lcd_clk_path_change(struct aml_lcd_drv_s *pdrv, int sel);
void lcd_clk_ss_param_init(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_parameter_init(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_probe(struct aml_lcd_drv_s *pdrv);
void lcd_clk_config_remove(struct aml_lcd_drv_s *pdrv);
int lcd_load_device_config(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p,
			   char *device_cfg_name);
void lcd_clk_init(void);
void aml_lcd_prbs_test(struct aml_lcd_drv_s *pdrv, unsigned int ms, unsigned int mode_flag);

/* lcd venc */
unsigned int lcd_get_encl_line_cnt(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_get_encl_frm_cnt(struct aml_lcd_drv_s *pdrv);
unsigned int lcd_get_max_line_cnt(struct aml_lcd_drv_s *pdrv);
void lcd_wait_vsync(struct aml_lcd_drv_s *pdrv);
int lcd_venc_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset);

void lcd_gamma_debug_test_en(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_debug_test(struct aml_lcd_drv_s *pdrv, unsigned int num);
void lcd_bist_change(struct aml_lcd_drv_s *pdrv, unsigned int level_r,
		     unsigned int level_g, unsigned int level_b);
void lcd_window_attr_change(struct aml_lcd_drv_s *pdrv);
void lcd_debug_cursor(struct aml_lcd_drv_s *pdrv, struct lcd_cursor_attr_s *cursor_attr);
void lcd_set_venc_timing(struct aml_lcd_drv_s *pdrv);
void lcd_set_venc(struct aml_lcd_drv_s *pdrv);
void lcd_set_venc_dummy(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_venc_fr_change(struct aml_lcd_drv_s *pdrv);
void lcd_venc_vrr_recovery(struct aml_lcd_drv_s *pdrv);
void lcd_venc_enable(struct aml_lcd_drv_s *pdrv, int flag);
void lcd_mute_set(struct aml_lcd_drv_s *pdrv, unsigned char flag);
int lcd_mute_state_get(struct aml_lcd_drv_s *pdrv);
int lcd_get_venc_state(struct aml_lcd_drv_s *pdrv);
int lcd_get_venc_init_config(struct aml_lcd_drv_s *pdrv);
int lcd_venc_config_init(struct lcd_data_s *pdata);
void lcd_module_reset(struct aml_lcd_drv_s *pdrv);

void lcd_venc_adj_htotal(struct aml_lcd_drv_s *pdrv, unsigned int htotal);
void lcd_venc_adj_vtotal(struct aml_lcd_drv_s *pdrv, unsigned int vtotal);

void lcd_connector_driver_init_pre(struct aml_lcd_drv_s *pdrv);
void lcd_connector_driver_disable_post(struct aml_lcd_drv_s *pdrv);
void lcd_connector_driver_init(struct aml_lcd_drv_s *pdrv);
void lcd_connector_driver_disable(struct aml_lcd_drv_s *pdrv);
void lcd_connector_config_probe(struct aml_lcd_drv_s *pdrv);
void lcd_connector_config_remove(struct aml_lcd_drv_s *pdrv);

struct aml_lcd_device_s *lcd_device_append_new(struct aml_lcd_drv_s *pdrv, char *device_cfg_name);
void lcd_device_pop_last(struct aml_lcd_drv_s *pdrv);
struct aml_lcd_device_s *lcd_device_assign(struct aml_lcd_drv_s *pdrv, unsigned char dev_idx);
void lcd_device_switch(struct aml_lcd_drv_s *pdrv, unsigned char target_dev_idx);
void lcd_device_list(struct aml_lcd_drv_s *pdrv);

void lcd_mode_driver_dummy_set(struct aml_lcd_drv_s *pdrv, int status);
void lcd_mode_auto_framerate_set(struct aml_lcd_drv_s *pdrv);
int lcd_vmode_remove_all(struct aml_lcd_drv_s *pdrv);
int lcd_mode_timing_set_next(struct aml_lcd_drv_s *pdrv, enum vmode_e vmode);
void lcd_mode_vout_server_init(struct aml_lcd_drv_s *pdrv);
void lcd_mode_vout_server_remove(struct aml_lcd_drv_s *pdrv);
int lcd_mode_init(struct aml_lcd_drv_s *pdrv);
int lcd_mode_remove(struct aml_lcd_drv_s *pdrv);
void lcd_mode_add_device_vmode(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p);
void lcd_mode_dft_timing_update_vinfo(struct aml_lcd_drv_s *pdrv);

/* lcd driver */
void lcd_power_if_early_on(struct aml_lcd_drv_s *pdrv);
void lcd_power_screen_black(struct aml_lcd_drv_s *pdrv);
void lcd_power_screen_restore(struct aml_lcd_drv_s *pdrv);
void lcd_proc_time_clear(struct aml_lcd_drv_s *pdrv);

void lcd_resource_add(struct aml_lcd_drv_s *pdrv, unsigned int res_type, unsigned int res_index);
void lcd_resource_free(struct aml_lcd_drv_s *pdrv, unsigned int res_type, unsigned int res_index);
int lcd_resource_is_ready(struct aml_lcd_drv_s *pdrv);
void lcd_resource_remove_all(struct aml_lcd_drv_s *pdrv);
int get_lcd_config_load_ready(unsigned char index);
int lcd_drm_add(struct device *dev);
void lcd_drm_remove(struct device *dev);
void lcd_late_resume(struct aml_lcd_drv_s *pdrv);

#endif
