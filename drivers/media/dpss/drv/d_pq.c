// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#ifdef RUN_ON_ARM
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/kfifo.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#endif
#include "d_define.h"
#include "../dpss/drv/d_pq_mgr.h"
#include "../dpss/dpss_s.h"

#include <linux/amlogic/media/di/di_pq.h>
#ifdef FUNC_EN_PQ
static struct di_ext_pq_ops *dipq_api;
#define DIPQ_INFO(fmt, args ...)		pr_info("dpsspq:" fmt, ##args)

static u32 dinr_me_buffer_data[DPSS_RO_ME_SIZE];
struct di_parm_s dinr_pq_data[DPSS_PQ_CHANNEL_NUM];
static struct ro_buffer_parm_s dae_pq_ro_buffer[DPSS_PQ_CHANNEL_NUM];
static struct ro_buffer_parm_s dpe_pq_ro_buffer[DPSS_PQ_CHANNEL_NUM];
static struct ro_buffer_parm_s dblk_h_pq_ro_buffer[DPSS_PQ_CHANNEL_NUM];
static struct ro_buffer_parm_s dblk_v_pq_ro_buffer[DPSS_PQ_CHANNEL_NUM];
static struct ro_buffer_parm_s pq_me_ro_buffer[DPSS_PQ_CHANNEL_NUM];
static u32 dinr_input_buffer_data[DPSS_RO_INPUT_SIZE];
static struct ro_buffer_parm_s pq_input_ro_buffer[DPSS_PQ_CHANNEL_NUM];
static u16 me_width_init;
static u16 me_height_init;

#define color_primaries ((vf->signal_type >> 16) & 0xff)
#define transfer_characteristic ((vf->signal_type >> 8) & 0xff)
#define cuva ((vf->signal_type >> 31) & 1)
#define SEI_MAGIC_CD 0x53656920 /* SEI */

unsigned int dpss_bypass_ko;
module_param_named(dpss_bypass_ko, dpss_bypass_ko, uint, 0664);

struct di_db_reg_s di_db_dm[DPSS_DB_DM_NUM] = {
	{0xD251, 4, 7, 8},
	{0xD25A, 24, 29, 1},
	{0xD25A, 16, 21, 1},
	{0xD25A, 8, 13, 2},
	{0xD25A, 0, 5, 3},
	{0xD259, 24, 29, 4},
	{0xD259, 16, 21, 6},
	{0xD259, 8, 13, 8},
	{0xD259, 0, 5, 10},
	{0xD258, 24, 29, 12},
	{0xD258, 16, 21, 16},
	{0xD258, 8, 13, 20},
	{0xD258, 0, 5, 24},
	{0xD257, 24, 29, 30},
	{0xD257, 16, 21, 38},
	{0xD257, 8, 13, 48},
	{0xD257, 0, 5, 60},
	{0xD25C, 24, 29, 28},
	{0xD25C, 16, 21, 24},
	{0xD25C, 8, 13, 18},
	{0xD25C, 0, 5, 18},
	{0xD25B, 24, 29, 22},
	{0xD25B, 16, 21, 28},
	{0xD25B, 8, 13, 32},
	{0xD25B, 0, 5, 32},
	{0xD25D, 0, 3, 15},
	{0xA2D0, 8, 17, 120},
	{0xA2D0, 0, 5, 32},
	{0xD250, 16, 21, 14},
	{0xD250, 8, 13, 8},
	{0xD250, 0, 5, 2},
	{0xD251, 12, 17, 10},
	{0xD252, 16, 21, 16},
	{0xD252, 8, 13, 48},
	{0xD252, 4, 7, 4},
	{0xD252, 0, 3, 4},
	{0xD253, 16, 21, 8},
	{0xD253, 8, 13, 28},
	{0xD253, 4, 7, 4},
	{0xD253, 0, 3, 2},
	{0xD254, 20, 27, 5},
	{0xD254, 12, 19, 15},
	{0xD254, 4, 11, 23},
	{0xD254, 0, 3, 1},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
};
EXPORT_SYMBOL(di_db_dm);

