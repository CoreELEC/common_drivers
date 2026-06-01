// SPDX-License-Identifier: (GPL-2.0+ OR MIT)
/*
 * Copyright (c) 2019 Amlogic, Inc. All rights reserved.
 */

#include "sys_def.h"
#ifdef RUN_ON_ARM
#include <linux/amlogic/media/di/dpss_interface.h>
#include <linux/kfifo.h>
#include <linux/types.h>
#include <linux/sched/clock.h>
#endif
#include <linux/amlogic/media/dpss/dpss_frc.h>

#include "dpss_frc_dbg.h"
#include "dpss_s.h"
#include "dpss_sys.h"
#include "dpss_s_frc.h"
#include "dpss_frc_rev.h"
#include "_frc_fw_table.h"
#include "hw/dpss.h"
#include "dpss_rdma_frc.h"
#include <linux/amlogic/media/dpss/frc_common_x.h>

static void dpss_frc_debug_parse_param(char *buf_orig, char **parm)
{
	char *ps, *token;
	char delim1[3] = " ";
	char delim2[2] = "\n";
	unsigned int n = 0;

	ps = buf_orig;
	strcat(delim1, delim2);
	while (1) {
		token = strsep(&ps, delim1);
		if (!token)
			break;
		if (*token == '\0')
			continue;
		parm[n++] = token;
	}
}

void dpss_frc_status(void)
{
	struct frc_chip_st *pchip_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	struct frc_interrupt_s *frc_int_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (pfw_data) {
		frc_top = &pfw_data->frc_top_type;
	} else {
		DBG_ERR("%s: frc_fw is null\n", __func__);
		return;
	}
	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_int_st = &state_st->frc_int_st;

	pr_frc(0, "drv_versio\t= %s\n", DPSS_FRC_FW_VER);
	pr_frc(0, "ko_version\t= %s\n", pfw_data->frc_alg_ver);
	pr_frc(0, "chip\t= %d,  vendor\t= %d\n", frc_top->chip,
			pfw_data->frc_fw_alg_ctrl.frc_algctrl_u8vendor);
	pr_frc(0, "memc_level\t= [dejudder:%d(%d), deblur:%d]\n",
			frc_top->frc_memc_level, frc_top->frc_memc_level_1,
			frc_top->frc_deblur_level);
	pr_frc(0, "ratio_n2m\t= [%d:%d]\tfilm\t= %d\n", frc_top->frc_input_n,
			frc_top->frc_output_m, (rd(FRC_REG_PHS_TABLE) >> 8) & 0xFF);
	pr_frc(0, "mc_link_enable\t= [dpss_frc:%2d, mc_link:%2d]\n",
			state_st->frc_en, !is_vd1_link_state());
	pr_frc(0, "mc_link_ctrl\t= [%2d,%2d,%2d,%2d,%2d,%2d]\n",
			state_st->dpss_reg, state_st->dpe_ready, enable_mc_link,
			!state_st->need_disable_mc_link, !state_st->disable_mc_link_intf,
			!state_st->unformat_bypass);
	pr_frc(0, "mc_bypass\t= %d, pre_vs_offset\t= %d\n",
			rd(FRC_MC_HW_CTRL0) & 0x3, rd_vc(pchip_st->encl_frc_ctrl) & 0xffff);
	pr_frc(0, "dpss_flag\t= %#x\n", frc_top->frc_prot_mode);
	pr_frc(0, "eos enable = %d\trev_mode = %d\ttrick_play = %d\n",
		state_st->enable_last_drop, frc_top->frc_rev_mode, state_st->trick_play_en);
	pr_frc(0, "game_n2m\t= %d, game_film\t= %d\n",
		game_st->game_n2m, game_st->game_film);
//	pr_frc(0, "secure_mode= %d, buf secured= %d\n",
//			state_st->frc_sts.secure_mode, pst_chip->buf_st->secured);
	pr_frc(0, "frame_rate= [%3d Hz  to  %3d Hz] (in_duration= %d)\n",
			state_st->n2m_status.input_framerate,
			state_st->n2m_status.output_framerate,
			state_st->n2m_status.duration);
	pr_frc(0, "irq_vs\t cnt= %8d[%d] duration= %dus stamp= %ld\n",
			frc_int_st->frc_vsync_cnt,
			rd(DPSS_DISP_GO_FIELD_CNT),
			frc_int_st->frc_vsync_duration,
			(ulong)frc_int_st->frc_vsync_timestamp);
	pr_frc(0, "irq_pre_vs\t cnt= %8d[%d] duration= %dus stamp= %ld\n",
			frc_int_st->pre_vsync_cnt,
			rd(DPSS_DPE_PRE_VSYNC_CNT),
			frc_int_st->pre_vsync_duration,
			(ulong)frc_int_st->pre_vsync_timestamp);
	pr_frc(0, "irq_inp\t cnt= %8d[%d] duration= %dus stamp= %ld\n",
			frc_int_st->inp_int_cnt,
			rd(DPSS_INP_FRM_INT_CNT),
			frc_int_st->inp_duration,
			(ulong)frc_int_st->inp_timestamp);
	//pr_frc(0, "irq_inp\t min_duration= %dus max_duration= %dus\n",
	//		frc_int_st->inp_min_duration,
	//		frc_int_st->inp_max_duration);
	pr_frc(0, "irq_dae0\t cnt= %8d[%d] duration= %dus stamp= %ld\n",
			frc_int_st->dae0_int_cnt,
			rd(DPSS_DAE0_FRM_INT_CNT),
			frc_int_st->dae0_duration,
			(ulong)frc_int_st->dae0_timestamp);
	pr_frc(0, "irq_chk_err_cnt\t= %d\n", frc_int_st->irq_chk_err_cnt);
	pr_frc(0, "src_frm_size\t= [%d, %d]\n", state_st->src_frm_width,
			state_st->src_frm_height);
//	pr_frc(0, "frc_out isr lost tsk_cnt:%d, err_cnt= (me:%d,mc:%d,vp:%d)\n",
//	       state_st->out_sts.lost_tsk_cnt, state_st->frc_sts.me_undone_cnt,
//	       state_st->frc_sts.mc_undone_cnt,
//	       state_st->frc_sts.vp_undone_cnt);
//	pr_frc(0, "film_mode= %d\n", frc_check_film_mode(devp));
//	pr_frc(0, "n2m_mode= %d\n", state_st->frc_sts.in_out_ratio);
//	pr_frc(0, "rdma_en= %d\n", fw_data->frc_top_type.rdma_en);
//	pr_frc(0, "frc_in hsize= %d vsize= %d\n",
//	       state_st->in_sts.in_hsize, state_st->in_sts.in_vsize);
//	pr_frc(0, "frc_out hsize= %d vsize= %d\n",
//	       state_st->out_sts.vout_width, state_st->out_sts.vout_height);
//	pr_frc(0, "vfb(0x1cb4/0x14ca)= %d\n", fw_data->frc_top_type.vfb);
	dpss_frc_check_reg_stats();
	dpss_frc_check_undone_stats();
}

static ssize_t dpss_frc_debug_if_help(char *buf)
{
	ssize_t len = 0;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct dpss_frc_fw_alg_ctrl_s *frc_fw_alg_ctrl;
	struct frc_debug_s *dbg_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return 0;
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pfw_data)
		return 0;
	frc_top = &pfw_data->frc_top_type;
	frc_fw_alg_ctrl = &pfw_data->frc_fw_alg_ctrl;
	state_st = &pchip_st->state_st;
	dbg_st = &pchip_st->dbg_st;

	len +=
		sprintf(buf + len, "ratio\t=%d\n", state_st->n2m_status.frc_ratio);
	len +=
		sprintf(buf + len, "fw_ctrl->frc_me_en\t=%d\n",
			frc_fw_alg_ctrl->frc_me_en);
	len +=
		sprintf(buf + len, "fw_ctrl->bbd_en\t=%d\n",
			frc_fw_alg_ctrl->bbd_en);
	len +=
		sprintf(buf + len, "fw_ctrl->vp_en\t=%d\n",
			frc_fw_alg_ctrl->vp_en);
	len +=
		sprintf(buf + len, "fw_ctrl->iplogo_en\t=%d\n",
			frc_fw_alg_ctrl->iplogo_en);
	len +=
		sprintf(buf + len, "fw_ctrl->melogo_en\t=%d\n",
			frc_fw_alg_ctrl->melogo_en);
	len +=
		sprintf(buf + len, "frc_top->memc_enable\t=%d\n",
			frc_top->memc_enable);
	len +=
		sprintf(buf + len, "frc_top->frc_memc_level\t=%d\n",
			frc_top->frc_memc_level);
	len +=
		sprintf(buf + len, "force_mc_phase0\t=%d\n",
			state_st->force_mc_phase0);
	len +=
		sprintf(buf + len, "dae0_bypass_mode\t=%d\n",
			state_st->dae0_bypass_mode);
	len +=
		sprintf(buf + len, "mc_bypass_always\t=%d\n",
			state_st->mc_bypass_always);
	len +=
		sprintf(buf + len, "force_n2m\t=%d, ratio=%d\n",
			state_st->force_n2m,
			state_st->n2m_status.frc_ratio);
	len +=
		sprintf(buf + len, "force_disable_dpe_mix\t=%d\n",
			state_st->force_disable_dpe_mix);
	len +=
		sprintf(buf + len, "force_not_send_frc_vf\t=%d\n",
			state_st->force_not_send_frc_vf);
	len +=
		sprintf(buf + len, "disable_io_ctrl\t=%d\n",
			dbg_st->ctrl_dbg.disable_io_ctrl);
	len +=
		sprintf(buf + len, "enable_frclink_cnt\t=%d\n",
			state_st->enable_frclink_cnt);
	len +=
		sprintf(buf + len, "enable_mc_link\t=%d\n",
			enable_mc_link);
	len +=
		sprintf(buf + len, "force_disable_drop_last\t=%d\n",
			state_st->force_disable_drop_last);
	len +=
		sprintf(buf + len, "force_disable_check_fallback\t=%d\n",
			state_st->force_disable_check_fallback);
	len +=
		sprintf(buf + len, "ui_frc_state_sel\t=%d\n",
			dbg_st->ctrl_dbg.ui_frc_state_sel);
	len +=
		sprintf(buf + len, "dbg_vfcd_hold_line\t=%d\n",
			pchip_st->dbg_vfcd_holdline);
	len +=
		sprintf(buf + len, "frc_dae_div4\t=%d\n",
			dbg_st->ctrl_dbg.frc_dae_div4);
	len +=
		sprintf(buf + len, "frc_get_crop\t= [%d,%d,%d,%d]\n",
			frc_top->crop_top, frc_top->crop_left,
			frc_top->crop_bottom, frc_top->crop_right);
	len +=
		sprintf(buf + len, "frc_use_vfcd_addr\t=%d\n",
			dbg_st->ctrl_dbg.frc_use_vfcd_addr);
	return len;
}

