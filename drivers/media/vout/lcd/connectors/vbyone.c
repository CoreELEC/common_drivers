// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/of.h>
#include <linux/reset.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include <linux/amlogic/media/vout/lcd/lcd_notify.h>
#include "../lcd_reg.h"
#include "../lcd_common.h"
#include "./lcd_connector.h"

static spinlock_t vx1_intr_lock;

struct vbyone_ctrl_regs_s vbo_reg_t7 = {
	.reg_status = VBO_STATUS_L_T7,
	.hpd_bit = 6,
	.reg_insig_ctrl = VBO_INSGN_CTRL_T7,
	.reg_filter_l = VBO_INFILTER_CTRL_T7,
	.reg_filter_h = VBO_INFILTER_CTRL_H_T7,
	.reg_holder_l = VBO_FSM_HOLDER_L_T7,
	.reg_holder_h = VBO_FSM_HOLDER_H_T7,
	.reg_ctrl_l = VBO_CTRL_L_T7,
	.reg_intr_ctrl = VBO_INTR_STATE_CTRL_T7,
	.reg_intr_state = VBO_INTR_STATE_T7,
	.reg_intr_unmask = VBO_INTR_UNMASK_T7,
};

struct vbyone_ctrl_regs_s vbo_reg_t3x = {
	.reg_status = VBO_STATUS_L_T3X,
	.hpd_bit = 16,
	.reg_insig_ctrl = VBO_INSGN_CTRL_T3X,
	.reg_filter_l = VBO_INFILTER_CTRL_T3X,
	.reg_filter_h = VBO_INFILTER_CTRL_H_T3X,
	.reg_holder_l = VBO_FSM_HOLDER_T3X,
	.reg_holder_h = VBO_FSM_HOLDER_T3X,
	.reg_ctrl_l = VBO_CTRL_T3X,
	.reg_intr_ctrl = VBO_INTR_STATE_CTRL_T3X,
	.reg_intr_state = VBO_INTR_STATE_T3X,
	.reg_intr_unmask = VBO_INTR_UNMASK_T3X,
};

struct vbyone_ctrl_regs_s vbo_reg_dft = {
	.reg_status = VBO_STATUS_L,
	.reg_insig_ctrl = VBO_INSGN_CTRL,
	.reg_filter_l = VBO_INFILTER_TICK_PERIOD_L,
	.reg_filter_h = VBO_INFILTER_TICK_PERIOD_H,
	.reg_holder_l = VBO_FSM_HOLDER_L,
	.reg_holder_h = VBO_FSM_HOLDER_H,
	.reg_ctrl_l = VBO_CTRL_L,
	.reg_intr_ctrl = VBO_INTR_STATE_CTRL,
	.reg_intr_state = VBO_INTR_STATE,
	.reg_intr_unmask = VBO_INTR_UNMASK,
};

struct vbyone_ctrl_regs_s *vx1_reg = &vbo_reg_t7;

#define VX1_LOCKN_INTERVAL        20    //unit:us
#define VX1_LOCKN_WAIT_TIMEOUT    20000 /* 20000*50us=1000ms */
#define VX1_LOCKN_STABLE_CNT      100
#define VX1_LOCKN_CONFIRM_DELAY   100 //us
#define VX1_LOCKN_CONFIRM_CNT     5
#define VX1_HPD_WAIT_TIMEOUT      500  /* 500*1ms=500ms */

#define VX1_TRAINING_TIMEOUT      60  /* vsync cnt */
#define VSYNC_CNT_VX1_RESET       5
#define VSYNC_CNT_VX1_STABLE      20
#define VSYNC_CNT_VX1_RETRY       60

#define VX1_MNT_INTERVAL          20  //unit:ms

#define VX1_FSM_ACQ_NEXT_STEP_CONTINUE     0
#define VX1_FSM_ACQ_NEXT_RELEASE_HOLDER    1
#define VX1_FSM_ACQ_NEXT                   VX1_FSM_ACQ_NEXT_STEP_CONTINUE
/* enable htpdn_fail,lockn_fail,acq_hold */
#define VBYONE_INTR_UNMASK        0x2bff /* 0x2a00 */

static unsigned int lcd_vbyone_get_fsm_state(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	unsigned int state;

	state = lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x3f;
	return state;
}

static void lcd_vbyone_force_cdr(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	LCD_DBG(pdrv, "%s", __func__);

	lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 7, 0, 4);
}

static void lcd_vbyone_force_lock(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	LCD_DBG(pdrv, "%s", __func__);

	lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 7, 0, 4);
	lcd_delay_ms(100);
	lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 5, 0, 4);
}

void lcd_vbyone_sw_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int reg_phy_tx_ctrl0, reg_phy_tx_ctrl1, reg_rst, offset;

	LCD_DBG(pdrv, "%s", __func__);

	if (pdrv->data->chip_type == LCD_CHIP_T7) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			break;
		default:
			LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T7 + offset;

		/* force PHY to 0 */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 8, 2);
		lcd_vcbus_write(reg_rst, 0x1ff);
		udelay(5);
		/* release PHY */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 2);
		lcd_vcbus_write(reg_rst, 0);
	} else if (pdrv->data->chip_type == LCD_CHIP_T3X) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY0_CNTL0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = COMBO_DPHY_EDP_LVDS_TX_PHY1_CNTL0;
			break;
		default:
			LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T3X + offset;

		/* force PHY to 0 */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 3, 8, 2);
		lcd_vcbus_write(reg_rst, 0x3);
		udelay(5);
		/* release PHY */
		lcd_combo_dphy_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 2);
		lcd_vcbus_write(reg_rst, 0);
	} else if (pdrv->data->chip_type == LCD_CHIP_T6X) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL1;
			break;
		default:
			LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T3X + offset;

		/* force PHY to 0 */
		lcd_vx1_lvds_ctrl_setb(pdrv, reg_phy_tx_ctrl0, 3, 8, 2);
		lcd_vcbus_write(reg_rst, 0x3);
		udelay(5);
		/* release PHY */
		lcd_vx1_lvds_ctrl_setb(pdrv, reg_phy_tx_ctrl0, 0, 8, 2);
		lcd_vcbus_write(reg_rst, 0);
	} else if (pdrv->data->chip_type == LCD_CHIP_T5W ||
		   pdrv->data->chip_type == LCD_CHIP_T3 ||
		   pdrv->data->chip_type == LCD_CHIP_T5M) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL0;
			break;
		case 1:
			reg_phy_tx_ctrl0 = ANACTRL_LVDS_TX_PHY_CNTL2;
			break;
		default:
			LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T7 + offset;

		/* force PHY to 0 */
		lcd_ana_setb(reg_phy_tx_ctrl0, 3, 8, 2);
		lcd_vcbus_write(reg_rst, 0x1ff);
		udelay(5);
		/* release PHY */
		lcd_ana_setb(reg_phy_tx_ctrl0, 0, 8, 2);
		lcd_vcbus_write(reg_rst, 0);
	} else if (pdrv->data->chip_type == LCD_CHIP_T6W) {
		switch (pdrv->index) {
		case 0:
			reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL1;
			break;
		case 1:
			reg_phy_tx_ctrl1 = ANACTRL_LVDS_TX_PHY_CNTL3;
			break;
		default:
			LCD_ERR(pdrv, "%s: invalid drv_index", __func__);
			return;
		}
		offset = pdrv->data->offset_venc_if[pdrv->index];
		reg_rst = VBO_SOFT_RST_T7 + offset;

		/* force PHY to 0 */
		lcd_vx1_lvds_ctrl_write(pdrv, reg_phy_tx_ctrl1, 0);
		lcd_vcbus_write(reg_rst, 0x1ff);
		udelay(5);
		/* release PHY */
		lcd_vx1_lvds_ctrl_write(pdrv, reg_phy_tx_ctrl1, 0x3000000);
		lcd_vcbus_write(reg_rst, 0);
	} else if (pdrv->data->chip_type == LCD_CHIP_A9) {
		/* force PHY to 0 */
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL0_A9, 0, 0, 1);
		lcd_vcbus_write(VBO_SOFT_RST_T7, 0x1ff);
		udelay(5);
		/* release PHY */
		lcd_vx1_lvds_ctrl_setb(pdrv, ANACTRL_LVDS_TX_PHY_CNTL0_A9, 1, 0, 1);
		lcd_vcbus_write(VBO_SOFT_RST_T7, 0);

	} else {
		/* force PHY to 0 */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 3, 8, 2);
		lcd_vcbus_write(VBO_SOFT_RST, 0x1ff);
		udelay(5);
		/* release PHY */
		lcd_ana_setb(HHI_LVDS_TX_PHY_CNTL0, 0, 8, 2);
		lcd_vcbus_write(VBO_SOFT_RST, 0);
	}
}