struct di_db_reg_s di_db_dct[DPSS_DB_DM_NUM] = {
	{0xD700, 29, 30, 3},
	{0xD700, 27, 28, 3},
	{0xB934, 31, 31, 1},
	{0xB934, 30, 30, 0},
	{0xB934, 24, 29, 0},
	{0xB934, 16, 23, 128},
	{0xB934, 12, 15, 8},
	{0xB934, 0, 8, 0},
	{0xD716, 24, 31, 10},
	{0xD716, 16, 23, 18},
	{0xD716, 15, 15, 1},
	{0xD716, 13, 14, 2},
	{0xD716, 8, 12, 10},
	{0xD717, 12, 13, 2},
	{0xD717, 0, 11, 0},//?
	{0xD718, 31, 31, 1},
	{0xD718, 30, 30, 0},
	{0xD718, 29, 29, 0},
	{0xD718, 28, 28, 0},
	{0xD718, 26, 27, 2},
	{0xD718, 24, 25, 1},
	{0xD718, 16, 23, 64},
	{0xD718, 0, 8, 0},
	{0xD719, 16, 23, 64},
	{0xD719, 0, 8, 0},
	{0xD71A, 24, 29, 8},
	{0xD71A, 16, 21, 16},
	{0xD71A, 8, 13, 32},
	{0xD71A, 0, 5, 32},
	{0xD71B, 24, 29, 32},
	{0xD71B, 16, 21, 32},
	{0xD71B, 8, 13, 32},
	{0xD71B, 0, 5, 32},
	{0xD71C, 0, 5, 32},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
	{0xffff, 0, 0, 0}, {0xffff, 0, 0, 0},
};
EXPORT_SYMBOL(di_db_dct);

int dpss_db_data_version(void)
{
	int version;

	version = 0x000A;
	return version;
}

void dpss_me_size_update(u8 ch)
{
	dinr_pq_data[ch].me_width = me_width_init;
	dinr_pq_data[ch].me_height = me_height_init;
}

void dpss_me_width(u32 data)
{
	me_width_init = data;
}

void dpss_me_height(u32 data)
{
	me_height_init = data;
}

struct di_parm_s *dpss_pq_data(u8 ch)
{
	return &dinr_pq_data[ch];
}

void dpss_me_ro_data_update(void)
{
	int index;
	u32 data32;

	if (dpss_bypass_ko)
		return;

	for (index = 0; index < DPSS_RO_ME_SIZE; index++) {
		data32 = rd(FRC_ME_RO_RGN_T_CONSIS_0 + index);
		dinr_me_buffer_data[index] = data32;
	}
}

void dpss_input_ro_data_update(unsigned int inp_ofrm_vld)
{
	if (dpss_bypass_ko)
		return;

	dinr_input_buffer_data[0] = rd(FRC_FD_DIF_GL);
	dinr_input_buffer_data[1] = inp_ofrm_vld;
}

void pq_update_ro_dae(u32 buf_id, u8 ch)
{
	if (dpss_bypass_ko)
		return;

	if (!dinr_pq_data[ch].dae_ro_buffer) {
		DBG_ERR("dae_ro_buffer is null!\n");
		return;
	}

	dbg_i1("%s:test01 pq\n", __func__);
	dinr_pq_data[ch].frame_index = buf_id;
	dinr_pq_data[ch].dae_ro_buffer->buf_id = buf_id;
	dinr_pq_data[ch].dae_ro_buffer->buf =
		dinr_pq_data[ch].vaddr_ro1 + buf_id * dinr_pq_data[ch].size_ro1;
	dinr_pq_data[ch].me_width = me_width_init;
	dinr_pq_data[ch].me_height = me_height_init;
	dbg_i1("%s:test00pq :%px,%d,%d,%d\n", __func__,
		dinr_pq_data[ch].dae_ro_buffer->buf,
		buf_id * dinr_pq_data[ch].size_ro1,
		dinr_pq_data[ch].me_width, dinr_pq_data[ch].me_height);
}