static void dpss_frc_debug_if(const char *buf, size_t count)
{
	char *buf_orig, *parm[47] = { NULL };
	int val1, val2;
	// struct dpss_frc_fw_data_s *fw_data;
	// struct frc_attribute_s *attr_st;
	// struct frc_state_s *state_st;
	// struct frc_clock_s *clk_st;
	struct frc_debug_s *dbg_st;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;

	if (!buf)
		return;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;

	// fw_data = (struct dpss_frc_fw_data_s *)devp->fw_data;
	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;
	// attr_st = pst_chip->attr_st;
	// clk_st = pst_chip->clk_st;
	// state_st = pst_chip->state_st;
	dbg_st = &pchip_st->dbg_st;
	state_st = &pchip_st->state_st;

	dpss_frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "status")) {
		dpss_frc_status();
	} else if (!strcmp(parm[0], "dbg_level")) {
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_dbg_en = (int)val1;
		pr_frc(0, "frc_dbg_en=%d\n", frc_dbg_en);
	} else if (!strcmp(parm[0], "monitor_inp")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < MONITOR_REG_MAX) {
				if (kstrtoint(parm[2], 16, &val2) == 0) {
					if (val2 < DPSS_RDMA_BUF_IDX_CLR + 1) {
						dbg_st->monitor_dbg.dbg_inp_reg[val1] = val2;
						dbg_st->monitor_dbg.dbg_reg_monitor_inp = 1;
					}
				}
			} else {
				dbg_st->monitor_dbg.dbg_reg_monitor_inp = 0;
			}
		}
	} else if (!strcmp(parm[0], "monitor_dae0")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < MONITOR_REG_MAX) {
				if (kstrtoint(parm[2], 16, &val2) == 0) {
					if (val2 < DPSS_RDMA_BUF_IDX_CLR + 1) {
						dbg_st->monitor_dbg.dbg_dae0_reg[val1] = val2;
						dbg_st->monitor_dbg.dbg_reg_monitor_dae0 = 1;
					}
				}
			} else {
				dbg_st->monitor_dbg.dbg_reg_monitor_dae0 = 0;
			}
		}
	} else if (!strcmp(parm[0], "monitor_pre_vs")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < MONITOR_REG_MAX) {
				if (kstrtoint(parm[2], 16, &val2) == 0) {
					if (val2 < DPSS_RDMA_BUF_IDX_CLR + 1) {
						dbg_st->monitor_dbg.dbg_pre_vs_reg[val1] = val2;
						dbg_st->monitor_dbg.dbg_reg_monitor_pre_vs = 1;
					}
				}
			} else {
				dbg_st->monitor_dbg.dbg_reg_monitor_pre_vs = 0;
			}
		}
	} else if (!strcmp(parm[0], "monitor_vsync")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < MONITOR_REG_MAX) {
				if (kstrtoint(parm[2], 16, &val2) == 0) {
					if (val2 < DPSS_RDMA_BUF_IDX_CLR + 1) {
						dbg_st->monitor_dbg.dbg_vs_reg[val1] = val2;
						dbg_st->monitor_dbg.dbg_reg_monitor_vs = 1;
					}
				}
			} else {
				dbg_st->monitor_dbg.dbg_reg_monitor_vs = 0;
			}
		}
	} else if (!strcmp(parm[0], "dbg_cfg")) {
		if (!parm[2]) {
			pr_frc(0, "err:input parameters error!\n");
			goto exit;
		}
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (kstrtoint(parm[2], 10, &val2) == 0) {
				dbg_st->debug_log_en_cfg[val1 % 999] = val2 % 4;
				frc_module_log_level[val1 % 999] = val2 % 4;
			}
		}
	} else if (!strcmp(parm[0], "force_ratio")) {
		if (!parm[2])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->dbg_set_n2m = val1;
		if (kstrtoint(parm[2], 10, &val1) == 0)
			state_st->n2m_status.force_frc_ratio = val1;
	} else if (!strcmp(parm[0], "check_frc_status")) {
		if (!parm[1]) {
			pr_frc(0, "err:input parameters error!\n");
			goto exit;
		}
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->check_frc_status_en = val1;
	} else if (!strcmp(parm[0], "memc_level")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_frc_set_dejudder((u8)val1);
	} else if (!strcmp(parm[0], "memc_deblur")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_frc_set_deblur((u8)val1);
	} else if (!strcmp(parm[0], "memc_enable")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (pchip_st->dbg_st.ctrl_dbg.ui_frc_state_sel)
				dpss_frc_set_mc_bypass((u8)val1);
			else
				dpss_set_mc_link_state((u8)val1);
		}
	} else if (!strcmp(parm[0], "demo_win")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_frc_demo_win((u8)val1, 1);
	} else if (!strcmp(parm[0], "frc_me_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			dpss_frc_set_memc_enable(FRC_ME_FUNC, (u8)val1);
			state_st->cur_frc_me_en = val1;
		}
	}  else if (!strcmp(parm[0], "frc_bbd_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_frc_set_memc_enable(FRC_BBD_FUNC, (u8)val1);
	}  else if (!strcmp(parm[0], "frc_vp_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_frc_set_memc_enable(FRC_VP_FUNC, (u8)val1);
	} else if (!strcmp(parm[0], "frc_melg_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_frc_set_memc_enable(FRC_MELG_FUNC, (u8)val1);
	} else if (!strcmp(parm[0], "frc_iplg_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_frc_set_memc_enable(FRC_IPLG_FUNC, (u8)val1);
	} else if (!strcmp(parm[0], "force_mc_phase0")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->force_mc_phase0 = val1;
	} else if (!strcmp(parm[0], "force_disable_dpe_mix")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->force_disable_dpe_mix = val1;
	} else if (!strcmp(parm[0], "force_not_send_frc_vf")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->force_not_send_frc_vf = val1;
	} else if (!strcmp(parm[0], "disable_io_ctrl")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dbg_st->ctrl_dbg.disable_io_ctrl = val1;
	} else if (!strcmp(parm[0], "enable_frclink_cnt")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->enable_frclink_cnt = val1;
	} else if (!strcmp(parm[0], "enable_mc_link")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			enable_mc_link = val1;
	} else if (!strcmp(parm[0], "force_disable_drop_last")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->force_disable_drop_last = val1;
	} else if (!strcmp(parm[0], "force_disable_check_fallback")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->force_disable_check_fallback = val1;
	} else if (!strcmp(parm[0], "ui_frc_state_sel")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dbg_st->ctrl_dbg.ui_frc_state_sel = val1;
	} else if (!strcmp(parm[0], "vfcd_hold_line")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			pchip_st->dbg_vfcd_holdline = val1;
	} else if (!strcmp(parm[0], "frc_dae_div4")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dbg_st->ctrl_dbg.frc_dae_div4 = val1;
	} else if (!strcmp(parm[0], "frc_use_vfcd_addr")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dbg_st->ctrl_dbg.frc_use_vfcd_addr = val1;
	}
exit:
	kfree(buf_orig);
}

/*
 *ssize_t frc_debug_buf_if_help(struct dpss_dev_s *devp, char *buf)
 *{
 *	ssize_t len = 0;
 *	len += sprintf(buf + len, "dump_bufcfg\t: dump buf address, size\n");
 *	len += sprintf(buf + len, "dump_linkbuf\t: dump link buffer data\n");
 *	len += sprintf(buf + len, "dump_init_reg\t: dump initial table\n");
 *	len += sprintf(buf + len, "dump_fixed_reg\t: dump fixed table\n");
 *	len += sprintf(buf + len, "dump_buf_reg\t: dump buffer register\n");
 *	len += sprintf(buf + len, "dump_data addr size\t: dump cma buf data\n");
 *	len += sprintf(buf + len,
 *		"buf_num\t\t:%d\n", devp->pchip_st->buf_st->frm_buf_num);
 *	len += sprintf(buf + len,
 *		"dc_set\t: x x x(me:mc_y:mc_c) set frc me,mc_y and mc_c comprate\n");
 *	len += sprintf(buf + len,
 *		"dc_mcdw_set\t: x x(mcdw_y:mcdw_c) set mcdw_y and mcdw_c comprate\n");
 *	len += sprintf(buf + len,
 *		"mcdw_ratio\t: 0xXX  set mcdw mc_y and mc_c ratio\n");
 *	len += sprintf(buf + len, "dc_apply\t: reset buffer when frc disable\n");
 *	return len;
 *}
 *
 *void frc_debug_buf_if(struct dpss_dev_s *devp, const char *buf, size_t count)
 *{
 *	char *buf_orig, *parm[47] = {NULL};
 *	int val1;
 *	int val2;
 *	struct frc_chip_st *pchip_st;
 *
 *	if (!devp)
 *		return;
 *
 *	if (!buf)
 *		return;
 *
 *	pchip_st = devp->pchip_st;
 *	buf_orig = kstrdup(buf, GFP_KERNEL);
 *	if (!buf_orig)
 *		return;
 *
 *	frc_debug_parse_param(buf_orig, (char **)&parm);
 *
 *	if (!strcmp(parm[0], "dump_bufcfg")) {
 *		frc_buf_dump_memory_size_info(pchip_st);
 *		frc_buf_dump_memory_addr_info(pchip_st);
 *	} else if (!strcmp(parm[0], "dump_linkbuf")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (val1 < FRC_BUF_MAX_IDX)
 *				frc_buf_dump_link_tab(pchip_st, (u32)val1);
 *		}
 *	} else if (!strcmp(parm[0], "dump_init_reg")) {
 *		frc_dump_reg_tab();
 *	} else if (!strcmp(parm[0], "dump_fixed_reg")) {
 *		frc_dump_fixed_table();
 *	} else if (!strcmp(parm[0], "dump_buf_reg")) {
 *		frc_dump_buf_reg();
 *	} else if (!strcmp(parm[0], "dump_data")) {
 *		if (kstrtoint(parm[1], 16, &val1))
 *			goto exit;
 *		if (kstrtoint(parm[2], 16, &val2))
 *			goto exit;
 *		frc_dump_buf_data(pchip_st, (u32)val1, (u32)val2);
 *	} else if (!strcmp(parm[0], "buf_num")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_set_buf_num((u32)val1);
 *	} else if (!strcmp(parm[0], "dc_set")) { //(me:mc_y:mc_c)
 *		if (!parm[3]) {
 *			pr_frc(0, "err:input me mc_y mc_c\n");
 *			goto exit;
 *		}
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			pchip_st->buf_st->me_comprate = val1;
 *		if (kstrtoint(parm[2], 10, &val1) == 0)
 *			pchip_st->buf_st->mc_y_comprate = val1;
 *		if (kstrtoint(parm[3], 10, &val1) == 0)
 *			pchip_st->buf_st->mc_c_comprate = val1;
 *	} else if (!strcmp(parm[0], "dc_mcdw_set")) { //(me:mc_y:mc_c)
 *		if (!parm[2]) {
 *			pr_frc(0, "err:input mcdw_y mcdw_c\n");
 *			goto exit;
 *		}
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			pchip_st->buf_st->mcdw_y_comprate = val1;
 *		if (kstrtoint(parm[2], 10, &val1) == 0)
 *			pchip_st->buf_st->mcdw_c_comprate = val1;
 *	} else if (!strcmp(parm[0], "mcdw_ratio")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_set_mcdw_buffer_ratio(val1);
 *	} else if (!strcmp(parm[0], "dc_apply")) {
 *		if (pchip_st->state_st->frc_sts.state == FRC_STATE_BYPASS) {
 *			frc_buf_release(pchip_st);
 *			frc_buf_set(devp);
 *		}
 *	}
 *exit:
 *	kfree(buf_orig);
 *}
 *
 *ssize_t frc_debug_rdma_if_help(struct dpss_dev_s *devp, char *buf)
 *{
 *	ssize_t len = 0;
 *	struct frc_rdma_s *frc_rdma = get_frc_rdma();
 *
 *	if (!devp)
 *		return len;
 *
 *	len += sprintf(buf + len, "status\t=show frc rdma status\n");
 *	// len += sprintf(buf + len, "frc_rdma\t=ctrl or debug frc rdma\n");
 *	len += sprintf(buf + len, "addr_val\t=addr val(set reg value to rdma table)\n");
 *	len += sprintf(buf + len, "rdma_en\t\t=%d drv, %d alg\n",
 *		frc_rdma->rdma_en, frc_rdma->rdma_alg_en);
 *	len += sprintf(buf + len, "rdma_table\t=show frc rdma table\n");
 *	len += sprintf(buf + len, "set trace_enable: echo 1 > /sys/class/frc/trace_enable\n");
 *	len += sprintf(buf + len, "set trace_reg: echo 0x60 xx > /sys/class/frc/trace_reg\n");
 *
 *	return len;
 *}
 *
 *ssize_t frc_debug_param_if_help(struct dpss_dev_s *devp, char *buf)
 *{
 *	ssize_t len = 0;
 *	struct dpss_frc_fw_data_s *fw_data;
 *	struct frc_debug_s *dbg_st;
 *	struct frc_state_s *state_st;
 *
 *	dbg_st = &devp->pchip_st->dbg_st;
 *	state_st = devp->pchip_st->state_st;
 *	fw_data = (struct dpss_frc_fw_data_s *)devp->fw_data;
 *	len += sprintf(buf + len, "dbg_input_hsize\t=%d\n", dbg_st->force_dbg.dbg_input_hsize);
 *	len += sprintf(buf + len, "dbg_input_vsize\t=%d\n", dbg_st->force_dbg.dbg_input_vsize);
 *	len += sprintf(buf + len, "dbg_ratio\t=%d\n", dbg_st->force_dbg.dbg_in_out_ratio);
 *	len += sprintf(buf + len, "dbg_force\t=%d\n", dbg_st->force_dbg.dbg_force_en);
 *	len += sprintf(buf + len, "monitor_ireg\t=%d\n", dbg_st->monitor_dbg.dbg_reg_monitor_i);
 *	len += sprintf(buf + len, "monitor_oreg\t=%d\n", dbg_st->monitor_dbg.dbg_reg_monitor_o);
 *	len += sprintf(buf + len, "monitor_dump\n");
 *	len += sprintf(buf + len, "monitor_vf\t=%d\n", dbg_st->monitor_dbg.dbg_vf_monitor);
 *	len += sprintf(buf + len, "secure_on\t=[start_addr size] under 32bit ddr\n");
 *	len += sprintf(buf + len, "secure_off\t=closed frc secure buf\n");
 *	len += sprintf(buf + len, "set_seg\t\t=(read reg check)\n");
 *	len += sprintf(buf + len, "set_demo\t=(read reg check)\n");
 *	len += sprintf(buf + len, "demo_win\t=(read reg check)\n");
 *	len += sprintf(buf + len, "out_line\t=%d\n", dbg_st->ctrl_dbg.out_line);
 *	len += sprintf(buf + len, "inp_err\t\t=%d\n", dbg_st->ud_dbg.res1_time_en);
 *	len += sprintf(buf + len, "dbg_ro\t\t=(check DBG_STAT reg\n");
 *	len += sprintf(buf + len, "clk_ctrl\t=%d\n", devp->pchip_st->clk_st->clk_ctrl);
 *	len += sprintf(buf + len, "frc_force_in\t=%d\n",
 *					(fw_data->frc_top_type.vfp & BIT_4));
 *	len += sprintf(buf + len, "frc_no_tell\t=%d\n",
 *					(fw_data->frc_top_type.vfp & BIT_7));
 *	len += sprintf(buf + len, "frc_dp [0-5]\t=1:red 5:black display pattern\n");
 *	len += sprintf(buf + len, "frc_ip [0-5]\t=1:red 5:black input pattern\n");
 *	len += sprintf(buf + len, "osdbit_fcolr\t=(read reg check)\n");
 *	len += sprintf(buf + len, "prot_mode\t=%d\n", state_st->frc_sts.prot_mode);
 *	len += sprintf(buf + len, "set_urgent\t=(read reg check)\n");
 *	len += sprintf(buf + len, "no_ko_mode\t=%d\n", state_st->frc_sts.no_ko_mode);
 *	len += sprintf(buf + len, "tell_ready\t=%d\n", dbg_st->ctrl_dbg.frc_on_dly_cnt);
 *	len += sprintf(buf + len, "chg_slice_num\t=(read reg check)\n");
 *	len += sprintf(buf + len, "control_0\t=%d\n", devp->pchip_st->other_st.control_0);
 *	return len;
 *}
 *
 *void frc_debug_param_if(struct dpss_dev_s *devp, const char *buf, size_t count)
 *{
 *	int val1, val2, val3;
 *	struct frc_chip_st *pst_chip;
 *	struct frc_debug_s *dbg_st;
 *	struct frc_state_s *state_st;
 *	struct frc_method_s *method_st;
 *	struct frc_other_s *other_st;
 *	char *buf_orig, *parm[47] = {NULL};
 *
 *	if (!devp || !buf)
 *		return;
 *
 *	buf_orig = kstrdup(buf, GFP_KERNEL);
 *	if (!buf_orig)
 *		return;
 *
 *	pst_chip = devp->pchip_st;
 *	method_st = pst_chip->method_st;
 *	dbg_st = &pst_chip->dbg_st;
 *	state_st = pst_chip->state_st;
 *	other_st = &pst_chip->other_st;
 *	frc_debug_parse_param(buf_orig, (char **)&parm);
 *
 *	if (!strcmp(parm[0], "dbg_size")) {
 *		if (!parm[1] || !parm[2])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->force_dbg.dbg_input_hsize = (u32)val1;
 *
 *		if (kstrtoint(parm[2], 10, &val2) == 0)
 *			dbg_st->force_dbg.dbg_input_vsize = (u32)val2;
 *	} else if (!strcmp(parm[0], "dbg_ratio")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->force_dbg.dbg_in_out_ratio = val1;
 *		pr_frc(0, "dbg_in_out_ratio:0x%x\n", dbg_st->force_dbg.dbg_in_out_ratio);
 *	} else if (!strcmp(parm[0], "dbg_force")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->force_dbg.dbg_force_en = (u32)val1;
 *	} else if (!strcmp(parm[0], "monitor_ireg")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (val1 < MONITOR_REG_MAX) {
 *				if (kstrtoint(parm[2], 16, &val2) == 0) {
 *					if (val1 < 0x3fff) {
 *						dbg_st->monitor_dbg.dbg_in_reg[val1] = val2;
 *						dbg_st->monitor_dbg.dbg_reg_monitor_i = 1;
 *					}
 *				}
 *			} else {
 *				dbg_st->monitor_dbg.dbg_reg_monitor_i = 0;
 *			}
 *		}
 *	} else if (!strcmp(parm[0], "monitor_oreg")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (val1 < MONITOR_REG_MAX) {
 *				if (kstrtoint(parm[2], 16, &val2) == 0) {
 *					if (val1 < 0x3fff) {
 *						dbg_st->monitor_dbg.dbg_out_reg[val1] = val2;
 *						dbg_st->monitor_dbg.dbg_reg_monitor_o = 1;
 *					}
 *				}
 *			} else {
 *				dbg_st->monitor_dbg.dbg_reg_monitor_o = 0;
 *			}
 *		}
 *	} else if (!strcmp(parm[0], "monitor_vf")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			dbg_st->monitor_dbg.dbg_vf_monitor = val1;
 *			dbg_st->monitor_dbg.dbg_buf_len = 0;
 *		}
 *	} else if (!strcmp(parm[0], "monitor_dump")) {
 *		frc_dump_monitor_data(devp);
 *	} else if (!strcmp(parm[0], "set_seg")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_set_seg_display((u8)val1, 8, 8, 8);
 *	} else if (!strcmp(parm[0], "set_demo")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_memc_set_demo((u8)val1);
 *	} else if (!strcmp(parm[0], "demo_win")) {
 *		//Test whether demo window works properly
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			set_frc_demo_window((u8)val1);
 *	} else if (!strcmp(parm[0], "out_line")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			dbg_st->ctrl_dbg.out_line = (u32)val1;
 *			pr_frc(2, "set frc adj me out line is %d\n",
 *				dbg_st->ctrl_dbg.out_line);
 *		}
 *	} else if (!strcmp(parm[0], "chk_motion")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.get_glb_motion_no_alg = val1;
 *	} else if (!strcmp(parm[0], "inp_err")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ud_dbg.res1_time_en = val1;
 *	} else if (!strcmp(parm[0], "dbg_ro")) {
 *		if (!parm[1]) {
 *			pr_frc(0, "err: input check\n");
 *			goto exit;
 *		}
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_check_hw_stats(devp, val1);
 *	} else if (!strcmp(parm[0], "clk_ctrl")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			pst_chip->clk_st->clk_ctrl = val1;
 *			schedule_work(&pst_chip->work_st.clk_work);
 *		}
 *	} else if (!strcmp(parm[0], "frc_force_in")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_set_enter_forcefilm(devp, val1);
 *	} else if (!strcmp(parm[0], "frc_no_tell")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_set_notell_film(devp, val1);
 *	} else if (!strcmp(parm[0], "frc_dp")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (val1) {
 *				dbg_st->pat_dbg.pat_en = 1;
 *				dbg_st->pat_dbg.pat_type |= BIT_1;
 *				dbg_st->pat_dbg.pat_color = (u8)val1;
 *			} else {
 *				dbg_st->pat_dbg.pat_en = 0;
 *				dbg_st->pat_dbg.pat_type |= ~BIT_1;
 *				dbg_st->pat_dbg.pat_color = (u8)val1;
 *			}
 *			frc_set_output_pattern(val1);
 *		}
 *	} else if (!strcmp(parm[0], "frc_ip")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (val1) {
 *				dbg_st->pat_dbg.pat_en = 1;
 *				dbg_st->pat_dbg.pat_type |= BIT_0;
 *				dbg_st->pat_dbg.pat_color = (u8)val1;
 *			} else {
 *				dbg_st->pat_dbg.pat_en = 0;
 *				dbg_st->pat_dbg.pat_type |= ~BIT_0;
 *				dbg_st->pat_dbg.pat_color = (u8)val1;
 *			}
 *			frc_set_input_pattern(val1);
 *		}
 *	} else if (!strcmp(parm[0], "ratio")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->frc_sts.in_out_ratio = val1;
 *	} else if (!strcmp(parm[0], "osdbit_fcolr")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			method_st->set_osdbit_falsecolor(val1);
 *	} else if (!strcmp(parm[0], "secure_on")) {
 *		if (!parm[1] || !parm[2])
 *			goto exit;
 *		if (kstrtoint(parm[1], 16, &val1) == 0) {
 *			if (kstrtoint(parm[2], 16, &val2) == 0)
 *				frc_test_mm_secure_set_on(devp, val1, val2);
 *		}
 *	} else if (!strcmp(parm[0], "secure_off")) {
 *		frc_test_mm_secure_set_off(devp);
 *	} else if (!strcmp(parm[0], "prot_mode")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->frc_sts.prot_mode = val1;
 *	} else if (!strcmp(parm[0], "set_urgent")) {
 *		if (!parm[1] || !parm[2] || !parm[3])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (kstrtoint(parm[2], 10, &val2) == 0)
 *				if (kstrtoint(parm[3], 10, &val3) == 0)
 *					frc_set_arb_ugt_cfg(val1, val2, val3);
 *		}
 *	} else if (!strcmp(parm[0], "no_ko_mode")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->frc_sts.no_ko_mode = val1;
 *	} else if (!strcmp(parm[0], "tell_ready")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.frc_on_dly_cnt = val1;
 *	} else if (!strcmp(parm[0], "chg_slice_num")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (method_st->set_slice_num)
 *				method_st->set_slice_num((u8)val1);
 *			else
 *				pr_frc(2, "chip(%d) unsupported\n", pst_chip->chip);
 *		}
 *	} else if (!strcmp(parm[0], "control_0")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			other_st->control_0 = val1;
 *	}
 *exit:
 *	kfree(buf_orig);
 *}
 *
 *ssize_t frc_debug_other_if_help(struct dpss_dev_s *devp, char *buf)
 *{
 *	ssize_t len = 0;
 *	struct dpss_frc_fw_data_s *fw_data;
 *	struct frc_state_s *state_st;
 *	struct frc_debug_s *dbg_st;
 *	struct frc_other_s *other_st;
 *
 *	if (!devp)
 *		return len;
 *
 *	state_st = devp->pchip_st->state_st;
 *	dbg_st = &devp->pchip_st->dbg_st;
 *	other_st = &devp->pchip_st->other_st;
 *	fw_data = (struct dpss_frc_fw_data_s *)devp->fw_data;
 *
 *	len += sprintf(buf + len, "crc_check_frm\t=%d %d\n",
 *		other_st->frc_crc_data.me_check_frm, other_st->frc_crc_data.mc_check_frm);
 *	len += sprintf(buf + len, "high_freq_en\t=%d\n", state_st->in_sts.high_freq_en);
 *	len += sprintf(buf + len, "inp_align_adj_en\t=%d\n", state_st->in_sts.inp_align_adj_en);
 *	len += sprintf(buf + len, "crash_int_en\t=(check log)\n");
 *	len += sprintf(buf + len, "pr_dbg\t\t=%d\n", dbg_st->ctrl_dbg.pr_dbg);
 *	len += sprintf(buf + len, "pre_vsync\t=%d\n", state_st->frc_sts.use_pre_vsync);
 *	len += sprintf(buf + len, "mute_en\t\t=%d\t%d\n",
 *		state_st->in_sts.enable_mute_flag, state_st->in_sts.mute_vsync_cnt);
 *	len += sprintf(buf + len, "task_hi_en\t=%d\n", state_st->frc_sts.hi_priority_task_en);
 *	len += sprintf(buf + len, "timer_ctrl\t=en:%d level:%d interval:%d\n",
 *			dbg_st->timer_dbg.timer_en, dbg_st->timer_dbg.timer_level,
 *			dbg_st->timer_dbg.time_interval);
 *	len += sprintf(buf + len, "frm_seg_en\t=%d\n", state_st->in_sts.frm_seg_en);
 *	len += sprintf(buf + len, "motion_ctrl\t=%d, read reg =0x%4x\n",
 *			fw_data->frc_top_type.motion_ctrl, fw_data->reg_val[0].addr);
 *	len += sprintf(buf + len, "task_run\t=%d\n", state_st->frc_sts.task_run_method);
 *	len += sprintf(buf + len, "freq_dis\t=%d\n", dbg_st->ctrl_dbg.dbg_freq_disable);
 *	len += sprintf(buf + len, "dis h_v size\t=%d\t%d\n",
 *		state_st->frc_sts.disable_h_size, state_st->frc_sts.disable_v_size);
 *
 *	return len;
 *}
 *
 *void frc_debug_other_if(struct dpss_dev_s *devp, const char *buf, size_t count)
 *{
 *	char *buf_orig, *parm[47] = {NULL};
 *	int val1;
 *	struct dpss_frc_fw_data_s *fw_data;
 *	struct frc_chip_st *pst_chip;
 *	struct frc_state_s *state_st;
 *	struct frc_debug_s *dbg_st;
 *	struct frc_other_s *other_st;
 *
 *	if (!devp || !buf)
 *		return;
 *
 *	pst_chip = devp->pchip_st;
 *	state_st = pst_chip->state_st;
 *	dbg_st = &pst_chip->dbg_st;
 *	other_st = &devp->pchip_st->other_st;
 *	fw_data = (struct dpss_frc_fw_data_s *)devp->fw_data;
 *	buf_orig = kstrdup(buf, GFP_KERNEL);
 *	if (!buf_orig)
 *		return;
 *
 *	frc_debug_parse_param(buf_orig, (char **)&parm);
 *
 *	if (!strcmp(parm[0], "high_freq_en")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->in_sts.high_freq_en = val1;
 *	} else if (!strcmp(parm[0], "inp_adj_en")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->in_sts.inp_align_adj_en = val1;
 *	} else if (!strcmp(parm[0], "crash_int_en")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			frc_set_axi_crash_irq(devp, val1);
 *	} else if (!strcmp(parm[0], "crc_read")) {
 *		frc_crc_read(devp);
 *	} else if (!strcmp(parm[0], "crc_check_frm")) {
 *		if (!parm[2])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			other_st->frc_crc_data.me_check_frm = val1;
 *		if (kstrtoint(parm[2], 10, &val1) == 0)
 *			other_st->frc_crc_data.mc_check_frm = val1;
 *		frc_crc_check_frm(devp);
 *	} else if (!strcmp(parm[0], "del_120_pth")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (pst_chip->method_st->revb_patch)
 *				pst_chip->method_st->revb_patch();
 *		}
 *	} else if (!strcmp(parm[0], "pr_dbg")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (dbg_st->ctrl_dbg.pr_dbg)
 *				pr_frc(2, "processing, try again later\n");
 *			else
 *				dbg_st->ctrl_dbg.pr_dbg = (u8)val1;
 *		}
 *	} else if (!strcmp(parm[0], "clr_mv_buf")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.clr_vbuf_ctl = val1;
 *	} else if (!strcmp(parm[0], "pre_vsync")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->frc_sts.use_pre_vsync = val1;
 *	} else if (!strcmp(parm[0], "mvrd_mode")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.dbg_mvrd_mode = val1;
 *	} else if (!strcmp(parm[0], "mute_dis")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.dbg_mute_disable = val1;
 *	} else if (!strcmp(parm[0], "frc_sus")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (val1 == 0) {
 *				state_st->frc_sts.auto_ctrl = false;
 *				PR_FRC("call %s\n", __func__);
 *				frc_power_domain_ctrl(devp, 0);
 *				if (pst_chip->power_on_flag)
 *					pst_chip->power_on_flag = false;
 *			} else {
 *				PR_FRC("call %s\n", __func__);
 *				frc_power_domain_ctrl(devp, 1);
 *				if (!pst_chip->power_on_flag)
 *					pst_chip->power_on_flag = true;
 *				pst_chip->method_st->set_frc_bypass(ON);
 *				state_st->frc_sts.auto_ctrl = true;
 *				state_st->frc_sts.re_config = true;
 *			}
 *		}
 *	} else if (!strcmp(parm[0], "mute_en")) {
 *		if (!parm[2])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			state_st->in_sts.enable_mute_flag = val1;
 *			if (val1)
 *				dbg_st->pat_dbg.pat_en = 1;
 *			else
 *				dbg_st->pat_dbg.pat_en = 0;
 *		}
 *		if (kstrtoint(parm[2], 10, &val1) == 0)
 *			state_st->in_sts.mute_vsync_cnt = val1;
 *	} else if (!strcmp(parm[0], "align_dbg")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.align_dbg_en = val1;
 *	} else if (!strcmp(parm[0], "task_hi_en")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->frc_sts.hi_priority_task_en = (u8)val1;
 *	} else if (!strcmp(parm[0], "timer_ctrl")) {
 *		if (!parm[3])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->timer_dbg.timer_en = (u8)val1;
 *		if (kstrtoint(parm[2], 10, &val1) == 0)
 *			dbg_st->timer_dbg.timer_level = (u16)val1;
 *		if (kstrtoint(parm[3], 10, &val1) == 0)
 *			dbg_st->timer_dbg.time_interval =
 *				(u8)(val1 > 16 ? 16 : val1);
 *		frc_timer_proc(devp);
 *	} else if (!strcmp(parm[0], "frm_seg_en")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->in_sts.frm_seg_en = val1;
 *	} else if (!strcmp(parm[0], "motion_ctrl")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			fw_data->frc_top_type.motion_ctrl = val1;
 *	} else if (!strcmp(parm[0], "task_run")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->frc_sts.task_run_method = val1;
 *	} else if (!strcmp(parm[0], "adj_mcdw")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0) {
 *			if (val1 >= 0 && val1 < 4)
 *				state_st->frc_sts.t3x_adj_mcdw_hv = val1;
 *		}
 *	} else if (!strcmp(parm[0], "freq_dis")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.dbg_freq_disable = val1;
 *	} else if (!strcmp(parm[0], "disable_size")) {
 *		if (!parm[2])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			state_st->frc_sts.disable_h_size = (val1 > 3840) ? 3840 : val1;
 *		if (kstrtoint(parm[2], 10, &val1) == 0)
 *			state_st->frc_sts.disable_v_size = (val1 > 2160) ? 2160 : val1;
 *	} else if (!strcmp(parm[0], "dur_dis")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->ctrl_dbg.dbg_dur0_disable = val1;
 *	}
 *exit:
 *	kfree(buf_orig);
 *}
 *
 *ssize_t frc_probe_dbg_if_help(struct dpss_dev_s *devp, char *buf)
 *{
 *	ssize_t len = 0;
 *	u32 reg_data;
 *	struct frc_debug_s *dbg_st;
 *
 *	if (!devp)
 *		return len;
 *
 *	dbg_st = &devp->pchip_st->dbg_st;
 *
 *	len += sprintf(buf + len, "path\t= %d [0:inp 1:vp 2:mc 3:mc csc]\n",
 *		dbg_st->probe_dbg.probe_path);
 *	len += sprintf(buf + len, "posi\t= (x:%d y:%d)\n",
 *		dbg_st->probe_dbg.posi_x, dbg_st->probe_dbg.posi_y);
 *	len += sprintf(buf + len, "en\t= %d\n", dbg_st->probe_dbg.probe_en);
 *	len += sprintf(buf + len, "color\t= %d [0:W 1:R 2:G 3:B]\n",
 *		dbg_st->probe_dbg.probe_color);
 *
 *	if (dbg_st->probe_dbg.probe_en) {
 *		frc_probe_dbg_proc(devp);
 *		reg_data = dbg_st->probe_dbg.out_data;
 *		len += sprintf(buf + len, "out-color\t= 0x%8x (Y:%d U:%d V:%d)\n",
 *			reg_data,
 *			(reg_data >> 20) & 0x3ff,
 *			(reg_data >> 10) & 0x3ff,
 *			(reg_data >>  0) & 0x3ff);
 *	}
 *
 *	return len;
 *}
 *
 *void frc_probe_dbg_if(struct dpss_dev_s *devp, const char *buf, size_t count)
 *{
 *	char *buf_orig, *parm[47] = {NULL};
 *	int val1, val2;
 *	struct frc_debug_s *dbg_st;
 *
 *	if (!devp || !buf)
 *		return;
 *
 *	buf_orig = kstrdup(buf, GFP_KERNEL);
 *	if (!buf_orig)
 *		return;
 *
 *	dbg_st = &devp->pchip_st->dbg_st;
 *	frc_debug_parse_param(buf_orig, (char **)&parm);
 *
 *	if (!strcmp(parm[0], "en")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->probe_dbg.probe_en = (u8)val1;
 *		frc_probe_dbg_proc(devp);
 *	} else if (!strcmp(parm[0], "path")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->probe_dbg.probe_path = (u8)val1;
 *		frc_probe_dbg_proc(devp);
 *	} else if (!strcmp(parm[0], "posi")) {
 *		if (!parm[2])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->probe_dbg.posi_x = (u16)val1;
 *		if (kstrtoint(parm[2], 10, &val2) == 0)
 *			dbg_st->probe_dbg.posi_y = (u16)val2;
 *		frc_probe_dbg_proc(devp);
 *	} else if (!strcmp(parm[0], "color")) {
 *		if (!parm[1])
 *			goto exit;
 *		if (kstrtoint(parm[1], 10, &val1) == 0)
 *			dbg_st->probe_dbg.probe_color = (u8)val1;
 *		frc_probe_dbg_proc(devp);
 *	}
 *exit:
 *	kfree(buf_orig);
 *}
 */

static void dpss_frc_reg_io(const char *buf)
{
	char *buf_orig, *parm[8] = { NULL };
	ulong val;
	unsigned int reg;
	unsigned int regvalue;
	unsigned int len;
	int i;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;
	dpss_frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "r")) {
		if (!parm[1])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		regvalue = rd(reg);
		pr_frc(0, "[0x%x] = 0x%x\n", reg, regvalue);
	} else if (!strcmp(parm[0], "w")) {
		if (!parm[1] || !parm[2])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		if (kstrtoul(parm[2], 16, &val) < 0)
			goto free_buf;
		regvalue = val;
		wr(reg, regvalue);
		pr_frc(0, "[0x%x] = 0x%x\n", reg, regvalue);
	} else if (!strcmp(parm[0], "d")) {
		if (!parm[1] || !parm[2])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;
		reg = val;
		if (kstrtoul(parm[2], 16, &val) < 0)
			goto free_buf;
		len = val;
		for (i = 0; i < len; i++) {
			regvalue = rd(reg + i);
			pr_frc(0, "[0x%x] = 0x%x\n", reg + i, regvalue);
		}
	}

free_buf:
	kfree(buf_orig);
}

static void dpss_frc_tool_dbg_store(const char *buf)
{
	// int i, count, flag = 0;
	char *buf_orig, *parm[8] = { NULL };
	ulong val;
	unsigned int reg;
	unsigned int regvalue;
	struct tool_debug_s *tool_dbg;
	struct frc_chip_st *pchip_st;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	pchip_st = dpss_get_frc_st();

	if (!buf_orig || !pchip_st)
		return;

	tool_dbg = &pchip_st->dbg_st.tool_dbg;
	dpss_frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "r")) {
		if (!parm[1])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;

		reg = val;
		tool_dbg->reg_read = reg;
		if (tool_dbg->reg_read >= 0x8000)
			tool_dbg->reg_read_val = rd(reg);
		else
			tool_dbg->reg_read_val = rd_vc(reg);
	} else if (!strcmp(parm[0], "w")) {
		if (!parm[1] || !parm[2])
			goto free_buf;
		if (kstrtoul(parm[1], 16, &val) < 0)
			goto free_buf;

		reg = val;
		if (kstrtoul(parm[2], 16, &val) < 0)
			goto free_buf;
		regvalue = val;
		if (reg >= 0x8000)
			wr(reg, regvalue);
		else
			wr_vc(reg, regvalue);
	}