static void lcd_vbyone_hw_filter(struct aml_lcd_drv_s *pdrv, int flag)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	unsigned int temp, period;
	unsigned int tick_period[] = {
		0xfff,
		0xff,    /* 1: 0.8us */
		0x1ff,   /* 2: 1.7us */
		0x3ff,   /* 3: 3.4us */
		0x7ff,   /* 4: 6.9us */
		0xfff,   /* 5: 13.8us */
		0x1fff,  /* 6: 27us */
		0x3fff,  /* 7: 55us */
		0x7fff,  /* 8: 110us */
		0xffff,  /* 9: 221us */
		0x1ffff, /* 10: 441us */
		0x3ffff, /* 11: 883us */
		0x7ffff, /* 12: 1.76ms */
		0xfffff, /* 13: 3.53ms */
	};

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;
	if (flag) {
		period = vx1_conf->hw_filter_time & 0xff;
		if (period >=
			(sizeof(tick_period) / sizeof(unsigned int)))
			period = tick_period[0];
		else
			period = tick_period[period];
		temp = period & 0xffff;
		lcd_vcbus_write(vx1_reg->reg_filter_l + offset, temp);
		temp = (period >> 16) & 0xf;
		lcd_vcbus_write(vx1_reg->reg_filter_h + offset, temp);
		/* hpd */
		temp = vx1_conf->hw_filter_cnt & 0xff;
		if (temp == 0xff) {
			lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 0, 8, 4);
		} else {
			temp = (temp == 0) ? 0x7 : temp;
			lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, temp, 8, 4);
		}
		/* lockn */
		temp = (vx1_conf->hw_filter_cnt >> 8) & 0xff;
		if (temp == 0xff) {
			lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 0, 12, 4);
		} else {
			temp = (temp == 0) ? 0x7 : temp;
			lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, temp, 12, 4);
		}
	} else {
		temp = (vx1_conf->hw_filter_time >> 8) & 0x1;
		if (temp) {
			lcd_vcbus_write(vx1_reg->reg_filter_l + offset, 0xff);
			lcd_vcbus_write(vx1_reg->reg_filter_h + offset, 0x0);
			lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 0, 8, 4);
			lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 0, 12, 4);
			LCD_PR(pdrv, "%s: %d disable for debug", __func__, flag);
		}
	}
}

static int lcd_vbyone_check(int lane_num, int region, int slice, int byte_mode)
{
	if (lane_num <= 0 || (lane_num & (lane_num - 1)) || lane_num > 8) //must 1,2,4,8
		return -1;
	if (region <= 0 || (region & (region - 1))) //1,2,4,8
		return -1;
	if ((slice == 1 && region > 4) || (slice == 2 && region > 8))
		return -1;
	if (lane_num % region)
		return -1;
	if (byte_mode != 3 && byte_mode != 4)
		return -1;
	return 0;
}

static int lcd_vbyone_lanes_set_dft(struct aml_lcd_drv_s *pdrv, int lane_num,
				    int byte_mode, int region_num,
				    int hsize, int vsize)
{
	int sublane_num;
	int region_size[4];
	int tmp;

	if (lcd_vbyone_check(lane_num, region_num, 0, byte_mode))
		return -1;
	LCD_DBG(pdrv, "byte_mode=%d, lane_num=%d, region_num=%d", byte_mode, lane_num, region_num);

	sublane_num = lane_num / region_num; /* lane num in each region */
	lcd_vcbus_setb(VBO_LANES, (lane_num - 1), 0, 3);
	lcd_vcbus_setb(VBO_LANES, (region_num - 1), 4, 2);
	lcd_vcbus_setb(VBO_LANES, (sublane_num - 1), 8, 3);
	lcd_vcbus_setb(VBO_LANES, (byte_mode - 1), 11, 2);

	if (region_num > 1) {
		region_size[3] = (hsize / lane_num) * sublane_num;
		tmp = (hsize % lane_num);
		region_size[0] = region_size[3] + (((tmp / sublane_num) > 0) ?
			sublane_num : (tmp % sublane_num));
		region_size[1] = region_size[3] + (((tmp / sublane_num) > 1) ?
			sublane_num : (tmp % sublane_num));
		region_size[2] = region_size[3] + (((tmp / sublane_num) > 2) ?
			sublane_num : (tmp % sublane_num));
		lcd_vcbus_write(VBO_REGION_00, region_size[0]);
		lcd_vcbus_write(VBO_REGION_01, region_size[1]);
		lcd_vcbus_write(VBO_REGION_02, region_size[2]);
		lcd_vcbus_write(VBO_REGION_03, region_size[3]);
	}
	lcd_vcbus_write(VBO_ACT_VSIZE, vsize);
	/* different from FBC code!!! */
	/* lcd_vcbus_setb(VBO_CTRL_H,0x80,11,5); */
	/* different from simulation code!!! */
	lcd_vcbus_setb(VBO_CTRL_H, 0x0, 0, 4);
	lcd_vcbus_setb(VBO_CTRL_H, 0x1, 9, 1);
	/* lcd_vcbus_setb(VBO_CTRL_L,enable,0,1); */

	return 0;
}

