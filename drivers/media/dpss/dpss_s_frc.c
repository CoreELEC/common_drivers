// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

//#include "register_t6w.h"
//#include "register_t6w_vc.h"

#include "sys_def.h"
#include <linux/module.h>
#include <linux/kfifo.h>
#include <linux/amlogic/gki_module.h>
#include <uapi/amlogic/frc.h>
#include <linux/amlogic/media/vfm/vframe.h>
#include <linux/amlogic/media/vfm/vframe_provider.h>
#include <linux/amlogic/media/vfm/vframe_receiver.h>

#include <linux/amlogic/media/vpu/vpu.h>
/*dma_get_cma_size_int_byte*/
#include <linux/amlogic/media/codec_mm/codec_mm.h>
#include <linux/amlogic/media/di/dpss_interface.h>
#ifdef CONFIG_AMLOGIC_VPU
#include <linux/amlogic/media/vpu/vpu.h>
#endif
#include <linux/amlogic/media/video_sink/vpp.h>
#include <linux/amlogic/media/video_sink/video.h>
#include <linux/amlogic/media/dpss/dpss_frc.h>
#include <linux/amlogic/media/amvecm/amvecm.h>
#ifdef CONFIG_AMLOGIC_MEDIA_ENHANCEMENT_DOLBYVISION
#include <linux/amlogic/media/amdolbyvision/dolby_vision.h>
#else
#include <linux/amlogic/media/amdolbyvision/dolby_vision_ext.h>
#endif
#include <linux/platform_device.h>
#include <linux/amlogic/media/dpss/frc_common_x.h>
#include <linux/amlogic/media/vout/vinfo.h>
#include <linux/amlogic/media/vout/vout_notify.h>
#include <linux/amlogic/media/frame_provider/tvin/tvin.h>
#include <linux/amlogic/media/vrr/vrr.h>
#include "dpss_base.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_hw.h"
#include "dpss_hw_frc.h"
#include "dpss_func.h"
#include "dpss_frc_dbg.h"
#include "dpss_frc_memc_dbg.h"
#include "dpss_s_frc.h"
#include "dpss_frc_rev.h"
#include "dpss_rdma_frc.h"
#include "hw/define.h"
#include "hw/dpss.h"
#include "hw/dpss_phslut.h"
#include "hw/dpss_mc.h"
#include "hw/dpss_lib.h"
#include "drv/d_pq.h"
#include "drv/d_hdr.h"

static struct frc_chip_st frc_st;
static struct dpss_frc_fw_data_s fw_data;	// important 2021_0510
static const char frc_alg_def_ver[] = "alg_ver:default";

struct display_queue display_buf_q;
int enable_mc_link = 1;
int frc_delay_dbg; // for avsync debug

#define N2M_LIST 29
#define DPSS_FRC_RESET_PATH_SKIP_CNT 5

//hdr_flag
#define hdr_color_primaries ((vfm->signal_type >> 16) & 0xff)
#define hdr_transfer_characteristic ((vfm->signal_type >> 8) & 0xff)
#define hdr_signal_cuva ((vfm->signal_type >> 31) & 1)

const char *vid_src_fmt_str[] = {
	"YUV444",
	"YUV422",
	"YUV420",
	"RGB",
	"_NG_",
	"PL_X1",
	"PL_X2",
	"PL_X3",
	"_NG_",
	"_NG_",
	"8b",
	"10b",
	"12b",
	"14b",
	"16b",
	"P010b",
	"10IN12b",
	"_NG_",
	"_NG_",
	"_NG_",
	"CMP_UN",
	"CMP_AFBC",
	"CMP_AFRC",
	"_NG_",
	"_NG_",
	"_NG_",
	"_NG_",
	"_NG_",
	"_NG_",
	"_NG_",
	"P_SRC",
	"I_SRC",
};

const char *stats_fmt_str[] = {
	"IDLE",
	"REQ",
	"INIT",
	"PROC",
	"IRDY",
	"ORDY",
	"NULL",
};

struct n2m_lut_s n2m_table[N2M_LIST] = {
	{12, 48, DPSS_FRC_RATIO_1_4},
	{12, 60, DPSS_FRC_RATIO_1_5},
	{12, 120, DPSS_FRC_RATIO_1_10},
	{15, 120, DPSS_FRC_RATIO_1_8},
	{24, 48, DPSS_FRC_RATIO_1_2},
	{24, 60, DPSS_FRC_RATIO_2_5},
	{24, 120, DPSS_FRC_RATIO_1_5},
	{24, 144, DPSS_FRC_RATIO_1_6},
	{25, 50, DPSS_FRC_RATIO_1_2},
	{25, 60, DPSS_FRC_RATIO_5_12},
	{25, 100, DPSS_FRC_RATIO_1_4},
	{30, 60, DPSS_FRC_RATIO_1_2},
	{30, 120, DPSS_FRC_RATIO_1_4},
	{50, 60, DPSS_FRC_RATIO_5_6},
	{50, 100, DPSS_FRC_RATIO_1_2},
	{50, 120, DPSS_FRC_RATIO_5_12},
	{60, 120, DPSS_FRC_RATIO_1_2},
	{60, 144, DPSS_FRC_RATIO_5_12},
	//t6x dlg
	{12, 240, DPSS_FRC_RATIO_1_20},
	{24, 240, DPSS_FRC_RATIO_1_10},
	{24, 288, DPSS_FRC_RATIO_1_12},
	{25, 200, DPSS_FRC_RATIO_1_8},
	{30, 240, DPSS_FRC_RATIO_1_8},
	{50, 200, DPSS_FRC_RATIO_1_4},
	{60, 240, DPSS_FRC_RATIO_1_4},
	{100, 200, DPSS_FRC_RATIO_1_2},
	{120, 240, DPSS_FRC_RATIO_1_2},
	{144, 288, DPSS_FRC_RATIO_1_2},

	{60, 60, DPSS_FRC_RATIO_1_1},
};

/*****************************************************************************/
#if !(IS_ENABLED(CONFIG_AMLOGIC_MEDIA_FRC))
int frc_dbg_en;
EXPORT_SYMBOL(frc_dbg_en);

int frc_kerdrv_ver;
EXPORT_SYMBOL(frc_kerdrv_ver);
#endif

enum frc_log_level_t frc_module_log_level[DPSS_MODULE_MAX] = {
	FRC_LOG_LEVEL_ERROR,	// ERROR
	FRC_LOG_LEVEL_WARN,	// WARN
	FRC_LOG_LEVEL_DEBUG,	// debug
	FRC_LOG_LEVEL_INFO,	// INFO
	FRC_LOG_LEVEL_NONE,
};
EXPORT_SYMBOL(frc_module_log_level);

const char *frc_module_name[DPSS_MODULE_MAX] = {
	"DPSS_FRC_TOP",
	"DPSS_FRC_INP",
	"DPSS_FRC_DAE",
	"DPSS_FRC_MC",
	"DPSS_FRC_DPE",

	"DPSS_FRC_GAME",
	"DPSS_FRC_OTHER",
	"DPSS_DRV_FIFO",
	"DPSS_CORE_DISP",
	"DPSS_VDIN_INF",

	"FRC_ALG_BBD",
	"FRC_ALG_FILM",
	"FRC_ALG_LOGO",
	"FRC_ALG_ME",
	"FRC_ALG_MC",
	"FRC_ALG_SCENE",
	"FRC_ALG_VP",
	"FRC_ALG_GLB",
};
EXPORT_SYMBOL(frc_module_name);

static struct class_attribute frc_class_attrs[] = {
	__ATTR(debug, 0644, dpss_frc_debug_show, dpss_frc_debug_store),
	__ATTR(reg, 0644, dpss_frc_reg_show, dpss_frc_reg_store),
	__ATTR(tool_debug, 0644, dpss_frc_tool_debug_show,
	       dpss_frc_tool_debug_store),
	__ATTR(buf, 0644, dpss_frc_buf_show, dpss_frc_buf_store),
	__ATTR(param, 0644, dpss_frc_param_show, dpss_frc_param_store),
	__ATTR(other, 0644, dpss_frc_other_show, dpss_frc_other_store),
	__ATTR(probe, 0664, dpss_frc_probe_dbg_show, dpss_frc_probe_dbg_store),
	__ATTR(dump_table, 0644, dpss_frc_dump_reg_table_show,
	       dpss_frc_dump_reg_table_store),
	__ATTR(rdma, 0644, dpss_frc_rdma_show, dpss_frc_rdma_store),
	__ATTR(game, 0644, dpss_frc_game_show, dpss_frc_game_store),
	__ATTR(bbd_ctrl_param, 0644, dpss_frc_bbd_ctrl_param_show,
	       dpss_frc_bbd_ctrl_param_store),
	__ATTR(vp_ctrl_param, 0644, dpss_frc_vp_ctrl_param_show,
	       dpss_frc_vp_ctrl_param_store),
	__ATTR(logo_ctrl_param, 0644, dpss_frc_logo_ctrl_param_show,
	       dpss_frc_logo_ctrl_param_store),
	__ATTR(iplogo_ctrl_param, 0644, dpss_frc_iplogo_ctrl_param_show,
	       dpss_frc_iplogo_ctrl_param_store),
	__ATTR(melogo_ctrl_param, 0644, dpss_frc_melogo_ctrl_param_show,
	       dpss_frc_melogo_ctrl_param_store),
	__ATTR(scene_chg_detect_param, 0644,
	       dpss_frc_scene_chg_detect_param_show,
	       dpss_frc_scene_chg_detect_param_store),
	__ATTR(fb_ctrl_param, 0644, dpss_frc_fb_ctrl_param_show,
	       dpss_frc_fb_ctrl_param_store),
	__ATTR(me_ctrl_param, 0644, dpss_frc_me_ctrl_param_show,
	       dpss_frc_me_ctrl_param_store),
	__ATTR(search_rang_param, 0644, dpss_frc_search_rang_param_show,
	       dpss_frc_search_rang_param_store),
	__ATTR(mc_ctrl_param, 0644, dpss_frc_mc_ctrl_param_show,
	       dpss_frc_mc_ctrl_param_store),
	__ATTR(me_rule_param, 0644, dpss_frc_me_rule_param_show,
	       dpss_frc_me_rule_param_store),
	__ATTR(film_ctrl_param, 0644, dpss_frc_film_ctrl_param_show,
	       dpss_frc_film_ctrl_param_store),
	__ATTR(glb_ctrl_param, 0644, dpss_frc_glb_ctrl_param_show,
	       dpss_frc_glb_ctrl_param_store),
	__ATTR(bad_edit_ctrl_param, 0644, dpss_frc_bad_edit_ctrl_param_show,
	       dpss_frc_bad_edit_ctrl_param_store),
	__ATTR(region_fb_ctrl_param, 0644, dpss_frc_region_fb_ctrl_param_show,
	       dpss_frc_region_fb_ctrl_param_store),
	__ATTR(me_patch_param, 0644, dpss_frc_me_patch_param_show,
	       dpss_frc_me_patch_param_store),
	__ATTR(vp_rule_param, 0644, dpss_frc_vp_rule_param_show,
	       dpss_frc_vp_rule_param_store),
	__ATTR(vp_patch_param, 0644, dpss_frc_vp_patch_param_show,
	       dpss_frc_vp_patch_param_store),
	__ATTR(logo_rule_param, 0644, dpss_frc_logo_rule_param_show,
	       dpss_frc_logo_rule_param_store),
	__ATTR(logo_patch_param, 0644, dpss_frc_logo_patch_param_show,
	       dpss_frc_logo_patch_param_store),
	__ATTR(bbd_rule_param, 0644, dpss_frc_bbd_rule_param_show,
	       dpss_frc_bbd_rule_param_store),
	__ATTR(mc_rule_param, 0644, dpss_frc_mc_rule_param_show,
	       dpss_frc_mc_rule_param_store),
	__ATTR(film_out_ctrl_param, 0644, dpss_frc_film_out_ctrl_param_show,
	       dpss_frc_film_out_ctrl_param_store),

	__ATTR(pq_di, 0644, dpss_pq_di_show, dpss_pq_di_store),
	__ATTR(pq_nr, 0644, dpss_pq_nr_show, dpss_pq_nr_store),
	__ATTR(pq_film, 0644, dpss_pq_film_show, dpss_pq_film_store),
	__ATTR(pq_en, 0644, dpss_pq_en_show, dpss_pq_en_store),
	__ATTR(pq_nrme, 0644, dpss_pq_nrme_show, dpss_pq_nrme_store),
	__ATTR(pq_ne, 0644, dpss_pq_ne_show, dpss_pq_ne_store),
	__ATTR(pq_deblock, 0644, dpss_pq_deblock_show, dpss_pq_deblock_store),
	__ATTR(pq_mnr, 0644, dpss_pq_mnr_show, dpss_pq_mnr_store),
	__ATTR(pq_snr, 0644, dpss_pq_snr_show, dpss_pq_snr_store),
	__ATTR(pq_tnr1, 0644, dpss_pq_tnr1_show, dpss_pq_tnr1_store),
	__ATTR(pq_tnr2, 0644, dpss_pq_tnr2_show, dpss_pq_tnr2_store),
	__ATTR(pq_tnr3, 0644, dpss_pq_tnr3_show, dpss_pq_tnr3_store),
	__ATTR(pq_tnr4, 0644, dpss_pq_tnr4_show, dpss_pq_tnr4_store),
	__ATTR(pq_tnr5, 0644, dpss_pq_tnr5_show, dpss_pq_tnr5_store),
	__ATTR(pq_tnr6, 0644, dpss_pq_tnr6_show, dpss_pq_tnr6_store),
	__ATTR(pq_tnr7, 0644, dpss_pq_tnr7_show, dpss_pq_tnr7_store),
	__ATTR(pq_tnr8, 0644, dpss_pq_tnr8_show, dpss_pq_tnr8_store),
	__ATTR(pq_film2, 0644, dpss_pq_film2_show, dpss_pq_film2_store),
	__ATTR(pq_madi, 0644, dpss_pq_madi_show, dpss_pq_madi_store),
	__ATTR(pq_madi2, 0644, dpss_pq_madi2_show, dpss_pq_madi2_store),
	__ATTR(pq_mcdi, 0644, dpss_pq_mcdi_show, dpss_pq_mcdi_store),
	__ATTR(pq_mcdi2, 0644, dpss_pq_mcdi2_show, dpss_pq_mcdi2_store),
	__ATTR(pq_di2, 0644, dpss_pq_di2_show, dpss_pq_di2_store),
	__ATTR(pq_di3, 0644, dpss_pq_di3_show, dpss_pq_di3_store),
	__ATTR(pq_dct, 0644, dpss_pq_dct_show, dpss_pq_dct_store),
	__ATTR_NULL
};

static inline unsigned int frc_align_up(unsigned int x, unsigned int a)
{
	// return (x + (a - 1)) & ~(a - 1);
	return (x + a) & ~(a - 1);
}

static inline unsigned int frc_align_down(unsigned int x, unsigned int a)
{
	return x & ~(a - 1);
}

unsigned int dpss_dbg_in_fmt_frc;
module_param_named(dpss_dbg_in_fmt_frc, dpss_dbg_in_fmt_frc, uint, 0664);

unsigned int dpss_dbg_o_fmt_frc;
module_param_named(dpss_dbg_o_fmt_frc, dpss_dbg_o_fmt_frc, uint, 0664);

unsigned int dpss_reset_ctrl = 0x10; // = 1; //bit 0 for frc rev
module_param_named(dpss_reset_ctrl, dpss_reset_ctrl, uint, 0664);
unsigned int dpss_ds_ctrl = 2;
module_param_named(dpss_ds_ctrl, dpss_ds_ctrl, uint, 0664);

unsigned int dae_wrpt_full_num = DPSS_HW_LOOP_IN_OUT_BUF_NUB;
module_param_named(dae_wrpt_full_num, dae_wrpt_full_num, uint, 0664);

//extern void dpss_val_2_top(struct PRM_DPSS_TOP *prm_top, struct dpss_user_para_s *prm_user);
//extern void _prm_top_init_buffer(struct PRM_DPSS_TOP *prm_top,
//				struct dpss_ch_s *pch,
//				unsigned int src,
//				unsigned int buf_nub);
//extern void _prm_top_init_vfm(struct dpss_ch_s *pch,
//				struct PRM_DPSS_TOP *prm_top,
//				struct dpss_sub_vf_s *vfms,
//				bool is_ex_di); //user case

// extern unsigned int dpss_dbg_dae_dpe_cfg; //bit 0 for dae  set 4 by console
struct frc_state_s g_frc_work_stats;
unsigned int mc_undone_cnt;
u32 mc_reset_cnt;
unsigned int inp_undone_cnt;
unsigned int me_undone_cnt;
unsigned int vp_undone_cnt;

static void mc_undone_check(void)
{
	bool mc_undone_flag = 0;
	u32 tmpreg_value, mc_uncnt, readback = 0;
	struct frc_debug_s *dbg_st;
	struct frc_chip_st *pchip_st;
	struct frc_interrupt_s *frc_int_st;
	struct frc_state_s *state_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	dbg_st =  &pchip_st->dbg_st;
	state_st = &pchip_st->state_st;
	frc_int_st = &state_st->frc_int_st;

	if (state_st->is_frc_vpp_link == 0)
		return;

	tmpreg_value = rd(FRC_RO_MC_STAT);
	mc_undone_flag = (tmpreg_value >> 12) & 0x1;
	mc_uncnt = (tmpreg_value >> 16) & 0x1fff;
	if (mc_undone_flag) {
		w_reg_bit(DPSS_DPE_MC_TOP_RESET, 1, 9, 1);
		if (mc_uncnt == 0) {
			g_frc_work_stats.undone_stats.frc_mc++;
			if (mc_undone_cnt < 10) {
				dbg_h2("%s cnt %d[inp:%d, dae0:%d, pre_vsync:%d, vsync:%d]\n",
					__func__,
					mc_undone_cnt,
					frc_int_st->inp_int_cnt,
					frc_int_st->dae0_int_cnt,
					frc_int_st->pre_vsync_cnt,
					frc_int_st->frc_vsync_cnt);
				//DBG_INF("mc_undone_stats:%#x 0xE006=%#x,0xE026=%#xn",
				//		tmpreg_value, rd(0xE026));
				dbg_h2("mc_undone_stats:%#x\n", tmpreg_value);
			}
		} else {
			g_frc_work_stats.undone_stats.frc_mc = 0;
		}
		g_frc_work_stats.undone_stats.frc_mc_undone_vcnt = mc_uncnt;
		mc_undone_cnt++;
		readback = rd(FRC_RO_MC_STAT);
		dbg_h2("%s stats:%#x, mc_udo_cnt:%d,  total:%d, clr to read:%#x\n",
			__func__, tmpreg_value, mc_uncnt, mc_undone_cnt, readback);
	} else {
		g_frc_work_stats.undone_stats.frc_mc = 0;
		g_frc_work_stats.undone_stats.frc_mc_undone_vcnt = 0;
	}

	if (dbg_st->ctrl_dbg.mc_rdmif_err_reset_en == 1)
		if (g_frc_work_stats.undone_stats.frc_mc >= 8 &&
			mc_uncnt == 0 &&
			mc_undone_flag &&
			g_frc_work_stats.undone_stats.frc_aa == 0) {
			wr(DPSS_DPE_MC_TOP_RESET, BIT_25);
			DBG_ERR("reset mc_top(rst_cnt:%d,un_cnt:%d, rd:%#x)\n",
				mc_reset_cnt, mc_undone_cnt, readback);
			DBG_ERR("un_occur[inp:%d, dae0:%d, pre_vs:%d, vs:%d]\n",
				frc_int_st->inp_int_cnt, frc_int_st->dae0_int_cnt,
				frc_int_st->pre_vsync_cnt, frc_int_st->frc_vsync_cnt);
			wr(DPSS_DPE_MC_TOP_RESET, BIT_25);
			g_frc_work_stats.undone_stats.frc_mc = 0;
			g_frc_work_stats.undone_stats.frc_aa = 0x55;
			mc_reset_cnt++;
		}
}

static void inp_undone_check(void)
{
	bool inp_undone_flag = 0;
	// static unsigned int inp_undone_cnt = 0;

	inp_undone_flag = rd(FRC_INP_UE_DBG) & 0x3f;
	if (inp_undone_flag) {
		g_frc_work_stats.undone_stats.frc_inp = inp_undone_flag;
		w_reg_bit(FRC_INP_UE_CLR, inp_undone_flag, 0, 6);
		inp_undone_cnt++;
		dbg_h2("%s inp_undone_cnt: %d\n", __func__, inp_undone_cnt);
	}
}

static void me_undone_check(void)
{
	bool me_undone_flag = 0;
	// static unsigned int me_undone_cnt = 0;

	me_undone_flag = rd(FRC_MEVP_RO_STAT0) & 0x1;
	if (me_undone_flag) {
		w_reg_bit(FRC_MEVP_CTRL0, 1, 31, 1);
		g_frc_work_stats.undone_stats.frc_mevp = me_undone_flag;
		me_undone_cnt++;
		dbg_h2("%s me_undone_cnt: %d\n", __func__, me_undone_cnt);
	}
}

static void vp_undone_check(void)
{
	bool vp_undone_flag = 0;
	// static unsigned int vp_undone_cnt = 0;

	vp_undone_flag = rd(FRC_VP_TOP_STAT) & 0x3;
	if (vp_undone_flag) {
		w_reg_bit(FRC_VP_TOP_CLR_STAT, vp_undone_flag, 0, 2);
		g_frc_work_stats.undone_stats.frc_vp = vp_undone_flag;
		vp_undone_cnt++;
		dbg_h2("%s vp_undone_cnt: %d\n", __func__, vp_undone_cnt);
	}
}

void dpss_nr_inp_undone_check(void)
{
	u32 temp = 0;
	static unsigned int nr_inp_undone_cnt;

	temp = rd(DPSS_INP_UNDONE) & 0x3;
	g_frc_work_stats.undone_stats.dpss_inp = temp & 0x1;
	if (g_frc_work_stats.undone_stats.dpss_inp) {
		g_frc_work_stats.undone_stats.dpss_inp_undo_info = temp >> 3;
		w_reg_bit(DPSS_CLR_INP_FLAG, 1, 0, 1);
		nr_inp_undone_cnt++;
		dbg_h2("%s undo_info: %#x\n", __func__,
		       g_frc_work_stats.undone_stats.dpss_inp_undo_info);
	}
}

void dpss_nr_undone_check(void)
{
	u32 temp = 0;

	temp = rd(DPSS_UNDONE_FLAG) & 0x3;
	if (temp & 3) {
		g_frc_work_stats.undone_stats.dpss_dpe = temp & 0x1;
		g_frc_work_stats.undone_stats.dpss_dae = temp >> 1 & 0x1;
		if (g_frc_work_stats.undone_stats.dpss_dae)
			w_reg_bit(DPSS_CLR_DAE_FLAG, 1, 0, 1);
		if (g_frc_work_stats.undone_stats.dpss_dpe)
			w_reg_bit(DPSS_CLR_DPE_FLAG, 1, 0, 1);
		dbg_h2("%s undone : %#x\n", __func__, temp);
	}
}

void dpss_aapps_undone_check(void)
{
	u32 temp = 0;

	temp = rd(FRC_AA_PPS_RO_DBG) & 0x3;
	if (temp) {
		// g_frc_work_stats.undone_stats.frc_aa = temp & 0x3;
		w_reg_bit(FRC_AA_PPS_CTRL, 1, 20, 1);
		dbg_h2("%s undo_info: %#x\n", __func__, temp);
	}
}

void frc_undone_check(void)
{
	inp_undone_check();
	me_undone_check();
	vp_undone_check();
	mc_undone_check();
	// dpss_nr_inp_undone_check();
	// dpss_nr_undone_check();
	// dpss_aapps_undone_check();
}

void dpss_frc_check_undone_stats(void)
{
	PR_FRC("mc_reset_cnt: %d, cur_udo_cnt:%d,  cur_total:%d\n",
		mc_reset_cnt,
		g_frc_work_stats.undone_stats.frc_mc_undone_vcnt,
		g_frc_work_stats.undone_stats.frc_mc);
	PR_FRC("undone cnt[mc:%d  inp:%d  me:%d  vp:%d]\n",
		mc_undone_cnt, inp_undone_cnt, me_undone_cnt, vp_undone_cnt);
}

void check_dpss_frc_status(void)
{
	u32 tmp1, tmp2, tmp3, tmp4;
	struct frc_chip_st *pchip_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st || !pchip_st->state_st.check_frc_status_en)
		return;

	tmp1 = rd(DPSS_FRC_INP_BUF_STATUS) & 0xff;
	tmp2 = rd(DPSS_FRC_DAE_BUF_STATUS) & 0xff;
	tmp3 = rd(DPSS_FRC_MC_IUFF_STATUS) & 0xf;
	tmp4 = rd(VPU_DAE_WRAP_INTF_RO);
	PR_FRC("ro_inp_status:%#x ro_dae_status:%#x ro_mc_status:%#x dae_check:%#x\n",
		tmp1, tmp2, tmp3, tmp4);
	g_frc_work_stats.buf_stats.inp_in_order_idx = rd(FRC_REG_INPUT_FUL_IDX);
	g_frc_work_stats.buf_stats.inp_out_order_idx =
	    rd(FRC_REG_OUT_FRAME_IDX);
	g_frc_work_stats.buf_stats.inp_out_w_order_idx =
	    rd(DPSS_FRC_INP_OBUF_WDATA);

	g_frc_work_stats.buf_stats.dae_in_order_idx = rd(DPSS_FBUF_DAE_FRM_IDX);
	g_frc_work_stats.buf_stats.dae_out_order_idx = rd(FRC_DAE_FRM_IDX);
	g_frc_work_stats.buf_stats.dpe_order_idx = rd(DPSS_FBUF_DPE_FRM_IDX);

	g_frc_work_stats.buf_stats.inp_rotation_idx =
	    rd(FRC_REG_PAT_POINTER) >> 4 & 0xF;
	g_frc_work_stats.buf_stats.dae_rotation_idx =
	    rd(FRC_DAE_IN_FRM_IDX) & 0xF;

	PR_FRC
	    ("%s: inp_idx(%d, %d); dae_idx(%d, %d), dpe_idx(%d), inp(%d),dae(%d)\n",
	     __func__, g_frc_work_stats.buf_stats.inp_in_order_idx,
	     g_frc_work_stats.buf_stats.inp_out_order_idx,
	     g_frc_work_stats.buf_stats.dae_in_order_idx,
	     g_frc_work_stats.buf_stats.dae_out_order_idx,
	     g_frc_work_stats.buf_stats.dpe_order_idx,
	     g_frc_work_stats.buf_stats.inp_rotation_idx,
	     g_frc_work_stats.buf_stats.dae_rotation_idx);

	tmp1 = rd(DPSS_FRC_BUF_ERR_FLAG);	// check overflow
	if (tmp1 != 0) {
		g_frc_work_stats.buf_stats.buf_err = tmp1 >> 28 & 0x1;
		g_frc_work_stats.buf_stats.inp_obuf_free_err = tmp1 >> 24 & 0x1;
		g_frc_work_stats.buf_stats.frc_inp_ibuf_err = tmp1 >> 16 & 0x1;
		g_frc_work_stats.buf_stats.frc_inp_obuf_err = tmp1 >> 12 & 0x1;
		g_frc_work_stats.buf_stats.frc_dae_ibuf_err = tmp1 >> 8 & 0x1;
		g_frc_work_stats.buf_stats.frc_dpe_ibuf_err = tmp1 >> 4 & 0x1;
		g_frc_work_stats.buf_stats.frc_dpe_obuf_err = tmp1 >> 0 & 0x1;
		PR_ERR("%s: frc_buf_overflow is err(%#x)\n", __func__, tmp1);
	}
	tmp2 = rd(DPSS_FBUF_BUF_ERR_FLAG);
	if (tmp2 != 0) {
		g_frc_work_stats.buf_stats.dpe_free_err = tmp2 >> 4 & 0x1;
		g_frc_work_stats.buf_stats.dae_ibuf_err = tmp2 >> 0 & 0x1;
		PR_ERR("%s: nr_buf_overflow is err(%#x)\n", __func__, tmp2);
	}

	tmp1 = rd(DPSS_FRC_PROC_STATUS);
	g_frc_work_stats.proc_stats.frc_inp_proc_st = tmp1 >> 16 & 0x7;
	g_frc_work_stats.proc_stats.frc_dae_proc_st = tmp1 >> 8 & 0x7;
	g_frc_work_stats.proc_stats.frc_dpe_proc_st = tmp1 >> 0 & 0x7;
	g_frc_work_stats.proc_stats.dpss_ctl_idle_stats =
	    rd(FRC_DPSS_CTRL_IDLE);
	PR_FRC("%s: proc_status: %#x, idle_stats:%#x\n", __func__, tmp1,
	       g_frc_work_stats.proc_stats.dpss_ctl_idle_stats);
}