void pq_update_ro_dae_pd(u32 buf_id, u8 ch)
{
	if (dpss_bypass_ko)
		return;

	if (!dinr_pq_data[ch].dae_ro_pd_buffer) {
		DBG_ERR("dae_ro_pd_buffer is null!\n");
		return;
	}

	dbg_i1("%s:test01 pq\n", __func__);
	//dinr_pq_data[ch].frame_index = buf_id;
	dinr_pq_data[ch].dae_ro_pd_buffer->buf_id = buf_id;
	dinr_pq_data[ch].dae_ro_pd_buffer->buf =
		dinr_pq_data[ch].vaddr_ro1 + buf_id * dinr_pq_data[ch].size_ro1;
	dbg_i1("%s:test00pq %x:%px,%d\n", __func__, buf_id,
		dinr_pq_data[ch].dae_ro_pd_buffer->buf,
		buf_id * dinr_pq_data[ch].size_ro1);
}

void pq_update_ro_dpe(u32 buf_id, u8 ch)
{
	if (dpss_bypass_ko)
		return;

	if (!dinr_pq_data[ch].dpe_ro_buffer) {
		DBG_ERR("dpe_ro_buffer is null!\n");
		return;
	}

	dinr_pq_data[ch].frame_index = buf_id;
	dinr_pq_data[ch].dpe_ro_buffer->buf_id = buf_id;
	dinr_pq_data[ch].dpe_ro_buffer->buf =
		dinr_pq_data[ch].vaddr_ro2 + buf_id * dinr_pq_data[ch].size_ro2;
	dbg_i1("%s:test00pq :%px,%d\n", __func__,
		dinr_pq_data[ch].dpe_ro_buffer->buf,
		buf_id * dinr_pq_data[ch].size_ro2);
}

void pq_update_ro_dblk_h(u32 buf_id, u8 ch)
{
	if (dpss_bypass_ko)
		return;

	if (!dinr_pq_data[ch].dblk_h_ro_buffer) {
		DBG_ERR("dblk_h_ro_buffer is null!\n");
		return;
	}

	dinr_pq_data[ch].frame_index = buf_id;
	dinr_pq_data[ch].dblk_h_ro_buffer->buf_id = buf_id;
	dinr_pq_data[ch].dblk_h_ro_buffer->buf =
		dinr_pq_data[ch].vaddr_ro3 + buf_id * dinr_pq_data[ch].size_ro3;
	dbg_i1("%s:test00pq :%px,%d\n", __func__,
		dinr_pq_data[ch].dblk_h_ro_buffer->buf,
		buf_id * dinr_pq_data[ch].size_ro1);
}

void pq_update_ro_dblk_v(u32 buf_id, u8 ch)
{
	if (dpss_bypass_ko)
		return;

	if (!dinr_pq_data[ch].dblk_v_ro_buffer) {
		DBG_ERR("dblk_v_ro_buffer is null!\n");
		return;
	}

	dinr_pq_data[ch].frame_index = buf_id;
	dinr_pq_data[ch].dblk_v_ro_buffer->buf_id = buf_id;
	dinr_pq_data[ch].dblk_v_ro_buffer->buf =
		dinr_pq_data[ch].vaddr_ro4 + buf_id * dinr_pq_data[ch].size_ro4;
	dbg_i1("%s:test00pq :%px,%d\n", __func__,
		dinr_pq_data[ch].dblk_v_ro_buffer->buf,
		buf_id * dinr_pq_data[ch].size_ro1);
}

void pq_update_ro_me(u32 buf_id, u8 ch)
{
	if (!dinr_pq_data[ch].me_ro_buffer) {
		DBG_ERR("me_ro_buffer is null!\n");
		return;
	}
	if (dpss_bypass_ko)
		return;

	dinr_pq_data[ch].frame_index = buf_id;
	dinr_pq_data[ch].me_ro_buffer->buf_id = buf_id;
	dinr_pq_data[ch].me_ro_buffer->buf = &dinr_me_buffer_data;
	dbg_i1("%s:test00pq :%px\n", __func__,
		dinr_pq_data[ch].me_ro_buffer->buf);
}
#endif /* FUNC_EN_PQ */