static int lcd_vbyone_lanes_set_t7(struct aml_lcd_drv_s *pdrv, int lane_num,
				   int byte_mode, int region_num,
				   int hsize, int vsize)
{
	unsigned int offset;
	int sublane_num;
	int region_size[4];
	int tmp;

	if (lcd_vbyone_check(lane_num, region_num, 0, byte_mode))
		return -1;
	LCD_DBG(pdrv, "byte_mode=%d, lane_num=%d, region_num=%d", byte_mode, lane_num, region_num);

	offset = pdrv->data->offset_venc_if[pdrv->index];
	sublane_num = lane_num / region_num; /* lane num in each region */
	lcd_vcbus_setb(VBO_LANES_T7 + offset, (lane_num - 1), 0, 3);
	lcd_vcbus_setb(VBO_LANES_T7 + offset, (region_num - 1), 4, 2);
	lcd_vcbus_setb(VBO_LANES_T7 + offset, (sublane_num - 1), 8, 3);
	lcd_vcbus_setb(VBO_LANES_T7 + offset, (byte_mode - 1), 11, 2);

	if (region_num > 1) {
		region_size[3] = (hsize / lane_num) * sublane_num;
		tmp = (hsize % lane_num);
		region_size[0] = region_size[3] + (((tmp / sublane_num) > 0) ?
			sublane_num : (tmp % sublane_num));
		region_size[1] = region_size[3] + (((tmp / sublane_num) > 1) ?
			sublane_num : (tmp % sublane_num));
		region_size[2] = region_size[3] + (((tmp / sublane_num) > 2) ?
			sublane_num : (tmp % sublane_num));
		lcd_vcbus_write(VBO_REGION_00_T7 + offset, region_size[0]);
		lcd_vcbus_write(VBO_REGION_01_T7 + offset, region_size[1]);
		lcd_vcbus_write(VBO_REGION_02_T7 + offset, region_size[2]);
		lcd_vcbus_write(VBO_REGION_03_T7 + offset, region_size[3]);
	}
	lcd_vcbus_write(VBO_ACT_VSIZE_T7 + offset, vsize);
	/* different from FBC code!!! */
	/* lcd_vcbus_setb(VBO_CTRL_H_T7 + offset,0x80,11,5); */
	/* different from simulation code!!! */
	lcd_vcbus_setb(VBO_CTRL_H_T7 + offset, 0x0, 0, 4);
	lcd_vcbus_setb(VBO_CTRL_H_T7 + offset, 0x1, 9, 1);
	/* lcd_vcbus_setb(VBO_CTRL_L_T7 + offset,enable,0,1); */

	return 0;
}

static int lcd_vbyone_lanes_set_t3x(struct aml_lcd_drv_s *pdrv, unsigned int offset, int lane_num,
			int byte_mode, int region_num, int slice, int ppc, int hsize, int vsize)
{
	int sublane_num, orgn_sub, orgns_num, slice_lane_num;
	unsigned int p2s_mode, pre_hact;

	if (slice == 0)
		slice = 1;
	if (region_num == 0)
		return -1;

	slice_lane_num = lane_num / slice;
	if (lcd_vbyone_check(slice_lane_num, region_num, slice, byte_mode))
		return -1;

	LCD_DBG(pdrv, "byte_mode=%d, lane_num=%d, region_num=%d slice=%d",
		byte_mode, lane_num, region_num, slice);

	sublane_num = slice_lane_num * slice / region_num; /* lane num in each region */
	lcd_vcbus_setb(VBO_LANES_T3X + offset, (slice_lane_num - 1), 0, 3);
	lcd_vcbus_setb(VBO_LANES_T3X + offset, (1 << slice) - 1, 4, 4);// group en
	lcd_vcbus_setb(VBO_LANES_T3X + offset, byte_mode - 1, 11, 2);
	lcd_vcbus_write(VBO_ACT_VSIZE_T3X + offset, vsize);

	orgn_sub = slice_lane_num == 8 ? (sublane_num == 4 ? 3 : 2) :
			   slice_lane_num == 4 ? (sublane_num == 1 ? 1 : 2) :
			   slice_lane_num == 2 ? (sublane_num == 1 ? 0 : 2) : 2;

	orgns_num = region_num == 1 ? 0 :
				 region_num == 2 ? 1 :
				 region_num == 4 ? 2 :
				 region_num == 8 ? 3 : 0;

	lcd_vcbus_setb(VBO_RGN_CTRL_T3X + offset, slice >> 1, 0, 2);
	lcd_vcbus_setb(VBO_RGN_CTRL_T3X + offset, orgns_num, 4, 3);//output region number
	lcd_vcbus_setb(VBO_RGN_CTRL_T3X + offset, orgn_sub, 8, 2);
	lcd_vcbus_setb(VBO_RGN_CTRL_T3X + offset, slice_lane_num == 4, 10, 1);
	lcd_vcbus_setb(VBO_RGN_CTRL_T3X + offset, 0, 16, 8);//hblank for read line buf

	pre_hact = hsize / slice * 4 / 5;
	//input pixels for each slice
	lcd_vcbus_setb(VBO_RGN_HSIZE_T3X + offset, hsize / slice, 0, 12);
	//first line pre-read pixels
	lcd_vcbus_setb(VBO_RGN_HSIZE_T3X + offset, pre_hact, 16, 12);

	p2s_mode = ppc == 2 && slice == 2 ? 0 :
			   ppc == 2 && slice == 1 ? 1 : 2;
	lcd_vcbus_write(VBO_SLICE_CTRL_T3X + offset, 0x0);
	lcd_vcbus_setb(VBO_SLICE_CTRL_T3X + offset, hsize / ppc, 0, 14);//slice hsize
	lcd_vcbus_setb(VBO_SLICE_CTRL_T3X + offset, p2s_mode, 14, 2);//ppc to slice

	lcd_vcbus_setb(VBO_CTRL_T3X + offset, 2, 16, 4);
	lcd_vcbus_setb(VBO_CTRL_T3X + offset, 1, 0, 1);//enable

	return 0;
}

static void lcd_vbyone_enable_dft(struct aml_lcd_drv_s *pdrv)
{
	int lane_count, byte_mode, region_num, hsize, vsize;
	int hsync_pol, vsync_pol, vin_color, vin_bpp;
	/* int color_fmt; */

	hsize = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
	vsize = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
	lane_count = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.lane_count; /* 8 */
	region_num = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.region_num; /* 2 */
	byte_mode = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.byte_mode; /* 4 */
	/* color_fmt = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.color_fmt; // 4 */

	vin_color = 4; /* fixed RGB */
	switch (pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits) {
	case 18:
		vin_bpp = 2; /* 18bbp 4:4:4 */
		break;
	case 24:
		vin_bpp = 1; /* 24bbp 4:4:4 */
		break;
	case 30:
	default:
		vin_bpp = 0; /* 30bbp 4:4:4 */
		break;
	}

	/* set Vbyone vin color format */
	lcd_vcbus_setb(VBO_VIN_CTRL, vin_color, 8, 3);
	lcd_vcbus_setb(VBO_VIN_CTRL, vin_bpp, 11, 2);

	lcd_vbyone_lanes_set_dft(pdrv, lane_count, byte_mode, region_num, hsize, vsize);
	/*set hsync/vsync polarity to let the polarity is low active inside the VbyOne */
	hsync_pol = 0;
	vsync_pol = 0;
	lcd_vcbus_setb(VBO_VIN_CTRL, hsync_pol, 4, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL, vsync_pol, 5, 1);

	lcd_vcbus_setb(VBO_VIN_CTRL, hsync_pol, 6, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL, vsync_pol, 7, 1);

	/* below line copy from simulation */
	/* gate the input when vsync asserted */
	lcd_vcbus_setb(VBO_VIN_CTRL, 1, 0, 2);
	/* lcd_vcbus_write(VBO_VBK_CTRL_0,0x13);
	 * lcd_vcbus_write(VBO_VBK_CTRL_1,0x56);
	 * lcd_vcbus_write(VBO_HBK_CTRL,0x3478);
	 * lcd_vcbus_setb(VBO_PXL_CTRL,0x2,0,4);
	 * lcd_vcbus_setb(VBO_PXL_CTRL,0x3,VBO_PXL_CTR1_BIT,VBO_PXL_CTR1_WID);
	 * set_vbyone_ctlbits(1,0,0);
	 */
	/* VBO_RGN_GEN clk always on */
	lcd_vcbus_setb(VBO_GCLK_MAIN, 2, 2, 2);

	lcd_vcbus_setb(LCD_PORT_SWAP, 0, 9, 2);
	/* lcd_vcbus_setb(LCD_PORT_SWAP, 1, 8, 1);//reverse lane output order */

	lcd_vbyone_hw_filter(pdrv, 1);
	lcd_vcbus_setb(VBO_INSGN_CTRL, 0, 2, 2);
	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vcbus_setb(VBO_CTRL_L, 1, 0, 1);

	lcd_vbyone_wait_timing_stable(pdrv);
	lcd_vbyone_sw_reset(pdrv);

	/* training hold */
	if (pdrv->curr_dev->dev_cfg.control.vbyone_cfg.ctrl_flag & 0x4)
		lcd_vbyone_cdr_training_hold(pdrv, 1);
}