/**********************************************************/
void DPSS_WRITE_FRC_REG(unsigned int reg, unsigned int val)
{
	wr(reg, val);
}
EXPORT_SYMBOL(DPSS_WRITE_FRC_REG);

void DPSS_WRITE_FRC_REG_BY_CPU(unsigned int reg, unsigned int val)
{
	DPSS_WRITE_FRC_REG(reg, val);
}
EXPORT_SYMBOL(DPSS_WRITE_FRC_REG_BY_CPU);

unsigned int DPSS_READ_FRC_REG(unsigned int reg)
{
	return rd(reg);
}
EXPORT_SYMBOL(DPSS_READ_FRC_REG);

void DPSS_WRITE_FRC_BITS(unsigned int reg, unsigned int val,
			      unsigned int start, unsigned int len)
{
	w_reg_bit(reg, val, start, len);
}
EXPORT_SYMBOL(DPSS_WRITE_FRC_BITS);

void DPSS_UPDATE_FRC_REG_BITS(unsigned int reg,
		unsigned int value, unsigned int mask)
{
	unsigned int val;

	value &= mask;
	val = rd(reg);
	val &= ~mask;
	val |= value;
	wr(reg, val);
}
EXPORT_SYMBOL(DPSS_UPDATE_FRC_REG_BITS);
/**********************************************************/
static bool frc_reverse_panel;
static u32 frc_mirror_panel;

static int frc_get_panel_reverse(char *str)
{
	char *ptr = str;

	/* Bootargs are defined as follows:
	 *   "video_reverse=n"
	 *      n=0: No flip
	 *      n=1: X and Y flip
	 *      n=2: X flip
	 *      n=3: Y flip
	 * The corresponding global vars:
	 *   reverse -- 0:No flip  1.X and Y flip
	 *    mirror -- 0:No flip  1:X flip 2:Y flip
	 */
	if (strstr(ptr, "1")) {
		frc_reverse_panel = true;
		frc_mirror_panel = DPSS_NO_MIRROR;
	} else if (strstr(ptr, "2")) {
		frc_reverse_panel = false;
		frc_mirror_panel = DPSS_H_MIRROR;
	} else if (strstr(ptr, "3")) {
		frc_reverse_panel = false;
		frc_mirror_panel = DPSS_V_MIRROR;
	} else {
		frc_reverse_panel = false;
		frc_mirror_panel = DPSS_NO_MIRROR;
	}

	return 0;
}

__setup("video_reverse=", frc_get_panel_reverse);
/**********************************************************/

static void frc_drv_initial(struct dpss_dev_s *devp)
{
	int idx = 0;
	//struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct dpss_frc_fw_alg_ctrl_s *frc_fw_alg_ctrl;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_data_s *pdata;
	struct frc_work_s *work_st;
	struct frc_debug_s *dbg_st;
	struct dpss_game_state_s *game_st;
	struct dpss_game_dly_s *dly_st;

	if (!devp || !devp->data_l) {
		PR_ERR("%s: dpss_devp null\n", __func__);
		return;
	}
	pdata = (struct dpss_data_s *)devp->data_l;
	devp->pchip_st = &frc_st;
	devp->fw_data = &fw_data;
	pchip_st = devp->pchip_st;
	state_st = &pchip_st->state_st;
	dbg_st =  &pchip_st->dbg_st;
	work_st = &devp->pchip_st->work_st;
	game_st = &pchip_st->game_st;
	dly_st = &game_st->dly_st;
	INIT_WORK(&work_st->print_work, frc_dbg_table_print);
	for (idx = 0; idx < DPSS_MODULE_MAX; idx++)
		frc_module_log_level[idx] = FRC_LOG_LEVEL_DEBUG;

	memset(devp->fw_data, 0, (sizeof(struct dpss_frc_fw_data_s)));
	strncpy(&fw_data.frc_alg_ver[0], &frc_alg_def_ver[0],
		strlen(frc_alg_def_ver));

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data) {
		frc_top = &pfw_data->frc_top_type;
		frc_fw_alg_ctrl =
			(struct dpss_frc_fw_alg_ctrl_s *)&pfw_data->frc_fw_alg_ctrl;
	} else {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;		// add abnormal case
	}
	frc_top->hsize = 1920;
	frc_top->vsize = 1088;
	frc_top->auto_align_en = 1;
	frc_top->align_hmode = 0;
	frc_top->align_vmode = 0;
	frc_top->dpss_mode = (enum DPSS_WORK_MODE)DPSS_FRC_MODE;
	frc_top->in_out_ratio = (enum frc_ratio_mode_type)DPSS_FRC_RATIO_1_1;
	frc_top->film_mode =  (enum en_drv_film_mode)DPSS_VIDEO;
	frc_top->mc_auto_en = 0;
	frc_top->dpss_nr_byp = 1;
	frc_top->mc_bypass = 0;
	frc_top->disp_pat_en = 0;
	frc_top->frc_ds_scale_en = 0;
	frc_top->frc_fbuf_only = 0;
	frc_top->badedit_en = 1;
	frc_top->force_mix = 1;
	frc_top->me_mc_link_en = 0;
	frc_top->mc_skip_mode = 0; // b10:h/2,v/2, b0:v/2, b1:h/2
	frc_top->frc_input_n = 1;
	frc_top->frc_output_m = 1;
	state_st->unformat_bypass = 0;
	state_st->special_format = 0;
	// state_st->mc_cut_position = 1;
	state_st->mc_cut_position = 0;
	state_st->pre_vsync_offset = 0;
	state_st->mc_lp_mode = 0;
	state_st->use_inp_big = 0x10;
	state_st->detect_speed = 0;
	state_st->detect_threshold = 3;
	state_st->dst_buf_th = 6;
	state_st->enable_frclink_cnt = 2;
	state_st->first_put_vfm = true;
	state_st->frc_only_rst = 0x3;
	state_st->frc_off_delay_vs_cnt = 1;
	frc_top->memc_enable = 2;
	frc_top->frc_memc_level = 10;
	frc_top->frc_deblur_level = 10;
	frc_top->mc_cut_en = 1;
	frc_fw_alg_ctrl->bbd_en = 1;
	frc_fw_alg_ctrl->vp_en = 1;
	frc_fw_alg_ctrl->iplogo_en = 1;
	frc_fw_alg_ctrl->melogo_en = 1;
	frc_fw_alg_ctrl->frc_me_en = 1;
	dbg_st->ctrl_dbg.mc_rdmif_err_reset_en = 1;
	dbg_st->ctrl_dbg.frc_dae_div4 = 1;
	// frc game
	dly_st->vtotal = 2250;
	dly_st->vsp_dly = 700;
	dly_st->pre_dly = 700;
	dly_st->vmin_num_adj = 20;
	dly_st->vmax_num_adj = 50;
	game_st->vrr_drop_cnt = 10;
	game_st->video_mute_cnt = 15;
	game_st->game_deblur = 0x40;
	game_st->delay_enable_cnt = 2;
	game_st->mc_link_en = true;
	game_st->game_vrr_en = true;
	game_st->inp_ctrl = true;
	game_st->game_dae_byp_en = false;
	set_dpss_rdma_reset_enable(1);

	frc_dbg(DPSS_FRC_TOP, "%s init_version_20250905\n", __func__);
	if (pdata && pdata->mdata) {
		if (pdata->mdata->ic_id == IC_ID_T6W) {
			frc_st.chip = ID_T6W;
			frc_st.encl_frc_ctrl = ENCL_FRC_CTRL;
			frc_st.venc_vrr_ctrl = VENC_VRR_CTRL;
			frc_st.venc_vrr_adj_lmt = VENC_VRR_ADJ_LMT;
			frc_st.venc_vrr_ctrl_1 = VENC_VRR_CTRL1;
			frc_st.venc_ro_vrr = VENC_RO_VRR;
		} else if (pdata->mdata->ic_id == IC_ID_T6X) {
			frc_st.chip = ID_T6X;
			// dpss_light_chg = 0;
			frc_st.encl_frc_ctrl = ENCL_FRC_CTRL_T6X;
			frc_st.venc_vrr_ctrl = VENC_VRR_CTRL_T6X;
			frc_st.venc_vrr_adj_lmt = VENC_VRR_ADJ_LMT_T6X;
			frc_st.venc_vrr_ctrl_1 = VENC_VRR_CTRL1_T6X;
			frc_st.venc_ro_vrr = VENC_RO_VRR_T6X;
		} else { // for further
			frc_st.chip = ID_T6W;
			frc_st.encl_frc_ctrl = ENCL_FRC_CTRL;
		}
		frc_dbg(DPSS_FRC_TOP, "ic_id=%d, chip=%d\n",
				pdata->mdata->ic_id, frc_st.chip);
	}
	frc_top->chip = frc_st.chip;
	// frc_fw_alg_init();
	dpss_frc_get_init_csc();
	mc_reset_cnt = 0;
}

void dpss_frc_prob(struct dpss_dev_s *devp)
{
	int ret = 0, i;

	if (!devp) {
		PR_ERR("%s: devp null\n", __func__);
		goto fail_class_create_file;
	}
	if (IS_ERR(devp->pclss)) {
		ret = PTR_ERR(devp->pclss);
		PR_ERR("%s: class is err(%d)\n", __func__, ret);
		devp->pclss = class_create(CLASS_NAME);
		if (IS_ERR(devp->pclss)) {
			ret = PTR_ERR(devp->pclss);
			PR_ERR("%s: failed to create class(%d)\n", __func__,
			       ret);
			goto fail_class_create_file;
		}
	}
	for (i = 0; frc_class_attrs[i].attr.name; i++) {
		ret = class_create_file(devp->pclss, &frc_class_attrs[i]);
		// pr_info("%s: pclss:%p, attrs[%d].name:%s\n", __func__,
		// devp->pclss, i, frc_class_attrs[i].attr.name);
		if (ret < 0) {
			PR_ERR("%s: failed to create attr file %s\n",
			       __func__, frc_class_attrs[i].attr.name);
			goto fail_attr_create;
		}
	}
	// disable mc_sub_mif
	w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0x0, 20, 8);
	frc_drv_initial(devp);
	dpss_frc_sync_status(1);

#ifdef USE_FRC_PRE_VS_RDMA
	dpss_rdma_init();
#endif
	return;
fail_attr_create:
	while (--i >= 0)
		class_remove_file(devp->pclss, &frc_class_attrs[i]);
	return;
fail_class_create_file:
	return;
}

void dpss_frc_remove(void)
{
#ifdef USE_FRC_PRE_VS_RDMA
	dpss_rdma_exit();
#endif
}

unsigned int dpss_dbg_pre_dly_dae = 0x1500001;
module_param_named(dpss_dbg_pre_dly_dae, dpss_dbg_pre_dly_dae, uint, 0664);

void hw_init_part1(struct vframe_s *vfrm)
{
	struct frc_chip_st *pchip_st;
	u32 vsync_lines, real_vtotal; // 8cc
	struct frc_state_s *state_st;
	u32 input_height;
	struct vinfo_s *vinfo = get_current_vinfo();
	u32 out_frm_rate = 0;
	u32 out_4k2k_120_up_flag;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st || !vinfo) {
		DBG_ERR("%s pchip_st or vinfo is null\n", "hw_init_part");
		return;
	}

	if (pchip_st->encl_frc_ctrl == 0) {
		DBG_ERR("%s encl_frc_ctrl reg null\n", __func__);
		return;
	}

	if (show_dpss_rdma_reset_status() == 1) {
		w_reg_bit(DPSS_RDMA_CTRL, 1, 1, 1);
		w_reg_bit(DPSS_RDMA_CTRL, 0, 1, 1);
	}

	if (vinfo->sync_duration_den != 0)
		out_frm_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;

	//if (pchip_st->chip == ID_T6X)
	//	real_vtotal = rd_vc(ENCL_VIDEO_MAX_LNCNT_T6X) & 0xFFFF;
	//else if (pchip_st->chip == ID_T6W)
	//	real_vtotal = rd_vc(ENCL_VIDEO_MAX_LNCNT);
	//else
	//	real_vtotal = 0x466;
	state_st = &pchip_st->state_st;
	real_vtotal = vinfo->vtotal;
	vsync_lines = vinfo->vtotal / 2;
	dbg_f1("%s:real_vtotal %d, out_frm_rate:%d\n",
			__func__, real_vtotal, out_frm_rate);

	if (pchip_st->chip == ID_T6X) {
		if (vinfo->height == 2160 && out_frm_rate > 99)
			out_4k2k_120_up_flag = 1;
		else if (vinfo->height == 1080 &&
				out_frm_rate > 199)
			out_4k2k_120_up_flag = 1;
		else
			out_4k2k_120_up_flag = 0;
	} else {
		out_4k2k_120_up_flag = 0;
	}
	if (vinfo->height > real_vtotal || real_vtotal == 0)
		real_vtotal = 0x466;

	if (vfrm->type & VIDTYPE_COMPRESS)
		input_height = vfrm->compHeight;
	else
		input_height = vfrm->height;

	if (pchip_st->state_st.pre_vsync_offset) {
		vsync_lines =
			pchip_st->state_st.pre_vsync_offset;
	} else if (out_4k2k_120_up_flag == 0) {
		if (input_height < 500)
			vsync_lines = 0x299;
		else if (input_height < 730)
			vsync_lines = 0x320;
		else
			vsync_lines = real_vtotal / 3 + 0x150;
	} else if (out_4k2k_120_up_flag == 1) {
		if (input_height < 730)
			vsync_lines = 0x320;
		else
			vsync_lines = real_vtotal / 3 + 0x150;
	}

	if (vsync_lines > vinfo->height)
		DBG_ERR("%s:vsync_lines %d, panel_height:%d\n",
				__func__, vsync_lines, vinfo->height);
	w_reg_bit_vc(pchip_st->encl_frc_ctrl, vsync_lines, 0, 16);//cfg_vrr_vtotal,cfg_frc_st_ln
	// w_reg_bit(DPSS_TOP_PER_DLY, 0x1, 0, 16);  //pre_vsync dae_dly_num
	// w_reg_bit(DPSS_TOP_PER_DLY, 0x150, 16, 16);   //pre_vsync dpe_dly_num
	wr(DPSS_TOP_PER_DLY, dpss_dbg_pre_dly_dae);
	//w_reg_bit(DPSS_TOP_SOFT_RST , 0xfff,1,12);//top initial reset
	w_reg_bit(DPSS_TOP_SYNC_RST, 0x1ff, 0, 9);// pls_ctrl_syn_rst  //for some regs latch
	dbg_f1("%s:vsync_line %d\n", __func__, vsync_lines);
}

// extern unsigned int g_dpss_dbg_vfm_in;
//dpss_prm_init
//0:nr_frc path 1:nr_di path
void _prm_top_init_frc(struct dpss_ch_s *pch, struct PRM_DPSS_TOP *prm_top,
		       int path_id)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	unsigned int fmt = YUV422, plan = PLANAR_X2, bit = BIT_010;
	unsigned int i_p, cmpr;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;

	if (!pchip_st) {
		dbg_h0("%s pchip_st is null\n", __func__);
		return;
	}
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	dbg_h1("%s:path_id =%d\n", __func__, path_id);
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data) {
		frc_top = &pfw_data->frc_top_type;
	} else {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;		// add abnormal case
	}

	//ary add:
	prm_top->dae_dpe_cfg_en = dpss_dbg_dae_dpe_cfg;
	dbg_h1("dae_dpe_cfg_en = %d\n", prm_top->dae_dpe_cfg_en);
	//---------
	if (path_id == 0) {
		prm_top->reverse[0] = 0;
		prm_top->reverse[1] = 0;
		prm_top->dae_dsx_scale = 1;
		prm_top->dae_dsy_scale = 1;
		prm_top->mvx_div_mode = 1;
		prm_top->mvy_div_mode = 1;
		prm_top->dpe_slc_num = 1;
		prm_top->dpe_dw_dsx = 1;
		prm_top->dpe_dw_dsy = 1;
		prm_top->dpe_mc_phsx2 = 0;	//set_need_check //ary set 1. ? if 0:vdin; 1:tbc?
		prm_top->auto_alig_en = frc_top->auto_align_en;
		prm_top->alig_hmode = frc_top->align_hmode;
		prm_top->alig_vmode = frc_top->align_hmode;
		prm_top->org_hsize = 0xffff;
		prm_top->film_mode = (enum DPSS_FILM_MODE)frc_top->film_mode;
		prm_top->frc_ratio = (enum DPSS_FRC_RATIO)frc_top->in_out_ratio;
		prm_top->dpss_mode = frc_top->dpss_mode; //DPSS_FRC_MODE;
		prm_top->badedit_en	= frc_top->badedit_en;
	}

	fmt = YUV422;
	plan = PLANAR_X1;
	bit = BIT_010;
	i_p = IS_PSRC;
	if (pch->d->is_10bit)
		bit = BIT_010;
	if (pch->d->is_afbcd) {
		if (pch->d->is_afrcd) {
			cmpr = CMPR_AFRC;
			plan = PLANAR_X2;
		} else {
			cmpr = CMPR_AFBC;
			plan = PLANAR_X1;
		}
	} else {
		cmpr = CMPR_UN;
		plan = PLANAR_X2;
		fmt = YUV422;
	}

	prm_top->frc_ds_scale_en   = frc_top->frc_ds_scale_en;
	prm_top->mc_skip_mode   = frc_top->mc_skip_mode;
	pch->d->frc_ds_scale_en = prm_top->frc_ds_scale_en;
	pch->d->mc_skip_mode = prm_top->mc_skip_mode;
	if (prm_top->frc_ds_scale_en == 1 && cmpr != CMPR_UN) {
		cmpr = CMPR_UN;
		state_st->compr_sel = DDR_MIF;
		plan = PLANAR_X1;
	}
	if (pch->d->fmt == YUV420)
		fmt = YUV420;
	else if (pch->d->fmt == YUV422)
		fmt = YUV422;
	else
		fmt = YUV444;

	if (fmt == YUV420 && cmpr == CMPR_UN)
		prm_top->frc_vfcd_vfmt = 1;
	else
		prm_top->frc_vfcd_vfmt = 0;

	if (pch->d->is_i || pch->d->proc_as_i)
		i_p = IS_ISRC;
	else
		i_p = IS_PSRC;

	dbg_h1("src fmt= %s, bit= %s, plane= %s, cmpr= %s\n",
			vid_src_fmt_str[fmt],
			vid_src_fmt_str[bit],
			vid_src_fmt_str[plan],
			vid_src_fmt_str[cmpr]);
	if (dpss_dbg_in_fmt_frc) {
		i_p = (dpss_dbg_in_fmt_frc >> 24) & 0xff;
		fmt = (dpss_dbg_in_fmt_frc >> 16) & 0xff;
		plan = (dpss_dbg_in_fmt_frc >> 8) & 0xff;
		bit = (dpss_dbg_in_fmt_frc) & 0xff;
		dbg_h1("frc_dbg set fmt= %s, bit= %s, plane= %s, cmpr= %s\n",
			vid_src_fmt_str[fmt],
			vid_src_fmt_str[bit],
			vid_src_fmt_str[plan],
			vid_src_fmt_str[cmpr]);
	}
	prm_top->src_fs_fmt = fmt_cfg(fmt, plan, bit, cmpr, i_p);
	// fmt = YUV422; // YUV422; //  YUV420
	plan = PLANAR_X1;
	bit = BIT_008;
	i_p = IS_PSRC;
	cmpr = CMPR_UN;
	dbg_h1("src dw fmt= %s, bit= %s, plane= %s, cmpr= %s\n",
			vid_src_fmt_str[fmt],
			vid_src_fmt_str[bit],
			vid_src_fmt_str[plan],
			vid_src_fmt_str[cmpr]);
	prm_top->src_ds_fmt	=
		fmt_cfg(fmt, plan, bit, cmpr, i_p); //to-do

	if (dpss_en_afbc) {
		prm_top->nro_fs_fmt =
		    fmt_cfg(YUV420, PLANAR_X1, BIT_008, CMPR_AFBC, IS_PSRC);
	} else {
		prm_top->nro_fs_fmt =
		    fmt_cfg(YUV420, PLANAR_X2, BIT_008, CMPR_UN, IS_PSRC);
	}
	prm_top->nro_ds_fmt =
	    fmt_cfg(YUV422, PLANAR_X1, BIT_010, CMPR_UN, IS_PSRC);

//      if (dpss_dbg_o_fmt) {
//              uncm = (dpss_dbg_o_fmt >> 24) & 0xff;
//              fmt = (dpss_dbg_o_fmt >> 16) & 0xff;
//              plan = (dpss_dbg_o_fmt >> 8) & 0xff;
//              bit = (dpss_dbg_o_fmt) & 0xff;
//              prm_top->nro_fs_fmt     = fmt_cfg(fmt,plan,bit,uncm,IS_PSRC);
//              dbg_h1("o:fmt:%d,%d,%d", fmt, plan, bit);
//      }

	prm_top->dpe_game_mode = game_st->game_n2m | game_st->game_film;
	prm_top->me_mc_link_en = frc_top->me_mc_link_en;	//me_mc_link_en
	prm_top->dae_step_sel = 2;
	prm_top->dolby_mode = DOLBY_WRAP_BYPS;
	prm_top->pvsync_intr_en = 1;

	prm_top->mc_setting.luma_srng_mode = SRNG_NORMAL;
	prm_top->mc_setting.chrm_srng_mode = SRNG_NORMAL;

	prm_top->frm_hsize_sel = 0;	//3;//frame hsize align mode
	prm_top->mif_mmu_mode_en = 0;
	prm_top->ds_mif_mmu_mode_en = 0;

	prm_top->comp_setting.vdin_mmu_mode_en = 0;	//0:disable 1:enable
	//prm_top->comp_setting.mmu_mode_en         = 0;//0:disable 1:enable
	prm_top->comp_setting.mmu_mode_en = pch->d->is_afbcd ? 1 : 0;
	prm_top->comp_setting.mmu_page_mode = 0;	//0:4k page 1:8k page
	prm_top->comp_setting.reg_444to422_mode = 0;	//filter mode
	prm_top->comp_setting.vfce_in_overlap = 0;	//vfce input overlap
	prm_top->comp_setting.src_comp_switch = 0;	//afrc afbc switch
	prm_top->comp_setting.src_switch_num = 3;	//switch num
	prm_top->comp_setting.nro_comp_switch = 0;	//afrc afbc switch
	prm_top->comp_setting.nro_switch_num = 3;	//switch num
	prm_top->comp_setting.afrc_head_mode = 1; //dpss_afrc_head;	//0:wo header 1: wi header
	prm_top->comp_setting.afrc_dict_mode_y = 0;	//
	prm_top->comp_setting.afrc_dict_mode_c = 0;	//
	prm_top->comp_setting.afrc_target_byte_y = 24; //dpss_afrc;	//target byte
	prm_top->comp_setting.afrc_target_byte_c = 24; //dpss_afrc;	//target byte
	prm_top->comp_setting.vfcd_rev_mode       = frc_top->frc_rev_mode;//rev mode
	prm_top->comp_setting.vfcd_h_skip = 0;	//skip
	prm_top->comp_setting.vfcd_h_skip = 0;	//skip
	prm_top->comp_setting.fmt444_comb = 0;	//comb 8bit 444
	prm_top->comp_setting.dpe_vfce_num = 0;	//vfce start num
	prm_top->comp_setting.dpe_vfcd_num = 1;	//vfcd start num

	prm_top->dp_set_rev_mode = frc_top->frc_rev_mode;
	prm_top->fnr_sw_mode = 0;
	prm_top->vdi_sw_mode = 0;
	prm_top->sw_tbc_mode = 0;
	prm_top->sw_ctrl_en = 0;
	prm_top->src_is_1080p_nods = frc_top->src_is_1080p_nods;
	prm_top->nr_hscale_on = 0;
	// prm_top->frc_fbuf_only = frc_top->frc_fbuf_only;
	prm_top->nr_double_rst_mode = 1;
	prm_top->bbd_only = 0;
	prm_top->bbd_parallel = 0;
	prm_top->cut_win_en = 0;	// dpss_frc_cut_win_en;
	prm_top->dpe_ro_wdma_en = 0;
	prm_top->lcevc_en = 0;
	prm_top->dpe_dw_off = 0;
	prm_top->nr_aapps_up = 0;
	prm_top->nr_aapps_ds = 0;
	prm_top->nr_aapps_on = 0;
	prm_top->nr_aapps_mode = 0;
	prm_top->di_interlace = 0;	//yu.zong 2024-12-06
	prm_top->dpss_nr_byp = frc_top->dpss_nr_byp;
	dbg_h2("%s\n", __func__);
}

static void frc_state_init(void)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;

	if (!pchip_st) {
		dbg_h0("%s pchip_st is null\n", __func__);
		return;
	}
	state_st = &pchip_st->state_st;

	state_st->frc_init = true;
	state_st->need_chg_trig_pos = false;
	state_st->trig_pos_chg = false;
	state_st->frc_src = FROM_DEC;
	state_st->src0_disp_obuf_rdy = 0;
	state_st->mc_bypass = true;
	state_st->have_update_vfcd = false;
	state_st->mc_bypass_cnt = state_st->force_mc_byp_cnt ? state_st->force_mc_byp_cnt : 1;
	state_st->frc_en = true;
	state_st->is_frc_vpp_link = false;
	state_st->need_set_phase0 = false;
	state_st->mc_set_phase0 = false;
	state_st->mc_bypass_always = 0;
	state_st->is_dos = true;
	state_st->check_fallback = 0;
	state_st->enable_last_drop = false;
	state_st->mc_last_idx = 0xff;
	state_st->is_pause_state_last_frmae = false;
	state_st->need_drop_dd = false;
	state_st->is_seek_bar = false;
	state_st->dae_ready = false;
	state_st->dpe_ready = false;
	state_st->mc_last_ready = false;
	state_st->dpe_mix = false;
	state_st->mv_buf_idx = 0;
	state_st->bypass_chg = false;
	state_st->mc_phase0_rdma = false;
	state_st->mc_link_available = false;
	state_st->phs_swth = false;
	state_st->dae0_available_cnt = 0;
	state_st->fast_pat_set_n2m = false;
	state_st->fast_pat_set_phase0 = false;
	state_st->disable_mc_link_intf = false;
	state_st->display_buf_info_cnt = 0;
	state_st->check_light_chg_mc = 0;
	state_st->need_force_mc_pre = 0;
	if (state_st->force_n2m == 0)
		memset(&state_st->n2m_status, 0, sizeof(state_st->n2m_status));
	memset(&state_st->me_pcn_st, 0, sizeof(state_st->me_pcn_st));
	memset(&state_st->mc_disp_st, 0, sizeof(state_st->mc_disp_st));
	memset(&pchip_st->win_st, 0, sizeof(struct frc_cut_win_s));
	memset(&pchip_st->vinfo_st, 0, sizeof(struct disp_screen_vinfo_s));
	memset(&state_st->frc_int_st, 0, sizeof(state_st->frc_int_st));
	memset(&g_frc_work_stats, 0, sizeof(struct frc_state_s));
	vpu_initqueue(&inp_bufQ);
}