free_buf:
	kfree(buf_orig);
}

/*
 * timer
 *static enum hrtimer_restart frc_timer_callback(struct hrtimer *timer)
 *{
 *	u8 i, time;
 *	u16 log;
 *	u32 reg_val;
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	log = devp->pchip_st->dbg_st.timer_dbg.timer_level;
 *	time = devp->pchip_st->dbg_st.timer_dbg.time_interval;
 *
 *	for (i = 0; i < rdma_trace_num; i++) {
 *		reg_val = READ_FRC_REG(rdma_trace_reg[i]);
 *		pr_frc(log, "reg[%04x]=0x%08x %9d\n", rdma_trace_reg[i], reg_val, reg_val);
 *	}
 *
 *	hrtimer_forward(&devp->frc_hi_timer, hrtimer_cb_get_time(timer),
 *					ktime_set(0, time * 1000000)); // unit: ns
 *
 *	return HRTIMER_RESTART;
 *}
 *
 *void frc_timer_proc(struct dpss_dev_s *devp)
 *{
 *	u8 timer_en, time;
 *
 *	timer_en = devp->pchip_st->dbg_st.timer_dbg.timer_en;
 *	time = devp->pchip_st->dbg_st.timer_dbg.time_interval;
 *	devp->frc_hi_timer.function = frc_timer_callback;
 *
 *	if (time > 16)
 *		time = 16;
 *	else if (time < 1)
 *		time = 1;
 *
 *	if (timer_en)
 *		hrtimer_start(&devp->frc_hi_timer,
 *			ktime_set(0, time * 1000000), HRTIMER_MODE_REL); // unit: ns
 *	else
 *		hrtimer_cancel(&devp->frc_hi_timer);
 *}
 */
