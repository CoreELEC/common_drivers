// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include <linux/init.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/amlogic/media/vout/lcd/lcd_vout.h>
#include "lcd_phy_config.h"
#include "../lcd_common.h"

static struct lcd_phy_ctrl_s *lcd_phy_ctrl;

inline unsigned int lcd_phy_vswing_level_to_value(struct aml_lcd_drv_s *pdrv,
			struct aml_lcd_device_s *dev_p, unsigned int level)
{
	if (!lcd_phy_ctrl)
		return 0;

	if (!lcd_phy_ctrl->phy_vswing_level_to_val)
		return level;

	return lcd_phy_ctrl->phy_vswing_level_to_val(pdrv, dev_p, level);
}

unsigned int lcd_phy_preem_level_to_value(struct aml_lcd_drv_s *pdrv,
			struct aml_lcd_device_s *dev_p, unsigned int level)
{
	if (!lcd_phy_ctrl)
		return 0;

	if (!lcd_phy_ctrl->phy_preem_level_to_val)
		return level;

	return lcd_phy_ctrl->phy_preem_level_to_val(pdrv, dev_p, level);
}

unsigned int lcd_phy_support_lane_phase(struct aml_lcd_drv_s *pdrv)
{
	if (!lcd_phy_ctrl || pdrv->curr_dev->dev_cfg.basic.lcd_type != LCD_MLVDS)
		return 0;

	return lcd_phy_ctrl->phy_lane_phase_sel_def ? 1 : 0;
}

int lcd_phy_param_preset(struct aml_lcd_drv_s *pdrv, struct aml_lcd_device_s *dev_p)
{
	struct phy_config_s *phy_cfg = &dev_p->dev_cfg.phy_cfg;
	struct phy_attr_s *phy = dev_p->dev_cfg.phy_cfg.act_phy;
	unsigned int amp = 0, preem = 0, rterm = 0;
	int i;

	if (!lcd_phy_ctrl)
		return -1;

	phy_cfg->lane_num = lcd_phy_ctrl->lane_num;
	if (lcd_phy_ctrl->phy_glb_param_dft_val)
		lcd_phy_ctrl->phy_glb_param_dft_val(pdrv, dev_p);
	if (lcd_phy_ctrl->phy_vswing_level_to_val)
		phy->vswing = lcd_phy_ctrl->phy_vswing_level_to_val(pdrv, dev_p,
								phy_cfg->vswing_level);
	if (lcd_phy_ctrl->phy_preem_level_to_val)
		preem = lcd_phy_ctrl->phy_preem_level_to_val(pdrv, dev_p, phy_cfg->preem_level);
	if (lcd_phy_ctrl->phy_amp_dft_val)
		amp = lcd_phy_ctrl->phy_amp_dft_val(pdrv, dev_p);
	if (lcd_phy_ctrl->phy_rterm_dft_val)
		rterm = lcd_phy_ctrl->phy_rterm_dft_val(pdrv, dev_p);

	for (i = 0; i < CH_LANE_MAX; i++) {
		if (i < phy_cfg->lane_num) {
			phy->lane[i].amp = amp;
			phy->lane[i].preem = preem;
			phy->lane[i].rterm = rterm;
			phy_cfg->ch_ctrl[i].sel = i;
			if (lcd_phy_ctrl->phy_lane_phase_sel_def) {
				phy_cfg->ch_ctrl[i].phase_sel =
					lcd_phy_ctrl->phy_lane_phase_sel_def(pdrv, dev_p, i);
			} else {
				phy_cfg->ch_ctrl[i].phase_sel = 0xff;
			}
			if (lcd_phy_ctrl->phy_lane_pn_swap_dft) {
				phy_cfg->ch_ctrl[i].pn_swap =
					lcd_phy_ctrl->phy_lane_pn_swap_dft(pdrv, dev_p, i);
			} else {
				phy_cfg->ch_ctrl[i].pn_swap = 0xff;
			}
			phy_cfg->ch_ctrl[i].en = 1;
		} else {
			phy_cfg->ch_ctrl[i].en = 0;
			phy_cfg->ch_ctrl[i].sel = 0xff;
		}
	}

	if (dev_p->dev_cfg.basic.lcd_type == LCD_MLVDS) {
		phy->clk_phase = dev_p->dev_cfg.control.mlvds_cfg.clk_phase & 0xfff;
		phy_cfg->bypass_resample = (dev_p->dev_cfg.control.mlvds_cfg.clk_phase >> 12) & 1;
	}

	return 0;
}