static void frc_compress_fmt_parse(struct dpss_sub_vf_s *vfs)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;

	if (!pchip_st || !vfs)
		return;

	state_st = &pchip_st->state_st;

	if (prm_top->dpss_mode != DPSS_FRC_MODE) {
		if (prm_top->nro_fs_fmt.src_cmpr == CMPR_UN)
			state_st->compr_sel = DDR_MIF;
		else if (prm_top->nro_fs_fmt.src_cmpr == CMPR_AFRC)
			state_st->compr_sel = DDR_AFRC;
		state_st->big_fmt = prm_top->nro_fs_fmt.src_fmt;
		state_st->big_bits = prm_top->nro_fs_fmt.src_bit;
		state_st->big_plane = prm_top->nro_fs_fmt.src_plan;
	} else {
		if (VFM_IS_COMP_MODE(vfs->type))
			if (VFM_IS_COMP_AFRC(vfs->type_ext))
				state_st->compr_sel = DDR_AFRC;
			else
				state_st->compr_sel = DDR_AFBC;
		else
			state_st->compr_sel = DDR_MIF;

		if (VFM_IS_FMT_420(vfs->type))
			state_st->big_fmt = YUV420;
		else if (VFM_IS_FMT_422(vfs->type))
			state_st->big_fmt = YUV422;
		else if (VFM_IS_FMT_444(vfs->type))
			state_st->big_fmt = YUV444;

		if (vfs->bitdepth & BITDEPTH_Y10)
			state_st->big_bits = BIT_010;
		else
			state_st->big_bits = BIT_008;

		state_st->big_plane = vfs->plane_num;
	}
	dbg_h2("frc compr_sel:%d\n", state_st->compr_sel);
}

extern unsigned int dpss_dbg_top_cfg0;
void init_frc_pre_light(struct vframe_s *vf)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;

	if (!pchip_st)
		return;

	state_st = &pchip_st->state_st;
	state_st->dae0_bypass_mode = 1;
	state_st->reverse_need_update = false;

	pchip_st->mc_pix_rmif.uv_swap = false;
	pchip_st->mc_pix_rmif.reverse[0] = 0;
	pchip_st->mc_pix_rmif.reverse[1] = 0;

	pchip_st->prm_mc.mc_rev_mode = 0;

	frc_transform_check(vf);
}

void init_frc_pre(struct dpss_sub_vf_s *vfs, struct dpss_ch_s *pch)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top; //ary need check
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top = NULL;
	struct dpss_frc_fw_alg_ctrl_s *frc_fw_alg_ctrl = NULL;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;
	struct frc_debug_s *dbg_st;

	if (!pchip_st)
		return;
	state_st = &pchip_st->state_st;
	dbg_st =  &pchip_st->dbg_st;
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;
	}
	frc_top = &pfw_data->frc_top_type;
	if (!frc_top) {
		DBG_ERR("%s: frc_top is null\n", __func__);
		return;
	}
	if (pps_out_w && pps_out_h) {
		frc_top->hsize = pch->c.prm_top.is_pps ? pps_out_w : prm_top->frm_hsize;
		frc_top->vsize = pch->c.prm_top.is_pps ? pps_out_h : prm_top->frm_vsize;
	} else {
		frc_top->hsize = pch->c.prm_top.is_pps ?
			prm_dpe->dpe_nr_size.pps_hsize : prm_top->frm_hsize;
		frc_top->vsize = pch->c.prm_top.is_pps ?
			prm_dpe->dpe_nr_size.pps_vsize : prm_top->frm_vsize;
	}
	frc_top->auto_align_en = 0;
	dbg_i0("%s:is_pps ch=%d,=%d,=%d\n", __func__,
		pch->c.ch, pch->c.prm_top.is_pps, pch->c.prm_top.is_di2pps);

	frc_top->motion_ctrl = prm_top->amdv_2_frc_frm;
	frc_top->mc_hsize = prm_top->frm_hsize;
	frc_top->mc_vsize = prm_top->frm_vsize;
	frc_top->crop_top = 0;
	frc_top->crop_left = 0;
	frc_top->crop_bottom = 0;
	frc_top->crop_right = 0;
	frc_top->need_dpe_mix = 0;
	frc_top->dpe_mix_cnt = 0;

	prm_top->mc_auto_en = frc_top->mc_auto_en;

	if (pch->c.prm_top.is_di2pps) {
		prm_top->mc_auto_en = 0;
		dbg_h2("dis mc_auto_en %d\n", prm_top->mc_auto_en);
	}

	mc_undone_cnt = 0;
	inp_undone_cnt = 0;
	me_undone_cnt = 0;
	vp_undone_cnt = 0;
	det_film_mode = 0;
	det_film_phs = 0;
	det_filmmode_chg = 0;
	bbd_init = 1;
	frc_state_init();
	display_init_queue(&display_buf_q);
	frc_compress_fmt_parse(vfs);
	frc_fw_alg_ctrl = &pfw_data->frc_fw_alg_ctrl;
	prm_top->frc_melogo_en = frc_fw_alg_ctrl->melogo_en;
	prm_top->frc_iplogo_en = frc_fw_alg_ctrl->iplogo_en;
	prm_top->frc_me_en = frc_fw_alg_ctrl->frc_me_en;
	prm_top->frc_vp_en = frc_fw_alg_ctrl->vp_en;
	prm_top->frc_bbd_en = frc_fw_alg_ctrl->bbd_en;
	prm_top->mc_lp_mode = state_st->mc_lp_mode;
	prm_top->use_inp_big = state_st->use_inp_big;
	prm_top->frc_dae_div4 = dbg_st->ctrl_dbg.frc_dae_div4;
	prm_top->badedit_en = 1; // nr + frc default
	frc_top->badedit_en = 1; // nr + frc default
	nr_dpe_dw_cfg(prm_top);
	if (prm_top->dpe_dw_dsx == 1 && prm_top->dpe_dw_dsy == 1)
		frc_top->is_me1mc4 = 0;
	else if (prm_top->dpe_dw_dsx == 2 && prm_top->dpe_dw_dsy == 2)
		frc_top->is_me1mc4 = 1;
	else if (prm_top->dpe_dw_dsx == 2 && prm_top->dpe_dw_dsy == 1)
		frc_top->is_me1mc4 = 2;

	if (pfw_data->frc_input_cfg)
		pfw_data->frc_input_cfg(pfw_data);

	prm_top->trig_byp_dae0 = false;
	prm_top->trig_byp_mc = false;
	prm_top->reset_path = true;
	state_st->mc_byp_switch_on = false;
	state_st->mc_byp_switch_off = false;
	state_st->mc_ini_rdma_done = false;
	state_st->use_phase0_done = true;
	state_st->enable_mc_cnt = 0;
	state_st->src_chg = false;
	state_st->first_put_vfm = true;
	state_st->cur_fw_pause = frc_top->fw_pause;
	state_st->cur_frc_me_en = prm_top->frc_me_en;
	state_st->used_seq_vfm = false;
	state_st->pps_frc = false;

	if (state_st->force_n2m == 1) {
		frc_top->in_out_ratio = state_st->n2m_status.frc_ratio;
		dbg_f1("%s: force_n2m : %d\n", __func__,
			frc_top->in_out_ratio);
	}
	if (state_st->unformat_bypass) {
		if (prm_top->frm_vsize * 100 / prm_top->frm_hsize > 124 &&
				prm_top->frm_hsize < 1300)
			state_st->special_format = 1;
		else
			state_st->special_format = 0;
	}
}

void init_frc_post(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfs)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top; //ary need check
	struct PRM_DPSS_DAE *prm_dae = prm_dpss_dae;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;

	if (!pchip_st) {
		DBG_ERR("%s: pchip_st is null\n", __func__);
		return;
	}
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;

	if (prm_top->sw_tbc_ctrl_en == 1) {
		//split inp->nrdi
		w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, 2, 0, 2);	//change fnr dae to sw-tbc ctrl mode

		//split nrdi->frc
		w_reg_bit(FRC_DAE_SW_CTRL0, 2, 30, 2);	//change frc dae to sw-tbc ctrl mode
		//w_reg_bit(DPSS_TOP_INT_MODE_CTRL,3,2*5,2);//change nr dpe int to frm done
		//w_reg_bit(DPSS_DPE_START_CTRL,0,0,1);//change nr dpe to hold-line start mode

		//w_reg_bit(DPSS_DPE_FRM_ST_CTRL,1,20,1);//change nr dpe tbc frm_start to reg config

		if (prm_top->dpss_mode == DPSS_FRC_MODE)
			wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88883180);//tbc mux, vdin->inp->me->mc->vpp
	}
	if (prm_top->game_mode_n2m == 1 || prm_top->game_mode_film == 1) {
		w_reg_bit(FRC_DPSS_SRC_BUFF_RLS_CTRL, 1, 0, 1);	//release in order=1, 0:disorder
		w_reg_bit(FRC_DAE_FRM_DLY_NUM, 1, 0, 5);	//reg_dae_frm_dly_num
		w_reg_bit(FRC_DPSS_LLM, 1, 0, 1);	// game mode
		w_reg_bit(FRC_DPSS_LLM, 1, 3, 1);
		w_reg_bit(FRC_ME_TOP_CTRL, 1, 29, 1);	//reg_me_mode_ctrl
		// w_reg_bit(0x1139,3,8,2);// change vdin line clr from frm_done to frm_rst
		// wr(VPU_DAE_WRAP_BYPASS, (prm_top->frm_hsize << 16) | prm_top->frm_vsize);
		wr(VPU_DAE_WRAP_BYPASS,
			(prm_dae->prm_size.me_hsize << 16) | prm_dae->prm_size.me_vsize);

		w_reg_bit(FRC_REG_BLK_SCALE, 0, 0, 2);	//change form badedit to normal mode
		w_reg_bit(VPU_DAE_WRAP_CTRL, 0, 0, 1);	//change frc me frm_start to hold-lint mode
		if (prm_top->game_mode_n2m == 1) {
			// wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88883088);	//tbc mux, vdin->me->mc->vpp
			//w_reg_bit(FRC_REG_TOP_CTRL7, 0x40000,0,20); //todo 1:2
			w_reg_bit_vc(VDIN_TOP_MISC, 2, 10, 2);
		} else {
			w_reg_bit_vc(VDIN_TOP_MISC, 0, 10, 2);  // game film
		}
		cfg_dpss_gmd_phs_lut(prm_top->frc_ratio, 1);	//set phase ini

		//change inp-tbc-ctrl frm_start from auto-config to reg-config
		if (prm_top->sw_buf_rls_en == 1)
			w_reg_bit(FRC_INP_HOLD_CTRL, 1, 24, 1);

	} else {
		w_reg_bit(FRC_DPSS_SRC_BUFF_RLS_CTRL, 0, 0, 1);
		w_reg_bit(FRC_DAE_FRM_DLY_NUM, 2, 0, 5);
		w_reg_bit(FRC_ME_TOP_CTRL, 0, 29, 1);
	}

	if (game_st->inp_ctrl) // reback inp start value
		w_reg_bit(FRC_INP_HOLD_CTRL, 0, 24, 1);

	venc_vrr_vdin();
	// prm_dpss_top.badedit_en == 0/1
	// w_reg_bit(DPSS_REG_LOOP_PHASE_ADJ, 3/0, 4, 2);
	if (prm_top->me_mc_link_en == 1 || prm_top->game_mode_n2m == 1 ||
		prm_top->game_mode_film == 1)
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 2, 0, 2);	//reg_dae_top_arb_mode
	else
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 1, 0, 2);	//reg_dae_top_arb_mode

	if (vinfo) {
		if (vinfo->width == 3840 && vinfo->height == 1080)
			w_reg_bit(FRC_MC_PREWR_BLANK, 4, 0, 11);
	}

	prm_top->film_hwfw_sel = 1;
	prm_top->dae_step_sel = 2;
	w_reg_bit(FRC_REG_FILM_PHS_1, prm_top->film_hwfw_sel, 16, 1);
	w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1, 0, 8); //reset mc rdmif

	if (state_st->mc_cut_position == 1)
		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1, 8, 1);
	else
		w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0, 8, 1);

	w_reg_bit(DPSS_REG_DAE_WRPT_FULL_NUM, dae_wrpt_full_num, 0, 4);
	w_reg_bit(FRC_REG_CURSOR, 0, 0, 3); //cancel dae force mix

	// nr+frc restore game frc rst mode
	w_reg_bit(FRC_REG_INP_FRM_RST_CTRL, 0, 0, 1);

	frc_set_mc_bypass(3);

	if (pchip_st && pchip_st->state_st.demo_win_en)
		dpss_frc_demo_win(1, 0); // update mc demo size
	dbg_h2("%s ok\n", __func__);
}

void frc_prm_top_init_vfm(struct dpss_ch_s *pch,
	struct PRM_DPSS_TOP *prm_top, struct dpss_sub_vf_s *vfms, bool is_ex_di)
{
	unsigned int vout_fr;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct vinfo_s *vinfo = get_current_vinfo();

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!prm_top || !vfms || !pfw_data) {
		DBG_ERR("%s is_ex_di:%d\n", __func__, is_ex_di);
		return;
	}

	frc_top = &pfw_data->frc_top_type;
	prm_top->frm_hsize = vfms->width;
	prm_top->frm_vsize = vfms->height;

	prm_top->amdv_frm_hsize = vfms->width;
	prm_top->amdv_frm_vsize = vfms->height;
	prm_top->amdv_org_hsize = vfms->width;
	prm_top->amdv_org_vsize = vfms->height;

	prm_top->src_h_buf_size = vfms->cvs_h;	//tmp for nv21/12
	dbg_h2("src_h_buf_size=%d\n", prm_top->src_h_buf_size);

	if (prm_top->cfg_slc == 1) {
		prm_top->slc_num = 4;
		prm_top->dpe_slc_num = 4;	//
	} else {
		prm_top->slc_num = 1;
		prm_top->dpe_slc_num = 1;	//
	}

	if (prm_top->frm_hsize * prm_top->frm_vsize < 1920 * 1080)
		prm_top->frc_fbuf_only = 1; //no dw buf
	else
		prm_top->frc_fbuf_only = 0;

	if (pch->c.case_id == TST_CASE_IDX_0002) {
		// prm_top->frc_ratio	   = DPSS_FRC_RATIO_1_2;
		if (prm_top->frm_hsize <= 1920 &&
			(prm_top->frm_hsize * prm_top->frm_vsize <= 1920 * 1080)) {
			prm_top->dae_dsx_scale  = 1;
			prm_top->dae_dsy_scale  = 1;
			prm_top->dpe_dw_dsx		= 1;
			prm_top->dpe_dw_dsy		= 1;
			prm_top->mvx_div_mode	= 1;
			prm_top->mvy_div_mode	= 1;
			prm_top->dpe_slc_num	= 2;
			prm_top->alig_hmode     = 0;
			prm_top->alig_vmode     = 0;
			dbg_i2("input is FHD\n");
		} else if (prm_top->frm_hsize <= 3840 &&
				(prm_top->frm_hsize * prm_top->frm_vsize <= 3840 * 1080)) {
			prm_top->dae_dsx_scale = 2;
			prm_top->dae_dsy_scale = 1;
			prm_top->dpe_dw_dsx    = 2;
			prm_top->dpe_dw_dsy    = 1;
			prm_top->mvx_div_mode  = 2;
			prm_top->mvy_div_mode  = 1;
			prm_top->dpe_slc_num   = 4;
			prm_top->alig_hmode    = 1;
			prm_top->alig_vmode    = 0;
			dbg_i2("input is 4K1k\n");
		} else {
			prm_top->dae_dsx_scale  = 2;
			prm_top->dae_dsy_scale  = 2;
			prm_top->dpe_dw_dsx		= 2;
			prm_top->dpe_dw_dsy		= 2;
			prm_top->mvx_div_mode	= 2;
			prm_top->mvy_div_mode	= 2;
			prm_top->dpe_slc_num	= 4;
			prm_top->alig_hmode     = 1;
			prm_top->alig_vmode     = 1;
			dbg_i2("input is UHD\n");
		}
		prm_top->fnr_sw_mode = 0;
		prm_top->cfg_seed = 1;
		prm_top->frc_en = true;
		if (prm_top->frc_ds_scale_en == 1) {
			prm_top->dpe_mc_size.frm_hsize =
				prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.frm_vsize =
				prm_top->frm_vsize >> prm_top->dae_dsy_scale;
			prm_top->dpe_mc_size.src_hsize =
				prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.src_vsize =
				prm_top->frm_vsize >> prm_top->dae_dsy_scale;
			prm_top->dae_dsx_scale  = 1;
			prm_top->dae_dsy_scale  = 1;
			prm_top->auto_alig_en	= 1;
			prm_top->dpe_dw_dsx		= 1;
			prm_top->dpe_dw_dsy		= 1;
			prm_top->mvx_div_mode	= 1;
			prm_top->mvy_div_mode	= 1;
			prm_top->dpe_slc_num	= 2;
			prm_top->alig_hmode     = 0;
			prm_top->alig_vmode     = 0;
			dbg_i2("dpe_mc_size frm(%d,%d),src(%d,%d)\n",
				prm_top->dpe_mc_size.frm_hsize,
				prm_top->dpe_mc_size.frm_vsize,
				prm_top->dpe_mc_size.src_hsize,
				prm_top->dpe_mc_size.src_vsize);
			// prm_top->frc_no_ds = 1;
		}
		//w_reg_bit(VPU_AXIRD_PATH_CTRL,0,0,3);

		// test game mode
		prm_top->game_mode_n2m = frc_top->game_mode & 0x1;
		prm_top->game_mode_film = (frc_top->game_mode >> 1) & 0x1;
		dbg_i2("game_mode_n2m = %d game_mode_film = %d\n",
			prm_top->game_mode_n2m, prm_top->game_mode_film);
		// test soft contrl case
		prm_top->sw_buf_rls_en = 0;
		prm_top->sw_tbc_ctrl_en = 0;
		// prm_top->inp_done_int = 1;
		prm_top->me_mc_link_en = 0;

		if (prm_top->game_mode_n2m | prm_top->game_mode_film) {
			prm_top->dpe_game_mode = 1;
			prm_top->inp_done_int = 0;
		} else {
			prm_top->dpe_game_mode = 0;
			prm_top->inp_done_int = 1;
		}
		fnr_dpe_obuf_rls_ini = 0;
		if (prm_top->frc_fbuf_only == 1) {
			prm_top->dae_dsx_scale = 1;
			prm_top->dae_dsx_scale = 1;
		}
		if (prm_top->mc_skip_mode == 3) { // h/2, v/2
			prm_top->dpe_mc_size.frm_hsize = prm_top->frm_hsize >> 1;
			prm_top->dpe_mc_size.frm_vsize = prm_top->frm_vsize >> 1;
		} else if (prm_top->mc_skip_mode == 1) { // h, v/2
			prm_top->dpe_mc_size.frm_hsize = prm_top->frm_hsize;
			prm_top->dpe_mc_size.frm_vsize = prm_top->frm_vsize >> 1;
		} else if (prm_top->mc_skip_mode == 2) { // h/2, v
			prm_top->dpe_mc_size.frm_hsize = prm_top->frm_hsize >> 1;
			prm_top->dpe_mc_size.frm_vsize = prm_top->frm_vsize;
		}
		dbg_i2("mc_skip_mode:%d, mc_frm(%d,%d)\n", prm_top->mc_skip_mode,
			prm_top->dpe_mc_size.frm_hsize, prm_top->dpe_mc_size.frm_vsize);
	} else if (pch->c.case_id == TST_CASE_IDX_0102) {
		// cfg_dpss_dae_me_rand_seed(0, 0, 0, 0);	// ME SEED
		// prm_top->frc_ratio = DPSS_FRC_RATIO_1_1; //DPSS_FRC_RATIO_1_2;
		prm_top->fnr_sw_mode = 0;
		prm_top->cfg_seed = 0;
		prm_top->frc_en = true;
		if (prm_top->frc_ds_scale_en == 1) {
			prm_top->dpe_mc_size.frm_hsize =
			    prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.frm_vsize =
			    prm_top->frm_vsize >> prm_top->dae_dsy_scale;
			prm_top->dpe_mc_size.src_hsize =
			    prm_top->frm_hsize >> prm_top->dae_dsx_scale;
			prm_top->dpe_mc_size.src_vsize =
			    prm_top->frm_vsize >> prm_top->dae_dsy_scale;
		}
		//w_reg_bit(VPU_AXIRD_PATH_CTRL,0,0,3);

		// test game mode
		prm_top->game_mode_n2m = frc_top->game_mode & 0x1;
		prm_top->game_mode_film = (frc_top->game_mode >> 1) & 0x1;
		dbg_i2("game_mode_n2m = %d game_mode_film = %d\n",
			prm_top->game_mode_n2m, prm_top->game_mode_film);
		// test soft contrl case
		prm_top->sw_buf_rls_en = 0;
		prm_top->sw_tbc_ctrl_en = 0;
		// prm_top->inp_done_int = 1;
		prm_top->me_mc_link_en = 0;

		if (prm_top->game_mode_n2m | prm_top->game_mode_film) {
			prm_top->dpe_game_mode = 1;
			prm_top->inp_done_int = 0;
		} else {
			prm_top->dpe_game_mode = 0;
			prm_top->inp_done_int = 1;
		}
		fnr_dpe_obuf_rls_ini = 0;
		if (prm_top->frm_hsize <= 1920 &&
			(prm_top->frm_hsize * prm_top->frm_vsize <= 1920 * 1080)) {
			prm_top->dae_dsx_scale  = 1;
			prm_top->dae_dsy_scale  = 1;
			prm_top->dpe_dw_dsx     = 1;
			prm_top->dpe_dw_dsy     = 1;
			prm_top->mvx_div_mode   = 1;
			prm_top->mvy_div_mode   = 1;
			prm_top->dpe_slc_num    = 2;
			dbg_i2("input is FHD\n");
		} else if (prm_top->frm_hsize <= 3840 &&
			(prm_top->frm_hsize * prm_top->frm_vsize <= 3840 * 1080)) {
			prm_top->dae_dsx_scale = 2;
			prm_top->dae_dsy_scale = 1;
			prm_top->dpe_dw_dsx    = 2;
			prm_top->dpe_dw_dsy    = 1;
			prm_top->mvx_div_mode  = 2;
			prm_top->mvy_div_mode  = 1;
			prm_top->dpe_slc_num   = 4;
			dbg_i2("input is 4K1k\n");
		} else {
			prm_top->dae_dsx_scale = 2;
			prm_top->dae_dsy_scale = 2;
			prm_top->dpe_dw_dsx    = 2;
			prm_top->dpe_dw_dsy    = 2;
			prm_top->mvx_div_mode  = 2;
			prm_top->mvy_div_mode  = 2;
			prm_top->dpe_slc_num   = 4;
			dbg_i2("input is UHD\n");
		}
	}

	if (pch->c.case_id == TST_CASE_IDX_0002 ||
		pch->c.case_id == TST_CASE_IDX_0102) {
		pch->d->frc_link = true;
	}

	if (vinfo)
		vout_fr = vinfo->sync_duration_num / vinfo->sync_duration_den;

	if (prm_top->frc_dae_div4 == 1 && vout_fr > 199 && prm_top->frm_vsize >= 1080) {
		prm_top->dpe_dw_dsx = 2;
		prm_top->dpe_dw_dsy = 2;
	}
	dbg_i2("frc link = %d\n", pch->d->frc_link);
}

static void dpss_frc_h_val_init(struct dpss_ch_s *pch)
{
	if (!pch) {
		DBG_WARN("%s:no input:\n", __func__);
		return;
	}
	memset(&pch->c.prm_top, 0, sizeof(pch->c.prm_top));
	memset(&pch->c.prm_dae, 0, sizeof(pch->c.prm_dae));
	// memset(&pch->c.prm_dpe, 0, sizeof(pch->c.prm_dpe));

	memset(&pch->c.dae_yuv_intf, 0, sizeof(pch->c.dae_yuv_intf));
	memset(&pch->c.prm_size, 0, sizeof(pch->c.prm_size));

	pch->c.prm_dae.ext_yuv_intf = &pch->c.dae_yuv_intf;
	pch->c.prm_dae.prm_size_ext = &pch->c.prm_size;
}

void dpss_frc_only_reset(unsigned int para)
{
	if (para & 0x1) {
		wr(DPSS_DPE_MC_TOP_RESET, BIT_22 + BIT_25);
		dbg_f1("mc reset bit 22+25\n");
	}
	if (para & 0x2) {
		w_reg_bit(DPSS_TOP_SOFT_RST, 0x047e, 0, 13);
		w_reg_bit(DPSS_TOP_SOFT_RST, 0x0000, 0, 13);
	}
}