static void lcd_vbyone_enable_t7(struct aml_lcd_drv_s *pdrv)
{
	int lane_count, byte_mode, region_num, hsize, vsize;
	int hsync_pol, vsync_pol, vin_color, vin_bpp;
	/* int color_fmt; */
	unsigned int offset;

	offset = pdrv->data->offset_venc_if[pdrv->index];

	hsize = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
	vsize = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
	lane_count = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.lane_count; /* 8 */
	region_num = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.region_num; /* 2 */
	byte_mode = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.byte_mode; /* 4 */
	/* color_fmt = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.color_fmt; // 4 */

	vin_color = 4; /* fixed RGB */
	switch (pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits) {
	case 18:
		vin_bpp = 2; /* 18bbp 4:4:4 */
		break;
	case 24:
		vin_bpp = 1; /* 24bbp 4:4:4 */
		break;
	case 30:
	default:
		vin_bpp = 0; /* 30bbp 4:4:4 */
		break;
	}

	/* set Vbyone vin color format */
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vin_color, 8, 3);
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vin_bpp, 11, 2);

	lcd_vbyone_lanes_set_t7(pdrv, lane_count, byte_mode, region_num, hsize, vsize);
	/*set hsync/vsync polarity to let the polarity is low active inside the VbyOne */
	hsync_pol = 0;
	vsync_pol = 0;
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, hsync_pol, 4, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vsync_pol, 5, 1);

	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, hsync_pol, 6, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, vsync_pol, 7, 1);

	/* below line copy from simulation */
	/* gate the input when vsync asserted */
	lcd_vcbus_setb(VBO_VIN_CTRL_T7 + offset, 1, 0, 2);
	/* lcd_vcbus_write(VBO_VBK_CTRL_0_T7 + offset,0x13);
	 * lcd_vcbus_write(VBO_VBK_CTRL_1_T7 + offset,0x56);
	 * lcd_vcbus_write(VBO_HBK_CTRL_T7 + offset,0x3478);
	 * lcd_vcbus_setb(VBO_PXL_CTRL_T7 + offset,0x2,0,4);
	 * lcd_vcbus_setb(VBO_PXL_CTRL_T7 + offset,0x3,VBO_PXL_CTR1_BIT,VBO_PXL_CTR1_WID);
	 * set_vbyone_ctlbits(1,0,0);
	 */
	/* VBO_RGN_GEN clk always on */
	lcd_vcbus_setb(VBO_GCLK_MAIN_T7 + offset, 2, 2, 2);

	/* PAD select: */
	lcd_vcbus_setb(LCD_PORT_SWAP_T7 + offset, 0, 9, 2);
	/* lcd_vcbus_setb(LCD_PORT_SWAP_T7 + offset, 1, 8, 1);//reverse lane output order */

	lcd_vbyone_hw_filter(pdrv, 1);
	lcd_vcbus_setb(VBO_INSGN_CTRL_T7 + offset, 0, 2, 2);
	lcd_vbyone_interrupt_enable(pdrv, 0);

	/*lsb first for t6w*/
	if (pdrv->data->chip_type == LCD_CHIP_T6W ||
		pdrv->data->chip_type == LCD_CHIP_A9)
		lcd_vcbus_setb(VBO_LANE_OUTPUT_T7 + offset, 0xff, 0, 8);
	else if (pdrv->data->chip_type == LCD_CHIP_T6X)
		lcd_vcbus_setb(VBO_LANE_OUTPUT_T7 + offset, 0xffff, 0, 8);

	lcd_vcbus_setb(VBO_CTRL_L_T7 + offset, 1, 0, 1);

	lcd_vbyone_wait_timing_stable(pdrv);
	lcd_vbyone_sw_reset(pdrv);

	/* training hold */
	if (pdrv->curr_dev->dev_cfg.control.vbyone_cfg.ctrl_flag & 0x4)
		lcd_vbyone_cdr_training_hold(pdrv, 1);
}

static void lcd_vbyone_enable_t3x(struct aml_lcd_drv_s *pdrv)
{
	int lane_count, byte_mode, region_num, slice, hsize, vsize;
	int hsync_pol, vsync_pol, vin_color, vin_bpp;
	/* int color_fmt; */
	unsigned int offset, ppc;

	offset = pdrv->data->offset_venc_if[pdrv->index];

	hsize = pdrv->curr_dev->dev_cfg.timing.act_timing.h_active;
	vsize = pdrv->curr_dev->dev_cfg.timing.act_timing.v_active;
	lane_count = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.lane_count; /* 8 */
	region_num = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.region_num; /* 2 */
	byte_mode = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.byte_mode; /* 4 */
	/* color_fmt = pdrv->curr_dev->dev_cfg.control.vbyone_cfg.color_fmt; // 4 */

	vin_color = 4; /* fixed RGB */
	switch (pdrv->curr_dev->dev_cfg.timing.act_timing.lcd_bits) {
	case 18:
		vin_bpp = 2; /* 18bbp 4:4:4 */
		break;
	case 24:
		vin_bpp = 1; /* 24bbp 4:4:4 */
		break;
	case 30:
	default:
		vin_bpp = 0; /* 30bbp 4:4:4 */
		break;
	}

	/* set Vbyone vin color format */
	lcd_vcbus_setb(VBO_VIN_CTRL_T3X + offset, vin_color, 8, 3);
	lcd_vcbus_setb(VBO_VIN_CTRL_T3X + offset, vin_bpp, 11, 2);

	ppc = pdrv->curr_dev->dev_cfg.timing.ppc;
	slice = ppc;
	lcd_vbyone_lanes_set_t3x(pdrv, offset, lane_count, byte_mode, region_num,
		slice, ppc, hsize, vsize);

	/*set hsync/vsync polarity to let the polarity is low active inside the VbyOne */
	hsync_pol = 0;
	vsync_pol = 0;
	lcd_vcbus_setb(VBO_VIN_CTRL_T3X + offset, hsync_pol, 4, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL_T3X + offset, vsync_pol, 5, 1);

	lcd_vcbus_setb(VBO_VIN_CTRL_T3X + offset, hsync_pol, 6, 1);
	lcd_vcbus_setb(VBO_VIN_CTRL_T3X + offset, vsync_pol, 7, 1);

	/* below line copy from simulation */
	/* gate the input when vsync asserted */
	lcd_vcbus_setb(VBO_VIN_CTRL_T3X + offset, 1, 0, 2);

	/* VBO_RGN_GEN clk always on */
	lcd_vcbus_setb(VBO_GCLK_MAIN_T3X + offset, 2, 2, 2);

	/* PAD select: */
	lcd_vcbus_setb(LCD_PORT_SWAP_T3X + offset, 0, 9, 2);
	/* lcd_vcbus_setb(LCD_PORT_SWAP_T7 + offset, 1, 8, 1);//reverse lane output order */

	lcd_vbyone_hw_filter(pdrv, 1);
	lcd_vcbus_setb(VBO_INSGN_CTRL_T3X + offset, 0, 2, 2);
	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vcbus_setb(VBO_CTRL_T3X + offset, 1, 0, 1);
	if (pdrv->data->chip_type == LCD_CHIP_T6X) {
		lcd_vcbus_setb(VBO_CTRL_T3X + offset, 0, 22, 1);
		lcd_vcbus_setb(VBO_LANE_OUTPUT_T3X + offset, 0xffff, 0, 16);
	}
	lcd_vbyone_wait_timing_stable(pdrv);
	lcd_vbyone_sw_reset(pdrv);

	/* training hold */
	if (pdrv->curr_dev->dev_cfg.control.vbyone_cfg.ctrl_flag & 0x4)
		lcd_vbyone_cdr_training_hold(pdrv, 1);
}