int lcd_phy_param_get(struct aml_lcd_drv_s *pdrv, struct phy_config_s *phy_cfg,
		      struct phy_attr_s *phy)
{
	int ret;

	if (!pdrv || !phy_cfg || !phy)
		return -1;
	if (!lcd_phy_ctrl || !lcd_phy_ctrl->phy_param_get) {
		LCD_PR(pdrv, "%s: phy_param_get is null", __func__);
		return -1;
	}

	memcpy(phy_cfg, &pdrv->curr_dev->dev_cfg.phy_cfg, sizeof(struct phy_config_s));
	lcd_lane_sel_get(pdrv, phy_cfg);
	ret = lcd_phy_ctrl->phy_param_get(pdrv, phy_cfg, phy);
	return ret;
}

int lcd_phy_param_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	struct phy_config_s local_phy_cfg, *phy_cfg;
	struct phy_attr_s local_phy, *phy;
	char str_sel[12], str_phase[12], str_pn_swap[12];
	int i, n, len = offset, ret;

	if (!pdrv)
		return 0;
	if (lcd_pxp_mode)
		return 0;

	ret = lcd_phy_param_get(pdrv, &local_phy_cfg, &local_phy);
	if (ret) {
		memset(&local_phy_cfg, 0, sizeof(struct phy_config_s));
		memset(&local_phy,     0, sizeof(struct phy_attr_s));
	}

	phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;
	phy = pdrv->curr_dev->dev_cfg.phy_cfg.act_phy;

	len = sprintf(buf,
		"vswing  = 0x%x(0x%x)\n"
		"odt     = 0x%x(0x%x)\n"
		"vcm     = 0x%x(0x%x)\n"
		"cv_mode = %d(%d)\n"
		"ref_bias= %d(%d)\n",
		phy->vswing, local_phy.vswing,
		phy->odt, local_phy.odt,
		phy->vcm, local_phy.vcm,
		phy->cv_mode, local_phy.cv_mode,
		phy->ref_bias, local_phy.ref_bias);
	len += sprintf(buf + len,
		"  lane  en    sel       pn_swap  phase_sel  amp      preem     rterm\n");
	for (i = 0; i < phy_cfg->lane_num; i++) {
		//lane sel
		if (phy_cfg->ch_ctrl[i].sel == 0xff)
			n = sprintf(str_sel, " - ");
		else
			n = sprintf(str_sel, "0x%x", phy_cfg->ch_ctrl[i].sel);
		if (local_phy_cfg.ch_ctrl[i].sel == 0xff)
			sprintf(str_sel + n, "( - )");
		else
			sprintf(str_sel + n, "(0x%x)", local_phy_cfg.ch_ctrl[i].sel);

		//pn swap
		if (local_phy_cfg.ch_ctrl[i].pn_swap == 0xff)
			n = sprintf(str_pn_swap, " - ");
		else
			n = sprintf(str_pn_swap, "0x%x", phy_cfg->ch_ctrl[i].pn_swap);
		if (local_phy_cfg.ch_ctrl[i].pn_swap == 0xff)
			sprintf(str_pn_swap + n, "( - )");
		else
			sprintf(str_pn_swap + n, "(0x%x)", local_phy_cfg.ch_ctrl[i].pn_swap);

		//phase sel
		if (phy_cfg->ch_ctrl[i].phase_sel == 0xff)
			n = sprintf(str_phase, " - ");
		else
			n = sprintf(str_phase, "0x%x", phy_cfg->ch_ctrl[i].phase_sel);
		if (local_phy_cfg.ch_ctrl[i].phase_sel == 0xff)
			sprintf(str_phase + n, "( - )");
		else
			sprintf(str_phase + n, "(0x%x)", local_phy_cfg.ch_ctrl[i].phase_sel);
		len += sprintf(buf + len,
			"  [%2d]: %d(%d), %s, %s, %s, 0x%x(0x%x), 0x%x(0x%x), 0x%x(0x%x)\n",
			i, phy_cfg->ch_ctrl[i].en, local_phy_cfg.ch_ctrl[i].en,
			str_sel, str_pn_swap, str_phase,
			phy->lane[i].amp, local_phy.lane[i].amp,
			phy->lane[i].preem, local_phy.lane[i].preem,
			phy->lane[i].rterm, local_phy.lane[i].rterm);
	}
	len += sprintf(buf + len,
		"flag=0x%x, state=%d, lane_num=%d, lane valid=0x%x, offset=%d, mask=0x%x\n",
		phy_cfg->flag, phy_cfg->state, phy_cfg->lane_num,
		phy_cfg->lane_valid, phy_cfg->lane_offset, phy_cfg->lane_mask);
	len += sprintf(buf + len,
		"ch_swap0=0x%x, ch_swap1=0x%x, ckdi=0x%x\n",
		phy_cfg->ch_swap0, phy_cfg->ch_swap1, phy_cfg->ckdi);

	return len;
}