void frc_only_int(struct dpss_ch_s *pch, struct dpss_sub_vf_s *vfs, struct vframe_s *vf)
{
	struct PRM_DPSS_TOP *prm_top = &pch->c.prm_top;//&prm_dpss_top;
	struct PRM_DPSS_DAE *prm_dae = &pch->c.prm_dae;//&prm_dpss_dae;
	struct PRM_DPSS_DPE *prm_dpe = &pch->c.prm_dpe;//&prm_dpss_dpe;
	struct dpss_frc_fw_alg_ctrl_s *frc_fw_alg_ctrl;
	struct dpss_user_para_s *prm_user, user_d;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	int i;
	unsigned int src = 0;

	if (!pchip_st) {
		dbg_h0("%s pchip_st is null\n", __func__);
		return;
	}
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		dbg_h0("%s pfw_data is null\n", __func__);
		return;
	}
	frc_top = &pfw_data->frc_top_type;
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;

	dpss_frc_h_val_init(pch);
	dpss_frc_only_reset(state_st->frc_only_rst);
	hw_init_part1(vf);

	// game clr
	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE, 0xf, 4, 4); //4:7
	w_reg_bit(VPU_DAE_WRAP_GMAE_MODE, 0x0, 4, 4);
	w_reg_bit(DPSS_DPE_MC_TOP_RESET, 0xf, 11, 4);  //11:14
	w_reg_bit(DPSS_DPE_MC_TOP_RESET, 0x0, 11, 4);
	w_reg_bit(FRC_INP_RD_YLINE, 0xf, 20, 4);     //20:23
	w_reg_bit(FRC_INP_RD_YLINE, 0x0, 20, 4);

	w_reg_bit(VPU_DAE_WRAP_MIF_MASK, 1, 27, 1);

	//tmp here:
	for (i = 0; i < DPSS_VFM_IN_NUB; i++)
		pch->d->h_dae_vf_idx[i] = 0xff;
	dbg_h2("%s:dae_vf:clean\n", __func__);
	//..
	dpss_inp_frm_cnt	      = 0;
	dpss_dae_frm_cnt_src0	  = 0;
	dpss_dae_frm_cnt_src1	  = 0;
	dpss_dae_frm_cnt_src2	  = 0;
	dpss_dpe_nr_frm_cnt	  = 0;
	dpss_dpe_di_frm_cnt	  = 0;
	mc_undone_cnt = 0;
	inp_undone_cnt = 0;
	me_undone_cnt = 0;
	vp_undone_cnt = 0;
	dpss_mc_rls_cnt = 0;

	src0_frm_idx_cnt = 0;

	ini_cfg_frc_dae = 0;
	frc_fst_frm = 0;
	//dpss_dbg_uv_swap = 0x1f4; // only frc used
	pch->c.cnt_in = 0;
	pch->c.cnt_proce = 0;
	frc_top->auto_align_en = 1;
	frc_state_init();
	frc_compress_fmt_parse(vfs);
	hw_val_init(pch->c.ch);
	frc_fw_alg_ctrl = &pfw_data->frc_fw_alg_ctrl;
	prm_top->frc_melogo_en = frc_fw_alg_ctrl->melogo_en;
	prm_top->frc_iplogo_en = frc_fw_alg_ctrl->iplogo_en;
	prm_top->frc_me_en = frc_fw_alg_ctrl->frc_me_en;
	prm_top->frc_vp_en = frc_fw_alg_ctrl->vp_en;
	prm_top->frc_bbd_en = frc_fw_alg_ctrl->bbd_en;
	prm_top->mc_lp_mode = state_st->mc_lp_mode;
	prm_dae->dctgrd_en = 0;
	prm_dpe->dcntr_en = 0;
	prm_top->mc_auto_en = 0;
	dpss_hdr_sw(false, vf);
	dbg_h2("%s dpss_hdr_sw false\n", __func__);

	prm_user = &user_d;
	memset(prm_user, 0, sizeof(*prm_user));

	dpss_val_user_frc(pch, prm_user, vfs);
	pch->d->is_i = false;
	dbg_i1("%s:is_i:%d %d %d\n", __func__,
		pch->d->is_i,
		pch->d->is_field,
		pch->d->is_top_first);
	if (VFM_IS_COMP_MODE(vfs->type)) {
		pch->d->is_afbcd = true;
		if (VFM_IS_COMP_AFRC(vfs->type_ext))
			pch->d->is_afrcd = true;
		else
			pch->d->is_afrcd = false;
		prm_top->mc_auto_en = 0;
	} else {
		pch->d->is_afbcd = false;
		pch->d->is_afrcd = false;
		prm_top->mc_auto_en = 0;
	}
	if (VFM_IS_VDIN_SRC(vfs->source_type))
		prm_top->force_dos_mode = 0;
	else
		prm_top->force_dos_mode = 1;

	if (vfs->bitdepth & BITDEPTH_Y10)
		pch->d->is_10bit = true;
	if (VFM_IS_FMT_420(vfs->type))
		pch->d->fmt = YUV420;
	else if (VFM_IS_FMT_422(vfs->type))
		pch->d->fmt = YUV422;
	else if (VFM_IS_FMT_444(vfs->type))
		pch->d->fmt = YUV444;
	src = prm_user->src_mode;

	if (vfs->plane_num == 1)
		pch->d->plane = PLANAR_X1;
	else if (vfs->plane_num == 2)
		pch->d->plane = PLANAR_X2;
	else
		pch->d->plane = PLANAR_X3;

	if (vf->flag & VFRAME_FLAG_VIDEO_LINEAR) {
		prm_top->little_endian = 1;
		prm_top->swap_64bit = 0;
	} else {
		prm_top->little_endian = 0;
		prm_top->swap_64bit = 1;
	}

	//dpss_dbg_irq_ko = 0x0; // frc only close fw irq
	_prm_top_init_frc(pch, prm_top, src); //path id 0 is src 0 //dpss_prm_init
	dpss_val_2_top(prm_top, prm_user);
	fpga_ucode_1217(prm_top, dpss_dbg_top_cfg0); //part of HW_Initialize 12-17
	_prm_top_init_buffer(prm_top, pch, src);
	hw_init_small_addr(prm_top, src);
	frc_prm_top_init_vfm(pch, prm_top, vfs, false);
	// pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	//if (pfw_data) {
	frc_top->hsize = prm_top->frm_hsize;
	frc_top->vsize = prm_top->frm_vsize;
	frc_top->dpss_mode = prm_top->dpss_mode;

	if (prm_top->dpe_dw_dsx == 1 && prm_top->dpe_dw_dsy == 1)
		frc_top->is_me1mc4 = 0;
	else if (prm_top->dpe_dw_dsx == 2 && prm_top->dpe_dw_dsy == 2)
		frc_top->is_me1mc4 = 1;
	else if (prm_top->dpe_dw_dsx == 2 && prm_top->dpe_dw_dsy == 1)
		frc_top->is_me1mc4 = 2;
	dbg_h2("frc_top->is_me1mc4:%d\n", frc_top->is_me1mc4);

	if (pfw_data->frc_input_cfg)
		pfw_data->frc_input_cfg(pfw_data);
	//} else {
	//	DBG_ERR("%s: frc_fw is null\n", __func__);
	//	return;
	//}
	hw_cfg_dpss_top(prm_top);
	hw_cfg_dpss_inp(prm_top, prm_dpss_inp);
	// hw_cfg_dpss_dae(DAE_FRC_MODE, prm_top, prm_dae);
	// hw_cfg_dpss_dae(DAE_FRC_MODE, prm_top, prm_dae);
	hw_cfg_dpss_dae_frc(DAE_FRC_MODE, prm_top, prm_dae);
	prm_top->film_hwfw_sel = 1;
	w_reg_bit(FRC_REG_FILM_PHS_1, prm_top->film_hwfw_sel, 16, 1);
	w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1, 0, 8); //reset mc rdmif

	if (prm_top->sw_tbc_ctrl_en == 1) {
		//split inp->nrdi
		w_reg_bit(DPSS_FNR_SW_DRV_CTRL1, 2, 0, 2);	//change fnr dae to sw-tbc ctrl mode

		//split nrdi->frc
		w_reg_bit(FRC_DAE_SW_CTRL0, 2, 30, 2);	//change frc dae to sw-tbc ctrl mode
		//w_reg_bit(DPSS_TOP_INT_MODE_CTRL,3,2*5,2);//change nr dpe int to frm done
		//w_reg_bit(DPSS_DPE_START_CTRL,0,0,1);//change nr dpe to hold-line start mode

		//w_reg_bit(DPSS_DPE_FRM_ST_CTRL,1,20,1);//change nr dpe tbc frm_start to reg config

		if (prm_top->dpss_mode == DPSS_FRC_MODE)
			wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88883180);//tbc mux, vdin->inp->me->mc->vpp
	}
	if (prm_top->game_mode_n2m == 1 || prm_top->game_mode_film == 1) {
		w_reg_bit(FRC_DPSS_SRC_BUFF_RLS_CTRL, 1, 0, 1);	//release in order=1, 0:disorder
		w_reg_bit(FRC_DAE_FRM_DLY_NUM, 1, 0, 5);	//reg_dae_frm_dly_num
		w_reg_bit(FRC_DPSS_LLM, 1, 0, 1);	// game mode
		w_reg_bit(FRC_DPSS_LLM, 1, 3, 1);   // game mode inp
		w_reg_bit(FRC_ME_TOP_CTRL, 1, 29, 1);	//reg_me_mode_ctrl
		// w_reg_bit(0x1139,3,8,2);//reg_line_clr,chg vdin line clr from frm_done to frm_rst
		// wr(VPU_DAE_WRAP_BYPASS, (prm_top->frm_hsize << 16) | prm_top->frm_vsize);
		wr(VPU_DAE_WRAP_BYPASS,
			(prm_dae->prm_size.me_hsize << 16) | prm_dae->prm_size.me_vsize);

		w_reg_bit(FRC_REG_BLK_SCALE, 0, 0, 2);	//phase gain chg form badedit to normal mode
		w_reg_bit(VPU_DAE_WRAP_CTRL, 0, 0, 1);	//change frc me frm_start to hold-lint mode
		//if (prm_top->game_mode_n2m == 1) {
			// wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88883088);	//tbc mux, vdin->me->mc->vpp
			//w_reg_bit(FRC_REG_TOP_CTRL7, 0x40000,0,20); //todo 1:2
			//w_reg_bit_vc(VDIN_TOP_MISC, 2, 10, 2);
		//} else {
			// game film ((vdin done chg index)
			//w_reg_bit_vc(VDIN_TOP_MISC, 0, 10, 2);
		//}
		w_reg_bit_vc(VDIN_TOP_MISC, 2, 10, 2); //vdin isr start chg index
		wr(FRC_DPSS_TBC_MUX_DOUT_SEL, 0x88883180); //tbc mux, vdin->inp->me->mc->vpp
		cfg_dpss_gmd_phs_lut(prm_top->frc_ratio, 1);	//set phase ini

		//change inp-tbc-ctrl frm_start from auto-config to reg-config
		if (prm_top->sw_buf_rls_en == 1 || game_st->inp_ctrl)
			w_reg_bit(FRC_INP_HOLD_CTRL, 1, 24, 1);

		dpss_frc_set_dejudder(10);
		dpss_frc_set_deblur(10);
		set_video_mute(FRC_MUTE_SET, true);
		game_st->video_mute_en = true;
		game_st->game_deblur = 64; // 0x40
		//w_reg_bit(FRC_ME_GCV_EN, 0, 16, 1); // disable clear v-buffer
		//w_reg_bit(FRC_ME_GCV2_EN, 0, 15, 1);// disable clear v-buffer2
		//w_reg_bit(FRC_MC_HW_CTRL1, 0, 16, 1); // fhd->4K err mc_fhd_lbuf
		//frc_top->fw_pause = 1; // pause fw
		prm_top->badedit_en = 0;
		frc_top->badedit_en = 0;
		w_reg_bit(FRC_REG_INP_FRM_CTRL, 0, 4, 1); //game mode used auto frm_start mode
		w_reg_bit(FRC_REG_INP_FRM_RST_CTRL, 1, 0, 1);
	} else {
		w_reg_bit(FRC_DPSS_SRC_BUFF_RLS_CTRL, 1, 0, 1);//release in order=1, 0:disorder
		w_reg_bit(FRC_DAE_FRM_DLY_NUM, 2, 0, 5);	//reg_dae_frm_dly_num
		w_reg_bit(FRC_ME_TOP_CTRL, 0, 29, 1);	//reg_me_mode_ctrl
	}

	game_st->vdin_buf_num = get_vdin_buffer_num_frc();
	w_reg_bit(FRC_AUTO_MODE_BUFF_NUM,
		game_st->vdin_buf_num, 0, 5); //set vdin buf num 1.29 from tao.yao

	frc_set_mc_bypass(3);
	venc_vrr_vdin();

	w_reg_bit(FRC_REG_CURSOR, 1, 3, 1);
	w_reg_bit(VPU_DAE_WRAP_ASYNC, 0, 25, 2);

	// prm_dpss_top.badedit_en == 0/1
	// w_reg_bit(DPSS_REG_LOOP_PHASE_ADJ, 3/0, 4, 2);

	if (prm_top->me_mc_link_en == 1 ||
			prm_top->game_mode_n2m == 1 ||
			prm_top->game_mode_film == 1)
		// w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 2, 0, 2); //reg_dae_top_arb_mode
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 0, 0, 2); //reg_dae_top_arb_mode
	else
		w_reg_bit(DPSS_DAE_TOP_ARB_MODE, 1, 0, 2);	//reg_dae_top_arb_mode
	// hw_config_top_post(prm_top);

	if (pchip_st && pchip_st->state_st.demo_win_en)
		dpss_frc_demo_win(1, 0); // update mc demo size

	if (dpss_dbg_top_cfg0 & C_BIT31)
		f_dpss_hw_init(prm_top);
	vpu_initqueue(&inp_bufQ);
	pch->d->init = true;
}

//use this for auto mode only:
static void frc_prm_top_input_buffer_set(struct PRM_DPSS_TOP *prm_top,
					unsigned int src,
					unsigned int idx,
					struct dpss_sub_vf_s *vfms)
{
	unsigned long laddr1, laddr2;
	unsigned int *addr_t1_src, *addr_t2_src, *addr_t3_dw, *addr_t4_dw;
	unsigned int reg_t1_src, reg_t2_src, reg_t3_dw, reg_t4_dw;

	if (src > 1) {
		DBG_WARN("%s:src fix to 0:%d\n", __func__, src);
		src = 0;
	}
	if (idx >= 16)
		idx = 0;
//for frc:
	if (prm_top->frc_en) {
		dae_rd_less_one = (rd(FRC_DPSS_LLM) >> 2) & 0x1;
		dbg_h2("%s:frc:dae_rd_less_one = %d\n", __func__, dae_rd_less_one);
	}

	if (src == 0) {
		addr_t1_src = &prm_top->src0_fbuf_yaddr[idx];
		addr_t2_src = &prm_top->src0_fbuf_caddr[idx];
		addr_t3_dw = &prm_top->src0_dwbuf_yaddr[idx];
		addr_t4_dw = &prm_top->src0_dwbuf_caddr[idx];
		reg_t1_src = DPSS_SRC0_FBUF_YADDR0 + idx;
		reg_t2_src  = DPSS_SRC0_FBUF_CADDR0 + idx;
		reg_t3_dw = DPSS_SRC0_DWBUF_YADDR0 + idx;
		reg_t4_dw = DPSS_SRC0_DWBUF_CADDR0 + idx;
	} else {
		addr_t1_src = &prm_top->src1_fbuf_yaddr[idx];
		addr_t2_src = &prm_top->src1_fbuf_caddr[idx];
		addr_t3_dw = &prm_top->src1_dwbuf_yaddr[idx];
		addr_t4_dw = &prm_top->src1_dwbuf_caddr[idx];
		reg_t1_src = DPSS_SRC1_FBUF_YADDR0 + idx;
		reg_t2_src  = DPSS_SRC1_FBUF_CADDR0 + idx;
		reg_t3_dw = DPSS_SRC1_DWBUF_YADDR0 + idx;
		reg_t4_dw = DPSS_SRC1_DWBUF_CADDR0 + idx;
	}
	laddr1 = vfms->canvas0_config[0].phy_addr;
	laddr2 = vfms->canvas0_config[1].phy_addr;

	*addr_t1_src = (unsigned int)(laddr1 >> 4);
	*addr_t2_src = (unsigned int)(laddr2 >> 4);
	*addr_t3_dw =  (unsigned int)(laddr1 >> 9);
	*addr_t4_dw =  (unsigned int)(laddr2 >> 9);

	wr(reg_t1_src, *addr_t1_src);
	wr(reg_t2_src, *addr_t2_src);
	wr(reg_t3_dw, *addr_t3_dw);
	wr(reg_t4_dw, *addr_t4_dw);
	dbg_h1("%s:idx:%d\n", __func__, idx);
	dbg_h1("\treg:addr_src:0x%x = 0x%x\n", reg_t1_src, rd(reg_t1_src));
	dbg_h1("\treg:addr_src:0x%x = 0x%x\n", reg_t2_src, rd(reg_t2_src));
	dbg_h1("\treg:addr_dw:0x%x = 0x%x\n", reg_t3_dw, rd(reg_t3_dw));
	dbg_h1("\treg:addr_dw:0x%x = 0x%x\n", reg_t4_dw, rd(reg_t4_dw));
}

static void frc_prm_top_input_afbcd(struct PRM_DPSS_TOP *prm_top,
				    unsigned int idx,
				    struct dpss_sub_vf_s *vfms)
{
	unsigned long laddr1, laddr2;
	unsigned int *addr_t1_dw, *addr_t2_dw;
	unsigned int reg_t1_dw, reg_t2_dw;
	unsigned int *addr_t1_src, *addr_t2_src;
	unsigned int reg_t1_src, reg_t2_src;

	if (!vfms) {
		DBG_WARN("%s:vfm is null\n", __func__);
		return;
	}
	if (idx >= (rd(FRC_AUTO_MODE_BUFF_NUM) & 0x1f))
		idx = 0;

	if (VFM_IS_COMP_MODE(vfms->type)) {
		if (VFM_IS_COMP_AFRC(vfms->type_ext)) {
			prm_top->src0_head_yaddr[idx] = vfms->afrc_info.luma_head_addr >> 4;
			prm_top->src0_fbuf_yaddr[idx] = vfms->afrc_info.luma_body_addr >> 4;
			prm_top->src0_head_caddr[idx] = vfms->afrc_info.chrm_head_addr >> 4;
			prm_top->src0_fbuf_caddr[idx] = vfms->afrc_info.chrm_body_addr >> 4;
			dbg_h2("afrc: addr <0x%x, 0x%x> <0x%x, 0x%x>\n",
				prm_top->src0_head_yaddr[idx], prm_top->src0_fbuf_yaddr[idx],
				prm_top->src0_head_caddr[idx], prm_top->src0_fbuf_caddr[idx]);
		} else {
			prm_top->src0_head_yaddr[idx] = vfms->compHeadAddr >> 4;
			prm_top->src0_head_caddr[idx] = prm_top->src0_head_yaddr[idx];
			prm_top->src0_fbuf_yaddr[idx] = vfms->compBodyAddr >> 4;
			dbg_h2("afbcd: addr <0x%lx, 0x%x> <0x%lx, 0x%x>\n",
				vfms->compHeadAddr, prm_top->src0_head_yaddr[idx],
				vfms->compBodyAddr, prm_top->src0_fbuf_yaddr[idx]);
		}
	}

	addr_t1_dw = &prm_top->src0_dwbuf_yaddr[idx];
	addr_t2_dw = &prm_top->src0_dwbuf_caddr[idx];

	reg_t1_dw = DPSS_SRC0_DWBUF_YADDR0 + idx;
	reg_t2_dw = DPSS_SRC0_DWBUF_CADDR0 + idx;

	laddr1 = vfms->canvas0_config[0].phy_addr;
	laddr2 = vfms->canvas0_config[1].phy_addr;

	dbg_h1("\tvfms->canvas0_config[0].phy_addr = %#lx\n", laddr1);
	dbg_h1("\tvfms->canvas0_config[1].phy_addr = %#lx\n", laddr2);

	*addr_t1_dw = (unsigned int)(laddr1 >> 9);
	*addr_t2_dw = (unsigned int)(laddr2 >> 9);

	wr(reg_t1_dw, *addr_t1_dw);
	wr(reg_t2_dw, *addr_t2_dw);
	dbg_h1("%s:idx:%d\n", __func__, idx);
	dbg_h1("\tdw_reg:addr:0x%x = %#x\n", reg_t1_dw, rd(reg_t1_dw));
	dbg_h1("\tdw_reg:addr:0x%x = %#x\n", reg_t2_dw, rd(reg_t2_dw));
	if (prm_top->frc_ds_scale_en == 0) {
		if (VFM_IS_COMP_MODE(vfms->type)) {
			if (VFM_IS_COMP_AFRC(vfms->type_ext)) {
				prm_top->src0_head_yaddr[idx] =
					vfms->afrc_info.luma_head_addr >> 4;
				prm_top->src0_fbuf_yaddr[idx] =
					vfms->afrc_info.luma_body_addr >> 4;
				prm_top->src0_head_caddr[idx] =
					vfms->afrc_info.chrm_head_addr >> 4;
				prm_top->src0_fbuf_caddr[idx] =
					vfms->afrc_info.chrm_body_addr >> 4;
				dbg_h2("afrcd: addr y<%#x, %#x> c<%#x, %#x>\n",
					prm_top->src0_head_yaddr[idx],
					prm_top->src0_fbuf_yaddr[idx],
					prm_top->src0_head_caddr[idx],
					prm_top->src0_fbuf_caddr[idx]);
			} else {
				prm_top->src0_head_yaddr[idx] = vfms->compHeadAddr >> 4;
				prm_top->src0_head_caddr[idx] = prm_top->src0_head_yaddr[idx];
				prm_top->src0_fbuf_yaddr[idx] = vfms->compBodyAddr >> 4;
				dbg_h2("afbcd: addr <%#lx, %#x> <%#lx, %#x>\n",
					vfms->compHeadAddr, prm_top->src0_head_yaddr[idx],
					vfms->compBodyAddr, prm_top->src0_fbuf_yaddr[idx]);
			}
		}
	} else { // frc_ds_scale_en == 1
		if (VFM_IS_COMP_MODE(vfms->type)) {
			if (VFM_IS_COMP_AFRC(vfms->type_ext))
				dbg_h2("afrcd: luma <%#lx, %#lx> chrm:<%#lx, %#lx>\n",
						(ulong)(vfms->afrc_info.luma_head_addr >> 4),
						(ulong)(vfms->afrc_info.luma_body_addr >> 4),
						(ulong)(vfms->afrc_info.chrm_head_addr >> 4),
						(ulong)(vfms->afrc_info.chrm_body_addr >> 4));
			else
				dbg_h2("afbcd: addr <%#lx, %#lx>\n",
					vfms->compHeadAddr,
					vfms->compBodyAddr);
		}
		addr_t1_src = &prm_top->src0_fbuf_yaddr[idx];
		addr_t2_src = &prm_top->src0_fbuf_caddr[idx];
		reg_t1_src = DPSS_SRC0_FBUF_YADDR0 + idx;
		reg_t2_src  = DPSS_SRC0_FBUF_CADDR0 + idx;
		*addr_t1_src = (unsigned int)(laddr1 >> 4);
		*addr_t2_src = (unsigned int)(laddr2 >> 4);
		wr(reg_t1_src, *addr_t1_src);
		wr(reg_t2_src, *addr_t2_src);
		dbg_h1("\tsrc_reg:addr:%#x = %#x\n", reg_t1_src, rd(reg_t1_src));
		dbg_h1("\tsrc_reg:addr:%#x = %#x\n", reg_t2_src, rd(reg_t2_src));
		dbg_h2("frc_ds_scale_en==1: addr<0x%x, 0x%x>\n",
				prm_top->src0_fbuf_yaddr[idx],
				prm_top->src0_fbuf_caddr[idx]);
	}
}

bool frc_only_input_buf(struct dpss_ch_s *pch, struct dpss_frc_i_s *frc_i)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	unsigned int src = prm_top->src_mode;
	unsigned int index;
	struct dpss_sub_vf_s *vfs;
	struct vframe_s *vf;
	struct dpss_game_state_s *game_st;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (!pchip_st)
		return false;

	game_st = &pchip_st->game_st;

	if (!frc_i || !frc_i->in_vfm) {
		DBG_ERR("%s:no vf?\n", __func__);
		return false;
	}

	vfs = &frc_i->sub_vf_in;
	vf = frc_i->in_vfm;

	index = pch->c.in_cnt % pch->c.in_b_nub;
	if (game_st->vdin_buf_num)
		index = pch->c.in_cnt % game_st->vdin_buf_num;
	pch->c.in_cnt++;
	frc_i->inp_idx = index;
	if (pch->d->is_afbcd)
		frc_prm_top_input_afbcd(prm_top, index, vfs);
	else
		frc_prm_top_input_buffer_set(prm_top, src, index, vfs);
	dbg_h2("in:%d vdin_buf_num:%d\n", index, game_st->vdin_buf_num);
	pch->d->h_dae_vf_idx[index] = frc_i->idx;
	dbg_h2("%s:dae_vf:set:%d,%d\n", __func__, index, frc_i->idx);

	if (src == 0)
		dbg_step0_trig_input(index);
	else
		dpss_trig_input_src1(index);

	return true;
}

void frc_update_ds_scale(struct vframe_s *vfm)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;

	if (prm_top->frc_ds_scale_en == 1) {
		dbg_h1("%s vfm_type(%#x)\n", __func__, vfm->type);
		if (VFM_IS_COMP_MODE(vfm->type)) {
			vfm->type &= ~VIDTYPE_COMPRESS;
			vfm->width = prm_top->dpe_mc_size.frm_hsize;
			vfm->height = prm_top->dpe_mc_size.frm_vsize;
			dbg_h1("after: vfm_type(%#x) mc:size(%d,%d), frm(%d,%d), comp(%d,%d)\n",
				vfm->type,
				prm_top->dpe_mc_size.frm_hsize,
				prm_top->dpe_mc_size.frm_vsize,
				vfm->width, vfm->height,
				vfm->compWidth, vfm->compHeight);
		}
	}
}

void detect_axis_change(struct pvpp_dis_frc_para_in_s *in_para)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct disp_screen_vinfo_s *frc_vinfo;

	if (!in_para || !pchip_st) {
		dbg_f1("not update axis\n");
		return;
	}

	frc_vinfo = &pchip_st->vinfo_st;

	if (frc_vinfo->x_d_st != in_para->vinfo.x_d_st ||
		frc_vinfo->y_d_st != in_para->vinfo.y_d_st ||
		frc_vinfo->x_d_end != in_para->vinfo.x_d_end ||
		frc_vinfo->y_d_end != in_para->vinfo.y_d_end) {
		frc_vinfo->vinfo_chg = 1;
		update_frc_screen_vinfo(in_para);
	} else {
		frc_vinfo->vinfo_chg = 0;
	}
}

void detect_crop_change(struct frc_win_s *vpp_win, struct vframe_s *vfm)
{
	struct frc_chip_st *pchip_st;
	struct frc_cut_win_s *frc_win;
	struct PRM_DPSS_TOP *prm_top;
	u32 src_width, src_height;
	bool cut_win_chg = false;

	pchip_st = dpss_get_frc_st();

	if (!vpp_win || !pchip_st || !vfm)
		return;

	frc_win = &pchip_st->win_st;
	prm_top  = prm_dpss_top;

	if (vpp_win->x_size == 0 || vpp_win->y_size == 0)
		return;

	if (vfm->type & VIDTYPE_COMPRESS) {
		src_width = vfm->compWidth;
		src_height = vfm->compHeight;
	} else {
		src_width = vfm->width;
		src_height = vfm->height;
	}

	if (frc_win->win_hbgn != vpp_win->x_st || frc_win->win_hend != vpp_win->x_end ||
		frc_win->win_vbgn != vpp_win->y_st || frc_win->win_vend != vpp_win->y_end)
		cut_win_chg = true;

	if (vpp_win->x_size == src_width && vpp_win->y_size == src_height && !prm_top->cut_win_en)
		cut_win_chg = false;

	frc_win->cut_win_chg = cut_win_chg;
	dbg_f2("cut_win_chg=%d sizes[(%d,%d) (%d,%d)]\n",
		frc_win->cut_win_chg,
		vpp_win->x_size, vpp_win->y_size,
		src_width, src_height);
}