void lcd_vbyone_disable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	lcd_vcbus_setb(vx1_reg->reg_ctrl_l + offset, 0, 0, 1);
	/* clear insig setting */
	lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 0, 2, 1);
	lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 0, 0, 1);
}

void lcd_vbyone_wait_timing_stable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	unsigned int timing_state;
	int i = 200;

	timing_state = lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x1ff;
	while ((timing_state) && (i > 0)) {
		/* clear video timing error intr */
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0x7, 0, 3);
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 3);
		lcd_delay_ms(2);
		timing_state = lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x1ff;
		i--;
	};
	LCD_DBG(pdrv, "vbyone timing state: 0x%03x, i=%d", timing_state, (200 - i));
	lcd_delay_ms(2);
}

void lcd_vbyone_cdr_training_hold(struct aml_lcd_drv_s *pdrv, int flag)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	struct vbyone_config_s *vx1_conf;

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;

	if (flag) {
		LCD_PR(pdrv, "ctrl_flag for cdr_training_hold");
		if (pdrv->data->chip_type == LCD_CHIP_T3X || pdrv->data->chip_type == LCD_CHIP_T6X)
			lcd_vcbus_setb(vx1_reg->reg_holder_h + offset, 0xffff, 16, 16);
		else
			lcd_vcbus_setb(vx1_reg->reg_holder_h + offset, 0xffff, 0, 16);
	} else {
		lcd_delay_ms(vx1_conf->cdr_training_hold);
		if (pdrv->data->chip_type == LCD_CHIP_T3X || pdrv->data->chip_type == LCD_CHIP_T6X)
			lcd_vcbus_setb(vx1_reg->reg_holder_h + offset, 0, 16, 16);//cdr_hold timer
		else
			lcd_vcbus_setb(vx1_reg->reg_holder_h + offset, 0, 0, 16);
	}
}

void lcd_vbyone_wait_hpd(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	unsigned int val;
	int i = 0;

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;

	LCD_DBG(pdrv, "%s ...", __func__);
	while (i++ < VX1_HPD_WAIT_TIMEOUT) {
		if (lcd_vcbus_getb(vx1_reg->reg_status + offset, vx1_reg->hpd_bit, 1) == 0)
			break;
		lcd_delay_us(1000);
	}

	val = lcd_vcbus_getb(vx1_reg->reg_status + offset, vx1_reg->hpd_bit, 1);
	if (val) {
		LCD_PR(pdrv, "%s: hpd=%d", __func__, val);
	} else {
		LCD_PR(pdrv, "%s: hpd=%d, wait time=%dms", pdrv->index, __func__, val, i);
		/* force low only activated for actual hpd is low */
		lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 1, 2, 2);
	}

	if (vx1_conf->ctrl_flag & 0x2) {
		LCD_PR(pdrv, "ctrl_flag for hpd_data delay");
		lcd_delay_ms(vx1_conf->hpd_data_delay);
	} else {
		usleep_range(10000, 10500);
		/* add 10ms delay for compatibility */
	}
}

static void lcd_vbyone_wait_lock(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	int i = VX1_LOCKN_WAIT_TIMEOUT, lock_cnt = 0, lock_ok = 0, lock_confirm_cnt = 0;

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;

	while ((i > 0)) {
		if ((lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x3f) == 0x20) {
			if (++lock_cnt >= VX1_LOCKN_STABLE_CNT) {
				lock_ok = 1;
				lock_confirm_cnt++;
			}
		} else {
			lock_cnt = 0;
			lock_confirm_cnt = 0;
		}
		if (lock_confirm_cnt == VX1_LOCKN_CONFIRM_CNT)
			break;
		if (lock_ok) {
			lock_ok = 0;
			lock_cnt = 0;
			lcd_delay_us(VX1_LOCKN_CONFIRM_DELAY * lock_confirm_cnt);
		} else {
			lcd_delay_us(VX1_LOCKN_INTERVAL);
		}
		i--;
	}
	LCD_PR(pdrv, "%s status: 0x%x, time=%dus",
		__func__, lcd_vcbus_read(vx1_reg->reg_status + offset),
		(VX1_LOCKN_WAIT_TIMEOUT - i) * VX1_LOCKN_INTERVAL);
}

#define LCD_VX1_WAIT_STABLE_POWER_ON_DELAY    300 /* ms */
void lcd_vbyone_power_on_wait_stable(struct aml_lcd_drv_s *pdrv)
{
	lcd_delay_ms(LCD_VX1_WAIT_STABLE_POWER_ON_DELAY);

	lcd_vbyone_wait_lock(pdrv);
	/* power on reset */
	if (pdrv->curr_dev->dev_cfg.control.vbyone_cfg.ctrl_flag & 0x1) {
		LCD_PR(pdrv, "ctrl_flag for power on reset");
		lcd_delay_ms(pdrv->curr_dev->dev_cfg.control.vbyone_cfg.power_on_reset_delay);
		lcd_vbyone_sw_reset(pdrv);
		lcd_vbyone_wait_lock(pdrv);
	}
}

void lcd_vbyone_wait_stable(struct aml_lcd_drv_s *pdrv)
{
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	int i = 0;

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return;

	lcd_vbyone_sw_reset(pdrv);

	while (i++ < VX1_LOCKN_WAIT_TIMEOUT) {
		if ((lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x3f) == 0x20)
			break;
		lcd_delay_us(50);
	}
	LCD_PR(pdrv, "%s status: 0x%x, i=%d", __func__,
		lcd_vcbus_read(vx1_reg->reg_status + offset), i);
}