void pq_update_ro_input(u32 buf_id, u8 ch)
{
	if (!dinr_pq_data[ch].input_ro_buffer) {
		DBG_ERR("input_ro_buffer is null!\n");
		return;
	}
	if (dpss_bypass_ko)
		return;

	dinr_input_buffer_data[2] = dpss_snr_en;
	dinr_input_buffer_data[3] = dpss_tnr_en;
	dinr_input_buffer_data[4] = dpss_dm_en;
	dinr_pq_data[ch].frame_index = buf_id;
	dinr_pq_data[ch].input_ro_buffer->buf_id = buf_id;
	dinr_pq_data[ch].input_ro_buffer->buf = &dinr_input_buffer_data;
	dbg_i1("%s:test00pq :%px\n", __func__,
		dinr_pq_data[ch].input_ro_buffer->buf);
}

void pq_update_suspend_flag(u32 suspend_flag)
{
	di_db_dm[REG_DM_MAX + 1].val = suspend_flag;
	dbg_i1("%s:pq :%x\n", __func__, suspend_flag);
}

enum vframe_signal_fmt_e dpss_sfmt(struct vframe_s *vf)
{
	if (!vf || dpss_bypass_ko)
		return VFRAME_SIGNAL_FMT_INVALID;

	/* invalid src fmt case */
	if (vf->src_fmt.sei_magic_code != SEI_MAGIC_CD) {
		if ((transfer_characteristic == 14 ||
			transfer_characteristic == 18) &&
			color_primaries == 9) {
			if (cuva)
				vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_CUVA_HLG;
			else
				vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_HLG;
		} else if ((transfer_characteristic == 16) &&
				 ((color_primaries == 9) ||
				  (color_primaries == 2))) {
			if (cuva)
				vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_CUVA_HDR;
			else
				vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_HDR10;
		} else {
			vf->src_fmt.fmt = VFRAME_SIGNAL_FMT_SDR;
		}
	}
	dbg_i1("[%s]  %d\n", __func__, vf->src_fmt.fmt);

	return vf->src_fmt.fmt;
}

