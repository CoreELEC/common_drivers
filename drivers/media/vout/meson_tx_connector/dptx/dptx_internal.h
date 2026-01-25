/* SPDX-License-Identifier: (GPL-2.0+ OR MIT) */
/*
 * Copyright (c) 2025 Amlogic, Inc. All rights reserved.
 */

#ifndef __DPTX_INTERNAL_H
#define __DPTX_INTERNAL_H

struct meson_tx_state;
struct tx_timing;

int dptx_hpd_plugout(struct meson_tx_dev *tx_dev);
int dptx_hpd_plugin(struct meson_tx_dev *tx_dev);
void dptx_remove_sysfs(struct meson_tx_dev *tx_dev);
int dptx_create_sysfs(struct meson_tx_dev *tx_dev);
bool dptx_validate_timing_with_basic_cs(struct meson_tx_dev *tx_base, struct tx_timing *timing);
int dptx_validate_tx_state_fmt_para(struct meson_tx_dev *tx_base,
	struct meson_tx_state *base_state);
int dptx_calc_hw_fmt_para(struct meson_tx_dev *tx_base, struct meson_tx_format_para *sw_para,
	struct dptx_hw_fmt_para *hw_para);

int dptx_common_init(struct dptx_common *tx_comm, struct dptx_hw_common *hw_comm);
int dptx_common_uninit(struct dptx_common *tx_comm);
void dptx_vout_init(struct dptx_common *tx_comm);
void dptx_vout_uninit(struct dptx_common *tx_comm);
struct dptx_common *dptx_get_edptx_inst(u8 idx);

#endif