void lcd_vbyone_interrupt_enable(struct aml_lcd_drv_s *pdrv, int flag)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	LCD_DBG(pdrv, "%s: %d", __func__, flag);

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;
	vx1_conf->timeout_reset_flag = 0;
	vx1_conf->training_stable_cnt = 0;
	vx1_conf->clk_err_cnt = 0;
	vx1_conf->unstable_trg = 0;
	vx1_conf->retry_cnt = 0;
	vx1_conf->vsync_cnt = 0;

	if (flag) {
		if (vx1_conf->vx1_intr_en) {
			vx1_conf->vx1_fsm_acq_st = 0;
			/* clear interrupt */
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0x01ff, 0, 9);
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 9);

			/* set hold in FSM_ACQ */
			if (vx1_conf->vs_intr_en == 3 ||
			    vx1_conf->vs_intr_en == 4)
				lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
			else
				lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0xffff, 0, 16);
			/* enable interrupt */
			lcd_vcbus_setb(vx1_reg->reg_intr_unmask + offset,
					VBYONE_INTR_UNMASK, 0, 15);

			vx1_conf->intr_state |= VX1_INTR_BIT_VX1;
		} else {
			/* mask interrupt */
			lcd_vcbus_write(vx1_reg->reg_intr_unmask + offset, 0x0);
			if (vx1_conf->vs_intr_en) {
				/* keep holder for vsync monitor enabled */
				/* set hold in FSM_ACQ */
				if (vx1_conf->vs_intr_en == 3 || vx1_conf->vs_intr_en == 4)
					lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
				else
					lcd_vcbus_setb(vx1_reg->reg_holder_l + offset,
							0xffff, 0, 16);
			} else {
				/* release holder for vsync monitor disabled */
				/* release hold in FSM_ACQ */
				lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
			}

			vx1_conf->vx1_fsm_acq_st = 0;
			/* clear interrupt */
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0x01ff, 0, 9);
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 9);

			vx1_conf->intr_state &= ~VX1_INTR_BIT_VX1;
		}

		if (vx1_conf->vs_intr_en)
			vx1_conf->intr_state |= VX1_INTR_BIT_VS;
		else
			vx1_conf->intr_state &= ~VX1_INTR_BIT_VS;
	} else {
		vx1_conf->intr_state = 0;

		/* mask interrupt */
		lcd_vcbus_write(vx1_reg->reg_intr_unmask + offset, 0x0);
		/* release hold in FSM_ACQ */
		lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
	}
}

static void lcd_vx1_intr_fsm_monitor(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int state;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];
	unsigned long flags = 0;

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;
	if ((vx1_conf->intr_state & VX1_INTR_BIT_VX1) == 0)
		return;

	spin_lock_irqsave(&vx1_intr_lock, flags);
	if (vx1_conf->unstable_trg == 0) {
		spin_unlock_irqrestore(&vx1_intr_lock, flags);
		return;
	}

	switch (vx1_conf->vx1_fsm_acq_st) {
	case 0:
		/* set FSM_continue */
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
		lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
#endif
		LCD_PR(pdrv, "vx1 sw reset");
		lcd_vbyone_sw_reset(pdrv);
		/* clear intr flag: fsm, lockn raise */
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 1, 15, 1);
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 15, 1);
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 1, 7, 1);
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 7, 1);
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
		vx1_conf->vx1_fsm_acq_st = 2;
		vx1_conf->vsync_cnt = 0;
#else
		vx1_conf->vx1_fsm_acq_st = 1;
#endif
		break;
	case 1:
		vx1_conf->vx1_fsm_acq_st = 2;
		vx1_conf->vsync_cnt = 0;
		/* set FSM_continue */
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_STEP_CONTINUE)
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 15, 1);
		lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 1, 15, 1);
#endif
		LCD_PR(pdrv, "vx1 fsm next, vs_cnt:%d", vx1_conf->vsync_cnt);
		break;
	case 2:
		state = lcd_vcbus_read(vx1_reg->reg_status + offset);
		if (vx1_conf->vsync_cnt++ >= VX1_TRAINING_TIMEOUT) {
			LCD_PR(pdrv, "vx1 fsm state:0x%x, vs_cnt:%d", state, vx1_conf->vsync_cnt);
			if ((state & 0x3f) != 0x20) {
				if (vx1_conf->timeout_reset_flag == 0) {
					vx1_conf->timeout_reset_flag = 1;
					vx1_conf->unstable_trg = 0;
					vx1_conf->vx1_fsm_acq_st = 0;
					lcd_queue_work(&pdrv->vx1_reset_work);
				}
			} else {
				vx1_conf->training_stable_cnt = 0;
				vx1_conf->vx1_fsm_acq_st = 3;
			}
		}
		break;
	case 3:
		state = lcd_vcbus_read(vx1_reg->reg_status + offset);
		if ((state & 0x3f) != 0x20) {
			vx1_conf->vx1_fsm_acq_st = 2;
			vx1_conf->vsync_cnt = VX1_TRAINING_TIMEOUT;
			break;
		}
		if (vx1_conf->training_stable_cnt++ >= VX1_LOCKN_CONFIRM_CNT) {
			vx1_conf->training_stable_cnt = 0;
			vx1_conf->vsync_cnt = 0;
			vx1_conf->unstable_trg = 0;
			vx1_conf->vx1_fsm_acq_st = 0;
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
			lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0xffff, 0, 16);
#endif
			LCD_PR(pdrv, "vx1 fsm stable");
		}
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&vx1_intr_lock, flags);
}

#define LCD_PCLK_TOLERANCE      2000000  /* 2M */
#define LCD_PCLK_ERR_CNT_MAX    3
static void lcd_vx1_clk_monitor_delayed_work(struct work_struct *p_work)
{
	struct delayed_work *d_work;
	struct aml_lcd_drv_s *pdrv;
	struct vbyone_config_s *vx1_conf;
	int encl_clk;

	d_work = container_of(p_work, struct delayed_work, work);
	pdrv = container_of(d_work, struct aml_lcd_drv_s, clk_mnt_delayed_work);

	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		goto vx1_clk_mnt_end;

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;

	encl_clk = lcd_encl_clk_msr(pdrv);
	if (encl_clk == 0)
		vx1_conf->clk_err_cnt++;
	else
		vx1_conf->clk_err_cnt = 0;
	if (vx1_conf->clk_err_cnt >= LCD_PCLK_ERR_CNT_MAX) {
		LCD_PR(pdrv, "%s: pll frequency error: %d", __func__, encl_clk);
		lcd_clk_pll_reset(pdrv);
		vx1_conf->clk_err_cnt = 0;
	}

	if (vx1_conf->vs_intr_en == 0)
		lcd_vx1_intr_fsm_monitor(pdrv);

vx1_clk_mnt_end:
	lcd_queue_delayed_work(&pdrv->clk_mnt_delayed_work, VX1_MNT_INTERVAL);
}

static void lcd_vx1_timeout_reset_work(struct work_struct *work)
{
	struct aml_lcd_drv_s *pdrv;

	pdrv = container_of(work, struct aml_lcd_drv_s, vx1_reset_work);

	if (pdrv->curr_dev->dev_cfg.control.vbyone_cfg.timeout_reset_flag == 0)
		return;

	LCD_PR(pdrv, "%s", __func__);
	lcd_module_reset(pdrv);
	pdrv->curr_dev->dev_cfg.control.vbyone_cfg.timeout_reset_flag = 0;
}