void pq_int(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfs)
{
#ifdef FUNC_EN_PQ
	int ch = pch->c.ch;
	struct di_parm_s *de_devp = dpss_pq_data(ch);

	memset((void *)(&dinr_pq_data[ch]), 0, sizeof(dinr_pq_data[0]));

	de_devp->frame_index = vfs->index_disp;
	de_devp->v_type = vfs->type;
	//de_devp->v_sig_fmt = vfs->sig_fmt;
	de_devp->v_width = vfs->width;
	if (de_devp->v_type & VIDTYPE_INTERLACE_TOP)
		de_devp->v_height = vfs->height / 2;
	else
		de_devp->v_height = vfs->height;
	if (vfs->sig_fmt == TVIN_SIG_FMT_CVBS_SECAM)
		de_devp->v_sig_fmt = 1;//secam
	else if (_IS_VDIN_SRC(vfs->source_type))
		de_devp->v_sig_fmt = 2;//vdin
	else
		de_devp->v_sig_fmt = 0;//dos
	if (vfs->bitdepth & BITDEPTH_Y10)
		de_devp->bitdepth = 1;
	else if (vfs->bitdepth & BITDEPTH_Y12)
		de_devp->bitdepth = 2;
	else
		de_devp->bitdepth = 0;
	de_devp->vaddr_ro1 = pch->c.vaddr_ro1;
	de_devp->vaddr_ro2 = pch->c.vaddr_ro2;
	de_devp->vaddr_ro3 = pch->c.vaddr_ro3;
	de_devp->vaddr_ro4 = pch->c.vaddr_ro4;
	de_devp->size_ro1 = pch->c.sml_info->size_ro1;
	de_devp->size_ro2 = pch->c.sml_info->size_ro2;
	de_devp->size_ro3 = pch->c.sml_info->size_grad_h;
	de_devp->size_ro4 = pch->c.sml_info->size_grad_v;

	dae_pq_ro_buffer[ch].buf = de_devp->vaddr_ro1;
	dpe_pq_ro_buffer[ch].buf = de_devp->vaddr_ro2;
	dblk_h_pq_ro_buffer[ch].buf = de_devp->vaddr_ro3;
	dblk_v_pq_ro_buffer[ch].buf = de_devp->vaddr_ro4;
	de_devp->dae_ro_buffer = &dae_pq_ro_buffer[ch];
	de_devp->dae_ro_pd_buffer = &dae_pq_ro_buffer[ch];
	de_devp->dpe_ro_buffer = &dpe_pq_ro_buffer[ch];
	de_devp->dblk_h_ro_buffer = &dblk_h_pq_ro_buffer[ch];
	de_devp->dblk_v_ro_buffer = &dblk_v_pq_ro_buffer[ch];
	de_devp->me_ro_buffer = &pq_me_ro_buffer[0];
	de_devp->input_ro_buffer = &pq_input_ro_buffer[0];
	dinr_input_buffer_data[2] = dpss_snr_en;
	dinr_input_buffer_data[3] = dpss_tnr_en;
	dinr_input_buffer_data[4] = dpss_dm_en;
	atomic_set(&de_devp->pq_dae_unreg, 1);
	atomic_set(&de_devp->pq_dpe_unreg, 1);
	if (di_db_dm[REG_DM_MAX + 1].val) {
		de_devp->reserved3 = 1;
		di_db_dm[REG_DM_MAX + 1].val = 0;
		DBG_INF("%s suspend :%d\n", __func__, de_devp->reserved3);
	}
	de_devp->checksum = "30";
	dbg_i1("%s-1:%d,%d,%d\n", __func__, de_devp->v_width, de_devp->v_height,
		de_devp->bitdepth);
	dbg_i1("%s-2:%px,%px,%d,%px,%px,%d,%px,%px,%px,%px\n", __func__,
		pch->c.vaddr_ro1,
		pch->c.vaddr_ro2, pch->c.sml_info->size_ro1,
		pch->c.vaddr_ro3, pch->c.vaddr_ro4,
		pch->c.sml_info->size_ro2,
		dinr_pq_data[ch].dae_ro_buffer,
		dinr_pq_data[ch].me_ro_buffer,
		dinr_pq_data[ch].input_ro_buffer,
		dinr_pq_data[ch].dae_ro_pd_buffer);
#endif /* FUNC_EN_PQ */
}

bool pq_can_exit(struct dpss_ch_s *pch)
{
	bool ret = false;

#ifdef FUNC_EN_PQ
	struct di_parm_s *de_devp;

	de_devp = dpss_pq_data(pch->c.ch);
	if (atomic_read(&de_devp->pq_dae_unreg) && atomic_read(&de_devp->pq_dpe_unreg))
		ret = true;
#endif
	return ret;
}

/**************************************
 * @DAE/APE
 * @step1:create_instance hw init
 * @step2--READ--WRITE--DAE(N)--READ--ALG--WRITE--DAE(N+1)--READ--ALG--WRITE--DAE(N+2)
 * @dae_hw_init
 * @READ:   dae_fwalg_read
 * @WRITE:  dae_fwalg_write
 * @ALG:    dae_fwalg
 * @step3:destroy_instance
 **************************************/

/***************************************
 * di api for other module *
 **************************************/