/* column: 1~8, color: 0~7, number: 0~15 */
/*
 *static void update_seg_7_show(u8 enable, u8 column, u8 color, u8 number)
 *{
 *	u8 value;
 *
 *	value = ((enable & 0x01) << 7) + ((color & 0x07) << 4) + (number & 0x0F);
 *
 *	// enable flag_number 2_1 ~ 2_8
 *	if (column == 1)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,
 *			value << 8, 0xFF00);
 *	else if (column == 2)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM17_NUM18_NUM21_NUM22,
 *			value, 0xFF);
 *	else if (column == 3)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,
 *			value << 24, 0xFF000000);
 *	else if (column == 4)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,
 *			value << 16, 0xFF0000);
 *	else if (column == 5)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,
 *			value << 8, 0xFF00);
 *	else if (column == 6)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM23_NUM24_NUM25_NUM26,
 *			value, 0xFF);
 *	else if (column == 7)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28,
 *			value << 24, 0xFF000000);
 *	else if (column == 8)
 *		UPDATE_FRC_REG_BITS(FRC_MC_SEVEN_FLAG_NUM27_NUM28,
 *			value << 16, 0xFF0000);
 *}
 *
 *void frc_dbg_frame_show(struct dpss_dev_s *devp)
 *{
 *	u8 i, enable, tmp_cnt;
 *	static u8 pre_flag;
 *	struct frc_state_s *state_st;
 *
 *	if (!devp)
 *		return;
 *
 *	state_st = devp->pchip_st->state_st;
 *	enable = state_st->in_sts.frm_seg_en;
 *	if (enable) {
 *		// in cnt
 *		tmp_cnt = (state_st->in_sts.vs_cnt / 100) % 10;
 *		update_seg_7_show(1, 1, 1, tmp_cnt);
 *		tmp_cnt = (state_st->in_sts.vs_cnt / 10) % 10;
 *		update_seg_7_show(1, 2, 1, tmp_cnt);
 *		tmp_cnt = state_st->in_sts.vs_cnt % 10;
 *		update_seg_7_show(1, 3, 1, tmp_cnt);
 *		// out cnt
 *		tmp_cnt = (state_st->out_sts.vs_cnt / 100) % 10;
 *		update_seg_7_show(1, 6, 2, tmp_cnt);
 *		tmp_cnt = (state_st->out_sts.vs_cnt / 10) % 10;
 *		update_seg_7_show(1, 7, 2, tmp_cnt);
 *		tmp_cnt = state_st->out_sts.vs_cnt % 10;
 *		update_seg_7_show(1, 8, 2, tmp_cnt);
 *	} else if (enable == 0 && enable != pre_flag) {
 *		for (i = 1; i < 9; i++)
 *			update_seg_7_show(0, i, 0, 0);  // clear
 *	}
 *
 *	pre_flag = enable;
 *}
 *
 *static void debug_level_func(const char *module, const char *debug_flags)
 *{
 *	int value;
 *
 *	if (kstrtoint(debug_flags, 10, &value) == 0) {
 *		frc_dbg_en = value;
 *		PR_FRC("debug_level = %d\n", frc_dbg_en);
 *	}
 *}
 *
 *static void debug_status_func(const char *module, const char *debug_flags)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	if (strcmp(debug_flags, "status") == 0)
 *		frc_status(devp);
 *}
 *
 *static void debug_ctrl_func(const char *module, const char *debug_flags)
 *{
 *	int value;
 *
 *	if (kstrtoint(debug_flags, 10, &value) == 0)
 *		frc_dbg_ctrl = value;
 *}
 *
 *static void auto_ctrl_func(const char *module, const char *debug_flags)
 *{
 *	int value;
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	if (kstrtoint(debug_flags, 10, &value) == 0) {
 *		if (frc_dbg_ctrl) {
 *			if (value < 100) { //for debug:forbid user-layer call
 *				pr_frc(0, "ctrl test..\n");
 *				return;
 *			}
 *			value = value - 100;
 *		}
 *		devp->pchip_st->state_st->frc_sts.auto_ctrl = value;
 *	}
 *}
 *
 *static void debug_mode_func(const char *module, const char *debug_flags)
 *{
 *	int value;
 *
 *	if (kstrtoint(debug_flags, 10, &value) == 0) {
 *		if (frc_dbg_ctrl) {
 *			if (value < 100) { //for debug:forbid user-layer call
 *				pr_frc(0, "ctrl test..\n");
 *				return;
 *			}
 *			value = value - 100;
 *		}
 *		if (value < FRC_STATE_NULL)
 *			frc_set_mode(value);
 *	}
 *}
 *
 *static void debug_pattern_func(const char *module, const char *debug_flags)
 *{
 *	struct frc_state_s *state_st;
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	state_st = devp->pchip_st->state_st;
 *	if (!strcmp(debug_flags, "enable"))
 *		state_st->frc_sts.frc_test_ptn = 1;
 *	else if (!strcmp(debug_flags, "disable"))
 *		state_st->frc_sts.frc_test_ptn = 0;
 *	frc_pattern_on(state_st->frc_sts.frc_test_ptn);
 *}
 *
 *static struct module_debug_node debug_nodes[] = {
 *	{
 *		.name = "debug_level",
 *		.set_debug_func_notify = debug_level_func,
 *	},
 *	{
 *		.name = "status",
 *		.set_debug_func_notify = debug_status_func,
 *	},
 *	{
 *		.name = "debug_ctrl",
 *		.set_debug_func_notify = debug_ctrl_func,
 *	},
 *	{
 *		.name = "auto_ctrl",
 *		.set_debug_func_notify = auto_ctrl_func,
 *	},
 *	{
 *		.name = "debug_mode",
 *		.set_debug_func_notify = debug_mode_func,
 *	},
 *	{
 *		.name = "test_pattern",
 *		.set_debug_func_notify = debug_pattern_func,
 *	},
 *};
 *
 *static bool get_module_config(const char *configs, const char *title, char *cmd)
 *{
 *	char *module_str;
 *	char *str_end;
 *	u8 cmd_len;
 *
 *	if (!configs || !title)
 *		return false;
 *
 *	module_str = strstr(configs, title);
 *
 *	if (module_str) {
 *		if (module_str > configs && module_str[-1] != ';')
 *			return false;
 *
 *		module_str += strlen(title);
 *		if (module_str[0] != ':' ||  module_str[1] == '\0')
 *			return false;
 *
 *		module_str += 1;
 *		str_end = strchr(module_str, ';');
 *		if (str_end) {
 *			cmd_len = str_end - module_str;
 *			if (cmd_len > MODULE_LEN - 1) {
 *				pr_frc(dbg_frc, "module_len is too long\n");
 *				return false;
 *			}
 *			strncpy(cmd, module_str, cmd_len);
 *			cmd[str_end - module_str] = '\0';
 *		} else {
 *			return false;
 *		}
 *
 *	} else {
 *		return false;
 *	}
 *	return true;
 *}
 *
 *static void set_frc_debug_flag(char *module_str, char *cmd_str)
 *{
 *	char *node_str;
 *	char *str_end;
 *	u8 node;
 *	u8 cmd_len;
 *
 *	for (node = 0; node < DEBUG_NODES; node++) {
 *		node_str = strstr(module_str, debug_nodes[node].name);
 *		if (!node_str)
 *			continue;
 *
 *		if (node_str > module_str && node_str[-1] != ',')
 *			break;
 *
 *		node_str += strlen(debug_nodes[node].name);
 *		if (node_str[0] != ':' || node_str[1] == '\0')
 *			break;
 *
 *		node_str += 1;
 *		str_end = strstr(node_str, ",");
 *		if (str_end)
 *			cmd_len = str_end - node_str;
 *		else
 *			cmd_len = strlen(node_str);
 *		if (cmd_len > CMD_LEN - 1) {
 *			pr_frc(dbg_frc, "cmd len is too long\n");
 *			break;
 *		}
 *
 *		strncpy(cmd_str, node_str, cmd_len);
 *		cmd_str[cmd_len] = '\0';
 *		pr_frc(dbg_frc + 2, "%s ok\n", __func__);
 *		debug_nodes[node].set_debug_func_notify(debug_nodes[node].name, cmd_str);
 *	}
 *}
 *
 *static void set_default_debug_flag(char *default_str, char *cmd_str)
 *{
 *	char *node_str;
 *	u8 cmd_len;
 *
 *	node_str = strstr(default_str, DEBUG_LEVEL);
 *	if (!node_str)
 *		return;
 *	if (node_str > default_str && node_str[-1] != ',')
 *		return;
 *
 *	node_str += strlen(DEBUG_LEVEL);
 *	if (node_str[0] != ':' || node_str[1] == '\0')
 *		return;
 *
 *	node_str += 1;
 *	cmd_len = strlen(node_str);
 *	if (cmd_len > CMD_LEN - 1) {
 *		pr_frc(dbg_frc, "cmd len is too long\n");
 *		return;
 *	}
 *
 *	strncpy(cmd_str, node_str, cmd_len);
 *	cmd_str[cmd_len] = '\0';
 *	pr_frc(dbg_frc + 2, "%s ok\n", __func__);
 *	if (strcmp(cmd_str, "0") == 0)
 *		debug_level_func(NULL, cmd_str);
 *	else
 *		debug_level_func(NULL, "2");
 *}
 *
 *void set_frc_config(const char *module, const char *debug, int len)
 *{
 *	char *default_str = kzalloc(sizeof(char) * 128, GFP_KERNEL);
 *	char *module_str = kzalloc(sizeof(char) * 128, GFP_KERNEL);
 *	char *cmd_str = kzalloc(sizeof(char) * 32, GFP_KERNEL);
 *
 *	if (get_module_config(debug, FRC_TITLE, module_str)) {
 *		pr_frc(dbg_frc, "%s: Display_FRC:%s\n", __func__, module_str);
 *		set_frc_debug_flag(module_str, cmd_str);
 *	} else if (get_module_config(debug, DEFAULT_TITLE, default_str)) {
 *		set_default_debug_flag(default_str, cmd_str);
 *	}
 *
 *	kfree(default_str);
 *	kfree(module_str);
 *	kfree(cmd_str);
 *}
 */