void frc_cut_win_chk_2_step(struct frc_win_s *vpp_win, struct vframe_s *vfm)
{
	struct PRM_DPSS_TOP *prm_top;
	struct frc_chip_st *pchip_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_cut_win_s *frc_win;
	unsigned int src_width, src_height;
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	static bool win_size_chg_flag;
	u32 src_from_nr;
	u32 align_base_h = 8, align_base_v = 8;
	u32 win_hbgn_align = 0;
	u32 win_hend_align = 0;
	u32 win_vbgn_align = 0;
	u32 win_vend_align = 0;
	u16 mc_hsize = 0;
	u16 mc_vsize = 0;
	u16 crop_top  = 0;
	u16 crop_left = 0;
	u16 crop_bottom = 0;
	u16 crop_right = 0;

	pchip_st = dpss_get_frc_st();
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data || !pchip_st || !vpp_win | !vfm) {
		DBG_ERR("frc_fw or pchip_st or win is null\n");
		win_size_chg_flag = 0;
		return;
	}
	prm_top  = prm_dpss_top;
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_top = &pfw_data->frc_top_type;
	frc_win = &pchip_st->win_st;

	if (state_st->mc_cut_position == 1)
		return;   // cut after mc

	if (!game_st->game_n2m && !game_st->game_film) {
		detect_crop_change(vpp_win, vfm);
		if (!frc_win->cut_win_chg)
			return;

		if (!state_st->mc_phase0_rdma)
			return;
	}

	if (vpp_win->x_size == 0 || vpp_win->y_size == 0) {
		dbg_f1("win size = 0, chg_fleg reset 0\n");
		win_size_chg_flag = 0;
		return;
	}
	if (vfm->type & VIDTYPE_COMPRESS) {
		src_width = vfm->compWidth;
		src_height = vfm->compHeight;
	} else {
		src_width = vfm->width;
		src_height = vfm->height;
	}

	//if (src_width > 1920) {
	//	align_base_h = 16;
	//	if (src_height > 1080)
	//		align_base_v = 16;
	//} else if (src_height > 1080) {
	//	align_base_h = 16;
	//	align_base_v = 16;
	//}

	if (prm_top->dpe_dw_dsx == 2)
		align_base_h = 16;
	if (prm_top->dpe_dw_dsy == 2)
		align_base_v = 16;

	if (frc_win->win_hbgn != vpp_win->x_st || frc_win->win_hend != vpp_win->x_end ||
		frc_win->win_vbgn != vpp_win->y_st || frc_win->win_vend != vpp_win->y_end) {
		frc_win->win_hbgn  = vpp_win->x_st;
		frc_win->win_hend  = vpp_win->x_end;
		frc_win->win_vbgn  = vpp_win->y_st;
		frc_win->win_vend  = vpp_win->y_end;
		frc_win->win_hsize = vpp_win->x_size;
		frc_win->win_vsize = vpp_win->y_size;
		frc_win->frm_hsize = vpp_win->orig_w;
		frc_win->frm_vsize = vpp_win->orig_h;
		if (vpp_win->x_size == src_width && vpp_win->y_size == src_height)
			frc_win->cut_win_en = 0;
		else
			frc_win->cut_win_en = 1;
		win_size_chg_flag = 1;
		dbg_f1("frc_cut_win_1_step:src_hv <%d,%d> win hv<%d,%d> org hv<%d,%d>\n",
				src_width, src_height,
				vpp_win->x_size, vpp_win->y_size,
				vpp_win->orig_w, vpp_win->orig_h);
		dbg_f1("frc_cut_win_1_step:win_size_chg_flag = %d\n",
				win_size_chg_flag);
		if (frc_top->mc_cut_en == 0)
			return;
	}

	if (win_size_chg_flag  == 1) {
		if (prm_top->dpss_mode == DPSS_FRC_MODE)
			src_from_nr = 0;
		else
			src_from_nr = 1;
		if (frc_win->cut_win_en == 1) {
			prm_top->cut_win_en = 1;
			state_st->cut_pre_mc_work = 1;
			prm_top->prm_cut_win.frm_hsize = src_width;
			prm_top->prm_cut_win.frm_vsize = src_height;

			win_hbgn_align = frc_align_down(frc_win->win_hbgn, align_base_h);
			win_hend_align = frc_align_up(frc_win->win_hend, align_base_h) - 1;
			win_vbgn_align = frc_align_down(frc_win->win_vbgn, align_base_v);
			win_vend_align = frc_align_up(frc_win->win_vend, align_base_v) - 1;
			if (win_vend_align > prm_top->frm_vsize)
				win_vend_align = frc_win->win_vend;

			prm_top->prm_cut_win.win_hbgn_align = win_hbgn_align;
			prm_top->prm_cut_win.win_hend_align = win_hend_align;
			prm_top->prm_cut_win.win_vbgn_align = win_vbgn_align;
			prm_top->prm_cut_win.win_vend_align = win_vend_align;

			prm_top->prm_cut_win.win_hsize_align = win_hend_align - win_hbgn_align + 1;
			prm_top->prm_cut_win.win_vsize_align = win_vend_align - win_vbgn_align + 1;

			mc_hsize = prm_top->prm_cut_win.win_hsize_align;
			mc_vsize = prm_top->prm_cut_win.win_vsize_align;
			crop_top  = win_vbgn_align;
			crop_left = win_hbgn_align;
			crop_bottom = src_height - 1 - win_vend_align;
			crop_right = frc_align_up(src_width, align_base_v) - 1 - win_hend_align;

			prm_top->prm_cut_win.win_hbgn  = frc_win->win_hbgn - win_hbgn_align;
			prm_top->prm_cut_win.win_hend  = frc_win->win_hend - win_hbgn_align;
			prm_top->prm_cut_win.win_vbgn  = frc_win->win_vbgn - win_vbgn_align;
			prm_top->prm_cut_win.win_vend  = frc_win->win_vend - win_vbgn_align;

			prm_top->prm_cut_win.win_hsize = prm_top->prm_cut_win.win_hsize_align;
			prm_top->prm_cut_win.win_vsize = prm_top->prm_cut_win.win_vsize_align;
			dbg_f1("frc_cut_win_1_step:prm_cut_win point <%d,%d,%d,%d>\n",
					prm_top->prm_cut_win.win_hbgn,
					prm_top->prm_cut_win.win_hend,
					prm_top->prm_cut_win.win_vbgn,
					prm_top->prm_cut_win.win_vend);
			dbg_f1("frc_cut_win_1_step:prm_cut_win size <%d,%d>,<%d,%d>\n",
					prm_top->prm_cut_win.win_hsize,
					prm_top->prm_cut_win.win_vsize,
					prm_top->prm_cut_win.win_hsize_align,
					prm_top->prm_cut_win.win_vsize_align);

			if (frc_top->mc_cut_en == 0)
				frc_mc_cut_0(prm_top, src_from_nr);
			else
				frc_mc_cut_1(prm_top, src_from_nr);
			//dpss_rdma_auto_wr_tri(2);
			update_mc_cut_size_to_fw(mc_hsize, mc_vsize,
						crop_top, crop_left, crop_bottom, crop_right);
			dbg_f1("cut_win_en=<%d, %d> frc_mc_cut_1_1\n",
					frc_win->cut_win_en, prm_top->cut_win_en);
		} else if (frc_win->cut_win_en == 0 && prm_top->cut_win_en == 1) {
			prm_top->cut_win_en = 0;
			state_st->cut_pre_mc_work = 1;
			if (frc_top->mc_cut_en == 0)
				frc_mc_cut_0(prm_top, src_from_nr);
			else
				frc_mc_cut_1(prm_top, src_from_nr);
			//dpss_rdma_auto_wr_tri(2);
			update_mc_cut_size_to_fw(prm_top->frm_hsize, prm_top->frm_vsize,
						0, 0, 0, 0);
			dbg_f1("cut_win_en=<%d, %d> frc_mc_cut_1_2\n",
				frc_win->cut_win_en, prm_top->cut_win_en);
		} else {
			dbg_f1("cut_win_en=<%d, %d>\n",
				frc_win->cut_win_en, prm_top->cut_win_en);
		}
		dbg_f1("frc_cut_win_2_step: win set <%d, %d, %d, %d>\n",
				vpp_win->x_st, vpp_win->x_end,
				vpp_win->y_st, vpp_win->y_end);
		win_size_chg_flag = 0;
	}
}

void frc_cut_win_set_2_step(struct frc_win_s *win, struct vframe_s *vfm)
{
	struct PRM_DPSS_TOP *prm_top;
	struct frc_chip_st *pchip_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_cut_win_s *win_s;
	unsigned int src_width, src_height;
	struct frc_state_s *state_st;
	static bool win_size_chg_flag;
	u32 temp_value, temp_value1;

	pchip_st = dpss_get_frc_st();
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data || !pchip_st || !win | !vfm) {
		DBG_ERR("%s: frc_fw or pchip_st or win is null\n", __func__);
		win_size_chg_flag = 0;
		return;
	}
	prm_top  = prm_dpss_top;
	state_st = &pchip_st->state_st;
	frc_top = &pfw_data->frc_top_type;

	if (state_st->mc_cut_position == 0)
		return;  //  // cut before mc

	if ((win->x_size == 0 || win->y_size == 0) && state_st->win_size_zero_flag)
		return;

	prm_top->cut_win_en = 0;
	if (vfm->type & VIDTYPE_COMPRESS) {
		src_width = vfm->compWidth;
		src_height = vfm->compHeight;
	} else {
		src_width = vfm->width;
		src_height = vfm->height;
	}
	win_s    = &pchip_st->win_st;
	if (win_s->win_hbgn != win->x_st || win_s->win_hend != win->x_end ||
		win_s->win_vbgn != win->y_st || win_s->win_vend != win->y_end) {
		win_s->win_hbgn  = win->x_st;
		win_s->win_hend  = win->x_end;
		win_s->win_vbgn  = win->y_st;
		win_s->win_vend  = win->y_end;
		dbg_f1("cut_win_2_step:src_hv<%d,%d>,win st<%d,%d>,win end<%d,%d>,chg_flag:%d\n",
				src_width, src_height,
				win->x_st, win->y_st,
				win->x_end, win->y_end,
				win_size_chg_flag);
		//wr(DPSS_DPE_MC_WIN_CUT_H, temp_value);
		//temp_value = win_s->win_vbgn | (win_s->win_vend << 16);
		//wr(DPSS_DPE_MC_WIN_CUT_V, temp_value);
		// set size register will result to garbage display
#ifdef USE_FRC_PRE_VS_RDMA
		if (rdma_enable()) {
			temp_value = win_s->win_hbgn | (win_s->win_hend << 16);
			DPSS_RDMA_WR_VS(DPSS_DPE_MC_WIN_CUT_H, temp_value);
			temp_value1 = win_s->win_vbgn | (win_s->win_vend << 16);
			DPSS_RDMA_WR_VS(DPSS_DPE_MC_WIN_CUT_V, temp_value1);
			// dpss_rdma_auto_wr_tri(2); // use new rdam function
			win_size_chg_flag = 0;
		} else {
			win_size_chg_flag = 1;
			return;
		}
#else
		win_size_chg_flag = 1;
		return;
#endif
	}
	if (win_size_chg_flag  == 1) {
		temp_value = win_s->win_hbgn | (win_s->win_hend << 16);
		wr(DPSS_DPE_MC_WIN_CUT_H, temp_value);
		temp_value = win_s->win_vbgn | (win_s->win_vend << 16);
		wr(DPSS_DPE_MC_WIN_CUT_V, temp_value);
		// w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 1, 8, 1);
		dbg_f1("frc_cut_win_2_en=<%d, %d> set\n",
			win_s->cut_win_en, prm_top->cut_win_en);
		dbg_f1("frc_cut_win_2_step: win set <%d, %d, %d, %d> set\n",
				win->x_st, win->x_end,
				win->y_st, win->y_end);
		win_size_chg_flag = 0;
	}
}

void frc_transform_check(struct vframe_s *vfm)
{
	struct frc_chip_st *pchip;
	struct dpss_frc_fw_data_s *pfw_data;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	int reverse, mirror;
	u32 plink_rev_mode = 0, frc_rev_mode = 0, rev_x = 0, uv_swap = 0;
	struct DPSS_MC0_TYPE *prm_mc;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_state_s *state_st;
	struct PRM_INTF_TYPE *pix_rmif;

	pchip = dpss_get_frc_st();
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();

	if (!pchip || !pfw_data || !vfm)
		return;

	prm_mc = &pchip->prm_mc;
	pix_rmif = &pchip->mc_pix_rmif;
	frc_top = &pfw_data->frc_top_type;
	state_st = &pchip->state_st;

	reverse = frc_reverse_panel;
	mirror = frc_mirror_panel;

	if (reverse)
		plink_rev_mode |= DPSS_HV_MIRROR;
	else if (mirror & DPSS_H_MIRROR)
		plink_rev_mode |= DPSS_H_MIRROR;
	else if (mirror & DPSS_V_MIRROR)
		plink_rev_mode |= DPSS_V_MIRROR;

	frc_rev_mode = prm_top->dp_set_rev_mode ^ plink_rev_mode;

	if (state_st->force_frc_rev_mode)
		frc_rev_mode = state_st->force_frc_rev_mode;

	rev_x = frc_rev_mode & DPSS_H_MIRROR;
	if (((prm_top->dpss_mode == DPSS_FRC_MODE && !(vfm->type & VIDTYPE_COMPRESS)) ||
		(prm_top->dpss_mode != DPSS_FRC_MODE &&
		prm_top->nro_fs_fmt.src_cmpr == CMPR_UN)) && rev_x)
		uv_swap = 1;
	prm_top->frc_rev_mode = frc_rev_mode;
	pix_rmif->uv_swap = uv_swap;
	prm_mc->mc_rev_mode = prm_top->frc_rev_mode;
	frc_top->frc_rev_mode = prm_top->frc_rev_mode & 0x3;
	state_st->reverse_need_update = frc_rev_mode != 0 ? true : false;

	dbg_f1("reverse:%d mirror:%d frc_rev_mode:%d[%d,%d]force:%d uv_swap:%d\n",
			reverse, mirror, prm_top->frc_rev_mode, prm_top->dp_set_rev_mode,
			plink_rev_mode, state_st->force_frc_rev_mode, pix_rmif->uv_swap);
}

bool is_vd1_link_state(void)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	bool is_vd1_link = rd_vc(VPU_AXIRD_PATH_CTRL) & 0x1;
	bool ret = false;

	if (!pchip_st)
		return is_vd1_link;

	state_st = &pchip_st->state_st;

	if (is_vd1_link && !state_st->is_frc_vpp_link)
		ret = true;
	if (is_vd1_link == state_st->is_frc_vpp_link)
		dbg_f1("vd1_link_state_sw=%d(%d,%d)\n",
			ret, is_vd1_link, !state_st->is_frc_vpp_link);
	return ret;
}

static void check_readback_reg(u8 reg_type)
{
	struct frc_chip_st *pchip_st;
	struct frc_debug_s *dbg_st;
	u32 dpss_dpe0_out_idx;
	u32 dpss_dpe1_out_idx;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	dbg_st = &pchip_st->dbg_st;
	if (dbg_st->ctrl_dbg.check_reg_status == 0)
		return;

	if (reg_type == BIT_0 &&
		(dbg_st->ctrl_dbg.check_reg_status & reg_type) == reg_type) {  // check mc cut reg
		dbg_f1("check: mc_cut reg(%#x,%#x), mc_phase(%#x)",
			rd(DPSS_DPE_MC_WIN_CUT_H),
			rd(DPSS_DPE_MC_WIN_CUT_V),
			rd(DPSS_DPE_MC_PHASE));
	} else if (reg_type == BIT_1 &&
		(dbg_st->ctrl_dbg.check_reg_status & reg_type) == reg_type) {
		dbg_f1("vd pps blend regs:\n");
		dbg_f1("prebld_h_size[0x1d20] = %#x\n", rd_vc(0x1d20));
		dbg_f1("prebld_h_start_end[0x1d1a] = %#x\n", rd_vc(0x1d1a));
		dbg_f1("prebld_v_start_end[0x1d1b] = %#x\n", rd_vc(0x1d1b));
		dbg_f1("pstbld_h_size[0x1d21] = %#x\n", rd_vc(0x1d21));
		dbg_f1("pstbld_h_start_end[0x1d1c] = %#x\n", rd_vc(0x1d1c));
		dbg_f1("pstbld_v_start_end[0x1d1d] = %#x\n", rd_vc(0x1d1d));
	} else if (reg_type == BIT_2 &&
		(dbg_st->ctrl_dbg.check_reg_status & reg_type) == reg_type) {
		dbg_f1("vsr pi regs:\n");
		dbg_f1("pi_en_mode[0x5048] = %#x\n", rd_vc(0x5048));
		dbg_f1("pi_in_hsc_part[0x505b] = %#x\n", rd_vc(0x505b));
		dbg_f1("pi_in_hsc_ini[0x505c] = %#x\n", rd_vc(0x505c));
		dbg_f1("pi_in_vsc_part[0x505d] = %#x\n", rd_vc(0x505d));
		dbg_f1("pi_in_vsc_ini[0x505e] = %#x\n", rd_vc(0x505e));
		dbg_f1("pi_hf_hsc_part[0x5057] =%#x\n", rd_vc(0x5057));
		dbg_f1("pi_hf_hsc_ini[0x5058] = %#x\n", rd_vc(0x5058));
		dbg_f1("pi_hf_vsc_part[0x5059] = %#x\n", rd_vc(0x5059));
		dbg_f1("pi_hf_vsc_ini[0x505a] = %#x\n", rd_vc(0x505a));
	} else if (reg_type == BIT_3 &&
		(dbg_st->ctrl_dbg.check_reg_status & reg_type) == reg_type) {
		dbg_f1("vsr safa regs:\n");
		dbg_f1("safa_pps_hsc_start_phase_step[0x5102] = %#x\n", rd_vc(0x5102));
		dbg_f1("safa_pps_vsc_start_phase_step[0x5101] = %#x\n", rd_vc(0x5101));
		dbg_f1("safa_pps_vsc_init[0x510b] = %#x\n", rd_vc(0x510b));
		dbg_f1("safa_pps_hsc_init[0x510c] = %#x\n", rd_vc(0x510c));
		dbg_f1("safa_pps_scale_idx_luma[0x5195] = %#x\n", rd_vc(0x5195));
		dbg_f1("safa_pps_scale_idx_chro[0x5197] = %#x\n", rd_vc(0x5197));
		dbg_f1("safa_pps_bot_vsc_init[0x519a] = %#x\n", rd_vc(0x519a));
	} else if (reg_type == BIT_4 &&
		(dbg_st->ctrl_dbg.check_reg_status & reg_type) == reg_type) {
		dbg_f1("vfcd-mif_scoe (%#x,%#x,%#x,%#x)\n",
			rd_vc(0x690e), rd_vc(0x690f), rd_vc(0x6916), rd_vc(0x6917));
		dbg_f1("vfcd-pix_pos (%#x,%#x,%#x,%#x)\n",
			rd_vc(0x696b), rd_vc(0x696c), rd_vc(0x696d), rd_vc(0x696e));
	} else if (reg_type == BIT_5 &&
		(dbg_st->ctrl_dbg.check_reg_status & reg_type) == reg_type) {
		dbg_f1("dpe_out_idx_regs:\n");
		dpss_dpe1_out_idx = rd(DPSS_FBUF_DPE_LOOP_IDX) & 0xff;
		dbg_f1("ro_dpe1_out_w_idx[0x83f1] = %2d [%#x]\n",
			(dpss_dpe1_out_idx >> 4), dpss_dpe1_out_idx);
		// dpss_dpe0_out_idx = rd(FRC_DPSS_DPE_OUT_IDX) & 0xff;
		dpss_dpe0_out_idx = rd(FRC_DPSS_DISP_BUFF_INFO_0);
		dbg_f1("ro_dpe0_out_r_pre/nxt_idx[0x80d0] = %2d/%2d\n",
			(dpss_dpe0_out_idx >> 2) & 0xf,
			(dpss_dpe0_out_idx >> 8) & 0xf);
	}
}

void get_nr_buf_idx(struct vframe_s *vfm, struct frc_state_s *state_st)
{
	struct dpss_nr_i_s *nr_i;

	if (!vfm || !state_st)
		return;

	nr_i = lk_get_nr_i(vfm);

	if (!nr_i)
		return;

	state_st->nr_buf_idx = nr_i->nr_buf_idx;
	pr_frc(1, "get nr_buf_idx=%d\n", state_st->nr_buf_idx);
}

void update_frc_screen_vinfo(struct pvpp_dis_frc_para_in_s *in_para)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct disp_screen_vinfo_s *frc_vinfo;

	if (!in_para || !pchip_st) {
		dbg_f1("not update frc vinfo\n");
		return;
	}

	frc_vinfo = &pchip_st->vinfo_st;

	frc_vinfo->vtotal = in_para->vinfo.vtotal;
	frc_vinfo->htotal = in_para->vinfo.htotal;
	frc_vinfo->width = in_para->vinfo.width;
	frc_vinfo->height = in_para->vinfo.height;
	frc_vinfo->frequency = in_para->vinfo.frequency;
	frc_vinfo->x_d_st = in_para->vinfo.x_d_st;
	frc_vinfo->y_d_st = in_para->vinfo.y_d_st;
	frc_vinfo->x_d_end = in_para->vinfo.x_d_end;
	frc_vinfo->y_d_end = in_para->vinfo.y_d_end;
	frc_vinfo->x_d_size = in_para->vinfo.x_d_size;
	frc_vinfo->y_d_size = in_para->vinfo.y_d_size;
}

//check_fallback
static bool frc_get_current_frame(void)
{
	u8 inp_idx;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;

	if (!pchip_st)
		return false;

	state_st = &pchip_st->state_st;
	inp_idx = display_buf_q.inp_idx % DPSS_QUEEN_NUM;
	if (inp_idx == 0)
		inp_idx = DPSS_QUEEN_NUM - 1;
	else
		inp_idx--;

	state_st->force_mc_cur_idx = display_buf_q.data[inp_idx].n;
	pr_frc(1, "force_mc_cur_idx=%d\n", state_st->force_mc_cur_idx);

	return true;
}

unsigned int dpss_dbg_mc_byp_cnt = 0x5;
module_param_named(dpss_dbg_mc_byp_cnt, dpss_dbg_mc_byp_cnt, uint, 0664);

int pvpp_display_frc_game(struct vframe_s *vfm,
			struct pvpp_dis_frc_para_in_s *in_para,
			void *out_para)
{
	int ret = 0;
	//struct pvpp_dis_frc_para_in_s *in;
	//static int last_x, last_v;
	enum EPVPP_FRC_API_MODE mode;
	enum EPVPP_DISPLAY_FRC_MODE display_mode;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	bool is_vd1_link = false;

	if (!vfm || !pchip_st)
		return ret;

	if (!in_para) {
		dbg_h1("pvpp_game: in_para is null\n");
		return ret;
	}

	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;

	if (!state_st->dpss_reg || !state_st->dpe_ready)
		return ret;

	is_vd1_link = is_vd1_link_state();

	// if (is_vd1_link && state_st->need_switch_to_vd1)
		// state_st->need_switch_to_vd1 = false;

	if (!enable_mc_link && state_st->mc_set_phase0) {
		state_st->is_frc_vpp_link = 0;
		dbg_f1("\tret:0 enable_mc_link:%d mc_set_phase0:%d\n",
				enable_mc_link,
				state_st->mc_set_phase0);
		return ret;
	}
	if (state_st->need_disable_mc_link || !game_st->mc_link_en) {
		state_st->is_frc_vpp_link = 0;
		return ret;
	}
	if (state_st->unformat_bypass &&
		state_st->special_format == 1) {
		state_st->is_frc_vpp_link = 0;
		dbg_f1("\tret:0 unformat_bypass:%d special_format:%d\n",
				state_st->unformat_bypass,
				state_st->special_format);
		return ret;
	}

	if (game_st->mc_need_byp_en)
		return ret;

	//get_nr_buf_idx(vfm, state_st);
	//frc_cut_win_check(&in_para->win, vfm);
	if (dpss_dae_frm_cnt_src0 <= state_st->enable_frclink_cnt) {
		dbg_h1("\tret:0 dae0_cnt:%d link_cnt:%d\n", dpss_dae_frm_cnt_src0,
			state_st->enable_frclink_cnt);
		return ret;
	}

	mode = in_para->link_mode;
	display_mode = in_para->dmode;
	if (in_para) {
		if (in_para->vinfo.x_d_size * in_para->vinfo.y_d_size <
			in_para->vinfo.width * in_para->vinfo.height) {
			dbg_h1("pvpp_game: litter_win bypass game frc\n");
			state_st->is_frc_vpp_link = 0;
			return ret;
		}

		dbg_h1("pvpp_game in_para win:size(%d,%d),start(%d,%d),end(%d,%d),orig(%d,%d)\n",
			in_para->win.x_size, in_para->win.y_size,
			in_para->win.x_st, in_para->win.y_st,
			in_para->win.x_end, in_para->win.y_end,
			in_para->win.orig_w, in_para->win.orig_h);
		dbg_h1("pvpp_game in_para vinfo:size(%d,%d)(%d,%d),start(%d,%d),end(%d,%d)\n",
			in_para->vinfo.width, in_para->vinfo.height,
			in_para->vinfo.x_d_size, in_para->vinfo.y_d_size,
			in_para->vinfo.x_d_st, in_para->vinfo.y_d_st,
			in_para->vinfo.x_d_end, in_para->vinfo.y_d_end);
	}

	if (display_mode == EPVPP_DISPLAY_MODE_FRC_BYPASS) {
		if (state_st->is_frc_vpp_link) {
			// state_st->need_switch_to_vd1 = true;
			state_st->is_frc_vpp_link = 0;
			state_st->have_update_vfcd = 0;
			dbg_f1("pvpp_game bypass frc\n");
		}
		if (is_vd1_link)
			w_reg_bit(DPSS_DPE_MC_START_CTRL, 3, 0, 2);
		ret = 0;
	} else if (display_mode == EPVPP_DISPLAY_MODE_FRC) {
		if (!state_st->is_frc_vpp_link) {
			state_st->is_frc_vpp_link = 1;
			state_st->have_update_vfcd = 1;
			state_st->mc_set_phase0 = false;
			wr(DPSS_DPE_MC_TOP_RESET, BIT_25); // RESET BIT 25
			w_reg_bit(DPSS_DPE_MC_START_CTRL, 2, 0, 2);
			hw_mc_vfcd_cfg_update();
			state_st->mc_bypass = true;
			state_st->mc_bypass_cnt = dpss_dbg_mc_byp_cnt & 0xffff;
			frc_set_mc_bypass(3);
			dbg_f1("pvpp_game enable frc\n");
		}
		hw_mc_reverse_update();
		frc_cut_win_chk_2_step(&in_para->win, vfm);
		frc_cut_win_set_2_step(&in_para->win, vfm);
		check_readback_reg(BIT_0);
		check_readback_reg(BIT_5);
		ret = 1;
	}
	check_readback_reg(BIT_1);
	check_readback_reg(BIT_2);
	check_readback_reg(BIT_3);
	check_readback_reg(BIT_4);
	return ret;
}