/**********************************************************
 **
 * @brief  pq_create_instance
 * @set channel to pq ko
 * @param[ch]  index   instance channel index
 *
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/

int pq_create_instance(u8 ch)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (dipq_api && dipq_api->pq_create_instance) {
		ret = dipq_api->pq_create_instance(ch);
		return ret;
	}
#endif
	return ret;
}

/**********************************************************
 **
 * @brief  pq_destroy_instance
 * @del channel to pq ko
 * @param[ch]  index   instance channel index
 *
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/

int pq_destroy_instance(u8 ch)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (dipq_api && dipq_api->pq_destroy_instance) {
		ret = dipq_api->pq_destroy_instance(ch);
		return ret;
	}
#endif
	return ret;
}

/**********************************************************
 **
 * @brief  dae_fwalg_read after create channel,every frame update
 * @read di/nr ro register to buffer,algorithm use it
 * @param[op]  read/write register
 * @param[pdi_parm] frame width/height/index,read ro update
 * @dae_ro_buffer
 * @dpe_ro_buffer
 * @dblk_ro_buffer
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/

int dae_fwalg_read(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;
	if (pdi_parm->vaddr_null)
		return ret;
	if (dipq_api && dipq_api->dae_read_fwalg) {
		ret = dipq_api->dae_read_fwalg(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

/**********************************************************
 **
 * @brief  dae_fwalg_write after create channel,every frame update
 * @write di/nr register
 * @param[op]  read/write register
 * @param[pdi_parm]  frame width/height/index
 *
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/

int dae_fwalg_write(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (pdi_parm->vaddr_null)
		return ret;

	if (dipq_api && dipq_api->dae_write_fwalg) {
		ret = dipq_api->dae_write_fwalg(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

/**********************************************************
 **
 * @brief  dae_fwalg(algorithm),after create channel,
 * @every frame update
 * @param[op]  read/write register
 * @param[pdi_parm]  write di/nr register
 *
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/

int dae_fwalg(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (pdi_parm->vaddr_null)
		return ret;
	if (dpss_bypass_ko)
		return ret;

	if (dipq_api && dipq_api->fwalg_dae) {
		ret = dipq_api->fwalg_dae(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

int dpe_fwalg_read(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (pdi_parm->vaddr_null)
		return ret;

	if (dipq_api && dipq_api->dpe_read_fwalg) {
		ret = dipq_api->dpe_read_fwalg(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

int dpe_fwalg_write(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (pdi_parm->vaddr_null)
		return ret;

	if (dipq_api && dipq_api->dpe_write_fwalg) {
		ret = dipq_api->dpe_write_fwalg(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

int dpe_fwalg(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (pdi_parm->vaddr_null)
		return ret;

	if (dipq_api && dipq_api->fwalg_dpe) {
		ret = dipq_api->fwalg_dpe(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

int dae_hw_init(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (dipq_api) {
		ret = dipq_api->dae_hw_init(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

int dpe_hw_init(struct di_parm_s *pdi_parm, const struct reg_acc *op)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dpss_bypass_ko)
		return ret;

	if (dipq_api && dipq_api->dpe_hw_init) {
		ret = dipq_api->dpe_hw_init(pdi_parm, op);
		return ret;
	}
#endif
	return ret;
}

/***************************************
 * global define for debug*
 **************************************/
ssize_t dpss_pq_di_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_DI, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_di_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_DI, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_di2_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_DI2, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_di2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_DI2, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_di3_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_DI3, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_di3_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_DI3, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_nr_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_NR, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_nr_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_NR, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_film_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_FILM, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_film_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_FILM, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_film2_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_FILM2, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_film2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_FILM2, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_nrme_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_NRME, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_nrme_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_NRME, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_ne_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_NE, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_ne_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_NE, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_deblock_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_DEBLOCK, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_deblock_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_DEBLOCK, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_snr_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_SNR, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_snr_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_SNR, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr1_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR1, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr1_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR1, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr2_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR2, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR2, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr3_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR3, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr3_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR3, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr4_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR4, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr4_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR4, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr5_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR5, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr5_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR5, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr6_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR6, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr6_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR6, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr7_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR7, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr7_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR7, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_tnr8_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_TNR8, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_tnr8_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_TNR8, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_mnr_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_MNR, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_mnr_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_MNR, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_madi_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_MADI, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_madi_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_MADI, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_madi2_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_MADI2, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_madi2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_MADI2, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_mcdi_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_MCDI, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_mcdi_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_MCDI, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_mcdi2_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_MCDI2, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_mcdi2_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_MCDI2, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

ssize_t dpss_pq_dct_show(const struct class *class, const struct class_attribute *attr,
				char *buf)
{
	int ret = -1;

#ifdef FUNC_EN_PQ
	if (dipq_api && dipq_api->dpss_di_param_show) {
		ret = dipq_api->dpss_di_param_show(PQ_MODE_DCT, buf);
		return ret;
	}
#endif
		return ret;
}