int lcd_phy_analog_reg_print(struct aml_lcd_drv_s *pdrv, char *buf, int offset)
{
	int n, len = 0;

	if (!pdrv)
		return 0;
	if (!lcd_phy_ctrl || !lcd_phy_ctrl->phy_reg_dump) {
		n = lcd_debug_info_len(len + offset);
		len += snprintf((buf + len), n, "%s: phy_reg_dump is null\n", __func__);
		return len;
	}

	n = lcd_debug_info_len(len + offset);
	len += snprintf((buf + len), n, "\nphy analog regs:\n");
	len += lcd_phy_ctrl->phy_reg_dump(pdrv, (buf + len), (len + offset));
	return len;
}

void lcd_phy_set(struct aml_lcd_drv_s *pdrv, int status)
{
	struct phy_config_s *phy_cfg;
	int i;

	if (lcd_pxp_mode)
		return;
	if (!pdrv->curr_dev)
		return;
	if (!lcd_phy_ctrl || !pdrv->phy_set) {
		LCD_PR(pdrv, "%s: phy_set is null", __func__);
		return;
	}

	phy_cfg = &pdrv->curr_dev->dev_cfg.phy_cfg;
	for (i = 0; i < pdrv->data->drv_max; i++) {
		if (pdrv->index == i)
			continue;
		if (phy_cfg->lane_valid & lcd_phy_ctrl->lane_lock[i]) {
			LCD_ERR(pdrv, "%s: lane_valid 0x%x conflict with lane_lock[%d] 0x%x",
				__func__, phy_cfg->lane_valid, i, lcd_phy_ctrl->lane_lock[i]);
			return;
		}
	}

	lcd_phy_ctrl->lane_lock[pdrv->index] = phy_cfg->lane_valid;
	if (status)
		lcd_phy_ctrl->lane_lock_total |= phy_cfg->lane_valid;
	else
		lcd_phy_ctrl->lane_lock_total &= ~phy_cfg->lane_valid;

	LCD_DBG(pdrv, "phy_set: %d: lane_valid=0x%x, lane_lock_total=0x%x",
		status, phy_cfg->lane_valid, lcd_phy_ctrl->lane_lock_total);

	if (status != LCD_PHY_LOCK_LANE)
		pdrv->phy_set(pdrv, status);
	phy_cfg->state = status;
}