int pvpp_display_frc(struct vframe_s *vfm,
			struct pvpp_dis_frc_para_in_s *in_para,
			void *out_para)
{
	int ret = 0;
	enum EPVPP_FRC_API_MODE mode;
	enum EPVPP_DISPLAY_FRC_MODE display_mode;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	struct frc_cut_win_s *frc_win;
	bool is_vd1_link = false;

	if (!pchip_st)
		return ret;

	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_win = &pchip_st->win_st;

	if (game_st->game_n2m || game_st->game_film) { // game frc
		ret = pvpp_display_frc_game(vfm, in_para, out_para);
		return ret;
	}

	if (!vfm || !in_para) {
		state_st->is_frc_vpp_link = 0;
		return ret;
	}

	if (!state_st->dpss_reg || !state_st->dpe_ready || !enable_mc_link ||
		state_st->need_disable_mc_link || state_st->disable_mc_link_intf ||
		(state_st->unformat_bypass && state_st->special_format == 1) ||
		dpss_dae_frm_cnt_src0 <= state_st->enable_frclink_cnt)
		goto exit;

	is_vd1_link = is_vd1_link_state();

	mode = in_para->link_mode;
	display_mode = in_para->dmode;
	if (in_para) {
		dbg_h1("in_para win:size(%d,%d),start(%d,%d),end(%d,%d), orig(%d,%d)\n",
			in_para->win.x_size, in_para->win.y_size,
			in_para->win.x_st, in_para->win.y_st,
			in_para->win.x_end, in_para->win.y_end,
			in_para->win.orig_w, in_para->win.orig_h);
		dbg_h1("in_para vinfo:size(%d,%d)(%d,%d),start(%d,%d),end(%d,%d)\n",
			in_para->vinfo.width, in_para->vinfo.height,
			in_para->vinfo.x_d_size, in_para->vinfo.y_d_size,
			in_para->vinfo.x_d_st, in_para->vinfo.y_d_st,
			in_para->vinfo.x_d_end, in_para->vinfo.y_d_end);
	}

	if (display_mode == EPVPP_DISPLAY_MODE_FRC_BYPASS) {
		if (state_st->is_frc_vpp_link) {
			state_st->is_frc_vpp_link = 0;
			state_st->have_update_vfcd = 0;
			state_st->check_fallback = 0;
			state_st->mc_link_available = false;
			state_st->dae0_available_cnt = 0;
			dbg_f1("bypass frc\n");
		}
		if (is_vd1_link)
			w_reg_bit(DPSS_DPE_MC_START_CTRL, 3, 0, 2);
		ret = 0;
	} else if (display_mode == EPVPP_DISPLAY_MODE_FRC) {
		if (is_vd1_link && (!state_st->mc_set_phase0 || !state_st->mc_link_available)) {
			state_st->need_set_phase0 = true;
			state_st->is_frc_vpp_link = 0;
			dbg_f1("need_set_phase0=%d, mc_link_avail=%d\n",
				state_st->need_set_phase0, state_st->mc_link_available);
			return ret;
		}
		if (is_vd1_link && state_st->check_fallback == 0) {
			if (frc_get_current_frame())
				state_st->check_fallback = 1;
			return ret;
		}
		state_st->need_set_phase0 = false;
		if (!state_st->is_frc_vpp_link) {
			state_st->is_frc_vpp_link = 1;
			state_st->have_update_vfcd = 1;
			wr(DPSS_DPE_MC_TOP_RESET, BIT_25); // RESET BIT 25
			w_reg_bit(DPSS_DPE_MC_START_CTRL, 2, 0, 2);
			hw_mc_vfcd_cfg_update();
			state_st->mc_bypass = true;
			state_st->mc_bypass_cnt = 1;
			frc_set_mc_bypass(3);
			dbg_f1("enable frc\n");
		}
		hw_mc_reverse_update();
		frc_cut_win_chk_2_step(&in_para->win, vfm);
		frc_cut_win_set_2_step(&in_para->win, vfm);
		check_readback_reg(BIT_0);
		check_readback_reg(BIT_5);
		ret = 1;
	}
	check_readback_reg(BIT_1);
	check_readback_reg(BIT_2);
	check_readback_reg(BIT_3);
	check_readback_reg(BIT_4);
	return ret;

exit:
	if (state_st->is_frc_vpp_link) {
		state_st->is_frc_vpp_link = 0;
		state_st->have_update_vfcd = 0;
		state_st->check_fallback = 0;
		state_st->mc_link_available = false;
		state_st->dae0_available_cnt = 0;
		dbg_f2("reg=%d,dpe_ready=%d,enable_mc_link=%d,dis_mc_link=%d,%d,unformat=%d,%d\n",
			state_st->dpss_reg, state_st->dpe_ready, enable_mc_link,
			state_st->need_disable_mc_link, state_st->disable_mc_link_intf,
			state_st->unformat_bypass, state_st->special_format);
	}
	return 0;
}

int frclink_vpp_check_vf(struct vframe_s *vfm)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;

	if (!vfm || !pchip_st)
		return -1;

	state_st = &pchip_st->state_st;

	if (!state_st->dpss_reg)
		return 0;
	return 1;
}

bool frclink_vpp_check_act(void)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct vinfo_s *vinfo = get_current_vinfo();
	u16  cur_frm_rate = 0;
	u32 width;
	u32 height;

	if (!pchip_st)
		return false;
	state_st = &pchip_st->state_st;

	if (!state_st->dpss_reg)
		return false;

	if (vinfo->sync_duration_den > 0)
		cur_frm_rate = vinfo->sync_duration_num / vinfo->sync_duration_den;
	else
		cur_frm_rate = 0;

	if (cur_frm_rate == 0)
		return false;

	width = vinfo->width;
	height = vinfo->height;
	if (pchip_st->chip == ID_T6W) {
		if (cur_frm_rate > 144 && cur_frm_rate < 200)
			return false;
	} else if (pchip_st->chip == ID_T6X) {
		if ((width == 3840 && height == 2160) &&
			(cur_frm_rate > 150 && cur_frm_rate < 199))
			return false;
		else if ((width == 3840 && height == 1080) &&
			(cur_frm_rate > 288))
			return false;
	}
	dbg_h1("frclink chk act done\n");
	return true;
};

int pvpp_sw_frc(bool on)
{
	return 1;
}

bool get_mc_link_property_change(void)
{
	bool is_vd1_link = false;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return false;
	state_st = &pchip_st->state_st;
	is_vd1_link = is_vd1_link_state();
	if (!is_vd1_link || !state_st->dpss_reg || !enable_mc_link ||
		state_st->need_disable_mc_link || state_st->disable_mc_link_intf)
		return false;
	dbg_f2("notify vpp enable mc link\n");
	return true;
}

void dpss_enable_mc_link(bool enable)
{
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;
	state_st = &pchip_st->state_st;

	state_st->disable_mc_link_intf = !enable;
	dbg_f1("disable_mc_link_intf=%d\n", state_st->disable_mc_link_intf);
}

void frc_unreg_hw(u32 ch)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	unsigned int wait_cnt = 0, vout_hz = 0, vsync_us = 0, wait_cnt_max = 200;
	struct vinfo_s *vinfo;
	u32 delay_us = 0;

	if (!pchip_st || ch)
		return;

	state_st = &pchip_st->state_st;

	state_st->dpss_reg = 0;
	vinfo = get_current_vinfo();
	if (vinfo && vinfo->sync_duration_den) {
		vout_hz = vinfo->sync_duration_num / vinfo->sync_duration_den;
		vout_hz = vinfo->brr_duration ? vinfo->brr_duration : vout_hz;
		if (vout_hz) {
			vsync_us = 1000000 / vout_hz;
			wait_cnt_max = (35 * vsync_us / 10) / 500; /* 3.5 vsync time */
		}
	}

	/* wait for frc link off */
	while (!is_vd1_link_state() && wait_cnt < wait_cnt_max) {
		usleep_range(500, 600);
		wait_cnt++;
	}

	w_reg_bit(FRC_REG_INPUT_SIZE, 0, 30, 1); //off frc hw

	/* wait fixed vsync frames before release buf*/
	if (vsync_us && state_st->frc_off_delay_vs_cnt) {
		delay_us = vsync_us * state_st->frc_off_delay_vs_cnt;
		if (delay_us > 2000000)
			delay_us = 2000000;

		if (delay_us >= 20000) {
			msleep(DIV_ROUND_UP((unsigned int)delay_us, 1000));
		} else {
			usleep_range((unsigned int)delay_us,
						(unsigned int)delay_us + 100);
		}
	}
	w_reg_bit(DPSS_DPE_MC_MIF_CTRL0, 0x0, 20, 8);

	dbg_h1("frc unreg wait_cnt:%d(%dms), wait_cnt_max:%d wait frc stop(%dms)\n",
		wait_cnt, wait_cnt * 500 / 1000, wait_cnt_max, delay_us / 1000);
}

void frc_unreg_clear_state(u32 ch)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;

	if (!pchip_st || ch)
		return;

	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;

	state_st->dpss_reg = 0;
	state_st->is_frc_vpp_link = 0;
	state_st->inp_alg_worked = 0;
	state_st->put_frame_cnt = 0;
	state_st->src0_disp_obuf_rdy = 0;
	state_st->frc_en = false;
	dpss_inp_frm_cnt = 0;
	memset(&state_st->frc_int_st, 0, sizeof(state_st->frc_int_st));

	if (game_st->game_n2m || game_st->game_film) {
		// exit dpss need disable frc vrr
		game_st->off_game_mode = true;
		game_st->game_n2m = 0;
		game_st->game_film = 0;
		venc_vrr_vdin();
	}

	if (game_st->video_mute_en) {
		game_st->video_mute_en = false;
		set_video_mute(FRC_MUTE_SET, false);
	}

	dpss_frc_rdma_unreg();
}

void frc_reg_update_state(u32 ch)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;

	if (!pchip_st || ch)
		return;
	state_st = &pchip_st->state_st;

	pchip_st->state_st.dpss_reg = 1;
	pchip_st->state_st.is_first_frame = 1;
	dpss_frc_rdma_reg();
}

/* Test whether demo window works properly for t6w
 * input: dome window num (1/2/3/4)
 * input: style: 0 for demo, 1 for debug
 * output: number of demo window
 */
void dpss_frc_demo_win(u8 demo_num, u8 style)
{
	u32 hsize, vsize;
	u32 tmp, tmp_x, tmp_y;
	static u8 demo_style;
	struct frc_chip_st *pchip_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st) {
		DBG_ERR("%s pchip_st is null\n", __func__);
		return;
	}

	tmp = rd(DPSS_DPE_MC_TOP_FSIZE);
	hsize = tmp >> 16 & 0xffff;
	vsize = tmp & 0xffff;

	if (!style)
		demo_style = 0;

	if (demo_num)
		pchip_st->state_st.demo_win_en = true;
	else
		pchip_st->state_st.demo_win_en = false;

	if (demo_num == 0) {
		w_reg_bit(FRC_MC_DEMO_WINDOW, 0, 0, 5);
		w_reg_bit(DPSS_DPE_MC_MC_DBG, 0, 17, 4);
		w_reg_bit(FRC_REG_MC_DEBUG1, 0, 17, 4);
		demo_style = 0;
	} else if (demo_num == 1) {
		if (!demo_style) {
			tmp_x = (hsize / 2) << 16;
			tmp_y = (vsize - 1) << 16;
		} else if (demo_style == 1) {
			tmp_x = hsize << 16;
			tmp_y = (vsize / 2) << 16;
		} else {
			tmp_x = (hsize / 4) * 3 << 16 | (hsize / 4);
			tmp_y = (vsize / 4) * 3 << 16 | vsize / 4;
		}
		demo_style++;
		if (demo_style > 2)
			demo_style = 0;
		// set demo window-1 position
		wr(DPSS_DPE_MC_DEMO_WIN1_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN1_Y, tmp_y);
		// enable demo window1
		//w_reg_bit(DPSS_DPE_MC_MC_DBG, 0x1, 17, 4);
		//w_reg_bit(FRC_REG_MC_DEBUG1, 0x1, 17, 4);
		w_reg_bit(FRC_MC_DEMO_WINDOW, 0x18, 0, 5);
	} else if (demo_num == 2) {
		tmp_x = (hsize / 2) << 16;
		tmp_y = (vsize / 2) << 16;
		// set demo window position
		wr(DPSS_DPE_MC_DEMO_WIN1_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN1_Y, tmp_y);

		tmp_x = hsize << 16 | hsize / 2;
		tmp_y = vsize << 16 | vsize / 2;
		wr(DPSS_DPE_MC_DEMO_WIN2_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN2_Y, tmp_y);

		// enable demo window
		//w_reg_bit(DPSS_DPE_MC_MC_DBG, 0x3, 17, 4);
		//w_reg_bit(FRC_REG_MC_DEBUG1, 0x3, 17, 4);
		w_reg_bit(FRC_MC_DEMO_WINDOW, 0x1c, 0, 5);
	} else if (demo_num == 3) {
		tmp_x = (hsize / 10) * 3 << 16;
		tmp_y = vsize << 16;
		// set demo window1 position
		wr(DPSS_DPE_MC_DEMO_WIN1_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN1_Y, tmp_y);

		tmp_x = (hsize / 10) * 7 << 16 | (hsize / 10) * 3;
		tmp_y = vsize << 16;
		// set demo window2 position
		wr(DPSS_DPE_MC_DEMO_WIN2_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN2_Y, tmp_y);

		tmp_x = (hsize) << 16 | (hsize / 10) * 7;
		tmp_y = (vsize) << 16;
		// set demo window3 position
		wr(DPSS_DPE_MC_DEMO_WIN3_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN3_Y, tmp_y);

		// enable demo window
		//w_reg_bit(DPSS_DPE_MC_MC_DBG, 0x7, 17, 4);
		//w_reg_bit(FRC_REG_MC_DEBUG1, 0x7, 17, 4);
		w_reg_bit(FRC_MC_DEMO_WINDOW, 0x1e, 0, 5);
	} else if (demo_num == 4) {
		tmp_x = (hsize / 10) * 4 << 16 | hsize / 10;
		tmp_y = (vsize / 10) * 4 << 16 | vsize / 10;
		// set demo window1 position
		wr(DPSS_DPE_MC_DEMO_WIN1_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN1_Y, tmp_y);

		tmp_x = (hsize / 10) * 9 << 16 | (hsize / 10) * 6;
		tmp_y = (vsize / 10) * 4 << 16 | (vsize / 10);
		// set demo window2 position
		wr(DPSS_DPE_MC_DEMO_WIN2_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN2_Y, tmp_y);

		tmp_x = (hsize / 10) * 4 << 16 | (hsize / 10);
		tmp_y = (vsize / 10) * 9 << 16 | (vsize / 10) * 6;
		// set demo window3 position
		wr(DPSS_DPE_MC_DEMO_WIN3_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN3_Y, tmp_y);

		tmp_x = (hsize / 10) * 9 << 16 | (hsize / 10) * 6;
		tmp_y = (vsize / 10) * 9 << 16 | (vsize / 10) * 6;
		// set demo window4 position
		wr(DPSS_DPE_MC_DEMO_WIN4_X, tmp_x);
		wr(DPSS_DPE_MC_DEMO_WIN4_Y, tmp_y);

		// enable demo window
		//w_reg_bit(DPSS_DPE_MC_MC_DBG, 0xf, 17, 4);
		//w_reg_bit(FRC_REG_MC_DEBUG1, 0xf, 17, 4);
		w_reg_bit(FRC_MC_DEMO_WINDOW, 0x1f, 0, 5);
	}

	dbg_h1("%s demo_num:%d, demo_style :%d\n",
			__func__, demo_num, demo_style);
}
EXPORT_SYMBOL(dpss_frc_demo_win);

void frc_set_mc_bypass(u8 bypass)
{
	if (bypass == 1) {
		w_reg_bit(FRC_MC_DBG_PHASE_EN, 0x3, 16, 2);
		w_reg_bit(FRC_MC_HW_CTRL0, 0x1, 0, 2);
	} else if (bypass == 3) {
		w_reg_bit(FRC_MC_DBG_PHASE_EN, 0x3, 16, 2);
		w_reg_bit(FRC_MC_HW_CTRL0, 0x3, 0, 2);
	} else if (bypass == 0) {
		w_reg_bit(FRC_MC_DBG_PHASE_EN, 0x0, 16, 2);
		w_reg_bit(FRC_MC_HW_CTRL0, 0x0, 0, 2);
	}
}

void frc_set_mc_bypass_rdma(u8 bypass)
{
	if (bypass == 1) {
		DPSS_RDMA_WR_BIT_PRE_VS(FRC_MC_DBG_PHASE_EN, 0x3, 16, 2);
		DPSS_RDMA_WR_BIT_PRE_VS(FRC_MC_HW_CTRL0, 0x1, 0, 2);
	} else if (bypass == 3) {
		DPSS_RDMA_WR_BIT_PRE_VS(FRC_MC_DBG_PHASE_EN, 0x3, 16, 2);
		DPSS_RDMA_WR_BIT_PRE_VS(FRC_MC_HW_CTRL0, 0x3, 0, 2);
	} else if (bypass == 0) {
		DPSS_RDMA_WR_BIT_PRE_VS(FRC_MC_DBG_PHASE_EN, 0x0, 16, 2);
		DPSS_RDMA_WR_BIT_PRE_VS(FRC_MC_HW_CTRL0, 0x0, 0, 2);
	}
}

void dpss_frc_set_mc_bypass(u8 enable_mc)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	u8 memc_stats = 0;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return;
	frc_top = &pfw_data->frc_top_type;
	if (frc_top->memc_enable == 2) {
		memc_stats = rd(FRC_MC_HW_CTRL0) & 0x1;
		frc_top->memc_enable = memc_stats ? 0 : 1;
	}
	if (enable_mc == frc_top->memc_enable)
		return;
	pr_frc(1, "ext_ctrl: memc_enable :%d\n", enable_mc);
	if (enable_mc == 0) {
		w_reg_bit(FRC_MC_DBG_PHASE_EN, 0x3, 16, 2);
		w_reg_bit(FRC_MC_HW_CTRL0, 0x3, 0, 2);
	} else  { //  if (enable_mc == 0)
		w_reg_bit(FRC_MC_DBG_PHASE_EN, 0x0, 16, 2);
		w_reg_bit(FRC_MC_HW_CTRL0, 0x0, 0, 2);
	}
	frc_top->memc_enable = enable_mc;
}
EXPORT_SYMBOL(dpss_frc_set_mc_bypass);

void dpss_set_mc_link_state(u8 enable_mc_link)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data || !pchip_st)
		return;

	frc_top = &pfw_data->frc_top_type;

	if (enable_mc_link == frc_top->memc_enable)
		return;

	pr_frc(1, "ext_ctrl: link_en:%d\n", enable_mc_link);
	if (enable_mc_link) {
		pchip_st->state_st.need_disable_mc_link = false;
		pchip_st->state_st.ui_set_n2m = false;
	} else {
		pchip_st->state_st.need_disable_mc_link = true;
		pchip_st->state_st.ui_set_n2m = true;
	}

	frc_top->memc_enable = enable_mc_link;
	pr_frc(1, "need_disable_mc_link=%d\n", pchip_st->state_st.need_disable_mc_link);
	usleep_range(500, 800);
}
EXPORT_SYMBOL(dpss_set_mc_link_state);

int dpss_frc_get_enable(void)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return 0;
	frc_top = &pfw_data->frc_top_type;
	return frc_top->memc_enable;
}

void dpss_frc_set_dejudder(u8 dejudder_level)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data) {
		frc_top = &pfw_data->frc_top_type;
	} else {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;	// add abnormal case
	}
	pr_frc(1, "ext_ctrl: set_memc_level:%d\n", dejudder_level);
	if (dejudder_level == pfw_data->frc_top_type.frc_memc_level)
		return;

	if (dejudder_level == FPP_MEMC_24PFILM) {
		pfw_data->frc_top_type.frc_memc_level_1 = 2;
		pfw_data->frc_top_type.frc_memc_level = 10;
	} else {
		pfw_data->frc_top_type.frc_memc_level_1 = 0;
		pfw_data->frc_top_type.frc_memc_level = dejudder_level;
	}
	if (pfw_data->frc_memc_level)
		pfw_data->frc_memc_level(pfw_data);
}
EXPORT_SYMBOL(dpss_frc_set_dejudder);

void dpss_frc_set_deblur(u8 deblur_level)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct dpss_game_state_s *game_st;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (!pchip_st)
		return;

	game_st = &pchip_st->game_st;
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data) {
		frc_top = &pfw_data->frc_top_type;
	} else {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;	// add abnormal case
	}

	game_st->game_deblur = (64 * deblur_level) / 10;
	pr_frc(1, "ext_ctrl: deblur_level:%d\n", deblur_level);
	if (frc_top->game_mode) { // bug fix, game frc
		pfw_data->frc_top_type.frc_deblur_level = 10;
		if (pfw_data->frc_memc_level && !is_vd1_link_state())
			pfw_data->frc_memc_level(pfw_data);
		pr_frc(1, "game frc: deblur_level(mc phs):0x%x\n",
			game_st->game_deblur);
		return;
	}
	if (deblur_level != pfw_data->frc_top_type.frc_deblur_level) {
		pfw_data->frc_top_type.frc_deblur_level = deblur_level;
		if (pfw_data->frc_memc_level)
			pfw_data->frc_memc_level(pfw_data);
	}
}
EXPORT_SYMBOL(dpss_frc_set_deblur);

static void frc_set_force_n2m(struct frc_state_s *state_st)
{
	if (!state_st)
		return;
	if (state_st->ui_set_n2m || state_st->dbg_set_n2m) {
		if (!state_st->force_n2m) {
			state_st->force_n2m = true;
			pr_frc(1, "force_n2m=%d\n", state_st->force_n2m);
		}
	} else if (state_st->fast_pat_set_n2m) {
		if (!state_st->force_n2m) {
			state_st->force_n2m = true;
			state_st->n2m_status.force_frc_ratio = DPSS_FRC_RATIO_1_1;
			pr_frc(1, "force_n2m=%d, force_frc_ratio=%d\n",
				state_st->force_n2m, state_st->n2m_status.force_frc_ratio);
		}
	} else {
		if (state_st->force_n2m) {
			state_st->n2m_property_change = true;
			state_st->force_n2m = false;
			pr_frc(1, "force_n2m=%d, n2m_property_change=%d\n",
				state_st->force_n2m, state_st->n2m_property_change);
		}
	}
}

static void get_vout_framerate(struct frc_state_s *state_st)
{
	struct vinfo_s *vinfo = get_current_vinfo();
	u16 out_fr_int;
	u16 out_fr_frac;

	if (!vinfo || !state_st)
		return;

	out_fr_int = state_st->n2m_status.out_fr.integer;
	out_fr_frac = state_st->n2m_status.out_fr.fraction;

	out_fr_int = vinfo->sync_duration_num / vinfo->sync_duration_den;
	out_fr_frac = (vinfo->sync_duration_num * 1000 / vinfo->sync_duration_den) % 1000;

	state_st->n2m_status.out_fr.integer = out_fr_int;
	state_st->n2m_status.out_fr.fraction = out_fr_frac;
	state_st->n2m_status.output_framerate = out_fr_int + (out_fr_frac + 500) / 1000;

	pr_frc(1, "vout:rate-%d.%d (%d)\n",
		out_fr_int, out_fr_frac, state_st->n2m_status.output_framerate);
}

static void frc_set_n2m(u32 in_fr, u32 out_fr, struct frc_state_s *state_st)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	enum DPSS_FRC_RATIO frc_ratio;
	int i;

	if (!state_st)
		return;
	if (state_st->force_n2m)
		return;

	for (i = 0; i < N2M_LIST; i++) {
		if (in_fr == n2m_table[i].input_framerate &&
				out_fr == n2m_table[i].output_framerate) {
			frc_ratio = n2m_table[i].frc_ratio;
			break;
		}
	}

	if (i == N2M_LIST) {
		dbg_f2("%s set ratio 1:1 in:%3d out:%3d\n", __func__, in_fr, out_fr);
		frc_ratio = DPSS_FRC_RATIO_1_1;
	}

	prm_top->frc_ratio = frc_ratio;
	state_st->n2m_status.frc_ratio = frc_ratio;
	dbg_f2("frc set n2m:%d\n", frc_ratio);
}

bool  frc_dynamic_detect_play_speed(void)
{
	u32 nr_buf_stats;
	u32 nr_dae_buf_cnt;
	bool condition_flag = false;

	nr_buf_stats = (rd(DPSS_FBUF_BUF_CNT) >> 16) & 0x1F;
	nr_dae_buf_cnt = (rd(DPSS_FBUF_BUF_CTRL) >> 16) & 0x1F;
	if (nr_buf_stats > nr_dae_buf_cnt - 3)
		condition_flag = true;
	else
		condition_flag = false;
	return condition_flag;
}

void dp_dynamic_detect_play_speed(bool en)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;

	if (!pchip_st)
		return;

	state_st = &pchip_st->state_st;
	state_st->fast_pat_set_n2m = en;
	state_st->fast_pat_set_phase0 = en;
	dbg_f1("fast_pat_set=%d\n", en);
}

enum hdr_fmt_s {
	FMT_SDR,
	FMT_HDR10,
	FMT_HDR10PLUS,
	FMT_HDR10PRIME,
	FMT_HLG,
	FMT_AMDV,
};

static enum hdr_fmt_s frc_set_hdr_flag(struct vframe_s *vfm)
{
	enum hdr_fmt_s hdr_flag = FMT_SDR;

	if (!vfm)
		return FMT_SDR;

	if (hdr_transfer_characteristic == 16 &&
			(hdr_color_primaries == 9 || hdr_color_primaries == 2))
		hdr_flag = FMT_HDR10;
	else if (hdr_transfer_characteristic == 0x30 &&
			(hdr_color_primaries == 9 || hdr_color_primaries == 2))
		hdr_flag = FMT_HDR10PLUS;
	else if (hdr_transfer_characteristic == 18 &&
			hdr_color_primaries == 9)
		hdr_flag = FMT_HLG;

	return hdr_flag;
}