ssize_t dpss_pq_dct_store(const struct class *class, const struct class_attribute *attr,
				const char *buf, size_t count)
{
	int ret = -1;
#ifdef FUNC_EN_PQ
	char	*buf_orig;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (dipq_api && dipq_api->dpss_di_param_store)
		ret = dipq_api->dpss_di_param_store(PQ_MODE_DCT, buf_orig, count);
	kfree(buf_orig);
#endif
	return ret;
}

struct dpss_class_debug_s {
	const char *name;
	unsigned int *parm_value;
	unsigned int parm_cnt;
	unsigned int read_only;
};

static struct dpss_class_debug_s dpss_debugfs_class_files[] = {
	{"tnr_en", &dpss_tnr_en, 1, 0},
	{"snr_en", &dpss_snr_en, 1, 0},
	{"dm_en", &dpss_dm_en, 1, 0},
	{"dblk_en", &dpss_dblk_en, 1, 0},
	{"dct_en", &dpss_dct_en, 1, 0},
	{"cue_en", &dpss_cue_en, 1, 0},
	{"cfr_en", &dpss_cfr_en, 1, 0},
	{"pq_en", &dpss_force_pq, 1, 0},
	{"frclink_en", &enable_mc_link, 1, 0},
	{"frc_bypass", &dpss_frc_bypass, 1, 0},
	{"nr_debug", &dpss_nr_debug, 1, 0},
	{"xlr_en", &dpss_xlr_en, 1, 0},
	{"xlr_side_en", &dpss_xlr_side_en, 1, 0},
	{"me_en", &dpss_me_en, 1, 0},
	{"di_debug", &dpss_di_debug, 1, 0},
	{"force_nr_debug", &dpss_force_nr_debug, 1, 0},
};

static struct dpss_class_debug_s *search_class_param(char *name)
{
	struct dpss_class_debug_s *find_param = NULL;
	int i;

	if (!name) {
		pr_info("%s, NULL param\n", __func__);
		return NULL;
	}
	for (i = 0; i < ARRAY_SIZE(dpss_debugfs_class_files); i++) {
		if (!strncmp(name, dpss_debugfs_class_files[i].name, 32))
			find_param = &dpss_debugfs_class_files[i];
	}

	return find_param;
}

ssize_t dpss_pq_en_show(const struct class *device,
				const struct class_attribute *attr,
				char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "pq_en show:\n");
	len += sprintf(buf + len, "tnr_en=%d\n", dpss_tnr_en);
	len += sprintf(buf + len, "snr_en=%d\n", dpss_snr_en);
	len += sprintf(buf + len, "dm_en=%d\n", dpss_dm_en);
	len += sprintf(buf + len, "dblk_en=%d\n", dpss_dblk_en);
	len += sprintf(buf + len, "dct_en=%d\n", dpss_dct_en);
	len += sprintf(buf + len, "cue_en=%d\n", dpss_cue_en);
	len += sprintf(buf + len, "cfr_en=%d\n", dpss_cfr_en);
	len += sprintf(buf + len, "pq_en=%d\n", dpss_force_pq);
	len += sprintf(buf + len, "frclink_en=%d\n", enable_mc_link);
	len += sprintf(buf + len, "frc_bypass=%d\n", dpss_frc_bypass);
	len += sprintf(buf + len, "nr_debug=%d\n", dpss_nr_debug);
	len += sprintf(buf + len, "xlr_en=%d\n", dpss_xlr_en);
	len += sprintf(buf + len, "xlr_side_en=%d\n", dpss_xlr_side_en);
	len += sprintf(buf + len, "me_en=%d\n", dpss_me_en);
	len += sprintf(buf + len, "di_debug=%d\n", dpss_di_debug);
	len += sprintf(buf + len, "force_nr_debug=%d\n", dpss_force_nr_debug);
	return len;
}

static int parse_param_ex(char *buf_orig, char **parm, int max_cnt)
{
	char *ps, *token;
	unsigned int n = 0;
	char delim1[3] = " ";
	char delim2[2] = "\n";

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		if (n >= max_cnt) {
			pr_info("%s, out of range\n", __func__);
			return n;
		}
		parm[n++] = token;
	}

	return n;
}

