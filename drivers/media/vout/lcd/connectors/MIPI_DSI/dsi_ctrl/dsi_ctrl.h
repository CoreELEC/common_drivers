/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef DSI_CTRL_H
#define DSI_CTRL_H

#include "../dsi_common.h"

struct dsi_cmd_req_s {
	u32 data_type;
	u32 vc_id;
	u8 *payload;
	u32 pld_count;
	u32 req_ack;
	u8 rd_data[DSI_RD_MAX];
	u8 rd_out_len;
};

struct dsi_intr_meas_sta_s {
	u8 vsync_meas_count_n;
	u32 vsync_meas_count_msb, vsync_meas_count_lsb;

	u8 edpite_meas_count_n;
	u32 edpite_meas_count_msb, edpite_meas_count_lsb;

	u8 edpihalt_status;
	u16 edpite_line, edpite_pix;

	u32 dsi_top_intr_sta;
	//[8] stat/clr deskewcalhs_pulse_ft interrupt
	//[7] stat/clr deskewcalhs_pulse_rt interrupt
	//[6] stat/clr deskewcalhsdone_pulse_rt interrupt
	//[5] stat/clr of EOF interrupt
	//[4] stat/clr of de_fall interrupt
	//[3] stat/clr of de_rise interrupt
	//[2] stat/clr of vs_fall interrupt
	//[1] stat/clr of vs_rise interrupt
	//[0] stat/clr of dwc_edpite interrupt
};

struct dsi_ctrl_s {
	/* DSI host&phy setup, if (init_on_table) enter op_mode_init*/
	void (*tx_ready)(struct aml_lcd_drv_s *pdrv);
	/* enter op_mode_disp, turn on encl output */
	void (*disp_on)(struct aml_lcd_drv_s *pdrv);

	/* turn off encl output, if (init_off_table) enter op_mode_init */
	void (*disp_off)(struct aml_lcd_drv_s *pdrv);
	/* DSI host&phy reset/powerdown */
	void (*tx_close)(struct aml_lcd_drv_s *pdrv);
	/* reload all para at first boot */
	void (*config_post)(struct aml_lcd_drv_s *pdrv);

	void (*fr_change_pre)(struct aml_lcd_drv_s *pdrv, u16 fr100);
	void (*fr_change_post)(struct aml_lcd_drv_s *pdrv);

	void (*set_dsi_irq_meas)(struct aml_lcd_drv_s *pdrv, u8 port,
			u8 vsync_meas_en, u8 edpite_meas_en, u16 intr_mask_setting);
	void (*dsi_irq_intr_process)(struct aml_lcd_drv_s *pdrv, u8 port,
			struct dsi_intr_meas_sta_s *intr_data);
	void (*dsi_irq_meas_process)(struct aml_lcd_drv_s *pdrv, u8 port,
			struct dsi_intr_meas_sta_s *intr_data);

	/* Processor to Peripheral Direction (Processor-Sourced) Packet Data Types */
	int (*DT_generic_short_write)(struct aml_lcd_drv_s *pdrv, u8 port,
						struct dsi_cmd_req_s *req);
	int (*DT_generic_read)(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
	int (*DT_DCS_short_write)(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
	int (*DT_DCS_read)(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
	int (*DT_set_max_return_pkt_size)(struct aml_lcd_drv_s *pdrv, u8 port,
						struct dsi_cmd_req_s *req);
	int (*DT_generic_long_write)(struct aml_lcd_drv_s *pdrv, u8 port,
						struct dsi_cmd_req_s *req);
	int (*DT_DCS_long_write)(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
	void (*DT_sink_shut_down)(struct aml_lcd_drv_s *pdrv, u8 port);
	void (*DT_sink_turn_on)(struct aml_lcd_drv_s *pdrv, u8 port);

	/*debug purpose*/
	void (*op_mode_switch)(struct aml_lcd_drv_s *pdrv, u8 port, u8 op_mode);
	void (*dphy_reset)(struct aml_lcd_drv_s *pdrv, u8 port);
	void (*host_reset)(struct aml_lcd_drv_s *pdrv, u8 port);
};

void dsi_tx_ready(struct aml_lcd_drv_s *pdrv);
void dsi_disp_on(struct aml_lcd_drv_s *pdrv);
void dsi_disp_off(struct aml_lcd_drv_s *pdrv);
void dsi_tx_close(struct aml_lcd_drv_s *pdrv);
void dsi_config_post(struct aml_lcd_drv_s *pdrv);

void dsi_fr_change_pre(struct aml_lcd_drv_s *pdrv, u16 fr100);
void dsi_fr_change_post(struct aml_lcd_drv_s *pdrv);

void dsi_top_set_irq_meas(struct aml_lcd_drv_s *pdrv, u8 port,
			u8 vsync_meas_en, u8 edpite_meas_en, u16 intr_mask_setting);
void dsi_top_read_intr_sta(struct aml_lcd_drv_s *pdrv, u8 port,
			struct dsi_intr_meas_sta_s *intr_data);
void dsi_top_read_meas_sta(struct aml_lcd_drv_s *pdrv, u8 port,
			struct dsi_intr_meas_sta_s *intr_data);

int dsi_DT_generic_short_write(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
int dsi_DT_generic_read(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
int dsi_DT_DCS_short_write(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
int dsi_DT_DCS_read(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
int dsi_DT_set_max_return_pkt_size(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
int dsi_DT_generic_long_write(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
int dsi_DT_DCS_long_write(struct aml_lcd_drv_s *pdrv, u8 port, struct dsi_cmd_req_s *req);
void dsi_DT_sink_shut_down(struct aml_lcd_drv_s *pdrv, u8 port);
void dsi_DT_sink_turn_on(struct aml_lcd_drv_s *pdrv, u8 port);

void dsi_op_mode_switch(struct aml_lcd_drv_s *pdrv, u8 port, u8 op_mode);
void dsi_dphy_reset(struct aml_lcd_drv_s *pdrv, u8 port);
void dsi_host_reset(struct aml_lcd_drv_s *pdrv, u8 port);

struct dsi_ctrl_s *dsi_bind_v1(struct aml_lcd_drv_s *pdrv);
struct dsi_ctrl_s *dsi_bind_v2(struct aml_lcd_drv_s *pdrv);

#endif