void dpss_h_update_frc(struct vframe_s *vfm)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	u16 in_fr_int;
	u16 in_fr_frac;
	u32 in_fr;
	u32 out_fr;
	u32 duration;
	u32 src_w, src_h;
	bool is_dos = false;
	bool is_4k2k = false;
	char frac_str[5];
	int len;
	static u32 detect_fast_cnt;

	if (!pchip_st) {
		dbg_f1("%s pchip is null\n", __func__);
		return;
	}
	if (!vfm)
		return;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return;
	frc_top = &pfw_data->frc_top_type;
	state_st = &pchip_st->state_st;

	frc_set_force_n2m(state_st);

	in_fr = state_st->n2m_status.input_framerate;
	out_fr = state_st->n2m_status.output_framerate;

	get_vout_framerate(state_st);

	duration = vfm->duration;
	in_fr_int = 96000 / duration;
	in_fr_frac = 96000 * 1000 / duration % 1000;
	sprintf(frac_str, "%03d", in_fr_frac);
	len = strlen(frac_str);
	while (len > 0 && frac_str[len - 1] == '0') {
		frac_str[len - 1] = '\0';
		len--;
	}
	state_st->n2m_status.duration = duration;
	state_st->n2m_status.in_fr.integer = in_fr_int;
	state_st->n2m_status.in_fr.fraction = in_fr_frac;
	state_st->n2m_status.input_framerate = in_fr_int + (in_fr_frac + 500) / 1000;
	if (state_st->n2m_status.input_framerate == 59)
		state_st->n2m_status.input_framerate = 60;//duration 1617
	dbg_f1("in:rate-%d.%s (%d)\n",
		in_fr_int, len > 0 ? frac_str : "0",
		state_st->n2m_status.input_framerate);
	if (state_st->force_n2m == 1 &&
		(frc_top->in_out_ratio != state_st->n2m_status.force_frc_ratio ||
		prm_top->frc_ratio != state_st->n2m_status.force_frc_ratio)) {
		prm_top->frc_ratio = state_st->n2m_status.frc_ratio;
		frc_top->in_out_ratio = state_st->n2m_status.frc_ratio;
		state_st->n2m_status.frc_ratio = state_st->n2m_status.force_frc_ratio;
		frc_top->frc_in_frm_rate = state_st->n2m_status.input_framerate;
		frc_top->frc_out_frm_rate = state_st->n2m_status.output_framerate;
		update_n2m_info_to_fw(prm_top->frc_ratio);
		dbg_f1("force-n2m:%d, frm_rate: %d to %d\n",
				prm_top->frc_ratio,
				frc_top->frc_in_frm_rate,
				frc_top->frc_out_frm_rate);
	}

	if (in_fr != state_st->n2m_status.input_framerate ||
			out_fr != state_st->n2m_status.output_framerate)
		state_st->n2m_property_change = true;

	if (state_st->n2m_property_change) {
		state_st->n2m_property_change = false;
		in_fr = state_st->n2m_status.input_framerate;
		out_fr = state_st->n2m_status.output_framerate;
		frc_top->frc_in_frm_rate = in_fr;
		frc_top->frc_out_frm_rate = out_fr;
		frc_set_n2m(in_fr, out_fr, state_st);
		update_n2m_info_to_fw(prm_top->frc_ratio);
	}
	if (state_st->detect_speed && prm_top->dpss_mode != DPSS_FRC_MODE) {
		if (frc_dynamic_detect_play_speed())
			detect_fast_cnt++;
		else if (detect_fast_cnt > 0)
			detect_fast_cnt--;
		if (detect_fast_cnt >= state_st->detect_threshold &&
			prm_top->frc_ratio != DPSS_FRC_RATIO_1_1) {
			prm_top->frc_ratio = DPSS_FRC_RATIO_1_1;
			frc_top->in_out_ratio = DPSS_FRC_RATIO_1_1;
			update_n2m_info_to_fw(prm_top->frc_ratio);
			dbg_f1("detect-speed_n2m:%d, frm_rate: %d to %d, cnt:%d\n",
				prm_top->frc_ratio,
				frc_top->frc_in_frm_rate,
				frc_top->frc_out_frm_rate,
				detect_fast_cnt);
		} else if (detect_fast_cnt == 0 &&
			prm_top->frc_ratio != state_st->n2m_status.frc_ratio) {
			prm_top->frc_ratio = state_st->n2m_status.frc_ratio;
			frc_top->in_out_ratio = state_st->n2m_status.frc_ratio;
			update_n2m_info_to_fw(prm_top->frc_ratio);
			dbg_f1("detect-normal_n2m:%d, frm_rate: %d to %d, cnt:%d\n",
				prm_top->frc_ratio,
				frc_top->frc_in_frm_rate,
				frc_top->frc_out_frm_rate,
				detect_fast_cnt);
		}
	}

	is_dos = !VFM_IS_VDIN_SRC(vfm->source_type);
	frc_top->frc_reserved = 0;
	frc_top->frc_reserved = VFM_IS_HDMI_SRC(vfm->source_type); // BIT0 =1
	frc_top->frc_reserved |= VFM_IS_DTV_SRC(vfm->flag_ext); // BIT2 =4
	src_w = (vfm->type & VIDTYPE_COMPRESS) ? vfm->compWidth : vfm->width;
	src_h = (vfm->type & VIDTYPE_COMPRESS) ? vfm->compHeight : vfm->height;
	state_st->src_frm_height = src_h;
	state_st->src_frm_width = src_w;
	if (vfm->dpss_flg != frc_top->frc_prot_mode)
		frc_top->frc_prot_mode = vfm->dpss_flg;
	is_4k2k = (src_w > 1920 && src_h > 1088);
	state_st->is_dos = is_dos;
	if (is_dos && is_4k2k) {
		if (pchip_st->chip == ID_T6W && in_fr >= 50)
			state_st->mc_bypass_always = 2;
		else if (pchip_st->chip == ID_T6X && in_fr == out_fr)
			state_st->mc_bypass_always = 1;
		else
			state_st->mc_bypass_always = 0;
		dbg_f2("is_dos=%d is_4k2k=%d(%d,%d) mc_bypass_always=%d\n",
			is_dos, is_4k2k, src_w, src_h, state_st->mc_bypass_always);
	}

	if (vfm->type_ext & VIDTYPE_EXT_DPSS_EOS && !state_st->need_drop_dd) {
		state_st->need_drop_dd = true;
		dbg_f2("need_drop_dd=%d\n", state_st->need_drop_dd);
	} else if (!(vfm->type_ext & VIDTYPE_EXT_DPSS_EOS) && state_st->need_drop_dd) {
		state_st->need_drop_dd = false;
		dbg_f2("need_drop_dd=%d\n", state_st->need_drop_dd);
	}

	if (state_st->is_seek_bar && !(vfm->type_ext & VIDTYPE_EXT_DPSS_EOS))
		state_st->is_seek_bar = false;

	frc_top->hdr_flag = (u8)frc_set_hdr_flag(vfm);
	dbg_f2("hdr_flag=%d\n", frc_top->hdr_flag);
}

static void frc_parse_vfcd_format(u32 format_value,
		unsigned int *fmt, unsigned int *bit, unsigned int *plan)
{
	unsigned int tmp;

	tmp = (format_value >> 8) & 0x3;
	if (tmp == 0)
		*bit = BIT_008;
	else if (tmp == 1)
		*bit = BIT_010;
	else
		*bit = BIT_012;

	tmp = (format_value >> 6) & 0x3;
	if (tmp == 0)
		*fmt = YUV444;
	else if (tmp == 1)
		*fmt = YUV422;
	else
		*fmt = YUV420;

	tmp = (format_value >> 4) & 0x3;
	if (tmp == 0)
		*plan = PLANAR_X1;
	else if (tmp == 1)
		*plan = PLANAR_X2;
	else
		*plan = PLANAR_X3;
}

static unsigned int frc_parse_compression(u32 format_value)
{
	unsigned int tmp = (format_value >> 16) & 0x3;

	if (tmp == 1)
		return CMPR_AFBC;
	else if (tmp == 3)
		return CMPR_AFRC;
	else
		return CMPR_UN;
}

static void frc_display_vfcd_info(u32 rmif_id, const char *name)
{
	u32 regs_ofst, format_value, vfcd_ro_stats;
	unsigned int cur_fmt, cur_plan, cur_bit, cur_cmpr;
	u32 frm_size, src_size, pad_en;
	u32 frm_width, frm_height, src_width, src_height;

	regs_ofst = get_vfcd_regs_ofst(rmif_id);

	format_value = rd_vc(RMIF_TOP_CTRL + regs_ofst);
	frc_parse_vfcd_format(format_value, &cur_fmt, &cur_bit, &cur_plan);

	format_value = rd_vc(VFCD_TOP_CTRL0 + regs_ofst);
	cur_cmpr = frc_parse_compression(format_value);

	vfcd_ro_stats = rd_vc(RMIF_RO_STAT + regs_ofst);
	PR_FRC("vfcd_%s fmt= %s, bit= %s, plane= %s, cmpr= %s, rmif_stat= %#x\n",
			name, vid_src_fmt_str[cur_fmt], vid_src_fmt_str[cur_bit],
			vid_src_fmt_str[cur_plan], vid_src_fmt_str[cur_cmpr],
			vfcd_ro_stats);

	// LUMA rmif size
	frm_size = rd_vc(LUMA_RMIF_SCOPE_X + regs_ofst);
	src_size = rd_vc(LUMA_RMIF_SCOPE_Y + regs_ofst);
	frm_width = frm_size & 0x1FFF;
	frm_height = (frm_size >> 16) & 0x1FFF;
	src_width = src_size & 0x1FFF;
	src_height = (src_size >> 16) & 0x1FFF;
	PR_FRC("%s_luma_rmif_start [%d, %d], end [%d, %d]\n",
		name, frm_width, src_width, frm_height, src_height);

	// CHRM rmif size
	frm_size = rd_vc(CHRM_RMIF_SCOPE_X + regs_ofst);
	src_size = rd_vc(CHRM_RMIF_SCOPE_Y + regs_ofst);
	frm_width = frm_size & 0x1FFF;
	frm_height = (frm_size >> 16) & 0x1FFF;
	src_width = src_size & 0x1FFF;
	src_height = (src_size >> 16) & 0x1FFF;
	PR_FRC("%s_chrm_rmif_start [%d, %d], end [%d, %d]\n",
		name, frm_width, src_width, frm_height, src_height);

	// LUMA pic size
	frm_size = rd_vc(VFCD_LUMA_PIC_XPOS + regs_ofst);
	src_size = rd_vc(VFCD_LUMA_PIC_YPOS + regs_ofst);
	frm_width = frm_size & 0x1FFF;
	frm_height = (frm_size >> 16) & 0x1FFF;
	src_width = src_size & 0x1FFF;
	src_height = (src_size >> 16) & 0x1FFF;
	PR_FRC("%s_luma_pic_start [%d, %d], end [%d, %d]\n",
		name, frm_height, src_height, frm_width, src_width);

	// CHRM pic size
	frm_size = rd_vc(VFCD_CHRM_PIC_XPOS + regs_ofst);
	src_size = rd_vc(VFCD_CHRM_PIC_YPOS + regs_ofst);
	frm_width = frm_size & 0x1FFF;
	frm_height = (frm_size >> 16) & 0x1FFF;
	src_width = src_size & 0x1FFF;
	src_height = (src_size >> 16) & 0x1FFF;
	PR_FRC("%s_chrm_pic_start [%d, %d], end [%d, %d]\n",
		name, frm_height, src_height, frm_width, src_width);

	// PAD info
	frm_size = rd_vc(VFCD_LUMA_PAD_SIZE + regs_ofst);
	src_size = rd_vc(VFCD_CHRM_PAD_SIZE + regs_ofst);
	pad_en = (frm_size >> 29) & 0x1;
	frm_width = frm_size & 0x1FFF;
	frm_height = (frm_size >> 16) & 0x1FFF;
	src_width = src_size & 0x1FFF;
	src_height = (src_size >> 16) & 0x1FFF;
	PR_FRC("%s_pad_en %d pad_luma_size [%d, %d], pad_chrm_size [%d, %d]\n",
		name, pad_en, frm_width, frm_height, src_width, src_height);

	//reverse
	frm_size = rd_vc(RMIF_TOP_CTRL + regs_ofst) & 0x3;
	src_size = (rd_vc(VFCD_AFBC_MODE + regs_ofst) >> 26) & 3;
	PR_FRC("%s_reverse [mif:%2d, compr:%2d]\n", name, frm_size, src_size);
}

void dpss_frc_check_reg_stats(void)
{
	u32 frm_size, src_size, det_size;
	u32 frm_height, frm_width, src_height, src_width;
	u32 det_height, det_width, frm_hofst;
	u32 mc_tmp_stats, nr_tmp_stats, di_tmp_stats;
	u32 frc_stats, nr_stats, di_stats;
	const char *dae_type;

	// read basic info
	frm_size = rd(FRC_INP_SW_FRM_ALIGNED_SIZE);
	frm_height = frm_size & 0x1FFF;
	frm_width = (frm_size >> 16) & 0x1FFF;
	src_size = rd(FRC_INP_SW_FRM_ORIGINAL_SIZE);
	src_height = src_size & 0x1FFF;
	src_width = (src_size >> 16) & 0x1FFF;
	det_size = rd(FRC_INP_SW_DETECT_SIZE);
	det_height = det_size & 0x1FFF;
	det_width = (det_size >> 16) & 0x1FFF;

	PR_FRC("inp_size frm= [%d,%d], src= [%d,%d], det= [%d,%d]\n",
		frm_width, frm_height, src_width, src_height,
		det_width, det_height);

	// read proc status
	frc_stats = rd(DPSS_FRC_PROC_STATUS);
	nr_stats = rd(DPSS_FBUF_PROC_STATUS);
	di_stats = rd(DPSS_FBUF_PROC_STATUS + (0x86 - 0x83) * 256);

	PR_FRC("frc_status\t: inp= %s, dae0= %s, dpe= %s\n",
		stats_fmt_str[(frc_stats >> 16) & 0x7],
		stats_fmt_str[(frc_stats >> 8) & 0x7],
		stats_fmt_str[frc_stats & 0x7]);
	PR_FRC("nr__status\t: dae= %s, dpe= %s\n",
		stats_fmt_str[(nr_stats >> 16) & 0x7],
		stats_fmt_str[nr_stats & 0x7]);
	PR_FRC("di__status\t: dae= %s, dpe= %s\n",
		stats_fmt_str[(di_stats >> 16) & 0x7],
		stats_fmt_str[di_stats & 0x7]);

	// gat dae info
	mc_tmp_stats = (frc_stats >> 8) & 0x7;
	nr_tmp_stats = (nr_stats >> 16) & 0x7;
	di_tmp_stats = (di_stats >> 16) & 0x7;

	frm_size = rd(VPU_DAE_WRAP_SW_SIZE0);
	frm_width = frm_size & 0x1FFF;
	frm_height = (frm_size >> 16) & 0x1FFF;
	src_size = rd(VPU_DAE_WRAP_SW_SIZE1);
	src_width = src_size & 0x1FFF;
	src_height = (src_size >> 16) & 0x1FFF;

	if (mc_tmp_stats == 0x3)
		dae_type = "frc_dae";
	else if (nr_tmp_stats == 0x3)
		dae_type = "nr_dae";
	else if (di_tmp_stats == 0x3)
		dae_type = "di_dae";
	else
		dae_type = "com_dae";

	PR_FRC("%s inp_size\t= [%d, %d], me_size [%d, %d]\n",
		dae_type, frm_width, frm_height, src_width, src_height);

	// MC size
	frm_size = rd(DPSS_DPE_MC_TOP_FSIZE);
	src_size = rd(DPSS_DPE_MC_SRC_FSIZE);
	frm_height = frm_size & 0x1FFF;
	frm_width = (frm_size >> 16) & 0x1FFF;
	src_height = src_size & 0x1FFF;
	src_width = (src_size >> 16) & 0x1FFF;
	PR_FRC("mc_size frame\t= [%d, %d], src= [%d, %d]\n",
		frm_width, frm_height, src_width, src_height);

	// cut info
	frm_size = rd(DPSS_DPE_MC_WIN_CUT_H);
	src_size = rd(DPSS_DPE_MC_WIN_CUT_V);
	frm_height = frm_size & 0x1FFF;
	frm_width = (frm_size >> 16) & 0x1FFF;
	src_height = src_size & 0x1FFF;
	src_width = (src_size >> 16) & 0x1FFF;
	PR_FRC("mc cut enable\t= %d\n", (rd(DPSS_DPE_MC_MIF_CTRL0) >> 8) & 0x1);
	PR_FRC("cut posi after mc\t= [%d, %d], [%d, %d]\n",
		frm_height, src_height, frm_width, src_width);
	PR_FRC("cut size after mc\t= [%d, %d]\n",
		frm_width - frm_height + 1, src_width - src_height + 1);

	// VFCD MC0   MC1 info
	frc_display_vfcd_info(DPSS_RMIF_MC0, "mc0");
	frc_display_vfcd_info(DPSS_RMIF_MC1, "mc1");

	// VPU NR post info
	frm_width = rd(VPU_POST_NR_SLC_ACT_HSIZE_0);
	frm_hofst = rd(VPU_POST_NR_SLC_ACT_HSIZE_1);
	PR_FRC("vpu_nr_post_slc_hor_size\t= [%d, %d, %d, %d]\n",
		frm_width & 0xFFFF, (frm_width >> 16) & 0xFFFF,
		frm_hofst & 0xFFFF, (frm_hofst >> 16) & 0xFFFF);

	//frm_vofst = rd(VPU_POST_NR_SLC_OVLP_SIZE);
	//PR_FRC("vpu_nr_post_slc_ovlp_size\t= [%d, %d, %d, %d]\n",
	//	frm_vofst & 0xFF, (frm_vofst >> 8) & 0xFF,
	//	(frm_vofst >> 16) & 0xFF, (frm_vofst >> 24) & 0xFF);
	//frm_height = rd(VPU_NR_OUT_CROP);
	//PR_FRC("vpu_nr_out_crop\t= [%d, %d, %d, %d]\n",
	//	(frm_height >> 16) & 0xFF, (frm_height >> 24) & 0xFF,
	//	frm_height & 0xFF, (frm_height >> 8) & 0xFF);
}

void dpss_frc_set_memc_enable(u8 function, u8 enable)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_fw_alg_ctrl_s *frc_fw_alg_ctrl;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return;
	frc_fw_alg_ctrl = (struct dpss_frc_fw_alg_ctrl_s *)&pfw_data->frc_fw_alg_ctrl;

	PR_FRC("%s func:%x, enable:%d\n",
			__func__, function, enable);
	switch (function) {
	case FRC_ME_FUNC:
		prm_top->frc_me_en = enable;
		frc_fw_alg_ctrl->frc_me_en = enable;
		break;
	case FRC_VP_FUNC:
		prm_top->frc_vp_en = enable;
		frc_fw_alg_ctrl->vp_en = enable;
		break;
	case FRC_BBD_FUNC:
		prm_top->frc_bbd_en = enable;
		frc_fw_alg_ctrl->bbd_en = enable;
		break;
	case FRC_MELG_FUNC:
		prm_top->frc_melogo_en = enable;
		frc_fw_alg_ctrl->melogo_en = enable;
		break;
	case FRC_IPLG_FUNC:
		prm_top->frc_iplogo_en = enable;
		frc_fw_alg_ctrl->iplogo_en = enable;
		break;
	default:
		break;
	}
}

void update_n2m_info_to_fw(enum DPSS_FRC_RATIO frc_ratio)
{
	u32 irn = 1, irm = 1;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return;

	frc_top = &pfw_data->frc_top_type;
	get_n2m_value_from_frc_ratio(frc_ratio, &irn, &irm);
	frc_top->frc_input_n = irn;
	frc_top->frc_output_m = irm;
	frc_top->in_out_ratio = frc_ratio;
}

void get_n2m_value_from_frc_ratio(enum DPSS_FRC_RATIO frc_ratio, u32 *inp_n, u32 *outp_m)
{
	switch (frc_ratio) {
	case DPSS_FRC_RATIO_1_1:
		*inp_n = 1;
		*outp_m = 1;
		break;
	case DPSS_FRC_RATIO_1_2:
		*inp_n = 1;
		*outp_m = 2;
		break;
	case DPSS_FRC_RATIO_2_3:
		*inp_n = 2;
		*outp_m = 3;
		break;
	case DPSS_FRC_RATIO_2_5:
		*inp_n = 2;
		*outp_m = 5;
		break;
	case DPSS_FRC_RATIO_5_6:
		*inp_n = 5;
		*outp_m = 6;
		break;
	case DPSS_FRC_RATIO_5_12:
		*inp_n = 5;
		*outp_m = 12;
		break;
	case DPSS_FRC_RATIO_2_9:
		*inp_n = 2;
		*outp_m = 9;
		break;
	case DPSS_FRC_RATIO_2_15:
		*inp_n = 2;
		*outp_m = 15;
		break;
	case DPSS_FRC_RATIO_1_4:
		*inp_n = 1;
		*outp_m = 4;
		break;
	case DPSS_FRC_RATIO_1_5:
		*inp_n = 1;
		*outp_m = 5;
		break;
	case DPSS_FRC_RATIO_1_6:
		*inp_n = 1;
		*outp_m = 6;
		break;
	case DPSS_FRC_RATIO_1_3:
		*inp_n = 1;
		*outp_m = 3;
		break;
	case DPSS_FRC_RATIO_3_5:
		*inp_n = 3;
		*outp_m = 5;
		break;
	case DPSS_FRC_RATIO_3_10:
		*inp_n = 3;
		*outp_m = 10;
		break;
	case DPSS_FRC_RATIO_4_5:
		*inp_n = 4;
		*outp_m = 5;
		break;
	case DPSS_FRC_RATIO_1_8:
		*inp_n = 1;
		*outp_m = 8;
		break;
	case DPSS_FRC_RATIO_1_10:
		*inp_n = 1;
		*outp_m = 10;
		break;
	case DPSS_FRC_RATIO_1_12:
		*inp_n = 1;
		*outp_m = 12;
		break;
	case DPSS_FRC_RATIO_1_20:
		*inp_n = 1;
		*outp_m = 20;
		break;
	default:
		*inp_n = 1;
		*outp_m = 1;
		break;
	}
}

const char * const mc_csc_str[] = {
	"CSC_OFF",
	"RGB_YUV709L",
	"RGB_YUV709F",
	"YUV709L_RGB",
	"YUV709F_RGB",
	"PAT_RED",
	"PAT_GREEN",
	"PAT_BLUE",
	"PAT_WHITE",
	"PAT_BLACK",
	"RETORE_DEF",
};

void dpss_frc_get_init_csc(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_mc_csc_set_s *mc_csc_s;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	mc_csc_s = &pchip_st->init_csc;
	mc_csc_s->coef00_01 = rd(FRC_MC_CSC_COEF_00_01);
	mc_csc_s->coef02_10 = rd(FRC_MC_CSC_COEF_02_10);
	mc_csc_s->coef11_12 = rd(FRC_MC_CSC_COEF_11_12);
	mc_csc_s->coef20_21 = rd(FRC_MC_CSC_COEF_20_21);
	mc_csc_s->coef22 = rd(FRC_MC_CSC_COEF_22);
	mc_csc_s->pre_offset01 = rd(FRC_MC_CSC_OFFSET_INP01);
	mc_csc_s->pre_offset2 = rd(FRC_MC_CSC_OFFSET_INP2);
	mc_csc_s->pst_offset01 = rd(FRC_MC_CSC_OFFSET_OUP01);
	mc_csc_s->pst_offset2 = rd(FRC_MC_CSC_OFFSET_OUP2);
	mc_csc_s->enable =
			(rd(FRC_MC_CSC_CTRL) >> 3) & BIT_0;

	pr_frc(1, "%s,mc_csc_en:%d mtx(%x-%x-%x)\n",
		__func__,
		mc_csc_s->enable,
		mc_csc_s->coef00_01,
		mc_csc_s->pre_offset01,
		mc_csc_s->pre_offset01);
}

void dpss_frc_mtx_cfg(enum frc_mc_mtx_csc_e mtx_csc)
{
	struct frc_chip_st *pchip_st;
	struct frc_mc_csc_set_s *mc_csc_s;
	unsigned int pre_offset01 = 0;
	unsigned int pre_offset2 = 0;
	unsigned int pst_offset01 = 0x0200, pst_offset2 = 0x0200;
	unsigned int coef00_01 = 0, coef02_10 = 0, coef11_12 = 0;
	unsigned int coef20_21 = 0, coef22 = 0;
	unsigned int en = 1;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	mc_csc_s = &pchip_st->init_csc;
	switch (mtx_csc) {
	case CSC_OFF:
		en = 0;
		break;
	case RGB_YUV709L:
		coef00_01 = 0x00bb0275;
		coef02_10 = 0x003f1f99;
		coef11_12 = 0x1ea601c2;
		coef20_21 = 0x01c21e67;
		coef22 = 0x00001fd7;
		pre_offset01 = 0x0;
		pre_offset2 = 0x0;
		pst_offset01 = 0x00400200;
		pst_offset2 = 0x00000200;
		break;
	case RGB_YUV709F:
		coef00_01 = 0xda02dc;
		coef02_10 = 0x4a1f8b;
		coef11_12 = 0x1e750200;
		coef20_21 = 0x2001e2f;
		coef22 = 0x1fd0;
		pre_offset01 = 0x0;
		pre_offset2 = 0x0;
		pst_offset01 = 0x200;
		pst_offset2 = 0x200;
		break;
	case YUV709L_RGB:
		coef00_01 = 0x04A80000;
		coef02_10 = 0x072C04A8;
		coef11_12 = 0x1F261DDD;
		coef20_21 = 0x04A80876;
		coef22 = 0x0;
		pre_offset01 = 0x1fc01e00;
		pre_offset2 = 0x000001e00;
		pst_offset01 = 0x0;
		pst_offset2 = 0x0;
		break;
	case YUV709F_RGB:
		coef00_01 = 0x04000000;
		coef02_10 = 0x064D0400;
		coef11_12 = 0x1F411E21;
		coef20_21 = 0x0400076D;
		coef22 = 0x0;
		pre_offset01 = 0x000001e00;
		pre_offset2 = 0x000001e00;
		pst_offset01 = 0x0;
		pst_offset2 = 0x0;
		break;
	case PAT_RD:
		coef00_01 = 0x0;
		coef02_10 = 0x0;
		coef11_12 = 0x0;
		coef20_21 = 0x0;
		coef22 = 0x0;
		pst_offset01 = 0x0;
		pst_offset2 = 0xFFF;
		break;
	case PAT_GR:
		coef00_01 = 0x0;
		coef02_10 = 0x0;
		coef11_12 = 0x0;
		coef20_21 = 0x0;
		coef22 = 0x0;
		pst_offset01 = 0x0FFF0000;
		pst_offset2 = 0x0;
		break;
	case PAT_BU:
		coef00_01 = 0x0;
		coef02_10 = 0x0;
		coef11_12 = 0x0;
		coef20_21 = 0x0;
		coef22 = 0x0;
		pst_offset01 = 0x0FFF;
		pst_offset2 = 0x0;
		break;
	case PAT_WT:
		coef00_01 = 0x0;
		coef02_10 = 0x0;
		coef11_12 = 0x0;
		coef20_21 = 0x0;
		coef22 = 0x0;
		pst_offset01 = 0x0FFF0FFF;
		pst_offset2 = 0x0200;
		break;
	case PAT_BK:
		coef00_01 = 0x0;
		coef02_10 = 0x0;
		coef11_12 = 0x0;
		coef20_21 = 0x0;
		coef22 = 0x0;
		pst_offset01 = 0x0200;
		pst_offset2 = 0x0200;
		break;
	case DEFAULT:
		coef00_01 = mc_csc_s->coef00_01;
		coef02_10 = mc_csc_s->coef02_10;
		coef11_12 = mc_csc_s->coef11_12;
		coef20_21 = mc_csc_s->coef20_21;
		coef22 = mc_csc_s->coef22;
		pre_offset01 = mc_csc_s->pre_offset01;
		pre_offset2 = mc_csc_s->pre_offset2;
		pst_offset01 = mc_csc_s->pst_offset01;
		pst_offset2 = mc_csc_s->pst_offset2;
		en = mc_csc_s->enable;
		break;
	default:
		coef00_01 = 0x0;
		coef02_10 = 0x0;
		coef11_12 = 0x0;
		coef20_21 = 0x0;
		coef22 = 0x0;
		pst_offset01 = 0x0200;
		pst_offset2 = 0x0200;
		en = 0;  // invalid case
		break;
	}

	wr(FRC_MC_CSC_OFFSET_INP01, pre_offset01);
	wr(FRC_MC_CSC_OFFSET_INP2, pre_offset2);
	wr(FRC_MC_CSC_COEF_00_01, coef00_01);
	wr(FRC_MC_CSC_COEF_02_10, coef02_10);
	wr(FRC_MC_CSC_COEF_11_12, coef11_12);
	wr(FRC_MC_CSC_COEF_20_21, coef20_21);
	wr(FRC_MC_CSC_COEF_22, coef22);
	wr(FRC_MC_CSC_OFFSET_OUP01, pst_offset01);
	wr(FRC_MC_CSC_OFFSET_OUP2, pst_offset2);
	w_reg_bit(FRC_MC_CSC_CTRL, en, 3, 1);
	pr_frc(1, "%s, en:%d, mtx csc : %s\n",
		__func__, en, mc_csc_str[mtx_csc]);
}

void dpss_frc_set_mc_pattern(u8 enpat)
{
	u8 realpattern =  4;

	if (enpat) {
		realpattern = realpattern + enpat;
		if (realpattern > 9)
			realpattern = 9;
		dpss_frc_mtx_cfg(realpattern);
	} else {
		dpss_frc_mtx_cfg(DEFAULT);
	}
}

void dpss_frc_pattern_dbg_ctrl(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_debug_s *dbg_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;
	dbg_st = &pchip_st->dbg_st;
	if (!dbg_st->pat_dbg.pat_en)
		return;
	dpss_frc_set_mc_pattern(dbg_st->pat_dbg.pat_color);
}

///////////////////////////////////////////////////////////
/*
 * get current frc video latency
 * return: ms
 */
