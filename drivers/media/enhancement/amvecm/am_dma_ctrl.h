/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifndef AM_DMA_CTRL_H
#define AM_DMA_CTRL_H

#include "set_hdr2_v0.h"

#define DMA_STATE_DISABLE 0
#define DMA_STATE_WAIT_ENABLE 1
#define DMA_STATE_ENABLE 2

enum lut_dma_wr_id_e {
	EN_DMA_WR_ID_LC_STTS_0 = 0,
	EN_DMA_WR_ID_LC_STTS_1,
	EN_DMA_WR_ID_VD1_HDR_HIST = 1,
	EN_DMA_WR_ID_VI_HIST_SPL_0,
	EN_DMA_WR_ID_VD2_HDR_HIST = 2,
	EN_DMA_WR_ID_VI_HIST_SPL_1,
	EN_DMA_WR_ID_CM2_LC = 3,
	EN_DMA_WR_ID_CM2_HIST_0,
	EN_DMA_WR_ID_VD1_1_HDR_HIST = 4,
	EN_DMA_WR_ID_CM2_HIST_1, /*5*/
	EN_DMA_WR_ID_VD2_2_HDR_HIST = 5,
	EN_DMA_WR_ID_VD1_HDR_0,
	EN_DMA_WR_ID_VD1_HDR_1,
	EN_DMA_WR_ID_VD2_HDR,
	EN_DMA_WR_ID_AMBIENT_LIGHT,
	EN_DMA_WR_ID_MAX,
};

enum lut_dma_rd_id_e {
	EN_DMA_RD_ID_GAMUT0 = 0,
	EN_DMA_RD_ID_GAMUT1 = 7,
	EN_DMA_RD_ID_3DLUT = 8,
	EN_DMA_RD_ID_MAX,
};

enum hdr_dma_id_e {
	EN_HDR_DMA_ID_VD1 = 0,
	EN_HDR_DMA_ID_VD2 = 1,
	EN_HDR_DMA_ID_VD1_1 = 3,
	EN_HDR_DMA_ID_VD2_1 = 4,
};

extern int am_dma_ctrl_dbg;
extern unsigned int dma_cm_en;
extern unsigned int dma_lc_en;

void am_dma_init(void);
void am_dma_reset_lc(int enable, int rdma_mode, int vpp_index);
void am_dma_update_mif_wr_cm_lc(enum wr_md_e md);

void am_dma_buffer_malloc(struct platform_device *pdev,
	enum lut_dma_wr_id_e dma_wr_id);
void am_dma_rd_buffer_malloc(struct platform_device *pdev,
	enum lut_dma_rd_id_e dma_rd_id);
void am_dma_buffer_free(struct platform_device *pdev,
	enum lut_dma_wr_id_e dma_wr_id);
void am_dma_rd_buffer_free(struct platform_device *pdev,
	enum lut_dma_rd_id_e dma_rd_id);

void am_dma_set_mif_wr_status(int enable);
void am_dma_set_mif_wr(enum lut_dma_wr_id_e dma_wr_id,
	int enable);
void am_dma_set_mif_rd(enum lut_dma_rd_id_e dma_wr_id,
	int enable);

int am_dma_get_mif_data_lc_stts(int index,
	unsigned int *data, unsigned int length);
int am_dma_get_mif_data_lc_curve(unsigned int *data,
	unsigned int h_num, unsigned int v_num);
void am_dma_get_mif_data_vi_hist(int index,
	unsigned short *data, unsigned int length);
void am_dma_get_mif_data_vi_hist_low(int index,
	unsigned int *data, unsigned int length);
int am_dma_get_mif_data_cm2_hist_hue(int index,
	unsigned int *data, unsigned int length);
int am_dma_get_mif_data_cm2_hist_sat(int index,
	unsigned int *data, unsigned int length);
int am_dma_get_mif_data_hdr2_hist(int index,
	unsigned int *data, unsigned int length);

void am_dma_get_blend_vi_hist(unsigned short *data,
	unsigned int length);
void am_dma_get_blend_vi_hist_low(unsigned int *data,
	unsigned int length);
void am_dma_get_blend_cm2_hist_hue(unsigned int *data,
	unsigned int length);
void am_dma_get_blend_cm2_hist_sat(unsigned int *data,
	unsigned int length);
void am_dma_get_blend_hdr2_hist(unsigned int *data,
	unsigned int length);

void am_dma_lut3d_buffer_malloc(struct platform_device *pdev);
void am_dma_lut3d_buffer_free(struct platform_device *pdev);
void am_dma_lut3d_set_data(int *data, int length);
void am_dma_lut3d_get_data(int *data, int length);
int am_dma_set_mif_data_gamut0(int *eotf, int *oetf, int *ootf);
int am_dma_set_mif_data_gamut1(int *eotf, int *oetf);
int am_dma_set_mif_data_3dlut(int *plut3d);
int check_dma_status(enum lut_dma_wr_id_e dma_wr_id);
void am_dma_updat_hdr2_hist(int mosaic_mode);
int am_dma_3dlut_buffer_init(void);
void am_dma_3dlut_buffer_release(void);
void am_dma_set_dma_mif_rd_en(enum lut_dma_rd_id_e dma_rd_id,
	int enable, int vpp_index);
#endif