int lcd_vbyone_vsync_handler(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf;
	int reset_flag = 0;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return 0;
	if ((vx1_conf->intr_state & VX1_INTR_BIT_VS) == 0)
		return 0;

	if (lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x40) /* hpd detect */
		return 0;

	lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 1, 0, 1);
	lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 1);

	if (vx1_conf->vs_intr_en == 0)
		return 0;

	if (vx1_conf->vs_intr_en == 4) {
		if (vx1_conf->vsync_cnt == 3) {
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0x3ff, 0, 10);
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 10);
			vx1_conf->vsync_cnt++;
		} else if (vx1_conf->vsync_cnt >= 5) {
			vx1_conf->vsync_cnt = 0;
			if (pdrv->data->chip_type == LCD_CHIP_T6X ||
				pdrv->data->chip_type == LCD_CHIP_T3X) {
				if (lcd_vcbus_read(vx1_reg->reg_intr_state + offset) & 0x4)
					reset_flag = 1;
			} else {
				if (lcd_vcbus_read(vx1_reg->reg_intr_state + offset) & 0x40)
					reset_flag = 1;
			}
			if (reset_flag) {
				lcd_vbyone_hw_filter(pdrv, 0);
				lcd_vbyone_sw_reset(pdrv);
				LCD_PR(pdrv, "vx1 sw_reset 4");

				lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0x3ff, 0, 10);
				lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 10);
				lcd_vbyone_hw_filter(pdrv, 1);
			}
		} else {
			vx1_conf->vsync_cnt++;
		}
	} else if (vx1_conf->vs_intr_en == 3) {
		if (vx1_conf->unstable_trg == 0)
			return 0;
		if (vx1_conf->vsync_cnt == VSYNC_CNT_VX1_RESET) {
			lcd_vbyone_hw_filter(pdrv, 0);
			lcd_vbyone_sw_reset(pdrv);
			LCD_PR(pdrv, "vx1 sw_reset 3");
			vx1_conf->vsync_cnt++;
		} else if ((vx1_conf->vsync_cnt > VSYNC_CNT_VX1_RESET) &&
			   (vx1_conf->vsync_cnt <= VSYNC_CNT_VX1_STABLE)  &&
			   (vx1_conf->retry_cnt <= VSYNC_CNT_VX1_RETRY)) {
			if (lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x20) {
				vx1_conf->vsync_cnt++;
			} else {
				vx1_conf->vsync_cnt = VSYNC_CNT_VX1_RESET + 1;
				vx1_conf->retry_cnt++;
			}
		} else if (vx1_conf->vsync_cnt > VSYNC_CNT_VX1_STABLE ||
			   vx1_conf->retry_cnt > VSYNC_CNT_VX1_RETRY) {
			vx1_conf->unstable_trg = 0;
			vx1_conf->vsync_cnt = 0;
			vx1_conf->retry_cnt = 0;
			lcd_vbyone_hw_filter(pdrv, 1);
		} else {
			vx1_conf->vsync_cnt++;
		}
	} else if (vx1_conf->vs_intr_en == 2) {
		if (vx1_conf->unstable_trg == 0) {
			if (!(lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x20)) {
				vx1_conf->unstable_trg = 1;
				vx1_conf->vsync_cnt = 0;
				lcd_vbyone_hw_filter(pdrv, 0);
				lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
				lcd_vbyone_sw_reset(pdrv);
				LCD_PR(pdrv, "vx1 sw_reset 2");
			}
			return 0;
		}
		if (vx1_conf->vsync_cnt >= VX1_TRAINING_TIMEOUT) {
			vx1_conf->vsync_cnt = 0;
			if (!(lcd_vcbus_read(vx1_reg->reg_status + offset) & 0x20)) {
				lcd_vbyone_hw_filter(pdrv, 0);
				lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
				lcd_vbyone_sw_reset(pdrv);
				LCD_PR(pdrv, "vx1 sw_reset 2");
			} else {
				vx1_conf->unstable_trg = 0;
				lcd_vbyone_hw_filter(pdrv, 1);
				lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0xffff, 0, 16);
			}
		} else {
			vx1_conf->vsync_cnt++;
		}
	} else { /*vx1_conf->vs_intr_en == 1*/
		lcd_vx1_intr_fsm_monitor(pdrv);
	}

	return 0;
}

static irqreturn_t lcd_vbyone_interrupt_handler(int irq, void *data)
{
	struct aml_lcd_drv_s *pdrv = (struct aml_lcd_drv_s *)data;
	struct vbyone_config_s *vx1_conf;
	unsigned int data32, data32_1;
	int encl_clk;
	unsigned long flags = 0;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;
	if ((pdrv->status & LCD_STATUS_IF_ON) == 0)
		return IRQ_HANDLED;
	if ((vx1_conf->intr_state & VX1_INTR_BIT_VX1) == 0)
		return IRQ_HANDLED;

	/* ****************************************
	 * vsync_intr_en: 2 or 4
	 * ****************************************
	 */
	if (vx1_conf->vs_intr_en == 2 ||
	    vx1_conf->vs_intr_en == 4)
		return IRQ_HANDLED;
	/* ************************************** */

	spin_lock_irqsave(&vx1_intr_lock, flags);

	lcd_vcbus_write(vx1_reg->reg_intr_unmask + offset, 0x0);  /* mask interrupt */

	encl_clk = lcd_encl_clk_msr(pdrv);
	if (pdrv->data->chip_type == LCD_CHIP_T3X ||
		pdrv->data->chip_type == LCD_CHIP_T6X) {
		// without case1 and case2 for 8k chip
		data32 = (lcd_vcbus_read(vx1_reg->reg_intr_state + offset) & 0x1ff);
		data32_1 = data32;
	} else {
		data32 = (lcd_vcbus_read(vx1_reg->reg_intr_state + offset) & 0x7fff);
		/* clear the interrupt */
		data32_1 = ((data32 >> 9) << 3); //htpdn lockn fsm interrupt
		if (data32 & 0x1c0) //de timing error
			data32_1 |= (1 << 2);
		if (data32 & 0x38) //vsync timing error
			data32_1 |= (1 << 1);
		if (data32 & 0x7) //hsync timing error
			data32_1 |= (1 << 0);
	}
	// clear interrupt status 1 -> 0
	lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, data32_1, 0, 9);
	lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 9);
	LCD_DBG(pdrv, "vx1 intr status: 0x%04x, encl_clkmsr: %d", data32, encl_clk);

	/* ****************************************
	 * vsync_intr_en: 3
	 * ****************************************
	 */
	if (vx1_conf->vs_intr_en == 3) {
		if (data32_1 & 0x40) { //bit:12 lockn rise up
			if (vx1_conf->unstable_trg == 0) {
				vx1_conf->unstable_trg = 1;
				vx1_conf->vsync_cnt = 0;
				LCD_PR(pdrv, "vx1 lockn rise edge");
			}
		}

		/* enable interrupt */
		lcd_vcbus_setb(vx1_reg->reg_intr_unmask + offset, VBYONE_INTR_UNMASK, 0, 15);

		spin_unlock_irqrestore(&vx1_intr_lock, flags);
		return IRQ_HANDLED;
	}
	/* ************************************** */

	/* ****************************************
	 * vsync_intr_en: 1 and 0
	 * ****************************************
	 */
	if (data32_1 & 0x80) { //bit:13 FSM acquisition
		if (vx1_conf->vx1_fsm_acq_st == 0)
			vx1_conf->unstable_trg = 1;
		LCD_DBG(pdrv, "vx1 fsm_acq wait end, status: 0x%x, fsm_acq_st: %d",
			lcd_vcbus_read(vx1_reg->reg_status + offset), vx1_conf->vx1_fsm_acq_st);
	}

	if (data32_1 & 0x7) { //bit9:0 hsync/vsync/de timing error
		LCD_DBG(pdrv, "vx1 reset for timing err");
		if (vx1_conf->unstable_trg) {
			lcd_vbyone_sw_reset(pdrv);
			/* clear intr flag: lockn raise */
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 1, 7, 1);
			lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 7, 1);
