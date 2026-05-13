/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef _AMLOGIC_eDPTX_EXPORT_H
#define _AMLOGIC_eDPTX_EXPORT_H

#include <linux/amlogic/media/vout/eDPTX/eDPTX.h>
#include <drm/amlogic/meson_connector_dev.h>

// char *lcd_get_dt_addr(void);
// int dptx_probe(void);

// /* global api for cmd */
// unsigned int dptx_driver_outputmode_check(u8 index, char *mode);
// void dptx_driver_vmode_list(void);

// void dptx_driver_probe(void);
// u8 dptx_driver_prepare(u8 index, char *mode);
// u8 dptx_driver_enable(u8 index, char *mode);
// void dptx_driver_disable(u8 index);
// void dptx_driver_info(u8 index);
// void dptx_driver_reg_print(u8 index);
// void dptx_driver_test(u8 index, u8 num);
// void dptx_driver_list_vmode(u8 index);
// void dptx_driver_set_vmode(u8 index, u8 num);

// void dptx_driver_clk_info(u8 index);
// void dptx_driver_debug_print(u8 index, unsigned int val);
// void dptx_driver_reg_info(u8 index);

// void dptx_driver_list_support_mode(void);
// int aml_lcd_edp_debug(int index, char *str, int num);

// int aml_lcd_driver_prbs(int index, unsigned int s, unsigned int mode_flag);

// int aml_lcd_driver_suspend(void *pm_ops);
// int aml_lcd_driver_resume(void *pm_ops);
// int aml_lcd_driver_poweroff(void *pm_ops);
// void aml_lcd_set_poweron_suspend_sta(int state);

struct edptx_drv_s *aml_edptx_get_driver(u8 drv_idx);
void aml_edptx_regist_hpd_cb(struct edptx_drv_s *edp, struct connector_hpd_cb *hpd_cb);
void aml_dptx_aux_xfer(struct edptx_drv_s *edp, u32 addr,
			u8 wr_cnt, u8 wr_buf[16], u8 rd_cnt, u8 rd_buf[16]);
struct edid *aml_edptx_get_raw_edid(struct edptx_drv_s *dptx);
int aml_edptx_get_hpd_state(u8 drv_idx);

#endif