/*
 *ssize_t frc_tool_debug_show(struct class *class,
 *	struct class_attribute *attr,
 *	char *buf)
 *{
 *	struct tool_debug_s *read_parm = NULL;
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	read_parm = &devp->pchip_st->dbg_st.tool_dbg;
 *	return sprintf(buf, "[0x%x] = 0x%x\n",
 *		read_parm->reg_read, read_parm->reg_read_val);
 *}
 *
 *ssize_t frc_tool_debug_store(struct class *class,
 *	struct class_attribute *attr,
 *	const char *buf,
 *	size_t count)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *	dpss_frc_tool_dbg_store(devp, buf);
 *	return count;
 *}
 *
 *ssize_t frc_debug_show(struct class *class,
 *	struct class_attribute *attr,
 *	char *buf)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *	return dpss_frc_debug_if_help(devp, buf);
 *}
 *
 *ssize_t frc_debug_store(struct class *class,
 *	struct class_attribute *attr,
 *	const char *buf,
 *	size_t count)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *	dpss_frc_debug_if(devp, buf, count);
 *
 *	return count;
 *}
 *
 *ssize_t frc_buf_show(struct class *class,
 *	struct class_attribute *attr,
 *	char *buf)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *	return dpss_frc_debug_buf_if_help(devp, buf);
 *}
 *
 *ssize_t frc_buf_store(struct class *class,
 *	struct class_attribute *attr,
 *	const char *buf,
 *	size_t count)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	dpss_frc_debug_buf_if(devp, buf, count);
 *
 *	return count;
 *}
 *
 *ssize_t frc_param_show(struct class *class,
 *	struct class_attribute *attr,
 *	char *buf)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	return dpss_frc_debug_param_if_help(devp, buf);
 *}
 *
 *ssize_t frc_param_store(struct class *class,
 *	struct class_attribute *attr,
 *	const char *buf,
 *	size_t count)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	dpss_frc_debug_param_if(devp, buf, count);
 *
 *	return count;
 *}
 *
 *ssize_t frc_other_show(struct class *class,
 *	struct class_attribute *attr,
 *	char *buf)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	return dpss_frc_debug_other_if_help(devp, buf);
 *}
 *
 *ssize_t frc_other_store(struct class *class,
 *	struct class_attribute *attr,
 *	const char *buf,
 *	size_t count)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	dpss_frc_debug_other_if(devp, buf, count);
 *
 *	return count;
 *}
 *
 *ssize_t frc_probe_dbg_show(struct class *class,
 *	struct class_attribute *attr,
 *	char *buf)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	return dpss_frc_probe_dbg_if_help(devp, buf);
 *}
 *
 *ssize_t frc_probe_dbg_store(struct class *class,
 *	struct class_attribute *attr,
 *	const char *buf,
 *	size_t count)
 *{
 *	struct dpss_dev_s *devp = dpss_get_devp();
 *
 *	dpss_frc_probe_dbg_if(devp, buf, count);
 *
 *	return count;
 *}
 */