void lcd_phy_reset(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_pxp_mode)
		return;
	if (!lcd_phy_ctrl || !lcd_phy_ctrl->phy_reset)
		return;

	lcd_phy_ctrl->phy_reset(pdrv);
	if (lcd_debug_print_flag & LCD_DBG_PR_NORMAL)
		LCD_PR(pdrv, "phy reset");
}

int lcd_phy_probe(struct aml_lcd_drv_s *pdrv)
{
	if (lcd_pxp_mode) {
		pdrv->phy_set = NULL;
		return 0;
	}

	if (!pdrv->curr_dev || !pdrv->curr_dev->dev_cfg.phy_cfg.act_phy || !lcd_phy_ctrl) {
		pdrv->phy_set = NULL;
		return 0;
	}

	lcd_phy_ctrl->lane_lock[pdrv->index] = 0;
	switch (pdrv->curr_dev->dev_cfg.basic.lcd_type) {
#ifdef CONFIG_AMLOGIC_LCD_LVDS
	case LCD_LVDS:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_lvds;
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_VBYONE
	case LCD_VBYONE:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_vx1;
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_TCON
	case LCD_MLVDS:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_mlvds;
		break;
	case LCD_P2P:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_p2p;
		break;
#endif
#ifdef CONFIG_AMLOGIC_LCD_MIPI_DSI
	case LCD_MIPI:
		pdrv->phy_set = lcd_phy_ctrl->phy_set_mipi;
		break;
#endif
	default:
		pdrv->phy_set = NULL;
		break;
	}

	if (pdrv->status & LCD_STATUS_IF_ON)
		lcd_phy_set(pdrv, LCD_PHY_LOCK_LANE);

	return 0;
}

int lcd_phy_config_init(struct lcd_data_s *pdata)
{
	lcd_phy_ctrl = NULL;

	switch (pdata->chip_type) {
	case LCD_CHIP_G12A:
	case LCD_CHIP_G12B:
	case LCD_CHIP_SM1:
		lcd_phy_ctrl = lcd_phy_config_init_g12(pdata);
		break;
	case LCD_CHIP_TM2:
		lcd_phy_ctrl = lcd_phy_config_init_tm2(pdata);
		break;
	case LCD_CHIP_T5D:
		lcd_phy_ctrl = lcd_phy_config_init_t5d(pdata);
		break;
	case LCD_CHIP_T5W:
		lcd_phy_ctrl = lcd_phy_config_init_t5w(pdata);
		break;
	case LCD_CHIP_T7:
		lcd_phy_ctrl = lcd_phy_config_init_t7(pdata);
		break;
	case LCD_CHIP_T3:
	case LCD_CHIP_T5M:
		lcd_phy_ctrl = lcd_phy_config_init_t3_t5m(pdata);
		break;
	case LCD_CHIP_C3:
		lcd_phy_ctrl = lcd_phy_config_init_c3(pdata);
		break;
	case LCD_CHIP_T3X:
		lcd_phy_ctrl = lcd_phy_config_init_t3x(pdata);
		break;
	case LCD_CHIP_TXHD2:
		lcd_phy_ctrl = lcd_phy_config_init_txhd2(pdata);
		break;
	case LCD_CHIP_S6:
		lcd_phy_ctrl = lcd_phy_config_init_s6(pdata);
		break;
	case LCD_CHIP_T6D:
		lcd_phy_ctrl = lcd_phy_config_init_t6d(pdata);
		break;
	case LCD_CHIP_T6W:
		lcd_phy_ctrl = lcd_phy_config_init_t6w(pdata);
		break;
	case LCD_CHIP_T6X:
		lcd_phy_ctrl = lcd_phy_config_init_t6x(pdata);
		break;
	case LCD_CHIP_A9:
		lcd_phy_ctrl = lcd_phy_config_init_a9(pdata);
		break;
	case LCD_CHIP_C5:
		lcd_phy_ctrl = lcd_phy_config_init_c5(pdata);
		break;
	default:
		break;
	}
	//if (lcd_phy_ctrl)
	//	lcd_phy_ctrl->lane_lock_total = 0;

	return 0;
}