#if (VX1_FSM_ACQ_NEXT == VX1_FSM_ACQ_NEXT_RELEASE_HOLDER)
			vx1_conf->vx1_fsm_acq_st = 2;
			vx1_conf->vsync_cnt = 0;
#else
			vx1_conf->vx1_fsm_acq_st = 1;
#endif
		}
	}

	/* enable interrupt */
	lcd_vcbus_setb(vx1_reg->reg_intr_unmask + offset, VBYONE_INTR_UNMASK, 0, 15);

	spin_unlock_irqrestore(&vx1_intr_lock, flags);

	return IRQ_HANDLED;
}

static void lcd_vbyone_interrupt_init(struct aml_lcd_drv_s *pdrv)
{
	struct vbyone_config_s *vx1_conf;
	unsigned int offset = pdrv->data->offset_venc_if[pdrv->index];

	vx1_conf = &pdrv->curr_dev->dev_cfg.control.vbyone_cfg;

	/* release sw filter ctrl in uboot */
	lcd_vcbus_setb(vx1_reg->reg_insig_ctrl + offset, 0, 0, 1);
	lcd_vbyone_hw_filter(pdrv, 1);

	if (pdrv->data->chip_type == LCD_CHIP_T3X ||
		pdrv->data->chip_type == LCD_CHIP_T6X) {
		/* set hold in FSM_ACQ */
		if (vx1_conf->vs_intr_en == 3 || vx1_conf->vs_intr_en == 4)
			lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
		else
			lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0xffff, 0, 16);
		/* set hold in FSM_CDR */
		lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 16, 16);
	} else {
		/* set hold in FSM_ACQ */
		if (vx1_conf->vs_intr_en == 3 ||
			vx1_conf->vs_intr_en == 4)
			lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0, 0, 16);
		else
			lcd_vcbus_setb(vx1_reg->reg_holder_l + offset, 0xffff, 0, 16);
		/* set hold in FSM_CDR */
		lcd_vcbus_setb(vx1_reg->reg_holder_h + offset, 0, 0, 16);
	}
	/* not wait lockn to 1 in FSM_ACQ */
	lcd_vcbus_setb(vx1_reg->reg_ctrl_l + offset, 1, 10, 1);
	/* lcd_vcbus_setb(VBO_CTRL_L, 0, 9, 1);*/   /*use sw pll_lock */
	/* reg_pll_lock = 1 to release force to FSM_ACQ*/
	/*lcd_vcbus_setb(VBO_CTRL_H, 1, 13, 1); */

	/* vx1 interrupt setting */
	lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 1, 12, 1);    /* intr pulse width */
	lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0x01ff, 0, 9); /* clear interrupt */
	lcd_vcbus_setb(vx1_reg->reg_intr_ctrl + offset, 0, 0, 9);

	LCD_DBG(pdrv, "%s", __func__);
}

#define VBYONE_IRQF   IRQF_SHARED /* IRQF_DISABLED */ /* IRQF_SHARED */

int lcd_vbyone_interrupt_up(struct aml_lcd_drv_s *pdrv)
{
	unsigned int venc_vx1_irq = 0;

	lcd_vbyone_interrupt_init(pdrv);
	spin_lock_init(&vx1_intr_lock);

	INIT_WORK(&pdrv->vx1_reset_work, lcd_vx1_timeout_reset_work);
	INIT_DELAYED_WORK(&pdrv->clk_mnt_delayed_work, lcd_vx1_clk_monitor_delayed_work);

	if (!pdrv->drv_res.res_vx1_irq) {
		LCD_ERR(pdrv, "res_vx1_irq is null");
		return -1;
	}
	venc_vx1_irq = pdrv->drv_res.res_vx1_irq;
	LCD_DBG(pdrv, "venc_vx1_irq: %d", venc_vx1_irq);

	snprintf(pdrv->drv_res.vbyone_isr_name, 10, "vbyone%d", pdrv->index);
	if (request_irq(venc_vx1_irq, lcd_vbyone_interrupt_handler, 0,
			pdrv->drv_res.vbyone_isr_name, (void *)pdrv)) {
		LCD_ERR(pdrv, "can't request %s", pdrv->drv_res.vbyone_isr_name);
	} else {
		LCD_DBG(pdrv, "request %s successful", pdrv->drv_res.vbyone_isr_name);
	}
	// pdrv->vbyone_vsync_handler = lcd_vbyone_vsync_handler;

	lcd_vbyone_interrupt_enable(pdrv, 1);

	lcd_queue_delayed_work(&pdrv->clk_mnt_delayed_work, VX1_MNT_INTERVAL);

	return 0;
}

void lcd_vbyone_interrupt_down(struct aml_lcd_drv_s *pdrv)
{
	lcd_vbyone_interrupt_enable(pdrv, 0);
	free_irq(pdrv->drv_res.res_vx1_irq, (void *)pdrv);
	cancel_work_sync(&pdrv->vx1_reset_work);
	cancel_delayed_work_sync(&pdrv->clk_mnt_delayed_work);

	LCD_DBG(pdrv, "free vbyone irq");
}

void lcd_vbyone_debug_cdr(struct aml_lcd_drv_s *pdrv)
{
	unsigned int state;

	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vbyone_force_cdr(pdrv);

	lcd_delay_ms(100);
	state = lcd_vbyone_get_fsm_state(pdrv);
	LCD_PR(pdrv, "vbyone cdr: fsm state: 0x%02x", state);
}

void lcd_vbyone_debug_lock(struct aml_lcd_drv_s *pdrv)
{
	unsigned int state;

	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vbyone_force_lock(pdrv);

	lcd_delay_ms(20);
	state = lcd_vbyone_get_fsm_state(pdrv);
	LCD_PR(pdrv, "vbyone cdr: fsm state: 0x%02x\n", state);
}

void lcd_vbyone_debug_reset(struct aml_lcd_drv_s *pdrv)
{
	unsigned int state;

	lcd_vbyone_interrupt_enable(pdrv, 0);

	lcd_vbyone_sw_reset(pdrv);

	lcd_delay_ms(200);
	state = lcd_vbyone_get_fsm_state(pdrv);
	LCD_PR(pdrv, "vbyone reset: fsm state: 0x%02x\n", state);

	lcd_vbyone_interrupt_enable(pdrv, 1);
}

void lcd_vbyone_enable(struct aml_lcd_drv_s *pdrv)
{
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T6W:
	case LCD_CHIP_A9:
		lcd_vbyone_enable_t7(pdrv);
		break;
	case LCD_CHIP_T3X:
	case LCD_CHIP_T6X:
		lcd_vbyone_enable_t3x(pdrv);
		break;
	default:
		lcd_vbyone_enable_dft(pdrv);
		break;
	}
}

void lcd_vbyone_probe(struct aml_lcd_drv_s *pdrv)
{
	switch (pdrv->data->chip_type) {
	case LCD_CHIP_T7:
	case LCD_CHIP_T3:
	case LCD_CHIP_T5W:
	case LCD_CHIP_T5M:
	case LCD_CHIP_T6W:
	case LCD_CHIP_A9:
		vx1_reg = &vbo_reg_t7;
		break;
	case LCD_CHIP_T3X:
	case LCD_CHIP_T6X:
		vx1_reg = &vbo_reg_t3x;
		break;
	default:
		vx1_reg = &vbo_reg_dft;
		break;
	}
}