/*ssize_t frc_reg_show(struct class *class,
 *	struct class_attribute *attr,
 *	char *buf)
 *{
 *	pr_frc(0, "read:  echo r reg > /sys/class/frc/reg\n");
 *	pr_frc(0, "write: echo w reg value > /sys/class/frc/reg\n");
 *	pr_frc(0, "dump:  echo d reg length > /sys/class/frc/reg\n");
 *	return 0;
 *}
 *
 *ssize_t frc_reg_store(struct class *class,
 *	struct class_attribute *attr,
 *	const char *buf,
 *	size_t count)
 *{
 *	dpss_dpss_frc_reg_io(buf);
 *	return count;
 *}
 */

static ssize_t dpss_frc_dump_reg_table_if_help(char *buf)
{
	ssize_t len = 0;
	// pr_info("1: dump vfcd table\n");
	// len += sprintf(buf + len, "eg: echo vfcd_tab > /sys/class/vpu_dpss/dump_table\n");
	// len += sprintf(buf + len, "vfcd_tab: vfcd reg table dump\n");
	pr_info("eg: echo vfcd_tab > /sys/class/vpu_dpss/dump_table\n");
	pr_info("vfcd_tab: vfcd reg table dump\n");
	pr_info("frc_init_tab: dump_param_frc_init_apb\n");
	pr_info("src0_buf_tab: dump src0 all buf regs\n");
	pr_info("frc_pd_tab: dump_frc_pd_regs_apb\n");
	pr_info("iplogo_tab: dump_iplogo_regs_apb\n");
	pr_info("melogo_tab: dump_me_logo_regs_apb\n");
	pr_info("vp_init_tab: dump_param_vp_init_apb\n");
	pr_info("mc_init_tab: dump_param_mc_init_apb\n");
	pr_info("me_regs_tab: dump_me_regs_apb\n");
	pr_info("all_tab: dump all tab\n");
	pr_info("fw_tab:  dump fw tab\n");

	return len;
}

void dpss_frc_dump_reg_table_if(const char *buf)
{
	char *buf_orig, *parm[47] = { NULL };
	int val1;
	struct frc_debug_s *dbg_st;
	struct frc_chip_st *pst_chip;

	if (!buf)
		return;

	pst_chip = dpss_get_frc_st();
	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig || !pst_chip)
		return;

	dbg_st = &pst_chip->dbg_st;
	dpss_frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "vfcd_tab")) {
		dump_vfcd_reg_t6w();
	} else if (!strcmp(parm[0], "frc_init_tab")) {
		dump_param_frc_init_apb();
	} else if (!strcmp(parm[0], "src0_buf_tab")) {
		dump_src0_buf_regs();
	} else if (!strcmp(parm[0], "frc_pd_tab")) {
		dump_frc_pd_regs_apb();
	} else if (!strcmp(parm[0], "iplogo_tab")) {
		dump_iplogo_regs_apb();
	} else if (!strcmp(parm[0], "melogo_tab")) {
		dump_me_logo_regs_apb();
	} else if (!strcmp(parm[0], "vp_init_tab")) {
		dump_param_vp_init_apb();
	} else if (!strcmp(parm[0], "mc_init_tab")) {
		dump_param_mc_init_apb();
	} else if (!strcmp(parm[0], "me_regs_tab")) {
		dump_me_regs_apb();
	} else if (!strcmp(parm[0], "vfcd_page")) {
		dump_vfcd_page();
	} else if (!strcmp(parm[0], "all_tab")) {
		dump_vfcd_reg_t6w();
		dump_param_frc_init_apb();
		dump_src0_buf_regs();
		dump_frc_pd_regs_apb();
		dump_iplogo_regs_apb();
		dump_me_logo_regs_apb();
		dump_param_vp_init_apb();
		dump_param_mc_init_apb();
		dump_me_regs_apb();
	} else if (!strcmp(parm[0], "fw_tab")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (dbg_st->ctrl_dbg.reg_tab_pr_en)
				pr_info("processing, try again later\n");
			else
				dbg_st->ctrl_dbg.reg_tab_pr_en = (u8)val1;
		}
	} else {
		pr_info("dpss: input param error!\n");
	}

exit:
	kfree(buf_orig);
}

ssize_t dpss_frc_reg_show(const struct class *class, const struct class_attribute *attr,
			  char *buf)
{
	pr_frc(0, "read:  echo r reg > /sys/class/vpu_dpss/reg\n");
	pr_frc(0, "write: echo w reg value > /sys/class/vpu_dpss/reg\n");
	pr_frc(0, "dump:  echo d reg length > /sys/class/vpu_dpss/reg\n");
	return 0;
}

ssize_t dpss_frc_reg_store(const struct class *class, const struct class_attribute *attr,
			   const char *buf, size_t count)
{
	dpss_frc_reg_io(buf);
	return count;
}

ssize_t dpss_frc_tool_debug_show(const struct class *class,
			const struct class_attribute *attr, char *buf)
{
	struct tool_debug_s *read_parm = NULL;
	struct frc_chip_st *pchip_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st) {
		pr_info("get frc st failed!\n");
		return 0;
	}

	read_parm = &pchip_st->dbg_st.tool_dbg;
	return sprintf(buf, "[0x%x] = 0x%x\n", read_parm->reg_read,
		       read_parm->reg_read_val);
}

ssize_t dpss_frc_tool_debug_store(const struct class *class,
			const struct class_attribute *attr, const char *buf,
				  size_t count)
{
	dpss_frc_tool_dbg_store(buf);
	return count;
}

ssize_t dpss_frc_debug_show(const struct class *class, const struct class_attribute *attr,
			    char *buf)
{
	return dpss_frc_debug_if_help(buf);
}

ssize_t dpss_frc_debug_store(const struct class *class, const struct class_attribute *attr,
			     const char *buf, size_t count)
{
	dpss_frc_debug_if(buf, count);
	return count;
}

ssize_t dpss_frc_buf_show(const struct class *class, const struct class_attribute *attr,
			  char *buf)
{
	return 0;
}

ssize_t dpss_frc_buf_store(const struct class *class, const struct class_attribute *attr,
			   const char *buf, size_t count)
{
	return count;
}

ssize_t dpss_frc_probe_dbg_show(const struct class *class,
			const struct class_attribute *attr, char *buf)
{
	return 0;
}

ssize_t dpss_frc_probe_dbg_store(const struct class *class,
			const struct class_attribute *attr, const char *buf,
				 size_t count)
{
	return count;
}

ssize_t dpss_frc_other_show(const struct class *class, const struct class_attribute *attr,
			    char *buf)
{
	return 0;
}

ssize_t dpss_frc_other_store(const struct class *class, const struct class_attribute *attr,
			     const char *buf, size_t count)
{
	return count;
}

ssize_t dpss_frc_debug_param_if_help(char *buf)
{
	ssize_t len = 0;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct frc_debug_s *dbg_st;
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	pchip_st = dpss_get_frc_st();
	if (!pfw_data || !pchip_st)
		return 0;
	frc_top = &pfw_data->frc_top_type;
	state_st = &pchip_st->state_st;
	dbg_st =  &pchip_st->dbg_st;
	len +=
		sprintf(buf + len, "frc_top->dpss_mode\t=%d\n", frc_top->dpss_mode);
	len +=
		sprintf(buf + len, "frc_top->badedit_en\t=%d\n",
			frc_top->badedit_en);
	len +=
		sprintf(buf + len, "frc_top->auto_align_en\t=%d\n",
			frc_top->auto_align_en);
	len +=
		sprintf(buf + len, "frc_top->mc_auto_en\t=%d\n",
			frc_top->mc_auto_en);
	len +=
		sprintf(buf + len, "frc_top->afrc_cmp_en\t=%d\n",
			frc_top->afrc_cmp_en);
	len +=
		sprintf(buf + len, "frc_top->memc_enable\t=%d\n",
			frc_top->memc_enable);
	len +=
		sprintf(buf + len, "frc_top->frc_rev_mode\t=%d\n",
			frc_top->frc_rev_mode);
	len +=
		sprintf(buf + len, "frc_top->me_mc_link_en\t=%d\n",
			frc_top->me_mc_link_en);
	len += sprintf(buf + len, "frc_top->fw_pause\t=%d\n",
			frc_top->fw_pause);
	len += sprintf(buf + len, "frc_top->game_mode\t=%d\n",
			frc_top->game_mode);
	len += sprintf(buf + len, "frc_top->frc_fbuf_only\t=%d\n",
			frc_top->frc_fbuf_only);
	len += sprintf(buf + len, "frc_top->frc_ds_scale_en =%d\n",
			frc_top->frc_ds_scale_en);
	len += sprintf(buf + len, "frc_top->src_little_endian =%d\n",
			frc_top->src_little_endian);
	len += sprintf(buf + len, "frc_top->src_uv_swap =%d\n",
			frc_top->src_uv_swap);
	len += sprintf(buf + len, "frc_top->src_swap_64_bit =%d\n",
			frc_top->src_swap_64_bit);
	len += sprintf(buf + len, "frc_top->mc_skip_mode\t=%d\n",
			frc_top->mc_skip_mode);
	len += sprintf(buf + len, "force_mc_byp_cnt\t=%d\n",
			state_st->force_mc_byp_cnt);
	len += sprintf(buf + len, "frc_top->mc_cut_en\t=%d\n",
			frc_top->mc_cut_en);
	len += sprintf(buf + len, "stats->unformat_bypass\t=%d\n",
			state_st->unformat_bypass);
	len += sprintf(buf + len, "stats->mc_cut_position\t=%d\n",
			state_st->mc_cut_position);
	len += sprintf(buf + len, "stats->pre_vsync_offset\t=%d\n",
			state_st->pre_vsync_offset);
	len += sprintf(buf + len, "stats->mc_lp_mode\t=%d\n",
			state_st->mc_lp_mode);
	len += sprintf(buf + len, "stats->use_inp_big\t= %#x\n",
			state_st->use_inp_big);
	len += sprintf(buf + len, "stats->detect_speed\t= %d, %d\n",
			state_st->detect_speed, state_st->detect_threshold);
	len += sprintf(buf + len, "dst_buf_th\t= %d\n",
			state_st->dst_buf_th);
	len += sprintf(buf + len, "frc_delay_dbg\t= %d\n",
			frc_delay_dbg);
	len += sprintf(buf + len, "mc_rdmif_err_reset_en\t= %d\n",
			dbg_st->ctrl_dbg.mc_rdmif_err_reset_en);
	len += sprintf(buf + len, "check_reg_status\t= %d\n",
			dbg_st->ctrl_dbg.check_reg_status);
	len += sprintf(buf + len, "need_disable_mc_link\t= %d\n",
			state_st->need_disable_mc_link);
	len += sprintf(buf + len, "trick_play_check_opt\t= %d\n",
			state_st->trick_play_check_opt);
	len += sprintf(buf + len, "trick_play_abnormal_level\t= %d\n",
			state_st->trick_play_abnormal_level);
	return len;
}