int dpss_frc_get_video_latency_for_gd(void)
{
	struct frc_chip_st *pchip_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct vinfo_s *vinfo;
	struct frc_state_s *state_st;
	u32 vout_hz = 0, is_mc_link = 0;
	int delay_time = -1;/*ms*/
	u8 frm_rate;
	enum DPSS_FRC_RATIO frc_ratio;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st) {
		DBG_ERR("%s pchip_st is null\n", __func__);
		return -1;
	}
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return -1;
	frc_top = &pfw_data->frc_top_type;

	vinfo = get_current_vinfo();
	if (!vinfo)
		return -1;

	if (vinfo && vinfo->sync_duration_den != 0)
		vout_hz = vinfo->sync_duration_num / vinfo->sync_duration_den;
	else
		return -1;

	frc_top->film_mode  = rd(FRC_REG_PHS_TABLE) >> 8 & 0xFF;

	state_st = &pchip_st->state_st;
	frm_rate = state_st->n2m_status.input_framerate;
	frc_ratio = state_st->n2m_status.frc_ratio;
	is_mc_link = !is_vd1_link_state();

	if (is_mc_link) {
		if (frm_rate == 0)
			return -1;
		else if (frc_ratio == DPSS_FRC_RATIO_1_1 &&
				frm_rate == vout_hz)
			delay_time =  35 * 100 / vout_hz;
		else
			delay_time =  35 * 100 / frm_rate;
	} else {
		delay_time = 0;
	}
	dbg_f2("%s delay_time = %d ms\n",
		__func__, delay_time);
	return delay_time;
}
EXPORT_SYMBOL(dpss_frc_get_video_latency_for_gd);

/*
 * get current frc video latency 1
 * return: out vsync num
 */
int dpss_frc_get_video_latency_for_gd1(void)
{
	struct frc_chip_st *pchip_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct vinfo_s *vinfo;
	struct frc_state_s *state_st;
	u32 vout_hz = 0, is_mc_link = 0;
	int delay_time = -1;/*ms*/
	u8 frm_rate;
	enum DPSS_FRC_RATIO frc_ratio;
	int delay_vsync_num = 0;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st) {
		DBG_ERR("%s pchip_st is null\n", __func__);
		return -1;
	}
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return -1;
	frc_top = &pfw_data->frc_top_type;

	vinfo = get_current_vinfo();
	if (!vinfo)
		return -1;
	if (vinfo && vinfo->sync_duration_den != 0)
		vout_hz = vinfo->sync_duration_num / vinfo->sync_duration_den;
	else
		return -1;

	frc_top->film_mode  = rd(FRC_REG_PHS_TABLE) >> 8 & 0xFF;

	state_st = &pchip_st->state_st;
	frm_rate = state_st->n2m_status.input_framerate;
	frc_ratio = state_st->n2m_status.frc_ratio;
	is_mc_link = !is_vd1_link_state();

	if (is_mc_link) {
		if (frm_rate == 0)
			return -1;
		else if (frc_ratio == DPSS_FRC_RATIO_1_1 &&
				frm_rate == vout_hz)
			delay_time =  35 * 100 / vout_hz;
		else
			delay_time =  35 * 100 / frm_rate;
	} else {
		delay_time = 0;
	}

	if (delay_time > 0)
		delay_vsync_num = delay_time * vout_hz / 1000;
	else
		delay_vsync_num = -2;
	dbg_f2("%s delay_vsync_num = %d\n",
			__func__, delay_vsync_num);

	return delay_vsync_num;
}
EXPORT_SYMBOL(dpss_frc_get_video_latency_for_gd1);

#define FRC_DELTA_CNT 64

/* for upper-layer configuration and debugging */
static struct frc_latency_st delta_ms[FRC_DELTA_CNT] = {
	/* in_fps  out_fps  delta_ms */
	{  0,      0,       0},
};

static int get_delta_ms(int in_fps, int out_fps)
{
	int i, delta = 0;

	for (i = 0; i < FRC_DELTA_CNT; i++) {
		if (!delta_ms[i].in_fps)
			break;
		if (in_fps == delta_ms[i].in_fps && out_fps == delta_ms[i].out_fps)
			delta = delta_ms[i].delta_ms;
	}

	return delta;
}

void set_delta_ms(int in_fps, int out_fps, int delta)
{
	int i, old_delta = 0, found = 0;

	for (i = 0; i < FRC_DELTA_CNT; i++) {
		if (!delta_ms[i].in_fps)
			break;
		if (in_fps == delta_ms[i].in_fps && out_fps == delta_ms[i].out_fps) {
			old_delta = delta_ms[i].delta_ms;
			delta_ms[i].delta_ms = delta;
			found = 1;
			DBG_INF("in_fps:%d out_fps:%d delta_ms:%d->%d\n",
				in_fps, out_fps, old_delta, delta);
		}
	}
	if (!found) {
		if (i < FRC_DELTA_CNT) {
			delta_ms[i].in_fps = in_fps;
			delta_ms[i].out_fps = out_fps;
			delta_ms[i].delta_ms = delta;
			DBG_INF("in_fps:%d out_fps:%d delta_ms:%d\n",
				in_fps, out_fps, delta);
		} else {
			DBG_ERR("no space to store\n");
		}
	}
}

void dump_delta_ms(void)
{
	int i;

	for (i = 0; i < FRC_DELTA_CNT; i++) {
		if (!delta_ms[i].in_fps)
			break;
		DBG_INF("in_fps:%-4d out_fps:%-4d delta_ms:%d\n",
			delta_ms[i].in_fps, delta_ms[i].out_fps,
			delta_ms[i].delta_ms);
	}
}

int dpss_frc_get_video_latency(void)
{
	struct frc_chip_st *pchip_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct vinfo_s *vinfo;
	struct frc_state_s *state_st;
	u32 vout_hz = 0, is_mc_link = 0, memc_bypass;
	int delay_time = -1, delta_ms = 0;/*ms*/
	u8 frm_rate;
	enum DPSS_FRC_RATIO frc_ratio;

	pchip_st = dpss_get_frc_st();
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pchip_st || !pchip_st->frc_en || !pfw_data) {
		dbg_f2("dpss_get_video_latency pchip_st or frc no ready\n");
		return -1;
	}

	frc_top = &pfw_data->frc_top_type;

	vinfo = get_current_vinfo();
	if (!vinfo)
		return -1;

	if (vinfo && vinfo->sync_duration_den != 0)
		vout_hz = vinfo->sync_duration_num / vinfo->sync_duration_den;
	else
		return -1;

	frc_top->film_mode  = rd(FRC_REG_PHS_TABLE) >> 8 & 0xFF;

	state_st = &pchip_st->state_st;
	frm_rate = state_st->n2m_status.input_framerate;
	frc_ratio = state_st->n2m_status.frc_ratio;

	if (vpu_clk_get() > 400)
		is_mc_link = !is_vd1_link_state();
	else
		is_mc_link = 0;

	if (frc_top->memc_enable != 1) {
		delay_time = 0;
		// dbg_f2("delay_time=0, memc_enable:%d", frc_top->memc_enable);
	} else if (is_mc_link) {
		memc_bypass = rd(FRC_MC_HW_CTRL0) & 0x1;
		// if (memc_bypass == 1) {
		//	delay_time = 0;
		//} else if (frm_rate == 0) {
		// if (frm_rate == 0) {
		//	delay_time = 50;
		if (frc_ratio == DPSS_FRC_RATIO_1_1 &&
				frm_rate == vout_hz) {
			delta_ms = get_delta_ms(frm_rate, vout_hz);
			//delay_time = (25 * 100 * 100 / vout_hz) / 100;
			delay_time = (35 * 100 * 100 / vout_hz) / 100;
			delay_time += delta_ms;
		} else {
			delta_ms = get_delta_ms(frm_rate, vout_hz);
			//delay_time = (20 * 100 * 100 / frm_rate) / 100 +
			//		(5 * 100 * 100 / vout_hz) / 100;
			delay_time = (20 * 100 * 100 / frm_rate) / 100 +
					(15 * 100 * 100 / vout_hz) / 100;
			delay_time += delta_ms;
		}
		// dbg_f2("delay_time:%d, memc_bypass:%d", delay_time, memc_bypass);
		if (frc_delay_dbg)
			delay_time = frc_delay_dbg; // ms
	} else {
		delay_time = 50;
		// dbg_f2("other delay_time = %d", delay_time);
	}
	dbg_f2("%s delay_time = %d ms delta_ms:%d vout_hz:%d frm_rate:%d frc_ratio:%d\n",
		__func__, delay_time, delta_ms, vout_hz, frm_rate, frc_ratio);

	return delay_time;
}
EXPORT_SYMBOL(dpss_frc_get_video_latency);

int dpss_frc_get_video_latency_new(u32 input_fps, u32 output_fps)
{
	struct frc_chip_st *pchip_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_state_s *state_st;
	u32 is_mc_link = 0, memc_bypass;
	int delay_time = -1, delta_ms = 0;/*ms*/
	u8 frm_rate;
	enum DPSS_FRC_RATIO frc_ratio;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st) {
		dbg_f2("get_frc_latency_new：pchip_st is null\n");
		return -1;
	}
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return -1;
	frc_top = &pfw_data->frc_top_type;

	frc_top->film_mode  = rd(FRC_REG_PHS_TABLE) >> 8 & 0xFF;

	state_st = &pchip_st->state_st;
	frm_rate = state_st->n2m_status.input_framerate;
	frc_ratio = state_st->n2m_status.frc_ratio;
	is_mc_link = !is_vd1_link_state();

	if (frc_top->memc_enable != 1) {
		delay_time = 0;
		// dbg_f2("delay_time=0, memc_enable:%d", frc_top->memc_enable);
	} else if (is_mc_link) {
		memc_bypass = rd(FRC_MC_HW_CTRL0) & 0x1;
		// if (memc_bypass == 1) {
		//	delay_time = 0;
		//} else if (frm_rate == 0) {
		// if (frm_rate == 0) {
		//	delay_time = 50;
		if (frc_ratio == DPSS_FRC_RATIO_1_1 &&
				frm_rate == output_fps) {
			delta_ms = get_delta_ms(frm_rate, output_fps);
			//delay_time = (25 * 100 * 100 / output_fps) / 100;
			delay_time = (35 * 100 * 100 / output_fps) / 100; // rdma_8buf
			delay_time += delta_ms;
		} else {
			delta_ms = get_delta_ms(frm_rate, output_fps);
			//delay_time = (20 * 100 * 100 / frm_rate) / 100 +
			//		(5 * 100 * 100 / output_fps) / 100;
			delay_time = (20 * 100 * 100 / frm_rate) / 100 +
					(15 * 100 * 100 / output_fps) / 100; // rdma_8buf
			delay_time += delta_ms;
		}
		// dbg_f2("delay_time:%d, memc_bypass:%d", delay_time, memc_bypass);
		if (frc_delay_dbg)
			delay_time = frc_delay_dbg; // ms
	} else {
		delay_time = 50;
		// dbg_f2("other delay_time = %d", delay_time);
	}
	dbg_f2("delay_time = %dms delta_ms:%d input_hz:%d vout_hz:%d frm_rate:%d frc_ratio:%d\n",
		 delay_time, delta_ms, input_fps, output_fps, frm_rate, frc_ratio);

	return delay_time;
}
EXPORT_SYMBOL(dpss_frc_get_video_latency_new);

int dpss_frc_tell_alg_vendor(u8 vendor_info)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct dpss_frc_fw_alg_ctrl_s *pfrc_fw_alg_ctrl;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data) {
		frc_top = &pfw_data->frc_top_type;
	} else {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return 0; // add abnormal case
	}
	pfrc_fw_alg_ctrl = (struct dpss_frc_fw_alg_ctrl_s *)&pfw_data->frc_fw_alg_ctrl;
	pr_frc(1, "tell_alg_vendor:0x%x\n", vendor_info);
	if (pfrc_fw_alg_ctrl->frc_algctrl_u8vendor != vendor_info)
		pfrc_fw_alg_ctrl->frc_algctrl_u8vendor = vendor_info;
	return 1;
}

void dpss_frc_get_memc_gmv(struct memc_gmv_s *memc_gmv)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_fw_alg_ctrl_s *pfrc_fw_alg_ctrl;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data || !memc_gmv) {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;
	}

	pfrc_fw_alg_ctrl = (struct dpss_frc_fw_alg_ctrl_s *)&pfw_data->frc_fw_alg_ctrl;

	memc_gmv->gmv_x = pfrc_fw_alg_ctrl->frc_algctrl_s16param1;
	memc_gmv->gmv_y = pfrc_fw_alg_ctrl->frc_algctrl_s16param2;

	pr_frc(1, "gmv_x=%d, gmv_y=%d\n", memc_gmv->gmv_x, memc_gmv->gmv_y);
}

void dpss_frc_resume(void)
{
	struct dpss_frc_fw_data_s *pfw_data;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;
	}
	if (pfw_data->frc_fw_reinit)
		pfw_data->frc_fw_reinit(pfw_data);
}

void update_mc_cut_size_to_fw(u16 mc_hsize, u16 mc_vsize,
				u16 crop_top, u16 crop_left, u16 crop_bottom, u16 crop_right)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;
	}

	frc_top = &pfw_data->frc_top_type;
	frc_top->mc_hsize = mc_hsize;
	frc_top->mc_vsize = mc_vsize;
	frc_top->crop_top = crop_top;
	frc_top->crop_left = crop_left;
	frc_top->crop_bottom = crop_bottom;
	frc_top->crop_right = crop_right;

	if (pfw_data->frc_input_cfg)
		pfw_data->frc_input_cfg(pfw_data);
}

int dpss_frc_fpp_memc_set_level(u8 level, u8 num)
{
	struct dpss_frc_fw_data_s *pfw_data;
	int flag = 0;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data) {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return 0;
	}
	pr_frc(1, "fpp_set_memc_level:%d[%d]\n", level, num);
	if (level != pfw_data->frc_top_type.frc_memc_level) {
		pfw_data->frc_top_type.frc_memc_level = level;
		flag = 1;
	}
	if (num != pfw_data->frc_top_type.frc_memc_level_1) {
		pfw_data->frc_top_type.frc_memc_level_1 = num;
		flag = 1;
	}
	if (pfw_data->frc_memc_level && flag)
		pfw_data->frc_memc_level(pfw_data);
	return 1;
}

static struct dpss_frc_vf_s frc_vf_s[DPSS_DPS_NUB];
static struct vframe_s get_frc_vf_s[DPSS_DPS_NUB];
static unsigned int inp_vfm_cnt;
static struct frc_vf_ctrl_s frc_vf_ctrl;
static unsigned int reset_path_skip_cnt;

static inline void frc_vf_ctrl_reset(void)
{
	memset(&frc_vf_ctrl, 0, sizeof(frc_vf_ctrl));
}

static inline void frc_vf_ctrl_record_wr(u32 nr_buf_id)
{
	u32 idx = nr_buf_id % DPSS_DPS_NUB;

	if (!frc_vf_ctrl.wr_initialized) {
		frc_vf_ctrl.wr_initialized = true;
	} else if (nr_buf_id <= frc_vf_ctrl.wr_seq) {
		dbg_h2("wr_seq back off new:%u last:%u\n",
			nr_buf_id, frc_vf_ctrl.wr_seq);
	}

	frc_vf_ctrl.wr_idx = idx;
	frc_vf_ctrl.wr_seq = nr_buf_id;
}

static inline u32 frc_vf_ctrl_update_rd(u32 hw_idx, u32 *sequence, bool *reuse_last)
{
	u32 idx = hw_idx % DPSS_DPS_NUB;
	u32 expected, ret_idx;

	if (reuse_last)
		*reuse_last = false;

	if (!frc_vf_ctrl.rd_initialized) {
		frc_vf_ctrl.rd_initialized = true;
		frc_vf_ctrl.rd_idx = idx;
		frc_vf_ctrl.rd_seq = frc_vf_s[idx].sequence;
		frc_vf_ctrl.last_idx = idx;
		frc_vf_ctrl.last_seq = frc_vf_ctrl.rd_seq;
		frc_vf_ctrl.last_valid = true;
		ret_idx = idx;
	} else {
		expected = (frc_vf_ctrl.rd_idx + 1) % DPSS_DPS_NUB;
		if (idx != expected) {
			dbg_h2("idx adjust hw:%u expect:%u\n",
				idx, expected);
			if (frc_vf_ctrl.last_valid) {
				if (reuse_last)
					*reuse_last = true;
				if (sequence)
					*sequence = frc_vf_ctrl.last_seq;
				return frc_vf_ctrl.last_idx;
			}
		}

		frc_vf_ctrl.rd_idx = idx;
		frc_vf_ctrl.rd_seq = frc_vf_s[idx].sequence;
		frc_vf_ctrl.last_idx = idx;
		frc_vf_ctrl.last_seq = frc_vf_ctrl.rd_seq;
		frc_vf_ctrl.last_valid = true;
		ret_idx = idx;
	}

	if (sequence)
		*sequence = frc_vf_ctrl.rd_seq;

	return ret_idx;
}

struct vframe_s *frc_get_vfm(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct vframe_s *vfm = NULL;
	u32 cur_idx, ro_disp_info_0, seq_id;
	bool reuse_last = false;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;

	if (!inp_vfm_cnt)
		return NULL;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return NULL;
	if (pchip_st->chip < ID_T6W || !(dpss_light_chg & C_BIT0) ||
			is_vd1_link_state() || prm_top->is_i)
		return NULL;

	if (reset_path_skip_cnt) {
		reset_path_skip_cnt--;
		return NULL;
	}

	if (prm_top->reset_path) { //for skip count
		prm_top->reset_path = false;
		reset_path_skip_cnt = DPSS_FRC_RESET_PATH_SKIP_CNT;
		return NULL;
	}

	state_st = &pchip_st->state_st;
	if (state_st->force_not_send_frc_vf)
		return NULL;
	ro_disp_info_0 = rd(FRC_DPSS_DISP_BUFF_INFO_0);
	cur_idx = state_st->mc_disp_st.wr_cur_idx;

	if (state_st->used_seq_vfm)
		cur_idx = frc_vf_ctrl_update_rd(cur_idx,
			&seq_id, &reuse_last);
	else
		cur_idx = state_st->mc_disp_st.wr_cur_idx % DPSS_DPS_NUB;

	get_frc_vf_s[cur_idx] = frc_vf_s[cur_idx].vfm;
	vfm = &get_frc_vf_s[cur_idx];

	dbg_h2("cur_idx:%d nr_buf_id:%d, vfm(w,h)=(%d,%d) vf:%p\n",
		cur_idx, vfm->nr_buf_id,
		vfm->compWidth, vfm->compHeight, vfm);

	return vfm;
}

struct vframe_s *get_vfm_from_frc(void)
{
	struct vframe_s *vfm = NULL;

	vfm = frc_get_vfm();

	return vfm;
}

void *put_vfm_to_frc(struct vframe_s *vfm)
{
	u32 i, vfm_id, idx;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;

	if (!vfm)
		return NULL;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st || !prm_top)
		return NULL;
	if (pchip_st->chip < ID_T6W)
		return NULL;

	if (prm_top->is_i)
		return NULL;

	dbg_h2("vf->nr_buf_id:%d, vfm-(w,h)=(%d,%d) vf:%p\n",
		vfm->nr_buf_id,
		vfm->compWidth, vfm->compHeight, vfm);
	state_st = &pchip_st->state_st;
	if (state_st->first_put_vfm) {
		dbg_h2("first vfm, nr_id:%d\n",
			vfm->nr_buf_id);
		// frc_init_vfm_s();
		inp_vfm_cnt = 0;
		state_st->first_put_vfm = false;
		frc_vf_ctrl_reset();
		for (i = 0; i < DPSS_DPS_NUB; i++) {
			frc_vf_s[i].nr_buf_id = i;
			frc_vf_s[i].sequence = 0;
			frc_vf_s[i].vfm = *vfm;
			frc_vf_s[i].vfm.nr_buf_id = i;
			get_frc_vf_s[i] = *vfm;
		}
	}

	vfm_id = vfm->nr_buf_id;
	frc_vf_ctrl_record_wr(vfm_id);
	idx = vfm_id % DPSS_DPS_NUB;
	frc_vf_s[idx].nr_buf_id = vfm_id;
	frc_vf_s[idx].sequence = vfm_id;
	frc_vf_s[idx].vfm = *vfm;
	frc_vf_s[idx].vfm.nr_buf_id = vfm_id;
	inp_vfm_cnt++;

	return NULL;
}

void update_frc_state(void)
{
	bool not_use_vf_flag = false;
	u32 cur_idx;
	struct vframe_s *vfs;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;
	struct AA_PPS_TOP_TYPE *pps = &g_nr_pps_cfg;
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;

	if (!pchip_st || !prm_top)
		return;

	state_st = &pchip_st->state_st;
	state_st->pps_frc = pps->pps_en ? true : false;
	pps->trig_pps = 0;

	cur_idx = state_st->mc_disp_st.wr_cur_idx % DPSS_DPS_NUB;
	vfs = &get_frc_vf_s[cur_idx];

	if (VFM_IS_COMP_MODE(vfs->type))
		if (VFM_IS_COMP_AFRC(vfs->type_ext))
			state_st->compr_sel = DDR_AFRC;
		else
			state_st->compr_sel = DDR_AFBC;
	else
		state_st->compr_sel = DDR_MIF;

	if (prm_top->nro_fs_fmt.src_cmpr == CMPR_UN && state_st->compr_sel != DDR_MIF) {
		state_st->compr_sel = DDR_MIF;
		not_use_vf_flag = true;
	} else if (prm_top->nro_fs_fmt.src_cmpr == CMPR_AFRC && state_st->compr_sel != DDR_AFRC) {
		state_st->compr_sel = DDR_AFRC;
		not_use_vf_flag = true;
	}

	if (VFM_IS_FMT_420(vfs->type))
		state_st->big_fmt = YUV420;
	else if (VFM_IS_FMT_422(vfs->type))
		state_st->big_fmt = YUV422;
	else if (VFM_IS_FMT_444(vfs->type))
		state_st->big_fmt = YUV444;

	if (vfs->bitdepth & BITDEPTH_Y10)
		state_st->big_bits = BIT_010;
	else
		state_st->big_bits = BIT_008;

	state_st->big_plane = vfs->plane_num;

	dbg_h2("update frc state done, compr_sel:%d-%d(flag),frc_pps:%d\n",
		state_st->compr_sel, not_use_vf_flag, state_st->pps_frc);
}

//call function only eos
void get_eos_from_dp(bool en)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct frc_state_s *state_st;

	if (!pchip_st)
		return;

	state_st = &pchip_st->state_st;

	if (en && !state_st->is_seek_bar)
		state_st->is_seek_bar = true;
	else
		state_st->is_seek_bar = false;
	dbg_f2("is_seek_bar=%d\n", state_st->is_seek_bar);
	dbg_h2("update frc state done, compr_sel:%d\n",
		 state_st->compr_sel);
}

void mc_res_switch_begin(void)
{
	struct PRM_DPSS_TOP *prm_top = prm_dpss_top;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;

	pchip_st = dpss_get_frc_st();

	if (!pchip_st)
		return;

	state_st = &pchip_st->state_st;
	if (prm_top->trig_byp_mc) {
		if (state_st->mc_disp_st.wr_cur_idx ==
			(prm_top->idx_done + 1) % DPSS_DPS_NUB) {
			prm_top->trig_byp_mc = false;
			state_st->src_chg = 1;
			state_st->force_mc_pre_idx = state_st->mc_disp_st.wr_cur_idx;
			state_st->need_force_mc_pre = 1;
			update_frc_state();
			dbg_h2("cur_idx:%d, idx_done:%d, new size idx:%d\n",
				state_st->mc_disp_st.wr_cur_idx, prm_top->idx_done,
				(prm_top->idx_done + 1) % DPSS_DPS_NUB);
		}
		state_st->mc_byp_switch_on = true;
		dbg_h2("trig byp mc\n");
	}
}

bool mc_phase0_ready(void)
{
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;

	pchip_st = dpss_get_frc_st();

	if (!pchip_st)
		return false;
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;

	if (game_st->game_n2m || game_st->game_film)
		return 1;
	else
		return state_st->mc_phase0_rdma;
}

int dpss_get_game_lock_st(struct dpss_status *state)
{
	u8 vrr_status;
	u32 vrr_val;
	static u32 vrr_lock_cnt;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct dpss_game_state_s *game_st;

	if (!pchip_st)
		return 0;

	game_st = &pchip_st->game_st;
	vrr_val = rd_vc(pchip_st->venc_ro_vrr);

	if (!game_st->game_vrr_en) {
		state->game_mode = 0;
		state->game_frc_locked = 1;
		return 0;
	}

	dbg_h0("vrr_val_ro:%x, vrr_lock_cnt:%d\n", vrr_val, vrr_lock_cnt);
	vrr_status = (vrr_val >> 31) & 0x1;
	state->game_mode = game_st->game_n2m ? 0x1 :
					   game_st->game_film ? 0x2 : 0;

	if (vrr_status)
		vrr_lock_cnt++;
	else
		vrr_lock_cnt = 0;

	state->game_frc_locked =
		vrr_lock_cnt > game_st->vrr_drop_cnt ? vrr_status : 0;

	return 0;
}

unsigned int dpss_dbg_off_vrr_in_vdin;
module_param_named(dpss_dbg_off_vrr_in_vdin, dpss_dbg_off_vrr_in_vdin, uint, 0664);

void vdin_game_frc_notify(struct game_frc_s *frc_data)
{
	struct dpss_frc_top_type_s *frc_top;
	struct dpss_frc_fw_data_s *pfw_data;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	// struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pchip_st || !pfw_data)
		return;

	// state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_top = &pfw_data->frc_top_type;

	if (!frc_data->vdin_stop_dec && dpss_dbg_off_vrr_in_vdin) {
		dbg_h0("vdin stop dec\n");
		if (frc_top->game_mode) {
			game_st->off_game_mode = true;
			venc_vrr_vdin();
		}
	}
}

void rx_game_frc_notify(void)
{
	struct dpss_frc_top_type_s *frc_top;
	struct dpss_frc_fw_data_s *pfw_data;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	// struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pchip_st || !pfw_data)
		return;

	// state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_top = &pfw_data->frc_top_type;

	if (frc_top->game_mode) {
		// game_st->off_game_mode = true;
		// venc_vrr_vdin();
		dbg_h0("rx sig chg\n");
		game_st->mc_need_byp_en = true; // byp mc and dis mc link
	}
}

bool get_dpss_game_frc_locked(void)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct dpss_game_state_s *game_st;

	if (!pchip_st)
		return false;

	game_st = &pchip_st->game_st;
	return game_st->do_game_frc;
}

void vsync_signal_to_game_frc(void)
{
	// static int delay_cnt;
	struct frc_chip_st *pchip_st = dpss_get_frc_st();
	struct dpss_game_state_s *game_st;

	if (!pchip_st)
		return;

	game_st = &pchip_st->game_st;

	if (game_st->delay_enable_game_frc) {
		if (aml_vrr_state())
			return;
		if (game_st->delay_enable_cnt > 0)
			game_st->delay_enable_cnt--;

		if (game_st->delay_enable_cnt == 0) {
			venc_vrr_vdin();
			game_st->delay_enable_game_frc = false;
			dbg_h0("delay enable game frc ok\n");
		}
	}
}

void dpss_frc_sync_status(u8 working)
{
	struct frc_chip_st *pchip_st = dpss_get_frc_st();

	if (!pchip_st) {
		dbg_h0("sync status: pchip_st is null\n");
		return;
	}
	pchip_st->frc_en = working;
}