ssize_t dpss_pq_en_store(const struct class *class,
			     const struct class_attribute *attr,
			     const char *buf, size_t count)
{
	char *buf_orig, *parm[32];
	unsigned int i, parm_cnt, *parm_value, parse_cnt;
	struct dpss_class_debug_s *find_param = NULL;
	int write = 0; /* 0: read  1: write*/
	int len = 0;
	char *buff = NULL;
	ssize_t ret = count;

	if (!buf)
		return count;

	buff = kmalloc(256, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	memset(parm, 0, sizeof(parm));
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig) {
		ret = -ENOMEM;
		goto free2;
	}
	parse_cnt = parse_param_ex(buf_orig, (char **)&parm, 32);
	if (!parse_cnt) {
		pr_info("need to input parameter(s)\n");
		goto free1;
	}
	if (parse_cnt > 1)
		write = 1;

	find_param = search_class_param(parm[0]);
	if (!find_param) {
		pr_info("cannot find %s\n", parm[0]);
		goto free1;
	}

	parm_cnt = find_param->parm_cnt;
	parm_value = find_param->parm_value;
	len = sprintf(buff, "%s ", parm[0]);
	for (i = 0; i < parm_cnt; i++)
		len += sprintf(buff + len, "%d ", parm_value[i]);
	if (write) {
		if (find_param->read_only) {
			pr_info("%s is read only\n", parm[0]);
			goto free1;
		}
		if ((parse_cnt - 1) < parm_cnt) {
			pr_info("need to input %s and %d value(s)\n",
				find_param->name, parm_cnt);
			goto free1;
		}
		len += sprintf(buff + len, "-> ");
		for (i = 1; i <= find_param->parm_cnt; i++) {
			if (kstrtou32(parm[i], 0, &parm_value[i - 1]) < 0) {
				ret = -EINVAL;
				goto free1;
			}
			len += sprintf(buff + len, "%d ", parm_value[i - 1]);
			if (!strcmp(parm[0], "snr_en")) {
				dinr_input_buffer_data[2] = parm_value[i - 1];
				pr_info("snr_en %d\n", parm_value[i - 1]);
			}
			if (!strcmp(parm[0], "tnr_en")) {
				dinr_input_buffer_data[3] = parm_value[i - 1];
				pr_info("tnr_en %d\n", parm_value[i - 1]);
			}
			if (!strcmp(parm[0], "dm_en")) {
				dinr_input_buffer_data[4] = parm_value[i - 1];
				pr_info("dm_en %d\n", parm_value[i - 1]);
			}
		}
	}
	pr_info("%s\n", buff);

free1:
	kfree(buf_orig);
free2:
	kfree(buff);

	return ret;
}

/***************************************
 * global define for initial,not api for dpss*
 **************************************/
/**********************************************************
 **
 * @brief  dae_hw_init after create channel,
 * @init parameter,only once
 * @param[op]  read/write register
 * @param[pdi_parm]  frame width/height/index,work_mode update
 *
 * @return      0 for success, or fail type if < 0
 *
 **********************************************************/
int RegisterDI_Function(struct di_ext_pq_ops *func, const char *ver)
{
	int ret = -1;
#ifdef FUNC_EN_PQ

	//mutex_lock(&di_tb_mutex);
	DIPQ_INFO("DI: RegDI digfunc %p, func: %p, ver:%s\n", dipq_api,
		func, ver);
	if (!dipq_api && func) {
		dipq_api = func;
		ret = 0;
	}
	//mutex_unlock(&di_tb_mutex);
#endif
	return ret;
}
EXPORT_SYMBOL(RegisterDI_Function);

int UnRegisterDI_Function(struct di_ext_pq_ops *func)
{
	int ret = -1;
#ifdef FUNC_EN_PQ

	//mutex_lock(&di_tb_mutex);
	pr_info("DI:TB UnRegDI: digfunc %p, func: %p\n", dipq_api, func);
	if (func && func == dipq_api) {
		dipq_api = NULL;
		ret = 0;
	}
	//mutex_unlock(&di_tb_mutex);
#endif
	return ret;
}
EXPORT_SYMBOL(UnRegisterDI_Function);