ssize_t dpss_frc_param_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	return dpss_frc_debug_param_if_help(buf);
}

void dpss_frc_debug_param_if(const char *buf, size_t count)
{
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct frc_debug_s *dbg_st;
	char *buf_orig, *parm[63] = { NULL };
	int val1;
	int val2;

	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	pchip_st = dpss_get_frc_st();
	if (!pfw_data || !buf || !pchip_st)
		return;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;

	frc_top = &pfw_data->frc_top_type;
	state_st = &pchip_st->state_st;
	dbg_st = &pchip_st->dbg_st;

	dpss_frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "dpss_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->dpss_mode = (u32)val1;
		pr_frc(0, "dpss_mode:0x%x\n", frc_top->dpss_mode);
	} else if (!strcmp(parm[0], "badedit_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->badedit_en = (u32)val1;
		pr_frc(0, "badedit_en:0x%x\n", frc_top->badedit_en);
	} else if (!strcmp(parm[0], "fw_pause")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			frc_top->fw_pause = (u32)val1;
			state_st->cur_fw_pause = (u32)val1;
		}
	} else if (!strcmp(parm[0], "frc_rev_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->frc_rev_mode = (u32)val1;
	} else if (!strcmp(parm[0], "auto_align_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->auto_align_en = (u32)val1;
	} else if (!strcmp(parm[0], "mc_auto_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->mc_auto_en = (u32)val1;
	} else if (!strcmp(parm[0], "afrc_cmp_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->afrc_cmp_en = (u32)val1;
	} else if (!strcmp(parm[0], "frc_ds_scale_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->frc_ds_scale_en = (u32)val1;
		pr_frc(2, "frc_ds_scale_en %d\n", frc_top->frc_ds_scale_en);
	} else if (!strcmp(parm[0], "frc_clk_auto")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			;	//      devp->clk_chg = val1;
			//      schedule_work(&devp->frc_clk_work);
		}
	} else if (!strcmp(parm[0], "frc_fbuf_only")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->frc_fbuf_only = (u32)val1;
		pr_frc(2, "frc_fbuf_only= %d\n", frc_top->frc_fbuf_only);
	} else if (!strcmp(parm[0], "force_mix")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->force_mix = (u32)val1;
		pr_frc(2, "force_mix= %d\n", frc_top->force_mix);
	} else if (!strcmp(parm[0], "src_uv_swap")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->src_uv_swap = (u32)val1;
		pr_frc(2, "src_uv_swap= %d\n", frc_top->src_uv_swap);
	} else if (!strcmp(parm[0], "src_swap_64")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->src_swap_64_bit = (u32)val1;
		pr_frc(2, "src_swap_64_bit= %d\n", frc_top->src_swap_64_bit);
	} else if (!strcmp(parm[0], "src_little_endian")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->src_little_endian = (u32)val1;
		pr_frc(2, "src_little_endian= %d\n", frc_top->src_little_endian);
	} else if (!strcmp(parm[0], "mc_skip_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->mc_skip_mode = (u32)val1;
		pr_frc(2, "mc_skip_mode= %d\n", frc_top->mc_skip_mode);
	} else if (!strcmp(parm[0], "unformat_bypass")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->unformat_bypass = (u32)val1;
		pr_frc(2, "unformat_bypass= %d\n", state_st->unformat_bypass);
	} else if (!strcmp(parm[0], "mc_cut_position")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->mc_cut_position = (u32)val1;
		pr_frc(2, "mc_cut_position= %d\n", state_st->mc_cut_position);
	} else if (!strcmp(parm[0], "pre_vsync_offset")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->pre_vsync_offset = (u32)val1;
		pr_frc(2, "pre_vsync_offset= %d\n", state_st->pre_vsync_offset);
	}  else if (!strcmp(parm[0], "mc_lp_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->mc_lp_mode = (bool)val1;
		pr_frc(2, "mc_lp_mode= %d\n", state_st->mc_lp_mode);
	} else if (!strcmp(parm[0], "use_inp_big_i")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			state_st->use_inp_big &= 0xF0;
			state_st->use_inp_big |= (val1 & 0x0F);
		}
		pr_frc(2, "use_inp_big_i= %#x\n", state_st->use_inp_big);
	} else if (!strcmp(parm[0], "use_inp_big_p")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			state_st->use_inp_big &= 0x0F;
			state_st->use_inp_big |= (val1 & 0x0F) << 4;
		}
		pr_frc(2, "use_inp_big_p= %#x\n", state_st->use_inp_big);
	} else if (!strcmp(parm[0], "detect_speed")) {
		if (!parm[2])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->detect_speed = (bool)val1;
		if (kstrtoint(parm[2], 10, &val1) == 0)
			state_st->detect_threshold = (bool)val1;
		pr_frc(2, "detect_speed= %d\n", state_st->detect_speed);
	} else if (!strcmp(parm[0], "frc_dp")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1) {
				dbg_st->pat_dbg.pat_en = 1;
				dbg_st->pat_dbg.pat_type |= BIT_1;
				dbg_st->pat_dbg.pat_color = (u8)val1;
			} else {
				dbg_st->pat_dbg.pat_en = 0;
				dbg_st->pat_dbg.pat_type |= ~BIT_1;
				dbg_st->pat_dbg.pat_color = (u8)val1;
			}
			dpss_frc_set_mc_pattern(val1);
		}
	} else if (!strcmp(parm[0], "ratio")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->in_out_ratio = val1;
	} else if (!strcmp(parm[0], "secure_on")) {
		if (!parm[1] || !parm[2])
			goto exit;
		if (kstrtoint(parm[1], 16, &val1) == 0)
			if (kstrtoint(parm[2], 16, &val2) == 0)
				pr_frc(2, "do nothing\n");
	} else if (!strcmp(parm[0], "secure_off")) {
		;		//      frc_test_mm_secure_set_off(devp);
	} else if (!strcmp(parm[0], "mc_cut_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->mc_cut_en = val1;
	} else if (!strcmp(parm[0], "force_mc_byp_cnt")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->force_mc_byp_cnt = (u32)val1;
	} else if (!strcmp(parm[0], "dst_buf_th")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->dst_buf_th = (u8)val1;
	} else if (!strcmp(parm[0], "frc_delay_dbg")) {
		if (!parm[1])
			goto exit;

		if (parm[1] && parm[2] && parm[3]) {
			int in_fps, out_fps, delta;

			if (kstrtoint(parm[1], 10, &in_fps) == 0 &&
				kstrtoint(parm[2], 10, &out_fps) == 0 &&
				kstrtoint(parm[3], 10, &delta) == 0)
				set_delta_ms(in_fps, out_fps, delta);
		} else if (!strcmp(parm[1], "dump")) {
			dump_delta_ms();
		} else {
			if (kstrtoint(parm[1], 10, &val1) == 0)
				frc_delay_dbg = val1;
		}
	} else if (!strcmp(parm[0], "mc_rdmif_err_reset_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dbg_st->ctrl_dbg.mc_rdmif_err_reset_en = (u8)val1;
	} else if (!strcmp(parm[0], "check_reg_status")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dbg_st->ctrl_dbg.check_reg_status = (u16)val1;
	} else if (!strcmp(parm[0], "win_size_flag")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1)
				state_st->win_size_zero_flag = true;
			else
				state_st->win_size_zero_flag = false;
		}
	} else if (!strcmp(parm[0], "used_seq_vfm")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1)
				state_st->used_seq_vfm = true;
			else
				state_st->used_seq_vfm = false;
		}
	} else if (!strcmp(parm[0], "frc_only_rst")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			state_st->frc_only_rst = val1;
	} else if (!strcmp(parm[0], "frc_off_delay_vs_cnt")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < 200)
				state_st->frc_off_delay_vs_cnt = val1;
		}
	} else if (!strcmp(parm[0], "trick_play_check_opt")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < 200)
				state_st->trick_play_check_opt = val1;
		}
	} else if (!strcmp(parm[0], "trick_play_abnormal_level")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 < 200)
				state_st->trick_play_abnormal_level = val1;
		}
	}

exit:
	kfree(buf_orig);
}

ssize_t dpss_frc_param_store(const struct class *class, const struct class_attribute *attr,
	const char *buf, size_t count)
{
	dpss_frc_debug_param_if(buf, count);
	return count;
}

ssize_t dpss_frc_debug_rdma_if_help(char *buf)
{
	ssize_t len = 0;

	len += sprintf(buf + len, "dpss_rdma_en\t= %d\n", rdma_enable());
	len += sprintf(buf + len, "rdma_dbg_en\t= %d\n", show_dpss_rdma_dbg_level());
	len += sprintf(buf + len, "buf_alloced\t= %d\n", show_dpss_rdma_buf_alloc_st());
	len += sprintf(buf + len, "reset_enable\t= %d\n", show_dpss_rdma_reset_status());

	return len;
}

void dpss_frc_debug_rdma_if(const char *buf, size_t count)
{
	u32 val1;
	u32 val2;
	char *buf_orig, *parm[47] = {NULL};

	if (!buf)
		return;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;

	dpss_frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "probe")) {
		dpss_rdma_init();
	} else if (!strcmp(parm[0], "exit")) {
		dpss_rdma_exit();
	} else if (!strcmp(parm[0], "man_reg_val")) {
		if (!parm[2])
			goto exit;
		if (kstrtoint(parm[1], 16, &val1))
			pr_frc(2, "do nothing\n");// val1 = val1 & 0xffff;
		if (kstrtoint(parm[2], 16, &val2))
			pr_frc(2, "do nothing\n");//val2 = val2 & 0xffffffff;
		pr_frc(0, "frc man rdma addr:%x, val:%x\n", val1, val2);
		dpss_rdma_man_wr_test(val1, val2);
	} else if (!strcmp(parm[0], "man_tri")) {
		dpss_rdma_man_wr_tri();
	} else if (!strcmp(parm[0], "man_clr")) {
		dpss_rdma_man_wr_clear();
	} else if (!strcmp(parm[0], "auto_reg_val")) {
		if (!parm[2])
			goto exit;
		if (kstrtoint(parm[1], 16, &val1))
			pr_frc(2, "do nothing\n");// val1 = val1 & 0xffff;
		if (kstrtoint(parm[2], 16, &val2))
			pr_frc(2, "do nothing\n");//val2 = val2 & 0xffffffff;
		pr_frc(0, "frc auto rdma addr:%x, val:%x\n", val1, val2);
		DPSS_RDMA_WR_PRE_VS(val1, val2);
	} else if (!strcmp(parm[0], "auto_tri")) {
		dpss_rdma_auto_wr_tri(1);
	} else if (!strcmp(parm[0], "dpss_rdma_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_rdma_force_chg_st(val1);
	} else if (!strcmp(parm[0], "rdma_dbg_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			dpss_rdma_dbg_level(val1);
	} else if (!strcmp(parm[0], "rdma_reset")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			set_dpss_rdma_reset_enable(val1);
	}

exit:
	kfree(buf_orig);
}

ssize_t dpss_frc_debug_game_if_help(char *buf)
{
	ssize_t len = 0;
	// struct dpss_frc_fw_data_s *pfw_data;
	// struct dpss_frc_top_type_s *frc_top;
	struct frc_chip_st *pchip_st;
	struct dpss_game_state_s *game_st;
	u32 encl_frc_ctrl, venc_vrr_ctrl, venc_vrr_adj_lmt, venc_vrr_ctrl_1;

	// pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return 0;

	// frc_top = &pfw_data->frc_top_type;
	game_st = &pchip_st->game_st;
	venc_vrr_ctrl = pchip_st->venc_vrr_ctrl;
	encl_frc_ctrl = pchip_st->encl_frc_ctrl;
	venc_vrr_adj_lmt = pchip_st->venc_vrr_adj_lmt;
	venc_vrr_ctrl_1 = pchip_st->venc_vrr_ctrl_1;

	len +=
		sprintf(buf + len, "game memc ver 20260415\tchip id:%d\n", pchip_st->chip);
	len +=
		sprintf(buf + len, "game_st->game_vrr_en\t=%d\n", game_st->game_vrr_en);
	len +=
		sprintf(buf + len, "game_st->mc_link_en\t=%d\n", game_st->mc_link_en);
	len +=
		sprintf(buf + len, "game_n2m\t=%d game_film\t=%d\n",
			game_st->game_n2m, game_st->game_film);
	len +=
		sprintf(buf + len, "game_st->dly_st.vtotal\t=%d\n", game_st->dly_st.vtotal);
	len +=
		sprintf(buf + len, "game_st->dly_st.vsp_dly\t=%d\n", game_st->dly_st.vsp_dly);
	len +=
		sprintf(buf + len, "game_st->dly_st.pre_dly\t=%d\n", game_st->dly_st.pre_dly);
	len +=
		sprintf(buf + len, "game_st->dly_st.vmin_num_adj\t=%d\n",
			game_st->dly_st.vmin_num_adj);
	len +=
		sprintf(buf + len, "game_st->dly_st.vmax_num_adj\t=%d\n",
			game_st->dly_st.vmax_num_adj);
	len +=
		sprintf(buf + len, "game_st->vrr_drop_cnt\t=%d\n", game_st->vrr_drop_cnt);
	len +=
		sprintf(buf + len, "game_st->video_mute_cnt\t=%d\n", game_st->video_mute_cnt);
	len +=
		sprintf(buf + len, "game_st->vdin_buf_num\t=%d\n", game_st->vdin_buf_num);
	len +=
		sprintf(buf + len, "game_st->inp_ctrl\t=%d\n", game_st->inp_ctrl);
	len +=
		sprintf(buf + len, "game_st->game_deblur\t=%d\n", game_st->game_deblur);
	len +=
		sprintf(buf + len, "game_st->mc_need_byp_en\t=%d\n", game_st->mc_need_byp_en);
	len +=
		sprintf(buf + len, "game_st->game_dae_byp_en\t=%d\n", game_st->game_dae_byp_en);
	len +=
		sprintf(buf + len, "ENCL_FRC_CTRL(0x%x)\t=%#X\n",
			encl_frc_ctrl, rd_vc(encl_frc_ctrl));
	len +=
		sprintf(buf + len, "VENC_VRR_CTRL(0x%x)\t=%#X\n",
			venc_vrr_ctrl, rd_vc(venc_vrr_ctrl));
	len +=
		sprintf(buf + len, "VENC_VRR_ADJ_LMT(0x%x)\t=%#X\n",
			venc_vrr_adj_lmt, rd_vc(venc_vrr_adj_lmt));
	len +=
		sprintf(buf + len, "VENC_VRR_CTRL1(0x%x)\t=%#X\n",
			venc_vrr_ctrl_1, rd_vc(venc_vrr_ctrl_1));
	return len;
}

void dpss_frc_debug_game_if(const char *buf, size_t count)
{
	u32 val1;
	struct frc_chip_st *pchip_st;
	struct frc_state_s *state_st;
	struct dpss_game_state_s *game_st;
	struct dpss_frc_fw_data_s *pfw_data;
	struct dpss_frc_top_type_s *frc_top;
	// u32 val2;
	char *buf_orig, *parm[47] = {NULL};

	if (!buf)
		return;

	buf_orig = kstrdup(buf, GFP_KERNEL);
	if (!buf_orig)
		return;

	pchip_st = dpss_get_frc_st();
	pfw_data = (struct dpss_frc_fw_data_s *)dpss_get_fw_data();
	if (!pchip_st || !pfw_data)
		return;

	state_st = &pchip_st->state_st;
	game_st = &pchip_st->game_st;
	frc_top = &pfw_data->frc_top_type;
	dpss_frc_debug_parse_param(buf_orig, (char **)&parm);

	if (!strcmp(parm[0], "vsp_dly")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->dly_st.vsp_dly = val1;
	} else if (!strcmp(parm[0], "pre_dly")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->dly_st.pre_dly = val1;
	} else if (!strcmp(parm[0], "vmin_num_adj")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->dly_st.vmin_num_adj = val1;
	} else if (!strcmp(parm[0], "vmax_num_adj")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->dly_st.vmax_num_adj = val1;
	} else if (!strcmp(parm[0], "mc_link_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->mc_link_en = val1 ? true : false;
	} else if (!strcmp(parm[0], "vrr_drop_cnt")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->vrr_drop_cnt = val1;
	} else if (!strcmp(parm[0], "game_vrr_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->game_vrr_en = val1;
	} else if (!strcmp(parm[0], "video_mute_cnt")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->video_mute_cnt = val1;
	} else if (!strcmp(parm[0], "game_mode")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			frc_top->game_mode = val1;
		game_st->game_n2m = frc_top->game_mode & 0x1;
		game_st->game_film = (frc_top->game_mode >> 1) & 0x1;
	} else if (!strcmp(parm[0], "inp_ctrl")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0)
			game_st->inp_ctrl = val1;
	} else if (!strcmp(parm[0], "game_deblur")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 <= 0x40)
				game_st->game_deblur = val1;
			else
				pr_frc(0, "err: mc phase between 0 and 0x40 when game mode\n");
		}
	} else if (!strcmp(parm[0], "game_dae_byp_en")) {
		if (!parm[1])
			goto exit;
		if (kstrtoint(parm[1], 10, &val1) == 0) {
			if (val1 > 0)
				game_st->game_dae_byp_en = true;
			else
				game_st->game_dae_byp_en = false;
		}
	}

exit:
	kfree(buf_orig);
}

ssize_t dpss_frc_dump_reg_table_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	dpss_frc_dump_reg_table_if_help(buf);
	return 0;
}

ssize_t dpss_frc_dump_reg_table_store(const struct class *class,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	dpss_frc_dump_reg_table_if(buf);
	return count;
}

ssize_t dpss_frc_rdma_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	return dpss_frc_debug_rdma_if_help(buf);
}

ssize_t dpss_frc_rdma_store(const struct class *class,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	dpss_frc_debug_rdma_if(buf, count);

	return count;
}

ssize_t dpss_frc_game_show(const struct class *class,
	const struct class_attribute *attr, char *buf)
{
	return dpss_frc_debug_game_if_help(buf);
}

ssize_t dpss_frc_game_store(const struct class *class,
	const struct class_attribute *attr, const char *buf, size_t count)
{
	dpss_frc_debug_game_if(buf, count);

	return count;
}

void dpss_inp_reg_monitor(void)
{
	u32 i;
	u32 reg;
	struct frc_chip_st *pchip_st;
	struct frc_monitor_s *mon_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	mon_st = &pchip_st->dbg_st.monitor_dbg;

	if (!mon_st->dbg_reg_monitor_inp)
		return;

	for (i = 0; i < MONITOR_REG_MAX; i++) {
		reg = mon_st->dbg_inp_reg[i];
		if (reg != 0) {
			if (mon_st->dbg_buf_len > FRC_CHK_REG_ARRAY_LEN) {
				mon_st->dbg_reg_monitor_inp = 0;
				mon_st->dbg_buf_len = 0;
				return;
			}
			mon_st->dbg_buf_len++;
			if (reg > 0x8000)
				pr_info(" 0x%x=0x%08x\n", reg, rd(reg));
			else
				pr_info(" 0x%x=0x%08x\n", reg, rd_vc(reg));
		}
	}
}

void dpss_dae0_reg_monitor(void)
{
	u32 i;
	u32 reg;
	struct frc_chip_st *pchip_st;
	struct frc_monitor_s *mon_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	mon_st = &pchip_st->dbg_st.monitor_dbg;

	if (!mon_st->dbg_reg_monitor_dae0)
		return;

	for (i = 0; i < MONITOR_REG_MAX; i++) {
		reg = mon_st->dbg_dae0_reg[i];
		if (reg != 0) {
			if (mon_st->dbg_buf_len > FRC_CHK_REG_ARRAY_LEN) {
				mon_st->dbg_reg_monitor_dae0 = 0;
				mon_st->dbg_buf_len = 0;
				return;
			}
			mon_st->dbg_buf_len++;
			// pr_info("ivs:%d 0x%x=0x%08x\n", dbg_st->in_sts.vs_cnt,
			// reg, READ_FRC_REG(reg));
			if (reg > 0x8000)
				pr_info(" 0x%x=0x%08x\n", reg, rd(reg));
			else
				pr_info(" 0x%x=0x%08x\n", reg, rd_vc(reg));
		}
	}
}

void dpss_pre_vs_reg_monitor(void)
{
	u32 i;
	u32 reg;
	struct frc_chip_st *pchip_st;
	struct frc_monitor_s *mon_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	mon_st = &pchip_st->dbg_st.monitor_dbg;

	if (!mon_st->dbg_reg_monitor_pre_vs)
		return;

	for (i = 0; i < MONITOR_REG_MAX; i++) {
		reg = mon_st->dbg_pre_vs_reg[i];
		if (reg != 0) {
			if (mon_st->dbg_buf_len > FRC_CHK_REG_ARRAY_LEN) {
				mon_st->dbg_reg_monitor_pre_vs = 0;
				mon_st->dbg_buf_len = 0;
				return;
			}
			mon_st->dbg_buf_len++;
			if (reg > 0x8000)
				pr_info(" 0x%x=0x%08x\n", reg, rd(reg));
			else
				pr_info(" 0x%x=0x%08x\n", reg, rd_vc(reg));
		}
	}
}

void dpss_vsync_reg_monitor(void)
{
	u32 i;
	u32 reg;
	struct frc_chip_st *pchip_st;
	struct frc_monitor_s *mon_st;

	pchip_st = dpss_get_frc_st();
	if (!pchip_st)
		return;

	mon_st = &pchip_st->dbg_st.monitor_dbg;

	if (!mon_st->dbg_reg_monitor_vs)
		return;

	for (i = 0; i < MONITOR_REG_MAX; i++) {
		reg = mon_st->dbg_vs_reg[i];
		if (reg != 0) {
			if (mon_st->dbg_buf_len > FRC_CHK_REG_ARRAY_LEN) {
				mon_st->dbg_reg_monitor_vs = 0;
				mon_st->dbg_buf_len = 0;
				return;
			}
			mon_st->dbg_buf_len++;
			if (reg > 0x8000)
				pr_info(" 0x%x=0x%08x\n", reg, rd(reg));
			else
				pr_info(" 0x%x=0x%08x\n", reg, rd_vc(reg));
		}
	}
}

void frc_dbg_table_print(struct work_struct *work)
{
	u16 i, tmp_num;
	struct frc_chip_st *pst_chip;
	struct dpss_dev_s *devp;

	devp = dpss_get_devp();
	pst_chip = dpss_get_frc_st();

	if (unlikely(!devp) || !pst_chip) {
		PR_ERR("%s err, devp is NULL\n", __func__);
		return;
	}
	// pst_chip = devp->pchip_st;
	// if (!devp->probe_ok)
	// return;
	// if (!pst_chip->power_on_flag)
	// return;

	pr_info("t6w/x print reg table\n");
	// tmp_num = pst_chip->attr_st->drv_regs_num;
	tmp_num = FW_REG_NUM;
	for (i = 0; i < tmp_num; i++) {
		pr_info("0x%04x, 0x%08x\n",
			fw_regs_table[i].addr,
			fw_regs_table[i].value);
	}
	pst_chip->dbg_st.ctrl_dbg.reg_tab_pr_en = 0;
}

void frc_dbg_read_table_value(void)
{
	u16 i, tmp_num;
	struct frc_chip_st *pst_chip;

	pst_chip = dpss_get_frc_st();
	// struct regs_s *tmp_drv_regs_table;

	// pst_chip = devp->pchip_st;
	// tmp_num = pst_chip->attr_st->drv_regs_num;
	tmp_num = FW_REG_NUM;
	// tmp_drv_regs_table = (struct regs_s *)pst_chip->attr_st->drv_regs_table;

	for (i = 0; i < tmp_num; i++) {
		// fw_regs_table[i].addr =
		// tmp_drv_regs_table[i].addr;
		fw_regs_table[i].value =
			rd(fw_regs_table[i].addr);
	}

	schedule_work(&pst_chip->work_st.print_work);
}

void dpss_game_reinit_reg_tab(void)
{
	u16 i;

	for (i = 0; i < FW_REG_NUM; i++)
		wr(fw_regs_table[i].addr, fw_regs_table[i].value);
}
